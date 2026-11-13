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
// Spitter - hangs from the ceiling and spits acid
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"weapons.h"

extern DLL_GLOBAL short g_sModelIndexNull;

#define SPITTER_FLINCH_DELAY			2		// at most one flinch every n secs

// acid spit projectile (CoF_Flygare.cpp)
class CAcidSpit : public CBaseEntity
{
public:
	void Spawn( void );

	static void Shoot( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	void Touch( CBaseEntity *pOther );
	void EXPORT Animate( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int  m_maxFrame;
};

class CSpitter : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	int IgnoreConditions ( void );

	float m_flNextSpit;
	float m_flNextFlinch;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void DeathSound( void );
	void MonsterThink( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void EXPORT InactiveThink( void );
	void EXPORT ActivateSpitter( void );

	int m_iActivated;
	int m_iActivating;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_spitter, CSpitter );

TYPEDESCRIPTION	CSpitter::m_SaveData[] =
{
	DEFINE_FIELD( CSpitter, m_iActivated, FIELD_INTEGER ),
	DEFINE_FIELD( CSpitter, m_iActivating, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CSpitter, CBaseMonster );

const char *CSpitter::pAttackSounds[] =
{
	"flygare/flygare_attack1.wav",
	"flygare/flygare_attack2.wav",
};

const char *CSpitter::pAlertSounds[] =
{
	"flygare/flygare_alert1.wav",
	"flygare/flygare_alert2.wav",
};

const char *CSpitter::pPainSounds[] =
{
	"flygare/flygare_pain.wav",
};

const char *CSpitter::pDeathSounds[] =
{
	"flygare/flygare_death.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CSpitter :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

void CSpitter :: DeathSound( void )
{
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pDeathSounds[ RANDOM_LONG(0,ARRAYSIZE(pDeathSounds)-1) ], 1.0, ATTN_NORM, 0, RANDOM_LONG(95,105) );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSpitter :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys;
}

int CSpitter :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

void CSpitter :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSpitter :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSpitter :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}

//=========================================================
// Spawn
//=========================================================
void CSpitter :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/spitter.mdl");

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= gSkillData.spitterHealth;

	pev->view_ofs		= Vector ( 0, 0, 4 );// position of the eyes relative to monster's origin.

	m_flFieldOfView		= -0.7;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	SetSequenceByName( "appearidle" );
	m_iActivated = 0;
	m_iActivating = 0;
	SetNextThink( 0.1 );
	SetThink( &CSpitter::InactiveThink );

	UTIL_PrecacheOther( "acidspit" );

	SetBoneController( 1, 0 );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSpitter :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/spitter.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pDeathSounds ); i++ )
		PRECACHE_SOUND((char *)pDeathSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);


	PRECACHE_MODEL("sprites/xssmke1.spr");
	PRECACHE_MODEL("sprites/flier_spit.spr");

	PRECACHE_SOUND("flygare/flygare_acid_hit.wav");
	PRECACHE_SOUND("flygare/flygare_fallhit.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CSpitter::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + SPITTER_FLINCH_DELAY;
	}

	return iIgnore;

}

void CSpitter :: Killed( entvars_t *pevAttacker, int iGib )
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
// ActivateSpitter - the appear sequence is over
//=========================================================
void CSpitter :: ActivateSpitter( void )
{
	MonsterInit();
	m_iActivated = 1;
}

//=========================================================
// InactiveThink - waits for a player to come near
//=========================================================
void CSpitter :: InactiveThink( void )
{
	CBaseEntity *pNear = NULL;

	if ( !m_iActivated && !m_iActivating )
	{
		while ( (pNear = UTIL_FindEntityInSphere( pNear, pev->origin, 128 )) != NULL )
		{
			if ( pNear && FClassnameIs( pNear->pev, "player" ) )
			{
				SetSequenceByName( "appear" );
				SetNextThink( 0.66 );
				SetThink( &CSpitter::ActivateSpitter );
				m_iActivating = 1;
				m_flNextSpit = gpGlobals->time + 1;

				if ( pev->target )
					FireTargets( STRING( pev->target ), pNear, pNear, USE_TOGGLE, 0 );

				return;
			}
		}
	}

	SetNextThink( 0.1 );
}

void CSpitter :: MonsterThink( void )
{
	if ( m_hEnemy != NULL )
	{
		if ( m_hEnemy != NULL )
		{
			float yaw = VecToYaw( m_hEnemy->pev->origin - pev->origin ) - pev->angles.y;

			if ( yaw > 180 )
				yaw -= 360;

			if ( yaw < -180 )
				yaw += 360;

			SetBoneController( 1, yaw );
		}
		else
			SetBoneController( 1, 0 );
	}

	if ( m_flNextSpit < gpGlobals->time && IsAlive() )
	{
		if ( m_hEnemy != NULL && HasConditions( bits_COND_SEE_ENEMY ) )
		{
			UTIL_MakeVectors( pev->angles );

			Vector vecStart, angleGun;
			GetAttachment( 0, vecStart, angleGun );

			Vector vecSpitDir = ( ( m_hEnemy->pev->origin + m_hEnemy->pev->view_ofs ) - vecStart ).Normalize();

			vecSpitDir.x += RANDOM_FLOAT( -0.05, 0.05 );
			vecSpitDir.y += RANDOM_FLOAT( -0.05, 0.05 );
			vecSpitDir.z += RANDOM_FLOAT( -0.05, 0 );

			// spew the spittle temporary ents.
			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecStart );
				WRITE_BYTE( TE_BLOODSPRITE );
				WRITE_COORD( vecStart.x );	// pos
				WRITE_COORD( vecStart.y );
				WRITE_COORD( vecStart.z );
				WRITE_SHORT( g_sModelIndexNull );		// sprite1
				WRITE_SHORT( g_sModelIndexBloodDrop );	// sprite2
				WRITE_BYTE( BLOOD_COLOR_RED );			// color
				WRITE_BYTE( 4 );						// scale
			MESSAGE_END();

			SetSequenceByName( "spit" );

			CAcidSpit::Shoot( pev, vecStart, vecSpitDir * 900 );
		}

		m_flNextSpit = gpGlobals->time + 1 + RANDOM_FLOAT( 0, 2 );
	}

	CBaseMonster::MonsterThink();
}
