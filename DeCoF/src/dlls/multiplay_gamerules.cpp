/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// teamplay_gamerules.cpp
//
#include <ctype.h>
#include <sys/stat.h>
#include <fstream>

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"

#include	"skill.h"
#include	"game.h"
#include	"items.h"
#include	"voice_gamemgr.h"
#include	"hltv.h"
#include	"shake.h"

extern DLL_GLOBAL CGameRules	*g_pGameRules;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgHealth;
extern int gmsgFaggot;
extern int gmsgDevDonator;
extern int gmsgLobby;
extern int gmsgTimerPlus;
extern int gmsgSimonMode;
extern int gmsgGreyFade;
extern int gmsgChapter;
extern int gmsgMP3;
extern int gmsgCustomSlv;
extern int gmsgCustomFng;
extern int gmsgCustomGlv;
extern int gmsgMPModel;

extern int g_teamplay;

extern bool m_gbIsInCutscene;
extern bool m_bHadSimonMode;
extern int m_iSimonMode;
extern int g_iSurvivalMode;
extern int m_iMusicManagerLoops;
extern bool gNoWeaponDrop;
extern float m_fMusicManagerFadeTime;

extern int CountConnectedPlayers( void );
extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// The declarations below belong in headers that are not part of the tree
// yet: CCoopLevelChangeManager (cof_cooplevelchangemanager.cpp), CEnvModel
// and CCoFSurvivalMode (effects.h in 1.55) and the Steamworks interface
// (steam_api.h). Layouts are the ones of the 1.55 PDB.
class CCoopLevelChangeManager
{
public:
	bool GivePlayerVariables( CBasePlayer *pPlayer );
};

extern CCoopLevelChangeManager *g_pLevelChange;

class CEnvModel : public CBaseAnimating
{
	void Spawn( void );
	void Precache( void );
	void EXPORT Think( void );
	void KeyValue( KeyValueData *pkvd );
	STATE GetState( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	float VecToYaw( Vector vecDir );

	void SetSequence( void );
	bool m_bIssuedNote;
	int m_ChestNumbers;
	int number1;
	int number2;

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;

public:
	void VisibilityCheck( void );
};

class CCoFSurvivalMode : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT SurvivalThink( void );

	bool m_bSurvivalMode;
	bool m_bSurvivalModeRunning;
	int m_sSurvivalTriggerWhenWon;
	int m_sSurvivalTriggerWhenAllDead;
	int m_iSurvivalTimeInSeconds;
	int m_iCurrentSurvivalTime;
	float m_flNextSurvivalCount;
	int m_iTimeSeconds;
	int m_iTimeMinutes;
	int m_iTimeHours;
};

class ISteamUserStats
{
public:
	virtual bool RequestCurrentStats() = 0;
};

extern "C" __declspec( dllimport ) ISteamUserStats *SteamUserStats();

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

float g_flIntermissionStartTime = 0;
float g_flDeleteCFGFiles = 0;
bool g_bHasDeletedCFGFiles = false;
bool m_HasCheckedSimonMode = false;
bool m_bSpawningSimon = false;
float m_flSimonSpawnTime = 0;

CVoiceGameMgr	g_VoiceGameMgr;

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
CountPlayersButNotMyself

Same as CountPlayers, without pPlayer
==============
*/
int CountPlayersButNotMyself( CBaseEntity *pPlayer )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt && pEnt != pPlayer )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
DeleteAllCFGFiles

Removes every saved player .cfg file from txtfiles
==============
*/
void DeleteAllCFGFiles( void )
{
	HANDLE hFind;
	char gamedir[1024];

	GET_GAME_DIR( gamedir );
	strcat( gamedir, "\\txtfiles\\" );

	std::string path = gamedir;
	std::string searchPattern = "*.cfg";
	std::string fullSearchPath = path + searchPattern;

	WIN32_FIND_DATA FindData;
	hFind = FindFirstFile( fullSearchPath.c_str(), &FindData );

	if ( hFind == INVALID_HANDLE_VALUE )
	{
		return;
	}

	struct stat stFileInfo;

	do
	{
		std::string filePath = path + FindData.cFileName;
		int intStat = stat( filePath.c_str(), &stFileInfo );

		if ( intStat == 0 )
			DeleteFile( filePath.c_str() );
	}
	while ( FindNextFile( hFind, &FindData ) > 0 );
}

/*
==============
ApplyCFGSettingToPlayer

Restores the player state saved in txtfiles/<netname>.cfg
==============
*/
void ApplyCFGSettingToPlayer( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	pPlayer->m_flNextTimeTakenCheck = gpGlobals->time;

	HANDLE hFind;
	char gamedir[1024];

	GET_GAME_DIR( gamedir );
	strcat( gamedir, "\\txtfiles\\" );

	std::string path = gamedir;
	std::string searchPattern = "*.cfg";
	std::string fullSearchPath = path + searchPattern;

	WIN32_FIND_DATA FindData;
	hFind = FindFirstFile( fullSearchPath.c_str(), &FindData );

	if ( hFind == INVALID_HANDLE_VALUE )
	{
		return;
	}

	struct stat stFileInfo;

	do
	{
		std::string filePath = path + FindData.cFileName;
		int intStat = stat( filePath.c_str(), &stFileInfo );

		if ( intStat == 0 )
		{
			char szName[256];
			strcpy( szName, STRING( pPlayer->pev->netname ) );
			strcat( szName, ".cfg" );

			if ( !strcmp( szName, FindData.cFileName ) )
			{
				char szFile[1024];
				char buffer[256];
				sprintf( szFile, "%s", FindData.cFileName );

				char filename[1024];
				GET_GAME_DIR( filename );
				strcat( filename, "/txtfiles/" );
				strcat( filename, STRING( pPlayer->pev->netname ) );
				strcat( filename, ".cfg" );

				std::ifstream file( filename );
				std::string line;

				while ( std::getline( file, line ) )
				{
					if ( strstr( line.c_str(), "health " ) )
					{
						line.erase( 0, strlen( "health " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->pev->health = atoi( buffer );
					}

					if ( strstr( line.c_str(), "stamina " ) )
					{
						line.erase( 0, strlen( "stamina " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->m_fStamina = atoi( buffer );
					}

					if ( strstr( line.c_str(), "quicksel1 " ) )
					{
						line.erase( 0, strlen( "quicksel1 " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						strcpy( buffer, pPlayer->m_sQuickSel1 );
					}

					if ( strstr( line.c_str(), "quicksel2 " ) )
					{
						line.erase( 0, strlen( "quicksel2 " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						strcpy( buffer, pPlayer->m_sQuickSel2 );
					}

					if ( strstr( line.c_str(), "quicksel3 " ) )
					{
						line.erase( 0, strlen( "quicksel3 " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						strcpy( buffer, pPlayer->m_sQuickSel3 );
					}

					if ( strstr( line.c_str(), "playerstats_shotsfired " ) )
					{
						line.erase( 0, strlen( "playerstats_shotsfired " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.shots_fired = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_shotshit " ) )
					{
						line.erase( 0, strlen( "playerstats_shotshit " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.shots_hit = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_syringesused " ) )
					{
						line.erase( 0, strlen( "playerstats_syringesused " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.syringes_used = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_monsterskilledmelee " ) )
					{
						line.erase( 0, strlen( "playerstats_monsterskilledmelee " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.monsters_killed_melee = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_monsterskilledshooting " ) )
					{
						line.erase( 0, strlen( "playerstats_monsterskilledshooting " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.monsters_killed_shooting = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_seconds " ) )
					{
						line.erase( 0, strlen( "playerstats_seconds " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.time_seconds = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_minutes " ) )
					{
						line.erase( 0, strlen( "playerstats_minutes " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.time_minutes = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_hours " ) )
					{
						line.erase( 0, strlen( "playerstats_hours " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.time_hours = atoi( buffer );
					}

					if ( strstr( line.c_str(), "playerstats_damagetaken " ) )
					{
						line.erase( 0, strlen( "playerstats_damagetaken " ) );
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;
						pPlayer->playerstats.damagetaken = atoi( buffer );
					}

					if ( strstr( line.c_str(), "weapon_" ) )
					{
						strcpy( buffer, line.c_str() );
						buffer[line.length()] = 0;

						pPlayer->GiveNamedItem( line.c_str(), false, false );

						for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
						{
							if ( pPlayer->m_rgpPlayerItems[i] && !FClassnameIs( pPlayer->m_rgpPlayerItems[i]->pev, "weapon_action" ) )
							{
								CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[i];

								while ( pPlayerItem )
								{
									if ( pPlayerItem->pszAmmo1() && FClassnameIs( pPlayerItem->pev, line.c_str() ) )
									{
										if ( std::getline( file, line ) )
										{
											strcpy( buffer, line.c_str() );
											buffer[line.length()] = 0;
											((CBasePlayerWeapon *)pPlayerItem)->m_iClip = atoi( buffer );
										}

										if ( std::getline( file, line ) )
										{
											strcpy( buffer, line.c_str() );
											buffer[line.length()] = 0;
											pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = atoi( buffer );
										}
									}

									pPlayerItem = pPlayerItem->m_pNext;
								}
							}
						}
					}
				}

				DeleteFile( filePath.c_str() );
			}
		}
	}
	while ( FindNextFile( hFind, &FindData ) > 0 );

	for ( int i = 1; i <= 6; i++ )
		pPlayer->InventoryMessage( i, pPlayer->cofrInventorySlot[i], 0 );
}

/*
==============
ApplyCFGSettingToPlayerLate

Applies maps/<mapname>.cfg to a player who joins after the level started
==============
*/
void ApplyCFGSettingToPlayerLate( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	char buffer[256];
	char filename[1024];

	GET_GAME_DIR( filename );
	strcat( filename, "/maps/" );
	strcat( filename, STRING( gpGlobals->mapname ) );
	strcat( filename, ".cfg" );

	std::ifstream file( filename );
	std::string line;

	while ( std::getline( file, line ) )
	{
		if ( strstr( line.c_str(), "health " ) )
		{
			line.erase( 0, strlen( "health " ) );
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;
			pPlayer->pev->health = atoi( buffer );
		}

		if ( strstr( line.c_str(), "stamina " ) )
		{
			line.erase( 0, strlen( "stamina " ) );
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;
			pPlayer->m_fStamina = atoi( buffer );
		}

		if ( strstr( line.c_str(), "quicksel1 " ) )
		{
			line.erase( 0, strlen( "quicksel1 " ) );
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;
			strcpy( buffer, pPlayer->m_sQuickSel1 );
		}

		if ( strstr( line.c_str(), "quicksel2 " ) )
		{
			line.erase( 0, strlen( "quicksel2 " ) );
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;
			strcpy( buffer, pPlayer->m_sQuickSel2 );
		}

		if ( strstr( line.c_str(), "quicksel3 " ) )
		{
			line.erase( 0, strlen( "quicksel3 " ) );
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;
			strcpy( buffer, pPlayer->m_sQuickSel3 );
		}

		if ( strstr( line.c_str(), "weapon_" ) )
		{
			strcpy( buffer, line.c_str() );
			buffer[line.length()] = 0;

			gNoWeaponDrop = true;
			pPlayer->GiveNamedItem( line.c_str(), false, false );
			gNoWeaponDrop = false;

			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				if ( pPlayer->m_rgpPlayerItems[i] && !FClassnameIs( pPlayer->m_rgpPlayerItems[i]->pev, "weapon_action" ) )
				{
					CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[i];

					while ( pPlayerItem )
					{
						if ( pPlayerItem->pszAmmo1() && FClassnameIs( pPlayerItem->pev, line.c_str() ) )
						{
							if ( std::getline( file, line ) )
							{
								strcpy( buffer, line.c_str() );
								buffer[line.length()] = 0;
								((CBasePlayerWeapon *)pPlayerItem)->m_iClip = atoi( buffer );
							}

							if ( std::getline( file, line ) )
							{
								strcpy( buffer, line.c_str() );
								buffer[line.length()] = 0;
								pPlayer->m_rgAmmo[((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType] = atoi( buffer );
							}
						}

						pPlayerItem = pPlayerItem->m_pNext;
					}
				}
			}
		}
	}

	for ( int i = 1; i <= 6; i++ )
		pPlayer->InventoryMessage( i, pPlayer->cofrInventorySlot[i], 0 );
}

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker)
	{
/*
		if ( g_teamplay )
		{
			if ( g_pGameRules->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}
*/

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	g_VoiceGameMgr.Init(&g_GameMgrHelper, gpGlobals->maxClients);

	m_HasCheckedSimonMode = false;

	m_bSpawningSimon = false;
	m_flSimonSpawnTime = 0;

	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;
	g_flDeleteCFGFiles = gpGlobals->time + 30;
	g_bHasDeletedCFGFiles = false;

	CVAR_SET_FLOAT( "sv_coop", 1 );

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// dedicated server
		char *servercfgfile = (char *)CVAR_GET_STRING( "servercfgfile" );

		if ( servercfgfile && servercfgfile[0] )
		{
			char szCommand[256];

			ALERT( at_console, "Executing dedicated server config file\n" );
			sprintf( szCommand, "exec %s\n", servercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];

			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
}

inline char *GET_INFOBUFFER( edict_t *e )
{
	return (*g_engfuncs.pfnGetInfoKeyBuffer)( e );
}

BOOL CHalfLifeMultiplay::ClientCommand( CBasePlayer *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgr.ClientCommand(pPlayer, pcmd))
		return TRUE;

	if ( FStrEq( pcmd, "nerdrage" ) )
	{
		if ( pPlayer && pPlayer->IsDeveloper() )
		{
			char *player = (char *)CMD_ARGV( 1 );

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && player && plr->pev->netname && !strcmp( STRING( plr->pev->netname ), player ) )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgFaggot, NULL, plr->pev );
						WRITE_BYTE( 1 );
					MESSAGE_END();

					plr->m_iIsAFaggot = 1;
					plr->EnableControl( FALSE );
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "vote" ) )
	{
		int value = atoi( CMD_ARGV( 1 ) );

		bool voted = false;
		int i;

		if ( g_pGameRules->m_bInVoteKick && g_pGameRules->m_sVoteKickVictimID )
		{
			for ( i = 1; i <= 32; i++ )
			{
				if ( pVoted[i] && !strcmp( GETPLAYERAUTHID( pPlayer->edict() ), pVoted[i] ) )
					voted = true;
			}

			if ( !voted )
			{
				for ( i = 1; i <= 32; i++ )
				{
					if ( !g_pGameRules->pVoted[i] )
					{
						g_pGameRules->pVoted[i] = (char *)GETPLAYERAUTHID( pPlayer->edict() );

						if ( value )
						{
							g_pGameRules->m_iVotesForKick++;
						}
						else
							g_pGameRules->m_iVotesAgainstKick++;
					}
				}
			}
		}

		return TRUE;
	}
	else if ( FStrEq( pcmd, "votekick" ) )
	{
		char *player = (char *)CMD_ARGV( 1 );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && player && plr->pev->netname )
			{
				char *name = (char *)STRING( plr->pev->netname );

				if ( name )
				{
					for ( int k = 0; name[k]; k++ )
						name[k] = tolower( name[k] );
				}

				if ( player )
				{
					for ( int k = 0; player[k]; k++ )
						player[k] = tolower( player[k] );
				}

				if ( player && name && !strcmp( name, player ) )
				{
					g_pGameRules->StartVoteKick( pPlayer, plr );
					break;
				}
			}
		}

		return TRUE;
	}

	return CGameRules::ClientCommand(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
// load all default values
	CGameRules::RefreshSkillData();

// override some values for multiplay.

	m_flNextDevDonatorCheckTime = gpGlobals->time;
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft;

extern cvar_t mp_chattime;

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	if ( m_flNextDevDonatorCheckTime < gpGlobals->time )
	{
		m_flNextDevDonatorCheckTime = gpGlobals->time + 2;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				if ( plr->IsDeveloper() )
					plr->pev->vuser1.y = 1;
				else if ( plr->IsDonator() )
					plr->pev->vuser1.y = 2;
				else
					plr->pev->vuser1.y = 0;

				MESSAGE_BEGIN( MSG_ALL, gmsgDevDonator );
					WRITE_BYTE( ENTINDEX( plr->edict() ) );
					WRITE_BYTE( plr->pev->vuser1.y );
				MESSAGE_END();
			}
		}
	}

	if ( m_bInLobby )
	{
		bool all_ready = true;
		bool has_players = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				if ( m_flNextLobbyUpdateTime < gpGlobals->time )
				{
					int voted = 0;

					if ( m_bReady[ENTINDEX( plr->edict() )] )
						voted = 1;

					MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
						WRITE_BYTE( ENTINDEX( plr->edict() ) );
						WRITE_BYTE( voted );
						WRITE_BYTE( 0 );
					MESSAGE_END();

					m_flNextLobbyUpdateTime = gpGlobals->time + 1;
				}
			}

			if ( plr && plr->pev->fuser4 == 0 && i <= 32 )
			{
				has_players = true;

				if ( !(plr->m_iHideHUD & HIDEHUD_HEALTH) )
					plr->m_iHideHUD |= HIDEHUD_HEALTH;

				if ( !(plr->m_iHideHUD & HIDEHUD_WEAPONS) )
					plr->m_iHideHUD |= HIDEHUD_WEAPONS;

				if ( !(plr->pev->flags & FL_NOTARGET) )
					plr->pev->flags |= FL_NOTARGET;

				plr->EnableControl( FALSE );

				UTIL_ScreenFade( plr, Vector( 0, 0, 0 ), 0, 1, 255, FFADE_OUT | FFADE_STAYOUT );

				if ( !m_bReady[i] )
				{
					all_ready = false;
				}
			}
		}

		if ( m_flLobbyStartOverrideTime > 0 && m_flLobbyStartOverrideTime < gpGlobals->time )
		{
			m_bInLobby = false;

			MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->pev->fuser4 == 0 )
				{
					plr->EnableControl( TRUE );
					UTIL_ScreenFade( plr, Vector( 0, 0, 0 ), 0.75, 1, 255, FFADE_IN );

					plr->m_iHideHUD &= ~HIDEHUD_HEALTH;
					plr->m_iHideHUD &= ~HIDEHUD_WEAPONS;
					plr->pev->flags &= ~FL_NOTARGET;
				}
			}

			CBaseEntity *pLobbyEntity = NULL;

			while ( pLobbyEntity = UTIL_FindEntityByClassname( pLobbyEntity, "cof_lobby_start" ) )
				pLobbyEntity->Use( NULL, NULL, USE_TOGGLE, 1 );
		}
		else if ( has_players && all_ready )
		{
			if ( !m_bStartingGameFromLobby )
			{
				m_iLobbyCount = 3;
				m_bStartingGameFromLobby = true;
				m_flNextLobbyCountDownTime = gpGlobals->time;
			}
			else
			{
				if ( m_iLobbyCount >= 0 )
				{
					if ( m_flNextLobbyCountDownTime < gpGlobals->time )
					{
						UTIL_ProMessage( NULL, UTIL_VarArgs( "Starting in %i seconds", m_iLobbyCount ), 5, true );
						m_iLobbyCount--;
						m_flNextLobbyCountDownTime = gpGlobals->time + 1;
						UTIL_EmitAmbientSound( ENT(0), g_vecZero, "common/wpn_hudoff.wav", 1, ATTN_NONE, 0, 100 );
					}
				}
			}

			if ( m_bStartingGameFromLobby && m_iLobbyCount < 0 )
			{
				m_bInLobby = false;

				CBaseEntity *pLobbyEntity = NULL;

				while ( pLobbyEntity = UTIL_FindEntityByClassname( pLobbyEntity, "cof_lobby_start" ) )
					pLobbyEntity->Use( NULL, NULL, USE_TOGGLE, 1 );

				MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 1 );
				MESSAGE_END();

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr && plr->pev->fuser4 == 0 )
					{
						plr->EnableControl( TRUE );
						UTIL_ScreenFade( plr, Vector( 0, 0, 0 ), 0.75, 1, 255, FFADE_IN );

						plr->m_iHideHUD &= ~HIDEHUD_HEALTH;
						plr->m_iHideHUD &= ~HIDEHUD_WEAPONS;
						plr->pev->flags &= ~FL_NOTARGET;
					}
				}
			}
		}
		else if ( m_bStartingGameFromLobby )
		{
			m_bStartingGameFromLobby = false;
			m_iLobbyCount = 3;
			m_flNextLobbyCountDownTime = gpGlobals->time;
		}
	}

	CGameRules::VoteKickThink();

	g_VoiceGameMgr.Update(gpGlobals->frametime);

	///// Check game rules /////
	int frags_remaining = 0;
	int time_remaining = 0;

	if ( m_bSpawningSimon && m_flSimonSpawnTime < gpGlobals->time && !m_gbIsInCutscene )
	{
		m_bSpawningSimon = false;
		m_flSimonSpawnTime = 0;

		if ( !m_iSimonMode )
		{
			bool hassimontriggers = false;

			CBaseEntity *pTrigger = UTIL_FindEntityByClassname( NULL, "trigger_booksimon" );

			if ( pTrigger )
			{
				if ( RANDOM_LONG( 0, 1 ) == 1 )
					hassimontriggers = true;
			}

			if ( !hassimontriggers || g_iSurvivalMode )
			{
				// spawn Simon on one of the spawn points
				edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
				CBaseEntity *pEntity;
				CBaseEntity *pList[32];
				int i;
				int count = 0;

				for ( i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
				{
					if ( pEdict->free )	// Not in use
						continue;

					if ( !pEdict )
						continue;

					pEntity = CBaseEntity::Instance( pEdict );

					if ( pEntity && FClassnameIs( pEntity->pev, "info_simon_spawnpoint" ) )
					{
						count++;

						if ( count <= 30 )
							pList[count] = pEntity;
					}
				}

				if ( count > 0 )
				{
					CBaseEntity *pSpawn = pList[RANDOM_LONG( 1, count )];

					if ( pSpawn )
					{
						if ( g_iSurvivalMode )
						{
							MESSAGE_BEGIN( MSG_ALL, gmsgTimerPlus );
								WRITE_BYTE( 0 );
								WRITE_BYTE( 0 );
								WRITE_BYTE( 1 );
							MESSAGE_END();
						}

						CBaseEntity *pSimon = CBaseEntity::Create( "monster_booksimon", pSpawn->pev->origin, pSpawn->pev->angles, NULL );
					}
				}
			}
			else
			{
				// let one of the trigger_booksimon spawn him
				edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
				CBaseEntity *pEntity;
				CBaseEntity *pList[32];
				int i;
				int count = 0;

				for ( i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
				{
					if ( pEdict->free )	// Not in use
						continue;

					if ( !pEdict )
						continue;

					pEntity = CBaseEntity::Instance( pEdict );

					if ( pEntity && FClassnameIs( pEntity->pev, "trigger_booksimon" ) )
					{
						count++;

						if ( count <= 30 )
							pList[count] = pEntity;
					}
				}

				if ( count > 0 )
				{
					int random = RANDOM_LONG( 1, count );
					pList[random]->pev->iuser1 = 1;
				}
			}
		}
	}

	if ( g_fGameOver )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime.value;

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time) )
				ChangeLevel(); // intermission is over
		}

		return;
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobals->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	if ( pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pCurrentWeapon )
{

	CBasePlayerItem *pCheck;
	CBasePlayerItem *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
	{
		// can't put this gun away right now, so can't switch.
		return FALSE;
	}

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
					{
						return TRUE;
					}
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
	{
		return FALSE;
	}

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	int players = CountConnectedPlayers();

	if ( SteamUserStats() )
		SteamUserStats()->RequestCurrentStats();

	if ( players >= 4 )
	{
		strcpy( szRejectReason, "Server is full (4 players max)" );
		return FALSE;
	}

	if ( pEntity )
		pEntity->v.flags &= ~FL_NOTARGET;

	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayer *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayer *pl )
{
	pl->pev->fuser4 = 0;
	pl->m_bCustomSleeveApplied = false;

	MESSAGE_BEGIN( MSG_ONE, gmsgCustomSlv, NULL, pl->pev );
		WRITE_STRING( NULL );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgCustomFng, NULL, pl->pev );
		WRITE_STRING( NULL );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgCustomGlv, NULL, pl->pev );
		WRITE_STRING( NULL );
	MESSAGE_END();

	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n",
		( pl->pev->netname && STRING(pl->pev->netname)[0] != 0 ) ? STRING(pl->pev->netname) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",
			STRING( pl->pev->netname ),
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",
			STRING( pl->pev->netname ),
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, pl->pev );
				WRITE_BYTE( plr->pev->health );
				WRITE_BYTE( ENTINDEX( plr->edict() ) );
				if ( g_pGameRules->IsMultiplayer() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();

			if ( plr->IsDeveloper() )
				plr->pev->vuser1.y = 1;
			else if ( plr->IsDonator() )
				plr->pev->vuser1.y = 2;
			else
				plr->pev->vuser1.y = 0;

			MESSAGE_BEGIN( MSG_ALL, gmsgDevDonator );
				WRITE_BYTE( ENTINDEX( plr->edict() ) );
				WRITE_BYTE( plr->pev->vuser1.y );
			MESSAGE_END();
		}
	}

	if ( g_fGameOver )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}

	int value = ENTINDEX( pl->edict() );

	if ( m_bInLobby && value > 0 && value <= 32 && gmsgLobby )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
			WRITE_BYTE( value );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		if ( !(pl->m_iHideHUD & HIDEHUD_HEALTH) )
			pl->m_iHideHUD |= HIDEHUD_HEALTH;

		if ( !(pl->m_iHideHUD & HIDEHUD_WEAPONS) )
			pl->m_iHideHUD |= HIDEHUD_WEAPONS;

		if ( !(pl->pev->flags & FL_NOTARGET) )
			pl->pev->flags |= FL_NOTARGET;

		g_pGameRules->m_bReady[value] = false;
	}

	ClientUserInfoChanged( pl, GET_INFOBUFFER( pl->edict() ) );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		edict_t *pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
		CBaseEntity *pEntity;

		for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
		{
			if ( pEdict->free )	// Not in use
				continue;

			pEntity = CBaseEntity::Instance( pEdict );
			if ( !pEntity )
				continue;

			if ( pEntity->pev->iuser1 > 0 && FClassnameIs( pEntity->edict(), "env_model" ) )
			{
				CEnvModel *pModelEnt = GetClassPtr( (CEnvModel *)pEntity->pev );

				if ( pModelEnt )
					pModelEnt->VisibilityCheck();
			}
		}
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t *pClient )
{
	if ( pClient )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		if ( pPlayer )
		{
			// let go of whoever was reviving this player
			if ( pPlayer->pPlayerReviving )
			{
				CBasePlayer *pReviver = GetClassPtr( (CBasePlayer *)pPlayer->pPlayerReviving->pev );

				if ( pReviver )
				{
					pReviver->pPlayerReviving = NULL;
					pReviver->pPlayerToRevive = NULL;
					pReviver->m_flReviveTime = gpGlobals->time;
				}
			}

			bool someonealive = false;
			int i;

			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->pev->fuser4 == 0 && plr->IsAlive() )
					someonealive = true;
			}

			bool survivalmode = false;

			if ( !someonealive )
			{
				CBaseEntity *pSurvivalEntity = NULL;

				while ( pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" ) )
					survivalmode = true;

				for ( i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr )
					{
						if ( survivalmode )
						{
							plr->PlayerStopMP3( 1 );
						}

						if ( survivalmode && pSurvivalEntity && !pSurvivalEntity->pev->iuser3 )
						{
							if ( pSurvivalEntity->pev->target )
							{
								FireTargets( STRING( pSurvivalEntity->pev->target ), pSurvivalEntity, pSurvivalEntity, USE_TOGGLE, 0 );
								pSurvivalEntity->pev->iuser3 = 1;
								GetClassPtr( (CCoFSurvivalMode *)pSurvivalEntity->pev )->m_bSurvivalModeRunning = false;
							}
						}
						else
						{
							plr->m_flDeadTime = gpGlobals->time + 5;
							plr->m_bIssuedGameOver = false;
							plr->m_bCoopGameOver = true;
							plr->m_flNextCountDown = gpGlobals->time;
							plr->m_iGameOverCounter = 5;
							UTIL_ProMessage( plr, "Game over", 4, false );
						}
					}
				}
			}

			pPlayer->pPlayerReviving = NULL;
			pPlayer->pPlayerToRevive = NULL;

			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",
					STRING( pPlayer->pev->netname ),
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",
					STRING( pPlayer->pev->netname ),
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( pAttacker->IsPlayer() && !( pPlayer->pev->flags & FL_FAKECLIENT ) )
		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayer *pPlayer )
{
	if ( g_fGameOver )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayer *pPlayer )
{
	CBaseEntity	*pWeaponEntity = NULL;

	CLIENT_COMMAND( pPlayer->edict(), "donatorcheck\n" );

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);

	bool survivalmode = false;

	CBaseEntity *pSurvivalEntity = NULL;

	while ( pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" ) )
		survivalmode = true;

	if ( !m_HasCheckedSimonMode && !m_bHadSimonMode )
	{
		m_HasCheckedSimonMode = true;

		if ( survivalmode )
		{
			m_bSpawningSimon = true;
			m_flSimonSpawnTime = gpGlobals->time + RANDOM_LONG( 60, 180 );
		}
		else
		{
			int random = RANDOM_LONG( 1, 5 );

			if ( random == 1 )
			{
				m_bSpawningSimon = true;
				m_flSimonSpawnTime = gpGlobals->time + RANDOM_LONG( 20, 240 );
			}
		}
	}

	if ( g_iSurvivalMode && g_pGameRules->IsMultiplayer() )
	{
		if ( m_iSimonMode )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSimonMode, NULL, pPlayer->pev );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSimonMode, NULL, pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}

	if ( m_iSimonMode )
	{
		pPlayer->PlayerSetMP3( MAKE_STRING( "fucked.mp3" ), 1, 1 );

		MESSAGE_BEGIN( MSG_ONE, gmsgGreyFade, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
	else
	{
		char *m_pMusicManager = (char *)CVAR_GET_STRING( "sv_coop_music" );

		if ( m_pMusicManager && strlen( m_pMusicManager ) > 1 )
			pPlayer->PlayerSetMP3( MAKE_STRING( m_pMusicManager ), m_iMusicManagerLoops, m_fMusicManagerFadeTime );
	}

	bool givedefault = true;

	// carry the weapons and stats over from the previous level
	if ( g_pLevelChange )
	{
		if ( g_pLevelChange->GivePlayerVariables( pPlayer ) )
			givedefault = false;
	}

	if ( givedefault )
	{
		while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
			pWeaponEntity->Touch( pPlayer );
	}

	bool ingameover = false;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->m_bCoopGameOver )
		{
			ingameover = true;

			pPlayer->m_flDeadTime = plr->m_flDeadTime;
			pPlayer->m_bIssuedGameOver = plr->m_bIssuedGameOver;
			pPlayer->m_bCoopGameOver = plr->m_bCoopGameOver;
			pPlayer->m_flNextCountDown = plr->m_flNextCountDown;
			pPlayer->m_iGameOverCounter = plr->m_iGameOverCounter;
		}

		if ( plr && plr->pDeathScene && plr->pDeathScene->pev->iuser4 != 1 )
		{
			pPlayer->m_bCoopWaitingForCutsceneToEnd = true;
			pPlayer->m_flNextCoopWaitingForCutsceneFadeTime = gpGlobals->time + 1;
		}
	}

	if ( ingameover )
	{
		if ( !(pPlayer->m_iHideHUD & HIDEHUD_HEALTH) )
			pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;

		if ( !(pPlayer->m_iHideHUD & HIDEHUD_WEAPONS) )
			pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS;

		if ( !(pPlayer->pev->flags & FL_NOTARGET) )
			pPlayer->pev->flags |= FL_NOTARGET;

		pPlayer->EnableControl( FALSE );

		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0, 1, 255, FFADE_OUT | FFADE_STAYOUT );

		MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
			WRITE_STRING( "Game over" );
			WRITE_STRING( NULL );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pPlayer->pev );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 1 );
			WRITE_COORD( 0 );
			WRITE_STRING( "game_over.mp3" );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayer *pPlayer )
{
	return gpGlobals->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair.value != 0 );
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay :: PlayerKilled( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayer *peKiller = NULL;
	CBaseEntity *ktmp = CBaseEntity::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayer*)ktmp;

	if ( pVictim->pev == pKiller )
	{  // killed self
//		pKiller->frags -= 1;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
//		pKiller->frags += IPointsForKill( peKiller, pVictim );

		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntity *Killer = CBaseEntity::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayer *pPlayer = (CBasePlayer*)CBaseEntity::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING( pevInflictor->classname );  // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING( pevInflictor->classname );
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItem *pWeapon )
{
	if ( weaponstay.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobals->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobals->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItem *pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay :: VecWeaponRespawnSpot( CBasePlayerItem *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItem *pWeapon )
{
	return GR_WEAPON_RESPAWN_NO;
/*
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;*/
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pItem )
{
	if ( weaponstay.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRules::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItem *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRules::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItem *pItem )
{
	return GR_ITEM_RESPAWN_NO;
/*
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;*/
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItem *pItem )
{
	return gpGlobals->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayer *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntity *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmo *pAmmo )
{
	return GR_AMMO_RESPAWN_NO;
/*
	if ( pAmmo->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;*/
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmo *pAmmo )
{
	return gpGlobals->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmo *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayer *pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRules::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayer *pl, float fvol )
{
/*
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
*/
	return TRUE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void )
{
	return TRUE;

//	return flashlight.value != 0;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return TRUE;
//	return ( allowmonsters.value != 0 );
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOver )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobals->time + ( (int)mp_chattime.value );
	g_flIntermissionStartTime = gpGlobals->time;

	g_fGameOver = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

char com_token[ 1500 ];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strcpy( szMap, com_token );

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strcpy( szBuffer, com_token );
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = max( item->minplayers, 0 );
						item->minplayers = min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncs.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = max( item->maxplayers, 0 );
						item->maxplayers = min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncs.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}

		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if ( strlen( value ) > 0 )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobals->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobals->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOver = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}

//=========================================================
// ClientUserInfoChanged - every player is forced to the
// model of his slot (police1 to police4, or the ones the
// map sets on worldspawn)
//=========================================================
void CHalfLifeMultiplay::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )
{
	char *mdls = g_engfuncs.pfnInfoKeyValue( infobuffer, "model" );

	int clientIndex = pPlayer->entindex();

	char szPlayer1Model[256];
	char szPlayer2Model[256];
	char szPlayer3Model[256];
	char szPlayer4Model[256];

	strcpy( szPlayer1Model, "police1" );
	strcpy( szPlayer2Model, "police2" );
	strcpy( szPlayer3Model, "police3" );
	strcpy( szPlayer4Model, "police4" );

	if ( g_pWorld )
	{
		if ( g_pWorld->player1model )
			strcpy( szPlayer1Model, STRING( g_pWorld->player1model ) );

		if ( g_pWorld->player2model )
			strcpy( szPlayer2Model, STRING( g_pWorld->player2model ) );

		if ( g_pWorld->player3model )
			strcpy( szPlayer3Model, STRING( g_pWorld->player3model ) );

		if ( g_pWorld->player4model )
			strcpy( szPlayer4Model, STRING( g_pWorld->player4model ) );
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr == pPlayer )
		{
			switch ( i )
			{
			case 1:
				if ( !FStrEq( mdls, szPlayer1Model ) )
				{
					g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", szPlayer1Model );

					MESSAGE_BEGIN( MSG_ONE, gmsgMPModel, NULL, pPlayer->pev );
					MESSAGE_END();
					return;
				}
				break;

			case 2:
				if ( !FStrEq( mdls, szPlayer2Model ) )
				{
					g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", szPlayer2Model );

					MESSAGE_BEGIN( MSG_ONE, gmsgMPModel, NULL, pPlayer->pev );
					MESSAGE_END();
					return;
				}
				break;

			case 3:
				if ( !FStrEq( mdls, szPlayer3Model ) )
				{
					g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", szPlayer3Model );

					MESSAGE_BEGIN( MSG_ONE, gmsgMPModel, NULL, pPlayer->pev );
					MESSAGE_END();
					return;
				}
				break;

			case 4:
				if ( !FStrEq( mdls, szPlayer4Model ) )
				{
					g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", szPlayer4Model );

					MESSAGE_BEGIN( MSG_ONE, gmsgMPModel, NULL, pPlayer->pev );
					MESSAGE_END();
					return;
				}
				break;

			default:
				if ( !FStrEq( mdls, "police1" ) )
				{
					g_engfuncs.pfnSetClientKeyValue( clientIndex, g_engfuncs.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model", "police1" );

					MESSAGE_BEGIN( MSG_ONE, gmsgMPModel, NULL, pPlayer->pev );
					MESSAGE_END();
					return;
				}
				break;
			}
		}
	}
}
