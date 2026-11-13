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
// cof_checkbrush.cpp - brush the player can look at for a message
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CCheckBrush : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
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

TYPEDESCRIPTION CCheckBrush::m_SaveData[] =
{
	DEFINE_FIELD( CCheckBrush, m_flNextUse, FIELD_TIME ),
	DEFINE_FIELD( CCheckBrush, m_flNextCheck, FIELD_TIME ),
	DEFINE_FIELD( CCheckBrush, m_sMaster, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCheckBrush, CBaseEntity );

LINK_ENTITY_TO_CLASS( trigger_checkbrush, CCheckBrush );

void CCheckBrush::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "usesound" ) )
	{
		pev->noise = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "checkdelay" ) )
	{
		m_flNextCheck = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CCheckBrush::Spawn( void )
{
	Precache( );

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

void CCheckBrush::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );
}

void CCheckBrush::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
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

	if ( pev->spawnflags & 1 )
	{
		// show the keypad code once the player has seen it
		if ( pPlayer->m_iKeypadNumber != 9999 )
		{
			char szCodeMsg[256];

			sprintf( szCodeMsg, "I don't want to watch it again... %i ...What is that anyway?", pPlayer->m_iKeypadNumber );
			UTIL_ProMessage( pPlayer, szCodeMsg, 5, false );
		}
	}
	else
	{
		UTIL_ProMessage( pPlayer, STRING( pev->message ), 5, false );
	}

	if ( pev->noise )
		EMIT_SOUND( ENT( pActivator->pev ), CHAN_AUTO, STRING( pev->noise ), 1, 0.8 );

	m_flNextUse = gpGlobals->time + m_flNextCheck;
}
