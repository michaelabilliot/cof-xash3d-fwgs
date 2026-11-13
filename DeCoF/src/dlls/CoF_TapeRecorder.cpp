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
// cof_taperecorder.cpp - the tape recorder the player saves the game with
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "effects.h"
#include "shake.h"

extern int gmsgSaveLoad;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

class CTapeRecorder : public CBaseAnimating
{
	void Spawn( void );
	void EXPORT BlinkThink( void );
	void Precache( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }
	void EXPORT TapeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData *pkvd );

	float m_flGlintTime;
	float m_useTime;
	int custommessage;
	bool m_bTapeReminder;

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

LINK_ENTITY_TO_CLASS( tape_recorder, CTapeRecorder );

TYPEDESCRIPTION CTapeRecorder::m_SaveData[] =
{
	DEFINE_FIELD( CTapeRecorder, m_useTime, FIELD_TIME ),
	DEFINE_FIELD( CTapeRecorder, m_flGlintTime, FIELD_TIME ),
	DEFINE_FIELD( CTapeRecorder, m_bTapeReminder, FIELD_BOOLEAN ),
	DEFINE_FIELD( CTapeRecorder, custommessage, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CTapeRecorder, CBaseAnimating );

void CTapeRecorder::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "custommsg" ) )
	{
		custommessage = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CTapeRecorder::Spawn( void )
{
	Precache( );

	if ( pev->model )
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	else
		SET_MODEL( ENT( pev ), "models/tape_recorder.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_BBOX;
	UTIL_SetSize( pev, Vector( -10, -6, -0.5 ), Vector( 10, 6, 0.5 ) );

	if ( !( pev->spawnflags & 2 ) )
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR( ENT( pev ) );
	}

	SetUse( &CTapeRecorder::TapeUse );
	SetNextThink( 1 );
	SetThink( &CTapeRecorder::BlinkThink );

	Vector Origin;
	Origin = pev->origin;
	Origin.z += 8;

	int r = 128;
	int g = 15;
	int b = 15;
	int radius = 10;

	pev->renderfx = 167;
}

void CTapeRecorder::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );
	else
		PRECACHE_MODEL( "models/tape_recorder.mdl" );

	PRECACHE_MODEL( "sprites/glow01.spr" );
	PRECACHE_SOUND( "save/save_no.wav" );
	PRECACHE_SOUND( "save/save_yes.wav" );
	PRECACHE_SOUND( "save/save_sequence.wav" );
}

void CTapeRecorder::BlinkThink( void )
{
	if ( m_flGlintTime > gpGlobals->time )
	{
		SetNextThink( 0.1 );
		return;
	}

	CSprite *pSprite = CSprite::SpriteCreate( "sprites/glow01.spr", Vector( pev->origin.x, pev->origin.y, pev->origin.z + 4 ), TRUE );
	pSprite->pev->scale = 0.1;
	pSprite->AnimateAndDie( 10 );
	pSprite->SetTransparency( kRenderTransAdd, 192, 0, 0, 255, kRenderFxNone );

	if ( pSprite )
	{
		pSprite->pev->origin = Vector( pev->origin.x, pev->origin.y, pev->origin.z + 4 );
	}

	m_flGlintTime = gpGlobals->time + 1;
	SetNextThink( 0.1 );
}

void CTapeRecorder::TapeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( m_useTime > gpGlobals->time )
		return;

	if ( pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		// first use only describes the tape recorder
		if ( !pPlayer->m_bHasSaved )
		{
			if ( pev->spawnflags & 1 )
				UTIL_ProMessage( pPlayer, "Huh? What's this doing here? Looks like an old tape recorder. I should be able to record my thoughts and experiences here.", 5, false );
			else if ( custommessage )
				UTIL_ProMessage( pPlayer, STRING( custommessage ), 5, false );
			else
				UTIL_ProMessage( pPlayer, "A dusty tape recorder. I can record my thoughts and experiences here.", 5, false );

			pPlayer->m_bHasSaved = true;
			m_useTime = gpGlobals->time + 1;
			return;
		}

		// saving costs a tape on the hardest difficulty
		if ( CVAR_GET_FLOAT( "difficulty" ) == 4 )
		{
			if ( !pPlayer->m_iTapes || !pPlayer->HasItemInSlot( "inventoryitems/cassettetape.txt", false ) )
			{
				UTIL_ProMessage( pPlayer, "I can't use this without a tape.", 2, false );
				m_useTime = gpGlobals->time + 2;
				return;
			}

			if ( !m_bTapeReminder )
			{
				char szMessage[128];

				if ( pPlayer->m_iTapes == 1 )
					sprintf( szMessage, "Looks like there's only enough room for 1 more save..." );
				else
					sprintf( szMessage, "Looks like there's enough room for %i more saves.", pPlayer->m_iTapes );

				m_bTapeReminder = true;
				UTIL_ProMessage( pPlayer, szMessage, 2, false );
				m_useTime = gpGlobals->time + 1;
				return;
			}
		}

		if ( pev->message )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSaveLoad, NULL, pPlayer->pev );
				WRITE_BYTE( 0 );
				WRITE_STRING( STRING( pev->message ) );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSaveLoad, NULL, pPlayer->pev );
				WRITE_BYTE( 0 );
				WRITE_STRING( "Unknown area" );
			MESSAGE_END();
		}

		UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0.1, 10, 255, FFADE_OUT | FFADE_STAYOUT );

		m_bTapeReminder = false;
		pPlayer->m_bHasSaved = true;
		m_useTime = gpGlobals->time + 2;
	}
}
