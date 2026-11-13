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

extern int gmsgParticle;
extern int gmsgKillWpnPart;


class CBookLaser : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void ItemPostFrame( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void SendInitMessage( CBasePlayer *player );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	bool m_bShooting;
};

LINK_ENTITY_TO_CLASS( weapon_booklaser, CBookLaser );

// sequences of models/weapons/booklaser/v_booklazer.mdl
enum booklaser_e
{
	BOOKLASER_IDLE = 0,
	BOOKLASER_SHOOT,
	BOOKLASER_DRAW,
	BOOKLASER_HOLSTER,
	BOOKLASER_SPRINT_TO,
	BOOKLASER_SPRINT_IDLE,
	BOOKLASER_SPRINT_FROM,
};

TYPEDESCRIPTION	CBookLaser::m_SaveData[] =
{
	DEFINE_FIELD( CBookLaser, m_bShooting, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CBookLaser, CBasePlayerWeapon );

void CBookLaser::SendInitMessage( CBasePlayer *player )
{
	if ( player && !m_bCheckedIfShouldBeDeleted )
	{
		m_bCheckedIfShouldBeDeleted = true;

		if ( !player->CheckingEndingOrUnlockables( 2097152, false, false ) )
			UTIL_Remove( this );
	}
}

void CBookLaser::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_booklaser" );

	Precache( );
	m_iId = 25;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/booklaser/w_booklaser.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_booklaser.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/booklaser/w_booklaser.mdl" );
}

void CBookLaser::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/booklaser/v_booklazer.mdl" );
	PRECACHE_MODEL( "models/weapons/booklaser/w_booklaser.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	PRECACHE_MODEL( "models/flash3.mdl" );

	PRECACHE_SOUND( "weapons/booklaser/flamethrower_burn.wav" );
	PRECACHE_SOUND( "weapons/booklaser/flamethrower_end.wav" );
	PRECACHE_SOUND( "weapons/booklaser/flamethrower_ignite.wav" );
}

int CBookLaser::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 25;
	p->iWeight = 1;
	return 1;
}

BOOL CBookLaser::Deploy( )
{
	if ( m_bShooting )
	{
		STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav" );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_end.wav", 1, 0.8, 0, 100 );
	}

	m_bShooting = false;
	pev->fuser2 = gpGlobals->time + 0.1;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	return DefaultDeploy( "models/weapons/booklaser/v_booklazer.mdl", "models/null.mdl", BOOKLASER_DRAW, "crowbar" );
}

void CBookLaser::Holster( )
{
	if ( m_bShooting )
	{
		STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav" );
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_end.wav", 1, 0.8, 0, 100 );
	}

	pev->fuser2 = gpGlobals->time + 0.1;
	m_bShooting = false;

	m_pPlayer->m_iFlashlightFlags &= ~16;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	SendWeaponAnim( BOOKLASER_HOLSTER );
	m_pPlayer->m_flNextAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBookLaser::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !m_bShooting )
	{
		UTIL_MakeVectors( m_pPlayer->pev->v_angle );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 8 + gpGlobals->v_forward * 8;

		UTIL_ScreenFade( m_pPlayer, Vector( 255, 0, 0 ), 0.2, 0, 64, 0 );
		UTIL_ScreenShake( m_pPlayer->GetGunPosition( ), 5.0, 0.5, 0.2, 25.0 );

		SendWeaponAnim( BOOKLASER_SHOOT );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.1;

		m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;

		pev->fuser1 = gpGlobals->time + 2;
		pev->fuser2 = gpGlobals->time + 0.1;

		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav", 1, 0.8, 0, 100 );
		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "weapons/booklaser/flamethrower_ignite.wav", 1, 0.8, 0, 100 );

		m_bShooting = true;

		m_pPlayer->m_iFlashlightFlags |= 16;

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/booklaser.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "models/weapons/booklaser/v_booklazer.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}
}

void CBookLaser::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;
}

void CBookLaser::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	SendWeaponAnim( BOOKLASER_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CBookLaser::ItemPostFrame( void )
{
	if ( m_bShooting && m_pPlayer )
	{
		if ( !( ( m_pPlayer->pev->button | m_pPlayer->m_afButtonPressed | m_pPlayer->m_afButtonReleased ) & IN_ATTACK ) )
		{
			m_bShooting = false;

			SendWeaponAnim( BOOKLASER_SHOOT );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.1;

			m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;

			m_pPlayer->m_iFlashlightFlags &= ~16;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav" );
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/booklaser/flamethrower_end.wav", 1, 0.8, 0, 100 );
		}
		else
		{
			if ( pev->fuser1 < gpGlobals->time )
			{
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav", 1, 0.8, 0, 100 );
				pev->fuser1 = gpGlobals->time + 2;
			}

			if ( pev->fuser2 < gpGlobals->time )
			{
				Vector vecSrc = m_pPlayer->GetGunPosition( );
				Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
				Vector vecDir;
				vecDir = m_pPlayer->FireBulletsPlayer( 6, vecSrc, vecAiming, VECTOR_CONE_5DEGREES, 400, 50, 0, 5, m_pPlayer->pev, m_pPlayer->random_seed );
				pev->fuser2 = gpGlobals->time + 0.1;
			}
		}
	}

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( BOOKLASER_SPRINT_TO );

				if ( m_bShooting )
				{
					pev->fuser2 = gpGlobals->time + 0.1;
					m_bShooting = false;

					STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav" );

					m_pPlayer->m_iFlashlightFlags &= ~16;

					MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( BOOKLASER_SPRINT_FROM );
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
				SendWeaponAnim( BOOKLASER_SPRINT_TO );

				if ( m_bShooting )
				{
					pev->fuser2 = gpGlobals->time + 0.1;
					m_bShooting = false;

					STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/booklaser/flamethrower_burn.wav" );

					m_pPlayer->m_iFlashlightFlags &= ~16;

					MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}

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
				SendWeaponAnim( BOOKLASER_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( BOOKLASER_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
