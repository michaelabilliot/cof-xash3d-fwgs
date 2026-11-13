//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyDialog.h>

namespace vgui = vgui2;

//-----------------------------------------------------------------------------
// Purpose: Holds all the game option pages
//-----------------------------------------------------------------------------
class COptionsDialog : public vgui::PropertyDialog
{
	typedef vgui::PropertyDialog BaseClass;

public:
	COptionsDialog( vgui::Panel *parent );
	~COptionsDialog();

	void Run();
	virtual void Activate();
	virtual void SetTitle( const char *title, bool surfaceTitle );

	virtual void OnClose();
};

#endif // OPTIONSDIALOG_H
