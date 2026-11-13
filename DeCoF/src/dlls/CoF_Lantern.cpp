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
#include "soundent.h"

// sequences of models/weapons/lantern/v_lantern.mdl
enum lantern_e
{
	LANTERN_DRAW = 0,
	LANTERN_SPRINT_IDLE,
	LANTERN_IDLE,
	LANTERN_PUNCH,
	LANTERN_HOLSTER,
	LANTERN_SPRINT_TO,
	LANTERN_SPRINT_FROM,
	LANTERN_PUNCH_MISS,
	LANTERN_FIDGET1,
	LANTERN_FIDGET2,
	LANTERN_FIDGET3,
	LANTERN_JUMP_TO,
	LANTERN_JUMP_FROM,
};

LINK_ENTITY_TO_CLASS( weapon_lantern, CLantern );


void CLantern::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_lantern" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/lantern/w_lantern.mdl" );

	m_iId = 10;

	pev->flags = 128;

	m_iSprintIdleAnim1 = 666;
	m_iSprintIdleAnim2 = 666;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_lantern.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 245;
	strwmodel = MAKE_STRING( "models/weapons/lantern/w_lantern.mdl" );

	FallInit();// get ready to fall down.
}

void CLantern::Fidget( void )
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
		SendWeaponAnim( LANTERN_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( LANTERN_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( LANTERN_FIDGET3 );
		break;
	}
}

void CLantern::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/lantern/v_lantern.mdl" );
	PRECACHE_MODEL( "models/weapons/lantern/w_lantern.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );
}

int CLantern::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 128;
	p->iId = m_iId = 10;
	p->iWeight = 5;

	return 1;
}

BOOL CLantern::Deploy( )
{
	m_iSprintState = 0;
	return DefaultDeploy( "models/weapons/lantern/v_lantern.mdl", "models/null.mdl", LANTERN_DRAW, "crowbar" );
}

void CLantern::Holster( )
{
	SendWeaponAnim( LANTERN_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
}

void CLantern::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );

	m_pPlayer->PlayerAnimation( "pistol_melee", false );

	bool bWillHit = false;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
			bWillHit = true;
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
			bWillHit = true;
	}

	if ( bWillHit )
		SendWeaponAnim( LANTERN_PUNCH );
	else
		SendWeaponAnim( LANTERN_PUNCH_MISS );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewPunch( 1, 1, 8 );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );
}

void CLantern::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	SendWeaponAnim( LANTERN_IDLE );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CLantern::ItemPostFrame( void )
{
	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				SendWeaponAnim( LANTERN_JUMP_TO );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( LANTERN_JUMP_FROM );
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

				SendWeaponAnim( LANTERN_SPRINT_TO );
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
				SendWeaponAnim( LANTERN_SPRINT_IDLE );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( LANTERN_SPRINT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
