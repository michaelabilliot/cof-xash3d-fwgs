//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Remote administration of the server (the dedicated server UI)
//
// $NoKeywords: $
//=============================================================================

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "host.h"
#include "net.h"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "filter.h"

void GetStatsString( char *buf, int bufSize );
}

#include "IGameServerData.h"
#include "UtlBuffer.h"
#include "UtlLinkedList.h"

//-----------------------------------------------------------------------------
// Purpose: Data accessor for the remote admin tools
//-----------------------------------------------------------------------------
class CServerRemoteAccess : public IGameServerData
{
public:
	CServerRemoteAccess();

	// handles a request
	virtual void WriteDataRequest( const void *buffer, int bufferSize );

	// gets returned data
	virtual int ReadDataResponse( void *data, int len );

	// sends a message to all the watching admin UI's
	void SendMessageToAdminUI( const char *message );

private:
	void RequestValue( int requestID, const char *variable );
	void SetValue( const char *variable, const char *value );
	void ExecCommand( const char *cmdString );
	bool LookupValue( const char *variable, CUtlBuffer &value );
	const char *LookupStringValue( const char *variable );

	// specific value requests
	void GetUserBanList( CUtlBuffer &value );
	void GetPlayerList( CUtlBuffer &value );
	void GetMapList( CUtlBuffer &value );

	struct DataResponse_t
	{
		CUtlBuffer packet;
	};

	// the set of responses
	CUtlLinkedList< DataResponse_t, int > m_ResponsePackets;

	int m_iBytesSent;
	int m_iBytesReceived;
};

CServerRemoteAccess g_ServerRemoteAccess;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerRemoteAccess, IGameServerData, GAMESERVERDATA_INTERFACE_VERSION, g_ServerRemoteAccess );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerRemoteAccess::CServerRemoteAccess()
{
	m_iBytesSent = 0;
	m_iBytesReceived = 0;
}

//-----------------------------------------------------------------------------
// Purpose: handles a request
//-----------------------------------------------------------------------------
void CServerRemoteAccess::WriteDataRequest( const void *buffer, int bufferSize )
{
	m_iBytesReceived += bufferSize;

	// ConMsg( "RemoteAccess: bytes received: %d\n", m_iBytesReceived );

	CUtlBuffer cmd( buffer, bufferSize );

	int requestID = cmd.GetInt();
	int requestType = cmd.GetInt();

	switch ( requestType )
	{
	case SERVERDATA_REQUESTVALUE:
		{
			char variable[256];
			cmd.GetString( variable );
			RequestValue( requestID, variable );
		}
		break;

	case SERVERDATA_SETVALUE:
		{
			char variable[256];
			char value[256];
			cmd.GetString( variable );
			cmd.GetString( value );
			SetValue( variable, value );
		}
		break;

	case SERVERDATA_EXECCOMMAND:
		{
			char command[512];
			cmd.GetString( command );
			ExecCommand( command );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets returned data
//-----------------------------------------------------------------------------
int CServerRemoteAccess::ReadDataResponse( void *data, int len )
{
	int i = m_ResponsePackets.Head();
	if ( m_ResponsePackets.IsValidIndex( i ) )
	{
		CUtlBuffer &response = m_ResponsePackets[i].packet;
		int bytesToCopy = response.TellPut();
		if ( bytesToCopy <= len )
		{
			memcpy( data, response.Base(), bytesToCopy );
		}
		else
		{
			// data too big, just throw away
			bytesToCopy = 0;
		}

		m_iBytesSent += bytesToCopy;

		// ConMsg( "RemoteAccess: bytes sent: %d\n", m_iBytesSent );

		m_ResponsePackets.Remove( i );
		return bytesToCopy;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: looks up a value and sends it back to the admin UI
//-----------------------------------------------------------------------------
void CServerRemoteAccess::RequestValue( int requestID, const char *variable )
{
	CUtlBuffer value( 0, 256, true );
	LookupValue( variable, value );

	// build the response
	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer &response = m_ResponsePackets[i].packet;

	response.PutInt( requestID );
	response.PutInt( SERVERDATA_RESPONSE_VALUE );
	response.PutString( variable );
	response.PutInt( value.TellPut() );
	if ( value.TellPut() )
	{
		response.Put( value.Base(), value.TellPut() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets a value
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SetValue( const char *variable, const char *value )
{
	// check for special types
	if ( !_stricmp( variable, "map" ) )
	{
		// push a map change command
		Cbuf_AddText( "changelevel " );
		Cbuf_AddText( (char *)value );
		Cbuf_AddText( "\n" );
		Cbuf_Execute();
	}
	else if ( !_stricmp( variable, "mapcycle" ) )
	{
		// write out a new mapcycle file
		FileHandle_t f = FS_Open( mapcyclefile.string, "wt" );
		if ( !f )
		{
			Con_Printf( "Couldn't write to read-only file %s, using file _dev_mapcycle.txt instead.\n", mapcyclefile.string );
			Cvar_DirectSet( &mapcyclefile, "_temp_mapcycle.txt" );
			f = FS_Open( mapcyclefile.string, "wt" );
			if ( !f )
				return;
		}

		FS_Write( value, strlen( value ) + 1, 1, f );
		FS_Close( f );
	}
	else
	{
		// Stick the cvar out there
		cvar_t *var = Cvar_FindVar( variable );
		if ( var )
		{
			Cvar_DirectSet( var, (char *)value );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: execs a command
//-----------------------------------------------------------------------------
void CServerRemoteAccess::ExecCommand( const char *cmdString )
{
	Cbuf_AddText( (char *)cmdString );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable
//-----------------------------------------------------------------------------
bool CServerRemoteAccess::LookupValue( const char *variable, CUtlBuffer &value )
{
	const char *strval = LookupStringValue( variable );
	if ( strval )
	{
		value.PutString( strval );
		value.PutChar( 0 );
	}
	else if ( !_stricmp( variable, "stats" ) )
	{
		char stats[512];
		GetStatsString( stats, sizeof( stats ) );
		value.PutString( stats );
		value.PutChar( 0 );
	}
	else if ( !_stricmp( variable, "banlist" ) )
	{
		// returns a list of banned users and ip's
		GetUserBanList( value );
	}
	else if ( !_stricmp( variable, "playerlist" ) )
	{
		GetPlayerList( value );
	}
	else if ( !_stricmp( variable, "maplist" ) )
	{
		GetMapList( value );
	}
	else if ( !_stricmp( variable, "uptime" ) )
	{
		int timeSeconds = (int)( Sys_FloatTime() - Host_GetStartTime() );
		value.PutInt( timeSeconds );
		value.PutChar( 0 );
	}
	else if ( !_stricmp( variable, "ipaddress" ) )
	{
		value.PutString( NET_AdrToString( net_local_adr ) );
		value.PutChar( 0 );
	}
	else if ( !_stricmp( variable, "mapcycle" ) )
	{
		int len = 0;
		void *mapcyclelist = COM_LoadFileForMe( mapcyclefile.string, &len );
		if ( mapcyclelist && len )
		{
			value.PutString( (char *)mapcyclelist );
			value.PutChar( 0 );
			COM_FreeFile( mapcyclelist );
		}
	}
	else
	{
		// value not found, null terminate
		value.PutChar( 0 );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the value of a particular server variable, returning it as a string
//-----------------------------------------------------------------------------
const char *CServerRemoteAccess::LookupStringValue( const char *variable )
{
	static char s_ReturnBuf[32];

	cvar_t *var = Cvar_FindVar( variable );
	if ( var )
	{
		return var->string;
	}

	// special types
	if ( !_stricmp( variable, "map" ) )
	{
		return sv.name;
	}
	else if ( !_stricmp( variable, "playercount" ) )
	{
		// number of players
		int count = 0;
		for ( int i = 0; i < svs.maxclients; i++ )
		{
			if ( svs.clients[i].active || svs.clients[i].spawned || svs.clients[i].connected )
			{
				count++;
			}
		}

		_snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", count );
		return s_ReturnBuf;
	}
	else if ( !_stricmp( variable, "maxplayers" ) )
	{
		_snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", svs.maxclients );
		return s_ReturnBuf;
	}
	else if ( !_stricmp( variable, "gamedescription" ) )
	{
		return gEntityInterface.pfnGetGameDescription();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a list of banned users and ip's
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetUserBanList( CUtlBuffer &value )
{
	int i;

	// add user bans
	for ( i = 0; i < numuserfilters; i++ )
	{
		value.Printf( "%i %s : %.3f min\n", i + 1, SV_GetIDString( &userfilters[i].userid ), userfilters[i].banTime );
	}

	// add ip filters
	for ( i = 0; i < numipfilters; i++ )
	{
		char b[4];
		*(unsigned *)b = ipfilters[i].compare;
		value.Printf( "%i %i.%i.%i.%i : %.3f min\n", i + numuserfilters + 1, b[0], b[1], b[2], b[3], ipfilters[i].banTime );
	}

	value.PutChar( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a list of the players on the server
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetPlayerList( CUtlBuffer &value )
{
	client_t *client = svs.clients;
	for ( int i = 0; i < svs.maxclients; i++, client++ )
	{
		// Don't send it to us if not active
		if ( !client->active )
			continue;

		if ( strlen( client->name ) < 1 )
			continue;

		value.Printf( "\"%s\" %s %s %d %d %d %d\n", client->name, SV_GetIDString( &client->network_userid ),
			NET_AdrToString( client->netchan.remote_address ), (int)( client->latency * 1000 ),
			(int)client->packet_loss, (int)client->edict->v.frags, (int)( realtime - client->netchan.connect_time ) );
	}

	value.PutChar( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a list of the maps on the server
//-----------------------------------------------------------------------------
void CServerRemoteAccess::GetMapList( CUtlBuffer &value )
{
	// search the directory structure.
	char mapwild[64];
	strcpy( mapwild, "maps/*.bsp" );
	const char *findfn = Sys_FindFirst( mapwild, NULL );
	while ( findfn )
	{
		char curDir[MAX_PATH];
		_snprintf( curDir, MAX_PATH, "maps/%s", findfn );
		FS_GetLocalPath( curDir, curDir, MAX_PATH );

		// limit maps displayed to ones for the mod only
		if ( strstr( curDir, com_gamedir ) )
		{
			// clean up the map name
			char mapName[MAX_PATH];
			strcpy( mapName, findfn );
			char *extension = strstr( mapName, ".bsp" );
			if ( extension )
			{
				*extension = 0;
			}

			// write it out
			value.PutString( mapName );
			value.PutString( "\n" );
		}
		findfn = Sys_FindNext( NULL );
	}

	Sys_FindClose();
	value.PutChar( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: sends a message to all the watching admin UI's
//-----------------------------------------------------------------------------
void CServerRemoteAccess::SendMessageToAdminUI( const char *message )
{
	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer &response = m_ResponsePackets[i].packet;

	response.PutInt( 0 );
	response.PutInt( SERVERDATA_UPDATE );
	response.PutString( message );
}

//-----------------------------------------------------------------------------
// Purpose: C function for rest of engine to access CServerRemoteAccess class
//-----------------------------------------------------------------------------
extern "C" void NotifyDedicatedServerUI( const char *message )
{
	g_ServerRemoteAccess.SendMessageToAdminUI( message );
}
