#include "cofr_legacy.h"
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
//
//  hud_msg.cpp
//

#include <string>
#include <list>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "mp3.h"
#include "rain.h"
#include "event_api.h"
#include "vgui_TeamFortressViewport.h"
#include "HUDControl_guess.h"
#include "RECBInventory_guess.h"
#include "Documents_guess.h"
#include "glmanager.h"
#include "CgWater_guess.h"
#include "..\game_shared\vgui_loadtga.h"
#include <direct.h>

//LRC - the fogging fog
float g_fFogColor[3];
float g_fStartDist;
float g_fEndDist;
//int g_iFinalStartDist; //for fading
int g_iFinalEndDist;   //for fading
float g_fFadeDuration; //negative = fading out

int iDifficulty_guess;				// "difficulty" cvar, read at InitHUD
int iGetUpTaps_guess;				// FallOver message, +jump presses needed to send "tapgetup"
int iGetUpTapCount_guess;			// +jump presses since the FallOver message
bool bCustomFingerTexture_guess;	// CustomFng message
bool bCustomGloveTexture_guess;		// CustomGlv message
bool bWheelchairLock_guess;			// Wheelchair message
bool bAttachCam_guess;		// cleared by AttachCam
int g_iDickMode_guess;
int iWheelchair_guess;				// Wheelchair message, the entity (0 when off)
int iBoat_guess;					// Boat message, the entity (0 when off)
cl_entity_t *pAttachCamEnt_guess;	// AttachCam message, the entity the view is attached to
int iAttachCamFlag_guess;			// AttachCam message
int iUnknown10542BC8_guess;			// AttachCam message
float flUnknown10542BCC_guess;		// AttachCam message
float flUnknown10542BD0_guess;		// AttachCam message
float flUnknown10542BD4_guess;		// AttachCam message

char com_token[ 1500 ];

vec3_t vecUnknown105431B4_guess;	// AttachCam message
vec3_t vecUnknown105431C0_guess;	// AttachCam message, the view angles
vec3_t vecUnknown105431CC_guess;	// AttachCam message, render_origin

#define MAX_CLIENTS 32

extern BEAM *pBeam;
extern BEAM *pBeam2;

extern "C" int iClientResetMaterials;

extern cl_entity_t *pCamTargetEnt_guess;
extern vec3_t vecWheelchairAngles_guess;
extern vec3_t vecCameraAttachment_guess;
extern vec3_t render_origin;
extern int iSkipSceneFramerate_guess;
extern int iGreyFade_guess;					// GreyFade message
extern int iUnknown104CC468_guess;			// cleared by the Concuss message, never read
extern bool bUnknown10559514_guess;			// cleared by the AttachCam message
extern bool g_bTakePhoto_guess;				// TakePhoto message, the next frame is captured
extern bool g_bHandsLoaded_guess;
extern bool g_bSleeveLoaded_guess;
extern bool g_bGloveLoaded_guess;
extern bool g_bPoliceSleeveLoaded_guess;

void MuzzleFlash_guess( cl_entity_t *pEntity, int iAttachment, bool bAlt, float flScale );

//=========================================================
// Declarations this file takes from the Steamworks SDK
// (steam_api.h, isteamfriends.h, steamclientpublic.h)
//=========================================================
typedef unsigned int uint32;
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

enum EUniverse
{
	k_EUniverseInvalid = 0,
	k_EUniversePublic = 1,
	k_EUniverseBeta = 2,
	k_EUniverseInternal = 3,
	k_EUniverseDev = 4,
	k_EUniverseMax
};

class CSteamID
{
public:
	CSteamID()
	{
		m_steamid.m_unAll64Bits = 0;
	}

	uint32 GetAccountID() const { return m_steamid.m_comp.m_unAccountID; }
	bool BClanAccount() const { return m_steamid.m_comp.m_EAccountType == k_EAccountTypeClan; }

private:
	union SteamID_t
	{
		struct SteamIDComponent_t
		{
			uint32			m_unAccountID : 32;
			unsigned int	m_unAccountInstance : 20;
			unsigned int	m_EAccountType : 4;
			EUniverse		m_EUniverse : 8;
		} m_comp;

		uint64 m_unAll64Bits;
	} m_steamid;
};

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
	virtual bool GetFriendGamePlayed( CSteamID steamIDFriend, void *pFriendGameInfo ) = 0;
	virtual const char *GetFriendPersonaNameHistory( CSteamID steamIDFriend, int iPersonaName ) = 0;
	virtual bool HasFriend( CSteamID steamIDFriend, int iFriendFlags ) = 0;
	virtual int GetClanCount() = 0;
	virtual CSteamID GetClanByIndex( int iClan ) = 0;
};

extern "C" __declspec( dllimport ) ISteamFriends * __cdecl SteamFriends();
//=========================================================

void IrrKlangPlayMP3_guess( const char *pszFile );
void IrrKlangPlaySFX_guess( const char *pszFile, int iType, bool bFlag, vec3_t vecOrigin );
void IrrKlangSetFade_guess( float flParm1, float flParm2, float flParm3, float flParm4 );
void IrrKlangSetLoop_guess( bool bLoop );
void IrrKlangSetStopping_guess( bool bStopping );
void IrrKlangStop_guess( float flFadeTime );

//
// COM_Parse, the same code as in multiplay_gamerules.cpp of the server
//
char *COM_Parse (char *data)
{
	int             c;
	int             len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}

// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);

	com_token[len] = 0;
	return data;
}

void GetCurrentPath( char *path )
{
	_getcwd( path, 260 );
}

char *CHud::CheckItem( char *pszName, char *pszFile )
{
	int gSCounter = 0;
	char szItemName[512];

	int length;
	char *pFile;
	char *aFile = pFile = (char *)gEngfuncs.COM_LoadFile( pszFile, 5, &length );

	if ( !pFile || !length )
		return "";

	while ( 1 )
	{
		pFile = COM_Parse( pFile );

		if ( strlen( com_token ) <= 0 )
			break;

		strcpy( szItemName, com_token );

		pFile = COM_Parse( pFile );

		if ( com_token[0] != '{' )
		{
			gEngfuncs.Con_Printf( " *\n" );
			gEngfuncs.Con_Printf( " * CheckItem parsing error: expecting { after key name\n" );
			gEngfuncs.Con_Printf( " *\n" );
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

				if ( !strcmp( szItemName, pszName ) )
				{
					gEngfuncs.COM_FreeFile( aFile );
					return com_token;
				}
			}
		}

		gSCounter++;
	}

	gEngfuncs.COM_FreeFile( aFile );
	return "";
}

// Steam clan 0x29D6CA
int IsInDonatorGroup1_guess( void )
{
	if ( SteamFriends() )
	{
		int iClans = SteamFriends()->GetClanCount();

		if ( SteamFriends()->GetClanCount() > 0 )
		{
			for ( int i = 0; i < iClans; i++ )
			{
				CSteamID clanID = SteamFriends()->GetClanByIndex( i );

				if ( clanID.BClanAccount() && clanID.GetAccountID() == 0x29D6CA )
					return 1;
			}
		}
	}

	return 0;
}

// Steam clan 0x46AE0A
int IsInDonatorGroup2_guess( void )
{
	if ( SteamFriends() )
	{
		int iClans = SteamFriends()->GetClanCount();

		if ( SteamFriends()->GetClanCount() > 0 )
		{
			for ( int i = 0; i < iClans; i++ )
			{
				CSteamID clanID = SteamFriends()->GetClanByIndex( i );

				if ( clanID.BClanAccount() && clanID.GetAccountID() == 0x46AE0A )
					return 1;
			}
		}
	}

	return 0;
}

/// USER-DEFINED SERVER MESSAGE HANDLERS

int CHud :: MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf )
{
	ASSERT( iSize == 0 );

	// clear all hud data
	HUDLIST *pList = m_pHudList;

	while ( pList )
	{
		if ( pList->p )
			pList->p->Reset();
		pList = pList->pNext;
	}

	pCamTargetEnt_guess = NULL;

	// reset sensitivity
	m_flMouseSensitivity = 0;

	// reset concussion effect
	m_iConcussionEffect = 0;

	m_flResetTime_guess = gEngfuncs.GetClientTime();
	m_flUnknown2A84_guess = 0;
	m_bUnknown2A88_guess = false;

	m_iFPSMin_guess = 0;
	m_iFPSMax_guess = 0;
	m_iFPSAverage_guess = 0;
	m_bFPSToggle_guess = false;
	m_flNextFPSTime_guess = gEngfuncs.GetClientTime();

	for ( int i = 1; i <= 100; i++ )
		m_iFPS_guess[i] = 0;

	m_iLadder_guess = 0;
	m_flUnknown2A9C_guess = 0;
	m_vecViewOfs_guess.x = m_vecViewOfs_guess.y = m_vecViewOfs_guess.z = 0;
	m_flLadderTime_guess = gEngfuncs.GetClientTime();

	//LRC - reset fog
	g_fStartDist = 0;
	g_fEndDist = 0;

	m_iFallOver_guess = 0;
	m_iMaglite_guess = 0;
	m_iUnreadSMS_guess = 0;
	m_flUnreadSMSTime_guess = gEngfuncs.GetClientTime();

	if ( gViewPort )
		gViewPort->m_flKeyInputTime_guess = gEngfuncs.GetClientTime();

	m_iViewEntity_guess = 0;
	m_flForwardTapTime_guess = gEngfuncs.GetClientTime();
	m_flBackTapTime_guess = gEngfuncs.GetClientTime();
	m_flLeftTapTime_guess = gEngfuncs.GetClientTime();
	m_flRightTapTime_guess = gEngfuncs.GetClientTime();
	m_BlackBar_guess.m_iOn_guess = 0;
	m_bViewLock_guess = false;

	if ( gViewPort && gViewPort->m_pRECBInventory_guess )
	{
		for ( int i = 1; i <= 6; i++ )
			GetSlotItem_guess( i )[0] = 0;

		gViewPort->m_pRECBInventory_guess->ClearItems_guess();
	}

	if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
	{
		gEngfuncs.Cvar_SetValue( "r_weather", 0 );
		gEngfuncs.Cvar_SetValue( "cg_water", 0 );
		gEngfuncs.Cvar_SetValue( "gl_bump", 0 );
		gEngfuncs.Cvar_SetValue( "gl_dynlight", 0 );
	}

	return 1;
}

void CAM_ToFirstPerson(void);

void CHud :: MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf )
{
	CAM_ToFirstPerson();
}

void CHud :: MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf )
{
//	CONPRINT("MSG:InitHUD");
	//LRC - clear the fog
	g_fStartDist = 0;
	g_fEndDist = 0;

	m_iMaglite_guess = 0;
	m_iUnreadSMS_guess = 0;
	m_flUnreadSMSTime_guess = gEngfuncs.GetClientTime();
	gViewPort->m_flKeyInputTime_guess = gEngfuncs.GetClientTime();
	m_iViewEntity_guess = 0;
	m_flForwardTapTime_guess = gEngfuncs.GetClientTime();
	m_flBackTapTime_guess = gEngfuncs.GetClientTime();
	m_flLeftTapTime_guess = gEngfuncs.GetClientTime();
	m_flRightTapTime_guess = gEngfuncs.GetClientTime();

	iDifficulty_guess = gEngfuncs.pfnGetCvarFloat( "difficulty" );

	m_flResetTime_guess = gEngfuncs.GetClientTime();
	m_flUnknown2A84_guess = 0;
	m_bUnknown2A88_guess = false;
	m_bViewLock_guess = false;

	m_iSkyMode = SKY_OFF; //LRC

	// prepare all hud data
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if ( pList->p )
			pList->p->InitHUDData();
		pList = pList->pNext;
	}

	//Probably not a good place to put this.
	pBeam = pBeam2 = NULL;

	if ( g_TextureManager_guess.IsHardware_guess() )
	{
		g_TextureManager_guess.LevelInit_guess();
		g_TextureManager_guess.PrecachePlayerShadow_guess();
		g_CgContext_guess.Shutdown_guess();
		g_CgContext_guess.Startup_guess();
		g_CgWater_guess.Init_guess();
	}
}

//LRC
void CHud :: MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf )
{
//	CONPRINT("MSG:SetFog");
	BEGIN_READ( pbuf, iSize );

	for ( int i = 0; i < 3; i++ )
		 g_fFogColor[ i ] = READ_BYTE();

	g_fFadeDuration = READ_SHORT();
	g_fStartDist = READ_SHORT();

	if (g_fFadeDuration > 0)
	{
//		// fading in
//		g_fStartDist = READ_SHORT();
		g_iFinalEndDist = READ_SHORT();
//		g_fStartDist = FOG_LIMIT;
		g_fEndDist = FOG_LIMIT;
	}
	else if (g_fFadeDuration < 0)
	{
//		// fading out
//		g_iFinalStartDist =
		g_iFinalEndDist = g_fEndDist = READ_SHORT();
	}
	else
	{
//		g_fStartDist = READ_SHORT();
		g_fEndDist = READ_SHORT();
	}
}

//LRC
void CHud :: MsgFunc_KeyedDLight( const char *pszName, int iSize, void *pbuf )
{
//	CONPRINT("MSG:KeyedDLight");
	BEGIN_READ( pbuf, iSize );

// as-yet unused:
//	float	decay;				// drop this each second
//	float	minlight;			// don't add when contributing less
//	qboolean	dark;			// subtracts light instead of adding (doesn't seem to do anything?)

	int iKey = READ_BYTE();
	dlight_t *dl = gEngfuncs.pEfxAPI->CL_AllocDlight( iKey );

	int bActive = READ_BYTE();
	if (!bActive)
	{
		// die instantly
		dl->die = gEngfuncs.GetClientTime();
	}
	else
	{
		// never die
		dl->die = gEngfuncs.GetClientTime() + 1E6;

		dl->origin[0] = READ_COORD();
		dl->origin[1] = READ_COORD();
		dl->origin[2] = READ_COORD();
		dl->radius = READ_BYTE();
		dl->color.r = READ_BYTE();
		dl->color.g = READ_BYTE();
		dl->color.b = READ_BYTE();
	}
}

//LRC
void CHud :: MsgFunc_AddShine( const char *pszName, int iSize, void *pbuf )
{
}

//LRC
void CHud :: MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf )
{
//	CONPRINT("MSG:SetSky");
	BEGIN_READ( pbuf, iSize );

	m_iSkyMode = READ_BYTE();
	m_vecSkyPos.x = READ_COORD();
	m_vecSkyPos.y = READ_COORD();
	m_vecSkyPos.z = READ_COORD();
}

int CHud :: MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_Teamplay = READ_BYTE();

	return 1;
}


int CHud :: MsgFunc_Damage(const char *pszName, int iSize, void *pbuf )
{
	int		armor, blood;
	Vector	from;
	int		i;
	float	count;

	BEGIN_READ( pbuf, iSize );
	armor = READ_BYTE();
	blood = READ_BYTE();

	for (i=0 ; i<3 ; i++)
		from[i] = READ_COORD();

	count = (blood * 0.5) + (armor * 0.5);

	if (count < 10)
		count = 10;

	// TODO: kick viewangles,  show damage visually

	return 1;
}

int CHud :: MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iConcussionEffect = READ_BYTE();

	if ( READ_BYTE() )
	{
		m_bConcussed_guess = true;
	}
	else
	{
		m_bConcussed_guess = false;
		iUnknown104CC468_guess = 0;
	}

	return 1;
}

// buz: gasmask message
int CHud :: MsgFunc_GasMask( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iGasMask = READ_BYTE();
//	m_flNextBreathTime = m_flTime + gEngfuncs.pfnRandomFloat( 2, 5 );
	return 1;
}

// buz: special tank message
int CHud :: MsgFunc_SpecTank( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_SpecTank_on = READ_BYTE();
	if (m_SpecTank_on == 0) // turn off
		return 1;
	else if (m_SpecTank_on == 2) // only ammo update
	{
		m_SpecTank_Ammo = READ_LONG();
		m_Ammo.m_fFade = 200.0f;
	}
	else // turn on
	{
		m_SpecTank_point.x = READ_COORD();
		m_SpecTank_point.y = READ_COORD();
		m_SpecTank_point.z = READ_COORD();
		m_SpecTank_defYaw = READ_COORD();
		m_SpecTank_coneHor = READ_COORD();
		m_SpecTank_coneVer = READ_COORD();
		m_SpecTank_distFwd = READ_COORD();
		m_SpecTank_distUp = READ_COORD();
		m_SpecTank_Ammo = READ_LONG();
	}
	return 1;
}


int CHud :: MsgFunc_PlayMP3( const char *pszName, int iSize, void *pbuf ) //AJH -Killar MP3
{
	char szFile[256];

	BEGIN_READ( pbuf, iSize );

	int iLoop = READ_BYTE();
	int iPlay = READ_BYTE();
	float flFadeTime = READ_COORD();
	strncpy( szFile, READ_STRING(), 256 );

	if ( gEngfuncs.pfnGetCvarFloat( "cl_downwiththesickness" ) == 1 )
		strncpy( szFile, "downwiththesickness.mp3", 256 );

	// turn on or off?
	if ( iPlay )
	{
		if ( flFadeTime != 0 )
			IrrKlangSetFade_guess( -1, -1, gEngfuncs.GetClientTime(), flFadeTime );

		if ( iLoop )
			IrrKlangSetLoop_guess( true );
		else
			IrrKlangSetLoop_guess( false );

		IrrKlangSetStopping_guess( false );
		IrrKlangPlayMP3_guess( szFile );
	}
	else
	{
		IrrKlangSetStopping_guess( true );
		IrrKlangStop_guess( flFadeTime );
	}

	return 1;
}


// rain
extern rain_properties Rain;

int CHud :: MsgFunc_RainData( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
		Rain.dripsPerSecond =	READ_SHORT();
		Rain.distFromPlayer =	READ_COORD();
		Rain.windX =			READ_COORD();
		Rain.windY =			READ_COORD();
		Rain.randX =			READ_COORD();
		Rain.randY =			READ_COORD();
		Rain.weatherMode =		READ_SHORT();
		Rain.globalHeight =		READ_COORD();

	return 1;
}

int CHud :: MsgFunc_ProFont( const char *pszName, int iSize, void *pbuf )
{
	char szText[256];
	char szRespawn[128];

	BEGIN_READ( pbuf, iSize );

	int iType = READ_BYTE();
	strncpy( szText, READ_STRING(), 256 );

	if ( strstr( szText, "DEADNOTIFY" ) )
	{
		if ( gHUD.GetKeyForCommand_guess( "+jump" ) )
			sprintf( szRespawn, "Press %s to respawn (lose weapons) or wait to be revived", gHUD.GetKeyForCommand_guess( "+jump" ) );
		else
			strcpy( szRespawn, "Press jump to respawn (lose weapons) or wait to be revived" );

		strncpy( szText, szRespawn, 256 );
	}

	if ( gViewPort && gViewPort->m_pHUDControl_guess )
	{
		strcpy( gViewPort->m_pHUDControl_guess->m_szMessage_guess, szText );
		gViewPort->m_pHUDControl_guess->m_flMessageAlpha_guess = 255;
		gViewPort->m_pHUDControl_guess->m_bMessageFadeOut_guess = false;
		gViewPort->m_pHUDControl_guess->m_bMessageHold_guess = false;
		gViewPort->m_pHUDControl_guess->m_flMessageHoldTime_guess = 0;
		gViewPort->m_pHUDControl_guess->m_iMessageType_guess = iType;
	}

	return 1;
}

int CHud :: MsgFunc_Maglite( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iMaglite_guess = READ_BYTE();
	return 1;
}

int CHud :: MsgFunc_Blur( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iBlur_guess = READ_BYTE();
	return 1;
}

int CHud :: MsgFunc_UnreadSMS( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iUnreadSMS_guess = READ_BYTE();
	int iBatteryLow = READ_BYTE();

	if ( gViewPort && gViewPort->m_pHUDControl_guess )
	{
		if ( !gHUD.m_iUnreadSMS_guess )
			gViewPort->m_pHUDControl_guess->ShowSMSIcon_guess( false );

		if ( gHUD.m_iUnreadSMS_guess )
		{
			if ( iBatteryLow )
				gViewPort->m_pHUDControl_guess->m_pSMSIcon_guess->setImage( vgui_LoadTGA( "gfx/vgui/phone_battery_low.tga" ) );
			else
				gViewPort->m_pHUDControl_guess->m_pSMSIcon_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_hudelem_smsicon.tga" ) );

			gViewPort->m_pHUDControl_guess->ShowSMSIcon_guess( true );
		}
	}

	return 1;
}

int CHud :: MsgFunc_ViewKnock( const char *pszName, int iSize, void *pbuf )
{
	vec3_t angles;
	gEngfuncs.GetViewAngles( angles );

	BEGIN_READ( pbuf, iSize );

	float flKnock = READ_BYTE();
	angles[0] -= flKnock;

	if ( !gHUD.m_bViewLock_guess )
		gEngfuncs.SetViewAngles( angles );

	if ( iWheelchair_guess )
		vecWheelchairAngles_guess[0] -= flKnock;

	return 1;
}

int CHud :: MsgFunc_CoFWeapon( const char *pszName, int iSize, void *pbuf )
{
	char szWeapon[128];

	BEGIN_READ( pbuf, iSize );

	READ_BYTE();
	strcpy( szWeapon, READ_STRING() );

	return 1;
}

int CHud :: MsgFunc_ViewEntity( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iViewEntity_guess = READ_SHORT();
	return 1;
}

int CHud :: MsgFunc_ViewLock( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iLock = READ_BYTE();

	if ( READ_COORD() == 666 )
		m_flViewLockTime_guess = 666;
	else
		m_flViewLockTime_guess = gEngfuncs.GetClientTime() + READ_COORD();

	if ( m_flViewLockTime_guess > 0 )
		m_bViewLockWasOn_guess = ( m_bViewLock_guess != 0 );
	else
		m_bViewLockWasOn_guess = false;

	m_bViewLock_guess = ( iLock != 0 );

	return 1;
}

int CHud :: MsgFunc_Ladder( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iLadder_guess = READ_BYTE();
	m_vecLadder_guess.x = READ_COORD();
	m_vecLadder_guess.y = READ_COORD();
	m_vecLadder_guess.z = READ_COORD();
	m_flLadderTime_guess = gEngfuncs.GetClientTime();

	return 1;
}

int CHud :: MsgFunc_FallOver( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iFallOver_guess = READ_BYTE();
	iGetUpTaps_guess = READ_BYTE();
	iGetUpTapCount_guess = 0;
	m_flLadderTime_guess = gEngfuncs.GetClientTime();

	return 1;
}

int CHud :: MsgFunc_BlackWhite( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iBlackWhite_guess = READ_BYTE();
	return 1;
}

int CHud :: MsgFunc_WeaponBody( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iBody = READ_BYTE();
	cl_entity_t *view = gEngfuncs.GetViewModel();

	if ( view )
		view->curstate.body = iBody;

	return 1;
}

int CHud :: MsgFunc_PhoneOff( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	if ( READ_BYTE() )
		m_bPhoneOff_guess = true;
	else
		m_bPhoneOff_guess = false;
	return 1;
}

int CHud :: MsgFunc_Muzzleflash( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iEntity = READ_SHORT();
	int iAttachment = READ_BYTE();
	cl_entity_t *pEntity = gEngfuncs.GetEntityByIndex( iEntity );

	if ( pEntity )
		MuzzleFlash_guess( pEntity, iAttachment, false, 1.0 );

	return 1;
}

int CHud :: MsgFunc_FlashFlags( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iFlashFlags_guess = READ_SHORT();
	return 1;
}

int CHud :: MsgFunc_Boat( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iEntity = READ_SHORT();

	if ( iEntity <= 0 )
		iBoat_guess = 0;
	else
		iBoat_guess = (int)gEngfuncs.GetEntityByIndex( iEntity );

	return 1;
}

int CHud :: MsgFunc_Ripple( const char *pszName, int iSize, void *pbuf )
{
	vec3_t origin;

	BEGIN_READ( pbuf, iSize );

	origin.x = READ_COORD();
	origin.y = READ_COORD();
	origin.z = READ_COORD();

	RippleEffect_guess( origin, READ_SHORT() );

	return 1;
}

int CHud :: MsgFunc_AddNote( const char *pszName, int iSize, void *pbuf )
{
	char szCode[32];
	char szDigit1[128];
	char szDigit2[128];
	char szDigit3[128];
	char szDigit4[128];
	int iDigit[32];

	BEGIN_READ( pbuf, iSize );

	int iNote = READ_BYTE();
	int iHidden = READ_BYTE();
	int iCode = READ_SHORT();
	int iCodeSource = READ_BYTE();

	if ( iNote < 1 || iNote > 16 )
		return 1;

	if ( iHidden && gViewPort && gViewPort->m_pDocuments_guess )
		gViewPort->m_pDocuments_guess->m_iHidden_guess[iNote] = 1;
	else
		gViewPort->m_pDocuments_guess->m_iHidden_guess[iNote] = 0;

	strcpy( m_Notes_guess[iNote].szLine_guess[4], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[5], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[6], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[7], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[0], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[1], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[2], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szLine_guess[3], READ_STRING() );
	strcpy( m_Notes_guess[iNote].szText1_guess, READ_STRING() );
	strcpy( m_Notes_guess[iNote].szText2_guess, READ_STRING() );
	strcpy( m_Notes_guess[iNote].szText3_guess, READ_STRING() );

	if ( iCode && iCodeSource )
	{
		if ( !gViewPort )
			return 1;

		if ( gViewPort->m_pDocuments_guess )
		{
			switch ( iCodeSource )
			{
			case 1:
				gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setImage( vgui_LoadTGA( "gfx/vgui/notes/codefrompc.tga" ) );
				break;
			case 2:
				gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setImage( vgui_LoadTGA( "gfx/vgui/notes/codefromvideotape.tga" ) );
				break;
			case 3:
				gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setImage( vgui_LoadTGA( "gfx/vgui/notes/codefromwindow.tga" ) );
				break;
			case 4:
				gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setImage( vgui_LoadTGA( "gfx/vgui/notes/phonecode.tga" ) );
				break;
			}

			sprintf( szCode, "%i", iCode );

			for ( unsigned int i = 0; i < strlen( szCode ); i++ )
				iDigit[i] = szCode[i] - '0';

			if ( iDigit[0] >= 0 && iDigit[0] <= 9 )
				sprintf( szDigit1, "gfx/vgui/notes/code_%i.tga", iDigit[0] );
			else
				strcpy( szDigit1, "gfx/vgui/notes/code_1.tga" );

			if ( iDigit[1] >= 0 && iDigit[1] <= 9 )
				sprintf( szDigit2, "gfx/vgui/notes/code_%i.tga", iDigit[1] );
			else
				strcpy( szDigit2, "gfx/vgui/notes/code_1.tga" );

			if ( iDigit[2] >= 0 && iDigit[2] <= 9 )
				sprintf( szDigit3, "gfx/vgui/notes/code_%i.tga", iDigit[2] );
			else
				strcpy( szDigit3, "gfx/vgui/notes/code_1.tga" );

			if ( iDigit[3] >= 0 && iDigit[3] <= 9 )
				sprintf( szDigit4, "gfx/vgui/notes/code_%i.tga", iDigit[3] );
			else
				strcpy( szDigit4, "gfx/vgui/notes/code_1.tga" );

			gViewPort->m_pDocuments_guess->m_pCodeDigit1_guess[iNote]->setImage( vgui_LoadTGA( szDigit1 ) );
			gViewPort->m_pDocuments_guess->m_pCodeDigit2_guess[iNote]->setImage( vgui_LoadTGA( szDigit2 ) );
			gViewPort->m_pDocuments_guess->m_pCodeDigit3_guess[iNote]->setImage( vgui_LoadTGA( szDigit3 ) );
			gViewPort->m_pDocuments_guess->m_pCodeDigit4_guess[iNote]->setImage( vgui_LoadTGA( szDigit4 ) );

			int x, y;
			int noteWide, noteTall;
			int digitWide, digitTall;
			gViewPort->m_pDocuments_guess->m_pCodeDigit1_guess[iNote]->getPos( x, y );
			gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->getSize( noteWide, noteTall );
			gViewPort->m_pDocuments_guess->m_pCodeDigit1_guess[iNote]->getSize( digitWide, digitTall );

			gViewPort->m_pDocuments_guess->m_pCodeDigit1_guess[iNote]->setPos( x + noteWide, y );
			gViewPort->m_pDocuments_guess->m_pCodeDigit2_guess[iNote]->setPos( x + noteWide + digitWide, y );
			gViewPort->m_pDocuments_guess->m_pCodeDigit3_guess[iNote]->setPos( x + noteWide + digitWide * 2, y );
			gViewPort->m_pDocuments_guess->m_pCodeDigit4_guess[iNote]->setPos( x + noteWide + digitWide * 3, y );

			gViewPort->m_pDocuments_guess->m_pCodeDigit1_guess[iNote]->setVisible( true );
			gViewPort->m_pDocuments_guess->m_pCodeDigit2_guess[iNote]->setVisible( true );
			gViewPort->m_pDocuments_guess->m_pCodeDigit3_guess[iNote]->setVisible( true );
			gViewPort->m_pDocuments_guess->m_pCodeDigit4_guess[iNote]->setVisible( true );
		}
	}

	if ( gViewPort && gViewPort->m_pDocuments_guess && !iHidden )
	{
		gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setImage( vgui_LoadTGA( m_Notes_guess[iNote].szText3_guess ) );
		gViewPort->m_pDocuments_guess->m_pNote_guess[iNote]->setVisible( true );
	}

	return 1;
}

int CHud :: MsgFunc_AttachCam( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iEntity = READ_SHORT();
	iAttachCamFlag_guess = READ_BYTE();
	iUnknown10542BC8_guess = READ_BYTE();
	vecUnknown105431B4_guess.x = READ_LONG();
	vecUnknown105431B4_guess.y = READ_LONG();
	vecUnknown105431B4_guess.z = READ_LONG();

	iUnknown10542BC8_guess = 1;
	vecUnknown105431B4_guess = render_origin;
	flUnknown10542BCC_guess = 2;
	flUnknown10542BD4_guess = 2;
	flUnknown10542BD0_guess = 8.5;
	bAttachCam_guess = false;
	bUnknown10559514_guess = false;

	if ( iEntity <= 0 )
	{
		if ( pAttachCamEnt_guess )
		{
			iUnknown10542BC8_guess = 2;
			vecUnknown105431B4_guess = vecCameraAttachment_guess;
			flUnknown10542BCC_guess = 2.5;
		}

		pAttachCamEnt_guess = NULL;
	}
	else
	{
		pAttachCamEnt_guess = gEngfuncs.GetEntityByIndex( iEntity );
		vecUnknown105431CC_guess = render_origin;
		gEngfuncs.GetViewAngles( vecUnknown105431C0_guess );
	}

	return 1;
}

int CHud :: MsgFunc_BigPrick( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	READ_BYTE();
	return 1;
}

int CHud :: MsgFunc_NewFade( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_iFadeR_guess = READ_BYTE();
	m_iFadeG_guess = READ_BYTE();
	m_iFadeB_guess = READ_BYTE();
	m_iFadeAlpha_guess = READ_BYTE();
	m_flFadeStart_guess = gEngfuncs.GetClientTime();
	m_flFadeTime_guess = READ_COORD();

	int iFadeOut = READ_BYTE();
	int iHold = READ_BYTE();

	m_flFadeEnd_guess = gEngfuncs.GetClientTime() + READ_COORD();
	if ( iFadeOut )
		m_bFadeOut_guess = true;
	else
		m_bFadeOut_guess = false;
	if ( iHold )
		m_bFadeHold_guess = true;
	else
		m_bFadeHold_guess = false;

	return 1;
}

int CHud :: MsgFunc_SkipScene( const char *pszName, int iSize, void *pbuf )
{
	if ( IsViewEntityActive_guess() )
	{
		if ( gEngfuncs.pfnGetCvarFloat( "host_framerate" ) == 0 )
		{
			if ( strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
			{
				if ( !strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
					gEngfuncs.Cvar_SetValue( "host_framerate", iSkipSceneFramerate_guess );
			}
		}
	}

	return 1;
}

int CHud :: MsgFunc_IronSights( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	m_vecIronSightOfs_guess.x = READ_COORD();
	m_vecIronSightOfs_guess.y = READ_COORD();
	m_vecIronSightOfs_guess.z = READ_COORD();
	READ_BYTE();

	if ( READ_BYTE() )
	{
		m_bIronSights_guess = true;
	}
	else
	{
		m_vecIronSightOfs_guess.x = 0;
		m_vecIronSightOfs_guess.y = 0;
		m_vecIronSightOfs_guess.z = 0;
		m_bIronSights_guess = false;
	}

	return 1;
}

int CHud :: MsgFunc_Wheelchair( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iEntity = READ_SHORT();

	if ( READ_BYTE() )
	{
		bWheelchairLock_guess = true;
		return 1;
	}

	iWheelchair_guess = ( iEntity <= 0 ) ? 0 : (int)gEngfuncs.GetEntityByIndex( iEntity );
	vecWheelchairAngles_guess[1] = 0;
	vecWheelchairAngles_guess[0] = 0;

	return 1;
}

int CHud :: MsgFunc_GreyFade( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iFadeIn = READ_BYTE();
	iGreyFade_guess = READ_BYTE();
	m_flGreyFadeStart_guess = 0;
	m_flGreyFadeEnd_guess = 0;

	if ( iFadeIn )
		m_flGreyFadeEnd_guess = gEngfuncs.GetClientTime();
	else
		m_flGreyFadeStart_guess = gEngfuncs.GetClientTime();

	return 1;
}

int CHud :: MsgFunc_TakePhoto( const char *pszName, int iSize, void *pbuf )
{
	g_bTakePhoto_guess = true;
	if(iSize==0)CoFR_CameraFlash();
	return 1;
}

int CHud :: MsgFunc_Objectives( const char *pszName, int iSize, void *pbuf )
{
	char szText[256];

	BEGIN_READ( pbuf, iSize );

	strcpy( gHUD.m_szObjectiveTitle_guess, READ_STRING() );
	strcpy( gHUD.m_szObjectiveText_guess, READ_STRING() );

	if ( READ_BYTE() && strlen( gHUD.m_szObjectiveTitle_guess ) > 2 && strlen( gHUD.m_szObjectiveText_guess ) > 2 )
	{
		ParseHintTokens( gHUD.m_szObjectiveTitle_guess, gHUD.m_szObjectiveText_guess );

		gViewPort->m_pHUDControl_guess->m_iHintLine_guess = 1;

		if ( strlen( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess ) > 2 )
		{
			int iLen = 0;
			for ( unsigned int i = 0; i < strlen( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess ); i++ )
			{
				if ( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess[i] == '@' )
				{
					iLen++;
					szText[i] = ' ';
				}
				else
				{
					iLen++;
					szText[i] = gViewPort->m_pHUDControl_guess->m_szHintLine1_guess[i];
				}
			}
			szText[iLen] = '\0';

			int w, h;
			gViewPort->m_pHUDControl_guess->m_pHintLabel_guess->setText( szText );
			gViewPort->m_pHUDControl_guess->m_pCreditsFont_guess->getTextSize( gViewPort->m_pHUDControl_guess->m_szHintLine1_guess, w, h );
			gViewPort->m_pHUDControl_guess->m_pHintLabel_guess->setPos( ScreenWidth / 2 - 400, -48 );
			gViewPort->m_pHUDControl_guess->m_bHint_guess = true;
			gViewPort->m_pHUDControl_guess->m_iHintAlpha_guess = 255;
			gViewPort->m_pHUDControl_guess->m_flHintTime_guess = gEngfuncs.GetClientTime() + 5;
		}
	}

	return 1;
}

int CHud :: MsgFunc_Unlocks( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	m_iUnlocks_guess = READ_LONG();
	return 1;
}

int CHud :: MsgFunc_CustomSlv( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szSleeveTexture_guess, READ_STRING(), 256 );

	if ( strlen( m_szSleeveTexture_guess ) <= 2 )
		m_szSleeveTexture_guess[0] = 0;

	g_bSleeveLoaded_guess = false;
	g_bPoliceSleeveLoaded_guess = false;

	return 1;
}

int CHud :: MsgFunc_CustomFng( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szFingerTexture_guess, READ_STRING(), 256 );
	bCustomFingerTexture_guess = true;

	if ( strlen( m_szFingerTexture_guess ) <= 2 )
	{
		bCustomFingerTexture_guess = false;
		m_szFingerTexture_guess[0] = 0;
	}

	g_bHandsLoaded_guess = false;

	return 1;
}

int CHud :: MsgFunc_CustomGlv( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	strncpy( m_szGloveTexture_guess, READ_STRING(), 256 );
	bCustomGloveTexture_guess = true;

	if ( strlen( m_szGloveTexture_guess ) <= 2 )
	{
		bCustomGloveTexture_guess = false;
		m_szGloveTexture_guess[0] = 0;
	}

	g_bGloveLoaded_guess = false;

	return 1;
}

int CHud :: MsgFunc_PlaySFX( const char *pszName, int iSize, void *pbuf )
{
	char szName[256];
	char szSound[256];

	BEGIN_READ( pbuf, iSize );

	char *pszSound = READ_STRING();
	int iType = READ_BYTE();
	int iFlag = READ_BYTE();
	float x = READ_COORD();
	float y = READ_COORD();
	float z = READ_COORD();
	int iEngine = READ_BYTE();
	READ_SHORT();

	vec3_t origin = Vector( x, y, z );

	if ( iEngine )
	{
		if ( strstr( pszSound, "weapons_new/" ) )
		{
			bool bFound = false;
			int j = 0;

			for ( unsigned int i = 0; i < strlen( pszSound ); i++ )
			{
				if ( bFound )
					szName[j++] = pszSound[i];
				else if ( pszSound[i] == '/' || pszSound[i] == '\\' )
					bFound = true;
			}

			if ( bFound )
			{
				szName[j] = '\0';
				strcpy( szSound, "weapons/" );
				strcat( szSound, szName );
				pszSound = szSound;
			}
		}

		if ( strstr( pszSound, "fire" ) || strstr( pszSound, "shoot" ) )
			gEngfuncs.pEventAPI->EV_PlaySound( 0, origin, CHAN_WEAPON, pszSound, 1.0, 0.8, 0, PITCH_NORM );
		else
			gEngfuncs.pEventAPI->EV_PlaySound( 0, origin, CHAN_AUTO, pszSound, 1.0, 0.8, 0, PITCH_NORM );

		return 1;
	}
	else
	{
		bool bFlag = false;
		if ( iFlag )
			bFlag = true;
		IrrKlangPlaySFX_guess( pszSound, iType, bFlag, origin );

		return 1;
	}
}

int CHud :: MsgFunc_Inventory( const char *pszName, int iSize, void *pbuf )
{
	char szItem[128];
	char szText[128];

	BEGIN_READ( pbuf, iSize );

	int iSlot = READ_BYTE();
	int iReset = READ_BYTE();
	strncpy( szItem, READ_STRING(), 128 );
	int iThree = READ_BYTE();
	int iSilent = READ_BYTE();
	int iTactical = READ_BYTE();

	strcpy( GetSlotItem_guess( iSlot ), szItem );

	if ( !gViewPort || !gViewPort->m_pRECBInventory_guess )
		return 1;

	if ( iThree )
	{
		if ( gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[3]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[3]->setVisible( false );
		if ( gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[4]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[4]->setVisible( false );
		if ( gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[5]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[5]->setVisible( false );

		if ( gViewPort->m_pRECBInventory_guess->m_iThree_guess == 0 )
		{
			gViewPort->m_pRECBInventory_guess->m_pInventory_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_inventory_three.tga" ) );
			gViewPort->m_pRECBInventory_guess->m_iThree_guess = 1;
		}
	}
	else
	{
		if ( !gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[3]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[3]->setVisible( true );
		if ( !gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[4]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[4]->setVisible( true );
		if ( !gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[5]->isVisible() )
			gViewPort->m_pRECBInventory_guess->m_pItemButtons_guess[5]->setVisible( true );

		if ( gViewPort->m_pRECBInventory_guess->m_iThree_guess == 1 )
		{
			gViewPort->m_pRECBInventory_guess->m_pInventory_guess->setImage( vgui_LoadTGA( "gfx/vgui/640_inventory.tga" ) );
			gViewPort->m_pRECBInventory_guess->m_iThree_guess = 0;
		}
	}

	if ( iReset )
		gViewPort->m_pRECBInventory_guess->ClearItems_guess();

	if ( iSlot )
	{
		if ( strlen( gHUD.GetSlotItem_guess( iSlot ) ) > 1 )
		{
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[iSlot - 1]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", GetSlotItem_guess( iSlot ) ) ) );

			char *pszNiceName = CheckItem( "nice_name", szItem );
			if ( pszNiceName )
			{
				int iNotBook = strcmp( pszNiceName, "Simon's Book" );
				sprintf( szText, iNotBook ? "You got the %s" : "You got %s", CheckItem( "nice_name", szItem ) );

				if ( gViewPort && gViewPort->m_pHUDControl_guess )
				{
					gViewPort->m_pHUDControl_guess->m_iMessageTime_guess = gEngfuncs.GetClientTime() + 4;

					if ( !iSilent )
					{
						strcpy( gViewPort->m_pHUDControl_guess->m_szMessage_guess, szText );
						gViewPort->m_pHUDControl_guess->m_flMessageAlpha_guess = 255;
						gViewPort->m_pHUDControl_guess->m_bMessageFadeOut_guess = false;
						gViewPort->m_pHUDControl_guess->m_bMessageHold_guess = false;
						gViewPort->m_pHUDControl_guess->m_flMessageHoldTime_guess = 0;
						gViewPort->m_pHUDControl_guess->m_iMessageType_guess = 5;
					}
				}
			}
		}
		else
		{
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[iSlot - 1]->setImage( NULL );
		}
	}

	if ( iTactical )
	{
		for ( int i = 1; i <= 6; i++ )
		{
			if ( !strcmp( "inventoryitems/weapons/weapon_glock.txt", GetSlotItem_guess( i ) ) )
			{
				strcpy( GetSlotItem_guess( i ), "inventoryitems/weapons/weapon_glocktac.txt" );
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[i - 1]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", GetSlotItem_guess( i ) ) ) );
			}
		}

		if ( !strcmp( "inventoryitems/weapons/weapon_glock.txt", gHUD.m_szInventoryWeapon1_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon1_guess, "inventoryitems/weapons/weapon_glocktac.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[6]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) );
		}

		if ( !strcmp( "inventoryitems/weapons/weapon_glock.txt", gHUD.m_szInventoryWeapon2_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon2_guess, "inventoryitems/weapons/weapon_glocktac.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[7]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) );
		}

		if ( !strcmp( "inventoryitems/weapons/weapon_glock.txt", gHUD.m_szInventoryWeapon3_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon3_guess, "inventoryitems/weapons/weapon_glocktac.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[9]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) );
		}
	}
	else
	{
		for ( int i = 1; i <= 6; i++ )
		{
			if ( !strcmp( "inventoryitems/weapons/weapon_glocktac.txt", GetSlotItem_guess( i ) ) )
			{
				strcpy( GetSlotItem_guess( i ), "inventoryitems/weapons/weapon_glock.txt" );
				gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[i - 1]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", GetSlotItem_guess( i ) ) ) );
			}
		}

		if ( !strcmp( "inventoryitems/weapons/weapon_glocktac.txt", gHUD.m_szInventoryWeapon1_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon1_guess, "inventoryitems/weapons/weapon_glock.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[6]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon1_guess ) ) );
		}

		if ( !strcmp( "inventoryitems/weapons/weapon_glocktac.txt", gHUD.m_szInventoryWeapon2_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon2_guess, "inventoryitems/weapons/weapon_glock.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[7]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon2_guess ) ) );
		}

		// original bug: tests the second quick slot again
		if ( !strcmp( "inventoryitems/weapons/weapon_glocktac.txt", gHUD.m_szInventoryWeapon2_guess ) )
		{
			strcpy( gHUD.m_szInventoryWeapon3_guess, "inventoryitems/weapons/weapon_glock.txt" );
			gViewPort->m_pRECBInventory_guess->m_pItemImages_guess[9]->setImage( vgui_LoadTGA( CheckItem( "inventory_picture", gHUD.m_szInventoryWeapon3_guess ) ) );
		}
	}

	return 1;
}

int CHud :: MsgFunc_Materials( const char *pszName, int iSize, void *pbuf )
{
	iClientResetMaterials = 1;
	return 1;
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
