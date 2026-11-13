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
#include "weapons.h"
#include "player.h"
#include "gamerules.h"

// Cry of Fear: keeps the MP5 fire mode, clip and ammo across a level change
class CMP5ModeSaveRestoreMP5 : public CBaseEntity
{
public:
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_mp5modesaverestore, CMP5ModeSaveRestoreMP5 );

void CMP5ModeSaveRestoreMP5::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "type" ) )
	{
		pev->health = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CMP5ModeSaveRestoreMP5::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CMP5ModeSaveRestoreMP5::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !g_pGameRules )
		return;

	CHalfLifeRules *pRules = dynamic_cast<CHalfLifeRules *>( g_pGameRules );

	if ( !pRules || !pRules->m_bUnknownDC_guess )
		return;

	if ( pev->health == 1 )
	{
		// restore the weapon saved by the previous map
		CBaseEntity *pGun = CBaseEntity::Create( "weapon_mp5", pev->origin, pev->angles, NULL );

		if ( !pGun )
			return;

		CMp5 *pMp5 = dynamic_cast<CMp5 *>( pGun );

		if ( !pMp5 )
			return;

		float flAmmo = pRules->m_flUnknownE4_guess;

		pMp5->m_iClip = pRules->m_flUnknownE0_guess;
		pMp5->m_iDefaultAmmo = 0;

		if ( !( flAmmo <= 0 ) )
		{
			CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

			if ( pPlayer )
				pPlayer->GiveAmmo( flAmmo, "mp5", flAmmo, true );
		}
	}
	else
	{
		// save the weapon of the player
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_FindEntityByClassname( NULL, "player" );

		if ( !pPlayer )
			return;

		CBasePlayerWeapon *pWeapon = pPlayer->GetPlayerWeapon( 27 );

		if ( pWeapon )
		{
			int iAmmoIndex = CBasePlayer::GetAmmoIndex( "mp5" );
			int iAmmo = pPlayer->m_rgAmmo[iAmmoIndex];

			pRules->m_flUnknownE0_guess = pWeapon->m_iClip;
			pRules->m_flUnknownE4_guess = iAmmo;
		}
	}
}
