//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/KeyValues.h>

#include <vgui_controls/ToggleButton.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ToggleButton::ToggleButton(Panel *parent, const char *panelName, const char* text) : Button(parent, panelName, text)
{
	SetButtonActivationType(ACTIVATE_ONPRESSED);
}

//-----------------------------------------------------------------------------
// Purpose: Turns double-click into normal click
//-----------------------------------------------------------------------------
void ToggleButton::OnMouseDoublePressed(MouseCode code)
{
	OnMousePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Color ToggleButton::GetButtonFgColor()
{
	if (IsSelected())
	{
		// highlight the text when depressed
		return _selectedColor;
	}
	else
	{
		return BaseClass::GetButtonFgColor();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool ToggleButton::CanBeDefaultButton(void)
{
    return false;
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the state of the button
//-----------------------------------------------------------------------------
void ToggleButton::DoClick()
{
	if (IsSelected())
	{
		ForceDepressed(false);
	}
	else if (!IsSelected())
	{
		ForceDepressed(true);
	}

	SetSelected(!IsSelected());
	FireActionSignal();

	// post a button toggled message
	KeyValues *msg = new KeyValues("ButtonToggled");
	msg->SetInt("state", IsSelected());
	PostActionSignal(msg);

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ToggleButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	_selectedColor = GetSchemeColor("BrightControlText", pScheme);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ToggleButton::OnKeyCodePressed(KeyCode code)
{
	if (code != KEY_ENTER)
	{
		BaseClass::OnKeyCodePressed(code);
	}
}
