//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The engine side of the VGUI2 user interface
//
// $NoKeywords: $
//=============================================================================

#ifndef IBASEUI_H
#define IBASEUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

struct cl_enginefuncs_s;

class IBaseUI : public IBaseInterface
{
public:
	virtual void Initialize( CreateInterfaceFn *factories, int count ) = 0;
	virtual void Start( struct cl_enginefuncs_s *engineFuncs, int interfaceVersion ) = 0;
	virtual void Shutdown() = 0;
	virtual int Key_Event( int down, int keynum, const char *pszCurrentBinding ) = 0;
	virtual void CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam ) = 0;
	virtual void Paint( int x, int y, int right, int bottom ) = 0;
	virtual void HideGameUI() = 0;
	virtual void ActivateGameUI() = 0;
	virtual void HideConsole() = 0;
	virtual void ShowConsole() = 0;
};

#define BASEUI_INTERFACE_VERSION "BaseUI001"

#endif // IBASEUI_H
