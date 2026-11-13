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
#include <fstream>
#include <string>

extern int gmsgUnreadSMS;
extern int gmsgPhone;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

LINK_ENTITY_TO_CLASS( weapon_mobile, CMobile );

// sequences of models/weapons/mobile/v_mobile.mdl
enum mobile_e
{
	MOBILE_IDLE_SMS = 0,
	MOBILE_SMS_LOOKSMS,
	MOBILE_DRAW_SMS,
	MOBILE_HOLSTER_SMS,
	MOBILE_SMSTOFLASH,
	MOBILE_IDLE_FLASH,
	MOBILE_FLASH_LOOKSMS,
	MOBILE_DRAW_FLASH,
	MOBILE_FLASHTOSMS,
	MOBILE_HOLSTER_FLASH,
	MOBILE_SMS_PUNCH,
	MOBILE_FLASH_PUNCH,
	MOBILE_RAISE_SMS,
	MOBILE_RAISE_FLASH,
	MOBILE_RAISE_IDLE,
	MOBILE_LOWER_SMS,
	MOBILE_LOWER_FLASH,
};


void CMobile::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_mobile" );
	Precache( );
	m_iId = 2;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/mobile/w_mobile.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	m_iSprintIdleAnim1 = 18;
	m_iSprintIdleAnim2 = 21;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_mobile.txt" );

	if ( custom_script )
		script = custom_script;

	strwmodel = MAKE_STRING( "models/weapons/mobile/w_mobile.mdl" );
}

void CMobile::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/mobile/v_mobile.mdl" );
	PRECACHE_MODEL( "models/weapons/mobile/w_mobile.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/mobile/mobile_switch.wav" );
	PRECACHE_SOUND( "weapons/mobile/mobile_sms.wav" );
	PRECACHE_SOUND( "weapons/mobile/punch_smack.wav" );
	PRECACHE_SOUND( "weapons/mobile/punch_swing.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	PRECACHE_SOUND( "phone/phone_ringing.wav" );
	PRECACHE_SOUND( "phone/phone_ringtone.wav" );
	PRECACHE_SOUND( "phone/tone_one.wav" );
	PRECACHE_SOUND( "phone/tone_two.wav" );
	PRECACHE_SOUND( "phone/tone_three.wav" );
	PRECACHE_SOUND( "phone/tone_four.wav" );
	PRECACHE_SOUND( "phone/tone_five.wav" );
	PRECACHE_SOUND( "phone/tone_six.wav" );
	PRECACHE_SOUND( "phone/tone_seven.wav" );
	PRECACHE_SOUND( "phone/tone_eight.wav" );
	PRECACHE_SOUND( "phone/tone_nine.wav" );
	PRECACHE_SOUND( "phone/tone_send.wav" );

	PRECACHE_SOUND( "weapons/rifle/rifle_insert.wav" );

	PRECACHE_SOUND( "phone/phone_vibrate.wav" );
	PRECACHE_SOUND( "weapons/mobile/low_battery_warn.wav" );
}

int CMobile::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 1;
	p->iPosition = 0;
	p->iId = 2;
	p->iWeight = 1;
	return 1;
}

BOOL CMobile::Deploy( )
{
	if ( pev->body != m_pPlayer->m_iPhoneMessage )
		pev->body = m_pPlayer->m_iPhoneMessage;

	SendWeaponBody( m_pPlayer, pev->body );

	m_iSprintState = 0;

	if ( m_pPlayer->m_iMobileFlashMode )
		return DefaultDeploy( "models/weapons/mobile/v_mobile.mdl", "models/null.mdl", MOBILE_DRAW_FLASH, "crowbar" );

	return DefaultDeploy( "models/weapons/mobile/v_mobile.mdl", "models/null.mdl", MOBILE_DRAW_SMS, "crowbar" );
}

void CMobile::Holster( )
{
	if ( m_pPlayer->m_bInPhoneCall )
		return;

	if ( m_pPlayer->m_iMobileFlashMode )
	{
		SendWeaponAnim( MOBILE_HOLSTER_FLASH );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	}
	else
	{
		SendWeaponAnim( MOBILE_HOLSTER_SMS );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	}
}

void CMobile::FixBattery( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/sleeve_generic3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

	if ( m_pPlayer->m_iMobileFlashMode )
	{
		SendWeaponAnim( MOBILE_DRAW_FLASH );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	}
	else
	{
		SendWeaponAnim( MOBILE_DRAW_SMS );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	}

	m_bDisabled = false;
	m_pPlayer->m_iPhoneDisabled = 0;
	SetBodygroup( 1, 0 );
	SetBodygroup( 2, 0 );
	m_pPlayer->m_iPhoneMessage = pev->body;
	SendWeaponBody( m_pPlayer, pev->body );

	m_pPlayer->m_bPhoneLow = false;
	m_pPlayer->m_fNextPhoneWarnTime = gpGlobals->time;

	m_bWaitingToFix = false;
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CMobile::MidBatteryFix( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/rifle/rifle_insert.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );

	SetNextThink( 1 );
	SetThink( &CMobile::FixBattery );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 5;
}

void CMobile::ItemPostFrame( void )
{
	if ( m_bWaitingToFix )
	{
		if ( !m_pPlayer->m_bInPhoneCall && !( m_pPlayer->m_fPhoneCallTime > gpGlobals->time ) &&
			!( m_flNextPrimaryAttack > gpGlobals->time ) && !( m_flNextSecondaryAttack > gpGlobals->time ) && !( m_flNextTertiaryAttack > gpGlobals->time ) &&
			!m_iSprintState )
		{
			if ( m_pPlayer->m_iMobileFlashMode )
			{
				SendWeaponAnim( MOBILE_HOLSTER_FLASH );
				m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;

				SetNextThink( 1 );
				SetThink( &CMobile::MidBatteryFix );
			}
			else
			{
				SendWeaponAnim( MOBILE_HOLSTER_SMS );
				m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;

				SetNextThink( 1 );
				SetThink( &CMobile::MidBatteryFix );
			}

			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "weapons/sleeve_generic2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flNextTertiaryAttack = m_pPlayer->m_flNextAttack = gpGlobals->time + 5;
		}
	}

	if ( pev->body != m_pPlayer->m_iPhoneMessage )
	{
		pev->body = m_pPlayer->m_iPhoneMessage;
		SendWeaponBody( m_pPlayer, pev->body );
	}

	if ( !m_pPlayer->m_iMobileFlashMode )
	{
		if ( m_pPlayer->m_iFlashlightFlags & 1 )
			m_pPlayer->m_iFlashlightFlags &= ~1;

		if ( m_pPlayer->m_iFlashlightFlags & 2 )
			m_pPlayer->m_iFlashlightFlags &= ~2;
	}

	pev->health = 50;

	if ( !m_pPlayer->m_bInPhoneCall && m_bPhoneCall )
		m_bPhoneCall = false;

	if ( m_pPlayer->m_UnreadSMS == 1 && !m_pPlayer->IsInCutscene() )
	{
		if ( m_pPlayer->m_bPhoneLow )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgUnreadSMS, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgUnreadSMS, NULL, m_pPlayer->pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		MessageGet();

		m_pPlayer->m_UnreadSMS = 0;
	}

	if ( m_pPlayer->m_fPhoneCallTime > gpGlobals->time )
	{
		SendWeaponAnim( MOBILE_RAISE_IDLE );
		return;
	}

	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	CBasePlayerWeapon::ItemPostFrame();
}

void CMobile::PrimaryAttack( void )
{
	if ( m_pPlayer->m_bInPhoneCall )
		return;

	if ( m_pPlayer->m_fPhoneCallTime > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( m_pPlayer->m_iMobileFlashMode )
	{
		SetNextThink( 0.66 );
		SetThink( &CMobile::FlashToggle );
		SendWeaponAnim( MOBILE_FLASHTOSMS );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
	}
	else
	{
		SetNextThink( 0.23 );
		SetThink( &CMobile::FlashToggle );
		SendWeaponAnim( MOBILE_SMSTOFLASH );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
	}
}

void CMobile::SecondaryAttack( void )
{
	if ( m_pPlayer->m_bInPhoneCall )
		return;

	if ( m_pPlayer->m_fPhoneCallTime > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	if ( m_pPlayer->m_iMobileFlashMode )
	{
		m_pPlayer->ViewPunch( 1, 1, 3 );
		SendWeaponAnim( MOBILE_FLASH_PUNCH );
		m_pPlayer->SetNextThink( 0.133 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkPunch );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );
	}
	else
	{
		m_pPlayer->ViewPunch( 1, 1, 3 );
		SendWeaponAnim( MOBILE_SMS_PUNCH );
		m_pPlayer->SetNextThink( 0.133 );
		m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkPunch );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );
	}
}

void CMobile::WeaponToggle( void )
{
	if ( m_pPlayer->m_bInPhoneCall )
		return;

	if ( m_pPlayer->m_fPhoneCallTime > gpGlobals->time )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_flNextTertiaryAttack > gpGlobals->time )
		return;

	m_pPlayer->ShowVGUIMenu( 200 );

	MESSAGE_BEGIN( MSG_ONE, gmsgPhone, NULL, m_pPlayer->pev );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
}

void CMobile::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_iSprintState )
		return;

	if ( m_pPlayer->m_bInPhoneCall && !m_bPhoneCall )
	{
		SendWeaponAnim( MOBILE_RAISE_IDLE );
		m_bPhoneCall = true;
		return;
	}

	if ( m_bPhoneCall )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_pPlayer->m_iMobileFlashMode )
		SendWeaponAnim( MOBILE_IDLE_FLASH );
	else
		SendWeaponAnim( MOBILE_IDLE_SMS );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CMobile::FlashToggle( void )
{
	if ( m_pPlayer->m_iMobileFlashMode )
	{
		m_pPlayer->m_iMobileFlashMode = 0;

		if ( m_pPlayer->m_iFlashlightFlags & 1 )
			m_pPlayer->m_iFlashlightFlags &= ~1;

		if ( m_pPlayer->m_iFlashlightFlags & 2 )
			m_pPlayer->m_iFlashlightFlags &= ~2;
	}
	else
	{
		if ( !m_bDisabled )
		{
			if ( !( m_pPlayer->m_iFlashlightFlags & 1 ) )
				m_pPlayer->m_iFlashlightFlags |= 1;

			if ( m_pPlayer->m_iFlashlightFlags & 2 )
				m_pPlayer->m_iFlashlightFlags &= ~2;
		}

		m_pPlayer->m_iMobileFlashMode = 1;
	}
}

void CMobile::FlashOff( void )
{
	if ( m_pPlayer->m_iFlashlightFlags & 1 )
	{
		m_pPlayer->m_iMobileFlashMode = 0;

		if ( m_pPlayer->m_iFlashlightFlags & 2 )
			m_pPlayer->m_iFlashlightFlags |= 2;

		if ( m_pPlayer->m_iFlashlightFlags & 1 )
			m_pPlayer->m_iFlashlightFlags |= 1;
	}
}

void CMobile::MessageGet( void )
{
	char sFileName[256];
	GET_GAME_DIR( sFileName );

	int language = 0;
	language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

	switch ( language )
	{
	case 0:
		strcat( sFileName, "/txtfiles/phone_messages.txt" );
		break;
	case 1:
		strcat( sFileName, "/txtfiles/phone_messages.txt" );
		break;
	case 2:
		strcat( sFileName, "/txtfiles/languages/dutch/phone_messages.txt" );
		break;
	case 3:
		strcat( sFileName, "/txtfiles/languages/french/phone_messages.txt" );
		break;
	case 4:
		strcat( sFileName, "/txtfiles/languages/german/phone_messages.txt" );
		break;
	case 5:
		strcat( sFileName, "/txtfiles/languages/norwegian/phone_messages.txt" );
		break;
	case 6:
		strcat( sFileName, "/txtfiles/languages/spanish/phone_messages.txt" );
		break;
	case 7:
		strcat( sFileName, "/txtfiles/languages/swedish/phone_messages.txt" );
		break;
	default:
		strcat( sFileName, "/txtfiles/phone_messages.txt" );
		break;
	}

	char sLineText[256];
	char DestinationText[256];

	std::ifstream file( sFileName );
	std::string line;

	std::string SourceString;

	int curLine = m_pPlayer->m_iPhoneMessage + 1;

	line = curLine;

	int i = 1;

	while ( getline( file, line ) )
	{
		if ( i == curLine )
		{
			if ( line.length() > 254 )
			{
				UTIL_CenterPrintAll( "Your message is too long. Rewrite it!" );
				continue;
			}

			strcpy( DestinationText, line.c_str() );
			DestinationText[line.length()] = 0;
		}

		i++;
	}

	if ( sLineText && !m_bDisabled )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
				UTIL_ProMessage( plr, DestinationText, 1, false );
		}
	}
}
