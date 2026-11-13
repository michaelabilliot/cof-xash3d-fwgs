// sv_steam3.cpp -- Steam authentication of the server and the client, master server updates

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
#include "client.h"
#include "sv_log.h"
#include "master.h"

extern cvar_t	password;
extern cvar_t	sv_region;
extern cvar_t	iphostport;
extern cvar_t	hostport;
}

#include "sv_steam3.h"

void CSteam3Server::OnGSPolicyResponse( GSPolicyResponse_t *pPolicyResponse )
{
	if ( BSecure() )
		Con_Printf( "   VAC secure mode is activated.\n" );
	else
		Con_Printf( "   VAC secure mode disabled.\n" );
}

void CSteam3Server::OnLogonSuccess( SteamServersConnected_t *pLogonSuccess )
{
	if ( !m_bLogOnResult )
	{
		m_bLogOnResult = true;
		if ( !m_bLanOnly )
			Con_Printf( "Connection to Steam servers successful.\n" );
	}
	else if ( !m_bLanOnly )
	{
		Con_Printf( "Reconnected to Steam servers.\n" );
	}

	m_SteamIDGS = ISteamGameServer_GetSteamID();
	SendUpdatedServerDetails();
}

uint64 CSteam3Server::GetSteamID( void )
{
	if ( !m_bLanOnly )
		return m_SteamIDGS.ConvertToUint64();

	return CSteamID( 0, 0, k_EUniversePublic, k_EAccountTypeInvalid ).ConvertToUint64();
}

void CSteam3Server::OnLogonFailure( SteamServerConnectFailure_t *pLogonFailure )
{
	if ( !m_bLogOnResult )
	{
		if ( pLogonFailure->m_eResult == k_EResultServiceUnavailable )
		{
			if ( !m_bLanOnly )
			{
				Con_Printf( "Connection to Steam servers successful (SU).\n" );
				if ( m_bWantToBeSecure )
					Con_Printf( "   VAC secure mode not available.\n" );
			}
		}
		else
		{
			if ( !m_bLanOnly )
				Con_Printf( "Could not establish connection to Steam servers.\n" );
		}
	}

	m_bLogOnResult = true;
}

void CSteam3Server::OnGSClientDeny( GSClientDeny_t *pGSClientDeny )
{
	client_t *cl = ClientFindFromSteamID( pGSClientDeny->m_SteamID );
	if ( !cl )
		return;

	OnGSClientDenyHelper( cl, pGSClientDeny->m_eDenyReason, pGSClientDeny->m_rgchOptionalText );
}

void CSteam3Server::OnGSClientDenyHelper( client_t *cl, EDenyReason eDenyReason, const char *pchOptionalText )
{
	switch ( eDenyReason )
	{
	case k_EDenyInvalidVersion:
		SV_DropClient( cl, FALSE, "Client version incompatible with server. \nPlease exit and restart" );
		break;

	case k_EDenyNotLoggedOn:
		if ( !m_bLanOnly )
			SV_DropClient( cl, FALSE, "No Steam logon\n" );
		break;

	case k_EDenyLoggedInElseWhere:
		if ( !m_bLanOnly )
			SV_DropClient( cl, FALSE, "This Steam account is being used in another location\n" );
		break;

	case k_EDenyNoLicense:
		SV_DropClient( cl, FALSE, "This Steam account does not own this game. \nPlease login to the correct Steam account." );
		break;

	case k_EDenyCheater:
		SV_DropClient( cl, FALSE, "VAC banned from secure server\n" );
		break;

	case k_EDenyUnknownText:
		if ( pchOptionalText && *pchOptionalText )
			SV_DropClient( cl, FALSE, (char *)pchOptionalText );
		else
			SV_DropClient( cl, FALSE, "Client dropped by server" );
		break;

	case k_EDenyIncompatibleAnticheat:
		SV_DropClient( cl, FALSE, "You are running an external tool that is incompatible with Secure servers." );
		break;

	case k_EDenyMemoryCorruption:
		SV_DropClient( cl, FALSE, "Memory corruption detected." );
		break;

	case k_EDenyIncompatibleSoftware:
		SV_DropClient( cl, FALSE, "You are running software that is not compatible with Secure servers." );
		break;

	case k_EDenySteamConnectionLost:
		if ( !m_bLanOnly )
			SV_DropClient( cl, FALSE, "Steam connection lost\n" );
		break;

	case k_EDenySteamConnectionError:
		if ( !m_bLanOnly )
			SV_DropClient( cl, FALSE, "Unable to connect to Steam\n" );
		break;

	case k_EDenySteamResponseTimedOut:
		SV_DropClient( cl, FALSE, "Client timed out while answering challenge.\n---> Please make sure that you have opened the appropriate ports on any firewall you are connected behind.\n---> See http://support.steampowered.com for help with firewall configuration." );
		break;

	case k_EDenySteamValidationStalled:
		if ( m_bLanOnly )
		{
			cl->network_userid.m_SteamID = 1;
			break;
		}
		break;

	default:
		SV_DropClient( cl, FALSE, "Client dropped by server" );
		break;
	}
}

void CSteam3Server::OnGSClientKick( GSClientKick_t *pGSClientKick )
{
	client_t *cl = ClientFindFromSteamID( pGSClientKick->m_SteamID );
	if ( cl )
		OnGSClientDenyHelper( cl, pGSClientKick->m_eDenyReason, NULL );
}

void CSteam3Server::OnGSClientApprove( GSClientApprove_t *pGSClientSteam2Accept )
{
	client_t *cl = ClientFindFromSteamID( pGSClientSteam2Accept->m_SteamID );
	if ( !cl )
		return;

	if ( SV_FilterUser( &cl->network_userid ) )
	{
		char msg[256];
		sprintf( msg, "You have been banned from this server\n" );
		SV_RejectConnection( &cl->netchan.remote_address, msg );
		SV_DropClient( cl, FALSE, "STEAM UserID %s is in server ban list\n", SV_GetClientIDString( cl ) );
	}
	else if ( SV_CheckForDuplicateSteamID( cl ) != -1 )
	{
		char msg[256];
		sprintf( msg, "Your UserID is already in use on this server.\n" );
		SV_RejectConnection( &cl->netchan.remote_address, msg );
		SV_DropClient( cl, FALSE, "STEAM UserID %s is already\nin use on this server\n", SV_GetClientIDString( cl ) );
	}
	else
	{
		char msg[512];
		_snprintf( msg, 512, "\"%s<%i><%s><>\" STEAM USERID validated\n", cl->name, cl->userid, SV_GetClientIDString( cl ) );
		Con_DPrintf( "%s", msg );
		Log_Printf( "%s", msg );
	}
}

client_t *CSteam3Server::ClientFindFromSteamID( CSteamID &steamIDFind )
{
	client_t *cl;
	int i;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->connected && !cl->active && !cl->spawned )
			continue;

		if ( cl->network_userid.idtype != AUTH_IDTYPE_STEAM )
			continue;

		if ( cl->network_userid.m_SteamID == steamIDFind.ConvertToUint64() )
			return cl;
	}

	return NULL;
}

CSteam3Server::CSteam3Server( void )
{
	m_bHasActivePlayers = false;
	m_bWantToBeSecure = false;
	m_bLanOnly = false;
	m_SteamIDGS = CSteamID( 1, 0, k_EUniverseInvalid, k_EAccountTypeInvalid );
}

void CSteam3Server::Activate( void )
{
	if ( m_bLoggedOn )
	{
		bool bLanOnly = sv_lan.value != 0;
		if ( m_bLanOnly != bLanOnly )
		{
			m_bLanOnly = bLanOnly;
			m_bWantToBeSecure = !COM_CheckParm( "-insecure" ) && !bLanOnly;
		}
		return;
	}

	m_bLoggedOn = true;

	uint32 unIP = 0;
	uint16 usPort = 26900;

	int iPort = COM_CheckParm( "-sport" );
	if ( iPort > 0 )
		usPort = atoi( com_argv[iPort + 1] );

	if ( net_local_adr.type == NA_IP )
		unIP = ntohl( *(u_long *)&net_local_adr.ip[0] );

	m_bLanOnly = sv_lan.value > 0;
	m_bWantToBeSecure = !COM_CheckParm( "-insecure" ) && !m_bLanOnly;

	uint32 unFlags = k_unServerFlagActive | ( m_bWantToBeSecure ? k_unServerFlagSecure : 0 ) | ( g_bIsDedicatedServer ? k_unServerFlagDedicated : 0 ) | ( Q_strlen( sv_password.string ) > 0 ? k_unServerFlagPassworded : 0 );

	char gamedir[MAX_PATH];
	COM_FileBase( com_gamedir, gamedir );

	EServerMode eSMode = eServerModeAuthenticationAndSecure;
	if ( !m_bWantToBeSecure )
		eSMode = eServerModeAuthentication;
	if ( m_bLanOnly )
		eSMode = eServerModeNoAuthentication;

	int uPort = (int)iphostport.value;
	if ( uPort == 0 )
		uPort = (int)hostport.value;

	int nAppID = GetGameAppID();
	if ( nAppID > 0 )
	{
		FILE *f = fopen( "steam_appid.txt", "w+" );
		if ( f )
		{
			fprintf( f, "%d\n", nAppID );
			fclose( f );
		}
	}

	if ( !SteamGameServer_Init( unIP, usPort, uPort, 0, MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE, eSMode, gamedir, gpszVersionString ) )
	{
		Sys_Error( "Unable to initialize Steam." );
		return;
	}

	m_bLogOnResult = false;
	m_hSteamPipe = SteamGameServer_GetHSteamPipe();

	if ( COM_CheckParm( "-nomaster" ) )
	{
		Con_Printf( "Master server communication disabled.\n" );
		gfNoMasterServer = TRUE;
		return;
	}

	if ( gfNoMasterServer || svs.maxclients <= 1 )
		return;

	ISteamMasterServerUpdater_SetActive( true );

	double fMasterHeartbeatTimeout;
	if ( !strcmp( com_gamedir, "dmc" ) )
		fMasterHeartbeatTimeout = 150.0;
	else if ( !strcmp( com_gamedir, "tfc" ) )
		fMasterHeartbeatTimeout = 400.0;
	else if ( !strcmp( com_gamedir, "cstrike" ) )
		fMasterHeartbeatTimeout = 400.0;
	else
		fMasterHeartbeatTimeout = 200.0;

	ISteamMasterServerUpdater_SetHeartbeatInterval( (int)fMasterHeartbeatTimeout );

	if ( g_bIsDedicatedServer && sv_region.value == -1 )
		Cvar_SetValue( "sv_region", 255 );

	bool iHasPW = ( sv_password.string[0] && Q_stricmp( sv_password.string, "none" ) ) ? true : false;

	NotifyOfLevelChange( true );
}

void CSteam3Server::Shutdown( void )
{
	if ( !m_bLoggedOn )
		return;

	ISteamMasterServerUpdater_NotifyShutdown();
	SteamGameServer_Shutdown();
	m_bLoggedOn = false;
}

void CSteam3Client::Shutdown( void )
{
	if ( !m_bLoggedOn )
		return;

	SteamAPI_Shutdown();
	m_bLoggedOn = false;
}

bool CSteam3Server::NotifyClientConnect( client_t *client, const void *pvSteam2Key, uint32 ucbSteam2Key )
{
	if ( !client || !m_bLoggedOn )
		return false;

	client->network_userid.idtype = AUTH_IDTYPE_STEAM;
	return ISteamGameServer_SendUserConnectAndAuthenticate( htonl( client->network_userid.clientip ), pvSteam2Key, ucbSteam2Key, &client->network_userid.m_SteamID );
}

bool CSteam3Server::NotifyBotConnect( client_t *client )
{
	if ( !client || !m_bLoggedOn )
		return false;

	client->network_userid.idtype = AUTH_IDTYPE_LOCAL;
	client->network_userid.m_SteamID = ISteamGameServer_CreateUnauthenticatedUserConnection();
	return true;
}

void CSteam3Server::NotifyClientDisconnect( client_t *cl )
{
	if ( !cl || !m_bLoggedOn )
		return;

	if ( cl->network_userid.idtype != AUTH_IDTYPE_STEAM && cl->network_userid.idtype != AUTH_IDTYPE_LOCAL )
		return;

	ISteamGameServer_SendUserDisconnect( cl->network_userid.m_SteamID );
}

void CSteam3Server::NotifyOfLevelChange( bool bForce )
{
	SendUpdatedServerDetails();

	int maxplayers = (int)sv_visiblemaxplayers.value;
	if ( maxplayers < 0 )
		maxplayers = svs.maxclients;

	bool iHasPW = ( sv_password.string[0] && Q_stricmp( sv_password.string, "none" ) ) ? true : false;

	ISteamMasterServerUpdater_SetBasicServerData( 48, cls.state == ca_dedicated, sv_region.string, gpszProductString, maxplayers, iHasPW, gEntityInterface.pfnGetGameDescription() );
	ISteamMasterServerUpdater_ClearAllKeyValues();

	for ( cvar_t *var = cvar_vars; var; var = var->next )
	{
		if ( !( var->flags & FCVAR_SERVER ) )
			continue;

		const char *szVal;
		if ( var->flags & FCVAR_PROTECTED )
		{
			if ( Q_strlen( var->string ) > 0 && Q_stricmp( var->string, "none" ) )
				szVal = "1";
			else
				szVal = "0";
		}
		else
		{
			szVal = var->string;
		}

		ISteamMasterServerUpdater_SetKeyValue( var->name, szVal );
	}
}

void CSteam3Server::RunFrame( void )
{
	if ( svs.maxclients <= 1 )
		return;

	double fCurTime = Sys_FloatTime();

	static double s_fLastRunFragsUpdate;
	if ( fCurTime - s_fLastRunFragsUpdate > 1.0 )
	{
		s_fLastRunFragsUpdate = fCurTime;

		bool bHasPlayers = false;
		int i;
		for ( i = 0; i < svs.maxclients; i++ )
		{
			if ( svs.clients[i].active || svs.clients[i].spawned || svs.clients[i].connected )
			{
				bHasPlayers = true;
				break;
			}
		}

		m_bHasActivePlayers = bHasPlayers;
		SendUpdatedServerDetails();

		int maxplayers = (int)sv_visiblemaxplayers.value;
		if ( maxplayers < 0 )
			maxplayers = svs.maxclients;

		bool iHasPW = ( sv_password.string[0] && Q_stricmp( sv_password.string, "none" ) ) ? true : false;

		ISteamMasterServerUpdater_SetBasicServerData( 48, cls.state == ca_dedicated, sv_region.string, gpszProductString, maxplayers, iHasPW, gEntityInterface.pfnGetGameDescription() );

		for ( i = 0; i < svs.maxclients; i++ )
		{
			if ( !svs.clients[i].active )
				continue;

			ISteamGameServer_BUpdateUserData( svs.clients[i].network_userid.m_SteamID, svs.clients[i].name, (int)svs.clients[i].edict->v.frags );
		}

		if ( ISteamMasterServerUpdater_WasRestartRequested() )
		{
			Con_Printf( "%cMasterRequestRestart\n", 3 );
			if ( COM_CheckParm( "-steam" ) )
			{
				Con_Printf( "Your server needs to be restarted in order to receive the latest update.\n" );
				Log_Printf( "Your server needs to be restarted in order to receive the latest update.\n" );
			}
			else
			{
				Con_Printf( "Your server is out of date.  Please update and restart.\n" );
			}
		}
	}

	static double s_fLastRunCallback;
	if ( fCurTime - s_fLastRunCallback > 0.1 )
	{
		s_fLastRunCallback = fCurTime;

		CallbackMsg_t callbackMsg_guess;
		HSteamCall steamCall_guess;
		while ( m_hSteamPipe && Steam_BGetCallback( m_hSteamPipe, &callbackMsg_guess, &steamCall_guess ) )
		{
			switch ( callbackMsg_guess.m_iCallback )
			{
			case SteamServersConnected_t::k_iCallback:
				OnLogonSuccess( (SteamServersConnected_t *)callbackMsg_guess.m_pubParam );
				break;
			case SteamServerConnectFailure_t::k_iCallback:
				OnLogonFailure( (SteamServerConnectFailure_t *)callbackMsg_guess.m_pubParam );
				break;
			case GSClientApprove_t::k_iCallback:
				OnGSClientApprove( (GSClientApprove_t *)callbackMsg_guess.m_pubParam );
				break;
			case GSClientDeny_t::k_iCallback:
				OnGSClientDeny( (GSClientDeny_t *)callbackMsg_guess.m_pubParam );
				break;
			case GSClientKick_t::k_iCallback:
				OnGSClientKick( (GSClientKick_t *)callbackMsg_guess.m_pubParam );
				break;
			case GSPolicyResponse_t::k_iCallback:
				OnGSPolicyResponse( (GSPolicyResponse_t *)callbackMsg_guess.m_pubParam );
				break;
			}

			Steam_FreeLastCallback( m_hSteamPipe );
		}
	}

	static double s_fLastRunSendPackets;
	if ( fCurTime - s_fLastRunSendPackets > 0.01 )
	{
		s_fLastRunSendPackets = fCurTime;

		uint16 port;
		uint32 ip;
		char szOutBuf[4096];
		int iLen = ISteamMasterServerUpdater_GetNextOutgoingPacket( szOutBuf, sizeof( szOutBuf ), &ip, &port );
		while ( iLen > 0 )
		{
			netadr_t netAdr;
			ip = htonl( ip );
			memcpy( netAdr.ip, &ip, sizeof( ip ) );
			netAdr.port = htons( port );
			netAdr.type = NA_IP;

			NET_SendPacket( NS_SERVER, iLen, szOutBuf, netAdr );
			iLen = ISteamMasterServerUpdater_GetNextOutgoingPacket( szOutBuf, sizeof( szOutBuf ), &ip, &port );
		}
	}
}

void CSteam3Server::SendUpdatedServerDetails( void )
{
	int count = 0;
	int bot = 0;
	int i;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		if ( svs.clients[i].active || svs.clients[i].spawned || svs.clients[i].connected )
		{
			if ( svs.clients[i].fakeclient )
				bot++;
			count++;
		}
	}

	int maxplayers = (int)sv_visiblemaxplayers.value;
	if ( maxplayers < 0 )
		maxplayers = svs.maxclients;

	ISteamGameServer_UpdateStatus( count, maxplayers, bot, Cvar_VariableString( "hostname" ), Cvar_VariableString( "hostname" ), sv.name );
}

const char *Steam_GetCommunityName( void )
{
	if ( SteamFriends() )
		return SteamFriends()->GetPersonaName();

	return NULL;
}

// Cry of Fear: 2 when the user is a member of the Steam group 2741962, 1 otherwise (not called)
extern "C" int Steam_GetClanMembership_guess( void )
{
	if ( SteamFriends() )
	{
		int nClans_guess = SteamFriends()->GetClanCount();
		if ( SteamFriends()->GetClanCount() > 0 )
		{
			for ( int i = 0; i < nClans_guess; i++ )
			{
				CSteamID steamIDClan_guess = SteamFriends()->GetClanByIndex( i );
				if ( steamIDClan_guess.BClanAccount() && steamIDClan_guess.GetAccountID() == 2741962 )
					return 2;
			}
		}
	}

	return 1;
}

int CSteam3Client::InitiateGameConnection( void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, bool bSecure )
{
	return ISteamUser_InitiateGameConnection( pData, cbMaxData, steamID, ntohl( unIPServer ), ntohs( usPortServer ), bSecure );
}

void CSteam3Client::TerminateConnection( uint32 unIPServer, uint16 usPortServer )
{
	ISteamUser_TerminateGameConnection( ntohl( unIPServer ), ntohs( usPortServer ) );
}

void CSteam3Client::InitClient( void )
{
	if ( m_bLoggedOn )
		return;

	m_bLoggedOn = true;

	if ( !SteamAPI_Init() )
	{
		Sys_Error( "Failed to initalize authentication interface. Exiting...\n" );
		return;
	}

	m_hSteamPipe = SteamAPI_GetHSteamPipe();
	m_bLogOnResult = false;
}

void CSteam3Client::OnClientGameServerDeny( ClientGameServerDeny_t *pClientGameServerDeny )
{
	COM_ExplainDisconnection( TRUE, "Invalid server version, unable to connect." );
	CL_Disconnect();
}

void CSteam3Client::OnGameServerChangeRequested( GameServerChangeRequested_t *pGameServerChangeRequested )
{
	Cvar_DirectSet( &password, pGameServerChangeRequested->m_rgchPassword );
	Con_Printf( "Connecting to %s\n", pGameServerChangeRequested->m_rgchServer );
	Cbuf_AddText( va( "connect %s\n", pGameServerChangeRequested->m_rgchServer ) );
}

void CSteam3Client::OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated )
{
	if ( Host_IsSinglePlayerGame() )
	{
		if ( pGameOverlayActivated->m_bActive )
			Cbuf_AddText( "setpause" );
		else
			Cbuf_AddText( "unpause" );
	}
}

void CSteam3Client::RunFrame( void )
{
	CallbackMsg_t callbackMsg_guess;
	HSteamCall steamCall_guess;

	while ( m_hSteamPipe && Steam_BGetCallback( m_hSteamPipe, &callbackMsg_guess, &steamCall_guess ) )
	{
		switch ( callbackMsg_guess.m_iCallback )
		{
		case ClientGameServerDeny_t::k_iCallback:
			OnClientGameServerDeny( (ClientGameServerDeny_t *)callbackMsg_guess.m_pubParam );
			break;
		case GameOverlayActivated_t::k_iCallback:
			OnGameOverlayActivated( (GameOverlayActivated_t *)callbackMsg_guess.m_pubParam );
			break;
		case GameServerChangeRequested_t::k_iCallback:
			OnGameServerChangeRequested( (GameServerChangeRequested_t *)callbackMsg_guess.m_pubParam );
			break;
		}

		Steam_FreeLastCallback( m_hSteamPipe );
	}
}

static CSteam3Server *s_Steam3Server;

qboolean Steam_NotifyClientConnect( client_t *cl, const void *pvSteam2Key, unsigned int ucbSteam2Key )
{
	if ( !s_Steam3Server )
		return FALSE;

	return Steam3Server()->NotifyClientConnect( cl, pvSteam2Key, ucbSteam2Key );
}

qboolean Steam_NotifyBotConnect( client_t *cl )
{
	if ( !s_Steam3Server )
		return FALSE;

	return Steam3Server()->NotifyBotConnect( cl );
}

void Steam_NotifyClientDisconnect( client_t *cl )
{
	if ( !s_Steam3Server )
		return;

	Steam3Server()->NotifyClientDisconnect( cl );
}

void Steam_NotifyOfLevelChange( void )
{
	if ( !s_Steam3Server )
		return;

	Steam3Server()->NotifyOfLevelChange( false );
}

void Steam_Shutdown( void )
{
	if ( !s_Steam3Server )
		return;

	Steam3Server()->Shutdown();
	delete s_Steam3Server;
	s_Steam3Server = NULL;
}

void Steam_Activate( void )
{
	if ( !s_Steam3Server )
		s_Steam3Server = new CSteam3Server;

	Steam3Server()->Activate();
}

void Steam_RunFrame( void )
{
	if ( !s_Steam3Server )
		return;

	Steam3Server()->RunFrame();
}

void Steam_SetCVar( const char *pchKey, const char *pchValue )
{
	if ( !s_Steam3Server )
		return;

	ISteamMasterServerUpdater_SetKeyValue( pchKey, pchValue );
}

void Steam_ClientRunFrame( void )
{
	Steam3Client().RunFrame();
}

void Steam_InitClient( void )
{
	Steam3Client().InitClient();
}

int Steam_GSInitiateGameConnection( void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, qboolean bSecure )
{
	return Steam3Client().InitiateGameConnection( pData, cbMaxData, steamID, unIPServer, usPortServer, bSecure != 0 );
}

void Steam_GSTerminateGameConnection( uint32 unIPServer, uint16 usPortServer )
{
	Steam3Client().TerminateConnection( unIPServer, usPortServer );
}

void Steam_ShutdownClient( void )
{
	Steam3Client().Shutdown();
}

uint64 Steam_GSGetSteamID( void )
{
	return Steam3Server()->GetSteamID();
}

qboolean Steam_GSBSecure( void )
{
	return Steam3Server()->BSecure();
}

qboolean Steam_GSBLoggedOn( void )
{
	return Steam3Server()->BLoggedOn();
}

qboolean Steam_GSBSecurePreference( void )
{
	return Steam3Server()->BWantsSecure();
}

TSteamGlobalUserID Steam_Steam3IDtoSteam2( uint64 unSteamID )
{
	CSteamID steamID = unSteamID;
	TSteamGlobalUserID steam2ID;
	steamID.ConvertToSteam2( &steam2ID );
	return steam2ID;
}

uint64 Steam_StringToSteamID( const char *pStr )
{
	CSteamID steamID;
	if ( !s_Steam3Server )
		steamID.SetFromSteam2String( pStr, k_EUniversePublic );
	else
		steamID.SetFromSteam2String( pStr, CSteamID( Steam3Server()->GetSteamID() ).GetEUniverse() );

	return steamID.ConvertToUint64();
}

const char *Steam_GetGSUniverse( void )
{
	CSteamID steamID( Steam3Server()->GetSteamID() );
	switch ( steamID.GetEUniverse() )
	{
	case k_EUniversePublic:
		return "";
	case k_EUniverseBeta:
		return "(beta)";
	case k_EUniverseInternal:
		return "(internal)";
	case k_EUniverseRC:
		return "(rc)";
	default:
		return "(u)";
	}
}

CSteam3Server *Steam3Server( void )
{
	return s_Steam3Server;
}

static CSteam3Client s_Steam3Client;

CSteam3Client &Steam3Client( void )
{
	return s_Steam3Client;
}

void Master_SetMaster_f( void )
{
	int i;
	int ipPort = 0;
	char *pszCmd;
	char *pszIPAddress_guess;
	char szAdr_guess[128];
	netadr_t netadr_guess;

	i = Cmd_Argc();

	if ( !s_Steam3Server )
	{
		Con_Printf( "Usage:\nSetmaster unavailable, start a server first.\n" );
		return;
	}

	if ( i < 2 || i > 5 )
	{
		char szServer_guess[1024];
		int nCount_guess = ISteamMasterServerUpdater_GetNumMasterServers();

		Con_Printf( "Usage:\nSetmaster <add | remove | enable | disable> <IP:port>\n" );
		if ( nCount_guess == 0 )
		{
			Con_Printf( "Current:  None\n" );
		}
		else
		{
			Con_Printf( "Current:\n" );
			for ( int iMaster_guess = 0; iMaster_guess < nCount_guess; iMaster_guess++ )
			{
				ISteamMasterServerUpdater_GetMasterServerAddress( iMaster_guess, szServer_guess, sizeof( szServer_guess ) );
				Con_Printf( "  %i:  %s\n", iMaster_guess, szServer_guess );
			}
		}
		return;
	}

	pszCmd = Cmd_Argv( 1 );
	if ( !pszCmd || !pszCmd[0] )
		return;

	if ( !Q_stricmp( pszCmd, "disable" ) && !gfNoMasterServer )
	{
		gfNoMasterServer = TRUE;
		ISteamMasterServerUpdater_SetActive( gfNoMasterServer != 0 );
		return;
	}

	if ( !Q_stricmp( pszCmd, "enable" ) && gfNoMasterServer )
	{
		gfNoMasterServer = FALSE;
		ISteamMasterServerUpdater_SetActive( gfNoMasterServer != 0 );
		return;
	}

	if ( Q_stricmp( pszCmd, "add" ) && Q_stricmp( pszCmd, "remove" ) )
	{
		Con_Printf( "Setmaster:  Unknown command %s\n", pszCmd );
		return;
	}

	pszIPAddress_guess = Cmd_Argv( 2 );
	if ( i == 5 )
	{
		char *pszPort_guess = Cmd_Argv( 4 );
		if ( pszPort_guess && pszPort_guess[0] )
			ipPort = Q_atoi( pszPort_guess );
	}

	if ( !ipPort )
		ipPort = 27010;

	_snprintf( szAdr_guess, sizeof( szAdr_guess ), "%s:%i", pszIPAddress_guess, ipPort );
	if ( !NET_StringToAdr( szAdr_guess, &netadr_guess ) )
	{
		Con_Printf( " Invalid address \"%s\", setmaster command ignored\n", szAdr_guess );
		return;
	}

	if ( !Q_stricmp( pszCmd, "add" ) )
	{
		if ( !ISteamMasterServerUpdater_AddMasterServer( szAdr_guess ) )
		{
			Con_Printf( " Invalid address \"%s\", setmaster command ignored\n", szAdr_guess );
			return;
		}

		gfNoMasterServer = FALSE;
		Con_Printf( "Adding master at %s\n", szAdr_guess );
	}
	else
	{
		if ( !ISteamMasterServerUpdater_RemoveMasterServer( szAdr_guess ) )
		{
			Con_Printf( " Invalid address \"%s\", setmaster command ignored\n", szAdr_guess );
			return;
		}
	}

	ISteamMasterServerUpdater_SetActive( !(bool)gfNoMasterServer );
}
