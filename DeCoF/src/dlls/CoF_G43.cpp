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

// sequences of models/weapons/g43/v_g43.mdl
enum g43_e
{
	G43_DRAW = 0,
	G43_DRAW_EMPTY,
	G43_FIRE_EMPTY,
	G43_FIRE_LAST,
	G43_FIRE1,
	G43_FIRE2,
	G43_FIRE3,
	G43_HOLSTER,
	G43_HOLSTER_EMPTY,
	G43_IDLE,
	G43_IDLE_EMPTY,
	G43_IRON_FIRE_EMPTY,
	G43_IRON_FIRE_LAST,
	G43_IRON_FIRE1,
	G43_IRON_FIRE2,
	G43_IRON_FIRE3,
	G43_IRON_FROM,
	G43_IRON_FROM_EMPTY,
	G43_IRON_IDLE,
	G43_IRON_IDLE_EMPTY,
	G43_IRON_TO,
	G43_IRON_TO_EMPTY,
	G43_JUMP_FROM,
	G43_JUMP_FROM_EMPTY,
	G43_JUMP_TO,
	G43_JUMP_TO_EMPTY,
	G43_MELEE,
	G43_MELEE_EMPTY,
	G43_RELOAD,
	G43_RELOAD_MID,
	G43_SPRINT,
	G43_SPRINT_EMPTY,
	G43_SPRINT_FROM,
	G43_SPRINT_FROM_EMPTY,
	G43_SPRINT_TO,
	G43_SPRINT_TO_EMPTY,
	G43_SUICIDE,
	G43_SUICIDE_EMPTY,
};

LINK_ENTITY_TO_CLASS( weapon_g43, CG43 );


void CG43::Fidget( void )
{
}

void CG43::IronsightsTo( void )
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

	if ( m_iClip )
		SendWeaponAnim( G43_IRON_TO );
	else
		SendWeaponAnim( G43_IRON_TO_EMPTY );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CG43::IronsightsFrom( void )
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

	if ( m_iClip )
		SendWeaponAnim( G43_IRON_FROM );
	else
		SendWeaponAnim( G43_IRON_FROM_EMPTY );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CG43::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CG43::SuicideShot );

	if ( m_iClip )
		SendWeaponAnim( G43_SUICIDE );
	else
		SendWeaponAnim( G43_SUICIDE_EMPTY );

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

void CG43::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/g43/g43_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CG43::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	if ( m_iClip )
		SendWeaponAnim( G43_DRAW );
	else
		SendWeaponAnim( G43_DRAW_EMPTY );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CG43::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_g43" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/g43/w_g43.mdl" );

	m_iId = 26;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;

		m_iDefaultAmmo = 10;
	}

	m_flKickPerShot = 2.4;

	m_iSprintIdleAnim1 = G43_SPRINT;
	m_iSprintIdleAnim2 = G43_SPRINT_EMPTY;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_g43.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	m_bCanSuicide = true;

	strwmodel = MAKE_STRING( "models/weapons/g43/w_g43.mdl" );

	FallInit();// get ready to fall down.
}

void CG43::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/g43/v_g43.mdl" );
	PRECACHE_MODEL( "models/weapons/g43/w_g43.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/g43/p_g43.mdl" );

	PRECACHE_SOUND( "weapons/g43/g43_fire.wav" );
}

int CG43::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "g43";
	p->iMaxAmmo1 = 40;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 10;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 26;
	p->iWeight = 5;

	return 1;
}

BOOL CG43::Deploy( )
{
	m_iSprintState = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( m_iClip )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/g43/v_g43.mdl", "police/models/weapons/g43/p_g43.mdl", G43_DRAW, "onehanded" );

		return DefaultDeploy( "models/weapons/g43/v_g43.mdl", "models/null.mdl", G43_DRAW, "onehanded" );
	}
	else
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/g43/v_g43.mdl", "police/models/weapons/g43/p_g43.mdl", G43_DRAW_EMPTY, "onehanded" );

		return DefaultDeploy( "models/weapons/g43/v_g43.mdl", "models/null.mdl", G43_DRAW_EMPTY, "onehanded" );
	}
}

void CG43::Holster( )
{
	if ( m_iSuicideState )
	{
		SetThink( NULL );
		m_iSuicideButtonsPressed = 0;
		m_iSuicideState = 0;

		UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

		if ( m_pPlayer->pev->fuser3 == 10 )
			m_pPlayer->pev->fuser3 = 0;
	}

	if ( m_bIronSights )
	{
		int on = 0;

		if ( m_bIronSights )
			on = 1;

		MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( on );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 350 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_pPlayer->pev->vuser2.x = 90;

		m_bIronSights = false;
	}

	if ( m_iClip )
		SendWeaponAnim( G43_HOLSTER );
	else
		SendWeaponAnim( G43_HOLSTER_EMPTY );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.05;
}

void CG43::SecondaryAttack( void )
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

	if ( m_iClip )
		SendWeaponAnim( G43_MELEE );
	else
		SendWeaponAnim( G43_MELEE_EMPTY );

	m_pPlayer->PlayerAnimation( "m16_melee", false );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CG43::PrimaryAttack( void )
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
			if ( m_bIronSights )
				SendWeaponAnim( G43_IRON_FIRE_EMPTY );
			else
				SendWeaponAnim( G43_FIRE_EMPTY );

			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.5;
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
					SendWeaponAnim( G43_IRON_FIRE_EMPTY );
				else
					SendWeaponAnim( G43_FIRE_EMPTY );

				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.5;
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
		WRITE_STRING( "models/weapons/g43/v_g43.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	int flags = 0;

	if ( m_pPlayer->m_iViewValue != 1 )
		flags = FEV_NOTHOST;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecSrc + gpGlobals->v_right * 32;

	PLAYBACK_EVENT_FULL( flags, ENT( m_pPlayer->pev ), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 );

	m_pPlayer->PlayerAnimation( "m16_shoot", false );

	m_pPlayer->ViewPunch( RANDOM_LONG( -8, 8 ), 5, 5 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 6.0, 0.5, 0.25, 3.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 8.0, 0.4, 0.25, 3.0 );

	if ( m_bIronSights )
	{
		if ( !m_iClip )
			SendWeaponAnim( G43_IRON_FIRE_EMPTY );
		else
			SendWeaponAnim( G43_IRON_FIRE1 + RANDOM_LONG( 0, 2 ) );
	}
	else
	{
		if ( !m_iClip )
			SendWeaponAnim( G43_FIRE_LAST );
		else
			SendWeaponAnim( G43_FIRE1 + RANDOM_LONG( 0, 2 ) );
	}

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.95;

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgG43_guess, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 6.0 );

	PlayWeaponSound_guess( "weapons_new/g43/g43_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	m_flNextPrimaryAttack = gpGlobals->time + 0.9;
	m_flNextSecondaryAttack = gpGlobals->time + 0.9;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 0.9;
}

void CG43::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
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

	int j = min( 10 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	if ( j == 0 )
		return;

	m_iSprintState = 2;

	if ( m_iClip )
	{
		m_iClip = 0;
		DefaultReload( 10, G43_RELOAD_MID, 2.73, 0, false );
		m_pPlayer->PlayerAnimation( "m16_reload", false );
	}
	else
	{
		m_iClip = 0;
		DefaultReload( 10, G43_RELOAD, 3.16, 0, false );
		m_pPlayer->PlayerAnimation( "m16_reloadempty", false );
	}
}

void CG43::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
	{
		if ( m_iClip )
			SendWeaponAnim( G43_IRON_IDLE );
		else
			SendWeaponAnim( G43_IRON_IDLE_EMPTY );
	}
	else
	{
		if ( m_iClip )
			SendWeaponAnim( G43_IDLE );
		else
			SendWeaponAnim( G43_IDLE_EMPTY );
	}

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CG43::TertiaryAttack( void )
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

void CG43::ItemPostFrame( void )
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

	if ( !m_iSprintState && !m_iSuicideState && m_flNextPrimaryAttack < gpGlobals->time && m_flNextSecondaryAttack < gpGlobals->time && m_flNextTertiaryAttack < gpGlobals->time )
	{
		if ( CVAR_GET_FLOAT( "ironsights_toggle" ) == 0 && m_bIronSights && !( ( m_pPlayer->pev->button | m_pPlayer->m_afButtonReleased | m_pPlayer->m_afButtonPressed ) & IN_CANCEL ) )
		{
			m_bIronSights = false;
			IronsightsFrom();
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

			if ( m_iClip )
				SendWeaponAnim( G43_SPRINT_FROM );
			else
				SendWeaponAnim( G43_SPRINT_FROM_EMPTY );
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

				if ( m_iClip )
					SendWeaponAnim( G43_SPRINT_TO );
				else
					SendWeaponAnim( G43_SPRINT_TO_EMPTY );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			if ( m_iClip )
				SendWeaponAnim( G43_SPRINT_FROM );
			else
				SendWeaponAnim( G43_SPRINT_FROM_EMPTY );

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

				if ( m_iClip )
					SendWeaponAnim( G43_SPRINT_TO );
				else
					SendWeaponAnim( G43_SPRINT_TO_EMPTY );

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
				if ( m_iClip )
					SendWeaponAnim( G43_SPRINT );
				else
					SendWeaponAnim( G43_SPRINT_EMPTY );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				if ( m_iClip )
					SendWeaponAnim( G43_SPRINT_FROM );
				else
					SendWeaponAnim( G43_SPRINT_FROM_EMPTY );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CG43Ammo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/ammo_g43.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/ammo_g43.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 10, "g43", 30, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else
		{
			if ( pOther->GiveAmmo( 10, "g43", 40, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}

		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_g43, CG43Ammo );
