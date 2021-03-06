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


// Initial conditions 
#define insFlowField3D(t,x,y,z, u,v,w,p)				\
  {									\
    dfloat bmRho = 40;							\
    dfloat bmDelta  = 0.05;						\
    dfloat rho = 1;							\
    dfloat Utangential = 0.25*(1+tanh(bmRho*(-z+0.5)))*(1+tanh(bmRho*(0.5+z))); \
    dfloat uout, vout;							\
    dfloat sphereRadius = 1;						\
    									\
    if(x*x+y*y>1e-4) {							\
      uout =  -y*Utangential/(x*x+y*y);					\
      vout =   x*Utangential/(x*x+y*y);					\
    }									\
    else{								\
      uout = 0;								\
      vout = 0;								\
    }									\
    									\
    dfloat wout = bmDelta*sin(8*atan2(y,x))*(1-z*z);			\
    									\
    dfloat udotx = uout*x+vout*y+wout*z;				\
    *u = uout - udotx*x/(sphereRadius*sphereRadius);			\
    *v = vout - udotx*y/(sphereRadius*sphereRadius);			\
    *w = wout - udotx*z/(sphereRadius*sphereRadius);			\
  }   

