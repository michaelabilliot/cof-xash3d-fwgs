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
// hud.cpp
//
// implementation of CHud class
//

//LRC - define to help track what calls are made on changelevel, save/restore, etc
#define ENGINE_DEBUG

#include <fstream>
#include <string>
#include <vector>

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "hud_servers.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "mp3.h"
#include "rain.h" // buz
#include "grass.h" // buz
#include "vgui_subtitles.h" // buz
#include "vgui_radio.h" // buz
#include "vgui_hud.h" // buz
#include "vgui_tabpanel.h" // buz

#include "demo.h"
#include "demo_api.h"
#include "vgui_scorepanel.h"
#include "triapiobjects.h"

#include "glmanager.h" // buz
#include "gl_renderer.h" // buz
#include "CgWater_guess.h"
#include "exception.h" // buz
#include "log.h"

//=========================================================
// Declarations this file takes from the Steamworks SDK
// (steam_api.h, isteamuser.h, steamclientpublic.h)
//=========================================================
typedef unsigned __int64 uint64;
typedef int HSteamUser;

class CSteamID
{
public:
	uint64 ConvertToUint64() const
	{
		return m_unAll64Bits;
	}

private:
	uint64 m_unAll64Bits;
};

class ISteamUser
{
public:
	virtual HSteamUser GetHSteamUser() = 0;
	virtual bool BLoggedOn() = 0;
	virtual CSteamID GetSteamID() = 0;
};

extern "C" __declspec( dllimport ) ISteamUser * __cdecl SteamUser();
//=========================================================

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
	virtual void GetPlayerTextColor(int entindex, int color[3])
	{
		color[0] = color[1] = color[2] = 255;

		if( entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo)/sizeof(g_PlayerExtraInfo[0]) )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

	virtual void UpdateCursorState()
	{
		gViewPort->UpdateCursorState();
	}

	virtual int	GetAckIconHeight()
	{
		return ScreenHeight - gHUD.m_iFontHeight*3 - 6;
	}

	virtual bool			CanShowSpeakerLabels()
	{
		if( gViewPort && gViewPort->m_pScoreBoard )
			return !gViewPort->m_pScoreBoard->isVisible();
		else
			return false;
	}
};
int iDefaultFOV_guess = 90;
int iSkipSceneFramerate_guess = 2;	// host_framerate while a scene is skipped (SkipScene message, CHud::Redraw)
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern cvar_t *sensitivity;

int iMenuMode_guess;			// 1 for the game menu map, 2 after the to3dmenu command
bool bXboxFix_guess;			// cleared by the xboxfix command
int iHudActive_guess;			// set by Init and VidInit, cleared by the destructor
cvar_t *cl_lw = NULL;
int iGoro_guess;				// "goro" cvar
float flWeaponIdleSpeed_guess;	// "cl_weapon_idle_speed" cvar
float flWeaponIdleDampener_guess;	// "cl_weapon_idle_dampener" cvar

extern int iDonator_guess;
extern cl_entity_t *pAttachCamEnt_guess;	// the entity the view is attached to (AttachCam message)
extern int iBoat_guess;
extern int iWheelchair_guess;
extern bool bWheelchairLock_guess;
extern cl_entity_t *pTelescopeEnt_guess;
extern cl_entity_t *pCamTargetEnt_guess;
extern vec3_t vecIronsightOfs_guess;
extern bool bCustomFingerTexture_guess;	// CustomFng message
extern bool bCustomGloveTexture_guess;	// CustomGlv message
extern int iGreyFade_guess;				// GreyFade message
extern float flNextSoundCacheTime_guess;	// "ambience/soundcache" in CHud::Redraw
extern bool bUnknown105431E9_guess;
extern float flUnknown105595A0_guess;
extern float flUnknown105595A4_guess;
extern float flUnknown105595A8_guess;
extern int iUnknown105595AC_guess;
extern float flUnknown1055959C_guess;

int IsInDonatorGroup1_guess( void );	// Steam clan 0x29D6CA
int IsInDonatorGroup2_guess( void );	// Steam clan 0x46AE0A

void IrrKlangCreate_guess( void );
void IrrKlangSetFade_guess( float flParm1, float flParm2, float flParm3, float flParm4 );
void IrrKlangStop_guess( float flFadeTime );

void ShutdownInput (void);

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Logo(pszName, iSize, pbuf );
}

//DECLARE_MESSAGE(m_Logo, Logo)
//LRC
int __MsgFunc_HUDColor(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_HUDColor(pszName, iSize, pbuf );
}

//LRC
int __MsgFunc_SetFog(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_SetFog( pszName, iSize, pbuf );
	return 1;
}

//LRC
int __MsgFunc_KeyedDLight(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_KeyedDLight( pszName, iSize, pbuf );
	return 1;
}

//LRC
int __MsgFunc_AddShine(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_AddShine( pszName, iSize, pbuf );
	return 1;
}

//LRC
int __MsgFunc_SetSky(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_SetSky( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
#ifdef ENGINE_DEBUG
//	CONPRINT("## ResetHUD\n");
#endif
	return gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf );
}

int __MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
#ifdef ENGINE_DEBUG
//	CONPRINT("## InitHUD\n");
#endif
	gHUD.MsgFunc_InitHUD( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ViewMode( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetFOV( pszName, iSize, pbuf );
}

int __MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Concuss( pszName, iSize, pbuf );
}

int __MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GameMode( pszName, iSize, pbuf );
}

// buz
int __MsgFunc_GasMask(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GasMask( pszName, iSize, pbuf );
}

// buz
int __MsgFunc_SpecTank(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_SpecTank( pszName, iSize, pbuf );
}

int __MsgFunc_ProFont(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_ProFont( pszName, iSize, pbuf );
}

int __MsgFunc_Maglite(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Maglite( pszName, iSize, pbuf );
}

int __MsgFunc_Blur(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Blur( pszName, iSize, pbuf );
}

int __MsgFunc_UnreadSMS(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_UnreadSMS( pszName, iSize, pbuf );
}

int __MsgFunc_ViewKnock(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_ViewKnock( pszName, iSize, pbuf );
}

int __MsgFunc_CoFWeapon(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_CoFWeapon( pszName, iSize, pbuf );
}

int __MsgFunc_ViewEntity(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_ViewEntity( pszName, iSize, pbuf );
}

int __MsgFunc_ViewLock(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_ViewLock( pszName, iSize, pbuf );
}

int __MsgFunc_Ladder(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Ladder( pszName, iSize, pbuf );
}

int __MsgFunc_FallOver(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_FallOver( pszName, iSize, pbuf );
}

int __MsgFunc_BlackWhite(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_BlackWhite( pszName, iSize, pbuf );
}

int __MsgFunc_WeaponBody(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_WeaponBody( pszName, iSize, pbuf );
}

int __MsgFunc_PhoneOff(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_PhoneOff( pszName, iSize, pbuf );
}

int __MsgFunc_Muzzleflash(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Muzzleflash( pszName, iSize, pbuf );
}

int __MsgFunc_FlashFlags(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_FlashFlags( pszName, iSize, pbuf );
}

int __MsgFunc_Boat(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Boat( pszName, iSize, pbuf );
}

int __MsgFunc_Ripple(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Ripple( pszName, iSize, pbuf );
}

int __MsgFunc_AddNote(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_AddNote( pszName, iSize, pbuf );
}

int __MsgFunc_AttachCam(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_AttachCam( pszName, iSize, pbuf );
}

int __MsgFunc_BigPrick(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_BigPrick( pszName, iSize, pbuf );
}

int __MsgFunc_NewFade(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_NewFade( pszName, iSize, pbuf );
}

int __MsgFunc_SkipScene(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_SkipScene( pszName, iSize, pbuf );
}

int __MsgFunc_IronSights(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_IronSights( pszName, iSize, pbuf );
}

int __MsgFunc_Wheelchair(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Wheelchair( pszName, iSize, pbuf );
}

int __MsgFunc_GreyFade(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GreyFade( pszName, iSize, pbuf );
}

int __MsgFunc_TakePhoto(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_TakePhoto( pszName, iSize, pbuf );
}

int __MsgFunc_Objectives(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Objectives( pszName, iSize, pbuf );
}

int __MsgFunc_Unlocks(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Unlocks( pszName, iSize, pbuf );
}

int __MsgFunc_CustomSlv(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_CustomSlv( pszName, iSize, pbuf );
}

int __MsgFunc_CustomFng(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_CustomFng( pszName, iSize, pbuf );
}

int __MsgFunc_CustomGlv(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_CustomGlv( pszName, iSize, pbuf );
}

int __MsgFunc_PlaySFX(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_PlaySFX( pszName, iSize, pbuf );
}

int __MsgFunc_Inventory(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Inventory( pszName, iSize, pbuf );
}

int __MsgFunc_Materials(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_Materials( pszName, iSize, pbuf );
}

// TFFree Command Menu
void __CmdFunc_OpenCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->ShowCommandMenu( gViewPort->m_StandardMenu );
	}
}

// TFC "special" command
void __CmdFunc_InputPlayerSpecial(void)
{
	if ( gViewPort )
	{
		gViewPort->InputPlayerSpecial();
	}
}

void __CmdFunc_CloseCommandMenu(void)
{
	if ( gViewPort )
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

void __CmdFunc_ForceCloseCommandMenu( void )
{
	if ( gViewPort )
	{
		gViewPort->HideCommandMenu();
	}
}

void __CmdFunc_ToggleServerBrowser( void )
{
	if ( gViewPort )
	{
		gViewPort->ToggleServerBrowser();
	}
}

void __CmdFunc_FinishLadder_guess( void )
{
	pAttachCamEnt_guess = NULL;
}

void __CmdFunc_DonatorCheck_guess( void )
{
	iDonator_guess = 0;

	bool bGroup2 = false;
	if ( IsInDonatorGroup2_guess() )
		bGroup2 = true;

	if ( !IsInDonatorGroup1_guess() )
	{
		if ( !bGroup2 )
			iDonator_guess = 0;
		else if ( bGroup2 == true )
			iDonator_guess = 3;
	}
	else
	{
		if ( !bGroup2 )
			iDonator_guess = 1;
		else if ( bGroup2 == true )
			iDonator_guess = 2;
	}

	if ( SteamUser() )
	{
		CSteamID steamID = SteamUser()->GetSteamID();

		if ( steamID.ConvertToUint64() == 268893370.0 )
			iDonator_guess = 4;

		if ( steamID.ConvertToUint64() == 268992206.0 )
			iDonator_guess = 4;
	}
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ValClass( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamNames( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Feign( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Detpack( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_MOTD( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_BuildSt( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_RandomPC( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ServerName( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Spectator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_AllowSpec( pszName, iSize, pbuf );
	return 0;
}


int __MsgFunc_PlayMP3(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_PlayMP3( pszName, iSize, pbuf );
}

void __CmdFunc_StopMP3( void )
{
	IrrKlangStop_guess( 0 );
}

void __CmdFunc_BeginSPGame_guess( void )
{
	gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0;map c_difficulty_settings" );
}

void __CmdFunc_To3DMenu_guess( void )
{
	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0" );
		return;
	}

	gEngfuncs.pfnClientCmd( "map c_game_menu1" );
	iMenuMode_guess = 2;
}

void __CmdFunc_ReturnMenu_guess( void )
{
	gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0" );
}

void __CmdFunc_MP3Player_guess( void )
{
}

void __CmdFunc_ToUnlockables_guess( void )
{
	if ( gEngfuncs.GetMaxClients() > 1 )
	{
		gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0" );
		return;
	}

	gEngfuncs.pfnClientCmd( "map c_unlockables" );
}

// not hooked in 1.6a
void InventorySound_guess( char *pszSound, float flVolume );

void __CmdFunc_Unlockables_guess( void )
{
	if ( gEngfuncs.pfnGetLevelName() && strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
	{
		gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0;map c_unlockables" );
	}
	else if ( iHudActive_guess )
	{
		InventorySound_guess( "inventory/inventory_no.wav", 1 );
	}
	else
	{
		gEngfuncs.pfnClientCmd( "disconnect;maxplayers 1;deathmatch 0;map c_unlockables" );
	}
}

void ClearTextureChains_guess( void );

void __CmdFunc_TextureTest_guess( void )
{
	ClearTextureChains_guess();
}

// buz
int __MsgFunc_TextWindow(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ShowTextWindow( pszName, iSize, pbuf );
	return 0;
}

// buz
int __MsgFunc_RainData(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_RainData( pszName, iSize, pbuf );
}

void InitHeadShield( void ); // buz
void InitGlows();
void GammaGraphInit();
void InitPostEffects();

void CanUseInit( void ); // Wargon: »конка юза.
void HUDControlInit_guess( void );
void VGUI_BoardInit_guess( void );
void NoteDocInit_guess( void );
void VGUI_KeypadInit_guess( void );
void PhoneInit_guess( void );
void PuzzlebarInit_guess( void );
void SaveInit_guess( void );
void YesNoInit_guess( void );
void StatueInit_guess( void );
void VGUI_ComputerInit_guess( void );
void UserDocInit_guess( void );
void PadlockInit_guess( void );
void SaveLoadInit_guess( void );
void VGUI_GameMenuInit_guess( void );
void ScopeInit_guess( void );
void TVFrameInit_guess( void );
void VGUI_BossBarInit_guess( void );
void TelephoneInit_guess( void );
void VGUI_QSlotInit_guess( void );
void VGUI_AmmoInvInit_guess( void );
void VGUI_CreditsInit_guess( void );
void VGUI_ClothesInit_guess( void );
void UnlockablesInit_guess( void );
void VGUI_CoopStatsInit_guess( void );
void VGUI_CustomDocInit_guess( void );

// This is called every time the DLL is loaded
void CHud :: Init( void )
{
	if ( !m_pVoiceTweak_guess )
		m_pVoiceTweak_guess = gEngfuncs.pVoiceTweak;

	m_szSleeveTexture_guess[0] = 0;
	m_szFingerTexture_guess[0] = 0;
	gHUD.m_szGloveTexture_guess[0] = 0;
	bCustomFingerTexture_guess = false;
	bCustomGloveTexture_guess = false;
	m_bSaveHack_guess = false;
	m_flSaveHackTime_guess = 0;
	m_iSaveHackSlot_guess = 0;
	iHudActive_guess = 1;
	m_flDamageTime_guess = 0;

	Log("Initializing variables\n");
	gl.Init();

	m_iBlur_guess = 0;
	flNextSoundCacheTime_guess = 0;
	m_bMenuClick_guess = false;
	m_flMenuClickTime_guess = 0;
	g_CgWater_guess.m_bEnabled_guess = false;

	InitGlows();
	RendererInit();
	GammaGraphInit();
	InitPostEffects();

	m_hMuzzleflash_guess = 0;
	iGreyFade_guess = 0;
	m_szObjectiveTitle_guess[0] = 0;
	m_szObjectiveText_guess[0] = 0;

#ifdef ENGINE_DEBUG
//	CONPRINT("## CHud::Init\n");
#endif
	InitRain();	// rain
	InitHeadShield(); // buz
	GrassInit(); // buz
	RadioIconInit(); // buz
	Hud2Init(); // buz
	SubtitleInit(); // buz
	TabPanelInit(); // buz
	HUDControlInit_guess();
	VGUI_BoardInit_guess();
	NoteDocInit_guess();
	VGUI_KeypadInit_guess();
	PhoneInit_guess();
	PuzzlebarInit_guess();
	SaveInit_guess();
	YesNoInit_guess();
	StatueInit_guess();
	VGUI_ComputerInit_guess();
	UserDocInit_guess();
	PadlockInit_guess();
	SaveLoadInit_guess();
	VGUI_GameMenuInit_guess();
	ScopeInit_guess();
	TVFrameInit_guess();
	VGUI_BossBarInit_guess();
	TelephoneInit_guess();
	VGUI_QSlotInit_guess();
	VGUI_AmmoInvInit_guess();
	VGUI_CreditsInit_guess();
	VGUI_ClothesInit_guess();
	UnlockablesInit_guess();
	VGUI_CoopStatsInit_guess();
	VGUI_CustomDocInit_guess();

	m_bConcussed_guess = false;

	CanUseInit(); // Wargon: »конка юза.

	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	HOOK_MESSAGE( HUDColor ); //LRC
	HOOK_MESSAGE( SetFog ); //LRC
	HOOK_MESSAGE( KeyedDLight ); //LRC
	HOOK_MESSAGE( AddShine ); //LRC
	HOOK_MESSAGE( SetSky ); //LRC
	HOOK_MESSAGE( GasMask ); //buz
	HOOK_MESSAGE( SpecTank ); //buz
	HOOK_MESSAGE( TextWindow ); // buz
	HOOK_MESSAGE( RainData );// buz
	HOOK_MESSAGE( ProFont );
	HOOK_MESSAGE( Maglite );
	HOOK_MESSAGE( Blur );
	HOOK_MESSAGE( UnreadSMS );
	HOOK_MESSAGE( ViewKnock );
	HOOK_MESSAGE( CoFWeapon );
	HOOK_MESSAGE( ViewEntity );
	HOOK_MESSAGE( ViewLock );
	HOOK_MESSAGE( Ladder );
	HOOK_MESSAGE( FallOver );
	HOOK_MESSAGE( BlackWhite );
	HOOK_MESSAGE( WeaponBody );
	HOOK_MESSAGE( PhoneOff );
	HOOK_MESSAGE( Muzzleflash );
	HOOK_MESSAGE( FlashFlags );
	HOOK_MESSAGE( Boat );
	HOOK_MESSAGE( Ripple );
	HOOK_MESSAGE( AddNote );
	HOOK_MESSAGE( AttachCam );
	HOOK_MESSAGE( BigPrick );
	HOOK_MESSAGE( NewFade );
	HOOK_MESSAGE( SkipScene );
	HOOK_MESSAGE( IronSights );
	HOOK_MESSAGE( Wheelchair );
	HOOK_MESSAGE( GreyFade );
	HOOK_MESSAGE( TakePhoto );
	HOOK_MESSAGE( Objectives );
	HOOK_MESSAGE( Unlocks );
	HOOK_MESSAGE( CustomSlv );
	HOOK_MESSAGE( CustomFng );
	HOOK_MESSAGE( CustomGlv );
	HOOK_MESSAGE( PlaySFX );

	// TFFree CommandMenu
	HOOK_COMMAND( "+commandmenu", OpenCommandMenu );
	HOOK_COMMAND( "-commandmenu", CloseCommandMenu );
	HOOK_COMMAND( "ForceCloseCommandMenu", ForceCloseCommandMenu );
	HOOK_COMMAND( "special", InputPlayerSpecial );
	HOOK_COMMAND( "togglebrowser", ToggleServerBrowser );
	HOOK_COMMAND( "finishladder", FinishLadder_guess );
	HOOK_COMMAND( "donatorcheck", DonatorCheck_guess );

	HOOK_MESSAGE( ValClass );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );

	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );

	HOOK_MESSAGE( Inventory );
	HOOK_MESSAGE( Materials );

	CVAR_CREATE( "dev_commentary", "0", FCVAR_ARCHIVE );
	m_pZoomSpeed = gEngfuncs.pfnRegisterVariable( "cl_zoomspeed","100", 0 );
	CVAR_CREATE( "hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );		// controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );		// controls whether or not to automatically take screenshots at the end of a round
	CVAR_CREATE( "brightvalue", "0.5", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "skybrightvalue", "0.1", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "goro", "0", FCVAR_ARCHIVE );
	CVAR_CREATE( "skybrightcolourr", "0.0", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "skybrightcolourg", "0.0", FCVAR_ARCHIVE | FCVAR_USERINFO );
	CVAR_CREATE( "skybrightcolourb", "0.0", FCVAR_ARCHIVE | FCVAR_USERINFO );
	iGoro_guess = CVAR_GET_FLOAT( "goro" );

	HOOK_MESSAGE( PlayMP3 );
	HOOK_COMMAND( "stopmp3", StopMP3 );
	IrrKlangSetFade_guess( 0, 0, 0, 0 );

	HOOK_COMMAND( "beginspgame", BeginSPGame_guess );
	HOOK_COMMAND( "to3dmenu", To3DMenu_guess );
	HOOK_COMMAND( "returnmenu", ReturnMenu_guess );
	HOOK_COMMAND( "returnmenu", ReturnMenu_guess );
	HOOK_COMMAND( "mp3player", MP3Player_guess );
	HOOK_COMMAND( "unlockablescmd", ToUnlockables_guess );
	HOOK_COMMAND( "texturetest", TextureTest_guess );

	m_iLogo = 0;
	m_iFOV = iDefaultFOV_guess;
	m_iHUDColor = 0x00FFFFFF;

	m_iFPSMin_guess = 0;
	m_iFPSMax_guess = 0;
	m_iFPSAverage_guess = 0;
	m_bFPSToggle_guess = false;
	m_flNextFPSTime_guess = gEngfuncs.GetClientTime();
	for ( int i = 1; i <= 100; i++ )
		m_iFPS_guess[i] = 0;

	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", 0 );
	RainInfo = gEngfuncs.pfnRegisterVariable( "cl_raininfo", "0", 0 );	// rain
//	default_fov = CVAR_CREATE( "default_fov", "90", 0 );// buz: turn off
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );

	// Wargon: fmod_volume регулирует громкость звуков, воспроизводимых через FMOD.
	CVAR_CREATE( "fmod_volume", "1.0", FCVAR_ARCHIVE );

	m_pSpriteList = NULL;

	// Clear any old HUD list
	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_Lensflare_guess.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (vgui::Panel**)&gViewPort);
	m_Particle.Init(); // (LRC) -- 30/08/02 November235: Particles to Order
	m_Neffect_guess.Init();
	m_BlackBar_guess.Init();

	m_Menu.Init();

	ServersInit();

	if ( !gEngfuncs.CheckParm( "-nomusic", NULL ) )
		IrrKlangCreate_guess();

	MsgFunc_ResetHUD(0, 0, NULL );
	Log("END Initializing variables\n");
}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
#ifdef ENGINE_DEBUG
//	CONPRINT("## CHud::destructor\n");
#endif

//	ResetRain();	// rain

	iHudActive_guess = 0;

	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;

	RendererCleanup();
//	g_objmanager.Reset();

	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = NULL;
	}

	ServersShutdown();
//	UnInstallExceptionHandler(); // buz
	UnloadCrashRpt(); // buz
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ) == 0 )
			return i;
	}

	return -1; // invalid sprite
}

void ResetHeadShield(); // buz
void ResetGlows();
void CAM_ToFirstPerson( void );

void CHud :: VidInit( void )
{
	iMenuMode_guess = 1;
	m_flDamageTime_guess = 0;
	m_bSaveHack_guess = false;
	m_flSaveHackTime_guess = 0;
	m_iSaveHackSlot_guess = 0;
	m_szSleeveTexture_guess[0] = 0;
	m_szFingerTexture_guess[0] = 0;
	gHUD.m_szGloveTexture_guess[0] = 0;
	bCustomFingerTexture_guess = false;
	bCustomGloveTexture_guess = false;
	flNextSoundCacheTime_guess = 0;
	iHudActive_guess = 1;
	IrrKlangSetFade_guess( 0, 0, 0, 0 );

	Log("\nInitializing video\n");

	m_szObjectiveTitle_guess[0] = 0;
	m_szObjectiveText_guess[0] = 0;
	m_bMenuClick_guess = false;
	m_flMenuClickTime_guess = 0;
	g_CgWater_guess.m_bEnabled_guess = false;
	iGoro_guess = CVAR_GET_FLOAT( "goro" );
	m_flGreyFadeEnd_guess = 0;
	m_flGreyFadeStart_guess = 0;
	m_flViewLockTime_guess = -1;
	m_bViewLockWasOn_guess = false;
	m_flUnknown350_guess = -1;
	m_vecUnknown344_guess = Vector( 0, 0, 0 );
	iGreyFade_guess = 0;
	flWeaponIdleSpeed_guess = CVAR_GET_FLOAT( "cl_weapon_idle_speed" );
	flWeaponIdleDampener_guess = CVAR_GET_FLOAT( "cl_weapon_idle_dampener" );
	bUnknown105431E9_guess = false;
	m_bIronSights_guess = false;
	vecIronsightOfs_guess.y = vecIronsightOfs_guess.x = vecIronsightOfs_guess.z = 0;
	m_vecIronSightOfs_guess.x = m_vecIronSightOfs_guess.y = m_vecIronSightOfs_guess.z = 0;
	m_vecIronSightOfs2_guess.x = m_vecIronSightOfs2_guess.y = m_vecIronSightOfs2_guess.z = 0;
	flUnknown105595A0_guess = 0;
	flUnknown105595A4_guess = 0;
	flUnknown105595A8_guess = 0;
	iUnknown105595AC_guess = 0;
	m_iFlashFlags_guess = 0;
	pTelescopeEnt_guess = NULL;
	pAttachCamEnt_guess = NULL;
	iBoat_guess = 0;
	iWheelchair_guess = 0;
	m_iCameraZoom_guess = 0;
	m_flNextSwayTime_guess = m_flTime;
	pCamTargetEnt_guess = NULL;
	bWheelchairLock_guess = false;
	m_iHoldBreath_guess = 0;
	flUnknown1055959C_guess = 0;
	bXboxFix_guess = false;
	m_bCameraZoomOn_guess = false;
	m_bXboxFix_guess[0] = false;
	m_bXboxFix_guess[1] = false;
	m_bXboxFix_guess[2] = false;
	m_bXboxFix_guess[3] = false;
	m_bXboxFix_guess[4] = false;
	m_bXboxFix_guess[5] = false;
	m_bXboxFix_guess[6] = false;
	m_bXboxFix_guess[7] = false;
	m_bXboxFix_guess[8] = false;
	m_bXboxFix_guess[9] = false;
	m_bXboxFix_guess[10] = false;
	m_bXboxFix_guess[11] = false;
	m_bXboxFix_guess[14] = false;
	m_bXboxFix_guess[15] = false;
	m_bXboxFix_guess[16] = false;
	m_bXboxFix_guess[17] = false;
	m_bXboxFix_guess[18] = false;
	m_bXboxFix_guess[19] = false;
	m_bXboxFix_guess[20] = false;
	m_bXboxFix_guess[21] = false;
	m_bXboxFix_guess[22] = false;
	m_bXboxFix_guess[23] = false;
	m_bXboxFix_guess[24] = false;
	m_bXboxFix_guess[25] = false;
	m_bXboxFix_guess[12] = false;
	m_bXboxFix_guess[13] = false;

	gl.VidInit(); // buz

	m_bConcussed_guess = false;

	ResetRain();	// rain

	m_flDeadTime = 0; // buz
	m_iGasMask = 0; // buz
	m_SpecTank_on = 0; // buz

	m_flResetTime_guess = gEngfuncs.GetClientTime();
	m_flUnknown2A84_guess = 0;
	m_bUnknown2A88_guess = false;
	m_iBlackWhite_guess = 0;

	ResetHeadShield(); // buz
	m_flFOV = -1; // buz
	m_flCameraZoomFOV_guess = 0;

	GrassVidInit();

	ResetGlows();

	g_objmanager.Reset();

	RendererVidInit();

	m_hMuzzleflash_guess = SPR_Load( "sprites/muzzleflash.spr" );

#ifdef ENGINE_DEBUG
//	CONPRINT("## CHud::VidInit\n");
#endif
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");

	m_hsprLogo = 0;
	m_hsprCursor = 0;
	m_iFOV = iDefaultFOV_guess;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE[m_iSpriteCount];
			m_rgrcRects = new wrect_t[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_Neffect_guess.VidInit();
	m_BlackBar_guess.VidInit();
	m_StatusIcons.VidInit();
	m_Lensflare_guess.Init();
	GetClientVoiceMgr()->VidInit();
	m_Particle.VidInit(); // (LRC) -- 30/08/02 November235: Particles to Order

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
	m_iUnknown2A90_guess = 0;

	m_iFadeR_guess = 0;
	m_iFadeG_guess = 0;
	m_iFadeB_guess = 0;
	m_iFadeA_guess = 0;
	m_iFadeAlpha_guess = 0;
	m_flFadeStart_guess = -1;
	m_flFadeTime_guess = -1;
	m_flFadeEnd_guess = -1;
	m_bFadeOut_guess = false;
	m_bFadeHold_guess = false;

	Log("End Initializing video\n");

	CAM_ToFirstPerson();
}

int CHud::MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

//LRC
int CHud::MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	m_iHUDColor = READ_LONG();

	return 1;
}

//float g_lastFOV = 0.0; buz

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );

	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;

	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
#if 0 // buz
float HUD_GetFOV( void )
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		int i = 0;
		unsigned char buf[ 100 ];

		// Active
		*( float * )&buf[ i ] = g_lastFOV;
		i += sizeof( float );

		Demo_WriteBuffer( TYPE_ZOOM, i, buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}
#endif

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int newfov = READ_BYTE();
//	int def_fov = CVAR_GET_FLOAT( "default_fov" ); // buz: turn off

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
//	if ( cl_lw && cl_lw->value )
//		return 1; // buz: nety

#if 0 // buz: ignore server's fov commands
	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = 90;//def_fov; // buz: turn off
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == 90)//def_fov ) // buz: turn off
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / /*(float)def_fov*/90) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}
#endif

	return 1;
}


void CHud::AddHudElem(CHudBase *phudelem)
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST *)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity( void )
{
	return m_flMouseSensitivity;
}

char *CHud::GetKeyForCommand_guess( char *pszCommand )
{
	static char szKey[256];
	szKey[0] = 0;

	char szConfig[256];
	strcpy( szConfig, gEngfuncs.pfnGetGameDirectory() );
	strcat( szConfig, "/config.cfg" );

	std::ifstream file( szConfig );
	std::string line;
	std::vector<std::string> tokens;

	while ( std::getline( file, line ) )
	{
		char szLine[256];
		strcpy( szLine, line.c_str() );
		szLine[line.length()] = 0;

		if ( strstr( szLine, "bind" ) && strstr( szLine, pszCommand ) )
		{
			bool bInQuotes = false;
			int j = 0;

			for ( int i = 1; i <= (int)strlen( szLine ); i++ )
			{
				if ( bInQuotes )
				{
					if ( szLine[i] == '"' )
					{
						szKey[j] = 0;
						return szKey;
					}

					szKey[j] = toupper( szLine[i] );
					j++;
				}
				else if ( szLine[i] == '"' )
				{
					bInQuotes = true;
				}
			}

			return NULL;
		}
	}

	gEngfuncs.Con_Printf( "Couldn't get key for bind %s!\n", pszCommand );
	return NULL;
}

char *CHud::GetSlotItem_guess( int iSlot )
{
	switch ( iSlot )
	{
	case 1:
		return m_szInventoryItem1_guess;
	case 2:
		return m_szInventoryItem2_guess;
	case 3:
		return m_szInventoryItem3_guess;
	case 4:
		return m_szInventoryItem4_guess;
	case 5:
		return m_szInventoryItem5_guess;
	case 6:
		return m_szInventoryItem6_guess;
	}

	return "";
}

int CHud::PlusUnknown5590_guess( int iValue )
{
	return m_iUnknown5590_guess + iValue;
}

int CHud::MinusUnknown5590_guess( int iValue )
{
	return iValue - PlusUnknown5590_guess( 0 );
}
