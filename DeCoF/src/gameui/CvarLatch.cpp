//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Saves the value of a cvar and restores it on destruction
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "CvarLatch.h"
#include "EngineInterface.h"
#include "../game_shared/shared_util.h"

//--------------------------------------------------------------------------------------------------------------
CvarLatch::CvarLatch( const char *cvarName, const char *defaultValue )
{
	m_cvarName = CloneString( cvarName );
	if ( engine->pfnGetCvarPointer( m_cvarName ) )
	{
		m_oldValue = CloneString( engine->pfnGetCvarString( m_cvarName ) );
	}
	else if ( defaultValue )
	{
		m_oldValue = CloneString( defaultValue );
	}
	else
	{
		m_oldValue = NULL;
	}
}

//--------------------------------------------------------------------------------------------------------------
CvarLatch::~CvarLatch()
{
	if ( m_oldValue )
	{
		engine->Cvar_Set( m_cvarName, m_oldValue );
		delete[] m_oldValue;
		m_oldValue = NULL;
	}
	delete[] m_cvarName;
	m_cvarName = NULL;
}
