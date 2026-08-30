/*
r_context.cpp -- Direct3D 9 renderer context
Copyright (C) 2023-2024 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either  of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <string.h>
#include "xash3d_types.h"
#include "const.h"
#include "beamdef.h"
#include "event_flags.h"
#include "entity_types.h"
#include "customentity.h"
#include "cvardef.h"
#include "xash3d_mathlib.h"
extern "C" {
#include "r_local.h"
}

#include <d3d9.h>
#include <d3dx9.h>
#include <TlHelp32.h>
#include <vector>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <cstdlib>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include "../../common/ref_params.h"
#include "../../common/pmove.h"
#include "../../common/r_studioint.h"
#include "dx_textures.h"
#include "dx_models.h"
#include "enginefeatures.h"
#include "dx_local.h"
#include "dx_profiler.h"

#ifndef SURF_DRAWTURB
#define SURF_DRAWTURB 0x0010
#endif

float g_frametime;

typedef struct
{
	float x, y, z, rhw;
	DWORD color;
	float u, v;
} vertex2d_t;

#define D3DFVF_VERTEX2D \
    (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

IDirect3D9 *d3d = nullptr;
IDirect3DDevice9 *device = nullptr;
static D3DPRESENT_PARAMETERS d3dpp;
HWND hwnd = nullptr;
static bool g_dxVideoInitialized = false;

ID3DXLine *g_dxLine = nullptr;

DWORD g_Color = D3DCOLOR_ARGB( 255, 255, 255, 255 );

static int g_2dRenderMode = kRenderNormal;

static bool g_2dModeActive = false;

static constexpr size_t DX_2D_BATCH_MAX_QUADS = 2048;
static std::vector<vertex2d_t> g_2dBatchVertices;
static bool g_2dBatchActive = false;
static int g_2dBatchTexture = 0;
static int g_2dBatchRenderMode = kRenderNormal;
static void R_Flush2DBatch( );
static void R_Reset2DBatch( );

static bool g_dxSceneActive = false;
unsigned int g_currentViewFlags = 0;

bool g_dxOverviewView = false;

static float g_dxOrthoMins[2] = {};
static float g_dxOrthoMaxs[2] = {};
static float g_dxOrthoCenter[2] = {};
static float g_dxOrthoHalf[2] = {};
static bool g_dxOrthoValid = false;

static cvar_t *g_dxRSpeedsCvar = nullptr;


static std::vector<cl_entity_s *> g_opaqueBrushEntities;
static std::vector<cl_entity_s *> g_opaqueStudioEntities;
static std::vector<cl_entity_s *> g_opaqueSpriteEntities;
static std::vector<cl_entity_s *> g_translucentCandidates;
static std::vector<cl_entity_s *> g_entityLightEmitters;
static std::vector<cl_entity_s *> g_waterBrushEntities;
static std::vector<cl_entity_s *> g_lateWaterBrushEntities;

static int g_opaqueBrushCount = 0;
static int g_opaqueStudioCount = 0;
static int g_opaqueSpriteCount = 0;
static int g_translucentCandidateCount = 0;
static int g_entityLightEmitterCount = 0;
static int g_waterBrushCount = 0;
static int g_lateWaterBrushCount = 0;

struct translucent_entity_t
{
	cl_entity_s *entity = nullptr;
	int rank = 0;
	float distanceSquared = 0.0f;
	int submissionOrder = 0;
};

static std::vector<translucent_entity_t> g_translucentDrawList;

cl_entity_s *g_beamEntities[DX_MAX_BEAM_ENTITIES] = {};
int g_beamEntityCount = 0;

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 64
#endif

static cl_entity_s g_playerDrawEntityCopies[MAX_CLIENTS + 1];
static bool g_playerSubmitted[MAX_CLIENTS + 1] = {};

struct player_model_cache_t
{
	model_t *model = nullptr;
	char name[MAX_OSPATH] = {};
	double nextCheckTime = 0.0;
	bool resolved = false;
};

static player_model_cache_t g_playerModelCache[MAX_CLIENTS] = {};

static constexpr int DX_PLAYER_INTERP_HISTORY = 8;

struct player_interp_sample_t
{
	entity_state_t state{};
	float arrivalTime = 0.0f;
};

struct player_interp_t
{
	bool valid = false;
	entity_state_t previous{};
	entity_state_t current{};
	player_interp_sample_t samples[DX_PLAYER_INTERP_HISTORY]{};
	int sampleCount = 0;
	int nextSample = 0;
};

static player_interp_t g_remotePlayerInterp[MAX_CLIENTS + 1];

vec3_t g_vRight, g_vUp, g_vForward, g_vOrigin, g_vAngles;

bool g_dxCullValid = false;
vec3_t g_dxCullOrigin;
vec3_t g_dxCullForward;
vec3_t g_dxCullRight;
vec3_t g_dxCullUp;
float g_dxCullTanX = 1.0f;
float g_dxCullTanY = 1.0f;
float g_dxCullNear = 4.0f;
float g_dxCullFar = 8192.0f;

static D3DVIEWPORT9 g_sceneViewport = {};
static D3DXMATRIX g_sceneProjection = {};
static bool g_sceneViewportValid = false;

static constexpr UINT DX_CONTEXT_SCRATCH_VB_DEFAULT_BYTES = 512u * 1024u;
static constexpr UINT DX_CONTEXT_SCRATCH_IB_DEFAULT_BYTES = 128u * 1024u;
static IDirect3DVertexBuffer9 *g_contextScratchVB = nullptr;
static IDirect3DIndexBuffer9 *g_contextScratchIB = nullptr;
static UINT g_contextScratchVBBytes = 0;
static UINT g_contextScratchIBBytes = 0;
static D3DFORMAT g_contextScratchIBFormat = D3DFMT_INDEX16;
static UINT g_contextScratchVBOffset = 0;
static UINT g_contextScratchIBOffset = 0;

static UINT R_AlignScratchOffset( UINT value, UINT alignment ) { return ( value + alignment - 1u ) & ~( alignment - 1u ); }

static UINT R_ScratchBufferSize( UINT neededBytes, UINT defaultBytes )
{
	UINT size = defaultBytes;
	while( size < neededBytes && size < 8u * 1024u * 1024u )
		size <<= 1;
	return std::max( size, neededBytes );
}

static void R_FreeScratchBuffers( void )
{
	if( g_contextScratchVB )
	{
		g_contextScratchVB->Release( );
		g_contextScratchVB = nullptr;
	}
	if( g_contextScratchIB )
	{
		g_contextScratchIB->Release( );
		g_contextScratchIB = nullptr;
	}
	g_contextScratchVBBytes = 0;
	g_contextScratchIBBytes = 0;
	g_contextScratchIBFormat = D3DFMT_INDEX16;
	g_contextScratchVBOffset = 0;
	g_contextScratchIBOffset = 0;
}

static void R_BeginScratchFrame( void )
{
	g_contextScratchVBOffset = 0;
	g_contextScratchIBOffset = 0;
}

static bool R_EnsureScratchVB( UINT neededBytes )
{
	if( !device || neededBytes == 0 )
		return false;
	if( g_contextScratchVB && g_contextScratchVBBytes >= neededBytes )
		return true;
	if( g_contextScratchVB )
	{
		g_contextScratchVB->Release( );
		g_contextScratchVB = nullptr;
	}
	g_contextScratchVBBytes = R_ScratchBufferSize( neededBytes, DX_CONTEXT_SCRATCH_VB_DEFAULT_BYTES );
	g_contextScratchVBOffset = 0;
	HRESULT result = device->CreateVertexBuffer( g_contextScratchVBBytes,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
		&g_contextScratchVB, nullptr );
	if( FAILED( result ) || !g_contextScratchVB )
	{
		g_contextScratchVBBytes = 0;
		return false;
	}
	return true;
}

static bool R_EnsureScratchIB( UINT neededBytes, D3DFORMAT format )
{
	if( !device || neededBytes == 0 )
		return false;
	if( g_contextScratchIB && g_contextScratchIBBytes >= neededBytes &&
		g_contextScratchIBFormat == format )
	{
		return true;
	}
	if( g_contextScratchIB )
	{
		g_contextScratchIB->Release( );
		g_contextScratchIB = nullptr;
	}
	g_contextScratchIBBytes = R_ScratchBufferSize( neededBytes, DX_CONTEXT_SCRATCH_IB_DEFAULT_BYTES );
	g_contextScratchIBFormat = format;
	g_contextScratchIBOffset = 0;
	HRESULT result = device->CreateIndexBuffer( g_contextScratchIBBytes,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, format, D3DPOOL_DEFAULT,
		&g_contextScratchIB, nullptr );
	if( FAILED( result ) || !g_contextScratchIB )
	{
		g_contextScratchIBBytes = 0;
		return false;
	}
	return true;
}

static bool R_UploadScratchVerts( const void *vertices, UINT bytes,
	UINT stride, UINT &offsetBytes )
{
	if( !R_EnsureScratchVB( bytes ) )
		return false;
	UINT alignedOffset = R_AlignScratchOffset( g_contextScratchVBOffset, 16u );
	DWORD lockFlags = D3DLOCK_NOOVERWRITE;
	if( alignedOffset + bytes > g_contextScratchVBBytes || alignedOffset == 0 )
	{
		alignedOffset = 0;
		lockFlags = D3DLOCK_DISCARD;
	}
	DX_ProfileLock( DX_PROFILE_SCRATCH_CONTEXT, DX_PROFILE_SCRATCH_VB, lockFlags, bytes );
	void *memory = nullptr;
	HRESULT result = g_contextScratchVB->Lock( alignedOffset, bytes, &memory, lockFlags );
	if( FAILED( result ) || !memory )
		return false;
	memcpy( memory, vertices, bytes );
	g_contextScratchVB->Unlock( );
	offsetBytes = alignedOffset;
	g_contextScratchVBOffset = R_AlignScratchOffset( alignedOffset + bytes, 16u );
	return true;
}

static bool R_UploadScratchIndices( const void *indices, UINT bytes,
	D3DFORMAT format, UINT &startIndex )
{
	if( !R_EnsureScratchIB( bytes, format ) )
		return false;
	const UINT indexSize = ( format == D3DFMT_INDEX16 ) ? 2u : 4u;
	UINT alignedOffset = R_AlignScratchOffset( g_contextScratchIBOffset, indexSize );
	DWORD lockFlags = D3DLOCK_NOOVERWRITE;
	if( alignedOffset + bytes > g_contextScratchIBBytes || alignedOffset == 0 )
	{
		alignedOffset = 0;
		lockFlags = D3DLOCK_DISCARD;
	}
	DX_ProfileLock( DX_PROFILE_SCRATCH_CONTEXT, DX_PROFILE_SCRATCH_IB, lockFlags, bytes );
	void *memory = nullptr;
	HRESULT result = g_contextScratchIB->Lock( alignedOffset, bytes, &memory, lockFlags );
	if( FAILED( result ) || !memory )
		return false;
	memcpy( memory, indices, bytes );
	g_contextScratchIB->Unlock( );
	startIndex = alignedOffset / indexSize;
	g_contextScratchIBOffset = R_AlignScratchOffset( alignedOffset + bytes, 16u );
	return true;
}

bool R_DrawScratch( D3DPRIMITIVETYPE type,
	UINT primitiveCount, UINT vertexCount, const void *vertices, UINT stride )
{
	if( !device || !vertices || primitiveCount == 0 || vertexCount == 0 )
		return false;
	UINT vertexOffset = 0;
	if( !R_UploadScratchVerts( vertices, vertexCount * stride, stride, vertexOffset ) )
		return false;
	device->SetStreamSource( 0, g_contextScratchVB, vertexOffset, stride );
	DX_ProfileDraw( false, primitiveCount );
	device->DrawPrimitive( type, 0, primitiveCount );
	return true;
}

static bool R_DrawScratchIndexed( D3DPRIMITIVETYPE type,
	UINT vertexCount, UINT primitiveCount, const void *indices,
	D3DFORMAT indexFormat, const void *vertices, UINT stride )
{
	if( !device || !vertices || !indices || vertexCount == 0 || primitiveCount == 0 )
		return false;
	UINT vertexOffset = 0;
	UINT startIndex = 0;
	const UINT indexSize = ( indexFormat == D3DFMT_INDEX16 ) ? 2u : 4u;
	if( !R_UploadScratchVerts( vertices, vertexCount * stride, stride, vertexOffset ) )
		return false;
	if( !R_UploadScratchIndices( indices, primitiveCount * 3u * indexSize,
		indexFormat, startIndex ) )
	{
		return false;
	}
	device->SetStreamSource( 0, g_contextScratchVB, vertexOffset, stride );
	device->SetIndices( g_contextScratchIB );
	DX_ProfileDraw( true, primitiveCount );
	device->DrawIndexedPrimitive( type, 0, 0, vertexCount, startIndex, primitiveCount );
	return true;
}

static constexpr int DX_SKYBOX_SIDES = 6;
static int g_skyTextures[DX_SKYBOX_SIDES] = {};

convar_t dx_remix = { (char *)"dx_remix", (char *)"0", 0 };
convar_t dx_gamma = { (char *)"dx_gamma", (char *)"1.0", 0 };
convar_t dx_brightness = { (char *)"dx_brightness", (char *)"1.0", 0 };

convar_t dx_fog = { (char *)"dx_fog", (char *)"1", 0 };

convar_t dx_msaa_samples = { (char *)"dx_msaa_samples", (char *)"16", 0 };
convar_t dx_msaa_quality = { (char *)"dx_msaa_quality", (char *)"-1", 0 };
convar_t dx_anisotropy = { (char *)"dx_anisotropy", (char *)"16", 0 };

convar_t dx_showtextures_zoom =
{ (char *)"dx_showtextures_zoom", (char *)"1.0", 0 };

convar_t r_viewmodel_fov = { (char *)"r_viewmodel_fov", (char *)"60", 0 };

convar_t r_viewmodel_near = { (char *)"r_viewmodel_near", (char *)"4.0", 0 };
convar_t r_viewmodel_offset_x = { (char *)"r_viewmodel_offset_x", (char *)"0", 0 };
convar_t r_viewmodel_offset_y = { (char *)"r_viewmodel_offset_y", (char *)"0", 0 };
convar_t r_viewmodel_offset_z = { (char *)"r_viewmodel_offset_z", (char *)"0.5", 0 };

convar_t dx_viewmodel_beam_offset =
{ (char *)"dx_viewmodel_beam_offset", (char *)"-3", 0 };

convar_t dx_player_stale_timeout =
{ (char *)"dx_player_stale_timeout", (char *)"0.50", 0 };

convar_t dx_player_dead_body_timeout =
{ (char *)"dx_player_dead_body_timeout", (char *)"1.50", 0 };

convar_t dx_player_use_engine_state =
{ (char *)"dx_player_use_engine_state", (char *)"1", 0 };

convar_t dx_player_interpolation =
{ (char *)"dx_player_interpolation", (char *)"1", 0 };

convar_t dx_player_interp_delay =
{ (char *)"dx_player_interp_delay", (char *)"0.10", 0 };
convar_t dx_player_interp_extrapolate =
{ (char *)"dx_player_interp_extrapolate", (char *)"0.25", 0 };
convar_t dx_player_interp_teleport_dist =
{ (char *)"dx_player_interp_teleport_dist", (char *)"192", 0 };

convar_t dx_client_efx = { (char *)"dx_client_efx", (char *)"1", 0 };
convar_t dx_particles = { (char *)"dx_particles", (char *)"1", 0 };
convar_t dx_particle_size_scale = { (char *)"dx_particle_size_scale", (char *)"1", 0 };
convar_t dx_particle_limit = { (char *)"dx_particle_limit", (char *)"8192", 0 };
convar_t dx_particle_additive = { (char *)"dx_particle_additive", (char *)"0", 0 };
convar_t dx_tracers = { (char *)"dx_tracers", (char *)"1", 0 };
convar_t dx_tracer_width = { (char *)"dx_tracer_width", (char *)"2.5", 0 };
convar_t dx_tracer_length_scale = { (char *)"dx_tracer_length_scale", (char *)"1", 0 };
convar_t dx_beam_particles = { (char *)"dx_beam_particles", (char *)"1", 0 };
convar_t dx_beams = { (char *)"dx_beams", (char *)"1", 0 };
convar_t dx_beam_trails = { (char *)"dx_beam_trails", (char *)"1", 0 };
convar_t dx_beam_trail_spacing = { (char *)"dx_beam_trail_spacing", (char *)"32", 0 };
convar_t dx_beam_width_scale = { (char *)"dx_beam_width_scale", (char *)"1", 0 };

convar_t dx_beam_sine = { (char *)"dx_beam_sine", (char *)"1", 0 };
convar_t dx_beam_sine_boost = { (char *)"dx_beam_sine_boost", (char *)"48", 0 };
convar_t dx_beam_sine_close_fade = { (char *)"dx_beam_sine_close_fade", (char *)"1", 0 };
convar_t dx_beam_sine_segments = { (char *)"dx_beam_sine_segments", (char *)"8", 0 };

convar_t dx_beam_sine_anchor = { (char *)"dx_beam_sine_anchor", (char *)"14", 0 };
convar_t dx_beam_sine_scale = { (char *)"dx_beam_sine_scale", (char *)"0.6", 0 };

convar_t dx_beam_close_precision = { (char *)"dx_beam_close_precision", (char *)"1", 0 };
convar_t dx_beam_close_precision_distance = { (char *)"dx_beam_close_precision_distance", (char *)"160", 0 };
convar_t dx_beam_close_precision_step = { (char *)"dx_beam_close_precision_step", (char *)"4", 0 };
convar_t dx_beam_endpoint_taper = { (char *)"dx_beam_endpoint_taper", (char *)"1", 0 };
convar_t dx_beam_endpoint_taper_distance = { (char *)"dx_beam_endpoint_taper_distance", (char *)"10", 0 };
convar_t dx_egon_beam_light = { (char *)"dx_egon_beam_light", (char *)"1", 0 };
convar_t dx_egon_beam_light_spacing = { (char *)"dx_egon_beam_light_spacing", (char *)"72", 0 };
convar_t dx_egon_beam_light_radius = { (char *)"dx_egon_beam_light_radius", (char *)"160", 0 };
convar_t dx_egon_beam_light_max_samples = { (char *)"dx_egon_beam_light_max_samples", (char *)"32", 0 };

static bool g_contextCvarsRegistered = false;
static bool g_showTextureCmdsRegistered = false;
static float g_lastGamma = -1.0f;
static float g_lastBrightness = -1.0f;
static float g_previousClientTime = -1.0f;
static int g_dxMSAASamplesReq = -9999;
static int g_dxMSAAQualityReq = -9999;
static int g_dxAppliedMSAASamples = 0;
static int g_dxAppliedMSAAQuality = 0;
static int g_dxAppliedAnisotropy = -1;
static bool g_dxMSAAForceDisabled = false;
static int g_dxMSAAForceOff = -9999;

enum dx_fog_source_t
{
	DX_FOG_NONE = 0,
	DX_FOG_INTERNAL_WATER,
	DX_FOG_INTERNAL_QUAKE,
	DX_FOG_CUSTOM
};

struct dx_fog_state_t
{
	dx_fog_source_t source = DX_FOG_NONE;
	bool customEnabled = false;
	bool skybox = false;
	bool allow = true;
	int suppressionDepth = 0;
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float density = 0.0f;
	float start = 0.0f;
	float end = 0.0f;
	float densityScale = 1.0f;
	int cachedContents = CONTENTS_EMPTY;
	int cachedWaterLevel = 0;
};

static dx_fog_state_t g_dxFog;
static bool g_dxFogTableSupported = true;
static bool g_dxFogVertexSupported = true;

static inline bool DX_IsLiquid( int contents )
{
	return contents == CONTENTS_WATER ||
		contents == CONTENTS_SLIME ||
		contents == CONTENTS_LAVA;
}

static inline DWORD DX_FloatBits( float value )
{
	DWORD bits = 0;
	static_assert( sizeof( bits ) == sizeof( value ), "D3D9 float render-state size mismatch" );
	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

static inline byte DX_FogByte( float value )
{
	value = std::max( 0.0f, std::min( 1.0f, value ) );
	return static_cast<byte>( value * 255.0f + 0.5f );
}

static bool DX_FogRequested( void )
{
	if( dx_fog.value == 0.0f )
		return false;

	if( g_dxFog.source == DX_FOG_INTERNAL_WATER ||
		g_dxFog.source == DX_FOG_INTERNAL_QUAKE )
		return true;

	return g_dxFog.source == DX_FOG_CUSTOM && g_dxFog.customEnabled;
}

static D3DFOGMODE DX_FogMode( void )
{
	if( g_dxFog.source == DX_FOG_INTERNAL_QUAKE )
		return D3DFOG_EXP2;
	if( g_dxFog.source == DX_FOG_INTERNAL_WATER )
		return D3DFOG_EXP;

	return g_dxFog.density > 0.0f ? D3DFOG_EXP2 : D3DFOG_LINEAR;
}

static void DX_ApplyFog( void )
{
	if( !device )
		return;

	const bool enabled = DX_FogRequested( ) &&
		g_dxFog.allow && g_dxFog.suppressionDepth == 0 &&
		( g_dxFogTableSupported || g_dxFogVertexSupported );

	if( !enabled )
	{
		DX_SetRenderStateCached( D3DRS_FOGENABLE, FALSE );
		return;
	}

	const D3DFOGMODE mode = DX_FogMode( );
	const DWORD color = D3DCOLOR_ARGB( 255,
		DX_FogByte( g_dxFog.color[0] ),
		DX_FogByte( g_dxFog.color[1] ),
		DX_FogByte( g_dxFog.color[2] ) );

	DX_SetRenderStateCached( D3DRS_FOGENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_FOGCOLOR, color );
	DX_SetRenderStateCached( D3DRS_RANGEFOGENABLE, FALSE );

	if( g_dxFogTableSupported )
	{
		DX_SetRenderStateCached( D3DRS_FOGTABLEMODE, static_cast<DWORD>( mode ) );
		DX_SetRenderStateCached( D3DRS_FOGVERTEXMODE, static_cast<DWORD>( D3DFOG_NONE ) );
	}
	else
	{
		DX_SetRenderStateCached( D3DRS_FOGTABLEMODE, static_cast<DWORD>( D3DFOG_NONE ) );
		DX_SetRenderStateCached( D3DRS_FOGVERTEXMODE, static_cast<DWORD>( mode ) );
	}

	const float density = g_dxFog.density *
		( ( g_dxFog.source == DX_FOG_INTERNAL_WATER ||
			g_dxFog.source == DX_FOG_INTERNAL_QUAKE ) ?
			g_dxFog.densityScale : 1.0f );

	DX_SetRenderStateCached( D3DRS_FOGDENSITY, DX_FloatBits( density ) );
	DX_SetRenderStateCached( D3DRS_FOGSTART, DX_FloatBits( g_dxFog.start ) );
	DX_SetRenderStateCached( D3DRS_FOGEND, DX_FloatBits( g_dxFog.end ) );
}

void DX_AllowFog( qboolean allowed )
{
	const bool value = allowed != 0;
	if( g_dxFog.allow == value )
		return;
	g_dxFog.allow = value;
	DX_ApplyFog( );
}

static void DX_PushFogOff( void )
{
	++g_dxFog.suppressionDepth;
	if( g_dxFog.suppressionDepth == 1 )
		DX_ApplyFog( );
}

static void DX_PopFogOff( void )
{
	if( g_dxFog.suppressionDepth <= 0 )
	{
		g_dxFog.suppressionDepth = 0;
		return;
	}
	if( --g_dxFog.suppressionDepth == 0 )
		DX_ApplyFog( );
}

static void DX_SetFogDensityScale( float scale )
{
	scale = std::max( 0.0f, scale );
	if( fabsf( g_dxFog.densityScale - scale ) <= 0.000001f )
		return;
	g_dxFog.densityScale = scale;
	DX_ApplyFog( );
}

static void DX_RefreshFogCaps( void )
{
	g_dxFogTableSupported = false;
	g_dxFogVertexSupported = false;
	if( !device )
		return;

	D3DCAPS9 caps;
	if( SUCCEEDED( device->GetDeviceCaps( &caps ) ) )
	{
		g_dxFogTableSupported =
			( caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE ) != 0;
		g_dxFogVertexSupported =
			( caps.RasterCaps & D3DPRASTERCAPS_FOGVERTEX ) != 0;
	}
}

static dxtexture_t *DX_FogTexture( int textureIndex )
{
	if( textureIndex <= 0 || textureIndex >= 8096 )
		return nullptr;
	return &g_Textures[textureIndex];
}

static dxtexture_t *DX_FindWaterFogTexture_r(
	const mnode_t *node, const mnode_t *ignore, bool down )
{
	if( !node || !g_worldmodel )
		return nullptr;
	if( node->contents == CONTENTS_SOLID )
		return nullptr;

	if( node->contents < 0 )
	{
		if( !DX_IsLiquid( node->contents ) )
			return nullptr;

		const mleaf_t *leaf = reinterpret_cast<const mleaf_t *>( node );
		msurface_t **mark = leaf->firstmarksurface;
		for( int i = 0; i < leaf->nummarksurfaces; ++i, ++mark )
		{
			msurface_t *surface = mark ? *mark : nullptr;
			if( surface && ( surface->flags & SURF_DRAWTURB ) &&
				surface->texinfo && surface->texinfo->texture )
			{
				return DX_FogTexture(
					surface->texinfo->texture->gl_texturenum );
			}
		}
		return nullptr;
	}

	mnode_t *child = node_child( node, 0, g_worldmodel );
	if( child && child != ignore )
	{
		if( dxtexture_t *texture =
			DX_FindWaterFogTexture_r( child, node, true ) )
			return texture;
	}

	child = node_child( node, 1, g_worldmodel );
	if( child && child != ignore )
	{
		if( dxtexture_t *texture =
			DX_FindWaterFogTexture_r( child, node, true ) )
			return texture;
	}

	if( down )
		return nullptr;

	return node->parent ?
		DX_FindWaterFogTexture_r( node->parent, node, false ) :
		nullptr;
}

static dxtexture_t *DX_FindWaterFogTexture(
	const mleaf_t *viewLeaf, cl_entity_s *waterEntity, int contents )
{
	if( waterEntity && waterEntity->model &&
		waterEntity->model->type == mod_brush )
	{
		model_t *model = waterEntity->model;
		msurface_t *surface = &model->surfaces[model->firstmodelsurface];
		for( int i = 0; i < model->nummodelsurfaces; ++i, ++surface )
		{
			if( ( surface->flags & SURF_DRAWTURB ) &&
				surface->texinfo && surface->texinfo->texture )
			{
				g_dxFog.cachedContents = contents;
				return DX_FogTexture(
					surface->texinfo->texture->gl_texturenum );
			}
		}
		return nullptr;
	}

	if( !viewLeaf )
		return nullptr;

	dxtexture_t *texture = nullptr;
	if( viewLeaf->parent )
		texture = DX_FindWaterFogTexture_r(
			viewLeaf->parent, nullptr, false );
	else
		texture = DX_FindWaterFogTexture_r(
			reinterpret_cast<const mnode_t *>( viewLeaf ), nullptr, false );

	if( texture )
		g_dxFog.cachedContents = contents;
	return texture;
}

static void DX_SetWaterFog( const dxtexture_t *texture )
{
	if( !texture )
		return;

	g_dxFog.color[0] = texture->fogParams[0] * ( 1.0f / 255.0f );
	g_dxFog.color[1] = texture->fogParams[1] * ( 1.0f / 255.0f );
	g_dxFog.color[2] = texture->fogParams[2] * ( 1.0f / 255.0f );
	g_dxFog.color[3] = 1.0f;
	g_dxFog.density = texture->fogParams[3] * 0.000025f;
	g_dxFog.start = 0.0f;
	g_dxFog.end = 0.0f;
	g_dxFog.customEnabled = false;
	g_dxFog.skybox = true;
	g_dxFog.source = DX_FOG_INTERNAL_WATER;
	g_dxFog.densityScale = 1.0f;
}

static void DX_UpdateFog( const ref_viewpass_s *rvp )
{
	if( !rvp )
		return;

	g_dxFog.allow = true;
	g_dxFog.suppressionDepth = 0;
	g_dxFog.densityScale = 1.0f;

	if( gp_host && gp_movevars &&
		FBitSet( gp_host->features, ENGINE_QUAKE_COMPATIBLE ) )
	{
		const uint32_t settings =
			static_cast<uint32_t>( gp_movevars->fog_settings );
		if( settings != 0 )
		{
			g_dxFog.color[0] = ( ( settings >> 24 ) & 0xFF ) * ( 1.0f / 255.0f );
			g_dxFog.color[1] = ( ( settings >> 16 ) & 0xFF ) * ( 1.0f / 255.0f );
			g_dxFog.color[2] = ( ( settings >> 8 ) & 0xFF ) * ( 1.0f / 255.0f );
			g_dxFog.color[3] = 1.0f;
			g_dxFog.density = ( settings & 0xFF ) * ( 1.0f / 255.0f ) * 0.01f;
			g_dxFog.start = 0.0f;
			g_dxFog.end = 0.0f;
			g_dxFog.customEnabled = false;
			g_dxFog.skybox = true;
			g_dxFog.source = DX_FOG_INTERNAL_QUAKE;
		}
		else if( g_dxFog.source == DX_FOG_INTERNAL_QUAKE )
		{
			g_dxFog.source = DX_FOG_NONE;
		}

		DX_ApplyFog( );
		return;
	}

	const int waterLevel = static_cast<int>(
		ENGINE_GET_PARM( PARM_WATER_LEVEL ) );

	mleaf_t *viewLeaf = nullptr;
	if( g_worldmodel && g_worldmodel->nodes &&
		FBitSet( rvp->flags, RF_DRAW_WORLD ) && gEngfuncs.Mod_PointInLeaf )
	{
		viewLeaf = gEngfuncs.Mod_PointInLeaf(
			rvp->vieworigin, g_worldmodel->nodes, g_worldmodel );
	}

	if( FBitSet( rvp->flags, RF_ONLY_CLIENTDRAW ) ||
		waterLevel < 3 ||
		!FBitSet( rvp->flags, RF_DRAW_WORLD ) ||
		!viewLeaf )
	{
		if( g_dxFog.source == DX_FOG_INTERNAL_WATER )
			g_dxFog.source = DX_FOG_NONE;
		if( g_dxFog.cachedWaterLevel == 3 )
			g_dxFog.cachedContents = CONTENTS_EMPTY;
		g_dxFog.cachedWaterLevel = waterLevel;
		DX_ApplyFog( );
		return;
	}

	cl_entity_s *waterEntity = gEngfuncs.CL_GetWaterEntity ?
		gEngfuncs.CL_GetWaterEntity( rvp->vieworigin ) : nullptr;
	const int contents =
		( waterEntity && waterEntity->model &&
		  waterEntity->model->type == mod_brush &&
		  waterEntity->curstate.skin < 0 ) ?
		waterEntity->curstate.skin : viewLeaf->contents;

	g_dxFog.cachedWaterLevel = waterLevel;

	if( !DX_IsLiquid( g_dxFog.cachedContents ) &&
		DX_IsLiquid( contents ) )
	{
		dxtexture_t *texture =
			DX_FindWaterFogTexture( viewLeaf, waterEntity, contents );
		if( texture )
			DX_SetWaterFog( texture );
		else if( g_dxFog.source == DX_FOG_INTERNAL_WATER )
			g_dxFog.source = DX_FOG_NONE;
	}
	else if( DX_IsLiquid( contents ) )
	{

		if( g_dxFog.source != DX_FOG_INTERNAL_WATER &&
			DX_IsLiquid( g_dxFog.cachedContents ) )
			g_dxFog.source = DX_FOG_INTERNAL_WATER;
	}
	else if( g_dxFog.source == DX_FOG_INTERNAL_WATER )
	{
		g_dxFog.source = DX_FOG_NONE;
		g_dxFog.cachedContents = CONTENTS_EMPTY;
	}

	DX_ApplyFog( );
}

static void DX_ResetFog( void )
{
	g_dxFog = dx_fog_state_t{};
	if( device )
		DX_SetRenderStateCached( D3DRS_FOGENABLE, FALSE );
}

static float R_ShowTexturesClampZoom( float zoom )
{
	return std::max( 0.5f, std::min( 4.0f, zoom ) );
}

static void R_ShowTexturesSetZoom( float zoom )
{
	zoom = R_ShowTexturesClampZoom( zoom );
	if( gEngfuncs.Cvar_SetValue )
		gEngfuncs.Cvar_SetValue( "dx_showtextures_zoom", zoom );
	else
		dx_showtextures_zoom.value = zoom;
}

static void R_TextureZoomInCmd( void ) { R_ShowTexturesSetZoom( dx_showtextures_zoom.value + 0.25f ); }

static void R_TextureZoomOutCmd( void ) { R_ShowTexturesSetZoom( dx_showtextures_zoom.value - 0.25f ); }

static void R_TextureZoomResetCmd( void ) { R_ShowTexturesSetZoom( 1.0f ); }

static void R_RegisterTextureCommands( void )
{
	if( g_showTextureCmdsRegistered )
		return;

	gEngfuncs.Cmd_AddCommand( "dx_showtextures_zoom_in",
		R_TextureZoomInCmd,
		"Increase r_showtextures preview size" );
	gEngfuncs.Cmd_AddCommand( "dx_showtextures_zoom_out",
		R_TextureZoomOutCmd,
		"Decrease r_showtextures preview size" );
	gEngfuncs.Cmd_AddCommand( "dx_showtextures_zoom_reset",
		R_TextureZoomResetCmd,
		"Reset r_showtextures preview size" );
	g_showTextureCmdsRegistered = true;
}

static void R_RemoveTextureCommands( void )
{
	if( !g_showTextureCmdsRegistered )
		return;

	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_in" );
	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_out" );
	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_reset" );
	g_showTextureCmdsRegistered = false;
}

static void R_RegisterContextCvars( void )
{
	if( g_contextCvarsRegistered )
		return;

	gEngfuncs.Cvar_RegisterVariable( &dx_remix );
	gEngfuncs.Cvar_RegisterVariable( &dx_gamma );
	gEngfuncs.Cvar_RegisterVariable( &dx_brightness );
	gEngfuncs.Cvar_RegisterVariable( &dx_fog );
	gEngfuncs.Cvar_RegisterVariable( &dx_msaa_samples );
	gEngfuncs.Cvar_RegisterVariable( &dx_msaa_quality );
	gEngfuncs.Cvar_RegisterVariable( &dx_anisotropy );
	gEngfuncs.Cvar_RegisterVariable( &dx_showtextures_zoom );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_fov );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_near );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_x );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_y );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_z );
	gEngfuncs.Cvar_RegisterVariable( &dx_viewmodel_beam_offset );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_stale_timeout );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_dead_body_timeout );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_use_engine_state );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_interpolation );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_interp_delay );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_interp_extrapolate );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_interp_teleport_dist );
	gEngfuncs.Cvar_RegisterVariable( &dx_client_efx );
	gEngfuncs.Cvar_RegisterVariable( &dx_particles );
	gEngfuncs.Cvar_RegisterVariable( &dx_particle_size_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_particle_limit );
	gEngfuncs.Cvar_RegisterVariable( &dx_particle_additive );
	gEngfuncs.Cvar_RegisterVariable( &dx_tracers );
	gEngfuncs.Cvar_RegisterVariable( &dx_tracer_width );
	gEngfuncs.Cvar_RegisterVariable( &dx_tracer_length_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_particles );
	gEngfuncs.Cvar_RegisterVariable( &dx_beams );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_trails );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_trail_spacing );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_width_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine_boost );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine_close_fade );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine_segments );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine_anchor );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_sine_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_close_precision );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_close_precision_distance );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_close_precision_step );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_endpoint_taper );
	gEngfuncs.Cvar_RegisterVariable( &dx_beam_endpoint_taper_distance );
	gEngfuncs.Cvar_RegisterVariable( &dx_egon_beam_light );
	gEngfuncs.Cvar_RegisterVariable( &dx_egon_beam_light_spacing );
	gEngfuncs.Cvar_RegisterVariable( &dx_egon_beam_light_radius );
	gEngfuncs.Cvar_RegisterVariable( &dx_egon_beam_light_max_samples );
	g_contextCvarsRegistered = true;
}

static void R_ApplyGammaRamp( void )
{
	if( !device )
		return;

	const float gamma = std::max( 0.25f, std::min( 4.0f, dx_gamma.value ) );
	const float brightness = std::max( 0.0f, std::min( 2.0f, dx_brightness.value ) );
	if( fabsf( gamma - g_lastGamma ) < 0.0001f &&
		fabsf( brightness - g_lastBrightness ) < 0.0001f )
	{
		return;
	}

	D3DGAMMARAMP ramp;
	for( int i = 0; i < 256; ++i )
	{
		const float source = static_cast<float>( i ) / 255.0f;
		const float corrected = std::min( 1.0f,
			powf( source, 1.0f / gamma ) * brightness );
		const WORD value = static_cast<WORD>( corrected * 65535.0f + 0.5f );
		ramp.red[i] = value;
		ramp.green[i] = value;
		ramp.blue[i] = value;
	}

	device->SetGammaRamp( 0, D3DSGR_NO_CALIBRATION, &ramp );
	g_lastGamma = gamma;
	g_lastBrightness = brightness;
}

typedef int texnum_t;

static void R_SimpleStub( void )
{
	;
}

void R_SetRenderMode( int mode );
static struct r_studio_interface_s *g_clientStudioDraw = nullptr;
static struct r_studio_interface_s *g_defaultStudioDraw = nullptr;
static cl_entity_s *g_studioApiCurrentEntity = nullptr;
static model_t *g_studioApiRenderModel = nullptr;
static void *g_studioApiHeader = nullptr;
static int g_studioApiForceFaceFlags = 0;
static int g_studioApiFrameCount = 0;
static float g_studioApiIdentity3x4[3][4] =
{
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f }
};
static float *g_studioApiIdentity3x4Rows[3] =
{
	g_studioApiIdentity3x4[0],
	g_studioApiIdentity3x4[1],
	g_studioApiIdentity3x4[2]
};
static float **g_studioApiIdentity3x4Ptr = g_studioApiIdentity3x4Rows;
static float ***g_studioApiIdentity3x4Ptr2 = &g_studioApiIdentity3x4Ptr;
static float ****g_studioApiIdentity3x4Ptr3 = &g_studioApiIdentity3x4Ptr2;

static void *R_StudioAPIMemCalloc( int number, size_t size )
{
	if( number <= 0 || size == 0 )
		return nullptr;
	if( gEngfuncs.Mod_Calloc )
		return gEngfuncs.Mod_Calloc( number, size );
	return calloc( static_cast<size_t>( number ), size );
}

static void *R_StudioAPICacheCheck( struct cache_user_s *c )
{
	return gEngfuncs.Mod_CacheCheck ? gEngfuncs.Mod_CacheCheck( c ) : nullptr;
}

static void R_StudioLoadCacheFile( const char *path, struct cache_user_s *cu )
{
	if( gEngfuncs.Mod_LoadCacheFile && path && cu )
		gEngfuncs.Mod_LoadCacheFile( path, cu );
}

static model_t *R_StudioAPIModForName( const char *name, int crash_if_missing )
{
	return gEngfuncs.Mod_ForName ?
		gEngfuncs.Mod_ForName( name, crash_if_missing ? true : false, false ) : nullptr;
}

static void *R_StudioAPIModExtradata( model_t *mod )
{
	return ( gEngfuncs.Mod_Extradata && mod ) ?
		gEngfuncs.Mod_Extradata( mod_studio, mod ) : nullptr;
}

static model_t *R_StudioAPIGetModelByIndex( int index )
{
	return ( gp_cl && index > 0 && index <= MAX_MODELS ) ? gp_cl->models[index] : nullptr;
}

static cl_entity_s *R_StudioAPIGetCurrentEntity( void )
{
	return g_studioApiCurrentEntity;
}

static player_info_t *R_StudioAPIPlayerInfo( int index )
{
	return gEngfuncs.pfnPlayerInfo ? gEngfuncs.pfnPlayerInfo( index ) : nullptr;
}

static entity_state_t *R_StudioAPIGetPlayerState( int index )
{
	return gEngfuncs.pfnGetPlayerState ? gEngfuncs.pfnGetPlayerState( index ) : nullptr;
}

static cl_entity_s *R_StudioAPIGetViewEntity( void )
{
	return reinterpret_cast<cl_entity_s *>( ENGINE_GET_PARM( PARM_GET_VIEWENT_PTR ) );
}

static void R_StudioAPIGetTimes( int *framecount, double *current, double *old )
{
	if( framecount ) *framecount = g_studioApiFrameCount;
	if( current ) *current = gp_cl ? gp_cl->time : ( gp_host ? gp_host->realtime : 0.0 );
	if( old ) *old = gp_cl ? gp_cl->oldtime : ( gp_host ? gp_host->realtime - gp_host->frametime : 0.0 );
}

static cvar_t *R_StudioAPIGetCvar( const char *name )
{
	return gEngfuncs.pfnGetCvarPointer ? gEngfuncs.pfnGetCvarPointer( name ) : nullptr;
}

static void R_StudioAPIGetViewInfo( float *origin, float *upv, float *rightv, float *vpnv )
{
	vec3_t forward, right, up;

	if( gpGlobals )
	{
		if( origin ) VectorCopy( gpGlobals->vieworg, origin );
		AngleVectors( gpGlobals->viewangles, forward, right, up );
	}
	else
	{
		if( origin ) VectorClear( origin );
		forward[0] = 1.0f; forward[1] = 0.0f; forward[2] = 0.0f;
		right[0] = 0.0f; right[1] = -1.0f; right[2] = 0.0f;
		up[0] = 0.0f; up[1] = 0.0f; up[2] = 1.0f;
	}

	if( upv ) VectorCopy( up, upv );
	if( rightv ) VectorCopy( right, rightv );
	if( vpnv ) VectorCopy( forward, vpnv );
}

static model_t *R_StudioAPIGetChromeSprite( void )
{
	return gEngfuncs.GetDefaultSprite ? gEngfuncs.GetDefaultSprite( REF_CHROME_SPRITE ) : nullptr;
}

static void R_StudioGetModelCounters( int **s, int **a )
{
	static int dummyStudioCounter = 0;
	static int dummyAliasCounter = 0;
	if( s ) *s = &dummyStudioCounter;
	if( a ) *a = &dummyAliasCounter;
}

static void R_StudioGetAliasScale( float *x, float *y )
{
	if( x ) *x = 1.0f;
	if( y ) *y = 1.0f;
}

static float ****R_StudioAPIGetBoneTransform( void ) { return g_studioApiIdentity3x4Ptr3; }
static float ****R_StudioAPIGetLightTransform( void ) { return g_studioApiIdentity3x4Ptr3; }
static float ***R_StudioAPIGetAliasTransform( void ) { return g_studioApiIdentity3x4Ptr2; }
static float ***R_StudioAPIGetRotationMatrix( void ) { return g_studioApiIdentity3x4Ptr2; }

static void R_StudioAPISetupModel( int bodypart, void **ppbodypart, void **ppsubmodel )
{
	(void)bodypart;
	if( ppbodypart ) *ppbodypart = nullptr;
	if( ppsubmodel ) *ppsubmodel = nullptr;
}

static int R_StudioAPICheckBBox( void ) { return 1; }
static void R_StudioDynamicLight( cl_entity_s *ent, struct alight_s *plight ) { (void)ent; (void)plight; }
static void R_StudioAPIEntityLight( struct alight_s *plight ) { (void)plight; }
static void R_StudioSetupLighting( struct alight_s *plighting ) { (void)plighting; }
static void R_StudioAPIDrawPoints( void ) { }
static void R_StudioAPIDrawHulls( void ) { }
static void R_StudioAPIDrawAbsBBox( void ) { }
static void R_StudioAPIDrawBones( void ) { }
static void R_StudioAPISetupSkin( void *ptexturehdr, int index ) { (void)ptexturehdr; (void)index; }
static void R_StudioAPISetRemapColors( int top, int bottom ) { (void)top; (void)bottom; }

static bool R_CopyPlayerModelName( const char *source, char *dest, size_t destSize )
{
	if( !dest || destSize == 0 )
		return false;
	dest[0] = '\0';
	if( !source )
		return false;

	size_t length = 0;
	for( ; length + 1 < destSize; ++length )
	{
		const unsigned char c = static_cast<unsigned char>( source[length] );
		if( c == 0 )
			break;
		if( c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' )
			return false;
		dest[length] = static_cast<char>( c );
	}

	if( length == 0 || source[length] != '\0' )
	{
		dest[0] = '\0';
		return false;
	}
	dest[length] = '\0';
	return true;
}

static model_t *R_StudioAPISetupPlayerModel( int index )
{
	player_info_t *info = R_StudioAPIPlayerInfo( index );
	if( !info || !gEngfuncs.Mod_ForName )
		return nullptr;

	char modelName[64]{};
	if( !R_CopyPlayerModelName( info->model, modelName, sizeof( modelName ) ) )
		return nullptr;

	char path[256]{};
	const int written = snprintf( path, sizeof( path ),
		"models/player/%s/%s.mdl", modelName, modelName );
	if( written < 0 || static_cast<size_t>( written ) >= sizeof( path ) )
		return nullptr;
	return gEngfuncs.Mod_ForName( path, false, false );
}

static void R_StudioClientEvents( void ) { }
static int R_StudioGetFaceFlags( void ) { return g_studioApiForceFaceFlags; }
static void R_StudioSetFaceFlags( int flags ) { g_studioApiForceFaceFlags = flags; }
static void R_StudioAPISetHeader( void *header ) { g_studioApiHeader = header; }
static void R_StudioAPISetRenderModel( model_t *model ) { g_studioApiRenderModel = model; }
static void R_StudioRestoreRenderer( void ) { R_SetRenderMode( kRenderNormal ); }
static void R_StudioSetChromeOrigin( void ) { }
static int R_StudioAPIIsHardware( void ) { return REF_D3D; }
static void R_StudioAPIDrawShadow( void ) { }
static void R_StudioSetRenderAmt( int ) { }
static void R_StudioSetCull( int ) { }
static void R_StudioRenderShadow( int iSprite, float *p1, float *p2, float *p3, float *p4 )
{
	(void)iSprite; (void)p1; (void)p2; (void)p3; (void)p4;
}

static qboolean R_StudioFillAPI( struct engine_studio_api_s *api, struct r_studio_interface_s *pDefaultDraw )
{
	if( !api )
		return false;

	memset( api, 0, sizeof( *api ) );
	g_defaultStudioDraw = pDefaultDraw;

	api->Mem_Calloc = R_StudioAPIMemCalloc;
	api->Cache_Check = R_StudioAPICacheCheck;
	api->LoadCacheFile = R_StudioLoadCacheFile;
	api->Mod_ForName = R_StudioAPIModForName;
	api->Mod_Extradata = R_StudioAPIModExtradata;
	api->GetModelByIndex = R_StudioAPIGetModelByIndex;
	api->GetCurrentEntity = R_StudioAPIGetCurrentEntity;
	api->PlayerInfo = R_StudioAPIPlayerInfo;
	api->GetPlayerState = R_StudioAPIGetPlayerState;
	api->GetViewEntity = R_StudioAPIGetViewEntity;
	api->GetTimes = R_StudioAPIGetTimes;
	api->GetCvar = R_StudioAPIGetCvar;
	api->GetViewInfo = R_StudioAPIGetViewInfo;
	api->GetChromeSprite = R_StudioAPIGetChromeSprite;
	api->GetModelCounters = R_StudioGetModelCounters;
	api->GetAliasScale = R_StudioGetAliasScale;
	api->StudioGetBoneTransform = R_StudioAPIGetBoneTransform;
	api->StudioGetLightTransform = R_StudioAPIGetLightTransform;
	api->StudioGetAliasTransform = R_StudioAPIGetAliasTransform;
	api->StudioGetRotationMatrix = R_StudioAPIGetRotationMatrix;
	api->StudioSetupModel = R_StudioAPISetupModel;
	api->StudioCheckBBox = R_StudioAPICheckBBox;
	api->StudioDynamicLight = R_StudioDynamicLight;
	api->StudioEntityLight = R_StudioAPIEntityLight;
	api->StudioSetupLighting = R_StudioSetupLighting;
	api->StudioDrawPoints = R_StudioAPIDrawPoints;
	api->StudioDrawHulls = R_StudioAPIDrawHulls;
	api->StudioDrawAbsBBox = R_StudioAPIDrawAbsBBox;
	api->StudioDrawBones = R_StudioAPIDrawBones;
	api->StudioSetupSkin = R_StudioAPISetupSkin;
	api->StudioSetRemapColors = R_StudioAPISetRemapColors;
	api->SetupPlayerModel = R_StudioAPISetupPlayerModel;
	api->StudioClientEvents = R_StudioClientEvents;
	api->GetForceFaceFlags = R_StudioGetFaceFlags;
	api->SetForceFaceFlags = R_StudioSetFaceFlags;
	api->StudioSetHeader = R_StudioAPISetHeader;
	api->SetRenderModel = R_StudioAPISetRenderModel;
	api->SetupRenderer = R_SetRenderMode;
	api->RestoreRenderer = R_StudioRestoreRenderer;
	api->SetChromeOrigin = R_StudioSetChromeOrigin;
	api->IsHardware = R_StudioAPIIsHardware;
	api->GL_StudioDrawShadow = R_StudioAPIDrawShadow;
	api->GL_SetRenderMode = R_SetRenderMode;
	api->StudioSetRenderamt = R_StudioSetRenderAmt;
	api->StudioSetCullState = R_StudioSetCull;
	api->StudioRenderShadow = R_StudioRenderShadow;
	return true;
}

static void R_StudioSetInterface( struct r_studio_interface_s *pDraw )
{
	g_clientStudioDraw = pDraw;
}

static void R_GetDetailScale( int texture, float *xScale, float *yScale )
{
	if( xScale ) *xScale = 1.0f;
	if( yScale ) *yScale = 1.0f;
}

static void R_SetDetailScale( int texture, float xScale, float yScale )
{
	;
}

static intptr_t RefGetParm( int parm, int arg );
static void R_OverrideTextureSize( unsigned int texnum, unsigned int srcWidth, unsigned int srcHeight );
static int WorldToScreen( const vec3_t world, vec3_t screen );
static colorVec R_LightPoint( const float *p );
static qboolean VID_CubemapShot( const char *base, uint size,
	const float *vieworg, qboolean skyshot );
static void DX_RenderFrame( const struct ref_viewpass_s *rvp );
static void Color4f( float r, float g, float b, float a );
static void Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
static void R_ShowTextures( );
void R_SetRenderMode( int mode );

static void R_APISetCurrentEntity( cl_entity_s *ent ) { (void)ent; }
static void R_APISetCurrentModel( model_t *mod ) { (void)mod; }
static void R_APIGetTextureParms( int texture, byte *red, byte *green, byte *blue, byte *alpha )
{
	(void)texture;
	if( red ) *red = 255;
	if( green ) *green = 255;
	if( blue ) *blue = 255;
	if( alpha ) *alpha = 255;
}
static lightstyle_t *R_APIGetLightStyle( int number )
{
	lightstyle_t *styles = reinterpret_cast<lightstyle_t *>( ENGINE_GET_PARM( PARM_GET_LIGHTSTYLES_PTR ) );
	return ( styles && number >= 0 && number < 256 ) ? &styles[number] : nullptr;
}
static dlight_t *R_APIGetDynamicLight( int number )
{
	dlight_t *lights = reinterpret_cast<dlight_t *>( ENGINE_GET_PARM( PARM_GET_DLIGHTS_PTR ) );
	return ( lights && number >= 0 && number < 64 ) ? &lights[number] : nullptr;
}
static dlight_t *R_APIGetEntityLight( int number )
{
	dlight_t *lights = reinterpret_cast<dlight_t *>( ENGINE_GET_PARM( PARM_GET_ELIGHTS_PTR ) );
	return ( lights && number >= 0 && number < 64 ) ? &lights[number] : nullptr;
}
static byte R_APILightToTexGamma( byte color ) { return color; }
static float R_APIGetFrameTime( void ) { return std::max( 0.0f, g_frametime ); }
static int R_APIFatPVS( const float *org, float radius, byte *visbuffer, qboolean merge, qboolean fullvis )
{
	return gEngfuncs.R_FatPVS ? gEngfuncs.R_FatPVS( org, radius, visbuffer, merge, fullvis ) : 0;
}
static void R_APIStoreEfrags( struct efrag_s **ppefrag, int framecount )
{
	if( gEngfuncs.R_StoreEfrags )
		gEngfuncs.R_StoreEfrags( ppefrag, framecount );
}
static int R_APILoadTextureArray( const char **names, int flags ) { (void)names; (void)flags; return 0; }
static int R_APICreateTexArray( const char *name, int width, int height, int depth, const void *buffer, texFlags_t flags )
{
	(void)name; (void)width; (void)height; (void)depth; (void)buffer; (void)flags; return 0;
}
static void R_APIDrawSingleDecal( struct decal_s *pDecal, msurface_t *fa ) { (void)pDecal; (void)fa; }
static float *R_APIDecalSetupVerts( struct decal_s *pDecal, msurface_t *surf, int texture, int *outCount )
{
	(void)pDecal; (void)surf; (void)texture;
	if( outCount ) *outCount = 0;
	return nullptr;
}
static void R_APIRemoveEntityDecals( model_t *mod ) { (void)mod; }
static struct movie_state_s *R_APIAVILoadVideo( const char *filename, qboolean loadAudio ) { (void)filename; (void)loadAudio; return nullptr; }
static qboolean R_APIAVIGetVideoInfo( struct movie_state_s *avi, int *xres, int *yres, float *duration )
{
	(void)avi;
	if( xres ) *xres = 0;
	if( yres ) *yres = 0;
	if( duration ) *duration = 0.0f;
	return false;
}
static int R_APIVideoFrame( struct movie_state_s *avi, float time ) { (void)avi; (void)time; return 0; }
static byte *R_APIAVIGetVideoFrame( struct movie_state_s *avi, int frame ) { (void)avi; (void)frame; return nullptr; }
static void R_APIUploadVideoFrame( int texture, int cols, int rows, int width, int height, const byte *data )
{
	DX_UpdateTexture( texture, cols, rows, width, height, data, PF_RGBA_32 );
}
static void R_APIUpdateTexture( int texture, int cols, int rows, int width, int height,
	const byte *data, pixformat_t format )
{
	(void)DX_UpdateTexture( texture, cols, rows, width, height, data, format );
}
static void R_APIAVIFreeVideo( struct movie_state_s *avi ) { (void)avi; }
static qboolean R_APIAVIIsActive( struct movie_state_s *avi ) { (void)avi; return false; }
static void R_APIAVIStreamSound( struct movie_state_s *avi, int entnum, float fvol, float attn, float synctime )
{
	(void)avi; (void)entnum; (void)fvol; (void)attn; (void)synctime;
}
static qboolean R_APIAVIThink( struct movie_state_s *avi ) { (void)avi; return false; }
static qboolean R_APIAVISetParm( struct movie_state_s *avi, movie_parms_e parm, ... ) { (void)avi; (void)parm; return false; }
static void R_APISelectTexture( int tmu ) { (void)tmu; }
static void R_APILoadTexMatrix( const float *matrix ) { (void)matrix; }
static void R_APITexIdentity( void ) { }
static void R_APICleanTextureUnits( int last ) { (void)last; }
static void R_APITexGen( unsigned int coord, unsigned int mode ) { (void)coord; (void)mode; }
static void R_APITextureTarget( unsigned int target ) { (void)target; }
static void R_APITexCoordArray( unsigned int texmode ) { (void)texmode; }
static void R_APIUpdateTexSize( int texnum, int width, int height, int depth ) { (void)texnum; (void)width; (void)height; (void)depth; }
static void R_APIGLDrawParticles( const struct ref_viewpass_s *rvp, qboolean transPass, float frametime ) { (void)rvp; (void)transPass; (void)frametime; }
static void R_APIEnvShot( const float *vieworg, const char *name,
	qboolean skyshot, int shotsize )
{
	if( !vieworg || !name || !name[0] )
		return;

	const uint size = static_cast<uint>(
		std::max( 16, std::min( 4096, shotsize > 0 ? shotsize : 256 ) ) );
	VID_CubemapShot( name, size, vieworg, skyshot );
}
static int R_APISPRLoadExt( const char *name, unsigned int texFlags )
{
	(void)name; (void)texFlags;
	return 0;
}
static colorVec R_APILightVec( const float *start, const float *end,
	float *lightspot, float *lightvec )
{
	colorVec result = { 0, 0, 0, 0 };
	if( !start )
		return result;

	vec3_t fallbackEnd;
	const float *traceEnd = end;
	if( !traceEnd )
	{
		VectorCopy( start, fallbackEnd );
		fallbackEnd[2] -= 8192.0f;
		traceEnd = fallbackEnd;
	}

	const D3DCOLOR packed = R_LightColorAlongSegment(
		start, traceEnd, lightspot, lightvec );
	result.r = ( packed >> 16 ) & 0xFF;
	result.g = ( packed >> 8 ) & 0xFF;
	result.b = packed & 0xFF;
	result.a = 0;
	return result;
}
static struct mstudiotex_s *R_APIStudioGetTexture( cl_entity_s *e ) { (void)e; return nullptr; }
static const struct ref_overview_s *R_APIGetOverviewParms( void ) { return gEngfuncs.GetOverviewParms ? gEngfuncs.GetOverviewParms( ) : nullptr; }
static const char *R_APIGetFileByIndex( int fileindex ) { (void)fileindex; return ""; }
static int R_APISaveFile( const char *filename, const void *data, int len ) { (void)filename; (void)data; (void)len; return 0; }
static void *R_APIMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return gEngfuncs._Mem_Alloc ? gEngfuncs._Mem_Alloc( 0, cb, true, filename, fileline ) : calloc( 1, cb );
}
static void R_APIMemFree( void *mem, const char *filename, const int fileline )
{
	if( gEngfuncs._Mem_Free ) gEngfuncs._Mem_Free( mem, filename, fileline );
	else free( mem );
}
static char **R_APIGetFilesList( const char *pattern, int *numFiles, int gamedironly )
{
	(void)pattern; (void)gamedironly;
	if( numFiles ) *numFiles = 0;
	return nullptr;
}
static unsigned int R_APIFileBufferCRC32( const void *buffer, const int length ) { (void)buffer; (void)length; return 0; }
static int R_APICompareFileTime( const char *filename1, const char *filename2, int *compare )
{
	(void)filename1; (void)filename2;
	if( compare ) *compare = 0;
	return 0;
}
static void *R_APIGetModel( int modelindex )
{
	return ( gp_cl && modelindex > 0 && modelindex <= MAX_MODELS ) ? gp_cl->models[modelindex] : nullptr;
}
static float R_APITime( void ) { return static_cast<float>( gEngfuncs.pfnTime ? gEngfuncs.pfnTime( ) : ( gp_host ? gp_host->realtime : 0.0 ) ); }
static void R_APICvarSet( const char *name, const char *value )
{
	if( gEngfuncs.Cvar_Set ) gEngfuncs.Cvar_Set( name, value );
}
static void R_APIFadeMusicVolume( float fadePercent ) { (void)fadePercent; }
static void R_APISetRandomSeed( int seed )
{
	if( gEngfuncs.COM_SetRandomSeed ) gEngfuncs.COM_SetRandomSeed( seed );
}

#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif
#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif
#ifndef GL_TEXTURE_MATRIX
#define GL_TEXTURE_MATRIX 0x0BA8
#endif

struct triapi_state_t
{
	bool active = false;
	bool screenSpace = false;
	int primitiveMode = TRI_TRIANGLES;
	int renderMode = kRenderNormal;
	float rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float texcoord[2] = { 0.0f, 0.0f };
	D3DCOLOR vertexColor = D3DCOLOR_ARGB( 255, 255, 255, 255 );

	std::vector<brush_vertex_t> vertices;
	std::vector<vertex2d_t> screenVertices;
	std::vector<vertex2d_t> expandedScreenVertices;
	vertex2d_t screenQuad[4]{};
	int screenQuadCount = 0;
	std::vector<DWORD> indices;
};

static triapi_state_t g_triApi;

static byte R_TriFloatToByte( float value )
{
	value = std::max( 0.0f, std::min( 1.0f, value ) );
	return static_cast<byte>( value * 255.0f + 0.5f );
}

static D3DCOLOR R_TriOpaqueColor( float red, float green, float blue )
{
	return D3DCOLOR_ARGB( 255,
		R_TriFloatToByte( red ),
		R_TriFloatToByte( green ),
		R_TriFloatToByte( blue ) );
}

static void R_TriRenderMode( int mode )
{

	g_triApi.renderMode = mode;

	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );

	switch( mode )
	{
	default:
	case kRenderNormal:
		DX_AllowFog( true );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		break;

	case kRenderTransAlpha:
		DX_AllowFog( true );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;

	case kRenderTransColor:
	case kRenderTransTexture:
		DX_AllowFog( true );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		break;

	case kRenderGlow:
	case kRenderTransAdd:
		DX_AllowFog( false );
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	}
}

static size_t R_TriVertexCount( )
{
	return g_triApi.screenSpace
		? g_triApi.screenVertices.size( )
		: g_triApi.vertices.size( );
}

static void R_TriBegin( int mode )
{
	if( !device )
		return;

	R_Flush2DBatch( );

	switch( mode )
	{
	case TRI_POINTS:
	case TRI_TRIANGLES:
	case TRI_TRIANGLE_FAN:
	case TRI_QUADS:
	case TRI_LINES:
	case TRI_TRIANGLE_STRIP:
	case TRI_QUAD_STRIP:
	case TRI_POLYGON:
		break;
	default:
		mode = TRI_POLYGON;
		break;
	}

	g_triApi.active = true;
	g_triApi.screenSpace = g_2dModeActive;
	g_triApi.primitiveMode = mode;
	g_triApi.indices.clear( );
	g_triApi.screenQuadCount = 0;

	if( g_triApi.screenSpace )
	{
		g_triApi.screenVertices.clear( );
		if( g_triApi.screenVertices.capacity( ) < 256 )
			g_triApi.screenVertices.reserve( 256 );
	}
	else
	{
		g_triApi.vertices.clear( );
		if( g_triApi.vertices.capacity( ) < 256 )
			g_triApi.vertices.reserve( 256 );
	}
}

static void R_TriEmitQuadIndices( )
{
	const size_t quadCount = R_TriVertexCount( ) / 4;
	g_triApi.indices.clear( );
	g_triApi.indices.reserve( quadCount * 6 );
	for( size_t quad = 0; quad < quadCount; ++quad )
	{
		const DWORD base = static_cast<DWORD>( quad * 4 );
		g_triApi.indices.push_back( base + 0 );
		g_triApi.indices.push_back( base + 1 );
		g_triApi.indices.push_back( base + 2 );
		g_triApi.indices.push_back( base + 0 );
		g_triApi.indices.push_back( base + 2 );
		g_triApi.indices.push_back( base + 3 );
	}
}

static void R_TriEmitQuadStripIndices( )
{
	const size_t count = R_TriVertexCount( );
	g_triApi.indices.clear( );
	if( count < 4 )
		return;

	g_triApi.indices.reserve( ( ( count - 2 ) / 2 ) * 6 );
	for( DWORD i = 0; i + 3 < count; i += 2 )
	{
		g_triApi.indices.push_back( i + 0 );
		g_triApi.indices.push_back( i + 1 );
		g_triApi.indices.push_back( i + 3 );
		g_triApi.indices.push_back( i + 0 );
		g_triApi.indices.push_back( i + 3 );
		g_triApi.indices.push_back( i + 2 );
	}
}

static void R_TriExpandQuadStrip( )
{
	const size_t count = g_triApi.screenVertices.size( );
	g_triApi.expandedScreenVertices.clear( );
	if( count < 4 )
		return;

	const size_t quadCount = ( count - 2 ) / 2;
	g_triApi.expandedScreenVertices.reserve( quadCount * 6 );
	for( size_t i = 0; i + 3 < count; i += 2 )
	{
		const auto &v0 = g_triApi.screenVertices[i + 0];
		const auto &v1 = g_triApi.screenVertices[i + 1];
		const auto &v2 = g_triApi.screenVertices[i + 2];
		const auto &v3 = g_triApi.screenVertices[i + 3];
		g_triApi.expandedScreenVertices.push_back( v0 );
		g_triApi.expandedScreenVertices.push_back( v1 );
		g_triApi.expandedScreenVertices.push_back( v3 );
		g_triApi.expandedScreenVertices.push_back( v0 );
		g_triApi.expandedScreenVertices.push_back( v3 );
		g_triApi.expandedScreenVertices.push_back( v2 );
	}
}

static void R_TriEnd( void )
{
	if( !g_triApi.active || !device )
	{
		g_triApi.active = false;
		return;
	}
	g_triApi.active = false;

	const UINT vertexCount = static_cast<UINT>( R_TriVertexCount( ) );
	if( vertexCount == 0 )
		return;

	if( g_triApi.screenSpace )
	{

		device->SetFVF( D3DFVF_VERTEX2D );
		const vertex2d_t *vertices = g_triApi.screenVertices.data( );

		switch( g_triApi.primitiveMode )
		{
		case TRI_POINTS:
			R_DrawScratch( D3DPT_POINTLIST, vertexCount,
				vertexCount, vertices, sizeof( vertex2d_t ) );
			break;

		case TRI_LINES:
			if( vertexCount >= 2 )
				R_DrawScratch( D3DPT_LINELIST, vertexCount / 2,
					( vertexCount / 2 ) * 2, vertices, sizeof( vertex2d_t ) );
			break;

		case TRI_TRIANGLE_STRIP:
			if( vertexCount >= 3 )
				R_DrawScratch( D3DPT_TRIANGLESTRIP,
					vertexCount - 2, vertexCount, vertices,
					sizeof( vertex2d_t ) );
			break;

		case TRI_TRIANGLE_FAN:
		case TRI_POLYGON:
			if( vertexCount >= 3 )
				R_DrawScratch( D3DPT_TRIANGLEFAN,
					vertexCount - 2, vertexCount, vertices,
					sizeof( vertex2d_t ) );
			break;

		case TRI_QUADS:

			if( vertexCount >= 6 )
			{
				R_DrawScratch( D3DPT_TRIANGLELIST,
					vertexCount / 3, vertexCount, vertices,
					sizeof( vertex2d_t ) );
			}
			break;

		case TRI_QUAD_STRIP:
			R_TriExpandQuadStrip( );
			if( !g_triApi.expandedScreenVertices.empty( ) )
			{
				const UINT expandedCount = static_cast<UINT>(
					g_triApi.expandedScreenVertices.size( ) );
				R_DrawScratch( D3DPT_TRIANGLELIST,
					expandedCount / 3, expandedCount,
					g_triApi.expandedScreenVertices.data( ),
					sizeof( vertex2d_t ) );
			}
			break;

		case TRI_TRIANGLES:
		default:
			if( vertexCount >= 3 )
				R_DrawScratch( D3DPT_TRIANGLELIST,
					vertexCount / 3, ( vertexCount / 3 ) * 3,
					vertices, sizeof( vertex2d_t ) );
			break;
		}
		return;
	}

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	device->SetTransform( D3DTS_WORLD, &identity );
	device->SetFVF( BRUSH_FVF );

	switch( g_triApi.primitiveMode )
	{
	case TRI_POINTS:
		R_DrawScratch( D3DPT_POINTLIST, vertexCount,
			vertexCount, g_triApi.vertices.data( ), sizeof( brush_vertex_t ) );
		break;

	case TRI_LINES:
		if( vertexCount >= 2 )
			R_DrawScratch( D3DPT_LINELIST, vertexCount / 2,
				( vertexCount / 2 ) * 2, g_triApi.vertices.data( ),
				sizeof( brush_vertex_t ) );
		break;

	case TRI_TRIANGLE_STRIP:
		if( vertexCount >= 3 )
			R_DrawScratch( D3DPT_TRIANGLESTRIP, vertexCount - 2,
				vertexCount, g_triApi.vertices.data( ), sizeof( brush_vertex_t ) );
		break;

	case TRI_TRIANGLE_FAN:
	case TRI_POLYGON:
		if( vertexCount >= 3 )
			R_DrawScratch( D3DPT_TRIANGLEFAN, vertexCount - 2,
				vertexCount, g_triApi.vertices.data( ), sizeof( brush_vertex_t ) );
		break;

	case TRI_QUADS:
		R_TriEmitQuadIndices( );
		if( !g_triApi.indices.empty( ) )
		{
			R_DrawScratchIndexed( D3DPT_TRIANGLELIST,
				vertexCount, static_cast<UINT>( g_triApi.indices.size( ) / 3 ),
				g_triApi.indices.data( ), D3DFMT_INDEX32,
				g_triApi.vertices.data( ), sizeof( brush_vertex_t ) );
		}
		break;

	case TRI_QUAD_STRIP:
		R_TriEmitQuadStripIndices( );
		if( !g_triApi.indices.empty( ) )
		{
			R_DrawScratchIndexed( D3DPT_TRIANGLELIST,
				vertexCount, static_cast<UINT>( g_triApi.indices.size( ) / 3 ),
				g_triApi.indices.data( ), D3DFMT_INDEX32,
				g_triApi.vertices.data( ), sizeof( brush_vertex_t ) );
		}
		break;

	case TRI_TRIANGLES:
	default:
		if( vertexCount >= 3 )
			R_DrawScratch( D3DPT_TRIANGLELIST, vertexCount / 3,
				( vertexCount / 3 ) * 3, g_triApi.vertices.data( ),
				sizeof( brush_vertex_t ) );
		break;
	}
}

static void R_TriColor4ub( byte red, byte green, byte blue, byte alpha )
{
	g_triApi.rgba[0] = red * ( 1.0f / 255.0f );
	g_triApi.rgba[1] = green * ( 1.0f / 255.0f );
	g_triApi.rgba[2] = blue * ( 1.0f / 255.0f );
	g_triApi.rgba[3] = alpha * ( 1.0f / 255.0f );
	g_triApi.vertexColor = D3DCOLOR_ARGB( 255, red, green, blue );

}

static void R_TriColor4f( float red, float green, float blue, float alpha )
{
	g_triApi.rgba[0] = red;
	g_triApi.rgba[1] = green;
	g_triApi.rgba[2] = blue;
	g_triApi.rgba[3] = alpha;

	if( g_triApi.renderMode == kRenderTransAlpha )
		g_triApi.vertexColor = R_TriOpaqueColor( red, green, blue );
	else
		g_triApi.vertexColor = R_TriOpaqueColor(
			red * alpha, green * alpha, blue * alpha );

}

static void R_TriTexCoord2f( float u, float v )
{
	g_triApi.texcoord[0] = u;
	g_triApi.texcoord[1] = v;
}

static void R_TriVertex3f( float x, float y, float z )
{
	if( !g_triApi.active )
		return;

	if( g_triApi.screenSpace )
	{

		vertex2d_t vertex;
		vertex.x = x - 0.5f;
		vertex.y = y - 0.5f;
		vertex.z = z;
		vertex.rhw = 1.0f;
		vertex.color = g_triApi.vertexColor;
		vertex.u = g_triApi.texcoord[0];
		vertex.v = g_triApi.texcoord[1];

		if( g_triApi.primitiveMode == TRI_QUADS )
		{

			g_triApi.screenQuad[g_triApi.screenQuadCount++] = vertex;
			if( g_triApi.screenQuadCount == 4 )
			{
				const vertex2d_t *q = g_triApi.screenQuad;
				g_triApi.screenVertices.push_back( q[0] );
				g_triApi.screenVertices.push_back( q[1] );
				g_triApi.screenVertices.push_back( q[2] );
				g_triApi.screenVertices.push_back( q[0] );
				g_triApi.screenVertices.push_back( q[2] );
				g_triApi.screenVertices.push_back( q[3] );
				g_triApi.screenQuadCount = 0;
			}
		}
		else
		{
			g_triApi.screenVertices.push_back( vertex );
		}
		return;
	}

	brush_vertex_t vertex;
	vertex.x = x;
	vertex.y = y;
	vertex.z = z;
	vertex.diffuse = g_triApi.vertexColor;
	vertex.u = g_triApi.texcoord[0];
	vertex.v = g_triApi.texcoord[1];
	g_triApi.vertices.push_back( vertex );
}

static void R_TriVertex3fv( const float *vertex )
{
	if( vertex )
		R_TriVertex3f( vertex[0], vertex[1], vertex[2] );
}

static void R_TriBrightness( float brightness )
{
	g_triApi.vertexColor = R_TriOpaqueColor(
		g_triApi.rgba[0] * g_triApi.rgba[3] * brightness,
		g_triApi.rgba[1] * g_triApi.rgba[3] * brightness,
		g_triApi.rgba[2] * g_triApi.rgba[3] * brightness );
	g_Color = g_triApi.vertexColor;
}

static void R_TriCullFace( TRICULLSTYLE mode )
{

	DX_SetRenderStateCached( D3DRS_CULLMODE,
		mode == TRI_FRONT ? D3DCULL_CW : D3DCULL_NONE );
}

static int R_TriSpriteTexture( model_t *spriteModel, int frame )
{
	if( !spriteModel || spriteModel->type != mod_sprite ||
		!spriteModel->cache.data || !gEngfuncs.R_GetSpriteFrame )
	{
		return 0;
	}

	mspriteframe_t *spriteFrame = static_cast<mspriteframe_t *>(
		gEngfuncs.R_GetSpriteFrame( spriteModel, static_cast<float>( frame ), 0.0f ) );
	if( !spriteFrame || spriteFrame->gl_texturenum <= 0 ||
		spriteFrame->gl_texturenum > g_texcount )
	{
		return 0;
	}

	DX_Bind( 0, spriteFrame->gl_texturenum );
	return 1;
}

static void R_TriFog( float flFogColor[3], float flStart, float flEnd, int bOn )
{

	if( g_dxFog.source == DX_FOG_INTERNAL_WATER ||
		g_dxFog.source == DX_FOG_INTERNAL_QUAKE ||
		dx_fog.value == 0.0f )
		return;

	g_dxFog.customEnabled = bOn != 0;
	g_dxFog.source = g_dxFog.customEnabled ? DX_FOG_CUSTOM : DX_FOG_NONE;

	if( flEnd <= flStart )
	{
		g_dxFog.customEnabled = false;
		g_dxFog.source = DX_FOG_NONE;
		DX_ApplyFog( );
		return;
	}

	if( flFogColor )
	{
		g_dxFog.color[0] = flFogColor[0] * ( 1.0f / 255.0f );
		g_dxFog.color[1] = flFogColor[1] * ( 1.0f / 255.0f );
		g_dxFog.color[2] = flFogColor[2] * ( 1.0f / 255.0f );
	}
	g_dxFog.color[3] = 1.0f;
	g_dxFog.start = flStart;
	g_dxFog.end = flEnd;

	if( g_dxFog.density <= 0.0f )
		g_dxFog.skybox = true;

	DX_ApplyFog( );
}

static void R_TriFogParams( float flDensity, int iFogSkybox )
{
	g_dxFog.density = std::max( 0.0f, flDensity );
	g_dxFog.skybox = iFogSkybox != 0;
	if( g_dxFog.source == DX_FOG_CUSTOM )
		DX_ApplyFog( );
}

static void R_TriScreenToWorld( const float *screen, float *world )
{
	if( !world )
		return;
	if( !device || !screen )
	{
		VectorClear( world );
		return;
	}

	D3DVIEWPORT9 viewport{};
	D3DXMATRIX worldMatrix{}, viewMatrix{}, projectionMatrix{};
	device->GetViewport( &viewport );
	device->GetTransform( D3DTS_WORLD, &worldMatrix );
	device->GetTransform( D3DTS_VIEW, &viewMatrix );
	device->GetTransform( D3DTS_PROJECTION, &projectionMatrix );

	D3DXVECTOR3 input( screen[0], screen[1], screen[2] );
	D3DXVECTOR3 output;
	D3DXVec3Unproject( &output, &input, &viewport,
		&projectionMatrix, &viewMatrix, &worldMatrix );
	world[0] = output.x;
	world[1] = output.y;
	world[2] = output.z;
}

static void R_TriCopyGLMatrix( const D3DXMATRIX &source, float *matrix )
{
	D3DXMATRIX transposed;
	D3DXMatrixTranspose( &transposed, &source );
	memcpy( matrix, &transposed, sizeof( float ) * 16 );
}

static void R_TriGetMatrix( const int pname, float *matrix )
{
	if( !matrix )
		return;

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	if( !device )
	{
		R_TriCopyGLMatrix( identity, matrix );
		return;
	}

	D3DXMATRIX world{}, view{}, projection{};
	switch( pname )
	{
	case GL_MODELVIEW_MATRIX:
		device->GetTransform( D3DTS_WORLD, &world );
		device->GetTransform( D3DTS_VIEW, &view );
		R_TriCopyGLMatrix( world * view, matrix );
		break;
	case GL_PROJECTION_MATRIX:
		device->GetTransform( D3DTS_PROJECTION, &projection );
		R_TriCopyGLMatrix( projection, matrix );
		break;
	case GL_TEXTURE_MATRIX:
		device->GetTransform( D3DTS_TEXTURE0, &world );
		R_TriCopyGLMatrix( world, matrix );
		break;
	default:
		R_TriCopyGLMatrix( identity, matrix );
		break;
	}
}

static int R_TriBoxInPVS( float *mins, float *maxs )
{

	(void)mins;
	(void)maxs;
	return 1;
}

static void R_TriLightAtPoint( float *pos, float *value )
{
	if( !value )
		return;
	if( !pos )
	{
		value[0] = value[1] = value[2] = 1.0f;
		return;
	}

	const colorVec color = R_LightPoint( pos );
	value[0] = static_cast<float>( color.r ) * ( 1.0f / 255.0f );
	value[1] = static_cast<float>( color.g ) * ( 1.0f / 255.0f );
	value[2] = static_cast<float>( color.b ) * ( 1.0f / 255.0f );
}

static void R_TriColor4fRendermode( float r, float g, float b, float a, int rendermode )
{
	R_TriRenderMode( rendermode );
	R_TriColor4f( r, g, b, a );
}

static const byte *R_GetTextureOriginalBuffer( unsigned int idx );

static void R_FillRenderAPI( render_api_t *api )
{
	if( !api ) return;
	memset( api, 0, sizeof( *api ) );
	api->RenderGetParm = RefGetParm;
	api->GetDetailScaleForTexture = R_GetDetailScale;
	api->GetExtraParmsForTexture = R_APIGetTextureParms;
	api->GetLightStyle = R_APIGetLightStyle;
	api->GetDynamicLight = R_APIGetDynamicLight;
	api->GetEntityLight = R_APIGetEntityLight;
	api->LightToTexGamma = R_APILightToTexGamma;
	api->GetFrameTime = R_APIGetFrameTime;
	api->R_SetCurrentEntity = R_APISetCurrentEntity;
	api->R_SetCurrentModel = R_APISetCurrentModel;
	api->R_FatPVS = R_APIFatPVS;
	api->R_StoreEfrags = R_APIStoreEfrags;
	api->GL_FindTexture = DX_FindTexture;
	api->GL_TextureName = DX_TextureName;
	api->GL_TextureData = R_GetTextureOriginalBuffer;
	api->GL_LoadTexture = DX_LoadTexture;
	api->GL_CreateTexture = DX_CreateTexture;
	api->GL_LoadTextureArray = R_APILoadTextureArray;
	api->GL_CreateTextureArray = R_APICreateTexArray;
	api->GL_FreeTexture = DX_FreeTexture;
	api->DrawSingleDecal = R_APIDrawSingleDecal;
	api->R_DecalSetupVerts = R_APIDecalSetupVerts;
	api->R_EntityRemoveDecals = R_APIRemoveEntityDecals;
	api->AVI_LoadVideo = R_APIAVILoadVideo;
	api->AVI_GetVideoInfo = R_APIAVIGetVideoInfo;
	api->AVI_GetVideoFrameNumber = R_APIVideoFrame;
	api->AVI_GetVideoFrame = R_APIAVIGetVideoFrame;
	api->AVI_UploadRawFrame = R_APIUploadVideoFrame;
	api->AVI_FreeVideo = R_APIAVIFreeVideo;
	api->AVI_IsActive = R_APIAVIIsActive;
	api->AVI_StreamSound = R_APIAVIStreamSound;
	api->AVI_Think = R_APIAVIThink;
	api->AVI_SetParm = R_APIAVISetParm;
	api->GL_Bind = DX_Bind;
	api->GL_SelectTexture = R_APISelectTexture;
	api->GL_LoadTextureMatrix = R_APILoadTexMatrix;
	api->GL_TexMatrixIdentity = R_APITexIdentity;
	api->GL_CleanUpTextureUnits = R_APICleanTextureUnits;
	api->GL_TexGen = R_APITexGen;
	api->GL_TextureTarget = R_APITextureTarget;
	api->GL_TexCoordArrayMode = R_APITexCoordArray;
	api->GL_GetProcAddress = gEngfuncs.GL_GetProcAddress;
	api->GL_UpdateTexSize = R_APIUpdateTexSize;
	api->GL_Reserved0 = R_SimpleStub;
	api->GL_Reserved1 = R_SimpleStub;
	api->GL_DrawParticles = R_APIGLDrawParticles;
	api->EnvShot = R_APIEnvShot;
	api->SPR_LoadExt = R_APISPRLoadExt;
	api->LightVec = R_APILightVec;
	api->StudioGetTexture = R_APIStudioGetTexture;
	api->GetOverviewParms = R_APIGetOverviewParms;
	api->GetFileByIndex = R_APIGetFileByIndex;
	api->pfnSaveFile = R_APISaveFile;
	api->R_Reserved0 = R_SimpleStub;
	api->pfnMemAlloc = R_APIMemAlloc;
	api->pfnMemFree = R_APIMemFree;
	api->pfnGetFilesList = R_APIGetFilesList;
	api->pfnFileBufferCRC32 = R_APIFileBufferCRC32;
	api->COM_CompareFileTime = R_APICompareFileTime;
	api->Host_Error = gEngfuncs.Host_Error;
	api->pfnGetModel = R_APIGetModel;
	api->pfnTime = R_APITime;
	api->Cvar_Set = R_APICvarSet;
	api->S_FadeMusicVolume = R_APIFadeMusicVolume;
	api->SetRandomSeed = R_APISetRandomSeed;
}

static void R_FillTriAPI( triangleapi_t *api )
{
	if( !api ) return;
	memset( api, 0, sizeof( *api ) );
	api->version = TRI_API_VERSION;
	api->RenderMode = R_TriRenderMode;
	api->Begin = R_TriBegin;
	api->End = R_TriEnd;
	api->Color4f = R_TriColor4f;
	api->Color4ub = R_TriColor4ub;
	api->TexCoord2f = R_TriTexCoord2f;
	api->Vertex3fv = R_TriVertex3fv;
	api->Vertex3f = R_TriVertex3f;
	api->Brightness = R_TriBrightness;
	api->CullFace = R_TriCullFace;
	api->SpriteTexture = R_TriSpriteTexture;
	api->WorldToScreen = WorldToScreen;
	api->Fog = R_TriFog;
	api->ScreenToWorld = R_TriScreenToWorld;
	api->GetMatrix = R_TriGetMatrix;
	api->BoxInPVS = R_TriBoxInPVS;
	api->LightAtPoint = R_TriLightAtPoint;
	api->Color4fRendermode = R_TriColor4fRendermode;
	api->FogParams = R_TriFogParams;
}

static int DX_ClampInt( int value, int minValue, int maxValue )
{
	return std::max( minValue, std::min( maxValue, value ) );
}

static int DX_MSAASamples( void ) { return DX_ClampInt( static_cast<int>( dx_msaa_samples.value + 0.5f ), 0, 16 ); }

static int DX_MSAAQuality( void ) { return DX_ClampInt( static_cast<int>( dx_msaa_quality.value ), -1, 1024 ); }

static D3DMULTISAMPLE_TYPE DX_MSAAType( int samples )
{
	switch( samples )
	{
	case 2: return D3DMULTISAMPLE_2_SAMPLES;
	case 3: return D3DMULTISAMPLE_3_SAMPLES;
	case 4: return D3DMULTISAMPLE_4_SAMPLES;
	case 5: return D3DMULTISAMPLE_5_SAMPLES;
	case 6: return D3DMULTISAMPLE_6_SAMPLES;
	case 7: return D3DMULTISAMPLE_7_SAMPLES;
	case 8: return D3DMULTISAMPLE_8_SAMPLES;
	case 9: return D3DMULTISAMPLE_9_SAMPLES;
	case 10: return D3DMULTISAMPLE_10_SAMPLES;
	case 11: return D3DMULTISAMPLE_11_SAMPLES;
	case 12: return D3DMULTISAMPLE_12_SAMPLES;
	case 13: return D3DMULTISAMPLE_13_SAMPLES;
	case 14: return D3DMULTISAMPLE_14_SAMPLES;
	case 15: return D3DMULTISAMPLE_15_SAMPLES;
	case 16: return D3DMULTISAMPLE_16_SAMPLES;
	default: return D3DMULTISAMPLE_NONE;
	}
}

static D3DFORMAT DX_BackBufferFormat( void )
{
	if( !d3d )
		return D3DFMT_X8R8G8B8;

	D3DDISPLAYMODE mode;
	if( SUCCEEDED( d3d->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &mode ) ) )
		return mode.Format;
	return D3DFMT_X8R8G8B8;
}

static void DX_ChooseMSAA( D3DPRESENT_PARAMETERS &params, bool printResult )
{
	const int requestedSamples = DX_MSAASamples( );
	const int requestedQuality = DX_MSAAQuality( );
	g_dxMSAASamplesReq = requestedSamples;
	g_dxMSAAQualityReq = requestedQuality;

	params.MultiSampleType = D3DMULTISAMPLE_NONE;
	params.MultiSampleQuality = 0;
	g_dxAppliedMSAASamples = 0;
	g_dxAppliedMSAAQuality = 0;

	if( requestedSamples < 2 || !d3d )
		return;

	if( g_dxMSAAForceDisabled && g_dxMSAAForceOff == requestedSamples )
		return;

	const D3DFORMAT backBufferFormat = DX_BackBufferFormat( );
	for( int samples = requestedSamples; samples >= 2; --samples )
	{
		const D3DMULTISAMPLE_TYPE type = DX_MSAAType( samples );
		if( type == D3DMULTISAMPLE_NONE )
			continue;

		DWORD colorQuality = 0;
		DWORD depthQuality = 0;
		if( FAILED( d3d->CheckDeviceMultiSampleType( D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL, backBufferFormat, TRUE, type, &colorQuality ) ) ||
			colorQuality == 0 )
		{
			continue;
		}
		if( FAILED( d3d->CheckDeviceMultiSampleType( D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL, params.AutoDepthStencilFormat, TRUE, type, &depthQuality ) ) ||
			depthQuality == 0 )
		{
			continue;
		}

		const DWORD qualityLevels = std::min( colorQuality, depthQuality );
		DWORD quality = qualityLevels - 1;
		if( requestedQuality >= 0 )
			quality = static_cast<DWORD>( std::min( requestedQuality,
				static_cast<int>( qualityLevels - 1 ) ) );

		params.BackBufferFormat = backBufferFormat;
		params.MultiSampleType = type;
		params.MultiSampleQuality = quality;
		g_dxAppliedMSAASamples = samples;
		g_dxAppliedMSAAQuality = static_cast<int>( quality );
		if( printResult )
		{
			gEngfuncs.Con_Printf( "D3D9: MSAA %dx enabled (quality %u/%u)\n",
				samples, quality, qualityLevels - 1 );
		}
		return;
	}

	if( printResult )
		gEngfuncs.Con_Printf( "D3D9: requested MSAA %dx is unsupported, using no MSAA\n", requestedSamples );
}

static void DX_SetupPresent( bool printResult )
{
	ZeroMemory( &d3dpp, sizeof( d3dpp ) );
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hwnd;
	d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.BackBufferCount = 1;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	DX_ChooseMSAA( d3dpp, printResult );
}

static bool DX_PresentChanged( void )
{
	return DX_MSAASamples( ) != g_dxMSAASamplesReq ||
		DX_MSAAQuality( ) != g_dxMSAAQualityReq;
}

static void DX_UpdateTextureQuality( bool force )
{
	if( !device )
		return;

	D3DCAPS9 caps{};
	device->GetDeviceCaps( &caps );

	int requested = static_cast<int>( dx_anisotropy.value + 0.5f );
	requested = DX_ClampInt( requested, 0, 16 );
	int resolved = 1;
	if( requested > 1 && caps.MaxAnisotropy > 1 &&
		( caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC ) )
	{
		resolved = std::min( requested, static_cast<int>( caps.MaxAnisotropy ) );
	}

	if( force || resolved != g_dxAppliedAnisotropy )
	{
		DX_SetGlobalAnisotropy( resolved );
		g_dxAppliedAnisotropy = resolved;
		if( resolved > 1 )
			gEngfuncs.Con_Printf( "D3D9: anisotropic filtering enabled %dx\n", resolved );
		else
			gEngfuncs.Con_Printf( "D3D9: anisotropic filtering disabled\n" );
	}
}

static bool DX_PathExists( const wchar_t *path )
{
	if( !path || !path[0] )
		return false;
	const DWORD attrs = GetFileAttributesW( path );
	return attrs != INVALID_FILE_ATTRIBUTES && ( attrs & FILE_ATTRIBUTE_DIRECTORY ) == 0;
}

static void DX_PathDir( const wchar_t *path, wchar_t *outDir, size_t outCount )
{
	if( !outDir || outCount == 0 )
		return;
	outDir[0] = L'\0';
	if( !path || !path[0] )
		return;

	wcsncpy_s( outDir, outCount, path, _TRUNCATE );
	wchar_t *slash = wcsrchr( outDir, L'\\' );
	wchar_t *forwardSlash = wcsrchr( outDir, L'/' );
	if( forwardSlash && ( !slash || forwardSlash > slash ) )
		slash = forwardSlash;
	if( slash )
		*slash = L'\0';
	else
		outDir[0] = L'\0';
}

static bool DX_PathStartsWith( const wchar_t *path, const wchar_t *prefix )
{
	if( !path || !prefix || !prefix[0] )
		return false;
	const size_t prefixLength = wcslen( prefix );
	return _wcsnicmp( path, prefix, prefixLength ) == 0;
}

static bool DX_IsSystemD3D9( const wchar_t *d3d9Path )
{
	if( !d3d9Path || !d3d9Path[0] )
		return true;

	wchar_t systemDir[MAX_PATH] = {};
	if( GetSystemDirectoryW( systemDir, MAX_PATH ) > 0 &&
		DX_PathStartsWith( d3d9Path, systemDir ) )
	{
		return true;
	}

	wchar_t windowsDir[MAX_PATH] = {};
	if( GetWindowsDirectoryW( windowsDir, MAX_PATH ) > 0 )
	{
		wchar_t system32[MAX_PATH] = {};
		wchar_t syswow64[MAX_PATH] = {};
		_snwprintf_s( system32, MAX_PATH, _TRUNCATE, L"%s\\System32", windowsDir );
		_snwprintf_s( syswow64, MAX_PATH, _TRUNCATE, L"%s\\SysWOW64", windowsDir );
		if( DX_PathStartsWith( d3d9Path, system32 ) ||
			DX_PathStartsWith( d3d9Path, syswow64 ) )
		{
			return true;
		}
	}

	return false;
}

static bool DX_ProcessRunning( const wchar_t *exeName )
{
	if( !exeName || !exeName[0] )
		return false;

	HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
		return false;

	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof( entry );
	bool found = false;
	if( Process32FirstW( snapshot, &entry ) )
	{
		do
		{
			if( _wcsicmp( entry.szExeFile, exeName ) == 0 )
			{
				found = true;
				break;
			}
		} while( Process32NextW( snapshot, &entry ) );
	}
	CloseHandle( snapshot );
	return found;
}

static bool DX_DetectRemix( void )
{

	HMODULE d3d9Module = GetModuleHandleW( L"d3d9.dll" );
	if( !d3d9Module )
		return false;

	wchar_t d3d9Path[MAX_PATH] = {};
	if( GetModuleFileNameW( d3d9Module, d3d9Path, MAX_PATH ) == 0 )
		return false;
	if( DX_IsSystemD3D9( d3d9Path ) )
		return false;

	wchar_t baseDir[MAX_PATH] = {};
	DX_PathDir( d3d9Path, baseDir, MAX_PATH );
	if( !baseDir[0] )
		return false;

	wchar_t remixRenderer[MAX_PATH] = {};
	wchar_t remixBridge[MAX_PATH] = {};
	_snwprintf_s( remixRenderer, MAX_PATH, _TRUNCATE,
		L"%s\\.trex\\d3d9.dll", baseDir );
	_snwprintf_s( remixBridge, MAX_PATH, _TRUNCATE,
		L"%s\\.trex\\NvRemixBridge.exe", baseDir );

	if( !DX_PathExists( remixRenderer ) || !DX_PathExists( remixBridge ) )
		return false;

	return DX_ProcessRunning( L"NvRemixBridge.exe" );
}

static void DX_UpdateRemix( void )
{
	const bool detected = DX_DetectRemix( );
	if( gEngfuncs.Cvar_SetValue )
		gEngfuncs.Cvar_SetValue( "dx_remix", detected ? 1.0f : 0.0f );
	else
		dx_remix.value = detected ? 1.0f : 0.0f;

	gEngfuncs.Con_Printf( "D3D9: RTX Remix %s (dx_remix %d)\n",
		detected ? "detected" : "not detected", detected ? 1 : 0 );
}

static HRESULT DX_CreateDeviceFlags( DWORD flags )
{
	return d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
		flags | D3DCREATE_FPU_PRESERVE, &d3dpp, &device );
}

static HRESULT DX_CreateDevice( void )
{
	HRESULT result = DX_CreateDeviceFlags( D3DCREATE_HARDWARE_VERTEXPROCESSING );
	if( FAILED( result ) )
		result = DX_CreateDeviceFlags( D3DCREATE_SOFTWARE_VERTEXPROCESSING );

	if( FAILED( result ) && d3dpp.MultiSampleType != D3DMULTISAMPLE_NONE )
	{
		gEngfuncs.Con_Printf( "D3D9: CreateDevice failed with MSAA, retrying without MSAA\n" );
		g_dxMSAAForceDisabled = true;
		g_dxMSAAForceOff = DX_MSAASamples( );
		DX_SetupPresent( false );
		result = DX_CreateDeviceFlags( D3DCREATE_HARDWARE_VERTEXPROCESSING );
		if( FAILED( result ) )
			result = DX_CreateDeviceFlags( D3DCREATE_SOFTWARE_VERTEXPROCESSING );
	}
	return result;
}

static bool DX_ResetDevice( bool rebuildPresentParams )
{
	if( !device )
		return false;

	if( rebuildPresentParams )
		DX_SetupPresent( true );

	if( g_dxLine ) g_dxLine->OnLostDevice( );
	R_FreeScratchBuffers( );
	R_ModelsLostDevice( );
	DX_InvalidateTextureBindCache( );

	HRESULT result = device->Reset( &d3dpp );
	if( FAILED( result ) && d3dpp.MultiSampleType != D3DMULTISAMPLE_NONE )
	{
		gEngfuncs.Con_Printf( "D3D9: Reset failed with MSAA, retrying without MSAA\n" );
		g_dxMSAAForceDisabled = true;
		g_dxMSAAForceOff = DX_MSAASamples( );
		DX_SetupPresent( false );
		result = device->Reset( &d3dpp );
	}

	if( FAILED( result ) )
		return false;

	if( g_dxLine ) g_dxLine->OnResetDevice( );
	DX_InvalidateTextureBindCache( );
	DX_RefreshFogCaps( );
	g_lastGamma = -1.0f;
	g_lastBrightness = -1.0f;
	DX_UpdateTextureQuality( true );
	R_ApplyGammaRamp( );
	return true;
}

static void DX_FreeVideo( void )
{
	if( !g_dxVideoInitialized )
		return;

	gEngfuncs.R_Free_Video( );
	g_dxVideoInitialized = false;
}

static qboolean R_Init( void )
{
	R_RegisterContextCvars( );
	R_RegisterModelCvars( );

	if( !gEngfuncs.R_Init_Video( REF_D3D ) )
	{
		gEngfuncs.Con_Printf( "D3D9: failed to initialize platform video\n" );
		return false;
	}
	g_dxVideoInitialized = true;

	void *windowHandle = nullptr;
	const ref_window_type_t windowType =
		gEngfuncs.R_GetWindowHandle( &windowHandle, REF_WINDOW_TYPE_WIN32 );
	hwnd = windowType == REF_WINDOW_TYPE_WIN32
		? static_cast<HWND>( windowHandle ) : nullptr;
	if( !hwnd || !IsWindow( hwnd ) )
	{
		gEngfuncs.Con_Printf( "D3D9: no valid Win32 window handle available\n" );
		hwnd = nullptr;
		DX_FreeVideo( );
		return false;
	}

	d3d = Direct3DCreate9( D3D_SDK_VERSION );
	if( !d3d )
	{
		gEngfuncs.Con_Printf( "D3D9: failed to create IDirect3D9\n" );
		hwnd = nullptr;
		DX_FreeVideo( );
		return false;
	}

	DX_SetupPresent( true );
	const HRESULT result = DX_CreateDevice( );
	if( FAILED( result ) || !device )
	{
		gEngfuncs.Con_Printf( "D3D9: CreateDevice failed\n" );
		d3d->Release( );
		d3d = nullptr;
		hwnd = nullptr;
		DX_FreeVideo( );
		return false;
	}

	DX_ProfilerRegisterCommands( );
	R_RegisterTextureCommands( );
	DX_UpdateRemix( );

	if( FAILED( D3DXCreateLine( device, &g_dxLine ) ) )
		g_dxLine = nullptr;

	DX_RefreshFogCaps( );
	DX_ResetFog( );
	DX_UpdateTextureQuality( true );
	R_ApplyGammaRamp( );
	return true;
}

static void R_Shutdown( void )
{
	R_RemoveTextureCommands( );
	DX_ProfilerRemoveCommands( );
	R_FreeScratchBuffers( );
	R_ModelsLostDevice( );
	DX_InvalidateTextureBindCache( );

	if( g_dxLine )
	{
		g_dxLine->Release( );
		g_dxLine = nullptr;
	}
	if( device )
	{
		device->Release( );
		device = nullptr;
	}
	if( d3d )
	{
		d3d->Release( );
		d3d = nullptr;
	}

	hwnd = nullptr;
	DX_FreeVideo( );
	g_previousClientTime = -1.0f;
	g_dxCullValid = false;
}

static const char *R_GetConfigName( void )
{
	return "ref_d3d9";
}

static qboolean R_SetDisplayTransform( ref_screen_rotation_t rotate, int x, int y, float scale_x, float scale_y )
{

	return true;
}

static void DX_SetupAttributes( int safegl )
{
	if( !device )
		return;

	DX_SetRenderStateCached( D3DRS_LIGHTING, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetSamplerStateCached( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
}

static cl_entity_s *R_EngineViewModelEntity( void )
{
	return reinterpret_cast<cl_entity_s *>(
		ENGINE_GET_PARM( PARM_GET_VIEWENT_PTR ) );
}

static cl_entity_s *R_CurrentViewModel( void )
{
	cl_entity_s *viewModel = R_EngineViewModelEntity( );
	if( !viewModel || !viewModel->model ||
		viewModel->model->type != mod_studio ||
		( viewModel->curstate.effects & EF_NODRAW ) )
	{
		return nullptr;
	}

	return viewModel;
}

static bool R_IsViewModelEntity( const cl_entity_s *entity )
{

	return entity && entity == R_EngineViewModelEntity( );
}

static bool R_IsRemotePlayerEntity( const cl_entity_s *entity )
{
	if( !entity || !entity->player || !gp_cl )
		return false;
	if( entity->index <= 0 || entity->index > gp_cl->maxclients )
		return false;
	return entity->index != gp_cl->playernum + 1;
}

static entity_state_t *R_GetPlayerState( const cl_entity_s *entity )
{
	if( !R_IsRemotePlayerEntity( entity ) || !gEngfuncs.pfnGetPlayerState )
		return nullptr;

	const int playerIndex = entity->index - 1;
	if( playerIndex < 0 || playerIndex >= gp_cl->maxclients )
		return nullptr;

	return gEngfuncs.pfnGetPlayerState( playerIndex );
}

static bool R_PlayerStateRenderable( const cl_entity_s *entity,
	const entity_state_t *state )
{
	if( !entity || !state )
		return false;
	if( state->modelindex <= 0 )
		return false;

	if( state->number > 0 && state->number != entity->index )
		return false;
	return true;
}

static float R_PlayerStateDistSq( const float a[3], const float b[3] )
{
	const float dx = a[0] - b[0];
	const float dy = a[1] - b[1];
	const float dz = a[2] - b[2];
	return dx * dx + dy * dy + dz * dz;
}

static float R_LerpAngleDegrees( float a, float b, float t )
{
	float delta = b - a;
	while( delta > 180.0f ) delta -= 360.0f;
	while( delta < -180.0f ) delta += 360.0f;
	return a + delta * t;
}

static float R_LerpStudioFrame256( float a, float b, float t )
{
	float delta = b - a;

	if( delta > 128.0f )
		delta -= 256.0f;
	else if( delta < -128.0f )
		delta += 256.0f;
	float out = a + delta * t;
	while( out < 0.0f ) out += 256.0f;
	while( out >= 256.0f ) out -= 256.0f;
	return out;
}

static bool R_DeadPlayerExpired( const cl_entity_s *entity )
{
	if( !entity || !gp_cl )
		return false;

	const entity_state_t *state = R_GetPlayerState( entity );
	if( entity->curstate.renderfx == kRenderFxDeadPlayer && state )
	{

		const float distSq = R_PlayerStateDistSq( entity->curstate.origin, state->origin );
		if( state->renderfx != kRenderFxDeadPlayer || distSq > 32.0f * 32.0f )
			return true;
	}

	if( dx_player_dead_body_timeout.value <= 0.0f )
		return false;
	if( entity->curstate.renderfx != kRenderFxDeadPlayer )
		return false;

	float stamp = entity->curstate.animtime;
	if( stamp <= 0.0f )
		stamp = entity->curstate.msg_time;
	if( stamp <= 0.0f )
		return false;

	const float timeout = std::max( 0.05f, dx_player_dead_body_timeout.value );
	return static_cast<float>( gp_cl->time - stamp ) > timeout;
}

static bool R_RemotePlayerStale( const cl_entity_s *entity )
{
	if( !R_IsRemotePlayerEntity( entity ) || dx_player_stale_timeout.value <= 0.0f )
		return false;

	const float timeout = std::max( 0.05f, dx_player_stale_timeout.value );

	const entity_state_t *state = R_GetPlayerState( entity );
	if( state )
	{
		if( !R_PlayerStateRenderable( entity, state ) )
			return true;

		const float age = static_cast<float>( gp_cl->time - state->msg_time );
		if( age > timeout )
			return true;

		const float staleDistSq = R_PlayerStateDistSq( entity->curstate.origin, state->origin );
		const float teleport = std::max( 32.0f, dx_player_interp_teleport_dist.value );
		if( staleDistSq > teleport * teleport && entity->curstate.renderfx == kRenderFxDeadPlayer )
			return true;

		return false;
	}

	if( entity->curstate.number <= 0 || entity->curstate.modelindex <= 0 )
		return true;
	const float age = static_cast<float>( gp_cl->time - entity->curstate.msg_time );
	return age > timeout;
}

static bool R_PlayerInterpChanged( const entity_state_t &a,
	const entity_state_t &b )
{
	if( fabsf( a.msg_time - b.msg_time ) > 0.0001f ) return true;
	if( R_PlayerStateDistSq( a.origin, b.origin ) > 0.001f ) return true;
	if( R_PlayerStateDistSq( a.angles, b.angles ) > 0.001f ) return true;
	if( a.modelindex != b.modelindex || a.renderfx != b.renderfx ) return true;

	if( a.sequence != b.sequence || a.gaitsequence != b.gaitsequence ||
		a.weaponmodel != b.weaponmodel ) return true;
	return false;
}

static int R_PlayerInterpSampleIndex( const player_interp_t &history, int chronologicalIndex )
{
	if( history.sampleCount <= 0 )
		return 0;
	const int first = ( history.nextSample - history.sampleCount + DX_PLAYER_INTERP_HISTORY ) % DX_PLAYER_INTERP_HISTORY;
	return ( first + chronologicalIndex ) % DX_PLAYER_INTERP_HISTORY;
}

static void R_ResetPlayerInterp( player_interp_t &history,
	const entity_state_t &state )
{
	history.valid = true;
	history.previous = state;
	history.current = state;
	history.sampleCount = 1;
	history.nextSample = 1 % DX_PLAYER_INTERP_HISTORY;
	history.samples[0].state = state;
	history.samples[0].arrivalTime = gp_cl ? static_cast<float>( gp_cl->time ) : state.msg_time;
}

static void R_PushPlayerInterp( player_interp_t &history,
	const entity_state_t &state )
{
	const float now = gp_cl ? static_cast<float>( gp_cl->time ) : state.msg_time;
	history.samples[history.nextSample].state = state;
	history.samples[history.nextSample].arrivalTime = now;
	history.nextSample = ( history.nextSample + 1 ) % DX_PLAYER_INTERP_HISTORY;
	if( history.sampleCount < DX_PLAYER_INTERP_HISTORY )
		++history.sampleCount;
}

static void R_UpdatePlayerInterp( int playerIndex,
	const entity_state_t &state )
{
	if( playerIndex <= 0 || playerIndex > MAX_CLIENTS )
		return;

	player_interp_t &history = g_remotePlayerInterp[playerIndex];
	if( !history.valid )
	{
		R_ResetPlayerInterp( history, state );
		return;
	}

	const float teleport = std::max( 32.0f, dx_player_interp_teleport_dist.value );
	const float distSq = R_PlayerStateDistSq( history.current.origin, state.origin );
	const bool timeWentBack = state.msg_time + 0.0001f < history.current.msg_time;
	const bool respawnOrModelSwap = state.modelindex != history.current.modelindex ||
		( state.renderfx != history.current.renderfx &&
		  ( state.renderfx == kRenderFxDeadPlayer || history.current.renderfx == kRenderFxDeadPlayer ) );

	if( timeWentBack || respawnOrModelSwap || distSq > teleport * teleport )
	{

		R_ResetPlayerInterp( history, state );
		return;
	}

	if( R_PlayerInterpChanged( history.current, state ) )
	{
		history.previous = history.current;
		history.current = state;
		R_PushPlayerInterp( history, state );
	}
}

static void R_LerpPlayerState( const entity_state_t &a,
	const entity_state_t &b, float frac, cl_entity_s &copy )
{
	if( frac < 0.0f ) frac = 0.0f;
	if( frac > 1.0f ) frac = 1.0f;

	for( int axis = 0; axis < 3; ++axis )
	{
		copy.origin[axis] = a.origin[axis] + ( b.origin[axis] - a.origin[axis] ) * frac;
		copy.angles[axis] = R_LerpAngleDegrees( a.angles[axis], b.angles[axis], frac );
		copy.curstate.origin[axis] = copy.origin[axis];
		copy.curstate.angles[axis] = copy.angles[axis];
	}

	if( a.sequence == b.sequence )
	{
		copy.curstate.sequence = b.sequence;
		copy.curstate.frame = R_LerpStudioFrame256( a.frame, b.frame, frac );
	}
	else
	{
		copy.curstate.sequence = b.sequence;
		copy.curstate.frame = b.frame;
	}
	copy.curstate.gaitsequence = b.gaitsequence;
	copy.curstate.weaponmodel = b.weaponmodel;
	copy.curstate.framerate = a.framerate + ( b.framerate - a.framerate ) * frac;

	copy.curstate.animtime = gp_cl ? static_cast<float>( gp_cl->time ) : b.animtime;

	for( int i = 0; i < 4; ++i )
	{
		copy.curstate.controller[i] = static_cast<byte>( std::max( 0.0f, std::min( 255.0f,
			a.controller[i] + ( b.controller[i] - a.controller[i] ) * frac + 0.5f ) ) );
		copy.latched.prevcontroller[i] = copy.curstate.controller[i];
	}
	for( int i = 0; i < 2; ++i )
	{
		copy.curstate.blending[i] = static_cast<byte>( std::max( 0.0f, std::min( 255.0f,
			a.blending[i] + ( b.blending[i] - a.blending[i] ) * frac + 0.5f ) ) );
		copy.latched.prevblending[i] = copy.curstate.blending[i];
		copy.latched.prevseqblending[i] = copy.curstate.blending[i];
	}

	copy.latched.prevanimtime = copy.curstate.animtime;
	VectorCopy( copy.origin, copy.latched.prevorigin );
	VectorCopy( copy.angles, copy.latched.prevangles );
}

static void R_ApplyPlayerInterp( int playerIndex, cl_entity_s &copy )
{
	if( dx_player_interpolation.value == 0.0f || playerIndex <= 0 ||
		playerIndex > MAX_CLIENTS || !gp_cl )
	{
		VectorCopy( copy.curstate.origin, copy.origin );
		VectorCopy( copy.curstate.angles, copy.angles );
		return;
	}

	const player_interp_t &history = g_remotePlayerInterp[playerIndex];
	if( !history.valid || history.sampleCount <= 0 )
	{
		VectorCopy( copy.curstate.origin, copy.origin );
		VectorCopy( copy.curstate.angles, copy.angles );
		return;
	}

	if( history.sampleCount == 1 )
	{
		R_LerpPlayerState( history.samples[R_PlayerInterpSampleIndex( history, 0 )].state,
			history.samples[R_PlayerInterpSampleIndex( history, 0 )].state, 0.0f, copy );
		return;
	}

	const float delay = std::max( 0.0f, std::min( 0.5f, dx_player_interp_delay.value ) );
	const float renderTime = static_cast<float>( gp_cl->time ) - delay;

	const player_interp_sample_t &oldestSample =
		history.samples[R_PlayerInterpSampleIndex( history, 0 )];
	const player_interp_sample_t &latestSample =
		history.samples[R_PlayerInterpSampleIndex( history, history.sampleCount - 1 )];
	if( renderTime <= oldestSample.arrivalTime )
	{
		R_LerpPlayerState( oldestSample.state, oldestSample.state, 0.0f, copy );
		return;
	}

	int older = history.sampleCount - 2;
	int newer = history.sampleCount - 1;
	for( int i = 1; i < history.sampleCount; ++i )
	{
		const int idx = R_PlayerInterpSampleIndex( history, i );
		if( history.samples[idx].arrivalTime >= renderTime )
		{
			older = i - 1;
			newer = i;
			break;
		}
	}

	const player_interp_sample_t &oldSample =
		history.samples[R_PlayerInterpSampleIndex( history, older )];
	const player_interp_sample_t &newSample =
		history.samples[R_PlayerInterpSampleIndex( history, newer )];

	if( renderTime <= latestSample.arrivalTime &&
		newSample.arrivalTime > oldSample.arrivalTime + 0.0001f )
	{
		const float frac = ( renderTime - oldSample.arrivalTime ) /
			( newSample.arrivalTime - oldSample.arrivalTime );
		R_LerpPlayerState( oldSample.state, newSample.state, frac, copy );
		return;
	}

	const int lastChrono = history.sampleCount - 1;
	const int prevChrono = std::max( 0, lastChrono - 1 );
	const player_interp_sample_t &prevSample =
		history.samples[R_PlayerInterpSampleIndex( history, prevChrono )];
	const player_interp_sample_t &lastSample =
		history.samples[R_PlayerInterpSampleIndex( history, lastChrono )];

	float frac = 1.0f;
	const float dt = lastSample.arrivalTime - prevSample.arrivalTime;
	if( dt > 0.0001f )
	{
		const float maxExtra = std::max( 0.0f, std::min( 1.0f, dx_player_interp_extrapolate.value ) );
		frac = 1.0f + std::max( 0.0f, std::min( maxExtra,
			( renderTime - lastSample.arrivalTime ) / dt ) );
	}

	entity_state_t extrapolated = lastSample.state;
	if( prevSample.state.sequence == lastSample.state.sequence )
		extrapolated.frame = R_LerpStudioFrame256( prevSample.state.frame,
			lastSample.state.frame, frac );
	extrapolated.framerate = prevSample.state.framerate +
		( lastSample.state.framerate - prevSample.state.framerate ) * frac;
	for( int axis = 0; axis < 3; ++axis )
	{
		extrapolated.origin[axis] = prevSample.state.origin[axis] +
			( lastSample.state.origin[axis] - prevSample.state.origin[axis] ) * frac;
		extrapolated.angles[axis] = R_LerpAngleDegrees( prevSample.state.angles[axis],
			lastSample.state.angles[axis], frac );
	}
	R_LerpPlayerState( extrapolated, extrapolated, 0.0f, copy );
}

static model_t *R_ResolvePlayerModel( const cl_entity_s *entity )
{
	if( !entity || !entity->player || !gp_cl ||
		!gEngfuncs.pfnPlayerInfo || !gEngfuncs.Mod_ForName )
	{
		return entity ? entity->model : nullptr;
	}

	const int playerIndex = entity->index - 1;
	if( playerIndex < 0 || playerIndex >= gp_cl->maxclients ||
		playerIndex >= MAX_CLIENTS )
	{
		return entity->model;
	}

	player_model_cache_t &cache = g_playerModelCache[playerIndex];
	const double now = gp_cl->time;

	if( cache.resolved && now < cache.nextCheckTime )
		return cache.model ? cache.model : entity->model;

	player_info_t *info = gEngfuncs.pfnPlayerInfo( playerIndex );

	if( !info || !info->model )
	{
		cache.nextCheckTime = now + 1.0;
		return cache.model ? cache.model : entity->model;
	}

	char modelName[sizeof( cache.name )]{};
	if( !R_CopyPlayerModelName( info->model, modelName, sizeof( modelName ) ) )
	{
		cache.nextCheckTime = now + 1.0;
		return cache.model ? cache.model : entity->model;
	}

	const bool changed = !cache.resolved || strcmp( cache.name, modelName ) != 0;
	if( changed )
	{
		memcpy( cache.name, modelName, sizeof( cache.name ) );
		char path[MAX_OSPATH]{};
		const int written = snprintf( path, sizeof( path ),
			"models/player/%s/%s.mdl", cache.name, cache.name );
		if( written < 0 || static_cast<size_t>( written ) >= sizeof( path ) )
		{
			cache.model = nullptr;
			cache.resolved = true;
		}
		else
		{
			model_t *resolved = gEngfuncs.Mod_ForName( path, false, false );
			cache.model = ( resolved && resolved->type == mod_studio ) ? resolved : nullptr;
			cache.resolved = true;
		}
	}

	cache.nextCheckTime = now + 1.0;
	return cache.model ? cache.model : entity->model;
}

static cl_entity_s *R_PreparePlayerCustomModel( cl_entity_s *entity )
{
	if( !entity || !entity->player )
		return entity;

	model_t *selected = R_ResolvePlayerModel( entity );
	if( !selected || selected == entity->model )
		return entity;

	if( entity->index <= 0 || entity->index > MAX_CLIENTS )
		return entity;

	cl_entity_s &copy = g_playerDrawEntityCopies[entity->index];
	if( entity != &copy )
		copy = *entity;

	copy.model = selected;

	return &copy;
}

static cl_entity_s *R_PreparePlayerEntity( cl_entity_s *entity )
{
	if( !entity )
		return nullptr;
	if( !R_IsRemotePlayerEntity( entity ) || dx_player_use_engine_state.value == 0.0f )
		return entity;
	if( entity->index <= 0 || entity->index > MAX_CLIENTS )
		return nullptr;

	entity_state_t *state = R_GetPlayerState( entity );
	if( !R_PlayerStateRenderable( entity, state ) )
		return nullptr;

	const float age = static_cast<float>( gp_cl->time - state->msg_time );
	const float timeout = std::max( 0.05f, dx_player_stale_timeout.value );
	if( dx_player_stale_timeout.value > 0.0f && age > timeout )
		return nullptr;

	R_UpdatePlayerInterp( entity->index, *state );

	cl_entity_s &copy = g_playerDrawEntityCopies[entity->index];
	copy = *entity;
	copy.curstate = *state;
	copy.index = entity->index;
	copy.player = entity->player;
	R_ApplyPlayerInterp( entity->index, copy );
	return &copy;
}

static qboolean R_AddEntity( cl_entity_s *clent, int type )
{
	if( !clent || !clent->model )
		return false;
	if( clent->curstate.effects & EF_NODRAW )
		return false;
	if( R_DeadPlayerExpired( clent ) )
		return false;
	if( R_RemotePlayerStale( clent ) )
		return false;

	clent = R_PreparePlayerEntity( clent );
	if( !clent )
		return false;

	clent = R_PreparePlayerCustomModel( clent );
	if( !clent )
		return false;

	if( R_IsRemotePlayerEntity( clent ) )
	{
		if( clent->index <= 0 || clent->index > MAX_CLIENTS )
			return false;
		if( g_playerSubmitted[clent->index] )
			return false;
		g_playerSubmitted[clent->index] = true;
	}

	if( R_IsViewModelEntity( clent ) )
		return true;

	if( type == ET_BEAM )
	{
		if( g_beamEntityCount >= DX_MAX_BEAM_ENTITIES )
			return false;
		g_beamEntities[g_beamEntityCount++] = clent;
		return true;
	}

	const int modelType = clent->model->type;
	if( modelType != mod_brush &&
		modelType != mod_studio &&
		modelType != mod_sprite )
	{
		return true;
	}

	if( ( clent->curstate.effects & ( EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_LIGHT ) ) != 0 )
	{
		g_entityLightEmitters.push_back( clent );
		++g_entityLightEmitterCount;
	}

	const int effectiveRenderMode = modelType == mod_studio
		? R_StudioRenderMode( clent )
		: clent->curstate.rendermode;

	if( effectiveRenderMode != kRenderNormal )
	{
		g_translucentCandidates.push_back( clent );
		++g_translucentCandidateCount;
		return true;
	}

	switch( modelType )
	{
	case mod_brush:
		g_opaqueBrushEntities.push_back( clent );
		++g_opaqueBrushCount;
		break;

	case mod_studio:
		g_opaqueStudioEntities.push_back( clent );
		++g_opaqueStudioCount;
		break;

	case mod_sprite:
		g_opaqueSpriteEntities.push_back( clent );
		++g_opaqueSpriteCount;
		break;
	}

	return true;
}

static void R_ProcessEntData( qboolean allocate, cl_entity_t *entities,
	unsigned int max_entities )
{

	(void)allocate;
	(void)entities;
	(void)max_entities;
}

static const byte *R_GetTextureOriginalBuffer( unsigned int idx )
{
	if( idx == 0 || idx > static_cast<unsigned int>( g_texcount ) )
		return nullptr;

	const dxtexture_t &texture = g_Textures[idx];
	return texture.original ? texture.original->buffer : nullptr;
}

static void DX_ProcessTexture( int texnum, float gamma, int topColor, int bottomColor )
{

}

static void R_SetupSky( int *skytextures )
{
	if( !skytextures )
	{
		memset( g_skyTextures, 0, sizeof( g_skyTextures ) );
		return;
	}

	for( int i = 0; i < DX_SKYBOX_SIDES; ++i )
		g_skyTextures[i] = skytextures[i];
}

static void R_Apply2DNeutralState( void )
{
	if( !device )
		return;

	DX_AllowFog( false );
	DX_BindRawTexture( 1, nullptr );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	DX_SetRenderStateCached( D3DRS_ZENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_LIGHTING, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
}

static void R_Set2DMode( qboolean enable )
{
	if( !device )
		return;
	if( !enable )
	{

		R_Flush2DBatch( );
		if( !g_2dModeActive )
			return;
		g_2dModeActive = false;
		DX_AllowFog( true );
		return;
	}

	if( g_2dModeActive )
		return;

	g_2dModeActive = true;
	R_Apply2DNeutralState( );
}

static void FillRGBA( int rendermode, float x, float y, float w, float h, byte r, byte g, byte b, byte a )
{
	if( !device || w <= 0.0f || h <= 0.0f )
		return;

	R_Flush2DBatch( );
	DX_PROFILE_SCOPE( profile2dFillScope, DX_PROFILE_2D );
	R_Set2DMode( true );
	DX_BindRawTexture( 0, nullptr );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE,
		( a < 255 || rendermode != kRenderNormal ) ? TRUE : FALSE );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

	vertex2d_t v[4];
	const DWORD color = D3DCOLOR_ARGB( a, r, g, b );
	v[0] = { x,     y,     0.0f, 1.0f, color, 0.0f, 0.0f };
	v[1] = { x + w, y,     0.0f, 1.0f, color, 1.0f, 0.0f };
	v[2] = { x,     y + h, 0.0f, 1.0f, color, 0.0f, 1.0f };
	v[3] = { x + w, y + h, 0.0f, 1.0f, color, 1.0f, 1.0f };

	device->SetFVF( D3DFVF_VERTEX2D );
	R_DrawScratch( D3DPT_TRIANGLESTRIP, 2, 4,
		v, sizeof( vertex2d_t ) );
}

static int WorldToScreen( const vec3_t world, vec3_t screen )
{
	if( !device || !world || !screen )
		return true;

	D3DVIEWPORT9 viewport{};
	D3DXMATRIX worldMatrix{}, viewMatrix{}, projectionMatrix{};
	device->GetViewport( &viewport );
	device->GetTransform( D3DTS_WORLD, &worldMatrix );
	device->GetTransform( D3DTS_VIEW, &viewMatrix );
	device->GetTransform( D3DTS_PROJECTION, &projectionMatrix );

	const D3DXVECTOR3 point( world[0], world[1], world[2] );
	D3DXVECTOR3 projected;
	D3DXVec3Project( &projected, &point, &viewport,
		&projectionMatrix, &viewMatrix, &worldMatrix );

	screen[0] = projected.x;
	screen[1] = projected.y;
	screen[2] = projected.z;
	return projected.z < 0.0f || projected.z > 1.0f;
}

static qboolean VID_ScreenShot( const char *filename, int shot_type )
{
	if( !device || !filename || !filename[0] )
		return false;

	IDirect3DSurface9 *backBuffer = nullptr;
	if( FAILED( device->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer ) ) ||
		!backBuffer )
	{
		return false;
	}

	const HRESULT result = D3DXSaveSurfaceToFile(
		filename, D3DXIFF_PNG, backBuffer, nullptr, nullptr );
	backBuffer->Release( );
	return SUCCEEDED( result );
}

static void R_CubemapFaceName( const char *base,
	const char *suffix, char *out, size_t outSize )
{
	if( !out || outSize == 0 )
		return;
	out[0] = '\0';

	const char *safeBase = ( base && base[0] ) ? base : "gfx/env/env";
	const bool hasPath = strchr( safeBase, '/' ) != nullptr ||
		strchr( safeBase, '\\' ) != nullptr;

	char normalized[512];
	if( hasPath )
		snprintf( normalized, sizeof( normalized ), "%s", safeBase );
	else
		snprintf( normalized, sizeof( normalized ), "gfx/env/%s", safeBase );

	char *lastSlash = strrchr( normalized, '/' );
	char *backSlash = strrchr( normalized, '\\' );
	if( backSlash && ( !lastSlash || backSlash > lastSlash ) )
		lastSlash = backSlash;
	char *dot = strrchr( normalized, '.' );
	if( dot && ( !lastSlash || dot > lastSlash ) )
		*dot = '\0';

	snprintf( out, outSize, "%s%s.tga", normalized, suffix );
}

static qboolean VID_CubemapShot( const char *base, uint size,
	const float *vieworg, qboolean skyshot )
{
	if( !device )
	{
		gEngfuncs.Con_Printf( "D3D9 cubemap: no D3D device\n" );
		return false;
	}
	if( !base || !base[0] )
	{
		gEngfuncs.Con_Printf( "D3D9 cubemap: empty output basename\n" );
		return false;
	}
	if( !g_worldmodel )
	{
		gEngfuncs.Con_Printf( "D3D9 cubemap: no world model loaded\n" );
		return false;
	}

	vec3_t captureOrigin;
	if( vieworg )
		VectorCopy( vieworg, captureOrigin );
	else
		VectorCopy( g_vOrigin, captureOrigin );

	D3DCAPS9 caps;
	const HRESULT capsResult = device->GetDeviceCaps( &caps );
	if( FAILED( capsResult ) )
	{
		gEngfuncs.Con_Printf(
			"D3D9 cubemap: GetDeviceCaps failed (0x%08X)\n",
			static_cast<unsigned int>( capsResult ) );
		return false;
	}

	if( size == 0 )
		size = 256u;
	size = std::max<uint>( 16u, size );
	size = std::min<uint>( size,
		std::min<uint>( caps.MaxTextureWidth, caps.MaxTextureHeight ) );

	CreateDirectoryA( "gfx", nullptr );
	CreateDirectoryA( "gfx\\env", nullptr );

	gEngfuncs.Con_Printf(
		"D3D9 %s: capture %s at %.1f %.1f %.1f, %ux%u\n",
		skyshot ? "skyshot" : "envshot", base,
		captureOrigin[0], captureOrigin[1], captureOrigin[2], size, size );

	IDirect3DSurface9 *oldTarget = nullptr;
	IDirect3DSurface9 *oldDepth = nullptr;
	IDirect3DSurface9 *captureTarget = nullptr;
	IDirect3DSurface9 *captureDepth = nullptr;
	IDirect3DTexture9 *captureTexture = nullptr;
	IDirect3DStateBlock9 *savedStateBlock = nullptr;
	D3DVIEWPORT9 oldViewport{};
	D3DXMATRIX oldWorld{}, oldView{}, oldProjection{};
	D3DSURFACE_DESC depthDesc{};

	const D3DVIEWPORT9 savedSceneViewport = g_sceneViewport;
	const D3DXMATRIX savedSceneProjection = g_sceneProjection;
	const bool savedSceneViewportValid = g_sceneViewportValid;
	const unsigned int savedViewFlags = g_currentViewFlags;
	const dx_fog_state_t savedFog = g_dxFog;

	vec3_t savedOrigin, savedAngles, savedForward, savedRight, savedUp;
	vec3_t savedCullOrigin, savedCullForward, savedCullRight, savedCullUp;
	VectorCopy( g_vOrigin, savedOrigin );
	VectorCopy( g_vAngles, savedAngles );
	VectorCopy( g_vForward, savedForward );
	VectorCopy( g_vRight, savedRight );
	VectorCopy( g_vUp, savedUp );
	VectorCopy( g_dxCullOrigin, savedCullOrigin );
	VectorCopy( g_dxCullForward, savedCullForward );
	VectorCopy( g_dxCullRight, savedCullRight );
	VectorCopy( g_dxCullUp, savedCullUp );
	const float savedTanX = g_dxCullTanX;
	const float savedTanY = g_dxCullTanY;
	const float savedNear = g_dxCullNear;
	const float savedFar = g_dxCullFar;
	const bool savedCullValid = g_dxCullValid;

	bool ownedScene = false;
	bool success = false;

	do
	{

		HRESULT hr = device->CreateStateBlock(
			D3DSBT_ALL, &savedStateBlock );
		if( FAILED( hr ) || !savedStateBlock )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: CreateStateBlock failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->GetRenderTarget( 0, &oldTarget );
		if( FAILED( hr ) || !oldTarget )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: GetRenderTarget failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->GetDepthStencilSurface( &oldDepth );
		if( FAILED( hr ) || !oldDepth )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: GetDepthStencilSurface failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->GetViewport( &oldViewport );
		if( FAILED( hr ) )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: GetViewport failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->GetTransform( D3DTS_WORLD, &oldWorld );
		if( SUCCEEDED( hr ) ) hr = device->GetTransform( D3DTS_VIEW, &oldView );
		if( SUCCEEDED( hr ) ) hr = device->GetTransform( D3DTS_PROJECTION, &oldProjection );
		if( FAILED( hr ) )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: GetTransform failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = oldDepth->GetDesc( &depthDesc );
		if( FAILED( hr ) )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: depth GetDesc failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->CreateTexture( size, size, 1,
			D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
			&captureTexture, nullptr );
		if( FAILED( hr ) || !captureTexture )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: CreateTexture %ux%u failed (0x%08X)\n",
				size, size, static_cast<unsigned int>( hr ) );
			break;
		}

		hr = captureTexture->GetSurfaceLevel( 0, &captureTarget );
		if( FAILED( hr ) || !captureTarget )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: GetSurfaceLevel failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		hr = device->CreateDepthStencilSurface(
			size, size, depthDesc.Format, D3DMULTISAMPLE_NONE, 0, TRUE,
			&captureDepth, nullptr );
		if( FAILED( hr ) || !captureDepth )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: CreateDepthStencilSurface %ux%u fmt %u failed (0x%08X)\n",
				size, size, static_cast<unsigned int>( depthDesc.Format ),
				static_cast<unsigned int>( hr ) );
			break;
		}

		if( !g_dxSceneActive )
		{
			hr = device->BeginScene( );
			if( FAILED( hr ) )
			{
				gEngfuncs.Con_Printf(
					"D3D9 cubemap: BeginScene failed (0x%08X)\n",
					static_cast<unsigned int>( hr ) );
				break;
			}
			g_dxSceneActive = true;
			ownedScene = true;
		}

		hr = device->SetRenderTarget( 0, captureTarget );
		if( SUCCEEDED( hr ) )
			hr = device->SetDepthStencilSurface( captureDepth );
		if( FAILED( hr ) )
		{
			gEngfuncs.Con_Printf(
				"D3D9 cubemap: target/depth bind failed (0x%08X)\n",
				static_cast<unsigned int>( hr ) );
			break;
		}

		static const char *suffixes[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
		static const float faceAngles[6][3] =
		{
			{   0.0f,  -90.0f, 0.0f },
			{   0.0f,  180.0f, 0.0f },
			{   0.0f,   90.0f, 0.0f },
			{   0.0f,    0.0f, 0.0f },
			{ -90.0f,    0.0f, 0.0f },
			{  90.0f,    0.0f, 0.0f }
		};

		success = true;
		for( int face = 0; face < 6; ++face )
		{
			hr = device->Clear( 0, nullptr,
				D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
				D3DCOLOR_XRGB( 0, 0, 0 ), 1.0f, 0 );
			if( FAILED( hr ) )
			{
				gEngfuncs.Con_Printf(
					"D3D9 cubemap: Clear face %d failed (0x%08X)\n",
					face, static_cast<unsigned int>( hr ) );
				success = false;
				break;
			}

			ref_viewpass_t rvp{};
			rvp.flags = RF_DRAW_WORLD | RF_DRAW_CUBEMAP;
			rvp.viewentity = 0;
			rvp.viewport[0] = 0;
			rvp.viewport[1] = 0;
			rvp.viewport[2] = static_cast<int>( size );
			rvp.viewport[3] = static_cast<int>( size );
			rvp.fov_x = rvp.fov_y = 90.0f;
			VectorCopy( captureOrigin, rvp.vieworigin );
			VectorCopy( faceAngles[face], rvp.viewangles );

			const float savedFrameTime = g_frametime;
			g_frametime = 0.0f;
			DX_RenderFrame( &rvp );
			g_frametime = savedFrameTime;

			char filename[640];
			R_CubemapFaceName(
				base, suffixes[face], filename, sizeof( filename ) );
			const HRESULT saveResult = D3DXSaveSurfaceToFile(
				filename, D3DXIFF_TGA, captureTarget, nullptr, nullptr );
			if( FAILED( saveResult ) )
			{
				gEngfuncs.Con_Printf(
					"D3D9 cubemap: failed to save %s (0x%08X)\n",
					filename, static_cast<unsigned int>( saveResult ) );
				success = false;
				break;
			}
		}
	} while( false );

	if( oldTarget ) device->SetRenderTarget( 0, oldTarget );
	if( oldDepth ) device->SetDepthStencilSurface( oldDepth );

	if( savedStateBlock )
		savedStateBlock->Apply( );

	if( oldTarget )
	{
		device->SetViewport( &oldViewport );
		device->SetTransform( D3DTS_WORLD, &oldWorld );
		device->SetTransform( D3DTS_VIEW, &oldView );
		device->SetTransform( D3DTS_PROJECTION, &oldProjection );
	}

	g_sceneViewport = savedSceneViewport;
	g_sceneProjection = savedSceneProjection;
	g_sceneViewportValid = savedSceneViewportValid;
	g_currentViewFlags = savedViewFlags;
	g_dxFog = savedFog;
	VectorCopy( savedOrigin, g_vOrigin );
	VectorCopy( savedAngles, g_vAngles );
	VectorCopy( savedForward, g_vForward );
	VectorCopy( savedRight, g_vRight );
	VectorCopy( savedUp, g_vUp );
	VectorCopy( savedCullOrigin, g_dxCullOrigin );
	VectorCopy( savedCullForward, g_dxCullForward );
	VectorCopy( savedCullRight, g_dxCullRight );
	VectorCopy( savedCullUp, g_dxCullUp );
	g_dxCullTanX = savedTanX;
	g_dxCullTanY = savedTanY;
	g_dxCullNear = savedNear;
	g_dxCullFar = savedFar;
	g_dxCullValid = savedCullValid;

	if( ownedScene )
	{
		device->EndScene( );
		g_dxSceneActive = false;
	}

	if( savedStateBlock ) savedStateBlock->Release( );
	if( captureDepth ) captureDepth->Release( );
	if( captureTarget ) captureTarget->Release( );
	if( captureTexture ) captureTexture->Release( );
	if( oldDepth ) oldDepth->Release( );
	if( oldTarget ) oldTarget->Release( );

	DX_InvalidateD3DStateCache( );
	DX_InvalidateTextureBindCache( );
	DX_ApplyFog( );

	if( success )
	{
		gEngfuncs.Con_Printf(
			"D3D9 %s: wrote six %ux%u TGA faces for %s\n",
			skyshot ? "skyshot" : "envshot", size, size, base );
	}
	return success ? true : false;
}

static colorVec R_LightPoint( const float *p )
{
	colorVec result = { 0, 0, 0, 0 };
	if( !p )
		return result;

	vec3_t end;
	VectorCopy( p, end );
	end[2] -= 8192.0f;
	const D3DCOLOR packed =
		R_LightColorAlongSegment( p, end, nullptr, nullptr );
	result.r = ( packed >> 16 ) & 0xFF;
	result.g = ( packed >> 8 ) & 0xFF;
	result.b = packed & 0xFF;
	result.a = 0;
	return result;
}

static void R_DecalRemoveAll( int textureIndex )
{

	R_RemoveDecalsByTexture( textureIndex );
}

static float R_StudioEstimateFrame( cl_entity_t *e, mstudioseqdesc_t *pseqdesc, double time )
{
	if( !e || !pseqdesc || pseqdesc->numframes <= 1 )
		return 0.0f;

	float frame = e->curstate.frame * ( pseqdesc->numframes - 1 ) / 256.0f;
	if( time > e->curstate.animtime )
		frame += static_cast<float>( time - e->curstate.animtime ) *
		e->curstate.framerate * pseqdesc->fps;

	const float lastFrame = static_cast<float>( pseqdesc->numframes - 1 );
	if( pseqdesc->flags & STUDIO_LOOPING )
	{
		frame = fmodf( frame, lastFrame );
		if( frame < 0.0f ) frame += lastFrame;
	}
	else
	{
		frame = std::max( 0.0f, std::min( lastFrame - 0.001f, frame ) );
	}
	return frame;
}

static void R_StudioLerpMovement( cl_entity_t *e, double time, vec3_t origin, vec3_t angles )
{
	if( !origin || !angles )
		return;

	if( !e )
	{
		VectorClear( origin );
		VectorClear( angles );
		return;
	}

	VectorCopy( e->origin, origin );
	VectorCopy( e->angles, angles );

	const float snapshotSpan = e->curstate.animtime - e->latched.prevanimtime;
	if( snapshotSpan <= 0.0001f || ( e->curstate.effects & EF_NOINTERP ) )
		return;

	float t = static_cast<float>( ( time - e->latched.prevanimtime ) / snapshotSpan );
	t = std::max( 0.0f, std::min( 1.0f, t ) );

	for( int axis = 0; axis < 3; ++axis )
	{
		origin[axis] = e->latched.prevorigin[axis] +
			( e->origin[axis] - e->latched.prevorigin[axis] ) * t;

		float delta = e->angles[axis] - e->latched.prevangles[axis];
		while( delta > 180.0f ) delta -= 360.0f;
		while( delta < -180.0f ) delta += 360.0f;
		angles[axis] = e->latched.prevangles[axis] + delta * t;
	}
}

static void R_SetSkyCloudsTextures( int solidskyTexture, int )
{

	bool hasSkybox = false;
	for( int texture : g_skyTextures )
		hasSkybox |= texture > 0;
	if( !hasSkybox && solidskyTexture > 0 )
	{
		for( int &texture : g_skyTextures )
			texture = solidskyTexture;
	}
}

static void DX_SubdivideSurface( model_t *mod, msurface_t *fa )
{
	;
}

static void CL_RunLightStyles( lightstyle_t *ls )
{
	if( !ls )
		return;

	std::array<int, DX_LIGHTSTYLE_COUNT> values;
	const int tick = gp_cl ? static_cast<int>( gp_cl->time * 10.0f ) : 0;

	for( int i = 0; i < DX_LIGHTSTYLE_COUNT; ++i )
	{
		const char *pattern = ls[i].pattern;
		if( !pattern || !pattern[0] )
		{
			values[i] = 256;
			continue;
		}

		const size_t length = strlen( pattern );
		const char sample = pattern[static_cast<size_t>( tick ) % length];
		values[i] = std::max( 0, std::min( 1024, ( sample - 'a' ) * 22 ) );
	}

	R_SetLightStyles( values.data( ), DX_LIGHTSTYLE_COUNT );
}

static void R_SyncLightStylesFromEngine( void )
{
	lightstyle_t *styles = reinterpret_cast<lightstyle_t *>(
		ENGINE_GET_PARM( PARM_GET_LIGHTSTYLES_PTR ) );
	CL_RunLightStyles( styles );
}

static qboolean Mod_ProcessRenderData( model_t *mod, qboolean create, const byte *buffer, size_t buffersize )
{

	if( strstr( mod->name, ".bsp" ) )
	{

		R_SyncLightStylesFromEngine( );
		R_LoadMap( mod );
		return true;
	}

	if( strstr( mod->name, ".mdl" ) )
	{
		if( gEngfuncs.drawFuncs->Mod_ProcessUserData )
			gEngfuncs.drawFuncs->Mod_ProcessUserData( mod, true, buffer );

		R_LoadStudioModel( mod );
	}

	return true;
}

static void Mod_StudioLoadTextures( model_t *mod, void *data )
{
	studiohdr_t *phdr = static_cast<studiohdr_t *>( data );

	if( !phdr )
		return;

	mstudiotexture_t *ptexture = reinterpret_cast<mstudiotexture_t *>( reinterpret_cast<byte *>( phdr ) + phdr->textureindex );
	if( phdr->textureindex > 0 )
	{
		for( int i = 0; i < phdr->numtextures; i++ )
			R_StudioLoadTexture( mod, phdr, &ptexture[i] );
	}
}

static dxtexture_t *R_GetTexture( int texnum )
{
	if( texnum <= 0 || texnum >= 8096 )
		return NULL;

	return &g_Textures[texnum];
}

static intptr_t RefGetParm( int parm, int arg )
{
	if( parm == PARM_TEX_SKYBOX )
	{
		return ( arg >= 0 && arg < DX_SKYBOX_SIDES ) ? g_skyTextures[arg] : 0;
	}

	dxtexture_t *tex = R_GetTexture( arg );
	if( !tex )
		return 0;

	switch( parm )
	{
	case PARM_TEX_WIDTH:
		return tex->width;

	case PARM_TEX_HEIGHT:
		return tex->height;

	case PARM_TEX_SRC_WIDTH:
		return tex->srcWidth ? tex->srcWidth : tex->width;

	case PARM_TEX_SRC_HEIGHT:
		return tex->srcHeight ? tex->srcHeight : tex->height;

	case PARM_TEX_ENCODE:
		return tex->encode;

	case PARM_TEX_MIPCOUNT:
		return 0;

	case PARM_TEX_DEPTH:
		return 0;

	case PARM_TEX_FLAGS:
		return tex->flags;

	case PARM_TEX_TEXNUM:
		return tex->texnum;
	case PARM_TEX_FILTERING:
	{
		return !( tex->flags & TF_NEAREST );
	}
	}

	return 0;
}

//TODO: optimize texture index lookup, as we do 2 reads instead of 1

static void R_OverrideTextureSize( unsigned int texnum, unsigned int srcWidth, unsigned int srcHeight )
{
	if( texnum == 0 || texnum > static_cast<unsigned int>( g_texcount ) )
		return;

	dxtexture_t &texture = g_Textures[texnum];
	texture.srcWidth = static_cast<int>( srcWidth );
	texture.srcHeight = static_cast<int>( srcHeight );
}

struct sky_vertex_t
{
	float x, y, z;
	DWORD diffuse;
	float u, v;
};

#define D3DFVF_SKY_VERTEX ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )

static constexpr int DX_SKY_TEXTURE_ORDER[DX_SKYBOX_SIDES] = { 0, 2, 1, 3, 4, 5 };
static constexpr int DX_SKY_ST_TO_VEC[DX_SKYBOX_SIDES][3] =
{
	{  3, -1,  2 }, { -3,  1,  2 }, {  1,  3,  2 },
	{ -1, -3,  2 }, { -2, -1,  3 }, {  2, -1, -3 }
};

static sky_vertex_t R_MakeSkyVertex( float s, float t, int axis, float farClip )
{
	const float b[3] = { s * farClip * 0.5f, t * farClip * 0.5f, farClip * 0.5f };
	float position[3];
	for( int component = 0; component < 3; ++component )
	{
		const int map = DX_SKY_ST_TO_VEC[axis][component];
		const float value = b[abs( map ) - 1];
		position[component] = ( map < 0 ? -value : value ) + g_vOrigin[component];
	}

	sky_vertex_t vertex;
	vertex.x = position[0];
	vertex.y = position[1];
	vertex.z = position[2];
	vertex.diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );
	vertex.u = ( s + 1.0f ) * 0.5f;
	vertex.v = 1.0f - ( t + 1.0f ) * 0.5f;
	return vertex;
}

static void R_DrawSkyFace( int axis, float farClip )
{
	const int textureIndex = g_skyTextures[DX_SKY_TEXTURE_ORDER[axis]];
	if( textureIndex <= 0 )
		return;

	const sky_vertex_t vertices[4] =
	{
		R_MakeSkyVertex( -1.0f, -1.0f, axis, farClip ),
		R_MakeSkyVertex( -1.0f,  1.0f, axis, farClip ),
		R_MakeSkyVertex( 1.0f,  1.0f, axis, farClip ),
		R_MakeSkyVertex( 1.0f, -1.0f, axis, farClip )
	};

	DX_Bind( 0, textureIndex );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
	R_DrawScratch( D3DPT_TRIANGLEFAN, 2, 4,
		vertices, sizeof( sky_vertex_t ) );
}

static void R_DrawSkybox( void )
{
	bool hasSky = false;
	for( int texture : g_skyTextures )
		hasSky |= texture > 0;
	if( !hasSky || !device )
		return;

	const bool manageFog = DX_FogRequested( );
	const bool fogSkybox = g_dxFog.skybox;
	const bool internalFog =
		g_dxFog.source == DX_FOG_INTERNAL_WATER ||
		g_dxFog.source == DX_FOG_INTERNAL_QUAKE;
	if( manageFog && !fogSkybox )
		DX_PushFogOff( );
	if( manageFog && internalFog )
		DX_SetFogDensityScale( 0.5f );

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	device->SetTransform( D3DTS_WORLD, &identity );
	device->SetFVF( D3DFVF_SKY_VERTEX );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

	for( int axis = 0; axis < DX_SKYBOX_SIDES; ++axis )
		R_DrawSkyFace( axis, 8192.0f );

	if( manageFog && internalFog )
		DX_SetFogDensityScale( 1.0f );
	if( manageFog && !fogSkybox )
		DX_PopFogOff( );

	device->SetTransform( D3DTS_WORLD, &identity );
	device->SetFVF( BRUSH_FVF );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
}

static void R_DrawViewModel( void )
{

	if( ENGINE_GET_PARM( PARM_THIRDPERSON ) )
		return;

	cl_entity_s *viewModel = R_CurrentViewModel( );
	if( !viewModel || !viewModel->model ||
		viewModel->model->type != mod_studio ||
		( viewModel->curstate.effects & EF_NODRAW ) )
	{
		return;
	}

	const D3DVIEWPORT9 oldViewport = g_sceneViewport;
	if( !g_sceneViewportValid || oldViewport.Height == 0 )
		return;

	const float aspect = static_cast<float>( oldViewport.Width ) / oldViewport.Height;
	const float fov = std::max( 10.0f, std::min( 170.0f, r_viewmodel_fov.value ) );

	const float zNear = std::max( 0.01f, std::min( 16.0f, r_viewmodel_near.value ) );

	D3DXMATRIX projection;
	D3DXMatrixPerspectiveFovLH( &projection, D3DXToRadian( fov ), aspect,
		zNear, 8192.0f );
	device->SetTransform( D3DTS_PROJECTION, &projection );

	D3DVIEWPORT9 viewModelViewport = oldViewport;

	viewModelViewport.MinZ = 0.0f;
	viewModelViewport.MaxZ = 0.30f;
	device->SetViewport( &viewModelViewport );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );

	const float forwardOffset = std::max( -128.0f, std::min( 128.0f,
		r_viewmodel_offset_x.value ) );
	const float rightOffset = std::max( -128.0f, std::min( 128.0f,
		r_viewmodel_offset_y.value ) );
	const float upOffset = std::max( -128.0f, std::min( 128.0f,
		r_viewmodel_offset_z.value ) );

	R_DrawStudioViewModel( viewModel, g_vOrigin, g_vAngles,
		g_vForward, g_vRight, g_vUp,
		forwardOffset, rightOffset, upOffset );

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	device->SetViewport( &oldViewport );
	device->SetTransform( D3DTS_WORLD, &identity );
	device->SetTransform( D3DTS_PROJECTION, &g_sceneProjection );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
}

static bool R_SphereVisible( const float center[3], float radius )
{
	if( !g_dxCullValid || r_nocull.value != 0.0f )
		return true;

	const float rel[3] =
	{
		center[0] - g_dxCullOrigin[0],
		center[1] - g_dxCullOrigin[1],
		center[2] - g_dxCullOrigin[2]
	};
	const float depth = DotProduct( rel, g_dxCullForward );
	const float side = DotProduct( rel, g_dxCullRight );
	const float vertical = DotProduct( rel, g_dxCullUp );

	if( depth + radius < g_dxCullNear )
		return false;
	if( depth - radius > g_dxCullFar )
		return false;
	if( depth * g_dxCullTanX + side + radius < 0.0f )
		return false;
	if( depth * g_dxCullTanX - side + radius < 0.0f )
		return false;
	if( depth * g_dxCullTanY + vertical + radius < 0.0f )
		return false;
	if( depth * g_dxCullTanY - vertical + radius < 0.0f )
		return false;
	return true;
}

static bool R_EntityMightBeVisible( const cl_entity_s *ent )
{
	if( !ent || !ent->model || r_nocull.value != 0.0f )
		return true;

	float center[3] = { ent->origin[0], ent->origin[1], ent->origin[2] };
	float radius = 256.0f;
	if( ent->model )
	{
		const float cx = 0.5f * ( ent->model->mins[0] + ent->model->maxs[0] );
		const float cy = 0.5f * ( ent->model->mins[1] + ent->model->maxs[1] );
		const float cz = 0.5f * ( ent->model->mins[2] + ent->model->maxs[2] );
		const float ex = 0.5f * ( ent->model->maxs[0] - ent->model->mins[0] );
		const float ey = 0.5f * ( ent->model->maxs[1] - ent->model->mins[1] );
		const float ez = 0.5f * ( ent->model->maxs[2] - ent->model->mins[2] );
		const float modelScale =
			ent->model->type != mod_brush && ent->curstate.scale > 0.0f
			? ent->curstate.scale : 1.0f;

		center[0] += cx * modelScale;
		center[1] += cy * modelScale;
		center[2] += cz * modelScale;
		radius = sqrtf( ex * ex + ey * ey + ez * ez ) * modelScale;
		if( radius < 8.0f )
			radius = 8.0f;
	}

	return R_SphereVisible( center, radius + 128.0f );
}

static int R_TranslucentEntityRank( const cl_entity_s *entity )
{
	if( !entity )
		return 0;

	const int renderMode = entity->model && entity->model->type == mod_studio &&
		entity->curstate.rendermode == kRenderNormal
		? kRenderTransAdd
		: entity->curstate.rendermode;

	switch( renderMode )
	{
	case kRenderTransTexture: return 1;
	case kRenderTransAdd:     return 2;
	case kRenderGlow:         return 3;
	default:                  return 0;
	}
}

static float R_TranslucentDistSq( const cl_entity_s *entity,
	const vec3_t viewOrigin )
{
	if( !entity )
		return 0.0f;

	if( entity->model &&
		entity->model->type == mod_brush &&
		entity->curstate.rendermode == kRenderTransAlpha )
	{
		return 1000000000.0f;
	}

	float x = entity->origin[0];
	float y = entity->origin[1];
	float z = entity->origin[2];
	if( entity->model )
	{
		const float centerScale =
			entity->model->type != mod_brush && entity->curstate.scale > 0.0f
			? entity->curstate.scale : 1.0f;
		x += 0.5f * ( entity->model->mins[0] + entity->model->maxs[0] ) * centerScale;
		y += 0.5f * ( entity->model->mins[1] + entity->model->maxs[1] ) * centerScale;
		z += 0.5f * ( entity->model->mins[2] + entity->model->maxs[2] ) * centerScale;
	}

	const float dx = x - viewOrigin[0];
	const float dy = y - viewOrigin[1];
	const float dz = z - viewOrigin[2];
	return dx * dx + dy * dy + dz * dz;
}

static bool R_TranslucentLess( const translucent_entity_t &a,
	const translucent_entity_t &b )
{

	if( a.distanceSquared != b.distanceSquared )
		return a.distanceSquared > b.distanceSquared;
	if( a.rank != b.rank )
		return a.rank < b.rank;
	return a.submissionOrder < b.submissionOrder;
}

static void R_SetWorldIdentity( const D3DXMATRIX &identity, bool &isIdentity )
{
	if( isIdentity )
		return;
	device->SetTransform( D3DTS_WORLD, &identity );
	isIdentity = true;
}

static void R_MarkWorldTransformDirty( bool &isIdentity )
{
	isIdentity = false;
}

static void R_SetupSceneDLights( )
{
	R_BeginDLights( );
	R_CollectEngineDLights( );
	R_CollectMuzzleLights( );
	for( int i = 0; i < g_entityLightEmitterCount; ++i )
		R_CollectEntityDLight( g_entityLightEmitters[i] );
	R_CollectBeamLights( );
	R_ApplyDLights( );
	R_UpdateDLightOverlay( );
}

static void R_DrawOpaqueEntities( const D3DXMATRIX &identity, bool &worldIsIdentity )
{
	g_waterBrushEntities.clear( );
	g_lateWaterBrushEntities.clear( );
	g_waterBrushCount = 0;
	g_lateWaterBrushCount = 0;

	DX_PROFILE_SCOPE( opaqueEntityScope, DX_PROFILE_OPAQUE_ENTITIES );

	for( int i = 0; i < g_opaqueBrushCount; ++i )
	{
		cl_entity_s *ent = g_opaqueBrushEntities[i];
		if( !R_EntityMightBeVisible( ent ) )
			continue;

		const bool hasWater = R_DrawBrushEntity( ent );
		R_MarkWorldTransformDirty( worldIsIdentity );
		if( !hasWater )
			continue;

		if( R_WaterIsTranslucent( ent ) )
		{
			g_lateWaterBrushEntities.push_back( ent );
			++g_lateWaterBrushCount;
		}
		else
		{
			g_waterBrushEntities.push_back( ent );
			++g_waterBrushCount;
		}
	}

	for( int i = 0; i < g_opaqueStudioCount; ++i )
	{
		cl_entity_s *ent = g_opaqueStudioEntities[i];
		if( !R_EntityMightBeVisible( ent ) )
			continue;
		R_DrawStudioEntity( ent );
		R_MarkWorldTransformDirty( worldIsIdentity );
	}

	R_SetWorldIdentity( identity, worldIsIdentity );
	for( int i = 0; i < g_opaqueSpriteCount; ++i )
	{
		cl_entity_s *ent = g_opaqueSpriteEntities[i];
		if( !R_EntityMightBeVisible( ent ) )
			continue;
		if( !R_QueueSprite( ent, ent->model ) )
			R_DrawSpriteImmediate( ent );
	}
}

static int R_BuildTranslucentList( const vec3_t viewOrigin )
{
	DX_PROFILE_SCOPE( classifySortScope, DX_PROFILE_ENTITY_CLASSIFY );
	g_translucentDrawList.clear( );

	for( int i = 0; i < g_translucentCandidateCount; ++i )
	{
		cl_entity_s *ent = g_translucentCandidates[i];
		if( !ent || !ent->model || !R_EntityMightBeVisible( ent ) )
			continue;

		translucent_entity_t item;
		item.entity = ent;
		item.rank = R_TranslucentEntityRank( ent );
		item.distanceSquared = R_TranslucentDistSq( ent, viewOrigin );
		item.submissionOrder = i;
		g_translucentDrawList.push_back( item );
	}

	if( g_translucentDrawList.size( ) > 1 )
	{
		std::stable_sort( g_translucentDrawList.begin( ),
			g_translucentDrawList.end( ), R_TranslucentLess );
	}
	return static_cast<int>( g_translucentDrawList.size( ) );
}

static void R_DrawWaterDepthPass( const D3DXMATRIX &identity, bool &worldIsIdentity,
	bool worldHasWater, bool worldWaterTranslucent )
{
	DX_PROFILE_SCOPE( waterDepthScope, DX_PROFILE_WATER_DEPTH );
	R_SetWorldIdentity( identity, worldIsIdentity );

	if( worldHasWater && !worldWaterTranslucent )
		R_DrawWorldWaterDepth( g_worldmodel );

	for( int i = 0; i < g_waterBrushCount; ++i )
	{
		R_DrawBrushWaterDepth( g_waterBrushEntities[i] );
		R_MarkWorldTransformDirty( worldIsIdentity );
	}
}

static void R_DrawWaterColorPass( const D3DXMATRIX &identity, bool &worldIsIdentity,
	bool worldHasWater, bool worldWaterTranslucent )
{
	DX_PROFILE_SCOPE( waterColorScope, DX_PROFILE_WATER_COLOR );
	R_SetWorldIdentity( identity, worldIsIdentity );

	if( worldHasWater && !worldWaterTranslucent )
		R_DrawWorldWater( g_worldmodel );

	for( int i = 0; i < g_waterBrushCount; ++i )
	{
		R_DrawBrushWater( g_waterBrushEntities[i] );
		R_MarkWorldTransformDirty( worldIsIdentity );
	}
}

static void R_DrawTranslucentEntities( int count, const D3DXMATRIX &identity,
	bool &worldIsIdentity )
{
	DX_PROFILE_SCOPE( translucentScope, DX_PROFILE_TRANSLUCENT_ENTITIES );

	for( int i = 0; i < count; ++i )
	{
		cl_entity_s *ent = g_translucentDrawList[i].entity;
		if( !ent || !ent->model )
			continue;

		switch( ent->model->type )
		{
		case mod_sprite:
			R_SetWorldIdentity( identity, worldIsIdentity );
			if( !R_QueueSprite( ent, ent->model ) )
				R_DrawSpriteImmediate( ent );
			break;

		case mod_brush:
		{
			DX_PROFILE_SCOPE( spriteFlushScope, DX_PROFILE_SPRITE_FLUSH );
			R_SetWorldIdentity( identity, worldIsIdentity );
			R_FlushSprites( );
		}
		if( R_DrawBrushEntity( ent ) )
		{
			R_MarkWorldTransformDirty( worldIsIdentity );
			R_DrawBrushWater( ent );
		}
		R_MarkWorldTransformDirty( worldIsIdentity );
		break;

		case mod_studio:
		{
			DX_PROFILE_SCOPE( spriteFlushScope, DX_PROFILE_SPRITE_FLUSH );
			R_SetWorldIdentity( identity, worldIsIdentity );
			R_FlushSprites( );
		}
		R_DrawStudioEntity( ent );
		R_MarkWorldTransformDirty( worldIsIdentity );
		break;

		default:
			break;
		}
	}
}

static void R_DrawLateWaterPass( const D3DXMATRIX &identity, bool &worldIsIdentity,
	bool worldWaterTranslucent )
{
	if( !worldWaterTranslucent && g_lateWaterBrushCount == 0 )
		return;

	DX_PROFILE_SCOPE( waterColorScope, DX_PROFILE_WATER_COLOR );
	R_SetWorldIdentity( identity, worldIsIdentity );

	if( worldWaterTranslucent )
		R_DrawWorldWater( g_worldmodel );

	for( int i = 0; i < g_lateWaterBrushCount; ++i )
	{
		R_DrawBrushWater( g_lateWaterBrushEntities[i] );
		R_MarkWorldTransformDirty( worldIsIdentity );
	}
	R_SetWorldIdentity( identity, worldIsIdentity );
}

void R_DrawEnts( const vec3_t view_origin )
{
	if( !device )
		return;

	DX_PROFILE_SCOPE( drawEntsScope, DX_PROFILE_DRAWENTS );

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	bool worldIsIdentity = false;

	device->SetFVF( BRUSH_FVF );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	R_SetWorldIdentity( identity, worldIsIdentity );

	R_SetupSceneDLights( );

	{
		DX_PROFILE_SCOPE( skyScope, DX_PROFILE_SKY );
		R_DrawSkybox( );
	}
	R_MarkWorldTransformDirty( worldIsIdentity );
	R_SetWorldIdentity( identity, worldIsIdentity );

	bool worldHasWater = false;
	bool worldWaterTranslucent = false;
	if( g_worldmodel )
	{
		DX_PROFILE_SCOPE( worldScope, DX_PROFILE_STATIC_WORLD );
		worldHasWater = R_DrawWorldBrushes( g_worldmodel );
		worldWaterTranslucent = worldHasWater && R_WaterIsTranslucent( nullptr );
	}

	{
		DX_PROFILE_SCOPE( beamScope, DX_PROFILE_BEAMS );
		R_DrawBeamEntities( false );
	}
	R_MarkWorldTransformDirty( worldIsIdentity );
	R_SetWorldIdentity( identity, worldIsIdentity );

	R_DrawOpaqueEntities( identity, worldIsIdentity );
	{
		DX_PROFILE_SCOPE( spriteScope, DX_PROFILE_SPRITE_FLUSH );
		R_SetWorldIdentity( identity, worldIsIdentity );
		R_FlushSprites( );
	}

	if( dx_client_efx.value != 0.0f && gEngfuncs.CL_DrawEFX )
		gEngfuncs.CL_DrawEFX( std::max( 0.0f, g_frametime ), false );
	if( gEngfuncs.pfnDrawNormalTriangles )
		gEngfuncs.pfnDrawNormalTriangles( );
	R_MarkWorldTransformDirty( worldIsIdentity );

	const int translucentCount = R_BuildTranslucentList( view_origin );
	R_DrawWaterDepthPass( identity, worldIsIdentity, worldHasWater, worldWaterTranslucent );
	R_DrawWaterColorPass( identity, worldIsIdentity, worldHasWater, worldWaterTranslucent );
	R_SetWorldIdentity( identity, worldIsIdentity );
	R_DrawTranslucentEntities( translucentCount, identity, worldIsIdentity );

	{
		DX_PROFILE_SCOPE( spriteScope, DX_PROFILE_SPRITE_FLUSH );
		R_SetWorldIdentity( identity, worldIsIdentity );
		R_FlushSprites( );
	}

	if( gEngfuncs.pfnDrawTransparentTriangles )
	{
		gEngfuncs.pfnDrawTransparentTriangles( );
		R_MarkWorldTransformDirty( worldIsIdentity );
		R_SetWorldIdentity( identity, worldIsIdentity );
	}

	R_DrawLateWaterPass( identity, worldIsIdentity, worldWaterTranslucent );

	{
		DX_PROFILE_SCOPE( beamScope, DX_PROFILE_BEAMS );
		R_DrawBeamEntities( true );
	}

	if( dx_client_efx.value != 0.0f && gEngfuncs.CL_DrawEFX )
	{
		DX_PushFogOff( );
		gEngfuncs.CL_DrawEFX( std::max( 0.0f, g_frametime ), true );
		DX_PopFogOff( );
	}

	{
		DX_PROFILE_SCOPE( spriteScope, DX_PROFILE_SPRITE_FLUSH );
		R_MarkWorldTransformDirty( worldIsIdentity );
		R_SetWorldIdentity( identity, worldIsIdentity );
		R_FlushSprites( );
	}

	if( !FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP | RF_DRAW_OVERVIEW ) )
	{
		DX_PROFILE_SCOPE( viewModelScope, DX_PROFILE_VIEWMODEL );
		R_DrawViewModel( );
	}
}

static void DX_RenderFrame( const struct ref_viewpass_s *rvp )
{
	if( !device || !rvp )
		return;

	g_currentViewFlags = rvp->flags;
	g_dxOverviewView =
		FBitSet( g_currentViewFlags, RF_DRAW_OVERVIEW ) != 0;

	const HRESULT cooperative = device->TestCooperativeLevel( );
	if( cooperative == D3DERR_DEVICELOST )
		return;
	if( cooperative == D3DERR_DEVICENOTRESET )
	{
		DX_ResetDevice( true );
		return;
	}

	if( DX_PresentChanged( ) )
	{
		DX_ResetDevice( true );
		return;
	}

	DX_UpdateTextureQuality( false );

	{
		DX_PROFILE_SCOPE( sceneSetupScope, DX_PROFILE_SCENE_SETUP );
		D3DVIEWPORT9 viewport{};
		viewport.X = rvp->viewport[0];
		viewport.Y = rvp->viewport[1];
		viewport.Width = rvp->viewport[2];
		viewport.Height = rvp->viewport[3];
		viewport.MinZ = 0.0f;
		viewport.MaxZ = 1.0f;
		device->SetViewport( &viewport );
		g_sceneViewport = viewport;
		g_sceneViewportValid = true;

		AngleVectors( rvp->viewangles, g_vForward, g_vRight, g_vUp );
		VectorCopy( rvp->vieworigin, g_vOrigin );
		VectorCopy( rvp->viewangles, g_vAngles );

		const D3DXVECTOR3 eye( rvp->vieworigin[0], rvp->vieworigin[1], rvp->vieworigin[2] );
		const D3DXVECTOR3 at( rvp->vieworigin[0] + g_vForward[0],
			rvp->vieworigin[1] + g_vForward[1],
			rvp->vieworigin[2] + g_vForward[2] );
		const D3DXVECTOR3 up( g_vUp[0], g_vUp[1], g_vUp[2] );

		D3DXMATRIX view, coordFix;
		D3DXMatrixLookAtRH( &view, &eye, &at, &up );
		D3DXMatrixIdentity( &coordFix );
		coordFix._33 = -1.0f;
		view = view * coordFix;
		device->SetTransform( D3DTS_VIEW, &view );

		const float aspect = viewport.Height
			? static_cast<float>( viewport.Width ) / viewport.Height : 1.0f;

		if( !g_dxOverviewView && r_nocull.value == 0.0f )
		{
			VectorCopy( g_vOrigin, g_dxCullOrigin );
			VectorCopy( g_vForward, g_dxCullForward );
			VectorCopy( g_vRight, g_dxCullRight );
			VectorCopy( g_vUp, g_dxCullUp );
			g_dxCullTanY = tanf( D3DXToRadian( rvp->fov_y ) * 0.5f );
			g_dxCullTanX = g_dxCullTanY * aspect;
			g_dxCullNear = 4.0f;
			g_dxCullFar = 8192.0f;
			g_dxCullValid = true;
		}
		else
		{
			g_dxCullValid = false;
		}

		D3DXMATRIX projection;
		bool haveProjection = false;

		if( g_dxOverviewView && gEngfuncs.GetOverviewParms )
		{
			const struct ref_overview_s *ov = gEngfuncs.GetOverviewParms( );
			if( ov &&
				fabsf( ov->xRight - ov->xLeft ) > 0.001f &&
				fabsf( ov->yBottom - ov->yTop ) > 0.001f &&
				fabsf( ov->zFar - ov->zNear ) > 0.001f )
			{

				D3DXMatrixOrthoOffCenterLH( &projection,
					ov->xLeft, ov->xRight,
					ov->yTop, ov->yBottom,
					ov->zNear, ov->zFar );
				haveProjection = true;
			}
		}

		if( g_dxOverviewView && !haveProjection &&
			g_dxOrthoValid )
		{

			const float width = std::max( 1.0f,
				g_dxOrthoMaxs[0] - g_dxOrthoMins[0] );
			const float height = std::max( 1.0f,
				g_dxOrthoMaxs[1] - g_dxOrthoMins[1] );
			D3DXMatrixOrthoLH( &projection, width, height, 4.0f, 16384.0f );
			haveProjection = true;
		}

		if( !haveProjection )
		{
			D3DXMatrixPerspectiveFovLH( &projection,
				D3DXToRadian( rvp->fov_y ),
				aspect, 4.0f, 8192.0f );
		}

		device->SetTransform( D3DTS_PROJECTION, &projection );
		g_sceneProjection = projection;
	}

	DX_UpdateFog( rvp );
	R_DrawEnts( rvp->vieworigin );
}

static void DX_OrthoBounds( const float *mins, const float *maxs )
{
	if( !mins || !maxs )
	{
		g_dxOrthoValid = false;
		return;
	}

	g_dxOrthoMins[0] = mins[0];
	g_dxOrthoMins[1] = mins[1];
	g_dxOrthoMaxs[0] = maxs[0];
	g_dxOrthoMaxs[1] = maxs[1];

	g_dxOrthoCenter[0] = ( mins[0] + maxs[0] ) * 0.5f;
	g_dxOrthoCenter[1] = ( mins[1] + maxs[1] ) * 0.5f;
	g_dxOrthoHalf[0] = fabsf( maxs[0] - mins[0] ) * 0.5f;
	g_dxOrthoHalf[1] = fabsf( maxs[1] - mins[1] ) * 0.5f;

	g_dxOrthoValid =
		g_dxOrthoHalf[0] > 0.001f &&
		g_dxOrthoHalf[1] > 0.001f;

	g_dxOverviewView = g_dxOrthoValid;
}

static qboolean R_SpeedsMessage( char *out, size_t size )
{
	if( !out || size == 0 )
		return false;

	if( !g_dxRSpeedsCvar && gEngfuncs.pfnGetCvarPointer )
		g_dxRSpeedsCvar = gEngfuncs.pfnGetCvarPointer( "r_speeds" );

	if( !g_dxRSpeedsCvar || g_dxRSpeedsCvar->value <= 0.0f )
		return false;

	const int mode = static_cast<int>( g_dxRSpeedsCvar->value );
	const dx_speeds_frame_t &speeds = DX_ProfileSpeeds( );

	switch( mode )
	{
	case 1:
		snprintf( out, size,
			"D3D9: %3u draw calls\n%3u indexed, %6u primitives",
			speeds.drawCalls,
			speeds.indexedDrawCalls,
			speeds.primitiveCount );
		break;

	case 2:
	{
		int leafCount = 0;
		int visibleLeafs = 0;
		int rowBytes = 0;
		byte *vis = R_GetWorldVis( );

		if( g_worldmodel && g_worldmodel->type == mod_brush &&
			g_worldmodel->numleafs > 0 )
		{
			leafCount = g_worldmodel->numleafs;
			rowBytes = ( leafCount + 7 ) >> 3;

			if( vis )
			{
				for( int leafBit = 0; leafBit < leafCount; ++leafBit )
					if( vis[leafBit >> 3] & ( 1 << ( leafBit & 7 ) ) )
						++visibleLeafs;
			}
		}

		snprintf( out, size,
			"Renderer: ^2D3D9^7\n\nvisible leafs:\n%3i / %3i\nPVS bytes: %3i",
			visibleLeafs, leafCount, rowBytes );
		break;
	}

	case 3:
	{
		int brushModels = g_opaqueBrushCount;
		int studioModels = g_opaqueStudioCount;
		int sprites = g_opaqueSpriteCount;

		for( int i = 0; i < g_translucentCandidateCount; ++i )
		{
			const cl_entity_s *ent = g_translucentCandidates[i];
			if( !ent || !ent->model )
				continue;

			switch( ent->model->type )
			{
			case mod_brush:  ++brushModels; break;
			case mod_studio: ++studioModels; break;
			case mod_sprite: ++sprites; break;
			default: break;
			}
		}

		snprintf( out, size,
			"%3i brush models\n%3i studio models\n%3i sprites",
			brushModels, studioModels, sprites );
		break;
	}

	case 4:
	{
		const int submitted =
			g_opaqueBrushCount +
			g_opaqueStudioCount +
			g_opaqueSpriteCount +
			g_translucentCandidateCount;

		snprintf( out, size,
			"%3i render entities\n%3i translucent\n%3i server entities",
			submitted,
			g_translucentCandidateCount,
			static_cast<int>( ENGINE_GET_PARM( PARM_NUMENTITIES ) ) );
		break;
	}

	case 5:
		snprintf( out, size,
			"%3i beam entities\n%3i light emitters\n%3i water brushes",
			g_beamEntityCount,
			g_entityLightEmitterCount,
			g_waterBrushCount + g_lateWaterBrushCount );
		break;

	default:
		snprintf( out, size,
			"D3D9: %3u draw calls\n%6u primitives",
			speeds.drawCalls,
			speeds.primitiveCount );
		break;
	}

	out[size - 1] = '\0';
	return true;
}

static byte *Mod_GetCurrentVis( void )
{

	return R_GetWorldVis( );
}

static void Color4f( float r, float g, float b, float a )
{

	const auto toByte = []( float value ) -> byte
		{
			value = std::max( 0.0f, std::min( 1.0f, value ) );
			return static_cast<byte>( value * 255.0f + 0.5f );
		};
	g_Color = D3DCOLOR_ARGB(
		toByte( a ), toByte( r ), toByte( g ), toByte( b ) );

	R_TriColor4f( r, g, b, a );
}

static void Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{

	g_Color = D3DCOLOR_ARGB( a, r, g, b );

	R_TriColor4ub( r, g, b, a );
}

static void R_Apply2DBlendMode( int mode )
{
	if( !device )
		return;

	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );

	switch( mode )
	{
	default:
	case kRenderNormal:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		break;

	case kRenderTransColor:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR );
		break;

	case kRenderTransTexture:
	case kRenderTransAlpha:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		break;

	case kRenderGlow:
	case kRenderTransAdd:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		break;
	}
}

static void R_Reset2DBatch( void )
{
	g_2dBatchVertices.clear( );
	g_2dBatchActive = false;
	g_2dBatchTexture = 0;
	g_2dBatchRenderMode = kRenderNormal;
}

static void R_Flush2DBatch( void )
{
	if( !g_2dBatchActive || g_2dBatchVertices.empty( ) )
	{
		R_Reset2DBatch( );
		return;
	}

	DX_PROFILE_SCOPE( profile2dBatchScope, DX_PROFILE_2D );
	if( device )
	{

		if( !g_2dModeActive )
		{
			g_2dModeActive = true;
		}
		R_Apply2DNeutralState( );
		R_Apply2DBlendMode( g_2dBatchRenderMode );
		DX_Bind( 0, g_2dBatchTexture );
		device->SetFVF( D3DFVF_VERTEX2D );

		const UINT vertexCount = static_cast<UINT>( g_2dBatchVertices.size( ) );
		const UINT primitiveCount = vertexCount / 3u;
		if( primitiveCount > 0 &&
			R_DrawScratch( D3DPT_TRIANGLELIST, primitiveCount,
				vertexCount, g_2dBatchVertices.data( ), sizeof( vertex2d_t ) ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_2D_BATCH_DRAWN, 1 );
		}
	}

	R_Reset2DBatch( );
}

static void R_Queue2DTexturedQuad( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, int texnum, int renderMode,
	D3DCOLOR color )
{
	if( !device || w <= 0.0f || h <= 0.0f )
		return;

	const size_t quadCount = g_2dBatchVertices.size( ) / 6u;
	if( g_2dBatchActive &&
		( g_2dBatchTexture != texnum ||
			g_2dBatchRenderMode != renderMode ||
			quadCount >= DX_2D_BATCH_MAX_QUADS ) )
	{
		R_Flush2DBatch( );
	}

	if( !g_2dBatchActive )
	{
		g_2dBatchActive = true;
		g_2dBatchTexture = texnum;
		g_2dBatchRenderMode = renderMode;
		if( g_2dBatchVertices.capacity( ) < 6u * DX_2D_BATCH_MAX_QUADS )
			g_2dBatchVertices.reserve( 6u * DX_2D_BATCH_MAX_QUADS );
	}

	x -= 0.5f;
	y -= 0.5f;
	const vertex2d_t tl = { x,     y,     0.0f, 1.0f, color, s1, t1 };
	const vertex2d_t tr = { x + w, y,     0.0f, 1.0f, color, s2, t1 };
	const vertex2d_t bl = { x,     y + h, 0.0f, 1.0f, color, s1, t2 };
	const vertex2d_t br = { x + w, y + h, 0.0f, 1.0f, color, s2, t2 };

	g_2dBatchVertices.push_back( tl );
	g_2dBatchVertices.push_back( tr );
	g_2dBatchVertices.push_back( bl );
	g_2dBatchVertices.push_back( bl );
	g_2dBatchVertices.push_back( tr );
	g_2dBatchVertices.push_back( br );
	DX_ProfileEvent( DX_PROFILE_EVENT_2D_QUAD_QUEUED, 1 );
}

static void VGUI_SetupDrawing( qboolean rect )
{

	R_Set2DMode( true );
	R_Apply2DBlendMode( g_2dRenderMode );
}

static void R_DrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, int texnum )
{
	if( !device || w <= 0.0f || h <= 0.0f )
		return;

	R_Set2DMode( true );
	R_Queue2DTexturedQuad( x, y, w, h, s1, t1, s2, t2,
		texnum, g_2dRenderMode, g_Color );
}

static void R_DrawRawTexture2D( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, IDirect3DBaseTexture9 *texture )
{
	if( !device || !texture || w <= 0.0f || h <= 0.0f )
		return;

	R_Flush2DBatch( );
	DX_PROFILE_SCOPE( profile2dRawScope, DX_PROFILE_2D );
	R_Set2DMode( true );
	R_Apply2DBlendMode( g_2dRenderMode );
	DX_BindRawTexture( 0, texture );

	DX_SetSamplerStateCached( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
	DX_SetSamplerStateCached( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
	DX_SetSamplerStateCached( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

	vertex2d_t v[4];
	x -= 0.5f;
	y -= 0.5f;
	const DWORD color = g_Color;
	v[0] = { x,     y,     0.0f, 1.0f, color, s1, t1 };
	v[1] = { x + w, y,     0.0f, 1.0f, color, s2, t1 };
	v[2] = { x,     y + h, 0.0f, 1.0f, color, s1, t2 };
	v[3] = { x + w, y + h, 0.0f, 1.0f, color, s2, t2 };
	device->SetFVF( D3DFVF_VERTEX2D );
	R_DrawScratch( D3DPT_TRIANGLESTRIP, 2, 4,
		v, sizeof( vertex2d_t ) );
}

struct show_texture_entry_t
{
	enum kind_t
	{
		DX_SHOW_REGULAR,
		DX_SHOW_LIGHTMAP
	} kind = DX_SHOW_REGULAR;

	int index = 0;
	IDirect3DTexture9 *rawTexture = nullptr;
	int width = 0;
	int height = 0;
	bool dynamic = false;
	bool dynamicOverlay = false;
	const char *materialName = nullptr;
	int allocationCount = 0;
	float occupancy = 0.0f;
};

static bool R_TextureKeyDown( int virtualKey )
{

	if( !hwnd || GetForegroundWindow( ) != hwnd )
		return false;
	return ( GetAsyncKeyState( virtualKey ) & 0x8000 ) != 0;
}

static void R_UpdateTextureZoom( bool active )
{
	static bool wasZoomInDown = false;
	static bool wasZoomOutDown = false;

	const bool zoomInDown =
		R_TextureKeyDown( 'Z' ) ||
		R_TextureKeyDown( VK_UP ) ||
		R_TextureKeyDown( VK_ADD );

	const bool zoomOutDown =
		R_TextureKeyDown( 'S' ) ||
		R_TextureKeyDown( VK_DOWN ) ||
		R_TextureKeyDown( VK_SUBTRACT );

	if( active )
	{
		if( zoomInDown && !wasZoomInDown )
			R_ShowTexturesSetZoom( dx_showtextures_zoom.value + 0.25f );
		if( zoomOutDown && !wasZoomOutDown )
			R_ShowTexturesSetZoom( dx_showtextures_zoom.value - 0.25f );
	}

	wasZoomInDown = zoomInDown;
	wasZoomOutDown = zoomOutDown;
}

static void R_ShowTextures( void )
{
	static cvar_t *showTextures = nullptr;
	static qboolean showHelp = true;
	static std::vector<show_texture_entry_t> entries;

	if( !device || !gpGlobals )
		return;

	if( !showTextures && gEngfuncs.pfnGetCvarPointer )
		showTextures = gEngfuncs.pfnGetCvarPointer( "r_showtextures" );

	const bool active = showTextures && showTextures->value > 0.0f;
	R_UpdateTextureZoom( active );

	if( !active )
	{
		showHelp = true;
		return;
	}

	if( showHelp && gEngfuncs.CL_CenterPrint )
	{
		gEngfuncs.CL_CenterPrint(
			"LEFT/RIGHT: page   Z/S or UP/DOWN: zoom   ESC: quit",
			0.25f );
		showHelp = false;
	}

	entries.clear( );
	const int atlasCount = std::max( 0, R_GetLightmapAtlasCount( ) );
	const size_t wantedCapacity =
		static_cast<size_t>( std::max( 0, g_texcount ) + atlasCount );
	if( entries.capacity( ) < wantedCapacity )
		entries.reserve( wantedCapacity );

	int regularTextureCount = 0;
	int liveAtlasCount = 0;

	for( int textureId = 1; textureId <= g_texcount; ++textureId )
	{
		const dxtexture_t &texture = g_Textures[textureId];
		if( !texture.tex )
			continue;

		show_texture_entry_t entry;
		entry.kind = show_texture_entry_t::DX_SHOW_REGULAR;
		entry.index = textureId;
		entry.rawTexture = texture.tex;
		entry.width = texture.width;
		entry.height = texture.height;
		entries.push_back( entry );
		++regularTextureCount;
	}

	for( int atlasIndex = 0; atlasIndex < atlasCount; ++atlasIndex )
	{
		IDirect3DTexture9 *atlasTexture =
			R_GetLightmapAtlasTexture( atlasIndex );
		if( !atlasTexture )
			continue;

		show_texture_entry_t entry;
		entry.kind = show_texture_entry_t::DX_SHOW_LIGHTMAP;
		entry.index = atlasIndex;
		entry.rawTexture = atlasTexture;
		entry.width = R_GetLightmapAtlasSize( atlasIndex );
		entry.height = entry.width;
		entry.dynamic = R_LightmapAtlasDynamic( atlasIndex ) != 0;
		entry.dynamicOverlay =
			R_LightmapAtlasOverlay( atlasIndex ) != 0;
		entry.materialName =
			R_GetLightmapAtlasMaterial( atlasIndex );
		entry.allocationCount =
			R_GetLightmapAtlasAllocCount( atlasIndex );
		entry.occupancy =
			R_GetLightmapAtlasOccupancy( atlasIndex );
		entries.push_back( entry );
		++liveAtlasCount;
	}

	const int screenWidth = std::max( 1, gpGlobals->width );
	const int screenHeight = std::max( 1, gpGlobals->height );

	int charHeight = 12;
	if( gEngfuncs.Con_DrawStringLen )
		gEngfuncs.Con_DrawStringLen( nullptr, nullptr, &charHeight );
	charHeight = std::max( 1, charHeight );

	const float zoom = R_ShowTexturesClampZoom( dx_showtextures_zoom.value );
	const int maxSquareSize = std::max( 32,
		std::min( screenWidth, screenHeight - charHeight * 2 ) );
	const int tileSize = std::max( 64, std::min( maxSquareSize,
		static_cast<int>( 200.0f * zoom + 0.5f ) ) );
	const int tileStrideY = tileSize + charHeight * 2;

	const int columns = std::max( 1, screenWidth / tileSize );
	const int rows = std::max( 1, screenHeight / tileStrideY );
	const int perPage = std::max( 1, columns * rows );
	const int totalEntries = static_cast<int>( entries.size( ) );
	const int maxPage = std::max( 1,
		( std::max( 1, totalEntries ) + perPage - 1 ) / perPage );
	const int requestedPage = std::max( 1,
		static_cast<int>( showTextures->value ) );
	const int page = std::min( requestedPage, maxPage );
	if( requestedPage != page && gEngfuncs.Cvar_SetValue )
		gEngfuncs.Cvar_SetValue( "r_showtextures", static_cast<float>( page ) );
	const int start = ( page - 1 ) * perPage;

	device->Clear( 0, nullptr, D3DCLEAR_TARGET,
		D3DCOLOR_XRGB( 0, 0, 0 ), 1.0f, 0 );

	const int savedMode = g_2dRenderMode;
	const D3DCOLOR savedColor = g_Color;
	g_2dRenderMode = kRenderTransTexture;
	g_Color = D3DCOLOR_ARGB( 255, 255, 255, 255 );

	const rgba_t textColor = { 255, 255, 255, 255 };

	for( int k = 0; k < perPage; ++k )
	{
		const int listIndex = start + k;
		if( listIndex < 0 || listIndex >= totalEntries )
			break;

		const show_texture_entry_t &entry = entries[listIndex];
		const float x = static_cast<float>(
			( k % columns ) * screenWidth / columns );
		const float y = static_cast<float>(
			( k / columns ) * screenHeight / rows );

		if( entry.kind == show_texture_entry_t::DX_SHOW_REGULAR )
		{
			R_DrawStretchPic( x, y,
				static_cast<float>( tileSize ),
				static_cast<float>( tileSize ),
				0.0f, 0.0f, 1.0f, 1.0f, entry.index );
		}
		else
		{
			R_DrawRawTexture2D( x, y,
				static_cast<float>( tileSize ),
				static_cast<float>( tileSize ),
				0.0f, 0.0f, 1.0f, 1.0f, entry.rawTexture );
		}

		if( !gEngfuncs.Con_DrawString )
			continue;

		char name[96]{};
		if( entry.kind == show_texture_entry_t::DX_SHOW_REGULAR )
		{
			const dxtexture_t &texture = g_Textures[entry.index];
			COM_FileBase( texture.name, name, sizeof( name ) );
			if( !name[0] )
				Q_snprintf( name, sizeof( name ), "#%d", entry.index );
		}
		else if( entry.dynamicOverlay )
		{
			Q_snprintf( name, sizeof( name ),
				"LM[%d] dyn-overlay", entry.index );
		}
		else if( entry.materialName && entry.materialName[0] )
		{
			Q_snprintf( name, sizeof( name ),
				"LM[%d] %s %s", entry.index,
				entry.dynamic ? "style" : "static-local",
				entry.materialName );
		}
		else
		{
			Q_snprintf( name, sizeof( name ),
				"LM[%d] %s", entry.index,
				entry.dynamic ? "style" : "static-shared" );
		}

		if( gEngfuncs.Con_DrawStringLen )
		{
			int textWidth = 0;
			gEngfuncs.Con_DrawStringLen( name, &textWidth, nullptr );
			if( textWidth > tileSize && strlen( name ) > 19 )
			{
				name[16] = '.';
				name[17] = '.';
				name[18] = '.';
				name[19] = '\0';
			}
		}

		gEngfuncs.Con_DrawString(
			static_cast<int>( x ) + 1,
			static_cast<int>( y ) + tileSize,
			name, textColor );

		char details[112];
		const unsigned long long bytes =
			static_cast<unsigned long long>( std::max( 0, entry.width ) ) *
			static_cast<unsigned long long>( std::max( 0, entry.height ) ) * 4ull;
		if( entry.kind == show_texture_entry_t::DX_SHOW_LIGHTMAP )
		{
			Q_snprintf( details, sizeof( details ),
				"%dx%d  %d rect%s  %.1f%% used  %.1f KB",
				entry.width, entry.height,
				entry.allocationCount,
				entry.allocationCount == 1 ? "" : "s",
				entry.occupancy * 100.0f,
				static_cast<double>( bytes ) / 1024.0 );
		}
		else
		{
			Q_snprintf( details, sizeof( details ),
				"%dx%d D3D9  %.1f KB",
				entry.width, entry.height,
				static_cast<double>( bytes ) / 1024.0 );
		}

		gEngfuncs.Con_DrawString(
			static_cast<int>( x ) + 1,
			static_cast<int>( y ) + tileSize + charHeight,
			details, textColor );
	}

	if( gEngfuncs.Con_DrawString )
	{
		char header[160];
		Q_snprintf( header, sizeof( header ),
			"page %d/%d  zoom %.2fx  textures %d + lightmaps %d",
			page, maxPage, zoom,
			regularTextureCount, liveAtlasCount );
		gEngfuncs.Con_DrawString( 4, 4, header, textColor );
	}

	g_Color = savedColor;
	g_2dRenderMode = savedMode;

	if( gEngfuncs.CL_DrawCenterPrint )
		gEngfuncs.CL_DrawCenterPrint( );
}

void R_BeginFrame( qboolean clear )
{
	if( !device )
		return;

	if( !g_dxRSpeedsCvar && gEngfuncs.pfnGetCvarPointer )
		g_dxRSpeedsCvar = gEngfuncs.pfnGetCvarPointer( "r_speeds" );

	DX_ProfilerBeginFrame(
		g_dxRSpeedsCvar && g_dxRSpeedsCvar->value > 0.0f );

	DX_PROFILE_SCOPE( beginFrameScope, DX_PROFILE_BEGINFRAME );
	g_2dRenderMode = kRenderNormal;
	g_2dModeActive = false;
	R_Reset2DBatch( );

	if( dx_remix_perf.value == 0.0f )
		DX_InvalidateTextureBindCache( );
	R_ApplyGammaRamp( );
	R_BeginScratchFrame( );
	R_BeginModelsFrame( );

	{
		const double profileStart = DX_ProfileActive( ) ? DX_Now( ) : 0.0;
		R_UpdateLightmaps( );
		if( DX_ProfileActive( ) )
			DX_ProfileTime( DX_PROFILE_LIGHTMAP_UPDATES, DX_Now( ) - profileStart );
	}
	DX_SetRenderStateCached( D3DRS_ZENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_LIGHTING, FALSE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	DX_BindRawTexture( 1, nullptr );
	DX_SetTextureStageStateCached( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
	DX_SetTextureStageStateCached( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
	g_Color = D3DCOLOR_ARGB( 255, 255, 255, 255 );

	if( clear )
	{
		const double profileStart = DX_ProfileActive( ) ? DX_Now( ) : 0.0;
		device->Clear( 0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			D3DCOLOR_XRGB( 0, 0, 0 ), 1.0f, 0 );
		if( DX_ProfileActive( ) )
			DX_ProfileTime( DX_PROFILE_CLEAR, DX_Now( ) - profileStart );
	}
	{
		const double profileStart = DX_ProfileActive( ) ? DX_Now( ) : 0.0;
		const HRESULT beginResult = device->BeginScene( );
		g_dxSceneActive = SUCCEEDED( beginResult );
		if( DX_ProfileActive( ) )
			DX_ProfileTime( DX_PROFILE_BEGINSCENE, DX_Now( ) - profileStart );
	}
}

void R_RenderFrame( )
{

}

void R_EndFrame( void )
{
	if( !device )
		return;

	R_Flush2DBatch( );

	{
		const double profileStart = DX_ProfileActive( ) ? DX_Now( ) : 0.0;
		if( g_dxSceneActive )
		{
			device->EndScene( );
			g_dxSceneActive = false;
		}
		if( DX_ProfileActive( ) )
			DX_ProfileTime( DX_PROFILE_ENDSCENE, DX_Now( ) - profileStart );
	}
	{
		const double profileStart = DX_ProfileActive( ) ? DX_Now( ) : 0.0;
		device->Present( nullptr, nullptr, nullptr, nullptr );
		if( DX_ProfileActive( ) )
			DX_ProfileTime( DX_PROFILE_PRESENT, DX_Now( ) - profileStart );
	}

	{
		DX_PROFILE_SCOPE( endMiscScope, DX_PROFILE_ENDFRAME_MISC );
		if( gp_cl )
		{
			if( g_previousClientTime >= 0.0f )
			{
				const float elapsed = static_cast<float>( gp_cl->time - g_previousClientTime );
				g_frametime = std::max( 0.0f, std::min( 0.25f, elapsed ) );
			}
			g_previousClientTime = gp_cl->time;
		}
	}

	DX_ProfilerSampleFrame( );
}

void R_SetRenderMode( int mode )
{

	if( mode != g_2dRenderMode )
		R_Flush2DBatch( );

	g_2dRenderMode = mode;
	DX_AllowFog( ( mode == kRenderGlow || mode == kRenderTransAdd ) ? false : true );
	switch( mode )
	{
	case kRenderNormal:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		break;
	case kRenderTransColor:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	case kRenderTransTexture:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	case kRenderGlow:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );

		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	case kRenderTransAlpha:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	case kRenderTransAdd:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	}
}

void R_ClearScene( void )
{
	++g_studioApiFrameCount;

	g_opaqueBrushEntities.clear( );
	g_opaqueStudioEntities.clear( );
	g_opaqueSpriteEntities.clear( );
	g_translucentCandidates.clear( );
	g_entityLightEmitters.clear( );
	g_waterBrushEntities.clear( );
	g_lateWaterBrushEntities.clear( );
	g_translucentDrawList.clear( );
	g_opaqueBrushCount = 0;
	g_opaqueStudioCount = 0;
	g_opaqueSpriteCount = 0;
	g_translucentCandidateCount = 0;
	g_entityLightEmitterCount = 0;
	g_waterBrushCount = 0;
	g_lateWaterBrushCount = 0;
	g_beamEntityCount = 0;

	memset( g_playerSubmitted, 0,
		sizeof( g_playerSubmitted ) );
}

void R_NewMap( void )
{
	DX_ResetFog( );
	g_worldmodel = gp_cl ? gp_cl->models[1] : nullptr;
	g_dxOverviewView = false;
	g_dxOrthoValid = false;
	memset( g_playerSubmitted, 0, sizeof( g_playerSubmitted ) );
	memset( g_remotePlayerInterp, 0, sizeof( g_remotePlayerInterp ) );
	memset( g_playerModelCache, 0, sizeof( g_playerModelCache ) );

	R_SyncLightStylesFromEngine( );
	R_RefreshLightmaps( );

	R_ActivateDecals( );
}

static void R_GammaChanged( qboolean state )
{
	g_lastGamma = -1.0f;
	g_lastBrightness = -1.0f;
	R_ApplyGammaRamp( );
	R_RefreshLightmaps( );
}

const ref_interface_t gReffuncs =
{
	.R_Init = R_Init,
	.R_Shutdown = R_Shutdown,
	.R_GetConfigName = R_GetConfigName,
	.R_SetDisplayTransform = R_SetDisplayTransform,

	.GL_SetupAttributes = DX_SetupAttributes,
	.GL_InitExtensions = R_SimpleStub,
	.GL_ClearExtensions = R_SimpleStub,

	.R_GammaChanged = R_GammaChanged,
	.R_BeginFrame = R_BeginFrame,
	.R_RenderScene = R_RenderFrame,
	.R_EndFrame = R_EndFrame,
	.R_PushScene = R_SimpleStub,
	.R_PopScene = R_SimpleStub,
	.GL_BackendStartFrame = R_SimpleStub,
	.GL_BackendEndFrame = R_SimpleStub,

	.R_ClearScreen = R_SimpleStub,
	.R_AllowFog = DX_AllowFog,
	.GL_SetRenderMode = R_SetRenderMode,

	.R_AddEntity = R_AddEntity,
	.R_ProcessEntData = R_ProcessEntData,

	.R_ShowTextures = R_ShowTextures,

	.R_GetTextureOriginalBuffer = R_GetTextureOriginalBuffer,
	.GL_LoadTextureFromBuffer = DX_LoadTextureFromBuffer,
	.GL_ProcessTexture = DX_ProcessTexture,
	.R_SetupSky = R_SetupSky,

	.R_Set2DMode = R_Set2DMode,
	.R_DrawStretchPic = R_DrawStretchPic,
	.FillRGBA = FillRGBA,
	.WorldToScreen = WorldToScreen,

	.VID_ScreenShot = VID_ScreenShot,
	.VID_CubemapShot = VID_CubemapShot,

	.R_LightPoint = R_LightPoint,

	.R_DecalShoot = R_AddDecal,
	.R_DecalRemoveAll = R_DecalRemoveAll,
	.R_CreateDecalList = R_CreateDecalList,
	.R_ClearAllDecals = R_ClearDecals,

	.R_StudioEstimateFrame = R_StudioEstimateFrame,
	.R_StudioLerpMovement = R_StudioLerpMovement,
	.R_StudioFillAPI = R_StudioFillAPI,
	.R_StudioSetDrawInterface = R_StudioSetInterface,

	.R_SetSkyCloudsTextures = R_SetSkyCloudsTextures,
	.GL_SubdivideSurface = DX_SubdivideSurface,
	.CL_RunLightStyles = CL_RunLightStyles,

	.Mod_ProcessRenderData = Mod_ProcessRenderData,
	.Mod_StudioLoadTextures = Mod_StudioLoadTextures,

	.CL_DrawParticles = CL_DrawParticles,
	.CL_DrawTracers = CL_DrawTracers,
	.CL_DrawBeams = CL_DrawBeams,

	.RefGetParm = RefGetParm,

	.R_GetDetailScaleForTexture = R_GetDetailScale,
	.R_SetDetailScaleForTexture = R_SetDetailScale,

	.GL_CreateTexture = DX_CreateTexture,
	.GL_FindTexture = DX_FindTexture,
	.GL_TextureName = DX_TextureName,
	.GL_TextureData = R_GetTextureOriginalBuffer,
	.GL_LoadTexture = DX_LoadTexture,
	.GL_FreeTexture = DX_FreeTexture,
	.R_OverrideTextureSourceSize = R_OverrideTextureSize,

	.GL_UpdateTexture = R_APIUpdateTexture,

	.GL_Bind = DX_Bind,

	.GL_RenderFrame = DX_RenderFrame,
	.GL_OrthoBounds = DX_OrthoBounds,
	.R_SpeedsMessage = R_SpeedsMessage,
	.Mod_GetCurrentVis = Mod_GetCurrentVis,
	.R_NewMap = R_NewMap,
	.R_ClearScene = R_ClearScene,

	.TriRenderMode = R_TriRenderMode,
	.Begin = R_TriBegin,
	.End = R_TriEnd,
	.Color4f = Color4f,
	.Color4ub = Color4ub,
	.Vertex3fv = R_TriVertex3fv,
	.Vertex3f = R_TriVertex3f,
	.CullFace = R_TriCullFace,

	.R_FillRenderAPI = R_FillRenderAPI,
	.R_FillTriAPI = R_FillTriAPI,

	.VGUI_SetupDrawing = VGUI_SetupDrawing,
};
