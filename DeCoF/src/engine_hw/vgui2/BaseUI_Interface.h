//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's VGUI2 user interface: loads vgui2 and GameUI, owns
//			the root panels and passes input and painting on to them
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEUI_INTERFACE_H
#define BASEUI_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "IBaseUI.h"

class CSysModule;

class CBaseUI : public IBaseUI
{
public:
	CBaseUI();
	virtual ~CBaseUI();

	virtual void Initialize( CreateInterfaceFn *factories, int count );
	virtual void Start( struct cl_enginefuncs_s *engineFuncs, int interfaceVersion );
	virtual void Shutdown();
	virtual int Key_Event( int down, int keynum, const char *pszCurrentBinding );
	virtual void CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
	virtual void Paint( int x, int y, int right, int bottom );
	virtual void HideGameUI();
	virtual void ActivateGameUI();
	virtual void HideConsole();
	virtual void ShowConsole();

private:
	CreateInterfaceFn m_FactoryList[5];
	int m_iNumFactories;

	CSysModule *m_hVGuiModule;
	CSysModule *m_hStaticGameUIModule;

	bool m_bHidingGameUI;
};

CBaseUI &BaseUI();

#endif // BASEUI_INTERFACE_H
