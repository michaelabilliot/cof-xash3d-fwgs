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
// cof_3dkeypad.cpp - keypad model the player types a code on
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "animation.h"
#include "monsterevent.h"
#include "player.h"
#include "items.h"

extern int gmsgAttachCam;

class CKeypadNew : public CBaseAnimating
{
	char szCode[256];
	CBasePlayer *m_pPlayer;
	int m_iLastPressed;
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE; }
	void EXPORT KeypadUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT KeypadThink( void );
	void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char *szSequence );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EnterNumber( int number );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
};

LINK_ENTITY_TO_CLASS( cof_keypadnew, CKeypadNew );

TYPEDESCRIPTION CKeypadNew::m_SaveData[] =
{
	DEFINE_FIELD( CKeypadNew, m_pPlayer, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CKeypadNew, CBaseAnimating );

void CKeypadNew::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "number" ) )
	{
		pev->iuser1 = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CKeypadNew::Spawn( void )
{
	char szTest[256];

	Precache( );

	if ( pev->model )
	{
		SET_MODEL( ENT( pev ), STRING( pev->model ) );
	}
	else
	{
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( 0.1 );
		return;
	}

	strcpy( szCode, "" );

	if ( !pev->iuser1 )
		pev->iuser1 = 1111;

	// number of digits of the code
	itoa( pev->iuser1, szTest, 10 );
	pev->iuser3 = strlen( szTest );

	pev->fuser1 = gpGlobals->time;

	UTIL_SetOrigin( this, pev->origin );

	pev->solid = SOLID_NOT;

	SetUse( &CKeypadNew::KeypadUse );
}

void CKeypadNew::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case 1004:
		EMIT_SOUND( edict(), CHAN_BODY, pEvent->options, 1, ATTN_IDLE );
		break;
	}
}

void CKeypadNew::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING( pev->model ) );

	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING( pev->noise ) );

	if ( pev->noise2 )
		PRECACHE_SOUND( (char *)STRING( pev->noise2 ) );

	if ( pev->noise3 )
		PRECACHE_SOUND( (char *)STRING( pev->noise3 ) );
}

void CKeypadNew::KeypadUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer && pPlayer->IsPlayer() && pPlayer->IsAlive() )
	{
		m_pPlayer = GetClassPtr( (CBasePlayer *)pPlayer->pev );

		if ( m_pPlayer )
		{
			pev->fuser1 = gpGlobals->time;

			pev->iuser2 = 0;

			SetNextThink( 0.1 );
			SetThink( &CKeypadNew::KeypadThink );
			pev->body = 1;

			// look at the keypad through its camera
			m_pPlayer->m_pAttachedCamera = this;
			m_pPlayer->m_pClientAttachedCamera = m_pPlayer->m_pAttachedCamera;

			m_pPlayer->m_iUsingComputerOrPhone = 1;

			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, m_pPlayer->pev );
				WRITE_SHORT( ENTINDEX( m_pPlayer->m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
				WRITE_LONG( m_pPlayer->GetGunPosition().x );
				WRITE_LONG( m_pPlayer->GetGunPosition().y );
				WRITE_LONG( m_pPlayer->GetGunPosition().z );
			MESSAGE_END();

			SetSequenceByName( "start_keypad" );

			pev->fuser2 = gpGlobals->time + 1;
		}
	}
}

//=========================================================
// SetSequenceByName
//=========================================================
void CKeypadNew::SetSequenceByName( char *szSequence )
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

void CKeypadNew::EnterNumber( int number )
{
	char szNum[4];
	itoa( number, szNum, 10 );

	strcat( szCode, szNum );

	// all digits are in, check the code
	if ( (int)strlen( szCode ) >= pev->iuser3 )
	{
		int check = atoi( szCode );

		if ( check == pev->iuser1 )
		{
			EMIT_SOUND( edict(), CHAN_ITEM, STRING( pev->noise3 ), 1, ATTN_IDLE );

			if ( pev->target && m_pPlayer )
				FireTargets( STRING( pev->target ), m_pPlayer, m_pPlayer, USE_TOGGLE, 0 );
		}
		else
		{
			EMIT_SOUND( edict(), CHAN_ITEM, STRING( pev->noise2 ), 1, ATTN_IDLE );
		}

		strcpy( szCode, "" );

		pev->fuser1 = gpGlobals->time + 1;
	}
}

void CKeypadNew::KeypadThink( void )
{
	float flInterval = StudioFrameAdvance( 0 );
	DispatchAnimEvents( flInterval );

	// wait until the last key is released
	if ( m_iLastPressed )
	{
		if ( GetAsyncKeyState( m_iLastPressed ) & 0x8000 )
		{
			SetNextThink( 0 );
			return;
		}

		m_iLastPressed = 0;
	}

	// the end animation is over, give the view back to the player
	if ( pev->iuser2 && pev->fuser2 <= gpGlobals->time )
	{
		m_pPlayer->m_pClientAttachedCamera = NULL;
		m_pPlayer->m_pAttachedCamera = NULL;

		pev->body = 0;

		m_pPlayer->m_iUsingComputerOrPhone = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, m_pPlayer->pev );
			WRITE_SHORT( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		return;
	}

	if ( pev->fuser1 <= gpGlobals->time )
	{
		if ( ( GetAsyncKeyState( VK_RETURN ) & 0x8000 ) || ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 ) )
		{
			SetSequenceByName( "end_keypad" );
			pev->iuser2 = 1;
			pev->fuser2 = gpGlobals->time + 1;
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD1 ) & 0x8000 ) || ( GetAsyncKeyState( '1' ) & 0x8000 ) )
		{
			SetSequenceByName( "press1" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD1 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD1;
			else
				m_iLastPressed = '1';

			EnterNumber( 1 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD2 ) & 0x8000 ) || ( GetAsyncKeyState( '2' ) & 0x8000 ) )
		{
			SetSequenceByName( "press2" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD2 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD2;
			else
				m_iLastPressed = '2';

			EnterNumber( 2 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD3 ) & 0x8000 ) || ( GetAsyncKeyState( '3' ) & 0x8000 ) )
		{
			SetSequenceByName( "press3" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD3 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD3;
			else
				m_iLastPressed = '3';

			EnterNumber( 3 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD4 ) & 0x8000 ) || ( GetAsyncKeyState( '4' ) & 0x8000 ) )
		{
			SetSequenceByName( "press4" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD4 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD4;
			else
				m_iLastPressed = '4';

			EnterNumber( 4 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD5 ) & 0x8000 ) || ( GetAsyncKeyState( '5' ) & 0x8000 ) )
		{
			SetSequenceByName( "press5" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD5 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD5;
			else
				m_iLastPressed = '5';

			EnterNumber( 5 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD6 ) & 0x8000 ) || ( GetAsyncKeyState( '6' ) & 0x8000 ) )
		{
			SetSequenceByName( "press6" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD6 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD6;
			else
				m_iLastPressed = '6';

			EnterNumber( 6 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD7 ) & 0x8000 ) || ( GetAsyncKeyState( '7' ) & 0x8000 ) )
		{
			SetSequenceByName( "press7" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD7 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD7;
			else
				m_iLastPressed = '7';

			EnterNumber( 7 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD8 ) & 0x8000 ) || ( GetAsyncKeyState( '8' ) & 0x8000 ) )
		{
			SetSequenceByName( "press8" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD8 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD8;
			else
				m_iLastPressed = '8';

			EnterNumber( 8 );
		}

		if ( ( GetAsyncKeyState( VK_NUMPAD9 ) & 0x8000 ) || ( GetAsyncKeyState( '9' ) & 0x8000 ) )
		{
			SetSequenceByName( "press9" );
			pev->fuser1 = gpGlobals->time + 0.2;
			pev->fuser2 = gpGlobals->time + 0.33;

			if ( GetAsyncKeyState( VK_NUMPAD9 ) & 0x8000 )
				m_iLastPressed = VK_NUMPAD9;
			else
				m_iLastPressed = '9';

			EnterNumber( 9 );
		}
	}

	SetNextThink( 0 );
}
