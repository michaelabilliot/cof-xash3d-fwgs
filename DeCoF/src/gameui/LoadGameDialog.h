//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef LOADGAMEDIALOG_H
#define LOADGAMEDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"

class KeyValues;

namespace vgui2
{
class ListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Displays game loading options
//-----------------------------------------------------------------------------
class CLoadGameDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CLoadGameDialog( vgui2::Panel *parent );
	~CLoadGameDialog();

protected:
	vgui2::ListPanel *m_pGameList;

	virtual void OnCommand( const char *command );
	virtual void OnClose();

	void CreateSavedGamesList();
	void ScanSavedGames();
	bool ParseSaveData( char const *pszFileName, char const *pszShortName, KeyValues *kv );
};

#endif // LOADGAMEDIALOG_H
