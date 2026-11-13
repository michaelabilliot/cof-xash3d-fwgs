//mp3 support added by Killar

#include "hud.h"
#include "cl_util.h"
#include "mp3.h"

#include <irrKlang.h>

using namespace irrklang;

char *UTIL_VarArgs_guess( const char *format, ... );

// static: the wrappers keep g_pMP3_guess in a register across the calls into CMP3
static CMP3		*g_pMP3_guess;			// 0x105440A4
static ISoundEngine	*g_pSoundEngine_guess;	// 0x105440A8
static ISound		*g_pSFX_guess;			// 0x105440AC, the looping sound effect (type 1)
static ISound		*g_pMusic_guess;		// 0x105440B0

CMP3::CMP3()
{
	VectorClear( m_vecUnknown08_guess );
	VectorClear( m_vecUnknown14_guess );
	VectorClear( m_vecUnknown20_guess );
}

void CMP3::Disabled_guess( void )
{
	return;
}

void CMP3::Frame( void )
{
	if ( !g_pSoundEngine_guess || !m_bInitialized_guess )
		return;

	if ( !m_pLocalPlayer_guess )
		m_pLocalPlayer_guess = gEngfuncs.GetLocalPlayer();

	if ( g_pSFX_guess )
	{
		float flVolume = CVAR_GET_FLOAT( "volume" );
		if ( flVolume > 1 )
			flVolume = 1;
		if ( flVolume < 0 )
			flVolume = 0;
		g_pSFX_guess->setVolume( flVolume );
	}

	if ( g_pMusic_guess )
	{
		float flVolume = CVAR_GET_FLOAT( "MP3Volume" );
		if ( flVolume > 1 )
			flVolume = 1;
		if ( flVolume < 0 )
			flVolume = 0;
		g_pMusic_guess->setVolume( flVolume );
	}

	// buz - fade out the music
	if ( g_pMusic_guess && m_flFadeoutStart + m_flFadeoutDuration > gEngfuncs.GetClientTime() )
	{
		float delta = ( gEngfuncs.GetClientTime() - m_flFadeoutStart ) / m_flFadeoutDuration;
		if ( delta >= 0.95 )
		{
			// fadeout finished, turn off
			g_pMusic_guess->stop();
			return;
		}

		int vol = ( CVAR_GET_FLOAT( "MP3Volume" ) * 255 ) - (int)( delta * 255 );
		if ( vol < 0 )
			vol = 0;
		if ( vol > 255 )
			vol = 255;

		float flVolume = vol / 255.0f;
		if ( flVolume < 0 )
			flVolume = 0;
		if ( flVolume > 1 )
			flVolume = 1;

		if ( flVolume > 0.05 )
			g_pMusic_guess->setVolume( flVolume );
		else
			g_pMusic_guess->stop();
	}

	if ( m_bHold_guess && g_pMusic_guess )
	{
		float flVolume = m_iVolume_guess / 255.0f;
		if ( flVolume < 0 )
			flVolume = 0;
		if ( flVolume > 1 )
			flVolume = 1;
		g_pMusic_guess->setVolume( flVolume );
	}
}

int CMP3::Initialize( void )
{
	m_bInitialized_guess = true;

	if ( g_pSoundEngine_guess )
	{
		m_pszPending_guess = NULL;
		m_pLocalPlayer_guess = NULL;
		m_iUnknown2C_guess = 0;
		m_iUnknown30_guess = 0;
		m_bUnknown34_guess = false;
		m_bUnknown35_guess = false;
		m_szMP3File[0] = 0;
		m_iIsPlaying = 0;
		m_iIsPaused_guess = 0;
		m_iVolume_guess = 0;
		m_bHold_guess = false;
		m_flFadeoutStart = 0;
		m_flFadeoutDuration = 0;
		m_flFadeinStart_guess = 0;
		m_flFadeinDuration_guess = 0;
		m_bLoop_guess = false;
		m_bStopping_guess = false;
		return 1;
	}

	return 0;
}

void IrrKlangCreate_guess( void )
{
	IrrKlangDestroy_guess();

	g_pMP3_guess = new CMP3;
	if ( g_pMP3_guess )
	{
		g_pMP3_guess->m_bInitialized_guess = false;
		g_pSoundEngine_guess = createIrrKlangDevice();
	}
}

void IrrKlangDestroy_guess( void )
{
	if ( g_pSoundEngine_guess )
		g_pSoundEngine_guess->drop();

	if ( g_pMP3_guess )
	{
		delete g_pMP3_guess;
		g_pMP3_guess = NULL;
	}
}

void IrrKlangFrame_guess( void )
{
	if ( g_pMP3_guess )
	{
		if ( !g_pMP3_guess->m_bInitialized_guess )
			g_pMP3_guess->Initialize();
		g_pMP3_guess->Frame();
	}
}

void IrrKlangInitialize_guess( void )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->Initialize();
}

bool IrrKlangIsPaused_guess( void )
{
	if ( g_pMP3_guess && g_pMP3_guess->m_iIsPaused_guess )
		return true;
	return false;
}

void IrrKlangPlayMP3_guess( const char *pszFile )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->PlayMP3( pszFile );
}

void IrrKlangPlaySFX_guess( const char *pszFile, int iType, bool bFlag, vec3_t vecOrigin )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->PlaySFX_guess( pszFile, iType, bFlag, vecOrigin );
}

void IrrKlangSetFade_guess( float flParm1, float flParm2, float flParm3, float flParm4 )
{
	if ( g_pMP3_guess )
	{
		if ( flParm1 >= 0 )
			g_pMP3_guess->m_flFadeoutStart = flParm1;
		if ( flParm2 >= 0 )
			g_pMP3_guess->m_flFadeoutDuration = flParm2;
		if ( flParm3 >= 0 )
			g_pMP3_guess->m_flFadeinStart_guess = flParm3;
		if ( flParm4 >= 0 )
			g_pMP3_guess->m_flFadeinDuration_guess = flParm4;
	}
}

void IrrKlangSetLoop_guess( bool bLoop )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->m_bLoop_guess = bLoop;
}

void IrrKlangSetPaused_guess( bool bPaused )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->SetPaused_guess( bPaused );
}

void IrrKlangSetStopping_guess( bool bStopping )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->m_bStopping_guess = bStopping;
}

void IrrKlangStop_guess( float flFadeTime )
{
	if ( g_pMP3_guess )
		g_pMP3_guess->StopMP3( flFadeTime );
}

// prints the average value of a block of bytes, never called
void __stdcall OutputAverage_guess( const unsigned char *pData, unsigned int iSize )
{
	int iTotal = 0;

	for ( unsigned int i = 0; i < iSize; i++ )
		iTotal += pData[i];

	gEngfuncs.pfnCenterPrint( UTIL_VarArgs_guess( "%.3f\n", (float)iTotal / (float)iSize ) );
}

int CMP3::PlayMP3( const char *pszSong )
{
	if ( !m_bInitialized_guess || !g_pSoundEngine_guess || gEngfuncs.CheckParm( "-nomusic", NULL ) )
		return 0;

	// the end songs never loop
	if ( pszSong )
	{
		if ( !strcmp( pszSong, "ap3hall1end.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "aparthall2end.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "aparthall2end2.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "aphall1end.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "basement1end.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "cityend.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "coopend.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "doctorend.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "docvic.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "hit1.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "hit2.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "hit3.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "nightmareend.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "sawend.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "sewerhit.mp3" ) )
			m_bLoop_guess = false;
		if ( !strcmp( pszSong, "trainhit.mp3" ) )
			m_bLoop_guess = false;
	}

	if ( m_bHold_guess )
	{
		m_pszPending_guess = pszSong;
		return 0;
	}

	if ( m_pszPending_guess && pszSong && !strcmp( m_pszPending_guess, pszSong ) )
		return 0;

	// buz: we are playing this song now, dont do anything
	if ( !strcmp( pszSong, m_szMP3File ) )
		return 0;

	m_flFadeoutStart = 0;
	m_flFadeoutDuration = 0;
	m_flFadeinStart_guess = 0;
	m_flFadeinDuration_guess = 0;

	char song[256];

	sprintf( song, "%s/mp3/%s", gEngfuncs.pfnGetGameDirectory(), pszSong );

	if ( m_bLoop_guess )
	{
		if ( g_pMusic_guess )
			g_pMusic_guess->stop();
		g_pMusic_guess = g_pSoundEngine_guess->play2D( song, true, true, true );
		if ( g_pMusic_guess )
		{
			float flVolume = CVAR_GET_FLOAT( "MP3Volume" );
			if ( flVolume > 1 )
				flVolume = 1;
			if ( flVolume < 0 )
				flVolume = 0;
			g_pMusic_guess->setVolume( flVolume );
			g_pMusic_guess->setIsPaused( false );
		}
	}
	else
	{
		if ( g_pMusic_guess )
			g_pMusic_guess->stop();
		g_pMusic_guess = g_pSoundEngine_guess->play2D( song, false, true, true );
		if ( g_pMusic_guess )
		{
			float flVolume = CVAR_GET_FLOAT( "MP3Volume" );
			if ( flVolume > 1 )
				flVolume = 1;
			if ( flVolume < 0 )
				flVolume = 0;
			g_pMusic_guess->setVolume( flVolume );
			g_pMusic_guess->setIsPaused( false );
		}
	}

	m_iIsPlaying = 1;
	strcpy( m_szMP3File, pszSong );
	return 1;
}

// PlayMP3 with a full path, never called
int CMP3::PlayMP3Path_guess( const char *pszPath )
{
	if ( !m_bInitialized_guess || !g_pSoundEngine_guess || gEngfuncs.CheckParm( "-nomusic", NULL ) )
		return 0;

	if ( m_iIsPlaying && g_pMusic_guess )
		g_pMusic_guess->stop();

	char song[256];

	sprintf( song, "%s", pszPath );

	if ( g_pMusic_guess )
		g_pMusic_guess->stop();
	g_pMusic_guess = g_pSoundEngine_guess->play2D( pszPath, false, true, true );
	if ( g_pMusic_guess )
	{
		g_pMusic_guess->setVolume( CVAR_GET_FLOAT( "MP3Volume" ) );
		g_pMusic_guess->setIsPaused( false );
	}

	m_iIsPlaying = 1;
	return 1;
}

void CMP3::PlaySFX_guess( const char *pszSample, int iType, bool bFlag, Vector vecOrigin )
{
	if ( !m_bInitialized_guess || !g_pSoundEngine_guess )
		return;

	char szNewSample[256];
	char szPath[256];

	// weapons/<folder>/<file> plays weapons_new/<file>
	if ( strstr( pszSample, "weapons/" ) )
	{
		bool bFound = false;
		int j = 0;

		for ( unsigned int i = 0; i < strlen( pszSample ); i++ )
		{
			if ( bFound )
				szPath[j++] = pszSample[i];
			else if ( pszSample[i] == '/' || pszSample[i] == '\\' )
				bFound = true;
		}

		if ( bFound )
		{
			szPath[j] = 0;
			strcpy( szNewSample, "weapons_new/" );
			strcat( szNewSample, szPath );
			pszSample = szNewSample;
		}
	}

	if ( iType == 1 )
	{
		if ( g_pSFX_guess )
			g_pSFX_guess->stop();

		sprintf( szPath, "%s/sound/%s", gEngfuncs.pfnGetGameDirectory(), pszSample );
		g_pSFX_guess = g_pSoundEngine_guess->play2D( szPath, true, true, true );
		if ( g_pSFX_guess )
		{
			float flVolume = CVAR_GET_FLOAT( "volume" );
			if ( flVolume > 1 )
				flVolume = 1;
			if ( flVolume < 0 )
				flVolume = 0;
			g_pSFX_guess->setVolume( flVolume );
			g_pSFX_guess->setPlaybackSpeed( gEngfuncs.pfnRandomFloat( -0.1, 0.1 ) + 1.0 );
			g_pSFX_guess->setIsPaused( false );
		}
	}
	else if ( iType == 2 )
	{
		if ( g_pSFX_guess )
			g_pSFX_guess->stop();
	}
	else
	{
		sprintf( szPath, "%s/sound/%s", gEngfuncs.pfnGetGameDirectory(), pszSample );
		ISound *pSound = g_pSoundEngine_guess->play2D( szPath, false, true, true );
		if ( pSound )
		{
			float flVolume = CVAR_GET_FLOAT( "volume" );
			if ( flVolume > 1 )
				flVolume = 1;
			if ( flVolume < 0 )
				flVolume = 0;
			pSound->setVolume( flVolume );
			pSound->setPlaybackSpeed( gEngfuncs.pfnRandomFloat( -0.1, 0.1 ) + 1.0 );
			pSound->setIsPaused( false );
		}
	}
}

int CMP3::SetPaused_guess( bool bPaused )
{
	if ( !m_bInitialized_guess || !g_pSoundEngine_guess )
		return 0;

	if ( bPaused )
	{
		m_iIsPaused_guess = 1;
		g_pSoundEngine_guess->setAllSoundsPaused( true );
		return 1;
	}

	m_iIsPaused_guess = 0;
	g_pSoundEngine_guess->setAllSoundsPaused( false );
	return 1;
}

int CMP3::Shutdown( void )
{
	return 1;
}

int CMP3::StopMP3( float fade )
{
	if ( !m_bInitialized_guess )
		return 0;

	if ( !g_pSoundEngine_guess || gEngfuncs.CheckParm( "-nomusic", NULL ) )
		return 1;

	m_bStopping_guess = true;

	if ( m_bHold_guess )
		m_bHold_guess = false;

	m_szMP3File[0] = 0;

	if ( fade == 0 )
	{
		if ( g_pMusic_guess )
			g_pMusic_guess->stop();
		m_iIsPlaying = 0;
		m_flFadeoutStart = 0;
		m_flFadeoutDuration = 0;
		m_bStopping_guess = false;
		return 1;
	}

	m_flFadeoutStart = gEngfuncs.GetClientTime();
	m_flFadeoutDuration = fade;
	return 1;
}
