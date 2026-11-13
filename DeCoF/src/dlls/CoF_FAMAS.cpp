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
#include "cofr_features.h"
#include "gamerules.h"
#include "animation.h"
#include "soundent.h"

extern int gmsgParticle;
extern int gmsgIronSights;
extern int gmsgCameraZoom;
extern unsigned short m_usThirdFlash;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// sequences of models/weapons/famas/v_famas.mdl
enum famas_e
{
	FAMAS_IDLE = 0,
	FAMAS_DRAW,
	FAMAS_DRAW_FIRST,
	FAMAS_HOLSTER,
	FAMAS_SHOOT1,
	FAMAS_RELOAD,
	FAMAS_RELOAD_EMPTY,
	FAMAS_SWITCH,
	FAMAS_SPRINT_TO,
	FAMAS_SPRINT_IDLE,
	FAMAS_SPRINT_FROM,
	FAMAS_MELEE,
	FAMAS_FIDGET1,
	FAMAS_FIDGET2,
	FAMAS_FIDGET3,
	FAMAS_IS_TO,
	FAMAS_IS_IDLE,
	FAMAS_IS_SHOOT,
	FAMAS_IS_FROM,
	FAMAS_SUICIDE,
	FAMAS_SHOOT_NAH,
	FAMAS_IS_SHOOT_NOSHOT,
	FAMAS_JUMP_TO,
	FAMAS_JUMP_FROM,
};

LINK_ENTITY_TO_CLASS( weapon_famas, CFAMAS );


void CFAMAS::SendInitMessage( CBasePlayer *player )
{
	if ( player && !m_bCheckedIfShouldBeDeleted )
	{
		m_bCheckedIfShouldBeDeleted = true;

		if ( !player->CheckingEndingOrUnlockables( 64, false, false ) && !CoFR_CarriedUnlock(player,64) )
			UTIL_Remove( this );
	}
}

void CFAMAS::StartSuicide( void )
{
	SetNextThink( 4.0 );
	SetThink( &CFAMAS::SuicideShot );

	SendWeaponAnim( FAMAS_SUICIDE );

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

void CFAMAS::SuicideShot( void )
{
	if ( m_iClip )
	{
		m_pPlayer->pev->health = 0;
		m_pPlayer->Killed( m_pPlayer->pev, GIB_NEVER );

		PlayWeaponSound_guess( "weapons_new/famas/famas_shoot.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		return;
	}

	PlayEmptySound();
	m_fEmptyClickTime = gpGlobals->time + 0.25;
	StopSuicide();
}

void CFAMAS::StopSuicide( void )
{
	SetThink( NULL );
	m_iSuicideButtonsPressed = 0;
	m_iSuicideState = 0;

	UTIL_ScreenFade( m_pPlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( m_pPlayer->pev->fuser3 == 10 )
		m_pPlayer->pev->fuser3 = 0;

	SendWeaponAnim( FAMAS_DRAW );

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CFAMAS::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_famas" );
	Precache( );

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/famas/w_famas.mdl" );

	m_iId = 23;

	m_iSprintIdleAnim1 = FAMAS_SPRINT_IDLE;
	m_iSprintIdleAnim2 = FAMAS_SPRINT_IDLE;

	m_flKickPerShot = 0.15;

	if ( pev->frags > 0 )
	{
		m_iDefaultAmmo = pev->frags;
	}
	else
	{
		if ( pev->frags == -2 )
			m_iClip = -666;
		m_iDefaultAmmo = 30;
	}

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_famas.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	m_bCanSuicide = true;

	strwmodel = MAKE_STRING( "models/weapons/famas/w_famas.mdl" );

	FallInit();// get ready to fall down.
}

void CFAMAS::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/famas/v_famas.mdl" );
	PRECACHE_MODEL( "models/weapons/famas/w_famas.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	PRECACHE_SOUND( "weapons/famas/famas_shoot.wav" );
	PRECACHE_SOUND( "subtitles/french4.wav" );

	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell

	PRECACHE_MODEL( "models/w_9mmclip.mdl" );
	PRECACHE_SOUND( "weapons/item_get.wav" );
}

int CFAMAS::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "famas";
	p->iMaxAmmo1 = 150;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = 30;
	p->iSlot = 4;
	p->iPosition = 0;
	p->iFlags = 0;
	p->iId = m_iId = 23;
	p->iWeight = 5;

	return 1;
}

BOOL CFAMAS::Deploy( )
{
	m_iSprintState = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		EMIT_SOUND( m_pPlayer->edict(), CHAN_VOICE, "subtitles/french4.wav", 1, ATTN_NORM );
		UTIL_ProMessage( m_pPlayer, "Ugh...Argh... French...", 0, false );
		return DefaultDeploy( "models/weapons/famas/v_famas.mdl", "models/null.mdl", FAMAS_DRAW_FIRST, "onehanded" );
	}

	return DefaultDeploy( "models/weapons/famas/v_famas.mdl", "models/null.mdl", FAMAS_DRAW, "onehanded" );
}

void CFAMAS::Holster( )
{
	SendWeaponAnim( FAMAS_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CFAMAS::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( gpGlobals->time > m_fEmptyClickTime )
		{
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
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.5;

			m_flNextPrimaryAttack = gpGlobals->time + 0.5;
			m_flNextSecondaryAttack = gpGlobals->time + 0.5;

			if ( m_bIronSights )
				SendWeaponAnim( FAMAS_IS_SHOOT_NOSHOT );
			else
				SendWeaponAnim( FAMAS_SHOOT_NAH );
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
		WRITE_STRING( "models/weapons/famas/v_famas.mdl" );
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

	m_pPlayer->ViewPunch( RANDOM_LONG( -2, 2 ), 1, 1 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	if ( m_bIronSights )
		SendWeaponAnim( FAMAS_IS_SHOOT );
	else
		SendWeaponAnim( FAMAS_SHOOT1 );

	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecShellSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick += m_flKickPerShot;
	m_flNextKickReductionTime = gpGlobals->time + 0.25;

	vecDir = m_pPlayer->FireBulletsPlayer( 1, vecShellSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, 15, m_pPlayer->pev, m_pPlayer->random_seed );

	m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 1.0 );

	PlayWeaponSound_guess( "weapons_new/famas/famas_shoot.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;
	m_flNextPrimaryAttack = gpGlobals->time + 0.06;
	m_flNextSecondaryAttack = gpGlobals->time + 1;

	if ( m_iClip <= 0 )
		m_flNextPrimaryAttack = gpGlobals->time + 1;
}

void CFAMAS::Reload( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( m_iSuicideState )
		return;

	if ( m_iClip == iMaxClip() )
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

	if ( m_iClip )
	{
		m_iClip = 0;
		DefaultReload( 30, FAMAS_RELOAD, 2.5, 1, false );
	}
	else
	{
		m_iClip = 0;
		DefaultReload( 30, FAMAS_RELOAD_EMPTY, 3.6, 1, false );
	}
}

void CFAMAS::WeaponIdle( void )
{
	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( m_bIronSights )
		SendWeaponAnim( FAMAS_IS_IDLE );
	else
		SendWeaponAnim( FAMAS_IDLE );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CFAMAS::IronsightsTo( void )
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

	SendWeaponAnim( FAMAS_IS_TO );

	m_bIronSights = true;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CFAMAS::IronsightsFrom( void )
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

	SendWeaponAnim( FAMAS_IS_FROM );

	m_bIronSights = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = gpGlobals->time + 0.3;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CFAMAS::TertiaryAttack( void )
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

void CFAMAS::ItemPostFrame( void )
{
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

			SendWeaponAnim( FAMAS_SPRINT_FROM );
		}
		else if ( m_iNextSprintAnim < gpGlobals->time && !m_iSprintState )
		{
			m_iSuicideState = 2;
			StartSuicide();
		}
	}

	if ( m_bWaitingToReloadIronsights )
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

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() && !m_bIronSights && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				SendWeaponAnim( FAMAS_JUMP_TO );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + time * 0.5;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( FAMAS_JUMP_FROM );
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

				SendWeaponAnim( FAMAS_SPRINT_TO );
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
				SendWeaponAnim( FAMAS_SPRINT_IDLE );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( FAMAS_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + 0.33;
				m_flTimeWeaponIdle = gpGlobals->time + 0.35;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CFAMAS::SecondaryAttack( void )
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

	SendWeaponAnim( FAMAS_MELEE );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CFAMAS::Fidget( void )
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
		SendWeaponAnim( FAMAS_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( FAMAS_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( FAMAS_FIDGET3 );
		break;
	}
}
