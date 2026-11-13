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
//=========================================================
// wheelchair - the wheelchair the player rides
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"
#include "animation.h"
#include "studio.h"
#include "shake.h"
#include "cdll_dll.h"

extern int gmsgWheelchair;
extern int gSpecialItemGive;

// The 1.55 debug build declares this class in cof_importantclassdefs.h.
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

LINK_ENTITY_TO_CLASS( wheelchair, CWheelchair );

TYPEDESCRIPTION CWheelchair::m_SaveData[] =
{
	DEFINE_FIELD( CWheelchair, wheelchairtime1, FIELD_TIME ),
	DEFINE_FIELD( CWheelchair, wheelchairtime2, FIELD_TIME ),
	DEFINE_FIELD( CWheelchair, wheelchairtime3, FIELD_TIME ),
	DEFINE_FIELD( CWheelchair, wheelchairtime4, FIELD_TIME ),
	DEFINE_FIELD( CWheelchair, m_bDead, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CWheelchair, CBaseMonster );

void CWheelchair::SetCollision( void )
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

void CWheelchair::ExitCheck( void )
{
	CBaseEntity *pExitEnt = NULL;

	if ( !m_pPlayer )
		return;

	if ( pExitEntity )
		return;

	while ( ( pExitEnt = UTIL_FindEntityInSphere( pExitEnt, pev->origin, 300 ) ) != NULL )
	{
		if ( FStrEq( STRING( pExitEnt->pev->classname ), "wheelchair_exit" ) )
		{
			pExitEntity = pExitEnt;
			UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 5, 255, FFADE_OUT | FFADE_STAYOUT );
			SetThink( &CWheelchair::Disembark );
			SetNextThink( 1 );
		}
	}
}

void CWheelchair::Disembark( void )
{
	if ( !m_pPlayer )
		return;

	if ( m_bDead )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgWheelchair, NULL, m_pPlayer->pev );
		WRITE_SHORT( -1 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	SetSequenceByName( "idle" );
	pev->body = 1;
	m_pPlayer->EnableControl( TRUE );

	m_pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;

	m_pPlayer->pev->origin = pev->origin;
	m_pPlayer->pev->origin.z += 36;
	m_pPlayer->pev->angles = pev->angles;
	m_pPlayer->pev->fixangle = 1;

	m_pPlayer->m_pWheelchair = NULL;
	m_pPlayer = NULL;
	pExitEntity = NULL;

	SetUse( &CWheelchair :: WheelchairUse );
	SetThink( &CWheelchair :: WheelchairThink );
	SetNextThink( 0 );

	pev->iuser1 = 0;
}

void CWheelchair::MoveForward( void )
{
	if ( pev->iuser1 == 2 )
		return;

	if ( m_bDead )
		return;

	if ( m_pPlayer && m_pPlayer->m_pActiveItem && GetClassPtr( (CBasePlayerWeapon *)m_pPlayer->m_pActiveItem->pev )->m_fInReload )
		return;

	float length = pev->velocity.Length();

	if ( length > pev->fuser1 )
		pev->velocity = pev->velocity.Normalize() * pev->fuser1;

	if ( pev->iuser1 != 1 )
		SetSequenceByName( "forward" );

	wheelchairtime3 = gpGlobals->time + 0.5;
	pev->iuser1 = 1;
}

void CWheelchair::MoveBack( void )
{
	if ( pev->iuser1 == 1 )
		return;

	if ( m_bDead )
		return;

	if ( m_pPlayer && m_pPlayer->m_pActiveItem && GetClassPtr( (CBasePlayerWeapon *)m_pPlayer->m_pActiveItem->pev )->m_fInReload )
		return;

	float length = pev->velocity.Length();

	if ( length > pev->fuser1 )
		pev->velocity = pev->velocity.Normalize() * pev->fuser1;

	if ( pev->iuser1 != 2 )
		SetSequenceByName( "back" );

	wheelchairtime3 = gpGlobals->time + 0.2;
	pev->iuser1 = 2;
}

void CWheelchair::TurnLeft( void )
{
	if ( m_bDead )
		return;

	if ( m_pPlayer && m_pPlayer->m_pActiveItem && GetClassPtr( (CBasePlayerWeapon *)m_pPlayer->m_pActiveItem->pev )->m_fInReload )
		return;

	UTIL_MakeVectors( pev->angles );

	pev->avelocity.y += pev->fuser4;

	float length = pev->avelocity.Length();

	if ( length > pev->fuser2 )
		pev->avelocity = pev->avelocity.Normalize() * pev->fuser2;

	wheelchairtime4 = gpGlobals->time + 0.2;

	if ( pev->iuser2 != 1 && pev->iuser2 != 5 )
		pev->iuser2 = 1;
}

void CWheelchair::TurnRight( void )
{
	if ( m_bDead )
		return;

	if ( m_pPlayer && m_pPlayer->m_pActiveItem && GetClassPtr( (CBasePlayerWeapon *)m_pPlayer->m_pActiveItem->pev )->m_fInReload )
		return;

	UTIL_MakeVectors( pev->angles );

	pev->avelocity.y -= pev->fuser4;

	float length = pev->avelocity.Length();

	if ( length > pev->fuser2 )
		pev->avelocity = pev->avelocity.Normalize() * pev->fuser2;

	wheelchairtime4 = gpGlobals->time + 0.2;

	if ( pev->iuser2 != 2 && pev->iuser2 != 5 )
		pev->iuser2 = 2;
}

void CWheelchair::SetSequenceByName( char *szSequence )
{
	int	iSequence;
	iSequence = LookupSequence ( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

void CWheelchair::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	if ( m_bDead )
		return;

	switch ( pEvent->event )
	{
	case 1666:
		UTIL_MakeVectors( pev->angles );
		pev->velocity = pev->velocity + gpGlobals->v_forward * 80.5;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move1.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move2.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 2:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move3.wav", 1, ATTN_NORM, 0, 100 );
			break;
		}
		break;

	case 1667:
		UTIL_MakeVectors( pev->angles );
		pev->velocity = pev->velocity - gpGlobals->v_forward * 50.5;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move1.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move2.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 2:
			EMIT_SOUND_DYN( edict(), CHAN_BODY, "player/wheelchair/wheelchair_move3.wav", 1, ATTN_NORM, 0, 100 );
			break;
		}
		break;

	default:
		CBaseAnimating::HandleAnimEvent( pEvent );
		break;
	}

	CBaseAnimating::HandleAnimEvent( pEvent );
}

int CWheelchair::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( m_bDead )
		return 0;

	bool killed = false;

	if ( flDamage >= pev->health )
	{
		flDamage = 0;
		killed = true;
	}

	int returnvalue = 0;

	if ( flDamage != 0 )
		returnvalue = CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	if ( m_pPlayer )
	{
		if ( !killed )
			m_pPlayer->pev->health = pev->health;
		else
			m_pPlayer->pev->health = 1;
	}

	if ( killed && m_pPlayer )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( m_pPlayer->edict(), "chr", "2" );

		m_pPlayer->DeathSound();

		m_pPlayer->SetNextThink( 0 );
		m_pPlayer->SetThink( &CBasePlayer::WheelchairPlayerDeathThink );

		m_pPlayer->m_flDeadTime = gpGlobals->time + 2;
		m_pPlayer->m_bIssuedGameOver = false;

		pev->iuser2 = 1;

		m_bDead = true;
		flDamage = pev->health - 1;
		m_iClass = CLASS_ALIEN_MONSTER;

		m_pPlayer->ClearOutSlot( 1, 1 );
		m_pPlayer->RemoveAllItems( FALSE );

		m_pPlayer->SetDualWieldWeapon( 0 );

		m_pPlayer->m_iDualWeaponLeftAmmo = -2;
		m_pPlayer->m_iDualWeaponRightAmmo = -2;

		MESSAGE_BEGIN( MSG_ONE, gmsgWheelchair, NULL, m_pPlayer->pev );
			WRITE_SHORT( ENTINDEX( edict() ) );
			WRITE_BYTE( 1 );
		MESSAGE_END();
	}
	else if ( m_pPlayer && flDamage != 0 )
	{
		m_pPlayer->Pain();
	}

	return returnvalue;
}

void CWheelchair::Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int cCount;
	BOOL fDone;
	CBaseEntity *pOwner;
}

void CWheelchair::WheelchairThink( void )
{
	if ( m_pPlayer )
		pev->health = m_pPlayer->pev->health;

	DROP_TO_FLOOR( ENT( pev ) );

	if ( wheelchairtime2 < gpGlobals->time )
	{
		UTIL_MakeVectors( pev->angles );
		pev->velocity = pev->velocity * pev->fuser3;
		pev->avelocity = pev->avelocity * pev->fuser3;

		float length = pev->velocity.Length();

		if ( length > pev->fuser1 )
			pev->velocity = pev->velocity.Normalize() * pev->fuser1;

		float length2 = pev->avelocity.Length();

		if ( length2 > pev->fuser2 )
			pev->avelocity = pev->avelocity.Normalize() * pev->fuser2;

		wheelchairtime2 = gpGlobals->time + 0.1;
	}

	if ( wheelchairtime3 < gpGlobals->time && pev->sequence != LookupSequence( "idle" ) )
	{
		SetSequenceByName( "idle" );
		pev->iuser1 = 0;
		pev->frags = 0;
	}
}

int CWheelchair::Classify( void )
{
	return m_iClass?m_iClass:CLASS_PLAYER_ALLY;
}

void CWheelchair::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/player_sick.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_FLY;

	pev->solid = SOLID_SLIDEBOX;

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 72 ) );

	// put the wheelchair on the floor
	TraceResult tr;
	Vector vecSrc = pev->origin;
	Vector vecEnd = vecSrc - gpGlobals->v_up * 1024;
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );
	pev->origin = tr.vecEndPos;

	pev->friction = 1.5;
	pev->gravity = 0.8;

	SetSequenceByName( "idle" );

	SetUse( &CWheelchair :: WheelchairUse );

	m_MonsterState = MONSTERSTATE_NONE;
	m_afCapability = bits_CAP_DOORS_GROUP;

	pev->health = 100;

	MonsterInit();

	m_bloodColor = BLOOD_COLOR_RED;
}

void CWheelchair::Precache( void )
{
	PRECACHE_MODEL( "models/player_sick.mdl" );
	PRECACHE_SOUND( "player/wheelchair/wheelchair_move1.wav" );
	PRECACHE_SOUND( "player/wheelchair/wheelchair_move2.wav" );
	PRECACHE_SOUND( "player/wheelchair/wheelchair_move3.wav" );
}

void CWheelchair::Embark( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->ClearOutSlot( 1, 1 );
	m_pPlayer->RemoveAllItems( FALSE );

	m_pPlayer->SetDualWieldWeapon( 0 );

	m_pPlayer->m_iDualWeaponLeftAmmo = -2;
	m_pPlayer->m_iDualWeaponRightAmmo = -2;

	pev->fuser1 = CVAR_GET_FLOAT( "cof_wheelchair_maxspeed" );
	pev->fuser2 = CVAR_GET_FLOAT( "cof_wheelchair_maxturnspeed" );
	pev->fuser3 = CVAR_GET_FLOAT( "cof_wheelchair_speedreduction" );
	pev->fuser4 = CVAR_GET_FLOAT( "cof_wheelchair_turnspeed" );

	SetUse( &CWheelchair :: WheelchairUse );

	// the player waits out of sight while riding
	CBaseEntity *pHide = UTIL_FindEntityByClassname( NULL, "wheelchair_hide_spot" );

	MESSAGE_BEGIN( MSG_ONE, gmsgWheelchair, NULL, m_pPlayer->pev );
		WRITE_SHORT( ENTINDEX( edict() ) );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	if ( pHide )
		UTIL_SetOrigin( m_pPlayer, pHide->pev->origin );

	m_pPlayer->m_pWheelchair = this;
	pev->health = m_pPlayer->pev->health;

	g_engfuncs.pfnSetPhysicsKeyValue( m_pPlayer->edict(), "chr", "1" );

	UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 0, 255, 0 );

	gSpecialItemGive = 1;
	m_pPlayer->m_bSilentGive = true;
	m_pPlayer->GiveNamedItem( "weapon_browning_wheelchair", false, false );
	m_pPlayer->GiveNamedItem( "weapon_browning_wheelchair", false, false );
	m_pPlayer->GiveNamedItem( "weapon_browning_wheelchair", false, false );
	m_pPlayer->GiveNamedItem( "weapon_browning_wheelchair", false, false );
	gSpecialItemGive = 0;

	SetThink( &CBaseMonster :: CallMonsterThink );
	AbsoluteNextThink( m_fNextThink + 0.1 );
}

void CWheelchair::WheelchairUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( !m_pPlayer )
		{
			pExitEntity = NULL;
			pExitTeller = NULL;

			m_pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );
			m_pPlayer->EnableControl( FALSE );

			Embark();
		}
	}
}

class CWheelchairHideSpot : public CPointEntity
{
	void Spawn( void );
};

void CWheelchairHideSpot::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( wheelchair_hide_spot, CWheelchairHideSpot );

class CWheelchairExitSpot : public CPointEntity
{
	void Spawn( void );
};

void CWheelchairExitSpot::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( wheelchair_exit, CWheelchairExitSpot );
