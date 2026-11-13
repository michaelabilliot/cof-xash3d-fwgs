//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_IINPUT_H
#define VGUI_IINPUT_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include "interface.h"

namespace vgui2
{

class Cursor;
typedef unsigned long HCursor;

class IInput : public IBaseInterface
{
public:
	virtual void SetMouseFocus(VPANEL) = 0;
	virtual void SetMouseCapture(VPANEL) = 0;
	virtual void GetKeyCodeText(KeyCode, char *, int) = 0;
	virtual VPANEL GetFocus() = 0;
	virtual VPANEL GetMouseOver() = 0;
	virtual void SetCursorPos(int, int) = 0;
	virtual void GetCursorPos(int &, int &) = 0;
	virtual bool WasMousePressed(MouseCode) = 0;
	virtual bool WasMouseDoublePressed(MouseCode) = 0;
	virtual bool IsMouseDown(MouseCode) = 0;
	virtual void SetCursorOveride(HCursor) = 0;
	virtual HCursor GetCursorOveride() = 0;
	virtual bool WasMouseReleased(MouseCode) = 0;
	virtual bool WasKeyPressed(KeyCode) = 0;
	virtual bool IsKeyDown(KeyCode) = 0;
	virtual bool WasKeyTyped(KeyCode) = 0;
	virtual bool WasKeyReleased(KeyCode) = 0;
	virtual VPANEL GetAppModalSurface() = 0;
	virtual void SetAppModalSurface(VPANEL) = 0;
	virtual void ReleaseAppModalSurface() = 0;
	virtual void GetCursorPosition(int &, int &) = 0;
};

} // namespace vgui2

#define VGUI_INPUT_INTERFACE_VERSION "VGUI_Input004"

#endif // VGUI_IINPUT_H
