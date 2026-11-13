//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef SAVEGAMEDIALOG_H
#define SAVEGAMEDIALOG_H

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
// Purpose: Displays game save options
//-----------------------------------------------------------------------------
class CSaveGameDialog : public CTaskFrame
{
	typedef CTaskFrame BaseClass;

public:
	CSaveGameDialog( vgui2::Panel *parent );
	~CSaveGameDialog();

protected:
	enum
	{
		SAVEGAME_MAPNAME_LEN = 32,
		SAVEGAME_COMMENT_LEN = 80,
		SAVEGAME_ELAPSED_LEN = 32,
	};

	vgui2::ListPanel *m_pGameList;

	virtual void OnCommand( const char *command );
	virtual void OnClose();

	void CreateSavedGamesList();
	void ScanSavedGames();
	bool ParseSaveData( char const *pszFileName, char const *pszShortName, KeyValues *kv );
	const char *FindSaveSlot();
};

#endif // SAVEGAMEDIALOG_H
