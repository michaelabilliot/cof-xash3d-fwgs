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
//=========================================================
// GameRules.cpp
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"gamerules.h"
#include	"teamplay_gamerules.h"
#include	"skill.h"
#include	"game.h"

extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

DLL_GLOBAL CGameRules*	g_pGameRules = NULL;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgMOTD;
extern int gmsgSayText;

extern int gEvilImpulse101;
extern int gSpecialItemGive;

int g_teamplay = 0;

//=========================================================
//=========================================================
BOOL CGameRules::CanHaveAmmo( CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry )
{
	int iAmmoIndex;

	if ( pszAmmoName )
	{
		iAmmoIndex = pPlayer->GetAmmoIndex( pszAmmoName );

		if ( iAmmoIndex > -1 )
		{
			if ( pPlayer->AmmoInventory( iAmmoIndex ) < iMaxCarry )
			{
				// player has room for more of this type of ammo
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
//=========================================================
edict_t *CGameRules :: GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	edict_t *pentSpawnSpot = EntSelectSpawnPoint( pPlayer );

	pPlayer->pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
	pPlayer->pev->v_angle  = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pentSpawnSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = TRUE;
	pPlayer->m_vPreviousOrigin = pPlayer->pev->origin;

	return pentSpawnSpot;
}

//=========================================================
//=========================================================
BOOL CGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBasePlayerItem *pWeapon )
{
	// only living players can have items
	if ( pPlayer->pev->deadflag != DEAD_NO )
		return FALSE;

	if ( !gEvilImpulse101 && !gSpecialItemGive )
	{
		if ( pWeapon->pev->iuser4 != 1 )
		{
			if ( !( ( pPlayer->pev->button | pPlayer->m_afButtonPressed | pPlayer->m_afButtonReleased ) & IN_USE ) )
				return FALSE;
		}
	}

	if ( pWeapon->pszAmmo1() )
	{
		if ( !CanHaveAmmo( pPlayer, pWeapon->pszAmmo1(), pWeapon->iMaxAmmo1() ) )
		{
			// we can't carry anymore ammo for this gun. We can only
			// have the gun if we aren't already carrying one of this type
			if ( pPlayer->HasPlayerItem( pWeapon ) )
			{
				return FALSE;
			}
		}
	}
	else
	{
		// weapon doesn't use ammo, don't take another if you already have it.
		if ( pPlayer->HasPlayerItem( pWeapon ) )
		{
			return FALSE;
		}
	}

	// note: will fall through to here if GetItemInfo doesn't fill the struct!
	return TRUE;
}

//=========================================================
// load the SkillData struct with the proper values based on the skill level.
//=========================================================
void CGameRules::RefreshSkillData ( void )
{
	int	iSkill;

	iSkill = (int)CVAR_GET_FLOAT("difficulty");
	g_iSkillLevel = iSkill;

	if ( iSkill < 1 )
	{
		iSkill = 1;
	}
	else if ( iSkill > 4 )
	{
		iSkill = 4;
	}

	gSkillData.iSkillLevel = iSkill;

	ALERT ( at_debug, "\nGAME SKILL LEVEL:%d\n",iSkill );

	// Syringe
	gSkillData.syringeHeal = GetSkillCvar( "sk_syringe_heal" );

	// Flare
	gSkillData.flareBurnTime = GetSkillCvar( "sk_flare_burn_time" );

	// Spitter
	gSkillData.spitterHealth = GetSkillCvar( "sk_spitter_health" );

	// AOM pills
	gSkillData.aomPills = GetSkillCvar( "sk_aom_pills" );

	// Axe
	gSkillData.DmgAxe = GetSkillCvar( "sk_cof_axe" );

	// Twitcher
	gSkillData.twitcherHealth = GetSkillCvar( "sk_twitcher_health" );
	gSkillData.twitcherDmgOneSlash = GetSkillCvar( "sk_twitcher_dmg_one_slash" );
	gSkillData.twitcherDmgBothSlash = GetSkillCvar( "sk_twitcher_dmg_both_slash" );

	// Twitcher 2
	gSkillData.twitcher2Health = GetSkillCvar( "sk_twitcher2_health" );
	gSkillData.twitcher2DmgOneSlash = GetSkillCvar( "sk_twitcher2_dmg_one_slash" );
	gSkillData.twitcher2DmgBothSlash = GetSkillCvar( "sk_twitcher2_dmg_both_slash" );

	// Twitcher 3
	gSkillData.twitcher3Health = GetSkillCvar( "sk_twitcher3_health" );
	gSkillData.twitcher3DmgOneSlash = GetSkillCvar( "sk_twitcher3_dmg_one_slash" );
	gSkillData.twitcher3DmgBothSlash = GetSkillCvar( "sk_twitcher3_dmg_both_slash" );

	// Twitcher 4
	gSkillData.twitcher4Health = GetSkillCvar( "sk_twitcher4_health" );
	gSkillData.twitcher4DmgOneSlash = GetSkillCvar( "sk_twitcher4_dmg_one_slash" );
	gSkillData.twitcher4DmgBothSlash = GetSkillCvar( "sk_twitcher4_dmg_both_slash" );

	// Sewmo
	gSkillData.sewmoHealth = GetSkillCvar( "sk_sewmo_health" );
	gSkillData.sewmoDmgOneSlash = GetSkillCvar( "sk_sewmo_dmg_one_slash" );
	gSkillData.sewmoDmgBothSlash = GetSkillCvar( "sk_sewmo_dmg_both_slash" );

	// Slower 3
	gSkillData.slower3Health = GetSkillCvar( "sk_slower3_health" );
	gSkillData.slower3DmgOneSlash = GetSkillCvar( "sk_slower3_dmg_one_slash" );
	gSkillData.slower3DmgBothSlash = GetSkillCvar( "sk_slower3_dmg_both_slash" );

	// Sewer boss
	gSkillData.sewerbossHealth = GetSkillCvar( "sk_sewerboss_health" );
	gSkillData.sewerbossMelee = GetSkillCvar( "sk_sewerboss_melee" );

	// Roof boss
	gSkillData.roofbossHealth = GetSkillCvar( "sk_roofboss_health" );
	gSkillData.roofbossHeadHit = GetSkillCvar( "sk_roofboss_headhit" );
	gSkillData.roofbossHeadHealth = GetSkillCvar( "sk_roofboss_headhealth" );
	gSkillData.roofbossAttackTime = GetSkillCvar( "sk_roofboss_attacktime" );

	// Zombie
	gSkillData.zombieHealth = GetSkillCvar( "sk_zombie_health" );
	gSkillData.zombieDmgOneSlash = GetSkillCvar( "sk_zombie_dmg_one_slash" );
	gSkillData.zombieDmgBothSlash = GetSkillCvar( "sk_zombie_dmg_both_slash" );

	// Taller
	gSkillData.tallerHealth = GetSkillCvar( "sk_taller_health" );
	gSkillData.tallerDmgPunch = GetSkillCvar( "sk_taller_dmg_punch" );

	// Suicider
	gSkillData.suiciderHealth = GetSkillCvar( "sk_suicider_health" );
	gSkillData.suiciderBullet = GetSkillCvar( "sk_suicider_bullet" );
	gSkillData.suiciderSuicide = GetSkillCvar( "sk_suicider_suicide" );

	// Flygare
	gSkillData.flygareAcid = GetSkillCvar( "sk_flygare_acid" );
	gSkillData.flygareHealth = GetSkillCvar( "sk_flygare_health" );
	gSkillData.flygareMelee = GetSkillCvar( "sk_flygare_melee" );

	// Child
	gSkillData.childHealth = GetSkillCvar( "sk_child_health" );
	gSkillData.childDmgOneSlash = GetSkillCvar( "sk_child_dmg_one_slash" );
	gSkillData.childDmgBothSlash = GetSkillCvar( "sk_child_dmg_both_slash" );

	// Sawrunner
	gSkillData.sawrunnerHealth = GetSkillCvar( "sk_sawrunner_health" );
	gSkillData.sawrunnerDmgOneSlash = GetSkillCvar( "sk_sawrunner_dmg_one_slash" );
	gSkillData.sawrunnerDmgBothSlash = GetSkillCvar( "sk_sawrunner_dmg_both_slash" );

	// Doctor
	gSkillData.doctorHealth = GetSkillCvar( "sk_doctor_health" );
	gSkillData.doctorP345 = GetSkillCvar( "sk_doctor_p345" );
	gSkillData.doctorRevolver = GetSkillCvar( "sk_doctor_revolver" );

	// Crazy runner
	gSkillData.rcrazyHealth = GetSkillCvar( "sk_rcrazy_health" );
	gSkillData.rcrazyDmgOneSlash = GetSkillCvar( "sk_rcrazy_dmg_one_slash" );
	gSkillData.rcrazyDmgBothSlash = GetSkillCvar( "sk_rcrazy_dmg_both_slash" );

	// Faceless
	gSkillData.facelessHealth = GetSkillCvar( "sk_faceless_health" );
	gSkillData.facelessDmgOneSlash = GetSkillCvar( "sk_faceless_dmg_one_slash" );
	gSkillData.facelessDmgBothSlash = GetSkillCvar( "sk_faceless_dmg_both_slash" );

	// Chainsaw boss
	gSkillData.chainsawbossHealth = GetSkillCvar( "sk_chainsawboss_health" );

	// Faster
	gSkillData.fasterHealth = GetSkillCvar( "sk_faster_health" );
	gSkillData.fasterDmgOneSlash = GetSkillCvar( "sk_faster_dmg_one_slash" );
	gSkillData.fasterDmgBothSlash = GetSkillCvar( "sk_faster_dmg_both_slash" );

	// Krypande
	gSkillData.krypandeHealth = GetSkillCvar( "sk_krypande_health" );
	gSkillData.krypandeDmgOneSlash = GetSkillCvar( "sk_krypande_dmg_one_slash" );
	gSkillData.krypandeDmgBothSlash = GetSkillCvar( "sk_krypande_dmg_both_slash" );

	// Baby
	gSkillData.babyHealth = GetSkillCvar( "sk_baby_health" );
	gSkillData.babyDmg = GetSkillCvar( "sk_baby_damage" );

	// Stranger
	gSkillData.strangerHealth = GetSkillCvar( "sk_stranger_health" );
	gSkillData.strangerDmg = GetSkillCvar( "sk_stranger_damage" );

// PLAYER WEAPONS
	gSkillData.DmgNightstick = GetSkillCvar( "sk_cof_nightstick" );
	gSkillData.DmgSwitchblade = GetSkillCvar( "sk_cof_switchblade" );
	gSkillData.DmgPunch = GetSkillCvar( "sk_cof_punch" );
	gSkillData.DmgSwitchblade2 = GetSkillCvar( "sk_cof_2switchblade" );
	gSkillData.DmgBranch = GetSkillCvar( "sk_cof_branch" );
	gSkillData.DmgSledgehammer = GetSkillCvar( "sk_cof_sledgehammer" );

	// Weapon melee
	gSkillData.DmgWeaponMelee = GetSkillCvar( "sk_cof_weapon_melee" );

	gSkillData.DmgM16 = GetSkillCvar( "sk_cof_m16" );
	gSkillData.DmgVP70 = GetSkillCvar( "sk_cof_VP70" );
	gSkillData.DmgGlock = GetSkillCvar( "sk_cof_glock" );
	gSkillData.DmgShotgun = GetSkillCvar( "sk_cof_shotgun" );
	gSkillData.DmgP345 = GetSkillCvar( "sk_cof_p345" );
	gSkillData.DmgRifle = GetSkillCvar( "sk_cof_rifle" );
	gSkillData.DmgRevolver = GetSkillCvar( "sk_cof_revolver" );
	gSkillData.DmgG43_guess = GetSkillCvar( "sk_cof_g43" );

	// HEALTH/CHARGE
	gSkillData.suitchargerCapacity = GetSkillCvar( "sk_suitcharger" );
	gSkillData.batteryCapacity = GetSkillCvar( "sk_battery" );
	gSkillData.healthchargerCapacity = GetSkillCvar ( "sk_healthcharger" );
	gSkillData.healthkitCapacity = GetSkillCvar ( "sk_healthkit" );
	gSkillData.flashlightCharge = GetSkillCvar ( "sk_flashcharge" );

	// monster damage adj
	gSkillData.monHead = GetSkillCvar( "sk_monster_head" );
	gSkillData.monChest = GetSkillCvar( "sk_monster_chest" );
	gSkillData.monStomach = GetSkillCvar( "sk_monster_stomach" );
	gSkillData.monLeg = GetSkillCvar( "sk_monster_leg" );
	gSkillData.monArm = GetSkillCvar( "sk_monster_arm" );

	// player damage adj
	gSkillData.plrHead = GetSkillCvar( "sk_player_head" );
	gSkillData.plrChest = GetSkillCvar( "sk_player_chest" );
	gSkillData.plrStomach = GetSkillCvar( "sk_player_stomach" );
	gSkillData.plrLeg = GetSkillCvar( "sk_player_leg" );
	gSkillData.plrArm = GetSkillCvar( "sk_player_arm" );

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = GetSkillCvar( "sk_plr_buckshot" );

	// The monster and player damage adjustments are read a second time
	// below (1.55 and 1.6a both do this).

	// monster damage adj
	gSkillData.monHead = GetSkillCvar( "sk_monster_head" );
	gSkillData.monChest = GetSkillCvar( "sk_monster_chest" );
	gSkillData.monStomach = GetSkillCvar( "sk_monster_stomach" );
	gSkillData.monLeg = GetSkillCvar( "sk_monster_leg" );
	gSkillData.monArm = GetSkillCvar( "sk_monster_arm" );

	// player damage adj
	gSkillData.plrHead = GetSkillCvar( "sk_player_head" );
	gSkillData.plrChest = GetSkillCvar( "sk_player_chest" );
	gSkillData.plrStomach = GetSkillCvar( "sk_player_stomach" );
	gSkillData.plrLeg = GetSkillCvar( "sk_player_leg" );
	gSkillData.plrArm = GetSkillCvar( "sk_player_arm" );

	// buz: maxspeeds
	gSkillData.plrPrimaryMaxSpeed = GetSkillCvar( "sk_primary_speed" );
	gSkillData.plrSecondaryMaxSpeed = GetSkillCvar( "sk_secondary_speed" );
	gSkillData.plrSprintMaxSpeed = GetSkillCvar( "sk_sprint_speed" );
	gSkillData.plrStairMaxSpeed = GetSkillCvar( "sk_stair_speed" );
}

//=========================================================
// instantiate the proper game rules object
//=========================================================

CGameRules *InstallGameRules( void )
{
	SERVER_COMMAND( "exec game.cfg\n" );
	SERVER_EXECUTE( );

	if ( !gpGlobals->deathmatch )
	{
		// generic half-life
		g_teamplay = 0;
		return new CHalfLifeRules;
	}
	else
	{
		// vanilla deathmatch
		g_teamplay = 0;
		return new CHalfLifeMultiplay;
	}
}

//=========================================================
// CoF 1.6a: set by CHalfLifeRules only (the byte at +0xDC)
//=========================================================
bool CGameRules::IsMapperMode_guess( void )
{
	CHalfLifeRules *pRules = dynamic_cast<CHalfLifeRules *>( this );

	if ( !pRules )
		return false;

	return pRules->m_bUnknownDC_guess;
}

//=========================================================
// vote kick: count the votes once the voting time is over
//=========================================================
void CGameRules::VoteKickThink( void )
{
	int i = 0;

	if ( m_bInVoteKick )
	{
		CBasePlayer *pVictim = NULL;

		if ( m_flVoteKickTime < gpGlobals->time && m_sVoteKickVictimID )
		{
			UTIL_LogPrintf( "Votekick: Deciding to kick\n" );

			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && !strcmp( GETPLAYERAUTHID( plr->edict() ), m_sVoteKickVictimID ) )
				{
					ALERT( at_console, "VOTEKICK - Found player %s with ID of %s against the kick ID of %s\n", STRING( plr->pev->netname ), GETPLAYERAUTHID( plr->edict() ), m_sVoteKickVictimID );
					pVictim = plr;
					pVoteKickVictimID = (char *)STRING( plr->pev->netname );
				}
			}

			if ( pVictim )
			{
				if ( m_iVotesForKick > m_iVotesAgainstKick )
				{
					for ( i = 1; i <= gpGlobals->maxClients; i++ )
					{
						CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

						if ( plr )
						{
							char szText[128];
							sprintf_s( szText, sizeof( szText ), " Vote kick: Kicking %s", STRING( pVictim->pev->netname ) );

							MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, plr->pev );
								WRITE_BYTE( ENTINDEX( plr->edict() ) );
								WRITE_STRING( szText );
							MESSAGE_END();
						}
					}

					UTIL_EmitAmbientSound( ENT(0), g_vecZero, "common/votekick/votekick_succeeded.wav", 1, ATTN_NONE, 0, 100 );

					char command[128];
					sprintf( command, "kick \"%s\"\n", STRING( pVictim->pev->netname ) );
					SERVER_COMMAND( command );
				}
				else
				{
					for ( i = 1; i <= gpGlobals->maxClients; i++ )
					{
						CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

						if ( plr )
						{
							MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, plr->pev );
								WRITE_BYTE( ENTINDEX( plr->edict() ) );
								WRITE_STRING( " Vote kick failed. Yes votes must exceed no votes." );
							MESSAGE_END();
						}
					}

					UTIL_EmitAmbientSound( ENT(0), g_vecZero, "common/votekick/votekick_failed.wav", 1, ATTN_NONE, 0, 100 );
				}
			}

			m_bInVoteKick = false;
			m_sVoteKickVictimID = NULL;
			pVoteKickVictimID = NULL;

			m_iVotesForKick = m_iVotesAgainstKick = 0;

			for ( i = 1; i <= 32; i++ )
				pVoted[i] = NULL;
		}
	}
}

//=========================================================
// vote kick: pCaller wants pVictim kicked
//=========================================================
void CGameRules::StartVoteKick( CBasePlayer *pCaller, CBasePlayer *pVictim )
{
	if ( !pCaller || !pVictim )
		return;

	if ( m_bInVoteKick )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pCaller->pev );
			WRITE_BYTE( ENTINDEX( pCaller->edict() ) );
			WRITE_STRING( " Vote kick already in progress" );
		MESSAGE_END();

		return;
	}
	else
	{
		char *idtest = (char *)GETPLAYERAUTHID( pVictim->edict() );

		if ( idtest )
		{
			if ( !strcmp( idtest, "STEAM_ID_PENDING" ) )
				return;

			if ( !strcmp( idtest, "STEAM_ID_LAN" ) )
				return;
		}

		m_bInVoteKick = true;
		m_flVoteKickTime = gpGlobals->time + CVAR_GET_FLOAT( "sv_votekick_time" );

		m_sVoteKickVictimID = (char *)GETPLAYERAUTHID( pVictim->edict() );

		int i = 0;

		m_iVotesForKick = m_iVotesAgainstKick = 0;

		for ( i = 1; i <= 32; i++ )
			pVoted[i] = NULL;

		UTIL_LogPrintf( " Votekick called against %s\n", STRING( pVictim->pev->netname ) );

		// the caller votes yes
		pVoted[1] = (char *)GETPLAYERAUTHID( pCaller->edict() );
		m_iVotesForKick = 1;

		char szText[128];
		sprintf( szText, " Vote kick called against %s", STRING( pVictim->pev->netname ) );

		UTIL_EmitAmbientSound( ENT(0), g_vecZero, "common/votekick/votekick_notify.wav", 1, ATTN_NONE, 0, 100 );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );
			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, plr->pev );
					WRITE_BYTE( ENTINDEX( plr->edict() ) );
					WRITE_STRING( szText );
				MESSAGE_END();
			}
		}
	}
}
