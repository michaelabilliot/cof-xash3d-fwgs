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

// sequences of models/weapons/shotgun/v_shotgun.mdl
enum shotgun_e
{
	SHOTGUN_IDLE1 = 0,
	SHOTGUN_SHOOT1,
	SHOTGUN_REL_START,
	SHOTGUN_REL_INSERT,
	SHOTGUN_REL_END,
	SHOTGUN_REL_END_NOSHOT,
	SHOTGUN_DRAW,
	SHOTGUN_HOLSTER,
	SHOTGUN_SPRINT_TO,
	SHOTGUN_SPRINT_IDLE,
	SHOTGUN_SPRINT_FROM,
	SHOTGUN_FIDGET1,
	SHOTGUN_FIDGET2,
	SHOTGUN_FIDGET3,
	SHOTGUN_IRON_TO,
	SHOTGUN_IRON_IDLE,
	SHOTGUN_IRON_SHOOT,
	SHOTGUN_IRON_FROM,
	SHOTGUN_MELEE,
	SHOTGUN_SUICIDE,
};

LINK_ENTITY_TO_CLASS( weapon_shotgun, CShotgun );

void CShotgun::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_shotgun" );
	Precache( );
	m_iId = 4;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/shotgun/w_shotgun.mdl" );

	m_iSprintIdleAnim1 = SHOTGUN_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	m_bCanSuicide = true;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;

		m_iDefaultAmmo = 5;
	}

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_shotgun.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/shotgun/w_shotgun.mdl" );

	FallInit();// get ready to fall down.
}

void CShotgun::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/shotgun/v_shotgun.mdl" );
	PRECACHE_MODEL( "models/weapons/shotgun/w_shotgun.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/shotgun/p_shotgun.mdl" );

	PRECACHE_MODEL( "models/null.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shotgunshell.mdl" );// shotgun shell

	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/shotgun/shoot.wav" );
	PRECACHE_SOUND( "weapons/shotgun/insert.wav" );
	PRECACHE_SOUND( "weapons/shotgun/pump_forward.wav" );
	PRECACHE_SOUND( "weapons/shotgun/pump_back.wav" );
	PRECACHE_SOUND( "weapons/shotgun/pump_seq.wav" );
}

int CShotgun::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "buckshot";
	p->iMaxAmmo1 = 50;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 5;
	p->iSlot = 5;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 4;
	p->iWeight = 20;

	return 1;
}

BOOL CShotgun::Deploy( )
{
	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	m_iSprintState = 0;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/shotgun/v_shotgun.mdl", "police/models/weapons/shotgun/p_shotgun.mdl", SHOTGUN_DRAW, "shotgun", 1 );

	return DefaultDeploy( "models/weapons/shotgun/v_shotgun.mdl", "models/null.mdl", SHOTGUN_DRAW, "shotgun", 1 );
}

void CShotgun::Holster( )
{
	SendWeaponAnim( SHOTGUN_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

	if ( m_iSuicideState )
	{
		SetThink( NULL );
		m_iSuicideButtonsPressed = 0;
		m_iSuicideState = 0;
		UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

		if ( m_pPlayer->pev->fuser3 == 10 )
			m_pPlayer->pev->fuser3 = 0;
	}
}

void CShotgun::SecondaryAttack( void )
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
	SendWeaponAnim( SHOTGUN_MELEE );

	m_pPlayer->PlayerAnimation( "shotgun_melee", false );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CShotgun::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.25;
		}

		return;
	}

	if ( m_fInReload )
	{
		if ( !pev->iuser2 )
			pev->iuser2 = 1;
	}

	if ( m_iClip <= 0 && !m_fInReload )
	{
		if ( !m_iClip )
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
		}

		return;
	}

	if ( m_fInReload )
		return;

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	SetThink( &CShotgun::LolShell );
	pev->nextthink = gpGlobals->time + 0.45;

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/shotgun/v_shotgun.mdl" );
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

	m_pPlayer->PlayerAnimation( "shotgun_shoot", false );

	UTIL_ScreenShake( m_pPlayer->pev->origin, 7.0, 0.35, 0.15, 2.0 );

	if ( m_bIronSights )
		SendWeaponAnim( SHOTGUN_IRON_SHOOT );
	else
		SendWeaponAnim( SHOTGUN_SHOOT1 );

	PlayWeaponSound_guess( "weapons_new/shotgun/shoot.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	Vector vecDir;

	m_pPlayer->FireBulletsPlayer( 8, vecSrc, vecAiming, VECTOR_CONE_8DEGREES, 2048, BULLET_PLAYER_357, 0, gSkillData.DmgShotgun, m_pPlayer->pev, m_pPlayer->random_seed );

	m_pPlayer->ViewPunch( RANDOM_LONG( -4, 4 ), 2, 2 );
	m_pPlayer->ViewKnock( 5.0 );

	if ( !m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate( "!HEV_AMO0", FALSE, 0 );

	m_flNextPrimaryAttack = gpGlobals->time + 1;

	if ( m_iClip )
		m_flTimeWeaponIdle = gpGlobals->time + 5.0;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_fInSpecialReload = 0;
}

void CShotgun::IronsightsTo( void )
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

	SendWeaponAnim( SHOTGUN_IRON_TO );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CShotgun::IronsightsFrom( void )
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

	SendWeaponAnim( SHOTGUN_IRON_FROM );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CShotgun::TertiaryAttack( void )
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

void CShotgun::ShellInsert( void )
{
	// Add them to the clip
	m_iClip += 1;
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= 1;
	m_fInSpecialReload = 1;
}

void CShotgun::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 || m_iClip == 5 )
		return;

	if ( m_iClip == 0 )
		pev->iuser1 = 1;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( m_iSuicideState )
		return;

	// don't reload until recoil is done
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_bWaitingToReloadIronsights )
		return;

	if ( m_bIronSights )
	{
		m_bWaitingToReloadIronsights = true;
		return;
	}

	m_iSprintState = 2;

	// check to see if we're ready to reload
	if ( m_fInSpecialReload == 0 )
	{
		pev->iuser2 = 0;
		SendWeaponAnim( SHOTGUN_REL_START );
		m_fInSpecialReload = 1;
		m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
		m_flTimeWeaponIdle = gpGlobals->time + 0.5;
		m_flNextPrimaryAttack = gpGlobals->time + 0.51;

		m_pPlayer->PlayerAnimation( "shotgun_reload_start", false );
		m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 1;
		return;
	}
	else if ( m_fInSpecialReload == 1 )
	{
		if ( m_flTimeWeaponIdle > gpGlobals->time )
			return;

		// was waiting for gun to move to side
		m_fInSpecialReload = 2;

		SendWeaponAnim( SHOTGUN_REL_INSERT );

		m_pPlayer->PlayerAnimation( "shotgun_reload_insert", false );
		m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 1;

		m_flNextReload = gpGlobals->time + 0.6;
		m_flTimeWeaponIdle = gpGlobals->time + 0.65;

		SetNextThink( 0.33 );
		SetThink( &CShotgun::ShellInsert );
	}
}

void CShotgun::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	if ( m_bIronSights )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		SendWeaponAnim( SHOTGUN_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( SHOTGUN_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( SHOTGUN_FIDGET3 );
		break;
	}
}

void CShotgun::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
		SendWeaponAnim( SHOTGUN_IRON_IDLE );
	else
		SendWeaponAnim( SHOTGUN_IDLE1 );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CShotgun::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CShotgun::SuicideShot );

	SendWeaponAnim( SHOTGUN_SUICIDE );

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

void CShotgun::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/shotgun/shoot.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CShotgun::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	SendWeaponAnim( SHOTGUN_DRAW );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CShotgun::ItemPostFrame( void )
{
	if ( m_fInSpecialReload && ( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
	{
		if ( !pev->iuser2 && m_iClip >= 1 )
			pev->iuser2 = 1;
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

			SendWeaponAnim( SHOTGUN_SPRINT_FROM );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
		}
	}

	if ( m_bWaitingToReloadIronsights && !m_fInSpecialReload )
	{
		if ( !m_pPlayer->m_bInPhoneCall && !( m_pPlayer->m_fPhoneCallTime > gpGlobals->time ) &&
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
	}

	if ( m_flTimeWeaponIdle < gpGlobals->time )
	{
		if ( m_fInSpecialReload )
		{
			if ( !pev->iuser2 )
			{
				CBasePlayer *pPlayer = m_pPlayer;
				if ( m_iClip != 5 && pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
				{
					Reload();
				}
				else
				{
					// reload debounce has timed out
					if ( pev->iuser1 )
					{
						SendWeaponAnim( SHOTGUN_REL_END_NOSHOT );
						m_flNextPrimaryAttack = gpGlobals->time + 0.6;
						m_pPlayer->PlayerAnimation( "shotgun_reload_endempty", false );
					}
					else
					{
						pPlayer->PlayerAnimation( "shotgun_reloadend", false );
						SendWeaponAnim( SHOTGUN_REL_END );
						m_flNextPrimaryAttack = gpGlobals->time + 0.5;
					}

					m_iSprintState = 0;
					pev->iuser1 = 0;

					m_fInSpecialReload = 0;
					m_flTimeWeaponIdle = gpGlobals->time + 1;

					pev->iuser2 = 0;
				}
			}
			else
			{
				if ( pev->iuser1 )
				{
					SendWeaponAnim( SHOTGUN_REL_END_NOSHOT );
					m_flNextPrimaryAttack = gpGlobals->time + 0.6;
					m_pPlayer->PlayerAnimation( "shotgun_reload_endempty", false );
				}
				else
				{
					SendWeaponAnim( SHOTGUN_REL_END );
					m_flNextPrimaryAttack = gpGlobals->time + 0.5;
					m_pPlayer->PlayerAnimation( "shotgun_reload_end", false );
				}

				m_iSprintState = 0;
				pev->iuser1 = 0;

				m_fInSpecialReload = 0;
				m_flTimeWeaponIdle = gpGlobals->time + 1;

				pev->iuser2 = 0;
			}
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
				SendWeaponAnim( SHOTGUN_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( SHOTGUN_SPRINT_FROM );

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
				SendWeaponAnim( SHOTGUN_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( SHOTGUN_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( SHOTGUN_SPRINT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CShotgun::LolShell( void )
{
	Vector vecShellVelocity = gpGlobals->v_right * 60 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
	Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 45 + gpGlobals->v_right * 30;
	EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHOTSHELL );
}


class CShotgunAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/ammo_shotshells.mdl" );
		CBasePlayerAmmo::Spawn( );

		pev->iuser1 = 9;
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/ammo_shotshells.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( pev->message )
		{
			if ( pOther->GiveAmmo( pev->message, "buckshot", 50, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
			return FALSE;
		}
		else
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				if ( pOther->GiveAmmo( pev->iuser1, "buckshot", 27, false ) != -1 )
				{
					EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
					return TRUE;
				}
				return FALSE;
			}
			else
			{
				if ( pOther->GiveAmmo( 9, "buckshot", 50, false ) != -1 )
				{
					EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
					return TRUE;
				}
				return FALSE;
			}
		}
	}
};
LINK_ENTITY_TO_CLASS( ammo_shells, CShotgunAmmo );
