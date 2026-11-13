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
// Taller
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"gamerules.h"
#include	"animation.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	TALLER_AE_PUNCH_guess		0x01
#define	TALLER_AE_STAMP_guess		0x02
#define	TALLER_AE_STEP_guess		0x03

class CTaller : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void SetActivity ( Activity NewActivity );

	float m_flNextFlinch;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void DeathSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackHitWallSounds[];
	static const char *pAttackMissSounds[];
	static const char *pDeathSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_taller, CTaller );

const char *CTaller::pAttackHitSounds[] =
{
	"taller/taller_player_punch.wav",
};

const char *CTaller::pAttackHitWallSounds[] =
{
	"taller/taller_wall_punch.wav",
};

const char *CTaller::pAttackMissSounds[] =
{
	"taller/taller_swing.wav",
};

const char *CTaller::pAttackSounds[] =
{
	"taller/taller_pain.wav",
};

const char *CTaller::pAlertSounds[] =
{
	"taller/taller_alert.wav",
};

const char *CTaller::pPainSounds[] =
{
	"taller/taller_pain.wav",
};

const char *CTaller::pDeathSounds[] =
{
	"taller/taller_die.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CTaller :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CTaller :: SetYawSpeed ( void )
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

int CTaller :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// HACK HACK -- until we fix this.
	if ( IsAlive() && !m_bWaitingToDie )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CTaller :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CTaller :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CTaller :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}

void CTaller :: DeathSound( void )
{
	// Play a random death sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pDeathSounds[ RANDOM_LONG(0,ARRAYSIZE(pDeathSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CTaller :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case TALLER_AE_PUNCH_guess:
		{
			CBaseEntity *pHurt = CheckTraceHullAttack( 90, gSkillData.tallerDmgPunch, DMG_CLUB, true, 0 );
			if ( pHurt )
			{
				if ( pHurt->IsPlayer() )
				{
					CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pHurt->pev );

					if ( pPlayer )
					{
						UTIL_MakeVectors( pev->angles );
						pPlayer->pev->punchangle.x = 5;
						pPlayer->pev->origin.z += 2;
						pPlayer->pev->velocity = pPlayer->pev->velocity + gpGlobals->v_forward * 250 + gpGlobals->v_up * 200;
						pPlayer->m_bWaitingToFall = true;
						pPlayer->ViewPunch( -10, 20, 20 );
						pPlayer->m_flSequenceOverrideTime = gpGlobals->time;
						pPlayer->m_bHasGotUp = false;
					}
				}

				if ( pHurt->Classify() == CLASS_NONE || pHurt->Classify() == CLASS_MACHINE )
					EMIT_SOUND_DYN ( ENT(pev), CHAN_AUTO, pAttackHitWallSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitWallSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
				else
					EMIT_SOUND_DYN ( ENT(pev), CHAN_AUTO, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}

			// CoF: the swing sound plays on hits too
			EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case TALLER_AE_STAMP_guess:
		{
			TraceResult tr;
			UTIL_MakeAimVectors( pev->angles );

			Vector vecStart = pev->origin;
			vecStart.z += pev->size.z * 0.25;
			Vector vecEnd = vecStart + (gpGlobals->v_forward * 90);

			UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );

			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBaseEntity *pCast = UTIL_PlayerByIndex( i );

					if ( pCast && pCast->IsAlive() )
					{
						TraceResult trace;
						UTIL_TraceModel( vecStart, vecEnd, head_hull, ENT( pCast->pev ), &trace );

						if ( trace.pHit )
							tr.pHit = trace.pHit;
					}
				}
			}

			bool playerhit = false;

			if ( tr.pHit )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if ( pEntity->IsPlayer() )
				{
					CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

					if ( pPlayer )
					{
						pPlayer->m_bKilledLyingDown = true;
						pPlayer->m_iDeathBits |= 2;
						pPlayer->m_iDeathBits |= 4;
						pPlayer->pev->health = 0;
						pPlayer->Killed( pPlayer->pev, GIB_NEVER );
						playerhit = true;
						EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, "taller/taller_stamp.wav", 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
					}
				}

				if ( !playerhit )
					EMIT_SOUND_DYN ( ENT(pev), CHAN_AUTO, pAttackHitWallSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitWallSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
		}
		break;

		case TALLER_AE_STEP_guess:
		{
			EMIT_SOUND( edict(), CHAN_BODY, "taller/taller_step.wav", 1, ATTN_NORM );
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
void CTaller :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/taller.mdl");

	int iScale_guess = (int)CVAR_GET_FLOAT( "cof_monsterhumanscale" ) * 2;
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 168 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	if ( pev->health == 0 )
		pev->health		= gSkillData.tallerHealth;

	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CTaller :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/taller.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackHitWallSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitWallSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pDeathSounds ); i++ )
		PRECACHE_SOUND((char *)pDeathSounds[i]);

	PRECACHE_SOUND("taller/taller_stamp.wav");
	PRECACHE_SOUND("taller/taller_player_impact.wav");
	PRECACHE_SOUND("taller/taller_step.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

int CTaller::IgnoreConditions ( void )
{
	int iIgnore = CBaseMonster::IgnoreConditions();
	iIgnore |= (bits_COND_LIGHT_DAMAGE|bits_COND_HEAVY_DAMAGE);

	return iIgnore;
}

void CTaller :: Killed( entvars_t *pevAttacker, int iGib )
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
// SetActivity - the stamp is used on lying or ducking
// players
//=========================================================
void CTaller :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity )
	{
	case ACT_MELEE_ATTACK1:
		if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && ( m_hEnemy->pev->fuser3 != 0 || m_hEnemy->pev->flags & FL_DUCKING ) )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && m_hEnemy->pev->fuser3 != 3 )
				iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
			else
				iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
		}
		else
			iSequence = LookupActivity ( ACT_MELEE_ATTACK1 );
		break;
	case ACT_MELEE_ATTACK2:
		if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && ( m_hEnemy->pev->fuser3 != 0 || m_hEnemy->pev->flags & FL_DUCKING ) )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && m_hEnemy->pev->fuser3 != 3 )
				iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
			else
				iSequence = LookupActivity ( ACT_MELEE_ATTACK2 );
		}
		else
			iSequence = LookupActivity ( ACT_MELEE_ATTACK1 );
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
