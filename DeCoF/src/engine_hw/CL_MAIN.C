// cl_main.c  -- client main loop

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "crc.h"
#include "info.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "gl_model.h"
#include "protocol.h"
#include "vid.h"
#include "render.h"
#include "draw.h"
#include "sound.h"
#include "tmessage.h"
#include "hashpak.h"
#include "pmove.h"
#include "vgui_int.h"
#include "cdll_int.h"
#include "hltv.h"
#include "con_nprint.h"

// gl_screen.c: the download percentage is a cvar in this build (screen.h
// declares it as a float)
extern	cvar_t	scr_downloading;
void	SCR_EndLoadingPlaque( void );

#define MAX_STARTUP_TIMINGS	32
#define MAX_LOCAL_SERVERS	16

typedef struct
{
	const char	*name;
	float		time;
} startup_timing_t;

typedef struct
{
	int			inuse;
	netadr_t	adr;
	char		info[2048];
} server_cache_t;

cvar_t	cl_weaponlistfix = { "cl_weaponlistfix", "0" };
cvar_t	cl_fixtimerate = { "cl_fixtimerate", "7.5" };
cvar_t	cl_showmessages = { "cl_showmessages", "0" };
cvar_t	cl_connect_test = { "cl_connect_test", "0" };
cvar_t	cl_needinstanced = { "cl_needinstanced", "0" };
cvar_t	cl_showfps = { "cl_showfps", "0" };
cvar_t	cl_cmdbackup = { "cl_cmdbackup", "2", FCVAR_ARCHIVE };
cvar_t	cl_showevents = { "cl_showevents", "0" };
cvar_t	cl_clockreset = { "cl_clockreset", "0.1" };
cvar_t	cl_himodels = { "cl_himodels", "0", FCVAR_ARCHIVE };
cvar_t	cl_gaitestimation = { "cl_gaitestimation", "1" };
cvar_t	cl_timeout = { "cl_timeout", "60", FCVAR_ARCHIVE };
cvar_t	cl_shownet = { "cl_shownet", "0" };
cvar_t	cl_showsizes = { "cl_showsizes", "0", FCVAR_SPONLY };
cvar_t	cl_solid_players = { "cl_solid_players", "1" };
cvar_t	cl_nodelta = { "cl_nodelta", "0" };
cvar_t	cl_idealpitchscale = { "cl_idealpitchscale", "0.8", FCVAR_ARCHIVE };
cvar_t	rcon_address = { "rcon_address", "" };
cvar_t	rcon_port = { "rcon_port", "0" };
cvar_t	cl_resend = { "cl_resend", "6.0" };
cvar_t	cl_slisttimeout = { "cl_slist", "10.0" };
cvar_t	cl_allow_download = { "cl_allowdownload", "1", FCVAR_ARCHIVE };
cvar_t	cl_allow_upload = { "cl_allowupload", "1", FCVAR_ARCHIVE };
cvar_t	cl_cmdrate = { "cl_cmdrate", "30", FCVAR_ARCHIVE };
cvar_t	cl_download_ingame = { "cl_download_ingame", "1", FCVAR_ARCHIVE };
cvar_t	cl_logofile = { "cl_logofile", "lambda", FCVAR_ARCHIVE };
cvar_t	cl_logocolor = { "cl_logocolor", "orange", FCVAR_ARCHIVE };
cvar_t	password = { "password", "", FCVAR_USERINFO };
cvar_t	cl_name = { "name", "unnamed", FCVAR_ARCHIVE | FCVAR_USERINFO | FCVAR_PRINTABLEONLY };
cvar_t	team = { "team", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	skin = { "skin", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	cl_model = { "model", "", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	topcolor = { "topcolor", "0", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	bottomcolor = { "bottomcolor", "0", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	rate = { "rate", "25000", FCVAR_USERINFO };
cvar_t	cl_updaterate = { "cl_updaterate", "20", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	cl_lw = { "cl_lw", "1", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	cl_lc = { "cl_lc", "1", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	cl_dlmax = { "cl_dlmax", "512", FCVAR_ARCHIVE | FCVAR_USERINFO };
cvar_t	fs_lazy_precache = { "fs_lazy_precache", "0" };
cvar_t	fs_precache_timings = { "fs_precache_timings", "0" };
cvar_t	fs_perf_warnings = { "fs_perf_warnings", "0" };
cvar_t	fs_startup_timings = { "fs_startup_timings", "0" };

startup_timing_t	g_StartupTimings[MAX_STARTUP_TIMINGS];
static int			g_iCurrentTiming;

client_static_t	cls;
client_state_t	cl;

// FIXME: put these on hunk?
efrag_t			cl_efrags[MAX_EFRAGS];
cl_entity_t		*cl_entities = NULL;
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];
dlight_t		cl_elights[MAX_ELIGHTS];

int				cl_numvisedicts;
cl_entity_t		*cl_visedicts[MAX_VISEDICTS];

playermove_t	g_clmove;

server_cache_t	cached_servers[MAX_LOCAL_SERVERS];

static char		g_lastrconcommand[1024];
static netadr_t	g_rconaddress;
static netadr_t	g_GameServerAddress;

qboolean	g_bRedirectedToProxy;

int			cl_playerindex;

float		g_LastScreenUpdateTime;

qboolean	cl_inmovie;

/*
====================
AddStartupTiming

====================
*/
void AddStartupTiming( const char *name )
{
	g_iCurrentTiming++;
	g_StartupTimings[g_iCurrentTiming].name = name;
	g_StartupTimings[g_iCurrentTiming].time = Sys_FloatTime();
}

/*
====================
SetupStartupTimings

====================
*/
void SetupStartupTimings( void )
{
	g_iCurrentTiming = 0;
	g_StartupTimings[g_iCurrentTiming].name = "Startup";
	g_StartupTimings[g_iCurrentTiming].time = Sys_FloatTime();
}

/*
====================
PrintStartupTimings

====================
*/
void PrintStartupTimings( void )
{
	int		i;

	Con_Printf( "Startup timings (%.2f total)\n", g_StartupTimings[g_iCurrentTiming].time - g_StartupTimings[0].time );
	Con_Printf( "    0.00    Startup\n" );

	for ( i = 1; i <= g_iCurrentTiming; i++ )
	{
		Con_Printf( "    %.2f    %s\n", g_StartupTimings[i].time - g_StartupTimings[i - 1].time, g_StartupTimings[i].name );
	}
}

/*
====================
CL_CheckClientState

====================
*/
void CL_CheckClientState( void )
{
	if ( ( cls.state == ca_connected || cls.state == ca_uninitialized ) && cls.signon == SIGNONS )
	{
		unsigned char	*IPPtr;
		short			port;

		cls.state = ca_active;

		if ( fs_startup_timings.value )
		{
			PrintStartupTimings();
		}

		IPPtr = cls.netchan.remote_address.ip;
		port = cls.netchan.remote_address.port;
		if ( !port )
		{
			IPPtr = net_local_adr.ip;
			port = net_local_adr.port;
		}

		VGuiWrap2_NotifyOfServerConnect( com_gamedir, *(int *)IPPtr, Q_ntohs( port ) );
	}
}

/*
====================
CL_UpdateSoundFade

====================
*/
void CL_UpdateSoundFade( void )
{
	int		nTotal;
	float	fGap;
	float	f;

	cls.soundfade.nClientSoundFadePercent = 0;

	nTotal = cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeInTime + cls.soundfade.soundFadeHoldTime;
	if ( realtime >= cls.soundfade.soundFadeStartTime + nTotal )
		return;

	fGap = realtime - cls.soundfade.soundFadeStartTime;

	if ( cls.soundfade.soundFadeOutTime && fGap < cls.soundfade.soundFadeOutTime )
	{
		f = fGap / cls.soundfade.soundFadeOutTime;
		cls.soundfade.nClientSoundFadePercent = cls.soundfade.nStartPercent * f;
	}
	else if ( fGap < cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime )
	{
		cls.soundfade.nClientSoundFadePercent = cls.soundfade.nStartPercent;
	}
	else
	{
		f = fGap - ( cls.soundfade.soundFadeOutTime + cls.soundfade.soundFadeHoldTime );
		f = f / cls.soundfade.soundFadeOutTime;
		f = 1.0f - f;
		cls.soundfade.nClientSoundFadePercent = cls.soundfade.nStartPercent * f;
	}
}

/*
==================
CL_PingServers_f

==================
*/
void CL_PingServers_f( void )
{
	int			i;
	netadr_t	adr;

	Con_Printf( "Searching for local servers...\n" );

	cls.slist_time = realtime;

	NET_Config( true );		// allow remote

	Q_memset( &adr, 0, sizeof( adr ) );

	for ( i = 0; i < 10; i++ )
	{
		adr.port = BigShort( Q_atoi( "27015" ) + i );

		if ( !noip )
		{
			adr.type = NA_BROADCAST;
			Netchan_OutOfBandPrint( NS_CLIENT, adr, "infostring" );
		}

		if ( !noipx )
		{
			adr.type = NA_BROADCAST_IPX;
			Netchan_OutOfBandPrint( NS_CLIENT, adr, "infostring" );
		}
	}
}

/*
==================
CL_Slist_f

==================
*/
void CL_Slist_f( void )
{
	num_servers = 0;
	Q_memset( cached_servers, 0, sizeof( cached_servers ) );

	CL_PingServers_f();
}

/*
==================
CL_PrintCachedServer

==================
*/
void CL_PrintCachedServer( int slot )
{
	server_cache_t	*p;

	if ( slot < 0 || slot >= MAX_LOCAL_SERVERS )
		return;

	p = &cached_servers[slot];
	if ( !p->inuse )
		return;

	Con_Printf( "%i ", slot + 1 );

	if ( Info_ValueForKey( p->info, "type" )[0] == 'p' )
	{
		Con_Printf( "Proxy for:  %s\n  ", Info_ValueForKey( p->info, "proxyaddress" ) );
	}
	else if ( Q_atoi( Info_ValueForKey( p->info, "proxytarget" ) ) )
	{
		Con_Printf( "Proxy target\n  " );
	}

	Con_Printf( "%s %s %i/%i\nAdr:  %s - Dir:  %s\n%s\n",
		Info_ValueForKey( p->info, "hostname" ),
		Info_ValueForKey( p->info, "map" ),
		Q_atoi( Info_ValueForKey( p->info, "players" ) ),
		Q_atoi( Info_ValueForKey( p->info, "max" ) ),
		NET_AdrToString( p->adr ),
		Info_ValueForKey( p->info, "gamedir" ),
		Info_ValueForKey( p->info, "desciption" ) );

	switch ( Info_ValueForKey( p->info, "type" )[0] )
	{
	case 'l':
		Con_Printf( "  listen - " );
		break;
	case 'd':
		Con_Printf( "  dedicated - " );
		break;
	case 'p':
		Con_Printf( "  proxy - " );
		break;
	default:
		Con_Printf( "  unknown server type - " );
		break;
	}

	if ( Info_ValueForKey( p->info, "os" )[0] == 'w' )
	{
		Con_Printf( "win32 - " );
	}
	else
	{
		Con_Printf( "linux - " );
	}

	if ( Q_atoi( Info_ValueForKey( p->info, "password" ) ) == 1 )
	{
		Con_Printf( "password - " );
	}
	else
	{
		Con_Printf( "nopasswd - " );
	}

	if ( Q_atoi( Info_ValueForKey( p->info, "secure" ) ) == 1 )
	{
		Con_Printf( "secure" );
	}
	else
	{
		Con_Printf( "insecure" );
	}

	Con_Printf( "\n" );
}

/*
==================
CL_AddToServerCache

==================
*/
void CL_AddToServerCache( netadr_t adr, char *info )
{
	int		i;
	float	fCurrentTime;

	if ( num_servers >= MAX_LOCAL_SERVERS )
		return;

	if ( cl_slisttimeout.value > 0.0 && ( fCurrentTime = realtime ) - cls.slist_time >= cl_slisttimeout.value )
		return;

	for ( i = 0; i < num_servers; i++ )
	{
		if ( NET_CompareAdr( cached_servers[i].adr, adr ) )
			return;
	}

	cached_servers[num_servers].inuse = 1;
	cached_servers[num_servers].adr = adr;
	Q_strncpy( cached_servers[num_servers].info, info, sizeof( cached_servers[num_servers].info ) - 1 );
	cached_servers[num_servers].info[sizeof( cached_servers[num_servers].info ) - 1] = 0;

	num_servers++;

	Con_Printf( "------------------\n" );
	CL_PrintCachedServer( num_servers - 1 );
}

/*
==================
CL_ParseStatusMessage_guess

Answer to the "infostring" broadcast of CL_PingServers_f; nothing calls it.
==================
*/
void CL_ParseStatusMessage_guess( void )
{
	char	*s;

	s = MSG_ReadString();
	if ( !s || !s[0] )
		return;

	CL_AddToServerCache( net_from, s );
}

/*
==================
CL_ListCachedServers_f

==================
*/
void CL_ListCachedServers_f( void )
{
	int				i;
	server_cache_t	*p;

	if ( !num_servers )
	{
		Con_Printf( "No local servers in list.\nTry 'slist' to search again.\n" );
		return;
	}

	for ( i = 0; i < num_servers; i++ )
	{
		p = &cached_servers[i];
		if ( !p->inuse )
			continue;

		Con_Printf( "------------------\n" );
		CL_PrintCachedServer( i );
	}
}

/*
==================
CL_ParseServerList

==================
*/
void CL_ParseServerList( void )
{
	char			szAddress[128] = "";
	unsigned char	cIP[4];
	int				i;
	unsigned short	iIPPort;
	int				nNumAddresses;
	int				count = 1;

	MSG_ReadByte();

	nNumAddresses = net_message.cursize - 6;
	nNumAddresses /= 6;

	while ( nNumAddresses-- > 0 )
	{
		Q_memset( szAddress, 0, sizeof( szAddress ) );

		for ( i = 0; i < 4; i++ )
		{
			cIP[i] = MSG_ReadByte();
		}

		_snprintf( szAddress, sizeof( szAddress ), "%i.%i.%i.%i", cIP[0], cIP[1], cIP[2], cIP[3] );

		iIPPort = BigShort( MSG_ReadShort() );

		if ( count <= 100 )
		{
			Con_Printf( "%4i:  %s:%i\n", count, szAddress, iIPPort );
		}

		count++;
	}

	Con_Printf( "%i total servers\n", count );
}

/*
==================
CL_ParseBatchModList

==================
*/
void CL_ParseBatchModList( void )
{
	char	*pszModName = NULL;
	char	szName[256];
	int		nPlayers = 0, nServers = 0;
	char	*pszServers;
	char	*pszPlayers;
	int		nCount = 0;

	MSG_ReadByte();

	szName[0] = 0;

	while ( 1 )
	{
		pszModName = MSG_ReadString();
		if ( !pszModName || !pszModName[0] )
			break;

		if ( !Q_stricmp( pszModName, "end-of-list" ) )
			break;

		if ( !Q_stricmp( pszModName, "more-in-list" ) )
			break;

		Q_strncpy( szName, pszModName, sizeof( szName ) - 1 );
		szName[sizeof( szName ) - 1] = 0;

		pszServers = MSG_ReadString();
		nServers = Q_atoi( pszServers );
		pszPlayers = MSG_ReadString();
		nPlayers = Q_atoi( pszPlayers );

		nCount++;

		if ( pszServers && pszPlayers )
		{
			Con_Printf( "%3i %5i %s\n", nServers, nPlayers, szName );
		}
	}

	Con_Printf( "%i servers\n", nCount );

	if ( pszModName && pszModName[0] && !Q_stricmp( pszModName, "more-in-list" ) )
	{
		char	szRequest[128];

		_snprintf( szRequest, sizeof( szRequest ), "%c\r\n%s\r\n", 'x', szName );

		Con_DPrintf( "Requesting next batch ( >%s ) of mods from %s\n", szName, NET_AdrToString( net_from ) );

		NET_SendPacket( NS_CLIENT, Q_strlen( szRequest ) + 1, szRequest, net_from );
	}
	else
	{
		Con_Printf( "Done.\n" );
	}
}

/*
==================
CL_ParseBatchUserList

==================
*/
void CL_ParseBatchUserList( void )
{
	char			szAddress[128] = "";
	unsigned char	cIP[4];
	int				i;
	unsigned short	iIPPort;
	int				nNumAddresses;
	static int		count = 1;
	int				unique = 0;
	int				hash = 0;

	MSG_ReadByte();

	hash = MSG_ReadLong();
	unique = MSG_ReadLong();

	nNumAddresses = net_message.cursize - msg_readcount;
	nNumAddresses /= 6;

	while ( nNumAddresses-- > 0 )
	{
		Q_memset( szAddress, 0, sizeof( szAddress ) );

		for ( i = 0; i < 4; i++ )
		{
			cIP[i] = MSG_ReadByte();
		}

		_snprintf( szAddress, sizeof( szAddress ), "%i.%i.%i.%i", cIP[0], cIP[1], cIP[2], cIP[3] );

		iIPPort = BigShort( MSG_ReadShort() );

		Con_Printf( "%4i:  %s:%i\n", count, szAddress, iIPPort );

		count++;
	}

	if ( unique && hash )
	{
		unsigned char	c[20];
		int				i;

		i = 0;
		Q_strcpy( (char *)c, "users" );
		i += Q_strlen( "users" ) + 1;

		*(int *)&c[i] = hash;
		i += 4;
		*(int *)&c[i] = unique;
		i += 4;

		Con_DPrintf( "Requesting next batch ( %i ) user list from %s\n", unique, NET_AdrToString( net_from ) );

		NET_SendPacket( NS_CLIENT, i, c, net_from );
	}
	else
	{
		Con_Printf( "%i users\n", count );
		count = 0;
		Con_Printf( "Done.\n" );
	}
}

/*
=====================
CL_GetFragmentSize

=====================
*/
int CL_GetFragmentSize( void *state )
{
	int		*state_value;
	int		retval;

	retval = 128;

	state_value = state;
	if ( *state_value == ca_active )
	{
		if ( cl_dlmax.value >= 16 )
		{
			retval = min( 1024.0, cl_dlmax.value );
		}
		else
		{
			retval = 256;
		}
	}

	return retval;
}

/*
=====================
CL_ConnectClient

=====================
*/
void CL_ConnectClient( void )
{
	ContinueLoadingProgressBar( "ClientConnect", 2, 0.0 );

	if ( cls.state == ca_connected )
	{
		if ( !cls.demoplayback )
			Con_DPrintf( "Duplicate connect ack. received.  Ignored.\n" );
		return;
	}

	cls.userid = 0;
	cls.trueaddress[0] = 0;

	if ( Cmd_Argc() >= 3 )
	{
		cls.userid = Q_atoi( Cmd_Argv( 1 ) );
		Q_strncpy( cls.trueaddress, Cmd_Argv( 2 ), sizeof( cls.trueaddress ) - 1 );
		cls.trueaddress[sizeof( cls.trueaddress ) - 1] = 0;
	}

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );

	NET_ClearLagData( true, false );

	if ( fs_startup_timings.value )
	{
		AddStartupTiming( "connection accepted by server" );
	}

	if ( !Q_stricmp( "loopback", NET_AdrToString( net_from ) ) )
	{
		Con_DPrintf( "Connection accepted.\n" );
	}
	else
	{
		Con_Printf( "Connection accepted by %s\n", NET_AdrToString( net_from ) );
	}

	cls.state = ca_connected;

	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, "new" );

	cls.demonum = -1;		// not in the demo loop now
	cls.signon = 0;
	cls.connect_time = realtime;
	cl.validsequence = 0;
	cl.delta_sequence = -1;
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = realtime;
}

/*
=====================
CL_ClientDllQuery

=====================
*/
int CL_ClientDllQuery( const char *s )
{
	int		len;
	int		maxsize = 2048;
	byte	data[2048];
	int		valid;

	if ( !Host_IsServerActive() )
		return 0;

	if ( Host_IsSinglePlayerGame() )
		return 0;

	Q_memset( data, 0, maxsize );

	len = maxsize - 4;
	valid = ClientDLL_ConnectionlessPacket( &net_from, s, (char *)&data[4], &len );

	if ( len && len <= ( maxsize - 4 ) )
	{
		*(int *)data = -1;
		NET_SendPacket( NS_CLIENT, len + 4, data, net_from );
	}

	return valid;
}

/*
=====================
CL_SendRcon

=====================
*/
void CL_SendRcon( netadr_t *to, unsigned int challenge )
{
	char	message[1152];

	if ( !g_lastrconcommand[0] )
	{
		Con_Printf( "Rcon:  Empty rcon string\n" );
		return;
	}

	if ( !NET_CompareAdr( *to, g_rconaddress ) )
	{
		Con_Printf( "Rcon:  Challenge from spoofed/invalid IP address\n" );
		return;
	}

	_snprintf( message, sizeof( message ), "rcon %u \"%s\" %s", challenge, rcon_password.string, g_lastrconcommand );

	Netchan_OutOfBandPrint( NS_CLIENT, *to, "%s", message );
}

/*
=====================
CL_ServiceChallenge

=====================
*/
void CL_ServiceChallenge( void )
{
	char			*type;
	char			*value;
	unsigned int	val;

	if ( Cmd_Argc() != 3 )
		return;

	type = Cmd_Argv( 1 );
	if ( !type || !type[0] || Q_stricmp( type, "rcon" ) )
		return;

	value = Cmd_Argv( 2 );
	val = Q_atoi( value );

	CL_SendRcon( &net_from, val );
}

/*
=====================
CL_CreateResponse

=====================
*/
void CL_CreateResponse( netadr_t *to, const char *varname, const char *value )
{
	char	response[1024];

	if ( value )
	{
		_snprintf( response, sizeof( response ), "%c%c%c%c\"%s\"=\"%s\"\n", 255, 255, 255, 255, varname, value );
	}
	else
	{
		_snprintf( response, sizeof( response ), "%c%c%c%c\"%s\"\n", 255, 255, 255, 255, varname );
	}

	NET_SendPacket( NS_CLIENT, Q_strlen( response ) + 1, response, *to );
}

/*
=====================
CL_ServiceGet

=====================
*/
void CL_ServiceGet( void )
{
	char	*var;
	cvar_t	*cv;

	// Only allow from loopback
	if ( net_from.type != NA_IP )
		return;

	if ( net_from.ip[0] != 127 ||
		net_from.ip[1] != 0 ||
		net_from.ip[2] != 0 ||
		net_from.ip[3] != 1 )
		return;

	if ( Cmd_Argc() != 2 )
		return;

	var = Cmd_Argv( 1 );
	if ( !var || !var[0] )
		return;

	if ( !Q_stricmp( "clientaddress", var ) )
	{
		CL_CreateResponse( &net_from, var, va( "%s", cls.trueaddress ) );
	}
	else if ( !Q_stricmp( "serveraddress", var ) )
	{
		CL_CreateResponse( &net_from, var, va( "%s", NET_AdrToString( cls.netchan.remote_address ) ) );
	}
	else if ( !Q_stricmp( "userid", var ) )
	{
		CL_CreateResponse( &net_from, var, va( "%i", cls.userid ) );
	}
	else
	{
		cv = Cvar_FindVar( var );
		if ( cv )
		{
			CL_CreateResponse( &net_from, var, va( "%s", cv->string ) );
		}
		else
		{
			CL_CreateResponse( &net_from, var, "unknown variable" );
		}
	}
}

/*
=====================
CL_Listen

=====================
*/
void CL_Listen( char *address )
{
	if ( !address )
		return;

	Net_StringToAdr( address, &cls.connect_stream );
	cls.connect_stream.type = NA_IP;

	if ( cls.connect_stream.ip[0] < 224 || cls.connect_stream.ip[0] > 239 )
	{
		Con_Printf( "%s is not a class D IP.\n", address );
		return;
	}

	if ( cls.demoplayback )
		CL_StopPlayback();

	CL_Disconnect();

	Q_memset( msg_buckets, 0, sizeof( msg_buckets ) );
	Q_memset( total_data, 0, sizeof( total_data ) );

	cls.connect_retry = 0;
	gfExtendedError = false;

	NET_Config( true );		// allow remote

	Netchan_Setup( NS_MULTICAST, &cls.netchan, cls.connect_stream, -1, &cls, CL_GetFragmentSize );

	NET_FlushSocket( NS_MULTICAST );

	cls.passive = true;

	if ( !cls.connect_stream.port )
	{
		cls.connect_stream.port = BigShort( Q_atoi( "27025" ) );
	}

	NET_JoinGroup( cls.netchan.sock, cls.connect_stream );

	Con_Printf( "Joining multicast group %s ...\n", NET_AdrToString( cls.connect_stream ) );

	NET_ClearLagData( true, false );

	cls.state = ca_connecting;
	cls.connect_time = realtime;
	cls.demonum = -1;
	cls.signon = 0;
	cl.validsequence = 0;
	cl.delta_sequence = -1;
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = realtime;
}

/*
=====================
CL_Listen_f

=====================
*/
void CL_Listen_f( void )
{
	char	*server;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "usage: listen <IP>\n" );
		return;
	}

	server = Cmd_Args();
	if ( !server )
		return;

	cls.challenge = 0;

	CL_Listen( server );
}

/*
=====================
CL_Commentator_f

=====================
*/
void CL_Commentator_f( void )
{
	int		i;
	char	*val;

	if ( Cmd_Argc() < 2 )
	{
		val = Info_ValueForKey( cls.userinfo, "*hltv" );
		if ( Q_strlen( val ) )
		{
			i = Q_atoi( val );
			if ( i == TYPE_COMMENTATOR )
				Con_Printf( "HLTV commentator mode enabled.\n" );
			else
				Con_Printf( "HLTV commentator mode disabled.\n" );
		}
		else
		{
			Con_Printf( "HLTV commentator mode disabled.\n" );
		}
		return;
	}

	i = Q_atoi( Cmd_Args() );

	switch ( i )
	{
	case 0:
		Info_SetValueForStarKey( cls.userinfo, "*hltv", va( "%i", TYPE_CLIENT ), MAX_INFO_STRING );
		break;
	case 1:
		Info_SetValueForStarKey( cls.userinfo, "*hltv", va( "%i", TYPE_COMMENTATOR ), MAX_INFO_STRING );
		break;
	}
}

/*
=====================
IsFromConnectedServer

=====================
*/
qboolean IsFromConnectedServer( netadr_t adr )
{
	return ( NET_CompareAdr( adr, g_GameServerAddress ) || NET_CompareAdr( adr, g_rconaddress ) );
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket( void )
{
	char			*s;
	char			*cmd;
	char			*args;
	unsigned char	c = 0;

	MSG_BeginReading();
	MSG_ReadLong();		// skip the -1 marker

	s = MSG_ReadStringLine();
	args = s;

	Cmd_TokenizeString( s );

	cmd = Cmd_Argv( 0 );
	if ( cmd )
		c = cmd[0];

	if ( !Q_stricmp( cmd, "challenge" ) )
	{
		if ( IsFromConnectedServer( net_from ) )
			CL_ServiceChallenge();
		return;
	}

	if ( c == S2C_CONNECTION )
	{
		if ( IsFromConnectedServer( net_from ) )
			CL_ConnectClient();
		return;
	}

	// multicast challenge
	if ( c == 'G' )
	{
		if ( cls.state != ca_connecting )
			return;

		if ( Cmd_Argc() != 4 )
			return;

		cls.challenge = Q_atoi( Cmd_Argv( 1 ) );
		cls.authprotocol = Q_atoi( Cmd_Argv( 2 ) );

		if ( cls.authprotocol == 255 )
			cls.authprotocol = 2;

		if ( cls.authprotocol == 2 )
		{
			CL_Listen( Cmd_Argv( 3 ) );
		}
		else
		{
			Con_Printf( "Invalid multicast authentication type (%i).\n", cls.authprotocol );
			CL_Disconnect_f();
		}
		return;
	}

	// challenge from the server we are connecting to
	if ( c == S2C_CHALLENGE )
	{
		if ( IsFromConnectedServer( net_from ) )
		{
			if ( cls.state != ca_connecting )
				return;

			if ( Cmd_Argc() != 3 && Cmd_Argc() != 5 )
				return;

			cls.challenge = Q_atoi( Cmd_Argv( 1 ) );
			cls.authprotocol = Q_atoi( Cmd_Argv( 2 ) );

			if ( cls.authprotocol == 255 )
				cls.authprotocol = 2;

			switch ( cls.authprotocol )
			{
			case 3:
				{
					char	*pchGSSteamID;
					char	*pchBSecure;

					if ( Cmd_Argc() != 5 )
						return;

					pchGSSteamID = Cmd_Argv( 3 );
					if ( !pchGSSteamID )
					{
						Con_Printf( "STEAM keysize is bogus (1)\n" );
						CL_Disconnect_f();
						return;
					}

					pchBSecure = Cmd_Argv( 4 );
					if ( !pchBSecure )
					{
						Con_Printf( "STEAM keysize is bogus (2)\n" );
						CL_Disconnect_f();
						return;
					}

					cls.isVAC2Secure = Q_atoi( pchBSecure );
					cls.GameServerSteamID = _atoi64( pchGSSteamID );

					CL_SendConnectPacket();
				}
				break;
			default:
				CL_SendConnectPacket();
				break;
			}
		}
		return;
	}

	// print command from somewhere
	if ( c == A2A_PRINT )
	{
		if ( IsFromConnectedServer( net_from ) )
		{
			MSG_BeginReading();
			MSG_ReadLong();
			MSG_ReadByte();
			Con_Printf( "%s\n", MSG_ReadString() );
		}
		return;
	}

	// bad password
	if ( c == '8' )
	{
		if ( IsFromConnectedServer( net_from ) )
		{
			if ( cls.state != ca_connecting )
				return;

			s++;
			if ( !Q_strncasecmp( s, "BADPASSWORD", Q_strlen( "BADPASSWORD" ) ) )
			{
				s += Q_strlen( "BADPASSWORD" );
			}

			Con_Printf( "%s", s );
			COM_ExplainDisconnection( false, "#GameUI_ServerConnectionFailedBadPassword" );
			Con_Printf( "Invalid server password.\n" );
			CL_Disconnect();
		}
		return;
	}

	// redirection to another server
	if ( c == 'L' )
	{
		if ( IsFromConnectedServer( net_from ) )
		{
			char	szCommand[260];

			if ( cls.state != ca_connecting )
				return;

			s++;
			_snprintf( szCommand, sizeof( szCommand ), "connect %s\n", s );
			Cbuf_AddText( szCommand );
			Con_Printf( "Redirecting connection to %s.\n", s );
			g_bRedirectedToProxy = true;
		}
		return;
	}

	// connection refused
	if ( c == '9' )
	{
		if ( IsFromConnectedServer( net_from ) )
		{
			if ( !COM_CheckParm( "-steam" ) && cls.state != ca_connecting )
				return;

			s++;
			COM_ExplainDisconnection( true, s );
			CL_Disconnect();
		}
		return;
	}

	if ( CL_ClientDllQuery( args ) )
		return;

	Con_Printf( "Unknown command:\n%c\n", c );
}

/*
=================
CL_PrecacheResources

=================
*/
qboolean CL_PrecacheResources( void )
{
	resource_t	*pResource, *next;

	SetLoadingProgressBarStatusText( "#GameUI_PrecachingResources" );
	ContinueLoadingProgressBar( "ClientConnect", 7, 0.0 );

	if ( fs_startup_timings.value )
	{
		AddStartupTiming( "begin CL_PrecacheResources()" );
	}

	// Precache everything we have on hand
	for ( pResource = cl.resourcesonhand.pNext; pResource && pResource != &cl.resourcesonhand; pResource = next )
	{
		next = pResource->pNext;

		if ( !( pResource->ucFlags & RES_PRECACHED ) )
		{
			switch ( pResource->type )
			{
			case t_decal:
				if ( !( pResource->ucFlags & RES_CUSTOM ) )
				{
					Draw_DecalSetName( pResource->nIndex, pResource->szFileName );
				}
				break;

			case t_sound:
				if ( pResource->ucFlags & RES_WASMISSING )
				{
					cl.sound_precache[pResource->nIndex] = NULL;
					break;
				}

				S_BeginPrecaching();
				cl.sound_precache[pResource->nIndex] = S_PrecacheSound( pResource->szFileName );
				S_EndPrecaching();

				if ( !cl.sound_precache[pResource->nIndex] && ( pResource->ucFlags & RES_FATALIFMISSING ) )
				{
					COM_ExplainDisconnection( true, "Cannot continue without sound %s, disconnecting.", pResource->szFileName );
					CL_Disconnect();
					return false;
				}
				break;

			case t_model:
				if ( pResource->nIndex >= cl.model_precache_count )
				{
					cl.model_precache_count = pResource->nIndex + 1;
					if ( cl.model_precache_count > MAX_MODELS )
						cl.model_precache_count = MAX_MODELS;
				}

				// Inline model, no need to load it
				if ( pResource->szFileName[0] == '*' )
					break;

				if ( fs_lazy_precache.value )
				{
					if ( !Q_strnicmp( pResource->szFileName, "maps", 4 ) )
					{
						cl.model_precache[pResource->nIndex] = Mod_ForName( pResource->szFileName, false, true );
					}
					else
					{
						cl.model_precache[pResource->nIndex] = Mod_FindName( true, pResource->szFileName );
					}
				}
				else
				{
					cl.model_precache[pResource->nIndex] = Mod_ForName( pResource->szFileName, false, true );
				}

				if ( !cl.model_precache[pResource->nIndex] )
				{
					if ( pResource->ucFlags )
					{
						Con_Printf( "Model %s not found and not available from server\n", pResource->szFileName );
					}

					if ( pResource->ucFlags & RES_FATALIFMISSING )
					{
						COM_ExplainDisconnection( true, "Cannot continue without model %s, disconnecting.", pResource->szFileName );
						CL_Disconnect();
						return false;
					}
				}

				if ( !Q_stricmp( pResource->szFileName, "models/player.mdl" ) )
				{
					cl_playerindex = pResource->nIndex;
				}
				break;

			case t_skin:
				break;

			case t_generic:
				break;

			case t_eventscript:
				cl.event_precache[pResource->nIndex].filename = Mem_Strdup( pResource->szFileName );
				cl.event_precache[pResource->nIndex].pszScript = (char *)COM_LoadFile( pResource->szFileName, 5, NULL );

				if ( !cl.event_precache[pResource->nIndex].pszScript )
				{
					Con_Printf( "Script %s not found and not available from server\n", pResource->szFileName );

					if ( pResource->ucFlags & RES_FATALIFMISSING )
					{
						COM_ExplainDisconnection( true, "Cannot continue without script %s, disconnecting.", pResource->szFileName );
						CL_Disconnect();
						return false;
					}
				}
				break;

			default:
				Con_DPrintf( "Unknown resource type\n" );
				break;
			}

			pResource->ucFlags |= RES_PRECACHED;
		}
	}

	if ( fs_startup_timings.value )
	{
		AddStartupTiming( "end  CL_PrecacheResources()" );
	}

	return true;
}

/*
==================
CL_MoveToOnHandList

==================
*/
void CL_MoveToOnHandList( resource_t *pResource )
{
	if ( !pResource )
	{
		Con_DPrintf( "Null resource passed to CL_MoveToOnHandList\n" );
		return;
	}

	CL_RemoveFromResourceList( pResource );
	CL_AddToResourceList( pResource, &cl.resourcesonhand );
}

/*
==================
CL_CleanFileName

==================
*/
const char *CL_CleanFileName( const char *filename )
{
	static char	*pfilename = "customization";

	if ( filename && *filename && *filename == '!' )
		return pfilename;

	return filename;
}

/*
==================
CL_RegisterCustomization

==================
*/
void CL_RegisterCustomization( resource_t *resource )
{
	customization_t	*pList;
	qboolean		bFound = false;

	// Already processed?
	pList = cl.players[resource->playernum].customdata.pNext;
	while ( pList )
	{
		if ( !Q_memcmp( pList->resource.rgucMD5_hash, resource->rgucMD5_hash, 16 ) )
		{
			bFound = true;
			break;
		}

		pList = pList->pNext;
	}

	if ( !bFound )
	{
		if ( !COM_CreateCustomization( &cl.players[resource->playernum].customdata, resource, resource->playernum, FCUST_FROMHPAK, NULL, NULL ) )
		{
			Con_Printf( "Unable to create custom decal for player %i\n", resource->playernum );
		}
	}
	else
	{
		Con_DPrintf( "Duplicate resource received and ignored.\n" );
	}
}

/*
==================
CL_LoadSecurityModules

Nothing calls it any more.
==================
*/
void CL_LoadSecurityModules( void )
{
	char	szCommand[20];

	AddStartupTiming( "begin CL_LoadSecurityModules()" );

	SetLoadingProgressBarStatusText( "#GameUI_LoadingSecurityModule" );
	ContinueLoadingProgressBar( "ClientConnect", 5, 0.0 );

	CloseSecurityModule();

	g_bUpdatingSecurityModules = false;

	if ( g_module.fLoaded )
	{
		szCommand[0] = 's';
		szCommand[1] = 'e';
		szCommand[2] = 'n';
		szCommand[3] = 'd';
		szCommand[4] = 'r';
		szCommand[5] = 'e';
		szCommand[6] = 's';
		szCommand[7] = 0;

		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_WriteString( &cls.netchan.message, szCommand );
	}
	else
	{
		COM_ExplainDisconnection( true, "#GameUI_ErrorLoadingSecurityModule" );
		CL_Disconnect();
	}

	AddStartupTiming( "end   CL_LoadSecurityModules()" );
}

/*
=====================
CL_ProcessFile

A file has been received via the fragmentation/reassembly layer, put it in the right spot and
 see if we have finished downloading files.
=====================
*/
void CL_ProcessFile( qboolean successfully_received, const char *filename )
{
	resource_t		*p, *n;
	char			*pfilename;
	unsigned char	rgucMD5_hash[16];
	char			szModuleC[20];

	szModuleC[0] = '!';
	szModuleC[1] = 'M';
	szModuleC[2] = 'o';
	szModuleC[3] = 'd';
	szModuleC[4] = 'u';
	szModuleC[5] = 'l';
	szModuleC[6] = 'e';
	szModuleC[7] = 'C';
	szModuleC[8] = '.';
	szModuleC[9] = 'd';
	szModuleC[10] = 'l';
	szModuleC[11] = 'l';
	szModuleC[12] = 0;

	if ( successfully_received && filename && filename[0] != '!' )
	{
		Con_Printf( "Processing %s\n", CL_CleanFileName( filename ) );
	}

	if ( !successfully_received )
	{
		Con_Printf( "Error: server failed to transmit file '%s'\n", CL_CleanFileName( filename ) );
	}

	pfilename = (char *)filename;
	if ( !Q_strnicmp( pfilename, "sound/", Q_strlen( "sound/" ) ) )
	{
		pfilename += Q_strlen( "sound/" );
	}

	for ( p = cl.resourcesneeded.pNext; p != &cl.resourcesneeded; p = n )
	{
		n = p->pNext;

		if ( !Q_strnicmp( filename, "!MD5", 4 ) )
		{
			COM_HexConvert( pfilename + 4, 32, rgucMD5_hash );

			if ( Q_memcmp( p->rgucMD5_hash, rgucMD5_hash, 16 ) )
				continue;
		}
		else if ( p->type == t_generic )
		{
			if ( Q_stricmp( p->szFileName, filename ) )
				continue;
		}
		else
		{
			if ( Q_stricmp( p->szFileName, pfilename ) )
				continue;
		}

		if ( successfully_received )
		{
			p->ucFlags &= ~RES_WASMISSING;
		}

		if ( filename[0] == '!' && cls.netchan.tempbuffer )
		{
			if ( p->nDownloadSize != cls.netchan.tempbuffersize )
			{
				Con_Printf( "CL_ProcessFile:  Downloaded %i bytes for purported %i byte file, ignoring download\n", cls.netchan.tempbuffersize, p->nDownloadSize );
			}
			else if ( p->ucFlags & RES_CUSTOM )
			{
				HPAK_AddLump( true, "custom.hpk", p, cls.netchan.tempbuffer, NULL );
				CL_RegisterCustomization( p );
			}
		}

		if ( cls.netchan.tempbuffer )
		{
			Mem_Free( cls.netchan.tempbuffer );
		}

		cls.netchan.tempbuffer = NULL;
		cls.netchan.tempbuffersize = 0;

		CL_MoveToOnHandList( p );
		break;
	}

	if ( cls.state == ca_disconnected )
		return;

	if ( cl.resourcesneeded.pNext == &cl.resourcesneeded )
	{
		byte		data[20480];
		sizebuf_t	msg;

		Q_memset( &msg, 0, sizeof( msg ) );
		msg.buffername = "Resource Registration";
		msg.data = data;
		msg.cursize = 0;
		msg.maxsize = sizeof( data );

		if ( CL_PrecacheResources() )
		{
			CL_RegisterResources( &msg );
		}

		if ( msg.cursize > 0 )
		{
			Netchan_CreateFragments( false, &cls.netchan, &msg );
			Netchan_FragSend( &cls.netchan );
		}
	}

	if ( cls.netchan.tempbuffer )
	{
		Con_Printf( "CL_ProcessFile: Received a decal %s, but didn't find it in resources needed list!\n", pfilename );
		Mem_Free( cls.netchan.tempbuffer );
	}

	cls.netchan.tempbuffer = NULL;
	cls.netchan.tempbuffersize = 0;
}

/*
====================
CL_GetMessage

Handles recording and playback of demos, on top of NET_ code
====================
*/
int CL_GetMessage( void )
{
	if ( cls.demoplayback )
		return CL_ReadDemoMessage();

	if ( cls.passive )
		return NET_GetPacket( NS_MULTICAST );

	return NET_GetPacket( NS_CLIENT );
}

/*
=================
CL_ReadPackets

Updates the local time and reads/handles messages on client net connection.
=================
*/
void CL_ReadPackets( void )
{
	int		msg_count = 0;

	if ( cls.state == ca_dedicated )
		return;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	while ( CL_GetMessage() )
	{
		msg_count++;

		if ( msg_count > 250 && !cls.demoplayback )
			continue;

		//
		// remote command packet
		//
		if ( *(int *)net_message.data == -1 )
		{
			CL_ConnectionlessPacket();
			continue;
		}

		//
		// packet from server
		//
		if ( !cls.passive && !cls.demoplayback && !NET_CompareAdr( net_from, cls.netchan.remote_address ) )
			continue;

		if ( cls.passive && cls.state == ca_connecting )
		{
			cls.state = ca_connected;
		}

		if ( cls.state == ca_disconnected || cls.state == ca_connecting )
			continue;

		if ( !cls.demoplayback )
		{
			if ( net_message.cursize < 8 )
			{
				Con_Printf( "CL_ReadPackets:  undersized packet %i bytes from %s\n", net_message.cursize, NET_AdrToString( net_from ) );
				continue;
			}

			if ( !Netchan_Process( &cls.netchan ) )
				continue;		// wasn't accepted for some reason
		}
		else
		{
			MSG_BeginReading();
		}

		CL_ParseServerMessage( true );
	}

	if ( msg_count > 250 && !cls.demoplayback )
	{
		Con_DPrintf( "Ignored %i messages\n", msg_count - 250 );
	}

	CL_SetSolidEntities();

	// Check for fragmentation/reassembly related packets.
	if ( cls.state != ca_dedicated &&
		cls.state != ca_disconnected &&
		Netchan_IncomingReady( &cls.netchan ) )
	{
		// Process the incoming buffer(s)
		if ( Netchan_CopyNormalFragments( &cls.netchan ) )
		{
			MSG_BeginReading();
			CL_ParseServerMessage( false );
		}

		if ( Netchan_CopyFileFragments( &cls.netchan ) )
		{
			if ( gDownloadFile[0] )
			{
				if ( gDownloadFile[0] == '!' )
				{
					SetSecondaryProgressBarText( "#GameUI_SecurityModule" );
				}
				else
				{
					char	buf[128];

					_snprintf( buf, sizeof( buf ), "File '%s'", gDownloadFile );
					SetSecondaryProgressBarText( buf );
				}

				SetSecondaryProgressBar( 100.0f );
			}

			// Remove from resource request stuff.
			CL_ProcessFile( true, cls.netchan.incomingfilename );
		}
	}

	//
	// check timeout
	//
	if ( cls.state >= ca_connected && !cls.demoplayback )
	{
		if ( ( realtime - cls.netchan.last_received ) > cl_timeout.value )
		{
			COM_ExplainDisconnection( true, "#GameUI_ServerConnectionTimeout" );
			CL_Disconnect();
			return;
		}
	}

	if ( cl_shownet.value )
	{
		Con_Printf( "\n" );
	}

	Netchan_UpdateProgress( &cls.netchan );

	if ( scr_downloading.value > 0 && gDownloadFile[0] )
	{
		char	buf[128];

		if ( gDownloadFile[0] == '!' )
		{
			_snprintf( buf, sizeof( buf ), "#GameUI_DownloadingSecurityModule" );
		}
		else
		{
			_snprintf( buf, sizeof( buf ), "Downloading file '%s'", gDownloadFile );
		}

		SetSecondaryProgressBar( scr_downloading.value / 100.0f );
		SetSecondaryProgressBarText( buf );
		SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	}

	Net_APICheckTimeouts();
}

/*
==================
CL_AddResource

==================
*/
resource_t *CL_AddResource( resourcetype_t type, char *name, int size, qboolean bFatalIfMissing, int index )
{
	resource_t	*r;

	r = &cl.resourcelist[cl.num_resources];

	if ( cl.num_resources >= MAX_RESOURCE_LIST )
	{
		Sys_Error( "Too many resources on client." );
	}

	cl.num_resources++;

	r->type = type;
	Q_strncpy( r->szFileName, name, sizeof( r->szFileName ) - 1 );
	r->szFileName[sizeof( r->szFileName ) - 1] = 0;
	r->nDownloadSize = size;
	r->ucFlags |= bFatalIfMissing ? RES_FATALIFMISSING : 0;
	r->nIndex = index;

	return r;
}

/*
==================
CL_CreateResourceList

==================
*/
void CL_CreateResourceList( void )
{
	FileHandle_t	fp;
	int				nSize;
	char			szFileName[MAX_PATH];
	unsigned char	rgucMD5_hash[16];
	resource_t		*pNewResource;

	if ( cls.state == ca_dedicated )
		return;

	HPAK_FlushHostQueue();

	cl.num_resources = 0;

	_snprintf( szFileName, sizeof( szFileName ), "tempdecal.wad" );

	Q_memset( rgucMD5_hash, 0, sizeof( rgucMD5_hash ) );

	fp = FS_Open( szFileName, "rb" );
	if ( !fp )
	{
		nSize = 0;
	}
	else
	{
		nSize = FS_Size( fp );
		MD5_Hash_File( rgucMD5_hash, szFileName, false, false, NULL );
	}

	if ( nSize )
	{
		pNewResource = CL_AddResource( t_decal, szFileName, nSize, false, 0 );
		if ( pNewResource )
		{
			pNewResource->ucFlags |= RES_CUSTOM;
			Q_memcpy( pNewResource->rgucMD5_hash, rgucMD5_hash, 16 );
			HPAK_AddLump( false, "custom.hpk", pNewResource, NULL, fp );
		}
	}

	if ( fp )
	{
		FS_Close( fp );
	}
}

/*
==================
CL_ClearCaches

==================
*/
void CL_ClearCaches( void )
{
	int		i;

	for ( i = 1; i < MAX_EVENTS; i++ )
	{
		if ( !cl.event_precache[i].pszScript )
			break;

		Mem_Free( (char *)cl.event_precache[i].pszScript );
		Mem_Free( (char *)cl.event_precache[i].filename );

		Q_memset( &cl.event_precache[i], 0, sizeof( event_t ) );
	}
}

/*
==================
CL_ClearClientState

==================
*/
void CL_ClearClientState( void )
{
	int					i;
	packet_entities_t	*pClientPack;

	for ( i = 0; i < CL_UPDATE_BACKUP; i++ )
	{
		pClientPack = &cl.frames[i].packet_entities;

		if ( pClientPack->entities )
		{
			Mem_Free( pClientPack->entities );
		}

		pClientPack->entities = NULL;
		pClientPack->num_entities = 0;
	}

	CL_ClearResourceLists();

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		COM_ClearCustomizationList( &cl.players[i].customdata, false );
	}

	CL_ClearCaches();

	Q_memset( &cl, 0, sizeof( cl ) );

	cl.resourcesneeded.pNext = cl.resourcesneeded.pPrev = &cl.resourcesneeded;
	cl.resourcesonhand.pNext = cl.resourcesonhand.pPrev = &cl.resourcesonhand;

	CL_CreateResourceList();
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState( qboolean bQuiet )
{
	int		i;

	if ( !Host_IsServerActive() )
	{
		Host_ClearMemory( bQuiet );
	}

	CL_ClearClientState();

	SZ_Clear( &cls.netchan.message );

	// clear other arrays
	Q_memset( cl_efrags, 0, sizeof( cl_efrags ) );
	Q_memset( cl_dlights, 0, sizeof( cl_dlights ) );
	Q_memset( cl_elights, 0, sizeof( cl_elights ) );
	Q_memset( cl_lightstyle, 0, sizeof( cl_lightstyle ) );

	CL_TempEntInit();

	//
	// allocate the efrags and chain together into a free list
	//
	cl.free_efrags = cl_efrags;
	for ( i = 0; i < MAX_EFRAGS - 1; i++ )
	{
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	}
	cl.free_efrags[i].entnext = NULL;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect( void )
{
	unsigned char	final[20];
	unsigned int	unIP;

	cls.connect_time = -99999;
	cls.connect_retry = 0;

	g_careerState = CAREER_NONE;

	SetCareerAudioState( false );

	SPR_Shutdown_NoModelFree();

	// stop sounds (especially looping!)
	S_StopAllSounds( true );

	// if running a local server, shut it down
	if ( cls.demoplayback )
	{
		CL_StopPlayback();
	}
	else if ( cls.state == ca_active || cls.state == ca_connected || cls.state == ca_uninitialized || cls.state == ca_connecting )
	{
		if ( cls.demorecording )
		{
			CL_Stop_f();
		}

		FS_LogLevelLoadStarted( "ExitGame" );

		if ( !cls.passive )
		{
			// send a disconnect message to the server
			if ( cls.netchan.remote_address.type != NA_UNUSED )
			{
				final[0] = clc_stringcmd;
				Q_strcpy( (char *)final + 1, "dropclient\n" );
				Netchan_Transmit( &cls.netchan, 13, final );
				Netchan_Transmit( &cls.netchan, 13, final );
				Netchan_Transmit( &cls.netchan, 13, final );
			}
		}
		else
		{
			NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
			NET_LeaveGroup( cls.netchan.sock, cls.game_stream );
		}

		cls.state = ca_disconnected;

		unIP = *(unsigned int *)g_GameServerAddress.ip;
		if ( g_GameServerAddress.type == NA_LOOPBACK )
		{
			unIP = *(unsigned int *)net_local_adr.ip;
		}

		Steam_GSTerminateGameConnection( unIP, g_GameServerAddress.port );

		Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );

		if ( Host_IsServerActive() )
		{
			Host_ShutdownServer( false );
		}

		CloseSecurityModule();
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;

	if ( cls.waitingforresources_guess )
	{
		FS_CancelWaitForResources_guess( cls.waitforresources_guess );
		cls.waitingforresources_guess = false;
	}

	CL_ClearState( true );

	Netchan_Clear( &cls.netchan );

	CL_DeallocateDynamicData();

	scr_downloading.value = -1.0f;
	sys_timescale.value = 1.0f;

	g_LastScreenUpdateTime = 0.0f;

	VGuiWrap2_NotifyOfServerDisconnect();
	StopLoadingProgressBar();

	ResetLevelState_guess();
}

/*
=====================
CL_Disconnect_f

=====================
*/
void CL_Disconnect_f( void )
{
	CL_Disconnect();

	if ( Host_IsServerActive() )
	{
		Host_ShutdownServer( false );
	}
}

/*
=====================
CL_ShutDownClientStatic

=====================
*/
void CL_ShutDownClientStatic( void )
{
	int					i;
	packet_entities_t	*pClientPack;

	for ( i = 0; i < CL_UPDATE_BACKUP; i++ )
	{
		pClientPack = &cl.frames[i].packet_entities;

		if ( pClientPack->entities )
		{
			Mem_Free( pClientPack->entities );
		}

		pClientPack->entities = NULL;
	}

	Q_memset( cl.frames, 0, CL_UPDATE_BACKUP * sizeof( frame_t ) );
}

/*
=====================
CL_Retry_f

=====================
*/
void CL_Retry_f( void )
{
	char	szCommand[MAX_PATH];

	if ( !cls.servername || !cls.servername[0] )
	{
		Con_Printf( "Can't retry, no previous connection\n" );
		return;
	}

	if ( cls.passive )
	{
		_snprintf( szCommand, sizeof( szCommand ), "listen %s\n", cls.servername );
	}
	else
	{
		_snprintf( szCommand, sizeof( szCommand ), "connect %s\n", cls.servername );
	}

	Cbuf_AddText( szCommand );

	Con_Printf( "Commencing connection retry to %s\n", cls.servername );
}

/*
=====================
CL_GetCDKeyHash

Connections will now use a hashed cd key value
=====================
*/
char *CL_GetCDKeyHash( void )
{
	char			szKeyBuffer[256];
	static char		szHashedKeyBuffer[256];
	int				nKeyLength;
	int				bDedicated;
	MD5Context_t	ctx;
	unsigned char	digest[16];

	nKeyLength = 64;
	bDedicated = 0;

	Sys_GetCDKey( szKeyBuffer, &nKeyLength, &bDedicated );

	if ( bDedicated )
	{
		Con_Printf( "Key has no meaning on dedicated server...\n" );
		return "";
	}

	if ( nKeyLength <= 0 || nKeyLength >= 256 )
	{
		Con_Printf( "Bogus key length on CD Key...\n" );
		return "";
	}

	szKeyBuffer[nKeyLength] = 0;

	Q_memset( &ctx, 0, sizeof( ctx ) );
	Q_memset( digest, 0, sizeof( digest ) );

	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)szKeyBuffer, nKeyLength );
	MD5Final( digest, &ctx );

	Q_memset( szHashedKeyBuffer, 0, 256 );
	Q_strncpy( szHashedKeyBuffer, MD5_Print( digest ), sizeof( szHashedKeyBuffer ) - 1 );
	szHashedKeyBuffer[sizeof( szHashedKeyBuffer ) - 1] = 0;

	return szHashedKeyBuffer;
}

/*
=====================
CL_ConnectTest

The body is gone in this build; only its locals remain.
=====================
*/
void CL_ConnectTest( void )
{
	double	disconnect_time;
	double	connect_time;
	double	time1_guess;
	double	time2_guess;
}

/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket( void )
{
	netadr_t		adr;
	char			data[2048];
	char			szServerName[MAX_PATH];
	int				len;
	unsigned char	buffer[1024];
	int				nCertificateSize = 0;
	char			protinfo[1024];
	qboolean		steam = false;
	char			rgchSteam3LoginCookie[1024];
	int				cbLogonBlob = 0;

	Q_memset( protinfo, 0, sizeof( protinfo ) );

	ContinueLoadingProgressBar( "ClientConnect", 1, 0.0 );

	if ( fs_startup_timings.value )
	{
		SetupStartupTimings();
	}

	Q_strncpy( szServerName, cls.servername, sizeof( szServerName ) );
	szServerName[sizeof( szServerName ) - 1] = 0;

	if ( !Q_stricmp( cls.servername, "local" ) )
	{
		_snprintf( szServerName, sizeof( szServerName ), "%s", "localhost" );
	}

	if ( !NET_StringToAdr( szServerName, &adr ) )
	{
		Con_Printf( "Unable to resolve %s\n", szServerName );
		cls.connect_time = -99999;
		cls.connect_retry = 0;
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );
		return;
	}

	if ( !adr.port )
	{
		adr.port = BigShort( Q_atoi( "27015" ) );
	}

	switch ( cls.authprotocol )
	{
	default:
		cls.authprotocol = 2;
		// fall through
	case 2:
		len = Q_strlen( CL_GetCDKeyHash() );
		Q_strncpy( (char *)buffer, CL_GetCDKeyHash(), sizeof( buffer ) - 1 );
		buffer[sizeof( buffer ) - 1] = 0;
		break;
	case 3:
		steam = true;
		break;
	}

	Info_SetValueForKey( protinfo, "prot", va( "%i", cls.authprotocol ), sizeof( protinfo ) );

	if ( steam )
	{
		Info_SetValueForKey( protinfo, "unique", va( "%i", -1 ), sizeof( protinfo ) );
		Info_SetValueForKey( protinfo, "raw", "steam", sizeof( protinfo ) );
	}
	else
	{
		Info_SetValueForKey( protinfo, "raw", (char *)buffer, sizeof( protinfo ) );
	}

	if ( cls.authprotocol != 2 )
	{
		Info_SetValueForKey( protinfo, "cdkey", CL_GetCDKeyHash(), sizeof( protinfo ) );
	}

	Q_memset( rgchSteam3LoginCookie, 0, sizeof( rgchSteam3LoginCookie ) );

	if ( steam )
	{
		unsigned int	unIP;

		unIP = *(unsigned int *)adr.ip;
		if ( adr.type == NA_LOOPBACK )
		{
			unIP = *(unsigned int *)net_local_adr.ip;
		}

		cbLogonBlob = Steam_GSInitiateGameConnection( rgchSteam3LoginCookie, sizeof( rgchSteam3LoginCookie ), cls.GameServerSteamID, unIP, adr.port, cls.isVAC2Secure );
	}

	cls.connect_time = realtime;	// for retransmit requests

	_snprintf( data, sizeof( data ), "%c%c%c%cconnect %i %i \"%s\" \"%s\"\n", 255, 255, 255, 255, PROTOCOL_VERSION, cls.challenge, protinfo, cls.userinfo );

	len = Q_strlen( data );

	if ( steam )
	{
		if ( len + cbLogonBlob > sizeof( data ) )
		{
			Con_Printf( "CL_SendConnectPacket:  Potential buffer overrun (%i/%i>%i) copying steamkeybuffer into connection packet\n", len, cbLogonBlob, sizeof( data ) );
			CL_Disconnect_f();
			return;
		}

		memcpy( &data[len], rgchSteam3LoginCookie, cbLogonBlob );
		len += cbLogonBlob;
	}

	g_GameServerAddress = adr;

	NET_SendPacket( NS_CLIENT, len, data, adr );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend( void )
{
	netadr_t	adr;
	char		data[2048];
	char		szServerName[MAX_PATH];

	CL_ConnectTest();

	// if the local server is running and we aren't then connect
	if ( cls.state == ca_disconnected && Host_IsServerActive() )
	{
		cls.state = ca_connecting;
		gfExtendedError = false;
		Q_strncpy( cls.servername, "localhost", sizeof( cls.servername ) - 1 );
		cls.servername[sizeof( cls.servername ) - 1] = 0;
		cls.authprotocol = 3;
		// we don't need a challenge on the localhost
		CL_SendConnectPacket();
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != ca_connecting )
		return;

	if ( cls.passive && ( realtime - cls.connect_time ) > 30.0 )
	{
		if ( cls.challenge )
		{
			Con_Printf( "Multicast failed. Falling back to unicast...\n" );
			cls.passive = false;
			NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
			CL_SendConnectPacket();
		}
		else
		{
			COM_ExplainDisconnection( true, "Joining multicast group failed after %.0f seconds.", 30.0 );
			cls.connect_time = -99999;
			cls.connect_retry = 0;
			cls.state = ca_disconnected;
			Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );
			cls.passive = false;
			NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
		}
		return;
	}

	if ( cls.passive )
		return;

	if ( cl_resend.value < 1.5f )
	{
		Cvar_SetValue( "cl_resend", 1.5f );
	}
	else if ( cl_resend.value > 20.0f )
	{
		Cvar_SetValue( "cl_resend", 20.0f );
	}

	if ( ( realtime - cls.connect_time ) < cl_resend.value )
		return;

	Q_strncpy( szServerName, cls.servername, sizeof( szServerName ) );
	szServerName[sizeof( szServerName ) - 1] = 0;

	if ( !Q_stricmp( cls.servername, "local" ) )
	{
		_snprintf( szServerName, sizeof( szServerName ), "%s", "localhost" );
	}

	if ( !NET_StringToAdr( szServerName, &adr ) )
	{
		Con_Printf( "Bad server address\n" );
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );
		return;
	}

	if ( !adr.port )
	{
		adr.port = BigShort( Q_atoi( "27015" ) );
	}

	if ( cls.connect_retry >= 4 )
	{
		COM_ExplainDisconnection( true, "#GameUI_CouldNotContactGameServer" );
		cls.connect_time = -99999;
		cls.connect_retry = 0;
		cls.state = ca_disconnected;
		Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );
		CL_Disconnect();
		return;
	}

	cls.connect_time = realtime;	// for retransmit requests

	if ( Q_stricmp( szServerName, "localhost" ) )
	{
		if ( cls.connect_retry == 0 )
		{
			Con_Printf( "Connecting to %s...\n", szServerName );
		}
		else
		{
			switch ( cls.connect_retry )
			{
			default:
				SetLoadingProgressBarStatusText( "#GameUI_RetryingConnectionToServer" );
				break;
			case 2:
				SetLoadingProgressBarStatusText( "#GameUI_RetryingConnectionToServer2" );
				break;
			case 3:
				SetLoadingProgressBarStatusText( "#GameUI_RetryingConnectionToServer3" );
				break;
			}

			Con_Printf( "Retrying %s...\n", szServerName );
		}
	}

	cls.connect_retry++;

	_snprintf( data, sizeof( data ), "%c%c%c%cgetchallenge steam\n", 255, 255, 255, 255 );

	g_GameServerAddress = adr;

	NET_SendPacket( NS_CLIENT, Q_strlen( data ), data, adr );
}

/*
================
CL_Connect_f

User command to connect to server
================
*/
void CL_Connect_f( void )
{
	char	*server;
	char	name[MAX_PATH];
	int		num;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "usage: connect <server>\n" );
		return;
	}

	SetCStrikeFlags();

	if ( ( g_bIsCStrike || g_bIsCZero ) && cmd_source != src_command )
	{
		Con_Printf( "Connect only works from the console.\n" );
		return;
	}

	server = Cmd_Args();
	if ( !server )
		return;

	Q_strncpy( name, server, sizeof( name ) );
	name[sizeof( name ) - 1] = 0;

	if ( cls.demoplayback )
	{
		CL_StopPlayback();
	}

	{
		CareerStateType	careerState = g_careerState;

		CL_Disconnect();

		if ( careerState == CAREER_LOADING )
		{
			g_careerState = CAREER_LOADING;
		}
	}

	StartLoadingProgressBar( "Connecting", 12 );
	SetLoadingProgressBarStatusText( "#GameUI_EstablishingConnection" );

	num = Q_atoi( name );	// In case it's an index.

	if ( !Q_strstr( name, "." ) && num > 0 && num <= num_servers )
	{
		Q_strncpy( name, NET_AdrToString( cached_servers[num - 1].adr ), sizeof( name ) );
		name[sizeof( name ) - 1] = 0;
	}

	Q_memset( msg_buckets, 0, sizeof( msg_buckets ) );
	Q_memset( total_data, 0, sizeof( total_data ) );

	Q_strncpy( cls.servername, name, sizeof( cls.servername ) - 1 );
	cls.servername[sizeof( cls.servername ) - 1] = 0;

	cls.state = ca_connecting;
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	cls.connect_retry = 0;

	cls.passive = false;
	cls.spectator = false;
	cls.isVAC2Secure = false;
	cls.GameServerSteamID = 0;

	Q_memset( &g_GameServerAddress, 0, sizeof( netadr_t ) );

	cls.challenge = 0;
	gfExtendedError = false;
	g_LastScreenUpdateTime = 0.0f;

	if ( Q_strnicmp( cls.servername, "local", 5 ) )
	{
		NET_Config( true );		// allow remote
	}
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply( void )
{
	char	mapname[256];
	int		l;

	Con_DPrintf( "CL_SignonReply: %i\n", cls.signon );

	switch ( cls.signon )
	{
	case 1:
		Cache_Report();		// print remaining memory

		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		MSG_WriteString( &cls.netchan.message, "sendents" );
		break;

	case 2:
		LevelLoaded_guess( cl.worldmodel );

		SCR_EndLoadingPlaque();		// allow normal screen updates

		sscanf( cl.levelname, "maps/%s.bsp", mapname );
		l = strlen( mapname );
		if ( l > 4 && !strcmp( &mapname[l - 4], ".bsp" ) )
		{
			mapname[l - 4] = 0;
		}

		FS_LogLevelLoadFinished( mapname );
		VGuiWrap2_LoadingFinished( "level", mapname );

		if ( g_bRedirectedToProxy && !cls.spectator )
		{
			Con_Printf( "Redirected to invalid server\n" );
			CL_Disconnect_f();
			g_bRedirectedToProxy = false;
		}

		g_bRedirectedToProxy = false;

		if ( cls.demoplayback )
		{
			Sequence_OnLevelLoad( mapname );
		}
		break;
	}
}

/*
==============
CL_PrintEntities_f

==============
*/
void CL_PrintEntities_f( void )
{
	cl_entity_t	*ent;
	int			i;

	for ( i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++ )
	{
		Con_Printf( "%3i:", i );

		if ( !ent->model )
			continue;

		Con_Printf( "%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
			ent->model->name, ent->curstate.frame,
			ent->origin[0], ent->origin[1], ent->origin[2],
			ent->angles[0], ent->angles[1], ent->angles[2] );
	}
}

/*
==============
CL_WaitForResources_guess

Starts the Steam resource wait for the map being loaded (called by
CL_ParseServerInfo).
==============
*/
void CL_WaitForResources_guess( void )
{
	char	*pBsp_guess = NULL;
	char	mapname_guess[MAX_PATH];
	char	szFilename_guess[512];

	cls.waitingforresources_guess = true;

	mapname_guess[0] = 0;
	sscanf( cl.levelname, "maps/%s.bsp", mapname_guess );

	pBsp_guess = strstr( mapname_guess, ".bsp" );
	if ( pBsp_guess )
	{
		*pBsp_guess = 0;
	}

	_snprintf( szFilename_guess, sizeof( szFilename_guess ), "reslists/%s/%s.lst", GetCurrentSteamAppName(), mapname_guess );

	cls.waitforresources_guess = FS_WaitForResources_guess( szFilename_guess );
	cls.waitforresourcestime_guess = Sys_FloatTime();
}

/*
==============
CL_WaitForResourcesDone_guess

==============
*/
void CL_WaitForResourcesDone_guess( void )
{
	char	szCommand_guess[20];

	if ( developer.value )
	{
		double	dCurTime_guess;
		float	flElapsed_guess;

		Con_DPrintf( "SteamWaitForResources() time: %.3f seconds\n", flElapsed_guess = ( dCurTime_guess = Sys_FloatTime() ) - cls.waitforresourcestime_guess );
	}

	szCommand_guess[0] = 's';
	szCommand_guess[1] = 'e';
	szCommand_guess[2] = 'n';
	szCommand_guess[3] = 'd';
	szCommand_guess[4] = 'r';
	szCommand_guess[5] = 'e';
	szCommand_guess[6] = 's';
	szCommand_guess[7] = 0;

	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, szCommand_guess );

	cls.waitingforresources_guess = false;
}

/*
==============
CL_CheckWaitForResources_guess

Called every frame by _Host_Frame.
==============
*/
void CL_CheckWaitForResources_guess( void )
{
	float		flProgress_guess;
	qboolean	complete_guess;

	if ( !cls.waitingforresources_guess )
		return;

	if ( FS_GetWaitForResourcesProgress_guess( cls.waitforresources_guess, &flProgress_guess, &complete_guess ) )
	{
		SetSecondaryProgressBarText( "#GameUI_UpdatingSteamResources" );
		SetSecondaryProgressBar( flProgress_guess );
		SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	}

	if ( complete_guess )
	{
		SetSecondaryProgressBar( 100.0f );
		CL_WaitForResourcesDone_guess();
	}
	else
	{
		CL_KeepConnectionActive();
	}
}

/*
==============
CL_TakeSnapshot_f

==============
*/
void CL_TakeSnapshot_f( void )
{
	int				i = 0, found = 0;
	char			base[64];
	char			filename[64];
	FileHandle_t	fp = NULL;

	if ( cl.num_entities && cl_entities[0].model )
	{
		COM_FileBase( cl_entities[0].model->name, base );
	}
	else
	{
		Q_strcpy( base, "Snapshot" );
	}

	for ( i = 0; i < 1000; i++ )
	{
		_snprintf( filename, sizeof( filename ), "%s%04d.bmp", base, i );

		fp = FS_Open( filename, "r" );
		if ( !fp )
		{
			found = 1;
			break;
		}

		FS_Close( fp );
	}

	if ( found )
	{
		VID_TakeSnapshot( filename );
	}
	else
	{
		Con_Printf( "Unable to take a screenshot.\n" );
	}
}

/*
==============
CL_StartMovie_f

==============
*/
void CL_StartMovie_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "startmovie <filename> <fps>\n" );
		return;
	}

	if ( !cls.demoplayback )
	{
		Con_Printf( "startmovie only during demo playback allowed.\n" );
		return;
	}

	host_framerate.value = 1.0f / Q_atof( Cmd_Argv( 2 ) );

	cl_inmovie = true;

	VID_WriteBuffer( Cmd_Argv( 1 ) );

	Con_Printf( "Started recording movie...\n" );
}

/*
==============
CL_EndMovie_f

==============
*/
void CL_EndMovie_f( void )
{
	if ( !cl_inmovie )
	{
		Con_Printf( "No movie started.\n" );
		return;
	}

	cl_inmovie = false;
	host_framerate.value = 0;

	Con_Printf( "Stopped recording movie...\n" );
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f( void )
{
	netadr_t		to;
	unsigned short	port;

	if ( !rcon_password.string )
	{
		Con_Printf( "You must set 'rcon_password' before\nissuing an rcon command.\n" );
		return;
	}

	if ( !Cmd_Args() )
	{
		Con_Printf( "Rcon:  Empty rcon string\n" );
		return;
	}

	if ( cls.state >= ca_connected )
	{
		if ( Q_strlen( rcon_address.string ) )
		{
			if ( !NET_StringToAdr( rcon_address.string, &to ) )
			{
				Con_Printf( "Unable to resolve rcon address %s\n", rcon_address.string );
				return;
			}
		}
		else
		{
			to = cls.netchan.remote_address;
		}
	}
	else
	{
		NET_Config( true );		// allow remote

		if ( !Q_strlen( rcon_address.string ) )
		{
			Con_Printf( "You must either be connected,\nor set the 'rcon_address' cvar\nto issue rcon commands\n" );
			return;
		}

		if ( !NET_StringToAdr( rcon_address.string, &to ) )
		{
			Con_Printf( "Unable to resolve rcon address %s\n", rcon_address.string );
			return;
		}
	}

	port = rcon_port.value;
	if ( !port )
	{
		if ( cls.state >= ca_connected )
		{
			port = BigShort( cls.netchan.remote_address.port );
		}
		else
		{
			port = Q_atoi( "27015" );
		}
	}

	to.port = BigShort( port );

	Q_strncpy( g_lastrconcommand, Cmd_Args(), sizeof( g_lastrconcommand ) - 1 );
	g_lastrconcommand[sizeof( g_lastrconcommand ) - 1] = 0;

	g_rconaddress = to;

	Netchan_OutOfBandPrint( NS_CLIENT, to, "challenge rcon\n" );
}

/*
==================
SetPal

Debugging tool, just flashes the screen
==================
*/
void SetPal( int i )
{
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight( int key )
{
	int			i;
	dlight_t	*dl;

	// first look for an exact key match
	if ( key )
	{
		dl = cl_dlights;
		for ( i = 0; i < MAX_DLIGHTS; i++, dl++ )
		{
			if ( dl->key == key )
			{
				Q_memset( dl, 0, sizeof( *dl ) );
				dl->key = key;
				r_dlightchanged |= 1 << i;
				r_dlightactive |= 1 << i;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for ( i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if ( dl->die < cl.time )
		{
			Q_memset( dl, 0, sizeof( *dl ) );
			dl->key = key;
			r_dlightchanged |= 1 << i;
			r_dlightactive |= 1 << i;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	Q_memset( dl, 0, sizeof( *dl ) );
	dl->key = key;
	r_dlightchanged |= 1;
	r_dlightactive |= 1;
	return dl;
}

/*
===============
CL_AllocElight

===============
*/
dlight_t *CL_AllocElight( int key )
{
	int			i;
	dlight_t	*el;

	// first look for an exact key match
	if ( key )
	{
		el = cl_elights;
		for ( i = 0; i < MAX_ELIGHTS; i++, el++ )
		{
			if ( el->key == key )
			{
				Q_memset( el, 0, sizeof( *el ) );
				el->key = key;
				return el;
			}
		}
	}

	// then look for anything else
	el = cl_elights;
	for ( i = 0; i < MAX_ELIGHTS; i++, el++ )
	{
		if ( el->die < cl.time )
		{
			Q_memset( el, 0, sizeof( *el ) );
			el->key = key;
			return el;
		}
	}

	el = &cl_elights[0];
	Q_memset( el, 0, sizeof( *el ) );
	el->key = key;
	return el;
}

/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights( void )
{
	int			i;
	dlight_t	*dl;
	float		time;

	if ( cls.signon != SIGNONS )
		return;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;

	r_dlightchanged = 0;
	r_dlightactive = 0;

	for ( i = 0; i < MAX_DLIGHTS; i++, dl++ )
	{
		if ( dl->radius )
		{
			if ( dl->die < cl.time )
			{
				r_dlightchanged |= 1 << i;
				dl->radius = 0;
			}
			else if ( dl->decay )
			{
				r_dlightchanged |= 1 << i;

				dl->radius -= time * dl->decay;
				if ( dl->radius < 0 )
					dl->radius = 0;
			}

			if ( dl->radius )
			{
				r_dlightactive |= 1 << i;
			}
		}
	}

	dl = cl_elights;
	for ( i = 0; i < MAX_ELIGHTS; i++, dl++ )
	{
		if ( !dl->radius )
			continue;

		if ( dl->die < cl.time )
		{
			dl->radius = 0;
			continue;
		}

		dl->radius -= time * dl->decay;
		if ( dl->radius < 0 )
			dl->radius = 0;
	}
}

/*
===============
CL_ComputePacketLoss

===============
*/
void CL_ComputePacketLoss( void )
{
	int			i;
	int			frm;
	frame_t		*frame;
	int			count = 0;
	int			lost = 0;

	if ( realtime < cls.packet_loss_recalc_time )
		return;

	// recalc every second
	cls.packet_loss_recalc_time = realtime + 1.0;

	// Compuate packet loss
	for ( i = cls.netchan.incoming_sequence - CL_UPDATE_BACKUP + 1; i <= cls.netchan.incoming_sequence; i++ )
	{
		frm = i;
		frame = &cl.frames[frm & CL_UPDATE_MASK];

		if ( frame->receivedtime == -1.0 )
		{
			lost++;
		}

		count++;
	}

	if ( count <= 0 )
	{
		cls.packet_loss = 0.0;
	}
	else
	{
		cls.packet_loss = lost * 100.0 / count;
	}
}

static float	g_flInterpolationAmount = 0.1f;

/*
===============
CL_DriftInterpolationAmount

===============
*/
int CL_DriftInterpolationAmount( int goal )
{
	int		msec;
	float	fgoal;

	fgoal = goal / 1000.0f;
	if ( fgoal != g_flInterpolationAmount )
	{
		float	maxmove;
		float	diff;

		maxmove = 0.05f * host_frametime;

		diff = fgoal - g_flInterpolationAmount;
		if ( diff > 0 )
		{
			diff = min( maxmove, diff );
		}
		else
		{
			diff = -min( maxmove, -diff );
		}

		g_flInterpolationAmount += diff;
	}

	msec = 1000.0f * g_flInterpolationAmount;
	msec = max( 0, msec );
	msec = min( 100, msec );

	return msec;
}

/*
===============
CL_ComputeClientInterpolationAmount

===============
*/
void CL_ComputeClientInterpolationAmount( usercmd_t *cmd )
{
	float		minupdaterate = 10.0f;
	float		maxupdaterate = 102.0f;
	int			minexinterp = 50;
	int			maxexinterp = 100;
	int			interpolation_msec;
	qboolean	forced = false;

	if ( cl_updaterate.value < minupdaterate )
	{
		Cvar_Set( "cl_updaterate", "20" );
		Con_Printf( "cl_updaterate minimum is %f, resetting to default (20)\n", minupdaterate );
	}

	if ( cl_updaterate.value > maxupdaterate )
	{
		Cvar_Set( "cl_updaterate", "20" );
		Con_Printf( "cl_updaterate maximum is %f, resetting to default (20)\n", maxupdaterate );
	}

	if ( cls.spectator )
	{
		maxexinterp = max( 200.0f, maxexinterp );
	}

	if ( cl_updaterate.value > 0 )
	{
		minexinterp = 1000.0f / cl_updaterate.value;
		minexinterp = max( 1, minexinterp );
	}

	interpolation_msec = 1000.0f * ex_interp.value;

	if ( interpolation_msec + 1 < minexinterp )
	{
		interpolation_msec = minexinterp;
		forced = true;
		Con_Printf( "ex_interp forced up to %i msec\n", interpolation_msec );
	}
	else if ( interpolation_msec - 1 > maxexinterp )
	{
		interpolation_msec = maxexinterp;
		forced = true;
		Con_Printf( "ex_interp forced down to %i msec\n", interpolation_msec );
	}

	if ( forced )
	{
		float	interpolation_sec;

		interpolation_sec = interpolation_msec / 1000.0f;
		Cvar_Set( "ex_interp", va( "%f", interpolation_sec ) );
	}

	interpolation_msec = max( minexinterp, interpolation_msec );
	interpolation_msec = min( maxexinterp, interpolation_msec );

	cmd->lerp_msec = CL_DriftInterpolationAmount( interpolation_msec );
}

/*
===============
UserIsConnectedOnLoopback

===============
*/
qboolean UserIsConnectedOnLoopback( void )
{
	return cls.netchan.remote_address.type == NA_LOOPBACK;
}

/*
=================
CL_Move

Constructs the movement command and sends it to the server
=================
*/
void CL_Move( void )
{
	sizebuf_t	buf;
	byte		data[2048];
	int			i;
	usercmd_t	*cmd;
	usercmd_t	*from, *to;
	usercmd_t	cmdbaseline;
	int			checksumposition;
	int			numbackup = 2;
	int			numcmds;
	int			newcmds;
	int			cmdnumber;
	cmd_t		*pcmd;
	int			active = 0;
	int			packet_loss = 0;
	int			frame_msec_guess;

	if ( cls.state == ca_dedicated || cls.state == ca_disconnected || cls.state == ca_connecting )
		return;

	if ( cls.state == ca_connected )
	{
		if ( !cls.passive )
		{
			Netchan_Transmit( &cls.netchan, 0, NULL );
		}
		return;
	}

	CL_ComputePacketLoss();

	numbackup = cl_cmdbackup.value;
	if ( numbackup < 0 )
	{
		numbackup = 0;
	}
	else if ( numbackup > 8 )
	{
		numbackup = 8;
	}

	buf.buffername = "Client Move";
	buf.maxsize = sizeof( data );
	buf.cursize = 0;
	buf.flags = 0;
	buf.data = data;

	i = cls.netchan.outgoing_sequence & CL_UPDATE_MASK;
	pcmd = &cl.commands[i];

	pcmd->senttime = realtime;
	pcmd->receivedtime = -1;
	pcmd->processedfuncs = false;
	pcmd->heldback = false;
	pcmd->sendsize = 0;

	cmd = &pcmd->cmd;
	Q_memset( cmd, 0, sizeof( *cmd ) );

	active = cls.signon == SIGNONS;

	CL_SetSolidEntities();
	CL_PushPMStates();
	CL_SetSolidPlayers( cl.playernum );

	// allow mice or other external controllers to add to the move
	ClientDLL_CreateMove( (float)host_frametime, cmd, active );

	CL_PopPMStates();

	CL_ComputeClientInterpolationAmount( cmd );

	cmd->lightlevel = cl.light_level;

	//
	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	//
	MSG_WriteByte( &buf, clc_move );

	// Save the position because we'll calc a checksum later.
	checksumposition = buf.cursize + 1;

	// Write a placeholder for the size
	MSG_WriteByte( &buf, 0 );
	// Write a placeholder for the checksum
	MSG_WriteByte( &buf, 0 );

	packet_loss = cls.packet_loss;
	packet_loss = max( 0, packet_loss );
	packet_loss = min( 100, packet_loss );

	if ( Voice_GetLoopback() )
	{
		packet_loss |= 0x80;
	}

	MSG_WriteByte( &buf, packet_loss & 0xFF );

	frame_msec_guess = host_frametime * 1000.0;
	frame_msec_guess = min( 255.0, frame_msec_guess );

	cmd->msec = frame_msec_guess;

	cl.cmd = *cmd;

	if ( cls.passive || cls.demoplayback )
		return;

	CL_PredictMove( false );

	if ( cl_cmdrate.value < 10.0 )
	{
		Cvar_Set( "cl_cmdrate", "10" );
	}

	// Determine number of backup commands to send along
	if ( cls.spectator &&
		cls.state == ca_active &&
		cl.delta_sequence == cl.validsequence &&
		( !cls.demorecording || !cls.demowaiting ) &&
		cls.nextcmdtime + 1.0f > realtime )
		return;

	if ( cl.maxclients == 1 ||
		( cls.netchan.remote_address.type == NA_LOOPBACK && !host_limitlocal.value ) ||
		( cls.nextcmdtime <= realtime && Netchan_CanPacket( &cls.netchan ) ) )
	{
		int		outgoing_sequence;

		if ( cl_cmdrate.value > 0.0 )
		{
			cls.nextcmdtime = realtime + ( 1.0f / cl_cmdrate.value );
		}
		else
		{
			cls.nextcmdtime = realtime;
		}

		if ( cls.lastoutgoingcommand == -1 )
		{
			outgoing_sequence = cls.netchan.outgoing_sequence;
			cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;
		}
		else
		{
			outgoing_sequence = cls.lastoutgoingcommand + 1;
		}

		if ( cls.spectator )
		{
			SZ_Clear( &buf );
		}
		else
		{
			// Say how many backups we'll be sending
			MSG_WriteByte( &buf, numbackup );

			// How many real commands have queued up
			newcmds = cls.netchan.outgoing_sequence - cls.lastoutgoingcommand;

			// Put an upper bound on this
			if ( newcmds > 16 )
			{
				newcmds = 16;
			}

			MSG_WriteByte( &buf, newcmds );

			Q_memset( &cmdbaseline, 0, sizeof( cmdbaseline ) );
			from = &cmdbaseline;

			numcmds = newcmds + numbackup;

			for ( i = numcmds - 1; i >= 0; i-- )
			{
				cmdnumber = ( cls.netchan.outgoing_sequence - i ) & CL_UPDATE_MASK;

				to = &cl.commands[cmdnumber].cmd;

				MSG_WriteUsercmd( &buf, to, from );

				from = to;
			}

			// Calculate the checksum
			buf.data[checksumposition - 1] = min( buf.cursize - checksumposition - 1, 255 );

			buf.data[checksumposition] = COM_BlockSequenceCRCByte(
				buf.data + checksumposition + 1,
				buf.cursize - checksumposition - 1,
				cls.netchan.outgoing_sequence );

			COM_Munge( buf.data + checksumposition + 1, min( buf.cursize - checksumposition - 1, 255 ), cls.netchan.outgoing_sequence );
		}

		if ( cls.state == ca_active &&
			cls.signon == SIGNONS &&
			( cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged ) >= CL_UPDATE_MASK &&
			( realtime - cls.netchan.last_received ) > 15.0 )
		{
			con_nprint_t	np;

			np.time_to_live = 1.0f;
			np.index = 0;
			np.color[0] = 1.0f;
			np.color[1] = 0.2f;
			np.color[2] = 0.0f;
			Con_NXPrintf( &np, "WARNING:  Connection Problem" );

			cl.validsequence = 0;
		}

		// Request delta compression of entities
		if ( cl.validsequence &&
			!cl_nodelta.value &&
			cls.state == ca_active &&
			( !cls.demorecording || !cls.demowaiting ) )
		{
			cl.delta_sequence = cl.validsequence;

			MSG_WriteByte( &buf, clc_delta );
			MSG_WriteByte( &buf, cl.validsequence & 0xFF );
		}
		else
		{
			cl.delta_sequence = -1;
		}

		// Remember outgoing command that we are sending
		cls.lastoutgoingcommand = cls.netchan.outgoing_sequence;

		// Update size counter for netgraph
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].sendsize = buf.cursize;

		// Composite the rest of the datagram..
		CL_AddVoiceToDatagram( false );

		// Only send if there's room
		if ( cls.datagram.cursize <= buf.maxsize - buf.cursize )
		{
			Q_memcpy( buf.data + buf.cursize, cls.datagram.data, cls.datagram.cursize );
			buf.cursize += cls.datagram.cursize;
		}

		cls.datagram.cursize = 0;

		//
		// deliver the message
		//
		Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
	}
	else
	{
		// Mark command as held back so we'll send it next time
		cl.commands[cls.netchan.outgoing_sequence & CL_UPDATE_MASK].heldback = true;

		// Increment sequence number so we can detect that we've held back packets.
		cls.netchan.outgoing_sequence++;
	}

	// Update download/upload slider.
	Netchan_UpdateProgress( &cls.netchan );
}

/*
=====================
CL_BeginUpload_f

=====================
*/
void CL_BeginUpload_f( void )
{
	char			*name;
	unsigned char	*buf = NULL;
	int				size = 0;
	unsigned char	md5[16];
	resource_t		custResource;

	name = Cmd_Argv( 1 );
	if ( !name || !name[0] )
	{
		Con_Printf( "Upload without filename\n" );
		return;
	}

	if ( !cl_allow_upload.value )
	{
		Con_Printf( "Ingoring decal upload ( cl_allow_upload is 0 )\n" );
		return;
	}

	if ( Q_strlen( name ) != 36 || Q_strnicmp( name, "!MD5", 4 ) )
	{
		Con_Printf( "Ingoring upload of non-customization\n" );
		return;
	}

	ContinueLoadingProgressBar( "ClientConnect", 10, 0.0 );

	Q_memset( &custResource, 0, sizeof( custResource ) );
	COM_HexConvert( name + 4, 32, md5 );

	if ( HPAK_ResourceForHash( "custom.hpk", md5, &custResource ) )
	{
		if ( Q_memcmp( md5, custResource.rgucMD5_hash, 16 ) )
		{
			Con_DPrintf( "Bogus data retrieved from custom.hpk, attempting to delete entry\n" );
			HPAK_RemoveLump( "custom.hpk", &custResource );
			return;
		}

		if ( HPAK_GetDataPointer( "custom.hpk", &custResource, &buf, &size ) )
		{
			unsigned char	md5[16];
			MD5Context_t	ctx;

			Q_memset( &ctx, 0, sizeof( ctx ) );
			MD5Init( &ctx );
			MD5Update( &ctx, buf, size );
			MD5Final( md5, &ctx );

			if ( Q_memcmp( custResource.rgucMD5_hash, md5, 16 ) )
			{
				Con_Printf( "HPAK_AddLump called with bogus lump, md5 mismatch\n" );
				Con_Printf( "Purported:  %s\n", MD5_Print( custResource.rgucMD5_hash ) );
				Con_Printf( "Actual   :  %s\n", MD5_Print( md5 ) );
				Con_Printf( "Removing conflicting lump\n" );
				HPAK_RemoveLump( "custom.hpk", &custResource );
				return;
			}
		}
	}

	if ( !buf || !size )
	{
		Con_Printf( "Ingoring customization upload, couldn't find decal locally\n" );
		return;
	}

	Netchan_CreateFileFragmentsFromBuffer( false, &cls.netchan, name, buf, size );
	Netchan_FragSend( &cls.netchan );

	Mem_Free( buf );
}

/*
=====================
CL_SendResourceListBlock

=====================
*/
void CL_SendResourceListBlock( void )
{
	int			nStartIndex;
	int			i;
	int			arg;
	sizebuf_t	msg;
	byte		buf[65536];

	ContinueLoadingProgressBar( "ClientConnect", 6, 0.0 );

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Client to Server Resource Block";
	msg.data = buf;
	msg.cursize = 0;
	msg.maxsize = sizeof( buf );

	if ( cls.state == ca_dedicated || cls.state == ca_disconnected )
	{
		Con_Printf( "custom resource list not valid -- not connected\n" );
		return;
	}

	arg = MSG_ReadLong();
	if ( !cls.demoplayback && arg != cl.servercount )
	{
		Con_Printf( "CL_SendResourceListBlock_f from different level\n" );
		return;
	}

	nStartIndex = MSG_ReadLong();
	if ( nStartIndex < 0 || nStartIndex > cl.num_resources )
	{
		Con_Printf( "custom resource list request out of range\n" );
		return;
	}

	MSG_WriteByte( &msg, clc_resourcelist );
	MSG_WriteShort( &msg, cl.num_resources );

	for ( i = nStartIndex; i < cl.num_resources; i++ )
	{
		MSG_WriteString( &msg, cl.resourcelist[i].szFileName );
		MSG_WriteByte( &msg, cl.resourcelist[i].type );
		MSG_WriteShort( &msg, cl.resourcelist[i].nIndex );
		MSG_WriteLong( &msg, cl.resourcelist[i].nDownloadSize );
		MSG_WriteByte( &msg, cl.resourcelist[i].ucFlags );

		if ( cl.resourcelist[i].ucFlags & RES_CUSTOM )
		{
			SZ_Write( &msg, cl.resourcelist[i].rgucMD5_hash, 16 );
		}
	}

	if ( msg.cursize > 0 )
	{
		Netchan_CreateFragments( false, &cls.netchan, &msg );
		Netchan_FragSend( &cls.netchan );
	}
}

/*
=================
CL_HudMessage

=================
*/
void CL_HudMessage( const char *pMessage )
{
	DispatchDirectUserMsg( "HudText", Q_strlen( pMessage ), (void *)pMessage );
}

/*
==================
CL_FullServerinfo_f

Sent by server when serverinfo changes
==================
*/
void CL_FullServerinfo_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "usage: fullserverinfo <complete info string>\n" );
		return;
	}

	Q_strncpy( cl.serverinfo, Cmd_Argv( 1 ), sizeof( cl.serverinfo ) - 1 );
	cl.serverinfo[sizeof( cl.serverinfo ) - 1] = 0;
}

/*
=================
CL_Shutdown

=================
*/
void CL_Shutdown( void )
{
	CL_FlushEventSystem();

	TextMessageShutdown();

	Net_APIShutDown();

	if ( cls.demoheader )
	{
		FS_Close( cls.demoheader );
		cls.demoheader = NULL;
	}

	if ( FS_FileExists( "demoheader.dmf" ) )
	{
		FS_RemoveFile( "demoheader.dmf", NULL );
	}
}

/*
=================
CL_GameDir_f

=================
*/
void CL_GameDir_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "gamedir is %s\n", com_gamedir );
		return;
	}

	CL_Disconnect_f();

	Con_Printf( "Setting gamedir to %s\n", Cmd_Argv( 1 ) );

	CL_CheckGameDirectory( Cmd_Argv( 1 ) );
}

/*
=================
CL_GetPlayerHulls

=================
*/
void CL_GetPlayerHulls( void )
{
	int		i;
	int		iResult;

	for ( i = 0; i < 4; i++ )
	{
		iResult = ClientDLL_GetHullBounds( i, player_mins[i], player_maxs[i] );
		if ( !iResult )
			break;
	}
}

/*
=================
CL_Rate_f

=================
*/
void CL_Rate_f( void )
{
	float	fNewRate;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  cl_rate <num>\nClient to server transmission rate (bytes/sec)\nCurrent:  %.5f\n", cls.netchan.rate );
		return;
	}

	fNewRate = Q_atof( Cmd_Argv( 1 ) );
	if ( fNewRate == 0 )
	{
		fNewRate = DEFAULT_RATE;
	}

	if ( fNewRate < 1000 || fNewRate > 20000 )
	{
		Con_Printf( "cl_rate:  Maximum %f, Minimum %f\n", 20000, 1000 );
		return;
	}

	cls.netchan.rate = fNewRate;
}

/*
=================
GetPos

=================
*/
void GetPos( vec3_t origin, vec3_t angles )
{
	origin[0] = r_refdef.vieworg[0];
	origin[1] = r_refdef.vieworg[1];
	origin[2] = r_refdef.vieworg[2];

	angles[0] = r_refdef.viewangles[0];
	angles[1] = r_refdef.viewangles[1];
	angles[2] = r_refdef.viewangles[2];

	if ( Cmd_Argc() == 2 && Q_atoi( Cmd_Argv( 1 ) ) == 2 && cls.state == ca_active )
	{
		origin[0] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].origin[0];
		origin[1] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].origin[1];
		origin[2] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].origin[2];

		angles[0] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].angles[0];
		angles[1] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].angles[1];
		angles[2] = cl.frames[cl.parsecountmod].playerstate[cl.playernum].angles[2];
	}
}

/*
=================
CL_SpecPos_f

=================
*/
void CL_SpecPos_f( void )
{
	vec3_t	origin;
	vec3_t	angles;

	GetPos( origin, angles );

	Con_Printf( "spec_pos %.1f %.1f %.1f %.1f %.1f\n", origin[0], origin[1], origin[2], angles[0], angles[1] );
}

/*
=================
CL_Init

=================
*/
void CL_Init( void )
{
	const char	*pchSteamCommunityName;

	pchSteamCommunityName = Steam_GetCommunityName();
	if ( !pchSteamCommunityName )
	{
		pchSteamCommunityName = "unknown";
	}

	Info_SetValueForKey( cls.userinfo, "name", pchSteamCommunityName, MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "topcolor", "0", MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "bottomcolor", "0", MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "rate", "2500", MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "cl_updaterate", "20", MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "cl_lw", "1", MAX_INFO_STRING );
	Info_SetValueForKey( cls.userinfo, "cl_lc", "1", MAX_INFO_STRING );

	Net_APIInit();

	CL_InitTEnts();
	CL_InitExtrap();

	TextMessageInit();

	Sequence_Init();

	//
	// register our commands
	//
	Cvar_RegisterVariable( &cl_weaponlistfix );
	Cvar_RegisterVariable( &cl_fixtimerate );
	Cvar_RegisterVariable( &cl_showmessages );
	Cvar_RegisterVariable( &cl_name );
	Cvar_RegisterVariable( &password );
	Cvar_RegisterVariable( &team );
	Cvar_RegisterVariable( &cl_model );
	Cvar_RegisterVariable( &skin );
	Cvar_RegisterVariable( &topcolor );
	Cvar_RegisterVariable( &bottomcolor );
	Cvar_RegisterVariable( &rate );
	Cvar_RegisterVariable( &cl_updaterate );
	Cvar_RegisterVariable( &cl_lw );
	Cvar_RegisterVariable( &cl_lc );
	Cvar_RegisterVariable( &cl_dlmax );
	Cvar_RegisterVariable( &fs_startup_timings );
	Cvar_RegisterVariable( &fs_lazy_precache );
	Cvar_RegisterVariable( &fs_precache_timings );
	Cvar_RegisterVariable( &fs_perf_warnings );
	Cvar_RegisterVariable( &cl_clockreset );
	Cvar_RegisterVariable( &cl_showevents );
	Cvar_RegisterVariable( &cl_himodels );
	Cvar_RegisterVariable( &cl_gaitestimation );
	Cvar_RegisterVariable( &cl_idealpitchscale );
	Cvar_RegisterVariable( &cl_resend );
	Cvar_RegisterVariable( &cl_timeout );
	Cvar_RegisterVariable( &cl_cmdbackup );
	Cvar_RegisterVariable( &cl_shownet );
	Cvar_RegisterVariable( &rcon_address );
	Cvar_RegisterVariable( &rcon_port );
	Cvar_RegisterVariable( &cl_solid_players );
	Cvar_RegisterVariable( &cl_slisttimeout );
	Cvar_RegisterVariable( &cl_download_ingame );
	Cvar_RegisterVariable( &cl_allow_download );
	Cvar_RegisterVariable( &cl_allow_upload );
	Cvar_RegisterVariable( &cl_gamegauge );
	Cvar_RegisterVariable( &cl_cmdrate );
	Cvar_RegisterVariable( &cl_showfps );
	Cvar_RegisterVariable( &cl_needinstanced );
	Cvar_RegisterVariable( &dev_overview );
	Cvar_RegisterVariable( &cl_logofile );
	Cvar_RegisterVariable( &cl_logocolor );

	ClientDLL_HudInit();

	Cmd_AddCommand( "cl_rate", CL_Rate_f );
	Cmd_AddCommand( "gamedir", CL_GameDir_f );
	Cmd_AddCommand( "connect", CL_Connect_f );
	Cmd_AddCommand( "fullserverinfo", CL_FullServerinfo_f );
	Cmd_AddCommand( "retry", CL_Retry_f );
	Cmd_AddCommand( "disconnect", CL_Disconnect_f );
	Cmd_AddCommand( "record", CL_Record_f );
	Cmd_AddCommand( "stop", CL_Stop_f );
	Cmd_AddCommand( "playdemo", CL_PlayDemo_f );
	Cmd_AddCommand( "viewdemo", CL_ViewDemo_f );
	Cmd_AddCommand( "timedemo", CL_TimeDemo_f );
	Cmd_AddCommand( "gg", CL_GameGauge_f );
	Cmd_AddCommand( "listdemo", CL_ListDemo_f );
	Cmd_AddCommand( "appenddemo", CL_AppendDemo_f );
	Cmd_AddCommand( "removedemo", CL_RemoveDemo_f );
	Cmd_AddCommand( "swapdemo", CL_SwapDemo_f );
	Cmd_AddCommand( "setdemoinfo", CL_SetDemoInfo_f );
	Cmd_AddCommand( "listen", CL_Listen_f );
	Cmd_AddCommand( "commentator", CL_Commentator_f );
	Cmd_AddCommand( "waveplaylen", CL_WavePlayLen_f );
	Cmd_AddCommand( "snapshot", CL_TakeSnapshot_f );
	Cmd_AddCommand( "startmovie", CL_StartMovie_f );
	Cmd_AddCommand( "endmovie", CL_EndMovie_f );
	Cmd_AddCommand( "entities", CL_PrintEntities_f );
	Cmd_AddCommand( "rcon", CL_Rcon_f );
	Cmd_AddCommand( "cl_messages", CL_DumpMessageLoad_f );
	Cmd_AddCommand( "pingservers", CL_PingServers_f );
	Cmd_AddCommand( "slist", CL_Slist_f );
	Cmd_AddCommand( "list", CL_ListCachedServers_f );
	Cmd_AddCommand( "upload", CL_BeginUpload_f );

	CL_InitPrediction();

	Cmd_AddCommand( "httpstop", CL_HTTPStop_f );
	Cmd_AddCommand( "spec_pos", CL_SpecPos_f );

	cls.datagram.data = cls.datagram_buf;
	cls.datagram.cursize = 0;
	cls.datagram.maxsize = sizeof( cls.datagram_buf );
	cls.datagram.flags = 0;
	cls.datagram.buffername = "cls.datagram";

	cls.isVAC2Secure = false;
	cls.GameServerSteamID = 0;

	Q_memset( &cl, 0, sizeof( cl ) );

	cl.resourcesneeded.pNext = cl.resourcesneeded.pPrev = &cl.resourcesneeded;
	cl.resourcesonhand.pNext = cl.resourcesonhand.pPrev = &cl.resourcesonhand;

	DELTA_RegisterDescription( "clientdata_t" );
	DELTA_RegisterDescription( "entity_state_t" );
	DELTA_RegisterDescription( "entity_state_player_t" );
	DELTA_RegisterDescription( "custom_entity_state_t" );
	DELTA_RegisterDescription( "usercmd_t" );
	DELTA_RegisterDescription( "weapon_data_t" );
	DELTA_RegisterDescription( "event_t" );
}

/*
=================
CL_SendKeepaliveToServer

=================
*/
void CL_SendKeepaliveToServer( void )
{
	sizebuf_t		buf;
	unsigned char	szData[32];
	int				iCount;

	if ( cls.state == ca_disconnected )
		return;

	buf.buffername = "Client Keepalive";
	buf.maxsize = sizeof( szData );
	buf.cursize = 0;
	buf.flags = 0;
	buf.data = szData;

	for ( iCount = 0; iCount < 32; iCount++ )
	{
		MSG_WriteByte( &buf, clc_nop );
	}

	Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
}

/*
=================
CL_KeepConnectionActive

=================
*/
void CL_KeepConnectionActive( void )
{
	static float	flLastKeepaliveTime;
	float			flCurrentTime;

	flCurrentTime = Sys_FloatTime();
	if ( flCurrentTime < flLastKeepaliveTime )
	{
		flLastKeepaliveTime = flCurrentTime;
	}

	if ( flCurrentTime - flLastKeepaliveTime > 5.0 )
	{
		CL_SendKeepaliveToServer();
		flLastKeepaliveTime = flCurrentTime;
	}
}

/*
=================
CL_GetModelByIndex

=================
*/
model_t *CL_GetModelByIndex( int index )
{
	model_t	*mod;

	mod = cl.model_precache[index];
	if ( !mod )
		return NULL;

	// needload 1: must be loaded, 2: unreferenced
	if ( mod->needload == 1 || mod->needload == 2 )
	{
		if ( fs_precache_timings.value )
		{
			double	end, start;

			start = Sys_FloatTime();
			Mod_LoadModel( mod, false, false );
			end = Sys_FloatTime();
			Con_DPrintf( "fs_precache_timings: loaded model %s in time %.3f sec\n", mod->name, (float)( end - start ) );
		}
		else
		{
			Mod_LoadModel( mod, false, false );
		}
	}

	return cl.model_precache[index];
}

/*
=================
CL_GetWorldModel

=================
*/
model_t *CL_GetWorldModel( void )
{
	return cl.worldmodel;
}

/*
=================
CL_GetServerCount

=================
*/
int CL_GetServerCount( void )
{
	return cl.servercount;
}
