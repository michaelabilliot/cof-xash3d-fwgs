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

extern int gmsgParticle;
extern int gmsgIronSights;
extern int gmsgCameraZoom;
extern unsigned short m_usThirdFlash;

// sequences of models/weapons/m16/v_m16.mdl
enum m16_e
{
	M16_DRAW_SINGLE = 0,
	M16_IDLE_SINGLE,
	M16_SHOOT_SINGLE,
	M16_RELOAD_SINGLE,
	M16_RELOAD_EMPTY_SINGLE,
	M16_HOLSTER_SINGLE,
	M16_SINGLE_TO_BURST,
	M16_DRAW_BURST,
	M16_IDLE_BURST,
	M16_SHOOT_BURST,
	M16_RELOAD_BURST,
	M16_RELOAD_EMPTY_BURST,
	M16_HOLSTER_BURST,
	M16_BURST_TO_SINGLE,
	M16_SPRINT_TO,
	M16_SPRINT_IDLE,
	M16_SPRINT_FROM,
	M16_SUICIDE,
	M16_IRON_TO,
	M16_IRON_IDLE,
	M16_IRON_FROM,
	M16_IRON_SHOOT,
	M16_MELEE,
	M16_SHOOT_NOSHOT_SINGLE,
	M16_SHOOT_NOSHOT_BURST,
	M16_IRON_SHOOT_NOSHOT,
	M16_FIDGET1_SINGLE,
	M16_FIDGET2_SINGLE,
	M16_FIDGET3_SINGLE,
	M16_FIDGET1_BURST,
	M16_FIDGET2_BURST,
	M16_FIDGET3_BURST,
	M16_IRON_SINGLETOBURST,
	M16_IRON_BURSTTOSINGLE,
	M16_JUMP_TO_SINGLE,
	M16_JUMP_FROM_SINGLE,
	M16_JUMP_TO,
	M16_JUMP_FROM,
};

LINK_ENTITY_TO_CLASS( weapon_m16, CM16 );
LINK_ENTITY_TO_CLASS( weapon_mp5k, CM16 );


void CM16::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CM16::SuicideShot );

	SendWeaponAnim( M16_SUICIDE );

	m_iSuicideButtonsPressed = 0;
	m_iSuicideButtonsNeeded = RANDOM_LONG( 10, 15 );

	UTIL_ScreenShake( m_pPlayer->pev->origin, 12.0, 100.0, 2.0, 1000.0 );
	UTIL_ScreenFade( m_pPlayer, Vector( 255, 0, 0 ), 2, 0, 125, 0 );

	m_pPlayer->pev->fuser3 = 10;

	m_pPlayer->ShowHint( MAKE_STRING( "suicide" ), false );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic1.wav", 1, ATTN_NORM );
		break;
	case 1:
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic2.wav", 1, ATTN_NORM );
		break;
	case 2:
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic3.wav", 1, ATTN_NORM );
		break;
	}
}

void CM16::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/m16/m16_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CM16::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;
	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( b_BurstMode )
		SendWeaponAnim( M16_DRAW_BURST );
	else
		SendWeaponAnim( M16_DRAW_SINGLE );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CM16::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_m16" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/m16/w_m16.mdl" );

	m_iId = 9;

	m_iSprintIdleAnim1 = M16_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	b_BurstMode = true;

	m_flKickPerShot = 0.75;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;

		m_iDefaultAmmo = 20;
	}

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_m16.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	m_bCanSuicide = true;

	strwmodel = MAKE_STRING( "models/weapons/m16/w_m16.mdl" );

	FallInit();// get ready to fall down.
}

void CM16::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/m16/v_m16.mdl" );
	PRECACHE_MODEL( "models/weapons/m16/w_m16.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	m_iShell = PRECACHE_MODEL( "models/556shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/m16/m16_cock.wav" );
	PRECACHE_SOUND( "weapons/m16/m16_magin.wav" );
	PRECACHE_SOUND( "weapons/m16/m16_magout.wav" );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/m16/p_m16.mdl" );

	PRECACHE_SOUND( "weapons/m16/m16_fire.wav" );
}

int CM16::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "m16";
	p->iMaxAmmo1 = 200;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 20;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 9;
	p->iWeight = 5;

	return 1;
}

BOOL CM16::Deploy( )
{
	m_iSprintState = 0;
	shoot_num = 0;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	m_pPlayer->ShowHint( MAKE_STRING( "weaponfunctionm16" ), false );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( b_BurstMode )
			return DefaultDeploy( "models/weapons/m16/v_m16.mdl", "police/models/weapons/m16/p_m16.mdl", M16_DRAW_BURST, "mp5" );
		else
			return DefaultDeploy( "models/weapons/m16/v_m16.mdl", "police/models/weapons/m16/p_m16.mdl", M16_DRAW_SINGLE, "mp5" );
	}
	else
	{
		if ( b_BurstMode )
			return DefaultDeploy( "models/weapons/m16/v_m16.mdl", "models/null.mdl", M16_DRAW_BURST, "mp5" );
		else
			return DefaultDeploy( "models/weapons/m16/v_m16.mdl", "models/null.mdl", M16_DRAW_SINGLE, "mp5" );
	}
}

void CM16::Holster( )
{
	if ( m_iSuicideState )
	{
		SetThink( NULL );
		m_iSuicideButtonsPressed = 0;
		m_iSuicideState = 0;
		UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );
	}

	if ( b_BurstMode )
		SendWeaponAnim( M16_HOLSTER_BURST );
	else
		SendWeaponAnim( M16_HOLSTER_SINGLE );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CM16::Shoot( void )
{
	if ( m_iSprintState || m_iSuicideState )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			if ( m_bIronSights )
				SendWeaponAnim( M16_IRON_SHOOT_NOSHOT );
			else
			{
				if ( !b_BurstMode )
					SendWeaponAnim( M16_SHOOT_NOSHOT_SINGLE );
				else
					SendWeaponAnim( M16_SHOOT_NOSHOT_BURST );
			}

			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.25;
		}

		return;
	}

	if ( m_iClip <= 0 )
	{
		if ( !m_fFireOnEmpty )
		{
			Reload( );
		}
		else
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				if ( m_bIronSights )
					SendWeaponAnim( M16_IRON_SHOOT_NOSHOT );
				else
				{
					if ( !b_BurstMode )
						SendWeaponAnim( M16_SHOOT_NOSHOT_SINGLE );
					else
						SendWeaponAnim( M16_SHOOT_NOSHOT_BURST );
				}

				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
		}

		return;
	}

	Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
	Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
	EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/m16/v_m16.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	int flags = 0;

	if ( m_pPlayer->m_iViewValue != 1 )
		flags = FEV_NOTHOST;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecShellSrc + gpGlobals->v_right * 32;
	PLAYBACK_EVENT_FULL( flags, ENT( m_pPlayer->pev ), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 );

	m_pPlayer->PlayerAnimation( "m16_shoot", false );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		m_pPlayer->ViewPunch( 1, 1, 1 );
		break;
	case 1:
		m_pPlayer->ViewPunch( -1, 1, 1 );
		break;
	case 2:
		m_pPlayer->ViewPunch( 1, 1, -1 );
		break;
	}

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	if ( m_bIronSights )
		SendWeaponAnim( M16_IRON_SHOOT );
	else
	{
		if ( b_BurstMode )
			SendWeaponAnim( M16_SHOOT_BURST );
		else
			SendWeaponAnim( M16_SHOOT_SINGLE );
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.35;
	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgM16, m_pPlayer->pev, m_pPlayer->random_seed );

	m_pPlayer->ViewKnock( 3.0 );

	int pitch = 100;

	PlayWeaponSound_guess( "weapons_new/m16/m16_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	if ( b_BurstMode )
	{
		shoot_num++;

		if ( shoot_num < 3 )
		{
			SetNextThink( 0.05 );
			SetThink( &CM16::Shoot );
		}
	}
}

void CM16::WeaponToggle( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
	{
		if ( b_BurstMode )
		{
			SendWeaponAnim( M16_IRON_BURSTTOSINGLE );
			b_BurstMode = false;
		}
		else
		{
			SendWeaponAnim( M16_IRON_SINGLETOBURST );
			b_BurstMode = true;
		}
	}
	else
	{
		if ( b_BurstMode )
		{
			SendWeaponAnim( M16_BURST_TO_SINGLE );
			b_BurstMode = false;
		}
		else
		{
			SendWeaponAnim( M16_SINGLE_TO_BURST );
			b_BurstMode = true;
		}
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
	m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CM16::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
		return;

	if ( shoot_num >= 3 )
		shoot_num = 0;

	if ( m_iClip <= 0 )
	{
		if ( !m_fFireOnEmpty )
		{
			Reload( );
		}
		else
		{
			PlayEmptySound( );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.8;
			m_flTimeWeaponIdle = gpGlobals->time + 0.8;
		}

		return;
	}

	Shoot();

	if ( b_BurstMode )
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.75;
		m_flNextSecondaryAttack = gpGlobals->time + 0.75;
		m_flTimeWeaponIdle = gpGlobals->time + 0.66;
	}
	else
	{
		m_flNextPrimaryAttack = gpGlobals->time + 0.1;
		m_flNextSecondaryAttack = gpGlobals->time + 0.1;
		m_flTimeWeaponIdle = gpGlobals->time + 0.66;
	}
}

void CM16::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
		return;

	if ( m_pPlayer->m_fStamina <= 0 )
		return;

	m_pPlayer->SetNextThink( 0.23 );
	m_pPlayer->SetThink( &CBasePlayer::MeleePushBack );
	SendWeaponAnim( M16_MELEE );

	m_pPlayer->PlayerAnimation( "m16_melee", false );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CM16::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( m_iSuicideState )
		return;

	if ( m_bWaitingToReloadIronsights )
		return;

	if ( m_bIronSights )
	{
		m_bWaitingToReloadIronsights = true;
		return;
	}

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	int j = min( 20 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	if ( j == 0 )
		return;

	m_iSprintState = 2;

	shoot_num = 0;

	if ( b_BurstMode )
	{
		if ( m_iClip )
		{
			m_iClip = 0;
			DefaultReload( 20, M16_RELOAD_BURST, 3, 0, false );
			m_pPlayer->PlayerAnimation( "m16_reload", false );
		}
		else
		{
			m_iClip = 0;
			DefaultReload( 20, M16_RELOAD_EMPTY_BURST, 4, 0, false );
			m_pPlayer->PlayerAnimation( "m16_reloadempty", false );
		}
	}
	else
	{
		if ( m_iClip )
		{
			m_iClip = 0;
			DefaultReload( 20, M16_RELOAD_SINGLE, 3, 0, false );
			m_pPlayer->PlayerAnimation( "m16_reload", false );
		}
		else
		{
			m_iClip = 0;
			DefaultReload( 20, M16_RELOAD_EMPTY_SINGLE, 4, 0, false );
			m_pPlayer->PlayerAnimation( "m16_reloadempty", false );
		}
	}
}

void CM16::WeaponIdle( void )
{
	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;
}

void CM16::ItemPostFrame( void )
{
	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

	if ( m_bWaitingToReloadIronsights && !m_pPlayer->m_bInPhoneCall && !( m_pPlayer->m_fPhoneCallTime > gpGlobals->time ) &&
		!( m_flNextPrimaryAttack > gpGlobals->time ) && !( m_flNextSecondaryAttack > gpGlobals->time ) && !( m_flNextTertiaryAttack > gpGlobals->time ) &&
		!m_iSprintState && !m_iSuicideState )
	{
		if ( m_bIronSights )
		{
			m_bIronSights = false;
			IronsightsFrom();
		}
		else
		{
			m_bWaitingToReloadIronsights = false;
			Reload();
		}
	}

	if ( m_iSuicideState == 2 )
	{
		UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 0.5, 0.5, 128, 0 );

		if ( m_pPlayer->m_afButtonPressed & ( IN_ATTACK | IN_ATTACK2 ) )
		{
			m_iSuicideButtonsPressed++;
			UTIL_ScreenShake( m_pPlayer->pev->origin, RANDOM_LONG( 5, 9 ), RANDOM_LONG( 5, 9 ), 0.15, 2.0 );

			if ( RANDOM_LONG( 0, 2 ) == 2 )
			{
				int pitch = 90 + RANDOM_LONG( 0, 15 );
				EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM, 0, pitch );
			}
		}

		if ( m_iSuicideButtonsPressed >= m_iSuicideButtonsNeeded )
			StopSuicide();
	}

	if ( m_iSuicideState == 1 )
	{
		if ( m_iSprintState && m_iNextSprintAnim < gpGlobals->time )
		{
			m_flNextPrimaryAttack = gpGlobals->time + 0.33;
			m_flTimeWeaponIdle = gpGlobals->time + 0.35;
			m_iNextSprintAnim = gpGlobals->time + 0.33;
			m_iSprintState = 0;

			SendWeaponAnim( M16_SPRINT_FROM );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
		}
	}

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() && !m_bIronSights && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( M16_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else
		{
			if ( m_iSprintState == 5 )
			{
				SendWeaponAnim( M16_SPRINT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;
				SendWeaponAnim( M16_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
				m_flTimeWeaponIdle = time / 2 + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( M16_SPRINT_IDLE );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( M16_SPRINT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = m_flNextSecondaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CM16::IronsightsTo( void )
{
	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_forward", (char *)STRING( script ) ) ) )
		m_fIronSightsForward = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_forward", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_right", (char *)STRING( script ) ) ) )
		m_fIronSightsRight = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_right", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_up", (char *)STRING( script ) ) ) )
		m_fIronSightsUp = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_up", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_zoom", (char *)STRING( script ) ) ) )
		m_iIronSightsZoom = atoi( m_pPlayer->GetDescriptionFromItem( "ironsight_zoom", (char *)STRING( script ) ) );

	MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
		WRITE_COORD( m_fIronSightsForward );
		WRITE_COORD( m_fIronSightsRight );
		WRITE_COORD( m_fIronSightsUp );
		WRITE_BYTE( m_iIronSightsZoom );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
		WRITE_COORD( 350 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_pPlayer->pev->vuser2.x = m_iIronSightsZoom;

	SendWeaponAnim( M16_IRON_TO );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CM16::IronsightsFrom( void )
{
	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_forward", (char *)STRING( script ) ) ) )
		m_fIronSightsForward = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_forward", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_right", (char *)STRING( script ) ) ) )
		m_fIronSightsRight = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_right", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_up", (char *)STRING( script ) ) ) )
		m_fIronSightsUp = atof( m_pPlayer->GetDescriptionFromItem( "ironsight_up", (char *)STRING( script ) ) );

	if ( strlen( m_pPlayer->GetDescriptionFromItem( "ironsight_zoom", (char *)STRING( script ) ) ) )
		m_iIronSightsZoom = atoi( m_pPlayer->GetDescriptionFromItem( "ironsight_zoom", (char *)STRING( script ) ) );

	MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_COORD( 0 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
		WRITE_COORD( 350 );
		WRITE_BYTE( 0 );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	m_pPlayer->pev->vuser2.x = 90;

	SendWeaponAnim( M16_IRON_FROM );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CM16::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	if ( m_bIronSights )
		return;

	if ( b_BurstMode )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( M16_FIDGET1_BURST );
			break;

		case 1:
			SendWeaponAnim( M16_FIDGET2_BURST );
			break;

		case 2:
			SendWeaponAnim( M16_FIDGET3_BURST );
			break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( M16_FIDGET1_SINGLE );
			break;

		case 1:
			SendWeaponAnim( M16_FIDGET2_SINGLE );
			break;

		case 2:
			SendWeaponAnim( M16_FIDGET3_SINGLE );
			break;
		}
	}
}

void CM16::TertiaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_CANCEL ) )
		return;

	if ( CVAR_GET_FLOAT( "ironsights_toggle" ) == 0 && m_bIronSights )
		return;

	m_bIronSights = !m_bIronSights;

	if ( m_bIronSights )
		IronsightsTo();
	else
		IronsightsFrom();
}


class CM16Ammo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/ammo_m16.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/ammo_m16.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 20, "m16", 60, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( 20, "m16", 200, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_m16, CM16Ammo );
