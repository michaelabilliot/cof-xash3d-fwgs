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

LINK_ENTITY_TO_CLASS( weapon_flare, CFlare );

extern int gmsgParticle;
extern int gmsgKillWpnPart;
extern int gmsgWpnInfo;
extern int gmsgQSlotClear;
extern bool m_gbIsInCutscene;

// sequences of models/weapons/flare/v_flare.mdl
enum flare_e
{
	FLARE_FLARE_DRAW = 0,
	FLARE_DRAW_LIT,
	FLARE_FLARE_IDLE,
	FLARE_FLARE_IGNITE,
	FLARE_LIT_TO_UNLIT,
	FLARE_FLARE_IDLE_LIT,
	FLARE_FLARE_HOLSTER,
	FLARE_FLARE_THROW,
	FLARE_THROW_ONEHANDED,
	FLARE_FLARE_SPRINT_UNLIT_TO,
	FLARE_FLARE_SPRINT_UNLIT_IDLE,
	FLARE_FLARE_SPRINT_UNLIT_FROM,
	FLARE_FLARE_SPRINT_LIT_TO,
	FLARE_FLARE_SPRINT_LIT_IDLE,
	FLARE_FLARE_SPRINT_LIT_FROM,
	FLARE_IGNITE_ONEHANDED,
	FLARE_DISCARD,
	FLARE_MELEE_UNLIT,
	FLARE_MELEE_UNLIT_MISS,
	FLARE_UNLIT_FIDGET1,
	FLARE_UNLIT_FIDGET2,
	FLARE_UNLIT_FIDGET3,
	FLARE_MELEE_LIT,
	FLARE_MELEE_LIT_MISS,
	FLARE_LIT_FIDGET1,
	FLARE_LIT_FIDGET2,
	FLARE_LIT_FIDGET3,
	FLARE_JUMP_TO_UNLIT,
	FLARE_JUMP_FROM_UNLIT,
	FLARE_JUMP_LIT_TO,
	FLARE_JUMP_LIT_FROM,
	FLARE_DRAW_FIRST,
};


void CFlare::ParticleRestore( void )
{
	if ( pev->iuser1 == 1 )
	{
		// burning flare lying in the world
		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/signal.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/blacksmoke.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_sparks.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_glow.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_extinguish.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}
}

void CFlare::FlareThink( void )
{
	if ( pev->owner )
	{
		STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );
		return;
	}

	SetNextThink( 0.1 );

	// burnt out
	if ( pev->radsuit_finished < gpGlobals->time && pev->iuser1 == 1 )
	{
		pev->renderfx = 0;
		pev->iuser1 = 2;
		pev->body = 0;
		STOP_SOUND( edict(), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );
		SetThink( NULL );
		return;
	}

	if ( pev->teleport_time < gpGlobals->time )
	{
		EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, ATTN_NORM );
		pev->teleport_time = gpGlobals->time + 6.9;
	}

	if ( pev->flags & FL_ONGROUND )
	{
		// lie flat on the ground
		pev->angles.x = 0;
		pev->angles.z = 0;

		// make it visible and give it a light
		if ( pev->effects & EF_NODRAW )
		{
			pev->effects &= ~EF_NODRAW;
			pev->effects |= EF_MUZZLEFLASH;
		}

		pev->solid = SOLID_TRIGGER;

		UTIL_SetOrigin( this, pev->origin );

		// can be picked up again
		SetUse( &CBasePlayerItem::DefaultUse );
		m_iCurrCaps = CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE;

		if ( pev->iuser4 == 1 )
			SetTouch( &CBasePlayerItem::DefaultTouch );
	}
}

void CFlare::Spawn( )
{
	pev->classname = MAKE_STRING( "weapon_flare" );
	Precache( );
	m_iId = 13;

	if ( custom_w_model )
		SET_MODEL( ENT( pev ), STRING( custom_w_model ) );
	else
		SET_MODEL( ENT( pev ), "models/weapons/flare/w_flare.mdl" );

	FallInit();// get ready to fall down.

	m_iSprintIdleAnim1 = FLARE_FLARE_SPRINT_UNLIT_IDLE;
	m_iSprintIdleAnim2 = FLARE_FLARE_SPRINT_LIT_IDLE;

	if ( pev->frags > 0 )
		m_iDefaultAmmo = pev->frags;
	else
		m_iDefaultAmmo = 1;

	while ( !script )
		script = MAKE_STRING( "inventoryitems/weapons/weapon_flare.txt" );

	if ( custom_script )
		script = custom_script;

	if ( pev->iuser1 == 1 )
	{
		SetThink( &CFlare::FlareThink );
		SetNextThink( 0 );

		pev->renderfx = 165;

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/signal.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/blacksmoke.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_sparks.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_glow.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_extinguish.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/weapons/flare/w_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}
	else
	{
		if ( pev->iuser1 == 0 )
			pev->renderfx = 99;

		if ( pev->iuser1 == 2 )
			pev->renderfx = 0;
	}

	strwmodel = MAKE_STRING( "models/weapons/flare/w_flare.mdl" );
}

void CFlare::Precache( void )
{
	PRECACHE_MODEL( "models/weapons/flare/v_flare.mdl" );
	PRECACHE_MODEL( "models/weapons/flare/w_flare.mdl" );
	PRECACHE_MODEL( "models/null.mdl" );

	if ( custom_v_model )
		PRECACHE_MODEL( custom_v_model );

	if ( custom_w_model )
		PRECACHE_MODEL( custom_w_model );

	PRECACHE_SOUND( "weapons/flare/flare_burn.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_ignite.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_movement1.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_movement2.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_movement3.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_strike1.wav" );
	PRECACHE_SOUND( "weapons/flare/flare_strike2.wav" );
}

int CFlare::GetItemInfo( ItemInfo *p )
{
	p->pszName = STRING( pev->classname );
	p->pszAmmo1 = "flares";
	p->iMaxAmmo1 = 5;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 2;
	p->iPosition = 0;
	p->iId = 13;
	p->iWeight = 1;
	return 1;
}

BOOL CFlare::Deploy( )
{
	m_bThrowing = false;
	m_bThrown = false;
	m_iSprintState = 0;

	if ( pev->iuser1 == 1 )
	{
		if ( m_pPlayer )
		{
			// lit flare in the hand
			MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
				WRITE_SHORT( entindex() );
				WRITE_STRING( "aurora/signal.aur" );
				WRITE_BYTE( 1 );
				WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
				WRITE_SHORT( 0 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
				WRITE_SHORT( entindex() );
				WRITE_STRING( "aurora/blacksmoke.aur" );
				WRITE_BYTE( 1 );
				WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
				WRITE_SHORT( 0 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
				WRITE_SHORT( entindex() );
				WRITE_STRING( "aurora/flare_sparks.aur" );
				WRITE_BYTE( 1 );
				WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
				WRITE_SHORT( 0 );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
				WRITE_SHORT( entindex() );
				WRITE_STRING( "aurora/flare_glow.aur" );
				WRITE_BYTE( 1 );
				WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
				WRITE_SHORT( 2 );
			MESSAGE_END();

			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, ATTN_NORM );
			m_fNextBurnTime = gpGlobals->time + 6.9;

			if ( !( m_pPlayer->m_iFlashlightFlags & 0x10 ) && !m_bThrown )
				m_pPlayer->m_iFlashlightFlags |= 0x10;
		}

		return DefaultDeploy( "models/weapons/flare/v_flare.mdl", "models/null.mdl", FLARE_DRAW_LIT, "crowbar" );
	}

	if ( !m_pPlayer->m_bFirstDeploy[m_iId] )
	{
		m_pPlayer->m_bFirstDeploy[m_iId] = true;
		return DefaultDeploy( "models/weapons/flare/v_flare.mdl", "models/null.mdl", FLARE_DRAW_FIRST, "crowbar" );
	}

	return DefaultDeploy( "models/weapons/flare/v_flare.mdl", "models/null.mdl", FLARE_FLARE_DRAW, "crowbar" );
}

void CFlare::Redeploy( void )
{
	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	m_pPlayer->m_iFlashlightFlags &= ~0x10;

	// take out a new flare
	m_bNoRedeploy = false;
	m_bThrowing = false;
	pev->renderfx = 0;
	pev->iuser1 = 0;
	pev->teleport_time = 0;
	pev->radsuit_finished = 0;
	SendWeaponAnim( FLARE_FLARE_DRAW );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 0.86;
}

void CFlare::Holster( )
{
	if ( pev->iuser1 == 1 )
	{
		// a burning flare is thrown away
		SendWeaponAnim( FLARE_FLARE_THROW );
		SetNextThink( 0.9 );
		SetThink( &CFlare::ThrowRemove );
		m_bThrowing = true;

		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 1;
	}
	else
	{
		SendWeaponAnim( FLARE_FLARE_HOLSTER );
		m_pPlayer->m_flNextAttack = gpGlobals->time + 0.5;
	}
}

void CFlare::Ignite( void )
{
	pev->radsuit_finished = gpGlobals->time + gSkillData.flareBurnTime;
	pev->iuser1 = 1;

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/flare/flare_ignite.wav", 1, ATTN_NORM );
	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, ATTN_NORM );

	m_fNextBurnTime = gpGlobals->time + 6.9;

	if ( !m_bThrown )
		m_pPlayer->m_iFlashlightFlags |= 0x10;

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/signal.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/blacksmoke.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/flare_sparks.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/flare_glow.aur" );
		WRITE_BYTE( 1 );
		WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
		WRITE_SHORT( 2 );
	MESSAGE_END();
}

void CFlare::ItemPostFrame( void )
{
	float speed = m_pPlayer->pev->velocity.Length2D();
	float time = 0;

	if ( m_gbIsInCutscene && m_pPlayer && pev->iuser1 == 1 )
	{
		STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );
		m_fNextBurnTime = gpGlobals->time;
	}

	if ( !m_fInReload && !m_iSuicideState && m_iSprintState != 2 && m_flNextPrimaryAttack < gpGlobals->time )
	{
		if ( m_pPlayer->SufficientlyOffTheGround() )
		{
			if ( !m_iSprintState )
			{
				m_iSprintState = 5;

				if ( pev->iuser1 == 1 )
					SendWeaponAnim( FLARE_JUMP_LIT_TO );
				else
					SendWeaponAnim( FLARE_JUMP_TO_UNLIT );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = time * 0.5 + gpGlobals->time;
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}
		else if ( m_iSprintState == 5 )
		{
			if ( pev->iuser1 == 1 )
				SendWeaponAnim( FLARE_JUMP_LIT_FROM );
			else
				SendWeaponAnim( FLARE_JUMP_FROM_UNLIT );

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

				if ( pev->iuser1 == 1 )
					SendWeaponAnim( FLARE_FLARE_SPRINT_LIT_TO );
				else
					SendWeaponAnim( FLARE_FLARE_SPRINT_UNLIT_TO );

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
				if ( pev->iuser1 == 1 )
					SendWeaponAnim( FLARE_FLARE_SPRINT_LIT_IDLE );
				else
					SendWeaponAnim( FLARE_FLARE_SPRINT_UNLIT_IDLE );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );
				m_iNextSprintAnim = time + gpGlobals->time;
			}
		}

		if ( ( !( m_pPlayer->pev->flags & (1<<18) ) || speed <= 0 ) && m_iNextSprintAnim < gpGlobals->time && m_iSprintState != 5 )
		{
			if ( m_iSprintState == 1 )
			{
				if ( pev->iuser1 == 1 )
					SendWeaponAnim( FLARE_FLARE_SPRINT_LIT_FROM );
				else
					SendWeaponAnim( FLARE_FLARE_SPRINT_UNLIT_FROM );

				time = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim );

				m_flNextPrimaryAttack = gpGlobals->time + 0.33;
				m_flTimeWeaponIdle = gpGlobals->time + 0.35;
				m_iSprintState = 0;
			}
		}
	}

	// the flare in the hand burnt out
	if ( pev->radsuit_finished < gpGlobals->time && pev->iuser1 == 1 && !m_bThrowing )
	{
		m_pPlayer->m_iFlashlightFlags &= ~0x10;
		pev->iuser1 = 2;
		pev->body = 0;
		STOP_SOUND( m_pPlayer->edict(), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgParticle, NULL, m_pPlayer->pev );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_extinguish.aur" );
			WRITE_BYTE( 1 );
			WRITE_STRING( "models/weapons/flare/v_flare.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		if ( !m_pPlayer->m_bInfiniteAmmo )
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;

		if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < 0 )
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( iMaxClip() );
			WRITE_SHORT( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );
		MESSAGE_END();

		old_m_iAmmo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];

		if ( !m_bNoRedeploy && !m_bThrowing )
		{
			SendWeaponAnim( FLARE_DISCARD );
			SetThink( &CFlare::Redeploy );
			SetNextThink( 1 );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 1.5;
		}
	}

	if ( pev->iuser1 == 1 )
	{
		if ( !( m_pPlayer->m_iFlashlightFlags & 0x10 ) && !m_bThrown )
			m_pPlayer->m_iFlashlightFlags |= 0x10;

		if ( m_fNextBurnTime < gpGlobals->time && !m_gbIsInCutscene )
		{
			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav", 0.3, ATTN_NORM );
			m_fNextBurnTime = gpGlobals->time + 6.9;
		}
	}
	else
	{
		if ( m_pPlayer->m_iFlashlightFlags & 0x10 )
			m_pPlayer->m_iFlashlightFlags &= ~0x10;
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CFlare::ThrowRemove( void )
{
	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgKillWpnPart, NULL, m_pPlayer->pev );
		WRITE_BYTE( 0 );
	MESSAGE_END();

	pev->iuser1 = 0;

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= 1 )
	{
		if ( !m_bNoRedeploy )
		{
			SetThink( &CFlare::Redeploy );
			SetNextThink( 0.5 );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = m_pPlayer->m_flNextAttack = gpGlobals->time + 1.5;
		}

		return;
	}

	if ( m_pPlayer->m_pLastItem )
		m_pPlayer->SelectLastItem( true );
	else if ( m_pPlayer->HasNamedPlayerItem( "weapon_mobile" ) )
		m_pPlayer->SelectItem( "weapon_mobile" );
	else if ( m_pPlayer->HasWeapons() )
		m_pPlayer->SelectRandomWeapon();

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

void CFlare::PrimaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	if ( pev->iuser1 > 0 )
	{
		// throw it away
		SendWeaponAnim( FLARE_FLARE_THROW );
		SetNextThink( 0.9 );
		SetThink( &CFlare::ThrowRemove );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1.5;
		m_bThrowing = true;
		m_pPlayer->m_iFlashlightFlags &= ~0x10;
		return;
	}

	SetNextThink( 0.86 );
	SetThink( &CFlare::Ignite );

	SendWeaponAnim( FLARE_FLARE_IGNITE );
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1.5;
}

void CFlare::SecondaryAttack( void )
{
	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_iSprintState || m_iSuicideState )
		return;

	m_pPlayer->SetNextThink( 0.3 );
	m_pPlayer->SetThink( &CBasePlayer::WeaponMelee );

	m_pPlayer->PlayerAnimation( "pistol_melee", false );

	bool bWillHit = false;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
			bWillHit = true;
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
			bWillHit = true;
	}

	if ( pev->iuser1 == 1 )
	{
		if ( bWillHit )
			SendWeaponAnim( FLARE_MELEE_LIT );
		else
			SendWeaponAnim( FLARE_MELEE_LIT_MISS );
	}
	else
	{
		if ( bWillHit )
			SendWeaponAnim( FLARE_MELEE_UNLIT );
		else
			SendWeaponAnim( FLARE_MELEE_UNLIT_MISS );
	}

	EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = m_flTimeWeaponIdle = gpGlobals->time + 1;
}

void CFlare::Fidget( void )
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
		if ( pev->iuser1 == 1 )
			SendWeaponAnim( FLARE_LIT_FIDGET1 );
		else
			SendWeaponAnim( FLARE_UNLIT_FIDGET1 );
		break;

	case 1:
		if ( pev->iuser1 == 1 )
			SendWeaponAnim( FLARE_LIT_FIDGET2 );
		else
			SendWeaponAnim( FLARE_UNLIT_FIDGET2 );
		break;

	case 2:
		if ( pev->iuser1 == 1 )
			SendWeaponAnim( FLARE_LIT_FIDGET3 );
		else
			SendWeaponAnim( FLARE_UNLIT_FIDGET3 );
		break;
	}
}

void CFlare::WeaponIdle( void )
{
	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_iSprintState )
		return;

	if ( m_flNextPrimaryAttack > gpGlobals->time || m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( pev->iuser1 == 1 )
		SendWeaponAnim( FLARE_FLARE_IDLE_LIT );
	else
		SendWeaponAnim( FLARE_FLARE_IDLE );

	m_flTimeWeaponIdle = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), m_pPlayer->pev->weaponanim ) + gpGlobals->time;
}
