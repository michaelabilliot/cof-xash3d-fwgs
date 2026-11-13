// snd_dma.c -- main control for any streaming sound output device

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "host.h"
#include "net.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "gl_model.h"
#include "sound.h"
#include "vgui_int.h"
#include "winquake.h"

#include <mmsystem.h>

void S_Play( void );
void S_PlayVol( void );
void S_Say( void );
void S_Say_Reliable( void );
void S_SoundList( void );
void S_StopAllSoundsC( void );
void S_SoundInfo_f( void );
void S_Update_( void );
void S_StopAllSounds( qboolean clear );

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t	channels[MAX_CHANNELS];
wavstream_t	wavstreams[CWAVSTREAMS];	// one per channel: streaming wav data (files)
int			total_channels;

int			snd_blocked = 0;
static qboolean	snd_ambient = 1;
qboolean	snd_initialized = false;

// pointer should go away
volatile dma_t	*shm = 0;
volatile dma_t	sn;

vec3_t		listener_origin;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
vec_t		sound_nominal_clip_dist = 1000.0;

int			soundtime;		// sample PAIRS
int			paintedtime;	// sample PAIRS

#define	MAX_SFX		1024
sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
int			num_sfx;

sfx_t		*ambient_sfx[NUM_AMBIENTS];

int			desired_speed = 11025;
int			desired_bits = 16;

int			sound_started = 0;

cvar_t	s_show = { "s_show", "0" };
cvar_t	a3d = { "s_a3d", "0", FCVAR_ARCHIVE };
cvar_t	s_eax = { "s_eax", "0", FCVAR_ARCHIVE };
cvar_t	bgmvolume = { "bgmvolume", "1", FCVAR_ARCHIVE };
cvar_t	MP3Volume = { "MP3Volume", "0.8", FCVAR_ARCHIVE };
cvar_t	MP3FadeTime = { "MP3FadeTime", "2.0", FCVAR_ARCHIVE };
cvar_t	volume = { "volume", "0.7", FCVAR_ARCHIVE };
cvar_t	suitvolume = { "suitvolume", "0.25", FCVAR_ARCHIVE };
cvar_t	hisound = { "hisound", "1.0", FCVAR_ARCHIVE };

cvar_t	nosound = { "nosound", "0" };
cvar_t	loadas8bit = { "loadas8bit", "0" };
cvar_t	bgmbuffer = { "bgmbuffer", "4096" };
cvar_t	ambient_level = { "ambient_level", "0.3" };
cvar_t	ambient_fade = { "ambient_fade", "100" };
cvar_t	snd_noextraupdate = { "snd_noextraupdate", "0" };
cvar_t	snd_show = { "snd_show", "0", FCVAR_SPONLY };
cvar_t	_snd_mixahead = { "_snd_mixahead", "0.1", FCVAR_ARCHIVE };

// A3D settings
cvar_t	s_buffersize = { "s_buffersize", "65536" };
cvar_t	s_rolloff = { "s_rolloff", "1.0", FCVAR_ARCHIVE };
cvar_t	s_doppler = { "s_doppler", "0.0", FCVAR_ARCHIVE };
cvar_t	s_distance = { "s_distance", "18", FCVAR_ARCHIVE };
cvar_t	s_automin_distance = { "s_automin_distance", "2.0", FCVAR_ARCHIVE };
cvar_t	s_automax_distance = { "s_automax_distance", "30.0", FCVAR_ARCHIVE };
cvar_t	s_min_distance = { "s_min_distance", "8.0", FCVAR_ARCHIVE };
cvar_t	s_max_distance = { "s_max_distance", "1000.0", FCVAR_ARCHIVE };
cvar_t	s_2dvolume = { "s_2dvolume", "0.88" };
cvar_t	s_geometry = { "s_geometry", "1" };
cvar_t	s_leafnum = { "s_leafnum", "0", FCVAR_ARCHIVE };
cvar_t	s_blipdir = { "s_blipdir", "off" };
cvar_t	s_refgain = { "s_refgain", "0.34", FCVAR_ARCHIVE };
cvar_t	s_refdelay = { "s_refdelay", "4", FCVAR_ARCHIVE };
cvar_t	s_polykeep = { "s_polykeep", "1000000000", FCVAR_ARCHIVE };
cvar_t	s_polysize = { "s_polysize", "10000000", FCVAR_ARCHIVE };
cvar_t	s_showtossed = { "s_showtossed", "0" };
cvar_t	s_numpolys = { "s_numpolys", "200", FCVAR_ARCHIVE };
cvar_t	s_usepvs = { "s_usepvs", "1" };
cvar_t	s_verbwet = { "s_verbwet", "0.15", FCVAR_ARCHIVE };
cvar_t	s_bloat = { "s_bloat", "2.0", FCVAR_ARCHIVE };
cvar_t	s_reverb = { "s_reverb", "1" };
cvar_t	s_occlude = { "s_occlude", "1" };
cvar_t	s_reflect = { "s_reflect", "1" };
cvar_t	s_materials = { "s_materials", "0" };
cvar_t	s_occfactor = { "s_occfactor", "0.25" };
cvar_t	s_occ_epsilon = { "s_occ_epsilon", "1.0" };

// ====================================================================
// User-setable variables
// ====================================================================

//
// Fake dma is a synchronous faking of the DMA progress used for
// isolating performance in the renderer.  The fakedma_updates is
// number of times S_Update() is called per second.
//

qboolean	fakedma = false;
int			fakedma_updates = 15;


void S_AmbientOff( void )
{
	snd_ambient = false;
}


void S_AmbientOn( void )
{
	snd_ambient = true;
}


void S_SoundInfo_f( void )
{
	if ( !sound_started || !shm )
	{
		Con_Printf( "sound system not started\n" );
		return;
	}

	Con_Printf( "%5d stereo\n", shm->channels - 1 );
	Con_Printf( "%5d samples\n", shm->samples );
	Con_Printf( "%5d samplepos\n", shm->samplepos );
	Con_Printf( "%5d samplebits\n", shm->samplebits );
	Con_Printf( "%5d submission_chunk\n", shm->submission_chunk );
	Con_Printf( "%5d speed\n", shm->speed );
	Con_Printf( "0x%x dma buffer\n", shm->buffer );
	Con_Printf( "%5d total_channels\n", total_channels );
}


/*
================
S_Startup
================
*/
void S_Startup( void )
{
	int		rc;

	if ( !snd_initialized )
		return;

	if ( !fakedma )
	{
		rc = SNDDMA_Init();

		if ( !rc )
		{
			Con_Printf( "S_Startup: SNDDMA_Init failed.\n" );
			sound_started = 0;
			return;
		}
	}

	sound_started = 1;
}


/*
================
S_QuerySoundSetting

Reads one setting out of config.cfg before the cvars are set from it
================
*/
qboolean S_QuerySoundSetting( const char *pszCvar, qboolean bDefault )
{
	char		szFile[MAX_PATH];
	char		*pbuffer, *pstart;
	qboolean	bResult = bDefault;

	_snprintf( szFile, sizeof( szFile ), "config.cfg" );

	pbuffer = COM_LoadFile( szFile, 5, NULL );
	if ( !pbuffer )
		return bDefault;

	pstart = pbuffer;
	while ( 1 )
	{
		pstart = COM_Parse( pstart );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		if ( !Q_stricmp( com_token, "bind" ) )
		{
			pstart = COM_Parse( pstart );
			pstart = COM_Parse( pstart );
			continue;
		}

		if ( com_token[0] == '+' )
			continue;

		if ( !Q_stricmp( com_token, pszCvar ) )
		{
			pstart = COM_Parse( pstart );
			bResult = atoi( com_token ) ? true : false;
			break;
		}

		pstart = COM_Parse( pstart );
	}

	Mem_Free( pbuffer );
	return bResult;
}


/*
================
S_Init
================
*/
void S_Init( void )
{
	qboolean	fUseA3D_guess;
	qboolean	fUseEAX_guess;

	Con_DPrintf( "Sound Initialization\n" );

	VOX_Init();

	if ( COM_CheckParm( "-nosound" ) )
		return;

	if ( COM_CheckParm( "-simsound" ) )
		fakedma = true;

	Cmd_AddCommand( "play", S_Play );
	Cmd_AddCommand( "playvol", S_PlayVol );
	Cmd_AddCommand( "speak", S_Say );
	Cmd_AddCommand( "spk", S_Say_Reliable );
	Cmd_AddCommand( "stopsound", S_StopAllSoundsC );
	Cmd_AddCommand( "soundlist", S_SoundList );
	Cmd_AddCommand( "soundinfo", S_SoundInfo_f );
	Cmd_AddCommand( "s_enable_a3d", S_EnableA3D_guess );
	Cmd_AddCommand( "s_disable_a3d", S_DisableA3D_guess );

	Cvar_RegisterVariable( &s_buffersize );
	Cvar_RegisterVariable( &s_rolloff );
	Cvar_RegisterVariable( &s_doppler );
	Cvar_RegisterVariable( &s_distance );
	Cvar_RegisterVariable( &s_automin_distance );
	Cvar_RegisterVariable( &s_automax_distance );
	Cvar_RegisterVariable( &s_min_distance );
	Cvar_RegisterVariable( &s_max_distance );
	Cvar_RegisterVariable( &s_2dvolume );
	Cvar_RegisterVariable( &s_geometry );
	Cvar_RegisterVariable( &s_leafnum );
	Cvar_RegisterVariable( &s_blipdir );
	Cvar_RegisterVariable( &s_refgain );
	Cvar_RegisterVariable( &s_refdelay );
	Cvar_RegisterVariable( &s_polykeep );
	Cvar_RegisterVariable( &s_polysize );
	Cvar_RegisterVariable( &s_numpolys );
	Cvar_RegisterVariable( &s_showtossed );
	Cvar_RegisterVariable( &s_usepvs );
	Cvar_RegisterVariable( &s_bloat );
	Cvar_RegisterVariable( &s_verbwet );
	Cvar_RegisterVariable( &s_reverb );
	Cvar_RegisterVariable( &s_occlude );
	Cvar_RegisterVariable( &s_reflect );
	Cvar_RegisterVariable( &s_materials );
	Cvar_RegisterVariable( &s_occfactor );
	Cvar_RegisterVariable( &s_occ_epsilon );
	Cvar_RegisterVariable( &s_show );
	Cvar_RegisterVariable( &a3d );
	Cvar_RegisterVariable( &s_eax );
	Cvar_RegisterVariable( &nosound );
	Cvar_RegisterVariable( &volume );
	Cvar_RegisterVariable( &suitvolume );
	Cvar_RegisterVariable( &hisound );
	Cvar_RegisterVariable( &loadas8bit );
	Cvar_RegisterVariable( &bgmvolume );
	Cvar_RegisterVariable( &MP3Volume );
	Cvar_RegisterVariable( &MP3FadeTime );
	Cvar_RegisterVariable( &bgmbuffer );
	Cvar_RegisterVariable( &ambient_level );
	Cvar_RegisterVariable( &ambient_fade );
	Cvar_RegisterVariable( &snd_noextraupdate );
	Cvar_RegisterVariable( &snd_show );
	Cvar_RegisterVariable( &_snd_mixahead );

	if ( host_parms.memsize < 0x800000 )
	{
		Cvar_Set( "loadas8bit", "1" );
		Con_DPrintf( "loading all sounds as 8bit\n" );
	}

	fUseA3D_guess = S_QuerySoundSetting( "s_a3d", false );
	fUseEAX_guess = S_QuerySoundSetting( "s_eax", false );

	Cvar_SetValue( "s_a3d", (float)( fUseA3D_guess ? 1.0 : 0.0 ) );
	Cvar_SetValue( "s_eax", (float)( fUseEAX_guess ? 1.0 : 0.0 ) );

	snd_initialized = true;

	S_Startup();

	SND_InitScaletable();

	known_sfx = Hunk_AllocName( MAX_SFX * sizeof( sfx_t ), "sfx_t" );
	num_sfx = 0;

	// create a piece of DMA memory
	if ( fakedma )
	{
		shm = (void *)Hunk_AllocName( sizeof( *shm ), "shm" );
		shm->splitbuffer = 0;
		shm->samplebits = 16;
		shm->speed = 22050;
		shm->channels = 2;
		shm->samples = 32768;
		shm->samplepos = 0;
		shm->soundalive = true;
		shm->gamealive = true;
		shm->submission_chunk = 1;
		shm->buffer = Hunk_AllocName( 1 << 16, "shmbuf" );

		Con_DPrintf( "Sound sampling rate: %i\n", shm->speed );
	}

	S_StopAllSounds( true );

	SX_Init();

	Wavstream_Init();
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown( void )
{
	VOX_Shutdown();

	if ( !sound_started )
		return;

	if ( shm )
		shm->gamealive = 0;

	shm = 0;
	sound_started = 0;

	if ( !fakedma )
	{
		SNDDMA_Shutdown();
	}
}


// =======================================================================
// Load a sound
// =======================================================================

/*
==================
S_FindName

return a pointer to the sfx struct; when pfInCache is given, it is set
to whether the sound data is in the cache
==================
*/
sfx_t *S_FindName( char *name, int *pfInCache )
{
	int		i;
	sfx_t	*sfx = NULL;

	if ( !name )
		Sys_Error( "S_FindName: NULL\n" );

	if ( Q_strlen( name ) >= MAX_QPATH )
		Sys_Error( "Sound name too long: %s", name );

	// see if already loaded
	for ( i = 0; i < num_sfx; i++ )
	{
		if ( !Q_strcasecmp( known_sfx[i].name, name ) )
		{
			if ( pfInCache )
			{
				// indicate whether or not sound is currently in the cache.
				if ( !Cache_Check( &known_sfx[i].cache ) )
					*pfInCache = 0;
				else
					*pfInCache = 1;
			}

			if ( known_sfx[i].servercount > 0 )
				known_sfx[i].servercount = cl.servercount;

			return &known_sfx[i];
		}

		if ( !sfx && known_sfx[i].servercount > 0 && known_sfx[i].servercount != cl.servercount )
			sfx = &known_sfx[i];
	}

	if ( !sfx )
	{
		if ( num_sfx == MAX_SFX )
			Sys_Error( "S_FindName: out of sfx_t" );

		sfx = &known_sfx[i];
		num_sfx++;
	}
	else
	{
		// free the cache if it's not already
		if ( Cache_Check( &sfx->cache ) )
			Cache_Free( &sfx->cache );
	}

	Q_strncpy( sfx->name, name, sizeof( sfx->name ) - 1 );
	sfx->name[sizeof( sfx->name ) - 1] = 0;

	if ( pfInCache )
		*pfInCache = 0;

	sfx->servercount = cl.servercount;
	return sfx;
}


/*
==================
S_FlushSoundData

free the cache of every sound that no stream is playing: the low
resolution ones, the high resolution ones or both
==================
*/
void S_FlushSoundData( int fDumpLores, int fDumpHires )
{
	int			i, j;
	sfxcache_t	*sc;
	channel_t	*ch = channels;
	int			fNoDiscard = 0;

	for ( i = 0; i < num_sfx; i++ )
	{
		sc = Cache_Check( &known_sfx[i].cache );
		if ( !sc )
			continue;

		// never discard a streaming sound that is playing
		for ( j = 0; j < total_channels; j++, ch++ )
		{
			if ( !ch->sfx )
				continue;

			if ( ch->entchannel != CHAN_STREAM )
				continue;

			if ( ch->sfx == &known_sfx[i] )
			{
				fNoDiscard = 1;
				break;
			}
		}

		if ( fNoDiscard )
		{
			fNoDiscard = 0;
			continue;
		}

		if ( fDumpLores && sc->speed <= shm->speed )
			Cache_Free( &known_sfx[i].cache );

		if ( fDumpHires && sc->speed > shm->speed )
			Cache_Free( &known_sfx[i].cache );
	}
}


/*
==================
S_TouchSound

==================
*/
void S_TouchSound( char *name )
{
	sfx_t	*sfx;

	if ( !sound_started )
		return;

	sfx = S_FindName( name, NULL );
	Cache_Check( &sfx->cache );
}


/*
==================
S_PrecacheSound

Reserve space for the name of the sound in a global array.
Load the data for the non-streaming sound. Streaming sounds
defer loading of data until just before playback.
==================
*/
sfx_t *S_PrecacheSound( char *name )
{
	sfx_t	*sfx;

	if ( !sound_started || nosound.value )
		return NULL;

	if ( Q_strlen( name ) >= MAX_QPATH )
		return NULL;

	if ( name[0] == CHAR_STREAM || name[0] == CHAR_SENTENCE )
	{
		// This is a streaming sound or a sentence name.
		// don't actually precache data, just store name

		sfx = S_FindName( name, NULL );
		return sfx;
	}
	else
	{
		// regular sound
		sfx = S_FindName( name, NULL );

		// cache it in
		if ( !fs_lazy_precache.value )
			S_LoadSound( sfx, NULL );

		return sfx;
	}
}


//=============================================================================

/*
=================
SND_FStreamIsPlaying

Scan channels looking for a stream that is playing this sound
=================
*/
int SND_FStreamIsPlaying( sfx_t *sfx )
{
	int ch_idx;

	for ( ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++ )
	{
		if ( channels[ch_idx].sfx == sfx )
			return 1;
	}

	return 0;
}


/*
=================
SND_PickDynamicChannel

Select a channel from the dynamic channel allocation area.  For the given entity,
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_PickDynamicChannel( int entnum, int entchannel, int fstatic_guess, sfx_t *sfx )
{
	int		ch_idx;
	int		first_to_die;
	int		life_left;
	int		max_ch_idx_guess;
	int		free_slot_guess;

	// Check for replacement sound, or find the best one to replace
	if ( entchannel == CHAN_STREAM && SND_FStreamIsPlaying( sfx ) )
		return NULL;

	free_slot_guess = -1;
	first_to_die = -1;
	life_left = 0x7fffffff;
	ch_idx = NUM_AMBIENTS;

	if ( !snd_isa3d_guess )
	{
		ch_idx = NUM_AMBIENTS;
	}
	else
	{
		ch_idx = NUM_AMBIENTS;
	}

	max_ch_idx_guess = hwGetNumChannels_guess() + NUM_AMBIENTS;

	for ( ; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++ )
	{
		// Never override a streaming sound that is currently playing or
		// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
		if ( channels[ch_idx].entchannel == CHAN_STREAM && wavstreams[ch_idx].hFile )
		{
			if ( entchannel == CHAN_VOICE )
				return NULL;

			continue;
		}

		// Channel 0 never overrides
		if ( ( entchannel != 0 && channels[ch_idx].entnum == entnum && channels[ch_idx].entchannel == entchannel ) || entchannel == -1 )
		{
			// always override sound from same entity
			if ( snd_isa3d_guess && ch_idx < max_ch_idx_guess )
				free_slot_guess = ch_idx;
			else
				free_slot_guess = -1;

			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if ( channels[ch_idx].entnum == cl.playernum + 1 && entnum != cl.playernum + 1 && channels[ch_idx].sfx )
			continue;

		if ( channels[ch_idx].end - paintedtime < life_left )
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;

			if ( !channels[ch_idx].sfx )
				free_slot_guess = ch_idx;
		}
	}

	if ( first_to_die == -1 )
		return NULL;

	if ( snd_isa3d_guess && free_slot_guess >= 0 )
		first_to_die = free_slot_guess;

	if ( channels[first_to_die].sfx )
	{
		// be sure and release previous channel
		// if sentence.
		S_FreeChannel( &channels[first_to_die] );
		channels[first_to_die].sfx = NULL;
	}

	return &channels[first_to_die];
}


/*
=====================
SND_PickStaticChannel

Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if
we're trying to allocate a channel for a stream sound that is
already playing.
=====================
*/
channel_t *SND_PickStaticChannel( int entnum, int entchannel, int fstatic_guess, sfx_t *sfx )
{
	int i;
	channel_t *ch = NULL;

	// Check for replacement sound, or find the best one to replace
	if ( sfx->name[0] == CHAR_STREAM && SND_FStreamIsPlaying( sfx ) )
		return NULL;

	for ( i = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; i < total_channels; i++ )
	{
		if ( channels[i].sfx == NULL )
			break;
	}

	if ( i < total_channels )
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if ( total_channels == MAX_CHANNELS )
		{
			Con_DPrintf( "total_channels == MAX_CHANNELS\n" );
			return NULL;
		}

		// get a channel for the static sound
		ch = &channels[total_channels];
		total_channels++;
	}

	return ch;
}


/*
=================
SND_Spatialize

Spatialize a sound, calculating the left and right volumes (and, for the
four speaker output, the rear ones)
=================
*/
void SND_Spatialize( channel_t *ch )
{
	vec_t	dot;
	vec_t	rfwdscale_guess;
	vec_t	dist;
	vec_t	lscale, rscale, scale;
	vec3_t	source_vec;
	sfx_t	*snd;
	cl_entity_t	*pent;
	vec_t	lfwdscale_guess;
	vec_t	dot_front_guess;
	vec_t	lrearscale_guess;
	vec_t	rbackscale_guess;

	// anything coming from the view entity will always be full volume
	if ( ch->entnum == cl.viewentity )
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		VOX_SetChanVol( ch );
		return;
	}

	// Make sure index is valid
	if ( ch->entnum > 0 && ch->entnum < cl.num_entities )
	{
		pent = &cl_entities[ch->entnum];

		if ( pent && pent->model && pent->curstate.messagenum == cl.parsecount )
		{
			VectorCopy( pent->origin, ch->origin );

			if ( pent->model->type == mod_brush )
			{
				// Mobile brushes have no origin, need to use the center of the bounding box
				ch->origin[0] += ( pent->model->mins[0] + pent->model->maxs[0] ) * 0.5;
				ch->origin[1] += ( pent->model->mins[1] + pent->model->maxs[1] ) * 0.5;
				ch->origin[2] += ( pent->model->mins[2] + pent->model->maxs[2] ) * 0.5;
			}
		}
	}

	// calculate stereo seperation and distance attenuation
	snd = ch->sfx;
	VectorSubtract( ch->origin, listener_origin, source_vec );

	if ( snd_isa3d_guess )
		dist = VectorNormalize( source_vec ) * ch->dist_mult * 0.5;
	else
		dist = VectorNormalize( source_vec ) * ch->dist_mult;

	dot = DotProduct( listener_right, source_vec );

	if ( shm && shm->channels == 1 )
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	if ( snd_iseax_guess )
	{
		dot_front_guess = DotProduct( listener_forward, source_vec );

		rfwdscale_guess = rscale * ( dot_front_guess + 1.0 ) / 2.0;
		rbackscale_guess = rscale * ( 1.0 - dot_front_guess ) / 2.0;
		lfwdscale_guess = lscale * ( dot_front_guess + 1.0 ) / 2.0;
		lrearscale_guess = lscale * ( 1.0 - dot_front_guess ) / 2.0;

		rscale = rfwdscale_guess;
		lscale = lfwdscale_guess;
	}

	// add in distance effect
	scale = ( 1.0 - dist ) * rscale;
	ch->rightvol = (int)( ch->master_vol * scale );

	scale = ( 1.0 - dist ) * lscale;
	ch->leftvol = (int)( ch->master_vol * scale );

	if ( snd_iseax_guess )
	{
		scale = ( 1.0 - dist ) * rbackscale_guess;
		ch->rrightvol_guess = (int)( ch->master_vol * scale );

		scale = ( 1.0 - dist ) * lrearscale_guess;
		ch->rleftvol_guess = (int)( ch->master_vol * scale );
	}

	// if playing a word, set volume
	VOX_SetChanVol( ch );

	if ( ch->rightvol < 0 )
		ch->rightvol = 0;
	if ( ch->leftvol < 0 )
		ch->leftvol = 0;

	if ( snd_iseax_guess )
	{
		if ( ch->rrightvol_guess < 0 )
			ch->rrightvol_guess = 0;
		if ( ch->rleftvol_guess < 0 )
			ch->rleftvol_guess = 0;
	}
}


// search through all channels for a channel that matches this
// entnum, entchannel and sfx, and perform alteration on channel
// as indicated by 'flags' parameter. If shut down request and
// sfx contains a sentence name, shut off the sentence.
// returns TRUE if sound was altered,
// returns FALSE if sound was not found (sound is not playing)

int S_AlterChannel( int entnum, int entchannel, sfx_t *sfx, int vol, int pitch, int flags )
{
	int ch_idx;

	if ( sfx->name[0] == CHAR_SENTENCE )
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isentence >= 0 and matches the
		// soundsource.

		for ( ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++ )
		{
			if ( channels[ch_idx].entnum == entnum
				&& channels[ch_idx].entchannel == entchannel
				&& channels[ch_idx].sfx != NULL
				&& channels[ch_idx].isentence >= 0 )
			{
				if ( flags & SND_CHANGE_PITCH )
					channels[ch_idx].pitch = pitch;

				if ( flags & SND_CHANGE_VOL )
					channels[ch_idx].master_vol = vol;

				if ( flags & SND_STOP )
					S_FreeChannel( &channels[ch_idx] );

				return TRUE;
			}
		}
		// channel not found
		return FALSE;
	}

	// regular sound or streaming sound

	for ( ch_idx = NUM_AMBIENTS; ch_idx < total_channels; ch_idx++ )
	{
		if ( channels[ch_idx].entnum == entnum
			&& channels[ch_idx].entchannel == entchannel
			&& channels[ch_idx].sfx == sfx )
		{
			if ( flags & SND_CHANGE_PITCH )
				channels[ch_idx].pitch = pitch;

			if ( flags & SND_CHANGE_VOL )
				channels[ch_idx].master_vol = vol;

			if ( flags & SND_STOP )
				S_FreeChannel( &channels[ch_idx] );

			return TRUE;
		}
	}

	return FALSE;
}


// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartDynamicSound( int entnum, int entchannel, sfx_t *sfx, vec_t *origin, float fvol, float attenuation, int flags, int pitch )
{
	channel_t	*target_chan, *check;
	sfxcache_t	*sc;
	int			vol;
	int			ch_idx;
	int			skip;
	int			fsentence = 0;

	if ( !sound_started )
		return;

	if ( !sfx )
		return;

	if ( nosound.value )
		return;

	if ( sfx->name[0] == CHAR_STREAM )
		entchannel = CHAN_STREAM;

	if ( entchannel == CHAN_STREAM && pitch != PITCH_NORM )
	{
		Con_DPrintf( "Warning: pitch shift ignored on stream sound %s\n", sfx->name );
		pitch = PITCH_NORM;
	}

	vol = fvol * 255;

	if ( vol > 255 )
	{
		Con_DPrintf( "S_StartDynamicSound: %s volume > 255", sfx->name );
		vol = 255;
	}

	if ( ( flags & SND_STOP ) || ( flags & SND_CHANGE_VOL ) || ( flags & SND_CHANGE_PITCH ) )
	{
		if ( S_AlterChannel( entnum, entchannel, sfx, vol, pitch, flags ) || ( flags & SND_STOP ) )
			return;

		// fall through - if we're not trying to stop the sound,
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if ( pitch == 0 )
	{
		Con_DPrintf( "Warning: S_StartDynamicSound Ignored, called with pitch 0" );
		return;
	}

	pitch *= ( 1.0f + sys_timescale.value ) * 0.5f;

	// pick a channel to play on
	target_chan = SND_PickDynamicChannel( entnum, entchannel, 0, sfx );

	if ( snd_isa3d_guess && (int)( ( (byte *)target_chan - (byte *)channels ) / sizeof( channel_t ) ) < hwGetNumChannels_guess() + NUM_AMBIENTS )
	{
		int		iChan_guess;
		int		state_guess;
		void	*hvoice_guess;

		iChan_guess = ( (byte *)target_chan - (byte *)channels ) / sizeof( channel_t );

		hvoice_guess = hwAllocChannel_guess( iChan_guess, 1 );
		if ( hvoice_guess )
			hwSetVoiceRelative_guess( hvoice_guess, cl_entities[entnum].curstate.movetype == MOVETYPE_NONE );

		state_guess = hwGetChannelState_guess( iChan_guess );
		switch ( state_guess )
		{
		case 0:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		case 1:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		case 2:
		case 6:
			hwSetChannelState_guess( iChan_guess, 6 );
			break;
		case 3:
			hwSetChannelState_guess( iChan_guess, 7 );
			break;
		default:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		}
	}

	if ( !target_chan )
		return;

	if ( sfx->name[0] == CHAR_SENTENCE || sfx->name[0] == '#' )
		fsentence = 1;

	// spatialize
	Q_memset( target_chan, 0, sizeof( *target_chan ) );
	VectorCopy( origin, target_chan->origin );
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	target_chan->pitch = pitch;
	target_chan->isentence = -1;

	SND_Spatialize( target_chan );

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of
	// range sounds don't fill the playback buffer.  For streaming sounds, we bypass this optimization.
	if ( !target_chan->leftvol && !target_chan->rightvol && entchannel != CHAN_STREAM )
	{
		target_chan->sfx = NULL;
		return;		// not audible at all
	}

	if ( fsentence )
	{
		// this is a sentence
		// link all words and load the first word
		char name[MAX_QPATH];

		Q_strncpy( name, sfx->name + 1, sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;

		sc = VOX_LoadSound( target_chan, name );
	}
	else
	{
		// regular or streamed sound fx
		sc = S_LoadSound( sfx, target_chan );
		target_chan->sfx = sfx;
	}

	if ( !sc )
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->pos = 0;
	target_chan->end = paintedtime + sc->length;

	if ( !fsentence && pitch != PITCH_NORM )
	{
		// if not a sentence, but we're pitch shifting, set up the
		// voxword_t for one word
		VOX_MakeSingleWordSentence( target_chan, pitch );
	}

	VOX_TrimStartEndTimes( target_chan, sc );

	// Init client entity mouth movement vars
	SND_InitMouth( entnum, entchannel );

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder

	check = &channels[NUM_AMBIENTS];
	for ( ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++ )
	{
		if ( check == target_chan )
			continue;

		if ( check->sfx == sfx && !check->pos )
		{
			skip = RandomLong( 0, (long)( shm->speed * 0.1 ) );
			if ( skip >= target_chan->end )
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}


/*
=================
S_StartStaticSound

Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
=================
*/
void S_StartStaticSound( int entnum, int entchannel, sfx_t *sfxin, vec_t *origin, float fvol, float attenuation, int flags, int pitch )
{
	channel_t	*ch;
	sfxcache_t	*sc;
	int			fvox = 0;
	sfx_t		*sfx;
	int			vol;

	sfx = sfxin;

	if ( !sfx )
		return;

	if ( sfxin->name[0] == CHAR_STREAM )
		entchannel = CHAN_STREAM;

	if ( entchannel == CHAN_STREAM && pitch != PITCH_NORM )
	{
		Con_DPrintf( "Warning: pitch shift ignored on stream sound %s\n", sfx->name );
		pitch = PITCH_NORM;
	}

	vol = fvol * 255;

	if ( vol > 255 )
	{
		Con_DPrintf( "S_StartStaticSound: %s volume > 255", sfxin->name );
		vol = 255;
	}

	if ( ( flags & SND_STOP ) || ( flags & SND_CHANGE_VOL ) || ( flags & SND_CHANGE_PITCH ) )
	{
		if ( S_AlterChannel( entnum, entchannel, sfxin, vol, pitch, flags ) || ( flags & SND_STOP ) )
			return;
	}

	if ( pitch == 0 )
	{
		Con_DPrintf( "Warning: S_StartStaticSound Ignored, called with pitch 0" );
		return;
	}

	pitch *= ( 1.0f + sys_timescale.value ) * 0.5f;

	// First, make sure the sound source entity is even in the PVS.
	ch = SND_PickStaticChannel( entnum, entchannel, 1, sfxin );

	if ( snd_isa3d_guess && (int)( ( (byte *)ch - (byte *)channels ) / sizeof( channel_t ) ) < hwGetNumChannels_guess() + NUM_AMBIENTS )
	{
		int		iChan_guess;
		int		state_guess;
		void	*hvoice_guess;

		iChan_guess = ( (byte *)ch - (byte *)channels ) / sizeof( channel_t );

		hvoice_guess = hwAllocChannel_guess( iChan_guess, 1 );
		if ( hvoice_guess )
			hwSetVoiceRelative_guess( hvoice_guess, cl_entities[entnum].curstate.movetype == MOVETYPE_NONE );

		state_guess = hwGetChannelState_guess( iChan_guess );
		switch ( state_guess )
		{
		case 0:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		case 1:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		case 2:
		case 6:
			hwSetChannelState_guess( iChan_guess, 6 );
			break;
		case 3:
			hwSetChannelState_guess( iChan_guess, 7 );
			break;
		default:
			hwSetChannelState_guess( iChan_guess, 5 );
			break;
		}
	}

	if ( !ch )
		return;

	if ( sfx->name[0] == CHAR_SENTENCE || sfx->name[0] == '#' )
	{
		// this is a sentence. link words to play in sequence.

		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'.

		char name[MAX_QPATH];

		Q_strncpy( name, sfx->name + 1, sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;

		// link all words and load the first word
		sc = VOX_LoadSound( ch, name );
		fvox = 1;
	}
	else
	{
		// load regular or stream sound
		sc = S_LoadSound( sfx, ch );
		ch->sfx = sfx;
		ch->isentence = -1;
	}

	if ( !sc )
	{
		ch->sfx = NULL;
		return;
	}

	VectorCopy( origin, ch->origin );
	ch->master_vol = vol;
	ch->dist_mult = attenuation / sound_nominal_clip_dist;
	ch->pos = 0;
	ch->end = paintedtime + sc->length;
	ch->pitch = pitch;
	ch->entnum = entnum;
	ch->entchannel = entchannel;

	if ( !fvox && pitch != PITCH_NORM )
	{
		// if not a sentence, but we're pitch shifting, set up the
		// voxword_t for one word
		VOX_MakeSingleWordSentence( ch, pitch );
	}

	VOX_TrimStartEndTimes( ch, sc );

	SND_Spatialize( ch );
}


/*
==================
S_StopSound

Stop all sounds for entity on a channel.
==================
*/
void S_StopSound( int entnum, int entchannel )
{
	int i;

	for ( i = NUM_AMBIENTS; i < total_channels; i++ )
	{
		if ( channels[i].entnum == entnum && channels[i].entchannel == entchannel )
			S_FreeChannel( &channels[i] );
	}
}


void S_StopAllSounds( qboolean clear )
{
	int i;

	if ( !sound_started )
		return;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( channels[i].sfx )
			S_FreeChannel( &channels[i] );
	}

	Q_memset( channels, 0, MAX_CHANNELS * sizeof( channel_t ) );

	// clear any remaining soundfade
	Wavstream_Init();

	if ( clear )
		S_ClearBuffer();

	if ( snd_isa3d_guess )
		hwStopAllSounds_guess();
}


void S_StopAllSoundsC( void )
{
	S_StopAllSounds( true );
}


void S_ClearBuffer( void )
{
	DWORD	*pAudioRL_guess, *pAudioFL_guess;
	DWORD	dwFR_guess;
	DWORD	dwLengthRR_guess;
	DWORD	*pAudioFR_guess;
	DWORD	*lpDataRR_guess;
	DWORD	dwRL_guess, dwLengthFL_guess;
	int		clear;

	if ( snd_iseax_guess )
	{
		int		reps;
		HRESULT	hresult;

		reps = 0;

		if ( !sound_started || !shm || ( !shm->buffer && !pDSBufFL_guess && !pDSBufFR_guess && !pDSBufRL_guess && !pDSBufRR_guess ) )
			return;

		while ( ( hresult = pDSBufFL_guess->lpVtbl->Lock( pDSBufFL_guess, 0, gSndBufSize / 2, &pAudioFL_guess, &dwLengthFL_guess, NULL, NULL, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock EAX FL Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore EAX FL buffer\n" );
				S_Shutdown();
				return;
			}
		}

		reps = 0;

		while ( ( hresult = pDSBufFR_guess->lpVtbl->Lock( pDSBufFR_guess, 0, gSndBufSize / 2, &pAudioFR_guess, &dwFR_guess, NULL, NULL, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock EAX FR Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore EAX FR buffer\n" );
				S_Shutdown();
				return;
			}
		}

		reps = 0;

		while ( ( hresult = pDSBufRL_guess->lpVtbl->Lock( pDSBufRL_guess, 0, gSndBufSize / 2, &pAudioRL_guess, &dwRL_guess, NULL, NULL, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock EAX RL Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore EAX RL buffer\n" );
				S_Shutdown();
				return;
			}
		}

		reps = 0;

		while ( ( hresult = pDSBufRR_guess->lpVtbl->Lock( pDSBufRR_guess, 0, gSndBufSize / 2, &lpDataRR_guess, &dwLengthRR_guess, NULL, NULL, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock EAX RR Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore EAX RR buffer\n" );
				S_Shutdown();
				return;
			}
		}

		Q_memset( pAudioFL_guess, 0, gSndBufSize / 2 );
		Q_memset( pAudioFR_guess, 0, gSndBufSize / 2 );
		Q_memset( pAudioRL_guess, 0, gSndBufSize / 2 );
		Q_memset( lpDataRR_guess, 0, gSndBufSize / 2 );

		pDSBufFL_guess->lpVtbl->Unlock( pDSBufFL_guess, pAudioFL_guess, dwLengthFL_guess, NULL, 0 );
		pDSBufFR_guess->lpVtbl->Unlock( pDSBufFR_guess, pAudioFR_guess, dwFR_guess, NULL, 0 );
		pDSBufRL_guess->lpVtbl->Unlock( pDSBufRL_guess, pAudioRL_guess, dwRL_guess, NULL, 0 );
		pDSBufRR_guess->lpVtbl->Unlock( pDSBufRR_guess, lpDataRR_guess, dwLengthRR_guess, NULL, 0 );
		return;
	}

	if ( !sound_started || !shm || ( snd_isa3d_guess && !hwIsActive_guess() ) )
		return;

	if ( !snd_isa3d_guess && ( !sound_started || !shm || ( !shm->buffer && !pDSBuf ) ) )
		return;

	if ( shm->samplebits == 8 )
		clear = 0x80;
	else
		clear = 0;

	if ( snd_isa3d_guess && hwIsActive_guess() )
	{
		DWORD	*pData;
		DWORD	dwSize;

		if ( hwLockBuffer_guess( &pData, &dwSize ) < 0 )
			return;

		Q_memset( pData, clear, shm->samples * shm->samplebits / 8 );

		hwUnlockBuffer_guess( pData, dwSize );
	}
	else if ( pDSBuf )
	{
		DWORD	dwSize;
		DWORD	*pData;
		int		reps;
		HRESULT	hresult;

		reps = 0;

		while ( ( hresult = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, &pData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_ClearBuffer: DS::Lock Sound Buffer Failed\n" );
				S_Shutdown();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_ClearBuffer: DS: couldn't restore buffer\n" );
				S_Shutdown();
				return;
			}
		}

		Q_memset( pData, clear, shm->samples * shm->samplebits / 8 );

		pDSBuf->lpVtbl->Unlock( pDSBuf, pData, dwSize, NULL, 0 );
	}
	else
	{
		Q_memset( shm->buffer, clear, shm->samples * shm->samplebits / 8 );
	}
}


//=============================================================================

/*
===================
S_UpdateAmbientSounds
===================
*/
void S_UpdateAmbientSounds( void )
{
	mleaf_t		*l;
	float		vol;
	int			ambient_channel;
	channel_t	*chan;

	if ( !snd_ambient )
		return;

	// calc ambient sound levels
	if ( !cl.worldmodel )
		return;

	l = Mod_PointInLeaf( listener_origin, cl.worldmodel );
	if ( !l || !ambient_level.value )
	{
		for ( ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++ )
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for ( ambient_channel = 0; ambient_channel < NUM_AMBIENTS; ambient_channel++ )
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = ambient_level.value * l->ambient_sound_level[ambient_channel];
		if ( vol < 8 )
			vol = 0;

		// don't adjust volume too fast
		if ( chan->master_vol < vol )
		{
			chan->master_vol += host_frametime * ambient_fade.value;
			if ( chan->master_vol > vol )
				chan->master_vol = vol;
		}
		else if ( chan->master_vol > vol )
		{
			chan->master_vol -= host_frametime * ambient_fade.value;
			if ( chan->master_vol < vol )
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}


/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( vec_t *origin, vec_t *forward, vec_t *right, vec_t *up )
{
	int			i;
	int			total;
	channel_t	*ch;
	channel_t	*combine;

	if ( !sound_started || ( snd_blocked > 0 ) )
		return;

	VectorCopy( origin, listener_origin );
	VectorCopy( forward, listener_forward );
	VectorCopy( right, listener_right );
	VectorCopy( up, listener_up );

	if ( snd_isa3d_guess )
	{
		hwUpdateListener_guess( origin );
		hwSetVolume_guess( volume.value );
		hwSetListenerPosition_guess( origin );
		hwSetListenerOrientation_guess( forward, up );
	}

	// update general area ambient sound sources
	if ( !snd_isa3d_guess )
		S_UpdateAmbientSounds();

	combine = NULL;

	// update spatialization for static and dynamic sounds
	ch = channels + NUM_AMBIENTS;
	for ( i = NUM_AMBIENTS; i < total_channels; i++, ch++ )
	{
		if ( !ch->sfx )
			continue;

		SND_Spatialize( ch );		// respatialize channel
	}

	//
	// debugging output
	//
	if ( snd_show.value )
	{
		total = 0;
		ch = channels;
		for ( i = 0; i < total_channels; i++, ch++ )
		{
			if ( ch->sfx && ( ch->leftvol || ch->rightvol ) )
			{
				Con_Printf( "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name );
				total++;
			}
		}

		Con_Printf( "----(%i)----\n", total );
	}

	// mix some sound
	S_Update_();
}


void GetSoundtime( void )
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;
	int		fullsamples;

	if ( shm )
		fullsamples = shm->samples / shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if ( samplepos < oldsamplepos )
	{
		buffers++;					// buffer wrapped

		if ( paintedtime > 0x40000000 )
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;
			S_StopAllSounds( true );
		}
	}

	oldsamplepos = samplepos;

	if ( shm )
		soundtime = buffers * fullsamples + samplepos / shm->channels;
}


/*
============
S_PrintStats

shows the playing channels in the notify area (s_show)
============
*/
void S_PrintStats( void )
{
	channel_t	*ch;
	int			i;

	if ( !s_show.value || cl.maxclients > 2 )
		return;

	ch = channels;
	for ( i = 0; i < total_channels; i++, ch++ )
	{
		if ( !ch->sfx )
		{
			Con_NPrintf( i & 31, "" );
			continue;
		}

		Con_NPrintf( i & 31, "%s %03i %02i %20s", vstr( ch->origin ), ch->entnum, ch->entchannel, ch->sfx->name );
	}
}


void S_ExtraUpdate( void )
{
	if ( !VGuiWrap2_IsGameUIVisible() )
		ClientDLL_IN_Accumulate();

	if ( snd_noextraupdate.value )
		return;		// don't pollute timings

	S_Update_();
}


void S_Update_( void )
{
	unsigned int	endtime;
	int				samps;
	DWORD			dwStatus;

	if ( !sound_started || ( snd_blocked > 0 ) )
		return;

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if ( paintedtime < soundtime )
	{
		//Con_Printf( "S_Update_ : overflow\n" );
		//paintedtime = soundtime;
	}

	if ( shm )
	{
		// mix ahead of current position
		endtime = soundtime + _snd_mixahead.value * shm->dmaspeed;
		samps = shm->samples >> ( shm->channels - 1 );
	}

	if ( snd_iseax_guess )
		samps += samps >> 1;

	if ( (int)( endtime - soundtime ) > samps )
		endtime = soundtime + samps;

	if ( snd_isa3d_guess )
	{
		SNDDMA_UpdateA3D_guess();
	}
	else if ( snd_iseax_guess )
	{
		// if the buffers were lost or stopped, restore and/or restart them
		if ( pDSBufFL_guess->lpVtbl->GetStatus( pDSBufFL_guess, &dwStatus ) != DS_OK )
			Con_Printf( "Couldn't get EAX FL sound buffer status\n" );

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBufFL_guess->lpVtbl->Restore( pDSBufFL_guess );

		if ( !( dwStatus & DSBSTATUS_PLAYING ) )
			pDSBufFL_guess->lpVtbl->Play( pDSBufFL_guess, 0, 0, DSBPLAY_LOOPING );

		if ( pDSBufFR_guess->lpVtbl->GetStatus( pDSBufFR_guess, &dwStatus ) != DS_OK )
			Con_Printf( "Couldn't get EAX FR sound buffer status\n" );

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBufFR_guess->lpVtbl->Restore( pDSBufFR_guess );

		if ( !( dwStatus & DSBSTATUS_PLAYING ) )
			pDSBufFR_guess->lpVtbl->Play( pDSBufFR_guess, 0, 0, DSBPLAY_LOOPING );

		if ( pDSBufRL_guess->lpVtbl->GetStatus( pDSBufRL_guess, &dwStatus ) != DS_OK )
			Con_Printf( "Couldn't get EAX RL sound buffer status\n" );

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBufRL_guess->lpVtbl->Restore( pDSBufRL_guess );

		if ( !( dwStatus & DSBSTATUS_PLAYING ) )
			pDSBufRL_guess->lpVtbl->Play( pDSBufRL_guess, 0, 0, DSBPLAY_LOOPING );

		if ( pDSBufRR_guess->lpVtbl->GetStatus( pDSBufRR_guess, &dwStatus ) != DS_OK )
			Con_Printf( "Couldn't get EAX RR sound buffer status\n" );

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBufRR_guess->lpVtbl->Restore( pDSBufRR_guess );

		if ( !( dwStatus & DSBSTATUS_PLAYING ) )
			pDSBufRR_guess->lpVtbl->Play( pDSBufRR_guess, 0, 0, DSBPLAY_LOOPING );
	}
	else if ( pDSBuf )
	{
		// if the buffer was lost or stopped, restore it and/or restart it
		if ( pDSBuf->lpVtbl->GetStatus( pDSBuf, &dwStatus ) != DS_OK )
			Con_Printf( "Couldn't get sound buffer status\n" );

		if ( dwStatus & DSBSTATUS_BUFFERLOST )
			pDSBuf->lpVtbl->Restore( pDSBuf );

		if ( !( dwStatus & DSBSTATUS_PLAYING ) )
			pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );
	}

	endtime >>= 1;

	S_PaintChannels( endtime );

	SNDDMA_Submit();
}


/*
===============================================================================

console functions

===============================================================================
*/

void S_Play( void )
{
	static int hash = 345;
	int 	i;
	char	name[256];
	sfx_t	*sfx;

	i = 1;
	while ( i < Cmd_Argc() )
	{
		Q_strncpy( name, Cmd_Argv( i ), sizeof( name ) - 5 );
		name[sizeof( name ) - 5] = 0;

		if ( !Q_strrchr( Cmd_Argv( i ), '.' ) )
			Q_strcat( name, ".wav" );

		sfx = S_PrecacheSound( name );
		S_StartDynamicSound( hash++, CHAN_AUTO, sfx, listener_origin, 1.0, 1.0, 0, PITCH_NORM );
		i++;
	}
}


void S_PlayVol( void )
{
	static int hash = 543;
	int		i;
	float	vol;
	char	name[256];
	sfx_t	*sfx;

	i = 1;
	while ( i < Cmd_Argc() )
	{
		Q_strncpy( name, Cmd_Argv( i ), sizeof( name ) - 5 );
		name[sizeof( name ) - 5] = 0;

		if ( !Q_strrchr( Cmd_Argv( i ), '.' ) )
			Q_strcat( name, ".wav" );

		sfx = S_PrecacheSound( name );
		vol = Q_atof( Cmd_Argv( i + 1 ) );
		S_StartDynamicSound( hash++, CHAN_AUTO, sfx, listener_origin, vol, 1.0, 0, PITCH_NORM );
		i += 2;
	}
}


void S_SoundList( void )
{
	int			i;
	sfx_t		*sfx;
	sfxcache_t	*sc;
	int			size, total;

	total = 0;
	for ( sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++ )
	{
		sc = Cache_Check( &sfx->cache );
		if ( !sc )
			continue;

		size = sc->length * sc->width * ( sc->stereo + 1 );
		total += size;

		if ( sc->loopstart >= 0 )
			Con_Printf( "L" );
		else
			Con_Printf( " " );

		Con_Printf( "(%2db) %6i : %s\n", sc->width * 8, size, sfx->name );
	}

	Con_Printf( "Total resident: %i\n", total );
}


void S_LocalSound( char *sound )
{
	sfx_t	*sfx;

	if ( nosound.value )
		return;

	if ( !sound_started )
		return;

	sfx = S_PrecacheSound( sound );
	if ( !sfx )
	{
		Con_Printf( "S_LocalSound: can't cache %s\n", sound );
		return;
	}

	S_StartDynamicSound( cl.viewentity, -1, sfx, vec3_origin, 1, 1, 0, PITCH_NORM );
}


// speak a sentence from console; works by passing in "!sentencename"
// or "sentence"

void S_Say( void )
{
	sfx_t	*sfx;
	char	sound[256];

	if ( nosound.value )
		return;

	if ( !sound_started )
		return;

	if ( Cmd_Argc() < 2 )
		return;

	Q_strncpy( sound, Cmd_Argv( 1 ), sizeof( sound ) - 1 );
	sound[sizeof( sound ) - 1] = 0;

	// DEBUG - test performance of dsp code
	if ( !sound )
		return;

	if ( !Q_strcmp( sound, "dsp" ) )
	{
		unsigned int	starttime_guess;
		int				count_guess = 10000;
		int				iter_guess;

		for ( iter_guess = 0; iter_guess < PAINTBUFFER_SIZE; iter_guess++ )
		{
			paintbuffer[iter_guess].left = RandomLong( 0, 2999 );
			paintbuffer[iter_guess].right = RandomLong( 0, 2999 );
		}

		Con_Printf( "Start profiling 10,000 calls to DSP\n" );

		// get system time
		starttime_guess = timeGetTime();

		for ( iter_guess = 0; iter_guess < count_guess; iter_guess++ )
			SX_RoomFX( PAINTBUFFER_SIZE, true, true );

		// display system time delta
		Con_Printf( "%d milliseconds \n", timeGetTime() - starttime_guess );
		return;
	}
	else if ( !Q_strcmp( sound, "paint" ) )
	{
		unsigned int	starttime_guess;
		int				count_guess = 10000;
		static int		hash = 543;
		sfx_t			*sfx_guess;
		int				psav_guess = paintedtime;

		Con_Printf( "Start profiling S_PaintChannels\n" );

		sfx_guess = S_PrecacheSound( "ambience/labdrone1.wav" );
		S_StartDynamicSound( hash++, CHAN_AUTO, sfx_guess, listener_origin, 1.0, 1.0, 0, PITCH_NORM );

		// get system time
		starttime_guess = timeGetTime();

		// paint a boatload of sound
		S_PaintChannels( paintedtime + count_guess * 512 );

		// display system time delta
		Con_Printf( "%d milliseconds \n", timeGetTime() - starttime_guess );
		paintedtime = psav_guess;
		return;
	}
	// END DEBUG

	if ( sound[0] != CHAR_SENTENCE )
	{
		// build a fake sentence name, then play the sentence text
		_snprintf( sound, sizeof( sound ), "xxtestxx %s", Cmd_Argv( 1 ) );

		// insert null terminator after sentence name
		rgpszrawsentence[cszrawsentences] = sound;
		cszrawsentences++;
		sound[8] = 0;

		sfx = S_PrecacheSound( "!xxtestxx" );
		if ( !sfx )
		{
			Con_Printf( "S_Say: can't cache %s\n", sound );
			return;
		}

		S_StartDynamicSound( cl.viewentity, -1, sfx, vec3_origin, 1, 1, 0, PITCH_NORM );

		// remove last
		rgpszrawsentence[--cszrawsentences] = NULL;
	}
	else
	{
		sfx = S_FindName( sound, NULL );
		if ( !sfx )
		{
			Con_Printf( "S_Say: can't find sentence name %s\n", sound );
			return;
		}

		S_StartDynamicSound( cl.viewentity, -1, sfx, vec3_origin, 1, 1, 0, PITCH_NORM );
	}
}


void S_Say_Reliable( void )
{
	sfx_t	*sfx;
	char	sound[256];

	if ( nosound.value )
		return;

	if ( !sound_started )
		return;

	if ( Cmd_Argc() < 2 )
		return;

	Q_strncpy( sound, Cmd_Argv( 1 ), sizeof( sound ) - 1 );
	sound[sizeof( sound ) - 1] = 0;

	// DEBUG - test performance of dsp code
	if ( !sound )
		return;

	if ( sound[0] != CHAR_SENTENCE )
	{
		// build a fake sentence name, then play the sentence text
		_snprintf( sound, sizeof( sound ), "xxtestxx %s", Cmd_Argv( 1 ) );

		// insert null terminator after sentence name
		rgpszrawsentence[cszrawsentences] = sound;
		cszrawsentences++;
		sound[8] = 0;

		sfx = S_PrecacheSound( "!xxtestxx" );
		if ( !sfx )
		{
			Con_Printf( "S_Say_Reliable: can't cache %s\n", sound );
			return;
		}

		S_StartStaticSound( cl.viewentity, CHAN_STATIC, sfx, vec3_origin, 1, 1, 0, PITCH_NORM );

		// remove last
		rgpszrawsentence[--cszrawsentences] = NULL;
	}
	else
	{
		sfx = S_FindName( sound, NULL );
		if ( !sfx )
		{
			Con_Printf( "S_Say_Reliable: can't find sentence name %s\n", sound );
			return;
		}

		S_StartStaticSound( cl.viewentity, CHAN_STATIC, sfx, vec3_origin, 1, 1, 0, PITCH_NORM );
	}
}


//=============================================================================

void S_ClearPrecache( void )
{
}


void S_BeginPrecaching( void )
{
	cl.fPrecaching = 1;
}


void S_EndPrecaching( void )
{
	cl.fPrecaching = 0;
}
