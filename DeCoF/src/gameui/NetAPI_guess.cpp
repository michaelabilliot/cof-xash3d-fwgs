//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Address helpers of the tracker master server socket
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NetAPI_guess.h"

class CNetAPI : public INetAPI
{
public:
	virtual void NetAdrToSockAddr_guess( netadr_t *a, struct sockaddr *s );
	virtual void SockAddrToNetAdr_guess( struct sockaddr *s, netadr_t *a );
	virtual char *AdrToString_guess( netadr_t *a );
	virtual bool StringToAdr_guess( const char *s, netadr_t *a );
	virtual void GetSocketAddress_guess( int socket, netadr_t *a );
	virtual bool CompareAdr_guess( netadr_t *a, netadr_t *b );
	virtual void GetLocalIP_guess( netadr_t *a );
};

static CNetAPI g_NetAPI_guess;
INetAPI *net_guess = &g_NetAPI_guess;

//-----------------------------------------------------------------------------
// Purpose: netadr_t to a winsock address
//-----------------------------------------------------------------------------
void CNetAPI::NetAdrToSockAddr_guess( netadr_t *a, struct sockaddr *s )
{
	memset( s, 0, sizeof( *s ) );

	if ( a->type == NA_BROADCAST )
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if ( a->type == NA_IP )
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
		((struct sockaddr_in *)s)->sin_port = a->port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: winsock address to a netadr_t
//-----------------------------------------------------------------------------
void CNetAPI::SockAddrToNetAdr_guess( struct sockaddr *s, netadr_t *a )
{
	if ( s->sa_family == AF_INET )
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: "a.b.c.d:port"
//-----------------------------------------------------------------------------
char *CNetAPI::AdrToString_guess( netadr_t *a )
{
	static char s[64];

	memset( s, 0, sizeof( s ) );

	if ( a )
	{
		if ( a->type == NA_LOOPBACK )
		{
			sprintf( s, "loopback" );
		}
		else if ( a->type == NA_IP )
		{
			sprintf( s, "%i.%i.%i.%i:%i", a->ip[0], a->ip[1], a->ip[2], a->ip[3], ntohs( a->port ) );
		}
	}

	return s;
}

//-----------------------------------------------------------------------------
// Purpose: "a.b.c.d[:port]" or a host name to a winsock address
//-----------------------------------------------------------------------------
static bool StringToSockaddr_guess( const char *s, struct sockaddr *sadr )
{
	struct hostent *h;
	char *colon;
	char copy[128];

	memset( sadr, 0, sizeof( *sadr ) );

	struct sockaddr_in *sin = (struct sockaddr_in *)sadr;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	strcpy( copy, s );

	// strip off a trailing :port if present
	for ( colon = copy; *colon; colon++ )
	{
		if ( *colon == ':' )
		{
			*colon = 0;
			sin->sin_port = htons( (short)atoi( colon + 1 ) );
		}
	}

	if ( copy[0] >= '0' && copy[0] <= '9' && strstr( copy, "." ) )
	{
		*(int *)&sin->sin_addr = inet_addr( copy );
	}
	else
	{
		if ( !( h = gethostbyname( copy ) ) )
			return false;
		*(int *)&sin->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: "localhost", "a.b.c.d[:port]" or a host name to a netadr_t
//-----------------------------------------------------------------------------
bool CNetAPI::StringToAdr_guess( const char *s, netadr_t *a )
{
	struct sockaddr sadr;

	if ( !strcmp( s, "localhost" ) )
	{
		memset( a, 0, sizeof( *a ) );
		a->type = NA_LOOPBACK;
		return true;
	}

	if ( !StringToSockaddr_guess( s, &sadr ) )
		return false;

	SockAddrToNetAdr_guess( &sadr, a );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: The address of the host with the port a socket is bound to
//-----------------------------------------------------------------------------
void CNetAPI::GetSocketAddress_guess( int socket, netadr_t *a )
{
	char buff[512];
	struct sockaddr_in address;
	int namelen;

	memset( a, 0, sizeof( *a ) );
	gethostname( buff, 512 );
	buff[511] = 0;

	StringToAdr_guess( buff, a );

	namelen = sizeof( address );
	if ( getsockname( socket, (struct sockaddr *)&address, &namelen ) == 0 )
	{
		a->port = address.sin_port;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Same type, address and port
//-----------------------------------------------------------------------------
bool CNetAPI::CompareAdr_guess( netadr_t *a, netadr_t *b )
{
	if ( a->type != b->type )
		return false;

	if ( a->type == NA_LOOPBACK )
		return true;

	if ( a->type == NA_IP && a->ip[0] == b->ip[0] && a->ip[1] == b->ip[1] && a->ip[2] == b->ip[2] && a->ip[3] == b->ip[3] && a->port == b->port )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: The first address of the local host
//-----------------------------------------------------------------------------
void CNetAPI::GetLocalIP_guess( netadr_t *a )
{
	char s[64];

	if ( !gethostname( s, 64 ) )
	{
		struct hostent *localip = gethostbyname( s );
		if ( localip )
		{
			a->type = NA_IP;
			a->port = 0;
			memcpy( a->ip, localip->h_addr_list[0], 4 );
		}
	}
}
