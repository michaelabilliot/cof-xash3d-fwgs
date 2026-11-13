// cd_internal.h -- the CD audio (MCI) and MP3 (Miles) music player

#ifndef CD_INTERNAL_H
#define CD_INTERNAL_H

#include "cd.h"

typedef struct _MP3_AUDIO_STATE
{
	bool	inuse;
	bool	suspended;
	bool	playing;
	char	trackname[512];
	int		tracknum;
	bool	looping;
	int		volume;
} MP3_AUDIO_STATE;

class CCDAudio : public ICDAudio
{
public:
	CCDAudio( void );
	virtual ~CCDAudio( void );

	int		Init( void );
	void	Shutdown( void );
	void	Play( int track, bool looping );
	void	PlayFile( const char *filename, bool looping );
	void	Stop( void );
	void	FadeOut( void );
	void	Pause( void );
	void	Resume( void );
	void	Frame( void );
	void	MP3_Resume_Audio( void );
	void	MP3_Suspend_Audio( void );

	void	CD_f( void );
	void	PrimeTrack( char *filename, int looping );

	void	ResetCDTimes( void );
	void	Eject( void );
	void	CloseDoor( void );
	void	GetAudioDiskInfo( void );
	void	SwitchToLauncher( void );
	void	SwitchToEngine( void );
	void	Reset( void );

	// these run on the CD thread (IThread::AddThreadItem)
	void	_Init( int, int );
	void	_Stop( int, int );
	void	_Pause( int, int );
	void	_Eject( int, int );
	void	_CloseDoor( int, int );
	void	_GetAudioDiskInfo( int, int );
	void	_Play( int track, int looping );
	void	_PrimeTrack( int track, int looping );
	void	_Resume( int, int );
	void	_SwitchToLauncher( int, int );
	void	_SwitchToEngine( int, int );
	void	_CDUpdate( int, int );
	void	_CDReset( int, int );

	bool	MP3_PlayTrack( int trackNum, bool looping );
	bool	MP3_PlayTrack( const char *filename, bool looping );
	void	MP3_PlayTrackFinalize( int trackNum, bool looping );
	bool	MP3_Init( void );
	void	MP3_Shutdown( void );
	int		MP3_SetVolume( int NewVol );
	bool	MP3_InitStream( int trackNum, bool looping );
	bool	MP3_InitStream( const char *filename, bool looping );
	void	MP3_StartStream( void );
	void	MP3_StopStream( void );
	void	MP3_SetPause( bool OnOff );
	void	MP3_ReleaseDriver( void );
	void	MP3_Loop( void );

private:
	float			m_flPlayTime;
	double			m_dStartTime;
	double			m_dPauseTime;
	double			m_dFadeOutTime;

	bool			m_bIsCDValid;
	bool			m_bIsPlaying;
	bool			m_bWasPlaying;
	bool			m_bInitialized;
	bool			m_bEnabled;
	bool			m_bIsLooping;
	bool			m_bIsPrimed;
	bool			m_bIsInMiddleOfPriming;

	float			m_flVolume;
	int				m_flMP3Volume;
	int				m_nPlayTrack;
	char			m_szPendingPlayFilename[512];
	int				m_nMaxCDTrack;
	bool			m_bResumeOnSwitch;
	unsigned int	m_uiDeviceID;
	int				m_rgRemapCD[100];

	MP3_AUDIO_STATE	m_MP3;
};

#endif // CD_INTERNAL_H
