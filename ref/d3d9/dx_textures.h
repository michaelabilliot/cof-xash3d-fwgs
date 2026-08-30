#ifndef DX_TEXTURES_H
#define DX_TEXTURES_H

#include <string.h>
#include "xash3d_types.h"
#include "const.h"
#include "cvardef.h"
#include "xash3d_mathlib.h"
#include "crtlib.h"
extern "C" {
#include "r_local.h"
}

#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <deque>
#include "dx_fast_hash.h"
#include <iostream>
#include <algorithm>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include "../../common/ref_params.h"

extern IDirect3D9 *d3d;
extern IDirect3DDevice9 *device;

extern int g_texcount;

int DX_LoadTextureFromBuffer( const char *name, rgbdata_t *pic, texFlags_t flags, qboolean update );
int DX_LoadTexture( const char *name, const byte *buf, size_t size, int flags );
int DX_CreateTexture( const char *name, int width, int height, const void *buffer, texFlags_t flags );
/* Equivalent of the ref API's GL_UpdateTexture callback.  cols/rows is the
 * uploaded rectangle size; width/height is the backing texture size. */
qboolean DX_UpdateTexture( int texnum, int cols, int rows, int width, int height,
	const byte *buffer, pixformat_t fmt );
/* Rebuild a gamma-sensitive texture from its saved unprocessed pixels. */
void DX_ProcessTexture( int texnum, float gamma, int topColor, int bottomColor );
/* Applies the legacy menu black colour-key only from the 2D drawing path. */
void DX_ApplyUIColorKey( int texnum );

int DX_FindTexture( const char *name );
const char *DX_TextureName( unsigned int texnum );
void R_StudioLoadTexture( model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture );
void DX_Bind( int tmu, unsigned int texnum );
void DX_FreeTexture( unsigned int texnum );

typedef struct
{
	IDirect3DTexture9 *tex;
	char name[64];
	int width;
	int height;
	int srcWidth;
	int srcHeight;
	char has_alpha;
	DWORD flags;
	WORD encode;
	/* Matches ref_gl's per-texture underwater fog metadata.  The engine image
	 * loader fills these four bytes as R,G,B,density for liquid textures. */
	rgba_t fogParams;
	rgbdata_t *original;
	int texnum;
} dxtexture_t;

extern std::deque<dxtexture_t> g_Textures;
//extern std::unordered_map< std::string, int > g_StudioTexIndex;
extern dxfast::HashMap<std::string, int> g_StudioTexIndex;



#endif
