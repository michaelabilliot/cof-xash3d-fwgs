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
#ifndef PLAYER_H
#define PLAYER_H


#include "pm_materials.h"
#include "cofr_stair_sprint.hpp"
#include <string>


//LRC - code for Werner Spahl's mod.
//#define XENWARRIOR

#ifdef XENWARRIOR
#define SOUND_FLASHLIGHT_IDLE   "ambience/alien_clicker1.wav"
#define LF_FLASH_RESUME (1<<13)
#define LF_FLASH_RESUME2 (1<<14)

extern float g_fEnvFadeTime;
#endif



#define PLAYER_FATAL_FALL_SPEED		1024// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		(float) 100 / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED		200
#define PLAYER_FALL_PUNCH_THRESHHOLD (float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.

//
// Player PHYSICS FLAGS bits
//
#define		PFLAG_ONLADDER		( 1<<0 )
#define		PFLAG_ONSWING		( 1<<0 )
#define		PFLAG_ONTRAIN		( 1<<1 )
#define		PFLAG_ONBARNACLE	( 1<<2 )
#define		PFLAG_DUCKING		( 1<<3 )		// In the process of ducking, but totally squatted yet
#define		PFLAG_USING			( 1<<4 )		// Using a continuous entity
#define		PFLAG_OBSERVER		( 1<<5 )		// player is locked in stationary cam mode. Spectators can move, observers can't.

//
// generic player
//
//-----------------------------------------------------
//This is Half-Life player entity
//-----------------------------------------------------
#define CSUITPLAYLIST	4		// max of 4 suit sentences queued up at any time

#define SUIT_GROUP			TRUE
#define	SUIT_SENTENCE		FALSE

#define	SUIT_REPEAT_OK		0
#define SUIT_NEXT_IN_30SEC	30
#define SUIT_NEXT_IN_1MIN	60
#define SUIT_NEXT_IN_5MIN	300
#define SUIT_NEXT_IN_10MIN	600
#define SUIT_NEXT_IN_30MIN	1800
#define SUIT_NEXT_IN_1HOUR	3600

#define CSUITNOREPEAT		32

#define	SOUND_FLASHLIGHT_ON		"items/flashlight1.wav"
#define	SOUND_FLASHLIGHT_OFF	"items/flashlight1.wav"

// buz: gasmask sounds
#define SOUND_GASMASK_ON	"items/gasmask1.wav"
#define SOUND_GASMASK_OFF	"items/gasmask2.wav"
// buz: headshield sounds
#define SOUND_SHIELD_ON		"items/headshield1.wav"
#define SOUND_SHIELD_OFF	"items/headshield2.wav"

#define TEAM_NAME_LENGTH	16

typedef enum
{
	PLAYER_IDLE,
	PLAYER_WALK,
	PLAYER_JUMP,
	PLAYER_SUPERJUMP,
	PLAYER_DIE,
	PLAYER_ATTACK1,
} PLAYER_ANIM;


//NB: changing this structure will cause problems! --LRC

#define MAX_ID_RANGE 2048
#define SBAR_STRING_SIZE 128

enum sbar_data
{
	SBAR_ID_TARGETNAME = 1,
	SBAR_ID_TARGETHEALTH,
	SBAR_ID_TARGETARMOR,
	SBAR_END,
};

#define CHAT_INTERVAL 1.0f

class CBasePlayerWeapon;
class CCoFDocument;
class CRealCamera;

struct statistics
{
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
	bool can_unlock_shit;
	int chosen_difficulty;
	bool doctor_mode;
	int language;
};

struct documents
{
	char page1[16];
	char page2[16];
	char page3[16];
	char page4[16];
	char note1[16];
	char note2[16];
	char note3[16];
	char note4[16];
	char turnsound[48];
	char title[64];
	char label[48];
};

class CBasePlayer : public CBaseMonster
{
public:
	EHANDLE m_pTelephone;
	EHANDLE m_pTelescope;
	EHANDLE m_pPuzzleBar;
	EHANDLE m_pBoat;
	EHANDLE m_pAttachedCamera;
	EHANDLE m_pWheelchair;
	// CoF 1.6a: new members (see docs/structs.md, CBasePlayer)
	bool m_bMuschi2_guess;				// toggled by the "muschi2" client command, set by CRoofBoss
	bool m_bUsedVanNumber;
	float m_flPhoneNumberTime_guess;	// set in PhoneNumber, checked in PostThink
	int m_iPhoneNumberState_guess;		// 0, 1 (PhoneNumber), 2 (PostThink)
	int m_iCoopDeadYaw;
	bool m_bKilledLyingDown;
	bool m_bCustomSleeveApplied;
	bool m_bJustJumped;
	float m_flMaxJustJumpedTime;
	bool m_bIsADonator;
	int m_iAllowedToUseTMP;
	int m_iIsAFaggot;
	bool m_bHoldingBreath;
	float m_flNextBreathHoldTime;
	int m_iCoopStuckPushYaw;
	bool m_bCoopWaitingForCutsceneToEnd;
	float m_flNextCoopWaitingForCutsceneFadeTime;
	float m_flNextDeadNotifyTime;
	float m_flCoopRespawnDelayTime;
	int m_iQuickSlotSlot[4];
	float m_flNextAdLibTime;
	float m_flNextNightVisionTime;
	CBasePlayerWeapon *m_pStoredNightVisionWeapon;
	bool m_bFirstDeploy[33];
	int m_iCurrentWeaponAnimation;
	int m_iCurrentWeaponAnimationDual;
	string_t m_sCustomObjectives;
	int m_iUnlockedClothes;
	int m_iClientUnlockedClothes;
	int m_iFoundHoodies;
	float m_flDodgeRechargeTime;
	bool CheckingEndingOrUnlockables( int type, bool ending6, bool ending7 );
	void SetEndingOrUnlockables( int type, bool hasntgot, bool ending6, bool ending7 );
	bool m_bHasNightVision;
	bool m_bChoking;
	void TripOver( void );
	int DistanceToTheGround( void );
	void EXPORT MeleePushBack( void );
	float m_flNextTimeTakenCheck;
	bool m_bLowHealth;
	float m_flNextMorphineDreamTime;
	int m_iOwnedWeaponsList[32];
	float m_flNextPunchTime;
	int m_iPlayerSaveLock;
	bool m_bResetGame;
	bool CheckBeatenEnding( int ending );
	void WriteBeatenEnding( int ending, int beaten );
	std::string EncryptStatsFile( std::string value, std::string key );
	bool m_b_Ending_RoofBossKilled;
	bool m_b_Ending_GivenP345;
	bool m_b_Ending_DeliveredPackage;
	float m_flCylinderRotateTime;
	float m_flNextHeartbeatTime;
	float m_flNextMPPlayerNameCheck;
	int m_iLookingAtPlayer;
	float m_flPlayerNameDieTime;
	char *szIPAddress;
	bool m_bCoopGameOver;
	float m_flNextCountDown;
	int m_iGameOverCounter;
	int IsDeveloper( void );
	int IsDonator( void );
	bool IsInSteamGroup_guess( unsigned int iGroupID );	// CoF 1.6a: unreferenced
	float m_flSequenceOverrideTime;
	int m_iSequenceOverride;
	float m_flGetupTime;
	bool m_bHasGotUp;
	bool m_bCantBeRevived;
	float m_flGaitSequenceOverrideTime;
	int m_iGaitSequenceOverride;
	bool m_bWaitingToFall;
	int GetWeaponAnimation( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void ShowHint( int hint, bool showanyway );
	string_t m_strHintsShown[33];
	void StartWheelchairMode( void );
	void StopWheelchairMode( void );
	float m_flTurnAroundTime;
	float m_flTurnVectorTime;
	Vector m_vTurnVector;
	bool m_bStartedDrowning;
	bool m_bWaterVoiceCancel;
	void EXPORT WeaponMelee( void );
	void CheckQuickDualWield( char *quicksel, char *lastquicksel );
	CBaseAnimating *pDeathScene;
	int m_iDoctorWeapon;
	int m_iDeathBits;
	EHANDLE m_pClientBoat;
	EHANDLE m_pClientAttachedCamera;
	bool EHandlesNotSame( EHANDLE edict1, EHANDLE edict2 );
	bool CanBeDualWielded( int weaponid );
	bool CanUseWeapon_guess( int weaponid );	// CoF 1.6a: unreferenced
	bool IsAFlashlight( int weaponid );
	bool m_bInWater;
	int m_iPhoneMessage;
	void RecallDocument( int slot );
	void AddDocument( CCoFDocument *pDocument );
	void DebugDocuments( void );
	enum { COFR_COOP_FLASHLIGHT = 32 };
	int m_iFlashlightFlags;
	int m_iClientFlashlightFlags;
	float m_flQuickSlotTimeDecay;
	int m_iDualWeaponLeftAmmo;
	int m_iDualWeaponRightAmmo;
	int m_iClientDualWeaponLeftAmmo;
	int m_iClientDualWeaponRightAmmo;
	int g_iDoorSound;
	void EXPORT PlayNewDoorSound( void );
	void DualWield( int slot1, int slot2 );
	void AddGoodPoint( int amount, bool subtract );
	int m_iGoodPoints;
	bool m_bBossHealthBar;
	string_t m_sBossBarEmpty;
	string_t m_sBossBarFull;
	int m_iBossMaxHealth;
	int m_iBossCurrentHealth;
	string_t m_sBossName;
	int m_iHealthCap;
	float m_fUseTime;
	float CalculateAccuracy( void );
	float m_flNextDistanceCheckTime;
	Vector m_vPreviousOrigin;
	void SetDualWieldWeapon( int model );
	void DualWieldAnimation( int anim );
	string_t m_sDualWieldViewModel;
	string_t m_sClientDualWieldViewModel;
	int m_bMirroredDualWield;
	EHANDLE m_pCurrentDocument;
	statistics playerstats;
	documents documentsstruct[17];
	int transferredentity_message[51];
	Vector transferredentity_origin[51];
	int transferredentity_classname[51];
	int transferredentity_clip[51];
	int m_iDualWeaponLeftID;
	int m_iDualWeaponRightID;
	void GiveDualWeapon( int id1, int id2 );
	bool xbox_lshoulderdown;
	bool xbox_rshoulderdown;
	bool xbox_ltriggerdown;
	bool xbox_rtriggerdown;
	bool xbox_dpadup;
	bool xbox_dpaddown;
	bool xbox_dpadleft;
	bool xbox_dpadright;
	bool xbox_ducktoggle;
	bool xbox_haspressedduck;
	float m_fControllerForwardSpeed;
	float m_fControllerSideSpeed;
	string_t m_sClientObjectiveText;
	string_t m_sObjectiveText;
	float m_flOutOfBreathDisableTime;
	int m_iMobileFlashMode;
	bool m_bThreeSlots;
	int m_iUsingComputerOrPhone;
	bool b_GoingThroughADoor;
	int m_iFlashlight;
	int m_iClientFlashlight;
	int m_ClientDrawBars;
	int m_DrawBars;
	int m_iClientBlurActive;
	int m_iBlurActive;
	bool m_UnreadSMS;
	int m_ClientLadder;
	bool m_OnStairs;
	float m_fFallingTime;
	bool m_bShouldFallFreeze;
	Vector m_LadderAngles;
	bool m_bNightmare;
	float m_fPhoneCallTime;
	bool m_bInPhoneCall;
	bool m_bDodgeDisabled;
	bool m_bScaredBreathing;
	float m_fNextScaredBreathingTime;
	int m_iBlackAndWhite;
	int m_iTapes;
	bool m_bSilentGive;
	bool m_bWeaponWaitingToDeploy;
	int m_iHintAmount;
	float m_fNextHeartBeatTime;
	bool m_bNearStranger;
	void HeartBeatCheck( void );
	int m_iSwitchbladeCuts;
	EHANDLE m_pLastAttackedEnemy;
	Vector GetAccuracy( float accuracy );
	int m_iPhoneDisabled;
	int m_iClientPhoneDisabled;
	bool m_bPhoneLow;
	float m_fNextPhoneWarnTime;
	float m_flDeadTime;
	bool m_bIssuedGameOver;
	bool m_bGibbed;
	bool m_bNoDeadFade;
	float m_flDeadAutoReviveTime;
	void VibrateCheck( void );
	float m_fVibrateTime;
	bool m_bVibrate;
	void EXPORT GameStart( void );
	void EXPORT CrashGame( void );
	void EXPORT DemoStart( void );
	char DestinationText[256];
	int SufficientlyOffTheGround( void );
	int m_iViewEntIndex;
	int m_iClientViewEntIndex;
	int m_iKeypadNumber;
	int m_iKeypadType;
	void Dodge( int direction );
	float m_fDodgeTime;
	char *GetDescriptionFromItem( char *item, char *filename );
	char m_sQuickSel1[128];
	char m_sQuickSel2[128];
	char m_sQuickSel3[128];
	void QuickSelectItem( int number );
	void MoveStatue( int Direction, int Type );
	void FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );
	void EXPORT MeleeHitThinkPunch( void );
	void EXPORT MeleeHitThinkSwitchblade( void );
	void EXPORT MeleeHitThinkSwitchblade2( void );
	void EXPORT MeleeHitThinkSwitchblade3( void );
	void EXPORT MeleeHitThinkNightStick( void );
	void EXPORT MeleeHitThinkNightStick2( void );
	void EXPORT MeleeHitThinkNightStick3( void );
	void EXPORT MeleeHitThinkBranch( void );
	void EXPORT MeleeHitThinkAxe( void );
	void EXPORT MeleeHitThinkSledgeHammer( void );
	void ShowVGUIMenu( int iMenuID );
	void PhoneNumber( int number );
	static const char *pMorphineInjectionSounds[0];
	static const char *pPainSounds[0];
	static const char *pSwingSounds[0];
	static const char *pDeathSounds[0];
	static const char *pVomitSounds[0];
	static const char *pLowHealthSounds[0];
	static const char *pMorphineInjectionSoundsPolice[0];
	static const char *pPainSoundsPolice[0];
	static const char *pSwingSoundsPolice[0];
	static const char *pDeathSoundsPolice[0];
	static const char *pLowHealthSoundsPolice[0];
	static const char *pPainSoundsDoctor[0];
	static const char *pSwingSoundsDoctor[0];
	static const char *pDeathSoundsDoctor[0];
	static const char *pLowHealthSoundsDoctor[0];
	float m_flNextLowHealthSound;
	void LowHealthSound( void );
	void MorphineSound( void );
	void SwingSound( void );
	void DeathSound ( void );
	int m_iMorphineCycleTime;
	int m_iMorphineRecover;
	void CheckMorphineRestore( void );
	bool m_bHasSaved;
	void EXPORT PreSave( int slot );
	void EXPORT EndSave( void );
	void StaminaThink( void );
	void BreatheThink( void );
	float m_fUnderwaterBreatheTime;
	bool m_bWasDrowning;
	bool m_bSubmerged;
	bool m_bWasInShallowWater;
	float m_flRippleTime;
	bool m_bInfiniteStamina;
	bool m_bInfiniteHealth;
	bool m_bInfiniteAmmo;
	float m_fClientStamina;
	float m_fStamina;
	float m_fStaminaChargeCyle;
	float m_fStaminaBreatheCyle;
	int inventorySlot[7];
	int inventorySlotModels[7];
	int inventorySlotClassname[7];
	int ClientinventorySlot[7];
	int AddItemToInventory( int name, int model, int classname );
	int IsSlotFree( int slot );
	void PrintInventoryInfoDebug( void );
	void PrintInventoryModelInfoDebug( void );
	void PrintInventoryClassnameInfoDebug( void );
	void CloseInventory( void );
	void SelectRandomWeapon( void );
	bool m_bUsedFirstSyringe;
	void MorphineTrip( void );
	void StopMorphineTrip( void );
	void Vomit( void );
	float m_fVomitTime;
	float m_fMorphineTripTime;
	bool m_bIsTripping;
	float m_fTrippingTime;
	bool m_bHasVomited;
	int m_iOldRoomType;
	int InventoryIsFull( void );
	void KeypadNumberCheck( int number );
	void ViewKnock( float Amount );
	void ViewLock( bool Lock, float time );
	int InventoryAddCheck( CBaseEntity *pEntity );
	void ClearOutSlot( int slotnum, int all );
	void ClearOutSlotNamed( int slot, int all );
	int HasItemInSlot( char *script, bool remove );
	int HasItemInSlotNumber( int slotnum, char *script );
	void InventoryMessage( int slotnum, int slot, int clearslots );
	void InventoryEquip( int slotnum );
	void InventoryUse( int slotnum );
	void InventoryCheck( int slotnum );
	void InventoryDrop( int slotnum );
	void InventoryCombine( int slot1, int slot2, char *result );
	void DropKeyItem( int slotnum );
	char *GetItemFromSlot( int slotnumber );
	char *GetModelFromSlot( int slotnumber );
	char *GetClassnameFromSlot( int slotnumber );
public:
	int					random_seed;    // See that is shared between client & server for shared weapons code

	int					m_iPlayerSound;// the index of the sound list slot reserved for this player
	int					m_iTargetVolume;// ideal sound volume. 
	int					m_iWeaponVolume;// how loud the player's weapon is right now.
	int					m_iExtraSoundTypes;// additional classification for this weapon's sound
	int					m_iWeaponFlash;// brightness of the weapon flash
	float				m_flStopExtraSoundTime;
	
	float				m_flFlashLightTime;	// Time until next battery draw/Recharge
	int					m_iFlashBattery;		// Flashlight Battery Draw

	int					m_afButtonLast;
	int					m_afButtonPressed;
	int					m_afButtonReleased;
	
	edict_t			   *m_pentSndLast;			// last sound entity to modify player room type
	float				m_flSndRoomtype;		// last roomtype set by sound entity
	float				m_flSndRange;			// dist from player to sound entity

	float				m_flFallVelocity;
	
	int					m_rgItems[MAX_ITEMS];
	int					m_fKnownItem;		// True when a new item needs to be added
	int					m_fNewAmmo;			// True when a new item has been added

	unsigned int		m_afPhysicsFlags;	// physics flags - set when 'normal' physics should be revisited or overriden
	float				m_fNextSuicideTime; // the time after which the player can next use the suicide command

	// buz: gasmask
//	int		m_iHasGasMask;
	int		m_iGasMaskOn;
	int		m_iUpdateGasMask;
	float	m_flGasMaskTime;
	float	m_flNextBreathTime;
	int		m_iLastGasMaskSound;

	// buz: head shield
	int		m_iHeadShieldOn;
	int		m_iUpdateHeadShield;
	float	m_flHeadShieldTime;

	// buz: special tank
	CBaseEntity		*m_pSpecTank;

	// buz: current playing mp3 file
	string_t	m_strCurrentMp3;
	int			m_iMp3NeedsUpdate;
	float m_fMp3FadeTime;
	bool m_bMp3ShouldLoop;
	float m_fMp3FadeInTime;
	bool IsInCutscene( void );
	void PlayerSetMP3( int strindex, int loop, float fadetime );
	void PlayerStopMP3( float fadetime );

	// buz: current goal description
	string_t	m_strCurrentGoalName;
	string_t	m_strCurrentGoalImageName;
	string_t	m_strCurrentGoalTitleName;
	int			m_iGoalNeedsUpdate;

	void	PlayerSetGoalDesc( string_t strindex, string_t title, string_t imgname );

	// buz: jumping height
	int		m_iJumpHeight; // in percents, 0 - 100
	void	SetJumpHeight( int value );

	// buz: real lightlevel (from 0 to 1)
	float	m_fLightlevel;
	int m_iUsingMic;

	// buz: last monster, who consider this player his enemy (allies will attack him)
	EHANDLE	m_hLastEnemy;
	EHANDLE m_hCurrentCamera;
	EHANDLE m_pPadlock;
	int m_iPadlockNumber[5];
	bool HasTaclight( void );
	CBasePlayerWeapon *GetPlayerWeapon( int id );

// these are time-sensitive things that we keep track of
	float				m_flTimeStepSound;	// when the last stepping sound was made
	float				m_flTimeWeaponIdle; // when to play another weapon idle animation.
	float				m_flSwimTime;		// how long player has been underwater
	float				m_flDuckTime;		// how long we've been ducking
	float				m_flWallJumpTime;	// how long until next walljump

	float				m_flSuitUpdate;					// when to play next suit update
	int					m_rgSuitPlayList[CSUITPLAYLIST];// next sentencenum to play for suit update
	int					m_iSuitPlayNext;				// next sentence slot for queue storage;
	int					m_rgiSuitNoRepeat[CSUITNOREPEAT];		// suit sentence no repeat list
	float				m_rgflSuitNoRepeatTime[CSUITNOREPEAT];	// how long to wait before allowing repeat
	int					m_lastDamageAmount;		// Last damage taken
	float				m_tbdPrev;				// Time-based damage timer

	float				m_flgeigerRange;		// range to nearest radiation source
	float				m_flgeigerDelay;		// delay per update of range msg to client
	int					m_igeigerRangePrev;
	int					m_iStepLeft;			// alternate left/right foot stepping sound
	char				m_szTextureName[CBTEXTURENAMEMAX];	// current texture name we're standing on
	char				m_chTextureType;		// current texture type

	int					m_idrowndmg;			// track drowning damage taken
	int					m_idrownrestored;		// track drowning damage restored

	int					m_bitsHUDDamage;		// Damage bits for the current fame. These get sent to 
												// the hude via the DAMAGE message
	BOOL				m_fInitHUD;				// True when deferred HUD restart msg needs to be sent
	BOOL				m_fGameHUDInitialized;
	int					m_iTrain;				// Train control position
	BOOL				m_fWeapon;				// Set this to FALSE to force a reset of the current weapon HUD info

	EHANDLE				m_pTank;				// the tank which the player is currently controlling,  NULL if no tank
	float				m_fDeadTime;			// the time at which the player died  (used in PlayerDeathThink())

	BOOL			m_fNoPlayerSound;	// a debugging feature. Player makes no sound if this is true. 
	BOOL			m_fLongJump; // does this player have the longjump module?

	float       m_tSneaking;
	int			m_iUpdateTime;		// stores the number of frame ticks before sending HUD update messages
	int			m_iClientHealth;	// the health currently known by the client.  If this changes, send a new
	int			m_iClientBattery;	// the Battery currently known by the client.  If this changes, send a new
	int			m_iHideHUD;		// the players hud weapon info is to be hidden
	int			m_iClientHideHUD;
	int			m_iFOV;			// field of view
	int			m_iClientFOV;	// client's known FOV

	void ViewPunch( float p, float y, float r ); // buz - нагло спер из хл2
	CRealCamera *m_pCamera;

	// usable player items 
	CBasePlayerItem	*m_rgpPlayerItems[MAX_ITEM_TYPES];
	CBasePlayerItem *m_pActiveItem;
	CBasePlayerItem *m_pClientActiveItem;  // client version of the active item
	CBasePlayerItem *m_pLastItem;
	CBasePlayerItem *m_pNextItem;
	CBasePlayerItem *m_pStoredActionItem;
	// shared ammo slots
	int	m_rgAmmo[MAX_AMMO_SLOTS];
	int	m_rgAmmoLast[MAX_AMMO_SLOTS];

	Vector				m_vecAutoAim;
	BOOL				m_fOnTarget;
	int					m_iDeaths;
	float				m_iRespawnFrames;	// used in PlayerDeathThink() to make sure players can always respawn

	int m_lastx, m_lasty;  // These are the previous update's crosshair angles, DON"T SAVE/RESTORE

	int m_nCustomSprayFrames;// Custom clan logo frames for this player
	float	m_flNextDecalTime;// next time this player can spray a decal

	char m_szTeamName[TEAM_NAME_LENGTH];

	virtual void Spawn( void );
	void Pain( void );

//	virtual void Think( void );
	virtual void Jump( void );
	virtual void Duck( void );
	virtual void PreThink( void );
	virtual void PostThink( void );
	virtual Vector GetGunPosition( void );
	virtual int TakeHealth( float flHealth, int bitsDamageType );
	virtual void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual void MultiplayerKilled( entvars_t *pevAttacker, int iGib );
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ) + pev->view_ofs * RANDOM_FLOAT( 0.5, 1.1 ); };		// position to shoot at
	virtual void StartSneaking( void ) { m_tSneaking = gpGlobals->time - 1; }
	virtual void StopSneaking( void ) { m_tSneaking = gpGlobals->time + 30; }
	virtual BOOL IsSneaking( void ) { return m_tSneaking <= gpGlobals->time; }
	virtual BOOL IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL ShouldFadeOnDeath( void ) { return FALSE; }
	virtual	BOOL IsPlayer( void ) { return TRUE; }			// Spectators should return FALSE for this, they aren't "players" as far as game logic is concerned

	virtual BOOL IsNetClient( void ) { return TRUE; }		// Bots should return FALSE for this, they can't receive NET messages
															// Spectators should return TRUE for this
	virtual const char *TeamID( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	void RenewItems(void);
	void PackDeadPlayerItems( void );
	void RemoveAllItems( BOOL removeSuit );
	void RemoveItems( int iWeaponMask, int i9mm, int i357, int iBuck, int iBolt, int iARGren, int iRock, int iEgon, int iSatchel, int iSnark, int iTrip, int iGren, int iHornet,
		int iAps, int iBarret, int iAk, int iAsval, int iRpk, int iGroza, int iPK);
	void RemoveAmmo( const char* szName, int iAmount );
	BOOL SwitchWeapon( CBasePlayerItem *pWeapon );

	// JOHN:  sends custom messages if player HUD data has changed  (eg health, ammo)
	virtual void UpdateClientData( void );
	
	static	TYPEDESCRIPTION m_playerSaveData[];

	// Player is moved across the transition by other means
	virtual int		ObjectCaps( void ) { return (CBaseMonster :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
	virtual void	Precache( void );
	BOOL			IsOnLadder( void );
	BOOL			FlashlightIsOn( void );
	void			FlashlightTurnOn( void );
	void			FlashlightTurnOff( void );
	
	void UpdatePlayerSound ( void );

	int Classify ( void );
	void SetAnimation( PLAYER_ANIM playerAnim );
	void SetWeaponAnimType( const char *szExtention );
	char m_szAnimExtention[32];

	// custom player functions
	virtual void ImpulseCommands( void );
	void CheatImpulseCommands( int iImpulse );

	void StartDeathCam( void );
	void StartObserver( Vector vecPosition, Vector vecViewAngle );

	void AddPoints( int score, BOOL bAllowNegativeScore );
	void AddPointsToTeam( int score, BOOL bAllowNegativeScore );
	BOOL AddPlayerItem( CBasePlayerItem *pItem );
	BOOL RemovePlayerItem( CBasePlayerItem *pItem );
	void DropPlayerItem( char *pszItemName, int inventoryslot );
	BOOL HasPlayerItem( CBasePlayerItem *pCheckItem );
	BOOL HasNamedPlayerItem( const char *pszItemName );
	BOOL HasWeapons( void );// do I have ANY weapons?
	void SelectPrevItem( int iItem );
	void SelectNextItem( int iItem );
	void SelectLastItem( bool nowait );
	void SelectItem(const char *pstr);
	void ItemPreFrame( void );
	void ItemPostFrame( void );
	void GiveNamedItem( const char *pszName, bool deletewhendropped, bool bSkipDonatorCheck_guess );	// CoF 1.6a: third parameter (weapon_tmp is given without the donator test)
	void GiveNamedItemAmmo( const char *pszName, int defaultammo );
	void EnableControl(BOOL fControl);
	void FallEnableControl( int fControl );
	void PlayerAnimation( const char *anim, bool gait );
	void EXPORT MultiplayerTouch( CBaseEntity *pOther );
	EHANDLE pPlayerToRevive;
	EHANDLE pPlayerReviving;
	CBaseEntity *pCorpseEnt;
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent );
	float m_flReviveTime;

	int  GiveAmmo( int iAmount, char *szName, int iMax, bool bNoMessage_guess );
	void SendAmmoUpdate(void);

	void WaterMove( void );
	void EXPORT PlayerDeathThink( void );
	void EXPORT MultiplayerPlayerDeathThink( void );
	void EXPORT WheelchairPlayerDeathThink( void );
	void RevivePlayer( void );
	void PlayerUse( void );

	void CheckSuitUpdate();
	void SetSuitUpdate(char *name, int fgroup, int iNoRepeat);
	void UpdateGeigerCounter( void );
	void CheckTimeBasedDamage( void );

	BOOL FBecomeProne ( void );
	void BarnacleVictimBitten ( entvars_t *pevBarnacle );
	void BarnacleVictimReleased ( void );
	static int GetAmmoIndex(const char *psz);
	int AmmoInventory( int iAmmoIndex );
	int Illumination( void );

	void ResetAutoaim( void );
	Vector GetAutoaimVector( float flDelta  );
	Vector AutoaimDeflection( Vector &vecSrc, float flDist, float flDelta  );

	void ForceClientDllUpdate( void );  // Forces all client .dll specific data to be resent to client.

	void DeathMessage( entvars_t *pevKiller );

	void SetCustomDecalFrames( int nFrames );
	int GetCustomDecalFrames( void );

	void CBasePlayer::TabulateAmmo( void );

	float m_flStartCharge;
	float m_flAmmoStartCharge;
	float m_flPlayAftershock;
	float m_flNextAmmoBurn;// while charging, when to absorb another unit of player's ammo?
	
	//Player ID
	void InitStatusBar( void );
	void UpdateStatusBar( void );
	int m_izSBarState[ SBAR_END ];
	float m_flNextSBarUpdateTime;
	float m_flStatusBarDisappearDelay;
	char m_SbarString0[ SBAR_STRING_SIZE ];
	char m_SbarString1[ SBAR_STRING_SIZE ];
	
	float m_flNextChatTime;

	// rain
	int		Rain_dripsPerSecond;
	float	Rain_windX, Rain_windY;
	float	Rain_randX, Rain_randY;

	int		Rain_ideal_dripsPerSecond;
	float	Rain_ideal_windX, Rain_ideal_windY;
	float	Rain_ideal_randX, Rain_ideal_randY;

	float	Rain_endFade; // 0 means off
	float	Rain_nextFadeUpdate;

	int		Rain_needsUpdate;

	void SendInitMessages(); // buz
	int m_iInitMessagesSent; // buz
	void ToThirdPerson( void );
	void ToFirstPerson( void );
	void SetCorrectView( void );
	int m_iViewValue;
	void ToSlowMo( void );
	void ToNormalMo( void );
	void SetCorrectMo( void );
	int m_iMoValue;
	// Appended source-owned inventory: existing private-data offsets stay unchanged.
	int cofrInventorySlot[9];
	int cofrInventorySlotModels[9];
	int cofrInventorySlotClassname[9];
	int cofrClientinventorySlot[9];
	int cofrInventorySchema;
	cof::StairSprintTracker cofrStairSprint;
	void CoFRMigrateInventory();

};

#define AUTOAIM_2DEGREES  0.0348994967025
#define AUTOAIM_5DEGREES  0.08715574274766
#define AUTOAIM_8DEGREES  0.1391731009601
#define AUTOAIM_10DEGREES 0.1736481776669


extern int	gmsgHudText;
extern int	gmsgParticle; // LRC
extern BOOL gInitHUD;

#endif // PLAYER_H
