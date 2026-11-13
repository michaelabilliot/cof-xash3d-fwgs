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
// Suicider - shoots with a pistol, shoots himself when the
// player comes close or the clip is empty
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"weapons.h"
#include	"effects.h"
#include	"animation.h"

extern int gmsgParticle;


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	SUICIDER_AE_SUICIDE_guess		0x01

#define SUICIDER_FLINCH_DELAY			2		// at most one flinch every n secs

class CSuicider : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void SpawnHeadGibs( Vector vecPos );
	void MonsterThink( void );
	void HeadExplode( void );

	float m_flShotFlashTime;
	int m_iPreviousActivity;
	float m_flBleedTime;
	int m_iClipSize;
	bool m_bSuicided;
	float m_flNextShotTime;
	float m_flReduceRecoilTime;
	float m_flUseTime;
	bool m_bSpawnedGun;

	float m_flNextFlinch;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void SetActivity ( Activity NewActivity );
	int IsHeadless( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	void Shoot( void );

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

LINK_ENTITY_TO_CLASS( monster_suicider, CSuicider );

TYPEDESCRIPTION	CSuicider::m_SaveData[] =
{
	DEFINE_FIELD( CSuicider, m_iClipSize, FIELD_INTEGER ),
	DEFINE_FIELD( CSuicider, m_bSuicided, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSuicider, m_flUseTime, FIELD_TIME ),
	DEFINE_FIELD( CSuicider, m_flNextShotTime, FIELD_TIME ),
	DEFINE_FIELD( CSuicider, m_flBleedTime, FIELD_TIME ),
	DEFINE_FIELD( CSuicider, m_flReduceRecoilTime, FIELD_TIME ),
	DEFINE_FIELD( CSuicider, m_flShotFlashTime, FIELD_TIME ),
	DEFINE_FIELD( CSuicider, m_bSpawnedGun, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CSuicider, CBaseMonster );

const char *CSuicider::pAttackHitSounds[] =
{
	"slower/hammer_strike1.wav",
	"slower/hammer_strike2.wav",
	"slower/hammer_strike3.wav",
};

const char *CSuicider::pAttackHitWallSounds[] =
{
	"slower/hammer_strike_wall1.wav",
	"slower/hammer_strike_wall2.wav",
	"slower/hammer_strike_wall3.wav",
};

const char *CSuicider::pAttackMissSounds[] =
{
	"slower/hammer_miss1.wav",
	"slower/hammer_miss2.wav",
};

const char *CSuicider::pAttackSounds[] =
{
	"slower/slower_attack1.wav",
	"slower/slower_attack2.wav",
};

const char *CSuicider::pAlertSounds[] =
{
	"slower/slower_alert10.wav",
	"slower/slower_alert20.wav",
	"slower/slower_alert30.wav",
};

const char *CSuicider::pPainSounds[] =
{
	"slower/slower_pain1.wav",
	"slower/slower_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CSuicider :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CSuicider :: SetYawSpeed ( void )
{
	int ys;

	ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );

	pev->yaw_speed = ys;
}

int CSuicider :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->sequence == LookupSequence( "run" ) && !m_bSuicided )
		m_iPreviousActivity = 3;
	else if ( !m_bSuicided )
		m_iPreviousActivity = -1;

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

void CSuicider :: PainSound( void )
{
	if ( IsHeadless() )
		return;

	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSuicider :: AlertSound( void )
{
	if ( IsHeadless() )
		return;

	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CSuicider :: AttackSound( void )
{
	if ( IsHeadless() )
		return;

	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}

//=========================================================
// HeadExplode - the head bursts and hurts what is around it
//=========================================================
void CSuicider :: HeadExplode( void )
{
	Vector vecSrc;
	Vector vecAng;
	CBaseEntity *pEntity = NULL;

	SetBodygroup( 0, 1 );

	int pitch = 100 + RANDOM_LONG(-5,5);

	if ( RANDOM_LONG( 0, 1 ) == 1 )
		EMIT_SOUND_DYN( ENT(pev), CHAN_ITEM, "baby/b_attack1.wav", 1, ATTN_NORM, 0, pitch );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_ITEM, "baby/b_attack2.wav", 1, ATTN_NORM, 0, pitch );

	// CoF: vecSrc is never set, the search uses an uninitialized position (original bug)
	while ( (pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, 64 )) != NULL )
		pEntity->TakeDamage( pev, pev, gSkillData.suiciderSuicide, DMG_GENERIC );
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CSuicider :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	Vector vecSrc;
	Vector vecAng;
	CSprite *pSprite = NULL;
	int pitch;

	switch( pEvent->event )
	{
		case SUICIDER_AE_SUICIDE_guess:
		{
			HeadExplode();

			GetAttachment( 0, vecSrc, vecAng );

			pitch = 100 + RANDOM_LONG(-5,5);

			if ( pev->iuser1 )
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/p345/p345_fire.wav", 1, ATTN_NORM, 0, pitch );
			else
				EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/glock/glock_fire.wav", 1, ATTN_NORM, 0, pitch );

			m_flShotFlashTime = gpGlobals->time + 0.1;
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
void CSuicider :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/suicider.mdl");

	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;

	if ( pev->health == 0 )
		pev->health		= gSkillData.suiciderHealth;

	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	m_bSuicided = false;
	m_flUseTime = gpGlobals->time;
	m_flReduceRecoilTime = gpGlobals->time;
	m_flShotFlashTime = gpGlobals->time;

	// iuser1: carries the P345 instead of the Glock
	if ( pev->iuser1 )
		m_iClipSize = 8;
	else
		m_iClipSize = 15;

	InitBoneControllers();

	MonsterInit();

	SetUse( NULL );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CSuicider :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/suicider.mdl");

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

	PRECACHE_MODEL("models/slower_headgibs.mdl");
	PRECACHE_MODEL("sprites/head_burst.spr");
	PRECACHE_MODEL("sprites/stmbal1.spr");

	if ( pev->iuser1 )
		PRECACHE_SOUND("weapons/p345/p345_fire.wav");
	else
		PRECACHE_SOUND("weapons/glock/glock_fire.wav");

	PRECACHE_SOUND("baby/b_attack1.wav");
	PRECACHE_SOUND("baby/b_attack2.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CSuicider::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + SUICIDER_FLINCH_DELAY;
	}

	return iIgnore;

}

void CSuicider :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	// reset the aim and the recoil
	pev->controller[0] = 128;
	pev->controller[1] = 128;
	pev->controller[2] = 128;
	pev->controller[3] = 128;

	SetBoneController( 2, 0 );

	// drop the pistol
	SetBodygroup( 1, 1 );

	if ( !m_bSpawnedGun )
	{
		m_bSpawnedGun = true;

		Vector vecSrc;
		Vector vecAng;
		GetAttachment( 1, vecSrc, vecAng );

		CBaseEntity *pGun;
		if ( pev->iuser1 )
			pGun = CBaseEntity::Create( "weapon_p345", vecSrc, pev->angles, edict() );
		else
			pGun = CBaseEntity::Create( "weapon_glock", vecSrc, pev->angles, edict() );

		// CoF: 1.55 also set the script, the clip and the velocity of the dropped gun here;
		// an empty loop is left (retail still aligns its head)
		for ( int i = 0; i < 1; i++ )
		{
		}
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

	if ( HasMemory( bits_MEMORY_KILLED ) )
	{
		if ( ShouldGibMonster( iGib ) )
			CallGibMonster();
		return;
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

void CSuicider :: SpawnHeadGibs( Vector vecPos )
{
	CSprite *pSprite = CSprite::SpriteCreate( "sprites/head_burst.spr", vecPos, TRUE );
	pSprite->pev->scale = 1.0;
	pSprite->AnimateAndDie( 20 );
	pSprite->SetTransparency( kRenderTransAlpha, 32, 0, 0, 255, kRenderFxNone );

	for ( int i = 0; i < 5; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );
		pGib->pev->origin = vecPos;

		Vector vecVol;

		switch ( i )
		{
		case 0:
			vecVol = Vector( 50, 0, 50 );
			break;
		case 1:
			vecVol = Vector( -50, 0, 50 );
			break;
		case 2:
			vecVol = Vector( 0, 50, 50 );
			break;
		case 3:
			vecVol = Vector( 0, -50, 50 );
			break;
		}

		pGib->pev->velocity = vecVol;
		pGib->Spawn( "models/slower_headgibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_RED;
		pGib->pev->body = i;

		float thinkTime = pGib->m_fNextThink - gpGlobals->time;

		pGib->m_lifeTime = (RANDOM_FLOAT( 0.95, 1.05 ) * 2);
		if ( pGib->m_lifeTime < thinkTime )
		{
			pGib->SetNextThink( pGib->m_lifeTime );
			pGib->m_lifeTime = 0;
		}

		pGib->pev->avelocity.x = RANDOM_FLOAT( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT( 100, 300 );
	}
}

int CSuicider :: IsHeadless( void )
{
	if ( !GetBodygroup( 0 ) )
		return FALSE;
	else
		return TRUE;
}

void CSuicider :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_HEAD:
			if ( IsHeadless() )
				break;

			// a bullet in the head blows it up
			if ( bitsDamageType & DMG_BULLET )
			{
				m_flBleedTime = gpGlobals->time + 3;
				TakeDamage( pev, pev, pev->health, DMG_CRUSH );
				HeadExplode();
			}
			break;
		}
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// Shoot - fire the pistol at the enemy
//=========================================================
void CSuicider :: Shoot( void )
{
	if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && m_flNextShotTime < gpGlobals->time )
	{
		float m_flDistanceToPlayer = ( m_hEnemy->pev->origin - pev->origin ).Length2D();

		if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && HasConditions( bits_COND_SEE_ENEMY ) )
		{
			if ( m_flDistanceToPlayer <= 1024 )
			{
				if ( m_iClipSize > 1 )
				{
					m_iClipSize--;

					// recoil
					pev->controller[0] = 0;

					int pitch = 100 + RANDOM_LONG(-5,5);

					if ( pev->iuser1 )
						EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/p345/p345_fire.wav", 1, ATTN_NORM, 0, pitch );
					else
						EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/glock/glock_fire.wav", 1, ATTN_NORM, 0, pitch );

					CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)m_hEnemy->pev );

					Vector vecSrc;
					Vector vecAng;
					GetAttachment( 1, vecSrc, vecAng );

					m_flNextShotTime = gpGlobals->time + RANDOM_FLOAT( 0.2, 0.8 );
					m_flShotFlashTime = gpGlobals->time + 0.1;

					Vector vecShootDir = ShootAtEnemy( vecSrc );

					// fuser1 scales the bullet damage
					if ( pev->fuser1 > 0 )
						FireBullets( 1, vecSrc, vecShootDir, VECTOR_CONE_8DEGREES, 768, BULLET_MONSTER_9MM, 1, gSkillData.suiciderBullet * pev->fuser1, pev );
					else
						FireBullets( 1, vecSrc, vecShootDir, VECTOR_CONE_8DEGREES, 768, BULLET_MONSTER_9MM, 1, gSkillData.suiciderBullet, pev );
				}
			}
		}
	}
}

void CSuicider :: MonsterThink( void )
{
	// the recoil wears off
	if ( m_flReduceRecoilTime < gpGlobals->time && IsAlive() )
	{
		int controllervalue = pev->controller[0];

		if ( pev->controller[0] < 250 )
		{
			pev->controller[0] += 125;

			if ( pev->controller[0] > 250 )
				pev->controller[0] = 250;
		}

		m_flReduceRecoilTime = gpGlobals->time + 0.1;
	}

	// aim at the enemy
	if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && IsAlive() )
	{
		float yaw = VecToYaw( m_hEnemy->pev->origin - pev->origin ) - pev->angles.y;

		if ( yaw > 180 ) yaw -= 360;
		if ( yaw < -180 ) yaw += 360;

		SetBoneController( 2, yaw );
	}
	else
		SetBoneController( 2, 0 );

	if ( m_hEnemy != NULL && m_hEnemy->IsPlayer() && IsAlive() )
	{
		if ( m_flNextShotTime < gpGlobals->time )
		{
			float m_flDistanceToPlayer = ( m_hEnemy->pev->origin - pev->origin ).Length2D();

			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)m_hEnemy->pev );

			if ( pPlayer )
			{
				if ( !HasConditions( bits_COND_ENEMY_OCCLUDED ) && HasConditions( bits_COND_SEE_ENEMY ) )
				{
					// too close, shoot himself
					if ( m_flDistanceToPlayer <= 60 )
					{
						if ( m_flNextShotTime < gpGlobals->time )
						{
							m_flBleedTime = gpGlobals->time + 3;
							m_bSuicided = true;
							TakeDamage( pev, pev, pev->health, DMG_CRUSH );
							m_flNextShotTime = gpGlobals->time + RANDOM_FLOAT( 0.2, 0.8 );
						}
					}
					// last bullet, shoot himself
					else if ( m_iClipSize == 1 )
					{
						if ( m_flNextShotTime < gpGlobals->time )
						{
							m_flBleedTime = gpGlobals->time + 3;
							m_bSuicided = true;
							TakeDamage( pev, pev, pev->health, DMG_CRUSH );
							m_flNextShotTime = gpGlobals->time + RANDOM_FLOAT( 0.2, 0.8 );
						}
					}
					else
						Shoot();
				}
			}
		}
	}

	if ( m_flBleedTime > gpGlobals->time && IsHeadless() )
	{
		Vector vecSrc, vecAng;
		GetAttachment( 0, vecSrc, vecAng );

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( entindex() );
			WRITE_STRING( "aurora/blood_effect.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( STRING( pev->model ) );
			WRITE_SHORT( 0 );
		MESSAGE_END();

		m_flBleedTime = gpGlobals->time;
	}

	CBaseMonster::MonsterThink();
}

//=========================================================
// SetActivity - the death sequence depends on how he died
//=========================================================
void CSuicider :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity )
	{
	case ACT_DIESIMPLE:
	case ACT_DIEBACKWARD:
	case ACT_DIEFORWARD:
	case ACT_DIEVIOLENT:
	case ACT_DIE_HEADSHOT:
	case ACT_DIE_CHESTSHOT:
	case ACT_DIE_GUTSHOT:
	case ACT_DIE_BACKSHOT:
		if ( m_iPreviousActivity == 3 || ( m_iPreviousActivity == 4 && !m_bSuicided ) )
		{
			iSequence = LookupActivity ( ACT_DIESIMPLE );
		}
		else
		{
			if ( m_bSuicided )
				iSequence = LookupActivity ( ACT_DIE_GUTSHOT );
			else
				iSequence = LookupActivity ( ACT_DIE_HEADSHOT );
		}
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
