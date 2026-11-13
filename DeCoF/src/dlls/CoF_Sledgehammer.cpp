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


class CSledgeHammer : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void ItemPostFrame( void );
	virtual void PrimaryAttack( void );
	int Swing( int );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
};

LINK_ENTITY_TO_CLASS( weapon_sledgehammer, CSledgeHammer );

// sequences of models/weapons/sledgehammer/v_sledgehammer.mdl
enum sledgehammer_e
{
	SLEDGEHAMMER_DRAW = 0,
	SLEDGEHAMMER_ATTACK,
	SLEDGEHAMMER_IDLE,
	SLEDGEHAMMER_HOLSTER,
	SLEDGEHAMMER_SPRINT_TO,
	SLEDGEHAMMER_SPRINT_IDLE,
	SLEDGEHAMMER_SPRINT_FROM,
};


void CSledgeHammer::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_sledgehammer" );

	Precache( );
	m_iId = 18;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/sledgehammer/w_sledgehammer.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_sledgehammer.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	m_iSprintIdleAnim1 = SLEDGEHAMMER_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	strwmodel = MAKE_STRING( "models/weapons/sledgehammer/w_sledgehammer.mdl" );
}

void CSledgeHammer::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/sledgehammer/v_sledgehammer.mdl" );
	PRECACHE_MODEL( "models/weapons/sledgehammer/w_sledgehammer.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/sledgehammer/p_sledgehammer.mdl" );

	PRECACHE_SOUND( "weapons/sledgehammer/sledgehammer_grab.wav" );
	PRECACHE_SOUND( "weapons/sledgehammer/sledgehammer_swing.wav" );
	PRECACHE_SOUND( "weapons/sledgehammer/sledgehammer_hit.wav" );
	PRECACHE_SOUND( "weapons/sledgehammer/sledgehammer_hitbody.wav" );
}

int CSledgeHammer::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 18;
	p->iWeight = 1;
	return 1;
}

BOOL CSledgeHammer::Deploy( )
{
	m_iSprintState = 0;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/sledgehammer/v_sledgehammer.mdl", "police/models/weapons/sledgehammer/p_sledgehammer.mdl", SLEDGEHAMMER_DRAW, "crowbar" );

	return DefaultDeploy( "models/weapons/sledgehammer/v_sledgehammer.mdl", "models/null.mdl", SLEDGEHAMMER_DRAW, "crowbar" );
}

void CSledgeHammer::Holster( )
{
	SendWeaponAnim( SLEDGEHAMMER_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
}

void CSledgeHammer::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	// not enough stamina to swing
	if ( m_pPlayer->m_fStamina < 8 )
		return;

	if ( m_iSprintState )
		return;

	m_pPlayer->PlayerAnimation( "sledgehammer_attack", false );

	SendWeaponAnim( SLEDGEHAMMER_ATTACK );
	m_pPlayer->SetNextThink( 1.0 );
	m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSledgeHammer );
	m_flNextPrimaryAttack = gpGlobals->time + 2.66;

	m_flTimeWeaponIdle = gpGlobals->time + 2.66;
}

void CSledgeHammer::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	SendWeaponAnim( SLEDGEHAMMER_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CSledgeHammer::ItemPostFrame( void )
{
	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( SLEDGEHAMMER_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( SLEDGEHAMMER_SPRINT_FROM );
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
				SendWeaponAnim( SLEDGEHAMMER_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( SLEDGEHAMMER_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( SLEDGEHAMMER_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
