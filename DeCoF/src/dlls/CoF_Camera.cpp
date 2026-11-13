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

extern int gmsgCustomDLight;
extern int gmsgTakePhoto;

class CCameraSpot : public CBaseEntity
{
	void Spawn( void );
};

void CCameraSpot::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( camera_spot, CCameraSpot );


LINK_ENTITY_TO_CLASS( weapon_camera, CCamera );

// sequences of models/weapons/camera/v_camera.mdl
enum camera_e
{
	CAMERA_IDLE = 0,
	CAMERA_DRAW_FIRST,
	CAMERA_HOLSTER,
	CAMERA_SHOOT,
	CAMERA_FIDGET1,
	CAMERA_JUMP_TO,
	CAMERA_JUMP_FROM,
	CAMERA_DRAW,
	CAMERA_FIDGET2,
	CAMERA_FIDGET3,
	CAMERA_SPRINT_TO,
	CAMERA_SPRINT_IDLE,
	CAMERA_SPRINT_FROM,
	CAMERA_MELEE,
};


void CCamera::SendInitMessage( CBasePlayer *player )
{
	if ( player && !m_bCheckedIfShouldBeDeleted && ( pev->spawnflags & 1 ) )
	{
		m_bCheckedIfShouldBeDeleted = true;

		// remove the camera if this unlockable is not available
		if ( !player->CheckingEndingOrUnlockables( 0x100000, false, false ) )
			UTIL_Remove( this );
	}
}

void CCamera::Fidget( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time || m_flTimeWeaponIdle > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( RANDOM_LONG( 0, 599 ) != 50 )
		return;

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		SendWeaponAnim( CAMERA_FIDGET1 );
		break;

	case 1:
		SendWeaponAnim( CAMERA_FIDGET2 );
		break;

	case 2:
		SendWeaponAnim( CAMERA_FIDGET3 );
		break;
	}
}

void CCamera::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_camera" );
	Precache( );
	m_iId = 5;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/camera/w_camera.mdl" );

	m_iClip = -1;
	FallInit();// get ready to fall down.

	m_iSprintIdleAnim1 = CAMERA_SPRINT_IDLE;
	m_iSprintIdleAnim2 = 666;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_camera.txt" );

	if ( custom_script )
		script = custom_script;

	pev->renderfx = 99;

	strwmodel = MAKE_STRING( "models/weapons/camera/w_camera.mdl" );
}

void CCamera::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/camera/v_camera.mdl" );
	PRECACHE_MODEL( "models/weapons/camera/w_camera.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );
	PRECACHE_SOUND( "weapons/camera/flash_charge.wav" );
	PRECACHE_SOUND( "weapons/camera/take_photo.wav" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );
}

int CCamera::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = NULL;
	p->iMaxAmmo1 = -1;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 6;
	p->iPosition = 0;
	p->iId = 5;
	p->iWeight = 1;
	return 1;
}

BOOL CCamera::Deploy( )
{
	m_iSprintState = 0;

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		return DefaultDeploy( "models/weapons/camera/v_camera.mdl", "models/null.mdl", CAMERA_DRAW_FIRST, "crowbar" );
	}

	return DefaultDeploy( "models/weapons/camera/v_camera.mdl", "models/null.mdl", CAMERA_DRAW, "crowbar" );
}

void CCamera::Holster( )
{
	SendWeaponAnim( CAMERA_HOLSTER );
	m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
}

void CCamera::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/camera/take_photo.wav", 1, ATTN_NORM );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/camera/flash_charge.wav", 1, ATTN_NORM );

	MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, m_pPlayer->pev->origin );
		WRITE_COORD( m_pPlayer->pev->origin.x );
		WRITE_COORD( m_pPlayer->pev->origin.y );
		WRITE_COORD( m_pPlayer->pev->origin.z );
		WRITE_BYTE( 40 );
		WRITE_BYTE( 1 );
		WRITE_BYTE( 30 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgTakePhoto, NULL, m_pPlayer->pev );
	MESSAGE_END();

	SendWeaponAnim( CAMERA_SHOOT );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;

	if ( pev->spawnflags & 1 )
	{
		// hurt every monster in view
		CBaseEntity *pEntity = NULL;

		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, m_pPlayer->GetGunPosition(), 1024 ) ) != NULL )
		{
			if ( !pEntity->IsPlayer() && m_pPlayer->FInViewCone( pEntity ) && m_pPlayer->FVisible( pEntity ) )
			{
				CBaseMonster *pMonster = pEntity->MyMonsterPointer();

				if ( pMonster && pMonster->pev->classname && strstr( STRING( pMonster->pev->classname ), "monster_" ) )
					pEntity->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, 0, (1<<28) );
			}
		}

		return;
	}

	TraceResult tr;
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );

	UTIL_TraceLine( m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs,
		m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 512,
		dont_ignore_monsters, m_pPlayer->edict(), &tr );

	if ( tr.pHit )
	{
		CBaseEntity *pEntity = NULL;

		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, tr.vecEndPos, 64 ) ) != NULL )
		{
			if ( pEntity )
			{
				if ( pEntity && FClassnameIs( pEntity->pev, "camera_spot_trigger" ) )
				{
					FireTargets( STRING( pEntity->pev->target ), m_pPlayer, m_pPlayer, USE_TOGGLE, 0 );
				}
				else if ( !pEntity->IsPlayer() )
				{
					CBaseMonster *pMonster = pEntity->MyMonsterPointer();

					if ( pMonster && pMonster->pev->classname && strstr( STRING( pMonster->pev->classname ), "monster_" ) )
						pMonster->TakeDamage( m_pPlayer->pev, m_pPlayer->pev, 0, (1<<28) );
				}
			}
		}
	}
}

void CCamera::SecondaryAttack( void )
{
}

void CCamera::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState )
		return;

	SendWeaponAnim( CAMERA_IDLE );
	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}

void CCamera::ItemPostFrame( void )
{
	if ( m_iSprintState == 2 && !m_fInReload )
		m_iSprintState = 0;

	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( !m_fInReload && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;
				SendWeaponAnim( CAMERA_JUMP_TO );

				m_flNextPrimaryAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
				m_iNextSprintAnim = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			SendWeaponAnim( CAMERA_JUMP_FROM );
			m_flNextPrimaryAttack = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
			m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
			m_iSprintState = 0;
		}

		if ( speed > 0 && ( m_pPlayer->pev->flags & (1<<18) ) && m_iSprintState != 5 )
		{
			if ( m_iSprintState != 1 )
			{
				m_iSprintState = 1;

				SendWeaponAnim( CAMERA_SPRINT_TO );
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
				SendWeaponAnim( CAMERA_SPRINT_IDLE );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				SendWeaponAnim( CAMERA_SPRINT_FROM );
				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + 0.33;
				m_flTimeWeaponIdle = gpGlobals->time + 0.35;
				m_iSprintState = 0;
			}
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}
