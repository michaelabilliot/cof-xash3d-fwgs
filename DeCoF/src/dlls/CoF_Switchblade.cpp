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

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "animation.h"

LINK_ENTITY_TO_CLASS( weapon_knife, CSwitchblade );
LINK_ENTITY_TO_CLASS( weapon_switchblade, CSwitchblade );

// sequences of models/weapons/switchblade/v_switchblade.mdl
enum switchblade_e
{
	SWITCHBLADE_P1IDLE = 0,
	SWITCHBLADE_P1DRAW,
	SWITCHBLADE_P1HOLSTER,
	SWITCHBLADE_P1ATTACK1,
	SWITCHBLADE_P1TOP2,
	SWITCHBLADE_P2IDLE,
	SWITCHBLADE_P2DRAW,
	SWITCHBLADE_P2HOLSTER,
	SWITCHBLADE_P2ATTACK1,
	SWITCHBLADE_P2TOP1,
	SWITCHBLADE_P1_SPRINT_TO,
	SWITCHBLADE_P1_SPRINT_IDLE,
	SWITCHBLADE_P1_SPRINT_FROM,
	SWITCHBLADE_P2_SPRINT_TO,
	SWITCHBLADE_P2_SPRINT_IDLE,
	SWITCHBLADE_P2_SPRINT_FROM,
	SWITCHBLADE_P1ATTACK2,
	SWITCHBLADE_P1ATTACK3,
	SWITCHBLADE_P2ATTACK2,
	SWITCHBLADE_P2ATTACK3,
	SWITCHBLADE_P1FIDGET1,
	SWITCHBLADE_P1FIDGET2,
	SWITCHBLADE_P1FIDGET3,
	SWITCHBLADE_P2FIDGET1,
	SWITCHBLADE_P2FIDGET2,
	SWITCHBLADE_P2FIDGET3,
};


void CSwitchblade::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_switchblade" );

	Precache( );
	m_iId = 6;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/switchblade/w_switchblade.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	m_iSprintIdleAnim1 = SWITCHBLADE_P1_SPRINT_IDLE;
	m_iSprintIdleAnim2 = SWITCHBLADE_P2_SPRINT_IDLE;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_switchblade.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/switchblade/w_switchblade.mdl" );
}

void CSwitchblade::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/switchblade/v_switchblade.mdl" );
	PRECACHE_MODEL( "models/weapons/switchblade/w_switchblade.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_hitwall1.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_hitwall2.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_hitbody1.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_hitbody2.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_swing.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_draw.wav" );
	PRECACHE_SOUND( "weapons/switchblade/switchblade_draw2.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/switchblade/p_switchblade.mdl" );
}

int CSwitchblade::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = 6;
	p->iWeight = 1;
	return 1;
}

BOOL CSwitchblade::Deploy( )
{
	m_iSprintState = 0;

	if ( m_pPlayer->m_iSwitchbladeCuts < 2 )
	{
		if ( m_iBloodAmount != 0 )
			m_iBloodAmount = 0;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts >= 2 && m_pPlayer->m_iSwitchbladeCuts <= 5 )
	{
		if ( m_iBloodAmount != 1 )
			m_iBloodAmount = 1;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts > 5 && m_pPlayer->m_iSwitchbladeCuts <= 8 )
	{
		if ( m_iBloodAmount != 2 )
			m_iBloodAmount = 2;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts > 8 )
	{
		if ( m_iBloodAmount != 3 )
			m_iBloodAmount = 3;
	}

	if ( pev->body != m_iBloodAmount )
	{
		pev->body = m_iBloodAmount;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( b_KnifeStance )
		{
			if ( pev->max_health != 1 )
				EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_draw2.wav", 1, 0.8 );

			return DefaultDeploy( "models/weapons/switchblade/v_switchblade.mdl", "police/models/weapons/switchblade/p_switchblade.mdl", SWITCHBLADE_P2DRAW, "crowbar" );
		}
		else
		{
			if ( pev->max_health != 1 )
				EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_draw.wav", 1, 0.8 );

			return DefaultDeploy( "models/weapons/switchblade/v_switchblade.mdl", "police/models/weapons/switchblade/p_switchblade.mdl", SWITCHBLADE_P1DRAW, "crowbar" );
		}
	}
	else
	{
		if ( b_KnifeStance )
		{
			if ( pev->max_health != 1 )
				EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_draw2.wav", 1, 0.8 );

			return DefaultDeploy( "models/weapons/switchblade/v_switchblade.mdl", "models/null.mdl", SWITCHBLADE_P2DRAW, "crowbar" );
		}
		else
		{
			if ( pev->max_health != 1 )
				EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_draw.wav", 1, 0.8 );

			return DefaultDeploy( "models/weapons/switchblade/v_switchblade.mdl", "models/null.mdl", SWITCHBLADE_P1DRAW, "crowbar" );
		}
	}
}

void CSwitchblade::Holster( )
{
	m_pPlayer->m_iSwitchbladeCuts--;

	if ( m_pPlayer->m_iSwitchbladeCuts < 0 )
		m_pPlayer->m_iSwitchbladeCuts = 0;

	if ( b_KnifeStance )
	{
		SendWeaponAnim( SWITCHBLADE_P2HOLSTER );
	}
	else
	{
		SendWeaponAnim( SWITCHBLADE_P1HOLSTER );
	}

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void FindHullIntersectionSwitchblade( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

float CSwitchblade::GetNextRageDecreaseTime( void )
{
	return 0.75;
}

void CSwitchblade::RageReset( void )
{
	m_iRageNeeded = RANDOM_LONG( 4, 7 );
}

void CSwitchblade::RageAttack( void )
{
	// turn around to face the last enemy hit
	if ( m_pPlayer->m_pLastAttackedEnemy )
	{
		if ( m_pPlayer->m_pLastAttackedEnemy->IsAlive() )
		{
			int desangle = m_pPlayer->m_pLastAttackedEnemy->pev->angles.y + 180;

			if ( desangle > 360 )
				desangle -= 360;

			if ( desangle == 360 )
				desangle = 0;

			m_pPlayer->pev->angles.y = desangle;
			m_pPlayer->pev->fixangle = 1;
		}
	}

	if ( b_KnifeStance )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK3 );
			break;
		}

		m_pPlayer->SetNextThink( 0.16 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSwitchblade3 );
		m_flNextPrimaryAttack = gpGlobals->time + 0.9;
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK2 );
			break;
		}

		m_pPlayer->SetNextThink( 0.16 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSwitchblade3 );
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
	}
}

void CSwitchblade::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	if ( m_bIronSights )
		return;

	if ( b_KnifeStance )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P2FIDGET1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P2FIDGET2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P2FIDGET3 );
			break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P1FIDGET1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P1FIDGET2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P1FIDGET3 );
			break;
		}
	}
}

void CSwitchblade::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( !b_KnifeStance )
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_swing.wav", 1, 0.8 );

	if ( b_KnifeStance )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P2ATTACK2 );
			break;
		}

		m_pPlayer->SetNextThink( 0.16 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSwitchblade2 );
		m_flNextPrimaryAttack = gpGlobals->time + 0.9;

		m_pPlayer->PlayerAnimation( "switchblade_attack2", false );
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK1 );
			break;
		case 1:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK2 );
			break;
		case 2:
			SendWeaponAnim( SWITCHBLADE_P1ATTACK2 );
			break;
		}

		m_pPlayer->SetNextThink( 0.16 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSwitchblade );
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;

		m_pPlayer->PlayerAnimation( "switchblade_attack1", false );
	}

	if ( b_KnifeStance )
		m_flTimeWeaponIdle = m_flNextSecondaryAttack = gpGlobals->time + 1.2;
	else
		m_flTimeWeaponIdle = m_flNextSecondaryAttack = gpGlobals->time + 1;
}

void CSwitchblade::BloodFingerprint( void )
{
}

void CSwitchblade::SecondaryAttack( void )
{
	if ( b_KnifeStance )
	{
		b_KnifeStance = false;
		SendWeaponAnim( SWITCHBLADE_P2TOP1 );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1.5;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
	}
	else
	{
		b_KnifeStance = true;
		SendWeaponAnim( SWITCHBLADE_P1TOP2 );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1.5;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
	}
}

void CSwitchblade::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( b_KnifeStance )
	{
		SendWeaponAnim( SWITCHBLADE_P2IDLE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;
	}
	else
	{
		SendWeaponAnim( SWITCHBLADE_P1IDLE );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 5;
	}
}

void CSwitchblade::ItemPostFrame( void )
{
	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

	if ( m_pPlayer->m_iSwitchbladeCuts < 2 )
	{
		if ( m_iBloodAmount != 0 )
			m_iBloodAmount = 0;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts >= 2 && m_pPlayer->m_iSwitchbladeCuts <= 5 )
	{
		if ( m_iBloodAmount != 1 )
			m_iBloodAmount = 1;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts > 5 && m_pPlayer->m_iSwitchbladeCuts <= 8 )
	{
		if ( m_iBloodAmount != 2 )
			m_iBloodAmount = 2;
	}
	else if ( m_pPlayer->m_iSwitchbladeCuts > 8 )
	{
		if ( m_iBloodAmount != 3 )
			m_iBloodAmount = 3;
	}

	if ( pev->body != m_iBloodAmount )
	{
		pev->body = m_iBloodAmount;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				if ( b_KnifeStance )
				{
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P2_SPRINT_TO );
				}
				else
				{
					SendWeaponAnim( SWITCHBLADE_P1_SPRINT_TO );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_TO );
				}

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			if ( b_KnifeStance )
			{
				SendWeaponAnim( SWITCHBLADE_P2_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P2_SPRINT_FROM );
			}
			else
			{
				SendWeaponAnim( SWITCHBLADE_P1_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_FROM );
			}

			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;

				if ( b_KnifeStance )
				{
					SendWeaponAnim( SWITCHBLADE_P2_SPRINT_TO );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P2_SPRINT_TO );
				}
				else
				{
					SendWeaponAnim( SWITCHBLADE_P1_SPRINT_TO );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_TO );
				}

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				if ( b_KnifeStance )
				{
					SendWeaponAnim( SWITCHBLADE_P2_SPRINT_IDLE );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P2_SPRINT_IDLE );
				}
				else
				{
					SendWeaponAnim( SWITCHBLADE_P1_SPRINT_IDLE );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_IDLE );
				}

				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				if ( b_KnifeStance )
				{
					SendWeaponAnim( SWITCHBLADE_P2_SPRINT_FROM );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_IDLE );
				}
				else
				{
					SendWeaponAnim( SWITCHBLADE_P1_SPRINT_FROM );
					time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), SWITCHBLADE_P1_SPRINT_IDLE );
				}

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
