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

#include "ogs.hpp"
#include "ogsKernels.hpp"

namespace ogs {

OGS_DEFINE_TYPE_SIZES()

void hostScatter(void* v,
                 void* gv,
                 const int Nentries,
                 const int Nvectors,
                 const dlong stride,
                 const dlong gstride,
                 const ogs_type type,
                 const ogs_op op,
                 ogs_t &ogs){

  const size_t Nbytes = ogs_type_size[type];

  ogs.reallocHostBuffer(Nbytes*Nentries*Nvectors);

  // scatter interior nodes
  if (ogs.NlocalGather)
    hostScatterKernel(ogs.NlocalGather, Nentries, Nvectors, gstride, stride,
                      ogs.localGatherOffsets, ogs.localGatherIds,
                      type, op, gv, v);

  if (ogs.NownedHalo)
    for (int i=0;i<Nvectors;i++)
      memcpy((char*)ogs.hostBuf+ogs.NhaloGather*Nbytes*Nentries*i,
             (char*)gv+ogs.NlocalGather*Nbytes + gstride*Nbytes*i,
             ogs.NownedHalo*Nbytes*Nentries);

  // MPI based scatter using gslib
  gsScatter(ogs.hostBuf, Nentries, Nvectors, ogs.NhaloGather,
            type, op, ogs.gshNonSym);

  if (ogs.NhaloGather)
    hostScatterKernel(ogs.NhaloGather, Nentries, Nvectors, ogs.NhaloGather, stride,
                      ogs.haloGatherOffsets, ogs.haloGatherIds,
                      type, op, ogs.hostBuf, v);
}

/*------------------------------------------------------------------------------
  The basic gather kernel
------------------------------------------------------------------------------*/
#define DEFINE_SCATTER(T)                                                       \
static void hostScatterKernel_##T(const dlong Ngather,                          \
                                  const int   Nentries,                         \
                                  const int   Nvectors,                         \
                                  const dlong gstride,                          \
                                  const dlong stride,                           \
                                  const dlong *gatherStarts,                    \
                                  const dlong *gatherIds,                       \
                                  const     T *gatherq,                         \
                                            T *q)                               \
{                                                                               \
  for(dlong g=0;g<Ngather*Nentries*Nvectors;++g){                               \
    const int m     = g/(Ngather*Nentries);                                     \
    const dlong vid = g%(Ngather*Nentries);                                     \
    const dlong gid = vid/Nentries;                                             \
    const int k     = vid%Nentries;                                             \
    const T gq = gatherq[k+gid*Nentries+m*gstride];                             \
    const dlong start = gatherStarts[gid];                                      \
    const dlong end = gatherStarts[gid+1];                                      \
    for(dlong n=start;n<end;++n){                                               \
      const dlong id = gatherIds[n];                                            \
      q[k+id*Nentries+m*stride] = gq;                                           \
    }                                                                           \
  }                                                                             \
}

#define DEFINE_PROCS(T) \
  DEFINE_SCATTER(T)

OGS_FOR_EACH_TYPE(DEFINE_PROCS)

#define SWITCH_TYPE_CASE(T) case ogs_##T: { WITH_TYPE(T); break; }
#define SWITCH_TYPE(type) switch(type) { \
    OGS_FOR_EACH_TYPE(SWITCH_TYPE_CASE) case ogs_type_n: break; }

void hostScatterKernel(const dlong Ngather,
                       const int Nentries,
                       const int Nvectors,
                       const dlong gstride,
                       const dlong stride,
                       const dlong *gatherStarts,
                       const dlong *gatherIds,
                       const ogs_type type,
                       const ogs_op op,
                       const void *gv,
                       void *v) {

#define WITH_TYPE(T)                          \
  hostScatterKernel_##T(Ngather,              \
                        Nentries,             \
                        Nvectors,             \
                        gstride,              \
                        stride,               \
                        gatherStarts,         \
                        gatherIds,            \
                        (T*) gv,              \
                        (T*) v);

  SWITCH_TYPE(type)

#undef  WITH_TYPE
}

} //namespace ogs