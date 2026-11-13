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
#include <unordered_map>
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
#include <cstdarg>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")

#include "../../common/ref_params.h"
#include "../../common/pmove.h"
#include "../../common/r_studioint.h"
#include "dx_textures.h"
#include "dx_models.h"
#include "enginefeatures.h"
#include "dx_local.h"
#include "dx_ragdoll.h"
#include "dx_cof_scene.h"
#include "dx_profiler.h"
#include "dx_batching.h"
#include "dx_cof_dual.h"
#include "dx_cof_pause.h"
#include "dx_map_light_api.h"
#include "dx_map_sky.h"
#include "dx_light_position.h"
#include "dx_studio_fixture.h"
#include "dx_cof_features.h"
#include "dx_cof_pickup_outline.h"
#include "dx_cof_crouch.h"
#include "dx_gun_smoke.h"
static dx_cof_crouch_signal g_cofCrouchSignal;
extern "C" __declspec(dllexport) int __cdecl DX_CofCrouchStateV1(int held,float height,float now){return g_cofCrouchSignal.publish(held,height,now)?1:0;}
bool R_CofShadowCrouchInput(float now,bool& held,float& height){return g_cofCrouchSignal.sample(now,held,height);}
static std::vector<cof_pickup::Span> g_cofPickupOutline;
static dx_cof_dual_state_t g_cofDual;
cl_entity_s* R_CofShadowSecondary(float now){return g_cofDual.current(now);}
bool R_CofDualSecondary(const cl_entity_s* ent){return ent&&ent==&g_cofDual.entity;}
// Native CoF mirrors the primary viewmodel too when it is the flare (its
// right arm is hidden, hand rule 2), so the flare and the mirrored secondary
// end up in opposite hands instead of both in the left one.
static bool R_CofPrimaryMirrored(float now){return g_cofDual.current(now)&&g_cofDual.primaryHiddenArm==2;}
static uint64_t g_cofDualPass=0;
static dx_cof_player_visibility_t g_cofPlayerVisibility;
static cof_features::Pickups g_cofPickups;
static int g_cofPickupIndex=0,g_cofPickupModel=0;static float g_cofPickupStrength=0;
static bool g_cofPickupLight=false;
static void R_CofPickupStatusCmd() {
 if(gEngfuncs.Con_Printf)gEngfuncs.Con_Printf("PICKUP outline index=%d model=%d strength=%.3f spans=%u softLight=%d\n",g_cofPickupIndex,g_cofPickupModel,g_cofPickupStrength,unsigned(g_cofPickupOutline.size()),int(g_cofPickupLight));
}
extern "C" __declspec(dllexport) int __cdecl DX_CofPickupV1(int index,int model,int active,float time) {
 return g_cofPickups.publish(index,model,active!=0,time)?1:0;
}
float R_CofPickupStrength(const cl_entity_s* ent) {
 return ent&&ent->model&&ent->index==g_cofPickupIndex&&ent->curstate.modelindex==g_cofPickupModel?
  g_cofPickupStrength:0.f;
}
static void R_ResetCofPlayerVisibility(){g_cofPlayerVisibility.clear();R_ResetCofShadow();g_cofCrouchSignal.clear();g_cofPickups.clear();g_cofPickupOutline.clear();g_cofPickupIndex=0;g_cofPickupModel=0;g_cofPickupStrength=0;g_cofPickupLight=false;}
extern "C" __declspec(dllexport) int __cdecl DX_CofPlayerVisibilityV1(unsigned reasons,float time) {
 return g_cofPlayerVisibility.publish(reasons,time)?1:0;
}
bool R_CofPlayerDrawAllowed(const cl_entity_s* entity) {
 // DeadPlayer geometry delegates to StudioDrawPlayer in the native client too.
 if(!entity||(!entity->player&&entity->curstate.renderfx!=kRenderFxDeadPlayer))return true;
 return !g_cofPlayerVisibility.hidden(R_CurrentRenderTime(0.f));
}

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
static void R_ResetTriangleBatch();
static void R_TriBatchStats();

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
static D3DXMATRIX g_sceneView = {};
static D3DXMATRIX g_sceneProjection = {};
static bool g_sceneViewportValid = false;

struct dx_debug_text_3d_t
{
	vec3_t world = { 0.0f, 0.0f, 0.0f };
	rgba_t color = { 255, 255, 255, 255 };
	std::string text;
};

static constexpr size_t DX_DEBUG_TEXT_3D_MAX = 4096;
static std::vector<dx_debug_text_3d_t> g_debugText3D;

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

bool R_DrawBoundVertexIndices(INT baseVertex,UINT vertexCount,const DWORD* indices,UINT count) {
    if(!device || !indices || !count || count%3 || count>262144) return false;
    UINT start=0;
    if(!R_UploadScratchIndices(indices,count*sizeof(DWORD),D3DFMT_INDEX32,start)) return false;
    DX_ProfileEvent(DX_PROFILE_EVENT_GLASS_INDEX_BYTES,count*sizeof(DWORD));
    device->SetIndices(g_contextScratchIB);
    const HRESULT result=device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,baseVertex,0,vertexCount,start,count/3);
    if(FAILED(result)) return false;
    DX_ProfileDraw(true,count/3);
    return true;
}

bool R_DrawScratch( D3DPRIMITIVETYPE type,
	UINT primitiveCount, UINT vertexCount, const void *vertices, UINT stride )
{
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
	if( !device || !vertices || primitiveCount == 0 || vertexCount == 0 )
		return false;
	UINT vertexOffset = 0;
	if( !R_UploadScratchVerts( vertices, vertexCount * stride, stride, vertexOffset ) )
		return false;
	device->SetStreamSource( 0, g_contextScratchVB, vertexOffset, stride );
	DX_ProfileDraw( false, primitiveCount );
	return SUCCEEDED(device->DrawPrimitive( type, 0, primitiveCount ));
}

static bool R_DrawScratchIndexed( D3DPRIMITIVETYPE type,
	UINT vertexCount, UINT primitiveCount, const void *indices,
	D3DFORMAT indexFormat, const void *vertices, UINT stride )
{
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
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

convar_t dx_cof_player_shadow = { (char *)"dx_cof_player_shadow", (char *)"1", FCVAR_ARCHIVE };
static convar_t dx_cof_pickup_outline = { (char *)"dx_cof_pickup_outline", (char *)"1", FCVAR_ARCHIVE };
convar_t dx_remix = { (char *)"dx_remix", (char *)"0", 0 };
convar_t dx_remix_decal_cutout =
{ (char *)"dx_remix_decal_cutout", (char *)"1", 0 };
convar_t dx_remix_decal_alpha_ref =
{ (char *)"dx_remix_decal_alpha_ref", (char *)"96", 0 };
convar_t dx_triapi_batch =
{ (char *)"dx_triapi_batch", (char *)"1", 0 };
static convar_t dx_triapi_debug={(char*)"dx_triapi_debug",(char*)"0",0};
convar_t dx_triapi_batch_max_vertices =
{ (char *)"dx_triapi_batch_max_vertices", (char *)"32768", 0 };
convar_t dx_remix_glass_batch =
{ (char *)"dx_remix_glass_batch", (char *)"1", 0 };
static convar_t dx_remix_stable_sprites =
{ (char *)"dx_remix_stable_sprites", (char *)"1", 0 };

convar_t dx_remix_glass_single_sided =
{ (char *)"dx_remix_glass_single_sided", (char *)"1", 0 };
convar_t dx_remix_map_lights =
{ (char *)"dx_remix_map_lights", (char *)"1", 0 };
convar_t dx_remix_map_light_api = { (char *)"dx_remix_map_light_api", (char *)"1", 0 };
convar_t dx_remix_map_light_radius = { (char *)"dx_remix_map_light_radius", (char *)"8", 0 };
convar_t dx_remix_map_light_clearance = { (char *)"dx_remix_map_light_clearance", (char *)"1", 0 };
convar_t dx_remix_map_light_centering = { (char *)"dx_remix_map_light_centering", (char *)"1", 0 };
convar_t dx_remix_map_light_output_scale = { (char *)"dx_remix_map_light_output_scale", (char *)"0.75", 0 };
// Relative to the project's existing light calibration; archived for next run.
convar_t light_level = { (char *)"light_level", (char *)"1", FCVAR_ARCHIVE };
static float R_MapLightLevel() {
 return std::isfinite(light_level.value) ? std::clamp(light_level.value,0.f,16.f) : 1.f;
}
convar_t dx_remix_map_light_intensity =
{ (char *)"dx_remix_map_light_intensity", (char *)"1", 0 };
convar_t dx_remix_map_light_range_scale =
{ (char *)"dx_remix_map_light_range_scale", (char *)"1", 0 };

convar_t dx_fog = { (char *)"dx_fog", (char *)"1", 0 };

convar_t dx_msaa_samples = { (char *)"dx_msaa_samples", (char *)"16", 0 };
convar_t dx_msaa_quality = { (char *)"dx_msaa_quality", (char *)"-1", 0 };
convar_t dx_anisotropy = { (char *)"dx_anisotropy", (char *)"16", 0 };

convar_t dx_showtextures_zoom =
{ (char *)"dx_showtextures_zoom", (char *)"1.0", 0 };
convar_t dx_debug_text3d_test =
{ (char *)"dx_debug_text3d_test", (char *)"0", 0 };

convar_t r_viewmodel_fov = { (char *)"r_viewmodel_fov", (char *)"60", 0 };

convar_t r_viewmodel_near = { (char *)"r_viewmodel_near", (char *)"4.0", 0 };
convar_t r_viewmodel_offset_x = { (char *)"r_viewmodel_offset_x", (char *)"0", 0 };
convar_t r_viewmodel_offset_y = { (char *)"r_viewmodel_offset_y", (char *)"0", 0 };
convar_t r_viewmodel_offset_z = { (char *)"r_viewmodel_offset_z", (char *)"0.5", 0 };

convar_t dx_viewmodel_attachment_fov =
{ (char *)"dx_viewmodel_attachment_fov", (char *)"1", FCVAR_ARCHIVE };
convar_t dx_cof_shell_unstick = { (char *)"dx_cof_shell_unstick", (char *)"1", FCVAR_ARCHIVE };
convar_t dx_viewmodel_beam_offset =
{ (char *)"dx_viewmodel_beam_offset", (char *)"-3", 0 };
// The viewmodel is drawn with r_viewmodel_fov, but client effects that read
// its public attachments (Aurora flare particles, smoke) are drawn with the
// world projection. Returns the lateral scale (about the view axis) that makes
// a world-space attachment land on the drawn viewmodel; 1 when not applicable.
float R_ViewModelAttachmentScale() {
 if(dx_viewmodel_attachment_fov.value==0.f||g_sceneProjection._34!=1.f||g_sceneProjection._22<=0.f)return 1.f;
 const float fov=std::max(10.f,std::min(170.f,r_viewmodel_fov.value));
 const float scale=(1.f/tanf(D3DXToRadian(fov)*.5f))/g_sceneProjection._22;
 return std::isfinite(scale)&&scale>.2f&&scale<5.f?scale:1.f;
}

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
static bool g_contextCommandsRegistered = false;
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

static dx_map_light::Bridge g_mapEmitterBridge;
static bool R_CofLanternModel(const cl_entity_s* ent,bool view) {
 return ent && ent->model && ent->model->type==mod_studio && !(ent->curstate.effects&EF_NODRAW) &&
  !Q_stricmp(ent->model->name,view?"models/weapons/lantern/v_lantern.mdl":"models/weapons/lantern/w_lantern.mdl");
}
static dx_map_sky::Selection g_mapSky{ GetTickCount() & 0x3fffffffu };
static ULONGLONG g_mapSkyRetryAt = 0;
static uint64_t g_mapEmitterIdentity = 0;

enum remix_map_light_type_t : unsigned char
{
	REMIX_MAP_LIGHT_POINT = 0,
	REMIX_MAP_LIGHT_SPOT,
	REMIX_MAP_LIGHT_DIRECTIONAL
};

struct remix_map_light_t
{
	int entityIndex = 0;
	float centeredRadius = 8.f;
	float manualRadius = 0.f;
	vec3_t centeredOrigin = {}, automaticOrigin = {};
	int fixture = -1;
	int bulb = -1;
	const char *positionReason = "authored; no verified fixture profile";
	const char *submissionRoute = "not submitted";
	bool manualPosition = false;
	char parentModel[64] = {};
	D3DXMATRIX parentInverse;
	bool parentResolved = false;
	vec3_t movingOrigin = {}, movingDirection = {};
	dx_map_light::Instance instance;
	remix_map_light_type_t type = REMIX_MAP_LIGHT_POINT;
	vec3_t origin = {};
	vec3_t direction = { 1.0f, 0.0f, 0.0f };
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float brightness = 0.0f;
	float fade = 1.0f;
	float innerCone = 10.0f;
	float outerCone = 10.0f;
	int falloff = 2;
	int style = 0;
};

struct map_light_debug_t
{
	char model[64] = {};
	char body[32] = {}, skin[32] = {}, scale[32] = {}, sequence[32] = {};
	char classname[64] = {};
	char origin[96] = {};
	char light[96] = {};
	char style[32] = {};
	char targetname[64] = {};
	char target[64] = {};
	char angles[64] = {};
	char angle[32] = {};
	char pitch[32] = {};
	char cone[32] = {};
	char cone2[32] = {};
	char fade[32] = {};
	char falloff[32] = {};
	char sky[32] = {};
	char spread[32] = {};
	char pattern[64] = {};
	char spawnflags[32] = {};
	char diffuseLight[96] = {};
	char diffuseLight2[96] = {};
};

struct map_entity_record_t
{
	map_light_debug_t fields;
	int entityIndex = 0;
};

struct remix_map_light_candidate_t
{
	size_t lightIndex = 0;
	float distanceSquared = 0.0f;
	bool directional = false;
};

static std::vector<dx_studio_fixture::Bulb> g_studioBulbs;
static std::vector<dx_light_position::Fixture> g_brushGlobes;
static std::vector<remix_map_light_t> g_remixMapLights;
static std::vector<remix_map_light_candidate_t> g_remixMapLightCandidates;
static int g_remixMapLightSlotCount = 0;
static int g_remixEnabledLightLimit = -1;
static bool g_remixMapLightCapacityWarned = false;
/* Keep the renderer's transient point lights in slots 0-7. Map lights start
 * at 8 so their state never collides with muzzle flashes or the flashlight. */
static constexpr int DX_REMIX_DYNAMIC_LIGHT_SLOTS = 8;

static const char *R_ParseMapEntityToken( const char *data,
	char *token, size_t tokenSize )
{
	if( !data || !token || tokenSize == 0 )
		return nullptr;

	token[0] = '\0';
	for( ;; )
	{
		while( *data && static_cast<unsigned char>( *data ) <= ' ' )
			++data;
		if( data[0] == '/' && data[1] == '/' )
		{
			while( *data && *data != '\n' )
				++data;
			continue;
		}
		break;
	}

	if( !*data )
		return nullptr;

	if( *data == '{' || *data == '}' )
	{
		token[0] = *data++;
		token[1] = '\0';
		return data;
	}

	size_t length = 0;
	if( *data == '"' )
	{
		++data;
		while( *data && *data != '"' )
		{
			if( length + 1 < tokenSize )
				token[length++] = *data;
			++data;
		}
		if( *data == '"' )
			++data;
	}
	else
	{
		while( *data && static_cast<unsigned char>( *data ) > ' ' &&
			*data != '{' && *data != '}' )
		{
			if( length + 1 < tokenSize )
				token[length++] = *data;
			++data;
		}
	}

	token[length] = '\0';
	return data;
}

static void R_CopyMapLightField( char *destination, size_t destinationSize,
	const char *value )
{
	if( !destination || destinationSize == 0 )
		return;
	std::snprintf( destination, destinationSize, "%s", value ? value : "" );
}

static void R_SetMapLightField( map_light_debug_t &light,
	const char *key, const char *value )
{
	if( !strcmp( key, "model" ) )
		R_CopyMapLightField( light.model, sizeof( light.model ), value );
	else if( !strcmp( key, "body" ) ) R_CopyMapLightField(light.body,sizeof(light.body),value);
	else if( !strcmp( key, "skin" ) ) R_CopyMapLightField(light.skin,sizeof(light.skin),value);
	else if( !strcmp( key, "scale" ) ) R_CopyMapLightField(light.scale,sizeof(light.scale),value);
	else if( !strcmp( key, "sequence" ) ) R_CopyMapLightField(light.sequence,sizeof(light.sequence),value);
	else if( !strcmp( key, "classname" ) )
		R_CopyMapLightField( light.classname, sizeof( light.classname ), value );
	else if( !strcmp( key, "origin" ) )
		R_CopyMapLightField( light.origin, sizeof( light.origin ), value );
	else if( !strcmp( key, "_light" ) ||
		( !strcmp( key, "light" ) && !light.light[0] ) )
		R_CopyMapLightField( light.light, sizeof( light.light ), value );
	else if( !strcmp( key, "style" ) )
		R_CopyMapLightField( light.style, sizeof( light.style ), value );
	else if( !strcmp( key, "targetname" ) )
		R_CopyMapLightField( light.targetname, sizeof( light.targetname ), value );
	else if( !strcmp( key, "target" ) )
		R_CopyMapLightField( light.target, sizeof( light.target ), value );
	else if( !strcmp( key, "angles" ) )
		R_CopyMapLightField( light.angles, sizeof( light.angles ), value );
	else if( !strcmp( key, "angle" ) )
		R_CopyMapLightField( light.angle, sizeof( light.angle ), value );
	else if( !strcmp( key, "pitch" ) )
		R_CopyMapLightField( light.pitch, sizeof( light.pitch ), value );
	else if( !strcmp( key, "_cone" ) )
		R_CopyMapLightField( light.cone, sizeof( light.cone ), value );
	else if( !strcmp( key, "_cone2" ) )
		R_CopyMapLightField( light.cone2, sizeof( light.cone2 ), value );
	else if( !strcmp( key, "_fade" ) )
		R_CopyMapLightField( light.fade, sizeof( light.fade ), value );
	else if( !strcmp( key, "_falloff" ) )
		R_CopyMapLightField( light.falloff, sizeof( light.falloff ), value );
	else if( !strcmp( key, "_sky" ) )
		R_CopyMapLightField( light.sky, sizeof( light.sky ), value );
	else if( !strcmp( key, "_spread" ) )
		R_CopyMapLightField( light.spread, sizeof( light.spread ), value );
	else if( !strcmp( key, "pattern" ) )
		R_CopyMapLightField( light.pattern, sizeof( light.pattern ), value );
	else if( !strcmp( key, "spawnflags" ) )
		R_CopyMapLightField( light.spawnflags, sizeof( light.spawnflags ), value );
	else if( !strcmp( key, "_diffuse_light" ) )
		R_CopyMapLightField( light.diffuseLight, sizeof( light.diffuseLight ), value );
	else if( !strcmp( key, "_diffuse_light2" ) )
		R_CopyMapLightField( light.diffuseLight2, sizeof( light.diffuseLight2 ), value );
}

static bool R_IsMapLightClassname( const char *classname )
{
	return classname && ( !strcmp( classname, "light" ) ||
		!strncmp( classname, "light_", 6 ) );
}

static void R_ParseMapEntities( std::vector<map_entity_record_t> &entities )
{
	entities.clear( );
	if( !g_worldmodel || !g_worldmodel->entities || !g_worldmodel->entities[0] )
		return;

	const char *cursor = g_worldmodel->entities;
	char token[1024];
	char key[256];
	int entityIndex = 0;
	while( ( cursor = R_ParseMapEntityToken( cursor, token, sizeof( token ) ) ) != nullptr )
	{
		if( strcmp( token, "{" ) )
			continue;

		map_entity_record_t entity{};
		entity.entityIndex = entityIndex++;
		while( ( cursor = R_ParseMapEntityToken( cursor, token, sizeof( token ) ) ) != nullptr )
		{
			if( !strcmp( token, "}" ) )
				break;

			R_CopyMapLightField( key, sizeof( key ), token );
			cursor = R_ParseMapEntityToken( cursor, token, sizeof( token ) );
			if( !cursor || !strcmp( token, "}" ) )
				break;
			R_SetMapLightField( entity.fields, key, token );
		}
		entities.push_back( entity );
	}
}

static bool R_ParseMapVector( const char *text, vec3_t value )
{
	if( !text || !text[0] )
		return false;
	return std::sscanf( text, "%f %f %f", &value[0], &value[1], &value[2] ) == 3;
}

static bool R_NormalizeMapDirection( vec3_t direction )
{
	const float length = VectorLength( direction );
	if( length <= 0.0001f )
		return false;
	VectorScale( direction, 1.0f / length, direction );
	return true;
}

static bool R_ParseMapLightEmission( const char *text,
	float &red, float &green, float &blue, float &brightness )
{
	float values[4] = {};
	const int count = text && text[0]
		? std::sscanf( text, "%f %f %f %f",
			&values[0], &values[1], &values[2], &values[3] ) : 0;

	if( count >= 4 )
	{
		red = std::max( 0.0f, std::min( 1.0f, values[0] / 255.0f ) );
		green = std::max( 0.0f, std::min( 1.0f, values[1] / 255.0f ) );
		blue = std::max( 0.0f, std::min( 1.0f, values[2] / 255.0f ) );
		brightness = std::max( 0.0f, values[3] );
		return brightness > 0.0f;
	}

	if( count == 3 )
	{
		red = std::max( 0.0f, std::min( 1.0f, values[0] / 255.0f ) );
		green = std::max( 0.0f, std::min( 1.0f, values[1] / 255.0f ) );
		blue = std::max( 0.0f, std::min( 1.0f, values[2] / 255.0f ) );
		brightness = 255.0f;
		return red > 0.0f || green > 0.0f || blue > 0.0f;
	}

	if( count == 1 )
	{
		red = green = blue = 1.0f;
		brightness = std::max( 0.0f, values[0] );
		return brightness > 0.0f;
	}

	return false;
}

static const map_entity_record_t *R_FindMapTarget(
	const std::vector<map_entity_record_t> &entities, const char *targetname )
{
	if( !targetname || !targetname[0] )
		return nullptr;
	for( const map_entity_record_t &entity : entities )
	{
		if( entity.fields.targetname[0] &&
			!strcmp( entity.fields.targetname, targetname ) )
		{
			return &entity;
		}
	}
	return nullptr;
}

static bool R_BuildMapLightDirection( const map_light_debug_t &raw,
	const std::vector<map_entity_record_t> &entities, const vec3_t origin,
	vec3_t direction )
{
	if( raw.target[0] )
	{
		const map_entity_record_t *target = R_FindMapTarget( entities, raw.target );
		vec3_t targetOrigin;
		if( target && R_ParseMapVector( target->fields.origin, targetOrigin ) )
		{
			VectorSubtract( targetOrigin, origin, direction );
			if( R_NormalizeMapDirection( direction ) )
				return true;
		}
	}

	vec3_t angles = {};
	R_ParseMapVector( raw.angles, angles );
	if( raw.angle[0] )
	{
		const float angle = static_cast<float>( atof( raw.angle ) );
		if( angle == -1.0f )
		{
			VectorSet( direction, 0.0f, 0.0f, 1.0f );
			return true;
		}
		if( angle == -2.0f )
		{
			VectorSet( direction, 0.0f, 0.0f, -1.0f );
			return true;
		}
		if( angle != 0.0f )
			angles[1] = angle;
	}
	if( raw.pitch[0] )
	{
		const float pitch = static_cast<float>( atof( raw.pitch ) );
		if( pitch != 0.0f )
		{
			// Hammer pitch has the opposite sign from AngleVectors.
			angles[0] = -pitch;
		}
	}

	vec3_t right, up;
	AngleVectors( angles, direction, right, up );
	return R_NormalizeMapDirection( direction );
}

static void R_DisableRemixMapLights( void )
{
	for( auto &light : g_remixMapLights ){g_mapEmitterBridge.retire( light.instance );light.submissionRoute="disabled";}
	if( device )
	{
		for( int i = 0; i < g_remixMapLightSlotCount; ++i )
			device->LightEnable( DX_REMIX_DYNAMIC_LIGHT_SLOTS + i, FALSE );
	}
	g_remixMapLightSlotCount = 0;
}

static void R_ClearRemixMapLights( void )
{
	R_DisableRemixMapLights( );
	g_remixMapLights.clear( );
	g_studioBulbs.clear();
	g_brushGlobes.clear();
	g_remixMapLightCandidates.clear( );
	g_remixEnabledLightLimit = -1;
	g_remixMapLightCapacityWarned = false;
}

static const float *R_MapLightPosition( const remix_map_light_t &light )
{
	if(light.parentResolved)return light.movingOrigin;
	return dx_remix_map_light_centering.value != 0.f ? light.centeredOrigin : light.origin;
}

static void R_BindMapLightParent(remix_map_light_t& light,const std::vector<map_entity_record_t>& entities)
{
    if(!g_worldmodel||Q_stricmp(g_worldmodel->name,"maps/c_subway2st3.bsp"))return;
    // These seven authored lights illuminate bigtrain's interior, including
    // its table lamp. They have no movewith key because the original game
    // baked their lighting into the train's moving brush lightmaps.
    struct Binding {int id;float origin[3];};
    static const Binding bindings[]={
        {199,{-389,-506,-475}},{200,{-317,-418,-475}},
        {201,{-317,-242,-475}},{202,{-317,16,-475}},
        {203,{-317,-1070,-475}},{209,{-334,-133,-543}},
        {211,{-391,-676,-475}}
    };
    bool matched=false;
    for(const auto& binding:bindings)if(binding.id==light.entityIndex){
        matched=true;for(int a=0;a<3;++a)if(fabsf(binding.origin[a]-light.origin[a])>.01f)matched=false;
        break;
    }
    if(!matched)return;
    const map_entity_record_t* parent=nullptr;
    for(const auto& entity:entities)if(!strcmp(entity.fields.targetname,"bigtrain")){
        if(parent)return;parent=&entity;
    }
    if(!parent||strcmp(parent->fields.classname,"func_train")||strcmp(parent->fields.model,"*19"))return;
    cl_entity_t reference{};model_t model{};model.type=mod_brush;reference.model=&model;
    R_ParseMapVector(parent->fields.origin,reference.origin);R_ParseMapVector(parent->fields.angles,reference.angles);
    const float expected[3]={-354,-1066,-536};
    for(int a=0;a<3;++a)if(!std::isfinite(reference.origin[a])||!std::isfinite(reference.angles[a])||fabsf(reference.origin[a]-expected[a])>.01f)return;
    D3DXMATRIX world;R_SetEntityWorldTransform(&reference,0,&world);
    if(!D3DXMatrixInverse(&light.parentInverse,nullptr,&world))return;
    snprintf(light.parentModel,sizeof(light.parentModel),"%s",parent->fields.model);
}

static void R_UpdateMapLightParents()
{
    for(auto& light:g_remixMapLights)if(light.parentModel[0]){
        light.parentResolved=false;const cl_entity_s* parent=nullptr;bool ambiguous=false;
        auto inspect=[&](const std::vector<cl_entity_s*>& list){for(auto entity:list){
            if(!entity||!entity->model||entity->model->type!=mod_brush||
                (entity->curstate.effects&EF_NODRAW)||Q_stricmp(entity->model->name,light.parentModel))continue;
            if(parent&&parent!=entity){ambiguous=true;continue;}parent=entity;
        }};
        inspect(g_opaqueBrushEntities);inspect(g_translucentCandidates);
        if(!parent||ambiguous)continue;
        bool finite=true;for(int a=0;a<3;++a)if(!std::isfinite(parent->origin[a])||!std::isfinite(parent->angles[a]))finite=false;
        if(!finite)continue;
        D3DXMATRIX world;R_SetEntityWorldTransform(parent,R_CurrentRenderTime(0.f),&world);
        const auto position=dx_remix_map_light_centering.value!=0.f?light.centeredOrigin:light.origin;
        D3DXVECTOR3 authored(position),direction(light.direction),local,moved;
        D3DXVec3TransformCoord(&local,&authored,&light.parentInverse);D3DXVec3TransformCoord(&moved,&local,&world);
        for(int a=0;a<3;++a)light.movingOrigin[a]=moved[a];
        D3DXVec3TransformNormal(&local,&direction,&light.parentInverse);D3DXVec3TransformNormal(&moved,&local,&world);
        const float length=D3DXVec3Length(&moved);if(!std::isfinite(length)||length<1e-8f)continue;
        for(int a=0;a<3;++a){light.movingDirection[a]=moved[a]/length;if(!std::isfinite(light.movingOrigin[a])||!std::isfinite(light.movingDirection[a]))finite=false;}
        light.parentResolved=finite;
    }
}

static bool R_MapLightPositionAllowed( const vec3_t original, const vec3_t position )
{
	if( !g_worldmodel || !g_worldmodel->nodes ) return false;
	int budget=8192;
	return dx_light_position::clearSegment(g_worldmodel->nodes,
		dx_light_position::Point{original[0],original[1],original[2]},
		dx_light_position::Point{position[0],position[1],position[2]},
		[](mnode_t *node){return node->contents;},
		[](mnode_t *node,const dx_light_position::Point& point){return node->plane?DotProduct(point.data(),node->plane->normal)-node->plane->dist:std::numeric_limits<float>::quiet_NaN();},
		[](mnode_t *node,int side){return node_child(node,side,g_worldmodel);},budget);
}

static std::vector<dx_light_position::Fixture> R_CollectLampFixtures( const std::vector<model_t *> &models )
{
	std::vector<dx_light_position::Face> faces;
	int owner=0;
	for( auto model:models )
	{
		++owner;
		if( !model || model->type!=mod_brush ) continue;
		int first=0,count=0;
		if( !R_GetBrushSurfaceRange(model,first,count) ) continue;
		for( int i=first;i<first+count;++i )
		{
			const auto surface=&model->surfaces[i];
			if( !surface->texinfo || !surface->texinfo->texture || surface->numedges<3 || surface->numedges>64 ) continue;
			const auto texture=surface->texinfo->texture;
			const std::string name(texture->name,strnlen(texture->name,sizeof(texture->name)));
			if( !dx_light_position::outdoorGlobeMaterial(name) ) continue;
			dx_light_position::Face face;face.owner=owner;
			for( int edge=0;edge<surface->numedges;++edge )
			{
				const auto vertex=R_GetBrushSurfaceVertex(model,surface,edge);
				if( !vertex ){face.vertices.clear();break;}
				face.vertices.push_back({vertex->position[0],vertex->position[1],vertex->position[2]});
			}
			if( face.vertices.size()>=3 )faces.push_back(std::move(face));
			if( faces.size()>32768 ) return {};
		}
	}
	return dx_light_position::fixtures(faces);
}

static void R_CollectStudioBulbs(const std::vector<map_entity_record_t>& entities)
{
    const auto fs=gEngfuncs.fsapi;
    if(!fs||!fs->Open||!fs->FileLength||!fs->Read||!fs->Close)return;
    std::map<std::string,std::vector<unsigned char>> cache;
    for(const auto& entity:entities)
    {
        const auto& raw=entity.fields;
        if(strcmp(raw.classname,"env_model")||!dx_studio_fixture::profile(raw.model))continue;
        const int body=atoi(raw.body),skin=atoi(raw.skin);
        if(dx_studio_fixture::profile(raw.model)==4&&body==1)continue;
        if(atoi(raw.sequence)!=0){gEngfuncs.Con_Printf("D3D9: fixture entity %d: unsupported nonzero sequence\n",entity.entityIndex);continue;}
        auto found=cache.find(raw.model);
        if(found==cache.end())
        {
            std::vector<unsigned char> data;auto file=fs->Open(raw.model,"rb",false);
            if(file){const auto size=fs->FileLength(file);if(size>0&&size<=16*1024*1024){data.resize(size_t(size));if(fs->Read(file,data.data(),data.size())!=size)data.clear();}fs->Close(file);}
            found=cache.emplace(raw.model,std::move(data)).first;
        }
        cl_entity_s entityTransform{};
        R_ParseMapVector(raw.origin,entityTransform.origin);R_ParseMapVector(raw.angles,entityTransform.angles);
        if(!raw.angles[0]&&raw.angle[0])entityTransform.angles[1]=float(atof(raw.angle));
        entityTransform.curstate.scale=raw.scale[0]?float(atof(raw.scale)):1.f;
        if(!std::isfinite(entityTransform.curstate.scale)){gEngfuncs.Con_Printf("D3D9: fixture entity %d: invalid scale\n",entity.entityIndex);continue;}
        if(entityTransform.curstate.scale<=0)entityTransform.curstate.scale=1.f;
        D3DMATRIX world;R_StudioFixtureTransform(entityTransform.origin,entityTransform.angles,entityTransform.curstate.scale,&world);
        dx_studio_fixture::Bulb bulb;std::string reason;
        if(!dx_studio_fixture::extract(found->second,raw.model,body,skin,world,bulb,reason))
        {gEngfuncs.Con_Printf("D3D9: fixture entity %d rejected: %s\n",entity.entityIndex,reason.c_str());continue;}
        bulb.entity=entity.entityIndex;
        const auto duplicate=std::find_if(g_studioBulbs.begin(),g_studioBulbs.end(),[&](const dx_studio_fixture::Bulb& existing){return existing.model==bulb.model&&dx_light_position::distanceSquared(existing.bounds.center,bulb.bounds.center)<.25f;});
        if(duplicate==g_studioBulbs.end())g_studioBulbs.push_back(std::move(bulb));
    }
}

static float R_CenteredFixtureRadius(const remix_map_light_t& light)
{
    const dx_light_position::Point origin{light.centeredOrigin[0],light.centeredOrigin[1],light.centeredOrigin[2]};
    float radius=8.f;
    if(light.bulb<0 && light.fixture>=0 && size_t(light.fixture)>=g_studioBulbs.size()) {
        const size_t index=size_t(light.fixture)-g_studioBulbs.size();
        if(index<g_brushGlobes.size()) {
            const auto& globe=g_brushGlobes[index];
            radius=std::max(radius,globe.radius+std::sqrt(dx_light_position::distanceSquared(origin,globe.center)));
        }
    }
    if(light.bulb>=0&&size_t(light.bulb)<g_studioBulbs.size())radius=std::max(radius,dx_studio_fixture::clearance(g_studioBulbs[size_t(light.bulb)],origin));
    return radius;
}

static float R_FinalMapLightRadius(const remix_map_light_t& light)
{
    if(light.manualRadius>0.f)return light.manualRadius;
    const float base=std::isfinite(dx_remix_map_light_radius.value)?std::clamp(dx_remix_map_light_radius.value,1.f,24.f):8.f;
    // Only verified studio/outdoor brush globes may enlarge an emitter.
    return light.fixture>=0 && dx_remix_map_light_clearance.value!=0.f && dx_remix_map_light_centering.value!=0.f ? std::min(24.f,std::max(base,light.centeredRadius)) : base;
}

static std::string R_LightPositionFile()
{
	if( !g_worldmodel ) return {};
	std::string name=g_worldmodel->name;
	const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);
	const auto dot=name.find_last_of('.');if(dot!=std::string::npos)name.resize(dot);
	if(name.empty()||name.size()>63)return {};
	for(unsigned char c:name)if(!std::isalnum(c)&&c!='_'&&c!='-')return {};
	return "maps/"+name+"_light_positions.txt";
}

static void R_LoadLightPositions()
{
	const auto fs=gEngfuncs.fsapi;const auto path=R_LightPositionFile();
	if(path.empty()||!fs||!fs->Open||!fs->FileLength||!fs->Read||!fs->Close)return;
	auto file=fs->Open(path.c_str(),"rb",false);if(!file)return;
	const auto size=fs->FileLength(file);std::string text;
	if(size>0&&size<=65536){text.resize(size_t(size));if(fs->Read(file,&text[0],text.size())!=size)text.clear();}
	fs->Close(file);
	for(const auto& entry:dx_light_position::parse(text)) for(auto& light:g_remixMapLights)
	{
		const dx_light_position::Point authored{light.origin[0],light.origin[1],light.origin[2]};
		if(light.type==REMIX_MAP_LIGHT_DIRECTIONAL||!dx_light_position::applicable(entry,light.entityIndex,authored))continue;
		if(entry.radius>0)light.manualRadius=entry.radius;
		if(!entry.hasPosition)continue;
		if(!R_MapLightPositionAllowed(light.origin,entry.position.data()))continue;
		if(light.bulb>=0&&dx_studio_fixture::clearance(g_studioBulbs[size_t(light.bulb)],entry.position)>24.f){gEngfuncs.Con_Printf("D3D9: manual light %d rejected: studio clearance exceeds 24\n",light.entityIndex);continue;}
		VectorCopy(entry.position.data(),light.centeredOrigin);light.manualPosition=true;
		light.centeredRadius=R_CenteredFixtureRadius(light);
	}
}

static void R_BuildRemixMapLights( void )
{
	R_ClearRemixMapLights( );
	std::vector<map_entity_record_t> entities;
	R_ParseMapEntities( entities );
	if( entities.empty( ) )
		return;

	// Stable map-specific identities do not depend on distance-sorted D3D slots.
	g_mapEmitterIdentity = 1469598103934665603ull;
	for( const char *c = g_worldmodel->name; *c; ++c ) g_mapEmitterIdentity = (g_mapEmitterIdentity ^ static_cast<unsigned char>(*c))*1099511628211ull;
	std::vector<model_t *> fixtures;
	if( gEngfuncs.Mod_ForName ) for( const auto &entity : entities )
	{
		const auto &raw = entity.fields;
		if( raw.model[0] != '*' || (strcmp(raw.classname,"func_illusionary") && strcmp(raw.classname,"func_wall")) ) continue;
		vec3_t origin={}, angles={};
		R_ParseMapVector(raw.origin,origin); R_ParseMapVector(raw.angles,angles);
		if( VectorLength(origin) > .001f || VectorLength(angles) > .001f ) continue;
		auto model=gEngfuncs.Mod_ForName(raw.model,false,false);
		if( model && model->type==mod_brush ) fixtures.push_back(model);
	}
	std::vector<model_t *> fixtureSurfaces=fixtures;
	fixtureSurfaces.push_back(g_worldmodel);
	R_CollectStudioBulbs(entities);
	std::vector<dx_light_position::Fixture> lampFixtures;
	for(const auto& bulb:g_studioBulbs)lampFixtures.push_back(bulb.bounds);
	const auto brushFixtures=R_CollectLampFixtures(fixtureSurfaces);
    for(const auto& globe:brushFixtures)if(globe.radius>=1.f && globe.radius<=24.f)g_brushGlobes.push_back(globe);
    lampFixtures.insert(lampFixtures.end(),g_brushGlobes.begin(),g_brushGlobes.end());
	g_remixMapLights.reserve( entities.size( ) );
	for( const map_entity_record_t &entity : entities )
	{
		const map_light_debug_t &raw = entity.fields;
		if( !R_IsMapLightClassname( raw.classname ) )
			continue;
		// The forest's isolated spawn fill is intentionally absent on every load.
		if(g_worldmodel && !Q_stricmp(g_worldmodel->name,"maps/c_forest1.bsp") &&
		   entity.entityIndex==2 && !strcmp(raw.classname,"light")) continue;

		remix_map_light_t light{};
		light.entityIndex = entity.entityIndex;
		R_ParseMapVector( raw.origin, light.origin );
		if( !R_ParseMapLightEmission( raw.light, light.r, light.g, light.b, light.brightness ) )
			continue;

		light.style = raw.style[0] ? abs( atoi( raw.style ) ) : 0;
		light.style = std::max( 0, std::min( DX_LIGHTSTYLE_COUNT - 1, light.style ) );
		light.fade = raw.fade[0] ? static_cast<float>( atof( raw.fade ) ) : 1.0f;
		if( light.fade <= 0.0f )
			light.fade = 1.0f;
		light.falloff = raw.falloff[0] ? atoi( raw.falloff ) : 2;
		if( light.falloff != 1 )
			light.falloff = 2;

		light.innerCone = raw.cone[0] ? static_cast<float>( atof( raw.cone ) ) : 10.0f;
		light.outerCone = raw.cone2[0] ? static_cast<float>( atof( raw.cone2 ) ) : light.innerCone;
		light.innerCone = std::max( 0.1f, std::min( 179.0f, light.innerCone ) );
		light.outerCone = std::max( light.innerCone, std::min( 179.0f, light.outerCone ) );

		const bool isEnvironment = !strcmp( raw.classname, "light_environment" );
		const bool isSpot = !strcmp( raw.classname, "light_spot" );
		const bool isSky = raw.sky[0] && atoi( raw.sky ) != 0;

		// Only light_spot is a cone; light is omnidirectional.
		if( isEnvironment || ( isSpot && isSky ) )
			light.type = REMIX_MAP_LIGHT_DIRECTIONAL;
		else if( isSpot )
			light.type = REMIX_MAP_LIGHT_SPOT;
		else
			light.type = REMIX_MAP_LIGHT_POINT;

		if( light.type != REMIX_MAP_LIGHT_POINT &&
			!R_BuildMapLightDirection( raw, entities, light.origin, light.direction ) )
		{
			VectorSet( light.direction, 1.0f, 0.0f, 0.0f );
		}

		// Authored indoor placement is intentional; nearby textured faces alone
		// do not justify moving the source into a shade or through its opening.
		VectorCopy(light.origin,light.centeredOrigin);
		// These are closed globe fixtures even when the map author used a
		// downward light_spot. Restricting correction to point lights left
		// the same bulb off-centre with radius 8 in nightmare/apartment maps.
		if(light.type!=REMIX_MAP_LIGHT_DIRECTIONAL && !lampFixtures.empty())
		{
			const auto match=dx_light_position::locate({light.origin[0],light.origin[1],light.origin[2]},lampFixtures,24.f,true);
			if(match.fixture>=0&&R_MapLightPositionAllowed(light.origin,match.position.data()))
			{VectorCopy(match.position.data(),light.centeredOrigin);light.fixture=match.fixture;
				if(size_t(match.fixture)<g_studioBulbs.size())light.bulb=match.fixture;
				light.positionReason=light.bulb>=0?"verified studio globe":"verified outdoor brush globe";}
			else if(match.fixture>=0)light.positionReason="correction crosses world solid";
		}
		VectorCopy(light.centeredOrigin,light.automaticOrigin);
		light.centeredRadius=R_CenteredFixtureRadius(light);
		R_BindMapLightParent(light,entities);
		g_remixMapLights.push_back( light );
	}

	R_LoadLightPositions();
	int centered=0;for(const auto& light:g_remixMapLights)if(light.fixture>=0||light.manualPosition)++centered;
	gEngfuncs.Con_Printf("D3D9: lamp centering: %d local fixtures (%d studio), %d matched lights (dx_remix_map_light_centering)\n",int(lampFixtures.size()),int(g_studioBulbs.size()),centered);
	g_remixMapLightCandidates.reserve( g_remixMapLights.size( ) );
	if( !g_remixMapLights.empty( ) )
	{
		int corrected = 0;
		for( const auto &light : g_remixMapLights ) if( R_FinalMapLightRadius(light) > 8.f ) ++corrected;
		gEngfuncs.Con_Printf("D3D9: map emitter clearance: %d enlarged beyond 8 units; energy preserved\n", corrected);
		gEngfuncs.Con_Printf( "D3D9: cached %d BSP light entities for RTX Remix\n",
			static_cast<int>( g_remixMapLights.size( ) ) );
	}
}

static float R_RemixMapLightStyleScale( const remix_map_light_t &light )
{
	if( light.style < 0 || light.style >= DX_LIGHTSTYLE_COUNT )
		return 1.0f;
	return std::max( 0.0f, static_cast<float>( g_lightStyleValues[light.style] ) / 256.0f );
}

static float R_RemixMapLightDistanceSquared( const remix_map_light_t &light )
{
	const auto position=R_MapLightPosition(light);
	const float x = position[0] - g_vOrigin[0];
	const float y = position[1] - g_vOrigin[1];
	const float z = position[2] - g_vOrigin[2];
	return x * x + y * y + z * z;
}

static float R_RemixMapLightRange( const remix_map_light_t &light )
{
	const float rangeScale = std::max( 0.05f,
		std::min( 16.0f, dx_remix_map_light_range_scale.value ) );
	return std::max( 32.0f, std::min( 8192.0f,
		light.brightness * 2.0f * rangeScale / light.fade ) );
}

static const remix_map_light_t *R_FindRemixMapLight( int entityIndex )
{
	for( const remix_map_light_t &light : g_remixMapLights )
	{
		if( light.entityIndex == entityIndex )
			return &light;
	}
	return nullptr;
}

static const char *R_RemixMapLightTypeName( remix_map_light_type_t type )
{
	switch( type )
	{
	case REMIX_MAP_LIGHT_SPOT: return "spot";
	case REMIX_MAP_LIGHT_DIRECTIONAL: return "directional";
	default: return "point";
	}
}

static void R_FillRemixMapD3DLight( const remix_map_light_t &source,
	float styleScale, D3DLIGHT9 &light )
{
	light = D3DLIGHT9{};
	const float intensityScale = std::max( 0.0f,
		std::min( 16.0f, dx_remix_map_light_intensity.value ) );
	const float intensity = source.brightness / 255.0f * styleScale * intensityScale;
	light.Diffuse.r = source.r * intensity;
	light.Diffuse.g = source.g * intensity;
	light.Diffuse.b = source.b * intensity;
	light.Diffuse.a = 1.0f;

	if( source.type == REMIX_MAP_LIGHT_DIRECTIONAL )
	{
		light.Type = D3DLIGHT_DIRECTIONAL;
		light.Direction.x = source.direction[0];
		light.Direction.y = source.direction[1];
		light.Direction.z = source.direction[2];
		return;
	}

	light.Type = source.type == REMIX_MAP_LIGHT_SPOT ? D3DLIGHT_SPOT : D3DLIGHT_POINT;
	const auto position=R_MapLightPosition(source);
	light.Position.x = position[0];
	light.Position.y = position[1];
	light.Position.z = position[2];
	light.Range = R_RemixMapLightRange( source );
	light.Attenuation0 = 1.0f;
	if( source.falloff == 1 )
	{
		light.Attenuation1 = 3.0f / light.Range;
		light.Attenuation2 = 0.0f;
	}
	else
	{
		light.Attenuation1 = 0.0f;
		light.Attenuation2 = 3.0f / ( light.Range * light.Range );
	}

	if( source.type == REMIX_MAP_LIGHT_SPOT )
	{
		const auto direction=source.parentResolved?source.movingDirection:source.direction;
		light.Direction.x = direction[0];
		light.Direction.y = direction[1];
		light.Direction.z = direction[2];

		// D3D9 uses full cone apertures; Hammer stores half-angles.
		const float d3dInnerCone = std::min( 179.0f, source.innerCone * 2.0f );
		const float d3dOuterCone = std::min( 179.0f, source.outerCone * 2.0f );
		light.Theta = DEG2RAD( d3dInnerCone );
		light.Phi = DEG2RAD( d3dOuterCone );
		light.Falloff = 1.0f;
	}
}

static void R_ApplyRemixMapLights( void )
{
	if( !device || dx_remix.value == 0.0f || dx_remix_map_lights.value == 0.0f ||
		g_remixMapLights.empty( ) || dx_remix_map_light_intensity.value <= 0.0f || R_MapLightLevel() <= 0.f )
	{
		R_DisableRemixMapLights( );
		return;
	}

	R_UpdateMapLightParents();
	g_remixMapLightCandidates.clear( );
	for( size_t i = 0; i < g_remixMapLights.size( ); ++i )
	{
		remix_map_light_t &light = g_remixMapLights[i];
		if(light.parentModel[0]&&!light.parentResolved){
			light.submissionRoute="moving parent absent";g_mapEmitterBridge.retire(light.instance);continue;
		}
		if( R_RemixMapLightStyleScale( light ) <= 0.001f )
		{
			light.submissionRoute="style off";
			g_mapEmitterBridge.retire(light.instance);
			continue;
		}
		remix_map_light_candidate_t candidate{};
		candidate.lightIndex = i;
		candidate.directional = light.type == REMIX_MAP_LIGHT_DIRECTIONAL;
		candidate.distanceSquared = candidate.directional ? 0.0f : R_RemixMapLightDistanceSquared( light );
		g_remixMapLightCandidates.push_back( candidate );
	}

	std::stable_sort( g_remixMapLightCandidates.begin( ), g_remixMapLightCandidates.end( ),
		[]( const remix_map_light_candidate_t &a, const remix_map_light_candidate_t &b )
		{
			if( a.directional != b.directional )
				return a.directional;
			if( a.distanceSquared != b.distanceSquared )
				return a.distanceSquared < b.distanceSquared;
			return a.lightIndex < b.lightIndex;
		} );

	int emitted = 0;
	int apiEmitted = 0;
	for( const remix_map_light_candidate_t &candidate : g_remixMapLightCandidates )
	{
		remix_map_light_t &source = g_remixMapLights[candidate.lightIndex];
		D3DLIGHT9 light{};
		R_FillRemixMapD3DLight( source, R_RemixMapLightStyleScale( source ), light );
		const float radius = R_FinalMapLightRadius(source);
		const float level = R_MapLightLevel();
		const float outputScale = (std::isfinite(dx_remix_map_light_output_scale.value) ? std::clamp(dx_remix_map_light_output_scale.value,0.f,4.f) : .75f) * level;
		// Attached lights use per-frame D3D submission. The map API bridge
		// throttles moving definitions and can recreate them on x86, which
		// leaves a fast train's light behind between updates.
		if( !source.parentModel[0] && dx_remix_map_light_api.value != 0.f && source.type != REMIX_MAP_LIGHT_DIRECTIONAL &&
			g_mapEmitterBridge.emit(source.instance,light,radius,g_mapEmitterIdentity ^ (uint64_t(source.entityIndex+1)*0x9e3779b97f4a7c15ull),outputScale) ) { source.submissionRoute="Remix API"; ++apiEmitted; continue; }
		g_mapEmitterBridge.retire(source.instance);
		source.submissionRoute=source.parentModel[0]?"moving parent D3D":"legacy capacity/error";
		light.Diffuse.r *= level;
		light.Diffuse.g *= level;
		light.Diffuse.b *= level;
		if( g_remixEnabledLightLimit >= 0 && DX_REMIX_DYNAMIC_LIGHT_SLOTS + emitted >= g_remixEnabledLightLimit ) continue;
		const DWORD slot = static_cast<DWORD>( DX_REMIX_DYNAMIC_LIGHT_SLOTS + emitted );
		HRESULT result = device->SetLight( slot, &light );
		if( FAILED( result ) )
		{
			g_remixEnabledLightLimit = DX_REMIX_DYNAMIC_LIGHT_SLOTS + emitted;
			if( !g_remixMapLightCapacityWarned )
			{
				gEngfuncs.Con_Printf(
					"D3D9: RTX Remix accepted %d BSP lights; SetLight(%u) failed (0x%08X). "
					"Set d3d9.maxEnabledLights to at least %d in rtx.conf to allow every active map light.\n",
					emitted, slot, static_cast<unsigned int>( result ),
					g_dxFixedLightCount + static_cast<int>( g_remixMapLightCandidates.size( ) ) );
				g_remixMapLightCapacityWarned = true;
			}
			continue;
		}

		result = device->LightEnable( slot, TRUE );
		if( FAILED( result ) )
		{
			g_remixEnabledLightLimit = DX_REMIX_DYNAMIC_LIGHT_SLOTS + emitted;
			if( !g_remixMapLightCapacityWarned )
			{
				gEngfuncs.Con_Printf(
					"D3D9: RTX Remix accepted %d BSP lights; LightEnable(%u) failed (0x%08X). "
					"Set d3d9.maxEnabledLights to at least %d in rtx.conf to allow every active map light.\n",
					emitted, slot, static_cast<unsigned int>( result ),
					g_dxFixedLightCount + static_cast<int>( g_remixMapLightCandidates.size( ) ) );
				g_remixMapLightCapacityWarned = true;
			}
			device->LightEnable( slot, FALSE );
			continue;
		}
		source.submissionRoute="legacy D3D";
		++emitted;
	}

	for( int i = emitted; i < g_remixMapLightSlotCount; ++i )
		device->LightEnable( DX_REMIX_DYNAMIC_LIGHT_SLOTS + i, FALSE );
	g_remixMapLightSlotCount = emitted;
	static uint64_t previousMap=0;
	static int previousApi=-1, previousD3D=-1;
	static ULONGLONG lastReport=0;
	const ULONGLONG now=GetTickCount64();
	if( previousMap != g_mapEmitterIdentity ||
		((previousApi != apiEmitted || previousD3D != emitted) && now-lastReport >= 5000) )
	{
		gEngfuncs.Con_Printf("D3D9: map emitters active: API=%d D3D=%d\n",apiEmitted,emitted);
		previousMap=g_mapEmitterIdentity;previousApi=apiEmitted;previousD3D=emitted;
		lastReport=now;
	}
}

static void R_PrintMapLight( const map_light_debug_t &light,
	int entityIndex, int lightIndex )
{
	const char *origin = light.origin[0] ? light.origin : "0 0 0";
	const char *style = light.style[0] ? light.style : "0";
	float red = 0.0f, green = 0.0f, blue = 0.0f, brightness = 0.0f;
	const int values = light.light[0]
		? std::sscanf( light.light, "%f %f %f %f",
			&red, &green, &blue, &brightness ) : 0;

	if( values >= 4 )
	{
		gEngfuncs.Con_Printf(
			"[%3d] ent %d %-18s origin=(%s) rgb=(%.0f %.0f %.0f) brightness=%.1f style=%s\n",
			lightIndex, entityIndex, light.classname, origin,
			red, green, blue, brightness, style );
	}
	else if( values == 1 )
	{
		gEngfuncs.Con_Printf(
			"[%3d] ent %d %-18s origin=(%s) rgb=(255 255 255) brightness=%.1f style=%s\n",
			lightIndex, entityIndex, light.classname, origin, red, style );
	}
	else if( values == 3 )
	{
		gEngfuncs.Con_Printf(
			"[%3d] ent %d %-18s origin=(%s) rgb=(%.0f %.0f %.0f) brightness=<RGB only> style=%s\n",
			lightIndex, entityIndex, light.classname, origin,
			red, green, blue, style );
	}
	else
	{
		gEngfuncs.Con_Printf(
			"[%3d] ent %d %-18s origin=(%s) _light=\"%s\" style=%s\n",
			lightIndex, entityIndex, light.classname, origin,
			light.light[0] ? light.light : "<unset>", style );
	}

	if( light.targetname[0] || light.target[0] || light.angles[0] ||
		light.angle[0] || light.pitch[0] || light.cone[0] || light.cone2[0] )
	{
		gEngfuncs.Con_Printf(
			"      targetname=\"%s\" target=\"%s\" angles=\"%s\" angle=%s pitch=%s cone=%s cone2=%s\n",
			light.targetname[0] ? light.targetname : "-",
			light.target[0] ? light.target : "-",
			light.angles[0] ? light.angles : "-",
			light.angle[0] ? light.angle : "-",
			light.pitch[0] ? light.pitch : "-",
			light.cone[0] ? light.cone : "-",
			light.cone2[0] ? light.cone2 : "-" );
	}

	if( light.fade[0] || light.falloff[0] || light.sky[0] || light.spread[0] ||
		light.pattern[0] || light.spawnflags[0] || light.diffuseLight[0] ||
		light.diffuseLight2[0] )
	{
		gEngfuncs.Con_Printf(
			"      fade=%s falloff=%s sky=%s spread=%s pattern=\"%s\" spawnflags=%s diffuse=\"%s\" diffuse2=\"%s\"\n",
			light.fade[0] ? light.fade : "-",
			light.falloff[0] ? light.falloff : "-",
			light.sky[0] ? light.sky : "-",
			light.spread[0] ? light.spread : "-",
			light.pattern[0] ? light.pattern : "-",
			light.spawnflags[0] ? light.spawnflags : "-",
			light.diffuseLight[0] ? light.diffuseLight : "-",
			light.diffuseLight2[0] ? light.diffuseLight2 : "-" );
	}

	const remix_map_light_t *remixLight = R_FindRemixMapLight( entityIndex );
	if( remixLight )
	{
		gEngfuncs.Con_Printf( "      remix=%s styleScale=%.3f",
			R_RemixMapLightTypeName( remixLight->type ),
			R_RemixMapLightStyleScale( *remixLight ) );
		if( remixLight->type != REMIX_MAP_LIGHT_POINT )
		{
			gEngfuncs.Con_Printf( " dir=(%.3f %.3f %.3f)",
				remixLight->direction[0], remixLight->direction[1], remixLight->direction[2] );
		}
		if( remixLight->type != REMIX_MAP_LIGHT_DIRECTIONAL )
		{
			gEngfuncs.Con_Printf(" position=(%.2f %.2f %.2f) source=%s",R_MapLightPosition(*remixLight)[0],R_MapLightPosition(*remixLight)[1],R_MapLightPosition(*remixLight)[2],remixLight->manualPosition?"manual":(remixLight->fixture>=0?"fixture":"authored"));
            gEngfuncs.Con_Printf(" radius=%.3f radiusSource=%s route=%s reason=%s",R_FinalMapLightRadius(*remixLight),remixLight->manualRadius>0?"manual":(remixLight->fixture>=0?"verified-profile":"default"),remixLight->submissionRoute,dx_remix_map_light_centering.value==0?"centering disabled":(remixLight->manualPosition?"manual override":remixLight->positionReason));
            if(remixLight->bulb>=0){const auto& bulb=g_studioBulbs[size_t(remixLight->bulb)];gEngfuncs.Con_Printf(" fixture=%s body=%d skin=%d entity=%d",bulb.model.c_str(),bulb.body,bulb.skin,bulb.entity);}
			gEngfuncs.Con_Printf( " range=%.1f attenuation=%s",
				R_RemixMapLightRange( *remixLight ),
				remixLight->falloff == 1 ? "linear" : "inverse-square" );
			if( remixLight->type == REMIX_MAP_LIGHT_SPOT )
				gEngfuncs.Con_Printf( " cone=(%.1f %.1f)",
					remixLight->innerCone, remixLight->outerCone );
		}
		gEngfuncs.Con_Printf( "\n" );
	}
}

static void R_ListMapLightsCmd( void )
{
	if( !g_worldmodel || !g_worldmodel->entities || !g_worldmodel->entities[0] )
	{
		gEngfuncs.Con_Printf( "D3D9: no loaded BSP entity data\n" );
		return;
	}

	gEngfuncs.Con_Printf( "D3D9: map lights for %s\n", g_worldmodel->name );
	std::vector<map_entity_record_t> entities;
	R_ParseMapEntities( entities );
	int lightCount = 0;
	for( const map_entity_record_t &entity : entities )
	{
		if( R_IsMapLightClassname( entity.fields.classname ) )
			R_PrintMapLight( entity.fields, entity.entityIndex, ++lightCount );
	}

	gEngfuncs.Con_Printf( "D3D9: %d map light entit%s found\n",
		lightCount, lightCount == 1 ? "y" : "ies" );
}

static void R_LightPositionCmd()
{
	if(!gEngfuncs.Cmd_Argc||!gEngfuncs.Cmd_Argv)return;
	const int argc=gEngfuncs.Cmd_Argc();
	if(argc<2){gEngfuncs.Con_Printf("Usage: dx_light_position <entity> [x y z | auto | authored]\nUse dx_listlights for entity IDs; dx_light_positions_save persists manual edits.\n");return;}
	char *end=nullptr;const long entity=std::strtol(gEngfuncs.Cmd_Argv(1),&end,10);
	if(!end||end==gEngfuncs.Cmd_Argv(1)||*end||entity<0||entity>1000000)return;
	auto found=std::find_if(g_remixMapLights.begin(),g_remixMapLights.end(),[&](const remix_map_light_t& light){return light.entityIndex==entity;});
	if(found==g_remixMapLights.end()||found->type==REMIX_MAP_LIGHT_DIRECTIONAL){gEngfuncs.Con_Printf("No local BSP light for entity %ld\n",entity);return;}
	auto& light=*found;
	if(argc==3&&!strcmp(gEngfuncs.Cmd_Argv(2),"auto")){VectorCopy(light.automaticOrigin,light.centeredOrigin);light.manualPosition=false;}
	else if(argc==3&&!strcmp(gEngfuncs.Cmd_Argv(2),"authored")){VectorCopy(light.origin,light.centeredOrigin);light.manualPosition=true;}
	else if(argc==5)
	{
		dx_light_position::Override entry;entry.entity=int(entity);entry.authored={light.origin[0],light.origin[1],light.origin[2]};
		for(int i=0;i<3;++i){const auto arg=gEngfuncs.Cmd_Argv(i+2);entry.position[i]=std::strtof(arg,&end);if(!end||end==arg||*end)return;}
		if(!dx_light_position::applicable(entry,int(entity),entry.authored)||!R_MapLightPositionAllowed(light.origin,entry.position.data())||
            (light.bulb>=0&&dx_studio_fixture::clearance(g_studioBulbs[size_t(light.bulb)],entry.position)>24.f))
		{gEngfuncs.Con_Printf("Position rejected: nonfinite, over 256 units from authored origin, crossing world solid, or studio clearance over 24.\n");return;}
		VectorCopy(entry.position.data(),light.centeredOrigin);light.manualPosition=true;
	}
	else if(argc!=2){gEngfuncs.Con_Printf("Usage: dx_light_position <entity> [x y z | auto | authored]\n");return;}
	light.centeredRadius=R_CenteredFixtureRadius(light);
	gEngfuncs.Con_Printf("Light %d: authored %.3f %.3f %.3f; corrected %.3f %.3f %.3f; radius %.3f; %s\n",light.entityIndex,light.origin[0],light.origin[1],light.origin[2],light.centeredOrigin[0],light.centeredOrigin[1],light.centeredOrigin[2],R_FinalMapLightRadius(light),light.manualPosition?"manual":"automatic");
}

static void R_LightRadiusCmd() {
 if(!gEngfuncs.Cmd_Argc||!gEngfuncs.Cmd_Argv)return;
 const int argc=gEngfuncs.Cmd_Argc();
 if(argc<2||argc>3){gEngfuncs.Con_Printf("Usage: dx_light_radius <entity> [1..24 | auto]\n");return;}
 char* end=nullptr;const char* arg=gEngfuncs.Cmd_Argv(1);
 const long entity=std::strtol(arg,&end,10);
 if(end==arg||*end||entity<0||entity>1000000)return;
 auto found=std::find_if(g_remixMapLights.begin(),g_remixMapLights.end(),[&](const remix_map_light_t& light){return light.entityIndex==entity;});
 if(found==g_remixMapLights.end()||found->type==REMIX_MAP_LIGHT_DIRECTIONAL){gEngfuncs.Con_Printf("No local BSP light for entity %ld\n",entity);return;}
 if(argc==3) {
  arg=gEngfuncs.Cmd_Argv(2);
  if(!strcmp(arg,"auto"))found->manualRadius=0.f;
  else {
   const float value=std::strtof(arg,&end);
   if(end==arg||*end||!std::isfinite(value)||value<1.f||value>24.f){gEngfuncs.Con_Printf("Radius must be a finite value from 1 to 24.\n");return;}
   found->manualRadius=value;
  }
 }
 gEngfuncs.Con_Printf("Light %ld: emitter radius %.3f (%s); position unchanged. dx_light_positions_save persists edits.\n",entity,R_FinalMapLightRadius(*found),found->manualRadius>0?"manual":"automatic");
}

static void R_SaveLightPositionsCmd()
{
	const auto fs=gEngfuncs.fsapi;const auto path=R_LightPositionFile();
	if(path.empty()||!fs||!fs->WriteFile)return;
	std::ostringstream text;text.imbue(std::locale::classic());text.precision(9);
	text<<"# entity authored_x authored_y authored_z corrected_x corrected_y corrected_z\n# radius entity authored_x authored_y authored_z radius_units\n";
	int count=0;
	for(const auto& light:g_remixMapLights)if(light.manualPosition||light.manualRadius>0)
	{
		if(++count>256){gEngfuncs.Con_Printf("Too many manual corrections; not saved.\n");return;}
        if(light.manualPosition) {
            text<<light.entityIndex;
            for(float x:light.origin)text<<' '<<x;
            for(float x:light.centeredOrigin)text<<' '<<x;
            text<<'\n';
        }
        if(light.manualRadius>0) {
            text<<"radius "<<light.entityIndex;
            for(float x:light.origin)text<<' '<<x;
            text<<' '<<light.manualRadius<<'\n';
        }
	}
	const auto contents=text.str();
	const bool saved=fs->WriteFile(path.c_str(),contents.data(),fs_offset_t(contents.size()))!=0;
	gEngfuncs.Con_Printf("%s %d manual light overrides: %s\n",saved?"Saved":"Failed to save",count,path.c_str());
}

static void R_CofShadowStatusCmd();
static void R_RegisterContextCommands( void )
{
	if( g_contextCommandsRegistered )
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
	gEngfuncs.Cmd_AddCommand( "dx_listlights", R_ListMapLightsCmd,
		"List BSP light entities and their key lighting data" );
	gEngfuncs.Cmd_AddCommand("dx_light_radius",R_LightRadiusCmd,"Inspect or change one BSP light emitter radius independently of position");
	gEngfuncs.Cmd_AddCommand("dx_light_position",R_LightPositionCmd,"Inspect or correct a BSP light position");
	gEngfuncs.Cmd_AddCommand("dx_light_positions_save",R_SaveLightPositionsCmd,"Save per-map position and radius overrides without editing the BSP");
	gEngfuncs.Cmd_AddCommand("dx_cof_shadow_status",R_CofShadowStatusCmd,"Inspect local Simon shadow gates and submission");
	gEngfuncs.Cmd_AddCommand("dx_cof_pickup_status",R_CofPickupStatusCmd,"Inspect aimed pickup outline without world lighting");
	gEngfuncs.Cmd_AddCommand("dx_cof_shadow_probe",R_CofShadowProbeCmd,"Isolate box, CPU Simon or skinned Simon 96 units ahead");
	gEngfuncs.Cmd_AddCommand("dx_cof_shadow_trace",R_CofShadowTraceCmd,"Log the next 32 shadow submissions");
	g_contextCommandsRegistered = true;
}

static void R_RemoveContextCommands( void )
{
	if( !g_contextCommandsRegistered )
		return;

	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_in" );
	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_out" );
	gEngfuncs.Cmd_RemoveCommand( "dx_showtextures_zoom_reset" );
	gEngfuncs.Cmd_RemoveCommand( "dx_listlights" );
	gEngfuncs.Cmd_RemoveCommand("dx_light_radius");
	gEngfuncs.Cmd_RemoveCommand("dx_light_position");
	gEngfuncs.Cmd_RemoveCommand("dx_light_positions_save");
	gEngfuncs.Cmd_RemoveCommand("dx_cof_shadow_status");
	gEngfuncs.Cmd_RemoveCommand("dx_cof_pickup_status");
	gEngfuncs.Cmd_RemoveCommand("dx_cof_shadow_probe");
	gEngfuncs.Cmd_RemoveCommand("dx_cof_shadow_trace");
	g_contextCommandsRegistered = false;
}

static void R_RegisterContextCvars( void )
{
	if( g_contextCvarsRegistered )
		return;

	gEngfuncs.Cvar_RegisterVariable( &dx_remix );
 gEngfuncs.Cvar_RegisterVariable( &dx_cof_player_shadow );
 gEngfuncs.Cvar_RegisterVariable( &dx_cof_pickup_outline );
	DX_RagdollRegister();
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_decal_cutout );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_decal_alpha_ref );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_glass_single_sided );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_glass_batch );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_stable_sprites );
	gEngfuncs.Cvar_RegisterVariable( &dx_triapi_batch );
	gEngfuncs.Cvar_RegisterVariable( &dx_triapi_debug );
	gEngfuncs.Cvar_RegisterVariable( &dx_triapi_batch_max_vertices );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_lights );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_api );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_radius );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_clearance );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_output_scale );
	gEngfuncs.Cvar_RegisterVariable( &light_level );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_centering );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_intensity );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_map_light_range_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_fog );
	gEngfuncs.Cvar_RegisterVariable( &dx_msaa_samples );
	gEngfuncs.Cvar_RegisterVariable( &dx_msaa_quality );
	gEngfuncs.Cvar_RegisterVariable( &dx_anisotropy );
	gEngfuncs.Cvar_RegisterVariable( &dx_showtextures_zoom );
	gEngfuncs.Cvar_RegisterVariable( &dx_debug_text3d_test );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_fov );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_near );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_x );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_y );
	gEngfuncs.Cvar_RegisterVariable( &r_viewmodel_offset_z );
	gEngfuncs.Cvar_RegisterVariable( &dx_viewmodel_beam_offset );
	gEngfuncs.Cvar_RegisterVariable( &dx_viewmodel_attachment_fov );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_shell_unstick );
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
// The historical float****/float*** API signatures carry FLAT matrix storage.
// Native clients cast the returned address to [MAXSTUDIOBONES][3][4], not a
// pointer chain (see ref/gl/gl_studio.c). All four writable regions must be
// independent: native dual-wield pose setup uses them outside our draw path.
alignas(16) static float g_studioApiBoneTransform[MAXSTUDIOBONES][3][4]{};
alignas(16) static float g_studioApiLightTransform[MAXSTUDIOBONES][3][4]{};
alignas(16) static float g_studioApiAliasTransform[3][4] =
{
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f }
};
alignas(16) static float g_studioApiRotationMatrix[3][4] =
{
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f }
};

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

static float ****R_StudioAPIGetBoneTransform( void ) { return reinterpret_cast<float ****>(g_studioApiBoneTransform); }
static float ****R_StudioAPIGetLightTransform( void ) { return reinterpret_cast<float ****>(g_studioApiLightTransform); }
static float ***R_StudioAPIGetAliasTransform( void ) { return reinterpret_cast<float ***>(g_studioApiAliasTransform); }
static float ***R_StudioAPIGetRotationMatrix( void ) { return reinterpret_cast<float ***>(g_studioApiRotationMatrix); }

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
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
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
static byte R_APILightToTexGamma( byte color )
{
	if( gp_host && FBitSet( gp_host->features, ENGINE_LINEAR_GAMMA_SPACE ) )
		return color;

	const auto *lightGammaTable = reinterpret_cast<const uint16_t *>(
		ENGINE_GET_PARM( PARM_GET_LIGHTGAMMATABLE_PTR ) );
	if( !lightGammaTable )
		return color;

	return static_cast<byte>( lightGammaTable[static_cast<unsigned int>( color ) << 2] >> 2 );
}
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
	bool gunSmoke = false;
	bool lakeGrass = false;
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
#include "dx_triapi_trace.h"
static dx_triangle_batch_t<brush_vertex_t> g_triangleBatch;
static bool g_triangleBatchScope=false, g_triangleBatchFlushing=false;
struct tri_batch_stats_t {
    unsigned sourceDraws=0,outputDraws=0,sourceVertices=0,triangles=0,failures=0;
    unsigned flushes[DX_TRI_FLUSH_COUNT]{};
};
static tri_batch_stats_t g_triBatchStats;

void DX_TriBatchBarrier(int reason) {
    if(g_triangleBatchFlushing || g_triangleBatch.vertices.empty()) return;
    g_triangleBatchFlushing=true;
    if(reason>=0 && reason<DX_TRI_FLUSH_COUNT) ++g_triBatchStats.flushes[reason];
    if(device) {
        D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
        device->SetTransform(D3DTS_WORLD,&identity); device->SetFVF(BRUSH_FVF);
        const UINT count=static_cast<UINT>(g_triangleBatch.vertices.size());
        if(R_DrawScratch(D3DPT_TRIANGLELIST,count/3,count,g_triangleBatch.vertices.data(),sizeof(brush_vertex_t))) {
            ++g_triBatchStats.outputDraws;
            DX_ProfileEvent(DX_PROFILE_EVENT_TRI_BATCH_DRAWN,1);
        } else ++g_triBatchStats.failures;
    }
    g_triangleBatch.vertices.clear(); g_triangleBatchFlushing=false;
}
static void R_ResetTriangleBatch() {
    R_TriTrace("reset");
    g_triangleBatch.vertices.clear(); g_triangleBatchScope=false; g_triApi.active=false;
}
static void R_TriBatchStats() {
    const auto& v=g_triBatchStats;
    gEngfuncs.Con_Printf("D3D9 TriAPI batch: enabled=%d maxVerts=%u sourceDraws=%u outputDraws=%u sourceVerts=%u triangles=%u failures=%u\n",
        dx_triapi_batch.value!=0.f,static_cast<unsigned>(DX_BatchVertexLimit(dx_triapi_batch_max_vertices.value)),
        v.sourceDraws,v.outputDraws,v.sourceVertices,v.triangles,v.failures);
    gEngfuncs.Con_Printf("saved submissions: %.2f%%\n",v.sourceDraws ?
        100.0*(1.0-static_cast<double>(v.outputDraws)/v.sourceDraws):0.0);
    gEngfuncs.Con_Printf("flushes: state=%u texture=%u incompatible=%u capacity=%u pass=%u\n",
        v.flushes[0],v.flushes[1],v.flushes[2],v.flushes[3],v.flushes[4]);
}


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
	R_TriTrace("begin-enter");
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

	if(g_2dModeActive || mode==TRI_POINTS || mode==TRI_LINES) DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
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
	R_TriTrace("begin-ready");
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
	R_TriTrace("end");
	if( !g_triApi.active || !device )
	{
		g_triApi.active = false;
		return;
	}
	g_triApi.active = false;
	if((g_triApi.gunSmoke||g_triApi.lakeGrass) && !g_triApi.screenSpace)return;

	const UINT vertexCount = static_cast<UINT>( R_TriVertexCount( ) );
	if( vertexCount == 0 )
		return;

    if(g_triangleBatchScope) {
        ++g_triBatchStats.sourceDraws; g_triBatchStats.sourceVertices+=vertexCount;
        unsigned triangles=0;
        const bool supported=DX_Triangulate(g_triApi.primitiveMode,vertexCount,[&](size_t,size_t,size_t){++triangles;});
        g_triBatchStats.triangles+=triangles;
        DX_ProfileEvent(DX_PROFILE_EVENT_TRI_SOURCE_DRAW,1);
        if(dx_triapi_batch.value!=0.f && !g_triApi.screenSpace && supported) {
            D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
            device->SetTransform(D3DTS_WORLD,&identity);
            g_triangleBatch.append(g_triApi.primitiveMode,g_triApi.vertices.data(),vertexCount,
                DX_BatchVertexLimit(dx_triapi_batch_max_vertices.value),[]{DX_TriBatchBarrier(DX_TRI_FLUSH_CAPACITY);});
            return;
        }
        ++g_triBatchStats.outputDraws;
    }
    DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);

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
			if(g_triApi.screenQuadCount<0 || g_triApi.screenQuadCount>=4)
				R_TriTrace("invalid-quad-index-before-write",true);
			g_triApi.screenQuad[g_triApi.screenQuadCount++] = vertex;
			if( g_triApi.screenQuadCount == 4 )
			{
				R_TriTrace("quad-commit");
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
			R_TriTrace("ui-vertex");
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
 g_triApi.gunSmoke=spriteModel&&R_CofGunSmokeReplace(spriteModel->name);
 g_triApi.lakeGrass=R_LakeForestReplacesSprite(spriteModel);
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
	DX_TriBatchBarrier(DX_TRI_FLUSH_STATE);

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
	DX_TriBatchBarrier(DX_TRI_FLUSH_STATE);
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

	if( detected )
	{
		if( gEngfuncs.Cvar_SetValue )
		{
			gEngfuncs.Cvar_SetValue( "r_nolm", 1.0f );
			gEngfuncs.Cvar_SetValue( "m_rawinput", 0.0f );
		}
		else
		{
			r_nolm.value = 1.0f;
		}
	}

	gEngfuncs.Con_Printf( "D3D9: RTX Remix %s (dx_remix %d%s)\n",
		detected ? "detected" : "not detected", detected ? 1 : 0,
		detected ? ", r_nolm=1; GPU skinning follows user setting; m_rawinput=0; glow entities disabled" : "" );
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

static void (__cdecl *g_inventoryResetCallback)()=nullptr;
extern "C" __declspec(dllexport) void __cdecl DX_SetInventoryResetCallbackV1(void (__cdecl *callback)()) {
    if(g_inventoryResetCallback && g_inventoryResetCallback!=callback)g_inventoryResetCallback();
    g_inventoryResetCallback=callback;
}
static bool DX_ResetDevice( bool rebuildPresentParams, const char *reason )
{
	if(g_inventoryResetCallback)g_inventoryResetCallback();
	R_ResetCofPlayerVisibility();
	g_cofDual.clear();
	R_ResetTriangleBatch();
	R_ResetCofLights();
	R_RetireViewmodelFlashes(nullptr);
	if( !device )
		return false;

	if( rebuildPresentParams )
		DX_SetupPresent( true );

	if( g_dxLine ) g_dxLine->OnLostDevice( );
	R_FreeScratchBuffers( );
	R_ModelsLostDevice( );
	DX_InvalidateTextureBindCache( );

	DX_RagdollDeviceReset(reason);
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
	DX_UpdateTextureQuality( true );
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
 R_CofGunSmokeInit();
 R_LakeReedsInit();
 R_LakeForestInit();
	gEngfuncs.Cmd_AddCommand("dx_triapi_batch_stats",R_TriBatchStats,"Show transparent TriangleAPI batch counters");
	R_RegisterContextCommands( );
	DX_UpdateRemix( );

	if( FAILED( D3DXCreateLine( device, &g_dxLine ) ) )
		g_dxLine = nullptr;

	DX_RefreshFogCaps( );
	DX_ResetFog( );
	DX_UpdateTextureQuality( true );
	return true;
}

static void R_Shutdown( void )
{
 R_CofGunSmokeShutdown();
 R_LakeReedsShutdown();
 R_LakeBedReset();
 R_LakeForestShutdown();
	if(g_inventoryResetCallback)g_inventoryResetCallback();
	g_mapSky.invalidate();
	g_mapSkyRetryAt = 0;
	R_ClearRemixMapLights();
	DX_RagdollReset("renderer-shutdown");
	R_ResetCofPlayerVisibility();
	g_cofDual.clear();
	R_ResetTriangleBatch();
	R_RemoveContextCommands( );
	DX_ProfilerRemoveCommands( );
	gEngfuncs.Cmd_RemoveCommand("dx_triapi_batch_stats");
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

// Versioned ABI avoids private global RVAs when this renderer is rebuilt.
extern "C" __declspec(dllexport) int __cdecl DX_InventoryDeviceV1(
	IDirect3DDevice9 **outDevice, bool **outSceneActive )
{
	if( !outDevice || !outSceneActive ) return 0;
	*outDevice = device;
	*outSceneActive = &g_dxSceneActive;
	return 1;
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

#include "r_efx.h"
// CoF's server spawns casings at eye + forward*80 (+ right*20), which is
// under the floor once the player aims down (or behind a wall when pressed
// against it), so the casing is born inside solid and never seen. The first
// time a casing temp entity is submitted, pull it back along the eye ray to
// just in front of whatever world surface blocks that ray. The client's
// temp-entity physics integrates entity.origin, so this moves the casing.
static void R_CofUnstickShell( cl_entity_s *clent, int type )
{
	if( type != ET_TEMPENTITY || dx_cof_shell_unstick.value == 0.0f || !gEngfuncs.CL_TraceLine ||
		clent->model->type != mod_studio || !Q_stristr( clent->model->name, "shell" ) )
		return;
	TEMPENTITY *temp = reinterpret_cast<TEMPENTITY *>(
		reinterpret_cast<byte *>( clent ) - offsetof( TEMPENTITY, entity ) );
	// One check per spawn: pool slots are reused, but never with the same die time.
	static std::unordered_map<const TEMPENTITY *, float> seen;
	auto found = seen.find( temp );
	if( found != seen.end() && found->second == temp->die )
		return;
	if( seen.size() > 4096 )
		seen.clear();
	seen[temp] = temp->die;
	vec3_t eye, target, delta;
	VectorCopy( g_vOrigin, eye );
	VectorCopy( clent->origin, target );
	VectorSubtract( target, eye, delta );
	if( DotProduct( delta, delta ) > 256.0f * 256.0f )
		return; // not a first-person casing
	const pmtrace_t trace = gEngfuncs.CL_TraceLine( eye, target, PM_WORLD_ONLY );
	if( trace.startsolid || trace.allsolid || trace.fraction >= 1.0f )
		return;
	vec3_t fixed;
	VectorMA( trace.endpos, 2.0f, trace.plane.normal, fixed );
	VectorCopy( fixed, clent->origin );
	VectorCopy( fixed, clent->curstate.origin );
	VectorCopy( fixed, clent->prevstate.origin );
	VectorCopy( fixed, clent->latched.prevorigin );
}

static qboolean R_AddEntity( cl_entity_s *clent, int type )
{
	if( !clent || !clent->model )
		return false;
	R_CofUnstickShell( clent, type );
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

	if(R_CofGunSmokeReplace(clent->model->name))return true;
	const int modelType = clent->model->type;
	if( modelType != mod_brush &&
		modelType != mod_studio &&
		modelType != mod_sprite )
	{
		return true;
	}

	if( ( clent->curstate.effects & ( EF_BRIGHTLIGHT | EF_DIMLIGHT | EF_LIGHT ) ) != 0 || R_CofLanternModel(clent,false) ||
  (cof_features::flareModel(clent->model->name,false)&&clent->curstate.renderfx==165) ||
  g_cofPickups.contains(clent->index,clent->curstate.modelindex,R_CurrentRenderTime(0.f)) )
	{
		g_entityLightEmitters.push_back( clent );
		++g_entityLightEmitterCount;
	}

	const int effectiveRenderMode = modelType == mod_studio
		? R_StudioRenderMode( clent )
		: clent->curstate.rendermode;

	const bool nightmareHeadlight = R_CofClientPresent() && g_worldmodel &&
        !Q_stricmp(g_worldmodel->name,"maps/c_nightmare.bsp") && modelType==mod_sprite &&
        !Q_stricmp(clent->model->name,"sprites/glow03.spr");
	if( dx_remix.value != 0.0f && effectiveRenderMode == kRenderGlow && !nightmareHeadlight )
		return false;

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
	DX_TriBatchBarrier(DX_TRI_FLUSH_STATE);
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

// Called before MainUI draws its black scrim and sharp text.
extern "C" __declspec(dllexport) int __cdecl DX_CofPauseBackgroundV1()
{
	if( !device || !g_dxSceneActive ) return 0;
	DX_TriBatchBarrier( DX_TRI_FLUSH_STATE );
	R_Flush2DBatch();
	const HRESULT result = cof_pause::blurBackground( device, g_dxSceneActive );
	DX_InvalidateD3DStateCache();
	DX_InvalidateTextureBindCache();
	return result == S_OK ? 1 : 0;
}

static void FillRGBA( int rendermode, float x, float y, float w, float h, byte r, byte g, byte b, byte a )
{
	if( !device || w <= 0.0f || h <= 0.0f )
		return;

	R_Flush2DBatch( );
	DX_PROFILE_SCOPE( profile2dFillScope, DX_PROFILE_2D );
	R_Set2DMode( true );
	R_Apply2DNeutralState();
	DX_SetRenderStateCached(D3DRS_BLENDOP,D3DBLENDOP_ADD);
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

static bool R_ProjectWorldPoint( const vec3_t world, D3DXVECTOR3 &projected )
{
	if( !device || !world || !g_sceneViewportValid ||
		g_sceneViewport.Width == 0 || g_sceneViewport.Height == 0 )
	{
		return false;
	}

	D3DXMATRIX identity;
	D3DXMatrixIdentity( &identity );
	const D3DXVECTOR3 point( world[0], world[1], world[2] );
	D3DXVec3Project( &projected, &point, &g_sceneViewport,
		&g_sceneProjection, &g_sceneView, &identity );
	return true;
}

static bool R_WorldPointBehindView( const vec3_t world )
{
	vec3_t delta;
	VectorSubtract( world, g_vOrigin, delta );
	return DotProduct( delta, g_vForward ) <= 0.001f;
}

static int WorldToScreen( const vec3_t world, vec3_t screen )
{
	if( !world || !screen )
		return true;

	D3DXVECTOR3 projected;
	if( !R_ProjectWorldPoint( world, projected ) )
		return true;

	const float viewportX = static_cast<float>( g_sceneViewport.X );
	const float viewportY = static_cast<float>( g_sceneViewport.Y );
	const float viewportWidth = static_cast<float>( g_sceneViewport.Width );
	const float viewportHeight = static_cast<float>( g_sceneViewport.Height );
	screen[0] = ( ( projected.x - viewportX ) / viewportWidth ) * 2.0f - 1.0f;
	screen[1] = 1.0f - ( ( projected.y - viewportY ) / viewportHeight ) * 2.0f;
	screen[2] = 0.0f;
	return R_WorldPointBehindView( world ) ||
		projected.z < 0.0f || projected.z > 1.0f;
}

void R_DebugText3D( const vec3_t world, const rgba_t color, const char *text )
{
	if( !world || !text || !text[0] ||
		FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP | RF_DRAW_OVERVIEW ) )
	{
		return;
	}

	if( g_debugText3D.size( ) >= DX_DEBUG_TEXT_3D_MAX )
		return;

	dx_debug_text_3d_t entry;
	VectorCopy( world, entry.world );
	if( color )
		memcpy( entry.color, color, sizeof( entry.color ) );
	entry.text = text;
	g_debugText3D.push_back( std::move( entry ) );
}

void R_DebugText3DPrintf( const vec3_t world, const rgba_t color,
	const char *format, ... )
{
	if( !format || !format[0] )
		return;

	char text[512];
	va_list args;
	va_start( args, format );
	Q_vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	R_DebugText3D( world, color, text );
}

static void R_FlushDebugText3D( void )
{
	if( g_debugText3D.empty( ) )
		return;

	if( !gEngfuncs.Con_DrawString || !g_sceneViewportValid ||
		FBitSet( g_currentViewFlags, RF_DRAW_CUBEMAP | RF_DRAW_OVERVIEW ) )
	{
		g_debugText3D.clear( );
		return;
	}

	for( const dx_debug_text_3d_t &entry : g_debugText3D )
	{
		D3DXVECTOR3 projected;
		if( !R_ProjectWorldPoint( entry.world, projected ) ||
			R_WorldPointBehindView( entry.world ) ||
			projected.z < 0.0f || projected.z > 1.0f )
		{
			continue;
		}

		const float minX = static_cast<float>( g_sceneViewport.X );
		const float minY = static_cast<float>( g_sceneViewport.Y );
		const float maxX = minX + static_cast<float>( g_sceneViewport.Width );
		const float maxY = minY + static_cast<float>( g_sceneViewport.Height );
		if( projected.x < minX || projected.x >= maxX ||
			projected.y < minY || projected.y >= maxY )
		{
			continue;
		}

		int textWidth = 0;
		int textHeight = 0;
		if( gEngfuncs.Con_DrawStringLen )
			gEngfuncs.Con_DrawStringLen( entry.text.c_str( ), &textWidth, &textHeight );

		const int x = static_cast<int>( projected.x ) - textWidth / 2;
		const int y = static_cast<int>( projected.y ) - textHeight / 2;
		gEngfuncs.Con_DrawString( x, y, entry.text.c_str( ), entry.color );
	}

	g_debugText3D.clear( );
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
	const D3DXMATRIX savedSceneView = g_sceneView;
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

		DX_TriBatchBarrier(DX_TRI_FLUSH_STATE);
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
	g_sceneView = savedSceneView;
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
		position[component] = map < 0 ? -value : value;
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
	// Keep the sky mesh camera-independent; only its instance follows the eye.
	D3DXMATRIX skyWorld;
	D3DXMatrixTranslation( &skyWorld, g_vOrigin[0], g_vOrigin[1], g_vOrigin[2] );
	device->SetTransform( D3DTS_WORLD, &skyWorld );
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

static cl_entity_s* R_CofHandheldViewModel(bool* mirrored=nullptr) {
 if(mirrored)*mirrored=false;
 const auto flashlight=[](const cl_entity_s* ent){return ent&&ent->model&&ent->model->type==mod_studio&&
  !(ent->curstate.effects&EF_NODRAW)&&!Q_stricmp(ent->model->name,"models/weapons/flashlight/v_flashlight.mdl");};
 auto ent=R_CurrentViewModel();if(flashlight(ent)){if(mirrored)*mirrored=R_CofPrimaryMirrored(R_CurrentRenderTime(0.f));return ent;}
 ent=g_cofDual.current(R_CurrentRenderTime(0.f));
 if(!flashlight(ent))return nullptr;
 if(mirrored)*mirrored=g_cofDual.mirror;
 return ent;
}
bool R_CofHandheldLightEquipped(){return R_CofHandheldViewModel()!=nullptr;}
bool R_CofHandheldLightPose(dx_cof_light_state_v2& pose) {
 bool mirrored=false;auto ent=R_CofHandheldViewModel(&mirrored);
 if(!ent)return false;
 float points[6],offsets[3]={
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_x.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_y.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_z.value))};
 if(!R_StudioViewModelLightAttachments(ent,R_CurrentRenderTime(pose.time),g_vOrigin,g_vAngles,offsets,points,mirrored))return false;
 for(int i=0;i<3;++i){pose.origin[i]=points[i];pose.direction[i]=points[3+i]-points[i];}
 return DX_CofValid(pose);
}

bool R_CofPhoneLightActive(unsigned flags) {
 const auto ent=R_CurrentViewModel();
 return !(flags&(DX_COF_HANDHELD|DX_COF_WEAPON)) &&
  (flags&(DX_COF_MAIN|DX_COF_REDUCED)) && ent && ent->model &&
  !Q_stricmp(ent->model->name,"models/weapons/mobile/v_mobile.mdl");
}
bool R_CofPhoneLightPose(dx_cof_light_state_v2& pose) {
 auto ent=R_CurrentViewModel();if(!ent)return false;
 float points[6],offsets[3]={
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_x.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_y.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_z.value))};
 if(!R_StudioViewModelLightAttachments(ent,R_CurrentRenderTime(pose.time),g_vOrigin,g_vAngles,offsets,points))return false;
 // Native74910 receives viewmodel attachment[1], not the camera or phone pivot.
 // The held phone follows normal view aim; only a stowed, switched-on
 // portable light uses the separate world-grounded direction.
 for(int i=0;i<3;++i)pose.origin[i]=points[3+i];
 return DX_CofValid(pose);
}
// Weapon-mounted light: start the beam at the Glock taclight lens rather than
// the camera, which sits behind the viewmodel hands.
bool R_CofWeaponLightPose(dx_cof_light_state_v2& pose) {
 const auto taclight=[](const cl_entity_s* ent){return ent&&ent->model&&ent->model->type==mod_studio&&
  !(ent->curstate.effects&EF_NODRAW)&&(!Q_stricmp(ent->model->name,"models/weapons/glock/v_glock.mdl")||
  !Q_stricmp(ent->model->name,"models/weapons/glock/v_glock_doctor.mdl"));};
 bool mirrored=R_CofPrimaryMirrored(R_CurrentRenderTime(0.f));auto ent=R_CurrentViewModel();
 if(!taclight(ent)){ent=g_cofDual.current(R_CurrentRenderTime(0.f));mirrored=g_cofDual.mirror;}
 if(!taclight(ent))return false;
 float points[6],offsets[3]={
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_x.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_y.value)),
  std::max(-128.f,std::min(128.f,r_viewmodel_offset_z.value))};
 if(!R_StudioViewModelTacLightPoints(ent,R_CurrentRenderTime(pose.time),g_vOrigin,g_vAngles,offsets,points,mirrored))return false;
 for(int i=0;i<3;++i){pose.origin[i]=points[i];pose.direction[i]=points[3+i]-points[i];}
 return DX_CofValid(pose);
}

static bool g_sceneViewModelHidden=false;
#include "dx_camera_viewmodel.h"
unsigned R_CofCameraFlashBlockers() {
 unsigned blocked=0;const auto ent=R_EngineViewModelEntity();
 if(!ent||!ent->model)blocked|=1u;
 else if(ent->model->type!=mod_studio||Q_stricmp(ent->model->name,"models/weapons/camera/v_camera.mdl"))blocked|=2u;
 if(g_sceneViewModelHidden)blocked|=4u;
 if(!gp_cl)blocked|=8u;
 else if(gp_cl->viewentity!=gp_cl->playernum+1)blocked|=16u;
 if(ENGINE_GET_PARM(PARM_LOCAL_HEALTH)<=0)blocked|=32u;
 if(ENGINE_GET_PARM(PARM_THIRDPERSON))blocked|=64u;
 dx_cof_light_state_v2 pose{};pose.time=R_CurrentRenderTime(0.f);
 for(int i=0;i<3;++i){pose.origin[i]=g_vOrigin[i];pose.direction[i]=g_vForward[i];}
 if(!DX_CofValid(pose))blocked|=128u;
 return blocked;
}
#include "dx_camera_socket.h"
bool R_CofCameraFlashArea(camera_socket::Area& area){
 if(R_CofCameraFlashBlockers())return false;
 const float offsets[3]={std::clamp(r_viewmodel_offset_x.value,-128.f,128.f),std::clamp(r_viewmodel_offset_y.value,-128.f,128.f),std::clamp(r_viewmodel_offset_z.value,-128.f,128.f)};
 return R_StudioCameraFlashArea(R_EngineViewModelEntity(),R_CurrentRenderTime(0.f),g_vOrigin,g_vAngles,offsets,area);
}
bool R_CofCameraFlashPose(dx_cof_light_state_v2& pose){
 camera_socket::Area area;if(!R_CofCameraFlashArea(area))return false;
 for(int i=0;i<3;++i){pose.origin[i]=area.source[i];pose.direction[i]=area.normal[i];}return DX_CofValid(pose);
}
extern "C" __declspec(dllexport) void __cdecl DX_SceneViewModelHiddenV1( int hidden )
{
	g_sceneViewModelHidden=hidden!=0;
	if(hidden)g_cofDual.clear();
	if(hidden)R_RetireViewmodelFlashes(nullptr);
}

extern "C" __declspec(dllexport) int __cdecl DX_CofDualViewModelV1(const cl_entity_t* entity,int mirror,float time)
{
    return g_cofDual.publish(entity,mirror,time)?1:0;
}

extern "C" __declspec(dllexport) void __cdecl DX_CofDualHandsV1(int primary,int secondary) {
    g_cofDual.primaryHiddenArm=g_cofDual.valid&&primary>=0&&primary<=2?primary:0;
    g_cofDual.secondaryHiddenArm=g_cofDual.valid&&secondary>=0&&secondary<=2?secondary:0;
}

static bool R_CofRifleScopeActive() {
 auto ent=R_CurrentViewModel();
 return R_CofClientPresent()&&!g_sceneViewModelHidden&&gp_cl&&gp_cl->viewentity==gp_cl->playernum+1&&
  ENGINE_GET_PARM(PARM_LOCAL_HEALTH)>0&&!ENGINE_GET_PARM(PARM_THIRDPERSON)&&ent&&ent->model&&
  !(ent->curstate.effects&EF_NODRAW)&&cof_features::rifleScope(ent->model->name);
}
void R_QueueCofPickupOutline(const studio_cache_t& cache,const D3DMATRIX& world,float strength) {
 if(!g_sceneViewportValid||!cache.gpuSkinningPoseValid||cache.gpuSkinningBoneCount<=0||
    cache.gpuSkinningBoneCount>MAXSTUDIOBONES||dx_cof_pickup_outline.value==0.f)return;
 D3DXMATRIX viewProjection,transform;D3DXMatrixMultiply(&viewProjection,&g_sceneView,&g_sceneProjection);
 D3DXMatrixMultiply(&transform,reinterpret_cast<const D3DXMATRIX*>(&world),&viewProjection);
 std::vector<cof_pickup::Point> points;std::vector<uint32_t> indices;
 for(const auto& batch:cache.batches) {
  if(points.size()+batch.sources.size()>65536||indices.size()+batch.indices.size()>98304)return;
  const uint32_t base=uint32_t(points.size());
  for(const auto& source:batch.sources) {
   cof_pickup::Point p;
   if(source.bone>=cache.gpuSkinningBoneCount){points.push_back(p);continue;}
   const float local[3]={source.x,source.y,source.z};float posed[3];
   const auto& bone=cache.gpuSkinningBoneMatrices[source.bone];
   for(int row=0;row<3;++row)posed[row]=local[0]*bone[row][0]+local[1]*bone[row][1]+local[2]*bone[row][2]+bone[row][3];
   const D3DXVECTOR4 input(posed[0],posed[1],posed[2],1);D3DXVECTOR4 clip;
   D3DXVec4Transform(&clip,&input,&transform);
   if(std::isfinite(clip.w)&&clip.w>.01f&&clip.z>=0&&clip.z<=clip.w) {
    p.x=float(g_sceneViewport.X)+(clip.x/clip.w+1.f)*float(g_sceneViewport.Width)*.5f;
    p.y=float(g_sceneViewport.Y)+(1.f-clip.y/clip.w)*float(g_sceneViewport.Height)*.5f;p.valid=true;
   }
   points.push_back(p);
  }
  for(auto index:batch.indices){if(index>=batch.sources.size())return;indices.push_back(base+index);}
 }
 g_cofPickupOutline=cof_pickup::outline(points,indices,float(g_sceneViewport.X),float(g_sceneViewport.Y),
  float(g_sceneViewport.Width),float(g_sceneViewport.Height),strength);
}
#include "dx_nightmare_photo.h"
static void R_DrawCofPickupOutline() {
 if(!device||g_cofPickupOutline.empty()||!g_sceneViewportValid||dx_cof_pickup_outline.value==0.f||
    FBitSet(g_currentViewFlags,RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW))return;
 R_Set2DMode(true);R_Flush2DBatch();R_Apply2DNeutralState();DX_BindRawTexture(0,nullptr);
 DX_SetTextureStageStateCached(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
 DX_SetTextureStageStateCached(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 DX_SetTextureStageStateCached(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
 DX_SetTextureStageStateCached(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
 DX_SetRenderStateCached(D3DRS_ALPHABLENDENABLE,TRUE);DX_SetRenderStateCached(D3DRS_BLENDOP,D3DBLENDOP_ADD);
 DX_SetRenderStateCached(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);DX_SetRenderStateCached(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
 DX_SetRenderStateCached(D3DRS_CULLMODE,D3DCULL_NONE);
 std::vector<vertex2d_t> vertices;vertices.reserve(g_cofPickupOutline.size()*6);
 for(const auto& span:g_cofPickupOutline) {
  const DWORD color=D3DCOLOR_ARGB(span.alpha,235,45,35);
  const vertex2d_t a={span.x-.5f,span.y-.5f,0,1,color,0,0},b={span.x+span.width-.5f,span.y-.5f,0,1,color,0,0};
  const vertex2d_t c={span.x-.5f,span.y+span.height-.5f,0,1,color,0,0},d={span.x+span.width-.5f,span.y+span.height-.5f,0,1,color,0,0};
  vertices.insert(vertices.end(),{a,b,c,b,d,c});
 }
 device->SetFVF(D3DFVF_VERTEX2D);
 R_DrawScratch(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),UINT(vertices.size()),vertices.data(),sizeof(vertex2d_t));
 R_Set2DMode(false);DX_InvalidateD3DStateCache();DX_InvalidateTextureBindCache();
}
static void R_DrawCofScopeOverlay() {
 if(!device||!g_sceneViewportValid||!R_CofRifleScopeActive()||
    FBitSet(g_currentViewFlags,RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW))return;
 R_Set2DMode(true);R_Flush2DBatch();R_Apply2DNeutralState();
 DX_BindRawTexture(0,nullptr);
 DX_SetTextureStageStateCached(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
 DX_SetTextureStageStateCached(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 DX_SetTextureStageStateCached(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
 DX_SetTextureStageStateCached(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
 DX_SetRenderStateCached(D3DRS_ALPHABLENDENABLE,TRUE);
 DX_SetRenderStateCached(D3DRS_BLENDOP,D3DBLENDOP_ADD);
 DX_SetRenderStateCached(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
 DX_SetRenderStateCached(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
 DX_SetRenderStateCached(D3DRS_CULLMODE,D3DCULL_NONE);
 std::vector<vertex2d_t> vertices;
 const float w=float(g_sceneViewport.Width),h=float(g_sceneViewport.Height);
 const float cx=float(g_sceneViewport.X)+w*.5f,cy=float(g_sceneViewport.Y)+h*.5f;
 const float radius=std::min(w,h)*.46f,outer=std::max(w,h)*2.f;
 const auto quad=[&](float ax,float ay,float bx,float by,float dx,float dy,float ex,float ey,byte innerAlpha,byte outerAlpha) {
  vertex2d_t a={ax,ay,0,1,D3DCOLOR_ARGB(innerAlpha,0,0,0),0,0},b={bx,by,0,1,D3DCOLOR_ARGB(innerAlpha,0,0,0),0,0};
  vertex2d_t d={dx,dy,0,1,D3DCOLOR_ARGB(outerAlpha,0,0,0),0,0},e={ex,ey,0,1,D3DCOLOR_ARGB(outerAlpha,0,0,0),0,0};
  vertices.insert(vertices.end(),{a,b,d,b,e,d});
 };
 // No texture in the aperture: the zoomed scene remains truly transparent.
 for(int i=0;i<256;++i) {
  const float a=float(i)*float(M_PI*2)/256.f,b=float(i+1)*float(M_PI*2)/256.f;
  const auto ring=[&](float inner,float end,byte ia,byte oa) {
   quad(cx+inner*cosf(a),cy+inner*sinf(a),cx+inner*cosf(b),cy+inner*sinf(b),
        cx+end*cosf(a),cy+end*sinf(a),cx+end*cosf(b),cy+end*sinf(b),ia,oa);
  };
  ring(radius,radius+1.f,0,255);ring(radius+1.f,outer,255,255);
 }
 const float line=std::max(1.f,h/900.f);
 const auto rect=[&](float x,float y,float width,float height) {quad(x,y,x+width,y,x,y+height,x+width,y+height,255,255);};
 rect(cx-radius,cy-line*.5f,radius*2,line);rect(cx-line*.5f,cy-radius,line,radius*2);
 for(int i=-3;i<=3;++i)if(i) {
  const float tick=radius*float(i)*.2f;
  rect(cx+tick-line*.5f,cy-4*line,line,8*line);rect(cx-4*line,cy+tick-line*.5f,8*line,line);
 }
 device->SetFVF(D3DFVF_VERTEX2D);
 R_DrawScratch(D3DPT_TRIANGLELIST,static_cast<UINT>(vertices.size()/3),static_cast<UINT>(vertices.size()),vertices.data(),sizeof(vertex2d_t));
 R_Set2DMode(false);DX_InvalidateD3DStateCache();DX_InvalidateTextureBindCache();
}

static void R_DrawViewModel( void )
{
 if(R_CofRifleScopeActive()){R_RetireViewmodelFlashes(nullptr);return;}
	// Match the other Xash renderers: external camera scenes have no weapon.
	const auto drawViewModel=gEngfuncs.pfnGetCvarPointer ?
		gEngfuncs.pfnGetCvarPointer("r_drawviewmodel") : nullptr;
	if(drawViewModel && drawViewModel->value==0.0f) { R_RetireViewmodelFlashes(nullptr);return; }
	if( g_sceneViewModelHidden || !gp_cl ||
		gp_cl->viewentity != gp_cl->playernum + 1 ||
		ENGINE_GET_PARM( PARM_LOCAL_HEALTH ) <= 0 ) { R_RetireViewmodelFlashes(nullptr);return; }
	if( ENGINE_GET_PARM( PARM_THIRDPERSON ) ) { R_RetireViewmodelFlashes(nullptr);return; }

	cl_entity_s *viewModel = R_CurrentViewModel( );
	if( !viewModel || !viewModel->model ||
		viewModel->model->type != mod_studio ||
		( viewModel->curstate.effects & EF_NODRAW ) )
	{
		R_RetireViewmodelFlashes(nullptr);
		if(!g_cofDual.current(static_cast<float>(gp_cl->time)))return;
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
 // The flash excludes viewmodels through Remix's light mask. Mark this
 // camera draw explicitly using its documented max-depth classification,
 // independent of FOV heuristics; retain every other weapon's viewport.
 if(dx_remix.value!=0.f&&viewModel&&viewModel->model&&
    !Q_stricmp(viewModel->model->name,"models/weapons/camera/v_camera.mdl"))viewModelViewport.MaxZ=0.f;
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

	const bool dualCurrent=g_cofDual.current(static_cast<float>(gp_cl->time))!=nullptr;
	const bool primaryMirrored=R_CofPrimaryMirrored(static_cast<float>(gp_cl->time));
	DX_SetRenderStateCached(D3DRS_CULLMODE,primaryMirrored?D3DCULL_CW:D3DCULL_CCW);
	if(viewModel)R_DrawStudioViewModel( viewModel, g_vOrigin, g_vAngles,
		g_vForward, g_vRight, g_vUp,
		forwardOffset, rightOffset, upOffset, primaryMirrored, true, dualCurrent?g_cofDual.primaryHiddenArm:0 );
	g_cofDual.submit(static_cast<float>(gp_cl->time),g_cofDualPass,[&](cl_entity_t* secondary,bool mirrored) {
		DX_SetRenderStateCached(D3DRS_CULLMODE,mirrored?D3DCULL_CW:D3DCULL_CCW);
		R_DrawStudioViewModel(secondary,g_vOrigin,g_vAngles,g_vForward,g_vRight,g_vUp,
			forwardOffset,rightOffset,upOffset,mirrored,true,g_cofDual.secondaryHiddenArm,false);
		const auto debug=gEngfuncs.pfnGetCvarPointer?gEngfuncs.pfnGetCvarPointer("cof8_dual_debug"):nullptr;
		static unsigned budget=120;
		if(debug && debug->value!=0.f && budget && gEngfuncs.Con_Printf) {
			--budget;gEngfuncs.Con_Printf("DUAL MODEL: draw-request model=%s seq=%d body=%d mirror=%d clientEvents=0\n",secondary->model->name,secondary->curstate.sequence,secondary->curstate.body,mirrored);
		}
	});

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
	// Side-plane normals (forward * tanFov +/- right/up) are not unit
	// length. Scale the sphere radius by that length to avoid clipping
	// intersecting models at wide FOVs and near the screen edges.
	const float radiusX = radius * sqrtf( 1.0f + g_dxCullTanX * g_dxCullTanX );
	const float radiusY = radius * sqrtf( 1.0f + g_dxCullTanY * g_dxCullTanY );

	if( depth + radius < g_dxCullNear )
		return false;
	if( depth - radius > g_dxCullFar )
		return false;
	if( depth * g_dxCullTanX + side + radiusX < 0.0f )
		return false;
	if( depth * g_dxCullTanX - side + radiusX < 0.0f )
		return false;
	if( depth * g_dxCullTanY + vertical + radiusY < 0.0f )
		return false;
	if( depth * g_dxCullTanY - vertical + radiusY < 0.0f )
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

		// Keep the sphere around the entity origin. A local bounds center
		// cannot simply be added in world space when the entity rotates.
		// This rotation-invariant bound also covers interpolated rotations.
		radius = ( sqrtf( ex * ex + ey * ey + ez * ez ) +
			sqrtf( cx * cx + cy * cy + cz * cz ) ) * modelScale;
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

// Remix pairs a batch's vertices with the previous frame's by index to build
// motion vectors. Distance sorting reorders the sprite quads every time the eye
// moves, so frame generation warps each sprite towards another sprite's position.
// Remix path traces the scene, so blending order does not matter there.
static bool R_RemixTranslucentLess( const translucent_entity_t &a,
	const translucent_entity_t &b )
{
	const bool aSprite = a.entity->model->type == mod_sprite;
	const bool bSprite = b.entity->model->type == mod_sprite;
	if( aSprite != bSprite )
		return bSprite;
	if( !aSprite )
		return R_TranslucentLess( a, b );
	if( a.entity->model != b.entity->model )
		return a.entity->model < b.entity->model;
	if( a.entity->curstate.rendermode != b.entity->curstate.rendermode )
		return a.entity->curstate.rendermode < b.entity->curstate.rendermode;
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

// Native lanterns use the client's private light pool, not engine dlights.
static void R_CollectLanternLights() {
 unsigned active=0;
 const auto submit=[&](int key,const float* point) {
  for(int i=0;i<3;++i)if(!std::isfinite(point[i]))return;
  // Keep a stable dynamic D3D light, as for the working phone/flashlight.
  // Recreating API lights as the hand sways produces temporal discontinuities.
  if(active>=6)return;
  R_AddDLight(point,220.f,8,8,8,R_CurrentRenderTime(0.f)+.1f,key);
  ++active;
 };
 if(device && dx_remix.value!=0.f && dx_dlights.value!=0.f && dx_dlight_entities.value!=0.f) {
  if(!g_sceneViewModelHidden && gp_cl && gp_cl->viewentity==gp_cl->playernum+1 && ENGINE_GET_PARM(PARM_LOCAL_HEALTH)>0 &&
     !ENGINE_GET_PARM(PARM_THIRDPERSON)) {
   const auto draw=gEngfuncs.pfnGetCvarPointer?gEngfuncs.pfnGetCvarPointer("r_drawviewmodel"):nullptr;
   cl_entity_s* hands[2]={R_CurrentViewModel(),g_cofDual.current(R_CurrentRenderTime(0.f))};
   float offsets[3]={std::clamp(r_viewmodel_offset_x.value,-128.f,128.f),
    std::clamp(r_viewmodel_offset_y.value,-128.f,128.f),std::clamp(r_viewmodel_offset_z.value,-128.f,128.f)};
   for(int hand=0;hand<2;++hand)if((!draw||draw->value!=0.f)&&R_CofLanternModel(hands[hand],true)) {
    float points[6];
    if(R_StudioViewModelLightAttachments(hands[hand],R_CurrentRenderTime(0.f),g_vOrigin,g_vAngles,offsets,points,hand==1?g_cofDual.mirror:R_CofPrimaryMirrored(R_CurrentRenderTime(0.f))))
     submit(-1-hand,points);
   }
  }
  for(auto ent:g_entityLightEmitters)if(R_CofLanternModel(ent,false)) {
   float point[3]={ent->origin[0],ent->origin[1],ent->origin[2]+6.f};
   if(ent->index>0)submit(ent->index,point);
  }

 }
 static unsigned previous=~0u;static ULONGLONG reported=0;
 if(previous!=active && GetTickCount64()-reported>1000) {
  gEngfuncs.Con_Printf("D3D9: stable lantern point lights: %u\n",active);previous=active;reported=GetTickCount64();
 }
}

unsigned R_CofRawLightFlags();
static void R_CollectCofFlareLights() {
 if(!R_CofClientPresent()||!gp_cl||dx_dlights.value==0.f||dx_dlight_entities.value==0.f)return;
 const float now=R_CurrentRenderTime(0.f);
 if(!g_sceneViewModelHidden&&gp_cl->viewentity==gp_cl->playernum+1&&ENGINE_GET_PARM(PARM_LOCAL_HEALTH)>0&&
    !ENGINE_GET_PARM(PARM_THIRDPERSON)) {
  auto draw=gEngfuncs.pfnGetCvarPointer?gEngfuncs.pfnGetCvarPointer("r_drawviewmodel"):nullptr;
  cl_entity_s* hands[2]={R_CurrentViewModel(),g_cofDual.current(now)};
  for(int hand=0;hand<2;++hand) {
   auto ent=hands[hand];
   if((draw&&draw->value==0.f)||!ent||!ent->model||(ent->curstate.effects&EF_NODRAW)||!cof_features::flareModel(ent->model->name,true))continue;
   auto hdr=static_cast<studiohdr_t*>(gEngfuncs.Mod_Extradata(mod_studio,ent->model));
   if(!hdr||ent->curstate.sequence<0||ent->curstate.sequence>=hdr->numseq||
      !R_StudioRangeValid(hdr,hdr->seqindex,size_t(hdr->numseq)*sizeof(mstudioseqdesc_t)))continue;
   const auto seqs=reinterpret_cast<const mstudioseqdesc_t*>(reinterpret_cast<const byte*>(hdr)+hdr->seqindex);
   const auto& seq=seqs[ent->curstate.sequence];char label[33]{};memcpy(label,seq.label,32);
   const float frame=R_StudioEstimateFrame(ent,const_cast<mstudioseqdesc_t*>(&seq),now);
   // Dual wield keeps a burning flare on flare_draw/flare_idle, so trust the
   // client's burning flag there instead of the sequence name.
   const bool dualBurning=hands[1]&&(R_CofRawLightFlags()&16u)&&!std::strstr(label,"unlit")&&
    std::strcmp(label,"discard")&&std::strcmp(label,"flare_holster");
   if(!dualBurning&&!cof_features::litFlareSequence(label,frame))continue;
   float points[6],offsets[3]={r_viewmodel_offset_x.value,r_viewmodel_offset_y.value,r_viewmodel_offset_z.value};
   if(R_StudioViewModelLightAttachments(ent,now,g_vOrigin,g_vAngles,offsets,points,hand==1?g_cofDual.mirror:R_CofPrimaryMirrored(now)))
    R_AddDLight(points,220.f,255,8,3,now+.1f,-20-hand);
  }
 }
 for(auto ent:g_entityLightEmitters)if(ent&&ent->model&&cof_features::flareModel(ent->model->name,false)&&
    ent->curstate.renderfx==165&&!(ent->curstate.effects&EF_NODRAW)) {
  float point[3]={ent->origin[0],ent->origin[1],ent->origin[2]+5.f};
  R_AddDLight(point,220.f,255,8,3,now+.1f,ent->index);
 }
}

static void R_SelectCofPickup() {
 g_cofPickupOutline.clear();
 g_cofPickupLight=false;
 g_cofPickupIndex=0;g_cofPickupModel=0;g_cofPickupStrength=0;
 if(!R_CofClientPresent()||!gp_cl||g_sceneViewModelHidden||gp_cl->viewentity!=gp_cl->playernum+1||
    ENGINE_GET_PARM(PARM_LOCAL_HEALTH)<=0||ENGINE_GET_PARM(PARM_THIRDPERSON)||
    FBitSet(g_currentViewFlags,RF_DRAW_CUBEMAP|RF_DRAW_OVERVIEW))return;
 const float now=R_CurrentRenderTime(0.f);float selected[3]{};
 for(auto ent:g_entityLightEmitters) {
  if(!ent||!ent->model||ent->model->type!=mod_studio||(ent->curstate.effects&EF_NODRAW)||
     !g_cofPickups.contains(ent->index,ent->curstate.modelindex,now))continue;
  float center[3];for(int i=0;i<3;++i)center[i]=ent->origin[i]+(ent->model->mins[i]+ent->model->maxs[i])*.5f;
  const float strength=cof_features::pickupStrength(g_vOrigin,g_vForward,center);
  if(strength<=g_cofPickupStrength||!gEngfuncs.CL_TraceLine)continue;
  auto trace=gEngfuncs.CL_TraceLine(g_vOrigin,center,PM_WORLD_ONLY);
  if(trace.startsolid||trace.allsolid||trace.fraction<.98f)continue;
  g_cofPickupIndex=ent->index;g_cofPickupModel=ent->curstate.modelindex;g_cofPickupStrength=strength;
  VectorCopy(center,selected);
 }
 // Keep the contour as the primary cue. A much weaker, smaller fill sits
 // above the item, avoiding the old light concentrated against the floor.
 const int red=int(std::lround(4.f*g_cofPickupStrength));
 if(g_cofPickupIndex&&red>0&&dx_cof_pickup_outline.value!=0.f&&dx_dlights.value!=0.f&&dx_dlight_entities.value!=0.f) {
  selected[2]+=4.f;const auto previous=g_dxDLights.size();
  R_AddDLight(selected,18.f,red,0,0,now+.1f,-40);
  g_cofPickupLight=g_dxDLights.size()>previous;
 }
}

static void R_SetupSceneDLights( )
{
	R_BeginDLights( );
	R_CollectLanternLights();
 R_CollectCofFlareLights();
 R_SelectCofPickup();
	R_CollectEngineDLights( );
	R_CollectMuzzleLights( );
	for( int i = 0; i < g_entityLightEmitterCount; ++i )
		R_CollectEntityDLight( g_entityLightEmitters[i] );
	R_CollectBeamLights( );
	R_ApplyDLights( );
	R_ApplyRemixMapLights( );
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
			g_translucentDrawList.end( ),
			dx_remix.value != 0.0f && dx_remix_stable_sprites.value != 0.0f
				? R_RemixTranslucentLess : R_TranslucentLess );
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

unsigned R_CofShadowSceneBlockers(unsigned flags){
 unsigned blocked=0;
 if(!R_CofClientPresent())blocked|=1u;if(dx_remix.value==0.f)blocked|=2u;
 if(g_sceneViewModelHidden)blocked|=4u;if(!gp_cl)blocked|=8u;
 else if(gp_cl->viewentity!=gp_cl->playernum+1)blocked|=16u;
 if(ENGINE_GET_PARM(PARM_LOCAL_HEALTH)<=0)blocked|=32u;
 if(ENGINE_GET_PARM(PARM_THIRDPERSON))blocked|=64u;
 if(g_cofPlayerVisibility.hidden(R_CurrentRenderTime(0.f)))blocked|=128u;
 if(flags&RF_DRAW_CUBEMAP)blocked|=256u;if(flags&RF_DRAW_OVERVIEW)blocked|=512u;
 return blocked;
}
static void R_CofShadowStatusCmd(){
 char path[MAX_PATH]{};GetModuleFileNameA(GetModuleHandleA("ref_d3d9.dll"),path,MAX_PATH);
 gEngfuncs.Con_Printf("SHADOW build=%s %s renderer=%s mode=%.0f device=%d CoF=%d remix=%.0f hiddenView=%d client=%d viewentity=%d local=%d health=%d thirdPerson=%d nativeHidden=%d viewFlags=%u\n",__DATE__,__TIME__,path,dx_cof_player_shadow.value,int(device!=nullptr),int(R_CofClientPresent()),dx_remix.value,int(g_sceneViewModelHidden),int(gp_cl!=nullptr),gp_cl?gp_cl->viewentity:-1,gp_cl?gp_cl->playernum+1:-1,int(ENGINE_GET_PARM(PARM_LOCAL_HEALTH)),int(ENGINE_GET_PARM(PARM_THIRDPERSON)),int(g_cofPlayerVisibility.hidden(R_CurrentRenderTime(0.f))),unsigned(g_currentViewFlags));
 gEngfuncs.Con_Printf("SHADOW blockedMask=0x%X (CoF=1 remix=2 hiddenView=4 client=8 externalCamera=16 dead=32 thirdPerson=64 nativeScene=128 cubemap=256 overview=512)\n",R_CofShadowSceneBlockers(unsigned(g_currentViewFlags)));
 R_CofShadowStudioStatus();
}
void R_DrawEnts( const vec3_t view_origin )
{
	if( !device )
		return;

	DX_PROFILE_SCOPE( drawEntsScope, DX_PROFILE_DRAWENTS );
 struct PhotoWorldPass {
  PhotoWorldPass(){R_BeginCofCameraFlashView();}
  ~PhotoWorldPass(){R_EndCofCameraFlashView();}
 } photoWorldPass;


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

	R_LakeReedsDraw();
	R_LakeBedDraw();
	R_LakeForestDraw();
	R_DrawOpaqueEntities( identity, worldIsIdentity );
	{
		DX_PROFILE_SCOPE( spriteScope, DX_PROFILE_SPRITE_FLUSH );
		R_SetWorldIdentity( identity, worldIsIdentity );
		R_FlushSprites( );
	}

	if( dx_client_efx.value != 0.0f && gEngfuncs.CL_DrawEFX )
		gEngfuncs.CL_DrawEFX( std::max( 0.0f, g_frametime ), false );
	if( gEngfuncs.pfnDrawNormalTriangles )
	{
		g_triApi.lakeGrass=false;
		gEngfuncs.pfnDrawNormalTriangles( );
		g_triApi.lakeGrass=false;
	}
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
        DX_TriBatchBarrier(DX_TRI_FLUSH_PASS);
        g_triBatchStats=tri_batch_stats_t{}; g_triangleBatchScope=true; g_triApi.gunSmoke=false; g_triApi.lakeGrass=false;
        gEngfuncs.pfnDrawTransparentTriangles( );
        DX_TriBatchBarrier(DX_TRI_FLUSH_PASS); g_triangleBatchScope=false; g_triApi.gunSmoke=false; g_triApi.lakeGrass=false;
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
  // World-space smoke uses the world camera, before viewmodel projection.
  R_CofGunSmokeDraw();
		R_DrawViewModel( );
	}
}

static void DX_RenderFrame( const struct ref_viewpass_s *rvp )
{
	++g_cofDualPass;
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
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
		DX_ResetDevice( true, "device-not-reset" );
		return;
	}

	if( DX_PresentChanged( ) )
	{
		DX_ResetDevice( true, "present-parameters-changed" );
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
		g_sceneView = view;

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
 if(R_CofShadowSceneBlockers(unsigned(rvp->flags))==0) {
  auto model=gEngfuncs.Mod_ForName?gEngfuncs.Mod_ForName("models/cofr/player/simon_shadow.mdl",false,false):nullptr;
  auto donor=gEngfuncs.Mod_ForName?gEngfuncs.Mod_ForName("models/cofr/player/police_shadow_anim.mdl",false,false):nullptr;
  R_DrawCofPlayerShadow(gEngfuncs.R_BeamGetEntity?gEngfuncs.R_BeamGetEntity(gp_cl->playernum+1):nullptr,model,donor,R_CurrentViewModel());
 }

 R_DrawNightmarePhoto();
 R_DrawCofPickupOutline();
 R_DrawCofScopeOverlay();
	if( dx_debug_text3d_test.value != 0.0f &&
		!FBitSet( rvp->flags, RF_DRAW_CUBEMAP | RF_DRAW_OVERVIEW ) )
	{
		vec3_t debugPoint;
		VectorMA( rvp->vieworigin, 160.0f, g_vForward, debugPoint );
		VectorMA( debugPoint, 16.0f, g_vUp, debugPoint );
		const rgba_t debugColor = { 255, 208, 64, 255 };
		R_DebugText3D( debugPoint, debugColor,
			"D3D9 world-space debug text" );
	}
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
	case kRenderScreenFadeModulate:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ZERO );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR );
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
		const dxtexture_t *texture = R_GetTexture( g_2dBatchTexture );
		if( texture && (texture->flags & TF_FONT) == TF_FONT )
		{
			// Client/menu effects can change the device directly, without updating
			// our shadows. Glyph UVs must address this atlas with no inherited
			// shader or generated/transformed texture coordinates. Keep batching
			// glyphs; repair the state once per submitted font batch.
			DX_TriBatchBarrier( DX_TRI_FLUSH_INCOMPATIBLE );
			DX_InvalidateTextureBindCache();
			device->SetVertexShader( nullptr );
			device->SetPixelShader( nullptr );
		}

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
	// Like ref_gl, VGUI always uses straight alpha, regardless of the last
	// HUD sprite's render mode. Record it for the deferred 2D batch as well.
	if( g_2dRenderMode != kRenderTransTexture )
		R_Flush2DBatch( );
	g_2dRenderMode = kRenderTransTexture;

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
	DX_RagdollFrame();
	R_ResetTriangleBatch();
	if( !device )
		return;
	const auto skyConnection = ENGINE_GET_PARM( PARM_CONNSTATE );
	const auto skyWorld = gp_cl ? gp_cl->models[1] : nullptr;
	// Recover the authoritative active map even if a loading/disconnect frame
	// cleared a previously submitted selection or no fresh BSP callback occurred.
	if( skyConnection == ca_active && skyWorld && g_mapSky.observeActive(skyWorld->name) )
		g_mapSkyRetryAt = 0;
	else if( skyConnection == ca_disconnected && !skyWorld )
		g_mapSky.disconnected();
	if( dx_remix.value != 0.0f && GetTickCount64() >= g_mapSkyRetryAt )
	{
		g_mapSkyRetryAt = GetTickCount64() + 1000;
		if( g_mapSky.update( reinterpret_cast<uintptr_t>( GetModuleHandleA("d3d9.dll") ),
			[]( const char* key, const char* value ) { return g_mapEmitterBridge.setConfig(key, value); } ) )
			gEngfuncs.Con_Printf("D3D9: NUMOS sky %s -> %s\n",
				g_mapSky.mapName().empty() ? "default" : g_mapSky.mapName().c_str(), g_mapSky.look());
	}

	if( !g_dxRSpeedsCvar && gEngfuncs.pfnGetCvarPointer )
		g_dxRSpeedsCvar = gEngfuncs.pfnGetCvarPointer( "r_speeds" );

	DX_ProfilerBeginFrame(
		g_dxRSpeedsCvar && g_dxRSpeedsCvar->value > 0.0f );

	DX_PROFILE_SCOPE( beginFrameScope, DX_PROFILE_BEGINFRAME );
	g_2dRenderMode = kRenderNormal;
	g_2dModeActive = false;
	R_Reset2DBatch( );
	g_debugText3D.clear( );

	if( dx_remix_perf.value == 0.0f )
		DX_InvalidateTextureBindCache( );
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
	R_TriTraceFlush("frame-complete");
	DX_TriBatchBarrier(DX_TRI_FLUSH_PASS); g_triangleBatchScope=false; g_triApi.gunSmoke=false; g_triApi.lakeGrass=false;
	if( !device )
		return;

	R_Flush2DBatch( );
	R_FlushDebugText3D( );
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
	case kRenderScreenFadeModulate:
		DX_SetRenderStateCached(D3DRS_ALPHABLENDENABLE,TRUE);
		DX_SetRenderStateCached(D3DRS_BLENDOP,D3DBLENDOP_ADD);
		DX_SetRenderStateCached(D3DRS_SRCBLEND,D3DBLEND_ZERO);
		DX_SetRenderStateCached(D3DRS_DESTBLEND,D3DBLEND_SRCCOLOR);
		DX_SetRenderStateCached(D3DRS_ZWRITEENABLE,FALSE);
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
 R_LakeReedsReset();
 R_LakeBedReset();
 R_LakeForestReset();
 R_CofGunSmokeReset();
	R_ResetAudiReplacement();
	DX_RagdollReset("new-map");
	R_ResetCofPlayerVisibility();
	g_cofDual.clear();
	R_ResetCofLights();
	R_RetireViewmodelFlashes(nullptr);
	R_ResetTriangleBatch();
	DX_ResetFog( );
	g_worldmodel = gp_cl ? gp_cl->models[1] : nullptr;
	// This callback also runs when a same-map reload reuses its cached BSP.
	g_mapSky.loaded( g_worldmodel ? g_worldmodel->name : nullptr );
	g_mapSkyRetryAt = 0;
	g_dxOverviewView = false;
	g_dxOrthoValid = false;
	memset( g_playerSubmitted, 0, sizeof( g_playerSubmitted ) );
	memset( g_remotePlayerInterp, 0, sizeof( g_remotePlayerInterp ) );
	memset( g_playerModelCache, 0, sizeof( g_playerModelCache ) );

	R_SyncLightStylesFromEngine( );
	R_RefreshLightmaps( );
	R_BuildRemixMapLights( );

	R_ActivateDecals( );
}

static void R_GammaChanged( qboolean state )
{
	(void)state;
	R_InvalidateLightmapGammaLut( );
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
