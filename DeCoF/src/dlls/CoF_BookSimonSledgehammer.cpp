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
// Book Simon with the sledgehammer
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"animation.h"
#include	"player.h"

extern int gmsgParticle;


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	BOOKSIMON_AE_HIT_guess				0x01
#define	BOOKSIMON_AE_KILL_guess				0x02
#define	BOOKSIMON_AE_KILL2_guess			0x03

class CBookSimonSledgehammer : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void SetActivity ( Activity NewActivity );
	void MonsterThink( void );
	void DeathNotice ( entvars_t *pevChild );
	void Killed( entvars_t *pevAttacker, int iGib );

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_booksimonsledgehammer, CBookSimonSledgehammer );

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CBookSimonSledgehammer :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

void CBookSimonSledgehammer :: SetActivity ( Activity NewActivity )
{
	if ( pev->frags != 0 )
		return;

	if ( m_IdealActivity == ACT_IDLE )
		m_IdealActivity = ACT_SIGNAL2;

	int	iSequence = ACTIVITY_NOT_AVAILABLE;

	switch ( NewActivity )
	{
	case ACT_WALK:
		iSequence = LookupActivity ( ACT_WALK_HURT );
		break;
	case ACT_RUN:
		iSequence = LookupActivity ( ACT_WALK_HURT );
		break;
	case ACT_MELEE_ATTACK1:
		iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
		break;
	case ACT_IDLE:
		iSequence = LookupActivity ( ACT_SIGNAL2 );
		break;
	default:
		iSequence = LookupActivity ( NewActivity );
		break;
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			pev->frame = 0;
		}

		pev->sequence		= iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
		SetYawSpeed();
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_debug, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBookSimonSledgehammer :: SetYawSpeed ( void )
{
	int ys;

	ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );

	pev->yaw_speed = ys;
}

void CBookSimonSledgehammer :: DeathNotice ( entvars_t *pevChild )
{
	pev->health = 0;
	Killed( pev, GIB_NEVER );
}

int CBookSimonSledgehammer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

	// pass the damage on to the entity named in our message field
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

	if ( pTarget )
	{
		CBaseMonster *pMonster = pTarget->MyMonsterPointer();
		CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

		// fire its death trigger now, the player gets the credit
		if ( flDamage >= pev->health && pMonster && player && pMonster->m_iszTriggerTarget )
		{
			FireTargets( STRING( pMonster->m_iszTriggerTarget ), player, player, USE_TOGGLE, 0 );
			pMonster->m_iTriggerCondition = 0;
			pMonster->m_iszTriggerTarget = 0;
		}

		pTarget->pev->owner = edict();
		pTarget->TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CBookSimonSledgehammer :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case BOOKSIMON_AE_HIT_guess:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 30, DMG_CLUB, false, 0 );

			if ( pHurt )
			{
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, "weapons/sledgehammer/sledgehammer_hitbody.wav", 1.0, 0.8, 0, 100 + RANDOM_LONG(-5,5) );
			}
		}
		break;

		case BOOKSIMON_AE_KILL_guess:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 1, DMG_CLUB, false, 0 );

			if ( pHurt )
			{
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, "weapons/sledgehammer/sledgehammer_hitbody.wav", 1.0, 0.8, 0, 100 + RANDOM_LONG(-5,5) );
				pHurt->pev->health = 0;
				pHurt->Killed( pev, GIB_NEVER );
			}
		}
		break;

		case BOOKSIMON_AE_KILL2_guess:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 1, DMG_CLUB, false, 0 );

			if ( pHurt )
			{
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, "weapons/sledgehammer/sledgehammer_hitbody.wav", 1.0, 0.8, 0, 100 + RANDOM_LONG(-5,5) );
				pHurt->pev->health = 0;
				pHurt->Killed( pev, GIB_NEVER );
			}
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CBookSimonSledgehammer :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/booksimon.mdl");

	int scale = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -scale, -scale, 0 ), Vector( scale, scale, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	SetBodygroup( 0, 4 );
	SetBodygroup( 1, 0 );

	if ( pev->health == 0 )
		pev->health		= 50;

	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();

	SetBodygroup( 2, 1 );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBookSimonSledgehammer :: Precache()
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/booksimon.mdl");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

int CBookSimonSledgehammer::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();

	iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);

	return iIgnore;
}

void CBookSimonSledgehammer :: Killed( entvars_t *pevAttacker, int iGib )
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

void CBookSimonSledgehammer :: MonsterThink( void )
{
	if ( m_IdealActivity == ACT_IDLE )
		m_IdealActivity = ACT_SIGNAL2;

	// light the flare the first time an enemy shows up
	if ( !pev->iuser1 && m_hEnemy )
	{
		pev->iuser1 = 1;

		pev->renderfx = 68;

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/signal.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/booksimon.mdl" );
			WRITE_SHORT( 3 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/blacksmoke.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/booksimon.mdl" );
			WRITE_SHORT( 3 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_sparks.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/booksimon.mdl" );
			WRITE_SHORT( 3 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/flare_glow.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/booksimon.mdl" );
			WRITE_SHORT( 3 );
		MESSAGE_END();

		EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/flare/flare_burn.wav", 0.3, 0.8 );
		EMIT_SOUND( ENT(pev), CHAN_VOICE, "weapons/flare/flare_ignite.wav", 1, 0.8 );
		pev->teleport_time = gpGlobals->time + 6.9;
	}

	if ( pev->teleport_time < gpGlobals->time && pev->iuser1 )
	{
		EMIT_SOUND( ENT(pev), CHAN_ITEM, "weapons/flare/flare_burn.wav", 0.3, 0.8 );
		pev->teleport_time = gpGlobals->time + 6.9;
	}

	CBaseMonster::MonsterThink();
}
