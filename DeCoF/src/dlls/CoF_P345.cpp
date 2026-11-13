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

// sequences of models/weapons/p345/v_p345.mdl
enum p345_e
{
	P345_IDLETEST = 0,
	P345_IDLE1_NOSHOT,
	P345_SHOOT_1,
	P345_SHOOT_EMPTY_1,
	P345_RELOAD,
	P345_RELOAD_NOSHOT,
	P345_DRAW,
	P345_DRAW_NOSHOT,
	P345_HOLSTER,
	P345_HOLSTER_NOSHOT,
	P345_SPRINT_TO,
	P345_SPRINT_IDLE,
	P345_SPRINT_FROM,
	P345_SPRINT_TO_NOSHOT,
	P345_SPRINT_IDLE_NOSHOT,
	P345_SPRINT_FROM_NOSHOT,
	P345_SHOOT_NOSHOT,
	P345_MELEE,
	P345_MELEE_NOSHOT,
	P345_IRON_TO,
	P345_IRON_TO_NOSHOT,
	P345_IRON_IDLE,
	P345_IRON_IDLE_NOSHOT,
	P345_IRON_FROM,
	P345_IRON_FROM_NOSHOT,
	P345_IRON_SHOOT,
	P345_IRON_SHOOT_EMPTY,
	P345_IRON_SHOOT_NOSHOT,
	P345_FIDGET1,
	P345_FIDGET1_NOSHOT,
	P345_FIDGET2,
	P345_FIDGET2_NOSHOT,
	P345_FIDGET3,
	P345_FIDGET3_NOSHOT,
	P345_SUICIDE,
	P345_SUICIDE_NOSHOT,
};

LINK_ENTITY_TO_CLASS( weapon_p345, CP345 );
LINK_ENTITY_TO_CLASS( weapon_P228, CP345 );


void CP345::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 || m_bIronSights )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		if ( !m_iClip )
			SendWeaponAnim( P345_FIDGET1_NOSHOT );
		else
			SendWeaponAnim( P345_FIDGET1 );
		break;

	case 1:
		if ( !m_iClip )
			SendWeaponAnim( P345_FIDGET2_NOSHOT );
		else
			SendWeaponAnim( P345_FIDGET2 );
		break;

	case 2:
		if ( !m_iClip )
			SendWeaponAnim( P345_FIDGET3_NOSHOT );
		else
			SendWeaponAnim( P345_FIDGET3 );
		break;
	}
}

void CP345::IronsightsTo( void )
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
		SendWeaponAnim( P345_IRON_TO );
	else
		SendWeaponAnim( P345_IRON_TO_NOSHOT );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CP345::IronsightsFrom( void )
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
		SendWeaponAnim( P345_IRON_FROM );
	else
		SendWeaponAnim( P345_IRON_FROM_NOSHOT );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CP345::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CP345::SuicideShot );

	if ( m_iClip )
		SendWeaponAnim( P345_SUICIDE );
	else
		SendWeaponAnim( P345_SUICIDE_NOSHOT );

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

void CP345::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/p345/p345_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CP345::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	if ( m_iClip )
		SendWeaponAnim( P345_DRAW );
	else
		SendWeaponAnim( P345_DRAW_NOSHOT );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CP345::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_p345" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/p345/w_p345.mdl" );

	m_iId = 7;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;
		m_iDefaultAmmo = 8;
	}

	m_flKickPerShot = 0.85;
	m_iSprintIdleAnim1 = P345_SPRINT_IDLE;
	m_iSprintIdleAnim2 = P345_SPRINT_IDLE_NOSHOT;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_p345.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;
	m_bCanSuicide = true;
	strwmodel = MAKE_STRING( "models/weapons/p345/w_p345.mdl" );

	FallInit();// get ready to fall down.
}

void CP345::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/p345/v_p345.mdl" );
	PRECACHE_MODEL( "models/weapons/p345/w_p345.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/p345/p_p345.mdl" );

	PRECACHE_SOUND( "weapons/p345/p345_click.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_magin.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_magout.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_magplace.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_rackback.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_slide.wav" );
	PRECACHE_SOUND( "weapons/p345/p345_fire.wav" );
}

int CP345::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "p345";
	p->iMaxAmmo1 = 80;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 8;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 7;
	p->iWeight = 5;

	return 1;
}

BOOL CP345::Deploy( )
{
	m_iSprintState = 0;
	m_iSuicideState = ( m_iSuicideState == 666 );

	if ( m_iClip == -666 )
	{
		m_iClip = 0;
	}
	else if ( m_iClip )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/p345/v_p345.mdl", "police/models/weapons/p345/p_p345.mdl", P345_DRAW, "onehanded" );

		return DefaultDeploy( "models/weapons/p345/v_p345.mdl", "models/null.mdl", P345_DRAW, "onehanded" );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/p345/v_p345.mdl", "police/models/weapons/p345/p_p345.mdl", P345_DRAW_NOSHOT, "onehanded" );

	return DefaultDeploy( "models/weapons/p345/v_p345.mdl", "models/null.mdl", P345_DRAW_NOSHOT, "onehanded" );
}

void CP345::Holster( )
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
		MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 1 );
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
		SendWeaponAnim( P345_HOLSTER );
	else
		SendWeaponAnim( P345_HOLSTER_NOSHOT );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CP345::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState || m_bIronSights )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );
	m_pPlayer->PlayerAnimation( "pistol_melee", false );

	if ( m_iClip )
		SendWeaponAnim( P345_MELEE );
	else
		SendWeaponAnim( P345_MELEE_NOSHOT );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CP345::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) || m_iSprintState || m_iSuicideState )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( gpGlobals->time > m_fEmptyClickTime )
		{
			if ( m_bIronSights )
				SendWeaponAnim( P345_IRON_SHOOT_NOSHOT );
			else
				SendWeaponAnim( P345_SHOOT_NOSHOT );

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
		else if ( gpGlobals->time > m_fEmptyClickTime )
		{
			if ( m_bIronSights )
				SendWeaponAnim( P345_IRON_SHOOT_NOSHOT );
			else
				SendWeaponAnim( P345_SHOOT_NOSHOT );

			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.5;
		}
		return;
	}

	Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
	Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
	EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( ENTINDEX( edict() ) );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/p345/v_p345.mdl" );
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
	m_pPlayer->ViewPunch( RANDOM_LONG( -4, 4 ), 3, 3 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 4.0, 0.2, 0.15, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.35, 0.15, 2.0 );

	if ( m_bIronSights )
	{
		if ( !m_iClip )
			SendWeaponAnim( P345_IRON_SHOOT_EMPTY );
		else
			SendWeaponAnim( P345_IRON_SHOOT );
	}
	else
	{
		if ( !m_iClip )
			SendWeaponAnim( P345_SHOOT_EMPTY_1 );
		else
			SendWeaponAnim( P345_SHOOT_1 );
	}

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.75;

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgP345, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 3.0 );

	PlayWeaponSound_guess( "weapons_new/p345/p345_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	m_flNextPrimaryAttack = gpGlobals->time + 0.2;
	m_flNextSecondaryAttack = gpGlobals->time + 0.2;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CP345::Reload( void )
{
	int j = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

	if ( j <= 0 || m_flNextPrimaryAttack > gpGlobals->time || m_iSprintState == 1 || m_iSprintState > 2 || m_iSuicideState || m_bWaitingToReloadIronsights )
		return;

	if ( m_bIronSights )
	{
		m_bWaitingToReloadIronsights = true;
		return;
	}

	if ( 8 - m_iClip >= j || 8 - m_iClip != 0 )
	{
		m_iSprintState = 2;

		if ( m_iClip )
		{
			m_iClip = 0;
			DefaultReload( 8, P345_RELOAD, 2.73, 0, false );
			m_pPlayer->PlayerAnimation( "p345_reload", false );
		}
		else
		{
			m_iClip = 0;
			DefaultReload( 8, P345_RELOAD_NOSHOT, 3.16, 0, false );
			m_pPlayer->PlayerAnimation( "p345_reloadempty", false );
		}
	}
}

void CP345::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time || m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
	{
		if ( m_iClip )
			SendWeaponAnim( P345_IRON_IDLE );
		else
			SendWeaponAnim( P345_IRON_IDLE_NOSHOT );
	}
	else
	{
		if ( m_iClip )
			SendWeaponAnim( P345_IDLETEST );
		else
			SendWeaponAnim( P345_IDLE1_NOSHOT );
	}

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CP345::TertiaryAttack( void )
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

void CP345::ItemPostFrame( void )
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
				SendWeaponAnim( P345_SPRINT_FROM );
			else
				SendWeaponAnim( P345_SPRINT_FROM_NOSHOT );
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
					SendWeaponAnim( P345_SPRINT_TO );
				else
					SendWeaponAnim( P345_SPRINT_TO_NOSHOT );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			if ( m_iClip )
				SendWeaponAnim( P345_SPRINT_FROM );
			else
				SendWeaponAnim( P345_SPRINT_FROM_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 && m_iSprintState != 1 )
		{
			m_iSprintState = 1;

			if ( m_iClip )
				SendWeaponAnim( P345_SPRINT_TO );
			else
				SendWeaponAnim( P345_SPRINT_TO_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
			m_iNextSprintAnim = time + gpGlobals->time;
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 && m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();
			if ( speed != 0 )
			{
				if ( m_iClip )
					SendWeaponAnim( P345_SPRINT_IDLE );
				else
					SendWeaponAnim( P345_SPRINT_IDLE_NOSHOT );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 && m_iSprintState == 1 )
		{
			if ( m_iClip )
				SendWeaponAnim( P345_SPRINT_FROM );
			else
				SendWeaponAnim( P345_SPRINT_FROM_NOSHOT );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CP345Ammo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/p345_clip.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/p345_clip.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 8, "p345", 24, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( 8, "p345", 80, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_p345, CP345Ammo );
