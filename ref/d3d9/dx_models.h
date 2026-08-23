#ifndef DX_MODELS_H
#define DX_MODELS_H

#include <string.h>
#include "xash3d_types.h"
#include "const.h"
#include "cvardef.h"
#include "xash3d_mathlib.h"
extern "C" {
#include "r_local.h"
}

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include "../../common/ref_params.h"

extern IDirect3D9 *d3d;
extern IDirect3DDevice9 *device;
extern model_t *g_worldmodel;

/* -------------------------------------------------------------------------
 * Vertex layout
 * ------------------------------------------------------------------------- */
#define BRUSH_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

typedef struct
{
	float    x, y, z;      /* object-space position                         */
	D3DCOLOR diffuse;      /* vertex colour (white = unlit placeholder)     */
	float    u, v;         /* base texture UV                               */
} brush_vertex_t;

typedef struct
{
	int                       texnum;
	std::vector<brush_vertex_t> verts;
	std::vector<int>          indices;

	int vertCount;
	int indexCount;
	int primitiveCount;
} renderdata_t;

typedef struct
{
	bool                     loaded;
	model_t *mod;
	std::vector<int>         texIds;
	std::vector<renderdata_t> renderdata;
} model_drawdata_t;

/* -------------------------------------------------------------------------
 * A 3x4 row-major transform matrix (same layout as Valve's bone matrices).
 * Row 0-2 are the 3 axes; column 3 is the translation.
 * We use float[3][4] throughout so it matches R_ConcatTransforms / StudioCalcBone.
 * ------------------------------------------------------------------------- */
typedef float BoneMat34[3][4];

/* -------------------------------------------------------------------------
 * Brush / world model
 * ------------------------------------------------------------------------- */
void R_LoadBrushModel( const model_t *mod, const D3DCOLOR *color,
					   model_drawdata_t *mdd, int modelIndex );

void R_BuildSurfaceVerts( const model_t *mod,
						  const msurface_t *surf,
						  brush_vertex_t *verts,
						  int *out_count,
						  renderdata_t *rd );

void R_LoadMap( const model_t *mod );
void R_DrawWorld( cl_entity_s *ent, model_t *mod );

/* -------------------------------------------------------------------------
 * Studio model
 * ------------------------------------------------------------------------- */

 /*
  * Decode one frame's worth of bone transforms from GoldSrc/Xash RLE anim data,
  * with sub-frame linear interpolation between floor(frame) and floor(frame)+1.
  *
  * hdr        – studio header
  * seq        – sequence index (use 0 for the rest / reference pose)
  * frame      – fractional frame in the engine's 0-255 range; values between
  *              integers produce a smooth interpolated pose
  * boneMats   – output array, caller supplies [hdr->numbones] BoneMat34 entries
  */
/*void StudioCalcBoneTransforms( studiohdr_t *hdr,
							   int          seq,
							   float          frame,
							   BoneMat34 *boneMats );*/

/*
 * Build the batched GPU-ready renderdata for a studio model, baking the
 * rest-pose (seq0/frame0) bone transforms into the vertex positions so the
 * model appears in its correct default orientation without a vertex shader.
 */
void ExtractStudioModelBatched( studiohdr_t *hdr, model_drawdata_t &out );

void R_LoadStudioModel( model_t *mod );

/*
 * Draw a studio model at the given sequence and interpolated frame (0-255).
 * For animated models the geometry is re-skinned each call using the
 * interpolated bone transforms from StudioCalcBoneTransforms.
 */
void R_DrawStudioModel( model_t *mod, int sequence, float frame );

int R_StudioGetTextureIndex( studiohdr_t *hdr,
							  mstudiomesh_t *mesh,
							  int             entitySkin );

void StudioCalcBoneTransforms( studiohdr_t *hdr, int seqIndex, float frame, BoneMat34 *boneMats,
								int gaitSeqIndex = -1, float gaitFrame = 0.0f, float gaitYaw = 0.0f );

void StudioSkinModel( studiohdr_t *hdr, int seq, float frame, model_drawdata_t &out,
					   int gaitSeq = -1, float gaitFrame = 0.0f, float gaitYaw = 0.0f );

void R_DrawStudioModel( model_t *mod, int sequence, float frame,
						 int gaitSeq = -1, float gaitFrame = 0.0f, float gaitYaw = 0.0f );

//extern convar_t r_studio_debug_val1;

#endif /* DX_MODELS_H */
