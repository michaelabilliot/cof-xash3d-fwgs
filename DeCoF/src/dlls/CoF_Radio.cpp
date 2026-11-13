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
// cof_radio.cpp - police radio, held like a weapon
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"
#include "animation.h"

class CRadioWeapon : public CBasePlayerWeapon
{
public:
	void Spawn( void );
	void Precache( void );
	int GetItemInfo( ItemInfo *p );
	void SecondaryAttack( void );
	BOOL Deploy( void );
	void Holster( void );
	void WeaponIdle( void );
	BOOL ShouldWeaponIdle( void ) { return TRUE; }
};

LINK_ENTITY_TO_CLASS( weapon_radio, CRadioWeapon );

// sequences of models/weapons/radio/v_radio.mdl
enum radio_e
{
	RADIO_IDLE = 0,
	RADIO_DRAW,
	RADIO_HOLSTER,
	RADIO_PUNCH,
};

void CRadioWeapon::Spawn( void )
{
	pev->classname = MAKE_STRING( "weapon_radio" );

	Precache( );
	m_iId = 22;
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	m_iClip = -1;
	FallInit();// get ready to fall down.

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_radio.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/null.mdl" );
}

void CRadioWeapon::Precache( void )
{
	PRECACHE_MODEL( "police/models/weapons/radio/v_radio.mdl" );
	PRECACHE_MODEL( "police/models/weapons/radio/p_radio.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );
}

int CRadioWeapon::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 22;
	p->iWeight = 1;
	return 1;
}

BOOL CRadioWeapon::Deploy( void )
{
	return DefaultDeploy( "police/models/weapons/radio/v_radio.mdl", "police/models/weapons/radio/p_radio.mdl", RADIO_DRAW, "crowbar" );
}

void CRadioWeapon::Holster( void )
{
	SendWeaponAnim( RADIO_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CRadioWeapon::SecondaryAttack( void )
{
	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	m_pPlayer->PlayerAnimation( "radio_melee", false );

	m_pPlayer->ViewPunch( 1, 1, 3 );
	SendWeaponAnim( RADIO_PUNCH );
	m_pPlayer->SetNextThink( 0.133 );
	m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkPunch );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );
}

void CRadioWeapon::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	SendWeaponAnim( RADIO_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}
