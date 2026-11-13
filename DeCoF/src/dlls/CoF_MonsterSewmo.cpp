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
// Sewmo - rises out of the water
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"animation.h"

extern void UTIL_SplashCalculation( CBaseEntity *pEntity, int splashtype );


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	SEWMO_AE_ATTACK_RIGHT		0x01
#define	SEWMO_AE_ATTACK_LEFT		0x02
#define	SEWMO_AE_ATTACK_BOTH		0x03
#define	SEWMO_AE_BREAK_FREE_guess	0x06

#define SEWMO_FLINCH_DELAY			2		// at most one flinch every n secs

#define SF_SEWMO_HIDDEN_guess		32		// wait in the water until a player comes near

//=========================================================
// monster-specific schedule types
//=========================================================
enum
{
	SCHED_SEWMO_BREAK_FREE_guess,
};

//=========================================================
// monster-specific tasks
//=========================================================
enum
{
	TASK_SEWMO_BREAK_FREE_guess = LAST_COMMON_TASK + 1,
};

class CSewmo : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );

	float m_flNextFlinch;

	CUSTOM_SCHEDULES;
	Schedule_t *GetSchedule( void );
	Schedule_t *GetScheduleOfType ( int Type );

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	void SetActivity ( Activity NewActivity );
	void Killed( entvars_t *pevAttacker, int iGib );
	void EXPORT InactiveThink( void );
	void EXPORT ActivateSewmo( void );

	int m_iActivated;
	int m_iActivating;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_sewmo, CSewmo );

TYPEDESCRIPTION	CSewmo::m_SaveData[] =
{
	DEFINE_FIELD( CSewmo, m_iActivated, FIELD_INTEGER ),
	DEFINE_FIELD( CSewmo, m_iActivating, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSewmo, CBaseMonster );

const char *CSewmo::pAttackHitSounds[] =
{
	"sewmo/claw_strike1.wav",
	"sewmo/claw_strike2.wav",
	"sewmo/claw_strike3.wav",
};

const char *CSewmo::pAttackMissSounds[] =
{
	"sewmo/claw_miss1.wav",
	"sewmo/claw_miss2.wav",
};

const char *CSewmo::pAttackSounds[] =
{
	"sewmo/sewmo_attack1.wav",
	"sewmo/sewmo_attack2.wav",
};

const char *CSewmo::pAlertSounds[] =
{
	"sewmo/sewmo_alert10.wav",
	"sewmo/sewmo_alert20.wav",
	"sewmo/sewmo_alert30.wav",
};

const char *CSewmo::pPainSounds[] =
{
	"sewmo/sewmo_pain1.wav",
	"sewmo/sewmo_pain2.wav",
};

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

// break free from whatever holds us (the body group shows it)
Task_t	tlBreakFree[] =
{
	{ TASK_STOP_MOVING,				(float)0		},
	{ TASK_SEWMO_BREAK_FREE_guess,	(float)0		},
};

Schedule_t	slBreakFree[] =
{
	{
		tlBreakFree,
		ARRAYSIZE ( tlBreakFree ),
		0,
		0,
		"Break Free"
	},
};

DEFINE_CUSTOM_SCHEDULES( CSewmo )
{
	slBreakFree,
};

IMPLEMENT_CUSTOM_SCHEDULES( CSewmo, CBaseMonster );

//=========================================================
// GetScheduleOfType - returns a pointer to one of the
// monster's available schedules of the indicated type.
//=========================================================
Schedule_t* CSewmo :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
	case SCHED_SEWMO_BREAK_FREE_guess:
		{
			return &slBreakFree[ 0 ];
		}
		break;
	}

	return CBaseMonster :: GetScheduleOfType( Type );
}

//=========================================================
// StartTask
//=========================================================
void CSewmo :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_SEWMO_BREAK_FREE_guess:
		{
			m_IdealActivity = ACT_SIGNAL1;
			break;
		}
	default:
		{
			CBaseMonster :: StartTask( pTask );
		}
	}
}

//=========================================================
// RunTask
//=========================================================
void CSewmo :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_SEWMO_BREAK_FREE_guess:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
			}
			break;
		}
	default:
		{
			CBaseMonster :: RunTask( pTask );
		}
	}
}

//=========================================================
// GetSchedule
//=========================================================
Schedule_t *CSewmo :: GetSchedule( void )
{
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_COMBAT:
		{
// dead enemy
			if ( HasConditions( bits_COND_ENEMY_DEAD ) )
			{
				// call base class, all code to handle dead enemies is centralized there.
				return CBaseMonster :: GetSchedule();
			}

			if ( HasConditions( bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE ) )
			{
				if ( pev->max_health / 2 < pev->health || RANDOM_LONG( 0, 1 ) == 1 )
					break;

				if ( !pev->body )
				{
					EMIT_SOUND( ENT(pev), CHAN_ITEM, "sewmo/break_free.wav", 1, ATTN_NORM );
					return GetScheduleOfType( SCHED_SEWMO_BREAK_FREE_guess );
				}
			}
		}
		break;
	}

	return CBaseMonster :: GetSchedule();
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CSewmo :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSewmo :: SetYawSpeed ( void )
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

int CSewmo :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

void CSewmo :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSewmo :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSewmo :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CSewmo :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case SEWMO_AE_BREAK_FREE_guess:
		{
			pev->body = 1;
		}
		break;

		case SEWMO_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash right!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.sewmoDmgOneSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}

				if ( pHurt->Classify() == CLASS_NONE || pHurt->Classify() == CLASS_MACHINE )
				{
					// no hit sound on props and machines
				}
				else
				{
					// Play a random attack hit sound
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
				}
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case SEWMO_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash left!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.sewmoDmgOneSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
				}

				if ( pHurt->Classify() == CLASS_NONE || pHurt->Classify() == CLASS_MACHINE )
				{
					// no hit sound on props and machines
				}
				else
				{
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
				}
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case SEWMO_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.sewmoDmgBothSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}

				if ( pHurt->Classify() == CLASS_NONE || pHurt->Classify() == CLASS_MACHINE )
				{
					// no hit sound on props and machines
				}
				else
				{
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
				}
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
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
void CSewmo :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/sewmo.mdl");
	int iScale_guess = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= gSkillData.sewmoHealth;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	if ( pev->spawnflags & SF_SEWMO_HIDDEN_guess )
	{
		SetSequenceByName( "appearidle" );
		pev->body = 0;
		m_iActivated = 0;
		m_iActivating = 0;
		SetNextThink( 0.1 );
		SetThink( &CSewmo::InactiveThink );
	}
	else
	{
		pev->body = 0;
		m_iActivated = 1;
		m_iActivating = 0;
		MonsterInit();
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSewmo :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/sewmo.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);

	PRECACHE_SOUND("sewmo/out_water.wav");
	PRECACHE_SOUND("sewmo/break_free.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CSewmo::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + SEWMO_FLINCH_DELAY;
	}

	return iIgnore;

}

void CSewmo :: Killed( entvars_t *pevAttacker, int iGib )
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

//=========================================================
// ActivateSewmo - the appear sequence is over
//=========================================================
void CSewmo :: ActivateSewmo( void )
{
	MonsterInit();
	m_iActivated = 1;
}

//=========================================================
// InactiveThink - waits in the water for a player
//=========================================================
void CSewmo :: InactiveThink( void )
{
	CBaseEntity *pNear = NULL;

	if ( !m_iActivated && !m_iActivating && pev->spawnflags & SF_SEWMO_HIDDEN_guess )
	{
		while ( (pNear = UTIL_FindEntityInSphere( pNear, pev->origin, 128 )) != NULL )
		{
			if ( pNear && FClassnameIs( pNear->pev, "player" ) )
			{
				SetSequenceByName( "appear" );
				SetNextThink( 0.8 );
				SetThink( &CSewmo::ActivateSewmo );
				m_iActivating = 1;
				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "sewmo/out_water.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
				UTIL_SplashCalculation( this, 0 );
				return;
			}
		}
	}

	SetNextThink( 0.1 );
}

//=========================================================
// SetActivity - the second attack is used once free
//=========================================================
void CSewmo :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;

	switch ( NewActivity )
	{
	case ACT_MELEE_ATTACK1:
		if ( !pev->body )
			iSequence = LookupActivity ( ACT_MELEE_ATTACK1 );
		else
			iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
		break;
	case ACT_MELEE_ATTACK2:
		if ( !pev->body )
			iSequence = LookupActivity ( ACT_MELEE_ATTACK1 );
		else
			iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
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
