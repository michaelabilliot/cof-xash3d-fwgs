//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Creates a HTML control
//
// $NoKeywords: $
//=============================================================================

#ifndef HTML_H
#define HTML_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/IImage.h>
#include <vgui/IHTML.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>
#include <UtlVector.h>

// the message map casts member pointers of this multiply inherited class to Panel ones
#pragma pointers_to_members( full_generality, virtual_inheritance )

namespace vgui2
{

class ScrollBar;

//-----------------------------------------------------------------------------
// Purpose: Control to display HTML content
//-----------------------------------------------------------------------------
class HTML : public Panel, public IHTMLEvents
{
	DECLARE_CLASS_SIMPLE( HTML, Panel );

public:
	HTML(Panel *parent, const char *name, bool allowJavaScript = false);
	~HTML();

	// IHTML pass through functions
	virtual void OpenURL(const char *URL, bool force = false);
	virtual bool StopLoading();
	virtual bool Refresh();
	virtual void Clear_guess();
	virtual void AddText_guess(const char *text);

	// configuration
	virtual void SetScrollbarsEnabled(bool state);
	virtual void SetContextMenuEnabled(bool state);
	virtual void NewWindowsOnly(bool state);

	// adds a custom URL handler
	virtual void AddCustomURLHandler(const char *customProtocolName, vgui2::Panel *target);

	bool IsScrolledToBottom();

	// overridden to make sure the HTML window is hidden with the panel
	virtual void SetVisible(bool state);

	// repaints the window every time milliseconds
	void StartAnimate_guess(int time);
	void StopAnimate_guess();

	// IHTMLEvents
	virtual bool OnStartURL(const char *url, const char *target, bool first);
	virtual void OnFinishURL(const char *url);
	virtual void OnProgressURL(long current, long maximum);
	virtual void OnSetStatusText(const char *text);
	virtual void OnUpdate();
	virtual void OnLink();
	virtual void OffLink();

protected:
	virtual void OnTick();
	virtual void ApplySchemeSettings(IScheme *pScheme);
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void OnMove();
	virtual void OnSizeChanged(int wide, int tall);
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseReleased(MouseCode code);
	virtual void OnCursorMoved(int x, int y);
	virtual void OnMouseDoublePressed(MouseCode code);
	virtual void OnKeyTyped(wchar_t unichar);
	virtual void OnKeyCodePressed(KeyCode code);
	virtual void OnMouseWheeled(int delta);

	// resizes the browser window to fit the panel
	virtual void BrowserResize();

	// works out the scroll bar positions
	virtual void CalcScrollBars(int w, int h);

	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );

	IHTML *browser_guess;
	IImage *loading_guess;
	IImage *picture_guess;
	ScrollBar *_hbar, *_vbar;

	int m_iMouseX, m_iMouseY;
	long m_iNextFrameTime_guess;
	int m_iAnimTime_guess;
	int m_iScrollX_guess, m_iScrollY_guess;
	int m_iScrollBorderX, m_iScrollBorderY;
	bool m_bRegenerateHTMLBitmap_guess;
	bool m_bScrollBarEnabled;
	bool m_bContextMenuEnabled;
	int m_iScrollbarSize;
	bool m_bNewWindowsOnly;
	bool m_bSetVisibleOnPerformLayout_guess;

	struct CustomURLHandler_t
	{
		PHandle hPanel;
		char url[32];
	};
	CUtlVector<CustomURLHandler_t> m_CustomURLHandlers;
};

} // namespace vgui2

#endif // HTML_H
