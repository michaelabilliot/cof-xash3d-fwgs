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

extern int g_iDoctorMode;
extern int gmsgWpnDeploy;

LINK_ENTITY_TO_CLASS( weapon_action, CAction );

// sequences of models/weapons/v_actions.mdl
enum action_e
{
	ACTION_EMPTYIDLE = 0,
	ACTION_SWIM,
	ACTION_SPRINTEMPTY_IDLE,
	ACTION_FLYBACKTO,
	ACTION_FLYBACKLOOP,
	ACTION_FLYBACKLOOPEND,
	ACTION_PUNCH1,
	ACTION_PUNCH2,
	ACTION_PUNCH1_MISS,
	ACTION_PUNCH2_MISS,
};

// the empty hands: pev->frags selects the action the player is doing

void CAction::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_action" );
	Precache( );
	m_iId = 17;
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	m_iClip = -1;
	FallInit();// get ready to fall down.

	pev->max_health = 1;
}

void CAction::Precache( void )
{
	if ( g_iDoctorMode )
		PRECACHE_MODEL( "models/weapons/v_actions_doctor.mdl" );
	else
		PRECACHE_MODEL( "models/weapons/v_actions.mdl" );

	PRECACHE_MODEL( "models/null.mdl" );
}

int CAction::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 0;
	p->iId = 17;
	p->iWeight = 1;
	p->iFlags = 64;

	return 1;
}

BOOL CAction::Deploy( )
{
	if ( pev->frags == 3 )
	{
		if ( g_iDoctorMode )
			return DefaultDeploy( "models/weapons/v_actions_doctor.mdl", "models/null.mdl", ACTION_FLYBACKTO, "crowbar" );
		else
			return DefaultDeploy( "models/weapons/v_actions.mdl", "models/null.mdl", ACTION_FLYBACKTO, "crowbar" );
	}

	if ( g_iDoctorMode )
		m_pPlayer->pev->viewmodel = MAKE_STRING( "models/weapons/v_actions_doctor.mdl" );
	else
		m_pPlayer->pev->viewmodel = MAKE_STRING( "models/weapons/v_actions.mdl" );

	m_pPlayer->pev->weaponmodel = MAKE_STRING( "models/null.mdl" );

	if ( g_iDoctorMode )
		SET_MODEL( ENT( pev ), "models/weapons/v_actions_doctor.mdl" );
	else
		SET_MODEL( ENT( pev ), "models/weapons/v_actions.mdl" );

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgWpnDeploy, NULL, m_pPlayer->pev );
	MESSAGE_END();

	SendWeaponAnim( ACTION_EMPTYIDLE, 0, pev->body );

	return TRUE;
}

void CAction::Holster( )
{
}

void CAction::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	switch ( (int)pev->frags )
	{
	case 1:
		// swimming
		if ( PlayerSpeed > 10 && !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		{
			SendWeaponAnim( ACTION_SWIM );

			if ( m_pPlayer->pev->flags & (1<<18) )
				m_flTimeWeaponIdle = gpGlobals->time + 0.8;
			else
				m_flTimeWeaponIdle = gpGlobals->time + 1.6;
		}
		else
			m_flTimeWeaponIdle = gpGlobals->time + 0.1;
		break;

	case 2:
		// sprinting
		if ( PlayerSpeed > 10 && ( m_pPlayer->pev->flags & (1<<18) ) )
		{
			SendWeaponAnim( ACTION_SPRINTEMPTY_IDLE );
			m_flTimeWeaponIdle = gpGlobals->time + 0.666;
		}
		else
			m_flTimeWeaponIdle = gpGlobals->time + 0.1;
		break;

	case 3:
		// falling
		if ( ( m_pPlayer->m_bWaitingToFall || ( m_pPlayer->pev->health > 0 && m_pPlayer->m_flFallVelocity >= 350 ) ) && !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		{
			SendWeaponAnim( ACTION_FLYBACKLOOP );
			m_flTimeWeaponIdle = gpGlobals->time + 0.666;
		}
		else
			m_flTimeWeaponIdle = gpGlobals->time + 0.1;
		break;

	case 10:
		SendWeaponAnim( ACTION_PUNCH1 );
		m_flTimeWeaponIdle = gpGlobals->time;
		pev->frags = 20;
		m_flNextPrimaryAttack = gpGlobals->time + 0.66;
		break;

	case 11:
		SendWeaponAnim( ACTION_PUNCH1_MISS );
		m_flTimeWeaponIdle = gpGlobals->time;
		m_flNextPrimaryAttack = gpGlobals->time + 0.66;
		pev->frags = 20;
		break;

	case 12:
		SendWeaponAnim( ACTION_PUNCH2 );
		m_flTimeWeaponIdle = gpGlobals->time;
		pev->frags = 20;
		m_flNextPrimaryAttack = gpGlobals->time + 0.66;
		break;

	case 13:
		SendWeaponAnim( ACTION_PUNCH2_MISS );
		m_flTimeWeaponIdle = gpGlobals->time;
		pev->frags = 20;
		m_flNextPrimaryAttack = gpGlobals->time + 0.66;
		break;
	}
}

void CAction::ItemPostFrame( void )
{
	// the punch is over
	if ( gpGlobals->time > m_flNextPrimaryAttack && pev->frags == 20 )
		pev->frags = 0;

	CBasePlayerWeapon::ItemPostFrame();
}
