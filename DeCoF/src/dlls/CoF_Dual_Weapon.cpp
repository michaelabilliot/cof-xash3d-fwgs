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

#include "CoF_Dual_Weapon.h"

LINK_ENTITY_TO_CLASS( dualwield, CDualWeaponRight );

TYPEDESCRIPTION	CDualWeaponRight::m_SaveData[] =
{
	DEFINE_FIELD( CDualWeaponRight, m_flNextIdleTime, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_flPrimaryAttackDelay, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_flSecondaryAttackDelay, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_sModel, FIELD_STRING ),
	DEFINE_FIELD( CDualWeaponRight, m_iIdleAnimation, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_bShotLooping, FIELD_BOOLEAN ),
	DEFINE_FIELD( CDualWeaponRight, shoot_loop_sound, FIELD_SOUNDNAME ),
	DEFINE_FIELD( CDualWeaponRight, shoot_loop_sound_end, FIELD_SOUNDNAME ),
	DEFINE_FIELD( CDualWeaponRight, m_iSprintState, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, dualbody, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_iThinkState, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_flThinkDecay, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_iWeaponMode, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_iIdleAnimation2, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_fIdleAnimation1Time, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_fIdleAnimation2Time, FIELD_TIME ),
	DEFINE_FIELD( CDualWeaponRight, m_iID, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeaponRight, m_fEmptyClickTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CDualWeaponRight, CBaseEntity );

void CDualWeaponRight::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;

	pev->movetype = MOVETYPE_NONE;
}

void CDualWeaponRight::Precache( void )
{
}

void CDualWeaponRight::SetModel( CBasePlayer *pPlayer, int model )
{
	if ( !pPlayer )
		return;

	m_sModel = model;

	pPlayer->SetDualWieldWeapon( model );
}

void CDualWeaponRight::SetDualBodyGroup( CBasePlayer *pPlayer, int group1, int group2 )
{
}

void CDualWeaponRight::SendWeaponAnim( CBasePlayer *pPlayer, int anim )
{
	if ( !pPlayer )
		return;

	pPlayer->m_iCurrentWeaponAnimation = anim;
	pPlayer->DualWieldAnimation( anim );
}

bool CDualWeaponRight::HasDualWeapon( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return false;

	if ( !pPlayer->m_sDualWieldViewModel )
		return false;

	return true;
}

void CDualWeaponRight::MeleeHitThinkPunch( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkPunch();
}

void CDualWeaponRight::MeleeHitThinkSwitchblade( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkSwitchblade();
}

void CDualWeaponRight::MeleeHitThinkSwitchblade2( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkSwitchblade2();
}

void CDualWeaponRight::MeleeHitThinkSwitchblade3( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkSwitchblade3();
}

void CDualWeaponRight::MeleeHitThinkBranch( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkBranch();
}

void CDualWeaponRight::MeleeHitThinkNightStick( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkNightStick();
}

void CDualWeaponRight::MeleeHitThinkNightStick2( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkNightStick2();
}

void CDualWeaponRight::MeleeHitThinkNightStick3( void )
{
	if ( !m_pPlayer )
		return;

	m_pPlayer->MeleeHitThinkNightStick3();
}

void CDualWeaponRight::WeaponThink( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;

	if ( pPlayer && pPlayer->m_pActiveItem )
	{
		if ( pPlayer->m_pActiveItem->m_iId != 15 )
		{
			if ( m_sModel )
				SetModel( pPlayer, 0 );
		}
	}

	if ( m_iThinkState > 0 )
	{
		if ( m_flThinkDecay < gpGlobals->time )
		{
			switch ( m_iThinkState )
			{
			case 1:
				m_pPlayer->MeleeHitThinkPunch();
				break;
			case 2:
				m_pPlayer->MeleeHitThinkSwitchblade();
				break;
			case 3:
				m_pPlayer->MeleeHitThinkSwitchblade2();
				break;
			case 4:
				m_pPlayer->MeleeHitThinkSwitchblade3();
				break;
			case 5:
				m_pPlayer->MeleeHitThinkNightStick();
				break;
			case 6:
				m_pPlayer->MeleeHitThinkNightStick2();
				break;
			case 7:
				m_pPlayer->MeleeHitThinkNightStick3();
				break;
			case 8:
				if ( !m_pPlayer->m_iPhoneDisabled )
				{
					if ( !( m_pPlayer->m_iFlashlightFlags & 1 ) )
						m_pPlayer->m_iFlashlightFlags |= 1;

					if ( m_pPlayer->m_iFlashlightFlags & 2 )
						m_pPlayer->m_iFlashlightFlags &= ~2;
				}

				m_pPlayer->m_iMobileFlashMode = 1;
				break;
			case 9:
				if ( m_pPlayer->m_iFlashlightFlags & 1 )
					m_pPlayer->m_iFlashlightFlags &= ~1;

				if ( m_pPlayer->m_iFlashlightFlags & 2 )
					m_pPlayer->m_iFlashlightFlags &= ~2;

				m_pPlayer->m_iMobileFlashMode = 0;
				break;
			case 10:
				m_pPlayer->MeleeHitThinkBranch();
				break;
			case 11:
				if ( m_pPlayer && m_pPlayer->m_pActiveItem )
				{
					if ( m_pPlayer->m_pActiveItem->m_iId == 15 )
					{
						CDualWeapon *pWeapon = GetClassPtr( (CDualWeapon *)m_pPlayer->m_pActiveItem->pev );

						if ( pWeapon )
							pWeapon->VP70Shoot();
					}
				}
				break;
			}

			m_iThinkState = 0;
		}
	}

	if ( m_bShotLooping )
	{
		if ( !( pPlayer->pev->button & IN_ATTACK ) || m_iSprintState || !m_pPlayer->IsAlive() )
		{
			m_bShotLooping = false;

			if ( shoot_loop_sound )
				STOP_SOUND( pPlayer->edict(), CHAN_AUTO, STRING( shoot_loop_sound ) );

			if ( shoot_loop_sound_end )
				EMIT_SOUND_DYN( ENT( pPlayer->pev ), CHAN_AUTO, STRING( shoot_loop_sound_end ), 1, 0.8, 0, 100 );
		}
	}

	if ( !m_iSprintState && m_flNextIdleTime < gpGlobals->time && m_flNextPrimaryAttack < gpGlobals->time && m_flNextSecondaryAttack < gpGlobals->time )
	{
		switch ( m_iID )
		{
		case 1:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 2:
			if ( m_pPlayer->m_iMobileFlashMode == 0 )
			{
				SendWeaponAnim( pPlayer, m_iIdleAnimation );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			}

			if ( m_pPlayer->m_iMobileFlashMode == 1 )
			{
				SendWeaponAnim( pPlayer, m_iIdleAnimation2 );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation2Time;
			}
			break;
		case 6:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 10:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 3:
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			{
				SendWeaponAnim( pPlayer, 0 );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			}
			else
			{
				SendWeaponAnim( pPlayer, 1 );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation2Time;
			}
			break;
		case 7:
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			{
				SendWeaponAnim( pPlayer, 0 );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			}
			else
			{
				SendWeaponAnim( pPlayer, 1 );
				m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation2Time;
			}
			break;
		case 19:
			SendWeaponAnim( pPlayer, 0 );
			m_flNextIdleTime = gpGlobals->time + 1;
			break;
		case 12:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 16:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 14:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		case 13:
			SendWeaponAnim( pPlayer, m_iIdleAnimation );
			m_flNextIdleTime = gpGlobals->time + m_fIdleAnimation1Time;
			break;
		}
	}
}

void CDualWeaponRight::SetVariables( float primarydelay, float secondarydelay, int idleanimation )
{
	m_flPrimaryAttackDelay = primarydelay;
	m_flSecondaryAttackDelay = secondarydelay;
	m_iIdleAnimation = idleanimation;
}
