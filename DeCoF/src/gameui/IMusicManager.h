//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Interface to the menu music player
//
// $NoKeywords: $
//=============================================================================

#ifndef IMUSICMANAGER_H
#define IMUSICMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

enum TrackType
{
	TRACKTYPE_INVALID = 1,
	TRACKTYPE_SPLASH,
	TRACKTYPE_ROUNDWIN,
	TRACKTYPE_ROUNDLOSE,
	TRACKTYPE_MATCHWIN,
	TRACKTYPE_MATCHLOSE,
	TRACKTYPE_CREDITS,

	TRACKTYPE_NUM
};

//-----------------------------------------------------------------------------
// Purpose: Plays the menu and round end music tracks
//-----------------------------------------------------------------------------
class IMusicManager : public IBaseInterface
{
public:
	virtual void StartTrack( TrackType track, bool looping ) = 0;
	virtual void StopTrack() = 0;
	virtual void FadeOutTrack() = 0;
	virtual bool IsPlayingTrack() = 0;
	virtual bool IsPlayingTrack( TrackType track ) = 0;
};

#endif // IMUSICMANAGER_H
