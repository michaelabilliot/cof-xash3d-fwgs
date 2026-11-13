//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Address helpers of the tracker master server socket. The class
//			names come from the RTTI; the method names are not recorded
//			anywhere and follow the Quake net_wins.c functions they wrap.
//
// $NoKeywords: $
//=============================================================================

#ifndef NETAPI_GUESS_H
#define NETAPI_GUESS_H

#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"

struct sockaddr;

class INetAPI
{
public:
	virtual void NetAdrToSockAddr_guess( netadr_t *a, struct sockaddr *s ) = 0;
	virtual void SockAddrToNetAdr_guess( struct sockaddr *s, netadr_t *a ) = 0;
	virtual char *AdrToString_guess( netadr_t *a ) = 0;
	virtual bool StringToAdr_guess( const char *s, netadr_t *a ) = 0;
	virtual void GetSocketAddress_guess( int socket, netadr_t *a ) = 0;
	virtual bool CompareAdr_guess( netadr_t *a, netadr_t *b ) = 0;
	virtual void GetLocalIP_guess( netadr_t *a ) = 0;
};

extern INetAPI *net_guess;

#endif // NETAPI_GUESS_H
