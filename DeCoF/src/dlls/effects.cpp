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
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "customentity.h"

//=========================================================
// Declared by the 1.6a effects.h before CSprite: the vtables and
// type descriptors of retail effects.obj start with these four (the
// 1.55 PDB has CEnvModel at line 78 of effects.h, CSprite at 279).
//=========================================================
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

class CEnvModel : public CBaseAnimating
{
	void Spawn( void );
	void Precache( void );
	void EXPORT Think( void );
	void KeyValue( KeyValueData *pkvd );
	STATE GetState( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	float VecToYaw( Vector vecDir );

	void SetSequence( void );
	bool m_bIssuedNote;
	int m_ChestNumbers;
	int number1;
	int number2;

	string_t m_iszSequence_On;
	string_t m_iszSequence_Off;
	int m_iAction_On;
	int m_iAction_Off;

public:
	void VisibilityCheck( void );
};

class CCofBreatheToggle : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

class CCofStressTest : public CBaseEntity
{
public:
	virtual void Spawn( void );
	void EXPORT TurnOnBumpMaps( void );
	void EXPORT TurnOnWater( void );
	void EXPORT TurnOnDynLight( void );
	void EXPORT TurnOnWeather( void );
	void EXPORT StartNextEffect( void );
	void EXPORT ReportEffect( void );
	void EXPORT Disconnect( void );
	void AllowToContinue( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int currenteffect;
	int startfps;
	int currentfps;
};

#include "effects.h"
#include "weapons.h"
#include "decals.h"
#include "func_break.h"
#include "shake.h"
#include "player.h" //LRC - footstep stuff
#include "locus.h" //LRC - locus utilities
#include "movewith.h" //LRC - the DesiredThink system
#include "studio.h"
#include "gamerules.h"
#include <cmath>

#define	SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired

#define SF_FUNNEL_REVERSE			1 // funnel effect repels particles instead of attracting them.
#define SF_FUNNEL_REPEATABLE		2 // allows a funnel to be refired

//=========================================================
// cof_keypad.h, included after effects.h (the retail CCofKeypad
// vtable follows CRainModify)
//=========================================================
class CCofKeypad : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool m_IsCorrect;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static TYPEDESCRIPTION m_SaveData[];
};

//=========================================================
// Declarations that belong in headers which are not rebuilt yet.
// effects.h: CCofComputer, CCofYesNo.
// cof_importantclassdefs.h: CCoFDocument.
// Layouts from the 1.55 PDB, offsets checked against 1.6a.
//=========================================================
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

// header not rebuilt yet (declared the same way in player.cpp)
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

// cof_cooplevelchangemanager header, not rebuilt yet (declared the same way in client.cpp)
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

//=========================================================
// Globals that no header declares (types from the 1.55 PDB)
//=========================================================
extern int gmsgUnreadSMS;
extern int gmsgBlackWhite;
extern int gmsgQSlotClear;
extern int gmsgNoteDoc;
extern int gmsgKeypad;
extern int gmsgCoFLogo;
extern int gmsgYesNo;
extern int gmsgComputer;
extern int gmsgAddNote;
extern int gmsgCameraZoom;
extern int gmsgBossBar;
extern int gmsgHealthCap;
extern int gmsgChapter;
extern int gmsgAttachCam;
extern int gmsgCoopCamAnim;
extern int gmsgLensFlare;
extern int gmsgCredits;
extern int gmsgGreyFade;
extern int gmsgClothes;
extern int gmsgHeadShield;
extern int gmsgBeatCoop;
extern int gmsgCoopStats;
extern int gmsgIntrodn;
extern int gmsgTimer;
extern int gmsgMP3;
extern int gmsgCustomSlv;
extern int gmsgCustomFng;
extern int gmsgCustomGlv;
extern int gEvilImpulse101;
extern int gSpecialItemGive;
extern int m_iMusicManagerLoops;
extern float m_fMusicManagerFadeTime;
extern const char *GetClassnameForId[];
bool showobjective;
extern int g_iSurvivalMode;
extern int m_iSimonMode;
extern bool m_gbIsInCutscene;

extern void UTIL_ProMessage( CBaseEntity *pEntity, const char *pMessage, int colour, bool all );
extern float GetSequenceTimeNamed( void *pmodel, char *sequence );
extern int CountConnectedPlayers( void );
extern int CountConnectedLivePlayers( void );

#undef SET_VIEW
extern void SET_VIEW( edict_t *pClient, edict_t *pViewent );

inline char *GET_PLAYER_INFOBUFFER( edict_t *e ) { return (*g_engfuncs.pfnGetInfoKeyBuffer)( e ); }


//LRC - make info_target an entity class in its own right
class CInfoTarget : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
};

LINK_ENTITY_TO_CLASS( info_target, CInfoTarget );

//LRC- force an info_target to use the sprite null.spr
#define SF_TARGET_HACK_VISIBLE	1

// Landmark class
void CInfoTarget :: Spawn( void )
{
	//Precache();
	pev->solid = SOLID_NOT;
	if (pev->spawnflags & SF_TARGET_HACK_VISIBLE)
	{
		PRECACHE_MODEL("sprites/null.spr");
		SET_MODEL(ENT(pev),"sprites/null.spr");
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
	}
}

void CInfoTarget :: Precache( void )
{
	if (pev->spawnflags & SF_TARGET_HACK_VISIBLE)
		PRECACHE_MODEL("sprites/null.spr");
}


class CBubbling : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	
	void	EXPORT FizzThink( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	virtual int		ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_density;
	int		m_frequency;
	int		m_bubbleModel;
	int		m_state;

	virtual STATE GetState( void ) { return m_state?STATE_ON:STATE_OFF; };
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

TYPEDESCRIPTION	CBubbling::m_SaveData[] = 
{
	DEFINE_FIELD( CBubbling, m_density, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_frequency, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_state, FIELD_INTEGER ),
	// Let spawn restore this!
	//	DEFINE_FIELD( CBubbling, m_bubbleModel, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBubbling, CBaseEntity );


#define SF_BUBBLES_STARTOFF		0x0001

void CBubbling::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), STRING(pev->model) );		// Set size

	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;
	int speed = pev->speed > 0 ? pev->speed : -pev->speed;

	// HACKHACK!!! - Speed in rendercolor
	pev->rendercolor.x = speed >> 8;
	pev->rendercolor.y = speed & 255;
	pev->rendercolor.z = (pev->speed < 0) ? 1 : 0;


	if ( !(pev->spawnflags & SF_BUBBLES_STARTOFF) )
	{
		SetThink(&CBubbling:: FizzThink );
		SetNextThink( 2.0 );
		m_state = 1;
	}
	else 
		m_state = 0;
}

void CBubbling::Precache( void )
{
	m_bubbleModel = PRECACHE_MODEL("sprites/bubble.spr");			// Precache bubble sprite
}


void CBubbling::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_state ) )
		m_state = !m_state;

	if ( m_state )
	{
		SetThink(&CBubbling:: FizzThink );
		SetNextThink( 0.1 );
	}
	else
	{
		SetThink( NULL );
		DontThink();
	}
}


void CBubbling::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "density"))
	{
		m_density = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		m_frequency = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "current"))
	{
		pev->speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}


void CBubbling::FizzThink( void )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, VecBModelOrigin(pev) );
		WRITE_BYTE( TE_FIZZ );
		WRITE_SHORT( (short)ENTINDEX( edict() ) );
		WRITE_SHORT( (short)m_bubbleModel );
		WRITE_BYTE( m_density );
	MESSAGE_END();

	if ( m_frequency > 19 ) // frequencies above 20 are treated as 20.
		SetNextThink( 0.5 );
	else
		SetNextThink( 2.5 - (0.1 * m_frequency) );
}

// --------------------------------------------------
// 
// Beams
//
// --------------------------------------------------

LINK_ENTITY_TO_CLASS( beam, CBeam );

void CBeam::Spawn( void )
{
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );
}

void CBeam::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeam::SetStartEntity( int entityIndex ) 
{ 
	pev->sequence = (entityIndex & 0x0FFF) | ((pev->sequence&0xF000)<<12); 
	pev->owner = g_engfuncs.pfnPEntityOfEntIndex( entityIndex );
}

void CBeam::SetEndEntity( int entityIndex ) 
{ 
	pev->skin = (entityIndex & 0x0FFF) | ((pev->skin&0xF000)<<12); 
	pev->aiment = g_engfuncs.pfnPEntityOfEntIndex( entityIndex );
}


// These don't take attachments into account
const Vector &CBeam::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		edict_t *pent =  g_engfuncs.pfnPEntityOfEntIndex( GetStartEntity() );
		return pent->v.origin;
	}
	return pev->origin;
}


const Vector &CBeam::GetEndPos( void )
{
	int type = GetType();
	if ( type == BEAM_POINTS || type == BEAM_HOSE )
	{
		return pev->angles;
	}

	edict_t *pent =  g_engfuncs.pfnPEntityOfEntIndex( GetEndEntity() );
	if ( pent )
		return pent->v.origin;
	return pev->angles;
}


CBeam *CBeam::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeam *pBeam = GetClassPtr( (CBeam *)NULL );
	pBeam->pev->classname = MAKE_STRING("beam");

	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}


void CBeam::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}


void CBeam::PointsInit( const Vector &start, const Vector &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::HoseInit( const Vector &start, const Vector &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::PointEntInit( const Vector &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeam::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeam::RelinkBeam( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->mins.x = min( startPos.x, endPos.x );
	pev->mins.y = min( startPos.y, endPos.y );
	pev->mins.z = min( startPos.z, endPos.z );
	pev->maxs.x = max( startPos.x, endPos.x );
	pev->maxs.y = max( startPos.y, endPos.y );
	pev->maxs.z = max( startPos.z, endPos.z );
	pev->mins = pev->mins - pev->origin;
	pev->maxs = pev->maxs - pev->origin;

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
}

#if 0
void CBeam::SetObjectCollisionBox( void )
{
	const Vector &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->absmin.x = min( startPos.x, endPos.x );
	pev->absmin.y = min( startPos.y, endPos.y );
	pev->absmin.z = min( startPos.z, endPos.z );
	pev->absmax.x = max( startPos.x, endPos.x );
	pev->absmax.y = max( startPos.y, endPos.y );
	pev->absmax.z = max( startPos.z, endPos.z );
}
#endif


void CBeam::TriggerTouch( CBaseEntity *pOther )
{
	if ( pOther->pev->flags & (FL_CLIENT | FL_MONSTER) )
	{
		if ( pev->owner )
		{
			CBaseEntity *pOwner = CBaseEntity::Instance(pev->owner);
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_debug, "Firing targets!!!\n" );
	}
}


CBaseEntity *CBeam::RandomTargetname( const char *szName )
{
	int total = 0;

	CBaseEntity *pEntity = NULL;
	CBaseEntity *pNewEntity = NULL;
	while ((pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL)
	{
		total++;
		if (RANDOM_LONG(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}


void CBeam::DoSparks( const Vector &start, const Vector &end )
{
	if ( pev->spawnflags & (SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND) )
	{
		if ( pev->spawnflags & SF_BEAM_SPARKSTART )
		{
			UTIL_Sparks( start );
		}
		if ( pev->spawnflags & SF_BEAM_SPARKEND )
		{
			UTIL_Sparks( end );
		}
	}
}


class CLightning : public CBeam
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	EXPORT StrikeThink( void );
	void	EXPORT TripThink( void );
	void	RandomArea( void );
	void	RandomPoint( Vector &vecSrc );
	void	Zap( const Vector &vecSrc, const Vector &vecDest );
	void	EXPORT StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	EXPORT ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !(pev->spawnflags & SF_BEAM_RING) )
			return TRUE;
		return FALSE;
	}

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	BeamUpdatePoints( void ); //LRC
	void	BeamUpdateVars( void );

	virtual STATE GetState( void ) { return m_active?STATE_OFF:STATE_ON; };

	int		m_active;
	int		m_iszStartEntity;
	int		m_iszEndEntity;
	float	m_life;
	int		m_boltWidth;
	int		m_noiseAmplitude;
	int		m_brightness;
	int		m_speed;
	float	m_restrike;
	int		m_spriteTexture;
	int		m_iszSpriteName;
	int		m_frameStart;

	float	m_radius;
};

LINK_ENTITY_TO_CLASS( env_lightning, CLightning );
LINK_ENTITY_TO_CLASS( env_beam, CLightning );

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	CLightning::Spawn();
	SetTouch(&CTripBeam:: TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif



TYPEDESCRIPTION	CLightning::m_SaveData[] = 
{
	DEFINE_FIELD( CLightning, m_active, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszStartEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_iszEndEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_life, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_boltWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_noiseAmplitude, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_brightness, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_speed, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_restrike, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_spriteTexture, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_frameStart, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_radius, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLightning, CBeam );


void CLightning::Spawn( void )
{
	if ( FStringNull( m_iszSpriteName ) )
	{
		SetThink(&CLightning:: SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	pev->dmgtime = gpGlobals->time;

	//LRC- a convenience for mappers. Will this mess anything up?
	if (pev->rendercolor == g_vecZero)
		pev->rendercolor = Vector(255, 255, 255);

	if (pev->frags == 0)
	{
		pev->frags = DMG_ENERGYBEAM;
	}

	if ( ServerSide() )
	{
		SetThink( NULL );
		if ( pev->dmg != 0 || !FStringNull(pev->target) )
		{
			SetThink(&CLightning:: TripThink );
			SetNextThink( 0.1 );
		}
		if ( pev->targetname )
		{
			if ( !(pev->spawnflags & SF_BEAM_STARTON) )
			{
				pev->effects = EF_NODRAW;
				m_active = 0;
				DontThink();
			}
			else
				m_active = 1;
		
			SetUse(&CLightning:: ToggleUse );
		}
	}
	else
	{
		m_active = 0;
		if ( !FStringNull(pev->targetname) )
		{
			SetUse(&CLightning:: StrikeUse );
		}
		if ( FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON) )
		{
			SetThink(&CLightning:: StrikeThink );
			SetNextThink( 1.0 );
		}
	}
}

void CLightning::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );
	CBeam::Precache();
}


void CLightning::Activate( void )
{
	if ( ServerSide() )
		BeamUpdateVars();

	CBeam::Activate();
}


void CLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


void CLightning::ToggleUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;
	if ( m_active )
	{
		m_active = 0;
		SUB_UseTargets( this, USE_OFF, 0 ); //LRC
		pev->effects |= EF_NODRAW;
		DontThink();
	}
	else
	{
		m_active = 1;
		SUB_UseTargets( this, USE_ON, 0 ); //LRC
		BeamUpdatePoints();
		pev->effects &= ~EF_NODRAW;
		DoSparks( GetStartPos(), GetEndPos() );
		if ( pev->dmg > 0 )
		{
			SetNextThink( 0 );
			pev->dmgtime = gpGlobals->time;
		}
	}
}


void CLightning::StrikeUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;

	if ( m_active )
	{
		m_active = 0;
		SetThink( NULL );
	}
	else
	{
		SetThink(&CLightning:: StrikeThink );
		SetNextThink( 0.1 );
	}

	if ( !FBitSet( pev->spawnflags, SF_BEAM_TOGGLE ) )
		SetUse( NULL );
}


int IsPointEntity( CBaseEntity *pEnt )
{
//	ALERT(at_console, "IsPE: %s, %d\n", STRING(pEnt->pev->classname), pEnt->pev->modelindex);
	if (pEnt->pev->modelindex && !(pEnt->pev->flags & FL_CUSTOMENTITY)) //LRC- follow (almost) any entity that has a model
		return 0;
	else
		return 1;

	return 0;
}


void CLightning::StrikeThink( void )
{
	if ( m_life != 0 && m_restrike != -1) //LRC non-restriking beams! what an idea!
	{
		if ( pev->spawnflags & SF_BEAM_RANDOM )
			SetNextThink( m_life + RANDOM_FLOAT( 0, m_restrike ) );
		else
			SetNextThink( m_life + m_restrike );
	}
	m_active = 1;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
		{
			RandomArea( );
		}
		else
		{
			CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
			if (pStart != NULL)
				RandomPoint( pStart->pev->origin );
			else
				ALERT( at_debug, "env_beam: unknown entity \"%s\"\n", STRING(m_iszStartEntity) );
		}
		return;
	}

	CBaseEntity *pStart = RandomTargetname( STRING(m_iszStartEntity) );
	CBaseEntity *pEnd = RandomTargetname( STRING(m_iszEndEntity) );

	if ( pStart != NULL && pEnd != NULL )
	{
		if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
		{
			if ( pev->spawnflags & SF_BEAM_RING)
			{
				// don't work
				//LRC- FIXME: tell the user there's a problem.
				return;
			}
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
			{
				if ( !IsPointEntity( pEnd ) )	// One point entity must be in pEnd
				{
					CBaseEntity *pTemp;
					pTemp = pStart;
					pStart = pEnd;
					pEnd = pTemp;
				}
				if ( !IsPointEntity( pStart ) )	// One sided
				{
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( pStart->entindex() );
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}
				else
				{
					WRITE_BYTE( TE_BEAMPOINTS);
					WRITE_COORD( pStart->pev->origin.x);
					WRITE_COORD( pStart->pev->origin.y);
					WRITE_COORD( pStart->pev->origin.z);
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}


			}
			else
			{
				if ( pev->spawnflags & SF_BEAM_RING)
					WRITE_BYTE( TE_BEAMRING );
				else
					WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( pStart->entindex() );
				WRITE_SHORT( pEnd->entindex() );
			}

			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( m_frameStart ); // framestart
			WRITE_BYTE( (int)pev->framerate); // framerate
			WRITE_BYTE( (int)(m_life*10.0) ); // life
			WRITE_BYTE( m_boltWidth );  // width
			WRITE_BYTE( m_noiseAmplitude );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
			WRITE_BYTE( m_speed );		// speed
		MESSAGE_END();
		DoSparks( pStart->pev->origin, pEnd->pev->origin );
		if ( pev->dmg || !FStringNull(pev->target))
		{
			TraceResult tr;
			UTIL_TraceLine( pStart->pev->origin, pEnd->pev->origin, dont_ignore_monsters, NULL, &tr );
			if (pev->dmg)
				BeamDamageInstant( &tr, pev->dmg );

			//LRC - tripbeams
			CBaseEntity* pTrip;
			if (!FStringNull(pev->target) && (pTrip = GetTripEntity( &tr )) != NULL)
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
		}
	}
}


CBaseEntity* CBeam::GetTripEntity( TraceResult *ptr )
{
	CBaseEntity* pTrip;

	if (ptr->flFraction == 1.0 || ptr->pHit == NULL)
		return NULL;

	pTrip = CBaseEntity::Instance(ptr->pHit);
	if (pTrip == NULL)
		return NULL;

	if ( FStringNull(pev->netname))
	{
		if (pTrip->pev->flags & (FL_CLIENT | FL_MONSTER))
			return pTrip;
		else
			return NULL;
	}
	else if ( FClassnameIs(pTrip->pev, STRING(pev->netname) ))
		return pTrip;
	else if ( FStrEq( STRING(pTrip->pev->targetname), STRING(pev->netname) ))
		return pTrip;
	else
		return NULL;
}

void CBeam::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();
	if ( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntity *pHit = CBaseEntity::Instance(ptr->pHit);
		if ( pHit )
		{
			if (pev->dmg > 0)
			{
				ClearMultiDamage();
				pHit->TraceAttack( pev, pev->dmg * (gpGlobals->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, pev->frags );
				ApplyMultiDamage( pev, pev );
				if ( pev->spawnflags & SF_BEAM_DECALS )
				{
					if ( pHit->IsBSPModel() )
						UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
				}
			}
			else
			{
				//LRC - beams that heal people
				pHit->TakeHealth( -(pev->dmg * (gpGlobals->time - pev->dmgtime)), DMG_GENERIC );
			}
		}
	}
	pev->dmgtime = gpGlobals->time;
}

//LRC - used to be DamageThink, but now it's more general.
void CLightning::TripThink( void )
{
	SetNextThink( 0.1 );
	TraceResult tr;

	//ALERT(at_console,"TripThink\n");

	if (pev->dmg != 0)
	{
		UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
		BeamDamage( &tr );
	}

	//LRC - tripbeams
	if (!FStringNull(pev->target))
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if (pTrip)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
}



void CLightning::Zap( const Vector &vecSrc, const Vector &vecDest )
{
#if 1
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( m_spriteTexture );
		WRITE_BYTE( m_frameStart ); // framestart
		WRITE_BYTE( (int)pev->framerate); // framerate
		WRITE_BYTE( (int)(m_life*10.0) ); // life
		WRITE_BYTE( m_boltWidth );  // width
		WRITE_BYTE( m_noiseAmplitude );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( m_speed );		// speed
	MESSAGE_END();
#else
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE(TE_LIGHTNING);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_BYTE(10);
		WRITE_BYTE(50);
		WRITE_BYTE(40);
		WRITE_SHORT(m_spriteTexture);
	MESSAGE_END();
#endif
	DoSparks( vecSrc, vecDest );
}

void CLightning::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecSrc = pev->origin;

		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if (tr1.flFraction == 1.0)
			continue;

		Vector vecDir2;
		do {
			vecDir2 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult		tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction != 1.0)
			continue;

		Zap( tr1.vecEndPos, tr2.vecEndPos );

		break;
	}
}


void CLightning::RandomPoint( Vector &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		Vector vecDir1 = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap( vecSrc, tr1.vecEndPos );
		break;
	}
}


// LRC: Called whenever the beam gets turned on, in case an alias changed or one of the points has moved.
void CLightning::BeamUpdatePoints( void )
{
	int beamType;
	int pointStart, pointEnd;

	CBaseEntity *pStart = UTIL_FindEntityByTargetname ( NULL, STRING(m_iszStartEntity) );
	CBaseEntity *pEnd   = UTIL_FindEntityByTargetname ( NULL, STRING(m_iszEndEntity) );
	if (!pStart || !pEnd) return;
	pointStart = IsPointEntity( pStart );
	pointEnd = IsPointEntity( pEnd );

	beamType = BEAM_ENTS;
	if ( pointStart || pointEnd )
	{
		if ( !pointStart )	// One point entity must be in pStart
		{
			CBaseEntity *pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			int swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}
		if ( !pointEnd )
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType( beamType );
	if ( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pStart->pev->origin );
		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->pev->origin );
		else
			SetEndEntity( ENTINDEX(ENT(pEnd->pev)) );
	}
	else
	{
		SetStartEntity( ENTINDEX(ENT(pStart->pev)) );
		SetEndEntity( ENTINDEX(ENT(pEnd->pev)) );
	}

	RelinkBeam();
}

void CLightning::BeamUpdateVars( void )
{
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture( m_spriteTexture );

	BeamUpdatePoints(); //LRC

	SetWidth( m_boltWidth );
	SetNoise( m_noiseAmplitude );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );
	if ( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if ( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if ( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );
}


LINK_ENTITY_TO_CLASS( env_laser, CLaser );

TYPEDESCRIPTION	CLaser::m_SaveData[] = 
{
	DEFINE_FIELD( CLaser, m_pStartSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CLaser, m_pEndSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CLaser, m_iszStartSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLaser, m_iszEndSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLaser, m_firePosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( CLaser, m_iProjection, FIELD_INTEGER ),
	DEFINE_FIELD( CLaser, m_iStoppedBy, FIELD_INTEGER ),
	DEFINE_FIELD( CLaser, m_iszStartPosition, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CLaser, CBeam );

void CLaser::Spawn( void )
{
	if ( FStringNull( pev->model ) )
	{
		SetThink(&CLaser:: SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	SetThink(&CLaser:: StrikeThink );
	pev->flags |= FL_CUSTOMENTITY;
}

void CLaser::PostSpawn( void )
{
	if ( m_iszStartSpriteName )
	{
		//LRC: allow the spritename to be the name of an env_sprite
		CBaseEntity *pTemp = UTIL_FindEntityByTargetname(NULL, STRING(m_iszStartSpriteName));
		if (pTemp == NULL)
		{
			m_pStartSprite = CSprite::SpriteCreate( STRING(m_iszStartSpriteName), pev->origin, TRUE );
			if (m_pStartSprite)
				m_pStartSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
		}
		else if (!FClassnameIs(pTemp->pev, "env_sprite"))
		{
			ALERT(at_error, "env_laser \"%s\" found startsprite %s, but can't use: not an env_sprite\n", STRING(pev->targetname), STRING(m_iszStartSpriteName));
			m_pStartSprite = NULL;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pStartSprite = (CSprite*)pTemp;
			m_pStartSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else if ( pev->spawnflags & SF_LASER_INTERPOLATE) // interpolated lasers must have sprites at the start
	{
		m_pStartSprite = CSprite::SpriteCreate( "sprites/null.spr", pev->origin, TRUE );
	}
	else
		m_pStartSprite = NULL;


	if ( m_iszEndSpriteName )
	{
		CBaseEntity *pTemp = UTIL_FindEntityByTargetname(NULL, STRING(m_iszEndSpriteName));
		if (pTemp == NULL)
		{
			m_pEndSprite = CSprite::SpriteCreate( STRING(m_iszEndSpriteName), pev->origin, TRUE );
			if (m_pEndSprite)
				m_pEndSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );
		}
		else if (!FClassnameIs(pTemp->pev, "env_sprite"))
		{
			ALERT(at_error, "env_laser \"%s\" found endsprite %s, but can't use: not an env_sprite\n", STRING(pev->targetname), STRING(m_iszEndSpriteName));
			m_pEndSprite = NULL;
		}
		else
		{
			// use an env_sprite defined by the mapper
			m_pEndSprite = (CSprite*)pTemp;
			m_pEndSprite->pev->movetype = MOVETYPE_NOCLIP;
		}
	}
	else if ( pev->spawnflags & SF_LASER_INTERPOLATE) // interpolated lasers must have sprites at the end
	{
		m_pEndSprite = CSprite::SpriteCreate( "sprites/null.spr", pev->origin, TRUE );
	}
	else
		m_pEndSprite = NULL;

	//LRC
	if ( m_pStartSprite && m_pEndSprite && pev->spawnflags & SF_LASER_INTERPOLATE )
		EntsInit( m_pStartSprite->entindex(), m_pEndSprite->entindex() );
	else
		PointsInit( pev->origin, pev->origin );

	if ( pev->targetname && !(pev->spawnflags & SF_BEAM_STARTON) )
		TurnOff();
	else
		TurnOn();
}

void CLaser::Precache( void )
{
	PRECACHE_MODEL( "sprites/null.spr" );
	pev->modelindex = PRECACHE_MODEL( (char *)STRING(pev->model) );
	if ( m_iszStartSpriteName )
	{
		// UGLY HACK to check whether this is a filename: does it contain a dot?
		const char *c = STRING(m_iszStartSpriteName);
		while (*c)
		{
			if (*c == '.')
			{
				PRECACHE_MODEL( (char *)STRING(m_iszStartSpriteName) );
				break;
			}
			c++; // the magic word?
		}
	}

	if ( m_iszEndSpriteName )
	{
		const char *c = STRING(m_iszEndSpriteName);
		while (*c)
		{
			if (*c == '.')
			{
				PRECACHE_MODEL( (char *)STRING(m_iszEndSpriteName) );
				break;
			}
			c++;
		}
	}
}


void CLaser::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LaserStart"))
	{
		m_iszStartPosition = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTowardsMode"))
	{
		m_iTowardsMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StartSprite"))
	{
		m_iszStartSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszEndSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iProjection"))
	{
		m_iProjection = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStoppedBy"))
	{
		m_iStoppedBy = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeam::KeyValue( pkvd );
}


void CLaser::TurnOff( void )
{
	pev->effects |= EF_NODRAW;
	DontThink();
	if ( m_pStartSprite )
	{
		m_pStartSprite->TurnOff();
		UTIL_SetVelocity(m_pStartSprite, g_vecZero); //LRC
	}
	if ( m_pEndSprite )
	{
		m_pEndSprite->TurnOff();
		UTIL_SetVelocity(m_pEndSprite, g_vecZero); //LRC
	}
}


void CLaser::TurnOn( void )
{
	pev->effects &= ~EF_NODRAW;

	if ( m_pStartSprite )
		m_pStartSprite->TurnOn();

	if ( m_pEndSprite )
		m_pEndSprite->TurnOn();

	pev->dmgtime = gpGlobals->time;

	if ( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if ( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
	else if ( pev->spawnflags & SF_BEAM_SOLID )
		SetFlags( BEAM_FSOLID );

	SetNextThink( 0 );
}


void CLaser::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int active = (GetState() == STATE_ON);

	if ( !ShouldToggle( useType, active ) )
		return;
	if ( active )
		TurnOff();
	else
		TurnOn();
}


void CLaser::FireAtPoint( Vector startpos, TraceResult &tr )
{
	if (pev->spawnflags & SF_LASER_INTERPOLATE && m_pStartSprite && m_pEndSprite)
	{
		UTIL_SetVelocity(m_pStartSprite, (startpos - m_pStartSprite->pev->origin)*10 );
		UTIL_SetVelocity(m_pEndSprite, (tr.vecEndPos - m_pEndSprite->pev->origin)*10 );
	}
	else
	{
		if (m_pStartSprite)
			UTIL_AssignOrigin( m_pStartSprite, startpos );

		if (m_pEndSprite)
			UTIL_AssignOrigin( m_pEndSprite, tr.vecEndPos );

		SetStartPos( startpos );
		SetEndPos( tr.vecEndPos );
	}

	BeamDamage( &tr );
	DoSparks( startpos, tr.vecEndPos );
}

void CLaser::StrikeThink( void )
{
	Vector startpos = pev->origin;
	if (m_iszStartPosition)
	{
		startpos = CalcLocus_Position(this, NULL, STRING(m_iszStartPosition));
	}

	if (m_iTowardsMode)
	{
		m_firePosition = startpos + CalcLocus_Velocity(this, NULL, STRING(pev->message));
	}
	else
	{
		CBaseEntity *pEnd = RandomTargetname( STRING(pev->message) );

		if ( pEnd )
			m_firePosition = pEnd->pev->origin;
	}
	
	TraceResult tr;

//LRC
//	UTIL_TraceLine( pev->origin, m_firePosition, dont_ignore_monsters, NULL, &tr );
	IGNORE_GLASS iIgnoreGlass;
	if (m_iStoppedBy % 2) // if it's an odd number
		iIgnoreGlass = ignore_glass;
	else
		iIgnoreGlass = dont_ignore_glass;

	IGNORE_MONSTERS iIgnoreMonsters;
	if (m_iStoppedBy <= 1)
		iIgnoreMonsters = dont_ignore_monsters;
	else if (m_iStoppedBy <= 3)
		iIgnoreMonsters = missile;
	else
		iIgnoreMonsters = ignore_monsters;

	if ( m_iProjection )
	{
		Vector vecProject = startpos + 4096*((m_firePosition - startpos).Normalize());
		UTIL_TraceLine( startpos, vecProject, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );
	}
	else
	{
		UTIL_TraceLine( startpos, m_firePosition, iIgnoreMonsters, iIgnoreGlass, NULL, &tr );
	}

	FireAtPoint( startpos, tr );

	//LRC - tripbeams
	if (pev->target)
	{
		// nicked from monster_tripmine:
		//HACKHACK Set simple box using this really nice global!
		gpGlobals->trace_flags = FTRACE_SIMPLEBOX;
		UTIL_TraceLine( startpos, m_firePosition, dont_ignore_monsters, NULL, &tr );
		CBaseEntity *pTrip = GetTripEntity( &tr );
		if (pTrip)
		{
			if (!FBitSet(pev->spawnflags, SF_BEAM_TRIPPED))
			{
				FireTargets(STRING(pev->target), pTrip, this, USE_TOGGLE, 0);
				pev->spawnflags |= SF_BEAM_TRIPPED;
			}
		}
		else
		{
			pev->spawnflags &= ~SF_BEAM_TRIPPED;
		}
	}
	SetNextThink( 0.1 );
}



class CGlow : public CPointEntity
{
public:
	void Spawn( void );
	void Think( void );
	void Animate( float frames );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float		m_lastTime;
	float		m_maxFrame;
};

LINK_ENTITY_TO_CLASS( env_glow, CGlow );

TYPEDESCRIPTION	CGlow::m_SaveData[] = 
{
	DEFINE_FIELD( CGlow, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CGlow, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CGlow, CPointEntity );

void CGlow::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	PRECACHE_MODEL( (char *)STRING(pev->model) );
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( m_maxFrame > 1.0 && pev->framerate != 0 )
		SetNextThink( 0.1 );

	m_lastTime = gpGlobals->time;
}


void CGlow::Think( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	SetNextThink( 0.1 );
	m_lastTime			= gpGlobals->time;
}


void CGlow::Animate( float frames )
{ 
	if ( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );
}


LINK_ENTITY_TO_CLASS( env_sprite, CSprite );

TYPEDESCRIPTION	CSprite::m_SaveData[] = 
{
	DEFINE_FIELD( CSprite, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CSprite, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSprite, CPointEntity );

void CSprite::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( pev->targetname && !(pev->spawnflags & SF_SPRITE_STARTON) )
		TurnOff();
	else
		TurnOn();
	
	// Worldcraft only sets y rotation, copy to Z
	if ( pev->angles.y != 0 && pev->angles.z == 0 )
	{
		pev->angles.z = pev->angles.y;
		pev->angles.y = 0;
	}
}


void CSprite::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );

	// Reset attachment after save/restore
	if ( pev->aiment )
		SetAttachment( pev->aiment, pev->body );
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}


void CSprite::SpriteInit( const char *pSpriteName, const Vector &origin )
{
	pev->model = MAKE_STRING(pSpriteName);
	pev->origin = origin;
	Spawn();
}

CSprite *CSprite::SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate )
{
	CSprite *pSprite = GetClassPtr( (CSprite *)NULL );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->pev->classname = MAKE_STRING("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}


void CSprite::AnimateThink( void )
{
	Animate( pev->framerate * (gpGlobals->time - m_lastTime) );

	SetNextThink( 0.1 );
	m_lastTime			= gpGlobals->time;
}

void CSprite::AnimateUntilDead( void )
{
	if ( gpGlobals->time > pev->dmgtime )
		UTIL_Remove(this);
	else
	{
		AnimateThink();
		SetNextThink( 0 );
	}
}

void CSprite::Expand( float scaleSpeed, float fadeSpeed )
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThink(&CSprite:: ExpandThink );

	SetNextThink( 0 );
	m_lastTime		= gpGlobals->time;
}


void CSprite::ExpandThink( void )
{
	float frametime = gpGlobals->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if ( pev->renderamt <= 0 )
	{
		pev->renderamt = 0;
		UTIL_Remove( this );
	}
	else
	{
		SetNextThink( 0.1 );
		m_lastTime			= gpGlobals->time;
	}
}


void CSprite::Animate( float frames )
{ 
	pev->frame += frames;
	if ( pev->frame > m_maxFrame )
	{
		if ( pev->spawnflags & SF_SPRITE_ONCE )
		{
			TurnOff();
		}
		else
		{
			if ( m_maxFrame > 0 )
				pev->frame = fmod( pev->frame, m_maxFrame );
		}
	}
}


void CSprite::TurnOff( void )
{
	pev->effects = EF_NODRAW;
	DontThink();
}


void CSprite::TurnOn( void )
{
	if (pev->message)
	{
		CBaseEntity *pTemp = UTIL_FindEntityByTargetname(NULL, STRING(pev->message));
		if (pTemp)
			SetAttachment(pTemp->edict(), pev->frags);
		else
			return;
	}
	pev->effects = 0;
	if ( (pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) )
	{
		SetThink(&CSprite:: AnimateThink );
		SetNextThink( 0 );
		m_lastTime = gpGlobals->time;
	}
	pev->frame = 0;
}


void CSprite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on = pev->effects != EF_NODRAW;
	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			SUB_UseTargets( this, USE_OFF, 0 ); //LRC
			TurnOff();
		}
		else
		{
			SUB_UseTargets( this, USE_ON, 0 ); //LRC
			TurnOn();
		}
	}
}

//=================================================================
// env_model: like env_sprite, except you can specify a sequence.
//=================================================================
#define SF_ENVMODEL_OFF			1
#define SF_ENVMODEL_DROPTOFLOOR	2
#define SF_ENVMODEL_SOLID		4

TYPEDESCRIPTION CEnvModel::m_SaveData[] =
{
	DEFINE_FIELD( CEnvModel, m_iszSequence_On, FIELD_STRING ),
	DEFINE_FIELD( CEnvModel, m_iszSequence_Off, FIELD_STRING ),
	DEFINE_FIELD( CEnvModel, m_iAction_On, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_iAction_Off, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_ChestNumbers, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, number1, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, number2, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvModel, m_bIssuedNote, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CEnvModel, CBaseAnimating );
LINK_ENTITY_TO_CLASS( env_model, CEnvModel );

void CEnvModel::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iszSequence_On"))
	{
		m_iszSequence_On = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSequence_Off"))
	{
		m_iszSequence_Off = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_On"))
	{
		m_iAction_On = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAction_Off"))
	{
		m_iAction_Off = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "chestnums"))
	{
		m_ChestNumbers = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseAnimating::KeyValue( pkvd );
	}
}

void CEnvModel::VisibilityCheck( void )
{
	if ( pev->iuser1 )
	{
		bool has1 = false;
		bool has2 = false;
		bool has3 = false;
		bool has4 = false;

		if ( CVAR_GET_FLOAT( "mp_copsalwaysvisible" ) > 0 )
			has1 = has2 = has3 = has4 = true;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr && plr->pev->fuser4 == 0 )
			{
				switch ( i )
				{
				case 1:
					has1 = true;
					pev->effects &= ~EF_NODRAW;
					break;

				case 2:
					has2 = true;
					pev->effects &= ~EF_NODRAW;
					break;

				case 3:
					has3 = true;
					pev->effects &= ~EF_NODRAW;
					break;

				case 4:
					has4 = true;
					pev->effects &= ~EF_NODRAW;
					break;
				}
			}
		}

		switch ( pev->iuser1 )
		{
		case 1:
			if ( !has1 )
				pev->effects |= EF_NODRAW;
			break;

		case 2:
			if ( !has2 )
				pev->effects |= EF_NODRAW;
			break;

		case 3:
			if ( !has3 )
				pev->effects |= EF_NODRAW;
			break;

		case 4:
			if ( !has4 )
				pev->effects |= EF_NODRAW;
			break;
		}
	}
}

void CEnvModel :: Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetOrigin(this, pev->origin);

	if ( pev->model )
	{
		if ( strstr( STRING(pev->model), "skogsris" ) )
			pev->renderfx = 94;
	}

	if (pev->spawnflags & SF_ENVMODEL_SOLID)
	{
		pev->solid = SOLID_SLIDEBOX;
		UTIL_SetSize(pev, Vector(-10, -10, -10), Vector(10, 10, 10));	//LRCT
	}

	if (pev->spawnflags & SF_ENVMODEL_DROPTOFLOOR)
	{
		pev->origin.z += 1;
		DROP_TO_FLOOR ( ENT(pev) );
	}

	if ( !pev->renderfx )
	{
		if ( pev->model )
		{
			if ( !strstr( STRING(pev->model), "obbsror" ) )
				pev->renderfx = 92;
		}
	}

	SetBoneController( 0, pev->fuser1 );
	SetBoneController( 1, pev->fuser2 );
	SetBoneController( 2, pev->fuser3 );
	SetBoneController( 3, pev->fuser4 );

	SetSequence();
	
	SetNextThink( 0.1 );

	number1 = RANDOM_LONG( 0, 8 );
	number2 = RANDOM_LONG( 0, 8 );

	if ( m_ChestNumbers )
	{
		SetBodygroup( 1, number1 );
		SetBodygroup( 2, number2 );
	}
}

void CEnvModel::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
}

STATE CEnvModel::GetState( void )
{
	if (pev->spawnflags & SF_ENVMODEL_OFF)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvModel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_ChestNumbers )
	{
		int realnumber = number1 + 1;
		int realnumber2 = number2 + 1;
		char szTempKey[8];

		sprintf( szTempKey, "%i7%i9", realnumber, realnumber2 );

		int codenumber = atoi( szTempKey );

		CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

		if ( player )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

			if ( pPlayer )
				pPlayer->m_iKeypadNumber = codenumber;
		}
	}

	if (ShouldToggle(useType, !(pev->spawnflags & SF_ENVMODEL_OFF)))
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			pev->spawnflags &= ~SF_ENVMODEL_OFF;
		else
			pev->spawnflags |= SF_ENVMODEL_OFF;

		SetSequence();
		SetNextThink( 0.1 );
	}
}

float CEnvModel::VecToYaw( Vector vecDir )
{
	if (vecDir.x == 0 && vecDir.y == 0 && vecDir.z == 0)
		return pev->angles.y;

	return UTIL_VecToYaw( vecDir );
}

void CEnvModel::Think( void )
{
	int iTemp;

	if ( pev->spawnflags & 8 )
	{
		CBaseEntity *pent = UTIL_FindEntityByClassname( NULL, "player" );

		if ( pent && !m_gbIsInCutscene )
		{
			Vector2D vec2LOS;


			UTIL_MakeVectors( pev->angles );

			vec2LOS = ( pent->pev->origin - pev->origin ).Make2D();
			vec2LOS = vec2LOS.Normalize();

			float flDot = DotProduct( vec2LOS, gpGlobals->v_forward.Make2D() );


			if ( flDot >= 0 )
			{
				float yaw = VecToYaw( pent->pev->origin - pev->origin ) - pev->angles.y;

				if (yaw > 180) yaw -= 360;
				if (yaw < -180) yaw += 360;


				SetBoneController( 0, yaw );
			}
		}

		if ( m_gbIsInCutscene )
		{

			SetBoneController( 0, 0 );
		}
	}

//	ALERT(at_console, "env_model Think fr=%f\n", pev->framerate);

	StudioFrameAdvance ( ); // set m_fSequenceFinished if necessary

//	if (m_fSequenceLoops)
//	{
//		SetNextThink( 1E6 );
//		return; // our work here is done.
//	}
	if (m_fSequenceFinished && !m_fSequenceLoops)
	{
		if (pev->spawnflags & SF_ENVMODEL_OFF)
			iTemp = m_iAction_Off;
		else
			iTemp = m_iAction_On;

		switch (iTemp)
		{
//		case 1: // loop
//			pev->animtime = gpGlobals->time;
//			m_fSequenceFinished = FALSE;
//			m_flLastEventCheck = gpGlobals->time;
//			pev->frame = 0;
//			break;
		case 2: // change state
			if (pev->spawnflags & SF_ENVMODEL_OFF)
				pev->spawnflags &= ~SF_ENVMODEL_OFF;
			else
				pev->spawnflags |= SF_ENVMODEL_OFF;
			SetSequence();
			break;
		default: //remain frozen
			return;
		}
	}

	if ( pev->spawnflags & 8 )
		SetNextThink( 0 );
	else
		SetNextThink( 0.1 );
}

void CEnvModel :: SetSequence( void )
{
	int iszSeq;

	if (pev->spawnflags & SF_ENVMODEL_OFF)
		iszSeq = m_iszSequence_Off;
	else
		iszSeq = m_iszSequence_On;

	if (!iszSeq)
		return;

	VisibilityCheck();

	pev->sequence = LookupSequence( STRING( iszSeq ));

	if (pev->sequence == -1)
	{
		if (pev->targetname)
			ALERT( at_error, "env_model %s: unknown sequence \"%s\"\n", STRING( pev->targetname ), STRING( iszSeq ));
		else
			ALERT( at_error, "env_model: unknown sequence \"%s\"\n", STRING( pev->targetname ), STRING( iszSeq ));
		pev->sequence = 0;
	}

	pev->frame = 0;
	ResetSequenceInfo( );

	if (pev->spawnflags & SF_ENVMODEL_OFF)
	{
		if (m_iAction_Off == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
	else
	{
		if (m_iAction_On == 1)
			m_fSequenceLoops = 1;
		else
			m_fSequenceLoops = 0;
	}
}


#define	SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired
#define SF_GIBSHOOTER_DEBUG			4 //LRC

class CGibShooter : public CBaseDelay
{
public:
	virtual void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void EXPORT ShootThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual CBaseEntity *CreateGib( Vector vecPos, Vector vecVel );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int	m_iGibs;
	int m_iGibCapacity;
	int m_iGibMaterial;
	int m_iGibModelIndex;
//	float m_flGibVelocity;
	float m_flVariance;
	float m_flGibLife;
	int m_iszTargetname;
	int m_iszPosition;
	int m_iszVelocity;
	int m_iszVelFactor;
	int m_iszSpawnTarget;
	int m_iBloodColor;
};

TYPEDESCRIPTION CGibShooter::m_SaveData[] =
{
	DEFINE_FIELD( CGibShooter, m_iGibs, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibCapacity, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( CGibShooter, m_iGibModelIndex, FIELD_INTEGER ),
//	DEFINE_FIELD( CGibShooter, m_flGibVelocity, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_flVariance, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_flGibLife, FIELD_FLOAT ),
	DEFINE_FIELD( CGibShooter, m_iszTargetname, FIELD_STRING),
	DEFINE_FIELD( CGibShooter, m_iszPosition, FIELD_STRING),
	DEFINE_FIELD( CGibShooter, m_iszVelocity, FIELD_STRING),
	DEFINE_FIELD( CGibShooter, m_iszVelFactor, FIELD_STRING),
	DEFINE_FIELD( CGibShooter, m_iszSpawnTarget, FIELD_STRING),
	DEFINE_FIELD( CGibShooter, m_iBloodColor, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CGibShooter, CBaseDelay );
LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );


void CGibShooter :: Precache ( void )
{
	if ( g_Language == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/germanygibs.mdl");
	}
	else if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/agibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PRECACHE_MODEL ("models/hgibs.mdl");
	}
}


void CGibShooter::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iGibs"))
	{
		m_iGibs = m_iGibCapacity = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVelocity"))
	{
		m_iszVelFactor = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flVariance"))
	{
		m_flVariance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flGibLife"))
	{
		m_flGibLife = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTargetName"))
	{
		m_iszTargetname = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelocity"))
	{
		m_iszVelocity = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszVelFactor"))
	{
		m_iszVelFactor = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSpawnTarget"))
	{
		m_iszSpawnTarget = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iBloodColor"))
	{
		m_iBloodColor = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
	{
		CBaseDelay::KeyValue( pkvd );
	}
}

void CGibShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;
	SetThink(&CGibShooter:: ShootThink );
	SetNextThink( 0 );
}

void CGibShooter::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

//	if ( m_flDelay == 0 )
//	{
//		m_flDelay = 0.1;
//	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	SetMovedir ( pev );
	if (pev->body == 0)
		pev->body = MODEL_FRAMES( m_iGibModelIndex );
}


CBaseEntity *CGibShooter :: CreateGib ( Vector vecPos, Vector vecVel )
{
	if ( CVAR_GET_FLOAT("violence_hgibs") == 0 )
		return NULL;

	CGib *pGib = GetClassPtr( (CGib *)NULL );
//	if (pGib)
//		ALERT(at_console, "Gib created ok\n");

	pGib->pev->origin = vecPos;
	pGib->pev->velocity = vecVel;

	if (m_iBloodColor == BLOOD_COLOR_YELLOW)
	{
		pGib->Spawn( "models/agibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
	}
	else if (m_iBloodColor)
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = m_iBloodColor;
	}
	else
	{
		pGib->Spawn( "models/hgibs.mdl" );
		pGib->m_bloodColor = BLOOD_COLOR_RED;
	}

	if ( pev->body <= 1 )
	{
		ALERT ( at_aiconsole, "GibShooter Body is <= 1!\n" );
	}

	pGib->pev->body = RANDOM_LONG ( 1, pev->body - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	float thinkTime = pGib->m_fNextThink - gpGlobals->time;

	pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT( 0.95, 1.05 ));	// +/- 5%
	if ( pGib->m_lifeTime < thinkTime )
	{
		pGib->SetNextThink( pGib->m_lifeTime );
		pGib->m_lifeTime = 0;
	}

	pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
	pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

	return pGib;
}


void CGibShooter :: ShootThink ( void )
{
	int i;
	if (m_flDelay == 0) // LRC - delay is 0, fire them all at once.
	{
		i = m_iGibs;
	}
	else
	{
		i = 1;
		SetNextThink( m_flDelay );
	}

	while (i > 0)
	{
		Vector vecShootDir;
		Vector vecPos;
		float flGibVelocity;
		if (!FStringNull(m_iszVelFactor))
			flGibVelocity = CalcLocus_Ratio(m_hActivator, STRING(m_iszVelFactor));
		else
			flGibVelocity = 1;

		if (!FStringNull(m_iszVelocity))
		{
			vecShootDir = CalcLocus_Velocity(this, m_hActivator, STRING(m_iszVelocity));
			flGibVelocity = flGibVelocity * vecShootDir.Length();
			vecShootDir = vecShootDir.Normalize();
		}
		else
			vecShootDir = pev->movedir;

		vecShootDir = vecShootDir + gpGlobals->v_right * RANDOM_FLOAT( -1, 1) * m_flVariance;;
		vecShootDir = vecShootDir + gpGlobals->v_forward * RANDOM_FLOAT( -1, 1) * m_flVariance;;
		vecShootDir = vecShootDir + gpGlobals->v_up * RANDOM_FLOAT( -1, 1) * m_flVariance;;

		vecShootDir = vecShootDir.Normalize();

		if (!FStringNull(m_iszPosition))
			vecPos = CalcLocus_Position(this, m_hActivator, STRING(m_iszPosition));
		else
			vecPos = pev->origin;
		CBaseEntity *pGib = CreateGib(vecPos, vecShootDir * flGibVelocity);
	
		if ( pGib )
		{
			pGib->pev->targetname = m_iszTargetname;
//			pGib->pev->velocity = vecShootDir * flGibVelocity;

			if (pev->spawnflags & SF_GIBSHOOTER_DEBUG)
				ALERT(at_debug, "DEBUG: %s \"%s\" creates a shot at %f %f %f; vel %f %f %f; pos \"%s\"\n", STRING(pev->classname), STRING(pev->targetname), pGib->pev->origin.x, pGib->pev->origin.y, pGib->pev->origin.z, pGib->pev->velocity.x, pGib->pev->velocity.y, pGib->pev->velocity.z, STRING(m_iszPosition));

			if (m_iszSpawnTarget)
				FireTargets( STRING(m_iszSpawnTarget), pGib, this, USE_TOGGLE, 0 );
		}

		i--;
		m_iGibs--;
	}

	if ( m_iGibs <= 0 )
	{
		if ( pev->spawnflags & SF_GIBSHOOTER_REPEATABLE )
		{
			m_iGibs = m_iGibCapacity;
			SetThink ( NULL );
			DontThink();
		}
		else
		{
			SetThink(&CGibShooter :: SUB_Remove );
			SetNextThink( 0 );
		}
	}
}


// Shooter particle
class CShot : public CSprite
{
public:
	void Touch ( CBaseEntity *pOther );
};

void CShot :: Touch ( CBaseEntity *pOther )
{
	if (pev->teleport_time > gpGlobals->time)
		return;
	// don't fire too often in collisions!
	// teleport_time is the soonest this can be touched again.
	pev->teleport_time = gpGlobals->time + 0.1;

	if (pev->netname)
		FireTargets( STRING(pev->netname), this, this, USE_TOGGLE, 0 );
	if (pev->message && pOther && pOther != g_pWorld)
		FireTargets( STRING(pev->message), pOther, this, USE_TOGGLE, 0 );
}

class CEnvShooter : public CGibShooter
{
	void		Precache( void );
	void		KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	void		Spawn( void );

	static	TYPEDESCRIPTION m_SaveData[];

	CBaseEntity	*CreateGib( Vector vecPos, Vector vecVel );

	int m_iszTouch;
	int m_iszTouchOther;
	int m_iPhysics;
	float m_fFriction;
	Vector m_vecSize;
};

TYPEDESCRIPTION	CEnvShooter::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvShooter, m_iszTouch, FIELD_STRING),
	DEFINE_FIELD( CEnvShooter, m_iszTouchOther, FIELD_STRING),
	DEFINE_FIELD( CEnvShooter, m_iPhysics, FIELD_INTEGER),
	DEFINE_FIELD( CEnvShooter, m_fFriction, FIELD_FLOAT),
	DEFINE_FIELD( CEnvShooter, m_vecSize, FIELD_VECTOR),
};

IMPLEMENT_SAVERESTORE(CEnvShooter,CGibShooter);
LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

void CEnvShooter::Spawn( void )
{
	int iBody = pev->body;
	CGibShooter::Spawn();
	pev->body = iBody;
}

void CEnvShooter :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "shootmodel"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "shootsounds"))
	{
		int iNoise = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;
		
		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouch"))
	{
		m_iszTouch = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszTouchOther"))
	{
		m_iszTouchOther = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iPhysics"))
	{
		m_iPhysics = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fFriction"))
	{
		m_fFriction = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_vecSize"))
	{
		UTIL_StringToVector((float*)m_vecSize, pkvd->szValue);
		m_vecSize = m_vecSize/2;
		pkvd->fHandled = TRUE;
	}
	else
	{
		CGibShooter::KeyValue( pkvd );
	}
}


void CEnvShooter :: Precache ( void )
{
	if (pev->model)
		m_iGibModelIndex = PRECACHE_MODEL( (char *)STRING(pev->model) );
	CBreakable::MaterialSoundPrecache( (Materials)m_iGibMaterial );
}


CBaseEntity *CEnvShooter :: CreateGib ( Vector vecPos, Vector vecVel )
{
	if (m_iPhysics <= 1) // normal gib or sticky gib
{
	CGib *pGib = GetClassPtr( (CGib *)NULL );

		pGib->pev->origin = vecPos;
		pGib->pev->velocity = vecVel;

	pGib->Spawn( STRING(pev->model) );
		if (m_iPhysics) // sticky gib
		{
			pGib->pev->movetype = MOVETYPE_TOSS;
			pGib->pev->solid = SOLID_BBOX;
			UTIL_SetSize ( pGib->pev, Vector ( 0, 0 ,0 ), Vector ( 0, 0, 0 ) );
			pGib->SetTouch(&CGib::StickyGibTouch);
		}
		if ( pev->body > 0 )
			pGib->pev->body = RANDOM_LONG( 0, pev->body-1 );
		if (m_iBloodColor)
			pGib->m_bloodColor = m_iBloodColor;
		else
			pGib->m_bloodColor = DONT_BLEED;
	pGib->m_material = m_iGibMaterial;

	pGib->pev->rendermode = pev->rendermode;
	pGib->pev->renderamt = pev->renderamt;
	pGib->pev->rendercolor = pev->rendercolor;
	pGib->pev->renderfx = pev->renderfx;
	pGib->pev->scale = pev->scale;
	pGib->pev->skin = pev->skin;

		float thinkTime = pGib->m_fNextThink - gpGlobals->time;

		pGib->m_lifeTime = (m_flGibLife * RANDOM_FLOAT( 0.95, 1.05 ));	// +/- 5%
		if ( pGib->m_lifeTime < thinkTime )
		{
			pGib->SetNextThink( pGib->m_lifeTime );
			pGib->m_lifeTime = 0;
		}

		pGib->pev->avelocity.x = RANDOM_FLOAT ( 100, 200 );
		pGib->pev->avelocity.y = RANDOM_FLOAT ( 100, 300 );

		return pGib;
	}

	// special shot
	CShot *pShot = GetClassPtr( (CShot*)NULL );
	pShot->pev->classname = MAKE_STRING("shot");
	pShot->pev->solid = SOLID_SLIDEBOX;
	pShot->pev->origin = vecPos;
	pShot->pev->velocity = vecVel;
	SET_MODEL(ENT(pShot->pev), STRING(pev->model));
	UTIL_SetSize(pShot->pev, -m_vecSize, m_vecSize);
	pShot->pev->renderamt = pev->renderamt;
	pShot->pev->rendermode = pev->rendermode;
	pShot->pev->rendercolor = pev->rendercolor;
	pShot->pev->renderfx = pev->renderfx;
	pShot->pev->netname = m_iszTouch;
	pShot->pev->message = m_iszTouchOther;
	pShot->pev->skin = pev->skin;
	pShot->pev->body = pev->body;
	pShot->pev->scale = pev->scale;
	pShot->pev->frame = pev->frame;
	pShot->pev->framerate = pev->framerate;
	pShot->pev->friction = m_fFriction;

	switch (m_iPhysics)
	{
	case 2: pShot->pev->movetype = MOVETYPE_NOCLIP; pShot->pev->solid = SOLID_NOT; break;
	case 3: pShot->pev->movetype = MOVETYPE_FLYMISSILE; break;
	case 4: pShot->pev->movetype = MOVETYPE_BOUNCEMISSILE; break;
	case 5: pShot->pev->movetype = MOVETYPE_TOSS; break;
	case 6: pShot->pev->movetype = MOVETYPE_BOUNCE; break;
	}

	if (pShot->pev->framerate)
	{
		pShot->m_maxFrame = (float) MODEL_FRAMES( pShot->pev->modelindex ) - 1;
		if (pShot->m_maxFrame > 1.0)
		{
			if (m_flGibLife)
			{
				pShot->pev->dmgtime = gpGlobals->time + m_flGibLife;
				pShot->SetThink(& CSprite::AnimateUntilDead );
			}
			else
			{
				pShot->SetThink(& CSprite::AnimateThink );
			}
			pShot->SetNextThink( 0 );
			pShot->m_lastTime = gpGlobals->time;
			return pShot;
		}
	}

	// if it's not animating
	if (m_flGibLife)
	{
		pShot->SetThink(&CShot::SUB_Remove);
		pShot->SetNextThink(m_flGibLife);
	}
	return pShot;
}




class CTestEffect : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	// void	KeyValue( KeyValueData *pkvd );
	void EXPORT TestThink( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iLoop;
	int		m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};


LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

void CTestEffect::Spawn( void )
{
	Precache( );
}

void CTestEffect::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
}

void CTestEffect::TestThink( void )
{
	int i;
	float t = (gpGlobals->time - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.spr", 100 );

		TraceResult		tr;

		Vector vecSrc = pev->origin;
		Vector vecDir = Vector( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir = vecDir.Normalize();
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, ignore_monsters, ENT(pev), &tr);

		pbeam->PointsInit( vecSrc, tr.vecEndPos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 100 );
		pbeam->SetScrollRate( 12 );
		
		m_flBeamTime[m_iBeam] = gpGlobals->time;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;

#if 0
		Vector vecMid = (vecSrc + tr.vecEndPos) * 0.5;
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE(TE_DLIGHT);
			WRITE_COORD(vecMid.x);	// X
			WRITE_COORD(vecMid.y);	// Y
			WRITE_COORD(vecMid.z);	// Z
			WRITE_BYTE( 20 );		// radius * 0.1
			WRITE_BYTE( 255 );		// r
			WRITE_BYTE( 180 );		// g
			WRITE_BYTE( 100 );		// b
			WRITE_BYTE( 20 );		// time * 10
			WRITE_BYTE( 0 );		// decay * 0.1
		MESSAGE_END( );
#endif
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->time - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		SetNextThink( 0.1 );
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->time;
		m_iBeam = 0;
		// pev->nextthink = gpGlobals->time;
		SetThink( NULL );
	}
}


void CTestEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink(&CTestEffect:: TestThink );
	SetNextThink( 0.1 );
	m_flStartTime = gpGlobals->time;
}



// Blood effects
class CBlood : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	int		Color( void ) { return pev->impulse; }
	inline	float 	BloodAmount( void ) { return pev->dmg; }

	inline	void SetColor( int color ) { pev->impulse = color; }
	inline	void SetBloodAmount( float amount ) { pev->dmg = amount; }
	
	Vector	Direction( CBaseEntity *pActivator ); //LRC - added pActivator, for locus system
	Vector	BloodPosition( CBaseEntity *pActivator );

private:
};

LINK_ENTITY_TO_CLASS( env_blood, CBlood );



#define SF_BLOOD_RANDOM		0x0001
#define SF_BLOOD_STREAM		0x0002
#define SF_BLOOD_PLAYER		0x0004
#define SF_BLOOD_DECAL		0x0008

void CBlood::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;
	SetMovedir( pev );
	if (Color() == 0) SetColor( BLOOD_COLOR_RED );
}


void CBlood::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "color"))
	{
		int color = atoi(pkvd->szValue);
		switch( color )
		{
		case 1:
			SetColor( BLOOD_COLOR_YELLOW );
			break;
		default:
			SetColor( BLOOD_COLOR_RED );
			break;
		}

		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "amount"))
	{
		SetBloodAmount( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


Vector CBlood::Direction( CBaseEntity *pActivator )
{
	if ( pev->spawnflags & SF_BLOOD_RANDOM )
		return UTIL_RandomBloodVector();
	else if (pev->netname)
		return CalcLocus_Velocity(this, pActivator, STRING(pev->netname));
	else
	return pev->movedir;
}


Vector CBlood::BloodPosition( CBaseEntity *pActivator )
{
	if ( pev->spawnflags & SF_BLOOD_PLAYER )
	{
		edict_t *pPlayer;

		if ( pActivator && pActivator->IsPlayer() )
		{
			pPlayer = pActivator->edict();
		}
		else
			pPlayer = g_engfuncs.pfnPEntityOfEntIndex( 1 );
		if ( pPlayer )
			return (pPlayer->v.origin + pPlayer->v.view_ofs) + Vector( RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10), RANDOM_FLOAT(-10,10) );
		// if no player found, fall through
	}
	else if (pev->target)
	{
		return CalcLocus_Position(this, pActivator, STRING(pev->target));
	}

	return pev->origin;
}


void CBlood::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & SF_BLOOD_STREAM )
		UTIL_BloodStream( BloodPosition(pActivator), Direction(pActivator), (Color() == BLOOD_COLOR_RED) ? 70 : Color(), BloodAmount() );
	else
		UTIL_BloodDrips( BloodPosition(pActivator), Direction(pActivator), Color(), BloodAmount(), g_vecZero );

	if ( pev->spawnflags & SF_BLOOD_DECAL )
	{
		Vector forward = Direction(pActivator);
		Vector start = BloodPosition( pActivator );
		TraceResult tr;

		UTIL_TraceLine( start, start + forward * BloodAmount() * 2, ignore_monsters, NULL, &tr );
		if ( tr.flFraction != 1.0 )
			UTIL_BloodDecalTrace( &tr, Color() );
	}
}



// Screen shake
class CShake : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	inline	float	Amplitude( void ) { return pev->scale; }
	inline	float	Frequency( void ) { return pev->dmg_save; }
	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	Radius( void ) { return pev->dmg; }

	inline	void	SetAmplitude( float amplitude ) { pev->scale = amplitude; }
	inline	void	SetFrequency( float frequency ) { pev->dmg_save = frequency; }
	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetRadius( float radius ) { pev->dmg = radius; }

	STATE m_iState; //LRC
	virtual STATE GetState( void ) { return m_iState; }; //LRC
	void	Think( void ) { m_iState = STATE_OFF; }; //LRC
private:
};

LINK_ENTITY_TO_CLASS( env_shake, CShake );

// pev->scale is amplitude
// pev->dmg_save is frequency
// pev->dmg_take is duration
// pev->dmg is radius
// radius of 0 means all players
// NOTE: UTIL_ScreenShake() will only shake players who are on the ground

#define SF_SHAKE_EVERYONE	0x0001		// Don't check radius
// UNDONE: These don't work yet
#define SF_SHAKE_DISRUPT	0x0002		// Disrupt controls
#define SF_SHAKE_INAIR		0x0004		// Shake players in air

void CShake::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;
	
	m_iState = STATE_OFF; //LRC

	if ( pev->spawnflags & SF_SHAKE_EVERYONE )
		pev->dmg = 0;
}


void CShake::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "amplitude"))
	{
		SetAmplitude( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		SetFrequency( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "duration"))
	{
		SetDuration( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		SetRadius( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CShake::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	UTIL_ScreenShake( pev->origin, Amplitude(), Frequency(), Duration(), Radius() );
	m_iState = STATE_ON; //LRC
	SetNextThink( Duration() ); //LRC
}


class CFade : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );

	virtual STATE GetState( void ) { return m_iState; }; // LRC
	void	Think( void ); //LRC

	inline	float	Duration( void ) { return pev->dmg_take; }
	inline	float	HoldTime( void ) { return pev->dmg_save; }

	inline	void	SetDuration( float duration ) { pev->dmg_take = duration; }
	inline	void	SetHoldTime( float hold ) { pev->dmg_save = hold; }

	STATE   m_iState; // LRC. Don't saverestore this value, it's not worth it.
private:
};

LINK_ENTITY_TO_CLASS( env_fade, CFade );

// pev->dmg_take is duration
// pev->dmg_save is hold duration
#define SF_FADE_IN				0x0001		// Fade in, not out
#define SF_FADE_MODULATE		0x0002		// Modulate, don't blend
#define SF_FADE_ONLYONE			0x0004
#define SF_FADE_PERMANENT		0x0008		//LRC - hold permanently

void CFade::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	m_iState = STATE_OFF; //LRC
}


void CFade::KeyValue( KeyValueData *pkvd )
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
	else
		CPointEntity::KeyValue( pkvd );
}


void CFade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int fadeFlags = 0;
	
	m_iState = STATE_TURN_ON; //LRC
	SetNextThink( Duration() ); //LRC

	if ( !(pev->spawnflags & SF_FADE_IN) )
		fadeFlags |= FFADE_OUT;

	if ( pev->spawnflags & SF_FADE_MODULATE )
		fadeFlags |= FFADE_MODULATE;

	if ( pev->spawnflags & SF_FADE_PERMANENT )	//LRC
		fadeFlags |= FFADE_STAYOUT;				//LRC

	if ( pev->spawnflags & SF_FADE_ONLYONE )
	{
		if ( pActivator->IsNetClient() )
		{
			#ifdef XENWARRIOR
			if (pActivator->IsPlayer() && ((CBasePlayer*)pActivator)->FlashlightIsOn())
			{
				((CBasePlayer*)pActivator)->FlashlightTurnOff();
				if (pev->spawnflags & SF_FADE_PERMANENT)
					g_fEnvFadeTime = gpGlobals->time + 1E6;
				else
					g_fEnvFadeTime = gpGlobals->time + Duration() + HoldTime();
			}
			#endif

			UTIL_ScreenFade( pActivator, pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags );
		}
	}
	else
	{
		#ifdef XENWARRIOR
		CBasePlayer *pPlayer = (CBasePlayer*)UTIL_FindEntityByTargetname(NULL, "*player");
		if (pPlayer)
			((CBasePlayer*)pPlayer)->FlashlightTurnOff();
		if (pev->spawnflags & SF_FADE_PERMANENT)
			g_fEnvFadeTime = gpGlobals->time + 1E6;
		else
			g_fEnvFadeTime = gpGlobals->time + Duration() + HoldTime();
		#endif

		UTIL_ScreenFadeAll( pev->rendercolor, Duration(), HoldTime(), pev->renderamt, fadeFlags );
	}
	SUB_UseTargets( this, USE_TOGGLE, 0 );
}

//LRC: a bolt-on state!
void CFade::Think( void )
{
	if (m_iState == STATE_TURN_ON)
	{
		m_iState = STATE_ON;
		if (!( pev->spawnflags & SF_FADE_PERMANENT))
			SetNextThink( HoldTime() );
	}
	else
		m_iState = STATE_OFF;
}


class CMessage : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
private:
};

LINK_ENTITY_TO_CLASS( env_message, CMessage );


void CMessage::Spawn( void )
{
	Precache();

	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;

	switch( pev->impulse )
	{
	case 1: // Medium radius
		pev->speed = ATTN_STATIC;
		break;
	
	case 2:	// Large radius
		pev->speed = ATTN_NORM;
		break;

	case 3:	//EVERYWHERE
		pev->speed = ATTN_NONE;
		break;
	
	default:
	case 0: // Small radius
		pev->speed = ATTN_IDLE;
		break;
	}
	pev->impulse = 0;

	// No volume, use normal
	if ( pev->scale <= 0 )
		pev->scale = 1.0;
}


void CMessage::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CMessage::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "messagesound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messagevolume"))
	{
		pev->scale = atof(pkvd->szValue) * 0.1;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "messageattenuation"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}


void CMessage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pPlayer = NULL;

	if ( pev->spawnflags & SF_MESSAGE_ALL )
		UTIL_ShowMessageAll( STRING(pev->message) );
	else
	{
		if ( pActivator && pActivator->IsPlayer() )
			pPlayer = pActivator;
		else
		{
			pPlayer = CBaseEntity::Instance( g_engfuncs.pfnPEntityOfEntIndex( 1 ) );
		}
		if ( pPlayer )
			UTIL_ShowMessage( STRING(pev->message), pPlayer );
	}
	if ( pev->noise )
	{
		EMIT_SOUND( edict(), CHAN_BODY, STRING(pev->noise), pev->scale, pev->speed );
	}
	if ( pev->spawnflags & SF_MESSAGE_ONCE )
		UTIL_Remove( this );

	SUB_UseTargets( this, USE_TOGGLE, 0 );
}



//=========================================================
// FunnelEffect
//=========================================================
class CEnvFunnel : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iSprite;	// Don't save, precache
};

void CEnvFunnel :: Precache ( void )
{
	//LRC
	if (pev->netname)
		m_iSprite = PRECACHE_MODEL ( (char*)STRING(pev->netname) );
	else
		m_iSprite = PRECACHE_MODEL ( "sprites/flare6.spr" );
}

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

void CEnvFunnel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//LRC
	Vector vecPos;
	if (pev->message)
		vecPos = CalcLocus_Position( this, pActivator, STRING(pev->message) );
	else
		vecPos = pev->origin;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_LARGEFUNNEL );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_SHORT( m_iSprite );

		if ( pev->spawnflags & SF_FUNNEL_REVERSE )// funnel flows in reverse?
		{
			WRITE_SHORT( 1 );
		}
		else
		{
			WRITE_SHORT( 0 );
		}


	MESSAGE_END();

	if (!(pev->spawnflags & SF_FUNNEL_REPEATABLE))
	{
		SetThink(&CEnvFunnel:: SUB_Remove );
		SetNextThink( 0 );
	}
}

void CEnvFunnel::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
}


//=========================================================
// LRC -  All the particle effects from Quake 1
//=========================================================
#define SF_QUAKEFX_REPEATABLE 1
class CEnvQuakeFx : public CPointEntity
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_quakefx, CEnvQuakeFx );

void CEnvQuakeFx::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos;
	if (pev->message)
		vecPos = CalcLocus_Position( this, pActivator, STRING(pev->message) );
	else
		vecPos = pev->origin;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( pev->impulse );
		WRITE_COORD( vecPos.x );
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		if (pev->impulse == TE_PARTICLEBURST)
		{
			WRITE_SHORT( pev->armortype );  // radius
			WRITE_BYTE( pev->frags );		// particle colour
			WRITE_BYTE( pev->health * 10 ); // duration
		}
		else if (pev->impulse == TE_EXPLOSION2)
		{
			// these fields seem to have no effect - except that it
			// crashes when I send "0" for the number of colours..
			WRITE_BYTE( 0 ); // colour
			WRITE_BYTE( 1 ); // number of colours
		}
	MESSAGE_END();

	if (!(pev->spawnflags & SF_QUAKEFX_REPEATABLE))
	{
		SetThink(&CEnvQuakeFx:: SUB_Remove );
		SetNextThink( 0 );
	}
}


//=========================================================
// LRC - Beam Trail effect
//=========================================================
#define SF_BEAMTRAIL_OFF 1
class CEnvBeamTrail : public CPointEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	STATE	GetState( void );
	void	EXPORT StartTrailThink ( void );
	void	Affect( CBaseEntity *pTarget, USE_TYPE useType );

	int		m_iSprite;	// Don't save, precache
};

void CEnvBeamTrail :: Precache ( void )
{
	if (pev->target)
		PRECACHE_MODEL("sprites/null.spr");
	if (pev->netname)
		m_iSprite = PRECACHE_MODEL ( (char*)STRING(pev->netname) );
}

LINK_ENTITY_TO_CLASS( env_beamtrail, CEnvBeamTrail );

STATE CEnvBeamTrail :: GetState ( void )
{
	if (pev->spawnflags & SF_BEAMTRAIL_OFF)
		return STATE_OFF;
	else
		return STATE_ON;
}

void CEnvBeamTrail :: StartTrailThink ( void )
{
	pev->spawnflags |= SF_BEAMTRAIL_OFF; // fake turning off, so the Use turns it on properly
	Use(this, this, USE_ON, 0);
}

void CEnvBeamTrail::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (pev->target)
	{
		CBaseEntity *pTarget = UTIL_FindEntityByTargetname(NULL, STRING(pev->target), pActivator );
		while (pTarget)
		{
			Affect(pTarget, useType);
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->target), pActivator );
		}
	}
	else
	{
		if (!ShouldToggle( useType ))
			return;
		Affect(this, useType);
	}

	if (useType == USE_ON)
		pev->spawnflags &= ~SF_BEAMTRAIL_OFF;
	else if (useType == USE_OFF)
		pev->spawnflags |= SF_BEAMTRAIL_OFF;
	else if (useType == USE_TOGGLE)
	{
		if (pev->spawnflags & SF_BEAMTRAIL_OFF)
			pev->spawnflags &= ~SF_BEAMTRAIL_OFF;
		else
			pev->spawnflags |= SF_BEAMTRAIL_OFF;
	}
}

void CEnvBeamTrail::Affect( CBaseEntity *pTarget, USE_TYPE useType )
{
	if (useType == USE_ON || pev->spawnflags & SF_BEAMTRAIL_OFF)
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT(pTarget->entindex());	// entity
			WRITE_SHORT( m_iSprite );	// model
			WRITE_BYTE( pev->health*10 ); // life
			WRITE_BYTE( pev->armorvalue );  // width
			WRITE_BYTE( pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE(TE_KILLBEAM);
			WRITE_SHORT(pTarget->entindex());
		MESSAGE_END();
	}
}

void CEnvBeamTrail::Spawn( void )
{
	Precache();

	SET_MODEL(ENT(pev), "sprites/null.spr");
	UTIL_SetSize(pev, Vector(0, 0, 0), Vector(0, 0, 0));

	if (!(pev->spawnflags & SF_BEAMTRAIL_OFF))
	{
		SetThink(&CEnvBeamTrail::StartTrailThink);
		UTIL_DesiredThink( this );
	}
}


//=========================================================
// LRC -  custom footstep sounds
//=========================================================
#define SF_FOOTSTEPS_SET 1
#define SF_FOOTSTEPS_ONCE 2

class CEnvFootsteps : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	STATE	GetState( void );
	STATE	GetState( CBaseEntity* pEnt );
	void	PrecacheNoise ( const char* szNoise );
};

LINK_ENTITY_TO_CLASS( env_footsteps, CEnvFootsteps );

void CEnvFootsteps::Spawn( void )
{
	Precache();
}

void CEnvFootsteps :: PrecacheNoise ( const char* szNoise )
{
	static char szBuf[128];
	int i = 0, j = 0;
	for (i = 0; szNoise[i]; i++)
	{
		if (szNoise[i] == '?')
		{
			strcpy(szBuf, szNoise);
			for (j = 0; j < 4; j++)
			{
				szBuf[i] = j+'1';
				PRECACHE_SOUND ( szBuf );
			}
		}
	}
	if (!j)
		PRECACHE_SOUND ( (char*)szNoise );
}

void CEnvFootsteps :: Precache ( void )
{
	if (pev->noise)
		PrecacheNoise(STRING(pev->noise));
	if (pev->noise1)
		PrecacheNoise(STRING(pev->noise1));
	if (pev->noise2)
		PrecacheNoise(STRING(pev->noise2));
	if (pev->noise3)
		PrecacheNoise(STRING(pev->noise3));
}

STATE CEnvFootsteps::GetState()
{
	if (pev->spawnflags & SF_FOOTSTEPS_SET) return STATE_OFF;
	return pev->impulse ? STATE_ON : STATE_OFF;
}

STATE CEnvFootsteps::GetState(CBaseEntity* pEnt)
{
	if (pev->spawnflags & SF_FOOTSTEPS_SET)
		return STATE_OFF;
	if (pEnt->IsPlayer())
	{
		// based on trigger_hurt code
		int playerMask = 1 << (pEnt->entindex() - 1);
		
		if ( pev->impulse & playerMask )
			return STATE_ON;
		else
			return STATE_OFF;
	}
	else return GetState();
}

void CEnvFootsteps::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
//	union floatToString ftsTemp;

	//CONSIDER: add an "all players" spawnflag, like game_text?
	if (pActivator && pActivator->IsPlayer())
	{
		int playerMask = 1 << (pActivator->entindex() - 1);
		
		if (pev->spawnflags & SF_FOOTSTEPS_SET || ( !(pev->impulse & playerMask) && (useType == USE_ON || useType == USE_TOGGLE)))
		{
			pev->impulse |= playerMask;
			if (pev->frags)
			{
				char sTemp[4];
				sprintf(sTemp, "%d", (int)pev->frags);
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "stype", sTemp );
				//pActivator->pev->iFootstepType = pev->frags;
			}
			else if (pev->noise)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "ssnd", STRING(pev->noise) );
			}
			if (pev->noise1)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "lsnd", STRING(pev->noise1) );
			}
			if (pev->noise2)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "wsnd", STRING(pev->noise2) );
			}
			if (pev->noise3)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "psnd", STRING(pev->noise3) );
			}
			// workaround for physinfo string bug: force the engine to null-terminate it
			g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "x", "0" );
			//ALERT(at_console, "ON, InfoString = %s\n", g_engfuncs.pfnGetPhysicsInfoString(pActivator->edict()));
			if (pev->spawnflags & SF_FOOTSTEPS_SET && pev->spawnflags & SF_FOOTSTEPS_ONCE)
			{
				UTIL_Remove(this);
			}
		}
		else if ( (pev->impulse & playerMask) && (useType == USE_OFF || useType == USE_TOGGLE))
		{
			pev->impulse &= ~playerMask;
			if (pev->frags)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "stype", "0" );
			}
			else if (pev->noise)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "ssnd", "0" );
			}
			if (pev->noise1)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "lsnd", "0" );
			}
			if (pev->noise2)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "wsnd", "0" );
			}
			if (pev->noise3)
			{
				g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "psnd", "0" );
			}
			// workaround for physinfo string bug: force the engine to null-terminate it
			g_engfuncs.pfnSetPhysicsKeyValue( pActivator->edict(), "x", "0" );
			//ALERT(at_console, "OFF, InfoString = %s\n", g_engfuncs.pfnGetPhysicsInfoString(pActivator->edict()));
			if (pev->spawnflags & SF_FOOTSTEPS_ONCE)
			{
				UTIL_Remove(this);
			}
		}
		else
		{
			//ALERT(at_console, "NO EFFECT\n");
		}
	}
}

//=========================================================
//LRC- the long-awaited effect. (Rain, in the desert? :)
//
//FIXME: give designers a _lot_ more control.
//=========================================================
#define MAX_RAIN_BEAMS 32

#define AXIS_X 1
#define AXIS_Y 2
#define AXIS_Z 0

#define EXTENT_OBSTRUCTED 1
#define EXTENT_ARCING 2
#define EXTENT_OBSTRUCTED_REVERSE 3
#define EXTENT_ARCING_REVERSE 4

class CEnvRain : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	STATE	m_iState;
	int		m_spriteTexture;
	int		m_iszSpriteName; // have to saverestore this, the beams keep a link to it
	int		m_dripSize;
	int		m_minDripSpeed;
	int		m_maxDripSpeed;
	int		m_burstSize;
	int		m_brightness;
	int		m_pitch; // don't saverestore this
	float	m_flUpdateTime;
	float	m_flMaxUpdateTime;
//	CBeam*	m_pBeams[MAX_RAIN_BEAMS];
	int m_axis;
	int m_iExtent;
	float m_fLifeTime;
	int m_iNoise;

	virtual STATE GetState( void ) { return m_iState; };
};

LINK_ENTITY_TO_CLASS( env_rain, CEnvRain );

TYPEDESCRIPTION	CEnvRain::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvRain, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_spriteTexture, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_dripSize, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_minDripSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_maxDripSpeed, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_burstSize, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_brightness, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_flUpdateTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvRain, m_flMaxUpdateTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvRain, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CEnvRain, m_axis, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_iExtent, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvRain, m_fLifeTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvRain, m_iNoise, FIELD_INTEGER ),
//	DEFINE_FIELD( CEnvRain, m_pBeams, FIELD_CLASSPTR, MAX_RAIN_BEAMS ),
};

IMPLEMENT_SAVERESTORE( CEnvRain, CBaseEntity );

void CEnvRain::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING(m_iszSpriteName) );
}

void CEnvRain::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_dripSize"))
	{
		m_dripSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_burstSize"))
	{
		m_burstSize = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_dripSpeed"))
	{
		int temp = atoi(pkvd->szValue);
		m_maxDripSpeed = temp + (temp/4);
		m_minDripSpeed = temp - (temp/4);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_brightness"))
	{
		m_brightness = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flUpdateTime"))
	{
		m_flUpdateTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flMaxUpdateTime"))
	{
		m_flMaxUpdateTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		m_pitch = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_axis"))
	{
		m_axis = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iExtent"))
	{
		m_iExtent = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fLifeTime"))
	{
		m_fLifeTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iNoise"))
	{
		m_iNoise = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvRain::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (!ShouldToggle(useType)) return;

	if (m_iState == STATE_ON)
	{
		m_iState = STATE_OFF;
		DontThink();
	}
	else
	{
		m_iState = STATE_ON;
		SetNextThink( 0.1 );
	}
}

#define SF_RAIN_START_OFF	1

void CEnvRain::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), STRING(pev->model) );		// Set size
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	if (pev->rendercolor == g_vecZero)
		pev->rendercolor = Vector(255,255,255);

	if (m_pitch)
		pev->angles.x = m_pitch;
	else if (pev->angles.x == 0) // don't allow horizontal rain.
		pev->angles.x = 90;

	if (m_burstSize == 0) // in case the level designer forgot to set it.
		m_burstSize = 2;

	if (pev->spawnflags & SF_RAIN_START_OFF)
		m_iState = STATE_OFF;
	else
	{
		m_iState = STATE_ON;
		SetNextThink( 0.1 );
	}
}

void CEnvRain::Think( void )
{
//	ALERT(at_console,"RainThink %d %d %d %s\n",m_spriteTexture,m_dripSize,m_brightness,STRING(m_iszSpriteName));
	Vector vecSrc;
	Vector vecDest;

	UTIL_MakeVectors(pev->angles);
	Vector vecOffs = gpGlobals->v_forward;
	switch (m_axis)
	{
	case AXIS_X:
		vecOffs = vecOffs * (pev->size.x / vecOffs.x);
		break;
	case AXIS_Y:
		vecOffs = vecOffs * (pev->size.y / vecOffs.y);
		break;
	case AXIS_Z:
		vecOffs = vecOffs * (pev->size.z / vecOffs.z);
		break;
	}

//	ALERT(at_console,"RainThink offs.z = %f, size.z = %f\n",vecOffs.z,pev->size.z);

	int repeats;
	if (!m_fLifeTime && !m_flUpdateTime && !m_flMaxUpdateTime)
		repeats = m_burstSize * 3;
	else
		repeats = m_burstSize;

	int drawn = 0;
	int tries = 0;
	TraceResult tr;
	BOOL bDraw;

	while (drawn < repeats && tries < (repeats*3))
	{
		tries++;
		if (m_axis == AXIS_X)
			vecSrc.x = pev->maxs.x;
		else
			vecSrc.x = pev->mins.x + RANDOM_LONG(0, pev->size.x);
		if (m_axis == AXIS_Y)
			vecSrc.y = pev->maxs.y;
		else
			vecSrc.y = pev->mins.y + RANDOM_LONG(0, pev->size.y);
		if (m_axis == AXIS_Z)
			vecSrc.z = pev->maxs.z;
		else
			vecSrc.z = pev->mins.z + RANDOM_LONG(0, pev->size.z);
		vecDest = vecSrc - vecOffs;
		bDraw = TRUE;

		switch (m_iExtent)
		{
		case EXTENT_OBSTRUCTED:
			UTIL_TraceLine( vecSrc, vecDest, ignore_monsters, NULL, &tr);
			vecDest = tr.vecEndPos;
			break;
		case EXTENT_OBSTRUCTED_REVERSE:
			UTIL_TraceLine( vecDest, vecSrc, ignore_monsters, NULL, &tr);
			vecSrc = tr.vecEndPos;
			break;
		case EXTENT_ARCING:
			UTIL_TraceLine( vecSrc, vecDest, ignore_monsters, NULL, &tr);
			if (tr.flFraction == 1.0) bDraw = FALSE;
			vecDest = tr.vecEndPos;
			break;
		case EXTENT_ARCING_REVERSE:
			UTIL_TraceLine( vecDest, vecSrc, ignore_monsters, NULL, &tr);
			if (tr.flFraction == 1.0) bDraw = FALSE;
			vecSrc = tr.vecEndPos;
			break;
		}
//		vecDest.z = pev->mins.z;
		if (!bDraw) continue;

		drawn++;

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMPOINTS );
			WRITE_COORD(vecDest.x);
			WRITE_COORD(vecDest.y);
			WRITE_COORD(vecDest.z);
			WRITE_COORD(vecSrc.x);
			WRITE_COORD(vecSrc.y);
			WRITE_COORD(vecSrc.z);
			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( (int)0 ); // framestart
			WRITE_BYTE( (int)0 ); // framerate
			if (m_fLifeTime) // life
				WRITE_BYTE( (int)(m_fLifeTime*10) );
			else if (m_flMaxUpdateTime)
				WRITE_BYTE( (int)( RANDOM_FLOAT(m_flUpdateTime, m_flMaxUpdateTime)*30 ));
			else
				WRITE_BYTE( (int)(m_flUpdateTime * 30) ); // life
			WRITE_BYTE( m_dripSize );  // width
			WRITE_BYTE( m_iNoise );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r,
			WRITE_BYTE( (int)pev->rendercolor.y );   //    g,
			WRITE_BYTE( (int)pev->rendercolor.z );   //       b
			WRITE_BYTE( m_brightness );	// brightness
			WRITE_BYTE( (int)RANDOM_LONG(m_minDripSpeed,m_maxDripSpeed) );		// speed
		MESSAGE_END();
	}

	// drawn will be false if we didn't draw anything.
	if (pev->target && drawn)
		FireTargets(STRING(pev->target), this, this, USE_TOGGLE, 0);

	if (m_flMaxUpdateTime)
		SetNextThink( RANDOM_FLOAT(m_flMaxUpdateTime, m_flUpdateTime) );
	else if (m_flUpdateTime)
		SetNextThink( m_flUpdateTime );
}

//==================================================================
//LRC- Xen monsters' warp-in effect, for those too lazy to build it. :)
//==================================================================
class CEnvWarpBall : public CBaseEntity
{
public:
	void	Precache( void );
	void	Spawn( void ) { Precache(); }
	void	Think( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

LINK_ENTITY_TO_CLASS( env_warpball, CEnvWarpBall );

void CEnvWarpBall::Precache( void )
{
	PRECACHE_MODEL( "sprites/lgtning.spr" );
	PRECACHE_MODEL( "sprites/Fexplo1.spr" );
	PRECACHE_MODEL( "sprites/XFlare1.spr" );
	PRECACHE_SOUND( "debris/beamstart2.wav" );
	PRECACHE_SOUND( "debris/beamstart7.wav" );
}

void CEnvWarpBall::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTimes = 0;
	int iDrawn = 0;
	TraceResult tr;
	Vector vecDest;
	CBeam *pBeam;
	while (iDrawn<pev->frags && iTimes<(pev->frags * 3)) // try to draw <frags> beams, but give up after 3x<frags> tries.
	{
		vecDest = pev->health * (Vector(RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1), RANDOM_FLOAT(-1,1)).Normalize());
		UTIL_TraceLine( pev->origin, pev->origin + vecDest, ignore_monsters, NULL, &tr);
		if (tr.flFraction != 1.0)
		{
			// we hit something.
			iDrawn++;
			pBeam = CBeam::BeamCreate("sprites/lgtning.spr",200);
			pBeam->PointsInit( pev->origin, tr.vecEndPos );
			pBeam->SetColor( 197, 243, 169 );
			pBeam->SetNoise( 65 );
			pBeam->SetBrightness( 150 );
			pBeam->SetWidth( 18 );
			pBeam->SetScrollRate( 35 );
			pBeam->SetThink(&CBeam:: SUB_Remove );
			pBeam->SetNextThink( 1 );
		}
		iTimes++;
	}
	EMIT_SOUND( edict(), CHAN_BODY, "debris/beamstart2.wav", 1, ATTN_NORM );

	CSprite *pSpr = CSprite::SpriteCreate( "sprites/Fexplo1.spr", pev->origin, TRUE );
	pSpr->AnimateAndDie( 10 );
	pSpr->SetTransparency(kRenderGlow,  77, 210, 130,  255, kRenderFxNoDissipation);

	pSpr = CSprite::SpriteCreate( "sprites/XFlare1.spr", pev->origin, TRUE );
	pSpr->AnimateAndDie( 10 );
	pSpr->SetTransparency(kRenderGlow,  184, 250, 214,  255, kRenderFxNoDissipation);

	SetNextThink( 0.5 );
}

void CEnvWarpBall::Think( void )
{
	EMIT_SOUND( edict(), CHAN_ITEM, "debris/beamstart7.wav", 1, ATTN_NORM );
	SUB_UseTargets( this, USE_TOGGLE, 0);
}

//==================================================================
//LRC- Shockwave effect, like when a Houndeye attacks.
//==================================================================
#define SF_SHOCKWAVE_CENTERED 1
#define SF_SHOCKWAVE_REPEATABLE 2

class CEnvShockwave : public CPointEntity
{
public:
	void	Precache( void );
	void	Spawn( void ) { Precache(); }
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void DoEffect( Vector vecPos );

	int m_iTime;
	int m_iRadius;
	int	m_iHeight;
	int m_iScrollRate;
	int m_iNoise;
	int m_iFrameRate;
	int m_iStartFrame;
	int m_iSpriteTexture;
	char m_cType;
	int m_iszPosition;
};

LINK_ENTITY_TO_CLASS( env_shockwave, CEnvShockwave );

TYPEDESCRIPTION	CEnvShockwave::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvShockwave, m_iHeight, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iTime, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iRadius, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iScrollRate, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iNoise, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iFrameRate, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iStartFrame, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_iSpriteTexture, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvShockwave, m_cType, FIELD_CHARACTER ),
	DEFINE_FIELD( CEnvShockwave, m_iszPosition, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CEnvShockwave, CBaseEntity );

void CEnvShockwave::Precache( void )
{
	m_iSpriteTexture = PRECACHE_MODEL( (char *)STRING(pev->netname) );
}

void CEnvShockwave::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iTime"))
	{
		m_iTime = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iRadius"))
	{
		m_iRadius = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iHeight"))
	{
		m_iHeight = atoi(pkvd->szValue)/2; //LRC- the actual height is doubled when drawn
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iScrollRate"))
	{
		m_iScrollRate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iNoise"))
	{
		m_iNoise = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFrameRate"))
	{
		m_iFrameRate = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iStartFrame"))
	{
		m_iStartFrame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszPosition"))
	{
		m_iszPosition = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_cType"))
	{
		m_cType = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CEnvShockwave::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	Vector vecPos;
	if (m_iszPosition)
		vecPos = CalcLocus_Position( this, pActivator, STRING(m_iszPosition) );
	else
		vecPos = pev->origin;

	if (!(pev->spawnflags & SF_SHOCKWAVE_CENTERED))
		vecPos.z += m_iHeight;

	// blast circle
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		if (m_cType)
			WRITE_BYTE( m_cType );
		else
			WRITE_BYTE( TE_BEAMCYLINDER );
		WRITE_COORD( vecPos.x );// coord coord coord (center position)
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z );
		WRITE_COORD( vecPos.x );// coord coord coord (axis and radius)
		WRITE_COORD( vecPos.y );
		WRITE_COORD( vecPos.z + m_iRadius );
		WRITE_SHORT( m_iSpriteTexture ); // short (sprite index)
		WRITE_BYTE( m_iStartFrame ); // byte (starting frame)
		WRITE_BYTE( m_iFrameRate ); // byte (frame rate in 0.1's)
		WRITE_BYTE( m_iTime ); // byte (life in 0.1's)
		WRITE_BYTE( m_iHeight );  // byte (line width in 0.1's)
		WRITE_BYTE( m_iNoise );   // byte (noise amplitude in 0.01's)
		WRITE_BYTE( pev->rendercolor.x );   // byte,byte,byte (color)
		WRITE_BYTE( pev->rendercolor.y );
		WRITE_BYTE( pev->rendercolor.z );
		WRITE_BYTE( pev->renderamt );  // byte (brightness)
		WRITE_BYTE( m_iScrollRate );	// byte (scroll speed in 0.1's)
	MESSAGE_END();

	if (!(pev->spawnflags & SF_SHOCKWAVE_REPEATABLE))
	{
		SetThink(&CEnvShockwave:: SUB_Remove );
		SetNextThink( 0 );
	}
}

//==================================================================
//LRC- env_dlight; Dynamic Entity Light creator
//==================================================================
#define SF_DLIGHT_ONLYONCE 1
#define SF_DLIGHT_STARTON  2
class CEnvDLight : public CPointEntity
{
public:
	void	PostSpawn( void );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );
	void	DesiredAction( void );
	virtual void	MakeLight( int iTime );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	STATE	GetState( void )
	{
		if (pev->spawnflags & SF_DLIGHT_STARTON)
			return STATE_ON;
		else
			return STATE_OFF;
	}

	Vector m_vecPos;
	int			m_iKey;
	static int	ms_iNextFreeKey;
};

LINK_ENTITY_TO_CLASS( env_dlight, CEnvDLight );

TYPEDESCRIPTION	CEnvDLight::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvDLight, m_vecPos, FIELD_VECTOR ),
	DEFINE_FIELD( CEnvDLight, m_iKey, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CEnvDLight, CPointEntity );

int CEnvDLight::ms_iNextFreeKey = 1;

void CEnvDLight::PostSpawn( void )
{
	// each env_dlight uses its own key to reference the light on the client
	m_iKey = ms_iNextFreeKey;
	ms_iNextFreeKey++;

	if (FStringNull(pev->targetname) || pev->spawnflags & SF_DLIGHT_STARTON)
	{
		UTIL_DesiredAction(this);
	}
}

void CEnvDLight::DesiredAction( void )
{
	pev->spawnflags &= ~SF_DLIGHT_STARTON;
	Use(this, this, USE_ON, 0);
}

void CEnvDLight::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (!ShouldToggle(useType))
	{
		return;
	}
	if (GetState() == STATE_ON)
	{
		// turn off
		MakeLight(false);
		pev->spawnflags &= ~SF_DLIGHT_STARTON;
		DontThink();
		return;
	}

	if (pev->message)
	{
		m_vecPos = CalcLocus_Position( this, pActivator, STRING(pev->message) );
	}
	else
	{
		m_vecPos = pev->origin;
	}

	// turn on
	MakeLight(true);
	pev->spawnflags |= SF_DLIGHT_STARTON;

	if (pev->health)
	{
		SetNextThink(pev->health);
	}
	else
	{
		if (pev->spawnflags & SF_DLIGHT_ONLYONCE)
		{
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( 0 );
		}
	}
}

extern int gmsgKeyedDLight;

void CEnvDLight::MakeLight( BOOL bActive)
{
	MESSAGE_BEGIN( MSG_ALL, gmsgKeyedDLight, NULL );
		WRITE_BYTE( m_iKey );
		WRITE_BYTE( bActive );			// visible?
		if (bActive)
		{
			WRITE_COORD( m_vecPos.x );		// X
			WRITE_COORD( m_vecPos.y );		// Y
			WRITE_COORD( m_vecPos.z );		// Z
			WRITE_BYTE( pev->renderamt );		// radius * 0.1
			WRITE_BYTE( pev->rendercolor.x );	// r
			WRITE_BYTE( pev->rendercolor.y );	// g
			WRITE_BYTE( pev->rendercolor.z );	// b
		}
	MESSAGE_END();
}

void CEnvDLight::Think( void )
{
	// turn off the light
	MakeLight( false );
	pev->spawnflags &= ~SF_DLIGHT_STARTON;

	if (pev->spawnflags & SF_DLIGHT_ONLYONCE)
	{
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( 0 );
	}
}

//==================================================================
//LRC- env_elight; Dynamic Entity Light creator
//==================================================================
class CEnvELight : public CEnvDLight
{
public:
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	MakeLight(int iTime);
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	EHANDLE m_hAttach;
};

LINK_ENTITY_TO_CLASS( env_elight, CEnvELight );

TYPEDESCRIPTION	CEnvELight::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvELight, m_hAttach, FIELD_EHANDLE ),
};

IMPLEMENT_SAVERESTORE( CEnvELight, CEnvDLight );

void CEnvELight::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (pev->target)
	{
		m_hAttach = UTIL_FindEntityByTargetname( NULL, STRING(pev->target), pActivator);
		if (m_hAttach == NULL)
		{
			ALERT(at_console, "env_elight \"%s\" can't find target %s\n", STRING(pev->targetname), STRING(pev->target));
			return; // error?
		}
	}
	else
	{
		m_hAttach = this;
	}

	CEnvDLight::Use(pActivator, pCaller, useType, value);
}

void CEnvELight::MakeLight(int iTime)
{
	if (m_hAttach == NULL)
	{
		DontThink();
		pev->takedamage = 0;
		return;
	}

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, pev->origin );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( m_hAttach->entindex( ) + 0x1000 * pev->impulse );		// entity, attachment
		WRITE_COORD( m_vecPos.x );		// X
		WRITE_COORD( m_vecPos.y );		// Y
		WRITE_COORD( m_vecPos.z );		// Z
		WRITE_COORD( pev->renderamt );		// radius * 0.1
		WRITE_BYTE( pev->rendercolor.x );	// r
		WRITE_BYTE( pev->rendercolor.y );	// g
		WRITE_BYTE( pev->rendercolor.z );	// b
		WRITE_BYTE( iTime );				// time * 10
		WRITE_COORD( pev->frags );			// decay * 0.1
	MESSAGE_END( );
}


//=========================================================
// LRC - Decal effect
//=========================================================
class CEnvDecal : public CPointEntity
{
public:
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( env_decal, CEnvDecal );

void CEnvDecal::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int iTexture = 0;

	switch(pev->impulse)
	{
		case 1: iTexture = DECAL_GUNSHOT1	+	RANDOM_LONG(0,4); break;
		case 2: iTexture = DECAL_BLOOD1		+	RANDOM_LONG(0,5); break;
		case 3: iTexture = DECAL_YBLOOD1	+	RANDOM_LONG(0,5); break;
		case 4: iTexture = DECAL_GLASSBREAK1+	RANDOM_LONG(0,2); break;
		case 5: iTexture = DECAL_BIGSHOT1	+	RANDOM_LONG(0,4); break;
		case 6: iTexture = DECAL_SCORCH1	+	RANDOM_LONG(0,1); break;
		case 7: iTexture = DECAL_SPIT1		+	RANDOM_LONG(0,1); break;
	}

	if (pev->impulse)
		iTexture = gDecals[ iTexture ].index;
	else
		iTexture = pev->skin; // custom texture

	Vector vecPos;
	if (!FStringNull(pev->target))
		vecPos = CalcLocus_Position( this, pActivator, STRING(pev->target) );
	else
		vecPos = pev->origin;

	Vector vecOffs;
	if (!FStringNull(pev->netname))
		vecOffs = CalcLocus_Velocity( this, pActivator, STRING(pev->netname) );
	else
	{
		UTIL_MakeVectors(pev->angles);
		vecOffs = gpGlobals->v_forward;
	}

	if (pev->message)
		vecOffs = vecOffs * CalcLocus_Ratio( pActivator, STRING(pev->message) );
	else
		vecOffs = vecOffs.Normalize() * 4000;


	TraceResult trace;
	int			entityIndex;

	UTIL_TraceLine( vecPos, vecPos+vecOffs, ignore_monsters, NULL, &trace );

	if (trace.flFraction == 1.0)
		return; // didn't hit anything, oh well
	
	entityIndex = (short)ENTINDEX(trace.pHit);

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY);
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( trace.vecEndPos.x );
		WRITE_COORD( trace.vecEndPos.y );
		WRITE_COORD( trace.vecEndPos.z );
		WRITE_SHORT( iTexture );
		WRITE_SHORT( entityIndex );
		if ( entityIndex )
			WRITE_SHORT( (int)VARS(trace.pHit)->modelindex );
	MESSAGE_END();
}

void CEnvDecal::Spawn( void )
{
	if (pev->impulse == 0)
	{
		pev->skin = DECAL_INDEX( STRING(pev->noise) );

		if ( pev->skin == 0 )
			ALERT( at_debug, "locus_decal \"%s\" can't find decal \"%s\"\n", STRING(pev->noise) );
	}
}


//=========================================================
// Beverage Dispenser
// overloaded pev->frags, is now a flag for whether or not a can is stuck in the dispenser. 
// overloaded pev->health, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseDelay
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	// it's 'on' while there are cans left
	virtual STATE GetState( void ) { return (pev->health > 0)?STATE_ON:STATE_OFF; };
};

void CEnvBeverage :: Precache ( void )
{
	PRECACHE_MODEL( "models/can.mdl" );
	PRECACHE_SOUND( "weapons/g_bounce3.wav" );
}

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );

void CEnvBeverage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->frags != 0 || pev->health <= 0 )
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	Vector vecPos;
	if (pev->target)
		vecPos = CalcLocus_Position( this, pActivator, STRING(pev->target) );
	else
		vecPos = pev->origin;

	CBaseEntity *pCan = CBaseEntity::Create( "item_sodacan", vecPos, pev->angles, edict() );

	if ( pev->skin == 6 )
	{
		// random
		pCan->pev->skin = RANDOM_LONG( 0, 5 );
	}
	else
	{
		pCan->pev->skin = pev->skin;
	}

	pev->frags = 1;
	pev->health--;

	//SetThink (SUB_Remove);
	//pev->nextthink = gpGlobals->time;
}

void CEnvBeverage::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->frags = 0;

	if ( pev->health == 0 )
	{
		pev->health = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseEntity
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	EXPORT CanThink ( void );
	void	EXPORT CanTouch ( CBaseEntity *pOther );
};

void CItemSoda :: Precache ( void )
{
	// added for Nemo1024  --LRC
	PRECACHE_MODEL( "models/can.mdl" );
	PRECACHE_SOUND( "weapons/g_bounce3.wav" );
}

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;

	SET_MODEL ( ENT(pev), "models/can.mdl" );
	UTIL_SetSize ( pev, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );
	
	SetThink(&CItemSoda::CanThink);
	SetNextThink( 0.5 );
}

void CItemSoda::CanThink ( void )
{
	EMIT_SOUND (ENT(pev), CHAN_WEAPON, "weapons/g_bounce3.wav", 1, ATTN_NORM );

	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize ( pev, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );
	SetThink ( NULL );
	SetTouch(&CItemSoda:: CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( !FNullEnt( pev->owner ) )
	{
		// tell the machine the can was taken
		pev->owner->v.frags = 0;
	}

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = EF_NODRAW;
	SetTouch ( NULL );
	SetThink(&CItemSoda:: SUB_Remove );
	SetNextThink( 0 );
}

//=========================================================
// LRC - env_fog, extended a bit from the DMC version
//=========================================================
#define SF_FOG_ACTIVE 1
#define SF_FOG_FADING 0x8000

class CEnvFog : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void EXPORT ResumeThink( void );
	void EXPORT Resume2Think( void );
	void EXPORT TurnOn( void );
	void EXPORT TurnOff( void );
	void EXPORT FadeInDone( void );
	void EXPORT FadeOutDone( void );
	void SendData( Vector col, int fFadeTime, int StartDist, int iEndDist);
	void KeyValue( KeyValueData *pkvd );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	STATE GetState( void );

	int m_iStartDist;
	int m_iEndDist;
	float m_iFadeIn;
	float m_iFadeOut;
	float m_fHoldTime;
	float m_fFadeStart; // if we're fading in/out, then when did the fade start?
};

TYPEDESCRIPTION	CEnvFog::m_SaveData[] = 
{
	DEFINE_FIELD( CEnvFog, m_iStartDist, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFog, m_iEndDist, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFog, m_iFadeIn, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFog, m_iFadeOut, FIELD_INTEGER ),
	DEFINE_FIELD( CEnvFog, m_fHoldTime, FIELD_FLOAT ),
	DEFINE_FIELD( CEnvFog, m_fFadeStart, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( CEnvFog, CBaseEntity );

void CEnvFog :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "startdist"))
	{
		m_iStartDist = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "enddist"))
	{
		m_iEndDist = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadein"))
	{
		m_iFadeIn = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "fadeout"))
	{
		m_iFadeOut = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "holdtime"))
	{
		m_fHoldTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

STATE CEnvFog::GetState( void )
{
	if (pev->spawnflags & SF_FOG_ACTIVE)
	{
		if (pev->spawnflags & SF_FOG_FADING)
			return STATE_TURN_ON;
		else
			return STATE_ON;
	}
	else
	{
		if (pev->spawnflags & SF_FOG_FADING)
			return STATE_TURN_OFF;
		else
			return STATE_OFF;
	}
}

void CEnvFog :: Spawn ( void )
{
	pev->effects |= EF_NODRAW;

	if (pev->targetname == 0)
		pev->spawnflags |= SF_FOG_ACTIVE;

	if (pev->spawnflags & SF_FOG_ACTIVE)
	{
		SetThink(&CEnvFog :: TurnOn );
		UTIL_DesiredThink( this );
	}

// Precache is now used only to continue after a game has loaded.
//	Precache();

	// things get messed up if we try to draw fog with a startdist
	// or an enddist of 0, so we don't allow it.
	if (m_iStartDist == 0) m_iStartDist = 1;
	if (m_iEndDist == 0) m_iEndDist = 1;
}

void CEnvFog :: Precache ( void )
{
	if (pev->spawnflags & SF_FOG_ACTIVE)
	{
		SetThink(&CEnvFog :: ResumeThink );
		SetNextThink( 0.1 );
	}
}

extern int gmsgSetFog;

void CEnvFog :: TurnOn ( void )
{
//	ALERT(at_console, "Fog turnon %f\n", gpGlobals->time);

	pev->spawnflags |= SF_FOG_ACTIVE;

	if( m_iFadeIn )
	{
		pev->spawnflags |= SF_FOG_FADING;
		SendData( pev->rendercolor, m_iFadeIn, m_iStartDist, m_iEndDist);
		SetNextThink( m_iFadeIn );
		SetThink(&CEnvFog :: FadeInDone );
	}
	else
	{
		pev->spawnflags &= ~SF_FOG_FADING;
		SendData( pev->rendercolor, 0, m_iStartDist, m_iEndDist);
		if (m_fHoldTime)
		{
			SetNextThink( m_fHoldTime );
			SetThink(&CEnvFog :: TurnOff );
		}
	}
}

void CEnvFog :: TurnOff ( void )
{
//	ALERT(at_console, "Fog turnoff\n");

	pev->spawnflags &= ~SF_FOG_ACTIVE;

	if( m_iFadeOut )
	{
		pev->spawnflags |= SF_FOG_FADING;
		SendData( pev->rendercolor, -m_iFadeOut, m_iStartDist, m_iEndDist);
		SetNextThink( m_iFadeOut );
		SetThink(&CEnvFog :: FadeOutDone );
	}
	else
	{
		pev->spawnflags &= ~SF_FOG_FADING;
		SendData( g_vecZero, 0, 0, 0 );
		DontThink();
	}
}

//yes, this intermediate think function is necessary.
// the engine seems to ignore the nextthink time when starting up.
// So this function gets called immediately after the precache finishes,
// regardless of what nextthink time is specified.
void CEnvFog :: ResumeThink ( void )
{
//	ALERT(at_console, "Fog resume %f\n", gpGlobals->time);
	SetThink(&CEnvFog ::FadeInDone);
	SetNextThink(0.1);
}

void CEnvFog :: FadeInDone ( void )
{
	pev->spawnflags &= ~SF_FOG_FADING;
	SendData( pev->rendercolor, 0, m_iStartDist, m_iEndDist);

	if (m_fHoldTime)
	{
		SetNextThink( m_fHoldTime );
		SetThink(&CEnvFog :: TurnOff );
	}
}

void CEnvFog :: FadeOutDone ( void )
{
	pev->spawnflags &= ~SF_FOG_FADING;
	SendData( g_vecZero, 0, 0, 0);
}

void CEnvFog :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
//	ALERT(at_console, "Fog use %s %s\n", GetStringForUseType(useType), GetStringForState(GetState()));
	if (ShouldToggle(useType))
	{
		if (pev->spawnflags & SF_FOG_ACTIVE)
			TurnOff();
		else
			TurnOn();
	}
}

void CEnvFog :: SendData ( Vector col, int iFadeTime, int iStartDist, int iEndDist )
{
//	ALERT(at_console, "Fog send (%d %d %d), %d - %d\n", col.x, col.y, col.z, iStartDist, iEndDist);
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer*)UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgSetFog, NULL, pPlayer->pev );
				WRITE_BYTE ( col.x );
				WRITE_BYTE ( col.y );
				WRITE_BYTE ( col.z );
				WRITE_SHORT ( iFadeTime );
				WRITE_SHORT ( iStartDist );
				WRITE_SHORT ( iEndDist );
			MESSAGE_END();

//			pPlayer->m_iFogStartDist = iStartDist;
//			pPlayer->m_iFogEndDist = iEndDist;
//			pPlayer->m_vecFogColor = col;
//			pPlayer->m_bClientFogRefresh = FALSE;
		}
	}
}

LINK_ENTITY_TO_CLASS( env_fog, CEnvFog );

//=========================================================
// LRC - env_sky, an unreal tournament-style sky effect
//=========================================================
class CEnvSky : public CBaseEntity
{
public:
	void Activate( void );
	void Think( void );
};

void CEnvSky :: Activate ( void )
{
	pev->effects |= EF_NODRAW;
	pev->nextthink = gpGlobals->time + 1.0;
}

extern int gmsgSetSky;

void CEnvSky :: Think ()
{
	MESSAGE_BEGIN(MSG_BROADCAST, gmsgSetSky, NULL);
		WRITE_BYTE(1); // mode
		WRITE_COORD(pev->origin.x); // view position
		WRITE_COORD(pev->origin.y);
		WRITE_COORD(pev->origin.z);
	MESSAGE_END();
}

LINK_ENTITY_TO_CLASS( env_sky, CEnvSky );



//=========================================================
// LRC - env_particle, uses the aurora particle system
//=========================================================
//extern int gmsgParticle = 0;
#define SF_PARTICLE_ON 1

LINK_ENTITY_TO_CLASS( env_particle, CParticle );

void CParticle::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->renderamt		= 128;
	pev->rendermode		= kRenderTransTexture;

	// 'body' determines whether the effect is active or not
	pev->body			= (pev->spawnflags & SF_PARTICLE_ON) != 0;

	Precache();

	UTIL_SetOrigin(this, pev->origin);
	SET_MODEL(edict(), "sprites/null.spr");
}


void CParticle::Precache( void )
{
	PRECACHE_MODEL("sprites/null.spr");
}

void CParticle::SendInitMessage( CBasePlayer *player )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgParticle );
		WRITE_SHORT( entindex() );
		WRITE_STRING( STRING(pev->message) );
		WRITE_BYTE( 0 );
		WRITE_STRING( "nah" );
		WRITE_SHORT( -1 );
	MESSAGE_END();
}

void CParticle::KillParticle( void )
{
	pev->body = 0;

	if ( pev->fuser1 != 0 )
	{
		SetNextThink( pev->fuser1 );
		SetThink( &CParticle ::SUB_Remove );
	}
	else
	{
		SetNextThink( 1 );
		SetThink( &CParticle ::SUB_Remove );
	}
}

CParticle *CParticle::CreateParticle( int message, bool starton, Vector origin, int weaponattachment, float life, CBaseEntity *attachentity, float velocityx, float velocityy, float velocityz )
{
	CParticle *pParticle = GetClassPtr( (CParticle *)NULL );

	UTIL_SetOrigin( pParticle, origin );

	if ( starton )
		pParticle->pev->spawnflags = 1;
	else
		pParticle->pev->spawnflags = 0;

	pParticle->pev->skin = weaponattachment;

	if ( velocityx != 0 || velocityy != 0 || velocityz != 0 )
	{
		pParticle->pev->skin = 666;
		pParticle->pev->movetype = MOVETYPE_FLY;

		pParticle->pev->velocity.x = velocityx;
		pParticle->pev->velocity.y = velocityy;
		pParticle->pev->velocity.z = velocityz;
	}

	pParticle->pev->message = message;
	pParticle->Spawn();

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		CBasePlayer *player = GetClassPtr( (CBasePlayer *)pPlayer->pev );

		if ( player )
			pParticle->SendInitMessage( player );
	}

	if ( life >= 0.1 )
	{
		pParticle->SetNextThink( life );
		pParticle->SetThink( &CParticle::KillParticle );
	}

	pParticle->pev->classname = MAKE_STRING( "env_particle" );

	return pParticle;
}

void CParticle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, pev->body ) )
	{
		pev->body = !pev->body;
	//	ALERT(at_console, "PARTICLE Setting body %d\n", pev->body); // buz test
	}
}



// ======= rain =========

void CRainSettings::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
}

void CRainSettings::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_flDistance"))
	{
		Rain_Distance = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iMode"))
	{
		Rain_Mode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

LINK_ENTITY_TO_CLASS( rain_settings, CRainSettings );

TYPEDESCRIPTION	CRainSettings::m_SaveData[] = 
{
	DEFINE_FIELD( CRainSettings, Rain_Distance, FIELD_FLOAT ),
	DEFINE_FIELD( CRainSettings, Rain_Mode, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CRainSettings, CBaseEntity );



void CRainModify::Spawn()
{
	pev->solid = SOLID_NOT;
	pev->movetype	= MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;

	if (FStringNull(pev->targetname))
		pev->spawnflags |= 1;
}

void CRainModify::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_iDripsPerSecond"))
	{
		Rain_Drips = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flWindX"))
	{
		Rain_windX = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flWindY"))
	{
		Rain_windY = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRandX"))
	{
		Rain_randX = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flRandY"))
	{
		Rain_randY = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_flTime"))
	{
		fadeTime = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	
	{
		CBaseEntity::KeyValue( pkvd );
	}
}

void CRainModify::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (pev->spawnflags & 1)
		return; // constant

	if (gpGlobals->deathmatch)
	{
		ALERT(at_console, "Rain error: only static rain in multiplayer\n");
		return; // not in multiplayer
	}

	CBasePlayer *pPlayer;
	pPlayer = (CBasePlayer *)CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));

	if (fadeTime)
	{ // write to 'ideal' settings
		pPlayer->Rain_ideal_dripsPerSecond = Rain_Drips;
		pPlayer->Rain_ideal_randX = Rain_randX;
		pPlayer->Rain_ideal_randY = Rain_randY;
		pPlayer->Rain_ideal_windX = Rain_windX;
		pPlayer->Rain_ideal_windY = Rain_windY;

		pPlayer->Rain_endFade = gpGlobals->time + fadeTime;
		pPlayer->Rain_nextFadeUpdate = gpGlobals->time + 1;
	}
	else
	{
		pPlayer->Rain_dripsPerSecond = Rain_Drips;
		pPlayer->Rain_randX = Rain_randX;
		pPlayer->Rain_randY = Rain_randY;
		pPlayer->Rain_windX = Rain_windX;
		pPlayer->Rain_windY = Rain_windY;

		pPlayer->Rain_needsUpdate = 1;
	}
}

LINK_ENTITY_TO_CLASS( rain_modify, CRainModify );

TYPEDESCRIPTION	CRainModify::m_SaveData[] = 
{
	DEFINE_FIELD( CRainModify, fadeTime, FIELD_FLOAT ),
	DEFINE_FIELD( CRainModify, Rain_Drips, FIELD_INTEGER ),
	DEFINE_FIELD( CRainModify, Rain_randX, FIELD_FLOAT ),
	DEFINE_FIELD( CRainModify, Rain_randY, FIELD_FLOAT ),
	DEFINE_FIELD( CRainModify, Rain_windX, FIELD_FLOAT ),
	DEFINE_FIELD( CRainModify, Rain_windY, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CRainModify, CBaseEntity );





// ====================================
// buz - grass
// ====================================
#define MAX_TYPE_TEXTURES	8 // must be same as defined in client's grass.cpp
extern int gmsgGrass;

class CGrassSprite  : public CPointEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void SendInitMessage( CBasePlayer *player );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

//	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	float	m_fRadius;
	int		m_iDensity;
	int		m_fScale;
	int		m_iMode;

	int m_iNumTextures;
	int m_strTextures[MAX_TYPE_TEXTURES];
};

LINK_ENTITY_TO_CLASS( env_grass_sprite, CGrassSprite );

TYPEDESCRIPTION	CGrassSprite::m_SaveData[] = 
{
	DEFINE_FIELD( CGrassSprite, m_fRadius, FIELD_FLOAT ),
	DEFINE_FIELD( CGrassSprite, m_iDensity, FIELD_INTEGER ),
	DEFINE_FIELD( CGrassSprite, m_fScale, FIELD_INTEGER ),
	DEFINE_FIELD( CGrassSprite, m_iMode, FIELD_INTEGER ),
	DEFINE_FIELD( CGrassSprite, m_iNumTextures, FIELD_INTEGER ),
	DEFINE_ARRAY( CGrassSprite, m_strTextures, FIELD_STRING, MAX_TYPE_TEXTURES ),
};

IMPLEMENT_SAVERESTORE( CGrassSprite, CPointEntity );

void CGrassSprite::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects |= EF_NODRAW;
}

void CGrassSprite::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "m_fRadius"))
	{
		m_fRadius = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iDensity"))
	{
		m_iDensity = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_fScale"))
	{
		float flscale = atof(pkvd->szValue);
		m_fScale = (int)(flscale * 256);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iMode"))
	{
		m_iMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else // add this to texture list
	{
		if (m_iNumTextures < MAX_TYPE_TEXTURES)
		{
			m_strTextures[m_iNumTextures] = ALLOC_STRING(pkvd->szKeyName);
			m_iNumTextures++;
			pkvd->fHandled = TRUE;
		}
		else
			ALERT(at_console, "CGrassSprite Warning: too many textures\n");
	}
}

void CGrassSprite::Precache()
{
	PRECACHE_MODEL( (char *)STRING(pev->model) );
//	ALERT(at_console, "precached this suxx %s\n", STRING(pev->model));
}

void CGrassSprite::SendInitMessage( CBasePlayer *player )
{
	// grass info message format:
	//		coord radius
	//		coord height
	//		short numents
	//		short (sprite scale)*256
	//		byte  mode
	//		byte  numtextures
	//			strings[numtextures] texture names
	//		string sprite name

	MESSAGE_BEGIN( MSG_ONE, gmsgGrass, NULL, player->pev );
		WRITE_COORD( m_fRadius );
		WRITE_COORD( pev->origin[2] );
		WRITE_SHORT( m_iDensity );
		WRITE_SHORT( m_fScale );
		WRITE_BYTE( m_iMode );
		WRITE_BYTE( m_iNumTextures );
		for (int i = 0; i < m_iNumTextures; i++)
			WRITE_STRING( STRING(m_strTextures[i]) );
		WRITE_STRING( STRING(pev->model) );
	MESSAGE_END();
}



// =================================
// buz: static decals
//
// netname - decal group name
// skin - direction specified
// =================================
extern int gmsgCustomDecal;

class CStaticDecal : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}

	int PasteDecal( int dir, CBasePlayer *player )
	{
		Vector vecdir(0,0,0);
		vecdir[dir/2] = (dir & 1) ? 10 : -10;	

		TraceResult tr;
		UTIL_TraceLine ( pev->origin, pev->origin + vecdir, ignore_monsters, ENT(pev), &tr);
		
		if (tr.flFraction == 1.0)
			return FALSE;

		if (tr.pHit)
		{
			int idx = ENTINDEX(tr.pHit);
			if (idx)
				return FALSE;
		}

		MESSAGE_BEGIN( MSG_ONE, gmsgCustomDecal, NULL, player->pev );
			WRITE_COORD( tr.vecEndPos.x );
			WRITE_COORD( tr.vecEndPos.y );
			WRITE_COORD( tr.vecEndPos.z );
			WRITE_COORD( tr.vecPlaneNormal.x );
			WRITE_COORD( tr.vecPlaneNormal.y );
			WRITE_COORD( tr.vecPlaneNormal.z );
			WRITE_BYTE( 1 );
			WRITE_STRING( STRING(pev->netname) );
		MESSAGE_END();
		return TRUE;
	}

	void SendInitMessage( CBasePlayer *player )
	{
		if (pev->skin)
		{
			if (pev->skin > 6)
			{
				ALERT(at_console, "Invalid dir value in static decal entity!\n");
				return;
			}

			PasteDecal( pev->skin - 1, player );
			return;
		}

		// try all directions
		for (int i = 0; i < 6; i++)
		{
			if (PasteDecal(i, player))
				return;
		}
	}
};

LINK_ENTITY_TO_CLASS( env_static_decal, CStaticDecal );



// =================================
// buz: 3d sky info messages
//
// envpos_sky: sets view origin in 3d sky
//		origin	- origin
//		skin	- ambientlight
//		body	- shadelight
//		flags	- use amblight hack | use shadelight hack
//		
// envpos_world: sets view origin in world (when sky movement requed)
//		origin	- origin
//		health	- speed
//
// =================================
extern int gmsgSkymarker_Sky;
extern int gmsgSkymarker_World;

#define SKYFLAG_USE_AMBLIGHT_HACK		1
#define SKYFLAG_USE_SHADELIGHT_HACK		2

class CEnvPos_Sky : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}
	
	void SendInitMessage( CBasePlayer *player )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSkymarker_Sky, NULL, player->pev );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_BYTE( pev->spawnflags );
			if (pev->spawnflags & SKYFLAG_USE_AMBLIGHT_HACK)
				WRITE_BYTE( pev->skin );
			if (pev->spawnflags & SKYFLAG_USE_SHADELIGHT_HACK)
				WRITE_BYTE( pev->body );
		MESSAGE_END();
	}
};


class CEnvPos_World : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}
	
	void SendInitMessage( CBasePlayer *player )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgSkymarker_World, NULL, player->pev );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
			WRITE_COORD( pev->health );
		MESSAGE_END();
	}
};

LINK_ENTITY_TO_CLASS( envpos_sky, CEnvPos_Sky );
LINK_ENTITY_TO_CLASS( envpos_world, CEnvPos_World );



// buz: trigger_followme
// formally, it does not any special, just fires target entity with chosen use type,
// and player sent as caller

class CTriggerFollow : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}
	
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		CBaseEntity *pEnt = CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));
		if (pEnt && pEnt->IsPlayer())
		{
			USE_TYPE send = USE_TOGGLE;
			if (pev->impulse == 0) send = USE_ON;
			else if (pev->impulse == 1) send = USE_OFF;
			FireTargets(STRING(pev->target), this, pEnt, send, 0);
		}
		else
			ALERT(at_console, "ERROR: cant get player!\n");
	}			
};

LINK_ENTITY_TO_CLASS( trigger_followme, CTriggerFollow );


// cof_survivalmode and cof_phonecheck are declared around CDynamicLight
// (their vtables lie on both sides of it in retail effects.obj)
class CCoFSurvivalMode : public CPointEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT SurvivalThink( void );
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

// buz: dynamic light entity
#define SF_DYNLIGHT_STARTOFF 1

class CDynamicLight : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->renderfx = 71; // dynamic light

		Precache();
		SET_MODEL(ENT(pev),"sprites/null.spr"); // should be visible to send to client
		UTIL_SetSize(pev, g_vecZero, g_vecZero);

		if (pev->spawnflags & SF_DYNLIGHT_STARTOFF)
			pev->effects |= EF_NODRAW;

		pev->renderamt = pev->renderamt / 8;
	}

	void Precache()
	{
		PRECACHE_MODEL("sprites/null.spr");
	}

	STATE GetState( void )
	{
		if (pev->effects & EF_NODRAW)
			return STATE_OFF;
		else
			return STATE_ON;
	}
	
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
	//	if (!ShouldToggle( useType ))
	//		return;

		if (useType == USE_ON)
			pev->effects &= ~EF_NODRAW;
		else if (useType == USE_OFF)
			pev->effects |= EF_NODRAW;
		else if (useType == USE_TOGGLE)
		{
			if (pev->effects & EF_NODRAW)
				pev->effects &= ~EF_NODRAW;
			else
				pev->effects |= EF_NODRAW;
		}
	}

	static CDynamicLight *CreateLight( Vector origin, int r, int g, int b, int radius );
};

class CCofPhoneCheck : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void UnlockCheck( int number );
	void DistanceCheck( int number );
};

CDynamicLight *CDynamicLight::CreateLight( Vector origin, int r, int g, int b, int radius )
{
	CDynamicLight *pDlight = GetClassPtr( (CDynamicLight *)NULL );
	pDlight->Spawn();
	pDlight->pev->origin = origin;

	pDlight->pev->classname = MAKE_STRING( "env_dynlight" );


	pDlight->pev->rendercolor.x = r;
	pDlight->pev->rendercolor.y = g;
	pDlight->pev->rendercolor.z = b;
	pDlight->pev->renderamt = radius;

	return pDlight;
}

LINK_ENTITY_TO_CLASS( env_dynlight, CDynamicLight );

//=========================================================
// cof_burningbarrel
//=========================================================
class CBurningBarrel : public CBaseAnimating
{
	virtual void Spawn( void );
	virtual void Precache( void );
	void SetCollision( void );
	CParticle *m_pParticle;
};

LINK_ENTITY_TO_CLASS( cof_burningbarrel, CBurningBarrel );

void CBurningBarrel::SetCollision( void )
{
	studiohdr_t *pstudiohdr;
	pstudiohdr = (studiohdr_t *)GET_MODEL_PTR( ENT(pev) );

	if ( pstudiohdr == NULL )
	{
		ALERT( at_console, "Unable to fetch model pointer!\n" );
		return;
	}

	mstudioseqdesc_t *pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	UTIL_SetSize( pev, pseqdesc[ pev->sequence ].bbmin, pseqdesc[ pev->sequence ].bbmax );
}

void CBurningBarrel::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT(pev), STRING(pev->model) );
	else
		SET_MODEL( ENT(pev), "models/props/utomhusd/barrel_new.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	SetCollision();

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT(pev) );

	if ( pev->noise )
		EMIT_SOUND( edict(), CHAN_VOICE, STRING(pev->noise), 1, ATTN_NORM );
	else
		EMIT_SOUND( edict(), CHAN_VOICE, "ambience/burning_barrel.wav", 1, ATTN_NORM );

	if ( !m_pParticle )
	{
		Vector origin, angle;
		GetAttachment( 0, origin, angle );
		m_pParticle = CParticle::CreateParticle( MAKE_STRING("aurora/fireysmoke.aur"), true, origin, 0, 0, this, 0, 0, 0 );
	}
}

void CBurningBarrel::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL( "models/props/utomhusd/barrel_new.mdl" );

	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
	else
		PRECACHE_SOUND( "ambience/burning_barrel.wav" );
}

//=========================================================
// cof_blur
//=========================================================
class CCofBlur : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_blur, CCofBlur );

void CCofBlur::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofBlur::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer->m_iBlurActive == 0 )
	{
		pPlayer->m_iBlurActive = 1;
	}
	else
	{
		pPlayer->m_iBlurActive = 0;
	}
}

//=========================================================
// trigger_cofmobile
//=========================================================
class CCofTriggerMobile : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_iszBodyNumber;
};

TYPEDESCRIPTION	CCofTriggerMobile::m_SaveData[] =
{
	DEFINE_FIELD( CCofTriggerMobile, m_iszBodyNumber, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCofTriggerMobile, CBaseEntity );

LINK_ENTITY_TO_CLASS( trigger_cofmobile, CCofTriggerMobile );

void CCofTriggerMobile::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "bodynumber" ) )
	{
		m_iszBodyNumber = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofTriggerMobile::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofTriggerMobile::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			CBasePlayerItem *pWeapon;

			for ( int j = 0; j < MAX_ITEM_TYPES; j++ )
			{
				pWeapon = plr->m_rgpPlayerItems[j];

				while ( pWeapon )
				{
					if ( FClassnameIs( pWeapon->pev, "weapon_mobile" ) )
					{
						CMobile *pMobile = GetClassPtr( (CMobile *)pWeapon->pev );

						if ( pMobile && !pMobile->m_bDisabled )
						{
							plr->m_iPhoneMessage = m_iszBodyNumber;

							plr->m_UnreadSMS = true;

							if ( pev->spawnflags & 2 )
							{
								plr->m_bPhoneLow = true;
								plr->m_fNextPhoneWarnTime = gpGlobals->time;
							}

							if ( plr->m_bPhoneLow )
							{
								MESSAGE_BEGIN( MSG_ONE, gmsgUnreadSMS, NULL, plr->pev );
									WRITE_BYTE( 1 );
									WRITE_BYTE( 1 );
								MESSAGE_END();
							}
							else
							{
								MESSAGE_BEGIN( MSG_ONE, gmsgUnreadSMS, NULL, plr->pev );
									WRITE_BYTE( 1 );
									WRITE_BYTE( 0 );
								MESSAGE_END();
							}

							if ( plr->m_pActiveItem && FClassnameIs( plr->m_pActiveItem->pev, "weapon_mobile" ) )
							{
								if ( plr->m_iMobileFlashMode )
									pMobile->SendWeaponAnim( 6, 1, 0 );
								else
									pMobile->SendWeaponAnim( 1, 1, 0 );
							}

							pMobile->m_flNextPrimaryAttack = pMobile->m_flNextSecondaryAttack = pMobile->m_flTimeWeaponIdle = gpGlobals->time + 1.52;
						}

						if ( !( pev->spawnflags & 1 ) )
						{
							EMIT_SOUND( ENT(plr->pev), CHAN_WEAPON, "weapons/mobile/mobile_sms.wav", 1, ATTN_NORM );
							EMIT_SOUND( ENT(plr->pev), CHAN_AUTO, "phone/phone_vibrate.wav", 1, ATTN_NORM );
						}

						break;
					}

					pWeapon = pWeapon->m_pNext;
				}
			}
		}
	}
}

//=========================================================
// cof_clearitems
//=========================================================
class CCofClearItems : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_clearitems, CCofClearItems );

void CCofClearItems::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofClearItems::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	bool hadtapes = false;

	if ( pPlayer->HasItemInSlot( "inventoryitems/cassettetape.txt", false ) )
		hadtapes = true;

	if ( pev->message )
	{
		if ( pPlayer )
		{
			if ( strstr( STRING(pev->message), "weapon_" ) )
			{
				CBasePlayerItem *pItem = NULL;

				for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
				{
					if ( pPlayer->m_rgpPlayerItems[i] )
					{
						pItem = pPlayer->m_rgpPlayerItems[i];

						while ( pItem )
						{
							if ( FClassnameIs( pItem->pev, STRING(pev->message) ) )
							{
								// remove it from the quick slots
								if ( !strcmp( pPlayer->m_sQuickSel1, STRING(pItem->pev->classname) ) )
								{
									strcpy( pPlayer->m_sQuickSel1, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pPlayer->pev );
										WRITE_BYTE( 1 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									pPlayer->m_iQuickSlotSlot[1] = 0;
								}

								if ( !strcmp( pPlayer->m_sQuickSel2, STRING(pItem->pev->classname) ) )
								{
									strcpy( pPlayer->m_sQuickSel2, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pPlayer->pev );
										WRITE_BYTE( 2 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									pPlayer->m_iQuickSlotSlot[2] = 0;
								}

								if ( !strcmp( pPlayer->m_sQuickSel3, STRING(pItem->pev->classname) ) )
								{
									strcpy( pPlayer->m_sQuickSel3, "" );

									MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pPlayer->pev );
										WRITE_BYTE( 3 );
										WRITE_BYTE( 0 );
									MESSAGE_END();

									pPlayer->m_iQuickSlotSlot[3] = 0;
								}

								// dual wielding this weapon, switch to the other one
								if ( pPlayer->m_iDualWeaponLeftID == pItem->m_iId )
								{
									if ( GetClassnameForId[pPlayer->m_iDualWeaponRightID] && pPlayer->HasNamedPlayerItem( GetClassnameForId[pPlayer->m_iDualWeaponRightID] ) )
									{
										if ( pPlayer->m_rgpPlayerItems[pPlayer->m_iDualWeaponRightID] )
										{
											pPlayer->SwitchWeapon( pPlayer->GetPlayerWeapon( pPlayer->m_iDualWeaponRightID ) );


											pPlayer->SetDualWieldWeapon( 0 );


											pPlayer->m_iDualWeaponLeftAmmo = -2;
											pPlayer->m_iDualWeaponRightAmmo = -2;
										}
									}
								}

								if ( pPlayer->m_iDualWeaponRightID == pItem->m_iId )
								{
									if ( GetClassnameForId[pPlayer->m_iDualWeaponLeftID] && pPlayer->HasNamedPlayerItem( GetClassnameForId[pPlayer->m_iDualWeaponLeftID] ) )
									{
										if ( pPlayer->m_rgpPlayerItems[pPlayer->m_iDualWeaponLeftID] )
										{
											pPlayer->SwitchWeapon( pPlayer->GetPlayerWeapon( pPlayer->m_iDualWeaponLeftID ) );


											pPlayer->SetDualWieldWeapon( 0 );


											pPlayer->m_iDualWeaponLeftAmmo = -2;
											pPlayer->m_iDualWeaponRightAmmo = -2;
										}
									}
								}

								pPlayer->ClearOutSlotNamed( pItem->script, 0 );

								pPlayer->pev->weapons &= ~(1 << pItem->m_iId);
								pPlayer->ClearOutSlotNamed( pItem->pev->message, 0 );
								pItem->DestroyItem();
								break;
							}

							pItem = pItem->m_pNext;
						}
					}

					if ( pItem )
						break;
				}
			}
			else
			{
				pPlayer->ClearOutSlotNamed( pev->message, 0 );
			}
		}
	}

	if ( ( pev->spawnflags & 1 ) && !( pev->spawnflags & 2 ) )
	{
		pPlayer->ClearOutSlot( 1, 1 );
		pPlayer->RemoveAllItems( FALSE );


		pPlayer->SetDualWieldWeapon( 0 );


		pPlayer->m_iDualWeaponLeftAmmo = -2;
		pPlayer->m_iDualWeaponRightAmmo = -2;

		strcpy( pPlayer->m_sQuickSel1, "" );
		strcpy( pPlayer->m_sQuickSel2, "" );
		strcpy( pPlayer->m_sQuickSel3, "" );

		for ( int i = 1; i <= 3; i++ )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, pPlayer->pev );
				WRITE_BYTE( i );
				WRITE_BYTE( 0 );
			MESSAGE_END();

			pPlayer->m_iQuickSlotSlot[i] = 0;
		}
	}

	if ( ( pev->spawnflags & 1 ) && ( pev->spawnflags & 2 ) )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				plr->ClearOutSlot( 1, 1 );
				plr->RemoveAllItems( FALSE );


				plr->SetDualWieldWeapon( 0 );


				plr->m_iDualWeaponLeftAmmo = -2;
				plr->m_iDualWeaponRightAmmo = -2;

				strcpy( plr->m_sQuickSel1, "" );
				strcpy( plr->m_sQuickSel2, "" );
				strcpy( plr->m_sQuickSel3, "" );

				for ( int i = 1; i <= 3; i++ )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgQSlotClear, NULL, plr->pev );
						WRITE_BYTE( i );
						WRITE_BYTE( 0 );
					MESSAGE_END();

					pPlayer->m_iQuickSlotSlot[i] = 0;
				}
			}
		}
	}

	if ( pev->spawnflags & 4 )
	{
		if ( pev->spawnflags & 2 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr )
				{
					plr->m_bThreeSlots = true;

					for ( int j = 1; j <= 3; j++ )
						plr->InventoryMessage( j, plr->cofrInventorySlot[j], 0 );
				}
			}
		}
		else
		{
			pPlayer->m_bThreeSlots = true;

			for ( int k = 1; k <= 3; k++ )
				pPlayer->InventoryMessage( k, pPlayer->cofrInventorySlot[k], 0 );
		}
	}

	if ( pPlayer->m_iTapes && hadtapes && !strcmp( STRING(pev->targetname), "bagremove" ) )
	{
		pPlayer->m_bSilentGive = true;
		pPlayer->AddItemToInventory( MAKE_STRING("inventoryitems/cassettetape.txt"), MAKE_STRING("models/items/cassette_tape.mdl"), MAKE_STRING("item_key") );
		pPlayer->m_bSilentGive = false;
	}
}

//=========================================================
// cof_serversettings
//=========================================================
class CCofServerSettings : public CBaseEntity
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( cof_serversettings, CCofServerSettings );

void CCofServerSettings::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

//=========================================================
// cof_difficultysettings
//=========================================================
class CCofDifficultySettings : public CBaseEntity
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( cof_difficultysettings, CCofDifficultySettings );

void CCofDifficultySettings::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

//=========================================================
// cof_unlockables
//=========================================================
class CCofUnlockables : public CBaseEntity
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( cof_unlockables, CCofUnlockables );

void CCofUnlockables::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

//=========================================================
// cof_loadgame
//=========================================================
class CCofLoadGame : public CBaseEntity
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( cof_loadgame, CCofLoadGame );

void CCofLoadGame::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

//=========================================================
// cof_gamemenu
//=========================================================
class CCofGameMenu : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Think( void );
};

LINK_ENTITY_TO_CLASS( cof_gamemenu, CCofGameMenu );

void CCofGameMenu::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofGameMenu::Think( void )
{
	if ( pev->iuser1 == 1 )
		SERVER_COMMAND( "map c_difficulty_settings\n" );
	if ( pev->iuser1 == 2 )
		SERVER_COMMAND( "map c_video_stress_test\n" );
}

//=========================================================
// cof_blackandwhite
//=========================================================
class CCofBlackAndWhite : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_blackandwhite, CCofBlackAndWhite );

void CCofBlackAndWhite::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofBlackAndWhite::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( pev->iuser1 > 255 )
		pev->iuser1 = 255;

	if ( pev->iuser1 < 0 )
		pev->iuser1 = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBlackWhite, NULL, pPlayer->pev );
				WRITE_BYTE( pev->iuser1 );
			MESSAGE_END();

			pPlayer->m_iBlackAndWhite = pev->iuser1;
		}
	}
}

//=========================================================
// cof_cracker
//=========================================================
class CCofCracker : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_cracker, CCofCracker );

void CCofCracker::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofCracker::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, 1024 ) ) != NULL )
	{
		if ( pEntity->pev->takedamage != DAMAGE_NO )
		{
			if ( FClassnameIs( pEntity->pev, "monster_sewerboss" ) || FClassnameIs( pEntity->pev, "player" ) )
			{
				if ( pEntity->pev->waterlevel > 0 )
					pEntity->TakeDamage( pev, pev, pev->dmg, DMG_SHOCK );
			}
		}
	}
}

//=========================================================
// cof_document
//=========================================================
LINK_ENTITY_TO_CLASS( cof_document, CCoFDocument );

TYPEDESCRIPTION	CCoFDocument::m_SaveData[] =
{
	DEFINE_ARRAY( CCoFDocument, page1, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, page2, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, page3, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, page4, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, note1, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, note2, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, note3, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( CCoFDocument, note4, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( CCoFDocument, turnsound, FIELD_SOUNDNAME ),
	DEFINE_ARRAY( CCoFDocument, label, FIELD_CHARACTER, 48 ),
};

IMPLEMENT_SAVERESTORE( CCoFDocument, CBaseEntity );

void CCoFDocument::Spawn( void )
{
	Precache();


	if ( strlen( label ) <= 1 )
		strcpy( label, "gfx/vgui/notes/hiddennote1.tga" );

	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCoFDocument::Precache( void )
{
	if ( turnsound )
		PRECACHE_SOUND( (char *)STRING(turnsound) );
}

void CCoFDocument::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		pPlayer->m_pCurrentDocument = this;

		MESSAGE_BEGIN( MSG_ONE, gmsgNoteDoc, NULL, pPlayer->pev );
			WRITE_STRING( page1 );
			WRITE_STRING( page2 );
			WRITE_STRING( page3 );
			WRITE_STRING( page4 );
			WRITE_STRING( note1 );
			WRITE_STRING( note2 );
			WRITE_STRING( note3 );
			WRITE_STRING( note4 );
			WRITE_STRING( STRING(turnsound) );
		MESSAGE_END();
	}
}

void CCoFDocument::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "page1" ) )
	{
		strcpy( page1, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "page2" ) )
	{
		strcpy( page2, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "page3" ) )
	{
		strcpy( page3, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "page4" ) )
	{
		strcpy( page4, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "note1" ) )
	{
		strcpy( note1, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "note2" ) )
	{
		strcpy( note2, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "note3" ) )
	{
		strcpy( note3, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "note4" ) )
	{
		strcpy( note4, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "turnsound" ) )
	{

		turnsound = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "label" ) )
	{
		strcpy( label, pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

//=========================================================
// cof_entteleport
//=========================================================
class CCofEntTeleport : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_entteleport, CCofEntTeleport );

void CCofEntTeleport::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofEntTeleport::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

	if ( pEntity )
	{
		UTIL_SetOrigin( pEntity, pev->origin );
	}
}

//=========================================================
// cof_giveitems
//=========================================================
class CCofGiveItems : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_giveitems, CCofGiveItems );

void CCofGiveItems::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofGiveItems::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->message )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( !pPlayer )
		return;

	pPlayer->m_bSilentGive = false;

	if ( pev->spawnflags & 1 )
	{
		if ( pPlayer )
		{
			if ( pPlayer->AddItemToInventory( pev->message, 0, 0 ) )
			{
				return;
			}
			else
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );


				CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", pev->message, pPlayer->pev->origin, pPlayer->pev->angles, pPlayer->edict(), 0 );

				if ( pKey )
				{
					pKey->pev->message = pev->message;
					pKey->pev->angles.x = 0;
					pKey->pev->angles.z = 0;
				}
			}
		}
	}
	else
	{
		if ( pPlayer )
		{
			if ( pPlayer->InventoryIsFull() == 1 )
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );
				pPlayer->GiveNamedItem( STRING(pev->message), false, false );
			}
			else if ( pev->spawnflags & 2 )
			{
				pPlayer->GiveNamedItem( STRING(pev->message), false, false );
			}
			else if ( pev->spawnflags & 16 )
			{
				gEvilImpulse101 = TRUE;
				pPlayer->GiveNamedItem( STRING(pev->message), false, false );
				gEvilImpulse101 = FALSE;
			}
			else
			{
				gSpecialItemGive = TRUE;
				pPlayer->m_bSilentGive = true;
				pPlayer->GiveNamedItem( STRING(pev->message), false, false );
				gSpecialItemGive = FALSE;
			}
		}
	}
}

//=========================================================
// cof_interdooronoff
//=========================================================
class CCofInterDoorOnOff : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_iszNewLockedBy;
};

void CCofInterDoorOnOff::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "newlockedby" ) )
	{
		m_iszNewLockedBy = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

TYPEDESCRIPTION	CCofInterDoorOnOff::m_SaveData[] =
{
	DEFINE_FIELD( CCofInterDoorOnOff, m_iszNewLockedBy, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCofInterDoorOnOff, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_interdooronoff, CCofInterDoorOnOff );

void CCofInterDoorOnOff::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofInterDoorOnOff::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->message), pActivator );

	if ( pTarget && FClassnameIs( pTarget->pev, "inter_door" ) )
	{
		CInterDoor *pDoor = GetClassPtr( (CInterDoor *)pTarget->pev );

		if ( pDoor->IsLocked() )
			pDoor->m_sLockedBy = 0;
		else
			pDoor->m_sLockedBy = MAKE_STRING( "somethinglocked" );
	}

	if ( pTarget && FClassnameIs( pTarget->pev, "func_button" ) )
	{
		CBaseButton *pButton = GetClassPtr( (CBaseButton *)pTarget->pev );

		if ( pButton )
		{
			if ( pButton->m_iszLockedBy )
			{
				pButton->m_isLocked = false;
				pButton->m_iszLockedBy = 0;
			}
			else
			{
				pButton->m_iszLockedBy = MAKE_STRING( "somethinglocked" );
				pButton->m_isLocked = true;
			}
		}
	}
}

//=========================================================
// cof_keypad (the class is declared in cof_keypad.h)
//=========================================================
TYPEDESCRIPTION	CCofKeypad::m_SaveData[] =
{
	DEFINE_FIELD( CCofKeypad, m_IsCorrect, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CCofKeypad, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_keypad, CCofKeypad );

void CCofKeypad::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofKeypad::Precache( void )
{
	PRECACHE_SOUND( "keypad/keypad_activate.wav" );
	PRECACHE_SOUND( "keypad/keypad_deny.wav" );
	PRECACHE_SOUND( "keypad/keypad_press.wav" );
}

void CCofKeypad::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !m_IsCorrect )
	{
		if ( pActivator && pActivator->IsPlayer() )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

			if ( pPlayer )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgKeypad, NULL, pPlayer->pev );
					WRITE_BYTE( pPlayer->m_iKeypadType );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 1 );
				MESSAGE_END();
			}
		}
	}
}

//=========================================================
// cof_keypad2
//=========================================================
class CCofKeypad2 : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_keypad2, CCofKeypad2 );

void CCofKeypad2::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofKeypad2::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		char szNumber[8];
		sprintf( szNumber, "279%i", RANDOM_LONG( 1, 9 ) );

		pPlayer->m_iKeypadNumber = atoi( szNumber );
		pPlayer->m_iKeypadType = 1;



		MESSAGE_BEGIN( MSG_ONE, gmsgKeypad, NULL, pPlayer->pev );
			WRITE_BYTE( pPlayer->m_iKeypadType );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

//=========================================================
// cof_keypad3
//=========================================================
class CCofKeypad3 : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_keypad3, CCofKeypad3 );

void CCofKeypad3::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofKeypad3::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		pPlayer->m_iKeypadType = 2;



		MESSAGE_BEGIN( MSG_ONE, gmsgKeypad, NULL, pPlayer->pev );
			WRITE_BYTE( pPlayer->m_iKeypadType );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
}

//=========================================================
// cof_keypad4
//=========================================================
class CCofKeypad4 : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_keypad4, CCofKeypad4 );

void CCofKeypad4::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofKeypad4::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			pPlayer->m_iKeypadType = 2;
			pPlayer->m_iKeypadNumber = 5247;

			MESSAGE_BEGIN( MSG_ONE, gmsgKeypad, NULL, pPlayer->pev );
				WRITE_BYTE( pPlayer->m_iKeypadType );
				WRITE_BYTE( 0 );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}
}

//=========================================================
// cof_phonetrigger
//=========================================================
class CCofPhoneTrigger : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_phonetrigger, CCofPhoneTrigger );

void CCofPhoneTrigger::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofPhoneTrigger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	FireTargets( STRING(pev->target), this, this, USE_TOGGLE, 0 );

	if ( !( pev->spawnflags & 1 ) )
		UTIL_Remove( this );
}

//=========================================================
// cof_phonecheck
//=========================================================
LINK_ENTITY_TO_CLASS( cof_phonecheck, CCofPhoneCheck );

void CCofPhoneCheck::Spawn( void )
{
	if ( pev->noise )
		Precache();

	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->movetype = MOVETYPE_NONE;

	if ( pev->spawnflags & 2 )
		pev->frags = 1;
	else
		pev->frags = 0;


	if ( pev->iuser1 == 0 )
		pev->iuser1 = 128;
}

void CCofPhoneCheck::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CCofPhoneCheck::DistanceCheck( int number )
{
	float m_distance = pev->iuser1;

	if ( m_distance < 32 )
		m_distance = 32;

	CBaseEntity *pEntity = NULL;

	while ( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, m_distance ) ) != NULL )
	{
		if ( FClassnameIs( pEntity->pev, "player" ) )
		{
			UnlockCheck( number );
			break;
		}
	}
}

void CCofPhoneCheck::UnlockCheck( int number )
{
	if ( number == pev->impulse )
	{
		if ( pev->owner )
		{
			CBaseEntity *pFire = CBaseEntity::Instance( pev->owner );

			if ( pFire )
				FireTargets( STRING(pev->target), pFire, pFire, USE_TOGGLE, 0 );
		}

		if ( pev->noise )
			EMIT_SOUND( ENT(pev), CHAN_ITEM, STRING(pev->noise), 1, ATTN_NORM );

		if ( !( pev->spawnflags & 1 ) )
			UTIL_Remove( this );
	}
}

//=========================================================
// cof_updatekeypad
//=========================================================
class CCofUpdateKeypad : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_updatekeypad, CCofUpdateKeypad );

void CCofUpdateKeypad::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofUpdateKeypad::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );
	CBaseEntity *pEnt = UTIL_FindEntityByTargetname( NULL, STRING(pev->message), pActivator );

	if ( pEnt )
		pEnt->Use( pActivator, pCaller, USE_TOGGLE, 0 );
}

//=========================================================
// cof_killmp3
//=========================================================
class CCofKillMP3 : public CBaseMonster
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_killmp3, CCofKillMP3 );

void CCofKillMP3::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofKillMP3::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			{

				CVAR_SET_STRING( "sv_coop_music", "" );
				m_fMusicManagerFadeTime = pev->frags;
				m_iMusicManagerLoops = 0;

				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr )
						plr->PlayerStopMP3( pev->frags );
				}
			}
			else
				pPlayer->PlayerStopMP3( pev->frags );
		}
	}
}

//=========================================================
// cof_logo
//=========================================================
class CCofLogo : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_logo, CCofLogo );

void CCofLogo::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofLogo::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCoFLogo, NULL, pPlayer->pev );
			WRITE_BYTE( pev->frags );
		MESSAGE_END();
	}
}

//=========================================================
// cof_speedchange
//=========================================================
class CCofSpeedChange : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_speedchange, CCofSpeedChange );

void CCofSpeedChange::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofSpeedChange::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

	if ( pEntity )
	{
		pEntity->pev->speed = pev->speed;
	}
}

//=========================================================
// cof_teleport
//=========================================================
class CCofTeleport : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_teleport, CCofTeleport );

void CCofTeleport::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofTeleport::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;




	if ( !pev->target )
		return;

	CBaseEntity *pMonster = NULL;

	if ( pev->message )
		pMonster = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );



	if ( pActivator )
	{
		CBasePlayer *pPlayer = NULL;

		if ( pActivator->IsPlayer() )
			pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer && !pev->message )
		{
			CBaseEntity *pTarget = NULL;
			pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target) );

			if ( !pTarget )
				return;

			Vector tmp = pTarget->pev->origin;

			if ( pPlayer->IsPlayer() )
			{
				tmp.z -= pPlayer->pev->mins.z;// make origin adjustments in case the teleportee is a player. (origin in center, not at feet)
			}

			tmp.z++;
			UTIL_SetOrigin( pPlayer, tmp );
			pPlayer->m_vPreviousOrigin = tmp;

			pPlayer->pev->angles = pTarget->pev->angles;
			pPlayer->pev->velocity = pPlayer->pev->basevelocity = g_vecZero;

			if ( pPlayer->IsPlayer() )
			{
				pPlayer->pev->v_angle.y = pTarget->pev->angles.y;
				pPlayer->pev->fixangle = TRUE;
			}


			pPlayer->pev->flags &= ~FL_ONGROUND;
			pPlayer->pev->fixangle = TRUE;

			pPlayer->b_GoingThroughADoor = false;
		}
		else if ( pev->message )
		{

			if ( pMonster )
			{
				CBaseEntity *pTarget = NULL;
				pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target) );

				if ( !pTarget )
					return;

				Vector tmp = pTarget->pev->origin;

				tmp.z++;
				UTIL_SetOrigin( pMonster, tmp );

				pMonster->pev->angles = pTarget->pev->angles;
				pMonster->pev->velocity = pMonster->pev->basevelocity = g_vecZero;

				pMonster->pev->flags &= ~FL_ONGROUND;
				pMonster->pev->fixangle = TRUE;
			}
		}
	}
}

//=========================================================
// cof_trainprop
//=========================================================
class CCofTrainProp : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT PropThink( void );
	void SwitchModel( void );
};

LINK_ENTITY_TO_CLASS( cof_trainprop, CCofTrainProp );

void CCofTrainProp::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if ( pev->spawnflags & 1 )
		SET_MODEL( ENT(pev), "models/props/trainjourney/train_hill.mdl" );
	else
	{
		switch ( RANDOM_LONG( 0, 5 ) )
		{
		case 0:
			SET_MODEL( ENT(pev), "models/props/trainjourney/buske.mdl" );
			break;
		case 1:
			SET_MODEL( ENT(pev), "models/props/trainjourney/buske2.mdl" );
			break;
		case 2:
			SET_MODEL( ENT(pev), "models/props/trainjourney/buske3.mdl" );
			break;
		case 3:
			SET_MODEL( ENT(pev), "models/props/trainjourney/granmodel.mdl" );
			break;
		case 4:
			SET_MODEL( ENT(pev), "models/props/trainjourney/granmodel2.mdl" );
			break;
		case 5:
			SET_MODEL( ENT(pev), "models/props/trainjourney/tall.mdl" );
			break;
		}
	}


	switch ( pev->iuser1 )
	{
	case 0:
		pev->iuser2 = pev->iuser3 + pev->origin.x; break;
	case 1:
		pev->iuser2 = pev->iuser3 + pev->origin.y; break;
	case 2:
		pev->iuser2 = pev->iuser3 + pev->origin.z; break;
	}

	pev->vuser1 = pev->origin;
}

void CCofTrainProp::Precache( void )
{
	if ( pev->spawnflags & 1 )
	{
		PRECACHE_MODEL( "models/props/trainjourney/train_hill.mdl" );
	}
	else
	{
		PRECACHE_MODEL( "models/props/trainjourney/buske.mdl" );
		PRECACHE_MODEL( "models/props/trainjourney/buske2.mdl" );
		PRECACHE_MODEL( "models/props/trainjourney/buske3.mdl" );
		PRECACHE_MODEL( "models/props/trainjourney/granmodel.mdl" );
		PRECACHE_MODEL( "models/props/trainjourney/granmodel2.mdl" );
		PRECACHE_MODEL( "models/props/trainjourney/tall.mdl" );
	}
}

void CCofTrainProp::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	SetNextThink( 0.1 );
	SetThink( &CCofTrainProp::PropThink );
}

void CCofTrainProp::SwitchModel( void )
{
	switch ( RANDOM_LONG( 0, 5 ) )
	{
	case 0:
		SET_MODEL( ENT(pev), "models/props/trainjourney/buske.mdl" );
		break;
	case 1:
		SET_MODEL( ENT(pev), "models/props/trainjourney/buske2.mdl" );
		break;
	case 2:
		SET_MODEL( ENT(pev), "models/props/trainjourney/buske3.mdl" );
		break;
	case 3:
		SET_MODEL( ENT(pev), "models/props/trainjourney/granmodel.mdl" );
		break;
	case 4:
		SET_MODEL( ENT(pev), "models/props/trainjourney/granmodel2.mdl" );
		break;
	case 5:
		SET_MODEL( ENT(pev), "models/props/trainjourney/tall.mdl" );
		break;
	}
}

void CCofTrainProp::PropThink( void )
{




	switch ( pev->iuser1 )
	{
	case 0:
		if ( pev->iuser3 > pev->origin.x )
			pev->origin.x += pev->fuser1;


		if ( pev->iuser3 <= pev->origin.x )
		{

			pev->origin = pev->vuser1;

			if ( !pev->spawnflags & 1 )
				SwitchModel();
		}
		break;
	case 1:
		if ( pev->iuser3 > pev->origin.y )
			pev->origin.y += pev->fuser1;


		if ( pev->iuser3 <= pev->origin.y )
		{

			pev->origin = pev->vuser1;

			if ( !pev->spawnflags & 1 )
				SwitchModel();
		}
		break;
	case 2:
		if ( pev->iuser3 > pev->origin.z )
			pev->origin.z += pev->fuser1;


		if ( pev->iuser3 <= pev->origin.z )
		{

			pev->origin = pev->vuser1;

			if ( !pev->spawnflags & 1 )
				SwitchModel();
		}
		break;
	}





	SetNextThink( 0 );
}

//=========================================================
// cof_triggeron
//=========================================================
class CCofTriggerOn : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_triggeron, CCofTriggerOn );

void CCofTriggerOn::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofTriggerOn::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pTtrigger = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );

	if ( pTtrigger )
	{
		if ( FClassnameIs( pTtrigger->pev, "trigger_once" ) || FClassnameIs( pTtrigger->pev, "trigger_multiple" ) )
		{
			pTtrigger->pev->iuser1 = 0;
		}
	}
}

//=========================================================
// cof_hint
//=========================================================
class CCofShowHint : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_hint, CCofShowHint );

void CCofShowHint::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofShowHint::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer && pev->message )
	{
		if ( CVAR_GET_FLOAT( "sv_disablehints" ) == 0 )
			pPlayer->ShowHint( pev->message, false );
	}
}

//=========================================================
// cof_yesno
//=========================================================
LINK_ENTITY_TO_CLASS( cof_yesno, CCofYesNo );

TYPEDESCRIPTION	CCofYesNo::m_SaveData[] =
{
	DEFINE_FIELD( CCofYesNo, m_cvarvalue, FIELD_INTEGER ),
	DEFINE_FIELD( CCofYesNo, m_sCvar, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_sWeapon, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_sItem, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_sNotHaveWeaponMessage, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_sNoTrigger, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_sCvarNo, FIELD_STRING ),
	DEFINE_FIELD( CCofYesNo, m_cvarvalueno, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCofYesNo, CBaseEntity );

void CCofYesNo::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "cvarset" ) )
	{
		m_sCvar = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cvarvalue" ) )
	{
		m_cvarvalue = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cvarsetno" ) )
	{
		m_sCvarNo = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cvarvalueno" ) )
	{
		m_cvarvalueno = atoi( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "weaponneed" ) )
	{
		m_sWeapon = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "itemneed" ) )
	{
		m_sItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "noitemmsg" ) )
	{
		m_sNotHaveWeaponMessage = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "notrigger" ) )
	{
		m_sNoTrigger = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofYesNo::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;

	if ( !pev->message )
		ALERT( at_console, "YesNo error! YesNo with no tga at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
}

void CCofYesNo::YesActivate( CBaseEntity *pActivator )
{
	if ( !pActivator )
		return;

	if ( pActivator->IsPlayer() )
	{
		if ( !strcmp( STRING(pev->targetname), "ryesno" ) )
			GetClassPtr( (CBasePlayer *)pActivator->pev )->pev->fuser3 = 0;
	}





	if ( pev->spawnflags & 1 )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			if ( m_sWeapon || m_sItem )
			{

				if ( m_sWeapon )
				{
					if ( pPlayer->HasNamedPlayerItem( STRING(m_sWeapon) ) )
					{
					}
				}

				if ( m_sItem )
				{
					pPlayer->HasItemInSlot( (char *)STRING(m_sItem), true );
				}
			}
		}
	}

	if ( m_sCvar )
		CVAR_SET_FLOAT( STRING(m_sCvar), m_cvarvalue );

	if ( pev->target )
		FireTargets( STRING(pev->target), pActivator, pActivator, USE_TOGGLE, 0 );
}

void CCofYesNo::NoActivate( CBaseEntity *pActivator )
{
	if ( !pActivator )
		return;



	if ( pActivator->IsPlayer() )
	{
		if ( !strcmp( STRING(pev->targetname), "ryesno" ) )
			GetClassPtr( (CBasePlayer *)pActivator->pev )->pev->fuser3 = 0;
	}

	if ( m_sCvarNo )
		CVAR_SET_FLOAT( STRING(m_sCvarNo), m_cvarvalueno );

	if ( m_sNoTrigger )
		FireTargets( STRING(m_sNoTrigger), pActivator, pActivator, USE_TOGGLE, 0 );
}

void CCofYesNo::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	if ( !pev->message )
	{
		ALERT( at_console, "YesNo error! YesNo with no tga at %f %f %f!\n", pev->origin.x, pev->origin.y, pev->origin.z );
		return;
	}

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{

		if ( !strcmp( STRING(pev->targetname), "ryesno" ) )
		{
			pPlayer->pev->velocity.x = 0;
			pPlayer->pev->velocity.y = 0;
			pPlayer->pev->fuser3 = 1;
		}

		if ( m_sWeapon || m_sItem )
		{

			if ( m_sWeapon )
			{
				if ( !pPlayer->HasNamedPlayerItem( STRING(m_sWeapon) ) )
				{
					if ( m_sNotHaveWeaponMessage )
					{
						UTIL_ProMessage( pPlayer, STRING(m_sNotHaveWeaponMessage), 5, false );
						return;
					}

					return;
				}
			}



			if ( m_sItem )
			{
				if ( !pPlayer->HasItemInSlot( (char *)STRING(m_sItem), false ) )
				{
					if ( m_sNotHaveWeaponMessage )
					{
						UTIL_ProMessage( pPlayer, STRING(m_sNotHaveWeaponMessage), 5, false );
						return;
					}

					return;
				}
			}
		}




		MESSAGE_BEGIN( MSG_ONE, gmsgYesNo, NULL, pPlayer->pev );
			WRITE_STRING( STRING(pev->message) );
			WRITE_SHORT( ENTINDEX( edict() ) );
		MESSAGE_END();
	}
}

//=========================================================
// cof_stresstest
//=========================================================
LINK_ENTITY_TO_CLASS( cof_stresstest, CCofStressTest );

void CCofStressTest::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;

	currenteffect = 0;
}



void CCofStressTest::StartNextEffect( void )
{
	startfps = CVAR_GET_FLOAT( "fps_current" );

	currenteffect++;

	switch ( currenteffect )
	{
	case 1:
		SetNextThink( 0.1 );
		SetThink( &CCofStressTest::TurnOnBumpMaps );
		UTIL_CenterPrintAll( "Enabling bump maps..." );
		break;
	case 2:
		SetNextThink( 0.1 );
		SetThink( &CCofStressTest::TurnOnWater );
		UTIL_CenterPrintAll( "Enabling water effects..." );
		break;
	case 3:
		SetNextThink( 0.1 );
		SetThink( &CCofStressTest::TurnOnDynLight );
		UTIL_CenterPrintAll( "Enabling dynamic lighting..." );
		break;
	case 4:
		SetNextThink( 0.1 );
		SetThink( &CCofStressTest::TurnOnWeather );
		UTIL_CenterPrintAll( "Enabling weather effects..." );
		break;
	}



	if ( currenteffect > 4 )
	{
		UTIL_CenterPrintAll( "Stress test finished. Disconnecting." );
		SetNextThink( 3 );
		SetThink( &CCofStressTest::Disconnect );
	}
}

void CCofStressTest::AllowToContinue( void )
{
	SetNextThink( 3 );
	SetThink( &CCofStressTest::StartNextEffect );
}

void CCofStressTest::Disconnect( void )
{
	(*g_engfuncs.pfnEndSection)( "_oem_end_training" );
}

void CCofStressTest::TurnOnBumpMaps( void )
{
	CVAR_SET_FLOAT( "gl_bump", 1 );
	SetNextThink( 5 );
	SetThink( &CCofStressTest::ReportEffect );
}

void CCofStressTest::TurnOnWater( void )
{
	CVAR_SET_FLOAT( "cg_water", 1 );
	SetNextThink( 5 );
	SetThink( &CCofStressTest::ReportEffect );
}

void CCofStressTest::TurnOnDynLight( void )
{
	CVAR_SET_FLOAT( "gl_dynlight", 1 );
	SetNextThink( 5 );
	SetThink( &CCofStressTest::ReportEffect );
}

void CCofStressTest::TurnOnWeather( void )
{
	CVAR_SET_FLOAT( "r_weather", 1 );
	SetNextThink( 5 );
	SetThink( &CCofStressTest::ReportEffect );
}

void CCofStressTest::ReportEffect( void )
{
	currentfps = CVAR_GET_FLOAT( "fps_current" );

	bool bad = false;

	if ( currentfps < startfps )
	{
		int amount = startfps - currentfps;

		if ( amount >= 5 )
			bad = true;
		else
			bad = false;
	}

	CBaseEntity *pTarget = NULL;
	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );


	if ( !pPlayer )
		return;

	switch ( currenteffect )
	{
	case 1:
		pTarget = UTIL_FindEntityByTargetname( pTarget, "stressbump" );

		if ( pTarget )
		{
			if ( bad )
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_bump_bad.tga" );
			else
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_bump_good.tga" );

			pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
		break;
	case 2:
		pTarget = UTIL_FindEntityByTargetname( pTarget, "stresswater" );

		if ( pTarget )
		{
			if ( bad )
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_water_bad.tga" );
			else
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_water_good.tga" );

			pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
		break;
	case 3:
		pTarget = UTIL_FindEntityByTargetname( pTarget, "stresslight" );

		if ( pTarget )
		{
			if ( bad )
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_lights_bad.tga" );
			else
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_lights_good.tga" );

			pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
		break;
	case 4:
		pTarget = UTIL_FindEntityByTargetname( pTarget, "stressweather" );

		if ( pTarget )
		{
			if ( bad )
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_weather_bad.tga" );
			else
				pTarget->pev->message = MAKE_STRING( "gfx/vgui/stress/stress_weather_good.tga" );

			pTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
		}
		break;
	}
}

void CCofStressTest::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;


	currenteffect = 0;
	SetNextThink( 5 );
	SetThink( &CCofStressTest::StartNextEffect );
}

//=========================================================
// cof_computer
//=========================================================
LINK_ENTITY_TO_CLASS( cof_computer, CCofComputer );

TYPEDESCRIPTION	CCofComputer::m_SaveData[] =
{
	DEFINE_ARRAY( CCofComputer, m_sUserName, FIELD_CHARACTER, 64 ),
	DEFINE_ARRAY( CCofComputer, m_sPassword, FIELD_CHARACTER, 64 ),
};

IMPLEMENT_SAVERESTORE( CCofComputer, CBaseEntity );

const char *CCofComputer::pUsernames[] =
{
	NULL,
	"davidspc",
	"jillusername",
	"excelsior",
	"dorrarpc",
	"confuseduser",
	"leatherhoff45",
	"ea45",
	"dorrardavid",
};

const char *CCofComputer::pPasswords[] =
{
	NULL,
	"apple45",
	"cloudyday",
	"rosesarered",
	"junesummer",
	"unforgettable",
	"thebeatles54",
	"marriageinapril",
	"jillanddavid",
};

void CCofComputer::Spawn( void )
{
	Precache();

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	pev->effects = 0;
	pev->frame = 0;

	SET_MODEL( ENT(pev), "models/props/inomhusd/cafe_monitor.mdl" );

	if ( pev->iuser1 )
		pev->body = 2;
	else
		pev->body = 1;


	pev->renderfx = 100;


	pev->iuser2 = RANDOM_LONG( 1, 8 );
	pev->iuser3 = RANDOM_LONG( 1, 8 );


	pev->fuser1 = 0;

	strncpy( m_sUserName, pUsernames[pev->iuser2], 64 );
	strncpy( m_sPassword, pPasswords[pev->iuser3], 64 );
}

void CCofComputer::Precache( void )
{
	PRECACHE_SOUND( "buttons/keyboard_type.wav" );
	PRECACHE_SOUND( "buttons/computer_logon.wav" );
	PRECACHE_SOUND( "buttons/computer_deny.wav" );

	PRECACHE_MODEL( "models/props/inomhusd/cafe_monitor.mdl" );
}

void CCofComputer::ComputerCheck( CBaseEntity *pUser, char *username, char *password )
{
	int correctuser = 0;
	int correctpass = 0;



	if ( !strcmp( username, m_sUserName ) )
		correctuser = 1;

	if ( !strcmp( password, m_sPassword ) )
		correctpass = 1;

	if ( correctuser && correctpass )
	{
		if ( pev->iuser1 == 0 )
		{
			pev->iuser1 = 1;

			if ( pev->target )
				FireTargets( STRING(pev->target), pUser, this, USE_TOGGLE, 0 );

			if ( pUser->IsPlayer() )
			{
				CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pUser->pev );

				if ( pPlayer )
				{
					char szNumber[16];
					sprintf( szNumber, "%i%i%i%i", pPlayer->m_iPadlockNumber[1], pPlayer->m_iPadlockNumber[2], pPlayer->m_iPadlockNumber[3], pPlayer->m_iPadlockNumber[4] );
					MESSAGE_BEGIN( MSG_ONE, gmsgComputer, NULL, pUser->pev );
						WRITE_BYTE( pev->iuser1 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( pev->fuser1 );
						WRITE_STRING( szNumber );
					MESSAGE_END();


					for ( int i = 1; i <= 16; i++ )
					{

						if ( strlen( pPlayer->documentsstruct[i].note1 ) < 1 )
						{
							strcpy( pPlayer->documentsstruct[i].note1, "nerds" );
							strcpy( pPlayer->documentsstruct[i].page1, "nerds" );
							strcpy( pPlayer->documentsstruct[i].turnsound, "nerds" );
							strcpy( pPlayer->documentsstruct[i].title, "nerds" );
							strcpy( pPlayer->documentsstruct[i].label, "nerds" );

							MESSAGE_BEGIN( MSG_ONE, gmsgAddNote, NULL, pPlayer->pev );
								WRITE_BYTE( i );
								WRITE_BYTE( 1 );
								WRITE_SHORT( atoi( szNumber ) );
								WRITE_BYTE( 1 );
								WRITE_STRING( pPlayer->documentsstruct[i].note1 );
								WRITE_STRING( pPlayer->documentsstruct[i].note2 );
								WRITE_STRING( pPlayer->documentsstruct[i].note3 );
								WRITE_STRING( pPlayer->documentsstruct[i].note4 );
								WRITE_STRING( pPlayer->documentsstruct[i].page1 );
								WRITE_STRING( pPlayer->documentsstruct[i].page2 );
								WRITE_STRING( pPlayer->documentsstruct[i].page3 );
								WRITE_STRING( pPlayer->documentsstruct[i].page4 );
								WRITE_STRING( pPlayer->documentsstruct[i].turnsound );
								WRITE_STRING( pPlayer->documentsstruct[i].title );
								WRITE_STRING( pPlayer->documentsstruct[i].label );
							MESSAGE_END();

							break;
						}
					}

					pPlayer->m_iUsingComputerOrPhone = 1;
				}
			}



			if ( pev->fuser1 == 0 )
				pev->fuser1 = 1;
		}
	}
	else
	{
		pev->iuser1 = 0;

		int value1 = 0;
		int value2 = 0;

		if ( correctuser )
			value1 = 0;
		else
			value1 = 1;

		if ( correctpass )
			value2 = 0;
		else
			value2 = 1;

		if ( pUser->IsPlayer() )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pUser->pev );

			if ( pPlayer )
			{

				char szNumber[16];
				sprintf( szNumber, "%i%i%i%i", pPlayer->m_iPadlockNumber[1], pPlayer->m_iPadlockNumber[2], pPlayer->m_iPadlockNumber[3], pPlayer->m_iPadlockNumber[4] );
				MESSAGE_BEGIN( MSG_ONE, gmsgComputer, NULL, pUser->pev );
					WRITE_BYTE( pev->iuser1 );
					WRITE_BYTE( value1 );
					WRITE_BYTE( value2 );
					WRITE_BYTE( 0 );
					WRITE_STRING( szNumber );
				MESSAGE_END();

				pPlayer->m_iUsingComputerOrPhone = 1;
			}
		}
	}




	if ( pev->iuser1 )
		pev->body = 2;
	else
		pev->body = 1;
}

void CCofComputer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{

		char szNumber[16];
		sprintf( szNumber, "%i%i%i%i", pPlayer->m_iPadlockNumber[1], pPlayer->m_iPadlockNumber[2], pPlayer->m_iPadlockNumber[3], pPlayer->m_iPadlockNumber[4] );
		MESSAGE_BEGIN( MSG_ONE, gmsgComputer, NULL, pPlayer->pev );
			WRITE_BYTE( pev->iuser1 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
			WRITE_STRING( szNumber );
		MESSAGE_END();

		pPlayer->m_iUsingComputerOrPhone = 1;
	}
}

//=========================================================
// cof_playerbreathetoggle
//=========================================================
LINK_ENTITY_TO_CLASS( cof_playerbreathetoggle, CCofBreatheToggle );

void CCofBreatheToggle::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofBreatheToggle::Precache( void )
{
	PRECACHE_SOUND( "player/vocal/breathing/breathing_1.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/breathing_2.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/breathing_3.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/breathing_4.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/breathing_5.wav" );
	PRECACHE_SOUND( "player/vocal/breathing/breathing_6.wav" );
}

void CCofBreatheToggle::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pev->frags != 0 )
		{
			pPlayer->m_bScaredBreathing = true;
			pPlayer->m_fNextScaredBreathingTime = gpGlobals->time;
		}
		else
		{
			pPlayer->m_bScaredBreathing = false;
			pPlayer->m_fNextScaredBreathingTime = 0;
		}
	}
}

//=========================================================
// cof_camerazoom
//=========================================================
class CCofCameraZoom : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	void EXPORT ReturnFOV( void );
};

LINK_ENTITY_TO_CLASS( cof_camerazoom, CCofCameraZoom );

void CCofCameraZoom::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;

	if ( pev->fuser1 == 110 )
		pev->fuser1 = 111;
}

void CCofCameraZoom::ReturnFOV( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *plr = UTIL_PlayerByIndex( i );

		if ( plr )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)plr->pev );

			if ( pPlayer )
			{
				if ( pev->spawnflags & 1 )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pPlayer->pev );
						WRITE_COORD( pev->fuser2 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
					MESSAGE_END();

					pPlayer->pev->vuser2.x = 90;
				}
				else
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pPlayer->pev );
						WRITE_COORD( 666 );
						WRITE_BYTE( 0 );
						WRITE_BYTE( 0 );
					MESSAGE_END();

					pPlayer->pev->vuser2.x = 90;
				}
			}
		}
	}
}

void CCofCameraZoom::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "zoomfov" ) )
	{
		pev->fuser1 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "zoomtime" ) )
	{
		pev->fuser2 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "returntime" ) )
	{
		pev->fuser3 = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofCameraZoom::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pPlayer->pev );
			WRITE_COORD( pev->fuser2 );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 0 );
		MESSAGE_END();

		pPlayer->pev->vuser2.x = pev->fuser1;

		SetNextThink( pev->fuser3 );
		SetThink( &CCofCameraZoom::ReturnFOV );
	}
}

//=========================================================
// cof_skipmanager
//=========================================================
class CCofSkipManager : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	int m_Entity;
	int m_aWeapon;
	int m_bWeapon;
	int m_cWeapon;
	int m_aItem;
	int m_bItem;
	int m_cItem;
	int m_Teleport;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

TYPEDESCRIPTION	CCofSkipManager::m_SaveData[] =
{
	DEFINE_FIELD( CCofSkipManager, m_Entity, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_aWeapon, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_bWeapon, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_cWeapon, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_aItem, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_bItem, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_cItem, FIELD_STRING ),
	DEFINE_FIELD( CCofSkipManager, m_Teleport, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCofSkipManager, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_skipmanager, CCofSkipManager );

void CCofSkipManager::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofSkipManager::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "entity" ) )
	{
		m_Entity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "aweapon" ) )
	{
		m_aWeapon = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bweapon" ) )
	{
		m_bWeapon = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "cweapon" ) )
	{
		m_cWeapon = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "aitem" ) )
	{
		m_aItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "bitem" ) )
	{
		m_bItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "citem" ) )
	{
		m_cItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "teleport" ) )
	{
		m_cItem = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofSkipManager::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = (CBasePlayer *)pActivator;

	if ( !pPlayer )
		return;




	if ( m_Teleport )
	{
		CBaseEntity *pTriggerTarget = UTIL_FindEntityByTargetname( NULL, STRING(m_Teleport) );

		if ( pTriggerTarget )
			pTriggerTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
	}




	if ( m_Entity )
	{
		CBaseEntity *pTriggerTarget = UTIL_FindEntityByTargetname( NULL, STRING(m_Entity) );

		if ( pTriggerTarget )
			pTriggerTarget->Use( pPlayer, pPlayer, USE_TOGGLE, 0 );
	}




	if ( m_aWeapon || m_bWeapon || m_cWeapon )
	{
		if ( m_aWeapon )
			pPlayer->GiveNamedItem( STRING(m_aWeapon), false, false );

		if ( m_bWeapon )
			pPlayer->GiveNamedItem( STRING(m_bWeapon), false, false );

		if ( m_cWeapon )
			pPlayer->GiveNamedItem( STRING(m_cWeapon), false, false );
	}




	if ( m_aItem )
	{
		if ( pPlayer )
		{
			if ( pPlayer->AddItemToInventory( m_aItem, 0, 0 ) )
			{
				return;
			}
			else
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );


				CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", m_aItem, pPlayer->pev->origin, pPlayer->pev->angles, pPlayer->edict(), 0 );

				if ( pKey )
				{
					pKey->pev->message = m_aItem;
					pKey->pev->angles.x = 0;
					pKey->pev->angles.z = 0;
				}
			}
		}
	}




	if ( m_bItem )
	{
		if ( pPlayer )
		{
			if ( pPlayer->AddItemToInventory( m_bItem, 0, 0 ) )
			{
				return;
			}
			else
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );


				CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", m_bItem, pPlayer->pev->origin, pPlayer->pev->angles, pPlayer->edict(), 0 );

				if ( pKey )
				{
					pKey->pev->message = m_bItem;
					pKey->pev->angles.x = 0;
					pKey->pev->angles.z = 0;
				}
			}
		}
	}




	if ( m_cItem )
	{
		if ( pPlayer )
		{
			if ( pPlayer->AddItemToInventory( m_cItem, 0, 0 ) )
			{
				return;
			}
			else
			{
				UTIL_ProMessage( pPlayer, "Inventory full", 2, false );


				CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", m_cItem, pPlayer->pev->origin, pPlayer->pev->angles, pPlayer->edict(), 0 );

				if ( pKey )
				{
					pKey->pev->message = m_cItem;
					pKey->pev->angles.x = 0;
					pKey->pev->angles.z = 0;
				}
			}
		}
	}
}

//=========================================================
// cof_weaponswitch
//=========================================================
class CCofWeaponSwitch : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_weaponswitch, CCofWeaponSwitch );

void CCofWeaponSwitch::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofWeaponSwitch::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pev->message )
		{
			if ( pPlayer->HasNamedPlayerItem( STRING(pev->message) ) )
				pPlayer->SelectItem( STRING(pev->message) );
		}
	}
}

//=========================================================
// cof_leveltrigger
//=========================================================
class CCofLevelTrigger : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	string_t m_sMap;
};

LINK_ENTITY_TO_CLASS( cof_leveltrigger, CCofLevelTrigger );

TYPEDESCRIPTION	CCofLevelTrigger::m_SaveData[] =
{
	DEFINE_FIELD( CCofLevelTrigger, m_sMap, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCofLevelTrigger, CBaseEntity );

void CCofLevelTrigger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "map" ) )
	{
		m_sMap = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofLevelTrigger::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofLevelTrigger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->message )
		CVAR_SET_STRING( "map_trigger_ent", STRING(pev->message) );

	if ( m_sMap )
		CVAR_SET_STRING( "map_trigger_ent_map", STRING(m_sMap) );
}

//=========================================================
// cof_phonedisable
//=========================================================
class CCofPhoneDisable : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_phonedisable, CCofPhoneDisable );

void CCofPhoneDisable::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofPhoneDisable::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			for ( int i = 0; i < MAX_ITEM_TYPES; i++ )
			{
				if ( pPlayer->m_rgpPlayerItems[i] )
				{
					CBasePlayerItem *pPlayerItem = pPlayer->m_rgpPlayerItems[i];

					while ( pPlayerItem )
					{
						if ( FClassnameIs( pPlayerItem->edict(), "weapon_mobile" ) )
						{
							CMobile *pMobile = GetClassPtr( (CMobile *)pPlayerItem->pev );

							if ( pMobile )
							{
								pMobile->m_bDisabled = true;
								pPlayer->m_iPhoneDisabled = 1;

								pMobile->SetBodygroup( 1, 7 );
								pMobile->SetBodygroup( 2, 1 );
								pMobile->SendWeaponBody( pPlayer, pMobile->pev->body );

								pPlayer->m_iPhoneMessage = pMobile->pev->body;
							}
						}















						pPlayerItem = pPlayerItem->m_pNext;
					}
				}
			}
		}
	}
}

//=========================================================
// cof_goodpoints
//=========================================================
class CCofGoodPoints : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_goodpoints, CCofGoodPoints );

void CCofGoodPoints::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofGoodPoints::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pev->impulse )
			pPlayer->AddGoodPoint( 1, false );
		else
			pPlayer->AddGoodPoint( 1, true );
	}
}

//=========================================================
// cof_goodpointstrigger
//=========================================================
class CCofGoodPointsTrigger : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_Trigger1;
	int m_Trigger2;
	int m_Trigger3;
	int m_Trigger4;
	int m_Trigger5;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void KeyValue( KeyValueData *pkvd );
};

TYPEDESCRIPTION	CCofGoodPointsTrigger::m_SaveData[] =
{
	DEFINE_FIELD( CCofGoodPointsTrigger, m_Trigger1, FIELD_STRING ),
	DEFINE_FIELD( CCofGoodPointsTrigger, m_Trigger2, FIELD_STRING ),
	DEFINE_FIELD( CCofGoodPointsTrigger, m_Trigger3, FIELD_STRING ),
	DEFINE_FIELD( CCofGoodPointsTrigger, m_Trigger4, FIELD_STRING ),
	DEFINE_FIELD( CCofGoodPointsTrigger, m_Trigger5, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCofGoodPointsTrigger, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_goodpointstrigger, CCofGoodPointsTrigger );

void CCofGoodPointsTrigger::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofGoodPointsTrigger::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "triggerone" ) )
	{
		m_Trigger1 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggertwo" ) )
	{
		m_Trigger2 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerthree" ) )
	{
		m_Trigger3 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerfour" ) )
	{
		m_Trigger4 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "triggerfive" ) )
	{
		m_Trigger5 = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofGoodPointsTrigger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		int points = pPlayer->m_iGoodPoints;


		if ( m_Trigger5 )
		{
			if ( points >= 5 )
			{
				FireTargets( STRING(m_Trigger5), pPlayer, pPlayer, USE_TOGGLE, 0 );
				return;
			}
		}


		if ( m_Trigger4 )
		{
			if ( points == 4 )
			{
				FireTargets( STRING(m_Trigger4), pPlayer, pPlayer, USE_TOGGLE, 0 );
				return;
			}
		}


		if ( m_Trigger3 )
		{
			if ( points == 3 )
			{
				FireTargets( STRING(m_Trigger3), pPlayer, pPlayer, USE_TOGGLE, 0 );
				return;
			}
		}


		if ( m_Trigger2 )
		{
			if ( points == 2 )
			{
				FireTargets( STRING(m_Trigger2), pPlayer, pPlayer, USE_TOGGLE, 0 );
				return;
			}
		}


		if ( m_Trigger1 )
		{
			if ( points <= 1 )
			{
				FireTargets( STRING(m_Trigger1), pPlayer, pPlayer, USE_TOGGLE, 0 );
			}
		}
	}
}

//=========================================================
// cof_bosshealthbar
//=========================================================
class CCofBossHealthBar : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	int m_TGAEmpty;
	int m_TGAFull;
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
	virtual void KeyValue( KeyValueData *pkvd );
};

TYPEDESCRIPTION	CCofBossHealthBar::m_SaveData[] =
{
	DEFINE_FIELD( CCofBossHealthBar, m_TGAEmpty, FIELD_STRING ),
	DEFINE_FIELD( CCofBossHealthBar, m_TGAFull, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCofBossHealthBar, CBaseEntity );

LINK_ENTITY_TO_CLASS( cof_bosshealthbar, CCofBossHealthBar );

void CCofBossHealthBar::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofBossHealthBar::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "emptytga" ) )
	{
		m_TGAEmpty = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fulltga" ) )
	{
		m_TGAFull = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCofBossHealthBar::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;


	pev->iuser1 = 1;



	if ( m_TGAEmpty && m_TGAFull && pev->message )
	{



		CBaseEntity *pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );

		if ( pTarget && pTarget->IsAlive() )
		{



			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && value != 666 )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( pPlayer )
					{



						pTarget->m_bIsBoss = true;
						pPlayer->m_bBossHealthBar = true;
						pPlayer->m_sBossBarEmpty = m_TGAEmpty;
						pPlayer->m_sBossBarFull = m_TGAFull;
						pPlayer->m_iBossMaxHealth = pTarget->pev->max_health;
						pPlayer->m_iBossCurrentHealth = pTarget->pev->health;
						pPlayer->m_sBossName = pTarget->pev->targetname;

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
			else
			{
				CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

				if ( pPlayer )
				{



					pTarget->m_bIsBoss = true;
					pPlayer->m_bBossHealthBar = true;
					pPlayer->m_sBossBarEmpty = m_TGAEmpty;
					pPlayer->m_sBossBarFull = m_TGAFull;
					pPlayer->m_iBossMaxHealth = pTarget->pev->max_health;
					pPlayer->m_iBossCurrentHealth = pTarget->pev->health;
					pPlayer->m_sBossName = pTarget->pev->targetname;

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
// cof_weapontrigger
//=========================================================
class CCofWeaponTrigger : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_weapontrigger, CCofWeaponTrigger );

void CCofWeaponTrigger::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofWeaponTrigger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;


	if ( !pev->message || !pev->noise || !pev->target )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pPlayer->HasNamedPlayerItem( STRING(pev->message) ) )
			FireTargets( STRING(pev->target), pPlayer, pPlayer, USE_TOGGLE, 0 );
		else
			FireTargets( STRING(pev->noise), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}
}

//=========================================================
// cof_maxhealthchange
//=========================================================
class CCofMaxHealthChange : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_maxhealthchange, CCofMaxHealthChange );

void CCofMaxHealthChange::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofMaxHealthChange::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		switch ( pev->impulse )
		{
		case 0:
			pPlayer->m_iHealthCap = 0;

			MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, pPlayer->pev );
				WRITE_BYTE( pPlayer->m_iHealthCap );
			MESSAGE_END();
			break;
		case 1:
			pPlayer->m_iHealthCap = 1;

			MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, pPlayer->pev );
				WRITE_BYTE( pPlayer->m_iHealthCap );
			MESSAGE_END();
			break;
		case 2:
			pPlayer->m_iHealthCap = 2;

			MESSAGE_BEGIN( MSG_ONE, gmsgHealthCap, NULL, pPlayer->pev );
				WRITE_BYTE( pPlayer->m_iHealthCap );
			MESSAGE_END();
			break;
		}
	}
}

//=========================================================
// drip
//=========================================================
class CDrip : public CBaseEntity
{
public:
	virtual void Spawn( void );
	static void Drop( Vector vecStart, Vector vecVelocity );
	virtual void Touch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( drip, CDrip );

void CDrip::Spawn( void )
{
	pev->movetype = MOVETYPE_FLY;
	pev->classname = MAKE_STRING( "drip" );

	pev->solid = SOLID_BBOX;

	SET_MODEL( ENT(pev), "models/props/blandat/drips.mdl" );
	pev->rendermode = kRenderTransAdd;
	pev->renderamt = 255;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;

	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );
}

void CDrip::Drop( Vector vecStart, Vector vecVelocity )
{
	CDrip *pDrip = GetClassPtr( (CDrip *)NULL );
	pDrip->Spawn();

	UTIL_SetOrigin( pDrip, vecStart );
	pDrip->pev->velocity = vecVelocity;
}

void CDrip::Touch( CBaseEntity *pOther )
{



	int iPitch = RANDOM_FLOAT( 90, 110 );
	EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "ambience/drip.wav", 1, ATTN_IDLE, 0, iPitch );


	SetThink( &CDrip::SUB_Remove );
	pev->nextthink = gpGlobals->time;
}

//=========================================================
// dripper
//=========================================================
class CDripper : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Think( void );
};

void CDripper::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	SetNextThink( RANDOM_LONG( 1, 3 ) );

	if ( pev->fuser1 == 0 )
		pev->fuser1 = 250;
}

void CDripper::Precache( void )
{
	UTIL_PrecacheOther( "drip" );
	PRECACHE_MODEL( "models/props/blandat/drips.mdl" );
	PRECACHE_SOUND( "ambience/drip.wav" );
}

void CDripper::Think( void )
{
	CDrip::Drop( pev->origin, Vector( 0, 0, pev->fuser1 ) );
	SetNextThink( RANDOM_FLOAT( 1, 3 ) );
}

LINK_ENTITY_TO_CLASS( dripper, CDripper );

//=========================================================
// cof_closeallvgui
//=========================================================
class CCoFCloseVGUI : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFCloseVGUI::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFCloseVGUI::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
		pPlayer->ShowVGUIMenu( 200 );
}

LINK_ENTITY_TO_CLASS( cof_closeallvgui, CCoFCloseVGUI );

//=========================================================
// cof_chapter
//=========================================================
class CCofChapter : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Precache( void );
};

void CCofChapter::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCofChapter::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CCofChapter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			if ( pev->message )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgChapter, NULL, pPlayer->pev );
					WRITE_STRING( STRING(pev->message) );

					if ( pev->noise )
						WRITE_STRING( STRING(pev->noise) );
					else
						WRITE_STRING( NULL );

				MESSAGE_END();
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_chapter, CCofChapter );

//=========================================================
// cof_killplayer
//=========================================================
class CCoFKillPlayer : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFKillPlayer::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFKillPlayer::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			if ( pev->spawnflags & 1 )
				plr->m_iDeathBits |= 1;

			if ( pev->spawnflags & 2 )
				plr->m_iDeathBits |= 2;

			if ( pev->spawnflags & 4 )
				plr->m_iDeathBits |= 4;

			if ( pev->spawnflags & 8 )
				plr->m_iDeathBits |= 8;

			if ( pev->spawnflags & 16 )
				plr->m_iDeathBits |= 16;

			if ( pev->spawnflags & 32 )
				plr->m_iDeathBits |= 32;

			plr->pev->health = 0;
			plr->Killed( plr->pev, GIB_NEVER );
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_killplayer, CCoFKillPlayer );

//=========================================================
// cof_doctorweaponset
//=========================================================
class CCoFDoctorWeaponSet : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFDoctorWeaponSet::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFDoctorWeaponSet::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_iDoctorWeapon = pev->iuser1;
}

LINK_ENTITY_TO_CLASS( cof_doctorweaponset, CCoFDoctorWeaponSet );

//=========================================================
// cof_doctorweapontrigger
//=========================================================
class CCoFDoctorWeaponTrigger : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFDoctorWeaponTrigger::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFDoctorWeaponTrigger::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		int value = GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_iDoctorWeapon;

		if ( value == 1 )
		{
			if ( pev->target )
				FireTargets( STRING(pev->target), pPlayer, this, USE_TOGGLE, 0 );
		}

		if ( value == 2 )
		{
			if ( pev->message )
				FireTargets( STRING(pev->message), pPlayer, this, USE_TOGGLE, 0 );
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_doctorweapontrigger, CCoFDoctorWeaponTrigger );

//=========================================================
// cof_lookat
//=========================================================
class CCoFLookAt : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFLookAt::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFLookAt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_vTurnVector = pev->origin;
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_flTurnVectorTime = gpGlobals->time + pev->fuser1;
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->ViewLock( true, pev->fuser1 );
		GetClassPtr( (CBasePlayer *)pPlayer->pev )->m_flTurnAroundTime = gpGlobals->time + 5;
	}
}

LINK_ENTITY_TO_CLASS( cof_lookat, CCoFLookAt );

//=========================================================
// cof_ladderclimb
//=========================================================
class CCoFLadderClimb : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFLadderClimb::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFLadderClimb::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{




		UTIL_SetOrigin( this, pev->origin );

		pPlayer->EnableControl( FALSE );
		pPlayer->pev->flags |= FL_NOTARGET;

		pPlayer->pDeathScene = (CBaseAnimating *)CBaseEntity::Create( "camera_model", pev->origin, pev->angles, pPlayer->edict() );

		if ( pPlayer->pDeathScene )
		{
			UTIL_SetOrigin( pPlayer->pDeathScene, Vector( pev->origin.x, pev->origin.y, pev->origin.z ) );
			SET_MODEL( ENT(pPlayer->pDeathScene->pev), "models/death_scene.mdl" );

			if ( pev->iuser1 == 1 )
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "ladder_climb_down" );
			else
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( "ladder_climb" );

			pPlayer->m_pAttachedCamera = pPlayer->pDeathScene;

			MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pPlayer->pev );
				WRITE_SHORT( ENTINDEX( pPlayer->m_pAttachedCamera->edict() ) );
				WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_ladderclimb, CCoFLadderClimb );

//=========================================================
// cof_mdlcutscene
//=========================================================
class CCoFMDLCutscene : public CBaseMonster
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	void EXPORT GetTargetEnt( void );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	void EXPORT StartNextAnimation( void );
	virtual int Classify( void );
	static TYPEDESCRIPTION m_SaveData[];
	int m_cAnimation[33];
	int m_iNumAnimations;
	int m_iCurrentAnimation;
};

void CCoFMDLCutscene::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
}

void CCoFMDLCutscene::GetTargetEnt( void )
{
	if ( pev->message )
	{
		CBaseEntity *pEntity = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );

		if ( pEntity )
		{
			pev->euser1 = pEntity->edict();
		}
	}
}

void CCoFMDLCutscene::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	if ( pev->renderfx != 184 )
		pev->renderfx = 70;

	if ( pev->message )
	{
		SetThink( &CCoFMDLCutscene::GetTargetEnt );
		SetNextThink( 2 );
	}


	if ( m_iCurrentAnimation < 1 )
		m_iCurrentAnimation = 1;
}





int CCoFMDLCutscene::Classify( void )
{
	return CLASS_NONE;
}



void CCoFMDLCutscene::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "animation1" ) )
	{
		m_cAnimation[1] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation2" ) )
	{
		m_cAnimation[2] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation3" ) )
	{
		m_cAnimation[3] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation4" ) )
	{
		m_cAnimation[4] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation5" ) )
	{
		m_cAnimation[5] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation6" ) )
	{
		m_cAnimation[6] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation7" ) )
	{
		m_cAnimation[7] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation8" ) )
	{
		m_cAnimation[8] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation9" ) )
	{
		m_cAnimation[9] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation10" ) )
	{
		m_cAnimation[10] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation11" ) )
	{
		m_cAnimation[11] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation12" ) )
	{
		m_cAnimation[12] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation13" ) )
	{
		m_cAnimation[13] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;

	}
	else if ( FStrEq( pkvd->szKeyName, "animation14" ) )
	{
		m_cAnimation[14] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation15" ) )
	{
		m_cAnimation[15] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation16" ) )
	{
		m_cAnimation[16] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation17" ) )
	{
		m_cAnimation[17] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation18" ) )
	{
		m_cAnimation[18] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation19" ) )
	{
		m_cAnimation[19] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation20" ) )
	{
		m_cAnimation[20] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation21" ) )
	{
		m_cAnimation[21] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation22" ) )
	{
		m_cAnimation[22] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation23" ) )
	{
		m_cAnimation[23] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation24" ) )
	{
		m_cAnimation[24] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation25" ) )
	{
		m_cAnimation[25] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation26" ) )
	{
		m_cAnimation[26] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation27" ) )
	{
		m_cAnimation[27] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation28" ) )
	{
		m_cAnimation[28] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation29" ) )
	{
		m_cAnimation[29] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else if ( FStrEq( pkvd->szKeyName, "animation30" ) )
	{
		m_cAnimation[30] = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
		m_iNumAnimations++;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

TYPEDESCRIPTION	CCoFMDLCutscene::m_SaveData[] =
{
	DEFINE_FIELD( CCoFMDLCutscene, m_iNumAnimations, FIELD_INTEGER ),
	DEFINE_ARRAY( CCoFMDLCutscene, m_cAnimation, FIELD_STRING, 33 ),
	DEFINE_FIELD( CCoFMDLCutscene, m_iCurrentAnimation, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CCoFMDLCutscene, CBaseEntity );

void CCoFMDLCutscene::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pev->model )
		return;

	CBasePlayer *pPlayer = NULL;
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
		pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

	if ( !pPlayer )
		return;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pev->iuser4 != 1 )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{


			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				plr->StopWheelchairMode();

				if ( pev->spawnflags & 32 )
					SET_VIEW( plr->edict(), plr->edict() );

				if ( pev->spawnflags & 1 )
					plr->EnableControl( FALSE );

				if ( pev->spawnflags & 2 )
					plr->m_iHideHUD |= HIDEHUD_HEALTH;

				if ( pev->spawnflags & 4 )
					plr->m_DrawBars = 1;

				if ( pev->spawnflags & 64 )
					plr->m_iHideHUD |= HIDEHUD_CUSTOMCROSSHAIR;

				plr->pev->flags |= FL_NOTARGET;

				plr->pDeathScene = (CBaseAnimating *)CBaseEntity::Create( "camera_model", pev->origin, pev->angles, plr->edict() );

				if ( plr->pDeathScene )
				{
					if ( pev->euser1 )
						plr->pDeathScene->pev->euser1 = pev->euser1;

					if ( pev->iuser4 )
						plr->pDeathScene->pev->iuser4 = pev->iuser4;

					if ( pev->renderfx == 184 )
						plr->pDeathScene->pev->renderfx = 184;
					else
						plr->pDeathScene->pev->renderfx = 70;

					UTIL_SetOrigin( plr->pDeathScene, Vector( pev->origin.x, pev->origin.y, pev->origin.z ) );
					SET_MODEL( ENT(plr->pDeathScene->pev), STRING(pev->model) );

					plr->m_pAttachedCamera = plr->pDeathScene;

					MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, plr->pev );
						WRITE_SHORT( ENTINDEX( plr->m_pAttachedCamera->edict() ) );
						WRITE_BYTE( 0 );
					MESSAGE_END();
				}
			}
		}

		pev->owner = pPlayer->edict();

		SetNextThink( 0 );
		SetThink( &CCoFMDLCutscene::StartNextAnimation );
	}
	else
	{
		if ( pPlayer )
		{
			pPlayer->StopWheelchairMode();

			if ( pev->spawnflags & 32 )
				SET_VIEW( pPlayer->edict(), pPlayer->edict() );

			if ( pev->spawnflags & 1 )
				pPlayer->EnableControl( FALSE );

			if ( pev->spawnflags & 2 )
				pPlayer->m_iHideHUD |= HIDEHUD_HEALTH;

			if ( pev->spawnflags & 4 )
				pPlayer->m_DrawBars = 1;

			pPlayer->pev->flags |= FL_NOTARGET;

			if ( pev->spawnflags & 64 )
				pPlayer->m_iHideHUD |= HIDEHUD_CUSTOMCROSSHAIR;

			pPlayer->pDeathScene = (CBaseAnimating *)CBaseEntity::Create( "camera_model", pev->origin, pev->angles, pPlayer->edict() );

			if ( pPlayer->pDeathScene )
			{
				if ( pev->euser1 )
					pPlayer->pDeathScene->pev->euser1 = pev->euser1;

				if ( pev->renderfx == 184 )
					pPlayer->pDeathScene->pev->renderfx = 184;
				else
					pPlayer->pDeathScene->pev->renderfx = 70;

				UTIL_SetOrigin( pPlayer->pDeathScene, Vector( pev->origin.x, pev->origin.y, pev->origin.z ) );
				SET_MODEL( ENT(pPlayer->pDeathScene->pev), STRING(pev->model) );

				pPlayer->m_pAttachedCamera = pPlayer->pDeathScene;

				MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pPlayer->pev );
					WRITE_SHORT( ENTINDEX( pPlayer->m_pAttachedCamera->edict() ) );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				pev->owner = pPlayer->edict();
			}


			SetNextThink( 0 );
			SetThink( &CCoFMDLCutscene::StartNextAnimation );
		}
	}
}





void CCoFMDLCutscene::StartNextAnimation( void )
{
	if ( !pev->owner )
		return;

	CBaseEntity *pEntity = CBaseEntity::Instance( pev->owner );

	if ( !pEntity )
		return;

	bool ismp = false;

	if ( g_pGameRules && g_pGameRules->IsMultiplayer() && pev->iuser4 != 1 )
		ismp = true;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pEntity->pev );

	if ( !pPlayer )
		return;

	if ( !pPlayer->pDeathScene )
		return;

	if ( m_iCurrentAnimation > m_iNumAnimations )
	{
		if ( pev->spawnflags & 16 )
		{
			if ( ismp )
			{
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{


					CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

					if ( plr )
					{
						plr->pDeathScene = NULL;
						plr->m_pAttachedCamera = NULL;
						plr->m_bCoopWaitingForCutsceneToEnd = false;
						plr->m_iHideHUD &= ~HIDEHUD_WEAPONS;

						if ( pev->spawnflags & 1 )
							plr->EnableControl( TRUE );

						if ( pev->spawnflags & 2 )
							plr->m_iHideHUD &= ~HIDEHUD_HEALTH;

						if ( pev->spawnflags & 4 )
							plr->m_DrawBars = 0;

						if ( pev->spawnflags & 64 )
							plr->m_iHideHUD &= ~HIDEHUD_CUSTOMCROSSHAIR;

						plr->pev->flags &= ~FL_NOTARGET;

						UTIL_Remove( plr->pDeathScene );

						MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, plr->pev );
							WRITE_SHORT( 0 );
							WRITE_BYTE( 0 );
						MESSAGE_END();


						MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, plr->pev );
							WRITE_COORD( 50 );
							WRITE_BYTE( 0 );
							WRITE_BYTE( 0 );
						MESSAGE_END();

						plr->pev->vuser2.x = 90;
					}
				}
			}
			else
			{
				pPlayer->pDeathScene = NULL;
				pPlayer->m_pAttachedCamera = NULL;
				pPlayer->m_bCoopWaitingForCutsceneToEnd = false;
				pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;

				if ( pev->spawnflags & 1 )
					pPlayer->EnableControl( TRUE );

				if ( pev->spawnflags & 2 )
					pPlayer->m_iHideHUD &= ~HIDEHUD_HEALTH;

				if ( pev->spawnflags & 4 )
					pPlayer->m_DrawBars = 0;

				pPlayer->pev->flags &= ~FL_NOTARGET;

				if ( pev->spawnflags & 64 )
					pPlayer->m_iHideHUD &= ~HIDEHUD_CUSTOMCROSSHAIR;

				UTIL_Remove( pPlayer->pDeathScene );

				MESSAGE_BEGIN( MSG_ONE, gmsgAttachCam, NULL, pPlayer->pev );
					WRITE_SHORT( 0 );
					WRITE_BYTE( 0 );
				MESSAGE_END();


				MESSAGE_BEGIN( MSG_ONE, gmsgCameraZoom, NULL, pPlayer->pev );
					WRITE_COORD( 50 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
				MESSAGE_END();

				pPlayer->pev->vuser2.x = 90;
			}
		}


		if ( pev->target )
			FireTargets( STRING(pev->target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}
	else
	{
		if ( ismp )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

				if ( plr && plr->pDeathScene )
				{
					if ( plr->pev->vuser1.x != 0 )
					{
						if ( !strcmp( STRING(pev->model), "models/ladder_climb_doctor.mdl" ) )
						{
							if ( m_iCurrentAnimation > 1 )
								plr->pDeathScene->pev->origin.z += 48;
						}
					}
					else
					{
						if ( !strcmp( STRING(pev->model), "models/ladder_climb.mdl" ) )
						{
							if ( m_iCurrentAnimation > 1 )
								plr->pDeathScene->pev->origin.z += 48;
						}
					}



					if ( g_pGameRules && g_pGameRules->IsMultiplayer() && false )
					{
						int seq = LookupSequence( STRING(m_cAnimation[m_iCurrentAnimation]) );

						if ( seq != -1 )
						{
							MESSAGE_BEGIN( MSG_ALL, gmsgCoopCamAnim );
								WRITE_BYTE( seq );
							MESSAGE_END();
						}

					}
					else
						((CBaseMonster *)plr->pDeathScene)->SetSequenceByName( (char *)STRING(m_cAnimation[m_iCurrentAnimation]) );
				}
			}

			if ( m_iCurrentAnimation <= m_iNumAnimations )
			{
				float delay = 0;
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT(pPlayer->pDeathScene->pev) ), (char *)STRING(m_cAnimation[m_iCurrentAnimation]) );

				if ( delay != 0 )
				{
					SetNextThink( delay );

				}
				else
					ALERT( at_console, "Unable to find delay\n" );
			}

			m_iCurrentAnimation++;
		}
		else
		{
			if ( pPlayer->pev->vuser1.x != 0 )
			{
				if ( !strcmp( STRING(pev->model), "models/ladder_climb_doctor.mdl" ) )
				{
					if ( m_iCurrentAnimation > 1 )
						pPlayer->pDeathScene->pev->origin.z += 48;
				}
			}
			else
			{
				if ( !strcmp( STRING(pev->model), "models/ladder_climb.mdl" ) )
				{
					if ( m_iCurrentAnimation > 1 )
						pPlayer->pDeathScene->pev->origin.z += 48;
				}
			}



			if ( g_pGameRules && g_pGameRules->IsMultiplayer() && false )
			{
				int seq = LookupSequence( STRING(m_cAnimation[m_iCurrentAnimation]) );

				if ( seq != -1 )
				{
					MESSAGE_BEGIN( MSG_ALL, gmsgCoopCamAnim );
						WRITE_BYTE( seq );
					MESSAGE_END();
				}

			}
			else
				((CBaseMonster *)pPlayer->pDeathScene)->SetSequenceByName( (char *)STRING(m_cAnimation[m_iCurrentAnimation]) );

			if ( m_iCurrentAnimation <= m_iNumAnimations )
			{
				float delay = 0;
				delay = GetSequenceTimeNamed( GET_MODEL_PTR( ENT(pPlayer->pDeathScene->pev) ), (char *)STRING(m_cAnimation[m_iCurrentAnimation]) );

				if ( delay != 0 )
				{
					SetNextThink( delay );

				}
				else
					ALERT( at_console, "Unable to find delay\n" );
			}

			m_iCurrentAnimation++;
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_mdlcutscene, CCoFMDLCutscene );

//=========================================================
// cof_entityrestore
//=========================================================
class CCoFEntityRestore : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFEntityRestore::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFEntityRestore::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		if ( pev->impulse )
		{
			for ( int i = 1; i <= 50; i++ )
			{
				if ( pPlayer->transferredentity_message[i] )
				{



					if ( FStrEq( STRING(pPlayer->transferredentity_classname[i]), "item_key" ) )
					{
						CBaseEntity *pKey = CBaseEntity::CreateWithScript( "item_key", pPlayer->transferredentity_message[i], pPlayer->transferredentity_origin[i], g_vecZero, pPlayer->edict(), 0 );
					}


					if ( strstr( STRING(pPlayer->transferredentity_classname[i]), "weapon_" ) )
					{
						CBaseEntity *pGun = CBaseEntity::Create( (char *)STRING(pPlayer->transferredentity_classname[i]), pPlayer->transferredentity_origin[i], g_vecZero, pPlayer->edict() );

						if ( pGun )
						{
							pGun->pev->message = pPlayer->transferredentity_message[i];
							((CBasePlayerWeapon *)pGun)->m_iClip = pPlayer->transferredentity_clip[i];

							pGun->Spawn();
						}
					}
				}



				pPlayer->transferredentity_message[i] = 0;
				pPlayer->transferredentity_origin[i] = g_vecZero;
				pPlayer->transferredentity_classname[i] = 0;
				pPlayer->transferredentity_clip[i] = 0;
			}
		}
		else
		{



			for ( int k = 1; k <= 50; k++ )
			{
				pPlayer->transferredentity_message[k] = 0;
				pPlayer->transferredentity_origin[k] = g_vecZero;
				pPlayer->transferredentity_classname[k] = 0;
				pPlayer->transferredentity_clip[k] = 0;
			}




			edict_t *pEdict = INDEXENT( 1 );
			int count = 1;

			for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
			{
				if ( !pEdict )
					continue;

				if ( pEdict->free )
					continue;

				if ( pEdict == edict() )
					continue;

				if ( pEdict->v.armorvalue == 666 )
				{



					if ( count <= 50 )
					{
						CBaseEntity *pEntity = CBaseEntity::Instance( pEdict );

						if ( !pEntity )
							continue;

						pPlayer->transferredentity_message[count] = pEntity->pev->message;
						pPlayer->transferredentity_origin[count] = pEntity->pev->origin;
						pPlayer->transferredentity_classname[count] = pEntity->pev->classname;

						if ( strstr( STRING(pEntity->pev->classname), "weapon_" ) )
						{
							if ( !((CBasePlayerWeapon *)pEntity)->m_iClip && ((CBasePlayerWeapon *)pEntity)->iMaxClip() )
								pPlayer->transferredentity_clip[count] = -666;
							else
								pPlayer->transferredentity_clip[count] = ((CBasePlayerWeapon *)pEntity)->m_iClip;
						}
						else
							pPlayer->transferredentity_clip[count] = 0;

						count++;
					}
				}
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_entityrestore, CCoFEntityRestore );

//=========================================================
// cof_slowmotion
//=========================================================
class CCoFSlowMotion : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFSlowMotion::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFSlowMotion::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	pev->iuser1 = !pev->iuser1;

	if ( pev->iuser1 )
		CVAR_SET_FLOAT( "host_framerate", pev->health );
	else
		CVAR_SET_FLOAT( "host_framerate", 0 );
}

LINK_ENTITY_TO_CLASS( cof_slowmotion, CCoFSlowMotion );

//=========================================================
// cof_addcodenote
//=========================================================
class CCofCodeNote : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

LINK_ENTITY_TO_CLASS( cof_addcodenote, CCofCodeNote );

void CCofCodeNote::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofCodeNote::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{


			for ( int i = 1; i <= 16; i++ )
			{


				if ( strlen( pPlayer->documentsstruct[i].note1 ) < 1 )
				{


					strcpy( pPlayer->documentsstruct[i].note1, "nerds" );
					strcpy( pPlayer->documentsstruct[i].page1, "nerds" );
					strcpy( pPlayer->documentsstruct[i].turnsound, "nerds" );
					strcpy( pPlayer->documentsstruct[i].title, "nerds" );
					strcpy( pPlayer->documentsstruct[i].label, "nerds" );

					MESSAGE_BEGIN( MSG_ONE, gmsgAddNote, NULL, pPlayer->pev );
						WRITE_BYTE( i );
						WRITE_BYTE( 1 );
						WRITE_SHORT( pPlayer->m_iKeypadNumber );
						WRITE_BYTE( pev->iuser1 );
						WRITE_STRING( pPlayer->documentsstruct[i].note1 );
						WRITE_STRING( pPlayer->documentsstruct[i].note2 );
						WRITE_STRING( pPlayer->documentsstruct[i].note3 );
						WRITE_STRING( pPlayer->documentsstruct[i].note4 );
						WRITE_STRING( pPlayer->documentsstruct[i].page1 );
						WRITE_STRING( pPlayer->documentsstruct[i].page2 );
						WRITE_STRING( pPlayer->documentsstruct[i].page3 );
						WRITE_STRING( pPlayer->documentsstruct[i].page4 );
						WRITE_STRING( pPlayer->documentsstruct[i].turnsound );
						WRITE_STRING( pPlayer->documentsstruct[i].title );
						WRITE_STRING( pPlayer->documentsstruct[i].label );
					MESSAGE_END();
					break;
				}
			}
		}
	}
}

//=========================================================
// cof_lensflare
//=========================================================
class CCofLensFlare : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void SendInitMessage( CBasePlayer *player );
};

LINK_ENTITY_TO_CLASS( cof_lensflare, CCofLensFlare );

void CCofLensFlare::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

void CCofLensFlare::SendInitMessage( CBasePlayer *player )
{
	if ( player )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgLensFlare, NULL, player->pev );
			WRITE_COORD( pev->origin.x );
			WRITE_COORD( pev->origin.y );
			WRITE_COORD( pev->origin.z );
		MESSAGE_END();
	}
}

//=========================================================
// cof_monster_random_spawn
//=========================================================
class CCofMonsterRandomSpawn : public CBaseEntity
{
public:
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT DoSpawnStuff( void );
};

LINK_ENTITY_TO_CLASS( cof_monster_random_spawn, CCofMonsterRandomSpawn );

void CCofMonsterRandomSpawn::Precache( void )
{
	if ( pev->message )
		UTIL_PrecacheOther( STRING(pev->message) );

	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
}

void CCofMonsterRandomSpawn::Spawn( void )
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->takedamage = 66;

	SetThink( &CCofMonsterRandomSpawn ::SUB_Remove );
	SetNextThink( 2 );

	pev->movetype = MOVETYPE_NONE;

	if ( pev->health > 0 )
	{
		SetThink( &CCofMonsterRandomSpawn ::DoSpawnStuff );
		SetNextThink( 2 );
	}
	else
	{
		SetThink( &CCofMonsterRandomSpawn ::SUB_Remove );
		SetNextThink( 5 );
	}
}

void CCofMonsterRandomSpawn::DoSpawnStuff( void )
{
	if ( pev->health <= 0 )
		return;

	edict_t *pEdict = INDEXENT( 1 );
	CBaseEntity *pEntity;
	CBaseEntity *pList[32];
	int i;
	int spawncount;
	int count;
	count = 0;
	int max = pev->frags;


	for ( i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )
			continue;

		if ( !pEdict )
			continue;

		if ( pEdict->v.takedamage == 66 )
		{
			pEntity = CBaseEntity::Instance( pEdict );

			if ( count < 30 && pEntity && FStrEq( STRING(pev->targetname), STRING(pEntity->pev->targetname) ) )
			{
				count++;
				pList[count] = pEntity;
			}
		}
	}

	if ( count <= 0 )
		return;

	spawncount = 0;


	for ( i = 1; i <= count; i++ )
	{
		int random = RANDOM_LONG( 1, count );

		if ( pList[random] && i <= max )
		{
			CBaseEntity *pNerd = CBaseEntity::Create( (char *)STRING(pList[random]->pev->message), pList[random]->pev->origin, pList[random]->pev->angles, NULL );

			if ( pNerd && strstr( STRING(pNerd->pev->classname), "_slower" ) )
			{
				pNerd->pev->body = RANDOM_LONG( 0, 2 );
			}

			if ( pNerd && pev->model )
			{
				SET_MODEL( ENT(pNerd->pev), STRING(pev->model) );

				if ( pev->spawnflags & 1 )
					pNerd->pev->spawnflags |= 4;

				pNerd->Spawn();
			}

			if ( pNerd && strstr( STRING(pNerd->pev->classname), "monster_" ) )
			{
				if ( pev->spawnflags & 1 )
					pNerd->pev->spawnflags |= 4;

				pNerd->Spawn();
			}

			pList[random] = NULL;
		}
	}

	UTIL_Remove( this );
}

//=========================================================
// info_simon_spawnpoint
//=========================================================
class CCofSimonRandomSpawn : public CBaseEntity
{
public:
	virtual void Spawn( void );
};

LINK_ENTITY_TO_CLASS( info_simon_spawnpoint, CCofSimonRandomSpawn );

void CCofSimonRandomSpawn::Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;

	pev->movetype = MOVETYPE_NONE;
}

//=========================================================
// cof_changelevel
//=========================================================
class CCoFChangeLevel : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFChangeLevel::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFChangeLevel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( g_pLevelChange )
	{



		memset( g_pLevelChange->playervariablestruct, 0, sizeof( g_pLevelChange->playervariablestruct ) );
		memset( g_pLevelChange->playervariablestructstats, 0, sizeof( g_pLevelChange->playervariablestructstats ) );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pEnt = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( pEnt )
			{
				g_pLevelChange->SetPlayerVariables( pEnt );
			}
		}
	}

	if ( pev->message )
		CHANGE_LEVEL( (char *)STRING(pev->message), NULL );
}

LINK_ENTITY_TO_CLASS( cof_changelevel, CCoFChangeLevel );

//=========================================================
// cof_wheelchairmode
//=========================================================
class CCoFWheelchairMode : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Precache( void );
};

void CCoFWheelchairMode::Precache( void )
{
	UTIL_PrecacheOther( "wheelchair" );
}

void CCoFWheelchairMode::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFWheelchairMode::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		pPlayer->StartWheelchairMode();

		if ( pev->message && pPlayer->m_pWheelchair )
		{
			CBaseEntity *pOrigin = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );

			if ( pOrigin )
			{
				UTIL_SetOrigin( pPlayer->m_pWheelchair, pOrigin->pev->origin );
				pPlayer->m_pWheelchair->pev->angles = pev->angles;
				pPlayer->m_pWheelchair->pev->fixangle = TRUE;
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_wheelchairmode, CCoFWheelchairMode );

//=========================================================
// cof_fatfuckingcunt
//=========================================================
class CCoFFatFuckingCunt : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFFatFuckingCunt::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFFatFuckingCunt::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
	{
		CBaseEntity *pFag = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );

		if ( pFag )
		{
			pFag->pev->health = 0;
			pFag->Killed( pev, GIB_NEVER );
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_fatfuckingcunt, CCoFFatFuckingCunt );

//=========================================================
// cof_simonspeak
//=========================================================
class CCoFSimonSpeak : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFSimonSpeak::Precache( void )
{
	if ( pev->noise )
		PRECACHE_SOUND( (char *)STRING(pev->noise) );
}

void CCoFSimonSpeak::Spawn( void )
{
	Precache();
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFSimonSpeak::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	if ( !pActivator->IsPlayer() )
		return;

	CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

	if ( pPlayer )
		EMIT_SOUND_DYN( pPlayer->edict(), pev->iuser1, STRING(pev->noise), pev->fuser1, ATTN_NORM, 0, PITCH_NORM );
}

LINK_ENTITY_TO_CLASS( cof_simonspeak, CCoFSimonSpeak );

//=========================================================
// cof_credits
//=========================================================
class CCoFCredits : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFCredits::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;

	if ( pev->fuser1 > 700 )
		pev->fuser1 = 700;
}

int CalculatePoints( CBasePlayer *pPlayer )
{

	int points = 0;
	int monsterskilled = pPlayer->playerstats.monsters_killed_melee + pPlayer->playerstats.monsters_killed_shooting;
	int minutestaken = pPlayer->playerstats.time_hours * 60 + pPlayer->playerstats.time_minutes;

	pPlayer->CalculateAccuracy();




	int difficultyadd = 0;

	if ( pPlayer->playerstats.chosen_difficulty == 1 )
		difficultyadd = 2;

	if ( pPlayer->playerstats.chosen_difficulty == 2 )
		difficultyadd = 4;

	if ( pPlayer->playerstats.chosen_difficulty == 3 )
		difficultyadd = 6;

	if ( pPlayer->playerstats.chosen_difficulty == 4 )
		difficultyadd = 7;

	points += difficultyadd;






	int accuracyadd = 0;

	if ( pPlayer->playerstats.gunaccuracy >= 30 && pPlayer->playerstats.gunaccuracy < 40 )
		accuracyadd = 1;

	if ( pPlayer->playerstats.gunaccuracy >= 40 && pPlayer->playerstats.gunaccuracy < 50 )
		accuracyadd = 2;

	if ( pPlayer->playerstats.gunaccuracy >= 50 && pPlayer->playerstats.gunaccuracy < 60 )
		accuracyadd = 3;

	if ( pPlayer->playerstats.gunaccuracy >= 60 && pPlayer->playerstats.gunaccuracy < 70 )
		accuracyadd = 4;

	if ( pPlayer->playerstats.gunaccuracy >= 70 && pPlayer->playerstats.gunaccuracy < 80 )
		accuracyadd = 5;

	if ( pPlayer->playerstats.gunaccuracy >= 80 && pPlayer->playerstats.gunaccuracy < 90 )
		accuracyadd = 6;

	if ( pPlayer->playerstats.gunaccuracy >= 90 )
		accuracyadd = 7;

	points += accuracyadd;





	int savedadd = 0;

	if ( pPlayer->playerstats.timessaved > 70 )
		savedadd = 1;

	if ( pPlayer->playerstats.timessaved <= 70 && pPlayer->playerstats.timessaved > 60 )
		savedadd = 2;

	if ( pPlayer->playerstats.timessaved <= 60 && pPlayer->playerstats.timessaved > 40 )
		savedadd = 3;

	if ( pPlayer->playerstats.timessaved <= 40 && pPlayer->playerstats.timessaved > 25 )
		savedadd = 4;

	if ( pPlayer->playerstats.timessaved <= 25 && pPlayer->playerstats.timessaved > 10 )
		savedadd = 5;

	if ( pPlayer->playerstats.timessaved <= 10 && pPlayer->playerstats.timessaved > 5 )
		savedadd = 6;

	if ( pPlayer->playerstats.timessaved <= 5 )
		savedadd = 7;

	points += savedadd;





	int syringesadd = 0;

	if ( pPlayer->playerstats.syringes_used > 45 )
		syringesadd = 1;

	if ( pPlayer->playerstats.syringes_used <= 45 && pPlayer->playerstats.syringes_used > 40 )
		syringesadd = 2;

	if ( pPlayer->playerstats.syringes_used <= 40 && pPlayer->playerstats.syringes_used > 30 )
		syringesadd = 3;

	if ( pPlayer->playerstats.syringes_used <= 30 && pPlayer->playerstats.syringes_used > 20 )
		syringesadd = 4;

	if ( pPlayer->playerstats.syringes_used <= 20 && pPlayer->playerstats.syringes_used > 10 )
		syringesadd = 5;

	if ( pPlayer->playerstats.syringes_used <= 10 && pPlayer->playerstats.syringes_used > 2 )
		syringesadd = 6;

	if ( pPlayer->playerstats.syringes_used <= 2 )
		syringesadd = 7;

	points += syringesadd;

































	int timeadd = 0;

	if ( minutestaken <= 420 && minutestaken > 360 )
		timeadd = 1;

	if ( minutestaken <= 360 && minutestaken > 300 )
		timeadd = 2;

	if ( minutestaken <= 300 && minutestaken > 240 )
		timeadd = 3;

	if ( minutestaken <= 240 && minutestaken > 180 )
		timeadd = 4;

	if ( minutestaken <= 180 && minutestaken > 150 )
		timeadd = 5;

	if ( minutestaken <= 150 )
		timeadd = 7;







	points += timeadd;






	if ( pPlayer->playerstats.started_from_the_beginning != true )
		points = 0;

	return points;
}

int CalculatePointsDoctor( CBasePlayer *pPlayer )
{
	pPlayer->CalculateAccuracy();


	int points = 0;
	int monsterskilled = pPlayer->playerstats.monsters_killed_melee + pPlayer->playerstats.monsters_killed_shooting;
	int minutestaken = pPlayer->playerstats.time_hours * 60 + pPlayer->playerstats.time_minutes;




	if ( pPlayer->playerstats.chosen_difficulty == 1 )
		points += 4;

	if ( pPlayer->playerstats.chosen_difficulty == 2 )
		points += 4;

	if ( pPlayer->playerstats.chosen_difficulty == 3 )
		points += 4;

	if ( pPlayer->playerstats.chosen_difficulty == 4 )
		points += 4;




	if ( pPlayer->playerstats.gunaccuracy >= 30 && pPlayer->playerstats.gunaccuracy < 40 )
		points += 1;

	if ( pPlayer->playerstats.gunaccuracy >= 40 && pPlayer->playerstats.gunaccuracy < 50 )
		points += 2;

	if ( pPlayer->playerstats.gunaccuracy >= 50 && pPlayer->playerstats.gunaccuracy < 60 )
		points += 3;

	if ( pPlayer->playerstats.gunaccuracy >= 60 && pPlayer->playerstats.gunaccuracy < 70 )
		points += 4;

	if ( pPlayer->playerstats.gunaccuracy >= 70 && pPlayer->playerstats.gunaccuracy < 80 )
		points += 5;

	if ( pPlayer->playerstats.gunaccuracy >= 80 && pPlayer->playerstats.gunaccuracy < 90 )
		points += 6;

	if ( pPlayer->playerstats.gunaccuracy >= 90 )
		points += 7;




























	if ( minutestaken <= 14 && minutestaken > 13 )
		points += 1;

	if ( minutestaken <= 13 && minutestaken > 12 )
		points += 2;

	if ( minutestaken <= 12 && minutestaken > 11 )
		points += 3;

	if ( minutestaken <= 11 && minutestaken > 10 )
		points += 4;

	if ( minutestaken <= 10 && minutestaken > 9 )
		points += 5;

	if ( minutestaken <= 9 && minutestaken > 8 )
		points += 6;

	if ( minutestaken <= 8 )
		points += 7;






	if ( pPlayer->playerstats.started_from_the_beginning != true )
		points = 0;

	return points;
}

void CCoFCredits::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	int ending6 = 0;
	int ending7 = 0;

	char sFileName[256];
	GET_GAME_DIR( sFileName );

	if ( pev->message )
	{
		if ( !strstr( STRING(pev->message), "txtfiles/" ) )
		{
			char szText[256];
			strcpy( szText, "/txtfiles/" );
			strcat( szText, STRING(pev->message) );
			strcat( sFileName, szText );
		}
		else
			strcat( sFileName, STRING(pev->message) );
	}
	else
		strcat( sFileName, "/txtfiles/credits.txt" );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if ( player )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

			if ( pPlayer )
			{
				int unlockflags = 0;

				int ending = 0;

				int m_iGrade = 7;


				if ( pPlayer->playerstats.started_from_the_beginning && strstr( STRING(gpGlobals->mapname), "c_ending" ) )
					pPlayer->playerstats.can_unlock_shit = true;


				if ( pPlayer->playerstats.started_from_the_beginning && !( pev->spawnflags & 1 ) && pPlayer->playerstats.can_unlock_shit )
				{

					if ( ( pPlayer->m_iFoundHoodies & 0x80000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x80000, false, false, false );
						unlockflags |= 0x80000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x80 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x80, false, false, false );
						unlockflags |= 0x80;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x100 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x100, false, false, false );
						unlockflags |= 0x100;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x200 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x200, false, false, false );
						unlockflags |= 0x200;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x400 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x400, false, false, false );
						unlockflags |= 0x400;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x800 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x800, false, false, false );
						unlockflags |= 0x800;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x1000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x1000, false, false, false );
						unlockflags |= 0x1000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x2000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x2000, false, false, false );
						unlockflags |= 0x2000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x4000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x4000, false, false, false );
						unlockflags |= 0x4000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x8000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x8000, false, false, false );
						unlockflags |= 0x8000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x10000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x10000, false, false, false );
						unlockflags |= 0x10000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x20000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x20000, false, false, false );
						unlockflags |= 0x20000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x40000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x40000, false, false, false );
						unlockflags |= 0x40000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x2000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x2000000, false, false, false );
						unlockflags |= 0x2000000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x4000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x4000000, false, false, false );
						unlockflags |= 0x4000000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x8000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x8000000, false, false, false );
						unlockflags |= 0x8000000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x10000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x10000000, false, false, false );
						unlockflags |= 0x10000000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x20000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x20000000, false, false, false );
						unlockflags |= 0x20000000;
					}

					if ( ( pPlayer->m_iFoundHoodies & 0x40000000 ) && ( pev->spawnflags & 2 ) )
					{
						pPlayer->SetEndingOrUnlockables( 0x40000000, false, false, false );
						unlockflags |= 0x40000000;
					}

					int minutes = pPlayer->playerstats.time_hours * 60;
					minutes += pPlayer->playerstats.time_minutes;

					if ( minutes <= 150 && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x100000, false, false ) && !pPlayer->playerstats.doctor_mode )
							unlockflags |= 0x100000;

						pPlayer->SetEndingOrUnlockables( 0x100000, false, false, false );
					}



					if ( !pPlayer->m_b_Ending_RoofBossKilled && !pPlayer->m_b_Ending_GivenP345 && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 1, false, false ) )
							unlockflags |= 1;

						pPlayer->SetEndingOrUnlockables( 1, false, false, false );
						ending = 1;
					}



					if ( !pPlayer->m_b_Ending_RoofBossKilled && !pPlayer->playerstats.doctor_mode && pPlayer->m_b_Ending_GivenP345 == true && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 2, false, false ) )
							unlockflags |= 2;

						pPlayer->SetEndingOrUnlockables( 2, false, false, false );
						ending = 2;
					}



					if ( pPlayer->m_b_Ending_RoofBossKilled == true && !pPlayer->playerstats.doctor_mode && !pPlayer->m_b_Ending_GivenP345 && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 4, false, false ) )
							unlockflags |= 4;

						pPlayer->SetEndingOrUnlockables( 4, false, false, false );
						ending = 3;
					}



					if ( pPlayer->m_b_Ending_RoofBossKilled == true && !pPlayer->playerstats.doctor_mode && pPlayer->m_b_Ending_GivenP345 == true && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 8, false, false ) )
							unlockflags |= 8;

						pPlayer->SetEndingOrUnlockables( 8, false, false, false );
						ending = 4;
					}



					if ( pPlayer->m_b_Ending_DeliveredPackage == true && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x10, false, false ) )
							unlockflags |= 0x10;

						pPlayer->SetEndingOrUnlockables( 0x10, false, false, false );

						ending = 5;
					}



					if ( pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
					{
						ending6 = 1;
						pPlayer->SetEndingOrUnlockables( 0, false, true, false );

						ending = 6;
					}



					if ( pPlayer->CheckingEndingOrUnlockables( 0, false, true ) )
						ending7 = 1;

					int points = 0;



					if ( pPlayer->playerstats.doctor_mode )
					{







						points = CalculatePointsDoctor( pPlayer );

						if ( points <= 2 )
							m_iGrade = 7;

						if ( points <= 4 && points > 2 )
							m_iGrade = 6;

						if ( points <= 10 && points > 4 )
							m_iGrade = 5;

						if ( points <= 14 && points > 10 )
							m_iGrade = 4;

						if ( points <= 16 && points > 14 )
							m_iGrade = 3;

						if ( points == 17 )
							m_iGrade = 2;

						if ( points >= 18 )
							m_iGrade = 1;
					}
					else
					{
						points = CalculatePoints( pPlayer );

						if ( points <= 2 )
							m_iGrade = 2;

						if ( points <= 7 && points > 2 )
							m_iGrade = 6;

						if ( points <= 14 && points > 7 )
							m_iGrade = 5;

						if ( points <= 21 && points > 14 )
							m_iGrade = 4;

						if ( points <= 28 && points > 21 )
							m_iGrade = 3;

						if ( points < 35 && points > 28 )
							m_iGrade = 2;

						if ( points >= 35 )
						{
							m_iGrade = 1;

							if ( !pPlayer->CheckingEndingOrUnlockables( 0x200000, false, false ) && ( pev->spawnflags & 2 ) )
								unlockflags |= 0x200000;

							if ( pev->spawnflags & 2 )
								pPlayer->SetEndingOrUnlockables( 0x200000, false, false, false );
						}
					}






					if ( !pPlayer->CheckingEndingOrUnlockables( 0x2000000, false, false ) && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
						unlockflags |= 0x2000000;

					if ( ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
						pPlayer->SetEndingOrUnlockables( 0x2000000, false, false, false );


					if ( !pPlayer->CheckingEndingOrUnlockables( 0x400000, false, false ) && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
						unlockflags |= 0x400000;

					if ( ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
						pPlayer->SetEndingOrUnlockables( 0x400000, false, false, false );


					if ( !pPlayer->CheckingEndingOrUnlockables( 0x80000000, false, false ) && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
						unlockflags |= 0x80000000;

					if ( ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
						pPlayer->SetEndingOrUnlockables( 0x80000000, false, false, false );


					if ( !pPlayer->CheckingEndingOrUnlockables( 0x20, false, false ) && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
						unlockflags |= 0x20;

					if ( ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
						pPlayer->SetEndingOrUnlockables( 0x20, false, false, false );


					if ( pPlayer->playerstats.chosen_difficulty == 4 && !pPlayer->playerstats.doctor_mode && ( pev->spawnflags & 2 ) )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x40, false, false ) )
							unlockflags |= 0x40;

						pPlayer->SetEndingOrUnlockables( 0x40, false, false, false );
					}














					if ( pPlayer->CheckingEndingOrUnlockables( 0x80, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x100, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x200, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x400, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x800, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x1000, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x2000, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x4000, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x8000, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x10000, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x20000, false, false ) && !pPlayer->playerstats.doctor_mode )
					{
						if ( pev->spawnflags & 2 )
						{
							if ( !pPlayer->CheckingEndingOrUnlockables( 0x40000, false, false ) )
								unlockflags |= 0x40000;

							pPlayer->SetEndingOrUnlockables( 0x40000, false, false, false );
						}
					}




					if ( ending == 1 && !pPlayer->playerstats.doctor_mode )
					{
						if ( pev->spawnflags & 2 )
						{
							if ( !pPlayer->CheckingEndingOrUnlockables( 0x2000, false, false ) )
								unlockflags |= 0x2000;

							pPlayer->SetEndingOrUnlockables( 0x2000, false, false, false );
						}
					}




					if ( ending == 2 && !pPlayer->playerstats.doctor_mode )
					{
						if ( pev->spawnflags & 2 )
						{
							if ( !pPlayer->CheckingEndingOrUnlockables( 0x800, false, false ) )
								unlockflags |= 0x800;

							pPlayer->SetEndingOrUnlockables( 0x800, false, false, false );
						}
					}




					if ( ending == 3 && ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x4000, false, false ) )
							unlockflags |= 0x4000;

						pPlayer->SetEndingOrUnlockables( 0x4000, false, false, false );
					}



					if ( ending == 4 && ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x10000, false, false ) )
							unlockflags |= 0x10000;

						pPlayer->SetEndingOrUnlockables( 0x10000, false, false, false );

						if ( !pPlayer->CheckingEndingOrUnlockables( 0x800000, false, false ) )
							unlockflags |= 0x800000;

						pPlayer->SetEndingOrUnlockables( 0x800000, false, false, false );
					}



					if ( ending == 5 && ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
					{
						if ( !pPlayer->CheckingEndingOrUnlockables( 0x80, false, false ) )
							unlockflags |= 0x80;

						pPlayer->SetEndingOrUnlockables( 0x80, false, false, false );
					}








					if ( pPlayer->CheckingEndingOrUnlockables( 1, false, false ) && pPlayer->CheckingEndingOrUnlockables( 2, false, false ) && pPlayer->CheckingEndingOrUnlockables( 4, false, false ) && pPlayer->CheckingEndingOrUnlockables( 8, false, false ) && pPlayer->CheckingEndingOrUnlockables( 0x10, false, false ) && !pPlayer->playerstats.doctor_mode )
					{
						if ( pev->spawnflags & 2 )
						{
							if ( !pPlayer->CheckingEndingOrUnlockables( 0x8000, false, false ) )
								unlockflags |= 0x8000;

							pPlayer->SetEndingOrUnlockables( 0x8000, false, false, false );
						}
					}
				}



				if ( ( pev->spawnflags & 2 ) && !pPlayer->playerstats.doctor_mode )
				{
					if ( pPlayer->CheckingEndingOrUnlockables( 1, false, false ) )
						unlockflags |= 1;

					if ( pPlayer->CheckingEndingOrUnlockables( 2, false, false ) )
						unlockflags |= 2;

					if ( pPlayer->CheckingEndingOrUnlockables( 4, false, false ) )
						unlockflags |= 4;

					if ( pPlayer->CheckingEndingOrUnlockables( 8, false, false ) )
						unlockflags |= 8;

					if ( pPlayer->CheckingEndingOrUnlockables( 0x10, false, false ) )
						unlockflags |= 0x10;
				}










				MESSAGE_BEGIN( MSG_ONE, gmsgCredits, NULL, pPlayer->pev );
					if ( pev->message )
					{
						if ( pev->spawnflags & 4 )
							WRITE_BYTE( 0 );
						else
							WRITE_BYTE( 1 );
					}
					else
						WRITE_BYTE( 0 );
					WRITE_STRING( sFileName );
					WRITE_SHORT( pPlayer->playerstats.shots_fired );
					WRITE_SHORT( pPlayer->playerstats.shots_hit );
					WRITE_SHORT( pPlayer->playerstats.syringes_used );
					WRITE_SHORT( pPlayer->playerstats.shots_missed );
					WRITE_COORD( pPlayer->CalculateAccuracy() );
					WRITE_SHORT( pPlayer->playerstats.timessaved );
					WRITE_SHORT( pPlayer->playerstats.weaponscollected );
					WRITE_SHORT( pPlayer->playerstats.damagetaken );
					WRITE_SHORT( pPlayer->playerstats.distancetravelled_metres );
					WRITE_SHORT( pPlayer->playerstats.distancetravelled_kilometres );
					WRITE_SHORT( pPlayer->playerstats.time_seconds );
					WRITE_SHORT( pPlayer->playerstats.time_minutes );
					WRITE_SHORT( pPlayer->playerstats.time_hours );
					WRITE_SHORT( pPlayer->playerstats.monsters_killed_melee );
					WRITE_SHORT( pPlayer->playerstats.monsters_killed_shooting );

					if ( pPlayer->playerstats.started_from_the_beginning && pPlayer->playerstats.can_unlock_shit )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );

					WRITE_BYTE( pev->iuser1 );

					if ( pev->spawnflags & 1 )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );
					WRITE_BYTE( pPlayer->playerstats.chosen_difficulty );
					WRITE_LONG( unlockflags );
					WRITE_BYTE( ending );
					WRITE_BYTE( m_iGrade );
					if ( pPlayer->playerstats.doctor_mode )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );


					if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );


					WRITE_BYTE( ending6 );
					WRITE_BYTE( ending7 );


					if ( pPlayer->CheckingEndingOrUnlockables( 0, true, false ) )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );


					if ( pPlayer->CheckingEndingOrUnlockables( 0, false, true ) )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );


					if ( pPlayer->playerstats.started_from_the_beginning && strstr( STRING(gpGlobals->mapname), "c_ending" ) )
						WRITE_BYTE( 1 );
					else
						WRITE_BYTE( 0 );
				MESSAGE_END();
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_credits, CCoFCredits );

//=========================================================
// cof_ending
//=========================================================
class CCoFEnding : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFEnding::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFEnding::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *player = UTIL_PlayerByIndex( i );

		if ( player )
		{
			CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

			if ( pPlayer )
			{
				switch ( pev->iuser1 )
				{
				case 0:
					pPlayer->m_b_Ending_RoofBossKilled = true;
					break;
				case 1:
					pPlayer->m_b_Ending_GivenP345 = true;
					break;
				case 2:
					pPlayer->m_b_Ending_DeliveredPackage = true;
					break;
				}
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_ending, CCoFEnding );

//=========================================================
// cof_begingame
//=========================================================
class CCoFGameBegin : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFGameBegin::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFGameBegin::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			plr->playerstats.started_from_the_beginning = true;
			plr->playerstats.chosen_difficulty = CVAR_GET_FLOAT( "difficulty" );

			if ( !strcmp( STRING(gpGlobals->mapname), "c_nightmare" ) || !strcmp( STRING(gpGlobals->mapname), "c_doc_city" ) )
				plr->playerstats.can_unlock_shit = true;

			int language = CVAR_GET_FLOAT( "cof_subtitlelanguage" );

			plr->playerstats.language = language;

			if ( pev->spawnflags & 1 )
			{
				plr->playerstats.doctor_mode = true;
				plr->pev->vuser1.x = 1;
			}
			else
				plr->pev->vuser1.x = 0;
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_begingame, CCoFGameBegin );

//=========================================================
// cof_greyfade
//=========================================================
class CCoFGreyFade : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFGreyFade::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFGreyFade::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgGreyFade, NULL, pPlayer->pev );
				WRITE_BYTE( pev->iuser1 );
				WRITE_BYTE( pev->iuser2 );
			MESSAGE_END();
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_greyfade, CCoFGreyFade );

//=========================================================
// cof_stopwheelchair
//=========================================================
class CCoFStopWheelchair : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFStopWheelchair::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFStopWheelchair::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
			pPlayer->StopWheelchairMode();
	}
}

LINK_ENTITY_TO_CLASS( cof_stopwheelchair, CCoFStopWheelchair );

//=========================================================
// cof_clothesmenu
//=========================================================
class CCoFClothesMenu : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	string_t m_sCameraEnt;
	string_t m_sSimonEnt;
	virtual void SendInitMessage( CBasePlayer *player );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static TYPEDESCRIPTION m_SaveData[];
};

TYPEDESCRIPTION	CCoFClothesMenu::m_SaveData[] =
{
	DEFINE_FIELD( CCoFClothesMenu, m_sCameraEnt, FIELD_STRING ),
	DEFINE_FIELD( CCoFClothesMenu, m_sSimonEnt, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CCoFClothesMenu, CPointEntity );

void CCoFClothesMenu::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFClothesMenu::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "cameraent" ) )
	{
		m_sCameraEnt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "simonent" ) )
	{
		m_sSimonEnt = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCoFClothesMenu::SendInitMessage( CBasePlayer *player )
{
	if ( m_sSimonEnt )
	{
		CBaseEntity *pSimon = UTIL_FindEntityByTargetname( NULL, STRING(m_sSimonEnt) );


		if ( pSimon )
		{
			pSimon->pev->rendermode = kRenderTransAdd;
			pSimon->pev->renderamt = 0;
		}
	}
}

void CCoFClothesMenu::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			int flags = 0;
			int canseeclothes = 0;

			if ( pPlayer->CheckingEndingOrUnlockables( 0x80, false, false ) )
			{
				canseeclothes = 1;
				flags |= 1;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x100, false, false ) )
			{
				canseeclothes = 1;
				flags |= 2;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x200, false, false ) )
			{
				canseeclothes = 1;
				flags |= 4;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x400, false, false ) )
			{
				canseeclothes = 1;
				flags |= 8;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x800, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x10;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x1000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x20;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x2000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x40;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x4000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x80;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x8000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x100;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x10000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x200;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x20000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x400;
			}

			if ( pPlayer->CheckingEndingOrUnlockables( 0x40000, false, false ) )
			{
				canseeclothes = 1;
				flags |= 0x800;
			}

			if ( m_sSimonEnt && m_sCameraEnt )
			{
				CBaseEntity *pSimon = UTIL_FindEntityByTargetname( NULL, STRING(m_sSimonEnt) );
				CBaseEntity *pCamera = UTIL_FindEntityByTargetname( NULL, STRING(m_sCameraEnt) );




				if ( value == 0 )
				{
					if ( pSimon )
					{
						pSimon->pev->rendermode = kRenderNormal;
						pSimon->pev->renderamt = 255;
					}


					if ( pCamera )
						pCamera->Use( pPlayer, pPlayer, USE_ON, 0 );
				}
				else
				{
					if ( pSimon )
					{
						pSimon->pev->rendermode = kRenderTransAdd;
						pSimon->pev->renderamt = 0;
					}


					if ( pCamera )
						pCamera->Use( pPlayer, pPlayer, USE_OFF, 666 );
				}
			}



			if ( value == 0 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgClothes, NULL, pPlayer->pev );
					WRITE_LONG( flags );
					WRITE_BYTE( canseeclothes );
					WRITE_BYTE( pPlayer->m_iUnlockedClothes );
					WRITE_BYTE( 1 );
				MESSAGE_END();
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_clothesmenu, CCoFClothesMenu );

//=========================================================
// cof_deadcat
//=========================================================
class CCoFDeadCat : public CBaseAnimating
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void EXPORT Think( void );
	void EXPORT CatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
};

LINK_ENTITY_TO_CLASS( cof_deadcat, CCoFDeadCat );

void CCoFDeadCat::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), "models/props/blandat/dead_cat.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT(pev) );

	pev->sequence = LookupSequence( "idle_test" );
	pev->frame = 0;
	ResetSequenceInfo();

	SetUse( &CCoFDeadCat::CatUse );

	pev->fuser1 = gpGlobals->time;

	SetNextThink( 0.1 );
}

void CCoFDeadCat::Precache( void )
{
	PRECACHE_MODEL( "models/props/blandat/dead_cat.mdl" );
	PRECACHE_SOUND( "subtitles/hellokitty.wav" );
	PRECACHE_SOUND( "player/find_note.wav" );
}

void CCoFDeadCat::CatUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() && pActivator->IsAlive() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer && pev->fuser1 < gpGlobals->time )
		{
			if ( pev->iuser1 >= 5 && !pPlayer->CheckingEndingOrUnlockables( 0x200, false, false ) && pev->iuser2 <= 0 )
			{
				UTIL_ProMessage( pPlayer, "Hello kitty...", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_VOICE, "subtitles/hellokitty.wav", 1, ATTN_NORM, 0, PITCH_NORM );

				pPlayer->m_iFoundHoodies |= 0x200;
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );

				pev->iuser2 = 1;
				pev->fuser1 = gpGlobals->time + 1.5;
			}
			else
			{
				pev->iuser1++;
				UTIL_ProMessage( pPlayer, "A dead cat...", 5, false );
				pev->fuser1 = gpGlobals->time + 0.75;
			}
		}
	}
}

void CCoFDeadCat::Think( void )
{
	StudioFrameAdvance();
	SetNextThink( 0.1 );
}

//=========================================================
// cof_hoodie
//=========================================================
class CCoFHoodie : public CBaseAnimating
{
	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void SendInitMessage( CBasePlayer *pPlayer );
	void EXPORT HoodieUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
};

LINK_ENTITY_TO_CLASS( cof_hoodie, CCoFHoodie );

void CCoFHoodie::Spawn( void )
{
	Precache();

	if ( pev->model )
		SET_MODEL( ENT(pev), STRING(pev->model) );
	else
		SET_MODEL( ENT(pev), "models/costumes/hoodie.mdl" );

	UTIL_SetOrigin( this, pev->origin );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT(pev) );

	SetUse( &CCoFHoodie::HoodieUse );
}

void CCoFHoodie::Precache( void )
{
	if ( pev->model )
		PRECACHE_MODEL( (char *)STRING(pev->model) );
	else
		PRECACHE_MODEL( "models/costumes/hoodie.mdl" );

	PRECACHE_SOUND( "player/find_note.wav" );
}

void CCoFHoodie::SendInitMessage( CBasePlayer *pPlayer )
{
	if ( pPlayer )
	{
		switch ( pev->iuser1 )
		{
		case 0:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x80, false, false ) )
				UTIL_Remove( this );
			break;
		case 1:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x100, false, false ) )
				UTIL_Remove( this );
			break;
		case 2:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x200, false, false ) )
				UTIL_Remove( this );
			break;
		case 3:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x400, false, false ) )
				UTIL_Remove( this );
			break;
		case 4:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x800, false, false ) )
				UTIL_Remove( this );
			break;
		case 5:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x1000, false, false ) )
				UTIL_Remove( this );
			break;
		case 6:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x2000, false, false ) )
				UTIL_Remove( this );
			break;
		case 7:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x4000, false, false ) )
				UTIL_Remove( this );
			break;
		case 8:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x8000, false, false ) )
				UTIL_Remove( this );
			break;
		case 9:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x10000, false, false ) )
				UTIL_Remove( this );
			break;
		case 10:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x20000, false, false ) )
				UTIL_Remove( this );
		case 11:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x2000000, false, false ) )
				UTIL_Remove( this );
		case 12:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x4000000, false, false ) )
				UTIL_Remove( this );
		case 13:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x8000000, false, false ) )
				UTIL_Remove( this );
		case 14:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x10000000, false, false ) )
				UTIL_Remove( this );
		case 15:
			if ( pPlayer->CheckingEndingOrUnlockables( 0x20000000, false, false ) )
				UTIL_Remove( this );
		}
	}
}

void CCoFHoodie::HoodieUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() && pActivator->IsAlive() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer )
		{
			switch ( pev->iuser1 )
			{
			case 0:
				pPlayer->m_iFoundHoodies |= 0x80;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Leatherhoff suit", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 1:
				pPlayer->m_iFoundHoodies |= 0x100;
				UTIL_ProMessage( pPlayer, "Unlocked costume: ModDB hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 2:
				pPlayer->m_iFoundHoodies |= 0x200;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Hello Kitty hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 3:
				pPlayer->m_iFoundHoodies |= 0x400;
				UTIL_ProMessage( pPlayer, "Unlocked costume: AoM hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 4:
				pPlayer->m_iFoundHoodies |= 0x800;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Camouflage hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 5:
				pPlayer->m_iFoundHoodies |= 0x1000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: HLC hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 6:
				pPlayer->m_iFoundHoodies |= 0x2000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Black metal suit", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 7:
				pPlayer->m_iFoundHoodies |= 0x4000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Team Psykskallar hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 8:
				pPlayer->m_iFoundHoodies |= 0x8000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Fuck anime hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 9:
				pPlayer->m_iFoundHoodies |= 0x10000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Sick Simon hoodie", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 10:
				pPlayer->m_iFoundHoodies |= 0x20000;
				UTIL_ProMessage( pPlayer, "Unlocked costume: Twitcher suit", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 11:
				pPlayer->m_iFoundHoodies |= 0x2000000;
				UTIL_ProMessage( pPlayer, "Unlocked book page: How To Unlock Book Pages", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 12:
				pPlayer->m_iFoundHoodies |= 0x4000000;
				UTIL_ProMessage( pPlayer, "Unlocked book page: Ranking System Explained", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 13:
				pPlayer->m_iFoundHoodies |= 0x8000000;
				UTIL_ProMessage( pPlayer, "Unlocked book page: How To Unlock Hoodies", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 14:
				pPlayer->m_iFoundHoodies |= 0x10000000;
				UTIL_ProMessage( pPlayer, "Unlocked book page: How To Unlock Secret Items", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 15:
				pPlayer->m_iFoundHoodies |= 0x20000000;
				UTIL_ProMessage( pPlayer, "Unlocked book page: How To Unlock Secret Weapons", 0, false );
				EMIT_SOUND_DYN( pPlayer->edict(), CHAN_AUTO, "player/find_note.wav", 1, ATTN_NORM, 0, PITCH_NORM );
				break;
			}
		}



		UTIL_Remove( this );
	}
}

//=========================================================
// aom_pills
//=========================================================
class CAoMPills : public CBaseAnimating
{
	virtual void Spawn( void );
	virtual void Precache( void );
	void EXPORT PillsUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE; }
};

LINK_ENTITY_TO_CLASS( aom_pills, CAoMPills );

void CAoMPills::Spawn( void )
{
	Precache();
	SET_MODEL( ENT(pev), "models/items/w_pills.mdl" );
	UTIL_SetOrigin( this, pev->origin );

	pev->origin.z += 1;
	DROP_TO_FLOOR( ENT(pev) );

	SetUse( &CAoMPills::PillsUse );
}

void CAoMPills::Precache( void )
{
	PRECACHE_MODEL( "models/items/w_pills.mdl" );
	PRECACHE_SOUND( "cutscene/smallmedkit1.wav" );
}

void CAoMPills::PillsUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pActivator && pActivator->IsPlayer() && pActivator->IsAlive() )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)pActivator->pev );

		if ( pPlayer && pPlayer->TakeHealth( gSkillData.aomPills, DMG_GENERIC ) )
		{
			EMIT_SOUND_DYN( edict(), CHAN_ITEM, "cutscene/smallmedkit1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
			UTIL_Remove( this );
		}
	}
}

//=========================================================
// cof_objective
//=========================================================
class CCoFObjective : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFObjective::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFObjective::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{


			if ( pev->target )
				pPlayer->m_sCustomObjectives = pev->target;

			pPlayer->m_sObjectiveText = pev->message;
			showobjective = true;
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_objective, CCoFObjective );

//=========================================================
// cof_startdoctormode
//=========================================================
class CCoFStartDoctorMode : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT GiveGun( void );
};

void CCoFStartDoctorMode::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFStartDoctorMode::GiveGun( void )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			pPlayer->ShowHint( MAKE_STRING( "nightvision" ), false );

			gSpecialItemGive = TRUE;
			pPlayer->m_bSilentGive = true;

			pPlayer->GiveNamedItem( "weapon_revolver", false, false );
			pPlayer->GiveNamedItem( "weapon_revolver", false, false );
			pPlayer->GiveNamedItem( "weapon_revolver", false, false );
			pPlayer->GiveNamedItem( "weapon_revolver", false, false );
			pPlayer->GiveNamedItem( "weapon_revolver", false, false );

			gSpecialItemGive = FALSE;
		}
	}
}

void CCoFStartDoctorMode::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{


			if ( pPlayer->pev->vuser1.x != 0 )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgHeadShield, NULL, pPlayer->pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( 10 );
					WRITE_STRING( "" );
					WRITE_BYTE( 0 );
				MESSAGE_END();








				pPlayer->m_iHeadShieldOn = 1;

				UTIL_ScreenFade( pPlayer, Vector( 0, 0, 0 ), 0.5, 0.1, 255, FFADE_IN );

				EMIT_SOUND_DYN( ENT(pPlayer->pev), CHAN_AUTO, "unlockables/nv_on.wav", 1, ATTN_NORM, 0, PITCH_NORM );


				pPlayer->ViewPunch( -24, 0.5, 0.5 );

				SetThink( &CCoFStartDoctorMode::GiveGun );
				SetNextThink( 1 );
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_startdoctormode, CCoFStartDoctorMode );

//=========================================================
// cof_stats
//=========================================================
class CCoFStats : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFStats::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFStats::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
		{
			int points = 0;
			int m_iGrade = 0;

			if ( pPlayer->playerstats.doctor_mode )
				points = CalculatePointsDoctor( pPlayer );
			else
				points = CalculatePoints( pPlayer );

			if ( pPlayer->playerstats.doctor_mode )
			{
				points = CalculatePointsDoctor( pPlayer );

				if ( points <= 2 )
					m_iGrade = 7;

				if ( points <= 4 && points > 2 )
					m_iGrade = 6;

				if ( points <= 10 && points > 4 )
					m_iGrade = 5;

				if ( points <= 14 && points > 10 )
					m_iGrade = 4;

				if ( points <= 16 && points > 14 )
					m_iGrade = 3;

				if ( points == 17 )
					m_iGrade = 2;

				if ( points >= 18 )
					m_iGrade = 1;
			}
			else
			{
				if ( points <= 4 )
					m_iGrade = 7;

				if ( points <= 8 && points > 4 )
					m_iGrade = 6;

				if ( points <= 16 && points > 12 )
					m_iGrade = 5;

				if ( points <= 20 && points > 16 )
					m_iGrade = 4;

				if ( points <= 24 && points > 20 )
					m_iGrade = 3;

				if ( points < 28 && points > 24 )
					m_iGrade = 2;

				if ( points >= 28 )
					m_iGrade = 1;
			}

			int ending6 = 0;
			int ending7 = 0;


			if ( pPlayer->playerstats.doctor_mode )
				ending6 = 1;


			if ( pPlayer->CheckingEndingOrUnlockables( 0, false, true ) )
				ending7 = 1;

			int unlockflags = 0;

			if ( pPlayer->CheckingEndingOrUnlockables( 1, false, false ) )
				unlockflags |= 1;
			if ( pPlayer->CheckingEndingOrUnlockables( 2, false, false ) )
				unlockflags |= 2;
			if ( pPlayer->CheckingEndingOrUnlockables( 4, false, false ) )
				unlockflags |= 4;
			if ( pPlayer->CheckingEndingOrUnlockables( 8, false, false ) )
				unlockflags |= 8;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x10, false, false ) )
				unlockflags |= 0x10;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x20, false, false ) )
				unlockflags |= 0x20;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x40, false, false ) )
				unlockflags |= 0x40;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x80, false, false ) )
				unlockflags |= 0x80;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x100, false, false ) )
				unlockflags |= 0x100;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x200, false, false ) )
				unlockflags |= 0x200;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x400, false, false ) )
				unlockflags |= 0x400;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x800, false, false ) )
				unlockflags |= 0x800;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x1000, false, false ) )
				unlockflags |= 0x1000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x2000, false, false ) )
				unlockflags |= 0x2000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x4000, false, false ) )
				unlockflags |= 0x4000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x8000, false, false ) )
				unlockflags |= 0x8000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x10000, false, false ) )
				unlockflags |= 0x10000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x20000, false, false ) )
				unlockflags |= 0x20000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x40000, false, false ) )
				unlockflags |= 0x40000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x80000, false, false ) )
				unlockflags |= 0x80000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x100000, false, false ) )
				unlockflags |= 0x100000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x200000, false, false ) )
				unlockflags |= 0x200000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x400000, false, false ) )
				unlockflags |= 0x400000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x800000, false, false ) )
				unlockflags |= 0x800000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x1000000, false, false ) )
				unlockflags |= 0x1000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x2000000, false, false ) )
				unlockflags |= 0x2000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x4000000, false, false ) )
				unlockflags |= 0x4000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x8000000, false, false ) )
				unlockflags |= 0x8000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x10000000, false, false ) )
				unlockflags |= 0x10000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x20000000, false, false ) )
				unlockflags |= 0x20000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x40000000, false, false ) )
				unlockflags |= 0x40000000;
			if ( pPlayer->CheckingEndingOrUnlockables( 0x80000000, false, false ) )
				unlockflags |= 0x80000000;


			if ( pPlayer->playerstats.started_from_the_beginning && pPlayer->playerstats.can_unlock_shit && pPlayer->playerstats.doctor_mode )
			{
				unlockflags = 0;

				if ( !pPlayer->CheckingEndingOrUnlockables( 0x1000000, false, false ) )
					pPlayer->SetEndingOrUnlockables( 0x1000000, false, false, false );

				unlockflags |= 0x1000000;
			}


			MESSAGE_BEGIN( MSG_ONE, gmsgCredits, NULL, pPlayer->pev );
				if ( pev->message )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				WRITE_STRING( "nah" );
				WRITE_SHORT( pPlayer->playerstats.shots_fired );
				WRITE_SHORT( pPlayer->playerstats.shots_hit );
				WRITE_SHORT( pPlayer->playerstats.syringes_used );
				WRITE_SHORT( pPlayer->playerstats.shots_missed );
				WRITE_COORD( pPlayer->CalculateAccuracy() );
				WRITE_SHORT( pPlayer->playerstats.timessaved );
				WRITE_SHORT( pPlayer->playerstats.weaponscollected );
				WRITE_SHORT( pPlayer->playerstats.damagetaken );
				WRITE_SHORT( pPlayer->playerstats.distancetravelled_metres );
				WRITE_SHORT( pPlayer->playerstats.distancetravelled_kilometres );
				WRITE_SHORT( pPlayer->playerstats.time_seconds );
				WRITE_SHORT( pPlayer->playerstats.time_minutes );
				WRITE_SHORT( pPlayer->playerstats.time_hours );
				WRITE_SHORT( pPlayer->playerstats.monsters_killed_melee );
				WRITE_SHORT( pPlayer->playerstats.monsters_killed_shooting );

				if ( pPlayer->playerstats.started_from_the_beginning && pPlayer->playerstats.can_unlock_shit )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				WRITE_BYTE( 30 );

				if ( pev->spawnflags & 1 )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				WRITE_BYTE( pPlayer->playerstats.chosen_difficulty );
				WRITE_LONG( unlockflags );
				WRITE_BYTE( 0 );
				WRITE_BYTE( m_iGrade );
				if ( pPlayer->playerstats.doctor_mode )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				WRITE_BYTE( 1 );


				if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );


				WRITE_BYTE( ending6 );
				WRITE_BYTE( ending7 );


				if ( pPlayer->CheckingEndingOrUnlockables( 0, true, false ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );


				if ( pPlayer->CheckingEndingOrUnlockables( 0, false, true ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );


				if ( pPlayer->playerstats.started_from_the_beginning && strstr( STRING(gpGlobals->mapname), "c_ending" ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
			MESSAGE_END();
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_stats, CCoFStats );

//=========================================================
// cof_spawnpointonoff
//=========================================================
class CCoFSpawnPointOnOff : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFSpawnPointOnOff::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFSpawnPointOnOff::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->message )
		return;

	CBaseEntity *pSpawn = UTIL_FindEntityByTargetname( NULL, STRING(pev->message) );

	if ( pSpawn )
	{
		pSpawn->pev->iuser1 = pev->iuser1;

		if ( pev->spawnflags & 1 )
		{
			if ( FClassnameIs( pSpawn->edict(), "cof_coop_spawn1" ) )
			{
				CBaseEntity *player = UTIL_PlayerByIndex( 1 );

				if ( player )
				{
					if ( player->IsPlayer() )
					{
						UTIL_SetOrigin( player, pSpawn->pev->origin );
						player->pev->angles = pSpawn->pev->angles;
						player->pev->fixangle = TRUE;
					}
				}
			}

			if ( FClassnameIs( pSpawn->edict(), "cof_coop_spawn2" ) )
			{
				CBaseEntity *player = UTIL_PlayerByIndex( 2 );

				if ( player )
				{
					if ( player->IsPlayer() )
					{
						UTIL_SetOrigin( player, pSpawn->pev->origin );
						player->pev->angles = pSpawn->pev->angles;
						player->pev->fixangle = TRUE;
					}
				}
			}

			if ( FClassnameIs( pSpawn->edict(), "cof_coop_spawn3" ) )
			{
				CBaseEntity *player = UTIL_PlayerByIndex( 3 );

				if ( player )
				{
					if ( player->IsPlayer() )
					{
						UTIL_SetOrigin( player, pSpawn->pev->origin );
						player->pev->angles = pSpawn->pev->angles;
						player->pev->fixangle = TRUE;
					}
				}
			}

			if ( FClassnameIs( pSpawn->edict(), "cof_coop_spawn4" ) )
			{
				CBaseEntity *player = UTIL_PlayerByIndex( 4 );

				if ( player )
				{
					if ( player->IsPlayer() )
					{
						UTIL_SetOrigin( player, pSpawn->pev->origin );
						player->pev->angles = pSpawn->pev->angles;
						player->pev->fixangle = TRUE;
					}
				}
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_spawnpointonoff, CCoFSpawnPointOnOff );

//=========================================================
// cof_coop_spawn1 to cof_coop_spawn4
//=========================================================
class CCoFCoopSpawn : public CPointEntity
{
	virtual void Spawn( void );
};

void CCoFCoopSpawn::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

LINK_ENTITY_TO_CLASS( cof_coop_spawn1, CCoFCoopSpawn );
LINK_ENTITY_TO_CLASS( cof_coop_spawn2, CCoFCoopSpawn );
LINK_ENTITY_TO_CLASS( cof_coop_spawn3, CCoFCoopSpawn );
LINK_ENTITY_TO_CLASS( cof_coop_spawn4, CCoFCoopSpawn );

//=========================================================
// cof_lobby_start
//=========================================================
class CCoFLobbyStart : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFLobbyStart::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFLobbyStart::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->message )
		return;

	if ( g_pLevelChange )
	{
		memset( g_pLevelChange->playervariablestruct, 0, sizeof( g_pLevelChange->playervariablestruct ) );
		memset( g_pLevelChange->playervariablestructstats, 0, sizeof( g_pLevelChange->playervariablestructstats ) );
	}



	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
	{
		CBasePlayer *pPlayer = GetClassPtr( (CBasePlayer *)player->pev );

		if ( pPlayer )
			FireTargets( STRING(pev->message), pPlayer, this, USE_TOGGLE, 0 );
	}
}

LINK_ENTITY_TO_CLASS( cof_lobby_start, CCoFLobbyStart );

//=========================================================
// cof_coop_stats
//=========================================================
class CCoFCoopStats : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void EXPORT RestartCampaign( void );
};

void CCoFCoopStats::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFCoopStats::RestartCampaign( void )
{
	if ( pev->message )
		CHANGE_LEVEL( (char *)STRING(pev->message), NULL );
	else
		CHANGE_LEVEL( "cof_campaign_01", NULL );
}

void CCoFCoopStats::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->spawnflags & 1 )
	{
		SetThink( &CCoFCoopStats::RestartCampaign );
		SetNextThink( 30 );
	}
	else
	{
		SetThink( &CCoFCoopStats::RestartCampaign );
		SetNextThink( 260 );
	}

	for ( int i = 1; i <= 4; i++ )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{



			pPlayer->SetEndingOrUnlockables( 0, false, false, true );

			MESSAGE_BEGIN( MSG_ALL, gmsgBeatCoop );
			MESSAGE_END();

			MESSAGE_BEGIN( MSG_ALL, gmsgCoopStats );
				WRITE_BYTE( ENTINDEX( pPlayer->edict() ) );
				WRITE_SHORT( pPlayer->playerstats.shots_fired );
				WRITE_SHORT( pPlayer->playerstats.shots_hit );
				WRITE_SHORT( pPlayer->playerstats.syringes_used );
				WRITE_SHORT( pPlayer->playerstats.shots_missed );
				WRITE_COORD( pPlayer->CalculateAccuracy() );
				//WRITE_SHORT( pPlayer->playerstats.timessaved );
				//WRITE_SHORT( pPlayer->playerstats.weaponscollected );
				WRITE_SHORT( pPlayer->playerstats.damagetaken );
				//WRITE_SHORT( pPlayer->playerstats.distancetravelled_metres );
				//WRITE_SHORT( pPlayer->playerstats.distancetravelled_kilometres );
				WRITE_SHORT( pPlayer->playerstats.time_seconds );
				WRITE_SHORT( pPlayer->playerstats.time_minutes );
				WRITE_SHORT( pPlayer->playerstats.time_hours );
				WRITE_SHORT( pPlayer->playerstats.monsters_killed_melee );
				WRITE_SHORT( pPlayer->playerstats.monsters_killed_shooting );
				WRITE_BYTE( 1 );
				if ( pPlayer->IsDeveloper() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				if ( pPlayer->IsDonator() )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );


				if ( pPlayer->CheckingEndingOrUnlockables( 0, true, false ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );


				if ( pPlayer->CheckingEndingOrUnlockables( 0, false, true ) )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				if ( g_iSurvivalMode )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				if ( g_iSurvivalMode )
					WRITE_SHORT( pPlayer->playerstats.suvivalpoints );
				else
					WRITE_SHORT( 0 );
				if ( pev->spawnflags & 1 )
					WRITE_BYTE( 1 );
				else
					WRITE_BYTE( 0 );
				if ( g_iSurvivalMode )
				{

					int overallpoints = 0;

					for ( int i = 1; i <= 4; i++ )
					{
						CBasePlayer *pPlayer = (CBasePlayer *)UTIL_PlayerByIndex( i );

						if ( pPlayer )
							overallpoints += pPlayer->playerstats.suvivalpoints;
					}

					WRITE_SHORT( overallpoints );
				}
				else
					WRITE_SHORT( 0 );
			MESSAGE_END();
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_coop_stats, CCoFCoopStats );

//=========================================================
// cof_introduction
//=========================================================
class CCoFIntroduction : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFIntroduction::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFIntroduction::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgIntrodn );
		WRITE_BYTE( pev->iuser1 );
		WRITE_BYTE( pev->iuser2 );

		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
			WRITE_BYTE( 1 );
		else
			WRITE_BYTE( 0 );
	MESSAGE_END();
}

LINK_ENTITY_TO_CLASS( cof_introduction, CCoFIntroduction );

//=========================================================
// cof_simpletext
//=========================================================
class CCoFSimpleText : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFSimpleText::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFSimpleText::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pev->message )
		return;

	bool all = false;


	if ( pev->spawnflags & 1 )
		all = true;

	CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

	if ( player )
		UTIL_ProMessage( player, STRING(pev->message), pev->iuser1, all );
}

LINK_ENTITY_TO_CLASS( cof_simpletext, CCoFSimpleText );

//=========================================================
// cof_survivalmode
//=========================================================
void CCoFSurvivalMode::Spawn( void )
{
	Precache();

	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFSurvivalMode::Precache( void )
{
	PRECACHE_SOUND( "common/survival_timeplus.wav" );
}

void CCoFSurvivalMode::SurvivalThink( void )
{
	if ( m_bSurvivalMode == true && m_bSurvivalModeRunning == true )
	{
		if ( pev->fuser1 < gpGlobals->time )
		{
			if ( m_iTimeSeconds == 60 )
			{
				m_iTimeSeconds = 0;
				m_iTimeMinutes++;
			}

			if ( m_iTimeMinutes == 60 )
			{
				m_iTimeMinutes = 0;
				m_iTimeHours++;
			}

			MESSAGE_BEGIN( MSG_ALL, gmsgTimer );
				WRITE_BYTE( 2 );
				WRITE_BYTE( m_iTimeSeconds );
				WRITE_BYTE( m_iTimeMinutes );
				WRITE_BYTE( m_iTimeHours );
				WRITE_SHORT( m_iSurvivalTimeInSeconds );
			MESSAGE_END();





			pev->fuser1 = gpGlobals->time + 3;
		}
	}

	if ( m_bSurvivalMode == true && m_bSurvivalModeRunning == true && !m_iSimonMode )
	{
		if ( m_flNextSurvivalCount < gpGlobals->time )
		{
			m_flNextSurvivalCount = gpGlobals->time + 1;

			if ( m_iCurrentSurvivalTime >= m_iSurvivalTimeInSeconds )
			{
				CBaseEntity *player = UTIL_FindEntityByClassname( NULL, "player" );

				if ( player && m_sSurvivalTriggerWhenWon && !pev->iuser3 )
				{
					m_bSurvivalModeRunning = false;

					CBaseEntity *pSurvivalEnt = NULL;
					pSurvivalEnt = UTIL_FindEntityByTargetname( NULL, STRING(m_sSurvivalTriggerWhenWon) );

					if ( pSurvivalEnt )
					{
						pev->iuser3 = 1;

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

						FireTargets( STRING(m_sSurvivalTriggerWhenWon), this, this, USE_TOGGLE, 0 );
					}
				}
			}
			else if ( !m_iSimonMode )
			{
				m_iCurrentSurvivalTime++;


				m_iTimeSeconds++;

				if ( m_iTimeSeconds == 60 )
				{
					m_iTimeSeconds = 0;
					m_iTimeMinutes++;
				}

				if ( m_iTimeMinutes == 60 )
				{
					m_iTimeMinutes = 0;
					m_iTimeHours++;
				}
			}
		}
	}

	SetNextThink( 0 );
}

void CCoFSurvivalMode::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( pev->iuser2 == 1 )
		return;

	if ( pev->target && pev->message )
	{
		m_bSurvivalModeRunning = true;
		m_bSurvivalMode = true;
		m_sSurvivalTriggerWhenWon = pev->message;
		m_sSurvivalTriggerWhenAllDead = pev->target;
		m_iSurvivalTimeInSeconds = pev->iuser1;
		m_iCurrentSurvivalTime = 0;
		pev->iuser2 = 1;
		SetThink( &CCoFSurvivalMode::SurvivalThink );
		SetNextThink( 0.1 );

		g_iSurvivalMode = 1;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				MESSAGE_BEGIN( MSG_ONE, gmsgTimer, NULL, plr->pev );
					WRITE_BYTE( 1 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_BYTE( 0 );
					WRITE_SHORT( m_iSurvivalTimeInSeconds );
				MESSAGE_END();
			}
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_survivalmode, CCoFSurvivalMode );

//=========================================================
// cof_coopgameover
//=========================================================
class CCoFCoopGameOver : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
};

void CCoFCoopGameOver::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFCoopGameOver::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBaseEntity *pSurvivalEntity = NULL;

	while ( ( pSurvivalEntity = UTIL_FindEntityByClassname( pSurvivalEntity, "cof_survivalmode" ) ) != NULL )
	{
		pSurvivalEntity->SetThink( NULL );
	}

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

		if ( plr && plr->pev->fuser4 == 0 )
		{
			plr->m_flDeadTime = gpGlobals->time + 10;
			plr->m_bIssuedGameOver = false;
			plr->m_bCoopGameOver = true;
			plr->m_flNextCountDown = gpGlobals->time;
			plr->m_iGameOverCounter = 10;

			plr->EnableControl( FALSE );
			plr->pev->flags |= FL_NOTARGET;

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

LINK_ENTITY_TO_CLASS( cof_coopgameover, CCoFCoopGameOver );

//=========================================================
// cof_randomtimedspawner
//=========================================================
class CCoFRandomTimedSpawner : public CPointEntity
{
public:
	virtual void Spawn( void );
	virtual void SendInitMessage( CBasePlayer *player );
	void EXPORT CheckIfDead( void );
	void EXPORT SpawnMonster( void );
	void EXPORT DelayStart( void );
};

void CCoFRandomTimedSpawner::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFRandomTimedSpawner::SendInitMessage( CBasePlayer *player )
{
	if ( !pev->message )
		return;

	if ( pev->iuser1 )
	{
		SetNextThink( pev->iuser1 );
		SetThink( &CCoFRandomTimedSpawner::DelayStart );
	}
	else
		DelayStart();
}

void CCoFRandomTimedSpawner::DelayStart( void )
{
	float mintime = pev->iuser1;
	float maxtime = pev->iuser2;
	float spawntime = maxtime - mintime;

	SetNextThink( RANDOM_FLOAT( 0, spawntime ) );
	SetThink( &CCoFRandomTimedSpawner::SpawnMonster );
}

void CCoFRandomTimedSpawner::SpawnMonster( void )
{
	edict_t *pEdict;
	int ents;
	CBaseEntity *pSpawnEnt[16];
	CBaseEntity *pEntity;
	ents = 0;

	pEdict = INDEXENT( 1 );

	for ( int i = 1; i < gpGlobals->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )
			continue;

		pEntity = CBaseEntity::Instance( pEdict );
		if ( !pEntity )
			continue;

		if ( FStrEq( STRING(pev->message), STRING(pEntity->pev->targetname) ) )
		{
			if ( ents < 16 )
			{
				pSpawnEnt[ents] = pEntity;
				ents++;
			}
		}
	}

	if ( ents > 0 )
	{
		int spawnat = RANDOM_LONG( 0, ents );

		if ( spawnat == ents )
			spawnat--;

		if ( pSpawnEnt[spawnat] && pev->target )
		{
			Vector origin = pSpawnEnt[spawnat]->pev->origin;
			Vector angles = pSpawnEnt[spawnat]->pev->angles;

			CBaseEntity *pCreate = CBaseEntity::Create( (char *)STRING(pev->target), origin, angles, NULL );

			if ( pCreate )
			{
				pev->euser1 = pCreate->edict();
				SetThink( &CCoFRandomTimedSpawner::CheckIfDead );
				SetNextThink( 0 );
			}
		}
	}
}

void CCoFRandomTimedSpawner::CheckIfDead( void )
{
	if ( pev->euser1 )
	{
		CBaseEntity *pEntity = CBaseEntity::Instance( pev->euser1 );

		if ( pEntity )
		{
			if ( !pEntity->IsAlive() )
			{



				if ( pev->iuser4 )
				{
					pev->iuser3++;

					if ( pev->iuser3 <= pev->iuser4 )
					{
						if ( pev->iuser1 )
						{
							SetNextThink( pev->iuser1 );
							SetThink( &CCoFRandomTimedSpawner::DelayStart );
						}
						else
						{
							SetThink( NULL );
							DelayStart();
							return;
						}
					}
				}
				else
				{
					if ( pev->iuser1 )
					{
						SetNextThink( pev->iuser1 );
						SetThink( &CCoFRandomTimedSpawner::DelayStart );
						return;
					}
					else
					{
						SetThink( NULL );
						DelayStart();
						return;
					}
				}
			}
		}
	}

	SetNextThink( 0.25 );
}

LINK_ENTITY_TO_CLASS( cof_randomtimedspawner, CCoFRandomTimedSpawner );

//=========================================================
// cof_customiseplayer
//=========================================================
class CCoFCustomisePlayerModels : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	int player1model;
	int player2model;
	int player3model;
	int player4model;
	int sleevetexture;
	int fingerstexture;
	int glovestexture_guess;
};

void CCoFCustomisePlayerModels::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "player1" ) )
	{
		player1model = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player2" ) )
	{
		player2model = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player3" ) )
	{
		player3model = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player4" ) )
	{
		player4model = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "sleeve" ) )
	{
		sleevetexture = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "fingers" ) )
	{
		fingerstexture = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "gloves" ) )
	{
		glovestexture_guess = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCoFCustomisePlayerModels::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFCustomisePlayerModels::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( g_pWorld )
	{
		if ( player1model )
			g_pWorld->player1model = player1model;

		if ( player2model )
			g_pWorld->player2model = player2model;

		if ( player3model )
			g_pWorld->player3model = player3model;

		if ( player4model )
			g_pWorld->player4model = player4model;

		if ( sleevetexture )
			g_pWorld->m_sCustomSleeve = sleevetexture;

		if ( fingerstexture )
			g_pWorld->m_sCustomFingers = fingerstexture;

		if ( glovestexture_guess )
			g_pWorld->m_sCustomGloves_guess = glovestexture_guess;


		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *plr = (CBasePlayer *)UTIL_PlayerByIndex( i );

			if ( plr )
			{
				if ( sleevetexture )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCustomSlv, NULL, plr->pev );
						WRITE_STRING( STRING(g_pWorld->m_sCustomSleeve) );
					MESSAGE_END();
				}

				if ( fingerstexture )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCustomFng, NULL, plr->pev );
						WRITE_STRING( STRING(g_pWorld->m_sCustomFingers) );
					MESSAGE_END();
				}

				if ( glovestexture_guess )
				{
					MESSAGE_BEGIN( MSG_ONE, gmsgCustomGlv, NULL, plr->pev );
						WRITE_STRING( STRING(g_pWorld->m_sCustomGloves_guess) );
					MESSAGE_END();
				}
			}

			if ( plr && g_pGameRules && g_pGameRules->IsMultiplayer() )
				g_pGameRules->ClientUserInfoChanged( plr, GET_PLAYER_INFOBUFFER( plr->edict() ) );
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_customiseplayer, CCoFCustomisePlayerModels );

//=========================================================
// cof_playercheck
//=========================================================
class CCoFPlayerCheck : public CPointEntity
{
	virtual void Spawn( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void KeyValue( KeyValueData *pkvd );
	int player1trigger;
	int player2trigger;
	int player3trigger;
	int player4trigger;
};

void CCoFPlayerCheck::KeyValue( KeyValueData *pkvd )
{
	if ( FStrEq( pkvd->szKeyName, "player1trigger" ) )
	{
		player1trigger = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player2trigger" ) )
	{
		player2trigger = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player3trigger" ) )
	{
		player3trigger = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq( pkvd->szKeyName, "player4trigger" ) )
	{
		player4trigger = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntity::KeyValue( pkvd );
}

void CCoFPlayerCheck::Spawn( void )
{
	UTIL_SetOrigin( this, pev->origin );
	pev->solid = SOLID_NOT;
	pev->effects = EF_NODRAW;
	pev->v_angle = g_vecZero;
}

void CCoFPlayerCheck::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int players = 0;

	if ( pev->spawnflags & 1 )
		players = CountConnectedLivePlayers();
	else
		players = CountConnectedPlayers();

	CBaseEntity *pPlayer = UTIL_FindEntityByClassname( NULL, "player" );

	if ( pPlayer )
	{
		switch ( players )
		{
		case 1:
			if ( player1trigger )
				FireTargets( STRING(player1trigger), pPlayer, pPlayer, USE_TOGGLE, 0 );
			break;

		case 2:
			if ( player2trigger )
				FireTargets( STRING(player2trigger), pPlayer, pPlayer, USE_TOGGLE, 0 );
			break;

		case 3:
			// original bug: tests player1trigger but fires player3trigger
			if ( player1trigger )
				FireTargets( STRING(player3trigger), pPlayer, pPlayer, USE_TOGGLE, 0 );
			break;

		case 4:
			if ( player4trigger )
				FireTargets( STRING(player4trigger), pPlayer, pPlayer, USE_TOGGLE, 0 );
			break;
		}
	}
}

LINK_ENTITY_TO_CLASS( cof_playercheck, CCoFPlayerCheck );


/*
// trigger_crash, debug testing entity

class CTriggerCrash : public CPointEntity
{
public:
	void Spawn()
	{
		pev->solid = SOLID_NOT;
		pev->movetype = MOVETYPE_NONE;
		pev->effects |= EF_NODRAW;
	}

	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		// this will cause crash
		int *ptr = 0;
		*ptr = 5;
	}			
};

LINK_ENTITY_TO_CLASS( trigger_crash, CTriggerCrash );*/