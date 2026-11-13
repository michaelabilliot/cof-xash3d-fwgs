// snd_win.c -- DirectSound, EAX, Aureal A3D and waveOut sound output

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "zone.h"
#include "sound.h"
#include "winquake.h"

#include <mmsystem.h>
#include <initguid.h>

#include "eax.h"

DEFINE_GUID(IID_IKsPropertySet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);
DEFINE_GUID(IID_IDirectSound3DBuffer, 0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

// the EAX speaker buffers hold half a second each
#define EAX_BUFFER_SIZE			0x8000

typedef enum {SIS_SUCCESS, SIS_FAILURE, SIS_NOTAVAIL, SIS_NOTAUREAL_guess} sndinitstat;

// the Aureal A3D glue (hla3d)
int		hwInit_guess( HWND hwnd, int numvoices, int speed, int bits );
void	hwShutdown_guess( HWND hwnd );
int		hwAllocVoices_guess( HWND hwnd );
void	hwFreeVoices_guess( void );
void	hwRestart_guess( void );
void	hwBlockSound_guess( void );
void	hwStartSound_guess( void );
void	hwSetMaxBufferSize_guess( int size );
void	hwSetBufferSize_guess( int size );
void	hwSetAutoDistance_guess( float mindist, float maxdist );
void	hwSetMinMaxDistance_guess( float mindist, float maxdist );
void	hwSetDistanceModel_guess( float doppler, int distance, float rolloff );
void	hwSetBlipDir_guess( int dir );
void	hwUpdateBlip_guess( void );
void	hwRenderGeometry_guess( void );
void	hwSetLeafNum_guess( int leafnum );
void	hwSetNumPolys_guess( int numpolys );
void	hwSetPolySize_guess( float polysize );
void	hwSetPolyKeep_guess( float polykeep );
void	hwSetReflectionDelay_guess( int delay );
void	hwSetReflectionGain_guess( float gain );
void	hwSetShowTossed_guess( int showtossed );
void	hwSetUsePVS_guess( int usepvs );
void	hwSetBloat_guess( float bloat );
void	hwSetOcclusions_guess( int occlude );
void	hwSetReflections_guess( int reflect );
void	hwSetOcclusionFactor_guess( float factor );
void	hwSetOcclusionEpsilon_guess( float epsilon );

// the Aureal A3D renderer
int		hwClear_guess( void );
int		hwFlush_guess( void );
int		hwUpdateStream_guess( void );
int		hwSet2DVolume_guess( float vol );
int		hwGetCurrentPosition_guess( DWORD *pdwPlay, DWORD *pdwWrite );

// set by the A3D glue when its settings must all be sent again
extern qboolean	a3d_forceupdate_guess;

extern EAX_REVERBPROPERTIES	eaxpre_guess[];

extern cvar_t	a3d;
extern cvar_t	s_eax;
extern cvar_t	s_buffersize;
extern cvar_t	s_rolloff;
extern cvar_t	s_doppler;
extern cvar_t	s_automin_distance;
extern cvar_t	s_automax_distance;
extern cvar_t	s_min_distance;
extern cvar_t	s_max_distance;
extern cvar_t	s_2dvolume;
extern cvar_t	s_geometry;
extern cvar_t	s_leafnum;
extern cvar_t	s_blipdir;
extern cvar_t	s_refgain;
extern cvar_t	s_refdelay;
extern cvar_t	s_polykeep;
extern cvar_t	s_polysize;
extern cvar_t	s_showtossed;
extern cvar_t	s_numpolys;
extern cvar_t	s_usepvs;
extern cvar_t	s_bloat;
extern cvar_t	s_reverb;
extern cvar_t	s_occlude;
extern cvar_t	s_reflect;
extern cvar_t	s_occfactor;
extern cvar_t	s_occ_epsilon;

static qboolean	wavonly;
static qboolean	dsound_init;
static qboolean	wav_init;
static qboolean	snd_firsttime = true, snd_isdirect, snd_iswave;
static qboolean	primary_format_set;

static int	sample16;
static int	snd_sent, snd_completed;

// the sound buffer is taken while this is above zero (Snd_AcquireBuffer and Snd_ReleaseBuffer)
static int	snd_buffer_count_guess;

/*
 * Global variables. Must be visible to window-procedure function
 *  so it can unlock and free the data block after it has been played.
 */

HANDLE		hData;
HPSTR		lpData, lpData2;

HGLOBAL		hWaveHdr;
LPWAVEHDR	lpWaveHdr;

HWAVEOUT    hWaveOut;

WAVEOUTCAPS	wavecaps;

DWORD	gSndBufSize;

MMTIME		mmstarttime;

LPDIRECTSOUND pDS;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

HINSTANCE hInstDS;

qboolean	snd_isa3d_guess;
qboolean	snd_iseax_guess;

LPDIRECTSOUNDBUFFER		pDSBufFL_guess = NULL;
LPDIRECTSOUNDBUFFER		pDSBufFR_guess = NULL;
LPDIRECTSOUNDBUFFER		pDSBufRL_guess = NULL;
LPDIRECTSOUNDBUFFER		pDSBufRR_guess = NULL;
LPDIRECTSOUND3DBUFFER	pDS3DBufFL_guess = NULL;
LPDIRECTSOUND3DBUFFER	pDS3DBufFR_guess = NULL;
LPDIRECTSOUND3DBUFFER	pDS3DBufRL_guess = NULL;
LPDIRECTSOUND3DBUFFER	pDS3DBufRR_guess = NULL;
LPDIRECTSOUNDBUFFER		pDSBufVoice_guess = NULL;
LPKSPROPERTYSET			pEAXPropFL_guess = NULL;
LPKSPROPERTYSET			pEAXPropFR_guess = NULL;
LPKSPROPERTYSET			pEAXPropRL_guess = NULL;
LPKSPROPERTYSET			pEAXPropRR_guess = NULL;

// play position of the front left EAX buffer when the buffers started
DWORD	eaxstarttime_guess = 0;
int		eaxroomtype_guess = 0;

// the A3D settings last sent (SNDDMA_UpdateA3D_guess)
static int		s_lastdoppler_guess = 0;
static int		s_lastleafnum_guess = 0;
static int		s_lastshowtossed_guess = 0;
static int		s_lastnumpolys_guess = 0;
static int		s_lastusepvs_guess = 0;
static float	s_lastrefgain_guess = 0;
static float	s_lastrefdelay_guess = 0;
static float	s_lastpolysize_guess = 0;
static float	s_lastpolykeep_guess = 0;
static float	s_lastbuffersize_guess = 0;
static float	s_lastrolloff_guess = 0;
static float	s_lastdistance_guess = 0;
static float	s_lastbloat_guess = 0;
static float	s_lastautomin_guess = 0;
static float	s_lastautomax_guess = 0;
static float	s_lastmin_guess = 0;
static float	s_lastmax_guess = 0;
static char		s_lastblipdir_guess[16] = { 0 };

static int		s_lastocclude_guess = -1;
static int		s_lastreflect_guess = -1;
static float	s_last2dvolume_guess = 1.0;
static float	s_lastoccfactor_guess = -1.0;
static float	s_lastoccepsilon_guess = -1.0;

sndinitstat SNDDMA_InitDirect (void);
qboolean SNDDMA_InitWav (void);
qboolean SNDDMA_InitEAX_guess( LPDIRECTSOUND lpDS, WAVEFORMATEX *pformat, DSBCAPS *pdsbcaps );
void FreeEAXBuffers_guess( void );
sndinitstat SNDDMA_InitA3D_guess( void );

/*
==================
S_GetWAVPointer
==================
*/
void *S_GetWAVPointer( void )
{
	if ( hWaveOut )
		return &hWaveOut;

	return NULL;
}

/*
==================
S_GetDSPointer
==================
*/
void S_GetDSPointer( LPDIRECTSOUND *lpDS, LPDIRECTSOUNDBUFFER *lpDSBuf )
{
	if ( snd_isa3d_guess )
		OutputDebugString( "error in S_GetDSPointer - shouldn't be called with A3D.\n" );

	*lpDS = pDS;
	*lpDSBuf = pDSBuf;
}

/*
==================
S_BlockSound
==================
*/
void S_BlockSound (void)
{
	if ( snd_isa3d_guess )
	{
		hwStopAllSounds_guess();
		hwBlockSound_guess();
		snd_blocked = 1;
	}
	else
	{
		if (snd_iswave)
		{
			snd_blocked++;

			if (snd_blocked == 1)
			{
				waveOutReset (hWaveOut);
			}
		}

		if ( pDSBuf )
			pDSBuf->lpVtbl->Stop( pDSBuf );

		if ( pEAXPropFL_guess )
			pEAXPropFL_guess->lpVtbl->Set( pEAXPropFL_guess, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, NULL, 0, &eaxpre_guess[0], sizeof( EAX_REVERBPROPERTIES ) );

		if ( pDSBufFL_guess )
			pDSBufFL_guess->lpVtbl->Stop( pDSBufFL_guess );
		if ( pDSBufFR_guess )
			pDSBufFR_guess->lpVtbl->Stop( pDSBufFR_guess );
		if ( pDSBufRL_guess )
			pDSBufRL_guess->lpVtbl->Stop( pDSBufRL_guess );
		if ( pDSBufRR_guess )
			pDSBufRR_guess->lpVtbl->Stop( pDSBufRR_guess );
		if ( pDSBufVoice_guess )
			pDSBufVoice_guess->lpVtbl->Stop( pDSBufVoice_guess );
	}
}


/*
==================
S_UnblockSound
==================
*/
void S_UnblockSound (void)
{
	if ( snd_isa3d_guess )
		snd_blocked = 0;

	if (snd_iswave)
	{
		snd_blocked--;
	}

	if ( pDSBuf )
		pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	if ( pEAXPropFL_guess )
		pEAXPropFL_guess->lpVtbl->Set( pEAXPropFL_guess, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, NULL, 0, &eaxpre_guess[eaxroomtype_guess], sizeof( EAX_REVERBPROPERTIES ) );

	if ( pDSBufFL_guess )
		pDSBufFL_guess->lpVtbl->Play( pDSBufFL_guess, 0, 0, DSBPLAY_LOOPING );
	if ( pDSBufFR_guess )
		pDSBufFR_guess->lpVtbl->Play( pDSBufFR_guess, 0, 0, DSBPLAY_LOOPING );
	if ( pDSBufRL_guess )
		pDSBufRL_guess->lpVtbl->Play( pDSBufRL_guess, 0, 0, DSBPLAY_LOOPING );
	if ( pDSBufRR_guess )
		pDSBufRR_guess->lpVtbl->Play( pDSBufRR_guess, 0, 0, DSBPLAY_LOOPING );
	if ( pDSBufVoice_guess )
		pDSBufVoice_guess->lpVtbl->Play( pDSBufVoice_guess, 0, 0, DSBPLAY_LOOPING );
}


/*
==================
FreeSound
==================
*/
void FreeSound (void)
{
	int		i;

	FreeEAXBuffers_guess();

	if ( snd_isa3d_guess )
		hwShutdown_guess( *pmainwindow );

	if (pDSBuf)
	{
		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->Release(pDSBuf);
	}

// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		pDSPBuf->lpVtbl->Release(pDSPBuf);
	}

	if (pDS)
	{
		pDS->lpVtbl->SetCooperativeLevel (pDS, *pmainwindow, DSSCL_NORMAL);
		pDS->lpVtbl->Release(pDS);
	}

	if (hWaveOut)
	{
		waveOutReset (hWaveOut);

		if (lpWaveHdr)
		{
			for (i=0 ; i< WAV_BUFFERS ; i++)
				waveOutUnprepareHeader (hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (hWaveOut);
		hWaveOut = 0;

		if (hWaveHdr)
		{
			GlobalUnlock(hWaveHdr);
			GlobalFree(hWaveHdr);
		}

		if (hData)
		{
			GlobalUnlock(hData);
			GlobalFree(hData);
		}

	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;

	if ( hInstDS )
	{
		pDirectSoundCreate = NULL;
		FreeLibrary( hInstDS );
		hInstDS = NULL;
	}
}

/*
==================
Snd_ReleaseBuffer

Lets go of the sound device while the video mode changes
==================
*/
void Snd_ReleaseBuffer( void )
{
	if ( snd_isa3d_guess )
	{
		S_Shutdown();
	}
	else if ( snd_isdirect )
	{
		if ( --snd_buffer_count_guess == 0 )
		{
			S_ClearBuffer();
			S_Shutdown();
		}
	}
}

/*
==================
Snd_AcquireBuffer
==================
*/
void Snd_AcquireBuffer( void )
{
	if ( snd_isa3d_guess )
	{
		S_Startup();
	}
	else if ( snd_isdirect )
	{
		if ( ++snd_buffer_count_guess == 1 )
		{
			S_Startup();
		}
	}
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
sndinitstat SNDDMA_InitDirect (void)
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	DWORD			dwSize, dwWrite;
	DSCAPS			dscaps;
	WAVEFORMATEX	format, pformat;
	HRESULT			hresult;
	int				reps;

	Q_memset ((void *)&sn, 0, sizeof (sn));

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = 22050;

	Q_memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = shm->channels;
    format.wBitsPerSample = shm->samplebits;
    format.nSamplesPerSec = shm->dmaspeed;
    format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
    format.cbSize = 0;
    format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign;

	if (!hInstDS)
	{
		hInstDS = (HINSTANCE)FS_LoadLibrary("dsound.dll");

		if (hInstDS == NULL)
		{
			Con_SafePrintf ("Couldn't load dsound.dll\n");
			return SIS_FAILURE;
		}

		pDirectSoundCreate = (void *)GetProcAddress(hInstDS,"DirectSoundCreate");

		if (!pDirectSoundCreate)
		{
			Con_SafePrintf ("Couldn't get DS proc addr\n");
			return SIS_FAILURE;
		}
	}

	hresult = iDirectSoundCreate(NULL, &pDS, NULL);

	if (hresult != DS_OK)
	{
		Con_DPrintf ("DirectSound create failed.\n");
		return SIS_FAILURE;
	}

	dscaps.dwSize = sizeof(dscaps);

	if (DS_OK != pDS->lpVtbl->GetCaps (pDS, &dscaps))
	{
		Con_SafePrintf ("Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Con_SafePrintf ("No DirectSound driver installed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

	if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, *pmainwindow, DSSCL_EXCLUSIVE))
	{
		Con_SafePrintf ("Set coop level failed\n");
		FreeSound ();
		return SIS_FAILURE;
	}

// get access to the primary buffer, if possible, so we can set the
// sound hardware format
	Q_memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	Q_memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	primary_format_set = false;

	if (!COM_CheckParm ("-snoforceformat"))
	{
		if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != pDSPBuf->lpVtbl->SetFormat (pDSPBuf, &pformat))
			{
				if (snd_firsttime)
					Con_DPrintf ("Set primary sound buffer format: no\n");
			}
			else
			{
				if (snd_firsttime)
					Con_DPrintf ("Set primary sound buffer format: yes\n");

				primary_format_set = true;
			}
		}
	}

	snd_iseax_guess = false;

	if ( s_eax.value )
		snd_iseax_guess = SNDDMA_InitEAX_guess( pDS, &format, &dsbcaps );

	if ( !snd_iseax_guess )
	{
		Cvar_Set( "s_eax", "0.0" );

		if (!primary_format_set || !COM_CheckParm ("-primarysound"))
		{
		// create the secondary buffer we'll actually work with
			Q_memset (&dsbuf, 0, sizeof(dsbuf));
			dsbuf.dwSize = sizeof(DSBUFFERDESC);
			dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE;
			dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
			dsbuf.lpwfxFormat = &format;

			Q_memset(&dsbcaps, 0, sizeof(dsbcaps));
			dsbcaps.dwSize = sizeof(dsbcaps);

			if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
			{
				Con_SafePrintf ("DS:CreateSoundBuffer Failed");
				FreeSound ();
				return SIS_FAILURE;
			}

			shm->channels = format.nChannels;
			shm->samplebits = format.wBitsPerSample;
			shm->dmaspeed = format.nSamplesPerSec;

			if (DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps))
			{
				Con_SafePrintf ("DS:GetCaps failed\n");
				FreeSound ();
				return SIS_FAILURE;
			}

			if (snd_firsttime)
				Con_SafePrintf ("Using secondary sound buffer\n");
		}
		else
		{
			if (DS_OK != pDS->lpVtbl->SetCooperativeLevel (pDS, *pmainwindow, DSSCL_WRITEPRIMARY))
			{
				Con_SafePrintf ("Set coop level failed\n");
				FreeSound ();
				return SIS_FAILURE;
			}

			if (DS_OK != pDSPBuf->lpVtbl->GetCaps (pDSPBuf, &dsbcaps))
			{
				Con_Printf ("DS:GetCaps failed\n");
				return SIS_FAILURE;
			}

			pDSBuf = pDSPBuf;
			Con_SafePrintf ("Using primary sound buffer\n");
		}

		// Make sure mixer is active
		pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

		if (snd_firsttime)
			Con_DPrintf("   %d channel(s)\n"
			               "   %d bits/sample\n"
						   "   %d bytes/sec\n",
						   shm->channels, shm->samplebits, shm->speed);

		gSndBufSize = dsbcaps.dwBufferBytes;

	// initialize the buffer
		reps = 0;

		while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Con_SafePrintf ("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
				FreeSound ();
				return SIS_FAILURE;
			}

			if (++reps > 10000)
			{
				Con_SafePrintf ("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
				FreeSound ();
				return SIS_FAILURE;
			}

		}

		Q_memset(lpData, 0, dwSize);
//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

		pDSBuf->lpVtbl->Unlock(pDSBuf, lpData, dwSize, NULL, 0);

		/* we don't want anyone to access the buffer directly w/o locking it first. */
		lpData = NULL;

		pDSBuf->lpVtbl->Stop(pDSBuf);
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmstarttime.u.sample, &dwWrite);
		pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	dsound_init = true;

	return SIS_SUCCESS;
}

/*
==================
SNDDMA_GetEAXDMAPos_guess

The position in the four EAX buffers, from the front left one
==================
*/
DWORD SNDDMA_GetEAXDMAPos_guess( void )
{
	DWORD	s;
	DWORD	dwPos_guess;

	pDSBufFL_guess->lpVtbl->GetCurrentPosition( pDSBufFL_guess, &dwPos_guess, NULL );

	dwPos_guess -= eaxstarttime_guess;
	s = dwPos_guess * 2;

	s >>= sample16;

	s &= ( shm->samples - 1 );

	return s;
}

/*
==================
FreeEAXBuffers_guess
==================
*/
void FreeEAXBuffers_guess( void )
{
	if ( pEAXPropFL_guess )
		pEAXPropFL_guess->lpVtbl->Release( pEAXPropFL_guess );
	pEAXPropFL_guess = NULL;

	if ( pEAXPropFR_guess )
		pEAXPropFR_guess->lpVtbl->Release( pEAXPropFR_guess );
	pEAXPropFR_guess = NULL;

	if ( pEAXPropRL_guess )
		pEAXPropRL_guess->lpVtbl->Release( pEAXPropRL_guess );
	pEAXPropRL_guess = NULL;

	if ( pEAXPropRR_guess )
		pEAXPropRR_guess->lpVtbl->Release( pEAXPropRR_guess );
	pEAXPropRR_guess = NULL;

	if ( pDS3DBufFL_guess )
		pDS3DBufFL_guess->lpVtbl->Release( pDS3DBufFL_guess );
	pDS3DBufFL_guess = NULL;

	if ( pDS3DBufFR_guess )
		pDS3DBufFR_guess->lpVtbl->Release( pDS3DBufFR_guess );
	pDS3DBufFR_guess = NULL;

	if ( pDS3DBufRL_guess )
		pDS3DBufRL_guess->lpVtbl->Release( pDS3DBufRL_guess );
	pDS3DBufRL_guess = NULL;

	if ( pDS3DBufRR_guess )
		pDS3DBufRR_guess->lpVtbl->Release( pDS3DBufRR_guess );
	pDS3DBufRR_guess = NULL;

	if ( pDSBufFL_guess )
		pDSBufFL_guess->lpVtbl->Release( pDSBufFL_guess );
	pDSBufFL_guess = NULL;

	if ( pDSBufFR_guess )
		pDSBufFR_guess->lpVtbl->Release( pDSBufFR_guess );
	pDSBufFR_guess = NULL;

	if ( pDSBufRL_guess )
		pDSBufRL_guess->lpVtbl->Release( pDSBufRL_guess );
	pDSBufRL_guess = NULL;

	if ( pDSBufRR_guess )
		pDSBufRR_guess->lpVtbl->Release( pDSBufRR_guess );
	pDSBufRR_guess = NULL;

	if ( pDSBufVoice_guess )
		pDSBufVoice_guess->lpVtbl->Release( pDSBufVoice_guess );
	pDSBufVoice_guess = NULL;
}

/*
==================
SNDDMA_InitEAX_guess

Four mono 3D buffers, one per speaker, with the EAX reverb on them,
and one more buffer for the voice sounds
==================
*/
qboolean SNDDMA_InitEAX_guess( LPDIRECTSOUND lpDS, WAVEFORMATEX *pformat, DSBCAPS *pdsbcaps )
{
	DSBUFFERDESC	dsbuf_voice_guess;
	DWORD			dwSize;
	HRESULT			hresult;
	DSBUFFERDESC	dsbuf;
	WAVEFORMATEX	eaxformat_guess;
	int				reps;
	DWORD			dwSupport;
	DWORD			dwWrite;

	if ( !lpDS )
		return false;

	Q_memcpy( &eaxformat_guess, pformat, sizeof( eaxformat_guess ) );
	eaxformat_guess.nChannels = 1;
	eaxformat_guess.nBlockAlign = eaxformat_guess.nChannels * eaxformat_guess.wBitsPerSample / 8;
	eaxformat_guess.nAvgBytesPerSec = eaxformat_guess.nSamplesPerSec * eaxformat_guess.nBlockAlign;

	Q_memset( &dsbuf, 0, sizeof( dsbuf ) );
	dsbuf.dwSize = sizeof( DSBUFFERDESC );
	dsbuf.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY;
	dsbuf.dwBufferBytes = EAX_BUFFER_SIZE;
	dsbuf.lpwfxFormat = &eaxformat_guess;

	if ( DS_OK != lpDS->lpVtbl->CreateSoundBuffer( lpDS, &dsbuf, &pDSBufFL_guess, NULL ) )
	{
		Con_SafePrintf( "DS:CreateSoundBuffer for EAX front left failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != lpDS->lpVtbl->CreateSoundBuffer( lpDS, &dsbuf, &pDSBufFR_guess, NULL ) )
	{
		Con_SafePrintf( "DS:CreateSoundBuffer for EAX front right failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != lpDS->lpVtbl->CreateSoundBuffer( lpDS, &dsbuf, &pDSBufRL_guess, NULL ) )
	{
		Con_SafePrintf( "DS:CreateSoundBuffer for EAX rear left failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != lpDS->lpVtbl->CreateSoundBuffer( lpDS, &dsbuf, &pDSBufRR_guess, NULL ) )
	{
		Con_SafePrintf( "DS:CreateSoundBuffer for EAX rear right failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	Q_memset( &dsbuf_voice_guess, 0, sizeof( dsbuf_voice_guess ) );
	dsbuf_voice_guess.dwSize = sizeof( DSBUFFERDESC );
	dsbuf_voice_guess.dwFlags = 0;
	dsbuf_voice_guess.dwBufferBytes = EAX_BUFFER_SIZE;
	dsbuf_voice_guess.lpwfxFormat = &eaxformat_guess;

	if ( DS_OK != lpDS->lpVtbl->CreateSoundBuffer( lpDS, &dsbuf_voice_guess, &pDSBufVoice_guess, NULL ) )
	{
		Con_SafePrintf( "DS:CreateSoundBuffer for voice failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufFL_guess->lpVtbl->QueryInterface( pDSBufFL_guess, &IID_IDirectSound3DBuffer, (void **)&pDS3DBufFL_guess ) )
	{
		Con_SafePrintf( "DS:Query 3DBuffer for EAX front left failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufFR_guess->lpVtbl->QueryInterface( pDSBufFR_guess, &IID_IDirectSound3DBuffer, (void **)&pDS3DBufFR_guess ) )
	{
		Con_SafePrintf( "DS:Query 3DBuffer for EAX front right failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufRL_guess->lpVtbl->QueryInterface( pDSBufRL_guess, &IID_IDirectSound3DBuffer, (void **)&pDS3DBufRL_guess ) )
	{
		Con_SafePrintf( "DS:Query 3DBuffer for EAX rear left failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufRR_guess->lpVtbl->QueryInterface( pDSBufRR_guess, &IID_IDirectSound3DBuffer, (void **)&pDS3DBufRR_guess ) )
	{
		Con_SafePrintf( "DS:Query 3DBuffer for EAX rear right failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	pDS3DBufFL_guess->lpVtbl->SetPosition( pDS3DBufFL_guess, -1.0, 0.0, 1.0, DS3D_IMMEDIATE );
	pDS3DBufFR_guess->lpVtbl->SetPosition( pDS3DBufFR_guess, 1.0, 0.0, 1.0, DS3D_IMMEDIATE );
	pDS3DBufRL_guess->lpVtbl->SetPosition( pDS3DBufRL_guess, -1.0, 0.0, -1.0, DS3D_IMMEDIATE );
	pDS3DBufRR_guess->lpVtbl->SetPosition( pDS3DBufRR_guess, 1.0, 0.0, -1.0, DS3D_IMMEDIATE );

	dwSupport = 0;

	if ( DS_OK == pDSBufFL_guess->lpVtbl->QueryInterface( pDSBufFL_guess, &IID_IKsPropertySet, (void **)&pEAXPropFL_guess ) )
	{
		if ( DS_OK == pEAXPropFL_guess->lpVtbl->QuerySupport( pEAXPropFL_guess, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, &dwSupport ) )
		{
			if ( ( dwSupport & KSPROPERTY_SUPPORT_SET | KSPROPERTY_SUPPORT_GET ) != ( KSPROPERTY_SUPPORT_GET | KSPROPERTY_SUPPORT_SET ) )
			{
				Con_SafePrintf( "The current device is not EAX capable." );
				FreeEAXBuffers_guess();
				return false;
			}
		}
		else
		{
			Con_SafePrintf( "EAX:QuerySupport for DSPROPSETID_EAX_ReverbProperties Failed" );
			FreeEAXBuffers_guess();
			return false;
		}
	}
	else
	{
		Con_SafePrintf( "EAX:QueryInterface for IID_IKsPropertySet FL Failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufFR_guess->lpVtbl->QueryInterface( pDSBufFR_guess, &IID_IKsPropertySet, (void **)&pEAXPropFR_guess ) )
	{
		Con_SafePrintf( "EAX:QueryInterface for IID_IKsPropertySet FR Failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufRL_guess->lpVtbl->QueryInterface( pDSBufRL_guess, &IID_IKsPropertySet, (void **)&pEAXPropRL_guess ) )
	{
		Con_SafePrintf( "EAX:QueryInterface for IID_IKsPropertySet RL Failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	if ( DS_OK != pDSBufRR_guess->lpVtbl->QueryInterface( pDSBufRR_guess, &IID_IKsPropertySet, (void **)&pEAXPropRR_guess ) )
	{
		Con_SafePrintf( "EAX:QueryInterface for IID_IKsPropertySet RR Failed" );
		FreeEAXBuffers_guess();
		return false;
	}

	shm->channels = pformat->nChannels;
	shm->samplebits = pformat->wBitsPerSample;
	shm->dmaspeed = pformat->nSamplesPerSec;

	Q_memset( pdsbcaps, 0, sizeof( DSBCAPS ) );
	pdsbcaps->dwSize = sizeof( DSBCAPS );

	if ( DS_OK != pDSBufFL_guess->lpVtbl->GetCaps( pDSBufFL_guess, pdsbcaps ) )
	{
		Con_SafePrintf( "DS:GetCaps failed for EAX buffer\n" );
		FreeEAXBuffers_guess();
		return false;
	}

	pDSBufFL_guess->lpVtbl->Play( pDSBufFL_guess, 0, 0, DSBPLAY_LOOPING );
	pDSBufFR_guess->lpVtbl->Play( pDSBufFR_guess, 0, 0, DSBPLAY_LOOPING );

	if ( snd_firsttime )
		Con_DPrintf( "   %d channel(s)\n"
		             "   %d bits/sample\n"
		             "   %d bytes/sec\n",
		             shm->channels, shm->samplebits, shm->speed );

	gSndBufSize = pdsbcaps->dwBufferBytes * 2;

	reps = 0;

	while ( ( hresult = pDSBufFL_guess->lpVtbl->Lock( pDSBufFL_guess, 0, pdsbcaps->dwBufferBytes, &lpData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for EAX FL\n" );
			FreeEAXBuffers_guess();
			return false;
		}

		if ( ++reps > 10000 )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS: couldn't restore buffer for EAX FL\n" );
			FreeEAXBuffers_guess();
			return false;
		}
	}

	Q_memset( lpData, 0, dwSize );
	pDSBufFL_guess->lpVtbl->Unlock( pDSBufFL_guess, lpData, dwSize, NULL, 0 );

	reps = 0;

	while ( ( hresult = pDSBufFR_guess->lpVtbl->Lock( pDSBufFR_guess, 0, pdsbcaps->dwBufferBytes, &lpData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for EAX FR\n" );
			FreeEAXBuffers_guess();
			return false;
		}

		if ( ++reps > 10000 )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS: couldn't restore buffer for EAX FR\n" );
			FreeEAXBuffers_guess();
			return false;
		}
	}

	Q_memset( lpData, 0, dwSize );
	pDSBufFR_guess->lpVtbl->Unlock( pDSBufFR_guess, lpData, dwSize, NULL, 0 );

	reps = 0;

	while ( ( hresult = pDSBufRL_guess->lpVtbl->Lock( pDSBufRL_guess, 0, pdsbcaps->dwBufferBytes, &lpData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for EAX RL\n" );
			FreeEAXBuffers_guess();
			return false;
		}

		if ( ++reps > 10000 )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS: couldn't restore buffer for EAX RL\n" );
			FreeEAXBuffers_guess();
			return false;
		}
	}

	Q_memset( lpData, 0, dwSize );
	pDSBufRL_guess->lpVtbl->Unlock( pDSBufRL_guess, lpData, dwSize, NULL, 0 );

	reps = 0;

	while ( ( hresult = pDSBufRR_guess->lpVtbl->Lock( pDSBufRR_guess, 0, pdsbcaps->dwBufferBytes, &lpData, &dwSize, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS::Lock Sound Buffer Failed for EAX RR\n" );
			FreeEAXBuffers_guess();
			return false;
		}

		if ( ++reps > 10000 )
		{
			Con_SafePrintf( "SNDDMA_InitDirect: DS: couldn't restore buffer for EAX RR\n" );
			FreeEAXBuffers_guess();
			return false;
		}
	}

	Q_memset( lpData, 0, dwSize );
	pDSBufRR_guess->lpVtbl->Unlock( pDSBufRR_guess, lpData, dwSize, NULL, 0 );

	lpData = NULL;

	pDSBufFL_guess->lpVtbl->Stop( pDSBufFL_guess );
	pDSBufFR_guess->lpVtbl->Stop( pDSBufFR_guess );
	pDSBufRL_guess->lpVtbl->Stop( pDSBufRL_guess );
	pDSBufRR_guess->lpVtbl->Stop( pDSBufRR_guess );

	pDSBufFL_guess->lpVtbl->GetCurrentPosition( pDSBufFL_guess, &eaxstarttime_guess, &dwWrite );
	pDSBufFR_guess->lpVtbl->SetCurrentPosition( pDSBufFR_guess, eaxstarttime_guess );
	pDSBufRL_guess->lpVtbl->SetCurrentPosition( pDSBufRL_guess, eaxstarttime_guess );
	pDSBufRR_guess->lpVtbl->SetCurrentPosition( pDSBufRR_guess, eaxstarttime_guess );
	pDSBufVoice_guess->lpVtbl->SetCurrentPosition( pDSBufVoice_guess, eaxstarttime_guess );

	pDSBufVoice_guess->lpVtbl->Play( pDSBufVoice_guess, 0, 0, DSBPLAY_LOOPING );
	pDSBufFL_guess->lpVtbl->Play( pDSBufFL_guess, 0, 0, DSBPLAY_LOOPING );
	pDSBufFR_guess->lpVtbl->Play( pDSBufFR_guess, 0, 0, DSBPLAY_LOOPING );
	pDSBufRL_guess->lpVtbl->Play( pDSBufRL_guess, 0, 0, DSBPLAY_LOOPING );
	pDSBufRR_guess->lpVtbl->Play( pDSBufRR_guess, 0, 0, DSBPLAY_LOOPING );

	if ( snd_firsttime )
		Con_SafePrintf( "EAX initialization successful\n" );

	return true;
}

/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
qboolean SNDDMA_InitWav (void)
{
	WAVEFORMATEX  format;
	int				i;
	HRESULT			hr;

	snd_sent = 0;
	snd_completed = 0;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = 22050;

	Q_memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = shm->channels;
	format.wBitsPerSample = shm->samplebits;
	format.nSamplesPerSec = shm->dmaspeed;
	format.nBlockAlign = format.nChannels
		*format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec
		*format.nBlockAlign;

	/* Open a waveform device for output using window callback. */
	hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER,
					&format,
					0, 0L, CALLBACK_NULL);

	if (hr != MMSYSERR_NOERROR)
	{
		Con_Printf ("The sound hardware is in use by another app.\nSound not available ( %i ).\n", hr);
		return false;
	}

	/*
	 * Allocate and lock memory for the waveform data. The memory
	 * for waveform data must be globally allocated with
	 * GMEM_MOVEABLE and GMEM_SHARE flags.

	*/
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize);
	if (!hData)
	{
		Con_SafePrintf ("Sound: Out of memory.\n");
		FreeSound ();
		return false;
	}
	lpData = GlobalLock(hData);
	if (!lpData)
	{
		Con_SafePrintf ("Sound: Failed to lock.\n");
		FreeSound ();
		return false;
	}
	Q_memset (lpData, 0, gSndBufSize);

	/*
	 * Allocate and lock memory for the header. This memory must
	 * also be globally allocated with GMEM_MOVEABLE and
	 * GMEM_SHARE flags.
	 */
	hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

	if (hWaveHdr == NULL)
	{
		Con_SafePrintf ("Sound: Failed to Alloc header.\n");
		FreeSound ();
		return false;
	}

	lpWaveHdr = (LPWAVEHDR) GlobalLock(hWaveHdr);

	if (lpWaveHdr == NULL)
	{
		Con_SafePrintf ("Sound: Failed to lock header.\n");
		FreeSound ();
		return false;
	}

	Q_memset (lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */
	for (i=0 ; i<WAV_BUFFERS ; i++)
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE;
		lpWaveHdr[i].lpData = lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(hWaveOut, lpWaveHdr+i, sizeof(WAVEHDR)) !=
				MMSYSERR_NOERROR)
		{
			Con_SafePrintf ("Sound: failed to prepare wave headers\n");
			FreeSound ();
			return false;
		}
	}

	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = gSndBufSize/(shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *) lpData;
	sample16 = (shm->samplebits/8) - 1;

	wav_init = true;

	return true;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/

int SNDDMA_Init(void)
{
	sndinitstat	stat;

	if (COM_CheckParm ("-wavonly"))
		wavonly = true;

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	/* Init A3D */
	if ( !wavonly && a3d.value )
		stat = SNDDMA_InitA3D_guess();

	if ( stat == SIS_SUCCESS )
	{
		Con_DPrintf( "Aureal A3D initialized\n" );

		if ( !snd_firsttime )
			hwRestart_guess();

		snd_isa3d_guess = true;
		snd_isdirect = false;
		Cvar_Set( "s_eax", "0.0" );
		snd_iseax_guess = false;
	}
	else if ( stat == SIS_NOTAUREAL_guess )
	{
		Con_DPrintf( "Non-Aureal A3D.dll found.  This is an unsupported version of A3D.\nDropping to Direct Sound\n" );
		snd_isa3d_guess = false;
	}
	else
	{
		snd_isa3d_guess = false;
	}

	if ( !snd_isa3d_guess )
	{
		Cvar_Set( "s_a3d", "0.0" );

		/* Init DirectSound */
		if (!wavonly && WinNT)
		{
			if (snd_firsttime || snd_isdirect)
			{
				stat = SNDDMA_InitDirect ();;

				if (stat == SIS_SUCCESS)
				{
					snd_isdirect = true;

					if (snd_firsttime)
						Con_DPrintf ("DirectSound initialized\n");
				}
				else
				{
					snd_isdirect = false;
					Con_DPrintf ("DirectSound failed to init\n");
				}
			}
		}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
		if (!dsound_init && (stat != SIS_NOTAVAIL))
		{
			if (snd_firsttime || snd_iswave)
			{

				snd_iswave = SNDDMA_InitWav ();

				if (snd_iswave)
				{
					if (snd_firsttime)
						Con_DPrintf ("Wave sound initialized\n");
				}
				else
				{
					Con_DPrintf ("Wave sound failed to init\n");
				}
			}
		}
	}

	snd_firsttime = false;

	snd_buffer_count_guess = 1;

	if (!snd_isa3d_guess && !dsound_init && !wav_init)
	{
		if (snd_firsttime)
			Con_DPrintf ("No sound device initialized\n");

		return 0;
	}

	return 1;
}

/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos(void)
{
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	if ( snd_iseax_guess )
		return SNDDMA_GetEAXDMAPos_guess();

	if ( snd_isa3d_guess )
	{
		if ( hwIsActive_guess() )
		{
			hwGetCurrentPosition_guess( &mmtime.u.sample, &dwWrite );
			s = mmtime.u.sample - mmstarttime.u.sample;
		}
	}
	else if (dsound_init)
	{
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if (wav_init)
	{
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;

	s &= (shm->samples-1);

	return s;
}

/*
==============
SNDDMA_UpdateA3D_guess

Sends the A3D settings that changed since the last frame
===============
*/
void SNDDMA_UpdateA3D_guess( void )
{
	if ( snd_isa3d_guess )
	{
		hwClear_guess();

		if ( a3d_forceupdate_guess || s_buffersize.value != s_lastbuffersize_guess )
		{
			s_lastbuffersize_guess = s_buffersize.value;
			hwSetBufferSize_guess( (int)s_buffersize.value );
		}

		if ( a3d_forceupdate_guess || s_rolloff.value != s_lastrolloff_guess || s_lastdoppler_guess != s_doppler.value || s_distance.value != s_lastdistance_guess )
		{
			s_lastrolloff_guess = s_rolloff.value;
			s_lastdoppler_guess = (int)s_doppler.value;
			s_lastdistance_guess = s_distance.value;
			hwSetDistanceModel_guess( s_doppler.value, (int)s_distance.value, s_rolloff.value );
		}

		if ( a3d_forceupdate_guess || s_lastautomin_guess != s_automin_distance.value || s_lastautomax_guess != s_automax_distance.value )
		{
			s_lastautomin_guess = s_automin_distance.value;
			s_lastautomax_guess = s_automax_distance.value;
			hwSetAutoDistance_guess( s_automin_distance.value, s_automax_distance.value );
		}

		if ( a3d_forceupdate_guess || s_lastmin_guess != s_min_distance.value || s_lastmax_guess != s_max_distance.value )
		{
			s_lastmin_guess = s_min_distance.value;
			s_lastmax_guess = s_max_distance.value;
			hwSetMinMaxDistance_guess( s_min_distance.value, s_max_distance.value );
		}

		if ( a3d_forceupdate_guess || s_last2dvolume_guess != s_2dvolume.value )
		{
			s_last2dvolume_guess = s_2dvolume.value;
			hwSet2DVolume_guess( s_last2dvolume_guess );
		}

		if ( a3d_forceupdate_guess || s_lastleafnum_guess != (int)s_leafnum.value )
		{
			s_lastleafnum_guess = (int)s_leafnum.value;
			hwSetLeafNum_guess( s_lastleafnum_guess );
		}

		if ( a3d_forceupdate_guess || s_lastrefgain_guess != s_refgain.value )
		{
			s_lastrefgain_guess = s_refgain.value;
			hwSetReflectionGain_guess( s_lastrefgain_guess );
		}

		if ( a3d_forceupdate_guess || s_lastrefdelay_guess != s_refdelay.value )
		{
			s_lastrefdelay_guess = s_refdelay.value;
			hwSetReflectionDelay_guess( (int)s_lastrefdelay_guess );
		}

		if ( a3d_forceupdate_guess || s_lastpolykeep_guess != s_polykeep.value )
		{
			s_lastpolykeep_guess = s_polykeep.value;
			hwSetPolyKeep_guess( s_lastpolykeep_guess );
		}

		if ( a3d_forceupdate_guess || s_lastpolysize_guess != s_polysize.value )
		{
			s_lastpolysize_guess = s_polysize.value;
			hwSetPolySize_guess( s_lastpolysize_guess );
		}

		if ( a3d_forceupdate_guess || s_lastnumpolys_guess != (int)s_numpolys.value )
		{
			s_lastnumpolys_guess = (int)s_numpolys.value;
			hwSetNumPolys_guess( s_lastnumpolys_guess );
		}

		if ( a3d_forceupdate_guess || s_lastshowtossed_guess != (int)s_showtossed.value )
		{
			s_lastshowtossed_guess = (int)s_showtossed.value;
			hwSetShowTossed_guess( s_lastshowtossed_guess );
		}

		if ( a3d_forceupdate_guess || s_lastusepvs_guess != (int)s_usepvs.value )
		{
			s_lastusepvs_guess = (int)s_usepvs.value;
			hwSetUsePVS_guess( s_lastusepvs_guess );
		}

		if ( a3d_forceupdate_guess || s_lastbloat_guess != s_bloat.value )
		{
			s_lastbloat_guess = s_bloat.value;
			hwSetBloat_guess( s_lastbloat_guess );
		}

		if ( a3d_forceupdate_guess || _strnicmp( s_lastblipdir_guess, s_blipdir.string, 16 ) )
		{
			Q_strncpy( s_lastblipdir_guess, s_blipdir.string, 16 );
			s_lastblipdir_guess[15] = 0;

			if ( !_strnicmp( s_lastblipdir_guess, "up", 16 ) )
				hwSetBlipDir_guess( 5 );
			else if ( !_strnicmp( s_lastblipdir_guess, "down", 16 ) )
				hwSetBlipDir_guess( 6 );
			else if ( !_strnicmp( s_lastblipdir_guess, "left", 16 ) )
				hwSetBlipDir_guess( 4 );
			else if ( !_strnicmp( s_lastblipdir_guess, "right", 16 ) )
				hwSetBlipDir_guess( 3 );
			else if ( !_strnicmp( s_lastblipdir_guess, "front", 16 ) )
				hwSetBlipDir_guess( 1 );
			else if ( !_strnicmp( s_lastblipdir_guess, "back", 16 ) )
				hwSetBlipDir_guess( 2 );
			else
				hwSetBlipDir_guess( 0 );
		}

		if ( a3d_forceupdate_guess || (int)s_reverb.value )
		{
		}

		if ( a3d_forceupdate_guess || s_lastreflect_guess != (int)s_reflect.value )
		{
			s_lastreflect_guess = (int)s_reflect.value;
			hwSetReflections_guess( s_lastreflect_guess );
		}

		if ( a3d_forceupdate_guess || s_lastocclude_guess != (int)s_occlude.value )
		{
			s_lastocclude_guess = (int)s_occlude.value;
			hwSetOcclusions_guess( s_lastocclude_guess );
		}

		if ( a3d_forceupdate_guess || s_lastoccfactor_guess != s_occfactor.value )
		{
			s_lastoccfactor_guess = s_occfactor.value;
			hwSetOcclusionFactor_guess( s_lastoccfactor_guess );
		}

		if ( a3d_forceupdate_guess || s_lastoccepsilon_guess != s_occ_epsilon.value )
		{
			s_lastoccepsilon_guess = s_occ_epsilon.value;
			hwSetOcclusionEpsilon_guess( s_lastoccfactor_guess );
		}

		a3d_forceupdate_guess = false;
	}
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	LPWAVEHDR	h;
	int			wResult;
	int			nBlocks_guess;

	if ( snd_isa3d_guess )
	{
		if ( !snd_blocked )
			hwUpdateStream_guess();

		if ( s_geometry.value )
			hwRenderGeometry_guess();

		hwUpdateBlip_guess();
		hwFlush_guess();
		return;
	}

	if (!wav_init)
		return;

	//
	// find which sound blocks have completed
	//
	while (1)
	{
		if ( snd_completed == snd_sent )
		{
			Con_DPrintf ("Sound overrun\n");
			break;
		}

		if ( ! (lpWaveHdr[ snd_completed & WAV_MASK].dwFlags & WHDR_DONE) )
		{
			break;
		}

		snd_completed++;	// this buffer has been played
	}

	//
	// submit two new sound blocks
	//
	nBlocks_guess = 8;

	while (((snd_sent - snd_completed) >> sample16) < nBlocks_guess)
	{
		h = lpWaveHdr + ( snd_sent&WAV_MASK );

		snd_sent++;
		/*
		 * Now the data block can be sent to the output device. The
		 * waveOutWrite function returns immediately and waveform
		 * data is sent to the output device in the background.
		 */
		wResult = waveOutWrite(hWaveOut, h, sizeof(WAVEHDR));

		if (wResult != MMSYSERR_NOERROR)
		{
			Con_SafePrintf ("Failed to write block to device\n");
			FreeSound ();
			return;
		}
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown(void)
{
	FreeSound ();
}

/*
==============
SNDDMA_Activate_guess

Gets the A3D voices back after the game window was activated
===============
*/
void SNDDMA_Activate_guess( qboolean bActive )
{
	if ( bActive )
	{
		if ( snd_isa3d_guess )
		{
			hwAllocVoices_guess( *pmainwindow );
			gSndBufSize = SECONDARY_BUFFER_SIZE;
		}
		else if ( snd_isa3d_guess )
		{
			hwFreeVoices_guess();
		}
	}
}

/*
==============
SNDDMA_InitA3D_guess
===============
*/
sndinitstat SNDDMA_InitA3D_guess( void )
{
	HRESULT	hresult;
	DWORD	dwWrite;

	OutputDebugString( "Initializing Aureal A3D...\n" );

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->dmaspeed = 22050;

	Con_Printf( "Initializing Aureal A3D...\n" );

	gSndBufSize = SECONDARY_BUFFER_SIZE;
	hwSetMaxBufferSize_guess( gSndBufSize );
	hwSetBufferSize_guess( gSndBufSize );

	hresult = hwInit_guess( *pmainwindow, 32, shm->dmaspeed, shm->samplebits );

	if ( hresult >= 0 )
	{
		Con_Printf( "ok\n" );
		OutputDebugString( "A3D Initialized successfully\n" );

		if ( snd_firsttime )
			Con_DPrintf( "A3D Initialized successfully\n"
			             "   %d channel(s)\n"
			             "   %d bits/sample\n"
			             "   %d bytes/sec in software\n"
			             "   %d bytes/sec in hardware\n",
			             shm->channels, shm->samplebits, shm->speed, shm->dmaspeed );

		gSndBufSize = SECONDARY_BUFFER_SIZE;

		shm->samples = SECONDARY_BUFFER_SIZE / ( shm->samplebits / 8 );
		shm->samplepos = 0;
		shm->submission_chunk = 1;
		shm->buffer = NULL;
		sample16 = ( shm->samplebits / 8 ) - 1;

		hwStartSound_guess();
		hwGetCurrentPosition_guess( &mmstarttime.u.sample, &dwWrite );

		return SIS_SUCCESS;
	}

	if ( hresult == -5 )
		return SIS_NOTAUREAL_guess;

	gSndBufSize = 0;
	Con_Printf( "failed\n" );
	return SIS_FAILURE;
}

/*
==============
S_DisableA3D_guess

Console command s_disable_a3d: drops from A3D to DirectSound or waveOut
===============
*/
void S_DisableA3D_guess( void )
{
	sndinitstat	stat;

	if ( !snd_isa3d_guess )
	{
		Con_Printf( "A3D is not currently running.\n" );
		return;
	}

	sound_started = 0;
	SNDDMA_Shutdown();
	Q_memset( (void *)shm, 0, sizeof( dma_t ) );

	snd_isa3d_guess = dsound_init = wav_init = 0;

	stat = SIS_FAILURE;

	if ( !wavonly && WinNT )
	{
		stat = SNDDMA_InitDirect();

		if ( stat == SIS_SUCCESS )
		{
			snd_isdirect = true;
			sound_started = 1;

			if ( snd_firsttime )
				Con_Printf( "dsound init succeeded\n" );
		}
		else
		{
			snd_isdirect = false;
			Con_Printf( "*** dsound init failed ***\n" );
		}
	}

	if ( !dsound_init && stat != SIS_NOTAVAIL )
	{
		if ( snd_firsttime || snd_iswave )
		{
			snd_iswave = SNDDMA_InitWav();

			if ( snd_iswave )
			{
				sound_started = 1;

				if ( snd_firsttime )
					Con_Printf( "Wave sound init succeeded\n" );
			}
			else
			{
				Con_Printf( "Wave sound init failed\n" );
			}
		}
	}

	snd_firsttime = false;
	snd_buffer_count_guess = 1;

	if ( !snd_isa3d_guess && !dsound_init && !wav_init )
	{
		if ( snd_firsttime )
			Con_Printf( "*** No sound device initialized ***\n" );
	}
}

/*
==============
S_EnableA3D_guess

Console command s_enable_a3d
===============
*/
void S_EnableA3D_guess( void )
{
	sndinitstat	stat;

	if ( snd_isa3d_guess )
	{
		Con_Printf( "A3D already enabled\n" );
		return;
	}

	sound_started = 0;
	SNDDMA_Shutdown();
	Q_memset( (void *)shm, 0, sizeof( dma_t ) );

	dsound_init = wav_init = 0;

	stat = SIS_FAILURE;
	stat = SNDDMA_InitA3D_guess();

	if ( stat == SIS_SUCCESS )
	{
		Con_DPrintf( "Aureal A3D initialized\n" );
		snd_isa3d_guess = true;
		snd_isdirect = false;
		sound_started = 1;
	}
	else if ( stat == SIS_NOTAUREAL_guess )
	{
		Con_DPrintf( "Non-Aureal A3D.dll found.  This is an unsupported version A3D.\nDropping to Direct Sound\n" );
		snd_isa3d_guess = false;
		S_DisableA3D_guess();
	}
	else
	{
		Con_DPrintf( "Aureal A3D initialization failed.  Dropping to Direct Sound\n" );
		snd_isa3d_guess = false;
		S_DisableA3D_guess();
	}
}
