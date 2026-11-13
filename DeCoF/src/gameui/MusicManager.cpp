//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The menu music player
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "MusicManager.h"
#include "CareerGame.h"
#include "EngineInterface.h"

#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>

EXPOSE_SINGLE_INTERFACE( MusicManager, IMusicManager, "MusicManager001" );

MusicManager *TheMusicManager = NULL;

//-----------------------------------------------------------------------------
// Purpose: Points TheMusicManager at the singleton and reads the playlist
//-----------------------------------------------------------------------------
void CreateMusicManager()
{
	TheMusicManager = &__g_MusicManager_singleton;
	TheMusicManager->Init();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ShutdownMusicManager()
{
	TheMusicManager = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
MusicManager::MusicManager()
{
	m_currentTrack = TRACKTYPE_INVALID;

	for ( int i=0; i<TRACKTYPE_NUM; ++i )
	{
		m_tracks[i].t_path[0] = 0;
		m_tracks[i].looping = false;
	}

	m_useLoopingInfo = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
MusicManager::~MusicManager()
{
}

//-----------------------------------------------------------------------------
// Purpose: Stops the current track
//-----------------------------------------------------------------------------
void MusicManager::Reset()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		StopTrack();
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads the track names from playlist.txt
//-----------------------------------------------------------------------------
void MusicManager::Init()
{
	Reset();

	KeyValues *playlistKeyValues = new KeyValues( "Playlist" );
	const char *filenameStr = "";
	if ( playlistKeyValues->LoadFromFile( vgui2::filesystem(), "playlist.txt" ) )
	{
		int version = playlistKeyValues->GetInt( "Version", 1 );
		if ( version > 1 )
		{
			m_useLoopingInfo = true;
		}

		filenameStr = playlistKeyValues->GetString( "Splash", "" );
		strncpy( m_tracks[TRACKTYPE_SPLASH].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_SPLASH].t_path ) );
		m_tracks[TRACKTYPE_SPLASH].looping = playlistKeyValues->GetInt( "SplashLoop", 1 ) != 0;

		filenameStr = playlistKeyValues->GetString( "RoundWin", "" );
		strncpy( m_tracks[TRACKTYPE_ROUNDWIN].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_ROUNDWIN].t_path ) );
		m_tracks[TRACKTYPE_ROUNDWIN].looping = playlistKeyValues->GetInt( "RoundWinLoop", 0 ) != 0;

		filenameStr = playlistKeyValues->GetString( "RoundLose", "" );
		strncpy( m_tracks[TRACKTYPE_ROUNDLOSE].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_ROUNDLOSE].t_path ) );
		m_tracks[TRACKTYPE_ROUNDLOSE].looping = playlistKeyValues->GetInt( "RoundLoseLoop", 0 ) != 0;

		filenameStr = playlistKeyValues->GetString( "MatchWin", "" );
		strncpy( m_tracks[TRACKTYPE_MATCHWIN].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_MATCHWIN].t_path ) );
		m_tracks[TRACKTYPE_MATCHWIN].looping = playlistKeyValues->GetInt( "MatchWinLoop", 0 ) != 0;

		filenameStr = playlistKeyValues->GetString( "MatchLose", "" );
		strncpy( m_tracks[TRACKTYPE_MATCHLOSE].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_MATCHLOSE].t_path ) );
		m_tracks[TRACKTYPE_MATCHLOSE].looping = playlistKeyValues->GetInt( "MatchLoseLoop", 0 ) != 0;

		filenameStr = playlistKeyValues->GetString( "Credits", "" );
		strncpy( m_tracks[TRACKTYPE_CREDITS].t_path, filenameStr, sizeof( m_tracks[TRACKTYPE_CREDITS].t_path ) );
		m_tracks[TRACKTYPE_CREDITS].looping = playlistKeyValues->GetInt( "CreditsLoop", 1 ) != 0;
	}

	playlistKeyValues->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Plays a track through the engine's cd emulation
//-----------------------------------------------------------------------------
void MusicManager::StartTrack( TrackType track, bool looping )
{
	if ( m_currentTrack == track )
		return;

	if ( track <= TRACKTYPE_INVALID || track >= TRACKTYPE_NUM )
		return;

	if ( m_useLoopingInfo )
	{
		looping = m_tracks[track].looping;
	}

	Career_Printf( "MusicManager::StartTrack - playing track %d %s\n", track, ( looping ) ? "as looping" : "" );

	engine->pfnPrimeMusicStream( m_tracks[track].t_path, looping != 0 );

	char command[128];
	sprintf( command, "cd %s %s\n", ( looping ) ? "loopfile" : "playfile", m_tracks[track].t_path );
	engine->pfnClientCmd( command );

	m_currentTrack = track;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void MusicManager::StopTrack()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		engine->pfnClientCmd( "cd stop" );
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void MusicManager::FadeOutTrack()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		engine->pfnClientCmd( "cd fadeout" );
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool MusicManager::IsPlayingTrack()
{
	return m_currentTrack != TRACKTYPE_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool MusicManager::IsPlayingTrack( TrackType track )
{
	return m_currentTrack == track;
}
