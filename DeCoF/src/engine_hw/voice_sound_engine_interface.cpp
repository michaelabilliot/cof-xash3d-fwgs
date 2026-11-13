//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the glue between the voice channels and the sound engine
//
// $NoKeywords: $
//=============================================================================

#include <math.h>

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "net.h"
#include "client.h"
#include "sound.h"

extern float	g_SND_VoiceOverdrive;

void	SND_ForceInitMouth( int entnum );
void	SND_ForceCloseMouth( int entnum );
void	SND_MoveMouth16( int entnum, short *pdata, int count );

int		VoiceSE_GetSoundDataCallback( sfxcache_t *pCache, char *pCopyBuf, int maxOutDataSize, int samplePos, int sampleCount );
}

#include "voice_sound_engine_interface.h"

// voice.cpp (voice.h clashes with the C declarations of client.h)
int		Voice_GetOutputData( const int iChannel, char *copyBufBytes, const int copyBufSize, const int samplePosition, const int sampleCount );
void	Voice_NotifyFreeChannel( int iChannel );

#define NUM_VOICE_SFX		5

// the voice channels play on these sound engine channels
#define VOICE_CHANNEL_BASE	7

class VoiceSE_SFX
{
public:
	sfx_t		m_SFX;
	sfxcache_t	m_SFXCache;
	short		m_PrevUpsampleValue;
};

static VoiceSE_SFX	g_VoiceSE_SFX[NUM_VOICE_SFX];

static float	g_VoiceOverdriveDuration = 0;
static bool		g_bVoiceOverdriveOn = false;

// When voice is on, all other sounds are decreased by this factor.
static cvar_t	voice_overdrive = { "voice_overdrive", "2" };
static cvar_t	voice_overdrivefadetime = { "voice_overdrivefadetime", "0.4" };	// How long it takes to fade in and out of the voice overdrive.

// ------------------------------------------------------------------------ //
// Static helpers
// ------------------------------------------------------------------------ //

// This is called when the sound engine needs more data.
int VoiceSE_GetSoundDataCallback( 
	sfxcache_t *pCache,
	char *pCopyBuf,
	int maxOutDataSize,
	int samplePos,
	int sampleCount )
{
	int i, j;

	// Find the voice channel this is for.
	for ( i = 0; i < NUM_VOICE_SFX; i++ )
	{
		VoiceSE_SFX *pChannel = &g_VoiceSE_SFX[i];
		if ( pCache == &pChannel->m_SFXCache )
		{
			// The voice data is half the sample rate of the sound engine,
			// so ask for half the samples and double them.
			int count = Voice_GetOutputData( i, pCopyBuf, maxOutDataSize, samplePos, sampleCount >> 1 );

			short *pSamples = (short *)pCopyBuf;
			int count2x = count * 2;

			for ( i = count2x - 1, j = count - 1; j > 0; j--, i -= 2 )
			{
				pSamples[i] = pSamples[j];
				pSamples[i - 1] = ( pSamples[j] + pSamples[j - 1] ) >> 1;
			}

			// The first sample goes between the last sample of the previous block and this one.
			if ( count2x >= 2 )
			{
				pSamples[1] = pSamples[0];
				pSamples[0] = ( pChannel->m_PrevUpsampleValue + pSamples[0] ) >> 1;
			}

			pChannel->m_PrevUpsampleValue = pSamples[count2x - 1];

			return count * 2;
		}
	}

	return 0;
}

// ------------------------------------------------------------------------ //
// Interface functions
// ------------------------------------------------------------------------ //

void VoiceSE_RegisterCvars()
{
	Cvar_RegisterVariable( &voice_overdrive );
	Cvar_RegisterVariable( &voice_overdrivefadetime );
}

bool VoiceSE_Init()
{
	if ( !sound_started )
		return false;

	Q_memset( g_VoiceSE_SFX, 0, sizeof( g_VoiceSE_SFX ) );

	for ( int i = 0; i < NUM_VOICE_SFX; i++ )
	{
		VoiceSE_SFX *pChannel = &g_VoiceSE_SFX[i];

		pChannel->m_SFXCache.length = 0x40000000;	// a stream: never ends
		pChannel->m_SFXCache.loopstart = (int)VoiceSE_GetSoundDataCallback;
		pChannel->m_SFXCache.speed = 22050;
		pChannel->m_SFXCache.width = 2;
	}

	return true;
}

void VoiceSE_Term()
{
}

void VoiceSE_Idle( float frametime )
{
	if ( g_bVoiceOverdriveOn )
	{
		if ( g_VoiceOverdriveDuration == voice_overdrivefadetime.value )
			return;

		g_VoiceOverdriveDuration = min( g_VoiceOverdriveDuration + frametime, voice_overdrivefadetime.value );
	}
	else
	{
		if ( g_VoiceOverdriveDuration == 0 )
			return;

		g_VoiceOverdriveDuration = max( g_VoiceOverdriveDuration - frametime, 0 );
	}

	float percent = g_VoiceOverdriveDuration / voice_overdrivefadetime.value;
	percent = (float)( -cos( percent * 3.1415926535 ) * 0.5 + 0.5 );		// Smooth it out..
	g_SND_VoiceOverdrive = ( voice_overdrive.value - 1 ) * percent + 1;
}

void VoiceSE_StartChannel( int iChannel )
{
	if ( iChannel < 0 || iChannel >= NUM_VOICE_SFX )
		return;

	VoiceSE_SFX *pChannel = &g_VoiceSE_SFX[iChannel];
	pChannel->m_PrevUpsampleValue = 0;

	// Start the sound.
	sfx_t *sfx = &g_VoiceSE_SFX[iChannel].m_SFX;
	Q_memset( sfx, 0, sizeof( *sfx ) );
	Q_strcpy( sfx->name, "?VoiceData" );

	S_StartDynamicSound( cl.viewentity, VOICE_CHANNEL_BASE + iChannel, sfx, vec3_origin, 1.0, 1.0, 0, PITCH_NORM );
}

void VoiceSE_EndChannel( int iChannel )
{
	S_StopSound( cl.viewentity, VOICE_CHANNEL_BASE + iChannel );
}

extern "C" sfxcache_t *VoiceSE_GetSFXCache( sfx_t *pSFX )
{
	for ( int i = 0; i < NUM_VOICE_SFX; i++ )
	{
		VoiceSE_SFX *pChannel = &g_VoiceSE_SFX[i];
		if ( &pChannel->m_SFX == pSFX )
			return &pChannel->m_SFXCache;
	}

	return NULL;
}

void VoiceSE_StartOverdrive()
{
	g_bVoiceOverdriveOn = true;
}

void VoiceSE_EndOverdrive()
{
	g_bVoiceOverdriveOn = false;
}

void VoiceSE_InitMouth( int entnum )
{
	SND_ForceInitMouth( entnum );
}

void VoiceSE_CloseMouth( int entnum )
{
	SND_ForceCloseMouth( entnum );
}

void VoiceSE_MoveMouth( int entnum, short *pSamples, int nSamples )
{
	SND_MoveMouth16( entnum, pSamples, nSamples );
}

extern "C" void VoiceSE_NotifyFreeChannel( int iChannel )
{
	Voice_NotifyFreeChannel( iChannel - VOICE_CHANNEL_BASE );
}
