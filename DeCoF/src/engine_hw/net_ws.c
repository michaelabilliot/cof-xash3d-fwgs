// net_ws.c -- Windows sockets: UDP and IPX, loopback, fake lag, split
// packets, the network thread and the net graph

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "host.h"
#include "net.h"
#include "delta.h"
#include "delta_packet.h"
#include "server.h"
#include "client.h"
#include "screen.h"
#include "draw.h"
#include "vid.h"

#include <stdarg.h>
#include <math.h>
#include <wsipx.h>

kbutton_t	*ClientDLL_FindKey( const char *name );

static void MaxPlayers_f( void );
void	NET_InitColors( void );

int					use_thread;
static qboolean		net_thread_initialized;
CRITICAL_SECTION	net_cs;

static cvar_t	net_address = { "net_address", "" };
static cvar_t	ipname = { "ip", "localhost" };
cvar_t			iphostport = { "ip_hostport", "0" };
cvar_t			hostport = { "hostport", "0" };
static cvar_t	defport = { "port", "27015" };
static cvar_t	ip_clientport = { "ip_clientport", "0" };
static cvar_t	clientport = { "clientport", "27005" };
cvar_t			clockwindow = { "clockwindow", "0.5" };
cvar_t			multicastport = { "multicastport", "27025" };
static cvar_t	ipx_hostport = { "ipx_hostport", "0" };
static cvar_t	ipx_clientport = { "ipx_clientport", "0" };
cvar_t			fakelag = { "fakelag", "0.0" };
cvar_t			fakeloss = { "fakeloss", "0.0" };
cvar_t			net_graph = { "net_graph", "0", FCVAR_ARCHIVE };
cvar_t			net_graphwidth = { "net_graphwidth", "150" };
cvar_t			net_scale = { "net_scale", "5", FCVAR_ARCHIVE };
cvar_t			net_graphpos = { "net_graphpos", "1", FCVAR_ARCHIVE };

qboolean	noip;
qboolean	noipx;

netadr_t	net_local_adr;
netadr_t	net_local_ipx_adr;

netadr_t	net_from;
byte		net_message_buffer[65536];
sizebuf_t	net_message;

byte		in_message_buf[65536];
sizebuf_t	in_message;
netadr_t	in_from;

int			ip_sockets[3];
int			ipx_sockets[3];

static int	net_sleepforever = 1;

#define	MAX_LOOPBACK	4

typedef struct
{
	byte	data[NET_MAX_PAYLOAD];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	int			get, send;
} loopback_t;

static loopback_t	loopbacks[2];

typedef struct packetlag_s
{
	unsigned char		*pPacketData;	// Raw stream data is stored.
	int					nSize;
	netadr_t			net_from;
	float				receivedTime;
	struct packetlag_s	*pNext;
	struct packetlag_s	*pPrev;
} packetlag_t;

static packetlag_t	g_pLagData[3];	// List of lag structures, if fakelag is set.

void NET_ThreadLock( void )
{
	if ( use_thread && net_thread_initialized )
		EnterCriticalSection( &net_cs );
}

void NET_ThreadUnlock( void )
{
	if ( use_thread && net_thread_initialized )
		LeaveCriticalSection( &net_cs );
}

unsigned short Q_ntohs( unsigned short netshort )
{
	return ntohs( netshort );
}

void NetadrToSockadr( netadr_t *a, struct sockaddr *s )
{
	Q_memset( s, 0, sizeof( *s ) );

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
	else if ( a->type == NA_IPX )
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		Q_memcpy( ((struct sockaddr_ipx *)s)->sa_netnum, &a->ipx[0], 4 );
		Q_memcpy( ((struct sockaddr_ipx *)s)->sa_nodenum, &a->ipx[4], 6 );
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
	else if ( a->type == NA_BROADCAST_IPX )
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		Q_memset( ((struct sockaddr_ipx *)s)->sa_netnum, 0, 4 );
		Q_memset( ((struct sockaddr_ipx *)s)->sa_nodenum, 0xff, 6 );
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
}

void SockadrToNetadr( struct sockaddr *s, netadr_t *a )
{
	if ( s->sa_family == AF_INET )
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
	else if ( s->sa_family == AF_IPX )
	{
		a->type = NA_IPX;
		Q_memcpy( &a->ipx[0], ((struct sockaddr_ipx *)s)->sa_netnum, 4 );
		Q_memcpy( &a->ipx[4], ((struct sockaddr_ipx *)s)->sa_nodenum, 6 );
		a->port = ((struct sockaddr_ipx *)s)->sa_socket;
	}
}

unsigned short NET_HostToNetShort( unsigned short us_in )
{
	return htons( us_in );
}

qboolean NET_CompareAdr( netadr_t a, netadr_t b )
{
	if ( a.type != b.type )
		return false;

	if ( a.type == NA_LOOPBACK )
		return true;

	if ( a.type == NA_IP )
	{
		if ( a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port )
			return true;
		return false;
	}

	if ( a.type == NA_IPX )
	{
		if ( ( Q_memcmp( a.ipx, b.ipx, 10 ) == 0 ) && a.port == b.port )
			return true;
		return false;
	}

	return false;
}

qboolean NET_CompareClassBAdr( netadr_t a, netadr_t b )
{
	if ( a.type != b.type )
		return false;

	if ( a.type == NA_LOOPBACK )
		return true;

	if ( a.type == NA_IP )
	{
		if ( a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] )
			return true;
		return false;
	}

	if ( a.type == NA_IPX )
		return true;

	return false;
}

qboolean NET_IsReservedAdr( netadr_t a )
{
	if ( a.type == NA_LOOPBACK )
		return true;

	if ( a.type == NA_IP )
	{
		if ( ( a.ip[0] == 10 ) || ( a.ip[0] == 127 ) ||
			( a.ip[0] == 172 && a.ip[1] >= 16 && a.ip[1] <= 31 ) ||
			( a.ip[0] == 192 && a.ip[1] >= 168 ) )
			return true;
		return false;
	}

	if ( a.type == NA_IPX )
		return true;

	return false;
}

qboolean NET_CompareBaseAdr( netadr_t a, netadr_t b )
{
	if ( a.type != b.type )
		return false;

	if ( a.type == NA_LOOPBACK )
		return true;

	if ( a.type == NA_IP )
	{
		if ( a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] )
			return true;
		return false;
	}

	if ( a.type == NA_IPX )
	{
		if ( Q_memcmp( a.ipx, b.ipx, 10 ) == 0 )
			return true;
		return false;
	}

	return false;
}

char *NET_AdrToString( netadr_t a )
{
	static char s[64];

	Q_memset( s, 0, sizeof( s ) );

	if ( a.type == NA_LOOPBACK )
		_snprintf( s, sizeof( s ), "loopback" );
	else if ( a.type == NA_IP )
		_snprintf( s, sizeof( s ), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs( a.port ) );
	else
		_snprintf( s, sizeof( s ), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%i", a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9], ntohs( a.port ) );

	return s;
}

char *NET_BaseAdrToString( netadr_t a )
{
	static char s[64];

	Q_memset( s, 0, sizeof( s ) );

	if ( a.type == NA_LOOPBACK )
		_snprintf( s, sizeof( s ), "loopback" );
	else if ( a.type == NA_IP )
		_snprintf( s, sizeof( s ), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3] );
	else
		_snprintf( s, sizeof( s ), "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x", a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9] );

	return s;
}

/*
=============
NET_StringToAdr

idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
#define DO( src, dest )		\
	copy[0] = s[src];		\
	copy[1] = s[src + 1];	\
	sscanf( copy, "%x", &val );	\
	((struct sockaddr_ipx *)sadr)->dest = val

qboolean NET_StringToSockaddr( char *s, struct sockaddr *sadr )
{
	struct hostent	*h;
	char	*colon;
	int		val;
	char	copy[128];

	Q_memset( sadr, 0, sizeof( *sadr ) );

	if ( ( strlen( s ) >= 23 ) && ( s[8] == ':' ) && ( s[21] == ':' ) )	// check for an IPX address
	{
		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		copy[2] = 0;
		DO( 0, sa_netnum[0] );
		DO( 2, sa_netnum[1] );
		DO( 4, sa_netnum[2] );
		DO( 6, sa_netnum[3] );
		DO( 9, sa_nodenum[0] );
		DO( 11, sa_nodenum[1] );
		DO( 13, sa_nodenum[2] );
		DO( 15, sa_nodenum[3] );
		DO( 17, sa_nodenum[4] );
		DO( 19, sa_nodenum[5] );
		sscanf( &s[22], "%u", &val );
		((struct sockaddr_ipx *)sadr)->sa_socket = htons( (unsigned short)val );
	}
	else
	{
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;

		((struct sockaddr_in *)sadr)->sin_port = 0;

		Q_strncpy( copy, s, sizeof( copy ) - 1 );
		copy[sizeof( copy ) - 1] = 0;

		// strip off a trailing :port if present
		for ( colon = copy; *colon; colon++ )
		{
			if ( *colon == ':' )
			{
				*colon = 0;
				((struct sockaddr_in *)sadr)->sin_port = htons( (short)atoi( colon + 1 ) );
			}
		}

		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr( copy );
		if ( *(int *)&((struct sockaddr_in *)sadr)->sin_addr == INADDR_NONE )
		{
			h = gethostbyname( copy );
			if ( !h )
				return false;
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
	}

	return true;
}

#undef DO

qboolean NET_StringToAdr( char *s, netadr_t *a )
{
	struct sockaddr sadr;

	if ( !Q_strcmp( s, "localhost" ) )
	{
		Q_memset( a, 0, sizeof( *a ) );
		a->type = NA_LOOPBACK;
		return true;
	}

	if ( !NET_StringToSockaddr( s, &sadr ) )
		return false;

	SockadrToNetadr( &sadr, a );

	return true;
}

qboolean NET_IsLocalAddress( netadr_t adr )
{
	return adr.type == NA_LOOPBACK;
}

char *NET_ErrorString( int code )
{
	switch ( code )
	{
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
}

void NET_TransferRawData( sizebuf_t *msg, unsigned char *pStart, int nSize )
{
	Q_memcpy( msg->data, pStart, nSize );
	msg->cursize = nSize;
}

qboolean NET_GetLoopPacket( netsrc_t sock, netadr_t *in_from, sizebuf_t *msg )
{
	int		i;
	loopback_t	*loop;

	loop = &loopbacks[sock];

	if ( loop->send - loop->get > MAX_LOOPBACK )
		loop->get = loop->send - MAX_LOOPBACK;

	if ( loop->get >= loop->send )
		return false;

	i = loop->get & ( MAX_LOOPBACK - 1 );
	loop->get++;

	NET_TransferRawData( msg, loop->msgs[i].data, loop->msgs[i].datalen );

	Q_memset( in_from, 0, sizeof( *in_from ) );
	in_from->type = NA_LOOPBACK;
	return true;
}

void NET_SendLoopPacket( netsrc_t sock, int length, void *data, netadr_t to )
{
	int		i;
	loopback_t	*loop;

	NET_ThreadLock();

	loop = &loopbacks[sock ^ 1];

	i = loop->send & ( MAX_LOOPBACK - 1 );
	loop->send++;

	Q_memcpy( loop->msgs[i].data, data, length );
	loop->msgs[i].datalen = length;

	NET_ThreadUnlock();
}

void NET_RemoveFromPacketList( packetlag_t *pPacket )
{
	pPacket->pPrev->pNext = pPacket->pNext;
	pPacket->pNext->pPrev = pPacket->pPrev;
	pPacket->pPrev = NULL;
	pPacket->pNext = NULL;
}

int NET_CountLaggedList( packetlag_t *pList )
{
	int c;
	packetlag_t *p;

	c = 0;

	for ( p = pList->pNext; p && p != pList; p = p->pNext )
		c++;

	return c;
}

void NET_ClearLaggedList( packetlag_t *pList )
{
	packetlag_t *p, *n;

	p = pList->pNext;
	while ( p && p != pList )
	{
		n = p->pNext;
		NET_RemoveFromPacketList( p );
		if ( p->pPacketData )
		{
			Mem_Free( p->pPacketData );
			p->pPacketData = NULL;
		}
		Mem_Free( p );
		p = n;
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

void NET_AddToLagged( netsrc_t sock, packetlag_t *pList, packetlag_t *pPacket, netadr_t *net_from, sizebuf_t messagedata, float timestamp )
{
	unsigned char *pStart;

	if ( pPacket->pPrev || pPacket->pNext )
	{
		Con_Printf( "Packet already linked\n" );
		return;
	}

	pPacket->pPrev = pList->pPrev;
	pList->pPrev->pNext = pPacket;
	pList->pPrev = pPacket;
	pPacket->pNext = pList;

	pStart = messagedata.data;
	pPacket->pPacketData = Mem_Malloc( messagedata.cursize );
	Q_memcpy( pPacket->pPacketData, pStart, messagedata.cursize );
	pPacket->nSize = messagedata.cursize;
	pPacket->receivedTime = timestamp;
	pPacket->net_from = *net_from;
}

static float	gFakeLag;

void NET_AdjustLag( void )
{
	static double lasttime;
	double	dt;
	float	diff;
	float	converge;

	dt = realtime - lasttime;
	dt = max( 0.0, dt );
	dt = min( 0.1, dt );

	lasttime = realtime;

	if ( !allow_cheats && fakelag.value != 0 )
	{
		Con_Printf( "Server must enable cheats to activate fakelag\n" );
		Cvar_SetValue( "fakelag", 0 );
		gFakeLag = 0;
		return;
	}

	if ( fakelag.value == gFakeLag )
		return;

	diff = fakelag.value - gFakeLag;
	converge = 200.0 * dt;
	if ( converge > fabs( diff ) )
		converge = fabs( diff );

	if ( diff < 0.0 )
		converge = -converge;

	gFakeLag += converge;
}

qboolean NET_LagPacket( qboolean newdata, netsrc_t sock, netadr_t *from, sizebuf_t *data )
{
	static int	losscount[3];
	packetlag_t	*pNewPacketLag;
	packetlag_t	*pPacket;
	float		curtime;

	if ( gFakeLag <= 0.0 )
	{
		NET_ClearLagData( true, true );
		return newdata;
	}

	curtime = realtime;

	if ( newdata )
	{
		if ( fakeloss.value )
		{
			if ( !allow_cheats )
			{
				Cvar_SetValue( "fakeloss", 0 );
			}
			else
			{
				losscount[sock]++;
				if ( fakeloss.value > 0 )
				{
					if ( RandomLong( 0, 100 ) <= (int)fakeloss.value )
						return false;
				}
				else
				{
					int ninterval;

					ninterval = fabs( fakeloss.value );
					ninterval = max( 2, ninterval );

					if ( !( losscount[sock] % ninterval ) )
						return false;
				}
			}
		}

		pNewPacketLag = Mem_ZeroMalloc( sizeof( packetlag_t ) );
		NET_AddToLagged( sock, &g_pLagData[sock], pNewPacketLag, from, *data, curtime );
	}

	pPacket = g_pLagData[sock].pNext;
	while ( pPacket != &g_pLagData[sock] )
	{
		if ( pPacket->receivedTime <= curtime - ( gFakeLag / 1000.0f ) )
		{
			NET_RemoveFromPacketList( pPacket );
			NET_TransferRawData( &in_message, pPacket->pPacketData, pPacket->nSize );
			in_from = pPacket->net_from;
			if ( pPacket->pPacketData )
				free( pPacket->pPacketData );
			Mem_Free( pPacket );
			return true;
		}

		pPacket = pPacket->pNext;
	}

	return false;
}

void NET_FlushSocket( netsrc_t sock )
{
	struct sockaddr	from;
	int		fromlen;
	int		net_socket = 0;
	unsigned char	buf[MAX_UDP_PACKET];

	net_socket = ip_sockets[sock];
	if ( !net_socket )
		return;

	fromlen = sizeof( from );
	while ( recvfrom( net_socket, buf, sizeof( buf ), 0, &from, &fromlen ) > 0 )
		;
}

typedef struct
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	char	buffer[MAX_UDP_PACKET];	// This has to be big enough to hold the largest message
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack( push, 1 )
typedef struct
{
	int		netID;
	int		sequenceNumber;
	unsigned char packetID;
} SPLITPACKET;
#pragma pack( pop )

#define NET_HEADER_FLAG_SPLITPACKET		-2
#define NET_MAX_SPLIT_PACKETS			5
#define SPLIT_SIZE						( MAX_ROUTEABLE_PACKET - sizeof( SPLITPACKET ) )

LONGPACKET	gNetSplit;

qboolean NET_GetLong( unsigned char *pData, int size, int *outSize )
{
	int		packetNumber, packetCount, sequenceNumber, offset;
	unsigned char	packetID;
	SPLITPACKET	*pHeader;
	static int	gNetSplitFlags[NET_MAX_SPLIT_PACKETS] = { -1, -1, -1, -1, -1 };

	pHeader = (SPLITPACKET *)pData;

	sequenceNumber = pHeader->sequenceNumber;
	packetID = pHeader->packetID;
	// High byte is packet number
	packetNumber = ( packetID >> 4 );
	// Low byte is number of total packets
	packetCount = ( packetID & 0xf );

	if ( packetNumber >= ARRAYSIZE( gNetSplitFlags ) || packetCount > ARRAYSIZE( gNetSplitFlags ) )
	{
		Con_Printf( "Malformed packet number (%i)\n", packetNumber );
		return false;
	}

	if ( gNetSplit.currentSequence == -1 || sequenceNumber != gNetSplit.currentSequence )
	{
		gNetSplit.currentSequence = sequenceNumber;
		gNetSplit.splitCount = packetCount;

		if ( net_showpackets.value == 4 )
			Con_Printf( "<-- Split packet restart %i count %i seq\n", packetCount, sequenceNumber );
	}

	size -= sizeof( SPLITPACKET );

	if ( gNetSplitFlags[packetNumber] != sequenceNumber )
	{
		if ( packetNumber == ( packetCount - 1 ) )
			gNetSplit.totalSize = SPLIT_SIZE * ( packetCount - 1 ) + size;

		--gNetSplit.splitCount;
		gNetSplitFlags[packetNumber] = sequenceNumber;

		if ( net_showpackets.value == 4 )
			Con_Printf( "<-- Split packet %i of %i, %i bytes %i seq\n", packetNumber + 1, packetCount, size, sequenceNumber );

		offset = SPLIT_SIZE * packetNumber;
		if ( offset + size > sizeof( gNetSplit.buffer ) )
		{
			Con_Printf( "Malformed packet size (%i, %i)\n", offset, size );
			return false;
		}

		memcpy( &gNetSplit.buffer[offset], pData + sizeof( SPLITPACKET ), size );
	}
	else
	{
		Con_Printf( "NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes )\n", packetNumber + 1, packetCount, size );
	}

	if ( gNetSplit.splitCount <= 0 )
	{
		int i;

		for ( i = 0; i < packetCount; i++ )
		{
			if ( gNetSplitFlags[i] != gNetSplit.currentSequence )
			{
				Con_Printf( "Split packet without all %i parts, part %i had wrong sequence %i/%i\n", packetCount, i + 1, gNetSplitFlags[i], gNetSplit.currentSequence );
				return false;
			}
		}

		gNetSplit.currentSequence = -1;	// Clear packet

		if ( gNetSplit.totalSize > sizeof( gNetSplit.buffer ) )
		{
			Con_Printf( "Split packet too large! %d bytes\n", gNetSplit.totalSize );
			return false;
		}

		memcpy( pData, gNetSplit.buffer, gNetSplit.totalSize );
		*outSize = gNetSplit.totalSize;
		return true;
	}

	return false;
}

qboolean NET_QueuePacket( netsrc_t sock )
{
	int		ret;
	struct sockaddr	from;
	int		fromlen;
	int		net_socket = 0;
	int		protocol;
	int		err;
	unsigned char	buf[MAX_UDP_PACKET];

	for ( protocol = 0; protocol < 2; protocol++ )
	{
		if ( protocol == 0 )
			net_socket = ip_sockets[sock];
		else
			net_socket = ipx_sockets[sock];

		if ( !net_socket )
			continue;

		fromlen = sizeof( from );
		ret = recvfrom( net_socket, buf, sizeof( buf ), 0, &from, &fromlen );
		if ( ret == -1 )
		{
			err = WSAGetLastError();

			if ( err == WSAEWOULDBLOCK )
				continue;
			if ( err == WSAECONNRESET )
				continue;
			if ( err == WSAECONNREFUSED )
				continue;
			if ( err == WSAEMSGSIZE )
			{
				Con_DPrintf( "NET_QueuePacket:  Ignoring oversized network message\n" );
				continue;
			}

			if ( cls.state == ca_dedicated )
				Con_Printf( "NET_QueuePacket: %s\n", NET_ErrorString( err ) );
			else
				Sys_Error( "NET_QueuePacket: %s", NET_ErrorString( err ) );
			continue;
		}

		SockadrToNetadr( &from, &in_from );

		if ( ret == sizeof( buf ) )
		{
			Con_Printf( "NET_QueuePacket:  Oversize packet from %s\n", NET_AdrToString( in_from ) );
			continue;
		}

		NET_TransferRawData( &in_message, buf, ret );

		// Check for split message
		if ( *(int *)in_message.data == NET_HEADER_FLAG_SPLITPACKET )
		{
			if ( in_message.cursize < sizeof( SPLITPACKET ) )
			{
				Con_Printf( "Invalid split packet length %i\n", in_message.cursize );
				return false;
			}

			return NET_GetLong( in_message.data, ret, &in_message.cursize );
		}

		return NET_LagPacket( true, sock, &in_from, &in_message );
	}

	return NET_LagPacket( false, sock, NULL, NULL );
}

int NET_Sleep( void )
{
	fd_set	fdset;
	int		i;
	int		sock;
	struct timeval	tv;
	int		number;

	FD_ZERO( &fdset );
	i = 0;

	for ( sock = 0; sock < 3; sock++ )
	{
		if ( ip_sockets[sock] )
		{
			FD_SET( ip_sockets[sock], &fdset );
			if ( ip_sockets[sock] > i )
				i = ip_sockets[sock];
		}

		if ( ipx_sockets[sock] )
		{
			FD_SET( ipx_sockets[sock], &fdset );
			if ( ipx_sockets[sock] > i )
				i = ipx_sockets[sock];
		}
	}

	tv.tv_sec = 0;
	tv.tv_usec = 20 * 1000;

	number = select( i + 1, &fdset, NULL, NULL, net_sleepforever ? NULL : &tv );
	return number;
}

typedef struct net_messages_s
{
	struct net_messages_s	*next;
	qboolean				preallocated;
	unsigned char			*buffer;
	netadr_t				from;
	int						buffersize;
} net_messages_t;

// Split up the queues so that the server can process messages from the
//  client and vice versa.
static net_messages_t	*messages[3];

// List of preallocated messages
#define NUM_MSG_QUEUES	40
#define MSG_QUEUE_SIZE	1536

static net_messages_t	*normalqueue;

static HANDLE	hNetThread;
DWORD			dwNetThreadId;

net_messages_t	*NET_AllocMsg( int size );

/*
==================
NET_ThreadMain_guess

The network thread: waits on the sockets and queues every received packet
for NET_GetPacket.
==================
*/
int NET_ThreadMain_guess( void )
{
	net_messages_t	*last_guess;
	net_messages_t	*msg_guess;
	qboolean		done_guess = false;
	int				sel_guess;
	int				sock_guess;
	qboolean		ret_guess;

	while ( 1 )
	{
		sel_guess = NET_Sleep();

		done_guess = false;
		while ( !done_guess && sel_guess )
		{
			done_guess = true;

			for ( sock_guess = 0; sock_guess < 3; sock_guess++ )
			{
				NET_ThreadLock();

				ret_guess = NET_QueuePacket( sock_guess );
				if ( ret_guess )
				{
					done_guess = false;

					msg_guess = NET_AllocMsg( in_message.cursize );
					Q_memcpy( msg_guess->buffer, in_message.data, in_message.cursize );
					msg_guess->from = in_from;

					if ( !messages[sock_guess] )
					{
						msg_guess->next = messages[sock_guess];
						messages[sock_guess] = msg_guess;
					}
					else
					{
						last_guess = messages[sock_guess];
						while ( last_guess->next )
							last_guess = last_guess->next;
						last_guess->next = msg_guess;
						msg_guess->next = NULL;
					}
				}

				NET_ThreadUnlock();
			}
		}

		Sys_Sleep( 1 );
	}

	return 0;
}

void NET_StartThread( void )
{
	if ( !use_thread )
		return;

	if ( net_thread_initialized )
		return;

	net_thread_initialized = true;

	InitializeCriticalSection( &net_cs );
	hNetThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)NET_ThreadMain_guess, NULL, 0, &dwNetThreadId );
	if ( !hNetThread )
	{
		DeleteCriticalSection( &net_cs );
		net_thread_initialized = false;
		use_thread = false;
		Sys_Error( "Couldn't initialize network thread, run without -net_thread\n" );
	}
}

void NET_StopThread( void )
{
	if ( !use_thread )
		return;

	if ( !net_thread_initialized )
		return;

	TerminateThread( hNetThread, 0 );
	DeleteCriticalSection( &net_cs );
	net_thread_initialized = false;
}

void *net_malloc( size_t size )
{
	unsigned char	*pbuffer;

	pbuffer = Mem_ZeroMalloc( size );
	return pbuffer;
}

net_messages_t *NET_AllocMsg( int size )
{
	net_messages_t	*pmsg;

	if ( size > MSG_QUEUE_SIZE || !normalqueue )
	{
		pmsg = net_malloc( sizeof( net_messages_t ) );
		pmsg->buffer = net_malloc( size );
		pmsg->buffersize = size;
		pmsg->preallocated = false;
		return pmsg;
	}

	pmsg = normalqueue;
	normalqueue = normalqueue->next;
	pmsg->buffersize = size;
	return pmsg;
}

void NET_FreeMsg( net_messages_t *pmsg )
{
	if ( pmsg->preallocated )
	{
		pmsg->next = normalqueue;
		normalqueue = pmsg;
	}
	else
	{
		Mem_Free( pmsg->buffer );
		Mem_Free( pmsg );
	}
}

qboolean NET_GetPacket( netsrc_t sock )
{
	net_messages_t	*pmsg;
	qboolean		bret = false;

	NET_AdjustLag();
	NET_ThreadLock();

	if ( NET_GetLoopPacket( sock, &in_from, &in_message ) )
	{
		bret = NET_LagPacket( true, sock, &in_from, &in_message );
	}
	else
	{
		if ( !use_thread )
			bret = NET_QueuePacket( sock );

		if ( !bret )
			bret = NET_LagPacket( false, sock, NULL, NULL );
	}

	if ( bret )
	{
		Q_memcpy( net_message.data, in_message.data, in_message.cursize );
		net_message.cursize = in_message.cursize;
		net_from = in_from;
	}
	else
	{
		if ( messages[sock] )
		{
			pmsg = messages[sock];
			messages[sock] = messages[sock]->next;

			net_message.cursize = pmsg->buffersize;
			Q_memcpy( net_message.data, pmsg->buffer, pmsg->buffersize );
			net_from = pmsg->from;
			msg_readcount = 0;

			NET_FreeMsg( pmsg );
			bret = true;
		}
	}

	NET_ThreadUnlock();
	return bret;
}

void NET_AllocateQueues( void )
{
	int				i;
	net_messages_t	*p;

	for ( i = 0; i < NUM_MSG_QUEUES; i++ )
	{
		p = Mem_ZeroMalloc( sizeof( net_messages_t ) );
		p->buffer = Mem_ZeroMalloc( MSG_QUEUE_SIZE );
		p->preallocated = true;
		p->next = normalqueue;
		normalqueue = p;
	}

	NET_StartThread();
}

void NET_FlushQueues( void )
{
	int				i;
	net_messages_t	*p, *n;

	NET_StopThread();

	for ( i = 0; i < 3; i++ )
	{
		p = messages[i];
		while ( p )
		{
			n = p->next;
			Mem_Free( p->buffer );
			Mem_Free( p );
			p = n;
		}

		messages[i] = NULL;
	}

	p = normalqueue;
	while ( p )
	{
		n = p->next;
		Mem_Free( p->buffer );
		Mem_Free( p );
		p = n;
	}

	normalqueue = NULL;
}

int NET_SendLong( netsrc_t sock, int s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen )
{
	static int32 gSequenceNumber = 1;

	// Do we need to break this packet up?
	if ( sock == NS_SERVER && len > MAX_ROUTEABLE_PACKET )
	{
		char	packet[MAX_ROUTEABLE_PACKET];
		int		totalSent, ret, size, packetCount, packetNumber;
		SPLITPACKET	*pPacket;

		// Bump sequence number
		gSequenceNumber++;
		if ( gSequenceNumber < 0 )
			gSequenceNumber = 1;

		pPacket = (SPLITPACKET *)packet;
		pPacket->netID = NET_HEADER_FLAG_SPLITPACKET;
		pPacket->sequenceNumber = gSequenceNumber;
		packetNumber = 0;
		totalSent = 0;
		packetCount = ( len + SPLIT_SIZE - 1 ) / SPLIT_SIZE;

		while ( len > 0 )
		{
			size = min( SPLIT_SIZE, len );

			pPacket->packetID = ( packetNumber << 4 ) + packetCount;

			memcpy( packet + sizeof( SPLITPACKET ), buf + ( packetNumber * SPLIT_SIZE ), size );

			if ( net_showpackets.value == 4 )
			{
				netadr_t adr;

				SockadrToNetadr( (struct sockaddr *)to, &adr );
				Con_DPrintf( "Sending split %i of %i with %i bytes and seq %i to %s\n", packetNumber + 1, packetCount, size, gSequenceNumber, NET_AdrToString( adr ) );
			}

			ret = sendto( s, packet, size + sizeof( SPLITPACKET ), flags, to, tolen );
			if ( ret < 0 )
				return ret;

			if ( ret >= size )
				totalSent += size;

			len -= size;
			packetNumber++;
		}

		return totalSent;
	}
	else
	{
		int nSend = 0;

		nSend = sendto( s, buf, len, flags, to, tolen );
		return nSend;
	}
}

void NET_SendPacket( netsrc_t sock, int length, void *data, netadr_t to )
{
	int		ret;
	struct sockaddr	addr;
	int		net_socket;

	if ( to.type == NA_LOOPBACK )
	{
		NET_SendLoopPacket( sock, length, data, to );
		return;
	}

	if ( to.type == NA_BROADCAST )
	{
		net_socket = ip_sockets[sock];
		if ( !net_socket )
			return;
	}
	else if ( to.type == NA_IP )
	{
		net_socket = ip_sockets[sock];
		if ( !net_socket )
			return;
	}
	else if ( to.type == NA_IPX )
	{
		net_socket = ipx_sockets[sock];
		if ( !net_socket )
			return;
	}
	else if ( to.type == NA_BROADCAST_IPX )
	{
		net_socket = ipx_sockets[sock];
		if ( !net_socket )
			return;
	}
	else
		Sys_Error( "NET_SendPacket: bad address type" );

	NetadrToSockadr( &to, &addr );

	ret = NET_SendLong( sock, net_socket, data, length, 0, &addr, sizeof( addr ) );
	if ( ret == -1 )
	{
		int err = WSAGetLastError();

		// wouldblock is silent
		if ( err == WSAEWOULDBLOCK )
			return;

		if ( err == WSAECONNREFUSED )
			return;

		if ( err == WSAECONNRESET )
			return;

		// some PPP links dont allow broadcasts
		if ( err == WSAEADDRNOTAVAIL && ( to.type == NA_BROADCAST || to.type == NA_BROADCAST_IPX ) )
			return;

		if ( cls.state == ca_dedicated )	// let dedicated servers continue after errors
		{
			Con_Printf( "NET_SendPacket ERROR: %s\n", NET_ErrorString( err ) );
		}
		else
		{
			if ( err == WSAEADDRNOTAVAIL || err == WSAENOBUFS )
			{
				Con_DPrintf( "NET_SendPacket Warning: %s : %s\n", NET_ErrorString( err ), NET_AdrToString( to ) );
			}
			else
			{
				Sys_Error( "NET_SendPacket ERROR: %s\n", NET_ErrorString( err ) );
			}
		}
	}
}

int NET_IPSocket( char *net_interface, int port, qboolean multicast )
{
	int		newsocket;
	struct sockaddr_in	address;
	qboolean	_true = 1;
	int		i = 1;
	int		err;

	if ( ( newsocket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == -1 )
	{
		err = WSAGetLastError();
		if ( err != WSAEAFNOSUPPORT )
			Con_Printf( "WARNING: UDP_OpenSocket: port: %d socket: %s", port, NET_ErrorString( err ) );
		return 0;
	}

	// make it non-blocking
	if ( ioctlsocket( newsocket, FIONBIO, &_true ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  ioctl FIONBIO: %s\n", port, NET_ErrorString( err ) );
		return 0;
	}

	// make it broadcast capable
	if ( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof( i ) ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_BROADCAST: %s\n", port, NET_ErrorString( err ) );
		return 0;
	}

	if ( COM_CheckParm( "-reuse" ) || multicast )
	{
		if ( setsockopt( newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&_true, sizeof( qboolean ) ) == -1 )
		{
			err = WSAGetLastError();
			Con_Printf( "WARNING: UDP_OpenSocket: port: %d  setsockopt SO_REUSEADDR: %s\n", port, NET_ErrorString( err ) );
			return 0;
		}
	}

	if ( !net_interface || !net_interface[0] || !Q_stricmp( net_interface, "localhost" ) )
		address.sin_addr.s_addr = INADDR_ANY;
	else
		NET_StringToSockaddr( net_interface, (struct sockaddr *)&address );

	if ( port == PORT_ANY )
		address.sin_port = 0;
	else
		address.sin_port = htons( (short)port );

	address.sin_family = AF_INET;

	if ( bind( newsocket, (void *)&address, sizeof( address ) ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: UDP_OpenSocket: port: %d  bind: %s\n", port, NET_ErrorString( err ) );
		closesocket( newsocket );
		return 0;
	}

	if ( COM_CheckParm( "-loopback" ) )
		i = 1;
	else
		i = 0;

	if ( setsockopt( newsocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&i, sizeof( i ) ) == -1 )
	{
		err = WSAGetLastError();
		Con_DPrintf( "WARNING: UDP_OpenSocket: port %d setsockopt IP_MULTICAST_LOOP: %s\n", port, NET_ErrorString( err ) );
	}

	return newsocket;
}

void NET_OpenIP( void )
{
	cvar_t	*ip;
	int		port;
	int		dedicated;
	int		sv_port = 0, cl_port = 0, mc_port = 0;
	static qboolean bFirst = true;

	ip = &ipname;
	dedicated = cls.state == ca_dedicated;

	NET_ThreadLock();

	if ( !ip_sockets[NS_SERVER] )
	{
		port = iphostport.value;
		if ( !port )
		{
			port = hostport.value;
			if ( !port )
			{
				hostport.value = defport.value;
				port = defport.value;
			}
		}

		sv_port = port;
		ip_sockets[NS_SERVER] = NET_IPSocket( ip->string, port, false );

		if ( !ip_sockets[NS_SERVER] && dedicated )
			Sys_Error( "Couldn't allocate dedicated server IP port %d.", sv_port );
	}

	NET_ThreadUnlock();

	// dedicated servers don't need client ports
	if ( cls.state == ca_dedicated )
		return;

	NET_ThreadLock();

	if ( !ip_sockets[NS_CLIENT] )
	{
		port = ip_clientport.value;
		if ( !port )
		{
			port = clientport.value;
			if ( !port )
				port = PORT_ANY;
		}

		cl_port = port;
		ip_sockets[NS_CLIENT] = NET_IPSocket( ip->string, port, false );
		if ( !ip_sockets[NS_CLIENT] )
			ip_sockets[NS_CLIENT] = NET_IPSocket( ip->string, PORT_ANY, false );
	}

	if ( !ip_sockets[NS_MULTICAST] )
	{
		port = multicastport.value;
		mc_port = port;
		ip_sockets[NS_MULTICAST] = NET_IPSocket( ip->string, port, true );
		if ( !ip_sockets[NS_MULTICAST] && !dedicated )
			Con_Printf( "Warning! Couldn't allocate multicast IP port.\n" );
	}

	NET_ThreadUnlock();

	if ( bFirst )
	{
		bFirst = false;
		Con_Printf( "NET Ports:  server %i, client %i\n", sv_port, cl_port );
	}
}

/*
====================
NET_IPXSocket_guess
====================
*/
int NET_IPXSocket_guess( int port )
{
	int		newsocket;
	struct sockaddr_ipx	address;
	int		_true = 1;
	int		err;

	if ( ( newsocket = socket( PF_IPX, SOCK_DGRAM, NSPROTO_IPX ) ) == -1 )
	{
		err = WSAGetLastError();
		if ( err != WSAEAFNOSUPPORT )
			Con_Printf( "WARNING: IPX_Socket: port: %d  socket: %s\n", port, NET_ErrorString( err ) );
		return 0;
	}

	// make it non-blocking
	if ( ioctlsocket( newsocket, FIONBIO, &_true ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: IPX_Socket: port: %d  ioctl FIONBIO: %s\n", port, NET_ErrorString( err ) );
		return 0;
	}

	// make it broadcast capable
	if ( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof( _true ) ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: IPX_Socket: port: %d  setsockopt SO_BROADCAST: %s\n", port, NET_ErrorString( err ) );
		return 0;
	}

	if ( setsockopt( newsocket, SOL_SOCKET, SO_REUSEADDR, (char *)&_true, sizeof( _true ) ) == -1 )
	{
		err = WSAGetLastError();
		return 0;
	}

	address.sa_family = AF_IPX;
	Q_memset( address.sa_netnum, 0, 4 );
	Q_memset( address.sa_nodenum, 0, 6 );
	if ( port == PORT_ANY )
		address.sa_socket = 0;
	else
		address.sa_socket = htons( (short)port );

	if ( bind( newsocket, (void *)&address, sizeof( address ) ) == -1 )
	{
		err = WSAGetLastError();
		Con_Printf( "WARNING: IPX_Socket: port: %d  bind: %s\n", port, NET_ErrorString( err ) );
		closesocket( newsocket );
		return 0;
	}

	return newsocket;
}

/*
====================
NET_OpenIPX_guess
====================
*/
void NET_OpenIPX_guess( void )
{
	int		port;
	int		dedicated;

	dedicated = cls.state == ca_dedicated;

	NET_ThreadLock();

	if ( !ipx_sockets[NS_SERVER] )
	{
		port = ipx_hostport.value;
		if ( !port )
		{
			port = hostport.value;
			if ( !port )
			{
				hostport.value = defport.value;
				port = defport.value;
			}
		}

		ipx_sockets[NS_SERVER] = NET_IPXSocket_guess( port );
	}

	NET_ThreadUnlock();

	// dedicated servers don't need client ports
	if ( dedicated )
		return;

	NET_ThreadLock();

	if ( !ipx_sockets[NS_CLIENT] )
	{
		port = ipx_clientport.value;
		if ( !port )
		{
			port = clientport.value;
			if ( !port )
				port = PORT_ANY;
		}

		ipx_sockets[NS_CLIENT] = NET_IPXSocket_guess( port );
		if ( !ipx_sockets[NS_CLIENT] )
			ipx_sockets[NS_CLIENT] = NET_IPXSocket_guess( PORT_ANY );
	}

	NET_ThreadUnlock();
}

void NET_GetLocalAddress( void )
{
	char	buff[512];
	struct sockaddr_in	address;
	struct sockaddr_ipx	ipxaddress_guess;
	int		namelen;
	int		net_error = 0;

	Q_memset( &net_local_ipx_adr, 0, sizeof( netadr_t ) );
	Q_memset( &net_local_adr, 0, sizeof( netadr_t ) );

	if ( noip )
	{
		Con_Printf( "TCP/IP Disabled.\n" );
	}
	else
	{
		if ( Q_strcmp( ipname.string, "localhost" ) )
		{
			Q_strncpy( buff, ipname.string, sizeof( buff ) - 1 );
			buff[sizeof( buff ) - 1] = 0;
		}
		else
		{
			gethostname( buff, sizeof( buff ) );
		}

		buff[sizeof( buff ) - 1] = 0;

		NET_StringToAdr( buff, &net_local_adr );

		namelen = sizeof( address );
		if ( getsockname( ip_sockets[NS_SERVER], (struct sockaddr *)&address, &namelen ) != 0 )
		{
			net_error = WSAGetLastError();
			noip = true;
			Con_Printf( "Could not get TCP/IP address, TCP/IP disabled\nReason:  %s\n", NET_ErrorString( net_error ) );
		}
		else
		{
			net_local_adr.port = address.sin_port;
			Con_Printf( "Server IP address %s\n", NET_AdrToString( net_local_adr ) );
			Cvar_Set( "net_address", va( "%s", NET_AdrToString( net_local_adr ) ) );
		}
	}

	if ( noipx )
	{
		Con_Printf( "No IPX Support.\n" );
	}
	else
	{
		namelen = sizeof( ipxaddress_guess );
		if ( getsockname( ipx_sockets[NS_SERVER], (struct sockaddr *)&ipxaddress_guess, &namelen ) != 0 )
		{
			net_error = WSAGetLastError();
			noipx = true;
		}
		else
		{
			SockadrToNetadr( (struct sockaddr *)&ipxaddress_guess, &net_local_ipx_adr );
			Con_Printf( "Server IPX address %s\n", NET_AdrToString( net_local_ipx_adr ) );
		}
	}
}

static int	net_configured;

int NET_IsConfigured( void )
{
	return net_configured;
}

/*
====================
NET_Config

A single player game will only use the loopback code
====================
*/
void NET_Config( qboolean multiplayer )
{
	int		i;
	static qboolean	old_config;
	static qboolean	bFirst = true;

	if ( old_config == multiplayer )
		return;

	old_config = multiplayer;

	if ( !multiplayer )
	{	// shut down any existing sockets
		NET_ThreadLock();

		for ( i = 0; i < 3; i++ )
		{
			if ( ip_sockets[i] )
			{
				closesocket( ip_sockets[i] );
				ip_sockets[i] = 0;
			}

			if ( ipx_sockets[i] )
			{
				closesocket( ipx_sockets[i] );
				ipx_sockets[i] = 0;
			}
		}

		NET_ThreadUnlock();
	}
	else
	{	// open sockets
		if ( !noip )
			NET_OpenIP();

		if ( !noipx )
			NET_OpenIPX_guess();

		if ( bFirst )
		{
			bFirst = false;
			NET_GetLocalAddress();
		}
	}

	net_configured = multiplayer != 0;
}

void NET_Init( void )
{
	int		i;

	NET_InitColors();

	Cmd_AddCommand( "maxplayers", MaxPlayers_f );

	Cvar_RegisterVariable( &net_address );
	Cvar_RegisterVariable( &ipname );
	Cvar_RegisterVariable( &iphostport );
	Cvar_RegisterVariable( &hostport );
	Cvar_RegisterVariable( &defport );
	Cvar_RegisterVariable( &ip_clientport );
	Cvar_RegisterVariable( &clientport );
	Cvar_RegisterVariable( &clockwindow );
	Cvar_RegisterVariable( &multicastport );
	Cvar_RegisterVariable( &ipx_hostport );
	Cvar_RegisterVariable( &ipx_clientport );
	Cvar_RegisterVariable( &fakelag );
	Cvar_RegisterVariable( &fakeloss );
	Cvar_RegisterVariable( &net_graph );
	Cvar_RegisterVariable( &net_graphwidth );
	Cvar_RegisterVariable( &net_scale );
	Cvar_RegisterVariable( &net_graphpos );

	if ( COM_CheckParm( "-netthread" ) )
		use_thread = true;

	if ( COM_CheckParm( "-netsleep" ) )
		net_sleepforever = 0;

	if ( COM_CheckParm( "-noipx" ) )
		noipx = true;

	if ( COM_CheckParm( "-noip" ) )
		noip = true;

	i = COM_CheckParm( "-port" );
	if ( i )
		Cvar_SetValue( "hostport", (float)atof( com_argv[i + 1] ) );

	i = COM_CheckParm( "-clockwindow" );
	if ( i )
		Cvar_SetValue( "clockwindow", (float)atof( com_argv[i + 1] ) );

	net_message.maxsize = sizeof( net_message_buffer );
	net_message.data = net_message_buffer;
	net_message.flags = 0;
	net_message.buffername = "net_message";

	in_message.maxsize = sizeof( in_message_buf );
	in_message.data = in_message_buf;
	in_message.flags = 0;
	in_message.buffername = "in_message";

	for ( i = 0; i < 3; i++ )
	{
		g_pLagData[i].pPrev = &g_pLagData[i];
		g_pLagData[i].pNext = g_pLagData[i].pPrev;
	}

	NET_AllocateQueues();

	Con_DPrintf( "Base networking initialized.\n" );
}

static void MaxPlayers_f( void )
{
	int		n;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "\"maxplayers\" is \"%u\"\n", svs.maxclients );
		return;
	}

	if ( sv.active )
	{
		Con_Printf( "maxplayers cannot be changed while a server is running.\n" );
		return;
	}

	n = Q_atoi( Cmd_Argv( 1 ) );
	if ( n < 1 )
		n = 1;

	if ( n > svs.maxclientslimit )
	{
		n = svs.maxclientslimit;
		Con_Printf( "\"maxplayers\" set to \"%u\"\n", n );
	}

	svs.maxclients = n;

	if ( n == 1 )
		Cvar_Set( "deathmatch", "0" );
	else
		Cvar_Set( "deathmatch", "1" );
}

void NET_ClearLagData( qboolean bClient, qboolean bServer )
{
	NET_ThreadLock();

	if ( bClient )
	{
		NET_ClearLaggedList( &g_pLagData[NS_CLIENT] );
		NET_ClearLaggedList( &g_pLagData[NS_MULTICAST] );
	}

	if ( bServer )
		NET_ClearLaggedList( &g_pLagData[NS_SERVER] );

	NET_ThreadUnlock();
}

void NET_Shutdown( void )
{
	NET_ThreadLock();

	NET_ClearLaggedList( &g_pLagData[NS_CLIENT] );
	NET_ClearLaggedList( &g_pLagData[NS_SERVER] );

	NET_ThreadUnlock();

	NET_Config( false );
	NET_FlushQueues();
}

qboolean NET_JoinGroup( netsrc_t sock, netadr_t addr )
{
	int		net_socket;
	int		err;
	struct ip_mreq	mreq;

	net_socket = ip_sockets[sock];

	mreq.imr_multiaddr.s_addr = *(int *)&addr.ip[0];
	mreq.imr_interface.s_addr = INADDR_ANY;

	if ( setsockopt( net_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof( mreq ) ) == -1 )
	{
		err = WSAGetLastError();
		if ( err != WSAEAFNOSUPPORT )
			Con_Printf( "WARNING: NET_JoinGroup: IP_ADD_MEMBERSHIP: %s", NET_ErrorString( err ) );
		return false;
	}

	return true;
}

qboolean NET_LeaveGroup( netsrc_t sock, netadr_t addr )
{
	int		net_socket;
	int		err;
	struct ip_mreq	mreq;

	net_socket = ip_sockets[sock];

	mreq.imr_multiaddr.s_addr = *(int *)&addr.ip[0];
	mreq.imr_interface.s_addr = INADDR_ANY;

	if ( setsockopt( net_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char *)&mreq, sizeof( mreq ) ) == -1 )
	{
		err = WSAGetLastError();
		if ( err != WSAEAFNOSUPPORT )
			return false;
	}

	return true;
}

//=============================================================================
// net graph

typedef struct
{
	unsigned short	client;
	unsigned short	players;
	unsigned short	entities;	// entities bytes, except for players
	unsigned short	tentities;	// temp entities
	unsigned short	sound;
	unsigned short	event;
	unsigned short	usr;
	unsigned short	msgbytes;
	unsigned short	voicebytes;
} netbandwidthgraph_t;

typedef struct
{
	int		latency;
	int		choked;
} packet_latency_t;

typedef struct
{
	byte	color[3];
	byte	alpha;
} netcolor_t;

static netcolor_t netcolors[5] =
{
	{ 255, 0, 0, 255 },
	{ 0, 0, 255, 255 },
	{ 240, 127, 63, 255 },
	{ 255, 255, 0, 255 },
	{ 63, 255, 63, 150 }
};

typedef struct
{
	float		cmd_lerp;
	int			size;
	qboolean	sent;
} cmdinfo_t;

#define	TIMINGS		1024	// Number of values to track (must be power of 2) b/c of masking
#define LERP_HEIGHT	24

static byte	colors[LERP_HEIGHT][3];

static byte	unusedcolor_guess[3] = { 0, 192, 255 };
static byte	holdcolor[3] = { 255, 0, 0 };
static byte	extrap_base_color[3] = { 255, 255, 255 };

void NET_FillRect( vrect_t *rect, byte *color, byte alpha )
{
	Draw_FillRGBA( rect->x, rect->y, rect->width, rect->height, color[0], color[1], color[2], alpha );
}

void NET_DrawString( int x, int y, int font, float r, float g, float b, char *fmt, ... )
{
	va_list		argptr;
	static char	string[1024];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Draw_SetTextColor( r, g, b );
	Draw_String( x, y, string );
}

void NET_GetColorValues( int color, byte *cv, byte *alpha )
{
	int			i;
	netcolor_t	*pc = &netcolors[color];

	for ( i = 0; i < 3; i++ )
		cv[i] = pc->color[i];

	*alpha = pc->alpha;
}

void NET_ColorForHeight( packet_latency_t *packet, byte *color, int *ping, byte *alpha )
{
	int		h = packet->latency;

	*ping = 0;

	switch ( h )
	{
	case 9999:
		NET_GetColorValues( 0, color, alpha );
		break;
	case 9998:
		NET_GetColorValues( 1, color, alpha );
		break;
	case 9997:
		NET_GetColorValues( 2, color, alpha );
		break;
	default:
		*ping = 1;
		NET_GetColorValues( packet->choked ? 3 : 4, color, alpha );
		break;
	}
}

void NET_InitColors( void )
{
	int		i, j;
	byte	mincolor[2][3];
	byte	maxcolor[2][3];
	float	dc[2][3];
	int		hfrac;
	float	f;

	mincolor[0][0] = 63;
	mincolor[0][1] = 0;
	mincolor[0][2] = 100;

	maxcolor[0][0] = 0;
	maxcolor[0][1] = 63;
	maxcolor[0][2] = 255;

	mincolor[1][0] = 255;
	mincolor[1][1] = 127;
	mincolor[1][2] = 0;

	maxcolor[1][0] = 250;
	maxcolor[1][1] = 0;
	maxcolor[1][2] = 0;

	for ( i = 0; i < 3; i++ )
	{
		dc[0][i] = (float)( maxcolor[0][i] - mincolor[0][i] );
		dc[1][i] = (float)( maxcolor[1][i] - mincolor[1][i] );
	}

	hfrac = LERP_HEIGHT / 3;

	for ( i = 0; i < LERP_HEIGHT; i++ )
	{
		if ( i < hfrac )
		{
			f = (float)i / (float)hfrac;
			for ( j = 0; j < 3; j++ )
				colors[i][j] = mincolor[0][j] + f * dc[0][j];
		}
		else
		{
			f = (float)( i - hfrac ) / (float)( LERP_HEIGHT - hfrac );
			for ( j = 0; j < 3; j++ )
				colors[i][j] = mincolor[1][j] + f * dc[1][j];
		}
	}
}

qboolean NET_AtEdge( int x, int width )
{
	if ( x <= 3 )
		return true;

	if ( x >= width - 4 )
		return true;

	return false;
}

void NET_DrawTimes( vrect_t vrect, cmdinfo_t *cmdinfo, int x, int w )
{
	int		i;
	int		j;
	int		extrap_point;
	int		a, h;
	vrect_t	rcFill;
	int		ptx, pty;

	ptx = max( x + w - 26, 1 );
	pty = max( vrect.y + vrect.height - 27, 1 );

	NET_DrawString( ptx, pty, 1, 0.9, 0.9, 0.7, "%i/s", (int)cl_cmdrate.value );

	extrap_point = LERP_HEIGHT / 3;

	for ( a = 0; a < w; a++ )
	{
		i = ( cls.netchan.outgoing_sequence - a ) & ( TIMINGS - 1 );
		h = min( ( cmdinfo[i].cmd_lerp / 3.0 ) * LERP_HEIGHT, LERP_HEIGHT );

		rcFill.x = x + w - a - 1;
		rcFill.width = 1;
		rcFill.height = 1;

		rcFill.y = vrect.y + vrect.height - 4;

		if ( h >= extrap_point )
		{
			int start = 0;

			h -= extrap_point;
			rcFill.y -= extrap_point;
			rcFill.y -= ( h - 1 );

			start = h - 1;

			for ( j = start; j < h; j++ )
			{
				NET_FillRect( &rcFill, colors[j + extrap_point], 255 );
				rcFill.y--;
			}
		}
		else
		{
			int oldh;
			int start = 0;

			oldh = h;
			rcFill.y -= h;
			h = 1;

			for ( j = 0; j < h; j++ )
			{
				NET_FillRect( &rcFill, colors[j + oldh], 255 );
				rcFill.y--;
			}
		}

		rcFill.y = vrect.y + vrect.height - 4 - extrap_point;

		if ( NET_AtEdge( a, w ) )
			NET_FillRect( &rcFill, extrap_base_color, 255 );

		rcFill.y = vrect.y + vrect.height - 3;

		if ( !cmdinfo[i].sent )
			NET_FillRect( &rcFill, holdcolor, 200 );
	}
}

void NET_GetFrameData( packet_latency_t *packet_latency, netbandwidthgraph_t *graph, int *choke_count, int *loss_count, int *biggest_message, float *latency, int *latency_count )
{
	int		i;
	int		frm;
	frame_t	*frame;
	float	frame_latency;

	*choke_count = 0;
	*loss_count = 0;
	*biggest_message = 0;
	*latency = 0;
	*latency_count = 0;

	for ( i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.incoming_sequence; i++ )
	{
		frm = i;
		frame = &cl.frames[frm & CL_UPDATE_MASK];

		packet_latency[frm & ( TIMINGS - 1 )].choked = frame->choked;
		if ( frame->choked )
			( *choke_count )++;

		if ( frame->invalid )
		{
			packet_latency[frm & ( TIMINGS - 1 )].latency = 9998;	// invalid delta
		}
		else if ( frame->receivedtime == -1 )
		{
			packet_latency[frm & ( TIMINGS - 1 )].latency = 9999;	// dropped
			( *loss_count )++;
		}
		else if ( frame->receivedtime == -3 )
		{
			packet_latency[frm & ( TIMINGS - 1 )].latency = 9997;	// skipped
		}
		else
		{
			frame_latency = min( 1.0, frame->latency );
			packet_latency[frm & ( TIMINGS - 1 )].latency = ( ( frame_latency + 0.1 ) / 1.1 ) * ( scr_graphheight.value - LERP_HEIGHT - 2 );

			if ( i > ( cls.netchan.incoming_sequence - 8 ) )
			{
				*latency += frame->latency * 1000.0;
				( *latency_count )++;
			}
		}

		graph[frm & ( TIMINGS - 1 )].client = frame->clientbytes;
		graph[frm & ( TIMINGS - 1 )].players = frame->playerinfobytes;
		graph[frm & ( TIMINGS - 1 )].entities = frame->packetentitybytes;
		graph[frm & ( TIMINGS - 1 )].tentities = frame->tentitybytes;
		graph[frm & ( TIMINGS - 1 )].sound = frame->soundbytes;
		graph[frm & ( TIMINGS - 1 )].event = frame->eventbytes;
		graph[frm & ( TIMINGS - 1 )].usr = frame->usrbytes;
		graph[frm & ( TIMINGS - 1 )].msgbytes = frame->msgbytes;
		graph[frm & ( TIMINGS - 1 )].voicebytes = frame->voicebytes;

		if ( frame->msgbytes > *biggest_message )
			*biggest_message = frame->msgbytes;
	}

	if ( *biggest_message > 1000 )
		*biggest_message = 1000;
}

void NET_GetCommandInfo( cmdinfo_t *cmdinfo )
{
	int		i;
	cmd_t	*cmd;

	for ( i = cls.netchan.outgoing_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.outgoing_sequence; i++ )
	{
		cmd = &cl.commands[i & CL_UPDATE_MASK];

		cmdinfo[i & ( TIMINGS - 1 )].cmd_lerp = cmd->frame_lerp;
		cmdinfo[i & ( TIMINGS - 1 )].sent = !cmd->heldback;
		cmdinfo[i & ( TIMINGS - 1 )].size = cmd->sendsize;
	}
}

void NET_DrawTextFields( int graphvalue, int x, int y, netbandwidthgraph_t *graph, cmdinfo_t *cmdinfo, int count, float avg, float *framerate, int packet_loss, int packet_choke )
{
	static int	lastout;
	char	sz[256];
	int		out;

	if ( count > 0 )
	{
		avg /= count;
		avg -= host_frametime / 2.0;

		if ( cl_updaterate.value > 0 )
			avg -= (int)( 1000 / cl_updaterate.value );

		avg = max( 0.0, avg );
	}
	else
	{
		avg = 0.0;
	}

	*framerate = *framerate * 0.5 + 0.5 * host_frametime;

	y -= scr_graphheight.value;

	if ( *framerate > 0.0 )
	{
		_snprintf( sz, sizeof( sz ), "%.1f fps", 1.0 / *framerate );
		NET_DrawString( x, y, 1, 0.9, 0.9, 0.7, sz );

		if ( avg > 1.0 )
		{
			_snprintf( sz, sizeof( sz ), "%i ms ", (int)avg );
			NET_DrawString( x + 75, y, 1, 0.9, 0.9, 0.7, sz );
		}

		y += 15;

		out = cmdinfo[( cls.netchan.outgoing_sequence - 1 ) & ( TIMINGS - 1 )].size;
		if ( !out )
			out = lastout;
		else
			lastout = out;

		_snprintf( sz, sizeof( sz ), "in :  %i %.2f k/s", graph[cls.netchan.incoming_sequence & ( TIMINGS - 1 )].msgbytes, cls.netchan.flow[FLOW_INCOMING].avgkbytespersec );
		NET_DrawString( x, y, 1, 0.9, 0.9, 0.7, sz );

		y += 15;

		_snprintf( sz, sizeof( sz ), "out:  %i %.2f k/s", out, cls.netchan.flow[FLOW_OUTGOING].avgkbytespersec );
		NET_DrawString( x, y, 1, 0.9, 0.9, 0.7, sz );

		y += 15;

		if ( graphvalue > 2 )
		{
			_snprintf( sz, sizeof( sz ), "loss: %i choke:  %i", (int)( packet_loss + 0.49 ), (int)( packet_choke + 0.49 ) );
			NET_DrawString( x, y, 1, 0.9, 0.9, 0.7, sz );

			y += 15;
		}
	}
}

int NET_GraphValue( void )
{
	int			graphtype;
	kbutton_t	*pgraphkey;

	graphtype = net_graph.value;
	pgraphkey = ClientDLL_FindKey( "in_graph" );

	if ( !graphtype && !( pgraphkey && ( pgraphkey->state & 1 ) ) )
		return 0;

	// With +graph key, use max area
	if ( !graphtype )
		graphtype = 2;

	return graphtype;
}

void NET_GraphGetXY( vrect_t *rect, int width, int *x, int *y )
{
	*x = rect->x + 5;

	switch ( (int)net_graphpos.value )
	{
	case 1:
		*x = rect->x + rect->width - 5 - width;
		break;
	case 2:
		*x = rect->x + ( rect->width - 10 - width ) / 2;
		break;
	}

	*y = rect->y + rect->height - LERP_HEIGHT - 5;
}

void NET_DrawHatches( int x, int y, int maxmsgbytes )
{
	int		starty;
	int		ystep;
	vrect_t	rcHatch;
	byte	colorminor[3];
	byte	color[3];

	ystep = (int)( 10.0 / net_scale.value );
	ystep = max( ystep, 1 );

	rcHatch.y = y;
	rcHatch.height = 1;
	rcHatch.x = x;
	rcHatch.width = 3;

	color[0] = 0;
	color[1] = 200;
	color[2] = 0;

	colorminor[0] = 63;
	colorminor[1] = 63;
	colorminor[2] = 0;

	for ( starty = rcHatch.y; rcHatch.y > 0 && ( ( starty - rcHatch.y ) * net_scale.value < ( maxmsgbytes + 50 ) ); rcHatch.y -= ystep )
	{
		if ( !( (int)( ( starty - rcHatch.y ) * net_scale.value ) % 50 ) )
		{
			NET_FillRect( &rcHatch, color, 255 );
		}
		else if ( ystep > 5 )
		{
			NET_FillRect( &rcHatch, colorminor, 200 );
		}
	}
}

void NET_DrawUpdateRate( int x, int y )
{
	NET_DrawString( x, y, 1, 0.9, 0.9, 0.7, "%i/s", (int)cl_updaterate.value );
}

int NET_DrawDataSegment( vrect_t *rcFill, int bytes, byte r, byte g, byte b, byte alpha )
{
	int		h;
	byte	color[3];

	h = bytes / net_scale.value;

	color[0] = r;
	color[1] = g;
	color[2] = b;

	rcFill->height = 1;
	rcFill->y -= h;

	if ( rcFill->y < 2 )
		return 0;

	NET_FillRect( rcFill, color, alpha );

	return 1;
}

static packet_latency_t		packet_latency[TIMINGS];
static cmdinfo_t			cmdinfo[TIMINGS];
static netbandwidthgraph_t	graph[TIMINGS];

static float	framerate = 0.0;
static float	packet_loss = 0.0;
static float	packet_choke = 0.0;

void SCR_NetGraph( void )
{
	int		graphtype;
	int		a, x, y, w, h, i;
	byte	color[3];
	byte	alpha;
	vrect_t	rcFill;
	vrect_t	vrect;
	int		lastvalidh;
	int		ping;
	int		maxmsgbytes = 0;
	float	loss = 0.0;
	float	choke = 0.0;
	int		loss_count = 0;
	int		choke_count = 0;
	float	avg_ping = 0.0;
	int		ping_count = 0;

	graphtype = NET_GraphValue();
	if ( !graphtype )
		return;

	// Since we divide by scale, make sure it's sensible
	if ( net_scale.value <= 0 )
		net_scale.value = 1.0;

	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.width;
	vrect.height = vid.height;

	w = min( TIMINGS, (int)net_graphwidth.value );
	if ( vrect.width < w + 10 )
		w = vrect.width - 10;

	// Get latency data
	NET_GetFrameData( packet_latency, graph, &choke_count, &loss_count, &maxmsgbytes, &avg_ping, &ping_count );

	// Packet loss
	loss = loss_count * 100.0 / CL_UPDATE_BACKUP;
	packet_loss = packet_loss * 0.5 + loss * 0.5;

	// Packet choke
	choke = choke_count * 100.0 / CL_UPDATE_BACKUP;
	packet_choke = packet_choke * 0.5 + choke * 0.5;

	// Get command data
	NET_GetCommandInfo( cmdinfo );

	NET_GraphGetXY( &vrect, w, &x, &y );

	if ( graphtype < 3 )
	{
		lastvalidh = 0;

		for ( a = 0; a < w; a++ )
		{
			i = ( cls.netchan.incoming_sequence - a ) & ( TIMINGS - 1 );
			h = packet_latency[i].latency;

			NET_ColorForHeight( &packet_latency[i], color, &ping, &alpha );

			// Skip invalid
			if ( !ping )
				h = lastvalidh;
			else
				lastvalidh = h;

			if ( h > ( scr_graphheight.value - LERP_HEIGHT - 2 ) )
				h = scr_graphheight.value - LERP_HEIGHT - 2;

			rcFill.x = x + w - a - 1;
			rcFill.y = y - h;
			rcFill.width = 1;

			if ( ping )
			{
				rcFill.height = 1;
				NET_FillRect( &rcFill, color, alpha );
			}
			else
			{
				rcFill.height = h;
				if ( h < 4 )
				{
					NET_FillRect( &rcFill, color, alpha );
				}
				else
				{
					rcFill.height = 2;
					NET_FillRect( &rcFill, color, alpha );

					rcFill.y += h - 2;
					NET_FillRect( &rcFill, color, alpha );
				}
			}

			rcFill.y = y;
			rcFill.height = 1;

			color[0] = 0;
			color[1] = 255;
			color[2] = 0;

			if ( NET_AtEdge( a, w ) )
				NET_FillRect( &rcFill, color, 160 );

			if ( graphtype < 2 )
				continue;

			// Draw a separator.
			rcFill.y = y - scr_graphheight.value - 1;
			rcFill.height = 1;

			color[0] = 255;
			color[1] = 255;
			color[2] = 255;

			if ( NET_AtEdge( a, w ) )
				NET_FillRect( &rcFill, color, 255 );

			// Move up for begining of data
			rcFill.y -= 1;

			// Packet didn't have any real data...
			if ( packet_latency[i].latency > 9995 )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].client, 255, 0, 0, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].players, 255, 255, 0, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].entities, 255, 0, 255, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].tentities, 0, 0, 255, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].sound, 0, 255, 0, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].event, 0, 255, 255, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].usr, 200, 200, 200, 128 ) )
				continue;

			if ( !NET_DrawDataSegment( &rcFill, graph[i].voicebytes, 255, 255, 255, 255 ) )
				continue;

			// Final data chunk is total size, don't use solid line routine for this
			h = graph[i].msgbytes / net_scale.value;

			color[0] = color[1] = color[2] = 240;

			rcFill.height = 1;
			rcFill.y = y - scr_graphheight.value - 1 - h;

			if ( rcFill.y < 2 )
				continue;

			NET_FillRect( &rcFill, color, 128 );
		}

		if ( graphtype >= 2 )
		{
			// Draw hatches for first one:
			// on the far right side
			NET_DrawHatches( x, y - scr_graphheight.value - 1, maxmsgbytes );
		}

		NET_DrawUpdateRate( max( 1, rcFill.x + w - 25 ), max( 1, y - scr_graphheight.value ) );

		NET_DrawTimes( vrect, cmdinfo, x, w );
	}

	NET_DrawTextFields( graphtype, x, y, graph, cmdinfo, ping_count, avg_ping, &framerate, packet_loss, packet_choke );
}

void SCR_DrawFPS( void )
{
	if ( !cl_showfps.value || host_frametime <= 0.0 )
		return;

	NET_DrawString( 2, 2, 0, 1.0, 1.0, 1.0, "%i fps", (int)( 1.0 / host_frametime ) );
}
