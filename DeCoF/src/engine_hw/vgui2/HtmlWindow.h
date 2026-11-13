//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the Internet Explorer control behind vgui2::IHTML (HtmlWindow.cpp)
//
// $NoKeywords: $
//=============================================================================

#ifndef HTMLWINDOW_H
#define HTMLWINDOW_H

#ifdef _WIN32
#pragma once
#endif

#include <windows.h>
#include <vgui/VGUI.h>
#include <vgui/IHTML.h>
#include "MemoryBitmap.h"

using vgui2::IHTML;
using vgui2::IHTMLEvents;
using vgui2::VPANEL;

struct IConnectionPoint;
struct IWebBrowser2;
struct IOleObject;
struct IOleInPlaceObject;
struct IViewObject;
struct IHTMLElement;
struct IDispatch;
class FrameSite;

class HtmlWindow : public IHTML, public IHTMLEvents
{
public:
	HtmlWindow(IHTMLEvents *events, VPANEL c, HWND parent, bool AllowJavaScript, bool DirectToHWND);
	virtual ~HtmlWindow();

	// IHTML
	virtual void OpenURL(const char *);
	virtual bool StopLoading();
	virtual bool Refresh();
	virtual bool Show(bool shown);

	HWND GetHWND() { return m_parent_guess; }
	HWND GetIEHWND() { return m_ieHWND_guess; }
	WNDPROC GetIEWndProc_guess() { return m_hIEWndProc_guess; }
	HHOOK GetHook_guess() { return m_hHook_guess; }
	UINT GetMouseMessage_guess() { return m_mouseMessage_guess; }

	virtual vgui2::IImage *GetBitmap() { return m_Bitmap_guess; }
	virtual void GetHTMLSize(int &wide, int &tall) { wide = m_iHtmlWide_guess; tall = m_iHtmlTall_guess; }
	virtual char *GetOpenedPage() { return m_currentUrl_guess; }

	virtual void OnSize(int x, int y, int w, int h);
	virtual void Clear();
	virtual void AddText(const char *text);
	virtual void OnMouse(vgui2::MouseCode code, MOUSE_STATE s, int x, int y);
	virtual void OnChar(wchar_t unichar);
	virtual void OnKeyDown(vgui2::KeyCode code);
	virtual void SetVisible(bool state);

	virtual void OnPaint(HDC hDC);
	virtual void ScrollHTML_guess(int x, int y);
	virtual bool OnMouseOver_guess();

	// IHTMLEvents
	virtual bool OnStartURL(const char *url, const char *target, bool first);
	virtual void OnFinishURL(const char *url);
	virtual void OnProgressURL(long current, long maximum);
	virtual void OnSetStatusText(const char *text);
	virtual void OnUpdate();
	virtual void OnLink();
	virtual void OffLink();

	IHTMLEvents *GetEvents_guess() { return m_events_guess; }
	void GetSize_guess(int &wide, int &tall) { wide = m_iWide_guess; tall = m_iTall_guess; }
	void GetApplication_guess(IDispatch **ppDisp);
	RECT SetBounds_guess();

private:
	void CreateBrowser_guess(bool AllowJavaScript);
	bool CheckIsLink_guess(IHTMLElement *el, char *type);
	void CalculateHTMLSize_guess(IHTMLElement *pElement);

	int m_iUnused_guess;
	char m_currentUrl_guess[512];
	bool m_specificallyOpened_guess;
	int m_iWide_guess, m_iTall_guess;
	int m_iX_guess, m_iY_guess;
	long m_iHtmlWide_guess, m_iHtmlTall_guess;
	int m_iScrollX_guess, m_iScrollY_guess;

	IConnectionPoint *m_connectionPoint_guess;
	IWebBrowser2 *m_webBrowser_guess;
	IOleObject *m_oleObject_guess;
	IOleInPlaceObject *m_oleInPlaceObject_guess;
	IViewObject *m_viewObject_guess;
	HWND m_parent_guess;
	HWND m_oleObjectHWND_guess;
	HWND m_ieHWND_guess;
	VPANEL m_context_guess;
	IHTMLEvents *m_events_guess;

	HDC m_hdcMem_guess;
	HDC m_lastHDC_guess;
	HBITMAP m_hBitmap_guess;
	vgui2::MemoryBitmap *m_Bitmap_guess;

	bool m_bUnused_guess;
	bool m_bLoaded_guess;
	bool m_bDirectToHWND_guess;
	FrameSite *m_frameSite_guess;
	DWORD m_adviseCookie_guess;
	DWORD m_HtmlEventsAdviseCookie_guess;
	HHOOK m_hHook_guess;
	bool m_bUnused2_guess;
	LPARAM m_lastMousePos_guess;
	UINT m_mouseMessage_guess;
	bool m_bVisible_guess;
	WNDPROC m_hIEWndProc_guess;
};

#endif // HTMLWINDOW_H
