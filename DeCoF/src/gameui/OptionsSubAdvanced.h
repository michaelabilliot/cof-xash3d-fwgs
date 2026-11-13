//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_ADVANCED_H
#define OPTIONS_SUB_ADVANCED_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

namespace vgui = vgui2;

class CContentControlDialog;

namespace vgui2
{
class CheckButton;
};

//-----------------------------------------------------------------------------
// Purpose: Advanced options, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubAdvanced : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubAdvanced, vgui::PropertyPage );

public:
	COptionsSubAdvanced( vgui::Panel *parent );
	~COptionsSubAdvanced();

	virtual void OnResetData();
	virtual void OnApplyChanges();

	virtual void OnCommand( const char *command );

private:
	void OnOpenContentControlDialog();

	MESSAGE_FUNC( OnContentControlClose, "ContentControlClose" );

	CContentControlDialog *m_pContentControlDialog;
	vgui::CheckButton *m_pContentCheckButton;
};

#endif // OPTIONS_SUB_ADVANCED_H
