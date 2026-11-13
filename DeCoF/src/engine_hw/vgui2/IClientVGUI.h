//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the VGUI2 interface of the client DLL (the slots of the
//			TeamFortressViewport IClientVGUI vtable in the SDK's game_controls.lib)
//
// $NoKeywords: $
//=============================================================================

#ifndef ICLIENTVGUI_H
#define ICLIENTVGUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <vgui/VGUI.h>

class IClientVGUI : public IBaseInterface
{
public:
	virtual void Initialize( CreateInterfaceFn *factories, int count ) = 0;
	virtual void Start() = 0;
	virtual void SetParent( vgui2::VPANEL parent ) = 0;
	virtual bool UseVGUI1() = 0;
	virtual void HideScoreBoard() = 0;
	virtual void HideAllVGUIMenu() = 0;
	virtual void ActivateClientUI() = 0;
	virtual void HideClientUI() = 0;
	virtual void Shutdown() = 0;
};

#define VCLIENTVGUI_INTERFACE_VERSION "VClientVGUI001"

#endif // ICLIENTVGUI_H
