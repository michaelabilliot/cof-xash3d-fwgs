//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Interface to the server data (remote administration)
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMESERVERDATA_H
#define IGAMESERVERDATA_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

//-----------------------------------------------------------------------------
// Purpose: Interface to server data
//-----------------------------------------------------------------------------
class IGameServerData : public IBaseInterface
{
public:
	virtual void WriteDataRequest( const void *buffer, int bufferSize ) = 0;

	virtual int ReadDataResponse( void *data, int len ) = 0;
};

#define GAMESERVERDATA_INTERFACE_VERSION "GameServerData001"

// enumerations for writing out the requests
enum ServerDataRequestType_t
{
	SERVERDATA_REQUESTVALUE,
	SERVERDATA_SETVALUE,
	SERVERDATA_EXECCOMMAND,
};

enum ServerDataResponseType_t
{
	SERVERDATA_RESPONSE_VALUE = 0,
	SERVERDATA_UPDATE,
};

#endif // IGAMESERVERDATA_H
