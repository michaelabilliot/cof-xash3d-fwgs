// winquake.h: Win32-specific Quake header file

#ifndef WINQUAKE_H
#define WINQUAKE_H

#include <dsound.h>

// snd_win.c
extern LPDIRECTSOUNDBUFFER	pDSBuf;
extern DWORD				gSndBufSize;

// set while the Aureal A3D renderer mixes the sounds (snd_win.c, 0x0274D200)
extern qboolean				snd_isa3d_guess;
// set while the sound goes to four EAX buffers, one per speaker (0x0274D26C)
extern qboolean				snd_iseax_guess;

// the four EAX speaker buffers: front left, front right, rear left, rear right
extern LPDIRECTSOUNDBUFFER	pDSBufFL_guess;
extern LPDIRECTSOUNDBUFFER	pDSBufFR_guess;
extern LPDIRECTSOUNDBUFFER	pDSBufRL_guess;
extern LPDIRECTSOUNDBUFFER	pDSBufRR_guess;
// the EAX buffer of the voice (net voice chat) sounds (0x02491DAC)
extern LPDIRECTSOUNDBUFFER	pDSBufVoice_guess;

// EAX property sets of the four speaker buffers (0x02491DB0)
extern LPKSPROPERTYSET		pEAXPropFL_guess;
extern LPKSPROPERTYSET		pEAXPropFR_guess;
extern LPKSPROPERTYSET		pEAXPropRL_guess;
extern LPKSPROPERTYSET		pEAXPropRR_guess;
// the EAX room preset last set (0x02491DC4)
extern int					eaxroomtype_guess;

qboolean	SNDDMA_Init( void );
int			SNDDMA_GetDMAPos( void );
void		SNDDMA_Submit( void );
void		SNDDMA_Shutdown( void );
void		SNDDMA_UpdateA3D_guess( void );
void		S_EnableA3D_guess( void );
void		S_DisableA3D_guess( void );

// the A3D hardware voices (names unproven)
int			hwGetNumChannels_guess( void );
void		*hwAllocChannel_guess( int ch_idx, int alloc );
int			hwGetChannelState_guess( int ch_idx );
int			hwSetChannelState_guess( int ch_idx, int state );
void		hwSetVoiceRelative_guess( void *voice, int relative );
void		hwStopAllSounds_guess( void );
int			hwIsActive_guess( void );
int			hwLockBuffer_guess( DWORD **pData, DWORD *dwSize );
void		hwUnlockBuffer_guess( DWORD *pData, DWORD dwSize );
void		hwUpdateListener_guess( vec_t *origin );
void		hwSetVolume_guess( float vol );
void		hwSetListenerPosition_guess( vec_t *origin );
void		hwSetListenerOrientation_guess( vec_t *forward, vec_t *up );
void		hwSetChannelDistance_guess( int ch_idx, float dist );

// set by Sys_CheckOSVersion (sys_dll2.cpp) on Windows NT 4.0 and later (0x0250D784)
extern qboolean	WinNT;

// the game window (0x024E0754)
extern HWND		*pmainwindow;

#endif // WINQUAKE_H
