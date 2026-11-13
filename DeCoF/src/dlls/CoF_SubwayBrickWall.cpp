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
//
// cof_subwaybrickwall.cpp - brick wall in the subway that breaks after a number of hits
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CSubwayBreakWall : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	virtual int ObjectCaps( void )
	{
		return ( CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE ) & ~FCAP_ACROSS_TRANSITION;
	}
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	float m_flNextUse;
};

LINK_ENTITY_TO_CLASS( trigger_subwaywall, CSubwayBreakWall );

void CSubwayBreakWall::Spawn( void )
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	pev->takedamage = DAMAGE_YES;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	pev->frame = 0;
}

void CSubwayBreakWall::Precache( void )
{
	if ( pev->noise1 )
		PRECACHE_SOUND( (char *)STRING( pev->noise1 ) );

	if ( pev->noise2 )
		PRECACHE_SOUND( (char *)STRING( pev->noise2 ) );
}

int CSubwayBreakWall::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// number of hits the wall takes
	int amount = pev->frags;

	if ( bitsDamageType & (1<<27) )
	{
		pev->iuser1++;

		if ( pev->iuser1 == amount )
		{
			if ( pev->target )
				FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );

			if ( pev->noise2 )
				EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise2 ), 1, 0.8 );

			UTIL_Remove( this );
		}
		else
		{
			if ( pev->noise1 )
				EMIT_SOUND( edict(), CHAN_VOICE, STRING( pev->noise1 ), 1, 0.8 );
		}
	}

	return 0;
}

void CSubwayBreakWall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( !UTIL_IsMasterTriggered( m_sMaster, pActivator ) )
		return;

	if ( m_flNextUse > gpGlobals->time )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	if ( pev->message )
		UTIL_ProMessage( pPlayer, STRING( pev->message ), 5, false );

	m_flNextUse = gpGlobals->time + 1;
}
