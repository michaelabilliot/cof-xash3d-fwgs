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
// cof_puzzlebars.cpp - the five bars of the bar puzzle
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "player.h"

extern int gmsgPuzzlebar;

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

class CPuzzleBar : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BarCheck( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_puzzlebar, CPuzzleBar );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CPuzzleBar::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CPuzzleBar::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/props/inomhusd/puzzle_bar.mdl" );

	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	// iuser1: bar closed, iuser2: spark when the bar check runs,
	// iuser3: starts closed
	pev->iuser1 = 0;
	pev->iuser2 = 0;

	if ( pev->iuser3 )
	{
		pev->iuser1 = 1;
		SetSequenceByName( "closed" );
	}
	else
	{
		SetSequenceByName( "opened" );
		pev->iuser1 = 0;
	}
}

void CPuzzleBar::Precache( void )
{
	PRECACHE_MODEL( "models/props/inomhusd/puzzle_bar.mdl" );
}

void CPuzzleBar::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->iuser1 )
	{
		SetSequenceByName( "close" );
		pev->iuser1 = 1;
		pev->iuser2 = 1;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
	else
	{
		SetSequenceByName( "open" );
		pev->iuser1 = 0;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
}

void CPuzzleBar::BarCheck( void )
{
	Vector vecSrc, vecAng;

	// every bar has to be closed
	CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );

	if ( pBar1 )
	{
		if ( !pBar1->pev->iuser1 )
			return;

		if ( pBar1->pev->iuser2 )
		{
			((CBaseAnimating *)pBar1)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar1->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

	if ( pBar2 )
	{
		if ( !pBar2->pev->iuser1 )
			return;

		if ( pBar2->pev->iuser2 )
		{
			((CBaseAnimating *)pBar2)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar2->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

	if ( pBar3 )
	{
		if ( !pBar3->pev->iuser1 )
			return;

		if ( pBar3->pev->iuser2 )
		{
			((CBaseAnimating *)pBar3)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar3->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

	if ( pBar4 )
	{
		if ( !pBar4->pev->iuser1 )
			return;

		if ( pBar4->pev->iuser2 )
		{
			((CBaseAnimating *)pBar4)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar4->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar5 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

	if ( pBar5 )
	{
		if ( !pBar5->pev->iuser1 )
			return;

		if ( pBar5->pev->iuser2 )
		{
			((CBaseAnimating *)pBar5)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar5->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( !pPlayer )
		return;

	GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;

	// solved, give the view back to the player
	CBaseEntity *pButton = UTIL_FindEntityByClassname( NULL, "cof_puzzlebarbutton" );

	if ( pButton )
	{
		FireTargets( STRING( pButton->pev->target ), pPlayer, this, USE_TOGGLE, 0 );
		pButton->pev->iuser1 = 1;

		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;
	}
}

class CPuzzleBar2 : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BarCheck( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_puzzlebar2, CPuzzleBar2 );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CPuzzleBar2::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CPuzzleBar2::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/props/inomhusd/puzzle_bar.mdl" );

	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	// iuser1: bar closed, iuser2: spark when the bar check runs,
	// iuser3: starts closed
	pev->iuser1 = 0;
	pev->iuser2 = 0;

	if ( pev->iuser3 )
	{
		pev->iuser1 = 1;
		SetSequenceByName( "closed" );
	}
	else
	{
		SetSequenceByName( "opened" );
		pev->iuser1 = 0;
	}
}

void CPuzzleBar2::Precache( void )
{
	PRECACHE_MODEL( "models/props/inomhusd/puzzle_bar.mdl" );
}

void CPuzzleBar2::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->iuser1 )
	{
		SetSequenceByName( "close" );
		pev->iuser1 = 1;
		pev->iuser2 = 1;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
	else
	{
		SetSequenceByName( "open" );
		pev->iuser1 = 0;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
}

void CPuzzleBar2::BarCheck( void )
{
	Vector vecSrc, vecAng;

	// every bar has to be closed
	CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );

	if ( pBar1 )
	{
		if ( !pBar1->pev->iuser1 )
			return;

		if ( pBar1->pev->iuser2 )
		{
			((CBaseAnimating *)pBar1)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar1->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

	if ( pBar2 )
	{
		if ( !pBar2->pev->iuser1 )
			return;

		if ( pBar2->pev->iuser2 )
		{
			((CBaseAnimating *)pBar2)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar2->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

	if ( pBar3 )
	{
		if ( !pBar3->pev->iuser1 )
			return;

		if ( pBar3->pev->iuser2 )
		{
			((CBaseAnimating *)pBar3)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar3->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

	if ( pBar4 )
	{
		if ( !pBar4->pev->iuser1 )
			return;

		if ( pBar4->pev->iuser2 )
		{
			((CBaseAnimating *)pBar4)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar4->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar5 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

	if ( pBar5 )
	{
		if ( !pBar5->pev->iuser1 )
			return;

		if ( pBar5->pev->iuser2 )
		{
			((CBaseAnimating *)pBar5)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar5->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( !pPlayer )
		return;

	GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;

	// solved, give the view back to the player
	CBaseEntity *pButton = UTIL_FindEntityByClassname( NULL, "cof_puzzlebarbutton" );

	if ( pButton )
	{
		FireTargets( STRING( pButton->pev->target ), pPlayer, this, USE_TOGGLE, 0 );
		pButton->pev->iuser1 = 1;

		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;
	}
}

class CPuzzleBar3 : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BarCheck( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_puzzlebar3, CPuzzleBar3 );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CPuzzleBar3::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CPuzzleBar3::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/props/inomhusd/puzzle_bar.mdl" );

	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	// iuser1: bar closed, iuser2: spark when the bar check runs,
	// iuser3: starts closed
	pev->iuser1 = 0;
	pev->iuser2 = 0;

	if ( pev->iuser3 )
	{
		pev->iuser1 = 1;
		SetSequenceByName( "closed" );
	}
	else
	{
		SetSequenceByName( "opened" );
		pev->iuser1 = 0;
	}
}

void CPuzzleBar3::Precache( void )
{
	PRECACHE_MODEL( "models/props/inomhusd/puzzle_bar.mdl" );
}

void CPuzzleBar3::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->iuser1 )
	{
		SetSequenceByName( "close" );
		pev->iuser1 = 1;
		pev->iuser2 = 1;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
	else
	{
		SetSequenceByName( "open" );
		pev->iuser1 = 0;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
}

void CPuzzleBar3::BarCheck( void )
{
	Vector vecSrc, vecAng;

	// every bar has to be closed
	CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );

	if ( pBar1 )
	{
		if ( !pBar1->pev->iuser1 )
			return;

		if ( pBar1->pev->iuser2 )
		{
			((CBaseAnimating *)pBar1)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar1->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

	if ( pBar2 )
	{
		if ( !pBar2->pev->iuser1 )
			return;

		if ( pBar2->pev->iuser2 )
		{
			((CBaseAnimating *)pBar2)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar2->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

	if ( pBar3 )
	{
		if ( !pBar3->pev->iuser1 )
			return;

		if ( pBar3->pev->iuser2 )
		{
			((CBaseAnimating *)pBar3)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar3->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

	if ( pBar4 )
	{
		if ( !pBar4->pev->iuser1 )
			return;

		if ( pBar4->pev->iuser2 )
		{
			((CBaseAnimating *)pBar4)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar4->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar5 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

	if ( pBar5 )
	{
		if ( !pBar5->pev->iuser1 )
			return;

		if ( pBar5->pev->iuser2 )
		{
			((CBaseAnimating *)pBar5)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar5->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( !pPlayer )
		return;

	GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;

	// solved, give the view back to the player
	CBaseEntity *pButton = UTIL_FindEntityByClassname( NULL, "cof_puzzlebarbutton" );

	if ( pButton )
	{
		FireTargets( STRING( pButton->pev->target ), pPlayer, this, USE_TOGGLE, 0 );
		pButton->pev->iuser1 = 1;

		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;
	}
}

class CPuzzleBar4 : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BarCheck( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_puzzlebar4, CPuzzleBar4 );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CPuzzleBar4::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CPuzzleBar4::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/props/inomhusd/puzzle_bar.mdl" );

	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	// iuser1: bar closed, iuser2: spark when the bar check runs,
	// iuser3: starts closed
	pev->iuser1 = 0;
	pev->iuser2 = 0;

	if ( pev->iuser3 )
	{
		pev->iuser1 = 1;
		SetSequenceByName( "closed" );
	}
	else
	{
		SetSequenceByName( "opened" );
		pev->iuser1 = 0;
	}
}

void CPuzzleBar4::Precache( void )
{
	PRECACHE_MODEL( "models/props/inomhusd/puzzle_bar.mdl" );
}

void CPuzzleBar4::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->iuser1 )
	{
		SetSequenceByName( "close" );
		pev->iuser1 = 1;
		pev->iuser2 = 1;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
	else
	{
		SetSequenceByName( "open" );
		pev->iuser1 = 0;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
}

void CPuzzleBar4::BarCheck( void )
{
	Vector vecSrc, vecAng;

	// every bar has to be closed
	CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );

	if ( pBar1 )
	{
		if ( !pBar1->pev->iuser1 )
			return;

		if ( pBar1->pev->iuser2 )
		{
			((CBaseAnimating *)pBar1)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar1->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

	if ( pBar2 )
	{
		if ( !pBar2->pev->iuser1 )
			return;

		if ( pBar2->pev->iuser2 )
		{
			((CBaseAnimating *)pBar2)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar2->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

	if ( pBar3 )
	{
		if ( !pBar3->pev->iuser1 )
			return;

		if ( pBar3->pev->iuser2 )
		{
			((CBaseAnimating *)pBar3)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar3->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

	if ( pBar4 )
	{
		if ( !pBar4->pev->iuser1 )
			return;

		if ( pBar4->pev->iuser2 )
		{
			((CBaseAnimating *)pBar4)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar4->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar5 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

	if ( pBar5 )
	{
		if ( !pBar5->pev->iuser1 )
			return;

		if ( pBar5->pev->iuser2 )
		{
			((CBaseAnimating *)pBar5)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar5->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( !pPlayer )
		return;

	GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;

	// solved, give the view back to the player
	CBaseEntity *pButton = UTIL_FindEntityByClassname( NULL, "cof_puzzlebarbutton" );

	if ( pButton )
	{
		FireTargets( STRING( pButton->pev->target ), pPlayer, this, USE_TOGGLE, 0 );
		pButton->pev->iuser1 = 1;

		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;
	}
}

class CPuzzleBar5 : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void );
	void EXPORT BarCheck( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_puzzlebar5, CPuzzleBar5 );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int CPuzzleBar5::Classify( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CPuzzleBar5::Spawn( void )
{
	Precache( );

	SET_MODEL( ENT( pev ), "models/props/inomhusd/puzzle_bar.mdl" );

	UTIL_SetSize( pev, Vector( -4, -4, 0 ), Vector( 4, 4, 32 ) );

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NOCLIP;
	m_bloodColor = DONT_BLEED;
	m_MonsterState = MONSTERSTATE_NONE;

	// iuser1: bar closed, iuser2: spark when the bar check runs,
	// iuser3: starts closed
	pev->iuser1 = 0;
	pev->iuser2 = 0;

	if ( pev->iuser3 )
	{
		pev->iuser1 = 1;
		SetSequenceByName( "closed" );
	}
	else
	{
		SetSequenceByName( "opened" );
		pev->iuser1 = 0;
	}
}

void CPuzzleBar5::Precache( void )
{
	PRECACHE_MODEL( "models/props/inomhusd/puzzle_bar.mdl" );
}

void CPuzzleBar5::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->iuser1 )
	{
		SetSequenceByName( "close" );
		pev->iuser1 = 1;
		pev->iuser2 = 1;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
	else
	{
		SetSequenceByName( "open" );
		pev->iuser1 = 0;
		SetNextThink( 1 );
		SetThink( &CPuzzleBar::BarCheck );
	}
}

void CPuzzleBar5::BarCheck( void )
{
	Vector vecSrc, vecAng;

	// every bar has to be closed
	CBaseEntity *pBar1 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar" );

	if ( pBar1 )
	{
		if ( !pBar1->pev->iuser1 )
			return;

		if ( pBar1->pev->iuser2 )
		{
			((CBaseAnimating *)pBar1)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar1->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar2 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar2" );

	if ( pBar2 )
	{
		if ( !pBar2->pev->iuser1 )
			return;

		if ( pBar2->pev->iuser2 )
		{
			((CBaseAnimating *)pBar2)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar2->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar3 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar3" );

	if ( pBar3 )
	{
		if ( !pBar3->pev->iuser1 )
			return;

		if ( pBar3->pev->iuser2 )
		{
			((CBaseAnimating *)pBar3)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar3->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar4 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar4" );

	if ( pBar4 )
	{
		if ( !pBar4->pev->iuser1 )
			return;

		if ( pBar4->pev->iuser2 )
		{
			((CBaseAnimating *)pBar4)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar4->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pBar5 = UTIL_FindEntityByClassname( NULL, "cof_puzzlebar5" );

	if ( pBar5 )
	{
		if ( !pBar5->pev->iuser1 )
			return;

		if ( pBar5->pev->iuser2 )
		{
			((CBaseAnimating *)pBar5)->GetAttachment( 0, vecSrc, vecAng );
			UTIL_Sparks( vecSrc );
			pBar5->pev->iuser2 = 0;
		}
	}
	else
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( !pPlayer )
		return;

	GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;

	// solved, give the view back to the player
	CBaseEntity *pButton = UTIL_FindEntityByClassname( NULL, "cof_puzzlebarbutton" );

	if ( pButton )
	{
		FireTargets( STRING( pButton->pev->target ), pPlayer, this, USE_TOGGLE, 0 );
		pButton->pev->iuser1 = 1;

		SET_VIEW( pPlayer->edict(), pPlayer->edict() );

		MESSAGE_BEGIN( MSG_ONE, gmsgPuzzlebar, NULL, pPlayer->pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_pPuzzleBar = NULL;
	}
}
