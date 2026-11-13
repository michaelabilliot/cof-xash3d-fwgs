//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef DEMOPLAYERFILEDIALOG_H
#define DEMOPLAYERFILEDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui2
{
class ListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Lists the demo files and posts the selected one
//-----------------------------------------------------------------------------
class CDemoPlayerFileDialog : public vgui2::Frame
{
	typedef vgui2::Frame BaseClass;

public:
	CDemoPlayerFileDialog( vgui2::Panel *parent, const char *name );
	~CDemoPlayerFileDialog();

	virtual void OnClose();
	virtual void OnCommand( const char *command );

private:
	void LoadDemoList();

	vgui2::ListPanel *m_pDemoList;
};

#endif // DEMOPLAYERFILEDIALOG_H
