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
// Saw crazy
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================

#define SAWCRAZY_FLINCH_DELAY			2		// at most one flinch every n secs

class CSawCrazy : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	int IgnoreConditions ( void );

	float m_flNextFlinch;

	void DeathSound( void );
	void MonsterThink( void );
	void EXPORT SawTouch( CBaseEntity *pOther );
	void Killed( entvars_t *pevAttacker, int iGib );

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckMeleeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckMeleeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_sawcrazy, CSawCrazy );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CSawCrazy :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

void CSawCrazy :: DeathSound( void )
{
	EMIT_SOUND_DYN( edict(), CHAN_VOICE, "sawcrazy/death.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSawCrazy :: SetYawSpeed ( void )
{
	int ys;

	ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

int CSawCrazy :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() && !m_bWaitingToDie )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// Spawn
//=========================================================
void CSawCrazy :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/sawcrazy.mdl");

	int iScale_guess = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= 150;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();

	pev->fuser1 = pev->fuser2 = pev->fuser3 = gpGlobals->time;
}

void CSawCrazy :: SawTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	if ( pOther->IsAlive() )
	{
		pOther->pev->health = 0;
		pOther->Killed( pOther->pev, GIB_NEVER );
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSawCrazy :: Precache()
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/sawcrazy.mdl");

	PRECACHE_SOUND("sawcrazy/dblsawloop.wav");
	PRECACHE_SOUND("sawcrazy/death.wav");
	PRECACHE_SOUND("sawcrazy/random1.wav");
	PRECACHE_SOUND("sawcrazy/random2.wav");
	PRECACHE_SOUND("sawrunner/chainsaw_attack_hit.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CSawCrazy::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	if ((m_Activity == ACT_MELEE_ATTACK1) || (m_Activity == ACT_MELEE_ATTACK1))
	{
#if 0
		if (pev->health < 20)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
		else
#endif
		if (m_flNextFlinch >= gpGlobals->time)
			iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);
	}

	if ((m_Activity == ACT_SMALL_FLINCH) || (m_Activity == ACT_BIG_FLINCH))
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + SAWCRAZY_FLINCH_DELAY;
	}

	return iIgnore;

}

void CSawCrazy :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	if ( HasMemory( bits_MEMORY_KILLED ) )
	{
		if ( ShouldGibMonster( iGib ) )
			CallGibMonster();
		return;
	}

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );

	if ( pAttacker && pAttacker->IsPlayer() )
	{
		if ( m_iLastDamageType & (1 << 26) )
			((CBasePlayer *)pAttacker)->playerstats.monsters_killed_melee++;
		else
			((CBasePlayer *)pAttacker)->playerstats.monsters_killed_shooting++;

		GetPointsForKill( (char *)STRING( pev->classname ), (CBasePlayer *)pAttacker );
	}

	STOP_SOUND( ENT(pev), CHAN_AUTO, "sawcrazy/dblsawloop.wav" );

	Remember( bits_MEMORY_KILLED );

	// clear the deceased's sound channels.(may have been firing or reloading when killed)
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM);
	m_IdealMonsterState = MONSTERSTATE_DEAD;
	// Make sure this condition is fired too (TakeDamage breaks out before this happens on death)
	SetConditions( bits_COND_LIGHT_DAMAGE );

	// tell owner ( if any ) that we're dead.This is mostly for MonsterMaker functionality.
	CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
	if ( pOwner )
	{
		pOwner->DeathNotice( pev );
	}

	if	( ShouldGibMonster( iGib ) )
	{
		CallGibMonster();
		return;
	}
	else if ( pev->flags & FL_MONSTER )
	{
		SetTouch( NULL );
		BecomeDead();
	}

	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}

	//pev->enemy = ENT( pevAttacker );//why? (sjb)

	m_IdealMonsterState = MONSTERSTATE_DEAD;
}

//=========================================================
// MonsterThink - the saw loop, the saw hits and the idle
// sounds run on their own timers
//=========================================================
void CSawCrazy :: MonsterThink( void )
{
	if ( pev->fuser3 < gpGlobals->time )
	{
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "sawcrazy/dblsawloop.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		pev->fuser3 = gpGlobals->time + 7;
	}

	if ( pev->fuser2 < gpGlobals->time && IsAlive() )
	{
		CBaseEntity *pHurt = CheckTraceHullAttack( 55, 200, DMG_SLASH, false, 0 );
		if ( pHurt && pHurt->IsPlayer() && pHurt->IsAlive() )
		{
			if ( pHurt->IsPlayer() )
			{
				pHurt->pev->punchangle.z = -18;
				pHurt->pev->punchangle.x = 5;
				pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
			}

			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "sawrunner/chainsaw_attack_hit.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		}

		// CoF: this sets the idle sound timer; fuser2 is never advanced, so the saw hits on every think
		pev->fuser1 = gpGlobals->time + 0.25;
	}

	if ( pev->fuser1 < gpGlobals->time )
	{
		switch ( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "sawcrazy/random1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "sawcrazy/random2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		}

		pev->fuser1 = gpGlobals->time + RANDOM_LONG( 10, 50 );
	}

	CBaseMonster::MonsterThink();
}
