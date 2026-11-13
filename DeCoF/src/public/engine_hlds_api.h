//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: the interface the dedicated server launcher runs the engine through
//
// $NoKeywords: $
//=============================================================================

#if !defined( ENGINE_HLDS_API_H )
#define ENGINE_HLDS_API_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

class IDedicatedServerAPI : public IBaseInterface
{
public:
	virtual bool		Init( char *basedir, char *cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory ) = 0;
	virtual int			Shutdown( void ) = 0;
	virtual bool		RunFrame( void ) = 0;
	virtual void		AddConsoleText( char *text ) = 0;
	virtual void		UpdateStatus( float *fps, int *nActive, int *nMaxPlayers, char *pszMap ) = 0;
};

#define VENGINE_HLDS_API_VERSION "VENGINE_HLDS_API_VERSION002"

#endif // ENGINE_HLDS_API_H
