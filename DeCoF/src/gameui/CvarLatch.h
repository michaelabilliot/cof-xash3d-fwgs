//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Saves the value of a cvar and restores it on destruction
//
// $NoKeywords: $
//=============================================================================

#ifndef CVARLATCH_H
#define CVARLATCH_H

#ifdef _WIN32
#pragma once
#endif

//--------------------------------------------------------------------------------------------------------------
/**
 * Remembers the value a cvar had when the latch was created, and sets it
 * back when the latch is destroyed
 */
class CvarLatch
{
public:
	CvarLatch( const char *cvarName, const char *defaultValue );
	~CvarLatch();

private:
	char *m_cvarName;
	char *m_oldValue;
};

#endif // CVARLATCH_H
