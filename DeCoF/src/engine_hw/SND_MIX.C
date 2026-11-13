// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "net.h"
#include "client.h"
#include "sound.h"
#include "winquake.h"
#include "eax.h"
#include "tmessage.h"
#include "Sequence.h"

#include <ctype.h>

int32	RandomLong( int32 lLow, int32 lHigh );

// voice data in the four channel EAX mix goes to its own buffer unless set
cvar_t	voice_eax_guess = { "voice_eax", "0" };
// divides the volume of all other sounds while net voice plays
float	g_SND_VoiceOverdrive = 1.0;

static int	s_careerAudioPaused = 0;

// set but never read (A3D mixing)
int		a3dpaint_guess = 0;

portable_samplepair_t	paintbuffer[( PAINTBUFFER_SIZE + 1 ) * 2];
// the rear speakers of the four channel EAX mix
portable_samplepair_t	rearpaintbuffer_guess[( PAINTBUFFER_SIZE + 1 ) * 2];
// net voice in the four channel EAX mix
portable_samplepair_t	voicepaintbuffer_guess[( PAINTBUFFER_SIZE + 1 ) * 2];
// the channels A3D plays itself, mixed here only to feed the room effects
portable_samplepair_t	a3dpaintbuffer_guess[( PAINTBUFFER_SIZE + 1 ) * 2];

int		snd_scaletable[32][256];
int		*snd_p, snd_linear_count, snd_vol;
short	*snd_out;

#define SOUND_11k		11025	// 11khz sample rate

void S_EAXTransferStereo16( int lpaintedtime, int endtime );
qboolean S_CheckWavEnd( channel_t *ch, sfxcache_t **psc, int ltime, int ichan );
void SND_PaintChannelFrom8( channel_t *ch, sfxcache_t *sc, int count );
void SND_PaintChannelFrom8Offs( portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int offset );
void SND_PaintChannelFrom16Offs( portable_samplepair_t *paintbuffer, channel_t *ch, short *sfx, int count, int offset, int bVoice );
qboolean SXDLY_Init( int idelay, float delay );
void SXDLY_Free( int idelay );
void SXRVB_CheckNewReverbVal( void );
void SXDLY_CheckNewDelayVal( void );
void SXDLY_CheckNewStereoDelayVal( void );
void SXRVB_DoAMod( int count );
void SXRVB_DoReverb( int count );
void SXDLY_DoDelay( int count );
void SXDLY_DoStereoDelay( int count );
void Wavstream_GetNextChunk( channel_t *ch, sfx_t *s );
void SND_MoveMouth( channel_t *ch, sfxcache_t *sc, int count );
int VOX_FPaintPitchChannelFrom8Offs( portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int pitch, int timecompress, int offset );
void VOX_ParseString( char *psz );
char *VOX_GetDirectory( char *szpath, char *psz );
int VOX_ParseWordParams( char *psz, voxword_t *pvoxword, int fFirst );
int VOX_IFindEmptySentence( void );

void SetCareerAudioState( int paused )
{
	s_careerAudioPaused = paused;
}

void CareerAudio_Command_f( void )
{
	SetCareerAudioState( 0 );
}

void S_TransferStereo16( int end )
{
	int		lpos;
	int		lpaintedtime;
	DWORD	*pbuf;
	int		endtime;
	int		reps;
	DWORD	dwSize, dwSize2;
	DWORD	*pbuf2;
	HRESULT	hresult;

	snd_vol = volume.value * 256;

	snd_p = (int *)paintbuffer;
	lpaintedtime = paintedtime << 1;
	endtime = end << 1;

	if ( snd_isa3d_guess && hwIsActive_guess() )
	{
		if ( hwLockBuffer_guess( &pbuf, &dwSize ) < 0 )
		{
			Con_DPrintf( "S_TransferStereo16: A3D::Lock MixBuffer Failed\n" );
			Con_DPrintf( "Restarting A3D engine...\n" );
			Con_Printf( "S_TransferStereo16: A3D::Lock MixBuffer Failed\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}
	else if ( snd_iseax_guess )
	{
		S_EAXTransferStereo16( lpaintedtime, endtime );
		return;
	}
	else if ( pDSBuf )
	{
		reps = 0;

		while ( ( hresult = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, &pbuf, &dwSize, &pbuf2, &dwSize2, 0 ) ) != DS_OK )
		{
			if ( hresult != DSERR_BUFFERLOST )
			{
				Con_Printf( "S_TransferStereo16: DS::Lock Sound Buffer Failed\n" );
				S_Shutdown();
				S_Startup();
				return;
			}

			if ( ++reps > 10000 )
			{
				Con_Printf( "S_TransferStereo16: DS: couldn't restore buffer\n" );
				S_Shutdown();
				S_Startup();
				return;
			}
		}
	}
	else
	{
		pbuf = (DWORD *)shm->buffer;
	}

	while ( lpaintedtime < endtime )
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ( ( shm->samples >> 1 ) - 1 );

		snd_out = (short *)pbuf + ( lpos << 1 );

		snd_linear_count = ( shm->samples >> 1 ) - lpos;
		if ( snd_linear_count > endtime - lpaintedtime )
			snd_linear_count = endtime - lpaintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		Snd_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		lpaintedtime += ( snd_linear_count >> 1 );
	}

	if ( snd_isa3d_guess )
		hwUnlockBuffer_guess( pbuf, dwSize );
	else if ( pDSBuf )
		pDSBuf->lpVtbl->Unlock( pDSBuf, pbuf, dwSize, NULL, 0 );
}

// writes the mix to the four EAX speaker buffers and the voice buffer
void S_EAXTransferStereo16( int lpaintedtime, int endtime )
{
	DWORD	*pBufFR2_guess;
	DWORD	*pbufFrontLeft_guess;
	DWORD	*pbufrl1_guess;
	int		val;
	short	*poutRL1_guess;
	short	*poutFR_guess;
	short	*poutFL_guess;
	DWORD	dwSizevoice_guess;
	int		reps;
	int		*pvoicebuf_guess;
	DWORD	*pbufvoice1_guess;
	short	*psndRR_guess;
	int		lpos;
	int		i;
	HRESULT	hresult;
	DWORD	dwsizeRL_guess;
	DWORD	dwSizeFrontRight_guess;
	DWORD	dwsizerr_guess;
	int		*prear_guess;
	DWORD	*pbufRR1_guess;
	short	*psndV_guess;
	DWORD	dwSizeFL_guess;

	reps = 0;
	while ( ( hresult = pDSBufFL_guess->lpVtbl->Lock( pDSBufFL_guess, 0, gSndBufSize / 2, &pbufFrontLeft_guess, &dwSizeFL_guess, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_Printf( "S_EAXTransferStereo16: DS::Lock Sound Buffer Failed EAX FL\n" );
			S_Shutdown();
			S_Startup();
			return;
		}

		if ( ++reps > 10000 )
		{
			Con_Printf( "S_EAXTransferStereo16: DS: couldn't restore buffer EAX FL\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}

	reps = 0;
	while ( ( hresult = pDSBufFR_guess->lpVtbl->Lock( pDSBufFR_guess, 0, gSndBufSize / 2, &pBufFR2_guess, &dwSizeFrontRight_guess, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_Printf( "S_EAXTransferStereo16: DS::Lock Sound Buffer Failed EAX FR\n" );
			S_Shutdown();
			S_Startup();
			return;
		}

		if ( ++reps > 10000 )
		{
			Con_Printf( "S_EAXTransferStereo16: DS: couldn't restore buffer EAX FR\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}

	reps = 0;
	while ( ( hresult = pDSBufRL_guess->lpVtbl->Lock( pDSBufRL_guess, 0, gSndBufSize / 2, &pbufrl1_guess, &dwsizeRL_guess, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_Printf( "S_EAXTransferStereo16: DS::Lock Sound Buffer Failed EAX RL\n" );
			S_Shutdown();
			S_Startup();
			return;
		}

		if ( ++reps > 10000 )
		{
			Con_Printf( "S_EAXTransferStereo16: DS: couldn't restore buffer EAX RL\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}

	reps = 0;
	while ( ( hresult = pDSBufRR_guess->lpVtbl->Lock( pDSBufRR_guess, 0, gSndBufSize / 2, &pbufRR1_guess, &dwsizerr_guess, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_Printf( "S_EAXTransferStereo16: DS::Lock Sound Buffer Failed EAX RR\n" );
			S_Shutdown();
			S_Startup();
			return;
		}

		if ( ++reps > 10000 )
		{
			Con_Printf( "S_EAXTransferStereo16: DS: couldn't restore buffer EAX RR\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}

	reps = 0;
	while ( ( hresult = pDSBufVoice_guess->lpVtbl->Lock( pDSBufVoice_guess, 0, gSndBufSize / 2, &pbufvoice1_guess, &dwSizevoice_guess, NULL, NULL, 0 ) ) != DS_OK )
	{
		if ( hresult != DSERR_BUFFERLOST )
		{
			Con_Printf( "S_EAXTransferStereo16: DS::Lock Sound Buffer Failed EAX Voice\n" );
			S_Shutdown();
			S_Startup();
			return;
		}

		if ( ++reps > 10000 )
		{
			Con_Printf( "S_EAXTransferStereo16: DS: couldn't restore buffer EAX Voice\n" );
			S_Shutdown();
			S_Startup();
			return;
		}
	}

	prear_guess = (int *)rearpaintbuffer_guess;
	pvoicebuf_guess = (int *)voicepaintbuffer_guess;

	while ( lpaintedtime < endtime )
	{
		// handle recirculating buffer issues
		lpos = lpaintedtime & ( ( shm->samples >> 1 ) - 1 );

		snd_linear_count = ( shm->samples >> 1 ) - lpos;
		if ( snd_linear_count > endtime - lpaintedtime )
			snd_linear_count = endtime - lpaintedtime;

		poutFL_guess = (short *)pbufFrontLeft_guess + lpos;
		poutFR_guess = (short *)pBufFR2_guess + lpos;
		poutRL1_guess = (short *)pbufrl1_guess + lpos;
		psndRR_guess = (short *)pbufRR1_guess + lpos;
		psndV_guess = (short *)pbufvoice1_guess + lpos;

		snd_linear_count <<= 1;

		for ( i = 0; i < snd_linear_count; i += 2 )
		{
			val = ( snd_p[i] * snd_vol ) >> 8;
			if ( val > 0x7fff )
				poutFL_guess[i >> 1] = 0x7fff;
			else if ( val < (short)0x8000 )
				poutFL_guess[i >> 1] = (short)0x8000;
			else
				poutFL_guess[i >> 1] = val;

			val = ( snd_p[i + 1] * snd_vol ) >> 8;
			if ( val > 0x7fff )
				poutFR_guess[i >> 1] = 0x7fff;
			else if ( val < (short)0x8000 )
				poutFR_guess[i >> 1] = (short)0x8000;
			else
				poutFR_guess[i >> 1] = val;

			val = ( prear_guess[i] * snd_vol ) >> 8;
			if ( val > 0x7fff )
				poutRL1_guess[i >> 1] = 0x7fff;
			else if ( val < (short)0x8000 )
				poutRL1_guess[i >> 1] = (short)0x8000;
			else
				poutRL1_guess[i >> 1] = val;

			val = ( prear_guess[i + 1] * snd_vol ) >> 8;
			if ( val > 0x7fff )
				psndRR_guess[i >> 1] = 0x7fff;
			else if ( val < (short)0x8000 )
				psndRR_guess[i >> 1] = (short)0x8000;
			else
				psndRR_guess[i >> 1] = val;

			val = ( pvoicebuf_guess[i] * snd_vol ) >> 8;
			if ( val > 0x7fff )
				psndV_guess[i >> 1] = 0x7fff;
			else if ( val < (short)0x8000 )
				psndV_guess[i >> 1] = (short)0x8000;
			else
				psndV_guess[i >> 1] = val;
		}

		snd_p += snd_linear_count;
		prear_guess += snd_linear_count;
		pvoicebuf_guess += snd_linear_count;
		lpaintedtime += ( snd_linear_count >> 1 );
	}

	pDSBufFL_guess->lpVtbl->Unlock( pDSBufFL_guess, pbufFrontLeft_guess, dwSizeFL_guess, NULL, 0 );
	pDSBufFR_guess->lpVtbl->Unlock( pDSBufFR_guess, pBufFR2_guess, dwSizeFrontRight_guess, NULL, 0 );
	pDSBufRL_guess->lpVtbl->Unlock( pDSBufRL_guess, pbufrl1_guess, dwsizeRL_guess, NULL, 0 );
	pDSBufRR_guess->lpVtbl->Unlock( pDSBufRR_guess, pbufRR1_guess, dwsizerr_guess, NULL, 0 );
	pDSBufVoice_guess->lpVtbl->Unlock( pDSBufVoice_guess, pbufvoice1_guess, dwSizevoice_guess, NULL, 0 );
}

void S_TransferPaintBuffer( int endtime )
{
	S_TransferStereo16( endtime );
}

void S_FreeChannel( channel_t *ch )
{
	if ( ch->entchannel == CHAN_STREAM )
	{
		int		i;

		i = ( (byte *)ch - (byte *)channels ) / sizeof( channel_t );
		Wavstream_Close( i );
		Cache_Free( &ch->sfx->cache );
	}
	else if ( ch->entchannel >= CHAN_NETWORKVOICE_BASE && ch->entchannel <= CHAN_NETWORKVOICE_END )
	{
		VoiceSE_NotifyFreeChannel( ch->entchannel );
	}

	if ( ch->isentence >= 0 )
		rgrgvoxword[ch->isentence][0].sfx = NULL;

	ch->isentence = -1;
	ch->sfx = NULL;

	SND_CloseMouth( ch );
}

//===============================================================================
// Mix sounds into paintbuffer
//===============================================================================

// return true if we've reached the end of the current wave, and the channel was freed
qboolean S_CheckWavEnd( channel_t *ch, sfxcache_t **psc, int ltime, int ichan )
{
	sfxcache_t	*sc;

	sc = *psc;

	// voice data never ends here
	if ( sc->length == 0x40000000 )
		return false;

	if ( sc->loopstart >= 0 )
	{
		ch->pos = sc->loopstart;
		ch->end = ltime + sc->length - ch->pos;

		if ( ch->isentence >= 0 )
			rgrgvoxword[ch->isentence][ch->iword].samplefrac = 0;

		return false;
	}

	if ( ch->entchannel == CHAN_STREAM )
	{
		if ( wavstreams[ichan].csamplesplayed >= wavstreams[ichan].info.samples )
		{
			S_FreeChannel( ch );
			if ( snd_isa3d_guess )
				hwStopChannel_guess( ichan );
			return true;
		}

		Wavstream_GetNextChunk( ch, ch->sfx );
		ch->pos = 0;
		ch->end = ltime + sc->length;
		return false;
	}

	if ( ch->isentence >= 0 )
	{
		sfx_t	*sfx;

		sfx = rgrgvoxword[ch->isentence][ch->iword].sfx;
		if ( sfx && !rgrgvoxword[ch->isentence][ch->iword].fKeepCached )
			Cache_Free( &sfx->cache );

		sfx = rgrgvoxword[ch->isentence][ch->iword + 1].sfx;
		sc = NULL;
		ch->sfx = sfx;

		if ( sfx )
		{
			sc = S_LoadSound( sfx, ch );
			*psc = sc;
		}

		if ( !sc || !sfx )
		{
			S_FreeChannel( ch );
			return true;
		}

		if ( snd_isa3d_guess )
		{
			int		state_guess;
			void	*hvoice_guess;

			state_guess = hwGetChannelState_guess( ichan );
			hvoice_guess = hwAllocChannel_guess( ichan, 1 );
			if ( hvoice_guess )
			{
				hwSetVoiceRelative_guess( hvoice_guess, cl_entities[ch->entnum].curstate.movetype == MOVETYPE_NONE );
				hwSetChannelDistance_guess( ichan, sound_nominal_clip_dist * ch->dist_mult );

				switch ( state_guess )
				{
				case 0:
					hwSetChannelState_guess( ichan, 5 );
					break;
				case 1:
					hwSetChannelState_guess( ichan, 5 );
					break;
				case 2:
				case 6:
					hwSetChannelState_guess( ichan, 6 );
					break;
				case 3:
					hwSetChannelState_guess( ichan, 7 );
					break;
				default:
					hwSetChannelState_guess( ichan, 5 );
					break;
				}
			}
		}

		ch->pos = 0;
		ch->end = ltime + sc->length;
		ch->iword++;
		VOX_TrimStartEndTimes( ch, sc );
		return false;
	}

	S_FreeChannel( ch );
	return true;
}

void S_MixChannelsToPaintbuffer( int end, int fPaintHiresSounds, int voiceOnly )
{
	int			i;
	channel_t	*ch;
	sfxcache_t	*sc;
	int			ltime, count;
	int			pitch;
	int			timecompress;
	int			fhitend;
	int			hires;
	int			offset;
	int			chend;
	int			bVoice;
	portable_samplepair_t	*pout;
	int			fA3DChannel_guess;

	hires = ( fPaintHiresSounds != 0 );

	for ( ch = channels, i = 0; i < total_channels; i++, ch++ )
	{
		fA3DChannel_guess = 0;

		if ( snd_isa3d_guess )
		{
			a3dpaint_guess = 0;
			pitch = -100;

			if ( ch->sfx )
			{
				sc = S_LoadSound( ch->sfx, ch );
				if ( sc )
				{
					if ( fPaintHiresSounds )
					{
						if ( sc->speed <= shm->speed )
							continue;
					}
					else
					{
						if ( sc->speed != shm->speed )
							continue;
					}

					pitch = ch->pitch;
					if ( ch->isentence >= 0 )
					{
						if ( rgrgvoxword[ch->isentence][ch->iword].pitch > 0 )
							pitch = rgrgvoxword[ch->isentence][ch->iword].pitch + pitch - 100;
						timecompress = rgrgvoxword[ch->isentence][ch->iword].timecompress;
					}
					else
					{
						timecompress = 0;
					}
				}
			}

			if ( hwPaintChannel_guess( i, ch, sc, 0, 0, pitch / 100.0f ) )
				fA3DChannel_guess = 1;
		}

		if ( !ch->sfx )
			continue;

		sc = S_LoadSound( ch->sfx, ch );

		bVoice = ( sc->length == 0x40000000 );

		if ( bVoice )
		{
			if ( !voiceOnly )
				continue;
		}
		else
		{
			if ( voiceOnly )
				continue;
		}

		if ( !ch->leftvol && !ch->rightvol && !ch->rleftvol_guess && !ch->rrightvol_guess )
		{
			// sentences must be advanced even when not heard
			if ( sc->loopstart >= 0 )
				continue;

			S_FreeChannel( ch );
			continue;
		}

		if ( !sc )
			continue;

		if ( fPaintHiresSounds )
		{
			if ( sc->speed <= shm->speed )
				continue;
		}
		else
		{
			if ( sc->speed != shm->speed )
				continue;
		}

		ltime = paintedtime;
		pitch = ch->pitch;

		if ( ch->isentence >= 0 )
		{
			if ( rgrgvoxword[ch->isentence][ch->iword].pitch > 0 )
				pitch = rgrgvoxword[ch->isentence][ch->iword].pitch + pitch - 100;
			timecompress = rgrgvoxword[ch->isentence][ch->iword].timecompress;
		}
		else
		{
			timecompress = 0;
		}

		fhitend = 0;

		while ( !fhitend && ltime < end )
		{
			if ( hires )
				chend = ch->end - sc->length + ( sc->length >> 1 );
			else
				chend = ch->end;

			if ( chend < end )
				count = chend - ltime;
			else
				count = end - ltime;

			// voice data must be 16 bit mono
			if ( bVoice && ( sc->width != 2 || sc->stereo ) )
			{
				fhitend = 1;
				continue;
			}

			count <<= hires;
			offset = ltime - paintedtime;
			offset <<= hires;

			if ( count > 0 && sc->width == 1 && ch->entnum > 0 && ( ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM ) )
				SND_MoveMouth( ch, sc, count );

			if ( count > 0 )
			{
				if ( fA3DChannel_guess )
					pout = a3dpaintbuffer_guess;
				else if ( voiceOnly && snd_iseax_guess && !(int)voice_eax_guess.value )
					pout = voicepaintbuffer_guess;
				else
					pout = paintbuffer;

				if ( fA3DChannel_guess && s_verbwet.value < 0.001f )
				{
					// A3D plays it and no reverb is wanted: just advance
					ch->pos += count;
				}
				else
				{
					int		oldleft;
					int		oldright;

					oldleft = ch->leftvol;
					oldright = ch->rightvol;

					if ( !bVoice )
					{
						ch->leftvol = ch->leftvol / g_SND_VoiceOverdrive;
						ch->rightvol = ch->rightvol / g_SND_VoiceOverdrive;
					}

					if ( sc->width == 1 )
					{
						if ( ( pitch != 100 || timecompress ) && ch->isentence >= 0 )
						{
							fhitend = VOX_FPaintPitchChannelFrom8Offs( pout, ch, sc, count, pitch, timecompress, offset );
						}
						else if ( ltime == paintedtime )
						{
							if ( snd_iseax_guess )
								SND_PaintChannelFrom8( ch, sc, count );
							else if ( pout != paintbuffer )
								SND_PaintChannelFrom8A3D_guess( ch, sc, count );
							else
								SND_PaintChannelFrom8Asm_guess( ch, sc, count );
						}
						else
						{
							SND_PaintChannelFrom8Offs( pout, ch, sc, count, offset );
						}
					}
					else
					{
						char	copyBuf[4096];
						short	*pSrcData;

						if ( bVoice )
						{
							pSrcData = (short *)copyBuf;
							count = ( (int (*)( sfxcache_t *, char *, int, int, int ))sc->loopstart )( sc, copyBuf, sizeof( copyBuf ), ch->pos, count );
						}
						else
						{
							pSrcData = (short *)&sc->data[ch->pos * 2];
						}

						SND_PaintChannelFrom16Offs( pout, ch, pSrcData, count, offset, sc->length == 0x40000000 );
					}

					ch->leftvol = oldleft;
					ch->rightvol = oldright;
				}

				ltime += count >> hires;

				if ( ch->entchannel == CHAN_STREAM )
					wavstreams[i].csamplesplayed += count;
			}

			// if at end of loop, restart
			if ( fhitend || ltime >= chend )
			{
				fhitend = 0;
				if ( S_CheckWavEnd( ch, &sc, ltime, i ) )
					break;
			}
		}
	}
}

void S_PaintChannels( int endtime )
{
	int		end;
	int		count;
	static portable_samplepair_t	paintprev;
	static portable_samplepair_t	rearpaintprev;
	static portable_samplepair_t	a3dpaintprev_guess;

	while ( paintedtime < endtime )
	{
		// if paintbuffer is smaller than DMA buffer
		end = endtime;
		if ( endtime - paintedtime > PAINTBUFFER_SIZE )
			end = paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer
		count = end - paintedtime;
		Q_memset( paintbuffer, 0, count * sizeof( portable_samplepair_t ) );

		if ( snd_iseax_guess )
		{
			Q_memset( rearpaintbuffer_guess, 0, count * sizeof( portable_samplepair_t ) );
			Q_memset( voicepaintbuffer_guess, 0, count * sizeof( portable_samplepair_t ) * 2 );
		}

		if ( s_verbwet.value >= 0.001f )
			Q_memset( a3dpaintbuffer_guess, 0, count * sizeof( portable_samplepair_t ) );

		// paint in the channels.
		if ( !cl.paused && !s_careerAudioPaused )
			S_MixChannelsToPaintbuffer( end, 0, 0 );

		if ( 1 )
		{
			int		i, j;
			int		count2x;

			// upsample all 11khz buffer by 2x
			count2x = count << 1;

			if ( hisound.value == 0 )
				SX_RoomFX( count, 1, 1 );

			if ( snd_isa3d_guess && s_verbwet.value >= 0.001f )
			{
				for ( i = count2x - 1, j = count - 1; j > 0; j-- )
				{
					paintbuffer[i] = paintbuffer[j];
					paintbuffer[i - 1].left = ( paintbuffer[j].left + paintbuffer[j - 1].left ) >> 1;
					paintbuffer[i - 1].right = ( paintbuffer[j].right + paintbuffer[j - 1].right ) >> 1;

					a3dpaintbuffer_guess[i] = a3dpaintbuffer_guess[j];
					a3dpaintbuffer_guess[i - 1].left = ( a3dpaintbuffer_guess[j].left + a3dpaintbuffer_guess[j - 1].left ) >> 1;
					a3dpaintbuffer_guess[i - 1].right = ( a3dpaintbuffer_guess[j].right + a3dpaintbuffer_guess[j - 1].right ) >> 1;

					i -= 2;
				}

				paintbuffer[1] = paintbuffer[0];
				paintbuffer[0].left = ( paintprev.left + paintbuffer[0].left ) >> 1;
				paintbuffer[0].right = ( paintprev.right + paintbuffer[0].right ) >> 1;

				a3dpaintbuffer_guess[1] = a3dpaintbuffer_guess[0];
				a3dpaintbuffer_guess[0].left = ( a3dpaintprev_guess.left + a3dpaintbuffer_guess[0].left ) >> 1;
				a3dpaintbuffer_guess[0].right = ( a3dpaintprev_guess.right + a3dpaintbuffer_guess[0].right ) >> 1;

				a3dpaintprev_guess = a3dpaintbuffer_guess[count2x - 1];
				paintprev = paintbuffer[count2x - 1];
			}
			else
			{
				for ( i = count2x - 1, j = count - 1; j > 0; j-- )
				{
					paintbuffer[i] = paintbuffer[j];
					paintbuffer[i - 1].left = ( paintbuffer[j].left + paintbuffer[j - 1].left ) >> 1;
					paintbuffer[i - 1].right = ( paintbuffer[j].right + paintbuffer[j - 1].right ) >> 1;

					if ( snd_iseax_guess )
					{
						rearpaintbuffer_guess[i] = rearpaintbuffer_guess[j];
						rearpaintbuffer_guess[i - 1].left = ( rearpaintbuffer_guess[j].left + rearpaintbuffer_guess[j - 1].left ) >> 1;
						rearpaintbuffer_guess[i - 1].right = ( rearpaintbuffer_guess[j].right + rearpaintbuffer_guess[j - 1].right ) >> 1;
					}

					i -= 2;
				}

				paintbuffer[1] = paintbuffer[0];
				paintbuffer[0].left = ( paintprev.left + paintbuffer[0].left ) >> 1;
				paintbuffer[0].right = ( paintprev.right + paintbuffer[0].right ) >> 1;

				if ( snd_iseax_guess )
				{
					rearpaintbuffer_guess[1] = rearpaintbuffer_guess[0];
					rearpaintbuffer_guess[0].left = ( rearpaintprev.left + rearpaintbuffer_guess[0].left ) >> 1;
					rearpaintbuffer_guess[0].right = ( rearpaintprev.right + rearpaintbuffer_guess[0].right ) >> 1;

					rearpaintprev = rearpaintbuffer_guess[count2x - 1];
				}

				paintprev = paintbuffer[count2x - 1];
			}

			if ( hisound.value > 0 )
			{
				// paint in the 22khz sounds
				if ( !cl.paused && !s_careerAudioPaused )
					S_MixChannelsToPaintbuffer( end, 1, 0 );

				SX_RoomFX( count2x, 1, 1 );
			}

			// paint in the voice data last
			if ( !cl.paused && !s_careerAudioPaused )
				S_MixChannelsToPaintbuffer( end, 1, 1 );
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		paintedtime = end;
	}
}

void SND_InitScaletable( void )
{
	int		i, j;

	for ( i = 0; i < 32; i++ )
		for ( j = 0; j < 256; j++ )
			snd_scaletable[i][j] = ( (signed char)j ) * i * 8;
}

void SND_PaintChannelFrom8( channel_t *ch, sfxcache_t *sc, int count )
{
	int		data;
	int		*lscale, *rscale, *rlscale, *rrscale;
	unsigned char	*sfx;
	int		i;

	if ( ch->leftvol > 255 )
		ch->leftvol = 255;
	if ( ch->rightvol > 255 )
		ch->rightvol = 255;
	if ( ch->rleftvol_guess > 255 )
		ch->rleftvol_guess = 255;
	if ( ch->rrightvol_guess > 255 )
		ch->rrightvol_guess = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	rlscale = snd_scaletable[ch->rleftvol_guess >> 3];
	rrscale = snd_scaletable[ch->rrightvol_guess >> 3];
	sfx = (unsigned char *)sc->data + ch->pos;

	for ( i = 0; i < count; i++ )
	{
		data = sfx[i];
		paintbuffer[i].left += lscale[data];
		paintbuffer[i].right += rscale[data];
		rearpaintbuffer_guess[i].left += rlscale[data];
		rearpaintbuffer_guess[i].right += rrscale[data];
	}

	ch->pos += count;
}

void SND_PaintChannelFrom8Offs( portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int offset )
{
	int		data;
	int		*lscale, *rscale;
	unsigned char	*sfx;
	int		i;
	portable_samplepair_t	*pbuffer;
	portable_samplepair_t	*prear_guess;
	int		*rlscale, *rrscale;

	if ( snd_iseax_guess )
	{
		prear_guess = &rearpaintbuffer_guess[offset];

		if ( ch->rleftvol_guess > 255 )
			ch->rleftvol_guess = 255;
		if ( ch->rrightvol_guess > 255 )
			ch->rrightvol_guess = 255;

		rlscale = snd_scaletable[ch->rleftvol_guess >> 3];
		rrscale = snd_scaletable[ch->rrightvol_guess >> 3];
	}

	pbuffer = &paintbuffer[offset];

	if ( ch->leftvol > 255 )
		ch->leftvol = 255;
	if ( ch->rightvol > 255 )
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = (unsigned char *)sc->data + ch->pos;

	for ( i = 0; i < count; i++ )
	{
		data = sfx[i];
		pbuffer[i].left += lscale[data];
		pbuffer[i].right += rscale[data];

		if ( snd_iseax_guess )
		{
			prear_guess[i].left += rlscale[data];
			prear_guess[i].right += rrscale[data];
		}
	}

	ch->pos += count;
}

void SND_PaintChannelFrom16Offs( portable_samplepair_t *paintbuffer, channel_t *ch, short *sfx, int count, int offset, int bVoice )
{
	int		data;
	int		left, right;
	int		fRear_guess;
	int		leftvol, rightvol;
	portable_samplepair_t	*prear_guess;
	int		rleftvol, rrightvol;
	int		i;
	portable_samplepair_t	*pbuffer;

	fRear_guess = snd_iseax_guess && !bVoice;

	if ( fRear_guess )
	{
		prear_guess = &rearpaintbuffer_guess[offset];
		rleftvol = ch->rleftvol_guess;
		rrightvol = ch->rrightvol_guess;
	}

	pbuffer = &paintbuffer[offset];
	leftvol = ch->leftvol;
	rightvol = ch->rightvol;

	for ( i = 0; i < count; i++ )
	{
		data = sfx[i];
		left = ( data * leftvol ) >> 8;
		right = ( data * rightvol ) >> 8;
		pbuffer[i].left += left;
		pbuffer[i].right += right;

		if ( fRear_guess )
		{
			left = ( data * rleftvol ) >> 8;
			right = ( data * rrightvol ) >> 8;
			prear_guess[i].left += left;
			prear_guess[i].right += right;
		}
	}

	ch->pos += count;
}

//===============================================================================
// DSP room effects: modulation, lowpass, reverb and delay
//===============================================================================

typedef short sample_t;

typedef struct dlyline_s
{
	int		cdelaysamplesmax;	// size of delay line in samples
	int		lp;					// lowpass flag 0 = off, 1 = on

	int		idelayinput;		// i/o indices into circular delay line
	int		idelayoutput;

	int		idelayoutputxf;		// crossfade output pointer
	int		xfade;				// crossfade value

	int		delaysamples;		// current delay setting
	int		delayfeed;			// current feedback setting

	int		lp0, lp1, lp2;		// lowpass filter buffer

	int		mod;				// sample modulation count
	int		modcur;

	HANDLE	hdelayline;			// handle to delay line buffer
	sample_t	*lpdelayline;	// buffer
} dlyline_t;

// rgsxdly[0] mono delay, [1] and [2] the two reverb lines, [3] stereo delay
dlyline_t	rgsxdly[4];
int			rgsxlp[10];			// lowpass filter buffer of the room modulation

int			sxamodl, sxamodr;	// amplitude modulation values
int			sxamodlt, sxamodrt;	// modulation targets
int			sxmod1, sxmod2;
int			sxmod1cur, sxmod2cur;

// mono delay
cvar_t	sxdly_delay = { "room_delay", "0" };		// current delay in seconds
cvar_t	sxdly_feedback = { "room_feedback", "0.2" };	// cyles
cvar_t	sxdly_lp = { "room_dlylp", "1.0" };			// lowpass filter

float	sxdly_delayprev;		// previous delay setting value

// mono reverb
cvar_t	sxrvb_size = { "room_size", "0" };
cvar_t	sxrvb_feedback = { "room_refl", "0.7" };
cvar_t	sxrvb_lp = { "room_rvblp", "1.0" };

float	sxrvb_sizeprev;

// stereo delay (no feedback)
cvar_t	sxste_delay = { "room_left", "0" };

float	sxste_delayprev;

// underwater/special fx modulations
cvar_t	sxmod_lowpass = { "room_lp", "0" };
cvar_t	sxmod_mod = { "room_mod", "0" };

// main interface
cvar_t	sxroom_type = { "room_type", "0" };
cvar_t	sxroomwater_type = { "waterroom_type", "14" };
float	sxroom_typeprev;

cvar_t	sxroom_off = { "room_off", "0" };

int		sxhires = 0;
int		sxhiresprev = 0;

void SX_Init( void )
{
	Q_memset( rgsxdly, 0, sizeof( dlyline_t ) * 4 );
	Q_memset( rgsxlp, 0, sizeof( int ) * 10 );

	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;
	sxroom_typeprev = -1.0;

	// flag hires sound mode
	sxhires = ( hisound.value == 0 ) ? 0 : 1;
	sxhiresprev = sxhires;

	// init amplitude modulation params
	sxamodl = sxamodr = 255;
	sxamodlt = sxamodrt = 255;

	if ( shm )
	{
		sxmod1 = 350 * ( shm->speed / SOUND_11k );	// 11k was the original sample rate all dsp was tuned at
		sxmod2 = 450 * ( shm->speed / SOUND_11k );
	}

	sxmod1cur = sxmod1;
	sxmod2cur = sxmod2;

	Cvar_RegisterVariable( &voice_eax_guess );
	Cvar_RegisterVariable( &sxdly_delay );
	Cvar_RegisterVariable( &sxdly_feedback );
	Cvar_RegisterVariable( &sxdly_lp );
	Cvar_RegisterVariable( &sxrvb_size );
	Cvar_RegisterVariable( &sxrvb_feedback );
	Cvar_RegisterVariable( &sxrvb_lp );
	Cvar_RegisterVariable( &sxste_delay );
	Cvar_RegisterVariable( &sxmod_lowpass );
	Cvar_RegisterVariable( &sxmod_mod );
	Cvar_RegisterVariable( &sxroom_type );
	Cvar_RegisterVariable( &sxroomwater_type );
	Cvar_RegisterVariable( &sxroom_off );
}

// free memory allocated for sound fx
void SX_Free( void )
{
	int		i;

	// release mono delay line
	SXDLY_Free( 0 );

	// release reverb lines
	for ( i = 0; i < 2; i++ )
		SXDLY_Free( i + 1 );

	SXDLY_Free( 3 );
}

// allocate a new delay line buffer if necessary
qboolean SXDLY_Init( int idelay, float delay )
{
	int			cbsamples;
	HANDLE		hData;
	HPSTR		lpData;
	dlyline_t	*pdly;

	pdly = &rgsxdly[idelay];

	if ( delay > 0.4 )
		delay = 0.4;

	if ( pdly->lpdelayline )
	{
		GlobalUnlock( pdly->hdelayline );
		GlobalFree( pdly->hdelayline );
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;
	}

	if ( delay == 0.0 )
		return true;

	pdly->cdelaysamplesmax = shm->speed * delay;
	pdly->cdelaysamplesmax <<= sxhires;
	pdly->cdelaysamplesmax += 1;

	cbsamples = pdly->cdelaysamplesmax * sizeof( sample_t );

	hData = GlobalAlloc( GMEM_MOVEABLE | GMEM_SHARE, cbsamples );
	if ( !hData )
	{
		Con_SafePrintf( "Sound FX: Out of memory.\n" );
		return false;
	}

	lpData = GlobalLock( hData );
	if ( !lpData )
	{
		Con_SafePrintf( "Sound FX: Failed to lock.\n" );
		GlobalFree( hData );
		return false;
	}

	Q_memset( lpData, 0, cbsamples );

	pdly->hdelayline = hData;
	pdly->lpdelayline = (sample_t *)lpData;

	// init delay loop input and output counters.
	// NOTE: init of idelayoutput only valid if pdly->delaysamples is set
	// NOTE: before this call!
	pdly->idelayinput = 0;
	pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;
	pdly->xfade = 0;
	pdly->lp = 1;
	pdly->mod = 0;
	pdly->modcur = 0;

	// init lowpass filter memory
	pdly->lp0 = pdly->lp1 = pdly->lp2 = 0;

	return true;
}

// release delay buffer and deactivate delay
void SXDLY_Free( int idelay )
{
	dlyline_t	*pdly;

	pdly = &rgsxdly[idelay];

	if ( pdly->lpdelayline )
	{
		GlobalUnlock( pdly->hdelayline );
		GlobalFree( pdly->hdelayline );
		pdly->hdelayline = NULL;
		pdly->lpdelayline = NULL;
	}
}

// check for new stereo delay param
void SXDLY_CheckNewStereoDelayVal( void )
{
	dlyline_t	*pdly = &rgsxdly[3];
	int			delaysamples;

	// set up stereo delay
	if ( sxste_delay.value != sxste_delayprev )
	{
		if ( sxste_delay.value == 0.0 )
		{
			// deactivate delay line
			SXDLY_Free( 3 );
			sxste_delayprev = 0.0;
		}
		else
		{
			delaysamples = min( sxste_delay.value, 0.1 ) * shm->speed;
			delaysamples <<= sxhires;

			// init delay line if not active
			if ( !pdly->lpdelayline )
			{
				pdly->delaysamples = delaysamples;
				SXDLY_Init( 3, 0.1 );
			}

			// do crossfade to new delay if delay has changed
			if ( delaysamples != pdly->delaysamples )
			{
				// set up crossfade from old pdly->delaysamples to new delaysamples
				pdly->idelayoutputxf = pdly->idelayinput - delaysamples;
				if ( pdly->idelayoutputxf < 0 )
					pdly->idelayoutputxf += pdly->cdelaysamplesmax;
				pdly->xfade = 128;
			}

			sxste_delayprev = sxste_delay.value;

			// UNDONE: modulation disabled
			pdly->mod = 0;
			pdly->modcur = pdly->mod;

			// deactivate line if rounded down to 0 delay
			if ( pdly->delaysamples == 0 )
				SXDLY_Free( 3 );
		}
	}
}

// stereo delay, left channel only, no feedback
void SXDLY_DoStereoDelay( int count )
{
	int			left;
	sample_t	sampledly;
	sample_t	samplexf;
	portable_samplepair_t	*pbuf;
	int			countr;

	// process delay line if active
	if ( rgsxdly[3].lpdelayline )
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...
		while ( countr-- )
		{
			if ( rgsxdly[3].mod && ( --rgsxdly[3].modcur < 0 ) )
				rgsxdly[3].modcur = rgsxdly[3].mod;

			// get delay line sample from left line
			sampledly = rgsxdly[3].lpdelayline[rgsxdly[3].idelayoutput];
			left = pbuf->left;

			// only process if left value or delayline value are non-zero or xfading
			if ( rgsxdly[3].xfade || sampledly || left )
			{
				// if we're not crossfading, and we're not modulating, but we'd like to be modulating,
				// then setup a new crossfade.
				if ( !rgsxdly[3].xfade && !rgsxdly[3].modcur && rgsxdly[3].mod )
				{
					// set up crossfade to new delay value, if we're not already doing an xfade
					rgsxdly[3].idelayoutputxf = rgsxdly[3].idelayoutput + ( ( RandomLong( 0, 255 ) * rgsxdly[3].delaysamples ) >> 9 );

					if ( rgsxdly[3].idelayoutputxf >= rgsxdly[3].cdelaysamplesmax )
						rgsxdly[3].idelayoutputxf -= rgsxdly[3].cdelaysamplesmax;

					rgsxdly[3].xfade = 128;
				}

				// modify sampledly if crossfading to new delay value
				if ( rgsxdly[3].xfade )
				{
					samplexf = ( rgsxdly[3].lpdelayline[rgsxdly[3].idelayoutputxf] * ( 128 - rgsxdly[3].xfade ) ) >> 7;
					sampledly = ( ( sampledly * rgsxdly[3].xfade ) >> 7 ) + samplexf;

					if ( ++rgsxdly[3].idelayoutputxf >= rgsxdly[3].cdelaysamplesmax )
						rgsxdly[3].idelayoutputxf = 0;

					if ( --rgsxdly[3].xfade == 0 )
						rgsxdly[3].idelayoutput = rgsxdly[3].idelayoutputxf;
				}

				// save output value into delay line
				if ( left > 32767 )
					left = 32767;
				else if ( left < -32768 )
					left = -32768;

				rgsxdly[3].lpdelayline[rgsxdly[3].idelayinput] = left;

				// save delay line sample into output buffer
				pbuf->left = sampledly;
			}
			else
			{
				// keep clearing out delay line, even if no signal in or out
				rgsxdly[3].lpdelayline[rgsxdly[3].idelayinput] = 0;
			}

			// update delay buffer pointers
			if ( ++rgsxdly[3].idelayinput >= rgsxdly[3].cdelaysamplesmax )
				rgsxdly[3].idelayinput = 0;

			if ( ++rgsxdly[3].idelayoutput >= rgsxdly[3].cdelaysamplesmax )
				rgsxdly[3].idelayoutput = 0;

			pbuf++;
		}
	}
}

// If sxdly_delay or sxdly_feedback have changed, update delaysamples
// and delayfeed values. This applies only to delay 0, the main echo line.
void SXDLY_CheckNewDelayVal( void )
{
	dlyline_t	*pdly = &rgsxdly[0];

	if ( sxdly_delay.value != sxdly_delayprev )
	{
		if ( sxdly_delay.value == 0.0 )
		{
			// deactivate delay line
			SXDLY_Free( 0 );
			sxdly_delayprev = sxdly_delay.value;
		}
		else
		{
			// compute new delay samples
			pdly->delaysamples = min( sxdly_delay.value, 0.4 ) * shm->speed;
			pdly->delaysamples <<= sxhires;

			// init delay line if not active
			if ( !pdly->lpdelayline )
				SXDLY_Init( 0, 0.4 );

			// flush delay line and filters
			if ( pdly->lpdelayline )
			{
				Q_memset( pdly->lpdelayline, 0, pdly->cdelaysamplesmax * sizeof( sample_t ) );
				pdly->lp0 = 0;
				pdly->lp1 = 0;
				pdly->lp2 = 0;
			}

			// init delay loop input and output counters
			pdly->idelayinput = 0;
			pdly->idelayoutput = pdly->cdelaysamplesmax - pdly->delaysamples;

			sxdly_delayprev = sxdly_delay.value;

			// deactivate line if rounded down to 0 delay
			if ( pdly->delaysamples == 0 )
				SXDLY_Free( 0 );
		}
	}

	pdly->lp = sxdly_lp.value;
	pdly->delayfeed = sxdly_feedback.value * 255;
}

// This routine updates both left and right output with
// the mono delayed signal. Delay is set through console vars room_delay
// and room_feedback.
void SXDLY_DoDelay( int count )
{
	int			val;
	int			valt;
	int			left;
	int			right;
	sample_t	sampledly;
	portable_samplepair_t	*pbuf;
	int			countr;

	// process mono delay line if active
	if ( rgsxdly[0].lpdelayline )
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...
		while ( countr-- )
		{
			// get delay line sample
			sampledly = rgsxdly[0].lpdelayline[rgsxdly[0].idelayoutput];

			left = pbuf->left;
			right = pbuf->right;

			// only process if delay line and paintbuffer samples are non zero
			if ( sampledly || left || right )
			{
				// get current sample from delay buffer

				// calculate delayed value from avg of left and right channels
				val = ( ( left + right ) >> 1 ) + ( ( rgsxdly[0].delayfeed * sampledly ) >> 8 );

				// limit val to short
				if ( val > 32767 )
					val = 32767;
				else if ( val < -32768 )
					val = -32768;

				// lowpass
				if ( rgsxdly[0].lp )
				{
					valt = ( rgsxdly[0].lp0 + rgsxdly[0].lp1 + ( val << 1 ) ) >> 2;
					rgsxdly[0].lp0 = rgsxdly[0].lp1;
					rgsxdly[0].lp1 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer
				rgsxdly[0].lpdelayline[rgsxdly[0].idelayinput] = valt;

				// decrease output value by max gain of delay with feedback
				// to provide for unity gain reverb
				// note: this gain varies with the feedback value.
				valt >>= 2;

				// mono delay in left and right channels
				left += valt;
				right += valt;

				// limit to short
				if ( left > 32767 )
					left = 32767;
				else if ( left < -32768 )
					left = -32768;

				if ( right > 32767 )
					right = 32767;
				else if ( right < -32768 )
					right = -32768;

				pbuf->left = left;
				pbuf->right = right;
			}
			else
			{
				// not playing samples, but must still flush lowpass buffer and delay line
				rgsxdly[0].lp0 = rgsxdly[0].lp1 = 0;
				rgsxdly[0].lpdelayline[rgsxdly[0].idelayinput] = 0;
			}

			// update delay buffer pointers
			if ( ++rgsxdly[0].idelayinput >= rgsxdly[0].cdelaysamplesmax )
				rgsxdly[0].idelayinput = 0;

			if ( ++rgsxdly[0].idelayoutput >= rgsxdly[0].cdelaysamplesmax )
				rgsxdly[0].idelayoutput = 0;

			pbuf++;
		}
	}
}

// Check for a parameter change on the reverb processor
void SXRVB_CheckNewReverbVal( void )
{
	dlyline_t	*pdly;
	int			delaysamples;
	int			i;
	int			mod;

	if ( sxrvb_size.value != sxrvb_sizeprev )
	{
		sxrvb_sizeprev = sxrvb_size.value;

		if ( sxrvb_size.value == 0.0 )
		{
			// deactivate all delay lines
			SXDLY_Free( 1 );
			SXDLY_Free( 2 );
		}
		else
		{
			for ( i = 1; i < 1 + 2; i++ )
			{
				// init delay line if not active
				pdly = &rgsxdly[i];

				switch ( i )
				{
				case 1:
					delaysamples = min( sxrvb_size.value, 0.1 ) * shm->speed;
					pdly->mod = 500 * ( shm->speed / SOUND_11k );
					pdly->mod <<= sxhires;
					break;
				case 2:
					delaysamples = min( sxrvb_size.value * 0.71, 0.1 ) * shm->speed;
					pdly->mod = 700 * ( shm->speed / SOUND_11k );
					pdly->mod <<= sxhires;
					break;
				}

				delaysamples <<= sxhires;

				// save mod value
				mod = pdly->mod;

				if ( !pdly->lpdelayline )
				{
					pdly->delaysamples = delaysamples;
					SXDLY_Init( i, 0.1 );
				}

				pdly->mod = mod;
				pdly->modcur = mod;

				// do crossfade to new delay if delay has changed
				if ( delaysamples != pdly->delaysamples )
				{
					// set up crossfade from old pdly->delaysamples to new delaysamples
					pdly->idelayoutputxf = pdly->idelayinput - delaysamples;
					if ( pdly->idelayoutputxf < 0 )
						pdly->idelayoutputxf += pdly->cdelaysamplesmax;
					pdly->xfade = 32;
				}

				// deactivate line if rounded down to 0 delay
				if ( pdly->delaysamples == 0 )
					SXDLY_Free( i );
			}
		}
	}

	rgsxdly[1].delayfeed = sxrvb_feedback.value * 255;
	rgsxdly[1].lp = sxrvb_lp.value;

	rgsxdly[2].delayfeed = sxrvb_feedback.value * 255;
	rgsxdly[2].lp = sxrvb_lp.value;
}

// main routine for updating the paintbuffer with new reverb values.
// This routine updates both left and right lines with
// the mono reverb signal. Delay is set through console vars room_reverb
// and room_feedback. 2 reverbs operating in parallel.
void SXRVB_DoReverb( int count )
{
	int			val;
	int			valt;
	int			left;
	int			right;
	sample_t	sampledly;
	sample_t	samplexf;
	portable_samplepair_t	*pbuf;
	int			countr;
	int			voutm;
	int			vlr;

	// process reverb lines if active
	if ( rgsxdly[1].lpdelayline )
	{
		pbuf = paintbuffer;
		countr = count;

		// process each sample in the paintbuffer...
		while ( countr-- )
		{
			left = pbuf->left;
			right = pbuf->right;
			vlr = ( left + right ) >> 1;

			// UNDONE: ignored
			if ( --rgsxdly[1].modcur < 0 )
				rgsxdly[1].modcur = rgsxdly[1].mod;

			// ========================== ISXRVB============================

			// get sample from delay line
			sampledly = rgsxdly[1].lpdelayline[rgsxdly[1].idelayoutput];

			// only process if something is non-zero
			if ( rgsxdly[1].xfade || sampledly || left || right )
			{
				// modulate delay rate
				if ( !rgsxdly[1].xfade && !rgsxdly[1].mod )
				{
					rgsxdly[1].idelayoutputxf = rgsxdly[1].idelayoutput + ( ( RandomLong( 0, 255 ) * rgsxdly[1].delaysamples ) >> 9 );

					if ( rgsxdly[1].idelayoutputxf >= rgsxdly[1].cdelaysamplesmax )
						rgsxdly[1].idelayoutputxf -= rgsxdly[1].cdelaysamplesmax;

					rgsxdly[1].xfade = 32;
				}

				if ( rgsxdly[1].xfade )
				{
					samplexf = ( rgsxdly[1].lpdelayline[rgsxdly[1].idelayoutputxf] * ( 32 - rgsxdly[1].xfade ) ) >> 5;
					sampledly = ( ( sampledly * rgsxdly[1].xfade ) >> 5 ) + samplexf;

					if ( ++rgsxdly[1].idelayoutputxf >= rgsxdly[1].cdelaysamplesmax )
						rgsxdly[1].idelayoutputxf = 0;

					if ( --rgsxdly[1].xfade == 0 )
						rgsxdly[1].idelayoutput = rgsxdly[1].idelayoutputxf;
				}

				if ( sampledly )
				{
					// get current sample from delay buffer

					// calculate delayed value from avg of left and right channels
					val = vlr + ( ( rgsxdly[1].delayfeed * sampledly ) >> 8 );

					// limit to short
					if ( val > 32767 )
						val = 32767;
					else if ( val < -32768 )
						val = -32768;
				}
				else
				{
					val = vlr;
				}

				// lowpass
				if ( rgsxdly[1].lp )
				{
					valt = ( rgsxdly[1].lp0 + val ) >> 1;
					rgsxdly[1].lp0 = val;
				}
				else
				{
					valt = val;
				}

				// store delay output value into output buffer
				rgsxdly[1].lpdelayline[rgsxdly[1].idelayinput] = valt;

				voutm = valt;
			}
			else
			{
				// not playing samples, but still must flush lowpass buffer & delay line
				rgsxdly[1].lp0 = 0;
				rgsxdly[1].lpdelayline[rgsxdly[1].idelayinput] = 0;

				voutm = 0;
			}

			// update delay buffer pointers
			if ( ++rgsxdly[1].idelayinput >= rgsxdly[1].cdelaysamplesmax )
				rgsxdly[1].idelayinput = 0;

			if ( ++rgsxdly[1].idelayoutput >= rgsxdly[1].cdelaysamplesmax )
				rgsxdly[1].idelayoutput = 0;

			// ========================== ISXRVB + 1========================

			// UNDONE: ignored
			if ( --rgsxdly[2].modcur < 0 )
				rgsxdly[2].modcur = rgsxdly[2].mod;

			if ( rgsxdly[2].lpdelayline )
			{
				// get sample from delay line
				sampledly = rgsxdly[2].lpdelayline[rgsxdly[2].idelayoutput];

				// only process if something is non-zero
				if ( rgsxdly[2].xfade || sampledly || left || right )
				{
					// modulate delay rate
					if ( !rgsxdly[2].xfade && !rgsxdly[2].mod )
					{
						rgsxdly[2].idelayoutputxf = rgsxdly[2].idelayoutput + ( ( RandomLong( 0, 255 ) * rgsxdly[2].delaysamples ) >> 9 );

						if ( rgsxdly[2].idelayoutputxf >= rgsxdly[2].cdelaysamplesmax )
							rgsxdly[2].idelayoutputxf -= rgsxdly[2].cdelaysamplesmax;

						rgsxdly[2].xfade = 32;
					}

					if ( rgsxdly[2].xfade )
					{
						samplexf = ( rgsxdly[2].lpdelayline[rgsxdly[2].idelayoutputxf] * ( 32 - rgsxdly[2].xfade ) ) >> 5;
						sampledly = ( ( sampledly * rgsxdly[2].xfade ) >> 5 ) + samplexf;

						if ( ++rgsxdly[2].idelayoutputxf >= rgsxdly[2].cdelaysamplesmax )
							rgsxdly[2].idelayoutputxf = 0;

						if ( --rgsxdly[2].xfade == 0 )
							rgsxdly[2].idelayoutput = rgsxdly[2].idelayoutputxf;
					}

					if ( sampledly )
					{
						// get current sample from delay buffer

						// calculate delayed value from avg of left and right channels
						val = vlr + ( ( rgsxdly[2].delayfeed * sampledly ) >> 8 );

						// limit to short
						if ( val > 32767 )
							val = 32767;
						else if ( val < -32768 )
							val = -32768;
					}
					else
					{
						val = vlr;
					}

					// lowpass
					if ( rgsxdly[2].lp )
					{
						valt = ( rgsxdly[2].lp0 + val ) >> 1;
						rgsxdly[2].lp0 = val;
					}
					else
					{
						valt = val;
					}

					// store delay output value into output buffer
					rgsxdly[2].lpdelayline[rgsxdly[2].idelayinput] = valt;

					voutm += valt;
				}
				else
				{
					// not playing samples, but still must flush lowpass buffer
					rgsxdly[2].lp0 = 0;
					rgsxdly[2].lpdelayline[rgsxdly[2].idelayinput] = 0;
				}

				// update delay buffer pointers
				if ( ++rgsxdly[2].idelayinput >= rgsxdly[2].cdelaysamplesmax )
					rgsxdly[2].idelayinput = 0;

				if ( ++rgsxdly[2].idelayoutput >= rgsxdly[2].cdelaysamplesmax )
					rgsxdly[2].idelayoutput = 0;
			}

			// ============================ Mix================================

			// add mono delay to left and right channels

			// drop output by inverse of cascaded gain for both reverbs
			voutm = ( 11 * voutm ) >> 6;

			left += voutm;
			right += voutm;

			// limit to short
			if ( left > 32767 )
				left = 32767;
			else if ( left < -32768 )
				left = -32768;

			if ( right > 32767 )
				right = 32767;
			else if ( right < -32768 )
				right = -32768;

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}

// amplitude modulator, low pass filter for underwater weirdness
void SXRVB_DoAMod( int count )
{
	int			valtl, valtr;
	int			left;
	int			right;
	portable_samplepair_t	*pbuf;
	int			countr;
	int			fLowpass;
	int			fmod;

	// process AMod if either filter or mod is active
	if ( sxmod_lowpass.value != 0.0 || sxmod_mod.value != 0.0 )
	{
		pbuf = paintbuffer;
		countr = count;

		fLowpass = ( sxmod_lowpass.value != 0.0 );
		fmod = ( sxmod_mod.value != 0.0 );

		// process each sample in the paintbuffer...
		while ( countr-- )
		{
			left = pbuf->left;
			right = pbuf->right;

			// only process if non-zero
			if ( fLowpass )
			{
				valtl = left;
				valtr = right;

				left = ( rgsxlp[0] + rgsxlp[1] + rgsxlp[2] + rgsxlp[3] + rgsxlp[4] + left ) >> 2;
				right = ( rgsxlp[5] + rgsxlp[6] + rgsxlp[7] + rgsxlp[8] + rgsxlp[9] + right ) >> 2;

				rgsxlp[4] = valtl;
				rgsxlp[9] = valtr;

				rgsxlp[0] = rgsxlp[1];
				rgsxlp[1] = rgsxlp[2];
				rgsxlp[2] = rgsxlp[3];
				rgsxlp[3] = rgsxlp[4];
				rgsxlp[4] = rgsxlp[5];
				rgsxlp[5] = rgsxlp[6];
				rgsxlp[6] = rgsxlp[7];
				rgsxlp[7] = rgsxlp[8];
				rgsxlp[8] = rgsxlp[9];
			}

			if ( fmod )
			{
				if ( --sxmod1cur < 0 )
					sxmod1cur = sxmod1;

				if ( !sxmod1 )
					sxamodlt = RandomLong( 32, 255 );

				if ( --sxmod2cur < 0 )
					sxmod2cur = sxmod2;

				if ( !sxmod2 )
					sxamodlt = RandomLong( 32, 255 );

				left = ( left * sxamodl ) >> 8;
				right = ( right * sxamodr ) >> 8;

				if ( sxamodl < sxamodlt )
					sxamodl++;
				else if ( sxamodl > sxamodlt )
					sxamodl--;

				if ( sxamodr < sxamodrt )
					sxamodr++;
				else if ( sxamodr > sxamodrt )
					sxamodr--;
			}

			// limit to short
			if ( left > 32767 )
				left = 32767;
			else if ( left < -32768 )
				left = -32768;

			if ( right > 32767 )
				right = 32767;
			else if ( right < -32768 )
				right = -32768;

			pbuf->left = left;
			pbuf->right = right;

			pbuf++;
		}
	}
}

typedef struct sx_preset_s
{
	float	room_lp;			// for water fx, lowpass for entire room
	float	room_mod;			// stereo amplitude modulation for room

	float	room_size;			// reverb: initial reflection size
	float	room_refl;			// reverb: decay time
	float	room_rvblp;			// reverb: low pass filtering level

	float	room_delay;			// mono delay: delay time
	float	room_feedback;		// mono delay: decay time
	float	room_dlylp;			// mono delay: low pass filtering level

	float	room_left;			// left channel delay time
} sx_preset_t;

sx_preset_t rgsxpre[29] =
{
//	          -------reverb--------  -------delay--------
// lp    mod   size   refl   rvblp  delay  feedback  dlylp  left
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.0,   0.0,      2.0,   0.0   },	// 0	off
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.065, 0.1,      0.0,   0.01  },	// 1	generic
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.02,  0.75,     0.0,   0.01  },	// 2	metalic
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.03,  0.78,     0.0,   0.02  },	// 3
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.06,  0.77,     0.0,   0.03  },	// 4
{ 0.0,  0.0,  0.05,  0.85,  1.0,   0.008, 0.96,     2.0,   0.01  },	// 5	tunnel
{ 0.0,  0.0,  0.05,  0.88,  1.0,   0.01,  0.98,     2.0,   0.02  },	// 6
{ 0.0,  0.0,  0.05,  0.92,  1.0,   0.015, 0.995,    2.0,   0.04  },	// 7
{ 0.0,  0.0,  0.05,  0.84,  1.0,   0.0,   0.0,      2.0,   0.012 },	// 8	chamber
{ 0.0,  0.0,  0.05,  0.9,   1.0,   0.0,   0.0,      2.0,   0.008 },	// 9
{ 0.0,  0.0,  0.05,  0.95,  1.0,   0.0,   0.0,      2.0,   0.004 },	// 10
{ 0.0,  0.0,  0.05,  0.7,   0.0,   0.0,   0.0,      2.0,   0.012 },	// 11	brite
{ 0.0,  0.0,  0.055, 0.78,  0.0,   0.0,   0.0,      2.0,   0.008 },	// 12
{ 0.0,  0.0,  0.05,  0.86,  0.0,   0.0,   0.0,      2.0,   0.002 },	// 13
{ 1.0,  0.0,  0.0,   0.0,   1.0,   0.0,   0.0,      2.0,   0.01  },	// 14	water
{ 1.0,  0.0,  0.0,   0.0,   1.0,   0.06,  0.85,     2.0,   0.02  },	// 15
{ 1.0,  0.0,  0.0,   0.0,   1.0,   0.2,   0.6,      2.0,   0.05  },	// 16
{ 0.0,  0.0,  0.05,  0.8,   1.0,   0.0,   0.48,     2.0,   0.016 },	// 17	concrete
{ 0.0,  0.0,  0.06,  0.9,   1.0,   0.0,   0.52,     2.0,   0.01  },	// 18
{ 0.0,  0.0,  0.07,  0.94,  1.0,   0.3,   0.6,      2.0,   0.008 },	// 19
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.3,   0.42,     2.0,   0.0   },	// 20	outside
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.35,  0.48,     2.0,   0.0   },	// 21
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.38,  0.6,      2.0,   0.0   },	// 22
{ 0.0,  0.0,  0.05,  0.9,   1.0,   0.2,   0.28,     0.0,   0.0   },	// 23	cavern
{ 0.0,  0.0,  0.07,  0.9,   1.0,   0.3,   0.4,      0.0,   0.0   },	// 24
{ 0.0,  0.0,  0.09,  0.9,   1.0,   0.35,  0.5,      0.0,   0.0   },	// 25
{ 0.0,  1.0,  0.01,  0.9,   0.0,   0.0,   0.0,      2.0,   0.05  },	// 26	weirdo
{ 0.0,  0.0,  0.0,   0.0,   1.0,   0.009, 0.999,    2.0,   0.04  },	// 27
{ 0.0,  0.0,  0.001, 0.999, 0.0,   0.2,   0.8,      2.0,   0.05  },	// 28
};

// the EAX environment of each room type, used when the EAX buffers play the sound
EAX_REVERBPROPERTIES eaxpre_guess[29] =
{
	{ EAX_ENVIRONMENT_GENERIC, 0.0F, 0.0F, 0.0F },				// 0	off
	{ EAX_ENVIRONMENT_ROOM, 0.417F, 0.4F, 0.666F },				// 1	generic
	{ EAX_ENVIRONMENT_BATHROOM, 0.3F, 1.499F, 0.166F },			// 2	metalic
	{ EAX_ENVIRONMENT_BATHROOM, 0.4F, 1.499F, 0.166F },			// 3
	{ EAX_ENVIRONMENT_BATHROOM, 0.6F, 1.499F, 0.166F },			// 4
	{ EAX_ENVIRONMENT_SEWERPIPE, 0.4F, 2.886F, 0.25F },			// 5	tunnel
	{ EAX_ENVIRONMENT_SEWERPIPE, 0.6F, 2.886F, 0.25F },			// 6
	{ EAX_ENVIRONMENT_SEWERPIPE, 0.8F, 2.886F, 0.25F },			// 7
	{ EAX_ENVIRONMENT_STONEROOM, 0.5F, 2.309F, 0.888F },		// 8	chamber
	{ EAX_ENVIRONMENT_STONEROOM, 0.65F, 2.309F, 0.888F },		// 9
	{ EAX_ENVIRONMENT_STONEROOM, 0.8F, 2.309F, 0.888F },		// 10
	{ EAX_ENVIRONMENT_STONECORRIDOR, 0.3F, 2.697F, 0.638F },	// 11	brite
	{ EAX_ENVIRONMENT_STONECORRIDOR, 0.5F, 2.697F, 0.638F },	// 12
	{ EAX_ENVIRONMENT_STONECORRIDOR, 0.65F, 2.697F, 0.638F },	// 13
	{ EAX_ENVIRONMENT_UNDERWATER, 1.0F, 1.499F, 0.0F },			// 14	water
	{ EAX_ENVIRONMENT_UNDERWATER, 1.0F, 2.499F, 0.0F },			// 15
	{ EAX_ENVIRONMENT_UNDERWATER, 1.0F, 3.499F, 0.0F },			// 16
	{ EAX_ENVIRONMENT_GENERIC, 0.65F, 1.493F, 0.5F },			// 17	concrete
	{ EAX_ENVIRONMENT_GENERIC, 0.85F, 1.493F, 0.5F },			// 18
	{ EAX_ENVIRONMENT_GENERIC, 1.0F, 1.493F, 0.5F },			// 19
	{ EAX_ENVIRONMENT_ARENA, 0.4F, 7.284F, 0.332F },			// 20	outside
	{ EAX_ENVIRONMENT_ARENA, 0.55F, 7.284F, 0.332F },			// 21
	{ EAX_ENVIRONMENT_ARENA, 0.7F, 7.284F, 0.332F },			// 22
	{ EAX_ENVIRONMENT_CONCERTHALL, 0.5F, 3.961F, 0.5F },		// 23	cavern
	{ EAX_ENVIRONMENT_CONCERTHALL, 0.7F, 3.961F, 0.5F },		// 24
	{ EAX_ENVIRONMENT_CONCERTHALL, 1.0F, 3.961F, 0.5F },		// 25
	{ EAX_ENVIRONMENT_DIZZY, 0.2F, 17.234F, 0.666F },			// 26	weirdo
	{ EAX_ENVIRONMENT_DIZZY, 0.3F, 17.234F, 0.666F },			// 27
	{ EAX_ENVIRONMENT_DIZZY, 0.4F, 17.234F, 0.666F },			// 28
};

// force next call to sx_roomfx to reload all room parameters.
// used when switching to/from hires sound mode.
void SX_ReloadRoomFX( void )
{
	// reset all roomtype parms
	sxroom_typeprev = -1.0;

	// reset all delay, reverb, stereo
	sxdly_delayprev = -1.0;
	sxrvb_sizeprev = -1.0;
	sxste_delayprev = -1.0;

	sxroom_typeprev = -1.0;
}

// main routine for processing room sound fx
// if fFilter is true, then run in-line filter (for underwater fx)
// if fTimefx is true, then run reverb and delay fx
// NOTE: only processes preset room_types from 0-28 (CSXROOM)
void SX_RoomFX( int count, int fFilter, int fTimefx )
{
	int		fReset;
	HRESULT	hresult;
	int		i;
	float	roomType;
	float	flWetMix_guess;
	int		j, k;

	// return right away if fx processing is turned off
	if ( sxroom_off.value != 0.0 )
		return;

	if ( pEAXPropFL_guess )
	{
		// the EAX hardware does the room effects
		if ( cl.waterlevel > 2 )
		{
			roomType = sxroomwater_type.value;
			flWetMix_guess = 1.0;
		}
		else
		{
			roomType = sxroom_type.value;
			flWetMix_guess = 0.38;
		}

		if ( roomType != sxroom_typeprev )
		{
			i = (int)roomType;
			sxroom_typeprev = roomType;

			pEAXPropFL_guess->lpVtbl->Set( pEAXPropFL_guess, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAXBUFFER_REVERBMIX, NULL, 0, &flWetMix_guess, sizeof( float ) );
			pEAXPropFR_guess->lpVtbl->Set( pEAXPropFR_guess, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAXBUFFER_REVERBMIX, NULL, 0, &flWetMix_guess, sizeof( float ) );
			pEAXPropRL_guess->lpVtbl->Set( pEAXPropRL_guess, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAXBUFFER_REVERBMIX, NULL, 0, &flWetMix_guess, sizeof( float ) );
			pEAXPropRR_guess->lpVtbl->Set( pEAXPropRR_guess, &DSPROPSETID_EAXBUFFER_ReverbProperties, DSPROPERTY_EAXBUFFER_REVERBMIX, NULL, 0, &flWetMix_guess, sizeof( float ) );

			hresult = pEAXPropFL_guess->lpVtbl->Set( pEAXPropFL_guess, &DSPROPSETID_EAX_ReverbProperties, DSPROPERTY_EAX_ALL, NULL, 0, &eaxpre_guess[i], sizeof( EAX_REVERBPROPERTIES ) );
			if ( hresult != DS_OK )
				Con_DPrintf( "Eax failed preset %d\n", i );
			else
				Con_DPrintf( "Eax preset %d\n", i );

			eaxroomtype_guess = i;
		}
	}
	else
	{
		// flag hires sound mode
		sxhires = ( hisound.value == 0 ) ? 0 : 1;

		if ( sxhires != sxhiresprev )
		{
			SX_ReloadRoomFX();
			sxhiresprev = sxhires;
		}

		fReset = false;

		if ( cl.waterlevel > 2 )
			roomType = sxroomwater_type.value;
		else
			roomType = sxroom_type.value;

		// only process legacy roomtypes here
		if ( roomType != sxroom_typeprev )
		{
			sxroom_typeprev = roomType;

			i = (int)roomType;

			if ( i < 29 && i >= 0 )
			{
				Cvar_SetValue( "room_lp", rgsxpre[i].room_lp );
				Cvar_SetValue( "room_mod", rgsxpre[i].room_mod );
				Cvar_SetValue( "room_size", rgsxpre[i].room_size );
				Cvar_SetValue( "room_refl", rgsxpre[i].room_refl );
				Cvar_SetValue( "room_rvblp", rgsxpre[i].room_rvblp );
				Cvar_SetValue( "room_delay", rgsxpre[i].room_delay );
				Cvar_SetValue( "room_feedback", rgsxpre[i].room_feedback );
				Cvar_SetValue( "room_dlylp", rgsxpre[i].room_dlylp );
				Cvar_SetValue( "room_left", rgsxpre[i].room_left );
			}

			SXRVB_CheckNewReverbVal();
			SXDLY_CheckNewDelayVal();
			SXDLY_CheckNewStereoDelayVal();

			fReset = true;
		}

		if ( fReset || roomType != 0.0 )
		{
			// debug code
			SXRVB_CheckNewReverbVal();
			SXDLY_CheckNewDelayVal();
			SXDLY_CheckNewStereoDelayVal();

			if ( fFilter )
				SXRVB_DoAMod( count );

			if ( fTimefx )
			{
				// the channels A3D plays get only the room effects here
				if ( snd_isa3d_guess && s_verbwet.value >= 0.001f )
				{
					for ( j = 0; j < count; j++ )
					{
						paintbuffer[j].left += (int)( a3dpaintbuffer_guess[j].left * s_verbwet.value );
						paintbuffer[j].right += (int)( a3dpaintbuffer_guess[j].right * s_verbwet.value );
					}
				}

				SXRVB_DoReverb( count );
				SXDLY_DoDelay( count );

				if ( snd_isa3d_guess && s_verbwet.value >= 0.001f )
				{
					for ( k = 0; k < count; k++ )
					{
						paintbuffer[k].left -= (int)( a3dpaintbuffer_guess[k].left * s_verbwet.value );
						paintbuffer[k].right -= (int)( a3dpaintbuffer_guess[k].right * s_verbwet.value );
					}
				}

				SXDLY_DoStereoDelay( count );
			}
		}
	}
}

//===============================================================================
// Client entity mouth movement code. Set entity mouthopen variable, based
// on the sound envelope of the voice channel playing.
// KellyB 10/22/97
//===============================================================================

// called when first starting to play wav data, to init the stream
qboolean Wavstream_Init( void )
{
	int		i;

	for ( i = 0; i < CWAVSTREAMS; i++ )
	{
		Q_memset( &wavstreams[i], 0, sizeof( wavstream_t ) );
		wavstreams[i].hFile = 0;
	}

	return true;
}

// close wav file for stream i
void Wavstream_Close( int i )
{
	if ( wavstreams[i].hFile )
		FS_Close( wavstreams[i].hFile );

	Q_memset( &wavstreams[i], 0, sizeof( wavstream_t ) );
	wavstreams[i].hFile = 0;
}

// if sound data is beyond the end of the data in memory, load the next chunk of the wav file
void Wavstream_GetNextChunk( channel_t *ch, sfx_t *s )
{
	byte		*data;
	sfxcache_t	*sc;
	int			cbread;
	int			i;

	i = ( (byte *)ch - (byte *)channels ) / sizeof( channel_t );

	sc = Cache_Check( &s->cache );

	wavstreams[i].lastposloaded = wavstreams[i].csamplesplayed + wavstreams[i].info.dataofs;

	data = COM_LoadFileLimit( NULL, wavstreams[i].lastposloaded, 0x8000, &cbread, &wavstreams[i].hFile );

	wavstreams[i].csamplesinmem = cbread;

	if ( wavstreams[i].csamplesinmem > wavstreams[i].info.samples - wavstreams[i].csamplesplayed )
		wavstreams[i].csamplesinmem = wavstreams[i].info.samples - wavstreams[i].csamplesplayed;

	sc->length = wavstreams[i].csamplesinmem;

	ResampleSfx( s, sc->speed, sc->width, data );
}

#define CAVGSAMPLES		10

void SND_ForceInitMouth( int entnum )
{
	if ( entnum < 0 || entnum >= cl.max_edicts )
		return;

	cl_entities[entnum].mouth.mouthopen = 0;
	cl_entities[entnum].mouth.sndavg = 0;
	cl_entities[entnum].mouth.sndcount = 0;
}

void SND_ForceCloseMouth( int entnum )
{
	if ( entnum < 0 || entnum >= cl.max_edicts )
		return;

	cl_entities[entnum].mouth.mouthopen = 0;
}

void SND_InitMouth( int entnum, int entchannel )
{
	if ( ( entchannel == CHAN_VOICE || entchannel == CHAN_STREAM ) && entnum > 0 )
		SND_ForceInitMouth( entnum );
}

void SND_CloseMouth( channel_t *ch )
{
	if ( ch->entnum > 0 && ( ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM ) )
		SND_ForceCloseMouth( ch->entnum );
}

void SND_MoveMouth( channel_t *ch, sfxcache_t *sc, int count )
{
	int			data;
	signed char	*pdata;
	int			i;
	int			savg;
	int			scount;
	cl_entity_t	*pent;

	pent = &cl_entities[ch->entnum];

	pdata = (signed char *)sc->data + ch->pos;

	i = 0;
	scount = pent->mouth.sndcount;
	savg = 0;

	while ( i < count && scount < CAVGSAMPLES )
	{
		data = pdata[i];
		savg += abs( data );

		i += 80 + ( (byte)data & 0x1F );
		scount++;
	}

	pent->mouth.sndavg += savg;
	pent->mouth.sndcount = (byte)scount;

	if ( pent->mouth.sndcount >= CAVGSAMPLES )
	{
		pent->mouth.mouthopen = pent->mouth.sndavg / CAVGSAMPLES;
		pent->mouth.sndavg = 0;
		pent->mouth.sndcount = 0;
	}
}

void SND_MoveMouth16( int entnum, short *pdata, int count )
{
	int			data;
	int			i;
	int			savg;
	int			scount;
	cl_entity_t	*pent;

	if ( entnum < 0 || entnum >= cl.max_edicts )
		return;

	pent = &cl_entities[entnum];

	i = 0;
	scount = pent->mouth.sndcount;
	savg = 0;

	while ( i < count && scount < CAVGSAMPLES )
	{
		data = pdata[i] >> 8;
		savg += abs( data );

		i += 80 + ( (byte)data & 0x1F );
		scount++;
	}

	pent->mouth.sndavg += savg;
	pent->mouth.sndcount = (byte)scount;

	if ( pent->mouth.sndcount >= CAVGSAMPLES )
	{
		pent->mouth.mouthopen = pent->mouth.sndavg / CAVGSAMPLES;
		pent->mouth.sndavg = 0;
		pent->mouth.sndcount = 0;
	}
}

//===============================================================================
// VOX. Algorithms to load and play spoken text sentences from a file.
//===============================================================================

char	*rgpparseword[CVOXWORDMAX];		// array of pointers to parsed words
char	voxperiod[] = "_period";		// vocal pause
char	voxcomma[] = "_comma";			// vocal pause

voxword_t	rgrgvoxword[16][CVOXWORDMAX];

void VOX_Init( void )
{
	Q_memset( rgrgvoxword, 0, sizeof( voxword_t ) * 16 * CVOXWORDMAX );

	VOX_ReadSentenceFile();
}

// parse a null terminated string of text into component words, with
// pointers to each word stored in rgpparseword
// note: this code actually alters the passed in string!
void VOX_ParseString( char *psz )
{
	int		i;
	int		fdone = 0;
	char	*pszscan = psz;
	char	c;

	Q_memset( rgpparseword, 0, sizeof( char * ) * CVOXWORDMAX );

	if ( !psz )
		return;

	i = 0;
	rgpparseword[i++] = psz;

	while ( !fdone && i < CVOXWORDMAX )
	{
		// scan up to next word
		c = *pszscan;
		while ( c && c != ' ' && c != '.' && c != ',' && c != '(' )
			c = *( ++pszscan );

		// if '(' then scan for matching ')'
		if ( c == '(' )
		{
			c = *( ++pszscan );
			while ( c && c != ')' )
				c = *( ++pszscan );

			if ( !c )
				fdone = 1;
			else
				c = *( ++pszscan );
		}

		// if at end of string, done
		if ( fdone || !c )
		{
			fdone = 1;
		}
		else
		{
			// if period or comma and not followed by end-of-line, add pause word
			if ( ( c == '.' || c == ',' ) && *( pszscan + 1 ) != '\n' && *( pszscan + 1 ) != '\r' && *( pszscan + 1 ) != 0 )
			{
				if ( c == '.' )
					rgpparseword[i++] = voxperiod;
				else
					rgpparseword[i++] = voxcomma;

				if ( i >= CVOXWORDMAX )
					break;
			}

			// null terminate substring
			*pszscan++ = 0;

			// skip whitespace
			c = *pszscan;
			while ( c && ( c == ' ' || c == '.' || c == ',' ) )
				c = *( ++pszscan );

			if ( !c )
				fdone = 1;
			else
				rgpparseword[i++] = pszscan;
		}
	}
}

// backwards scan psz for last '/'
// if found, copy string up to and including last '/' to szpath
// return pointer to string after the '/'
char *VOX_GetDirectory( char *szpath, char *psz )
{
	char	c;
	int		cb = 0;
	char	*pszscan = psz + Q_strlen( psz ) - 1;

	// scan backwards until first '/' or start of string
	c = *pszscan;
	while ( pszscan > psz && c != '/' )
	{
		c = *( --pszscan );
		cb++;
	}

	if ( c != '/' )
	{
		// didn't find '/', return default directory
		Q_strcpy( szpath, "vox/" );
		return psz;
	}

	cb = Q_strlen( psz ) - cb;
	Q_memcpy( szpath, psz, cb );
	szpath[cb] = 0;
	return pszscan + 1;
}

// set channel volume based on volume of current word
void VOX_SetChanVol( channel_t *ch )
{
	float	scale;
	int		vol;

	if ( ch->isentence < 0 )
		return;

	vol = rgrgvoxword[ch->isentence][ch->iword].volume;
	if ( vol == 100 )
		return;

	scale = vol;
	scale = scale / 100.0;

	if ( scale == 1.0 )
		return;

	ch->rightvol = ch->rightvol * scale;
	ch->leftvol = ch->leftvol * scale;

	if ( snd_iseax_guess )
	{
		ch->rrightvol_guess = ch->rrightvol_guess * scale;
		ch->rleftvol_guess = ch->rleftvol_guess * scale;
	}
}

// parse a word in a sentence for parameters; the parameters
// override the default values of the voxword
int VOX_ParseWordParams( char *psz, voxword_t *pvoxword, int fFirst )
{
	char	*pszsave = psz;
	char	c;
	char	ct;
	char	sznum[8];
	int		i;
	static voxword_t	voxwordDefault;

	// init to defaults if this is the first word in string.
	if ( fFirst )
	{
		voxwordDefault.pitch = -1;
		voxwordDefault.volume = 100;
		voxwordDefault.start = 0;
		voxwordDefault.end = 100;
		voxwordDefault.fKeepCached = 0;
		voxwordDefault.timecompress = 0;
	}

	*pvoxword = voxwordDefault;

	// look at next to last char to see if we have a
	// valid format:
	c = *( psz + Q_strlen( psz ) - 1 );

	if ( c != ')' )
		return 1;		// no formatting, return

	// scan forward to first '('
	c = *psz;
	while ( c != '(' && c != ')' )
		c = *( ++psz );

	if ( c == ')' )
		return 0;		// bogus formatting

	// null terminate
	*psz = 0;
	ct = *( ++psz );

	while ( 1 )
	{
		// scan until we hit a character in the commandSet
		while ( ct && ct != 'v' && ct != 'p' && ct != 's' && ct != 'e' && ct != 't' && ct != ')' )
			ct = *( ++psz );

		if ( ct == ')' )
			break;

		Q_memset( sznum, 0, sizeof( sznum ) );
		i = 0;

		c = *( ++psz );

		if ( !isdigit( c ) )
			break;

		// read number
		while ( isdigit( c ) && i < sizeof( sznum ) - 1 )
		{
			sznum[i++] = c;
			c = *( ++psz );
		}

		// get value of number
		i = Q_atoi( sznum );

		switch ( ct )
		{
		case 'v': pvoxword->volume = i; break;
		case 'p': pvoxword->pitch = i; break;
		case 's': pvoxword->start = i; break;
		case 'e': pvoxword->end = i; break;
		case 't': pvoxword->timecompress = i; break;
		}

		ct = c;
	}

	// if the string has zero length, this was an isolated
	// parameter block.  Set default voxword to these
	// values
	if ( Q_strlen( pszsave ) == 0 )
	{
		voxwordDefault = *pvoxword;
		return 0;
	}
	else
		return 1;
}

// find an empty sentence slot
int VOX_IFindEmptySentence( void )
{
	int		k;

	for ( k = 0; k < 16; k++ )
	{
		if ( !rgrgvoxword[k][0].sfx )
			break;
	}

	if ( k >= 16 )
	{
		Con_DPrintf( "Sentence or Pitch shift ignored. > 16 playing!\n" );
		return -1;
	}

	return k;
}

// play a single word as a sentence, to get pitch shifting
void VOX_MakeSingleWordSentence( channel_t *ch, int pitch )
{
	voxword_t	voxword;
	int			k;

	k = VOX_IFindEmptySentence();

	if ( k >= 0 )
	{
		voxword.volume = 100;
		voxword.pitch = 100;
		voxword.start = 0;
		voxword.end = 100;
		voxword.fKeepCached = 1;
		voxword.samplefrac = 0;
		voxword.sfx = ch->sfx;
		voxword.timecompress = 0;

		rgrgvoxword[k][0] = voxword;
		rgrgvoxword[k][1].sfx = NULL;

		ch->pitch = pitch;
		ch->isentence = k;
		ch->iword = 0;
	}
	else
	{
		ch->pitch = 100;
		ch->isentence = -1;
	}
}

// link all sounds in sentence, start playing first word
sfxcache_t *VOX_LoadSound( channel_t *pchan, char *pszin )
{
	char		buffer[512];
	int			i, j, k, cword;
	char		pathbuffer[64];
	char		szpath[32];
	sfxcache_t	*sc;
	voxword_t	rgvoxword[CVOXWORDMAX];
	char		*psz;

	if ( !pszin )
		return NULL;

	Q_memset( rgvoxword, 0, sizeof( voxword_t ) * CVOXWORDMAX );
	Q_memset( buffer, 0, sizeof( buffer ) );

	// lookup actual string in rgpszrawsentence,
	// set pointer to string data
	psz = VOX_LookupString( pszin, NULL );

	if ( !psz )
	{
		Con_DPrintf( "VOX_LoadSound: no sentence named %s\n", pszin );
		return NULL;
	}

	// get directory from string, advance psz
	psz = VOX_GetDirectory( szpath, psz );

	if ( Q_strlen( psz ) > sizeof( buffer ) - 1 )
	{
		Con_DPrintf( "VOX_LoadSound: sentence is too long %s\n", psz );
		return NULL;
	}

	// copy into buffer
	Q_strncpy( buffer, psz, sizeof( buffer ) - 1 );
	buffer[sizeof( buffer ) - 1] = 0;
	psz = buffer;

	// parse sentence (also inserts null terminators between words)
	VOX_ParseString( psz );

	// for each word in the sentence, construct the filename,
	// lookup the sfx and save each pointer in a temp array
	i = 0;
	cword = 0;
	while ( rgpparseword[i] )
	{
		// Get any pitch, volume, start, end params into voxword
		if ( VOX_ParseWordParams( rgpparseword[i], &rgvoxword[cword], i == 0 ) )
		{
			// this is a valid word (as opposed to a parameter block)
			_snprintf( pathbuffer, sizeof( pathbuffer ), "%s%s.wav", szpath, rgpparseword[i] );
			pathbuffer[sizeof( pathbuffer ) - 1] = 0;

			if ( Q_strlen( pathbuffer ) >= MAX_QPATH )
				continue;

			// find name, if already in cache, mark voxword
			// so we don't discard when word is done playing
			rgvoxword[cword].sfx = S_FindName( pathbuffer, &( rgvoxword[cword].fKeepCached ) );
			cword++;
		}
		i++;
	}

	k = VOX_IFindEmptySentence();
	if ( k < 0 )
		return NULL;

	j = 0;
	while ( rgvoxword[j].sfx != NULL )
	{
		rgrgvoxword[k][j] = rgvoxword[j];
		j++;
	}

	rgrgvoxword[k][j].sfx = NULL;

	pchan->isentence = k;
	pchan->iword = 0;
	pchan->sfx = rgrgvoxword[k][0].sfx;

	sc = S_LoadSound( rgvoxword[0].sfx, NULL );
	if ( !sc )
		S_FreeChannel( pchan );

	return sc;
}

// pitch shift an 8 bit wav into the paintbuffer, with optional time compression
// returns true if we hit the end of the sample
int VOX_FPaintPitchChannelFrom8Offs( portable_samplepair_t *paintbuffer, channel_t *ch, sfxcache_t *sc, int count, int pitch, int timecompress, int offset )
{
	int		data;
	int		*lscale, *rscale;
	unsigned char	*sfx;
	int		i;
	int		posold;
	int		samplefrac, fracstep, srcsample;
	float	stepscale;
	int		cb;
	portable_samplepair_t	*pbuffer;
	portable_samplepair_t	*prear_guess;
	int		*rlscale, *rrscale;

	srcsample = 0;
	pbuffer = &paintbuffer[offset];
	prear_guess = &rearpaintbuffer_guess[offset];

	if ( ch->isentence < 0 )
		return 0;

	stepscale = (float)pitch / 100.0;	// adjust playback pitch
	stepscale = (float)pitch / 100.0;

	if ( ch->leftvol > 255 )
		ch->leftvol = 255;
	if ( ch->rightvol > 255 )
		ch->rightvol = 255;

	lscale = snd_scaletable[ch->leftvol >> 3];
	rscale = snd_scaletable[ch->rightvol >> 3];
	sfx = sc->data;

	if ( snd_iseax_guess )
	{
		if ( ch->rleftvol_guess > 255 )
			ch->rleftvol_guess = 255;
		if ( ch->rrightvol_guess > 255 )
			ch->rrightvol_guess = 255;

		rlscale = snd_scaletable[ch->rleftvol_guess >> 3];
		rrscale = snd_scaletable[ch->rrightvol_guess >> 3];
	}

	samplefrac = rgrgvoxword[ch->isentence][ch->iword].samplefrac;
	fracstep = stepscale * 256;
	cb = rgrgvoxword[ch->isentence][ch->iword].cbtrim;

	srcsample = samplefrac >> 8;
	samplefrac += fracstep;

	if ( !timecompress )
	{
		for ( i = 0; i < count && srcsample < cb; i++ )
		{
			data = sfx[srcsample];

			pbuffer[i].left += lscale[data];
			pbuffer[i].right += rscale[data];

			if ( snd_iseax_guess )
			{
				prear_guess[i].left += rlscale[data];
				prear_guess[i].right += rrscale[data];
			}

			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
		}
	}
	else
	{
		int		chunksize;
		int		skipbytes;
		int		lowsample;
		int		playcount;
		int		cdata;
		int		j;

		chunksize = cb / 8;
		skipbytes = ( chunksize * timecompress ) / 100;

		i = 0;
		while ( i < count && srcsample < cb )
		{
			lowsample = srcsample % chunksize;

			// if we're in the skip zone of the chunk, play the whole chunk
			if ( srcsample < skipbytes || lowsample >= skipbytes )
			{
				cdata = chunksize - lowsample;
			}
			else
			{
				// scan forward to first zero crossing, painting as we go
				for ( j = 0; i < count && srcsample < cb && j < 255; j++, i++ )
				{
					data = sfx[srcsample];

					if ( abs( data ) <= 2 )
						break;

					pbuffer[i].left += lscale[data];
					pbuffer[i].right += rscale[data];

					if ( snd_iseax_guess )
					{
						prear_guess[i].left += rlscale[data];
						prear_guess[i].right += rrscale[data];
					}

					srcsample = samplefrac >> 8;
					samplefrac += fracstep;
				}

				if ( i > PAINTBUFFER_SIZE )
					Con_DPrintf( "timecompress scan forward: overwrote paintbuffer!" );

				if ( srcsample >= cb )
					break;

				if ( i >= count )
					break;

				// now skip forward to the next zero crossing after the skip zone
				while ( 1 )
				{
					lowsample = srcsample % chunksize;

					if ( lowsample < skipbytes )
					{
						samplefrac += ( skipbytes - lowsample ) << 8;
						srcsample += skipbytes - lowsample;
					}

					for ( j = 0; srcsample < cb && j < 255; j++ )
					{
						if ( abs( sfx[srcsample] ) <= 2 )
							break;

						srcsample = samplefrac >> 8;
						samplefrac += fracstep;
					}

					if ( srcsample >= cb )
						break;

					lowsample = srcsample % chunksize;

					if ( lowsample >= skipbytes )
					{
						cdata = chunksize - lowsample;
						break;
					}
				}
			}

			// paint the rest of the chunk
			playcount = ( cdata << 8 ) / fracstep;

			if ( i + playcount > count )
				playcount = count - i;

			if ( !playcount )
				playcount = 1;

			for ( j = 0; i < count && j < playcount && srcsample < cb; j++, i++ )
			{
				data = sfx[srcsample];

				pbuffer[i].left += lscale[data];
				pbuffer[i].right += rscale[data];

				if ( snd_iseax_guess )
				{
					prear_guess[i].left += rlscale[data];
					prear_guess[i].right += rrscale[data];
				}

				srcsample = samplefrac >> 8;
				samplefrac += fracstep;
			}
		}
	}

	// update the position in the sentence word
	rgrgvoxword[ch->isentence][ch->iword].samplefrac = samplefrac - fracstep;

	posold = ch->pos;
	ch->pos = samplefrac >> 8;
	ch->end += i - ( ch->pos - posold );

	if ( srcsample >= cb )
		return 1;

	return 0;
}

char	*rgpszrawsentence[CVOXFILESENTENCEMAX];
int		cszrawsentences = 0;

char	szsentences[] = "sound/sentences.txt";	// sentence file

// Load sentence file into memory, insert null terminators to
// delimit sentence name/sentence pairs.  Keep pointer to each
// sentence name so we can search later.
void VOX_ReadSentenceFile( void )
{
	char	*pBuf;
	char	*pch;
	char	*pchlast;
	int		nSentenceCount;
	int		nFileLength;

	VOX_Shutdown();

	// load file
	pBuf = (char *)COM_LoadFileForMe( szsentences, &nFileLength );
	if ( !pBuf )
	{
		Con_DPrintf( "VOX_ReadSentenceFile: Couldn't load %s\n", szsentences );
		return;
	}

	pch = pBuf;
	pchlast = pch + nFileLength;
	nSentenceCount = 0;

	while ( nSentenceCount < CVOXFILESENTENCEMAX && pch < pchlast )
	{
		char	*pszSentenceName = NULL;
		char	*pszSentenceValue;
		char	c;

		// skip newline, cr, tab, space
		c = *pch;
		while ( pch < pchlast && ( c == '\n' || c == '\r' || c == '\t' || c == ' ' ) )
			c = *( ++pch );

		// skip entire line if first char is /
		if ( *pch != '/' )
		{
			pszSentenceName = pch;

			// scan forward to first space, insert null terminator
			// after sentence name
			c = *pch;
			while ( pch < pchlast && c != ' ' && c != '\t' )
				c = *( ++pch );

			if ( pch < pchlast )
				*pch++ = 0;

			pszSentenceValue = pch;
		}

		// scan forward to end of sentence or eof
		c = *pch;
		while ( pch < pchlast && c != '\n' && c != '\r' )
			c = *( ++pch );

		// insert null terminator
		if ( pch < pchlast )
			*pch++ = 0;

		// if we have a sentence name, copy it
		if ( pszSentenceName )
		{
			int		nTotalSize = strlen( pszSentenceName ) + strlen( pszSentenceValue ) + 2;

			rgpszrawsentence[nSentenceCount] = (char *)Mem_Malloc( nTotalSize );
			memcpy( rgpszrawsentence[nSentenceCount], pszSentenceName, nTotalSize );
			nSentenceCount++;
		}
	}

	cszrawsentences = nSentenceCount;

	Mem_Free( pBuf );
}

void VOX_Shutdown( void )
{
	int		nCount;

	for ( nCount = 0; nCount < cszrawsentences; nCount++ )
		Mem_Free( rgpszrawsentence[nCount] );

	cszrawsentences = 0;
}

// Given a sentence name, return the sentence data and
// optionally the sentence index
char *VOX_LookupString( char *pszin, int *psentencenum )
{
	int		i;
	char	*cptr;
	sentenceEntry_s	*sentenceEntry = NULL;

	if ( pszin[0] == '#' )
	{
		const char	*indexAsString = pszin + 1;

		i = atoi( indexAsString );
		sentenceEntry = SequenceGetSentenceByIndex( i );
		if ( sentenceEntry )
			return sentenceEntry->data;
	}

	for ( i = 0; i < cszrawsentences; i++ )
	{
		if ( !Q_strcasecmp( pszin, rgpszrawsentence[i] ) )
		{
			if ( psentencenum )
				*psentencenum = i;

			cptr = rgpszrawsentence[i] + Q_strlen( rgpszrawsentence[i] ) + 1;
			while ( *cptr == ' ' || *cptr == '\t' )
				cptr++;

			return cptr;
		}
	}

	return NULL;
}

// Trim the start and end of the current word to the closest zero crossings
void VOX_TrimStartEndTimes( channel_t *ch, sfxcache_t *sc )
{
	float		sstart;
	float		send;
	int			length;
	int			i;
	int			srcsample;
	int			skiplen;
	voxword_t	*pvoxword;
	signed char	*pdata;

	if ( ch->isentence < 0 )
		return;

	pvoxword = &rgrgvoxword[ch->isentence][ch->iword];
	pvoxword->cbtrim = sc->length;

	send = pvoxword->end;
	send = pvoxword->end;
	sstart = pvoxword->start;
	sstart = pvoxword->start;

	if ( sstart >= send )
		return;

	// adjust start time
	if ( sstart > 0.0 && sstart < 100.0 )
	{
		length = sc->length;
		skiplen = length * ( sstart / 100.0 );
		ch->pos += skiplen;

		// scan forward to a zero crossing
		pdata = (signed char *)sc->data;
		srcsample = ch->pos;

		for ( i = 0; srcsample < length && i < 255; i++ )
		{
			if ( pdata[srcsample] >= -2 && pdata[srcsample] <= 2 )
			{
				ch->pos = srcsample;
				ch->end -= skiplen + i;
				break;
			}
			srcsample++;
		}

		if ( pvoxword->pitch != 100 )
			pvoxword->samplefrac += ch->pos << 8;
	}

	// adjust end time
	if ( send > 0.0 && send < 100.0 )
	{
		length = sc->length * ( ( 100.0 - send ) / 100.0 );
		ch->end -= length;

		// scan backward to a zero crossing
		pdata = (signed char *)sc->data;
		srcsample = sc->length - length;

		for ( i = 0; srcsample > ch->pos && i < 255; i++ )
		{
			if ( pdata[srcsample] >= -2 && pdata[srcsample] <= 2 )
			{
				ch->end -= i;
				pvoxword->cbtrim -= i + length;
				break;
			}
			srcsample--;
		}
	}
}

/*
===============================================================================

A3D HARDWARE VOICES

===============================================================================
*/

// the A3D source wrappers of the Aureal renderer library (names unproven)
void	A3D_SetCurrentSfx_guess( sfx_t *sfx );
int		hwPlayVoice_guess( void *hvoice, int mode );
int		hwStopVoice_guess( void *hvoice );
int		hwResetVoice_guess( void *hvoice );
int		hwSetVoicePosition_guess( void *hvoice, vec_t *origin );
int		A3DSource_SetGain_guess( void *hvoice, float gain );
int		A3DSource_SetPitch_guess( void *hvoice, float pitch );
int		hwSetVoiceOcclusions_guess( void *hvoice, int occlude );
int		hwIsVoicePlaying_guess( void *hvoice );
int		hwUnlockVoice_guess( void *hvoice, void *pbuf1, DWORD cb1, void *pbuf2, DWORD cb2 );
int		A3DSource_Lock_guess( void *hvoice, int size, void **pbuf1, DWORD *cb1, void **pbuf2, DWORD *cb2, DWORD flags );
int		A3DSource_GetFreeSamples_guess( void *hvoice );
int		A3DSource_Update_guess( void *hvoice );
int		A3DSource_GetDone_guess( void *hvoice, int *done );
int		A3DSource_Feed16_guess( void *hvoice, short *data, int size, int unused );
int		A3DSource_Restart_guess( void *hvoice );
int		A3DSource_GetStreamPos_guess( void *hvoice );
int		A3DSource_SetStreamPos_guess( void *hvoice, int pos );

// the voice slot tables of the engine A3D glue (names unproven)
int		hwFindChannelSlot_guess( int ch_idx );
void	hwSetSlotChannel_guess( int slot, int ch_idx );
void	hwSetChannelPos_guess( int ch_idx, int pos );

int		hA3D_FeedBuffer( void *hvoice, int ch_idx, channel_t *ch, sfxcache_t *sc, int count, int unused, float pitch );

/*
=================
hwPaintChannel_guess
=================
*/
int hwPaintChannel_guess( int ch_idx, channel_t *ch, sfxcache_t *sc, int count, int unused, float pitch )
{
	vec3_t	pos_guess;
	void	*hVoice_guess;
	int		ret_guess;
	float	fwd_guess;

	hVoice_guess = hwAllocChannel_guess( ch_idx, 0 );
	if ( !hVoice_guess )
		return 0;

	if ( ch->entchannel == CHAN_STREAM )
		return 0;

	A3D_SetCurrentSfx_guess( ch->sfx );

	ret_guess = hA3D_FeedBuffer( hVoice_guess, ch_idx, ch, sc, count, unused, pitch );
	if ( !ret_guess )
		return 0;

	if ( ch->entnum == cl.viewentity )
	{
		// the local player's own sounds play just in front of the listener
		fwd_guess = s_distance.value * 0.1;
		pos_guess[0] = fwd_guess * listener_forward[0] + listener_origin[0];
		pos_guess[1] = fwd_guess * listener_forward[1] + listener_origin[1];
		pos_guess[2] = fwd_guess * listener_forward[2] + listener_origin[2];

		hwSetVoiceOcclusions_guess( hVoice_guess, 0 );
	}
	else
	{
		VectorCopy( ch->origin, pos_guess );
	}

	hwSetVoicePosition_guess( hVoice_guess, pos_guess );
	A3DSource_SetGain_guess( hVoice_guess, ch->master_vol / 255.0f );
	hwSetChannelDistance_guess( ch_idx, sound_nominal_clip_dist * ch->dist_mult );

	A3D_SetCurrentSfx_guess( NULL );

	return 1;
}

/*
=================
hA3D_FeedBuffer

Streams the next part of the channel's wave into its A3D source
=================
*/
int hA3D_FeedBuffer( void *hvoice, int ch_idx, channel_t *ch, sfxcache_t *sc, int count, int unused, float pitch )
{
	int		done_guess;
	int		newState_guess;
	int		looping_guess;
	int		stop_guess;
	int		nToWrite_guess;
	byte	*psrc8_guess;
	int		oldstate_guess;
	int		streampos_guess;
	int		length_guess;

	done_guess = 0;

	oldstate_guess = hwGetChannelState_guess( ch_idx );
	newState_guess = oldstate_guess;

	if ( ch->sfx && Q_strstr( ch->sfx->name, "tride/" ) )
		oldstate_guess = oldstate_guess;

	A3DSource_Update_guess( hvoice );

	stop_guess = ( !ch->sfx || !sc || ( !ch->leftvol && !ch->rightvol ) );

	if ( stop_guess )
	{
		if ( !ch->sfx || !sc )
		{
			switch ( oldstate_guess )
			{
			case 4:
				hwStopChannel_guess( ch_idx );
				return 1;

			case 3:
				A3DSource_GetDone_guess( hvoice, &done_guess );
				if ( done_guess )
				{
					hwResetVoice_guess( hvoice );
					hwSetChannelState_guess( ch_idx, 0 );
					hwSetChannelPos_guess( ch_idx, 0 );
					A3DSource_SetStreamPos_guess( hvoice, 0 );
					hwSetSlotChannel_guess( hwFindChannelSlot_guess( ch_idx ), -1 );
				}
				return 1;

			case 1:
			case 2:
			case 5:
			case 6:
			case 7:
				hwResetVoice_guess( hvoice );
				hwSetChannelState_guess( ch_idx, 0 );
				hwSetSlotChannel_guess( hwFindChannelSlot_guess( ch_idx ), -1 );
				return 0;

			case 0:
				return 0;
			}
		}
		else if ( !ch->leftvol && !ch->rightvol )
		{
			hwStopVoice_guess( hvoice );
			return 0;
		}
	}
	else
	{
		looping_guess = sc->loopstart >= 0;
		streampos_guess = A3DSource_GetStreamPos_guess( hvoice );
		length_guess = sc->length;
	}

	if ( sc->width != 1 && sc->width != 2 )
		return 0;

	if ( ( oldstate_guess == 2 || oldstate_guess == 6 ) && !looping_guess )
	{
		hwResetVoice_guess( hvoice );
		oldstate_guess = 1;
	}

	switch ( oldstate_guess )
	{
	case 4:
		if ( A3DSource_Restart_guess( hvoice ) < 0 )
		{
			ch->pos += count;
			return 1;
		}
		hwSetChannelState_guess( ch_idx, 3 );
		// fall through

	case 3:
		A3DSource_GetDone_guess( hvoice, &done_guess );
		if ( done_guess )
		{
			hwResetVoice_guess( hvoice );
			hwSetChannelState_guess( ch_idx, 0 );
			hwSetChannelPos_guess( ch_idx, 0 );
			A3DSource_SetStreamPos_guess( hvoice, 0 );
			hwSetSlotChannel_guess( hwFindChannelSlot_guess( ch_idx ), -1 );
		}
		// fall through

	case 0:
		ch->pos += count;
		return 1;

	case 5:
	case 7:
		hwResetVoice_guess( hvoice );
		streampos_guess = 0;
		// fall through

	case 1:
	case 2:
	case 6:
		nToWrite_guess = A3DSource_GetFreeSamples_guess( hvoice );
		nToWrite_guess = min( length_guess - streampos_guess, nToWrite_guess );

		if ( looping_guess )
			newState_guess = 6;
		else
			newState_guess = 1;
		break;

	default:
		OutputDebugString( "hA3D_FeedBuffer - unhandled status.\n" );
		return 0;
	}

	if ( nToWrite_guess > 0 )
	{
		if ( sc->width == 1 )
		{
			void	*pbuf1_guess;
			DWORD	cb1_guess;
			void	*pbuf2_guess;
			DWORD	cb2_guess;
			int		j_guess;
			short	*pdst_guess;

			psrc8_guess = sc->data + streampos_guess;

			if ( A3DSource_Lock_guess( hvoice, nToWrite_guess * 2, &pbuf1_guess, &cb1_guess, &pbuf2_guess, &cb2_guess, 0 ) >= 0 )
			{
				// widen the 8 bit samples into the 16 bit source buffer
				for ( pdst_guess = (short *)pbuf1_guess, j_guess = 0; j_guess < cb1_guess / 2; j_guess++, pdst_guess++, psrc8_guess++ )
					*pdst_guess = ( *psrc8_guess << 8 ) ^ 0x80;

				for ( pdst_guess = (short *)pbuf2_guess, j_guess = 0; j_guess < cb2_guess / 2; j_guess++, pdst_guess++, psrc8_guess++ )
					*pdst_guess = ( *psrc8_guess << 8 ) ^ 0x80;

				if ( hwUnlockVoice_guess( hvoice, pbuf1_guess, cb1_guess, pbuf2_guess, cb2_guess ) < 0
					&& hwUnlockVoice_guess( hvoice, pbuf1_guess, cb1_guess, pbuf2_guess, cb2_guess ) < 0 )
				{
					hwStopVoice_guess( hvoice );
					return -1;
				}
			}
			else
			{
				return 0;
			}
		}
		else if ( sc->width == 2 )
		{
			short	*psrc16_guess;

			psrc16_guess = (short *)sc->data + streampos_guess;

			if ( A3DSource_Feed16_guess( hvoice, psrc16_guess, nToWrite_guess * 2, unused ) < 0 )
			{
				OutputDebugString( "PaintToA3D: Error feeding A3DSource data!\n" );
				return 0;
			}
		}
	}

	ch->pos += count;
	streampos_guess += nToWrite_guess;

	done_guess = 0;
	if ( streampos_guess >= length_guess )
	{
		if ( looping_guess )
			streampos_guess = sc->loopstart;
		else
			done_guess = 1;
	}

	hwSetChannelPos_guess( ch_idx, ch->pos );
	A3DSource_SetStreamPos_guess( hvoice, streampos_guess );

	if ( pitch >= 0 )
	{
		pitch = (float)sc->speed / shm->dmaspeed * pitch;
		A3DSource_SetPitch_guess( hvoice, pitch );
	}

	hwSetChannelState_guess( ch_idx, newState_guess );

	if ( done_guess )
	{
		hwStopChannel_guess( ch_idx );
	}
	else if ( !hwIsVoicePlaying_guess( hvoice ) )
	{
		hwPlayVoice_guess( hvoice, -1 );
	}

	return 1;
}

/*
=================
hwStopChannel_guess
=================
*/
void hwStopChannel_guess( int ch_idx )
{
	int		state_guess;
	void	*hvoice_guess;

	hvoice_guess = hwAllocChannel_guess( ch_idx, 0 );

	if ( !snd_isa3d_guess )
		return;

	if ( hvoice_guess )
	{
		state_guess = hwGetChannelState_guess( ch_idx );
		if ( state_guess == 4 )
		{
			if ( A3DSource_Restart_guess( hvoice_guess ) >= 0 )
				hwSetChannelState_guess( ch_idx, 3 );
			else
				hwSetChannelState_guess( ch_idx, 4 );
		}
		else if ( state_guess != 3 && state_guess != 0 )
		{
			if ( A3DSource_Restart_guess( hvoice_guess ) >= 0 )
				hwSetChannelState_guess( ch_idx, 3 );
			else
				hwSetChannelState_guess( ch_idx, 4 );
		}
	}
}
