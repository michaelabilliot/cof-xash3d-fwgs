//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The tasks of a career map
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERGAMETASKS_H
#define CAREERGAMETASKS_H

#ifdef _WIN32
#pragma once
#endif

#include "ICareerUI.h"

//-----------------------------------------------------------------------------
// Purpose: A task as the career screens show it
//-----------------------------------------------------------------------------
class CCareerUITask : public ICareerTask
{
public:
	CCareerUITask( const char *taskName );

	virtual bool IsComplete() { return m_isComplete; }
	virtual bool IsCompletedThisRound() { return m_completedThisRound; }
	virtual bool IsCompletedThisMatch() { return m_isComplete; }
	virtual const char *GetMap() { return ""; }
	virtual const char *GetTaskName() { return m_name; }
	virtual const wchar_t *GetLocalizedTaskName() { return m_localizedName; }
	virtual const wchar_t *GetLocalizedTaskNameWithCompletion();

	virtual void Reset() { m_completedThisRound = false; m_isComplete = false; m_partials = 0; }
	virtual void Set( bool thisRound ) { m_completedThisRound = true; m_isComplete = true; m_partials = 0; }
	virtual void StartRound() { m_completedThisRound = false; }

	virtual const char *GetWeaponName() const { return m_weapon; }
	virtual int GetRepeat() const { return m_repeat; }
	virtual bool MustSurvive() const { return m_survive; }
	virtual bool InARow() const { return m_inarow; }
	virtual int GetPartial() const { return m_partials; }
	virtual void SetPartial( int num ) { m_partials = num; }
	virtual bool IsMaxRoundTime() const { return m_isWinFast; }

protected:
	bool m_isComplete;
	bool m_completedThisRound;
	char m_name[128];
	char m_weapon[128];
	wchar_t m_localizedName[512];
	wchar_t m_localizedNameCompleted[512];
	int m_repeat;
	bool m_survive;
	bool m_inarow;
	int m_partials;
	bool m_isWinFast;
};

#endif // CAREERGAMETASKS_H
