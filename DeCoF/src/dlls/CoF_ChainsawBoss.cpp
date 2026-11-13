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
// Chainsaw boss (the sawer)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"weapons.h"

// TODO(header): the 1.55 PDB declares CChainsawBoss in cof_chainsawboss.h
// (GetActivity at line 22, CheckRangeAttack1/2 at lines 41/42)
class CChainsawBoss : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void MonsterThink( void );

	bool m_bIssuedSmoke;
	float m_flSmokeTime;
	EHANDLE m_hPlayer;

	void KillMonster( void );
	Activity GetActivity( void ) { return m_Activity; }

	float m_flNextFlinch;
	int m_iDamageBeforeFall;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );

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

// TODO(header): CoF_ThirdPersonInteraction.cpp defines this class (1.55 PDB); no header declares it
class CThirdPersonInteraction : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void TriggerAction( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	void SetSequenceByName( char *szSequence );
	void EXPORT NahThink( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	bool m_bSpawnChainsawBlood;
	EHANDLE m_pPlayer;
	EHANDLE m_pMonster;
};

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	CHAINSAWBOSS_AE_ATTACK_RIGHT		0x01
#define	CHAINSAWBOSS_AE_ATTACK_LEFT			0x02
#define	CHAINSAWBOSS_AE_ATTACK_BOTH			0x03

#define CHAINSAWBOSS_FLINCH_DELAY			2		// at most one flinch every n secs

LINK_ENTITY_TO_CLASS( monster_bosschainsaw, CChainsawBoss );

const char *CChainsawBoss::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CChainsawBoss::pAttackMissSounds[] =
{
	"boss/sawer/chainsaw_attack.wav",
};

const char *CChainsawBoss::pAttackSounds[] =
{
	"boss/sawer/sawer_attack1.wav",
	"boss/sawer/sawer_attack2.wav",
};

const char *CChainsawBoss::pAlertSounds[] =
{
	"boss/sawer/sawer_alert10.wav",
	"boss/sawer/sawer_alert20.wav",
	"boss/sawer/sawer_alert30.wav",
};

const char *CChainsawBoss::pPainSounds[] =
{
	"boss/sawer/sawer_pain1.wav",
	"boss/sawer/sawer_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CChainsawBoss :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CChainsawBoss :: SetYawSpeed ( void )
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

int CChainsawBoss :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	int pitch;

	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	if ( bitsDamageType != DMG_BULLET && pev->skin != 1 )
		flDamage = 0;

	// HACK HACK -- until we fix this.
	if ( IsAlive() && !m_bWaitingToDie )
		PainSound();

	if ( flDamage >= pev->health )
	{
		Stop();
		RouteClear();
		SetActivity( ACT_SMALL_FLINCH );
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CChainsawBoss :: PainSound( void )
{
}

void CChainsawBoss :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND( ENT(pev), CHAN_AUTO, "boss/sawer/chainsaw_loop.wav", 1.0, ATTN_NORM );
}

void CChainsawBoss :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CChainsawBoss :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CHAINSAWBOSS_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 0, DMG_SLASH, false, 0 );

			if ( pHurt )
			{
				CBaseEntity *pKiller = CBaseEntity::Create( "third_person_interaction", pev->origin, g_vecZero, edict() );

				if ( pKiller )
				{
					pKiller->pev->model = MAKE_STRING( "models/chainsawguy2.mdl" );
					pKiller->pev->message = MAKE_STRING( "action" );

					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pPlayer = pHurt;
					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pMonster = this;

					pKiller->Spawn();


					UTIL_Remove( this );
				}
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CHAINSAWBOSS_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 0, DMG_SLASH, false, 0 );

			if ( pHurt )
			{
				CBaseEntity *pKiller = CBaseEntity::Create( "third_person_interaction", pev->origin, g_vecZero, edict() );

				if ( pKiller )
				{
					pKiller->pev->model = MAKE_STRING( "models/chainsawguy2.mdl" );
					pKiller->pev->message = MAKE_STRING( "action" );

					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pPlayer = pHurt;
					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pMonster = this;

					pKiller->Spawn();


					UTIL_Remove( this );
				}
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CHAINSAWBOSS_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, 0, DMG_SLASH, false, 0 );

			if ( pHurt )
			{
				CBaseEntity *pKiller = CBaseEntity::Create( "third_person_interaction", pev->origin, g_vecZero, edict() );

				if ( pKiller )
				{
					pKiller->pev->model = MAKE_STRING( "models/chainsawguy2.mdl" );
					pKiller->pev->message = MAKE_STRING( "action" );

					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pPlayer = pHurt;
					GetClassPtr( (CThirdPersonInteraction *)pKiller->pev )->m_pMonster = this;

					pKiller->Spawn();


					UTIL_Remove( this );
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
void CChainsawBoss :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/chainsawguy.mdl");
	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 100 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= gSkillData.chainsawbossHealth;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_HEAR | bits_CAP_TURN_HEAD | bits_CAP_DOORS_GROUP;

	MonsterInit();

	m_iDamageBeforeFall = 0;
	m_flSmokeTime = gpGlobals->time + 2;
	m_bIssuedSmoke = false;
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CChainsawBoss :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/chainsawguy.mdl");

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


	PRECACHE_SOUND("boss/sawer/chainsawed_player.wav");
	PRECACHE_MODEL("models/chainsawguy2.mdl");

	PRECACHE_SOUND("boss/sawer/chainsaw_loop.wav");
	PRECACHE_SOUND("boss/sawer/eye_open.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

int CChainsawBoss::IgnoreConditions ( void )
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

	if (m_Activity == ACT_SMALL_FLINCH)
	{
		if (m_flNextFlinch < gpGlobals->time)
			m_flNextFlinch = gpGlobals->time + CHAINSAWBOSS_FLINCH_DELAY;
	}

	return iIgnore;
}

void CChainsawBoss :: KillMonster( void )
{
	UTIL_Remove( this );
}

void CChainsawBoss :: MonsterThink( void )
{
	if ( !m_bIssuedSmoke && m_flSmokeTime < gpGlobals->time )
	{
		pev->body = 1;

		m_bIssuedSmoke = true;
	}

	if ( GetActivity() != ACT_COWER && pev->skin == 1 )
		pev->skin = 0;

	if ( pev->skin == 1 && pev->yaw_speed != 0 )
		pev->yaw_speed = 0;

	if ( pev->skin == 0 && pev->yaw_speed != 120 )
		pev->yaw_speed = 120;

	CBaseMonster::MonsterThink();
}

void CChainsawBoss :: Killed( entvars_t *pevAttacker, int iGib )
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

	STOP_SOUND( ENT(pev), CHAN_AUTO, "boss/sawer/chainsaw_loop.wav" );

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
// TraceAttack - only a melee hit in the back while the
// sawer cowers (hitgroup 9) does real damage
//=========================================================
void CChainsawBoss :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	CBaseEntity *pEnt = CBaseEntity::Instance( pevAttacker );

	// melee does double damage
	if ( bitsDamageType & (1 << 26) )
		flDamage *= 2;

	if ( pEnt && pEnt->IsPlayer() )
	{
		Vector2D	vec2LOS;
		float		flDot;

		UTIL_MakeVectors ( pev->angles );

		vec2LOS = ( pEnt->pev->origin - pev->origin ).Make2D();
		vec2LOS = vec2LOS.Normalize();

		flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

		if ( flDot < 0 )
		{
			// hit from behind
			if ( GetActivity() == ACT_COWER )
			{
				if ( bitsDamageType & (1 << 26) )
				{
					ptr->iHitgroup = 9;
				}
			}
		}
	}

	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case HITGROUP_HEAD:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case HITGROUP_CHEST:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case HITGROUP_STOMACH:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
		case 10:
		case 11:
		case 12:
		case 13:
		case 18:
		case 19:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
		case 14:
		case 15:
		case 16:
		case 17:
			if ( GetActivity() != ACT_COWER )
				m_iDamageBeforeFall += flDamage;
			SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
			flDamage = 0;
			break;
		case 8:
			flDamage = 0;
			break;
		case 9:
			if ( GetActivity() == ACT_COWER && pev->skin == 1 )
			{
				int pitch = 95 + RANDOM_LONG(0,9);

				if (RANDOM_LONG(0,3) < 2)
					EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
				flDamage = gSkillData.monHead;
				SpawnBlood( ptr->vecEndPos, BLOOD_COLOR_YELLOW, flDamage, pev->velocity );

				break;
			}
			else
			{
				if ( GetActivity() != ACT_COWER )
					m_iDamageBeforeFall += flDamage;
				SpawnBlood( ptr->vecEndPos, BloodColor(), 2, pev->velocity );
				flDamage = 0;
			}
			break;
		default:
			break;
		}

		switch ( g_iSkillLevel )
		{
		case SKILL_EASY:
			if ( m_iDamageBeforeFall >= 50 && GetActivity() != ACT_COWER )
			{
				pev->skin = 1;
				Stop();
				RouteClear();
				SetActivity( ACT_COWER );
				m_iDamageBeforeFall = 0;
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "boss/sawer/eye_open.wav", 1, ATTN_NORM );
			}
			break;
		case SKILL_MEDIUM:
			if ( m_iDamageBeforeFall >= 70 && GetActivity() != ACT_COWER )
			{
				pev->skin = 1;
				Stop();
				RouteClear();
				SetActivity( ACT_COWER );
				m_iDamageBeforeFall = 0;
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "boss/sawer/eye_open.wav", 1, ATTN_NORM );
			}
			break;
		case SKILL_HARD:
			if ( m_iDamageBeforeFall >= 120 && GetActivity() != ACT_COWER )
			{
				pev->skin = 1;
				Stop();
				RouteClear();
				SetActivity( ACT_COWER );
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "boss/sawer/eye_open.wav", 1, ATTN_NORM );
				m_iDamageBeforeFall = 0;
			}
			break;
		case 4:
			if ( m_iDamageBeforeFall >= 120 && GetActivity() != ACT_COWER )
			{
				pev->skin = 1;
				Stop();
				RouteClear();
				SetActivity( ACT_COWER );
				EMIT_SOUND( ENT(pev), CHAN_VOICE, "boss/sawer/eye_open.wav", 1, ATTN_NORM );
				m_iDamageBeforeFall = 0;
			}
			break;
		}

		if ( bitsDamageType != DMG_BULLET && pev->skin != 1 )
			flDamage = 0;

		SpawnBlood( ptr->vecEndPos, BloodColor(), flDamage * 3, pev->velocity );
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );

		if ( bitsDamageType == DMG_BULLET || ptr->iHitgroup == 9 )
			AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}
