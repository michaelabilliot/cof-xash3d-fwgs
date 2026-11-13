//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef DEMOEVENTSDIALOG_H
#define DEMOEVENTSDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui = vgui2;

class IEngineWrapper;
class IDemoPlayer;
class DirectorCmd;
class CDemoEditDialog;

namespace vgui2
{
class ListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: list of the director commands of the demo being played
//-----------------------------------------------------------------------------
class CDemoEventsDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDemoEventsDialog, vgui::Frame );

public:
	CDemoEventsDialog( vgui::Panel *parent, const char *name, IEngineWrapper *engine, IDemoPlayer *demoPlayer );
	~CDemoEventsDialog();

	virtual void OnCommand( const char *command );

	MESSAGE_FUNC( OnUpdate, "UpdateCmdList" );
	MESSAGE_FUNC( OnUpdateLastCmd, "UpdateLastCmd" ) { OnUpdate(); }

protected:
	void GetCurrentCmd();
	void OnAdd();
	void OnGoto();
	void OnRemove();
	void OnModify();

	vgui2::ListPanel	*m_EventList;
	IEngineWrapper		*m_Engine;
	IDemoPlayer			*m_DemoPlayer;
	DirectorCmd			*m_CurrentCmd;
	CDemoEditDialog		*m_EditDialog;
};

#endif // DEMOEVENTSDIALOG_H
