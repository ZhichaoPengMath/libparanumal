/*

The MIT License (MIT)

Copyright (c) 2017 Tim Warburton, Noel Chalmers, Jesse Chan, Ali Karakus

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

#include "adaptive.h"



typedef union intorfloat {
  int ier;
  float w;
} ierw_t;

dfloat adaptiveCascadingWeightedInnerProduct(adaptive_t *adaptive, occa::memory &o_w, occa::memory &o_a, occa::memory &o_b){

  // use bin sorting by exponent to make the end reduction more robust
  // [ assumes that the partial reduction is ok in FP32 ]
  int Naccumulators = 256;
  int Nmantissa = 23;

  double *accumulators   = (double*) calloc(Naccumulators, sizeof(double));
  double *g_accumulators = (double*) calloc(Naccumulators, sizeof(double));

  mesh_t *mesh = adaptive->mesh;
  dfloat *tmp = adaptive->tmp;

  dlong Nblock = adaptive->Nblock;
  dlong Ntotal = mesh->Nelements*mesh->Np;
  
  occa::memory &o_tmp = adaptive->o_tmp;
  
  if(adaptive->options.compareArgs("DISCRETIZATION","CONTINUOUS"))
    adaptive->weightedInnerProduct2Kernel(Ntotal, o_w, o_a, o_b, o_tmp);
  else
    adaptive->innerProductKernel(Ntotal, o_a, o_b, o_tmp);
  
  o_tmp.copyTo(tmp);
  
  for(int n=0;n<Nblock;++n){
    const dfloat ftmpn = tmp[n];

    ierw_t ierw;
    ierw.w = fabs(ftmpn);
    
    int iexp = ierw.ier>>Nmantissa; // strip mantissa
    accumulators[iexp] += (double)ftmpn;
  }
  
  MPI_Allreduce(accumulators, g_accumulators, Naccumulators, MPI_DOUBLE, MPI_SUM, mesh->comm);
  
  double wab = 0.0;
  for(int n=0;n<Naccumulators;++n){ 
    wab += g_accumulators[Naccumulators-1-n]; // reverse order is important here (dominant first)
  }
  
  free(accumulators);
  free(g_accumulators);
  
  return wab;
}

#if 0

dfloat adaptiveWeightedNorm2(adaptive_t *adaptive, occa::memory &o_w, occa::memory &o_a){

  mesh_t *mesh = adaptive->mesh;
  dfloat *tmp = adaptive->tmp;
  dlong Nblock = adaptive->Nblock;
  dlong Nblock2 = adaptive->Nblock2;
  dlong Ntotal = mesh->Nelements*mesh->Np;

  if(adaptive->options.compareArgs("THREAD MODEL", "Serial")){

    const dfloat * __restrict__ cpu_w = (dfloat*)__builtin_assume_aligned(o_w.ptr(), USE_OCCA_MEM_BYTE_ALIGN);
    const dfloat * __restrict__ cpu_a = (dfloat*)__builtin_assume_aligned(o_a.ptr(), USE_OCCA_MEM_BYTE_ALIGN) ;

    // w'*(a.a)
    dfloat wa2 = 0;

    const hlong M = mesh->Nelements*mesh->Np;

    for(hlong i=0;i<M;++i){
      const dfloat ai = cpu_a[i];
      wa2 += ai*ai*cpu_w[i];
    }

    dfloat globalwa2 = 0;
    MPI_Allreduce(&wa2, &globalwa2, 1, MPI_DFLOAT, MPI_SUM, mesh->comm);
    
    return globalwa2;
  }

  occa::memory &o_tmp = adaptive->o_tmp;
  occa::memory &o_tmp2 = adaptive->o_tmp2;

  if(adaptive->options.compareArgs("DISCRETIZATION","CONTINUOUS"))
    adaptive->weightedNorm2Kernel(Ntotal, o_w, o_a, o_tmp);
  else
    adaptive->innerProductKernel(Ntotal, o_a, o_a, o_tmp);

  /* add a second sweep if Nblock>Ncutoff */
  dlong Ncutoff = 100;
  dlong Nfinal;

  if(Nblock>Ncutoff){
    
    mesh->sumKernel(Nblock, o_tmp, o_tmp2);
    
    o_tmp2.copyTo(tmp);
    
    Nfinal = Nblock2;
	
  }
  else{
    o_tmp.copyTo(tmp);
    
    Nfinal = Nblock;
  }    

  dfloat wab = 0;
  for(dlong n=0;n<Nfinal;++n){
    wab += tmp[n];
  }

  dfloat globalwab = 0;
  MPI_Allreduce(&wab, &globalwab, 1, MPI_DFLOAT, MPI_SUM, mesh->comm);

  return globalwab;
}

#endif

dfloat adaptiveInnerProduct(adaptive_t *adaptive, occa::memory &o_a, occa::memory &o_b){

  mesh_t *mesh = adaptive->mesh;
  dfloat *tmp = adaptive->tmp;
  dlong Nblock = adaptive->Nblock;
  dlong Ntotal = mesh->Nelements*mesh->Np;

  occa::memory &o_tmp = adaptive->o_tmp;

  adaptive->innerProductKernel(Ntotal, o_a, o_b, o_tmp);

  o_tmp.copyTo(tmp);

  dfloat ab = 0;
  for(dlong n=0;n<Nblock;++n){
    ab += tmp[n];
  }

  dfloat globalab = 0;
  MPI_Allreduce(&ab, &globalab, 1, MPI_DFLOAT, MPI_SUM, mesh->comm);

  return globalab;
}
