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
// cof_statues.cpp - the four statues of the statue puzzle
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "animation.h"
#include "player.h"

extern int gmsgStatue;

// declared in effects.h
class CStatueEagle : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

// declared in effects.h
class CStatueHorse : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

// declared in effects.h
class CStatueLion : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

// declared in effects.h
class CStatueOwl : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

class CStatuePuzzleComplete : public CBaseEntity
{
public:
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( statue_puzzle_complete, CStatuePuzzleComplete );

void CStatuePuzzleComplete::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CStatuePuzzleComplete::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator->IsPlayer() )
	{
		// close the statue screen
		MESSAGE_BEGIN( MSG_ONE, gmsgStatue, NULL, pActivator->pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( "gfx/vgui/640_eagle_button.tga" );
		MESSAGE_END();

		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
	}
}

void CheckStatuePuzzle( void )
{
	CBaseEntity *pEagle;
	CBaseEntity *pHorse;
	CBaseEntity *pLion;
	CBaseEntity *pOwl;
	CBaseEntity *pStatueEnd;

	pEagle = UTIL_FindEntityByClassname( NULL, "statue_eagle" );
	pHorse = UTIL_FindEntityByClassname( NULL, "statue_horse" );
	pLion = UTIL_FindEntityByClassname( NULL, "statue_lion" );
	pOwl = UTIL_FindEntityByClassname( NULL, "statue_owl" );
	pStatueEnd = UTIL_FindEntityByClassname( NULL, "statue_puzzle_complete" );

	if ( pEagle && pHorse && pLion && pOwl && pStatueEnd )
	{
		if ( GetClassPtr( (CStatueEagle *)pEagle->pev )->m_iPosition == 2 && GetClassPtr( (CStatueHorse *)pHorse->pev )->m_iPosition == 1 && GetClassPtr( (CStatueLion *)pLion->pev )->m_iPosition == 1 && GetClassPtr( (CStatueOwl *)pOwl->pev )->m_iPosition == 4 )
		{
			CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );
			pStatueEnd->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
	}
}

TYPEDESCRIPTION CStatueOwl::m_SaveData[] =
{
	DEFINE_FIELD( CStatueOwl, m_iPosition, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStatueOwl, CBaseAnimating );
LINK_ENTITY_TO_CLASS( statue_owl, CStatueOwl );

void CStatueOwl::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "startpos" ) )
	{
		m_iPosition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CStatueOwl::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/props/utomhusd/owl.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	UTIL_SetSize( pev, Vector( -16, -16, -44 ), Vector( 16, 16, 44 ) );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	// 0 centre, 1 left, 2 up, 3 right, 4 back
	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 1:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 2:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 3:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 4:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;
	}
}

void CStatueOwl::Precache( void )
{
	PRECACHE_MODEL( "models/props/utomhusd/owl.mdl" );
	PRECACHE_SOUND( "statue_puzzle/statue_move.wav" );
}

//=========================================================
// SetSequenceByName
//=========================================================
void CStatueOwl::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CStatueOwl::MoveLeft( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 1;
		break;

	case 3:
		SetSequenceByName( "right_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueOwl::MoveRight( void )
{
	if ( m_iPosition == 2 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 3;
		break;

	case 1:
		SetSequenceByName( "left_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueOwl::MoveUp( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 3 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 2;
		break;

	case 4:
		SetSequenceByName( "back_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueOwl::MoveDown( void )
{
	if ( m_iPosition == 1 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 4;
		break;

	case 2:
		SetSequenceByName( "up_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

TYPEDESCRIPTION CStatueEagle::m_SaveData[] =
{
	DEFINE_FIELD( CStatueEagle, m_iPosition, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStatueEagle, CBaseAnimating );
LINK_ENTITY_TO_CLASS( statue_eagle, CStatueEagle );

void CStatueEagle::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "startpos" ) )
	{
		m_iPosition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CStatueEagle::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/props/utomhusd/eagle.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	UTIL_SetSize( pev, Vector( -16, -16, -44 ), Vector( 16, 16, 44 ) );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	// 0 centre, 1 left, 2 up, 3 right, 4 back
	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 1:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 2:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 3:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 4:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;
	}
}

void CStatueEagle::Precache( void )
{
	PRECACHE_MODEL( "models/props/utomhusd/eagle.mdl" );
	PRECACHE_SOUND( "statue_puzzle/statue_move.wav" );
}

//=========================================================
// SetSequenceByName
//=========================================================
void CStatueEagle::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CStatueEagle::MoveLeft( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 1;
		break;

	case 3:
		SetSequenceByName( "right_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueEagle::MoveRight( void )
{
	if ( m_iPosition == 2 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 3;
		break;

	case 1:
		SetSequenceByName( "left_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueEagle::MoveUp( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 3 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 2;
		break;

	case 4:
		SetSequenceByName( "back_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueEagle::MoveDown( void )
{
	if ( m_iPosition == 1 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 4;
		break;

	case 2:
		SetSequenceByName( "up_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

TYPEDESCRIPTION CStatueLion::m_SaveData[] =
{
	DEFINE_FIELD( CStatueLion, m_iPosition, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStatueLion, CBaseAnimating );
LINK_ENTITY_TO_CLASS( statue_lion, CStatueLion );

void CStatueLion::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "startpos" ) )
	{
		m_iPosition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CStatueLion::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/props/utomhusd/lion.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	UTIL_SetSize( pev, Vector( -16, -16, -44 ), Vector( 16, 16, 44 ) );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	// 0 centre, 1 left, 2 up, 3 right, 4 back
	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 1:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 2:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 3:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 4:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;
	}
}

void CStatueLion::Precache( void )
{
	PRECACHE_MODEL( "models/props/utomhusd/lion.mdl" );
	PRECACHE_SOUND( "statue_puzzle/statue_move.wav" );
}

//=========================================================
// SetSequenceByName
//=========================================================
void CStatueLion::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CStatueLion::MoveLeft( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 1;
		break;

	case 3:
		SetSequenceByName( "right_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueLion::MoveRight( void )
{
	if ( m_iPosition == 2 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 3;
		break;

	case 1:
		SetSequenceByName( "left_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueLion::MoveUp( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 3 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 2;
		break;

	case 4:
		SetSequenceByName( "back_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueLion::MoveDown( void )
{
	if ( m_iPosition == 1 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 4;
		break;

	case 2:
		SetSequenceByName( "up_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

TYPEDESCRIPTION CStatueHorse::m_SaveData[] =
{
	DEFINE_FIELD( CStatueHorse, m_iPosition, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStatueHorse, CBaseAnimating );
LINK_ENTITY_TO_CLASS( statue_horse, CStatueHorse );

void CStatueHorse::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "startpos" ) )
	{
		m_iPosition = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CStatueHorse::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT( pev ), "models/props/utomhusd/horse.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_PUSHSTEP;
	pev->solid = SOLID_SLIDEBOX;
	UTIL_SetSize( pev, Vector( -16, -16, -44 ), Vector( 16, 16, 44 ) );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT( pev ) );

	// 0 centre, 1 left, 2 up, 3 right, 4 back
	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 1:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 2:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 3:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;

	case 4:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		break;
	}
}

void CStatueHorse::Precache( void )
{
	PRECACHE_MODEL( "models/props/utomhusd/horse.mdl" );
	PRECACHE_SOUND( "statue_puzzle/statue_move.wav" );
}

//=========================================================
// SetSequenceByName
//=========================================================
void CStatueHorse::SetSequenceByName( char *szSequence )
{
	int iSequence;

	iSequence = LookupSequence( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT( at_aiconsole, "%s has no sequence named:%f\n", STRING( pev->classname ), szSequence );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}
}

void CStatueHorse::MoveLeft( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_left" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 1;
		break;

	case 3:
		SetSequenceByName( "right_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueHorse::MoveRight( void )
{
	if ( m_iPosition == 2 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_right" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 3;
		break;

	case 1:
		SetSequenceByName( "left_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueHorse::MoveUp( void )
{
	if ( m_iPosition == 1 || m_iPosition == 2 || m_iPosition == 3 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_up" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 2;
		break;

	case 4:
		SetSequenceByName( "back_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}

void CStatueHorse::MoveDown( void )
{
	if ( m_iPosition == 1 || m_iPosition == 3 || m_iPosition == 4 )
		return;

	switch ( m_iPosition )
	{
	case 0:
		SetSequenceByName( "centre_to_back" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 4;
		break;

	case 2:
		SetSequenceByName( "up_to_centre" );
		EMIT_SOUND( ENT( pev ), CHAN_BODY, "statue_puzzle/statue_move.wav", 1, 0.8 );
		m_iPosition = 0;
		break;
	}

	CheckStatuePuzzle();
}
