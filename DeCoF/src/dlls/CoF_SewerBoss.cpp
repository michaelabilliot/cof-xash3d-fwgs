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
// Sewer boss
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
#define	SEWERBOSS_AE_ATTACK			0x01

#define SEWERBOSS_FLINCH_DELAY		2		// at most one flinch every n secs

class CSewerboss : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );

	float m_flNextFlinch;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float m_flElectrocuteTime;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void MonsterThink( void );
	void Killed( entvars_t *pevAttacker, int iGib );

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackHitWallSounds[];
	static const char *pAttackMissSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_sewerboss, CSewerboss );

TYPEDESCRIPTION	CSewerboss::m_SaveData[] =
{
	DEFINE_FIELD( CSewerboss, m_flElectrocuteTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CSewerboss, CBaseMonster );

const char *CSewerboss::pAttackHitSounds[] =
{
	"boss/sewer/mace_hitflesh.wav",
};

const char *CSewerboss::pAttackHitWallSounds[] =
{
	"boss/sewer/mace_hitwall.wav",
};

const char *CSewerboss::pAttackMissSounds[] =
{
	"boss/sewer/mace_swing.wav",
};

const char *CSewerboss::pAttackSounds[] =
{
	"slower/slower_attack1.wav",
	"slower/slower_attack2.wav",
};

const char *CSewerboss::pAlertSounds[] =
{
	"slower/slower_alert10.wav",
	"slower/slower_alert20.wav",
	"slower/slower_alert30.wav",
};

const char *CSewerboss::pPainSounds[] =
{
	"slower/slower_pain1.wav",
	"slower/slower_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CSewerboss :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSewerboss :: SetYawSpeed ( void )
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

int CSewerboss :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// only electricity hurts the sewer boss
	if ( bitsDamageType == DMG_SHOCK )
	{
		// HACK HACK -- until we fix this.
		if ( IsAlive() && !m_bWaitingToDie )
			PainSound();

		m_flElectrocuteTime = gpGlobals->time + 2.5;
		pev->skin = 1;
		Stop();
		RouteClear();
		SetActivity( ACT_TWITCH );

		return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	return 0;
}

void CSewerboss :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSewerboss :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSewerboss :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CSewerboss :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case SEWERBOSS_AE_ATTACK:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 70, gSkillData.sewerbossMelee, DMG_CLUB | DMG_NEVERGIB, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					if ( pHurt->IsPlayer() )
					{
						CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pHurt->pev );

						if ( pPlayer )
							pPlayer->ViewPunch( -15, 60, 60 );
					}

					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}

				if ( pHurt->Classify() == CLASS_NONE || pHurt->Classify() == CLASS_MACHINE )
				{
					EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitWallSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitWallSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
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
void CSewerboss :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/sewer_boss.mdl");
	int iScale_guess = (int)CVAR_GET_FLOAT( "cof_monsterhumanscale" ) * 2;
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 100 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= gSkillData.sewerbossHealth;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSewerboss :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/sewer_boss.mdl");

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
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================

int CSewerboss::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + SEWERBOSS_FLINCH_DELAY;
	}

	return iIgnore;
}

void CSewerboss :: Killed( entvars_t *pevAttacker, int iGib )
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

void CSewerboss :: MonsterThink( void )
{
	if ( m_flElectrocuteTime < gpGlobals->time && pev->skin )
		pev->skin = 0;

	if ( pev->skin == 1 && pev->yaw_speed != 0 )
		pev->yaw_speed = 0;

	if ( pev->skin == 0 && pev->yaw_speed != 120 )
		pev->yaw_speed = 120;

	CBaseMonster::MonsterThink();
}
