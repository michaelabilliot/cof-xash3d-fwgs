//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Creates a HTML control
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vgui/Cursor.h>
#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui/MouseCode.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/ScrollBar.h>

#include "FileSystem.h"

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: returns true if steam is running in offline mode
//-----------------------------------------------------------------------------
inline bool IsSteamInOfflineMode_guess()
{
	int offline = 0;
	vgui2::system()->GetRegistryInteger("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Offline", offline);
	return (offline == 1);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
HTML::HTML(Panel *parent, const char *name, bool allowJavaScript) : Panel(parent, name)
{
	browser_guess = surface()->CreateHTMLWindow(this, GetVPanel());

	m_iNextFrameTime_guess = 0;
	m_iAnimTime_guess = 0;
	loading_guess = NULL;
	picture_guess = NULL;
	m_iScrollBorderX = m_iScrollBorderY = 0;
	m_iScrollX_guess = m_iScrollY_guess = 0;
	m_bScrollBarEnabled = true;
	m_bContextMenuEnabled = true;
	m_bNewWindowsOnly = false;
	m_bSetVisibleOnPerformLayout_guess = false;

	if (surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		SetCursor(dc_blank);
	}

	_hbar = new ScrollBar(this, "HorizScrollBar", false);
	_hbar->SetVisible(false);
	_hbar->AddActionSignalTarget(this);

	_vbar = new ScrollBar(this, "VertScrollBar", true);
	_vbar->SetVisible(false);
	_vbar->AddActionSignalTarget(this);

	m_bRegenerateHTMLBitmap_guess = true;

	if (IsProportional())
	{
		int width, height;
		int sw, sh;
		surface()->GetProportionalBase(width, height);
		surface()->GetScreenSize(sw, sh);

		m_iScrollbarSize = (int)((float)18 * ((float)sw / (float)width));
	}
	else
	{
		m_iScrollbarSize = 18;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
HTML::~HTML()
{
	surface()->DeleteHTMLWindow(browser_guess);
}

//-----------------------------------------------------------------------------
// Purpose: Handle message to change our cursor
//-----------------------------------------------------------------------------
void HTML::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetBorder(pScheme->GetBorder("BrowserBorder"));
	BrowserResize();
}

//-----------------------------------------------------------------------------
// Purpose: Causes the HTML window to repaint itself every time milliseconds,
//			to allow animated gifs and the like
//-----------------------------------------------------------------------------
void HTML::StartAnimate_guess(int time)
{
	m_iAnimTime_guess = time;
}

//-----------------------------------------------------------------------------
// Purpose: stops the repainting
//-----------------------------------------------------------------------------
void HTML::StopAnimate_guess()
{
	m_iNextFrameTime_guess = 0xffffffff; // next update is at infinity
	m_iAnimTime_guess = 0;
}

//-----------------------------------------------------------------------------
// Purpose: overrides panel class, paints a texture of the HTML window as a background
//-----------------------------------------------------------------------------
void HTML::PaintBackground()
{
	BaseClass::PaintBackground();

	if (m_bRegenerateHTMLBitmap_guess)
	{
		if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
		{
			surface()->PaintHTMLWindow(browser_guess);
		}
		m_bRegenerateHTMLBitmap_guess = false;

		int w, h;
		GetSize(w, h);
		CalcScrollBars(w, h);
	}

	// the window is a textured background
	if (surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
		return;

	if (browser_guess)
	{
		picture_guess = browser_guess->GetBitmap();
	}

	if (picture_guess)
	{
		surface()->DrawSetColor(GetBgColor());
		picture_guess->SetPos(0, 0);
		picture_guess->Paint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: causes a repaint when the layout changes
//-----------------------------------------------------------------------------
void HTML::PerformLayout()
{
	BaseClass::PerformLayout();
	Repaint();
	if (m_bSetVisibleOnPerformLayout_guess)
	{
		if (browser_guess)
		{
			browser_guess->SetVisible(true);
		}
		m_bSetVisibleOnPerformLayout_guess = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: overrides the panel's SetVisible() method so we can hide/show the HTML window
//-----------------------------------------------------------------------------
void HTML::SetVisible(bool state)
{
	BaseClass::SetVisible(state);

	int w, h;
	GetSize(w, h);
	CalcScrollBars(w, h);
	BrowserResize();

	if (!state)
	{
		m_bSetVisibleOnPerformLayout_guess = false;
	}

	if (IsLayoutInvalid() && state)
	{
		m_bSetVisibleOnPerformLayout_guess = true;
	}
	else if (browser_guess)
	{
		browser_guess->SetVisible(state);
	}
}

//-----------------------------------------------------------------------------
// Purpose: resizes the browser window when the panel moves
//-----------------------------------------------------------------------------
void HTML::OnMove()
{
	BaseClass::OnMove();
	BrowserResize();
}

//-----------------------------------------------------------------------------
// Purpose: calculates the need for and position of both horizontal and vertical scroll bars
//-----------------------------------------------------------------------------
void HTML::CalcScrollBars(int w, int h)
{
	int img_w_guess = 0, img_h_guess = 0;

	if (m_bScrollBarEnabled)
	{
		if (browser_guess)
		{
			browser_guess->GetHTMLSize(img_w_guess, img_h_guess);
		}

		if (img_w_guess < 0 || img_h_guess < 0)
		{
			m_iScrollBorderX = 0;
			m_iScrollBorderY = 0;
			_vbar->SetVisible(false);
			_hbar->SetVisible(false);
			BrowserResize();
			return;
		}

		if (img_h_guess > h)
		{
			if (!_vbar->IsVisible())
			{
				_vbar->SetVisible(true);
				// displayable area has changed, need to force an update
				PostMessage(this, new KeyValues("ScrollBarSliderMoved"), 0.02f);
			}

			_vbar->SetEnabled(false);
			_vbar->SetRangeWindow(h / 2);
			_vbar->SetRange(0, img_h_guess - h / 2);
			_vbar->SetButtonPressedScrollValue(5);
			_vbar->SetPos(w - (m_iScrollbarSize + 1), 1);
			if (img_w_guess > w)
			{
				_vbar->SetSize(m_iScrollbarSize, h - m_iScrollbarSize - 2);
			}
			else
			{
				_vbar->SetSize(m_iScrollbarSize, h - 2);
			}
			m_iScrollBorderX = m_iScrollbarSize + 1;
		}
		else
		{
			m_iScrollBorderX = 0;
			_vbar->SetVisible(false);
			BrowserResize();
		}

		if (img_w_guess > w)
		{
			_hbar->SetVisible(true);
			_hbar->SetEnabled(false);
			_hbar->SetRangeWindow(w / 2);
			_hbar->SetRange(0, img_w_guess - w / 2);
			_hbar->SetButtonPressedScrollValue(5);
			_hbar->SetPos(1, h - (m_iScrollbarSize + 1));
			if (img_h_guess > h)
			{
				_hbar->SetSize(w - m_iScrollbarSize - 1, m_iScrollbarSize);
			}
			else
			{
				_hbar->SetSize(w - 1, m_iScrollbarSize);
			}
			m_iScrollBorderY = m_iScrollbarSize + 2;
		}
		else
		{
			m_iScrollBorderY = 0;
			_hbar->SetVisible(false);
			BrowserResize();
		}
	}
	else
	{
		m_iScrollBorderX = 0;
		m_iScrollBorderY = 0;
		_vbar->SetVisible(false);
		_hbar->SetVisible(false);
		BrowserResize();
	}
}

//-----------------------------------------------------------------------------
// Purpose: opens the URL, will accept any URL that IE accepts
//-----------------------------------------------------------------------------
void HTML::OpenURL(const char *URL, bool force)
{
	if (IsSteamInOfflineMode_guess() && !force)
	{
		const char *baseDir_guess = getenv("HTML_OFFLINE_DIR");
		if (baseDir_guess)
		{
			// get the app we need to run
			char htmlLocation_guess[_MAX_PATH];
			char otherName_guess[128];
			char fileLocation_guess[_MAX_PATH];

			if (!vgui2::filesystem()->FileExists(baseDir_guess))
			{
				_snprintf(otherName_guess, sizeof(otherName_guess), "%senglish.html", "Steam\\cached\\offline_");
				baseDir_guess = otherName_guess;
			}
			vgui2::filesystem()->GetLocalCopy(baseDir_guess); // put this file on disk for IE to load

			vgui2::filesystem()->GetLocalPath(baseDir_guess, fileLocation_guess, sizeof(fileLocation_guess));
			_snprintf(htmlLocation_guess, sizeof(htmlLocation_guess), "file://%s", fileLocation_guess);

			if (browser_guess)
			{
				browser_guess->OpenURL(htmlLocation_guess);
			}
		}
		else
		{
			if (browser_guess)
			{
				browser_guess->OpenURL(URL);
			}
		}
	}
	else
	{
		if (browser_guess)
		{
			browser_guess->OpenURL(URL);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: aborts the loading of the current page
//-----------------------------------------------------------------------------
bool HTML::StopLoading()
{
	return (browser_guess && browser_guess->StopLoading());
}

//-----------------------------------------------------------------------------
// Purpose: refreshes the current page
//-----------------------------------------------------------------------------
bool HTML::Refresh()
{
	return (browser_guess && browser_guess->Refresh());
}

//-----------------------------------------------------------------------------
// Purpose: empties the current HTML container of any HTML text (used in conjunction with AddText)
//-----------------------------------------------------------------------------
void HTML::Clear_guess()
{
	if (browser_guess)
	{
		browser_guess->Clear();
	}
}

//-----------------------------------------------------------------------------
// Purpose: appends the text to the end of the current HTML container
//-----------------------------------------------------------------------------
void HTML::AddText_guess(const char *text)
{
	if (browser_guess)
	{
		browser_guess->AddText(text);
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles the panel being resized
//-----------------------------------------------------------------------------
void HTML::OnSizeChanged(int wide, int tall)
{
	BaseClass::OnSizeChanged(wide, tall);
	CalcScrollBars(wide, tall);
	BrowserResize();

	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: lets the page render itself when it is animated
//-----------------------------------------------------------------------------
void HTML::OnTick()
{
	if (IsVisible() && m_iAnimTime_guess && system()->GetTimeMillis() >= m_iNextFrameTime_guess)
	{
		m_iNextFrameTime_guess = system()->GetTimeMillis() + m_iAnimTime_guess;
		m_bRegenerateHTMLBitmap_guess = true;
		if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
		{
			Repaint();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes mouse clicks to the control
//-----------------------------------------------------------------------------
void HTML::OnMousePressed(MouseCode code)
{
	// ask for the focus to come to this window
	RequestFocus();

	// now tell the browser about the click
	// ignore right clicks if context menu has been disabled
	if ((code != MOUSE_RIGHT || m_bContextMenuEnabled) && browser_guess)
	{
		browser_guess->OnMouse(code, IHTML::DOWN, m_iMouseX, m_iMouseY);
	}

	// force the browser to update its window
	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes mouse up events
//-----------------------------------------------------------------------------
void HTML::OnMouseReleased(MouseCode code)
{
	if (browser_guess)
	{
		browser_guess->OnMouse(code, IHTML::UP, m_iMouseX, m_iMouseY);
	}

	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: keeps track of where the cursor is
//-----------------------------------------------------------------------------
void HTML::OnCursorMoved(int x, int y)
{
	MouseCode code_guess = MOUSE_LEFT;
	m_iMouseX = x;
	m_iMouseY = y;
	if (browser_guess)
	{
		browser_guess->OnMouse(code_guess, IHTML::MOVE, x, y);
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes double click events to the browser
//-----------------------------------------------------------------------------
void HTML::OnMouseDoublePressed(MouseCode code)
{
	if (browser_guess)
	{
		browser_guess->OnMouse(code, IHTML::DOWN, m_iMouseX, m_iMouseY);
	}

	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: passes key presses to the browser (we don't current do this)
//-----------------------------------------------------------------------------
void HTML::OnKeyTyped(wchar_t unichar)
{
}

//-----------------------------------------------------------------------------
// Purpose: passes key presses to the browser
//-----------------------------------------------------------------------------
void HTML::OnKeyCodePressed(KeyCode code)
{
	RequestFocus();
	if (code == KEY_PAGEDOWN || code == KEY_SPACE)
	{
		int val = _vbar->GetValue();
		val += 200;
		_vbar->SetValue(val);
	}
	else if (code == KEY_PAGEUP)
	{
		int val = _vbar->GetValue();
		val -= 200;
		_vbar->SetValue(val);
	}

	if (browser_guess)
	{
		browser_guess->OnKeyDown(code);
	}

	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: scrolls the vertical scroll bar on a web page
//-----------------------------------------------------------------------------
void HTML::OnMouseWheeled(int delta)
{
	if (_vbar)
	{
		int val = _vbar->GetValue();
		val -= (delta * 25);
		_vbar->SetValue(val);
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a custom URL handler
//-----------------------------------------------------------------------------
void HTML::AddCustomURLHandler(const char *customProtocolName, vgui2::Panel *target)
{
	int index = m_CustomURLHandlers.AddToTail();
	m_CustomURLHandlers[index].hPanel = target;
	strncpy(m_CustomURLHandlers[index].url, customProtocolName, sizeof(m_CustomURLHandlers[index].url));
}

//-----------------------------------------------------------------------------
// Purpose: called when the browser is about to load a page
//-----------------------------------------------------------------------------
bool HTML::OnStartURL(const char *url, const char *target, bool first)
{
	if (IsVisible() && !surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		SetCursor(dc_arrow);
	}

	bool bURLHandled = false;
	for (int i = 0; i < m_CustomURLHandlers.Count(); i++)
	{
		if (!strnicmp(m_CustomURLHandlers[i].url, url, strlen(m_CustomURLHandlers[i].url)))
		{
			// we have a custom handler
			Panel *targetPanel_guess = m_CustomURLHandlers[i].hPanel;
			if (targetPanel_guess)
			{
				PostMessage(targetPanel_guess, new KeyValues("CustomURL", "url", url + strlen(m_CustomURLHandlers[i].url) + 3, "protocol", m_CustomURLHandlers[i].url));
			}

			bURLHandled = true;
		}
	}

	if (bURLHandled)
		return false;

	if (m_bNewWindowsOnly)
	{
		if (target && (!stricmp(target, "_blank") || !stricmp(target, "_new"))) // only allow NEW windows (_blank ones)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: shapes the browser window to the panel
//-----------------------------------------------------------------------------
void HTML::BrowserResize()
{
	int w, h;
	GetSize(w, h);
	IBorder *border = scheme()->GetIScheme(GetScheme())->GetBorder("BrowserBorder");
	int left = 0, top = 0, right = 0, bottom = 0;
	if (border)
	{
		border->GetInset(left, top, right, bottom);
	}

	// the inset is one pixel bigger than the border
	left++;
	top++;
	right++;
	bottom++;

	if (browser_guess)
	{
		browser_guess->OnSize(m_iScrollX_guess + left, m_iScrollY_guess + top, w - m_iScrollBorderX - right, h - m_iScrollBorderY - bottom);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the page has finished loading
//-----------------------------------------------------------------------------
void HTML::OnFinishURL(const char *url)
{
	// reset the scroll bar positions
	_vbar->SetValue(0);
	_hbar->SetValue(0);
	m_iScrollX_guess = m_iScrollY_guess = 0;

	int w, h;
	GetSize(w, h);
	CalcScrollBars(w, h);
	BrowserResize();

	m_bRegenerateHTMLBitmap_guess = true; // repaint the window, as we have a new picture to show
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called as the page loads
//-----------------------------------------------------------------------------
void HTML::OnProgressURL(long current, long maximum)
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the status text changes
//-----------------------------------------------------------------------------
void HTML::OnSetStatusText(const char *text)
{
}

//-----------------------------------------------------------------------------
// Purpose: called when the browser wants to redraw
//-----------------------------------------------------------------------------
void HTML::OnUpdate()
{
	// only let it redraw every m_iAnimTime milliseconds, so stop it sucking up all the CPU time
	if (m_iAnimTime_guess && system()->GetTimeMillis() >= m_iNextFrameTime_guess)
	{
		m_iNextFrameTime_guess = system()->GetTimeMillis() + m_iAnimTime_guess;
		m_bRegenerateHTMLBitmap_guess = true;
		if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
		{
			Repaint();
		}
	}

	int w, h;
	GetSize(w, h);
	CalcScrollBars(w, h);
	BrowserResize();
}

//-----------------------------------------------------------------------------
// Purpose: called when the cursor moves over a link
//-----------------------------------------------------------------------------
void HTML::OnLink()
{
	if (IsVisible() && !surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		SetCursor(dc_hand);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the cursor leaves a link
//-----------------------------------------------------------------------------
void HTML::OffLink()
{
	if (IsVisible() && !surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		SetCursor(dc_arrow);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the scroll bars move
//-----------------------------------------------------------------------------
void HTML::OnSliderMoved()
{
	if (_hbar->IsVisible())
	{
		m_iScrollX_guess = _hbar->GetValue();
	}
	else
	{
		m_iScrollX_guess = 0;
	}

	if (_vbar->IsVisible())
	{
		m_iScrollY_guess = _vbar->GetValue();
	}
	else
	{
		m_iScrollY_guess = 0;
	}

	BrowserResize();

	m_bRegenerateHTMLBitmap_guess = true;
	if (!surface()->SupportsFeature(ISurface::DIRECT_HWND_RENDER))
	{
		Repaint();
	}

	PostActionSignal(new KeyValues("HTMLSliderMoved"));
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the vertical scroll bar is at the bottom of the page
//-----------------------------------------------------------------------------
bool HTML::IsScrolledToBottom()
{
	if (!_vbar->IsVisible())
		return true;

	int img_w_guess = 0, img_h_guess = 0;
	if (browser_guess)
	{
		browser_guess->GetHTMLSize(img_w_guess, img_h_guess);
	}

	return (m_iScrollY_guess + GetTall() >= img_h_guess);
}

//-----------------------------------------------------------------------------
// Purpose: enables or disables the scroll bars
//-----------------------------------------------------------------------------
void HTML::SetScrollbarsEnabled(bool state)
{
	m_bScrollBarEnabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: enables or disables the right click context menu
//-----------------------------------------------------------------------------
void HTML::SetContextMenuEnabled(bool state)
{
	m_bContextMenuEnabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: only allow new browser windows to be opened
//-----------------------------------------------------------------------------
void HTML::NewWindowsOnly(bool state)
{
	m_bNewWindowsOnly = state;
}
