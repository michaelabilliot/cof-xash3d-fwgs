/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Doctor boss shoot target: an invisible brush in front of
// each box that passes the damage on to the doctor while he
// aims or shoots from that box.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"player.h"
#include	"cof_doctorboss.h"


class CDoctorBossShoot : public CBaseDelay
{
public:
	void Spawn( void );
	void Precache( void );
	int Classify( void )
	{
		return CLASS_NONE;
	}
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( cof_doctorshoot, CDoctorBossShoot );

void CDoctorBossShoot :: Spawn( void )
{
	Precache( );

	pev->takedamage = DAMAGE_YES;

	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;

	pev->effects = EF_NODRAW;

	pev->teleport_time = gpGlobals->time;
	SET_MODEL( ENT(pev), STRING(pev->model) );
}

void CDoctorBossShoot :: Precache( void )
{
	PRECACHE_SOUND( "doc_ai/bar_ricochet.wav" );
}

void CDoctorBossShoot :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( pev->teleport_time > gpGlobals->time )
		return;

	CDoctorBoss *pDoctor = NULL;
	CBaseEntity *pFind = UTIL_FindEntityByClassname( NULL, "monster_doctorboss" );

	if ( FClassnameIs( pevAttacker, "monster_doctorboss" ) )
		return;

	if ( pFind )
	{
		pDoctor = GetClassPtr( (CDoctorBoss *)pFind->pev );

		if ( pDoctor )
		{
			switch ( pev->iuser1 )
			{
			case 1:
				if ( pDoctor->IsAimingOrShooting() == 1 || pDoctor->IsAimingOrShooting() == 5 )
				{
					pev->teleport_time = gpGlobals->time + 1;
					pDoctor->pev->iuser2 = 1;
					pDoctor->TakeDamage( pevAttacker, pevAttacker, flDamage, bitsDamageType );
					UTIL_BloodDrips( ptr->vecEndPos, vecDir, BLOOD_COLOR_RED, 50, pDoctor->pev->velocity );
				}
				break;

			case 2:
				if ( pDoctor->IsAimingOrShooting() == 2 || pDoctor->IsAimingOrShooting() == 6 )
				{
					pev->teleport_time = gpGlobals->time + 1;
					pDoctor->pev->iuser2 = 1;
					pDoctor->TakeDamage( pevAttacker, pevAttacker, flDamage, bitsDamageType );
					UTIL_BloodDrips( ptr->vecEndPos, vecDir, BLOOD_COLOR_RED, 50, pDoctor->pev->velocity );
				}
				break;

			case 3:
				if ( pDoctor->IsAimingOrShooting() == 3 || pDoctor->IsAimingOrShooting() == 7 )
				{
					pev->teleport_time = gpGlobals->time + 1;
					pDoctor->pev->iuser2 = 1;
					pDoctor->TakeDamage( pevAttacker, pevAttacker, flDamage, bitsDamageType );
					UTIL_BloodDrips( ptr->vecEndPos, vecDir, BLOOD_COLOR_RED, 50, pDoctor->pev->velocity );
				}
				break;

			case 4:
				if ( pDoctor->IsAimingOrShooting() == 4 || pDoctor->IsAimingOrShooting() == 8 )
				{
					pev->teleport_time = gpGlobals->time + 1;
					pDoctor->pev->iuser2 = 1;
					pDoctor->TakeDamage( pevAttacker, pevAttacker, flDamage, bitsDamageType );
					UTIL_BloodDrips( ptr->vecEndPos, vecDir, BLOOD_COLOR_RED, 50, pDoctor->pev->velocity );
				}
				break;
			}
		}
	}
}
