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

// sequences of models/weapons/glock/v_glock.mdl
enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE1_NOSHOT,
	GLOCK_SHOOT_1,
	GLOCK_SHOOT_EMPTY_1,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOSHOT,
	GLOCK_DRAW,
	GLOCK_DRAW_NOSHOT,
	GLOCK_HOLSTER,
	GLOCK_HOLSTER_NOSHOT,
	GLOCK_SPRINT_TO,
	GLOCK_SPRINT_TO_NOSHOT,
	GLOCK_SPRINT_IDLE,
	GLOCK_SPRINT_IDLE_NOSHOT,
	GLOCK_SPRINT_FROM,
	GLOCK_SPRINT_FROM_NOSHOT,
	GLOCK_SUICIDE,
	GLOCK_SWITCH,
	GLOCK_SWITCH_NOSHOT,
	GLOCK_SUICIDE_NOSHOT,
	GLOCK_SHOOT_NOSHOT,
	GLOCK_FIDGET1,
	GLOCK_FIDGET1_NOSHOT,
	GLOCK_FIDGET2,
	GLOCK_FIDGET2_NOSHOT,
	GLOCK_FIDGET3,
	GLOCK_FIDGET3_NOSHOT,
	GLOCK_MELEE,
	GLOCK_MELEE_NOSHOT,
	GLOCK_DRAW_FIRST,
	GLOCK_IRON_TO,
	GLOCK_IRON_TO_NOSHOT,
	GLOCK_IRON_IDLE,
	GLOCK_IRON_IDLE_NOSHOT,
	GLOCK_IRON_SHOOT,
	GLOCK_IRON_SHOOT_NOSHOT,
	GLOCK_IRON_FROM,
	GLOCK_IRON_FROM_NOSHOT,
	GLOCK_IRON_SHOOT_EMPTY,
	GLOCK_JUMP_TO,
	GLOCK_JUMP_TO_NOSHOT,
	GLOCK_JUMP_FROM,
	GLOCK_JUMP_FROM_NOSHOT,
};

LINK_ENTITY_TO_CLASS( weapon_beretta, CGlock );
LINK_ENTITY_TO_CLASS( weapon_glock, CGlock );


void CGlock::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CGlock::SuicideShot );

	if ( m_iClip )
		SendWeaponAnim( GLOCK_SUICIDE );
	else
		SendWeaponAnim( GLOCK_SUICIDE_NOSHOT );

	m_iSuicideButtonsPressed = 0;
	m_iSuicideButtonsNeeded = RANDOM_LONG( 10, 15 );

	UTIL_ScreenShake( m_pPlayer->pev->origin, 12.0, 100.0, 2.0, 1000.0 );
	UTIL_ScreenFade( m_pPlayer, Vector( 255, 0, 0 ), 2, 0, 125, 0 );
	m_pPlayer->pev->fuser3 = 10;
	m_pPlayer->ShowHint( MAKE_STRING( "suicide" ), false );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0: EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic1.wav", 1, ATTN_NORM ); break;
	case 1: EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic2.wav", 1, ATTN_NORM ); break;
	case 2: EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_VOICE, "player/Vocal/Suicide/suicide_panic3.wav", 1, ATTN_NORM ); break;
	}
}

void CGlock::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/glock/glock_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CGlock::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	if ( m_iClip )
		SendWeaponAnim( GLOCK_DRAW );
	else
		SendWeaponAnim( GLOCK_DRAW_NOSHOT );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CGlock::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_glock" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/glock/w_glock.mdl" );

	m_iId = WEAPON_GLOCK;
	m_iSprintIdleAnim1 = GLOCK_SPRINT_IDLE;
	m_iSprintIdleAnim2 = GLOCK_SPRINT_IDLE_NOSHOT;
	m_flKickPerShot = 1.25;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;
		m_iDefaultAmmo = GLOCK_MAX_CLIP;
	}

	m_bCanDualWield = true;
	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_glock.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;
	m_bCanSuicide = true;
	strwmodel = MAKE_STRING( "models/weapons/glock/w_glock.mdl" );

	FallInit();// get ready to fall down.
}

void CGlock::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/glock/v_glock.mdl" );
	PRECACHE_MODEL( "models/weapons/glock/w_glock.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/glock/p_glock.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/glock/glock_switch.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_magin.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_magout.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_magplace.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_rackback.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_slide.wav" );
	PRECACHE_SOUND( "weapons/rifle/rifle_insert.wav" );
	PRECACHE_SOUND( "weapons/glock/glock_fire.wav" );
}

int CGlock::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "glock";
	p->iMaxAmmo1 = 150;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = GLOCK_MAX_CLIP;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = WEAPON_GLOCK;
	p->iWeight = GLOCK_WEIGHT;

	return 1;
}

BOOL CGlock::Deploy( )
{
	if ( m_bTaclight )
	{
		if ( m_pPlayer->m_iFlashlightFlags & 4 )
			pev->body = 2;
		else
			pev->body = 1;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	if ( !m_bTaclight && pev->body )
	{
		pev->body = 0;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	m_iSprintState = 0;
	m_iSuicideState = ( m_iSuicideState == 666 );

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		m_pPlayer->ShowHint( MAKE_STRING( "ironsights" ), false );

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "police/models/weapons/glock/p_glock.mdl", GLOCK_DRAW_FIRST, "onehanded" );

		return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "models/null.mdl", GLOCK_DRAW_FIRST, "onehanded" );
	}

	if ( m_iClip )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "police/models/weapons/glock/p_glock.mdl", GLOCK_DRAW, "onehanded" );

		return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "models/null.mdl", GLOCK_DRAW, "onehanded" );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "police/models/weapons/glock/p_glock.mdl", GLOCK_DRAW_NOSHOT, "onehanded" );

	return DefaultDeploy( "models/weapons/glock/v_glock.mdl", "models/null.mdl", GLOCK_DRAW_NOSHOT, "onehanded" );
}

void CGlock::Holster( )
{
	if ( m_iClip )
		SendWeaponAnim( GLOCK_HOLSTER );
	else
		SendWeaponAnim( GLOCK_HOLSTER_NOSHOT );

	if ( m_iSuicideState )
	{
		SetThink( NULL );
		m_iSuicideButtonsPressed = 0;
		m_iSuicideState = 0;

		UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

		if ( m_pPlayer->pev->fuser3 == 10 )
			m_pPlayer->pev->fuser3 = 0;
	}

	if ( m_pPlayer->m_iFlashlightFlags & 4 )
		m_pPlayer->m_iFlashlightFlags &= ~4;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CGlock::ToggleSwitch( void )
{
	if ( m_pPlayer->m_iFlashlightFlags & 4 )
		m_pPlayer->m_iFlashlightFlags &= ~4;
	else
		m_pPlayer->m_iFlashlightFlags |= 4;
}

void CGlock::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState || m_bIronSights )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		if ( m_iClip )
			SendWeaponAnim( GLOCK_FIDGET1 );
		else
			SendWeaponAnim( GLOCK_FIDGET1_NOSHOT );
		break;

	case 1:
		if ( m_iClip )
			SendWeaponAnim( GLOCK_FIDGET2 );
		else
			SendWeaponAnim( GLOCK_FIDGET2_NOSHOT );
		break;

	case 2:
		if ( m_iClip )
			SendWeaponAnim( GLOCK_FIDGET3 );
		else
			SendWeaponAnim( GLOCK_FIDGET3_NOSHOT );
		break;
	}
}

void CGlock::WeaponToggle( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( !m_bTaclight || m_bIronSights || m_iSprintState || m_iSuicideState )
		return;

	if ( m_iClip )
		SendWeaponAnim( GLOCK_SWITCH );
	else
		SendWeaponAnim( GLOCK_SWITCH_NOSHOT );

	SetNextThink( 0.3 );
	SetThink( &CGlock::ToggleSwitch );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.66;
}

void CGlock::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState || m_bIronSights )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );
	m_pPlayer->PlayerAnimation( "pistol_melee", false );

	if ( m_iClip )
		SendWeaponAnim( GLOCK_MELEE );
	else
		SendWeaponAnim( GLOCK_MELEE_NOSHOT );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CGlock::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_iSprintState || m_iSuicideState )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_bIronSights )
			SendWeaponAnim( GLOCK_IRON_SHOOT_NOSHOT );
		else
			SendWeaponAnim( GLOCK_SHOOT_NOSHOT );

		if ( gpGlobals->time > m_fEmptyClickTime )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.5;
		}

		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
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
			if ( m_bIronSights )
				SendWeaponAnim( GLOCK_IRON_SHOOT_NOSHOT );
			else
				SendWeaponAnim( GLOCK_SHOOT_NOSHOT );

			if ( gpGlobals->time > m_fEmptyClickTime )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.5;
			}
		}

		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
		return;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( ENTINDEX( edict() ) );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/glock/v_glock.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	int flags;
	if ( m_pPlayer->m_iViewValue != 1 )
		flags = FEV_NOTHOST;
	else
		flags = 0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecSrc + gpGlobals->v_right * 32;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 );

	m_pPlayer->PlayerAnimation( "pistol_shoot", false );
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;
	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.55;

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgGlock, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewPunch( RANDOM_LONG( -2, 2 ), 1, 1 );

	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	if ( m_bIronSights )
	{
		if ( !m_iClip )
			SendWeaponAnim( GLOCK_IRON_SHOOT_EMPTY );
		else
			SendWeaponAnim( GLOCK_IRON_SHOOT );
	}
	else
	{
		if ( !m_iClip )
			SendWeaponAnim( GLOCK_SHOOT_EMPTY_1 );
		else
			SendWeaponAnim( GLOCK_SHOOT_1 );
	}

	m_pPlayer->ViewKnock( 3.0 );

	PlayWeaponSound_guess( "weapons_new/glock/glock_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	m_flNextPrimaryAttack = gpGlobals->time + 0.1;
	m_flNextSecondaryAttack = gpGlobals->time + 1;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CGlock::Reload( void )
{
	int j = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

	if ( j <= 0 || m_flNextPrimaryAttack > gpGlobals->time || m_iSprintState == 1 || m_iSprintState > 2 || m_iSuicideState || m_bWaitingToReloadIronsights )
		return;

	if ( m_bIronSights )
	{
		m_bWaitingToReloadIronsights = true;
		return;
	}

	if ( GLOCK_MAX_CLIP - m_iClip >= j || GLOCK_MAX_CLIP - m_iClip != 0 )
	{
		m_fReloadSpeed = 0;
		m_iSprintState = 2;

		if ( m_iClip )
		{
			m_iClip = 0;
			DefaultReload( GLOCK_MAX_CLIP, GLOCK_RELOAD, 2.6, 0, false );
			m_pPlayer->PlayerAnimation( "pistol_reload", false );
		}
		else
		{
			m_iClip = 0;
			DefaultReload( GLOCK_MAX_CLIP, GLOCK_RELOAD_NOSHOT, 3.5, 0, false );
			m_pPlayer->PlayerAnimation( "pistol_reloadempty", false );
		}
	}
}

void CGlock::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time || m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
	{
		if ( m_iClip )
			SendWeaponAnim( GLOCK_IRON_IDLE );
		else
			SendWeaponAnim( GLOCK_IRON_IDLE_NOSHOT );
	}
	else
	{
		if ( m_iClip )
			SendWeaponAnim( GLOCK_IDLE1 );
		else
			SendWeaponAnim( GLOCK_IDLE1_NOSHOT );
	}

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CGlock::AttachTaclight( void )
{
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/sleeve_generic3.wav", 1, ATTN_NORM );

	m_bTaclight = true;
	if ( m_pPlayer->m_iFlashlightFlags & 4 )
		pev->body = 2;
	else
		pev->body = 1;
	SendWeaponBody( m_pPlayer, pev->body );
	m_bWaitingToFix = false;

	m_pPlayer->ShowHint( MAKE_STRING( "weaponfunctionglock" ), false );

	if ( m_iClip )
	{
		SendWeaponAnim( GLOCK_DRAW );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	}
	else
	{
		SendWeaponAnim( GLOCK_DRAW_NOSHOT );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
}

void CGlock::IronsightsTo( void )
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
		SendWeaponAnim( GLOCK_IRON_TO );
	else
		SendWeaponAnim( GLOCK_IRON_TO_NOSHOT );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CGlock::IronsightsFrom( void )
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
		SendWeaponAnim( GLOCK_IRON_FROM );
	else
		SendWeaponAnim( GLOCK_IRON_FROM_NOSHOT );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CGlock::TertiaryAttack( void )
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

void CGlock::MidAttachTaclight( void )
{
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/rifle/rifle_insert.wav", 1, ATTN_NORM );

	SetNextThink( 1.0 );
	SetThink( &CGlock::AttachTaclight );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 5;
}

void CGlock::ItemPostFrame( void )
{
	if ( !m_iSprintState && !m_iSuicideState && m_flNextPrimaryAttack < gpGlobals->time && m_flNextSecondaryAttack < gpGlobals->time && m_flNextTertiaryAttack < gpGlobals->time )
	{
		if ( CVAR_GET_FLOAT( "ironsights_toggle" ) == 0 && m_bIronSights && !( ( m_pPlayer->pev->button | m_pPlayer->m_afButtonReleased | m_pPlayer->m_afButtonPressed ) & IN_CANCEL ) )
		{
			m_bIronSights = false;
			IronsightsFrom();
		}
	}

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

	if ( m_bTaclight )
	{
		if ( m_pPlayer->m_iFlashlightFlags & 4 )
		{
			if ( pev->body != 2 )
				pev->body = 2;
		}
		else
		{
			if ( pev->body != 1 )
				pev->body = 1;
		}

		SendWeaponBody( m_pPlayer, pev->body );
	}

	if ( !m_bTaclight && pev->body )
	{
		pev->body = 0;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	if ( m_bWaitingToFix && !m_pPlayer->m_bInPhoneCall && !( m_pPlayer->m_fPhoneCallTime > gpGlobals->time ) &&
		!( m_flNextPrimaryAttack > gpGlobals->time ) && !( m_flNextSecondaryAttack > gpGlobals->time ) && !( m_flNextTertiaryAttack > gpGlobals->time ) &&
		!m_iSprintState && !m_iSuicideState )
	{
		if ( m_bIronSights )
		{
			m_bIronSights = false;
			IronsightsFrom();
		}

		if ( m_iClip )
		{
			SendWeaponAnim( GLOCK_HOLSTER );
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

			SetNextThink( 1.0 );
			SetThink( &CGlock::MidAttachTaclight );
		}
		else
		{
			SendWeaponAnim( GLOCK_HOLSTER_NOSHOT );
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

			SetNextThink( 1.0 );
			SetThink( &CGlock::MidAttachTaclight );
		}

		EMIT_SOUND( ENT( pev ), CHAN_VOICE, "weapons/sleeve_generic2.wav", 1, ATTN_NORM );

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 5;
	}

	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

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
				SendWeaponAnim( GLOCK_SPRINT_FROM );
			else
				SendWeaponAnim( GLOCK_SPRINT_FROM_NOSHOT );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
		}
	}

	float time;
	float speed = m_pPlayer->pev->velocity.Length2D();

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() && !m_bIronSights && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				if ( m_iClip )
					SendWeaponAnim( GLOCK_JUMP_TO );
				else
					SendWeaponAnim( GLOCK_JUMP_TO_NOSHOT );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			if ( m_iClip )
				SendWeaponAnim( GLOCK_JUMP_FROM );
			else
				SendWeaponAnim( GLOCK_JUMP_FROM_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 && m_iSprintState != 1 )
		{
			m_iSprintState = 1;

			if ( m_iClip )
				SendWeaponAnim( GLOCK_SPRINT_TO );
			else
				SendWeaponAnim( GLOCK_SPRINT_TO_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
			m_iNextSprintAnim = time + gpGlobals->time;
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 && m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time )
		{
			float time;
	float speed = m_pPlayer->pev->velocity.Length2D();
			if ( speed != 0 )
			{
				if ( m_iClip )
					SendWeaponAnim( GLOCK_SPRINT_IDLE );
				else
					SendWeaponAnim( GLOCK_SPRINT_IDLE_NOSHOT );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 && m_iSprintState == 1 )
		{
			if ( m_iClip )
				SendWeaponAnim( GLOCK_SPRINT_FROM );
			else
				SendWeaponAnim( GLOCK_SPRINT_FROM_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = gpGlobals->time + 0.33;
			m_flTimeWeaponIdle = gpGlobals->time + 0.35;
			m_iSprintState = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CGlockAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/clip.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/clip.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( GLOCK_MAX_CLIP, "glock", 45, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( GLOCK_MAX_CLIP, "glock", 150, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_glock, CGlockAmmo );
