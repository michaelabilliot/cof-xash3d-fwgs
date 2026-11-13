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
// cof_telephone.cpp - telephone the player dials a number on
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"

extern int gmsgTelephone;

// declared in cof_importantclassdefs.h
class CCofTelephone : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void TelephoneRight( void );
	void TelephoneWrong( void );
	void EXPORT RightNumber( void );
	void EXPORT WrongNumber( void );
	void EXPORT DialTone( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	EHANDLE m_pPlayer;
};

TYPEDESCRIPTION CCofTelephone::m_SaveData[] =
{
	DEFINE_FIELD( CCofTelephone, m_pPlayer, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CCofTelephone, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_telephone, CCofTelephone );

void CCofTelephone::Spawn( void )
{
	Precache( );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofTelephone::Precache( void )
{
	PRECACHE_SOUND( "phone/telephone_button_press.wav" );
	PRECACHE_SOUND( "phone/telephone_dial_tone.wav" );
	PRECACHE_SOUND( "phone/telephone_dialing.wav" );
	PRECACHE_SOUND( "phone/telephone_receiver_down.wav" );
	PRECACHE_SOUND( "phone/telephone_receiver_up.wav" );
	PRECACHE_SOUND( "phone/telephone_wrong_number.wav" );
}

void CCofTelephone::DialTone( void )
{
	if ( m_pPlayer )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTelephone, NULL, m_pPlayer->pev );
			WRITE_BYTE( 2 );
			WRITE_STRING( STRING( pev->message ) );
		MESSAGE_END();
	}
}

void CCofTelephone::RightNumber( void )
{
	if ( m_pPlayer )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgTelephone, NULL, m_pPlayer->pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( STRING( pev->message ) );
		MESSAGE_END();

		GetClassPtr( (CBasePlayer *)m_pPlayer->pev )->m_pTelephone = NULL;
	}

	if ( pev->target )
		FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
}

void CCofTelephone::WrongNumber( void )
{
	if ( m_pPlayer )
	{
		SetThink( &CCofTelephone::DialTone );
		SetNextThink( 6.1 );

		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "phone/telephone_wrong_number.wav", 1, 0.8, 0, 100 );
	}
}

void CCofTelephone::TelephoneRight( void )
{
	// the right number was dialed, the telephone cannot be used anymore
	pev->health = 1;

	SetThink( &CCofTelephone::RightNumber );
	SetNextThink( 7 );

	MESSAGE_BEGIN( MSG_ONE, gmsgTelephone, NULL, m_pPlayer->pev );
		WRITE_BYTE( 3 );
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();

	if ( m_pPlayer )
		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "phone/telephone_dialing.wav", 1, 0.8, 0, 100 );
}

void CCofTelephone::TelephoneWrong( void )
{
	SetThink( &CCofTelephone::WrongNumber );
	SetNextThink( 7 );

	MESSAGE_BEGIN( MSG_ONE, gmsgTelephone, NULL, m_pPlayer->pev );
		WRITE_BYTE( 3 );
		WRITE_STRING( STRING( pev->message ) );
	MESSAGE_END();

	if ( m_pPlayer )
		EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_AUTO, "phone/telephone_dialing.wav", 1, 0.8, 0, 100 );
}

void CCofTelephone::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( pev->health != 0 )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		pPlayer->m_pTelephone = this;
		m_pPlayer = pPlayer;

		MESSAGE_BEGIN( MSG_ONE, gmsgTelephone, NULL, pPlayer->pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( STRING( pev->message ) );
		MESSAGE_END();
	}
}
