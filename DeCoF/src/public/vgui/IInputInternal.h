//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the input calls that only the surface makes (window messages in)
//
// $NoKeywords: $
//=============================================================================

#ifndef IINPUTINTERNAL_H
#define IINPUTINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/IInput.h>

namespace vgui2
{

class IInputInternal : public IInput
{
public:
	// processes input for a frame
	virtual void RunFrame() = 0;

	virtual void UpdateMouseFocus(int x, int y) = 0;

	// called when a panel becomes invalid
	virtual void PanelDeleted(VPANEL panel) = 0;

	// inputs into vgui input handling
	virtual void InternalCursorMoved(int x, int y) = 0; // expects input in surface space
	virtual void InternalMousePressed(MouseCode code) = 0;
	virtual void InternalMouseDoublePressed(MouseCode code) = 0;
	virtual void InternalMouseReleased(MouseCode code) = 0;
	virtual void InternalMouseWheeled(int delta) = 0;
	virtual void InternalKeyCodePressed(KeyCode code) = 0;
	virtual void InternalKeyCodeTyped(KeyCode code) = 0;
	virtual void InternalKeyTyped(wchar_t unichar) = 0;
	virtual void InternalKeyCodeReleased(KeyCode code) = 0;
};

} // namespace vgui2

#define VGUI_INPUTINTERNAL_INTERFACE_VERSION "VGUI_InputInternal001"

#endif // IINPUTINTERNAL_H
