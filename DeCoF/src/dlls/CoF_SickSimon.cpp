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
// Book Simon (monster_booksimon): the sick Simon in the
// wheelchair chase (pev->frags != 0) and the Simon mode
// boss of the coop and survival games (pev->frags == 0)
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"weapons.h"
#include	"player.h"
#include	"effects.h"
#include	"animation.h"
#include	"gamerules.h"
#include	"shake.h"

extern bool m_bHadSimonMode;
extern int m_iSimonMode;
extern int g_iSurvivalMode;
extern bool m_HasCheckedSimonMode;
extern bool m_bSpawningSimon;
extern float m_flSimonSpawnTime;
extern int m_iMusicManagerLoops;
extern float m_fMusicManagerFadeTime;
extern int gmsgSimonMode;
extern int gmsgGreyFade;
extern int gmsgBossBar;
extern int gmsgTimerPlus;
extern int gmsgCustomDLight;
extern int gmsgMuzzleflash;

class CBookSimon : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void MonsterThink( void );
	void EXPORT RealKilled( void );

	int m_iDamageToHide;
	int m_iDamageNeeded;
	bool m_bCanSeePlayer;
	bool m_bTMPSound;

	BOOL CheckMeleeAttack1 ( float flDot, float flDist );
	BOOL CheckMeleeAttack2 ( float flDot, float flDist );

	float m_flSequenceTimeOverride;

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Killed( entvars_t *pevAttacker, int iGib );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void SetActivity ( Activity NewActivity );

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }

	float m_flNextShootTime;
	int m_iBulletsLoaded;
	int m_iAimState;
	int m_iWeapon;

	CBasePlayer *pPlayerWhoKilledMe;
	CBasePlayer *pPlayer;

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
};

LINK_ENTITY_TO_CLASS( monster_booksimon, CBookSimon );

TYPEDESCRIPTION	CBookSimon::m_SaveData[] =
{
	DEFINE_FIELD( CBookSimon, m_flNextShootTime, FIELD_TIME ),
	DEFINE_FIELD( CBookSimon, m_flSequenceTimeOverride, FIELD_TIME ),
	DEFINE_FIELD( CBookSimon, m_iBulletsLoaded, FIELD_INTEGER ),
	DEFINE_FIELD( CBookSimon, m_iAimState, FIELD_INTEGER ),
	DEFINE_FIELD( CBookSimon, m_iWeapon, FIELD_INTEGER ),
	DEFINE_FIELD( CBookSimon, pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBookSimon, m_iDamageToHide, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBookSimon, CBaseMonster );

BOOL CBookSimon :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	if ( pev->frags == 0 )
	{
		if ( m_iWeapon != 3 )
			return FALSE;
	}

	if ( flDist <= 64 && flDot >= 0.7 && m_hEnemy != NULL && FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) )
	{
		return TRUE;
	}
	return FALSE;
}

BOOL CBookSimon :: CheckMeleeAttack2 ( float flDot, float flDist )
{
	if ( pev->frags == 0 )
	{
		if ( m_iWeapon != 3 )
			return FALSE;
	}

	if ( flDist <= 64 && flDot >= 0.7 && m_hEnemy != NULL && FBitSet ( m_hEnemy->pev->flags, FL_ONGROUND ) )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CBookSimon :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CBookSimon :: SetYawSpeed ( void )
{
	int ys;

	if ( pev->frags != 0 )
		ys = 0;
	else
		ys = CVAR_GET_FLOAT( "cof_monsterhumanyawspeed" );

	pev->yaw_speed = ys;
}

//=========================================================
// Spawn
//=========================================================
void CBookSimon :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/booksimon.mdl");

	int iScale_guess = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= 100;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	if ( pev->frags == 0 )
	{
		m_bHadSimonMode = true;
		m_iSimonMode = 1;

		if ( g_iSurvivalMode && g_pGameRules->IsMultiplayer() )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgSimonMode );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				UTIL_ScreenFade( plr, Vector( 0, 0, 0 ), 2, 0, 255, FFADE_IN );

				MESSAGE_BEGIN( MSG_ONE, gmsgGreyFade, NULL, plr->pev );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				plr->PlayerSetMP3( MAKE_STRING( "fucked.mp3" ), 1, 1 );
			}
		}
	}

	m_iDamageNeeded = 60;
	m_iWeapon = pev->iuser4;

	if ( pev->frags != 0 )
	{
		// how close the wheelchair may get before Simon hides
		if ( pev->iuser2 == 0 )
			pev->iuser2 = 50;

		SetBodygroup( 2, 0 );

		switch ( m_iWeapon )
		{
		case 0:
			SetBodygroup( 0, 1 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = 5;
			break;
		case 2:
			SetBodygroup( 0, 2 );
			SetBodygroup( 1, 1 );
			m_iBulletsLoaded = 30;
			break;
		case 1:
			SetBodygroup( 0, 3 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = 15;
			break;
		case 3:
			SetBodygroup( 0, 4 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = -1;
			break;
		}
	}
	else
	{
		m_iWeapon = 0;

		SetBodygroup( 2, 0 );

		switch ( m_iWeapon )
		{
		case 0:
			SetBodygroup( 0, 1 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = 5;
			break;
		case 2:
			SetBodygroup( 0, 2 );
			SetBodygroup( 1, 1 );
			m_iBulletsLoaded = 30;
			break;
		case 1:
			SetBodygroup( 0, 3 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = 15;
			break;
		case 3:
			SetBodygroup( 0, 4 );
			SetBodygroup( 1, 0 );
			m_iBulletsLoaded = -1;
			break;
		}
	}

	pPlayerWhoKilledMe = NULL;

	MonsterInit();

	if ( pev->frags == 0 )
		m_iDamageNeeded = 300;

	pev->health = m_iDamageNeeded;
	pev->max_health = m_iDamageNeeded;

	if ( pev->frags != 0 && m_iAimState == 0 )
		pev->effects |= EF_NODRAW;

	bool alreadyaboss = false;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
		CBaseEntity *pEntity;

		for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
		{
			if ( pEdict->free )	// Not in use
				continue;

			pEntity = CBaseEntity::Instance( pEdict );
			if ( !pEntity )
				continue;

			if ( pEntity->m_bIsBoss )
			{
				alreadyaboss = true;
				break;
			}
			else
				continue;
		}
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && !alreadyaboss )
	{
		m_bIsBoss = true;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *player = UTIL_PlayerByIndex( i );

			if ( player && player->IsPlayer() )
			{
				CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

				if ( pPlayer )
				{
					pPlayer->m_bBossHealthBar = true;
					pPlayer->m_sBossBarEmpty = MAKE_STRING( "gfx/vgui/boss/simon_health_empty.tga" );
					pPlayer->m_sBossBarFull = MAKE_STRING( "gfx/vgui/boss/simon_health_full.tga" );
					pPlayer->m_iBossMaxHealth = pev->max_health;
					pPlayer->m_iBossCurrentHealth = pev->health;
					pPlayer->m_sBossName = pev->targetname;

					MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pPlayer->pev );
						WRITE_BYTE( 1 );
						WRITE_STRING( STRING(pPlayer->m_sBossBarEmpty) );
						WRITE_STRING( STRING(pPlayer->m_sBossBarFull) );
						WRITE_SHORT( pPlayer->m_iBossCurrentHealth );
						WRITE_SHORT( pPlayer->m_iBossMaxHealth );
					MESSAGE_END();
				}
			}
		}
	}
}

//=========================================================
// SetActivity
//=========================================================
void CBookSimon :: SetActivity ( Activity NewActivity )
{
	if ( pev->frags != 0 )
		return;

	int	iSequence;

	switch ( NewActivity )
	{
	case ACT_WALK:
		if ( pev->frags != 0 )
			iSequence = LookupActivity ( ACT_WALK_HURT );
		else
			iSequence = LookupActivity ( ACT_WALK );
		break;
	case ACT_RUN:
		if ( pev->frags != 0 )
			iSequence = LookupActivity ( ACT_WALK_HURT );
		else
			iSequence = LookupActivity ( ACT_RUN );
		break;
	case ACT_IDLE:
		if ( pev->frags != 0 )
		{
			iSequence = LookupActivity ( ACT_MELEE_ATTACK1 );

			if ( pev->frags != 0 && m_iAimState == 2 && m_iDamageToHide < m_iDamageNeeded )
			{
				switch ( m_iWeapon )
				{
				case 0:
					if ( ( pev->sequence == LookupSequence( "shotgun_appear" ) || pev->sequence == LookupSequence( "shotgun_idle" ) ) && m_flSequenceTimeOverride < gpGlobals->time )
					{
						SetSequenceByName( "shotgun_idle" );
						return;
					}
					break;
				case 1:
					if ( ( pev->sequence == LookupSequence( "glock_appear" ) || pev->sequence == LookupSequence( "glock_idle" ) ) && m_flSequenceTimeOverride < gpGlobals->time )
					{
						SetSequenceByName( "glock_idle" );
						return;
					}
					break;
				case 2:
					if ( ( pev->sequence == LookupSequence( "tmp_appear" ) || pev->sequence == LookupSequence( "tmp_idle" ) ) && m_flSequenceTimeOverride < gpGlobals->time )
					{
						SetSequenceByName( "tmp_idle" );
						return;
					}
					break;
				}
			}
		}
		else
			iSequence = LookupActivity ( ACT_IDLE );
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
		ALERT ( at_console, "%s has no sequence for act:%d\n", STRING(pev->classname), NewActivity );
		pev->sequence		= 0;	// Set to the reset anim (if it's there)
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CBookSimon :: Precache()
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/booksimon.mdl");

	PRECACHE_MODEL("sprites/muzzleflash.spr");
}

//=========================================================
// RealKilled - the think set by Killed
//=========================================================
void CBookSimon :: RealKilled( void )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			char *m_pMusicManager = (char *)CVAR_GET_STRING( "sv_coop_music" );

			if ( m_pMusicManager && strlen( m_pMusicManager ) > 1 )
				plr->PlayerSetMP3( MAKE_STRING( m_pMusicManager ), m_iMusicManagerLoops, m_fMusicManagerFadeTime );
		}
	}

	if ( g_iSurvivalMode && g_pGameRules->IsMultiplayer() )
	{
		m_HasCheckedSimonMode = false;
		m_bHadSimonMode = false;

		m_bSpawningSimon = true;
		m_flSimonSpawnTime = gpGlobals->time + RANDOM_LONG( 120, 240 );

		MESSAGE_BEGIN( MSG_ALL, gmsgTimerPlus );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 2 );
		MESSAGE_END();

		if ( pPlayerWhoKilledMe )
		{
			GetPointsForKill( (char *)STRING( pev->classname ), pPlayerWhoKilledMe );
			pPlayerWhoKilledMe = NULL;
		}
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

	if ( pev->flags & FL_MONSTER )
	{
		SetTouch( NULL );
		BecomeDead();
	}

	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}

	m_IdealMonsterState = MONSTERSTATE_DEAD;
	UTIL_Remove( this );
}

void CBookSimon :: Killed( entvars_t *pevAttacker, int iGib )
{
	if ( pev->iuser1 )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pevAttacker );

	if ( pEntity && pEntity->IsPlayer() )
		pPlayerWhoKilledMe = GetClassPtr( (CBasePlayer *)pEntity->pev );

	if ( pev->frags != 0 )
	{
		RealKilled();
		return;
	}

	pev->iuser1 = 1;

	m_iSimonMode = 0;

	if ( g_iSurvivalMode && g_pGameRules->IsMultiplayer() )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgSimonMode );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			UTIL_ScreenFade( plr, Vector( 0, 0, 0 ), 2, 0.5, 255, FFADE_IN );

			MESSAGE_BEGIN( MSG_ONE, gmsgGreyFade, NULL, plr->pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			plr->PlayerStopMP3( 1 );
		}
	}

	SetThink( &CBookSimon::RealKilled );
	SetNextThink( 2 );
	pev->effects |= EF_NODRAW;
}

void CBookSimon :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator )
	{
		pev->effects &= ~EF_NODRAW;
		m_iAimState = 2;

		if ( pev->frags == 0 )
		{
			m_bIsBoss = true;

			CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

			if ( player && player->IsPlayer() )
			{
				CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

				if ( pPlayer )
				{
					pPlayer->m_bBossHealthBar = true;
					pPlayer->m_sBossBarEmpty = MAKE_STRING( "gfx/vgui/boss/simon_health_empty.tga" );
					pPlayer->m_sBossBarFull = MAKE_STRING( "gfx/vgui/boss/simon_health_full.tga" );
					pPlayer->m_iBossMaxHealth = pev->max_health;
					pPlayer->m_iBossCurrentHealth = pev->health;
					pPlayer->m_sBossName = pev->targetname;

					MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pPlayer->pev );
						WRITE_BYTE( 1 );
						WRITE_STRING( STRING(pPlayer->m_sBossBarEmpty) );
						WRITE_STRING( STRING(pPlayer->m_sBossBarFull) );
						WRITE_SHORT( pPlayer->m_iBossCurrentHealth );
						WRITE_SHORT( pPlayer->m_iBossMaxHealth );
					MESSAGE_END();
				}
			}
		}

		switch ( m_iWeapon )
		{
		case 0:
			SetSequenceByName( "shotgun_appear" );
			break;
		case 1:
			SetSequenceByName( "glock_appear" );
			break;
		case 2:
			SetSequenceByName( "tmp_appear" );
			break;
		}

		m_flSequenceTimeOverride = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), pev->sequence ) + gpGlobals->time;
	}
}

int CBookSimon :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( pev->effects & EF_NODRAW )
		return 0;

	if ( pev->frags != 0 && m_iAimState == 3 )
		return 0;

	// Take 30% damage from bullets
	if ( bitsDamageType == DMG_BULLET )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();
		float flForce = DamageForce( flDamage );
		pev->velocity = pev->velocity + vecDir * flForce;
		flDamage *= 0.3;
	}

	if ( pev->frags != 0 )
	{
		if ( m_iDamageToHide >= m_iDamageNeeded )
			return 0;

		// pass the damage on to the entity named in the message key
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING( pev->message ) );

		if ( pTarget )
			pTarget->TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

		m_iDamageToHide += flDamage;
		flDamage = 0;
		pev->health = m_iDamageNeeded - m_iDamageToHide;

		if ( pev->health <= 0 )
			pev->health = 1;
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}

void CBookSimon :: MonsterThink( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		CBaseMonster::MonsterThink();
		return;
	}

	if ( pev->frags == 0 )
	{
		if ( GetBodygroup( 2 ) )
			SetBodygroup( 2, 0 );

		if ( m_flNextShootTime < gpGlobals->time )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->IsAlive() )
				{
					if ( FInViewCone( plr ) && FVisible( plr ) )
					{
						Vector vecEnemyPos = plr->pev->origin;

						float flDistToEnemy = ( vecEnemyPos - pev->origin ).Length();

						if ( flDistToEnemy <= 1024 )
						{
							Vector AttachOrg, AttachOrg2, AttachAng2, vecShootOrigin;
							UTIL_MakeVectors ( pev->angles );
							vecShootOrigin = pev->origin + Vector( 0, 0, 35 );
							entvars_t *pevPlr = plr->pev;
							Vector vecShootDir = ( plr->pev->origin + ( plr->BodyTarget( vecShootOrigin ) - pevPlr->origin ) - vecShootOrigin ).Normalize();

							EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/shotgun/shoot.wav", 1, ATTN_NORM, 0, 100 );
							FireBullets( 5, vecShootOrigin, vecShootDir, VECTOR_CONE_5DEGREES, 1024, BULLET_MONSTER_12MM, 10, 1, NULL );
							m_flNextShootTime = gpGlobals->time + 1 + RANDOM_FLOAT( 0, 0.5 );

							SetBodygroup( 2, 1 );

							GetAttachment( 2, AttachOrg2, AttachAng2 );

							MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, pev->origin );
								WRITE_COORD( AttachOrg2.x );
								WRITE_COORD( AttachOrg2.y );
								WRITE_COORD( AttachOrg2.z );
								WRITE_BYTE( 10 );
								WRITE_BYTE( 5 );
								WRITE_BYTE( 30 );
							MESSAGE_END();

							CSprite *pSprite = CSprite::SpriteCreate( "sprites/muzzleflash.spr", AttachOrg2, FALSE );
							pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
							pSprite->SetScale( 0.25 );
							pSprite->SetNextThink( 0.1 );
							pSprite->SetThink( &CSprite::SUB_Remove );
							pSprite->SetAttachment( edict(), 2 );
							break;
						}
					}
				}
			}
		}
	}

	if ( pev->frags != 0 )
	{
		if ( GetBodygroup( 2 ) )
			SetBodygroup( 2, 0 );

		if ( !m_bCanSeePlayer && m_bTMPSound )
		{
			m_bTMPSound = false;
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/tmp/tmp_shoot_end.wav", 1, ATTN_NORM, 0, 100 );
		}

		int distance = 0;

		if ( pPlayer && pPlayer->m_pWheelchair )
			distance = ( pPlayer->m_pWheelchair->pev->origin - pev->origin ).Length2D();

		if ( ( m_iBulletsLoaded <= 0 && m_iAimState == 1 ) || ( m_iDamageToHide >= m_iDamageNeeded && m_iAimState == 1 ) || ( m_iAimState == 1 && distance > 0 && distance <= pev->iuser2 ) )
		{
			m_iAimState = 3;

			switch ( m_iWeapon )
			{
			case 0:
				SetSequenceByName( "shotgun_disappear" );
				break;
			case 1:
				SetSequenceByName( "glock_disappear" );
				break;
			case 2:
				SetSequenceByName( "tmp_disappear" );
				break;
			}

			m_flSequenceTimeOverride = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), pev->sequence ) + gpGlobals->time;
		}
	}

	if ( pev->frags != 0 && m_iAimState == 3 && m_flSequenceTimeOverride < gpGlobals->time )
	{
		UTIL_Remove( this );
		return;
	}

	if ( pev->frags != 0 && m_iAimState == 2 && m_iDamageToHide < m_iDamageNeeded )
	{
		switch ( m_iWeapon )
		{
		case 0:
			if ( pev->sequence == LookupSequence( "shotgun_appear" ) && m_flSequenceTimeOverride < gpGlobals->time )
			{
				m_iAimState = 1;
				SetSequenceByName( "shotgun_idle" );
			}
			break;
		case 1:
			if ( pev->sequence == LookupSequence( "glock_appear" ) && m_flSequenceTimeOverride < gpGlobals->time )
			{
				m_iAimState = 1;
				SetSequenceByName( "glock_idle" );
			}
			break;
		case 2:
			if ( pev->sequence == LookupSequence( "tmp_appear" ) && m_flSequenceTimeOverride < gpGlobals->time )
			{
				m_iAimState = 1;
				SetSequenceByName( "tmp_idle" );
			}
			break;
		}
	}

	m_bCanSeePlayer = false;

	if ( pev->frags != 0 && m_iAimState == 1 )
	{
		if ( !pPlayer )
		{
			CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

			if ( player && player->IsPlayer() )
			{
				CBasePlayer *pCastPlayer = GetClassPtr( (CBasePlayer *)player->pev );

				if ( pCastPlayer )
					pPlayer = pCastPlayer;
			}
		}
		else if ( pPlayer->m_pWheelchair )
		{
			TraceResult tr;

			Vector AttachOrg, AttachAng;
			Vector AttachOrg2, AttachAng2;
			GetAttachment( 0, AttachOrg, AttachAng );
			Vector ChairOrg = pPlayer->m_pWheelchair->pev->origin;
			ChairOrg.z += 36;

			UTIL_TraceLine( AttachOrg, ChairOrg, dont_ignore_monsters, ENT(pev), &tr );

			if ( tr.pHit )
			{
				CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

				if ( pHit && pHit == pPlayer->m_pWheelchair )
				{
					if ( !m_bCanSeePlayer )
						m_bCanSeePlayer = true;

					if ( m_flNextShootTime < gpGlobals->time && m_iBulletsLoaded > 0 )
					{
						Vector vecShootOrigin;

						UTIL_MakeVectors ( pev->angles );
						vecShootOrigin = pev->origin + Vector( 0, 0, 35 );
						Vector vecShootDir = ( ( pPlayer->m_pWheelchair->BodyTarget( vecShootOrigin ) - pPlayer->m_pWheelchair->pev->origin ) + pPlayer->m_pWheelchair->pev->origin - vecShootOrigin ).Normalize();

						MESSAGE_BEGIN( MSG_ONE, gmsgMuzzleflash, NULL, pPlayer->pev );
							WRITE_SHORT( entindex() );
							WRITE_BYTE( 1 );
						MESSAGE_END();

						switch ( m_iWeapon )
						{
						case 0:
							{
								EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/shotgun/shoot.wav", 1, ATTN_NORM, 0, 100 );
								FireBullets( 5, vecShootOrigin, vecShootDir, VECTOR_CONE_5DEGREES, 1024, BULLET_MONSTER_12MM, 10, 2, NULL );
								m_flNextShootTime = gpGlobals->time + 1 + RANDOM_FLOAT( 0, 0.5 );

								m_iBulletsLoaded--;

								GetAttachment( 2, AttachOrg2, AttachAng2 );

								CSprite *pSprite = CSprite::SpriteCreate( "sprites/muzzleflash.spr", AttachOrg2, FALSE );
								pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
								pSprite->SetScale( 0.25 );
								pSprite->SetNextThink( 0.1 );
								pSprite->SetThink( &CSprite::SUB_Remove );
								pSprite->SetAttachment( edict(), 2 );
							}
							break;
						case 2:
							{
								if ( !m_bTMPSound )
								{
									m_bTMPSound = true;
									EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/tmp/tmp_shoot_loop.wav", 1, ATTN_NORM, 0, 100 );
								}

								m_iBulletsLoaded--;

								GetAttachment( 3, AttachOrg2, AttachAng2 );

								CSprite *pSprite = CSprite::SpriteCreate( "sprites/muzzleflash.spr", AttachOrg2, FALSE );
								pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
								pSprite->SetScale( 0.25 );
								pSprite->SetNextThink( 0.1 );
								pSprite->SetThink( &CSprite::SUB_Remove );
								pSprite->SetAttachment( edict(), 3 );

								FireBullets( 1, vecShootOrigin, vecShootDir, VECTOR_CONE_10DEGREES, 1024, BULLET_MONSTER_12MM, 1, 1, NULL );
								m_flNextShootTime = gpGlobals->time + 0.09;
							}
							break;
						case 1:
							{
								EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "weapons/glock/glock_fire.wav", 1, ATTN_NORM, 0, 100 );
								FireBullets( 5, vecShootOrigin, vecShootDir, VECTOR_CONE_4DEGREES, 1024, BULLET_MONSTER_12MM, 4, 1, NULL );
								m_flNextShootTime = gpGlobals->time + 0.6 + RANDOM_FLOAT( 0, 0.5 );

								m_iBulletsLoaded--;

								GetAttachment( 1, AttachOrg2, AttachAng2 );

								CSprite *pSprite = CSprite::SpriteCreate( "sprites/muzzleflash.spr", AttachOrg2, FALSE );
								pSprite->SetTransparency( kRenderTransAdd, 255, 255, 255, 128, kRenderFxNone );
								pSprite->SetScale( 0.1 );
								pSprite->SetNextThink( 0.1 );
								pSprite->SetThink( &CSprite::SUB_Remove );
								pSprite->SetAttachment( edict(), 1 );
							}
							break;
						}
					}
				}
			}
		}
	}

	if ( pev->frags != 0 )
	{
		if ( pPlayer )
		{
			if ( pPlayer->m_pWheelchair )
			{
				// turn the head to the wheelchair
				Vector2D	vec2LOS;
				UTIL_MakeVectors ( pev->angles );

				vec2LOS = ( pPlayer->m_pWheelchair->pev->origin - pev->origin ).Make2D();
				vec2LOS = vec2LOS.Normalize();

				float yaw = VecToYaw( pPlayer->m_pWheelchair->pev->origin - pev->origin ) - pev->angles.y;

				if (yaw > 180) yaw -= 360;
				if (yaw < -180) yaw += 360;

				SetBoneController( 2, yaw );
			}
		}
	}

	CBaseMonster::MonsterThink();
}
