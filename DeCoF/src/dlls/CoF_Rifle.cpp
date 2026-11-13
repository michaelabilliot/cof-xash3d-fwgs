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
extern int gmsgHoldBreath;
extern unsigned short m_usThirdFlash;
extern bool m_gbIsInCutscene;

// sequences of models/weapons/rifle/v_rifle.mdl
enum rifle_e
{
	RIFLE_IDLE = 0,
	RIFLE_DRAW,
	RIFLE_HOLSTER,
	RIFLE_SHOOT1,
	RIFLE_RELOAD_START,
	RIFLE_RELOAD_INSERT,
	RIFLE_RELOAD_END,
	RIFLE_SCOPE_TO,
	RIFLE_SCOPE_FROM,
	RIFLE_SPRINT_TO,
	RIFLE_SPRINT_IDLE,
	RIFLE_SPRINT_FROM,
	RIFLE_MELEE,
	RIFLE_ALIGN,
	RIFLE_FIDGET1,
	RIFLE_FIDGET2,
	RIFLE_FIDGET3,
	RIFLE_SUICIDE,
	RIFLE_SHOOT_NOSHOT,
	RIFLE_DRAW_FIRST,
	RIFLE_JUMP_TO,
	RIFLE_JUMP_FROM,
};

LINK_ENTITY_TO_CLASS( weapon_rifle, CRifle );


void CRifle::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	if ( m_bIronSights || m_bRaised )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		SendWeaponAnim( RIFLE_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( RIFLE_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( RIFLE_FIDGET3 );
		break;
	}
}

void CRifle::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CRifle::SuicideShot );

	SendWeaponAnim( RIFLE_SUICIDE );

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

void CRifle::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/rifle/rifle_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CRifle::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	SendWeaponAnim( RIFLE_DRAW );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CRifle::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_rifle" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/rifle/w_rifle.mdl" );

	m_iId = 11;

	m_iSprintIdleAnim1 = 1;
	m_iSprintIdleAnim2 = 2;

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

	m_flKickPerShot = 5.0;

	m_bCanSuicide = true;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_rifle.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/rifle/w_rifle.mdl" );

	FallInit();// get ready to fall down.
}

void CRifle::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/rifle/v_rifle.mdl" );
	PRECACHE_MODEL( "models/weapons/rifle/w_rifle.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	m_iShell = PRECACHE_MODEL( "models/556shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );

	PRECACHE_SOUND( "player/vocal/breathing/hold_breath.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/release_breath.wav" );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/rifle/p_rifle.mdl" );

	PRECACHE_MODEL( "models/weapons/rifle/v_rifle_scope.mdl" );

	PRECACHE_SOUND( "weapons/rifle/rifle_cock_back.wav" );
	PRECACHE_SOUND( "weapons/rifle/rifle_cock_forward.wav" );
	PRECACHE_SOUND( "weapons/rifle/rifle_insert.wav" );

	PRECACHE_SOUND( "weapons/rifle/rifle_fire.wav" );
}

int CRifle::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "rifle";
	p->iMaxAmmo1 = 50;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 5;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 11;
	p->iWeight = 5;

	return 1;
}

BOOL CRifle::Deploy( )
{
	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	m_bWaitingToReload = false;
	m_iSprintState = 0;
	m_bWantToEndReload = false;

	m_pPlayer->ShowHint( MAKE_STRING( "weaponfunctionsniperbreathe" ), false );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/rifle/v_rifle.mdl", "police/models/weapons/rifle/p_rifle.mdl", RIFLE_DRAW, "shotgun" );

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		return DefaultDeploy( "models/weapons/rifle/v_rifle.mdl", "models/null.mdl", RIFLE_DRAW_FIRST, "shotgun" );
	}

	return DefaultDeploy( "models/weapons/rifle/v_rifle.mdl", "models/null.mdl", RIFLE_DRAW, "shotgun" );
}

void CRifle::Holster( )
{
	m_bWaitingToReload = false;
	SendWeaponAnim( RIFLE_HOLSTER );
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

	if ( m_bRaised )
	{
		m_bRaised = false;
		PostZoom();

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 666 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		if ( m_pPlayer->m_bHoldingBreath )
		{
			m_pPlayer->m_bHoldingBreath = false;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM );
		}

		m_pPlayer->pev->vuser2.x = 90;
	}
}

void CRifle::Zoom( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
		WRITE_COORD( 400 );
		WRITE_BYTE( 1 );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	m_pPlayer->pev->vuser2.x = 85;

	SetNextThink( 0.15 );
	SetThink( &CRifle::PostZoom );
}

void CRifle::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bRaised )
		return;

	if ( m_pPlayer->m_fStamina <= 0 )
		return;

	m_pPlayer->SetNextThink( 0.23 );
	m_pPlayer->SetThink( &CBasePlayer::MeleePushBack );
	SendWeaponAnim( RIFLE_MELEE );

	m_pPlayer->PlayerAnimation( "rifle_melee", false );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CRifle::TertiaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !m_bRaised )
	{
		m_bRaised = true;
		SendWeaponAnim( RIFLE_SCOPE_TO );

		SetNextThink( 0.1 );
		SetThink( &CRifle::Zoom );

		UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 0.1, 1, 255, 5 );
	}
	else
	{
		m_bRaised = false;
		SendWeaponAnim( RIFLE_SCOPE_FROM );
		PostZoom();

		if ( m_pPlayer->m_bHoldingBreath )
		{
			m_pPlayer->m_bHoldingBreath = false;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM );
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 666 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_pPlayer->pev->vuser2.x = 90;
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.25;
}

void CRifle::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			SendWeaponAnim( RIFLE_SHOOT_NOSHOT );
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
				SendWeaponAnim( RIFLE_SHOOT_NOSHOT );
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
		}
		return;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/gunsmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/rifle/v_rifle.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_iClip--;

	int flags = 0;

	if ( m_pPlayer->m_iViewValue != 1 )
		flags = FEV_NOTHOST;

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecShellSrc + gpGlobals->v_right * 32;
	PLAYBACK_EVENT_FULL( flags, ENT( m_pPlayer->pev ), m_usThirdFlash, 0.0, m_pPlayer->pev->origin, vecShellEnd, 0, 0, m_iId, 0, 0, 0 );

	m_pPlayer->PlayerAnimation( "rifle_shoot", false );

	if ( !m_bRaised )
		m_pPlayer->ViewPunch( RANDOM_LONG( -9, 9 ), 4, 4 );
	else
		m_pPlayer->ViewPunch( RANDOM_LONG( -4, 4 ), 3, 2 );

	if ( !m_bRaised )
	{
		if ( m_pPlayer->pev->flags & FL_DUCKING )
			UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.3, 0.2, 4.0 );
		else
			UTIL_ScreenShake( m_pPlayer->pev->origin, 7.0, 0.4, 0.3, 6.0 );
	}
	else
	{
		if ( m_pPlayer->pev->flags & FL_DUCKING )
			UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
		else
			UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );
	}

	SendWeaponAnim( RIFLE_SHOOT1 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 1.8;

	if ( !m_bRaised )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_RIFLE, 2, gSkillData.DmgRifle, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, VECTOR_CONE_1DEGREES, 8192, BULLET_PLAYER_RIFLE, 2, gSkillData.DmgRifle, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 0.58;

	if ( !m_bRaised )
		m_pPlayer->ViewKnock( 5.0 );
	else
		m_pPlayer->ViewKnock( 3.0 );

	PlayWeaponSound_guess( "weapons_new/rifle/rifle_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	if ( m_bRaised )
	{
		m_iWantToPullBolt = 1;
		m_flNextBoltPullTime = gpGlobals->time + 0.66;
	}

	m_iWantToEjectShell = 1;
	m_flNextShellEjectTime = gpGlobals->time + 0.76;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
	m_flNextPrimaryAttack = gpGlobals->time + 1.66;
	m_flNextSecondaryAttack = gpGlobals->time + 1.66;
	m_flNextTertiaryAttack = gpGlobals->time + 1.66;
	m_flTimeWeaponIdle = gpGlobals->time + 1.66;
}

void CRifle::RifleEjectBrass( void )
{
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecShellEnd = vecShellSrc + gpGlobals->v_right * 32;

	if ( ( g_pGameRules && !g_pGameRules->IsMultiplayer() ) || m_pPlayer->m_iViewValue != 1 )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 60 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 60 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}
}

void CRifle::Reload( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( m_bRaised )
	{
		m_bRaised = false;
		SendWeaponAnim( RIFLE_SCOPE_FROM );
		PostZoom();

		if ( m_pPlayer->m_bHoldingBreath )
		{
			m_pPlayer->m_bHoldingBreath = false;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM );
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 666 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_bWaitingToReload = true;

		m_pPlayer->pev->vuser2.x = 90;
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
		return;
	}

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	int j = min( 5 - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );

	if ( j == 0 )
		return;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0 )
		return;

	if ( m_bWaitingToReload )
		m_bWaitingToReload = false;

	if ( m_iClip )
	{
		m_iWantToEjectShell = 2;
		m_flNextShellEjectTime = gpGlobals->time + 0.4;
	}

	m_fReloadSpeed = 0;

	m_iSprintState = 2;

	m_bRifleReload = true;
	SendWeaponAnim( RIFLE_RELOAD_START );
	m_flNextPrimaryAttack = gpGlobals->time + 1;
	m_flNextSecondaryAttack = gpGlobals->time + 1;
	m_flNextTertiaryAttack = gpGlobals->time + 1;
	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->PlayerAnimation( "rifle_reload_start", false );
	m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 1.5;

	SetNextThink( 0.66 );
	SetThink( &CRifle::DoReload );
}

void CRifle::DoReload( void )
{
	if ( m_iClip >= 5 || m_bWantToEndReload || !m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		SendWeaponAnim( RIFLE_RELOAD_END );
		m_flNextPrimaryAttack = gpGlobals->time + 0.66;
		m_flNextSecondaryAttack = gpGlobals->time + 0.66;
		m_flNextTertiaryAttack = gpGlobals->time + 0.66;
		m_flTimeWeaponIdle = gpGlobals->time + 0.66;
		m_bRifleReload = false;
		m_bWantToEndReload = false;
		m_pPlayer->PlayerAnimation( "rifle_reload_end", false );
	}
	else
	{
		m_pPlayer->PlayerAnimation( "rifle_reload_insert", false );
		m_pPlayer->m_flSequenceOverrideTime = gpGlobals->time + 1.5;

		SendWeaponAnim( RIFLE_RELOAD_INSERT );
		m_flNextPrimaryAttack = gpGlobals->time + 0.83;
		m_flNextSecondaryAttack = gpGlobals->time + 0.83;
		m_flNextTertiaryAttack = gpGlobals->time + 0.83;
		m_flTimeWeaponIdle = gpGlobals->time + 0.83;

		SetNextThink( 0.3 );
		SetThink( &CRifle::InsertBullet );
	}
}

void CRifle::InsertBullet( void )
{
	m_iClip++;
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->TabulateAmmo();

	int pitch = 95 + RANDOM_LONG( 0, 10 );
	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/rifle/rifle_insert.wav", 1, ATTN_NORM, 0, pitch );

	SetNextThink( 0.5 );
	SetThink( &CRifle::DoReload );
}

void CRifle::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	SendWeaponAnim( RIFLE_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CRifle::PostZoom( void )
{
	if ( m_bRaised )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 666 );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_BYTE( 30 );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		m_pPlayer->pev->vuser2.x = 30;

		m_pPlayer->pev->viewmodel = MAKE_STRING( "models/weapons/rifle/v_rifle_scope.mdl" );
		UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 0.2, 0, 255, 0 );
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgIronSights, NULL, m_pPlayer->pev );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_COORD( 0 );
			WRITE_BYTE( 30 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 0.2, 0, 255, 0 );

		if ( custom_v_model )
			m_pPlayer->pev->viewmodel = custom_v_model;
		else
			m_pPlayer->pev->viewmodel = MAKE_STRING( "models/weapons/rifle/v_rifle.mdl" );
	}
}

void CRifle::ItemPostFrame( void )
{
	if ( m_pPlayer && m_bRaised && ( m_pPlayer->pev->button & IN_RUN ) && m_pPlayer->m_fStamina >= 1 )
	{
		if ( !m_pPlayer->m_bHoldingBreath && m_pPlayer->m_flNextBreathHoldTime < gpGlobals->time )
		{
			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, m_pPlayer->pev );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			m_pPlayer->m_bHoldingBreath = true;

			m_pPlayer->m_flNextBreathHoldTime = gpGlobals->time + 1;

			EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "player/vocal/breathing/hold_breath.wav", 1, ATTN_NORM );
		}
	}

	if ( m_pPlayer && !m_bRaised )
	{
		if ( m_iWantToPullBolt > 0 )
			m_iWantToPullBolt = 0;
	}

	if ( m_pPlayer && m_bRaised )
	{
		if ( m_iWantToPullBolt == 1 && m_flNextBoltPullTime < gpGlobals->time )
		{
			m_iWantToPullBolt = 2;
			m_flNextBoltPullTime = gpGlobals->time + 0.33;
			EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/rifle/rifle_cock_back.wav", 1, ATTN_NORM );
			m_pPlayer->ViewPunch( 0, 0, 15 );
		}

		if ( m_iWantToPullBolt == 2 && m_flNextBoltPullTime < gpGlobals->time )
		{
			m_iWantToPullBolt = 0;
			EMIT_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/rifle/rifle_cock_forward.wav", 1, ATTN_NORM );
			m_pPlayer->ViewPunch( 0, 0, -15 );
		}
	}

	if ( m_iWantToEjectShell && m_flNextShellEjectTime < gpGlobals->time )
	{
		if ( m_iWantToEjectShell == 2 )
			m_iClip--;

		m_iWantToEjectShell = 0;
		RifleEjectBrass();
	}

	if ( m_iSuicideState == 2 )
	{
		UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 0.5, 0.5, 128, 0 );

		if ( ( m_pPlayer->m_afButtonPressed & IN_ATTACK ) || ( m_pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
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

			SendWeaponAnim( RIFLE_SPRINT_FROM );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
		}
	}

	if ( m_bRaised && m_gbIsInCutscene )
	{
		m_bRaised = false;
		SendWeaponAnim( RIFLE_SCOPE_FROM );
		PostZoom();

		if ( m_pPlayer->m_bHoldingBreath )
		{
			m_pPlayer->m_bHoldingBreath = false;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM );
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, m_pPlayer->pev );
			WRITE_COORD( 666 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_pPlayer->pev->vuser2.x = 90;
	}

	if ( m_bWaitingToReload )
		Reload();

	if ( m_bRifleReload && ( m_pPlayer->m_afButtonPressed & IN_ATTACK ) && !m_bWantToEndReload )
		m_bWantToEndReload = true;

	if ( m_iSprintState == 2 && !m_bRifleReload )
		m_iSprintState = 0;

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_bRifleReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( !m_bRifleReload && m_pPlayer->SufficientlyOffTheGround() && !m_bIronSights && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( RIFLE_JUMP_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 && !m_bRifleReload )
		{
			SendWeaponAnim( RIFLE_JUMP_FROM );

			time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
			m_flNextPrimaryAttack = time + gpGlobals->time;
			m_flTimeWeaponIdle = time + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && !m_bRifleReload && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;

				SendWeaponAnim( RIFLE_SPRINT_TO );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( !m_bRifleReload && ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( RIFLE_SPRINT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( !m_bRifleReload && ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( RIFLE_SPRINT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_flTimeWeaponIdle = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}


class CRifleAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{
		Precache( );
		SET_MODEL( ENT( pev ), "models/ammo/ammo_rifle.mdl" );
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_MODEL( "models/ammo/ammo_rifle.mdl" );
		PRECACHE_SOUND( "weapons/item_get.wav" );
	}
	BOOL AddAmmo( CBaseEntity *pOther )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			if ( pOther->GiveAmmo( 5, "rifle", 15, false ) != -1 )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return TRUE;
			}
		}
		else if ( pOther->GiveAmmo( 5, "rifle", 50, false ) != -1 )
		{
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return TRUE;
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( ammo_rifle, CRifleAmmo );
