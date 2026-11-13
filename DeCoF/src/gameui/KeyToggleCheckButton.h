//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef KEYTOGGLECHECKBUTTON_H
#define KEYTOGGLECHECKBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/CheckButton.h>

namespace vgui = vgui2;

class CKeyToggleCheckButton : public vgui::CheckButton
{
	typedef vgui::CheckButton BaseClass;

public:
	CKeyToggleCheckButton( vgui::Panel *parent, const char *panelName, const char *text, char const *keyname, char const *cmdname );
	~CKeyToggleCheckButton();

	virtual void Paint();

	void Reset();
	void ApplyChanges();
	bool HasBeenModified();

private:
	char *m_pszKeyName;
	char *m_pszCmdName;

	bool m_bStartValue;
};

#endif // KEYTOGGLECHECKBUTTON_H
