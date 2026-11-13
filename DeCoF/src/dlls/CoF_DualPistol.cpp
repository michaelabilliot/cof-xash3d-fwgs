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
#include <fstream>
#include <string>
#include "player.h"
#include "gamerules.h"
#include "animation.h"
#include "soundent.h"
#include "studio.h"

extern int gmsgParticle;
extern int gmsgKillWpnPart;
extern int gmsgDualFlash;
extern int gmsgDualWield;
extern int gmsgDualAmmo;
extern int gmsgUnreadSMS;
extern int gmsgQSlotClear;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );
extern void GetCurrentPath( char *path );

#include "CoF_Dual_Weapon.h"



void MessageGet( int phonemessage )
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

	int curLine = phonemessage + 1;

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

	if ( sLineText )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
				UTIL_ProMessage( plr, DestinationText, 1, false );
		}
	}
}

void WeaponSetBodygroup( void *pmodel, entvars_t *pev, int iGroup, int iValue )
{
	studiohdr_t *pstudiohdr;

	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr )
		return;

	if ( iGroup > pstudiohdr->numbodyparts )
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if ( iValue >= pbodypart->nummodels )
		return;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;

	pev->body = (pev->body - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}

LINK_ENTITY_TO_CLASS( weapon_dualweapon, CDualWeapon );


float CDualWeapon::GetAttackDelayForWeapon( int weaponid )
{
	switch ( weaponid )
	{
	case 1:		// flashlight
		return 0.5;
	case 2:		// mobile
		return 1;
	case 3:		// glock
		return 0.2;
	case 5:		// camera
		return 1.2;
	case 6:		// switchblade
		return 0.8;
	case 7:		// p345
		return 0.25;
	case 21:	// revolver
		return 0.9;
	case 19:	// vp70
		return 0.55;
	case 16:	// branch
		return 1.5;
	case 12:	// nightstick
		return 0.9;
	case 14:	// tmp
		return 0.07;
	}

	return 0.5;
}

void CDualWeapon::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_dualweapon" );
	Precache( );
	SET_MODEL( ENT( pev ), "models/null.mdl" );
	m_iId = 15;

	m_bCanDualWield = true;

	m_bCanSuicide = true;


	strwmodel = MAKE_STRING( "models/null.mdl" );

	m_iCurrentShot = 0;

	m_iLeftWeaponID = pev->iuser1;
	m_iRightWeaponID = pev->iuser2;

	FallInit();// get ready to fall down.
}

void CDualWeapon::Precache( void )
{
	m_iShell = PRECACHE_MODEL( "models/shell.mdl" );// brass shell
}

int CDualWeapon::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 15;
	p->iWeight = 1;
	p->iFlags = ITEM_FLAG_NOAUTORELOAD;

	return 1;
}

bool IsMelee( int id )
{
	switch ( id )
	{
	case 1:		// flashlight
		return true;
	case 2:		// mobile
		return true;
	case 3:		// glock
		return false;
	case 5:		// camera
		return true;
	case 6:		// switchblade
		return true;
	case 10:	// lantern
		return true;
	case 7:		// p345
		return false;
	case 21:	// revolver
		return false;
	case 19:	// vp70
		return false;
	case 16:	// branch
		return true;
	case 12:	// nightstick
		return true;
	case 14:	// tmp
		return false;
	case 13:	// flare
		return true;
	}

	return false;
}

void CDualWeapon::SetBodyForWeapons( bool left )
{
}

void CDualWeapon::UpdateWeaponAmmo( void )
{
	int i;

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_pPlayer->m_rgpPlayerItems[i] )
		{
			CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if ( gun )
				{
					if ( gun->m_iId == m_iLeftWeaponID )
						gun->m_iClip = m_pPlayer->m_iDualWeaponLeftAmmo;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	for ( i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_pPlayer->m_rgpPlayerItems[i] )
		{
			CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if ( gun )
				{
					if ( gun->m_iId == m_iRightWeaponID )
						gun->m_iClip = m_pPlayer->m_iDualWeaponRightAmmo;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}
}

void CDualWeapon::SetAmmoValues( void )
{
	switch ( m_iLeftWeaponID )
	{
	case 1:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 2:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 3:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iLeftWeaponID )
							m_pPlayer->m_iDualWeaponLeftAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 5:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 6:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 10:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 16:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 7:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iLeftWeaponID )
							m_pPlayer->m_iDualWeaponLeftAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 21:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iLeftWeaponID )
							m_pPlayer->m_iDualWeaponLeftAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 19:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iLeftWeaponID )
							m_pPlayer->m_iDualWeaponLeftAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 12:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	case 14:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iLeftWeaponID )
							m_pPlayer->m_iDualWeaponLeftAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 13:
		m_pPlayer->m_iDualWeaponLeftAmmo = -1;
		break;
	}

	switch ( m_iRightWeaponID )
	{
	case 1:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 2:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 3:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iRightWeaponID )
							m_pPlayer->m_iDualWeaponRightAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 5:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 6:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 10:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 16:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 7:
	case 21:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iRightWeaponID )
							m_pPlayer->m_iDualWeaponRightAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 19:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iRightWeaponID )
							m_pPlayer->m_iDualWeaponRightAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 12:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	case 14:
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_pPlayer->m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_pPlayer->m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( gun->m_iId == m_iRightWeaponID )
							m_pPlayer->m_iDualWeaponRightAmmo = gun->m_iClip;
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
		break;
	case 13:
		m_pPlayer->m_iDualWeaponRightAmmo = -1;
		break;
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgDualAmmo, NULL, m_pPlayer->pev );
		WRITE_LONG( m_pPlayer->m_iDualWeaponLeftAmmo );
		WRITE_LONG( m_pPlayer->m_iDualWeaponRightAmmo );
	MESSAGE_END();
}

BOOL CDualWeapon::Deploy( )
{
	m_iCurrentShot = 0;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( m_iSuicideState == 666 )
		m_iSuicideState = 1;
	else
		m_iSuicideState = 0;

	m_iDamageL = gSkillData.DmgGlock;
	m_iDamageR = gSkillData.DmgGlock;

	gDualWeapon.SetModel( m_pPlayer, MAKE_STRING( GetModelForWeapon( m_iLeftWeaponID, true ) ) );

	SetAmmoValues();

	gDualWeapon.m_iSprintState = 0;
	m_iSprintState = 0;

	CFlare *pFlare = NULL;

	gDualWeapon.m_iID = m_iLeftWeaponID;

	switch ( m_iLeftWeaponID )
	{
	case 1:		// flashlight
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_fIdleAnimation1Time = 1;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 1 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 16:	// branch
		gDualWeapon.m_iIdleAnimation = 15;
		gDualWeapon.m_fIdleAnimation1Time = 15;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 16 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 3:		// glock
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_iIdleAnimation2 = 1;

		gDualWeapon.m_fIdleAnimation1Time = 5;
		gDualWeapon.m_fIdleAnimation2Time = 5;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 6 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 7 );

		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 7:		// p345
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_iIdleAnimation2 = 1;

		gDualWeapon.m_fIdleAnimation1Time = 5;
		gDualWeapon.m_fIdleAnimation2Time = 5;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 6 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 7 );

		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 21:	// revolver
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_iIdleAnimation2 = 0;

		gDualWeapon.m_fIdleAnimation1Time = 5;
		gDualWeapon.m_fIdleAnimation2Time = 5;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 1 );

		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 19:	// vp70
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_iIdleAnimation2 = 0;

		gDualWeapon.m_fIdleAnimation1Time = 5;
		gDualWeapon.m_fIdleAnimation2Time = 5;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 1 );

		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 6:		// switchblade
		gDualWeapon.m_iIdleAnimation = 5;
		gDualWeapon.m_fIdleAnimation1Time = 5.1;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 6 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 13:	// flare
		gDualWeapon.m_iIdleAnimation = 2;
		gDualWeapon.m_iIdleAnimation2 = 5;

		gDualWeapon.m_fIdleAnimation1Time = 3;
		gDualWeapon.m_fIdleAnimation2Time = 3;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 0 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 0.83;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 0.83;
		break;

	case 10:	// lantern
		gDualWeapon.m_iIdleAnimation = 2;
		gDualWeapon.m_fIdleAnimation1Time = 5;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 0 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 1.0;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.0;
		break;

	case 2:		// mobile
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_iIdleAnimation2 = 5;

		gDualWeapon.m_fIdleAnimation1Time = 6.66;
		gDualWeapon.m_fIdleAnimation1Time = 6.66;

		if ( gDualWeapon.m_iWeaponMode > 1 )
			gDualWeapon.m_iWeaponMode = 0;

		if ( m_pPlayer->m_iMobileFlashMode == 0 )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
			gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 0.5;
			gDualWeapon.m_flNextIdleTime = gpGlobals->time + 0.5;
		}

		if ( m_pPlayer->m_iMobileFlashMode == 1 )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 7 );
			gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 0.5;
			gDualWeapon.m_flNextIdleTime = gpGlobals->time + 0.5;
		}
		break;

	case 12:	// nightstick
		gDualWeapon.m_iIdleAnimation = 0;
		gDualWeapon.m_fIdleAnimation1Time = 1.2;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 1 );
		gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time + 0.7;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 0.7;
		break;
	}

	m_sLeftHandWeaponModel = MAKE_STRING( GetModelForWeapon( m_iLeftWeaponID, true ) );

	switch ( m_iRightWeaponID )
	{
	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->iuser1 == 1 )
			{
				if ( m_pPlayer )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
						WRITE_SHORT( entindex() );
						WRITE_STRING( "aurora/signal.aur" );
						WRITE_BYTE( 1 );
						if ( pFlare->custom_v_model )
							WRITE_STRING( STRING( pFlare->custom_v_model ) );
						else
							WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
						WRITE_SHORT( 0 );
					MESSAGE_END();

					MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
						WRITE_SHORT( entindex() );
						WRITE_STRING( "aurora/blacksmoke.aur" );
						WRITE_BYTE( 1 );
						if ( pFlare->custom_v_model )
							WRITE_STRING( STRING( pFlare->custom_v_model ) );
						else
							WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
						WRITE_SHORT( 0 );
					MESSAGE_END();

					MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
						WRITE_SHORT( entindex() );
						WRITE_STRING( "aurora/flare_sparks.aur" );
						WRITE_BYTE( 1 );
						if ( pFlare->custom_v_model )
							WRITE_STRING( STRING( pFlare->custom_v_model ) );
						else
							WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
						WRITE_SHORT( 0 );
					MESSAGE_END();

					MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
						WRITE_SHORT( entindex() );
						WRITE_STRING( "aurora/flare_glow.aur" );
						WRITE_BYTE( 1 );
						if ( pFlare->custom_v_model )
							WRITE_STRING( STRING( pFlare->custom_v_model ) );
						else
							WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
						WRITE_SHORT( 2 );
					MESSAGE_END();

					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, 0.8 );
					pFlare->m_fNextBurnTime = gpGlobals->time + 6.9;

					if ( !( m_pPlayer->m_iFlashlightFlags & 16 ) )
						m_pPlayer->m_iFlashlightFlags |= 16;
				}

				return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 1, "onehanded" );
			}
			else
			{
				return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 0, "onehanded" );
			}
		}

		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 0, "onehanded" );

	case 16:	// branch
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 16, "onehanded" );

	case 3:		// glock
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 6, "onehanded" );
		else
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 7, "onehanded" );

	case 7:		// p345
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 6, "onehanded" );
		else
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 7, "onehanded" );

	case 21:	// revolver
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 1, "onehanded" );

	case 19:	// vp70
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 1, "onehanded" );

	case 6:		// switchblade
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 6, "onehanded" );

	case 10:	// lantern
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 0, "onehanded" );

	case 2:		// mobile
		switch ( m_pPlayer->m_iMobileFlashMode )
		{
		case 0:
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 2, "onehanded" );
		case 1:
			return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 7, "onehanded" );
		}
		break;

	case 12:	// nightstick
		return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 1, "onehanded" );
	}

	return DefaultDeploy( GetModelForWeapon( m_iRightWeaponID, false ), "models/null.mdl", 6, "onehanded" );
}

char *CDualWeapon::GetModelForWeapon( int weaponid, bool left )
{
	CBasePlayerWeapon *pWeapon = m_pPlayer->GetPlayerWeapon( weaponid );

	if ( pWeapon && pWeapon->custom_v_model )
		return (char *)STRING( pWeapon->custom_v_model );

	if ( left )
	{
		switch ( weaponid )
		{
		case 1:		// flashlight
			return "models/null.mdl";

		case 2:		// mobile
			return "models/null.mdl";
		case 6:		// switchblade
			return "models/weapons/switchblade/v_switchblade.mdl";

		case 10:	// lantern
			return "models/null.mdl";
		case 3:		// glock
			return "models/weapons/glock/v_glock.mdl";
		case 7:		// p345
			return "models/weapons/p345/v_p345.mdl";
		case 21:	// revolver
			return "models/weapons/revolver/v_revolver.mdl";
		case 19:	// vp70
			return "models/weapons/vp70/v_vp70.mdl";
		case 12:	// nightstick
			return "models/weapons/nightstick/v_nightstick.mdl";
		case 16:	// branch
			return "models/weapons/branch/v_branch.mdl";
		case 14:	// tmp
			return "models/null.mdl";
		case 13:	// flare
			return "models/null.mdl";
		default:
			return "models/null.mdl";
		}
	}
	else
	{
		switch ( weaponid )
		{
		case 1:		// flashlight
			return "models/weapons/flashlight/v_flashlight.mdl";

		case 2:		// mobile
			return "models/weapons/mobile/v_mobile.mdl";
		case 3:		// glock
			return "models/weapons/glock/v_glock.mdl";
		case 7:		// p345
			return "models/weapons/p345/v_p345.mdl";
		case 21:	// revolver
			return "models/weapons/revolver/v_revolver.mdl";
		case 19:	// vp70
			return "models/weapons/vp70/v_vp70.mdl";
		case 5:		// camera
			return "models/null.mdl";
		case 6:		// switchblade
			return "models/weapons/switchblade/v_switchblade.mdl";

		case 10:	// lantern
			return "models/weapons/lantern/v_lantern.mdl";
		case 12:	// nightstick
			return "models/weapons/nightstick/v_nightstick.mdl";
		case 16:	// branch
			return "models/weapons/branch/v_branch.mdl";
		case 14:	// tmp
			return "models/null.mdl";
		case 13:	// flare
			return "models/weapons/flare/v_flare.mdl";
		default:
			return "models/null.mdl";
		}
	}
}

void CDualWeapon::FlareRedeploy( void )
{
	if ( !m_pPlayer )
		return;

	CFlare *pFlare = NULL;
	pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

	if ( !pFlare )
		return;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	m_pPlayer->m_iFlashlightFlags &= ~16;


	pFlare->m_bNoRedeploy = false;
	pFlare->m_bThrowing = false;
	pFlare->pev->renderfx = 0;
	pFlare->pev->iuser1 = 0;
	pFlare->pev->teleport_time = 0;
	pFlare->pev->radsuit_finished = 0;
	SendWeaponAnim( 0 );
	m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 0.86;
}

void CDualWeapon::ThrowRemove( void )
{
	if ( !m_pPlayer )
		return;

	CFlare *pFlare = NULL;
	pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

	if ( !pFlare )
		return;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pFlare->pev->iuser1 = 0;

	if ( m_pPlayer->m_rgAmmo[pFlare->m_iPrimaryAmmoType] >= 1 )
	{
		if ( !pFlare->m_bNoRedeploy )
		{
			SetThink( &CDualWeapon::FlareRedeploy );
			SetNextThink( 0.5 );
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 1.5;
		}

		return;
	}

	if ( m_pPlayer->m_pLastItem )
	{
		if ( FClassnameIs( m_pPlayer->m_pLastItem->edict(), "weapon_flare" ) )
		{
			if ( m_iLeftWeaponID && m_iLeftWeaponID != 13 )
			{
				CBasePlayerWeapon *pWeapon = m_pPlayer->GetPlayerWeapon( m_iLeftWeaponID );

				if ( pWeapon )
				{
					m_pPlayer->m_pLastItem = pWeapon;
					m_pPlayer->SelectLastItem( true );
				}
				else
					m_pPlayer->SelectLastItem( true );
			}
			else
				m_pPlayer->SelectLastItem( true );
		}
		else
			m_pPlayer->SelectLastItem( true );
	}
	else
	{
		if ( m_pPlayer->HasNamedPlayerItem( "weapon_mobile" ) )
			m_pPlayer->SelectItem( "weapon_mobile" );
		else if ( m_pPlayer->HasWeapons() )
			m_pPlayer->SelectRandomWeapon();
	}

	if ( m_pPlayer )
	{
		// clear the quick slots that hold the flare
		if ( !strcmp( m_pPlayer->m_sQuickSel1, STRING( pFlare->pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel1, "" );

			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[1] = 0;
		}

		if ( !strcmp( m_pPlayer->m_sQuickSel2, STRING( pFlare->pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel2, "" );

			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 2 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[2] = 0;
		}

		if ( !strcmp( m_pPlayer->m_sQuickSel3, STRING( pFlare->pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel3, "" );

			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 3 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[3] = 0;
		}

		m_pPlayer->pev->weapons &= ~( 1 << pFlare->m_iId );
		m_pPlayer->HasItemInSlot( (char *)STRING( pFlare->script ), true );

		m_pPlayer->RemovePlayerItem( pFlare );

		m_pPlayer->pev->weapons &= ~( 1 << m_iId );
		m_pPlayer->HasItemInSlot( (char *)STRING( script ), true );

		m_pPlayer->RemovePlayerItem( this );
	}

	pFlare->Kill();
	Kill();
}

void CDualWeapon::Holster( )
{
	CFlare *pFlare = NULL;

	switch ( m_iLeftWeaponID )
	{
	case 1:		// flashlight
		gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
		break;

	case 3:		// glock
		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 9 );
		break;

	case 7:		// p345
		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 9 );
		break;

	case 21:	// revolver
		gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
		break;

	case 19:	// vp70
		gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
		break;

	case 6:		// switchblade
		gDualWeapon.SendWeaponAnim( m_pPlayer, 7 );
		break;

	case 10:	// lantern
		gDualWeapon.SendWeaponAnim( m_pPlayer, 4 );
		break;

	case 2:		// mobile
		switch ( m_pPlayer->m_iMobileFlashMode )
		{
		case 0:
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
			break;
		case 1:
			gDualWeapon.SendWeaponAnim( m_pPlayer, 9 );
			break;
		}
		break;

	case 12:	// nightstick
		gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
		break;
	case 16:	// branch
		gDualWeapon.SendWeaponAnim( m_pPlayer, 18 );
		break;
	}

	switch ( m_iRightWeaponID )
	{
	case 1:		// flashlight
		SendWeaponAnim( 2 );
		break;

	case 3:		// glock
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 8 );
		else
			SendWeaponAnim( 9 );
		break;

	case 7:		// p345
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 8 );
		else
			SendWeaponAnim( 9 );
		break;

	case 21:	// revolver
		SendWeaponAnim( 2 );
		break;

	case 19:	// vp70
		SendWeaponAnim( 3 );
		break;

	case 6:		// switchblade
		SendWeaponAnim( 7 );
		break;

	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare && pFlare->pev->iuser1 == 1 )
		{
			SendWeaponAnim( 8 );
			SetNextThink( 0.9 );
			SetThink( &CDualWeapon::ThrowRemove );
			pFlare->m_bThrowing = true;
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
		}

		if ( ( pFlare && !pFlare->pev->iuser1 ) || !pFlare )
		{
			SendWeaponAnim( 6 );
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
		}

		if ( pFlare && pFlare->pev->iuser1 == 2 )
		{
			m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
		}
		break;

	case 10:	// lantern
		SendWeaponAnim( 4 );
		break;

	case 2:		// mobile
		switch ( m_pPlayer->m_iMobileFlashMode )
		{
		case 0:
			SendWeaponAnim( 3 );
			break;
		case 1:
			SendWeaponAnim( 9 );
			break;
		}
		break;

	case 12:	// nightstick
		SendWeaponAnim( 2 );
		break;
	case 16:	// branch
		SendWeaponAnim( 18 );
		break;
	}

	gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.5;

	m_pPlayer->m_flNextAttack = gpGlobals->time + 1.0;
}

void CDualWeapon::SecondaryAttack( void )
{
	switch ( m_iRightWeaponID )
	{
	case 1:		// flashlight
		FlashlightSwitch( false );
		break;
	case 2:		// mobile
		MobileSwitch( false );
		break;
	case 3:		// glock
		GlockShoot( false );
		break;
	case 5:		// camera
		CameraShoot( false );
		break;
	case 6:		// switchblade
		SwitchbladeSlash( false );
		break;
	case 7:		// p345
		P345Shoot( false );
		break;
	case 21:	// revolver
		RevolverShoot( false );
		break;
	case 19:	// vp70
		StartVP70Shoot( false );
		break;
	case 12:	// nightstick
		NightstickHit( false );
		break;
	case 16:	// branch
		BranchHit( false );
		break;
	case 14:	// tmp
		TMPShoot( false );
		break;
	case 13:	// flare
		FlareIgnite( false );
		break;
	}
}

void CDualWeapon::PrimaryAttack( void )
{
	switch ( m_iLeftWeaponID )
	{
	case 1:		// flashlight
		FlashlightSwitch( true );
		break;
	case 2:		// mobile
		MobileSwitch( true );
		break;
	case 3:		// glock
		GlockShoot( true );
		break;
	case 5:		// camera
		CameraShoot( true );
		break;
	case 6:		// switchblade
		SwitchbladeSlash( true );
		break;
	case 7:		// p345
		P345Shoot( true );
		break;
	case 21:	// revolver
		RevolverShoot( true );
		break;
	case 19:	// vp70
		StartVP70Shoot( true );
		break;
	case 12:	// nightstick
		NightstickHit( true );
		break;
	case 16:	// branch
		BranchHit( true );
		break;
	case 14:	// tmp
		TMPShoot( true );
		break;
	case 13:	// flare
		FlareIgnite( true );
		break;
	}
}

void CDualWeapon::FlashlightSwitch( bool left )
{
	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "items/flashlight1.wav", 1, 0.8, 0, 100 );
	m_pPlayer->m_iFlashlight = !m_pPlayer->m_iFlashlight;

	if ( left )
		gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
	else
		SendWeaponAnim( 3 );

	if ( m_pPlayer->m_iFlashlight )
	{
		if ( !( m_pPlayer->m_iFlashlightFlags & 8 ) )
			m_pPlayer->m_iFlashlightFlags |= 8;
	}
	else
	{
		if ( m_pPlayer->m_iFlashlightFlags & 8 )
			m_pPlayer->m_iFlashlightFlags &= ~8;
	}

	if ( left )
		m_flNextPrimaryAttack = gpGlobals->time + 0.5;
	else
		m_flNextSecondaryAttack = gpGlobals->time + 0.5;
}

void CDualWeapon::MobileFlashToggle( void )
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
		if ( !m_pPlayer->m_iPhoneDisabled )
		{
			if ( !( m_pPlayer->m_iFlashlightFlags & 1 ) )
				m_pPlayer->m_iFlashlightFlags |= 1;

			if ( m_pPlayer->m_iFlashlightFlags & 2 )
				m_pPlayer->m_iFlashlightFlags &= ~2;
		}

		m_pPlayer->m_iMobileFlashMode = 1;
	}
}

void CDualWeapon::MobileSwitch( bool left )
{
	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( m_pPlayer->m_iMobileFlashMode == 1 )
		{
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.66;
			gDualWeapon.m_iThinkState = 9;
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
			m_flNextPrimaryAttack = gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
		}

		if ( m_pPlayer->m_iMobileFlashMode == 0 )
		{
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.23;
			gDualWeapon.m_iThinkState = 8;
			gDualWeapon.SendWeaponAnim( m_pPlayer, 4 );
			m_flNextPrimaryAttack = gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
		}
	}
	else
	{
		if ( m_pPlayer->m_iMobileFlashMode == 1 )
		{
			SetNextThink( 0.66 );
			SetThink( &CDualWeapon::MobileFlashToggle );
			SendWeaponAnim( 8 );
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		}
		else if ( m_pPlayer->m_iMobileFlashMode == 0 )
		{
			SetNextThink( 0.23 );
			SetThink( &CDualWeapon::MobileFlashToggle );
			SendWeaponAnim( 4 );
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
		}
	}
}

void CDualWeapon::CameraShoot( bool left ) {}

void CDualWeapon::NightstickHit( bool left )
{
	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;
	}

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		if ( left )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.4;
			gDualWeapon.m_iThinkState = 5;
			m_flNextPrimaryAttack = gpGlobals->time + 0.9;
		}
		else
		{
			SendWeaponAnim( 3 );
			m_pPlayer->SetNextThink( 0.4 );
			m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick );
			m_flNextSecondaryAttack = gpGlobals->time + 0.9;
		}
		break;

	case 1:
		if ( left )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 4 );
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.31;
			gDualWeapon.m_iThinkState = 6;
			m_flNextPrimaryAttack = gpGlobals->time + 0.9;
		}
		else
		{
			SendWeaponAnim( 4 );
			m_pPlayer->SetNextThink( 0.31 );
			m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick2 );
			m_flNextSecondaryAttack = gpGlobals->time + 0.9;
		}
		break;

	case 2:
		if ( left )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 5 );
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.35;
			gDualWeapon.m_iThinkState = 7;
			m_flNextPrimaryAttack = gpGlobals->time + 0.9;
		}
		else
		{
			SendWeaponAnim( 5 );
			m_pPlayer->SetNextThink( 0.35 );
			m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkNightStick3 );
			m_flNextSecondaryAttack = gpGlobals->time + 0.9;
		}
		break;
	}

	if ( left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CDualWeapon::BranchHit( bool left )
{
	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;
	}

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	switch ( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		if ( left )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 17 );
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.34;
			gDualWeapon.m_iThinkState = 10;
			m_flNextPrimaryAttack = gpGlobals->time + 1;
		}
		else
		{
			SendWeaponAnim( 17 );
			m_pPlayer->SetNextThink( 0.34 );
			m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkBranch );
			m_flNextSecondaryAttack = gpGlobals->time + 1;
		}
		break;

	case 1:
		if ( left )
		{
			gDualWeapon.SendWeaponAnim( m_pPlayer, 17 );
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.34;
			gDualWeapon.m_iThinkState = 10;
			m_flNextPrimaryAttack = gpGlobals->time + 1;
		}
		else
		{
			SendWeaponAnim( 17 );
			m_pPlayer->SetNextThink( 0.34 );
			m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkBranch );
			m_flNextSecondaryAttack = gpGlobals->time + 1;
		}
		break;
	}

	EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_WEAPON, "weapons/branch/branch_swing.wav", 1, 0.8, 0, 100 );

	if ( left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CDualWeapon::SwitchbladeSlash( bool left )
{
	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;
	}

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( left )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
			break;
		case 1:
			gDualWeapon.SendWeaponAnim( m_pPlayer, 18 );
			break;
		case 2:
			gDualWeapon.SendWeaponAnim( m_pPlayer, 19 );
			break;
		}
	}
	else
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			SendWeaponAnim( 8 );
			break;
		case 1:
			SendWeaponAnim( 18 );
			break;
		case 2:
			SendWeaponAnim( 19 );
			break;
		}
	}

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/switchblade/switchblade_swing.wav", 1, 0.8 );

	m_pPlayer->SetNextThink( 0.16 );
	m_pPlayer->SetThink( &CBasePlayer::MeleeHitThinkSwitchblade );

	if ( left )
	{
		m_flNextPrimaryAttack = GetAttackDelayForWeapon( m_iLeftWeaponID ) + gpGlobals->time;
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1.2;
	}
	else
	{
		m_flNextSecondaryAttack = GetAttackDelayForWeapon( m_iRightWeaponID ) + gpGlobals->time;
		m_flTimeWeaponIdle = gpGlobals->time + 1.2;
	}
}

void CDualWeapon::P345Shoot( bool left )
{
	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
			return;
	}
	else
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
			return;
	}

	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo <= 0 )
		{
			if ( gDualWeapon.m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				gDualWeapon.m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
			return;
		}
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo <= 0 )
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.8;
			}
			return;
		}
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.8;
		}
		return;
	}

	if ( left )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 - gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}
	else
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}

	if ( !left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( GetModelForWeapon( m_iRightWeaponID, false ) );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( left )
		m_pPlayer->ViewPunch( 10, 4, 4 );
	else
		m_pPlayer->ViewPunch( -10, 4, 4 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.2, 0.15, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 6.0, 0.35, 0.15, 2.0 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick = 5;

	if ( left )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageL, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageR, m_pPlayer->pev, m_pPlayer->random_seed );

	if ( left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 8.0 );

	int pitch = 95 + RANDOM_LONG( 0, 10 );

	PlayWeaponSound_guess( "weapons_new/p345/p345_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	if ( left )
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponLeftAmmo--;

		m_flNextPrimaryAttack = GetAttackDelayForWeapon( m_iLeftWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}
	else
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponRightAmmo--;

		m_flNextSecondaryAttack = GetAttackDelayForWeapon( m_iRightWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}

	if ( left )
	{
		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
	}
	else
	{
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 2 );
		else
			SendWeaponAnim( 3 );
	}

	if ( left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualFlash, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CDualWeapon::RevolverShoot( bool left )
{
	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
			return;
	}
	else
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
			return;
	}

	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo <= 0 )
		{
			if ( gDualWeapon.m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				gDualWeapon.m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
			return;
		}
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo <= 0 )
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.8;
			}
			return;
		}
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.8;
		}
		return;
	}

	if ( left )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 - gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}
	else
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}

	if ( !left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( GetModelForWeapon( m_iRightWeaponID, false ) );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( left )
		m_pPlayer->ViewPunch( 10, 4, 4 );
	else
		m_pPlayer->ViewPunch( -10, 4, 4 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.2, 0.15, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 6.0, 0.35, 0.15, 2.0 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick = 8;

	if ( left )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageL, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageR, m_pPlayer->pev, m_pPlayer->random_seed );

	if ( left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 8.0 );

	int pitch = 95 + RANDOM_LONG( 0, 10 );

	PlayWeaponSound_guess( "weapons_new/revolver/revolver_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	if ( left )
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponLeftAmmo--;

		m_flNextPrimaryAttack = GetAttackDelayForWeapon( m_iLeftWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}
	else
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponRightAmmo--;

		m_flNextSecondaryAttack = GetAttackDelayForWeapon( m_iRightWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}

	if ( left )
	{
		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
	}
	else
	{
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 3 );
		else
			SendWeaponAnim( 3 );
	}

	if ( left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualFlash, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CDualWeapon::StartVP70Shoot( bool left )
{
	if ( left )
		vp70left = true;
	else
		vp70left = false;

	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
			return;
	}
	else
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
			return;
	}

	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo <= 0 )
		{
			if ( gDualWeapon.m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				gDualWeapon.m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
			return;
		}
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo <= 0 )
		{
			if ( m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				m_fEmptyClickTime = gpGlobals->time + 0.8;
			}
			return;
		}
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.8;
		}
		return;
	}

	m_iCurrentShot = 0;
	VP70Shoot();
}

void CDualWeapon::VP70Shoot( void )
{
	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( vp70left )
	{
		if ( m_pPlayer->m_iDualWeaponLeftAmmo <= 0 )
		{
			if ( gDualWeapon.m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				gDualWeapon.m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
			return;
		}
	}
	else
	{
		if ( m_pPlayer->m_iDualWeaponRightAmmo <= 0 )
		{
			if ( gDualWeapon.m_fEmptyClickTime < gpGlobals->time )
			{
				PlayEmptySound( );
				gDualWeapon.m_fEmptyClickTime = gpGlobals->time + 0.25;
			}
			return;
		}
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		if ( m_fEmptyClickTime < gpGlobals->time )
		{
			PlayEmptySound( );
			m_fEmptyClickTime = gpGlobals->time + 0.8;
		}
		return;
	}

	if ( vp70left )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 - gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}
	else
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}

	if ( !vp70left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( GetModelForWeapon( m_iRightWeaponID, false ) );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

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

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	float PlayerSpeed = m_pPlayer->pev->velocity.Length2D();

	m_flCurrentKick = 2;

	if ( vp70left )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageL, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageR, m_pPlayer->pev, m_pPlayer->random_seed );

	if ( vp70left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 2;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 2;

	m_pPlayer->ViewKnock( 3.0 );

	int pitch = 95 + RANDOM_LONG( 0, 10 );

	PlayWeaponSound_guess( "weapons_new/vp70/vp70_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	if ( vp70left )
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponLeftAmmo--;

		m_flNextPrimaryAttack = GetAttackDelayForWeapon( m_iLeftWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}
	else
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponRightAmmo--;

		m_flNextSecondaryAttack = GetAttackDelayForWeapon( m_iRightWeaponID ) + gpGlobals->time;
		UpdateWeaponAmmo();
	}

	if ( vp70left )
		gDualWeapon.SendWeaponAnim( m_pPlayer, 4 );
	else
		SendWeaponAnim( 4 );

	if ( vp70left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualFlash, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	m_iCurrentShot++;

	if ( m_iCurrentShot < 3 )
	{
		if ( !vp70left )
		{
			SetNextThink( 0.027 );
			SetThink( &CDualWeapon::VP70Shoot );
		}
		else
		{
			gDualWeapon.m_flThinkDecay = gpGlobals->time + 0.027;
			gDualWeapon.m_iThinkState = 11;
		}
	}
}

void CDualWeapon::GlockShoot( bool left )
{
	if ( m_iSprintState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( ( m_pPlayer->pev->flags & (1<<18) ) || !( m_pPlayer->pev->flags & FL_ONGROUND ) )
		return;

	if ( left )
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK ) )
			return;
	}
	else
	{
		if ( !( m_pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
			return;
	}

	if ( left )
	{
		if ( m_flNextPrimaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo <= 0 )
		{
			PlayEmptySound( );
			m_flNextPrimaryAttack = gpGlobals->time + 0.8;
			return;
		}
	}
	else
	{
		if ( m_flNextSecondaryAttack > gpGlobals->time )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo <= 0 )
		{
			PlayEmptySound( );
			m_flNextSecondaryAttack = gpGlobals->time + 0.8;
			return;
		}
	}

	// don't fire underwater
	if ( m_pPlayer->pev->waterlevel == 3 )
	{
		PlayEmptySound( );

		if ( left )
		{
			m_flNextPrimaryAttack = gDualWeapon.m_flNextIdleTime = gpGlobals->time + 0.25;
			return;
		}
		else
		{
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 0.25;
			return;
		}
	}

	if ( left )
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 - gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}
	else
	{
		Vector vecShellVelocity = gpGlobals->v_right * 220 + gpGlobals->v_up * 200 + gpGlobals->v_forward * 40;
		Vector vecShellOrigin = pev->origin + pev->view_ofs + gpGlobals->v_forward * 80 + gpGlobals->v_right * 20;
		EjectBrass( vecShellOrigin, vecShellVelocity, pev->angles.y, m_iShell, TE_BOUNCE_SHELL );
	}

	if ( !left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/gunsmoke.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( GetModelForWeapon( m_iRightWeaponID, false ) );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( left )
		m_pPlayer->ViewPunch( 6, 2, 2 );
	else
		m_pPlayer->ViewPunch( -6, 2, 2 );

	if ( m_pPlayer->pev->flags & FL_DUCKING )
		UTIL_ScreenShake( m_pPlayer->pev->origin, 2.0, 0.1, 0.1, 2.0 );
	else
		UTIL_ScreenShake( m_pPlayer->pev->origin, 5.0, 0.25, 0.15, 2.0 );

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle );

	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_10DEGREES );

	Vector vecDir;

	if ( left )
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageL, m_pPlayer->pev, m_pPlayer->random_seed );
	else
		vecDir = m_pPlayer->FireBulletsPlayer( 1, vecSrc, vecAiming, m_AccuracyVector, 8192, BULLET_PLAYER_357, 2, m_iDamageR, m_pPlayer->pev, m_pPlayer->random_seed );

	if ( left )
		gDualWeapon.m_flNextIdleTime = gpGlobals->time + 1;
	else
		m_flTimeWeaponIdle = gpGlobals->time + 1;

	m_pPlayer->ViewKnock( 5.0 );

	int pitch = 95 + RANDOM_LONG( 0, 10 );

	if ( left )
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponLeftAmmo--;

		PlayWeaponSound_guess( "weapons_new/glock/glock_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		UpdateWeaponAmmo();
	}
	else
	{
		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_iDualWeaponRightAmmo--;

		PlayWeaponSound_guess( "weapons_new/glock/glock_fire.wav", 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
		UpdateWeaponAmmo();
	}

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_COMBAT;
	m_pPlayer->m_flStopExtraSoundTime = gpGlobals->time + 0.2;

	if ( left )
	{
		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 2 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );
	}
	else
	{
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 2 );
		else
			SendWeaponAnim( 3 );
	}

	if ( left )
		m_flNextPrimaryAttack = GetAttackDelayForWeapon( m_iLeftWeaponID ) + gpGlobals->time;
	else
		m_flNextSecondaryAttack = GetAttackDelayForWeapon( m_iRightWeaponID ) + gpGlobals->time;

	if ( left )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualFlash, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CDualWeapon::FlareDoIgnite( void )
{
	CFlare *pFlare = NULL;
	pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

	if ( !pFlare )
		return;

	pFlare->pev->radsuit_finished = gpGlobals->time + gSkillData.flareBurnTime;
	pFlare->pev->iuser1 = 1;

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/flare/flare_ignite.wav", 1, 0.8 );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, 0.8 );

	pFlare->m_fNextBurnTime = gpGlobals->time + 6.9;
	m_pPlayer->m_iFlashlightFlags |= 16;


	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/signal.aur" );
		WRITE_BYTE( 1 );
		if ( pFlare->custom_v_model )
			WRITE_STRING( STRING( pFlare->custom_v_model ) );
		else
			WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();


	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/blacksmoke.aur" );
		WRITE_BYTE( 1 );
		if ( pFlare->custom_v_model )
			WRITE_STRING( STRING( pFlare->custom_v_model ) );
		else
			WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/flare_sparks.aur" );
		WRITE_BYTE( 1 );
		if ( pFlare->custom_v_model )
			WRITE_STRING( STRING( pFlare->custom_v_model ) );
		else
			WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/flare_glow.aur" );
		WRITE_BYTE( 1 );
		if ( pFlare->custom_v_model )
			WRITE_STRING( STRING( pFlare->custom_v_model ) );
		else
			WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 2 );
	MESSAGE_END();
}

void CDualWeapon::FlareIgnite( bool left )
{
	if ( left )
		return;

	if ( !m_pPlayer )
		return;

	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	CFlare *pFlare = NULL;
	pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

	if ( pFlare )
	{
		if ( pFlare->pev->iuser1 > 0 )
		{
			SendWeaponAnim( 8 );
			SetNextThink( 0.9 );
			SetThink( &CDualWeapon::ThrowRemove );
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1.5;
			pFlare->m_bThrowing = true;
		}
		else
		{
			SetNextThink( 0.86 );
			SetThink( &CDualWeapon::FlareDoIgnite );

			SendWeaponAnim( 15 );
			m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		}
	}
}

void CDualWeapon::TMPShoot( bool left )
{
}

void CDualWeapon::ReloadFinishLeft( void )
{
	int j;

	switch ( m_iLeftWeaponID )
	{
	case 3:		// glock
		j = min( 15 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] );

		m_pPlayer->m_iDualWeaponLeftAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 6 );
		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/glock/glock_magin.wav", 1, 0, 0, 100 );
		break;

	case 7:		// p345
		j = min( 8 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] );

		m_pPlayer->m_iDualWeaponLeftAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 6 );
		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/p345/p345_magin.wav", 1, 0, 0, 100 );
		break;

	case 19:	// vp70
		j = min( 8 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] );

		m_pPlayer->m_iDualWeaponLeftAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 1 );
		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/vp70/vp70_magin.wav", 1, 0, 0, 100 );
		break;
	}
}

void CDualWeapon::ReloadFinishRight( void )
{
	int j;

	switch ( m_iRightWeaponID )
	{
	case 3:		// glock
		j = min( 15 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] );

		m_pPlayer->m_iDualWeaponRightAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		SendWeaponAnim( 6 );
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/glock/glock_magin.wav", 1, 0, 0, 100 );
		break;

	case 7:		// p345
		j = min( 8 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] );

		m_pPlayer->m_iDualWeaponRightAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		SendWeaponAnim( 6 );
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/p345/p345_magin.wav", 1, 0, 0, 100 );
		break;

	case 19:	// vp70
		j = min( 8 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] );

		m_pPlayer->m_iDualWeaponRightAmmo += j;
		m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] -= j;

		m_pPlayer->TabulateAmmo();

		m_fInReload = FALSE;

		SendWeaponAnim( 1 );
		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;

		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/vp70/vp70_magin.wav", 1, 0, 0, 100 );
		break;
	}
}

void CDualWeapon::Reload( void )
{
	int j;
	char CurrentPath[260];

	GetCurrentPath( CurrentPath );

	if ( !strstr( CurrentPath, "editor321@planethalflife.com" ) )
		return;

	if ( m_iSprintState == 1 || m_iSprintState > 2 )
		return;

	if ( gDualWeapon.m_iSprintState == 1 || gDualWeapon.m_iSprintState > 2 )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( IsMelee( m_iLeftWeaponID ) && IsMelee( m_iRightWeaponID ) )
		return;

	j = 0;

	switch ( m_iRightWeaponID )
	{
	case 3:		// glock
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] <= 0 )
			return;

		if ( m_iLeftWeaponID != 6 )
			return;

		j = min( 15 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] );

		if ( j == 0 )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 8 );
		else
			SendWeaponAnim( 9 );

		m_pPlayer->m_iDualWeaponRightAmmo = 0;

		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/glock/glock_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishRight );
		SetNextThink( 1 );
		break;

	case 19:	// vp70
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] <= 0 )
			return;

		if ( m_iLeftWeaponID != 6 )
			return;

		j = min( 8 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] );

		if ( j == 0 )
			return;

		SendWeaponAnim( 3 );

		m_pPlayer->m_iDualWeaponRightAmmo = 0;

		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/vp70/vp70_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishRight );
		SetNextThink( 1 );
		break;

	case 7:		// p345
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] <= 0 )
			return;

		if ( m_iLeftWeaponID != 6 )
			return;

		j = min( 8 - m_pPlayer->m_iDualWeaponRightAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] );

		if ( j == 0 )
			return;

		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 8 );
		else
			SendWeaponAnim( 9 );

		m_pPlayer->m_iDualWeaponRightAmmo = 0;

		m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/p345/p345_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishRight );
		SetNextThink( 1 );
		break;
	}

	switch ( m_iLeftWeaponID )
	{
	case 3:		// glock
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] <= 0 )
			return;

		if ( m_iRightWeaponID != 6 )
			return;

		j = min( 15 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "glock" )] );

		if ( j == 0 )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 9 );

		m_pPlayer->m_iDualWeaponLeftAmmo = 0;

		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/glock/glock_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishLeft );
		SetNextThink( 1 );
		break;

	case 19:	// vp70
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] <= 0 )
			return;

		if ( m_iRightWeaponID != 6 )
			return;

		j = min( 8 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "vp70" )] );

		if ( j == 0 )
			return;

		gDualWeapon.SendWeaponAnim( m_pPlayer, 3 );

		m_pPlayer->m_iDualWeaponLeftAmmo = 0;

		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/vp70/vp70_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishLeft );
		SetNextThink( 1 );
		break;

	case 7:		// p345
		if ( m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] <= 0 )
			return;

		if ( m_iRightWeaponID != 6 )
			return;

		j = min( 8 - m_pPlayer->m_iDualWeaponLeftAmmo, m_pPlayer->m_rgAmmo[m_pPlayer->GetAmmoIndex( "p345" )] );

		if ( j == 0 )
			return;

		if ( m_pPlayer->m_iDualWeaponLeftAmmo )
			gDualWeapon.SendWeaponAnim( m_pPlayer, 8 );
		else
			gDualWeapon.SendWeaponAnim( m_pPlayer, 9 );

		m_pPlayer->m_iDualWeaponLeftAmmo = 0;

		gDualWeapon.m_flNextPrimaryAttack = m_flNextPrimaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 2;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/p345/p345_magout.wav", 1, 0, 0, 100 );
		SetThink( &CDualWeapon::ReloadFinishLeft );
		SetNextThink( 1 );
		break;
	}
}

void CDualWeapon::WeaponIdle( void )
{
	CFlare *pFlare = NULL;

	m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

	if ( m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( gDualWeapon.m_iSprintState )
		return;

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	switch ( m_iRightWeaponID )
	{
	case 1:		// flashlight
		SendWeaponAnim( 0 );
		break;

	case 2:		// mobile
		if ( m_pPlayer->m_iMobileFlashMode )
			SendWeaponAnim( 5 );
		else
			SendWeaponAnim( 0 );
		break;

	case 3:		// glock
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 0 );
		else
			SendWeaponAnim( 1 );
		break;

	case 5:		// camera
		break;
	case 6:		// switchblade
		SendWeaponAnim( 5 );
		break;
	case 10:	// lantern
		SendWeaponAnim( 2 );
		return;

	case 19:	// vp70
		SendWeaponAnim( 0 );
		break;
	case 21:	// revolver
		SendWeaponAnim( 0 );
		break;
	case 7:		// p345
		if ( m_pPlayer->m_iDualWeaponRightAmmo )
			SendWeaponAnim( 0 );
		else
			SendWeaponAnim( 1 );
		break;

	case 12:	// nightstick
		SendWeaponAnim( 0 );
		break;
	case 16:	// branch
		SendWeaponAnim( 15 );
		break;

	case 14:	// tmp
		break;
	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->iuser1 == 1 )
				SendWeaponAnim( 5 );
			else
				SendWeaponAnim( 2 );
		}
		return;

	default:
		return;
	}

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

int GetSprintFrom( int weapon, bool left, CBasePlayer *m_pPlayer )
{
	CFlare *pFlare = NULL;

	switch ( weapon )
	{
	case 1:		// flashlight
		return 6;

	case 2:		// mobile
		return -1;

	case 3:		// glock
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 14;
			else
				return 15;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 14;
			else
				return 15;
		}

	case 5:		// camera
		return -1;

	case 6:		// switchblade
		return 15;

	case 10:	// lantern
		return 6;

	case 19:	// vp70
		if ( left )
			return 15;
		else
			return 15;

	case 7:		// p345
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 12;
			else
				return 15;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 12;
			else
				return 15;
		}

	case 21:	// revolver
		if ( left )
			return 14;
		else
			return 14;

	case 12:	// nightstick
		return 12;

	case 16:	// branch
		return 7;

	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->iuser1 == 1 )
				return 14;
			else
				return 11;
		}
		break;
	}

	return -1;
}

int GetSprintTo( int weapon, bool left, CBasePlayer *m_pPlayer )
{
	CFlare *pFlare = NULL;

	switch ( weapon )
	{
	case 1:		// flashlight
		return 4;

	case 2:		// mobile
		return -1;

	case 3:		// glock
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 10;
			else
				return 11;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 10;
			else
				return 11;
		}

	case 5:		// camera
		return -1;

	case 6:		// switchblade
		return 13;

	case 10:	// lantern
		return 5;

	case 21:	// revolver
		return 12;

	case 7:		// p345
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 10;
			else
				return 13;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 10;
			else
				return 13;
		}

	case 19:	// vp70
		if ( left )
			return 13;
		else
			return 13;

	case 12:	// nightstick
		return 10;

	case 16:	// branch
		return 5;

	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->iuser1 == 1 )
				return 12;
			else
				return 9;
		}
		break;
	}

	return -1;
}

int GetSprintIdle( int weapon, bool left, CBasePlayer *m_pPlayer )
{
	CFlare *pFlare = NULL;

	switch ( weapon )
	{
	case 1:		// flashlight
		return 5;

	case 2:		// mobile
		return -1;

	case 3:		// glock
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 12;
			else
				return 13;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 12;
			else
				return 13;
		}

	case 5:		// camera
		return -1;

	case 6:		// switchblade
		return 14;

	case 10:	// lantern
		return 1;

	case 21:	// revolver
		return 13;

	case 7:		// p345
		if ( left )
		{
			if ( m_pPlayer->m_iDualWeaponLeftAmmo )
				return 11;
			else
				return 14;
		}
		else
		{
			if ( m_pPlayer->m_iDualWeaponRightAmmo )
				return 11;
			else
				return 14;
		}

	case 19:	// vp70
		if ( left )
			return 14;
		else
			return 14;

	case 12:	// nightstick
		return 11;

	case 16:	// branch
		return 6;

	case 13:	// flare
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->iuser1 == 1 )
				return 13;
			else
				return 10;
		}
		break;
	}

	return -1;
}

void CDualWeapon::ItemPostFrame( void )
{
	if ( m_pPlayer )
	{
		CBasePlayerWeapon *Left = m_pPlayer->GetPlayerWeapon( m_pPlayer->m_iDualWeaponLeftID );
		CBasePlayerWeapon *Right = m_pPlayer->GetPlayerWeapon( m_pPlayer->m_iDualWeaponRightID );

		if ( gDualWeapon.HasDualWeapon( m_pPlayer ) )
		{
			if ( !gDualWeapon.m_pPlayer )
				gDualWeapon.m_pPlayer = m_pPlayer;

			if ( gDualWeapon.m_pPlayer )
				gDualWeapon.WeaponThink( m_pPlayer );
		}

		if ( !Left || !Right )
		{
			if ( m_pPlayer )
			{
				m_pPlayer->pev->weapons &= ~( 1 << m_iId );

				m_pPlayer->RemovePlayerItem( this );
			}

			m_pPlayer->m_sDualWieldViewModel = 0;
			Kill();

			if ( Right )
			{
				Right->Deploy();
				Right->UpdateItemInfo();
				m_pPlayer->m_pActiveItem = Right;
			}
			else if ( Left )
			{
				Left->Deploy();
				Left->UpdateItemInfo();
				m_pPlayer->m_pActiveItem = Left;
			}
			else
				m_pPlayer->SelectRandomWeapon();

			return;
		}
	}

	if ( m_iRightWeaponID == 13 && m_pPlayer )
	{
		CFlare *pFlare = NULL;
		pFlare = (CFlare *)m_pPlayer->GetPlayerWeapon( 13 );

		if ( pFlare )
		{
			if ( pFlare->pev->radsuit_finished < gpGlobals->time && pFlare->pev->iuser1 == 1 )
			{
				m_pPlayer->m_iFlashlightFlags &= ~16;
				pFlare->pev->iuser1 = 2;
				pev->body = 0;
				STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
					WRITE_SHORT( entindex() );
					WRITE_STRING( "aurora/flare_extinguish.aur" );
					WRITE_BYTE( 1 );
					if ( pFlare->custom_v_model )
						WRITE_STRING( STRING( pFlare->custom_v_model ) );
					else
						WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
					WRITE_SHORT( 0 );
				MESSAGE_END();

				if ( !pFlare->m_bThrowing )
				{
					SendWeaponAnim( 4 );
					m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 0.4;
				}
			}

			if ( pFlare->pev->iuser1 == 1 )
			{
				if ( !( m_pPlayer->m_iFlashlightFlags & 16 ) )
					m_pPlayer->m_iFlashlightFlags |= 16;

				if ( pFlare->m_fNextBurnTime < gpGlobals->time )
				{
					EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, 0.8 );
					pFlare->m_fNextBurnTime = gpGlobals->time + 6.9;
				}
			}
			else
			{
				if ( m_pPlayer->m_iFlashlightFlags & 16 )
					m_pPlayer->m_iFlashlightFlags &= ~16;
			}
		}
	}

	if ( m_iLeftWeaponID == 2 || m_iRightWeaponID == 2 )
	{
		if ( m_iLeftWeaponID == 2 )
		{
			if ( gDualWeapon.dualbody != m_pPlayer->m_iPhoneMessage )
			{
				gDualWeapon.dualbody = m_pPlayer->m_iPhoneMessage;

				MESSAGE_BEGIN( MSG_ONE, gmsgDualWield, NULL, m_pPlayer->pev );
					WRITE_STRING( STRING( m_pPlayer->m_sDualWieldViewModel ) );
					WRITE_BYTE( gDualWeapon.dualbody );
					WRITE_BYTE( m_pPlayer->m_iDualWeaponLeftID );
					WRITE_BYTE( m_pPlayer->m_iDualWeaponRightID );
				MESSAGE_END();
			}
		}

		if ( m_iRightWeaponID == 2 )
		{
			if ( pev->body != m_pPlayer->m_iPhoneMessage )
			{
				pev->body = m_pPlayer->m_iPhoneMessage;
				SendWeaponBody( m_pPlayer, pev->body );
			}
		}

		if ( m_pPlayer->m_UnreadSMS == 1 && !m_pPlayer->IsInCutscene() && m_pPlayer->GetPlayerWeapon( 2 ) )
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

			MessageGet( m_pPlayer->m_iPhoneMessage );

			m_pPlayer->m_UnreadSMS = false;
		}
	}

	if ( m_fNextAccuracyTime < gpGlobals->time )
	{
		if ( m_flNextPrimaryAttack < gpGlobals->time && m_flNextSecondaryAttack < gpGlobals->time && m_flNextTertiaryAttack < gpGlobals->time )
		{
			if ( m_Accuracy > 0 )
				m_Accuracy -= 0.75;

			if ( m_Accuracy < 0 )
				m_Accuracy = 0;
		}

		m_fNextAccuracyTime = gpGlobals->time + 0.1;
	}

	if ( m_iSprintState == 2 && !m_fInReload )
	{
		gDualWeapon.m_iSprintState = 0;
		m_iSprintState = 0;
	}

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( m_Accuracy < 1 )
		m_AccuracyVector = Vector( 0, 0, 0 );
	else if ( m_Accuracy >= 1 && m_Accuracy < 2 )
		m_AccuracyVector = VECTOR_CONE_1DEGREES;
	else if ( m_Accuracy >= 2 && m_Accuracy < 3 )
		m_AccuracyVector = VECTOR_CONE_3DEGREES;
	else if ( m_Accuracy >= 3 && m_Accuracy < 4 )
		m_AccuracyVector = VECTOR_CONE_6DEGREES;
	else if ( m_Accuracy >= 4 )
		m_AccuracyVector = VECTOR_CONE_10DEGREES;
	else
		m_AccuracyVector = Vector( 0, 0, 0 );

	CBasePlayerWeapon::ItemPostFrame();
}
