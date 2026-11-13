//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef MENUBUTTON_H
#define MENUBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Button.h>
#include "vgui_controls/Menu.h"

namespace vgui2
{

class Menu;
class TextImage;

//-----------------------------------------------------------------------------
// Purpose: Button that displays a menu when pressed
//-----------------------------------------------------------------------------
class MenuButton : public Button
{
	DECLARE_CLASS_SIMPLE( MenuButton, Button );

public:
	MenuButton(Panel *parent, const char *panelName, const char *text);
	~MenuButton();

	// functions designed to be overriden
	virtual void OnShowMenu(Menu *menu) {}
	virtual void OnHideMenu(Menu *menu) {}

	virtual void SetMenu(Menu *menu);
	virtual void HideMenu(void);
	virtual void OnKillFocus();
	virtual void DrawFocusBorder(int tx0, int ty0, int tx1, int ty1);
	MESSAGE_FUNC( OnMenuClose, "MenuClose" );
	virtual void DoClick();
	virtual void SetOpenOffsetY(int yOffset);

	virtual bool CanBeDefaultButton(void);

	// sets the direction in which the menu opens from the button, defaults to down
	enum MenuDirection_e
	{
		LEFT,
		RIGHT,
		UP,
		DOWN,
		CURSOR,	// make the menu appear under the mouse cursor
		ALIGN_WITH_PARENT, // make the menu appear under the parent
	};
	virtual void SetOpenDirection(MenuDirection_e direction);

	virtual void OnKeyCodeTyped(KeyCode code);
	virtual void OnCursorEntered();

private:
	Menu *m_pMenu;
	MenuDirection_e m_iDirection;

	int _openOffsetY; // vertical offset of menu from the menu button
};

} // namespace vgui2

#endif // MENUBUTTON_H
