//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERLISTDIALOG_H
#define PLAYERLISTDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui2
{
class Button;
class ListPanel;
};

namespace vgui = vgui2;

//-----------------------------------------------------------------------------
// Purpose: List of players, their ingame-name and their mute state
//-----------------------------------------------------------------------------
class CPlayerListDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPlayerListDialog, vgui::Frame );

public:
	CPlayerListDialog( vgui::Panel *parent );
	~CPlayerListDialog();

	virtual void Activate();

private:
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	virtual void OnCommand( const char *command );

	void ToggleMuteStateOfSelectedUser();
	void RefreshPlayerProperties();

	vgui::ListPanel *m_pPlayerList;
	vgui::Button *m_pMuteButton;
};

#endif // PLAYERLISTDIALOG_H
