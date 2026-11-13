//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the surface's embedded web browser window and its event sink
//
// $NoKeywords: $
//=============================================================================

#ifndef IHTML_H
#define IHTML_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/MouseCode.h>
#include <vgui/KeyCode.h>

namespace vgui2
{

class IImage;

//-----------------------------------------------------------------------------
// Purpose: basic interface for a HTML window
//-----------------------------------------------------------------------------
class IHTML
{
public:
	// open a new page
	virtual void OpenURL(const char *) = 0;

	// stops the existing page from loading
	virtual bool StopLoading() = 0;

	// refreshes the current page
	virtual bool Refresh() = 0;

	// display the control
	virtual bool Show(bool shown) = 0;

	// return the currently opened page
	virtual char *GetOpenedPage() = 0;

	// called when the browser needs to be resized
	virtual void OnSize(int x, int y, int w, int h) = 0;

	// returns the width and height (in pixels) of the HTML page
	virtual void GetHTMLSize(int &wide, int &tall) = 0;

	// clear the text in an existing control
	virtual void Clear() = 0;

	// add text to the browser control (as a HTML formated string)
	virtual void AddText(const char *text) = 0;

	enum MOUSE_STATE { UP, DOWN, MOVE };

	// unused functions we keep around so the vtable layout is binary compatible
	virtual void OnMouse(MouseCode code, MOUSE_STATE s, int x, int y) = 0;
	virtual void OnChar(wchar_t unichar) = 0;
	virtual void OnKeyDown(KeyCode code) = 0;

	virtual IImage *GetBitmap() = 0;
	virtual void SetVisible(bool state) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: possible events to send to the HTML control
//-----------------------------------------------------------------------------
class IHTMLEvents
{
public:
	// return true to continue loading the page, false to stop it
	virtual bool OnStartURL(const char *url, const char *target, bool first) = 0;
	virtual void OnFinishURL(const char *url) = 0;
	virtual void OnProgressURL(long current, long maximum) = 0;
	virtual void OnSetStatusText(const char *text) = 0;
	virtual void OnUpdate() = 0;
	virtual void OnLink() = 0;
	virtual void OffLink() = 0;
};

} // namespace vgui2

#endif // IHTML_H
