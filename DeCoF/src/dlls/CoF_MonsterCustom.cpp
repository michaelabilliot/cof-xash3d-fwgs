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
// Custom monster - model, sounds and damage set by the level designer
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
#define	CUSTOM_AE_ATTACK_RIGHT		0x01
#define	CUSTOM_AE_ATTACK_LEFT		0x02
#define	CUSTOM_AE_ATTACK_BOTH		0x03

#define CUSTOM_FLINCH_DELAY			2		// at most one flinch every n secs

class CCustomMonster : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );

	float m_flNextFlinch;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void DeathSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	int attacksound1;
	int attacksound2;
	int alertsound1;
	int alertsound2;
	int painsound1;
	int painsound2;
	int deathsound1;
	int deathsound2;
	int attackhitsound1;
	int attackhitsound2;
	int attackhitwallsound1;
	int attackhitwallsound2;
	int attackmisssound1;
	int attackmisssound2;

	void KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_custom, CCustomMonster );

// CoF: attackhitwallsound1 is saved twice and attackhitwallsound2 not at all (original bug)
TYPEDESCRIPTION	CCustomMonster::m_SaveData[] =
{
	DEFINE_FIELD( CCustomMonster, attacksound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attacksound2, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, alertsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, alertsound2, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, painsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, painsound2, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, deathsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, deathsound2, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackhitsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackhitsound2, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackhitwallsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackhitwallsound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackmisssound1, FIELD_STRING ),
	DEFINE_FIELD( CCustomMonster, attackmisssound2, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCustomMonster, CBaseMonster );

void CCustomMonster::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "attacksound1"))
	{
		attacksound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attacksound2"))
	{
		attacksound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "alertsound1"))
	{
		alertsound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "alertsound2"))
	{
		alertsound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "painsound1"))
	{
		painsound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "painsound2"))
	{
		painsound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deathsound1"))
	{
		deathsound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "deathsound2"))
	{
		deathsound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackhitsound1"))
	{
		attackhitsound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackhitsound2"))
	{
		attackhitsound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackhitwallsound1"))
	{
		attackhitwallsound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackhitwallsound2"))
	{
		attackhitwallsound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackmisssound1"))
	{
		attackmisssound1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "attackmisssound2"))
	{
		attackmisssound2 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CCustomMonster :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CCustomMonster :: SetYawSpeed ( void )
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

int CCustomMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

	// fuser1: 1 = immune to melee, 2 = immune to bullets
	int value = pev->fuser1;

	switch ( value )
	{
	case 1:
		if ( (bitsDamageType & (1 << 26)) || bitsDamageType == (1 << 26) )
			return 0;
		break;
	case 2:
		if ( (bitsDamageType & DMG_BULLET) || bitsDamageType == DMG_BULLET )
			return 0;
		break;
	}

	// HACK HACK -- until we fix this.
	if ( IsAlive() && !m_bWaitingToDie )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CCustomMonster :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
	{
		if ( RANDOM_LONG( 0, 1 ) == 1 )
			EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( painsound1 ), 1.0, ATTN_NORM, 0, pitch );
		else
			EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( painsound2 ), 1.0, ATTN_NORM, 0, pitch );
	}
}

void CCustomMonster :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if ( RANDOM_LONG( 0, 1 ) == 1 )
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( alertsound1 ), 1.0, ATTN_NORM, 0, pitch );
	else
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( alertsound2 ), 1.0, ATTN_NORM, 0, pitch );
}

void CCustomMonster :: AttackSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if ( RANDOM_LONG( 0, 1 ) == 1 )
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( attacksound1 ), 1.0, ATTN_NORM, 0, pitch );
	else
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( attacksound2 ), 1.0, ATTN_NORM, 0, pitch );
}

void CCustomMonster :: DeathSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if ( RANDOM_LONG( 0, 1 ) == 1 )
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( deathsound1 ), 1.0, ATTN_NORM, 0, pitch );
	else
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, STRING( deathsound2 ), 1.0, ATTN_NORM, 0, pitch );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
// The damage of the three attacks is in iuser1 to iuser3.
//=========================================================
void CCustomMonster :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CUSTOM_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, pev->iuser1, DMG_SLASH, false, 0 );
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
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
				else
				{
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
			}
			else
			{
				if ( RANDOM_LONG( 0, 1 ) == 1 )
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				else
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
			}

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CUSTOM_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, pev->iuser2, DMG_SLASH, false, 0 );
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
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
				else
				{
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
			}
			else
			{
				if ( RANDOM_LONG( 0, 1 ) == 1 )
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				else
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
			}

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CUSTOM_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, pev->iuser3, DMG_SLASH, false, 0 );
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
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitwallsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
				else
				{
					if ( RANDOM_LONG( 0, 1 ) == 1 )
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
					else
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackhitsound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				}
			}
			else
			{
				if ( RANDOM_LONG( 0, 1 ) == 1 )
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound1 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
				else
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, STRING( attackmisssound2 ), 1.0, ATTN_NORM, 0, PITCH_NORM );
			}

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
void CCustomMonster :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/slower.mdl");

	int iScale_guess = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CCustomMonster :: Precache()
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/slower.mdl");

	PRECACHE_SOUND( attacksound1 );
	PRECACHE_SOUND( attacksound2 );

	PRECACHE_SOUND( alertsound1 );
	PRECACHE_SOUND( alertsound2 );

	PRECACHE_SOUND( painsound1 );
	PRECACHE_SOUND( painsound2 );

	PRECACHE_SOUND( deathsound1 );
	PRECACHE_SOUND( deathsound2 );

	PRECACHE_SOUND( attackhitsound1 );
	PRECACHE_SOUND( attackhitsound2 );

	PRECACHE_SOUND( attackhitwallsound1 );
	PRECACHE_SOUND( attackhitwallsound2 );

	PRECACHE_SOUND( attackmisssound1 );
	PRECACHE_SOUND( attackmisssound2 );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CCustomMonster::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + CUSTOM_FLINCH_DELAY;
	}

	return iIgnore;

}

void CCustomMonster :: Killed( entvars_t *pevAttacker, int iGib )
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
