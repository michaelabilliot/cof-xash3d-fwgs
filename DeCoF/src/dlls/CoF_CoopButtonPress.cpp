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
// cof_coopbuttonpress.cpp - co-op button that only works when every player is ready
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CCoFCoopAllPlayersButton : public CBaseToggle
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void )
	{
		return ( CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE ) & ~FCAP_ACROSS_TRANSITION;
	}

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	static TYPEDESCRIPTION m_SaveData[];

	float m_flNextUse;
	float m_flNextCheck;
};

TYPEDESCRIPTION CCoFCoopAllPlayersButton::m_SaveData[] =
{
	DEFINE_FIELD( CCoFCoopAllPlayersButton, m_flNextUse, FIELD_TIME ),
	DEFINE_FIELD( CCoFCoopAllPlayersButton, m_flNextCheck, FIELD_TIME ),
	DEFINE_FIELD( CCoFCoopAllPlayersButton, m_sMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCoFCoopAllPlayersButton, CBaseEntity );

LINK_ENTITY_TO_CLASS( func_coopallplayersbutton, CCoFCoopAllPlayersButton );

void CCoFCoopAllPlayersButton::Spawn( void )
{
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_flNextUse = 0;

	if ( m_flNextCheck == 0 )
		m_flNextCheck = 2;

	pev->frame = 0;
}

void CCoFCoopAllPlayersButton::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
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

	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

	if ( pTarget )
	{
		// the target is set to 1 when every player is ready
		if ( pTarget->pev->iuser1 == 1 )
		{
			if ( pev->target )
				FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
		}
		else
		{
			UTIL_ProMessage( pPlayer, "Please wait until everyone is ready!", 2, false );
		}
	}

	m_flNextUse = gpGlobals->time + m_flNextCheck;
}
