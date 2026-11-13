// sv_steam3.h -- the Steam game server and client of the engine

#ifndef SV_STEAM3_H
#define SV_STEAM3_H

#include "steam_api_c.h"

class CSteam3
{
public:
	CSteam3()
	{
		m_bLoggedOn = false;
		m_bLogOnResult = false;
		m_hSteamPipe = 0;
	}

	virtual ~CSteam3() {}
	virtual void Shutdown( void ) = 0;

protected:
	bool		m_bLoggedOn;
	bool		m_bLogOnResult;
	HSteamPipe	m_hSteamPipe;
};

class CSteam3Server : public CSteam3
{
public:
	CSteam3Server( void );

	void		Activate( void );
	void		RunFrame( void );
	virtual void	Shutdown( void );

	client_t	*ClientFindFromSteamID( CSteamID &steamIDFind );
	bool		NotifyClientConnect( client_t *client, const void *pvSteam2Key, uint32 ucbSteam2Key );
	void		NotifyClientDisconnect( client_t *cl );
	bool		NotifyBotConnect( client_t *client );
	void		NotifyOfLevelChange( bool bForce );

	uint64		GetSteamID( void );
	bool		BSecure( void ) const		{ return ISteamGameServer_BSecure(); }
	bool		BLanOnly( void ) const		{ return m_bLanOnly; }
	bool		BWantsSecure( void ) const	{ return m_bWantToBeSecure; }
	bool		BLoggedOn( void ) const		{ return m_bLoggedOn && ISteamGameServer_BLoggedOn(); }

	void		OnGSClientApprove( GSClientApprove_t *pGSClientSteam2Accept );
	void		OnGSClientDeny( GSClientDeny_t *pGSClientDeny );
	void		OnGSClientKick( GSClientKick_t *pGSClientKick );
	void		OnGSPolicyResponse( GSPolicyResponse_t *pPolicyResponse );
	void		OnLogonSuccess( SteamServersConnected_t *pLogonSuccess );
	void		OnLogonFailure( SteamServerConnectFailure_t *pLogonFailure );

	void		OnGSClientDenyHelper( client_t *cl, EDenyReason eDenyReason, const char *pchOptionalText );
	void		SendUpdatedServerDetails( void );

protected:
	bool		m_bHasActivePlayers;
	bool		m_bWantToBeSecure;
	bool		m_bLanOnly;
	CSteamID	m_SteamIDGS;
};

class CSteam3Client : public CSteam3
{
public:
	CSteam3Client() {}

	void		InitClient( void );
	void		RunFrame( void );
	virtual void	Shutdown( void );

	int			InitiateGameConnection( void *pData, int cbMaxData, uint64 steamID, uint32 unIPServer, uint16 usPortServer, bool bSecure );
	void		TerminateConnection( uint32 unIPServer, uint16 usPortServer );

	void		OnClientGameServerDeny( ClientGameServerDeny_t *pClientGameServerDeny );
	void		OnGameServerChangeRequested( GameServerChangeRequested_t *pGameServerChangeRequested );
	void		OnGameOverlayActivated( GameOverlayActivated_t *pGameOverlayActivated );
};

CSteam3Server	*Steam3Server( void );
CSteam3Client	&Steam3Client( void );

#endif // SV_STEAM3_H
