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
// cof_interdoorcoop.cpp - co-op versions of the inter doors and the co-op
// level change door
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "shake.h"
#include "cdll_dll.h"
#include "studio.h"
#include "animation.h"
#include <fstream>

extern int m_iSimonMode;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// cof_cooplevelchangemanager header, not rebuilt yet (declared the same way in effects.cpp)
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

extern CCoopLevelChangeManager *g_pLevelChange;

class CInterDoorCoop : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	void SetCollision( void );
	void EnableDoor( bool disable );
	bool m_bDisabled;
	int m_iNewLevelDoorSound;
	void StartSequence( void );
	int IsLocked( void );
	void EXPORT EndSequence( void );
	void UnlockDoor( CBaseEntity *pActivator );
	void ShiftMonsters( void );
	bool m_bSilentRoom;
	void EXPORT EndGame( void );
	CBaseEntity *pWalkEnt;
	void SetSequenceByName( char *szSequence );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_sMusic;
	string_t m_sLockedBy;
	string_t m_sLockedMsg;
	string_t m_sUnlockedMsg;
	string_t m_sProperTarget;
	string_t m_sUseUnlock;
	float m_fNextUse;
	float m_iMusicFadeOut;
	float m_iMusicFadeIn;
};

TYPEDESCRIPTION CInterDoorCoop::m_SaveData[] =
{
	DEFINE_FIELD( CInterDoorCoop, m_sMusic, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_sLockedBy, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_sLockedMsg, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_sUnlockedMsg, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_sProperTarget, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_sUseUnlock, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, m_fNextUse, FIELD_TIME ),
	DEFINE_FIELD( CInterDoorCoop, m_iMusicFadeOut, FIELD_FLOAT ),
	DEFINE_FIELD( CInterDoorCoop, m_iMusicFadeIn, FIELD_FLOAT ),
	DEFINE_FIELD( CInterDoorCoop, m_bSilentRoom, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInterDoorCoop, m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInterDoorCoop, m_iNewLevelDoorSound, FIELD_STRING ),
	DEFINE_FIELD( CInterDoorCoop, pWalkEnt, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CInterDoorCoop, CBaseAnimating );

LINK_ENTITY_TO_CLASS( inter_door_coop, CInterDoorCoop );

//=========================================================
// SetSequenceByName
//=========================================================
void CInterDoorCoop::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CInterDoorCoop::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "opensound" ) )
	{
		pev->noise = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "closesound" ) )
	{
		pev->noise1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "music" ) )
	{
		m_sMusic = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lockedby" ) )
	{
		m_sLockedBy = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lockedmsg" ) )
	{
		m_sLockedMsg = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lockedsnd" ) )
	{
		pev->noise2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "unlockedsnd" ) )
	{
		pev->noise3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "unlockedmsg" ) )
	{
		m_sUnlockedMsg = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "propertarget" ) )
	{
		m_sProperTarget = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "useunlock" ) )
	{
		m_sUseUnlock = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "musicfadein" ) )
	{
		m_iMusicFadeIn = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "musicfadeout" ) )
	{
		m_iMusicFadeOut = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "silentroom" ) )
	{
		int value = atoi( pkvd->szValue );

		if ( value )
			m_bSilentRoom = true;
		else
			m_bSilentRoom = false;

		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "newlevelsnd" ) )
	{
		m_iNewLevelDoorSound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseAnimating::KeyValue( pkvd );
}

void CInterDoorCoop::SetCollision( void )
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ) );

	if ( pstudiohdr == NULL )
	{
		ALERT( at_console, "Unable to fetch model pointer!\n" );
		return;
	}

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize( pev, pseqdesc[ pev->sequence ].bbmin, pseqdesc[ pev->sequence ].bbmax );
}

void CInterDoorCoop::Spawn( void )
{
	if ( pev->spawnflags & 4 )
		m_bDisabled = true;

	Precache( );

	pev->solid = SOLID_SLIDEBOX;

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/coop/door_coop.mdl" );

	SetCollision();

	m_fNextUse = gpGlobals->time;

	if ( m_iMusicFadeIn == 0 )
		m_iMusicFadeIn = 3;

	if ( m_iMusicFadeOut == 0 )
		m_iMusicFadeOut = 3;
}

void CInterDoorCoop::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );

	if ( pev->noise1 )
		PRECACHE_SOUND( (char *)STRING( pev->noise1 ) );

	if ( pev->noise2 )
		PRECACHE_SOUND( (char *)STRING( pev->noise2 ) );

	if ( pev->noise3 )
		PRECACHE_SOUND( (char *)STRING( pev->noise3 ) );

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/coop/door_coop.mdl" );
}

int CInterDoorCoop::IsLocked( void )
{
	if ( m_sLockedBy )
		return TRUE;

	return FALSE;
}

void CInterDoorCoop::UnlockDoor( CBaseEntity *pActivator )
{
	if ( !pActivator )
		return;

	if ( pev->noise3 )
		EMIT_SOUND( ENT( pev ), CHAN_BODY, STRING( pev->noise3 ), 1, 0.8 );

	if ( m_sUnlockedMsg )
		UTIL_ProMessage( pActivator, STRING( m_sUnlockedMsg ), 5, false );

	m_sLockedBy = 0;
	m_sLockedMsg = 0;
	m_fNextUse = gpGlobals->time + 1;
}

void CInterDoorCoop::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( m_bDisabled )
		return;

	if ( m_fNextUse > gpGlobals->time )
		return;

	// unlock another door from this side
	if ( m_sUseUnlock )
	{
		CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_sUseUnlock ) );

		if ( pEntity && FClassnameIs( pEntity->pev, "inter_door" ) )
		{
			CInterDoorCoop *pDoor = GetClassPtr( (CInterDoorCoop *)pEntity->pev );

			if ( pDoor && pDoor->IsLocked() )
				pDoor->UnlockDoor( pActivator );

			m_sUseUnlock = 0;
			m_fNextUse = gpGlobals->time + 1;
			return;
		}
	}

	if ( pev->owner )
	{
		char szMessage[128];
		CBaseEntity *pUser = CBaseEntity::Instance( pev->owner );

		sprintf( szMessage, "I can't use this while %s is using it", STRING( pUser->pev->netname ) );
		UTIL_ProMessage( pActivator, szMessage, 5, false );
		m_fNextUse = gpGlobals->time + 1;
		return;
	}

	if ( pActivator->IsPlayer() && m_iNewLevelDoorSound )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
			pPlayer->g_iDoorSound = m_iNewLevelDoorSound;
	}

	if ( IsLocked() )
	{
		if ( m_sLockedMsg )
			UTIL_ProMessage( pActivator, STRING( m_sLockedMsg ), 5, false );

		if ( pev->noise2 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, STRING( pev->noise2 ), 1, 0.8 );
	}
	else
	{
		pev->owner = pActivator->edict();
		StartSequence();
	}

	m_fNextUse = gpGlobals->time + 1;
}

void CInterDoorCoop::StartSequence( void )
{
	CBaseEntity *pUser;

	if ( pev->owner )
		pUser = CBaseEntity::Instance( pev->owner );
	else
	{
		pev->owner = NULL;
		return;
	}

	if ( pUser )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pUser->pev );

		if ( !pPlayer )
			return;

		pPlayer->b_GoingThroughADoor = true;

		SetSequenceByName( "door_open" );

		// a player model walks through the door while it opens
		UTIL_MakeVectors( pev->angles );
		Vector vecEnd = pev->origin - gpGlobals->v_forward * 16;

		if ( !pWalkEnt )
			pWalkEnt = CBaseEntity::Create( "env_model", vecEnd, pev->angles, edict() );

		if ( pWalkEnt )
		{
			SET_MODEL( ENT( pWalkEnt->pev ), "models/player.mdl" );
			UTIL_SetOrigin( pWalkEnt, vecEnd );
			pWalkEnt->pev->origin.z += 36;
			pWalkEnt->pev->controller[0] = 127;
			pWalkEnt->pev->controller[1] = 127;
			pWalkEnt->pev->controller[2] = 127;
			pWalkEnt->pev->controller[3] = 127;
			pWalkEnt->pev->angles = g_vecZero;
			pWalkEnt->pev->angles.y = pev->angles.y;
			((CBaseMonster *)pWalkEnt)->SetSequenceByName( "door" );
		}

		if ( pev->noise )
			EMIT_SOUND( ENT( pPlayer->pev ), CHAN_AUTO, STRING( pev->noise ), 1, 0.8 );

		// freeze the player while the door opens
		pPlayer->EnableControl( FALSE );
		pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;
		pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS;
		pPlayer->pev->flags |= FL_NOTARGET;

		if ( pPlayer->m_strCurrentMp3 )
		{
			if ( m_bSilentRoom )
			{
				if ( m_iMusicFadeOut != -1 )
					pPlayer->PlayerStopMP3( m_iMusicFadeOut );
				else
					pPlayer->PlayerStopMP3( 1 );

				pPlayer->m_strCurrentMp3 = m_sMusic;
			}
			else
			{
				if ( m_sMusic )
				{
					if ( m_iMusicFadeOut != -1 )
						pPlayer->PlayerStopMP3( m_iMusicFadeOut );
					else
						pPlayer->PlayerStopMP3( 0 );

					pPlayer->m_strCurrentMp3 = m_sMusic;
				}
			}
		}
		else
		{
			if ( m_sMusic )
				pPlayer->m_strCurrentMp3 = m_sMusic;
		}

		ShiftMonsters();

		SetNextThink( 3 );
		SetThink( &CInterDoorCoop::EndSequence );
		return;
	}

	pev->owner = NULL;
}

void CInterDoorCoop::EndGame( void )
{
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
		CLIENT_COMMAND( pPlayer->edict(), "map c_game_menu1\n" );
}

void CInterDoorCoop::EndSequence( void )
{
	CBaseEntity *pUser;

	if ( pWalkEnt )
	{
		UTIL_Remove( pWalkEnt );
		pWalkEnt = NULL;
	}

	if ( pev->owner )
		pUser = CBaseEntity::Instance( pev->owner );
	else
	{
		pev->owner = NULL;
		return;
	}

	if ( pUser )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pUser->pev );

		if ( !pPlayer )
		{
			ALERT( at_console, "No PLAYER!\n" );
			return;
		}

		// the demo chapters end at these doors
		if ( pev->target )
		{
			if ( FStrEq( STRING( pev->target ), "closegame1" ) )
			{
				UTIL_ProMessage( pPlayer, "End of apartment demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoorCoop::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame2" ) )
			{
				UTIL_ProMessage( pPlayer, "End of city demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoorCoop::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame3" ) )
			{
				UTIL_ProMessage( pPlayer, "End of subway demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoorCoop::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame4" ) )
			{
				UTIL_ProMessage( pPlayer, "End of forest demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoorCoop::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}
		}

		CBaseEntity *pTarget = NULL;
		pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ) );

		if ( !pTarget )
		{
			ALERT( at_console, "No TARGET!\n" );
			return;
		}

		Vector tmp = pTarget->pev->origin;

		if ( pPlayer->IsPlayer() )
			tmp.z -= pPlayer->pev->mins.z;

		tmp.z += 1;
		UTIL_SetOrigin( pPlayer, tmp );
		pPlayer->m_vPreviousOrigin = tmp;

		pPlayer->pev->angles = pTarget->pev->angles;
		pPlayer->pev->velocity = pPlayer->pev->basevelocity = g_vecZero;
		if ( pPlayer->IsPlayer() )
		{
			pPlayer->pev->v_angle = pTarget->pev->angles;
			pPlayer->pev->fixangle = TRUE;
		}

		pPlayer->pev->flags &= ~FL_ONGROUND;
		pPlayer->pev->fixangle = TRUE;

		pPlayer->b_GoingThroughADoor = false;

		if ( pev->noise1 )
			EMIT_SOUND( ENT( pPlayer->pev ), CHAN_AUTO, STRING( pev->noise1 ), 1, 0.8 );

		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0.5, 0, 255, FFADE_IN );

		pPlayer->EnableControl( TRUE );
		pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
		pPlayer->pev->flags &= ~FL_NOTARGET;

		if ( m_sProperTarget )
			FireTargets( STRING( m_sProperTarget ), this, this, USE_TOGGLE, 0 );

		if ( m_sMusic )
		{
			if ( pev->spawnflags & 2 )
			{
				if ( m_sMusic )
				{
					if ( m_iMusicFadeIn != -1 )
						pPlayer->PlayerSetMP3( m_sMusic, 1, m_iMusicFadeIn );
					else
						pPlayer->PlayerSetMP3( m_sMusic, 1, 0 );
				}
			}
			else
			{
				if ( m_sMusic )
				{
					if ( m_iMusicFadeIn != -1 )
						pPlayer->PlayerSetMP3( m_sMusic, 0, m_iMusicFadeIn );
					else
						pPlayer->PlayerSetMP3( m_sMusic, 0, 0 );
				}
			}
		}

		pev->owner = NULL;
		return;
	}

	ALERT( at_console, "No USER!\n" );

	pev->owner = NULL;
}

void CInterDoorCoop::ShiftMonsters( void )
{
	CBaseEntity *pEntity = NULL;

	// move the monsters that follow the player to their spawn position
	CBaseEntity *pTarget = NULL;
	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ) );

	if ( pTarget )
	{
		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pTarget->pev->origin, 1024 ) ) != NULL )
		{
			if ( pEntity->pev->classname && !FClassnameIs( pEntity->edict(), "monster_spitter" ) && strstr( STRING( pEntity->pev->classname ), "monster_" ) && !FClassnameIs( pEntity->edict(), "monster_generic" ) )
			{
				if ( pEntity->IsAlive() )
				{
					// original bug: asks the target for its monster instead of the entity found
					CBaseMonster *pMonster = pTarget->MyMonsterPointer();

					if ( pMonster && pMonster->m_vOriginalOrigin != g_vecZero )
						UTIL_SetOrigin( pMonster, pMonster->m_vOriginalOrigin );
				}
			}
		}
	}
}

void CInterDoorCoop::EnableDoor( bool disable )
{
	if ( disable )
		m_bDisabled = true;
	else
		m_bDisabled = false;
}

class CInterDoorDisableCoop : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( inter_door_disable_coop, CInterDoorDisableCoop );

void CInterDoorDisableCoop::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CInterDoorDisableCoop::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( pev->target )
	{
		CBaseEntity *pDoor = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

		if ( pDoor && FClassnameIs( pDoor->edict(), "inter_door_coop" ) )
		{
			CInterDoorCoop *pInterDoor = GetClassPtr( (CInterDoorCoop *)pDoor->pev );

			if ( pInterDoor )
			{
				// frags 0 disables the door, anything else enables it
				if ( pev->frags != 0 )
					pInterDoor->EnableDoor( false );
				else
					pInterDoor->EnableDoor( true );
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( inter_door_exit_coop, CPointEntity );

class CInterDoorCoopChangeLevel : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT ChangeLevelThink( void );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE) & ~FCAP_ACROSS_TRANSITION; }
	void BuildPlayerList( CBasePlayer *pPlayer );
	void KeyValue( KeyValueData *pkvd );

	string_t m_szMusic;
};

void CInterDoorCoopChangeLevel::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "music" ) )
	{
		m_szMusic = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( inter_door_coop_changelevel, CInterDoorCoopChangeLevel );

void CInterDoorCoopChangeLevel::Spawn( void )
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	pev->frame = 0;
}

void CInterDoorCoopChangeLevel::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CInterDoorCoopChangeLevel::BuildPlayerList( CBasePlayer *pPlayer )
{
	char filename[1024];

	if ( !pPlayer )
		return;

	// txtfiles/<player name>.cfg keeps the player state across the level change
	GET_GAME_DIR( filename );
	strcat( filename, "/txtfiles/" );
	strcat( filename, STRING( pPlayer->pev->netname ) );
	strcat( filename, ".cfg" );

	std::ofstream os( filename );

	if ( pPlayer->pev->health <= 0 )
		os << "health " << 50 << "\n";
	else
		os << "health " << pPlayer->pev->health << "\n";

	os << "stamina " << pPlayer->m_fStamina << "\n";

	if ( strlen( pPlayer->m_sQuickSel1 ) > 1 )
		os << "quicksel1 " << pPlayer->m_sQuickSel1 << "\n";

	if ( strlen( pPlayer->m_sQuickSel2 ) > 1 )
		os << "quicksel2 " << pPlayer->m_sQuickSel2 << "\n";

	if ( strlen( pPlayer->m_sQuickSel3 ) > 1 )
		os << "quicksel3 " << pPlayer->m_sQuickSel3 << "\n";

	os << "playerstats_shotsfired " << pPlayer->playerstats.shots_fired << "\n";
	os << "playerstats_shotshit " << pPlayer->playerstats.shots_hit << "\n";
	os << "playerstats_syringesused " << pPlayer->playerstats.syringes_used << "\n";
	os << "playerstats_monsterskilledmelee " << pPlayer->playerstats.monsters_killed_melee << "\n";
	os << "playerstats_monsterskilledshooting " << pPlayer->playerstats.monsters_killed_shooting << "\n";
	os << "playerstats_seconds " << pPlayer->playerstats.time_seconds << "\n";
	os << "playerstats_minutes " << pPlayer->playerstats.time_minutes << "\n";
	os << "playerstats_hours " << pPlayer->playerstats.time_hours << "\n";
	os << "playerstats_damagetaken " << pPlayer->playerstats.damagetaken << "\n";

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( pPlayer->m_rgpPlayerItems[i] && !FClassnameIs( pPlayer->m_rgpPlayerItems[i]->pev, "weapon_action" ) )
		{
			CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				os << STRING( pPlayerItem->pev->classname ) << "\n";

				os << ((CBasePlayerWeapon *)pPlayerItem)->m_iClip << "\n";

				os << pPlayer->m_rgAmmo[ ((CBasePlayerWeapon *)pPlayerItem)->m_iPrimaryAmmoType ] << "\n";

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	os << std::flush;
	os.close();
}

void CInterDoorCoopChangeLevel::ChangeLevelThink( void )
{
	CHANGE_LEVEL( (char *)STRING( pev->message ), NULL );
}

void CInterDoorCoopChangeLevel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	if ( !pev->target )
		return;

	if ( !pev->message )
		return;

	if ( m_szMusic )
		CVAR_SET_STRING( "sv_coop_music", STRING( m_szMusic ) );

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );
	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pTarget && pPlayer )
	{
		if ( m_iSimonMode )
		{
			UTIL_ProMessage( pPlayer, "What the... The door is jammed!", 2, false );
			return;
		}

		// the target is set to 1 when every player is ready
		if ( pTarget->pev->iuser1 != 1 )
		{
			UTIL_ProMessage( pPlayer, "Please wait until everyone is ready!", 2, false );
			return;
		}

		if ( pev->noise )
			EMIT_SOUND( ENT( pev ), CHAN_AUTO, STRING( pev->noise ), 1, 0.8 );

		if ( g_pLevelChange )
		{
			memset( g_pLevelChange->playervariablestruct, 0, sizeof( g_pLevelChange->playervariablestruct ) );
			memset( g_pLevelChange->playervariablestructstats, 0, sizeof( g_pLevelChange->playervariablestructstats ) );
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pEnt = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( pEnt )
			{
				pEnt->b_GoingThroughADoor = true;

				UTIL_ScreenFade( pEnt, Vector( 0, 0, 0 ), 0.5, 2, 255, FFADE_OUT | FFADE_STAYOUT );

				// freeze every player until the level changes
				pEnt->EnableControl( FALSE );
				pEnt->m_iHideHUD |= HIDEHUD_HEALTH;
				pEnt->m_iHideHUD |= HIDEHUD_WEAPONS;
				pEnt->pev->flags |= FL_NOTARGET;

				if ( m_szMusic )
					pEnt->PlayerSetMP3( m_szMusic, pev->iuser1, pev->fuser1 );

				if ( g_pLevelChange )
					g_pLevelChange->SetPlayerVariables( pEnt );
			}
		}

		SetThink( &CInterDoorCoopChangeLevel::ChangeLevelThink );
		SetNextThink( 2 );
	}
}
