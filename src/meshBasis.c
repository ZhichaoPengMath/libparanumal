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

#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include <math.h>

extern "C"
{
  void dgesv_ (int *NrowsA, int *NcolsA, double *A, int *LDA, int *ipiv,  double *B, int *LDB, int *info);
  void dgeev_ (char *JOBVL, char *JOBVR, int *N, double *A, int *LDA, double *WR, double *WI, double *VL, int *LDVL, double *VR, int *LDVR, double *WORK, int *LWORK, int *INFO );
}


void matrixInverse(int N, dfloat *A);
void matrixEig(int N, dfloat *A, dfloat *VR, dfloat *WR, dfloat *WI);

void readDfloatArray(FILE *fp, const char *label, dfloat **A, int *Nrows, int* Ncols);
void readIntArray(FILE *fp, const char *label, int **A, int *Nrows, int* Ncols);

void meshMassMatrix(int Np, dfloat *V, dfloat **MM);
int meshVandermonde1D(int N, int Npoints, dfloat *r, dfloat **V, dfloat **Vr);

dfloat mygamma(dfloat x){

  dfloat lgam = lgamma(x);
  dfloat gam  = signgam*exp(lgam);
  return gam;
}

int meshJacobiGQ(dfloat alpha, dfloat beta, int N, dfloat **x, dfloat **w){

  // function NGQ = JacobiGQ(alpha,beta,N, x, w)
  // Purpose: Compute the N'th order Gauss quadrature points, x, 
  //          and weights, w, associated with the Jacobi 
  //          polynomial, of type (alpha,beta) > -1 ( <> -0.5).

  *x = (dfloat*) calloc(N+1, sizeof(dfloat));
  *w = (dfloat*) calloc(N+1, sizeof(dfloat));
  
  if (N==0){
    x[0][0] = (alpha-beta)/(alpha+beta+2);
    w[0][0] = 2;
    return N+1;
  }

  // Form symmetric matrix from recurrence.
  dfloat *J = (dfloat*) calloc((N+1)*(N+1), sizeof(dfloat));
  dfloat *h1 = (dfloat*) calloc(N+1, sizeof(dfloat));
  
  for(int n=0;n<=N;++n){
    h1[n] = 2*n+alpha+beta;
  }

  // J = J + J';
  for(int n=0;n<=N;++n){
    // J = diag(-1/2*(alpha^2-beta^2)./(h1+2)./h1) + ...
    J[n*(N+1)+n]+= -0.5*(alpha*alpha-beta*beta)/((h1[n]+2)*h1[n])*2; // *2 for symm
    
    //    diag(2./(h1(1:N)+2).*sqrt((1:N).*((1:N)+alpha+beta).*((1:N)+alpha).*((1:N)+beta)./(h1(1:N)+1)./(h1(1:N)+3)),1);
    if(n<N){
      J[n*(N+1)+n+1]   += (2./(h1[n]+2.))*sqrt((n+1)*(n+1+alpha+beta)*(n+1+alpha)*(n+1+beta)/((h1[n]+1)*(h1[n]+3)));
      J[(n+1)*(N+1)+n] += (2./(h1[n]+2.))*sqrt((n+1)*(n+1+alpha+beta)*(n+1+alpha)*(n+1+beta)/((h1[n]+1)*(h1[n]+3)));
    }
  }

  dfloat eps = 1;
  while(1+eps>1){
    eps = eps/2.;
  }
  printf("MACHINE PRECISION %e\n", eps);
  
  if (alpha+beta<10*eps) J[0] = 0;

  // Compute quadrature by eigenvalue solve

  //  [V,D] = eig(J); 
  dfloat *WR = (dfloat*) calloc(N+1, sizeof(dfloat));
  dfloat *WI = (dfloat*) calloc(N+1, sizeof(dfloat));
  dfloat *VR = (dfloat*) calloc((N+1)*(N+1), sizeof(dfloat));

  // x = diag(D);
  matrixEig(N+1, J, VR, *x, WI);

  
  //w = (V(1,:)').^2*2^(alpha+beta+1)/(alpha+beta+1)*gamma(alpha+1)*.gamma(beta+1)/gamma(alpha+beta+1);

  for(int n=0;n<=N;++n){
    w[0][n] = pow(VR[0*(N+1)+n],2)*(pow(2,alpha+beta+1)/(alpha+beta+1))*mygamma(alpha+1)*mygamma(beta+1)/mygamma(alpha+beta+1);
  }

  // sloppy sort

  for(int n=0;n<=N;++n){
    for(int m=n+1;m<=N;++m){
      if(x[0][n]>x[0][m]){
	dfloat tmpx = x[0][m];
	dfloat tmpw = w[0][m];
	x[0][m] = x[0][n];
	w[0][m] = w[0][n];
	x[0][n] = tmpx;
	w[0][n] = tmpw;
      }
    }
  }
  
  free(WR);
  free(WI);
  free(VR);
  
  return N+1;
}

int meshJacobiGL(dfloat alpha, dfloat beta, int N, dfloat **x, dfloat **w){

  *x = (dfloat*) calloc(N+1, sizeof(dfloat));
  *w = (dfloat*) calloc(N+1, sizeof(dfloat));

  x[0][0] = -1.;
  x[0][N] =  1.;
  
  if(N>1){
    dfloat *wtmp, *xtmp;
    meshJacobiGQ(alpha+1,beta+1, N-2, &xtmp, &wtmp);
    
    for(int n=1;n<N;++n){
      x[0][n] = xtmp[n-1];
    }
    
    free(xtmp);
    free(wtmp);
  }

  dfloat *MM, *V, *Vr, *Vs;

  meshVandermonde1D(N, N+1, x[0], &V, &Vr);
  meshMassMatrix(N+1, V, &MM);

  // use weights from mass lumping
  for(int n=0;n<=N;++n){
    dfloat res = 0;
    for(int m=0;m<=N;++m){
      res += MM[n*(N+1)+m];
    }
    w[0][n] = res;
  }
  
  return N+1;
}



dfloat meshJacobiP(dfloat a, dfloat alpha, dfloat beta, int N){
  
  dfloat ax = a; 

  dfloat *P = (dfloat *) calloc((N+1), sizeof(dfloat));

  // Zero order
  dfloat gamma0 = pow(2,(alpha+beta+1))/(alpha+beta+1)*mygamma(1+alpha)*mygamma(1+beta)/mygamma(1+alpha+beta);
  dfloat p0     = 1.0/sqrt(gamma0);

  if (N==0){ free(P); return p0;}
  P[0] = p0; 

  // first order
  dfloat gamma1 = (alpha+1)*(beta+1)/(alpha+beta+3)*gamma0;
  dfloat p1     = ((alpha+beta+2)*ax/2 + (alpha-beta)/2)/sqrt(gamma1);
  if (N==1){free(P); return p1;} 

  P[1] = p1;

  /// Repeat value in recurrence.
  dfloat aold = 2/(2+alpha+beta)*sqrt((alpha+1.)*(beta+1.)/(alpha+beta+3.));
  /// Forward recurrence using the symmetry of the recurrence.
  for(int i=1;i<=N-1;++i){
    dfloat h1 = 2.*i+alpha+beta;
    dfloat anew = 2./(h1+2.)*sqrt( (i+1.)*(i+1.+alpha+beta)*(i+1+alpha)*(i+1+beta)/(h1+1)/(h1+3));
    dfloat bnew = -(alpha*alpha-beta*beta)/h1/(h1+2);
    P[i+1] = 1./anew*( -aold*P[i-1] + (ax-bnew)*P[i]);
    aold =anew;
  }
  
  dfloat pN = P[N]; 
  free(P);
  return pN;

}

dfloat meshGradJacobiP(dfloat a, dfloat alpha, dfloat beta, int N){

  dfloat PNr = 0;

  if(N>0)
    PNr = sqrt(N*(N+alpha+beta+1.))*meshJacobiP(a, alpha+1.0, beta+1.0, N-1);

  return PNr;
}


void meshOrthonormalBasis1D(dfloat a, int i, dfloat *P, dfloat *Pr){
  // 
  dfloat p1 = meshJacobiP(a,0,0,i);
  dfloat p1a = meshGradJacobiP(a,0,0,i);

  *P = p1;
  *Pr = p1a;
}


void meshOrthonormalBasisTri2D(dfloat a, dfloat b, int i, int j, dfloat *P, dfloat *Pr, dfloat *Ps){
  // 
  dfloat p1 = meshJacobiP(a,0,0,i);
  dfloat p2 = meshJacobiP(b,2*i+1,0,j);

  dfloat p1a = meshGradJacobiP(a,0,0,i);
  dfloat p2b = meshGradJacobiP(b,2*i+1,0,j);

  *P = sqrt(2.0)*p1*p2*pow(1.0-b,i);

  *Pr = p1a*p2;
  if(i>0)
    *Pr *= pow(0.5*(1.0-b),i-1);

  *Ps = p1a*p2*0.5*(1.0+a);
  if(i>0)
    *Ps *= pow(0.5*(1.0-b),i-1);

  dfloat tmp = p2b*pow(0.5*(1.0-b), i);
  if(i>0)
    tmp -= 0.5*i*p2*pow(0.5*(1.0-b), i-1);

  *Ps += p1*tmp;

  // normalize
  *Pr *= pow(2.0, i+0.5);
  *Ps *= pow(2.0, i+0.5);
  
}

void meshOrthonormalBasisQuad2D(dfloat a, dfloat b, int i, int j, dfloat *P, dfloat *Pr, dfloat *Ps){
  // 
  dfloat p1 = meshJacobiP(a,0,0,i);
  dfloat p2 = meshJacobiP(b,0,0,j);
  dfloat p1a = meshGradJacobiP(a,0,0,i);
  dfloat p2b = meshGradJacobiP(b,0,0,j);

  *P = p1*p2;
  *Pr = p1a*p2;
  *Ps = p1*p2b;
  
}

// TW: check normalization
void meshOrthonormalBasisTet3D(dfloat a, dfloat b, dfloat c, int i, int j, int k, dfloat *P, dfloat *Pr, dfloat *Ps, dfloat *Pt){
  // 
  dfloat p1 = meshJacobiP(a,0,0,i);
  dfloat p2 = meshJacobiP(b,2*i+1,0,j);
  dfloat p3 = meshJacobiP(c,2*(i+j)+2,0,k);

  dfloat p1a = meshGradJacobiP(a,0,0,i);
  dfloat p2b = meshGradJacobiP(b,2*i+1,0,j);
  dfloat p3c = meshGradJacobiP(c,2*(i+j)+2,0,k);

  *P = 2.*sqrt(2.0)*p1*p2*p3*pow(1.0-b,i)*pow(1.0-c,i+j);

  *Pr = p1a*p2*p3;
  if(i>0)
    *Pr *= pow(0.5*(1.0-b), i-1);
  if(i+j>0)
    *Pr *= pow(0.5*(1.0-c), i+j-1);

  *Ps = 0.5*(1.0+a)*(*Pr);
  dfloat tmp = p2b*pow(0.5*(1.0-b), i);
  if(i>0)
    tmp += -0.5*i*p2*pow(0.5*(1.0-b), i-1);
  if(i+j>0)
    tmp *= pow(0.5*(1.0-c), i+j-1);
  tmp *= p1*p3;
  *Ps += tmp;

  *Pt = 0.5*(1.0+a)*(*Pr) + 0.5*(1.0+b)*tmp;
  tmp = p3c*pow(0.5*(1-c), i+j);
  if(i+j>0)
    tmp -= 0.5*(i+j)*(p3*pow(0.5*(1.0-c), i+j-1));
  tmp *= p1*p2*pow(0.5*(1-b), i);
  *Pt += tmp;

  *Pr *= pow(2, 2*i+j+1.5);
  *Ps *= pow(2, 2*i+j+1.5);
  *Pt *= pow(2, 2*i+j+1.5);

}

void meshOrthonormalBasisHex3D(dfloat a, dfloat b, dfloat c, int i, int j, int k, dfloat *P, dfloat *Pr, dfloat *Ps, dfloat *Pt){
  // 
  dfloat p1 = meshJacobiP(a,0,0,i);
  dfloat p2 = meshJacobiP(b,0,0,j);
  dfloat p3 = meshJacobiP(c,0,0,k);
  dfloat p1a = meshGradJacobiP(a,0,0,i);
  dfloat p2b = meshGradJacobiP(b,0,0,j);
  dfloat p3c = meshGradJacobiP(c,0,0,k);

  *P = p1*p2*p3;
  *Pr = p1a*p2*p3;
  *Ps = p1*p2b*p3;
  *Pt = p1*p2*p3c;
  
}


int meshVandermonde1D(int N, int Npoints, dfloat *r, dfloat **V, dfloat **Vr){

  int Np = (N+1);
  
  *V  = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  for(int n=0; n<Npoints; n++){

    int sk = 0;
    for(int i=0; i<=N; i++){
      int id = n*Np+sk;
      meshOrthonormalBasis1D(r[n], i, V[0]+id, Vr[0]+id);
      sk++;
    }
  }
  
  return Np;
}



int meshVandermondeTri2D(int N, int Npoints, dfloat *r, dfloat *s, dfloat **V, dfloat **Vr, dfloat **Vs){

  int Np = (N+1)*(N+2)/2;
  
  *V  = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vs = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  for(int n=0; n<Npoints; n++){

    dfloat a, b;
    
    // First convert to ab coordinates
    if(fabs(s[n]-1.0)>1e-8)
      a = 2.0*(1.+r[n])/(1.0-s[n])-1.0;
    else
      a = -1.0; 
    
    b = s[n];
    
    int sk=0;
    for(int i=0; i<=N; i++){
      for(int j=0; j<=N-i; j++){
	int id = n*Np+sk;
	meshOrthonormalBasisTri2D(a, b, i, j, V[0]+id,Vr[0]+id, Vs[0]+id);
	sk++;
      }
    }
  }

  return Np;
}


int meshVandermondeQuad2D(int N, int Npoints, dfloat *r, dfloat *s, dfloat **V, dfloat **Vr, dfloat **Vs){

  int Np = (N+1)*(N+1);
  
  *V  = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vs = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  for(int n=0; n<Npoints; n++){

    int sk = 0;
    for(int i=0; i<=N; i++){
      for(int j=0; j<=N; j++){
	int id = n*Np+sk;
	meshOrthonormalBasisQuad2D(r[n], s[n], i, j, V[0]+id, Vr[0]+id, Vs[0]+id);
	sk++;
      }
    }
  }

  return Np;
}



int meshVandermondeTet3D(int N, int Npoints, dfloat *r, dfloat *s, dfloat *t,
			  dfloat **V, dfloat **Vr, dfloat **Vs, dfloat **Vt){

  int Np = (N+1)*(N+2)*(N+3)/6; 

  *V  = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vs = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vt = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  
  for(int n=0; n<Npoints; n++){
    // First convert to abc coordinates
    dfloat a, b, c;
    
    if(fabs(s[n]+t[n])>1e-8)
      a = 2.0*(1.+r[n])/(-s[n]-t[n])-1.0;
    else
      a = -1.0; 

    if(fabs(t[n]-1)>1e-8)
      b = 2.0*(1+s[n])/(1.-t[n])-1.0;
    else
      b = -1.;
    
    c = t[n];

    int sk=0;
    for(int i=0; i<=N; i++){
      for(int j=0; j<=N-i; j++){
	for(int k=0; k<=N-i-j; k++){
	  int id = n*Np+sk;	
	  meshOrthonormalBasisTet3D(a, b, c, i, j, k, V[0]+id, Vr[0]+id, Vs[0]+id, Vt[0]+id);
	  sk++;
	}
      }
    }
  }

  return Np;
}


int meshVandermondeHex3D(int N, int Npoints, dfloat *r, dfloat *s, dfloat *t, dfloat **V, dfloat **Vr, dfloat **Vs, dfloat **Vt){

  int Np = (N+1)*(N+1)*(N+1);
  
  *V  = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vs = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Vt = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  for(int n=0; n<Npoints; n++){

    int sk = 0;
    for(int i=0; i<=N; i++){
      for(int j=0; j<=N; j++){
	for(int k=0; k<=N; k++){
	  int id = n*Np+sk;
	  meshOrthonormalBasisHex3D(r[n], s[n], t[n], i, j, k, V[0]+id, Vr[0]+id, Vs[0]+id, Vt[0]+id);
	  sk++;
	}
      }
    }
  }

  return Np;
}


// C = A/B  = trans(trans(B)\trans(A))
// assume row major
void matrixRightSolve(int NrowsA, int NcolsA, dfloat *A, int NrowsB, int NcolsB, dfloat *B, dfloat *C){

  int info;

  int NrowsX = NcolsB;
  int NcolsX = NrowsB;
  
  int NrowsY = NcolsA;
  int NcolsY = NrowsA;
  
  int lwork = NrowsX*NcolsX;
  
  // compute inverse mass matrix
  double *tmpX = (double*) calloc(NrowsX*NcolsX, sizeof(double));
  double *tmpY = (double*) calloc(NrowsY*NcolsY, sizeof(double));

  int    *ipiv = (int*) calloc(NrowsX, sizeof(int));
  double *work = (double*) calloc(lwork, sizeof(double));

  for(int n=0;n<NrowsX*NcolsX;++n){
    tmpX[n] = B[n];
  }

  for(int n=0;n<NrowsY*NcolsY;++n){
    tmpY[n] =A[n];
  }
  
  dgesv_(&NrowsX, &NcolsY, tmpX, &NrowsX, ipiv, tmpY, &NrowsY, &info); // ?

  for(int n=0;n<NrowsY*NcolsY;++n){
    C[n] = tmpY[n];
  }

  if(info)
    printf("matrixRightSolve: dgesv reports info = %d when inverting matrix\n", info);

  free(work);
  free(ipiv);
  free(tmpX);
  free(tmpY);
}



// compute right eigenvectors
void matrixEig(int N, dfloat *A, dfloat *VR, dfloat *WR, dfloat *WI){

  char JOBVL = 'N';
  char JOBVR = 'V';
  int LDA = N;
  int LDVL = N;
  int LDVR = N;
  int LWORK = 8*N;

  double *tmpA  = (double*) calloc(N*N,sizeof(double));
  double *tmpWR = (double*) calloc(N,sizeof(double));
  double *tmpWI = (double*) calloc(N,sizeof(double));
  double *tmpVR = (double*) calloc(N*N,sizeof(double));
  double *tmpVL = NULL;
  double *WORK  = (double*) calloc(LWORK,sizeof(double));

  int info;

  for(int n=0;n<N;++n){
    for(int m=0;m<N;++m){
      tmpA[n+m*N] = A[n*N+m];
    }
  }
  
  dgeev_ (&JOBVL, &JOBVR, &N, tmpA, &LDA, tmpWR, tmpWI, tmpVL, &LDVL, tmpVR, &LDVR, WORK, &LWORK, &info);

  for(int n=0;n<N;++n){
    WR[n] = tmpWR[n];
    WI[n] = tmpWI[n];
    for(int m=0;m<N;++m){
      VR[n+m*N] = tmpVR[n*N+m];
    }
  }
  
}

void matrixPrint(FILE *fp, const char *mess, int Nrows, int Ncols, dfloat *A){
#if 1
  fprintf(fp, "%s=[\n", mess);
  for(int n=0;n<Nrows;++n){
    for(int m=0;m<Ncols;++m){
      fprintf(fp," % e ", A[n*Ncols+m]);
    }
    fprintf(fp,"\n");
  }
  fprintf(fp,"];\n");
#endif
}


void matrixCompare(FILE *fp, const char *mess, int Nrows, int Ncols, dfloat *A, dfloat *B){

  dfloat maxd = 0;
  for(int n=0;n<Nrows;++n){
    for(int m=0;m<Ncols;++m){
      dfloat newd = fabs(A[n*Ncols+m]-B[n*Ncols+m]);
      if(newd>maxd)
	maxd = newd;
    }
  }
  fprintf(fp,"%s=% e\n", mess, maxd);
}


void meshDmatrix1D(int N, int Npoints, dfloat *r, dfloat **Dr){

  dfloat *V, *Vr;
  int Np = meshVandermonde1D(N, Npoints, r, &V, &Vr);
  
  *Dr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  
  matrixRightSolve(Np, Np, Vr, Np, Np, V, Dr[0]);
  
  free(V);
  free(Vr);
}




void meshDmatricesTri2D(int N, int Npoints, dfloat *r, dfloat *s, dfloat **Dr, dfloat **Ds){
  
  dfloat *V, *Vr, *Vs;
  int Np = meshVandermondeTri2D(N, Npoints, r, s, &V, &Vr, &Vs);
  
  *Dr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Ds = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  
  matrixRightSolve(Np, Np, Vr, Np, Np, V, Dr[0]);
  matrixRightSolve(Np, Np, Vs, Np, Np, V, Ds[0]);

  free(V);
  free(Vr);
  free(Vs);
}

void meshDmatricesQuad2D(int N, int Npoints, dfloat *r, dfloat *s, dfloat **Dr, dfloat **Ds){

  dfloat *V, *Vr, *Vs;
  int Np = meshVandermondeQuad2D(N, Npoints, r, s, &V, &Vr, &Vs);

  *Dr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Ds = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  matrixRightSolve(Np, Np, Vr, Np, Np, V, Dr[0]);
  matrixRightSolve(Np, Np, Vs, Np, Np, V, Ds[0]);
  
  free(V);
  free(Vr);
  free(Vs);
}

void meshDmatricesTet3D(int N, int Npoints, dfloat *r, dfloat *s, dfloat *t, dfloat **Dr, dfloat **Ds, dfloat **Dt){

  dfloat *V, *Vr, *Vs, *Vt;
  int Np = meshVandermondeTet3D(N, Npoints, r, s, t, &V, &Vr, &Vs, &Vt);

  *Dr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Ds = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Dt = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  matrixRightSolve(Np, Np, Vr, Np, Np, V, Dr[0]);
  matrixRightSolve(Np, Np, Vs, Np, Np, V, Ds[0]);
  matrixRightSolve(Np, Np, Vt, Np, Np, V, Dt[0]);

  free(V);
  free(Vr);
  free(Vs);
  free(Vt);
}


void meshDmatricesHex3D(int N, int Npoints, dfloat *r, dfloat *s, dfloat *t, dfloat **Dr, dfloat **Ds, dfloat **Dt){

  dfloat *V, *Vr, *Vs, *Vt;
  int Np = meshVandermondeHex3D(N, Npoints, r, s, t, &V, &Vr, &Vs, &Vt);

  *Dr = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Ds = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));
  *Dt = (dfloat *) calloc(Npoints*Np, sizeof(dfloat));

  matrixRightSolve(Np, Np, Vr, Np, Np, V, Dr[0]);
  matrixRightSolve(Np, Np, Vs, Np, Np, V, Ds[0]);
  matrixRightSolve(Np, Np, Vt, Np, Np, V, Dt[0]);

  free(V);
  free(Vr);
  free(Vs);
  free(Vt);

}

// assumes NpointsIn = (N+1)*(N+2)/2
void meshInterpolationMatrixTri2D(int N,
				  int NpointsIn, dfloat *rIn, dfloat *sIn,
				  int NpointsOut, dfloat *rOut, dfloat *sOut,
				  dfloat **I){

  dfloat *VIn, *VrIn, *VsIn;
  dfloat *VOut, *VrOut, *VsOut;
  
  int NpIn  = meshVandermondeTri2D(N, NpointsIn, rIn, sIn, &VIn, &VrIn, &VsIn);
  int NpOut = meshVandermondeTri2D(N, NpointsOut, rOut, sOut, &VOut, &VrOut, &VsOut);
  
  *I = (dfloat *) calloc(NpointsIn*NpointsOut, sizeof(dfloat));
  
  matrixRightSolve(NpointsOut, NpOut, VOut, NpointsIn, NpIn, VIn, *I);

  free(VIn);  free(VrIn);  free(VsIn);
  free(VOut); free(VrOut); free(VsOut);
}


// assumes NpointsIn = (N+1)*(N+2)*(N+3)/6
void meshInterpolationMatrixTet3D(int N,
				  int NpointsIn,  dfloat *rIn,  dfloat *sIn,  dfloat *tIn,
				  int NpointsOut, dfloat *rOut, dfloat *sOut, dfloat *tOut,
				  dfloat **I){

  dfloat *VIn,  *VrIn,  *VsIn,  *VtIn;
  dfloat *VOut, *VrOut, *VsOut, *VtOut;
  
  int NpIn  = meshVandermondeTet3D(N, NpointsIn,  rIn,  sIn,  tIn,  &VIn,  &VrIn,  &VsIn,  &VtIn);
  int NpOut = meshVandermondeTet3D(N, NpointsOut, rOut, sOut, tOut, &VOut, &VrOut, &VsOut, &VtOut);
  
  *I = (dfloat *) calloc(NpointsIn*NpointsOut, sizeof(dfloat));
  
  matrixRightSolve(NpointsOut, NpOut, VOut, NpointsIn, NpIn, VIn, *I);

  free(VIn);  free(VrIn);  free(VsIn);  free(VtIn);
  free(VOut); free(VrOut); free(VsOut); free(VtOut);
}



// masMatrix = inv(V')*inv(V) = inv(V*V')
void meshMassMatrix(int Np, dfloat *V, dfloat **MM){

  *MM = (dfloat *) calloc(Np*Np, sizeof(dfloat));
  
  for(int n=0;n<Np;++n){
    for(int m=0;m<Np;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[n*Np+i]*V[m*Np+i];
      }
      MM[0][n*Np + m] = res;
    }
  }
  
  matrixInverse(Np, MM[0]);
}


void meshLiftMatrixTri2D(int N, int Np, int *faceNodes, dfloat *r, dfloat *s, dfloat **LIFT){


  dfloat *V, *Vr, *Vs, *MMf, *Vf, *Vrf;
  int Nfp = N+1;
  int Nfaces = 3;

  dfloat *Emat = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));
  dfloat *faceR = (dfloat*) calloc(Nfp, sizeof(dfloat));

  meshVandermondeTri2D(N, Np, r, s, &V, &Vr, &Vs);
  
  for(int f=0;f<Nfaces;++f){

    for(int n=0;n<Nfp;++n){
      if(f==0) faceR[n] = r[faceNodes[n + 0*Nfp]];
      if(f==1) faceR[n] = r[faceNodes[n + 1*Nfp]];
      if(f==2) faceR[n] = s[faceNodes[n + 2*Nfp]];
    }

    // compute mass matrix for nodes on face
    meshVandermonde1D(N, Nfp, faceR, &Vf, &Vrf);
    meshMassMatrix(Nfp, Vf, &MMf);

    for(int n=0;n<Nfp;++n){
      for(int m=0;m<Nfp;++m){
	int id = faceNodes[n+f*Nfp];
	Emat[id*Nfaces*Nfp + m + f*Nfp] = MMf[n*Nfp+m];
      }
    }
    
    free(Vf);
    free(Vrf);
    free(MMf);
  }
  
  
  // inv(mass matrix)*\I_n (L_i,L_j)_{edge_n}
  *LIFT = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));
  dfloat *tmp = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));

  //  LIFT = V*(V'*Emat);

  for(int n=0;n<Np;++n){
    for(int m=0;m<Nfp*Nfaces;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[i*Np+n]*Emat[i*Nfaces*Nfp+m];
      }
      tmp[n*Nfaces*Nfp+m] = res;
    }
  }

  for(int n=0;n<Np;++n){
    for(int m=0;m<Nfp*Nfaces;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[n*Np+i]*tmp[i*Nfaces*Nfp+m];
      }
      LIFT[0][n*Nfaces*Nfp+m] = res;
    }
  }
  
  free(Emat);
  free(faceR);
  free(V);
  free(Vr);
  free(Vs);
}

void meshLiftMatrixTet3D(int N, int Np, int *faceNodes, dfloat *r, dfloat *s, dfloat *t, dfloat **LIFT){


  dfloat *V, *Vr, *Vs, *Vt, *MMf, *Vf, *Vrf, *Vsf;
  int Nfp = (N+1)*(N+2)/2;
  int Nfaces = 4;

  dfloat *Emat = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));
  dfloat *faceR = (dfloat*) calloc(Nfp, sizeof(dfloat));
  dfloat *faceS = (dfloat*) calloc(Nfp, sizeof(dfloat));

  meshVandermondeTet3D(N, Np, r, s, t, &V, &Vr, &Vs, &Vt);
  
  for(int f=0;f<Nfaces;++f){

    for(int n=0;n<Nfp;++n){
      if(f==0){ faceR[n] = r[faceNodes[n + 0*Nfp]]; faceS[n] = s[faceNodes[n + 0*Nfp]];}
      if(f==1){ faceR[n] = r[faceNodes[n + 1*Nfp]]; faceS[n] = t[faceNodes[n + 1*Nfp]];}
      if(f==2){ faceR[n] = s[faceNodes[n + 2*Nfp]]; faceS[n] = t[faceNodes[n + 2*Nfp]];}
      if(f==3){ faceR[n] = s[faceNodes[n + 3*Nfp]]; faceS[n] = t[faceNodes[n + 3*Nfp]];}
    }

    // compute mass matrix for nodes on face
    meshVandermondeTri2D(N, Nfp, faceR, faceS, &Vf, &Vrf, &Vsf);
    meshMassMatrix(Nfp, Vf, &MMf);

    for(int n=0;n<Nfp;++n){
      for(int m=0;m<Nfp;++m){
	int id = faceNodes[n+f*Nfp];
	Emat[id*Nfaces*Nfp + m + f*Nfp] = MMf[n*Nfp+m];
      }
    }
    
    free(Vf);
    free(Vrf);
    free(Vsf);
    free(MMf);
  }
  
  
  // inv(mass matrix)*\I_n (L_i,L_j)_{edge_n}
  *LIFT = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));
  dfloat *tmp = (dfloat*) calloc(Np*Nfaces*Nfp, sizeof(dfloat));

  //  LIFT = V*(V'*Emat);

  for(int n=0;n<Np;++n){
    for(int m=0;m<Nfp*Nfaces;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[i*Np+n]*Emat[i*Nfaces*Nfp+m];
      }
      tmp[n*Nfaces*Nfp+m] = res;
    }
  }

  for(int n=0;n<Np;++n){
    for(int m=0;m<Nfp*Nfaces;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[n*Np+i]*tmp[i*Nfaces*Nfp+m];
      }
      LIFT[0][n*Nfaces*Nfp+m] = res;
    }
  }
  
  free(Emat);
  free(faceR);
  free(faceS);
  free(V);
  free(Vr);
  free(Vs);
  free(Vt);
}

void meshCubatureWeakDmatrices1D(int N, int Np, dfloat *V,
				 int cubNp, dfloat *cubr, dfloat *cubw,
				 dfloat **cubDrT, dfloat **cubProject){

  dfloat *cubV, *cubVr;

  meshVandermonde1D(N, cubNp, cubr, &cubV, &cubVr);

  // cubDrT = V*transpose(cVr)*diag(cubw);
  // cubProject = V*cV'*diag(cubw); %% relies on (transpose(cV)*diag(cubw)*cV being the identity)

  for(int n=0;n<cubNp;++n){
    for(int m=0;m<Np;++m){
      // scale by cubw
      cubVr[n*Np+m] *= cubw[n];
      cubV[n*Np+m]  *= cubw[n];
    }
  }

  *cubDrT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubProject = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));

  for(int n=0;n<Np;++n){
    for(int m=0;m<cubNp;++m){
      dfloat resP = 0, resDrT = 0;

      for(int i=0;i<Np;++i){
	dfloat Vni = V[n*Np+i];
	resDrT += Vni*cubVr[m*Np+i];
	resP   += Vni*cubV[m*Np+i];
      }

      cubDrT[0][n*cubNp+m] = resDrT;
      cubProject[0][n*cubNp+m] = resP;
    }
  }
  
  free(cubV);
  free(cubVr);
}


void meshCubatureWeakDmatricesTri2D(int N, int Np, dfloat *V,
				    int cubNp, dfloat *cubr, dfloat *cubs, dfloat *cubw,
				    dfloat **cubDrT, dfloat **cubDsT, dfloat **cubProject){

  dfloat *cubV, *cubVr, *cubVs;

  meshVandermondeTri2D(N, cubNp, cubr, cubs, &cubV, &cubVr, &cubVs);

  // cubDrT = V*transpose(cVr)*diag(cubw);
  // cubDsT = V*transpose(cVs)*diag(cubw);
  // cubProject = V*cV'*diag(cubw); %% relies on (transpose(cV)*diag(cubw)*cV being the identity)

  for(int n=0;n<cubNp;++n){
    for(int m=0;m<Np;++m){
      // scale by cubw
      cubVr[n*Np+m] *= cubw[n];
      cubVs[n*Np+m] *= cubw[n];
      cubV[n*Np+m]  *= cubw[n];
    }
  }

  *cubDrT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubDsT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubProject = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));

  for(int n=0;n<Np;++n){
    for(int m=0;m<cubNp;++m){
      dfloat resP = 0, resDrT = 0, resDsT = 0;

      for(int i=0;i<Np;++i){
	dfloat Vni = V[n*Np+i];
	resDrT += Vni*cubVr[m*Np+i];
	resDsT += Vni*cubVs[m*Np+i];
	resP   += Vni*cubV[m*Np+i];
      }

      cubDrT[0][n*cubNp+m] = resDrT;
      cubDsT[0][n*cubNp+m] = resDsT;
      cubProject[0][n*cubNp+m] = resP;
    }
  }
  
  free(cubV);
  free(cubVr);
  free(cubVs);
  
}

void meshCubatureWeakDmatricesTet3D(int N, int Np, dfloat *V,
				    int cubNp, dfloat *cubr, dfloat *cubs, dfloat *cubt, dfloat *cubw,
				    dfloat **cubDrT, dfloat **cubDsT, dfloat **cubDtT, dfloat **cubProject){

  dfloat *cubV, *cubVr, *cubVs, *cubVt;

  meshVandermondeTet3D(N, cubNp, cubr, cubs, cubt, &cubV, &cubVr, &cubVs, &cubVt);

  // cubDrT = V*transpose(cVr)*diag(cubw);
  // cubDsT = V*transpose(cVs)*diag(cubw);
  // cubDtT = V*transpose(cVt)*diag(cubw);
  // cubProject = V*cV'*diag(cubw); %% relies on (transpose(cV)*diag(cubw)*cV being the identity)

  for(int n=0;n<cubNp;++n){
    for(int m=0;m<Np;++m){
      // scale by cubw
      cubVr[n*Np+m] *= cubw[n];
      cubVs[n*Np+m] *= cubw[n];
      cubVt[n*Np+m] *= cubw[n];
      cubV[n*Np+m]  *= cubw[n];
    }
  }

  *cubDrT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubDsT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubDtT = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));
  *cubProject = (dfloat*) calloc(cubNp*Np, sizeof(dfloat));

  for(int n=0;n<Np;++n){
    for(int m=0;m<cubNp;++m){
      dfloat resP = 0, resDrT = 0, resDsT = 0, resDtT = 0;

      for(int i=0;i<Np;++i){
	dfloat Vni = V[n*Np+i];
	resDrT += Vni*cubVr[m*Np+i];
	resDsT += Vni*cubVs[m*Np+i];
	resDtT += Vni*cubVt[m*Np+i];
	resP   += Vni*cubV[m*Np+i];
      }

      cubDrT[0][n*cubNp+m] = resDrT;
      cubDsT[0][n*cubNp+m] = resDsT;
      cubDtT[0][n*cubNp+m] = resDtT;
      cubProject[0][n*cubNp+m] = resP;
    }
  }
  
  free(cubV);
  free(cubVr);
  free(cubVs);
  free(cubVt);
  
}

int meshCubatureSurfaceMatricesTri2D(int N, int Np, dfloat *r, dfloat *s, dfloat *V, int *faceNodes,
				     int intNfp,  dfloat **intInterp, dfloat **intLIFT){

  int Nfaces = 3;
  int Nfp = N+1;

  dfloat *z, *w;

  //Surface cubature per face
  meshJacobiGQ(0,0,intNfp-1, &z, &w);

  dfloat *ir = (dfloat*) calloc(intNfp*Nfaces, sizeof(dfloat));
  dfloat *is = (dfloat*) calloc(intNfp*Nfaces, sizeof(dfloat));
  dfloat *iw = (dfloat*) calloc(intNfp*Nfaces, sizeof(dfloat));

  for(int n=0;n<intNfp;++n){
    ir[0*intNfp + n] =  z[n];
    ir[1*intNfp + n] = -z[n];
    ir[2*intNfp + n] = -1.0;

    is[0*intNfp + n] = -1.0;
    is[1*intNfp + n] =  z[n];
    is[2*intNfp + n] = -z[n];

    iw[0*intNfp + n] =  w[n];
    iw[1*intNfp + n] =  w[n];
    iw[2*intNfp + n] =  w[n];
  }

  dfloat *sInterp;
  meshInterpolationMatrixTri2D(N, Np, r, s, Nfaces*intNfp, ir, is, &sInterp);

  *intInterp = (dfloat*) calloc(intNfp*Nfaces*Nfp, sizeof(dfloat));
  for(int n=0;n<intNfp;++n){
    for(int m=0;m<Nfp;++m){
      intInterp[0][0*intNfp*Nfp + n*Nfp + m] = sInterp[(n+0*intNfp)*Np+faceNodes[0*Nfp+m]];
      intInterp[0][1*intNfp*Nfp + n*Nfp + m] = sInterp[(n+1*intNfp)*Np+faceNodes[1*Nfp+m]];
      intInterp[0][2*intNfp*Nfp + n*Nfp + m] = sInterp[(n+2*intNfp)*Np+faceNodes[2*Nfp+m]];
    }
  }

  // integration node lift matrix
  //iLIFT = V*V'*sInterp'*diag(iw(:));
  for(int n=0;n<Nfaces*intNfp;++n){
    for(int m=0;m<Np;++m){
	// scale by cubw
	sInterp[n*Np+m] *= iw[n];
    }
  }

  dfloat *tmp = (dfloat*) calloc(Np*Nfaces*intNfp, sizeof(dfloat));

  for(int n=0;n<Nfaces*intNfp;++n){
    for(int m=0;m<Np;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[i*Np+m]*sInterp[n*Np+i];
      }
      tmp[m*Nfaces*intNfp+n] = res;
    }
  }

  *intLIFT = (dfloat*) calloc(Nfaces*intNfp*Np, sizeof(dfloat));

  for(int n=0;n<Nfaces*intNfp;++n){
    for(int m=0;m<Np;++m){
      dfloat res = 0;
      for(int i=0;i<Np;++i){
	res += V[m*Np+i]*tmp[i*Nfaces*intNfp+n];
      }
      intLIFT[0][m*Nfaces*intNfp+n] = res;
    }
  }

  free(ir);  free(is);  free(iw);  free(sInterp); free(tmp);
  
  return Nfaces*intNfp;
}


#if TEST_MESH_BASIS==1
// mpic++ -I../libs/gatherScatter/ -I../../occa/include  -I../include -o meshBasis meshBasis.c matrixInverse.c readArray.c -Ddfloat=double -llapack -lblas -lm -DDHOLMES='"../"' -DdfloatFormat='"%lf"' -DTEST_MESH_BASIS=1 

// to run with degree 2:
// ./meshBasis 2
int main(int argc, char **argv){

  int *faceNodes;
  
  dfloat *r, *s, *t, *w;
  dfloat *Dr, *Ds, *Dt;
  dfloat *Vr, *Vs, *Vt, *V;
  dfloat *MM, *LIFT;

  dfloat *cubr, *cubs, *cubt, *cubw;
  dfloat *cubInterp;
  dfloat *cubDrT, *cubDsT, *cubDtT, *cubProject;

  dfloat *intInterp, *intLIFT;
  
  dfloat *fileDr, *fileDs, *fileDt,  *fileMM, *fileLIFT;
  dfloat *fileCubInterp, *fileCubDrT, *fileCubDsT, *fileCubDtT, *fileCubProject;
  dfloat *fileIntLIFT, *fileIntInterp;
  
  int Nrows, Ncols;

  int N = atoi(argv[1]);
  int cubNp, Np;
  int Nfaces, Nfp, intNfp;
  
  char fname[BUFSIZ];

  { // 1D interval test
    meshJacobiGQ(0,0,N, &cubr, &cubw);
    meshJacobiGL(0,0,N, &r, &w); 
    for(int n=0;n<=N;++n)  printf("xgll[%d] = % e, wgll[%d] = % e \n", n, r[n], n, w[n]);
    for(int n=0;n<=N;++n)  printf("xgl[%d]  = % e, wgl[%d]  = % e \n", n, cubr[n], n, cubw[n]);

    Np = N+1;
    cubNp = N+1;
    
    meshVandermonde1D(N, Np, r, &V, &Vr);
    
    meshCubatureWeakDmatrices1D(N, Np, V, cubNp, cubr, cubw, &cubDrT, &cubProject); 

  }
  
  { // TRIANGLE TEST
    sprintf(fname, DHOLMES "/nodes/triangleN%02d.dat", N);
    
    FILE *fp = fopen(fname, "r");

    Nfaces = 3;
    Nfp = N+1;
    
    readDfloatArray(fp, "Nodal r-coordinates", &r,&Np,&Ncols);
    readDfloatArray(fp, "Nodal s-coordinates", &s,&Np,&Ncols);

    readIntArray(fp, "Nodal Face nodes", &faceNodes,&Nrows,&Ncols);
    
    readDfloatArray(fp, "Nodal Dr differentiation matrix", &(fileDr), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Ds differentiation matrix", &(fileDs), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Mass Matrix", &fileMM,&Np,&Ncols);

    readDfloatArray(fp, "Nodal Lift Matrix", &fileLIFT,&Np,&Ncols);
    
    readDfloatArray(fp, "Cubature r-coordinates", &cubr,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature s-coordinates", &cubs,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature weights", &cubw,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature Interpolation Matrix", &fileCubInterp,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature Weak Dr Differentiation Matrix", &fileCubDrT,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Weak Ds Differentiation Matrix", &fileCubDsT,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Projection Matrix", &fileCubProject,&Nrows,&Ncols);

    readDfloatArray(fp, "Cubature Surface Interpolation Matrix", &fileIntInterp,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Surface Lift Matrix", &fileIntLIFT,&Nrows,&Ncols);
    
    fclose(fp);
    
    meshDmatricesTri2D(N, Np, r, s, &Dr, &Ds);

    meshVandermondeTri2D(N, Np, r, s, &V, &Vr, &Vs);
    meshInterpolationMatrixTri2D(N, Np, r, s, cubNp, cubr, cubs, &cubInterp);
    meshMassMatrix(Np, V, &MM);
    meshLiftMatrixTri2D(N, Np, faceNodes, r, s, &LIFT);

    meshCubatureWeakDmatricesTri2D(N, Np, V, cubNp, cubr, cubs, cubw, &cubDrT, &cubDsT, &cubProject); 

    intNfp = ceil(3.*Nfp/2.);

    printf("intNfp = %d\n", intNfp);
    meshCubatureSurfaceMatricesTri2D(N, Np, r, s, V, faceNodes, intNfp, &intInterp, &intLIFT);

    matrixPrint(stdout, "TRI2D: intInterp", Nfaces*intNfp, Nfp, intInterp);
    
    matrixCompare(stdout, "TRI2D: |Dr-fileDr|", Np, Np, Dr, fileDr);
    matrixCompare(stdout, "TRI2D: |Ds-fileDs|", Np, Np, Ds, fileDs);
    matrixCompare(stdout, "TRI2D: |MM-fileMM|", Np, Np, MM, fileMM);

    matrixCompare(stdout, "TRI2D: |LIFT-fileLIFT|", Np, Nfaces*Nfp, LIFT, fileLIFT);

    matrixCompare(stdout, "TRI2D: |cubInterp-fileCubInterp|", cubNp, Np, cubInterp, fileCubInterp);
    matrixCompare(stdout, "TRI2D: |cubDrT-fileCubDrT|",       Np, cubNp, cubDrT,    fileCubDrT);
    matrixCompare(stdout, "TRI2D: |cubDsT-fileCubDsT|",       Np, cubNp, cubDsT,    fileCubDsT);

    matrixCompare(stdout, "TRI2D: |intInterp-fileIntInterp|", intNfp*Nfaces, Nfp, intInterp, fileIntInterp);
    matrixCompare(stdout, "TRI2D: |intLIFT-fileIntLIFT|", Np, intNfp*Nfaces, intLIFT, fileIntLIFT);
    
  }

  { // QUAD TEST
    sprintf(fname, DHOLMES "/nodes/quadrilateralN%02d.dat", N);
    
    FILE *fp = fopen(fname, "r");
    
    readDfloatArray(fp, "Nodal r-coordinates", &r,&Np,&Ncols);
    readDfloatArray(fp, "Nodal s-coordinates", &s,&Np,&Ncols);

    readDfloatArray(fp, "Nodal Dr differentiation matrix", &(fileDr), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Ds differentiation matrix", &(fileDs), &Np, &Ncols);
    
    fclose(fp);
    
    meshDmatricesQuad2D(N, Np, r, s, &Dr, &Ds);
    meshVandermondeQuad2D(N, Np, r, s, &V, &Vr, &Vs);

    matrixCompare(stdout, "QUAD2D: |Dr-fileDr|", Np, Np, Dr, fileDr);
    matrixCompare(stdout, "QUAD2D: |Ds-fileDs|", Np, Np, Ds, fileDs);
  }

  
  { // TETRAHEDRON TEST
    sprintf(fname, DHOLMES "/nodes/tetN%02d.dat", N);
    
    FILE *fp = fopen(fname, "r");

    Nfaces = 3;
    Nfp = (N+1)*(N+2)/2;
    
    readDfloatArray(fp, "Nodal r-coordinates", &r,&Np,&Ncols);
    readDfloatArray(fp, "Nodal s-coordinates", &s,&Np,&Ncols);
    readDfloatArray(fp, "Nodal t-coordinates", &t,&Np,&Ncols);

    readIntArray(fp, "Nodal Face nodes", &faceNodes,&Nrows,&Ncols);
    
    readDfloatArray(fp, "Nodal Dr differentiation matrix", &(fileDr), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Ds differentiation matrix", &(fileDs), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Dt differentiation matrix", &(fileDt), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Mass Matrix", &fileMM,&Np,&Ncols);
    readDfloatArray(fp, "Nodal Lift Matrix", &fileLIFT,&Np,&Ncols);

    readDfloatArray(fp, "Cubature r-coordinates", &cubr,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature s-coordinates", &cubs,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature t-coordinates", &cubt,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature weights", &cubw,&cubNp,&Ncols);

    readDfloatArray(fp, "Cubature Interpolation Matrix", &fileCubInterp,&cubNp,&Ncols);
    readDfloatArray(fp, "Cubature Weak Dr Differentiation Matrix", &fileCubDrT,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Weak Ds Differentiation Matrix", &fileCubDsT,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Weak Dt Differentiation Matrix", &fileCubDtT,&Nrows,&Ncols);
    readDfloatArray(fp, "Cubature Projection Matrix", &fileCubProject,&Nrows,&Ncols);
    
    fclose(fp);
    
    meshDmatricesTet3D(N, Np, r, s, t, &Dr, &Ds, &Dt);
    meshVandermondeTet3D(N, Np, r, s, t, &V, &Vr, &Vs, &Vt);
    meshMassMatrix(Np, V, &MM);
    meshLiftMatrixTet3D(N, Np, faceNodes, r, s, t, &LIFT);

    meshInterpolationMatrixTet3D(N, Np, r, s, t, cubNp, cubr, cubs, cubt, &cubInterp);

    meshCubatureWeakDmatricesTet3D(N, Np, V, cubNp, cubr, cubs, cubt, cubw, &cubDrT, &cubDsT, &cubDtT, &cubProject); 
    
    matrixCompare(stdout, "TET3D: |MM-fileMM|", Np, Np, MM, fileMM);
    matrixCompare(stdout, "TET3D: |Dr-fileDr|", Np, Np, Dr, fileDr);
    matrixCompare(stdout, "TET3D: |Ds-fileDs|", Np, Np, Ds, fileDs);
    matrixCompare(stdout, "TET3D: |Dt-fileDt|", Np, Np, Dt, fileDt);

    matrixCompare(stdout, "TET3D: |LIFT-fileLIFT|", Np, Nfaces*Nfp, LIFT, fileLIFT);

    matrixCompare(stdout, "TET3D: |cubInterp-fileCubInterp|", cubNp, Np, cubInterp, fileCubInterp);
    matrixCompare(stdout, "TET3D: |cubDrT-fileCubDrT|",       Np, cubNp, cubDrT,    fileCubDrT);
    matrixCompare(stdout, "TET3D: |cubDsT-fileCubDsT|",       Np, cubNp, cubDsT,    fileCubDsT);
    matrixCompare(stdout, "TET3D: |cubDtT-fileCubDtT|",       Np, cubNp, cubDtT,    fileCubDtT);
    
  }

  if(0){ // HEX TEST [ does not have Dr,Ds,Dt in node files ]
    sprintf(fname, DHOLMES "/nodes/hexN%02d.dat", N);
    
    FILE *fp = fopen(fname, "r");
    
    readDfloatArray(fp, "Nodal r-coordinates", &r,&Np,&Ncols);
    readDfloatArray(fp, "Nodal s-coordinates", &s,&Np,&Ncols);
    readDfloatArray(fp, "Nodal t-coordinates", &t,&Np,&Ncols);

    readDfloatArray(fp, "Nodal Dr differentiation matrix", &(fileDr), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Ds differentiation matrix", &(fileDs), &Np, &Ncols);
    readDfloatArray(fp, "Nodal Dt differentiation matrix", &(fileDt), &Np, &Ncols);
    
    fclose(fp);
    
    meshDmatricesHex3D(N, Np, r, s, t, &Dr, &Ds, &Dt);
    meshVandermondeHex3D(N, Np, r, s, t, &V, &Vr, &Vs, &Vt);
    
    matrixCompare(stdout, "HEX3D: |Dr-fileDr|", Np, Np, Dr, fileDr);
    matrixCompare(stdout, "HEX3D: |Ds-fileDs|", Np, Np, Ds, fileDs);
    matrixCompare(stdout, "HEX3D: |Dt-fileDt|", Np, Np, Dt, fileDt);
  }

  
  return 0;
}


#endif






