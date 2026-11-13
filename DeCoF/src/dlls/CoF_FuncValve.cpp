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
//=========================================================
// func_valve - a valve wheel the player turns by holding use
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

// The 1.55 debug build declares this class in cof_importantclassdefs.h.
class CFuncValve : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void ValveReturn( void );
	void DegreesCheck( void );
	void Off( void );
	void EXPORT ProThink( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual STATE GetState( void );
	virtual void KeyValue( KeyValueData *pkvd );
	void UnlockValve( CBaseEntity *pActivator );
	void EXPORT FindTargetEnt( void );
	string_t m_sLockedBy;
	string_t m_sLockedMsg;
	string_t m_sUnlockedMsg;
	Vector m_vInitialEntityAngles;
	EHANDLE m_hRotateModel;
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextCharge;
	int m_iOn;
	float m_flSoundTime;
};

TYPEDESCRIPTION CFuncValve::m_SaveData[] =
{
	DEFINE_FIELD( CFuncValve, m_flNextCharge, FIELD_TIME ),
	DEFINE_FIELD( CFuncValve, m_iOn, FIELD_INTEGER ),
	DEFINE_FIELD( CFuncValve, m_flSoundTime, FIELD_TIME ),
	DEFINE_FIELD( CFuncValve, m_hRotateModel, FIELD_EHANDLE ),
	DEFINE_FIELD( CFuncValve, m_vInitialEntityAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncValve, m_sLockedBy, FIELD_STRING ),
	DEFINE_FIELD( CFuncValve, m_sLockedMsg, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CFuncValve, CBaseEntity );

LINK_ENTITY_TO_CLASS( func_valve, CFuncValve );


void CFuncValve::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "lockedby" ) )
	{
		m_sLockedBy = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lockedmsg" ) )
	{
		m_sLockedMsg = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "lockedsnd" ) )
	{
		pev->noise2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "unlockedsnd" ) )
	{
		pev->noise3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "unlockedmsg" ) )
	{
		m_sUnlockedMsg = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CFuncValve::FindTargetEnt( void )
{
	if ( pev->message )
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

		if ( pTarget )
		{
			m_hRotateModel = pTarget;
			m_vInitialEntityAngles = m_hRotateModel->pev->angles;
		}
	}

	SetNextThink( 0.1 );
	SetThink( &CFuncValve::ProThink );
}

void CFuncValve::Spawn( void )
{
	Precache();

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, pev->mins, pev->maxs );
	SET_MODEL( ENT( pev ), STRING( pev->model ) );

	if ( pev->message )
	{
		SetNextThink( 2 );
		SetThink( &CFuncValve::FindTargetEnt );
	}
	else
	{
		SetNextThink( 0.1 );
		SetThink( &CFuncValve::ProThink );
	}

	if ( m_sLockedBy )
	{
		pev->rendermode = kRenderTransAdd;
		pev->renderamt = 0;
	}
	else
	{
		pev->rendermode = kRenderNormal;
		pev->renderamt = 255;
	}

	if ( pev->iuser1 >= 360 )
		pev->iuser1 = 359;

	if ( pev->iuser2 <= 0 )
		pev->iuser2 = 2;

	if ( pev->iuser4 > 2 )
		pev->iuser4 = 2;

	if ( pev->iuser4 < 0 )
		pev->iuser4 = 0;

	pev->iuser3 = 0;
}

void CFuncValve::Precache( void )
{
	PRECACHE_SOUND( "buttons/valve_turn.wav" );
	PRECACHE_SOUND( "buttons/valve_start.wav" );
	PRECACHE_SOUND( "buttons/valve_end.wav" );
	PRECACHE_SOUND( "buttons/placevalve.wav" );
}

void CFuncValve::UnlockValve( CBaseEntity *pActivator )
{
	if ( !pActivator )
		return;

	if ( pev->noise3 )
		EMIT_SOUND( ENT( pev ), CHAN_BODY, STRING( pev->noise3 ), 1, ATTN_NORM );

	if ( m_sUnlockedMsg )
		UTIL_ProMessage( pActivator, STRING( m_sUnlockedMsg ), 5, false );

	pev->rendermode = kRenderNormal;
	pev->renderamt = 255;

	m_sLockedBy = 0;
	m_sLockedMsg = 0;
}

void CFuncValve::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pev->iuser3 > 0 )
		return;

	m_hActivator = pActivator;

	if ( !pActivator->IsPlayer() )
		return;

	if ( m_flNextCharge >= gpGlobals->time )
		return;

	if ( m_sLockedBy )
	{
		if ( m_sLockedMsg )
			UTIL_ProMessage( pActivator, STRING( m_sLockedMsg ), 5, false );

		if ( pev->noise2 )
			EMIT_SOUND( ENT( pev ), CHAN_BODY, STRING( pev->noise2 ), 1, ATTN_NORM );

		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if ( m_iOn == 0 )
	{
		m_iOn++;
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "buttons/valve_start.wav", 1, ATTN_NORM );
		m_flSoundTime = gpGlobals->time + 0.31;
	}

	if ( m_iOn == 1 && m_flSoundTime <= gpGlobals->time )
	{
		m_iOn++;
		EMIT_SOUND( ENT( pev ), CHAN_STATIC, "buttons/valve_turn.wav", 1, ATTN_NORM );
	}

	switch ( pev->iuser4 )
	{
	case 0:
		pev->angles.x += pev->iuser2;

		if ( pev->angles.x > 359 )
			pev->angles.x = 359;
		break;

	case 1:
		pev->angles.y += pev->iuser2;

		if ( pev->angles.y > 359 )
			pev->angles.y = 359;
		break;

	case 2:
		pev->angles.z += pev->iuser2;

		if ( pev->angles.z > 359 )
			pev->angles.z = 359;
		break;
	}

	if ( m_hRotateModel )
	{
		switch ( pev->impulse )
		{
		case 0:
			if ( m_hRotateModel )
			{
				m_hRotateModel->pev->angles.x += pev->iuser2;

				if ( m_hRotateModel->pev->angles.x > 359 )
					m_hRotateModel->pev->angles.x = 359;
			}
			break;

		case 1:
			if ( m_hRotateModel )
			{
				m_hRotateModel->pev->angles.y += pev->iuser2;

				if ( m_hRotateModel->pev->angles.y > 359 )
					m_hRotateModel->pev->angles.y = 359;
			}
			break;

		case 2:
			if ( m_hRotateModel )
			{
				m_hRotateModel->pev->angles.z += pev->iuser2;

				if ( m_hRotateModel->pev->angles.z > 359 )
					m_hRotateModel->pev->angles.z = 359;
			}
			break;
		}
	}

	m_flNextCharge = gpGlobals->time + 0.025;
}

void CFuncValve::ValveReturn( void )
{
	if ( pev->iuser3 )
		return;

	switch ( pev->iuser4 )
	{
	case 0:
		if ( pev->angles.x > 0 )
			pev->angles.x -= pev->iuser2;

		if ( pev->angles.x < 0 )
			pev->angles.x = 0;
		break;

	case 1:
		if ( pev->angles.y > 0 )
			pev->angles.y -= pev->iuser2;

		if ( pev->angles.y < 0 )
			pev->angles.y = 0;
		break;

	case 2:
		if ( pev->angles.z > 0 )
			pev->angles.z -= pev->iuser2;

		if ( pev->angles.z < 0 )
			pev->angles.z = 0;
		break;
	}

	if ( m_hRotateModel )
	{
		switch ( pev->impulse )
		{
		case 0:
			if ( m_hRotateModel->pev->angles.x > m_vInitialEntityAngles.x )
				m_hRotateModel->pev->angles.x -= pev->iuser2;

			if ( m_hRotateModel->pev->angles.x < m_vInitialEntityAngles.x )
				m_hRotateModel->pev->angles.x = m_vInitialEntityAngles.x;
			break;

		case 1:
			if ( m_hRotateModel->pev->angles.y > m_vInitialEntityAngles.y )
				m_hRotateModel->pev->angles.y -= pev->iuser2;

			if ( m_hRotateModel->pev->angles.y < m_vInitialEntityAngles.y )
				m_hRotateModel->pev->angles.y = m_vInitialEntityAngles.y;
			break;

		case 2:
			if ( m_hRotateModel->pev->angles.z > m_vInitialEntityAngles.z )
				m_hRotateModel->pev->angles.z -= pev->iuser2;

			if ( m_hRotateModel->pev->angles.z < m_vInitialEntityAngles.z )
				m_hRotateModel->pev->angles.z = m_vInitialEntityAngles.z;
			break;
		}
	}
}

void CFuncValve::DegreesCheck( void )
{
	if ( pev->iuser3 )
		return;

	switch ( pev->iuser4 )
	{
	case 0:
		if ( pev->iuser1 <= pev->angles.x )
		{
			FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
			SetNextThink( 0 );
			pev->iuser3 = 1;
			STOP_SOUND( ENT( pev ), CHAN_STATIC, "buttons/valve_turn.wav" );
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "buttons/valve_end.wav", 1, ATTN_NORM );
		}
		break;

	case 1:
		if ( pev->iuser1 <= pev->angles.y )
		{
			FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
			SetNextThink( 0 );
			pev->iuser3 = 1;
			STOP_SOUND( ENT( pev ), CHAN_STATIC, "buttons/valve_turn.wav" );
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "buttons/valve_end.wav", 1, ATTN_NORM );
		}
		break;

	case 2:
		if ( pev->iuser1 <= pev->angles.z )
		{
			FireTargets( STRING( pev->target ), this, this, USE_TOGGLE, 0 );
			SetNextThink( 0 );
			pev->iuser3 = 1;
			STOP_SOUND( ENT( pev ), CHAN_STATIC, "buttons/valve_turn.wav" );
			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "buttons/valve_end.wav", 1, ATTN_NORM );
		}
		break;
	}
}

void CFuncValve::ProThink( void )
{
	if ( GetState() == STATE_OFF && !pev->iuser3 )
	{
		ValveReturn();
		m_iOn = 0;
		STOP_SOUND( ENT( pev ), CHAN_STATIC, "buttons/valve_turn.wav" );
	}

	if ( m_hRotateModel )
	{
		if ( m_hRotateModel->pev->rendermode != pev->rendermode )
			m_hRotateModel->pev->rendermode = pev->rendermode;

		if ( m_hRotateModel->pev->renderamt != pev->renderamt )
			m_hRotateModel->pev->renderamt = pev->renderamt;
	}

	DegreesCheck();

	SetNextThink( 0.025 );
}

STATE CFuncValve::GetState( void )
{
	if ( m_flNextCharge < gpGlobals->time )
		return STATE_OFF;
	else
		return STATE_IN_USE;
}
