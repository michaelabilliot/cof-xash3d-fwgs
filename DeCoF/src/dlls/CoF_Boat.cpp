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
// boat - the rowing boat the player rides
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "animation.h"
#include "studio.h"
#include "shake.h"
#include "cdll_dll.h"
#include "movewith.h"

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

extern int gmsgBoat;
extern int gmsgRipple;

// The 1.55 debug build declares this class in cof_importantclassdefs.h.
class CBoat : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT BoatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float boattime1;
	float boattime2;
	float boattime3;
	float boattime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT BoatThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

LINK_ENTITY_TO_CLASS( boat, CBoat );

TYPEDESCRIPTION CBoat::m_SaveData[] =
{
	DEFINE_FIELD( CBoat, boattime1, FIELD_TIME ),
	DEFINE_FIELD( CBoat, boattime2, FIELD_TIME ),
	DEFINE_FIELD( CBoat, boattime3, FIELD_TIME ),
	DEFINE_FIELD( CBoat, boattime4, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CBoat, CBaseAnimating );

void CBoat::SetCollision( void )
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( ENT( pev ) );

	if ( pstudiohdr == NULL )
	{
		ALERT( at_console, "Unable to fetch model pointer!\n" );
		return;
	}

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize( pev, pseqdesc[ pev->sequence ].bbmin, pseqdesc[ pev->sequence ].bbmax );
}

void CBoat::ExitCheck( void )
{
	CBaseEntity *pExitEnt = NULL;

	if ( !m_pPlayer )
		return;

	if ( pExitEntity )
		return;

	while ( ( pExitEnt = UTIL_FindEntityInSphere( pExitEnt, pev->origin, 300 ) ) != NULL )
	{
		if ( FStrEq( STRING( pExitEnt->pev->classname ), "boat_exit" ) )
		{
			pExitEntity = pExitEnt;
			UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 5, 255, FFADE_OUT | FFADE_STAYOUT );
			SetThink( &CBoat::Disembark );
			SetNextThink( 1 );
		}
	}
}

void CBoat::Disembark( void )
{
	if ( !m_pPlayer || !pExitEntity )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, m_pPlayer->pev );
		WRITE_SHORT( -1 );
	MESSAGE_END();

	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;

	UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 0, 255, 0 );

	// put the player on the dock
	UTIL_SetOrigin( m_pPlayer, pExitEntity->pev->origin );
	UTIL_SetAngles( m_pPlayer, pExitEntity->pev->angles );

	// and move the boat to where the dock wants it
	pev->origin.x = pExitEntity->pev->iuser1;
	pev->origin.y = pExitEntity->pev->iuser2;
	pev->origin.z = pExitEntity->pev->iuser3;
	pev->angles.y = pExitEntity->pev->iuser4;

	SetSequenceByName( "idle" );
	pev->body = 1;
	m_pPlayer->EnableControl( TRUE );

	if ( pev->iuser3 == 1 )
	{
		pev->iuser3 = 2;
		m_pPlayer->PlayerStopMP3( 2 );
	}

	m_pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;

	m_pPlayer->m_pBoat = NULL;
	m_pPlayer = NULL;
	pExitEntity = NULL;

	SetUse( &CBoat :: BoatUse );
	SetThink( &CBoat :: BoatThink );
	SetNextThink( 0 );

	pev->iuser1 = 0;
}

void CBoat::MoveForward( void )
{
	if ( pev->iuser1 == 2 )
		return;

	UTIL_MakeVectors( pev->angles );

	pev->velocity = pev->velocity + gpGlobals->v_forward * 2.5;

	float length = pev->velocity.Length();

	if ( length > 70 )
		pev->velocity = pev->velocity.Normalize() * 70;

	if ( pev->iuser1 != 1 )
		SetSequenceByName( "row_forward" );

	boattime3 = gpGlobals->time + 0.5;
	pev->iuser1 = 1;
}

void CBoat::MoveBack( void )
{
	if ( pev->iuser1 == 1 )
		return;

	UTIL_MakeVectors( pev->angles );

	pev->velocity = pev->velocity - gpGlobals->v_forward * 2.5;

	float length = pev->velocity.Length();

	if ( length > 70 )
		pev->velocity = pev->velocity.Normalize() * 70;

	if ( pev->iuser1 != 2 )
		SetSequenceByName( "row_backwards" );

	boattime3 = gpGlobals->time + 0.2;
	pev->iuser1 = 2;
}

void CBoat::TurnLeft( void )
{
	UTIL_MakeVectors( pev->angles );

	pev->avelocity.y += 0.25;

	float length = pev->avelocity.Length();

	if ( length > 20 )
		pev->avelocity = pev->avelocity.Normalize() * 20;

	if ( pev->iuser1 == 0 )
		SetSequenceByName( "row_left_for" );

	boattime3 = gpGlobals->time + 0.2;

	if ( pev->iuser2 != 1 && pev->iuser2 != 5 )
		pev->iuser2 = 1;
}

void CBoat::TurnRight( void )
{
	UTIL_MakeVectors( pev->angles );

	pev->avelocity.y -= 0.25;

	float length = pev->avelocity.Length();

	if ( length > 20 )
		pev->avelocity = pev->avelocity.Normalize() * 20;

	if ( pev->iuser1 == 0 )
		SetSequenceByName( "row_right_for" );

	boattime3 = gpGlobals->time + 0.2;

	if ( pev->iuser2 != 2 && pev->iuser2 != 5 )
		pev->iuser2 = 2;
}

void CBoat::SetSequenceByName( char *szSequence )
{
	int	iSequence;
	iSequence = LookupSequence ( szSequence );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence named:%f\n", STRING(pev->classname), szSequence );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

void CBoat::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch ( pEvent->event )
	{
	case 3:
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "boat/row_boat1.wav", 1, ATTN_NONE, 0, 100 );
			break;

		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "boat/row_boat2.wav", 1, ATTN_NONE, 0, 100 );
			break;

		case 2:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "boat/row_boat3.wav", 1, ATTN_NONE, 0, 100 );
			break;
		}
		break;

	default:
		CBaseAnimating::HandleAnimEvent( pEvent );
		break;
	}
}

void Splash( Vector origin )
{
}

void CBoat::BoatThink( void )
{
	float flInterval = StudioFrameAdvance( );

	Vector attachmentorg, attachmentangles;
	Vector attachment2org, attachment2angles;

	GetAttachment( 1, attachmentorg, attachmentangles );
	GetAttachment( 2, attachment2org, attachment2angles );

	// tell the player about docks nearby
	CBaseEntity *pExitEnt = NULL;

	while ( ( pExitEnt = UTIL_FindEntityInSphere( pExitEnt, pev->origin, 300 ) ) != NULL )
	{
		if ( FStrEq( STRING( pExitEnt->pev->classname ), "boat_exit" ) )
		{
			if ( pExitEnt != pExitTeller && m_pPlayer )
			{
				pExitTeller = pExitEnt;

				if ( pExitEnt->pev->message )
					UTIL_ProMessage( m_pPlayer, UTIL_VarArgs( "Press USE to dock at %s", STRING( pExitEnt->pev->message ) ), 5, false );
				else
					UTIL_ProMessage( m_pPlayer, "Press USE to dock here", 5, false );
			}
		}
	}

	// splashes when the oars enter the water
	if ( UTIL_PointContents( attachmentorg ) != CONTENTS_WATER && UTIL_PointContents( attachment2org ) != CONTENTS_WATER )
	{
		if ( pev->frags != 0 )
		{
			Splash( attachmentorg );
			Splash( attachment2org );
			pev->frags = 0;
		}
	}

	if ( UTIL_PointContents( attachmentorg ) == CONTENTS_WATER && UTIL_PointContents( attachment2org ) != CONTENTS_WATER )
	{
		if ( pev->frags != 1 )
		{
			Splash( attachmentorg );
			pev->frags = 1;
		}
	}

	if ( UTIL_PointContents( attachmentorg ) != CONTENTS_WATER && UTIL_PointContents( attachment2org ) == CONTENTS_WATER )
	{
		if ( pev->frags != 2 )
		{
			Splash( attachment2org );
			pev->frags = 2;
		}
	}

	if ( UTIL_PointContents( attachmentorg ) == CONTENTS_WATER && UTIL_PointContents( attachment2org ) == CONTENTS_WATER )
	{
		if ( pev->frags != 3 )
		{
			Splash( attachmentorg );
			Splash( attachment2org );
			pev->frags = 3;
		}
	}

	// the rower is only drawn when nobody rides the boat
	if ( m_pPlayer )
	{
		if ( pev->body != 0 )
			pev->body = 0;
	}
	else
	{
		if ( pev->body != 1 )
			pev->body = 1;
	}

	if ( pev->avelocity.y > 2 || pev->avelocity.y < -2 )
	{
		if ( pev->iuser2 == 1 )
		{
			pev->iuser2 = 5;
			SetBoneController( 0, -60 );
		}

		if ( pev->iuser2 == 2 )
		{
			pev->iuser2 = 5;
			SetBoneController( 0, 60 );
		}
	}
	else
	{
		pev->iuser2 = 0;
		SetBoneController( 0, 0 );
	}

	if ( boattime2 < gpGlobals->time )
	{
		UTIL_MakeVectors( pev->angles );
		pev->velocity = pev->velocity * 0.85;
		pev->avelocity = pev->avelocity * 0.85;

		float length = pev->velocity.Length();

		if ( length > 70 )
			pev->velocity = pev->velocity.Normalize() * 70;

		float length2 = pev->avelocity.Length();

		if ( length2 > 20 )
			pev->avelocity = pev->avelocity.Normalize() * 20;

		boattime2 = gpGlobals->time + 0.1;
	}

	// ripples around the oars
	if ( boattime1 < gpGlobals->time && pev->frags != 0 )
	{
		Vector attachmentorg, attachmentangles;
		GetAttachment( 1, attachmentorg, attachmentangles );

		float offsetx = RANDOM_FLOAT( 0, 1 );
		float offsety = RANDOM_FLOAT( 0, 1 );

		if ( pev->frags == 1 || pev->frags == 3 )
		{
			while ( UTIL_PointContents( attachmentorg ) == CONTENTS_WATER )
				attachmentorg.z += 1;

			MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
				WRITE_COORD( attachmentorg.x + offsetx );
				WRITE_COORD( attachmentorg.y + offsety );
				WRITE_COORD( attachmentorg.z );
				WRITE_SHORT( 10 );
			MESSAGE_END();
		}

		offsetx = RANDOM_FLOAT( 0, 1 );
		offsety = RANDOM_FLOAT( 0, 1 );

		GetAttachment( 2, attachmentorg, attachmentangles );

		if ( pev->frags == 2 || pev->frags == 3 )
		{
			while ( UTIL_PointContents( attachmentorg ) == CONTENTS_WATER )
				attachmentorg.z += 1;

			MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
				WRITE_COORD( attachmentorg.x + offsetx );
				WRITE_COORD( attachmentorg.y + offsety );
				WRITE_COORD( attachmentorg.z );
				WRITE_SHORT( 10 );
			MESSAGE_END();
		}

		boattime1 = gpGlobals->time + 0.1;
	}

	// ripples around the boat
	if ( boattime4 < gpGlobals->time )
	{
		if ( pev->velocity.Length() > 10 )
		{
			for ( int i = 1; i <= 3; i++ )
			{
				float offsetx = RANDOM_FLOAT( 0, 4 );
				float offsety = RANDOM_FLOAT( 0, 4 );

				MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
					WRITE_COORD( offsetx + pev->origin.x );
					WRITE_COORD( offsety + pev->origin.y );
					WRITE_COORD( pev->origin.z );
					WRITE_SHORT( 60 );
				MESSAGE_END();
			}
		}

		boattime4 = gpGlobals->time + 0.25;
	}

	if ( boattime3 < gpGlobals->time && pev->sequence != LookupSequence( "idle" ) )
	{
		SetSequenceByName( "idle" );
		pev->iuser1 = 0;
		pev->frags = 0;
	}

	SetNextThink( 0 );

	DispatchAnimEvents( flInterval );
}

void CBoat::Spawn( void )
{
	Precache();

	SET_MODEL( ENT( pev ), "models/boat.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->movetype = MOVETYPE_FLY;

	pev->solid = SOLID_SLIDEBOX;

	UTIL_SetSize( pev, Vector( -25, -25, -8 ), Vector( 25, 25, 56 ) );

	pev->friction = 1.5;
	pev->gravity = 0.8;

	pev->body = 1;

	SetSequenceByName( "idle" );

	SetUse( &CBoat :: BoatUse );
	SetThink( &CBoat :: BoatThink );
	SetNextThink( 0 );
}

void CBoat::Precache( void )
{
	PRECACHE_MODEL( "models/boat.mdl" );
	PRECACHE_SOUND( "boat/row_boat1.wav" );
	PRECACHE_SOUND( "boat/row_boat2.wav" );
	PRECACHE_SOUND( "boat/row_boat3.wav" );
	PRECACHE_SOUND( "boat/boat_idle.wav" );
}

void CBoat::Embark( void )
{
	if ( !m_pPlayer )
		return;

	SetUse( &CBoat :: BoatUse );
	SetThink( &CBoat :: BoatThink );
	SetNextThink( 0 );

	// the player waits out of sight while rowing
	CBaseEntity *pHide = UTIL_FindEntityByClassname( NULL, "boat_hide_spot" );

	MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, m_pPlayer->pev );
		WRITE_SHORT( ENTINDEX( edict() ) );
	MESSAGE_END();

	if ( pHide )
		UTIL_SetOrigin( m_pPlayer, pHide->pev->origin );

	EMIT_SOUND_DYN( edict(), CHAN_BODY, "boat/boat_idle.wav", 1, ATTN_NONE, 0, 100 );

	m_pPlayer->m_pBoat = this;

	if ( pev->iuser3 == 0 )
	{
		pev->iuser3 = 1;
		m_pPlayer->PlayerSetMP3( MAKE_STRING( "boat.mp3" ), 1, 1 );
	}

	UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 0, 255, 0 );

	m_pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;

	if ( pev->iuser4 == 0 )
	{
		pev->iuser4 = 1;
		UTIL_ProMessage( m_pPlayer, "Use movement keys to move the boat, USE key to disembark", 5, false );
	}
}

void CBoat::BoatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( !m_pPlayer )
		{
			pExitEntity = NULL;
			pExitTeller = NULL;

			m_pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );
			m_pPlayer->EnableControl( FALSE );
			SetThink( &CBoat::Embark );
			SetNextThink( 1 );

			UTIL_ScreenFade( m_pPlayer, Vector( 0, 0, 0 ), 1, 5, 255, FFADE_OUT | FFADE_STAYOUT );
		}
	}
}

class CBoatHideSpot : public CPointEntity
{
	void Spawn( void );
};

void CBoatHideSpot::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( boat_hide_spot, CBoatHideSpot );

class CBoatExitSpot : public CPointEntity
{
	void Spawn( void );
};

void CBoatExitSpot::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( boat_exit, CBoatExitSpot );

// Legacy combined input packet: opposite directions cancel before native timers.
void CoFR_BoatInput(CBasePlayer* p,unsigned mask){
 if(!p||!p->IsAlive()||!p->m_pBoat||mask>15)return;
 if((mask&3)==3)mask&=~3u;if((mask&12)==12)mask&=~12u;
 auto boat=static_cast<CBoat*>((CBaseEntity*)p->m_pBoat);if(!boat||!FClassnameIs(boat->pev,"boat"))return;
 if(mask&1)boat->MoveForward();if(mask&2)boat->MoveBack();if(mask&4)boat->TurnLeft();if(mask&8)boat->TurnRight();
}
