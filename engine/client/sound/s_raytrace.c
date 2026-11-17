/*
s_raytrace.c - raytraced sound propagation (occlusion, reflections, reverb)
Copyright (C) 2026 Cry of Fear REMIXED contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Replaces the room_type DSP with a per-sound acoustic model in the spirit of
Sound Physics Remastered:

 * direct path: rays from the source to the listener; every wall crossed adds
   absorption (by material and thickness) which lowpasses and quiets the dry
   signal.
 * environment: rays shot from the source on a fibonacci sphere bounce off
   world and brush entity surfaces. Path length and surface reflectivity feed
   four reverb sends (short to very long decay); bounce points that can see the
   listener count as "shared airspace", which lets reverb through unfiltered.
 * mixing: every channel gets its own dry lowpass and four filtered sends into
   four stereo feedback delay network reverbs.

Tracing happens on the main thread in SND_RT_Frame, bounded by a per frame
time budget; results are smoothed and cached.
*/

#include "common.h"
#include "client.h"
#include "sound.h"
#include "pm_local.h"

#define SRT_SLOTS          4
#define SRT_MAX_RAYS       512
#define SRT_MAX_BOUNCES    16
#define SRT_MAX_SPANS      24
#define SRT_CACHE_SIZE     48
#define SRT_TEXCACHE_SIZE  2048 // power of two
#define SRT_FDN_LINES      8
#define SRT_ALLPASSES      4
#define SRT_HF_REFERENCE   5000.0f // EFX lowpass reference frequency
#define SRT_SPEED_OF_SOUND 343.0f

// cvars
static CVAR_DEFINE_AUTO( snd_rt, "1", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: 0 = classic room_type DSP, 1 = raytraced occlusion and reverb" );
static CVAR_DEFINE_AUTO( snd_rt_rays, "32", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: environment rays per sound (4-512)" );
static CVAR_DEFINE_AUTO( snd_rt_bounces, "4", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: reflections followed per ray (1-16)" );
static CVAR_DEFINE_AUTO( snd_rt_maxdist, "4096", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: max length of one ray segment, in units" );
static CVAR_DEFINE_AUTO( snd_rt_units_per_meter, "40", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: world units per meter (sets room size for the reverb time)" );
static CVAR_DEFINE_AUTO( snd_rt_reverb_gain, "1.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: overall reverb volume" );
static CVAR_DEFINE_AUTO( snd_rt_reverb_brightness, "1.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: reverb high frequency decay (lower is darker)" );
static CVAR_DEFINE_AUTO( snd_rt_reflectivity, "1.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: scales how reverberant surfaces are (>1 divides absorption, longer reverb)" );
static CVAR_DEFINE_AUTO( snd_rt_decay_scale, "1.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: multiplier on the estimated reverb time" );
static CVAR_DEFINE_AUTO( snd_rt_occlusion, "1.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: how strongly walls absorb sound (0 disables occlusion)" );
static CVAR_DEFINE_AUTO( snd_rt_occlusion_unit, "16", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: wall thickness in units that absorbs like one full block" );
static CVAR_DEFINE_AUTO( snd_rt_occlusion_variation, "10", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: radius of extra occlusion rays around the source, lets sound bend round edges (0 = one ray)" );
static CVAR_DEFINE_AUTO( snd_rt_max_occlusion, "10", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: cap on accumulated occlusion" );
static CVAR_DEFINE_AUTO( snd_rt_airspace, "1", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: simulate shared airspace (reflections reaching the listener bypass occlusion)" );
static CVAR_DEFINE_AUTO( snd_rt_brush_entities, "1", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: doors and other solid brush entities block and reflect sound" );
static CVAR_DEFINE_AUTO( snd_rt_refresh, "0.1", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: seconds between re-evaluations of a playing sound while it or the listener moves" );
static CVAR_DEFINE_AUTO( snd_rt_refresh_static, "0.5", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: seconds between re-evaluations when nothing moved (doors may have)" );
static CVAR_DEFINE_AUTO( snd_rt_budget, "2.0", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: milliseconds of tracing allowed per frame" );
static CVAR_DEFINE_AUTO( snd_rt_smoothing, "0.08", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: seconds to glide between acoustic parameter updates" );
static CVAR_DEFINE_AUTO( snd_rt_underwater, "0.08", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: high frequency gain while the listener is underwater (1 = no filter)" );
static CVAR_DEFINE_AUTO( snd_rt_source_lift, "1", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: trace model sounds from mid-body instead of the feet" );
static CVAR_DEFINE_AUTO( snd_rt_debug, "0", FCVAR_ARCHIVE, "raytraced audio: 1 = show per-sound acoustic parameters, 2 = also print evaluation timings" );

// per slot reverb parameters (decay time, HF decay ratio, gain, reflections gain,
// reflections delay, late delay, line length scale)
typedef struct srt_slot_def_s
{
	float decay;
	float hf_ratio;
	float gain;
	float refl_gain;
	float refl_delay;
	float late_delay;
	float size;
} srt_slot_def_t;

static const srt_slot_def_t srt_slot_defs[SRT_SLOTS] =
{
	{ 0.15f,  0.6f, 0.17f, 2.5f, 0.001f, 0.011f, 0.35f },
	{ 0.55f,  0.7f, 0.26f, 0.2f, 0.015f, 0.011f, 0.6f  },
	{ 1.68f,  0.7f, 0.43f, 0.0f, 0.021f, 0.021f, 1.0f  },
	{ 4.142f, 0.5f, 0.34f, 0.0f, 0.025f, 0.021f, 1.4f  },
};

static convar_t *srt_slot_cvars[SRT_SLOTS];
static CVAR_DEFINE( snd_rt_slot0, "snd_rt_slot0_decay", "0.15", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: decay time of the small-space reverb, seconds" );
static CVAR_DEFINE( snd_rt_slot1, "snd_rt_slot1_decay", "0.55", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: decay time of the room reverb, seconds" );
static CVAR_DEFINE( snd_rt_slot2, "snd_rt_slot2_decay", "1.68", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: decay time of the hall reverb, seconds" );
static CVAR_DEFINE( snd_rt_slot3, "snd_rt_slot3_decay", "4.142", FCVAR_ARCHIVE|FCVAR_FILTERABLE, "raytraced audio: decay time of the cavern reverb, seconds" );

//
// materials
//
typedef struct srt_material_s
{
	const char *name;
	float      reflectivity; // fraction of sound energy reflected per bounce (0-1)
	float      absorption;   // occlusion per full wall
} srt_material_t;

// indexed by CoF material letter - 'A'; slot 26 is unlisted textures, 27 fences ('{')
#define SRT_MAT_DEFAULT 26
#define SRT_MAT_FENCE   27
#define SRT_MAT_COUNT   28

static const srt_material_t srt_material_defaults[SRT_MAT_COUNT] =
{
	{ "asphalt",   0.90f, 1.0f  }, // A
	{ "twigs",     0.50f, 0.5f  }, // B
	{ "concrete",  0.93f, 1.0f  }, // C
	{ "dirt",      0.60f, 1.0f  }, // D
	{ "E",         0.80f, 1.0f  }, // E
	{ "flesh",     0.40f, 0.6f  }, // F
	{ "grate",     0.40f, 0.15f }, // G
	{ "paper",     0.50f, 0.4f  }, // H
	{ "mud",       0.55f, 1.0f  }, // I
	{ "J",         0.80f, 1.0f  }, // J
	{ "K",         0.80f, 1.0f  }, // K
	{ "L",         0.80f, 1.0f  }, // L
	{ "metal",     0.92f, 1.0f  }, // M
	{ "snow",      0.30f, 0.8f  }, // N
	{ "sand",      0.55f, 1.0f  }, // O
	{ "computer",  0.80f, 0.7f  }, // P
	{ "Q",         0.80f, 1.0f  }, // Q
	{ "gravel",    0.60f, 1.0f  }, // R
	{ "slosh",     0.85f, 0.5f  }, // S
	{ "tile",      0.94f, 1.0f  }, // T
	{ "grass",     0.50f, 0.8f  }, // U
	{ "vent",      0.85f, 0.4f  }, // V
	{ "wood",      0.80f, 0.6f  }, // W
	{ "X",         0.80f, 1.0f  }, // X
	{ "glass",     0.90f, 0.35f }, // Y
	{ "carpet",    0.45f, 0.7f  }, // Z
	{ "default",   0.90f, 1.0f  }, // unlisted textures: mostly brick, plaster and concrete
	{ "fence",     0.30f, 0.1f  }, // '{' alpha-tested textures
};

static srt_material_t srt_materials[SRT_MAT_COUNT];

typedef struct srt_texmat_s
{
	char name[16];
	byte mat;
} srt_texmat_t;

typedef struct srt_params_s
{
	float direct_gain;
	float direct_cutoff;          // HF gain at 5 kHz
	float send_gain[SRT_SLOTS];
	float send_cutoff[SRT_SLOTS];
	float occlusion;              // diagnostics
	float airspace;
	float rt60;
} srt_params_t;

typedef struct srt_chan_s
{
	qboolean     valid;     // has parameters, process wet
	qboolean     pending;   // needs its first evaluation
	qboolean     fallback;  // parameters are the cheap approximation
	double       last_eval;
	vec3_t       eval_source;
	vec3_t       eval_listener;
	srt_params_t target;
	srt_params_t cur;

	// filter state
	float        dry_lp[2][2];          // [stage][left/right]
	float        send_lp[SRT_SLOTS];
	float        dry_gain_prev;
	float        send_gain_prev[SRT_SLOTS];
} srt_chan_t;

typedef struct srt_cache_s
{
	double       time;
	vec3_t       source;
	vec3_t       listener;
	srt_params_t params;
} srt_cache_t;

typedef struct srt_delay_s
{
	float *buf;
	int   size;
	int   pos;
} srt_delay_t;

typedef struct srt_reverb_s
{
	srt_delay_t predelay;
	srt_delay_t allpass[SRT_ALLPASSES];
	float       allpass_gain;
	srt_delay_t line[SRT_FDN_LINES];
	int         line_len[SRT_FDN_LINES];
	float       line_gain[SRT_FDN_LINES];
	float       line_damp[SRT_FDN_LINES];
	float       line_lp[SRT_FDN_LINES];
	int         refl_taps[4];
	int         late_tap;
	float       refl_gain;
	float       out_gain;
	float       cfg_decay, cfg_bright; // what the coefficients were built for
} srt_reverb_t;

static struct
{
	qboolean     initialized;
	int          rate;

	srt_chan_t   chans[MAX_CHANNELS];
	srt_cache_t  cache[SRT_CACHE_SIZE];
	int          cache_next;

	// listener environment (a sound at the listener), used as fallback
	srt_params_t listener_env;
	qboolean     listener_env_valid;
	double       listener_env_time;
	vec3_t       listener_env_pos;

	// directions
	vec3_t       dirs[SRT_MAX_RAYS];
	int          numdirs;

	// materials
	srt_texmat_t *texmats;
	int          numtexmats;
	qboolean     materials_loaded;
	struct { const texture_t *tex; byte mat; } texcache[SRT_TEXCACHE_SIZE];
	const model_t *texcache_world;

	// frame budget
	uint         frame;
	double       spent;
	int          rr_next;
	int          traces;
	int          evals_frame;

	// mixing
	float        bus[SRT_SLOTS][PAINTBUFFER_SIZE + 1];
	qboolean     bus_used[SRT_SLOTS];
	srt_reverb_t reverb[SRT_SLOTS];
	float        uw_lp[2][2];

	// stats
	double       stat_time;
	int          stat_evals, stat_traces;
	double       stat_ms;
	int          last_evals, last_traces;
	double       last_ms;
} srt;

static float SRT_Clamp( float v, float lo, float hi )
{
	return v < lo ? lo : ( v > hi ? hi : v );
}

/*
===============================================================================

MATERIALS

===============================================================================
*/
static int SRT_TexMatCompare( const void *a, const void *b )
{
	return Q_stricmp( ((const srt_texmat_t *)a)->name, ((const srt_texmat_t *)b)->name );
}

static const char *SRT_SkipLine( const char *p )
{
	while( *p && *p != '\n' ) p++;
	if( *p == '\n' ) p++;
	return p;
}

// copies a whitespace separated token, returns position after it
static const char *SRT_Token( const char *p, char *out, size_t size )
{
	size_t len = 0;

	while( *p == ' ' || *p == '\t' || *p == '\r' ) p++;

	while( *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' )
	{
		if( len + 1 < size ) out[len++] = *p;
		p++;
	}

	out[len] = '\0';
	return p;
}

static int SRT_MaterialIndexForLetter( char c )
{
	c = Q_toupper( c );
	if( c >= 'A' && c <= 'Z' )
		return c - 'A';
	return SRT_MAT_DEFAULT;
}

static void SRT_LoadTextureMaterials( void )
{
	fs_offset_t size;
	byte *file;
	int count = 0;

	Mem_Free2( &srt.texmats );
	srt.numtexmats = 0;

	file = FS_LoadFile( DEFAULT_SOUNDPATH "materials.txt", &size, false );
	if( !file )
		return;

	// upper bound on entries: line count
	for( fs_offset_t i = 0; i < size; i++ )
		if( file[i] == '\n' ) count++;
	count++;

	srt.texmats = Mem_Calloc( sndpool, sizeof( *srt.texmats ) * count );

	char *text = Mem_Malloc( sndpool, size + 1 );
	memcpy( text, file, size );
	text[size] = '\0';
	Mem_Free( file );

	for( const char *p = text; *p; p = SRT_SkipLine( p ))
	{
		char letter[8], name[64];
		const char *q = SRT_Token( p, letter, sizeof( letter ));

		if( !letter[0] || ( letter[0] == '/' && letter[1] == '/' ))
			continue;

		SRT_Token( q, name, sizeof( name ));
		if( !name[0] || srt.numtexmats >= count )
			continue;

		// game code compares the first 12 characters (CBTEXTURENAMEMAX - 1)
		Q_strncpy( srt.texmats[srt.numtexmats].name, name, 13 );
		srt.texmats[srt.numtexmats].mat = SRT_MaterialIndexForLetter( letter[0] );
		srt.numtexmats++;
	}

	Mem_Free( text );
	qsort( srt.texmats, srt.numtexmats, sizeof( *srt.texmats ), SRT_TexMatCompare );
}

// optional overrides: sound/snd_rt_materials.txt, lines of "<letter|default|fence> <reflectivity> <absorption>"
static void SRT_LoadMaterialOverrides( void )
{
	fs_offset_t size;
	byte *file = FS_LoadFile( DEFAULT_SOUNDPATH "snd_rt_materials.txt", &size, false );

	if( !file )
		return;

	char *text = Mem_Malloc( sndpool, size + 1 );
	memcpy( text, file, size );
	text[size] = '\0';
	Mem_Free( file );

	for( const char *p = text; *p; p = SRT_SkipLine( p ))
	{
		char key[32], refl[32], absorb[32];
		const char *q = SRT_Token( p, key, sizeof( key ));
		int idx;

		if( !key[0] || ( key[0] == '/' && key[1] == '/' ) || key[0] == '#' )
			continue;

		q = SRT_Token( q, refl, sizeof( refl ));
		SRT_Token( q, absorb, sizeof( absorb ));

		if( !Q_stricmp( key, "default" )) idx = SRT_MAT_DEFAULT;
		else if( !Q_stricmp( key, "fence" )) idx = SRT_MAT_FENCE;
		else if( key[1] == '\0' && Q_toupper( key[0] ) >= 'A' && Q_toupper( key[0] ) <= 'Z' ) idx = SRT_MaterialIndexForLetter( key[0] );
		else continue;

		if( refl[0] ) srt_materials[idx].reflectivity = Q_atof( refl );
		if( absorb[0] ) srt_materials[idx].absorption = Q_atof( absorb );
	}

	Mem_Free( text );
}

static void SRT_LoadMaterials( void )
{
	memcpy( srt_materials, srt_material_defaults, sizeof( srt_materials ));
	SRT_LoadTextureMaterials();
	SRT_LoadMaterialOverrides();
	memset( srt.texcache, 0, sizeof( srt.texcache ));
	srt.materials_loaded = true;
}

static int SRT_MaterialForTexture( const texture_t *tex )
{
	const char *name;
	srt_texmat_t key, *found;
	uint hash;

	if( !tex )
		return SRT_MAT_DEFAULT;

	if( srt.texcache_world != cl.worldmodel )
	{
		memset( srt.texcache, 0, sizeof( srt.texcache ));
		srt.texcache_world = cl.worldmodel;
	}

	hash = (uint)(((uintptr_t)tex >> 4 ) * 2654435761u ) & ( SRT_TEXCACHE_SIZE - 1 );
	for( int probe = 0; probe < 8; probe++ )
	{
		int i = ( hash + probe ) & ( SRT_TEXCACHE_SIZE - 1 );

		if( srt.texcache[i].tex == tex )
			return srt.texcache[i].mat;

		if( !srt.texcache[i].tex )
		{
			hash = i;
			break;
		}
	}

	name = tex->name;

	int mat = SRT_MAT_DEFAULT;

	if( name[0] == '{' )
		mat = SRT_MAT_FENCE;

	// same prefix stripping as the game's texture type lookup
	if( name[0] == '-' || name[0] == '+' )
		name += ( name[1] ? 2 : 1 );
	if( name[0] == '{' || name[0] == '!' || name[0] == '~' || name[0] == ' ' )
		name++;

	if( srt.numtexmats > 0 )
	{
		Q_strncpy( key.name, name, 13 );
		found = bsearch( &key, srt.texmats, srt.numtexmats, sizeof( key ), SRT_TexMatCompare );

		if( !found && tex->name[0] == '!' )
		{
			// CoF lists some liquids with the '!'
			Q_strncpy( key.name, tex->name, 13 );
			found = bsearch( &key, srt.texmats, srt.numtexmats, sizeof( key ), SRT_TexMatCompare );
		}

		if( found )
			mat = found->mat;
	}

	srt.texcache[hash & ( SRT_TEXCACHE_SIZE - 1 )].tex = tex;
	srt.texcache[hash & ( SRT_TEXCACHE_SIZE - 1 )].mat = mat;

	return mat;
}

static const srt_material_t *SRT_SpanMaterial( const pm_soundspan_t *span, float *absorb_scale )
{
	const texture_t *tex = NULL;
	int mat;

	if( span->surf && span->surf->texinfo )
		tex = span->surf->texinfo->texture;

	mat = SRT_MaterialForTexture( tex );

	*absorb_scale = 1.0f;

	// translucent brush entities are windows and grates
	if( span->entindex > 0 && span->entindex < clgame.pmove->numphysent )
	{
		if( clgame.pmove->physents[span->entindex].rendermode != kRenderNormal )
			*absorb_scale = 0.35f;
	}

	return &srt_materials[mat];
}

/*
===============================================================================

TRACING

===============================================================================
*/
static qboolean SRT_WorldReady( void )
{
	if( cls.state != ca_active || !cl.worldmodel || !clgame.pmove )
		return false;

	if( clgame.pmove->numphysent <= 0 || clgame.pmove->physents[0].model != cl.worldmodel )
		return false;

	return true;
}

static qboolean SRT_PointInSolid( const vec3_t p )
{
	hull_t *hull = &cl.worldmodel->hulls[0];
	return PM_HullPointContents( hull, hull->firstclipnode, p ) == CONTENTS_SOLID;
}

static int SRT_Trace( const vec3_t a, const vec3_t b, pm_soundspan_t *spans, int max, qboolean first_only )
{
	srt.traces++;
	return PM_SoundTrace( clgame.pmove, a, b, spans, max, first_only, snd_rt_brush_entities.value == 0.0f );
}

static qboolean SRT_Visible( const vec3_t a, const vec3_t b )
{
	pm_soundspan_t span;
	return SRT_Trace( a, b, &span, 1, true ) == 0;
}

static void SRT_BuildDirections( int count )
{
	const float golden_angle = M_PI_F * ( 3.0f - sqrtf( 5.0f ));

	count = bound( 4, count, SRT_MAX_RAYS );
	if( count == srt.numdirs )
		return;

	for( int i = 0; i < count; i++ )
	{
		float z = 1.0f - 2.0f * ( i + 0.5f ) / count;
		float r = sqrtf( Q_max( 0.0f, 1.0f - z * z ));
		float phi = golden_angle * i;

		VectorSet( srt.dirs[i], cosf( phi ) * r, sinf( phi ) * r, z );
	}

	srt.numdirs = count;
}

// moves a point that sits inside world geometry to the nearest open spot
static void SRT_UnstickPoint( vec3_t p )
{
	static const float offsets[] = { 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
	static const vec3_t axes[] =
	{
		{ 0, 0, 1 }, { 0, 0, -1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }
	};

	if( !SRT_PointInSolid( p ))
		return;

	for( int i = 0; i < ARRAYSIZE( offsets ); i++ )
	{
		for( int j = 0; j < ARRAYSIZE( axes ); j++ )
		{
			vec3_t test;
			VectorMA( p, offsets[i], axes[j], test );

			if( !SRT_PointInSolid( test ))
			{
				VectorCopy( test, p );
				return;
			}
		}
	}
}

// occlusion accumulated along one path
static float SRT_PathOcclusion( const vec3_t a, const vec3_t b )
{
	pm_soundspan_t spans[SRT_MAX_SPANS];
	vec3_t delta;
	float len, occ = 0.0f;
	float unit = Q_max( 1.0f, snd_rt_occlusion_unit.value );
	int n;

	VectorSubtract( b, a, delta );
	len = VectorLength( delta );
	if( len < 1.0f )
		return 0.0f;

	n = SRT_Trace( a, b, spans, ARRAYSIZE( spans ), false );

	for( int i = 0; i < n; i++ )
	{
		float scale, thickness, blocks;
		const srt_material_t *mat;

		if( spans[i].contents != CONTENTS_SOLID )
			continue; // sky brushes do not occlude

		mat = SRT_SpanMaterial( &spans[i], &scale );
		thickness = ( spans[i].frac_out - spans[i].frac_in ) * len;

		// thin walls still muffle, thick ones keep muffling but level off
		blocks = SRT_Clamp( thickness / unit, 0.25f, 4.0f );
		occ += mat->absorption * scale * blocks;
	}

	return occ;
}

static float SRT_Occlusion( const vec3_t source, const vec3_t listener )
{
	float occ = SRT_PathOcclusion( source, listener );
	float radius = snd_rt_occlusion_variation.value;

	if( occ > 0.0f && radius > 0.0f )
	{
		// a few rays from around the source; sound bends round edges, keep the least occluded
		vec3_t dir, right, up;
		VectorSubtract( listener, source, dir );
		VectorNormalize( dir );
		VectorVectors( dir, right, up );

		for( int i = 0; i < 4 && occ > 0.0f; i++ )
		{
			vec3_t p;
			float sx = ( i == 0 ) ? 1.0f : ( i == 1 ) ? -1.0f : 0.0f;
			float sy = ( i == 2 ) ? 1.0f : ( i == 3 ) ? -1.0f : 0.0f;

			VectorMA( source, sx * radius, right, p );
			VectorMA( p, sy * radius, up, p );

			if( SRT_PointInSolid( p ))
				continue;

			// the offset point must actually be reachable from the source
			if( !SRT_Visible( source, p ))
				continue;

			occ = Q_min( occ, SRT_PathOcclusion( p, listener ));
		}
	}

	return Q_min( occ, snd_rt_max_occlusion.value );
}

// crossfade weights between the two reverbs whose decay times bracket rt60
static void SRT_SlotWeights( float rt60, float *w )
{
	float decay[SRT_SLOTS];

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		decay[k] = Q_max( 0.05f, srt_slot_cvars[k]->value );
		w[k] = 0.0f;
	}

	if( rt60 <= decay[0] )
	{
		w[0] = 1.0f;
		return;
	}

	for( int k = 0; k < SRT_SLOTS - 1; k++ )
	{
		if( rt60 <= decay[k + 1] && decay[k + 1] > decay[k] )
		{
			float t = logf( rt60 / decay[k] ) / logf( decay[k + 1] / decay[k] );
			t = SRT_Clamp( t, 0.0f, 1.0f );

			// equal power
			w[k] = cosf( t * M_PI_F * 0.5f );
			w[k + 1] = sinf( t * M_PI_F * 0.5f );
			return;
		}
	}

	w[SRT_SLOTS - 1] = 1.0f;
}

/*
==================
SRT_Evaluate

computes the acoustic parameters of a sound at source heard at listener

rays bounce around the source; the mean free path, average surface absorption
and how often rays escape (sky, open space) give an Eyring reverb time, which
selects the reverb slots. The fraction of rays that hit anything sets how much
reverb there is at all. Occlusion and shared airspace work as in Sound Physics.
==================
*/
static void SRT_Evaluate( const vec3_t source_in, const vec3_t listener, qboolean self, srt_params_t *out )
{
	const float upm = Q_max( 1.0f, snd_rt_units_per_meter.value );
	const float maxdist = Q_max( 64.0f, snd_rt_maxdist.value );
	const float refl_mult = Q_max( 0.05f, snd_rt_reflectivity.value );
	const float absorption_coeff = Q_max( 0.0f, snd_rt_occlusion.value ) * 3.0f;
	const int bounces = bound( 1, (int)snd_rt_bounces.value, SRT_MAX_BOUNCES );
	float shared = 0.0f, occlusion = 0.0f;
	float seg_len = 0.0f, absorb_sum = 0.0f;
	int seg_hits = 0, seg_escapes = 0, primary_hits = 0, vis_tests = 0;
	vec3_t source;

	SRT_BuildDirections( (int)snd_rt_rays.value );

	VectorCopy( source_in, source );
	SRT_UnstickPoint( source );

	if( !self && absorption_coeff > 0.0f )
		occlusion = SRT_Occlusion( source, listener );

	for( int i = 0; i < srt.numdirs; i++ )
	{
		pm_soundspan_t hit;
		vec3_t pos, dir, end, normal;

		VectorCopy( source, pos );
		VectorCopy( srt.dirs[i], dir );
		VectorMA( pos, maxdist, dir, end );

		if( !SRT_Trace( pos, end, &hit, 1, true ) || hit.contents == CONTENTS_SKY )
		{
			seg_escapes++;
			continue;
		}

		primary_hits++;

		for( int b = 0; b < bounces; b++ )
		{
			float scale, alpha, d;
			vec3_t hitpos, newdir, newend;
			pm_soundspan_t next;
			const srt_material_t *mat;

			VectorLerp( pos, hit.frac_in, end, hitpos );
			seg_len += hit.frac_in * maxdist;
			seg_hits++;

			if( VectorIsNull( hit.normal )) VectorNegate( dir, normal );
			else VectorCopy( hit.normal, normal );

			// step off the surface
			VectorMA( hitpos, 1.0f, normal, hitpos );

			mat = SRT_SpanMaterial( &hit, &scale );
			alpha = ( 1.0f - SRT_Clamp( mat->reflectivity, 0.0f, 0.999f )) / refl_mult;
			absorb_sum += SRT_Clamp( alpha, 0.001f, 1.0f );

			// can this reflection reach the listener?
			vis_tests++;
			if( SRT_Visible( hitpos, listener ))
				shared += 1.0f;

			if( b == bounces - 1 )
				break;

			// reflect and continue
			d = DotProduct( dir, normal );
			VectorMA( dir, -2.0f * d, normal, newdir );
			VectorNormalize( newdir );
			VectorMA( hitpos, maxdist, newdir, newend );

			if( !SRT_Trace( hitpos, newend, &next, 1, true ) || next.contents == CONTENTS_SKY )
			{
				seg_escapes++;
				break;
			}

			VectorCopy( hitpos, pos );
			VectorCopy( newdir, dir );
			VectorCopy( newend, end );
			hit = next;
		}
	}

	// reverb time (Eyring): RT60 = 0.161 V / (-S ln(1 - a)), mean free path = 4V / S
	float rt60 = 0.0f, enclosure = 0.0f;
	if( seg_hits > 0 )
	{
		float mfp = ( seg_len / seg_hits ) / upm;
		float alpha = absorb_sum / seg_hits;
		float p_escape = (float)seg_escapes / ( seg_hits + seg_escapes );
		float alpha_eff = 1.0f - ( 1.0f - alpha ) * ( 1.0f - p_escape );

		alpha_eff = SRT_Clamp( alpha_eff, 0.001f, 0.999f );
		rt60 = 0.0403f * mfp / -logf( 1.0f - alpha_eff );
		rt60 = SRT_Clamp( rt60 * Q_max( 0.05f, snd_rt_decay_scale.value ), 0.05f, 10.0f );
		enclosure = (float)primary_hits / srt.numdirs;
	}

	// fraction of bounce points that see the listener, Sound Physics scale (0-64)
	shared = vis_tests > 0 ? shared * 64.0f / vis_tests : 0.0f;
	if( snd_rt_airspace.value == 0.0f || self )
		shared = self ? 64.0f : 0.0f;

	float weight[SRT_SLOTS];
	weight[0] = SRT_Clamp( shared / 20.0f, 0.0f, 1.0f );
	weight[1] = SRT_Clamp( shared / 15.0f, 0.0f, 1.0f );
	weight[2] = SRT_Clamp( shared / 10.0f, 0.0f, 1.0f );
	weight[3] = SRT_Clamp( shared / 10.0f, 0.0f, 1.0f );

	static const float slot_occlusion[SRT_SLOTS] = { 1.0f, 1.5f, 1.5f, 1.5f };
	for( int k = 0; k < SRT_SLOTS; k++ )
		out->send_cutoff[k] = expf( -occlusion * absorption_coeff * slot_occlusion[k] ) * ( 1.0f - weight[k] ) + weight[k];

	float avg_shared = ( weight[0] + weight[1] + weight[2] + weight[3] ) * 0.25f;
	float direct_cutoff = expf( -occlusion * absorption_coeff );

	// keep some direction when the airspace is shared
	direct_cutoff = Q_max( sqrtf( avg_shared ) * 0.2f, direct_cutoff );

	out->direct_cutoff = direct_cutoff;
	out->direct_gain = powf( direct_cutoff, 0.1f );

	float slot_w[SRT_SLOTS];
	SRT_SlotWeights( rt60, slot_w );

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		float g = rt60 > 0.0f ? enclosure * slot_w[k] : 0.0f;
		out->send_gain[k] = SRT_Clamp( g, 0.0f, 1.0f ) * powf( out->send_cutoff[k], 0.1f );
	}

	out->occlusion = occlusion;
	out->airspace = shared;
	out->rt60 = rt60;
}

// cheap approximation: the listener's own environment plus this sound's occlusion
static void SRT_EvaluateFallback( const vec3_t source_in, const vec3_t listener, qboolean self, srt_params_t *out )
{
	const float absorption_coeff = Q_max( 0.0f, snd_rt_occlusion.value ) * 3.0f;
	float occlusion = 0.0f;
	vec3_t source;

	*out = srt.listener_env;

	if( self || absorption_coeff <= 0.0f )
		return;

	VectorCopy( source_in, source );
	SRT_UnstickPoint( source );
	occlusion = SRT_Occlusion( source, listener );

	if( occlusion <= 0.0f )
		return;

	out->direct_cutoff = expf( -occlusion * absorption_coeff );
	out->direct_gain = powf( out->direct_cutoff, 0.1f );

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		float c = expf( -occlusion * absorption_coeff * ( k ? 1.5f : 1.0f ));
		out->send_cutoff[k] = c;
		out->send_gain[k] *= powf( c, 0.1f );
	}

	out->occlusion = occlusion;
	out->airspace = 0.0f;
}

static qboolean SRT_CacheLookup( const vec3_t source, const vec3_t listener, srt_params_t *out )
{
	const double maxage = Q_max( 0.05f, snd_rt_refresh.value * 2.0f );

	for( int i = 0; i < SRT_CACHE_SIZE; i++ )
	{
		srt_cache_t *c = &srt.cache[i];

		if( c->time <= 0.0 || host.realtime - c->time > maxage )
			continue;

		if( VectorDistance2( c->source, source ) > 8.0f * 8.0f )
			continue;

		if( VectorDistance2( c->listener, listener ) > 8.0f * 8.0f )
			continue;

		*out = c->params;
		return true;
	}

	return false;
}

static void SRT_CacheStore( const vec3_t source, const vec3_t listener, const srt_params_t *params )
{
	srt_cache_t *c = &srt.cache[srt.cache_next];

	srt.cache_next = ( srt.cache_next + 1 ) % SRT_CACHE_SIZE;
	c->time = host.realtime;
	VectorCopy( source, c->source );
	VectorCopy( listener, c->listener );
	c->params = *params;
}

static void SRT_TimedEvaluate( const vec3_t source, const vec3_t listener, qboolean self, srt_params_t *out )
{
	double start;

	if( SRT_CacheLookup( source, listener, out ))
		return;

	start = Platform_DoubleTime();
	srt.traces = 0;

	SRT_Evaluate( source, listener, self, out );

	double ms = ( Platform_DoubleTime() - start ) * 1000.0;
	srt.spent += ms;
	srt.stat_ms += ms;
	srt.stat_evals++;
	srt.stat_traces += srt.traces;
	srt.evals_frame++;

	SRT_CacheStore( source, listener, out );
}

/*
===============================================================================

CHANNEL SCHEDULING

===============================================================================
*/
static qboolean SRT_Enabled( void )
{
	return snd_rt.value != 0.0f;
}

qboolean SND_RT_Active( void )
{
	return srt.initialized && SRT_Enabled();
}

void SND_RT_ResetChannel( const channel_t *ch )
{
	int idx = ch - snd.channels;
	srt_chan_t *s;

	if( idx < 0 || idx >= MAX_CHANNELS )
		return;

	s = &srt.chans[idx];
	memset( s, 0, sizeof( *s ));
	s->pending = true;
}

static qboolean SRT_ChannelEligible( const channel_t *ch, qboolean *self )
{
	*self = false;

	if( FBitSet( ch->flags, FL_CHAN_LOCAL_SOUND ))
		return false; // menu and console sounds

	if( ch->entnum == snd.entnum )
	{
		*self = true; // the listener's own sounds: environment only
		return true;
	}

	// sounds played everywhere have no position
	if( ch->dist_mult <= 0.0f )
		return false;

	return true;
}

static void SRT_SourcePosition( const channel_t *ch, qboolean self, vec3_t out )
{
	if( self )
	{
		VectorCopy( snd.origin, out );
		return;
	}

	VectorCopy( ch->origin, out );

	if( snd_rt_source_lift.value && !FBitSet( ch->flags, FL_CHAN_STATIC_SOUND ) && ch->entnum > 0 )
	{
		cl_entity_t *ent = CL_GetEntityByIndex( ch->entnum );

		// model origins are at the feet, sound comes from the body
		if( ent && ent->model && ent->model->type == mod_studio && ent->curstate.maxs[2] > 0.0f )
		{
			vec3_t lifted;
			VectorCopy( out, lifted );
			lifted[2] += Q_min( ent->curstate.maxs[2] * 0.6f, 48.0f );

			if( !SRT_PointInSolid( lifted ) && SRT_Visible( out, lifted ))
				VectorCopy( lifted, out );
		}
	}
}

static void SRT_UpdateListenerEnv( void )
{
	double interval = Q_max( 0.02f, snd_rt_refresh.value );
	qboolean moved = VectorDistance2( srt.listener_env_pos, snd.origin ) > 8.0f * 8.0f;

	if( srt.listener_env_valid && host.realtime - srt.listener_env_time < ( moved ? interval : snd_rt_refresh_static.value ))
		return;

	SRT_TimedEvaluate( snd.origin, snd.origin, true, &srt.listener_env );
	srt.listener_env_valid = true;
	srt.listener_env_time = host.realtime;
	VectorCopy( snd.origin, srt.listener_env_pos );
}

static void SRT_UpdateChannel( channel_t *ch, srt_chan_t *s, qboolean self, qboolean allow_full )
{
	vec3_t source;

	SRT_SourcePosition( ch, self, source );

	if( allow_full )
	{
		SRT_TimedEvaluate( source, snd.origin, self, &s->target );
		s->fallback = false;
	}
	else
	{
		SRT_EvaluateFallback( source, snd.origin, self, &s->target );
		s->fallback = true;
	}

	if( s->pending || !s->valid )
	{
		s->cur = s->target;
		s->dry_gain_prev = s->cur.direct_gain;
		for( int k = 0; k < SRT_SLOTS; k++ )
			s->send_gain_prev[k] = s->cur.send_gain[k];
	}

	s->valid = true;
	s->pending = false;
	s->last_eval = host.realtime;
	VectorCopy( source, s->eval_source );
	VectorCopy( snd.origin, s->eval_listener );
}

void SND_RT_Frame( void )
{
	const double budget = Q_max( 0.1f, snd_rt_budget.value );

	if( !SND_RT_Active( ))
		return;

	if( !srt.materials_loaded )
		SRT_LoadMaterials();

	if( srt.frame != host.framecount )
	{
		srt.frame = host.framecount;
		srt.spent = 0.0;
		srt.evals_frame = 0;
	}

	if( host.realtime - srt.stat_time >= 1.0 )
	{
		srt.last_evals = srt.stat_evals;
		srt.last_traces = srt.stat_traces;
		srt.last_ms = srt.stat_ms;
		srt.stat_evals = srt.stat_traces = 0;
		srt.stat_ms = 0.0;
		srt.stat_time = host.realtime;
	}

	if( !SRT_WorldReady() || SRT_PointInSolid( snd.origin ))
	{
		// no world to trace (loading, menus) or noclipping: play dry
		for( int i = 0; i < MAX_CHANNELS; i++ )
			srt.chans[i].valid = false;
		srt.listener_env_valid = false;
		return;
	}

	SRT_UpdateListenerEnv();

	// new sounds first, they must not start dry
	for( int i = NUM_AMBIENTS; i < snd.total_channels; i++ )
	{
		channel_t *ch = &snd.channels[i];
		srt_chan_t *s = &srt.chans[i];
		qboolean self;

		if( !ch->sfx || !s->pending )
			continue;

		if( !SRT_ChannelEligible( ch, &self ))
		{
			s->pending = false;
			s->valid = false;
			continue;
		}

		// over budget: approximate now, refine on a later frame
		SRT_UpdateChannel( ch, s, self, srt.spent < budget * 2.0 );
	}

	// then refresh playing sounds, round robin, within the budget
	if( srt.rr_next >= snd.total_channels )
		srt.rr_next = 0;

	for( int n = 0; n < snd.total_channels && srt.spent < budget; n++ )
	{
		int i = srt.rr_next;
		channel_t *ch;
		srt_chan_t *s;
		qboolean self, moved;
		vec3_t source;
		double interval;

		srt.rr_next = ( srt.rr_next + 1 ) % snd.total_channels;
		if( i < NUM_AMBIENTS )
			continue;

		ch = &snd.channels[i];
		s = &srt.chans[i];

		if( !ch->sfx || !s->valid )
			continue;

		if( !ch->leftvol && !ch->rightvol )
			continue;

		if( !SRT_ChannelEligible( ch, &self ))
		{
			s->valid = false;
			continue;
		}

		SRT_SourcePosition( ch, self, source );
		moved = VectorDistance2( source, s->eval_source ) > 8.0f * 8.0f || VectorDistance2( snd.origin, s->eval_listener ) > 8.0f * 8.0f;
		interval = moved ? snd_rt_refresh.value : snd_rt_refresh_static.value;

		if( !s->fallback && host.realtime - s->last_eval < interval )
			continue;

		SRT_UpdateChannel( ch, s, self, true );
	}
}

/*
===============================================================================

DSP

===============================================================================
*/
static void SRT_DelayAlloc( srt_delay_t *d, int size )
{
	d->size = Q_max( 2, size );
	d->pos = 0;
	d->buf = Mem_Calloc( sndpool, sizeof( float ) * d->size );
}

static void SRT_DelayFree( srt_delay_t *d )
{
	Mem_Free2( &d->buf );
	d->size = d->pos = 0;
}

static inline float SRT_DelayTap( const srt_delay_t *d, int delay )
{
	int idx = d->pos - delay;
	if( idx < 0 ) idx += d->size;
	return d->buf[idx];
}

static inline void SRT_DelayWrite( srt_delay_t *d, float v )
{
	d->buf[d->pos] = v;
	if( ++d->pos >= d->size ) d->pos = 0;
}

static void SRT_ReverbConfigure( srt_reverb_t *rv, int slot )
{
	const srt_slot_def_t *def = &srt_slot_defs[slot];
	float decay = Q_max( 0.05f, srt_slot_cvars[slot]->value );
	float bright = Q_max( 0.05f, snd_rt_reverb_brightness.value );
	float hf_decay = Q_max( 0.02f, decay * def->hf_ratio * bright );

	for( int i = 0; i < SRT_FDN_LINES; i++ )
	{
		float len = (float)rv->line_len[i];
		float g = powf( 10.0f, -3.0f * len / ( decay * srt.rate ));
		float g_hf = powf( 10.0f, -3.0f * len / ( hf_decay * srt.rate ));
		float r = SRT_Clamp( g_hf / g, 0.01f, 1.0f );

		// one pole lowpass: DC gain 1, nyquist gain r
		rv->line_gain[i] = g;
		rv->line_damp[i] = ( 1.0f - r ) / ( 1.0f + r );
	}

	rv->cfg_decay = decay;
	rv->cfg_bright = bright;
}

static void SRT_ReverbInit( srt_reverb_t *rv, int slot )
{
	static const float line_ms[SRT_FDN_LINES] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.1f };
	static const float allpass_ms[SRT_ALLPASSES] = { 4.77f, 3.59f, 12.73f, 9.30f };
	static const float refl_ms[4] = { 0.0f, 3.1f, 7.3f, 11.9f };
	const srt_slot_def_t *def = &srt_slot_defs[slot];
	const float ms = srt.rate / 1000.0f;

	memset( rv, 0, sizeof( *rv ));

	SRT_DelayAlloc( &rv->predelay, (int)(( def->refl_delay + def->late_delay + 0.02f ) * srt.rate ) + 4 );

	for( int i = 0; i < SRT_ALLPASSES; i++ )
		SRT_DelayAlloc( &rv->allpass[i], (int)( allpass_ms[i] * ms * Q_max( 0.5f, def->size )) + 1 );
	rv->allpass_gain = 0.6f;

	for( int i = 0; i < SRT_FDN_LINES; i++ )
	{
		rv->line_len[i] = Q_max( 16, (int)( line_ms[i] * ms * def->size ));
		SRT_DelayAlloc( &rv->line[i], rv->line_len[i] + 1 );
	}

	for( int i = 0; i < 4; i++ )
		rv->refl_taps[i] = 1 + (int)(( def->refl_delay * 1000.0f + refl_ms[i] ) * ms );
	rv->late_tap = 1 + (int)(( def->refl_delay + def->late_delay ) * srt.rate );
	rv->refl_gain = def->refl_gain * 0.25f;
	rv->out_gain = 1.0f / sqrtf( SRT_FDN_LINES );

	SRT_ReverbConfigure( rv, slot );
}

static void SRT_ReverbFree( srt_reverb_t *rv )
{
	SRT_DelayFree( &rv->predelay );
	for( int i = 0; i < SRT_ALLPASSES; i++ )
		SRT_DelayFree( &rv->allpass[i] );
	for( int i = 0; i < SRT_FDN_LINES; i++ )
		SRT_DelayFree( &rv->line[i] );
}

static inline void SRT_Hadamard8( float *x )
{
	for( int h = 1; h < 8; h <<= 1 )
	{
		for( int i = 0; i < 8; i += h << 1 )
		{
			for( int j = i; j < i + h; j++ )
			{
				float a = x[j], b = x[j + h];
				x[j] = a + b;
				x[j + h] = a - b;
			}
		}
	}

	for( int i = 0; i < 8; i++ )
		x[i] *= 0.35355339f; // 1/sqrt(8)
}

static void SRT_ReverbProcess( srt_reverb_t *rv, const float *in, float *out_l, float *out_r, int count )
{
	for( int n = 0; n < count; n++ )
	{
		float x, early_l, early_r, late_l = 0.0f, late_r = 0.0f;
		float y[SRT_FDN_LINES];

		SRT_DelayWrite( &rv->predelay, in[n] );

		// early reflections, alternating sides
		early_l = ( SRT_DelayTap( &rv->predelay, rv->refl_taps[0] ) + SRT_DelayTap( &rv->predelay, rv->refl_taps[2] )) * rv->refl_gain;
		early_r = ( SRT_DelayTap( &rv->predelay, rv->refl_taps[1] ) + SRT_DelayTap( &rv->predelay, rv->refl_taps[3] )) * rv->refl_gain;

		// diffuse the late input
		x = SRT_DelayTap( &rv->predelay, rv->late_tap );
		for( int i = 0; i < SRT_ALLPASSES; i++ )
		{
			srt_delay_t *ap = &rv->allpass[i];
			float delayed = ap->buf[ap->pos];
			float v = x + rv->allpass_gain * delayed;

			x = delayed - rv->allpass_gain * v;
			ap->buf[ap->pos] = v;
			if( ++ap->pos >= ap->size ) ap->pos = 0;
		}

		// feedback delay network
		for( int i = 0; i < SRT_FDN_LINES; i++ )
		{
			float v = SRT_DelayTap( &rv->line[i], rv->line_len[i] );

			if( i & 1 ) late_r += v;
			else late_l += v;

			// damping then decay
			rv->line_lp[i] = v + ( rv->line_lp[i] - v ) * rv->line_damp[i];
			y[i] = rv->line_lp[i] * rv->line_gain[i];
		}

		SRT_Hadamard8( y );

		for( int i = 0; i < SRT_FDN_LINES; i++ )
			SRT_DelayWrite( &rv->line[i], y[i] + x * (( i & 2 ) ? -0.5f : 0.5f ));

		out_l[n] = early_l + late_l * rv->out_gain;
		out_r[n] = early_r + late_r * rv->out_gain;
	}
}

static void SRT_FreeDSP( void )
{
	for( int k = 0; k < SRT_SLOTS; k++ )
		SRT_ReverbFree( &srt.reverb[k] );
	srt.rate = 0;
}

static void SRT_CheckDSP( void )
{
	int rate = snd.format.speed ? snd.format.speed : SOUND_DMA_SPEED;

	if( srt.rate != rate )
	{
		SRT_FreeDSP();
		srt.rate = rate;
		for( int k = 0; k < SRT_SLOTS; k++ )
			SRT_ReverbInit( &srt.reverb[k], k );
		memset( srt.bus, 0, sizeof( srt.bus ));
		memset( srt.uw_lp, 0, sizeof( srt.uw_lp ));
		return;
	}

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		srt_reverb_t *rv = &srt.reverb[k];

		if( rv->cfg_decay != Q_max( 0.05f, srt_slot_cvars[k]->value ) || rv->cfg_bright != Q_max( 0.05f, snd_rt_reverb_brightness.value ))
			SRT_ReverbConfigure( rv, k );
	}
}

// one pole coefficient for a lowpass whose cascade of 'poles' stages has gain hf_gain at 5 kHz
static float SRT_LowpassCoef( float hf_gain, int poles )
{
	float fc, ratio;

	if( hf_gain >= 0.999f )
		return 1.0f;

	hf_gain = Q_max( hf_gain, 0.0005f );

	// |H(f)|^2 per pole = 1 / (1 + (f/fc)^2)
	ratio = poles == 2 ? ( 1.0f / hf_gain - 1.0f ) : ( 1.0f / ( hf_gain * hf_gain ) - 1.0f );
	fc = SRT_HF_REFERENCE / sqrtf( Q_max( ratio, 1e-6f ));
	fc = SRT_Clamp( fc, 60.0f, srt.rate * 0.45f );

	return 1.0f - expf( -2.0f * M_PI_F * fc / srt.rate );
}

static void SRT_SmoothParams( srt_chan_t *s, int count )
{
	float tau = Q_max( 0.001f, snd_rt_smoothing.value );
	float k = 1.0f - expf( -(float)count / ( tau * srt.rate ));
	srt_params_t *c = &s->cur;
	const srt_params_t *t = &s->target;

	c->direct_gain += ( t->direct_gain - c->direct_gain ) * k;
	c->direct_cutoff += ( t->direct_cutoff - c->direct_cutoff ) * k;
	for( int i = 0; i < SRT_SLOTS; i++ )
	{
		c->send_gain[i] += ( t->send_gain[i] - c->send_gain[i] ) * k;
		c->send_cutoff[i] += ( t->send_cutoff[i] - c->send_cutoff[i] ) * k;
	}
}

/*
==================
SND_RT_MixChannel

adds one channel's freshly mixed samples to the room buffer, applying the
direct path filter, and feeds the reverb sends
==================
*/
void SND_RT_MixChannel( const channel_t *ch, const portable_samplepair_t *src, portable_samplepair_t *dst, int count )
{
	int idx = ch - snd.channels;
	srt_chan_t *s;
	float inv = 1.0f / Q_max( count, 1 );

	if( idx < 0 || idx >= MAX_CHANNELS || !srt.rate || !srt.chans[idx].valid )
	{
		for( int i = 0; i < count; i++ )
		{
			dst[i].left += src[i].left;
			dst[i].right += src[i].right;
		}
		return;
	}

	s = &srt.chans[idx];
	SRT_SmoothParams( s, count );

	// direct path
	{
		const float a = SRT_LowpassCoef( s->cur.direct_cutoff, 2 );
		const float g0 = s->dry_gain_prev, g1 = s->cur.direct_gain;
		float l0 = s->dry_lp[0][0], l1 = s->dry_lp[1][0];
		float r0 = s->dry_lp[0][1], r1 = s->dry_lp[1][1];

		if( a >= 1.0f )
		{
			for( int i = 0; i < count; i++ )
			{
				float g = g0 + ( g1 - g0 ) * ( i * inv );
				dst[i].left += (int)( src[i].left * g );
				dst[i].right += (int)( src[i].right * g );
			}
			// keep the filter primed so switching back in does not click
			if( count > 0 )
			{
				l0 = l1 = (float)src[count - 1].left;
				r0 = r1 = (float)src[count - 1].right;
			}
		}
		else
		{
			for( int i = 0; i < count; i++ )
			{
				float g = g0 + ( g1 - g0 ) * ( i * inv );

				l0 += a * ( src[i].left - l0 );
				l1 += a * ( l0 - l1 );
				r0 += a * ( src[i].right - r0 );
				r1 += a * ( r0 - r1 );

				dst[i].left += (int)( l1 * g );
				dst[i].right += (int)( r1 * g );
			}
		}

		s->dry_lp[0][0] = l0; s->dry_lp[1][0] = l1;
		s->dry_lp[0][1] = r0; s->dry_lp[1][1] = r1;
		s->dry_gain_prev = g1;
	}

	// reverb sends
	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		const float g0 = s->send_gain_prev[k], g1 = s->cur.send_gain[k];
		const float a = SRT_LowpassCoef( s->cur.send_cutoff[k], 1 );
		float *bus = srt.bus[k];
		float lp = s->send_lp[k];

		s->send_gain_prev[k] = g1;

		if( g0 < 0.0005f && g1 < 0.0005f )
		{
			s->send_lp[k] = 0.0f;
			continue;
		}

		for( int i = 0; i < count; i++ )
		{
			float m = ( src[i].left + src[i].right ) * 0.5f;
			float g = g0 + ( g1 - g0 ) * ( i * inv );

			lp += a * ( m - lp );
			bus[i] += lp * g;
		}

		s->send_lp[k] = lp;
		srt.bus_used[k] = true;
	}
}

/*
==================
SND_RT_RenderReverb

runs the reverbs on what the channels sent this block and adds them to the room buffer
==================
*/
void SND_RT_RenderReverb( portable_samplepair_t *dst, int count, qboolean paused )
{
	static float out_l[PAINTBUFFER_SIZE + 1], out_r[PAINTBUFFER_SIZE + 1];
	const float master = Q_max( 0.0f, snd_rt_reverb_gain.value );

	count = Q_min( count, PAINTBUFFER_SIZE );

	if( !paused )
	{
		for( int k = 0; k < SRT_SLOTS; k++ )
		{
			srt_reverb_t *rv = &srt.reverb[k];
			const float gain = srt_slot_defs[k].gain * master;

			if( !rv->predelay.buf )
				continue;

			SRT_ReverbProcess( rv, srt.bus[k], out_l, out_r, count );

			for( int i = 0; i < count; i++ )
			{
				dst[i].left += (int)( out_l[i] * gain );
				dst[i].right += (int)( out_r[i] * gain );
			}
		}

		// underwater: muffle everything
		if( cl.local.waterlevel > 2 && snd_rt_underwater.value < 0.999f )
		{
			const float a = SRT_LowpassCoef( snd_rt_underwater.value, 2 );

			for( int i = 0; i < count; i++ )
			{
				srt.uw_lp[0][0] += a * ( dst[i].left - srt.uw_lp[0][0] );
				srt.uw_lp[1][0] += a * ( srt.uw_lp[0][0] - srt.uw_lp[1][0] );
				srt.uw_lp[0][1] += a * ( dst[i].right - srt.uw_lp[0][1] );
				srt.uw_lp[1][1] += a * ( srt.uw_lp[0][1] - srt.uw_lp[1][1] );
				dst[i].left = (int)srt.uw_lp[1][0];
				dst[i].right = (int)srt.uw_lp[1][1];
			}
		}
		else memset( srt.uw_lp, 0, sizeof( srt.uw_lp ));
	}

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		if( srt.bus_used[k] )
			memset( srt.bus[k], 0, sizeof( float ) * ( count + 1 ));
		srt.bus_used[k] = false;
	}
}

void SND_RT_PrepareMix( void )
{
	SRT_CheckDSP();
}

/*
===============================================================================

DEBUGGING

===============================================================================
*/
static void SRT_PrintParams( const char *label, const srt_params_t *p )
{
	Con_Printf( "%s: rt60 %.2fs | direct gain %.2f cutoff %.2f | sends %.2f %.2f %.2f %.2f | cutoffs %.2f %.2f %.2f %.2f | occlusion %.2f airspace %.1f\n",
		label, p->rt60, p->direct_gain, p->direct_cutoff,
		p->send_gain[0], p->send_gain[1], p->send_gain[2], p->send_gain[3],
		p->send_cutoff[0], p->send_cutoff[1], p->send_cutoff[2], p->send_cutoff[3],
		p->occlusion, p->airspace );
}

// evaluates a sound at the point you look at (or "snd_rt_probe x y z")
static void SRT_Probe_f( void )
{
	vec3_t source;
	srt_params_t p;
	double start;

	if( !SRT_WorldReady( ))
	{
		Con_Printf( "snd_rt_probe: not in a map\n" );
		return;
	}

	if( !srt.materials_loaded )
		SRT_LoadMaterials();

	if( Cmd_Argc() >= 4 )
	{
		VectorSet( source, Q_atof( Cmd_Argv( 1 )), Q_atof( Cmd_Argv( 2 )), Q_atof( Cmd_Argv( 3 )));
	}
	else
	{
		pm_soundspan_t hit;
		vec3_t end;

		VectorMA( snd.origin, 8192.0f, snd.forward, end );
		if( SRT_Trace( snd.origin, end, &hit, 1, true ))
		{
			const texture_t *tex = ( hit.surf && hit.surf->texinfo ) ? hit.surf->texinfo->texture : NULL;
			float scale;
			const srt_material_t *mat = SRT_SpanMaterial( &hit, &scale );

			VectorLerp( snd.origin, hit.frac_in, end, source );
			VectorMA( source, -16.0f, snd.forward, source );
			Con_Printf( "looking at %s (%s, reflectivity %.2f, absorption %.2f), %.0f units away\n",
				tex ? tex->name : "?", mat->name, mat->reflectivity, mat->absorption * scale, hit.frac_in * 8192.0f );
		}
		else VectorMA( snd.origin, 256.0f, snd.forward, source );
	}

	srt.traces = 0;
	start = Platform_DoubleTime();
	SRT_Evaluate( source, snd.origin, false, &p );
	Con_Printf( "source (%.0f %.0f %.0f): %d traces in %.3f ms\n", source[0], source[1], source[2], srt.traces, ( Platform_DoubleTime() - start ) * 1000.0 );
	SRT_PrintParams( "probe", &p );

	if( srt.listener_env_valid )
		SRT_PrintParams( "listener", &srt.listener_env );
}

static void SRT_Info_f( void )
{
	Con_Printf( "raytraced audio %s, %d rays x %d bounces, last second: %d evaluations, %d traces, %.2f ms\n",
		SND_RT_Active() ? "on" : "off", srt.numdirs ? srt.numdirs : (int)snd_rt_rays.value, (int)snd_rt_bounces.value,
		srt.last_evals, srt.last_traces, srt.last_ms );
	Con_Printf( "%d texture materials from " DEFAULT_SOUNDPATH "materials.txt\n", srt.numtexmats );

	if( srt.listener_env_valid )
		SRT_PrintParams( "listener", &srt.listener_env );

	// playing channels, so it is visible which sounds go through the mixer
	for( int i = NUM_AMBIENTS; i < snd.total_channels; i++ )
	{
		const channel_t *ch = &snd.channels[i];
		const srt_chan_t *s = &srt.chans[i];
		const wavdata_t *sc;
		qboolean self, eligible;

		if( !ch->sfx || !( sc = ch->sfx->cache ))
			continue;

		eligible = SRT_ChannelEligible( ch, &self );

		Con_Printf( "  ch %3d ent %4d chan %3d %s%-40s %2db %5dHz %dch pos %6d/%6d vol %3d/%3d %s",
			i, ch->entnum, ch->entchannel, FBitSet( ch->flags, FL_CHAN_FORCE_LOOP ) ? "LOOP " : "",
			ch->sfx->name, sc->width * 8, sc->rate, sc->channels, (int)ch->sample, sc->samples, ch->leftvol, ch->rightvol,
			!eligible ? "dry" : self ? "self" : "world" );

		if( eligible && s->valid )
			Con_Printf( " rt60 %.2f sends %.2f %.2f %.2f %.2f occ %.2f\n", s->target.rt60,
				s->cur.send_gain[0], s->cur.send_gain[1], s->cur.send_gain[2], s->cur.send_gain[3], s->target.occlusion );
		else Con_Printf( "%s\n", eligible ? " (pending)" : "" );
	}
}

static void SRT_Material_f( void )
{
	int idx;

	if( !srt.materials_loaded )
		SRT_LoadMaterials();

	if( Cmd_Argc() < 2 )
	{
		Con_Printf( S_USAGE "snd_rt_material <letter|default|fence> [reflectivity] [absorption]\n" );
		for( int i = 0; i < SRT_MAT_COUNT; i++ )
		{
			char key = i < 26 ? 'A' + i : ' ';
			Con_Printf( "  %c %-9s reflectivity %.2f absorption %.2f\n", key, srt_materials[i].name, srt_materials[i].reflectivity, srt_materials[i].absorption );
		}
		return;
	}

	if( !Q_stricmp( Cmd_Argv( 1 ), "default" )) idx = SRT_MAT_DEFAULT;
	else if( !Q_stricmp( Cmd_Argv( 1 ), "fence" )) idx = SRT_MAT_FENCE;
	else idx = SRT_MaterialIndexForLetter( Cmd_Argv( 1 )[0] );

	if( Cmd_Argc() >= 3 ) srt_materials[idx].reflectivity = Q_atof( Cmd_Argv( 2 ));
	if( Cmd_Argc() >= 4 ) srt_materials[idx].absorption = Q_atof( Cmd_Argv( 3 ));

	memset( srt.cache, 0, sizeof( srt.cache ));
	Con_Printf( "%s: reflectivity %.2f absorption %.2f\n", srt_materials[idx].name, srt_materials[idx].reflectivity, srt_materials[idx].absorption );
}

static void SRT_Reload_f( void )
{
	SRT_LoadMaterials();
	memset( srt.cache, 0, sizeof( srt.cache ));
	Con_Printf( "raytraced audio: reloaded materials (%d textures)\n", srt.numtexmats );
}

void SND_RT_DebugOverlay( void )
{
	con_nprint_t info =
	{
		.color = { 0.5f, 0.9f, 1.0f },
		.index = 0,
		.time_to_live = 0.5f,
	};

	if( !snd_rt_debug.value || !SND_RT_Active( ))
		return;

	Con_NXPrintf( &info, "snd_rt: %d evals/s, %d traces/s, %.1f ms/s | listener rt60 %.2fs sends %.2f %.2f %.2f %.2f\n",
		srt.last_evals, srt.last_traces, srt.last_ms, srt.listener_env.rt60,
		srt.listener_env.send_gain[0], srt.listener_env.send_gain[1], srt.listener_env.send_gain[2], srt.listener_env.send_gain[3] );
	info.index++;

	for( int i = NUM_AMBIENTS; i < snd.total_channels && info.index < 28; i++ )
	{
		const channel_t *ch = &snd.channels[i];
		const srt_chan_t *s = &srt.chans[i];

		if( !ch->sfx || !s->valid || ( !ch->leftvol && !ch->rightvol ))
			continue;

		Con_NXPrintf( &info, "%-28.28s rt %.2f dg %.2f dc %.2f s %.2f %.2f %.2f %.2f occ %.2f air %4.1f%s\n",
			ch->sfx->name, s->target.rt60, s->cur.direct_gain, s->cur.direct_cutoff,
			s->cur.send_gain[0], s->cur.send_gain[1], s->cur.send_gain[2], s->cur.send_gain[3],
			s->target.occlusion, s->target.airspace, s->fallback ? " ~" : "" );
		info.index++;
	}
}

/*
===============================================================================

INIT

===============================================================================
*/
void SND_RT_Init( void )
{
	Cvar_RegisterVariable( &snd_rt );
	Cvar_RegisterVariable( &snd_rt_rays );
	Cvar_RegisterVariable( &snd_rt_bounces );
	Cvar_RegisterVariable( &snd_rt_maxdist );
	Cvar_RegisterVariable( &snd_rt_units_per_meter );
	Cvar_RegisterVariable( &snd_rt_reverb_gain );
	Cvar_RegisterVariable( &snd_rt_reverb_brightness );
	Cvar_RegisterVariable( &snd_rt_reflectivity );
	Cvar_RegisterVariable( &snd_rt_decay_scale );
	Cvar_RegisterVariable( &snd_rt_occlusion );
	Cvar_RegisterVariable( &snd_rt_occlusion_unit );
	Cvar_RegisterVariable( &snd_rt_occlusion_variation );
	Cvar_RegisterVariable( &snd_rt_max_occlusion );
	Cvar_RegisterVariable( &snd_rt_airspace );
	Cvar_RegisterVariable( &snd_rt_brush_entities );
	Cvar_RegisterVariable( &snd_rt_refresh );
	Cvar_RegisterVariable( &snd_rt_refresh_static );
	Cvar_RegisterVariable( &snd_rt_budget );
	Cvar_RegisterVariable( &snd_rt_smoothing );
	Cvar_RegisterVariable( &snd_rt_underwater );
	Cvar_RegisterVariable( &snd_rt_source_lift );
	Cvar_RegisterVariable( &snd_rt_debug );
	Cvar_RegisterVariable( &snd_rt_slot0 );
	Cvar_RegisterVariable( &snd_rt_slot1 );
	Cvar_RegisterVariable( &snd_rt_slot2 );
	Cvar_RegisterVariable( &snd_rt_slot3 );

	srt_slot_cvars[0] = &snd_rt_slot0;
	srt_slot_cvars[1] = &snd_rt_slot1;
	srt_slot_cvars[2] = &snd_rt_slot2;
	srt_slot_cvars[3] = &snd_rt_slot3;

	Cmd_AddCommand( "snd_rt_probe", SRT_Probe_f, "raytraced audio: evaluate a sound where you look (or at x y z) and print its parameters" );
	Cmd_AddCommand( "snd_rt_info", SRT_Info_f, "raytraced audio: print status and the listener environment" );
	Cmd_AddCommand( "snd_rt_material", SRT_Material_f, "raytraced audio: list or set material reflectivity/absorption" );
	Cmd_AddCommand( "snd_rt_reload", SRT_Reload_f, "raytraced audio: reload sound/materials.txt and sound/snd_rt_materials.txt" );

	memcpy( srt_materials, srt_material_defaults, sizeof( srt_materials ));
	srt.initialized = true;
}

void SND_RT_Shutdown( void )
{
	Cmd_RemoveCommand( "snd_rt_probe" );
	Cmd_RemoveCommand( "snd_rt_info" );
	Cmd_RemoveCommand( "snd_rt_material" );
	Cmd_RemoveCommand( "snd_rt_reload" );

	SRT_FreeDSP();
	Mem_Free2( &srt.texmats );
	srt.numtexmats = 0;
	srt.materials_loaded = false;
	srt.initialized = false;
}

// new map or sound restart: drop stale state
void SND_RT_ClearState( void )
{
	memset( srt.chans, 0, sizeof( srt.chans ));
	memset( srt.cache, 0, sizeof( srt.cache ));
	srt.listener_env_valid = false;
	srt.texcache_world = NULL;

	for( int k = 0; k < SRT_SLOTS; k++ )
	{
		srt_reverb_t *rv = &srt.reverb[k];

		if( rv->predelay.buf ) memset( rv->predelay.buf, 0, sizeof( float ) * rv->predelay.size );
		for( int i = 0; i < SRT_ALLPASSES; i++ )
			if( rv->allpass[i].buf ) memset( rv->allpass[i].buf, 0, sizeof( float ) * rv->allpass[i].size );
		for( int i = 0; i < SRT_FDN_LINES; i++ )
		{
			if( rv->line[i].buf ) memset( rv->line[i].buf, 0, sizeof( float ) * rv->line[i].size );
			rv->line_lp[i] = 0.0f;
		}
	}
}
