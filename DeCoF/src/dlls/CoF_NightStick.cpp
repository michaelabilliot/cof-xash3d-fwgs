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


class CNightstick : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void ItemPostFrame( void );
	virtual void PrimaryAttack( void );
	int Swing( int );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
};

LINK_ENTITY_TO_CLASS( weapon_nightstick, CNightstick );

LINK_ENTITY_TO_CLASS( weapon_hammer, CNightstick );

// sequences of models/weapons/nightstick/v_nightstick.mdl
enum nightstick_e
{
	NIGHTSTICK_IDLE1 = 0,
	NIGHTSTICK_DRAW,
	NIGHTSTICK_HOLSTER,
	NIGHTSTICK_ATTACK1,
	NIGHTSTICK_ATTACK2,
	NIGHTSTICK_ATTACK3,
	NIGHTSTICK_FIDGET1,
	NIGHTSTICK_FIDGET2,
	// 8 is a second sequence named "idle1"
	NIGHTSTICK_FIDGET3 = 9,
	NIGHTSTICK_SPRINT_TO,
	NIGHTSTICK_SPRINT_IDLE,
	NIGHTSTICK_SPRINT_FROM,
	NIGHTSTICK_JUMP_TO,
	NIGHTSTICK_JUMP_FROM,
};


void CNightstick::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_nightstick" );

	Precache( );
	m_iId = 12;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/nightstick/w_nightstick.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_nightstick.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/nightstick/w_nightstick.mdl" );
}

void CNightstick::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/nightstick/v_nightstick.mdl" );
	PRECACHE_MODEL( "models/weapons/nightstick/w_nightstick.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/nightstick/nightstick_hitwall1.wav" );
	PRECACHE_SOUND( "weapons/nightstick/nightstick_hitwall2.wav" );
	PRECACHE_SOUND( "weapons/nightstick/nightstick_hitbody1.wav" );
	PRECACHE_SOUND( "weapons/nightstick/nightstick_hitbody2.wav" );
	PRECACHE_SOUND( "weapons/nightstick/nightstick_swing.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/nightstick/p_nightstick.mdl" );
}

int CNightstick::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 12;
	p->iWeight = 1;
	return 1;
}

BOOL CNightstick::Deploy( )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/nightstick/v_nightstick.mdl", "police/models/weapons/nightstick/p_nightstick.mdl", NIGHTSTICK_DRAW, "crowbar" );

	return DefaultDeploy( "models/weapons/nightstick/v_nightstick.mdl", "models/null.mdl", NIGHTSTICK_DRAW, "crowbar" );
}

void CNightstick::Holster( )
{
	SendWeaponAnim( NIGHTSTICK_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CNightstick::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	m_pPlayer->PlayerAnimation( "nightstick_attack", false );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		SendWeaponAnim( NIGHTSTICK_ATTACK1 );
		m_pPlayer->SetNextThink( 0.31 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick2 );
		m_flNextPrimaryAttack = gpGlobals->time + 1;
		break;

	case 1:
		SendWeaponAnim( NIGHTSTICK_ATTACK2 );
		m_pPlayer->SetNextThink( 0.31 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick2 );
		m_flNextPrimaryAttack = gpGlobals->time + 1;
		break;

	case 2:
		m_pPlayer->SetNextThink( 0.31 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick2 );
		m_flNextPrimaryAttack = gpGlobals->time + 1;
		SendWeaponAnim( NIGHTSTICK_ATTACK3 );
		break;
	}

	m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CNightstick::Fidget( void )
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
		SendWeaponAnim( NIGHTSTICK_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( NIGHTSTICK_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( NIGHTSTICK_FIDGET3 );
		break;
	}
}

void CNightstick::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	SendWeaponAnim( NIGHTSTICK_IDLE1 );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CNightstick::ItemPostFrame( void )
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
				SendWeaponAnim( NIGHTSTICK_JUMP_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( NIGHTSTICK_JUMP_FROM );
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
				SendWeaponAnim( NIGHTSTICK_SPRINT_TO );

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
				SendWeaponAnim( NIGHTSTICK_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( NIGHTSTICK_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
