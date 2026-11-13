//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the HLTV director module (camera and event commands)
//
// $NoKeywords: $
//=============================================================================

#ifndef IDIRECTOR_H
#define IDIRECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include "ISystemModule.h"

class IWorld;
class IProxy;
class BitBuffer;
class DirectorCmd;
class IObjectContainer;

class IDirector : virtual public ISystemModule
{
public:
	virtual					~IDirector() {}

	virtual void			NewGame( IWorld *world, IProxy *proxy ) = 0;
	virtual char			*GetModName() = 0;
	virtual void			WriteCommands( BitBuffer *stream, float startTime, float endTime ) = 0;
	virtual int				AddCommand( DirectorCmd *cmd ) = 0;
	virtual bool			RemoveCommand( int index ) = 0;
	virtual DirectorCmd		*GetLastCommand() = 0;
	virtual IObjectContainer	*GetCommands() = 0;
};

#endif // IDIRECTOR_H
