//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <vgui/IBorder.h>
#include <vgui/IScheme.h>
#include <vgui/KeyCode.h>

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/FocusNavGroup.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertyPage::PropertyPage(Panel *parent, const char *panelName, bool paintBorder) : EditablePanel(parent, panelName)
{
	_paintRaised = paintBorder;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertyPage::~PropertyPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when page is loaded.  Data should be reloaded from document into controls.
//-----------------------------------------------------------------------------
void PropertyPage::OnResetData()
{
}

//-----------------------------------------------------------------------------
// Purpose: Called when the OK / Apply button is pressed.  Changed data should be written into document.
//-----------------------------------------------------------------------------
void PropertyPage::OnApplyChanges()
{
}

//-----------------------------------------------------------------------------
// Purpose: Designed to be overriden
//-----------------------------------------------------------------------------
void PropertyPage::OnPageShow()
{
}

//-----------------------------------------------------------------------------
// Purpose: Designed to be overriden
//-----------------------------------------------------------------------------
void PropertyPage::OnPageHide()
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pageTab -
//-----------------------------------------------------------------------------
void PropertyPage::OnPageTabActivated(Panel *pageTab)
{
	_pageTab = pageTab;
}

//-----------------------------------------------------------------------------
// Purpose: Draws border
//-----------------------------------------------------------------------------
void PropertyPage::PaintBorder()
{
	IBorder *border = GetBorder();

	if (_paintRaised == true && border && _pageTab.Get())
	{
		// get the tab's position
		int px, py, pwide, ptall;
		_pageTab->GetBounds(px, py, pwide, ptall);

		int wide, tall;
		GetSize(wide, tall);
		border->Paint(0, 0, wide, tall, IBorder::SIDE_TOP, px + 1, px + pwide - 1);
	}
	else
	{
		BaseClass::PaintBorder();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles key presses
//-----------------------------------------------------------------------------
void PropertyPage::OnKeyCodeTyped(KeyCode code)
{
	switch (code)
	{
		// left and right only get propogated to parents if our tab has focus
	case KEY_RIGHT:
		{
			if (_pageTab != 0 && _pageTab->HasFocus())
				BaseClass::OnKeyCodeTyped(code);
			break;
		}
	case KEY_LEFT:
		{
			if (_pageTab != 0 && _pageTab->HasFocus())
				BaseClass::OnKeyCodeTyped(code);
			break;
		}
	default:
		BaseClass::OnKeyCodeTyped(code);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the border
//-----------------------------------------------------------------------------
void PropertyPage::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	if (_paintRaised)
	{
		SetBorder(pScheme->GetBorder("ButtonBorder"));
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void PropertyPage::SetVisible(bool state)
{
	if (IsVisible() && !state)
	{
		// if we're going away and we have a current button, get rid of it
		if (GetFocusNavGroup().GetCurrentDefaultButton())
		{
			GetFocusNavGroup().SetCurrentDefaultButton(NULL);
		}
	}

	BaseClass::SetVisible(state);
}
