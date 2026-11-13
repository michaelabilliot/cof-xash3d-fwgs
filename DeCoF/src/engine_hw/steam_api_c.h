// steam_api_c.h -- the flat C interface of steam_api_c.dll (Steamworks SDK of
// 2010: SteamGameServer010, SteamMasterServerUpdater001, SteamUser016,
// SteamFriends009) that this Visual C++ 6.0 engine imports instead of the
// C++ interfaces of steam_api.dll; trimmed to what the engine uses

#ifndef STEAM_API_C_H
#define STEAM_API_C_H

#include "steamclientpublic.h"

typedef unsigned char	uint8;
typedef unsigned short	uint16;

// handle to a communication pipe to the Steam client
typedef int HSteamPipe;
// handle to single instance of a steam user
typedef int HSteamUser;
// handle to a single call into the Steam client
typedef int HSteamCall;

// Server modes
enum EServerMode
{
	eServerModeInvalid = 0,					// DO NOT USE
	eServerModeNoAuthentication = 1,		// Don't authenticate user logins and don't list on the server list
	eServerModeAuthentication = 2,			// Authenticate users, list on the server list, don't run VAC on clients that connect
	eServerModeAuthenticationAndSecure = 3,	// Authenticate users, list on the server list and VAC protect clients
};

// use the game socket for the master server queries (SteamGameServer_Init usQueryPort)
#define MASTERSERVERUPDATERPORT_USEGAMESOCKETSHARE	((uint16)-1)

// server flags of the master server updater
const uint32 k_unServerFlagNone			= 0x00;
const uint32 k_unServerFlagActive		= 0x01;		// server has users playing
const uint32 k_unServerFlagSecure		= 0x02;		// server wants to be secure
const uint32 k_unServerFlagDedicated	= 0x04;		// server is dedicated
const uint32 k_unServerFlagLinux		= 0x08;		// linux build
const uint32 k_unServerFlagPassworded	= 0x10;		// password protected
const uint32 k_unServerFlagPrivate		= 0x20;		// server shouldn't list on master server and won't enforce authentication of users that connect to the server

// callback ids of the interfaces
enum { k_iSteamUserCallbacks = 100 };
enum { k_iSteamGameServerCallbacks = 200 };
enum { k_iSteamFriendsCallbacks = 300 };

// one callback posted by the Steam client, read with Steam_BGetCallback
struct CallbackMsg_t
{
	HSteamUser m_hSteamUser;
	int m_iCallback;
	uint8 *m_pubParam;
	int m_cubParam;
};

// called when a connection to the Steam back-end has been established
struct SteamServersConnected_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 1 };
};

// called when a connection attempt has failed
struct SteamServerConnectFailure_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 2 };
	EResult m_eResult;
};

// sent by the Steam server to the client telling it to disconnect from the specified game server
struct ClientGameServerDeny_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 13 };

	uint32 m_uAppID;
	uint32 m_unGameServerIP;
	uint16 m_usGameServerPort;
	uint16 m_bSecure;
	uint32 m_uReason;
};

// received when the game server requests to be displayed as secure (VAC protected)
struct GSPolicyResponse_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 15 };
	uint8 m_bSecure;
};

// client has been approved to connect to this game server
struct GSClientApprove_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 1 };
	CSteamID m_SteamID;
};

// client has been denied to connection to this game server
struct GSClientDeny_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 2 };
	CSteamID m_SteamID;
	EDenyReason m_eDenyReason;
	char m_rgchOptionalText[128];
};

// request the game server should kick the user
struct GSClientKick_t
{
	enum { k_iCallback = k_iSteamGameServerCallbacks + 3 };
	CSteamID m_SteamID;
	EDenyReason m_eDenyReason;
};

// posted when game overlay activates or deactivates
struct GameOverlayActivated_t
{
	enum { k_iCallback = k_iSteamFriendsCallbacks + 31 };
	uint8 m_bActive;	// true if it's just been activated, false otherwise
};

// called when the user tries to join a different game server from their friends list
struct GameServerChangeRequested_t
{
	enum { k_iCallback = k_iSteamFriendsCallbacks + 32 };
	char m_rgchServer[64];		// server address ("127.0.0.1:27015", "tf2.valvesoftware.com")
	char m_rgchPassword[64];	// server password, if any
};

// list of states a friend can be in
enum EPersonaState
{
	k_EPersonaStateOffline = 0,			// friend is not currently logged on
	k_EPersonaStateOnline = 1,			// friend is logged on
	k_EPersonaStateBusy = 2,			// user is on, but busy
	k_EPersonaStateAway = 3,			// auto-away feature
	k_EPersonaStateSnooze = 4,			// auto-away for a long time
	k_EPersonaStateMax,
};

// set of relationships to other users
enum EFriendRelationship
{
	k_EFriendRelationshipNone = 0,
	k_EFriendRelationshipBlocked = 1,
	k_EFriendRelationshipRequestRecipient = 2,
	k_EFriendRelationshipFriend = 3,
	k_EFriendRelationshipRequestInitiator = 4,
	k_EFriendRelationshipIgnored = 5,
	k_EFriendRelationshipIgnoredFriend = 6,
};

struct FriendGameInfo_t;

// interface to accessing information about individual users (SteamFriends009,
// the methods up to the ones the engine calls)
class ISteamFriends
{
public:
	virtual const char *GetPersonaName() = 0;
	virtual void SetPersonaName( const char *pchPersonaName ) = 0;
	virtual EPersonaState GetPersonaState() = 0;
	virtual int GetFriendCount( int iFriendFlags ) = 0;
	virtual CSteamID GetFriendByIndex( int iFriend, int iFriendFlags ) = 0;
	virtual EFriendRelationship GetFriendRelationship( CSteamID steamIDFriend ) = 0;
	virtual EPersonaState GetFriendPersonaState( CSteamID steamIDFriend ) = 0;
	virtual const char *GetFriendPersonaName( CSteamID steamIDFriend ) = 0;
	virtual bool GetFriendGamePlayed( CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo ) = 0;
	virtual const char *GetFriendPersonaNameHistory( CSteamID steamIDFriend, int iPersonaName ) = 0;
	virtual bool HasFriend( CSteamID steamIDFriend, int iFriendFlags ) = 0;
	virtual int GetClanCount() = 0;
	virtual CSteamID GetClanByIndex( int iClan ) = 0;
};

extern "C"
{

// steam_api
__declspec( dllimport ) bool		SteamAPI_Init( void );
__declspec( dllimport ) void		SteamAPI_Shutdown( void );
__declspec( dllimport ) HSteamPipe	SteamAPI_GetHSteamPipe( void );
__declspec( dllimport ) ISteamFriends	*SteamFriends( void );
__declspec( dllimport ) bool		Steam_BGetCallback( HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg, HSteamCall *phSteamCall );
__declspec( dllimport ) void		Steam_FreeLastCallback( HSteamPipe hSteamPipe );

// crash dumps (sys_dll2.cpp)
typedef void ( *PFNPreMinidumpCallback )( void *context );
__declspec( dllimport ) void		SteamAPI_WriteMiniDump( uint32 uStructuredExceptionCode, void *pvExceptionInfo, uint32 uBuildID );
__declspec( dllimport ) void		SteamAPI_SetMiniDumpComment( const char *pchMsg );
__declspec( dllimport ) void		SteamAPI_UseBreakpadCrashHandler( char const *pchVersion, char const *pchDate, char const *pchTime, bool bFullMemoryDumps, void *pvContext, PFNPreMinidumpCallback m_pfnPreMinidumpCallback );

// filesystem.cpp
__declspec( dllimport ) void		SteamAPI_SetBreakpadAppID( uint32 unAppID );
__declspec( dllimport ) bool		ISteamApps_BIsLowViolence( void );
__declspec( dllimport ) const char	*ISteamApps_GetCurrentGameLanguage( void );

// steam_gameserver
__declspec( dllimport ) bool		SteamGameServer_Init( uint32 unIP, uint16 usPort, uint16 usGamePort, uint16 usSpectatorPort, uint16 usQueryPort, EServerMode eServerMode, const char *pchGameDir, const char *pchVersionString );
__declspec( dllimport ) void		SteamGameServer_Shutdown( void );
__declspec( dllimport ) HSteamPipe	SteamGameServer_GetHSteamPipe( void );

// ISteamGameServer
__declspec( dllimport ) bool		ISteamGameServer_SendUserConnectAndAuthenticate( uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, uint64 *pSteamIDUser );
__declspec( dllimport ) void		ISteamGameServer_SendUserDisconnect( uint64 steamIDUser );
__declspec( dllimport ) bool		ISteamGameServer_UpdateStatus( int cPlayers, int cPlayersMax, int cBotPlayers, const char *pchServerName, const char *pSpectatorServerName, const char *pchMapName );
__declspec( dllimport ) uint64		ISteamGameServer_GetSteamID( void );
__declspec( dllimport ) bool		ISteamGameServer_BSecure( void );
__declspec( dllimport ) bool		ISteamGameServer_BLoggedOn( void );

// ISteamMasterServerUpdater
__declspec( dllimport ) void		ISteamMasterServerUpdater_SetActive( bool bActive );
__declspec( dllimport ) void		ISteamMasterServerUpdater_SetHeartbeatInterval( int iHeartbeatInterval );
__declspec( dllimport ) int			ISteamMasterServerUpdater_GetNextOutgoingPacket( void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort );
__declspec( dllimport ) void		ISteamMasterServerUpdater_SetBasicServerData( unsigned short nProtocolVersion, bool bDedicatedServer, const char *pRegionName, const char *pProductName, unsigned short nMaxReportedClients, bool bPasswordProtected, const char *pGameDescription );
__declspec( dllimport ) void		ISteamMasterServerUpdater_ClearAllKeyValues( void );
__declspec( dllimport ) void		ISteamMasterServerUpdater_SetKeyValue( const char *pKey, const char *pValue );
__declspec( dllimport ) void		ISteamMasterServerUpdater_NotifyShutdown( void );
__declspec( dllimport ) bool		ISteamMasterServerUpdater_WasRestartRequested( void );
__declspec( dllimport ) int			ISteamMasterServerUpdater_GetNumMasterServers( void );
__declspec( dllimport ) int			ISteamMasterServerUpdater_GetMasterServerAddress( int iServer, char *pOut, int outBufferSize );
__declspec( dllimport ) bool		ISteamMasterServerUpdater_AddMasterServer( const char *pServerAddress );
__declspec( dllimport ) bool		ISteamMasterServerUpdater_RemoveMasterServer( const char *pServerAddress );

// ISteamUser
__declspec( dllimport ) int			ISteamUser_InitiateGameConnection( void *pAuthBlob, int cbMaxAuthBlob, uint64 steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure );
__declspec( dllimport ) void		ISteamUser_TerminateGameConnection( uint32 unIPServer, uint16 usPortServer );

}

#endif // STEAM_API_C_H
