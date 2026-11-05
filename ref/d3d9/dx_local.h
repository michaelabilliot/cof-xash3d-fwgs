#include "dx_cof_light.h"
#include "dx_muzzle_policy.h"
#pragma once

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
#include "../../common/ref_params.h"
#include "../../common/pmove.h"
#include "../../common/r_studioint.h"
#include "dx_fast_hash.h"
#include "dx_textures.h"
#include "dx_models.h"
#include "enginefeatures.h"
#include "dx_profiler.h"
#include "pm_local.h"
#include "sprite.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 64
#endif
#ifndef MAXSTUDIOBONES
#define MAXSTUDIOBONES 128
#endif
#ifndef MAXSTUDIOCONTROLLERS
#define MAXSTUDIOCONTROLLERS 8
#endif
#ifndef MAXSTUDIOATTACHMENTS
#define MAXSTUDIOATTACHMENTS 4
#endif
#ifndef MAXEVENTSTRING
#define MAXEVENTSTRING 64
#endif
#ifndef SURF_DRAWTURB
#define SURF_DRAWTURB 0x0010
#endif
#ifndef SURF_PLANEBACK
#define SURF_PLANEBACK 0x0002
#endif
#ifndef SURF_CONVEYOR
#define SURF_CONVEYOR 0x0040
#endif
#ifndef LIGHTMAP_FVF
#define LIGHTMAP_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2 )
#endif

inline constexpr size_t DX_STUDIO_MODEL_FRONT_CACHE_SIZE = 256;
inline constexpr size_t DX_WORLD_MODEL_FRONT_CACHE_SIZE = 128;
inline constexpr int DX_LIGHTMAP_STYLE_SLOTS = 4;
inline constexpr int DX_LIGHTSTYLE_COUNT = 256;
inline constexpr UINT DX_SCRATCH_MAX_BYTES = 16u * 1024u * 1024u;
inline constexpr int DX_STUDIO_MAX_LOCAL_LIGHTS = 4;
inline constexpr int DX_MAX_BEAM_ENTITIES = 1024;

extern IDirect3D9 *d3d;
extern IDirect3DDevice9 *device;
extern float g_frametime;
extern bool g_dxOverviewView;
extern vec3_t g_vRight, g_vUp, g_vForward, g_vOrigin, g_vAngles;

extern bool g_dxCullValid;
extern vec3_t g_dxCullOrigin, g_dxCullForward, g_dxCullRight, g_dxCullUp;
extern float g_dxCullTanX, g_dxCullTanY, g_dxCullNear, g_dxCullFar;

struct studio_cache_t;
float R_CofPhotoTime();
void R_ResetNightmarePhoto();
void R_QueueNightmarePhoto(const cl_entity_s*,const studio_cache_t&,const D3DMATRIX&);
void R_QueueNightmarePhotoCpu(const cl_entity_s*,const model_drawdata_t&,const D3DMATRIX&);

struct lightmap_vertex_t
{
	float x, y, z;
	D3DCOLOR diffuse;
	float u, v;
	float lu, lv;
};

struct world_lightmap_atlas_t
{
	IDirect3DTexture9 *texture = nullptr;
	std::vector<D3DCOLOR> pixels;
	int nextX = 0;
	int nextY = 0;
	int rowHeight = 0;
	int size = 1024;

	size_t usedTexels = 0;
	int allocationCount = 0;

	bool dynamic = false;

	bool dynamicOverlay = false;
	texture_t *materialGroup = nullptr;
};

struct world_surface_draw_range_t
{
	UINT firstIndex = 0;
	UINT indexCount = 0;
	float normal[3] = { 0.0f, 0.0f, 0.0f };
	float planeDist = 0.0f;
};

struct world_material_batch_t;
bool R_DrawNightmareX(const world_material_batch_t&,const cl_entity_s*,float);
struct world_material_batch_t
{
	texture_t *baseTexture = nullptr;

	int atlasIndex = -1;

	bool conveyor = false;

	std::vector<lightmap_vertex_t> vertices;
	std::vector<DWORD> indices;
	std::vector<world_surface_draw_range_t> surfaceRanges;

	IDirect3DVertexBuffer9 *vertexBuffer = nullptr;
	IDirect3DIndexBuffer9 *indexBuffer = nullptr;
	UINT sharedBaseVertex = 0;
	UINT sharedStartIndex = 0;

	int cellX = 0;
	int cellY = 0;
	int cellZ = 0;
	float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
	float maxs[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool boundsValid = false;

	std::vector<int> pvsLeafs;
	bool pvsAlwaysVisible = false;

	UINT vertexCount = 0;
	UINT primitiveCount = 0;
};

struct world_dynamic_overlay_range_t
{

	size_t allocationIndex = static_cast<size_t>( -1 );
	UINT firstIndex = 0;
	UINT indexCount = 0;
};

struct world_dynamic_overlay_batch_t
{
	texture_t *baseTexture = nullptr;
	int overlayAtlasIndex = -1;
	bool conveyor = false;

	std::vector<world_dynamic_overlay_range_t> ranges;

	std::vector<lightmap_vertex_t> vertices;
	std::vector<DWORD> indices;
	UINT sharedBaseVertex = 0;
	UINT sharedStartIndex = 0;
	UINT vertexCount = 0;
	UINT primitiveCount = 0;

	int cellX = 0;
	int cellY = 0;
	int cellZ = 0;
	float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
	float maxs[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool boundsValid = false;
	std::vector<int> pvsLeafs;
	bool pvsAlwaysVisible = false;
};

struct water_source_vertex_t
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float s = 0.0f;
	float t = 0.0f;
};

struct world_water_batch_t
{
	texture_t *baseTexture = nullptr;
	std::vector<water_source_vertex_t> sources;
	std::vector<brush_vertex_t> dynamicVertices;
	std::vector<DWORD> indices;

	IDirect3DVertexBuffer9 *vertexBuffer = nullptr;
	IDirect3DIndexBuffer9 *indexBuffer = nullptr;

	std::vector<int> pvsLeafs;
	bool pvsAlwaysVisible = false;

	UINT vertexCount = 0;
	UINT primitiveCount = 0;

	int cellX = 0;
	int cellY = 0;
	int cellZ = 0;
	float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
	float maxs[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool boundsValid = false;

	bool vertexCacheValid = false;
	float cachedAnimTime = -9999.0f;
	float cachedWaveScale = -1.0f;
	float cachedWaveSpeed = -1.0f;
	float cachedEntityScale = -9999.0f;
	bool cachedRippleTextureMode = false;
	bool cachedUvWaves = false;
	int cachedRenderMode = INT_MIN;
	int cachedRenderAmt = INT_MIN;
	int cachedRenderColorR = INT_MIN;
	int cachedRenderColorG = INT_MIN;
	int cachedRenderColorB = INT_MIN;
};

struct world_lightmap_allocation_t
{
	msurface_t *surface = nullptr;
	int atlasIndex = -1;
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;

	int overlayAtlasIndex = -1;
	int overlayX = 0;
	int overlayY = 0;

	float mins[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
	float maxs[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	bool boundsValid = false;

	std::array<byte, DX_LIGHTMAP_STYLE_SLOTS> styles{};
	std::array<int, DX_LIGHTMAP_STYLE_SLOTS> lastStyleValues{};

	bool queuedForUpdate = false;

	std::vector<unsigned char> aoGeometryOcclusion;
	int aoCacheWidth = 0;
	int aoCacheHeight = 0;
	float aoCacheRadius = -999.0f;
	float aoCacheEdgeStrength = -999.0f;
	float aoCacheCornerStrength = -999.0f;
	float aoCacheSurfaceBorders = -999.0f;
	float aoCacheTrueEdges = -999.0f;
	float aoCacheTrueEdgeStrength = -999.0f;
	float aoCacheTrueEdgeNeighbor = -999.0f;
	float aoCacheCoplanarSuppress = -999.0f;

};

struct world_lightmap_store_t
{

	std::vector<world_lightmap_atlas_t> atlases;

	std::vector<world_lightmap_atlas_t> dynamicOverlayAtlases;
	std::vector<world_lightmap_allocation_t> allocations;
	dxfast::HashMap<const msurface_t *, size_t> allocationBySurface;

	dxfast::HashMap<texture_t *, int> dynamicPageSizeByMaterial;

	std::array<std::vector<size_t>, DX_LIGHTSTYLE_COUNT> allocationsByStyle;
};

struct world_model_draw_t
{

	const model_t *sourceModel = nullptr;
	bool pvsApplicable = false;

	IDirect3DVertexBuffer9 *sharedVertexBuffer = nullptr;
	IDirect3DIndexBuffer9 *sharedIndexBuffer = nullptr;

	IDirect3DVertexBuffer9 *overlaySharedVertexBuffer = nullptr;
	IDirect3DIndexBuffer9 *overlaySharedIndexBuffer = nullptr;

	std::vector<world_material_batch_t> batches;
	std::vector<world_dynamic_overlay_batch_t> dynamicOverlayBatches;
	std::vector<world_water_batch_t> waterBatches;

	std::vector<std::vector<int>> waterPvsPortals;
};

struct attachment_cache_t
{
	vec3_t points[MAXSTUDIOATTACHMENTS]{};
	bool valid[MAXSTUDIOATTACHMENTS]{};
	float updateTime = -99999.0f;
};

struct muzzleflash_state_t
{
	const model_t *model = nullptr;
	dx_muzzle_cursor_t cursor;
	unsigned visibilityGeneration=0;
	bool modelHasMuzzleEvents=false;
	int lastSequence = -999999;

	float lastFrame = -1.0f;
	float lastAnimTime = -99999.0f;
	bool hadEfMuzzleFlash = false;
};

struct studio_light_probe_cache_t
{
	bool valid = false;
	int lightStyleRevision = -1;
	int directionSign = -1;
	bool extendedLighting = false;
	bool extendedPhysentProbe = false;
	bool forceSky = false;
	bool usedSky = false;
	const model_t *model = nullptr;
	unsigned int skySignature = 0;
	float sampleTime = -99999.0f;
	vec3_t origin = { 0.0f, 0.0f, 0.0f };
	vec3_t lightDirection = { 0.0f, 0.0f, -1.0f };
	D3DCOLOR color = D3DCOLOR_ARGB( 255, 255, 255, 255 );
};

struct glow_occlusion_query_t
{
	IDirect3DQuery9 *query = nullptr;
	bool issued = false;
	bool hasResult = false;
	bool visible = true;
	float lastIssueTime = -99999.0f;
};

struct cof_flare_event_state_t { const model_t* model=nullptr;dx_muzzle_cursor_t cursor; };
struct entity_cache_t
{
 cof_flare_event_state_t flare;
	attachment_cache_t attachments;
	muzzleflash_state_t muzzleflash;
	studio_light_probe_cache_t lightProbe;
	glow_occlusion_query_t glow;
	studio_cache_t *studioGpu = nullptr;
};

struct studio_mode_slot_t
{
	const model_t *key = nullptr;
	int effectiveRenderMode = kRenderNormal;
};

struct world_model_slot_t
{
	const model_t *key = nullptr;
	world_model_draw_t *value = nullptr;
};

struct dx_dlight_t
{
	vec3_t origin;
	float radius;
	float r;
	float g;
	float b;
	float dieTime;
	int key;
};

struct dx_line_light_t
{
	vec3_t start;
	vec3_t end;
	float radius;
	float r;
	float g;
	float b;
	float dieTime;
	int key;
};

struct muzzleflash_t
{
	const cl_entity_s *entity;
	const model_t *model;
	vec3_t origin;
	int textureId;
	float scale;
	float startTime;
	float dieTime;
	float rotation;
	bool viewModel;
};

struct dx_entity_state_t
{
	DWORD alphaBlendEnable;
	DWORD srcBlend;
	DWORD destBlend;
	DWORD blendOp;
	DWORD zWriteEnable;
	DWORD zEnable;
	DWORD alphaTestEnable;
	DWORD alphaFunc;
	DWORD alphaRef;
	DWORD textureFactor;
	DWORD colorOp;
	DWORD colorArg1;
	DWORD colorArg2;
	DWORD alphaOp;
	DWORD alphaArg1;
	DWORD alphaArg2;
	DWORD texCoordIndex;
	DWORD textureTransformFlags;
};

using surface_pvs_leafs_t = std::vector<std::vector<int>>;

struct player_gait_state_t
{
	bool initialized = false;
	float gaitFrame = 0.0f;
	float gaitYaw = 0.0f;
	float previousOrigin[3] = { 0.0f, 0.0f, 0.0f };
	float lastPoseTime = -999999.0f;
};

struct player_gait_pose_t
{
	bool active = false;
	int sequence = -1;
	float frame256 = 0.0f;
	float worldYaw = 0.0f;
};

struct studio_local_light_t
{

	vec3_t origin{};
	float radiusSq = 0.0f;
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float intensity = 0.0f;
};

enum studio_material_state_t
{
	DX_STUDIO_MATERIAL_BASE = 0,
	DX_STUDIO_MATERIAL_MASKED,
	DX_STUDIO_MATERIAL_ADDITIVE
};

struct studio_source_vertex_t
{
	float x, y, z;
	float u, v;
	float nx, ny, nz;
	byte bone;
	byte normalBone;
};

struct studio_fast_lit_vertex_t
{
	float x, y, z;
	D3DCOLOR diffuse;
	float u, v;
};

struct studio_normal_lit_vertex_t
{
	float x, y, z;
	float nx, ny, nz;
	D3DCOLOR diffuse;
	float u, v;
};

struct studio_skin_vertex_t
{
	float x, y, z;
	float nx, ny, nz;
	float u, v;

	float boneConstBase;
	float normalBoneConstBase;

	float chromeFlag;
};

struct studio_batch_t
{
	int textureId = 0;
	unsigned int materialFlags = 0u;
	bool chrome = false;
	std::vector<studio_source_vertex_t> sources;
	std::vector<DWORD> indices;
	IDirect3DVertexBuffer9 *vertexBuffer = nullptr;
	IDirect3DIndexBuffer9 *indexBuffer = nullptr;

	UINT vertexStart = 0;
	UINT vertexCount = 0;
	UINT primitiveCount = 0;
};

struct studio_cache_t
{
	model_t *model = nullptr;
	int body = 0;
	int skin = 0;
	int hiddenArm = 0;
	std::vector<studio_batch_t> batches;
	IDirect3DVertexBuffer9 *combinedVertexBuffer = nullptr;
	UINT combinedVertexCount = 0;
	bool combinedVertexBufferMode = false;
	IDirect3DVertexBuffer9 *gpuSkinSourceVertexBuffer = nullptr;
	UINT gpuSkinSourceVertexCount = 0;
	bool gpuSkinningBufferMode = false;
	bool gpuSkinningPoseValid = false;
	bool physicsPoseUploaded = false;
	int gpuSkinningBoneCount = 0;
	BoneMat34 gpuSkinningBoneMatrices[MAXSTUDIOBONES];

	bool topologyValid = false;
	bool poseUploaded = false;
	bool containsChrome = false;
	int lastSequence = -1;
	float lastFrame = -999999.0f;
	int lastGaitSequence = -1;
	float lastGaitFrame = -999999.0f;
	float lastGaitYaw = -999999.0f;
	D3DCOLOR lastDiffuse = 0;
	bool lastNormalLightingEnabled = true;
	bool lastUploadNormalsEnabled = false;
	bool buffersUploadNormals = false;
	float lastNormalLightScale = -1.0f;
	vec3_t currentWorldLightDir = { 0.0f, 0.0f, -1.0f };
	vec3_t lastCpuLocalLightDir = { 0.0f, 0.0f, -1.0f };
	bool lastCpuLocalLightDirValid = false;
	studio_local_light_t localLights[DX_STUDIO_MAX_LOCAL_LIGHTS]{};
	int localLightCount = 0;
	byte lastController[4] = {};
	byte lastBlending[2] = {};
	byte lastMouth = 0;
	bool poseBudgetDeferred = false;
	uint64_t poseBudgetDeferredFrame = 0;
};

struct studio_light_result_t
{
	D3DCOLOR diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );
	vec3_t direction = { 0.0f, 0.0f, -1.0f };
	float directFraction = 0.0f;
};

struct sprite_batch_key_t
{
	int texture = 0;
	int renderMode = kRenderNormal;
	D3DCOLOR textureFactor = 0xFFFFFFFF;
	bool disableDepth = false;
};

struct lightmap_rect_t
{
	int width = 0;
	int height = 0;
};

struct ao_edge_t
{
	vec3_t a = { 0.0f, 0.0f, 0.0f };
	vec3_t b = { 0.0f, 0.0f, 0.0f };
};

struct ao_surface_cache_t
{
	const msurface_t *surface = nullptr;
	std::vector<ao_edge_t> occludingEdges;
};

struct lightmap_dirty_atlas_t
{
	std::vector<RECT> rects;
};

struct beam_light_segment_t
{
	vec3_t start;
	vec3_t end;
	float radius;
	float spacing;
	float dieTime;
	int r;
	int g;
	int b;
	int key;
	int maxSamples;
};

struct dynamic_light_vertex_t
{
	float x, y, z;
	D3DCOLOR diffuse;
	float u, v;
};

enum dx_profile_state_kind_t
{
	DX_PROFILE_STATE_RENDER = 0,
	DX_PROFILE_STATE_STAGE,
	DX_PROFILE_STATE_SAMPLER
};

enum dx_profile_texture_kind_t
{
	DX_PROFILE_TEXTURE_GL = 0,
	DX_PROFILE_TEXTURE_RAW
};

enum dx_profile_scratch_owner_t
{
	DX_PROFILE_SCRATCH_CONTEXT = 0,
	DX_PROFILE_SCRATCH_MODEL
};

enum dx_profile_scratch_buffer_t
{
	DX_PROFILE_SCRATCH_VB = 0,
	DX_PROFILE_SCRATCH_IB
};

enum
{
	DX_PROFILE_PHASE_BRUSHMODEL = DX_PROFILE_BRUSHMODEL,
	DX_PROFILE_PHASE_STUDIOMODEL = DX_PROFILE_STUDIOMODEL,
	DX_PROFILE_PHASE_SPRITEMODEL = DX_PROFILE_SPRITEMODEL,
	DX_PROFILE_PHASE_DECALS = DX_PROFILE_DECALS,
	DX_PROFILE_PHASE_WATERMODEL = DX_PROFILE_WATERMODEL,
	DX_PROFILE_PHASE_STUDIO_LIGHT_PROBE = DX_PROFILE_STUDIO_LIGHT_PROBE,
	DX_PROFILE_PHASE_STUDIO_SKY_PROBE = DX_PROFILE_STUDIO_SKY_PROBE,
	DX_PROFILE_PHASE_STUDIO_DYNAMIC_LIGHTS = DX_PROFILE_STUDIO_DYNAMIC_LIGHTS,
	DX_PROFILE_PHASE_STUDIO_LOCAL_LIGHTS = DX_PROFILE_STUDIO_LOCAL_LIGHTS
};

enum dx_profile_event_t
{
	DX_PROFILE_EVENT_WORLD_BATCH_DRAWN = 0,
	DX_PROFILE_EVENT_WORLD_CULLED_PVS,
	DX_PROFILE_EVENT_WORLD_CULLED_FRUSTUM,
	DX_PROFILE_EVENT_VIS_CACHE_HIT,
	DX_PROFILE_EVENT_VIS_CACHE_MISS,
	DX_PROFILE_EVENT_WATER_BATCH_DRAWN,
	DX_PROFILE_EVENT_SPRITE_QUEUED,
	DX_PROFILE_EVENT_SPRITE_BATCH_DRAWN,
	DX_PROFILE_EVENT_DECAL_BATCH_DRAWN,
	DX_PROFILE_EVENT_STUDIO_CACHE_HIT,
	DX_PROFILE_EVENT_STUDIO_CACHE_MISS,
	DX_PROFILE_EVENT_LIGHTMAP_ALLOC_UPDATED,
	DX_PROFILE_EVENT_RIPPLE_UPLOAD,
	DX_PROFILE_EVENT_GLOW_QUERY_ISSUED,
	DX_PROFILE_EVENT_STUDIO_VB_LOCK,
	DX_PROFILE_EVENT_STUDIO_UPLOAD_KB,
	DX_PROFILE_EVENT_STUDIO_BATCH_DRAWN,
	DX_PROFILE_EVENT_STUDIO_BUDGET_SKIP,
	DX_PROFILE_EVENT_LIGHTMAP_ATLAS_UPLOAD,
	DX_PROFILE_EVENT_LIGHTMAP_THREAD_JOBS,
	DX_PROFILE_EVENT_STUDIO_LIGHT_PROBE_CACHE_HIT,
	DX_PROFILE_EVENT_STUDIO_LIGHT_PROBE_REBUILD,
	DX_PROFILE_EVENT_STUDIO_SKY_TRACE,
	DX_PROFILE_EVENT_STUDIO_DYNAMIC_LIGHT_CANDIDATE,
	DX_PROFILE_EVENT_STUDIO_LOCAL_LIGHT_CANDIDATE,
	DX_PROFILE_EVENT_STUDIO_SKY_PROBE_USED,
	DX_PROFILE_EVENT_2D_QUAD_QUEUED,
	DX_PROFILE_EVENT_2D_BATCH_DRAWN,
	DX_PROFILE_EVENT_TRI_SOURCE_DRAW,
	DX_PROFILE_EVENT_TRI_BATCH_DRAWN,
	DX_PROFILE_EVENT_GLASS_SURFACE,
	DX_PROFILE_EVENT_GLASS_CULLED,
	DX_PROFILE_EVENT_GLASS_DRAW,
	DX_PROFILE_EVENT_GLASS_INDEX_BYTES,
	DX_PROFILE_EVENT_COUNT
};

#define DX_MODEL_PROFILE_SCOPE( name, phaseId ) dx_profile_scope_t name( phaseId )

#ifndef STUDIO_NF_FLATSHADE
#define STUDIO_NF_FLATSHADE 0x0001
#endif
#ifndef STUDIO_NF_CHROME
#define STUDIO_NF_CHROME 0x0002
#endif
#ifndef STUDIO_NF_FULLBRIGHT
#define STUDIO_NF_FULLBRIGHT 0x0004
#endif
#ifndef STUDIO_NF_NOMIPS
#define STUDIO_NF_NOMIPS 0x0008
#endif
#ifndef STUDIO_NF_ADDITIVE
#define STUDIO_NF_ADDITIVE 0x0020
#endif
#ifndef STUDIO_NF_MASKED
#define STUDIO_NF_MASKED 0x0040
#endif
#ifndef STUDIO_NF_UV_COORDS
#define STUDIO_NF_UV_COORDS ( 1u << 31 )
#endif
#ifndef STUDIO_AMBIENT_LIGHT
#define STUDIO_AMBIENT_LIGHT ( 1u << 8 )
#endif
#ifndef STUDIO_FORCE_SKYLIGHT
#define STUDIO_FORCE_SKYLIGHT ( 1u << 10 )
#endif
#ifndef CL_EVENT_MUZZLEFLASH0
#define CL_EVENT_MUZZLEFLASH0 5001
#endif
#ifndef CL_EVENT_MUZZLEFLASH1
#define CL_EVENT_MUZZLEFLASH1 5011
#endif
#ifndef CL_EVENT_MUZZLEFLASH2
#define CL_EVENT_MUZZLEFLASH2 5021
#endif
#ifndef CL_EVENT_MUZZLEFLASH3
#define CL_EVENT_MUZZLEFLASH3 5031
#endif
#ifndef CL_EVENT_NPC_MUZZLEFLASH0
#define CL_EVENT_NPC_MUZZLEFLASH0 5003
#endif
#ifndef EF_BRIGHTLIGHT
#define EF_BRIGHTLIGHT 4
#endif
#ifndef EF_DIMLIGHT
#define EF_DIMLIGHT 8
#endif
#ifndef EF_LIGHT
#define EF_LIGHT 64
#endif
#ifndef EF_NOANIMTEXTURES
#define EF_NOANIMTEXTURES 256
#endif
#ifndef EF_FRAMEANIMTEXTURES
#define EF_FRAMEANIMTEXTURES 512
#endif
#ifndef MAX_DLIGHTS
#define MAX_DLIGHTS 32
#endif
#ifndef MAX_ELIGHTS
#define MAX_ELIGHTS 128
#endif
#ifndef STUDIO_FAST_LIT_FVF
#define STUDIO_FAST_LIT_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif
#ifndef STUDIO_NORMAL_LIT_FVF
#define STUDIO_NORMAL_LIT_FVF ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif
#ifndef DYNAMIC_LIGHT_FVF
#define DYNAMIC_LIGHT_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif
#ifndef DEG2RAD
#define DEG2RAD( x ) ( (x) * ( (float)M_PI / 180.0f ) )
#endif

void DX_AllowFog( qboolean allowed );
enum dx_tri_flush_t { DX_TRI_FLUSH_STATE, DX_TRI_FLUSH_TEXTURE, DX_TRI_FLUSH_INCOMPATIBLE,
    DX_TRI_FLUSH_CAPACITY, DX_TRI_FLUSH_PASS, DX_TRI_FLUSH_COUNT };
void DX_TriBatchBarrier(int reason);
bool R_DrawBoundVertexIndices(INT baseVertex,UINT vertexCount,const DWORD* indices,UINT count);
extern convar_t dx_remix_glass_batch;
void DX_BindRawTexture( int tmu, IDirect3DBaseTexture9 *texture );
int DX_FindTexture( const char *name );
void DX_InvalidateD3DStateCache( void );
void DX_InvalidateTextureBindCache( void );
void DX_InvalidateTextureUnit( int tmu );
void DX_ShutdownWorkers( void );
void DX_LightmapParallelFor( int itemCount, int minItemsPerJob, const std::function<void( int, int )> &fn );
void DX_ParallelFor( int itemCount, int minItemsPerJob, const std::function<void( int, int )> &fn );
void DX_MatrixIdentity( D3DMATRIX &matrix );
void DX_SetGlobalAnisotropy( int maxAnisotropy );
HRESULT DX_SetRenderStateCached( D3DRENDERSTATETYPE state, DWORD value );
HRESULT DX_SetSamplerStateCached( DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value );
HRESULT DX_SetTextureStageStateCached( DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value );
bool DX_ThreadStudioBones( int boneCount );
bool DX_ThreadStudioSkinning( int totalVertices );
qboolean DX_UpdateTexture( int texnum, int cols, int rows, int width, int height, const byte *buffer, pixformat_t fmt );
void R_ActivateDecals( );
void R_AddBeamLight( const vec3_t start, const vec3_t end, float radius, float spacing, int red, int green, int blue, float dieTime, int key, int maxSamples );
void R_AddDecal( int textureIndex, int entityIndex, int modelIndex, vec3_t position, int flags, float scale );
D3DCOLOR R_AddStudioDLights( D3DCOLOR baseColor, const vec3_t point );
void R_AddSurfacePVSLeafs( std::vector<int> &batchLeafs, bool &alwaysVisible, const surface_pvs_leafs_t &surfaceLeafs, int surfaceIndex );
void R_FinalizePVSLeafs( std::vector<int> &batchLeafs, bool alwaysVisible );
bool R_PVSLeafsVisible( const world_model_draw_t &model, const std::vector<int> &batchLeafs, bool alwaysVisible );
void R_ForEachEntityCache( void ( *fn )( entity_cache_t & ) );
void R_AddWaterLeafSamples( const model_t *world, const msurface_t *surface, std::vector<int> &portalLeaves );
void R_AddWaterSurfaceToBatch( const model_t *mod, msurface_t *surface, world_model_draw_t &out, int surfaceIndex, const surface_pvs_leafs_t &surfaceLeafs );
void R_ApplyDLights( void );
void R_ApplyStudioMaterialState( unsigned int flags, const cl_entity_s *ent, studio_material_state_t &currentState );
void R_BeginDLights( void );
void R_BeginEntityRenderMode( const cl_entity_s *ent, dx_entity_state_t *savedState );
void R_BeginModelsFrame( void );
bool R_BoundsVisibleInCurrentView( const float mins[3], const float maxs[3], bool valid );
bool R_BrushTextureNeedsAlphaTest( const texture_t *texture );
void R_BuildMapLightmaps( const model_t *mod );
void R_BuildSurfaceBounds( const model_t *mod, const msurface_t *surf, float mins[3], float maxs[3], bool &valid );
void R_BuildSurfacePVSLeafMap( const model_t *world, surface_pvs_leafs_t &surfaceLeafs );
void R_RegisterStudioModelDrawData( model_t *mod, model_drawdata_t *value );
void R_FreeModelScratchBuffers( void );
void R_CellForLightmapVerts( const lightmap_vertex_t *vertices, int count, int &cellX, int &cellY, int &cellZ );
void R_CheckLightmapBakeParams( void );
void R_InvalidateLightmapGammaLut( void );
void R_ClearAOEdgeCache( void );
void R_ClearDecals( );
void R_ClearStudioGpuCaches( );
void R_CollectBeamLights( void );
void R_CollectEngineDLights( void );
void R_CollectEntityDLight( const cl_entity_s *ent );
void R_CollectMuzzleLights( void );
void R_AddDLight(const vec3_t origin,float radius,int red,int green,int blue,float dieTime,int key);
int R_CreateDecalList( struct decallist_s *pList );
float R_CurrentRenderTime( float fallback );
float R_DistancePointToSegmentSq( const vec3_t point, const vec3_t start, const vec3_t end, float &outFraction );
void R_DrawBeamEntities( bool transparent );
void R_DebugText3D( const vec3_t world, const rgba_t color, const char *text );
void R_DebugText3DPrintf( const vec3_t world, const rgba_t color, const char *format, ... );
bool R_DrawBrushEntity( cl_entity_s *ent );
void R_DrawBrushWater( cl_entity_s *ent );
void R_DrawBrushWaterDepth( cl_entity_s *ent );
void R_DrawLightmappedBrushModel( const world_model_draw_t &model, const cl_entity_s *ent, float time );
bool R_DrawScratch( D3DPRIMITIVETYPE type, UINT primitiveCount, UINT vertexCount, const void *vertices, UINT stride );
bool R_DrawScratchIndexedPrimitive( D3DPRIMITIVETYPE type, UINT vertexCount, UINT primitiveCount, const void *indices, D3DFORMAT indexFormat, const void *vertices, UINT stride );
bool R_DrawScratchPrimitive( D3DPRIMITIVETYPE type, UINT primitiveCount, UINT vertexCount, const void *vertices, UINT stride );
void R_DrawSpriteImmediate( cl_entity_s *ent );
void R_DrawStudioEntity( cl_entity_s *ent );
void R_DrawStudioViewModel( cl_entity_s *ent, const vec3_t viewOrigin, const vec3_t viewAngles, const vec3_t viewForward, const vec3_t viewRight, const vec3_t viewUp, float forwardOffset, float rightOffset, float upOffset, bool mirror=false, bool muzzleEvents=true, int hiddenArm=0 );
void R_DrawWaterIndexed( world_water_batch_t &batch );
bool R_DrawWorldBrushes( model_t *mod );
void R_DrawWorldWater( model_t *mod );
void R_DrawWorldWaterDepth( model_t *mod );
float R_DynamicLightIntensityScale( );
bool R_DynamicLightTouchesBounds( const vec3_t mins, const vec3_t maxs );
void R_ExpandOverlayBounds( world_dynamic_overlay_batch_t &batch, const lightmap_vertex_t *vertices, int count );
void R_ExpandWorldBatchBounds( world_material_batch_t &batch, const lightmap_vertex_t *vertices, int count );
world_dynamic_overlay_batch_t &R_FindDynamicOverlayBatch( world_model_draw_t &model, texture_t *baseTexture, int overlayAtlasIndex, bool conveyor, int cellX, int cellY, int cellZ );
entity_cache_t *R_FindEntityCache( const cl_entity_s *ent );
int R_FindMuzzleFlashTexture( int spriteType );
model_drawdata_t *R_FindStudioModelDrawData( model_t *mod );
world_material_batch_t &R_FindWorldBatch( world_model_draw_t &model, texture_t *baseTexture, int atlasIndex, bool conveyor, int cellX, int cellY, int cellZ );
bool R_FixedStringEquals( const char *field, size_t fieldSize, const char *text );
void R_FlushSprites( void );
void R_FreeRippleCaches( bool releaseManaged );
void R_FreeWaterBatches( world_water_batch_t &batch, bool releaseManagedIndex );
void R_FreeWorldModel( world_model_draw_t &model );
bool R_GetAttachmentPoint( const cl_entity_s *ent, int attachmentIndex, vec3_t outPoint );

bool R_GetBrushSurfaceRange( const model_t *mod, int &outFirstSurface, int &outSurfaceCount );
const mvertex_t *R_GetBrushSurfaceVertex( const model_t *mod, const msurface_t *surface, int vertexOrdinal );
entity_cache_t &R_GetEntityCache( const cl_entity_s *ent );
int R_GetLightmapAtlasAllocCount( int atlasIndex );
int R_GetLightmapAtlasCount( void );
const char *R_GetLightmapAtlasMaterial( int atlasIndex );
float R_GetLightmapAtlasOccupancy( int atlasIndex );
int R_GetLightmapAtlasSize( int atlasIndex );
IDirect3DTexture9 *R_GetLightmapAtlasTexture( int atlasIndex );
world_model_draw_t *R_GetLightmappedBrushModel( const model_t *mod );
int R_GetSurfaceEdge( const model_t *mod, const msurface_t *surface, int edgeOrdinal );
const std::vector<size_t> *R_GetVisibleWorldBatches( const world_model_draw_t &model );
byte *R_GetWorldVis( void );
void R_InvalidateWorldVis( );
D3DCOLOR R_LightColorAlongSegment( const vec3_t start, const vec3_t end, float *lightspot, float *lightvec );
qboolean R_LightmapAtlasDynamic( int atlasIndex );
qboolean R_LightmapAtlasOverlay( int atlasIndex );
bool R_LightmapTexelWorldPoint( const msurface_t *surface, int sampleX, int sampleY, float sampleSize, vec3_t outPoint );
void R_LoadMap( const model_t *mod );
void R_LoadStudioModel( model_t *mod );
void R_MarkWaterBatchesVisible( world_model_draw_t &model );
void R_ModelsLostDevice( );
bool R_NoInterpolationEnabled( );
byte R_OverbrightChannel( byte value );
bool R_OverbrightEnabled( );
size_t R_PointerFrontCacheIndex( const void *ptr, size_t mask );
bool R_QueueSprite( cl_entity_s *ent, model_t *mod );
bool R_ReadSurfaceStaticLight( const msurface_t *surface, const vec3_t point, color24 &outLight, float *outLightVec );
void R_RefreshLightmaps( );
void R_RegisterModelCvars( );
void R_ReleaseGlowOcclusionQueries( );
void R_RemoveDecalsByTexture( int textureIndex );
void R_RestoreEntityRenderMode( const dx_entity_state_t *state );
void R_RestoreOpaqueEntityBaseline( );
void R_SaveEntityRenderState( dx_entity_state_t *state );
texture_t *R_SelectAnimatedTexture( texture_t *texture, const cl_entity_s *ent, float time );
void R_SetAlphaTestEnabled( bool enabled );
void R_SetEntityWorldTransform( const cl_entity_s *ent, float renderTime, D3DMATRIX *out );
void R_SetLightStyles( const int *values, int count );
void R_SetLightmapStageEnabled( qboolean enabled );
void R_SetOpaqueEntityBaseline( );
void R_SetTextureStage( DWORD colorOp, DWORD colorArg1, DWORD colorArg2, DWORD alphaOp, DWORD alphaArg1, DWORD alphaArg2 );
void R_SpawnRendererMuzzleFlash( const cl_entity_s *ent, const vec3_t origin, int eventOption, bool viewModel, float renderTime );
bool R_StudioHeaderValid( const studiohdr_t *hdr );
void R_StudioLoadTexture( model_t *mod, studiohdr_t *phdr, mstudiotexture_t *ptexture );
bool R_StudioRangeValid( const studiohdr_t *hdr, int offset, size_t bytes );
int R_StudioRenderFxRandomLong( int minimum, int maximum );
int R_StudioRenderMode( const cl_entity_s *ent );
void R_StudioStoreLocalLights( studio_cache_t &cache, const studio_local_light_t *lights, int count );
bool R_StudioTraceLight( const vec3_t start, const vec3_t end, color24 &outLight, float *outSpot, float *outLightVec, bool allowReverseExtendedTrace, bool engineExtendedIndependent );

float R_SurfaceSampleSize( const msurface_t *surface );
void R_TransformBounds( const float mins[3], const float maxs[3], const D3DMATRIX &matrix, vec3_t outMins, vec3_t outMaxs );
void R_TransformPoint( const D3DMATRIX &matrix, float x, float y, float z, vec3_t out );
bool R_UIntMul( UINT a, UINT b, UINT &out );
void R_UpdateDLightOverlay( void );
void R_UpdateLightmaps( );
bool R_UploadOverlayBatches( world_model_draw_t &model );
bool R_UploadWaterBatch( world_water_batch_t &batch );
bool R_UploadWorldBatch( world_material_batch_t &batch );
bool R_UploadWorldBatches( world_model_draw_t &model );
bool R_WaterIsTranslucent( const cl_entity_s *ent );
int R_WorldBatchCellSize( );
bool R_WorldMegabatchEnabled( const model_t *mod );
bool R_WorldPVSAvailable( const model_t *model );

extern std::vector<ao_surface_cache_t> g_aoEdgeCache;
extern float g_aoEdgeCacheCoplanarSuppress;
extern const model_t *g_aoEdgeCacheModel;
extern float g_aoEdgeCacheNeighbour;
extern cl_entity_s *g_beamEntities[DX_MAX_BEAM_ENTITIES];
extern int g_beamEntityCount;
extern unsigned int g_currentViewFlags;
extern std::vector<unsigned char> g_dlightOverlayActive;
extern std::vector<size_t> g_dlightRestore;
extern std::vector<size_t> g_dlightSelected;
extern std::vector<size_t> g_dlightTouchedAllocs;
extern std::vector<beam_light_segment_t> g_dxBeamLights;
extern std::vector<dx_dlight_t> g_dxDLights;
extern int g_dxFixedLightCount;
extern std::vector<dx_line_light_t> g_dxLineLights;
extern int g_dxStudioGlobalLightCount;
extern std::array<uint16_t, 128> g_dxStudioGlobalLightIndices;
extern int g_dxStudioLocalLightCount;
extern std::array<uint16_t, 128> g_dxStudioLocalLightIndices;
extern unsigned int g_dynamicLightmapSignature;
extern std::vector<lightmap_dirty_atlas_t> g_dynamicOverlayDirtyScratch;
extern std::vector<unsigned char> g_dynamicOverlaySelectedMask;
extern int g_glowQueriesIssuedThisFrame;
extern float g_glowQueryBudgetTime;
extern float g_lastLightmapUpdateClock;
extern int g_lightStyleRevision;
extern bool g_lightStyleUpdateActive;
extern std::array<int, DX_LIGHTSTYLE_COUNT> g_lightStyleValues;
extern std::array<byte, 256> g_lightmapGammaLut;
extern world_lightmap_store_t g_mapLightmaps;
extern float g_nextLightmapUpdateTime;
extern std::vector<size_t> g_pendingLightmapAllocations;
extern size_t g_pendingLightmapCursor;
extern std::array<player_gait_state_t, MAX_CLIENTS> g_playerGaitStates;
extern cl_entity_s g_playerWeaponDrawEntities[MAX_CLIENTS + 1];
extern std::vector<muzzleflash_t> g_rendererMuzzleFlashes;
extern bool g_spriteBatchActive;
extern sprite_batch_key_t g_spriteBatchKey;
extern std::vector<brush_vertex_t> g_spriteBatchVertices;
extern std::deque<studio_cache_t> g_studioEntityGpuCacheStorage;
extern int g_studioPoseUpdatesThisFrame;
extern uint32_t g_studioRenderFxRandomState;
extern std::array<studio_mode_slot_t,
	DX_STUDIO_MODEL_FRONT_CACHE_SIZE> g_studioRenderModeFrontCache;
extern studio_light_probe_cache_t g_viewModelLightProbeCache;
extern std::deque<world_model_draw_t> g_worldLightmapStorage;
extern dxfast::HashMap<const model_t *, world_model_draw_t *> g_worldLightmappedModels;
extern std::array<world_model_slot_t, DX_WORLD_MODEL_FRONT_CACHE_SIZE>
g_worldModelFrontCache;
void DX_ShutdownWorkers( );

extern convar_t dx_ao;
extern convar_t dx_ao_cache;
extern convar_t dx_ao_contrast;
extern convar_t dx_ao_contrast_strength;
extern convar_t dx_ao_contrast_style_updates;
extern convar_t dx_ao_contrast_threshold;
extern convar_t dx_ao_coplanar_suppress;
extern convar_t dx_ao_corner_strength;
extern convar_t dx_ao_edge_strength;
extern convar_t dx_ao_min_light;
extern convar_t dx_ao_radius;
extern convar_t dx_ao_strength;
extern convar_t dx_ao_surface_borders;
extern convar_t dx_ao_true_edge_neighbor;
extern convar_t dx_ao_true_edge_strength;
extern convar_t dx_ao_true_edges;
extern convar_t dx_beam_close_precision;
extern convar_t dx_beam_close_precision_distance;
extern convar_t dx_beam_close_precision_step;
extern convar_t dx_beam_endpoint_taper;
extern convar_t dx_beam_endpoint_taper_distance;
extern convar_t dx_beam_particles;
extern convar_t dx_beam_sine;
extern convar_t dx_beam_sine_anchor;
extern convar_t dx_beam_sine_boost;
extern convar_t dx_beam_sine_close_fade;
extern convar_t dx_beam_sine_scale;
extern convar_t dx_beam_sine_segments;
extern convar_t dx_beam_trail_spacing;
extern convar_t dx_beam_trails;
extern convar_t dx_beam_width_scale;
extern convar_t dx_beams;
extern convar_t dx_dlight_beam_intensity;
extern convar_t dx_dlight_beams;
extern convar_t dx_dlight_elights;
extern convar_t dx_dlight_engine;
extern convar_t dx_dlight_entities;
extern convar_t dx_dlight_fixed;
extern convar_t dx_dlight_geometry;
extern convar_t dx_dlight_intensity;
extern convar_t dx_dlight_lightmaps;
extern convar_t dx_dlight_max;
extern convar_t dx_dlight_min_light;
extern convar_t dx_dlight_muzzleflash;
extern convar_t dx_dlight_radius_scale;
extern convar_t dx_dlight_studio;
extern convar_t dx_dlight_subdivide;
extern convar_t dx_dlight_world;
extern convar_t dx_dlights;
extern convar_t dx_egon_beam_light;
extern convar_t dx_egon_beam_light_max_samples;
extern convar_t dx_egon_beam_light_radius;
extern convar_t dx_egon_beam_light_spacing;
extern convar_t dx_glow_occlusion;
extern convar_t dx_glow_query_budget;
extern convar_t dx_glow_query_hz;
extern convar_t dx_glow_query_size;
extern convar_t dx_glowshell_freq;
extern convar_t dx_lightmap_alloc_budget;
extern convar_t dx_lightmap_dynamic_page;
extern convar_t dx_lightmap_max_scale;
extern convar_t dx_lightmap_merge_uploads;
extern convar_t dx_lightmap_min_alloc_budget;
extern convar_t dx_lightmap_overlay_page;
extern convar_t dx_lightmap_rebind;
extern convar_t dx_lightmap_ref_fps;
extern convar_t dx_lightmap_update_budget;
extern convar_t dx_lightmap_update_hz;
extern convar_t dx_lightmap_update_time_scaled;
extern convar_t dx_muzzleflash_before_viewmodel;
extern convar_t dx_muzzleflash_depth_test;
extern convar_t dx_muzzleflash_duration;
extern convar_t dx_muzzleflash_forward_offset;
extern convar_t dx_muzzleflash_scale;
extern convar_t dx_muzzleflash_sprite;
extern convar_t dx_muzzleflash_viewmodel_light;
extern convar_t dx_muzzleflash_viewmodel_light_scale;
extern convar_t dx_particle_additive;
extern convar_t dx_particle_limit;
extern convar_t dx_particle_size_scale;
extern convar_t dx_particles;
extern convar_t dx_player_weapon_models;
extern convar_t dx_remix;
extern convar_t dx_remix_decal_alpha_ref;
extern convar_t dx_remix_decal_cutout;
extern convar_t dx_remix_glass_single_sided;
extern convar_t dx_remix_perf;
extern convar_t dx_separate_style_lightmaps;
extern convar_t dx_studio_extended_lighting;
extern convar_t dx_studio_glowshell;
extern convar_t dx_studio_gpu_chrome;
extern convar_t dx_studio_gpu_max_bones;
extern convar_t dx_studio_gpu_skinning;
extern convar_t dx_studio_gpu_smooth;
extern convar_t dx_studio_light_cache_dist;
extern convar_t dx_studio_normal_lighting;
extern convar_t dx_studio_normals;
extern convar_t dx_studio_stagger_updates;
extern convar_t dx_studio_update_budget;
extern convar_t dx_studio_anim_debug;
void R_BeginStudioPoseFrame();
extern convar_t dx_studio_update_hz;
extern convar_t dx_studio_upload_normals;
extern convar_t dx_studio_viewmodel_throttle;
extern convar_t dx_tracer_length_scale;
extern convar_t dx_tracer_width;
extern convar_t dx_tracers;
extern convar_t dx_viewmodel_beam_offset;
extern convar_t dx_world_batch_cell;
extern convar_t r_nocull;
extern convar_t r_nointerp;
extern convar_t r_nolm;
extern convar_t r_viewmodel_near;

void CL_DrawParticles( double frametime, particle_t *particles, float partsize );
void CL_DrawTracers( double frametime, particle_t *tracers );
void CL_DrawBeams( int fTrans, BEAM *beams );

extern convar_t dx_cof_flashlight,dx_cof_light_debug,dx_cof_light_test;
void R_ResetCofLights();
void R_CofLightDebug(const char* format,...);
void R_RetireViewmodelFlashes(const cl_entity_s* entity);

bool R_CofClientPresent();
bool R_CofPlayerDrawAllowed(const cl_entity_s* entity);

extern convar_t dx_cof_flashlight_brightness;
extern convar_t dx_cof_camera_flash,dx_cof_camera_flash_brightness;
namespace camera_socket {struct Area;}
bool R_CofCameraFlashArea(camera_socket::Area&);
bool R_StudioCameraFlashArea(cl_entity_s*,float,const float*,const float*,const float*,camera_socket::Area&);
float R_CofCameraConversionFactor();
float R_CofCameraLegacyRadius();
bool R_CofCameraFlashPose(dx_cof_light_state_v2& pose);
unsigned R_CofCameraFlashBlockers();
void R_BeginCofCameraFlashView();
void R_EndCofCameraFlashView();
bool R_CofCameraFlashShadowActive();
bool R_StudioCameraFlashSocket(cl_entity_s*,float,const float*,const float*,const float*,float*,float*);
bool R_CofCameraViewModelCorrection(const float*,const float*,D3DMATRIX&);
void R_RefreshCofCameraViewModelSettings();
int R_StudioCameraFlashPoint(cl_entity_s* ent,float time,const float* origin,const float* angles,const float* offsets,float* point);
bool R_CofHandheldLightPose(dx_cof_light_state_v2& pose);
bool R_CofHandheldLightEquipped();
bool R_CofPhoneLightActive(unsigned flags);
bool R_CofPhoneLightPose(dx_cof_light_state_v2& pose);
int R_StudioViewModelLightAttachments(cl_entity_s* ent,float time,const float* viewOrigin,const float* viewAngles,const float* offsets,float* points,bool mirror=false);

void R_ResetCofShadow();
void R_DrawCofPlayerShadow(cl_entity_s* player,model_t* model,model_t* donor,cl_entity_s* viewmodel);
float R_CofPickupStrength(const cl_entity_s* entity);
void R_QueueCofPickupOutline(const studio_cache_t& cache,const D3DMATRIX& world,float strength);

extern convar_t dx_cof_player_shadow;
cl_entity_s* R_CofShadowSecondary(float now);
bool R_CofShadowCrouchInput(float now,bool& held,float& height);

void R_CofShadowProbeCmd();
void R_CofShadowTraceCmd();
void R_CofShadowStudioStatus();
