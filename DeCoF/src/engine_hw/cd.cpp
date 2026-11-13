// cd.cpp -- CD audio (MCI) and MP3 (Miles) music player; the commands run on the CD thread

#include <windows.h>
#include <mmsystem.h>
#include <time.h>

extern "C"
{
#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "mathlib.h"
#include "host.h"
#include "winquake.h"
#include "cdaudio.h"

extern	cvar_t	bgmvolume;
extern	cvar_t	MP3Volume;
extern	cvar_t	MP3FadeTime;

void	FS_GetLocalCopy( char *pszFileName );
}

#include "mss.h"
#include "cd.h"
#include "cd_internal.h"
#include "ithread.h"

HDIGDRIVER	MP3digitalDriver;
HSTREAM		MP3stream;

static CCDAudio g_CDAudio;
ICDAudio *cdaudio = &g_CDAudio;

// files of the MP3 tracks, by track number: 0 and 1 are not music,
// the next ones replace the Half-Life CD tracks, the rest is added by MP3_GetTrack
char	*g_pszMP3trackFileMap[200];
int		g_iMP3FirstMalloc, g_iMP3NumTracks;

// seconds a CD track may run past its length before it counts as finished
const float TRACK_EXTRA_TIME = 2.0;

HWND GetMainWindow( void )
{
	return *pmainwindow;
}

CCDAudio *GetInteralCDAudio( void )
{
	return &g_CDAudio;
}

void CD_Command_f( void )
{
	g_CDAudio.CD_f();
}

int MP3_GetTrack( const char *pszTrack )
{
	int i = 0, iTrack = 0;

	if ( pszTrack && !Q_strstr( pszTrack, ":" ) && !Q_strstr( pszTrack, ".." ) )
	{
		char *pszFormat = "%s";
		char szTemp[MAX_PATH];

		if ( !Q_strstr( pszTrack, ".mp3" ) )
			pszFormat = "%s.mp3";

		_snprintf( szTemp, sizeof( szTemp ) - 1, pszFormat, pszTrack );
		szTemp[sizeof( szTemp ) - 1] = 0;
		COM_FixSlashes( szTemp );

		for ( i = 0; i < 200; i++ )
		{
			if ( !g_pszMP3trackFileMap[i] )
			{
				g_pszMP3trackFileMap[i] = Mem_Strdup( szTemp );
				iTrack = i;
				g_iMP3NumTracks = i + 1;
				break;
			}

			if ( !strcmp( g_pszMP3trackFileMap[i], szTemp ) )
			{
				iTrack = i;
				break;
			}
		}
	}

	return iTrack;
}

void MP3_Command_f( void )
{
	char *pszCommand = NULL, *pszTrack = NULL;

	if ( Cmd_Argc() < 2 )
		return;

	pszCommand = Cmd_Argv( 1 );
	pszTrack = Cmd_Argv( 2 );

	if ( !_stricmp( pszCommand, "play" ) )
	{
		g_CDAudio.Stop();
		g_CDAudio.Play( MP3_GetTrack( pszTrack ), false );
	}

	if ( !_stricmp( pszCommand, "playfile" ) )
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile( pszTrack, false );
	}

	if ( !_stricmp( pszCommand, "loop" ) )
	{
		g_CDAudio.Stop();
		g_CDAudio.Play( MP3_GetTrack( pszTrack ), true );
	}

	if ( !_stricmp( pszCommand, "loopfile" ) )
	{
		g_CDAudio.Stop();
		g_CDAudio.PlayFile( pszTrack, true );
	}

	if ( !_stricmp( pszCommand, "stop" ) )
	{
		g_CDAudio.Stop();
	}
}

void PrimeMusicStream( char *filename, int looping )
{
	g_CDAudio.PrimeTrack( filename, looping );
}

CCDAudio::CCDAudio( void )
{
	ResetCDTimes();

	m_bIsCDValid = false;
	m_bIsPlaying = false;
	m_bIsPrimed = false;
	m_bIsInMiddleOfPriming = false;
	m_bWasPlaying = false;
	m_bInitialized = false;
	m_bEnabled = false;
	m_bIsLooping = false;
	m_flVolume = 1.0;
	m_flMP3Volume = 127;
	m_dFadeOutTime = 0.0;
	m_nPlayTrack = 0;
	m_szPendingPlayFilename[0] = 0;
	m_nMaxCDTrack = 0;
	m_bResumeOnSwitch = false;
	m_uiDeviceID = 0;

	memset( m_rgRemapCD, 0, sizeof( m_rgRemapCD ) );
}

CCDAudio::~CCDAudio( void )
{
}

void CCDAudio::ResetCDTimes( void )
{
	m_flPlayTime = 0.0;
	m_dStartTime = 0.0;
	m_dPauseTime = 0.0;
}

void CCDAudio::_Stop( int, int )
{
	if ( !m_bEnabled )
		return;

	if ( !m_bIsPlaying && !m_bWasPlaying )
		return;

	m_bIsPlaying = false;
	m_bWasPlaying = false;

	MP3_StopStream();

	m_MP3.inuse = false;
	m_MP3.suspended = false;
	m_MP3.playing = false;
	m_MP3.trackname[0] = 0;
	m_MP3.tracknum = 0;
	m_MP3.looping = false;
	m_MP3.volume = m_flMP3Volume;

	mciSendCommand( m_uiDeviceID, MCI_STOP, 0, (DWORD)NULL );

	ResetCDTimes();
}

void CCDAudio::_Pause( int, int )
{
	MCI_GENERIC_PARMS	mciGenericParms;

	if ( !m_bEnabled )
		return;

	if ( !m_bIsPlaying )
		return;

	if ( m_MP3.inuse )
	{
		MP3_SetPause( true );
	}
	else
	{
		mciGenericParms.dwCallback = (DWORD)GetMainWindow();
		mciSendCommand( m_uiDeviceID, MCI_PAUSE, 0, (DWORD)(LPVOID)&mciGenericParms );
	}

	m_bWasPlaying = m_bIsPlaying;
	m_bIsPlaying = false;
	m_dPauseTime = realtime;
}

void CCDAudio::_Eject( int, int )
{
	DWORD	dwReturn;

	_Stop( 0, 0 );

	dwReturn = mciSendCommand( m_uiDeviceID, MCI_SET, MCI_SET_DOOR_OPEN, (DWORD)NULL );

	ResetCDTimes();
}

void CCDAudio::_CloseDoor( int, int )
{
	DWORD	dwReturn;

	dwReturn = mciSendCommand( m_uiDeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, (DWORD)NULL );

	ResetCDTimes();
}

void CCDAudio::_GetAudioDiskInfo( int, int )
{
	DWORD				dwReturn;
	MCI_STATUS_PARMS	mciStatusParms;

	m_bIsCDValid = false;

	mciStatusParms.dwItem = MCI_STATUS_READY;
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms );
	if ( dwReturn )
		return;

	if ( !mciStatusParms.dwReturn )
		return;

	mciStatusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms );
	if ( dwReturn )
		return;

	if ( mciStatusParms.dwReturn < 1 )
		return;

	m_bIsCDValid = true;
	m_nMaxCDTrack = mciStatusParms.dwReturn;
}

void CCDAudio::_Play( int track, int looping )
{
	int					mins, secs;
	DWORD				dwReturn;
	MCI_PLAY_PARMS		mciPlayParms;
	MCI_STATUS_PARMS	mciStatusParms;
	DWORD				trackLength_guess;

	if ( !m_bEnabled )
		return;

	if ( !track && strlen( m_szPendingPlayFilename ) > 0 )
	{
		MP3_PlayTrack( m_szPendingPlayFilename, looping ? true : false );
		m_szPendingPlayFilename[0] = 0;
		return;
	}

	_GetAudioDiskInfo( 0, 0 );

	if ( !m_bIsCDValid )
	{
		MP3_PlayTrack( track, looping ? true : false );
		return;
	}

	if ( track > m_nMaxCDTrack )
	{
		MP3_PlayTrack( track, looping ? true : false );
	}

	track = m_rgRemapCD[track];

	if ( track < 1 )
		return;

	// don't try to play a non-audio track
	mciStatusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms );
	if ( dwReturn )
	{
		UINT	cchError_guess = 256;
		char	szError_guess[256];

		mciGetErrorString( dwReturn, szError_guess, cchError_guess );
		return;
	}

	if ( mciStatusParms.dwReturn != MCI_CDA_TRACK_AUDIO )
		return;

	// get the length of the track to be played
	mciStatusParms.dwItem = MCI_STATUS_LENGTH;
	mciStatusParms.dwTrack = track;
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK | MCI_WAIT, (DWORD)(LPVOID)&mciStatusParms );
	if ( dwReturn )
		return;

	if ( m_bIsPlaying )
	{
		if ( m_nPlayTrack == track )
			return;

		_Stop( 0, 0 );
	}

	trackLength_guess = mciStatusParms.dwReturn;

	mciPlayParms.dwFrom = MCI_MAKE_TMSF( track, 0, 0, 0 );
	mciPlayParms.dwTo = ( mciStatusParms.dwReturn << 8 ) | track;
	mciPlayParms.dwCallback = (DWORD)GetMainWindow();
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_PLAY, MCI_NOTIFY | MCI_FROM | MCI_TO, (DWORD)(LPVOID)&mciPlayParms );
	if ( dwReturn )
		return;

	ResetCDTimes();

	m_dStartTime = realtime;

	mins = MCI_MSF_MINUTE( trackLength_guess );
	secs = MCI_MSF_SECOND( trackLength_guess );
	m_flPlayTime = mins * 60.0f + secs;

	m_bIsLooping = looping ? true : false;
	m_nPlayTrack = track;
	m_bIsPlaying = true;

	if ( m_flVolume == 0.0 )
		_Pause( 0, 0 );
}

void CCDAudio::_PrimeTrack( int track, int looping )
{
	m_bIsInMiddleOfPriming = true;

	if ( !track )
	{
		if ( strlen( m_szPendingPlayFilename ) > 0 )
		{
			MP3_InitStream( m_szPendingPlayFilename, looping != 0 );
			m_bIsPrimed = true;
		}
	}
	else if ( track > 1 && track < g_iMP3NumTracks )
	{
		MP3_InitStream( track, looping != 0 );
		m_bIsPrimed = true;
	}

	m_bIsInMiddleOfPriming = false;
}

void CCDAudio::_Resume( int, int )
{
	double			curTime;
	DWORD			dwReturn;
	MCI_PLAY_PARMS	mciPlayParms;

	if ( !m_bEnabled )
		return;

	if ( !m_bWasPlaying )
		return;

	if ( m_MP3.inuse )
	{
		MP3_SetPause( false );
	}
	else
	{
		mciPlayParms.dwFrom = MCI_MAKE_TMSF( m_nPlayTrack, 0, 0, 0 );
		mciPlayParms.dwTo = MCI_MAKE_TMSF( m_nPlayTrack + 1, 0, 0, 0 );
		mciPlayParms.dwCallback = (DWORD)GetMainWindow();
		dwReturn = mciSendCommand( m_uiDeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD)(LPVOID)&mciPlayParms );
		if ( dwReturn )
		{
			ResetCDTimes();
			return;
		}
	}

	m_bIsPlaying = true;

	if ( !m_MP3.inuse )
	{
		curTime = realtime;
		m_dStartTime += curTime - m_dPauseTime;
		m_dPauseTime = 0.0;
	}
}

void CCDAudio::_SwitchToLauncher( int, int )
{
	if ( m_bEnabled && m_bIsPlaying )
	{
		m_bResumeOnSwitch = true;
		_Pause( 0, 0 );
	}
}

void CCDAudio::_SwitchToEngine( int, int )
{
	if ( m_bResumeOnSwitch )
	{
		m_bResumeOnSwitch = false;
		_Resume( 0, 0 );
	}
}

void CCDAudio::CD_f( void )
{
	char	*command;
	int		ret;
	int		n;

	if ( Cmd_Argc() < 2 )
		return;

	command = Cmd_Argv( 1 );

	if ( !_stricmp( command, "on" ) )
	{
		m_bEnabled = true;
		return;
	}

	if ( !_stricmp( command, "off" ) )
	{
		if ( m_bIsPlaying )
			Stop();
		m_bEnabled = false;
		return;
	}

	if ( !_stricmp( command, "reset" ) )
	{
		m_bEnabled = true;
		if ( m_bIsPlaying )
			Stop();
		for ( n = 0; n < 100; n++ )
			m_rgRemapCD[n] = n;
		GetAudioDiskInfo();
		return;
	}

	if ( !_stricmp( command, "remap" ) )
	{
		ret = Cmd_Argc() - 2;
		if ( ret > 0 )
		{
			for ( n = 1; n <= ret; n++ )
				m_rgRemapCD[n] = atoi( Cmd_Argv( n + 1 ) );
		}
		return;
	}

	if ( !_stricmp( command, "close" ) )
	{
		CloseDoor();
		return;
	}

	if ( !_stricmp( command, "mp3info" ) )
	{
		Con_Printf( "Current MP3 Title: %s\n", m_MP3.trackname );
		Con_Printf( "Current MP3 Track: %i\n", m_MP3.tracknum );
		Con_Printf( "Current MP3 Volume: %i\n", m_MP3.volume );
		return;
	}

	if ( Cmd_Argc() > 2 && !_stricmp( command, "mp3track" ) )
	{
		n = atoi( Cmd_Argv( 2 ) );
		n++;

		if ( !MP3_PlayTrack( n, false ) )
			return;

		ResetCDTimes();
		m_bIsLooping = false;
		m_nPlayTrack = n;
		m_bIsPlaying = true;
		return;
	}

	if ( !_stricmp( command, "play" ) )
	{
		Play( atoi( Cmd_Argv( 2 ) ), false );
		return;
	}

	if ( !_stricmp( command, "playfile" ) )
	{
		PlayFile( Cmd_Argv( 2 ), false );
		return;
	}

	if ( !_stricmp( command, "loop" ) )
	{
		Play( atoi( Cmd_Argv( 2 ) ), true );
		return;
	}

	if ( !_stricmp( command, "loopfile" ) )
	{
		PlayFile( Cmd_Argv( 2 ), true );
		return;
	}

	if ( !_stricmp( command, "stop" ) )
	{
		Stop();
		return;
	}

	if ( !_stricmp( command, "fadeout" ) )
	{
		FadeOut();
		return;
	}

	if ( !_stricmp( command, "pause" ) )
	{
		Pause();
		return;
	}

	if ( !_stricmp( command, "resume" ) )
	{
		Resume();
		return;
	}

	if ( !_stricmp( command, "eject" ) )
	{
		if ( m_bIsPlaying )
			Stop();
		Eject();
		m_bIsCDValid = false;
		return;
	}

	if ( !_stricmp( command, "info" ) )
	{
		Con_Printf( "%u tracks\n", g_iMP3NumTracks - 1 );
		if ( m_bIsPlaying )
			Con_Printf( "Currently %s track %u\n", m_bIsLooping ? "looping" : "playing", m_nPlayTrack );
		else if ( m_bWasPlaying )
			Con_Printf( "Paused %s track %u\n", m_bIsLooping ? "looping" : "playing", m_nPlayTrack );
		Con_Printf( "Volume is %f\n", m_flVolume );
	}
}

void CCDAudio::_CDUpdate( int, int )
{
	double	curVolume;
	double	current;

	if ( !m_bIsPlaying )
		return;

	curVolume = MP3Volume.value;

	if ( m_dFadeOutTime != 0.0 )
	{
		double curTime_guess = Sys_FloatTime();

		if ( curTime_guess >= m_dFadeOutTime )
		{
			_Stop( 0, 0 );
			m_dFadeOutTime = 0.0;
		}
		else
		{
			curVolume = MP3Volume.value * ( ( m_dFadeOutTime - curTime_guess ) / MP3FadeTime.value );
		}

		MP3_SetVolume( curVolume * 127 );

		if ( thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 ) )
			Sleep( 0 );
	}

	if ( !m_flPlayTime || !m_dStartTime )
		return;

	current = realtime;

	// the track has run out: start it again when looping
	if ( (float)( current - m_dStartTime ) >= TRACK_EXTRA_TIME + m_flPlayTime )
	{
		if ( m_bIsPlaying )
		{
			m_bIsPlaying = false;

			if ( m_bIsLooping )
				Play( m_nPlayTrack, true );
		}
	}
}

int CCDAudio::Init( void )
{
	m_MP3.inuse = false;
	m_MP3.suspended = false;
	m_MP3.playing = false;
	m_MP3.trackname[0] = 0;
	m_MP3.tracknum = 0;
	m_MP3.looping = false;
	m_MP3.volume = 100;

	memset( g_pszMP3trackFileMap, 0, sizeof( g_pszMP3trackFileMap ) );

	g_pszMP3trackFileMap[0] = "";
	g_pszMP3trackFileMap[1] = "";
	g_pszMP3trackFileMap[2] = "media\\Half-Life01.mp3";
	g_pszMP3trackFileMap[3] = "media\\Prospero01.mp3";
	g_pszMP3trackFileMap[4] = "media\\Half-Life12.mp3";
	g_pszMP3trackFileMap[5] = "media\\Half-Life07.mp3";
	g_pszMP3trackFileMap[6] = "media\\Half-Life10.mp3";
	g_pszMP3trackFileMap[7] = "media\\Suspense01.mp3";
	g_pszMP3trackFileMap[8] = "media\\Suspense03.mp3";
	g_pszMP3trackFileMap[9] = "media\\Half-Life09.mp3";
	g_pszMP3trackFileMap[10] = "media\\Half-Life02.mp3";
	g_pszMP3trackFileMap[11] = "media\\Half-Life13.mp3";
	g_pszMP3trackFileMap[12] = "media\\Half-Life04.mp3";
	g_pszMP3trackFileMap[13] = "media\\Half-Life15.mp3";
	g_pszMP3trackFileMap[14] = "media\\Half-Life14.mp3";
	g_pszMP3trackFileMap[15] = "media\\Half-Life16.mp3";
	g_pszMP3trackFileMap[16] = "media\\Suspense02.mp3";
	g_pszMP3trackFileMap[17] = "media\\Half-Life03.mp3";
	g_pszMP3trackFileMap[18] = "media\\Half-Life08.mp3";
	g_pszMP3trackFileMap[19] = "media\\Prospero02.mp3";
	g_pszMP3trackFileMap[20] = "media\\Half-Life05.mp3";
	g_pszMP3trackFileMap[21] = "media\\Prospero04.mp3";
	g_pszMP3trackFileMap[22] = "media\\Half-Life11.mp3";
	g_pszMP3trackFileMap[23] = "media\\Half-Life06.mp3";
	g_pszMP3trackFileMap[24] = "media\\Prospero03.mp3";
	g_pszMP3trackFileMap[25] = "media\\Half-Life17.mp3";
	g_pszMP3trackFileMap[26] = "media\\Prospero05.mp3";
	g_pszMP3trackFileMap[27] = "media\\Suspense05.mp3";
	g_pszMP3trackFileMap[28] = "media\\Suspense07.mp3";

	g_iMP3NumTracks = g_iMP3FirstMalloc = 29;

	ResetCDTimes();

	if ( MP3_Init() )
		MP3_SetVolume( MP3Volume.value * 127 );

	m_bInitialized = true;
	m_bEnabled = true;

	if ( COM_CheckParm( "-nocdaudio" ) || COM_CheckParm( "-nosound" ) )
		m_bEnabled = false;

	thread->AddThreadItem( &CCDAudio::_Init, 0, 0 );

	return 0;
}

void CCDAudio::_Init( int, int )
{
	DWORD			dwReturn;
	MCI_OPEN_PARMS	mciOpenParms;
	MCI_SET_PARMS	mciSetParms;
	int				n;

	if ( COM_CheckParm( "-nocdaudio" ) )
		return;

	mciOpenParms.lpstrDeviceType = "cdaudio";
	dwReturn = mciSendCommand( 0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_SHAREABLE, (DWORD)(LPVOID)&mciOpenParms );
	if ( dwReturn )
		return;

	m_uiDeviceID = mciOpenParms.wDeviceID;

	// set the time format to track/minute/second/frame (TMSF)
	mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
	dwReturn = mciSendCommand( m_uiDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)(LPVOID)&mciSetParms );
	if ( dwReturn )
	{
		mciSendCommand( m_uiDeviceID, MCI_CLOSE, 0, (DWORD)NULL );
		return;
	}

	for ( n = 0; n < 100; n++ )
		m_rgRemapCD[n] = n;

	GetAudioDiskInfo();
}

void CCDAudio::Shutdown( void )
{
	if ( m_MP3.inuse )
	{
		MP3_SetPause( true );
		MP3_StopStream();
	}

	MP3_Shutdown();

	if ( !m_bInitialized )
		return;

	_Stop( 0, 0 );
	mciSendCommand( m_uiDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD)NULL );
	ResetCDTimes();
}

void CCDAudio::_CDReset( int, int )
{
	Shutdown();
	Sleep( 1000 );
	Init();
}

void CCDAudio::Reset( void )
{
	thread->AddThreadItem( &CCDAudio::_CDReset, 0, 0 );
}

void CCDAudio::Stop( void )
{
	thread->AddThreadItem( &CCDAudio::_Stop, 0, 0 );
}

void CCDAudio::Pause( void )
{
	if ( !m_bInitialized )
		return;

	thread->AddThreadItem( &CCDAudio::_Pause, 0, 0 );
}

void CCDAudio::Eject( void )
{
	thread->AddThreadItem( &CCDAudio::_Eject, 0, 0 );
}

void CCDAudio::CloseDoor( void )
{
	thread->AddThreadItem( &CCDAudio::_CloseDoor, 0, 0 );
}

void CCDAudio::GetAudioDiskInfo( void )
{
	thread->AddThreadItem( &CCDAudio::_GetAudioDiskInfo, 0, 0 );
}

void CCDAudio::Play( int track, bool looping )
{
	m_dFadeOutTime = 0.0;
	thread->AddThreadItem( &CCDAudio::_Play, track, looping ? 1 : 0 );
}

void CCDAudio::PlayFile( const char *filename, bool looping )
{
	m_dFadeOutTime = 0.0;
	strncpy( m_szPendingPlayFilename, filename, sizeof( m_szPendingPlayFilename ) );
	thread->AddThreadItem( &CCDAudio::_Play, 0, looping ? 1 : 0 );
	Sleep( 0 );
}

void CCDAudio::PrimeTrack( char *filename, int looping )
{
	time_t	startTime;
	time_t	curTime;

	m_dFadeOutTime = 0.0;
	strncpy( m_szPendingPlayFilename, filename, sizeof( m_szPendingPlayFilename ) );
	thread->AddThreadItem( &CCDAudio::_PrimeTrack, 0, looping );
	Sleep( 0 );

	// wait up to two seconds for the CD thread to open the stream
	startTime = timeGetTime();
	curTime = startTime;

	while ( m_bIsInMiddleOfPriming && ( curTime - startTime ) < 2000 )
	{
		Sleep( 0 );
		curTime = timeGetTime();
	}
}

void CCDAudio::Resume( void )
{
	if ( !m_bInitialized )
		return;

	thread->AddThreadItem( &CCDAudio::_Resume, 0, 0 );
}

void CCDAudio::SwitchToLauncher( void )
{
	thread->AddThreadItem( &CCDAudio::_SwitchToLauncher, 0, 0 );
}

void CCDAudio::SwitchToEngine( void )
{
	thread->AddThreadItem( &CCDAudio::_SwitchToEngine, 0, 0 );
}

void CCDAudio::FadeOut( void )
{
	m_dFadeOutTime = realtime + MP3FadeTime.value;
	thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 );
}

void CCDAudio::Frame( void )
{
	int		volume_guess;

	if ( !m_bEnabled )
		return;

	if ( m_flVolume != bgmvolume.value )
	{
		if ( m_flVolume )
		{
			m_flVolume = 0.0;
			Pause();
		}
		else
		{
			m_flVolume = 1.0;
			Resume();
		}

		Cvar_DirectSet( &bgmvolume, va( "%f", m_flVolume ) );
	}

	if ( m_dFadeOutTime == 0.0 )
	{
		volume_guess = MP3Volume.value * 127;

		if ( m_flMP3Volume != volume_guess )
			m_flMP3Volume = MP3_SetVolume( volume_guess );
	}

	thread->AddThreadItem( &CCDAudio::_CDUpdate, 0, 0 );
}

void CDAudio_Init( void )
{
	thread->Init();
}

void CDAudio_Shutdown( void )
{
	thread->Shutdown();
}

void CDAudio_Play( int track, int looping )
{
	cdaudio->Play( track, looping ? true : false );
}

void CDAudio_Pause( void )
{
	cdaudio->Pause();
}

void CDAudio_Resume( void )
{
	cdaudio->Resume();
}

void MP3_Resume_Audio( void )
{
	cdaudio->MP3_Resume_Audio();
}

void MP3_Suspend_Audio( void )
{
	cdaudio->MP3_Suspend_Audio();
}

bool CCDAudio::MP3_PlayTrack( int trackNum, bool looping )
{
	if ( trackNum < 2 || trackNum >= g_iMP3NumTracks )
		return false;

	if ( !m_bIsPrimed && !MP3_InitStream( trackNum, looping ) )
		return false;

	m_bIsPrimed = false;
	MP3_PlayTrackFinalize( trackNum, looping );

	return true;
}

bool CCDAudio::MP3_PlayTrack( const char *filename, bool looping )
{
	if ( !m_bIsPrimed && !MP3_InitStream( filename, looping ) )
		return false;

	m_bIsPrimed = false;
	MP3_PlayTrackFinalize( 0, looping );

	return true;
}

void CCDAudio::MP3_PlayTrackFinalize( int trackNum, bool looping )
{
	MP3_StartStream();

	m_MP3.inuse = true;
	m_MP3.playing = true;
	m_MP3.tracknum = trackNum;
	m_MP3.looping = looping;

	ResetCDTimes();

	m_bIsLooping = looping;
	m_nPlayTrack = trackNum;
	m_bIsPlaying = true;

	if ( m_flMP3Volume == 0 )
		_Pause( 0, 0 );
}

bool CCDAudio::MP3_Init( void )
{
	MP3_ReleaseDriver();

	char *pszOldDir_guess = AIL_set_redist_directory( "." );

	AIL_startup();
	AIL_set_preference( 15, 0 );

	MP3digitalDriver = AIL_open_digital_driver( 22050, 16, 2, 1 );

	return MP3digitalDriver != NULL;
}

void CCDAudio::MP3_Shutdown( void )
{
	int i = 0;

	MP3_StopStream();
	MP3_ReleaseDriver();
	AIL_shutdown();

	for ( i = g_iMP3FirstMalloc; i < 200; i++ )
	{
		if ( g_pszMP3trackFileMap[i] )
			Mem_Free( g_pszMP3trackFileMap[i] );
	}
}

void CCDAudio::MP3_Resume_Audio( void )
{
	MP3_Init();

	if ( m_MP3.playing && m_MP3.suspended )
	{
		if ( m_MP3.tracknum )
			MP3_PlayTrack( m_MP3.tracknum, m_MP3.looping );
		else
			MP3_PlayTrack( m_MP3.trackname, m_MP3.looping );
	}
}

void CCDAudio::MP3_Suspend_Audio( void )
{
	m_MP3.playing = m_bIsPlaying;

	if ( m_bIsPlaying && MP3stream )
		m_MP3.suspended = true;

	MP3_StopStream();
	MP3_ReleaseDriver();
}

void CCDAudio::MP3_ReleaseDriver( void )
{
	if ( MP3digitalDriver )
	{
		AIL_close_digital_driver( MP3digitalDriver );
		MP3digitalDriver = NULL;
	}
}

int CCDAudio::MP3_SetVolume( int NewVol )
{
	if ( NewVol < 0 )
		NewVol = 0;
	else if ( NewVol > 127 )
		NewVol = 127;

	if ( m_MP3.volume == NewVol )
		return -1;

	m_MP3.volume = NewVol;

	if ( MP3stream )
		AIL_set_stream_volume( MP3stream, m_MP3.volume );

	return m_MP3.volume;
}

bool CCDAudio::MP3_InitStream( int trackNum, bool looping )
{
	char fullPath[512];

	m_bIsPrimed = false;

	if ( !MP3digitalDriver && !MP3_Init() )
		return false;

	FS_GetLocalPath( g_pszMP3trackFileMap[trackNum], fullPath, sizeof( fullPath ) );
	FS_GetLocalCopy( fullPath );

	_Stop( 0, 0 );
	m_dFadeOutTime = 0.0;

	MP3stream = AIL_open_stream( MP3digitalDriver, fullPath, 0 );

	if ( MP3stream )
	{
		AIL_set_stream_user_data( MP3stream, 0, AIL_stream_playback_rate( MP3stream ) );
		AIL_set_stream_volume( MP3stream, m_MP3.volume );

		strcpy( m_MP3.trackname, g_pszMP3trackFileMap[trackNum] );

		if ( looping )
			MP3_Loop();

		Con_DPrintf( "MP3_InitStream(%d, %s) successful\n", trackNum, g_pszMP3trackFileMap[trackNum] );
		return true;
	}

	Con_DPrintf( "warning: MP3_InitStream(%d, %s) failed\n", trackNum, g_pszMP3trackFileMap[trackNum] );
	return false;
}

bool CCDAudio::MP3_InitStream( const char *filename, bool looping )
{
	int iTrackNum = 0;

	if ( !filename || !strlen( filename ) )
		return false;

	return MP3_InitStream( MP3_GetTrack( filename ), looping );
}

void CCDAudio::MP3_StartStream( void )
{
	if ( MP3stream )
		AIL_start_stream( MP3stream );
}

void CCDAudio::MP3_StopStream( void )
{
	if ( MP3stream )
	{
		AIL_close_stream( MP3stream );
		MP3stream = NULL;
	}

	m_bIsPrimed = false;
}

void CCDAudio::MP3_Loop( void )
{
	if ( MP3stream )
		AIL_set_stream_loop_count( MP3stream, 0 );
}

void CCDAudio::MP3_SetPause( bool OnOff )
{
	if ( MP3stream )
		AIL_pause_stream( MP3stream, OnOff );
}
