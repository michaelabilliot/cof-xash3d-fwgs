// net_api.c -- network services exported to the client dll

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "host.h"
#include "client.h"
#include "net.h"
#include "net_api.h"
#include "info.h"
#include "mem.h"

extern	cvar_t	rate;
extern	double	g_flLatency;

typedef struct net_api_query_s
{
	struct net_api_query_s	*next;
	int						context;
	int						type;
	int						flags;
	double					requesttime;
	double					timeout;
	netadr_t				request;
	net_api_response_func_t	callback;
} net_api_query_t;

static net_api_query_t	*g_queries;

/*
=================
Net_APIClearQueries

=================
*/
void Net_APIClearQueries( void )
{
	net_api_query_t *p, *n;

	p = g_queries;
	while ( p )
	{
		n = p->next;
		Mem_Free( p );
		p = n;
	}

	g_queries = NULL;
}

static net_adrlist_t	*g_addresses;

/*
=================
Net_KillServerList

=================
*/
void Net_KillServerList( void )
{
	net_adrlist_t *p, *n;

	p = g_addresses;
	while ( p )
	{
		n = p->next;
		Mem_Free( p );
		p = n;
	}

	g_addresses = NULL;
}

/*
=================
Net_APIInit

=================
*/
void Net_APIInit( void )
{
	g_queries = NULL;
	g_addresses = NULL;
}

/*
=================
Net_APIShutDown

=================
*/
void Net_APIShutDown( void )
{
	Net_APIClearQueries();
	Net_KillServerList();
}

/*
=================
Net_APIFind

=================
*/
net_api_query_t *Net_APIFind( int type, netadr_t *remote )
{
	net_api_query_t *p;

	p = g_queries;
	while ( p )
	{
		if ( p->type == type )
		{
			if ( p->request.type == NA_BROADCAST &&
				( remote->type == NA_BROADCAST || remote->type == NA_IP ) )
				return p;

			if ( p->request.type == NA_BROADCAST_IPX &&
				( remote->type == NA_BROADCAST_IPX || remote->type == NA_IPX ) )
				return p;

			if ( NET_CompareAdr( *remote, p->request ) )
				return p;
		}

		p = p->next;
	}

	return NULL;
}

/*
=================
Net_APIFindContext

=================
*/
net_api_query_t *Net_APIFindContext( int context )
{
	net_api_query_t *p;

	p = g_queries;
	while ( p )
	{
		if ( context == p->context )
			return p;

		p = p->next;
	}

	return NULL;
}

/*
=================
Net_APIFailed

=================
*/
void Net_APIFailed( net_api_query_t *p )
{
	net_response_t r;

	if ( !p->callback )
		return;

	r.context = p->context;
	r.type = p->type;
	r.error = NET_ERROR_TIMEOUT;
	r.remote_address = p->request;
	r.response = NULL;
	r.ping = realtime - p->requesttime;

	p->callback( &r );
}

/*
=================
Net_APIKill

=================
*/
void Net_APIKill( net_api_query_t *kill )
{
	net_api_query_t *p, *n;
	net_api_query_t *newlist = NULL;

	if ( !kill )
		return;

	p = g_queries;
	while ( p )
	{
		n = p->next;
		if ( p == kill )
		{
			Mem_Free( p );
		}
		else
		{
			p->next = newlist;
			newlist = p;
		}
		p = n;
	}

	g_queries = newlist;
}

/*
=================
Net_APICheckTimeouts

=================
*/
void Net_APICheckTimeouts( void )
{
	net_api_query_t *p, *n;
	net_api_query_t *newlist = NULL;

	p = g_queries;
	while ( p )
	{
		n = p->next;
		if ( p->requesttime + p->timeout < realtime )
		{
			Net_APIFailed( p );
			Mem_Free( p );
		}
		else
		{
			p->next = newlist;
			newlist = p;
		}
		p = n;
	}

	g_queries = newlist;
}

/*
=================
Net_GetBatchServerList

=================
*/
void Net_GetBatchServerList( sizebuf_t *msg, int batch )
{
	MSG_WriteByte( msg, A2M_GET_SERVERS_BATCH );
	MSG_WriteLong( msg, batch );
}

/*
=================
Net_SendRequest

=================
*/
void Net_SendRequest( int context, int request, int flags, double timeout, struct netadr_s *remote_address, net_api_response_func_t response )
{
	unsigned char data[1024];
	sizebuf_t msg;
	net_api_query_t *q;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Net_SendRequest";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	q = (net_api_query_t *)Mem_ZeroMalloc( sizeof( net_api_query_t ) );
	q->context = context;
	q->type = request;
	q->flags = flags;
	q->requesttime = realtime;
	q->timeout = timeout;
	q->request = *remote_address;
	q->callback = response;

	q->next = g_queries;
	g_queries = q;

	switch ( request )
	{
	case NETAPI_REQUEST_SERVERLIST:
		Net_KillServerList();
		Net_GetBatchServerList( &msg, 0 );
		break;
	case NETAPI_REQUEST_PING:
		MSG_WriteLong( &msg, -1 );
		MSG_WriteString( &msg, "ping\n" );
		break;
	case NETAPI_REQUEST_RULES:
		MSG_WriteLong( &msg, -1 );
		MSG_WriteString( &msg, "rules\n" );
		break;
	case NETAPI_REQUEST_PLAYERS:
		MSG_WriteLong( &msg, -1 );
		MSG_WriteString( &msg, "players\n" );
		break;
	case NETAPI_REQUEST_DETAILS:
		MSG_WriteLong( &msg, -1 );
		MSG_WriteString( &msg, "details\n" );
		break;
	default:
		Con_Printf( "Unknown request type:  %i\n", request );
		break;
	}

	if ( msg.cursize > 0 )
	{
		NET_SendPacket( NS_CLIENT, msg.cursize, msg.data, q->request );
	}
}

/*
=================
NET_ParsePlayersResponse

=================
*/
void NET_ParsePlayersResponse( char *returninfo )
{
	int i;
	char *s;
	int n;
	int index;
	int frags;
	float time;
	char sz[256];

	returninfo[0] = 0;

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	n = MSG_ReadByte();

	Info_SetValueForKey( returninfo, "players", va( "%i", n ), 2048 );

	for ( i = 0; i < n; i++ )
	{
		index = MSG_ReadByte();
		s = MSG_ReadString();
		if ( !s )
			break;

		_snprintf( sz, sizeof( sz ), "%s", s );
		Info_SetValueForKey( returninfo, va( "p%iname", index ), sz, 2048 );

		frags = MSG_ReadLong();
		_snprintf( sz, sizeof( sz ), "%i", frags );
		Info_SetValueForKey( returninfo, va( "p%ifrags", index ), sz, 2048 );

		time = MSG_ReadFloat();
		_snprintf( sz, sizeof( sz ), "%.2f", time );
		Info_SetValueForKey( returninfo, va( "p%itime", index ), sz, 2048 );
	}
}

/*
=================
NET_ParseRulesResponse

=================
*/
void NET_ParseRulesResponse( char *returninfo )
{
	int n;
	int i;
	char *key, *val;
	char szkey[64], szval[1024];

	returninfo[0] = 0;

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	n = MSG_ReadShort();

	Info_SetValueForKey( returninfo, "rules", va( "%i", n ), 2048 );

	for ( i = 0; i < n; i++ )
	{
		key = MSG_ReadString();
		if ( !key || !key[0] )
			break;

		Q_strncpy( szkey, key, sizeof( szkey ) - 1 );
		szkey[sizeof( szkey ) - 1] = 0;

		val = MSG_ReadString();
		if ( !val )
			break;

		Q_strncpy( szval, val, sizeof( szval ) - 1 );
		szval[sizeof( szval ) - 1] = 0;

		Info_SetValueForKey( returninfo, szkey, szval, 2048 );
	}
}

/*
=================
NET_ParseServerInfoResponse

=================
*/
void NET_ParseServerInfoResponse( char *returninfo )
{
	char *s;
	int n;
	char c;

	returninfo[0] = 0;

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();

	// server address
	s = MSG_ReadString();

	s = MSG_ReadString();
	Info_SetValueForKey( returninfo, "hostname", s, 2048 );
	s = MSG_ReadString();
	Info_SetValueForKey( returninfo, "map", s, 2048 );
	s = MSG_ReadString();
	Info_SetValueForKey( returninfo, "gamedir", s, 2048 );
	s = MSG_ReadString();
	Info_SetValueForKey( returninfo, "gamedesc", s, 2048 );

	n = MSG_ReadLong();
	Info_SetValueForKey( returninfo, "current", va( "%i", n ), 2048 );
	n = MSG_ReadLong();
	Info_SetValueForKey( returninfo, "max", va( "%i", n ), 2048 );
	n = MSG_ReadByte();
	Info_SetValueForKey( returninfo, "protocol", va( "%i", n ), 2048 );

	c = MSG_ReadByte();
	Info_SetValueForKey( returninfo, "type", va( "%c", c ), 2048 );
	c = MSG_ReadByte();
	Info_SetValueForKey( returninfo, "os", va( "%c", c ), 2048 );

	c = MSG_ReadByte() ? true : false;
	Info_SetValueForKey( returninfo, "pw", c ? "Y" : "N", 2048 );
	c = MSG_ReadByte() ? true : false;
	Info_SetValueForKey( returninfo, "secure", c ? "1" : "0", 2048 );
	c = MSG_ReadByte() ? true : false;
	Info_SetValueForKey( returninfo, "mod", c ? "Y" : "N", 2048 );

	if ( c )
	{
		s = MSG_ReadString();
		Info_SetValueForKey( returninfo, "mod_url", s, 2048 );
		s = MSG_ReadString();
		Info_SetValueForKey( returninfo, "mod_dl", s, 2048 );
		// hlversion
		s = MSG_ReadString();
		n = MSG_ReadLong();
		Info_SetValueForKey( returninfo, "mod_ver", va( "%i", n ), 2048 );
		n = MSG_ReadLong();
		Info_SetValueForKey( returninfo, "mod_size", va( "%i", n ), 2048 );
		c = MSG_ReadByte() ? true : false;
		Info_SetValueForKey( returninfo, "svonly", c ? "Y" : "N", 2048 );
		c = MSG_ReadByte() ? true : false;
		Info_SetValueForKey( returninfo, "cldll", c ? "Y" : "N", 2048 );
	}
}

/*
=================
NET_ParseServerList

=================
*/
int NET_ParseServerList( void )
{
	net_adrlist_t *p;
	netadr_t adr;
	char szadr[128] = "";
	unsigned char ip[4];
	unsigned short port;
	int nNumAddresses;
	int unique;

	MSG_BeginReading();
	MSG_ReadLong();
	MSG_ReadByte();
	MSG_ReadByte();
	unique = MSG_ReadLong();

	nNumAddresses = net_message.cursize - msg_readcount;
	nNumAddresses /= 6;

	while ( nNumAddresses-- > 0 )
	{
		Q_memset( szadr, 0, sizeof( szadr ) );

		ip[0] = MSG_ReadByte();
		ip[1] = MSG_ReadByte();
		ip[2] = MSG_ReadByte();
		ip[3] = MSG_ReadByte();
		port = BigShort( MSG_ReadShort() );

		_snprintf( szadr, sizeof( szadr ), "%i.%i.%i.%i:%i", ip[0], ip[1], ip[2], ip[3], port );

		if ( NET_StringToAdr( szadr, &adr ) )
		{
			p = (net_adrlist_t *)Mem_ZeroMalloc( sizeof( net_adrlist_t ) );
			p->remote_address = adr;
			p->next = g_addresses;
			g_addresses = p;
		}
	}

	if ( unique )
	{
		unsigned char data[1024];
		sizebuf_t msg;

		Q_memset( &msg, 0, sizeof( msg ) );
		msg.buffername = "NET_ParseServerList";
		msg.data = data;
		msg.cursize = 0;
		msg.maxsize = sizeof( data );

		Net_GetBatchServerList( &msg, unique );

		if ( msg.cursize )
		{
			NET_SendPacket( NS_CLIENT, msg.cursize, msg.data, net_from );
		}
	}
	else
	{
		return 1;
	}

	return 0;
}

/*
=================
Net_APIProcess

=================
*/
int Net_APIProcess( void )
{
	int iret;
	char c;
	net_api_query_t *p;
	net_response_t r;
	char returninfo[2048];

	iret = 0;
	p = NULL;
	returninfo[0] = 0;

	MSG_BeginReading();
	MSG_ReadLong();
	c = MSG_ReadByte();

	switch ( c )
	{
	case M2A_SERVER_BATCH:
		if ( NET_ParseServerList() )
		{
			p = Net_APIFind( NETAPI_REQUEST_SERVERLIST, &net_from );
			if ( p )
			{
				if ( p->callback )
				{
					Q_memset( &r, 0, sizeof( r ) );
					r.context = p->context;
					r.type = p->type;
					r.error = NET_SUCCESS;
					r.response = g_addresses;
					r.ping = realtime - p->requesttime;

					p->callback( &r );
				}

				if ( !( p->flags & FNETAPI_MULTIPLE_RESPONSE ) )
					Net_APIKill( p );

				iret = 1;
			}

			Net_KillServerList();
		}
		break;
	case S2A_INFO_DETAILED:
		p = Net_APIFind( NETAPI_REQUEST_DETAILS, &net_from );
		if ( p )
		{
			if ( p->callback )
			{
				NET_ParseServerInfoResponse( returninfo );

				r.context = p->context;
				r.type = p->type;
				r.error = NET_SUCCESS;
				r.remote_address = net_from;
				r.response = returninfo;
				r.ping = realtime - p->requesttime;

				p->callback( &r );
			}

			if ( !( p->flags & FNETAPI_MULTIPLE_RESPONSE ) )
				Net_APIKill( p );

			iret = 1;
		}
		break;
	case A2A_ACK:
		p = Net_APIFind( NETAPI_REQUEST_PING, &net_from );
		if ( p )
		{
			if ( p->callback )
			{
				r.context = p->context;
				r.type = p->type;
				r.error = NET_SUCCESS;
				r.remote_address = net_from;
				r.response = returninfo;
				r.ping = realtime - p->requesttime;

				p->callback( &r );
			}

			if ( !( p->flags & FNETAPI_MULTIPLE_RESPONSE ) )
				Net_APIKill( p );

			iret = 1;
		}
		break;
	case S2A_PLAYER:
		p = Net_APIFind( NETAPI_REQUEST_PLAYERS, &net_from );
		if ( p )
		{
			if ( p->callback )
			{
				NET_ParsePlayersResponse( returninfo );

				r.context = p->context;
				r.type = p->type;
				r.error = NET_SUCCESS;
				r.remote_address = net_from;
				r.response = returninfo;
				r.ping = realtime - p->requesttime;

				p->callback( &r );
			}

			if ( !( p->flags & FNETAPI_MULTIPLE_RESPONSE ) )
				Net_APIKill( p );

			iret = 1;
		}
		break;
	case S2A_RULES:
		p = Net_APIFind( NETAPI_REQUEST_RULES, &net_from );
		if ( p )
		{
			if ( p->callback )
			{
				NET_ParseRulesResponse( returninfo );

				r.context = p->context;
				r.type = p->type;
				r.error = NET_SUCCESS;
				r.remote_address = net_from;
				r.response = returninfo;
				r.ping = realtime - p->requesttime;

				p->callback( &r );
			}

			if ( !( p->flags & FNETAPI_MULTIPLE_RESPONSE ) )
				Net_APIKill( p );

			iret = 1;
		}
		break;
	default:
		Con_Printf( "Net_APIProcess: Unknown message type %i\n", c );
		break;
	}

	return iret;
}

/*
=================
Net_AdrToString

=================
*/
char *Net_AdrToString( struct netadr_s *a )
{
	return NET_AdrToString( *a );
}

/*
=================
Net_StringToAdr

=================
*/
int Net_StringToAdr( char *s, struct netadr_s *a )
{
	return NET_StringToAdr( s, a );
}

/*
=================
Net_InitNetworking

=================
*/
void Net_InitNetworking( void )
{
	NET_Config( true );
}

/*
=================
Net_CompareAdr

=================
*/
int Net_CompareAdr( struct netadr_s *a, struct netadr_s *b )
{
	return NET_CompareAdr( *a, *b );
}

/*
=================
Net_GetPacketLoss

=================
*/
int Net_GetPacketLoss( void )
{
	int packet_loss = 0;

	if ( cls.state == ca_active )
	{
		packet_loss = (int)cls.packet_loss;
		packet_loss = ( packet_loss < 0 ) ? 0 : packet_loss;
		packet_loss = ( packet_loss > 100 ) ? 100 : packet_loss;
	}

	return packet_loss;
}

/*
=================
Net_Status

=================
*/
void Net_Status( struct net_status_s *status )
{
	int connected;

	if ( !status )
		return;

	connected = ( cls.state != ca_disconnected ) ? 1 : 0;

	status->connected = connected;
	status->local_address = net_local_adr;
	status->remote_address = cls.netchan.remote_address;
	status->packet_loss = connected ? Net_GetPacketLoss() : 0;
	status->latency = connected ? g_flLatency : 0.0;
	status->connection_time = connected ? realtime - cls.netchan.connect_time : 0.0;
	status->rate = rate.value;
}

/*
=================
Net_CancelRequest

=================
*/
void Net_CancelRequest( int context )
{
	net_api_query_t *p;

	p = Net_APIFindContext( context );
	if ( p )
	{
		Net_APIKill( p );
	}
}

/*
=================
Net_CancelAllRequests

=================
*/
void Net_CancelAllRequests( void )
{
	Net_APIClearQueries();
}

net_api_t netapi =
{
	Net_InitNetworking,
	Net_Status,
	Net_SendRequest,
	Net_CancelRequest,
	Net_CancelAllRequests,
	Net_AdrToString,
	Net_CompareAdr,
	Net_StringToAdr,
	Info_ValueForKey,
	Info_RemoveKey,
	Info_SetValueForKey,
};
