//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: what the dedicated server launcher exports to the engine
//
// $NoKeywords: $
//=============================================================================

#if !defined( IDEDICATEDEXPORTS_H )
#define IDEDICATEDEXPORTS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

class IDedicatedExports : public IBaseInterface
{
public:
	virtual void		Sys_Printf( char *text ) = 0;
};

#define VENGINE_DEDICATEDEXPORTS_API_VERSION "VENGINE_DEDICATEDEXPORTS_API_VERSION001"

#endif // IDEDICATEDEXPORTS_H
