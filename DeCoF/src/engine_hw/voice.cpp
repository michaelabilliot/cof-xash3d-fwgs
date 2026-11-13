// voice.cpp -- voice communication: recording, codec and playback channels

#include <math.h>

extern "C"
{
#include "quakedef.h"
#include "cvar.h"
#include "cdll_int.h"

void	FS_GetLocalCopy( char *pszFileName );
void	MP3_Suspend_Audio( void );
void	MP3_Resume_Audio( void );
void	ClientDLL_VoiceStatus( int entindex, qboolean bTalking );
}

#include "interface.h"
#include "CircularBuffer.h"
#include "ivoicecodec.h"
#include "ivoicerecord.h"
#include "voice_gain.h"
#include "voice_mixer_controls.h"
#include "voice_sound_engine_interface.h"
#include "voice_wavefile.h"
#include "voice.h"

// the format the microphone records in
WAVEFORMATEX g_VoiceSampleFormat =
{
	WAVE_FORMAT_PCM,		// wFormatTag
	1,						// nChannels
	8000,					// nSamplesPerSec
	16000,					// nAvgBytesPerSec
	2,						// nBlockAlign
	16,						// wBitsPerSample
	sizeof( WAVEFORMATEX )	// cbSize
};

extern "C"
{
cvar_t	voice_dsound = { "voice_dsound", "1" };
cvar_t	voice_avggain = { "voice_avggain", "0.5" };
cvar_t	voice_maxgain = { "voice_maxgain", "5" };
cvar_t	voice_scale = { "voice_scale", "1", FCVAR_ARCHIVE };

cvar_t	voice_loopback = { "voice_loopback", "0" };
cvar_t	voice_fadeouttime = { "voice_fadeouttime", "0.1" };	// It fades to no sound at the tail end of your voice data when you release the key.

// Debugging cvars.
cvar_t	voice_profile = { "voice_profile", "0" };
cvar_t	voice_showchannels = { "voice_showchannels", "0" };	// 1 = list channels
															// 2 = show timing info, etc
cvar_t	voice_showincoming = { "voice_showincoming", "0" };	// show incoming voice data

cvar_t	voice_enable = { "voice_enable", "1", FCVAR_ARCHIVE };	// Globally enable or disable voice.

// Force the microphone to be the recording source.
cvar_t	voice_forcemicrecord = { "voice_forcemicrecord", "1", FCVAR_ARCHIVE };
}

int		g_nVoiceFadeSamples = 1;	// Calculated each frame from the cvar.
float	g_VoiceFadeMul = 1.0f;		// 1 / (g_nVoiceFadeSamples - 1).

// While in tweak mode, you can't hear anything anyone else is saying, and your own voice data
// goes directly to the speakers.
bool	g_bInTweakMode = false;

bool	g_bVoiceAtLeastPartiallyInitted = false;

// The mixer controls of the microphone.
IMixerControls	*g_pMixerControls = NULL;

// Timing info for each frame.
static double	g_CompressTime = 0;
static double	g_DecompressTime = 0;
static double	g_GainTime = 0;
static double	g_UpsampleTime = 0;

static qboolean	g_bLocalPlayerTalkingAck = false;
static float	g_LocalPlayerTalkingTimeout = 0;

CSysModule	*g_hVoiceCodecDLL = 0;

// Voice recorder. Can be waveIn, DSound, or whatever.
static IVoiceRecord	*g_pVoiceRecord = NULL;
static IVoiceCodec	*g_pEncodeCodec = NULL;

static qboolean	g_bVoiceRecording = false;	// Are we recording at the moment?

// measures the time spent in the voice code (voice_profile)
class CVoiceTimer
{
public:
	inline void Start()
	{
		if ( voice_profile.value )
		{
			m_StartTime = Sys_FloatTime();
		}
	}

	inline void End( double *out )
	{
		if ( voice_profile.value )
		{
			*out += Sys_FloatTime() - m_StartTime;
		}
	}

	double	m_StartTime;
};

// ------------------------------------------------------------------------ //
// CVoiceChannel.
// ------------------------------------------------------------------------ //

class CVoiceChannel
{
public:
	CVoiceChannel();

	// Called when someone speaks and a new voice channel is setup to hold the data.
	void	Init( int nEntity );

public:
	int		m_iEntity;		// Number of the entity speaking on this channel (index into cl_entities).
							// This is -1 when the channel is unused.

	CSizedCircularBuffer<22050>	m_Buffer;	// Circular buffer containing the voice data.

	// Used for upsampling..
	double	m_LastFraction;	// Fraction between m_LastSample and the next sample.
	short	m_LastSample;

	bool	m_bStarved;		// Set to true when the channel runs out of data for the mixer.
							// The channel is killed at that point.

	float	m_TimePad;		// Set to TIME_PADDING when the first voice packet comes in from a sender.
							// We add time padding (for frametime differences)
							// by waiting a certain amount of time before starting to output the sound.

	IVoiceCodec	*m_pVoiceCodec;	// Each channel gets is own IVoiceCodec instance so the codec can maintain state.

	CAutoGain	m_AutoGain;		// Gain we're applying to this channel

	CVoiceChannel	*m_pNext;
};

CVoiceChannel::CVoiceChannel()
{
	m_iEntity = -1;
	m_pVoiceCodec = NULL;
}

void CVoiceChannel::Init( int nEntity )
{
	m_iEntity = nEntity;
	m_bStarved = false;
	m_Buffer.Flush();
	m_TimePad = 0.2f;
	m_LastSample = 0;
	m_LastFraction = 0.999;

	m_AutoGain.Reset( 128, voice_maxgain.value, voice_avggain.value, voice_scale.value );
}

// Incoming voice channels.
CVoiceChannel	g_VoiceChannels[5];

// These are used for recording the wave data into files for debugging.
char		*g_pUncompressedFileData = NULL;
int			g_nUncompressedDataBytes = 0;
const char	*g_pUncompressedDataFilename = NULL;

char		*g_pDecompressedFileData = NULL;
int			g_nDecompressedDataBytes = 0;
const char	*g_pDecompressedDataFilename = NULL;

char		*g_pMicInputFileData = NULL;
int			g_nMicInputFileBytes = 0;
int			g_CurMicInputFileByte = 0;
double		g_MicStartTime;

inline void ApplyFadeToSamples( short *pSamples, int nSamples, int fadeOffset, float fadeMul )
{
	for ( int i = 0; i < nSamples; i++ )
	{
		float percent = ( i + fadeOffset ) * fadeMul;
		pSamples[i] = (short)( pSamples[i] * ( 1 - percent ) );
	}
}

int Voice_GetOutputData( const int iChannel, char *copyBufBytes, const int copyBufSize, const int samplePosition, const int sampleCount )
{
	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];
	short *pCopyBuf = (short *)copyBufBytes;

	int maxOutSamples = copyBufSize / 2;

	// Copy the data out.
	CCircularBuffer *pBuffer = &pChannel->m_Buffer;
	int nBytesToRead = pBuffer->GetReadAvailable();
	nBytesToRead = min( min( nBytesToRead, maxOutSamples ), sampleCount * 2 );
	int nSamplesGotten = pBuffer->Read( pCopyBuf, nBytesToRead ) / 2;

	// Are we at the end of the buffer's data? If so, fade data to silence so it doesn't clip.
	int readSamplesAvail = pBuffer->GetReadAvailable() / 2;
	if ( readSamplesAvail < g_nVoiceFadeSamples )
	{
		int bufferFadeOffset = max( ( readSamplesAvail + nSamplesGotten ) - g_nVoiceFadeSamples, 0 );
		int globalFadeOffset = max( g_nVoiceFadeSamples - ( readSamplesAvail + nSamplesGotten ), 0 );

		ApplyFadeToSamples( &pCopyBuf[bufferFadeOffset], nSamplesGotten - bufferFadeOffset, globalFadeOffset, g_VoiceFadeMul );
	}

	// If there weren't enough samples in the received buffer, pad it with a constant value.
	if ( nSamplesGotten < sampleCount )
	{
		int wantedSampleCount = min( sampleCount, maxOutSamples );
		memset( &pCopyBuf[nSamplesGotten], 0, ( wantedSampleCount - nSamplesGotten ) * 2 );
		nSamplesGotten = wantedSampleCount;
	}

	// If the buffer is out of data, mark this channel to go away.
	if ( pBuffer->GetReadAvailable() == 0 )
	{
		pChannel->m_bStarved = true;
	}

	if ( voice_showchannels.value >= 2 )
	{
		Con_Printf( "Voice - mixed %d samples from channel %d\n", nSamplesGotten, iChannel );
	}

	VoiceSE_MoveMouth( pChannel->m_iEntity, (short *)copyBufBytes, nSamplesGotten );
	return nSamplesGotten;
}

CVoiceChannel *GetVoiceChannel( int iChannel, qboolean bAssert )
{
	if ( iChannel < 0 || iChannel >= 5 )
	{
		return NULL;
	}

	return &g_VoiceChannels[iChannel];
}

void Voice_RegisterCvars( void )
{
	Cvar_RegisterVariable( &voice_loopback );
	Cvar_RegisterVariable( &voice_maxgain );
	Cvar_RegisterVariable( &voice_avggain );
	Cvar_RegisterVariable( &voice_scale );
	Cvar_RegisterVariable( &voice_fadeouttime );
	Cvar_RegisterVariable( &voice_profile );
	Cvar_RegisterVariable( &voice_showchannels );
	Cvar_RegisterVariable( &voice_showincoming );
	Cvar_RegisterVariable( &voice_enable );
	Cvar_RegisterVariable( &voice_dsound );
	Cvar_RegisterVariable( &voice_forcemicrecord );

	VoiceSE_RegisterCvars();
}

qboolean Voice_Init( const char *pCodecName, int quality )
{
	if ( voice_enable.value == 0 )
		return false;

	Voice_Deinit();

	g_bVoiceAtLeastPartiallyInitted = true;

	if ( !VoiceSE_Init() )
		return false;

	// Get the voice input device.
	int rate = g_VoiceSampleFormat.nSamplesPerSec;

	if ( voice_dsound.value == 0 )
	{
		g_pVoiceRecord = CreateVoiceRecord_WaveIn_guess( rate );
		if ( !g_pVoiceRecord )
		{
			Con_Printf( "Unable to initialize waveIn. Trying DirectSound." );
			g_pVoiceRecord = CreateVoiceRecord_DSound_guess( rate );
		}
	}
	else
	{
		g_pVoiceRecord = CreateVoiceRecord_DSound_guess( rate );
		if ( !g_pVoiceRecord )
		{
			Con_Printf( "Unable to initialize DirectSoundCapture. Trying waveIn." );
			g_pVoiceRecord = CreateVoiceRecord_WaveIn_guess( rate );
		}
	}

	if ( !g_pVoiceRecord )
	{
		Con_Printf( "Unable to initialize wave input. You won't be able to speak to other players through a microphone.\n" );
	}

	// Get the codec.
	char dllName[256];
	_snprintf( dllName, sizeof( dllName ), "%s.dll", pCodecName );

	// the Miles codec needs the Miles DLL next to it
	FS_GetLocalCopy( "MSS32.DLL" );

	if ( !g_hVoiceCodecDLL )
	{
		FS_GetLocalCopy( dllName );
		g_hVoiceCodecDLL = Sys_LoadModule( dllName );
	}

	if ( !g_hVoiceCodecDLL )
	{
		Con_Printf( "Unable to load voice codec DLL %s.  Voice disabled.\n", dllName );
		Voice_Deinit();
		return false;
	}

	CreateInterfaceFn createCodecFn = Sys_GetFactory( g_hVoiceCodecDLL );
	if ( !createCodecFn )
	{
		Con_Printf( "Unable to get a factory for voice codec %s.  Voice disabled.\n", pCodecName );
		Voice_Deinit();
		return false;
	}

	g_pEncodeCodec = (IVoiceCodec *)createCodecFn( pCodecName, NULL );
	if ( !g_pEncodeCodec )
	{
		Con_Printf( "Unable to create interface for voice codec %s.  Voice disabled.\n", pCodecName );
		Voice_Deinit();
		return false;
	}

	if ( !g_pEncodeCodec->Init( quality ) )
	{
		Con_Printf( "Unable to initialize voice codec %s.  Voice disabled.\n", pCodecName );
		Voice_Deinit();
		return false;
	}

	for ( int i = 0; i < 5; i++ )
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		pChannel->m_pVoiceCodec = (IVoiceCodec *)createCodecFn( pCodecName, NULL );
		if ( !pChannel->m_pVoiceCodec || !pChannel->m_pVoiceCodec->Init( quality ) )
		{
			Con_Printf( "Unable to get voice codec (%s) interface for channel %d.  Voice disabled.\n", pCodecName, i );
			Voice_Deinit();
			return false;
		}
	}

	g_pMixerControls = GetMixerControls();

	if ( voice_forcemicrecord.value && g_pMixerControls )
	{
		g_pMixerControls->SelectMicrophoneForWaveInput();
	}

	return true;
}

void Voice_EndChannel( int iChannel, bool bTellVoiceSE )
{
	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];

	if ( bTellVoiceSE )
		VoiceSE_EndChannel( iChannel );

	if ( pChannel->m_iEntity >= 0 )
	{
		ClientDLL_VoiceStatus( pChannel->m_iEntity, false );
		VoiceSE_CloseMouth( pChannel->m_iEntity );
	}

	pChannel->m_iEntity = -1;
}

void Voice_NotifyFreeChannel( int iChannel )
{
	Voice_EndChannel( iChannel, false );
}

void Voice_EndAllChannels( void )
{
	int i;

	for ( i = 0; i < 5; i++ )
	{
		Voice_EndChannel( i, true );
	}
}

void Voice_Deinit( void )
{
	// This call tends to be expensive and when voice is not enabled it will continually
	// call in here, so avoid the work if possible.
	if ( !g_bVoiceAtLeastPartiallyInitted )
		return;

	if ( !g_bIsDedicatedServer )
		MP3_Suspend_Audio();

	Voice_EndAllChannels();

	Voice_RecordStop();

	for ( int i = 0; i < 5; i++ )
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		if ( pChannel->m_pVoiceCodec )
		{
			pChannel->m_pVoiceCodec->Release();
			pChannel->m_pVoiceCodec = NULL;
		}
	}

	if ( g_pEncodeCodec )
	{
		g_pEncodeCodec->Release();
		g_pEncodeCodec = NULL;
	}

	if ( g_hVoiceCodecDLL )
	{
		Sys_UnloadModule( g_hVoiceCodecDLL );
		g_hVoiceCodecDLL = NULL;
	}

	if ( g_pVoiceRecord )
	{
		g_pVoiceRecord->Release();
		g_pVoiceRecord = NULL;
	}

	VoiceSE_Term();

	if ( g_pMixerControls )
	{
		g_pMixerControls->Release();
		g_pMixerControls = NULL;
	}

	if ( !g_bIsDedicatedServer )
		MP3_Resume_Audio();

	g_bVoiceAtLeastPartiallyInitted = false;
}

qboolean Voice_GetLoopback( void )
{
	return !!voice_loopback.value;
}

void Voice_LocalPlayerTalkingAck( void )
{
	if ( !g_bLocalPlayerTalkingAck )
	{
		// Tell the client DLL when this changes.
		ClientDLL_VoiceStatus( -2, TRUE );
	}

	g_bLocalPlayerTalkingAck = true;
	g_LocalPlayerTalkingTimeout = 0;
}

void Voice_UpdateVoiceTweakMode( void )
{
	if ( !g_bInTweakMode || !g_pVoiceRecord )
		return;

	char uchVoiceData[4096];
	bool bFinal = false;
	int nDataLength = Voice_GetCompressedData( uchVoiceData, sizeof( uchVoiceData ), bFinal );

	Voice_AddIncomingData( -100, uchVoiceData, nDataLength, 0 );
}

void Voice_Idle( float frametime )
{
	if ( voice_enable.value == 0 )
	{
		Voice_Deinit();
		return;
	}

	if ( g_bLocalPlayerTalkingAck )
	{
		g_LocalPlayerTalkingTimeout += frametime;
		if ( g_LocalPlayerTalkingTimeout > 0.2f )
		{
			g_bLocalPlayerTalkingAck = false;

			// Tell the client DLL.
			ClientDLL_VoiceStatus( -2, FALSE );
		}
	}

	// Precalculate these to speedup the voice fadeout.
	g_nVoiceFadeSamples = max( (int)( voice_fadeouttime.value * 11025 ), 2 );
	g_VoiceFadeMul = 1.0f / ( g_nVoiceFadeSamples - 1 );

	if ( g_pVoiceRecord )
		g_pVoiceRecord->Idle();

	// If we're in voice tweak mode, feed our own data back to us.
	Voice_UpdateVoiceTweakMode();

	// Age the channels.
	int nActive = 0;
	for ( int i = 0; i < 5; i++ )
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		if ( pChannel->m_iEntity != -1 )
		{
			if ( pChannel->m_bStarved )
			{
				// Kill the channel. It's done playing.
				Voice_EndChannel( i, true );
			}
			else
			{
				float oldpad = pChannel->m_TimePad;
				pChannel->m_TimePad -= frametime;
				if ( oldpad > 0 && pChannel->m_TimePad <= 0 )
				{
					// Start its audio.
					VoiceSE_StartChannel( i );
					ClientDLL_VoiceStatus( pChannel->m_iEntity, TRUE );
					VoiceSE_InitMouth( pChannel->m_iEntity );
				}

				++nActive;
			}
		}
	}

	if ( nActive == 0 )
		VoiceSE_EndOverdrive();

	VoiceSE_Idle( frametime );

	// voice_showchannels.
	if ( voice_showchannels.value >= 1 )
	{
		for ( int i = 0; i < 5; i++ )
		{
			CVoiceChannel *pChannel = &g_VoiceChannels[i];

			if ( pChannel->m_iEntity == -1 )
				continue;

			Con_Printf( "Voice - chan %d, ent %d, bufsize: %d\n", i, pChannel->m_iEntity, pChannel->m_Buffer.GetReadAvailable() );
		}
	}

	// Show profiling data?
	if ( voice_profile.value )
	{
		Con_Printf( "Voice - compress: %7.2fu, decompress: %7.2fu, gain: %7.2fu, upsample: %7.2fu, total: %7.2fu\n",
			g_CompressTime * 1000000.0,
			g_DecompressTime * 1000000.0,
			g_GainTime * 1000000.0,
			g_UpsampleTime * 1000000.0,
			( g_CompressTime + g_DecompressTime + g_GainTime + g_UpsampleTime ) * 1000000.0 );

		g_CompressTime = g_DecompressTime = g_GainTime = g_UpsampleTime = 0;
	}
}

qboolean Voice_IsRecording( void )
{
	return g_bVoiceRecording && !g_bInTweakMode;
}

qboolean Voice_RecordStart( const char *pUncompressedFile, const char *pDecompressedFile, const char *pMicInputFile )
{
	if ( !g_pEncodeCodec )
		return false;

	Voice_RecordStop();

	g_pEncodeCodec->ResetState();

	if ( pMicInputFile )
	{
		int a, b, c;
		ReadWaveFile( pMicInputFile, g_pMicInputFileData, g_nMicInputFileBytes, a, b, c );
		g_CurMicInputFileByte = 0;
		g_MicStartTime = Sys_FloatTime();
	}

	if ( pUncompressedFile )
	{
		g_pUncompressedFileData = new char[1024 * 1024];
		g_nUncompressedDataBytes = 0;
		g_pUncompressedDataFilename = pUncompressedFile;
	}

	if ( pDecompressedFile )
	{
		g_pDecompressedFileData = new char[1024 * 1024];
		g_nDecompressedDataBytes = 0;
		g_pDecompressedDataFilename = pDecompressedFile;
	}

	g_bVoiceRecording = false;
	if ( g_pVoiceRecord )
	{
		g_bVoiceRecording = g_pVoiceRecord->RecordStart();
		if ( g_bVoiceRecording )
		{
			// Tell the client DLL.
			ClientDLL_VoiceStatus( -1, TRUE );
		}
	}

	return g_bVoiceRecording;
}

qboolean Voice_RecordStop( void )
{
	// Write the files out for debugging.
	if ( g_pMicInputFileData )
	{
		delete [] g_pMicInputFileData;
		g_pMicInputFileData = NULL;
	}

	if ( g_pUncompressedFileData )
	{
		WriteWaveFile( g_pUncompressedDataFilename, g_pUncompressedFileData, g_nUncompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, g_VoiceSampleFormat.nSamplesPerSec );
		delete [] g_pUncompressedFileData;
		g_pUncompressedFileData = NULL;
	}

	if ( g_pDecompressedFileData )
	{
		WriteWaveFile( g_pDecompressedDataFilename, g_pDecompressedFileData, g_nDecompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, g_VoiceSampleFormat.nSamplesPerSec );
		delete [] g_pDecompressedFileData;
		g_pDecompressedFileData = NULL;
	}

	if ( g_pVoiceRecord )
		g_pVoiceRecord->RecordStop();

	if ( g_bVoiceRecording )
	{
		// Tell the client DLL.
		ClientDLL_VoiceStatus( -1, FALSE );
	}

	g_bVoiceRecording = false;
	return true;
}

static int g_VoiceVolume = 0;

int Voice_GetCompressedData( char *pchDest, int nCount, qboolean bFinal )
{
	IVoiceCodec *pCodec_guess = g_pEncodeCodec;

	if ( g_pVoiceRecord && pCodec_guess )
	{
		short tempData[8192];
		int gotten = g_pVoiceRecord->GetRecordedData( tempData, min( nCount / 2, 8192 ) );

		// If they want to get the data from a file instead of the mic, use that.
		if ( g_pMicInputFileData )
		{
			double curtime = Sys_FloatTime();
			int nShouldGet = ( curtime - g_MicStartTime ) * g_VoiceSampleFormat.nSamplesPerSec;
			gotten = min( sizeof( tempData ) / 2, min( nShouldGet, ( g_nMicInputFileBytes - g_CurMicInputFileByte ) / 2 ) );
			memcpy( tempData, &g_pMicInputFileData[g_CurMicInputFileByte], gotten * 2 );
			g_CurMicInputFileByte += gotten * 2;
			g_MicStartTime = curtime;
		}

		CVoiceTimer timer;
		timer.Start();
		int nCompressedBytes = pCodec_guess->Compress( (char *)tempData, gotten, pchDest, nCount, bFinal != 0 );
		timer.End( &g_CompressTime );

		// Write to our file buffers..
		if ( g_pUncompressedFileData )
		{
			int nToWrite = min( gotten * 2, 1024 * 1024 - g_nUncompressedDataBytes );
			memcpy( &g_pUncompressedFileData[g_nUncompressedDataBytes], tempData, nToWrite );
			g_nUncompressedDataBytes += nToWrite;
		}

		return nCompressedBytes;
	}
	else
	{
		return 0;
	}
}

int VoiceTweak_GetSpeakingVolume( void )
{
	return g_VoiceVolume;
}

int Voice_AssignChannel( int nEntity )
{
	// Don't allow the tweak mode channel to be assigned.
	if ( g_bInTweakMode )
		return -2;

	// See if a channel already exists for this entity and if so, just return it.
	int iFree = -1;
	for ( int i = 0; i < 5; i++ )
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		if ( pChannel->m_iEntity == nEntity )
		{
			return i;
		}
		else if ( pChannel->m_iEntity == -1 && pChannel->m_pVoiceCodec )
		{
			pChannel->m_pVoiceCodec->ResetState();
			iFree = i;
			break;
		}
	}

	// If they're all used, then don't allow them to make a new channel.
	if ( iFree == -1 )
	{
		return -1;
	}

	CVoiceChannel *pChannel = &g_VoiceChannels[iFree];
	pChannel->Init( nEntity );
	VoiceSE_StartOverdrive();

	return iFree;
}

int Voice_GetChannel( int nEntity )
{
	for ( int i = 0; i < 5; i++ )
		if ( g_VoiceChannels[i].m_iEntity == nEntity )
			return i;

	return -1;
}

double UpsampleIntoBuffer( const short *pSrc, int nSrcSamples, CCircularBuffer *pBuffer, double startFraction, double rate )
{
	double maxFraction = nSrcSamples - 1;

	while ( 1 )
	{
		if ( startFraction >= maxFraction )
			break;

		int iSample = (int)startFraction;
		double frac = startFraction - floor( startFraction );

		double val1 = pSrc[iSample];
		double val2 = pSrc[iSample + 1];
		short newSample = (short)( val1 + ( val2 - val1 ) * frac );
		pBuffer->Write( &newSample, sizeof( newSample ) );

		startFraction += rate;
	}

	// Return the fractional part of the start fraction for next time.
	return startFraction - floor( startFraction );
}

int Voice_AddIncomingData( int nChannel, const char *pchData, int nCount, int iSequenceNumber )
{
	CVoiceChannel *pChannel;

	// If in tweak mode, we call this during Idle with -100 as the channel, so any channel
	// we get from the network doesn't get added to the buffers.
	if ( g_bInTweakMode )
	{
		g_VoiceVolume = 0;

		if ( nChannel == -100 )
			nChannel = 0;
		else
			return 0;
	}

	pChannel = GetVoiceChannel( nChannel, true );
	if ( !pChannel || !pChannel->m_pVoiceCodec )
	{
		return 0;
	}

	pChannel->m_bStarved = false;	// This only really matters if you call Voice_AddIncomingData between the time the mixer
									// asks for data and Voice_Idle is called.

	// Decompress.
	CVoiceTimer timer;
	timer.Start();

	char decompressed[8192];
	int nDecompressed = pChannel->m_pVoiceCodec->Decompress( pchData, nCount, decompressed, sizeof( decompressed ) );

	if ( g_bInTweakMode )
	{
		short *data = (short *)decompressed;

		for ( int i = 0; i < nDecompressed; i++ )
		{
			g_VoiceVolume = max( abs( data[i] ), g_VoiceVolume );
		}

		g_VoiceVolume &= 0xFE00;
	}

	timer.End( &g_DecompressTime );

	// Gain.
	timer.Start();
	pChannel->m_AutoGain.ProcessSamples( (short *)decompressed, nDecompressed );
	timer.End( &g_GainTime );

	// Upsample into the dest buffer. We could do this in a mixer but it complicates the mixer.
	timer.Start();
	pChannel->m_LastFraction = UpsampleIntoBuffer(
		(short *)decompressed,
		nDecompressed,
		&pChannel->m_Buffer,
		pChannel->m_LastFraction,
		(double)g_VoiceSampleFormat.nSamplesPerSec / 11025 );
	pChannel->m_LastSample = decompressed[nDecompressed];
	timer.End( &g_UpsampleTime );

	// Write to our file buffers..
	if ( g_pDecompressedFileData )
	{
		int nToWrite = min( nDecompressed * 2, 1024 * 1024 - g_nDecompressedDataBytes );
		memcpy( &g_pDecompressedFileData[g_nDecompressedDataBytes], decompressed, nToWrite );
		g_nDecompressedDataBytes += nToWrite;
	}

	if ( voice_showincoming.value != 0 )
	{
		Con_Printf( "Voice - %d incoming samples added to channel %d\n", nDecompressed, nChannel );
	}

	return nChannel;
}

void Voice_FlushChannel( int nChannel )
{
	if ( nChannel < 0 || nChannel >= 5 )
		return;

	g_VoiceChannels[nChannel].m_Buffer.Flush();
}

// ------------------------------------------------------------------------ //
// IVoiceTweak implementation.
// ------------------------------------------------------------------------ //

int VoiceTweak_StartVoiceTweakMode()
{
	// If we're already in voice tweak mode, return an error.
	if ( g_bInTweakMode )
		return 0;

	if ( !g_pMixerControls )
		return 0;

	Voice_EndAllChannels();
	Voice_RecordStart( NULL, NULL, NULL );
	Voice_AssignChannel( -500 );
	g_bInTweakMode = true;

	return 1;
}

void VoiceTweak_EndVoiceTweakMode()
{
	if ( !g_bInTweakMode )
		return;

	g_bInTweakMode = false;
	Voice_RecordStop();
}

void VoiceTweak_SetControlFloat( VoiceTweakControl iControl, float flValue )
{
	if ( !g_pMixerControls )
		return;

	if ( iControl == MicrophoneVolume )
	{
		g_pMixerControls->SetValue_Float( IMixerControls::MicVolume, flValue );
	}
	else if ( iControl == OtherSpeakerScale )
	{
		voice_scale.value = flValue;
	}
	else if ( iControl == MicBoost )
	{
		g_pMixerControls->SetValue_Float( IMixerControls::MicBoost, flValue );
	}
}

float VoiceTweak_GetControlFloat( VoiceTweakControl iControl )
{
	if ( !g_pMixerControls )
		return 0;

	if ( iControl == MicrophoneVolume )
	{
		float value = 1;
		g_pMixerControls->GetValue_Float( IMixerControls::MicVolume, value );
		return value;
	}
	else if ( iControl == OtherSpeakerScale )
	{
		return voice_scale.value;
	}
	else if ( iControl == MicBoost )
	{
		float value = 1;
		g_pMixerControls->GetValue_Float( IMixerControls::MicBoost, value );
		return value;
	}

	return 1;
}

IVoiceTweak g_VoiceTweakAPI =
{
	VoiceTweak_StartVoiceTweakMode,
	VoiceTweak_EndVoiceTweakMode,
	VoiceTweak_SetControlFloat,
	VoiceTweak_GetControlFloat,
	VoiceTweak_GetSpeakingVolume
};
