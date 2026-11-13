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

#include <vgui_controls/CheckButton.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Check box image
//-----------------------------------------------------------------------------
class CheckImage : public TextImage
{
public:
	CheckImage(CheckButton *CheckButton) : TextImage( "g" )
	{
		_CheckButton = CheckButton;

		SetSize(20, 13);
	}

	virtual void Paint()
	{
		DrawSetTextFont(GetFont());

		// draw background
		if (_CheckButton->IsEnabled() && _CheckButton->m_bCheckButtonCheckable)
		{
			DrawSetTextColor(_bgColor);
		}
		else
		{
			DrawSetTextColor(_CheckButton->GetBgColor());
		}
		DrawPrintChar(0, 1, 'g');

		// draw border box
		DrawSetTextColor(_borderColor1);
		DrawPrintChar(0, 1, 'e');
		DrawSetTextColor(_borderColor2);
		DrawPrintChar(0, 1, 'f');

		// draw selected check
		if (_CheckButton->IsSelected())
		{
			DrawSetTextColor(_checkColor);
			DrawPrintChar(0, 2, 'b');
		}
	}

	Color _borderColor1;
	Color _borderColor2;
	Color _checkColor;

	Color _bgColor;

private:
	CheckButton *_CheckButton;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CheckButton::CheckButton(Panel *parent, const char *panelName, const char *text) : ToggleButton(parent, panelName, text)
{
	SetContentAlignment(a_west);
	m_bCheckButtonCheckable = true;

	// create the image
	_checkBoxImage = new CheckImage(this);

	SetTextImageIndex(1);
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	_selectedFgColor = Color( 196, 181, 80, 255 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CheckButton::~CheckButton()
{
	delete _checkBoxImage;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CheckButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetDefaultColor( GetSchemeColor("FgColor", pScheme), GetBgColor() );
	_checkBoxImage->_bgColor = GetSchemeColor("CheckBgColor", Color(62, 70, 55, 255), pScheme);
	_checkBoxImage->_borderColor1 = GetSchemeColor("CheckButtonBorder1", Color(20, 20, 20, 255), pScheme);
	_checkBoxImage->_borderColor2 = GetSchemeColor("CheckButtonBorder2", Color(90, 90, 90, 255), pScheme);
	_checkBoxImage->_checkColor = GetSchemeColor("CheckButtonCheck", Color(20, 20, 20, 255), pScheme);
	_selectedFgColor = GetSchemeColor("BrightControlText", GetSchemeColor("ControlText", pScheme), pScheme);

	_checkBoxImage->SetFont( pScheme->GetFont("Marlett", IsProportional()) );
	_checkBoxImage->ResizeImageToContent();
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	// don't draw a background
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IBorder *CheckButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Check the button
//-----------------------------------------------------------------------------
void CheckButton::SetSelected(bool state )
{
	if (m_bCheckButtonCheckable)
	{
		// send a message saying we've been checked
		KeyValues *msg = new KeyValues("CheckButtonChecked", "state", (int)state);
		PostActionSignal(msg);

		BaseClass::SetSelected(state);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the state of the check can be changed
//-----------------------------------------------------------------------------
void CheckButton::SetCheckButtonCheckable(bool state)
{
	m_bCheckButtonCheckable = state;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Gets a different foreground text color if we are selected
//-----------------------------------------------------------------------------
Color CheckButton::GetButtonFgColor()
{
	if (IsSelected())
	{
		return _selectedFgColor;
	}

	return BaseClass::GetButtonFgColor();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CheckButton::OnCheckButtonChecked(Panel *panel)
{
}
