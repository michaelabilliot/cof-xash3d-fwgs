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
// Doctor boss: hides behind four boxes and shoots the
// player with the weapon the player gave up (p345 or
// revolver). The cof_doctorshoot brushes in front of the
// boxes pass the damage on while he aims or shoots.
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"weapons.h"
#include	"player.h"
#include	"animation.h"
#include	"cof_doctorboss.h"

extern int gmsgCustomDLight;
extern bool m_gbIsInCutscene;

const char *ActivityGet[] = { NULL, "HIDING", "AIMING", "RUNNING" };

int sameactioncount = 0;

LINK_ENTITY_TO_CLASS( monster_doctorboss, CDoctorBoss );

//=========================================================
// KeyValue
//=========================================================
void CDoctorBoss :: KeyValue( KeyValueData *pkvd )
{
	CBaseMonster::KeyValue( pkvd );
}

//=========================================================
// Classify - indicates this monster's place in the
// relationship table.
//=========================================================
int	CDoctorBoss :: Classify ( void )
{
	return CLASS_HUMAN_MILITARY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CDoctorBoss :: SetYawSpeed ( void )
{
	pev->yaw_speed = 0;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CDoctorBoss :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

void CDoctorBoss :: SetDoctorAnimation( char *szSequence )
{
	Stop();
	RouteClear();
	SetActivity( GetAct( szSequence ) );
	m_flSequenceTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), LookupSequence( szSequence ) ) + gpGlobals->time;
}

Activity CDoctorBoss :: GetAct( char *szSequence )
{
	if ( !strcmp( szSequence, "box1hide" ) )
		return ACT_GUARD;

	if ( !strcmp( szSequence, "box1getup" ) )
		return ACT_SWIM;

	if ( !strcmp( szSequence, "box1shoot" ) )
		return ACT_HOP;

	if ( !strcmp( szSequence, "box1getback" ) )
		return ACT_LEAP;

	if ( !strcmp( szSequence, "box1runtobox2" ) )
		return ACT_FALL;

	if ( !strcmp( szSequence, "box2hide" ) )
		return ACT_LAND;

	if ( !strcmp( szSequence, "box2getup" ) )
		return ACT_STRAFE_LEFT;

	if ( !strcmp( szSequence, "box2shoot" ) )
		return ACT_STRAFE_RIGHT;

	if ( !strcmp( szSequence, "box2getback" ) )
		return ACT_ROLL_LEFT;

	if ( !strcmp( szSequence, "box2runtobox1" ) )
		return ACT_ROLL_RIGHT;

	if ( !strcmp( szSequence, "box2runtobox3" ) )
		return ACT_CROUCH;

	if ( !strcmp( szSequence, "box3hide" ) )
		return ACT_CROUCHIDLE;

	if ( !strcmp( szSequence, "box3getup" ) )
		return ACT_STAND;

	if ( !strcmp( szSequence, "box3shoot" ) )
		return ACT_USE;

	if ( !strcmp( szSequence, "box3getback" ) )
		return ACT_SIGNAL1;

	if ( !strcmp( szSequence, "box3runtobox2" ) )
		return ACT_SIGNAL2;

	if ( !strcmp( szSequence, "box3runtobox4" ) )
		return ACT_SIGNAL3;

	if ( !strcmp( szSequence, "box4hide" ) )
		return ACT_TWITCH;

	if ( !strcmp( szSequence, "box4getup" ) )
		return ACT_COWER;

	if ( !strcmp( szSequence, "box4shoot" ) )
		return ACT_SMALL_FLINCH;

	if ( !strcmp( szSequence, "box4getback" ) )
		return ACT_BIG_FLINCH;

	if ( !strcmp( szSequence, "box4runtobox3" ) )
		return ACT_RELOAD;

	if ( !strcmp( szSequence, "nothing" ) )
		return ACT_ARM;

	return ACT_RESET;
}

void CDoctorBoss :: SendInitMessage( CBasePlayer *player )
{
	m_iCurrentBox = 2;

	m_flNextShoot = gpGlobals->time;


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

	pev->spawnflags = SF_MONSTER_NO_YELLOW_BLOBS;
}

void CDoctorBoss :: HurtSound( void )
{
	if ( pev->fuser4 < gpGlobals->time )
	{
		switch ( RANDOM_LONG( 1, 7 ) )
		{
		case 1:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch1.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 2:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch2.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 3:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch3.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 4:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch4.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 5:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch5.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 6:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch6.wav", 1, ATTN_NORM, 0, 100 );
			break;

		case 7:
			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "doc_ai/ouch7.wav", 1, ATTN_NORM, 0, 100 );
			break;
		}

		pev->fuser4 = gpGlobals->time + 0.5;
	}
}

//=========================================================
// Spawn
//=========================================================
void CDoctorBoss :: Spawn()
{
	m_bJustTakenDamage = false;

	Precache( );

	if ( pev->model )
		SET_MODEL(ENT(pev), STRING(pev->model));
	else
		SET_MODEL(ENT(pev), "models/doctor_boss.mdl");

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= DONT_BLEED;
	pev->health			= gSkillData.doctorHealth;
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	m_iCurrentBox = 2;

	m_flNextShoot = gpGlobals->time;


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

	pev->spawnflags = SF_MONSTER_NO_YELLOW_BLOBS;

	MonsterInit();
}

void CDoctorBoss :: Killed( entvars_t *pevAttacker, int iGib )
{
	m_bKilled = true;

	if ( pev->target )
	{
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pPlayer && pPlayer->IsPlayer() )
			FireTargets( STRING( pev->target ), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	UTIL_Remove( this );
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CDoctorBoss :: Precache()
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL("models/doctor_boss.mdl");

	m_iBrassShell = PRECACHE_MODEL("models/shell.mdl");

	PRECACHE_SOUND("doc_ai/ouch1.wav");
	PRECACHE_SOUND("doc_ai/ouch2.wav");
	PRECACHE_SOUND("doc_ai/ouch3.wav");
	PRECACHE_SOUND("doc_ai/ouch4.wav");
	PRECACHE_SOUND("doc_ai/ouch5.wav");
	PRECACHE_SOUND("doc_ai/ouch6.wav");
	PRECACHE_SOUND("doc_ai/ouch7.wav");

	PRECACHE_SOUND("doc_ai/p345_reload.wav");
	PRECACHE_SOUND("doc_ai/revolver_reload.wav");
	PRECACHE_SOUND("weapons/explode5.wav");
}

//=========================================================
// DecideWhatToDo - the doctor's AI: hide, get up and shoot,
// or run to the next box
//=========================================================
void CDoctorBoss :: DecideWhatToDo( void )
{
	if ( m_flReloadTime > gpGlobals->time )
		return;

	if ( m_bWaitingToReload )
		return;

	if ( m_flNextAIThink > gpGlobals->time )
		return;

	int chosenactivity = RANDOM_LONG( 0, 2 );

	// shoot more often
	int shootoverride = RANDOM_LONG( 1, 3 );

	if ( shootoverride == 1 )
	{
		chosenactivity = 1;
	}

	if ( m_bJustTakenDamage && chosenactivity == 1 )
	{
		// maybe run away after being hurt
		int overridechance = RANDOM_LONG( 0, 1 );

		if ( overridechance )
		{
			m_bJustTakenDamage = false;
			chosenactivity = 2;
		}
	}

	if ( m_iActivity == 2 )
	{
		// running to another box
		if ( m_flSequenceTime > gpGlobals->time )
		{
			m_flNextAIThink = gpGlobals->time + 0.1;
			return;
		}

		// arrived, hide

		switch ( m_iCurrentBox )
		{
		case 1:
			SetDoctorAnimation( "box1hide" );
			break;
		case 2:
			SetDoctorAnimation( "box2hide" );
			break;
		case 3:
			SetDoctorAnimation( "box3hide" );
			break;
		case 4:
			SetDoctorAnimation( "box4hide" );
			break;
		}

		m_iActivity = 0;

		m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
		return;
	}

	if ( chosenactivity == 0 && m_iActivity == 0 )
	{
		sameactioncount++;
	}

	if ( sameactioncount >= 3 )
	{
		// hid too long, run

		chosenactivity = 2;
		m_flNextAIThink = gpGlobals->time + 0.1;
		sameactioncount = 0;
	}

	switch ( chosenactivity )
	{
	case 0:
		// hide
		switch ( m_iActivity )
		{
		case 0:
			// already hiding
			m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			m_iActivity = chosenactivity;
			return;

		case 1:
			// standing up, get back
			switch ( m_iCurrentBox )
			{
			case 1:
				SetDoctorAnimation( "box1getback" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 2:
				SetDoctorAnimation( "box2getback" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 3:
				SetDoctorAnimation( "box3getback" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 4:
				SetDoctorAnimation( "box4getback" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;
			}
		}

	case 1:
		// get up and shoot
		switch ( m_iActivity )
		{
		case 0:
			switch ( m_iCurrentBox )
			{
			case 1:
				SetDoctorAnimation( "box1getup" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 2:
				SetDoctorAnimation( "box2getup" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 3:
				SetDoctorAnimation( "box3getup" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;

			case 4:
				SetDoctorAnimation( "box4getup" );
				m_iActivity = chosenactivity;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				return;
			}
			break;

		case 1:
			m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			return;
		}

	case 2:
		// run to another box
		if ( m_iActivity != 2 )
		{
			switch ( m_iCurrentBox )
			{
			case 1:

				SetDoctorAnimation( "box1runtobox2" );
				m_iActivity = chosenactivity;
				m_iCurrentBox = 2;

				m_flNextAIThink = gpGlobals->time + 0.1;
				return;

			case 2:

				if ( RANDOM_LONG( 0, 1 ) == 1 )
				{
					SetDoctorAnimation( "box2runtobox1" );
					m_iActivity = chosenactivity;
					m_iCurrentBox = 1;

					m_flNextAIThink = gpGlobals->time + 0.1;
					return;
				}
				else
				{
					SetDoctorAnimation( "box2runtobox3" );
					m_iActivity = chosenactivity;
					m_iCurrentBox = 3;

					m_flNextAIThink = gpGlobals->time + 0.1;
					return;
				}

			case 3:

				if ( RANDOM_LONG( 0, 1 ) == 1 )
				{
					SetDoctorAnimation( "box3runtobox2" );
					m_iActivity = chosenactivity;
					m_iCurrentBox = 2;

					m_flNextAIThink = gpGlobals->time + 0.1;
					return;
				}
				else
				{
					SetDoctorAnimation( "box3runtobox4" );
					m_iActivity = chosenactivity;
					m_iCurrentBox = 4;

					m_flNextAIThink = gpGlobals->time + 0.1;
					return;
				}

			case 4:

				SetDoctorAnimation( "box4runtobox3" );
				m_iActivity = chosenactivity;
				m_iCurrentBox = 3;

				m_flNextAIThink = gpGlobals->time + 0.1;
				return;
			}
		}
	}
}

void CDoctorBoss :: MonsterThink( void )
{
	if ( m_bKilled )
		return;

	if ( m_iGunType == 0 )
	{
		// wait for the player to choose the weapon
		CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pPlayer )
		{
			int value = GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_iDoctorWeapon;

			if ( value > 0 )
			{
				m_iGunType = value;

				if ( m_iGunType == 1 )
				{
					// p345

					pev->body = 0;
					m_iClip = 8;
				}

				if ( m_iGunType == 2 )
				{
					// revolver

					pev->body = 1;
					m_iClip = 6;
				}
			}
		}

		SetNextThink( 0.05 );
		return;
	}

	StudioFrameAdvance( );
	DecideWhatToDo();

	if ( pPlayer )
	{
		if ( m_hTargetEnt == NULL || m_hTargetEnt != pPlayer )
			m_hTargetEnt = pPlayer;

		if ( m_hEnemy == NULL || m_hEnemy != pPlayer )
			m_hEnemy = pPlayer;
	}

	if ( m_bWaitingToReload && m_flSequenceTime < gpGlobals->time )
	{
		// reload behind the box

		if ( GetActivity() == GetAct( "box1hide" ) || GetActivity() == GetAct( "box2hide" ) || GetActivity() == GetAct( "box3hide" ) || GetActivity() == GetAct( "box4hide" ) || GetActivity() == GetAct( "box1getback" ) || GetActivity() == GetAct( "box2getback" ) || GetActivity() == GetAct( "box3getback" ) || GetActivity() == GetAct( "box4getback" ) )
		{
			Vector BoneOrigin, BoneAngles;
			GetBonePosition( 9, BoneOrigin, BoneAngles );

			switch ( m_iGunType )
			{
			case 1:
				m_flReloadTime = gpGlobals->time + 3;
				m_bWaitingToReload = false;
				UTIL_EmitAmbientSound( edict(), BoneOrigin, "doc_ai/p345_reload.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );
				m_iClip = 8;
				break;

			case 2:
				m_flReloadTime = gpGlobals->time + 5;
				m_bWaitingToReload = false;
				UTIL_EmitAmbientSound( edict(), BoneOrigin, "doc_ai/revolver_reload.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );
				m_iClip = 6;
				break;
			}
		}
	}

	if ( m_iActivity == 1 )
	{
		// standing up: shoot

		if ( GetActivity() == GetAct( "box1shoot" ) || GetActivity() == GetAct( "box2shoot" ) || GetActivity() == GetAct( "box3shoot" ) || GetActivity() == GetAct( "box4shoot" ) )
		{
			if ( m_flNextShoot < gpGlobals->time && pPlayer )
			{
				TraceResult tr;
				Vector AngleVec;


				Vector BoneOrigin, BoneAngles;
				GetBonePosition( 9, BoneOrigin, BoneAngles );

			switch ( m_iGunType )
			{
			case 1:
				UTIL_TraceLine( BoneOrigin, pPlayer->EyePosition(), dont_ignore_monsters, ENT(pev), &tr );

				if ( tr.pHit )
				{
					CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

					if ( pHit && pHit->IsPlayer() )
					{
						if ( m_iClip > 0 )
						{
							m_flNextShoot = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.5 );
							m_iClip--;

							if ( m_iGunType == 1 )
							{
								UTIL_MakeVectors( pev->angles );
								Vector vecShootOrigin = GetGunPosition();
								Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

								Vector vecBrassPos, vecBrassDir;
								GetAttachment( 1, vecBrassPos, vecBrassDir );
								Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
								EjectBrass( vecBrassPos, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );


								float minaccuracy = 2;
								float maxaccuracy = 10;

								Vector vecEnd = pPlayer->EyePosition() + Vector( RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ) );
								TraceResult tr;
								UTIL_TraceLine( vecBrassPos, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

								CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

								if ( pHit && pHit->IsPlayer() )
									pPlayer->TakeDamage( pev, pev, gSkillData.doctorP345, DMG_BULLET );

								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/p345/p345_fire.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

								MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, pev->origin );
									WRITE_COORD( BoneOrigin.x );
									WRITE_COORD( BoneOrigin.y );
									WRITE_COORD( BoneOrigin.z );
									WRITE_BYTE( 10 );
									WRITE_BYTE( 5 );
									WRITE_BYTE( 30 );
								MESSAGE_END();
							}

							if ( m_iGunType == 2 )
							{
								UTIL_MakeVectors( pev->angles );
								Vector vecShootOrigin = GetGunPosition();
								Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

								Vector vecBrassPos, vecBrassDir;
								GetAttachment( 1, vecBrassPos, vecBrassDir );

								float minaccuracy = 2;
								float maxaccuracy = 10;

								Vector vecEnd = pPlayer->EyePosition() + Vector( RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ) );
								TraceResult tr;
								UTIL_TraceLine( vecBrassPos, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

								CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

								if ( pHit && pHit->IsPlayer() )
									pPlayer->TakeDamage( pev, pev, gSkillData.doctorRevolver, DMG_BULLET );

								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/revolver/revolver_fire.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

								MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, pev->origin );
									WRITE_COORD( BoneOrigin.x );
									WRITE_COORD( BoneOrigin.y );
									WRITE_COORD( BoneOrigin.z );
									WRITE_BYTE( 10 );
									WRITE_BYTE( 5 );
									WRITE_BYTE( 30 );
								MESSAGE_END();
							}
						}
						else
						{
							if ( m_iGunType == 1 )
								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/weapon_fire_empty.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );
							if ( m_iGunType == 2 )
								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/weapon_fire_empty_noclick.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

							m_bWaitingToReload = true;

							switch ( m_iCurrentBox )
							{
							case 1:
								SetDoctorAnimation( "box1getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 2:
								SetDoctorAnimation( "box2getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 3:
								SetDoctorAnimation( "box3getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 4:
								SetDoctorAnimation( "box4getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;
							}
						}
					}
				}
				break;

			case 2:
				UTIL_TraceLine( BoneOrigin, pPlayer->EyePosition(), dont_ignore_monsters, ENT(pev), &tr );

				if ( tr.pHit )
				{
					CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

					if ( pHit && pHit->IsPlayer() )
					{
						if ( m_iClip > 0 )
						{
							m_flNextShoot = gpGlobals->time + RANDOM_FLOAT( 0.5, 1.5 );
							m_iClip--;

							if ( m_iGunType == 1 )
							{
								UTIL_MakeVectors( pev->angles );
								Vector vecShootOrigin = GetGunPosition();
								Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

								Vector vecBrassPos, vecBrassDir;
								GetAttachment( 1, vecBrassPos, vecBrassDir );
								Vector vecShellVelocity = gpGlobals->v_right * RANDOM_FLOAT( 40, 90 ) + gpGlobals->v_up * RANDOM_FLOAT( 75, 200 ) + gpGlobals->v_forward * RANDOM_FLOAT( -40, 40 );
								EjectBrass( vecBrassPos, vecShellVelocity, pev->angles.y, m_iBrassShell, TE_BOUNCE_SHELL );


								float minaccuracy = 2;
								float maxaccuracy = 10;

								Vector vecEnd = pPlayer->EyePosition() + Vector( RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ) );
								TraceResult tr;
								UTIL_TraceLine( vecBrassPos, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

								CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

								if ( pHit && pHit->IsPlayer() )
									pPlayer->TakeDamage( pev, pev, gSkillData.doctorP345, DMG_BULLET );

								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/p345/p345_fire.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

								MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, pev->origin );
									WRITE_COORD( BoneOrigin.x );
									WRITE_COORD( BoneOrigin.y );
									WRITE_COORD( BoneOrigin.z );
									WRITE_BYTE( 10 );
									WRITE_BYTE( 5 );
									WRITE_BYTE( 30 );
								MESSAGE_END();
							}

							if ( m_iGunType == 2 )
							{
								UTIL_MakeVectors( pev->angles );
								Vector vecShootOrigin = GetGunPosition();
								Vector vecShootDir = ShootAtEnemy( vecShootOrigin );

								Vector vecBrassPos, vecBrassDir;
								GetAttachment( 1, vecBrassPos, vecBrassDir );

								float minaccuracy = 2;
								float maxaccuracy = 10;

								Vector vecEnd = pPlayer->EyePosition() + Vector( RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ), RANDOM_FLOAT( minaccuracy, maxaccuracy ) );
								TraceResult tr;
								UTIL_TraceLine( vecBrassPos, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

								CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

								if ( pHit && pHit->IsPlayer() )
									pPlayer->TakeDamage( pev, pev, gSkillData.doctorRevolver, DMG_BULLET );

								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/revolver/revolver_fire.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

								MESSAGE_BEGIN( MSG_PAS, gmsgCustomDLight, pev->origin );
									WRITE_COORD( BoneOrigin.x );
									WRITE_COORD( BoneOrigin.y );
									WRITE_COORD( BoneOrigin.z );
									WRITE_BYTE( 10 );
									WRITE_BYTE( 5 );
									WRITE_BYTE( 30 );
								MESSAGE_END();
							}
						}
						else
						{
							if ( m_iGunType == 1 )
								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/weapon_fire_empty.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );
							if ( m_iGunType == 2 )
								UTIL_EmitAmbientSound( edict(), BoneOrigin, "weapons/weapon_fire_empty_noclick.wav", 1, ATTN_NONE, 0, 100 + RANDOM_LONG( -10, 10 ) );

							m_bWaitingToReload = true;

							switch ( m_iCurrentBox )
							{
							case 1:
								SetDoctorAnimation( "box1getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 2:
								SetDoctorAnimation( "box2getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 3:
								SetDoctorAnimation( "box3getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;

							case 4:
								SetDoctorAnimation( "box4getback" );
								m_iActivity = 0;

								m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
								break;
							}
						}
					}
				}
				break;
			}
			}
		}

		if ( m_flSequenceTime < gpGlobals->time )
		{
			switch ( m_iCurrentBox )
			{
			case 1:
				if ( GetActivity() != GetAct( "box1shoot" ) )
					SetDoctorAnimation( "box1shoot" );
				break;

			case 2:
				if ( GetActivity() != GetAct( "box2shoot" ) )
					SetDoctorAnimation( "box2shoot" );
				break;

			case 3:
				if ( GetActivity() != GetAct( "box3shoot" ) )
					SetDoctorAnimation( "box3shoot" );
				break;

			case 4:
				if ( GetActivity() != GetAct( "box4shoot" ) )
					SetDoctorAnimation( "box4shoot" );
				break;
			}
		}
	}

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

	if ( pPlayer )
	{
		if ( !m_gbIsInCutscene )
		{
			// look at the player
			Vector2D	vec2LOS;
			UTIL_MakeVectors ( pev->angles );

			vec2LOS = ( pPlayer->pev->origin - pev->origin ).Make2D();
			vec2LOS = vec2LOS.Normalize();

			float yaw = VecToYaw( pPlayer->pev->origin - pev->origin ) - pev->angles.y;

			if (yaw > 180) yaw -= 360;
			if (yaw < -180) yaw += 360;

			SetBoneController( 0, yaw );
		}

		if ( m_gbIsInCutscene )
		{
			SetBoneController( 0, 0 );
		}
	}

	SetNextThink( 0.05 );
}

int CDoctorBoss :: IsAimingOrShooting( void )
{
	if ( GetActivity() == GetAct( "box1shoot" ) )
		return 1;

	if ( GetActivity() == GetAct( "box2shoot" ) )
		return 2;

	if ( GetActivity() == GetAct( "box3shoot" ) )
		return 3;

	if ( GetActivity() == GetAct( "box4shoot" ) )
		return 4;

	if ( GetActivity() == GetAct( "box1getup" ) )
		return 5;

	if ( GetActivity() == GetAct( "box2getup" ) )
		return 6;

	if ( GetActivity() == GetAct( "box3getup" ) )
		return 7;

	if ( GetActivity() == GetAct( "box4getup" ) )
		return 8;

	return 0;
}

int CDoctorBoss :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( IsAimingOrShooting() > 4 )
	{
		HurtSound();
		return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	// iuser2 is set by the cof_doctorshoot brush that passed the damage on
	if ( pev->iuser2 == 1 )
	{
		pev->iuser2 = 0;


		if ( m_iActivity == 1 )
		{
			// hit while shooting, get back


			switch ( m_iCurrentBox )
			{
			case 1:
				SetDoctorAnimation( "box1getback" );
				m_iActivity = 0;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				break;

			case 2:
				SetDoctorAnimation( "box2getback" );
				m_iActivity = 0;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				break;

			case 3:
				SetDoctorAnimation( "box3getback" );
				m_iActivity = 0;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				break;

			case 4:
				SetDoctorAnimation( "box4getback" );
				m_iActivity = 0;

				m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
				break;
			}
		}

		HurtSound();

		m_bJustTakenDamage = true;
		return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	return 0;
}

//=========================================================
// doctor_helper - a damage target for the doctor boss (its
// owner) that copies its health to him
//=========================================================
class CDoctorHelper : public CBaseMonster
{
public:
	void Spawn( void );
	int  Classify ( void );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
};

LINK_ENTITY_TO_CLASS( doctor_helper, CDoctorHelper );

//=========================================================
// Classify
//=========================================================
int	CDoctorHelper :: Classify ( void )
{
	return CLASS_NONE;
}

//=========================================================
// Spawn
//=========================================================
void CDoctorHelper :: Spawn()
{
	SET_MODEL(ENT(pev), "models/doctor_boss.mdl");

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_RED;
	m_MonsterState		= MONSTERSTATE_NONE;

	pev->health			= gSkillData.doctorHealth;

	MonsterInit();
}

int CDoctorHelper :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( !pev->owner )
		return 0;

	CDoctorBoss *pDoctor = (CDoctorBoss *)CBaseEntity::Instance( pev->owner );

	if ( !pDoctor )
		return 0;


	if ( pDoctor->m_iActivity == 1 )
	{
		switch ( pDoctor->m_iCurrentBox )
		{
		case 1:
			pDoctor->SetDoctorAnimation( "box1getback" );
			pDoctor->m_iActivity = 0;

			pDoctor->m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			break;

		case 2:
			pDoctor->SetDoctorAnimation( "box2getback" );
			pDoctor->m_iActivity = 0;

			pDoctor->m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			break;

		case 3:
			pDoctor->SetDoctorAnimation( "box3getback" );
			pDoctor->m_iActivity = 0;

			pDoctor->m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			break;

		case 4:
			pDoctor->SetDoctorAnimation( "box4getback" );
			pDoctor->m_iActivity = 0;

			pDoctor->m_flNextAIThink = gpGlobals->time + RANDOM_FLOAT( 1, 2.5 );
			break;
		}
	}

	pDoctor->m_bJustTakenDamage = true;
	int value = CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );

	pDoctor->pev->health = pev->health;

	return value;
}

void CDoctorHelper :: Killed( entvars_t *pevAttacker, int iGib )
{
	if ( !pev->owner )
		return;

	CDoctorBoss *pDoctor = (CDoctorBoss *)CBaseEntity::Instance( pev->owner );
}
