// sv_log.c -- server logging: the log file under logs/ and the UDP log
// listeners set with logaddress and logaddress_add

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "info.h"
#include "net.h"
#include "delta.h"
#include "server.h"
#include "protocol.h"
#include "sv_log.h"

#include <time.h>
#include <stdarg.h>

cvar_t	mp_logfile = { "mp_logfile", "1", FCVAR_SERVER };
cvar_t	mp_logecho = { "mp_logecho", "1" };
cvar_t	sv_log_onefile = { "sv_log_onefile", "0" };
cvar_t	sv_log_singleplayer = { "sv_log_singleplayer", "0" };
cvar_t	sv_logsecret = { "sv_logsecret", "0" };

typedef struct loglist
{
	server_log_t	log;
	struct loglist	*next;
} LOGLIST_T;

LOGLIST_T	*firstLog;

/*
==================
Log_Printf

Prints a time stamped line to the log listeners, the console and the log file
==================
*/
void Log_Printf( char *fmt, ... )
{
	va_list		argptr;
	static char	string[1024];
	char		*p;
	time_t		ltime;
	struct tm	*today;

	if ( !svs.log.net_log && !firstLog && !svs.log.active )
		return;

	time( &ltime );
	today = localtime( &ltime );

	_snprintf( string, sizeof( string ), "L %02i/%02i/%04i - %02i:%02i:%02i: ",
		today->tm_mon + 1, today->tm_mday, today->tm_year + 1900, today->tm_hour, today->tm_min, today->tm_sec );

	p = string;
	p += Q_strlen( string );

	va_start( argptr, fmt );
	_vsnprintf( p, sizeof( string ) - Q_strlen( string ), fmt, argptr );
	va_end( argptr );

	if ( svs.log.net_log || firstLog )
	{
		LOGLIST_T	*list;

		list = firstLog;

		if ( svs.log.net_log )
			Netchan_OutOfBandPrint( NS_SERVER, svs.log.net_address, "log %s", string );

		while ( list )
		{
			if ( sv_logsecret.value )
				Netchan_OutOfBandPrint( NS_SERVER, list->log.net_address, "%c%s%s", S2A_LOGKEY, sv_logsecret.string, string );
			else
				Netchan_OutOfBandPrint( NS_SERVER, list->log.net_address, "log %s", string );

			list = list->next;
		}
	}

	if ( !svs.log.active )
		return;

	if ( svs.maxclients <= 1 && sv_log_singleplayer.value == 0 )
		return;

	if ( mp_logecho.value )
		Con_Printf( "%s", string );

	if ( svs.log.file && mp_logfile.value )
		FS_FPrintf( svs.log.file, "%s", string );
}

/*
==================
Log_PrintServerVars

==================
*/
void Log_PrintServerVars( void )
{
	cvar_t	*var;

	if ( !svs.log.active )
		return;

	Log_Printf( "Server cvars start\n" );

	// loop through cvars...
	for ( var = cvar_vars; var; var = var->next )
	{
		if ( !( var->flags & FCVAR_SERVER ) )
			continue;

		Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, var->string );
	}

	Log_Printf( "Server cvars end\n" );
}

/*
==================
Log_Close

==================
*/
void Log_Close( void )
{
	if ( svs.log.file )
	{
		Log_Printf( "Log file closed\n" );
		FS_Close( svs.log.file );
	}

	svs.log.file = NULL;
}

/*
==================
Log_Open

==================
*/
void Log_Open( void )
{
	time_t			ltime;
	struct tm		*today;
	char			szFileBase[MAX_PATH];
	char			szTestFile[MAX_PATH];
	int				i;
	FileHandle_t	fp;
	char			*temp;

	fp = NULL;
	temp = NULL;

	if ( !svs.log.active )
		return;

	if ( sv_log_onefile.value && svs.log.file )
		return;

	if ( !mp_logfile.value )
	{
		Con_Printf( "Server logging data to console.\n" );
		return;
	}

	Log_Close();

	// Find a new log file slot
	time( &ltime );
	today = localtime( &ltime );

	temp = Cvar_VariableString( "logsdir" );

	if ( !temp || Q_strlen( temp ) <= 0 || Q_strstr( temp, ":" ) || Q_strstr( temp, ".." ) )
		_snprintf( szFileBase, sizeof( szFileBase ), "logs/L%02i%02i", today->tm_mon + 1, today->tm_mday );
	else
		_snprintf( szFileBase, sizeof( szFileBase ), "%s/L%02i%02i", temp, today->tm_mon + 1, today->tm_mday );

	for ( i = 0; i < 1000; i++ )
	{
		_snprintf( szTestFile, sizeof( szTestFile ), "%s%03i.log", szFileBase, i );

		COM_FixSlashes( szTestFile );
		COM_CreatePath( szTestFile );

		fp = FS_Open( szTestFile, "r" );
		if ( !fp )
		{
			COM_CreatePath( szTestFile );

			fp = FS_Open( szTestFile, "wt" );
			if ( !fp )
				i = 1000;
			else
				Con_Printf( "Server logging data to file %s\n", szTestFile );
			break;
		}

		FS_Close( fp );
	}

	if ( i == 1000 )
	{
		Con_Printf( "Unable to open logfiles under %s\nLogging disabled\n", szFileBase );
		svs.log.active = false;
		return;
	}

	if ( fp )
		svs.log.file = fp;

	Log_Printf( "Log file started (file \"%s\") (game \"%s\") (version \"%i/%s/%d\")\n",
		szTestFile, Info_ValueForKey( Info_Serverinfo(), "*gamedir" ), PROTOCOL_VERSION, gpszVersionString, build_number() );
}

/*
==================
SV_SetLogAddress_f

==================
*/
void SV_SetLogAddress_f( void )
{
	char		*s;
	int			nPort;
	char		szAdr[MAX_PATH];
	netadr_t	adr;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress:  usage\nlogaddress ip port\n" );
		if ( svs.log.active )
			Con_Printf( "current:  %s\n", NET_AdrToString( svs.log.net_address ) );
		return;
	}

	s = Cmd_Argv( 1 );
	nPort = Q_atoi( Cmd_Argv( 2 ) );
	if ( !nPort )
	{
		Con_Printf( "logaddress:  must specify a valid port\n" );
		return;
	}

	if ( !s || *s == '\0' )
	{
		Con_Printf( "logaddress:  unparseable address\n" );
		return;
	}

	_snprintf( szAdr, sizeof( szAdr ), "%s:%i", s, nPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		svs.log.net_log = true;
		svs.log.net_address = adr;
		Con_Printf( "logaddress:  %s\n", NET_AdrToString( adr ) );
	}
	else
	{
		Con_Printf( "logaddress:  unable to resolve %s\n", szAdr );
	}
}

/*
==================
SV_AddLogAddress_f

==================
*/
void SV_AddLogAddress_f( void )
{
	char		*s;
	int			nPort;
	char		szAdr[MAX_PATH];
	netadr_t	adr;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress_add:  usage\nlogaddress_add ip port\n" );

		if ( firstLog )
		{
			LOGLIST_T	*list;

			list = firstLog;
			while ( list )
			{
				Con_Printf( "current:  %s\n", NET_AdrToString( list->log.net_address ) );
				list = list->next;
			}
		}
		return;
	}

	s = Cmd_Argv( 1 );
	nPort = Q_atoi( Cmd_Argv( 2 ) );
	if ( !nPort )
	{
		Con_Printf( "logaddress_add:  must specify a valid port\n" );
		return;
	}

	if ( !s || *s == '\0' )
	{
		Con_Printf( "logaddress_add:  unparseable address\n" );
		return;
	}

	_snprintf( szAdr, sizeof( szAdr ), "%s:%i", s, nPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( firstLog )
		{
			LOGLIST_T	*list;
			char		found;

			list = firstLog;
			found = false;

			if ( !memcmp( list->log.net_address.ip, adr.ip, 4 ) && list->log.net_address.port == adr.port )
				found = true;

			// find the end of the list, checking the rest of it for the address
			while ( list->next )
			{
				if ( !memcmp( list->log.net_address.ip, adr.ip, 4 ) && list->log.net_address.port == adr.port )
				{
					found = true;
					break;
				}

				list = list->next;
			}

			if ( found == true )
			{
				Con_Printf( "logaddress_add:  address already in list\n" );
				return;
			}

			{
				LOGLIST_T	*tmp;

				tmp = (LOGLIST_T *)malloc( sizeof( LOGLIST_T ) );
				if ( !tmp )
				{
					Con_Printf( "logaddress_add:  error allocating new node\n" );
					return;
				}

				list->next = tmp;
				tmp->log.net_address = adr;
				tmp->next = NULL;

				Con_Printf( "logaddress_add:  %s\n", NET_AdrToString( adr ) );
			}
		}
		else
		{
			firstLog = (LOGLIST_T *)malloc( sizeof( LOGLIST_T ) );
			if ( !firstLog )
			{
				Con_Printf( "logaddress_add:  error allocating new node\n" );
				return;
			}

			firstLog->log.net_address = adr;
			firstLog->next = NULL;

			Con_Printf( "logaddress_add:  %s\n", NET_AdrToString( adr ) );
		}
	}
	else
	{
		Con_Printf( "logaddress_add:  unable to resolve %s\n", szAdr );
	}
}

/*
==================
SV_DelLogAddress_f

==================
*/
void SV_DelLogAddress_f( void )
{
	char		*s;
	int			nPort;
	char		szAdr[MAX_PATH];
	netadr_t	adr;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress_del:  usage\nlogaddress_del ip port\n" );

		if ( firstLog )
		{
			LOGLIST_T	*list;

			list = firstLog;
			while ( list )
			{
				Con_Printf( "current:  %s\n", NET_AdrToString( list->log.net_address ) );
				list = list->next;
			}
		}
		return;
	}

	s = Cmd_Argv( 1 );
	nPort = Q_atoi( Cmd_Argv( 2 ) );
	if ( !nPort )
	{
		Con_Printf( "logaddress_del:  must specify a valid port\n" );
		return;
	}

	if ( !s || *s == '\0' )
	{
		Con_Printf( "logaddress_del:  unparseable address\n" );
		return;
	}

	_snprintf( szAdr, sizeof( szAdr ), "%s:%i", s, nPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( firstLog )
		{
			LOGLIST_T	*list, *prev;
			char		found;

			list = firstLog;
			prev = NULL;
			found = false;

			while ( list )
			{
				if ( !memcmp( list->log.net_address.ip, adr.ip, 4 ) && list->log.net_address.port == adr.port )
				{
					found = true;
					break;
				}

				prev = list;
				list = list->next;
			}

			if ( found == true )
			{
				if ( prev )
					prev->next = list->next;
				else
					firstLog = NULL;

				Con_Printf( "deleting:  %s\n", NET_AdrToString( adr ) );
				free( list );
				return;
			}

			Con_Printf( "logaddress_del:  Couldn't find address in list\n" );
			return;
		}

		Con_Printf( "logaddress_del:  No addresses added yet\n" );
		return;
	}

	Con_Printf( "logaddress_del:  unable to resolve %s\n", szAdr );
}

/*
==================
SV_ServerLog_f

==================
*/
void SV_ServerLog_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "usage:  log < on | off >\n" );

		if ( svs.log.active )
			Con_Printf( "currently logging\n" );
		else
			Con_Printf( "not currently logging\n" );
		return;
	}

	if ( !Q_stricmp( Cmd_Argv( 1 ), "off" ) )
	{
		if ( svs.log.active )
		{
			if ( svs.log.file )
				Log_Close();

			Con_Printf( "Server logging disabled.\n" );
			svs.log.active = false;
		}
	}
	else if ( !Q_stricmp( Cmd_Argv( 1 ), "on" ) )
	{
		svs.log.active = true;
		Log_Open();
	}
	else
	{
		Con_Printf( "log:  unknown parameter %s, 'on' and 'off' are valid\n", Cmd_Argv( 1 ) );
	}
}
