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
// cof_bowlingball_guess.cpp - bowling ball prop (new in 1.6a; the file name is a guess)
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

class CBowlingBall : public CBaseAnimating
{
public:
	void Spawn( void );
	void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }

private:
	void EXPORT BowlingUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( bowling_ball, CBowlingBall );

void CBowlingBall::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/props/blandat/bowling_ball.mdl" );
}

void CBowlingBall::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/props/blandat/bowling_ball.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -2, -2, 0 ), Vector( 2, 2, 4 ) );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	SetUse( &CBowlingBall::BowlingUse );
}

void CBowlingBall::BowlingUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );
	}
}
