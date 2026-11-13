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
// cof_interdoor.cpp - doors that fade the screen out and move the player
// to the other side
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "monsters.h"
#include "player.h"
#include "shake.h"
#include "cdll_dll.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// declared in cof_interdoor.h
class CInterDoor : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
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

TYPEDESCRIPTION CInterDoor::m_SaveData[] =
{
	DEFINE_FIELD( CInterDoor, m_sMusic, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_sLockedBy, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_sLockedMsg, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_sUnlockedMsg, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_sProperTarget, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_sUseUnlock, FIELD_STRING ),
	DEFINE_FIELD( CInterDoor, m_fNextUse, FIELD_TIME ),
	DEFINE_FIELD( CInterDoor, m_iMusicFadeOut, FIELD_FLOAT ),
	DEFINE_FIELD( CInterDoor, m_iMusicFadeIn, FIELD_FLOAT ),
	DEFINE_FIELD( CInterDoor, m_bSilentRoom, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInterDoor, m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CInterDoor, m_iNewLevelDoorSound, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CInterDoor, CBaseToggle );

LINK_ENTITY_TO_CLASS( inter_door, CInterDoor );

void CInterDoor::KeyValue( KeyValueData *pkvd )
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
		CBaseToggle::KeyValue( pkvd );
}

void CInterDoor::Spawn( void )
{
	if ( pev->spawnflags & 4 )
		m_bDisabled = true;

	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	m_fNextUse = gpGlobals->time;

	m_iMusicFadeOut = -1; m_iMusicFadeIn = -1;
}

void CInterDoor::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );

	if ( pev->noise1 )
		PRECACHE_SOUND( (char *)STRING( pev->noise1 ) );

	if ( pev->noise2 )
		PRECACHE_SOUND( (char *)STRING( pev->noise2 ) );

	if ( pev->noise3 )
		PRECACHE_SOUND( (char *)STRING( pev->noise3 ) );
}

int CInterDoor::IsLocked( void )
{
	if ( m_sLockedBy )
		return TRUE;

	return FALSE;
}

void CInterDoor::UnlockDoor( CBaseEntity *pActivator )
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

void CInterDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	if ( m_bDisabled )
		return;

	if ( m_fNextUse > gpGlobals->time )
		return;

	// this door only opens once an unlockable is earned
	if ( !strcmp( STRING( pev->targetname ), "unlockablesdoor" ) )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			if ( !pPlayer->CheckingEndingOrUnlockables( 1, false, false ) && !pPlayer->CheckingEndingOrUnlockables( 2, false, false ) && !pPlayer->CheckingEndingOrUnlockables( 4, false, false ) && !pPlayer->CheckingEndingOrUnlockables( 8, false, false ) && !pPlayer->CheckingEndingOrUnlockables( 16, false, false ) )
			{
				UTIL_ProMessage( pActivator, "I can't open it. Strange, there's no lock...", 5, false );
				m_fNextUse = gpGlobals->time + 1;
				return;
			}
		}
		else
		{
			UTIL_ProMessage( pActivator, "I can't open it. Strange, there's no lock...", 5, false );
			m_fNextUse = gpGlobals->time + 1;
			return;
		}
	}

	// unlock another door from this side
	if ( m_sUseUnlock )
	{
		CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING( m_sUseUnlock ) );

		if ( pEntity && FClassnameIs( pEntity->pev, "inter_door" ) )
		{
			CInterDoor *pDoor = GetClassPtr( (CInterDoor *)pEntity->pev );

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

void CInterDoor::StartSequence( void )
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

		if ( pev->noise )
			EMIT_SOUND( ENT( pPlayer->pev ), CHAN_AUTO, STRING( pev->noise ), 1, 0.8 );

		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0.5, 2, 255, FFADE_OUT | FFADE_STAYOUT );

		pPlayer->m_OnStairs = false;

		// freeze the player while the screen is black
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

		SetNextThink( 2 );
		SetThink( &CInterDoor::EndSequence );
		return;
	}

	pev->owner = NULL;
}

void CInterDoor::EndGame( void )
{
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
		CLIENT_COMMAND( pPlayer->edict(), "map c_game_menu1\n" );
}

void CInterDoor::EndSequence( void )
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
				SetThink( &CInterDoor::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame2" ) )
			{
				UTIL_ProMessage( pPlayer, "End of city demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoor::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame3" ) )
			{
				UTIL_ProMessage( pPlayer, "End of subway demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoor::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}

			if ( FStrEq( STRING( pev->target ), "closegame4" ) )
			{
				UTIL_ProMessage( pPlayer, "End of forest demo chapter!", 2, false );
				SetNextThink( 2 );
				SetThink( &CInterDoor::EndGame );
				pPlayer->PlayerStopMP3( 1 );
				return;
			}
		}

		CBaseEntity *pTarget = NULL;

		// the exit depends on the ending the player is heading for
		if ( !strcmp( STRING( pev->target ), "endingcheck" ) )
		{
			char m_szMapName[32];

			strcpy( m_szMapName, STRING( pev->target ) );

			if ( pPlayer )
			{
				if ( !pPlayer->m_b_Ending_RoofBossKilled && !pPlayer->m_b_Ending_GivenP345 )
					strcpy( m_szMapName, "ending_exit1" );

				if ( !pPlayer->m_b_Ending_RoofBossKilled && pPlayer->m_b_Ending_GivenP345 == 1 )
					strcpy( m_szMapName, "ending_exit2" );

				if ( pPlayer->m_b_Ending_RoofBossKilled == 1 && !pPlayer->m_b_Ending_GivenP345 )
					strcpy( m_szMapName, "ending_exit3" );

				if ( pPlayer->m_b_Ending_RoofBossKilled == 1 && pPlayer->m_b_Ending_GivenP345 == 1 )
					strcpy( m_szMapName, "ending_exit4" );

				if ( pPlayer->m_b_Ending_DeliveredPackage == 1 )
					strcpy( m_szMapName, "ending_exit5" );
			}

			pTarget = UTIL_FindEntityByTargetname( pTarget, m_szMapName );
		}
		else
		{
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING( pev->target ) );
		}

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
						pPlayer->PlayerSetMP3( m_sMusic, 1, 0 );
					else
						pPlayer->PlayerSetMP3( m_sMusic, 1, 0 );
				}
			}
			else
			{
				if ( m_sMusic )
				{
					if ( m_iMusicFadeIn != -1 )
						pPlayer->PlayerSetMP3( m_sMusic, 0, 0 );
					else
						pPlayer->PlayerSetMP3( m_sMusic, 0, 0 );
				}
			}
		}

		pPlayer->m_iWeaponVolume = 1000;

		pev->owner = NULL;
		return;
	}

	ALERT( at_console, "No USER!\n" );

	pev->owner = NULL;
}

void CInterDoor::ShiftMonsters( void )
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
					CBaseMonster *pMonster = pEntity->MyMonsterPointer();

					if ( pMonster && pMonster->m_vOriginalOrigin != g_vecZero )
						UTIL_SetOrigin( pMonster, pMonster->m_vOriginalOrigin );
				}
			}
		}
	}
}

void CInterDoor::EnableDoor( bool disable )
{
	if ( disable )
		m_bDisabled = true;
	else
		m_bDisabled = false;
}

class CInterDoorDisable : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( inter_door_disable, CInterDoorDisable );

void CInterDoorDisable::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CInterDoorDisable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( pev->target )
	{
		CBaseEntity *pDoor = UTIL_FindEntityByTargetname( NULL, STRING( pev->target ) );

		if ( pDoor && FClassnameIs( pDoor->edict(), "inter_door" ) )
		{
			CInterDoor *pInterDoor = GetClassPtr( (CInterDoor *)pDoor->pev );

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

LINK_ENTITY_TO_CLASS( inter_door_exit, CPointEntity );

class CCofInterDoorExitChange : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_inter_door_exitchange, CCofInterDoorExitChange );

void CCofInterDoorExitChange::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofInterDoorExitChange::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		// point the door named in message to a new exit
		if ( pev->target && pev->message )
		{
			CBaseEntity *pDoor = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

			if ( pDoor )
				pDoor->pev->target = pev->target;
		}
	}
}
