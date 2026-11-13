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
// cof_barshoot.cpp - bars that make the doctor complain when shot
//

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "player.h"

class CBarShoot : public CBaseDelay
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void )
	{
		return CLASS_NONE;
	}
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual int ObjectCaps( void )
	{
		return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION;
	}

	static const char *pSoundLengths[];
	static const char *pVoiceSounds[];
};

const char *CBarShoot::pSoundLengths[] =
{
	"4.5",
	"4.0",
	"2.5",
	"2.0",
	"2.5",
	"3.0",
};

const char *CBarShoot::pVoiceSounds[] =
{
	"doc_ai/be_careful_dont_make_me_angry.wav",
	"doc_ai/dont_make_me_angry.wav",
	"doc_ai/simon_stop.wav",
	"doc_ai/stop.wav",
	"doc_ai/stop_doing_that.wav",
	"doc_ai/what_u_doin.wav",
};

LINK_ENTITY_TO_CLASS( cof_barshoot, CBarShoot );

void CBarShoot::Spawn( void )
{
	Precache( );

	pev->takedamage = DAMAGE_YES;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	pev->teleport_time = gpGlobals->time;

	// last voice sound played
	pev->frags = -1;

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
}

void CBarShoot::Precache( void )
{
	int i;

	for ( i = 0; i < ARRAYSIZE( pVoiceSounds ); i++ )
		PRECACHE_SOUND( (char *)pVoiceSounds[i] );

	PRECACHE_SOUND( "doc_ai/bar_ricochet.wav" );
}

void CBarShoot::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	UTIL_Sparks( ptr->vecEndPos );
	UTIL_EmitAmbientSound( ENT( 0 ), ptr->vecEndPos, "doc_ai/bar_ricochet.wav", 1.5, 0, 0, 100 + RANDOM_LONG( -10, 10 ) );

	// the doctor is not talking anymore
	if ( pev->teleport_time < gpGlobals->time )
	{
		int soundfile = 0;

choose_sound:
		soundfile = RANDOM_LONG( 0, 5 );

		// never the same line twice in a row
		if ( soundfile == pev->frags )
		{
			goto choose_sound;
		}

		pev->frags = soundfile;

		if ( pev->message )
		{
			CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

			if ( pEntity )
			{
				pev->teleport_time = gpGlobals->time + atof( pSoundLengths[soundfile] );
				EMIT_SOUND_DYN( pEntity->edict(), CHAN_VOICE, pVoiceSounds[soundfile], 1, 0.8, 0, 100 );
			}
		}
	}
}
