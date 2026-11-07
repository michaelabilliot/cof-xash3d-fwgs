#include "dx_fast_hash.h"
#include "dx_textures.h"
#include "dx_models.h"
#include "enginefeatures.h"
#include "dx_local.h"
#include "pm_local.h"
#include <d3dx9.h>
#include <limits>

#include "sprite.h"
#include "cvardef.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <utility>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <atomic>
#include <string>

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

#ifndef MAXEVENTSTRING
#define MAXEVENTSTRING 64
#endif
#ifndef MAXSTUDIOATTACHMENTS
#define MAXSTUDIOATTACHMENTS 4
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
#ifndef FDECAL_USE_LANDMARK
#define FDECAL_USE_LANDMARK 0x02
#endif
#ifndef FDECAL_DONTSAVE
#define FDECAL_DONTSAVE 0x20
#endif
#ifndef FDECAL_PERMANENT
#define FDECAL_PERMANENT 0x01
#endif
#ifndef FDECAL_LOCAL_SPACE
#define FDECAL_LOCAL_SPACE 0x80
#endif
#ifndef MODEL_HAS_ORIGIN
#define MODEL_HAS_ORIGIN ( 1 << 1 )
#endif
#ifndef MAX_RENDER_DECALS
#define MAX_RENDER_DECALS 4096
#endif

dxfast::HashMap<model_t *, model_drawdata_t *> globalModels;

model_t *g_worldmodel = nullptr;

#ifndef MAX_CLIENTS
#define MAX_CLIENTS 64
#endif

static constexpr size_t DX_RENDERER_DIRECT_ENTITY_SLOTS = 8192;

struct studio_model_slot_t
{
	model_t *key = nullptr;
	model_drawdata_t *value = nullptr;
};
static std::array<studio_model_slot_t, DX_STUDIO_MODEL_FRONT_CACHE_SIZE>
g_studioModelFrontCache{};

std::array<studio_mode_slot_t,
	DX_STUDIO_MODEL_FRONT_CACHE_SIZE> g_studioRenderModeFrontCache{};

size_t R_PointerFrontCacheIndex( const void *ptr, size_t mask )
{
	std::uintptr_t value = reinterpret_cast<std::uintptr_t>( ptr ) >> 4;
	value ^= value >> 11;
	value ^= value >> 23;
	return static_cast<size_t>( value ) & mask;
}

static void R_CacheStudioModelDrawData( model_t *mod, model_drawdata_t *value )
{
	if( !mod )
		return;
	auto &slot = g_studioModelFrontCache[
		R_PointerFrontCacheIndex( mod, DX_STUDIO_MODEL_FRONT_CACHE_SIZE - 1 )];
	slot.key = mod;
	slot.value = value;
}

void R_RegisterStudioModelDrawData( model_t *mod, model_drawdata_t *value )
{
	if( !mod || !value )
		return;

	auto found = globalModels.find( mod );
	if( found != globalModels.end( ) )
		found->second = value;
	else
		globalModels.emplace( mod, value );

	R_CacheStudioModelDrawData( mod, value );
}

model_drawdata_t *R_FindStudioModelDrawData( model_t *mod )
{
	if( !mod )
		return nullptr;

	auto &slot = g_studioModelFrontCache[
		R_PointerFrontCacheIndex( mod, DX_STUDIO_MODEL_FRONT_CACHE_SIZE - 1 )];
	if( slot.key == mod )
		return slot.value;

	if( model_drawdata_t **found = globalModels.try_get( mod ) )
	{
		slot.key = mod;
		slot.value = *found;
		return *found;
	}
	return nullptr;
}

convar_t r_nolm = { (char *)"r_nolm", (char *)"0", 0 };
convar_t r_nointerp = { (char *)"r_nointerp", (char *)"0", 0 };

convar_t dx_overbright =
{ (char *)"dx_overbright", (char *)"1", 0 };

convar_t dx_ao =
{ (char *)"dx_ao", (char *)"0", 0 };
convar_t dx_ao_strength =
{ (char *)"dx_ao_strength", (char *)"0.28", 0 };
convar_t dx_ao_radius =
{ (char *)"dx_ao_radius", (char *)"24", 0 };
convar_t dx_ao_edge_strength =
{ (char *)"dx_ao_edge_strength", (char *)"0.75", 0 };
convar_t dx_ao_corner_strength =
{ (char *)"dx_ao_corner_strength", (char *)"0.35", 0 };
convar_t dx_ao_min_light =
{ (char *)"dx_ao_min_light", (char *)"0.55", 0 };

convar_t dx_ao_surface_borders =
{ (char *)"dx_ao_surface_borders", (char *)"0", 0 };

convar_t dx_ao_true_edges =
{ (char *)"dx_ao_true_edges", (char *)"1", 0 };
convar_t dx_ao_true_edge_strength =
{ (char *)"dx_ao_true_edge_strength", (char *)"1.15", 0 };
convar_t dx_ao_true_edge_neighbor =
{ (char *)"dx_ao_true_edge_neighbor", (char *)"1", 0 };
convar_t dx_ao_coplanar_suppress =
{ (char *)"dx_ao_coplanar_suppress", (char *)"1", 0 };
convar_t dx_ao_contrast =
{ (char *)"dx_ao_contrast", (char *)"1", 0 };
convar_t dx_ao_contrast_strength =
{ (char *)"dx_ao_contrast_strength", (char *)"0.75", 0 };
convar_t dx_ao_contrast_threshold =
{ (char *)"dx_ao_contrast_threshold", (char *)"0.08", 0 };

convar_t dx_ao_cache =
{ (char *)"dx_ao_cache", (char *)"1", 0 };

convar_t dx_ao_contrast_style_updates =
{ (char *)"dx_ao_contrast_style_updates", (char *)"0", 0 };

convar_t dx_separate_style_lightmaps =
{ (char *)"dx_separate_style_lightmaps", (char *)"1", 0 };

convar_t dx_lightmap_dynamic_page =
{ (char *)"dx_lightmap_dynamic_page", (char *)"256", 0 };

convar_t dx_lightmap_overlay_page =
{ (char *)"dx_lightmap_overlay_page", (char *)"256", 0 };

convar_t dx_lightmap_update_budget =
{ (char *)"dx_lightmap_update_budget", (char *)"262144", 0 };

convar_t dx_lightmap_alloc_budget =
{ (char *)"dx_lightmap_alloc_budget", (char *)"48", 0 };

convar_t dx_lightmap_update_time_scaled =
{ (char *)"dx_lightmap_update_time_scaled", (char *)"1", 0 };
convar_t dx_lightmap_ref_fps =
{ (char *)"dx_lightmap_ref_fps", (char *)"800", 0 };
convar_t dx_lightmap_max_scale =
{ (char *)"dx_lightmap_max_scale", (char *)"16", 0 };

convar_t dx_lightmap_min_alloc_budget =
{ (char *)"dx_lightmap_min_alloc_budget", (char *)"96", 0 };

convar_t dx_threaded_lightmaps =
{ (char *)"dx_threaded_lightmaps", (char *)"1", 0 };
convar_t dx_lightmap_threads =
{ (char *)"dx_lightmap_threads", (char *)"4", 0 };

convar_t dx_lightmap_merge_uploads =
{ (char *)"dx_lightmap_merge_uploads", (char *)"1", 0 };

convar_t dx_lightmap_rebind =
{ (char *)"dx_lightmap_rebind", (char *)"0", 0 };

convar_t dx_water_ripple =
{ (char *)"dx_water_ripple", (char *)"1", 0 };

convar_t dx_water_ripple_resolution =
{ (char *)"dx_water_ripple_resolution", (char *)"0", 0 };

convar_t dx_water_ripple_hz =
{ (char *)"dx_water_ripple_hz", (char *)"40", 0 };

convar_t dx_water_ripple_upload_hz =
{ (char *)"dx_water_ripple_upload_hz", (char *)"0", 0 };

convar_t dx_water_ripple_upload_mpix =
{ (char *)"dx_water_ripple_upload_mpix", (char *)"16", 0 };

convar_t dx_water_ripple_dynamic =
{ (char *)"dx_water_ripple_dynamic", (char *)"1", 0 };

convar_t dx_water_wave_size =
{ (char *)"dx_water_wave_size", (char *)"0.5", 0 };
convar_t dx_water_wave_speed =
{ (char *)"dx_water_wave_speed", (char *)"1", 0 };
convar_t dx_water_subdivide =
{ (char *)"dx_water_subdivide", (char *)"128", 0 };

convar_t dx_water_update_hz =
{ (char *)"dx_water_update_hz", (char *)"30", 0 };

convar_t dx_water_depth_prepass =
{ (char *)"dx_water_depth_prepass", (char *)"0", 0 };

convar_t r_nocull =
{ (char *)"r_nocull", (char *)"0", 0 };

convar_t r_novis =
{ (char *)"r_novis", (char *)"0", 0 };

convar_t dx_pvs_water_margin =
{ (char *)"dx_pvs_water_margin", (char *)"128", 0 };

convar_t dx_glow_occlusion =
{ (char *)"dx_glow_occlusion", (char *)"1", 0 };
convar_t dx_glow_query_size =
{ (char *)"dx_glow_query_size", (char *)"8", 0 };

convar_t dx_glow_query_hz =
{ (char *)"dx_glow_query_hz", (char *)"30", 0 };
convar_t dx_glow_query_budget =
{ (char *)"dx_glow_query_budget", (char *)"64", 0 };
convar_t dx_world_batch_cell =
{ (char *)"dx_world_batch_cell", (char *)"1024", 0 };

convar_t dx_world_megabatch =
{ (char *)"dx_world_megabatch", (char *)"1", 0 };

convar_t dx_decals =
{ (char *)"dx_decals", (char *)"1", 0 };
convar_t dx_decal_offset =
{ (char *)"dx_decal_offset", (char *)"0.25", 0 };
convar_t dx_decal_limit =
{ (char *)"dx_decal_limit", (char *)"2048", 0 };

convar_t dx_decal_lighting =
{ (char *)"dx_decal_lighting", (char *)"1", 0 };

convar_t dx_decal_vertex_lighting =
{ (char *)"dx_decal_vertex_lighting", (char *)"0", 0 };

convar_t dx_decal_project_fallback =
{ (char *)"dx_decal_project_fallback", (char *)"1", 0 };
convar_t dx_decal_debug =
{ (char *)"dx_decal_debug", (char *)"0", 0 };

convar_t dx_studio_normals =
{ (char *)"dx_studio_normals", (char *)"1", 0 };
convar_t dx_studio_normal_lighting =
{ (char *)"dx_studio_normal_lighting", (char *)"1", 0 };

convar_t dx_studio_extended_lighting =
{ (char *)"dx_studio_extended_lighting", (char *)"1", 0 };

convar_t dx_studio_upload_normals =
{ (char *)"dx_studio_upload_normals", (char *)"0", 0 };

convar_t dx_studio_light_cache_dist =
{ (char *)"dx_studio_light_cache_dist", (char *)"4", 0 };

convar_t dx_player_weapon_models =
{ (char *)"dx_player_weapon_models", (char *)"1", 0 };

convar_t dx_studio_update_hz =
{ (char *)"dx_studio_update_hz", (char *)"60", 0 };

convar_t dx_studio_stagger_updates =
{ (char *)"dx_studio_stagger_updates", (char *)"1", 0 };
convar_t dx_studio_viewmodel_throttle =
{ (char *)"dx_studio_viewmodel_throttle", (char *)"0", 0 };

convar_t dx_studio_update_budget =
{ (char *)"dx_studio_update_budget", (char *)"8", 0 };
convar_t dx_studio_anim_debug =
{ (char *)"dx_studio_anim_debug", (char *)"0", 0 };

convar_t dx_studio_gpu_skinning =
{ (char *)"dx_studio_gpu_skinning", (char *)"1", 0 };

convar_t dx_studio_gpu_chrome =
{ (char *)"dx_studio_gpu_chrome", (char *)"1", 0 };

convar_t dx_studio_gpu_max_bones =
{ (char *)"dx_studio_gpu_max_bones", (char *)"72", 0 };

convar_t dx_studio_gpu_smooth =
{ (char *)"dx_studio_gpu_smooth", (char *)"1", 0 };

convar_t dx_studio_glowshell =
{ (char *)"dx_studio_glowshell", (char *)"1", 0 };

convar_t dx_glowshell_freq =
{ (char *)"dx_glowshell_freq", (char *)"2.2", 0 };

convar_t dx_muzzleflash_sprite =
{ (char *)"dx_muzzleflash_sprite", (char *)"1", 0 };
convar_t dx_muzzleflash_duration =
{ (char *)"dx_muzzleflash_duration", (char *)"0.06", 0 };
convar_t dx_muzzleflash_scale =
{ (char *)"dx_muzzleflash_scale", (char *)"1", 0 };
convar_t dx_muzzleflash_forward_offset =
{ (char *)"dx_muzzleflash_forward_offset", (char *)"1.5", 0 };
convar_t dx_muzzleflash_depth_test =
{ (char *)"dx_muzzleflash_depth_test", (char *)"0", 0 };

convar_t dx_muzzleflash_before_viewmodel =
{ (char *)"dx_muzzleflash_before_viewmodel", (char *)"1", 0 };

convar_t dx_muzzleflash_viewmodel_light =
{ (char *)"dx_muzzleflash_viewmodel_light", (char *)"1", 0 };
convar_t dx_muzzleflash_viewmodel_light_scale =
{ (char *)"dx_muzzleflash_viewmodel_light_scale", (char *)"1", 0 };

convar_t dx_cof_flashlight = { (char*)"dx_cof_flashlight", (char*)"1", 0 };
convar_t dx_cof_flashlight_brightness = { (char*)"dx_cof_flashlight_brightness", (char*)"0.25", 0 };
convar_t dx_cof_camera_flash = { (char*)"dx_cof_camera_flash", (char*)"1", 0 };
convar_t dx_cof_camera_flash_brightness = { (char*)"dx_cof_camera_flash_brightness", (char*)"0.8", 0 };
convar_t dx_cof_light_debug = { (char*)"dx_cof_light_debug", (char*)"0", 0 };
convar_t dx_cof_light_test = { (char*)"dx_cof_light_test", (char*)"0", 0 };

convar_t dx_dlights =
{ (char *)"dx_dlights", (char *)"1", 0 };
convar_t dx_dlight_fixed =
{ (char *)"dx_dlight_fixed", (char *)"1", 0 };

convar_t dx_dlight_engine =
{ (char *)"dx_dlight_engine", (char *)"1", 0 };
convar_t dx_dlight_elights =
{ (char *)"dx_dlight_elights", (char *)"1", 0 };

convar_t dx_dlight_world =
{ (char *)"dx_dlight_world", (char *)"1", 0 };

convar_t dx_dlight_lightmaps =
{ (char *)"dx_dlight_lightmaps", (char *)"1", 0 };

convar_t dx_dlight_beams =
{ (char *)"dx_dlight_beams", (char *)"1", 0 };
convar_t dx_dlight_beam_intensity =
{ (char *)"dx_dlight_beam_intensity", (char *)"1", 0 };
convar_t dx_dlight_geometry =
{ (char *)"dx_dlight_geometry", (char *)"0", 0 };
convar_t dx_dlight_subdivide =
{ (char *)"dx_dlight_subdivide", (char *)"0", 0 };
convar_t dx_dlight_min_light =
{ (char *)"dx_dlight_min_light", (char *)"2", 0 };
convar_t dx_dlight_studio =
{ (char *)"dx_dlight_studio", (char *)"1", 0 };
convar_t dx_dlight_entities =
{ (char *)"dx_dlight_entities", (char *)"1", 0 };
convar_t dx_dlight_muzzleflash =
{ (char *)"dx_dlight_muzzleflash", (char *)"1", 0 };
convar_t dx_dlight_intensity =
{ (char *)"dx_dlight_intensity", (char *)"1", 0 };
convar_t dx_dlight_radius_scale =
{ (char *)"dx_dlight_radius_scale", (char *)"1", 0 };
convar_t dx_dlight_max =
{ (char *)"dx_dlight_max", (char *)"64", 0 };

convar_t dx_cpu_worker_threads =
{ (char *)"dx_cpu_worker_threads", (char *)"0", 0 };

convar_t dx_cpu_max_threads =
{ (char *)"dx_cpu_max_threads", (char *)"2", 0 };

convar_t dx_threaded_studio_bones =
{ (char *)"dx_threaded_studio_bones", (char *)"0", 0 };
convar_t dx_threaded_studio_skinning =
{ (char *)"dx_threaded_studio_skinning", (char *)"1", 0 };
convar_t dx_threaded_ripples =
{ (char *)"dx_threaded_ripples", (char *)"1", 0 };
convar_t dx_threaded_studio_min_vertices =
{ (char *)"dx_threaded_studio_min_vertices", (char *)"4096", 0 };
convar_t dx_threaded_ripple_min_pixels =
{ (char *)"dx_threaded_ripple_min_pixels", (char *)"262144", 0 };

convar_t dx_remix_perf =
{ (char *)"dx_remix_perf", (char *)"1", 0 };

convar_t dx_lightmap_update_hz =
{ (char *)"dx_lightmap_update_hz", (char *)"0", 0 };

static bool g_modelCvarsRegistered = false;

void R_RegisterModelCvars( )
{
	if( g_modelCvarsRegistered )
		return;

	gEngfuncs.Cvar_RegisterVariable( &r_nolm );
	gEngfuncs.Cvar_RegisterVariable( &r_nointerp );
	gEngfuncs.Cvar_RegisterVariable( &dx_overbright );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_strength );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_radius );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_edge_strength );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_corner_strength );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_min_light );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_surface_borders );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_true_edges );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_true_edge_strength );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_true_edge_neighbor );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_coplanar_suppress );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_contrast );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_contrast_strength );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_contrast_threshold );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_cache );
	gEngfuncs.Cvar_RegisterVariable( &dx_ao_contrast_style_updates );
	gEngfuncs.Cvar_RegisterVariable( &dx_separate_style_lightmaps );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_dynamic_page );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_overlay_page );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_update_budget );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple_resolution );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple_upload_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple_upload_mpix );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_ripple_dynamic );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_wave_size );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_wave_speed );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_subdivide );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_update_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_water_depth_prepass );
	gEngfuncs.Cvar_RegisterVariable( &r_nocull );
	gEngfuncs.Cvar_RegisterVariable( &r_novis );
	gEngfuncs.Cvar_RegisterVariable( &dx_pvs_water_margin );
	gEngfuncs.Cvar_RegisterVariable( &dx_glow_occlusion );
	gEngfuncs.Cvar_RegisterVariable( &dx_glow_query_size );
	gEngfuncs.Cvar_RegisterVariable( &dx_glow_query_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_glow_query_budget );
	gEngfuncs.Cvar_RegisterVariable( &dx_world_batch_cell );
	gEngfuncs.Cvar_RegisterVariable( &dx_world_megabatch );
	gEngfuncs.Cvar_RegisterVariable( &dx_decals );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_offset );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_limit );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_lighting );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_vertex_lighting );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_project_fallback );
	gEngfuncs.Cvar_RegisterVariable( &dx_decal_debug );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_normals );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_normal_lighting );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_extended_lighting );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_upload_normals );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_light_cache_dist );
	gEngfuncs.Cvar_RegisterVariable( &dx_player_weapon_models );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_update_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_stagger_updates );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_viewmodel_throttle );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_update_budget );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_anim_debug );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_gpu_skinning );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_gpu_chrome );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_gpu_max_bones );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_gpu_smooth );
	gEngfuncs.Cvar_RegisterVariable( &dx_studio_glowshell );
	gEngfuncs.Cvar_RegisterVariable( &dx_glowshell_freq );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_sprite );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_duration );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_forward_offset );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_depth_test );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_before_viewmodel );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_viewmodel_light );
	gEngfuncs.Cvar_RegisterVariable( &dx_muzzleflash_viewmodel_light_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_flashlight );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_flashlight_brightness );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_camera_flash );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_camera_flash_brightness );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_light_debug );
	gEngfuncs.Cvar_RegisterVariable( &dx_cof_light_test );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlights );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_fixed );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_engine );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_elights );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_world );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_lightmaps );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_beams );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_beam_intensity );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_geometry );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_subdivide );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_min_light );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_studio );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_entities );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_muzzleflash );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_intensity );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_radius_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_dlight_max );
	gEngfuncs.Cvar_RegisterVariable( &dx_cpu_worker_threads );
	gEngfuncs.Cvar_RegisterVariable( &dx_cpu_max_threads );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_studio_bones );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_studio_skinning );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_ripples );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_studio_min_vertices );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_ripple_min_pixels );
	gEngfuncs.Cvar_RegisterVariable( &dx_remix_perf );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_update_hz );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_alloc_budget );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_update_time_scaled );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_ref_fps );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_max_scale );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_min_alloc_budget );
	gEngfuncs.Cvar_RegisterVariable( &dx_threaded_lightmaps );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_threads );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_merge_uploads );
	gEngfuncs.Cvar_RegisterVariable( &dx_lightmap_rebind );
	g_modelCvarsRegistered = true;
}

class dx_worker_pool_t
{
public:
	~dx_worker_pool_t( )
	{
		Shutdown( );
	}

	void Ensure( int workerCount )
	{
		workerCount = std::max( 0, std::min( 16, workerCount ) );
		std::unique_lock<std::mutex> lock( mutex );
		if( workerCount == activeWorkerCount && !stopping )
			return;
		lock.unlock( );
		Shutdown( );
		if( workerCount <= 0 )
			return;

		std::unique_lock<std::mutex> startLock( mutex );
		stopping = false;
		activeWorkerCount = workerCount;
		workers.reserve( workerCount );
		for( int i = 0; i < workerCount; ++i )
		{
			workers.emplace_back( [this]( )
			{
				for( ;; )
				{
					std::function<void( )> job;
					{
						std::unique_lock<std::mutex> jobLock( mutex );
						condition.wait( jobLock, [this]( )
						{
							return stopping || !jobs.empty( );
						} );
						if( stopping && jobs.empty( ) )
							return;
						job = std::move( jobs.front( ) );
						jobs.pop_front( );
					}
					job( );
				}
			} );
		}
	}

	void Shutdown( )
	{
		{
			std::lock_guard<std::mutex> lock( mutex );
			stopping = true;
			jobs.clear( );
		}
		condition.notify_all( );
		for( std::thread &worker : workers )
		{
			if( worker.joinable( ) )
				worker.join( );
		}
		workers.clear( );
		{
			std::lock_guard<std::mutex> lock( mutex );
			activeWorkerCount = 0;
			stopping = false;
		}
	}

	int WorkerCount( ) const
	{
		return activeWorkerCount;
	}

	void Submit( std::function<void( )> job )
	{
		{
			std::lock_guard<std::mutex> lock( mutex );
			jobs.push_back( std::move( job ) );
		}
		condition.notify_one( );
	}

private:
	mutable std::mutex mutex;
	std::condition_variable condition;
	std::deque<std::function<void( )>> jobs;
	std::vector<std::thread> workers;
	bool stopping = false;
	int activeWorkerCount = 0;
};

static dx_worker_pool_t g_dxWorkerPool;

static dx_worker_pool_t g_dxLightmapWorkers;

void DX_ShutdownWorkers( )
{
	g_dxWorkerPool.Shutdown( );
	g_dxLightmapWorkers.Shutdown( );
}

void DX_LightmapParallelFor( int itemCount, int minItemsPerJob,
	const std::function<void( int, int )> &fn )
{
	if( itemCount <= 0 )
		return;

	if( dx_threaded_lightmaps.value == 0.0f )
	{
		fn( 0, itemCount );
		return;
	}

	int requestedWorkers = static_cast<int>( dx_lightmap_threads.value );
	if( requestedWorkers <= 0 )
		requestedWorkers = 4;
	requestedWorkers = std::max( 0, std::min( 4, requestedWorkers ) );
	if( requestedWorkers <= 1 || itemCount <= 1 )
	{
		fn( 0, itemCount );
		return;
	}

	if( minItemsPerJob <= 0 )
		minItemsPerJob = 1;

	g_dxLightmapWorkers.Ensure( requestedWorkers );
	const int workerCount = g_dxLightmapWorkers.WorkerCount( );
	int jobCount = ( itemCount + minItemsPerJob - 1 ) / minItemsPerJob;
	jobCount = std::max( 1, std::min( jobCount, workerCount ) );
	if( jobCount <= 1 )
	{
		fn( 0, itemCount );
		return;
	}

	DX_ProfileEvent( DX_PROFILE_EVENT_LIGHTMAP_THREAD_JOBS,
		static_cast<unsigned int>( jobCount ) );

	std::mutex doneMutex;
	std::condition_variable doneCondition;
	std::atomic<int> remainingJobs( jobCount );

	for( int jobIndex = 0; jobIndex < jobCount; ++jobIndex )
	{
		const int begin = ( itemCount * jobIndex ) / jobCount;
		const int end = ( itemCount * ( jobIndex + 1 ) ) / jobCount;
		g_dxLightmapWorkers.Submit( [&, begin, end]( )
		{
			fn( begin, end );
			if( remainingJobs.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
			{
				std::lock_guard<std::mutex> lock( doneMutex );
				doneCondition.notify_one( );
			}
		} );
	}

	std::unique_lock<std::mutex> waitLock( doneMutex );
	doneCondition.wait( waitLock, [&remainingJobs]( )
	{
		return remainingJobs.load( std::memory_order_acquire ) == 0;
	} );
}

static int DX_WorkerCount( )
{
	int requested = static_cast<int>( dx_cpu_worker_threads.value );
	if( requested <= 0 )
	{
		unsigned int hardwareThreads = std::thread::hardware_concurrency( );
		if( hardwareThreads <= 1 )
			return 0;
		requested = static_cast<int>( hardwareThreads ) - 1;
	}

	requested = std::max( 0, std::min( 16, requested ) );
	int maxThreads = static_cast<int>( dx_cpu_max_threads.value );
	if( maxThreads > 0 )
		requested = std::min( requested, std::max( 1, std::min( 16, maxThreads ) ) );
	return requested;
}

static bool DX_WorkersAvailable( )
{
	const int desired = DX_WorkerCount( );
	if( desired <= 0 )
	{
		g_dxWorkerPool.Ensure( 0 );
		return false;
	}
	g_dxWorkerPool.Ensure( desired );
	return g_dxWorkerPool.WorkerCount( ) > 0;
}

void DX_ParallelFor( int itemCount, int minItemsPerJob,
	const std::function<void( int, int )> &fn )
{
	if( itemCount <= 0 )
		return;

	if( minItemsPerJob <= 0 )
		minItemsPerJob = 1;

	if( !DX_WorkersAvailable( ) )
	{
		fn( 0, itemCount );
		return;
	}

	const int workerCount = g_dxWorkerPool.WorkerCount( );
	int jobCount = ( itemCount + minItemsPerJob - 1 ) / minItemsPerJob;
	jobCount = std::max( 1, std::min( jobCount, workerCount ) );
	if( jobCount <= 1 )
	{
		fn( 0, itemCount );
		return;
	}

	std::mutex doneMutex;
	std::condition_variable doneCondition;
	std::atomic<int> remainingJobs( jobCount );

	for( int jobIndex = 0; jobIndex < jobCount; ++jobIndex )
	{
		const int begin = ( itemCount * jobIndex ) / jobCount;
		const int end = ( itemCount * ( jobIndex + 1 ) ) / jobCount;
		g_dxWorkerPool.Submit( [&, begin, end]( )
		{
			fn( begin, end );
			if( remainingJobs.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
			{
				std::lock_guard<std::mutex> lock( doneMutex );
				doneCondition.notify_one( );
			}
		} );
	}

	std::unique_lock<std::mutex> waitLock( doneMutex );
	doneCondition.wait( waitLock, [&remainingJobs]( )
	{
		return remainingJobs.load( std::memory_order_acquire ) == 0;
	} );
}

bool DX_ThreadStudioBones( int boneCount )
{
	if( dx_threaded_studio_bones.value == 0.0f )
		return false;
	return boneCount >= 32 && DX_WorkerCount( ) > 1;
}

bool DX_ThreadStudioSkinning( int totalVertices )
{
	if( dx_threaded_studio_skinning.value == 0.0f )
		return false;
	const int minVertices = std::max( 0,
		static_cast<int>( dx_threaded_studio_min_vertices.value ) );
	return totalVertices >= minVertices && DX_WorkerCount( ) > 1;
}

static bool DX_ThreadRipples( int totalPixels )
{
	if( dx_threaded_ripples.value == 0.0f )
		return false;
	const int minPixels = std::max( 0,
		static_cast<int>( dx_threaded_ripple_min_pixels.value ) );
	return totalPixels >= minPixels && DX_WorkerCount( ) > 0;
}

bool R_NoInterpolationEnabled( )
{
	return r_nointerp.value != 0.0f;
}

bool R_OverbrightEnabled( )
{
	return dx_overbright.value != 0.0f;
}

byte R_OverbrightChannel( byte value )
{
	if( !R_OverbrightEnabled( ) )
		return value;
	return static_cast<byte>( std::min( 255, static_cast<int>( value ) * 2 ) );
}

#ifndef LIGHTMAP_FVF
#define LIGHTMAP_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2 )
#endif

std::array<int, DX_LIGHTSTYLE_COUNT> g_lightStyleValues = []( )
	{
		std::array<int, DX_LIGHTSTYLE_COUNT> values;
		values.fill( 256 );
		return values;
	}( );

std::array<byte, 256> g_lightmapGammaLut{};
int g_lightStyleRevision = 0;

dxfast::HashMap<const model_t *, world_model_draw_t *> g_worldLightmappedModels;
std::deque<world_model_draw_t> g_worldLightmapStorage;

std::array<world_model_slot_t, DX_WORLD_MODEL_FRONT_CACHE_SIZE>
g_worldModelFrontCache{};

world_lightmap_store_t g_mapLightmaps;
std::vector<size_t> g_dlightTouchedAllocs;
std::vector<unsigned char> g_dlightOverlayActive;
unsigned int g_dynamicLightmapSignature = 0;

struct entity_cache_slot_t
{
	const cl_entity_s *key = nullptr;
	entity_cache_t cache;
};

static std::array<entity_cache_slot_t, DX_RENDERER_DIRECT_ENTITY_SLOTS>
g_entitySlots{};
static std::vector<uint16_t> g_activeEntitySlots;
static dxfast::HashMap<const cl_entity_s *, entity_cache_t *>
g_entityOverflow;
static std::deque<entity_cache_t> g_entityOverflowStorage;
static const cl_entity_s *g_lastEntityKey = nullptr;
static entity_cache_t *g_lastEntityCache = nullptr;

static inline entity_cache_slot_t *
R_EntityCacheSlot( const cl_entity_s *ent )
{
	if( !ent )
		return nullptr;
	const int entityNumber = ent->curstate.number;
	if( entityNumber <= 0 ||
		static_cast<size_t>( entityNumber ) >= DX_RENDERER_DIRECT_ENTITY_SLOTS )
	{
		return nullptr;
	}
	return &g_entitySlots[static_cast<size_t>( entityNumber )];
}

entity_cache_t *R_FindEntityCache( const cl_entity_s *ent )
{
	if( g_lastEntityCache && g_lastEntityKey == ent )
		return g_lastEntityCache;

	if( entity_cache_slot_t *slot = R_EntityCacheSlot( ent ) )
	{
		if( slot->key == ent )
		{
			g_lastEntityKey = ent;
			g_lastEntityCache = &slot->cache;
			return &slot->cache;
		}
	}

	if( entity_cache_t **found =
		g_entityOverflow.try_get( ent ) )
	{
		g_lastEntityKey = ent;
		g_lastEntityCache = *found;
		return *found;
	}
	return nullptr;
}

entity_cache_t &R_GetEntityCache( const cl_entity_s *ent )
{
	if( entity_cache_t *existing = R_FindEntityCache( ent ) )
		return *existing;

	if( entity_cache_slot_t *slot = R_EntityCacheSlot( ent ) )
	{
		if( slot->key == nullptr )
		{
			const size_t slotIndex =
				static_cast<size_t>( ent->curstate.number );
			slot->key = ent;
			slot->cache = entity_cache_t{};
			g_activeEntitySlots.push_back(
				static_cast<uint16_t>( slotIndex ) );
			g_lastEntityKey = ent;
			g_lastEntityCache = &slot->cache;
			return slot->cache;
		}
	}

	g_entityOverflowStorage.emplace_back( );
	entity_cache_t *cache =
		&g_entityOverflowStorage.back( );
	g_entityOverflow.insert_unique( ent, cache );
	g_lastEntityKey = ent;
	g_lastEntityCache = cache;
	return *cache;
}

void R_ForEachEntityCache( void ( *fn )( entity_cache_t & ) )
{
	if( !fn )
		return;
	for( const uint16_t slotIndex : g_activeEntitySlots )
		fn( g_entitySlots[slotIndex].cache );
	for( auto &cache : g_entityOverflowStorage )
		fn( cache );
}

static void R_ResetAttachments( )
{
	R_ForEachEntityCache( []( entity_cache_t &cache )
	{
		cache.attachments = attachment_cache_t{};
	} );
}

static void R_ClearEntityCache( )
{
	for( const uint16_t slotIndex : g_activeEntitySlots )
		g_entitySlots[slotIndex] =
		entity_cache_slot_t{};
	g_activeEntitySlots.clear( );

	if( g_activeEntitySlots.capacity( ) < 1024 )
		g_activeEntitySlots.reserve( 1024 );
	g_entityOverflow.clear( );
	g_entityOverflow.max_load_factor( 0.70f );
	g_entityOverflow.reserve( 64 );
	g_entityOverflowStorage.clear( );
	g_lastEntityKey = nullptr;
	g_lastEntityCache = nullptr;
}

std::vector<size_t> g_pendingLightmapAllocations;
size_t g_pendingLightmapCursor = 0;
float g_nextLightmapUpdateTime = -99999.0f;
float g_lastLightmapUpdateClock = -1.0f;

static constexpr UINT DX_SCRATCH_VB_DEFAULT_BYTES = 1024u * 1024u;
static constexpr UINT DX_SCRATCH_IB_DEFAULT_BYTES = 256u * 1024u;
static IDirect3DVertexBuffer9 *g_scratchVertexBuffer = nullptr;
static IDirect3DIndexBuffer9 *g_scratchIndexBuffer = nullptr;
static UINT g_scratchVertexBufferBytes = 0;
static UINT g_scratchIndexBufferBytes = 0;
static D3DFORMAT g_scratchIndexFormat = D3DFMT_INDEX32;
static UINT g_scratchVertexOffset = 0;
static UINT g_scratchIndexOffset = 0;

bool R_UIntMul( UINT a, UINT b, UINT &out )
{
	if( a != 0 && b > std::numeric_limits<UINT>::max( ) / a )
		return false;
	out = a * b;
	return true;
}

static UINT R_AlignScratchOffset( UINT value, UINT alignment )
{
	if( alignment == 0 || value > std::numeric_limits<UINT>::max( ) - ( alignment - 1u ) )
		return std::numeric_limits<UINT>::max( );
	return ( value + alignment - 1u ) & ~( alignment - 1u );
}

static UINT R_ScratchBufferSizeFor( UINT neededBytes, UINT defaultBytes )
{
	if( neededBytes == 0 || neededBytes > DX_SCRATCH_MAX_BYTES )
		return 0;
	UINT size = std::min( defaultBytes, DX_SCRATCH_MAX_BYTES );
	while( size < neededBytes && size <= DX_SCRATCH_MAX_BYTES / 2u )
		size <<= 1;
	return size >= neededBytes ? size : 0;
}

void R_FreeModelScratchBuffers( )
{
	if( g_scratchVertexBuffer )
	{
		g_scratchVertexBuffer->Release( );
		g_scratchVertexBuffer = nullptr;
	}
	if( g_scratchIndexBuffer )
	{
		g_scratchIndexBuffer->Release( );
		g_scratchIndexBuffer = nullptr;
	}
	g_scratchVertexBufferBytes = 0;
	g_scratchIndexBufferBytes = 0;
	g_scratchIndexFormat = D3DFMT_INDEX32;
	g_scratchVertexOffset = 0;
	g_scratchIndexOffset = 0;
}

static bool R_EnsureScratchVertexBuffer( UINT neededBytes )
{
	if( !device || neededBytes == 0 )
		return false;
	if( g_scratchVertexBuffer && g_scratchVertexBufferBytes >= neededBytes )
		return true;
	if( g_scratchVertexBuffer )
	{
		g_scratchVertexBuffer->Release( );
		g_scratchVertexBuffer = nullptr;
	}
	g_scratchVertexBufferBytes = R_ScratchBufferSizeFor( neededBytes, DX_SCRATCH_VB_DEFAULT_BYTES );
	if( g_scratchVertexBufferBytes == 0 )
		return false;
	g_scratchVertexOffset = 0;
	HRESULT result = device->CreateVertexBuffer( g_scratchVertexBufferBytes,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT,
		&g_scratchVertexBuffer, nullptr );
	if( FAILED( result ) || !g_scratchVertexBuffer )
	{
		g_scratchVertexBufferBytes = 0;
		return false;
	}
	return true;
}

static bool R_EnsureScratchIndexBuffer( UINT neededBytes, D3DFORMAT format )
{
	if( !device || neededBytes == 0 )
		return false;
	if( g_scratchIndexBuffer && g_scratchIndexBufferBytes >= neededBytes &&
		g_scratchIndexFormat == format )
	{
		return true;
	}
	if( g_scratchIndexBuffer )
	{
		g_scratchIndexBuffer->Release( );
		g_scratchIndexBuffer = nullptr;
	}
	g_scratchIndexBufferBytes = R_ScratchBufferSizeFor( neededBytes, DX_SCRATCH_IB_DEFAULT_BYTES );
	if( g_scratchIndexBufferBytes == 0 )
		return false;
	g_scratchIndexFormat = format;
	g_scratchIndexOffset = 0;
	HRESULT result = device->CreateIndexBuffer( g_scratchIndexBufferBytes,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, format, D3DPOOL_DEFAULT,
		&g_scratchIndexBuffer, nullptr );
	if( FAILED( result ) || !g_scratchIndexBuffer )
	{
		g_scratchIndexBufferBytes = 0;
		return false;
	}
	return true;
}

static bool R_UploadScratchVertices( const void *vertices, UINT bytes,
	UINT stride, UINT &offsetBytes )
{
	if( !R_EnsureScratchVertexBuffer( bytes ) )
		return false;
	UINT alignedOffset = R_AlignScratchOffset( g_scratchVertexOffset, 16u );
	DWORD lockFlags = D3DLOCK_NOOVERWRITE;
	if( alignedOffset == std::numeric_limits<UINT>::max( ) || alignedOffset == 0 ||
		bytes > g_scratchVertexBufferBytes - std::min( alignedOffset, g_scratchVertexBufferBytes ) )
	{
		alignedOffset = 0;
		lockFlags = D3DLOCK_DISCARD;
	}
	DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_VB, lockFlags, bytes );
	void *memory = nullptr;
	HRESULT result = g_scratchVertexBuffer->Lock( alignedOffset, bytes, &memory, lockFlags );
	if( FAILED( result ) || !memory )
		return false;
	memcpy( memory, vertices, bytes );
	g_scratchVertexBuffer->Unlock( );
	offsetBytes = alignedOffset;
	g_scratchVertexOffset = ( alignedOffset <= std::numeric_limits<UINT>::max( ) - bytes ) ?
		R_AlignScratchOffset( alignedOffset + bytes, 16u ) : 0;
	return true;
}

static bool R_UploadScratchIndices( const void *indices, UINT bytes,
	D3DFORMAT format, UINT &startIndex )
{
	if( !R_EnsureScratchIndexBuffer( bytes, format ) )
		return false;
	const UINT indexSize = ( format == D3DFMT_INDEX16 ) ? 2u : 4u;
	UINT alignedOffset = R_AlignScratchOffset( g_scratchIndexOffset, indexSize );
	DWORD lockFlags = D3DLOCK_NOOVERWRITE;
	if( alignedOffset == std::numeric_limits<UINT>::max( ) || alignedOffset == 0 ||
		bytes > g_scratchIndexBufferBytes - std::min( alignedOffset, g_scratchIndexBufferBytes ) )
	{
		alignedOffset = 0;
		lockFlags = D3DLOCK_DISCARD;
	}
	DX_ProfileLock( DX_PROFILE_SCRATCH_MODEL, DX_PROFILE_SCRATCH_IB, lockFlags, bytes );
	void *memory = nullptr;
	HRESULT result = g_scratchIndexBuffer->Lock( alignedOffset, bytes, &memory, lockFlags );
	if( FAILED( result ) || !memory )
		return false;
	memcpy( memory, indices, bytes );
	g_scratchIndexBuffer->Unlock( );
	startIndex = alignedOffset / indexSize;
	g_scratchIndexOffset = ( alignedOffset <= std::numeric_limits<UINT>::max( ) - bytes ) ?
		R_AlignScratchOffset( alignedOffset + bytes, 16u ) : 0;
	return true;
}

bool R_DrawScratchPrimitive( D3DPRIMITIVETYPE type, UINT primitiveCount,
	UINT vertexCount, const void *vertices, UINT stride )
{
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
	if( !device || !vertices || primitiveCount == 0 || vertexCount == 0 )
		return false;
	UINT vertexBytes = 0;
	if( stride == 0 || !R_UIntMul( vertexCount, stride, vertexBytes ) )
		return false;
	UINT vertexOffset = 0;
	if( !R_UploadScratchVertices( vertices, vertexBytes, stride, vertexOffset ) )
		return false;
	device->SetStreamSource( 0, g_scratchVertexBuffer, vertexOffset, stride );
	DX_ProfileDraw( false, primitiveCount );
	device->DrawPrimitive( type, 0, primitiveCount );
	return true;
}

bool R_DrawScratchIndexedPrimitive( D3DPRIMITIVETYPE type, UINT vertexCount,
	UINT primitiveCount, const void *indices, D3DFORMAT indexFormat,
	const void *vertices, UINT stride )
{
	DX_TriBatchBarrier(DX_TRI_FLUSH_INCOMPATIBLE);
	if( !device || !vertices || !indices || vertexCount == 0 || primitiveCount == 0 )
		return false;
	UINT vertexBytes = 0, indexCount = 0, indexBytes = 0;
	const UINT indexSize = ( indexFormat == D3DFMT_INDEX16 ) ? 2u : 4u;
	if( stride == 0 || !R_UIntMul( vertexCount, stride, vertexBytes ) ||
		!R_UIntMul( primitiveCount, 3u, indexCount ) ||
		!R_UIntMul( indexCount, indexSize, indexBytes ) )
		return false;
	UINT vertexOffset = 0;
	UINT startIndex = 0;
	if( !R_UploadScratchVertices( vertices, vertexBytes, stride, vertexOffset ) )
		return false;
	if( !R_UploadScratchIndices( indices, indexBytes, indexFormat, startIndex ) )
		return false;
	device->SetStreamSource( 0, g_scratchVertexBuffer, vertexOffset, stride );
	device->SetIndices( g_scratchIndexBuffer );
	DX_ProfileDraw( true, primitiveCount );
	device->DrawIndexedPrimitive( type, 0, 0, vertexCount, startIndex, primitiveCount );
	return true;
}

int g_studioPoseUpdatesThisFrame = 0;

void R_BeginModelsFrame( void )
{
	R_RegisterModelCvars( );
	g_scratchVertexOffset = 0;
	g_scratchIndexOffset = 0;
	R_BeginStudioPoseFrame();
	R_CheckLightmapBakeParams( );
}

const mvertex_t *R_GetBrushSurfaceVertex( const model_t *mod,
	const msurface_t *surface, int vertexOrdinal )
{
	if( !mod || !surface || !mod->vertexes || !mod->surfedges ||
		mod->numvertexes <= 0 || mod->numedges <= 0 || mod->numsurfedges <= 0 ||
		vertexOrdinal < 0 || vertexOrdinal >= surface->numedges || surface->firstedge < 0 )
	{
		return nullptr;
	}

	if( vertexOrdinal > std::numeric_limits<int>::max( ) - surface->firstedge )
		return nullptr;
	const int surfedgeIndex = surface->firstedge + vertexOrdinal;
	if( surfedgeIndex < 0 || surfedgeIndex >= mod->numsurfedges )
		return nullptr;

	const int edgeIndex = mod->surfedges[surfedgeIndex];
	if( edgeIndex == INT_MIN )
		return nullptr;

	const int absoluteEdge = edgeIndex < 0 ? -edgeIndex : edgeIndex;
	if( absoluteEdge < 0 || absoluteEdge >= mod->numedges )
		return nullptr;

	const int edgeVertex = edgeIndex < 0 ? 1 : 0;
	unsigned int vertexIndex = 0;
	if( FBitSet( mod->flags, MODEL_QBSP2 ) )
	{
		if( !mod->edges32 )
			return nullptr;
		vertexIndex = mod->edges32[absoluteEdge].v[edgeVertex];
	}
	else
	{
		if( !mod->edges16 )
			return nullptr;
		vertexIndex = mod->edges16[absoluteEdge].v[edgeVertex];
	}

	return vertexIndex < static_cast<unsigned int>( mod->numvertexes ) ?
		&mod->vertexes[vertexIndex] : nullptr;
}

int R_GetSurfaceEdge( const model_t *mod,
	const msurface_t *surface, int edgeOrdinal )
{
	if( !mod || !surface || !mod->surfedges || edgeOrdinal < 0 ||
		edgeOrdinal >= surface->numedges )
	{
		return -1;
	}

	if( mod->numsurfedges <= 0 || surface->firstedge < 0 ||
		edgeOrdinal > std::numeric_limits<int>::max( ) - surface->firstedge )
		return -1;
	const int surfedgeIndex = surface->firstedge + edgeOrdinal;
	if( surfedgeIndex < 0 || surfedgeIndex >= mod->numsurfedges )
		return -1;

	const int edgeIndex = mod->surfedges[surfedgeIndex];
	if( edgeIndex == INT_MIN )
		return -1;

	const int absoluteEdge = edgeIndex < 0 ? -edgeIndex : edgeIndex;
	return ( absoluteEdge >= 0 && absoluteEdge < mod->numedges ) ?
		absoluteEdge : -1;
}

void R_BuildSurfaceBounds( const model_t *mod, const msurface_t *surf,
	float mins[3], float maxs[3], bool &valid )
{
	valid = false;
	if( !mod || !surf || surf->numedges < 3 )
		return;

	mins[0] = mins[1] = mins[2] = FLT_MAX;
	maxs[0] = maxs[1] = maxs[2] = -FLT_MAX;
	for( int i = 0; i < surf->numedges; ++i )
	{
		const mvertex_t *vertex = R_GetBrushSurfaceVertex( mod, surf, i );
		if( !vertex )
			continue;
		AddPointToBounds( vertex->position, mins, maxs );
		valid = true;
	}
}

static D3DCOLOR R_DecalSurfaceLightColor( const msurface_t *surface,
	const vec3_t point )
{
	if( dx_decal_lighting.value == 0.0f )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	color24 light = { 255, 255, 255 };
	if( surface && surface->samples && !( surface->flags & SURF_DRAWTILED ) &&
		R_ReadSurfaceStaticLight( surface, point, light, nullptr ) )
	{
		return D3DCOLOR_ARGB( 255, R_OverbrightChannel( light.r ),
			R_OverbrightChannel( light.g ), R_OverbrightChannel( light.b ) );
	}

	return D3DCOLOR_ARGB( 255, 255, 255, 255 );
}

static bool R_DecalHasDynamicStyle( const msurface_t *surface )
{
	if( !surface )
		return false;

	for( int i = 0; i < DX_LIGHTMAP_STYLE_SLOTS; ++i )
	{
		const byte style = surface->styles[i];
		if( style == 255 )
			break;
		if( style != 0 )
			return true;
	}
	return false;
}

#include "dx_cof_decal.h"

struct dx_decal_vertex_t
{
	float x, y, z;
	D3DCOLOR diffuse;
	float u, v;
};

struct dx_decal_clip_vertex_t
{
	std::array<float, 3> position{};
	float s = 0.0f;
	float t = 0.0f;
};

struct dx_decal_t
{
	int textureIndex = 0;
	short entityIndex = 0;
	short modelIndex = 0;
	byte flags = 0;
	float scale = 1.0f;
	vec3_t position{};
	vec3_t normal{};
	const msurface_t *surface = nullptr;
	int lightStyleRevision = -1;
	bool dynamicLightStyles = false;
	bool cofCustom = false;
	std::vector<dx_decal_vertex_t> vertices;
};

static void R_ApplyDecalLight( dx_decal_t &decal, D3DCOLOR color )
{
	for( dx_decal_vertex_t &vertex : decal.vertices )
		vertex.diffuse = color;
}

static std::vector<dx_decal_t> g_dxDecals;

struct remix_decal_texture_t
{
	IDirect3DTexture9 *source = nullptr;
	IDirect3DTexture9 *cutout = nullptr;
};

static std::vector<remix_decal_texture_t> g_remixDecalTextures;

static void R_ClearRemixDecalTextures( )
{
	for( remix_decal_texture_t &entry : g_remixDecalTextures )
	{
		if( entry.cutout )
			entry.cutout->Release( );
	}
	g_remixDecalTextures.clear( );
}

static IDirect3DTexture9 *R_RemixLegacyDecalTexture( int textureIndex, bool cofCustom = false )
{
	if( !device || textureIndex <= 0 || textureIndex > g_texcount )
		return nullptr;

	dxtexture_t &sourceTexture = g_Textures[textureIndex];
	if( !sourceTexture.tex || ( !cofCustom && ( sourceTexture.flags & TF_HAS_ALPHA ) != 0 ) )
		return sourceTexture.tex;

	if( g_remixDecalTextures.size( ) <= static_cast<size_t>( textureIndex ) )
		g_remixDecalTextures.resize( static_cast<size_t>( textureIndex ) + 1 );

	remix_decal_texture_t &cache = g_remixDecalTextures[textureIndex];
	if( cache.source == sourceTexture.tex && cache.cutout )
		return cache.cutout;

	if( cache.cutout )
	{
		cache.cutout->Release( );
		cache.cutout = nullptr;
	}
	cache.source = sourceTexture.tex;

	IDirect3DTexture9 *cutout = nullptr;
	const int width = std::max( 1, sourceTexture.width );
	const int height = std::max( 1, sourceTexture.height );
	if( FAILED( device->CreateTexture( width, height, 1, 0, D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED, &cutout, nullptr ) ) || !cutout )
	{
		return nullptr;
	}

	D3DLOCKED_RECT sourceLock{};
	D3DLOCKED_RECT destinationLock{};
	if( FAILED( sourceTexture.tex->LockRect( 0, &sourceLock, nullptr, D3DLOCK_READONLY ) ) ||
		FAILED( cutout->LockRect( 0, &destinationLock, nullptr, 0 ) ) )
	{
		if( sourceLock.pBits )
			sourceTexture.tex->UnlockRect( 0 );
		cutout->Release( );
		return nullptr;
	}

	for( int y = 0; y < height; ++y )
	{
		const byte *source = static_cast<const byte *>( sourceLock.pBits ) +
			static_cast<size_t>( y ) * sourceLock.Pitch;
		byte *destination = static_cast<byte *>( destinationLock.pBits ) +
			static_cast<size_t>( y ) * destinationLock.Pitch;

		for( int x = 0; x < width; ++x, source += 4, destination += 4 )
		{
			const int b = source[0];
			const int g = source[1];
			const int r = source[2];
			const int alpha = cof_decal::inkAlpha( r, g, b );
			destination[0] = static_cast<byte>( b );
			destination[1] = static_cast<byte>( g );
			destination[2] = static_cast<byte>( r );
			destination[3] = static_cast<byte>( alpha );
		}
	}

	cutout->UnlockRect( 0 );
	sourceTexture.tex->UnlockRect( 0 );
	cache.cutout = cutout;
	return cache.cutout;
}

struct pending_decal_t
{
	int textureIndex = 0;
	int entityIndex = 0;
	int modelIndex = 0;
	vec3_t position{};
	int flags = 0;
	float scale = 1.0f;
};

static std::vector<pending_decal_t> g_dxPendingDecals;
static bool g_dxDecalMapReady = false;
static bool g_dxReplayDecals = false;
static constexpr size_t DX_MAX_PENDING_DECALS = 4096;

static bool R_DecalPositionEqual( const vec3_t a, const vec3_t b )
{

	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static bool R_DecalRestoreMode( )
{
	if( g_dxReplayDecals )
		return true;

	const connstate_t state =
		static_cast<connstate_t>( ENGINE_GET_PARM( PARM_CONNSTATE ) );
	return state == ca_connected || state == ca_validate;
}

static bool R_HasPendingDecal( int textureIndex, int entityIndex,
	int modelIndex, const vec3_t position )
{
	for( const pending_decal_t &decal : g_dxPendingDecals )
	{
		if( decal.textureIndex == textureIndex &&
			decal.entityIndex == entityIndex &&
			decal.modelIndex == modelIndex &&
			R_DecalPositionEqual( decal.position, position ) )
		{
			return true;
		}
	}
	return false;
}

static bool R_HasRestoreDecal( int textureIndex,
	int entityIndex, int modelIndex, const vec3_t position )
{
	if( !R_DecalRestoreMode( ) )
		return false;

	for( const dx_decal_t &decal : g_dxDecals )
	{
		if( decal.textureIndex == textureIndex &&
			decal.entityIndex == entityIndex &&
			decal.modelIndex == modelIndex &&
			R_DecalPositionEqual( decal.position, position ) )
		{
			return true;
		}
	}
	return false;
}

static void R_QueuePendingDecal( int textureIndex, int entityIndex,
	int modelIndex, const vec3_t position, int flags, float scale )
{
	if( !position || g_dxPendingDecals.size( ) >= DX_MAX_PENDING_DECALS )
		return;

	if( R_HasPendingDecal(
		textureIndex, entityIndex, modelIndex, position ) )
	{
		if( dx_decal_debug.value != 0.0f )
		{
			gEngfuncs.Con_Printf(
				"D3D9 decal: suppressed duplicate pending tex %d ent %d model %d "
				"at %.1f %.1f %.1f\n",
				textureIndex, entityIndex, modelIndex,
				position[0], position[1], position[2] );
		}
		return;
	}

	pending_decal_t pending;
	pending.textureIndex = textureIndex;
	pending.entityIndex = entityIndex;
	pending.modelIndex = modelIndex;
	VectorCopy( position, pending.position );
	pending.flags = flags;
	pending.scale = scale;
	g_dxPendingDecals.push_back( pending );

	if( dx_decal_debug.value != 0.0f )
	{
		gEngfuncs.Con_Printf(
			"D3D9 decal: deferred tex %d ent %d model %d "
			"(map setup, pending=%u)\n",
			textureIndex, entityIndex, modelIndex,
			static_cast<unsigned int>( g_dxPendingDecals.size( ) ) );
	}
}

static void R_DecalTextureName( int textureIndex, char *out, size_t outSize )
{
	if( !out || outSize == 0 )
		return;
	out[0] = '\0';
	if( textureIndex <= 0 || textureIndex > g_texcount )
		return;

	const char *name = g_Textures[textureIndex].name;
	if( !name || !name[0] )
		return;

	const char *base = name;
	for( const char *p = name; *p; ++p )
	{
		if( *p == '/' || *p == '\\' )
			base = p + 1;
	}

	const char *end = base + strlen( base );
	for( const char *p = end; p > base; --p )
	{
		if( p[-1] == '.' )
		{
			end = p - 1;
			break;
		}
		if( p[-1] == '/' || p[-1] == '\\' )
			break;
	}

	const size_t length = static_cast<size_t>( end - base );
	const size_t copyLength = std::min( length, outSize - 1 );
	memcpy( out, base, copyLength );
	out[copyLength] = '\0';
}

static int R_DecalDepth( size_t decalIndex )
{
	if( decalIndex >= g_dxDecals.size( ) )
		return 0;

	const msurface_t *surface = g_dxDecals[decalIndex].surface;
	if( !surface )
		return 0;

	int depth = 0;
	for( size_t i = 0; i < decalIndex; ++i )
	{
		if( g_dxDecals[i].surface == surface )
			++depth;
	}
	return std::min( 255, depth );
}

static bool R_DecalListDuplicate( const decallist_s *list, int count,
	const decallist_s &candidate )
{
	for( int i = 0; i < count; ++i )
	{
		if( list[i].entityIndex != candidate.entityIndex ||
			strcmp( list[i].name, candidate.name ) != 0 )
		{
			continue;
		}

		vec3_t delta;
		VectorSubtract( candidate.position, list[i].position, delta );
		if( VectorLength( delta ) < 2.0f )
			return true;
	}
	return false;
}

int R_CreateDecalList( struct decallist_s *pList )
{
	if( !pList || !g_worldmodel )
		return 0;

	int total = 0;
	for( size_t decalIndex = 0;
		decalIndex < g_dxDecals.size( ) && total < MAX_RENDER_DECALS;
		++decalIndex )
	{
		const dx_decal_t &decal = g_dxDecals[decalIndex];
		if( !decal.surface || decal.textureIndex <= 0 ||
			FBitSet( decal.flags, FDECAL_DONTSAVE ) )
		{
			continue;
		}

		decallist_s entry{};
		VectorCopy( decal.position, entry.position );
		entry.entityIndex = decal.entityIndex;
		entry.depth = static_cast<byte>( R_DecalDepth( decalIndex ) );
		entry.flags = decal.flags;
		entry.scale = decal.scale;
		VectorCopy( decal.normal, entry.impactPlaneNormal );
		R_DecalTextureName(
			decal.textureIndex, entry.name, sizeof( entry.name ) );
		if( !entry.name[0] )
			continue;

		if( R_DecalListDuplicate( pList, total, entry ) )
			continue;

		pList[total++] = entry;
	}

	std::stable_sort( pList, pList + total,
		[]( const decallist_s &a, const decallist_s &b )
		{
			return a.depth < b.depth;
		} );

	if( dx_decal_debug.value != 0.0f )
	{
		gEngfuncs.Con_Printf(
			"D3D9 decals: exported %d/%u decals for save/transition\n",
			total, static_cast<unsigned int>( g_dxDecals.size( ) ) );
	}
	return total;
}

static void R_NormalizeDecalVector( vec3_t v )
{
	const float lengthSq = DotProduct( v, v );
	if( lengthSq <= 0.000001f )
	{
		v[0] = 1.0f;
		v[1] = 0.0f;
		v[2] = 0.0f;
		return;
	}

	const float invLength = 1.0f / sqrtf( lengthSq );
	v[0] *= invLength;
	v[1] *= invLength;
	v[2] *= invLength;
}

static void R_BuildDecalAxes( const msurface_t *surface,
	vec3_t normal, vec3_t tangent, vec3_t bitangent )
{
	if( surface && surface->plane )
	{
		VectorCopy( surface->plane->normal, normal );
		if( surface->flags & SURF_PLANEBACK )
			VectorNegate( normal, normal );
	}
	else
	{
		normal[0] = 0.0f;
		normal[1] = 0.0f;
		normal[2] = 1.0f;
	}
	R_NormalizeDecalVector( normal );

	if( surface && surface->texinfo )
	{
		tangent[0] = surface->texinfo->vecs[0][0];
		tangent[1] = surface->texinfo->vecs[0][1];
		tangent[2] = surface->texinfo->vecs[0][2];

		bitangent[0] = surface->texinfo->vecs[1][0];
		bitangent[1] = surface->texinfo->vecs[1][1];
		bitangent[2] = surface->texinfo->vecs[1][2];
	}
	else
	{
		VectorClear( tangent );
		VectorClear( bitangent );
	}

	if( DotProduct( tangent, tangent ) <= 0.000001f )
	{
		if( fabsf( normal[2] ) < 0.9f )
		{
			tangent[0] = -normal[1];
			tangent[1] = normal[0];
			tangent[2] = 0.0f;
		}
		else
		{
			tangent[0] = 1.0f;
			tangent[1] = 0.0f;
			tangent[2] = 0.0f;
		}
	}
	R_NormalizeDecalVector( tangent );

	if( DotProduct( bitangent, bitangent ) <= 0.000001f )
	{
		CrossProduct( normal, tangent, bitangent );
	}
	R_NormalizeDecalVector( bitangent );
}

static bool R_DecalClipInside( const dx_decal_clip_vertex_t &vertex,
	int axis, float limit, bool keepLess )
{
	const float value = axis == 0 ? vertex.s : vertex.t;
	return keepLess ? value <= limit + 0.001f : value >= limit - 0.001f;
}

static dx_decal_clip_vertex_t R_DecalClipLerp(
	const dx_decal_clip_vertex_t &a,
	const dx_decal_clip_vertex_t &b, int axis, float limit )
{
	const float av = axis == 0 ? a.s : a.t;
	const float bv = axis == 0 ? b.s : b.t;
	float f = 0.0f;
	if( fabsf( bv - av ) > 0.000001f )
		f = ( limit - av ) / ( bv - av );
	f = std::max( 0.0f, std::min( 1.0f, f ) );

	dx_decal_clip_vertex_t out;
	out.position[0] = a.position[0] + ( b.position[0] - a.position[0] ) * f;
	out.position[1] = a.position[1] + ( b.position[1] - a.position[1] ) * f;
	out.position[2] = a.position[2] + ( b.position[2] - a.position[2] ) * f;
	out.s = a.s + ( b.s - a.s ) * f;
	out.t = a.t + ( b.t - a.t ) * f;
	return out;
}

static void R_DecalClipAgainstPlane( std::vector<dx_decal_clip_vertex_t> &poly,
	int axis, float limit, bool keepLess )
{
	if( poly.empty( ) )
		return;

	std::vector<dx_decal_clip_vertex_t> out;
	out.reserve( poly.size( ) + 2 );
	dx_decal_clip_vertex_t previous = poly.back( );
	bool previousInside = R_DecalClipInside( previous, axis, limit, keepLess );

	for( const dx_decal_clip_vertex_t &current : poly )
	{
		const bool currentInside = R_DecalClipInside( current, axis, limit, keepLess );
		if( currentInside != previousInside )
			out.push_back( R_DecalClipLerp( previous, current, axis, limit ) );
		if( currentInside )
			out.push_back( current );

		previous = current;
		previousInside = currentInside;
	}

	poly.swap( out );
}

static bool R_SurfaceCanReceiveDecal( const msurface_t *surface )
{
	if( !surface || !surface->texinfo || !surface->texinfo->texture ||
		!surface->plane || surface->numedges < 3 )
	{
		return false;
	}

	if( surface->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_CONVEYOR ) )
		return false;

	return true;
}

static bool R_BuildDecalOnSurface( const model_t *mod, const msurface_t *surface,
	int textureIndex, const vec3_t origin, float scale,
	std::vector<dx_decal_vertex_t> &outVertices, vec3_t outNormal )
{
	if( !mod || !R_SurfaceCanReceiveDecal( surface ) ||
		textureIndex <= 0 || textureIndex > g_texcount )
	{
		return false;
	}

	const dxtexture_t &texture = g_Textures[textureIndex];
	int textureWidth = static_cast<int>( texture.width );
	int textureHeight = static_cast<int>( texture.height );
	textureWidth = std::max( 1, textureWidth );
	textureHeight = std::max( 1, textureHeight );

	const float decalScale = std::max( 0.01f, std::min( 16.0f, scale ) );
	const float width = static_cast<float>( textureWidth ) / decalScale;
	const float height = static_cast<float>( textureHeight ) / decalScale;
	const float halfWidth = width * 0.5f;
	const float halfHeight = height * 0.5f;
	if( halfWidth <= 0.0f || halfHeight <= 0.0f )
		return false;

	vec3_t normal, tangent, bitangent;
	R_BuildDecalAxes( surface, normal, tangent, bitangent );
	VectorCopy( normal, outNormal );

	const float planeDistance = fabsf( DotProduct( origin, surface->plane->normal ) -
		surface->plane->dist );
	if( planeDistance > std::max( 8.0f, std::max( width, height ) * 0.25f ) )
		return false;

	std::vector<dx_decal_clip_vertex_t> polygon;
	polygon.reserve( surface->numedges );
	for( int i = 0; i < surface->numedges; ++i )
	{
		const mvertex_t *vertex = R_GetBrushSurfaceVertex( mod, surface, i );
		if( !vertex )
			return false;

		dx_decal_clip_vertex_t v;
		v.position[0] = vertex->position[0];
		v.position[1] = vertex->position[1];
		v.position[2] = vertex->position[2];
		vec3_t delta;
		delta[0] = vertex->position[0] - origin[0];
		delta[1] = vertex->position[1] - origin[1];
		delta[2] = vertex->position[2] - origin[2];
		v.s = DotProduct( delta, tangent );
		v.t = DotProduct( delta, bitangent );
		polygon.push_back( v );
	}

	R_DecalClipAgainstPlane( polygon, 0, -halfWidth, false );
	R_DecalClipAgainstPlane( polygon, 0, halfWidth, true );
	R_DecalClipAgainstPlane( polygon, 1, -halfHeight, false );
	R_DecalClipAgainstPlane( polygon, 1, halfHeight, true );
	if( polygon.size( ) < 3 )
		return false;

	auto makeVertex = [&]( const dx_decal_clip_vertex_t &in )
		{
			dx_decal_vertex_t out;
			out.x = in.position[0];
			out.y = in.position[1];
			out.z = in.position[2];
			if( dx_decal_lighting.value != 0.0f &&
				dx_decal_vertex_lighting.value != 0.0f )
			{
				vec3_t lightPoint;
				lightPoint[0] = in.position[0];
				lightPoint[1] = in.position[1];
				lightPoint[2] = in.position[2];
				out.diffuse = R_DecalSurfaceLightColor( surface, lightPoint );
			}
			else
			{
				out.diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );
			}
			out.u = 0.5f + in.s / width;
			out.v = 0.5f + in.t / height;
			return out;
		};

	for( size_t i = 2; i < polygon.size( ); ++i )
	{
		outVertices.push_back( makeVertex( polygon[0] ) );
		outVertices.push_back( makeVertex( polygon[i - 1] ) );
		outVertices.push_back( makeVertex( polygon[i] ) );
	}
	return true;
}

static void R_PruneDecalLimit( )
{
	int limit = static_cast<int>( dx_decal_limit.value );
	limit = std::max( 0, std::min( 8192, limit ) );
	if( limit == 0 )
	{
		g_dxDecals.clear( );
		return;
	}

	if( g_dxDecals.size( ) > static_cast<size_t>( limit ) )
	{
		size_t removeCount =
			g_dxDecals.size( ) - static_cast<size_t>( limit );

		for( auto it = g_dxDecals.begin( );
			it != g_dxDecals.end( ) && removeCount > 0; )
		{
			if( !FBitSet( it->flags, FDECAL_PERMANENT ) )
			{
				it = g_dxDecals.erase( it );
				--removeCount;
			}
			else
			{
				++it;
			}
		}
	}
}


static std::vector<cof_decal::Message> g_cofPendingDecals, g_cofStaticDecals;
static std::vector<cof_decal::Group> g_cofDecalGroups;
static bool g_cofDefinitionsLoaded = false;
static unsigned g_cofVariantSerial = 0;
static void R_LoadCofDecalDefinitions( )
{
 if( g_cofDefinitionsLoaded ) return;
 g_cofDefinitionsLoaded = true;
 const fs_api_t *fs = gEngfuncs.fsapi;
 if( !fs || !fs->Open || !fs->Read || !fs->Close || !fs->FileLength ) return;
 file_t *file = fs->Open( "gfx/decals/decalinfo.txt", "rb", false );
 if( !file ) return;
 const fs_offset_t size = fs->FileLength( file );
 if( size > 0 && size <= 1024 * 1024 ) {
  std::vector<char> text( static_cast<size_t>( size ) );
  if( fs->Read( file, text.data( ), text.size( ) ) == size )
   g_cofDecalGroups = cof_decal::parse( text.data( ), text.size( ) );
 }
 fs->Close( file );
}
static int R_AddCofDecal( const cof_decal::Message &message )
{
 if( !g_dxDecalMapReady || !g_worldmodel || g_worldmodel->type != mod_brush ) {
  if( g_cofPendingDecals.size( ) >= 256 ) return 0;
  g_cofPendingDecals.push_back( message ); return 1;
 }
 R_LoadCofDecalDefinitions( );
 const cof_decal::Group *group = nullptr;
 for( const auto &candidate : g_cofDecalGroups )
  if( candidate.name == message.group ) { group = &candidate; break; }
 if( !group || group->variants.empty( ) ) return 0;
 if( message.flags ) for( const auto &previous : g_cofStaticDecals )
  if( previous.group == message.group && previous.origin == message.origin && previous.normal == message.normal ) return 1;
 const auto &variant = group->variants[g_cofVariantSerial++ % group->variants.size( )];
 const std::string path = "gfx/decals/" + variant.texture + ".tga";
 const int texture = DX_LoadTexture( path.c_str( ), nullptr, 0, TF_CLAMP | TF_NOMIPMAP );
 if( texture <= 0 ) return 0;
 cof_decal::Vec tangent, bitangent;
 cof_decal::axes( message.normal, tangent, bitangent );
 const float radius = sqrtf( variant.halfWidth * variant.halfWidth + variant.halfHeight * variant.halfHeight );
 if( !message.flags ) {
  const float distance = radius * variant.overlap * 2.0f;
  g_dxDecals.erase( std::remove_if( g_dxDecals.begin( ), g_dxDecals.end( ), [&]( const dx_decal_t &d ) {
   const cof_decal::Vec delta{d.position[0]-message.origin[0],d.position[1]-message.origin[1],d.position[2]-message.origin[2]};
   return d.cofCustom && !(d.flags & FDECAL_PERMANENT) && d.textureIndex == texture && cof_decal::dot(delta,delta)<distance*distance;
  }),g_dxDecals.end( ) );
 }
 int firstSurface = 0, surfaceCount = 0;
 if( !R_GetBrushSurfaceRange(g_worldmodel,firstSurface,surfaceCount) ) return 0;
 const size_t before = g_dxDecals.size( );
 for( int i=0; i<surfaceCount; ++i ) {
  const msurface_t *surface = &g_worldmodel->surfaces[firstSurface+i];
  if( !R_SurfaceCanReceiveDecal(surface) || surface->numedges>4096 ) continue;
  cof_decal::Vec normal{surface->plane->normal[0],surface->plane->normal[1],surface->plane->normal[2]};
  const float planeDistance = cof_decal::dot(message.origin,normal)-surface->plane->dist;
  if( surface->flags & SURF_PLANEBACK ) for( float &v : normal ) v=-v;
  if( cof_decal::dot(normal,message.normal)<0.7f || fabsf(planeDistance)>radius ) continue;
  std::vector<dx_decal_clip_vertex_t> polygon;
  polygon.reserve(surface->numedges);
  for( int edge=0; edge<surface->numedges; ++edge ) {
   const mvertex_t *v=R_GetBrushSurfaceVertex(g_worldmodel,surface,edge);
   if(!v){polygon.clear();break;}
   const cof_decal::Vec delta{v->position[0]-message.origin[0],v->position[1]-message.origin[1],v->position[2]-message.origin[2]};
   polygon.push_back({{v->position[0],v->position[1],v->position[2]},cof_decal::dot(delta,tangent),cof_decal::dot(delta,bitangent)});
  }
  R_DecalClipAgainstPlane(polygon,0,-variant.halfWidth,false);
  R_DecalClipAgainstPlane(polygon,0,variant.halfWidth,true);
  R_DecalClipAgainstPlane(polygon,1,-variant.halfHeight,false);
  R_DecalClipAgainstPlane(polygon,1,variant.halfHeight,true);
  if(polygon.size()<3)continue;
  dx_decal_t decal;decal.textureIndex=texture;decal.cofCustom=true;
  decal.flags=static_cast<byte>(FDECAL_DONTSAVE|(message.flags?FDECAL_PERMANENT:0));decal.surface=surface;
  for(int j=0;j<3;++j){decal.position[j]=message.origin[j];decal.normal[j]=normal[j];}
  const auto vertex=[&](const dx_decal_clip_vertex_t &v){return dx_decal_vertex_t{v.position[0],v.position[1],v.position[2],D3DCOLOR_ARGB(255,255,255,255),0.5f+v.s/(2*variant.halfWidth),0.5f+v.t/(2*variant.halfHeight)};};
  for(size_t j=2;j<polygon.size();++j){decal.vertices.push_back(vertex(polygon[0]));decal.vertices.push_back(vertex(polygon[j-1]));decal.vertices.push_back(vertex(polygon[j]));}
  g_dxDecals.push_back(std::move(decal));
 }
 const bool produced=g_dxDecals.size()>before;
 if(produced&&message.flags&&g_cofStaticDecals.size()<4096)g_cofStaticDecals.push_back(message);
 R_PruneDecalLimit();return produced?1:0;
}
extern "C" __declspec(dllexport) int __cdecl DX_CofCustomDecalV1(const void *data,int size)
{
 cof_decal::Message message;
 return cof_decal::decode(data,size,message)?R_AddCofDecal(message):0;
}

void R_AddDecal( int textureIndex, int entityIndex, int modelIndex,
	vec3_t position, int flags, float scale )
{
	R_RegisterModelCvars( );
	if( dx_decals.value == 0.0f || !position )
		return;

	if( !g_dxDecalMapReady && !g_dxReplayDecals )
	{
		R_QueuePendingDecal( textureIndex, entityIndex, modelIndex,
			position, flags, scale );
		return;
	}

	if( !g_worldmodel || g_worldmodel->type != mod_brush )
	{
		if( !g_dxReplayDecals )
			R_QueuePendingDecal( textureIndex, entityIndex, modelIndex,
				position, flags, scale );
		return;
	}

	if( textureIndex <= 0 || textureIndex > g_texcount ||
		!g_Textures[textureIndex].tex )
	{

		if( !g_dxReplayDecals )
			R_QueuePendingDecal( textureIndex, entityIndex, modelIndex,
				position, flags, scale );
		return;
	}

	const model_t *decalModel = g_worldmodel;
	cl_entity_s *decalEntity = nullptr;
	int resolvedModelIndex = modelIndex;

	if( entityIndex > 0 && gEngfuncs.R_BeamGetEntity )
		decalEntity = gEngfuncs.R_BeamGetEntity( entityIndex );

	if( resolvedModelIndex <= 0 && decalEntity )
		resolvedModelIndex = decalEntity->curstate.modelindex;

	bool explicitModelRange = false;
	if( gp_cl && resolvedModelIndex > 0 &&
		resolvedModelIndex <= gp_cl->nummodels &&
		gp_cl->models[resolvedModelIndex] &&
		gp_cl->models[resolvedModelIndex]->type == mod_brush )
	{
		decalModel = gp_cl->models[resolvedModelIndex];
		explicitModelRange = true;
	}
	else if( entityIndex > 0 )
	{

		return;
	}

	vec3_t decalPosition;
	VectorCopy( position, decalPosition );

	if( decalEntity && !FBitSet( flags, FDECAL_LOCAL_SPACE ) )
	{
		D3DMATRIX entityWorld;
		R_SetEntityWorldTransform( decalEntity,
			gp_cl ? static_cast<float>( gp_cl->time ) : 0.0f, &entityWorld );

		D3DXMATRIX inverseWorld;
		if( D3DXMatrixInverse( &inverseWorld, nullptr,
			reinterpret_cast<const D3DXMATRIX *>( &entityWorld ) ) )
		{
			D3DXVECTOR3 worldPoint( position[0], position[1], position[2] );
			D3DXVECTOR3 localPoint;
			D3DXVec3TransformCoord( &localPoint, &worldPoint, &inverseWorld );
			decalPosition[0] = localPoint.x;
			decalPosition[1] = localPoint.y;
			decalPosition[2] = localPoint.z;
			SetBits( flags, FDECAL_LOCAL_SPACE );
		}
	}

	if( decalModel && !FBitSet( decalModel->flags, MODEL_HAS_ORIGIN ) )
		SetBits( flags, FDECAL_USE_LANDMARK );

	if( R_HasRestoreDecal(
		textureIndex, entityIndex, resolvedModelIndex, decalPosition ) )
	{
		if( dx_decal_debug.value != 0.0f )
		{
			gEngfuncs.Con_Printf(
				"D3D9 decal: suppressed restore duplicate tex %d '%s' "
				"ent %d model %d at %.1f %.1f %.1f\n",
				textureIndex, g_Textures[textureIndex].name,
				entityIndex, resolvedModelIndex,
				decalPosition[0], decalPosition[1], decalPosition[2] );
		}
		return;
	}

	int firstSurface = 0;
	int surfaceCount = 0;
	if( explicitModelRange )
	{
		if( !R_GetBrushSurfaceRange( decalModel, firstSurface, surfaceCount ) )
			return;
	}
	else if( resolvedModelIndex > 0 &&
		resolvedModelIndex < g_worldmodel->numsubmodels )
	{
		const dmodel_t &submodel = g_worldmodel->submodels[resolvedModelIndex];
		firstSurface = submodel.firstface;
		surfaceCount = submodel.numfaces;
	}
	else if( !R_GetBrushSurfaceRange( decalModel, firstSurface, surfaceCount ) )
	{
		firstSurface = 0;
		surfaceCount = g_worldmodel->numsurfaces;
	}

	const size_t startCount = g_dxDecals.size( );
	for( int i = 0; i < surfaceCount; ++i )
	{
		const int surfaceIndex = firstSurface + i;
		if( surfaceIndex < 0 || surfaceIndex >= decalModel->numsurfaces )
			continue;

		const msurface_t *surface = &decalModel->surfaces[surfaceIndex];
		dx_decal_t decal;
		decal.textureIndex = textureIndex;
		decal.entityIndex = static_cast<short>( std::max( 0, entityIndex ) );
		decal.modelIndex = static_cast<short>( std::max( 0, resolvedModelIndex ) );
		decal.flags = static_cast<byte>( flags & 0xFF );
		decal.scale = std::max( 0.01f, std::min( 16.0f, scale ) );
		VectorCopy( decalPosition, decal.position );
		if( R_BuildDecalOnSurface( decalModel, surface, textureIndex,
			decalPosition, decal.scale, decal.vertices, decal.normal ) )
		{
			decal.surface = surface;
			decal.dynamicLightStyles = R_DecalHasDynamicStyle( surface );
			decal.lightStyleRevision = g_lightStyleRevision;
			if( dx_decal_lighting.value != 0.0f &&
				dx_decal_vertex_lighting.value == 0.0f )
			{
				R_ApplyDecalLight( decal,
					R_DecalSurfaceLightColor( surface, decalPosition ) );
			}
			g_dxDecals.push_back( std::move( decal ) );
		}
	}

	if( g_dxDecals.size( ) == startCount )
	{
		float bestDistance = FLT_MAX;
		const msurface_t *bestSurface = nullptr;
		for( int i = 0; i < surfaceCount; ++i )
		{
			const int surfaceIndex = firstSurface + i;
			if( surfaceIndex < 0 || surfaceIndex >= decalModel->numsurfaces )
				continue;
			const msurface_t *surface = &decalModel->surfaces[surfaceIndex];
			if( !R_SurfaceCanReceiveDecal( surface ) )
				continue;
			const float distance = fabsf( DotProduct( decalPosition,
				surface->plane->normal ) - surface->plane->dist );
			if( distance < bestDistance )
			{
				bestDistance = distance;
				bestSurface = surface;
			}
		}

		if( bestSurface )
		{
			vec3_t projectedPosition;
			VectorCopy( decalPosition, projectedPosition );
			if( dx_decal_project_fallback.value != 0.0f && bestSurface->plane )
			{
				const float signedDistance = DotProduct( decalPosition,
					bestSurface->plane->normal ) - bestSurface->plane->dist;
				VectorMA( decalPosition, -signedDistance, bestSurface->plane->normal,
					projectedPosition );
			}

			dx_decal_t decal;
			decal.textureIndex = textureIndex;
			decal.entityIndex = static_cast<short>( std::max( 0, entityIndex ) );
			decal.modelIndex = static_cast<short>( std::max( 0, resolvedModelIndex ) );
			decal.flags = static_cast<byte>( flags & 0xFF );
			decal.scale = std::max( 0.01f, std::min( 16.0f, scale ) );
			VectorCopy( projectedPosition, decal.position );
			if( R_BuildDecalOnSurface( decalModel, bestSurface, textureIndex,
				projectedPosition, decal.scale, decal.vertices, decal.normal ) )
			{
				decal.surface = bestSurface;
				decal.dynamicLightStyles = R_DecalHasDynamicStyle( bestSurface );
				decal.lightStyleRevision = g_lightStyleRevision;
				if( dx_decal_lighting.value != 0.0f &&
					dx_decal_vertex_lighting.value == 0.0f )
				{
					R_ApplyDecalLight( decal,
						R_DecalSurfaceLightColor( bestSurface, projectedPosition ) );
				}
				g_dxDecals.push_back( std::move( decal ) );
			}
		}
	}

	if( dx_decal_debug.value != 0.0f )
	{
		const dxtexture_t &debugTexture = g_Textures[textureIndex];
		gEngfuncs.Con_Printf(
			"D3D9 decal: tex %d '%s' texflags 0x%08X alpha %d "
			"decalflags 0x%02X permanent %d ent %d model %d->%d added %u "
			"world %.1f %.1f %.1f local %.1f %.1f %.1f\n",
			textureIndex, debugTexture.name,
			static_cast<unsigned int>( debugTexture.flags ),
			( debugTexture.flags & TF_HAS_ALPHA ) || debugTexture.has_alpha ? 1 : 0,
			flags & 0xFF, FBitSet( flags, FDECAL_PERMANENT ) ? 1 : 0,
			entityIndex, modelIndex, resolvedModelIndex,
			static_cast<unsigned int>( g_dxDecals.size( ) - startCount ),
			position[0], position[1], position[2],
			decalPosition[0], decalPosition[1], decalPosition[2] );
	}

	R_PruneDecalLimit( );
}

void R_ActivateDecals( )
{
	g_dxDecalMapReady = true;
 std::vector<cof_decal::Message> custom;
 custom.swap(g_cofPendingDecals);
 for(const auto &message:custom)R_AddCofDecal(message);
	if( g_dxPendingDecals.empty( ) )
		return;

	std::vector<pending_decal_t> pending;
	pending.swap( g_dxPendingDecals );

	g_dxReplayDecals = true;
	const size_t before = g_dxDecals.size( );
	for( const pending_decal_t &decal : pending )
	{
		vec3_t position;
		VectorCopy( decal.position, position );
		R_AddDecal( decal.textureIndex, decal.entityIndex, decal.modelIndex,
			position, decal.flags, decal.scale );
	}
	g_dxReplayDecals = false;

	if( dx_decal_debug.value != 0.0f )
	{
		gEngfuncs.Con_Printf(
			"D3D9 decals: replayed %u sign-on decals, produced %u fragments\n",
			static_cast<unsigned int>( pending.size( ) ),
			static_cast<unsigned int>( g_dxDecals.size( ) - before ) );
	}
}

void R_ClearDecals( )
{
 g_cofPendingDecals.clear();g_cofStaticDecals.clear();g_cofDecalGroups.clear();
 g_cofDefinitionsLoaded=false;g_cofVariantSerial=0;
	g_dxDecals.clear( );
	g_dxPendingDecals.clear( );
	R_ClearRemixDecalTextures( );
	R_ResetAttachments( );
}

void R_RemoveDecalsByTexture( int textureIndex )
{
	if( textureIndex < 0 )
		return;

	const size_t activeBefore = g_dxDecals.size( );
	const size_t pendingBefore = g_dxPendingDecals.size( );

	g_dxDecals.erase(
		std::remove_if( g_dxDecals.begin( ), g_dxDecals.end( ),
			[textureIndex]( const dx_decal_t &decal )
			{
				if( FBitSet( decal.flags, FDECAL_PERMANENT ) )
					return false;
				return textureIndex == 0 || decal.textureIndex == textureIndex;
			} ),
		g_dxDecals.end( ) );

	g_dxPendingDecals.erase(
		std::remove_if( g_dxPendingDecals.begin( ), g_dxPendingDecals.end( ),
			[textureIndex]( const pending_decal_t &decal )
			{
				if( FBitSet( decal.flags, FDECAL_PERMANENT ) )
					return false;
				return textureIndex == 0 || decal.textureIndex == textureIndex;
			} ),
		g_dxPendingDecals.end( ) );

	if( dx_decal_debug.value != 0.0f )
	{
		gEngfuncs.Con_Printf(
			"D3D9 decals: R_DecalRemoveAll texture %d removed %u active, "
			"%u pending; permanent decals preserved\n",
			textureIndex,
			static_cast<unsigned int>( activeBefore - g_dxDecals.size( ) ),
			static_cast<unsigned int>( pendingBefore - g_dxPendingDecals.size( ) ) );
	}
}

static void R_RefreshDecalLight( dx_decal_t &decal )
{
	if( decal.lightStyleRevision == g_lightStyleRevision )
		return;

	if( !decal.dynamicLightStyles )
	{
		decal.lightStyleRevision = g_lightStyleRevision;
		return;
	}

	if( dx_decal_lighting.value == 0.0f )
	{
		R_ApplyDecalLight( decal,
			D3DCOLOR_ARGB( 255, 255, 255, 255 ) );
		decal.lightStyleRevision = g_lightStyleRevision;
		return;
	}

	if( dx_decal_vertex_lighting.value == 0.0f )
	{
		R_ApplyDecalLight( decal,
			R_DecalSurfaceLightColor( decal.surface, decal.position ) );
		decal.lightStyleRevision = g_lightStyleRevision;
		return;
	}

	for( dx_decal_vertex_t &vertex : decal.vertices )
	{
		vec3_t point;
		point[0] = vertex.x;
		point[1] = vertex.y;
		point[2] = vertex.z;
		vertex.diffuse = R_DecalSurfaceLightColor( decal.surface, point );
	}
	decal.lightStyleRevision = g_lightStyleRevision;
}

static DWORD DX_FloatStateBits( float value )
{
	DWORD bits = 0;
	static_assert( sizeof( bits ) == sizeof( value ), "D3D float state size mismatch" );
	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

static D3DCOLOR R_DecalEntityFactor( const cl_entity_s *ent )
{
	if( !ent )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	const int mode = ent->curstate.rendermode;
	const int renderAmt = std::max( 0, std::min( 255,
		static_cast<int>( ent->curstate.renderamt ) ) );

	switch( mode )
	{
	case kRenderTransColor:
	{
		int r = ent->curstate.rendercolor.r;
		int g = ent->curstate.rendercolor.g;
		int b = ent->curstate.rendercolor.b;
		if( r == 0 && g == 0 && b == 0 )
			r = g = b = 255;
		return D3DCOLOR_ARGB( renderAmt, r, g, b );
	}

	case kRenderTransAdd:

		return D3DCOLOR_ARGB( 255, renderAmt, renderAmt, renderAmt );

	case kRenderTransAlpha:

		if( gp_host && FBitSet( gp_host->features, ENGINE_QUAKE_COMPATIBLE ) )
			return D3DCOLOR_ARGB( renderAmt, 255, 255, 255 );
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	case kRenderNormal:
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	default:

		return D3DCOLOR_ARGB( renderAmt, 255, 255, 255 );
	}
}

static D3DCOLOR R_ModulateDecal( D3DCOLOR diffuse, D3DCOLOR factor )
{
	const unsigned int da = ( diffuse >> 24 ) & 0xFF;
	const unsigned int dr = ( diffuse >> 16 ) & 0xFF;
	const unsigned int dg = ( diffuse >> 8 ) & 0xFF;
	const unsigned int db = diffuse & 0xFF;

	const unsigned int fa = ( factor >> 24 ) & 0xFF;
	const unsigned int fr = ( factor >> 16 ) & 0xFF;
	const unsigned int fg = ( factor >> 8 ) & 0xFF;
	const unsigned int fb = factor & 0xFF;

	return D3DCOLOR_ARGB(
		( da * fa + 127 ) / 255,
		( dr * fr + 127 ) / 255,
		( dg * fg + 127 ) / 255,
		( db * fb + 127 ) / 255 );
}

static void R_DrawEntityDecals( const cl_entity_s *ent )
{
	DX_MODEL_PROFILE_SCOPE( decalProfileScope, DX_PROFILE_PHASE_DECALS );
	if( dx_decals.value == 0.0f || g_dxDecals.empty( ) || !device )
		return;

	const int entityIndex = ent ? ent->curstate.number : 0;
	static std::vector<dx_decal_t *> decalDrawList;
	static std::vector<dx_decal_vertex_t> decalVertexScratch;
	decalDrawList.clear( );

	for( dx_decal_t &decal : g_dxDecals )
	{
		if( decal.entityIndex != entityIndex || decal.vertices.empty( ) ||
			decal.textureIndex <= 0 || decal.textureIndex > g_texcount ||
			!g_Textures[decal.textureIndex].tex )
		{
			continue;
		}

		if( !decal.cofCustom ) R_RefreshDecalLight( decal );
		decalDrawList.push_back( &decal );
	}

	if( decalDrawList.empty( ) )
		return;

	if( dx_decal_debug.value != 0.0f && ent &&
		ent->curstate.rendermode != kRenderNormal )
	{
		static int lastDebugEntity = -1;
		static int lastDebugMode = -1;
		static int lastDebugAmount = -1;
		if( lastDebugEntity != entityIndex ||
			lastDebugMode != ent->curstate.rendermode ||
			lastDebugAmount != ent->curstate.renderamt )
		{
			gEngfuncs.Con_Printf(
				"D3D9 decals: entity %d mode %d renderamt %d drawing %u "
				"decal fragments with parent transparency\n",
				entityIndex, ent->curstate.rendermode,
				static_cast<int>( ent->curstate.renderamt ),
				static_cast<unsigned int>( decalDrawList.size( ) ) );
			lastDebugEntity = entityIndex;
			lastDebugMode = ent->curstate.rendermode;
			lastDebugAmount = ent->curstate.renderamt;
		}
	}

	if( dx_remix_perf.value != 0.0f )
	{
		std::stable_sort( decalDrawList.begin( ), decalDrawList.end( ),
			[]( const dx_decal_t *a, const dx_decal_t *b )
			{
				return a->textureIndex < b->textureIndex;
			} );
	}

	device->SetFVF( BRUSH_FVF );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );

	const float decalBiasStrength = std::max( 0.0f,
		std::min( 4.0f, dx_decal_offset.value ) );
	const float decalDepthBias = dx_remix.value != 0.0f
		? 0.0f : -0.0004f * decalBiasStrength;
	DX_SetRenderStateCached( D3DRS_DEPTHBIAS,
		DX_FloatStateBits( decalDepthBias ) );

	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	R_SetLightmapStageEnabled( false );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	const D3DCOLOR entityDecalFactor = R_DecalEntityFactor( ent );
	const bool modulateEntityDecal =
		entityDecalFactor != D3DCOLOR_ARGB( 255, 255, 255, 255 );
	const bool remixDecal = dx_remix.value != 0.0f;
	const bool remixDecalCutout = remixDecal && dx_remix_decal_cutout.value != 0.0f;
	const float remixOffset = 0.05f + 0.025f * decalBiasStrength;

	for( size_t first = 0; first < decalDrawList.size( ); )
	{
		const int textureIndex = decalDrawList[first]->textureIndex;
		const bool cofCustom = decalDrawList[first]->cofCustom;
		size_t last = first + 1;
		while( last < decalDrawList.size( ) &&
			decalDrawList[last]->textureIndex == textureIndex &&
			decalDrawList[last]->cofCustom == cofCustom )
		{
			++last;
		}

		decalVertexScratch.clear( );
		size_t vertexReserve = 0;
		for( size_t i = first; i < last; ++i )
			vertexReserve += decalDrawList[i]->vertices.size( );
		decalVertexScratch.reserve( vertexReserve );

		for( size_t i = first; i < last; ++i )
		{
			const dx_decal_t &decal = *decalDrawList[i];
			const std::vector<dx_decal_vertex_t> &vertices = decal.vertices;

			if( !modulateEntityDecal && !remixDecal )
			{
				decalVertexScratch.insert( decalVertexScratch.end( ),
					vertices.begin( ), vertices.end( ) );
				continue;
			}

			for( const dx_decal_vertex_t &source : vertices )
			{
				dx_decal_vertex_t vertex = source;
				if( remixDecal )
				{
					// Depth bias is raster-only, so offset Remix decals geometrically.
					vertex.x += decal.normal[0] * remixOffset;
					vertex.y += decal.normal[1] * remixOffset;
					vertex.z += decal.normal[2] * remixOffset;
					vertex.diffuse = D3DCOLOR_ARGB( 255, 255, 255, 255 );
				}
				if( modulateEntityDecal )
					vertex.diffuse = R_ModulateDecal(
						vertex.diffuse, entityDecalFactor );
				decalVertexScratch.push_back( vertex );
			}
		}

		const dxtexture_t &decalTexture = g_Textures[textureIndex];

		const bool hasAlpha = ( decalTexture.flags & TF_HAS_ALPHA ) != 0;
		IDirect3DTexture9 *remixLegacyCutout = nullptr;
		if( ( remixDecal && cofCustom ) || ( remixDecalCutout && !hasAlpha ) )
			remixLegacyCutout = R_RemixLegacyDecalTexture( textureIndex, cofCustom );
		const bool remixCutout = ( remixDecalCutout || ( remixDecal && cofCustom ) ) &&
			( ( hasAlpha && !cofCustom ) || remixLegacyCutout != nullptr );
		// An unconverted gray card must never enter a ray-traced capture.
		if( remixDecal && cofCustom && !remixLegacyCutout ) { first = last; continue; }
		if( remixCutout )
		{
			const DWORD alphaRef = static_cast<DWORD>( std::max( 0.0f,
				std::min( 255.0f, dx_remix_decal_alpha_ref.value ) ) );
			DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
			DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
			DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
			DX_SetRenderStateCached( D3DRS_ALPHAREF, alphaRef );
		}
		else
		{
			const bool premultiplied =
				( decalTexture.flags & TF_PREMULTIPLIED ) != 0;
			DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
			DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
			DX_SetRenderStateCached( D3DRS_SRCBLEND,
				cofCustom ? D3DBLEND_DESTCOLOR :
				( premultiplied ? D3DBLEND_ONE : D3DBLEND_SRCALPHA ) );
			DX_SetRenderStateCached( D3DRS_DESTBLEND,
				cofCustom ? D3DBLEND_SRCCOLOR : D3DBLEND_INVSRCALPHA );
		}

		if( remixDecal )
		{
			DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		}
		else
		{
			DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
			DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		}

		if( remixLegacyCutout )
		{
			DX_BindRawTexture( 0, remixLegacyCutout );
			DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
			DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
		}
		else
		{
			DX_Bind( 0, textureIndex );
		}
		DX_ProfileEvent( DX_PROFILE_EVENT_DECAL_BATCH_DRAWN, 1 );
		R_DrawScratchPrimitive( D3DPT_TRIANGLELIST,
			static_cast<UINT>( decalVertexScratch.size( ) / 3 ),
			static_cast<UINT>( decalVertexScratch.size( ) ),
			decalVertexScratch.data( ), sizeof( dx_decal_vertex_t ) );

		first = last;
	}

	DX_SetRenderStateCached( D3DRS_DEPTHBIAS,
		DX_FloatStateBits( 0.0f ) );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );

	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
}

float R_SurfaceSampleSize( const msurface_t *surface )
{
	float sampleSize = 16.0f;
	if( surface && gEngfuncs.Mod_SampleSizeForFace )
	{
		sampleSize = gEngfuncs.Mod_SampleSizeForFace( const_cast<msurface_t *>( surface ) );
		if( sampleSize <= 0.0f )
			sampleSize = 16.0f;
	}
	return sampleSize;
}
const model_t *g_aoEdgeCacheModel = nullptr;
float g_aoEdgeCacheNeighbour = -999.0f;
float g_aoEdgeCacheCoplanarSuppress = -999.0f;
std::vector<ao_surface_cache_t> g_aoEdgeCache;

bool g_lightStyleUpdateActive = false;

static void R_CellForPoint( const float point[3], int &cellX, int &cellY, int &cellZ )
{
	const int cellSize = R_WorldBatchCellSize( );
	if( cellSize <= 0 )
	{
		cellX = cellY = cellZ = 0;
		return;
	}

	const float invCell = 1.0f / static_cast<float>( cellSize );
	cellX = static_cast<int>( floorf( point[0] * invCell ) );
	cellY = static_cast<int>( floorf( point[1] * invCell ) );
	cellZ = static_cast<int>( floorf( point[2] * invCell ) );
}

void R_CellForLightmapVerts( const lightmap_vertex_t *vertices, int count,
	int &cellX, int &cellY, int &cellZ )
{
	float center[3] = { 0.0f, 0.0f, 0.0f };
	if( !vertices || count <= 0 )
	{
		R_CellForPoint( center, cellX, cellY, cellZ );
		return;
	}

	for( int i = 0; i < count; ++i )
	{
		center[0] += vertices[i].x;
		center[1] += vertices[i].y;
		center[2] += vertices[i].z;
	}
	const float invCount = 1.0f / static_cast<float>( count );
	center[0] *= invCount;
	center[1] *= invCount;
	center[2] *= invCount;
	R_CellForPoint( center, cellX, cellY, cellZ );
}

static void R_ExpandBatchBounds( float mins[3], float maxs[3], bool &valid,
	float x, float y, float z )
{
	mins[0] = std::min( mins[0], x );
	mins[1] = std::min( mins[1], y );
	mins[2] = std::min( mins[2], z );
	maxs[0] = std::max( maxs[0], x );
	maxs[1] = std::max( maxs[1], y );
	maxs[2] = std::max( maxs[2], z );
	valid = true;
}

void R_ExpandWorldBatchBounds( world_material_batch_t &batch,
	const lightmap_vertex_t *vertices, int count )
{
	for( int i = 0; vertices && i < count; ++i )
		R_ExpandBatchBounds( batch.mins, batch.maxs, batch.boundsValid,
			vertices[i].x, vertices[i].y, vertices[i].z );
}

void R_ExpandOverlayBounds(
	world_dynamic_overlay_batch_t &batch,
	const lightmap_vertex_t *vertices, int count )
{
	for( int i = 0; vertices && i < count; ++i )
		R_ExpandBatchBounds( batch.mins, batch.maxs, batch.boundsValid,
			vertices[i].x, vertices[i].y, vertices[i].z );
}

static void R_ExpandWaterBatchBounds( world_water_batch_t &batch,
	const std::vector<std::array<float, 3>> &polygon )
{
	for( const auto &point : polygon )
		R_ExpandBatchBounds( batch.mins, batch.maxs, batch.boundsValid,
			point[0], point[1], point[2] );
}

static bool R_BoundsOverlapWithPadding( const float minsA[3], const float maxsA[3], bool validA,
	const float minsB[3], const float maxsB[3], bool validB, float padding )
{
	if( !validA || !validB )
		return false;

	padding = std::max( 0.0f, padding );
	return minsA[0] <= maxsB[0] + padding && maxsA[0] + padding >= minsB[0] &&
		minsA[1] <= maxsB[1] + padding && maxsA[1] + padding >= minsB[1] &&
		minsA[2] <= maxsB[2] + padding && maxsA[2] + padding >= minsB[2];
}

void R_MarkWaterBatchesVisible( world_model_draw_t &model )
{
	if( model.batches.empty( ) || model.waterBatches.empty( ) )
		return;

	float margin = dx_pvs_water_margin.value;
	if( margin <= 0.0f )
		return;
	margin = std::min( 512.0f, margin );

	for( auto &batch : model.batches )
	{
		if( batch.pvsAlwaysVisible || !batch.boundsValid )
			continue;

		for( const auto &water : model.waterBatches )
		{
			if( R_BoundsOverlapWithPadding( batch.mins, batch.maxs, batch.boundsValid,
				water.mins, water.maxs, water.boundsValid, margin ) )
			{
				batch.pvsAlwaysVisible = true;
				break;
			}
		}
	}
}

static bool R_BoxOutsideFrustumPlane( const float mins[3], const float maxs[3],
	const float normal[3], float dist )
{

	const float x = normal[0] >= 0.0f ? maxs[0] : mins[0];
	const float y = normal[1] >= 0.0f ? maxs[1] : mins[1];
	const float z = normal[2] >= 0.0f ? maxs[2] : mins[2];
	return x * normal[0] + y * normal[1] + z * normal[2] + dist < 0.0f;
}

bool R_BoundsVisibleInCurrentView( const float mins[3], const float maxs[3], bool valid )
{

	if( r_nocull.value != 0.0f || !g_dxCullValid || !valid )
		return true;

	constexpr float cullPadding = 64.0f;
	float expandedMins[3] =
	{
		mins[0] - cullPadding,
		mins[1] - cullPadding,
		mins[2] - cullPadding
	};
	float expandedMaxs[3] =
	{
		maxs[0] + cullPadding,
		maxs[1] + cullPadding,
		maxs[2] + cullPadding
	};

	float normal[3];
	float dist;

	VectorCopy( g_dxCullForward, normal );
	dist = -DotProduct( g_dxCullOrigin, normal ) - g_dxCullNear;
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	normal[0] = -g_dxCullForward[0];
	normal[1] = -g_dxCullForward[1];
	normal[2] = -g_dxCullForward[2];
	dist = DotProduct( g_dxCullOrigin, g_dxCullForward ) + g_dxCullFar;
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	normal[0] = g_dxCullForward[0] * g_dxCullTanX + g_dxCullRight[0];
	normal[1] = g_dxCullForward[1] * g_dxCullTanX + g_dxCullRight[1];
	normal[2] = g_dxCullForward[2] * g_dxCullTanX + g_dxCullRight[2];
	dist = -DotProduct( g_dxCullOrigin, normal );
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	normal[0] = g_dxCullForward[0] * g_dxCullTanX - g_dxCullRight[0];
	normal[1] = g_dxCullForward[1] * g_dxCullTanX - g_dxCullRight[1];
	normal[2] = g_dxCullForward[2] * g_dxCullTanX - g_dxCullRight[2];
	dist = -DotProduct( g_dxCullOrigin, normal );
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	normal[0] = g_dxCullForward[0] * g_dxCullTanY + g_dxCullUp[0];
	normal[1] = g_dxCullForward[1] * g_dxCullTanY + g_dxCullUp[1];
	normal[2] = g_dxCullForward[2] * g_dxCullTanY + g_dxCullUp[2];
	dist = -DotProduct( g_dxCullOrigin, normal );
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	normal[0] = g_dxCullForward[0] * g_dxCullTanY - g_dxCullUp[0];
	normal[1] = g_dxCullForward[1] * g_dxCullTanY - g_dxCullUp[1];
	normal[2] = g_dxCullForward[2] * g_dxCullTanY - g_dxCullUp[2];
	dist = -DotProduct( g_dxCullOrigin, normal );
	if( R_BoxOutsideFrustumPlane( expandedMins, expandedMaxs, normal, dist ) )
		return false;

	return true;
}

static std::vector<byte> g_currentWorldPVS;

static std::vector<byte> g_allVisibleWorldPVS;
static const model_t *g_currentWorldPVSModel = nullptr;
static int g_currentWorldPVSLeaf = -999999;
static int g_currentWorldPVSLeafCount = 0;
static bool g_currentWorldPVSAllVisible = true;
static bool g_currentWorldPVSTransparentWater = false;

static void R_InvalidateCurrentWorldPVS( )
{
	g_currentWorldPVS.clear( );
	g_currentWorldPVSModel = nullptr;
	g_currentWorldPVSLeaf = -999999;
	g_currentWorldPVSLeafCount = 0;
	g_currentWorldPVSAllVisible = true;
	g_currentWorldPVSTransparentWater = false;
}

bool R_WorldPVSAvailable( const model_t *model )
{
	return model && model == g_worldmodel && model->type == mod_brush &&
		model->visdata && model->leafs && model->nodes && model->numleafs > 0;
}

static bool R_WorldPVSEnabledForModel( const model_t *model )
{

	return !g_dxOverviewView &&
		r_novis.value == 0.0f &&
		R_WorldPVSAvailable( model );
}

static const mleaf_t *R_PointInWorldLeaf( const model_t *world, const vec3_t point )
{
	if( !world || !world->nodes || !world->leafs || !point )
		return nullptr;

	const mnode_t *node = world->nodes;
	while( node && node->contents >= 0 )
	{
		const mplane_t *plane = node->plane;
		if( !plane )
			return nullptr;

		const float distance = DotProduct( point, plane->normal ) - plane->dist;
		node = node_child( node, distance < 0.0f ? 1 : 0, world );
	}

	return reinterpret_cast<const mleaf_t *>( node );
}

static bool R_DecompressVis( const byte *input, byte *output, int outputBytes )
{
	if( !output || outputBytes <= 0 )
		return false;

	if( !input )
	{
		memset( output, 0xFF, static_cast<size_t>( outputBytes ) );
		return true;
	}

	byte *out = output;
	byte *outEnd = output + outputBytes;
	size_t encodedBytes = 0;
	const size_t encodedLimit = static_cast<size_t>( outputBytes ) * 2u + 2u;
	while( out < outEnd && encodedBytes < encodedLimit )
	{
		const byte value = *input++;
		++encodedBytes;
		if( value )
		{
			*out++ = value;
			continue;
		}

		if( encodedBytes >= encodedLimit )
			break;
		const int count = *input++;
		++encodedBytes;
		if( count <= 0 )
			break;
		for( int i = 0; i < count && out < outEnd; ++i )
			*out++ = 0;
	}

	if( out != outEnd )
	{
		memset( out, 0, static_cast<size_t>( outEnd - out ) );
		return false;
	}
	return true;
}

static bool R_UseTransparentWaterPVS( )
{

	return dx_water_ripple.value != 0.0f;
}

static bool R_MergeLeafPVS( const model_t *world, int leafBit,
	std::vector<byte> &dst, std::vector<byte> &scratch )
{
	if( !world || leafBit < 0 || leafBit >= world->numleafs )
		return false;

	if( world->numleafs <= 0 || world->numleafs > ( std::numeric_limits<int>::max( ) - 7 ) )
		return false;
	const int rowBytes = ( world->numleafs + 7 ) >> 3;
	if( static_cast<int>( scratch.size( ) ) != rowBytes )
		scratch.resize( static_cast<size_t>( rowBytes ) );

	const mleaf_t &leaf = world->leafs[leafBit + 1];
	R_DecompressVis( leaf.compressed_vis, scratch.data( ), rowBytes );

	bool changed = false;
	for( int i = 0; i < rowBytes; ++i )
	{
		const size_t index = static_cast<size_t>( i );
		const byte merged = static_cast<byte>( dst[index] | scratch[index] );
		if( merged != dst[index] )
		{
			dst[index] = merged;
			changed = true;
		}
	}

	const byte leafMask = static_cast<byte>( 1 << ( leafBit & 7 ) );
	byte &leafByte = dst[leafBit >> 3];
	if( ( leafByte & leafMask ) == 0 )
	{
		leafByte |= leafMask;
		changed = true;
	}

	return changed;
}

static bool R_UpdateCurrentWorldPVS( const world_model_draw_t &drawModel )
{
	const model_t *world = drawModel.sourceModel;
	if( !R_WorldPVSEnabledForModel( world ) )
	{
		g_currentWorldPVSAllVisible = true;
		return false;
	}

	const mleaf_t *leaf = R_PointInWorldLeaf( world, g_vOrigin );
	if( !leaf )
	{
		g_currentWorldPVSAllVisible = true;
		return false;
	}

	const int leafIndex = static_cast<int>( leaf - world->leafs );
	if( leafIndex <= 0 || leafIndex > world->numleafs )
	{
		g_currentWorldPVSAllVisible = true;
		return false;
	}

	const int leafBit = leafIndex - 1;
	const int rowBytes = ( world->numleafs + 7 ) >> 3;
	const bool transparentWater = R_UseTransparentWaterPVS( );

	if( g_currentWorldPVSModel == world &&
		g_currentWorldPVSLeaf == leafBit &&
		g_currentWorldPVSLeafCount == world->numleafs &&
		g_currentWorldPVSTransparentWater == transparentWater &&
		static_cast<int>( g_currentWorldPVS.size( ) ) == rowBytes )
	{
		g_currentWorldPVSAllVisible = false;
		return true;
	}

	g_currentWorldPVS.assign( static_cast<size_t>( rowBytes ), 0 );
	R_DecompressVis(
		leaf->compressed_vis, g_currentWorldPVS.data( ), rowBytes );
	g_currentWorldPVS[leafBit >> 3] |=
		static_cast<byte>( 1 << ( leafBit & 7 ) );

	if( transparentWater && !drawModel.waterPvsPortals.empty( ) )
	{
		std::vector<byte> mergedLeaf(
			static_cast<size_t>( world->numleafs ), 0 );
		std::vector<byte> scratch( static_cast<size_t>( rowBytes ), 0 );
		mergedLeaf[static_cast<size_t>( leafBit )] = 1;

		bool changed = true;
		int iterations = 0;
		const int maxIterations = std::min<int>(
			32, std::max<int>(
				1, static_cast<int>( drawModel.waterPvsPortals.size( ) ) ) );

		while( changed && iterations++ < maxIterations )
		{
			changed = false;

			for( const std::vector<int> &portal : drawModel.waterPvsPortals )
			{
				bool visiblePortal = false;
				for( const int portalLeaf : portal )
				{
					if( portalLeaf < 0 || portalLeaf >= world->numleafs )
						continue;

					if( portalLeaf == leafBit ||
						( g_currentWorldPVS[portalLeaf >> 3] &
							( 1 << ( portalLeaf & 7 ) ) ) )
					{
						visiblePortal = true;
						break;
					}
				}

				if( !visiblePortal )
					continue;

				for( const int portalLeaf : portal )
				{
					if( portalLeaf < 0 || portalLeaf >= world->numleafs )
						continue;
					if( mergedLeaf[static_cast<size_t>( portalLeaf )] )
						continue;

					mergedLeaf[static_cast<size_t>( portalLeaf )] = 1;
					if( R_MergeLeafPVS(
						world, portalLeaf, g_currentWorldPVS, scratch ) )
					{
						changed = true;
					}
				}
			}
		}
	}

	g_currentWorldPVSModel = world;
	g_currentWorldPVSLeaf = leafBit;
	g_currentWorldPVSLeafCount = world->numleafs;
	g_currentWorldPVSTransparentWater = transparentWater;
	g_currentWorldPVSAllVisible = false;
	return true;
}

void R_BuildSurfacePVSLeafMap( const model_t *world,
	surface_pvs_leafs_t &surfaceLeafs )
{
	surfaceLeafs.clear( );
	if( !R_WorldPVSAvailable( world ) || !world->surfaces ||
		world->numsurfaces <= 0 )
	{
		return;
	}

	surfaceLeafs.resize( static_cast<size_t>( world->numsurfaces ) );
	for( int leafIndex = 1; leafIndex <= world->numleafs; ++leafIndex )
	{
		const mleaf_t &leaf = world->leafs[leafIndex];
		if( leaf.nummarksurfaces <= 0 || !leaf.firstmarksurface )
			continue;

		const int pvsLeafBit = leafIndex - 1;
		for( int mark = 0; mark < leaf.nummarksurfaces; ++mark )
		{
			msurface_t *surface = leaf.firstmarksurface[mark];
			if( !surface )
				continue;

			const ptrdiff_t surfaceIndex = surface - world->surfaces;
			if( surfaceIndex < 0 || surfaceIndex >= world->numsurfaces )
				continue;

			surfaceLeafs[static_cast<size_t>( surfaceIndex )].push_back( pvsLeafBit );
		}
	}

	for( auto &leafs : surfaceLeafs )
	{
		std::sort( leafs.begin( ), leafs.end( ) );
		leafs.erase( std::unique( leafs.begin( ), leafs.end( ) ), leafs.end( ) );
	}
}

void R_AddWaterLeafSamples( const model_t *world,
	const msurface_t *surface, std::vector<int> &portalLeaves )
{
	if( !world || !surface || !surface->plane ||
		surface->numedges < 3 || !world->leafs || world->numleafs <= 0 )
	{
		return;
	}

	vec3_t faceNormal;
	VectorCopy( surface->plane->normal, faceNormal );
	if( surface->flags & SURF_PLANEBACK )
		VectorNegate( faceNormal, faceNormal );

	if( faceNormal[2] <= 0.5f )
		return;

	vec3_t center = { 0.0f, 0.0f, 0.0f };
	int validVertices = 0;
	for( int i = 0; i < surface->numedges; ++i )
	{
		const mvertex_t *vertex =
			R_GetBrushSurfaceVertex( world, surface, i );
		if( !vertex )
			continue;

		VectorAdd( center, vertex->position, center );
		++validVertices;
	}

	if( validVertices < 3 )
		return;

	VectorScale( center, 1.0f / static_cast<float>( validVertices ), center );

	static const float sampleOffsets[] = { 2.0f, 4.0f, 8.0f };
	for( const float offset : sampleOffsets )
	{
		for( int side = -1; side <= 1; side += 2 )
		{
			vec3_t sample;
			VectorMA( center, offset * static_cast<float>( side ),
				faceNormal, sample );

			const mleaf_t *leaf = R_PointInWorldLeaf( world, sample );
			if( !leaf || leaf->contents == CONTENTS_SOLID )
				continue;

			const int leafIndex = static_cast<int>( leaf - world->leafs );
			if( leafIndex <= 0 || leafIndex > world->numleafs )
				continue;

			portalLeaves.push_back( leafIndex - 1 );
		}

		std::sort( portalLeaves.begin( ), portalLeaves.end( ) );
		portalLeaves.erase(
			std::unique( portalLeaves.begin( ), portalLeaves.end( ) ),
			portalLeaves.end( ) );
		if( portalLeaves.size( ) >= 2 )
			break;
	}
}

void R_AddSurfacePVSLeafs( std::vector<int> &batchLeafs, bool &alwaysVisible,
	const surface_pvs_leafs_t &surfaceLeafs, int surfaceIndex )
{
	if( surfaceLeafs.empty( ) || surfaceIndex < 0 ||
		surfaceIndex >= static_cast<int>( surfaceLeafs.size( ) ) )
	{
		alwaysVisible = true;
		return;
	}

	const std::vector<int> &leafs = surfaceLeafs[static_cast<size_t>( surfaceIndex )];
	if( leafs.empty( ) )
	{
		alwaysVisible = true;
		return;
	}

	batchLeafs.insert( batchLeafs.end( ), leafs.begin( ), leafs.end( ) );
}

void R_FinalizePVSLeafs( std::vector<int> &batchLeafs, bool alwaysVisible )
{
	if( alwaysVisible || batchLeafs.empty( ) )
		return;
	std::sort( batchLeafs.begin( ), batchLeafs.end( ) );
	batchLeafs.erase( std::unique( batchLeafs.begin( ), batchLeafs.end( ) ),
		batchLeafs.end( ) );
}

bool R_PVSLeafsVisible( const world_model_draw_t &model,
	const std::vector<int> &batchLeafs, bool alwaysVisible )
{
	if( !model.pvsApplicable || alwaysVisible || batchLeafs.empty( ) )
		return true;
	if( !R_UpdateCurrentWorldPVS( model ) || g_currentWorldPVSAllVisible )
		return true;

	for( const int leafBit : batchLeafs )
	{
		if( leafBit < 0 || leafBit >= g_currentWorldPVSLeafCount )
			return true;
		if( g_currentWorldPVS[leafBit >> 3] & ( 1 << ( leafBit & 7 ) ) )
			return true;
	}
	return false;
}

struct static_world_visibility_cache_t
{
	bool valid = false;
	const model_t *sourceModel = nullptr;
	size_t batchCount = 0;
	int pvsLeaf = -999999;
	bool pvsAllVisible = true;
	bool viewCullValid = false;
	float rNovis = -1.0f;
	float rNocull = -1.0f;
	vec3_t origin{};
	vec3_t cullOrigin{};
	vec3_t forward{};
	vec3_t right{};
	vec3_t up{};
	float tanX = 0.0f;
	float tanY = 0.0f;
	float nearPlane = 0.0f;
	float farPlane = 0.0f;
	std::vector<size_t> visibleBatchIndices;
};

static static_world_visibility_cache_t g_staticWorldVisibilityCache;

void R_InvalidateWorldVis( )
{
	g_staticWorldVisibilityCache.valid = false;
	g_staticWorldVisibilityCache.sourceModel = nullptr;
	g_staticWorldVisibilityCache.batchCount = 0;
	g_staticWorldVisibilityCache.visibleBatchIndices.clear( );
}

static bool R_CloseFloat( float a, float b )
{
	return fabsf( a - b ) <= 0.0001f;
}

static bool R_CloseVec3( const vec3_t a, const vec3_t b )
{
	return R_CloseFloat( a[0], b[0] ) &&
		R_CloseFloat( a[1], b[1] ) &&
		R_CloseFloat( a[2], b[2] );
}

static int R_CurrentPVSLeaf( const world_model_draw_t &model,
	bool &allVisible )
{
	allVisible = true;
	if( !R_WorldPVSEnabledForModel( model.sourceModel ) )
		return -1;
	if( !R_UpdateCurrentWorldPVS( model ) || g_currentWorldPVSAllVisible )
		return -1;
	allVisible = false;

	const unsigned int primary =
		static_cast<unsigned int>( g_currentWorldPVSLeaf + 1 ) & 0x00FFFFFFu;
	const unsigned int transparentBit =
		g_currentWorldPVSTransparentWater ? 0x40000000u : 0u;
	return static_cast<int>( primary | transparentBit );
}

static bool R_UsePVSCache(
	const world_model_draw_t &model )
{

	return dx_world_megabatch.value != 0.0f &&
		model.sourceModel == g_worldmodel;
}

static bool R_WorldVisCacheMatches( const world_model_draw_t &model,
	int pvsLeaf, bool pvsAllVisible )
{
	const static_world_visibility_cache_t &cache = g_staticWorldVisibilityCache;

	const bool commonMatch =
		cache.valid &&
		cache.sourceModel == model.sourceModel &&
		cache.batchCount == model.batches.size( ) &&
		cache.pvsLeaf == pvsLeaf &&
		cache.pvsAllVisible == pvsAllVisible &&
		R_CloseFloat( cache.rNovis, r_novis.value );

	if( !commonMatch )
		return false;

	if( R_UsePVSCache( model ) )
	{

		return true;
	}

	return cache.viewCullValid == g_dxCullValid &&
		R_CloseFloat( cache.rNocull, r_nocull.value ) &&
		R_CloseVec3( cache.origin, g_vOrigin ) &&
		R_CloseVec3( cache.cullOrigin, g_dxCullOrigin ) &&
		R_CloseVec3( cache.forward, g_dxCullForward ) &&
		R_CloseVec3( cache.right, g_dxCullRight ) &&
		R_CloseVec3( cache.up, g_dxCullUp ) &&
		R_CloseFloat( cache.tanX, g_dxCullTanX ) &&
		R_CloseFloat( cache.tanY, g_dxCullTanY ) &&
		R_CloseFloat( cache.nearPlane, g_dxCullNear ) &&
		R_CloseFloat( cache.farPlane, g_dxCullFar );
}

const std::vector<size_t> *R_GetVisibleWorldBatches(
	const world_model_draw_t &model )
{
	if( dx_remix_perf.value == 0.0f ||
		dx_remix_perf.value == 0.0f ||
		model.sourceModel != g_worldmodel )
	{
		return nullptr;
	}

	bool pvsAllVisible = true;
	const int pvsLeaf = R_CurrentPVSLeaf( model, pvsAllVisible );
	if( R_WorldVisCacheMatches( model, pvsLeaf, pvsAllVisible ) )
	{
		DX_ProfileEvent( DX_PROFILE_EVENT_VIS_CACHE_HIT, 1 );
		return &g_staticWorldVisibilityCache.visibleBatchIndices;
	}
	DX_ProfileEvent( DX_PROFILE_EVENT_VIS_CACHE_MISS, 1 );

	static_world_visibility_cache_t &cache = g_staticWorldVisibilityCache;
	cache.valid = true;
	cache.sourceModel = model.sourceModel;
	cache.batchCount = model.batches.size( );
	cache.pvsLeaf = pvsLeaf;
	cache.pvsAllVisible = pvsAllVisible;
	cache.viewCullValid = g_dxCullValid;
	cache.rNovis = r_novis.value;
	cache.rNocull = r_nocull.value;
	VectorCopy( g_vOrigin, cache.origin );
	VectorCopy( g_dxCullOrigin, cache.cullOrigin );
	VectorCopy( g_dxCullForward, cache.forward );
	VectorCopy( g_dxCullRight, cache.right );
	VectorCopy( g_dxCullUp, cache.up );
	cache.tanX = g_dxCullTanX;
	cache.tanY = g_dxCullTanY;
	cache.nearPlane = g_dxCullNear;
	cache.farPlane = g_dxCullFar;

	cache.visibleBatchIndices.clear( );
	cache.visibleBatchIndices.reserve( model.batches.size( ) );

	const bool pvsOnlyMegabatch =
		R_UsePVSCache( model );

	for( size_t i = 0; i < model.batches.size( ); ++i )
	{
		const world_material_batch_t &batch = model.batches[i];
		if( batch.primitiveCount == 0 )
			continue;

		if( !R_PVSLeafsVisible( model, batch.pvsLeafs, batch.pvsAlwaysVisible ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_CULLED_PVS, 1 );
			continue;
		}

		if( !pvsOnlyMegabatch &&
			!R_BoundsVisibleInCurrentView(
				batch.mins, batch.maxs, batch.boundsValid ) )
		{
			DX_ProfileEvent( DX_PROFILE_EVENT_WORLD_CULLED_FRUSTUM, 1 );
			continue;
		}

		cache.visibleBatchIndices.push_back( i );
	}
	return &cache.visibleBatchIndices;
}

bool R_WorldMegabatchEnabled( const model_t *mod )
{

	return dx_world_megabatch.value != 0.0f && mod && mod == g_worldmodel;
}

world_material_batch_t &R_FindWorldBatch( world_model_draw_t &model,
	texture_t *baseTexture, int atlasIndex, bool conveyor,
	int cellX, int cellY, int cellZ )
{
	for( auto &batch : model.batches )
	{
		if( batch.baseTexture == baseTexture && batch.atlasIndex == atlasIndex &&
			batch.conveyor == conveyor &&
			batch.cellX == cellX && batch.cellY == cellY && batch.cellZ == cellZ )
		{
			return batch;
		}
	}

	world_material_batch_t batch;
	batch.baseTexture = baseTexture;
	batch.atlasIndex = atlasIndex;
	batch.conveyor = conveyor;
	batch.cellX = cellX;
	batch.cellY = cellY;
	batch.cellZ = cellZ;
	model.batches.push_back( std::move( batch ) );
	return model.batches.back( );
}

world_dynamic_overlay_batch_t &R_FindDynamicOverlayBatch(
	world_model_draw_t &model, texture_t *baseTexture, int overlayAtlasIndex,
	bool conveyor, int cellX, int cellY, int cellZ )
{
	for( auto &batch : model.dynamicOverlayBatches )
	{
		if( batch.baseTexture == baseTexture &&
			batch.overlayAtlasIndex == overlayAtlasIndex &&
			batch.conveyor == conveyor &&
			batch.cellX == cellX && batch.cellY == cellY && batch.cellZ == cellZ )
		{
			return batch;
		}
	}

	world_dynamic_overlay_batch_t batch;
	batch.baseTexture = baseTexture;
	batch.overlayAtlasIndex = overlayAtlasIndex;
	batch.conveyor = conveyor;
	batch.cellX = cellX;
	batch.cellY = cellY;
	batch.cellZ = cellZ;
	model.dynamicOverlayBatches.push_back( std::move( batch ) );
	return model.dynamicOverlayBatches.back( );
}

bool R_UploadWorldBatch( world_material_batch_t &batch )
{
	batch.vertexCount = static_cast<UINT>( batch.vertices.size( ) );
	batch.primitiveCount = static_cast<UINT>( batch.indices.size( ) / 3 );

	if( batch.vertexCount == 0 || batch.primitiveCount == 0 )
		return false;

	HRESULT result = device->CreateVertexBuffer(
		batch.vertexCount * sizeof( lightmap_vertex_t ),
		D3DUSAGE_WRITEONLY, LIGHTMAP_FVF, D3DPOOL_MANAGED,
		&batch.vertexBuffer, nullptr );
	if( FAILED( result ) || !batch.vertexBuffer )
		return false;

	void *vertexMemory = nullptr;
	result = batch.vertexBuffer->Lock( 0, 0, &vertexMemory, 0 );
	if( FAILED( result ) || !vertexMemory )
	{
		batch.vertexBuffer->Release( );
		batch.vertexBuffer = nullptr;
		return false;
	}
	memcpy( vertexMemory, batch.vertices.data( ),
		batch.vertices.size( ) * sizeof( lightmap_vertex_t ) );
	batch.vertexBuffer->Unlock( );

	result = device->CreateIndexBuffer(
		static_cast<UINT>( batch.indices.size( ) * sizeof( DWORD ) ),
		D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED,
		&batch.indexBuffer, nullptr );
	if( FAILED( result ) || !batch.indexBuffer )
	{
		batch.vertexBuffer->Release( );
		batch.vertexBuffer = nullptr;
		return false;
	}

	void *indexMemory = nullptr;
	result = batch.indexBuffer->Lock( 0, 0, &indexMemory, 0 );
	if( FAILED( result ) || !indexMemory )
	{
		batch.indexBuffer->Release( );
		batch.indexBuffer = nullptr;
		batch.vertexBuffer->Release( );
		batch.vertexBuffer = nullptr;
		return false;
	}
	memcpy( indexMemory, batch.indices.data( ), batch.indices.size( ) * sizeof( DWORD ) );
	batch.indexBuffer->Unlock( );

	if( dx_dlight_world.value == 0.0f )
	{
		std::vector<lightmap_vertex_t>( ).swap( batch.vertices );
		if(batch.surfaceRanges.empty()) std::vector<DWORD>( ).swap( batch.indices );
	}
	return true;
}

bool R_UploadWorldBatches( world_model_draw_t &model )
{
	if( !device || model.batches.empty( ) )
		return false;

	size_t totalVertices = 0;
	size_t totalIndices = 0;
	for( auto &batch : model.batches )
	{
		batch.vertexCount = static_cast<UINT>( batch.vertices.size( ) );
		batch.primitiveCount = static_cast<UINT>( batch.indices.size( ) / 3 );
		totalVertices += batch.vertices.size( );
		totalIndices += batch.indices.size( );
	}

	if( totalVertices == 0 || totalIndices == 0 ||
		totalVertices > static_cast<size_t>( INT_MAX ) ||
		totalIndices > static_cast<size_t>( UINT_MAX ) )
	{
		return false;
	}

	const size_t vertexBytes = totalVertices * sizeof( lightmap_vertex_t );
	const size_t indexBytes = totalIndices * sizeof( DWORD );
	if( vertexBytes > static_cast<size_t>( UINT_MAX ) ||
		indexBytes > static_cast<size_t>( UINT_MAX ) )
	{
		return false;
	}

	IDirect3DVertexBuffer9 *sharedVB = nullptr;
	IDirect3DIndexBuffer9 *sharedIB = nullptr;

	HRESULT result = device->CreateVertexBuffer(
		static_cast<UINT>( vertexBytes ),
		D3DUSAGE_WRITEONLY, LIGHTMAP_FVF, D3DPOOL_MANAGED,
		&sharedVB, nullptr );
	if( FAILED( result ) || !sharedVB )
		return false;

	result = device->CreateIndexBuffer(
		static_cast<UINT>( indexBytes ),
		D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED,
		&sharedIB, nullptr );
	if( FAILED( result ) || !sharedIB )
	{
		sharedVB->Release( );
		return false;
	}

	void *vertexMemory = nullptr;
	void *indexMemory = nullptr;
	result = sharedVB->Lock( 0, 0, &vertexMemory, 0 );
	if( FAILED( result ) || !vertexMemory )
	{
		sharedIB->Release( );
		sharedVB->Release( );
		return false;
	}

	result = sharedIB->Lock( 0, 0, &indexMemory, 0 );
	if( FAILED( result ) || !indexMemory )
	{
		sharedVB->Unlock( );
		sharedIB->Release( );
		sharedVB->Release( );
		return false;
	}

	UINT vertexCursor = 0;
	UINT indexCursor = 0;
	byte *vertexDst = static_cast<byte *>( vertexMemory );
	byte *indexDst = static_cast<byte *>( indexMemory );

	for( auto &batch : model.batches )
	{
		batch.sharedBaseVertex = vertexCursor;
		batch.sharedStartIndex = indexCursor;

		if( !batch.vertices.empty( ) )
		{
			const size_t bytes =
				batch.vertices.size( ) * sizeof( lightmap_vertex_t );
			memcpy( vertexDst +
				static_cast<size_t>( vertexCursor ) * sizeof( lightmap_vertex_t ),
				batch.vertices.data( ), bytes );
			vertexCursor += static_cast<UINT>( batch.vertices.size( ) );
		}

		if( !batch.indices.empty( ) )
		{
			const size_t bytes = batch.indices.size( ) * sizeof( DWORD );
			memcpy( indexDst +
				static_cast<size_t>( indexCursor ) * sizeof( DWORD ),
				batch.indices.data( ), bytes );
			indexCursor += static_cast<UINT>( batch.indices.size( ) );
		}
	}

	sharedIB->Unlock( );
	sharedVB->Unlock( );

	model.sharedVertexBuffer = sharedVB;
	model.sharedIndexBuffer = sharedIB;

	const bool keepCpuTriangles = dx_dlight_world.value != 0.0f &&
		dx_dlight_geometry.value != 0.0f &&
		( model.sourceModel != g_worldmodel );
	if( !keepCpuTriangles )
	{
		for( auto &batch : model.batches )
		{
			std::vector<lightmap_vertex_t>( ).swap( batch.vertices );
			if(batch.surfaceRanges.empty()) std::vector<DWORD>( ).swap( batch.indices );
		}
	}

	return true;
}

bool R_UploadOverlayBatches( world_model_draw_t &model )
{
	if( !device || model.dynamicOverlayBatches.empty( ) )
		return false;

	size_t totalVertices = 0;
	size_t totalIndices = 0;
	for( auto &batch : model.dynamicOverlayBatches )
	{
		batch.vertexCount = static_cast<UINT>( batch.vertices.size( ) );
		batch.primitiveCount = static_cast<UINT>( batch.indices.size( ) / 3 );

		const UINT batchIndexCount = static_cast<UINT>( batch.indices.size( ) );
		for( world_dynamic_overlay_range_t &range : batch.ranges )
		{
			if( range.firstIndex >= batchIndexCount )
			{
				range.indexCount = 0;
				continue;
			}
			range.indexCount = std::min(
				range.indexCount, batchIndexCount - range.firstIndex );
			range.indexCount -= range.indexCount % 3;
		}

		totalVertices += batch.vertices.size( );
		totalIndices += batch.indices.size( );
	}
	if( totalVertices == 0 || totalIndices == 0 ||
		totalVertices > static_cast<size_t>( INT_MAX ) ||
		totalIndices > static_cast<size_t>( UINT_MAX ) )
	{
		return false;
	}

	const size_t vertexBytes = totalVertices * sizeof( lightmap_vertex_t );
	const size_t indexBytes = totalIndices * sizeof( DWORD );
	if( vertexBytes > static_cast<size_t>( UINT_MAX ) ||
		indexBytes > static_cast<size_t>( UINT_MAX ) )
		return false;

	IDirect3DVertexBuffer9 *vb = nullptr;
	IDirect3DIndexBuffer9 *ib = nullptr;
	HRESULT result = device->CreateVertexBuffer( static_cast<UINT>( vertexBytes ),
		D3DUSAGE_WRITEONLY, LIGHTMAP_FVF, D3DPOOL_MANAGED, &vb, nullptr );
	if( FAILED( result ) || !vb )
		return false;
	result = device->CreateIndexBuffer( static_cast<UINT>( indexBytes ),
		D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib, nullptr );
	if( FAILED( result ) || !ib )
	{
		vb->Release( );
		return false;
	}

	void *vertexMemory = nullptr;
	void *indexMemory = nullptr;
	if( FAILED( vb->Lock( 0, 0, &vertexMemory, 0 ) ) || !vertexMemory )
	{
		ib->Release( ); vb->Release( ); return false;
	}
	if( FAILED( ib->Lock( 0, 0, &indexMemory, 0 ) ) || !indexMemory )
	{
		vb->Unlock( ); ib->Release( ); vb->Release( ); return false;
	}

	UINT vertexCursor = 0;
	UINT indexCursor = 0;
	byte *vertexDst = static_cast<byte *>( vertexMemory );
	byte *indexDst = static_cast<byte *>( indexMemory );
	for( auto &batch : model.dynamicOverlayBatches )
	{
		batch.sharedBaseVertex = vertexCursor;
		batch.sharedStartIndex = indexCursor;
		if( !batch.vertices.empty( ) )
		{
			memcpy( vertexDst + static_cast<size_t>( vertexCursor ) *
				sizeof( lightmap_vertex_t ), batch.vertices.data( ),
				batch.vertices.size( ) * sizeof( lightmap_vertex_t ) );
			vertexCursor += static_cast<UINT>( batch.vertices.size( ) );
		}
		if( !batch.indices.empty( ) )
		{
			memcpy( indexDst + static_cast<size_t>( indexCursor ) * sizeof( DWORD ),
				batch.indices.data( ), batch.indices.size( ) * sizeof( DWORD ) );
			indexCursor += static_cast<UINT>( batch.indices.size( ) );
		}
	}
	ib->Unlock( );
	vb->Unlock( );
	model.overlaySharedVertexBuffer = vb;
	model.overlaySharedIndexBuffer = ib;

	for( auto &batch : model.dynamicOverlayBatches )
	{
		std::vector<lightmap_vertex_t>( ).swap( batch.vertices );
		std::vector<DWORD>( ).swap( batch.indices );
	}
	return true;
}

static world_water_batch_t &R_FindWorldWaterBatch( world_model_draw_t &model,
	texture_t *baseTexture, int cellX, int cellY, int cellZ )
{
	for( auto &batch : model.waterBatches )
	{
		if( batch.baseTexture == baseTexture &&
			batch.cellX == cellX && batch.cellY == cellY && batch.cellZ == cellZ )
		{
			return batch;
		}
	}

	world_water_batch_t batch;
	batch.baseTexture = baseTexture;
	batch.cellX = cellX;
	batch.cellY = cellY;
	batch.cellZ = cellZ;
	model.waterBatches.push_back( std::move( batch ) );
	return model.waterBatches.back( );
}

static float R_ClampWaterSubdivision( )
{
	return std::max( 32.0f, std::min( 256.0f, dx_water_subdivide.value ) );
}

static bool R_WaterRippleTextureEnabled( )
{
	return dx_water_ripple.value != 0.0f;
}

static float R_WaterQuantizedTime( float time )
{
	const float updateHz = std::max( 0.0f, std::min( 240.0f, dx_water_update_hz.value ) );
	if( updateHz <= 0.0f )
		return time;
	return floorf( time * updateHz + 0.5f ) / updateHz;
}

static float R_WaterEntityAlpha( const cl_entity_s *ent, bool rippleMode )
{

	if( ent && ent->curstate.rendermode != kRenderNormal )
	{
		return std::max( 0.0f, std::min( 1.0f,
			static_cast<float>( ent->curstate.renderamt ) / 255.0f ) );
	}

	return rippleMode ? 0.65f : 1.0f;
}

static D3DCOLOR R_WaterVertexColor( const cl_entity_s *ent, byte shade, bool rippleMode )
{
	int r = shade;
	int g = shade;
	int b = shade;
	if( ent )
	{
		int er = ent->curstate.rendercolor.r;
		int eg = ent->curstate.rendercolor.g;
		int eb = ent->curstate.rendercolor.b;
		if( er == 0 && eg == 0 && eb == 0 )
			er = eg = eb = 255;

		r = ( er * shade + 127 ) / 255;
		g = ( eg * shade + 127 ) / 255;
		b = ( eb * shade + 127 ) / 255;
	}

	const byte a = static_cast<byte>( std::max( 0.0f, std::min( 255.0f,
		R_WaterEntityAlpha( ent, rippleMode ) * 255.0f + 0.5f ) ) );
	return D3DCOLOR_ARGB( a, r, g, b );
}

static const std::array<float, 256> &R_WaterTurbTable( )
{

	static std::array<float, 256> table{};
	static bool tableReady = false;
	if( !tableReady )
	{
		for( int i = 0; i < 256; ++i )
		{
			const float angle = static_cast<float>( i ) * 6.28318530718f / 256.0f;
			table[i] = sinf( angle ) * 8.0f;
		}
		tableReady = true;
	}
	return table;
}

static float R_WaterTurb( float phase )
{
	const int index = static_cast<int>( phase * ( 256.0f / 6.28318530718f ) ) & 255;
	return R_WaterTurbTable( )[index];
}

static float R_WaterTurbIndex( float rawIndex )
{
	const int index = static_cast<int>( rawIndex ) & 255;
	return R_WaterTurbTable( )[index];
}

static float R_WaterGLVerticalWaveZ( const water_source_vertex_t &source,
	const cl_entity_s *ent, float time )
{
	// Remix replacements require source positions independent of time and eye height.
	if( dx_remix.value != 0.0f )
		return source.z;

	const float scale = ent ? ent->curstate.scale : 0.0f;
	if( fabsf( scale ) <= 0.0001f )
		return source.z;

	const float waveHeight = ( source.z >= g_vOrigin[2] ) ? -scale : scale;
	float nv = R_WaterTurbIndex( time * 160.0f + source.y + source.x ) + 8.0f;
	nv += ( R_WaterTurbIndex( source.x * 5.0f + time * 171.0f - source.y ) + 8.0f ) * 0.8f;
	return source.z + nv * waveHeight;
}

static int R_RippleNextPowerOfTwo( int value )
{
	int result = 1;
	while( result < value && result < 4096 )
		result <<= 1;
	return result;
}

static int R_RippleClampDimension( int value )
{

	value = std::max( 16, std::min( 512, value ) );
	return std::max( 16, std::min( 512, R_RippleNextPowerOfTwo( value ) ) );
}

static int R_RippleRequestedMaxDimension( )
{
	int requested = static_cast<int>( dx_water_ripple_resolution.value );
	if( requested <= 0 )
		return 0;
	return R_RippleClampDimension( requested );
}

static size_t R_RippleTexelCount( int size )
{
	return static_cast<size_t>( size ) * static_cast<size_t>( size );
}

static constexpr int RIPPLE_SIMULATION_SIZE = 128;
static constexpr float RIPPLE_SIMULATION_SCALE = 1.0f / RIPPLE_SIMULATION_SIZE;

struct water_ripple_anim_t
{
	std::vector<short> buffers[2];
	short *current = nullptr;
	short *previous = nullptr;
	int size = 0;
	int mask = 0;
	float lastThinkTime = -9999.0f;
	unsigned int serial = 1;
	bool initialized = false;
};

static water_ripple_anim_t g_waterRipples;

struct water_ripple_sample_t
{
	int baseSourceX = 0;
	int baseSourceY = 0;
	int i00 = 0;
	int i10 = 0;
	int i01 = 0;
	int i11 = 0;
	int wx = 0;
	int wy = 0;
};

struct water_ripple_texture_cache_t
{
	IDirect3DTexture9 *texture = nullptr;
	std::vector<D3DCOLOR> pixels;
	std::vector<water_ripple_sample_t> sampleMap;
	int textureId = 0;
	int width = 0;
	int height = 0;
	int sampleSourceWidth = 0;
	int sampleSourceHeight = 0;
	unsigned int serial = 0;
	float lastUploadTime = -9999.0f;
	bool dynamicTexture = false;
};

static std::array<water_ripple_texture_cache_t *, 8096>
g_waterRippleTextureCacheById{};
static std::deque<water_ripple_texture_cache_t>
g_waterRippleCache;

static water_ripple_texture_cache_t &R_GetWaterRippleCache( int textureId )
{
	static water_ripple_texture_cache_t invalidCache;
	if( textureId < 0 ||
		static_cast<size_t>( textureId ) >= g_waterRippleTextureCacheById.size( ) )
	{
		return invalidCache;
	}

	water_ripple_texture_cache_t *&cache =
		g_waterRippleTextureCacheById[static_cast<size_t>( textureId )];
	if( !cache )
	{
		g_waterRippleCache.emplace_back( );
		cache = &g_waterRippleCache.back( );
	}
	return *cache;
}

static float R_RippleUpdateInterval( )
{
	const float hz = std::max( 0.0f, std::min( 240.0f, dx_water_ripple_hz.value ) );
	if( hz <= 0.0f )
		return 0.0f;
	return 1.0f / hz;
}

static float R_RippleUploadInterval( int width, int height )
{
	const float explicitHz = std::max( 0.0f, std::min( 240.0f, dx_water_ripple_upload_hz.value ) );
	if( explicitHz > 0.0f )
		return 1.0f / explicitHz;

	const float maxMegapixelsPerSecond =
		std::max( 1.0f, std::min( 256.0f, dx_water_ripple_upload_mpix.value ) );
	const float pixels = static_cast<float>( std::max( 1, width ) ) *
		static_cast<float>( std::max( 1, height ) );
	const float maxUploadsPerSecond =
		( maxMegapixelsPerSecond * 1000000.0f ) / pixels;

	const float simHz = std::max( 1.0f, std::min( 240.0f, dx_water_ripple_hz.value ) );
	const float effectiveHz = std::max( 1.0f, std::min( simHz, maxUploadsPerSecond ) );
	return 1.0f / effectiveHz;
}

static void R_FreeRippleCache( water_ripple_texture_cache_t &cache, bool releaseManaged )
{
	if( cache.texture && ( releaseManaged || cache.dynamicTexture ) )
	{
		cache.texture->Release( );
		cache.texture = nullptr;
		cache.pixels.clear( );
		cache.sampleMap.clear( );
		cache.textureId = 0;
		cache.width = 0;
		cache.height = 0;
		cache.sampleSourceWidth = 0;
		cache.sampleSourceHeight = 0;
		cache.serial = 0;
		cache.lastUploadTime = -9999.0f;
		cache.dynamicTexture = false;
	}
}

void R_FreeRippleCaches( bool releaseManaged )
{
	for( auto &cache : g_waterRippleCache )
		R_FreeRippleCache( cache, releaseManaged );

	if( releaseManaged )
	{
		g_waterRippleTextureCacheById.fill( nullptr );
		g_waterRippleCache.clear( );
	}
}

static void R_ResetWaterRipples( float time )
{
	const int size = RIPPLE_SIMULATION_SIZE;
	g_waterRipples.size = size;
	g_waterRipples.mask = size - 1;
	g_waterRipples.buffers[0].assign( R_RippleTexelCount( size ), 0 );
	g_waterRipples.buffers[1].assign( R_RippleTexelCount( size ), 0 );
	g_waterRipples.current = g_waterRipples.buffers[0].data( );
	g_waterRipples.previous = g_waterRipples.buffers[1].data( );
	g_waterRipples.lastThinkTime = time;
	++g_waterRipples.serial;
	g_waterRipples.initialized = true;
}

static void R_SpawnWaterRipple( )
{
	const int size = g_waterRipples.size;
	if( !g_waterRipples.previous || size < 16 )
		return;

	const int x = 4 + ( rand( ) % ( size - 8 ) );
	const int y = 4 + ( rand( ) % ( size - 8 ) );
	const short strength = static_cast<short>( 2048 + ( rand( ) & 2047 ) );
	for( int dy = -1; dy <= 1; ++dy )
	{
		for( int dx = -1; dx <= 1; ++dx )
			g_waterRipples.previous[( y + dy ) * size + x + dx] += strength;
	}
}

static void R_AnimateWaterRipples( float time )
{
	const int wantedSize = RIPPLE_SIMULATION_SIZE;
	if( !g_waterRipples.initialized || g_waterRipples.size != wantedSize ||
		time < g_waterRipples.lastThinkTime )
	{
		R_ResetWaterRipples( time );
		R_SpawnWaterRipple( );
		return;
	}

	const float updateInterval = R_RippleUpdateInterval( );
	if( updateInterval > 0.0f &&
		time - g_waterRipples.lastThinkTime < updateInterval )
	{
		return;
	}

	if( updateInterval <= 0.0f &&
		fabsf( time - g_waterRipples.lastThinkTime ) < 0.000001f )
	{
		return;
	}

	std::swap( g_waterRipples.current, g_waterRipples.previous );
	short *current = g_waterRipples.current;
	short *previous = g_waterRipples.previous;
	const int size = g_waterRipples.size;
	const int mask = g_waterRipples.mask;

	auto rippleStepRows = [&]( int beginY, int endY )
		{
			for( int y = beginY; y < endY; ++y )
			{
				const int yUp = ( y - 1 ) & mask;
				const int yDown = ( y + 1 ) & mask;
				for( int x = 0; x < size; ++x )
				{
					const int xLeft = ( x - 1 ) & mask;
					const int xRight = ( x + 1 ) & mask;
					const int index = y * size + x;
					int value = previous[y * size + xLeft] +
						previous[y * size + xRight] +
						previous[yUp * size + x] +
						previous[yDown * size + x];
					value = ( value >> 1 ) - current[index];
					value -= value >> 5;
					current[index] = static_cast<short>( std::max( -32768, std::min( 32767, value ) ) );
				}
			}
		};

	if( DX_ThreadRipples( size * size ) )
		DX_ParallelFor( size, 16, rippleStepRows );
	else
		rippleStepRows( 0, size );

	if( rand( ) < RAND_MAX / 3 )
		R_SpawnWaterRipple( );

	g_waterRipples.lastThinkTime = time;
	++g_waterRipples.serial;
}

static D3DCOLOR R_ReadRippleSourcePixel( const byte *sourcePixels,
	int sourceWidth, int sourceHeight, int x, int y )
{
	if( !sourcePixels || sourceWidth <= 0 || sourceHeight <= 0 )
		return D3DCOLOR_ARGB( 255, 255, 255, 255 );

	x %= sourceWidth;
	y %= sourceHeight;
	if( x < 0 ) x += sourceWidth;
	if( y < 0 ) y += sourceHeight;

	const byte *pixel = sourcePixels +
		( static_cast<size_t>( y ) * sourceWidth + x ) * 4;
	const byte alpha = pixel[3] ? pixel[3] : 255;
	return D3DCOLOR_ARGB( alpha, pixel[0], pixel[1], pixel[2] );
}

static void R_GetWaterRippleTextureSize( const texture_t *texture,
	const dxtexture_t &sourceTexture, int &width, int &height )
{
	const int requestedMaxSize = R_RippleRequestedMaxDimension( );
	int sourceWidth = 64;
	if( sourceTexture.original && sourceTexture.original->width > 0 )
		sourceWidth = static_cast<int>( sourceTexture.original->width );
	else if( sourceTexture.width > 0 )
		sourceWidth = static_cast<int>( sourceTexture.width );
	else if( texture && texture->width > 0 )
		sourceWidth = static_cast<int>( texture->width );
	sourceWidth = std::max( 1, sourceWidth );

	int sourceHeight = 64;
	if( sourceTexture.original && sourceTexture.original->height > 0 )
		sourceHeight = static_cast<int>( sourceTexture.original->height );
	else if( sourceTexture.height > 0 )
		sourceHeight = static_cast<int>( sourceTexture.height );
	else if( texture && texture->height > 0 )
		sourceHeight = static_cast<int>( texture->height );
	sourceHeight = std::max( 1, sourceHeight );

	if( requestedMaxSize <= 0 )
	{

		width = R_RippleClampDimension( sourceWidth );
		height = R_RippleClampDimension( sourceHeight );
		return;
	}

	width = height = requestedMaxSize;
	if( sourceWidth > sourceHeight )
	{
		width = requestedMaxSize;
		height = R_RippleClampDimension( static_cast<int>(
			static_cast<float>( sourceHeight ) / sourceWidth * requestedMaxSize + 0.5f ) );
	}
	else if( sourceHeight > sourceWidth )
	{
		height = requestedMaxSize;
		width = R_RippleClampDimension( static_cast<int>(
			static_cast<float>( sourceWidth ) / sourceHeight * requestedMaxSize + 0.5f ) );
	}
}

static bool R_EnsureWaterRippleTexture( water_ripple_texture_cache_t &cache,
	texture_t *texture, const dxtexture_t &sourceTexture, int textureId )
{
	if( !device || !texture || textureId <= 0 )
		return false;

	int rippleTextureWidth = 0;
	int rippleTextureHeight = 0;
	R_GetWaterRippleTextureSize( texture, sourceTexture, rippleTextureWidth, rippleTextureHeight );
	const bool wantDynamicTexture = dx_water_ripple_dynamic.value != 0.0f;

	if( cache.texture && cache.textureId == textureId &&
		cache.width == rippleTextureWidth &&
		cache.height == rippleTextureHeight &&
		cache.dynamicTexture == wantDynamicTexture )
	{
		return true;
	}

	R_FreeRippleCache( cache, true );

	cache.textureId = 0;
	cache.serial = 0;
	cache.lastUploadTime = -9999.0f;
	cache.width = rippleTextureWidth;
	cache.height = rippleTextureHeight;
	cache.sampleSourceWidth = 0;
	cache.sampleSourceHeight = 0;
	cache.dynamicTexture = false;
	cache.pixels.clear( );
	cache.sampleMap.clear( );

	HRESULT result = E_FAIL;
	if( wantDynamicTexture )
	{
		result = device->CreateTexture( rippleTextureWidth, rippleTextureHeight, 1,
			D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
			&cache.texture, nullptr );
		cache.dynamicTexture = SUCCEEDED( result ) && cache.texture;
	}

	if( !cache.texture )
	{
		result = device->CreateTexture( rippleTextureWidth, rippleTextureHeight, 1, 0,
			D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cache.texture, nullptr );
		cache.dynamicTexture = false;
	}

	if( FAILED( result ) || !cache.texture )
	{
		cache.texture = nullptr;
		cache.pixels.clear( );
		cache.sampleMap.clear( );
		cache.width = cache.height = 0;
		cache.sampleSourceWidth = cache.sampleSourceHeight = 0;
		cache.serial = 0;
		cache.lastUploadTime = -9999.0f;
		cache.dynamicTexture = false;
		return false;
	}

	cache.textureId = textureId;
	return true;
}

static bool R_RebuildWaterRippleSampleMap( water_ripple_texture_cache_t &cache,
	int sourceWidth, int sourceHeight )
{
	if( cache.width <= 0 || cache.height <= 0 ||
		sourceWidth <= 0 || sourceHeight <= 0 )
	{
		cache.sampleMap.clear( );
		cache.sampleSourceWidth = cache.sampleSourceHeight = 0;
		return false;
	}

	if( cache.sampleSourceWidth == sourceWidth &&
		cache.sampleSourceHeight == sourceHeight &&
		cache.sampleMap.size( ) == static_cast<size_t>( cache.width ) * cache.height )
	{
		return true;
	}

	cache.sampleMap.resize( static_cast<size_t>( cache.width ) * cache.height );
	const int simSize = RIPPLE_SIMULATION_SIZE;
	const int simMask = simSize - 1;

	auto buildSampleRows = [&]( int beginY, int endY )
		{
			for( int y = beginY; y < endY; ++y )
			{
				const float v = ( static_cast<float>( y ) + 0.5f ) /
					static_cast<float>( std::max( 1, cache.height ) );
				const float simY = v * simSize;
				const int y0 = static_cast<int>( floorf( simY ) ) & simMask;
				const int y1 = ( y0 + 1 ) & simMask;
				const float fy = simY - floorf( simY );
				const int wy = std::max( 0, std::min( 65535,
					static_cast<int>( fy * 65536.0f + 0.5f ) ) );

				for( int x = 0; x < cache.width; ++x )
				{
					const float u = ( static_cast<float>( x ) + 0.5f ) /
						static_cast<float>( std::max( 1, cache.width ) );
					const float simX = u * simSize;
					const int x0 = static_cast<int>( floorf( simX ) ) & simMask;
					const int x1 = ( x0 + 1 ) & simMask;
					const float fx = simX - floorf( simX );
					const int wx = std::max( 0, std::min( 65535,
						static_cast<int>( fx * 65536.0f + 0.5f ) ) );

					water_ripple_sample_t &sample = cache.sampleMap[
						static_cast<size_t>( y ) * cache.width + x];
					sample.baseSourceX = ( x * sourceWidth ) / cache.width;
					sample.baseSourceY = ( y * sourceHeight ) / cache.height;
					sample.i00 = y0 * simSize + x0;
					sample.i10 = y0 * simSize + x1;
					sample.i01 = y1 * simSize + x0;
					sample.i11 = y1 * simSize + x1;
					sample.wx = wx;
					sample.wy = wy;
				}
			}
		};

	if( DX_ThreadRipples( cache.width * cache.height ) )
		DX_ParallelFor( cache.height, 16, buildSampleRows );
	else
		buildSampleRows( 0, cache.height );

	cache.sampleSourceWidth = sourceWidth;
	cache.sampleSourceHeight = sourceHeight;
	return true;
}

static int R_RippleRoundDiv( int value, int divisor )
{
	if( divisor <= 0 )
		return 0;
	return value >= 0 ? ( value + divisor / 2 ) / divisor :
		( value - divisor / 2 ) / divisor;
}

static int R_RippleSampleHeightCached( const water_ripple_sample_t &sample )
{
	const short *current = g_waterRipples.current;
	if( !current )
		return 0;

	const int wx = sample.wx;
	const int wy = sample.wy;
	const int invWx = 65536 - wx;
	const int invWy = 65536 - wy;
	const int h00 = static_cast<int>( current[sample.i00] );
	const int h10 = static_cast<int>( current[sample.i10] );
	const int h01 = static_cast<int>( current[sample.i01] );
	const int h11 = static_cast<int>( current[sample.i11] );
	const int hx0 = ( h00 * invWx + h10 * wx ) >> 16;
	const int hx1 = ( h01 * invWx + h11 * wx ) >> 16;
	return ( hx0 * invWy + hx1 * wy ) >> 16;
}

static IDirect3DTexture9 *R_UpdateWaterRippleTexture( texture_t *texture, float time )
{
	if( !R_WaterRippleTextureEnabled( ) || !texture || texture->gl_texturenum <= 0 ||
		texture->gl_texturenum > g_texcount )
	{
		return nullptr;
	}

	const int textureId = texture->gl_texturenum;
	const dxtexture_t &sourceTexture = g_Textures[textureId];
	const byte *sourcePixels = sourceTexture.original ? sourceTexture.original->buffer : nullptr;
	if( !sourcePixels )
		return nullptr;

	water_ripple_texture_cache_t &cache = R_GetWaterRippleCache( textureId );
	if( !R_EnsureWaterRippleTexture( cache, texture, sourceTexture, textureId ) )
		return nullptr;

	R_AnimateWaterRipples( time );
	if( cache.serial == g_waterRipples.serial )
		return cache.texture;

	int sourceWidth = sourceTexture.width > 0 ? static_cast<int>( sourceTexture.width ) : 1;
	if( sourceTexture.original && sourceTexture.original->width > 0 )
		sourceWidth = static_cast<int>( sourceTexture.original->width );
	sourceWidth = std::max( 1, sourceWidth );

	int sourceHeight = sourceTexture.height > 0 ? static_cast<int>( sourceTexture.height ) : 1;
	if( sourceTexture.original && sourceTexture.original->height > 0 )
		sourceHeight = static_cast<int>( sourceTexture.original->height );
	sourceHeight = std::max( 1, sourceHeight );
	const int rippleWidth = cache.width;
	const int rippleHeight = cache.height;
	if( rippleWidth <= 0 || rippleHeight <= 0 )
		return nullptr;

	if( !R_RebuildWaterRippleSampleMap( cache, sourceWidth, sourceHeight ) )
		return nullptr;

	const float uploadInterval = R_RippleUploadInterval( rippleWidth, rippleHeight );
	if( cache.serial != 0 && time >= cache.lastUploadTime &&
		time - cache.lastUploadTime < uploadInterval )
	{
		return cache.texture;
	}

	cache.pixels.resize( static_cast<size_t>( rippleWidth ) * rippleHeight );
	auto fillRippleTextureRows = [&]( int beginY, int endY )
		{
			for( int y = beginY; y < endY; ++y )
			{
				D3DCOLOR *dst = cache.pixels.data( ) +
					static_cast<size_t>( y ) * rippleWidth;
				const water_ripple_sample_t *row = cache.sampleMap.data( ) +
					static_cast<size_t>( y ) * rippleWidth;
				for( int x = 0; x < rippleWidth; ++x )
				{
					const water_ripple_sample_t &sample = row[x];
					const int height = R_RippleSampleHeightCached( sample );
					const int rippleX = R_RippleRoundDiv( height * rippleWidth, 65536 );
					const int rippleY = R_RippleRoundDiv( height * rippleHeight, 65536 );
					const int sx = sample.baseSourceX +
						R_RippleRoundDiv( rippleX * sourceWidth, rippleWidth );
					const int sy = sample.baseSourceY -
						R_RippleRoundDiv( rippleY * sourceHeight, rippleHeight );
					dst[x] = R_ReadRippleSourcePixel( sourcePixels, sourceWidth, sourceHeight, sx, sy );
				}
			}
		};

	if( DX_ThreadRipples( rippleWidth * rippleHeight ) )
		DX_ParallelFor( rippleHeight, 32, fillRippleTextureRows );
	else
		fillRippleTextureRows( 0, rippleHeight );

	D3DLOCKED_RECT locked;
	const DWORD lockFlags = cache.dynamicTexture ? D3DLOCK_DISCARD : D3DLOCK_NO_DIRTY_UPDATE;
	if( FAILED( cache.texture->LockRect( 0, &locked, nullptr, lockFlags ) ) )
	{
		return nullptr;
	}

	for( int y = 0; y < rippleHeight; ++y )
	{
		void *dst = static_cast<byte *>( locked.pBits ) +
			static_cast<size_t>( y ) * locked.Pitch;
		const void *src = cache.pixels.data( ) +
			static_cast<size_t>( y ) * rippleWidth;
		memcpy( dst, src, static_cast<size_t>( rippleWidth ) * sizeof( D3DCOLOR ) );
	}
	cache.texture->UnlockRect( 0 );

	if( !cache.dynamicTexture )
	{
		RECT rect{ 0, 0, rippleWidth, rippleHeight };
		cache.texture->AddDirtyRect( &rect );
	}
	cache.serial = g_waterRipples.serial;
	cache.lastUploadTime = time;
	return cache.texture;
}

static void R_AddWaterPolygon( world_water_batch_t &batch, const mtexinfo_t *texinfo,
	const std::vector<std::array<float, 3>> &polygon )
{
	if( !texinfo || polygon.size( ) < 3 )
		return;

	const DWORD baseVertex = static_cast<DWORD>( batch.sources.size( ) );
	for( const auto &position : polygon )
	{
		water_source_vertex_t vertex;
		vertex.x = position[0];
		vertex.y = position[1];
		vertex.z = position[2];
		vertex.s = DotProduct( position.data( ), texinfo->vecs[0] );
		vertex.t = DotProduct( position.data( ), texinfo->vecs[1] );
		batch.sources.push_back( vertex );
	}

	for( DWORD i = 2; i < static_cast<DWORD>( polygon.size( ) ); ++i )
	{
		batch.indices.push_back( baseVertex );
		batch.indices.push_back( baseVertex + i - 1 );
		batch.indices.push_back( baseVertex + i );
	}
}

static void R_SubdivideWaterPolygon( world_water_batch_t &batch,
	const mtexinfo_t *texinfo, const std::vector<std::array<float, 3>> &polygon,
	int recursionDepth = 0 )
{
	if( polygon.size( ) < 3 )
		return;
	if( recursionDepth > 64 )
	{
		R_AddWaterPolygon( batch, texinfo, polygon );
		return;
	}

	const float subdivision = R_ClampWaterSubdivision( );
	for( int axis = 0; axis < 3; ++axis )
	{
		float mins = polygon[0][axis];
		float maxs = polygon[0][axis];
		for( const auto &position : polygon )
		{
			mins = std::min( mins, position[axis] );
			maxs = std::max( maxs, position[axis] );
		}

		float split = subdivision * floorf( ( mins + maxs ) * 0.5f / subdivision + 0.5f );
		if( maxs - split < 8.0f || split - mins < 8.0f )
			continue;

		std::vector<std::array<float, 3>> front;
		std::vector<std::array<float, 3>> back;
		front.reserve( polygon.size( ) + 4 );
		back.reserve( polygon.size( ) + 4 );

		for( size_t i = 0; i < polygon.size( ); ++i )
		{
			const auto &current = polygon[i];
			const auto &next = polygon[( i + 1 ) % polygon.size( )];
			const float currentDistance = current[axis] - split;
			const float nextDistance = next[axis] - split;

			if( currentDistance >= 0.0f )
				front.push_back( current );
			if( currentDistance <= 0.0f )
				back.push_back( current );

			if( ( currentDistance > 0.0f && nextDistance < 0.0f ) ||
				( currentDistance < 0.0f && nextDistance > 0.0f ) )
			{
				const float fraction = currentDistance / ( currentDistance - nextDistance );
				std::array<float, 3> midpoint{};
				for( int component = 0; component < 3; ++component )
				{
					midpoint[component] = current[component] +
						( next[component] - current[component] ) * fraction;
				}
				front.push_back( midpoint );
				back.push_back( midpoint );
			}
		}

		R_SubdivideWaterPolygon( batch, texinfo, front, recursionDepth + 1 );
		R_SubdivideWaterPolygon( batch, texinfo, back, recursionDepth + 1 );
		return;
	}

	R_AddWaterPolygon( batch, texinfo, polygon );
}

static void R_WaterSurfaceNormal( const msurface_t *surface,
	const std::vector<std::array<float, 3>> &polygon, float normal[3] )
{
	normal[0] = normal[1] = 0.0f;
	normal[2] = 1.0f;

	if( surface && surface->plane )
	{
		normal[0] = surface->plane->normal[0];
		normal[1] = surface->plane->normal[1];
		normal[2] = surface->plane->normal[2];
		if( surface->flags & SURF_PLANEBACK )
		{
			normal[0] = -normal[0];
			normal[1] = -normal[1];
			normal[2] = -normal[2];
		}
		return;
	}

	if( polygon.size( ) >= 3 )
	{
		float a[3] = {
			polygon[1][0] - polygon[0][0],
			polygon[1][1] - polygon[0][1],
			polygon[1][2] - polygon[0][2]
		};
		float b[3] = {
			polygon[2][0] - polygon[0][0],
			polygon[2][1] - polygon[0][1],
			polygon[2][2] - polygon[0][2]
		};
		normal[0] = a[1] * b[2] - a[2] * b[1];
		normal[1] = a[2] * b[0] - a[0] * b[2];
		normal[2] = a[0] * b[1] - a[1] * b[0];
		const float length = sqrtf( normal[0] * normal[0] +
			normal[1] * normal[1] + normal[2] * normal[2] );
		if( length > 0.0001f )
		{
			normal[0] /= length;
			normal[1] /= length;
			normal[2] /= length;
		}
	}
}

void R_AddWaterSurfaceToBatch( const model_t *mod, msurface_t *surface,
	world_model_draw_t &out, int surfaceIndex,
	const surface_pvs_leafs_t &surfaceLeafs )
{
	if( !mod || !surface || !surface->texinfo || !surface->texinfo->texture ||
		surface->numedges < 3 || surface->numedges > 64 )
	{
		return;
	}

	std::vector<std::array<float, 3>> polygon;
	polygon.reserve( surface->numedges );
	for( int i = 0; i < surface->numedges; ++i )
	{
		const mvertex_t *vertex = R_GetBrushSurfaceVertex( mod, surface, i );
		if( !vertex )
			return;
		polygon.push_back( { vertex->position[0], vertex->position[1], vertex->position[2] } );
	}

	float normal[3];
	R_WaterSurfaceNormal( surface, polygon, normal );
	const bool topFace = normal[2] > 0.5f;

	if( !topFace )
		return;

	world_water_batch_t &batch = R_FindWorldWaterBatch( out, surface->texinfo->texture,
		0, 0, 0 );
	R_AddSurfacePVSLeafs( batch.pvsLeafs, batch.pvsAlwaysVisible, surfaceLeafs, surfaceIndex );
	R_ExpandWaterBatchBounds( batch, polygon );
	R_SubdivideWaterPolygon( batch, surface->texinfo, polygon );
}

bool R_UploadWaterBatch( world_water_batch_t &batch )
{
	batch.vertexCount = static_cast<UINT>( batch.sources.size( ) );
	batch.primitiveCount = static_cast<UINT>( batch.indices.size( ) / 3 );
	if( batch.vertexCount == 0 || batch.primitiveCount == 0 )
		return false;

	batch.dynamicVertices.resize( batch.sources.size( ) );

	if( !batch.indexBuffer )
	{
		if( batch.indices.size( ) > static_cast<size_t>( DX_SCRATCH_MAX_BYTES / sizeof( DWORD ) ) )
			return false;
		const UINT indexBytes = static_cast<UINT>( batch.indices.size( ) * sizeof( DWORD ) );
		HRESULT result = device->CreateIndexBuffer(
			indexBytes, D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_MANAGED,
			&batch.indexBuffer, nullptr );
		if( FAILED( result ) || !batch.indexBuffer )
			return false;

		void *indexMemory = nullptr;
		result = batch.indexBuffer->Lock( 0, 0, &indexMemory, 0 );
		if( FAILED( result ) || !indexMemory )
		{
			batch.indexBuffer->Release( );
			batch.indexBuffer = nullptr;
			return false;
		}
		memcpy( indexMemory, batch.indices.data( ), batch.indices.size( ) * sizeof( DWORD ) );
		batch.indexBuffer->Unlock( );
	}

	if( !batch.vertexBuffer )
	{
		HRESULT result = device->CreateVertexBuffer(
			batch.vertexCount * sizeof( brush_vertex_t ),
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, BRUSH_FVF, D3DPOOL_DEFAULT,
			&batch.vertexBuffer, nullptr );
		if( FAILED( result ) || !batch.vertexBuffer )
			return false;
	}

	return true;
}

static bool R_UpdateWaterBatchVertices( world_water_batch_t &batch,
	texture_t *texture, const cl_entity_s *ent, float time, bool rippleTextureMode )
{
	if( !texture || batch.sources.empty( ) )
		return false;

	if( !R_UploadWaterBatch( batch ) )
		return false;

	constexpr float waterTexturePeriod = 64.0f;
	const float tw = waterTexturePeriod;
	const float th = waterTexturePeriod;
	const float waveScale = dx_remix.value != 0.0f ? 0.0f : std::max( 0.0f, std::min( 4.0f, dx_water_wave_size.value ) );
	const float speed = dx_remix.value != 0.0f ? 0.0f : dx_water_wave_speed.value;
	const bool combineRippleAndWaves = rippleTextureMode;
	const bool uvWaves = waveScale > 0.0f && ( !rippleTextureMode || combineRippleAndWaves );
	const float entityScale = dx_remix.value == 0.0f && ent ? ent->curstate.scale : 0.0f;
	const bool vertexWaves = fabsf( entityScale ) > 0.0001f;
	const float animTime = ( uvWaves || vertexWaves ) ? R_WaterQuantizedTime( time ) : 0.0f;
	const int renderMode = ent ? ent->curstate.rendermode : kRenderNormal;
	const int renderAmt = ent ? ent->curstate.renderamt : 255;
	const int renderColorR = ent ? ent->curstate.rendercolor.r : 255;
	const int renderColorG = ent ? ent->curstate.rendercolor.g : 255;
	const int renderColorB = ent ? ent->curstate.rendercolor.b : 255;

	if( batch.vertexCacheValid &&
		fabsf( batch.cachedAnimTime - animTime ) < 0.00001f &&
		fabsf( batch.cachedWaveScale - waveScale ) < 0.00001f &&
		fabsf( batch.cachedWaveSpeed - speed ) < 0.00001f &&
		fabsf( batch.cachedEntityScale - entityScale ) < 0.00001f &&
		batch.cachedRippleTextureMode == rippleTextureMode &&
		batch.cachedUvWaves == uvWaves &&
		batch.cachedRenderMode == renderMode &&
		batch.cachedRenderAmt == renderAmt &&
		batch.cachedRenderColorR == renderColorR &&
		batch.cachedRenderColorG == renderColorG &&
		batch.cachedRenderColorB == renderColorB )
	{
		return true;
	}

	void *vertexMemory = nullptr;
	HRESULT result = batch.vertexBuffer->Lock( 0, 0, &vertexMemory, D3DLOCK_DISCARD );
	if( FAILED( result ) || !vertexMemory )
	{
		R_FreeWaterBatches( batch, false );
		return false;
	}

	auto *destination = static_cast<brush_vertex_t *>( vertexMemory );
	for( size_t i = 0; i < batch.sources.size( ); ++i )
	{
		const water_source_vertex_t &source = batch.sources[i];
		float warpedS = source.s;
		float warpedT = source.t;
		if( uvWaves )
		{

			warpedS += R_WaterTurb( source.t * 0.125f + animTime * speed ) * waveScale;
			warpedT += R_WaterTurb( source.s * 0.125f + animTime * speed ) * waveScale;
		}

		byte shade = 255;

		brush_vertex_t &vertex = destination[i];
		vertex.x = source.x;
		vertex.y = source.y;
		vertex.z = R_WaterGLVerticalWaveZ( source, ent, animTime );
		vertex.diffuse = R_WaterVertexColor( ent, shade, rippleTextureMode );
		vertex.u = warpedS / tw;
		vertex.v = warpedT / th;
	}

	batch.vertexBuffer->Unlock( );
	batch.vertexCacheValid = true;
	batch.cachedAnimTime = animTime;
	batch.cachedWaveScale = waveScale;
	batch.cachedWaveSpeed = speed;
	batch.cachedEntityScale = entityScale;
	batch.cachedRippleTextureMode = rippleTextureMode;
	batch.cachedUvWaves = uvWaves;
	batch.cachedRenderMode = renderMode;
	batch.cachedRenderAmt = renderAmt;
	batch.cachedRenderColorR = renderColorR;
	batch.cachedRenderColorG = renderColorG;
	batch.cachedRenderColorB = renderColorB;
	return true;
}
void R_LoadMap( const model_t *mod )
{
	R_RegisterModelCvars( );

	globalModels.max_load_factor( 0.70f );
	if( globalModels.size( ) < 512 )
		globalModels.reserve( 512 );

	g_worldmodel = const_cast<model_t *>( mod );
	g_studioRenderModeFrontCache.fill( studio_mode_slot_t{} );
	R_ClearAOEdgeCache( );
	R_InvalidateCurrentWorldPVS( );
	R_InvalidateWorldVis( );

	g_dxDecalMapReady = false;
	R_ClearDecals( );

	R_FreeRippleCaches( true );
	R_ReleaseGlowOcclusionQueries( );
	R_ClearStudioGpuCaches( );
	R_ClearEntityCache( );
	for( auto &cache : g_worldLightmapStorage )
		R_FreeWorldModel( cache );
	g_worldLightmappedModels.clear( );
	g_worldLightmapStorage.clear( );
	g_worldModelFrontCache.fill( world_model_slot_t{} );
	g_worldLightmappedModels.max_load_factor( 0.70f );
	g_worldLightmappedModels.reserve( 64 );

	R_BuildMapLightmaps( mod );

	if( mod )
		R_GetLightmappedBrushModel( mod );
}

std::array<player_gait_state_t, MAX_CLIENTS> g_playerGaitStates{};
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

std::vector<muzzleflash_t> g_rendererMuzzleFlashes;

#ifndef MAX_DLIGHTS
#define MAX_DLIGHTS 32
#endif
#ifndef MAX_ELIGHTS
#define MAX_ELIGHTS 128
#endif
std::vector<dx_dlight_t> g_dxDLights;

std::array<uint16_t, 128> g_dxStudioGlobalLightIndices{};
std::array<uint16_t, 128> g_dxStudioLocalLightIndices{};
int g_dxStudioGlobalLightCount = 0;
int g_dxStudioLocalLightCount = 0;

std::vector<dx_line_light_t> g_dxLineLights;
int g_dxFixedLightCount = 0;

std::vector<beam_light_segment_t> g_dxBeamLights;
std::vector<size_t> g_dlightSelected;
std::vector<size_t> g_dlightRestore;
std::vector<unsigned char> g_dynamicOverlaySelectedMask;
std::vector<lightmap_dirty_atlas_t> g_dynamicOverlayDirtyScratch;
#ifndef MAXSTUDIOBONES
#define MAXSTUDIOBONES 128
#endif

#ifndef STUDIO_FAST_LIT_FVF
#define STUDIO_FAST_LIT_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif
#ifndef STUDIO_NORMAL_LIT_FVF
#define STUDIO_NORMAL_LIT_FVF ( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1 )
#endif

std::deque<studio_cache_t> g_studioEntityGpuCacheStorage;

studio_light_probe_cache_t g_viewModelLightProbeCache;

uint32_t g_studioRenderFxRandomState = 0xA341316Cu;

#define DYNAMIC_LIGHT_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 )

void R_SaveEntityRenderState( dx_entity_state_t *state )
{
	device->GetRenderState( D3DRS_ALPHABLENDENABLE, &state->alphaBlendEnable );
	device->GetRenderState( D3DRS_SRCBLEND, &state->srcBlend );
	device->GetRenderState( D3DRS_DESTBLEND, &state->destBlend );
	device->GetRenderState( D3DRS_BLENDOP, &state->blendOp );
	device->GetRenderState( D3DRS_ZWRITEENABLE, &state->zWriteEnable );
	device->GetRenderState( D3DRS_ZENABLE, &state->zEnable );
	device->GetRenderState( D3DRS_ALPHATESTENABLE, &state->alphaTestEnable );
	device->GetRenderState( D3DRS_ALPHAFUNC, &state->alphaFunc );
	device->GetRenderState( D3DRS_ALPHAREF, &state->alphaRef );
	device->GetRenderState( D3DRS_TEXTUREFACTOR, &state->textureFactor );
	device->GetTextureStageState( 0, D3DTSS_COLOROP, &state->colorOp );
	device->GetTextureStageState( 0, D3DTSS_COLORARG1, &state->colorArg1 );
	device->GetTextureStageState( 0, D3DTSS_COLORARG2, &state->colorArg2 );
	device->GetTextureStageState( 0, D3DTSS_ALPHAOP, &state->alphaOp );
	device->GetTextureStageState( 0, D3DTSS_ALPHAARG1, &state->alphaArg1 );
	device->GetTextureStageState( 0, D3DTSS_ALPHAARG2, &state->alphaArg2 );
	device->GetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, &state->texCoordIndex );
	device->GetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, &state->textureTransformFlags );
}

static void R_RestoreEntityRenderState( const dx_entity_state_t *state )
{
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, state->alphaBlendEnable );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, state->srcBlend );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, state->destBlend );
	DX_SetRenderStateCached( D3DRS_BLENDOP, state->blendOp );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, state->zWriteEnable );
	DX_SetRenderStateCached( D3DRS_ZENABLE, state->zEnable );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, state->alphaTestEnable );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, state->alphaFunc );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, state->alphaRef );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, state->textureFactor );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, state->colorOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, state->colorArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, state->colorArg2 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, state->alphaOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, state->alphaArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, state->alphaArg2 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, state->texCoordIndex );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, state->textureTransformFlags );
}

static int R_EntityRenderFxAlpha( const cl_entity_s *ent )
{
	if( !ent )
		return 255;

	if( ent->curstate.renderfx != kRenderFxHologram &&
		ent->curstate.renderfx != kRenderFxDistort )
	{
		return std::max( 0, std::min( 255,
			static_cast<int>( ent->curstate.renderamt ) ) );
	}

	vec3_t toEntity;
	VectorSubtract( ent->origin, g_vOrigin, toEntity );
	float distance = DotProduct( toEntity, g_vForward );

	if( ent->curstate.renderfx == kRenderFxDistort )
		distance = 1.0f;

	int blend = 0;
	if( distance > 0.0f )
	{
		constexpr int baseAlpha = 180;
		if( distance <= 100.0f )
		{
			blend = baseAlpha;
		}
		else
		{
			blend = static_cast<int>(
				( 1.0f - ( distance - 100.0f ) * ( 1.0f / 400.0f ) ) *
				static_cast<float>( baseAlpha ) );
		}

		blend += R_StudioRenderFxRandomLong( -32, 31 );
	}

	return std::max( 0, std::min( 255, blend ) );
}

static D3DCOLOR R_EntityRenderColor( const cl_entity_s *ent, int renderMode )
{
	int r = 255, g = 255, b = 255;
	int a = 255;

	if( ent )
	{
		r = ent->curstate.rendercolor.r;
		g = ent->curstate.rendercolor.g;
		b = ent->curstate.rendercolor.b;

		if( r == 0 && g == 0 && b == 0 )
			r = g = b = 255;

		if( renderMode != kRenderNormal )
			a = R_EntityRenderFxAlpha( ent );
	}

	return D3DCOLOR_RGBA( r, g, b, a );
}

void R_SetAlphaTestEnabled( bool enabled )
{
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, enabled ? TRUE : FALSE );
	if( enabled )
	{
		DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
		// A 50% cutout removes bilinear alpha fringes under Remix.
		DX_SetRenderStateCached( D3DRS_ALPHAREF,
			dx_remix.value != 0.0f ? 127 : 0 );
	}
}

static void R_ApplyStudioBlend( const cl_entity_s *ent )
{
	const int renderMode = ent ? ent->curstate.rendermode : kRenderNormal;
	DX_AllowFog( ( renderMode == kRenderGlow || renderMode == kRenderTransAdd )
		? false : true );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );

	switch( renderMode )
	{
	default:
	case kRenderNormal:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
	case kRenderTransAlpha:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	case kRenderGlow:
	case kRenderTransAdd:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		break;
	}
}

static studio_material_state_t R_StudioMaterialStateForFlags(
	unsigned int flags )
{

	if( flags & STUDIO_NF_MASKED )
		return DX_STUDIO_MATERIAL_MASKED;
	if( flags & STUDIO_NF_ADDITIVE )
		return DX_STUDIO_MATERIAL_ADDITIVE;
	return DX_STUDIO_MATERIAL_BASE;
}

void R_ApplyStudioMaterialState( unsigned int flags,
	const cl_entity_s *ent, studio_material_state_t &currentState )
{
	const studio_material_state_t targetState =
		R_StudioMaterialStateForFlags( flags );

	if( targetState == currentState )
		return;

	if( currentState != DX_STUDIO_MATERIAL_BASE )
	{
		R_ApplyStudioBlend( ent );

		if( currentState == DX_STUDIO_MATERIAL_MASKED )
		{
			DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
			DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );
		}

		currentState = DX_STUDIO_MATERIAL_BASE;
	}

	if( targetState == DX_STUDIO_MATERIAL_BASE )
		return;

	if( targetState == DX_STUDIO_MATERIAL_MASKED )
	{
		DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
		DX_SetRenderStateCached( D3DRS_ALPHAREF, 128 );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
		currentState = DX_STUDIO_MATERIAL_MASKED;
		return;
	}

	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
	if( !ent || ent->curstate.rendermode == kRenderNormal )
	{
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_AllowFog( false );
	}
	else
	{
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
	}
	currentState = DX_STUDIO_MATERIAL_ADDITIVE;
}

bool R_BrushTextureNeedsAlphaTest( const texture_t *texture )
{
	if( !texture )
		return false;

	const char *name = texture->name;
	if( name[0] == '+' && name[1] )
		name += 2;
	return name[0] == '{';
}

void R_SetTextureStage( DWORD colorOp, DWORD colorArg1, DWORD colorArg2,
	DWORD alphaOp, DWORD alphaArg1, DWORD alphaArg2 )
{
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, colorOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, colorArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, colorArg2 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, alphaOp );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, alphaArg1 );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, alphaArg2 );
}

void R_SetOpaqueEntityBaseline( )
{
	DX_AllowFog( true );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
		D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE );
}

void R_RestoreOpaqueEntityBaseline( )
{
	DX_AllowFog( true );

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
}

void R_RestoreEntityRenderMode( const dx_entity_state_t *state )
{
	DX_AllowFog( true );
	if( dx_remix_perf.value == 0.0f && state )
	{
		R_RestoreEntityRenderState( state );
		return;
	}

	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
	R_SetOpaqueEntityBaseline( );
}

void R_BeginEntityRenderMode( const cl_entity_s *ent,
	dx_entity_state_t *savedState )
{
	const int renderMode = ent ? ent->curstate.rendermode : kRenderNormal;
	DX_AllowFog( ( renderMode == kRenderGlow || renderMode == kRenderTransAdd ) ?
		false : true );

	if( dx_remix_perf.value == 0.0f )
		R_SaveEntityRenderState( savedState );

	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );

	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
	DX_SetRenderStateCached( D3DRS_ALPHAREF, 0 );
	DX_SetRenderStateCached( D3DRS_TEXTUREFACTOR,
		R_EntityRenderColor( ent, renderMode ) );

	switch( renderMode )
	{
	default:
	case kRenderNormal:
		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE );
		break;

	case kRenderTransColor:

		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_SELECTARG2, D3DTA_TEXTURE, D3DTA_TFACTOR,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;

	case kRenderTransTexture:

		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;

	case kRenderGlow:
	{

		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;
	}

	case kRenderTransAlpha:

		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;

	case kRenderTransAdd:

		DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, TRUE );
		DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
		DX_SetRenderStateCached( D3DRS_DESTBLEND, D3DBLEND_ONE );
		DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, FALSE );
		R_SetTextureStage( D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
			D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_TFACTOR );
		break;
	}
}

static void R_DrawWaterBrushModel( world_model_draw_t &model,
	const cl_entity_s *ent, float time, bool depthOnly )
{
	DX_MODEL_PROFILE_SCOPE( waterProfileScope, DX_PROFILE_PHASE_WATERMODEL );
	if( model.waterBatches.empty( ) )
		return;

	const bool rippleMode = R_WaterRippleTextureEnabled( );
	if( R_WaterEntityAlpha( ent, rippleMode ) <= 0.0f )
		return;

	device->SetFVF( BRUSH_FVF );
	R_SetLightmapStageEnabled( false );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXCOORDINDEX, 0 );
	DX_SetTextureStageStateCached( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	DX_SetTextureStageStateCached( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	const int renderMode = ent ? ent->curstate.rendermode : kRenderNormal;
	const bool additiveWater = renderMode == kRenderTransAdd || renderMode == kRenderGlow;
	if( !depthOnly )
		DX_AllowFog( additiveWater ? false : true );
	const bool translucentWater = renderMode != kRenderNormal || R_WaterEntityAlpha( ent, rippleMode ) < 1.0f;
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, depthOnly ? TRUE : ( translucentWater ? FALSE : TRUE ) );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, ( !depthOnly && translucentWater ) ? TRUE : FALSE );
	DX_SetRenderStateCached( D3DRS_COLORWRITEENABLE, depthOnly ? 0 : 0x0000000F );
	DX_SetRenderStateCached( D3DRS_BLENDOP, D3DBLENDOP_ADD );
	DX_SetRenderStateCached( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	DX_SetRenderStateCached( D3DRS_DESTBLEND, additiveWater ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
	DX_SetSamplerStateCached( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
	DX_SetSamplerStateCached( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	DX_SetSamplerStateCached( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

	int boundBaseTexture = -1;
	IDirect3DTexture9 *boundRippleTexture = nullptr;
	for( auto &batch : model.waterBatches )
	{
		// Pin the primary frame even if an entity requests alternate/frame animation.
		texture_t *texture = dx_remix.value != 0.0f
			? R_SelectAnimatedTexture( batch.baseTexture, nullptr, 0.0f )
			: R_SelectAnimatedTexture( batch.baseTexture, ent, time );
		if( !texture || texture->gl_texturenum <= 0 || batch.primitiveCount == 0 )
			continue;

		if( !ent && !R_PVSLeafsVisible( model, batch.pvsLeafs, batch.pvsAlwaysVisible ) )
			continue;

		if( !ent && !R_BoundsVisibleInCurrentView( batch.mins, batch.maxs, batch.boundsValid ) )
			continue;

		IDirect3DTexture9 *rippleTexture = depthOnly || dx_remix.value != 0.0f ? nullptr : R_UpdateWaterRippleTexture( texture, time );
		const bool usingRippleTexture = dx_remix.value == 0.0f && R_WaterRippleTextureEnabled( );

		if( !R_UpdateWaterBatchVertices( batch, texture, ent, time, rippleMode ) )
		{
			if( batch.dynamicVertices.size( ) != batch.sources.size( ) )
				batch.dynamicVertices.resize( batch.sources.size( ) );
			constexpr float waterTexturePeriod = 64.0f;
			const float tw = waterTexturePeriod;
			const float th = waterTexturePeriod;
			for( size_t i = 0; i < batch.sources.size( ); ++i )
			{
				const water_source_vertex_t &source = batch.sources[i];
				brush_vertex_t &vertex = batch.dynamicVertices[i];
				vertex.x = source.x;
				vertex.y = source.y;
				vertex.z = R_WaterGLVerticalWaveZ( source, ent, time );
				vertex.diffuse = R_WaterVertexColor( ent, 255, rippleMode );
				vertex.u = source.s / tw;
				vertex.v = source.t / th;
			}
		}

		if( !depthOnly && usingRippleTexture && rippleTexture )
		{
			if( rippleTexture != boundRippleTexture )
			{
				DX_BindRawTexture( 0, rippleTexture );
				boundRippleTexture = rippleTexture;
				boundBaseTexture = -1;
			}
		}
		else if( !depthOnly && texture->gl_texturenum != boundBaseTexture )
		{
			DX_Bind( 0, texture->gl_texturenum );
			boundBaseTexture = texture->gl_texturenum;
			boundRippleTexture = nullptr;
		}

		DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_NONE );
		R_DrawWaterIndexed( batch );
	}

	DX_SetRenderStateCached( D3DRS_COLORWRITEENABLE, 0x0000000F );
	DX_SetRenderStateCached( D3DRS_CULLMODE, D3DCULL_CCW );
	DX_SetRenderStateCached( D3DRS_ZENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZWRITEENABLE, TRUE );
	DX_SetRenderStateCached( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
	DX_SetRenderStateCached( D3DRS_ALPHABLENDENABLE, FALSE );
	DX_SetRenderStateCached( D3DRS_ALPHATESTENABLE, FALSE );
	if( !depthOnly )
		DX_AllowFog( true );
}

float g_glowQueryBudgetTime = -99999.0f;
int g_glowQueriesIssuedThisFrame = 0;

sprite_batch_key_t g_spriteBatchKey;
bool g_spriteBatchActive = false;
std::vector<brush_vertex_t> g_spriteBatchVertices;

cl_entity_s g_playerWeaponDrawEntities[MAX_CLIENTS + 1];

static void R_DrawBrushWaterPass( cl_entity_s *ent, model_t *mod,
	bool depthOnly )
{
	if( !device || !mod || mod->type != mod_brush )
		return;

	if( ent )
	{
		D3DMATRIX entityWorld;
		R_SetEntityWorldTransform( ent, gp_cl->time, &entityWorld );
		device->SetTransform( D3DTS_WORLD, &entityWorld );
	}

	world_model_draw_t *model = R_GetLightmappedBrushModel( mod );
	if( model )
		R_DrawWaterBrushModel( *model, ent, gp_cl->time, depthOnly );

}

bool R_WaterIsTranslucent( const cl_entity_s *ent )
{
	const bool rippleMode = R_WaterRippleTextureEnabled( );
	if( ent && ent->curstate.rendermode != kRenderNormal )
		return true;

	return R_WaterEntityAlpha( ent, rippleMode ) < 0.999f;
}

byte *R_GetWorldVis( void )
{
	if( !g_worldmodel || g_worldmodel->type != mod_brush ||
		g_worldmodel->numleafs <= 0 )
	{
		return nullptr;
	}

	const int rowBytes = ( g_worldmodel->numleafs + 7 ) >> 3;

	auto AllVisible = [rowBytes]( ) -> byte *
		{
			if( static_cast<int>( g_allVisibleWorldPVS.size( ) ) != rowBytes )
				g_allVisibleWorldPVS.assign(
					static_cast<size_t>( rowBytes ), static_cast<byte>( 0xFF ) );
			return g_allVisibleWorldPVS.empty( )
				? nullptr
				: g_allVisibleWorldPVS.data( );
		};

	if( !R_WorldPVSEnabledForModel( g_worldmodel ) )
		return AllVisible( );

	world_model_draw_t *model = R_GetLightmappedBrushModel( g_worldmodel );
	if( !model ||
		!R_UpdateCurrentWorldPVS( *model ) ||
		g_currentWorldPVSAllVisible ||
		g_currentWorldPVS.empty( ) )
	{
		return AllVisible( );
	}

	return g_currentWorldPVS.data( );
}

bool R_DrawWorldBrushes( model_t *mod )
{
	if( !device || !mod || mod->type != mod_brush )
		return false;

	world_model_draw_t *model = R_GetLightmappedBrushModel( mod );
	if( !model )
		return false;

	R_DrawLightmappedBrushModel( *model, nullptr, gp_cl->time );
	R_DrawEntityDecals( nullptr );
	return !model->waterBatches.empty( );
}

bool R_DrawBrushEntity( cl_entity_s *ent )
{
	if( !device || !ent || !ent->model || ent->model->type != mod_brush )
		return false;

	D3DMATRIX entityWorld;
	R_SetEntityWorldTransform( ent, gp_cl->time, &entityWorld );
	device->SetTransform( D3DTS_WORLD, &entityWorld );

	world_model_draw_t *model = R_GetLightmappedBrushModel( ent->model );
	if( !model )
		return false;

	R_DrawLightmappedBrushModel( *model, ent, gp_cl->time );
	R_DrawEntityDecals( ent );
	return !model->waterBatches.empty( );
}

void R_DrawWorldWaterDepth( model_t *mod )
{
	if( dx_water_depth_prepass.value == 0.0f )
		return;
	if( !R_WaterRippleTextureEnabled( ) )
		return;
	R_DrawBrushWaterPass( nullptr, mod, true );
}

void R_DrawBrushWaterDepth( cl_entity_s *ent )
{
	if( !ent || !ent->model || dx_water_depth_prepass.value == 0.0f )
		return;

	if( !R_WaterRippleTextureEnabled( ) &&
		ent->curstate.rendermode == kRenderNormal )
	{
		return;
	}

	R_DrawBrushWaterPass( ent, ent->model, true );
}

void R_DrawWorldWater( model_t *mod )
{
	R_DrawBrushWaterPass( nullptr, mod, false );
}

void R_DrawBrushWater( cl_entity_s *ent )
{
	if( !ent || !ent->model )
		return;
	R_DrawBrushWaterPass( ent, ent->model, false );
}
