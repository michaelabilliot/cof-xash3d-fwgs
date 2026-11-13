//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CHANGEGAMEDIALOG_H
#define CHANGEGAMEDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

namespace vgui2
{
class ListPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Lists the other games installed next to this one and switches to them
//-----------------------------------------------------------------------------
class CChangeGameDialog : public vgui2::Frame
{
	typedef vgui2::Frame BaseClass;

public:
	CChangeGameDialog( vgui2::Panel *parent );
	~CChangeGameDialog();

protected:
	virtual void OnCommand( const char *command );
	virtual void OnClose();

private:
	void LoadModList_guess();

	vgui2::ListPanel *m_pModList;
};

#endif // CHANGEGAMEDIALOG_H
