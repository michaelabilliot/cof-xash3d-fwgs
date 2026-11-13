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
// cof_statueuse.cpp - brush that opens the statue puzzle screens
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern int gmsgStatue;
extern int gmsgBoard;

class CStatueUse : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void )
	{
		return ( CBaseEntity::ObjectCaps() | FCAP_CONTINUOUS_USE ) & ~FCAP_ACROSS_TRANSITION;
	}

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );

	float m_flNextUse;
	int m_iStatueType;

	static TYPEDESCRIPTION m_SaveData[];
};

TYPEDESCRIPTION CStatueUse::m_SaveData[] =
{
	DEFINE_FIELD( CStatueUse, m_sMaster, FIELD_STRING ),
	DEFINE_FIELD( CStatueUse, m_flNextUse, FIELD_FLOAT ),
	DEFINE_FIELD( CStatueUse, m_iStatueType, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStatueUse, CBaseEntity );

LINK_ENTITY_TO_CLASS( trigger_statueuse, CStatueUse );

void CStatueUse::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "master" ) )
	{
		m_sMaster = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "statuetype" ) )
	{
		m_iStatueType = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CStatueUse::Spawn( void )
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_flNextUse = 0;
	pev->frame = 0;
}

void CStatueUse::Precache( void )
{
	PRECACHE_SOUND( "statue_puzzle/statue_switch.wav" );
}

void CStatueUse::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
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

	switch ( m_iStatueType )
	{
	case 0:
		MESSAGE_BEGIN( MSG_ONE, gmsgStatue, NULL, pPlayer->pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "gfx/vgui/640_eagle_button.tga" );
		MESSAGE_END();
		break;

	case 1:
		MESSAGE_BEGIN( MSG_ONE, gmsgStatue, NULL, pPlayer->pev );
			WRITE_BYTE( 2 );
			WRITE_STRING( "gfx/vgui/640_horse_button.tga" );
		MESSAGE_END();
		break;

	case 2:
		MESSAGE_BEGIN( MSG_ONE, gmsgStatue, NULL, pPlayer->pev );
			WRITE_BYTE( 3 );
			WRITE_STRING( "gfx/vgui/640_lion_button.tga" );
		MESSAGE_END();
		break;

	case 3:
		MESSAGE_BEGIN( MSG_ONE, gmsgStatue, NULL, pPlayer->pev );
			WRITE_BYTE( 4 );
			WRITE_STRING( "gfx/vgui/640_owl_button.tga" );
		MESSAGE_END();
		break;

	case 4:
		MESSAGE_BEGIN( MSG_ONE, gmsgBoard, NULL, pPlayer->pev );
			WRITE_STRING( "gfx/vgui/640_gustavdahlwelcome.tga" );
		MESSAGE_END();
		break;
	}

	m_flNextUse = gpGlobals->time + 1;
}
