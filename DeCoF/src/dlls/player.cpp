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

===== player.cpp ========================================================

  functions dealing with the player

*/

#include "extdll.h"
#include "util.h"

#include "cbase.h"
#include "player.h"
#include "cofr_features.h"
#include "trains.h"
#include "nodes.h"
#include "weapons.h"
#include "soundent.h"
#include "monsters.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"
#include "game.h"
#include "hltv.h"
#include "effects.h" //LRC
#include "movewith.h" //LRC
#include "animation.h"
#include <direct.h>
#include <fstream>
#include <vector>

// #define DUCKFIX

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL	BOOL	g_fDrawLines;
int gEvilImpulse101;
BOOL g_markFrameBounds = 0; //LRC
extern DLL_GLOBAL int		g_iSkillLevel, gDisplayTitle;


BOOL gInitHUD = TRUE;

extern void CopyToBodyQue(entvars_t* pev);
extern void respawn(entvars_t *pev, BOOL fCopyCorpse);
extern Vector VecBModelOrigin(entvars_t *pevBModel );
extern edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer );

// the world node graph
extern CGraph	WorldGraph;

// CoF globals defined in other files (types from the 1.55 PDB)
extern int g_iDoctorMode;
extern int g_iSurvivalMode;
extern int m_iSimonMode;
extern float g_flWeaponCheat;
extern bool newgore;
extern bool fvisiblecheck;
extern bool showobjective;
extern int demolevel;
extern unsigned short m_usViewPunch;
extern unsigned short m_usClientSound;
extern const char *GetNameForId[];
extern char com_token[];
extern char *COM_Parse( char *data );
extern int CountConnectedPlayers( void );

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );

extern void UTIL_SplashCalculation( CBaseEntity *pEntity, int splashtype );

class CCoFDocument : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];
	char page1[32];
	char page2[32];
	char page3[32];
	char page4[32];
	char note1[32];
	char note2[32];
	char note3[32];
	char note4[32];
	int turnsound;
	char label[48];
};

class CBoat : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT BoatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float boattime1;
	float boattime2;
	float boattime3;
	float boattime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT BoatThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

class CWheelchair : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT WheelchairUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void MoveForward( void );
	void MoveBack( void );
	void TurnLeft( void );
	void TurnRight( void );
	bool m_bDead;
	virtual void Killed( entvars_t *pevAttacker, int iGib );
	virtual int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	virtual int Classify( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	float wheelchairtime1;
	float wheelchairtime2;
	float wheelchairtime3;
	float wheelchairtime4;
	void EXPORT Embark( void );
	void ExitCheck( void );
	void EXPORT Disembark( void );
	CBaseEntity *pExitEntity;
	CBaseEntity *pExitTeller;
	void SetCollision( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	void EXPORT WheelchairThink( void );
	void SetSequenceByName( char *szSequence );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_IMPULSE_USE; }
	CBasePlayer *m_pPlayer;
};

class CCofTelephone : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void TelephoneRight( void );
	void TelephoneWrong( void );
	void RightNumber( void );
	void WrongNumber( void );
	void DialTone( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	EHANDLE m_pPlayer;
};

class CPuzzleBarButton : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseToggle :: ObjectCaps() | FCAP_CONTINUOUS_USE) & ~FCAP_ACROSS_TRANSITION; }
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual void Precache( void );
	void ToggleBar( int bar, CBasePlayer *pPlayer );
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextUse;
	float m_flNextCheck;
};

class CItemPadlock : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void Rotate( int number );
	void EXPORT Check( void );
	void EXPORT Finished( void );
	virtual STATE GetState( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return CBaseAnimating :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	void SetSequenceByName( char *szSequence );
	void Cancel( void );
};

class CCofComputer : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void ComputerCheck( CBaseEntity *pUser, char *username, char *password );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	char m_sUserName[64];
	char m_sPassword[64];
	static const char *pUsernames[];
	static const char *pPasswords[];
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

class CCofYesNo : public CBaseEntity
{
public:
	virtual void Spawn( void );
	void YesActivate( CBaseEntity *pActivator );
	void NoActivate( CBaseEntity *pActivator );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_sWeapon;
	string_t m_sItem;
	string_t m_sNotHaveWeaponMessage;
	string_t m_sNoTrigger;
	string_t m_sCvar;
	string_t m_sCvarNo;
	int m_cvarvalue;
	int m_cvarvalueno;
};

class CParticle : public CPointEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SendInitMessage( CBasePlayer *player );
	void EXPORT KillParticle( void );
	static CParticle *CreateParticle( int message, bool starton, Vector origin, int weaponattachment, float life, CBaseEntity *attachentity, float velocityx, float velocityy, float velocityz );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

class CRealCamera : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	void SetSequenceByName( char *szSequence );
	virtual void Precache( void );
	void StartAnimation( void );
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	virtual void Think( void );
	void CameraUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

class CInterDoor : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	void EnableDoor( bool disable );
	bool m_bDisabled;
	int m_iNewLevelDoorSound;
	void StartSequence( void );
	int IsLocked( void );
	void EndSequence( void );
	void UnlockDoor( CBaseEntity *pActivator );
	void ShiftMonsters( void );
	bool m_bSilentRoom;
	void EndGame( void );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_sMusic;
	string_t m_sLockedBy;
	string_t m_sLockedMsg;
	string_t m_sUnlockedMsg;
	string_t m_sProperTarget;
	string_t m_sUseUnlock;
	float m_fNextUse;
	float m_iMusicFadeOut;
	float m_iMusicFadeIn;
};

class CFuncValve : public CBaseToggle
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void ValveReturn( void );
	void DegreesCheck( void );
	void Off( void );
	void ProThink( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual STATE GetState( void );
	virtual void KeyValue( KeyValueData *pkvd );
	void UnlockValve( CBaseEntity *pActivator );
	void FindTargetEnt( void );
	string_t m_sLockedBy;
	string_t m_sLockedMsg;
	string_t m_sUnlockedMsg;
	Vector m_vInitialEntityAngles;
	EHANDLE m_hRotateModel;
	static TYPEDESCRIPTION m_SaveData[];
	float m_flNextCharge;
	int m_iOn;
	float m_flSoundTime;
};

struct playervariables
{
	bool m_bUsed;
	int m_iAllowedToUseTMP;
	char *szPlayerName;
	char *szLastUsedWeapon;
	int m_iLastUsedWeaponID;
	bool m_bIsADonator;
	int m_iAmmoInventory[6];
	int m_iAmmoInventoryAmount[6];
	int m_iPlayerHealth;
	int m_iPlayerStamina;
	char szPlayerQuickSlot1[128];
	char szPlayerQuickSlot2[128];
	char szPlayerQuickSlot3[128];
	bool m_bWasDead;
	int suvivalpoints;
	int shots_fired;
	int shots_hit;
	int syringes_used;
	int shots_missed;
	float gunaccuracy;
	int timessaved;
	int weaponscollected;
	int damagetaken;
	float distancetravelled_metres;
	float distancetravelled_kilometres;
	int time_seconds;
	int time_minutes;
	int time_hours;
	int monsters_killed_melee;
	int monsters_killed_shooting;
	bool started_from_the_beginning;
	int chosen_difficulty;
	bool doctor_mode;
	int m_iQuickSlotSlot[4];
	char *szWeapon1;
	int iClip1;
	int iAmmo1;
	char *szWeapon2;
	int iClip2;
	int iAmmo2;
	char *szWeapon3;
	int iClip3;
	int iAmmo3;
	char *szWeapon4;
	int iClip4;
	int iAmmo4;
	char *szWeapon5;
	int iClip5;
	int iAmmo5;
	char *szWeapon6;
	int iClip6;
	int iAmmo6;
	char szQuickSel1[128];
	char szQuickSel2[128];
	char szQuickSel3[128];
	bool m_bFirstDeploy[33];
};

class CCoopLevelChangeManager
{
public:
	CCoopLevelChangeManager( void );
	void GiveWeaponFromInventory( char *weapon, int slot, int i, CBasePlayer *pPlayer );
	void SetPlayerVariables( CBasePlayer *pPlayer );
	bool GivePlayerVariables( CBasePlayer *pPlayer );
	bool m_bJustHadGameOver;
	playervariables playervariablestruct[33];
	playervariables playervariablestructstats[33];
};

extern CCoopLevelChangeManager *g_pLevelChange;

// util.cpp: the engine's SetView wrapped in a function
#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

// effects.cpp
class CCoFSurvivalMode : public CPointEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void SurvivalThink( void );
	bool m_bSurvivalMode;
	bool m_bSurvivalModeRunning;
	string_t m_sSurvivalTriggerWhenWon;
	string_t m_sSurvivalTriggerWhenAllDead;
	int m_iSurvivalTimeInSeconds;
	int m_iCurrentSurvivalTime;
	float m_flNextSurvivalCount;
	int m_iTimeSeconds;
	int m_iTimeMinutes;
	int m_iTimeHours;
};

#include "CoF_Dual_Weapon.h"


// cof_keypad.h
class CCofKeypad : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool m_IsCorrect;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];
};

class CCofPhoneCheck : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void UnlockCheck( int number );
	void DistanceCheck( int number );
};

typedef unsigned __int32 uint32;
typedef unsigned __int64 uint64;

enum EAccountType
{
	k_EAccountTypeInvalid = 0,
	k_EAccountTypeIndividual = 1,
	k_EAccountTypeMultiseat = 2,
	k_EAccountTypeGameServer = 3,
	k_EAccountTypeAnonGameServer = 4,
	k_EAccountTypePending = 5,
	k_EAccountTypeContentServer = 6,
	k_EAccountTypeClan = 7,
	k_EAccountTypeChat = 8,
	k_EAccountTypeConsoleUser = 9,
	k_EAccountTypeAnonUser = 10,
	k_EAccountTypeMax
};

class CSteamID
{
public:
	CSteamID() { m_steamid.m_unAll64Bits = 0; }
	uint32 GetAccountID() const { return m_steamid.m_comp.m_unAccountID; }
	EAccountType GetEAccountType() const { return ( EAccountType )m_steamid.m_comp.m_EAccountType; }

private:
	union SteamID_t
	{
		struct SteamIDComponent_t
		{
			uint32 m_unAccountID : 32;
			unsigned int m_unAccountInstance : 20;
			unsigned int m_EAccountType : 4;
			int m_EUniverse : 8;
		} m_comp;

		uint64 m_unAll64Bits;
	} m_steamid;
};

struct FriendGameInfo_t;

class ISteamFriends
{
public:
	virtual const char *GetPersonaName() = 0;
	virtual void SetPersonaName( const char *pchPersonaName ) = 0;
	virtual int GetPersonaState() = 0;
	virtual int GetFriendCount( int iFriendFlags ) = 0;
	virtual CSteamID GetFriendByIndex( int iFriend, int iFriendFlags ) = 0;
	virtual int GetFriendRelationship( CSteamID steamIDFriend ) = 0;
	virtual int GetFriendPersonaState( CSteamID steamIDFriend ) = 0;
	virtual const char *GetFriendPersonaName( CSteamID steamIDFriend ) = 0;
	virtual bool GetFriendGamePlayed( CSteamID steamIDFriend, FriendGameInfo_t *pFriendGameInfo ) = 0;
	virtual const char *GetFriendPersonaNameHistory( CSteamID steamIDFriend, int iPersonaName ) = 0;
	virtual bool HasFriend( CSteamID steamIDFriend, int iFriendFlags ) = 0;
	virtual int GetClanCount() = 0;
	virtual CSteamID GetClanByIndex( int iClan ) = 0;
};

extern "C" __declspec( dllimport ) ISteamFriends * __cdecl SteamFriends();

class CStatueEagle : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

class CStatueHorse : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

class CStatueLion : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};

class CStatueOwl : public CBaseAnimating
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	virtual void KeyValue( KeyValueData *pkvd );
	void SetSequenceByName( char * );
	void MoveLeft( void );
	void MoveRight( void );
	void MoveUp( void );
	void MoveDown( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];

	int m_iPosition;
};


// buz: flags for goal panel
#define TP_FL_ENABLE	1
#define TP_FL_TITLE		2
#define TP_FL_IMAGE		4
#define TP_FL_POPUP		8


#define TRAIN_ACTIVE	0x80
#define TRAIN_NEW		0xc0
#define TRAIN_OFF		0x00
#define TRAIN_NEUTRAL	0x01
#define TRAIN_SLOW		0x02
#define TRAIN_MEDIUM	0x03
#define TRAIN_FAST		0x04
#define TRAIN_BACK		0x05

#define	FLASH_DRAIN_TIME	 1.2 //100 units/3 minutes
#define	FLASH_CHARGE_TIME	 0.2 // 100 units/20 seconds  (seconds per unit)

#ifdef XENWARRIOR
  float g_fEnvFadeTime = 0;    // flashlight can't be used until this time expires.
							// this is just a big hack, doesn't work with saverestore, etc...
							// Doing it properly would just be effort.
#endif

// Global Savedata for player
TYPEDESCRIPTION	CBasePlayer::m_playerSaveData[] =
{
	DEFINE_FIELD( CBasePlayer, m_flFlashLightTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iFlashBattery, FIELD_INTEGER ),

	DEFINE_FIELD( CBasePlayer, m_afButtonLast, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonPressed, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_afButtonReleased, FIELD_INTEGER ),

	DEFINE_ARRAY( CBasePlayer, m_rgItems, FIELD_INTEGER, MAX_ITEMS ),
	DEFINE_FIELD( CBasePlayer, m_afPhysicsFlags, FIELD_INTEGER ),

	DEFINE_FIELD( CBasePlayer, m_flTimeStepSound, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flSwimTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flDuckTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flWallJumpTime, FIELD_TIME ),

	DEFINE_FIELD( CBasePlayer, m_flSuitUpdate, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, m_rgSuitPlayList, FIELD_INTEGER, CSUITPLAYLIST ),
	DEFINE_FIELD( CBasePlayer, m_iSuitPlayNext, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_rgiSuitNoRepeat, FIELD_INTEGER, CSUITNOREPEAT ),
	DEFINE_ARRAY( CBasePlayer, m_rgflSuitNoRepeatTime, FIELD_TIME, CSUITNOREPEAT ),
	DEFINE_FIELD( CBasePlayer, m_lastDamageAmount, FIELD_INTEGER ),

	DEFINE_ARRAY( CBasePlayer, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CBasePlayer, m_pActiveItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pLastItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pNextItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pStoredActionItem, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_pStoredNightVisionWeapon, FIELD_CLASSPTR ),

	DEFINE_ARRAY( CBasePlayer, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_FIELD( CBasePlayer, m_idrowndmg, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_idrownrestored, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_tSneaking, FIELD_TIME ),

	DEFINE_FIELD( CBasePlayer, m_iTrain, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bitsHUDDamage, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flFallVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_iTargetVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponVolume, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iExtraSoundTypes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iWeaponFlash, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fLongJump, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fInitHUD, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_tbdPrev, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bIsTripping, FIELD_BOOLEAN ),

	DEFINE_FIELD( CBasePlayer, m_pTank, FIELD_EHANDLE ), // NB: this points to a CFuncTank*Controls* now. --LRC
	DEFINE_FIELD( CBasePlayer, m_iHideHUD, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFOV, FIELD_INTEGER ),

	// buz
	DEFINE_FIELD( CBasePlayer, m_pSpecTank, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_strCurrentMp3, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_strCurrentGoalName, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_strCurrentGoalImageName, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_strCurrentGoalTitleName, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_fMp3FadeTime, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_bMp3ShouldLoop, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fMp3FadeInTime, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, m_iJumpHeight, FIELD_INTEGER ),

	// buz: rain stuff
	DEFINE_FIELD( CBasePlayer, Rain_dripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, Rain_windX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_windY, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_randX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_randY, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, Rain_ideal_dripsPerSecond, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_windX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_windY, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_randX, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, Rain_ideal_randY, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, Rain_endFade, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, Rain_nextFadeUpdate, FIELD_TIME ),

	// buz: real lightlevel
	DEFINE_FIELD( CBasePlayer, m_fLightlevel, FIELD_FLOAT ),

	DEFINE_FIELD( CBasePlayer, b_GoingThroughADoor, FIELD_BOOLEAN ),
	DEFINE_ARRAY( CBasePlayer, inventorySlot, FIELD_STRING, 7 ),
	DEFINE_ARRAY( CBasePlayer, cofrInventorySlot, FIELD_STRING, 9 ),
	DEFINE_ARRAY( CBasePlayer, cofrInventorySlotModels, FIELD_STRING, 9 ),
	DEFINE_ARRAY( CBasePlayer, cofrInventorySlotClassname, FIELD_STRING, 9 ),
	DEFINE_FIELD( CBasePlayer, cofrInventorySchema, FIELD_INTEGER ),

	DEFINE_ARRAY( CBasePlayer, inventorySlotModels, FIELD_STRING, 7 ),
	DEFINE_ARRAY( CBasePlayer, inventorySlotClassname, FIELD_STRING, 7 ),
	DEFINE_FIELD( CBasePlayer, m_iFlashlight, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_DrawBars, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iBlurActive, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_UnreadSMS, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bHasSaved, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fStamina, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_fStaminaChargeCyle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_fStaminaBreatheCyle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bInfiniteStamina, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bInfiniteHealth, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bInfiniteAmmo, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_iKeypadNumber, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iViewEntIndex, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_sQuickSel1, FIELD_CHARACTER, 128 ),
	DEFINE_ARRAY( CBasePlayer, m_sQuickSel2, FIELD_CHARACTER, 128 ),
	DEFINE_ARRAY( CBasePlayer, m_sQuickSel3, FIELD_CHARACTER, 128 ),
	DEFINE_FIELD( CBasePlayer, m_bUsedFirstSyringe, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fMorphineTripTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_fTrippingTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iViewValue, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iMoValue, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_ClientLadder, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_OnStairs, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fFallingTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bShouldFallFreeze, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_LadderAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CBasePlayer, m_bNightmare, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fPhoneCallTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bInPhoneCall, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_pCamera, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_bDodgeDisabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fNextScaredBreathingTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bScaredBreathing, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bThreeSlots, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_hCurrentCamera, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_pPadlock, FIELD_EHANDLE ),
	DEFINE_ARRAY( CBasePlayer, m_iPadlockNumber, FIELD_INTEGER, 5 ),
	DEFINE_FIELD( CBasePlayer, m_iBlackAndWhite, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iTapes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bWeaponWaitingToDeploy, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_iHintAmount, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fNextHeartBeatTime, FIELD_FLOAT ),
	DEFINE_FIELD( CBasePlayer, m_bNearStranger, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_iSwitchbladeCuts, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_pLastAttackedEnemy, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_iPhoneDisabled, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bPhoneLow, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_fNextPhoneWarnTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bNoDeadFade, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_sDualWieldViewModel, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_bMirroredDualWield, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_vPreviousOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CBasePlayer, m_flNextDistanceCheckTime, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, playerstats, FIELD_CHARACTER, sizeof( statistics ) ),
	DEFINE_FIELD( CBasePlayer, m_iGoodPoints, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bBossHealthBar, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_sBossBarEmpty, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_sBossBarFull, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_sBossName, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_iBossMaxHealth, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iBossCurrentHealth, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iHealthCap, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iKeypadType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_fUseTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iDualWeaponLeftAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iDualWeaponRightAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, g_iDoorSound, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_iFlashlightFlags, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bInWater, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_iMobileFlashMode, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iDualWeaponLeftID, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iDualWeaponRightID, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_flQuickSlotTimeDecay, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iPhoneMessage, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[1], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[2], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[3], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[4], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[5], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[6], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[7], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[8], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[9], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[10], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[11], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[12], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[13], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[14], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[15], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, documentsstruct[16], FIELD_CHARACTER, sizeof( documents ) ),
	DEFINE_ARRAY( CBasePlayer, transferredentity_message, FIELD_STRING, 51 ),
	DEFINE_ARRAY( CBasePlayer, transferredentity_origin, FIELD_VECTOR, 51 ),
	DEFINE_ARRAY( CBasePlayer, transferredentity_classname, FIELD_STRING, 51 ),
	DEFINE_ARRAY( CBasePlayer, transferredentity_clip, FIELD_INTEGER, 51 ),
	DEFINE_FIELD( CBasePlayer, m_pBoat, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_pAttachedCamera, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_pWheelchair, FIELD_EHANDLE ),
	DEFINE_FIELD( CBasePlayer, m_fUnderwaterBreatheTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bWasDrowning, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bSubmerged, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bWasInShallowWater, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_flRippleTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iDeathBits, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iDoctorWeapon, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bWaterVoiceCancel, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_bStartedDrowning, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_flTurnVectorTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flTurnAroundTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_vTurnVector, FIELD_VECTOR ),
	DEFINE_FIELD( CBasePlayer, pDeathScene, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayer, m_bWaitingToFall, FIELD_BOOLEAN ),
	DEFINE_ARRAY( CBasePlayer, m_strHintsShown, FIELD_STRING, 33 ),
	DEFINE_FIELD( CBasePlayer, m_flCylinderRotateTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flNextHeartbeatTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bLowHealth, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_flNextMorphineDreamTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_fVomitTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_flNextTimeTakenCheck, FIELD_TIME ),
	DEFINE_ARRAY( CBasePlayer, m_iOwnedWeaponsList, FIELD_INTEGER, 32 ),
	DEFINE_FIELD( CBasePlayer, m_flNextPunchTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iPlayerSaveLock, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_b_Ending_RoofBossKilled, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_b_Ending_GivenP345, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_b_Ending_DeliveredPackage, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_flOutOfBreathDisableTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_iUnlockedClothes, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iFoundHoodies, FIELD_INTEGER ),
	DEFINE_ARRAY( CBasePlayer, m_bFirstDeploy, FIELD_BOOLEAN, 33 ),
	DEFINE_FIELD( CBasePlayer, m_iCurrentWeaponAnimation, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_iCurrentWeaponAnimationDual, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_sObjectiveText, FIELD_STRING ),
	DEFINE_FIELD( CBasePlayer, m_flNextNightVisionTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bHasNightVision, FIELD_BOOLEAN ),
	DEFINE_ARRAY( CBasePlayer, m_iQuickSlotSlot, FIELD_INTEGER, 4 ),
	DEFINE_FIELD( CBasePlayer, m_iHeadShieldOn, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayer, m_bJustJumped, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBasePlayer, m_flMaxJustJumpedTime, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_sCustomObjectives, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayer, m_bUsedVanNumber, FIELD_BOOLEAN ),
};

const char *CBasePlayer::pMorphineInjectionSounds[] =
{
	"player/vocal/MorphineInjection/Morphine1.wav",
	"player/vocal/MorphineInjection/Morphine2.wav",
	"player/vocal/MorphineInjection/Morphine3.wav",
	"player/vocal/MorphineInjection/Morphine4.wav",
	"player/vocal/MorphineInjection/Morphine5.wav",
	"player/vocal/MorphineInjection/Morphine6.wav",
	"player/vocal/MorphineInjection/Morphine7.wav",
	"player/vocal/MorphineInjection/Morphine8.wav",
};

const char *CBasePlayer::pLowHealthSounds[] =
{
	"player/vocal/LowHealth/lhealth1.wav",
	"player/vocal/LowHealth/lhealth2.wav",
	"player/vocal/LowHealth/lhealth3.wav",
	"player/vocal/LowHealth/lhealth4.wav",
	"player/vocal/LowHealth/lhealth5.wav",
};

const char *CBasePlayer::pPainSounds[] =
{
	"player/vocal/Pain/Pain1.wav",
	"player/vocal/Pain/Pain2.wav",
	"player/vocal/Pain/Pain3.wav",
	"player/vocal/Pain/Pain4.wav",
	"player/vocal/Pain/Pain5.wav",
	"player/vocal/Pain/Pain6.wav",
	"player/vocal/Pain/Pain7.wav",
	"player/vocal/Pain/Pain8.wav",
	"player/vocal/Pain/Pain9.wav",
	"player/vocal/Pain/Pain10.wav",
	"player/vocal/Pain/Pain11.wav",
	"player/vocal/Pain/Pain12.wav",
	"player/vocal/Pain/Pain13.wav",
	"player/vocal/Pain/Pain14.wav",
	"player/vocal/Pain/Pain15.wav",
	"player/vocal/Pain/Pain16.wav",
};

const char *CBasePlayer::pSwingSounds[] =
{
	"player/vocal/Swing/Swing1.wav",
	"player/vocal/Swing/Swing2.wav",
	"player/vocal/Swing/Swing3.wav",
	"player/vocal/Swing/Swing4.wav",
	"player/vocal/Swing/Swing5.wav",
	"player/vocal/Swing/Swing6.wav",
	"player/vocal/Swing/Swing7.wav",
	"player/vocal/Swing/Swing8.wav",
	"player/vocal/Swing/Swing9.wav",
	"player/vocal/Swing/Swing10.wav",
};

const char *CBasePlayer::pDeathSounds[] =
{
	"player/vocal/Death/Death1.wav",
	"player/vocal/Death/Death2.wav",
	"player/vocal/Death/Death3.wav",
	"player/vocal/Death/Death4.wav",
	"player/vocal/Death/Death5.wav",
	"player/vocal/Death/Death6.wav",
	"player/vocal/Death/Death7.wav",
};

const char *CBasePlayer::pVomitSounds[] =
{
	"player/vocal/Vomit/vomit1.wav",
};

const char *CBasePlayer::pMorphineInjectionSoundsPolice[] =
{
	"player/police/MorphineInjection/Morphine1.wav",
	"player/police/MorphineInjection/Morphine2.wav",
	"player/police/MorphineInjection/Morphine3.wav",
	"player/police/MorphineInjection/Morphine4.wav",
};

const char *CBasePlayer::pLowHealthSoundsPolice[] =
{
	"player/police/LowHealth/lhealth1.wav",
	"player/police/LowHealth/lhealth2.wav",
	"player/police/LowHealth/lhealth3.wav",
	"player/police/LowHealth/lhealth4.wav",
};

const char *CBasePlayer::pPainSoundsPolice[] =
{
	"player/police/Pain/Pain1.wav",
	"player/police/Pain/Pain2.wav",
	"player/police/Pain/Pain3.wav",
	"player/police/Pain/Pain4.wav",
	"player/police/Pain/Pain5.wav",
	"player/police/Pain/Pain6.wav",
	"player/police/Pain/Pain7.wav",
	"player/police/Pain/Pain8.wav",
	"player/police/Pain/Pain9.wav",
	"player/police/Pain/Pain10.wav",
};

const char *CBasePlayer::pSwingSoundsPolice[] =
{
	"player/police/Swing/Swing1.wav",
	"player/police/Swing/Swing2.wav",
	"player/police/Swing/Swing3.wav",
	"player/police/Swing/Swing4.wav",
	"player/police/Swing/Swing5.wav",
	"player/police/Swing/Swing6.wav",
};

const char *CBasePlayer::pDeathSoundsPolice[] =
{
	"player/police/Death/Death1.wav",
	"player/police/Death/Death2.wav",
	"player/police/Death/Death3.wav",
	"player/police/Death/Death4.wav",
	"player/police/Death/Death5.wav",
};

const char *CBasePlayer::pLowHealthSoundsDoctor[] =
{
	"player/Doctor/LowHealth/lhealth1.wav",
	"player/Doctor/LowHealth/lhealth2.wav",
	"player/Doctor/LowHealth/lhealth3.wav",
	"player/Doctor/LowHealth/lhealth4.wav",
	"player/Doctor/LowHealth/lhealth5.wav",
};

const char *CBasePlayer::pPainSoundsDoctor[] =
{
	"player/Doctor/Pain/Pain1.wav",
	"player/Doctor/Pain/Pain2.wav",
	"player/Doctor/Pain/Pain3.wav",
	"player/Doctor/Pain/Pain4.wav",
	"player/Doctor/Pain/Pain5.wav",
	"player/Doctor/Pain/Pain6.wav",
	"player/Doctor/Pain/Pain7.wav",
	"player/Doctor/Pain/Pain8.wav",
};

const char *CBasePlayer::pSwingSoundsDoctor[] =
{
	"player/Doctor/Swing/Swing1.wav",
	"player/Doctor/Swing/Swing2.wav",
	"player/Doctor/Swing/Swing3.wav",
	"player/Doctor/Swing/Swing4.wav",
	"player/Doctor/Swing/Swing5.wav",
	"player/Doctor/Swing/Swing6.wav",
	"player/Doctor/Swing/Swing7.wav",
	"player/Doctor/Swing/Swing8.wav",
	"player/Doctor/Swing/Swing9.wav",
	"player/Doctor/Swing/Swing10.wav",
};

const char *CBasePlayer::pDeathSoundsDoctor[] =
{
	"player/Doctor/Death/Death1.wav",
	"player/Doctor/Death/Death2.wav",
	"player/Doctor/Death/Death3.wav",
	"player/Doctor/Death/Death4.wav",
	"player/Doctor/Death/Death5.wav",
	"player/Doctor/Death/Death6.wav",
	"player/Doctor/Death/Death7.wav",
};

bool m_bMorphineMuffle;
bool m_gbIsInCutscene;
entvars_t *g_pevLastInflictor;
char *szCustomCampaign;
int gSpecialItemGive;
bool gNoWeaponDrop;
extern "C" int foot_splash;
int foot_splash;
bool heartbeat;
bool clientmaterials;
float m_fFaggotTime;
bool m_bRestored;
int saveslot = 1;

int giPrecacheHLweapons = 0;
int giPrecacheGrunt = 0;
int gmsgShake = 0;
int gmsgFade = 0;
int gmsgSelAmmo = 0;
int gmsgFlashlight = 0;
int gmsgFlashBattery = 0;
int gmsgResetHUD = 0;
int gmsgInitHUD = 0;
int gmsgSetFog = 0; //LRC
int gmsgKeyedDLight = 0;//LRC
int gmsgSetSky = 0;		//LRC
int gmsgHUDColor = 0;	//LRC
int gmsgAddShine = 0;   // LRC
int gmsgParticle = 0; // LRC
int gmsgDelParticle = 0; // buz
int gmsgShowGameTitle = 0;
int gmsgCurWeapon = 0;
int gmsgHealth = 0;
int gmsgDamage = 0;
int gmsgBattery = 0;
int gmsgTrain = 0;
int gmsgLogo = 0;
int gmsgWeaponList = 0;
int gmsgAmmoX = 0;
int gmsgHudText = 0;
int gmsgDeathMsg = 0;
int gmsgScoreInfo = 0;
int gmsgTeamInfo = 0;
int gmsgTeamScore = 0;
int gmsgGameMode = 0;
int gmsgMOTD = 0;
int gmsgServerName = 0;
int gmsgAmmoPickup = 0;
int gmsgWeapPickup = 0;
int gmsgItemPickup = 0;
int gmsgHideWeapon = 0;
int gmsgSetCurWeap = 0;
int gmsgSayText = 0;
int gmsgTextMsg = 0;
int gmsgSetFOV = 0;
int gmsgShowMenu = 0;
int gmsgGeigerRange = 0;
int gmsgTeamNames = 0;
int gmsgStatusIcon = 0; //LRC
int gmsgStatusText = 0;
int gmsgStatusValue = 0;
int gmsgSpecTank = 0; // buz
int gmsgTextWindow = 0; // buz
int gmsgMP3 = 0; // buz
int gmsgRainData = 0; // rain
int gmsgGrass = 0; // buz
int gmsgRadioIcon = 0; // buz
int gmsgTabPanel = 0; // buz
int gmsgCustomDecal = 0; // buz
int gmsgCustomDLight = 0; // buz
int gmsgSkymarker_Sky = 0; // buz
int gmsgSkymarker_World = 0; // buz
int gmsgProFont = 0;
int gmsgMaglite = 0;
int gmsgBlackBars = 0;
int gmsgBlur = 0;
int gmsgUnreadSMS = 0;
int gmsgStamina = 0;
int gmsgVGUIMenu = 0;
int gmsgBoard = 0;
int gmsgNoteDoc = 0;
int gmsgKeypad = 0;
int gmsgPhone = 0;
int gmsgPuzzlebar = 0;
int gmsgCoFLogo = 0;
int gmsgSave = 0;
int gmsgCoFWeapon = 0;
int gmsgViewKnock = 0;
int gmsgViewEntity = 0;
int gmsgViewLock = 0;
int gmsgLadder = 0;
int gmsgFallOver = 0;
int gmsgWpnInfo = 0;
int gmsgShowHint = 0;
int gmsgAccuracy = 0;
int gmsgYesNo = 0;
int gmsgStatue = 0;
int gmsgCameraZoom = 0;
int gmsgPadlock = 0;
int gmsgSaveLoad = 0;
int gmsgGameMenu = 0;
int gmsgBlackWhite = 0;
int gmsgCanUse = 0;
int gmsgInventory = 0;
int gmsgConcuss = 0;
int gmsgComputer = 0;
int gmsgUserDoc = 0;
int gmsgHeadShield = 0;
int gmsgWeaponBody = 0;
int gmsgScope = 0;
int gmsgBomb = 0;
int gmsgPhoneOff = 0;
int gmsgMuzzleflash = 0;
int gmsgDualWield = 0;
int gmsgDualAnim = 0;
int gmsgDualFlash = 0;
int gmsgModelAttach = 0;
int gmsgHealthCap = 0;
int gmsgBossBar = 0;
int gmsgDualAmmo = 0;
int gmsgThirdPerson = 0;
int gmsgTelephone = 0;
int gmsgTelescope = 0;
int gmsgQSlotClear = 0;
int gmsgFlashFlags = 0;
int gmsgDocuments = 0;
int gmsgBoat = 0;
int gmsgRipple = 0;
int gmsgAddNote = 0;
int gmsgAttachCam = 0;
int gmsgFaggot = 0;
int gmsgNewFade = 0;
int gmsgChapter = 0;
int gmsgSkipScene = 0;
int gmsgKillWpnPart = 0;
int gmsgWpnDeploy = 0;
int gmsgIronSights = 0;
int gmsgIgnite = 0;
int gmsgWheelchair = 0;
int gmsgControlHint = 0;
int gmsgLensFlare = 0;
int gmsgGreyFade = 0;
int gmsgAmmoInv = 0;
int gmsgReadyRoom = 0;
int gmsgRevolverCyl = 0;
int gmsgCredits = 0;
int gmsgQSlotUpdate = 0;
int gmsgTakePhoto = 0;
int gmsgClothes = 0;
int gmsgObjectives = 0;
int gmsgUnlocks = 0;
int gmsgTimer = 0;
int gmsgTimerPlus = 0;
int gmsgAdLib = 0;
int gmsgLobby = 0;
int gmsgInventChck = 0;
int gmsgCoopStats = 0;
int gmsgMPModel = 0;
int gmsgBeatCoop = 0;
int gmsgHoldBreath = 0;
int gmsgDevDonator = 0;
int gmsgCoopCamAnim = 0;
int gmsgIntrodn = 0;
int gmsgLookingAt = 0;
int gmsgSimonMode = 0;
int gmsgSyringe = 0;
int gmsgCustomDoc = 0;
int gmsgMaterials = 0;
int gmsgCustomSlv = 0;
int gmsgCustomFng = 0;
int gmsgCustomGlv = 0;
int gmsgPlaySFX = 0;	// CoF 1.6a (name from the message)
int gmsgSaveHack = 0;	// CoF 1.6a (name from the message)

float m_flTime;
float m_fOldTime;
float m_flTimeDelta;
bool shitcock;
int lastquickslotpressed;

void LinkUserMessages( void )
{
	// Already taken care of?
	if ( gmsgSelAmmo )
	{
		return;
	}

	gmsgSelAmmo = REG_USER_MSG( "SelAmmo", sizeof(SelAmmo) );
	gmsgCurWeapon = REG_USER_MSG( "CurWeapon", 3 );
	gmsgGeigerRange = REG_USER_MSG( "Geiger", 1 );
	gmsgFlashlight = REG_USER_MSG( "Flashlight", 2 );
	gmsgFlashBattery = REG_USER_MSG( "FlashBat", 1 );
	gmsgHealth = REG_USER_MSG( "Health", 3 );
	gmsgDamage = REG_USER_MSG( "Damage", 13 );
	gmsgBattery = REG_USER_MSG( "Battery", 2 );
	gmsgTrain = REG_USER_MSG( "Train", 1 );
	gmsgHudText = REG_USER_MSG( "HudText", -1 );
	gmsgSayText = REG_USER_MSG( "SayText", -1 );
	gmsgTextMsg = REG_USER_MSG( "TextMsg", -1 );
	gmsgWeaponList = REG_USER_MSG( "WeaponList", -1 );
	gmsgResetHUD = REG_USER_MSG( "ResetHUD", 1 );
	gmsgInitHUD = REG_USER_MSG( "InitHUD", 0 );
	gmsgSetFog = REG_USER_MSG( "SetFog", 9 );
	gmsgKeyedDLight = REG_USER_MSG( "KeyedDLight", -1 );
	gmsgSetSky = REG_USER_MSG( "SetSky", 7 );
	gmsgHUDColor = REG_USER_MSG( "HUDColor", 4 );
	gmsgAddShine = REG_USER_MSG( "AddShine", -1 );
	gmsgParticle = REG_USER_MSG( "Particle", -1 );
	gmsgDelParticle = REG_USER_MSG( "DelPart", -1 );
	gmsgShowGameTitle = REG_USER_MSG( "GameTitle", 1 );
	gmsgDeathMsg = REG_USER_MSG( "DeathMsg", -1 );
	gmsgScoreInfo = REG_USER_MSG( "ScoreInfo", 9 );
	gmsgTeamInfo = REG_USER_MSG( "TeamInfo", -1 );
	gmsgTeamScore = REG_USER_MSG( "TeamScore", -1 );
	gmsgGameMode = REG_USER_MSG( "GameMode", 1 );
	gmsgMOTD = REG_USER_MSG( "MOTD", -1 );
	gmsgServerName = REG_USER_MSG( "ServerName", -1 );
	gmsgAmmoPickup = REG_USER_MSG( "AmmoPickup", -1 );
	gmsgWeapPickup = REG_USER_MSG( "WeapPickup", 1 );
	gmsgItemPickup = REG_USER_MSG( "ItemPickup", -1 );
	gmsgHideWeapon = REG_USER_MSG( "HideWeapon", 1 );
	gmsgSetFOV = REG_USER_MSG( "SetFOV", 1 );
	gmsgShowMenu = REG_USER_MSG( "ShowMenu", -1 );
	gmsgShake = REG_USER_MSG( "ScreenShake", sizeof(ScreenShake) );
	gmsgFade = REG_USER_MSG( "ScreenFade", 10 );
	gmsgAmmoX = REG_USER_MSG( "AmmoX", 3 );
	gmsgTeamNames = REG_USER_MSG( "TeamNames", -1 );
	gmsgStatusIcon = REG_USER_MSG( "StatusIcon", -1 );
	gmsgStatusText = REG_USER_MSG( "StatusText", -1 );
	gmsgStatusValue = REG_USER_MSG( "StatusValue", 3 );
	gmsgSpecTank = REG_USER_MSG( "SpecTank", -1 );
	gmsgTextWindow = REG_USER_MSG( "TextWindow", -1 );
	gmsgMP3 = REG_USER_MSG( "PlayMP3", -1 );
	gmsgRainData = REG_USER_MSG( "RainData", 16 );
	gmsgGrass = REG_USER_MSG( "GrassInfo", -1 );
	gmsgRadioIcon = REG_USER_MSG( "RadioIcon", -1 );
	gmsgTabPanel = REG_USER_MSG( "TabPanel", -1 );
	gmsgCustomDecal = REG_USER_MSG( "customdecal", -1 );
	gmsgCustomDLight = REG_USER_MSG( "mydlight", -1 );
	gmsgSkymarker_Sky = REG_USER_MSG( "skymark_sky", -1 );
	gmsgSkymarker_World = REG_USER_MSG( "skymark_w", -1 );
	gmsgProFont = REG_USER_MSG( "ProFont", -1 );
	gmsgInventory = REG_USER_MSG( "Inventory", -1 );
	gmsgMaglite = REG_USER_MSG( "Maglite", 1 );
	gmsgBlackBars = REG_USER_MSG( "BlackBars", -1 );
	gmsgBlur = REG_USER_MSG( "Blur", 1 );
	gmsgUnreadSMS = REG_USER_MSG( "UnreadSMS", 2 );
	gmsgStamina = REG_USER_MSG( "Stamina", 3 );
	gmsgVGUIMenu = REG_USER_MSG( "VGUIMenu", 2 );
	gmsgBoard = REG_USER_MSG( "board", -1 );
	gmsgNoteDoc = REG_USER_MSG( "NoteDoc", -1 );
	gmsgKeypad = REG_USER_MSG( "Keypad", 3 );
	gmsgPhone = REG_USER_MSG( "Phone", 1 );
	gmsgPuzzlebar = REG_USER_MSG( "Puzzlebar", 1 );
	gmsgCoFLogo = REG_USER_MSG( "CoFLogo", 1 );
	gmsgSave = REG_USER_MSG( "Save", 1 );
	gmsgCoFWeapon = REG_USER_MSG( "CoFWeapon", -1 );
	gmsgViewKnock = REG_USER_MSG( "ViewKnock", 1 );
	gmsgViewEntity = REG_USER_MSG( "ViewEntity", -1 );
	gmsgViewLock = REG_USER_MSG( "ViewLock", -1 );
	gmsgCanUse = REG_USER_MSG( "CanUse", -1 );
	gmsgConcuss = REG_USER_MSG( "Concuss", 2 );
	gmsgLadder = REG_USER_MSG( "Ladder", -1 );
	gmsgFallOver = REG_USER_MSG( "FallOver", -1 );
	gmsgWpnInfo = REG_USER_MSG( "WpnInfo", 4 );
	gmsgShowHint = REG_USER_MSG( "ShowHint", -1 );
	gmsgAccuracy = REG_USER_MSG( "Accuracy", -1 );
	gmsgYesNo = REG_USER_MSG( "YesNo", -1 );
	gmsgStatue = REG_USER_MSG( "Statue", -1 );
	gmsgComputer = REG_USER_MSG( "Computer", -1 );
	gmsgUserDoc = REG_USER_MSG( "UserDoc", 2 );
	gmsgHeadShield = REG_USER_MSG( "HeadShield", -1 );
	gmsgCameraZoom = REG_USER_MSG( "CameraZoom", -1 );
	gmsgPadlock = REG_USER_MSG( "Padlock", 1 );
	gmsgSaveLoad = REG_USER_MSG( "SaveLoad", -1 );
	gmsgGameMenu = REG_USER_MSG( "GameMenu", 0 );
	gmsgBlackWhite = REG_USER_MSG( "BlackWhite", 1 );
	gmsgWeaponBody = REG_USER_MSG( "WeaponBody", 1 );
	gmsgScope = REG_USER_MSG( "Scope", 1 );
	gmsgBomb = REG_USER_MSG( "Bomb", 1 );
	gmsgPhoneOff = REG_USER_MSG( "PhoneOff", 1 );
	gmsgMuzzleflash = REG_USER_MSG( "Muzzleflash", -1 );
	gmsgDualWield = REG_USER_MSG( "DualWield", -1 );
	gmsgDualAnim = REG_USER_MSG( "DualAnim", 1 );
	gmsgDualFlash = REG_USER_MSG( "DualFlash", 1 );
	gmsgModelAttach = REG_USER_MSG( "ModelAttach", -1 );
	gmsgBossBar = REG_USER_MSG( "BossBar", -1 );
	gmsgHealthCap = REG_USER_MSG( "HealthCap", 1 );
	gmsgDualAmmo = REG_USER_MSG( "DualAmmo", -1 );
	gmsgThirdPerson = REG_USER_MSG( "ThirdPerson", -1 );
	gmsgTelephone = REG_USER_MSG( "Telephone", -1 );
	gmsgTelescope = REG_USER_MSG( "Telescope", -1 );
	gmsgQSlotClear = REG_USER_MSG( "QSlotClear", 2 );
	gmsgFlashFlags = REG_USER_MSG( "FlashFlags", -1 );
	gmsgDocuments = REG_USER_MSG( "Documents", -1 );
	gmsgBoat = REG_USER_MSG( "Boat", -1 );
	gmsgRipple = REG_USER_MSG( "Ripple", -1 );
	gmsgAddNote = REG_USER_MSG( "AddNote", -1 );
	gmsgAttachCam = REG_USER_MSG( "AttachCam", -1 );
	gmsgFaggot = REG_USER_MSG( "BigPrick", 1 );
	gmsgNewFade = REG_USER_MSG( "NewFade", -1 );
	gmsgChapter = REG_USER_MSG( "Chapter", -1 );
	gmsgSkipScene = REG_USER_MSG( "SkipScene", 0 );
	gmsgKillWpnPart = REG_USER_MSG( "KillWpnPart", 1 );
	gmsgWpnDeploy = REG_USER_MSG( "WpnDeploy", 0 );
	gmsgIronSights = REG_USER_MSG( "IronSights", -1 );
	gmsgIgnite = REG_USER_MSG( "Ignite", -1 );
	gmsgWheelchair = REG_USER_MSG( "Wheelchair", -1 );
	gmsgControlHint = REG_USER_MSG( "ControlHint", -1 );
	gmsgLensFlare = REG_USER_MSG( "LensFlare", 6 );
	gmsgGreyFade = REG_USER_MSG( "GreyFade", 2 );
	gmsgAmmoInv = REG_USER_MSG( "AmmoInv", 3 );
	gmsgReadyRoom = REG_USER_MSG( "ReadyRoom", 1 );
	gmsgRevolverCyl = REG_USER_MSG( "RevolverCyl", 1 );
	gmsgCredits = REG_USER_MSG( "Credits", -1 );
	gmsgQSlotUpdate = REG_USER_MSG( "QSlotUpdate", 2 );
	gmsgTakePhoto = REG_USER_MSG( "TakePhoto", 0 );
	gmsgClothes = REG_USER_MSG( "Clothes", 7 );
	gmsgObjectives = REG_USER_MSG( "Objectives", -1 );
	gmsgUnlocks = REG_USER_MSG( "Unlocks", -1 );
	gmsgTimer = REG_USER_MSG( "Timer", 6 );
	gmsgTimerPlus = REG_USER_MSG( "TimerPlus", 3 );
	gmsgAdLib = REG_USER_MSG( "AdLib", 4 );
	gmsgLobby = REG_USER_MSG( "Lobby", 3 );
	gmsgInventChck = REG_USER_MSG( "InventChck", 0 );
	gmsgCoopStats = REG_USER_MSG( "CoopStats", -1 );
	gmsgMPModel = REG_USER_MSG( "MPModel", 0 );
	gmsgBeatCoop = REG_USER_MSG( "BeatCoop", 0 );
	gmsgHoldBreath = REG_USER_MSG( "HoldBreath", 1 );
	gmsgDevDonator = REG_USER_MSG( "DevDonator", 2 );
	gmsgCoopCamAnim = REG_USER_MSG( "CoopCamAnim", 1 );
	gmsgIntrodn = REG_USER_MSG( "Introdn", 3 );
	gmsgLookingAt = REG_USER_MSG( "LookingAt", 1 );
	gmsgSimonMode = REG_USER_MSG( "SimonMode", 1 );
	gmsgSyringe = REG_USER_MSG( "Syringe", 2 );
	gmsgCustomDoc = REG_USER_MSG( "CustomDoc", -1 );
	gmsgMaterials = REG_USER_MSG( "Materials", 0 );
	gmsgCustomSlv = REG_USER_MSG( "CustomSlv", -1 );
	gmsgCustomFng = REG_USER_MSG( "CustomFng", -1 );
	gmsgCustomGlv = REG_USER_MSG( "CustomGlv", -1 );
	gmsgPlaySFX = REG_USER_MSG( "PlaySFX", -1 );
	gmsgSaveHack = REG_USER_MSG( "SaveHack", 1 );
}

LINK_ENTITY_TO_CLASS( player, CBasePlayer );



void CBasePlayer :: Pain( void )
{
	if ( pev->waterlevel < 3 )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pPainSoundsPolice[ RANDOM_LONG( 0, 9 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
		else if ( pev->vuser1.x )
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pPainSoundsDoctor[ RANDOM_LONG( 0, 7 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
		else
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pPainSounds[ RANDOM_LONG( 0, 15 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	}
	else if ( !m_bStartedDrowning )
	{
		m_bStartedDrowning = true;

		switch ( RANDOM_LONG( 0, 2 ) )
		{
		case 0:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "player/vocal/underwaterhurt1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "player/vocal/underwaterhurt2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:
			EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, "player/vocal/underwaterhurt3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
	}
}

void CBasePlayer :: MorphineSound( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pMorphineInjectionSoundsPolice[ RANDOM_LONG( 0, 3 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pMorphineInjectionSounds[ RANDOM_LONG( 0, 7 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
}

void CBasePlayer :: LowHealthSound( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pLowHealthSoundsPolice[ RANDOM_LONG( 0, 3 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	else if ( pev->vuser1.x )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pLowHealthSoundsDoctor[ RANDOM_LONG( 0, 4 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pLowHealthSounds[ RANDOM_LONG( 0, 4 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
}

void CBasePlayer :: SwingSound( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pSwingSoundsPolice[ RANDOM_LONG( 0, 5 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	else if ( pev->vuser1.x )
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pSwingSoundsDoctor[ RANDOM_LONG( 0, 9 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	else
		EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pSwingSounds[ RANDOM_LONG( 0, 9 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
}

void CBasePlayer :: DeathSound( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( pev->waterlevel < 3 )
			EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, pDeathSoundsPolice[ RANDOM_LONG( 0, 4 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
	}
	else
	{
		if ( pev->waterlevel < 3 )
		{
			if ( pev->vuser1.x )
				EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, pDeathSoundsDoctor[ RANDOM_LONG( 0, 6 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
			else
				EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, pDeathSounds[ RANDOM_LONG( 0, 6 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
		}
	}
}

/*
 *
 */
Vector VecVelocityForDamage(float flDamage)
{
	Vector vec(RANDOM_FLOAT(-100,100), RANDOM_FLOAT(-100,100), RANDOM_FLOAT(200,300));

	if (flDamage > -50)
		vec = vec * 0.7;
	else if (flDamage > -200)
		vec = vec * 2;
	else
		vec = vec * 10;

	return vec;
}

int TrainSpeed(int iSpeed, int iMax)
{
	float fSpeed, fMax;
	int iRet = 0;

	fMax = (float)iMax;
	fSpeed = iSpeed;

	fSpeed = fSpeed/fMax;

	if (iSpeed < 0)
		iRet = TRAIN_BACK;
	else if (iSpeed == 0)
		iRet = TRAIN_NEUTRAL;
	else if (fSpeed < 0.33)
		iRet = TRAIN_SLOW;
	else if (fSpeed < 0.66)
		iRet = TRAIN_MEDIUM;
	else
		iRet = TRAIN_FAST;

	return iRet;
}

// override takehealth
// bitsDamageType indicates type of damage healed.

int CBasePlayer :: TakeHealth( float flHealth, int bitsDamageType )
{
	return CBaseMonster :: TakeHealth (flHealth, bitsDamageType);

}

Vector CBasePlayer :: GetGunPosition( )
{
//	UTIL_MakeVectors(pev->v_angle);
//	m_HackedGunPos = pev->view_ofs;
	Vector origin;

	origin = pev->origin + pev->view_ofs;

	return origin;
}

//=========================================================
// TraceAttack
//=========================================================
void CBasePlayer :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	if ( b_GoingThroughADoor )
		return;

	if ( pev->takedamage )
	{
		m_LastHitGroup = ptr->iHitgroup;

		switch ( ptr->iHitgroup )
		{
		case HITGROUP_GENERIC:
			break;
		case HITGROUP_HEAD:
			flDamage *= gSkillData.plrHead;
			break;
		case HITGROUP_CHEST:
			flDamage *= gSkillData.plrChest;
			break;
		case HITGROUP_STOMACH:
			flDamage *= gSkillData.plrStomach;
			break;
		case HITGROUP_LEFTARM:
		case HITGROUP_RIGHTARM:
			flDamage *= gSkillData.plrArm;
			break;
		case HITGROUP_LEFTLEG:
		case HITGROUP_RIGHTLEG:
			flDamage *= gSkillData.plrLeg;
			break;
		case 8:
			flDamage = 0;
		case 9:
			flDamage = gSkillData.plrHead;
			break;
		default:
			break;
		}

		// buz: player dont bleeds
	//	SpawnBlood(ptr->vecEndPos, BloodColor(), flDamage);// a little surface blood.
	//	TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
	}
}

/*
	Take some damage.
	NOTE: each call to TakeDamage with bitsDamageType set to a time-based damage
	type will cause the damage time countdown to be reset.  Thus the ongoing effects of poison, radiation
	etc are implemented with subsequent calls to TakeDamage using DMG_GENERIC.
*/

#define ARMOR_RATIO	 0.2	// Armor Takes 80% of the damage
#define ARMOR_BONUS  0.5	// Each Point of Armor is work 1/x points of health

int CBasePlayer :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if ( b_GoingThroughADoor )
		return 0;

	// have suit diagnose the problem - ie: report damage type
	int bitsDamage = bitsDamageType;
	int ffound = TRUE;
	int fmajor;
	int fcritical;
	int fTookDamage;
	int ftrivial;
	float flRatio;
	float flBonus;
	float flHealthPrev = pev->health;

	flBonus = ARMOR_BONUS;
	flRatio = ARMOR_RATIO;

	if ( ( bitsDamageType & DMG_BLAST ) && g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		// blasts damage armor more.
		flBonus *= 2;
	}

	// Already dead
	if ( !IsAlive() )
		return 0;
	// go take the damage first


	CBaseEntity *pAttacker = CBaseEntity::Instance(pevAttacker);

	if ( g_pGameRules && !g_pGameRules->FPlayerCanTakeDamage( this, pAttacker ) )
	{
		// Refuse the damage
		return 0;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && ( pAttacker->pev->flags & FL_MONSTER ) && !g_iSurvivalMode )
	{
		flDamage *= 1.2;

		float flDamageExtra = 0;
		flDamageExtra = CountConnectedPlayers() * 0.2;

		flDamage *= 1 + flDamageExtra;
	}

	if ( flDamage < 40 && g_iSurvivalMode )
		flDamage *= 0.8;

	// keep track of amount of damage last sustained
	m_lastDamageAmount = flDamage;

	// Armor.
	if (pev->armorvalue && !(bitsDamageType & (DMG_FALL | DMG_DROWN | DMG_CLUB | DMG_SLASH | DMG_NERVEGAS)) )// armor doesn't protect against fall or drown damage!
	{
		float flNew = flDamage * flRatio;

		float flArmor;

		flArmor = (flDamage - flNew) * flBonus;

		// Does this use more armor than we have?
		if (flArmor > pev->armorvalue)
		{
			flArmor = pev->armorvalue;
			flArmor *= (1/flBonus);
			flNew = flDamage - flArmor;
			pev->armorvalue = 0;
		}
		else
			pev->armorvalue -= flArmor;

		flDamage = flNew;
	}

	// this cast to INT is critical!!! If a player ends up with 0.5 health, the engine will get that
	// as an int (zero) and think the player is dead! (this will incite a clientside screentilt, etc)
	fTookDamage = CBaseMonster::TakeDamage(pevInflictor, pevAttacker, (int)flDamage, bitsDamageType);

	// reset damage time countdown for each type of time based damage player just sustained

	{
		for (int i = 0; i < CDMG_TIMEBASED; i++)
			if (bitsDamageType & (DMG_PARALYZE << i))
				m_rgbTimeBasedDamage[i] = 0;
	}

	// tell director about it
	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// take damage event
		WRITE_SHORT( ENTINDEX(this->edict()) );	// index number of primary entity
		WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		WRITE_LONG( 5 );   // eventflags (priority and flags)
	MESSAGE_END();


	// how bad is it, doc?

	ftrivial = (pev->health > 75 || m_lastDamageAmount < 5);
	fmajor = (m_lastDamageAmount > 25);
	fcritical = (pev->health < 30);

	// handle all bits set in this damage message,
	// let the suit give player the diagnosis

	// UNDONE: add sounds for types of damage sustained (ie: burn, shock, slash )

	// UNDONE: still need to record damage and heal messages for the following types

		// DMG_BURN
		// DMG_FREEZE
		// DMG_BLAST
		// DMG_SHOCK

	m_bitsDamageType |= bitsDamage; // Save this so we can report it to the client
	m_bitsHUDDamage = -1;  // make sure the damage bits get resent

	// buz: new punch system
	if (bitsDamage & (DMG_BULLET | DMG_BLAST | (1 << 25)) && pevInflictor && pevInflictor != pev)
	{
		float punch, pmax, divide;

		if (bitsDamage & DMG_BLAST )
		{
			pmax = CVAR_GET_FLOAT("blast_punch_max");
			divide = CVAR_GET_FLOAT("blast_punch_divide");
		}
		else
		{
			pmax = CVAR_GET_FLOAT("bullet_punch_max");
			divide = CVAR_GET_FLOAT("bullet_punch_divide");
		}

		if (divide == 0) divide = 1;

		punch = (flDamage > pmax ? pmax : flDamage) / divide;
		Vector to = pevInflictor->origin - pev->origin;
		to.z = 0;
		to = to.Normalize();
		Vector tempAngle = Vector(0, pev->v_angle.y, 0);
		UTIL_MakeVectors(tempAngle);

		ViewPunch(DotProduct(gpGlobals->v_forward, to) * punch,
			DotProduct(gpGlobals->v_right, to) * punch,
			DotProduct(gpGlobals->v_right, to) * punch);
	}
	else
	{
	//	pev->punchangle.x = -2; // old punchangle
		ViewPunch(1, 0, 0);
	}

	if ( flDamage < pev->health && !( bitsDamageType & DMG_FALL ) )
		UTIL_ScreenFade( this, Vector( 48, 0, 0 ), 0.2, 0, 50, 0 );

	if ( pAttacker && !FClassnameIs( pAttacker->edict(), "trigger_hurt" ) )
		playerstats.damagetaken += flDamage;

	Pain();
	return fTookDamage; // buz" no hev sounds

	while (fTookDamage && (!ftrivial || (bitsDamage & DMG_TIMEBASED)) && ffound && bitsDamage)
	{
		ffound = FALSE;

		if (bitsDamage & DMG_CLUB)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture
			bitsDamage &= ~DMG_CLUB;
			ffound = TRUE;
		}
		if (bitsDamage & (DMG_FALL | DMG_CRUSH))
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG5", FALSE, SUIT_NEXT_IN_30SEC);	// major fracture
			else
				SetSuitUpdate("!HEV_DMG4", FALSE, SUIT_NEXT_IN_30SEC);	// minor fracture

			bitsDamage &= ~(DMG_FALL | DMG_CRUSH);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_BULLET)
		{
			if (m_lastDamageAmount > 5)
				SetSuitUpdate("!HEV_DMG6", FALSE, SUIT_NEXT_IN_30SEC);	// blood loss detected
			//else
			//	SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_BULLET;
			ffound = TRUE;
		}

		if (bitsDamage & (1 << 25))
		{
			bitsDamage &= ~(1 << 25);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_SLASH)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG1", FALSE, SUIT_NEXT_IN_30SEC);	// major laceration
			else
				SetSuitUpdate("!HEV_DMG0", FALSE, SUIT_NEXT_IN_30SEC);	// minor laceration

			bitsDamage &= ~DMG_SLASH;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_SONIC)
		{
			if (fmajor)
				SetSuitUpdate("!HEV_DMG2", FALSE, SUIT_NEXT_IN_1MIN);	// internal bleeding
			bitsDamage &= ~DMG_SONIC;
			ffound = TRUE;
		}

		if (bitsDamage & (DMG_POISON | DMG_PARALYZE))
		{
			SetSuitUpdate("!HEV_DMG3", FALSE, SUIT_NEXT_IN_1MIN);	// blood toxins detected
			bitsDamage &= ~(DMG_POISON | DMG_PARALYZE);
			ffound = TRUE;
		}

		if (bitsDamage & DMG_ACID)
		{
			SetSuitUpdate("!HEV_DET1", FALSE, SUIT_NEXT_IN_1MIN);	// hazardous chemicals detected
			bitsDamage &= ~DMG_ACID;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_NERVEGAS)
		{
			SetSuitUpdate("!HEV_DET0", FALSE, SUIT_NEXT_IN_1MIN);	// biohazard detected
			bitsDamage &= ~DMG_NERVEGAS;
			ffound = TRUE;
		}

		if (bitsDamage & DMG_RADIATION)
		{
			SetSuitUpdate("!HEV_DET2", FALSE, SUIT_NEXT_IN_1MIN);	// radiation detected
			bitsDamage &= ~DMG_RADIATION;
			ffound = TRUE;
		}
		if (bitsDamage & DMG_SHOCK)
		{
			bitsDamage &= ~DMG_SHOCK;
			ffound = TRUE;
		}
	}

	if (fTookDamage && !ftrivial && fmajor && flHealthPrev >= 75)
	{
		// first time we take major damage...
		// turn automedic on if not on
		SetSuitUpdate("!HEV_MED1", FALSE, SUIT_NEXT_IN_30MIN);	// automedic on

		// give morphine shot if not given recently
		SetSuitUpdate("!HEV_HEAL7", FALSE, SUIT_NEXT_IN_30MIN);	// morphine shot
	}

	if (fTookDamage && !ftrivial && fcritical && flHealthPrev < 75)
	{

		// already took major damage, now it's critical...
		if (pev->health < 6)
			SetSuitUpdate("!HEV_HLTH3", FALSE, SUIT_NEXT_IN_10MIN);	// near death
		else if (pev->health < 20)
			SetSuitUpdate("!HEV_HLTH2", FALSE, SUIT_NEXT_IN_10MIN);	// health critical

		// give critical health warnings
		if (!RANDOM_LONG(0,3) && flHealthPrev < 50)
			SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
	}

	// if we're taking time based damage, warn about its continuing effects
	if (fTookDamage && (bitsDamageType & DMG_TIMEBASED) && flHealthPrev < 75)
		{
			if (flHealthPrev < 50)
			{
				if (!RANDOM_LONG(0,3))
					SetSuitUpdate("!HEV_DMG7", FALSE, SUIT_NEXT_IN_5MIN); //seek medical attention
			}
			else
				SetSuitUpdate("!HEV_HLTH1", FALSE, SUIT_NEXT_IN_10MIN);	// health dropping
		}

	return fTookDamage;
}

void CBasePlayer::WeaponMelee( void )
{
	if ( !m_pActiveItem )
		return;

	int damage = gSkillData.DmgWeaponMelee;

	switch ( m_pActiveItem->m_iId )
	{
	case WEAPON_GLOCK:
		SwingSound();
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM, 0, PITCH_NORM );

		ViewPunch( 1, 1, 8 );
		break;
	case 10:
		break;
	default:
		SwingSound();
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM, 0, PITCH_NORM );

		ViewPunch( 1, 1, 8 );
		break;
	}

	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, damage, gpGlobals->v_forward, &tr, DMG_NEVERGIB | (1 << 26) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() && pEntity->Classify() != CLASS_MACHINE )
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/punch_smack.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.15, 2 );
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/punch_smack.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		}
	}
}



//=========================================================
// PackDeadPlayerItems - call this when a player dies to
// pack up the appropriate weapons and ammo items, and to
// destroy anything that shouldn't be packed.
//
// This is pretty brute force :(
//=========================================================
void CBasePlayer::PackDeadPlayerItems( void )
{
#if 0
	int iWeaponRules;
	int iAmmoRules;
	int i;
	CBasePlayerWeapon *rgpPackWeapons[ 20 ];// 20 hardcoded for now. How to determine exactly how many weapons we have?
	int iPackAmmo[ MAX_AMMO_SLOTS + 1];
	int iPW = 0;// index into packweapons array
	int iPA = 0;// index into packammo array

	memset(rgpPackWeapons, NULL, sizeof(rgpPackWeapons) );
	memset(iPackAmmo, -1, sizeof(iPackAmmo) );

	// get the game rules 
	iWeaponRules = g_pGameRules->DeadPlayerWeapons( this );
 	iAmmoRules = g_pGameRules->DeadPlayerAmmo( this );

	if ( iWeaponRules == GR_PLR_DROP_GUN_NO && iAmmoRules == GR_PLR_DROP_AMMO_NO )
	{
		// nothing to pack. Remove the weapons and return. Don't call create on the box!
		RemoveAllItems( TRUE );
		return;
	}

// go through all of the weapons and make a list of the ones to pack
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			// there's a weapon here. Should I pack it?
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[ i ];

			while ( pPlayerItem )
			{
				switch( iWeaponRules )
				{
				case GR_PLR_DROP_GUN_ACTIVE:
					if ( m_pActiveItem && pPlayerItem == m_pActiveItem )
					{
						// this is the active item. Pack it.
						rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					}
					break;

				case GR_PLR_DROP_GUN_ALL:
					rgpPackWeapons[ iPW++ ] = (CBasePlayerWeapon *)pPlayerItem;
					break;

				default:
					break;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

// now go through ammo and make a list of which types to pack.
	if ( iAmmoRules != GR_PLR_DROP_AMMO_NO )
	{
		for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
		{
			if ( m_rgAmmo[ i ] > 0 )
			{
				// player has some ammo of this type.
				switch ( iAmmoRules )
				{
				case GR_PLR_DROP_AMMO_ALL:
					iPackAmmo[ iPA++ ] = i;
					break;

				case GR_PLR_DROP_AMMO_ACTIVE:
					if ( m_pActiveItem && i == m_pActiveItem->PrimaryAmmoIndex() ) 
					{
						// this is the primary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					else if ( m_pActiveItem && i == m_pActiveItem->SecondaryAmmoIndex() ) 
					{
						// this is the secondary ammo type for the active weapon
						iPackAmmo[ iPA++ ] = i;
					}
					break;

				default:
					break;
				}
			}
		}
	}

// create a box to pack the stuff into.
	CWeaponBox *pWeaponBox = (CWeaponBox *)CBaseEntity::Create( "weaponbox", pev->origin, pev->angles, edict() );

	pWeaponBox->pev->angles.x = 0;// don't let weaponbox tilt.
	pWeaponBox->pev->angles.z = 0;

	pWeaponBox->SetThink(& CWeaponBox::Kill );
	pWeaponBox->SetNextThink( 120 );

// back these two lists up to their first elements
	iPA = 0;
	iPW = 0;

// pack the ammo
	while ( iPackAmmo[ iPA ] != -1 )
	{
		pWeaponBox->PackAmmo( MAKE_STRING( CBasePlayerItem::AmmoInfoArray[ iPackAmmo[ iPA ] ].pszName ), m_rgAmmo[ iPackAmmo[ iPA ] ] );
		iPA++;
	}

// now pack all of the items in the lists
	while ( rgpPackWeapons[ iPW ] )
	{
		// weapon unhooked from the player. Pack it into der box.
		pWeaponBox->PackWeapon( rgpPackWeapons[ iPW ] );

		iPW++;
	}

	pWeaponBox->pev->velocity = pev->velocity * 1.2;// weaponbox has player's velocity, then some.

	RemoveAllItems( TRUE );// now strip off everything that wasn't handled by the code above.
#endif
}

void CBasePlayer::RemoveAllItems( BOOL removeSuit )
{
	if (m_pActiveItem)
	{
		ResetAutoaim( );
		m_pActiveItem->Holster( );
		m_pActiveItem = NULL;
	}

	m_pLastItem = NULL;

	int i;
	CBasePlayerItem *pPendingItem;
	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		m_pActiveItem = m_rgpPlayerItems[i];
		while (m_pActiveItem)
		{
			if ( !strcmp( m_sQuickSel1, STRING( m_pActiveItem->pev->classname ) ) )
			{
				strcpy( m_sQuickSel1, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[1] = 0;
			}

			if ( !strcmp( m_sQuickSel2, STRING( m_pActiveItem->pev->classname ) ) )
			{
				strcpy( m_sQuickSel2, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[2] = 0;
			}

			if ( !strcmp( m_sQuickSel3, STRING( m_pActiveItem->pev->classname ) ) )
			{
				strcpy( m_sQuickSel3, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 3 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[3] = 0;
			}

			pPendingItem = m_pActiveItem->m_pNext;
			m_pActiveItem->Drop( );
			m_pActiveItem = pPendingItem;
		}
		m_rgpPlayerItems[i] = NULL;
	}
	m_pActiveItem = NULL;

	pev->viewmodel		= 0;
	pev->weaponmodel	= 0;

	if ( removeSuit )
		pev->weapons = 0;
	else
		pev->weapons &= ~WEAPON_ALLWEAPONS;

	for ( i = 0; i < MAX_AMMO_SLOTS;i++)
	{
		m_rgAmmo[i] = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
			WRITE_BYTE( i );
			WRITE_SHORT( m_rgAmmo[i] );
		MESSAGE_END();
	}

	CBasePlayerItem *pItem = NULL;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			pItem = m_rgpPlayerItems[i];

			while ( pItem )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pev );
					WRITE_BYTE( pItem->m_iId );
					WRITE_BYTE( pItem->iMaxClip() );
					WRITE_SHORT( m_rgAmmo[ ((CBasePlayerWeapon *)pItem)->m_iPrimaryAmmoType ] );
				MESSAGE_END();

				pItem = pItem->m_pNext;
			}
		}
	}

	UpdateClientData();
	// send Selected Weapon Message to our client
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0);
		WRITE_BYTE(0);
	MESSAGE_END();
}

//LRC
void CBasePlayer::RemoveAmmo( const char* szName, int iAmount )
{
//	ALERT(at_console, "RemoveAmmo(\"%s\", %d): \n", szName, iAmount);

	if (iAmount == -3 || iAmount == -1)
	{
		return;
	}

	int x = GetAmmoIndex(szName);

	if (iAmount > 0)
	{
		m_rgAmmo[x] -= iAmount;
		if (m_rgAmmo[x] < 0)
		{
			m_rgAmmo[x] = 0;
//			ALERT(at_console, "Reduce to 0\n");
		}
//		else
//			ALERT(at_console, "Reduce\n");
	}
	else
	{
//		ALERT(at_console, "All\n");
		m_rgAmmo[x] = 0;
	}
}

//LRC
void CBasePlayer::RemoveItems( int iWeaponMask, int i9mm, int i357, int iBuck, int iBolt, int iARGren, int iRock, int iUranium, int iSatchel, int iSnark, int iTrip, int iGren, int iHornet,
							  int iAps, int iBarret, int iAk, int iAsval, int iRpk, int iGroza, int iPK)
{
	int i;
	CBasePlayerItem *pCurrentItem;

	RemoveAmmo("9mm", i9mm);
	RemoveAmmo("357", i357);
	RemoveAmmo("buckshot", iBuck);
	RemoveAmmo("bolts", iBolt);
	RemoveAmmo("ARgrenades", iARGren);
	RemoveAmmo("uranium", iUranium);
	RemoveAmmo("rockets", iRock);
	RemoveAmmo("Satchel Charge", iSatchel);
	RemoveAmmo("Snarks", iSnark);
	RemoveAmmo("Trip Mine", iTrip);
	RemoveAmmo("Hand Grenade", iGren);
	RemoveAmmo("Hornets", iHornet);
	// buz: paranoia ammos
	RemoveAmmo("aps", iAps);
	RemoveAmmo("barret", iBarret);
	RemoveAmmo("ak", iAk);
	RemoveAmmo("asval", iAsval);
	RemoveAmmo("rpk", iRpk);
	RemoveAmmo("groza", iGroza);
	RemoveAmmo("painkillers", iPK);

	for (i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i] == NULL)
			continue;
		pCurrentItem = m_rgpPlayerItems[i];
		while (pCurrentItem->m_pNext)
		{
			if (!(1<<pCurrentItem->m_pNext->m_iId & iWeaponMask))
			{
				((CBasePlayerWeapon*)pCurrentItem)->DrainClip(this, FALSE, i9mm, i357, iBuck, iBolt, iARGren, iRock, iUranium, iSatchel, iSnark, iTrip, iGren, iAps, iBarret, iAk, iAsval, iRpk, iGroza);
				//remove pCurrentItem->m_pNext from the list
//				ALERT(at_console, "Removing %s. (id = %d)\n", pCurrentItem->m_pNext->pszName(), pCurrentItem->m_pNext->m_iId);
				pCurrentItem->m_pNext->Drop( );
				if (m_pLastItem == pCurrentItem->m_pNext)
					m_pLastItem = NULL;
				pCurrentItem->m_pNext = pCurrentItem->m_pNext->m_pNext;
			}
			else
			{
				//we're keeping this, so we need to empty the clip
				((CBasePlayerWeapon*)pCurrentItem)->DrainClip(this, TRUE, i9mm, i357, iBuck, iBolt, iARGren, iRock, iUranium, iSatchel, iSnark, iTrip, iGren, iAps, iBarret, iAk, iAsval, iRpk, iGroza);
				//now, leave pCurrentItem->m_pNext in the list and go on to the next
//				ALERT(at_console, "Keeping %s. (id = %d)\n", pCurrentItem->m_pNext->pszName(), pCurrentItem->m_pNext->m_iId);
				pCurrentItem = pCurrentItem->m_pNext;
			}
		}
		// we've gone through items 2+, now we finish off by checking item 1.
		if (!(1<<m_rgpPlayerItems[i]->m_iId & iWeaponMask))
		{
			((CBasePlayerWeapon*)pCurrentItem)->DrainClip(this, FALSE, i9mm, i357, iBuck, iBolt, iARGren, iRock, iUranium, iSatchel, iSnark, iTrip, iGren, iAps, iBarret, iAk, iAsval, iRpk, iGroza);
//			ALERT(at_console, "Removing %s. (id = %d)\n", m_rgpPlayerItems[i]->pszName(), m_rgpPlayerItems[i]->m_iId);
			m_rgpPlayerItems[i]->Drop( );
			if (m_pLastItem == m_rgpPlayerItems[i])
				m_pLastItem = NULL;
			m_rgpPlayerItems[i] = m_rgpPlayerItems[i]->m_pNext;
		}
		else
		{
			((CBasePlayerWeapon*)pCurrentItem)->DrainClip(this, TRUE, i9mm, i357, iBuck, iBolt, iARGren, iRock, iUranium, iSatchel, iSnark, iTrip, iGren, iAps, iBarret, iAk, iAsval, iRpk, iGroza);
//			ALERT(at_console, "Keeping %s. (id = %d)\n", m_rgpPlayerItems[i]->pszName(), m_rgpPlayerItems[i]->m_iId);
		}
	}

	int suit = pev->weapons & 1<<WEAPON_SUIT;
	pev->weapons &= ~(1<<WEAPON_SUIT);
//	ALERT(at_console, "weapons was %d; ", pev->weapons);
	pev->weapons &= iWeaponMask;
//	ALERT(at_console, "now %d\n(Mask is %d)", pev->weapons, iWeaponMask);
	if (suit && !(iWeaponMask & 1))
		pev->weapons |= 1<<WEAPON_SUIT;

	// are we dropping the active item?
	if (m_pActiveItem && !(1<<m_pActiveItem->m_iId & iWeaponMask))
	{
		ResetAutoaim( );
		m_pActiveItem->Holster( );
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
		m_pActiveItem = NULL;

		UpdateClientData();
		// send Selected Weapon Message to our client
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
			WRITE_BYTE(0);
			WRITE_BYTE(0);
			WRITE_BYTE(0);
		MESSAGE_END();
	}
	else
	{
		if (m_pActiveItem && !((CBasePlayerWeapon*)m_pActiveItem)->IsUseable())
		{
			//lower the gun if it's out of ammo
			((CBasePlayerWeapon*)m_pActiveItem)->m_flTimeWeaponIdle = UTIL_WeaponTimeBase();
		}
		UpdateClientData();
	}
}

void CBasePlayer::RevivePlayer( void )
{
	m_bWaitingToFall = false;
	m_iRespawnFrames = 0;

	ResetSequenceInfo( );

	int animDesired;

	if ( pev->flags & FL_DUCKING )
		animDesired = LookupSequence( "crouch_revived" );
	else
		animDesired = LookupSequence( "revived" );

	if ( animDesired == -1 )
		animDesired = 0;

	if ( pev->sequence != animDesired || !m_fSequenceLoops )
		pev->frame = 0;

	pev->gaitsequence = 0;
	m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

	pev->fuser3 = 3;
	m_flGetupTime = m_flSequenceOverrideTime;

	if ( pev->sequence != animDesired )
	{
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo( );
	}
}

void CBasePlayer::MultiplayerPlayerDeathThink( void )
{
	if ( pev->fuser3 == 3 && m_flGetupTime < gpGlobals->time )
	{
		pev->deadflag = DEAD_NO;
		pev->movetype = MOVETYPE_WALK;
		ClearBits( pev->flags, FL_ONGROUND );
		pev->health = 30;

		int iHealth = max( pev->health, 0 );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, plr->pev );
					WRITE_BYTE( iHealth );
					WRITE_BYTE( ENTINDEX( edict() ) );
					if ( g_pGameRules->IsMultiplayer() )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}

		if ( m_pActiveItem )
		{
			m_pActiveItem->Deploy( );

			MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( m_pActiveItem->m_iId );
				WRITE_BYTE( ((CBasePlayerWeapon *)m_pActiveItem)->m_iClip );
			MESSAGE_END();
		}

		ToFirstPerson();

		pev->fuser3 = 0;

		UTIL_ProMessage( this, " ", 4, false );
		return;
	}

	if ( m_bCoopGameOver == true && m_iGameOverCounter <= 0 && m_flDeadTime < gpGlobals->time && !m_bIssuedGameOver )
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			m_bIssuedGameOver = true;

			if ( g_pLevelChange )
			{
				for ( int playerloop = 1; playerloop <= gpGlobals->maxClients; playerloop++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( playerloop );

					if ( plr )
						g_pLevelChange->SetPlayerVariables( plr );
				}
			}

			CHANGE_LEVEL( (char *)STRING( gpGlobals->mapname ), NULL );
		}
	}

	if ( pev->fuser3 == 3 && m_flGetupTime > gpGlobals->time && pev->deadflag == DEAD_DEAD )
	{
		StudioFrameAdvance( );
		return;
	}

	if ( pev->modelindex && !m_fSequenceFinished && pev->deadflag == DEAD_DYING )
	{
		StudioFrameAdvance( );

		m_iRespawnFrames++;
		if ( m_iRespawnFrames < 120 )
			return;
	}

	if ( pev->movetype && FBitSet( pev->flags, FL_ONGROUND ) )
		pev->movetype = MOVETYPE_NONE;

	if ( pev->deadflag == DEAD_DYING )
		pev->deadflag = DEAD_DEAD;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

	UTIL_SetSize( pev, Vector( -16, -16, -36 ), Vector( 16, 16, 36 ) );
}

void CBasePlayer::MultiplayerKilled( entvars_t *pevAttacker, int iGib )
{
	ViewLock( false, 0 );

	m_iCoopDeadYaw = pev->angles.y;

	if ( m_iCoopDeadYaw == 0 )
		m_iCoopDeadYaw = 1;

	SetThink( &CBasePlayer::MultiplayerPlayerDeathThink );
	SetNextThink( 0.1 );

	m_flCoopRespawnDelayTime = gpGlobals->time + 60;
	m_flNextDeadNotifyTime = gpGlobals->time + 60;

	m_flDeadAutoReviveTime = CVAR_GET_FLOAT( "sv_auto_respawn_time" ) + gpGlobals->time;

	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	pPlayerToRevive = NULL;
	pPlayerReviving = NULL;
	m_flReviveTime = gpGlobals->time;

	if ( pev->flags & FL_FROZEN )
		EnableControl( TRUE );

	bool someonealive = false;
	bool survivalmode = false;

	CBaseEntity *pSurvivalEntity = NULL;

	while ( ( pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" ) ) != NULL )
		survivalmode = true;

	int i;
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		bool beingrevived = false;

		if ( plr )
		{
			if ( plr->pev->sequence == LookupSequence( "crouch_revived" ) )
				beingrevived = true;

			if ( plr->pev->sequence == LookupSequence( "revived" ) )
				beingrevived = true;
		}

		if ( plr && plr->pev->fuser4 == 0 && ( plr->IsAlive() || beingrevived ) )
			someonealive = true;
	}

	if ( !someonealive )
	{
		if ( survivalmode )
		{
			CBaseEntity *pSurvivalEntityCheck = NULL;

			pSurvivalEntityCheck = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" );

			for ( int players = 1; players <= gpGlobals->maxClients; players++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( players );

				if ( plr )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgTimer, NULL, plr->pev );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_SHORT( 0 );
					MESSAGE_END();

					plr->PlayerStopMP3( 1 );
				}
			}

			if ( pSurvivalEntityCheck && pSurvivalEntityCheck->pev->iuser3 == 0 )
			{
				if ( pSurvivalEntityCheck->pev->target )
				{
					GetClassPtr( (CCoFSurvivalMode *)pSurvivalEntityCheck->pev )->m_bSurvivalModeRunning = false;
					pSurvivalEntityCheck->pev->iuser3 = 1;
					FireTargets( STRING( pSurvivalEntityCheck->pev->target ), pSurvivalEntityCheck, pSurvivalEntityCheck, USE_TOGGLE, 0 );
				}
			}
		}
		else
		{
			for ( i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr )
				{
					plr->m_flDeadTime = gpGlobals->time + 10;
					plr->m_bIssuedGameOver = false;
					plr->m_bCoopGameOver = true;
					plr->m_flNextCountDown = gpGlobals->time;
					plr->m_iGameOverCounter = 10;

					MESSAGE_BEGIN( MSG_ONE, gmsgGreyFade, NULL, plr->pev );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 2 );
					MESSAGE_END();

					MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, plr->pev );
						WRITE_STRING( "Game over" );
						WRITE_STRING( NULL );
					MESSAGE_END();

					MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, plr->pev );
						WRITE_BYTE( 1 );
						WRITE_BYTE( 1 );
						WRITE_COORD( 0 );
						WRITE_STRING( "game_over.mp3" );
					MESSAGE_END();
				}
			}
		}
	}

	ToThirdPerson();

	if ( m_pActiveItem )
	{
		m_pActiveItem->DontThink();
		m_pActiveItem->SetThink( NULL );
	}

	ViewPunch( 5, 10, 80 );

	if ( pev->waterlevel >= 3 )
	{
		m_iDeathBits |= 1;
		m_iDeathBits |= 2;
		m_iDeathBits |= 4;
		m_iDeathBits |= 32;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		m_iDeathBits |= 2;
		m_iDeathBits |= 4;
	}

	b_GoingThroughADoor = false;

	m_iFlashlightFlags = 0;

	if ( pev->vuser2.x != 90 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pev );
			WRITE_COORD( 150 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		pev->vuser2.x = 90;
	}

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster( );

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor );

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// buz: spec tank
	if (m_pSpecTank)
	{
		m_pSpecTank->Use( this, this, USE_OFF, 0 );
		m_pSpecTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	CSound *pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	int animDesired;

	if ( m_bKilledLyingDown )
		animDesired = LookupSequence( "taller_lie" );
	else if ( pev->flags & FL_DUCKING )
		animDesired = LookupSequence( "crouch_death" );
	else
		animDesired = LookupSequence( "death" );

	if ( animDesired == -1 )
		animDesired = 0;

	pev->frame = 0;

	pev->gaitsequence = 0;
	m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

	pev->sequence = animDesired;
	pev->frame = 0;
	ResetSequenceInfo( );

	pev->velocity = g_vecZero;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes

	pev->deadflag		= DEAD_DYING;
	pev->velocity.x		= g_vecZero.x;
	pev->velocity.y		= g_vecZero.y;
	pev->movetype		= MOVETYPE_TOSS;

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	SetDualWieldWeapon( 0 );

	m_iDualWeaponLeftAmmo = -2;
	m_iDualWeaponRightAmmo = -2;

	// send "health" update message to zero
	m_iClientHealth = 0;
	m_iClientUnlockedClothes = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, plr->pev );
				WRITE_BYTE( m_iClientHealth );
				WRITE_BYTE( ENTINDEX( edict() ) );
				if ( g_pGameRules->IsMultiplayer() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0XFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE(0);
	MESSAGE_END();

	if ( !m_bGibbed && !( m_iDeathBits & 1 ) )
		DeathSound();

	pev->angles.x = 0;
	pev->angles.z = 0;

	m_bWaitingToFall = false;
	m_bHasGotUp = false;
}

/*
 * GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
 *
 * ENTITY_METHOD(PlayerDie)
 */
void CBasePlayer::Killed( entvars_t *pevAttacker, int iGib )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		MultiplayerKilled( pevAttacker, iGib );
		return;
	}

	CSound *pSound;

	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "common/null.wav", 1, ATTN_NORM, 0, PITCH_NORM );

	if ( m_pActiveItem )
	{
		m_pActiveItem->DontThink();
		m_pActiveItem->SetThink( NULL );
	}

	if ( pev->flags & FL_FROZEN )
		EnableControl( TRUE );

	if ( pev->waterlevel >= 3 )
	{
		m_iDeathBits |= 1;
		m_iDeathBits |= 2;
		m_iDeathBits |= 4;
		m_iDeathBits |= 32;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		m_iDeathBits |= 2;
		m_iDeathBits |= 4;
	}

	b_GoingThroughADoor = false;

	m_iFlashlightFlags = 0;

	if ( pev->vuser2.x != 90 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pev );
			WRITE_COORD( 150 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 1 );
		MESSAGE_END();

		pev->vuser2.x = 90;
	}

	// Holster weapon immediately, to allow it to cleanup
	if ( m_pActiveItem )
		m_pActiveItem->Holster( );

	g_pGameRules->PlayerKilled( this, pevAttacker, g_pevLastInflictor );

	if ( m_pTank != NULL )
	{
		m_pTank->Use( this, this, USE_OFF, 0 );
		m_pTank = NULL;
	}

	// buz: spec tank
	if (m_pSpecTank)
	{
		m_pSpecTank->Use( this, this, USE_OFF, 0 );
		m_pSpecTank = NULL;
	}

	// this client isn't going to be thinking for a while, so reset the sound until they respawn
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( edict() ) );
	{
		if ( pSound )
		{
			pSound->Reset();
		}
	}

	m_iRespawnFrames = 0;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes

	pev->deadflag		= DEAD_DYING;
	pev->movetype		= MOVETYPE_TOSS;
	ClearBits( pev->flags, FL_ONGROUND );
	if (pev->velocity.z < 10)
		pev->velocity.z += RANDOM_FLOAT(0,300);

	// clear out the suit message cache so we don't keep chattering
	SetSuitUpdate(NULL, FALSE, 0);

	SetDualWieldWeapon( 0 );

	m_iDualWeaponLeftAmmo = -2;
	m_iDualWeaponRightAmmo = -2;

	// send "health" update message to zero
	m_iClientHealth = 0;
	m_iClientUnlockedClothes = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, plr->pev );
				WRITE_BYTE( m_iClientHealth );
				WRITE_BYTE( ENTINDEX( edict() ) );
				if ( g_pGameRules->IsMultiplayer() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}

	// Tell Ammo Hud that the player is dead
	MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(0XFF);
		WRITE_BYTE(0xFF);
	MESSAGE_END();

	// reset FOV
	pev->fov = m_iFOV = m_iClientFOV = 0;

	MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
		WRITE_BYTE(0);
	MESSAGE_END();

	PlayerStopMP3( 1 );

	if ( !m_bNoDeadFade && !( m_iDeathBits & 8 ) )
		UTIL_ScreenFade( this, Vector( 128, 0, 0 ), 5, 1, 64, FFADE_OUT | FFADE_STAYOUT );

	m_flDeadTime = gpGlobals->time + 2;
	m_bIssuedGameOver = false;

	if ( ( pev->health < -40 && iGib != GIB_NEVER ) || iGib == GIB_ALWAYS )
	{
		pev->solid			= SOLID_NOT;
		GibMonster();	// This clears pev->model
		pev->effects |= EF_NODRAW;
		return;
	}

	if ( !m_bGibbed && !( m_iDeathBits & 1 ) )
		DeathSound();

	if ( !m_bGibbed && !( m_iDeathBits & 2 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 2 );
			WRITE_STRING( "" );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if ( !m_bShouldFallFreeze && !( m_iDeathBits & 32 ) )
	{
		pev->velocity.x /= 2;
		pev->velocity.y /= 2;
		pev->velocity.z /= 2;

		m_fFallingTime = gpGlobals->time + 10;
		m_bShouldFallFreeze = true;

		pev->fuser3 = 1;

		MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
			WRITE_BYTE( 1 );
		MESSAGE_END();
	}

	pev->angles.x = 0;
	pev->angles.z = 0;

	SetThink(&CBasePlayer::PlayerDeathThink);
	SetNextThink( 0.1 );
}



int CBasePlayer::GetWeaponAnimation( void )
{
	int animDesired;

	if ( m_bMuschi2_guess )
		return -1;

	switch ( m_pActiveItem->m_iId )
	{
	case 4:
		animDesired = LookupSequence( "shotgun_idle" );
		break;
	case 11:
		animDesired = LookupSequence( "rifle_idle" );
		break;
	case 9:
	case 26:
		animDesired = LookupSequence( "m16_idle" );
		break;
	case 3:
	case 7:
	case 21:
		animDesired = LookupSequence( "pistol_idle" );
		break;
	case 19:
		animDesired = LookupSequence( "vp70_idle" );
		break;
	case 2:
	case 22:
		animDesired = LookupSequence( "radio_idle" );
		break;
	case 10:
	case 16:
		animDesired = LookupSequence( "branch_idle" );
		break;
	case 12:
		animDesired = LookupSequence( "nightstick_idle" );
		break;
	case 18:
		animDesired = LookupSequence( "sledgehammer_idle" );
		break;
	case 8:
		animDesired = LookupSequence( "syringe_idle" );
		break;
	case 6:
		animDesired = LookupSequence( "switchblade_idle" );
		break;
	case 14:
		animDesired = LookupSequence( "tmp_idle" );
		break;
	case 27:
		animDesired = LookupSequence( "mp5_idle" );
		break;
	default:
		animDesired = LookupSequence( "idle" );
		break;
	}

	return animDesired;
}

// Set the activity based on an event or current state
void CBasePlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	if ( pev->fuser3 == 4 )
		return;

	if ( m_bHasGotUp == true && m_flGetupTime > gpGlobals->time )
		return;

	if ( m_bMuschi2_guess )
	{
		pev->gaitsequence = -1;
		pev->sequence = -1;
		pev->frame = 0;
		ResetSequenceInfo( );
		return;
	}

	float speed = pev->velocity.Length2D();

	int animDesired = -1;
	int gaitDesired = -1;

	if ( ( pev->fuser3 < 2 || pev->fuser3 == 25 ) && !pPlayerToRevive && !m_bWaitingToFall && IsAlive() )
	{
		if ( speed > 10 )
		{
			if ( pev->flags & (1<<18) )
			{
				animDesired = LookupSequence( "sprint" );

				gaitDesired = LookupSequence( "sprint" );

				if ( animDesired >= 0 && gaitDesired >= 0 )
				{
					if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
						pev->gaitsequence = gaitDesired;
				}
			}
			else if ( pev->flags & (1<<19) )
			{
				if ( m_pActiveItem )
					animDesired = GetWeaponAnimation();
				else
					animDesired = LookupSequence( "stamina" );

				gaitDesired = LookupSequence( "stamina" );

				if ( animDesired >= 0 && gaitDesired >= 0 )
				{
					if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
						pev->gaitsequence = gaitDesired;
				}
			}
			else
			{
				if ( m_pActiveItem )
					animDesired = GetWeaponAnimation();
				else
					animDesired = LookupSequence( "run" );

				gaitDesired = LookupSequence( "run" );

				if ( animDesired >= 0 && gaitDesired >= 0 )
				{
					if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
						pev->gaitsequence = gaitDesired;
				}
			}
		}
		else
		{
			if ( m_pActiveItem )
				animDesired = GetWeaponAnimation();
			else
				animDesired = LookupSequence( "idle" );

			gaitDesired = LookupSequence( "idle" );

			if ( animDesired >= 0 && gaitDesired >= 0 )
			{
				if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
					pev->gaitsequence = gaitDesired;
			}
		}

		if ( FBitSet( pev->flags, FL_DUCKING ) )
		{
			if ( speed > 10 )
			{
				if ( m_pActiveItem )
					animDesired = GetWeaponAnimation();
				else
					animDesired = LookupSequence( "crouch_crouch" );

				gaitDesired = LookupSequence( "crouch_crouch" );
				if ( gaitDesired >= 0 )
				{
					if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
						pev->gaitsequence = gaitDesired;
				}
			}
			else
			{
				if ( m_pActiveItem )
					animDesired = GetWeaponAnimation();
				else
					animDesired = LookupSequence( "crouch_idle" );

				gaitDesired = LookupSequence( "crouch_idle" );
				if ( gaitDesired >= 0 )
				{
					if ( pev->gaitsequence != gaitDesired && m_flGaitSequenceOverrideTime < gpGlobals->time )
						pev->gaitsequence = gaitDesired;
				}
			}
		}
	}

	// Already using the desired animation?
	if ( pev->sequence == animDesired )
		return;

	if ( m_flSequenceOverrideTime > gpGlobals->time )
		return;

	//ALERT( at_console, "Set animation to %d\n", animDesired );
	// Reset to first frame of desired animation
	pev->sequence		= animDesired;
	pev->frame			= 0;
	ResetSequenceInfo( );
}

/*
===========
TabulateAmmo
This function is used to find and store
all the ammo we have into the ammo vars.
============
*/
void CBasePlayer::TabulateAmmo()
{
	ammo_9mm = AmmoInventory( GetAmmoIndex( "9mm" ) );
	ammo_357 = AmmoInventory( GetAmmoIndex( "357" ) );
	ammo_argrens = AmmoInventory( GetAmmoIndex( "ARgrenades" ) );
	ammo_bolts = AmmoInventory( GetAmmoIndex( "bolts" ) );
	ammo_buckshot = AmmoInventory( GetAmmoIndex( "buckshot" ) );
	ammo_rockets = AmmoInventory( GetAmmoIndex( "rockets" ) );
	ammo_uranium = AmmoInventory( GetAmmoIndex( "uranium" ) );
	ammo_hornets = AmmoInventory( GetAmmoIndex( "Hornets" ) );

	// buz: Paranoia ammo types
	ammo_aps	= AmmoInventory( GetAmmoIndex( "aps" ) );
	ammo_barret	= AmmoInventory( GetAmmoIndex( "barret" ) );
	ammo_ak		= AmmoInventory( GetAmmoIndex( "ak" ) );
	ammo_asval	= AmmoInventory( GetAmmoIndex( "asval" ) );
	ammo_rpk	= AmmoInventory( GetAmmoIndex( "rpk" ) );
	ammo_groza	= AmmoInventory( GetAmmoIndex( "groza" ) );
}


/*
===========
WaterMove
============
*/
#define AIRTIME	12		// lung full of air lasts this many seconds

void CBasePlayer::WaterMove()
{
	if (pev->movetype == MOVETYPE_NOCLIP)
		return;

	if (pev->health < 0)
		return;

	// waterlevel 0 - not in water
	// waterlevel 1 - feet in water
	// waterlevel 2 - waist in water
	// waterlevel 3 - head in water

	if (pev->waterlevel != 3 || pev->watertype <= CONTENT_FLYFIELD)
	{
		// not underwater

		// play 'up for air' sound
	//	if (pev->air_finished < gpGlobals->time)
	//		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade1.wav", 1, ATTN_NORM);
	//	else if (pev->air_finished < gpGlobals->time + 9)
	//		EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/pl_wade2.wav", 1, ATTN_NORM);

		pev->air_finished = gpGlobals->time + AIRTIME;
		pev->dmg = 2;

		// if we took drowning damage, give it back slowly
		if (m_idrowndmg > m_idrownrestored)
		{
			// set drowning damage bit.  hack - dmg_drownrecover actually
			// makes the time based damage code 'give back' health over time.
			// make sure counter is cleared so we start count correctly.

			// NOTE: this actually causes the count to continue restarting
			// until all drowning damage is healed.

			m_bitsDamageType |= DMG_DROWNRECOVER;
			m_bitsDamageType &= ~DMG_DROWN;
			m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;
		}

	}
	else if (pev->watertype > CONTENT_FLYFIELD) // FLYFIELD, FLYFIELD_GRAVITY & FOG aren't really water...
	{	// fully under water
		// stop restoring damage while underwater
		m_bitsDamageType &= ~DMG_DROWNRECOVER;
		m_rgbTimeBasedDamage[itbd_DrownRecover] = 0;

		if (pev->air_finished < gpGlobals->time)		// drown!
		{
			if (pev->pain_finished < gpGlobals->time)
			{
				// take drowning damage
				pev->dmg += 1;
				if (pev->dmg > 5)
					pev->dmg = 5;
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), pev->dmg, DMG_DROWN);
				pev->pain_finished = gpGlobals->time + 1;

				// track drowning damage, give it back when
				// player finally takes a breath

				m_bWasDrowning = true;

				m_idrowndmg += pev->dmg;
			}
		}
		else
		{
			m_bitsDamageType &= ~DMG_DROWN;
		}
	}

	if (!pev->waterlevel || pev->watertype <= CONTENT_FLYFIELD )
	{
		if (FBitSet(pev->flags, FL_INWATER))
		{
			ClearBits(pev->flags, FL_INWATER);
		}
		return;
	}

	// make bubbles
	if ( pev->waterlevel >= 3 && IsAlive() )
	{
		if ( m_fUnderwaterBreatheTime < gpGlobals->time )
		{
			UTIL_MakeVectors( pev->v_angle + pev->punchangle );
			Vector vecSrc = EyePosition() + gpGlobals->v_forward * 10 - gpGlobals->v_up * 10;
			UTIL_Bubbles( vecSrc, vecSrc + Vector( 0, 0, 32 ), 5 );

			EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, "player/vocal/water_breathe.wav", 1, ATTN_NORM, 0, 100 + RANDOM_LONG( -5, 5 ) );
			m_fUnderwaterBreatheTime = gpGlobals->time + 4;
		}
	}

	if (pev->watertype == CONTENT_LAVA)		// do damage
	{
		if (pev->dmgtime < gpGlobals->time)
			TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 10 * pev->waterlevel, DMG_BURN);
	}
	else if (pev->watertype == CONTENT_SLIME)		// do damage
	{
		pev->dmgtime = gpGlobals->time + 1;
		TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), 4 * pev->waterlevel, DMG_ACID);
	}

	if (!FBitSet(pev->flags, FL_INWATER))
	{
		SetBits(pev->flags, FL_INWATER);
		pev->dmgtime = 0;
	}
}


// TRUE if the player is attached to a ladder
BOOL CBasePlayer::IsOnLadder( void )
{
	return ( pev->movetype == MOVETYPE_FLY );
}

void CBasePlayer::WheelchairPlayerDeathThink( void )
{
	if ( m_flDeadTime < gpGlobals->time && !m_bIssuedGameOver )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			m_bIssuedGameOver = true;

			MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, pev );
				WRITE_BYTE( 35 );
				if ( !( m_iDeathBits & 16 ) )
					WRITE_BYTE( 0 );
				else
					WRITE_BYTE( 1 );
			MESSAGE_END();
		}
	}

	SetNextThink( 0 );
}

void CBasePlayer::PlayerDeathThink(void)
{
	if ( pDeathScene )
	{
		float flInterval = pDeathScene->StudioFrameAdvance( );
		pDeathScene->DispatchAnimEvents( flInterval );
	}

	if ( m_flDeadTime < gpGlobals->time && !m_bIssuedGameOver )
	{
		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
		{
			m_bIssuedGameOver = true;

			if ( g_pWorld && g_pWorld->m_sCustomDoctorMap )
			{
				int holder = g_pWorld->m_sCustomDoctorMap;

				MESSAGE_BEGIN( MSG_ONE, gmsgCustomDoc, NULL, pev );
					WRITE_STRING( STRING( holder ) );
				MESSAGE_END();
			}

			MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, pev );
				WRITE_BYTE( 35 );
				if ( !( m_iDeathBits & 16 ) )
					WRITE_BYTE( 0 );
				else
					WRITE_BYTE( 1 );
			MESSAGE_END();
		}
	}

	float flForward;

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		flForward = pev->velocity.Length() - 20;
		if (flForward <= 0)
			pev->velocity = g_vecZero;
		else
			pev->velocity = flForward * pev->velocity.Normalize();
	}

//	if ( HasWeapons() )
//	{
		// we drop the guns here because weapons that have an area effect and can kill their user
		// will sometimes crash coming back from CBasePlayer::Killed() if they kill their owner because the
		// player class sometimes is freed. It's safer to manipulate the weapons once we know
		// we aren't calling into any of their code anymore through the player pointer.
//		PackDeadPlayerItems();
//	}


//	if (pev->modelindex && (!m_fSequenceFinished) && (pev->deadflag == DEAD_DYING))
//	{
//		StudioFrameAdvance( );
//
//		m_iRespawnFrames++;				// Note, these aren't necessarily real "frames", so behavior is dependent on # of client movement commands
//		if ( m_iRespawnFrames < 120 )   // Animations should be no longer than this
//			return;
//	}

	// once we're done animating our death and we're on the ground, we want to set movetype to None so our dead body won't do collisions and stuff anymore
	// this prevents a bug where the dead body would go to a player's head if he walked over it while the dead player was clicking their button to respawn
	if ( pev->movetype != MOVETYPE_NONE && FBitSet(pev->flags, FL_ONGROUND) )
		pev->movetype = MOVETYPE_NONE;

	if (pev->deadflag == DEAD_DYING)
		pev->deadflag = DEAD_DEAD;

	StopAnimation();

	pev->effects |= EF_NOINTERP;
	pev->framerate = 0.0;

/*	BOOL fAnyButtonDown = (pev->button & ~IN_SCORE );

	// wait for all buttons released
	if (pev->deadflag == DEAD_DEAD)
	{
		if (fAnyButtonDown)
			return;

		if ( g_pGameRules->FPlayerCanRespawn( this ) )
		{
			m_fDeadTime = gpGlobals->time;
			pev->deadflag = DEAD_RESPAWNABLE;
		}

		return;
	}

// if the player has been dead for one second longer than allowed by forcerespawn,
// forcerespawn isn't on. Send the player off to an intermission camera until they
// choose to respawn.
	if ( g_pGameRules->IsMultiplayer() && ( gpGlobals->time > (m_fDeadTime + 6) ) && !(m_afPhysicsFlags & PFLAG_OBSERVER) )
	{
		// go to dead camera.
		StartDeathCam();
	}

// wait for any button down,  or mp_forcerespawn is set and the respawn time is up
	if (!fAnyButtonDown
		&& !( g_pGameRules->IsMultiplayer() && forcerespawn.value > 0 && (gpGlobals->time > (m_fDeadTime + 5))) )
		return;
*/
	pev->button = 0;
	m_iRespawnFrames = 0;
	DontThink();
}

//=========================================================
// StartDeathCam - find an intermission spot and send the
// player off into observer mode
//=========================================================
void CBasePlayer::StartDeathCam( void )
{
	CBaseEntity *pSpot, *pNewSpot;
	int iRand;

	if ( pev->view_ofs == g_vecZero )
	{
		// don't accept subsequent attempts to StartDeathCam()
		return;
	}

	pSpot = UTIL_FindEntityByClassname( NULL, "info_intermission");

	if ( pSpot )
	{
		// at least one intermission spot in the world.
		iRand = RANDOM_LONG( 0, 3 );

		while ( iRand > 0 )
		{
			pNewSpot = UTIL_FindEntityByTargetname( pSpot, "info_intermission");

			if ( pNewSpot )
			{
				pSpot = pNewSpot;
			}

			iRand--;
		}

	//	CopyToBodyQue( pev );
		StartObserver( pSpot->pev->origin, pSpot->pev->v_angle );
	}
	else
	{
		// no intermission spot. Push them up in the air, looking down at their corpse
		TraceResult tr;
	//	CopyToBodyQue( pev );
		UTIL_TraceLine( pev->origin, pev->origin + Vector( 0, 0, 128 ), ignore_monsters, edict(), &tr );
		StartObserver( tr.vecEndPos, UTIL_VecToAngles( tr.vecEndPos - pev->origin  ) );
		return;
	}
}

void CBasePlayer::StartObserver( Vector vecPosition, Vector vecViewAngle )
{
	m_afPhysicsFlags |= PFLAG_OBSERVER;

	pev->view_ofs = g_vecZero;
	pev->angles = pev->v_angle = vecViewAngle;
	pev->fixangle = TRUE;
	pev->solid = SOLID_NOT;
	pev->takedamage = DAMAGE_NO;
	pev->movetype = MOVETYPE_NONE;
	pev->modelindex = 0;
	UTIL_SetOrigin( this, vecPosition );
}

//
// PlayerUse - handles USE keypress
//
#define	PLAYER_SEARCH_RADIUS	(float)80
#define PLAYER_DISTUSE_RADIUS	(float)128

void CBasePlayer::PlayerUse ( void )
{
	if ( m_bInPhoneCall )
		return;

	if ( m_iUsingComputerOrPhone )
		return;

	if ( IsOnLadder() )
		return;

	// Was use pressed or released?
	if ( ! ((pev->button | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	if ( m_fUseTime > gpGlobals->time )
		return;

	if ( pPlayerToRevive )
		return;

	Vector vecSrc = pev->origin + pev->view_ofs;
	Vector vecEnd = pev->origin + pev->view_ofs + (gpGlobals->v_forward * PLAYER_SEARCH_RADIUS);

	// Hit Use on a train?
	if ( m_afButtonPressed & IN_USE )
	{
		if ( m_pTank != NULL )
		{
			// Stop controlling the tank
			// TODO: Send HUD Update
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
			return;
		}
		// buz: spec tank
		else if (m_pSpecTank)
		{
			m_pSpecTank->Use( this, this, USE_OFF, 0 );
			m_pSpecTank = NULL;
			return;
		}
		else
		{
			if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
			{
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
			else
			{	// Start controlling the train!
				CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );

				if ( pTrain && !(pev->button & IN_JUMP) && FBitSet(pev->flags, FL_ONGROUND) &&
					 (pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) && pTrain->OnControls(pev) )
				{
					m_afPhysicsFlags |= PFLAG_ONTRAIN;
					m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
					m_iTrain |= TRAIN_NEW;
				//	EMIT_SOUND( ENT(pev), CHAN_ITEM, "plats/train_use1.wav", 0.8, ATTN_NORM);
					return;
				}
			}
		}
	}

	CBaseEntity *pObject = NULL;
	CBaseEntity *pClosest = NULL;
	Vector		vecLOS;
	float flMaxDot = VIEW_FIELD_NARROW;
	float flDot;
	TraceResult tr;
	int caps;

	UTIL_MakeVectors ( pev->v_angle );// so we know which way we are facing

	//LRC- try to get an exact entity to use.
	// (is this causing "use-buttons-through-walls" problems? Surely not!)
	UTIL_TraceLine( pev->origin + pev->view_ofs,
					pev->origin + pev->view_ofs + (gpGlobals->v_forward * PLAYER_SEARCH_RADIUS),
					dont_ignore_monsters, ENT(pev), &tr );
	if (tr.pHit)
	{
		pObject = CBaseEntity::Instance(tr.pHit);
		if (!pObject || !(pObject->ObjectCaps() & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE)))
		{
			pObject = NULL;
		}
	}

	if (!pObject) //LRC- couldn't find a direct solid object to use, try the normal method
	{
		while ((pObject = UTIL_FindEntityInSphere( pObject, pev->origin, PLAYER_SEARCH_RADIUS )) != NULL)
		{
			caps = pObject->ObjectCaps();
			if (pObject != this && caps & (FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE) && !(caps & FCAP_ONLYDIRECT_USE)) //LRC - we can't see 'direct use' entities in this section
			{
				// buz: test special usability by sequence bbox for monsters
				CBaseMonster *pMonster = pObject->MyMonsterPointer();
				if (pMonster)
				{
					Vector mins, maxs;
					pMonster->ExtractBbox(pMonster->pev->sequence, mins, maxs);
					vecLOS = (((mins + maxs) * 0.5) + pMonster->pev->origin - (pev->origin + pev->view_ofs));
					vecLOS = UTIL_ClampVectorToBox( vecLOS, ((mins + maxs) * 0.5) );
				}
				else
				{
					vecLOS = (VecBModelOrigin( pObject->pev ) - (pev->origin + pev->view_ofs));

					// This essentially moves the origin of the target to the corner nearest the player to test to see
					// if it's "hull" is in the view cone
					vecLOS = UTIL_ClampVectorToBox( vecLOS, pObject->pev->size * 0.5 );
				}

				flDot = DotProduct (vecLOS , gpGlobals->v_forward);
				if (flDot > flMaxDot || vecLOS == g_vecZero ) // LRC - if the player is standing inside this entity, it's also ok to use it.
				{// only if the item is in front of the user
					pClosest = pObject;
					flMaxDot = flDot;
//					ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
				}
//				ALERT( at_console, "%s : %f\n", STRING( pObject->pev->classname ), flDot );
			}
		}
		pObject = pClosest;
	}

	if (!pObject) // buz - try distance use
	{
		UTIL_TraceLine( pev->origin + pev->view_ofs,
				pev->origin + pev->view_ofs + (gpGlobals->v_forward * PLAYER_DISTUSE_RADIUS),
				dont_ignore_monsters, ENT(pev), &tr );
		if (tr.pHit)
		{
			pObject = CBaseEntity::Instance(tr.pHit);
			if (!pObject || !(pObject->ObjectCaps() & FCAP_DISTANCE_USE))
			{
				pObject = NULL;
			}
		}
	}

	// Found an object
	if (pObject && pObject != this )
	{
		//!!!UNDONE: traceline here to prevent USEing buttons through walls
		caps = pObject->ObjectCaps();

	//	if ( m_afButtonPressed & IN_USE )
	//		EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/wpn_select.wav", 0.4, ATTN_NORM);

		if ( ( (pev->button & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
			 ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pObject->Use( this, this, USE_SET, 1 );

			if ( !( caps & FCAP_CONTINUOUS_USE ) )
				m_fUseTime = gpGlobals->time + 0.5;
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		// (actually, nothing uses on/off. They're either continuous - rechargers and momentary
		// buttons - or they're impulse - buttons, doors, tanks, trains, etc.) --LRC
		else if ( (m_afButtonReleased & IN_USE) && (pObject->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
		{
			pObject->Use( this, this, USE_SET, 0 );

			if ( !( caps & FCAP_CONTINUOUS_USE ) )
				m_fUseTime = gpGlobals->time + 0.5;
		}
	}
//	else
//	{
//		if ( m_afButtonPressed & IN_USE )
//			EMIT_SOUND( ENT(pev), CHAN_ITEM, "common/wpn_denyselect.wav", 0.4, ATTN_NORM);
//	}
}

float CBasePlayer::CalculateAccuracy( void )
{
	float accuracy = 100;

	if ( playerstats.shots_fired > 0 && playerstats.shots_hit > 0 )
		accuracy = (float)playerstats.shots_hit / playerstats.shots_fired * 100;

	if ( playerstats.shots_fired > 0 && playerstats.shots_hit == 0 )
		accuracy = 0;

	playerstats.gunaccuracy = accuracy;

	return accuracy;
}

void CBasePlayer::Jump()
{
	Vector		vecWallCheckDir;// direction we're tracing a line to find a wall when walljumping
	Vector		vecAdjustedVelocity;
	Vector		vecSpot;
	TraceResult	tr;

	if ( m_bInPhoneCall )
		return;

	if (FBitSet(pev->flags, FL_WATERJUMP))
		return;

	if (pev->waterlevel >= 2 && pev->watertype != CONTENT_FOG)
		return;

	// jump velocity is sqrt( height * gravity * 2)

	// If this isn't the first frame pressing the jump button, break out.
	if ( !FBitSet( m_afButtonPressed, IN_JUMP ) )
		return;         // don't pogo stick

	if ( !(pev->flags & FL_ONGROUND) || !pev->groundentity )
		return;

	int animDesired = LookupSequence( "jump" );

	if ( animDesired == -1 )
		animDesired = 0;

	if ( pev->sequence != animDesired || !m_fSequenceLoops )
		pev->frame = 0;

	pev->gaitsequence = 0;
	m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

	if ( pev->sequence != animDesired )
	{
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo( );
	}

// many features in this function use v_forward, so makevectors now.
	UTIL_MakeVectors (pev->angles);

	// ClearBits(pev->flags, FL_ONGROUND);		// don't stairwalk

	if ( !m_bInfiniteStamina )
		m_fStamina -= 15;

	if ( pev->waterlevel > 0 )
		UTIL_SplashCalculation( this, 3 );

	SetAnimation( PLAYER_JUMP );

	ViewPunch( -6, 1, 1 );

	if ( m_fLongJump && (pev->button & IN_DUCK) && ( pev->flDuckTime > 0 ) && pev->velocity.Length() > 50 )
	{
		SetAnimation( PLAYER_SUPERJUMP );
	}

	m_bJustJumped = true;
	m_flMaxJustJumpedTime = gpGlobals->time + 3;

	// buz: report active item about jumping
	if (m_pActiveItem)
		m_pActiveItem->PlayerJump();

	// If you're standing on a conveyor, add its velocity to yours (for momentum)
	entvars_t *pevGround = VARS(pev->groundentity);
	if ( pevGround && (pevGround->flags & FL_CONVEYOR) )
	{
		pev->velocity = pev->velocity + pev->basevelocity;
	}
}



// This is a glorious hack to find free space when you've crouched into some solid space
// Our crouching collisions do not work correctly for some reason and this is easier
// than fixing the problem :(
void FixPlayerCrouchStuck( edict_t *pPlayer )
{
	TraceResult trace;

	// Move up as many as 18 pixels if the player is stuck.
	for ( int i = 0; i < 18; i++ )
	{
		UTIL_TraceHull( pPlayer->v.origin, pPlayer->v.origin, dont_ignore_monsters, head_hull, pPlayer, &trace );
		if ( trace.fStartSolid )
			pPlayer->v.origin.z ++;
		else
			break;
	}
}

void CBasePlayer::Duck( )
{
	if (pev->button & IN_DUCK)
	{
		if ( m_IdealActivity != ACT_LEAP )
		{
			SetAnimation( PLAYER_WALK );
		}
	}
}

//
// ID's player as such.
//
int  CBasePlayer::Classify ( void )
{
	return CLASS_PLAYER;
}


void CBasePlayer::AddPoints( int score, BOOL bAllowNegativeScore )
{
/*	// Positive score always adds
	if ( score < 0 )
	{
		if ( !bAllowNegativeScore )
		{
			if ( pev->frags < 0 )		// Can't go more negative
				return;

			if ( -score > pev->frags )	// Will this go negative?
			{
				score = -pev->frags;		// Sum will be 0
			}
		}
	}

	pev->frags += score;

	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(edict()) );
		WRITE_SHORT( pev->frags );
		WRITE_SHORT( m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( g_pGameRules->GetTeamIndex( m_szTeamName ) + 1 );
	MESSAGE_END();
*/
}


void CBasePlayer::AddPointsToTeam( int score, BOOL bAllowNegativeScore )
{
	int index = entindex();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer && i != index )
		{
			if ( g_pGameRules && g_pGameRules->PlayerRelationship( this, pPlayer ) == GR_TEAMMATE )
			{
				pPlayer->AddPoints( score, bAllowNegativeScore );
			}
		}
	}
}

//Player ID
void CBasePlayer::InitStatusBar()
{
	m_flStatusBarDisappearDelay = 0;
	m_SbarString1[0] = m_SbarString0[0] = 0;
}

void CBasePlayer::UpdateStatusBar()
{
/*	int newSBarState[ SBAR_END ];
	char sbuf0[ SBAR_STRING_SIZE ];
	char sbuf1[ SBAR_STRING_SIZE ];

	memset( newSBarState, 0, sizeof(newSBarState) );
	strcpy( sbuf0, m_SbarString0 );
	strcpy( sbuf1, m_SbarString1 );

	// Find an ID Target
	TraceResult tr;
	UTIL_MakeVectors( pev->v_angle + pev->punchangle );
	Vector vecSrc = EyePosition();
	Vector vecEnd = vecSrc + (gpGlobals->v_forward * MAX_ID_RANGE);
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, edict(), &tr);

	if (tr.flFraction != 1.0)
	{
		if ( !FNullEnt( tr.pHit ) )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

			if (pEntity->Classify() == CLASS_PLAYER )
			{
				newSBarState[ SBAR_ID_TARGETNAME ] = ENTINDEX( pEntity->edict() );
				strcpy( sbuf1, "1 %p1\n2 Health: %i2%%\n3 Armor: %i3%%" );

				// allies and medics get to see the targets health
				if ( g_pGameRules->PlayerRelationship( this, pEntity ) == GR_TEAMMATE )
				{
					newSBarState[ SBAR_ID_TARGETHEALTH ] = 100 * (pEntity->pev->health / pEntity->pev->max_health);
					newSBarState[ SBAR_ID_TARGETARMOR ] = pEntity->pev->armorvalue; //No need to get it % based since 100 it's the max.
				}

				m_flStatusBarDisappearDelay = gpGlobals->time + 1.0;
			}
		}
		else if ( m_flStatusBarDisappearDelay > gpGlobals->time )
		{
			// hold the values for a short amount of time after viewing the object
			newSBarState[ SBAR_ID_TARGETNAME ] = m_izSBarState[ SBAR_ID_TARGETNAME ];
			newSBarState[ SBAR_ID_TARGETHEALTH ] = m_izSBarState[ SBAR_ID_TARGETHEALTH ];
			newSBarState[ SBAR_ID_TARGETARMOR ] = m_izSBarState[ SBAR_ID_TARGETARMOR ];
		}
	}

	BOOL bForceResend = FALSE;

	if ( strcmp( sbuf0, m_SbarString0 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_STRING( sbuf0 );
		MESSAGE_END();

		strcpy( m_SbarString0, sbuf0 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	if ( strcmp( sbuf1, m_SbarString1 ) )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusText, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( sbuf1 );
		MESSAGE_END();

		strcpy( m_SbarString1, sbuf1 );

		// make sure everything's resent
		bForceResend = TRUE;
	}

	// Check values and send if they don't match
	for (int i = 1; i < SBAR_END; i++)
	{
		if ( newSBarState[i] != m_izSBarState[i] || bForceResend )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgStatusValue, NULL, pev );
				WRITE_BYTE( i );
				WRITE_SHORT( newSBarState[i] );
			MESSAGE_END();

			m_izSBarState[i] = newSBarState[i];
		}
	}
*/
}






#define CLIMB_SHAKE_FREQUENCY	22	// how many frames in between screen shakes when climbing
#define	MAX_CLIMB_SPEED			200	// fastest vertical climbing speed possible
#define	CLIMB_SPEED_DEC			15	// climbing deceleration rate
#define	CLIMB_PUNCH_X			-7  // how far to 'punch' client X axis when climbing
#define CLIMB_PUNCH_Z			7	// how far to 'punch' client Z axis when climbing

void CBasePlayer::PreThink(void)
{
	if ( m_bHoldingBreath )
	{
		if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_rifle" ) )
		{
			if ( !GetClassPtr( (CRifle *)m_pActiveItem->pev )->m_bRaised || m_pActiveItem->m_iSuicideState || m_pActiveItem->m_iSprintState )
			{
				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, pev );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			}
		}

		if ( m_bHoldingBreath && !( pev->button & IN_RUN ) )
		{
			m_bHoldingBreath = false;

			MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		}

		if ( !m_pActiveItem || ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_rifle" ) ) )
		{
			if ( m_bHoldingBreath )
			{
				m_bHoldingBreath = false;

				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, pev );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				EMIT_SOUND_DYN( edict(), CHAN_VOICE, "player/vocal/breathing/release_breath.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			}
		}

		if ( m_fStamina <= 0 )
		{
			if ( m_bHoldingBreath )
			{
				m_bHoldingBreath = false;

				MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgHoldBreath, NULL, pev );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				ViewPunch( -15, 0.5, 0.5 );
			}
		}
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( !IsAlive() && !m_bCoopGameOver && m_flNextDeadNotifyTime < gpGlobals->time )
		{
			UTIL_ProMessage( this, "DEADNOTIFYMESSAGE", 2, false );
			m_flNextDeadNotifyTime = gpGlobals->time + 12;
		}

		if ( ( !IsAlive() && m_flCoopRespawnDelayTime < gpGlobals->time && ( pev->button & IN_JUMP ) ) || ( !IsAlive() && !m_bCoopGameOver && m_flDeadAutoReviveTime != 0 && m_flDeadAutoReviveTime < gpGlobals->time ) )
		{
			pev->button = 0;
			m_iRespawnFrames = 0;

			ClearOutSlot( 1, 1 );
			RemoveAllItems( FALSE );

			SetDualWieldWeapon( 0 );

			m_iDualWeaponLeftAmmo = -2;
			m_iDualWeaponRightAmmo = -2;

			strcpy( m_sQuickSel1, "" );
			strcpy( m_sQuickSel2, "" );
			strcpy( m_sQuickSel3, "" );

			for ( int i = 1; i <= 3; i++ )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( i );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[i] = 0;
			}

			ToFirstPerson();

			respawn( pev, !( m_afPhysicsFlags & PFLAG_OBSERVER ) );
			DontThink();
			m_flDeadAutoReviveTime = 0;
		}

		CBaseEntity *pSolid = NULL;
		bool insideanyplayer = false;
		bool insideamonster = false;

/*
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			CBaseEntity *pList[32];

			for ( int j = 0; j < 32; j++ )
				pList[j] = NULL;

			int count = UTIL_EntitiesInBox( pList, 32, pev->absmin, pev->absmax, FL_MONSTER );

			int touchingplayercount = 0;

			if ( count )
			{
				for ( int i = 0; i < count; i++ )
				{
					if ( pList[i] && pList[i]->IsAlive() )
					{
						CBaseMonster *pMonster = pList[i]->MyMonsterPointer();

						if ( pMonster )
						{
							insideamonster = true;
							break;
						}
					}
				}
			}
		}
*/

		while ( ( pSolid = UTIL_FindEntityByClassname( pSolid, "player" ) ) != NULL )
		{
			if ( pSolid )
			{
				if ( Intersects( pSolid ) && pSolid != this )
					insideanyplayer = true;
			}
		}

		if ( insideanyplayer || insideamonster )
		{
			int pushstrength = 50;

			pev->solid = SOLID_NOT;
		}
		else if ( IsAlive() )
			pev->solid = SOLID_SLIDEBOX;
	}

	if ( m_pActiveItem )
		((CBasePlayerWeapon *)m_pActiveItem)->CalculateWeaponAccuracy();

	if ( m_bIsTripping != m_bMorphineMuffle )
		m_bMorphineMuffle = m_bIsTripping;

	if ( m_bIsTripping && !m_bHasVomited && m_fVomitTime < gpGlobals->time )
		Vomit();

	if ( m_bCoopGameOver && m_flNextCountDown < gpGlobals->time )
	{
		if ( m_iGameOverCounter >= 0 && m_iGameOverCounter <= 5 )
			UTIL_ProMessage( this, UTIL_VarArgs( "Restarting the map in %i seconds\n", m_iGameOverCounter ), 5, false );

		m_flNextCountDown = gpGlobals->time + 1;
		m_iGameOverCounter--;
	}

	if ( m_bIsTripping && (int)CVAR_GET_FLOAT( "room_type" ) != 16 )
		CVAR_SET_FLOAT( "room_type", 16 );

	if ( m_iFlashlightFlags & 64 )
	{
		if ( !m_iHeadShieldOn )
			m_iFlashlightFlags &= ~64;
	}
	else if ( m_iHeadShieldOn == 1 && m_flNextNightVisionTime < gpGlobals->time )
		m_iFlashlightFlags |= 64;

	if ( m_iHeadShieldOn == 1 )
	{
		if ( (int)CVAR_GET_FLOAT( "room_type" ) != 14 )
			CVAR_SET_FLOAT( "room_type", 14 );
	}

	if ( !m_iHeadShieldOn )
	{
		if ( (int)CVAR_GET_FLOAT( "room_type" ) != m_iOldRoomType )
			CVAR_SET_FLOAT( "room_type", m_iOldRoomType );
	}

	if ( pev->health < 25 && !m_bChoking )
	{
		if ( m_pWheelchair )
		{
			if ( m_pWheelchair->pev->iuser2 != 1 )
			{
				if ( !m_bLowHealth )
				{
					m_bLowHealth = true;

					if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
						g_engfuncs.pfnSetPhysicsKeyValue( edict(), "low", "1" );
				}

				if ( m_flNextHeartbeatTime < gpGlobals->time && IsAlive() && !m_gbIsInCutscene && !m_pAttachedCamera && pev->waterlevel < 3 )
				{
					EMIT_SOUND_DYN( edict(), CHAN_AUTO, "player/heartbeat1.wav", 0.25, ATTN_NORM, 0, PITCH_NORM );
					m_flNextHeartbeatTime = gpGlobals->time + 1;
				}

				if ( m_flNextLowHealthSound < gpGlobals->time && IsAlive() && !m_gbIsInCutscene && !m_pAttachedCamera && pev->waterlevel < 3 )
				{
					LowHealthSound();
					m_flNextLowHealthSound = gpGlobals->time + RANDOM_LONG( 10, 30 );
				}
			}
		}
		else
		{
			if ( !m_bLowHealth )
			{
				m_bLowHealth = true;

				if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
					g_engfuncs.pfnSetPhysicsKeyValue( edict(), "low", "1" );
			}

			if ( m_flNextHeartbeatTime < gpGlobals->time && IsAlive() && !m_gbIsInCutscene && !m_pAttachedCamera && pev->waterlevel < 3 )
			{
				EMIT_SOUND_DYN( edict(), CHAN_AUTO, "player/heartbeat1.wav", 0.25, ATTN_NORM, 0, PITCH_NORM );
				m_flNextHeartbeatTime = gpGlobals->time + 1;
			}

			if ( m_flNextLowHealthSound < gpGlobals->time && IsAlive() && !m_gbIsInCutscene && !m_pAttachedCamera && pev->waterlevel < 3 )
			{
				LowHealthSound();
				m_flNextLowHealthSound = gpGlobals->time + RANDOM_LONG( 10, 30 );
			}
		}
	}
	else if ( m_bLowHealth )
	{
		m_bLowHealth = false;

		if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
			g_engfuncs.pfnSetPhysicsKeyValue( edict(), "low", "0" );
	}

	if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_revolver" ) )
	{
		CBasePlayerWeapon *pRevolver = GetPlayerWeapon( 21 );

		if ( pRevolver && m_flCylinderRotateTime < gpGlobals->time )
		{
			if ( pev->vuser4.z != ((CRevolver *)pRevolver)->m_iRotation )
			{
				pev->vuser4.z -= 12;

				if ( pev->vuser4.z < 0 )
					pev->vuser4.z += 360;

				if ( pev->vuser4.z > 360 )
					pev->vuser4.z -= 360;
			}

			m_flCylinderRotateTime = gpGlobals->time + 0.05;
		}
	}

	if ( m_pWheelchair && !( pev->flags & FL_FROZEN ) )
		EnableControl( FALSE );

	if ( m_pWheelchair )
		GetClassPtr( (CWheelchair *)m_pWheelchair->pev )->WheelchairThink();

	int buttonsChanged = (m_afButtonLast ^ pev->button);	// These buttons have changed this frame

	// Debounced button codes for pressed/released
	// UNDONE: Do we need auto-repeat?
	m_afButtonPressed =  buttonsChanged & pev->button;		// The ones that changed and are now down are "pressed"
	m_afButtonReleased = buttonsChanged & (~pev->button);	// The ones that changed and aren't down are "released"

	if ( pPlayerToRevive && pev->fuser3 != 2 )
		pev->fuser3 = 2;

	if ( pPlayerToRevive && m_flSequenceOverrideTime <= gpGlobals->time )
	{
		int animDesired;

		if ( pev->flags & FL_DUCKING )
			animDesired = LookupSequence( "crouch_cpr_idle" );
		else
			animDesired = LookupSequence( "cpr_idle" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	if ( pPlayerToRevive && ( m_afButtonReleased & IN_USE ) )
	{
		int animDesired;

		if ( pPlayerToRevive->IsPlayer() )
		{
			CBaseEntity *pNerdGettingRevived = CBaseEntity::Instance( pPlayerToRevive->edict() );

			if ( pNerdGettingRevived )
				GetClassPtr( (CBasePlayer *)pNerdGettingRevived->pev )->pPlayerReviving = NULL;
		}

		pPlayerToRevive = NULL;
		m_flReviveTime = gpGlobals->time;

		if ( pev->flags & FL_DUCKING )
			animDesired = LookupSequence( "crouch_cpr_up" );
		else
			animDesired = LookupSequence( "cpr_up" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		pev->fuser3 = 4;
		m_flGetupTime = m_flSequenceOverrideTime;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	if ( pPlayerToRevive && ( pPlayerToRevive->IsAlive() || GetClassPtr( (CBasePlayer *)pPlayerToRevive->pev )->pPlayerReviving != this ) )
	{
		int animDesired;

		pPlayerToRevive = NULL;
		m_flReviveTime = gpGlobals->time;

		if ( pev->flags & FL_DUCKING )
			animDesired = LookupSequence( "crouch_cpr_up" );
		else
			animDesired = LookupSequence( "cpr_up" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		pev->fuser3 = 4;
		m_flGetupTime = m_flSequenceOverrideTime;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	if ( pev->fuser3 == 4 && m_flGetupTime < gpGlobals->time && !pPlayerToRevive )
	{
		ToFirstPerson();
		pev->fuser3 = 0;
		m_flGaitSequenceOverrideTime = m_flSequenceOverrideTime = gpGlobals->time;
		SetAnimation( PLAYER_WALK );
	}

	if ( pPlayerToRevive && ( pev->button & IN_USE ) && m_flReviveTime < gpGlobals->time )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pPlayerToRevive->pev );

		if ( pPlayer )
		{
			int animDesired;

			pPlayer->RevivePlayer();
			pPlayer->pPlayerReviving = NULL;
			pPlayerToRevive = NULL;
			m_flReviveTime = gpGlobals->time;

			if ( pev->flags & FL_DUCKING )
				animDesired = LookupSequence( "crouch_cpr_up" );
			else
				animDesired = LookupSequence( "cpr_up" );

			if ( animDesired == -1 )
				animDesired = 0;

			if ( pev->sequence != animDesired || !m_fSequenceLoops )
				pev->frame = 0;

			pev->gaitsequence = 0;
			m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

			pev->fuser3 = 4;
			m_flGetupTime = m_flSequenceOverrideTime;

			if ( pev->sequence != animDesired )
			{
				pev->sequence = animDesired;
				pev->frame = 0;
				ResetSequenceInfo();
			}
		}
	}

	g_pGameRules->PlayerThink( this );

//	CheckDesiredList( ); //LRC
	//CheckAssistList(); //LRC

	if ( g_fGameOver )
		return;         // intermission or finale

	UTIL_MakeVectors(pev->v_angle);             // is this still used?

	ItemPreFrame( );
	WaterMove();

	if ( g_pGameRules && g_pGameRules->FAllowFlashlight() )
		m_iHideHUD &= ~HIDEHUD_FLASHLIGHT;
	else
		m_iHideHUD |= HIDEHUD_FLASHLIGHT;


	// JOHN: checks if new client data (for HUD and view control) needs to be sent to the client
	UpdateClientData();
	CheckTimeBasedDamage();
	CheckSuitUpdate();
	CheckMorphineRestore();

	if (pev->deadflag >= DEAD_DYING)
	{
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			MultiplayerPlayerDeathThink();
		else
			PlayerDeathThink();
		return;
	}

	if ( m_pBoat )
	{
		if ( !( pev->flags & FL_FROZEN ) )
			EnableControl( FALSE );

		return;
	}

	// So the correct flags get sent to client asap.
	//
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
		pev->flags |= FL_ONTRAIN;
	else
		pev->flags &= ~FL_ONTRAIN;

	// Train speed control
	if ( m_afPhysicsFlags & PFLAG_ONTRAIN )
	{
		CBaseEntity *pTrain = CBaseEntity::Instance( pev->groundentity );
		float vel;

		if ( !pTrain )
		{
			TraceResult trainTrace;
			// Maybe this is on the other side of a level transition
			UTIL_TraceLine( pev->origin, pev->origin + Vector(0,0,-38), ignore_monsters, ENT(pev), &trainTrace );

			// HACKHACK - Just look for the func_tracktrain classname
			if ( trainTrace.flFraction != 1.0 && trainTrace.pHit )
			pTrain = CBaseEntity::Instance( trainTrace.pHit );


			if ( !pTrain || !(pTrain->ObjectCaps() & FCAP_DIRECTIONAL_USE) || !pTrain->OnControls(pev) )
			{
				//ALERT( at_error, "In train mode with no train!\n" );
				m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
				m_iTrain = TRAIN_NEW|TRAIN_OFF;
				return;
			}
		}
		else if ( !FBitSet( pev->flags, FL_ONGROUND ) || FBitSet( pTrain->pev->spawnflags, SF_TRACKTRAIN_NOCONTROL ) || (pev->button & (IN_MOVELEFT|IN_MOVERIGHT) ) )
		{
			// Turn off the train if you jump, strafe, or the train controls go dead
			m_afPhysicsFlags &= ~PFLAG_ONTRAIN;
			m_iTrain = TRAIN_NEW|TRAIN_OFF;
			return;
		}

		pev->velocity = g_vecZero;
		vel = 0;
		if ( m_afButtonPressed & IN_FORWARD )
		{
			vel = 1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}
		else if ( m_afButtonPressed & IN_BACK )
		{
			vel = -1;
			pTrain->Use( this, this, USE_SET, (float)vel );
		}

		if (vel)
		{
			m_iTrain = TrainSpeed(pTrain->pev->speed, pTrain->pev->impulse);
			m_iTrain |= TRAIN_ACTIVE|TRAIN_NEW;
		}

	} else if (m_iTrain & TRAIN_ACTIVE)
		m_iTrain = TRAIN_NEW; // turn off train

	if (pev->button & IN_JUMP)
	{
		// If on a ladder, jump off the ladder
		// else Jump
		Jump();
	}


	// If trying to duck, already ducked, or in the process of ducking
	if ((pev->button & IN_DUCK) || FBitSet(pev->flags,FL_DUCKING) || (m_afPhysicsFlags & PFLAG_DUCKING) )
		Duck();

	if ( !FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		m_flFallVelocity = -pev->velocity.z;
	}

	// StudioFrameAdvance( );//!!!HACKHACK!!! Can't be hit by traceline when not animating?

	// Clear out ladder pointer
	m_hEnemy = NULL;

	if ( m_afPhysicsFlags & PFLAG_ONBARNACLE )
	{
		pev->velocity = g_vecZero;
	}
}
/* Time based Damage works as follows:
	1) There are several types of timebased damage:

		#define DMG_PARALYZE		(1 << 14)	// slows affected creature down
		#define DMG_NERVEGAS		(1 << 15)	// nerve toxins, very bad
		#define DMG_POISON			(1 << 16)	// blood poisioning
		#define DMG_RADIATION		(1 << 17)	// radiation exposure
		#define DMG_DROWNRECOVER	(1 << 18)	// drown recovery
		#define DMG_ACID			(1 << 19)	// toxic chemicals or acid burns
		#define DMG_SLOWBURN		(1 << 20)	// in an oven
		#define DMG_SLOWFREEZE		(1 << 21)	// in a subzero freezer

	2) A new hit inflicting tbd restarts the tbd counter - each monster has an 8bit counter,
		per damage type. The counter is decremented every second, so the maximum time
		an effect will last is 255/60 = 4.25 minutes.  Of course, staying within the radius
		of a damaging effect like fire, nervegas, radiation will continually reset the counter to max.

	3) Every second that a tbd counter is running, the player takes damage.  The damage
		is determined by the type of tdb.
			Paralyze		- 1/2 movement rate, 30 second duration.
			Nervegas		- 5 points per second, 16 second duration = 80 points max dose.
			Poison			- 2 points per second, 25 second duration = 50 points max dose.
			Radiation		- 1 point per second, 50 second duration = 50 points max dose.
			Drown			- 5 points per second, 2 second duration.
			Acid/Chemical	- 5 points per second, 10 second duration = 50 points max.
			Burn			- 10 points per second, 2 second duration.
			Freeze			- 3 points per second, 10 second duration = 30 points max.

	4) Certain actions or countermeasures counteract the damaging effects of tbds:

		Armor/Heater/Cooler - Chemical(acid),burn, freeze all do damage to armor power, then to body
							- recharged by suit recharger
		Air In Lungs		- drowning damage is done to air in lungs first, then to body
							- recharged by poking head out of water
							- 10 seconds if swiming fast
		Air In SCUBA		- drowning damage is done to air in tanks first, then to body
							- 2 minutes in tanks. Need new tank once empty.
		Radiation Syringe	- Each syringe full provides protection vs one radiation dosage
		Antitoxin Syringe	- Each syringe full provides protection vs one poisoning (nervegas or poison).
		Health kit			- Immediate stop to acid/chemical, fire or freeze damage.
		Radiation Shower	- Immediate stop to radiation damage, acid/chemical or fire damage.


*/

// If player is taking time based damage, continue doing damage to player -
// this simulates the effect of being poisoned, gassed, dosed with radiation etc -
// anything that continues to do damage even after the initial contact stops.
// Update all time based damage counters, and shut off any that are done.

// The m_bitsDamageType bit MUST be set if any damage is to be taken.
// This routine will detect the initial on value of the m_bitsDamageType
// and init the appropriate counter.  Only processes damage every second.

//#define PARALYZE_DURATION	30		// number of 2 second intervals to take damage
//#define PARALYZE_DAMAGE		0.0		// damage to take each 2 second interval

//#define NERVEGAS_DURATION	16
//#define NERVEGAS_DAMAGE		5.0

//#define POISON_DURATION		25
//#define POISON_DAMAGE		2.0

//#define RADIATION_DURATION	50
//#define RADIATION_DAMAGE	1.0

//#define ACID_DURATION		10
//#define ACID_DAMAGE			5.0

//#define SLOWBURN_DURATION	2
//#define SLOWBURN_DAMAGE		1.0

//#define SLOWFREEZE_DURATION	1.0
//#define SLOWFREEZE_DAMAGE	3.0

/* */


void CBasePlayer::CheckTimeBasedDamage()
{
	int i;
	BYTE bDuration = 0;

	static float gtbdPrev = 0.0;

	if (!(m_bitsDamageType & DMG_TIMEBASED))
		return;

	// only check for time based damage approx. every 2 seconds
	if (abs(gpGlobals->time - m_tbdPrev) < 2.0)
		return;

	m_tbdPrev = gpGlobals->time;

	for (i = 0; i < CDMG_TIMEBASED; i++)
	{
		// make sure bit is set for damage type
		if (m_bitsDamageType & (DMG_PARALYZE << i))
		{
			switch (i)
			{
			case itbd_Paralyze:
				// UNDONE - flag movement as half-speed
				bDuration = PARALYZE_DURATION;
				break;
			case itbd_NerveGas:
//				TakeDamage(pev, pev, NERVEGAS_DAMAGE, DMG_GENERIC);
				bDuration = NERVEGAS_DURATION;
				break;
			case itbd_Poison:
				TakeDamage(pev, pev, POISON_DAMAGE, DMG_GENERIC);
				bDuration = POISON_DURATION;
				break;
			case itbd_Radiation:
//				TakeDamage(pev, pev, RADIATION_DAMAGE, DMG_GENERIC);
				bDuration = RADIATION_DURATION;
				break;
			case itbd_DrownRecover:
				// NOTE: this hack is actually used to RESTORE health
				// after the player has been drowning and finally takes a breath
				if (m_idrowndmg > m_idrownrestored)
				{
					int idif = min(m_idrowndmg - m_idrownrestored, 10);

					TakeHealth(idif, DMG_GENERIC);
					m_idrownrestored += idif;
				}
				bDuration = 4;	// get up to 5*10 = 50 points back
				break;
			case itbd_Acid:
//				TakeDamage(pev, pev, ACID_DAMAGE, DMG_GENERIC);
				bDuration = ACID_DURATION;
				break;
			case itbd_SlowBurn:
//				TakeDamage(pev, pev, SLOWBURN_DAMAGE, DMG_GENERIC);
				bDuration = SLOWBURN_DURATION;
				break;
			case itbd_SlowFreeze:
//				TakeDamage(pev, pev, SLOWFREEZE_DAMAGE, DMG_GENERIC);
				bDuration = SLOWFREEZE_DURATION;
				break;
			default:
				bDuration = 0;
			}

			if (m_rgbTimeBasedDamage[i])
			{
				// use up an antitoxin on poison or nervegas after a few seconds of damage
				if (((i == itbd_NerveGas) && (m_rgbTimeBasedDamage[i] < NERVEGAS_DURATION)) ||
					((i == itbd_Poison)   && (m_rgbTimeBasedDamage[i] < POISON_DURATION)))
				{
					if (m_rgItems[ITEM_ANTIDOTE])
					{
						m_rgbTimeBasedDamage[i] = 0;
						m_rgItems[ITEM_ANTIDOTE]--;
					//	SetSuitUpdate("!HEV_HEAL4", FALSE, SUIT_REPEAT_OK); // buz: no hev sounds
					}
				}


				// decrement damage duration, detect when done.
				if (!m_rgbTimeBasedDamage[i] || --m_rgbTimeBasedDamage[i] == 0)
				{
					m_rgbTimeBasedDamage[i] = 0;
					// if we're done, clear damage bits
					m_bitsDamageType &= ~(DMG_PARALYZE << i);
				}
			}
			else
				// first time taking this damage type - init damage duration
				m_rgbTimeBasedDamage[i] = bDuration;
		}
	}
}

/*
THE POWER SUIT

The Suit provides 3 main functions: Protection, Notification and Augmentation.
Some functions are automatic, some require power.
The player gets the suit shortly after getting off the train in C1A0 and it stays
with him for the entire game.

Protection

	Heat/Cold
		When the player enters a hot/cold area, the heating/cooling indicator on the suit
		will come on and the battery will drain while the player stays in the area.
		After the battery is dead, the player starts to take damage.
		This feature is built into the suit and is automatically engaged.
	Radiation Syringe
		This will cause the player to be immune from the effects of radiation for N seconds. Single use item.
	Anti-Toxin Syringe
		This will cure the player from being poisoned. Single use item.
	Health
		Small (1st aid kits, food, etc.)
		Large (boxes on walls)
	Armor
		The armor works using energy to create a protective field that deflects a
		percentage of damage projectile and explosive attacks. After the armor has been deployed,
		it will attempt to recharge itself to full capacity with the energy reserves from the battery.
		It takes the armor N seconds to fully charge.

Notification (via the HUD)

x	Health
x	Ammo
x	Automatic Health Care
		Notifies the player when automatic healing has been engaged.
x	Geiger counter
		Classic Geiger counter sound and status bar at top of HUD
		alerts player to dangerous levels of radiation. This is not visible when radiation levels are normal.
x	Poison
	Armor
		Displays the current level of armor.

Augmentation

	Reanimation (w/adrenaline)
		Causes the player to come back to life after he has been dead for 3 seconds.
		Will not work if player was gibbed. Single use.
	Long Jump
		Used by hitting the ??? key(s). Caused the player to further than normal.
	SCUBA
		Used automatically after picked up and after player enters the water.
		Works for N seconds. Single use.

Things powered by the battery

	Armor
		Uses N watts for every M units of damage.
	Heat/Cool
		Uses N watts for every second in hot/cold area.
	Long Jump
		Uses N watts for every jump.
	Alien Cloak
		Uses N watts for each use. Each use lasts M seconds.
	Alien Shield
		Augments armor. Reduces Armor drain by one half

*/

// if in range of radiation source, ping geiger counter

#define GEIGERDELAY 0.25

void CBasePlayer :: UpdateGeigerCounter( void )
{
	BYTE range;

	// delay per update ie: don't flood net with these msgs
	if (gpGlobals->time < m_flgeigerDelay)
		return;

	m_flgeigerDelay = gpGlobals->time + GEIGERDELAY;

	// send range to radition source to client

	range = (BYTE) (m_flgeigerRange / 4);

	if (range != m_igeigerRangePrev)
	{
		m_igeigerRangePrev = range;

		MESSAGE_BEGIN( MSG_ONE, gmsgGeigerRange, NULL, pev );
			WRITE_BYTE( range );
		MESSAGE_END();
	}

	// reset counter and semaphore
	if (!RANDOM_LONG(0,3))
		m_flgeigerRange = 1000;

}

/*
================
CheckSuitUpdate

Play suit update if it's time
================
*/

#define SUITUPDATETIME	3.5
#define SUITFIRSTUPDATETIME 0.1

void CBasePlayer::CheckSuitUpdate()
{
	int i;
	int isentence = 0;
	int isearch = m_iSuitPlayNext;

	// Ignore suit updates if no suit
	if ( !(pev->weapons & (1<<WEAPON_SUIT)) )
		return;

	// if in range of radiation source, ping geiger counter
	UpdateGeigerCounter();

//	if ( g_pGameRules->IsMultiplayer() )
//	{
		// don't bother updating HEV voice in multiplayer.
		return;
//	} // buz

	if ( gpGlobals->time >= m_flSuitUpdate && m_flSuitUpdate > 0)
	{
		// play a sentence off of the end of the queue
		for (i = 0; i < CSUITPLAYLIST; i++)
			{
			if (isentence = m_rgSuitPlayList[isearch])
				break;

			if (++isearch == CSUITPLAYLIST)
				isearch = 0;
			}

		if (isentence)
		{
			m_rgSuitPlayList[isearch] = 0;
			if (isentence > 0)
			{
				// play sentence number

				char sentence[CBSENTENCENAME_MAX+1];
				strcpy(sentence, "!");
				strcat(sentence, gszallsentencenames[isentence]);
				EMIT_SOUND_SUIT(ENT(pev), sentence);
			}
			else
			{
				// play sentence group
				EMIT_GROUPID_SUIT(ENT(pev), -isentence);
			}
		m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
		}
		else
			// queue is empty, don't check
			m_flSuitUpdate = 0;
	}
}

// add sentence to suit playlist queue. if fgroup is true, then
// name is a sentence group (HEV_AA), otherwise name is a specific
// sentence name ie: !HEV_AA0.  If iNoRepeat is specified in
// seconds, then we won't repeat playback of this word or sentence
// for at least that number of seconds.

void CBasePlayer::SetSuitUpdate(char *name, int fgroup, int iNoRepeatTime)
{
	int i;
	int isentence;
	int iempty = -1;


	// Ignore suit updates if no suit
	if ( !(pev->weapons & (1<<WEAPON_SUIT)) )
		return;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		// due to static channel design, etc. We don't play HEV sounds in multiplayer right now.
		return;
	}

	// if name == NULL, then clear out the queue

	if (!name)
	{
		for (i = 0; i < CSUITPLAYLIST; i++)
			m_rgSuitPlayList[i] = 0;
		return;
	}
	// get sentence or group number
	if (!fgroup)
	{
		isentence = SENTENCEG_Lookup(name, NULL);
		if (isentence < 0)
		{
			ALERT(at_debug,"HEV couldn't find sentence %s\n",name);
			return;
		}
	}
	else
		// mark group number as negative
		isentence = -SENTENCEG_GetIndex(name);

	// check norepeat list - this list lets us cancel
	// the playback of words or sentences that have already
	// been played within a certain time.

	for (i = 0; i < CSUITNOREPEAT; i++)
	{
		if (isentence == m_rgiSuitNoRepeat[i])
			{
			// this sentence or group is already in
			// the norepeat list

			if (m_rgflSuitNoRepeatTime[i] < gpGlobals->time)
				{
				// norepeat time has expired, clear it out
				m_rgiSuitNoRepeat[i] = 0;
				m_rgflSuitNoRepeatTime[i] = 0.0;
				iempty = i;
				break;
				}
			else
				{
				// don't play, still marked as norepeat
				return;
				}
			}
		// keep track of empty slot
		if (!m_rgiSuitNoRepeat[i])
			iempty = i;
	}

	// sentence is not in norepeat list, save if norepeat time was given

	if (iNoRepeatTime)
	{
		if (iempty < 0)
			iempty = RANDOM_LONG(0, CSUITNOREPEAT-1); // pick random slot to take over
		m_rgiSuitNoRepeat[iempty] = isentence;
		m_rgflSuitNoRepeatTime[iempty] = iNoRepeatTime + gpGlobals->time;
	}

	// find empty spot in queue, or overwrite last spot

	m_rgSuitPlayList[m_iSuitPlayNext++] = isentence;
	if (m_iSuitPlayNext == CSUITPLAYLIST)
		m_iSuitPlayNext = 0;

	if (m_flSuitUpdate <= gpGlobals->time)
	{
		if (m_flSuitUpdate == 0)
			// play queue is empty, don't delay too long before playback
			m_flSuitUpdate = gpGlobals->time + SUITFIRSTUPDATETIME;
		else
			m_flSuitUpdate = gpGlobals->time + SUITUPDATETIME;
	}

}

/*
================
CheckPowerups

Check for turning off powerups

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
================
*/
	static void
CheckPowerups(entvars_t *pev)
{
	if (pev->health <= 0)
		return;

	pev->modelindex = g_ulModelIndexPlayer;    // don't use eyes
}


//=========================================================
// UpdatePlayerSound - updates the position of the player's
// reserved sound slot in the sound list.
//=========================================================
void CBasePlayer :: UpdatePlayerSound ( void )
{
	int iBodyVolume;
	int iVolume;
	CSound *pSound;

	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt :: ClientSoundIndex( edict() ) );

	if ( !pSound )
	{
		ALERT ( at_debug, "Client lost reserved sound!\n" );
		return;
	}

	pSound->m_iType = bits_SOUND_NONE;

	// now calculate the best target volume for the sound. If the player's weapon
	// is louder than his body/movement, use the weapon volume, else, use the body volume.

	if ( FBitSet ( pev->flags, FL_ONGROUND ) )
	{
		iBodyVolume = pev->velocity.Length();

		// clamp the noise that can be made by the body, in case a push trigger,
		// weapon recoil, or anything shoves the player abnormally fast.
		if ( iBodyVolume > 512 )
		{
			iBodyVolume = 512;
		}
	}
	else
	{
		iBodyVolume = 0;
	}

	if ( pev->button & IN_JUMP )
	{
		iBodyVolume += 100;
	}

	m_iWeaponVolume = 1000;

// convert player move speed and actions into sound audible by monsters.
	if ( m_iWeaponVolume > iBodyVolume )
	{
		m_iTargetVolume = m_iWeaponVolume;

		// OR in the bits for COMBAT sound if the weapon is being louder than the player.
		pSound->m_iType |= bits_SOUND_COMBAT;
	}
	else
	{
		m_iTargetVolume = iBodyVolume;
	}

	// decay weapon volume over time so bits_SOUND_COMBAT stays set for a while
	m_iWeaponVolume -= 250 * gpGlobals->frametime;
	if ( m_iWeaponVolume < 0 )
	{
		iVolume = 0;
	}


	// if target volume is greater than the player sound's current volume, we paste the new volume in
	// immediately. If target is less than the current volume, current volume is not set immediately to the
	// lower volume, rather works itself towards target volume over time. This gives monsters a much better chance
	// to hear a sound, especially if they don't listen every frame.
	iVolume = pSound->m_iVolume;

	if ( m_iTargetVolume > iVolume )
	{
		iVolume = m_iTargetVolume;
	}
	else if ( iVolume > m_iTargetVolume )
	{
		iVolume -= 250 * gpGlobals->frametime;

		if ( iVolume < m_iTargetVolume )
		{
			iVolume = 0;
		}
	}

	if ( m_fNoPlayerSound )
	{
		// debugging flag, lets players move around and shoot without monsters hearing.
		iVolume = 0;
	}

	if ( gpGlobals->time > m_flStopExtraSoundTime )
	{
		// since the extra sound that a weapon emits only lasts for one client frame, we keep that sound around for a server frame or two
		// after actual emission to make sure it gets heard.
		m_iExtraSoundTypes = 0;
	}

	if ( pSound )
	{
		pSound->m_vecOrigin = pev->origin;
		pSound->m_iType |= ( bits_SOUND_PLAYER | m_iExtraSoundTypes );
		pSound->m_iVolume = iVolume;
	}

	// keep track of virtual muzzle flash
	m_iWeaponFlash -= 256 * gpGlobals->frametime;
	if (m_iWeaponFlash < 0)
		m_iWeaponFlash = 0;

	//UTIL_MakeVectors ( pev->angles );
	//gpGlobals->v_forward.z = 0;

	// Below are a couple of useful little bits that make it easier to determine just how much noise the
	// player is making.
	// UTIL_ParticleEffect ( pev->origin + gpGlobals->v_forward * iVolume, g_vecZero, 255, 25 );
	//ALERT ( at_console, "%d/%d\n", iVolume, m_iTargetVolume );
}

void CBasePlayer::VibrateCheck( void )
{

}

void CBasePlayer::HeartBeatCheck( void )
{
	CBaseEntity *pEntity = NULL;
	bool nearstranger = false;
	float distance = 0;
	bool takedamage = false;

	if ( !IsAlive() )
		return;

	if ( m_bNearStranger && !b_GoingThroughADoor )
	{
		while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 250 ) ) != NULL )
		{
			if ( pEntity->IsAlive() && FClassnameIs( pEntity->edict(), "monster_stranger" ) )
			{
				CBaseMonster *pMonster = pEntity->MyMonsterPointer();

				if ( pMonster && pMonster->m_hEnemy == this && !pMonster->HasConditions( bits_COND_ENEMY_OCCLUDED ) && !pMonster->m_bFrozenByCamera )
				{
					nearstranger = true;
					distance = ( pMonster->pev->origin - pev->origin ).Length();

					if ( distance > 200 && distance <= 300 )
					{
						if ( m_fNextHeartBeatTime < gpGlobals->time && !heartbeat && !takedamage )
						{
							heartbeat = true;
							m_fNextHeartBeatTime = gpGlobals->time + 1;
							takedamage = true;
						}

						UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 1, 64, 0 );
					}

					if ( distance > 100 && distance <= 200 )
					{
						if ( m_fNextHeartBeatTime < gpGlobals->time && !heartbeat && !takedamage )
						{
							heartbeat = true;
							takedamage = true;
							m_fNextHeartBeatTime = gpGlobals->time + 0.75;
						}

						UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 1, 96, 0 );
					}

					if ( distance > 75 && distance <= 100 )
					{
						if ( m_fNextHeartBeatTime < gpGlobals->time && !heartbeat && !takedamage )
						{
							m_fNextHeartBeatTime = gpGlobals->time + 0.5;
							heartbeat = true;
							takedamage = true;
						}

						UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 1, 128, 0 );
					}

					if ( distance > 0 && distance <= 75 )
					{
						if ( m_fNextHeartBeatTime < gpGlobals->time && !heartbeat && !takedamage )
						{
							m_fNextHeartBeatTime = gpGlobals->time + 0.25;
							heartbeat = true;
							takedamage = true;
						}

						UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 1, 192, 0 );
					}
				}
			}
		}

		if ( !nearstranger )
			m_bNearStranger = false;

		if ( m_fNextHeartBeatTime < gpGlobals->time && heartbeat )
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "stranger/st_hearbeat.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			heartbeat = false;
		}


		if ( takedamage )
		{
			TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), gSkillData.strangerDmg, DMG_GENERIC );
			takedamage = false;
		}
	}
}

bool CBasePlayer::EHandlesNotSame( EHANDLE edict1, EHANDLE edict2 )
{
	if ( edict1 && edict2 )
	{
		if ( edict1->edict() == edict2->edict() )
			return false;

		if ( edict1->edict() != edict2->edict() )
			return true;
	}

	if ( !edict1 && !edict2 )
		return false;

	if ( ( edict1 && !edict2 ) || ( !edict1 && edict2 ) )
		return true;

	return true;
}

void ShiftMonsters( Vector origin )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, origin, 1024 ) ) != NULL )
	{
		if ( pEntity->pev->classname && !FClassnameIs( pEntity->edict(), "monster_spitter" ) && strstr( STRING( pEntity->pev->classname ), "monster_" ) && !FClassnameIs( pEntity->edict(), "monster_generic" ) )
		{
			if ( pEntity->IsAlive() )
			{
				CBaseMonster *pMonster = pEntity->MyMonsterPointer();

				if ( pMonster && pMonster->m_vOriginalOrigin != g_vecZero )
					UTIL_SetOrigin( pMonster, pMonster->m_vOriginalOrigin );
			}
		}
	}
}

void CBasePlayer::PostThink()
{
	if ( m_bInPhoneCall )
	{
		if ( m_iPhoneNumberState_guess )
		switch ( m_iPhoneNumberState_guess )
		{
		case 1:
			if ( gpGlobals->time > m_flPhoneNumberTime_guess )
			{
				m_iPhoneNumberState_guess = 2;
				EMIT_SOUND( ENT(pev), CHAN_AUTO, "ambience/van_number.wav", 1, ATTN_NORM );
				m_flPhoneNumberTime_guess = gpGlobals->time + 6.5;
			}
			break;

		case 2:
			if ( gpGlobals->time > m_flPhoneNumberTime_guess )
			{
				m_iPhoneNumberState_guess = 0;

				if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_mobile" ) )
				{
					CMobile *pMobile = GetClassPtr( (CMobile *)m_pActiveItem->pev );

					if ( pMobile )
					{
						if ( m_iMobileFlashMode )
						{
							pMobile->m_flTimeWeaponIdle = gpGlobals->time + 1.1;
							pMobile->SendWeaponAnim( 16 );
							m_flNextAttack = gpGlobals->time + 1.1;
							m_bInPhoneCall = false;
						}
						else
						{
							pMobile->m_flTimeWeaponIdle = gpGlobals->time + 1.1;
							pMobile->SendWeaponAnim( 15 );
							m_flNextAttack = gpGlobals->time + 1.1;
							m_bInPhoneCall = false;
						}
						EMIT_SOUND( ENT(pev), CHAN_AUTO, "phone/tone_send.wav", 1, ATTN_NORM );
						m_iHideHUD &= ~HIDEHUD_HEALTH;
						m_DrawBars = 0;
						pev->fuser3 = 0;
					}
				}
			}
			break;
		}
	}

	if ( m_pActiveItem && m_pActiveItem->m_iId == 10 )
	{
		if ( pev->rendercolor.x != m_pActiveItem->pev->rendercolor.x )
			pev->rendercolor.x = m_pActiveItem->pev->rendercolor.x;

		if ( pev->rendercolor.y != m_pActiveItem->pev->rendercolor.y )
			pev->rendercolor.y = m_pActiveItem->pev->rendercolor.y;

		if ( pev->rendercolor.z != m_pActiveItem->pev->rendercolor.z )
			pev->rendercolor.z = m_pActiveItem->pev->rendercolor.z;

		if ( pev->renderamt != m_pActiveItem->pev->renderamt )
			pev->renderamt = m_pActiveItem->pev->renderamt;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && gpGlobals->time > m_flNextMPPlayerNameCheck )
	{
		m_flNextMPPlayerNameCheck = gpGlobals->time + 0.15;

		TraceResult tr;

		UTIL_MakeVectors( pev->v_angle );
		Vector vecSrc = GetGunPosition();
		Vector vecEnd = vecSrc + gpGlobals->v_forward * 384;

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

		if ( tr.pHit )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

			if ( pEntity && pEntity->IsPlayer() && pEntity != this )
			{
				if ( m_iLookingAtPlayer != ENTINDEX( pEntity->edict() ) )
				{
					m_iLookingAtPlayer = ENTINDEX( pEntity->edict() );

					MESSAGE_BEGIN( MSG_ONE, gmsgLookingAt, NULL, pev );
						WRITE_BYTE( m_iLookingAtPlayer );
					MESSAGE_END();

					m_flPlayerNameDieTime = gpGlobals->time + 0.15;
				}
			}
			else if ( m_iLookingAtPlayer )
			{
				m_iLookingAtPlayer = 0;

				MESSAGE_BEGIN( MSG_ONE, gmsgLookingAt, NULL, pev );
					WRITE_BYTE( m_iLookingAtPlayer );
				MESSAGE_END();
			}
		}
		else if ( m_iLookingAtPlayer )
		{
			m_iLookingAtPlayer = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgLookingAt, NULL, pev );
				WRITE_BYTE( m_iLookingAtPlayer );
			MESSAGE_END();
		}
	}

	if ( pev->movetype == MOVETYPE_NOCLIP && !CoFR_OfflineCheats() )
		pev->movetype = MOVETYPE_WALK;

	if ( pev->flags & FL_GODMODE )
		pev->flags &= ~FL_GODMODE;

	if ( m_bJustJumped )
	{
		if ( ( pev->flags & FL_ONGROUND ) || gpGlobals->time > m_flMaxJustJumpedTime )
			m_bJustJumped = false;
	}

	if ( gpGlobals->time > m_flNextTimeTakenCheck && !m_gbIsInCutscene )
	{
		playerstats.time_seconds++;

		if ( playerstats.time_seconds > 59 )
		{
			playerstats.time_seconds = 0;
			playerstats.time_minutes++;
		}

		if ( playerstats.time_minutes > 59 )
		{
			playerstats.time_minutes = 0;
			playerstats.time_hours++;
		}

		m_flNextTimeTakenCheck = gpGlobals->time + 1;
	}

	if ( g_fGameOver )
		goto pt_end;         // intermission or finale

	if ( !IsAlive() )
		goto pt_end;

	if ( pPlayerReviving && !pPlayerReviving->IsAlive() )
		pPlayerReviving = NULL;

	if ( m_bCoopWaitingForCutsceneToEnd )
	{
		UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0, 1, 255, FFADE_OUT | FFADE_STAYOUT );
		EnableControl( FALSE );

		if ( !( m_iHideHUD & HIDEHUD_HEALTH ) )
			m_iHideHUD |= HIDEHUD_HEALTH;

		if ( !( m_iHideHUD & HIDEHUD_WEAPONS ) )
			m_iHideHUD |= HIDEHUD_WEAPONS;

		if ( !( pev->flags & FL_NOTARGET ) )
			pev->flags |= FL_NOTARGET;

		if ( gpGlobals->time > m_flNextCoopWaitingForCutsceneFadeTime )
		{
			m_flNextCoopWaitingForCutsceneFadeTime = gpGlobals->time + 5;
			UTIL_ProMessage( this, "Please wait until cutscene has finished.", 2, false );
		}
	}

	if ( gpGlobals->time > m_flNextDistanceCheckTime )
	{
		float distance = ( m_vPreviousOrigin - pev->origin ).Length2D();
		m_vPreviousOrigin = pev->origin;

		playerstats.distancetravelled_metres += distance / 39.3700787;

		if ( playerstats.distancetravelled_metres > 999 )
		{
			playerstats.distancetravelled_metres = 0;
			playerstats.distancetravelled_kilometres += 1;
		}

		m_flNextDistanceCheckTime = gpGlobals->time + 0.1;
	}

	if ( m_bRestored )
	{
		ShiftMonsters( pev->origin );
		m_OnStairs = false;
		m_bRestored = false;

		//if ( m_iPlayerSaveLock != 1337 )
		//	CLIENT_COMMAND( edict(), "disconnect\n" );

		m_iPlayerSaveLock = 0;

		m_iWeaponVolume = 1000;

		if ( playerstats.started_from_the_beginning )
		{
			int difficulty = CVAR_GET_FLOAT( "difficulty" );
			int language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

			if ( language != playerstats.language )
				CVAR_SET_FLOAT( "cof_subtitlelanguage", playerstats.language );

			if ( difficulty != playerstats.chosen_difficulty )
				CVAR_SET_FLOAT( "difficulty", playerstats.chosen_difficulty );

			if ( g_pGameRules )
				g_pGameRules->RefreshSkillData();
		}

		m_iClientUnlockedClothes = 0;

		if ( m_iQuickSlotSlot[1] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( m_iQuickSlotSlot[1] );
			MESSAGE_END();
		}

		if ( m_iQuickSlotSlot[2] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 2 );
				WRITE_BYTE( m_iQuickSlotSlot[2] );
			MESSAGE_END();
		}

		if ( m_iQuickSlotSlot[3] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 3 );
				WRITE_BYTE( m_iQuickSlotSlot[3] );
			MESSAGE_END();
		}

		if ( gmsgConcuss && m_bIsTripping )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
				WRITE_BYTE( 10 );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}

		if ( m_pActiveItem && m_pActiveItem->m_pPlayer )
		{
			((CBasePlayerWeapon *)m_pActiveItem)->m_flTimeWeaponIdle = gpGlobals->time;
			((CBasePlayerWeapon *)m_pActiveItem)->WeaponIdle();

			gDualWeapon.m_flNextPrimaryAttack = gpGlobals->time;
			gDualWeapon.m_flNextSecondaryAttack = gpGlobals->time;
			gDualWeapon.m_flNextIdleTime = gpGlobals->time;
			gDualWeapon.WeaponThink( this );
		}

		if ( pev->vuser1.x )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 10 );
				WRITE_STRING( "" );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			m_iHeadShieldOn = 1;

			UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 0.1, 255, 0 );

			EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, "unlockables/nv_on.wav", 1, ATTN_NORM, 0, PITCH_NORM );

			ViewPunch( -24, 0.5, 0.5 );
		}
	}

	if ( clientmaterials )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgMaterials, NULL, pev );
		MESSAGE_END();

		clientmaterials = false;
	}

	if ( m_iMp3NeedsUpdate )
	{
		if ( FStringNull( m_strCurrentMp3 ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
				WRITE_COORD( m_fMp3FadeTime );
				WRITE_STRING( "nothing" );
			MESSAGE_END();

			m_fMp3FadeTime = 0;
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pev );
				if ( m_bMp3ShouldLoop )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );

				WRITE_BYTE( 1 );
				WRITE_COORD( m_fMp3FadeInTime );
				WRITE_STRING( STRING( m_strCurrentMp3 ) );
			MESSAGE_END();
		}

		m_iMp3NeedsUpdate = 0;
	}

	if ( m_iUsingMic )
		m_iWeaponVolume = 1000;

	if ( m_iHeadShieldOn == 101 )
	{
		if ( m_pActiveItem )
		{
			if ( gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack && gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextSecondaryAttack && gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextTertiaryAttack && !m_pActiveItem->m_iSuicideState && !m_pActiveItem->m_iSprintState && !((CBasePlayerWeapon *)m_pActiveItem)->m_fInReload && gpGlobals->time > m_flNextAttack )
			{
				m_pStoredNightVisionWeapon = (CBasePlayerWeapon *)m_pActiveItem;
				m_pActiveItem->Holster();
				m_flNextNightVisionTime = GetSequenceTime( GET_MODEL_PTR( ENT( m_pActiveItem->pev ) ), pev->weaponanim ) + gpGlobals->time;
				m_pActiveItem = NULL;
			}
		}
		else if ( gpGlobals->time > m_flNextNightVisionTime )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( 10 );
				WRITE_STRING( "" );
				if ( pev->vuser1.x )
					WRITE_BYTE( 0 );
				else
					WRITE_BYTE( 1 );
			MESSAGE_END();

			m_flNextNightVisionTime = gpGlobals->time + 0.7;

			m_iHeadShieldOn = 1;

			UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 0.1, 255, FFADE_OUT );

			EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, "unlockables/nv_on.wav", 1, ATTN_NORM, 0, PITCH_NORM );

			ViewPunch( -24, 0.5, 0.5 );
		}
	}

	if ( m_iHeadShieldOn == 100 )
	{
		if ( m_pActiveItem )
		{
			if ( gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack && gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextSecondaryAttack && gpGlobals->time > ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextTertiaryAttack && !m_pActiveItem->m_iSuicideState && !m_pActiveItem->m_iSprintState && !((CBasePlayerWeapon *)m_pActiveItem)->m_fInReload && gpGlobals->time > m_flNextAttack )
			{
				m_pStoredNightVisionWeapon = (CBasePlayerWeapon *)m_pActiveItem;
				m_pActiveItem->Holster();
				m_flNextNightVisionTime = GetSequenceTime( GET_MODEL_PTR( ENT( m_pActiveItem->pev ) ), pev->weaponanim ) + gpGlobals->time;
				m_pActiveItem = NULL;
			}
		}
		else if ( gpGlobals->time > m_flNextNightVisionTime )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 10 );
				WRITE_STRING( "" );
				if ( pev->vuser1.x )
					WRITE_BYTE( 0 );
				else
					WRITE_BYTE( 1 );
			MESSAGE_END();

			m_flNextNightVisionTime = gpGlobals->time + 0.7;

			m_iHeadShieldOn = 0;

			UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 0.5, 0.1, 255, 0 );

			EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, "unlockables/nv_off.wav", 1, ATTN_NORM, 0, PITCH_NORM );

			ViewPunch( -24, -0.5, -0.5 );
		}
	}

	if ( m_pStoredNightVisionWeapon && m_flNextNightVisionTime != 666 && gpGlobals->time > m_flNextNightVisionTime )
	{
		m_pStoredNightVisionWeapon->Deploy();
		m_pStoredNightVisionWeapon->UpdateItemInfo();
		m_pActiveItem = m_pStoredNightVisionWeapon;
		m_pStoredNightVisionWeapon = NULL;
	}

	if ( b_GoingThroughADoor && g_pGameRules && g_pGameRules->IsMultiplayer() && IsAlive() )
	{
		if ( !( pev->effects & EF_NODRAW ) )
			pev->effects |= EF_NODRAW;
	}

	if ( pev->vuser4.x != pev->fuser3 )
		pev->vuser4.x = pev->fuser3;

	if ( pDeathScene && pDeathScene->pev->euser1 )
		pDeathScene->pev->origin = pDeathScene->pev->euser1->v.origin;

	if ( m_bWaitingToFall && pev->fuser3 != 2 )
	{
		int animDesired = LookupSequence( "taller_fall" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	if ( pev->fuser3 == 2 && !m_bHasGotUp && gpGlobals->time > m_flSequenceOverrideTime )
	{
		int animDesired = LookupSequence( "taller_lie" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}
	}

	if ( m_bWaitingToFall && ( pev->flags & FL_ONGROUND ) && pev->fuser3 == 0 )
	{
		EMIT_SOUND_DYN( ENT(pev), CHAN_AUTO, "taller/taller_player_impact.wav", 1, ATTN_NORM, 0, 90 );

		int animDesired = LookupSequence( "taller_land" );

		if ( animDesired == -1 )
			animDesired = 0;

		if ( pev->sequence != animDesired || !m_fSequenceLoops )
			pev->frame = 0;

		pev->gaitsequence = 0;
		m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

		if ( pev->sequence != animDesired )
		{
			pev->sequence = animDesired;
			pev->frame = 0;
			ResetSequenceInfo();
		}

		pev->velocity.x /= 2;
		pev->velocity.y /= 2;
		pev->velocity.z /= 2;

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			m_fFallingTime = gpGlobals->time + 3;
		else
			m_fFallingTime = gpGlobals->time + 5;

		m_bShouldFallFreeze = true;

		pev->fuser3 = 2;

		MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_BYTE( RANDOM_LONG( 6, 12 ) );
		MESSAGE_END();

		ShowHint( 100, false );

		ViewPunch( -30, RANDOM_LONG( -35, 35 ), RANDOM_LONG( -35, 35 ) );
		Pain();

		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !m_OnStairs )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !m_OnStairs )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}

		if ( pev->maxspeed < 30 )
			pev->maxspeed = 30;
	}

	if ( m_flTurnVectorTime > gpGlobals->time )
	{
		m_fOldTime = m_flTime;
		m_flTime = gpGlobals->time;
		m_flTimeDelta = (double)m_flTime - m_fOldTime;

		Vector vecGoal = UTIL_VecToAngles( m_vTurnVector - ( pev->origin + pev->view_ofs ) );
		vecGoal.x = -vecGoal.x;

		if ( pev->angles.y > 360 )
			pev->angles.y -= 360;

		if ( pev->angles.y < 0 )
			pev->angles.y += 360;

		float dx = vecGoal.x - pev->angles.x;
		float dy = vecGoal.y - pev->angles.y;

		if ( dx < -180 )
			dx += 360;

		if ( dx > 180 )
			dx -= 360;

		if ( dy < -180 )
			dy += 360;

		if ( dy > 180 )
			dy -= 360;

		pev->angles.x = vecGoal.x;
		pev->angles.y = vecGoal.y;

		pev->fixangle = TRUE;
	}

	m_flTime = gpGlobals->time;

	if ( !m_bWaterVoiceCancel )
	{
		if ( pev->waterlevel >= 3 )
		{
			m_bWaterVoiceCancel = true;
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "common/null.wav", 1, ATTN_NORM );
		}
	}
	else
	{
		if ( pev->waterlevel < 3 )
			m_bWaterVoiceCancel = false;
	}

	if ( pev->waterlevel >= 1 && pev->waterlevel < 3 && !( pev->flags & FL_ONGROUND ) )
	{
		if ( gpGlobals->time > m_flRippleTime )
		{
			TraceResult splashtr;
			Vector SplashStart, SplashEnd;

			SplashStart = pev->origin + pev->view_ofs;
			SplashEnd = SplashStart - Vector( 0, 0, 1024 );

			UTIL_TraceLine( SplashStart, SplashEnd, ignore_monsters, dont_ignore_glass, ENT(pev), &splashtr );

			if ( splashtr.flFraction != 1.0 )
			{
				if ( UTIL_PointContents( splashtr.vecEndPos ) == CONTENTS_WATER )
				{
					Vector SplashOrigin = splashtr.vecEndPos;

					while ( UTIL_PointContents( SplashOrigin ) == CONTENTS_WATER )
						SplashOrigin.z += 1;

					float speed = pev->velocity.Length2D();

					if ( speed > 20 )
					{
						MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
							WRITE_COORD( SplashOrigin.x );
							WRITE_COORD( SplashOrigin.y );
							WRITE_COORD( SplashOrigin.z );
							WRITE_SHORT( 30 );
						MESSAGE_END();
					}
					else
					{
						MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
							WRITE_COORD( SplashOrigin.x );
							WRITE_COORD( SplashOrigin.y );
							WRITE_COORD( SplashOrigin.z );
							WRITE_SHORT( 20 );
						MESSAGE_END();
					}
				}
			}

			m_flRippleTime = gpGlobals->time + 0.25;
		}
	}
	else
	{
		if ( m_flRippleTime > gpGlobals->time )
			m_flRippleTime = gpGlobals->time;
	}

	if ( m_bWasInShallowWater )
	{
		if ( pev->waterlevel < 1 )
			m_bWasInShallowWater = false;
	}
	else
	{
		if ( pev->waterlevel > 0 )
			m_bWasInShallowWater = true;
	}

	if ( m_bWasDrowning )
	{
		if ( pev->waterlevel < 3 )
		{
			m_bWasDrowning = false;
			EMIT_SOUND( ENT(pev), CHAN_VOICE, "player/vocal/water_surface.wav", 1, ATTN_NORM );
		}
	}

	if ( m_bStartedDrowning )
	{
		if ( pev->waterlevel < 3 )
			m_bStartedDrowning = false;
	}

	if ( !m_bSubmerged )
	{
		if ( pev->waterlevel >= 3 )
		{
			if ( !( pev->flags & FL_ONGROUND ) )
			{
				if ( m_bWasInShallowWater )
					EMIT_SOUND( ENT(pev), CHAN_AUTO, "player/water_small_splash.wav", 1, ATTN_NORM );
				else
					EMIT_SOUND( ENT(pev), CHAN_AUTO, "player/water_splash.wav", 1, ATTN_NORM );
			}
			else
			{
				EMIT_SOUND( ENT(pev), CHAN_AUTO, "player/water_small_splash.wav", 1, ATTN_NORM );
			}

			m_bSubmerged = true;
		}
	}
	else
	{
		if ( pev->waterlevel < 3 )
		{
			EMIT_SOUND( ENT(pev), CHAN_AUTO, "player/water_jump.wav", 1, ATTN_NORM );
			m_bSubmerged = false;
		}
	}

	if ( pev->waterlevel > 2 )
	{
		if ( !HasNamedPlayerItem( "weapon_action" ) )
			GiveNamedItem( "weapon_action", false, false );

		if ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
			m_pStoredActionItem = m_pActiveItem;

		if ( HasNamedPlayerItem( "weapon_action" ) )
		{
			if ( ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) ) || !m_pActiveItem )
			{
				CBasePlayerItem *pItem = NULL;

				for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
				{
					if ( m_rgpPlayerItems[ i ] )
					{
						pItem = m_rgpPlayerItems[ i ];

						while ( pItem )
						{
							if ( FClassnameIs( pItem->pev, "weapon_action" ) )
								break;

							pItem = pItem->m_pNext;
						}
					}

					if ( pItem )
						break;
				}

				if ( pItem )
				{
					pItem->Deploy();
					pItem->UpdateItemInfo();
					m_pActiveItem = pItem;
				}
			}
		}

		if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 1 )
			m_pActiveItem->pev->frags = 1;
	}
	else
	{
		if ( ( !m_pActiveItem || ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && pev->fuser3 <= 0 ) )
			&& ( pev->button & IN_ATTACK2 ) && gpGlobals->time > m_flNextPunchTime )
		{
			if ( !HasNamedPlayerItem( "weapon_action" ) )
				GiveNamedItem( "weapon_action", false, false );

			if ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
				m_pStoredActionItem = m_pActiveItem;
			else
				m_pStoredActionItem = NULL;

			if ( HasNamedPlayerItem( "weapon_action" ) )
			{
				if ( ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) ) || !m_pActiveItem )
				{
					CBasePlayerItem *pItem = NULL;

					for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
					{
						if ( m_rgpPlayerItems[ i ] )
						{
							pItem = m_rgpPlayerItems[ i ];

							while ( pItem )
							{
								if ( FClassnameIs( pItem->pev, "weapon_action" ) )
									break;

								pItem = pItem->m_pNext;
							}
						}

						if ( pItem )
							break;
					}

					if ( pItem )
					{
						pItem->Deploy();
						pItem->UpdateItemInfo();
						m_pActiveItem = pItem;
					}
				}
			}

			if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
			{
				SetNextThink( 0.233 );
				SetThink( &CBasePlayer::MeleeHitThinkPunch );

				m_flNextPunchTime = gpGlobals->time + 1;

				TraceResult tr;

				UTIL_MakeVectors( pev->v_angle );
				Vector vecSrc = GetGunPosition();
				Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

				ViewPunch( 1, 1, 3 );
				EMIT_SOUND( ENT(pev), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );

				if ( tr.flFraction < 1.0 )
				{
					if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 4 )
						m_pActiveItem->pev->frags = 10;
				}
				else
				{
					if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 5 )
						m_pActiveItem->pev->frags = 11;
				}
			}
		}

		if ( !HasWeapons() && ( pev->flags & (1<<18) ) && !m_bWaitingToFall && m_flFallVelocity < 350 && gpGlobals->time > m_flNextAttack )
		{
			if ( !HasNamedPlayerItem( "weapon_action" ) )
				GiveNamedItem( "weapon_action", false, false );

			if ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
				m_pStoredActionItem = m_pActiveItem;

			if ( HasNamedPlayerItem( "weapon_action" ) )
			{
				if ( ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) ) || !m_pActiveItem )
				{
					CBasePlayerItem *pItem = NULL;

					for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
					{
						if ( m_rgpPlayerItems[ i ] )
						{
							pItem = m_rgpPlayerItems[ i ];

							while ( pItem )
							{
								if ( FClassnameIs( pItem->pev, "weapon_action" ) )
									break;

								pItem = pItem->m_pNext;
							}
						}

						if ( pItem )
							break;
					}

					if ( pItem )
					{
						pItem->Deploy();
						pItem->UpdateItemInfo();
						m_pActiveItem = pItem;
					}
				}
			}

			if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 2 )
				m_pActiveItem->pev->frags = 2;
		}
		else if ( !HasWeapons() && !m_pStoredActionItem && pev->fuser3 <= 0 )
		{
			if ( ( pev->button & IN_ATTACK2 ) && gpGlobals->time > m_flNextPunchTime )
			{
				if ( !HasNamedPlayerItem( "weapon_action" ) )
					GiveNamedItem( "weapon_action", false, false );

				if ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
					m_pStoredActionItem = m_pActiveItem;

				if ( HasNamedPlayerItem( "weapon_action" ) )
				{
					if ( ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) ) || !m_pActiveItem )
					{
						CBasePlayerItem *pItem = NULL;

						for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
						{
							if ( m_rgpPlayerItems[ i ] )
							{
								pItem = m_rgpPlayerItems[ i ];

								while ( pItem )
								{
									if ( FClassnameIs( pItem->pev, "weapon_action" ) )
										break;

									pItem = pItem->m_pNext;
								}
							}

							if ( pItem )
								break;
						}

						if ( pItem )
						{
							pItem->Deploy();
							pItem->UpdateItemInfo();
							m_pActiveItem = pItem;
						}
					}
				}

				if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 2 )
				{
					SetNextThink( 0.233 );
					SetThink( &CBasePlayer::MeleeHitThinkPunch );

					m_flNextPunchTime = gpGlobals->time + 1;

					TraceResult tr;

					UTIL_MakeVectors( pev->v_angle );
					Vector vecSrc = GetGunPosition();
					Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

					UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT(pev), &tr );

					ViewPunch( 1, 1, 3 );
					EMIT_SOUND( ENT(pev), CHAN_AUTO, "weapons/mobile/punch_swing.wav", 1, ATTN_NORM );

					if ( tr.flFraction < 1.0 )
					{
						if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 4 )
							m_pActiveItem->pev->frags = 10;
					}
					else
					{
						if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 5 )
							m_pActiveItem->pev->frags = 11;
					}
				}
			}
		}
		else
		{
			if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) && m_pActiveItem->pev->frags != 20 )
			{
				if ( m_pActiveItem->pev->frags == 3 )
				{
					m_pActiveItem->Holster();
					m_pActiveItem = NULL;

					if ( m_pStoredActionItem && HasNamedPlayerItem( STRING( m_pStoredActionItem->pev->classname ) ) )
					{
						m_pStoredActionItem->Deploy();
						m_pStoredActionItem->UpdateItemInfo();
						m_pActiveItem = m_pStoredActionItem;
						m_pStoredActionItem = NULL;
					}
				}
				else if ( m_pActiveItem->pev->frags != 5 )
				{
					m_pActiveItem->Holster();
					m_pActiveItem = NULL;

					if ( m_pStoredActionItem && HasNamedPlayerItem( STRING( m_pStoredActionItem->pev->classname ) ) )
					{
						m_pStoredActionItem->Deploy();
						m_pStoredActionItem->UpdateItemInfo();
						m_pActiveItem = m_pStoredActionItem;
						m_pStoredActionItem = NULL;
					}
				}
			}
		}
	}

	if ( m_pAttachedCamera )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( m_pAttachedCamera->edict() );

		if ( pEntity )
			((CBaseAnimating *)pEntity)->StudioFrameAdvance( 0 );
	}

	if ( m_iIsAFaggot )
	{
		if ( !( pev->flags & FL_FROZEN ) )
			EnableControl( FALSE );
	}

	pev->vuser2[1] = pev->velocity.Length2D();

	if ( foot_splash )
	{
		TraceResult splashtr;
		Vector SplashStart, SplashEnd;

		switch ( foot_splash )
		{
		case 1:
			SplashStart = pev->origin + pev->view_ofs - gpGlobals->v_right * 6;
			SplashEnd = SplashStart - Vector( 0, 0, 100 );

			UTIL_TraceLine( SplashStart, SplashEnd, ignore_monsters, dont_ignore_glass, ENT(pev), &splashtr );

			if ( splashtr.flFraction != 1.0 )
			{
				if ( UTIL_PointContents( splashtr.vecEndPos ) == CONTENTS_WATER )
				{
					Vector SplashOrigin = splashtr.vecEndPos;

					while ( UTIL_PointContents( SplashOrigin ) == CONTENTS_WATER )
						SplashOrigin.z += 1;

					MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
						WRITE_COORD( SplashOrigin.x );
						WRITE_COORD( SplashOrigin.y );
						WRITE_COORD( SplashOrigin.z );
						WRITE_SHORT( 10 );
					MESSAGE_END();
				}
			}
			break;

		case 2:
			SplashStart = pev->origin + pev->view_ofs + gpGlobals->v_right * 6;
			SplashEnd = SplashStart - Vector( 0, 0, 100 );

			UTIL_TraceLine( SplashStart, SplashEnd, ignore_monsters, dont_ignore_glass, ENT(pev), &splashtr );

			if ( splashtr.flFraction != 1.0 )
			{
				if ( UTIL_PointContents( splashtr.vecEndPos ) == CONTENTS_WATER )
				{
					Vector SplashOrigin = splashtr.vecEndPos;

					while ( UTIL_PointContents( SplashOrigin ) == CONTENTS_WATER )
						SplashOrigin.z += 1;

					MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
						WRITE_COORD( SplashOrigin.x );
						WRITE_COORD( SplashOrigin.y );
						WRITE_COORD( SplashOrigin.z );
						WRITE_SHORT( 10 );
					MESSAGE_END();
				}
			}
			break;

		case 3:
			SplashStart = pev->origin + pev->view_ofs;
			SplashEnd = SplashStart - Vector( 0, 0, 100 );

			UTIL_TraceLine( SplashStart, SplashEnd, ignore_monsters, dont_ignore_glass, ENT(pev), &splashtr );

			if ( splashtr.flFraction != 1.0 )
			{
				if ( UTIL_PointContents( splashtr.vecEndPos ) == CONTENTS_WATER )
				{
					Vector SplashOrigin = splashtr.vecEndPos;

					while ( UTIL_PointContents( SplashOrigin ) == CONTENTS_WATER )
						SplashOrigin.z += 1;

					MESSAGE_BEGIN( MSG_ALL, gmsgRipple );
						WRITE_COORD( SplashOrigin.x );
						WRITE_COORD( SplashOrigin.y );
						WRITE_COORD( SplashOrigin.z );
						WRITE_SHORT( 40 );
					MESSAGE_END();
				}
			}
			break;
		}

		foot_splash = 0;
	}

	if ( m_iFlashlightFlags != m_iClientFlashlightFlags )
	{
		m_iClientFlashlightFlags = m_iFlashlightFlags;

		MESSAGE_BEGIN( MSG_ONE, gmsgFlashFlags, NULL, pev );
			WRITE_SHORT( m_iFlashlightFlags );
		MESSAGE_END();
	}

	if ( m_iHealthCap > 0 )
	{
		if ( m_iHealthCap == 1 )
		{
			if ( pev->health > 80 )
				pev->health = 80;
		}

		if ( m_iHealthCap == 2 )
		{
			if ( pev->health > 66 )
				pev->health = 66;
		}
	}

	if ( m_bPhoneLow && !m_iPhoneDisabled
		&& gpGlobals->time > m_fNextPhoneWarnTime )
	{
		if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_mobile" ) )
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/low_battery_warn.wav", 1, ATTN_NONE, 0, PITCH_NORM );
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "phone/phone_vibrate.wav", 1, ATTN_NONE, 0, PITCH_NORM );
		}
		else
		{
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "weapons/mobile/low_battery_warn.wav", 0.25, ATTN_NONE, 0, PITCH_NORM );
			EMIT_SOUND_DYN( edict(), CHAN_AUTO, "phone/phone_vibrate.wav", 0.25, ATTN_NONE, 0, PITCH_NORM );
		}

		m_fNextPhoneWarnTime = gpGlobals->time + 10;
	}

	if ( m_hCurrentCamera && FClassnameIs( m_hCurrentCamera->edict(), "camera" ) )
	{
		if ( m_hCurrentCamera->pev->message )
		{
			CBaseEntity *pCameraTarget = UTIL_FindEntityByTargetname( NULL, STRING( m_hCurrentCamera->pev->message ) );

			if ( pCameraTarget )
			{
				Vector VecSrc, VecAng;
			}
		}
	}

	if ( m_iSwitchbladeCuts > 10 )
		m_iSwitchbladeCuts = 10;

	if ( m_iSwitchbladeCuts < 0 )
		m_iSwitchbladeCuts = 0;

	entvars_t *pevGround;

	pevGround = VARS( pev->groundentity );

	if ( pevGround && ( pevGround->flags & FL_MONSTER ) && ( pev->flags & FL_ONGROUND ) )
		pev->velocity = pev->velocity - gpGlobals->v_forward * CVAR_GET_FLOAT( "cof_monsterpushstrength" );

	if ( m_gbIsInCutscene )
	{
		if ( pev->renderamt != 0 )
			pev->renderamt = 0;

		if ( !IsInCutscene() )
			m_gbIsInCutscene = false;
	}
	else
	{
		if ( pev->renderamt != 255 )
			pev->renderamt = 255;

		if ( IsInCutscene() )
		{
			if ( m_pActiveItem && ((CBasePlayerWeapon *)m_pActiveItem)->m_bIronSights )
				m_pActiveItem->IronsightsFrom();

			m_gbIsInCutscene = true;
		}
	}

	if ( m_bWeaponWaitingToDeploy && gpGlobals->time > m_flNextAttack )
	{
		m_pLastItem = m_pActiveItem;
		m_pActiveItem = m_pNextItem;

		if ( m_pActiveItem && !FClassnameIs( m_pActiveItem->edict(), "weapon_action" ) )
		{
			m_pActiveItem->Deploy();
			m_pActiveItem->UpdateItemInfo();
		}

		m_bWeaponWaitingToDeploy = false;
	}

	if ( m_pActiveItem )
	{
		if ( m_iFlashlightFlags & 1 )
		{
			if ( !m_iPhoneDisabled )
			{
				if ( !FClassnameIs( m_pActiveItem->pev, "weapon_mobile" ) )
				{
					if ( FClassnameIs( m_pActiveItem->pev, "weapon_dualweapon" ) )
					{
						CDualWeapon *pDualWeapon = GetClassPtr( (CDualWeapon *)m_pActiveItem->pev );

						if ( pDualWeapon->m_iLeftWeaponID != 2 && pDualWeapon->m_iRightWeaponID != 2 )
						{
							m_iFlashlightFlags &= ~1;

							if ( !m_iPhoneDisabled )
								m_iFlashlightFlags |= 2;
						}
					}
					else
					{
						m_iFlashlightFlags &= ~1;

						if ( !m_iPhoneDisabled )
							m_iFlashlightFlags |= 2;
					}
				}
			}
			else
			{
				m_iFlashlightFlags &= ~1;
			}
		}

		if ( m_iFlashlightFlags & 16 )
		{
			if ( !FClassnameIs( m_pActiveItem->pev, "weapon_flare" ) && !FClassnameIs( m_pActiveItem->pev, "weapon_booklaser" ) )
				m_iFlashlightFlags &= ~16;
		}

		if ( m_iFlashlightFlags & 2 )
		{
			if ( m_iPhoneDisabled )
			{
				m_iFlashlightFlags &= ~2;
			}
			else
			{
				if ( FClassnameIs( m_pActiveItem->pev, "weapon_mobile" ) )
				{
					if ( !m_iPhoneDisabled )
						m_iFlashlightFlags |= 1;

					m_iFlashlightFlags &= ~2;
				}

				if ( !HasNamedPlayerItem( "weapon_mobile" ) )
				{
					m_iFlashlightFlags &= ~1;
					m_iFlashlightFlags &= ~2;
				}

				if ( FClassnameIs( m_pActiveItem->pev, "weapon_dualweapon" ) )
				{
					CDualWeapon *pDualWeapon = GetClassPtr( (CDualWeapon *)m_pActiveItem->pev );

					if ( pDualWeapon )
					{
						if ( pDualWeapon->m_iLeftWeaponID == 2 || pDualWeapon->m_iRightWeaponID == 2 )
						{
							if ( !m_iPhoneDisabled )
								m_iFlashlightFlags |= 1;

							m_iFlashlightFlags &= ~2;
						}
					}
				}
			}
		}

		if ( m_iFlashlightFlags & 4 )
		{
			if ( !HasTaclight() )
				m_iFlashlightFlags &= ~4;
			else if ( !FClassnameIs( m_pActiveItem->pev, "weapon_glock" ) )
				m_iFlashlightFlags &= ~4;
		}

		if ( m_iFlashlightFlags & 8 )
		{
			if ( !FClassnameIs( m_pActiveItem->pev, "weapon_flashlight" ) && !FClassnameIs( m_pActiveItem->pev, "weapon_dualweapon" ) )
			{
				m_iFlashlightFlags &= ~8;
				m_iFlashlight = 0;
			}
		}
	}
	else
	{
		if ( m_iFlashlightFlags & 1 )
		{
			m_iFlashlightFlags &= ~1;

			if ( !( m_iFlashlightFlags & 2 ) && !m_iPhoneDisabled )
				m_iFlashlightFlags |= 2;
		}

		if ( m_iFlashlightFlags & 2 )
		{
			if ( !HasNamedPlayerItem( "weapon_mobile" ) || m_iPhoneDisabled )
			{
				m_iFlashlightFlags &= ~1;
				m_iFlashlightFlags &= ~2;
			}
		}

		if ( m_iFlashlightFlags & 4 )
			m_iFlashlightFlags &= ~4;

		if ( m_iFlashlightFlags & 8 )
		{
			m_iFlashlightFlags &= ~8;
			m_iFlashlight = 0;
		}

		if ( m_iFlashlightFlags & 16 )
			m_iFlashlightFlags &= ~16;
	}

	if ( gpGlobals->time > m_fFallingTime && m_bShouldFallFreeze && IsAlive() )
	{
		if ( m_bWaitingToFall )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				if ( !m_bHasGotUp )
				{
					PlayerAnimation( "taller_getup", false );
					m_bHasGotUp = true;
					m_flGetupTime = m_flSequenceOverrideTime;
				}

				if ( gpGlobals->time > m_flGetupTime )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
						WRITE_BYTE( 0 );
					MESSAGE_END();

					m_iSequenceOverride = -1;
					m_iGaitSequenceOverride = -1;

					if ( pev->fuser3 == 2 )
						m_bWeaponWaitingToDeploy = true;

					pev->fuser3 = 0;
					m_bShouldFallFreeze = false;
					m_bWaitingToFall = false;
					SetAnimation( PLAYER_WALK );
				}
			}
			else
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				if ( pev->fuser3 == 2 )
					m_bWeaponWaitingToDeploy = true;

				int animDesired = LookupSequence( "taller_getup" );

				if ( animDesired == -1 )
					animDesired = 0;

				if ( pev->sequence != animDesired || !m_fSequenceLoops )
					pev->frame = 0;

				pev->gaitsequence = 0;
				m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

				if ( pev->sequence != animDesired )
				{
					pev->sequence = animDesired;
					pev->frame = 0;
					ResetSequenceInfo();
				}

				pev->fuser3 = 0;
				m_bShouldFallFreeze = false;
				m_bWaitingToFall = false;
			}
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pev->fuser3 = 0;
			m_bShouldFallFreeze = false;
		}
	}

	if ( IsOnLadder() )
	{
		if ( !m_ClientLadder )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLadder, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_COORD( m_LadderAngles.x );
				WRITE_COORD( m_LadderAngles.y );
				WRITE_COORD( m_LadderAngles.z );
			MESSAGE_END();

			pev->flags |= (1<<17);
			m_ClientLadder = 1;
		}
	}
	else
	{
		if ( m_ClientLadder )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgLadder, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
				WRITE_COORD( 0 );
			MESSAGE_END();

			pev->flags &= ~(1<<17);
			m_ClientLadder = 0;
		}
	}

	if ( m_iClientViewEntIndex != m_iViewEntIndex )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgViewEntity, NULL, pev );
			WRITE_SHORT( m_iViewEntIndex );
		MESSAGE_END();

		m_iClientViewEntIndex = m_iViewEntIndex;
	}

	StaminaThink();
	BreatheThink();

	HeartBeatCheck();

	if ( pev->flags & (1<<18) )
	{
		if ( pev->vuser2.z != 1 )
			pev->vuser2.z = 1;
	}
	else
	{
		if ( pev->vuser2.z != 0 )
			pev->vuser2.z = 0;
	}

	if ( m_bIsTripping && gpGlobals->time > m_flNextMorphineDreamTime )
	{
		m_flNextMorphineDreamTime = gpGlobals->time + 11;
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, "player/morphine_dream.wav", 0.65, ATTN_NORM, 0, PITCH_NORM );
	}

	if ( m_bIsTripping && gpGlobals->time > m_fTrippingTime )
		StopMorphineTrip();

	if ( m_ClientDrawBars != m_DrawBars )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBlackBars, NULL, pev );
			WRITE_BYTE( m_DrawBars );
		MESSAGE_END();

		m_ClientDrawBars = m_DrawBars;
	}

	if ( EHandlesNotSame( m_pAttachedCamera, m_pClientAttachedCamera ) )
	{
		if ( !m_pAttachedCamera )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pev );
				WRITE_SHORT( -1 );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pev );
				WRITE_SHORT( ENTINDEX( m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		m_pClientAttachedCamera = m_pAttachedCamera;
	}

	if ( EHandlesNotSame( m_pBoat, m_pClientBoat ) )
	{
		if ( !m_pBoat )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, pev );
				WRITE_SHORT( -1 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, pev );
				WRITE_SHORT( ENTINDEX( m_pBoat->edict() ) );
			MESSAGE_END();
		}

		m_pClientBoat = m_pBoat;
	}

	if ( m_sClientObjectiveText != m_sObjectiveText )
	{
		m_sClientObjectiveText = m_sObjectiveText;

		int language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );
		char m_sLanguage[128];

		switch ( language )
		{
		case 0:
			strcpy( m_sLanguage, "txtfiles/hints.txt" );
			break;
		case 1:
			strcpy( m_sLanguage, "txtfiles/hints.txt" );
			break;
		case 2:
			strcpy( m_sLanguage, "txtfiles/languages/dutch/hints.txt" );
			break;
		case 3:
			strcpy( m_sLanguage, "txtfiles/languages/french/hints.txt" );
			break;
		case 4:
			strcpy( m_sLanguage, "txtfiles/languages/german/hints.txt" );
			break;
		case 5:
			strcpy( m_sLanguage, "txtfiles/languages/norwegian/hints.txt" );
			break;
		case 6:
			strcpy( m_sLanguage, "txtfiles/languages/spanish/hints.txt" );
			break;
		case 7:
			strcpy( m_sLanguage, "txtfiles/languages/swedish/hints.txt" );
			break;
		default:
			strcpy( m_sLanguage, "txtfiles/hints.txt" );
			break;
		}

		if ( m_sCustomObjectives )
			strcpy( m_sLanguage, STRING( m_sCustomObjectives ) );

		m_sCustomObjectives = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgObjectives, NULL, pev );
			WRITE_STRING( STRING( m_sObjectiveText ) );
			WRITE_STRING( m_sLanguage );
			if ( showobjective )
			{
				showobjective = false;
				WRITE_BYTE( 1 );
			}
			else
				WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if ( m_iClientBlurActive != m_iBlurActive )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgBlur, NULL, pev );
			WRITE_BYTE( m_iBlurActive );
		MESSAGE_END();

		m_iClientBlurActive = m_iBlurActive;
	}

	if ( m_iClientFlashlight != m_iFlashlight )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgMaglite, NULL, pev );
			WRITE_BYTE( m_iFlashlight );
		MESSAGE_END();

		m_iClientFlashlight = m_iFlashlight;
	}

	// Handle Tank controlling
	if ( m_pTank != NULL )
	{ // if they've moved too far from the gun,  or selected a weapon, unuse the gun
		if ( m_pTank->OnControls( pev ) && !pev->weaponmodel )
		{
			//m_pTank->Use( this, this, USE_SET, 2 );	// try fire the gun
		}
		else
		{  // they've moved off the platform
			m_pTank->Use( this, this, USE_OFF, 0 );
			m_pTank = NULL;
		}
	}

	if ( m_pSpecTank != NULL )
	{
		if ( m_pSpecTank->OnControls( pev ) && !pev->weaponmodel )
		{
			m_pSpecTank->Use( this, this, USE_SET, 2 );	// try fire the gun
		}
		else
		{
			m_pSpecTank->Use( this, this, USE_OFF, 0 );
			m_pSpecTank = NULL;
		}
	}

	// do weapon stuff
	ItemPostFrame( );

	if ( gDualWeapon.HasDualWeapon( this ) )
		gDualWeapon.WeaponThink( this );

	if ( FBitSet( pev->flags, FL_ONGROUND ) && pev->health > 0 && m_flFallVelocity > 175 && m_flFallVelocity < 350 )
	{
		pev->flags |= FL_ONGROUND;

		if ( pev->watertype == CONTENT_WATER )
		{
			if ( !pev->groundentity || VARS( pev->groundentity )->velocity.z == 0 )
				UTIL_SplashCalculation( this, 1 );
		}
	}

	// check to see if player landed hard enough to make a sound
	// falling farther than half of the maximum safe distance, but not as far a max safe distance will
	// play a bootscrape sound, and no damage will be inflicted. Fallling a distance shorter than half
	// of maximum safe distance will make no sound. Falling farther than max safe distance will play a
	// fallpain sound, and damage will be inflicted based on how far the player fell

	if ( (FBitSet(pev->flags, FL_ONGROUND)) && (pev->health > 0) && m_flFallVelocity >= 350 )
	{
		// ALERT ( at_console, "%f\n", m_flFallVelocity );

		if (pev->watertype == CONTENT_WATER)
		{
			// Did he hit the world or a non-moving entity?
			// BUG - this happens all the time in water, especially when
			// BUG - water has current force
			if ( !pev->groundentity || VARS(pev->groundentity)->velocity.z == 0 )
				UTIL_SplashCalculation( this, 1 );
		}
		else if ( m_flFallVelocity > 580 )
		{// after this point, we start doing damage

			float flFallDamage = g_pGameRules->FlPlayerFallDamage( this );

			if ( flFallDamage > pev->health )
			{//splat
				// note: play on item channel because we play footstep landing on body channel
				EMIT_SOUND(ENT(pev), CHAN_ITEM, "common/bodysplat.wav", 1, ATTN_NORM);
			}

			if ( flFallDamage > 0 )
			{
				TripOver();
				TakeDamage(VARS(eoNullEntity), VARS(eoNullEntity), flFallDamage, DMG_FALL );
				pev->punchangle.x = 0;
			}
		}

		if ( IsAlive() )
		{
			SetAnimation( PLAYER_WALK );
		}
	}

	if (FBitSet(pev->flags, FL_ONGROUND))
	{
		if (m_flFallVelocity > 64 && g_pGameRules && !g_pGameRules->IsMultiplayer())
		{
			CSoundEnt::InsertSound ( bits_SOUND_PLAYER, pev->origin, m_flFallVelocity, 0.2 );
			// ALERT( at_console, "fall %f\n", m_flFallVelocity );
		}
		m_flFallVelocity = 0;
	}

	// select the proper animation for the player character
	if ( IsAlive() )
	{
		if (!pev->velocity.x && !pev->velocity.y)
			SetAnimation( PLAYER_IDLE );
		else if ((pev->velocity.x || pev->velocity.y) && (FBitSet(pev->flags, FL_ONGROUND)))
			SetAnimation( PLAYER_WALK );
		else if (pev->waterlevel > 1)
			SetAnimation( PLAYER_WALK );
	}

	float flInterval;

	flInterval = StudioFrameAdvance( );
	DispatchAnimEvents( flInterval );

	if ( pDeathScene )
	{
		float flInterval = pDeathScene->StudioFrameAdvance( );
		pDeathScene->DispatchAnimEvents( flInterval );
	}

	CheckPowerups(pev);

	UpdatePlayerSound();

	// Track button info so we can detect 'pressed' and 'released' buttons next frame
	m_afButtonLast = pev->button;

pt_end:
	if ( m_iClientViewEntIndex != m_iViewEntIndex )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgViewEntity, NULL, pev );
			WRITE_SHORT( m_iViewEntIndex );
		MESSAGE_END();

		m_iClientViewEntIndex = m_iViewEntIndex;
	}

	if ( m_iMp3NeedsUpdate )
	{
		if ( FStringNull( m_strCurrentMp3 ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
				WRITE_COORD( m_fMp3FadeTime );
				WRITE_STRING( "nothing" );
			MESSAGE_END();

			m_fMp3FadeTime = 0;
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgMP3, NULL, pev );
				if ( m_bMp3ShouldLoop )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );

				WRITE_BYTE( 1 );
				WRITE_COORD( m_fMp3FadeInTime );
				WRITE_STRING( STRING( m_strCurrentMp3 ) );
			MESSAGE_END();
		}

		m_iMp3NeedsUpdate = 0;
	}
}


bool CBasePlayer::IsInCutscene( void )
{
	if ( ( m_hCurrentCamera || m_pAttachedCamera ) && m_iViewEntIndex && m_iViewEntIndex != ENTINDEX( edict() ) )
		return true;

	return false;
}

//=========================================================
// IsSpawnPointValid - Returns TRUE if the spawn point is valid
//=========================================================
BOOL IsSpawnPointValid( CBaseEntity *pPlayer, CBaseEntity *pSpot )
{
	CBaseEntity *ent = NULL;

	if ( pSpot->GetState( pPlayer ) != STATE_ON )
	{
		return FALSE;
	}

	while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 )) != NULL )
	{
		// if ent is a client, don't spawn on 'em
		if ( ent->IsPlayer() && ent != pPlayer )
			return FALSE;
	}

	return TRUE;
}


DLL_GLOBAL CBaseEntity	*g_pLastSpawn;

/*
============
EntSelectSpawnPoint

Returns the entity to spawn at

USES AND SETS GLOBAL g_pLastSpawn
============
*/
edict_t *EntSelectSpawnPoint( CBaseEntity *pPlayer )
{
	CBaseEntity *pSpot = NULL;

	CBaseEntity *pSpawn1 = NULL;
	CBaseEntity *pSpawn2 = NULL;
	CBaseEntity *pSpawn3 = NULL;
	CBaseEntity *pSpawn4 = NULL;

	edict_t		*player;

	player = pPlayer->edict();

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pCast = UTIL_PlayerByIndex( i );

			if ( pCast && pPlayer && pPlayer == pCast )
			{
				switch ( i )
				{
				case 1:
					while ( ( pSpawn1 = UTIL_FindEntityByClassname( pSpawn1, "cof_coop_spawn1" ) ) != NULL )
					{
						if ( pSpawn1 && pSpawn1->pev->iuser1 == 0 )
							return pSpawn1->edict();
					}
					break;
				case 2:
					while ( ( pSpawn2 = UTIL_FindEntityByClassname( pSpawn2, "cof_coop_spawn2" ) ) != NULL )
					{
						if ( pSpawn2 && pSpawn2->pev->iuser1 == 0 )
							return pSpawn2->edict();
					}
					break;
				case 3:
					while ( ( pSpawn3 = UTIL_FindEntityByClassname( pSpawn3, "cof_coop_spawn3" ) ) != NULL )
					{
						if ( pSpawn3 && pSpawn3->pev->iuser1 == 0 )
							return pSpawn3->edict();
					}
					break;
				case 4:
					while ( ( pSpawn4 = UTIL_FindEntityByClassname( pSpawn4, "cof_coop_spawn4" ) ) != NULL )
					{
						if ( pSpawn4 && pSpawn4->pev->iuser1 == 0 )
							return pSpawn4->edict();
					}
					break;
				}
			}
		}
	}

// choose a info_player_deathmatch point
	if ( g_pGameRules && g_pGameRules->IsCoOp() )
	{
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_coop");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
		pSpot = UTIL_FindEntityByClassname( g_pLastSpawn, "info_player_start");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}
	else if ( g_pGameRules && g_pGameRules->IsDeathmatch() )
	{
		pSpot = g_pLastSpawn;
		// Randomize the start spot
		for ( int i = RANDOM_LONG(1,5); i > 0; i-- )
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		if ( FNullEnt( pSpot ) )  // skip over the null point
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );

		CBaseEntity *pFirstSpot = pSpot;

		do
		{
			if ( pSpot )
			{
				// check if pSpot is valid
				if ( IsSpawnPointValid( pPlayer, pSpot ) )
				{
					if ( pSpot->pev->origin == Vector( 0, 0, 0 ) )
					{
						pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
						continue;
					}

					// if so, go to pSpot
					goto ReturnSpot;
				}
			}
			// increment pSpot
			pSpot = UTIL_FindEntityByClassname( pSpot, "info_player_deathmatch" );
		} while ( pSpot != pFirstSpot ); // loop if we're not back to the start

		// we haven't found a place to spawn yet,  so kill any guy at the first spawn point and spawn there
		if ( !FNullEnt( pSpot ) )
		{
			CBaseEntity *ent = NULL;
			while ( (ent = UTIL_FindEntityInSphere( ent, pSpot->pev->origin, 128 )) != NULL )
			{
				// if ent is a client, kill em (unless they are ourselves)
				if ( ent->IsPlayer() && !(ent->edict() == player) )
					ent->TakeDamage( VARS(INDEXENT(0)), VARS(INDEXENT(0)), 300, DMG_GENERIC );
			}
			goto ReturnSpot;
		}
	}

	// If startspot is set, (re)spawn there.
	if ( FStringNull( gpGlobals->startspot ) || !strlen(STRING(gpGlobals->startspot)))
	{
		pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}
	else
	{
		pSpot = UTIL_FindEntityByTargetname( NULL, STRING(gpGlobals->startspot) );
		if ( !FNullEnt(pSpot) )
			goto ReturnSpot;
	}

ReturnSpot:
	if ( FNullEnt( pSpot ) )
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return INDEXENT(0);
	}

	g_pLastSpawn = pSpot;
	return pSpot->edict();
}

void GetCurrentPath( char *path )
{
	getcwd( path, 260 );
}

void CBasePlayer::CrashGame( void )
{
	exit( -1 );
}

void CBasePlayer::Spawn( void )
{
	char CurrentPath[260];

	GetCurrentPath( CurrentPath );

	m_fFaggotTime = gpGlobals->time;

	pev->classname		= MAKE_STRING("player");
	pev->health			= 100;
	pev->armorvalue		= 0;
	pev->takedamage		= DAMAGE_AIM;
	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_WALK;
	pev->max_health		= pev->health;
	pev->flags		   &= FL_PROXY;	// keep proxy flag sey by engine
	pev->flags		   |= FL_CLIENT;
	pev->dmg			= 2;				// initial water damage
	pev->effects		= 0;
	pev->deadflag		= DEAD_NO;
	pev->dmg_take		= 0;
	pev->dmg_save		= 0;
	pev->friction		= 1.0;
	pev->gravity		= 1.0;
	m_bitsHUDDamage		= -1;
	m_bitsDamageType	= 0;
	m_afPhysicsFlags	= 0;
	m_fLongJump			= FALSE;// no longjump module.

	m_iClientPhoneDisabled = -1;

	// buz
	m_strCurrentMp3 = 0;
	m_fMp3FadeTime = 0;
	m_fMp3FadeInTime = 0;
	m_bMp3ShouldLoop = false;

	SetJumpHeight(100);

	// buz rain
	Rain_dripsPerSecond = 0;
	Rain_windX = 0;
	Rain_windY = 0;
	Rain_randX = 0;
	Rain_randY = 0;
	Rain_ideal_dripsPerSecond = 0;
	Rain_ideal_windX = 0;
	Rain_ideal_windY = 0;
	Rain_ideal_randX = 0;
	Rain_ideal_randY = 0;
	Rain_endFade = 0;
	Rain_nextFadeUpdate = 0;

	ClearOutSlot( 1, 1 );
	m_iFlashlight = 0;
	m_iFlashlightFlags &= ~COFR_COOP_FLASHLIGHT;

	pev->vuser2.x = 90;

	xbox_lshoulderdown = false;
	xbox_rshoulderdown = false;
	xbox_ltriggerdown = false;
	xbox_rtriggerdown = false;
	xbox_dpadup = false;
	xbox_dpaddown = false;
	xbox_dpadleft = false;
	xbox_dpadright = false;
	xbox_ducktoggle = false;
	xbox_haspressedduck = false;

	m_iMp3NeedsUpdate = 1;

	pPlayerToRevive = NULL;
	pPlayerReviving = NULL;

	m_flReviveTime = gpGlobals->time;
	m_flNextAdLibTime = gpGlobals->time;

	m_flDeadAutoReviveTime = 0;

	m_flNextDeadNotifyTime = gpGlobals->time;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		for ( int fuckyoufaggotswatching = 1; fuckyoufaggotswatching <= gpGlobals->maxClients; fuckyoufaggotswatching++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( fuckyoufaggotswatching );

			if ( plr && plr == this )
			{
				m_iCoopStuckPushYaw = fuckyoufaggotswatching * 50;

				if ( m_iCoopStuckPushYaw >= 360 )
					m_iCoopStuckPushYaw = RANDOM_LONG( 0, 350 );

				break;
			}
		}
	}

	m_iLookingAtPlayer = 0;

	m_bKilledLyingDown = false;

	char *m_sMaterials = (char *)CVAR_GET_STRING( "sv_custommaterials" );

	if ( strlen( m_sMaterials ) > 2 )
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "mtl", m_sMaterials );
	else
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "mtl", "0" );

	clientmaterials = true;

	CLIENT_COMMAND( edict(), "donatorcheck\n" );

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	if ( g_iSurvivalMode )
	{
		for ( int k = 0; k < 32; k++ )
			m_bFirstDeploy[k] = true;
	}

	m_flNextMPPlayerNameCheck = gpGlobals->time;

	m_bCoopGameOver = false;
	m_flNextCountDown = 0;
	m_iGameOverCounter = 0;

	pev->fuser4 = 0;

	m_bCantBeRevived = false;

	m_flSequenceOverrideTime = m_flGaitSequenceOverrideTime = gpGlobals->time;
	m_iSequenceOverride = m_iGaitSequenceOverride = -1;

	m_fUnderwaterBreatheTime = gpGlobals->time;

	lastquickslotpressed = 0;

	heartbeat = false;

	m_iGoodPoints = 2;
	m_bBossHealthBar = false;

	m_vPreviousOrigin = pev->origin;

	m_bNoDeadFade = false;

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "asd", CVAR_GET_STRING( "cof_walksteptime" ) );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "bsd", CVAR_GET_STRING( "cof_runsteptime" ) );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "csd", CVAR_GET_STRING( "cof_sprintsteptime" ) );

	m_sCustomObjectives = 0;
	m_sClientObjectiveText = 0;
	m_iClientBlurActive = -1;
	m_bSilentGive = false;

	SET_VIEW( edict(), edict() );

	UTIL_ProMessage( this, " ", 2, true );

	m_pPadlock = NULL;

	if ( m_iPadlockNumber[1] == 0 || m_iPadlockNumber[2] == 0 || m_iPadlockNumber[3] == 0 || m_iPadlockNumber[4] == 0 )
	{
		m_iPadlockNumber[1] = RANDOM_LONG( 1, 9 );
		m_iPadlockNumber[2] = RANDOM_LONG( 1, 9 );
		m_iPadlockNumber[3] = RANDOM_LONG( 1, 9 );
		m_iPadlockNumber[4] = RANDOM_LONG( 1, 9 );
	}

	m_LadderAngles = g_vecZero;
	m_OnStairs = false;
	m_bUsedFirstSyringe = false;
	m_fMorphineTripTime = 0;
	m_fTrippingTime = 0;
	m_fFallingTime = 0;
	m_iUsingComputerOrPhone = 0;

	SetCorrectView();
	SetCorrectMo();

	m_iMorphineRecover = 0;
	m_iMorphineCycleTime = gpGlobals->time;

	m_fDodgeTime = gpGlobals->time;

	m_iViewEntIndex = 0;
	m_fStamina = 100;
	m_fStaminaChargeCyle = m_fStaminaBreatheCyle = gpGlobals->time;

	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgStamina, NULL, pev );
		WRITE_BYTE( m_fStamina );
		WRITE_COORD( 0 );
	MESSAGE_END();

	pev->fov = m_iFOV				= 0;// init field of view.
	m_iClientFOV		= -1; // make sure fov reset is sent

	m_flNextDecalTime	= 0;// let this player decal as soon as he spawns.

	m_flgeigerDelay = gpGlobals->time + 2.0;	// wait a few seconds until user-defined message registrations
												// are recieved by all clients

	m_flTimeStepSound	= 0;
	m_iStepLeft = 0;
	m_flFieldOfView		= 0.5;// some monsters use this to determine whether or not the player is looking at them.

	m_bloodColor	= BLOOD_COLOR_RED;
	m_flNextAttack	= UTIL_WeaponTimeBase();
	StartSneaking();

	m_iFlashBattery = 99;
	m_flFlashLightTime = 1; // force first message

// dont let uninitialized value here hurt the player
	m_flFallVelocity = 0;

	g_pGameRules->SetDefaultPlayerTeam( this );
	edict_t* startent = g_pGameRules->GetPlayerSpawnSpot( this );

	//LRC- support the new "start with HEV" flag... // buz - moved here
	if (startent->v.spawnflags & 1) // the START WITH SUIT flag
		pev->weapons |= (1<<WEAPON_SUIT);

    SET_MODEL(ENT(pev), "models/player.mdl");
    g_ulModelIndexPlayer = pev->modelindex;
	pev->sequence		= LookupActivity( ACT_IDLE );

	if ( FBitSet(pev->flags, FL_DUCKING) )
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	else
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);

    pev->view_ofs = VEC_VIEW;
	Precache();
	m_HackedGunPos		= Vector( 0, 32, 0 );

	if ( m_iPlayerSound == SOUNDLIST_EMPTY )
	{
		ALERT ( at_debug, "Couldn't alloc player sound slot!\n" );
	}

	m_fNoPlayerSound = FALSE;// normal sound behavior.

	m_pLastItem = NULL;
	m_fInitHUD = TRUE;
	m_iClientHideHUD = -1;  // force this to be recalculated
	m_fWeapon = FALSE;
	m_pClientActiveItem = NULL;
	m_iClientBattery = -1;

	// reset all ammo values to 0
	for ( int i = 0; i < MAX_AMMO_SLOTS; i++ )
	{
		m_rgAmmo[i] = 0;
		m_rgAmmoLast[i] = 0;  // client ammo values also have to be reset  (the death hud clear messages does on the client side)
	}

	m_lastx = m_lasty = 0;

	m_flNextChatTime = gpGlobals->time;

	g_pGameRules->PlayerSpawn( this );
}


void CBasePlayer :: Precache( void )
{
	// in the event that the player JUST spawned, and the level node graph
	// was loaded, fix all of the node graph pointers before the game starts.

	// !!!BUGBUG - now that we have multiplayer, this needs to be moved!
	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( !WorldGraph.FSetGraphPointers() )
		{
			ALERT ( at_debug, "**Graph pointers were not set!\n");
		}
		else
		{
			ALERT ( at_debug, "**Graph Pointers Set!\n" );
		}
	}

	// SOUNDS / MODELS ARE PRECACHED in ClientPrecache() (game specific)
	// because they need to precache before any clients have connected

	// init geiger counter vars during spawn and each time
	// we cross a level transition

	m_flgeigerRange = 1000;
	m_igeigerRangePrev = 1000;

	m_bitsDamageType = 0;
	m_bitsHUDDamage = -1;

	m_iClientPhoneDisabled = -1;

	m_iClientBattery = -1;

	m_iTrain = TRAIN_NEW;

	m_fMp3FadeTime = 5;
	m_fMp3FadeInTime = 5;
	m_iGoalNeedsUpdate = 2; // buz
	m_iInitMessagesSent = 0; // buz

	// buz: Paranoia's speed adjustment
	pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;

	// Make sure any necessary user messages have been registered
	LinkUserMessages();

	m_iUpdateTime = 5;  // won't update for 1/2 a second

	// buz fix
	TabulateAmmo();

	Rain_needsUpdate = 1; // buz rain

	if ( gInitHUD )
		m_fInitHUD = TRUE;
}


int CBasePlayer::Save( CSave &save )
{
	cofrInventorySchema=1;
	if ( !CBaseMonster::Save(save) )
		return 0;

	int status = save.WriteFields( "cPLAYER", "PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData) );

	m_iPlayerSaveLock = 0;
	return status;
}


//
// Marks everything as new so the player will resend this to the hud.
//
void CBasePlayer::RenewItems(void)
{

}


int CBasePlayer::Restore( CRestore &restore )
{
	if ( !CBaseMonster::Restore(restore) )
		return 0;

	int status = restore.ReadFields( "PLAYER", this, m_playerSaveData, ARRAYSIZE(m_playerSaveData) );
	CoFRMigrateInventory();

	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;
	// landmark isn't present.
	if ( !pSaveData->fUseLandmark )
	{
		ALERT( at_debug, "No Landmark:%s\n", pSaveData->szLandmarkName );

		// default to normal spawn
		edict_t* pentSpawnSpot = EntSelectSpawnPoint( this );
		pev->origin = VARS(pentSpawnSpot)->origin + Vector(0,0,1);
		pev->angles = VARS(pentSpawnSpot)->angles;
		m_vPreviousOrigin = pev->origin;
	}
	pev->v_angle.z = 0;	// Clear out roll
	pev->angles = pev->v_angle;

	pev->fixangle = TRUE;           // turn this way immediately

// Copied from spawn() for now
	m_bloodColor	= BLOOD_COLOR_RED;

    g_ulModelIndexPlayer = pev->modelindex;

	if ( FBitSet(pev->flags, FL_DUCKING) )
	{
		// Use the crouch HACK
		//FixPlayerCrouchStuck( edict() );
		// Don't need to do this with new player prediction code.
		UTIL_SetSize(pev, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX);
	}
	else
	{
		UTIL_SetSize(pev, VEC_HULL_MIN, VEC_HULL_MAX);
	}

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "hl", "1" );

	if ( m_fLongJump )
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "1" );
	}
	else
	{
		g_engfuncs.pfnSetPhysicsKeyValue( edict(), "slj", "0" );
	}

	if ( g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		if ( !m_bLowHealth )
			g_engfuncs.pfnSetPhysicsKeyValue( edict(), "low", "0" );
		else
			g_engfuncs.pfnSetPhysicsKeyValue( edict(), "low", "1" );
	}

	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "asd", CVAR_GET_STRING( "cof_walksteptime" ) );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "bsd", CVAR_GET_STRING( "cof_runsteptime" ) );
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "csd", CVAR_GET_STRING( "cof_sprintsteptime" ) );

	// buz: restore jump height
	SetJumpHeight(m_iJumpHeight);

	if ( m_strCurrentMp3 )
		PlayerSetMP3( m_strCurrentMp3, 1, 1 );

	m_fFaggotTime = gpGlobals->time;

	RenewItems();

	g_flWeaponCheat = CVAR_GET_FLOAT( "sv_cheats" );

	m_bRestored = true;

	m_flNextTimeTakenCheck = m_flNextDistanceCheckTime = gpGlobals->time;

	return status;
}

void CBasePlayer::PlayNewDoorSound( void )
{
	if ( g_iDoorSound )
	{
		EMIT_SOUND_DYN( edict(), CHAN_AUTO, STRING( g_iDoorSound ), 1, ATTN_NONE, 0, PITCH_NORM );
		g_iDoorSound = 0;
	}
}

void CBasePlayer::SelectNextItem( int iItem )
{
	if ( m_fPhoneCallTime > gpGlobals->time )
		return;

	CBasePlayerItem *pItem;

	pItem = m_rgpPlayerItems[ iItem ];

	if (!pItem)
		return;

	if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_flNextAttack > gpGlobals->time )
		return;

	if (pItem == m_pActiveItem)
	{
		// select the next one in the chain
		pItem = m_pActiveItem->m_pNext;
		if (! pItem)
		{
			return;
		}

		CBasePlayerItem *pLast;
		pLast = pItem;
		while (pLast->m_pNext)
			pLast = pLast->m_pNext;

		// relink chain
		pLast->m_pNext = m_pActiveItem;
		m_pActiveItem->m_pNext = NULL;
		m_rgpPlayerItems[ iItem ] = pItem;
	}

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
	{
		m_pActiveItem->Holster( );
	}

	m_bWeaponWaitingToDeploy = true;
}

void CBasePlayer::SelectItem(const char *pstr)
{
	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	if (!pstr)
		return;

	if ( m_fPhoneCallTime > gpGlobals->time )
		return;

	if ( m_pActiveItem && ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack > gpGlobals->time )
		return;

	if ( m_pActiveItem && ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextSecondaryAttack > gpGlobals->time )
		return;

	if ( m_pActiveItem && ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextTertiaryAttack > gpGlobals->time )
		return;

	if ( m_flNextAttack > gpGlobals->time )
		return;

	if ( pev->waterlevel >= 3 && strcmp( pstr, "weapon_action" ) )
		return;

	if ( pev->fuser3 == 10 )
		return;

	CBasePlayerItem *pItem = NULL;

	for (int i = 0; i < MAX_ITEM_TYPES; i++)
	{
		if (m_rgpPlayerItems[i])
		{
			pItem = m_rgpPlayerItems[i];

			while (pItem)
			{
				if (FClassnameIs(pItem->pev, pstr))
					break;
				pItem = pItem->m_pNext;
			}
		}

		if (pItem)
			break;
	}

	if (!pItem)
		return;


	if (pItem == m_pActiveItem)
		return;

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
	{
		if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_bIronSights )
			m_pActiveItem->IronsightsFrom( );

		m_pActiveItem->Holster( );
	}

	m_bWeaponWaitingToDeploy = true;

	m_pNextItem = pItem;
}


void CBasePlayer::SelectLastItem( bool nowait )
{
	if (!m_pLastItem)
	{
		return;
	}

	if ( !nowait )
	{
		if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack > gpGlobals->time )
			return;

		if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextSecondaryAttack > gpGlobals->time )
			return;

		if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextTertiaryAttack > gpGlobals->time )
			return;

		if ( m_flNextAttack > gpGlobals->time )
			return;
	}

	if ( m_fPhoneCallTime > gpGlobals->time )
		return;

	// buz fix: dont allow to switch to guns with no bullets
	if (!m_pLastItem->CanDeploy())
		return;

	if ( m_pActiveItem && !m_pActiveItem->CanHolster() )
	{
		return;
	}

	ResetAutoaim( );

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster( );

	m_bWeaponWaitingToDeploy = true;

	CBasePlayerItem *pTemp = m_pActiveItem;
	m_pNextItem = m_pLastItem;
	m_pLastItem = pTemp;
}

//==============================================
// HasWeapons - do I have any weapons at all?
//==============================================
BOOL CBasePlayer::HasWeapons( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] && m_rgpPlayerItems[ i ]->m_iId != 17 )
		{
			return TRUE;
		}
	}

	return FALSE;
}

void CBasePlayer::SelectPrevItem( int iItem )
{
}


const char *CBasePlayer::TeamID( void )
{
	if ( pev == NULL )		// Not fully connected yet
		return "";

	// return their team name
	return m_szTeamName;
}


//==============================================
// !!!UNDONE:ultra temporary SprayCan entity to apply
// decal frame at a time. For PreAlpha CD
//==============================================
class CSprayCan : public CBaseEntity
{
public:
	void	Spawn ( entvars_t *pevOwner );
	void	Think( void );

	virtual int	ObjectCaps( void ) { return FCAP_DONT_SAVE; }
};

void CSprayCan::Spawn ( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector ( 0 , 0 , 32 );
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);
	pev->frame = 0;

	SetNextThink( 0.1 );
	//EMIT_SOUND(ENT(pev), CHAN_VOICE, "player/sprayer.wav", 1, ATTN_NORM);
}

void CSprayCan::Think( void )
{
	TraceResult	tr;
	int playernum;
	int nFrames;
	CBasePlayer *pPlayer;

	pPlayer = (CBasePlayer *)GET_PRIVATE(pev->owner);

	if (pPlayer)
		nFrames = pPlayer->GetCustomDecalFrames();
	else
		nFrames = -1;

	playernum = ENTINDEX(pev->owner);

	// ALERT(at_console, "Spray by player %i, %i of %i\n", playernum, (int)(pev->frame + 1), nFrames);

	UTIL_MakeVectors(pev->angles);
	UTIL_TraceLine ( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, & tr);

	// No customization present.
	if (nFrames == -1)
	{
		UTIL_DecalTrace( &tr, DECAL_LAMBDA6 );
		UTIL_Remove( this );
	}
	else
	{
		UTIL_PlayerDecalTrace( &tr, playernum, pev->frame, TRUE );
		// Just painted last custom frame.
		if ( pev->frame++ >= (nFrames - 1))
			UTIL_Remove( this );
	}

	SetNextThink( 0.1 );
}

class	CBloodSplat : public CBaseEntity
{
public:
	void	Spawn ( entvars_t *pevOwner );
	void	Spray ( void );
};

void CBloodSplat::Spawn ( entvars_t *pevOwner )
{
	pev->origin = pevOwner->origin + Vector ( 0 , 0 , 32 );
	pev->angles = pevOwner->v_angle;
	pev->owner = ENT(pevOwner);

	SetThink(&CBloodSplat:: Spray );
	SetNextThink( 0.1 );
}

void CBloodSplat::Spray ( void )
{
	TraceResult	tr;

	if ( g_Language != LANGUAGE_GERMAN )
	{
		UTIL_MakeVectors(pev->angles);
		UTIL_TraceLine ( pev->origin, pev->origin + gpGlobals->v_forward * 128, ignore_monsters, pev->owner, & tr);

		UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
	}
	SetThink(&CBloodSplat:: SUB_Remove );
	SetNextThink( 0.1 );
}

//==============================================

void CBasePlayer::GiveNamedItem( const char *pszName, bool deletewhendropped, bool bSkipDonatorCheck_guess )
{
	if ( pszName && !strcmp( pszName, "weapon_famas" ) )
	{
		if ( !CheckingEndingOrUnlockables( 64, false, false ) )
			return;
	}

	if ( pszName && !strcmp( pszName, "weapon_axe" ) )
	{
		if ( !CheckingEndingOrUnlockables( 524288, false, false ) )
			return;
	}

	if ( pszName && !strcmp( pszName, "weapon_tmp" ) )
	{
		if ( !bSkipDonatorCheck_guess && !IsDonator() && !m_iAllowedToUseTMP )
			return;
	}

	if ( pszName && !strcmp( pszName, "weapon_mp5" ) )
	{
		if ( !IsDeveloper() && m_iNew16a_guess != 2 && m_iNew16a_guess != 3 && m_iNew16a_guess != 4 && g_pGameRules && !g_pGameRules->IsMapperMode_guess() )
			return;
	}

	edict_t	*pent;

	int istr = MAKE_STRING(pszName);

	pent = CREATE_NAMED_ENTITY(istr);
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_debug, "NULL Ent in GiveNamedItem!\n" );
		return;
	}
	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	pent->v.iuser4 = 1;

	if ( gNoWeaponDrop )
	{
		pent->v.iuser3 = 1;
		gNoWeaponDrop = false;
	}

	if ( m_bSilentGive && strstr( pszName, "weapon_" ) )
		pent->v.max_health = 1;

	if ( deletewhendropped && strstr( pszName, "weapon_" ) )
		pent->v.max_health = 2;

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );
}

void CBasePlayer::GiveNamedItemAmmo( const char *pszName, int defaultammo )
{
	edict_t	*pent;

	int istr = MAKE_STRING(pszName);

	pent = CREATE_NAMED_ENTITY(istr);
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_debug, "NULL Ent in GiveNamedItem!\n" );
		return;
	}
	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	if ( strstr( STRING( pent->v.classname ), "weapon_" ) )
	{
		if ( defaultammo >= 0 )
			pent->v.frags = defaultammo;
	}

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );
}



CBaseEntity *FindEntityForward( CBaseEntity *pMe )
{
	TraceResult tr;

	UTIL_MakeVectors(pMe->pev->v_angle);
	UTIL_TraceLine(pMe->pev->origin + pMe->pev->view_ofs,pMe->pev->origin + pMe->pev->view_ofs + gpGlobals->v_forward * 8192,dont_ignore_monsters, pMe->edict(), &tr );
	if ( tr.flFraction != 1.0 && !FNullEnt( tr.pHit) )
	{
		CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
		return pHit;
	}
	return NULL;
}


BOOL CBasePlayer :: FlashlightIsOn( void )
{
#ifdef XENWARRIOR
	return FBitSet(pev->effects, EF_BRIGHTLIGHT);
#else
//	return FBitSet(pev->effects, EF_DIMLIGHT);
#endif

	return FALSE;
}


void CBasePlayer :: FlashlightTurnOn( void )
{
	if ( g_pGameRules && !g_pGameRules->FAllowFlashlight() )
	{
		return;
	}

#ifdef XENWARRIOR
	if (g_fEnvFadeTime > gpGlobals->time)
	{
		return;
	}
#endif

	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
	WRITE_BYTE(1);
	WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = gpGlobals->time + FLASH_DRAIN_TIME;
}


void CBasePlayer :: FlashlightTurnOff( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgFlashlight, NULL, pev );
		WRITE_BYTE(0);
		WRITE_BYTE(m_iFlashBattery);
	MESSAGE_END();

	m_flFlashLightTime = gpGlobals->time + FLASH_CHARGE_TIME;
}

void CBasePlayer::GameStart( void )
{
	if ( szCustomCampaign )
	{
		char szCommand[256];

		sprintf( szCommand, "map %s\n", szCustomCampaign );
		szCustomCampaign = NULL;
		CLIENT_COMMAND( edict(), szCommand );
	}
	else
		CLIENT_COMMAND( edict(), "map c_intro\n" );
}

int CBasePlayer::SufficientlyOffTheGround( void )
{
	if ( m_bJustJumped )
		return TRUE;

	return FALSE;
}

void CBasePlayer::DemoStart( void )
{
	switch ( demolevel )
	{
	case 1:
		CLIENT_COMMAND( edict(), "map c_intro\n" );
		break;
	case 2:
		CLIENT_COMMAND( edict(), "map c_city\n" );
		break;
	case 3:
		CLIENT_COMMAND( edict(), "map c_subwaysick1\n" );
		break;
	case 4:
		CLIENT_COMMAND( edict(), "map c_forest3\n" );
		break;
	default:
		CLIENT_COMMAND( edict(), "map c_start\n" );
		break;
	}
}


void CBasePlayer :: SendInitMessages( void )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity *pEntity;

	if ( !pEdict )
		return;

	CBaseEntity *pSettingsEntity = NULL;
	CBaseEntity *pDifficultyEntity = NULL;
	CBaseEntity *pLoadEntity = NULL;
	CBaseEntity *pMenuEntity = NULL;
	CBaseEntity *pUnlockEntity = NULL;

	CBaseEntity *pSurvivalEntity = NULL;

	while ( ( pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" ) ) != NULL )
	{
		if ( pSurvivalEntity->pev->iuser2 == 0 )
			pSurvivalEntity->Use( this, this, USE_TOGGLE, 0 );

		for ( int k = 0; k < 32; k++ )
			m_bFirstDeploy[k] = true;
	}

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		CBaseEntity *pBossBar = NULL;

		while ( ( pBossBar = UTIL_FindEntityByClassname( pBossBar, "cof_bosshealthbar" ) ) != NULL )
		{
			if ( pBossBar->pev->iuser1 )
				pBossBar->Use( this, this, USE_TOGGLE, 666 );
		}
	}

	CBaseEntity *pCoFGiver = NULL;

	while ( ( pCoFGiver = UTIL_FindEntityByClassname( pCoFGiver, "lol_giver" ) ) != NULL )
		pCoFGiver->Use( this, this, USE_TOGGLE, 0 );

	while ( ( pSettingsEntity = UTIL_FindEntityByClassname( pSettingsEntity, "cof_serversettings" ) ) != NULL )
	{
		ShowVGUIMenu( 33 );
		m_iHideHUD |= HIDEHUD_HEALTH;
		EnableControl( FALSE );
	}

	if ( strstr( STRING( gpGlobals->mapname ), "game_menu" ) || !strcmp( STRING( gpGlobals->mapname ), "c_unlockables" ) )
	{
		int unlockflags = 0;

		if ( CheckingEndingOrUnlockables( 1, false, false ) )
			unlockflags |= 1;

		if ( CheckingEndingOrUnlockables( 2, false, false ) )
			unlockflags |= 2;

		if ( CheckingEndingOrUnlockables( 4, false, false ) )
			unlockflags |= 4;

		if ( CheckingEndingOrUnlockables( 8, false, false ) )
			unlockflags |= 8;

		if ( CheckingEndingOrUnlockables( 0x10, false, false ) )
			unlockflags |= 0x10;

		if ( CheckingEndingOrUnlockables( 0x20, false, false ) )
			unlockflags |= 0x20;

		if ( CheckingEndingOrUnlockables( 0x40, false, false ) )
			unlockflags |= 0x40;

		if ( CheckingEndingOrUnlockables( 0x80, false, false ) )
			unlockflags |= 0x80;

		if ( CheckingEndingOrUnlockables( 0x100, false, false ) )
			unlockflags |= 0x100;

		if ( CheckingEndingOrUnlockables( 0x200, false, false ) )
			unlockflags |= 0x200;

		if ( CheckingEndingOrUnlockables( 0x400, false, false ) )
			unlockflags |= 0x400;

		if ( CheckingEndingOrUnlockables( 0x800, false, false ) )
			unlockflags |= 0x800;

		if ( CheckingEndingOrUnlockables( 0x1000, false, false ) )
			unlockflags |= 0x1000;

		if ( CheckingEndingOrUnlockables( 0x2000, false, false ) )
			unlockflags |= 0x2000;

		if ( CheckingEndingOrUnlockables( 0x4000, false, false ) )
			unlockflags |= 0x4000;

		if ( CheckingEndingOrUnlockables( 0x8000, false, false ) )
			unlockflags |= 0x8000;

		if ( CheckingEndingOrUnlockables( 0x10000, false, false ) )
			unlockflags |= 0x10000;

		if ( CheckingEndingOrUnlockables( 0x20000, false, false ) )
			unlockflags |= 0x20000;

		if ( CheckingEndingOrUnlockables( 0x40000, false, false ) )
			unlockflags |= 0x40000;

		if ( CheckingEndingOrUnlockables( 0x80000, false, false ) )
			unlockflags |= 0x80000;

		if ( CheckingEndingOrUnlockables( 0x100000, false, false ) )
			unlockflags |= 0x100000;

		if ( CheckingEndingOrUnlockables( 0x200000, false, false ) )
			unlockflags |= 0x200000;

		if ( CheckingEndingOrUnlockables( 0x400000, false, false ) )
			unlockflags |= 0x400000;

		if ( CheckingEndingOrUnlockables( 0x800000, false, false ) )
			unlockflags |= 0x800000;

		if ( CheckingEndingOrUnlockables( 0x1000000, false, false ) )
			unlockflags |= 0x1000000;

		if ( CheckingEndingOrUnlockables( 0x2000000, false, false ) )
			unlockflags |= 0x2000000;

		if ( CheckingEndingOrUnlockables( 0x4000000, false, false ) )
			unlockflags |= 0x4000000;

		if ( CheckingEndingOrUnlockables( 0x8000000, false, false ) )
			unlockflags |= 0x8000000;

		if ( CheckingEndingOrUnlockables( 0x10000000, false, false ) )
			unlockflags |= 0x10000000;

		if ( CheckingEndingOrUnlockables( 0x20000000, false, false ) )
			unlockflags |= 0x20000000;

		if ( CheckingEndingOrUnlockables( 0x40000000, false, false ) )
			unlockflags |= 0x40000000;

		if ( CheckingEndingOrUnlockables( 0x80000000, false, false ) )
			unlockflags |= 0x80000000;

		int ending6 = 0;
		int ending7 = 0;

		if ( CheckingEndingOrUnlockables( 0, true, false ) )
			ending6 = 1;

		if ( CheckingEndingOrUnlockables( 0, false, true ) )
			ending7 = 1;

		MESSAGE_BEGIN( MSG_ONE, gmsgUnlocks, NULL, pev );
			WRITE_LONG( unlockflags );
			WRITE_BYTE( ending6 );
			WRITE_BYTE( ending7 );
		MESSAGE_END();
	}

	while ( ( pUnlockEntity = UTIL_FindEntityByClassname( pUnlockEntity, "cof_unlockables" ) ) != NULL )
	{
		ShowVGUIMenu( 188 );
		m_iHideHUD |= HIDEHUD_HEALTH;
		EnableControl( FALSE );
	}

	while ( ( pDifficultyEntity = UTIL_FindEntityByClassname( pDifficultyEntity, "cof_difficultysettings" ) ) != NULL )
	{
		int unlockflags = 0;

		if ( CheckingEndingOrUnlockables( 1, false, false ) )
			unlockflags |= 1;

		if ( CheckingEndingOrUnlockables( 2, false, false ) )
			unlockflags |= 2;

		if ( CheckingEndingOrUnlockables( 4, false, false ) )
			unlockflags |= 4;

		if ( CheckingEndingOrUnlockables( 8, false, false ) )
			unlockflags |= 8;

		if ( CheckingEndingOrUnlockables( 0x10, false, false ) )
			unlockflags |= 0x10;

		int ending6 = 0;
		int ending7 = 0;

		if ( CheckingEndingOrUnlockables( 0, true, false ) )
			ending6 = 1;

		if ( CheckingEndingOrUnlockables( 0, false, true ) )
			ending7 = 1;

		if ( CheckingEndingOrUnlockables( 0x20, false, false ) )
			unlockflags |= 0x20;

		if ( CheckingEndingOrUnlockables( 0x40, false, false ) )
			unlockflags |= 0x40;

		if ( CheckingEndingOrUnlockables( 0x80, false, false ) )
			unlockflags |= 0x80;

		if ( CheckingEndingOrUnlockables( 0x100, false, false ) )
			unlockflags |= 0x100;

		if ( CheckingEndingOrUnlockables( 0x200, false, false ) )
			unlockflags |= 0x200;

		if ( CheckingEndingOrUnlockables( 0x400, false, false ) )
			unlockflags |= 0x400;

		if ( CheckingEndingOrUnlockables( 0x800, false, false ) )
			unlockflags |= 0x800;

		if ( CheckingEndingOrUnlockables( 0x1000, false, false ) )
			unlockflags |= 0x1000;

		if ( CheckingEndingOrUnlockables( 0x2000, false, false ) )
			unlockflags |= 0x2000;

		if ( CheckingEndingOrUnlockables( 0x4000, false, false ) )
			unlockflags |= 0x4000;

		if ( CheckingEndingOrUnlockables( 0x8000, false, false ) )
			unlockflags |= 0x8000;

		if ( CheckingEndingOrUnlockables( 0x10000, false, false ) )
			unlockflags |= 0x10000;

		if ( CheckingEndingOrUnlockables( 0x20000, false, false ) )
			unlockflags |= 0x20000;

		if ( CheckingEndingOrUnlockables( 0x40000, false, false ) )
			unlockflags |= 0x40000;

		if ( CheckingEndingOrUnlockables( 0x80000, false, false ) )
			unlockflags |= 0x80000;

		if ( CheckingEndingOrUnlockables( 0x100000, false, false ) )
			unlockflags |= 0x100000;

		if ( CheckingEndingOrUnlockables( 0x200000, false, false ) )
			unlockflags |= 0x200000;

		if ( CheckingEndingOrUnlockables( 0x400000, false, false ) )
			unlockflags |= 0x400000;

		if ( CheckingEndingOrUnlockables( 0x800000, false, false ) )
			unlockflags |= 0x800000;

		if ( CheckingEndingOrUnlockables( 0x1000000, false, false ) )
			unlockflags |= 0x1000000;

		if ( CheckingEndingOrUnlockables( 0x2000000, false, false ) )
			unlockflags |= 0x2000000;

		if ( CheckingEndingOrUnlockables( 0x4000000, false, false ) )
			unlockflags |= 0x4000000;

		if ( CheckingEndingOrUnlockables( 0x8000000, false, false ) )
			unlockflags |= 0x8000000;

		if ( CheckingEndingOrUnlockables( 0x10000000, false, false ) )
			unlockflags |= 0x10000000;

		if ( CheckingEndingOrUnlockables( 0x20000000, false, false ) )
			unlockflags |= 0x20000000;

		if ( CheckingEndingOrUnlockables( 0x40000000, false, false ) )
			unlockflags |= 0x40000000;

		if ( CheckingEndingOrUnlockables( 0x80000000, false, false ) )
			unlockflags |= 0x80000000;

		MESSAGE_BEGIN( MSG_ONE, gmsgUnlocks, NULL, pev );
			WRITE_LONG( unlockflags );
			WRITE_BYTE( ending6 );
			WRITE_BYTE( ending7 );
		MESSAGE_END();

		ShowVGUIMenu( 32 );
		m_iHideHUD |= HIDEHUD_HEALTH;
		EnableControl( FALSE );
	}

	while ( ( pLoadEntity = UTIL_FindEntityByClassname( pLoadEntity, "cof_loadgame" ) ) != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSaveLoad, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_STRING( "nothing" );
		MESSAGE_END();
	}

	while ( ( pMenuEntity = UTIL_FindEntityByClassname( pMenuEntity, "cof_gamemenu" ) ) != NULL )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgGameMenu, NULL, pev );
		MESSAGE_END();
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;

		pEntity = CBaseEntity::Instance(pEdict);
		if ( !pEntity )
			continue;

		pEntity->SendInitMessage( this );
	}

	if ( g_iDoorSound )
	{
		SetNextThink( 0.5 );
		SetThink( &CBasePlayer::PlayNewDoorSound );
		CVAR_SET_FLOAT( "v_dark", 1 );
	}
	else
	{
		SetThink( NULL );
		SetNextThink( 0 );
	}

	UTIL_ProMessage( this, " ", 2, false );
}


/*
===============
ForceClientDllUpdate

When recording a demo, we need to have the server tell us the entire client state
so that the client side .dll can behave correctly.
Reset stuff so that the state is transmitted.
===============
*/
void CBasePlayer :: ForceClientDllUpdate( void )
{
	m_sClientDualWieldViewModel = MAKE_STRING( "update" );
	m_iClientHealth  = -1;
	m_iClientUnlockedClothes = -1;
	m_iClientBattery = -1;
	m_iClientDualWeaponLeftAmmo = -2;
	m_iClientDualWeaponRightAmmo = -2;
	m_iTrain |= TRAIN_NEW;  // Force new train message.
	m_fWeapon = FALSE;          // Force weapon send
	m_fKnownItem = FALSE;    // Force weaponinit messages.
	m_fInitHUD = TRUE;		// Force HUD gmsgResetHUD message
	m_iClientPhoneDisabled = -1;

	m_iClientFlashlightFlags = -1;

	// Now force all the necessary messages
	//  to be sent.
	UpdateClientData();
}

/*
============
ImpulseCommands
============
*/
void CBasePlayer::ImpulseCommands( )
{
	if ( !strcmp( STRING( gpGlobals->mapname ), "c_server_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_difficulty_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_loadgame" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_video_stress_test" ) )
		return;

	if ( m_iUsingComputerOrPhone )
		return;

	TraceResult	tr;// UNDONE: kill me! This is temporary for PreAlpha CDs

	// Handle use events
	PlayerUse();

	int iImpulse = pev->impulse;
	if (iImpulse==101) { pev->impulse=0;CoFR_Impulse101(this);return; }
	switch (iImpulse)
	{
	case 99:
		{

		int iOn;

		if (!gmsgLogo)
		{
			iOn = 1;
			gmsgLogo = REG_USER_MSG("Logo", 1);
		}
		else
		{
			iOn = 0;
		}

		ASSERT( gmsgLogo > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgLogo, NULL, pev );
			WRITE_BYTE(iOn);
		MESSAGE_END();

		if(!iOn)
			gmsgLogo = 0;
		break;
		}
	case 100:
		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine ( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), & tr);

		if ( tr.pHit && tr.pHit->v.takedamage )
		{
			ALERT( at_console, "+++ Classname is %s\n", STRING( pev->classname ) );
			ALERT( at_console, "+++ Model is %s\n", STRING( pev->model ) );

			if ( pev->flags & FL_MONSTER )
				ALERT( at_console, "+++ Has monster flag\n" );
		}
		break;

	case	201:// paint decal

		if ( gpGlobals->time < m_flNextDecalTime )
		{
			// too early!
			break;
		}

		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine ( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), & tr);

		if ( tr.flFraction != 1.0 )
		{// line hit something, so paint a decal
			m_flNextDecalTime = gpGlobals->time + decalfrequency.value;
			CSprayCan *pCan = GetClassPtr((CSprayCan *)NULL);
			pCan->Spawn( pev );
		}

		break;

	case    204:  //  Demo recording, update client dll specific data again.
		ForceClientDllUpdate();
		break;
	default:
		// check all of the cheat impulse commands now
		CheatImpulseCommands( iImpulse );
		break;
	}

	pev->impulse = 0;
}

//=========================================================
//=========================================================
void CBasePlayer::CheatImpulseCommands( int iImpulse )
{
	if ( !strcmp( STRING( gpGlobals->mapname ), "c_server_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_difficulty_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_loadgame" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_video_stress_test" ) )
		return;

	if ( m_iUsingComputerOrPhone )
		return;

#if !defined( HLDEMO_BUILD )
	CBaseEntity *pEntity;
	TraceResult tr;

	switch ( iImpulse )
	{
	case 76:
		{
			if (!giPrecacheGrunt)
			{
				giPrecacheGrunt = 1;
				ALERT(at_debug, "You must now restart to use Grunt-o-matic.\n");
			}
			else
			{
				UTIL_MakeVectors( Vector( 0, pev->v_angle.y, 0 ) );
				Create("monster_human_grunt", pev->origin + gpGlobals->v_forward * 128, pev->angles);
			}
			break;
		}

	case 90: //LRC - send USE_TOGGLE
		{
			char *impulsetarget = (char *)CVAR_GET_STRING( "sohl_impulsetarget" );
			if (impulsetarget)
				FireTargets(impulsetarget, this, this, USE_TOGGLE, 0);
			break;
		}
	case 91: //LRC - send USE_ON
		{
			char *impulsetarget = (char *)CVAR_GET_STRING( "sohl_impulsetarget" );
			if (impulsetarget)
				FireTargets(impulsetarget, this, this, USE_ON, 0);
			break;
		}
	case 92: //LRC - send USE_OFF
		{
			char *impulsetarget = (char *)CVAR_GET_STRING( "sohl_impulsetarget" );
			if (impulsetarget)
				FireTargets(impulsetarget, this, this, USE_OFF, 0);
			break;
		}

	case 103:
		// What the hell are you doing?
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			CBaseMonster *pMonster = pEntity->MyMonsterPointer();
			if ( pMonster )
				pMonster->ReportAIState();
		}
		break;

	case 104:
		// Dump all of the global state varaibles (and global entity names)
		gGlobalState.DumpGlobals();
		break;

	case	105:// player makes no sound for monsters to hear.
		{
			if ( m_fNoPlayerSound )
			{
				ALERT ( at_debug, "Player is audible\n" );
				m_fNoPlayerSound = FALSE;
			}
			else
			{
				ALERT ( at_debug, "Player is silent\n" );
				m_fNoPlayerSound = TRUE;
			}
			break;
		}

	case 106:
		// Give me the classname and targetname of this entity.
		pEntity = FindEntityForward( this );
		if ( pEntity )
		{
			ALERT ( at_debug, "Classname: %s", STRING( pEntity->pev->classname ) );

			if ( !FStringNull ( pEntity->pev->targetname ) )
			{
				ALERT ( at_debug, " - Targetname: %s\n", STRING( pEntity->pev->targetname ) );
			}
			else
			{
				ALERT ( at_debug, " - TargetName: No Targetname\n" );
			}

			ALERT ( at_debug, "Model: %s\n", STRING( pEntity->pev->model ) );
			if ( pEntity->pev->globalname )
				ALERT ( at_debug, "Globalname: %s\n", STRING( pEntity->pev->globalname ) );
			ALERT(at_debug, "State: %s\n", GetStringForState( pEntity->GetState() )); //LRC
		}
		break;

	case 107:
		{
			TraceResult tr;

			edict_t		*pWorld = g_engfuncs.pfnPEntityOfEntIndex( 0 );

			Vector start = pev->origin + pev->view_ofs;
			Vector end = start + gpGlobals->v_forward * 1024;
			UTIL_TraceLine( start, end, ignore_monsters, edict(), &tr );
			if ( tr.pHit )
				pWorld = tr.pHit;
			const char *pTextureName = TRACE_TEXTURE( pWorld, start, end );
			if ( pTextureName )
				ALERT( at_debug, "Texture: %s\n", pTextureName );
		}
		break;
	case	195:// show shortest paths for entire level to nearest node
		{
			Create("node_viewer_fly", pev->origin, pev->angles);
		}
		break;
	case	196:// show shortest paths for entire level to nearest node
		{
			Create("node_viewer_large", pev->origin, pev->angles);
		}
		break;
	case	197:// show shortest paths for entire level to nearest node
		{
			Create("node_viewer_human", pev->origin, pev->angles);
		}
		break;
	case	199:// show nearest node and all connections
		{
			ALERT ( at_debug, "%d\n", WorldGraph.FindNearestNode ( pev->origin, bits_NODE_GROUP_REALM ) );
			WorldGraph.ShowNodeConnections ( WorldGraph.FindNearestNode ( pev->origin, bits_NODE_GROUP_REALM ) );
		}
		break;
	case	202:// Random blood splatter
		UTIL_MakeVectors(pev->v_angle);
		UTIL_TraceLine ( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 128, ignore_monsters, ENT(pev), & tr);

		if ( tr.flFraction != 1.0 )
		{// line hit something, so paint a decal
			CBloodSplat *pBlood = GetClassPtr((CBloodSplat *)NULL);
			pBlood->Spawn( pev );
		}
		break;
	}
#endif	// HLDEMO_BUILD
}

//
// Add a weapon to the player (Item == Weapon == Selectable Object)
//
int CBasePlayer::AddPlayerItem( CBasePlayerItem *pItem )
{
	CBasePlayerItem *pInsert;

	pInsert = m_rgpPlayerItems[pItem->iItemSlot()];

	while (pInsert)
	{
		if (FClassnameIs( pInsert->pev, STRING( pItem->pev->classname) ))
		{
			if (pItem->AddDuplicate( pInsert, this ))
			{
				g_pGameRules->PlayerGotWeapon ( this, pItem );
				pItem->CheckRespawn();

				// ugly hack to update clip w/o an update clip message
				pInsert->UpdateItemInfo( );
				if (m_pActiveItem)
					m_pActiveItem->UpdateItemInfo( );

				pItem->Kill( );
			}
			else if (gEvilImpulse101 || gSpecialItemGive)
			{
				// FIXME: remove anyway for deathmatch testing
				pItem->Kill( );
			}
			return FALSE;
		}
		pInsert = pInsert->m_pNext;
	}


	if (pItem->AddToPlayer( this ))
	{
		g_pGameRules->PlayerGotWeapon ( this, pItem );
		pItem->CheckRespawn();

		pItem->m_pNext = m_rgpPlayerItems[pItem->iItemSlot()];
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem;

		// should we switch to this item?
		if ( g_pGameRules && g_pGameRules->FShouldSwitchWeapon( this, pItem ) )
		{
			SwitchWeapon( pItem );
		}

		return TRUE;
	}
	else if (gEvilImpulse101 || gSpecialItemGive)
	{
		// FIXME: remove anyway for deathmatch testing
		pItem->Kill( );
	}
	return FALSE;
}



int CBasePlayer::RemovePlayerItem( CBasePlayerItem *pItem )
{
	if (m_pActiveItem == pItem)
	{
		ResetAutoaim( );
		pItem->Holster( );
		pItem->DontThink();// crowbar may be trying to swing again, etc.
		pItem->SetThink( NULL );
		m_pActiveItem = NULL;
		pev->viewmodel = 0;
		pev->weaponmodel = 0;
	}
	else if ( m_pLastItem == pItem )
		m_pLastItem = NULL;

	CBasePlayerItem *pPrev = m_rgpPlayerItems[pItem->iItemSlot()];

	if (pPrev == pItem)
	{
		m_rgpPlayerItems[pItem->iItemSlot()] = pItem->m_pNext;
		return TRUE;
	}
	else
	{
		while (pPrev && pPrev->m_pNext != pItem)
		{
			pPrev = pPrev->m_pNext;
		}
		if (pPrev)
		{
			pPrev->m_pNext = pItem->m_pNext;
			return TRUE;
		}
	}
	return FALSE;
}


//
// Returns the unique ID for the ammo, or -1 if error
//
int CBasePlayer :: GiveAmmo( int iCount, char *szName, int iMax, bool bNoMessage_guess )
{
	if ( !szName )
	{
		// no ammo.
		return -1;
	}

	if ( g_pGameRules && !g_pGameRules->CanHaveAmmo( this, szName, iMax ) )
	{
		// game rules say I can't have any more of this ammo type.
		if ( ( pev->button | m_afButtonPressed | m_afButtonReleased ) & IN_USE )
		{
			char szMessage[128];

			if ( szName == "syringe" )
				strcpy( szMessage, "I can't carry any more syringes" );
			else if ( szName == "flare" )
				strcpy( szMessage, "I can't carry any more flares" );
			else
				sprintf( szMessage, "I can't carry any more %s ammo", szName );

			UTIL_ProMessage( this, szMessage, 2, false );
		}

		return -1;
	}

	int i = 0;

	i = GetAmmoIndex( szName );

	if ( i < 0 || i >= MAX_AMMO_SLOTS )
		return -1;

	int iAdd = min( iCount, iMax - m_rgAmmo[i] );
	if ( iAdd < 1 )
		return i;

	if ( !bNoMessage_guess )
	{
		char szMessage[128];

		if ( szName == "buckshot" )
			sprintf( szMessage, "Picked up a box of %i shells", iAdd );
		else if ( szName == "m16" )
			sprintf( szMessage, "Picked up an %s magazine with %i rounds", szName, iAdd );
		else if ( szName == "syringe" )
			strcpy( szMessage, "Picked up a syringe" );
		else if ( szName == "flares" )
			strcpy( szMessage, "Picked up a flare" );
		else if ( szName == "revolver" )
			sprintf( szMessage, "Picked up a box of %i revolver rounds", iAdd );
		else if ( szName == "rifle" )
			sprintf( szMessage, "Picked up a box of %i rifle rounds", iAdd );
		else
			sprintf( szMessage, "Picked up a %s magazine with %i rounds", szName, iAdd );

		if ( szName != "flashlight" )
			UTIL_ProMessage( this, szMessage, 2, false );
	}

	m_rgAmmo[ i ] += iAdd;


	if ( gmsgAmmoPickup )  // make sure the ammo messages have been linked first
	{
		// Send the message that ammo has been picked up
		MESSAGE_BEGIN( MSG_ONE, gmsgAmmoPickup, NULL, pev );
			WRITE_BYTE( GetAmmoIndex(szName) );		// ammo ID
			WRITE_BYTE( iAdd );		// amount
			WRITE_STRING( szName ); // buz: send also ammo name to display in hud..
		MESSAGE_END();
	}

	MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
		WRITE_BYTE( i );
		WRITE_SHORT( m_rgAmmo[i] );
	MESSAGE_END();

	TabulateAmmo();

	CBasePlayerItem *pWeapon;

	for ( int j = 0; j < MAX_ITEM_TYPES; j++ )
	{
		pWeapon = m_rgpPlayerItems[j];

		while ( pWeapon )
		{
			if ( pWeapon->pszAmmo1() && !strcmp( pWeapon->pszAmmo1(), szName ) )
			{
				if ( gmsgWpnInfo )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pev );
						WRITE_BYTE( pWeapon->m_iId );
						WRITE_BYTE( pWeapon->iMaxClip() );
						WRITE_SHORT( m_rgAmmo[ GetClassPtr( (CBasePlayerWeapon *)pWeapon->pev )->m_iPrimaryAmmoType ] );
					MESSAGE_END();
				}
			}

			pWeapon = pWeapon->m_pNext;
		}
	}

	return i;
}


/*
============
ItemPreFrame

Called every frame by the player PreThink
============
*/
void CBasePlayer::ItemPreFrame()
{
#if defined( CLIENT_WEAPONS )
    if ( m_flNextAttack > 0 )
#else
    if ( gpGlobals->time < m_flNextAttack )
#endif
	{
		return;
	}

	if (!m_pActiveItem)
		return;

	m_pActiveItem->ItemPreFrame( );
}


/*
============
ItemPostFrame

Called every frame by the player PostThink
============
*/
void CBasePlayer::ItemPostFrame()
{
	static int fInSelect = FALSE;

	if ( m_pCamera )
		m_pCamera->Think();

	// check if the player is using a tank
	if ( m_pTank != NULL )
		return;

	// buz: spec tank
	if ( m_pSpecTank )
		return;

#if defined( CLIENT_WEAPONS )
    if ( m_flNextAttack > 0 )
#else
    if ( gpGlobals->time < m_flNextAttack )
#endif
	{
		return;
	}

	ImpulseCommands();

	if (!m_pActiveItem)
		return;

	if ( m_pActiveItem && !m_pActiveItem->m_pPlayer )
		m_pActiveItem->m_pPlayer = this;

	if ( m_pActiveItem->m_pPlayer )
		m_pActiveItem->ItemPostFrame( );
}

int CBasePlayer::AmmoInventory( int iAmmoIndex )
{
	if (iAmmoIndex == -1)
	{
		return -1;
	}

	return m_rgAmmo[ iAmmoIndex ];
}

int CBasePlayer::GetAmmoIndex(const char *psz)
{
	int i;

	if (!psz)
		return -1;

	for (i = 1; i < MAX_AMMO_SLOTS; i++)
	{
		if ( !CBasePlayerItem::AmmoInfoArray[i].pszName )
			continue;

		if (stricmp( psz, CBasePlayerItem::AmmoInfoArray[i].pszName ) == 0)
			return i;
	}

	return -1;
}

// Called from UpdateClientData
// makes sure the client has all the necessary ammo info,  if values have changed
void CBasePlayer::SendAmmoUpdate(void)
{
	for (int i=0; i < MAX_AMMO_SLOTS;i++)
	{
		if (m_rgAmmo[i] != m_rgAmmoLast[i])
		{
			m_rgAmmoLast[i] = m_rgAmmo[i];

			if ( m_rgAmmo[i] < 0 )
				m_rgAmmo[i] = 0;

			// send "Ammo" update message
			MESSAGE_BEGIN( MSG_ONE, gmsgAmmoX, NULL, pev );
				WRITE_BYTE( i );
				WRITE_SHORT( m_rgAmmo[i] );
			MESSAGE_END();
		}
	}
}

/*
=========================================================
	UpdateClientData

resends any changed player HUD info to the client.
Called every frame by Player::PreThink
Also called at start of demo recording and playback by
ForceClientDllUpdate to ensure the demo gets messages
reflecting all of the HUD state info.
=========================================================
*/
void CBasePlayer :: UpdateClientData( void )
{
	if (m_fInitHUD)
	{
		m_fInitHUD = FALSE;
		gInitHUD = FALSE;

		CBaseEntity *pSurvivalEntity = NULL;

		pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" );

		CCoFSurvivalMode *pSurviveEnt = NULL;

		if ( pSurvivalEntity )
			pSurviveEnt = GetClassPtr( (CCoFSurvivalMode *)pSurvivalEntity->pev );

		if ( pSurvivalEntity && pSurviveEnt )
		{
			if ( pSurviveEnt->pev->iuser2 > 0 && pSurviveEnt->m_bSurvivalModeRunning == true )
			{
				if ( g_iSurvivalMode )
				{
					for ( int k = 0; k < 32; k++ )
						m_bFirstDeploy[k] = true;
				}

				int seconds = pSurviveEnt->m_iTimeSeconds;
				int minutes = pSurviveEnt->m_iTimeMinutes;
				int hours = pSurviveEnt->m_iTimeHours;
				int desired = pSurviveEnt->m_iSurvivalTimeInSeconds;

				MESSAGE_BEGIN( MSG_ONE, gmsgTimer, NULL, pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( seconds );
					WRITE_BYTE( minutes );
					WRITE_BYTE( hours );
					WRITE_SHORT( desired );
				MESSAGE_END();

				if ( m_iSimonMode )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgTimerPlus );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 3 );
					MESSAGE_END();
				}
			}
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgInventChck, NULL, pev );
		MESSAGE_END();

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr )
				{
					if ( plr->IsDeveloper() )
						plr->pev->vuser1.y = 1;
					else if ( plr->IsDonator() )
						plr->pev->vuser1.y = 2;
					else
						plr->pev->vuser1.y = 0;

					MESSAGE_BEGIN( MSG_ALL, gmsgDevDonator );
						WRITE_BYTE( ENTINDEX( plr->edict() ) );
						WRITE_BYTE( plr->pev->vuser1.y );
					MESSAGE_END();

					if ( plr->HasNamedPlayerItem( "weapon_syringe" ) )
					{
						MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
							WRITE_BYTE( ENTINDEX( plr->edict() ) );
							WRITE_BYTE( 1 );
						MESSAGE_END();
					}
					else
					{
						MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
							WRITE_BYTE( ENTINDEX( plr->edict() ) );
							WRITE_BYTE( 0 );
						MESSAGE_END();
					}
				}
			}
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgResetHUD, NULL, pev );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgBlur, NULL, pev );
			WRITE_BYTE( m_iBlurActive );
		MESSAGE_END();

		MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, pev );
			WRITE_BYTE( m_iHealthCap );
		MESSAGE_END();

		if ( !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:1:7832479" ) || !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:1:33405825" ) || !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:1:39527250" ) )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgFaggot, NULL, pev );
				WRITE_BYTE( 1 );
			MESSAGE_END();

			m_iIsAFaggot = 1;
			EnableControl( FALSE );
		}
		else
		{
			m_iIsAFaggot = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgFaggot, NULL, pev );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		if ( !m_pAttachedCamera )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pev );
				WRITE_SHORT( -1 );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pev );
				WRITE_SHORT( ENTINDEX( m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}

		if ( !m_pBoat )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, pev );
				WRITE_SHORT( -1 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBoat, NULL, pev );
				WRITE_SHORT( ENTINDEX( m_pBoat->edict() ) );
			MESSAGE_END();
		}

		for ( int i = 1; i <= 16; i++ )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgAddNote, NULL, pev );
				WRITE_BYTE( i );
				WRITE_BYTE( 0 );
				WRITE_SHORT( 0 );
				WRITE_BYTE( 0 );
				WRITE_STRING( documentsstruct[i].note1 );
				WRITE_STRING( documentsstruct[i].note2 );
				WRITE_STRING( documentsstruct[i].note3 );
				WRITE_STRING( documentsstruct[i].note4 );
				WRITE_STRING( documentsstruct[i].page1 );
				WRITE_STRING( documentsstruct[i].page2 );
				WRITE_STRING( documentsstruct[i].page3 );
				WRITE_STRING( documentsstruct[i].page4 );
				WRITE_STRING( documentsstruct[i].turnsound );
				WRITE_STRING( documentsstruct[i].title );
				WRITE_STRING( documentsstruct[i].label );
			MESSAGE_END();
		}

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			bool incutscene = false;

			if ( m_hCurrentCamera && m_hCurrentCamera->pev->iuser1 )
			{
				incutscene = true;
				SET_VIEW( edict(), m_hCurrentCamera->edict() );

				pev->flags |= FL_NOTARGET;
				pev->takedamage = DAMAGE_NO;

				if ( m_hCurrentCamera->pev->spawnflags & 16 )
					m_iHideHUD |= HIDEHUD_HEALTH;

				if ( m_hCurrentCamera->pev->spawnflags & 32 )
					m_DrawBars = 1;

				if ( m_hCurrentCamera->pev->iuser2 )
					m_iHideHUD |= HIDEHUD_CUSTOMCROSSHAIR;

				if ( m_hCurrentCamera->pev->spawnflags & 256 )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
						WRITE_BYTE( 1 );
						WRITE_BYTE( 1 );
						WRITE_STRING( "" );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}
			}

			for ( int playerloop = 1; playerloop <= gpGlobals->maxClients; playerloop++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( playerloop );

				if ( plr )
				{
					if ( plr->m_hCurrentCamera && plr->m_hCurrentCamera->pev->iuser1 )
					{
						incutscene = true;
						m_hCurrentCamera = plr->m_hCurrentCamera;
						SET_VIEW( edict(), plr->m_hCurrentCamera->edict() );

						plr->pev->flags |= FL_NOTARGET;
						plr->pev->takedamage = DAMAGE_NO;

						if ( plr->m_hCurrentCamera->pev->spawnflags & 16 )
							m_iHideHUD |= HIDEHUD_HEALTH;

						if ( plr->m_hCurrentCamera->pev->spawnflags & 32 )
							m_DrawBars = 1;

						if ( plr->m_hCurrentCamera->pev->iuser2 )
							m_iHideHUD |= HIDEHUD_CUSTOMCROSSHAIR;

						if ( plr->m_hCurrentCamera->pev->spawnflags & 256 )
						{
							MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
								WRITE_BYTE( 1 );
								WRITE_BYTE( 1 );
								WRITE_STRING( "" );
								WRITE_BYTE( 0 );
							MESSAGE_END();
						}

						break;
					}
				}
			}

			if ( !incutscene )
				SET_VIEW( edict(), edict() );
		}

		if ( m_bThreeSlots )
		{
			for ( int i = 1; i <= 8; i++ )
			{
				InventoryMessage( i, cofrInventorySlot[i], 0 );
			}
		}
		else
		{
			for ( int i = 1; i <= 8; i++ )
			{
				InventoryMessage( i, cofrInventorySlot[i], 0 );
			}
		}

		for ( int qslots = 1; qslots <= 3; qslots++ )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
				WRITE_BYTE( qslots );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}

		if ( m_iQuickSlotSlot[1] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_BYTE( m_iQuickSlotSlot[1] );
			MESSAGE_END();
		}

		if ( m_iQuickSlotSlot[2] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 2 );
				WRITE_BYTE( m_iQuickSlotSlot[2] );
			MESSAGE_END();
		}

		if ( m_iQuickSlotSlot[3] )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotUpdate, NULL, pev );
				WRITE_BYTE( 3 );
				WRITE_BYTE( m_iQuickSlotSlot[3] );
			MESSAGE_END();
		}

		if ( !m_fGameHUDInitialized )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgInitHUD, NULL, pev );
			MESSAGE_END();

			g_pGameRules->InitHUD( this );
			m_fGameHUDInitialized = TRUE;
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				FireTargets( "game_playerjoin", this, this, USE_TOGGLE, 0 );
			}
		}

		if ( m_bBossHealthBar )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pev );
				WRITE_BYTE( 1 );
				WRITE_STRING( STRING( m_sBossBarEmpty ) );
				WRITE_STRING( STRING( m_sBossBarFull ) );
				WRITE_SHORT( m_iBossCurrentHealth );
				WRITE_SHORT( m_iBossMaxHealth );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBossBar, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_STRING( STRING( m_sBossBarEmpty ) );
				WRITE_STRING( STRING( m_sBossBarFull ) );
				WRITE_SHORT( m_iBossCurrentHealth );
				WRITE_SHORT( m_iBossMaxHealth );
			MESSAGE_END();
		}

		FireTargets( "game_playerspawn", this, this, USE_TOGGLE, 0 );

		InitStatusBar();

		if ( m_iBlackAndWhite )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBlackWhite, NULL, pev );
				WRITE_BYTE( m_iBlackAndWhite );
			MESSAGE_END();
		}

		if ( !strcmp( STRING( gpGlobals->mapname ), "c_video_stress_test" ) )
		{
			CBaseEntity *pStressTest = NULL;
			CBaseEntity *pStressCamera = NULL;

			pStressTest = UTIL_FindEntityByClassname( pStressTest, "cof_stresstest" );
			pStressCamera = UTIL_FindEntityByClassname( pStressCamera, "trigger_camera" );

			if ( pStressTest )
				pStressTest->Use( this, this, USE_TOGGLE, 0 );

			if ( pStressCamera )
				pStressCamera->Use( this, this, USE_TOGGLE, 0 );
		}
	}

	if ( m_iHideHUD != m_iClientHideHUD )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgHideWeapon, NULL, pev );
			WRITE_BYTE( m_iHideHUD );
		MESSAGE_END();

		m_iClientHideHUD = m_iHideHUD;
	}

	if ( m_iFOV != m_iClientFOV )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSetFOV, NULL, pev );
			WRITE_BYTE( m_iFOV );
		MESSAGE_END();

		// cache FOV change at end of function, so weapon updates can see that FOV has changed
	}

	// HACKHACK -- send the message to display the game title
	if (gDisplayTitle)
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, pev );
		WRITE_BYTE( 0 );
		MESSAGE_END();
		gDisplayTitle = 0;
	}

	if ( m_iClientPhoneDisabled != m_iPhoneDisabled )
	{
		m_iClientPhoneDisabled = m_iPhoneDisabled;

		MESSAGE_BEGIN( MSG_ONE, gmsgPhoneOff, NULL, pev );
			WRITE_BYTE( m_iPhoneDisabled );
		MESSAGE_END();
	}

	if ( m_sDualWieldViewModel != m_sClientDualWieldViewModel )
	{
		m_sClientDualWieldViewModel = m_sDualWieldViewModel;

		if ( !m_sDualWieldViewModel )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgDualWield, NULL, pev );
				WRITE_STRING( "models/null.mdl" );
				WRITE_BYTE( 0 );
				WRITE_BYTE( -1 );
				WRITE_BYTE( -1 );
				WRITE_SHORT( -1 );
			MESSAGE_END();
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgDualWield, NULL, pev );
				WRITE_STRING( STRING( m_sDualWieldViewModel ) );
				WRITE_BYTE( gDualWeapon.dualbody );
				WRITE_BYTE( m_iDualWeaponLeftID );
				WRITE_BYTE( m_iDualWeaponRightID );
                WRITE_SHORT(entindex()); // Client requires a valid owner anchor after the model/IDs.
			MESSAGE_END();
		}
	}

	if (pev->health != m_iClientHealth)
	{
		int iHealth = max( pev->health, 0 );  // make sure that no negative health values are sent

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHealth, NULL, plr->pev );
					WRITE_BYTE( iHealth );
					WRITE_BYTE( ENTINDEX( edict() ) );
					if ( g_pGameRules->IsMultiplayer() )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}

		m_iClientHealth = pev->health;
	}

	if ( m_iClientDualWeaponLeftAmmo != m_iDualWeaponLeftAmmo )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualAmmo, NULL, pev );
			WRITE_LONG( m_iDualWeaponLeftAmmo );
			WRITE_LONG( m_iDualWeaponRightAmmo );
		MESSAGE_END();

		m_iClientDualWeaponLeftAmmo = m_iDualWeaponLeftAmmo;
	}

	if ( m_iClientDualWeaponRightAmmo != m_iDualWeaponRightAmmo )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualAmmo, NULL, pev );
			WRITE_LONG( m_iDualWeaponLeftAmmo );
			WRITE_LONG( m_iDualWeaponRightAmmo );
		MESSAGE_END();

		m_iClientDualWeaponRightAmmo = m_iDualWeaponRightAmmo;
	}

	if (pev->armorvalue != m_iClientBattery)
	{
		m_iClientBattery = pev->armorvalue;

		ASSERT( gmsgBattery > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgBattery, NULL, pev );
			WRITE_SHORT( (int)pev->armorvalue);
		MESSAGE_END();
	}

	if (pev->dmg_take || pev->dmg_save || m_bitsHUDDamage != m_bitsDamageType)
	{
		// Comes from inside me if not set
		Vector damageOrigin = pev->origin;
		// send "damage" message
		// causes screen to flash, and pain compass to show direction of damage
		edict_t *other = pev->dmg_inflictor;
		if ( other )
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(other);
			if ( pEntity )
				damageOrigin = pEntity->Center();
		}

		// only send down damage type that have hud art
		int visibleDamageBits = m_bitsDamageType & DMG_SHOWNHUD;

		MESSAGE_BEGIN( MSG_ONE, gmsgDamage, NULL, pev );
			WRITE_BYTE( pev->dmg_save );
			WRITE_BYTE( pev->dmg_take );
			WRITE_LONG( visibleDamageBits );
			WRITE_COORD( damageOrigin.x );
			WRITE_COORD( damageOrigin.y );
			WRITE_COORD( damageOrigin.z );
			if ( !( m_bitsDamageType & DMG_FALL ) )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
		MESSAGE_END();

		pev->dmg_take = 0;
		pev->dmg_save = 0;
		m_bitsHUDDamage = m_bitsDamageType;

		// Clear off non-time-based damage indicators
		m_bitsDamageType &= DMG_TIMEBASED;
	}

	if ( m_iClientUnlockedClothes != m_iUnlockedClothes && gmsgClothes )
	{
		int flags = 0;
		int canseeclothes = 0;

		if ( CheckingEndingOrUnlockables( 128, false, false ) )
		{
			canseeclothes = 1;
			flags |= 1;
		}

		if ( CheckingEndingOrUnlockables( 256, false, false ) )
		{
			canseeclothes = 1;
			flags |= 2;
		}

		if ( CheckingEndingOrUnlockables( 512, false, false ) )
		{
			canseeclothes = 1;
			flags |= 4;
		}

		if ( CheckingEndingOrUnlockables( 1024, false, false ) )
		{
			canseeclothes = 1;
			flags |= 8;
		}

		if ( CheckingEndingOrUnlockables( 2048, false, false ) )
		{
			canseeclothes = 1;
			flags |= 16;
		}

		if ( CheckingEndingOrUnlockables( 4096, false, false ) )
		{
			canseeclothes = 1;
			flags |= 32;
		}

		if ( CheckingEndingOrUnlockables( 8192, false, false ) )
		{
			canseeclothes = 1;
			flags |= 64;
		}

		if ( CheckingEndingOrUnlockables( 16384, false, false ) )
		{
			canseeclothes = 1;
			flags |= 128;
		}

		if ( CheckingEndingOrUnlockables( 32768, false, false ) )
		{
			canseeclothes = 1;
			flags |= 256;
		}

		if ( CheckingEndingOrUnlockables( 65536, false, false ) )
		{
			canseeclothes = 1;
			flags |= 512;
		}

		if ( CheckingEndingOrUnlockables( 131072, false, false ) )
		{
			canseeclothes = 1;
			flags |= 1024;
		}

		if ( CheckingEndingOrUnlockables( 262144, false, false ) )
		{
			canseeclothes = 1;
			flags |= 2048;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgClothes, NULL, pev );
			WRITE_LONG( flags );
			WRITE_BYTE( canseeclothes );
			WRITE_BYTE( m_iUnlockedClothes );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_iClientUnlockedClothes = m_iUnlockedClothes;
	}

	// RAIN START
	// calculate and update rain fading
	if (Rain_endFade > 0)
	{
		if (gpGlobals->time < Rain_endFade)
		{ // we're in fading process
			if (Rain_nextFadeUpdate <= gpGlobals->time)
			{
				int secondsLeft = Rain_endFade - gpGlobals->time + 1;

				Rain_dripsPerSecond += (Rain_ideal_dripsPerSecond - Rain_dripsPerSecond) / secondsLeft;
				Rain_windX += (Rain_ideal_windX - Rain_windX) / (float)secondsLeft;
				Rain_windY += (Rain_ideal_windY - Rain_windY) / (float)secondsLeft;
				Rain_randX += (Rain_ideal_randX - Rain_randX) / (float)secondsLeft;
				Rain_randY += (Rain_ideal_randY - Rain_randY) / (float)secondsLeft;

				Rain_nextFadeUpdate = gpGlobals->time + 1; // update once per second
				Rain_needsUpdate = 1;

				ALERT(at_aiconsole, "Rain fading: curdrips: %i, idealdrips %i\n", Rain_dripsPerSecond, Rain_ideal_dripsPerSecond);
			}
		}
		else
		{ // finish fading process
			Rain_nextFadeUpdate = 0;
			Rain_endFade = 0;

			Rain_dripsPerSecond = Rain_ideal_dripsPerSecond;
			Rain_windX = Rain_ideal_windX;
			Rain_windY = Rain_ideal_windY;
			Rain_randX = Rain_ideal_randX;
			Rain_randY = Rain_ideal_randY;
			Rain_needsUpdate = 1;

			ALERT(at_aiconsole, "Rain fading finished at %i drips\n", Rain_dripsPerSecond);
		}
	}


	// send rain message
	if (Rain_needsUpdate)
	{
	//search for rain_settings entity
		edict_t *pFind;
		pFind = FIND_ENTITY_BY_CLASSNAME( NULL, "rain_settings" );
		if (!FNullEnt( pFind ))
		{
		// rain allowed on this map
			CBaseEntity *pEnt = CBaseEntity::Instance( pFind );
			CRainSettings *pRainSettings = (CRainSettings *)pEnt;

			float raindistance = pRainSettings->Rain_Distance;
			float rainheight = pRainSettings->pev->origin[2];
			int rainmode = pRainSettings->Rain_Mode;

			// search for constant rain_modifies
			pFind = FIND_ENTITY_BY_CLASSNAME( NULL, "rain_modify" );
			while ( !FNullEnt( pFind ) )
			{
				if (pFind->v.spawnflags & 1)
				{
					// copy settings to player's data and clear fading
					CBaseEntity *pEnt = CBaseEntity::Instance( pFind );
					CRainModify *pRainModify = (CRainModify *)pEnt;

					Rain_dripsPerSecond = pRainModify->Rain_Drips;
					Rain_windX = pRainModify->Rain_windX;
					Rain_windY = pRainModify->Rain_windY;
					Rain_randX = pRainModify->Rain_randX;
					Rain_randY = pRainModify->Rain_randY;

					Rain_endFade = 0;
					break;
				}
				pFind = FIND_ENTITY_BY_CLASSNAME( pFind, "rain_modify" );
			}

			MESSAGE_BEGIN(MSG_ONE, gmsgRainData, NULL, pev);
				WRITE_SHORT(Rain_dripsPerSecond);
				WRITE_COORD(raindistance);
				WRITE_COORD(Rain_windX);
				WRITE_COORD(Rain_windY);
				WRITE_COORD(Rain_randX);
				WRITE_COORD(Rain_randY);
				WRITE_SHORT(rainmode);
				WRITE_COORD(rainheight);
			MESSAGE_END();

			if (Rain_dripsPerSecond)
				ALERT(at_aiconsole, "Sending enabling rain message\n");
			else
				ALERT(at_aiconsole, "Sending disabling rain message\n");
		}
		else
		{ // no rain on this map
			Rain_dripsPerSecond = 0;
			Rain_windX = 0;
			Rain_windY = 0;
			Rain_randX = 0;
			Rain_randY = 0;
			Rain_ideal_dripsPerSecond = 0;
			Rain_ideal_windX = 0;
			Rain_ideal_windY = 0;
			Rain_ideal_randX = 0;
			Rain_ideal_randY = 0;
			Rain_endFade = 0;
			Rain_nextFadeUpdate = 0;

			ALERT(at_aiconsole, "Clearing rain data\n");
		}

		Rain_needsUpdate = 0;
	}
	// RAIN END


	// Update Flashlight
	// buz: infinite flashlight
/*	if ((m_flFlashLightTime) && (m_flFlashLightTime <= gpGlobals->time))
	{
		if (FlashlightIsOn())
		{
			if (m_iFlashBattery)
			{
				m_flFlashLightTime = FLASH_DRAIN_TIME + gpGlobals->time;
				m_iFlashBattery--;

				if (!m_iFlashBattery)
					FlashlightTurnOff();
			}
		}
		else
		{
			if (m_iFlashBattery < 100)
			{
				m_flFlashLightTime = FLASH_CHARGE_TIME + gpGlobals->time;
				m_iFlashBattery++;
			}
			else
				m_flFlashLightTime = 0;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgFlashBattery, NULL, pev );
		WRITE_BYTE(m_iFlashBattery);
		MESSAGE_END();
	}*/


	// buz: update goal window
	if (m_iGoalNeedsUpdate)
	{
		if (FStringNull(m_strCurrentGoalName))
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgTabPanel, NULL, pev );
				WRITE_BYTE(0); // turn off
			MESSAGE_END();
			ALERT(at_aiconsole, "Player goal window cleared\n");
		}
		else
		{
			int flags = 1;
			if (!FStringNull(m_strCurrentGoalTitleName))
				flags |= TP_FL_TITLE;

			if (!FStringNull(m_strCurrentGoalImageName))
				flags |= TP_FL_IMAGE;

			if (m_iGoalNeedsUpdate != 2)
				flags |= TP_FL_POPUP;

			MESSAGE_BEGIN( MSG_ONE, gmsgTabPanel, NULL, pev );
				WRITE_BYTE(flags); // show
				WRITE_STRING(STRING(m_strCurrentGoalName));

				if (flags & TP_FL_TITLE)
					WRITE_STRING(STRING(m_strCurrentGoalTitleName));

				if (flags & TP_FL_IMAGE)
					WRITE_STRING(STRING(m_strCurrentGoalImageName));
			MESSAGE_END();
			ALERT(at_aiconsole, "Player goal description set\n");
		}

		m_iGoalNeedsUpdate = 0;
	}

	// buz: send particle systems
	if (!m_iInitMessagesSent)
	{
		SendInitMessages();
		m_iInitMessagesSent = 1;
	}

	if (m_iTrain & TRAIN_NEW)
	{
		ASSERT( gmsgTrain > 0 );
		// send "health" update message
		MESSAGE_BEGIN( MSG_ONE, gmsgTrain, NULL, pev );
			WRITE_BYTE(m_iTrain & 0xF);
		MESSAGE_END();

		m_iTrain &= ~TRAIN_NEW;
	}

	//
	// New Weapon?
	//
	if (!m_fKnownItem)
	{
		m_fKnownItem = TRUE;

	// WeaponInit Message
	// byte  = # of weapons
	//
	// for each weapon:
	// byte		name str length (not including null)
	// bytes... name
	// byte		Ammo Type
	// byte		Ammo2 Type
	// byte		bucket
	// byte		bucket pos
	// byte		flags
	// ????		Icons

		// Send ALL the weapon info now
		int i;

		for (i = 0; i < MAX_WEAPONS; i++)
		{
			ItemInfo& II = CBasePlayerItem::ItemInfoArray[i];

			if ( !II.iId )
				continue;

			const char *pszName;
			if (!II.pszName)
				pszName = "Empty";
			else
				pszName = II.pszName;

			MESSAGE_BEGIN( MSG_ONE, gmsgWeaponList, NULL, pev );
				WRITE_STRING(pszName);			// string	weapon name
				WRITE_BYTE(GetAmmoIndex(II.pszAmmo1));	// byte		Ammo Type
				WRITE_BYTE(II.iMaxAmmo1);				// byte     Max Ammo 1
				WRITE_BYTE(GetAmmoIndex(II.pszAmmo2));	// byte		Ammo2 Type
				WRITE_BYTE(II.iMaxAmmo2);				// byte     Max Ammo 2
				WRITE_BYTE(II.iSlot);					// byte		bucket
				WRITE_BYTE(II.iPosition);				// byte		bucket pos
				WRITE_BYTE(II.iId);						// byte		id (bit index into pev->weapons)
				WRITE_BYTE(II.iFlags);					// byte		Flags
			MESSAGE_END();
		}
	}


	SendAmmoUpdate();

	// buz: update spec machinegun state
	if (m_pSpecTank)
	{
		m_pSpecTank->Use( this, this, USE_SET, 3 ); // update client data
	}

	// Update all the items
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )  // each item updates it's successors
			m_rgpPlayerItems[i]->UpdateClientData( this );
	}

	// buz: fix bug with crosshairs: send weapon update message if
	// no one item didnt sent it before
	if ( !m_fWeapon )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

	//	m_iClientClip = m_iClip;
	//	m_iClientWeaponState = state;
		m_fWeapon = TRUE;
	}

	// Cache and client weapon change
	m_pClientActiveItem = m_pActiveItem;
	m_iClientFOV = m_iFOV;

	// Update Status Bar
	if ( m_flNextSBarUpdateTime < gpGlobals->time )
	{
	//	UpdateStatusBar();
		m_flNextSBarUpdateTime = gpGlobals->time + 0.2;
	}
}

void CBasePlayer::PlayerSetMP3( int strindex, int loop, float fadetime )
{
	m_strCurrentMp3 = strindex;

	if ( loop )
		m_bMp3ShouldLoop = true;
	else
		m_bMp3ShouldLoop = false;

	m_fMp3FadeInTime = fadetime;

	m_iMp3NeedsUpdate = 1;
}

void CBasePlayer::PlayerStopMP3( float fadetime )
{
	m_strCurrentMp3 = 0;
	m_bMp3ShouldLoop = false;
	m_fMp3FadeTime = fadetime;
	m_iMp3NeedsUpdate = 1;
}

//=========================================================
// buz: PlayerSetGoalDesc - sets goal description for this client
//=========================================================
void CBasePlayer::PlayerSetGoalDesc( string_t strindex, string_t title, string_t imgname )
{
	m_strCurrentGoalName = strindex;
	m_strCurrentGoalTitleName = title;
	m_strCurrentGoalImageName = imgname;
	m_iGoalNeedsUpdate = 1;
}

//=========================================================
// FBecomeProne - Overridden for the player to set the proper
// physics flags when a barnacle grabs player.
//=========================================================
BOOL CBasePlayer :: FBecomeProne ( void )
{
	m_afPhysicsFlags |= PFLAG_ONBARNACLE;
	return TRUE;
}

//=========================================================
// BarnacleVictimBitten - bad name for a function that is called
// by Barnacle victims when the barnacle pulls their head
// into its mouth. For the player, just die.
//=========================================================
void CBasePlayer :: BarnacleVictimBitten ( entvars_t *pevBarnacle )
{
	TakeDamage ( pevBarnacle, pevBarnacle, pev->health + pev->armorvalue, DMG_SLASH | DMG_ALWAYSGIB );
}

//=========================================================
// BarnacleVictimReleased - overridden for player who has
// physics flags concerns.
//=========================================================
void CBasePlayer :: BarnacleVictimReleased ( void )
{
	m_afPhysicsFlags &= ~PFLAG_ONBARNACLE;
}


//=========================================================
// Illumination
// return player light level plus virtual muzzle flash
//=========================================================
int CBasePlayer :: Illumination( void )
{
	int iIllum = CBaseEntity::Illumination( );

	iIllum += m_iWeaponFlash;
	if (iIllum > 255)
		return 255;
	return iIllum;
}


void CBasePlayer :: EnableControl(BOOL fControl)
{
	if (!fControl)
	{
		pev->flags |= FL_FROZEN;
		pev->velocity = g_vecZero; //LRC - stop view bobbing
	}
	else
		pev->flags &= ~FL_FROZEN;

	if ( m_bShouldFallFreeze )
	{
		m_bShouldFallFreeze = false;
		m_fFallingTime = 0;
	}
}

void CBasePlayer :: FallEnableControl( int fControl )
{
	if (!fControl)
	{
		pev->flags |= FL_FROZEN;
		pev->velocity = g_vecZero; //LRC - stop view bobbing
	}
	else
		pev->flags &= ~FL_FROZEN;
}


#define DOT_1DEGREE   0.9998476951564
#define DOT_2DEGREE   0.9993908270191
#define DOT_3DEGREE   0.9986295347546
#define DOT_4DEGREE   0.9975640502598
#define DOT_5DEGREE   0.9961946980917
#define DOT_6DEGREE   0.9945218953683
#define DOT_7DEGREE   0.9925461516413
#define DOT_8DEGREE   0.9902680687416
#define DOT_9DEGREE   0.9876883405951
#define DOT_10DEGREE  0.9848077530122
#define DOT_15DEGREE  0.9659258262891
#define DOT_20DEGREE  0.9396926207859
#define DOT_25DEGREE  0.9063077870367

//=========================================================
// Autoaim
// set crosshair position to point to enemey
//=========================================================
Vector CBasePlayer :: GetAutoaimVector( float flDelta )
{
	return gpGlobals->v_forward;
/*
{
	if (g_iSkillLevel == SKILL_HARD)
	{
		UTIL_MakeVectors( pev->v_angle + pev->punchangle );
		return gpGlobals->v_forward;
	}

	Vector vecSrc = GetGunPosition( );
	float flDist = 8192;

	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (1 || g_iSkillLevel == SKILL_MEDIUM)
	{
		m_vecAutoAim = Vector( 0, 0, 0 );
		// flDelta *= 0.5;
	}

	BOOL m_fOldTargeting = m_fOnTarget;
	Vector angles = AutoaimDeflection(vecSrc, flDist, flDelta );

	// update ontarget if changed
	if ( !g_pGameRules->AllowAutoTargetCrosshair() )
		m_fOnTarget = 0;
	else if (m_fOldTargeting != m_fOnTarget)
	{
		m_pActiveItem->UpdateItemInfo( );
	}

	if (angles.x > 180)
		angles.x -= 360;
	if (angles.x < -180)
		angles.x += 360;
	if (angles.y > 180)
		angles.y -= 360;
	if (angles.y < -180)
		angles.y += 360;

	if (angles.x > 25)
		angles.x = 25;
	if (angles.x < -25)
		angles.x = -25;
	if (angles.y > 12)
		angles.y = 12;
	if (angles.y < -12)
		angles.y = -12;


	// always use non-sticky autoaim
	// UNDONE: use sever variable to chose!
	if (0 || g_iSkillLevel == SKILL_EASY)
	{
		m_vecAutoAim = m_vecAutoAim * 0.67 + angles * 0.33;
	}
	else
	{
		m_vecAutoAim = angles * 0.9;
	}

	// m_vecAutoAim = m_vecAutoAim * 0.99;

	// Don't send across network if sv_aim is 0
	if ( g_psv_aim->value != 0 )
	{
		if ( m_vecAutoAim.x != m_lastx ||
			 m_vecAutoAim.y != m_lasty )
		{
			SET_CROSSHAIRANGLE( edict(), -m_vecAutoAim.x, m_vecAutoAim.y );
			
			m_lastx = m_vecAutoAim.x;
			m_lasty = m_vecAutoAim.y;
		}
	}

	// ALERT( at_console, "%f %f\n", angles.x, angles.y );

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );
	return gpGlobals->v_forward;
}
*/
}


Vector CBasePlayer :: AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  )
{
	edict_t		*pEdict = g_engfuncs.pfnPEntityOfEntIndex( 1 );
	CBaseEntity	*pEntity;
	float		bestdot;
	Vector		bestdir;
	edict_t		*bestent;
	TraceResult tr;

	if ( g_psv_aim->value == 0 )
	{
		m_fOnTarget = FALSE;
		return g_vecZero;
	}

	UTIL_MakeVectors( pev->v_angle + pev->punchangle + m_vecAutoAim );

	// try all possible entities
	bestdir = gpGlobals->v_forward;
	bestdot = flDelta; // +- 10 degrees
	bestent = NULL;

	m_fOnTarget = FALSE;

	UTIL_TraceLine( vecSrc, vecSrc + bestdir * flDist, dont_ignore_monsters, edict(), &tr );


	if ( tr.pHit && tr.pHit->v.takedamage != DAMAGE_NO)
	{
		// don't look through water
		if (!((pev->waterlevel != 3 && tr.pHit->v.waterlevel == 3) 
			|| (pev->waterlevel == 3 && tr.pHit->v.waterlevel == 0)))
		{
			if (tr.pHit->v.takedamage == DAMAGE_AIM)
				m_fOnTarget = TRUE;

			return m_vecAutoAim;
		}
	}

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		Vector center;
		Vector dir;
		float dot;

		if ( pEdict->free )	// Not in use
			continue;
		
		if (pEdict->v.takedamage != DAMAGE_AIM)
			continue;
		if (pEdict == edict())
			continue;
//		if (pev->team > 0 && pEdict->v.team == pev->team)
//			continue;	// don't aim at teammate
		if ( g_pGameRules && !g_pGameRules->ShouldAutoAim( this, pEdict ) )
			continue;

		pEntity = Instance( pEdict );
		if (pEntity == NULL)
			continue;

		if (!pEntity->IsAlive())
			continue;

		// don't look through water
		if ((pev->waterlevel != 3 && pEntity->pev->waterlevel == 3) 
			|| (pev->waterlevel == 3 && pEntity->pev->waterlevel == 0))
			continue;

		center = pEntity->BodyTarget( vecSrc );

		dir = (center - vecSrc).Normalize( );

		// make sure it's in front of the player
		if (DotProduct (dir, gpGlobals->v_forward ) < 0)
			continue;

		dot = fabs( DotProduct (dir, gpGlobals->v_right ) ) 
			+ fabs( DotProduct (dir, gpGlobals->v_up ) ) * 0.5;

		// tweek for distance
		dot *= 1.0 + 0.2 * ((center - vecSrc).Length() / flDist);

		if (dot > bestdot)
			continue;	// to far to turn

		UTIL_TraceLine( vecSrc, center, dont_ignore_monsters, edict(), &tr );
		if (tr.flFraction != 1.0 && tr.pHit != pEdict)
		{
			// ALERT( at_console, "hit %s, can't see %s\n", STRING( tr.pHit->v.classname ), STRING( pEdict->v.classname ) );
			continue;
		}

		// don't shoot at friends
		if (IRelationship( pEntity ) < 0)
		{
			if ( !pEntity->IsPlayer() && g_pGameRules && !g_pGameRules->IsDeathmatch())
				// ALERT( at_console, "friend\n");
				continue;
		}

		// can shoot at this one
		bestdot = dot;
		bestent = pEdict;
		bestdir = dir;
	}

	if (bestent)
	{
		bestdir = UTIL_VecToAngles (bestdir);
		bestdir.x = -bestdir.x;
		bestdir = bestdir - pev->v_angle - pev->punchangle;

		if (bestent->v.takedamage == DAMAGE_AIM)
			m_fOnTarget = TRUE;

		return bestdir;
	}

	return Vector( 0, 0, 0 );
}

void CBasePlayer :: ResetAutoaim( )
{
	if (m_vecAutoAim.x != 0 || m_vecAutoAim.y != 0)
	{
		m_vecAutoAim = Vector( 0, 0, 0 );
		SET_CROSSHAIRANGLE( edict(), 0, 0 );
	}
	m_fOnTarget = FALSE;
}

/*
=============
SetCustomDecalFrames

  UNDONE:  Determine real frame limit, 8 is a placeholder.
  Note:  -1 means no custom frames present.
=============
*/
void CBasePlayer :: SetCustomDecalFrames( int nFrames )
{
	if (nFrames > 0 &&
		nFrames < 8)
		m_nCustomSprayFrames = nFrames;
	else
		m_nCustomSprayFrames = -1;
}

/*
=============
GetCustomDecalFrames

  Returns the # of custom frames this player's custom clan logo contains.
=============
*/
int CBasePlayer :: GetCustomDecalFrames( void )
{
	return m_nCustomSprayFrames;
}


//=========================================================
// DropPlayerItem - drop the named item, or if no name,
// the active item.
//=========================================================
void CBasePlayer::DropPlayerItem ( char *pszItemName, int inventoryslot )
{
	bool activeitem = false;

	if ( !strlen( pszItemName ) )
	{
		// if this string has no length, the client didn't type a name!
		// assume player wants to drop the active item.
		// make the string null to make future operations in this function easier
		pszItemName = NULL;
	}

	CBasePlayerItem *pWeapon;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			if ( pszItemName )
			{
				// try to match by name.
				if ( !strcmp( pszItemName, STRING( pWeapon->pev->classname ) ) )
				{
					// match!
					if ( pWeapon == m_pActiveItem )
						activeitem = true;

					break;
				}
			}
			else
			{
				// trying to drop active item
				if ( pWeapon == m_pActiveItem )
				{
					// active item!
					activeitem = true;

					break;
				}
			}

			pWeapon = pWeapon->m_pNext;
		}

		// if we land here with a valid pWeapon pointer, that's because we found the
		// item we want to drop and hit a BREAK;  pWeapon is the item.
		if ( pWeapon )
		{
			if ( ((CBasePlayerWeapon *)pWeapon)->m_bIronSights )
				pWeapon->IronsightsFrom( );

			pWeapon->SetThink( NULL );

			if ( pWeapon->pev->iuser3 )
			{
				if ( activeitem )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}

				pev->weapons &= ~(1<<pWeapon->m_iId);// take item off hud
				HasItemInSlot( (char *)STRING( pWeapon->script ), true );

				RemovePlayerItem( pWeapon );
				return;
			}

			if ( pWeapon->pev->classname )
			{
				if ( pWeapon->iMaxClip() != WEAPON_NOCLIP || !pWeapon->pszAmmo1() )
				{
					if ( pWeapon->m_pPlayer )
					{
						UTIL_MakeVectors ( pev->v_angle );

						pev->weapons &= ~(1<<pWeapon->m_iId);// take item off hud

						if ( pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) )
						{
							pWeapon->pev->spawnflags |= SF_NORESPAWN;
							pWeapon->pev->movetype = MOVETYPE_NONE;
							pWeapon->pev->solid = SOLID_NOT;
							pWeapon->pev->effects = EF_NODRAW;
							pWeapon->pev->modelindex = 0;
							pWeapon->pev->model = iStringNull;
							pWeapon->pev->owner = edict();
							pWeapon->SetThink( NULL );
							pWeapon->SetTouch( NULL );
							pWeapon->m_pPlayer = NULL;
							pWeapon->slotnumber = 0;
						}
					}
					else
					{
						pWeapon->pev->spawnflags |= SF_NORESPAWN;
						pWeapon->pev->movetype = MOVETYPE_NONE;
						pWeapon->pev->solid = SOLID_NOT;
						pWeapon->pev->effects = EF_NODRAW;
						pWeapon->pev->modelindex = 0;
						pWeapon->pev->model = iStringNull;
						pWeapon->pev->owner = edict();
						pWeapon->SetThink( NULL );
						pWeapon->SetTouch( NULL );
						pWeapon->m_pPlayer = NULL;
						pWeapon->slotnumber = 0;
					}
				}

				if ( pWeapon )
				{
					if ( pWeapon->pev->max_health == 2 )
						UTIL_Remove( pWeapon );

					if ( pWeapon->iMaxClip() != WEAPON_NOCLIP || !pWeapon->pszAmmo1() )
					{
						pWeapon->pev->origin = pev->origin;

						pWeapon->pev->movetype = MOVETYPE_NONE;
						pWeapon->pev->solid = SOLID_TRIGGER;
						pWeapon->pev->aiment = NULL;
						pWeapon->pev->effects &= ~EF_NODRAW;
						pWeapon->pev->owner = NULL;

						pWeapon->Spawn( );

						DROP_TO_FLOOR( ENT( pWeapon->pev ) );

						if ( inventoryslot )
							ClearOutSlot( inventoryslot, 0 );

						if ( activeitem )
						{
							MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
								WRITE_BYTE( 0 );
								WRITE_BYTE( 0 );
								WRITE_BYTE( 0 );
							MESSAGE_END();
						}

						pWeapon->pev->armorvalue = 666;
					}
					else
					{
					if ( FClassnameIs( pWeapon->edict(), "weapon_syringe" ) )
					{
						m_rgAmmo[ GetClassPtr( (CSyringe *)pWeapon->pev )->m_iPrimaryAmmoType ]--;

						if ( m_rgAmmo[ GetClassPtr( (CSyringe *)pWeapon->pev )->m_iPrimaryAmmoType ] < 0 )
							m_rgAmmo[ GetClassPtr( (CSyringe *)pWeapon->pev )->m_iPrimaryAmmoType ] = 0;

						if ( gmsgWpnInfo )
						{
							MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pev );
								WRITE_BYTE( pWeapon->m_iId );
								WRITE_BYTE( pWeapon->iMaxClip() );
								WRITE_SHORT( m_rgAmmo[ GetClassPtr( (CSyringe *)pWeapon->pev )->m_iPrimaryAmmoType ] );
							MESSAGE_END();

							pWeapon->old_m_iAmmo = m_rgAmmo[ GetClassPtr( (CBasePlayerWeapon *)pWeapon->pev )->m_iPrimaryAmmoType ];
						}

						if ( m_rgAmmo[ GetClassPtr( (CSyringe *)pWeapon->pev )->m_iPrimaryAmmoType ] < 1 )
						{
							GetClassPtr( (CSyringe *)pWeapon->pev )->DestroyItem();

							if ( inventoryslot )
								ClearOutSlot( inventoryslot, 0 );

							GetClassPtr( (CSyringe *)pWeapon->pev )->b_HasUsed = true;

							if ( activeitem )
							{
								MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
									WRITE_BYTE( 0 );
									WRITE_BYTE( 0 );
									WRITE_BYTE( 0 );
								MESSAGE_END();
							}
						}

						CBaseEntity *pEntity = CBaseEntity::Create( "weapon_syringe", pev->origin, pev->angles, edict() );

						if ( pEntity )
						{
							DROP_TO_FLOOR( ENT( pEntity->pev ) );

							pEntity->pev->armorvalue = 666;
						}
					}

					if ( FClassnameIs( pWeapon->edict(), "weapon_flare" ) )
					{
						if ( 0 )
						{
							// never runs: the old code addressed the flare through its pev
							((CFlare *)pWeapon->pev)->SendWeaponAnim( 7, 1, 0 );
							((CFlare *)pWeapon->pev)->SetNextThink( 0.9 );
							((CFlare *)pWeapon->pev)->SetThink( &CFlare::ThrowRemove );
							((CFlare *)pWeapon->pev)->m_flNextPrimaryAttack = ((CFlare *)pWeapon->pev)->m_flNextSecondaryAttack = ((CFlare *)pWeapon->pev)->m_flTimeWeaponIdle = gpGlobals->time + 1.5;

							m_iFlashlightFlags &= ~16;
						}
						else
						{
							m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ]--;

							if ( m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ] < 0 )
								m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ] = 0;

							if ( gmsgWpnInfo )
							{
								MESSAGE_BEGIN( MSG_ONE, gmsgWpnInfo, NULL, pev );
									WRITE_BYTE( pWeapon->m_iId );
									WRITE_BYTE( pWeapon->iMaxClip() );
									WRITE_SHORT( m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ] );
								MESSAGE_END();

								pWeapon->old_m_iAmmo = m_rgAmmo[ GetClassPtr( (CBasePlayerWeapon *)pWeapon->pev )->m_iPrimaryAmmoType ];
							}

							if ( m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ] <= 0 )
							{
								pev->weapons &= ~(1<<pWeapon->m_iId);// take item off hud
								HasItemInSlot( (char *)STRING( pWeapon->script ), true );

								RemovePlayerItem( pWeapon );

								if ( activeitem )
								{
									MESSAGE_BEGIN( MSG_ONE, gmsgCurWeapon, NULL, pev );
										WRITE_BYTE( 0 );
										WRITE_BYTE( 0 );
										WRITE_BYTE( 0 );
									MESSAGE_END();
								}

								pWeapon->Kill( );

								if ( inventoryslot )
									ClearOutSlot( inventoryslot, 0 );
							}

							CBaseEntity *pEntity = CBaseEntity::Create( "weapon_flare", pev->origin, pev->angles, edict() );

							if ( pEntity )
							{
								DROP_TO_FLOOR( ENT( pEntity->pev ) );

								pEntity->pev->armorvalue = 666;

								if ( m_rgAmmo[ GetClassPtr( (CFlare *)pWeapon->pev )->m_iPrimaryAmmoType ] <= 0 && pWeapon->pev->iuser1 > 0 )
								{
									pEntity->pev->iuser1 = pWeapon->pev->iuser1;

									if ( pEntity->pev->iuser1 == 1 )
										pEntity->pev->body = 1;
									else
										pEntity->pev->body = 0;

									pEntity->pev->teleport_time = gpGlobals->time;
									pEntity->pev->owner = NULL;
									pEntity->Spawn( );

									pEntity->pev->radsuit_finished = pWeapon->pev->radsuit_finished;
								}
							}
						}
					}
					}
				}
			}

			return;
		}
	}
}

//=========================================================
// HasPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasPlayerItem( CBasePlayerItem *pCheckItem )
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
// HasNamedPlayerItem Does the player already have this item?
//=========================================================
BOOL CBasePlayer::HasNamedPlayerItem( const char *pszItemName )
{
	CBasePlayerItem *pItem;
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pItem = m_rgpPlayerItems[ i ];

		while (pItem)
		{
			if ( !strcmp( pszItemName, STRING( pItem->pev->classname ) ) )
			{
				return TRUE;
			}
			pItem = pItem->m_pNext;
		}
	}

	return FALSE;
}

//=========================================================
//
//=========================================================
BOOL CBasePlayer :: SwitchWeapon( CBasePlayerItem *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		return FALSE;
	}

	ResetAutoaim( );

	if (m_pActiveItem)
	{
		m_pActiveItem->Holster( );
	}

	m_bWeaponWaitingToDeploy = true;

	m_pNextItem = pWeapon;

	return TRUE;
}


//=========================================================
// Dead HEV suit prop
// LRC- i.e. the dead blokes you see in Xen.
//=========================================================
class CDeadHEV : public CBaseMonster
{
public:
	void Spawn( void );
	int	Classify ( void ) { return	CLASS_HUMAN_MILITARY; }

	void KeyValue( KeyValueData *pkvd );

	int	m_iPose;// which sequence to display	-- temporary, don't need to save
	static char *m_szPoses[4];
};

char *CDeadHEV::m_szPoses[] = { "deadback", "deadsitting", "deadstomach", "deadtable" };

void CDeadHEV::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "pose"))
	{
		m_iPose = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else 
		CBaseMonster::KeyValue( pkvd );
}

LINK_ENTITY_TO_CLASS( monster_hevsuit_dead, CDeadHEV );

//=========================================================
// ********** DeadHEV SPAWN **********
//=========================================================
void CDeadHEV :: Spawn( void )
{
	PRECACHE_MODEL("models/player.mdl");
	SET_MODEL(ENT(pev), "models/player.mdl");

	pev->effects		= 0;
	pev->yaw_speed		= 8;
	pev->sequence		= 0;
	pev->body			= 1;
	m_bloodColor		= BLOOD_COLOR_RED;

	pev->sequence = LookupSequence( m_szPoses[m_iPose] );

	if (pev->sequence == -1)
	{
		ALERT ( at_debug, "Dead hevsuit with bad pose\n" );
		pev->sequence = 0;
	}

	// Corpses have less health
	pev->health			= 8;

	MonsterInitDead();
}


class CStripWeapons : public CPointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

private:
//	int m_iAmmo[MAX_WEAPONS];
	int m_i9mm;
	int m_i357;
	int m_iBuck;
	int m_iBolt;
	int m_iARGren;
	int m_iRock;
	int m_iUranium;
	int m_iSatchel;
	int m_iSnark;
	int m_iTrip;
	int m_iGren;
	int m_iHornet;
// buz: paranoia ammos
	int m_iAps;
	int m_iBarret;
	int m_iAk;
	int m_iAsval;
	int m_iRpk;
	int m_iGroza;
	int m_iPK;
};

LINK_ENTITY_TO_CLASS( player_weaponstrip, CStripWeapons );

TYPEDESCRIPTION	CStripWeapons::m_SaveData[] = 
{
	DEFINE_FIELD( CStripWeapons, m_i9mm, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_i357, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iBuck, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iBolt, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iARGren, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iRock, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iUranium, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iSatchel, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iSnark, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iTrip, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iGren, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iHornet, FIELD_INTEGER ),
	// buz: paranoia ammos
	DEFINE_FIELD( CStripWeapons, m_iAps, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iBarret, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iAk, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iAsval, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iRpk, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iGroza, FIELD_INTEGER ),
	DEFINE_FIELD( CStripWeapons, m_iPK, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CStripWeapons, CPointEntity );

void CStripWeapons :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "bullets"))
	{
		m_i9mm= atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "magnum"))
	{
		m_i357 = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shotgun"))
	{
		m_iBuck = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "crossbow"))
	{
		m_iBolt = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "argrenades"))
	{
		m_iARGren = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rockets"))
	{
		m_iRock = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "uranium"))
	{
		m_iUranium = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "satchels"))
	{
		m_iSatchel = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "snarks"))
	{
		m_iSnark = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "tripmines"))
	{
		m_iTrip = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "handgrenades"))
	{
		m_iGren = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "hornetgun"))
	{
		m_iHornet = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
// buz: paranoia ammos
	else if (FStrEq(pkvd->szKeyName, "aps"))
	{
		m_iAps = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "barret"))
	{
		m_iBarret = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "ak"))
	{
		m_iAk = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "asval"))
	{
		m_iAsval = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "groza"))
	{
		m_iGroza = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "rpk"))
	{
		m_iRpk = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "painkillers"))
	{
		m_iPK = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CStripWeapons :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = NULL;

	if ( pActivator && pActivator->IsPlayer() )
	{
		pPlayer = (CBasePlayer *)pActivator;
	}
	else if ( g_pGameRules && !g_pGameRules->IsDeathmatch() )
	{
		pPlayer = (CBasePlayer *)CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
	}

	if ( pPlayer )
	{
		pPlayer->RemoveItems( pev->spawnflags, m_i9mm, m_i357, m_iBuck, m_iBolt,
				m_iARGren, m_iRock, m_iUranium, m_iSatchel, m_iSnark, m_iTrip, m_iGren, m_iHornet,
				m_iAps, m_iBarret, m_iAk, m_iAsval, m_iRpk, m_iGroza, m_iPK);
	}
}


class CRevertSaved : public CPointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT MessageThink( void );
	void	EXPORT LoadThink( void );
	void	KeyValue( KeyValueData *pkvd );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	HoldTime( void ) { return pev->dmg_save; }
	inline	float	MessageTime( void ) { return m_messageTime; }
	inline	float	LoadTime( void ) { return m_loadTime; }

	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetHoldTime( float hold ) { pev->dmg_save = hold; }
	inline	void	SetMessageTime( float time ) { m_messageTime = time; }
	inline	void	SetLoadTime( float time ) { m_loadTime = time; }

private:
	float	m_messageTime;
	float	m_loadTime;
};

LINK_ENTITY_TO_CLASS( player_loadsaved, CRevertSaved );

TYPEDESCRIPTION	CRevertSaved::m_SaveData[] = 
{
	DEFINE_FIELD( CRevertSaved, m_messageTime, FIELD_FLOAT ),	// These are not actual times, but durations, so save as floats
	DEFINE_FIELD( CRevertSaved, m_loadTime, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CRevertSaved, CPointEntity );

void CRevertSaved :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		SetHoldTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagetime"))
	{
		SetMessageTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "loadtime"))
	{
		SetLoadTime( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else 
		CPointEntity::KeyValue( pkvd );
}

void CRevertSaved :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	UTIL_ScreenFadeAll( pev->rendercolor, Duration(), HoldTime(), pev->renderamt, FFADE_OUT );
	SetNextThink( MessageTime() );
	SetThink(&CRevertSaved :: MessageThink );
}


void CRevertSaved :: MessageThink( void )
{
	UTIL_ShowMessageAll( STRING(pev->message) );
	float nextThink = LoadTime() - MessageTime();
	if ( nextThink > 0 ) 
	{
		SetNextThink( nextThink );
		SetThink(&CRevertSaved :: LoadThink );
	}
	else
		LoadThink();
}


void CRevertSaved :: LoadThink( void )
{
	if ( !gpGlobals->deathmatch )
	{
		SERVER_COMMAND("reload\n");
	}
}

//=========================================================
// Trigger to disable a player
//=========================================================
#define SF_FREEZE_LOCUS 1

class CPlayerFreeze:public CBaseDelay
{
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void Think( void );
	STATE GetState( void ) { return m_hActivator == NULL? STATE_OFF: STATE_ON; }
};

void CPlayerFreeze::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (!(pev->spawnflags & SF_FREEZE_LOCUS))
	{
		pActivator = UTIL_FindEntityByClassname(NULL, "player");
	}

	if (pActivator && pActivator->pev->flags & FL_CLIENT)
	{
		if (!ShouldToggle(useType, pActivator->pev->flags & FL_FROZEN))
			return;

		if (pActivator->pev->flags & FL_FROZEN)
		{
			// unfreeze him
			((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(TRUE);
			m_hActivator = NULL;
			DontThink();
		}
		else
		{
			// freeze him
			((CBasePlayer *)((CBaseEntity *)pActivator))->EnableControl(FALSE);
			if (m_flDelay)
			{
				m_hActivator = pActivator;
				SetNextThink(m_flDelay);
			}
		}
	}
}

void CPlayerFreeze::Think ( void )
{
	Use(m_hActivator, this, USE_ON, 0);
}

LINK_ENTITY_TO_CLASS( player_freeze, CPlayerFreeze );


//=========================================================
// Multiplayer intermission spots.
//=========================================================
class CInfoIntermission:public CPointEntity
{
	void Spawn( void );
	void Think( void );
};

void CInfoIntermission::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	SetNextThink( 2 );// let targets spawn!

}

void CInfoIntermission::Think ( void )
{
	CBaseEntity *pTarget;

	// find my target
	pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

	if ( pTarget )
	{
		pev->v_angle = UTIL_VecToAngles( (pTarget->pev->origin - pev->origin).Normalize() );
		pev->v_angle.x = -pev->v_angle.x;
	}
}

LINK_ENTITY_TO_CLASS( info_intermission, CInfoIntermission );


// buz: set jump height function
void CBasePlayer::SetJumpHeight(int value)
{
	m_iJumpHeight = value;
	char buf[16];
	itoa(m_iJumpHeight, buf, 10);
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "jh", buf );
//	ALERT(at_aiconsole, "SETTING JUMP: %s\n", buf);
}
	


//==============================================================
// Hud sprite displayer
//==============================================================
#define SF_HUDSPR_ACTIVE 1

class CHudSprite:public CBaseEntity
{
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE GetState( void ) { return pev->spawnflags & SF_HUDSPR_ACTIVE? STATE_ON:STATE_OFF; }
	void Think( void );
};

void CHudSprite::Spawn( void )
{
	if (FStringNull(pev->targetname))
	{
		pev->spawnflags |= SF_HUDSPR_ACTIVE;
	}

	if (pev->spawnflags & SF_HUDSPR_ACTIVE)
	{
		SetNextThink(2);
	}
}

void CHudSprite::Think( void )
{
	Use(this, this, USE_ON, 0);
}

void CHudSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
	{
		pActivator = CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex( 1 ));
	}

	if (ShouldToggle(useType))
	{
		if (pev->spawnflags & SF_HUDSPR_ACTIVE)
			pev->spawnflags &= ~SF_HUDSPR_ACTIVE;
		else
			pev->spawnflags |= SF_HUDSPR_ACTIVE;
	}

//		byte   : TRUE = ENABLE icon, FALSE = DISABLE icon
//		string : the sprite name to display
//		byte   : red
//		byte   : green
//		byte   : blue
	MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, pActivator->pev );
		WRITE_BYTE(pev->spawnflags & SF_HUDSPR_ACTIVE);
		WRITE_STRING(STRING(pev->model));
		WRITE_BYTE(pev->rendercolor.x);
		WRITE_BYTE(pev->rendercolor.y);
		WRITE_BYTE(pev->rendercolor.z);
	MESSAGE_END();
}

LINK_ENTITY_TO_CLASS( hud_sprite, CHudSprite );



// buz
void CBasePlayer::ViewPunch( float p, float y, float r )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		Vector vecSend = Vector( p, y, r );
		PLAYBACK_EVENT_FULL( FEV_HOSTONLY, ENT(pev), m_usViewPunch, 0, vecSend, vecSend, 0, 0, 0, 0, 0, 0 );
		return;
	}

	// vuser3 is punch speed
	pev->vuser3[0] -= p * 20;
	pev->vuser3[1] += y * 20;
	pev->vuser3[2] += r * 20;
}



int CBasePlayer::InventoryAddCheck( CBaseEntity *pEntity )
{
	if ( !strncmp( STRING( pEntity->pev->classname ), "weapon_", 7 ) )
	{
		CBasePlayerItem *pItem = GetClassPtr( (CBasePlayerItem *)pEntity->pev );

		if ( pItem )
		{
			if ( AddItemToInventory( pItem->script, pItem->pev->model, pItem->pev->classname ) )
				return TRUE;
		}
	}
	else
	{
		if ( AddItemToInventory( pEntity->pev->message, pEntity->pev->model, pEntity->pev->classname ) )
			return TRUE;
	}

	UTIL_ProMessage( this, "Inventory full", 2, false );
	return FALSE;
}

int CBasePlayer::AddItemToInventory( int name, int model, int classname )
{
	if ( FStringNull( name ) )
	{
		ALERT( at_console, "ERROR: Missing name\n" );
		return FALSE;
	}

	int freeslot = 0;

	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( IsSlotFree( cofrInventorySlot[i] ) )
			{
				freeslot = i;
				break;
			}
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( IsSlotFree( cofrInventorySlot[i] ) )
			{
				freeslot = i;
				break;
			}
		}
	}

	if ( !freeslot )
		return FALSE;

	if ( !strncmp( STRING( classname ), "weapon_", 7 ) )
	{
		if ( !HasItemInSlot( (char *)STRING( name ), false ) )
		{
			cofrInventorySlot[freeslot] = name;
			cofrInventorySlotModels[freeslot] = model;
			cofrInventorySlotClassname[freeslot] = classname;
			InventoryMessage( freeslot, cofrInventorySlot[freeslot], 0 );
		}
	}
	else
	{
		cofrInventorySlot[freeslot] = name;
			cofrInventorySlotModels[freeslot] = model;
			cofrInventorySlotClassname[freeslot] = classname;
		InventoryMessage( freeslot, cofrInventorySlot[freeslot], 0 );
	}

	if ( !gSpecialItemGive )
		EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( !strcmp( STRING( classname ), "weapon_syringe" ) )
		{
			MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
				WRITE_BYTE( ENTINDEX( edict() ) );
				WRITE_BYTE( 1 );
			MESSAGE_END();
		}
	}

	return TRUE;
}

int CBasePlayer::IsSlotFree( int slot )
{
	if ( slot )
		return FALSE;

	return TRUE;
}

int CBasePlayer::InventoryIsFull( void )
{
	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( IsSlotFree( cofrInventorySlot[i] ) )
				return FALSE;
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( IsSlotFree( cofrInventorySlot[i] ) )
				return FALSE;
		}
	}

	return TRUE;
}

void CBasePlayer::PrintInventoryInfoDebug( void )
{
	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains: %s\n", i, STRING( cofrInventorySlot[i] ) );
		}

		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Client Slot %i contains: %s\n", i, STRING( cofrClientinventorySlot[i] ) );
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains: %s\n", i, STRING( cofrInventorySlot[i] ) );
		}

		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Client Slot %i contains: %s\n", i, STRING( cofrClientinventorySlot[i] ) );
		}
	}
}

void CBasePlayer::PrintInventoryModelInfoDebug( void )
{
	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains model: %s\n", i, STRING( cofrInventorySlotModels[i] ) );
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains model: %s\n", i, STRING( cofrInventorySlotModels[i] ) );
		}
	}
}

void CBasePlayer::PrintInventoryClassnameInfoDebug( void )
{
	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains model: %s\n", i, STRING( cofrInventorySlotClassname[i] ) );
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			ALERT( at_console, "Slot %i contains model: %s\n", i, STRING( cofrInventorySlotClassname[i] ) );
		}
	}
}

void CBasePlayer::ClearOutSlot( int slotnum, int all )
{
	if ( all )
	{
		if ( m_bThreeSlots )
		{
			for ( int i = 1; i <= 8; i++ )
			{
				cofrInventorySlot[i] = 0;
				cofrInventorySlotModels[i] = 0;
				cofrInventorySlotClassname[i] = 0;

				cofrClientinventorySlot[i] = cofrInventorySlot[i];

				InventoryMessage( i, cofrInventorySlot[i], 0 );
			}
		}
		else
		{
			for ( int i = 1; i <= 8; i++ )
			{
				cofrInventorySlot[i] = 0;
				cofrInventorySlotModels[i] = 0;
				cofrInventorySlotClassname[i] = 0;

				cofrClientinventorySlot[i] = cofrInventorySlot[i];

				InventoryMessage( i, cofrInventorySlot[i], 0 );
			}
		}
	}
	else
	{
		cofrInventorySlot[slotnum] = 0;
		cofrInventorySlotModels[slotnum] = 0;
		cofrInventorySlotClassname[slotnum] = 0;

		cofrClientinventorySlot[slotnum] = cofrInventorySlot[slotnum];

		InventoryMessage( slotnum, cofrInventorySlot[slotnum], 0 );
	}
}

void CBasePlayer::ClearOutSlotNamed( int slot, int all )
{
	ALERT( at_console, "Trying to delete %s!\n", STRING( slot ) );

	if ( all )
	{
		if ( m_bThreeSlots )
		{
			for ( int i = 1; i <= 8; i++ )
			{
				cofrInventorySlot[i] = 0;
				cofrInventorySlotModels[i] = 0;
				cofrInventorySlotClassname[i] = 0;

				cofrClientinventorySlot[i] = cofrInventorySlot[i];

				InventoryMessage( i, cofrInventorySlot[i], 1 );
			}
		}
		else
		{
			for ( int i = 1; i <= 8; i++ )
			{
				cofrInventorySlot[i] = 0;
				cofrInventorySlotModels[i] = 0;
				cofrInventorySlotClassname[i] = 0;

				cofrClientinventorySlot[i] = cofrInventorySlot[i];

				InventoryMessage( i, cofrInventorySlot[i], 1 );
			}
		}
	}
	else
	{
		if ( m_bThreeSlots )
		{
			for ( int i = 1; i <= 8; i++ )
			{
				if ( FStrEq( STRING( cofrInventorySlot[i] ), STRING( slot ) ) )
				{
					cofrInventorySlot[i] = 0;
					cofrInventorySlotModels[i] = 0;
					cofrInventorySlotClassname[i] = 0;

					cofrClientinventorySlot[i] = cofrInventorySlot[i];

					InventoryMessage( i, cofrInventorySlot[i], 0 );

					if ( !all )
						return;
				}
			}
		}
		else
		{
			for ( int i = 1; i <= 8; i++ )
			{
				if ( FStrEq( STRING( cofrInventorySlot[i] ), STRING( slot ) ) )
				{
					cofrInventorySlot[i] = 0;
					cofrInventorySlotModels[i] = 0;
					cofrInventorySlotClassname[i] = 0;

					cofrClientinventorySlot[i] = cofrInventorySlot[i];

					InventoryMessage( i, cofrInventorySlot[i], 0 );

					if ( !all )
						return;
				}
			}
		}
	}
}

int CBasePlayer::HasItemInSlot( char *script, bool remove )
{
	if ( m_bThreeSlots )
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( FStrEq( STRING( cofrInventorySlot[i] ), script ) )
			{
				if ( remove )
					ClearOutSlot( i, 0 );

				return TRUE;
			}
		}
	}
	else
	{
		for ( int i = 1; i <= 8; i++ )
		{
			if ( FStrEq( STRING( cofrInventorySlot[i] ), script ) )
			{
				if ( remove )
					ClearOutSlot( i, 0 );

				return TRUE;
			}
		}
	}

	return FALSE;
}

int CBasePlayer::HasItemInSlotNumber( int slotnum, char *script )
{
	if ( FStrEq( STRING( cofrInventorySlot[slotnum] ), script ) )
	{
		return TRUE;
	}

	return FALSE;
}

char *CBasePlayer::GetItemFromSlot( int slotnumber )
{
	if ( m_bThreeSlots )
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}
	else
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}

	if ( slotnumber <= 0 )
		slotnumber = 1;

	ALERT( at_console, "GetItemFromSlot: %s\n", STRING( cofrInventorySlot[slotnumber] ) );

	return (char *)STRING( cofrInventorySlot[slotnumber] );
}

char *CBasePlayer::GetModelFromSlot( int slotnumber )
{
	if ( m_bThreeSlots )
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}
	else
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}

	if ( slotnumber <= 0 )
		slotnumber = 1;

	ALERT( at_console, "GetModelFromSlot: %s\n", STRING( cofrInventorySlotModels[slotnumber] ) );

	return (char *)STRING( cofrInventorySlotModels[slotnumber] );
}

char *CBasePlayer::GetClassnameFromSlot( int slotnumber )
{
	if ( m_bThreeSlots )
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}
	else
	{
		if ( slotnumber > 8 )
			slotnumber = 8;
	}

	if ( slotnumber <= 0 )
		slotnumber = 1;

	ALERT( at_console, "GetClassnameFromSlot: %s\n", STRING( cofrInventorySlotClassname[slotnumber] ) );

	return (char *)STRING( cofrInventorySlotClassname[slotnumber] );
}

void CBasePlayer::InventoryMessage( int slotnum, int slot, int clearslots )
{
	int threeslots = 0;

	if ( m_bThreeSlots )
		threeslots = 1;
	if ( clearslots )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgInventory, NULL, pev );
			WRITE_BYTE( slotnum );
			WRITE_BYTE( 0 );
			WRITE_STRING( STRING( slot ) );
			WRITE_BYTE( threeslots );
			if ( m_bSilentGive )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
			if ( HasTaclight() )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgInventory, NULL, pev );
			WRITE_BYTE( slotnum );
			WRITE_BYTE( 0 );
			WRITE_STRING( STRING( slot ) );
			WRITE_BYTE( threeslots );
			if ( m_bSilentGive )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
			if ( HasTaclight() )
				WRITE_BYTE( 1 );
			else
				WRITE_BYTE( 0 );
		MESSAGE_END();
	}

	if ( m_bSilentGive )
		m_bSilentGive = false;
}

void CBasePlayer::CloseInventory( void )
{
	ShowVGUIMenu( 222 );
}

void CBasePlayer::InventoryEquip( int slotnum )
{
	if (slotnum < 1 || slotnum > 8) return;
	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	if ( pev->fuser3 == 10 )
		return;

	if ( strlen( STRING( cofrInventorySlot[slotnum] ) ) <= 1 )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	char szClassname[128];

	if ( strlen( GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) > 1 )
		sprintf( szClassname, "%s", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) );

	if ( !strcmp( "item_key", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		InventoryUse( slotnum );
		return;
	}

	if ( !strncmp( GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ), "weapon_", 7 ) )
	{
		SelectItem( GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) );
		CloseInventory();
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
	}
}

void CBasePlayer::InventoryCombine( int slot1, int slot2, char *result )
{
	if (slot1<1 || slot1>8 || slot2<1 || slot2>8 || slot1==slot2 || !result) return;
	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	if ( strlen( result ) <= 1 )
	{
		ALERT( at_console, "COMBINE ERROR: No combine result\n" );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	if ( IsSlotFree( cofrInventorySlot[slot1] ) || IsSlotFree( cofrInventorySlot[slot2] ) )
	{
		ALERT( at_console, "COMBINE ERROR: There's nothing in one or both slots: %i %i\n", slot1, slot2 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	if ( slot1 <= 0 || slot1 > 8 || slot2 <= 0 || slot2 > 8 || slot1 == slot2 )
	{
		ALERT( at_console, "COMBINE ERROR: Wrong slots: %i %i\n", slot1, slot2 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	char szCompare1[128];
	sprintf( szCompare1, "%s", GetDescriptionFromItem( "combined_result", (char *)STRING( cofrInventorySlot[slot1] ) ) );

	char szCompare2[128];
	sprintf( szCompare2, "%s", GetDescriptionFromItem( "combined_result", (char *)STRING( cofrInventorySlot[slot2] ) ) );

	if ( !strcmp( szCompare1, szCompare2 ) )
	{
		if ( !strcmp( result, "glockwithtaclight" ) )
		{
			if ( !strcmp( STRING( cofrInventorySlot[slot1] ), "inventoryitems/taclight.txt" ) )
			{
				CloseInventory();
				ClearOutSlot( slot1, 0 );

				CBasePlayerWeapon *pGlock = GetPlayerWeapon( WEAPON_GLOCK );

				if ( pGlock )
					((CGlock *)pGlock)->m_bWaitingToFix = true;

				InventoryMessage( slot1, cofrInventorySlot[slot1], 0 );
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return;
			}
			else if ( !strcmp( STRING( cofrInventorySlot[slot2] ), "inventoryitems/taclight.txt" ) )
			{
				CloseInventory();
				ClearOutSlot( slot2, 0 );

				CBasePlayerWeapon *pGlock = GetPlayerWeapon( WEAPON_GLOCK );

				if ( pGlock )
					((CGlock *)pGlock)->m_bWaitingToFix = true;

				InventoryMessage( slot2, cofrInventorySlot[slot2], 0 );
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return;
			}
		}
		else if ( !strcmp( result, "phonewithbattery" ) )
		{
			if ( !strcmp( STRING( cofrInventorySlot[slot1] ), "inventoryitems/mobile_battery.txt" ) )
			{
				CloseInventory();
				ClearOutSlot( slot1, 0 );

				CBasePlayerWeapon *pMobile = GetPlayerWeapon( 2 );

				if ( pMobile )
					((CMobile *)pMobile)->m_bWaitingToFix = true;

				InventoryMessage( slot1, cofrInventorySlot[slot1], 0 );
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return;
			}
			else if ( !strcmp( STRING( cofrInventorySlot[slot2] ), "inventoryitems/mobile_battery.txt" ) )
			{
				CloseInventory();
				ClearOutSlot( slot2, 0 );

				CBasePlayerWeapon *pMobile = GetPlayerWeapon( 2 );

				if ( pMobile )
					((CMobile *)pMobile)->m_bWaitingToFix = true;

				InventoryMessage( slot2, cofrInventorySlot[slot2], 0 );
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
				return;
			}
		}
		else
		{
			ClearOutSlot( slot1, 0 );
			ClearOutSlot( slot2, 0 );

			cofrInventorySlot[slot1] = ALLOC_STRING( result );
			cofrInventorySlotModels[slot1] = ALLOC_STRING( GetDescriptionFromItem( "3dmdl", result ) );
			cofrInventorySlotClassname[slot1] = ALLOC_STRING( GetDescriptionFromItem( "classname", result ) );
			InventoryMessage( slot1, cofrInventorySlot[slot1], 0 );

			EMIT_SOUND( ENT( pev ), CHAN_ITEM, "weapons/item_get.wav", 1, ATTN_NORM );
			return;
		}
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}


void CBasePlayer::InventoryUse( int slotnum )
{
	if (slotnum < 1 || slotnum > 8) return;
	if ( strlen( GetDescriptionFromItem( "map_name", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) <= 1 )
	{
		UTIL_ProMessage( this, "Error! Item script seems to have no nice name!", 2, false );
		return;
	}

	char szCompare[128];
	strncpy( szCompare, GetDescriptionFromItem( "map_name", (char *)STRING( cofrInventorySlot[slotnum] ) ), 128 );

	CBaseEntity *pObject = NULL;
	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	UTIL_TraceLine( pev->origin + pev->view_ofs, pev->origin + pev->view_ofs + gpGlobals->v_forward * 80, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.pHit )
	{
		pObject = CBaseEntity::Instance( tr.pHit );

		if ( !pObject || !( pObject->ObjectCaps() & ( FCAP_IMPULSE_USE | FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE ) ) )
			pObject = NULL;
	}

	if ( pObject )
	{
		if ( FClassnameIs( pObject->pev, "inter_door" ) )
		{
			CInterDoor *pDoor = GetClassPtr( (CInterDoor *)pObject->pev );

			if ( pDoor )
			{
				if ( !strcmp( szCompare, STRING( pDoor->m_sLockedBy ) ) )
				{
					if ( !( pDoor->pev->spawnflags & 1 ) )
						ClearOutSlot( slotnum, 0 );

					pDoor->UnlockDoor( this );
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}

		if ( FClassnameIs( pObject->pev, "func_valve" ) )
		{
			CFuncValve *pValve = GetClassPtr( (CFuncValve *)pObject->pev );

			if ( pValve )
			{
				if ( !strcmp( szCompare, STRING( pValve->m_sLockedBy ) ) )
				{
					if ( !( pValve->pev->spawnflags & 1 ) )
						ClearOutSlot( slotnum, 0 );

					pValve->UnlockValve( this );
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}

		if ( FClassnameIs( pObject->pev, "func_button" ) )
		{
			CBaseButton *pButton = GetClassPtr( (CBaseButton *)pObject->pev );

			if ( pButton && pButton->m_isLocked )
			{
				if ( !strcmp( szCompare, STRING( pButton->m_iszLockedBy ) ) )
				{
					if ( !( pButton->pev->spawnflags & 1024 ) )
						ClearOutSlot( slotnum, 0 );

					pButton->UnlockDoor();
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}
	}

	CBaseEntity *pClosest = NULL;
	float flMaxDot = 0.7;

	Vector vecLOS;

	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 64 ) ) != NULL )
	{
		if ( FClassnameIs( pEntity->pev, "inter_door" ) )
		{
			CInterDoor *pDoor = GetClassPtr( (CInterDoor *)pEntity->pev );

			if ( pDoor && pDoor->IsLocked() )
			{
				if ( !strcmp( szCompare, STRING( pDoor->m_sLockedBy ) ) )
				{
					if ( !( pDoor->pev->spawnflags & 1 ) )
						ClearOutSlot( slotnum, 0 );

					pDoor->UnlockDoor( this );
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}

		if ( FClassnameIs( pEntity->pev, "func_valve" ) )
		{
			CFuncValve *pValve = GetClassPtr( (CFuncValve *)pEntity->pev );

			if ( pValve )
			{
				if ( pValve->m_sLockedBy && !strcmp( szCompare, STRING( pValve->m_sLockedBy ) ) )
				{
					if ( !( pValve->pev->spawnflags & 1 ) )
						ClearOutSlot( slotnum, 0 );

					pValve->UnlockValve( this );
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}

		if ( FClassnameIs( pEntity->pev, "func_button" ) )
		{
			CBaseButton *pButton = GetClassPtr( (CBaseButton *)pEntity->pev );

			if ( pButton && pButton->m_iszLockedBy && pButton->m_isLocked )
			{
				if ( !strcmp( szCompare, STRING( pButton->m_iszLockedBy ) ) )
				{
					if ( !( pButton->pev->spawnflags & 1024 ) )
						ClearOutSlot( slotnum, 0 );

					pButton->UnlockDoor();
					EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					CloseInventory();
					return;
				}
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "inventory/inventory_no.wav", 1, ATTN_NORM );
			}
		}
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}


void CBasePlayer::InventoryDrop( int slotnum )
{
	if (slotnum < 1 || slotnum > 8) return;
	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	if ( !IsAlive() )
		return;

	if ( strlen( STRING( cofrInventorySlot[slotnum] ) ) <= 1 )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	if ( !strcmp( "weapon_mobile", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		if ( g_pWorld && g_pWorld->pev->iuser2 != 1 )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			UTIL_ProMessage( this, "You can't drop your phone", 5, false );
			return;
		}
	}

	if ( !strcmp( "weapon_syringe", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_syringe" ) && ((CBasePlayerWeapon *)m_pActiveItem)->m_flNextPrimaryAttack > gpGlobals->time )
		{
			EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			return;
		}
	}

	if ( !strcmp( "weapon_radio", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		UTIL_ProMessage( this, "You can't drop your radio", 5, false );
		return;
	}

	CBaseEntity *pWorld = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 0 ) );

	if ( pWorld && pWorld->pev->spawnflags & 8 )
	{
		if ( pWorld->pev->noise3 )
			UTIL_ProMessage( this, STRING( pWorld->pev->noise3 ), 5, false );
		else
			UTIL_ProMessage( this, "I better keep this until I've explored this area. It doesn't look safe.", 5, false );

		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	if ( !strcmp( "item_key", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		DropKeyItem( slotnum );
		return;
	}

	if ( !strcmp( "item_glocktaclight", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		ClearOutSlot( slotnum, 0 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		CBaseEntity::Create( "item_glocktaclight", pev->origin, pev->angles, edict() );
		return;
	}

	if ( !strcmp( "item_phonebattery", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
	{
		ClearOutSlot( slotnum, 0 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		CBaseEntity::Create( "item_phonebattery", pev->origin, pev->angles, edict() );
		return;
	}

	if ( !strncmp( GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ), "weapon_", 7 ) )
	{
		if ( strstr( STRING( cofrInventorySlot[slotnum] ), "weapon_syringe" ) || strstr( STRING( cofrInventorySlot[slotnum] ), "weapon_flare" ) )
		{
			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				if ( m_rgpPlayerItems[i] )
				{
					CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

					while ( pPlayerItem )
					{
						if ( strstr( STRING( cofrInventorySlot[slotnum] ), "weapon_syringe" ) && FClassnameIs( pPlayerItem->edict(), "weapon_syringe" ) )
						{
							if ( m_rgAmmo[ GetAmmoIndex( "syringe" ) ] <= 1 )
							{
								if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
								{
									MESSAGE_BEGIN( MSG_ALL, gmsgSyringe );
										WRITE_BYTE( ENTINDEX( edict() ) );
										WRITE_BYTE( 0 );
									MESSAGE_END();
								}

								if ( !strcmp( m_sQuickSel1, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel1, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 1 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[1] = 0;
								}

								if ( !strcmp( m_sQuickSel2, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel2, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 2 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[2] = 0;
								}

								if ( !strcmp( m_sQuickSel3, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel3, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 3 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[3] = 0;
								}
							}
						}

						if ( strstr( STRING( cofrInventorySlot[slotnum] ), "weapon_flare" ) && FClassnameIs( pPlayerItem->edict(), "weapon_flare" ) )
						{
							if ( m_rgAmmo[ GetAmmoIndex( "flares" ) ] <= 1 )
							{
								if ( !strcmp( m_sQuickSel1, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel1, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 1 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[1] = 0;
								}

								if ( !strcmp( m_sQuickSel2, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel2, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 2 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[2] = 0;
								}

								if ( !strcmp( m_sQuickSel3, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
								{
									strcpy( m_sQuickSel3, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
										WRITE_BYTE( 3 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									m_iQuickSlotSlot[3] = 0;
								}
							}
						}

						pPlayerItem = pPlayerItem->m_pNext;
					}
				}
			}
		}
		else
		{
			if ( !strcmp( m_sQuickSel1, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
			{
				strcpy( m_sQuickSel1, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[1] = 0;
			}

			if ( !strcmp( m_sQuickSel2, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
			{
				strcpy( m_sQuickSel2, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 2 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[2] = 0;
			}

			if ( !strcmp( m_sQuickSel3, GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ) ) )
			{
				strcpy( m_sQuickSel3, "" );

				MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pev );
					WRITE_BYTE( 3 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				m_iQuickSlotSlot[3] = 0;
			}
		}

		DropPlayerItem( GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slotnum] ) ), slotnum );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_click.wav", 1, ATTN_NORM, 0, PITCH_NORM );
		return;
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, ATTN_NORM, 0, PITCH_NORM );
}

void CBasePlayer::InventoryCheck( int slotnum )
{
	if (slotnum < 1 || slotnum > 8) return;
}

void CBasePlayer::DropKeyItem( int slotnum )
{
	if (slotnum < 1 || slotnum > 8) return;
	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", cofrInventorySlot[slotnum], pev->origin + pev->view_ofs, pev->v_angle, edict(), 0 );

	if ( pKey )
	{
		pKey->pev->message = cofrInventorySlot[slotnum];
		pKey->pev->angles.x = 0;
		pKey->pev->angles.z = 0;

		pKey->pev->armorvalue = 666;

		ClearOutSlot( slotnum, 0 );
		CloseInventory();
	}
}

void CBasePlayer::FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity )
{
	int			i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult tmpTrace;
	Vector		vecHullEnd = tr.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, pEntity, &tmpTrace );
	if ( tmpTrace.flFraction < 1.0 )
	{
		tr = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEntity, &tmpTrace );
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						tr = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

void CBasePlayer::MeleeHitThinkPunch( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgPunch, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/mobile/punch_smack.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 1, 0.25, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.15, 2 );
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/mobile/punch_smack.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 1, 0.25, 0.1, 2 );
		}
	}
}

void CBasePlayer::MeleeHitThinkBranch( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 40;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgBranch, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/branch/branch_hit.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.15, 2 );
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/branch/branch_hit.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.1, 2 );
		}
	}
}

void CBasePlayer::MeleeHitThinkAxe( void )
{
	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 40;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgAxe, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/axe/axe_hitbody.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.15, 2 );
			int pitch = 100 + RANDOM_LONG( -10, 10 );
			EMIT_SOUND_DYN( ENT( pev ), CHAN_WEAPON, "weapons/axe/axe_hit.wav", 1, ATTN_NORM, 0, pitch );

			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.1, 2 );
		}
	}
}

void CBasePlayer::MeleeHitThinkNightStick( void )
{
	SwingSound();

	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( 1, 1, 8 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgNightstick, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody2.wav", 1, ATTN_NORM );

			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.15, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkNightStick2( void )
{
	SwingSound();

	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( -3, -8, 0 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_swing.wav", 1, ATTN_NORM );

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgNightstick, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.1, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody2.wav", 1, ATTN_NORM );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.15, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkNightStick3( void )
{
	SwingSound();

	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( 6, 2, 0 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgNightstick, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.1, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitbody2.wav", 1, ATTN_NORM );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 5, 0.25, 0.15, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/nightstick/nightstick_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkSledgeHammer( void )
{
	SwingSound();

	TraceResult tr;

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( -4, 1, 8 );

	m_fStamina -= 8;

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgSledgehammer, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 24 ) | ( 1 << 26 ) | ( 1 << 27 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/sledgehammer/sledgehammer_hitbody.wav", 1, ATTN_NORM );
			UTIL_ScreenShake( pev->origin, 8, 0.5, 0.1, 2 );
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 8, 0.5, 0.1, 2 );
			EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/sledgehammer/sledgehammer_hit.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkSwitchblade( void )
{
	TraceResult tr;

	if ( RANDOM_LONG( 1, 10 ) <= 7 )
		SwingSound();

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( 2, -5, 0 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgSwitchblade, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody2.wav", 1, ATTN_NORM );

			UTIL_ScreenShake( pev->origin, 1, 0.25, 0.1, 2 );

			if ( pEntity->BloodColor() == BLOOD_COLOR_RED )
			{
				if ( m_iSwitchbladeCuts < 10 )
					m_iSwitchbladeCuts++;
			}
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 3, 0.25, 0.15, 2 );

			DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkSwitchblade2( void )
{
	TraceResult tr;

	SwingSound();

	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_swing.wav", 1, ATTN_NORM );

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( -4, 1, 0 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgSwitchblade2, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			UTIL_ScreenShake( pev->origin, 1, 0.25, 0.1, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody2.wav", 1, ATTN_NORM );

			if ( pEntity->BloodColor() == BLOOD_COLOR_RED )
			{
				if ( m_iSwitchbladeCuts < 10 )
					m_iSwitchbladeCuts++;
			}
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.15, 2 );

			DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}

void CBasePlayer::MeleeHitThinkSwitchblade3( void )
{
	TraceResult tr;

	SwingSound();

	EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_swing.wav", 1, ATTN_NORM );

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( pev ), &tr );

	ViewPunch( -15, 12, 6 );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction < 1.0 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if ( pEntity )
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( pev, gSkillData.DmgSwitchblade2 * 1.5, gpGlobals->v_forward, &tr, DMG_NEVERGIB | ( 1 << 26 ) );
			ApplyMultiDamage( pev, pev );
		}

		if ( pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
		{
			UTIL_ScreenShake( pev->origin, 1, 0.25, 0.1, 2 );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitbody2.wav", 1, ATTN_NORM );

			if ( m_iSwitchbladeCuts < 10 )
				m_iSwitchbladeCuts++;
		}
		else
		{
			UTIL_ScreenShake( pev->origin, 2, 0.25, 0.15, 2 );

			DecalGunshot( &tr, BULLET_PLAYER_CROWBAR );

			if ( RANDOM_LONG( 0, 1 ) == 1 )
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall1.wav", 1, ATTN_NORM );
			else
				EMIT_SOUND( ENT( pev ), CHAN_WEAPON, "weapons/switchblade/switchblade_hitwall2.wav", 1, ATTN_NORM );
		}
	}
}


void CBasePlayer::PreSave( int slot )
{
	if ( !IsAlive() )
		return;

	saveslot = slot;

	ShowVGUIMenu( 200 );

	m_iHideHUD |= HIDEHUD_HEALTH;
	m_iHideHUD |= HIDEHUD_WEAPONS;
	pev->flags |= FL_NOTARGET;
	EMIT_SOUND( ENT( pev ), CHAN_VOICE, "save/save_sequence.wav", 1, ATTN_NORM );
	pev->velocity = g_vecZero;
	EnableControl( FALSE );

	UTIL_ProMessage( this, "Saving progress...", 2, false );

	if ( CVAR_GET_FLOAT( "difficulty" ) == 4 )
	{
		m_iTapes--;

		if ( m_iTapes == 0 )
			HasItemInSlot( "inventoryitems/cassettetape.txt", true );
	}

	b_Saving = true;

	SetNextThink( 3 );
	SetThink( &CBasePlayer::EndSave );
}

void CBasePlayer::EndSave( void )
{
	b_Saving = false;

	m_iHideHUD &= ~HIDEHUD_HEALTH;
	m_iHideHUD &= ~HIDEHUD_WEAPONS;
	pev->flags &= ~FL_NOTARGET;
	EnableControl( TRUE );

	playerstats.timessaved++;

	UTIL_ProMessage( this, "Saved", 4, false );
	UTIL_ScreenFade( this, Vector( 0, 0, 0 ), 1, 0, 255, 0 );
}

void CBasePlayer::StaminaThink( void )
{
	if ( !strcmp( STRING( gpGlobals->mapname ), "c_server_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_difficulty_settings" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_loadgame" ) )
		return;

	if ( !strcmp( STRING( gpGlobals->mapname ), "c_video_stress_test" ) )
		return;

	float speed = pev->velocity.Length2D();
	const bool uphill = cofrStairSprint.update(gpGlobals->time,pev->origin.x,pev->origin.y,pev->origin.z,m_OnStairs,(pev->flags & FL_ONGROUND)!=0);
	// Legacy sprint policy: stairs alone never trip, cap speed, or veto sprint.
	// Keep the real stair state for uphill charging; all other restrictions remain.
	const bool stairSprintRestriction = false;

	if ( IsOnLadder() && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( m_pActiveItem && m_pActiveItem->m_bRifleReload && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( m_pActiveItem && m_pActiveItem->m_bRaised && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( ( pev->button & IN_BACK ) && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( ( pev->flags & FL_ONGROUND ) && stairSprintRestriction && ( pev->flags & (1<<18) ) && !( pev->flags & FL_FROZEN ) )
	{
		TripOver();
	}

	if ( ( pev->flags & FL_DUCKING ) && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( m_pActiveItem && m_pActiveItem->m_iSuicideState && ( pev->flags & (1<<18) ) )
	{
		pev->flags &= ~(1<<18);

		if ( !( pev->flags & (1<<19) ) )
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
		else
		{
			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( !( pev->flags & (1<<19) ) )
	{
		if ( !stairSprintRestriction )
		{
			if ( ( pev->flags & (1<<18) ) && pev->maxspeed != gSkillData.plrSprintMaxSpeed )
				pev->maxspeed = gSkillData.plrSprintMaxSpeed;

			if ( !( pev->flags & (1<<18) ) && pev->maxspeed != gSkillData.plrPrimaryMaxSpeed )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
		}
		else
		{
			if ( pev->maxspeed != gSkillData.plrStairMaxSpeed )
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}
	else
	{
		if ( !stairSprintRestriction )
		{
			if ( pev->maxspeed != gSkillData.plrSecondaryMaxSpeed )
				pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
		}
		else
		{
			if ( pev->maxspeed != gSkillData.plrStairMaxSpeed )
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( !( pev->flags & (1<<18) ) && !stairSprintRestriction && ( ( m_pActiveItem && !m_pActiveItem->m_iSuicideState ) || !m_pActiveItem ) )
	{
		if ( ( pev->button & IN_RUN ) && !( pev->button & IN_BACK ) && m_fStamina >= 1 && !IsOnLadder() && speed != 0 && !( pev->flags & FL_DUCKING ) )
		{
			if ( !m_pActiveItem || ( m_pActiveItem && !m_pActiveItem->m_bRifleReload && !m_pActiveItem->m_bRaised ) )
			{
				if ( ( !m_pActiveItem || !((CBasePlayerWeapon *)m_pActiveItem)->m_bIronSights ) && pev->waterlevel != 2 )
				{
					pev->flags |= (1<<18);

					pev->maxspeed = gSkillData.plrSprintMaxSpeed;

					if ( !m_bInfiniteStamina )
						m_fStamina -= 5;
				}
			}
		}
	}

	if ( pev->flags & (1<<18) )
	{
		if ( !( pev->button & IN_RUN ) || ( m_pActiveItem && ((CBasePlayerWeapon *)m_pActiveItem)->m_bIronSights ) || ( pev->button & IN_BACK ) || m_fStamina == 0 || ( pev->flags & (1<<19) ) || ( m_pActiveItem && m_pActiveItem->m_iSuicideState ) )
		{
			pev->flags &= ~(1<<18);

			if ( !stairSprintRestriction )
				pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
			else
				pev->maxspeed = gSkillData.plrStairMaxSpeed;
		}
	}

	if ( m_fStaminaChargeCyle < gpGlobals->time && m_fStamina > 0 )
	{
		if ( ( ( pev->flags & (1<<18) ) && speed != 0 ) || m_bHoldingBreath )
		{
			if ( !m_bInfiniteStamina )
			{
				if ( m_bHoldingBreath )
					m_fStamina -= 1;
				else
					m_fStamina -= uphill ? 2.3f : 2.f;
			}

			if ( m_bNightmare )
				m_fStaminaChargeCyle = gpGlobals->time + 0.05;
			else
				m_fStaminaChargeCyle = gpGlobals->time + 0.1;
		}
		else
		{
			if ( pev->flags & FL_DUCKING )
				m_fStamina += 2;
			else
				m_fStamina += 1;

			if ( m_bNightmare )
				m_fStaminaChargeCyle = gpGlobals->time + 0.75;
			else
				m_fStaminaChargeCyle = gpGlobals->time + 0.25;
		}
	}

	if ( m_fStaminaChargeCyle < gpGlobals->time && m_fStamina == 0 )
	{
		m_fStamina = 0.1;

		m_flNextBreathHoldTime = gpGlobals->time + 4;

		if ( pev->waterlevel < 3 && m_flOutOfBreathDisableTime < gpGlobals->time )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				if ( RANDOM_LONG( 0, 1 ) == 1 )
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/player_out_of_breath1.wav", 1, 0.8 );
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/player_out_of_breath2.wav", 1, 0.8 );
			}
			else
			{
				if ( pev->vuser1.x )
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/player_out_of_breath.wav", 1, 0.8 );
				else
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/player_out_of_breath.wav", 1, 0.8 );
			}

			m_flOutOfBreathDisableTime = gpGlobals->time + 30;
		}

		if ( m_bNightmare )
			m_fStaminaChargeCyle = gpGlobals->time + 10;
		else
			m_fStaminaChargeCyle = gpGlobals->time + 5;

		pev->flags |= (1<<19);
		m_fNextScaredBreathingTime = gpGlobals->time + 5;
	}

	if ( ( pev->flags & (1<<19) ) && m_fStamina >= 1 )
		pev->flags &= ~(1<<19);

	if ( m_fStamina < 0 )
		m_fStamina = 0;

	if ( m_fStamina > 100 )
		m_fStamina = 100;

	if ( m_fClientStamina != m_fStamina )
	{
		MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgStamina, NULL, pev );
			WRITE_BYTE( m_fStamina );

			if ( m_flDodgeRechargeTime != 0 )
				WRITE_COORD( m_flDodgeRechargeTime );
			else
				WRITE_COORD( 0 );
		MESSAGE_END();

		if ( m_flDodgeRechargeTime != 0 )
			m_flDodgeRechargeTime = 0;

		m_fClientStamina = m_fStamina;
	}

	if ( ( pev->flags & (1<<18) ) && pev->waterlevel < 3 )
	{
		if ( pev->vuser2.x == 90 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pev );
				WRITE_COORD( 50 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pev->vuser2.x = 95;
		}
	}
	else
	{
		if ( pev->vuser2.x == 95 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pev );
				WRITE_COORD( 50 );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pev->vuser2.x = 90;
		}
	}

	if ( pev->waterlevel >= 1 )
	{
		TraceResult tr;
		UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, 13 ), ignore_monsters, edict(), &tr );

		if ( UTIL_PointContents( tr.vecEndPos ) == CONTENTS_WATER )
		{
			if ( pev->maxspeed > 30 )
				pev->maxspeed = 30;

			if ( !m_bInWater )
				m_bInWater = true;
		}
		else
		{
			if ( m_bInWater )
			{
				EMIT_SOUND( ENT( pev ), CHAN_ITEM, "player/exit_water.wav", 1, 0.8 );
				m_bInWater = false;
			}
		}
	}

	if ( pev->maxspeed < 30 )
		pev->maxspeed = 30;
}

void CBasePlayer::BreatheThink( void )
{
	float speed = pev->velocity.Length2D();

	if ( pev->waterlevel >= 3 )
		return;

	if ( m_fStamina >= 1 && ( pev->flags & (1<<18) ) && speed != 0 )
	{
		if ( m_fStaminaBreatheCyle < gpGlobals->time )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{
				switch ( RANDOM_LONG( 0, 8 ) )
				{
				case 0:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe1.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 1.02;
					break;
				case 1:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe2.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 0.81;
					break;
				case 2:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe3.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 0.93;
					break;
				case 3:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe4.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 0.95;
					break;
				case 4:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe5.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 0.92;
					break;
				case 5:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe6.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 1.02;
					break;
				case 6:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe7.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 1.05;
					break;
				case 7:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe8.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 1.05;
					break;
				case 8:
					EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/police/sprint_breathe9.wav", 1, 0.8 );
					m_fStaminaBreatheCyle = gpGlobals->time + 1.09;
					break;
				}
			}
			else
			{
				if ( pev->vuser1.x )
				{
					switch ( RANDOM_LONG( 0, 4 ) )
					{
					case 0:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/sprint_breathe1.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.64;
						break;
					case 1:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/sprint_breathe2.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.17;
						break;
					case 2:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/sprint_breathe3.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.45;
						break;
					case 3:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/sprint_breathe4.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.45;
						break;
					case 4:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/doctor/sprint_breathe5.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.6;
						break;
					}
				}
				else
				{
					switch ( RANDOM_LONG( 0, 4 ) )
					{
					case 0:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprint/sprint_breathe1.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.4;
						break;
					case 1:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprint/sprint_breathe2.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.5;
						break;
					case 2:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprint/sprint_breathe3.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.15;
						break;
					case 3:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprint/sprint_breathe4.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.5;
						break;
					case 4:
						EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/sprint/sprint_breathe5.wav", 1, 0.8 );
						m_fStaminaBreatheCyle = gpGlobals->time + 1.27;
						break;
					}
				}
			}

			m_fNextScaredBreathingTime = gpGlobals->time + 4;
		}
	}
	else
	{
		if ( IsAlive() && m_bScaredBreathing && m_fNextScaredBreathingTime < gpGlobals->time && !( pev->flags & (1<<19) ) )
		{
			switch ( RANDOM_LONG( 0, 5 ) )
			{
			case 0:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_1.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4.5;
				break;
			case 1:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_2.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4.5;
				break;
			case 2:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_3.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4;
				break;
			case 3:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_4.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4;
				break;
			case 4:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_5.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4;
				break;
			case 5:
				EMIT_SOUND( ENT( pev ), CHAN_VOICE, "player/vocal/breathing/breathing_6.wav", 1, 0.8 );
				m_fNextScaredBreathingTime = gpGlobals->time + 4;
				break;
			}
		}
	}
}

void CBasePlayer::ShowVGUIMenu( int iMenuID )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, pev );
		WRITE_BYTE( iMenuID );
		WRITE_BYTE( 0 );
	MESSAGE_END();
}

void CBasePlayer::KeypadNumberCheck( int number )
{
	CBaseEntity *pKeypad = UTIL_FindEntityByClassname( NULL, "cof_keypad" );

	if ( !pKeypad )
	{
		ALERT( at_error, "Error: Can't find keypad entity!\n" );
		return;
	}

	if ( number == m_iKeypadNumber && m_iKeypadNumber != 9999 )
	{
		GetClassPtr( (CCofKeypad *)pKeypad->pev )->m_IsCorrect = true;

		if ( pKeypad->pev->message )
			UTIL_ProMessage( this, UTIL_VarArgs( "%s", STRING( pKeypad->pev->message ) ), 5, false );
		else
			UTIL_ProMessage( this, "You unlocked the door.", 5, false );

		FireTargets( STRING( GetClassPtr( (CCofKeypad *)pKeypad->pev )->pev->target ), this, this, USE_TOGGLE, 0 );

		MESSAGE_BEGIN( MSG_ONE, gmsgKeypad, NULL, pev );
			WRITE_BYTE( m_iKeypadType );
			WRITE_BYTE( 1 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		m_iKeypadNumber = 9999;
	}
	else
	{
		GetClassPtr( (CCofKeypad *)pKeypad->pev )->m_IsCorrect = false;
		EMIT_SOUND( ENT(pev), CHAN_AUTO, "keypad/keypad_deny.wav", 1, ATTN_NORM );
	}
}

void CBasePlayer::PhoneNumber( int number )
{
	CBaseEntity *pCheck = NULL;

	pCheck = UTIL_FindEntityByClassname( NULL, "cof_phonecheck" );

	if ( pCheck )
	{
		GetClassPtr( (CCofPhoneCheck *)pCheck->pev )->pev->owner = edict();

		if ( pCheck->pev->frags == 1 )
			GetClassPtr( (CCofPhoneCheck *)pCheck->pev )->UnlockCheck( number );
		else
			GetClassPtr( (CCofPhoneCheck *)pCheck->pev )->DistanceCheck( number );
	}

	if ( !m_bUsedVanNumber && !m_bInPhoneCall && m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_mobile" ) )
	{
		if ( number == 911 || number == 112 )
		{
			m_bUsedVanNumber = true;

			CMobile *pMobile = GetClassPtr( (CMobile *)m_pActiveItem->pev );

			EMIT_SOUND( ENT(pev), CHAN_AUTO, "phone/tone_send.wav", 1, ATTN_NORM );

			m_iHideHUD |= HIDEHUD_HEALTH;
			m_DrawBars = 1;
			pev->fuser3 = 5;
			m_flNextAttack = gpGlobals->time + 4;

			if ( m_iMobileFlashMode )
			{
				pMobile->m_flTimeWeaponIdle = gpGlobals->time + 1.1;
				pMobile->SendWeaponAnim( 13 );
			}
			else
			{
				pMobile->m_flTimeWeaponIdle = gpGlobals->time + 1.1;
				pMobile->SendWeaponAnim( 12 );
			}

			m_bInPhoneCall = true;
			m_iPhoneNumberState_guess = 1;
			m_flPhoneNumberTime_guess = gpGlobals->time + 1;
		}
	}
}

void CBasePlayer::ViewKnock( float Amount )
{
	MESSAGE_BEGIN( MSG_ONE_UNRELIABLE, gmsgViewKnock, NULL, pev );
		WRITE_BYTE( Amount );
	MESSAGE_END();
}

void CBasePlayer::ViewLock( bool Lock, float time )
{
	if ( Lock )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgViewLock, NULL, pev );
			WRITE_BYTE( 1 );
			WRITE_COORD( time );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgViewLock, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_COORD( 0 );
		MESSAGE_END();
	}
}

void CBasePlayer::Dodge( int direction )
{
	if ( m_bDodgeDisabled )
		return;

	if ( pev->flags & FL_FROZEN )
		return;

	if ( pev->flags & FL_DUCKING )
		return;

	if ( pev->button & IN_DUCK )
		return;

	if ( pev->waterlevel >= 3 )
		return;

	if ( pev->flDuckTime )
		return;

	if ( m_fStamina < 15 || m_fDodgeTime > gpGlobals->time )
		return;

	UTIL_MakeVectors( pev->v_angle );

	SwingSound();

	if ( !m_bInfiniteStamina )
		m_fStamina -= 15;

	float flZVel = pev->velocity.z;

	switch ( direction )
	{
	case 1:
		pev->velocity = gpGlobals->v_forward * 600;
		ViewPunch( 4, 2, 2 );
		break;
	case 2:
		pev->velocity = -( gpGlobals->v_forward * 600 );
		ViewPunch( -4, 2, 2 );
		break;
	case 3:
		pev->velocity = -( gpGlobals->v_right * 600 );
		ViewPunch( 2, 4, 2 );
		break;
	case 4:
		pev->velocity = gpGlobals->v_right * 600;
		ViewPunch( 2, -4, 2 );
		break;
	}

	pev->velocity.z = flZVel;

	m_flDodgeRechargeTime = 1;
	m_fDodgeTime = gpGlobals->time + m_flDodgeRechargeTime;
}

void CBasePlayer::CheckQuickDualWield( char *quicksel, char *lastquicksel )
{
	if ( !quicksel || !lastquicksel )
		return;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return;

	if ( !strncmp( quicksel, "weapon_", 7 ) && !strncmp( lastquicksel, "weapon_", 7 ) )
	{
		if ( HasNamedPlayerItem( quicksel ) && HasNamedPlayerItem( lastquicksel ) )
		{
			int id1 = 0;
			int id2 = 0;
			int i = 0;

			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				if ( m_rgpPlayerItems[i] )
				{
					CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

					while ( pPlayerItem )
					{
						if ( FClassnameIs( pPlayerItem->pev, quicksel ) )
							id1 = pPlayerItem->m_iId;

						pPlayerItem = pPlayerItem->m_pNext;
					}
				}
			}

			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				if ( m_rgpPlayerItems[i] )
				{
					CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

					while ( pPlayerItem )
					{
						if ( FClassnameIs( pPlayerItem->pev, lastquicksel ) )
							id2 = pPlayerItem->m_iId;

						pPlayerItem = pPlayerItem->m_pNext;
					}
				}
			}

			if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_dualweapon" ) )
			{
				CDualWeapon *pDualWeapon = GetClassPtr( (CDualWeapon *)m_pActiveItem->pev );

				if ( pDualWeapon )
				{
					if ( pDualWeapon->m_iLeftWeaponID == id1 && pDualWeapon->m_iRightWeaponID == id2 )
						return;
				}
			}

			if(id1==31||id2==31){CloseInventory();UTIL_ProMessage(this,"RPG-7 cannot be dual wielded",2,false);return;}
			if ( IsAFlashlight( id1 ) && IsAFlashlight( id2 ) )
			{
				CloseInventory();
				UTIL_ProMessage( this, "I don't need to dual wield two light sources", 2, false );
				return;
			}

			if ( !CanBeDualWielded( id1 ) && !CanBeDualWielded( id2 ) )
			{
				CloseInventory();
				UTIL_ProMessage( this, UTIL_VarArgs( "Both %s and %s cannot be dual wielded", GetNameForId[id1], GetNameForId[id2] ), 2, false );
				return;
			}

			if ( ( !CanBeDualWielded( id1 ) && CanBeDualWielded( id2 ) ) || ( CanBeDualWielded( id1 ) && !CanBeDualWielded( id2 ) ) )
			{
				CloseInventory();
				UTIL_ProMessage( this, UTIL_VarArgs( "%s cannot be dual wielded with %s", GetNameForId[id1], GetNameForId[id2] ), 2, false );
				return;
			}

			if ( id1 > 0 && id2 > 0 && id1 != id2 )
			{
				GiveDualWeapon( id1, id2 );
				EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "inventory/inventory_click.wav", 1, 0.8, 0, PITCH_NORM );
			}
		}
	}
}

void CBasePlayer::QuickSelectItem( int number )
{
	if ( number < 1 || number > 3 )
		return;

	if ( pev->flags & FL_FROZEN )
		return;

	if ( m_bInPhoneCall )
		return;

	if ( IsOnLadder() )
		return;

	if ( pev->fuser3 == 10 )
		return;

	if ( m_flQuickSlotTimeDecay > gpGlobals->time && lastquickslotpressed != number && g_pGameRules && !g_pGameRules->IsMultiplayer() )
	{
		switch ( number )
		{
		case 1:
			switch ( lastquickslotpressed )
			{
			case 1:
				CheckQuickDualWield( m_sQuickSel1, m_sQuickSel1 );
				break;
			case 2:
				CheckQuickDualWield( m_sQuickSel1, m_sQuickSel2 );
				break;
			case 3:
				CheckQuickDualWield( m_sQuickSel1, m_sQuickSel3 );
				break;
			}
			break;
		case 2:
			switch ( lastquickslotpressed )
			{
			case 1:
				CheckQuickDualWield( m_sQuickSel2, m_sQuickSel1 );
				break;
			case 2:
				CheckQuickDualWield( m_sQuickSel2, m_sQuickSel2 );
				break;
			case 3:
				CheckQuickDualWield( m_sQuickSel2, m_sQuickSel3 );
				break;
			}
			break;
		case 3:
			switch ( lastquickslotpressed )
			{
			case 1:
				CheckQuickDualWield( m_sQuickSel3, m_sQuickSel1 );
				break;
			case 2:
				CheckQuickDualWield( m_sQuickSel3, m_sQuickSel2 );
				break;
			case 3:
				CheckQuickDualWield( m_sQuickSel3, m_sQuickSel3 );
				break;
			}
			break;
		}
	}

	lastquickslotpressed = number;

	m_flQuickSlotTimeDecay = gpGlobals->time + 0.25;

	if ( number == 1 )
	{
		if ( !strncmp( m_sQuickSel1, "weapon_", 7 ) )
		{
			if ( HasNamedPlayerItem( m_sQuickSel1 ) )
			{
				SelectItem( m_sQuickSel1 );
				return;
			}
		}
	}

	if ( number == 2 )
	{
		if ( !strncmp( m_sQuickSel2, "weapon_", 7 ) )
		{
			if ( HasNamedPlayerItem( m_sQuickSel2 ) )
			{
				SelectItem( m_sQuickSel2 );
				return;
			}
		}
	}

	if ( number == 3 )
	{
		if ( !strncmp( m_sQuickSel3, "weapon_", 7 ) )
		{
			if ( HasNamedPlayerItem( m_sQuickSel3 ) )
			{
				SelectItem( m_sQuickSel3 );
				return;
			}
		}
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, 0.8, 0, PITCH_NORM );
}

char *CBasePlayer::GetDescriptionFromItem( char *item, char *filename )
{
	int gSCounter = 0;
	char szItemName[512];

	int length;
	char *pFile;
	char *aFile = pFile = (char *)LOAD_FILE_FOR_ME( filename, &length );

	if ( !pFile || !length )
	{
		ALERT( at_console, " *\n" );
		ALERT( at_console, " * GetItemDescription failed to load file %s\n", filename );
		ALERT( at_console, " *\n" );
		return "";
	}

	while ( 1 )
	{
		pFile = COM_Parse( pFile );

		if ( strlen( com_token ) <= 0 )
			break;

		strcpy( szItemName, com_token );

		pFile = COM_Parse( pFile );

		if ( com_token[0] != '{' )
		{
			ALERT( at_console, " *\n" );
			ALERT( at_console, " * CheckItem parsing error: expecting { after key name\n" );
			ALERT( at_console, " *\n" );
			break;
		}

		while ( 1 )
		{
			pFile = COM_Parse( pFile );

			if ( com_token[0] == '}' )
				break;

			if ( !strcmp( "data", com_token ) )
			{
				pFile = COM_Parse( pFile );

				if ( !strcmp( szItemName, item ) )
				{
					FREE_FILE( aFile );
					return com_token;
				}
			}
		}

		gSCounter++;
	}

	FREE_FILE( aFile );
	ALERT( at_console, "KEY ERROR: Couldn't parse token of key at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
	return "";
}

void CBasePlayer::SelectRandomWeapon( void )
{
	int randcount = 0;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

		while ( pPlayerItem )
		{
			if ( !FClassnameIs( pPlayerItem->pev, "weapon_action" ) )
				randcount++;

			pPlayerItem = pPlayerItem->m_pNext;
		}
	}

	if ( randcount )
	{
		CBasePlayerItem *pPlayerSelect = m_rgpPlayerItems[ RANDOM_LONG( 1, randcount ) ];

		if ( pPlayerSelect )
		{
			pPlayerSelect->Deploy();
			pPlayerSelect->UpdateItemInfo();
		}
	}
}

void CBasePlayer::Vomit( void )
{
	m_bHasVomited = true;

	ViewPunch( -20, 0, 0 );

	MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
		WRITE_SHORT( entindex() );
		WRITE_STRING( "aurora/vomit.aur" );
		WRITE_BYTE( 0 );
		WRITE_STRING( STRING( pev->model ) );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	EMIT_SOUND_DYN( ENT(pev), CHAN_VOICE, pVomitSounds[ RANDOM_LONG( 0, 0 ) ], 1, ATTN_NORM, 0, PITCH_NORM );
}

void CBasePlayer::MorphineTrip( void )
{
	if ( m_bIsTripping )
		return;

	if ( !m_iBlurActive )
	{
		m_bIsTripping = true;
		m_bHasVomited = false;
		m_fTrippingTime = gpGlobals->time + 30;

		if ( RANDOM_LONG( 0, 2 ) > 0 )
		{
			m_bHasVomited = false;
			m_fVomitTime = RANDOM_LONG( 5, 20 ) + gpGlobals->time;
		}
		else
			m_bHasVomited = true;

		m_flNextMorphineDreamTime = gpGlobals->time;

		UTIL_ScreenFade( this, Vector( 255, 255, 255 ), 0.25, 0, 64, 0 );

		m_iBlurActive = 1;

		m_iOldRoomType = CVAR_GET_FLOAT( "room_type" );
		CVAR_SET_FLOAT( "room_type", 16 );

		MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
			WRITE_BYTE( 10 );
			WRITE_BYTE( 1 );
		MESSAGE_END();
	}
}

void CBasePlayer::StopMorphineTrip( void )
{
	if ( m_bIsTripping )
	{
		m_bIsTripping = false;
		m_bHasVomited = false;
		m_fTrippingTime = 0;
		m_fMorphineTripTime = 0;

		STOP_SOUND( edict(), CHAN_AUTO, "player/morphine_dream.wav" );

		m_iBlurActive = 0;

		CVAR_SET_FLOAT( "room_type", m_iOldRoomType );

		MESSAGE_BEGIN( MSG_ONE, gmsgConcuss, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

void CBasePlayer::CheckMorphineRestore( void )
{
	if ( fabs( gpGlobals->time - m_iMorphineCycleTime ) < 0.1 )
		return;

	m_iMorphineCycleTime = gpGlobals->time;

	if ( m_iMorphineRecover <= 0 )
		return;

	if ( pev->health >= pev->max_health )
	{
		if ( m_iMorphineRecover > 0 )
			m_iMorphineRecover = 0;

		return;
	}

	int oldNumber = m_iMorphineRecover;
	m_iMorphineRecover -= 5;

	if ( m_iMorphineRecover < 0 )
		m_iMorphineRecover = 0;

	int newNumber = oldNumber - m_iMorphineRecover;

	m_fStamina += newNumber;
	TakeHealth( newNumber, DMG_GENERIC );

	if ( m_fStamina > 100 )
		m_fStamina = 100;
}

void CBasePlayer::ToThirdPerson( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgThirdPerson, NULL, pev );
		WRITE_BYTE( 1 );
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pPlayerToRevive )
			WRITE_SHORT( ENTINDEX( pPlayerToRevive->edict() ) );
		else
			WRITE_SHORT( 255 );
	MESSAGE_END();

	m_iViewValue = 1;
}

void CBasePlayer::ToFirstPerson( void )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgThirdPerson, NULL, pev );
		WRITE_BYTE( 0 );
		WRITE_SHORT( 255 );
	MESSAGE_END();

	m_iViewValue = 0;
}

void CBasePlayer::SetCorrectView( void )
{
	if ( m_iViewValue == 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgThirdPerson, NULL, pev );
			WRITE_BYTE( 0 );
			WRITE_SHORT( 255 );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgThirdPerson, NULL, pev );
			WRITE_BYTE( 1 );
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pPlayerToRevive )
				WRITE_SHORT( ENTINDEX( pPlayerToRevive->edict() ) );
			else
				WRITE_SHORT( 255 );
		MESSAGE_END();
	}
}

void CBasePlayer::ToSlowMo( void )
{
	CVAR_SET_FLOAT( "host_framerate", 0.005 );
	m_iMoValue = 1;
}

void CBasePlayer::ToNormalMo( void )
{
	CVAR_SET_FLOAT( "host_framerate", 0 );
	m_iMoValue = 0;
}

void CBasePlayer::SetCorrectMo( void )
{
	if ( m_iMoValue == 0 )
	{
		CVAR_SET_FLOAT( "host_framerate", 0 );
	}
	else
	{
		CVAR_SET_FLOAT( "host_framerate", 0.005 );
	}
}

void CBasePlayer::MoveStatue( int Direction, int Type )
{
	CBaseEntity *pEagle;
	CBaseEntity *pHorse;
	CBaseEntity *pLion;
	CBaseEntity *pOwl;

	switch ( Type )
	{
	case 1:
		switch ( Direction )
		{
		case 0:
			pEagle = UTIL_FindEntityByClassname( NULL, "statue_eagle" );
			if ( pEagle )
			{
				GetClassPtr( (CStatueEagle *)pEagle->pev )->MoveUp();
			}
			break;
		case 1:
			pEagle = UTIL_FindEntityByClassname( NULL, "statue_eagle" );
			if ( pEagle )
			{
				GetClassPtr( (CStatueEagle *)pEagle->pev )->MoveRight();
			}
			break;
		case 2:
			pEagle = UTIL_FindEntityByClassname( NULL, "statue_eagle" );
			if ( pEagle )
			{
				GetClassPtr( (CStatueEagle *)pEagle->pev )->MoveDown();
			}
			break;
		case 3:
			pEagle = UTIL_FindEntityByClassname( NULL, "statue_eagle" );
			if ( pEagle )
			{
				GetClassPtr( (CStatueEagle *)pEagle->pev )->MoveLeft();
			}
			break;
		}
		break;
	case 2:
		switch ( Direction )
		{
		case 0:
			pHorse = UTIL_FindEntityByClassname( NULL, "statue_horse" );
			if ( pHorse )
			{
				GetClassPtr( (CStatueHorse *)pHorse->pev )->MoveUp();
			}
			break;
		case 1:
			pHorse = UTIL_FindEntityByClassname( NULL, "statue_horse" );
			if ( pHorse )
			{
				GetClassPtr( (CStatueHorse *)pHorse->pev )->MoveRight();
			}
			break;
		case 2:
			pHorse = UTIL_FindEntityByClassname( NULL, "statue_horse" );
			if ( pHorse )
			{
				GetClassPtr( (CStatueHorse *)pHorse->pev )->MoveDown();
			}
			break;
		case 3:
			pHorse = UTIL_FindEntityByClassname( NULL, "statue_horse" );
			if ( pHorse )
			{
				GetClassPtr( (CStatueHorse *)pHorse->pev )->MoveLeft();
			}
			break;
		}
		break;
	case 3:
		switch ( Direction )
		{
		case 0:
			pLion = UTIL_FindEntityByClassname( NULL, "statue_lion" );
			if ( pLion )
			{
				GetClassPtr( (CStatueLion *)pLion->pev )->MoveUp();
			}
			break;
		case 1:
			pLion = UTIL_FindEntityByClassname( NULL, "statue_lion" );
			if ( pLion )
			{
				GetClassPtr( (CStatueLion *)pLion->pev )->MoveRight();
			}
			break;
		case 2:
			pLion = UTIL_FindEntityByClassname( NULL, "statue_lion" );
			if ( pLion )
			{
				GetClassPtr( (CStatueLion *)pLion->pev )->MoveDown();
			}
			break;
		case 3:
			pLion = UTIL_FindEntityByClassname( NULL, "statue_lion" );
			if ( pLion )
			{
				GetClassPtr( (CStatueLion *)pLion->pev )->MoveLeft();
			}
			break;
		}
		break;
	case 4:
		switch ( Direction )
		{
		case 0:
			pOwl = UTIL_FindEntityByClassname( NULL, "statue_owl" );
			if ( pOwl )
			{
				GetClassPtr( (CStatueOwl *)pOwl->pev )->MoveUp();
			}
			break;
		case 1:
			pOwl = UTIL_FindEntityByClassname( NULL, "statue_owl" );
			if ( pOwl )
			{
				GetClassPtr( (CStatueOwl *)pOwl->pev )->MoveRight();
			}
			break;
		case 2:
			pOwl = UTIL_FindEntityByClassname( NULL, "statue_owl" );
			if ( pOwl )
			{
				GetClassPtr( (CStatueOwl *)pOwl->pev )->MoveDown();
			}
			break;
		case 3:
			pOwl = UTIL_FindEntityByClassname( NULL, "statue_owl" );
			if ( pOwl )
			{
				GetClassPtr( (CStatueOwl *)pOwl->pev )->MoveLeft();
			}
			break;
		}
		break;
	}
}

Vector CBasePlayer::GetAccuracy( float accuracy )
{
	if ( accuracy <= 1 )
	{
		return VECTOR_CONE_1DEGREES;
	}
	else if ( accuracy > 1 && accuracy <= 2 )
	{
		return VECTOR_CONE_2DEGREES;
	}
	else if ( accuracy > 2 && accuracy <= 3 )
	{
		return VECTOR_CONE_3DEGREES;
	}
	else if ( accuracy > 3 && accuracy <= 4 )
	{
		return VECTOR_CONE_4DEGREES;
	}
	else if ( accuracy > 4 && accuracy <= 5 )
	{
		return VECTOR_CONE_5DEGREES;
	}
	else if ( accuracy > 5 && accuracy <= 6 )
	{
		return VECTOR_CONE_6DEGREES;
	}
	else if ( accuracy > 6 && accuracy <= 7 )
	{
		return VECTOR_CONE_7DEGREES;
	}
	else if ( accuracy > 7 && accuracy <= 8 )
	{
		return VECTOR_CONE_8DEGREES;
	}
	else if ( accuracy > 8 && accuracy <= 9 )
	{
		return VECTOR_CONE_9DEGREES;
	}
	else
		return VECTOR_CONE_10DEGREES;
}

class CGiver : public CPointEntity
{
	void Spawn( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CGiver::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CGiver::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			gSpecialItemGive = 1;
			pPlayer->m_bSilentGive = true;

			switch ( pev->iuser1 )
			{
			case 1:
				pPlayer->GiveNamedItem( "weapon_mobile", false, false );
				pPlayer->GiveNamedItem( "weapon_switchblade", false, false );
				pPlayer->GiveNamedItem( "weapon_glock", false, false );
				pPlayer->GiveNamedItem( "weapon_tmp", false, false );
				pPlayer->GiveNamedItem( "weapon_tmp", false, false );
				pPlayer->GiveNamedItem( "weapon_tmp", false, false );
				pPlayer->GiveNamedItem( "weapon_syringe", false, false );
				break;
			case 2:
				pPlayer->GiveNamedItem( "weapon_flashlight", false, false );
				pPlayer->GiveNamedItem( "weapon_nightstick", false, false );
				pPlayer->GiveNamedItem( "weapon_shotgun", false, false );
				pPlayer->GiveNamedItem( "weapon_shotgun", false, false );
				pPlayer->GiveNamedItem( "weapon_syringe", false, false );
				break;
			case 3:
				pPlayer->GiveNamedItem( "weapon_flashlight", false, false );
				pPlayer->GiveNamedItem( "weapon_nightstick", false, false );
				pPlayer->GiveNamedItem( "weapon_shotgun", false, false );
				pPlayer->GiveNamedItem( "weapon_shotgun", false, false );
				break;
			}

			gSpecialItemGive = 0;
		}
	}
}

LINK_ENTITY_TO_CLASS( lol_giver, CGiver );

void CBasePlayer::SetDualWieldWeapon( int model )
{
	m_sDualWieldViewModel = model;

	if ( model == 0 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualWield, NULL, pev );
			WRITE_STRING( "models/null.mdl" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( -1 );
			WRITE_BYTE( -1 );
			WRITE_SHORT( -1 );
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgDualWield, NULL, pev );
			WRITE_STRING( STRING( m_sDualWieldViewModel ) );
			WRITE_BYTE( gDualWeapon.dualbody );
			WRITE_BYTE( m_iDualWeaponLeftID );
			WRITE_BYTE( m_iDualWeaponRightID );
            WRITE_SHORT(entindex());
		MESSAGE_END();
	}
}

void CBasePlayer::DualWieldAnimation( int anim )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgDualAnim, NULL, pev );
		WRITE_BYTE( anim );
	MESSAGE_END();
}

bool CBasePlayer::IsAFlashlight( int weaponid )
{
	switch ( weaponid )
	{
	case 1:
		return true;

	case 2:
		return true;

	case 10:
		return true;

	case 13:
		return true;
	}

	return false;
}

bool CBasePlayer::CanBeDualWielded( int weaponid )
{
	switch ( weaponid )
	{
	case 1:
		return true;

	case 2:
		return true;

	case 3:
		return true;

	case 6:
		return true;

	case 7:
		return true;

	case 21:
		if ( g_pWorld && g_pWorld->pev->iuser3 )
			return true;
		else
			return false;

	case 16:
		return true;

	case 10:
		return true;

	case 12:
		return true;

	case 13:
		return true;
	case 19:
		return true;
	}

	return false;
}

bool CBasePlayer::CanUseWeapon_guess( int weaponid )
{
	// MP5A3: Steam group members and mapper mode only
	if ( weaponid == 27 )
	{
		if ( !IsInSteamGroup_guess( 4632074 ) && !IsInSteamGroup_guess( 764566 ) && !g_pGameRules->IsMapperMode_guess() )
			return false;
	}

	return true;
}

void CBasePlayer::GiveDualWeapon( int id1, int id2 )
{
	if ( id1 == 13 && id1 != id2 )
	{
		GiveDualWeapon( id2, id1 );
		return;
	}

	if ( IsAFlashlight( id1 ) && !IsAFlashlight( id2 ) )
	{
		GiveDualWeapon( id2, id1 );
		return;
	}

	if ( HasNamedPlayerItem( "weapon_dualweapon" ) )
	{
		for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					if ( FClassnameIs( pPlayerItem->pev, "weapon_dualweapon" ) )
					{
						pev->weapons &= ~( 1 << pPlayerItem->m_iId );
						pPlayerItem->DestroyItem();
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
	}

	gEvilImpulse101 = TRUE;

	int istr = MAKE_STRING( "weapon_dualweapon" );

	edict_t *pent = CREATE_NAMED_ENTITY( istr );
	if ( FNullEnt( pent ) )
	{
		ALERT( at_console, "NULL Ent in GiveNamedItem!\n" );
		return;
	}

	VARS( pent )->origin = pev->origin;
	pent->v.spawnflags |= SF_NORESPAWN;

	pent->v.iuser4 = 1;

	pent->v.iuser1 = id1;
	pent->v.iuser2 = id2;

	m_iDualWeaponLeftID = id1;
	m_iDualWeaponRightID = id2;

	DispatchSpawn( pent );
	DispatchTouch( pent, ENT( pev ) );
	gEvilImpulse101 = FALSE;
}

void CBasePlayer::AddGoodPoint( int amount, bool subtract )
{
	if ( subtract )
	{
		m_iGoodPoints -= amount;

		if ( m_iGoodPoints < 0 )
			m_iGoodPoints = 0;
	}
	else
	{
		m_iGoodPoints += amount;

		if ( m_iGoodPoints > 5 )
			m_iGoodPoints = 5;
	}
}

void CBasePlayer::DualWield( int slot1, int slot2 )
{
	if (slot1<1 || slot1>8 || slot2<1 || slot2>8 || slot1==slot2) return;
	bool found1 = false;
	bool found2 = false;
	int weaponid1 = 0;
	int weaponid2 = 0;

	if ( weaponid1 == 13 && weaponid1 != weaponid2 )
	{
		DualWield( slot2, slot1 );
		return;
	}

	if ( IsSlotFree( cofrInventorySlot[slot1] ) || IsSlotFree( cofrInventorySlot[slot2] ) )
	{
		ALERT( at_console, "DUAL WIELD ERROR: There's nothing in one or both slots: %i %i\n", slot1, slot2 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, 0.8, 0, PITCH_NORM );
		return;
	}

	if ( slot1 <= 0 || slot1 > 8 || slot2 <= 0 || slot2 > 8 || slot1 == slot2 )
	{
		ALERT( at_console, "DUAL WIELD ERROR: Wrong slots: %i %i\n", slot1, slot2 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, 0.8, 0, PITCH_NORM );
		return;
	}

	char szWeapon1[128];
	sprintf( szWeapon1, "%s", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slot1] ) ) );

	char szWeapon2[128];
	sprintf( szWeapon2, "%s", GetDescriptionFromItem( "classname", (char *)STRING( cofrInventorySlot[slot2] ) ) );

	if ( m_pActiveItem )
	{
		if ( ((CBasePlayerWeapon *)m_pActiveItem)->m_bIronSights )
		{
			m_pActiveItem->IronsightsFrom();
		}
	}

	if ( strstr( szWeapon1, "weapon_" ) && strstr( szWeapon2, "weapon_" ) )
	{
		int i;

		for ( i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( !strcmp( STRING( gun->pev->classname ), szWeapon1 ) )
						{
							weaponid1 = gun->m_iId;
							found1 = true;
						}
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}

		for ( i = 0; i < MAX_ITEM_TYPES; i++ )
		{
			if ( m_rgpPlayerItems[i] )
			{
				CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

				while ( pPlayerItem )
				{
					CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

					if ( gun )
					{
						if ( !strcmp( STRING( gun->pev->classname ), szWeapon2 ) )
						{
							weaponid2 = gun->m_iId;
							found2 = true;
						}
					}

					pPlayerItem = pPlayerItem->m_pNext;
				}
			}
		}
	}
	else
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_ITEM, "inventory/inventory_no.wav", 1, 0.8, 0, PITCH_NORM );
		return;
	}

	if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_dualweapon" ) )
	{
		CDualWeapon *pDualWeapon = GetClassPtr( (CDualWeapon *)m_pActiveItem->pev );

		if ( pDualWeapon )
		{
			if ( pDualWeapon->m_iLeftWeaponID == weaponid1 && pDualWeapon->m_iRightWeaponID == weaponid2 )
				return;
		}
	}

	if(weaponid1==31||weaponid2==31){UTIL_ProMessage(this,"RPG-7 cannot be dual wielded",2,false);return;}
	if ( IsAFlashlight( weaponid1 ) && IsAFlashlight( weaponid2 ) )
	{
		CloseInventory();
		UTIL_ProMessage( this, "I don't need to dual wield two light sources", 2, false );
		return;
	}

	if ( !CanBeDualWielded( weaponid1 ) && !CanBeDualWielded( weaponid2 ) )
	{
		CloseInventory();
		UTIL_ProMessage( this, UTIL_VarArgs( "Both %s and %s cannot be dual wielded", GetNameForId[weaponid1], GetNameForId[weaponid2] ), 2, false );
		return;
	}

	if ( ( !CanBeDualWielded( weaponid1 ) && CanBeDualWielded( weaponid2 ) ) || ( CanBeDualWielded( weaponid1 ) && !CanBeDualWielded( weaponid2 ) ) )
	{
		CloseInventory();
		UTIL_ProMessage( this, UTIL_VarArgs( "%s cannot be dual wielded with %s", GetNameForId[weaponid1], GetNameForId[weaponid2] ), 2, false );
		return;
	}

	if ( found1 && found2 )
	{
		GiveDualWeapon( weaponid1, weaponid2 );
		EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "inventory/inventory_click.wav", 1, 0.8, 0, PITCH_NORM );
		CloseInventory();

		return;
	}

	EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "inventory/inventory_no.wav", 1, 0.8, 0, PITCH_NORM );
}

void CBasePlayer::DebugDocuments( void )
{
	for ( int i = 1; i <= 16; i++ )
	{
		ALERT( at_console, "Note %i contains %s\n", i, documentsstruct[i].note1 );
	}
}

void CBasePlayer::AddDocument( CCoFDocument *pDocument )
{
	if ( !pDocument )
		return;

	int i;

	for ( i = 1; i <= 16; i++ )
	{
		if ( strlen( documentsstruct[i].note1 ) > 1 )
		{
			if ( !strcmp( documentsstruct[i].note1, pDocument->note1 ) )
			{
				return;
			}
		}
	}

	for ( i = 1; i <= 16; i++ )
	{
		if ( strlen( documentsstruct[i].note1 ) < 1 )
		{
			char sFileName[256];
			GET_GAME_DIR( sFileName );
			strcat( sFileName, "/notes/" );
			strcat( sFileName, pDocument->note1 );

			std::vector<std::string> text_file;

			std::ifstream ifs( sFileName );
			std::string temp;
			char DestinationText[256];

			if ( !ifs.fail() )
			{
				std::string line;
				int number = 1;
				int linecount = 1;

				while ( std::getline( ifs, line ) )
				{
					if ( linecount == number )
					{
						text_file.push_back( temp );
						strcpy( DestinationText, line.c_str() );
						DestinationText[ line.length() ] = 0;
						UTIL_ProMessage( this, UTIL_VarArgs( "Archived \"%s\"", DestinationText ), 2, false );

						strcpy( documentsstruct[i].note1, pDocument->note1 );
						strcpy( documentsstruct[i].note2, pDocument->note2 );
						strcpy( documentsstruct[i].note3, pDocument->note3 );
						strcpy( documentsstruct[i].note4, pDocument->note4 );
						strcpy( documentsstruct[i].page1, pDocument->page1 );
						strcpy( documentsstruct[i].page2, pDocument->page2 );
						strcpy( documentsstruct[i].page3, pDocument->page3 );
						strcpy( documentsstruct[i].page4, pDocument->page4 );
						strcpy( documentsstruct[i].turnsound, STRING( pDocument->turnsound ) );
						strcpy( documentsstruct[i].title, DestinationText );
						strcpy( documentsstruct[i].label, pDocument->label );

						MESSAGE_BEGIN( MSG_ONE, gmsgAddNote, NULL, pev );
							WRITE_BYTE( i );
							WRITE_BYTE( 0 );
							WRITE_SHORT( 0 );
							WRITE_BYTE( 0 );
							WRITE_STRING( documentsstruct[i].note1 );
							WRITE_STRING( documentsstruct[i].note2 );
							WRITE_STRING( documentsstruct[i].note3 );
							WRITE_STRING( documentsstruct[i].note4 );
							WRITE_STRING( documentsstruct[i].page1 );
							WRITE_STRING( documentsstruct[i].page2 );
							WRITE_STRING( documentsstruct[i].page3 );
							WRITE_STRING( documentsstruct[i].page4 );
							WRITE_STRING( documentsstruct[i].turnsound );
							WRITE_STRING( documentsstruct[i].title );
							WRITE_STRING( documentsstruct[i].label );
						MESSAGE_END();
					}

					linecount++;
				}
			}

			return;
		}
	}
}

void CBasePlayer::RecallDocument( int slot )
{
	if ( slot < 1 || slot > 16 )
		return;

	if ( strlen( documentsstruct[slot].note1 ) > 1 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgNoteDoc, NULL, pev );
			WRITE_STRING( documentsstruct[slot].page1 );
			WRITE_STRING( documentsstruct[slot].page2 );
			WRITE_STRING( documentsstruct[slot].page3 );
			WRITE_STRING( documentsstruct[slot].page4 );
			WRITE_STRING( documentsstruct[slot].note1 );
			WRITE_STRING( documentsstruct[slot].note2 );
			WRITE_STRING( documentsstruct[slot].note3 );
			WRITE_STRING( documentsstruct[slot].note4 );
			WRITE_STRING( documentsstruct[slot].turnsound );
			WRITE_STRING( documentsstruct[slot].title );
			WRITE_STRING( documentsstruct[slot].label );
		MESSAGE_END();
	}
}

CBasePlayerWeapon *CBasePlayer::GetPlayerWeapon( int id )
{
	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		if ( m_rgpPlayerItems[i] )
		{
			CBasePlayerItem *pPlayerItem = m_rgpPlayerItems[i];

			while ( pPlayerItem )
			{
				CBasePlayerWeapon *gun = (CBasePlayerWeapon *)pPlayerItem->GetWeaponPtr();

				if ( gun )
				{
					if ( gun->m_iId == id )
						return gun;
				}

				pPlayerItem = pPlayerItem->m_pNext;
			}
		}
	}

	return NULL;
}

bool CBasePlayer::HasTaclight( void )
{
	CBasePlayerItem *pItem;

	for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
	{
		pItem = m_rgpPlayerItems[i];

		while ( pItem )
		{
			if ( !strcmp( STRING( pItem->pev->classname ), "weapon_glock" ) )
			{
				if ( ((CGlock *)pItem)->m_bTaclight )
					return true;
			}

			pItem = pItem->m_pNext;
		}
	}

	return false;
}

void CBasePlayer::StartWheelchairMode( void )
{
	if ( m_pWheelchair )
		return;

	CBaseEntity *pChair = CBaseEntity::Create( "wheelchair", pev->origin, pev->angles, edict() );

	if ( pChair )
	{
		ClearOutSlot( 1, 1 );
		RemoveAllItems( FALSE );
		EnableControl( FALSE );
		pev->flags |= FL_NOTARGET;

		if ( m_iHeadShieldOn )
		{
			m_iFlashlightFlags &= ~64;
			m_iHeadShieldOn = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pev );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 10 );
				WRITE_STRING( "" );
				if ( pev->vuser1.x )
					WRITE_BYTE( 0 );
				else
					WRITE_BYTE( 1 );
			MESSAGE_END();
		}

		m_iHealthCap = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, pev );
			WRITE_BYTE( m_iHealthCap );
		MESSAGE_END();

		pev->health = 100;

		pChair->pev->angles.x = 0;
		GetClassPtr( (CWheelchair *)pChair->pev )->WheelchairUse( this, this, USE_TOGGLE, 0 );
	}
}

void CBasePlayer::StopWheelchairMode( void )
{
	if ( !m_pWheelchair )
		return;

	GetClassPtr( (CWheelchair *)m_pWheelchair->pev )->Disembark();
	g_engfuncs.pfnSetPhysicsKeyValue( edict(), "chr", "1" );
}

void CBasePlayer::ShowHint( int hint, bool showanyway )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		return;

	int i;

	for ( i = 1; i < 32; i++ )
	{
		if ( FStrEq( STRING( m_strHintsShown[i] ), STRING( hint ) ) )
			return;
	}

	for ( i = 1; i < 32; i++ )
	{
		if ( m_strHintsShown[i] == 0 )
		{
			m_strHintsShown[i] = hint;

			char m_sLanguage[128];

			int language = 0;
			language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

			switch ( language )
			{
			case 0:
				strcpy( m_sLanguage, "txtfiles/hints.txt" );
				break;
			case 1:
				strcpy( m_sLanguage, "txtfiles/hints.txt" );
				break;
			case 2:
				strcpy( m_sLanguage, "txtfiles/languages/dutch/hints.txt" );
				break;
			case 3:
				strcpy( m_sLanguage, "txtfiles/languages/french/hints.txt" );
				break;
			case 4:
				strcpy( m_sLanguage, "txtfiles/languages/german/hints.txt" );
				break;
			case 5:
				strcpy( m_sLanguage, "txtfiles/languages/norwegian/hints.txt" );
				break;
			case 6:
				strcpy( m_sLanguage, "txtfiles/languages/spanish/hints.txt" );
				break;
			case 7:
				strcpy( m_sLanguage, "txtfiles/languages/swedish/hints.txt" );
				break;
			default:
				strcpy( m_sLanguage, "txtfiles/hints.txt" );
				break;
			}

			MESSAGE_BEGIN( MSG_ONE, gmsgControlHint, NULL, pev );
				WRITE_STRING( STRING( hint ) );
				WRITE_STRING( m_sLanguage );
			MESSAGE_END();
			return;
		}
	}
}

void CBasePlayer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_bCantBeRevived && !pPlayerReviving && !m_bCoopGameOver && pActivator && pActivator->IsPlayer() && !IsAlive() && pActivator->IsAlive() )
	{
		CBasePlayer *pReviver = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pReviver && !pReviver->pPlayerToRevive && ( pev->flags & FL_ONGROUND ) )
		{
			pReviver->pev->velocity = g_vecZero;

			pReviver->pev->origin = pev->origin;

			pReviver->pev->angles.y = pev->angles.y + 90;
			pReviver->pev->angles.x = 0;
			pReviver->pev->v_angle.x = 0;
			pReviver->pev->fixangle = TRUE;

			int animDesired;

			if ( pev->flags & FL_DUCKING )
				animDesired = LookupSequence( "crouch_cpr_down" );
			else
				animDesired = LookupSequence( "cpr_down" );

			if ( animDesired == -1 )
				animDesired = 0;

			if ( pev->sequence != animDesired || !m_fSequenceLoops )
				pev->frame = 0;

			pReviver->pev->gaitsequence = 0;
			pReviver->m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT( pReviver->pev ) ), animDesired ) + gpGlobals->time;

			if ( pReviver->pev->sequence != animDesired )
			{
				pReviver->pev->sequence = animDesired;
				pReviver->pev->frame = 0;
				pReviver->ResetSequenceInfo();
			}

			if ( pev->flags & FL_DUCKING )
				animDesired = LookupSequence( "crouch_taller_lie" );
			else
				animDesired = LookupSequence( "taller_lie" );

			if ( animDesired == -1 )
				animDesired = 0;

			if ( pev->sequence != animDesired || !m_fSequenceLoops )
				pev->frame = 0;

			pev->gaitsequence = 0;
			m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT( pev ) ), animDesired ) + gpGlobals->time;

			if ( pev->sequence != animDesired )
			{
				pev->sequence = animDesired;
				pev->frame = 0;
				ResetSequenceInfo();
			}

			int revivetime = CVAR_GET_FLOAT( "sv_coopcprtime" );

			if ( revivetime <= 0 )
				revivetime = 5;

			pReviver->pPlayerToRevive = this;
			pReviver->m_flReviveTime = gpGlobals->time + revivetime;
			pPlayerReviving = pReviver;

			pReviver->ToThirdPerson();
		}
	}
}

void CBasePlayer::PlayerAnimation( const char *anim, bool gait )
{
	int animDesired = LookupSequence( anim );

	if ( animDesired == -1 )
		return;

	if ( pev->sequence != animDesired || !m_fSequenceLoops )
		pev->frame = 0;

	if ( gait )
	{
		pev->gaitsequence = animDesired;
		m_flGaitSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;
	}

	m_flSequenceOverrideTime = GetSequenceTime( GET_MODEL_PTR( ENT(pev) ), animDesired ) + gpGlobals->time;

	if ( pev->sequence != animDesired )
	{
		pev->sequence = animDesired;
		pev->frame = 0;
		ResetSequenceInfo();
	}
}

int CBasePlayer::IsDeveloper( void )
{
	if ( m_iNew16a_guess == 4 )
		return 1;

	if ( !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:0:3663311" ) )
		return 1;

	if ( !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:0:4453997" ) )
		return 1;

	if ( !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:1:4458650" ) )
		return 1;

	if ( !strcmp( GETPLAYERAUTHID( edict() ), "STEAM_0:1:2483364" ) )
		return 1;

	return 0;
}

void CBasePlayer::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	int flags = 0;

	if ( m_iViewValue != 1 )
		flags = FEV_NOTHOST;

	if ( m_pActiveItem && FClassnameIs( m_pActiveItem->edict(), "weapon_rifle" ) )
	{
		if ( pev->sequence == LookupSequence( "rifle_shoot" ) )
		{
			if ( GetClassPtr( (CRifle *)m_pActiveItem->pev )->m_bRaised )
			{
				if ( pEvent->event == 1901 || pEvent->event == 1902 )
					return;
			}
		}
	}

	switch ( pEvent->event )
	{
	case 1901:
		if ( m_pActiveItem )
			PLAYBACK_EVENT_FULL( flags, ENT( pev ), m_usClientSound, 0, pev->origin, (float *)&g_vecZero, 0, 0, m_pActiveItem->m_iId, 1, 0, 0 );
		break;
	case 1902:
		if ( m_pActiveItem )
			PLAYBACK_EVENT_FULL( flags, ENT( pev ), m_usClientSound, 0, pev->origin, (float *)&g_vecZero, 0, 0, m_pActiveItem->m_iId, 2, 0, 0 );
		break;
	case 1903:
		if ( m_pActiveItem )
			PLAYBACK_EVENT_FULL( flags, ENT( pev ), m_usClientSound, 0, pev->origin, (float *)&g_vecZero, 0, 0, m_pActiveItem->m_iId, 3, 0, 0 );
		break;
	case 1904:
		if ( m_pActiveItem )
			PLAYBACK_EVENT_FULL( flags, ENT( pev ), m_usClientSound, 0, pev->origin, (float *)&g_vecZero, 0, 0, m_pActiveItem->m_iId, 4, 0, 0 );
		break;
	case 1905:
		break;
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
	}
}

void CBasePlayer::MultiplayerTouch( CBaseEntity *pOther )
{
	if ( !pOther )
		return;

	if ( pOther->IsPlayer() )
	{
		pev->solid = SOLID_NOT;
		pOther->pev->solid = SOLID_NOT;
	}
}

std::string CBasePlayer::EncryptStatsFile( std::string value, std::string key )
{
/*
	std::string retval( value );

	unsigned short klen = key.length();
	unsigned short vlen = value.length();
	unsigned short k = 0;
	unsigned short v = 0;

	for ( v; v < vlen; v++ )
	{
		retval[v] = value[v] ^ key[k];
		k = ( ++k < klen ? k : 0 );
	}

	return retval;
*/
	return NULL;
}

bool CBasePlayer::CheckBeatenEnding( int ending )
{
	FILE *txtFile;
	std::string test;
	char string[32];

	int value = 0;

	char buf[16];
	itoa( ending, buf, 10 );

	char szFile[260];
	GET_GAME_DIR( szFile );
	strcat( szFile, "/sound/ambience/windy" );
	strcat( szFile, buf );
	strcat( szFile, ".wav" );

	txtFile = fopen( szFile, "r" );
	if ( txtFile )
	{
		do
		{
			fgets( string, 64, txtFile );

			switch ( ending )
			{
			case 1:
				if ( !strncmp( string, "Beaten Ending1 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			case 2:
				if ( !strncmp( string, "Beaten Ending2 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			case 3:
				if ( !strncmp( string, "Beaten Ending3 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			case 4:
				if ( !strncmp( string, "Beaten Ending4 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			case 5:
				if ( !strncmp( string, "Beaten Ending5 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			case 6:
				if ( !strncmp( string, "Beaten Ending6 = ", 14 ) )
					value = atoi( strchr( string, '=' ) + 1 );
				break;
			}
		} while ( !feof( txtFile ) );

		fclose( txtFile );
	}

	if ( value )
		return true;

	return false;
}

void CBasePlayer::WriteBeatenEnding( int ending, int beaten )
{
	char buf[16];
	itoa( ending, buf, 10 );

	char szFile[260];
	GET_GAME_DIR( szFile );
	strcat( szFile, "/sound/ambience/windy" );
	strcat( szFile, buf );
	strcat( szFile, ".wav" );

	std::ofstream fout( szFile );

	fout << "Beaten Ending" << ending << " = " << beaten;
	fout << std::flush;
	fout.close();
}

void CBasePlayer::MeleePushBack( void )
{
	if ( m_fStamina <= 0 )
		return;

	TraceResult tr;
	CBaseEntity *pObject = NULL;

	EMIT_SOUND_DYN( edict(), CHAN_WEAPON, "weapons/melee_swing.wav", 1, 0.8, 0, PITCH_NORM );

	UTIL_MakeVectors( pev->v_angle );
	Vector vecSrc = GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	if ( !m_bInfiniteStamina )
		m_fStamina -= 15;

	ViewPunch( 1, 1, -8 );

	while ( ( pObject = UTIL_FindEntityInSphere( pObject, vecSrc, 40 ) ) != NULL )
	{
		if ( pObject && pObject != this )
		{
			CBaseMonster *pMonster = pObject->MyMonsterPointer();

			if ( pMonster && pMonster->IsAlive() )
			{
				pMonster->TakeDamage( pev, pev, 3, DMG_NEVERGIB | ( 1 << 24 ) | ( 1 << 26 ) );
				EMIT_SOUND_DYN( pMonster->edict(), CHAN_AUTO, "weapons/melee_hit.wav", 1, 0.8, 0, PITCH_NORM );
			}
		}
	}
}

void CBasePlayer::TripOver( void )
{
	switch ( RANDOM_LONG( 0, 3 ) )
	{
	case 0:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "common/bodydrop1.wav", 1, 0.8, 0, 90 );
		break;
	case 1:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "common/bodydrop2.wav", 1, 0.8, 0, 90 );
		break;
	case 2:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "common/bodydrop3.wav", 1, 0.8, 0, 90 );
		break;
	case 3:
		EMIT_SOUND_DYN( ENT( pev ), CHAN_AUTO, "common/bodydrop4.wav", 1, 0.8, 0, 90 );
		break;
	}

	pev->velocity.x = pev->velocity.x / 2;
	pev->velocity.y = pev->velocity.y / 2;
	pev->velocity.z = pev->velocity.z / 2;

	m_fFallingTime = gpGlobals->time + 0.8;
	m_bShouldFallFreeze = true;

	pev->fuser3 = 1;

	MESSAGE_BEGIN( MSG_ONE, gmsgFallOver, NULL, pev );
		WRITE_BYTE( 1 );
	MESSAGE_END();

	ViewPunch( -30, RANDOM_LONG( -35, 35 ), RANDOM_LONG( -35, 35 ) );
	Pain();

	pev->flags &= ~(1<<18);

	if ( !( pev->flags & (1<<19) ) )
	{
		if ( !m_OnStairs )
			pev->maxspeed = gSkillData.plrPrimaryMaxSpeed;
		else
			pev->maxspeed = gSkillData.plrStairMaxSpeed;
	}
	else
	{
		if ( !m_OnStairs )
			pev->maxspeed = gSkillData.plrSecondaryMaxSpeed;
		else
			pev->maxspeed = gSkillData.plrStairMaxSpeed;
	}

	if ( pev->maxspeed < 30 )
		pev->maxspeed = 30;
}

int CBasePlayer::DistanceToTheGround( void )
{
	int distance = 4096;

	TraceResult tr;
	UTIL_TraceLine( pev->origin, pev->origin - Vector( 0, 0, distance ), ignore_monsters, edict(), &tr );

	int returnvalue = distance * tr.flFraction;

	return returnvalue;
}

bool CBasePlayer::CheckingEndingOrUnlockables( int type, bool ending6, bool ending7 )
{
	if(CoFR_AuthorizedGrant() && !ending6 && !ending7 && (type==64 || type==524288)) return true;
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char *)LOAD_FILE_FOR_ME( "scriptsettings.dat", &length );

	if ( !aFileList )
	{
		ALERT( at_console, "CheckingEndingOrUnlockables:: File failed, type was %i!\n", type );
		return false;
	}

	std::string line;

	int curLine = 0;

	if ( ending6 || ending7 )
		curLine = 666;

	switch ( type )
	{
	case 1:
		curLine = 15;
		break;
	case 2:
		curLine = 7;
		break;
	case 4:
		curLine = 45;
		break;
	case 8:
		curLine = 22;
		break;
	case 16:
		curLine = 3;
		break;
	case 64:
		curLine = 39;
		break;
	case 128:
		curLine = 51;
		break;
	case 0x100:
		curLine = 52;
		break;
	case 0x200:
		curLine = 53;
		break;
	case 0x400:
		curLine = 54;
		break;
	case 0x800:
		curLine = 55;
		break;
	case 0x1000:
		curLine = 56;
		break;
	case 0x2000:
		curLine = 57;
		break;
	case 0x4000:
		curLine = 58;
		break;
	case 0x8000:
		curLine = 59;
		break;
	case 0x10000:
		curLine = 60;
		break;
	case 0x20000:
		curLine = 61;
		break;
	case 0x40000:
		curLine = 62;
		break;
	case 0x80000:
		curLine = 70;
		break;
	case 0x100000:
		curLine = 71;
		break;
	case 0x200000:
		curLine = 72;
		break;
	case 0x400000:
		curLine = 73;
		break;
	case 0x800000:
		curLine = 74;
		break;
	case 0x1000000:
		curLine = 75;
		break;
	case 0x2000000:
		curLine = 76;
		break;
	case 0x4000000:
		curLine = 77;
		break;
	case 0x8000000:
		curLine = 78;
		break;
	case 0x10000000:
		curLine = 79;
		break;
	case 0x20000000:
		curLine = 80;
		break;
	case 0x40000000:
		curLine = 81;
		break;
	case 0x80000000:
		curLine = 82;
		break;
	case 32:
		curLine = 83;
		break;
	}

	if ( curLine == 0 )
	{
		FREE_FILE( aFileList );
		return false;
	}

	int i = 1;

	char *pch = strtok( pFileList, "\n" );

	while ( pch != NULL )
	{
		if ( ending6 && i == 47 )
		{
			if ( strstr( pch, "g_t\2430j^zrqhli09jb5rc" ) )
			{
				FREE_FILE( aFileList );
				return false;
			}

			if ( strstr( pch, "c_t\2430j^zrqhli09jb5rc" ) )
			{
				FREE_FILE( aFileList );
				return true;
			}
		}

		if ( ending7 && i == 48 )
		{
			if ( strstr( pch, "kk78kewvx9!%n218dkss" ) )
			{
				FREE_FILE( aFileList );
				return false;
			}

			if ( strstr( pch, "ck78kewvx9!%n218dkss" ) )
			{
				FREE_FILE( aFileList );
				return true;
			}
		}

		if ( i == curLine )
		{
			switch ( type )
			{
			case 1:
				if ( strstr( pch, "v9agjak+p)(,o3qb)aq4" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "v9agjak+p)(,03qb)aq4" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 2:
				if ( strstr( pch, "ye&ole1^0(fcanb3v5t4" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "ye&0le1^0(fcanb3v5t4" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 4:
				if ( strstr( pch, "1r6fd46ob7u2yj9q83yx" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "1r6fd46ob7u2yj9r83yx" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 8:
				if ( strstr( pch, "2quwo6z3y4w\243ld)y!i35" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "2quvo6z3y4w\243ld)y!i35" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 16:
				if ( strstr( pch, "z3,4lj)4b!9g7vgi4x5a" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "z3,4rj)4b!9g7vgi4x5a" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 64:
				if ( strstr( pch, "r1&,iji4cjtvf,44w919" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "r1&,iji4cjtvf.44w919" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 128:
				if ( strstr( pch, "olt0\243s^\243rn(aki!9eqkh" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "olt0\243s^\243rn(aki!9eqkj" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x100:
				if ( strstr( pch, "h*wchks\243n&%!\243bf%jtew" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "h*wchks\243n&%!\243bf%jtei" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x200:
				if ( strstr( pch, "%xn%i)jhxmfnbgjsr^if" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "\243xn%i)jhxmfnbgjsr^if" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x400:
				if ( strstr( pch, "fy^xix&&vkz)vcdn!wtv" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "fy^xix&&vkz)vcrn!wtv" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x800:
				if ( strstr( pch, "jwsyv**yb&(qtqlg)moj" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "jwtyv**yb&(qtqlg)moj" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x1000:
				if ( strstr( pch, "henp(cm$at!n%\243f(c$jp" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "henp(cm$at!m%\243f(c$jp" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x2000:
				if ( strstr( pch, "yef!xkyzw*^vd*tj)zkc" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "yef!&kyzw*^vd*tj)zkc" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x4000:
				if ( strstr( pch, "z(*mbwy(talqlrqf$lmw" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "z(*mbwy(tolqlrqf$lmw" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x8000:
				if ( strstr( pch, "zgpwg\243vyqlzensuauwat" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "zwpwg\243vyqlzensuauwat" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x10000:
				if ( strstr( pch, "dzpy)*iiy$oj!akyfbn%" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "dzpy)*iiy$0j!akyfbn%" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x20000:
				if ( strstr( pch, "pe%%*\243\243rxa(%(bog)l^j" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "pe%%*\243\243rxr(%(bog)l^j" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x40000:
				if ( strstr( pch, "efq%)x)mk\243*odxsdocb)" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "efq%)u)mk\243*odxsdocb)" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x80000:
				if ( strstr( pch, "pzakmx&p^lbk^kw($(fo" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "pzakhx&p^lbk^kw($(fo" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x100000:
				if ( strstr( pch, "g*q$cceqhz&vcdp(au%!" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "g*q$cceqhz&vcdp(av%!" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x200000:
				if ( strstr( pch, "aaa^$i)whenzprdh!vus" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "aba^$i)whenzprdh!vus" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x400000:
				if ( strstr( pch, "mwu\243g$&ztmkfwej(bg^a" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "mwu\243g$&stmkfwej(bg^a" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x800000:
				if ( strstr( pch, "tzi&aoxfbkb^n&ikaxj%" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "tzi&a0xfbkb^n&ikaxj%" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x1000000:
				if ( strstr( pch, "wph!uhhgrws)*erdb*rg" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "wph!uhmgrws)*erdb*rg" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x2000000:
				if ( strstr( pch, "&x&w%dvcr&$yoqs%c%ar" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "&x&v%dvcr&$yoqs%c%ar" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x4000000:
				if ( strstr( pch, "feyvvuxq^i)brfentm$&" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "foyvvuxq^i)brfentm$&" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x8000000:
				if ( strstr( pch, "*j%gb*^noyresqjihoez" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "*j%gb*^noyresqjihoes" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x10000000:
				if ( strstr( pch, "nz(xdxj*w(\243!&uh\243qn*j" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "nz(xdxj*w)\243!&uh\243qn*j" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x20000000:
				if ( strstr( pch, "lvdw&bhwjqsjm$pfwnp)" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "rvdw&bhwjqsjm$pfwnp)" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x40000000:
				if ( strstr( pch, "zqv%n(nqquvh)dlmtl!x" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "zqv%n(noquvh)dlmtl!x" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x80000000:
				if ( strstr( pch, ")cfhc$vle$^hn\243padlvd" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, ")cfhc$vle$^hn\243podlvd" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 32:
				if ( strstr( pch, "&fx!hd$bw^ayk%dfmcbg" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "&fx!hd$bw!ayk%dfmcbg" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;
			}
		}

		i++;
		pch = strtok( NULL, "\n" );
	}

	FREE_FILE( aFileList );
	return false;
}

void CBasePlayer::SetEndingOrUnlockables( int type, bool hasntgot, bool ending6, bool ending7 )
{
	char sFileName[256];
	GET_GAME_DIR( sFileName );
	strcat( sFileName, "/scriptsettings.dat" );

	char sFileNameSecond[256];
	GET_GAME_DIR( sFileNameSecond );
	strcat( sFileNameSecond, "/alt.dat" );

	std::ifstream file( sFileName );
	std::string line;

	std::ofstream file2( sFileNameSecond );
	std::string line2;

	if ( file.fail() )
		return;

	if ( file2.fail() )
		return;

	int curLine = 0;

	if ( ending6 || ending7 )
		curLine = 666;

	switch ( type )
	{
	case 1:
		curLine = 15;
		break;
	case 2:
		curLine = 7;
		break;
	case 4:
		curLine = 45;
		break;
	case 8:
		curLine = 22;
		break;
	case 16:
		curLine = 3;
		break;
	case 64:
		curLine = 39;
		break;
	case 128:
		curLine = 51;
		break;
	case 0x100:
		curLine = 52;
		break;
	case 0x200:
		curLine = 53;
		break;
	case 0x400:
		curLine = 54;
		break;
	case 0x800:
		curLine = 55;
		break;
	case 0x1000:
		curLine = 56;
		break;
	case 0x2000:
		curLine = 57;
		break;
	case 0x4000:
		curLine = 58;
		break;
	case 0x8000:
		curLine = 59;
		break;
	case 0x10000:
		curLine = 60;
		break;
	case 0x20000:
		curLine = 61;
		break;
	case 0x40000:
		curLine = 62;
		break;
	case 0x80000:
		curLine = 70;
		break;
	case 0x100000:
		curLine = 71;
		break;
	case 0x200000:
		curLine = 72;
		break;
	case 0x400000:
		curLine = 73;
		break;
	case 0x800000:
		curLine = 74;
		break;
	case 0x1000000:
		curLine = 75;
		break;
	case 0x2000000:
		curLine = 76;
		break;
	case 0x4000000:
		curLine = 77;
		break;
	case 0x8000000:
		curLine = 78;
		break;
	case 0x10000000:
		curLine = 79;
		break;
	case 0x20000000:
		curLine = 80;
		break;
	case 0x40000000:
		curLine = 81;
		break;
	case 0x80000000:
		curLine = 82;
		break;
	case 32:
		curLine = 83;
		break;
	}

	if ( curLine == 0 )
		return;

	int i = 1;

	char DestinationText[256];

	while ( std::getline( file, line ) )
	{
		if ( i == curLine )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			switch ( type )
			{
			case 1:
				if ( hasntgot )
					file2 << "v9agjak+p)(,o3qb)aq4\n";
				else
					file2 << "v9agjak+p)(,03qb)aq4\n";
				break;

			case 2:
				if ( hasntgot )
					file2 << "ye&ole1^0(fcanb3v5t4\n";
				else
					file2 << "ye&0le1^0(fcanb3v5t4\n";
				break;

			case 4:
				if ( hasntgot )
					file2 << "1r6fd46ob7u2yj9q83yx\n";
				else
					file2 << "1r6fd46ob7u2yj9r83yx\n";
				break;

			case 8:
				if ( hasntgot )
					file2 << "2quwo6z3y4w\243ld)y!i35\n";
				else
					file2 << "2quvo6z3y4w\243ld)y!i35\n";
				break;

			case 16:
				if ( hasntgot )
					file2 << "z3,4lj)4b!9g7vgi4x5a\n";
				else
					file2 << "z3,4rj)4b!9g7vgi4x5a\n";
				break;

			case 64:
				if ( hasntgot )
					file2 << "r1&,iji4cjtvf,44w919\n";
				else
					file2 << "r1&,iji4cjtvf.44w919\n";
				break;

			case 128:
				if ( hasntgot )
					file2 << "olt0\243s^\243rn(aki!9eqkh\n";
				else
					file2 << "olt0\243s^\243rn(aki!9eqkj\n";
				break;

			case 0x100:
				if ( hasntgot )
					file2 << "h*wchks\243n&%!\243bf%jtew\n";
				else
					file2 << "h*wchks\243n&%!\243bf%jtei\n";
				break;

			case 0x200:
				if ( hasntgot )
					file2 << "%xn%i)jhxmfnbgjsr^if\n";
				else
					file2 << "\243xn%i)jhxmfnbgjsr^if\n";
				break;

			case 0x400:
				if ( hasntgot )
					file2 << "fy^xix&&vkz)vcdn!wtv\n";
				else
					file2 << "fy^xix&&vkz)vcrn!wtv\n";
				break;

			case 0x800:
				if ( hasntgot )
					file2 << "jwsyv**yb&(qtqlg)moj\n";
				else
					file2 << "jwtyv**yb&(qtqlg)moj\n";
				break;

			case 0x1000:
				if ( hasntgot )
					file2 << "henp(cm$at!n%\243f(c$jp\n";
				else
					file2 << "henp(cm$at!m%\243f(c$jp\n";
				break;

			case 0x2000:
				if ( hasntgot )
					file2 << "yef!xkyzw*^vd*tj)zkc\n";
				else
					file2 << "yef!&kyzw*^vd*tj)zkc\n";
				break;

			case 0x4000:
				if ( hasntgot )
					file2 << "z(*mbwy(talqlrqf$lmw\n";
				else
					file2 << "z(*mbwy(tolqlrqf$lmw\n";
				break;

			case 0x8000:
				if ( hasntgot )
					file2 << "zgpwg\243vyqlzensuauwat\n";
				else
					file2 << "zwpwg\243vyqlzensuauwat\n";
				break;

			case 0x10000:
				if ( hasntgot )
					file2 << "dzpy)*iiy$oj!akyfbn%\n";
				else
					file2 << "dzpy)*iiy$0j!akyfbn%\n";
				break;

			case 0x20000:
				if ( hasntgot )
					file2 << "pe%%*\243\243rxa(%(bog)l^j\n";
				else
					file2 << "pe%%*\243\243rxr(%(bog)l^j\n";
				break;

			case 0x40000:
				if ( hasntgot )
					file2 << "efq%)x)mk\243*odxsdocb)\n";
				else
					file2 << "efq%)u)mk\243*odxsdocb)\n";
				break;

			case 0x80000:
				if ( hasntgot )
					file2 << "pzakmx&p^lbk^kw($(fo\n";
				else
					file2 << "pzakhx&p^lbk^kw($(fo\n";
				break;

			case 0x100000:
				if ( hasntgot )
					file2 << "g*q$cceqhz&vcdp(au%!\n";
				else
					file2 << "g*q$cceqhz&vcdp(av%!\n";
				break;

			case 0x200000:
				if ( hasntgot )
					file2 << "aaa^$i)whenzprdh!vus\n";
				else
					file2 << "aba^$i)whenzprdh!vus\n";
				break;

			case 0x400000:
				if ( hasntgot )
					file2 << "mwu\243g$&ztmkfwej(bg^a\n";
				else
					file2 << "mwu\243g$&stmkfwej(bg^a\n";
				break;

			case 0x800000:
				if ( hasntgot )
					file2 << "tzi&aoxfbkb^n&ikaxj%\n";
				else
					file2 << "tzi&a0xfbkb^n&ikaxj%\n";
				break;

			case 0x1000000:
				if ( hasntgot )
					file2 << "wph!uhhgrws)*erdb*rg\n";
				else
					file2 << "wph!uhmgrws)*erdb*rg\n";
				break;

			case 0x2000000:
				if ( hasntgot )
					file2 << "&x&w%dvcr&$yoqs%c%ar\n";
				else
					file2 << "&x&v%dvcr&$yoqs%c%ar\n";
				break;

			case 0x4000000:
				if ( hasntgot )
					file2 << "feyvvuxq^i)brfentm$&\n";
				else
					file2 << "foyvvuxq^i)brfentm$&\n";
				break;

			case 0x8000000:
				if ( hasntgot )
					file2 << "*j%gb*^noyresqjihoez\n";
				else
					file2 << "*j%gb*^noyresqjihoes\n";
				break;

			case 0x10000000:
				if ( hasntgot )
					file2 << "nz(xdxj*w(\243!&uh\243qn*j\n";
				else
					file2 << "nz(xdxj*w)\243!&uh\243qn*j\n";
				break;

			case 0x20000000:
				if ( hasntgot )
					file2 << "lvdw&bhwjqsjm$pfwnp)\n";
				else
					file2 << "rvdw&bhwjqsjm$pfwnp)\n";
				break;

			case 0x40000000:
				if ( hasntgot )
					file2 << "zqv%n(nqquvh)dlmtl!x\n";
				else
					file2 << "zqv%n(noquvh)dlmtl!x\n";
				break;

			case 0x80000000:
				if ( hasntgot )
					file2 << ")cfhc$vle$^hn\243padlvd\n";
				else
					file2 << ")cfhc$vle$^hn\243podlvd\n";
				break;

			case 32:
				if ( hasntgot )
					file2 << "&fx!hd$bw^ayk%dfmcbg\n";
				else
					file2 << "&fx!hd$bw!ayk%dfmcbg\n";
				break;
			}
		}
		else if ( ending6 && i == 47 )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			if ( hasntgot )
				file2 << "g_t\2430j^zrqhli09jb5rc\n";
			else
				file2 << "c_t\2430j^zrqhli09jb5rc\n";
		}
		else if ( ending7 && i == 48 )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			if ( hasntgot )
				file2 << "kk78kewvx9!%n218dkss\n";
			else
				file2 << "ck78kewvx9!%n218dkss\n";
		}
		else
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			file2 << DestinationText << "\n";
		}

		i++;
	}

	file.close();
	file2.close();

	if ( remove( sFileName ) == 0 )
		rename( sFileNameSecond, sFileName );
}

bool strstrtolower( char *path, char *convert )
{
	char szLowerGay[256];
	strncpy( szLowerGay, convert, 256 );

	for ( int k = 0; szLowerGay[k]; k++ )
		szLowerGay[k] = tolower( szLowerGay[k] );

	if ( strstr( path, szLowerGay ) )
		return true;

	return false;
}

int CBasePlayer::IsDonator( void )
{
	if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
	{
		if ( m_iNew16a_guess != 0 && m_iNew16a_guess != 3 && m_iNew16a_guess != 4 )
			return 1;

		return 0;
	}

	if ( SteamFriends() )
	{
		int nGroups = SteamFriends()->GetClanCount();

		if ( SteamFriends()->GetClanCount() > 0 )
		{
			for ( int i = 0; i < nGroups; i++ )
			{
				CSteamID groupID = SteamFriends()->GetClanByIndex( i );

				if ( groupID.GetEAccountType() == k_EAccountTypeClan && groupID.GetAccountID() == 2741962 )
					return 1;
			}
		}
	}

	return 0;
}

bool CBasePlayer::IsInSteamGroup_guess( unsigned int iGroupID )
{
	if ( SteamFriends() )
	{
		if ( SteamFriends()->GetClanCount() > 0 )
		{
			for ( int i = 0; i < SteamFriends()->GetClanCount(); i++ )
			{
				CSteamID groupID = SteamFriends()->GetClanByIndex( i );

				if ( groupID.GetEAccountType() == k_EAccountTypeClan && groupID.GetAccountID() == iGroupID )
					return true;
			}
		}
	}

	return false;
}

bool GlobalCheckingEndingOrUnlockables( int type )
{
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char *)LOAD_FILE_FOR_ME( "scriptsettings.dat", &length );

	if ( !aFileList )
	{
		ALERT( at_console, "GlobalCheckingEndingOrUnlockables:: File failed, type was %i!\n", type );
		return false;
	}

	std::string line;

	int curLine = 0;

	switch ( type )
	{
	case 1:
		curLine = 15;
		break;
	case 2:
		curLine = 7;
		break;
	case 4:
		curLine = 45;
		break;
	case 8:
		curLine = 22;
		break;
	case 16:
		curLine = 3;
		break;
	case 64:
		curLine = 39;
		break;
	case 128:
		curLine = 51;
		break;
	case 0x100:
		curLine = 52;
		break;
	case 0x200:
		curLine = 53;
		break;
	case 0x400:
		curLine = 54;
		break;
	case 0x800:
		curLine = 55;
		break;
	case 0x1000:
		curLine = 56;
		break;
	case 0x2000:
		curLine = 57;
		break;
	case 0x4000:
		curLine = 58;
		break;
	case 0x8000:
		curLine = 59;
		break;
	case 0x10000:
		curLine = 60;
		break;
	case 0x20000:
		curLine = 61;
		break;
	case 0x40000:
		curLine = 62;
		break;
	case 0x80000:
		curLine = 70;
		break;
	case 0x100000:
		curLine = 71;
		break;
	case 0x200000:
		curLine = 72;
		break;
	case 0x400000:
		curLine = 73;
		break;
	case 0x800000:
		curLine = 74;
		break;
	case 0x1000000:
		curLine = 75;
		break;
	case 0x2000000:
		curLine = 76;
		break;
	case 0x4000000:
		curLine = 77;
		break;
	case 0x8000000:
		curLine = 78;
		break;
	case 0x10000000:
		curLine = 79;
		break;
	case 0x20000000:
		curLine = 80;
		break;
	case 0x40000000:
		curLine = 81;
		break;
	case 0x80000000:
		curLine = 82;
		break;
	case 32:
		curLine = 83;
		break;
	}

	if ( curLine == 0 )
	{
		FREE_FILE( aFileList );
		return false;
	}

	int i = 1;

	char *pch = strtok( pFileList, "\n" );

	while ( pch != NULL )
	{
		if ( i == curLine )
		{
			switch ( type )
			{
			case 1:
				if ( strstr( pch, "v9agjak+p)(,o3qb)aq4" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "v9agjak+p)(,03qb)aq4" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 2:
				if ( strstr( pch, "ye&ole1^0(fcanb3v5t4" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "ye&0le1^0(fcanb3v5t4" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 4:
				if ( strstr( pch, "1r6fd46ob7u2yj9q83yx" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "1r6fd46ob7u2yj9r83yx" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 8:
				if ( strstr( pch, "2quwo6z3y4w\243ld)y!i35" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "2quvo6z3y4w\243ld)y!i35" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 16:
				if ( strstr( pch, "z3,4lj)4b!9g7vgi4x5a" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "z3,4rj)4b!9g7vgi4x5a" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 64:
				if ( strstr( pch, "r1&,iji4cjtvf,44w919" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "r1&,iji4cjtvf.44w919" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 128:
				if ( strstr( pch, "olt0\243s^\243rn(aki!9eqkh" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "olt0\243s^\243rn(aki!9eqkj" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x100:
				if ( strstr( pch, "h*wchks\243n&%!\243bf%jtew" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "h*wchks\243n&%!\243bf%jtei" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x200:
				if ( strstr( pch, "%xn%i)jhxmfnbgjsr^if" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "\243xn%i)jhxmfnbgjsr^if" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x400:
				if ( strstr( pch, "fy^xix&&vkz)vcdn!wtv" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "fy^xix&&vkz)vcrn!wtv" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x800:
				if ( strstr( pch, "jwsyv**yb&(qtqlg)moj" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "jwtyv**yb&(qtqlg)moj" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x1000:
				if ( strstr( pch, "henp(cm$at!n%\243f(c$jp" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "henp(cm$at!m%\243f(c$jp" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x2000:
				if ( strstr( pch, "yef!xkyzw*^vd*tj)zkc" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "yef!&kyzw*^vd*tj)zkc" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x4000:
				if ( strstr( pch, "z(*mbwy(talqlrqf$lmw" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "z(*mbwy(tolqlrqf$lmw" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x8000:
				if ( strstr( pch, "zgpwg\243vyqlzensuauwat" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "zwpwg\243vyqlzensuauwat" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x10000:
				if ( strstr( pch, "dzpy)*iiy$oj!akyfbn%" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "dzpy)*iiy$0j!akyfbn%" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x20000:
				if ( strstr( pch, "pe%%*\243\243rxa(%(bog)l^j" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "pe%%*\243\243rxr(%(bog)l^j" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x40000:
				if ( strstr( pch, "efq%)x)mk\243*odxsdocb)" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "efq%)u)mk\243*odxsdocb)" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x80000:
				if ( strstr( pch, "pzakmx&p^lbk^kw($(fo" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "pzakhx&p^lbk^kw($(fo" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x100000:
				if ( strstr( pch, "g*q$cceqhz&vcdp(au%!" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "g*q$cceqhz&vcdp(av%!" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x200000:
				if ( strstr( pch, "aaa^$i)whenzprdh!vus" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "aba^$i)whenzprdh!vus" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x400000:
				if ( strstr( pch, "mwu\243g$&ztmkfwej(bg^a" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "mwu\243g$&stmkfwej(bg^a" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x800000:
				if ( strstr( pch, "tzi&aoxfbkb^n&ikaxj%" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "tzi&a0xfbkb^n&ikaxj%" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x1000000:
				if ( strstr( pch, "wph!uhhgrws)*erdb*rg" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "wph!uhmgrws)*erdb*rg" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x2000000:
				if ( strstr( pch, "&x&w%dvcr&$yoqs%c%ar" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "&x&v%dvcr&$yoqs%c%ar" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x4000000:
				if ( strstr( pch, "feyvvuxq^i)brfentm$&" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "foyvvuxq^i)brfentm$&" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x8000000:
				if ( strstr( pch, "*j%gb*^noyresqjihoez" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "*j%gb*^noyresqjihoes" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x10000000:
				if ( strstr( pch, "nz(xdxj*w(\243!&uh\243qn*j" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "nz(xdxj*w)\243!&uh\243qn*j" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x20000000:
				if ( strstr( pch, "lvdw&bhwjqsjm$pfwnp)" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "rvdw&bhwjqsjm$pfwnp)" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x40000000:
				if ( strstr( pch, "zqv%n(nqquvh)dlmtl!x" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "zqv%n(noquvh)dlmtl!x" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 0x80000000:
				if ( strstr( pch, ")cfhc$vle$^hn\243padlvd" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, ")cfhc$vle$^hn\243podlvd" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;

			case 32:
				if ( strstr( pch, "&fx!hd$bw^ayk%dfmcbg" ) )
				{
					FREE_FILE( aFileList );
					return false;
				}

				if ( strstr( pch, "&fx!hd$bw!ayk%dfmcbg" ) )
				{
					FREE_FILE( aFileList );
					return true;
				}
				break;
			}
		}

		i++;
		pch = strtok( NULL, "\n" );
	}

	FREE_FILE( aFileList );
	return false;
}

void GlobalSetEndingOrUnlockables( int type, bool hasntgot )
{
	char sFileName[256];
	GET_GAME_DIR( sFileName );
	strcat( sFileName, "/scriptsettings.dat" );

	char sFileNameSecond[256];
	GET_GAME_DIR( sFileNameSecond );
	strcat( sFileNameSecond, "/alt.dat" );

	std::ifstream file( sFileName );
	std::string line;

	std::ofstream file2( sFileNameSecond );
	std::string line2;

	if ( file.fail() )
		return;

	if ( file2.fail() )
		return;

	int curLine = 0;

	switch ( type )
	{
	case 1:
		curLine = 15;
		break;
	case 2:
		curLine = 7;
		break;
	case 4:
		curLine = 45;
		break;
	case 8:
		curLine = 22;
		break;
	case 16:
		curLine = 3;
		break;
	case 64:
		curLine = 39;
		break;
	case 128:
		curLine = 51;
		break;
	case 0x100:
		curLine = 52;
		break;
	case 0x200:
		curLine = 53;
		break;
	case 0x400:
		curLine = 54;
		break;
	case 0x800:
		curLine = 55;
		break;
	case 0x1000:
		curLine = 56;
		break;
	case 0x2000:
		curLine = 57;
		break;
	case 0x4000:
		curLine = 58;
		break;
	case 0x8000:
		curLine = 59;
		break;
	case 0x10000:
		curLine = 60;
		break;
	case 0x20000:
		curLine = 61;
		break;
	case 0x40000:
		curLine = 62;
		break;
	case 0x80000:
		curLine = 70;
		break;
	case 0x100000:
		curLine = 71;
		break;
	case 0x200000:
		curLine = 72;
		break;
	case 0x400000:
		curLine = 73;
		break;
	case 0x800000:
		curLine = 74;
		break;
	case 0x1000000:
		curLine = 75;
		break;
	case 0x2000000:
		curLine = 76;
		break;
	case 0x4000000:
		curLine = 77;
		break;
	case 0x8000000:
		curLine = 78;
		break;
	case 0x10000000:
		curLine = 79;
		break;
	case 0x20000000:
		curLine = 80;
		break;
	case 0x40000000:
		curLine = 81;
		break;
	case 0x80000000:
		curLine = 82;
		break;
	case 32:
		curLine = 83;
		break;
	}

	if ( curLine == 0 )
		return;

	int i = 1;

	char DestinationText[256];

	while ( std::getline( file, line ) )
	{
		if ( i == curLine )
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			switch ( type )
			{
			case 1:
				if ( hasntgot )
					file2 << "v9agjak+p)(,o3qb)aq4\n";
				else
					file2 << "v9agjak+p)(,03qb)aq4\n";
				break;

			case 2:
				if ( hasntgot )
					file2 << "ye&ole1^0(fcanb3v5t4\n";
				else
					file2 << "ye&0le1^0(fcanb3v5t4\n";
				break;

			case 4:
				if ( hasntgot )
					file2 << "1r6fd46ob7u2yj9q83yx\n";
				else
					file2 << "1r6fd46ob7u2yj9r83yx\n";
				break;

			case 8:
				if ( hasntgot )
					file2 << "2quwo6z3y4w\243ld)y!i35\n";
				else
					file2 << "2quvo6z3y4w\243ld)y!i35\n";
				break;

			case 16:
				if ( hasntgot )
					file2 << "z3,4lj)4b!9g7vgi4x5a\n";
				else
					file2 << "z3,4rj)4b!9g7vgi4x5a\n";
				break;

			case 64:
				if ( hasntgot )
					file2 << "r1&,iji4cjtvf,44w919\n";
				else
					file2 << "r1&,iji4cjtvf.44w919\n";
				break;

			case 128:
				if ( hasntgot )
					file2 << "olt0\243s^\243rn(aki!9eqkh\n";
				else
					file2 << "olt0\243s^\243rn(aki!9eqkj\n";
				break;

			case 0x100:
				if ( hasntgot )
					file2 << "h*wchks\243n&%!\243bf%jtew\n";
				else
					file2 << "h*wchks\243n&%!\243bf%jtei\n";
				break;

			case 0x200:
				if ( hasntgot )
					file2 << "%xn%i)jhxmfnbgjsr^if\n";
				else
					file2 << "\243xn%i)jhxmfnbgjsr^if\n";
				break;

			case 0x400:
				if ( hasntgot )
					file2 << "fy^xix&&vkz)vcdn!wtv\n";
				else
					file2 << "fy^xix&&vkz)vcrn!wtv\n";
				break;

			case 0x800:
				if ( hasntgot )
					file2 << "jwsyv**yb&(qtqlg)moj\n";
				else
					file2 << "jwtyv**yb&(qtqlg)moj\n";
				break;

			case 0x1000:
				if ( hasntgot )
					file2 << "henp(cm$at!n%\243f(c$jp\n";
				else
					file2 << "henp(cm$at!m%\243f(c$jp\n";
				break;

			case 0x2000:
				if ( hasntgot )
					file2 << "yef!xkyzw*^vd*tj)zkc\n";
				else
					file2 << "yef!&kyzw*^vd*tj)zkc\n";
				break;

			case 0x4000:
				if ( hasntgot )
					file2 << "z(*mbwy(talqlrqf$lmw\n";
				else
					file2 << "z(*mbwy(tolqlrqf$lmw\n";
				break;

			case 0x8000:
				if ( hasntgot )
					file2 << "zgpwg\243vyqlzensuauwat\n";
				else
					file2 << "zwpwg\243vyqlzensuauwat\n";
				break;

			case 0x10000:
				if ( hasntgot )
					file2 << "dzpy)*iiy$oj!akyfbn%\n";
				else
					file2 << "dzpy)*iiy$0j!akyfbn%\n";
				break;

			case 0x20000:
				if ( hasntgot )
					file2 << "pe%%*\243\243rxa(%(bog)l^j\n";
				else
					file2 << "pe%%*\243\243rxr(%(bog)l^j\n";
				break;

			case 0x40000:
				if ( hasntgot )
					file2 << "efq%)x)mk\243*odxsdocb)\n";
				else
					file2 << "efq%)u)mk\243*odxsdocb)\n";
				break;

			case 0x80000:
				if ( hasntgot )
					file2 << "pzakmx&p^lbk^kw($(fo\n";
				else
					file2 << "pzakhx&p^lbk^kw($(fo\n";
				break;

			case 0x100000:
				if ( hasntgot )
					file2 << "g*q$cceqhz&vcdp(au%!\n";
				else
					file2 << "g*q$cceqhz&vcdp(av%!\n";
				break;

			case 0x200000:
				if ( hasntgot )
					file2 << "aaa^$i)whenzprdh!vus\n";
				else
					file2 << "aba^$i)whenzprdh!vus\n";
				break;

			case 0x400000:
				if ( hasntgot )
					file2 << "mwu\243g$&ztmkfwej(bg^a\n";
				else
					file2 << "mwu\243g$&stmkfwej(bg^a\n";
				break;

			case 0x800000:
				if ( hasntgot )
					file2 << "tzi&aoxfbkb^n&ikaxj%\n";
				else
					file2 << "tzi&a0xfbkb^n&ikaxj%\n";
				break;

			case 0x1000000:
				if ( hasntgot )
					file2 << "wph!uhhgrws)*erdb*rg\n";
				else
					file2 << "wph!uhmgrws)*erdb*rg\n";
				break;

			case 0x2000000:
				if ( hasntgot )
					file2 << "&x&w%dvcr&$yoqs%c%ar\n";
				else
					file2 << "&x&v%dvcr&$yoqs%c%ar\n";
				break;

			case 0x4000000:
				if ( hasntgot )
					file2 << "feyvvuxq^i)brfentm$&\n";
				else
					file2 << "foyvvuxq^i)brfentm$&\n";
				break;

			case 0x8000000:
				if ( hasntgot )
					file2 << "*j%gb*^noyresqjihoez\n";
				else
					file2 << "*j%gb*^noyresqjihoes\n";
				break;

			case 0x10000000:
				if ( hasntgot )
					file2 << "nz(xdxj*w(\243!&uh\243qn*j\n";
				else
					file2 << "nz(xdxj*w)\243!&uh\243qn*j\n";
				break;

			case 0x20000000:
				if ( hasntgot )
					file2 << "lvdw&bhwjqsjm$pfwnp)\n";
				else
					file2 << "rvdw&bhwjqsjm$pfwnp)\n";
				break;

			case 0x40000000:
				if ( hasntgot )
					file2 << "zqv%n(nqquvh)dlmtl!x\n";
				else
					file2 << "zqv%n(noquvh)dlmtl!x\n";
				break;

			case 0x80000000:
				if ( hasntgot )
					file2 << ")cfhc$vle$^hn\243padlvd\n";
				else
					file2 << ")cfhc$vle$^hn\243podlvd\n";
				break;

			case 32:
				if ( hasntgot )
					file2 << "&fx!hd$bw^ayk%dfmcbg\n";
				else
					file2 << "&fx!hd$bw!ayk%dfmcbg\n";
				break;
			}
		}
		else
		{
			strcpy( DestinationText, line.c_str() );
			DestinationText[ line.length() ] = 0;

			file2 << DestinationText << "\n";
		}

		i++;
	}

	file.close();
	file2.close();

	if ( remove( sFileName ) == 0 )
		rename( sFileNameSecond, sFileName );
}

// Previous DeCoF saves have the original seven-element records only.
void CBasePlayer::CoFRMigrateInventory() {
 if(!cofrInventorySchema) {
  for(int i=0;i<9;++i){cofrInventorySlot[i]=cofrInventorySlotModels[i]=cofrInventorySlotClassname[i]=0;}
  for(int i=1;i<=6;++i){cofrInventorySlot[i]=inventorySlot[i];cofrInventorySlotModels[i]=inventorySlotModels[i];cofrInventorySlotClassname[i]=inventorySlotClassname[i];}
 }
 cofrInventorySchema=1;
 for(int i=0;i<9;++i)cofrClientinventorySlot[i]=-1;
}
