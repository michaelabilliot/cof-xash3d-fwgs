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

// sequences of models/weapons/flashlight/v_flashlight.mdl
enum flashlight_e
{
	FLASHLIGHT_IDLE = 0,
	FLASHLIGHT_DRAW,
	FLASHLIGHT_HOLSTER,
	FLASHLIGHT_SWITCH,
	FLASHLIGHT_SPRINT_TO,
	FLASHLIGHT_SPRINT_IDLE,
	FLASHLIGHT_SPRINT_FROM,
	FLASHLIGHT_FIDGET1,
	FLASHLIGHT_FIDGET2,
	FLASHLIGHT_FIDGET3,
	FLASHLIGHT_MELEE,
	FLASHLIGHT_MELEE_MISS,
	FLASHLIGHT_JUMP_TO,
	FLASHLIGHT_JUMP_FROM,
};

LINK_ENTITY_TO_CLASS( weapon_flashlight, CFlashlight );


void CFlashlight::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_flashlight" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/flashlight/w_flashlight.mdl" );

	m_iId = 1;

	m_iSprintIdleAnim1 = FLASHLIGHT_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_flashlight.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;
	strwmodel = MAKE_STRING( "models/weapons/flashlight/w_flashlight.mdl" );

	if ( pev->frags > 0 )
		m_iDefaultAmmo = pev->frags;
	else
		m_iDefaultAmmo = 100;

	FallInit();// get ready to fall down.
}

void CFlashlight::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/flashlight/v_flashlight.mdl" );
	PRECACHE_MODEL( "models/weapons/flashlight/w_flashlight.mdl" );
	PRECACHE_MODEL( "models/weapons/flashlight/p_flashlight.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );
}

int CFlashlight::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "flashlight";
	p->iMaxAmmo1 = 200;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 100;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 1;
	p->iWeight = 1;

	return 1;
}

void CFlashlight::TurnOn( void )
{
	if ( !m_pPlayer->m_iFlashlight )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/flashlight1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

		m_pPlayer->m_iFlashlight = !m_pPlayer->m_iFlashlight;

		if ( !( m_pPlayer->m_iFlashlightFlags & 8 ) )
			m_pPlayer->m_iFlashlightFlags |= 8;
	}
}

void CFlashlight::TurnOff( void )
{
	if ( m_pPlayer->m_iFlashlight )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/flashlight1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

		m_pPlayer->m_iFlashlight = !m_pPlayer->m_iFlashlight;

		if ( m_pPlayer->m_iFlashlightFlags & 8 )
			m_pPlayer->m_iFlashlightFlags &= ~8;
	}
}

BOOL CFlashlight::Deploy( )
{
	m_iSprintState = 0;
	m_iSuicideState = 0;

	SetThink( &CFlashlight::TurnOn );
	SetNextThink( 0.4 );

	return DefaultDeploy( "models/weapons/flashlight/v_flashlight.mdl", "models/weapons/flashlight/p_flashlight.mdl", FLASHLIGHT_DRAW, "mp5" );
}

void CFlashlight::Holster( )
{
	SendWeaponAnim( FLASHLIGHT_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;

	SetThink( &CFlashlight::TurnOff );
	SetNextThink( 0.25 );
}

void CFlashlight::ToggleSwitch( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/flashlight1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
	m_pPlayer->m_iFlashlight = !m_pPlayer->m_iFlashlight;
	SendWeaponAnim( FLASHLIGHT_SWITCH );

	if ( m_pPlayer->m_iFlashlight )
	{
		// turned on
		if ( !( m_pPlayer->m_iFlashlightFlags & 8 ) )
			m_pPlayer->m_iFlashlightFlags |= 8;
	}
	else
	{
		if ( m_pPlayer->m_iFlashlightFlags & 8 )
			m_pPlayer->m_iFlashlightFlags &= ~8;
	}

	m_flNextPrimaryAttack = gpGlobals->time + 0.5;
}

void CFlashlight::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	int j = min( 100 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );
}

void CFlashlight::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	SendWeaponAnim( FLASHLIGHT_IDLE );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CFlashlight::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	ToggleSwitch();
}

void CFlashlight::SecondaryAttack( void )
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
		SendWeaponAnim( FLASHLIGHT_MELEE );
	else
		SendWeaponAnim( FLASHLIGHT_MELEE_MISS );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CFlashlight::ItemPostFrame( void )
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
				SendWeaponAnim( FLASHLIGHT_JUMP_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( FLASHLIGHT_JUMP_FROM );

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
				SendWeaponAnim( FLASHLIGHT_SPRINT_TO );

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
				SendWeaponAnim( FLASHLIGHT_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( FLASHLIGHT_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CFlashlight::Fidget( void )
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
		SendWeaponAnim( FLASHLIGHT_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( FLASHLIGHT_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( FLASHLIGHT_FIDGET3 );
		break;
	}
}


class CFlashlightAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/weapons/flashlight/flashlight_battery.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/weapons/flashlight/flashlight_battery.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( pOther->GiveAmmo( 100, "flashlight", 200, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_flashlightbattery, CFlashlightAmmo );
