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
// Watro - the hand that strikes out of the water
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"animation.h"

extern void UTIL_SplashCalculation( CBaseEntity *pEntity, int splashtype );

class CWatro : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SetYawSpeed( void );
	virtual int Classify( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT WatroThink( void );
	void EXPORT InactiveThink( void );
	void EXPORT ActivateWatro( void );
	int m_iActivated;
	int m_iActivating;
	int m_iAttacking;
	float m_flAttackTime;
	float m_flNextIdleTime;
};

LINK_ENTITY_TO_CLASS( monster_watro, CWatro );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CWatro :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CWatro :: SetYawSpeed ( void )
{
	pev->yaw_speed = 0;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CWatro :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CWatro :: Spawn()
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/watro.mdl" );
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_RED;
	pev->health			= 150;
	pev->view_ofs		= Vector ( 0, 0, 16 );
	m_flFieldOfView		= -0.7;
	m_MonsterState		= MONSTERSTATE_NONE;

	pev->renderfx = 101;

	SetSequenceByName( "under" );
	m_flNextIdleTime = gpGlobals->time + 1;
	pev->iuser1 = 0;
	m_iActivated = 0;
	m_iActivating = 0;
	SetNextThink( 0.1 );
	SetThink( &CWatro::InactiveThink );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CWatro :: Precache()
{
	PRECACHE_MODEL( "models/watro.mdl" );
	PRECACHE_SOUND( "sewmo/out_water.wav" );
	PRECACHE_SOUND( "watro/watro_swing.wav" );
	PRECACHE_SOUND( "watro/watro_hit.wav" );
}

void CWatro :: WatroThink( void )
{
	if ( pev->health <= 0 && !pev->iuser2 )
	{
		pev->iuser2 = 1;

		SetSequenceByName( "disappear" );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "sewmo/out_water.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
		UTIL_SplashCalculation( this, 0 );

		pev->fuser2 = gpGlobals->time + 1;
	}

	if ( pev->iuser2 )
	{
		if ( gpGlobals->time > pev->fuser2 )
			UTIL_Remove( this );

		SetNextThink( 0.1 );
		return;
	}

	if ( gpGlobals->time > m_flNextIdleTime && pev->iuser1 )
	{
		SetSequenceByName( "idle" );
		m_flNextIdleTime = gpGlobals->time + 4;
	}

	if ( pev->iuser1 )
	{
		if ( gpGlobals->time > pev->fuser1 && m_iAttacking == 2 )
			m_iAttacking = 0;

		if ( m_iAttacking == 1 && gpGlobals->time > m_flAttackTime )
		{
			m_iAttacking = 2;

			UTIL_MakeVectors( pev->angles );
			Vector origin = pev->origin + gpGlobals->v_forward * 100;

			if ( m_iActivated == 1 )
			{
				UTIL_MakeVectors( pev->angles );
				Vector origin = pev->origin + gpGlobals->v_forward * 100;

				CBaseEntity *pNear = NULL;

				while ( ( pNear = UTIL_FindEntityInSphere( pNear, origin, 128 ) ) != NULL )
				{
					if ( pNear->IsPlayer() && pNear->IsAlive() && ( pNear->pev->flags & FL_ONGROUND ) )
					{
						CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pNear->pev );

						if ( pPlayer )
						{
							EMIT_SOUND_DYN( edict(), CHAN_AUTO, "watro/watro_hit.wav", 1, ATTN_NORM, 0, 100 );
							pPlayer->ViewPunch( -10, 20, 20 );
							pNear->TakeDamage( VARS( 0 ), VARS( 0 ), 15, 0 );
						}
					}
				}
			}
		}
		else if ( m_iAttacking == 0 && gpGlobals->time > pev->fuser1 )
		{
			UTIL_MakeVectors( pev->angles );
			Vector origin = pev->origin + gpGlobals->v_forward * 100;

			CBaseEntity *pNear = NULL;

			while ( ( pNear = UTIL_FindEntityInSphere( pNear, origin, 128 ) ) != NULL )
			{
				if ( pNear->IsPlayer() && pNear->IsAlive() )
				{
					CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pNear->pev );

					if ( pPlayer )
					{
						SetSequenceByName( "attack" );

						m_iAttacking = 1;
						m_flAttackTime = gpGlobals->time + 0.45;
						m_flNextIdleTime = gpGlobals->time + 2;
						pev->fuser1 = gpGlobals->time + 1;
						EMIT_SOUND_DYN( edict(), CHAN_AUTO, "watro/watro_swing.wav", 1, ATTN_NORM, 0, 100 );
					}
				}
			}
		}
	}

	SetNextThink( 0.1 );
}

void CWatro :: ActivateWatro( void )
{
	SetThink( &CWatro::WatroThink );
	SetNextThink( 0.1 );

	pev->iuser1 = 1;

	pev->renderfx = 93;

	m_iActivated = 1;
	m_iAttacking = 0;

	pev->effects = 0;
	pev->takedamage = DAMAGE_AIM;
	pev->deadflag = DEAD_NO;
}

void CWatro :: InactiveThink( void )
{
	CBaseEntity *pNear = NULL;

	if ( !m_iActivated && !m_iActivating && !pev->iuser1 )
	{
		UTIL_MakeVectors( pev->angles );
		Vector origin = pev->origin + gpGlobals->v_forward * 100;

		while ( ( pNear = UTIL_FindEntityInSphere( pNear, origin, 256 ) ) != NULL )
		{
			if ( pNear && FClassnameIs( pNear->pev, "player" ) )
			{
				SetSequenceByName( "appear" );
				SetNextThink( 1 );
				SetThink( &CWatro::ActivateWatro );
				m_iActivating = 1;
				EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "sewmo/out_water.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
				UTIL_SplashCalculation( this, 0 );
				m_flNextIdleTime = gpGlobals->time + 1;
				return;
			}
		}
	}

	SetNextThink( 0.1 );
}
