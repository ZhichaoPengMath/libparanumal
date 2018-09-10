/*

The MIT License (MIT)

Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus, Rajesh Gandham

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "solver.hpp"
#include "agmg.hpp"

namespace parAlmond {

void solver_t::AMGSetup(parCSR *A){

  // approximate Nrows at coarsest level
  coarseLevel = new coarseSolver(options);
  const int gCoarseSize = coarseLevel->getTargetSize();

  AMGstartLev = numLevels;

  agmgLevel *L = new agmgLevel(A, ktype);
  levels[numLevels] = L;

  hlong globalSize = L->A->globalRowStarts[size];

  //if the system if already small, dont create MG levels
  bool done = false;
  if(globalSize <= gCoarseSize){
    coarseLevel->setup(A);
    baseLevel = numLevels;
    done = true;
  }

  numLevels++;
  printf("Nrows = %d, \n", globalSize);
  int rank, size;
  MPI_Comm_rank(A->comm, &rank);
  MPI_Comm_size(A->comm, &size);
  printf("rank = %d, size = %d\n", rank , size);

  while(!done){
    printf("Setting up coarse level %d\n", numLevels);
    L = coarsenAgmgLevel((agmgLevel*)(levels[numLevels-1]), ktype, options);
    levels[numLevels] = L;
    hlong globalCoarseSize = L->A->globalRowStarts[size];
    printf("Nrows = %d, \n", globalCoarseSize);

    int rank, size;
    MPI_Comm_rank(L->A->comm, &rank);
    MPI_Comm_size(L->A->comm, &size);
    printf("rank = %d, size = %d\n", rank , size);

    numLevels++;

    if(globalCoarseSize <= gCoarseSize || globalSize < 2*globalCoarseSize){
      coarseLevel->setup(L->A);
      baseLevel = numLevels-1;
      break;
    }
    globalSize = globalCoarseSize;
  }

  size_t requiredBytes = 3*levels[AMGstartLev]->Ncols*sizeof(dfloat);
  allocateScratchSpace(requiredBytes, device);

  for (int n=AMGstartLev;n<numLevels;n++) {
    setupAgmgSmoother((agmgLevel*)(levels[n]), stype, ChebyshevIterations);
    allocateAgmgVectors((agmgLevel*)(levels[n]), n, numLevels, ctype);
    syncAgmgToDevice((agmgLevel*)(levels[n]), n, numLevels, ctype);
  }
  coarseLevel->syncToDevice();
}

//create coarsened problem
agmgLevel *coarsenAgmgLevel(agmgLevel *level, KrylovType ktype, setupAide options){

  int rank, size;
  MPI_Comm_rank(level->comm, &rank);
  MPI_Comm_size(level->comm, &size);

  parCSR *C = strongGraph(level->A);

  hlong *FineToCoarse = (hlong *) malloc(level->A->Ncols*sizeof(hlong));
  hlong *globalAggStarts = (hlong *) calloc(size+1,sizeof(hlong));

  formAggregates(level->A, C, FineToCoarse, globalAggStarts);

  // adjustPartition(FineToCoarse, options);

  dfloat *nullCoarseA;
  parCSR *P = constructProlongation(level->A, FineToCoarse, globalAggStarts, &nullCoarseA);
  parCSR *R = transpose(P);
  parCSR *A = galerkinProd(level->A, P);

  A->null = nullCoarseA;

  agmgLevel *coarseLevel = new agmgLevel(A,P,R, ktype);

  //update the number of columns required for this level (from R)
  level->Ncols = (level->Ncols > R->Ncols) ? level->Ncols : R->Ncols;

  return coarseLevel;
}

void setupAgmgSmoother(agmgLevel *level, SmoothType s, int ChebIterations){

  level->stype = s;
  level->ChebyshevIterations = ChebIterations;

  if((s == DAMPED_JACOBI)||(s == CHEBYSHEV)){
    // estimate rho(invD * A)
    dfloat rho = level->A->rhoDinvA();

    if (s == DAMPED_JACOBI) {
      level->lambda = (4./3.)/rho;
    } else if (s == CHEBYSHEV) {
      level->lambda1 = rho;
      level->lambda0 = rho/10.;
    }
  }
}

void allocateAgmgVectors(agmgLevel *level, int k, int numLevels, CycleType ctype) {

  if (k) level->x    = (dfloat *) calloc(level->Ncols,sizeof(dfloat));
  if (k) level->rhs  = (dfloat *) calloc(level->Nrows,sizeof(dfloat));

  level->res  = (dfloat *) calloc(level->Ncols,sizeof(dfloat));

  //kcycle vectors
  if (ctype==KCYCLE) {
    if ((k>0) && (k<NUMKCYCLES+1)) {
      level->ck = (dfloat *) calloc(level->Ncols,sizeof(dfloat));
      level->vk = (dfloat *) calloc(level->Nrows,sizeof(dfloat));
      level->wk = (dfloat *) calloc(level->Nrows,sizeof(dfloat));
    }
  }
}

void syncAgmgToDevice(agmgLevel *level, int k, int numLevels, CycleType ctype) {

  occa::device device = level->A->device;

  level->o_A = new parHYB(level->A);
  level->o_A->syncToDevice();
  if (k) {
    level->o_R = new parHYB(level->R);
    level->o_P = new parHYB(level->P);
    level->o_R->syncToDevice();
    level->o_P->syncToDevice();
  }

  if (level->x  ) level->o_x   = device.malloc(level->Ncols*sizeof(dfloat),level->x);
  if (level->rhs) level->o_rhs = device.malloc(level->Nrows*sizeof(dfloat),level->rhs);
  if (level->res) level->o_res = device.malloc(level->Ncols*sizeof(dfloat),level->res);

  if (ctype==KCYCLE) {
    if (level->ck) {
      level->o_ck = device.malloc(level->Ncols*sizeof(dfloat),level->ck);
      level->o_vk = device.malloc(level->Nrows*sizeof(dfloat),level->vk);
      level->o_wk = device.malloc(level->Nrows*sizeof(dfloat),level->wk);
    }
  }
}

} //namespace parAlmond