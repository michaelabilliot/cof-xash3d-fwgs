//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui/MouseCode.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

static const float NOB_SIZE_guess = 8.0f;

//-----------------------------------------------------------------------------
// Purpose: Create a slider bar with ticks underneath it
//-----------------------------------------------------------------------------
Slider::Slider(Panel *parent, const char *panelName) : Panel(parent, panelName)
{
	_dragging = false;
	_value = 0;
	_range[0] = 0;
	_range[1] = 0;
	_buttonOffset = 0;
	_sliderBorder = NULL;
	_insetBorder = NULL;
	m_nNumTicks = 10;
	_leftCaption = NULL;
	_rightCaption = NULL;

	SetThumbWidth(8);
	RecomputeNobPosFromValue();
	AddActionSignalTarget(this);
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the slider bar.
//			Warning less than 30 pixels tall and everything probably won't fit.
//-----------------------------------------------------------------------------
void Slider::OnSizeChanged(int wide, int tall)
{
	BaseClass::OnSizeChanged(wide, tall);

	RecomputeNobPosFromValue();
}

//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider to one of the ticks.
//-----------------------------------------------------------------------------
void Slider::SetValue(int value, bool bTriggerChangeMessage)
{
	int oldValue = _value;

	if (value < _range[0])
	{
		value = _range[0];
	}
	if (value > _range[1])
	{
		value = _range[1];
	}

	_value = value;

	RecomputeNobPosFromValue();

	if (_value != oldValue && bTriggerChangeMessage)
	{
		SendSliderMovedMessage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the value of the slider
//-----------------------------------------------------------------------------
int Slider::GetValue()
{
	return _value;
}

//-----------------------------------------------------------------------------
// Purpose: Layout the slider before drawing it on screen.
//-----------------------------------------------------------------------------
void Slider::PerformLayout()
{
	BaseClass::PerformLayout();
	RecomputeNobPosFromValue();

	if (_leftCaption)
	{
		_leftCaption->ResizeImageToContent();
	}
	if (_rightCaption)
	{
		_rightCaption->ResizeImageToContent();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Move the nob on the slider in response to changing its value.
//-----------------------------------------------------------------------------
void Slider::RecomputeNobPosFromValue()
{
	int wide, tall;
	GetPaintSize(wide, tall);

	float fwide = (float)wide;
	float frange = (float)(_range[1] - _range[0]);
	float fvalue = (float)(_value - _range[0]);
	float fper = fvalue / frange;
	float freepixels = fwide - _nobSize;
	float leftpixel = 0;
	float firstpixel = freepixels * fper + leftpixel;

	_nobPos[0] = (int)(firstpixel);
	_nobPos[1] = (int)(firstpixel + _nobSize);

	if (_nobPos[1] > wide)
	{
		_nobPos[0] = wide - (int)_nobSize;
		_nobPos[1] = wide;
	}

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Sync the slider's value up with the nob's position.
//-----------------------------------------------------------------------------
void Slider::RecomputeValueFromNobPos()
{
	int wide, tall;
	GetPaintSize(wide, tall);

	float fwide = (float)wide;
	float frange = (float)(_range[1] - _range[0]);
	float fvalue = (float)(_value - _range[0]);
	float fnob = (float)_nobPos[0];
	float freepixels = fwide - _nobSize;

	// map into the reduced range
	fvalue = fnob / freepixels;

	// scale by the true range
	fvalue *= frange;

	// take care of rounding issues
	_value = (int)(fvalue + _range[0] + 0.5);
}

//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the slider moves
//-----------------------------------------------------------------------------
void Slider::SendSliderMovedMessage()
{
	// send a changed message
	PostActionSignal(new KeyValues("SliderMoved", "position", _value));
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Slider::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(GetSchemeColor("Slider/SliderFgColor", pScheme));

	_sliderBorder = pScheme->GetBorder("ButtonBorder");
	_insetBorder = pScheme->GetBorder("ButtonDepressedBorder");

	if (_leftCaption)
	{
		_leftCaption->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
	}

	if (_rightCaption)
	{
		_rightCaption->SetFont(pScheme->GetFont("DefaultVerySmall", IsProportional()));
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Slider::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);

	char buf[256];
	if (_leftCaption)
	{
		_leftCaption->GetText(buf, 255);
		outResourceData->SetString("leftText", buf);
	}

	if (_rightCaption)
	{
		_rightCaption->GetText(buf, 255);
		outResourceData->SetString("rightText", buf);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Slider::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	const char *left = inResourceData->GetString("leftText", NULL);
	const char *right = inResourceData->GetString("rightText", NULL);

	int thumbWidth = inResourceData->GetInt("thumbwidth", 0);
	if (thumbWidth != 0)
	{
		SetThumbWidth(thumbWidth);
	}

	SetTickCaptions(left, right);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *Slider::GetDescription()
{
	static char buf[1024];
	_snprintf(buf, sizeof(buf), "%s, string leftText, string rightText", BaseClass::GetDescription());
	return buf;
}

//-----------------------------------------------------------------------------
// Purpose: Get the rectangle to draw the slider track in.
//-----------------------------------------------------------------------------
void Slider::GetTrackRect(int &x, int &y, int &w, int &h)
{
	int wide, tall;
	GetPaintSize(wide, tall);

	x = 0;
	y = 8;
	w = wide - (int)_nobSize;
	h = 4;
}

//-----------------------------------------------------------------------------
// Purpose: Draw everything on screen
//-----------------------------------------------------------------------------
void Slider::Paint()
{
	DrawTicks();

	DrawTickLabels();

	// Draw nob last so it draws over ticks.
	DrawNob();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the ticks below the slider.
//-----------------------------------------------------------------------------
void Slider::DrawTicks()
{
	int x, y;
	int wide, tall;
	GetTrackRect(x, y, wide, tall);

	// Figure out how to draw the ticks
	GetPaintSize(wide, tall);

	float fwide = (float)wide;
	float freepixels = fwide - _nobSize;

	float leftpixel = _nobSize / 2.0f;

	float pixelspertick = freepixels / (m_nNumTicks);

	y += (int)_nobSize;
	int tickHeight = 5;

	IScheme *pScheme = scheme()->GetIScheme(GetScheme());
	if (IsEnabled())
	{
		surface()->DrawSetColor(GetSchemeColor("SliderTickColor", pScheme));
		for (int i = 0; i <= m_nNumTicks; i++)
		{
			int xpos = (int)(leftpixel + i * pixelspertick);

			surface()->DrawFilledRect(xpos, y, xpos + 1, y + tickHeight);
		}
	}
	else
	{
		surface()->DrawSetColor(GetSchemeColor("DisabledFgColor1", pScheme));
		for (int i = 0; i <= m_nNumTicks; i++)
		{
			int xpos = (int)(leftpixel + i * pixelspertick);
			surface()->DrawFilledRect(xpos + 1, y + 1, xpos + 2, y + tickHeight + 1);
		}
		surface()->DrawSetColor(GetSchemeColor("DisabledFgColor2", pScheme));
		for (i = 0; i <= m_nNumTicks; i++)
		{
			int xpos = (int)(leftpixel + i * pixelspertick);
			surface()->DrawFilledRect(xpos, y, xpos + 1, y + tickHeight);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw Tick labels under the ticks.
//-----------------------------------------------------------------------------
void Slider::DrawTickLabels()
{
	int x, y;
	int wide, tall;
	GetTrackRect(x, y, wide, tall);

	// Figure out how to draw the ticks
	GetPaintSize(wide, tall);
	y += (int)NOB_SIZE_guess + 4;

	// Draw Start and end range values
	IScheme *pScheme = scheme()->GetIScheme(GetScheme());
	if (IsEnabled())
		surface()->DrawSetTextColor(GetSchemeColor("SliderTickColor", pScheme));
	else
		surface()->DrawSetTextColor(GetSchemeColor("DisabledFgColor1", pScheme));

	if (_leftCaption != NULL)
	{
		_leftCaption->SetPos(0, y);
		if (IsEnabled())
		{
			_leftCaption->SetColor(GetSchemeColor("SliderTickColor", pScheme));
		}
		else
		{
			_leftCaption->SetColor(GetSchemeColor("DisabledFgColor1", pScheme));
		}

		_leftCaption->Paint();
	}

	if (_rightCaption != NULL)
	{
		int rwide, rtall;
		_rightCaption->GetSize(rwide, rtall);
		_rightCaption->SetPos((int)(wide - rwide), y);
		if (IsEnabled())
		{
			_rightCaption->SetColor(GetSchemeColor("SliderTickColor", pScheme));
		}
		else
		{
			_rightCaption->SetColor(GetSchemeColor("DisabledFgColor1", pScheme));
		}
		_rightCaption->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the nob part of the slider.
//-----------------------------------------------------------------------------
void Slider::DrawNob()
{
	// horizontal nob
	int x, y;
	int wide, tall;
	GetTrackRect(x, y, wide, tall);
	Color col = GetFgColor();
	surface()->DrawSetColor(col);

	int nobheight = 16;

	surface()->DrawFilledRect(
		_nobPos[0],
		y + tall / 2 - nobheight / 2,
		_nobPos[1],
		y + tall / 2 + nobheight / 2);

	// border
	if (_sliderBorder)
	{
		_sliderBorder->Paint(
			_nobPos[0],
			y + tall / 2 - nobheight / 2,
			_nobPos[1],
			y + tall / 2 + nobheight / 2);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the text labels of the Start and end ticks.
//-----------------------------------------------------------------------------
void Slider::SetTickCaptions(const char *left, const char *right)
{
	if (left)
	{
		if (_leftCaption)
		{
			_leftCaption->SetText(left);
		}
		else
		{
			_leftCaption = new TextImage(left);
		}
	}
	if (right)
	{
		if (_rightCaption)
		{
			_rightCaption->SetText(right);
		}
		else
		{
			_rightCaption = new TextImage(right);
		}
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Set the text labels of the Start and end ticks.
//-----------------------------------------------------------------------------
void Slider::SetTickCaptions(const wchar_t *left, const wchar_t *right)
{
	if (left)
	{
		if (_leftCaption)
		{
			_leftCaption->SetText(left);
		}
		else
		{
			_leftCaption = new TextImage(left);
		}
	}
	if (right)
	{
		if (_rightCaption)
		{
			_rightCaption->SetText(right);
		}
		else
		{
			_rightCaption = new TextImage(right);
		}
	}
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the slider track
//-----------------------------------------------------------------------------
void Slider::PaintBackground()
{
	BaseClass::PaintBackground();

	int x, y;
	int wide, tall;

	GetTrackRect(x, y, wide, tall);

	IScheme *pScheme = scheme()->GetIScheme(GetScheme());
	surface()->DrawSetColor(GetSchemeColor("SliderTrackColor", pScheme));
	surface()->DrawFilledRect(x, y, x + wide, y + tall);
	if (_insetBorder)
	{
		_insetBorder->Paint(x, y, x + wide, y + tall);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the slider.
//-----------------------------------------------------------------------------
void Slider::SetRange(int min, int max)
{
	if (max < min)
	{
		max = min;
	}

	if (min > max)
	{
		min = max;
	}

	_range[0] = min;
	_range[1] = max;

	// update the value (forces it within the range)
	if (_value < _range[0])
	{
		_value = _range[0];
	}
	else if (_value > _range[1])
	{
		_value = _range[1];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the max and min values of the slider
//-----------------------------------------------------------------------------
void Slider::GetRange(int &min, int &max)
{
	min = _range[0];
	max = _range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Respond when the cursor is moved in our window if we are clicking
//			and dragging.
//-----------------------------------------------------------------------------
void Slider::OnCursorMoved(int x, int y)
{
	if (!_dragging)
	{
		return;
	}

	input()->GetCursorPosition(x, y);
	ScreenToLocal(x, y);

	int wide, tall;
	GetPaintSize(wide, tall);

	_nobPos[0] = _nobDragStartPos[0] + (x - _dragStartPos[0]);
	_nobPos[1] = _nobDragStartPos[1] + (x - _dragStartPos[0]);

	if (_nobPos[1] > wide)
	{
		_nobPos[0] = wide - (_nobPos[1] - _nobPos[0]);
		_nobPos[1] = wide;
	}

	if (_nobPos[0] < 0)
	{
		_nobPos[1] = _nobPos[1] - _nobPos[0];
		_nobPos[0] = 0;
	}

	RecomputeValueFromNobPos();
	Repaint();
	SendSliderMovedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Respond to mouse presses. Trigger Record staring positon.
//-----------------------------------------------------------------------------
void Slider::OnMousePressed(MouseCode code)
{
	int x, y;

	if (!IsEnabled())
		return;

	input()->GetCursorPosition(x, y);

	ScreenToLocal(x, y);
	RequestFocus();

	if ((x >= _nobPos[0]) && (x < _nobPos[1]))
	{
		_dragging = true;
		input()->SetMouseCapture(GetVPanel());
		_nobDragStartPos[0] = _nobPos[0];
		_nobDragStartPos[1] = _nobPos[1];
		_dragStartPos[0] = x;
		_dragStartPos[1] = y;
	}
	else
	{
		// clicked on the track, jump the nob to the click position
		int min_guess, max_guess;
		GetRange(min_guess, max_guess);
		int wide_guess = GetWide();
		SetValue((int)((float)(max_guess - min_guess) / wide_guess * x) + min_guess);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Just handle double presses like mouse presses
//-----------------------------------------------------------------------------
void Slider::OnMouseDoublePressed(MouseCode code)
{
	OnMousePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses
//-----------------------------------------------------------------------------
void Slider::OnKeyCodeTyped(KeyCode code)
{
	switch (code)
	{
	// for now left and right arrows just open or close submenus if they are there.
	case KEY_LEFT:
	case KEY_DOWN:
		{
			int val = GetValue();
			SetValue(val - 1);
			break;
		}
	case KEY_RIGHT:
	case KEY_UP:
		{
			int val = GetValue();
			SetValue(val + 1);
			break;
		}
	case KEY_PAGEDOWN:
		{
			int min, max;
			GetRange(min, max);
			float range = (float)max - min;
			float pertick = range / m_nNumTicks;
			int val = GetValue();
			SetValue(val - (int)pertick);
			break;
		}
	case KEY_PAGEUP:
		{
			int min, max;
			GetRange(min, max);
			float range = (float)max - min;
			float pertick = range / m_nNumTicks;
			int val = GetValue();
			SetValue(val + (int)pertick);
			break;
		}
	case KEY_HOME:
		{
			int min, max;
			GetRange(min, max);
			SetValue(min);
			break;
		}
	case KEY_END:
		{
			int min, max;
			GetRange(min, max);
			SetValue(max);
			break;
		}
	default:
		BaseClass::OnKeyCodeTyped(code);
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop dragging when the mouse is released.
//-----------------------------------------------------------------------------
void Slider::OnMouseReleased(MouseCode code)
{
	_dragging = false;
	input()->SetMouseCapture(NULL);
}

//-----------------------------------------------------------------------------
// Purpose: Get the nob's position (the ends of each side of the nob)
//-----------------------------------------------------------------------------
void Slider::GetNobPos(int &min, int &max)
{
	min = _nobPos[0];
	max = _nobPos[1];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Slider::SetButtonOffset(int buttonOffset)
{
	_buttonOffset = buttonOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Slider::SetThumbWidth(int width)
{
	_nobSize = (float)width;
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of ticks that appear under the slider.
//-----------------------------------------------------------------------------
void Slider::SetNumTicks(int ticks)
{
	m_nNumTicks = ticks;
}
