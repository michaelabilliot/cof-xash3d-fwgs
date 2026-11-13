//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The menu music player
//
// $NoKeywords: $
//=============================================================================

#ifndef MUSICMANAGER_H
#define MUSICMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h>

#include "IMusicManager.h"

//-----------------------------------------------------------------------------
// Purpose: Plays the menu and round end music tracks
//-----------------------------------------------------------------------------
class MusicManager : public IMusicManager
{
public:
	MusicManager();
	virtual ~MusicManager();

	virtual void StartTrack( TrackType track, bool looping );
	virtual void StopTrack();
	virtual void FadeOutTrack();
	virtual bool IsPlayingTrack();
	virtual bool IsPlayingTrack( TrackType track );

	void Init();
	void Reset();

private:
	TrackType m_currentTrack;

	struct TrackInfo
	{
		char t_path[_MAX_PATH];
		bool looping;
	};
	TrackInfo m_tracks[TRACKTYPE_NUM];

	bool m_useLoopingInfo;
};

extern MusicManager *TheMusicManager;

void CreateMusicManager();
void ShutdownMusicManager();

#endif // MUSICMANAGER_H
