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

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );


class CBranch : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void ItemPostFrame( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	int Swing( int );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	int m_iSwing;
	int bHit;
};

LINK_ENTITY_TO_CLASS( weapon_branch, CBranch );

// sequences of models/weapons/branch/v_branch.mdl
enum branch_e
{
	BRANCH_IDLE = 0,
	BRANCH_DRAW,
	BRANCH_HOLSTER,
	BRANCH_ATTACK1,
	BRANCH_ATTACK2,
	BRANCH_SPRINT_TO,
	BRANCH_SPRINT_IDLE,
	BRANCH_SPRINT_FROM,
	BRANCH_SHOVE,
	BRANCH_FIDGET1,
	BRANCH_FIDGET2,
	BRANCH_FIDGET3,
	BRANCH_JUMP_TO,
	BRANCH_JUMP_FROM,
	BRANCH_DRAW_FIRST,
	BRANCH_IDLE_ONEHAND,
	BRANCH_DRAW_ONEHAND,
	BRANCH_ATTACK1_ONEHAND,
	BRANCH_HOLSTER_ONEHAND,
};


void CBranch::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_branch" );

	Precache( );
	m_iId = 16;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/branch/w_branch.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_branch.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/branch/w_branch.mdl" );
}

void CBranch::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/branch/v_branch.mdl" );
	PRECACHE_MODEL( "models/weapons/branch/w_branch.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/branch/branch_hit.wav" );
	PRECACHE_SOUND( "weapons/branch/branch_swing.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	PRECACHE_SOUND( "weapons/branch/branch_first_get.wav" );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/branch/p_branch.mdl" );
}

int CBranch::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 16;
	p->iWeight = 1;
	return 1;
}

BOOL CBranch::Deploy( )
{
	if ( !m_pPlayer->m_bFirstDeploy[m_iId] && g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_VOICE, "weapons/branch/branch_first_get.wav", 1, 0.8, 0, 100 );
		UTIL_ProMessage( m_pPlayer, "Better than nothing I guess...", 0, false );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/branch/v_branch.mdl", "police/models/weapons/branch/p_branch.mdl", BRANCH_DRAW, "crowbar" );

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		return DefaultDeploy( "models/weapons/branch/v_branch.mdl", "models/null.mdl", BRANCH_DRAW_FIRST, "crowbar" );
	}

	return DefaultDeploy( "models/weapons/branch/v_branch.mdl", "models/null.mdl", BRANCH_DRAW, "crowbar" );
}

void CBranch::Holster( )
{
	SendWeaponAnim( BRANCH_HOLSTER );
	m_pPlayer->m_flNextAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBranch::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		SendWeaponAnim( BRANCH_ATTACK1 );
		m_pPlayer->SetNextThink( 0.37 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkBranch );
		m_flNextPrimaryAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
		break;

	case 1:
		SendWeaponAnim( BRANCH_ATTACK2 );
		m_pPlayer->SetNextThink( 0.37 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkBranch );
		m_flNextPrimaryAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
		break;
	}

	m_pPlayer->PlayerAnimation( "branch_attack", false );

	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/branch/branch_swing.wav", 1, 0.8, 0, 100 );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBranch::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );

	m_pPlayer->PlayerAnimation( "branch_melee", false );
	SendWeaponAnim( BRANCH_SHOVE );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CBranch::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		SendWeaponAnim( BRANCH_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( BRANCH_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( BRANCH_FIDGET3 );
		break;
	}
}

void CBranch::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	SendWeaponAnim( BRANCH_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBranch::ItemPostFrame( void )
{
	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( BRANCH_JUMP_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( BRANCH_JUMP_FROM );
			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;
				SendWeaponAnim( BRANCH_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( BRANCH_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( BRANCH_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
