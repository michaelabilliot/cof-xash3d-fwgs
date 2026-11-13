//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// engine/launcher interface
#if !defined( ENGINE_LAUNCHER_APIH )
#define ENGINE_LAUNCHER_APIH
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: This is the interface exported by the engine.dll to allow a launcher
//  to run the engine
//-----------------------------------------------------------------------------
class IEngineAPI : public IBaseInterface
{
public:
	virtual int		Run( void *instance, char *basedir, char *cmdline, char *postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) = 0;
};

#define VENGINE_LAUNCHER_API_VERSION "VENGINE_LAUNCHER_API_VERSION002"

#endif // ENGINE_LAUNCHER_APIH
