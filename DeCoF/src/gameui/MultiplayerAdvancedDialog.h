//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef MULTIPLAYERADVANCEDDIALOG_H
#define MULTIPLAYERADVANCEDDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include "taskframe.h"
#include <vgui_controls/PropertyPage.h>

class CInfoDescription;
class mpcontrol_t;
struct CPanelListPanel;

//-----------------------------------------------------------------------------
// Purpose: builds the controls of the multiplayer options from user.scr
//-----------------------------------------------------------------------------
class CMultiplayerAdvanced
{
protected:
	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();

	CInfoDescription *m_pDescription;

	mpcontrol_t *m_pList;

	CPanelListPanel *m_pListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Displays a game-specific list of options
//-----------------------------------------------------------------------------
class CMultiplayerAdvancedDialog : public CTaskFrame, public CMultiplayerAdvanced
{
	typedef CTaskFrame BaseClass;

public:
	CMultiplayerAdvancedDialog( vgui2::Panel *parent );
	~CMultiplayerAdvancedDialog();

	virtual void OnCommand( const char *command );
	virtual void OnClose();
};

//-----------------------------------------------------------------------------
// Purpose: the same options as a property page
//-----------------------------------------------------------------------------
class CMultiplayerAdvancedPage : public vgui2::PropertyPage, public CMultiplayerAdvanced
{
public:
	CMultiplayerAdvancedPage( vgui2::Panel *parent );
	~CMultiplayerAdvancedPage();

	virtual void OnResetData();
	virtual void OnApplyChanges();
};

#endif // MULTIPLAYERADVANCEDDIALOG_H
