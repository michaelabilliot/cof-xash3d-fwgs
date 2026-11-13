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
// Crazy bitch - makes the player shoot himself
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"player.h"
#include	"weapons.h"
#include	"animation.h"


//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	CRAZYBITCH_AE_ATTACK_RIGHT		0x01
#define	CRAZYBITCH_AE_ATTACK_LEFT		0x02
#define	CRAZYBITCH_AE_ATTACK_BOTH		0x03

#define CRAZYBITCH_FLINCH_DELAY			2		// at most one flinch every n secs

class CCrazyBitch : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int IgnoreConditions ( void );
	void StopSuicide( void );

	float m_flNextFlinch;
	float m_flSuicideAttempt;
	bool b_StartSuicide;
	bool b_CanSuicideYou;
	bool b_ReadyToSuicide;
	bool b_TriedToSuicide;

	void MonsterThink( void );

	CBasePlayer *pSuicidePlayer;

	void PainSound( void );
	void AlertSound( void );
	void AttackSound( void );
	void Killed( entvars_t *pevAttacker, int iGib );
	void SetActivity ( Activity NewActivity );

	static const char *pAttackSounds[];
	static const char *pAlertSounds[];
	static const char *pSuicideSounds[];
	static const char *pPainSounds[];
	static const char *pAttackHitSounds[];
	static const char *pAttackMissSounds[];

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	// No range attacks
	BOOL CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
	BOOL CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_crazybitch, CCrazyBitch );

TYPEDESCRIPTION	CCrazyBitch::m_SaveData[] =
{
	DEFINE_FIELD( CCrazyBitch, m_flSuicideAttempt, FIELD_TIME ),
	DEFINE_FIELD( CCrazyBitch, b_StartSuicide, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCrazyBitch, b_CanSuicideYou, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCrazyBitch, b_ReadyToSuicide, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCrazyBitch, b_TriedToSuicide, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCrazyBitch, pSuicidePlayer, FIELD_CLASSPTR ),
};

IMPLEMENT_SAVERESTORE( CCrazyBitch, CBaseMonster );

const char *CCrazyBitch::pAttackHitSounds[] =
{
	"crazylady/knife_hitbody1.wav",
	"crazylady/knife_hitbody2.wav",
};

const char *CCrazyBitch::pAttackMissSounds[] =
{
	"crazylady/knife_swing.wav",
};

const char *CCrazyBitch::pAttackSounds[] =
{
	"crazylady/lady_attack1.wav",
	"crazylady/lady_attack2.wav",
};

const char *CCrazyBitch::pAlertSounds[] =
{
	"crazylady/lady_alert10.wav",
	"crazylady/lady_alert20.wav",
	"crazylady/lady_alert30.wav",
};

const char *CCrazyBitch::pSuicideSounds[] =
{
	"player/Vocal/Suicide/suicide_panic1.wav",
	"player/Vocal/Suicide/suicide_panic2.wav",
	"player/Vocal/Suicide/suicide_panic3.wav",
};

const char *CCrazyBitch::pPainSounds[] =
{
	"crazylady/lady_pain1.wav",
	"crazylady/lady_pain2.wav",
};

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CCrazyBitch :: Classify ( void )
{
	return m_iClass?m_iClass:CLASS_ALIEN_MONSTER;
}

//=========================================================
// StopSuicide - the player stops shooting himself
//=========================================================
void CCrazyBitch :: StopSuicide( void )
{
	if ( !pSuicidePlayer )
		return;

	bool is_in_suicide = false;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( pSuicidePlayer->m_rgpPlayerItems[i] )
		{
			CBasePlayerItem *pPlayerItem = pSuicidePlayer->m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if ( gun && gun->m_iSuicideState )
				{
					is_in_suicide = true;
					gun->m_iSuicideButtonsPressed = 0;
					gun->m_iSuicideState = 0;
					gun->m_flNextPrimaryAttack = gun->m_flNextSecondaryAttack = gun->m_flTimeWeaponIdle = gpGlobals->time + 1;
					gun->Deploy();
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	if ( !is_in_suicide )
		return;

	UTIL_ScreenFade( pSuicidePlayer, Vector( 128, 128, 128 ), 0.5, 0, 255, 0 );

	if ( pSuicidePlayer->pev->fuser3 == 10 )
		pSuicidePlayer->pev->fuser3 = 0;

	pSuicidePlayer->m_flNextAttack = gpGlobals->time + 1;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CCrazyBitch :: SetYawSpeed ( void )
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

int CCrazyBitch :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
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

void CCrazyBitch :: PainSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG(0,ARRAYSIZE(pPainSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CCrazyBitch :: AlertSound( void )
{
	int pitch = 95 + RANDOM_LONG(0,9);

	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAlertSounds[ RANDOM_LONG(0,ARRAYSIZE(pAlertSounds)-1) ], 1.0, ATTN_NORM, 0, pitch );
}

void CCrazyBitch :: AttackSound( void )
{
	// Play a random attack sound
	EMIT_SOUND_DYN ( ENT(pev), CHAN_VOICE, pAttackSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
}


//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CCrazyBitch :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CRAZYBITCH_AE_ATTACK_RIGHT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash right!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.krypandeDmgOneSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = -18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
				}
				// Play a random attack hit sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else // Play a random attack miss sound
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CRAZYBITCH_AE_ATTACK_LEFT:
		{
			// do stuff for this event.
	//		ALERT( at_console, "Slash left!\n" );
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.krypandeDmgOneSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.z = 18;
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_right * 100;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
			}
			else
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackMissSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackMissSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );

			if (RANDOM_LONG(0,1))
				AttackSound();
		}
		break;

		case CRAZYBITCH_AE_ATTACK_BOTH:
		{
			// do stuff for this event.
			CBaseEntity *pHurt = CheckTraceHullAttack( 55, gSkillData.krypandeDmgBothSlash, DMG_SLASH, false, 0 );
			if ( pHurt )
			{
				if ( pHurt->pev->flags & (FL_MONSTER|FL_CLIENT) )
				{
					pHurt->pev->punchangle.x = 5;
					pHurt->pev->velocity = pHurt->pev->velocity + gpGlobals->v_forward * -100;
				}
				EMIT_SOUND_DYN ( ENT(pev), CHAN_WEAPON, pAttackHitSounds[ RANDOM_LONG(0,ARRAYSIZE(pAttackHitSounds)-1) ], 1.0, ATTN_NORM, 0, 100 + RANDOM_LONG(-5,5) );
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
void CCrazyBitch :: Spawn()
{
	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/crazywoman.mdl");
	int iScale_guess = CVAR_GET_FLOAT( "cof_monsterhumanscale" );
	UTIL_SetSize( pev, Vector( -iScale_guess, -iScale_guess, 0 ), Vector( iScale_guess, iScale_guess, 72 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	if ( pev->health == 0 )
		pev->health		= gSkillData.krypandeHealth;
	pev->view_ofs		= Vector ( 0, 0, 28 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;
	m_afCapability		= bits_CAP_DOORS_GROUP;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CCrazyBitch :: Precache()
{
	int i;

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/crazywoman.mdl");

	for ( i = 0; i < ARRAYSIZE( pAttackHitSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackMissSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAttackSounds ); i++ )
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pAlertSounds ); i++ )
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pSuicideSounds ); i++ )
		PRECACHE_SOUND((char *)pSuicideSounds[i]);

	for ( i = 0; i < ARRAYSIZE( pPainSounds ); i++ )
		PRECACHE_SOUND((char *)pPainSounds[i]);

	PRECACHE_SOUND("crazylady/suicide_attempt.wav");
	PRECACHE_SOUND("crazylady/baby_burst.wav");
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================



int CCrazyBitch::IgnoreConditions ( void )
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
			m_flNextFlinch = gpGlobals->time + CRAZYBITCH_FLINCH_DELAY;
	}

	return iIgnore;

}

void CCrazyBitch :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

	StopSuicide();

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
// SetActivity - the first attack bursts the baby she holds
//=========================================================
void CCrazyBitch :: SetActivity ( Activity NewActivity )
{
	int	iSequence = ACTIVITY_NOT_AVAILABLE;
	void *pmodel = GET_MODEL_PTR( ENT(pev) );

	switch ( NewActivity )
	{
	case ACT_MELEE_ATTACK1:
		if ( !pev->body )
		{
			iSequence = LookupActivity ( ACT_SIGNAL2 );
			pev->body = 1;

			Vector vecSrc, vecAng;
			GetAttachment( 1, vecSrc, vecAng );

			EMIT_SOUND( ENT(pev), CHAN_AUTO, "crazylady/baby_burst.wav", 1, ATTN_NORM );

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_BLOODSPRITE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z );
				WRITE_SHORT( g_sModelIndexBloodSpray );
				WRITE_SHORT( g_sModelIndexBloodDrop );
				WRITE_BYTE( BLOOD_COLOR_RED );
				WRITE_BYTE( 16 );
			MESSAGE_END();
		}
		else
			iSequence = LookupActivity ( NewActivity );
		break;
	case ACT_MELEE_ATTACK2:
		if ( !pev->body )
		{
			iSequence = LookupActivity ( ACT_SIGNAL2 );
			pev->body = 1;

			Vector vecSrc, vecAng;
			GetAttachment( 1, vecSrc, vecAng );

			EMIT_SOUND( ENT(pev), CHAN_AUTO, "crazylady/baby_burst.wav", 1, ATTN_NORM );

			MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecSrc );
				WRITE_BYTE( TE_BLOODSPRITE );
				WRITE_COORD( vecSrc.x );
				WRITE_COORD( vecSrc.y );
				WRITE_COORD( vecSrc.z );
				WRITE_SHORT( g_sModelIndexBloodSpray );
				WRITE_SHORT( g_sModelIndexBloodDrop );
				WRITE_BYTE( BLOOD_COLOR_RED );
				WRITE_BYTE( 16 );
			MESSAGE_END();
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

	CBaseMonster::SetActivity( NewActivity );
}

//=========================================================
// MonsterThink - when she sees the player holding a gun that
// can be used for a suicide, she makes him shoot himself
//=========================================================
void CCrazyBitch :: MonsterThink( void )
{
	CBaseMonster::MonsterThink();

	if ( IsAlive() && !b_TriedToSuicide && !HasConditions( bits_COND_NO_AMMO_LOADED ) && HasConditions( bits_COND_SEE_ENEMY ) )
	{
		if ( m_hEnemy != NULL && !b_StartSuicide )
		{
			b_CanSuicideYou = true;
			b_StartSuicide = true;
			int random = 2;
			m_flSuicideAttempt = gpGlobals->time + random;
		}

		if ( m_hEnemy != NULL && b_StartSuicide == true && m_flSuicideAttempt < gpGlobals->time && !b_ReadyToSuicide )
		{
			if ( FClassnameIs( m_hEnemy->pev, "player" ) )
			{
				CBasePlayer *pPlayer = NULL;

				pPlayer = GetClassPtr( (CBasePlayer *)m_hEnemy->pev );

				bool cansuicide = false;

				if ( pPlayer && pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_bCanSuicide )
					cansuicide = true;

				// dual pistols: one of the two guns can be used
				if ( pPlayer->m_pActiveItem && pPlayer->m_pActiveItem->m_iId == 15 )
				{
					if ( ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iLeftWeaponID == 3 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iLeftWeaponID == 7 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iLeftWeaponID == 21 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iLeftWeaponID == 19 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iLeftWeaponID == 23 )
						cansuicide = true;

					if ( ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iRightWeaponID == 3 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iRightWeaponID == 7 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iRightWeaponID == 21 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iRightWeaponID == 19 || ((CDualWeapon *)pPlayer->m_pActiveItem)->m_iRightWeaponID == 23 )
						cansuicide = true;
				}

				if ( pPlayer && pPlayer->pev->vuser4.x == 0 && pPlayer->m_pActiveItem && cansuicide && b_CanSuicideYou )
				{
					b_StartSuicide = true;
					b_ReadyToSuicide = true;
					m_flSuicideAttempt = gpGlobals->time + 1;
				}
			}
		}
	}

	if ( b_StartSuicide && m_flSuicideAttempt < gpGlobals->time && b_ReadyToSuicide )
	{
		CBasePlayer *pPlayer = NULL;
		pPlayer = (CBasePlayer *)CBaseEntity::Instance( INDEXENT( 1 ) );

		if ( pPlayer && pPlayer->m_pActiveItem && IsAlive() )
		{
			pSuicidePlayer = pPlayer;

			int weaponid = pPlayer->m_pActiveItem->m_iId;
			bool must_be_switched = false;

			// dual pistols: switch to the gun that can be used
			if ( weaponid == 15 && !((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_bIronSights && ((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_flNextPrimaryAttack < gpGlobals->time && ((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_flNextSecondaryAttack < gpGlobals->time && ((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_flNextTertiaryAttack < gpGlobals->time )
			{
				if ( pPlayer->m_iDualWeaponLeftID == 3 || pPlayer->m_iDualWeaponLeftID == 7 || pPlayer->m_iDualWeaponLeftID == 21 || pPlayer->m_iDualWeaponLeftID == 19 || pPlayer->m_iDualWeaponLeftID == 23 )
					weaponid = pPlayer->m_iDualWeaponLeftID;

				if ( pPlayer->m_iDualWeaponRightID == 3 || pPlayer->m_iDualWeaponRightID == 7 || pPlayer->m_iDualWeaponRightID == 21 || pPlayer->m_iDualWeaponRightID == 19 || pPlayer->m_iDualWeaponRightID == 23 )
					weaponid = pPlayer->m_iDualWeaponRightID;

				must_be_switched = true;

				pPlayer->SelectItem( STRING( pPlayer->GetPlayerWeapon( weaponid )->pev->classname ) );
			}

			switch ( weaponid )
			{
			case 23:
				{
					CFAMAS *pFamas = (CFAMAS *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pFamas )
					{
						if ( !pFamas->m_bIronSights && pFamas->m_flNextPrimaryAttack < gpGlobals->time && pFamas->m_flNextSecondaryAttack < gpGlobals->time && pFamas->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pFamas->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 26:
				{
					CG43 *pG43 = (CG43 *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pG43 )
					{
						if ( !pG43->m_bIronSights && pG43->m_flNextPrimaryAttack < gpGlobals->time && pG43->m_flNextSecondaryAttack < gpGlobals->time && pG43->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pG43->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 3:
				{
					CGlock *pGlock = (CGlock *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pGlock )
					{
						if ( !pGlock->m_bWaitingToFix && !pGlock->m_bIronSights && pGlock->m_flNextPrimaryAttack < gpGlobals->time && pGlock->m_flNextSecondaryAttack < gpGlobals->time && pGlock->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pGlock->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 9:
				{
					CM16 *pM16 = (CM16 *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pM16 )
					{
						if ( !pM16->m_bIronSights && pM16->m_flNextPrimaryAttack < gpGlobals->time && pM16->m_flNextSecondaryAttack < gpGlobals->time && pM16->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pM16->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 7:
				{
					CP345 *pP345 = (CP345 *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pP345 )
					{
						if ( !pP345->m_bIronSights && pP345->m_flNextPrimaryAttack < gpGlobals->time && pP345->m_flNextSecondaryAttack < gpGlobals->time && pP345->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pP345->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 21:
				{
					CRevolver *pRevolver = (CRevolver *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pRevolver )
					{
						if ( !pRevolver->m_bIronSights && !pRevolver->m_bReloading && !pRevolver->m_bWantToEndReload && pRevolver->m_flNextPrimaryAttack < gpGlobals->time && pRevolver->m_flNextSecondaryAttack < gpGlobals->time && pRevolver->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pRevolver->m_iSuicideState = 1;
						}
					}
				}
				break;

			// CoF: no break after the rifle and the TMP, they also run the next case (original bug)
			case 11:
				{
					CRifle *pRifle = (CRifle *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pRifle )
					{
						if ( !pRifle->m_bIronSights && pRifle->m_iWantToEjectShell == 0 && !pRifle->m_bWaitingToReload && !pRifle->m_bRaised && pRifle->m_flNextPrimaryAttack < gpGlobals->time && pRifle->m_flNextSecondaryAttack < gpGlobals->time && pRifle->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pRifle->m_iSuicideState = 1;
						}
					}
				}

			case 4:
				{
					CShotgun *pShotgun = (CShotgun *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pShotgun )
					{
						if ( !pShotgun->m_bIronSights && pShotgun->m_fInReload == 0 && pShotgun->m_flNextPrimaryAttack < gpGlobals->time && pShotgun->m_flNextSecondaryAttack < gpGlobals->time && pShotgun->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pShotgun->m_iSuicideState = 1;
						}
					}
				}
				break;

			case 14:
				{
					CTmp *pTMP = (CTmp *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pTMP )
					{
						if ( !pTMP->m_bIronSights && pTMP->m_flNextPrimaryAttack < gpGlobals->time && pTMP->m_flNextSecondaryAttack < gpGlobals->time && pTMP->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pTMP->m_iSuicideState = 1;
						}
					}
				}

			case 27:
				{
					// CoF: the MP5 (new in 1.6a); its class is not declared in a header yet
					CBasePlayerWeapon *pMp5 = pPlayer->GetPlayerWeapon( weaponid );

					if ( pMp5 )
					{
						if ( !pMp5->m_bIronSights && pMp5->m_flNextPrimaryAttack < gpGlobals->time && pMp5->m_flNextSecondaryAttack < gpGlobals->time && pMp5->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pMp5->m_iSuicideState = 1;
						}
					}
				}

			case 19:
				{
					CVP70 *pVP70 = (CVP70 *)pPlayer->GetPlayerWeapon( weaponid );

					if ( pVP70 )
					{
						if ( !pVP70->m_bIronSights && pVP70->m_flNextPrimaryAttack < gpGlobals->time && pVP70->m_flNextSecondaryAttack < gpGlobals->time && pVP70->m_flNextTertiaryAttack < gpGlobals->time )
						{
							Stop();
							RouteClear();
							SetActivity( ACT_SIGNAL1 );

							b_StartSuicide = false;
							b_CanSuicideYou = false;
							b_TriedToSuicide = true;

							if ( must_be_switched )
								pPlayer->GetPlayerWeapon( weaponid )->m_iSuicideState = 666;
							else
								pVP70->m_iSuicideState = 1;
						}
					}
				}
				break;
			}
		}
	}
}
