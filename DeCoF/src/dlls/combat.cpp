/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== combat.cpp ========================================================

  functions dealing with damage infliction & death

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "soundent.h"
#include "decals.h"
#include "animation.h"
#include "weapons.h"
#include "player.h"
#include "gamerules.h"
#include "func_break.h"
#include "../engine/studio.h" //LRC
#include "pm_materials.h" // buz
#include "cof_doctorboss.h"

extern DLL_GLOBAL Vector		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern entvars_t *g_pevLastInflictor;

extern int gmsgParticle;
extern int gmsgBossBar;


bool fvisiblecheck;

#define GERMAN_GIB_COUNT		4
#define	HUMAN_GIB_COUNT			6
#define ALIEN_GIB_COUNT			4


// HACKHACK -- The gib velocity equations don't work
void CGib :: LimitVelocity( void )
{
	float length = pev->velocity.Length();

	// ceiling at 1500.  The gib velocity equation is not bounded properly.  Rather than tune it
	// in 3 separate places again, I'll just limit it here.
	if ( length > 1500.0 )
		pev->velocity = pev->velocity.Normalize() * 1500;		// This should really be sv_maxvelocity * 0.75 or something
}


void CGib :: SpawnStickyGibs( entvars_t *pevVictim, Vector vecOrigin, int cGibs )
{
	int i;

	if ( g_Language == LANGUAGE_GERMAN )
	{
		// no sticky gibs in germany right now!
		return;
	}

	for ( i = 0 ; i < cGibs ; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->Spawn( "models/stickygib.mdl" );
		pGib->pev->body = RANDOM_LONG(0,2);

		if ( pevVictim )
		{
			pGib->pev->origin.x = vecOrigin.x + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.y = vecOrigin.y + RANDOM_FLOAT( -3, 3 );
			pGib->pev->origin.z = vecOrigin.z + RANDOM_FLOAT( -3, 3 );

			/*
			pGib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * (RANDOM_FLOAT ( 0 , 1 ) );
			*/

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.15, 0.15 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.15, 0.15 );

			pGib->pev->velocity = pGib->pev->velocity * 900;

			pGib->pev->avelocity.x = RANDOM_FLOAT ( 250, 400 );
			pGib->pev->avelocity.y = RANDOM_FLOAT ( 250, 400 );

			// copy owner's blood color
			pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();

			if ( pevVictim->health > -50)
			{
				pGib->pev->velocity = pGib->pev->velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
				pGib->pev->velocity = pGib->pev->velocity * 2;
			}
			else
			{
				pGib->pev->velocity = pGib->pev->velocity * 4;
			}


			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector ( 0, 0 ,0 ), Vector ( 0, 0, 0 ) );
			pGib->SetTouch(&CGib:: StickyGibTouch );
			pGib->SetThink (NULL);
		}
		pGib->LimitVelocity();
	}
}

void CGib :: SpawnHeadGib( entvars_t *pevVictim )
{
	if ( g_Language == LANGUAGE_GERMAN )
		SpawnHeadGib(pevVictim, "models/germangibs.mdl" );// throw one head
	else
		SpawnHeadGib(pevVictim, "models/hgibs.mdl" );
}

void CGib :: SpawnGib( entvars_t *pevVictim, const char *szGibModel, int body )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

	pGib->Spawn( szGibModel );
	pGib->pev->body = body;

	if ( FStrEq( szGibModel, "models/simon_gibs.mdl" ) )
	{
		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( pGib->entindex() );
			WRITE_STRING( "aurora/blood_effect.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( szGibModel );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	if ( pevVictim )
	{
		pGib->pev->origin = pevVictim->origin + pevVictim->view_ofs;

		edict_t		*pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict() );

		if ( RANDOM_LONG ( 0, 100 ) <= 5 && pentPlayer )
		{
			// 5% chance head will be thrown at player's face.
			entvars_t	*pevPlayer;

			pevPlayer = VARS( pentPlayer );
			pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
			pGib->pev->velocity.z += 100;
		}
		else
		{
			pGib->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
		}


		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

		// copy owner's blood color
		pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();

		if ( pevVictim->health > -50)
		{
			pGib->pev->velocity = pGib->pev->velocity * 0.7;
		}
		else if ( pevVictim->health > -200)
		{
			pGib->pev->velocity = pGib->pev->velocity * 2;
		}
		else
		{
			pGib->pev->velocity = pGib->pev->velocity * 4;
		}
	}
	pGib->LimitVelocity();
}

void CGib :: SpawnHeadGib( entvars_t *pevVictim, const char* szGibModel )
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

	pGib->Spawn( szGibModel );// throw one head
	pGib->pev->body = 0;

	if ( pevVictim )
	{
		pGib->pev->origin = pevVictim->origin + pevVictim->view_ofs;

		edict_t		*pentPlayer = FIND_CLIENT_IN_PVS( pGib->edict() );

		if ( RANDOM_LONG ( 0, 100 ) <= 5 && pentPlayer )
		{
			// 5% chance head will be thrown at player's face.
			entvars_t	*pevPlayer;

			pevPlayer = VARS( pentPlayer );
			pGib->pev->velocity = ( ( pevPlayer->origin + pevPlayer->view_ofs ) - pGib->pev->origin ).Normalize() * 300;
			pGib->pev->velocity.z += 100;
		}
		else
		{
			pGib->pev->velocity = Vector (RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));
		}


		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

		// copy owner's blood color
		pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();

		if ( pevVictim->health > -50)
		{
			pGib->pev->velocity = pGib->pev->velocity * 0.7;
		}
		else if ( pevVictim->health > -200)
		{
			pGib->pev->velocity = pGib->pev->velocity * 2;
		}
		else
		{
			pGib->pev->velocity = pGib->pev->velocity * 4;
		}
	}
	pGib->LimitVelocity();
}

void CGib :: SpawnRandomGibs( entvars_t *pevVictim, int cGibs, int human )
{
	if ( g_Language == LANGUAGE_GERMAN )
		SpawnRandomGibs(pevVictim, cGibs, 1, "models/germangibs.mdl");
	else if (human)
		SpawnRandomGibs(pevVictim, cGibs, 1, "models/hgibs.mdl");
	else
		SpawnRandomGibs(pevVictim, cGibs, 0, "models/agibs.mdl");
}

//LRC - changed signature, to support custom gib models
void CGib :: SpawnRandomGibs( entvars_t *pevVictim, int cGibs, int notfirst, const char *szGibModel )
{
	if (cGibs == 0) return; // spawn nothing!

	CGib *pGib = GetClassPtr( (CGib *)NULL );
	pGib->Spawn( szGibModel );

	//LRC - check the model itself to find out how many gibs are available
	studiohdr_t *pstudiohdr = (studiohdr_t *)(GET_MODEL_PTR( ENT(pGib->pev) ));
	if (! pstudiohdr)
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);
	//ALERT(at_console, "read %d bodyparts, canonical is %d\n", pbodypart->nummodels, HUMAN_GIB_COUNT);

	for (int cSplat = 0 ; cSplat < cGibs ; cSplat++ )
	{
		if (pGib == NULL) // first time through, we set pGib before the loop started
		{
			pGib = GetClassPtr( (CGib *)NULL );
			pGib->Spawn( szGibModel );
		}

		if (notfirst)
			pGib->pev->body = RANDOM_LONG(1, pbodypart->nummodels - 1);// start at one to avoid throwing random amounts of skulls (0th gib)
		else
			pGib->pev->body = RANDOM_LONG(0, pbodypart->nummodels - 1);

		if ( pevVictim )
		{
			// spawn the gib somewhere in the monster's bounding volume
			pGib->pev->origin.x = pevVictim->absmin.x + pevVictim->size.x * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.y = pevVictim->absmin.y + pevVictim->size.y * (RANDOM_FLOAT ( 0 , 1 ) );
			pGib->pev->origin.z = pevVictim->absmin.z + pevVictim->size.z * (RANDOM_FLOAT ( 0 , 1 ) ) + 1;	// absmin.z is in the floor because the engine subtracts 1 to enlarge the box

			// make the gib fly away from the attack vector
			pGib->pev->velocity = g_vecAttackDir * -1;

			// mix in some noise
			pGib->pev->velocity.x += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.y += RANDOM_FLOAT ( -0.25, 0.25 );
			pGib->pev->velocity.z += RANDOM_FLOAT ( -0.25, 0.25 );

			pGib->pev->velocity = pGib->pev->velocity * RANDOM_FLOAT ( 300, 400 );

			pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
			pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

			// copy owner's blood color
			pGib->m_bloodColor = (CBaseEntity::Instance(pevVictim))->BloodColor();

			if ( pevVictim->health > -50)
			{
				pGib->pev->velocity = pGib->pev->velocity * 0.7;
			}
			else if ( pevVictim->health > -200)
			{
				pGib->pev->velocity = pGib->pev->velocity * 2;
			}
			else
			{
				pGib->pev->velocity = pGib->pev->velocity * 4;
			}

			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector( 0 , 0 , 0 ), Vector ( 0, 0, 0 ) );
		}
		pGib->LimitVelocity();
		pGib = NULL; //LRC
	}
}

//LRC - work out gibs from blood colour, instead of from class.

BOOL CBaseMonster :: HasHumanGibs( void )
{
	int myClass = Classify();

	// these types of monster don't use gibs
	if ( myClass == CLASS_NONE || myClass == CLASS_MACHINE ||
		 myClass == CLASS_PLAYER_BIOWEAPON && myClass == CLASS_ALIEN_BIOWEAPON)
	{
		return FALSE;
	}
	else
	{
		return (this->m_bloodColor == BLOOD_COLOR_RED);
	}

//	if ( myClass == CLASS_HUMAN_MILITARY ||
//		 myClass == CLASS_PLAYER_ALLY	||
//		 myClass == CLASS_HUMAN_PASSIVE  ||
//		 myClass == CLASS_PLAYER )
//
//		 return TRUE;
//
//	return FALSE;
}


//LRC - work out gibs from blood colour, instead.
BOOL CBaseMonster :: HasAlienGibs( void )
{
	int myClass = Classify();

	// these types of monster don't use gibs
	if ( myClass == CLASS_NONE || myClass == CLASS_MACHINE ||
		 myClass == CLASS_PLAYER_BIOWEAPON && myClass == CLASS_ALIEN_BIOWEAPON)
	{
		return FALSE;
	}
	else
	{
		return (this->m_bloodColor == BLOOD_COLOR_GREEN);
	}

//	int myClass = Classify();
//
//	if ( myClass == CLASS_ALIEN_MILITARY ||
//		 myClass == CLASS_ALIEN_MONSTER	||
//		 myClass == CLASS_ALIEN_PASSIVE  ||
//		 myClass == CLASS_INSECT  ||
//		 myClass == CLASS_ALIEN_PREDATOR  ||
//		 myClass == CLASS_ALIEN_PREY )
//
//		 return TRUE;
//
//	return FALSE;
}


void CBaseMonster::FadeMonster( void )
{
	StopAnimation();
	pev->velocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
	pev->avelocity = g_vecZero;
	pev->animtime = gpGlobals->time;
	pev->effects |= EF_NOINTERP;
	SUB_StartFadeOut();

	ALERT( at_console, "Fading monster!\n" );
}

//=========================================================
// GibMonster - create some gore and get rid of a monster's
// model.
//=========================================================
void CBaseMonster :: GibMonster( void )
{
	TraceResult	tr;
	BOOL		gibbed = FALSE;
	int			iszCustomGibs;

	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM);

	if ( IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pev );

		if ( pPlayer )
		{
			pPlayer->m_bGibbed = true;
			pPlayer->m_bCantBeRevived = true;
		}

		if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )
		{
			gibbed = TRUE;

			for ( int i = 1; i <= 7; i++ )
			{
				int body = i;
				body--;
				CGib::SpawnGib( pev, "models/simon_gibs.mdl", body );
			}
		}

		return;
	}

	if ( iszCustomGibs = HasCustomGibs() ) //LRC - monster_generic can have a custom gibset
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )
		{
			CGib::SpawnHeadGib( pev, STRING(iszCustomGibs) );
			CGib::SpawnRandomGibs( pev, 4, 1, STRING(iszCustomGibs) );
		}
		gibbed = TRUE;
	}
	// only humans throw skulls !!!UNDONE - eventually monsters will have their own sets of gibs
	else if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") != 0 )// Only the player will ever fail this test
		{
			CGib::SpawnHeadGib( pev );
			CGib::SpawnRandomGibs( pev, 4, 1 );	// throw some human gibs.
		}
		gibbed = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") != 0 )// Should never fail this test, but someone might call it directly
		{
			CGib::SpawnRandomGibs( pev, 4, 0 );	// Throw alien gibs
		}
		gibbed = TRUE;
	}

	if ( !IsPlayer() )
	{
		if ( gibbed )
		{
			// don't remove players!
			SetThink(&CBaseMonster :: SUB_Remove );
			SetNextThink( 0 );
		}
		else
		{
			FadeMonster();
		}
	}
}

//=========================================================
// GetDeathActivity - determines the best type of death
// anim to play.
//=========================================================
Activity CBaseMonster :: GetDeathActivity ( void )
{
	Activity	deathActivity;
	BOOL		fTriedDirection;
	float		flDot;
	TraceResult	tr;
	Vector		vecSrc;

	if ( pev->deadflag != DEAD_NO )
	{
		// don't run this while dying.
		return m_IdealActivity;
	}

	if ( m_bIsBoss )
	{
		m_bIsBoss = false;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *player = UTIL_PlayerByIndex( i );
			if ( player )
			{
				if ( player->IsPlayer() )
				{
					CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );
					if ( pPlayer )
					{
						if ( FStrEq( STRING( pev->targetname ), STRING( pPlayer->m_sBossName ) ) )
						{
							pPlayer->m_iBossCurrentHealth = pev->health;

							if ( pPlayer->m_iBossCurrentHealth < 0 )
								pPlayer->m_iBossCurrentHealth = 0;

							pPlayer->m_sBossName = 0;

							MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pPlayer->pev );
								WRITE_BYTE( 2 );
								WRITE_STRING( STRING( pPlayer->m_sBossBarEmpty ) );
								WRITE_STRING( STRING( pPlayer->m_sBossBarFull ) );
								WRITE_SHORT( pPlayer->m_iBossCurrentHealth );
								WRITE_SHORT( pPlayer->m_iBossMaxHealth );
							MESSAGE_END();

							m_bIsBoss = false;
							pPlayer->m_bBossHealthBar = false;
							pPlayer->m_sBossBarEmpty = 0;
							pPlayer->m_sBossBarFull = 0;
							pPlayer->m_iBossMaxHealth = 0;
							pPlayer->m_iBossCurrentHealth = 0;
							pPlayer->m_sBossName = 0;
						}
					}
				}
			}
		}

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			CBaseEntity *pBossEntity = UTIL_FindEntityByClassname( NULL, "monster_booksimon" );

			if ( pBossEntity && pBossEntity->IsAlive() && pBossEntity->m_bIsBoss )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *player = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( player && player->IsPlayer() )
					{
						CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

						if ( pPlayer )
						{
							pPlayer->m_bBossHealthBar = true;
							pPlayer->m_sBossBarEmpty = MAKE_STRING( "gfx/vgui/boss/simon_health_empty.tga" );
							pPlayer->m_sBossBarFull = MAKE_STRING( "gfx/vgui/boss/simon_health_full.tga" );
							pPlayer->m_iBossMaxHealth = pBossEntity->pev->max_health;
							pPlayer->m_iBossCurrentHealth = pBossEntity->pev->health;
							pPlayer->m_sBossName = pBossEntity->pev->targetname;

							MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pPlayer->pev );
								WRITE_BYTE( 1 );
								WRITE_STRING( STRING( pPlayer->m_sBossBarEmpty ) );
								WRITE_STRING( STRING( pPlayer->m_sBossBarFull ) );
								WRITE_SHORT( pPlayer->m_iBossCurrentHealth );
								WRITE_SHORT( pPlayer->m_iBossMaxHealth );
							MESSAGE_END();
						}
					}
				}
			}
		}
	}

	vecSrc = Center();

	fTriedDirection = FALSE;
	deathActivity = ACT_DIESIMPLE;// in case we can't find any special deaths to do.

	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );

	switch ( m_LastHitGroup )
	{
		// try to pick a region-specific death.
	case HITGROUP_HEAD:
		deathActivity = ACT_DIE_HEADSHOT;
		break;

	case HITGROUP_STOMACH:
		deathActivity = ACT_DIE_GUTSHOT;
		break;

	case HITGROUP_GENERIC:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;

	default:
		// try to pick a death based on attack direction
		fTriedDirection = TRUE;

		if ( flDot > 0.3 )
		{
			deathActivity = ACT_DIEFORWARD;
		}
		else if ( flDot <= -0.3 )
		{
			deathActivity = ACT_DIEBACKWARD;
		}
		break;
	}


	// can we perform the prescribed death?
	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// no! did we fail to perform a directional death?
		if ( fTriedDirection )
		{
			// if yes, we're out of options. Go simple.
			deathActivity = ACT_DIESIMPLE;
		}
		else
		{
			// cannot perform the ideal region-specific death, so try a direction.
			if ( flDot > 0.3 )
			{
				deathActivity = ACT_DIEFORWARD;
			}
			else if ( flDot <= -0.3 )
			{
				deathActivity = ACT_DIEBACKWARD;
			}
		}
	}

	if ( LookupActivity ( deathActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		// if we're still invalid, simple is our only option.
		deathActivity = ACT_DIESIMPLE;
	}

	if ( deathActivity == ACT_DIEFORWARD )
	{
			// make sure there's room to fall forward
			UTIL_TraceHull ( vecSrc, vecSrc + gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( deathActivity == ACT_DIEBACKWARD )
	{
			// make sure there's room to fall backward
			UTIL_TraceHull ( vecSrc, vecSrc - gpGlobals->v_forward * 64, dont_ignore_monsters, head_hull, edict(), &tr );

			if ( tr.flFraction != 1.0 )
			{
				deathActivity = ACT_DIESIMPLE;
			}
	}

	if ( FClassnameIs( edict(), "monster_baby" ) )
	{
		if ( pev->body != 0 )
		{
			deathActivity = ACT_SIGNAL1;
		}
	}

	return deathActivity;
}

//=========================================================
// GetSmallFlinchActivity - determines the best type of flinch
// anim to play.
//=========================================================
Activity CBaseMonster :: GetSmallFlinchActivity ( void )
{
	Activity	flinchActivity;
	BOOL		fTriedDirection;
	float		flDot;

	fTriedDirection = FALSE;
	UTIL_MakeVectors ( pev->angles );
	flDot = DotProduct ( gpGlobals->v_forward, g_vecAttackDir * -1 );

	switch ( m_LastHitGroup )
	{
		// pick a region-specific flinch
	case HITGROUP_HEAD:
		flinchActivity = ACT_FLINCH_HEAD;
		break;
	case HITGROUP_STOMACH:
		flinchActivity = ACT_FLINCH_STOMACH;
		break;
	case HITGROUP_LEFTARM:
	case 10:
	case 11:
	case 18:
		flinchActivity = ACT_FLINCH_LEFTARM;
		break;
	case HITGROUP_RIGHTARM:
	case 12:
	case 13:
	case 19:
		flinchActivity = ACT_FLINCH_RIGHTARM;
		break;
	case HITGROUP_LEFTLEG:
	case 14:
	case 15:
		flinchActivity = ACT_FLINCH_LEFTLEG;
		break;
	case HITGROUP_RIGHTLEG:
	case 16:
	case 17:
		flinchActivity = ACT_FLINCH_RIGHTLEG;
		break;
	case 8:
		flinchActivity = ACT_SMALL_FLINCH;
		break;
	case 9:
		flinchActivity = ACT_SMALL_FLINCH;
		break;
	case HITGROUP_GENERIC:
	default:
		// just get a generic flinch.
		flinchActivity = ACT_SMALL_FLINCH;
		break;
	}


	// do we have a sequence for the ideal activity?
	if ( LookupActivity ( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
	{
		if ( LookupActivity ( ACT_SMALL_FLINCH ) != ACTIVITY_NOT_AVAILABLE )
			flinchActivity = ACT_SMALL_FLINCH;
		else
			return ACT_EXCITED;
	}

	return flinchActivity;
}


void CBaseMonster::BecomeDead( void )
{
	pev->takedamage = DAMAGE_YES;// don't let autoaim aim at corpses.

	// give the corpse half of the monster's original maximum health.
	pev->health = pev->max_health / 2;
	pev->max_health = 5; // max_health now becomes a counter for how many blood decals the corpse can place.

	// make the corpse fly away from the attack vector
	if ( !FClassnameIs( edict(), "monster_spitter" ) )
		pev->movetype = MOVETYPE_TOSS;
	//pev->flags &= ~FL_ONGROUND;

	pev->teleport_time = gpGlobals->time + 20;
	pev->radsuit_finished = 1;
	//pev->origin.z += 2;
	//pev->velocity = g_vecAttackDir * -1;
	//pev->velocity = pev->velocity * RANDOM_FLOAT( 300, 400 );
}


BOOL CBaseMonster::ShouldGibMonster( int iGib )
{
	return FALSE;

/*
	if ( ( iGib == GIB_NORMAL && pev->health < GIB_HEALTH_VALUE ) || ( iGib == GIB_ALWAYS ) )
		return TRUE;

	return FALSE;
*/
}


void CBaseMonster::CallGibMonster( void )
{
	return;

	BOOL fade = FALSE;

	if ( HasHumanGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
			fade = TRUE;
	}
	else if ( HasAlienGibs() )
	{
		if ( CVAR_GET_FLOAT("violence_agibs") == 0 )
			fade = TRUE;
	}

	pev->takedamage = DAMAGE_NO;
	pev->solid = SOLID_NOT;// do something with the body. while monster blows up

	if ( fade )
	{
		FadeMonster();
	}
	else
	{
		pev->effects = EF_NODRAW; // make the model invisible.
		GibMonster();
	}

	pev->deadflag = DEAD_DEAD;
	FCheckAITrigger();

	// don't let the status bar glitch for players.with <0 health.
	if (pev->health < -99)
	{
		pev->health = 0;
	}

	if ( ShouldFadeOnDeath() && !fade )
		UTIL_Remove(this);
}


/*
============
Killed
============
*/
void CBaseMonster :: Killed( entvars_t *pevAttacker, int iGib )
{
	unsigned int	cCount = 0;
	BOOL			fDone = FALSE;

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
		BossDamageCheck();
	}

	//pev->enemy = ENT( pevAttacker );//why? (sjb)

	m_IdealMonsterState = MONSTERSTATE_DEAD;
}

void CBaseEntity :: SUB_StartFadeInAndOut ( void )
{
	pev->renderamt = 2;

	pev->solid = SOLID_NOT;

	SetNextThink( 0.1 );
	SetThink(&CBaseEntity :: SUB_FadeInAndOut );
}

void CBaseEntity :: SUB_FadeInAndOut ( void )
{
	int MaxAmount = 32;
	int FadeAmount = 3;

	if ( pev->renderamt < MaxAmount && !m_bFadeOut )
	{
		pev->renderamt += FadeAmount;
		SetNextThink( 0.1 );
	}


	if ( pev->renderamt >= MaxAmount && !m_bFadeOut )
	{
		m_bFadeOut = true;
		int iNextThink = RANDOM_LONG( 0, 2 );
		SetNextThink( iNextThink );
		return;
	}

	if ( pev->renderamt > 0 && m_bFadeOut == true )
	{
		pev->renderamt -= FadeAmount;
		SetNextThink( 0.1 );
	}

	if ( pev->renderamt <= 7 && m_bFadeOut == true )
	{
		pev->renderamt = 0;
		SetNextThink( 0.2 );
		SetThink(&CBaseEntity :: SUB_Remove );
	}
}

//
// fade out - slowly fades a entity out, then removes it.
//
// DON'T USE ME FOR GIBS AND STUFF IN MULTIPLAYER!
// SET A FUTURE THINK AND A RENDERMODE!!
void CBaseEntity :: SUB_StartFadeOut ( void )
{
	SetNextThink( 0.2 );
	SetThink(&CBaseEntity :: SUB_Remove );

/*
	if (pev->rendermode == kRenderNormal)
	{
		pev->renderamt = 255;
		pev->rendermode = kRenderTransTexture;
	}

	pev->solid = SOLID_NOT;
	pev->avelocity = g_vecZero;

	SetNextThink( 0.1 );
	SetThink(&CBaseEntity :: SUB_FadeOut );
*/
}

void CBaseEntity :: SUB_FadeOut ( void  )
{
	if ( pev->renderamt > 7 )
	{
		pev->renderamt -= 7;
		SetNextThink( 0.1 );
	}
	else
	{
		pev->renderamt = 0;
		SetNextThink( 0.2 );
		SetThink(&CBaseEntity :: SUB_Remove );
	}
}

//=========================================================
// WaitTillLand - in order to emit their meaty scent from
// the proper location, gibs should wait until they stop
// bouncing to emit their scent. That's what this function
// does.
//=========================================================
void CGib :: WaitTillLand ( void )
{
	if (!IsInWorld())
	{
		UTIL_Remove( this );
		return;
	}

	if ( pev->velocity == g_vecZero )
	{
		SetThink(&CGib ::SUB_StartFadeOut);
		SetNextThink( m_lifeTime );

		// If you bleed, you stink!
		if ( m_bloodColor != DONT_BLEED )
		{
			// ok, start stinkin!
			CSoundEnt::InsertSound ( bits_SOUND_MEAT, pev->origin, 384, 25 );
		}
	}
	else
	{
		// wait and check again in another half second.
		SetNextThink( 0.5 );
	}
}

//
// Gib bounces on the ground or wall, sponges some blood down, too!
//
void CGib :: BounceGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;

	//if ( RANDOM_LONG(0,1) )
	//	return;// don't bleed everytime

	if (pev->flags & FL_ONGROUND)
	{
		pev->velocity = pev->velocity * 0.9;
		pev->angles.x = 0;
		pev->angles.z = 0;
		pev->avelocity.x = 0;
		pev->avelocity.z = 0;
	}
	else
	{
		if ( g_Language != LANGUAGE_GERMAN && m_cBloodDecals > 0 && m_bloodColor != DONT_BLEED )
		{
			vecSpot = pev->origin + Vector ( 0 , 0 , 8 );//move up a bit, and trace down.
			UTIL_TraceLine ( vecSpot, vecSpot + Vector ( 0, 0, -24 ),  ignore_monsters, ENT(pev), & tr);

			UTIL_BloodDecalTrace( &tr, m_bloodColor );

			m_cBloodDecals--;
		}

		if ( m_material != matNone && RANDOM_LONG(0,2) == 0 )
		{
			float volume;
			float zvel = fabs(pev->velocity.z);

			volume = 0.8 * min(1.0, ((float)zvel) / 450.0);

			CBreakable::MaterialSoundRandom( edict(), (Materials)m_material, volume );
		}
	}
}

//
// Sticky gib puts blood on the wall and stays put.
//
void CGib :: StickyGibTouch ( CBaseEntity *pOther )
{
	Vector	vecSpot;
	TraceResult	tr;

	SetThink(&CGib :: SUB_Remove );
	SetNextThink( 10 );

	if ( !FClassnameIs( pOther->pev, "worldspawn" ) )
	{
		SetNextThink( 0 );
		return;
	}

	UTIL_TraceLine ( pev->origin, pev->origin + pev->velocity * 32,  ignore_monsters, ENT(pev), & tr);

	UTIL_BloodDecalTrace( &tr, m_bloodColor );

	pev->velocity = tr.vecPlaneNormal * -1;
	pev->angles = UTIL_VecToAngles ( pev->velocity );
	pev->velocity = g_vecZero;
	pev->avelocity = g_vecZero;
	pev->movetype = MOVETYPE_NONE;
}

//
// Throw a chunk
//
void CGib :: Spawn( const char *szGibModel )
{
	pev->movetype = MOVETYPE_BOUNCE;
	pev->friction = 0.55; // deading the bounce a bit

	// sometimes an entity inherits the edict from a former piece of glass,
	// and will spawn using the same render FX or rendermode! bad!
	pev->renderamt = 255;
	pev->rendermode = kRenderNormal;
	pev->renderfx = kRenderFxNone;
	pev->solid = SOLID_TRIGGER; //LRC - so that they don't get in each other's way when we fire lots
//	pev->solid = SOLID_SLIDEBOX;/// hopefully this will fix the VELOCITY TOO LOW crap
	pev->classname = MAKE_STRING("gib");

	SET_MODEL(ENT(pev), szGibModel);
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));

	SetNextThink( 4 );
	m_lifeTime = 25;
	SetThink(&CGib :: WaitTillLand );
	SetTouch(&CGib :: BounceGibTouch );

	m_material = matNone;
	m_cBloodDecals = 5;// how many blood decals this gib can place (1 per bounce until none remain).
}

// take health
int CBaseMonster :: TakeHealth (float flHealth, int bitsDamageType)
{
	if (!pev->takedamage)
		return 0;

	// clear out any damage types we healed.
	// UNDONE: generic health should not heal any
	// UNDONE: time-based damage

	m_bitsDamageType &= ~(bitsDamageType & ~DMG_TIMEBASED);

	return CBaseEntity::TakeHealth(flHealth, bitsDamageType);
}

void CBaseMonster :: KillRemove( void )
{
	pev->health = 0;
	Killed( pev, GIB_NEVER );
}

/*
============
TakeDamage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
bitsDamageType indicates the type of damage sustained, ie: DMG_SHOCK

Time-based damage: only occurs while the monster is within the trigger_hurt.
When a monster is poisoned via an arrow etc it takes all the poison damage at once.



GLOBALS ASSUMED SET:  g_iSkillLevel
============
*/
int CBaseMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	float	flTake;
	Vector	vecDir;

	if (!pev->takedamage)
		return 0;

	if ( !IsAlive() )
	{
		return DeadTakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
	}

	if ( pev->deadflag == DEAD_NO )
	{
		// no pain sound during death animation.
		if ( !m_bWaitingToDie )
			PainSound();// "Ouch!"
	}

	if ( (pev->flags & FL_MONSTER) && IsAlive() && m_pCine == NULL && (bitsDamageType & (1 << 24)) && pev->health > flDamage && !m_bIsBoss )
	{
		if ( pev->deadflag == DEAD_NO )
		{
			Stop();
			RouteClear();

			ChangeSchedule( GetScheduleOfType( SCHED_ALERT_SMALL_FLINCH ) );
		}
	}

	if ( bitsDamageType & (1 << 28) )
	{
		if ( !m_bFrozenByCamera && (pev->flags & FL_MONSTER) && pev->takedamage == DAMAGE_AIM && IsAlive() && m_pCine == NULL && pev->health > flDamage && !m_bIsBoss )
		{
			if ( pev->deadflag == DEAD_NO )
			{
				m_bFrozenByCamera = true;
				pev->animtime = 0;
				pev->framerate = 0;
				SetThink( NULL );
			}
		}

		return 0;
	}

	//!!!LATER - make armor consideration here!
	flTake = flDamage;

	m_iLastDamageType = bitsDamageType;

	// set damage type sustained
	m_bitsDamageType |= bitsDamageType;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if (!FNullEnt( pevInflictor ))
	{
		CBaseEntity *pInflictor = CBaseEntity :: Instance( pevInflictor );
		if (pInflictor)
		{
			if ( pInflictor->IsPlayer() )
				GetClassPtr( (CBasePlayer *)pInflictor->pev )->m_pLastAttackedEnemy = this;

			vecDir = ( pInflictor->Center() - Vector ( 0, 0, 10 ) - Center() ).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// todo: remove after combining shotgun blasts?
	if ( IsPlayer() )
	{
		if ( pevInflictor )
			pev->dmg_inflictor = ENT(pevInflictor);

		pev->dmg_take += flTake;

		// check for godmode or invincibility
		if ( pev->flags & FL_GODMODE )
		{
			return 0;
		}

		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pev );

		if ( pPlayer && pPlayer->m_bInfiniteHealth )
			return 0;
	}

	// if this is a player, move him around!
	if ( ( !FNullEnt( pevInflictor ) ) && (pev->movetype == MOVETYPE_WALK) && (!pevAttacker || pevAttacker->solid != SOLID_TRIGGER) )
	{
		pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage );
	}

	// do the damage
	pev->health -= flTake;


	// HACKHACK Don't kill monsters in a script.  Let them break their scripts first
	if ( m_MonsterState == MONSTERSTATE_SCRIPT )
	{
		SetConditions( bits_COND_LIGHT_DAMAGE );
		return 0;
	}

	BossDamageCheck();

	if ( pev->health <= 0 )
	{
		g_pevLastInflictor = pevInflictor;

		if ( m_bFrozenByCamera )
		{
			m_bFrozenByCamera = false;
			SetThink( &CBaseMonster::CallMonsterThink );
			SetNextThink( 0 );
		}

		if ( bitsDamageType & DMG_ALWAYSGIB )
		{
			Killed( pevAttacker, GIB_ALWAYS );
		}
		// Wargon: Никогда не гибать если тип дамаги - DMG_SLASH. (1.1)
		else if ((bitsDamageType & DMG_NEVERGIB) || (bitsDamageType & DMG_SLASH))
		{
			Killed( pevAttacker, GIB_NEVER );
		}
		else
		{
			Killed( pevAttacker, GIB_NORMAL );
		}

		g_pevLastInflictor = NULL;

		return 0;
	}

	// react to the damage (get mad)
	if ( (pev->flags & FL_MONSTER) && !FNullEnt(pevAttacker) )
	{
		//LRC - new behaviours, for m_iPlayerReact.
		if (pevAttacker->flags & FL_CLIENT)
		{
			if (m_iPlayerReact == 2)
			{
				// just get angry.
				Remember( bits_MEMORY_PROVOKED );
			}
			else if (m_iPlayerReact == 3)
			{
				// try to decide whether it was deliberate... if I have an enemy, assume it was just crossfire.
				if ( m_hEnemy == NULL )
				{
					if ( (m_afMemory & bits_MEMORY_SUSPICIOUS) || UTIL_IsFacing( pevAttacker, pev->origin ) )
						Remember( bits_MEMORY_PROVOKED );
					else
						Remember( bits_MEMORY_SUSPICIOUS );
				}
			}
		}

		if ( pevAttacker->flags & (FL_MONSTER | FL_CLIENT) )
		{// only if the attack was a monster or client!

			// enemy's last known position is somewhere down the vector that the attack came from.
			if (pevInflictor)
			{
				if (m_hEnemy == NULL || pevInflictor == m_hEnemy->pev || !HasConditions(bits_COND_SEE_ENEMY))
				{
					m_vecEnemyLKP = pevInflictor->origin;
				}
			}
			else
			{
				m_vecEnemyLKP = pev->origin + ( g_vecAttackDir * 64 );
			}

			MakeIdealYaw( m_vecEnemyLKP );

			// add pain to the conditions
			// !!!HACKHACK - fudged for now. Do we want to have a virtual function to determine what is light and
			// heavy damage per monster class?
			if ( flDamage > 0 )
			{
				SetConditions(bits_COND_LIGHT_DAMAGE);
			}

			if ( flDamage >= 20 )
			{
				SetConditions(bits_COND_HEAVY_DAMAGE);
			}
		}
	}

	if ( (bitsDamageType & DMG_BURN) && (pev->flags & FL_MONSTER) && ( pev->renderfx <= 0 || ( pev->renderfx >= 30 && pev->renderfx <= 40 ) ) )
	{
		if ( pev->renderfx < 30 )
			pev->renderfx = 30;

		pev->renderfx++;

		if ( pev->vuser1.x > 40 )
			pev->vuser1.x = 40;
	}

	return 1;
}

//=========================================================
// DeadTakeDamage - takedamage function called when a monster's
// corpse is damaged.
//=========================================================
int CBaseMonster :: DeadTakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	Vector			vecDir;

	// grab the vector of the incoming attack. ( pretend that the inflictor is a little lower than it really is, so the body will tend to fly upward a bit).
	vecDir = Vector( 0, 0, 0 );
	if (!FNullEnt( pevInflictor ))
	{
		CBaseEntity *pInflictor = CBaseEntity :: Instance( pevInflictor );
		if (pInflictor)
		{
			vecDir = ( pInflictor->Center() - Vector ( 0, 0, 10 ) - Center() ).Normalize();
			vecDir = g_vecAttackDir = vecDir.Normalize();
		}
	}

#if 0// turn this back on when the bounding box issues are resolved.

	pev->flags &= ~FL_ONGROUND;
	pev->origin.z += 1;

	// let the damage scoot the corpse around a bit.
	if ( !FNullEnt(pevInflictor) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		pev->velocity = pev->velocity + vecDir * -DamageForce( flDamage );
	}

#endif
	// kill the corpse if enough damage was done to destroy the corpse and the damage is of a type that is allowed to destroy the corpse.
	if ( bitsDamageType & DMG_GIB_CORPSE )
	{
		if ( pev->health <= flDamage )
		{
			pev->health = -50;
			Killed( pevAttacker, GIB_ALWAYS );
			return 0;
		}
		// Accumulate corpse gibbing damage, so you can gib with multiple hits
		pev->health -= flDamage * 0.1;
	}

	return 1;
}


float CBaseMonster :: DamageForce( float damage )
{
	float force = damage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;

	if ( force > 1000.0)
	{
		force = 1000.0;
	}

	return force;
}


//
// RadiusDamage - this entity is exploding, or otherwise needs to inflict damage upon entities within a certain range.
//
// only damage ents that can clearly be seen by the explosion!

void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType )
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage, falloff;
	Vector		vecSpot;

	if ( flRadius )
		falloff = flDamage / flRadius;
	else
		falloff = 1.0;

	int bInWater = (UTIL_PointContents ( vecSrc ) == CONTENTS_WATER);

	vecSrc.z += 1;// in case grenade is lying on the ground

	if ( !pevAttacker )
		pevAttacker = pevInflictor;

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere( pEntity, vecSrc, flRadius )) != NULL)
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			// buz: skip grenade damage from player's grenades to invincible monsters
			if ( pEntity->pev->spawnflags & SF_MONSTER_INVINCIBLE )
			{
				CBaseEntity *pEnt = CBaseEntity::Instance( pevAttacker );
				if (pEnt->IsPlayer())
					continue;
			}

			// UNDONE: this should check a damage mask, not an ignore
			if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			// blast's don't tavel into or out of water
			if (bInWater && pEntity->pev->waterlevel == 0)
				continue;
			if (!bInWater && pEntity->pev->waterlevel == 3)
				continue;

			vecSpot = pEntity->BodyTarget( vecSrc );

			UTIL_TraceLine ( vecSrc, vecSpot, dont_ignore_monsters, ENT(pevInflictor), &tr );

			if ( tr.flFraction == 1.0 || tr.pHit == pEntity->edict() )
			{// the explosion can 'see' this entity, so hurt them!
				if (tr.fStartSolid)
				{
					// if we're stuck inside them, fixup the position and distance
					tr.vecEndPos = vecSrc;
					tr.flFraction = 0.0;
				}

				// decrease damage for an ent that's farther from the bomb.
				flAdjustedDamage = ( vecSrc - tr.vecEndPos ).Length() * falloff;
				flAdjustedDamage = flDamage - flAdjustedDamage;

				if ( flAdjustedDamage < 0 )
				{
					flAdjustedDamage = 0;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage( );
					pEntity->TraceAttack( pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize( ), &tr, bitsDamageType );
					ApplyMultiDamage( pevInflictor, pevAttacker );
				}
				else
				{
					pEntity->TakeDamage ( pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType );
				}
			}
		}
	}
}


void CBaseMonster :: RadiusDamage(entvars_t* pevInflictor, entvars_t*	pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( pev->origin, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


void CBaseMonster :: RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType )
{
	::RadiusDamage( vecSrc, pevInflictor, pevAttacker, flDamage, flDamage * 2.5, iClassIgnore, bitsDamageType );
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
//
// Used for many contact-range melee attacks. Bites, claws, etc.
//=========================================================
CBaseEntity* CBaseMonster :: CheckTraceHullAttack( float flDist, int iDamage, int iDmgType, bool playersonly, int positionoffset )
{
	TraceResult tr;

	if (IsPlayer())
		UTIL_MakeVectors( pev->angles );
	else
		UTIL_MakeAimVectors( pev->angles );

	Vector vecStart = pev->origin;
	vecStart.z += pev->size.z * 0.5;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * flDist );

	UTIL_TraceHull( vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pCast = UTIL_PlayerByIndex( i );

			if ( pCast && pCast->IsAlive() )
			{
				TraceResult trace;
				UTIL_TraceModel( vecStart, vecEnd, head_hull, ENT(pCast->pev), &trace );

				if ( trace.pHit )
					tr.pHit = trace.pHit;
			}
		}
	}

	if ( tr.pHit )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( playersonly && !pEntity->IsPlayer() )
			return NULL;

		if ( iDamage > 0 )
		{
			pEntity->TakeDamage( pev, pev, iDamage, iDmgType );
		}

		return pEntity;
	}

	return NULL;
}


//=========================================================
// FInViewCone - returns true is the passed ent is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall.
//=========================================================
BOOL CBaseMonster :: FInViewCone ( CBaseEntity *pEntity )
{
	Vector2D	vec2LOS;
	float	flDot;

	UTIL_MakeVectors ( pev->angles );

	vec2LOS = ( pEntity->pev->origin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FInViewCone - returns true is the passed vector is in
// the caller's forward view cone. The dot product is performed
// in 2d, making the view cone infinitely tall.
//=========================================================
BOOL CBaseMonster :: FInViewCone ( Vector *pOrigin )
{
	Vector2D	vec2LOS;
	float		flDot;

	UTIL_MakeVectors ( pev->angles );

	vec2LOS = ( *pOrigin - pev->origin ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	flDot = DotProduct (vec2LOS , gpGlobals->v_forward.Make2D() );

	if ( flDot > m_flFieldOfView )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target
//=========================================================
BOOL CBaseEntity :: FVisible ( CBaseEntity *pEntity )
{
	TraceResult tr;
	Vector		vecLookerOrigin;
	Vector		vecTargetOrigin;

	if ( FBitSet( pEntity->pev->flags, FL_NOTARGET ) && fvisiblecheck )
	{
		ALERT( at_console, "FVisible FAIL - FL_NOTARGET\n" );
		return FALSE;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pEntity->IsPlayer() && pEntity->pev->fuser3 == 3 )
	{
		if ( fvisiblecheck )
			ALERT( at_console, "FVisible FAIL - fuser3\n" );
		return FALSE;
	}

	// don't look through water
	if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3)
		|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
	{
		if ( fvisiblecheck )
			ALERT( at_console, "FVisible FAIL - waterlevel\n" );
		return FALSE;
	}

	vecLookerOrigin = pev->origin + pev->view_ofs;//look through the caller's 'eyes'
	vecTargetOrigin = pEntity->EyePosition();

	UTIL_TraceLine(vecLookerOrigin, vecTargetOrigin, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr);

	if (tr.flFraction != 1.0 && tr.pHit != ENT(pEntity->pev)) //LRC - added so that monsters can "see" some bsp objects
	{
	//	ALERT(at_console, "can't see \"%s\"\n", STRING(pEntity->pev->classname));
		if ( fvisiblecheck )
			ALERT( at_console, "FVisible FAIL - LOS\n" );
		return FALSE;// Line of sight is not established
	}
	else
	{
	//	ALERT(at_console, "Seen ok\n");
		return TRUE;// line of sight is valid.
	}
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target vector
//=========================================================
BOOL CBaseEntity :: FVisible ( const Vector &vecOrigin )
{
	TraceResult tr;
	Vector		vecLookerOrigin;

	vecLookerOrigin = EyePosition();//look through the caller's 'eyes'

	UTIL_TraceLine(vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, ENT(pev)/*pentIgnore*/, &tr);

	if (tr.flFraction != 1.0)
	{
		return FALSE;// Line of sight is not established
	}
	else
	{
		return TRUE;// line of sight is valid.
	}
}

/*
================
TraceAttack
================
*/
void CBaseEntity::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();

		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage, pev->velocity);// a little surface blood.
			TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}
}


//=========================================================
// GoreCalculation - CoF: monster_slower and monster_zombie
// lose body parts, kept as bits of pev->playerclass
//=========================================================
void CBaseMonster :: GoreCalculation( int hitbox, float flDamage )
{
	if ( FClassnameIs( edict(), "monster_slower" ) || FClassnameIs( edict(), "monster_zombie" ) )
	{
		switch ( hitbox )
		{
		case HITGROUP_HEAD:
			if ( !(pev->playerclass & 1) )
			{
				pev->playerclass |= 1;
				ALERT( at_console, "Setting GOREFLAG_LOST_HEAD\n" );
			}
			break;
		case 18:
			if ( !(pev->playerclass & 2) )
			{
				pev->playerclass |= 2;
				ALERT( at_console, "Setting GOREFLAG_LOST_LEFTHAND\n" );
			}
			break;
		case 19:
			if ( !(pev->playerclass & 4) )
			{
				pev->playerclass |= 4;
				ALERT( at_console, "Setting GOREFLAG_LOST_RIGHTHAND\n" );
			}
			break;
		case 10:
			if ( !(pev->playerclass & 8) )
			{
				pev->playerclass |= 2;
				pev->playerclass |= 8;
				ALERT( at_console, "Setting GOREFLAG_LOST_LOWER_LEFTARM\n" );
			}
			break;
		case 11:
			if ( !(pev->playerclass & 16) )
			{
				pev->playerclass |= 2;
				pev->playerclass |= 8;
				pev->playerclass |= 16;
				ALERT( at_console, "Setting GOREFLAG_LOST_UPPER_LEFTARM\n" );
			}
			break;
		case 12:
			if ( !(pev->playerclass & 12) )
			{
				pev->playerclass |= 4;
				pev->playerclass |= 32;
				ALERT( at_console, "Setting HITGROUP_LOWER_RIGHTARM\n" );
			}
			break;
		case 13:
			if ( !(pev->playerclass & 64) )
			{
				pev->playerclass |= 4;
				pev->playerclass |= 32;
				pev->playerclass |= 64;
				ALERT( at_console, "Setting GOREFLAG_LOST_UPPER_RIGHTARM\n" );
			}
			break;
		case 14:
			if ( !(pev->playerclass & 128) )
			{
				pev->playerclass |= 128;
				ALERT( at_console, "Setting GOREFLAG_LOST_LOWER_LEFTLEG\n" );
			}
			break;
		case 15:
			if ( !(pev->playerclass & 256) )
			{
				pev->playerclass |= 256;
				pev->playerclass |= 128;
				ALERT( at_console, "Setting GOREFLAG_LOST_UPPER_LEFTLEG\n" );
			}
			break;
		case 16:
			if ( !(pev->playerclass & 512) )
			{
				pev->playerclass |= 512;
				ALERT( at_console, "Setting HITGROUP_LOWER_RIGHTLEG\n" );
			}
			break;
		case 17:
			if ( !(pev->playerclass & 1024) )
			{
				pev->playerclass |= 1024;
				pev->playerclass |= 512;
				ALERT( at_console, "Setting HITGROUP_UPPER_RIGHTLEG\n" );
			}
			break;
		}
	}
}

//=========================================================
// TraceAttack
//=========================================================
void CBaseMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( pev->takedamage )
	{
		if ( pev->spawnflags & SF_MONSTER_INVINCIBLE )
		{
		//	ALERT(at_console, "yes im incvincible\n");
			// check for player
			CBaseEntity *pEnt = CBaseEntity::Instance( pevAttacker );
			if (pEnt->IsPlayer())
			{
			//	ALERT(at_console, "ent is player\n");
				return;
			}

			// check for owner (it may be grenade, thrown by player)
			if (pevAttacker->owner)
			{
				pEnt = CBaseEntity::Instance( pevAttacker->owner );
				if (pEnt->IsPlayer())
				{
				//	ALERT(at_console, "owner is player\n");
					return;
				}
			}
		}

		m_LastHitGroup = ptr->iHitgroup;
		TraceBleed( flDamage, vecDir, ptr, bitsDamageType );

		TraceResult btr; // Wargon: Переменная для трейса декали мозгов от хедшотов.

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			// Wargon: Декаль мозгов от хедшотов.
			UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecDir * 172, ignore_monsters, ENT(pev), &btr );
			UTIL_CustomDecal( &btr, "brains" );

			// buz: more blood in the head!!! let the brains fly out and smash on ground, muahahaha!!
			SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage * 4, pev->velocity);

			if ( bitsDamageType & (1 << 25) )
				flDamage *= gSkillData.monHead * 2;
			else
				flDamage *= gSkillData.monHead;

			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.monChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.monStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
		case 10:
		case 11:
		case 12:
		case 13:
		case 18:
		case 19:
			flDamage *= gSkillData.monArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
		case 14:
		case 15:
		case 16:
		case 17:
			flDamage *= gSkillData.monLeg;
			break;
		case 8:
			flDamage = 0;
			break;
		case 9:
			flDamage = gSkillData.monHead;
			break;
		default:
			break;
		}

		GoreCalculation( ptr->iHitgroup, flDamage );

		SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage * 2, pev->velocity); // Wargon: Побольше крови.
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.

This version is used by Monsters.
================
*/
extern unsigned short m_usSmokePuff; // buz - smoke event

void CBaseEntity::FireBullets(ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker )
{
	static int tracerCount;
	int tracer;
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	CBasePlayer *pPlayer = NULL;

	if ( IsPlayer() )
	{
		pPlayer = GetClassPtr( (CBasePlayer *)pev );

		if ( pPlayer )
		{
			if ( pPlayer->m_pActiveItem && ((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_bIronSights )
			{
				vecSpread.x /= 2;
				vecSpread.y /= 2;
				vecSpread.z /= 2;
			}

			if ( pPlayer && iBulletType != 50 )
				pPlayer->playerstats.shots_fired++;
		}
	}

	if ( pevAttacker == NULL )
		pevAttacker = pev;  // the default attacker is ourselves

	ClearMultiDamage();

	if ( iBulletType == BULLET_PLAYER_CS )
		gMultiDamage.type = DMG_NEVERGIB | (1 << 25);
	else
		gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for (ULONG iShot = 1; iShot <= cShots; iShot++)
	{
		// get circular gaussian spread
		float x, y, z;
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		Vector vecEnd;

		vecEnd = vecSrc + vecDir * flDistance;
		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);

		tracer = 0;
		if (iTracerFreq != 0 && (tracerCount++ % iTracerFreq) == 0)
		{
			Vector vecTracerSrc;

			if ( IsPlayer() )
			{// adjust tracer position for player
				vecTracerSrc = vecSrc + Vector ( 0 , 0 , -4 ) + gpGlobals->v_right * 2 + gpGlobals->v_forward * 16;
			}
			else
			{
				vecTracerSrc = vecSrc;
			}

			if ( iTracerFreq != 1 )		// guns that always trace also always decal
				tracer = 1;
			switch( iBulletType )
			{
			case BULLET_MONSTER_MP5:
			case BULLET_MONSTER_9MM:
			case BULLET_MONSTER_12MM:
			default:
				MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, vecTracerSrc );
					WRITE_BYTE( TE_TRACER );
					WRITE_COORD( vecTracerSrc.x );
					WRITE_COORD( vecTracerSrc.y );
					WRITE_COORD( vecTracerSrc.z );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
				MESSAGE_END();
				break;
			}
		}
		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			char textype = CHAR_TEX_CONCRETE;
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( iDamage )
			{
				if ( iBulletType == BULLET_PLAYER_CS )
					pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_NEVERGIB | (1 << 25) );
				else
					pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB );

			//	TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
			//	DecalGunshot( &tr, iBulletType );
			}
			else switch(iBulletType)
			{
			default:
			case BULLET_NONE: // FIX
				pEntity->TraceAttack(pevAttacker, 50, vecDir, &tr, DMG_CLUB);
			//	TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				// only decal glass
				if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
				{
					UTIL_DecalTrace( &tr, DECAL_GLASSBREAK1 + RANDOM_LONG(0,2) );
				}

				break;
			}

			// buz: invoke smoke puff on client side
			if (VARS(tr.pHit)->solid == SOLID_BSP && !tr.fInWater)
			{
				switch (textype)
				{
				case CHAR_TEX_GRASS:
				case CHAR_TEX_DIRT:
				case CHAR_TEX_GRATE:
				case CHAR_TEX_SLOSH:
				case CHAR_TEX_GLASS:
				case CHAR_TEX_COMPUTER:
				case CHAR_TEX_FLESH:
					break;

				default:
					{
					//	Vector vecSend = tr.vecPlaneNormal * 2048; //buz: client side will normalize it again
					//	PLAYBACK_EVENT_FULL( 0, ENT(pevAttacker), m_usSmokePuff, 0, tr.vecEndPos, vecSend, 0, 0, 0, 0, 0, 0 );
					}
				}
			}
		}
		// make bullet trails
		UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (flDistance * tr.flFraction) / 64.0 );
	}

	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

	if ( !pHit || pHit->IsBSPModel() )
	{
		if ( pHit->IsBSPModel() )
		{
			if ( pPlayer && iBulletType != 50 )
			{
				if ( !FClassnameIs( pHit->edict(), "func_breakable" ) && !FClassnameIs( pHit->edict(), "cof_doctorshoot" ) )
				{
					pPlayer->playerstats.shots_missed++;
				}
				else
				{
					if ( FClassnameIs( pHit->edict(), "cof_doctorshoot" ) )
					{
						bool hit = false;

						CDoctorBoss *pDoctor = NULL;
						CBaseEntity *pFind = UTIL_FindEntityByClassname( NULL, "monster_doctorboss" );

						if ( pFind )
						{
							pDoctor = GetClassPtr( (CDoctorBoss *)pFind->pev );

							if ( pDoctor )
							{
								switch ( pHit->pev->iuser1 )
								{
								case 1:
									if ( pDoctor->IsAimingOrShooting() == 1 || pDoctor->IsAimingOrShooting() == 5 )
										hit = true;
									break;
								case 2:
									if ( pDoctor->IsAimingOrShooting() == 2 || pDoctor->IsAimingOrShooting() == 6 )
										hit = true;
									break;
								case 3:
									if ( pDoctor->IsAimingOrShooting() == 3 || pDoctor->IsAimingOrShooting() == 7 )
										hit = true;
									break;
								case 4:
									if ( pDoctor->IsAimingOrShooting() == 4 || pDoctor->IsAimingOrShooting() == 8 )
										hit = true;
									break;
								}
							}
						}

						if ( !hit )
							pPlayer->playerstats.shots_missed++;
						else
							pPlayer->playerstats.shots_hit++;
					}
					else
						pPlayer->playerstats.shots_hit++;
				}
			}
		}
		else
		{
			if ( pPlayer && iBulletType != 50 )
				pPlayer->playerstats.shots_missed++;
		}
	}
	else
	{
		if ( pPlayer && iBulletType != 50 )
		{
			if ( pPlayer->IRelationship( pHit ) <= R_NO )
				pPlayer->playerstats.shots_missed++;
			else
				pPlayer->playerstats.shots_hit++;
		}
	}

	ApplyMultiDamage(pev, pevAttacker);
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.

This version is used by Players, uses the random seed generator to sync client and server side shots.
================
*/
Vector CBaseEntity::FireBulletsPlayer ( ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker, int shared_rand )
{
	CBasePlayer *pPlayer = NULL;

	if ( IsPlayer() )
		pPlayer = GetClassPtr( (CBasePlayer *)pev );

	TraceResult tr;
	Vector vecRight;
	Vector vecUp;

	vecRight = gpGlobals->v_right;
	vecUp = gpGlobals->v_up;
	float x, y, z;

	if ( IsPlayer() )
	{
		if ( pPlayer )
		{
			if ( pPlayer->m_pActiveItem && ((CBasePlayerWeapon *)pPlayer->m_pActiveItem)->m_bIronSights )
			{
				vecSpread.x /= 2;
				vecSpread.y /= 2;
				vecSpread.z /= 2;
			}

			if ( pPlayer && iBulletType != 50 )
				pPlayer->playerstats.shots_fired++;
		}
	}

	if ( pevAttacker == NULL )
		pevAttacker = pev;  // the default attacker is ourselves

	ClearMultiDamage();

	if ( iBulletType == BULLET_PLAYER_CS )
		gMultiDamage.type = DMG_NEVERGIB | (1 << 25);
	else
		gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	if ( iBulletType == 50 )
		gMultiDamage.type = DMG_NEVERGIB | DMG_BURN;

	if ( iBulletType == 51 )
		gMultiDamage.type = DMG_NEVERGIB | (1 << 28);

	bool hit = false;
	CBasePlayerWeapon *pBrowning = NULL;

	for ( ULONG iShot = 1; iShot <= cShots; iShot++ )
	{
		//Use player's random seed.
		// get circular gaussian spread
		x = UTIL_SharedRandomFloat( shared_rand + iShot, -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 1 + iShot ) , -0.5, 0.5 );
		y = UTIL_SharedRandomFloat( shared_rand + ( 2 + iShot ), -0.5, 0.5 ) + UTIL_SharedRandomFloat( shared_rand + ( 3 + iShot ), -0.5, 0.5 );
		z = x * x + y * y;

		if ( pPlayer && pPlayer->m_pWheelchair && pPlayer->m_pActiveItem )
		{
			pBrowning = GetClassPtr( (CBasePlayerWeapon *)pPlayer->m_pActiveItem->pev );

			if ( pBrowning )
			{
				int x = atoi( CMD_ARGV( 1 ) );
				int y = atoi( CMD_ARGV( 2 ) );
				int z = atoi( CMD_ARGV( 3 ) );

				flDistance = 0;
				vecDirShooting = GetClassPtr( (CBrowningWheelchair *)pPlayer->m_pActiveItem->pev )->m_vEndVector;
			}
		}

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		Vector vecEnd;

		vecEnd = vecSrc + vecDir * flDistance;

		if ( pBrowning && pPlayer && pPlayer->m_pWheelchair )
			vecEnd = GetClassPtr( (CBrowningWheelchair *)pPlayer->m_pActiveItem->pev )->m_vEndVector;

		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);

		if ( IsPlayer() || iBulletType == BULLET_PLAYER_RIFLE )
		{
			if ( tr.flFraction != 1.0 )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if ( pEntity && ( pEntity->IsPlayer() || iBulletType == BULLET_PLAYER_RIFLE ) )
				{
					if ( iBulletType == BULLET_PLAYER_RIFLE && !pEntity->IsPlayer() )
					{
						if ( pEntity->MyMonsterPointer() )
						{
							UTIL_TraceLine( tr.vecEndPos, vecEnd, dont_ignore_monsters, ENT(pEntity->pev), &tr );

							if ( pEntity->MyMonsterPointer() )
								pEntity->TakeDamage( pev, pevAttacker, iDamage, gMultiDamage.type );
						}
					}
					else
					{
						UTIL_TraceLine( tr.vecEndPos, vecEnd, dont_ignore_monsters, ENT(pEntity->pev), &tr );

						if ( pEntity->MyMonsterPointer() )
							pEntity->TakeDamage( pev, pevAttacker, iDamage, gMultiDamage.type );
					}
				}
			}

			if ( tr.flFraction != 1.0 )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if ( pEntity && pEntity->IsPlayer() )
				{
					UTIL_TraceLine( tr.vecEndPos, vecEnd, dont_ignore_monsters, ENT(pEntity->pev), &tr );
				}
			}

			if ( tr.flFraction != 1.0 )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

				if ( pEntity && pEntity->IsPlayer() )
				{
					UTIL_TraceLine( tr.vecEndPos, vecEnd, dont_ignore_monsters, ENT(pEntity->pev), &tr );
				}
			}
		}

		hit = false;

		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( pEntity && FClassnameIs( pEntity->edict(), "cof_doctorshoot" ) )
			{
				CDoctorBoss *pDoctor = NULL;
				CBaseEntity *pFind = UTIL_FindEntityByClassname( NULL, "monster_doctorboss" );

				if ( pFind )
				{
					pDoctor = GetClassPtr( (CDoctorBoss *)pFind->pev );

					if ( pDoctor )
					{
						switch ( pEntity->pev->iuser1 )
						{
						case 1:
							if ( pDoctor->IsAimingOrShooting() == 1 || pDoctor->IsAimingOrShooting() == 5 )
								hit = true;
							break;
						case 2:
							if ( pDoctor->IsAimingOrShooting() == 2 || pDoctor->IsAimingOrShooting() == 6 )
								hit = true;
							break;
						case 3:
							if ( pDoctor->IsAimingOrShooting() == 3 || pDoctor->IsAimingOrShooting() == 7 )
								hit = true;
							break;
						case 4:
							if ( pDoctor->IsAimingOrShooting() == 4 || pDoctor->IsAimingOrShooting() == 8 )
								hit = true;
							break;
						}
					}
				}
			}

			if ( iDamage )
			{
				if ( iBulletType == BULLET_PLAYER_CS )
					pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_NEVERGIB | (1 << 25) );
				else
					pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_BULLET | DMG_NEVERGIB );

			//	TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
			//	DecalGunshot( &tr, iBulletType );
			}
			else switch(iBulletType)
			{
			case BULLET_NONE: // FIX
				pEntity->TraceAttack(pevAttacker, 50, vecDir, &tr, DMG_CLUB);
			//	TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				// only decal glass
				if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
				{
					UTIL_DecalTrace( &tr, DECAL_GLASSBREAK1 + RANDOM_LONG(0,2) );
				}

				break;
			}

			if ( VARS(tr.pHit)->solid == SOLID_BSP && !tr.fInWater && iBulletType != 50 && iBulletType != 51 )
			{
				UTIL_CustomDecal( &tr, "shot" );
			}
		}

		if ( iBulletType != 50 && iBulletType != 51 )
		{
			// make bullet trails
			UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (flDistance * tr.flFraction) / 64.0 );
		}
	}
	ApplyMultiDamage(pev, pevAttacker);

	CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );

	if ( !pHit || pHit->IsBSPModel() && iBulletType != 50 && iBulletType != 51 )
	{
		if ( pHit->IsBSPModel() )
		{
			if ( pPlayer && iBulletType != 50 )
			{
				if ( !FClassnameIs( pHit->edict(), "func_breakable" ) && !FClassnameIs( pHit->edict(), "cof_doctorshoot" ) )
				{
					pPlayer->playerstats.shots_missed++;
				}
				else
				{
					if ( FClassnameIs( pHit->edict(), "cof_doctorshoot" ) )
					{
						if ( !hit )
							pPlayer->playerstats.shots_missed++;
						else
							pPlayer->playerstats.shots_hit++;
					}
					else
						pPlayer->playerstats.shots_hit++;
				}
			}
		}
		else
		{
			if ( pPlayer && iBulletType != 50 )
				pPlayer->playerstats.shots_missed++;
		}
	}
	else
	{
		if ( pPlayer && iBulletType != 50 && iBulletType != 51 )
		{
			if ( pPlayer->IRelationship( pHit ) <= R_NO )
				pPlayer->playerstats.shots_missed++;
			else
				pPlayer->playerstats.shots_hit++;
		}
	}

	return Vector( x * vecSpread.x, y * vecSpread.y, 0.0 );
}

void CBaseEntity :: TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if (BloodColor() == DONT_BLEED)
		return;

	if (flDamage == 0)
		return;

	if (! (bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_MORTAR | (1 << 25))))
		return;

	// make blood decal on the wall!
	TraceResult Bloodtr;
	Vector vecTraceDir;
	float flNoise;
	int cCount;
	int i;

/*
	if ( !IsAlive() )
	{
		// dealing with a dead monster.
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return;
		}
		else
		{
			pev->max_health--;
		}
	}
*/

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * -172, ignore_monsters, ENT(pev), &Bloodtr);

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

//=========================================================
//=========================================================
void CBaseMonster :: MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const Vector &vecDir )
{
	// make blood decal on the wall!
	TraceResult Bloodtr;
	Vector vecTraceDir;
	int i;

	if ( !IsAlive() )
	{
		// dealing with a dead monster.
		if ( pev->max_health <= 0 )
		{
			// no blood decal for a monster that has already decalled its limit.
			return;
		}
		else
		{
			pev->max_health--;
		}
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir;

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * 172, ignore_monsters, ENT(pev), &Bloodtr);

/*
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_SHOWLINE);
			WRITE_COORD( ptr->vecEndPos.x );
			WRITE_COORD( ptr->vecEndPos.y );
			WRITE_COORD( ptr->vecEndPos.z );

			WRITE_COORD( Bloodtr.vecEndPos.x );
			WRITE_COORD( Bloodtr.vecEndPos.y );
			WRITE_COORD( Bloodtr.vecEndPos.z );
		MESSAGE_END();
*/

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

//=========================================================
// CoFGib - CoF: blow the monster into generic gore pieces
//=========================================================
void CBaseMonster :: CoFGib( void )
{
	if ( m_bUngibbable )
		return;

	for ( int i = 0; i <= 9; i++ )
	{
		CGib *pGib = GetClassPtr( (CGib *)NULL );

		Vector randomangle = Vector( RANDOM_LONG( 0, 359 ), RANDOM_LONG( 0, 359 ), RANDOM_LONG( 0, 359 ) );

		pGib->Spawn( "models/generic_gore.mdl" );
		pGib->pev->body = RANDOM_LONG( 0, 3 );

		pGib->pev->origin = pev->origin;
		pGib->pev->angles.x = RANDOM_LONG( 0, 359 );
		pGib->pev->angles.y = RANDOM_LONG( 0, 359 );
		pGib->pev->angles.z = RANDOM_LONG( 0, 359 );

		pGib->pev->velocity = Vector( RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( -100, 100 ), RANDOM_FLOAT( 200, 300 ) );

		if ( RANDOM_LONG( 0, 1 ) )
			pGib->pev->origin.z += RANDOM_LONG( 0, pev->maxs.z );
		else
			pGib->pev->origin.z -= RANDOM_LONG( 0, pev->mins.z );

		pGib->pev->avelocity.x = RANDOM_FLOAT( 250, 400 );
		pGib->pev->avelocity.y = RANDOM_FLOAT( 250, 400 );

		pGib->LimitVelocity();

		pGib->pev->iuser1 = 1;

		MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
			WRITE_SHORT( pGib->entindex() );
			WRITE_STRING( "aurora/blood_effect.aur" );
			WRITE_BYTE( 0 );
			WRITE_STRING( "models/generic_gore.mdl" );
			WRITE_SHORT( 0 );
		MESSAGE_END();
	}

	EMIT_SOUND( ENT(pev), CHAN_WEAPON, "common/bodysplat.wav", 1, ATTN_NORM );

	UTIL_Remove( this );
}
