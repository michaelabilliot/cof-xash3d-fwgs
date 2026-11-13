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
// cof_puzzlebarsbutton.cpp - button that opens the bar puzzle view
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern int gmsgPuzzlebar;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

// declared in cof_importantclassdefs.h
class CPuzzleBarButton : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual void Precache( void );
	void ToggleBar( int bar, CBasePlayer *pPlayer );
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextUse;
	float m_flNextCheck;
};

TYPEDESCRIPTION CPuzzleBarButton::m_SaveData[] =
{
	DEFINE_FIELD( CPuzzleBarButton, m_flNextUse, FIELD_TIME ),
	DEFINE_FIELD( CPuzzleBarButton, m_flNextCheck, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CPuzzleBarButton, CBaseToggle );

LINK_ENTITY_TO_CLASS( cof_puzzlebarbutton, CPuzzleBarButton );

void CPuzzleBarButton::Spawn( void )
{
	Precache( );

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );
	m_flNextUse = 0;
	m_flNextCheck = 2;
}

void CPuzzleBarButton::Precache( void )
{
	PRECACHE_SOUND( "keypad/keypad_press.wav" );
	PRECACHE_SOUND( "ambience/asy_puzzlebar_slide.wav" );
}

void CPuzzleBarButton::ToggleBar( int bar, CBasePlayer *pPlayer )
{
	CBaseEntity *pBar = NULL;
	CBaseEntity *pBar2 = NULL;

	if ( !pPlayer )
		return;

	switch ( bar )
	{
	case 1:
		pBar = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );
		pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

		if ( pBar )
			pBar->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );

		if ( pBar2 )
			pBar2->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		break;

	case 2:
		pBar = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );
		pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

		if ( pBar )
			pBar->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );

		if ( pBar2 )
			pBar2->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		break;

	case 3:
		pBar = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );
		pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

		if ( pBar )
			pBar->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );

		if ( pBar2 )
			pBar2->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		break;

	case 4:
		pBar = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );
		pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

		if ( pBar )
			pBar->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );

		if ( pBar2 )
			pBar2->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		break;
	}

	if ( pBar )
	{
		EMIT_SOUND_DYN( pBar->edict(), CHAN_BODY, "ambience/asy_puzzlebar_slide.wav", 0.8, ATTN_NONE, 0, 100 );
		EMIT_SOUND_DYN( pBar->edict(), CHAN_VOICE, "keypad/keypad_press.wav", 1, ATTN_NONE, 0, 100 );
	}
}

void CPuzzleBarButton::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( m_flNextUse > gpGlobals->time )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	pPlayer->m_pPuzzleBar = this;

	// the puzzle is solved
	if ( pev->iuser1 )
	{
		UTIL_ProMessage( pPlayer, "I don't need to use this anymore.", 5, false );
		m_flNextUse = gpGlobals->time + m_flNextCheck;
		return;
	}

	if ( pPlayer && pev->message )
	{
		CBaseEntity *pHelper = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

		if ( !FNullEnt( pHelper ) )
		{
			SET_VIEW( pPlayer->edict(), pHelper->edict() );
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 1 );
		MESSAGE_END();
	}

	m_flNextUse = gpGlobals->time + m_flNextCheck;
}
