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
extern int gmsgAmmoX;
extern unsigned short m_usThirdFlash;
extern unsigned short m_usRevolverShells;
extern int g_iDoctorMode;

// sequences of models/weapons/revolver/v_revolver.mdl
enum revolver_e
{
	REVOLVER_IDLE1 = 0,
	REVOLVER_DRAW,
	REVOLVER_HOLSTER,
	REVOLVER_SHOOT1,
	REVOLVER_RELOAD_START,
	REVOLVER_RELOAD_INSERT1,
	REVOLVER_RELOAD_INSERT2,
	REVOLVER_RELOAD_INSERT3,
	REVOLVER_RELOAD_INSERT4,
	REVOLVER_RELOAD_INSERT5,
	REVOLVER_RELOAD_END,
	REVOLVER_SHOOT_NOSHOT,
	REVOLVER_SPRINT_TO,
	REVOLVER_SPRINT_IDLE,
	REVOLVER_SPRINT_FROM,
	REVOLVER_IRON_TO,
	REVOLVER_IRON_IDLE,
	REVOLVER_IRON_FROM,
	REVOLVER_IRON_SHOOT,
	REVOLVER_IRON_SHOOT_EMPTY,
	REVOLVER_MELEE,
	REVOLVER_SUICIDE,
};

LINK_ENTITY_TO_CLASS( weapon_revolver, CRevolver );


void CRevolver::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CRevolver::SuicideShot );

	SendWeaponAnim( REVOLVER_SUICIDE );

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

void CRevolver::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/revolver/revolver_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CRevolver::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	SendWeaponAnim( REVOLVER_DRAW );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CRevolver::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_revolver" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/revolver/w_revolver.mdl" );

	m_iId = 21;

	m_iSprintIdleAnim1 = 0;
	m_iSprintIdleAnim2 = 0;

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

	m_flKickPerShot = 3.15;

	pev->body = m_iClip;

	m_bCanDualWield = true;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_revolver.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/revolver/w_revolver.mdl" );

	FallInit();// get ready to fall down.
}

void CRevolver::Precache( void )
{
	if ( g_iDoctorMode )
		PRECACHE_MODEL( "models/weapons/revolver/v_revolver_doctor.mdl" );
	else
		PRECACHE_MODEL( "models/weapons/revolver/v_revolver.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/revolver/p_revolver.mdl" );

	PRECACHE_MODEL( "models/weapons/revolver/w_revolver.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "weapons/revolver/revolver_fire.wav" );
}

int CRevolver::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "revolver";
	p->iMaxAmmo1 = 50;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 5;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 21;
	p->iWeight = 5;

	return 1;
}

void CRevolver::ResetCylinder( void )
{
	if ( !m_pPlayer->m_bInfiniteAmmo )
	{
		m_iRotation = 0;
		m_pPlayer->pev->vuser4.z = 0;
	}

	SetThink( &CRevolver::ReloadInsert );
	SetNextThink( 1.3 );
}

void CRevolver::SetCylinder( void )
{
	if ( !m_pPlayer->m_bInfiniteAmmo )
	{
		m_iRotation = m_iClip * 72;
		m_pPlayer->pev->vuser4.z = m_iRotation;
	}
}

BOOL CRevolver::Deploy( )
{
	m_iSprintState = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	m_bReloading = m_bWantToEndReload = false;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		pev->body = m_iClip;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/revolver/v_revolver.mdl", "police/models/weapons/revolver/p_revolver.mdl", REVOLVER_DRAW, "onehanded" );

	if ( g_iDoctorMode )
		return DefaultDeploy( "models/weapons/revolver/v_revolver_doctor.mdl", "models/null.mdl", REVOLVER_DRAW, "onehanded" );

	return DefaultDeploy( "models/weapons/revolver/v_revolver.mdl", "models/null.mdl", REVOLVER_DRAW, "onehanded" );
}

void CRevolver::Holster( )
{
	SendWeaponAnim( REVOLVER_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	SetThink( NULL );

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

void CRevolver::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_bReloading )
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
			m_fEmptyClickTime = gpGlobals->time + 1;
		}

		m_flNextPrimaryAttack = gpGlobals->time + 1;
		m_flNextSecondaryAttack = gpGlobals->time + 1;

		if ( m_bIronSights )
			SendWeaponAnim( REVOLVER_IRON_SHOOT_EMPTY );
		else
			SendWeaponAnim( REVOLVER_SHOOT_NOSHOT );

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
				SendWeaponAnim( REVOLVER_IRON_SHOOT_EMPTY );
			else
				SendWeaponAnim( REVOLVER_SHOOT_NOSHOT );

			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 1;
			}
		}

		m_flNextPrimaryAttack = gpGlobals->time + 1;
		m_flNextSecondaryAttack = gpGlobals->time + 1;
		return;
	}

	pev->iuser3 = 0;

	if ( g_iDoctorMode )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "models/weapons/revolver/v_revolver_doctor.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "models/weapons/revolver/v_revolver.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iRotation = m_iClip * 72;

	PLAYBACK_EVENT_FULL( 0, ENT( m_pPlayer->pev ), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, (float *)&g_vecZero, 0.0, 0.0, m_iId, 0, 0, 0 );

	m_pPlayer->ViewPunch( RANDOM_LONG( -2, 2 ), 1, 1 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	if ( m_bIronSights )
		SendWeaponAnim( REVOLVER_IRON_SHOOT );
	else
		SendWeaponAnim( REVOLVER_SHOOT1 );

	m_pPlayer->PlayerAnimation( "pistol_shoot", false );

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

	if ( m_pPlayer && m_pPlayer->playerstats.doctor_mode )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, 35, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, gSkillData.DmgRevolver, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 4.0 );

	PlayWeaponSound_guess( "weapons_new/revolver/revolver_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
	m_flNextPrimaryAttack = gpGlobals->time + 0.65;
	m_flNextSecondaryAttack = gpGlobals->time + 0.65;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CRevolver::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( m_iSuicideState )
		return;

	int j = min( 5 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	if ( j == 0 )
		return;

	if ( m_bReloading )
		return;

	if ( m_bWaitingToReloadIronsights )
		return;

	if ( m_bIronSights )
	{
		m_bWaitingToReloadIronsights = true;
		return;
	}

	m_fReloadSpeed = 0;

	m_iSprintState = 2;

	if ( pev->body )
	{
		for ( int i = 1; i <= pev->body; i++ )
			PLAYBACK_EVENT_FULL( 0, ENT( m_pPlayer->pev ), m_usRevolverShells, i * 0.05 + 1.2, m_pPlayer->pev->origin, (float *)&g_vecZero, 0.0, 0.0, m_iClip, 0, 0, 0 );
	}

	m_iClip = 0;
	SendWeaponAnim( REVOLVER_RELOAD_START );

	m_pPlayer->PlayerAnimation( "revolver_reload_start", false );
	m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 2;

	SetThink( &CRevolver::ResetCylinder );
	SetNextThink( 0.2 );

	m_bReloading = true;
}

void CRevolver::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );

	m_pPlayer->PlayerAnimation( "pistol_melee", false );

	SendWeaponAnim( REVOLVER_MELEE );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CRevolver::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bIronSights )
		SendWeaponAnim( REVOLVER_IRON_IDLE );
	else
		SendWeaponAnim( REVOLVER_IDLE1 );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CRevolver::ReloadInsert( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
	{
		m_bReloading = m_bWantToEndReload = false;
		m_iSprintState = 0;
		SendWeaponAnim( REVOLVER_RELOAD_END );

		m_pPlayer->PlayerAnimation( "revolver_reload_end", false );

		SetThink( &CRevolver::SetCylinder );
		SetNextThink( 0.3 );

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		return;
	}

	if ( m_bWantToEndReload && m_iClip > 0 )
	{
		m_bReloading = m_bWantToEndReload = false;
		m_iSprintState = 0;
		SendWeaponAnim( REVOLVER_RELOAD_END );

		m_pPlayer->PlayerAnimation( "revolver_reload_end", false );

		SetThink( &CRevolver::SetCylinder );
		SetNextThink( 0.3 );

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		return;
	}

	if ( m_iClip < 5 )
	{
		m_flInsertTime = gpGlobals->time + 0.5;
		m_bInsertBullet = true;
	}

	switch ( m_iClip )
	{
	case 0:
		pev->body = 1;
		SendWeaponAnim( REVOLVER_RELOAD_INSERT1 );
		break;

	case 1:
		pev->body = 2;
		SendWeaponAnim( REVOLVER_RELOAD_INSERT2 );
		break;

	case 2:
		pev->body = 3;
		SendWeaponAnim( REVOLVER_RELOAD_INSERT3 );
		break;

	case 3:
		pev->body = 4;
		SendWeaponAnim( REVOLVER_RELOAD_INSERT4 );
		break;

	case 4:
		pev->body = 5;
		SendWeaponAnim( REVOLVER_RELOAD_INSERT5 );
		break;
	}

	m_pPlayer->PlayerAnimation( "revolver_reload_insert", false );
	m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 1;

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.83;

	if ( m_iClip >= 5 )
	{
		m_bReloading = m_bWantToEndReload = false;
		m_iSprintState = 0;
		SendWeaponAnim( REVOLVER_RELOAD_END );
		m_pPlayer->PlayerAnimation( "revolver_reload_end", false );
		SetThink( NULL );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.83;
		return;
	}

	SetNextThink( 0.83 );
}

void CRevolver::IronsightsTo( void )
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

	SendWeaponAnim( REVOLVER_IRON_TO );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.3;
}

void CRevolver::IronsightsFrom( void )
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

	SendWeaponAnim( REVOLVER_IRON_FROM );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.3;
}

void CRevolver::TertiaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_CANCEL ) )
		return;

	if ( CVAR_GET_FLOAT( "ironsights_toggle" ) == 0 && m_bIronSights )
		return;

	if ( m_bReloading )
		return;

	m_bIronSights = !m_bIronSights;

	if ( m_bIronSights )
		IronsightsTo();
	else
		IronsightsFrom();
}

void CRevolver::ItemPostFrame( void )
{
	if ( m_bReloading && !m_bWantToEndReload && ( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
		m_bWantToEndReload = true;

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

			SendWeaponAnim( REVOLVER_SPRINT_FROM );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
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

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() && !m_bIronSights && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				SendWeaponAnim( REVOLVER_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

			SendWeaponAnim( REVOLVER_SPRINT_FROM );

			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;
				SendWeaponAnim( REVOLVER_SPRINT_TO );

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
				SendWeaponAnim( REVOLVER_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( REVOLVER_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	if ( m_bInsertBullet && m_flInsertTime < gpGlobals->time )
	{
		m_bInsertBullet = false;

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] && m_iClip < 5 )
		{
			// Add them to the clip
			m_iClip++;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			m_pPlayer->TabulateAmmo();

			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, m_pPlayer->pev );
				WRITE_BYTE( m_iPrimaryAmmoType );
				WRITE_SHORT( max( min( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType], 254 ), 0 ) );
			MESSAGE_END();
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CRevolverAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/ammo_revolver.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/ammo_revolver.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 5, "revolver", 15, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( 5, "revolver", 50, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_revolver, CRevolverAmmo );
