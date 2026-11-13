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

LINK_ENTITY_TO_CLASS( weapon_syringe, CSyringe );

extern int gmsgSyringe;
extern int gmsgQSlotClear;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// sequences of models/weapons/syringe/v_syringe.mdl
enum syringe_e
{
	SYRINGE_IDLE = 0,
	SYRINGE_DEPLOY,
	SYRINGE_HOLSTER,
	SYRINGE_USE,
	SYRINGE_STAB,
	SYRINGE_STAB_MISS,
};


void CSyringe::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_syringe" );
	Precache( );
	m_iId = 8;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/syringe/w_syringe.mdl" );

	pHealVictim = NULL;

	if ( pev->frags > 0 )
		m_iDefaultAmmo = pev->frags;
	else
		m_iDefaultAmmo = 1;

	FallInit();// get ready to fall down.

	m_iSprintIdleAnim1 = 7;
	m_iSprintIdleAnim2 = 666;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_syringe.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/syringe/w_syringe.mdl" );
}

void CSyringe::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/syringe/v_syringe.mdl" );
	PRECACHE_MODEL( "models/weapons/syringe/w_syringe.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/syringe/syringe_inject.wav" );
	PRECACHE_SOUND( "weapons/syringe/syringe_insert.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		PRECACHE_MODEL( "police/models/weapons/syringe/p_syringe.mdl" );
}

int CSyringe::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "syringe";
	p->iMaxAmmo1 = 10;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 0;
	p->iId = 8;
	p->iWeight = 1;
	p->iFlags = 64;
	return 1;
}

BOOL CSyringe::Deploy( )
{
	pHealVictim = NULL;

	m_iSprintState = 0;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;

		UTIL_ProMessage( m_pPlayer, "Looks like morphine. This is strong stuff, so I'd better be careful with it.", 5, false );
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return DefaultDeploy( "models/weapons/syringe/v_syringe.mdl", "police/models/weapons/syringe/p_syringe.mdl", SYRINGE_DEPLOY, "crowbar", 1 );

	return DefaultDeploy( "models/weapons/syringe/v_syringe.mdl", "models/null.mdl", SYRINGE_DEPLOY, "crowbar", 1 );
}

void CSyringe::Holster( )
{
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	SetThink( NULL );
	SendWeaponAnim( SYRINGE_HOLSTER );
}

void CSyringe::Redeploy( void )
{
	SendWeaponAnim( SYRINGE_DEPLOY );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
}

void CSyringe::DestroyItem( void )
{
	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= 1 )
	{
		Redeploy();
		return;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
			WRITE_BYTE( ENTINDEX( m_pPlayer->edict() ) );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if ( m_pPlayer && m_pPlayer->m_pActiveItem && m_pPlayer->m_pActiveItem == this )
	{
		if ( m_pPlayer->m_pLastItem )
			m_pPlayer->SelectLastItem( true );
	}

	if ( m_pPlayer )
	{
		// clear the quick slots that hold this item
		if ( !strcmp( m_pPlayer->m_sQuickSel1, STRING( pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel1, "" );

			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[1] = 0;
		}

		if ( !strcmp( m_pPlayer->m_sQuickSel2, STRING( pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel2, "" );

			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 2 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[2] = 0;
		}

		if ( !strcmp( m_pPlayer->m_sQuickSel3, STRING( pev->classname ) ) )
		{
			strcpy( m_pPlayer->m_sQuickSel3, "" );
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, m_pPlayer->pev );
				WRITE_BYTE( 3 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_pPlayer->m_iQuickSlotSlot[3] = 0;
		}

		m_pPlayer->pev->weapons &= ~( 1 << m_iId );
		m_pPlayer->HasItemInSlot( (char *)STRING( script ), true );

		m_pPlayer->RemovePlayerItem( this );
	}

	Kill();
}

void CSyringe::Inject( void )
{
	CBasePlayer *pPlayerToHeal = NULL;

	if ( pHealVictim )
		pPlayerToHeal = pHealVictim;
	else
		pPlayerToHeal = m_pPlayer;

	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		if ( pPlayerToHeal->m_fMorphineTripTime > gpGlobals->time && !pPlayerToHeal->m_bIsTripping )
			pPlayerToHeal->MorphineTrip();
	}

	m_pPlayer->playerstats.syringes_used++;

	pPlayerToHeal->m_iMorphineRecover = gSkillData.syringeHeal;
	pPlayerToHeal->MorphineSound();
	pPlayerToHeal->m_fMorphineTripTime = CVAR_GET_FLOAT( "sv_morphinetriptime" ) + gpGlobals->time;

	if ( !m_pPlayer->m_bInfiniteAmmo )
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= 1 )
	{
		SetThink( &CSyringe::Redeploy );
		SetNextThink( 1.5 );
		b_HasUsed = true;
	}
	else
	{
		SetThink( &CSyringe::DestroyItem );
		SetNextThink( 1.5 );
		b_HasUsed = true;
	}
}

void CSyringe::BladeRetract( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		m_pPlayer->pev->fuser3 = 0;
		m_pPlayer->ViewLock( false, 0 );

		if ( pHealVictim )
		{
			if ( pHealVictim->IsAlive() )
			{
				pHealVictim->pev->fuser3 = 0;
				pHealVictim->ViewLock( false, 0 );
			}
			else
			{
				pHealVictim->ViewLock( false, 0 );
				pHealVictim->RevivePlayer();

				m_pPlayer->playerstats.syringes_used++;

				if ( !m_pPlayer->m_bInfiniteAmmo )
					m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

				if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
					m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;

				if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= 1 )
				{
					SetThink( &CSyringe::Redeploy );
					SetNextThink( 1.5 );
					b_HasUsed = true;
				}
				else
				{
					SetThink( &CSyringe::DestroyItem );
					SetNextThink( 1.5 );
					b_HasUsed = true;
				}

				return;
			}
		}
	}

	Inject();
}

void CSyringe::SecondaryAttack( void )
{
	if ( m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		return;

	pHealVictim = NULL;

	entvars_t *pevStart = m_pPlayer->pev;
	Vector vecStart = pevStart->origin + m_pPlayer->pev->view_ofs;
	entvars_t *pevEnd = m_pPlayer->pev;
	Vector vecEnd = pevEnd->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 48;

	TraceResult tr;
	UTIL_MakeVectors( m_pPlayer->pev->angles );
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	UTIL_TraceLine( vecStart, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	CBaseEntity *pHitEntity = NULL;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pCast = UTIL_PlayerByIndex( i );

			if ( pCast && pCast->IsAlive() )
			{
				TraceResult trace;
				UTIL_TraceModel( vecStart, vecEnd, head_hull, ENT( pCast->pev ), &trace );

				if ( trace.pHit )
				{
					CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

					if ( pEntity )
						pHitEntity = pEntity;
				}
			}
		}
	}

	if ( tr.flFraction != 1.0 || pHitEntity )
	{
		CBaseEntity *pEntity = NULL;

		if ( pHitEntity )
			pEntity = pHitEntity;
		else
			pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity && !pEntity->IsPlayer() )
		{
			// revive a dead player lying next to what was hit
			CBaseEntity *pSphereEnt = NULL;

			while ( ( pSphereEnt = UTIL_FindEntityInSphere( pSphereEnt, tr.vecEndPos, 64 ) ) != NULL )
			{
				if ( pSphereEnt && pSphereEnt->IsPlayer() && !pSphereEnt->IsAlive() )
				{
					CBasePlayer *pHealed = GetClassPtr( (CBasePlayer *)pSphereEnt->pev );

					if ( pHealed && !pHealed->m_bCantBeRevived )
					{
						SendWeaponAnim( SYRINGE_STAB );

						m_pPlayer->pev->fuser3 = 25;
						m_pPlayer->ViewLock( true, 666 );

						m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 8;
						m_flTimeWeaponIdle = gpGlobals->time + 2.8;

						m_pPlayer->PlayerAnimation( "syringe_heal_player_hit", false );

						pHealVictim = pHealed;

						SetNextThink( 2.16 );
						SetThink( &CSyringe::BladeRetract );
						return;
					}
				}
			}
		}

		if ( pEntity && pEntity->IsPlayer() )
		{
			CBasePlayer *pHealed = GetClassPtr( (CBasePlayer *)pEntity->pev );

			if ( pHealed )
			{
				if ( pHealed->IsAlive() && pHealed->pev->health < 100 )
				{
					m_pPlayer->pev->fuser3 = 25;
					m_pPlayer->ViewLock( true, 666 );
					SendWeaponAnim( SYRINGE_STAB );
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 8;
					m_flTimeWeaponIdle = gpGlobals->time + 2.8;

					m_pPlayer->PlayerAnimation( "syringe_heal_player_hit", false );

					pHealVictim = pHealed;

					pHealed->pev->fuser3 = 25;
					pHealed->ViewLock( true, 666 );

					SetNextThink( 1.5 );
					SetThink( &CSyringe::BladeRetract );
				}
			}
		}
		else
		{
			SendWeaponAnim( SYRINGE_STAB_MISS );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
			m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		}
	}
	else
	{
		SendWeaponAnim( SYRINGE_STAB_MISS );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 1;
		m_flTimeWeaponIdle = gpGlobals->time + 1.5;
	}
}

void CSyringe::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	pHealVictim = NULL;

	SendWeaponAnim( SYRINGE_USE );

	m_pPlayer->PlayerAnimation( "syringe_use", false );

	SetNextThink( 2 );
	SetThink( &CSyringe::Inject );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->time + 5;
}

void CSyringe::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() ) return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	SendWeaponAnim( SYRINGE_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CSyringe::ItemPostFrame( void )
{
	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( 6 );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( 8 );

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
				SendWeaponAnim( 6 );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( m_pPlayer->pev->flags & (1<<18) ) && speed > 0 && m_iSprintState != 5 )
		{
			float speed = m_pPlayer->pev->velocity.Length2D();

			if ( m_iSprintState == 1 && m_iNextSprintAnim < gpGlobals->time && speed != 0 )
			{
				SendWeaponAnim( 7 );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( 8 );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_flNextPrimaryAttack = time + gpGlobals->time;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
