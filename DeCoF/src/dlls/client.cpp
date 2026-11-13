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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <ctype.h>

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "cofr_features.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "customentity.h"
#include "weapons.h"
#include "weaponinfo.h"
#include "usercmd.h"
#include "netadr.h"
#include "movewith.h"
#include "shake.h"

#include	"skill.h" // buz

//=========================================================
// Declarations this file takes from headers that are not rebuilt yet:
// cof_importantclassdefs.h (CCoFDocument, CBoat, CWheelchair,
// CCofTelephone, CPuzzleBarButton), effects.h (CItemPadlock,
// CCofComputer, CCofYesNo, CParticle), the coop level change manager
// header (playervariables, CCoopLevelChangeManager) and the Steamworks
// SDK (steam_api.h, isteamuserstats.h). Layouts from the 1.55 PDB,
// sizes checked against the 1.6a allocations.
//=========================================================
class CCoFDocument : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];
	char page1[32];
	char page2[32];
	char page3[32];
	char page4[32];
	char note1[32];
	char note2[32];
	char note3[32];
	char note4[32];
	int turnsound;
	char label[48];
};

class CBoat : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT BoatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float boattime1;
	float boattime2;
	float boattime3;
	float boattime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT BoatThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

class CWheelchair : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT WheelchairUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	bool m_bDead;
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int Classify( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float wheelchairtime1;
	float wheelchairtime2;
	float wheelchairtime3;
	float wheelchairtime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT WheelchairThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

class CCofTelephone : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void TelephoneRight( void );
	void TelephoneWrong( void );
	void RightNumber( void );
	void WrongNumber( void );
	void DialTone( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	EHANDLE m_pPlayer;
};

class CPuzzleBarButton : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual void Precache( void );
	void ToggleBar( int bar, CBasePlayer *pPlayer );
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextUse;
	float m_flNextCheck;
};

class CItemPadlock : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void Rotate( int number );
	void EXPORT Check( void );
	void EXPORT Finished( void );
	virtual STATE GetState( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseAnimating :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetSequenceByName( char *szSequence );
	void Cancel( void );
};

class CCofComputer : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void ComputerCheck( CBaseEntity *pUser, char *username, char *password );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	char m_sUserName[64];
	char m_sPassword[64];
	static const char *pUsernames[];
	static const char *pPasswords[];
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CCofYesNo : public CBaseEntity
{
public:
	virtual void Spawn( void );
	void YesActivate( CBaseEntity *pActivator );
	void NoActivate( CBaseEntity *pActivator );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_sWeapon;
	string_t m_sItem;
	string_t m_sNotHaveWeaponMessage;
	string_t m_sNoTrigger;
	string_t m_sCvar;
	string_t m_sCvarNo;
	int m_cvarvalue;
	int m_cvarvalueno;
};

class CParticle : public CPointEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SendInitMessage( CBasePlayer *player );
	void EXPORT KillParticle( void );
	static CParticle *CreateParticle( int message, bool starton, Vector origin, int weaponattachment, float life, CBaseEntity *attachentity, float velocityx, float velocityy, float velocityz );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

struct playervariables
{
	bool m_bUsed;
	int m_iAllowedToUseTMP;
	char *szPlayerName;
	char *szLastUsedWeapon;
	int m_iLastUsedWeaponID;
	bool m_bIsADonator;
	int m_iAmmoInventory[6];
	int m_iAmmoInventoryAmount[6];
	int m_iPlayerHealth;
	int m_iPlayerStamina;
	char szPlayerQuickSlot1[128];
	char szPlayerQuickSlot2[128];
	char szPlayerQuickSlot3[128];
	bool m_bWasDead;
	int suvivalpoints;
	int shots_fired;
	int shots_hit;
	int syringes_used;
	int shots_missed;
	float gunaccuracy;
	int timessaved;
	int weaponscollected;
	int damagetaken;
	float distancetravelled_metres;
	float distancetravelled_kilometres;
	int time_seconds;
	int time_minutes;
	int time_hours;
	int monsters_killed_melee;
	int monsters_killed_shooting;
	bool started_from_the_beginning;
	int chosen_difficulty;
	bool doctor_mode;
	int m_iQuickSlotSlot[4];
	char *szWeapon1;
	int iClip1;
	int iAmmo1;
	char *szWeapon2;
	int iClip2;
	int iAmmo2;
	char *szWeapon3;
	int iClip3;
	int iAmmo3;
	char *szWeapon4;
	int iClip4;
	int iAmmo4;
	char *szWeapon5;
	int iClip5;
	int iAmmo5;
	char *szWeapon6;
	int iClip6;
	int iAmmo6;
	char szQuickSel1[128];
	char szQuickSel2[128];
	char szQuickSel3[128];
	bool m_bFirstDeploy[33];
};

class CCoopLevelChangeManager
{
public:
	CCoopLevelChangeManager( void );
	void GiveWeaponFromInventory( char *weapon, int slot, int i, CBasePlayer *pPlayer );
	void SetPlayerVariables( CBasePlayer *pPlayer );
	bool GivePlayerVariables( CBasePlayer *pPlayer );
	bool m_bJustHadGameOver;
	playervariables playervariablestruct[33];
	playervariables playervariablestructstats[33];
};

typedef __int32 int32;
typedef unsigned __int32 uint32;

class ISteamUserStats
{
public:
	virtual bool RequestCurrentStats() = 0;
	virtual bool GetStat( const char *pchName, int32 *pData ) = 0;
	virtual bool GetStat( const char *pchName, float *pData ) = 0;
	virtual bool SetStat( const char *pchName, int32 nData ) = 0;
	virtual bool SetStat( const char *pchName, float fData ) = 0;
	virtual bool UpdateAvgRateStat( const char *pchName, float flCountThisSession, double dSessionLength ) = 0;
	virtual bool GetAchievement( const char *pchName, bool *pbAchieved ) = 0;
	virtual bool SetAchievement( const char *pchName ) = 0;
	virtual bool ClearAchievement( const char *pchName ) = 0;
	virtual bool GetAchievementAndUnlockTime( const char *pchName, bool *pbAchieved, uint32 *punUnlockTime ) = 0;
	virtual bool StoreStats() = 0;
};

extern "C" __declspec( dllimport ) ISteamUserStats * __cdecl SteamUserStats();
//=========================================================

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL int		g_iSkillLevel;
extern DLL_GLOBAL ULONG		g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);
extern int gmsgSayText;
extern int gmsgHUDColor;
extern int gmsgScoreInfo;
extern int gmsgWpnInfo;
extern int gmsgCameraZoom;
extern int gmsgSaveLoad;
extern int gmsgAddNote;
extern int gmsgAdLib;
extern int gmsgLobby;
extern int giPrecacheGrunt;

extern int g_teamplay;

extern char *szCustomCampaign;
extern int g_fNightmare;
extern int g_iDoctorMode;
extern int m_iSimonMode;
extern bool m_bSpawningSimon;
extern float m_flSimonSpawnTime;
extern CCoopLevelChangeManager *g_pLevelChange;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );
extern BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted );

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

void LinkUserMessages( void );

static const char *pPoliceAdLibFollow[] =
{
	"player/police/adlib/follow_1.wav",
	"player/police/adlib/follow_2.wav",
	"player/police/adlib/follow_3.wav",
	"player/police/adlib/follow_4.wav",
	"player/police/adlib/follow_5.wav",
	"player/police/adlib/follow_6.wav",
	"player/police/adlib/follow_7.wav",
};

static const char *pPoliceAdLibHelp[] =
{
	"player/police/adlib/help_1.wav",
	"player/police/adlib/help_2.wav",
	"player/police/adlib/help_3.wav",
	"player/police/adlib/help_4.wav",
	"player/police/adlib/help_5.wav",
};

static const char *pPoliceAdLibSomething[] =
{
	"player/police/adlib/something_1.wav",
	"player/police/adlib/something_2.wav",
	"player/police/adlib/something_3.wav",
	"player/police/adlib/something_4.wav",
	"player/police/adlib/something_5.wav",
	"player/police/adlib/something_6.wav",
	"player/police/adlib/something_7.wav",
	"player/police/adlib/something_8.wav",
	"player/police/adlib/something_9.wav",
};

int demolevel = 1;

static const char *pMorphineInjectionSounds[] =
{
	"player/vocal/MorphineInjection/Morphine1.wav",
	"player/vocal/MorphineInjection/Morphine2.wav",
	"player/vocal/MorphineInjection/Morphine3.wav",
	"player/vocal/MorphineInjection/Morphine4.wav",
	"player/vocal/MorphineInjection/Morphine5.wav",
	"player/vocal/MorphineInjection/Morphine6.wav",
	"player/vocal/MorphineInjection/Morphine7.wav",
	"player/vocal/MorphineInjection/Morphine8.wav",
};

static const char *pMorphineInjectionSoundsPolice[] =
{
	"player/police/MorphineInjection/Morphine1.wav",
	"player/police/MorphineInjection/Morphine2.wav",
	"player/police/MorphineInjection/Morphine3.wav",
	"player/police/MorphineInjection/Morphine4.wav",
};

static const char *pLowHealthSounds[] =
{
	"player/vocal/LowHealth/lhealth1.wav",
	"player/vocal/LowHealth/lhealth2.wav",
	"player/vocal/LowHealth/lhealth3.wav",
	"player/vocal/LowHealth/lhealth4.wav",
	"player/vocal/LowHealth/lhealth5.wav",
};

static const char *pLowHealthSoundsDoctor[] =
{
	"player/doctor/LowHealth/lhealth1.wav",
	"player/doctor/LowHealth/lhealth2.wav",
	"player/doctor/LowHealth/lhealth3.wav",
	"player/doctor/LowHealth/lhealth4.wav",
	"player/doctor/LowHealth/lhealth5.wav",
};

static const char *pLowHealthSoundsPolice[] =
{
	"player/police/LowHealth/lhealth1.wav",
	"player/police/LowHealth/lhealth2.wav",
	"player/police/LowHealth/lhealth3.wav",
	"player/police/LowHealth/lhealth4.wav",
};

static const char *pPainSounds[] =
{
	"player/vocal/Pain/Pain1.wav",
	"player/vocal/Pain/Pain2.wav",
	"player/vocal/Pain/Pain3.wav",
	"player/vocal/Pain/Pain4.wav",
	"player/vocal/Pain/Pain5.wav",
	"player/vocal/Pain/Pain6.wav",
	"player/vocal/Pain/Pain7.wav",
	"player/vocal/Pain/Pain8.wav",
	"player/vocal/Pain/Pain9.wav",
	"player/vocal/Pain/Pain10.wav",
	"player/vocal/Pain/Pain11.wav",
	"player/vocal/Pain/Pain12.wav",
	"player/vocal/Pain/Pain13.wav",
	"player/vocal/Pain/Pain14.wav",
	"player/vocal/Pain/Pain15.wav",
	"player/vocal/Pain/Pain16.wav",
};

static const char *pPainSoundsDoctor[] =
{
	"player/doctor/Pain/Pain1.wav",
	"player/doctor/Pain/Pain2.wav",
	"player/doctor/Pain/Pain3.wav",
	"player/doctor/Pain/Pain4.wav",
	"player/doctor/Pain/Pain5.wav",
	"player/doctor/Pain/Pain6.wav",
	"player/doctor/Pain/Pain7.wav",
	"player/doctor/Pain/Pain8.wav",
};

static const char *pPainSoundsPolice[] =
{
	"player/police/Pain/Pain1.wav",
	"player/police/Pain/Pain2.wav",
	"player/police/Pain/Pain3.wav",
	"player/police/Pain/Pain4.wav",
	"player/police/Pain/Pain5.wav",
	"player/police/Pain/Pain6.wav",
	"player/police/Pain/Pain7.wav",
	"player/police/Pain/Pain8.wav",
	"player/police/Pain/Pain9.wav",
	"player/police/Pain/Pain10.wav",
};

static const char *pSwingSounds[] =
{
	"player/vocal/Swing/Swing1.wav",
	"player/vocal/Swing/Swing2.wav",
	"player/vocal/Swing/Swing3.wav",
	"player/vocal/Swing/Swing4.wav",
	"player/vocal/Swing/Swing5.wav",
	"player/vocal/Swing/Swing6.wav",
	"player/vocal/Swing/Swing7.wav",
	"player/vocal/Swing/Swing8.wav",
	"player/vocal/Swing/Swing9.wav",
	"player/vocal/Swing/Swing10.wav",
};

static const char *pSwingSoundsDoctor[] =
{
	"player/doctor/Swing/Swing1.wav",
	"player/doctor/Swing/Swing2.wav",
	"player/doctor/Swing/Swing3.wav",
	"player/doctor/Swing/Swing4.wav",
	"player/doctor/Swing/Swing5.wav",
	"player/doctor/Swing/Swing6.wav",
	"player/doctor/Swing/Swing7.wav",
	"player/doctor/Swing/Swing8.wav",
	"player/doctor/Swing/Swing9.wav",
	"player/doctor/Swing/Swing10.wav",
};

static const char *pSwingSoundsPolice[] =
{
	"player/police/Swing/Swing1.wav",
	"player/police/Swing/Swing2.wav",
	"player/police/Swing/Swing3.wav",
	"player/police/Swing/Swing4.wav",
	"player/police/Swing/Swing5.wav",
	"player/police/Swing/Swing6.wav",
};

static const char *pDeathSounds[] =
{
	"player/vocal/Death/Death1.wav",
	"player/vocal/Death/Death2.wav",
	"player/vocal/Death/Death3.wav",
	"player/vocal/Death/Death4.wav",
	"player/vocal/Death/Death5.wav",
	"player/vocal/Death/Death6.wav",
	"player/vocal/Death/Death7.wav",
};

static const char *pDeathSoundsDoctor[] =
{
	"player/doctor/Death/Death1.wav",
	"player/doctor/Death/Death2.wav",
	"player/doctor/Death/Death3.wav",
	"player/doctor/Death/Death4.wav",
	"player/doctor/Death/Death5.wav",
	"player/doctor/Death/Death6.wav",
	"player/doctor/Death/Death7.wav",
};

static const char *pDeathSoundsPolice[] =
{
	"player/police/Death/Death1.wav",
	"player/police/Death/Death2.wav",
	"player/police/Death/Death3.wav",
	"player/police/Death/Death4.wav",
	"player/police/Death/Death5.wav",
};

static const char *pVomitSounds[] =
{
	"player/vocal/Vomit/vomit1.wav",
};

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_TOSS;
	pev->deadflag	= DEAD_DEAD;
	pev->nextthink	= -1;
}

CBasePlayer *CBasePlayerByIndex( int playerIndex )
{
	CBasePlayer *pPlayer = NULL;
	entvars_t *pev;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->free && ( (pPlayerEdict->v.flags & FL_FAKECLIENT) || (pPlayerEdict->v.flags & FL_CLIENT) ) )
		{
			pev = &pPlayerEdict->v;
			pPlayer = GetClassPtr( (CBasePlayer *)pev );
		}
	}

	return pPlayer;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ]  )
{
	return g_pGameRules->ClientConnected( pEntity, pszName, pszAddress, szRejectReason );

// a client connecting during an intermission can cause problems
//	if (intermission_running)
//		ExitIntermission ();

}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect( edict_t *pEntity )
{
	if (g_fGameOver)
		return;

	CBaseEntity *pQuitter = CBaseEntity::Instance( pEntity );
	int i;

	if ( pQuitter )
	{
		if ( pQuitter->IsPlayer() )
			((CBasePlayer *)pQuitter)->m_hCurrentCamera = NULL;

		for ( i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->pPlayerReviving == pQuitter )
			{
				plr->pPlayerReviving = NULL;
				break;
			}
		}
	}

	if ( g_pLevelChange )
	{
		for ( i = 1; i <= 32; i++ )
		{
			if ( g_pLevelChange->m_bJustHadGameOver && g_pLevelChange->playervariablestruct[i].m_bUsed && g_pLevelChange->playervariablestruct[i].szPlayerName == STRING(pEntity->v.netname) )
			{
				memset( &g_pLevelChange->playervariablestruct[i], 0, sizeof( playervariables ) );
				memset( &g_pLevelChange->playervariablestructstats[i], 0, sizeof( playervariables ) );
			}
		}
	}

	if ( g_pGameRules && g_pGameRules->m_bInLobby )
	{
		int value = ENTINDEX( pEntity );

		if ( value > 0 && value <= 32 )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
				WRITE_BYTE( ENTINDEX( pEntity ) );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			g_pGameRules->m_bReady[value] = false;
		}
	}

	char text[256];
	sprintf( text, "- %s has left the game\n", STRING(pEntity->v.netname) );
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound.
		if ( pSound )
		{
			pSound->Reset();
		}
	}

// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.fuser4 = 1;
	pEntity->v.takedamage = DAMAGE_NO;// don't attract autoaim
	pEntity->v.solid = SOLID_NOT;// nonsolid
	UTIL_SetEdictOrigin ( pEntity, pEntity->v.origin );
	pEntity->v.flags |= FL_NOTARGET;

	g_pGameRules->ClientDisconnected( pEntity );
}


// called by ClientKill and DeadThink
void respawn(entvars_t* pev, BOOL fCopyCorpse)
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		/*if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}*/

		// respawn player
		GetClassPtr( (CBasePlayer *)pev)->Spawn( );
	}
	else
	{       // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	CBasePlayer *pl = (CBasePlayer*) CBasePlayer::Instance( pev );

	if ( pl->m_fNextSuicideTime > gpGlobals->time )
		return;  // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1;  // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed( pev, GIB_NEVER );

//	pev->modelindex = g_ulModelIndexPlayer;
//	pev->frags -= 2;		// extra penalty
//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn() ;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
}

#include "voice_gamemgr.h"
extern CVoiceGameMgr g_VoiceGameMgr;

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[128];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer *)pev);

	//Not yet.
	if ( player->m_flNextChatTime > gpGlobals->time )
		 return;

	if ( player && player->m_iUsingComputerOrPhone == 1 )
		return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
		{
			p = (char *)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
		{
			sprintf( szTemp, "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			sprintf( szTemp, "%s", ( char * )pcmd );
		}
		p = szTemp;
	}

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// make sure the text has content
	char *pc;
	for ( pc = p; pc != NULL && *pc != 0; pc++ )
	{
		if ( isprint( *pc ) && !isspace( *pc ) )
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if ( pc != NULL )
		return;  // no character found, so say nothing

// turn on color set 2  (color on,  no sound)
	if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );


	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ((client = (CBasePlayer*)UTIL_FindEntityByClassname( client, "player" )) != NULL) && (!FNullEnt(client->edict())) )
	{
		if ( !client->pev )
			continue;

		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		// can the receiver hear the sender? or has he muted him?
		if ( g_VoiceGameMgr.PlayerHasBlockedPlayer( client, player ) )
			continue;

		if ( teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );

	char * temp;
	if ( teamonly )
		temp = "say_team";
	else
		temp = "say";

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n",
			STRING( pEntity->v.netname ),
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			temp,
			p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n",
			STRING( pEntity->v.netname ),
			GETPLAYERUSERID( pEntity ),
			GETPLAYERAUTHID( pEntity ),
			GETPLAYERUSERID( pEntity ),
			temp,
			p );
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	if(CoFR_ServerCommand((CBasePlayer*)GET_PRIVATE(pEntity))) return;
	const char *pcmd = CMD_ARGV(0);

	// coop lobby: only the lobby commands go through
	if ( g_pGameRules && g_pGameRules->m_bInLobby )
	{
		if ( !FStrEq( pcmd, "readylobby" ) && !FStrEq( pcmd, "nerdip" ) )
			return;
	}

	// menu maps only take their own commands
	if ( !strcmp( STRING( gpGlobals->mapname ), "c_server_settings" ) && !strstr( pcmd, "closegame" ) && !strstr( pcmd, "buildcoopsettings" ) && !strstr( pcmd, "freeze" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_difficulty_settings" ) && !strstr( pcmd, "closegame" ) && !strstr( pcmd, "campaign" ) && !strstr( pcmd, "skillset" ) && !strstr( pcmd, "freeze" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_video_stress_test" ) && !strstr( pcmd, "stresscontinue" ) && !strstr( pcmd, "yesnoactivate" ) && !strstr( pcmd, "freeze" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_loadgame" ) && !strstr( pcmd, "closegame" ) && !strstr( pcmd, "loadgamemenu" ) && !strstr( pcmd, "cofload" ) && !strstr( pcmd, "freeze" ) && !strstr( pcmd, "closegame" ) )
		return;

	if ( strstr( STRING( gpGlobals->mapname ), "c_game_menu" ) && !strstr( pcmd, "closegame" ) && !strstr( pcmd, "freeze" ) && !strstr( pcmd, "campaign" ) && !strstr( pcmd, "gamemenucmd" ) && !strstr( pcmd, "skillset" ) && !strstr( pcmd, "cofload" ) && !strstr( pcmd, "subtitleset" ) )
		return;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	entvars_t *pev = &pEntity->v;

	if ( GetClassPtr((CBasePlayer *)pev)->m_bInPhoneCall )
		return;

	// computer
	if ( GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone == 1 && !strstr( pcmd, "freeze" ) && !strstr( pcmd, "stopcomputer" ) && !strstr( pcmd, "unfreeze" ) && !strstr( pcmd, "computercheck" ) )
		return;

	// phone
	if ( GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone == 2 && !strstr( pcmd, "freeze" ) && !strstr( pcmd, "unfreeze" ) && !strstr( pcmd, "phonenumber" ) )
		return;

	if ( FStrEq(pcmd, "VModEnable") ) //LRC - shut up about VModEnable...
	{
		return;
	}
	else if ( FStrEq(pcmd, "hud_color") ) //LRC
	{
		if (CMD_ARGC() == 4)
		{
			int col = (atoi(CMD_ARGV(1)) & 255) << 16;
			col += (atoi(CMD_ARGV(2)) & 255) << 8;
			col += (atoi(CMD_ARGV(3)) & 255);
			MESSAGE_BEGIN( MSG_ONE, gmsgHUDColor, NULL, &pEntity->v );
				WRITE_LONG(col);
			MESSAGE_END();
		}
		else
		{
			ALERT(at_console, "Syntax: hud_color RRR GGG BBB\n");
		}
	}
	else if ( FStrEq(pcmd, "fire") ) //LRC - trigger entities manually
	{
/*		if (g_flWeaponCheat)
		{
			CBaseEntity *pPlayer = CBaseEntity::Instance(pEntity);
			if (CMD_ARGC() > 1)
			{
				FireTargets(CMD_ARGV(1), pPlayer, pPlayer, USE_TOGGLE, 0);
			}
			else
			{
				TraceResult tr;
				UTIL_MakeVectors(pev->v_angle);
				UTIL_TraceLine(
					pev->origin + pev->view_ofs,
					pev->origin + pev->view_ofs + gpGlobals->v_forward * 1000,
					dont_ignore_monsters, pEntity, &tr
				);

				if (tr.pHit)
				{
					CBaseEntity *pHitEnt = CBaseEntity::Instance(tr.pHit);
					if (pHitEnt)
					{
						pHitEnt->Use(pPlayer, pPlayer, USE_TOGGLE, 0);
						ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Fired %s \"%s\"\n", STRING(pHitEnt->pev->classname), STRING(pHitEnt->pev->targetname) ) );
					}
				}
			}
		}*/
	}
	else if ( FStrEq( pcmd, "stopcomputer" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			if ( pPlayer->m_iUsingComputerOrPhone == 1 )
				pPlayer->m_iUsingComputerOrPhone = 0;
		}
	}
	else if ( FStrEq( pcmd, "aitest" ) )
	{
		edict_t *pEdict = INDEXENT( 1 );
		CBaseEntity *pEntity;

		if ( !pEdict )
			return;

		for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
		{
			if ( pEdict->free )	// Not in use
				continue;

			if ( pEdict->v.flags & FL_MONSTER )
			{
				pEntity = CBaseEntity::Instance( pEdict );

				if ( pEntity )
				{
					CBaseMonster *pMonster = pEntity->MyMonsterPointer();

					if ( pMonster )
					{
						pMonster->Stop();
						pMonster->RouteClear();
						pMonster->ChangeSchedule( pMonster->GetScheduleOfType( SCHED_ALERT_SMALL_FLINCH ) );
					}
				}
			}
		}
	}
	else if ( FStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq(pcmd, "say_team" ) )
	{
		Host_Say( pEntity, 1 );
	}
	else if ( FStrEq(pcmd, "fullupdate" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->ForceClientDllUpdate();
	}
	else if ( FStrEq( pcmd, "keypadnumber" ) )
	{
		int number = atoi( CMD_ARGV(1) );
		GetClassPtr((CBasePlayer *)pev)->KeypadNumberCheck( number );
	}
	else if ( FStrEq( pcmd, "puzzlebara" ) )
	{
		CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );
		CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

		if ( pBar1 )
			pBar1->Use( NULL, NULL, USE_TOGGLE, 0 );
		if ( pBar2 )
			pBar2->Use( NULL, NULL, USE_TOGGLE, 0 );
	}
	else if ( FStrEq( pcmd, "puzzlebarb" ) )
	{
		CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );
		CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

		if ( pBar2 )
			pBar2->Use( NULL, NULL, USE_TOGGLE, 0 );
		if ( pBar3 )
			pBar3->Use( NULL, NULL, USE_TOGGLE, 0 );
	}
	else if ( FStrEq( pcmd, "puzzlebarc" ) )
	{
		CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );
		CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

		if ( pBar3 )
			pBar3->Use( NULL, NULL, USE_TOGGLE, 0 );
		if ( pBar4 )
			pBar4->Use( NULL, NULL, USE_TOGGLE, 0 );
	}
	else if ( FStrEq( pcmd, "freeze" ) )
	{
		// normal freeze
		if ( atoi( CMD_ARGV(1) ) == 1024 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( FALSE );
		}

		// computer
		if ( atoi( CMD_ARGV(1) ) == 512 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( FALSE );
			GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone = 1;
		}

		// phone
		if ( atoi( CMD_ARGV(1) ) == 768 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( FALSE );
			GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone = 2;
		}
	}
	else if ( FStrEq( pcmd, "unfreeze" ) )
	{
		// normal freeze
		if ( atoi( CMD_ARGV(1) ) == 1024 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( TRUE );
		}

		// computer
		if ( atoi( CMD_ARGV(1) ) == 512 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( TRUE );
			GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone = 0;
		}

		// fade back in
		if ( atoi( CMD_ARGV(1) ) == 666 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( TRUE );
			UTIL_ScreenFade( GetClassPtr((CBasePlayer *)pev), Vector( 0, 0, 0 ), 1, 0, 255, FFADE_IN );
		}

		// phone
		if ( atoi( CMD_ARGV(1) ) == 768 )
		{
			GetClassPtr((CBasePlayer *)pev)->EnableControl( TRUE );
			GetClassPtr((CBasePlayer *)pev)->m_iUsingComputerOrPhone = 0;
		}
	}
	else if ( FStrEq( pcmd, "closegame" ) )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
			(*g_engfuncs.pfnEndSection)( "_oem_end_training" );
	}
	else if ( FStrEq( pcmd, "unfreezesave" ) )
	{
		if ( atoi( CMD_ARGV(1) ) == 1024 )
		{
			int slot = atoi( CMD_ARGV(2) );
			GetClassPtr((CBasePlayer *)pev)->PreSave( slot );
		}
	}
	else if ( FStrEq( pcmd, "phonenumber" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->PhoneNumber( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "dodgeforward" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->Dodge( 1 );
	}
	else if ( FStrEq( pcmd, "dodgebackward" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->Dodge( 2 );
	}
	else if ( FStrEq( pcmd, "dodgeleft" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->Dodge( 3 );
	}
	else if ( FStrEq( pcmd, "dodgeright" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->Dodge( 4 );
	}
	else if ( FStrEq( pcmd, "hinttoggle" ) )
	{
		if ( CVAR_GET_FLOAT( "sv_disablehints" ) == 0 )
			CVAR_SET_FLOAT( "sv_disablehints", 1 );
		else
			CVAR_SET_FLOAT( "sv_disablehints", 0 );
	}
	else if ( FStrEq( pcmd, "quicksel" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_fPhoneCallTime < gpGlobals->time && !pPlayer->IsOnLadder() && !pPlayer->m_bInPhoneCall )
		{
			int number = atoi( CMD_ARGV(1) );

			pPlayer->QuickSelectItem( number );
		}
	}
	else if ( FStrEq( pcmd, "quickselset1" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			int quickslotnumber = atoi( CMD_ARGV(1) );
			pPlayer->m_iQuickSlotSlot[1] = quickslotnumber;

			// only weapons go into the quick slots
			if ( !strncmp( pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber] ) ), "weapon_", 7 ) )
			{
				strcpy( pPlayer->m_sQuickSel1, pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber] ) ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_AUTO, "inventory/inventory_no.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			}
		}
	}
	else if ( FStrEq( pcmd, "quickselset2" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			int quickslotnumber2 = atoi( CMD_ARGV(1) );
			pPlayer->m_iQuickSlotSlot[2] = quickslotnumber2;

			// only weapons go into the quick slots
			if ( !strncmp( pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber2] ) ), "weapon_", 7 ) )
			{
				strcpy( pPlayer->m_sQuickSel2, pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber2] ) ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_AUTO, "inventory/inventory_no.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			}
		}
	}
	else if ( FStrEq( pcmd, "quickselset3" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			int quickslotnumber3 = atoi( CMD_ARGV(1) );
			pPlayer->m_iQuickSlotSlot[3] = quickslotnumber3;

			// only weapons go into the quick slots
			if ( !strncmp( pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber3] ) ), "weapon_", 7 ) )
			{
				strcpy( pPlayer->m_sQuickSel3, pPlayer->GetDescriptionFromItem( "classname", (char *)STRING( pPlayer->cofrInventorySlot[quickslotnumber3] ) ) );
			}
			else
			{
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_AUTO, "inventory/inventory_no.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			}
		}
	}
	else if ( FStrEq( pcmd, "campaign" ) )
	{
		int campaign = ALLOC_STRING( CMD_ARGV(1) );
		szCustomCampaign = (char *)STRING( campaign );
	}
	else if ( FStrEq( pcmd, "laugh" ) )
	{
		int value = atoi( CMD_ARGV(1) );

		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsDeveloper() )
		{
			switch ( value )
			{
			case 1:
				EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "ambience/laugh1.wav", 1, ATTN_NONE );
				break;
			case 2:
				EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "ambience/laugh2.wav", 1, ATTN_NONE );
				break;
			case 3:
				EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "ambience/laugh3.wav", 1, ATTN_NONE );
				break;
			}
		}
	}
	else if ( FStrEq( pcmd, "skillset" ) )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			if ( atoi( CMD_ARGV(1) ) == 4 )
			{
				GetClassPtr((CBasePlayer *)pev)->m_bNightmare = true;
				CVAR_SET_FLOAT( "difficulty", 4 );
				g_fNightmare = 1;
			}
			else
			{
				GetClassPtr((CBasePlayer *)pev)->m_bNightmare = false;
				CVAR_SET_FLOAT( "difficulty", atoi( CMD_ARGV(1) ) );
				g_fNightmare = 0;
			}

			CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
			EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "inventory/game_start.wav", 1, ATTN_NORM );

			UTIL_ScreenFade( pPlayer, Vector( 255, 255, 255 ), 5, 5, 255, FFADE_OUT | FFADE_STAYOUT );

			pPlayer->SetNextThink( 5 );
			pPlayer->SetThink( &CBasePlayer::GameStart );
		}
	}
	else if ( FStrEq( pcmd, "computercheck" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			CBaseEntity *pComputer = NULL;
			pComputer = UTIL_FindEntityByClassname( NULL, "cof_computer" );

			if ( pComputer )
				GetClassPtr((CCofComputer *)pComputer->pev)->ComputerCheck( pPlayer, (char *)CMD_ARGV(1), (char *)CMD_ARGV(2) );
		}
	}
	else if ( FStrEq( pcmd, "dodgetoggle" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->m_bDodgeDisabled = !pPlayer->m_bDodgeDisabled;

		if ( !pPlayer->m_bDodgeDisabled )
			UTIL_ProMessage( pPlayer, "Dodge disabled", 1, false );
		else
			UTIL_ProMessage( pPlayer, "Dodge enabled", 1, false );
	}
	else if ( FStrEq( pcmd, "inventoryequip" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->InventoryEquip( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "recalldocument" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->RecallDocument( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "closedocument" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pCurrentDocument )
		{
			pPlayer->AddDocument( GetClassPtr((CCoFDocument *)pPlayer->m_pCurrentDocument->pev) );
			pPlayer->m_pCurrentDocument = NULL;
		}
	}
	else if ( FStrEq( pcmd, "attack1pressed" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button |= IN_ATTACK;
	}
	else if ( FStrEq( pcmd, "attack1released" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button &= ~IN_ATTACK;
	}
	else if ( FStrEq( pcmd, "attack2pressed" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button |= IN_ATTACK2;
	}
	else if ( FStrEq( pcmd, "attack2released" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button &= ~IN_ATTACK2;
	}
	else if ( FStrEq( pcmd, "forwardpressed" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button |= IN_FORWARD;
	}
	else if ( FStrEq( pcmd, "forwardreleased" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button &= ~IN_FORWARD;
	}
	else if ( FStrEq( pcmd, "backpressed" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button |= IN_BACK;
	}
	else if ( FStrEq( pcmd, "backreleased" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pDeathScene )
			pPlayer->pDeathScene->pev->button &= ~IN_BACK;
	}
	else if ( FStrEq( pcmd, "tapgetup" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pev->fuser3 != 0 )
		{
			pPlayer->m_fFallingTime = gpGlobals->time;
		}
	}
	else if ( FStrEq( pcmd, "muschi" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsDeveloper() )
		{
			if ( pPlayer->m_flSequenceOverrideTime < gpGlobals->time )
			{
				pPlayer->PlayerAnimation( "pro", false );
				EMIT_SOUND( pPlayer->edict(), CHAN_AUTO, "ambience/really_muschi.wav", 1, ATTN_NONE );

				if ( SteamUserStats() )
				{
					int32 muschi = 0;
					SteamUserStats()->GetStat( "stat_muschi", &muschi );
					muschi++;
					SteamUserStats()->SetStat( "stat_muschi", muschi );
					SteamUserStats()->StoreStats();
					ALERT( at_console, "You have muschied %i times\n", muschi );
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "muschi2" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsDeveloper() )
			pPlayer->m_bMuschi2_guess = !pPlayer->m_bMuschi2_guess;
	}
	else if ( FStrEq( pcmd, "knockout" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsDeveloper() )
		{
			if ( pPlayer->m_flSequenceOverrideTime < gpGlobals->time )
			{
				TraceResult tr;

				UTIL_MakeVectors( pPlayer->pev->v_angle );
				Vector vecSrc = pPlayer->GetGunPosition( );
				Vector vecEnd = vecSrc + gpGlobals->v_forward * 72;

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pPlayer->pev ), &tr );

				if ( tr.pHit )
				{
					CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

					if ( pHit && pHit->IsPlayer() )
					{
						CBasePlayer *pPunched = GetClassPtr((CBasePlayer *)pHit->pev);

						if ( pPunched )
						{
							pPlayer->PlayerAnimation( "knockout", false );

							EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_AUTO, "weapons/mobile/punch_smack.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );

							UTIL_MakeVectors( pev->angles );
							pPunched->pev->punchangle.x = 5;
							pPunched->pev->origin.z += 2;
							pPunched->pev->velocity = pPunched->pev->velocity + gpGlobals->v_forward * 250 + gpGlobals->v_up * 200;
							pPunched->m_bWaitingToFall = true;
							pPunched->ViewPunch( -10, 20, 20 );
							pPunched->m_flSequenceOverrideTime = gpGlobals->time;
							pPunched->m_bHasGotUp = false;
						}
					}
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "closeclothes" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);
		CBaseEntity *pClothes = UTIL_FindEntityByClassname( NULL, "cof_clothesmenu" );

		if ( pClothes && pPlayer )
			pClothes->Use( pPlayer, pPlayer, USE_OFF, 1 );
	}
	else if ( FStrEq( pcmd, "setclothes" ) )
	{
		int value = atoi( CMD_ARGV(1) );
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->m_iUnlockedClothes = value;

		if ( CMD_ARGV(2) )
			CVAR_SET_STRING( "sv_custom_hoodie", CMD_ARGV(2) );
	}
	else if ( FStrEq( pcmd, "adlib" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsInCutscene() )
			return;

		if ( pPlayer && !pPlayer->IsAlive() )
			return;

		if ( pPlayer && pPlayer->m_flNextAdLibTime < gpGlobals->time && g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			int value = atoi( CMD_ARGV(1) );

			switch ( value )
			{
			case 0:
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_VOICE, pPoliceAdLibFollow[ RANDOM_LONG( 0, 6 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 1:
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_VOICE, pPoliceAdLibHelp[ RANDOM_LONG( 0, 4 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 2:
				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_VOICE, pPoliceAdLibSomething[ RANDOM_LONG( 0, 8 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			}

			float nextadlib = 1;
			pPlayer->m_flNextAdLibTime = gpGlobals->time + nextadlib;

			MESSAGE_BEGIN( MSG_ALL, gmsgAdLib );
				WRITE_COORD( nextadlib );
				WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
				WRITE_BYTE( value );
			MESSAGE_END();
		}
	}
	else if ( FStrEq( pcmd, "inventoryuse" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->InventoryUse( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "querydonators" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsDeveloper() )
		{
			ALERT( at_console, "Querying donator statuses: \n" );

			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBaseEntity *plr = UTIL_PlayerByIndex( i );

				if ( plr )
					ALERT( at_console, "Donator status: %s | %i \n", STRING( plr->pev->netname ), plr->m_iNew16a_guess );
			}
		}
	}
	else if ( FStrEq( pcmd, "simonmode" ) )
	{
		if ( !m_iSimonMode )
		{
			if ( g_pGameRules )
			{
				m_bSpawningSimon = true;
				m_flSimonSpawnTime = gpGlobals->time;
			}
		}
		else
		{
			CBaseEntity *pSpawn = UTIL_FindEntityByClassname( NULL, "monster_booksimon" );

			if ( pSpawn )
				pSpawn->TakeDamage( NULL, NULL, pSpawn->pev->health, DMG_CRUSH );
		}
	}
	else if ( FStrEq( pcmd, "wheelchairendvec" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 20 )
		{
			CBasePlayerWeapon *pBrowning = GetClassPtr((CBasePlayerWeapon *)pPlayer->m_pActiveItem->pev);

			if ( pBrowning )
			{
				int x = atoi( CMD_ARGV(1) );
				int y = atoi( CMD_ARGV(2) );
				int z = atoi( CMD_ARGV(3) );

				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vEndVector.x = x;
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vEndVector.y = y;
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vEndVector.z = z;
			}
		}
	}
	else if ( FStrEq( pcmd, "wheelchairfire1" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 20 )
		{
			CBasePlayerWeapon *pBrowning = GetClassPtr((CBasePlayerWeapon *)pPlayer->m_pActiveItem->pev);

			if ( pBrowning && !pPlayer->IsOnLadder() && CanAttack( pBrowning->m_flNextPrimaryAttack, gpGlobals->time, pBrowning->UseDecrement() ) )
			{
				if ( (pBrowning->m_iClip == 0 && pBrowning->pszAmmo1()) || (pBrowning->iMaxClip() == -1 && !pPlayer->m_rgAmmo[pBrowning->PrimaryAmmoIndex()]) )
				{
					pBrowning->m_fFireOnEmpty = TRUE;
				}

				int x = atoi( CMD_ARGV(1) );
				int y = atoi( CMD_ARGV(2) );
				int z = atoi( CMD_ARGV(3) );

				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vAimingVector.x = x;
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vAimingVector.y = y;
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->m_vAimingVector.z = z;

				pPlayer->TabulateAmmo();
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->PrimaryAttack();
			}
		}
	}
	else if ( FStrEq( pcmd, "wheelchairreload" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 20 )
		{
			CBasePlayerWeapon *pBrowning = GetClassPtr((CBasePlayerWeapon *)pPlayer->m_pActiveItem->pev);

			if ( pBrowning )
				GetClassPtr((CBrowningWheelchair *)pPlayer->m_pActiveItem->pev)->Reload();
		}
	}
	else if ( FStrEq( pcmd, "zoominmdl" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			int amount = atoi( CMD_ARGV(1) );

			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pPlayer->pev );
				WRITE_COORD( 50 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pPlayer->pev->vuser2.x = amount;
		}
	}
	else if ( FStrEq( pcmd, "throwflare" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			// flare in the hand
			if ( pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 13 )
			{
				CFlare *pWeaponFlare = GetClassPtr((CFlare *)pPlayer->m_pActiveItem->pev);

				if ( pWeaponFlare )
				{
					UTIL_MakeVectors( pPlayer->pev->v_angle + pPlayer->pev->punchangle );
					Vector vecSrc = pPlayer->EyePosition() + gpGlobals->v_forward * 10;

					TraceResult tr;
					UTIL_TraceLine( pPlayer->EyePosition(), vecSrc, dont_ignore_monsters, pPlayer->edict(), &tr );

					CBaseEntity *pFlare = CBaseEntity::Create( "weapon_flare", tr.vecEndPos, pPlayer->pev->v_angle, pPlayer->edict() );

					if ( pFlare )
					{
						UTIL_MakeVectors( pPlayer->pev->v_angle );
						UTIL_SetOrigin( pFlare, tr.vecEndPos );

						if ( pWeaponFlare->pev->iuser1 > 0 )
						{
							pFlare->pev->iuser1 = pWeaponFlare->pev->iuser1;

							if ( pFlare->pev->iuser1 == 1 )
								pFlare->pev->body = 1;
							else
								pFlare->pev->body = 0;

							pFlare->pev->teleport_time = gpGlobals->time;
							pFlare->pev->owner = NULL;
							pFlare->Spawn();

							pFlare->pev->radsuit_finished = pWeaponFlare->pev->radsuit_finished;
						}

						if ( !pPlayer->m_bInfiniteAmmo )
							pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType]--;

						if ( pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] < 0 )
							pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] = 0;

						MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pPlayer->pev );
							WRITE_BYTE( pWeaponFlare->m_iId );
							WRITE_BYTE( pWeaponFlare->iMaxClip() );
							WRITE_SHORT( pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] );
						MESSAGE_END();

						pWeaponFlare->old_m_iAmmo = pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType];

						pPlayer->m_iFlashlightFlags &= ~16;

						STOP_SOUND( pPlayer->edict(), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

						if ( pWeaponFlare->pev->iuser1 == 1 )
						{
							if ( RANDOM_LONG( 0, 1 ) )
								EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "weapons/flare/flare_movement1.wav", 1, ATTN_NORM );
							else
								EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "weapons/flare/flare_movement2.wav", 1, ATTN_NORM );
						}

						pFlare->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;
					}
				}
			}

			// flare in the left hand of a dual wield
			if ( pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 15 && pPlayer->m_iDualWeaponRightID == 13 )
			{
				CFlare *pWeaponFlare = NULL;
				pWeaponFlare = (CFlare *)pPlayer->GetPlayerWeapon( 13 );

				if ( pWeaponFlare )
				{
					int x = atoi( CMD_ARGV(1) );
					int y = atoi( CMD_ARGV(2) );
					int z = atoi( CMD_ARGV(3) );

					UTIL_MakeVectors( pPlayer->pev->v_angle + pPlayer->pev->punchangle );
					Vector vecSrc = pPlayer->EyePosition() + gpGlobals->v_forward * 10;

					TraceResult tr;
					UTIL_TraceLine( pPlayer->EyePosition(), vecSrc, dont_ignore_monsters, pPlayer->edict(), &tr );

					CBaseEntity *pFlare = CBaseEntity::Create( "weapon_flare", tr.vecEndPos, pPlayer->pev->v_angle, pPlayer->edict() );

					if ( pFlare )
					{
						UTIL_MakeVectors( pPlayer->pev->v_angle );
						UTIL_SetOrigin( pFlare, tr.vecEndPos );

						if ( pWeaponFlare->pev->iuser1 > 0 )
						{
							pFlare->pev->iuser1 = pWeaponFlare->pev->iuser1;

							if ( pFlare->pev->iuser1 == 1 )
								pFlare->pev->body = 1;
							else
								pFlare->pev->body = 0;

							pFlare->pev->teleport_time = gpGlobals->time;
							pFlare->pev->owner = NULL;
							pFlare->Spawn();

							pFlare->pev->radsuit_finished = pWeaponFlare->pev->radsuit_finished;
						}

						if ( !pPlayer->m_bInfiniteAmmo )
							pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType]--;

						if ( pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] < 0 )
							pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] = 0;

						MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pPlayer->pev );
							WRITE_BYTE( pWeaponFlare->m_iId );
							WRITE_BYTE( pWeaponFlare->iMaxClip() );
							WRITE_SHORT( pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType] );
						MESSAGE_END();

						pWeaponFlare->old_m_iAmmo = pPlayer->m_rgAmmo[pWeaponFlare->m_iPrimaryAmmoType];

						pWeaponFlare->m_bThrown = true;
						pPlayer->m_iFlashlightFlags &= ~16;

						STOP_SOUND( pPlayer->edict(), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

						if ( pWeaponFlare->pev->iuser1 == 1 )
						{
							if ( RANDOM_LONG( 0, 1 ) )
								EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "weapons/flare/flare_movement1.wav", 1, ATTN_NORM );
							else
								EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "weapons/flare/flare_movement2.wav", 1, ATTN_NORM );
						}

						if ( pWeaponFlare->pev->iuser1 == 2 )
							EMIT_SOUND( ENT(pPlayer->pev), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );

						pFlare->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * 300 + gpGlobals->v_forward * 100;
					}
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "inventorydualwield" ) )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

			if ( pPlayer )
				pPlayer->DualWield( atoi( CMD_ARGV(1) ), atoi( CMD_ARGV(2) ) );
		}
	}
	else if ( FStrEq( pcmd, "inventorydrop" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->InventoryDrop( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "inventorycombine" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->InventoryCombine( atoi( CMD_ARGV(1) ), atoi( CMD_ARGV(2) ), (char *)CMD_ARGV(3) );
	}
	else if ( FStrEq( pcmd, "inventorycheck" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->InventoryCheck( atoi( CMD_ARGV(1) ) );
	}
	else if ( FStrEq( pcmd, "inventorydeletenamed" ) )
	{
		int all = atoi( CMD_ARGV(2) );

		if ( all )
			GetClassPtr((CBasePlayer *)pev)->ClearOutSlotNamed( MAKE_STRING( CMD_ARGV(1) ), 1 );
		else
			GetClassPtr((CBasePlayer *)pev)->ClearOutSlotNamed( MAKE_STRING( CMD_ARGV(1) ), 0 );
	}
	else if ( FStrEq( pcmd, "boilercheck" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pPuzzleBar )
		{
			int value = atoi( CMD_ARGV(1) );

			// 5 leaves the puzzle
			if ( value == 5 )
			{
				SET_VIEW( pPlayer->edict(), pPlayer->edict() );
				pPlayer->m_pPuzzleBar = NULL;
			}
			else
			{
				GetClassPtr((CPuzzleBarButton *)pPlayer->m_pPuzzleBar->pev)->ToggleBar( value, pPlayer );
			}
		}
	}
	else if ( FStrEq( pcmd, "subtitleset" ) )
	{
		int number = atoi( CMD_ARGV(1) );
		CVAR_SET_FLOAT( "cof_subtitlelanguage", number );

		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
			pPlayer->playerstats.language = number;
	}
	else if ( FStrEq( pcmd, "gamemenucmd" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( strstr( STRING( gpGlobals->mapname ), "game_menu" ) )
		{
			int number = atoi( CMD_ARGV(1) );
			CBaseEntity *pMenu = UTIL_FindEntityByClassname( NULL, "cof_gamemenu" );

			if ( pMenu && pPlayer )
			{
				if ( number == 1 )
				{
					pMenu->pev->iuser1 = 1;
					pMenu->SetNextThink( 1 );
				}

				if ( number == 2 )
				{
					pMenu->pev->iuser1 = 2;
					pMenu->SetNextThink( 2 );
				}

				UTIL_ScreenFade( pPlayer, Vector( 255, 255, 255 ), 1, 10, 255, FFADE_OUT );
			}
		}
	}
	else if ( FStrEq( pcmd, "readylobby" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && g_pGameRules->m_bInLobby )
			{
				if ( g_pGameRules->m_bReady[ENTINDEX( pPlayer->edict() )] )
				{
					g_pGameRules->m_bReady[ENTINDEX( pPlayer->edict() )] = false;

					MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}
				else
				{
					g_pGameRules->m_bReady[ENTINDEX( pPlayer->edict() )] = true;

					MESSAGE_BEGIN( MSG_ALL, gmsgLobby );
						WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
						WRITE_BYTE( 1 );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "loadgamemenu" ) )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

			if ( pPlayer )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgSaveLoad, NULL, pPlayer->pev );
					WRITE_BYTE( 1 );
					WRITE_STRING( "nothing" );
				MESSAGE_END();
			}
		}
	}
	else if ( FStrEq( pcmd, "swimboost" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->pev->waterlevel >= 3 )
		{
			int zvel = pPlayer->pev->velocity.z;

			// sprinting
			if ( pPlayer->pev->flags & (1<<18) )
				pPlayer->pev->velocity = pPlayer->pev->velocity * 5;
			else
				pPlayer->pev->velocity = pPlayer->pev->velocity * 3;

			pPlayer->pev->velocity.z = zvel;
		}
	}
	else if ( FStrEq( pcmd, "swimbubbles" ) )
	{
		int x = atoi( CMD_ARGV(1) );
		int y = atoi( CMD_ARGV(2) );
		int z = atoi( CMD_ARGV(3) );
		Vector vecSrc = Vector( x, y, z );

		CParticle *m_pParticle = CParticle::CreateParticle( MAKE_STRING( "aurora/bubbles.aur" ), true, vecSrc, 0, 1, NULL, 0, 0, 0 );
	}
	else if ( FStrEq( pcmd, "boatdisembark" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pBoat )
			GetClassPtr((CBoat *)pPlayer->m_pBoat->pev)->ExitCheck();
	}
	else if ( FStrEq( pcmd, "boatup" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pBoat )
			GetClassPtr((CBoat *)pPlayer->m_pBoat->pev)->MoveForward();
	}
	else if ( FStrEq( pcmd, "boatdown" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pBoat )
			GetClassPtr((CBoat *)pPlayer->m_pBoat->pev)->MoveBack();
	}
	else if ( FStrEq( pcmd, "boatleft" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pBoat )
			GetClassPtr((CBoat *)pPlayer->m_pBoat->pev)->TurnLeft();
	}
	else if ( FStrEq( pcmd, "boatright" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pBoat )
			GetClassPtr((CBoat *)pPlayer->m_pBoat->pev)->TurnRight();
	}
	else if ( FStrEq( pcmd, "wheelchairup" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pWheelchair )
			GetClassPtr((CWheelchair *)pPlayer->m_pWheelchair->pev)->MoveForward();
	}
	else if ( FStrEq( pcmd, "wheelchairdown" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pWheelchair )
			GetClassPtr((CWheelchair *)pPlayer->m_pWheelchair->pev)->MoveBack();
	}
	else if ( FStrEq( pcmd, "wheelchairleft" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pWheelchair )
			GetClassPtr((CWheelchair *)pPlayer->m_pWheelchair->pev)->TurnLeft();
	}
	else if ( FStrEq( pcmd, "wheelchairright" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pWheelchair )
			GetClassPtr((CWheelchair *)pPlayer->m_pWheelchair->pev)->TurnRight();
	}
	else if ( FStrEq( pcmd, "telescopecancel" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pTelescope && pPlayer->m_pTelescope->pev->health != 0 )
		{
			pPlayer->m_pTelescope->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
	}
	else if ( FStrEq( pcmd, "flashlight" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->IsInCutscene() )
			return;

		if ( pPlayer && pPlayer->m_pAttachedCamera )
			return;

		if ( pPlayer && pPlayer->m_pWheelchair )
			return;

		// doctor mode: the flashlight key toggles the night vision
		if ( g_iDoctorMode )
		{
			if ( pPlayer && pPlayer->m_flNextNightVisionTime < gpGlobals->time && pPlayer->m_flNextNightVisionTime != 666 )
			{
				if ( pPlayer->m_iHeadShieldOn == 1 )
					pPlayer->m_iHeadShieldOn = 100;
				else if ( pPlayer->m_iHeadShieldOn == 0 )
					pPlayer->m_iHeadShieldOn = 101;
			}

			return;
		}

		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

			if ( pPlayer && !pPlayer->m_pAttachedCamera )
			{
				if ( pPlayer->CheckingEndingOrUnlockables( 0x1000000, false, false ) )
				{
					if ( pPlayer->m_bHasNightVision )
					{
						if ( pPlayer->m_flNextNightVisionTime < gpGlobals->time && pPlayer->m_flNextNightVisionTime != 666 )
						{
							if ( pPlayer->m_iHeadShieldOn == 1 )
								pPlayer->m_iHeadShieldOn = 100;
							else if ( pPlayer->m_iHeadShieldOn == 0 )
								pPlayer->m_iHeadShieldOn = 101;
						}
					}
				}
			}
		}

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pPlayer && pPlayer->IsAlive() )
		{
			pPlayer->m_iFlashlightFlags ^= CBasePlayer::COFR_COOP_FLASHLIGHT;
			const char *sound = (pPlayer->m_iFlashlightFlags & CBasePlayer::COFR_COOP_FLASHLIGHT)
				? SOUND_FLASHLIGHT_ON : SOUND_FLASHLIGHT_OFF;
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, sound, 1.0, ATTN_NORM, 0, PITCH_NORM);
		}
	}
	else if ( FStrEq( pcmd, "weapontoggle" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pActiveItem )
			GetClassPtr((CBasePlayerWeapon *)pPlayer->m_pActiveItem->pev)->WeaponToggle();
	}
	else if ( FStrEq( pcmd, "telescopefinish" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			// add a note to the first free document slot
			for ( int i = 1; i <= 16; i++ )
			{
				if ( strlen( pPlayer->documentsstruct[i].note1 ) < 1 )
				{
					strcpy( pPlayer->documentsstruct[i].note1, "nerds" );
					strcpy( pPlayer->documentsstruct[i].page1, "nerds" );
					strcpy( pPlayer->documentsstruct[i].turnsound, "nerds" );
					strcpy( pPlayer->documentsstruct[i].title, "nerds" );
					strcpy( pPlayer->documentsstruct[i].label, "nerds" );

					MESSAGE_BEGIN( MSG_ONE, gmsgAddNote, NULL, pPlayer->pev );
						WRITE_BYTE( i );
						WRITE_BYTE( 1 );
						WRITE_SHORT( pPlayer->m_iKeypadNumber );
						WRITE_BYTE( 3 );
						WRITE_STRING( pPlayer->documentsstruct[i].note1 );
						WRITE_STRING( pPlayer->documentsstruct[i].note2 );
						WRITE_STRING( pPlayer->documentsstruct[i].note3 );
						WRITE_STRING( pPlayer->documentsstruct[i].note4 );
						WRITE_STRING( pPlayer->documentsstruct[i].page1 );
						WRITE_STRING( pPlayer->documentsstruct[i].page2 );
						WRITE_STRING( pPlayer->documentsstruct[i].page3 );
						WRITE_STRING( pPlayer->documentsstruct[i].page4 );
						WRITE_STRING( pPlayer->documentsstruct[i].turnsound );
						WRITE_STRING( pPlayer->documentsstruct[i].title );
						WRITE_STRING( pPlayer->documentsstruct[i].label );
					MESSAGE_END();
					break;
				}
			}
		}

		if ( pPlayer && pPlayer->m_pTelescope && pPlayer->m_pTelescope->pev->health != 0 )
		{
			pPlayer->m_pTelescope->pev->frags = 1;
		}
	}
	else if ( FStrEq( pcmd, "telephoneright" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pTelephone )
		{
			GetClassPtr((CCofTelephone *)pPlayer->m_pTelephone->pev)->TelephoneRight();
		}
	}
	else if ( FStrEq( pcmd, "telephonewrong" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer && pPlayer->m_pTelephone )
		{
			GetClassPtr((CCofTelephone *)pPlayer->m_pTelephone->pev)->TelephoneWrong();
		}
	}
	else if ( FStrEq( pcmd, "cofload" ) )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			int number = atoi( CMD_ARGV(1) );

			switch ( number )
			{
			case 1:
				SERVER_COMMAND( "load cofsave1\n" );
				break;
			case 2:
				SERVER_COMMAND( "load cofsave2\n" );
				break;
			case 3:
				SERVER_COMMAND( "load cofsave3\n" );
				break;
			case 4:
				SERVER_COMMAND( "load cofsave4\n" );
				break;
			case 5:
				SERVER_COMMAND( "load cofsave5\n" );
				break;
			}
		}
	}
	else if ( FStrEq( pcmd, "padlockrotate" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			int number = atoi( CMD_ARGV(1) );

			if ( pPlayer->m_pPadlock )
			{
				CItemPadlock *pPadlock = GetClassPtr((CItemPadlock *)pPlayer->m_pPadlock->pev);

				if ( pPadlock )
					pPadlock->Rotate( number );
			}
		}
	}
	else if ( FStrEq( pcmd, "padlockcancel" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			if ( pPlayer->m_pPadlock )
			{
				CItemPadlock *pPadlock = GetClassPtr((CItemPadlock *)pPlayer->m_pPadlock->pev);

				if ( pPadlock )
					pPadlock->Cancel();
			}
		}
	}
	else if ( FStrEq( pcmd, "yesnoactivate" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( INDEXENT( atoi( CMD_ARGV(1) ) ) );

			if ( pEntity )
			{
				if ( FClassnameIs( pEntity->edict(), "cof_yesno" ) )
				{
					GetClassPtr((CCofYesNo *)pEntity->pev)->YesActivate( pPlayer );
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "yesnono" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr((CBasePlayer *)pev);

		if ( pPlayer )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( INDEXENT( atoi( CMD_ARGV(1) ) ) );

			if ( pEntity )
			{
				if ( FClassnameIs( pEntity->edict(), "cof_yesno" ) )
				{
					GetClassPtr((CCofYesNo *)pEntity->pev)->NoActivate( pPlayer );
				}
			}
		}
	}
	else if ( FStrEq( pcmd, "movestatue" ) )
	{
		// direction, type
		GetClassPtr((CBasePlayer *)pev)->MoveStatue( atoi( CMD_ARGV(1) ), atoi( CMD_ARGV(2) ) );
	}
	else if ( g_pGameRules && g_pGameRules->ClientCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';

		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq( STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" )) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof(sName) - 1 );
		sName[ sizeof(sName) - 1 ] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX(pEntity), infobuffer, "name", sName );

		char text[256];
		sprintf( text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n", 
				STRING( pEntity->v.netname ), 
				GETPLAYERUSERID( pEntity ), 
				GETPLAYERAUTHID( pEntity ),
				GETPLAYERUSERID( pEntity ), 
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
	}

	g_pGameRules->ClientUserInfoChanged( GetClassPtr((CBasePlayer *)&pEntity->v), infobuffer );
}

int g_PrecacheCSWeapons;
static int g_serveractive = 0;
int g_serveractivevar = 0;

void ServerDeactivate( void )
{
	// make sure they reinitialise the World in the next server
	g_pWorld = NULL;

	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate 
	if ( g_serveractive != 1 )
	{
		return;
	}

	g_serveractive = 0;
	g_serveractivevar = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int				i;
	CBaseEntity		*pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;
	g_serveractivevar = 1;

	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;
		
		// Clients aren't necessarily initialized until ClientPutInServer()
		if ( i < clientMax || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !(pClass->pev->flags & FL_DORMANT) )
		{
			pClass->Activate();
		}
		else
		{
			ALERT( at_debug, "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
		}
	}

	// Link user messages here to make sure first client can get them...
	LinkUserMessages();
}

// a cached version of gpGlobals->frametime. The engine sets frametime to 0 if the player is frozen... so we just cache it in prethink,
// allowing it to be restored later and used by CheckDesiredList.
float cached_frametime = 0.0f;

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer) { CoFR_ServerFrame(pPlayer); pPlayer->PreThink(); }

	cached_frametime = gpGlobals->frametime;
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->PostThink( );

	// use the old frametime, even if the engine has reset it
	gpGlobals->frametime = cached_frametime;

	//LRC - moved to here from CBasePlayer::PostThink, so that
	// things don't stop when the player dies
	CheckDesiredList( );
}



void ParmsNewLevel( void )
{
}


void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay.value;
	g_ulFrameCount++;

//	CheckDesiredList(); //LRC
	CheckAssistList(); //LRC
}


void ClientPrecache( void )
{
	int i;

	UTIL_PrecacheOther( "monster_sawcrazy" );

	// setup precaches always needed
	PRECACHE_MODEL( "models/error.mdl" );
	PRECACHE_SOUND( "ambience/van_number.wav" );

	PRECACHE_SOUND( "ambience/really_muschi.wav" );
	PRECACHE_SOUND( "player/morphine_dream.wav" );
	PRECACHE_SOUND( "player/heartbeat1.wav" );

	PRECACHE_SOUND( "ambience/laugh1.wav" );
	PRECACHE_SOUND( "ambience/laugh2.wav" );
	PRECACHE_SOUND( "ambience/laugh3.wav" );

	PRECACHE_SOUND( "player/vocal/underwaterhurt1.wav" );
	PRECACHE_SOUND( "player/vocal/underwaterhurt2.wav" );
	PRECACHE_SOUND( "player/vocal/underwaterhurt3.wav" );
	PRECACHE_SOUND( "player/vocal/water_surface.wav" );

	PRECACHE_SOUND( "player/pl_fallpain.wav" );
	PRECACHE_SOUND( "player/pl_swim.wav" );

	// cache files
	PRECACHE_MODEL( "sprcache" );
	PRECACHE_SOUND( "ambience/soundcache" );

	PRECACHE_MODEL( "models/generic_gore.mdl" );

	PRECACHE_SOUND( "ui/3d_click.wav" );
	PRECACHE_SOUND( "ui/3d_rollover.wav" );

	PRECACHE_SOUND( "stranger/st_hearbeat.wav" );

	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		PRECACHE_SOUND( "player/sprint/bag_movement.wav" );
	else
	{
		PRECACHE_SOUND( "player/sprint/equipment_movement.wav" );

		PRECACHE_SOUND( "common/votekick/votekick_failed.wav" );
		PRECACHE_SOUND( "common/votekick/votekick_notify.wav" );
		PRECACHE_SOUND( "common/votekick/votekick_succeeded.wav" );
	}

	PRECACHE_SOUND( "player/show_hint.wav" );

	PRECACHE_SOUND( "inventory/game_start.wav" );

	PRECACHE_SOUND( "player/vocal/water_breathe.wav" );

	PRECACHE_SOUND( "player/footsteps/crawl1.wav" );
	PRECACHE_SOUND( "player/footsteps/crawl2.wav" );
	PRECACHE_SOUND( "player/footsteps/crawl3.wav" );
	PRECACHE_SOUND( "player/footsteps/crawl4.wav" );

	PRECACHE_SOUND( "player/get_down.wav" );
	PRECACHE_SOUND( "player/get_up.wav" );

	PRECACHE_SOUND( "player/footsteps/concrete1.wav" );
	PRECACHE_SOUND( "player/footsteps/concrete2.wav" );
	PRECACHE_SOUND( "player/footsteps/concrete3.wav" );
	PRECACHE_SOUND( "player/footsteps/concrete4.wav" );

	PRECACHE_SOUND( "player/footsteps/dirt1.wav" );
	PRECACHE_SOUND( "player/footsteps/dirt2.wav" );
	PRECACHE_SOUND( "player/footsteps/dirt3.wav" );
	PRECACHE_SOUND( "player/footsteps/dirt4.wav" );

	PRECACHE_SOUND( "player/footsteps/grass1.wav" );
	PRECACHE_SOUND( "player/footsteps/grass2.wav" );
	PRECACHE_SOUND( "player/footsteps/grass3.wav" );
	PRECACHE_SOUND( "player/footsteps/grass4.wav" );

	PRECACHE_SOUND( "player/footsteps/metal1.wav" );
	PRECACHE_SOUND( "player/footsteps/metal2.wav" );
	PRECACHE_SOUND( "player/footsteps/metal3.wav" );
	PRECACHE_SOUND( "player/footsteps/metal4.wav" );

	PRECACHE_SOUND( "player/footsteps/mud1.wav" );
	PRECACHE_SOUND( "player/footsteps/mud2.wav" );
	PRECACHE_SOUND( "player/footsteps/mud3.wav" );
	PRECACHE_SOUND( "player/footsteps/mud4.wav" );

	PRECACHE_SOUND( "player/footsteps/sand1.wav" );
	PRECACHE_SOUND( "player/footsteps/sand2.wav" );
	PRECACHE_SOUND( "player/footsteps/sand3.wav" );
	PRECACHE_SOUND( "player/footsteps/sand4.wav" );

	PRECACHE_SOUND( "player/footsteps/snow1.wav" );
	PRECACHE_SOUND( "player/footsteps/snow2.wav" );
	PRECACHE_SOUND( "player/footsteps/snow3.wav" );
	PRECACHE_SOUND( "player/footsteps/snow4.wav" );

	PRECACHE_SOUND( "player/footsteps/wood1.wav" );
	PRECACHE_SOUND( "player/footsteps/wood2.wav" );
	PRECACHE_SOUND( "player/footsteps/wood3.wav" );
	PRECACHE_SOUND( "player/footsteps/wood4.wav" );

	PRECACHE_SOUND( "player/exit_water.wav" );

	PRECACHE_SOUND( "player/footsteps/splash1.wav" );
	PRECACHE_SOUND( "player/footsteps/splash2.wav" );
	PRECACHE_SOUND( "player/footsteps/splash3.wav" );
	PRECACHE_SOUND( "player/footsteps/splash4.wav" );

	PRECACHE_SOUND( "player/footsteps/wade1.wav" );
	PRECACHE_SOUND( "player/footsteps/wade2.wav" );
	PRECACHE_SOUND( "player/footsteps/wade3.wav" );
	PRECACHE_SOUND( "player/footsteps/wade4.wav" );

	PRECACHE_SOUND( "player/footsteps/glass1.wav" );
	PRECACHE_SOUND( "player/footsteps/glass2.wav" );
	PRECACHE_SOUND( "player/footsteps/glass3.wav" );
	PRECACHE_SOUND( "player/footsteps/glass4.wav" );

	PRECACHE_SOUND( "player/footsteps/paper1.wav" );
	PRECACHE_SOUND( "player/footsteps/paper2.wav" );
	PRECACHE_SOUND( "player/footsteps/paper3.wav" );
	PRECACHE_SOUND( "player/footsteps/paper4.wav" );

	PRECACHE_SOUND( "player/footsteps/carpet1.wav" );
	PRECACHE_SOUND( "player/footsteps/carpet2.wav" );
	PRECACHE_SOUND( "player/footsteps/carpet3.wav" );
	PRECACHE_SOUND( "player/footsteps/carpet4.wav" );

	PRECACHE_SOUND( "player/footsteps/twig_snap1.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap2.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap3.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap4.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap5.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap6.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap7.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap8.wav" );
	PRECACHE_SOUND( "player/footsteps/twig_snap9.wav" );

	PRECACHE_SOUND( "player/footsteps/gravel1.wav" );
	PRECACHE_SOUND( "player/footsteps/gravel2.wav" );
	PRECACHE_SOUND( "player/footsteps/gravel3.wav" );
	PRECACHE_SOUND( "player/footsteps/gravel4.wav" );

//	PRECACHE_SOUND("plats/train_use1.wav");		// use a train

	PRECACHE_SOUND("buttons/spark5.wav");		// hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");

// player pain sounds
//	PRECACHE_SOUND("player/pl_pain2.wav");
//	PRECACHE_SOUND("player/pl_pain4.wav");
//	PRECACHE_SOUND("player/pl_pain5.wav");
//	PRECACHE_SOUND("player/pl_pain6.wav");
//	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");
	PRECACHE_MODEL( "models/simon_gibs.mdl" );
	PRECACHE_MODEL( "sprites/blood_puff.spr" );

	// night vision
	PRECACHE_SOUND( "unlockables/nv_on.wav" );
	PRECACHE_SOUND( "unlockables/nv_off.wav" );

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	// water
	PRECACHE_SOUND( "player/water_jump.wav" );
	PRECACHE_SOUND( "player/water_small_splash.wav" );
	PRECACHE_SOUND( "player/water_splash.wav" );

#ifdef XENWARRIOR
	PRECACHE_SOUND( SOUND_FLASHLIGHT_IDLE );
#endif

	// geiger sounds

//	PRECACHE_SOUND("player/geiger6.wav");
//	PRECACHE_SOUND("player/geiger5.wav");
//	PRECACHE_SOUND("player/geiger4.wav");
//	PRECACHE_SOUND("player/geiger3.wav");
//	PRECACHE_SOUND("player/geiger2.wav");
//	PRECACHE_SOUND("player/geiger1.wav");

	PRECACHE_MODEL( "models/flash1.mdl" );
	PRECACHE_MODEL( "sprites/scr_set.spr" );

	// sprint breathing
	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		if ( strstr( STRING( gpGlobals->mapname ), "doc_" ) )
		{
			PRECACHE_SOUND( "player/doctor/player_out_of_breath.wav" );
			PRECACHE_SOUND( "player/doctor/sprint_breathe1.wav" );
			PRECACHE_SOUND( "player/doctor/sprint_breathe2.wav" );
			PRECACHE_SOUND( "player/doctor/sprint_breathe3.wav" );
			PRECACHE_SOUND( "player/doctor/sprint_breathe4.wav" );
			PRECACHE_SOUND( "player/doctor/sprint_breathe5.wav" );
		}
		else
		{
			PRECACHE_SOUND( "player/player_out_of_breath.wav" );
			PRECACHE_SOUND( "player/sprint/sprint_breathe1.wav" );
			PRECACHE_SOUND( "player/sprint/sprint_breathe2.wav" );
			PRECACHE_SOUND( "player/sprint/sprint_breathe3.wav" );
			PRECACHE_SOUND( "player/sprint/sprint_breathe4.wav" );
			PRECACHE_SOUND( "player/sprint/sprint_breathe5.wav" );
		}
	}
	else
	{
		PRECACHE_SOUND( "player/police/player_out_of_breath1.wav" );
		PRECACHE_SOUND( "player/police/player_out_of_breath2.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe1.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe2.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe3.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe4.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe5.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe6.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe7.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe8.wav" );
		PRECACHE_SOUND( "player/police/sprint_breathe9.wav" );
	}

	// inventory
	PRECACHE_SOUND( "inventory/inventory_click.wav" );
	PRECACHE_SOUND( "inventory/inventory_close.wav" );
	PRECACHE_SOUND( "inventory/inventory_no.wav" );
	PRECACHE_SOUND( "inventory/inventory_open.wav" );
	PRECACHE_SOUND( "inventory/inventory_toggle.wav" );

	// player voice
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		for ( i = 0; i < ARRAYSIZE( pPoliceAdLibFollow ); i++ )
			PRECACHE_SOUND( (char *)pPoliceAdLibFollow[i] );

		for ( i = 0; i < ARRAYSIZE( pPoliceAdLibHelp ); i++ )
			PRECACHE_SOUND( (char *)pPoliceAdLibHelp[i] );

		for ( i = 0; i < ARRAYSIZE( pPoliceAdLibSomething ); i++ )
			PRECACHE_SOUND( (char *)pPoliceAdLibSomething[i] );

		for ( i = 0; i < ARRAYSIZE( pPainSoundsPolice ); i++ )
			PRECACHE_SOUND( (char *)pPainSoundsPolice[i] );

		for ( i = 0; i < ARRAYSIZE( pDeathSoundsPolice ); i++ )
			PRECACHE_SOUND( (char *)pDeathSoundsPolice[i] );

		for ( i = 0; i < ARRAYSIZE( pSwingSoundsPolice ); i++ )
			PRECACHE_SOUND( (char *)pSwingSoundsPolice[i] );

		for ( i = 0; i < ARRAYSIZE( pMorphineInjectionSoundsPolice ); i++ )
			PRECACHE_SOUND( (char *)pMorphineInjectionSoundsPolice[i] );

		for ( i = 0; i < ARRAYSIZE( pLowHealthSoundsPolice ); i++ )
			PRECACHE_SOUND( (char *)pLowHealthSoundsPolice[i] );
	}
	else if ( strstr( STRING( gpGlobals->mapname ), "doc_" ) )
	{
		for ( i = 0; i < ARRAYSIZE( pPainSoundsDoctor ); i++ )
			PRECACHE_SOUND( (char *)pPainSoundsDoctor[i] );

		for ( i = 0; i < ARRAYSIZE( pDeathSoundsDoctor ); i++ )
			PRECACHE_SOUND( (char *)pDeathSoundsDoctor[i] );

		for ( i = 0; i < ARRAYSIZE( pSwingSoundsDoctor ); i++ )
			PRECACHE_SOUND( (char *)pSwingSoundsDoctor[i] );

		for ( i = 0; i < ARRAYSIZE( pLowHealthSoundsDoctor ); i++ )
			PRECACHE_SOUND( (char *)pLowHealthSoundsDoctor[i] );
	}
	else
	{
		for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
			PRECACHE_SOUND( (char *)pPainSounds[i] );

		for ( i = 0; i < ARRAYSIZE( pDeathSounds ); i++ )
			PRECACHE_SOUND( (char *)pDeathSounds[i] );

		for ( i = 0; i < ARRAYSIZE( pSwingSounds ); i++ )
			PRECACHE_SOUND( (char *)pSwingSounds[i] );

		for ( i = 0; i < ARRAYSIZE( pMorphineInjectionSounds ); i++ )
			PRECACHE_SOUND( (char *)pMorphineInjectionSounds[i] );

		for ( i = 0; i < ARRAYSIZE( pLowHealthSounds ); i++ )
			PRECACHE_SOUND( (char *)pLowHealthSounds[i] );
	}

	for ( i = 0; i < ARRAYSIZE( pVomitSounds ); i++ )
		PRECACHE_SOUND( (char *)pVomitSounds[i] );
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/

const char *GetGameDescription()
{
//	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
//		return g_pGameRules->GetGameDescription();
//	else
		return "Cry of Fear";
//	static char buf[128];
//	sprintf (buf, "Half-Life: Paranoia (last build %s)", __DATE__);
//	return buf;
}

/*
================
Sys_Error

Engine is going to shut down, allows setting a breakpoint in game .dll to catch that occasion
================
*/
void Sys_Error( const char *error_string )
{
	// Default case, do nothing.  MOD AUTHORS:  Add code ( e.g., _asm { int 3 }; here to cause a breakpoint for debugging your game .dlls
}

/*
================
PlayerCustomization

A new player customization has been registered on the server
UNDONE:  This only sets the # of frames of the spray can logo
animation right now.
================
*/
void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (!pPlayer)
	{
		ALERT(at_debug, "PlayerCustomization:  Couldn't get player!\n");
		return;
	}

	if (!pCust)
	{
		ALERT(at_debug, "PlayerCustomization:  NULL customization!\n");
		return;
	}

	switch (pCust->resource.type)
	{
	case t_decal:
		pPlayer->SetCustomDecalFrames(pCust->nUserData2); // Second int is max # of frames.
		break;
	case t_sound:
	case t_skin:
	case t_model:
		// Ignore for now.
		break;
	default:
		ALERT(at_debug, "PlayerCustomization:  Unknown customization type!\n");
		break;
	}
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect( );
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect( );
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink( );
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	Vector org;
	edict_t *pView = pClient;

	// Find the client's PVS
	if ( pViewEntity )
	{
		pView = pViewEntity;
	}

	CBasePlayer *pPlayer = NULL;

	// boats, wheelchairs, cameras and death scenes move the view
	if ( pClient->v.flags & FL_CLIENT )
	{
		CBaseEntity *pCast = CBaseEntity::Instance( pClient );

		if ( pCast )
		{
			pPlayer = GetClassPtr( (CBasePlayer *)pCast->pev );

			if ( pPlayer && ( pPlayer->m_pBoat || pPlayer->m_pWheelchair || pPlayer->m_pAttachedCamera || pPlayer->pDeathScene ) )
			{
				if ( pPlayer->m_pBoat )
					pView = pPlayer->m_pBoat->edict();

				if ( pPlayer->m_pAttachedCamera )
				{
					pView = pPlayer->m_pAttachedCamera->edict();
				}

				if ( pPlayer->m_pWheelchair )
					pView = pPlayer->m_pWheelchair->edict();

				if ( pPlayer->pDeathScene )
				{
					pView = pPlayer->pDeathScene->edict();
				}
			}
		}
	}

	if ( pClient->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}

	org = pView->v.origin + pView->v.view_ofs;
	if ( pView->v.flags & FL_DUCKING )
	{
		org = org + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	}

	*pvs = ENGINE_SET_PVS ( (float *)&org );
	*pas = ENGINE_SET_PAS ( (float *)&org );
}

#include "entity_state.h"

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	int					i;

	// don't send if flagged for NODRAW and it's not the host getting the message
	if ( ( ent->v.effects == EF_NODRAW ) &&
		 ( ent != host ) )
		return 0;

	// Ignore ents without valid / visible models
	if ( !ent->v.modelindex || !STRING( ent->v.model ) )
		return 0;

	// Don't send spectators to other players
	if ( ( ent->v.flags & FL_SPECTATOR ) && ( ent != host ) )
	{
		return 0;
	}

	// Ignore if not the host and not touching a PVS/PAS leaf
	// If pSet is NULL, then the test will always succeed and the entity will be added to the update
	if ( ent != host && ent->v.renderfx != 70 && ent->v.renderfx != 137 && ent->v.renderfx != 85 ) // buz: always send 3d skybox ents
	{
		if ( !ENGINE_CHECK_VISIBILITY( (const struct edict_s *)ent, pSet ) )
		{
			return 0;
		}
	}


	// Don't send entity to local client if the client says it's predicting the entity itself.
	if ( ent->v.flags & FL_SKIPLOCALHOST )
	{
		if ( ( hostflags & 1 ) && ( ent->v.owner == host ) )
			return 0;
	}
	
	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !(ent->v.groupinfo & host->v.groupinfo ) )
					return 0;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & host->v.groupinfo )
					return 0;
			}
		}

		UTIL_UnsetGroupTrace();
	}

	memset( state, 0, sizeof( *state ) );

	// Assign index so we can track this entity from frame to frame and
	//  delta from it.
	state->number	  = e;
	state->entityType = ENTITY_NORMAL;
	
	// Flag custom entities.
	if ( ent->v.flags & FL_CUSTOMENTITY )
	{
		state->entityType = ENTITY_BEAM;
	}

	// 
	// Copy state data
	//

	// Round animtime to nearest millisecond
	state->animtime   = (int)(1000.0 * ent->v.animtime ) / 1000.0;

	memcpy( state->origin, ent->v.origin, 3 * sizeof( float ) );
	memcpy( state->angles, ent->v.angles, 3 * sizeof( float ) );
	memcpy( state->mins, ent->v.mins, 3 * sizeof( float ) );
	memcpy( state->maxs, ent->v.maxs, 3 * sizeof( float ) );

	memcpy( state->startpos, ent->v.startpos, 3 * sizeof( float ) );
	memcpy( state->endpos, ent->v.endpos, 3 * sizeof( float ) );

	state->impacttime = ent->v.impacttime;
	state->starttime = ent->v.starttime;

	state->modelindex = ent->v.modelindex;
		
	state->frame      = ent->v.frame;

	state->skin       = ent->v.skin;
	state->effects    = ent->v.effects;

	// This non-player entity is being moved by the game .dll and not the physics simulation system
	//  make sure that we interpolate it's position on the client if it moves
	if ( !player &&
		 ent->v.animtime &&
		 ent->v.velocity[ 0 ] == 0 && 
		 ent->v.velocity[ 1 ] == 0 && 
		 ent->v.velocity[ 2 ] == 0 )
	{
		state->eflags |= EFLAG_SLERP;
	}

	state->scale	  = ent->v.scale;
	state->solid	  = ent->v.solid;
//	state->colormap   = ent->v.colormap;
	state->colormap   = ENTINDEX(ent); // buz: particle system will search by this

	state->movetype   = ent->v.movetype;
	state->sequence   = ent->v.sequence;
	state->framerate  = ent->v.framerate;
	state->body       = ent->v.body;

	for (i = 0; i < 4; i++)
	{
		state->controller[i] = ent->v.controller[i];
	}

	for (i = 0; i < 2; i++)
	{
		state->blending[i]   = ent->v.blending[i];
	}

	state->rendermode    = ent->v.rendermode;
	state->renderamt     = ent->v.renderamt; 
	state->renderfx      = ent->v.renderfx;
	state->rendercolor.r = ent->v.rendercolor.x;
	state->rendercolor.g = ent->v.rendercolor.y;
	state->rendercolor.b = ent->v.rendercolor.z;

	state->aiment = 0;
	if ( ent->v.aiment )
	{
		state->aiment = ENTINDEX( ent->v.aiment );
	}

	state->owner = 0;
	if ( ent->v.owner )
	{
		int owner = ENTINDEX( ent->v.owner );
		
		// Only care if owned by a player
		if ( owner >= 1 && owner <= gpGlobals->maxClients )
		{
			state->owner = owner;	
		}
	}

	// HACK:  Somewhat...
	// Class is overridden for non-players to signify a breakable glass object ( sort of a class? )
	// that's 'class' in the sense medic, engineer, etc... !! --LRC
	if ( !player )
	{
		state->playerclass  = ent->v.playerclass;
	}

	// Special stuff for players only
	if ( player )
	{
		memcpy( state->basevelocity, ent->v.basevelocity, 3 * sizeof( float ) );

		state->weaponmodel  = MODEL_INDEX( STRING( ent->v.weaponmodel ) );
		state->gaitsequence = ent->v.gaitsequence;
		state->spectator = ent->v.flags & FL_SPECTATOR;
		state->friction     = ent->v.friction;

		state->gravity      = ent->v.gravity;
//		state->team			= ent->v.team;
//		
		state->usehull      = ( ent->v.flags & FL_DUCKING ) ? 1 : 0;
		state->health		= ent->v.health;
		// CoF does not send a player class for remote co-op players. Its
		// existing 16-bit snapshot field carries the tagged light state.
		if ( ent != host && g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			auto *owner = static_cast<CBasePlayer *>(CBaseEntity::Instance(ent));
			if ( owner && owner->IsAlive() && !state->spectator &&
			     !owner->IsInCutscene() && !owner->m_pAttachedCamera &&
			     !owner->m_pWheelchair && !owner->m_pBoat )
				state->playerclass = 0x4000 | (owner->m_iFlashlightFlags & 0x2f);
		}
	}

	return 1;
}

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	28

/*
===================
CreateBaseline

Creates baselines used for network encoding, especially for player data since players are not spawned until connect time.
===================
*/
void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	baseline->origin		= entity->v.origin;
	baseline->angles		= entity->v.angles;
	baseline->frame			= entity->v.frame;
	baseline->skin			= (short)entity->v.skin;

	// render information
	baseline->rendermode	= (byte)entity->v.rendermode;
	baseline->renderamt		= (byte)entity->v.renderamt;
	baseline->rendercolor.r	= (byte)entity->v.rendercolor.x;
	baseline->rendercolor.g	= (byte)entity->v.rendercolor.y;
	baseline->rendercolor.b	= (byte)entity->v.rendercolor.z;
	baseline->renderfx		= (byte)entity->v.renderfx;

	if ( player )
	{
		baseline->mins			= player_mins;
		baseline->maxs			= player_maxs;

		baseline->colormap		= eindex;
		baseline->modelindex	= playermodelindex;
		baseline->friction		= 1.0;
		baseline->movetype		= MOVETYPE_WALK;

		baseline->scale			= entity->v.scale;
		baseline->solid			= SOLID_SLIDEBOX;
		baseline->framerate		= 1.0;
		baseline->gravity		= 1.0;

	}
	else
	{
		baseline->mins			= entity->v.mins;
		baseline->maxs			= entity->v.maxs;

		baseline->colormap		= 0;
		baseline->modelindex	= entity->v.modelindex;//SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype		= entity->v.movetype;

		baseline->scale			= entity->v.scale;
		baseline->solid			= entity->v.solid;
		baseline->framerate		= entity->v.framerate;
		baseline->gravity		= entity->v.gravity;
	}
}

typedef struct
{
	char name[32];
	int	 field;
} entity_field_alias_t;

#define FIELD_ORIGIN0			0
#define FIELD_ORIGIN1			1
#define FIELD_ORIGIN2			2
#define FIELD_ANGLES0			3
#define FIELD_ANGLES1			4
#define FIELD_ANGLES2			5

static entity_field_alias_t entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
};

void Entity_FieldInit( struct delta_s *pFields )
{
	entity_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN0 ].name );
	entity_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN1 ].name );
	entity_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ORIGIN2 ].name );
	entity_field_alias[ FIELD_ANGLES0 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES0 ].name );
	entity_field_alias[ FIELD_ANGLES1 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES1 ].name );
	entity_field_alias[ FIELD_ANGLES2 ].field		= DELTA_FINDFIELD( pFields, entity_field_alias[ FIELD_ANGLES2 ].name );
}

/*
==================
Entity_Encode

Callback for sending entity_state_t info over network. 
FIXME:  Move to script
==================
*/
void Entity_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->impacttime != 0 ) && ( t->starttime != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );

		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ANGLES2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

static entity_field_alias_t player_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
};

void Player_FieldInit( struct delta_s *pFields )
{
	player_field_alias[ FIELD_ORIGIN0 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN0 ].name );
	player_field_alias[ FIELD_ORIGIN1 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN1 ].name );
	player_field_alias[ FIELD_ORIGIN2 ].field		= DELTA_FINDFIELD( pFields, player_field_alias[ FIELD_ORIGIN2 ].name );
}

/*
==================
Player_Encode

Callback for sending entity_state_t for players info over network. 
==================
*/
void Player_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int localplayer = 0;
	static int initialized = 0;

	if ( !initialized )
	{
		Player_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	// Never send origin to local player, it's sent with more resolution in clientdata_t structure
	localplayer =  ( t->number - 1 ) == ENGINE_CURRENT_PLAYER();
	if ( localplayer )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}

	if ( ( t->movetype == MOVETYPE_FOLLOW ) &&
		 ( t->aiment != 0 ) )
	{
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
	else if ( t->aiment != f->aiment )
	{
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN0 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN1 ].field );
		DELTA_SETBYINDEX( pFields, entity_field_alias[ FIELD_ORIGIN2 ].field );
	}
}

#define CUSTOMFIELD_ORIGIN0			0
#define CUSTOMFIELD_ORIGIN1			1
#define CUSTOMFIELD_ORIGIN2			2
#define CUSTOMFIELD_ANGLES0			3
#define CUSTOMFIELD_ANGLES1			4
#define CUSTOMFIELD_ANGLES2			5
#define CUSTOMFIELD_SKIN			6
#define CUSTOMFIELD_SEQUENCE		7
#define CUSTOMFIELD_ANIMTIME		8

entity_field_alias_t custom_entity_field_alias[]=
{
	{ "origin[0]",			0 },
	{ "origin[1]",			0 },
	{ "origin[2]",			0 },
	{ "angles[0]",			0 },
	{ "angles[1]",			0 },
	{ "angles[2]",			0 },
	{ "skin",				0 },
	{ "sequence",			0 },
	{ "animtime",			0 },
};

void Custom_Entity_FieldInit( struct delta_s *pFields )
{
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field	= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].name );
	custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].name );
	custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field= DELTA_FINDFIELD( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].name );
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode( struct delta_s *pFields, const unsigned char *from, const unsigned char *to )
{
	entity_state_t *f, *t;
	int beamType;
	static int initialized = 0;

	if ( !initialized )
	{
		Custom_Entity_FieldInit( pFields );
		initialized = 1;
	}

	f = (entity_state_t *)from;
	t = (entity_state_t *)to;

	beamType = t->rendermode & 0x0f;
		
	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ORIGIN2 ].field );
	}

	if ( beamType != BEAM_POINTS )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES0 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES1 ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANGLES2 ].field );
	}

	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SKIN ].field );
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_SEQUENCE ].field );
	}

	// animtime is compared by rounding first
	// see if we really shouldn't actually send it
	if ( (int)f->animtime == (int)t->animtime )
	{
		DELTA_UNSETBYINDEX( pFields, custom_entity_field_alias[ CUSTOMFIELD_ANIMTIME ].field );
	}
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders( void )
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
#if defined( CLIENT_WEAPONS )
	int i;
	weapon_data_t *item;
	entvars_t *pev = &player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );
	CBasePlayerWeapon *gun;
	
	ItemInfo II;

	memset( info, 0, 32 * sizeof( weapon_data_t ) );

	if ( !pl )
		return 1;

	// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( pl->m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = pl->m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();
				if ( gun && gun->UseDecrement() )
				{
					// Get The ID.
					memset( &II, 0, sizeof( II ) );
					gun->GetItemInfo( &II );

					if ( II.iId >= 0 && II.iId < 32 )
					{
						item = &info[ II.iId ];
					 	
						item->m_iId						= II.iId;
						item->m_iClip					= gun->m_iClip;

						item->m_flTimeWeaponIdle		= max( gun->m_flTimeWeaponIdle, -0.001 );
						item->m_flNextPrimaryAttack		= max( gun->m_flNextPrimaryAttack, -0.001 );
						item->m_flNextSecondaryAttack	= max( gun->m_flNextSecondaryAttack, -0.001 );
						item->m_fInReload				= gun->m_fInReload;
						item->m_fInSpecialReload		= gun->m_fInSpecialReload;
						item->fuser1					= max( gun->pev->fuser1, -0.001 );
						item->fuser2					= gun->m_flStartThrow;
						item->fuser3					= gun->m_flReleaseThrow;
						item->iuser1					= gun->m_chargeReady;
						item->iuser2					= gun->m_fInAttack;
						item->iuser3					= gun->m_fireState;
						
											
//						item->m_flPumpTime				= max( gun->m_flPumpTime, -0.001 );
					}
				}
				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
#else
	memset( info, 0, 32 * sizeof( weapon_data_t ) );
#endif
	return 1;
}

/*
=================
UpdateClientData

Data sent to current client only
engine sets cd to 0 before calling.
=================
*/
void UpdateClientData ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd )
{
	cd->flags			= ent->v.flags;
	cd->health			= ent->v.health;

	cd->viewmodel		= MODEL_INDEX( STRING( ent->v.viewmodel ) );

	cd->waterlevel		= ent->v.waterlevel;
	cd->watertype		= ent->v.watertype;
	cd->weapons			= ent->v.weapons;

	// Vectors
	cd->origin			= ent->v.origin;
	cd->velocity		= ent->v.velocity;
	cd->view_ofs		= ent->v.view_ofs;

	// no punch angles in coop
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		cd->punchangle	= g_vecZero;
	else
		cd->punchangle	= ent->v.punchangle;

	cd->bInDuck			= ent->v.bInDuck;
	cd->flTimeStepSound = ent->v.flTimeStepSound;
	cd->flDuckTime		= ent->v.flDuckTime;
	cd->flSwimTime		= ent->v.flSwimTime;
	cd->waterjumptime	= ent->v.teleport_time;

	strcpy( cd->physinfo, ENGINE_GETPHYSINFO( ent ) );

	cd->maxspeed		= ent->v.maxspeed;
	cd->fov				= ent->v.fov;
	cd->weaponanim		= ent->v.weaponanim;

	cd->pushmsec		= ent->v.pushmsec;

	entvars_t *pev = (entvars_t *)&ent->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if ( pl )
	{
		cd->vuser2.x = pl->pev->vuser2.x;
		cd->vuser2.y = pl->pev->vuser2.y;
		cd->vuser2.z = pl->pev->vuser2.z;
		cd->vuser4.x = pl->pev->vuser4.x;

		if ( pl->m_pActiveItem )
			cd->vuser4.y = pl->m_pActiveItem->m_Accuracy;
		else
			cd->vuser4.y = 0;

		cd->vuser4.z = pl->pev->vuser4.z;
	}

	if ( pl && pl->m_pActiveItem )
	{
		// m16 burst mode for the hud
		if ( FClassnameIs( pl->m_pActiveItem->edict(), "weapon_m16" ) )
		{
			CBasePlayerWeapon *pM16 = pl->GetPlayerWeapon( 9 );

			if ( pM16 )
			{
				if ( ((CM16 *)pM16)->b_BurstMode )
					cd->vuser3.x = 1;
				else
					cd->vuser3.x = 0;
			}
		}

		cd->vuser3.y = ((CBasePlayerWeapon *)pl->m_pActiveItem)->m_flCurrentKick;
		cd->vuser3.z = pl->pev->vuser3.z;
	}

	if ( pl )
	{
		cd->vuser1 = pl->pev->vuser1;
#ifndef COFR_XASH_ABI
		// Xash has no trailing clientdata extension; the client never reads it.
		cd->isdonator = pl->m_iNew16a_guess;
#endif
	}
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
//	ALERT(at_console, "got usercmd with lightlevel %f\n", cmd->viewangles[2]);

	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;

	pl->m_iNew16a_guess = cmd->is_donator;

	if ( cmd->viewangles[2] > 0 )
		pl->m_iUsingMic = 1;
	else
		pl->m_iUsingMic = 0;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd ( const edict_t *player )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;
	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

/*
================================
GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:				// Normal player
		mins = VEC_HULL_MIN;
		maxs = VEC_HULL_MAX;
		iret = 1;
		break;
	case 1:				// Crouched player
		mins = VEC_DUCK_HULL_MIN;
		maxs = VEC_DUCK_HULL_MAX;
		iret = 1;
		break;
	case 2:				// Point based hull
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
CreateInstancedBaselines

Create pseudo-baselines for items that aren't placed in the map at spawn time, but which are likely
to be created during play ( e.g., grenades, ammo packs, projectiles, corpses, etc. )
================================
*/
void CreateInstancedBaselines ( void )
{
	int iret = 0;
	entity_state_t state;

	memset( &state, 0, sizeof( state ) );

	// Create any additional baselines here for things like grendates, etc.
	// iret = ENGINE_INSTANCE_BASELINE( pc->pev->classname, &state );

	// Destroy objects.
	//UTIL_Remove( pc );
}

/*
================================
InconsistentFile

One of the ENGINE_FORCE_UNMODIFIED files failed the consistency check for the specified player
 Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
================================
*/
int	InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( disconnect_message, "Server is enforcing file consistency for %s\n", filename );

	// Kick now with specified disconnect message.
	return 1;
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too, 
  if you want.
================================
*/
int AllowLagCompensation( void )
{
	return 1;
}
