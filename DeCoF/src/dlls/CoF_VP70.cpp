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

// sequences of models/weapons/vp70/v_vp70.mdl
enum vp70_e
{
	VP70_IDLE = 0,
	VP70_DRAW,
	VP70_DRAW_FIRST,
	VP70_HOLSTER,
	VP70_SHOOT,
	VP70_SHOOT_NOSHOT,
	VP70_RELOAD,
	VP70_RELOAD_NOSHOT,
	VP70_IRON_TO,
	VP70_IRON_IDLE,
	VP70_IRON_SHOOT,
	VP70_IRON_SHOOT_NOSHOT,
	VP70_IRON_FROM,
	VP70_SPRINT_TO,
	VP70_SPRINT_IDLE,
	VP70_SPRINT_FROM,
	VP70_JUMP_TO,
	VP70_JUMP_FROM,
	VP70_FIDGET1,
	VP70_FIDGET2,
	VP70_FIDGET3,
	VP70_MELEE,
	VP70_SUICIDE,
};

LINK_ENTITY_TO_CLASS( weapon_vp70, CVP70 );


void CVP70::Fidget( void )
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
			SendWeaponAnim( VP70_FIDGET1 );
		break;

	case 1:
		if ( m_iClip )
			SendWeaponAnim( VP70_FIDGET2 );
		break;

	case 2:
		if ( m_iClip )
			SendWeaponAnim( VP70_FIDGET3 );
		break;
	}
}

void CVP70::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CVP70::SuicideShot );

	SendWeaponAnim( VP70_SUICIDE );

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

void CVP70::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		int pitch = 95 + RANDOM_LONG( 0, 10 );
		PlayWeaponSound_guess( "weapons/vp70/vp70_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CVP70::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	SendWeaponAnim( VP70_DRAW );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CVP70::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_vp70" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/vp70/w_vp70.mdl" );

	m_iId = 19;
	m_iSprintIdleAnim1 = VP70_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;
		m_iDefaultAmmo = 18;
	}

	m_flKickPerShot = 0.75;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_vp70.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;
	m_bCanSuicide = true;
	strwmodel = MAKE_STRING( "models/weapons/vp70/w_vp70.mdl" );

	FallInit();// get ready to fall down.
}

void CVP70::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/vp70/v_vp70.mdl" );
	PRECACHE_MODEL( "models/weapons/vp70/w_vp70.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/vp70/p_vp70.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/vp70/vp70_cock.wav" );
	PRECACHE_SOUND( "weapons/vp70/vp70_magin.wav" );
	PRECACHE_SOUND( "weapons/vp70/vp70_magout.wav" );
	PRECACHE_SOUND( "weapons/vp70/vp70_draw_first_seq.wav" );
	PRECACHE_SOUND( "weapons/vp70/vp70_fire.wav" );
}

int CVP70::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "vp70";
	p->iMaxAmmo1 = 180;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 18;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 19;
	p->iWeight = 5;

	return 1;
}

BOOL CVP70::Deploy( )
{
	m_iSprintState = 0;
	shoot_num = 0;
	m_iSuicideState = ( m_iSuicideState == 666 );

	if ( m_iClip == -666 )
		m_iClip = 0;

	m_bIronSights = false;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			return DefaultDeploy( "models/weapons/vp70/v_vp70.mdl", "police/models/weapons/vp70/p_vp70.mdl", VP70_DRAW_FIRST, "mp5" );

		return DefaultDeploy( "models/weapons/vp70/v_vp70.mdl", "models/null.mdl", VP70_DRAW_FIRST, "mp5" );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/vp70/v_vp70.mdl", "police/models/weapons/vp70/p_vp70.mdl", VP70_DRAW, "mp5" );

	return DefaultDeploy( "models/weapons/vp70/v_vp70.mdl", "models/null.mdl", VP70_DRAW, "mp5" );
}

void CVP70::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState || m_bIronSights )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );
	m_pPlayer->PlayerAnimation( "vp70_melee", false );

	SendWeaponAnim( VP70_MELEE );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CVP70::Holster( )
{
	SendWeaponAnim( VP70_HOLSTER );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CVP70::Shoot( void )
{
	if ( m_iSprintState || m_iSuicideState )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( gpGlobals->time > m_fEmptyClickTime )
		{
			if ( m_bIronSights )
				SendWeaponAnim( VP70_IRON_SHOOT_NOSHOT );
			else
				SendWeaponAnim( VP70_SHOOT_NOSHOT );

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
		else if ( gpGlobals->time > m_fEmptyClickTime )
		{
			if ( m_bIronSights )
				SendWeaponAnim( VP70_IRON_SHOOT_NOSHOT );
			else
				SendWeaponAnim( VP70_SHOOT_NOSHOT );

			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.25;
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
		WRITE_STRING( "models/weapons/vp70/v_vp70.mdl" );
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

	m_pPlayer->PlayerAnimation( "vp70_shoot", false );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0: m_pPlayer->ViewPunch( 1, 1, 1 ); break;
	case 1: m_pPlayer->ViewPunch( -1, 1, 1 ); break;
	case 2: m_pPlayer->ViewPunch( 1, 1, -1 ); break;
	}

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	if ( m_bIronSights )
		SendWeaponAnim( VP70_IRON_SHOOT );
	else
		SendWeaponAnim( VP70_SHOOT );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.35;

	Vector vecDir;
	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgVP70, m_pPlayer->pev, m_pPlayer->random_seed );

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/vp70/vp70_fire.wav", 1, ATTN_NORM );

	m_pPlayer->ViewKnock( 3.0 );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	shoot_num++;
	if ( shoot_num < 3 )
	{
		SetNextThink( 0.027 );
		SetThink( &CVP70::Shoot );
	}
}

void CVP70::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( shoot_num >= 3 )
		shoot_num = 0;

	if ( m_iClip <= 0 )
	{
		if ( !m_fFireOnEmpty )
		{
			Reload( );
			return;
		}

		if ( m_bIronSights )
			SendWeaponAnim( VP70_IRON_SHOOT_NOSHOT );
		else
			SendWeaponAnim( VP70_SHOOT_NOSHOT );

		PlayEmptySound( );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 0.8;
		m_flTimeWeaponIdle = gpGlobals->time + 0.8;
		return;
	}

	int flags;
	if ( m_pPlayer->m_iViewValue != 1 )
		flags = FEV_NOTHOST;
	else
		flags = 0;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecShellSrc + gpGlobals->v_right * 32;

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 );

	Shoot();

	m_flNextPrimaryAttack = gpGlobals->time + 0.55;
	m_flNextSecondaryAttack = gpGlobals->time + 0.55;
	m_flNextTertiaryAttack = gpGlobals->time + 0.55;
	m_flTimeWeaponIdle = gpGlobals->time + 0.6;
}

void CVP70::TertiaryAttack( void )
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

void CVP70::IronsightsTo( void )
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

	SendWeaponAnim( VP70_IRON_TO );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CVP70::IronsightsFrom( void )
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

	SendWeaponAnim( VP70_IRON_FROM );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CVP70::Reload( void )
{
	int j = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

	if ( j <= 0 || m_iSprintState == 1 || m_iSprintState > 2 || m_iSuicideState || m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( ( 18 - m_iClip >= j || 18 - m_iClip != 0 ) && !m_bWaitingToReloadIronsights )
	{
		if ( m_bIronSights )
		{
			m_bWaitingToReloadIronsights = true;
			return;
		}

		m_iSprintState = 2;
		shoot_num = 0;

		if ( m_iClip )
		{
			m_pPlayer->PlayerAnimation( "vp70_reload", false );
			m_iClip = 0;
			DefaultReload( 18, VP70_RELOAD, GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), VP70_RELOAD ), 0, false );
		}
		else
		{
			m_pPlayer->PlayerAnimation( "vp70_reload_empty", false );
			m_iClip = 0;
			DefaultReload( 18, VP70_RELOAD_NOSHOT, GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), VP70_RELOAD_NOSHOT ), 0, false );
		}
	}
}

void CVP70::WeaponIdle( void )
{
}

void CVP70::ItemPostFrame( void )
{
	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

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

			SendWeaponAnim( VP70_SPRINT_FROM );
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

				SendWeaponAnim( VP70_JUMP_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( VP70_JUMP_FROM );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 && m_iSprintState != 1 )
		{
			m_iSprintState = 1;

			SendWeaponAnim( VP70_SPRINT_TO );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = time * 0.5 + gpGlobals->time;
			m_iNextSprintAnim = time + gpGlobals->time;
			m_flTimeWeaponIdle = time / 2 + gpGlobals->time;
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 && m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();
			if ( speed != 0 )
			{
				SendWeaponAnim( VP70_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 && m_iSprintState == 1 )
		{
			SendWeaponAnim( VP70_SPRINT_FROM );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CVP70Ammo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/vp70_clip.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/vp70_clip.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 18, "vp70", 54, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( 18, "vp70", 180, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_vp70, CVP70Ammo );
