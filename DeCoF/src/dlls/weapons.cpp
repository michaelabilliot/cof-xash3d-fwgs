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

===== weapons.cpp ========================================================

  functions governing the selection/use of weapons for players

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "cofr_features.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"

extern CGraph	WorldGraph;
extern int gEvilImpulse101;
extern int gSpecialItemGive;
extern int g_PrecacheCSWeapons;

extern int gmsgCurWeapon;
extern int gmsgWpnInfo;
extern int gmsgWeaponBody;
extern int gmsgWpnDeploy;
extern int gmsgCustomSlv;
extern int gmsgCustomFng;
extern int gmsgCustomGlv;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );


#define NOT_USED 255

DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
DLL_GLOBAL  const char *g_pModelNameLaser = "sprites/laserbeam.spr";
DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for the initial blood
DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for splattered blood
DLL_GLOBAL	short	g_sModelIndexNull;
DLL_GLOBAL	short	g_sModelIndexWaterSpray;
DLL_GLOBAL	short	g_sModelIndexWaterDrop;

DLL_GLOBAL unsigned short m_usSmokePuff; // buz - smoke event
DLL_GLOBAL unsigned short m_usViewPunch;
DLL_GLOBAL unsigned short m_usWeaponShoot;
DLL_GLOBAL unsigned short m_usClientSound;
DLL_GLOBAL unsigned short m_usThirdFlash;
DLL_GLOBAL unsigned short m_usRevolverShells;

const char *GetNameForId[] =
{
	NULL,
	"Flashlight",
	"Mobile Phone",
	"Glock 19",
	"Shotgun",
	"Camera",
	"Switchblade",
	"P345",
	"Syringe",
	"M16",
	"Lantern",
	"Sniper Rifle",
	"Nightstick",
	"Flare",
	"TMP",
	"Dual Wield",
	"Branch",
	" ",
	"Sledgehammer",
	"VP70",
	"Browning HP",
	"Taurus .357 MAG",
	"Police Radio",
	"FAMAS",
	"Axe",
	"Simon's book",
	"Gewehr 43",
	"MP5A3",
};

const char *GetClassnameForId[] =
{
	NULL,
	"weapon_flashlight",
	"weapon_mobile",
	"weapon_glock",
	"weapon_shotgun",
	"weapon_camera",
	"weapon_switchblade",
	"weapon_p345",
	"weapon_syringe",
	"weapon_m16",
	"weapon_lantern",
	"weapon_rifle",
	"weapon_nightstick",
	"weapon_flare",
	"weapon_tmp",
	"weapon_dualweapon",
	"weapon_branch",
	"weapon_action",
	"weapon_sledgehammer",
	"weapon_vp70",
	"weapon_browning_wheelchair",
	"weapon_revolver",
	"weapon_radio",
	"weapon_famas",
	"weapon_axe",
	"weapon_booklaser",
	"weapon_g43",
	"weapon_mp5",
};

ItemInfo CBasePlayerItem::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerItem::AmmoInfoArray[MAX_AMMO_SLOTS];

MULTIDAMAGE gMultiDamage;

#define TRACER_FREQ		4			// Tracers fire every fourth bullet


//=========================================================
// MaxAmmoCarry - pass in a name and this function will tell
// you the maximum amount of that type of ammunition that a
// player can carry.
//=========================================================
int MaxAmmoCarry( int iszName )
{
	for ( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo1 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo1 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo1;
		if ( CBasePlayerItem::ItemInfoArray[i].pszAmmo2 && !strcmp( STRING(iszName), CBasePlayerItem::ItemInfoArray[i].pszAmmo2 ) )
			return CBasePlayerItem::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_debug, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ) );
	return -1;
}


/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage

==============================================================================
*/

//
// ClearMultiDamage - resets the global multi damage accumulator
//
void ClearMultiDamage(void)
{
	gMultiDamage.pEntity = NULL;
	gMultiDamage.amount	= 0;
	gMultiDamage.type = 0;
}


//
// ApplyMultiDamage - inflicts contents of global multi damage register on gMultiDamage.pEntity
//
// GLOBALS USED:
//		gMultiDamage

void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker )
{
	Vector		vecSpot1;//where blood comes from
	Vector		vecDir;//direction blood should go
	TraceResult	tr;

	if ( !gMultiDamage.pEntity )
		return;

	gMultiDamage.pEntity->TakeDamage(pevInflictor, pevAttacker, gMultiDamage.amount, gMultiDamage.type );
}


// GLOBALS USED:
//		gMultiDamage

void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType)
{
	if ( !pEntity )
		return;

	gMultiDamage.type |= bitsDamageType;

	if ( pEntity != gMultiDamage.pEntity )
	{
		ApplyMultiDamage(pevInflictor,pevInflictor); // UNDONE: wrong attacker!
		gMultiDamage.pEntity	= pEntity;
		gMultiDamage.amount		= 0;
	}

	gMultiDamage.amount += flDamage;
}

/*
================
SpawnBlood
================
*/
void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage, Vector velocity)
{
	UTIL_BloodDrips( vecSpot, g_vecAttackDir, bloodColor, (int)flDamage, velocity );
}


int DamageDecal( CBaseEntity *pEntity, int bitsDamageType )
{
	if ( !pEntity )
		return (DECAL_GUNSHOT1 + RANDOM_LONG(0,4));

	return pEntity->DamageDecal( bitsDamageType );
}

void DecalGunshot( TraceResult *pTrace, int iBulletType )
{
	// Is the entity valid
	if ( !UTIL_IsValidEntity( pTrace->pHit ) )
		return;

	// buz
	if (pTrace->fInWater)
		return;

	if ( VARS(pTrace->pHit)->solid == SOLID_BSP || VARS(pTrace->pHit)->movetype == MOVETYPE_PUSHSTEP )
	{
		CBaseEntity *pEntity = NULL;
		// Decal the wall with a gunshot
		if ( !FNullEnt(pTrace->pHit) )
			pEntity = CBaseEntity::Instance(pTrace->pHit);

		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		default:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_PLAYER_CS:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, (1 << 25) ) );
			break;
		case BULLET_MONSTER_12MM:
			// smoke and decal
			UTIL_GunshotDecalTrace( pTrace, DamageDecal( pEntity, DMG_BULLET ) );
			break;
		case BULLET_PLAYER_CROWBAR:
			// wall decal
			UTIL_DecalTrace( pTrace, DamageDecal( pEntity, DMG_CLUB ) );
			break;
		}
	}
}



//
// EjectBrass - tosses a brass shell from passed origin at passed velocity
//
void EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	// FIX: when the player shoots, their gun isn't in the same position as it is on the model other players see.

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}


#if 0
// UNDONE: This is no longer used?
void ExplodeModel( const Vector &vecOrigin, float speed, int model, int count )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, vecOrigin );
		WRITE_BYTE ( TE_EXPLODEMODEL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_COORD( speed );
		WRITE_SHORT( model );
		WRITE_SHORT( count );
		WRITE_BYTE ( 15 );// 1.5 seconds
	MESSAGE_END();
}
#endif


int giAmmoIndex = 0;

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry( const char *szAmmoname )
{
	// make sure it's not already in the registry
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if ( stricmp( CBasePlayerItem::AmmoInfoArray[i].pszName, szAmmoname ) == 0 )
			return; // ammo already in registry, just quite
	}


	giAmmoIndex++;
	ASSERT( giAmmoIndex < MAX_AMMO_SLOTS );
	if ( giAmmoIndex >= MAX_AMMO_SLOTS )
		giAmmoIndex = 0;

	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].pszName = szAmmoname;
	CBasePlayerItem::AmmoInfoArray[giAmmoIndex].iId = giAmmoIndex;   // yes, this info is redundant
}


// Precaches the weapon and queues the weapon info for sending to clients
void UTIL_PrecacheOtherWeapon( const char *szClassname )
{
	edict_t	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_debug, "NULL Ent in UTIL_PrecacheOtherWeapon\n" );
		return;
	}

	CBaseEntity *pEntity = CBaseEntity::Instance (VARS( pent ));

	if (pEntity)
	{
		ItemInfo II;
		pEntity->Precache( );
		memset( &II, 0, sizeof II );
		if ( ((CBasePlayerItem*)pEntity)->GetItemInfo( &II ) )
		{
			CBasePlayerItem::ItemInfoArray[II.iId] = II;

			if ( II.pszAmmo1 && *II.pszAmmo1 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo1 );
			}

			if ( II.pszAmmo2 && *II.pszAmmo2 )
			{
				AddAmmoNameToAmmoRegistry( II.pszAmmo2 );
			}

			memset( &II, 0, sizeof II );
		}
	}

	REMOVE_ENTITY(pent);
}

void UTIL_PrecacheSpecialItems( void ) // buz
{
/*	ItemInfo II;

	// add gasmask to weapons list
	memset( &II, 0, sizeof II );
	II.pszName = "gasmask";
	II.iSlot = 0;
	II.iPosition = 2;
	II.iId = WEAPON_GASMASK;
	CBasePlayerItem::ItemInfoArray[II.iId] = II;

	// add head shield to weapons list
	memset( &II, 0, sizeof II );
	II.pszName = "headshield";
	II.iSlot = 0;
	II.iPosition = 1;
	II.iId = WEAPON_HEADSHIELD;
	CBasePlayerItem::ItemInfoArray[II.iId] = II;

	// add painkiller to weapons list
	memset( &II, 0, sizeof II );
	II.pszName = "painkiller";
	II.pszAmmo1 = "painkillers";
	II.iMaxAmmo1 = 10;
	II.iSlot = 0;
	II.iPosition = 3;
	II.iId = WEAPON_PAINKILLER;
	CBasePlayerItem::ItemInfoArray[II.iId] = II;

	AddAmmoNameToAmmoRegistry( "painkillers" );*/
}

void LoadSpreadTable( char *filename ); // buz

// called by worldspawn
void W_Precache(void)
{
	memset( CBasePlayerItem::ItemInfoArray, 0, sizeof(CBasePlayerItem::ItemInfoArray) );
	memset( CBasePlayerItem::AmmoInfoArray, 0, sizeof(CBasePlayerItem::AmmoInfoArray) );
	giAmmoIndex = 0;

	if ( g_PrecacheCSWeapons )
	{
		UTIL_PrecacheOtherWeapon( "weapon_csak47" );
		UTIL_PrecacheOtherWeapon( "weapon_csaug" );
		UTIL_PrecacheOtherWeapon( "weapon_csawp" );
		UTIL_PrecacheOtherWeapon( "weapon_csbomb" );
		UTIL_PrecacheOtherWeapon( "weapon_csdeagle" );
		UTIL_PrecacheOtherWeapon( "weapon_cselites" );
		UTIL_PrecacheOtherWeapon( "weapon_csfamas" );
		UTIL_PrecacheOtherWeapon( "weapon_csfiveseven" );
		UTIL_PrecacheOtherWeapon( "weapon_csflashbang" );
		UTIL_PrecacheOtherWeapon( "weapon_csg3sg1" );
		UTIL_PrecacheOtherWeapon( "weapon_csm4a1" );
	}

	// ammo
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		UTIL_PrecacheOther( "ammo_glock" );
		UTIL_PrecacheOther( "ammo_p345" );
		UTIL_PrecacheOther( "ammo_shells" );
		UTIL_PrecacheOther( "ammo_rifle" );
		UTIL_PrecacheOther( "ammo_revolver" );
		UTIL_PrecacheOther( "ammo_m16" );
		UTIL_PrecacheOther( "ammo_vp70" );
		UTIL_PrecacheOther( "ammo_g43" );
	}

	// common world objects
	UTIL_PrecacheOther( "item_battery" );
	UTIL_PrecacheOther( "item_suit" );
	UTIL_PrecacheOther( "item_antidote" );
	UTIL_PrecacheOther( "item_security" );
	UTIL_PrecacheOther( "item_longjump" );
	UTIL_PrecacheOther( "monster_slower3" );

	UTIL_PrecacheOther( "item_glocktaclight" );
	UTIL_PrecacheOther( "item_phonebattery" );

	// flashlight
	UTIL_PrecacheOtherWeapon( "weapon_flashlight" );

	// mobile phone
	UTIL_PrecacheOtherWeapon( "weapon_mobile" );

	// glock
	UTIL_PrecacheOtherWeapon( "weapon_glock" );
	UTIL_PrecacheOtherWeapon( "weapon_rpg7" );

	// browning
	UTIL_PrecacheOtherWeapon( "weapon_browning_wheelchair" );

	// book laser
	UTIL_PrecacheOtherWeapon( "weapon_booklaser" );

	// axe
	UTIL_PrecacheOtherWeapon( "weapon_axe" );

	// dual weapon
	UTIL_PrecacheOtherWeapon( "weapon_dualweapon" );

	// p345
	UTIL_PrecacheOtherWeapon( "weapon_p345" );

	// g43
	UTIL_PrecacheOtherWeapon( "weapon_g43" );

	// shotgun
	UTIL_PrecacheOtherWeapon( "weapon_shotgun" );

	// m16
	UTIL_PrecacheOtherWeapon( "weapon_m16" );

	// vp70
	UTIL_PrecacheOtherWeapon( "weapon_vp70" );

	// rifle
	UTIL_PrecacheOtherWeapon( "weapon_rifle" );

	// syringe
	UTIL_PrecacheOtherWeapon( "weapon_syringe" );

	// action
	UTIL_PrecacheOtherWeapon( "weapon_action" );

	// camera
	UTIL_PrecacheOtherWeapon( "weapon_camera" );

	// switchblade
	UTIL_PrecacheOtherWeapon( "weapon_switchblade" );

	// lantern
	UTIL_PrecacheOtherWeapon( "weapon_lantern" );

	// nightstick
	UTIL_PrecacheOtherWeapon( "weapon_nightstick" );

	// sledgehammer
	UTIL_PrecacheOtherWeapon( "weapon_sledgehammer" );

	// branch
	UTIL_PrecacheOtherWeapon( "weapon_branch" );

	// flare
	UTIL_PrecacheOtherWeapon( "weapon_flare" );

	// tmp
	UTIL_PrecacheOtherWeapon( "weapon_tmp" );

	// mp5
	UTIL_PrecacheOtherWeapon( "weapon_mp5" );

	// famas
	UTIL_PrecacheOtherWeapon( "weapon_famas" );

	// revolver
	UTIL_PrecacheOtherWeapon( "weapon_revolver" );

	UTIL_PrecacheOther( "weaponbox" );// container for dropped deathmatch weapons

	// muzzleflashes
	PRECACHE_MODEL( "sprites/gunsmoke.spr" );
	PRECACHE_MODEL( "models/flash1.mdl" );
	PRECACHE_MODEL( "models/flash2.mdl" );
	PRECACHE_MODEL( "models/flash3.mdl" );

	g_sModelIndexFireball = PRECACHE_MODEL ("sprites/zerogxplode.spr");// fireball
	g_sModelIndexWExplosion = PRECACHE_MODEL ("sprites/WXplo1.spr");// underwater fireball
	g_sModelIndexSmoke = PRECACHE_MODEL ("sprites/steam1.spr");// smoke
	g_sModelIndexBubbles = PRECACHE_MODEL ("sprites/bubble.spr");//bubbles
	g_sModelIndexBloodSpray = PRECACHE_MODEL ("sprites/bloodspray.spr"); // initial blood
	g_sModelIndexBloodDrop = PRECACHE_MODEL ("sprites/blood.spr"); // splattered blood
	g_sModelIndexNull = PRECACHE_MODEL ("sprites/null.spr");

	g_sModelIndexWaterSpray = PRECACHE_MODEL ("sprites/splash_main.spr");
	g_sModelIndexWaterDrop = PRECACHE_MODEL ("sprites/waterdrop.spr");

	g_sModelIndexLaser = PRECACHE_MODEL( (char *)g_pModelNameLaser );
	g_sModelIndexLaserDot = PRECACHE_MODEL("sprites/laserdot.spr");

	m_usSmokePuff = PRECACHE_EVENT( 1, "events/smokepuff.sc" ); // buz
	m_usViewPunch = PRECACHE_EVENT( 1, "events/viewpunch.sc" );
	m_usWeaponShoot = PRECACHE_EVENT( 1, "events/weaponshoot.sc" );
	m_usClientSound = PRECACHE_EVENT( 1, "events/clientsound.sc" );
	m_usThirdFlash = PRECACHE_EVENT( 1, "events/muzzleflash.sc" );
	m_usRevolverShells = PRECACHE_EVENT( 1, "events/revolvershells.sc" );

	// used by explosions
	PRECACHE_MODEL ("models/grenade.mdl");
	PRECACHE_MODEL ("sprites/explode1.spr");

	PRECACHE_SOUND ("weapons/weapon_fire_empty.wav");
	PRECACHE_SOUND ("weapons/weapon_fire_empty_noclick.wav");

	PRECACHE_SOUND ("weapons/melee_swing.wav");
	PRECACHE_SOUND ("weapons/melee_hit.wav");

	// weapon switch
	PRECACHE_SOUND ("weapons/sleeve_generic1.wav");
	PRECACHE_SOUND ("weapons/sleeve_generic2.wav");
	PRECACHE_SOUND ("weapons/sleeve_generic3.wav");

	PRECACHE_SOUND ("weapons/dead_arm.wav");

	PRECACHE_SOUND ("weapons/debris1.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris2.wav");// explosion aftermaths
	PRECACHE_SOUND ("weapons/debris3.wav");// explosion aftermaths

//	PRECACHE_SOUND ("weapons/grenade_hit1.wav");//grenade
//	PRECACHE_SOUND ("weapons/grenade_hit2.wav");//grenade
//	PRECACHE_SOUND ("weapons/grenade_hit3.wav");//grenade

//	PRECACHE_SOUND ("weapons/bullet_hit1.wav");	// hit by bullet
//	PRECACHE_SOUND ("weapons/bullet_hit2.wav");	// hit by bullet

//	PRECACHE_SOUND ("items/weapondrop1.wav");// weapon falls to the ground

	// pickup
	PRECACHE_SOUND ("weapons/weapon_get.wav");
	PRECACHE_SOUND ("weapons/item_get.wav");

	// buz
//	PRECACHE_MODEL ("sprites/confrag.spr");
//	PRECACHE_MODEL ("sprites/woodfrag.spr");
	PRECACHE_MODEL ("sprites/wsplash_x.spr");

	// buz:
	LoadSpreadTable("spread_settings.txt");
}




TYPEDESCRIPTION	CBasePlayerItem::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerItem, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerItem, m_pNext, FIELD_CLASSPTR ),
	//DEFINE_FIELD( CBasePlayerItem, m_fKnown, FIELD_INTEGER ),Reset to zero on load
	DEFINE_FIELD( CBasePlayerItem, m_iId, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdPrimary, FIELD_INTEGER ),
	// DEFINE_FIELD( CBasePlayerItem, m_iIdSecondary, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iCurrCaps, FIELD_INTEGER ), // Wargon: usable items.
	DEFINE_FIELD( CBasePlayerItem, strwmodel, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayerItem, script, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayerItem, m_iSprintState, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iNextSprintAnim, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerItem, m_bCanSuicide, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerItem, m_iSuicideState, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iSuicideButtonsNeeded, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iSuicideButtonsPressed, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iRageNeeded, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iRageLevel, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_fNextRageDecrease, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerItem, m_iSprintIdleAnim1, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iSprintIdleAnim1, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerItem, m_fReloadSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerItem, m_bRifleReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerItem, m_bWantToEndReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerItem, m_bRaised, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerItem, m_iReloadState, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_iNextReloadAnim, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerItem, slotnumber, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerItem, m_Accuracy, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerItem, m_bCanDualWield, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerItem, CBaseAnimating );


TYPEDESCRIPTION	CBasePlayerWeapon::m_SaveData[] =
{
#if defined( CLIENT_WEAPONS )
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_FLOAT ),
#else	// CLIENT_WEAPONS
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextTertiaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flAccuracyTime, FIELD_TIME ),
#endif	// CLIENT_WEAPONS
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_bCheckedIfShouldBeDeleted, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerWeapon, custom_v_model, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayerWeapon, custom_w_model, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayerWeapon, custom_script, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayerWeapon, shoot_loop_sound, FIELD_SOUNDNAME ),
	DEFINE_FIELD( CBasePlayerWeapon, shoot_loop_sound_end, FIELD_SOUNDNAME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_bShotLooping, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerWeapon, m_fEmptyClickTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_bIronSights, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayerWeapon, m_fIronSightsForward, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_fIronSightsRight, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_fIronSightsUp, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iIronSightsZoom, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flKickPerShot, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flCurrentKick, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextKickReductionTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_AccuracyVector, FIELD_VECTOR ),
	DEFINE_FIELD( CBasePlayerWeapon, m_bWaitingToReloadIronsights, FIELD_BOOLEAN ),
//	DEFINE_FIELD( CBasePlayerWeapon, m_iClientClip, FIELD_INTEGER )	 , reset to zero on load so hud gets updated correctly
//  DEFINE_FIELD( CBasePlayerWeapon, m_iClientWeaponState, FIELD_INTEGER ), reset to zero on load so hud gets updated correctly
};

IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBasePlayerItem );


void CBasePlayerItem :: SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-24, -24, 0);
	pev->absmax = pev->origin + Vector(24, 24, 16);
}


//=========================================================
// Sets up movetype, size, solidtype for a new weapon.
//=========================================================
void CBasePlayerItem :: FallInit( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0) );//pointsize until it lands on the ground.

	// Wargon: the item is usable.
	SetUse( &CBasePlayerItem::DefaultUse );
	m_iCurrCaps = CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE;

	if ( pev->iuser4 == 1 )
		SetTouch(&CBasePlayerItem :: DefaultTouch );

	SetThink(&CBasePlayerItem :: FallThink );

	SetNextThink( 0.1 );
}

int CBasePlayerItem::iMaxAmmo1( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( FClassnameIs( edict(), "weapon_syringe" ) )
			return 1;

		if ( ItemInfoArray[ m_iId ].iMaxAmmo1 > 0 )
			return ItemInfoArray[ m_iId ].iMaxClip * 3;
		else
			return ItemInfoArray[ m_iId ].iMaxAmmo1;
	}

	return ItemInfoArray[ m_iId ].iMaxAmmo1;
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerItem::FallThink ( void )
{
	SetNextThink( 0.1 );

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		//if ( !FNullEnt( pev->owner ) )
		//{
		//	int pitch = 95 + RANDOM_LONG(0,29);
		//	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);
		//}

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		Materialize();
	}
}

//=========================================================
// Materialize - make a CBasePlayerItem visible and tangible
//=========================================================
void CBasePlayerItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		//EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	pev->solid = SOLID_TRIGGER;

	UTIL_SetOrigin( this, pev->origin );// link into world.

	// Wargon: the item is usable.
	SetUse( &CBasePlayerItem::DefaultUse );
	m_iCurrCaps = CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE;

	if ( pev->iuser4 == 1 )
		SetTouch(&CBasePlayerItem::DefaultTouch);

	SetThink (NULL);

}

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerItem::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		Materialize();
		return;
	}

	SetNextThink( time );
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should
// it respawn?
//=========================================================
void CBasePlayerItem :: CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO:
		return;
		break;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerItem::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = CBaseEntity::Create( (char *)STRING( pev->classname ), g_pGameRules->VecWeaponRespawnSpot( this ), pev->angles, pev->owner );

	if ( pNewWeapon )
	{
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now

		// Wargon: the item is not usable.
		pNewWeapon->SetUse( NULL );
		m_iCurrCaps = CBaseEntity::ObjectCaps();

		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink(&CBasePlayerItem:: AttemptToMaterialize );

		DROP_TO_FLOOR ( ENT(pev) );

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->AbsoluteNextThink( g_pGameRules->FlWeaponRespawnTime( this ) );
	}
	else
	{
		ALERT ( at_debug, "Respawn failed to create %s!\n", STRING( pev->classname ) );
	}

	return pNewWeapon;
}

void CBasePlayerItem::DefaultTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	// if it's not a player, ignore
	if ( !pOther->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pOther->pev );
 if(m_iId==31&&!CoFR_RpgPickupAllowed(pPlayer,static_cast<CBasePlayerWeapon*>(this)->m_iClip))return;

	if ( pPlayer && !strcmp( STRING( pev->classname ), "weapon_tmp" ) )
	{
		if ( !pPlayer->IsDonator() )
		{
			UTIL_ProMessage( pPlayer, "This is a donator-only weapon.", 5, false );
			return;
		}
	}

	if ( pPlayer && !strcmp( STRING( pev->classname ), "weapon_mp5" ) )
	{
		if ( !pPlayer->IsDeveloper() && pPlayer->m_iNew16a_guess != 2 && pPlayer->m_iNew16a_guess != 3 && pPlayer->m_iNew16a_guess != 4 && g_pGameRules && !g_pGameRules->IsMapperMode_guess() )
		{
			UTIL_ProMessage( pPlayer, "This is for mappers only.", 5, false );
			return;
		}
	}

	// Lantern
	if ( !strcmp( STRING( pev->classname ), "weapon_lantern" ) )
	{
		if ( pPlayer->HasNamedPlayerItem( "weapon_lantern" ) )
			return;
	}

	// can I have this?
	if ( g_pGameRules && !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( FClassnameIs( edict(), "weapon_flare" ) )
			UTIL_ProMessage( pPlayer, "I can't carry any more syringes.", 5, false );

		if ( gEvilImpulse101 || gSpecialItemGive )
		{
			UTIL_Remove( this );
		}
		return;
	}

	if ( pev->iuser4 == 1 )
		pev->iuser4 = 0;

	// Flare
	if ( FClassnameIs( edict(), "weapon_flare" ) )
	{
		if ( pev->iuser1 == 1 )
		{
			UTIL_ProMessage( pPlayer, "It's too hot to pick up", 5, false );
			return;
		}
	}

	if ( FClassnameIs( edict(), "weapon_flare" ) && pev->iuser1 == 2 )
	{
		if ( pev->iuser1 == 2 )
		{
			UTIL_ProMessage( pPlayer, "The flare is burnt out", 5, false );
			return;
		}

		if ( pev->rendermode == kRenderTransAdd && pev->renderamt == 0 )
			return;
	}

	// Inventory
	if ( script && pev->model && pev->classname && !FStrEq( STRING( pev->classname ), "weapon_dualweapon" ) && !FStrEq( STRING( pev->classname ), "weapon_action" ) )
	{
		if ( !pPlayer->HasNamedPlayerItem( STRING( pev->classname ) ) )
		{
			if ( pPlayer->AddItemToInventory( script, pev->model, pev->classname ) )
			{
			}
			else
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );
				return;
			}
		}
	}

	if (pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );

		bool hasitalready = false;
		int k;

		// statistics
		if ( !FClassnameIs( edict(), "weapon_action" ) && !FClassnameIs( edict(), "weapon_dualweapon" ) )
		{
			for ( k = 1; k <= 32; k++ )
			{
				if ( pPlayer->m_iOwnedWeaponsList[k] == m_iId )
				{
					hasitalready = true;
					break;
				}
			}

			if ( !hasitalready )
			{
				for ( k = 1; k <= 32; k++ )
				{
					if ( !pPlayer->m_iOwnedWeaponsList[k] )
					{
						pPlayer->m_iOwnedWeaponsList[k] = m_iId;
						pPlayer->playerstats.weaponscollected++;
						break;
					}
				}
			}
		}

		if ( !gSpecialItemGive && !FStrEq( STRING( pev->classname ), "weapon_dualweapon" ) )
		{
			if ( pev->max_health != 1 )
			{
				if ( pPlayer )
					EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "weapons/weapon_get.wav", 1, 0.8);
			}
		}
	}

	SUB_UseTargets( pOther, USE_TOGGLE, 0 ); // UNDONE: when should this happen?
}

BOOL CanAttack( float attack_time, float curtime, BOOL isPredicted )
{
#if defined( CLIENT_WEAPONS )
	if ( !isPredicted )
#else
	if ( 1 )
#endif
	{
		return ( attack_time <= curtime ) ? TRUE : FALSE;
	}
	else
	{
		return ( attack_time <= 0.0 ) ? TRUE : FALSE;
	}
}

bool CBasePlayerWeapon::CanCurrentlySuicide( void )
{
	if ( !m_fInReload && m_bCanSuicide && m_flNextPrimaryAttack < gpGlobals->time && m_flNextSecondaryAttack < gpGlobals->time )
		return true;

	return false;
}

bool wasmoving;
bool wasjumping;
bool wasducking;
bool wassprinting;
int minimum;

void CBasePlayerWeapon::CalculateWeaponAccuracy( void )
{
	if ( !m_pPlayer )
		return;

	float speed;

	speed = m_pPlayer->pev->velocity.Length2D();

	if ( speed > 10 && !( m_pPlayer->pev->flags & (1<<18) ) && !( m_pPlayer->pev->flags & FL_DUCKING ) )
	{
		if ( m_flCurrentKick < 3 )
		{
			minimum = 3;
			m_flCurrentKick = 3;
		}

		if ( !wasmoving )
		{
			m_flCurrentKick *= 1.5;
			wasmoving = true;
		}
	}
	else
	{
		if ( wasmoving )
			wasmoving = false;
	}

	if ( m_pPlayer->pev->flags & (1<<18) )
	{
		if ( m_flCurrentKick < 7 )
		{
			minimum = 7;
			m_flCurrentKick = 7;
		}

		if ( !wassprinting )
		{
			wassprinting = true;
			m_flCurrentKick *= 3;
		}
	}
	else
	{
		if ( wassprinting )
			wassprinting = false;
	}

	if ( m_pPlayer->pev->flags & FL_DUCKING )
	{
		if ( !wasducking )
		{
			wasducking = true;
			m_flCurrentKick /= 2;
		}

		if ( speed > 10 && m_flCurrentKick < 2 )
		{
			minimum = 2;
			m_flCurrentKick = 2;
		}
	}
	else
	{
		if ( wasducking )
			wasducking = false;
	}

	if ( !( m_pPlayer->pev->flags & FL_ONGROUND ) )
	{
		if ( !wasjumping )
		{
			wasjumping = true;
			m_flCurrentKick += 8;
		}

		if ( m_flCurrentKick < 8 )
		{
			minimum = 8;
			m_flCurrentKick = 8;
		}
	}
	else
	{
		if ( wasjumping )
			wasjumping = false;
	}

	if ( !wasjumping && !wasmoving && !wasducking && !wassprinting )
		minimum = 0;

	if ( m_iId == 15 )
		m_flCurrentKick = 7;

	if ( m_flCurrentKick < 1 )
		m_AccuracyVector = g_vecZero;
	else if ( m_flCurrentKick >= 1 && m_flCurrentKick < 2 )
		m_AccuracyVector = VECTOR_CONE_1DEGREES;
	else if ( m_flCurrentKick >= 2 && m_flCurrentKick < 3 )
		m_AccuracyVector = VECTOR_CONE_2DEGREES;
	else if ( m_flCurrentKick >= 3 && m_flCurrentKick < 4 )
		m_AccuracyVector = VECTOR_CONE_3DEGREES;
	else if ( m_flCurrentKick >= 4 && m_flCurrentKick < 5 )
		m_AccuracyVector = VECTOR_CONE_4DEGREES;
	else if ( m_flCurrentKick >= 5 && m_flCurrentKick < 6 )
		m_AccuracyVector = VECTOR_CONE_5DEGREES;
	else if ( m_flCurrentKick >= 6 && m_flCurrentKick < 7 )
		m_AccuracyVector = VECTOR_CONE_6DEGREES;
	else if ( m_flCurrentKick >= 7 && m_flCurrentKick < 8 )
		m_AccuracyVector = VECTOR_CONE_7DEGREES;
	else if ( m_flCurrentKick >= 8 && m_flCurrentKick < 9 )
		m_AccuracyVector = VECTOR_CONE_8DEGREES;
	else if ( m_flCurrentKick >= 9 && m_flCurrentKick < 10 )
		m_AccuracyVector = VECTOR_CONE_9DEGREES;
	else if ( m_flCurrentKick >= 10 )
		m_AccuracyVector = VECTOR_CONE_10DEGREES;

	if ( m_flNextKickReductionTime < gpGlobals->time )
	{
		m_flCurrentKick -= 1;
		m_flNextKickReductionTime = gpGlobals->time + 0.1;
	}

	if ( m_flCurrentKick < 0 )
		m_flCurrentKick = 0;

	if ( minimum > m_flCurrentKick )
		m_flCurrentKick = minimum;

	if ( m_flCurrentKick > 10 )
		m_flCurrentKick = 10;
}

void CBasePlayerWeapon::ItemPostFrame( void )
{
	if ( !m_iPlayEmptySound )
	{
		if ( m_iClip > 0 )
			m_iPlayEmptySound = 1;
	}

	Fidget();

	if ( FClassnameIs( edict(), "weapon_dualweapon" ) )
	{
		CDualWeapon *pWeapon = GetClassPtr( (CDualWeapon *)pev );

		if ( m_bShotLooping && pWeapon )
		{
			if ( pWeapon->m_iRightWeaponID == 14 || pWeapon->m_iRightWeaponID == 23 || pWeapon->m_iRightWeaponID == 27 )
			{
				if ( !( m_pPlayer->pev->button & IN_ATTACK2 ) || m_iSprintState )
				{
					m_bShotLooping = false;

					if ( shoot_loop_sound )
					{
						if ( g_pGameRules->IsMultiplayer() )
							STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, STRING( shoot_loop_sound ) );
						else
							PlayWeaponSound_guess( STRING( shoot_loop_sound ), 2, true, m_pPlayer->EarPosition(), m_pPlayer, true );
					}

					if ( shoot_loop_sound_end )
					{
						if ( g_pGameRules->IsMultiplayer() )
							EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, STRING( shoot_loop_sound_end ), 1, 0.8, 0, 100 );
						else
							PlayWeaponSound_guess( STRING( shoot_loop_sound_end ), 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
					}
				}
			}
		}
	}
	else
	{
		if ( m_bShotLooping )
		{
			if ( !m_pPlayer->IsAlive() || m_iClip <= 0 || m_fInReload || !( m_pPlayer->pev->button & IN_ATTACK ) || m_iSprintState )
			{
				m_bShotLooping = false;

				if ( shoot_loop_sound )
				{
					if ( g_pGameRules->IsMultiplayer() )
						STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, STRING( shoot_loop_sound ) );
					else
						PlayWeaponSound_guess( STRING( shoot_loop_sound_end ), 2, true, m_pPlayer->EarPosition(), m_pPlayer, true );
				}

				if ( shoot_loop_sound_end )
				{
					if ( g_pGameRules->IsMultiplayer() )
						EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_WEAPON, STRING( shoot_loop_sound_end ), 1, 0.8, 0, 100 );
					else
						PlayWeaponSound_guess( STRING( shoot_loop_sound_end ), 0, true, m_pPlayer->EarPosition(), m_pPlayer, true );
				}
			}
		}
	}

	// rage
	if ( m_fNextRageDecrease < gpGlobals->time )
	{
		m_fNextRageDecrease = GetNextRageDecreaseTime() + gpGlobals->time;

		if ( m_iRageLevel > 0 )
			m_iRageLevel--;
	}

	if ( m_iRageNeeded <= 0 )
		RageReset();

	if ( m_iRageLevel < 0 )
		m_iRageLevel = 0;

	if ( m_iRageLevel > m_iRageNeeded )
		m_iRageLevel = m_iRageNeeded;

	// update the ammo display when the reserve changes
	if ( old_m_iAmmo != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( iMaxClip() );
			WRITE_SHORT( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );
		MESSAGE_END();

		old_m_iAmmo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
	}

	if ((m_fInReload) && ( m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase() ) && !( iFlags() & 32 ) )
	{
		// infinite clip
		if ( m_iId == 23 )
		{
			// complete the reload.
			m_iClip = iMaxClip();

			m_fInReload = FALSE;
		}
		else
		{
			// complete the reload.
			int j = min( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

			// Add them to the clip
			m_iClip += j;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

			m_pPlayer->TabulateAmmo();

			m_fInReload = FALSE;
		}
	}

	if ((m_pPlayer->pev->button & IN_ATTACK2) && m_pPlayer->pev->fuser3 <= 0 && CanAttack( m_flNextSecondaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( !m_pPlayer->IsOnLadder() )
		{
			if ( pszAmmo2() && !m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()] )
			{
				m_fFireOnEmpty = TRUE;
			}

			m_pPlayer->TabulateAmmo();
			SecondaryAttack();
		}
	}
	else if ((m_pPlayer->pev->button & IN_CANCEL) && m_pPlayer->pev->fuser3 <= 0 && CanAttack( m_pPlayer->m_flNextAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( !m_pPlayer->IsOnLadder() )
		{
			m_pPlayer->TabulateAmmo();
			TertiaryAttack();
		}
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) && m_pPlayer->pev->fuser3 <= 0 && CanAttack( m_flNextPrimaryAttack, gpGlobals->time, UseDecrement() ) )
	{
		if ( !m_pPlayer->IsOnLadder() )
		{
			if ( (m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && !m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()] ) )
			{
				m_fFireOnEmpty = TRUE;
			}

			m_pPlayer->TabulateAmmo();
			PrimaryAttack();
		}
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload && !FClassnameIs( edict(), "weapon_dualweapon" ) )
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		if ( !m_pPlayer->IsOnLadder() )
			Reload();
	}
	else if ( m_pPlayer->pev->button & IN_RELOAD && !m_fInReload && FClassnameIs( edict(), "weapon_dualweapon" ) )
	{
		// the dual weapon reloads with or without a clip
		if ( !m_pPlayer->IsOnLadder() )
			Reload();
	}
	else if ( !(m_pPlayer->pev->button & (IN_ATTACK|IN_CANCEL|IN_ATTACK2) ) )
	{
		// no fire buttons down

		m_fFireOnEmpty = FALSE;

/*		if ( !IsUseable() && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
		{
			// weapon isn't useable, switch.
			if ( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_pPlayer, this ) )
			{
				m_flNextPrimaryAttack = ( UseDecrement() ? 0.0 : gpGlobals->time ) + 0.3;
				return;
			}
		}
		else*/
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if ( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < ( UseDecrement() ? 0.0 : gpGlobals->time ) )
			{
			//	Reload();
				return;
			}
		}

		if ( m_iId == 17 )
			WeaponIdle( );
		return;
	}

	// catch all
	if ( ShouldWeaponIdle() )
	{
		if ( m_iId == 17 )
			WeaponIdle();
	}
}

void CBasePlayerItem::DestroyItem( void )
{
	if ( m_pPlayer )
	{
		// if attached to a player, remove.
		m_pPlayer->RemovePlayerItem( this );
	}

	Kill( );
}

int CBasePlayerItem::AddToPlayer( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer;

	return TRUE;
}

void CBasePlayerItem::Drop( void )
{
	// Wargon: the item is not usable.
	SetUse( NULL );
	m_iCurrCaps = CBaseEntity::ObjectCaps();

	SetTouch( NULL );
	SetThink(&CBasePlayerItem::SUB_Remove);
	SetNextThink( 0.1 );
}

void CBasePlayerItem::Kill( void )
{
	// Wargon: the item is not usable.
	SetUse( NULL );
	m_iCurrCaps = CBaseEntity::ObjectCaps();

	SetTouch( NULL );

	pev->armorvalue = 0;

	if ( FClassnameIs( edict(), "weapon_flare" ) )
	{
		STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

		pev->renderfx = 0;
		pev->rendermode = kRenderTransAdd;
		pev->renderamt = 0;
		pev->body = 0;
		SetThink(&CBasePlayerItem::SUB_Remove);
		SetNextThink( 0 );
	}
	else
	{
		SetThink(&CBasePlayerItem::SUB_Remove);
		SetNextThink( 0.1 );
	}
}

void CBasePlayerItem::Holster( void )
{
	m_pPlayer->pev->viewmodel = 0;
	m_pPlayer->pev->weaponmodel = 0;
}

void CBasePlayerItem::AttachToPlayer ( CBasePlayer *pPlayer )
{
	// found a hoodie
	if ( FClassnameIs( edict(), "weapon_axe" ) )
	{
		if ( !( pPlayer->m_iFoundHoodies & 0x80000 ) && !pPlayer->CheckingEndingOrUnlockables( 0x80000, false, false ) )
			pPlayer->m_iFoundHoodies |= 0x80000;
	}

	// the burning flare stays visible
	if ( FClassnameIs( edict(), "weapon_flare" ) && pev->iuser1 == 1 )
	{
		STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

		pev->renderfx = 0;
		pev->rendermode = kRenderTransAdd;
		pev->renderamt = 0;
		pev->body = 0;
	}
	else
		pev->effects = EF_NODRAW; // ??

	pev->armorvalue = 0;

	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->aiment = pPlayer->edict();
	// buz: dont override gun's modelindex (hack)
	// pev->modelindex = 0;// server won't send down to clients if modelindex == 0
	// pev->model = iStringNull;
	pev->owner = pPlayer->edict();
	SetNextThink( 0.1 );
//	SetThink(NULL); // buz test
	SetTouch( NULL );

	// Wargon: the item is not usable.
	SetUse( NULL );
	m_iCurrCaps = CBaseEntity::ObjectCaps();
}

//LRC
void CBasePlayerWeapon :: SetNextThink( float delay )
{
	m_fNextThink = UTIL_WeaponTimeBase() + delay;
	pev->nextthink = m_fNextThink;
}


// CALLED THROUGH the newly-touched weapon's instance. The existing player weapon is pOriginal
int CBasePlayerWeapon::AddDuplicate( CBasePlayerItem *pOriginal, CBasePlayer *pPlayer )
{
/*	if ( m_iDefaultAmmo )
	{
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	}
	else
	{
		// a dead player dropped this.
		return ExtractClipAmmo( (CBasePlayerWeapon *)pOriginal );
	}*/
	if ( FClassnameIs( edict(), "weapon_flare" ) && pev->iuser1 == 1 )
	{
		if ( m_pPlayer )
			STOP_SOUND( ENT( m_pPlayer->pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

		pev->renderfx = 0;
		pev->rendermode = kRenderTransAdd;
		pev->renderamt = 0;
		pev->body = 0;
	}
	else
		pev->effects = EF_NODRAW;

	if ( pev->armorvalue == 666 )
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	else
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
}


int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	int bResult = CBasePlayerItem::AddToPlayer( pPlayer );

	pPlayer->pev->weapons |= (1<<m_iId);

	if ( !m_iPrimaryAmmoType )
	{
		m_iPrimaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}


	if (bResult)
		return AddWeapon( );
	return FALSE;
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;
	if ( pPlayer->m_pActiveItem == this )
	{
		if ( pPlayer->m_fOnTarget )
			state = WEAPON_IS_ONTARGET;
		else
			state = 1;
	}

	// Forcing send of all data!
	if ( !pPlayer->m_fWeapon )
	{
		bSend = TRUE;
	}

	// This is the current or last weapon, so the state will need to be updated
	if ( this == pPlayer->m_pActiveItem ||
		 this == pPlayer->m_pClientActiveItem )
	{
		if ( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem )
		{
			bSend = TRUE;
		}
	}

	// If the ammo, state, or fov has changed, update the weapon
	if ( m_iClip != m_iClientClip ||
		 state != m_iClientWeaponState ||
		 pPlayer->m_iFOV != pPlayer->m_iClientFOV )
	{
		bSend = TRUE;
	}

	if ( bSend )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pPlayer->pev );
			WRITE_BYTE( state );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( m_iClip );
		MESSAGE_END();

		m_iClientClip = m_iClip;
		m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if ( m_pNext )
		m_pNext->UpdateClientData( pPlayer );

	return 1;
}

void CBasePlayerWeapon::SendWeaponBody( CBasePlayer *pPlayer, int body )
{
	if ( pPlayer )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWeaponBody, NULL, pPlayer->pev );
			WRITE_BYTE( body );
		MESSAGE_END();
	}
}

void CBasePlayerWeapon::SendWeaponAnim( int iAnim, int skiplocal, int body )
{
//	if ( UseDecrement() )
//		skiplocal = 1;
//	else
		skiplocal = 0; // buz: predictig system is disabled

	m_pPlayer->m_iCurrentWeaponAnimation = iAnim;
	m_pPlayer->pev->weaponanim = iAnim;

#if defined( CLIENT_WEAPONS )
	if ( skiplocal && ENGINE_CANSKIP( m_pPlayer->edict() ) )
		return;
#endif

	MESSAGE_BEGIN( MSG_ONE, SVC_WEAPONANIM, NULL, m_pPlayer->pev );
		WRITE_BYTE( iAnim );						// sequence number
		WRITE_BYTE( pev->body );					// weaponmodel bodygroup.
	MESSAGE_END();
}

BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;

	if ( pev->armorvalue == 666 && !m_pPlayer->HasPlayerItem( this ) )
	{
		pev->armorvalue = 0;
		return TRUE;
	}

	if (iMaxClip < 1)
	{
		m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry, false );
	}
	else if (m_iClip == 0)
	{
		int i;
		i = min( m_iClip + iCount, iMaxClip ) - m_iClip;
		m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry, false );
	}
	else
	{
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry, false );
	}

	// m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] = iMaxCarry; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iPrimaryAmmoType = iIdAmmo;
		if (m_pPlayer->HasPlayerItem( this ) )
		{
			// play the "got ammo" sound only if we gave some ammo to a player that already had this gun.
			// if the player is just getting this gun for the first time, DefaultTouch will play the "picked up gun" sound for us.
			if ( pev->max_health != 1 )
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "weapons/item_get.wav", 1, 0.8);
		}
	}

	return iIdAmmo > 0 ? TRUE : FALSE;
}


BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax, false );

	//m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] = iMax; // hack for testing

	if (iIdAmmo > 0)
	{
		m_iSecondaryAmmoType = iIdAmmo;

		if ( pev->max_health != 1 )
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "weapons/item_get.wav", 1, 0.8);
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

//=========================================================
// IsUseable - this function determines whether or not a
// weapon is useable by the player in its current state.
// (does it have ammo loaded? do I have any ammo for the
// weapon?, etc)
//=========================================================
BOOL CBasePlayerWeapon :: IsUseable( void )
{
	if (m_iClip <= 0 && (m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] <= 0 && iMaxAmmo1() != -1))
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CBasePlayerWeapon :: CanDeploy( void )
{
//	BOOL bHasAmmo = 0;

	if ( m_pPlayer && m_pPlayer->IsOnLadder() )
		return FALSE;

	if ( !pszAmmo1() )
	{
		// this weapon doesn't use ammo, can always deploy.
		return TRUE;
	}

/*	if ( pszAmmo1() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if ( pszAmmo2() )
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= 1;
	}
	if (!bHasAmmo)
	{
		return FALSE;
	}*/

	return TRUE;
}

// buz
float GetSequenceTime( void *pmodel, int seqnum );

// buz
// precondition: item's pev->modelindex should be set to view model index
void CBasePlayerWeapon :: WeaponDelay(int iAnim)
{
	float delay = GetSequenceTime( GET_MODEL_PTR(ENT(pev)), iAnim );
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + delay;
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_pPlayer->m_flNextAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + delay;
}

void CBasePlayerWeapon :: WeaponDelayTime( float time )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + time;
	m_flNextSecondaryAttack = m_flNextPrimaryAttack = m_pPlayer->m_flNextAttack;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + time;
}


BOOL CBasePlayerWeapon :: DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal /* = 0 */, int body )
{
	if (!CanDeploy( ))
		return FALSE;

	if ( m_iClip == -666 )
		m_iClip = 0;

	if ( !m_pPlayer )
		return FALSE;

	// custom arms of the map
	if ( !m_pPlayer->m_bCustomSleeveApplied )
	{
		m_pPlayer->m_bCustomSleeveApplied = true;

		if ( g_pWorld && g_pWorld->m_sCustomSleeve )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCustomSlv, NULL, m_pPlayer->pev );
				WRITE_STRING( STRING( g_pWorld->m_sCustomSleeve ) );
			MESSAGE_END();
		}

		if ( g_pWorld && g_pWorld->m_sCustomFingers )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCustomFng, NULL, m_pPlayer->pev );
				WRITE_STRING( STRING( g_pWorld->m_sCustomFingers ) );
			MESSAGE_END();
		}

		if ( g_pWorld && g_pWorld->m_sCustomGloves_guess )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCustomGlv, NULL, m_pPlayer->pev );
				WRITE_STRING( STRING( g_pWorld->m_sCustomGloves_guess ) );
			MESSAGE_END();
		}
	}

	if ( custom_v_model )
	{
		szViewModel = (char *)STRING( custom_v_model );
		ALERT( at_console, "Setting custom v model: %s\n", szViewModel );
	}

	m_bIronSights = false;

	// reset the recoil
	m_flNextKickReductionTime = gpGlobals->time;
	m_flCurrentKick = 0;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgWpnDeploy, NULL, m_pPlayer->pev );
	MESSAGE_END();

	// update the ammo display when the reserve changes
	if ( old_m_iAmmo != m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( iMaxClip() );
			WRITE_SHORT( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] );
		MESSAGE_END();

		old_m_iAmmo = m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType];
	}

	int pitch = 100 + RANDOM_LONG( -5, 5 );

	if ( pev->max_health != 1 )
	{
		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic1.wav", 1, 0.8, 0, pitch );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic2.wav", 1, 0.8, 0, pitch );
			break;
		case 2:
			EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic3.wav", 1, 0.8, 0, pitch );
			break;
		}
	}
	else
		pev->max_health = 0;

	m_pPlayer->TabulateAmmo();

	m_pPlayer->pev->viewmodel = MAKE_STRING(szViewModel);
	m_pPlayer->pev->weaponmodel = MAKE_STRING(szWeaponModel);
	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt );
	SendWeaponAnim( iAnim, skiplocal, body );

//	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
//	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;

	// buz: make weapon world model same as view model (it's anyway has EF_NODRAW flag)
	SET_MODEL(ENT(pev), szViewModel);
	//pev->modelindex = (*g_engfuncs.pfnModelIndex)(szViewModel);
	//pev->model = MAKE_STRING( szViewModel );

	// buz: deploy time based on animation length
	WeaponDelay(iAnim);

	return TRUE;
}

int CBasePlayerWeapon :: DefaultReload( int iClipSize, int iAnim, float fDelay, int body, bool infinitereload )
{
	if ( m_iId != 23 )
	{
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
			return FALSE;

		int j = min(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

		if (j == 0)
			return FALSE;
	}

	// buz: fDelay is now ignored, and replaced by animtime!
	WeaponDelay(iAnim);

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim( iAnim, UseDecrement() ? 1 : 0 );

	m_fInReload = TRUE;

//	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
	return TRUE;
}

BOOL CBasePlayerWeapon :: PlayEmptySound( void )
{
	if (m_iPlayEmptySound)
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_AUTO, "weapons/weapon_fire_empty.wav", 0.8, 0.8);
		m_iPlayEmptySound = 0;
		return 0;
	}
	else
	{
		EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_AUTO, "weapons/weapon_fire_empty_noclick.wav", 0.8, 0.8);
		m_iPlayEmptySound = 0;
		return 0;
	}
}

void CBasePlayerWeapon :: ResetEmptySound( void )
{
	m_iPlayEmptySound = 1;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::PrimaryAmmoIndex( void )
{
	return m_iPrimaryAmmoType;
}

//=========================================================
//=========================================================
int CBasePlayerWeapon::SecondaryAmmoIndex( void )
{
	return -1;
}

void CBasePlayerWeapon::Holster( void )
{
	m_fInReload = FALSE; // cancel any reload in progress.
	m_pPlayer->pev->viewmodel = 0;
	m_pPlayer->pev->weaponmodel = 0;

	int pitch = 100 + RANDOM_LONG( -5, 5 );

	switch ( RANDOM_LONG( 0, 2 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic1.wav", 1, 0.8, 0, pitch );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic2.wav", 1, 0.8, 0, pitch );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT( m_pPlayer->pev ), CHAN_AUTO, "weapons/sleeve_generic3.wav", 1, 0.8, 0, pitch );
		break;
	}
}

// Wargon: SaveData for usable ammo.
TYPEDESCRIPTION CBasePlayerAmmo::m_SaveData[] =
{
	DEFINE_FIELD( CBasePlayerAmmo, m_iCurrCaps, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerAmmo, CBaseEntity );

void CBasePlayerAmmo::Spawn( void )
{
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-4, -4, 0), Vector(4, 4, 8));
	UTIL_SetOrigin( this, pev->origin );

	if ( pev->iuser4 == 1 )
		SetTouch(&CBasePlayerAmmo:: DefaultTouch );

	pev->renderfx = 99;

	// Wargon: the ammo is usable.
	SetUse( &CBasePlayerAmmo::DefaultUse );
	m_iCurrCaps = CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE;
}

CBaseEntity* CBasePlayerAmmo::Respawn( void )
{
	pev->effects |= EF_NODRAW;
	SetTouch( NULL );

	// Wargon: the ammo is not usable.
	SetUse( NULL );
	m_iCurrCaps = CBaseEntity::ObjectCaps();

	UTIL_SetOrigin( this, g_pGameRules->VecAmmoRespawnSpot( this ) );// move to wherever I'm supposed to repawn.

	SetThink(&CBasePlayerAmmo:: Materialize );
	AbsoluteNextThink( g_pGameRules->FlAmmoRespawnTime( this ) );

	return this;
}

void CBasePlayerAmmo::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		//EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/suitchargeok1.wav", 1, ATTN_NORM, 0, 150 );
		pev->effects &= ~EF_NODRAW;
		pev->effects |= EF_MUZZLEFLASH;
	}

	if ( pev->iuser4 == 1 )
		SetTouch(&CBasePlayerAmmo:: DefaultTouch );

	// Wargon: the ammo is usable.
	SetUse( &CBasePlayerAmmo::DefaultUse );
	m_iCurrCaps = CBaseEntity::ObjectCaps() | FCAP_IMPULSE_USE;
}

void CBasePlayerAmmo :: DefaultTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pOther->pev );

	if ( !pPlayer )
		return;

	if ( pev->iuser4 != 1 )
	{
		if ( !( ( pPlayer->pev->button | pPlayer->m_afButtonPressed | pPlayer->m_afButtonReleased ) & IN_USE ) )
			return;
	}

	if (AddAmmo( pOther ))
	{
		if ( g_pGameRules && g_pGameRules->AmmoShouldRespawn( this ) == GR_AMMO_RESPAWN_YES )
		{
			Respawn();
		}
		else
		{
			SetTouch( NULL );

			// Wargon: the ammo is not usable.
			SetUse( NULL );
			m_iCurrCaps = CBaseEntity::ObjectCaps();

			SetThink(&CBasePlayerAmmo ::SUB_Remove);
			SetNextThink( 0.1 );
		}
	}
	else if ( gEvilImpulse101 || gSpecialItemGive )
	{
		// evil impulse 101 hack, kill always
		SetTouch( NULL );

		// Wargon: the ammo is not usable.
		SetUse( NULL );
		m_iCurrCaps = CBaseEntity::ObjectCaps();

		if ( FClassnameIs( edict(), "weapon_flare" ) && pev->iuser1 == 1 )
		{
			if ( pPlayer )
				STOP_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/flare/flare_burn.wav" );

			pev->renderfx = 0;
			pev->rendermode = kRenderTransAdd;
			pev->renderamt = 0;
			pev->body = 0;
		}
		else
		{
			SetThink(&CBasePlayerAmmo ::SUB_Remove);
			SetNextThink( 0.1 );
		}
	}
}

//=========================================================
// called by the new item with the existing item as parameter
//
// if we call ExtractAmmo(), it's because the player is picking up this type of weapon for
// the first time. If it is spawned by the world, m_iDefaultAmmo will have a default ammo amount in it.
// if  this is a weapon dropped by a dying player, has 0 m_iDefaultAmmo, which means only the ammo in
// the weapon clip comes along.
//=========================================================
int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iReturn = 0;

	if ( pWeapon->m_iClip == -666 && iMaxClip() != -1 )
		return TRUE;

	if ( pszAmmo1() != NULL )
	{
		// blindly call with m_iDefaultAmmo. It's either going to be a value or zero. If it is zero,
		// we only get the ammo in the weapon's clip, which is what we want.
		iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( 0, (char *)pszAmmo2(), iMaxAmmo2() );
	}

	return iReturn;
}

//=========================================================
// called by the new item's class with the existing item as parameter
//=========================================================
int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int			iAmmo;

	if ( pWeapon->m_iClip == -666 )
		return TRUE;

	if ( m_iClip == WEAPON_NOCLIP )
	{
		iAmmo = 0;// guns with no clips always come empty if they are second-hand
	}
	else
	{
		iAmmo = m_iClip;
	}

	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1(), false ); // , &m_iPrimaryAmmoType
}

//=========================================================
// RetireWeapon - no more ammo for this gun, put it away.
//=========================================================
void CBasePlayerWeapon::RetireWeapon( void )
{
	// first, no viewmodel at all.
	m_pPlayer->pev->viewmodel = iStringNull;
	m_pPlayer->pev->weaponmodel = iStringNull;
	//m_pPlayer->pev->viewmodelindex = NULL;

	g_pGameRules->GetNextBestWeapon( m_pPlayer, this );
}

//=========================================================
// LRC - remove the specified ammo from this gun
//=========================================================
void CBasePlayerWeapon::DrainClip(CBasePlayer* pPlayer, BOOL keep, int i9mm, int i357, int iBuck, int iBolt, int iARGren, int iRock, int iUranium, int iSatchel, int iSnark, int iTrip, int iGren,
								  int iAps, int iBarret, int iAk, int iAsval, int iRpk, int iGroza)
{
	int iPAI = PrimaryAmmoIndex();
	int iAmt;
	if (iPAI == -1) return;
	else if (iPAI == pPlayer->GetAmmoIndex("9mm"))			iAmt = i9mm;
	else if (iPAI == pPlayer->GetAmmoIndex("357"))			iAmt = i357;
	else if (iPAI == pPlayer->GetAmmoIndex("buckshot"))		iAmt = iBuck;
	else if (iPAI == pPlayer->GetAmmoIndex("bolts"))		iAmt = iBolt;
	else if (iPAI == pPlayer->GetAmmoIndex("ARgrenades"))	iAmt = iARGren;
	else if (iPAI == pPlayer->GetAmmoIndex("uranium"))		iAmt = iUranium;
	else if (iPAI == pPlayer->GetAmmoIndex("rockets"))		iAmt = iRock;
	else if (iPAI == pPlayer->GetAmmoIndex("Satchel Charge")) iAmt = iSatchel;
	else if (iPAI == pPlayer->GetAmmoIndex("Snarks"))		iAmt = iSnark;
	else if (iPAI == pPlayer->GetAmmoIndex("Trip Mine"))	iAmt = iTrip;
	else if (iPAI == pPlayer->GetAmmoIndex("Hand Grenade")) iAmt = iGren;
	// buz: paranoia ammos
	else if (iPAI == pPlayer->GetAmmoIndex("aps"))		iAmt = iAps;
	else if (iPAI == pPlayer->GetAmmoIndex("barret"))	iAmt = iBarret;
	else if (iPAI == pPlayer->GetAmmoIndex("ak"))		iAmt = iAk;
	else if (iPAI == pPlayer->GetAmmoIndex("asval"))	iAmt = iAsval;
	else if (iPAI == pPlayer->GetAmmoIndex("rpk"))		iAmt = iRpk;
	else if (iPAI == pPlayer->GetAmmoIndex("groza"))		iAmt = iGroza;
	else return;

	if (iAmt > 0)
	{
		m_iClip -= iAmt;
		if (m_iClip < 0) m_iClip = 0;
	}
	else if (iAmt >= -1)
	{
		m_iClip = 0;
	}

	// if we're not keeping the gun, transfer the remainder of its clip
	// into the main ammo store
	if (!keep)
		pPlayer->m_rgAmmo[iPAI] = m_iClip;
}

void CBasePlayerWeapon :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "customv"))
	{
		custom_v_model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "customw"))
	{
		custom_w_model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "customscript"))
	{
		custom_script = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

//*********************************************************
// weaponbox code:
//*********************************************************

LINK_ENTITY_TO_CLASS( weaponbox, CWeaponBox );

TYPEDESCRIPTION	CWeaponBox::m_SaveData[] =
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CWeaponBox, weaponmodel, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

//=========================================================
//
//=========================================================
void CWeaponBox::Precache( void )
{
	PRECACHE_MODEL("models/w_weaponbox.mdl");
}

//=========================================================
//=========================================================
void CWeaponBox :: KeyValue( KeyValueData *pkvd )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else
	{
		ALERT ( at_debug, "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
	}
}

//=========================================================
// CWeaponBox - Spawn
//=========================================================
void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	pev->renderfx = 99;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );

	SET_MODEL( ENT(pev), "models/w_weaponbox.mdl");
}

//=========================================================
// CWeaponBox - Kill - the think function that removes the
// box from the world.
//=========================================================
void CWeaponBox::Kill( void )
{
	CBasePlayerItem *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SetThink(&CBasePlayerItem::SUB_Remove);
			pWeapon->SetNextThink( 0.1 );
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

//=========================================================
// CWeaponBox - Touch: try to add my contents to the toucher
// if the toucher is a player.
//=========================================================
void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) )
	{
		return;
	}

	if ( !pOther->IsPlayer() )
	{
		// only players may touch a weaponbox.
		return;
	}

	if ( !pOther->IsAlive() )
	{
		// no dead guys.
		return;
	}

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

	if ( pev->iuser4 != 1 )
	{
		if ( !( ( pPlayer->pev->button | pPlayer->m_afButtonPressed | pPlayer->m_afButtonReleased ) & IN_USE ) )
			return;
	}

	// only one lantern
	if ( !strcmp( STRING( pev->model ), "models/weapons/lantern/w_lantern.mdl" ) )
	{
		if ( pPlayer->HasNamedPlayerItem( "weapon_lantern" ) )
			return;
	}

// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// there's some ammo of this type.
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), MaxAmmoCarry( m_rgiszAmmo[ i ] ), false );

			//ALERT ( at_console, "Gave %d rounds of %s\n", m_rgAmmo[i], STRING(m_rgiszAmmo[i]) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

// go through my weapons and try to give the usable ones to the player.
// it's important the the player be given ammo first, so the weapons code doesn't refuse
// to deploy a better weapon that the player may pick up because he has no ammo for it.
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerItem *pItem;

			// have at least one weapon in this slot
			while ( m_rgpPlayerItems[ i ] )
			{
				//ALERT ( at_console, "trying to give %s\n", STRING( m_rgpPlayerItems[ i ]->pev->classname ) );

				pItem = m_rgpPlayerItems[ i ];
				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box

				// the weapon keeps the world model and the message of the box
				pItem->strwmodel = weaponmodel;
				pItem->pev->model = pItem->strwmodel;

				pItem->pev->message = pev->message;
				pItem->pev->flags |= pev->flags;

				if ( !pPlayer->InventoryAddCheck( pItem ) )
					return;

				if ( pPlayer->AddPlayerItem( pItem ) )
				{
					pItem->AttachToPlayer( pPlayer );
				}
			}
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "weapons/weapon_get.wav", 1, 0.8 );
	SetTouch(NULL);
	UTIL_Remove(this);
}

//=========================================================
// CWeaponBox - PackWeapon: Add this weapon to the box
//=========================================================
int CWeaponBox::PackWeapon( CBasePlayerItem *pWeapon, int wmodel )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) )
	{
		return FALSE;// box can only hold one of each weapon type
	}

	if ( pWeapon->m_pPlayer )
	{
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
		{
			// failed to unhook the weapon from the player!
			return FALSE;
		}
	}

	weaponmodel = wmodel;

	int iWeaponSlot = pWeapon->iItemSlot();

	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();
	pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;
	pWeapon->slotnumber = 0;

	//ALERT ( at_console, "packed %s\n", STRING(pWeapon->pev->classname) );

	return TRUE;
}

//=========================================================
// CWeaponBox - PackAmmo
//=========================================================
BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		ALERT ( at_debug, "NULL String in PackAmmo!\n" );
		return FALSE;
	}

	iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		//ALERT ( at_console, "Packed %d rounds of %s\n", iCount, STRING(iszName) );
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox - GiveAmmo
//=========================================================
int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex)
				*pIndex = i;

			int iAdd = min( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;

				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex)
			*pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;

		return i;
	}
	ALERT( at_debug, "out of named ammo slots\n");
	return i;
}

//=========================================================
// CWeaponBox::HasWeapon - is a weapon of this type already
// packed in this box?
//=========================================================
BOOL CWeaponBox::HasWeapon( CBasePlayerItem *pCheckItem )
{
	CBasePlayerItem *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) ))
		{
			return TRUE;
		}
		pItem = pItem->m_pNext;
	}

	return FALSE;
}

//=========================================================
// CWeaponBox::IsEmpty - is there anything in this box?
//=========================================================
BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			return FALSE;
		}
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// still have a bit of this type of ammo
			return FALSE;
		}
	}

	return TRUE;
}

//=========================================================
//=========================================================
void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-16, -16, 0);
	pev->absmax = pev->origin + Vector(16, 16, 16);
}


void CBasePlayerWeapon::PrintState( void )
{
	ALERT( at_debug, "primary:  %f\n", m_flNextPrimaryAttack );
	ALERT( at_debug, "idle   :  %f\n", m_flTimeWeaponIdle );

//	ALERT( at_debug, "nextrl :  %f\n", m_flNextReload );
//	ALERT( at_debug, "nextpum:  %f\n", m_flPumpTime );

//	ALERT( at_debug, "m_frt  :  %f\n", m_fReloadTime );
	ALERT( at_debug, "m_finre:  %i\n", m_fInReload );
//	ALERT( at_debug, "m_finsr:  %i\n", m_fInSpecialReload );

	ALERT( at_debug, "m_iclip:  %i\n", m_iClip );
}

TYPEDESCRIPTION	CMobile::m_SaveData[] =
{
	DEFINE_FIELD( CMobile, m_bPhoneCall, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMobile, m_bDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMobile, m_bWaitingToFix, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CMobile, CBasePlayerWeapon );

TYPEDESCRIPTION	CRifle::m_SaveData[] =
{
	DEFINE_FIELD( CRifle, m_bRaised, FIELD_BOOLEAN ),
	DEFINE_FIELD( CRifle, m_bWaitingToReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( CRifle, m_iWantToEjectShell, FIELD_INTEGER ),
	DEFINE_FIELD( CRifle, m_flNextShellEjectTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CRifle, CBasePlayerWeapon );

TYPEDESCRIPTION	CShotgun::m_SaveData[] =
{
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( CShotgun, m_fInSpecialReload, FIELD_INTEGER ),
	DEFINE_FIELD( CShotgun, m_flNextReload, FIELD_TIME ),
	DEFINE_FIELD( CShotgun, m_flPumpTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CShotgun, CBasePlayerWeapon );

TYPEDESCRIPTION	CSwitchblade::m_SaveData[] =
{
	DEFINE_FIELD( CSwitchblade, b_KnifeStance, FIELD_BOOLEAN ),
	DEFINE_FIELD( CSwitchblade, m_iBloodAmount, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CSwitchblade, CBasePlayerWeapon );

TYPEDESCRIPTION	CSyringe::m_SaveData[] =
{
	DEFINE_FIELD( CSyringe, b_HasUsed, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CSyringe, CBasePlayerWeapon );

TYPEDESCRIPTION	CGlock::m_SaveData[] =
{
	DEFINE_FIELD( CGlock, m_bTaclight, FIELD_BOOLEAN ),
	DEFINE_FIELD( CGlock, m_bWaitingToFix, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CGlock, CBasePlayerWeapon );

TYPEDESCRIPTION	CRevolver::m_SaveData[] =
{
	DEFINE_FIELD( CRevolver, m_bReloading, FIELD_BOOLEAN ),
	DEFINE_FIELD( CRevolver, m_bWantToEndReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( CRevolver, m_bInsertBullet, FIELD_BOOLEAN ),
	DEFINE_FIELD( CRevolver, m_flInsertTime, FIELD_TIME ),
	DEFINE_FIELD( CRevolver, m_iRotation, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRevolver, CBasePlayerWeapon );

TYPEDESCRIPTION	CBrowningWheelchair::m_SaveData[] =
{
	DEFINE_FIELD( CBrowningWheelchair, m_bWaitingToRedeploy, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBrowningWheelchair, m_vAimingVector, FIELD_VECTOR ),
	DEFINE_FIELD( CBrowningWheelchair, m_vEndVector, FIELD_VECTOR ),
};
IMPLEMENT_SAVERESTORE( CBrowningWheelchair, CBasePlayerWeapon );

TYPEDESCRIPTION	CM16::m_SaveData[] =
{
	DEFINE_FIELD( CM16, b_BurstMode, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CM16, CBasePlayerWeapon );

TYPEDESCRIPTION	CFlare::m_SaveData[] =
{
	DEFINE_FIELD( CFlare, m_bThrowing, FIELD_BOOLEAN ),
	DEFINE_FIELD( CFlare, m_fNextBurnTime, FIELD_TIME ),
	DEFINE_FIELD( CFlare, m_bThrown, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CFlare, CBasePlayerWeapon );

TYPEDESCRIPTION	CDualWeapon::m_SaveData[] =
{
	DEFINE_FIELD( CDualWeapon, m_sLeftHandWeaponModel, FIELD_STRING ),
	DEFINE_FIELD( CDualWeapon, m_ShotDelayL, FIELD_FLOAT ),
	DEFINE_FIELD( CDualWeapon, m_iDamageL, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeapon, m_ShotDelayR, FIELD_FLOAT ),
	DEFINE_FIELD( CDualWeapon, m_iDamageR, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeapon, m_iRightWeaponID, FIELD_INTEGER ),
	DEFINE_FIELD( CDualWeapon, m_iLeftWeaponID, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CDualWeapon, CBasePlayerWeapon );

// CS weapons

TYPEDESCRIPTION	CCSAug::m_SaveData[] =
{
	DEFINE_FIELD( CCSAug, m_bZoomed, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CCSAug, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSAWP::m_SaveData[] =
{
	DEFINE_FIELD( CCSAWP, m_bZoomed, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CCSAWP, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSBomb::m_SaveData[] =
{
	DEFINE_FIELD( CCSBomb, m_bPlanting, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCSBomb, m_bPlanted, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCSBomb, m_fPlantProgress, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CCSBomb, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSElites::m_SaveData[] =
{
	DEFINE_FIELD( CCSElites, m_bRight, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CCSElites, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSFamas::m_SaveData[] =
{
	DEFINE_FIELD( CCSFamas, m_bBurst, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCSFamas, shoot_num, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CCSFamas, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSFlashbang::m_SaveData[] =
{
	DEFINE_FIELD( CCSFlashbang, m_bPrimed, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCSFlashbang, m_bThrown, FIELD_BOOLEAN ),
	DEFINE_FIELD( CCSFlashbang, m_flGrenadeThrowTime, FIELD_TIME ),
	DEFINE_FIELD( CCSFlashbang, m_flRedrawTime, FIELD_TIME ),
};
IMPLEMENT_SAVERESTORE( CCSFlashbang, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSG3SG1::m_SaveData[] =
{
	DEFINE_FIELD( CCSG3SG1, m_iZoomLevel, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CCSG3SG1, CBasePlayerWeapon );

TYPEDESCRIPTION	CCSM4A1::m_SaveData[] =
{
	DEFINE_FIELD( CCSM4A1, m_bSilencer, FIELD_BOOLEAN ),
};
IMPLEMENT_SAVERESTORE( CCSM4A1, CBasePlayerWeapon );
