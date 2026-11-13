//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IENGINEVGUI_H
#define IENGINEVGUI_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <vgui/VGUI.h>

struct TSteamProgress;
struct TSteamError;

enum VGUIPANEL
{
	PANEL_ROOT = 0,
	PANEL_CLIENTDLL,
	PANEL_GAMEUIDLL
};

// Purpose: Centerpoint for handling all user interface in the engine
class IEngineVGui : public IBaseInterface
{
public:
	virtual ~IEngineVGui() {}
	virtual vgui2::VPANEL GetPanel( VGUIPANEL type ) = 0;
	virtual bool SteamRefreshLogin( const char *password, bool isSecure ) = 0;
	virtual bool SteamProcessCall( bool *finished, TSteamProgress *progress, TSteamError *steamError ) = 0;
};

#define VENGINE_VGUI_VERSION "VEngineVGui001"

#endif // IENGINEVGUI_H
