//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Interface to the Career UI, for use by the client dll
//
// $NoKeywords: $
//=============================================================================

#ifndef ICAREERUI_H
#define ICAREERUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <vector>

enum CareerDifficultyType
{
	CAREER_DIFFICULTY_EASY = 0,
	CAREER_DIFFICULTY_NORMAL,
	CAREER_DIFFICULTY_HARD,
	CAREER_DIFFICULTY_EXPERT,

	MAX_CAREER_DIFFICULTY
};

//-----------------------------------------------------------------------------
// Purpose: An interface to a career task
//-----------------------------------------------------------------------------
class ICareerTask
{
public:
	virtual bool IsComplete() = 0;
	virtual bool IsCompletedThisRound() = 0;
	virtual bool IsCompletedThisMatch() = 0;
	virtual const char *GetMap() = 0;
	virtual const char *GetTaskName() = 0;
	virtual const wchar_t *GetLocalizedTaskName() = 0;
	virtual const wchar_t *GetLocalizedTaskNameWithCompletion() = 0;
	virtual void Reset() = 0;
	virtual void Set( bool ) = 0;
	virtual void StartRound() = 0;
	virtual const char *GetWeaponName() const = 0;
	virtual int GetRepeat() const = 0;
	virtual bool MustSurvive() const = 0;
	virtual bool InARow() const = 0;
	virtual int GetPartial() const = 0;
	virtual void SetPartial( int ) = 0;
	virtual bool IsMaxRoundTime() const = 0;
};

typedef std::vector< ICareerTask * > ITaskVec;
typedef ITaskVec::iterator ITaskVecIt;

struct MapInfo
{
	const char *name;
	bool defeated;
};

//-----------------------------------------------------------------------------
// Purpose: An interface to the Career UI
//-----------------------------------------------------------------------------
class ICareerUI : public IBaseInterface
{
public:
	virtual bool IsPlayingMatch() = 0;
	virtual ITaskVec *GetCurrentTaskVec() = 0;
	virtual bool PlayAsCT() = 0;
	virtual int GetReputationGained() = 0;
	virtual int GetNumMapsUnlocked() = 0;
	virtual bool DoesWinUnlockAll() = 0;
	virtual int GetRoundTimeLength() = 0;
	virtual int GetWinfastLength() = 0;
	virtual CareerDifficultyType GetDifficulty() const = 0;
	virtual int GetCurrentMapTriplet( MapInfo *maps ) = 0;
	virtual void OnRoundEndMenuOpen( bool didWin ) = 0;
	virtual void OnMatchEndMenuOpen( bool didWin ) = 0;
	virtual void OnRoundEndMenuClose( bool stillPlaying ) = 0;
	virtual void OnMatchEndMenuClose( bool stillPlaying ) = 0;
};

#define CAREERUI_INTERFACE_VERSION "CareerUI001"

#endif // ICAREERUI_H
