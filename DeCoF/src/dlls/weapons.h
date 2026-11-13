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
#ifndef WEAPONS_H
#define WEAPONS_H

#include "effects.h"

class CBasePlayer;
extern int gmsgWeapPickup;

void DeactivateSatchels( CBasePlayer *pOwner );

// Contact Grenade / Timed grenade / Satchel Charge
class CGrenade : public CBaseMonster
{
public:
	void Spawn( void );

	typedef enum { SATCHEL_DETONATE = 0, SATCHEL_RELEASE } SATCHELCODE;

	static CGrenade *ShootTimed( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity, float time );
	static CGrenade *ShootContact( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static CGrenade *ShootSatchelCharge( entvars_t *pevOwner, Vector vecStart, Vector vecVelocity );
	static void UseSatchelCharges( entvars_t *pevOwner, SATCHELCODE code );

	void Explode( Vector vecSrc, Vector vecAim );
	void Explode( TraceResult *pTrace, int bitsDamageType );
	void EXPORT Smoke( void );

	void EXPORT BounceTouch( CBaseEntity *pOther );
	void EXPORT SlideTouch( CBaseEntity *pOther );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	void EXPORT DangerSoundThink( void );
	void EXPORT PreDetonate( void );
	void EXPORT Detonate( void );
	void EXPORT DetonateUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT TumbleThink( void );

	virtual void BounceSound( void );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void Killed( entvars_t *pevAttacker, int iGib );

	BOOL m_fRegisteredSound;// whether or not this grenade has issued its DANGER sound to the world sound list yet.
};


// constant items
#define ITEM_HEALTHKIT		1
#define ITEM_ANTIDOTE		2
#define ITEM_SECURITY		3
#define ITEM_BATTERY		4

#define WEAPON_NONE				0
#define WEAPON_CROWBAR			1
#define	WEAPON_GLOCK			3	// CoF (2 in the HLSDK)
#define WEAPON_PYTHON			3
#define WEAPON_MP5				4
#define WEAPON_CHAINGUN			5
#define WEAPON_CROSSBOW			6
#define WEAPON_SHOTGUN			7
#define WEAPON_RPG				8
#define WEAPON_GAUSS			9
#define WEAPON_EGON				10
#define WEAPON_HORNETGUN		11
#define WEAPON_HANDGRENADE		12
#define WEAPON_TRIPMINE			13
#define	WEAPON_SATCHEL			14
#define	WEAPON_SNARK			15

// buz: paranoia weapons goes here
#define WEAPON_AKS				16
#define WEAPON_APS				17
#define WEAPON_VAL				18
#define WEAPON_RPK				19
#define WEAPON_AK47				20
#define WEAPON_GROZA			21

#define WEAPON_GASMASK		22
#define WEAPON_HEADSHIELD	23
#define WEAPON_PAINKILLER	24

#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))

#define WEAPON_SUIT				31	// ?????

#define MAX_WEAPONS			32


#define MAX_NORMAL_BATTERY	100


// weapon weight factors (for auto-switching)   (-1 = noswitch)
#define CROWBAR_WEIGHT		0
#define GLOCK_WEIGHT		5	// CoF (10 in the HLSDK)
#define PYTHON_WEIGHT		15
#define MP5_WEIGHT			15
#define SHOTGUN_WEIGHT		15
#define CROSSBOW_WEIGHT		10
#define RPG_WEIGHT			20
#define GAUSS_WEIGHT		20
#define EGON_WEIGHT			20
#define HORNETGUN_WEIGHT	10
#define HANDGRENADE_WEIGHT	5
#define SNARK_WEIGHT		5
#define SATCHEL_WEIGHT		-10
#define TRIPMINE_WEIGHT		-10


// weapon clip/carry ammo capacities
#define URANIUM_MAX_CARRY		100
#define	_9MM_MAX_CARRY			120  // buz: used in paranoia
#define _357_MAX_CARRY			36
#define BUCKSHOT_MAX_CARRY		125 // buz: used in paranoia
#define BOLT_MAX_CARRY			50
#define ROCKET_MAX_CARRY		5
#define HANDGRENADE_MAX_CARRY	10
#define SATCHEL_MAX_CARRY		5
#define TRIPMINE_MAX_CARRY		5
#define SNARK_MAX_CARRY			15
#define HORNET_MAX_CARRY		8
#define M203_GRENADE_MAX_CARRY	10
// buz: paranoia wpns
#define APS_MAX_CARRY		120
#define BARRET_MAX_CARRY	60
#define AK_MAX_CARRY		120
#define AK47_MAX_CARRY		120
#define PARANOIA_MP5_MAX_CARRY		120
#define ASVAL_MAX_CARRY		80
#define RPK_MAX_CARRY		200

// the maximum amount of ammo each weapon's clip can hold
#define WEAPON_NOCLIP			-1

//#define CROWBAR_MAX_CLIP		WEAPON_NOCLIP
#define GLOCK_MAX_CLIP			15	// CoF (17 in the HLSDK)
#define PYTHON_MAX_CLIP			6
#define MP5_MAX_CLIP			30  // buz: used in paranoia
#define MP5_DEFAULT_AMMO		30  // buz: used in paranoia
#define SHOTGUN_MAX_CLIP		8  // buz: used in paranoia
#define CROSSBOW_MAX_CLIP		5
#define RPG_MAX_CLIP			1
#define GAUSS_MAX_CLIP			WEAPON_NOCLIP
#define EGON_MAX_CLIP			WEAPON_NOCLIP
#define HORNETGUN_MAX_CLIP		WEAPON_NOCLIP
#define HANDGRENADE_MAX_CLIP	WEAPON_NOCLIP
#define SATCHEL_MAX_CLIP		WEAPON_NOCLIP
#define TRIPMINE_MAX_CLIP		WEAPON_NOCLIP
#define SNARK_MAX_CLIP			WEAPON_NOCLIP
// buz: paranoia wpns
#define APS_MAX_CLIP	12
#define BARRET_MAX_CLIP	20
#define AKS_MAX_CLIP	30
#define AK47_MAX_CLIP	30
#define PARANOIA_MP5_MAX_CLIP	30
#define ASVAL_MAX_CLIP	20
#define RPK_MAX_CLIP	100
#define GROZA_MAX_CLIP	30


// the default amount of ammo that comes with each gun when it spawns
#define GLOCK_DEFAULT_GIVE			17
#define PYTHON_DEFAULT_GIVE			6
#define MP5_DEFAULT_GIVE			30  // buz: used in paranoia
#define MP5_DEFAULT_AMMO			30 // buz: used in paranoia
#define MP5_M203_DEFAULT_GIVE		0
#define SHOTGUN_DEFAULT_GIVE		12 // buz: used in paranoia
#define CROSSBOW_DEFAULT_GIVE		5
#define RPG_DEFAULT_GIVE			1
#define GAUSS_DEFAULT_GIVE			20
#define EGON_DEFAULT_GIVE			20
#define HANDGRENADE_DEFAULT_GIVE	5
#define SATCHEL_DEFAULT_GIVE		1
#define TRIPMINE_DEFAULT_GIVE		1
#define SNARK_DEFAULT_GIVE			5
#define HIVEHAND_DEFAULT_GIVE		8
// buz: paranoia wpns
#define APS_DEFAULT_GIVE	24
#define BARRET_DEFAULT_GIVE	40
#define AKS_DEFAULT_GIVE	30
#define AK47_DEFAULT_GIVE	40
#define PARANOIA_MP5_DEFAULT_GIVE	30
#define ASVAL_DEFAULT_GIVE	20
#define RPK_DEFAULT_GIVE	100
#define GROZA_DEFAULT_GIVE	30

// The amount of ammo given to a player by an ammo item.
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_GLOCKCLIP_GIVE		GLOCK_MAX_CLIP
#define AMMO_357BOX_GIVE		PYTHON_MAX_CLIP
#define AMMO_MP5CLIP_GIVE		MP5_MAX_CLIP
#define AMMO_CHAINBOX_GIVE		200
#define AMMO_M203BOX_GIVE		2
#define AMMO_BUCKSHOTBOX_GIVE	12
#define AMMO_CROSSBOWCLIP_GIVE	CROSSBOW_MAX_CLIP
#define AMMO_RPGCLIP_GIVE		RPG_MAX_CLIP
#define AMMO_URANIUMBOX_GIVE	20
#define AMMO_SNARKBOX_GIVE		5

// bullet types
typedef	enum
{
	BULLET_NONE = 0,
	BULLET_PLAYER_9MM, // glock
	BULLET_PLAYER_MP5, // mp5
	BULLET_PLAYER_357, // python
	BULLET_PLAYER_BUCKSHOT, // shotgun
	BULLET_PLAYER_CROWBAR, // crowbar swipe
	BULLET_PLAYER_CS,		// CoF (1.55 PDB enum)

	BULLET_MONSTER_9MM,
	BULLET_MONSTER_MP5,
	BULLET_MONSTER_12MM,

// paranoia bullets
	BULLET_PLAYER_APS,
	BULLET_PLAYER_BARRET,
	BULLET_PLAYER_AKS,
	BULLET_PLAYER_AK47,
	BULLET_PLAYER_ASVAL,
	BULLET_PLAYER_RPK,
	BULLET_PLAYER_GROZA,

	BULLET_MONSTER_AK,
	BULLET_MONSTER_ASVAL,
	BULLET_MONSTER_GROZA,
	BULLET_MONSTER_GLOCK,

	BULLET_TERROR_AK,
	BULLET_TERROR_RPK,
	BULLET_PLAYER_RIFLE,	// CoF (1.55 PDB enum)
} Bullet;


#define ITEM_FLAG_SELECTONEMPTY		1
#define ITEM_FLAG_NOAUTORELOAD		2
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4
#define ITEM_FLAG_LIMITINWORLD		8
#define ITEM_FLAG_EXHAUSTIBLE		16 // A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_NOAUTOSWITCHTO_guess	64 // CoF: CHalfLifeRules::FShouldSwitchWeapon refuses the switch

#define WEAPON_IS_ONTARGET 0x40

typedef struct
{
	int		iSlot;
	int		iPosition;
	const char	*pszAmmo1;	// ammo 1 type
	int		iMaxAmmo1;		// max ammo 1
	const char	*pszAmmo2;	// ammo 2 type
	int		iMaxAmmo2;		// max ammo 2
	const char	*pszName;
	int		iMaxClip;
	int		iId;
	int		iFlags;
	int		iWeight;// this value used to determine this weapon's importance in autoselection.
} ItemInfo;

typedef struct
{
	const char *pszName;
	int iId;
} AmmoInfo;

// Items that the player has in their inventory that they can use
class CBasePlayerItem : public CBaseAnimating
{
public:
	// buz: get advance spread vec to send it to client
	// gun dont use advanced spread by default
	virtual Vector GetSpreadVec( void ) {return Vector(0, 0, 0);};
	float m_fNextAccuracyTime;

	// buz: gun jumping actions
	virtual void PlayerJump() {};
	bool m_bRifleReload;
	bool m_bWantToEndReload;
	bool m_bRaised;
	bool m_bCanDualWield;

	// buz: get current weapon mode (for toggleable weapons)
	virtual int GetMode( void ) {return 0;}; // 0 means gun dont use mode

	virtual void SetObjectCollisionBox( void );
	virtual void IronsightsTo( void ) { }
	virtual void IronsightsFrom( void ) { }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];

	virtual int AddToPlayer( CBasePlayer *pPlayer );	// return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerItem *pItem, CBasePlayer *pPlayer ) { return FALSE; }	// return TRUE if you want your duplicate removed from world
	void EXPORT DestroyItem( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther );	// default weapon touch

	// Wargon: Переменные для юзабельности оружий.
	void EXPORT DefaultUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { DefaultTouch( pActivator ); }
	virtual int ObjectCaps( void ) { return m_iCurrCaps | FCAP_ACROSS_TRANSITION; }
	int m_iCurrCaps;

	void EXPORT FallThink ( void );// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize( void );// make a weapon visible and tangible
	void EXPORT AttemptToMaterialize( void );  // the weapon desires to become visible and tangible, if the game rules allow for it
	CBaseEntity* Respawn ( void );// copy a weapon
	void FallInit( void );
	void CheckRespawn( void );
	virtual int GetItemInfo(ItemInfo *p) { return 0; };	// returns 0 if struct not filled out
	virtual BOOL CanDeploy( void ) { return TRUE; };
	virtual BOOL Deploy( )								// returns is deploy was successful
		 { return TRUE; };

	virtual BOOL CanHolster( void ) { return TRUE; };// can this weapon be put away right now?
	virtual void Holster( void );
	virtual void UpdateItemInfo( void ) { return; };

	virtual void ItemPreFrame( void )	{ return; }		// called each frame by the player PreThink
	virtual void ItemPostFrame( void ) { return; }		// called each frame by the player PostThink

	virtual void Drop( void );
	virtual void Kill( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );

	virtual int PrimaryAmmoIndex() { return -1; };
	virtual int SecondaryAmmoIndex() { return -1; };

	virtual int UpdateClientData( CBasePlayer *pPlayer ) { return 0; }

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return NULL; };

	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];
	int strwmodel;
	int script;
	int slotnumber;
	float m_Accuracy;
	int old_m_iClip;
	int old_m_iAmmo;
	float oldAccuracy;
	int m_iSprintState;
	float m_iNextSprintAnim;
	int m_iReloadState;
	float m_iNextReloadAnim;
	float m_fReloadSpeed;
	bool m_bCanSuicide;
	int m_iSuicideState;
	int m_iSuicideButtonsNeeded;
	int m_iSuicideButtonsPressed;
	virtual void StartSuicide( void ) { }
	virtual void StopSuicide( void ) { }
	int m_iRageNeeded;
	int m_iRageLevel;
	float m_fNextRageDecrease;
	int m_iSprintIdleAnim1;
	int m_iSprintIdleAnim2;

	CBasePlayer	*m_pPlayer;
	CBasePlayerItem *m_pNext;
	int		m_iId;												// WEAPON_???

	virtual int iItemSlot( void ) { return 0; }			// return 0 to MAX_ITEMS_SLOTS, used in hud

	int			iItemPosition( void ) { return ItemInfoArray[ m_iId ].iPosition; }
	const char	*pszAmmo1( void )	{ return ItemInfoArray[ m_iId ].pszAmmo1; }
	int			iMaxAmmo1( void );	// CoF: out of line in weapons.cpp (multiplayer limits)
	const char	*pszAmmo2( void )	{ return ItemInfoArray[ m_iId ].pszAmmo2; }
	int			iMaxAmmo2( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo2; }
	const char	*pszName( void )	{ return ItemInfoArray[ m_iId ].pszName; }
	int			iMaxClip( void )	{ return ItemInfoArray[ m_iId ].iMaxClip; }
	int			iWeight( void )		{ return ItemInfoArray[ m_iId ].iWeight; }
	int			iFlags( void )		{ return ItemInfoArray[ m_iId ].iFlags; }
	// int		m_iIdPrimary;										// Unique Id for primary ammo
	// int		m_iIdSecondary;										// Unique Id for secondary ammo
};


// inventory items that 
class CBasePlayerWeapon : public CBasePlayerItem
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
	int custom_v_model;
	int custom_w_model;
	int custom_script;
	virtual void KeyValue( KeyValueData *pkvd );
	bool m_bIronSights;
	float m_fIronSightsForward;
	float m_fIronSightsRight;
	float m_fIronSightsUp;
	int m_iIronSightsZoom;
	Vector m_AccuracyVector;
	float m_flKickPerShot;
	float m_flCurrentKick;
	float m_flNextKickReductionTime;
	void CalculateWeaponAccuracy( void );
	bool m_bWaitingToReloadIronsights;
	bool m_bShotLooping;
	int shoot_loop_sound;
	int shoot_loop_sound_end;

	virtual void SetNextThink( float delay ); //LRC

	void	WeaponDelay(int iAnim); // buz
	void	WeaponDelayTime(float time); // buz
	float m_fEmptyClickTime;
	bool CanCurrentlySuicide( void );

	// generic weapon versions of CBasePlayerItem calls
	virtual int AddToPlayer( CBasePlayer *pPlayer );
	virtual int AddDuplicate( CBasePlayerItem *pOriginal, CBasePlayer *pPlayer );

	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon ); //{ return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );// { return TRUE; };			// Return TRUE if you can add ammo to yourself when picked up

	virtual int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player

	// generic "shared" ammo handlers
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );

	virtual void UpdateItemInfo( void ) {};	// updates HUD state

	int m_iPlayEmptySound;
	int m_fFireOnEmpty;		// True when the gun is empty and the player is still holding down the
							// attack key(s)
	virtual BOOL PlayEmptySound( void );
	virtual void ResetEmptySound( void );

	virtual void SendWeaponAnim( int iAnim, int skiplocal = 1, int body = 0 );  // skiplocal is 1 if client is predicting weapon animations
	virtual void SendWeaponBody( CBasePlayer *pPlayer, int body );

	virtual BOOL CanDeploy( void );
	virtual BOOL IsUseable( void );
	BOOL DefaultDeploy( char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal = 0, int body = 0 );
	int DefaultReload( int iClipSize, int iAnim, float fDelay, int body, bool infinitereload );

	virtual void ItemPostFrame( void );	// called each frame by the player PostThink
	// called by CBasePlayerWeapons ItemPostFrame()
	virtual void PrimaryAttack( void ) { return; }				// do "+ATTACK"
	virtual void SecondaryAttack( void ) { return; }			// do "+ATTACK2"
	virtual void TertiaryAttack( void ) { return; }
	virtual void WeaponToggle( void ) { return; }
	virtual void RageAttack( void ) { return; }
	virtual void RageReset( void ) { return; }
	virtual float GetNextRageDecreaseTime( void ) { return 0.75; }
	virtual void Reload( void ) { return; }						// do "+RELOAD"
	virtual void WeaponIdle( void ) { return; }					// called when no buttons pressed
	virtual int UpdateClientData( CBasePlayer *pPlayer );		// sends hud info to client dll, if things have changed
	virtual void RetireWeapon( void );
	virtual BOOL ShouldWeaponIdle( void ) {return FALSE; };
	virtual void Holster( void );
	virtual BOOL UseDecrement( void ) { return FALSE; };
	virtual void Fidget( void ) { return; }
	
	//LRC - used by weaponstrip
	void DrainClip(CBasePlayer* pPlayer, BOOL keep, int i9mm, int i357, int iBuck, int iBolt, int iARGren, int iRock, int iUranium, int iSatchel, int iSnark, int iTrip, int iGren,
		int iAps, int iBarret, int iAk, int iAsval, int iRpk, int iGroza);
	bool m_bCheckedIfShouldBeDeleted;
	
	int	PrimaryAmmoIndex(); 
	int	SecondaryAmmoIndex(); 

	void PrintState( void );

	virtual CBasePlayerItem *GetWeaponPtr( void ) { return (CBasePlayerItem *)this; };
	float m_flAccuracyTime;

	float m_flPumpTime;
	int		m_fInSpecialReload;									// Are we in the middle of a reload for the shotguns
	float	m_flNextPrimaryAttack;								// soonest time ItemPostFrame will call PrimaryAttack
	float	m_flNextSecondaryAttack;							// soonest time ItemPostFrame will call SecondaryAttack
	float m_flNextTertiaryAttack;
	float	m_flTimeWeaponIdle;									// soonest time ItemPostFrame will call WeaponIdle
	int		m_iPrimaryAmmoType;									// "primary" ammo index into players m_rgAmmo[]
	int		m_iSecondaryAmmoType;								// "secondary" ammo index into players m_rgAmmo[]
	int		m_iClip;											// number of shots left in the primary weapon clip, -1 it not used
	int		m_iClientClip;										// the last version of m_iClip sent to hud dll
	int		m_iClientWeaponState;								// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int		m_fInReload;										// Are we in the middle of a reload;

	int		m_iDefaultAmmo;// how much ammo you get when you pick up this weapon as placed by a level designer.
};


class CBasePlayerAmmo : public CBaseEntity
{
public:
	virtual void Spawn( void );

	// Wargon: Переменные для юзабельности патронов.
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT DefaultUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value ) { DefaultTouch( pActivator ); }
	virtual int ObjectCaps( void ) { return m_iCurrCaps | FCAP_ACROSS_TRANSITION; }
	int m_iCurrCaps;

	void EXPORT DefaultTouch( CBaseEntity *pOther ); // default weapon touch
	virtual BOOL AddAmmo( CBaseEntity *pOther ) { return TRUE; };

	CBaseEntity* Respawn( void );
	void EXPORT Materialize( void );
};


extern DLL_GLOBAL	short	g_sModelIndexLaser;// holds the index for the laser beam
extern DLL_GLOBAL	const char *g_pModelNameLaser;

extern DLL_GLOBAL	short	g_sModelIndexLaserDot;// holds the index for the laser beam dot
extern DLL_GLOBAL	short	g_sModelIndexFireball;// holds the index for the fireball
extern DLL_GLOBAL	short	g_sModelIndexSmoke;// holds the index for the smoke cloud
extern DLL_GLOBAL	short	g_sModelIndexWExplosion;// holds the index for the underwater explosion
extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model
extern DLL_GLOBAL	short	g_sModelIndexBloodDrop;// holds the sprite index for blood drops
extern DLL_GLOBAL	short	g_sModelIndexBloodSpray;// holds the sprite index for blood spray (bigger)

extern void ClearMultiDamage(void);
extern void ApplyMultiDamage(entvars_t* pevInflictor, entvars_t* pevAttacker );
extern void AddMultiDamage( entvars_t *pevInflictor, CBaseEntity *pEntity, float flDamage, int bitsDamageType);

extern void DecalGunshot( TraceResult *pTrace, int iBulletType );
extern void SpawnBlood(Vector vecSpot, int bloodColor, float flDamage, Vector velocity);
extern int DamageDecal( CBaseEntity *pEntity, int bitsDamageType );
extern void RadiusDamage( Vector vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType );

typedef struct 
{
	CBaseEntity		*pEntity;
	float			amount;
	int				type;
} MULTIDAMAGE;

extern MULTIDAMAGE gMultiDamage;


#define LOUD_GUN_VOLUME			1000
#define NORMAL_GUN_VOLUME		600
#define QUIET_GUN_VOLUME		200

#define	BRIGHT_GUN_FLASH		512
#define NORMAL_GUN_FLASH		256
#define	DIM_GUN_FLASH			128

#define BIG_EXPLOSION_VOLUME	2048
#define NORMAL_EXPLOSION_VOLUME	1024
#define SMALL_EXPLOSION_VOLUME	512

#define	WEAPON_ACTIVITY_VOLUME	64

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

//=========================================================
// CWeaponBox - a single entity that can store weapons
// and ammo. 
//=========================================================
class CWeaponBox : public CBaseEntity
{
	void Precache( void );
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsEmpty( void );
	int  GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );

public:
	void EXPORT Kill ( void );
	int		Save( CSave &save );
	int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	int weaponmodel;

	BOOL HasWeapon( CBasePlayerItem *pCheckItem );
	int PackWeapon( CBasePlayerItem *pWeapon, int wmodel );
	BOOL PackAmmo( int iszName, int iCount );
	
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each 

	int m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int	m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities

	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

#ifdef CLIENT_DLL
bool bIsMultiplayer ( void );
void LoadVModel ( char *szViewModel, CBasePlayer *m_pPlayer );
#endif

class CFlashlight : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	void EXPORT TurnOn( void );
	void EXPORT TurnOff( void );
	void ToggleSwitch( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
};


class CMobile : public CBasePlayerWeapon
{
public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	bool m_bWaitingToFix;
	bool m_bDisabled;
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	void EXPORT FixBattery( void );
	void EXPORT FlashToggle( void );
	void EXPORT FlashOff( void );
	void EXPORT MidBatteryFix( void );
	virtual void ItemPostFrame( void );
	void MessageGet( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void WeaponToggle( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	int m_iSwing;
	int bHit;
	bool m_bPhoneCall;
};


class CGlock : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void WeaponToggle( void );
	virtual void Fidget( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT AttachTaclight( void );
	void EXPORT MidAttachTaclight( void );
	bool m_bWaitingToFix;
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	void EXPORT ToggleSwitch( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	bool m_bTaclight;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};


class CRevolver : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	int m_iRotation;
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	void EXPORT ResetCylinder( void );
	void EXPORT SetCylinder( void );
	bool m_bReloading;
	bool m_bWantToEndReload;
	bool m_bInsertBullet;
	float m_flInsertTime;
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	void EXPORT ReloadInsert( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};


class CBrowningWheelchair : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	bool m_bWaitingToRedeploy;
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	Vector m_vAimingVector;
	Vector m_vEndVector;
};


class CDualWeapon : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	void EXPORT MobileFlashToggle( void );
	void EXPORT ThrowRemove( void );
	void EXPORT FlareDoIgnite( void );
	void EXPORT FlareRedeploy( void );
	void SetAmmoValues( void );
	void UpdateWeaponAmmo( void );
	void SetBodyForWeapons( bool left );
	int m_iCurrentShot;
	bool vp70left;
	void FlashlightSwitch( bool left );
	void MobileSwitch( bool left );
	void CameraShoot( bool left );
	void GlockShoot( bool left );
	void NightstickHit( bool left );
	void BranchHit( bool left );
	void SwitchbladeSlash( bool left );
	void P345Shoot( bool left );
	void RevolverShoot( bool left );
	void StartVP70Shoot( bool left );
	void EXPORT VP70Shoot( void );
	void TMPShoot( bool left );
	void FlareIgnite( bool left );
	int m_iRightWeaponID;
	int m_iLeftWeaponID;
	void EXPORT ReloadFinishLeft( void );
	void EXPORT ReloadFinishRight( void );
	float GetAttackDelayForWeapon( int weaponid );
	char *GetModelForWeapon( int weaponid, bool left );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	string_t m_sLeftHandWeaponModel;
	float m_ShotDelayL;
	int m_iDamageL;
	float m_ShotDelayR;
	int m_iDamageR;
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	virtual int UseDecrement( void ) { return 0; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};


class CTmp : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	virtual void SendInitMessage( CBasePlayer *player );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
};


class CFAMAS : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void SendInitMessage( CBasePlayer *player );
	virtual void Fidget( void );
	virtual void SecondaryAttack( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual void PrimaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CRifle : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	void EXPORT RifleEjectBrass( void );
	void EXPORT DoReload( void );
	void EXPORT InsertBullet( void );
	void EXPORT PostZoom( void );
	int m_iWantToPullBolt;
	float m_flNextBoltPullTime;
	int m_iWantToEjectShell;
	float m_flNextShellEjectTime;
	bool m_bWaitingToReload;
	void EXPORT Zoom( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CLantern : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void WeaponIdle( void );
	virtual void ItemPostFrame( void );
	virtual void SecondaryAttack( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	virtual void Fidget( void );
};


class CAction : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void WeaponIdle( void );
	virtual void ItemPostFrame( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
};


class CP345 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CG43 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CShotgun : public CBasePlayerWeapon
{
public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 3; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual void Fidget( void );
	virtual void ItemPostFrame( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_fInReload;
	float m_flNextReload;
	int m_iShell;
	void EXPORT LolShell( void );
	void EXPORT ShellInsert( void );
};


class CCamera : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void SendInitMessage( CBasePlayer *player );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	virtual void ItemPostFrame( void );
};


class CSwitchblade : public CBasePlayerWeapon
{
public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void ItemPostFrame( void );
	virtual void Fidget( void );
	virtual void RageReset( void );
	virtual void RageAttack( void );
	virtual float GetNextRageDecreaseTime( void );
	int m_iBloodAmount;
	void EXPORT BloodFingerprint( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	int Swing( int );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	int m_iSwing;
	int bHit;
	bool b_KnifeStance;
};


class CSyringe : public CBasePlayerWeapon
{
public:
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void ItemPostFrame( void );
	CBasePlayer *pHealVictim;
	void EXPORT BladeRetract( void );
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	void EXPORT DestroyItem( void );
	void EXPORT Redeploy( void );
	void EXPORT Inject( void );
	bool b_HasUsed;
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
};


class CM16 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	virtual void Fidget( void );
	int shoot_num;
	virtual void PrimaryAttack( void );
	virtual void WeaponToggle( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	void EXPORT Shoot( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	bool b_BurstMode;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};


class CMp5 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	virtual void SendInitMessage( CBasePlayer *player );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
};


class CVP70 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void ) { return 1; }
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void IronsightsTo( void );
	virtual void IronsightsFrom( void );
	void EXPORT SuicideShot( void );
	virtual void StartSuicide( void );
	virtual void StopSuicide( void );
	int shoot_num;
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void TertiaryAttack( void );
	void EXPORT Shoot( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CFlare : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int GetItemInfo( ItemInfo *p );
	virtual void Fidget( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual int Deploy( void );
	virtual void Holster( void );
	virtual void WeaponIdle( void );
	virtual int ShouldWeaponIdle( void ) { return 1; }
	void EXPORT FlareThink( void );
	void EXPORT Redeploy( void );
	bool m_bThrowing;
	bool m_bThrown;
	bool m_bNoRedeploy;
	void ParticleRestore( void );
	virtual void ItemPostFrame( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	void EXPORT ThrowRemove( void );
	void EXPORT Ignite( void );
	float m_fNextBurnTime;
};


class CCSM4A1 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	bool m_bSilencer;
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSBomb : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	void Plant( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	bool m_bPlanting;
	bool m_bPlanted;
	float m_fPlantProgress;
	virtual int Deploy( void );
	virtual void WeaponIdle( void );
};


class CCSElites : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	bool m_bRight;
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSAWP : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	bool m_bZoomed;
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSAug : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	bool m_bZoomed;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSFamas : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	bool m_bBurst;
	int shoot_num;
	void Shoot( void );
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSFlashbang : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	bool m_bPrimed;
	float m_flGrenadeThrowTime;
	float m_flRedrawTime;
	bool m_bThrown;
	virtual int Deploy( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


class CCSG3SG1 : public CBasePlayerWeapon
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int iItemSlot( void );
	virtual int GetItemInfo( ItemInfo * );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void );
	virtual void ItemPostFrame( void );
	virtual void Holster( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float GetMinAccuracyForAction( void );
	void DoAccuracyCalculations( void );
	int m_iZoomLevel;
	virtual int Deploy( void );
	virtual void Reload( void );
	virtual void WeaponIdle( void );
	int m_iShell;
};


#endif // WEAPONS_H
