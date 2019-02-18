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

#include "stokes.h"

/* Host versions of stokesVec operations (for debugging only). */
static void stokesVecCopyHost(stokes_t *stokes, stokesVec_t u, stokesVec_t v);
static void stokesVecGatherScatterHost(stokes_t *stokes, stokesVec_t v);
static void stokesVecInnerProductHost(stokes_t *stokes, stokesVec_t u, stokesVec_t v, dfloat *c);
static void stokesVecScaleHost(stokes_t *stokes, stokesVec_t v, dfloat c);
static void stokesVecScaledAddHost(stokes_t *stokes, dfloat a, stokesVec_t u, dfloat b, stokesVec_t v);
static void stokesVecZeroHost(stokes_t *stokes, stokesVec_t v);

/* Device versions of stokesVec operations. */
static void stokesVecCopyDevice(stokes_t *stokes, stokesVec_t u, stokesVec_t v);
static void stokesVecGatherScatterDevice(stokes_t *stokes, stokesVec_t v);
static void stokesVecUnmaskedGatherScatterDevice(stokes_t *stokes, stokesVec_t v);
static void stokesVecInnerProductDevice(stokes_t *stokes, stokesVec_t u, stokesVec_t v, dfloat *c);
static void stokesVecScaleDevice(stokes_t *stokes, stokesVec_t v, dfloat c);
static void stokesVecScaledAddDevice(stokes_t *stokes, dfloat a, stokesVec_t u, dfloat b, stokesVec_t v);
static void stokesVecZeroDevice(stokes_t *stokes, stokesVec_t v);

/*****************************************************************************/

void stokesVecAllocate(stokes_t *stokes, stokesVec_t *v)
{
  v->v = (dfloat*)calloc(stokes->Ndof, sizeof(dfloat));

  v->x = v->v;
  v->y = v->v + stokes->NtotalV;
  if (stokes->meshV->dim == 2) {
    v->z = NULL;
    v->p = v->v + 2*stokes->NtotalV;
  } else if (stokes->meshV->dim == 3) {
    v->z = v->v + 2*stokes->NtotalV;
    v->p = v->v + 3*stokes->NtotalV;
  }

  v->o_v = stokes->meshV->device.malloc(stokes->Ndof*sizeof(dfloat), v->v);

  v->o_x = v->o_v;
  v->o_y = v->o_v + stokes->NtotalV*sizeof(dfloat);
  if (stokes->meshV->dim == 2) {
    v->o_p = v->o_v + 2*stokes->NtotalV*sizeof(dfloat);
  } else if (stokes->meshV->dim == 3) {
    v->o_z = v->o_v + 2*stokes->NtotalV*sizeof(dfloat);
    v->o_p = v->o_v + 3*stokes->NtotalV*sizeof(dfloat);
  }

  return;
}

void stokesVecFree(stokes_t *stokes, stokesVec_t *v)
{
  free(v->v);
  v->v = NULL;
  v->x = NULL;
  v->y = NULL;
  v->z = NULL;
  v->p = NULL;

  v->o_v.free();

  /* TODO:  What to do with the rest of the device pointers? */

  return;
}

void stokesVecCopyHostToDevice(stokesVec_t v)
{
  v.o_v.copyFrom(v.v);
  return;
}

void stokesVecCopyDeviceToHost(stokesVec_t v)
{
  v.o_v.copyTo(v.v);
  return;
}

/*****************************************************************************/

/* Copies v <-- u. */
void stokesVecCopy(stokes_t *stokes, stokesVec_t u, stokesVec_t v)
{
  stokesVecCopyDevice(stokes, u, v);
  return;
}

void stokesVecGatherScatter(stokes_t *stokes, stokesVec_t v)
{
  stokesVecGatherScatterDevice(stokes, v);
  return;
}

void stokesVecUnmaskedGatherScatter(stokes_t *stokes, stokesVec_t v)
{
  stokesVecUnmaskedGatherScatterDevice(stokes, v);
  return;
}

/* Computes c = v'*u. */
void stokesVecInnerProduct(stokes_t *stokes, stokesVec_t u, stokesVec_t v, dfloat *c)
{
  stokesVecInnerProductDevice(stokes, u, v, c);
  return;
}

/* Computes v <-- c*v for vector v, scalar c. */
void stokesVecScale(stokes_t *stokes, stokesVec_t v, dfloat c)
{
  stokesVecScaleDevice(stokes, v, c);
  return;
}

/* Computes v <-- au + bv for vectors u, v and scalars a, b. */
void stokesVecScaledAdd(stokes_t *stokes, dfloat a, stokesVec_t u, dfloat b, stokesVec_t v)
{
  stokesVecScaledAddDevice(stokes, a, u, b, v);
  return;
}

/* Sets v <-- 0. */
void stokesVecZero(stokes_t *stokes, stokesVec_t v)
{
  stokesVecZeroDevice(stokes, v);
  return;
}

/*****************************************************************************/

static void stokesVecCopyHost(stokes_t *stokes, stokesVec_t u, stokesVec_t v)
{
  for (int i = 0; i < stokes->Ndof; i++)
    v.v[i] = u.v[i];

  return;
}

static void stokesVecGatherScatterHost(stokes_t *stokes, stokesVec_t v)
{
  ogsGatherScatter(v.x, ogsDfloat, ogsAdd, stokes->ogs);
  ogsGatherScatter(v.y, ogsDfloat, ogsAdd, stokes->ogs);
  if (stokes->meshV->dim == 3)
    ogsGatherScatter(v.z, ogsDfloat, ogsAdd, stokes->ogs);
  ogsGatherScatter(v.p, ogsDfloat, ogsAdd, stokes->meshP->ogs);

  return;
}

/* TODO:  The inverse degree weighting is only applicable for C0 FEM. */
static void stokesVecInnerProductHost(stokes_t *stokes, stokesVec_t u, stokesVec_t v, dfloat *c)
{
  *c = 0.0;

  if (stokes->meshV->dim == 2) {
    for (int i = 0; i < stokes->NtotalV; i++)
      *c += (u.x[i]*v.x[i] + u.y[i]*v.y[i])*stokes->meshV->ogs->invDegree[i];
  } else if (stokes->meshV->dim == 3) {
    for (int i = 0; i < stokes->NtotalV; i++)
      *c += (u.x[i]*v.x[i] + u.y[i]*v.y[i] + u.z[i]*v.z[i])*stokes->meshV->ogs->invDegree[i];
  }

  for (int i = 0; i < stokes->NtotalP; i++)
    *c += u.p[i]*v.p[i]*stokes->meshP->ogs->invDegree[i];

  return;
}

static void stokesVecScaleHost(stokes_t *stokes, stokesVec_t v, dfloat c)
{
  for (int i = 0; i < stokes->Ndof; i++)
    v.v[i] *= c;

  return;
}

static void stokesVecScaledAddHost(stokes_t *stokes, dfloat a, stokesVec_t u, dfloat b, stokesVec_t v)
{
  for (int i = 0; i < stokes->Ndof; i++)
    v.v[i] = a*u.v[i] + b*v.v[i];

  return;
}

static void stokesVecZeroHost(stokes_t *stokes, stokesVec_t v)
{
  memset(v.v, 0, stokes->Ndof*sizeof(dfloat));
  return;
}

/*****************************************************************************/

static void stokesVecCopyDevice(stokes_t *stokes, stokesVec_t u, stokesVec_t v)
{
  v.o_v.copyFrom(u.o_v);
  return;
}

/* TODO:  The inverse degree weighting is only applicable for C0 FEM.
 *
 * TODO:  Might it be better to store the inverse degree weights in one big
 * long vector of length stokes->Ndof and call the inner product kernel just
 * once?  This would use more memory, but it might be more efficient, and it
 * would simplify the code.)
 */
static void stokesVecInnerProductDevice(stokes_t *stokes, stokesVec_t u, stokesVec_t v, dfloat *c)
{
  *c = 0.0;

  /* TODO:  Replace host loops with further kernel calls if we had too many
   * blocks.  (See, e.g., ellipticWeightedInnerProduct().)
   */
  stokes->weightedInnerProductKernel(stokes->NtotalV, stokes->ogs->o_invDegree, u.o_x, v.o_x, stokes->o_workV);
  stokes->o_workV.copyTo(stokes->workV);
  for (int i = 0; i < stokes->NblockV; i++)
    *c += stokes->workV[i];

  stokes->weightedInnerProductKernel(stokes->NtotalV, stokes->ogs->o_invDegree, u.o_y, v.o_y, stokes->o_workV);
  stokes->o_workV.copyTo(stokes->workV);
  for (int i = 0; i < stokes->NblockV; i++)
    *c += stokes->workV[i];

  if (stokes->meshV->dim == 3) {
    stokes->weightedInnerProductKernel(stokes->NtotalV, stokes->ogs->o_invDegree, u.o_z, v.o_z, stokes->o_workV);
    stokes->o_workV.copyTo(stokes->workV);
    for (int i = 0; i < stokes->NblockV; i++)
      *c += stokes->workV[i];
  }

  stokes->weightedInnerProductKernel(stokes->NtotalP, stokes->meshP->ogs->o_invDegree, u.o_p, v.o_p, stokes->o_workP);
  stokes->o_workP.copyTo(stokes->workP);
  for (int i = 0; i < stokes->NblockP; i++)
    *c += stokes->workP[i];

  /* TODO:  MPI. */

  return;
}

static void stokesVecGatherScatterDevice(stokes_t *stokes, stokesVec_t v)
{
  if (stokes->options.compareArgs("VELOCITY DISCRETIZATION", "CONTINUOUS")) {
    ogsGatherScatter(v.o_x, ogsDfloat, ogsAdd, stokes->ogs);
    ogsGatherScatter(v.o_y, ogsDfloat, ogsAdd, stokes->ogs);
    if (stokes->meshV->dim == 3)
      ogsGatherScatter(v.o_z, ogsDfloat, ogsAdd, stokes->ogs);
  }

  if (stokes->options.compareArgs("PRESSURE DISCRETIZATION", "CONTINUOUS")) {
    ogsGatherScatter(v.o_p, ogsDfloat, ogsAdd, stokes->meshP->ogs);
  }

  return;
}

static void stokesVecUnmaskedGatherScatterDevice(stokes_t *stokes, stokesVec_t v)
{
  if (stokes->options.compareArgs("VELOCITY DISCRETIZATION", "CONTINUOUS")) {
    ogsGatherScatter(v.o_x, ogsDfloat, ogsAdd, stokes->meshV->ogs);
    ogsGatherScatter(v.o_y, ogsDfloat, ogsAdd, stokes->meshV->ogs);
    if (stokes->meshV->dim == 3)
      ogsGatherScatter(v.o_z, ogsDfloat, ogsAdd, stokes->meshV->ogs);
  }

  if (stokes->options.compareArgs("PRESSURE DISCRETIZATION", "CONTINUOUS")) {
    ogsGatherScatter(v.o_p, ogsDfloat, ogsAdd, stokes->meshP->ogs);
  }

  return;
}

static void stokesVecScaleDevice(stokes_t *stokes, stokesVec_t v, dfloat c)
{
  stokes->vecScaleKernel(stokes->Ndof, c, v.o_v);
  return;
}

static void stokesVecScaledAddDevice(stokes_t *stokes, dfloat a, stokesVec_t u, dfloat b, stokesVec_t v)
{
  stokes->vecScaledAddKernel(stokes->Ndof, a, u.o_v, b, v.o_v);
  return;
}

static void stokesVecZeroDevice(stokes_t *stokes, stokesVec_t v)
{
  stokes->vecZeroKernel(stokes->Ndof, v.o_v);
  return;
}

/*****************************************************************************/

void stokesVecPrint(stokes_t *stokes, stokesVec_t v)
{
  for (int i = 0; i < stokes->Ndof; i++)
    printf("% .20e\n", v.v[i]);

  return;
}
