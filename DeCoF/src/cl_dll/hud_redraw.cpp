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
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "vgui_TeamFortressViewport.h"
#include "vgui_hud.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

// buz: for IsHardware check
#include "r_studioint.h"
#include "com_model.h"
extern engine_studio_api_t IEngineStudio;

#include "vgui_TeamFortressViewport.h"
#include "vgui_paranoiatext.h"// buz
#include "mp3.h" // buz
#include "gl_renderer.h" // buz
#include "const.h"
#include "cl_entity.h"
#include "event_api.h"
#include "kbutton.h"
#include "HUDControl_guess.h"
#include "CutsceneSkip_guess.h"
#include "rain.h"

#define MAX_LOGO_FRAMES 56

// buz: spread value
vec3_t g_vSpread;
int g_iGunMode;
int iDoctorMode_guess;	// vuser1[0] of the local player state

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

extern int g_iVisibleMouse;

//float HUD_GetFOV( void ); buz

extern cvar_t *sensitivity;
extern cvar_t *cv_blurtest;
extern cvar_t *cv_screenblur_guess;
extern cvar_t *cv_blurstepsx_guess;
extern cvar_t *cv_blurstepsy_guess;
extern cvar_t *cv_opacity_guess;

extern int iDefaultFOV_guess;
extern int iSkipSceneFramerate_guess;
extern int iDifficulty_guess;
extern int g_iDickMode_guess;
extern int iBoat_guess;
extern int iWheelchair_guess;
extern bool bWheelchairLock_guess;
extern float flMouseOfs2X_guess;
extern bool bIronsightEdit_guess;
extern vec3_t vecIronsightOfs_guess;
extern cl_entity_t *pTelescopeEnt_guess;
extern int iUnknown1054506C_guess;
extern float flUnknown105595A0_guess;
extern float flUnknown105595A4_guess;
extern float flUnknown105595A8_guess;
extern int iUnknown105595AC_guess;
extern kbutton_t in_forward;
extern kbutton_t in_back;
extern kbutton_t in_moveleft;
extern kbutton_t in_moveright;

float flNextSoundCacheTime_guess;
float flGunParam1_guess;	// vuser2[1] of the local player state, set by HUD_ProcessPlayerState
bool bUnknown105431E9_guess;


// true while the view is set to another entity than the local player (ViewEntity message)
bool IsViewEntityActive_guess( void )
{
	if ( gEngfuncs.GetLocalPlayer() && gHUD.m_iViewEntity_guess )
	{
		if ( gHUD.m_iViewEntity_guess != gEngfuncs.GetLocalPlayer()->index )
			return true;
	}

	return false;
}

// adds a frame rate sample, returns the average of the last 50
int CHud :: AverageFPS_guess( int iFPS )
{
	if ( m_iFPSCount_guess >= 50 )
	{
		for ( int i = 1; i <= 50; i++ )
			m_iFPS_guess[i] = 0;
		m_iFPSCount_guess = 0;
	}

	m_iFPSCount_guess++;
	m_iFPS_guess[m_iFPSCount_guess] = iFPS;

	int iTotal, iCount;
	iTotal = iCount = 0;

	for ( int j = 1; j <= 50; j++ )
	{
		if ( m_iFPS_guess[j] )
		{
			iTotal += m_iFPS_guess[j];
			iCount++;
		}
	}

	return iTotal / iCount;
}

// Think
void CHud::Think(void)
{
	if ( flUnknown105595A0_guess + flUnknown105595A4_guess > gEngfuncs.GetClientTime() && iUnknown1054506C_guess < 3 && gEngfuncs.GetLocalPlayer() )
	{
		if ( gEngfuncs.GetClientTime() > flUnknown105595A8_guess )
		{
			iUnknown105595AC_guess++;
			FallingDripEffect_guess( gEngfuncs.GetLocalPlayer()->origin, 1.0 );
			flUnknown105595A8_guess = gEngfuncs.GetClientTime() + iUnknown105595AC_guess * 0.05 + gEngfuncs.pfnRandomFloat( 0.1, 0.25 );
		}
	}

	if ( iBoat_guess )
 {
  unsigned mask=(in_forward.state&1?1:0)|(in_back.state&1?2:0)|(in_moveleft.state&1?4:0)|(in_moveright.state&1?8:0);
  char packet[32];sprintf(packet,"cof8_boat %u",mask);ServerCmd(packet);
 }

	if ( iWheelchair_guess )
	{
		if ( in_forward.state & 1 )
			ServerCmd( "wheelchairup" );
		if ( in_back.state & 1 )
			ServerCmd( "wheelchairdown" );
		if ( in_moveleft.state & 1 )
			ServerCmd( "wheelchairleft" );
		if ( in_moveright.state & 1 )
			ServerCmd( "wheelchairright" );
	}

	// buz: calc dead time
	if (gHUD.m_Health.m_iHealth <= 0 && !m_flDeadTime)
	{
		m_flDeadTime = gEngfuncs.GetClientTime();
	}

	if (gHUD.m_Health.m_iHealth > 0)
	{
		m_flDeadTime = 0;
	}

	float targetFOV;
	HUDLIST *pList = m_pHudList;
	static float lasttime = 0;

	while (pList)
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	if		(g_iGunMode > 3)	targetFOV = g_iGunMode;
	else if	(g_iGunMode == 3)	targetFOV = 30;
	else if	(g_iGunMode == 2)	targetFOV = 60;
	else						targetFOV = iDefaultFOV_guess;

	static float lastFixedFov = 0;

	if (m_flFOV < 0)
	{
		m_flFOV = targetFOV;
		lasttime = gEngfuncs.GetClientTime();
		lastFixedFov = m_flFOV;
	}
	else
	{
		float curtime = gEngfuncs.GetClientTime();
		float mod = targetFOV - m_flFOV;
		if (mod < 0) mod *= -1;
		if (mod < 30) mod = 30;
		if (g_iGunMode == 3 || lastFixedFov == 30) mod *= 2;
		mod /= 30;

		if ( g_iGunMode <= 3 || !m_flCameraZoomFOV_guess )
		{
			if (m_flFOV < targetFOV) {
				m_flFOV += (curtime - lasttime) * m_pZoomSpeed->value * mod;
				if (m_flFOV > targetFOV)
				{
					m_flFOV = targetFOV;
					lastFixedFov = m_flFOV;
				}
			}
			else if (m_flFOV > targetFOV) {
				m_flFOV -= (curtime - lasttime) * m_pZoomSpeed->value * mod;
				if (m_flFOV < targetFOV)
				{
					m_flFOV = targetFOV;
					lastFixedFov = m_flFOV;
				}
			}
		}
		else if ( g_iGunMode == 666 || m_flCameraZoomFOV_guess == 666 )
		{
			m_flFOV = targetFOV;
		}
		else
		{
			if (m_flFOV < targetFOV) {
				m_flFOV += (curtime - lasttime) * m_flCameraZoomFOV_guess * mod;
				if (m_flFOV > targetFOV)
				{
					m_flFOV = targetFOV;
					lastFixedFov = m_flFOV;
				}
			}
			else if (m_flFOV > targetFOV) {
				m_flFOV -= (curtime - lasttime) * m_flCameraZoomFOV_guess * mod;
				if (m_flFOV < targetFOV)
				{
					m_flFOV = targetFOV;
					lastFixedFov = m_flFOV;
				}
			}
		}
		lasttime = curtime;
	}

	m_iFOV = m_flFOV;

	// Set a new sensitivity
	if ( m_iFOV == iDefaultFOV_guess )
	{
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * (m_flFOV / (float)iDefaultFOV_guess) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}
}

//LRC - fog fading values
extern float g_fFadeDuration;
extern float g_fStartDist;
extern float g_fEndDist;
//extern int g_iFinalStartDist;
extern int g_iFinalEndDist;

void OrthoGasMask(void); // buz
void OrthoScope(void); // buz
void OrthoVGUI(void); // buz
void ApplyPostEffects(); // buz
extern vec3_t g_CrosshairAngle; // buz
extern int g_HeadShieldState; // buz

// called by Redraw between the time update and the blur, disabled in 1.6a: the binary keeps
// only the return, the old body is unknown (Paranoia called gMP3.Frame() from Redraw)
void DrawMP3Frame_guess( void )
{
	return;

//	gMP3.Frame();
}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	char szText[128];
	char szCmd[32];

	ApplyPostEffects(); // buz

	if ( m_bSaveHack_guess && m_iSaveHackSlot_guess )
	{
		if ( gEngfuncs.GetClientTime() > m_flSaveHackTime_guess )
		{
			sprintf( szCmd, "savehack cofsave%i\n", m_iSaveHackSlot_guess );
			ServerCmd( szCmd );
			m_bSaveHack_guess = false;
			m_flSaveHackTime_guess = 0;
			m_iSaveHackSlot_guess = 0;
		}
	}

	if ( g_iDickMode_guess )
	{
		if ( gEngfuncs.GetClientTime() > flNextSoundCacheTime_guess && gEngfuncs.GetLocalPlayer() )
		{
			gEngfuncs.pEventAPI->EV_PlaySound( -1, gEngfuncs.GetLocalPlayer()->origin, CHAN_AUTO, "ambience/soundcache", 1.0, ATTN_NORM, 0, PITCH_NORM );
			flNextSoundCacheTime_guess = gEngfuncs.GetClientTime() + 15.0;
		}
	}

	// the wheelchair lock lowers the view
	static float flNextWheelchairTime = 0;

	if ( bWheelchairLock_guess )
	{
		if ( gEngfuncs.GetClientTime() > flNextWheelchairTime )
		{
			flMouseOfs2X_guess += 5;
			if ( flMouseOfs2X_guess > 40 )
				flMouseOfs2X_guess = 40;
			flNextWheelchairTime = gEngfuncs.GetClientTime() + 0.025;
		}
	}
	else if ( flNextWheelchairTime > 0 )
	{
		flNextWheelchairTime = 0;
	}

	if ( m_bViewLock_guess && m_flViewLockTime_guess != 666 )
	{
		if ( gEngfuncs.GetClientTime() > m_flViewLockTime_guess && !m_bViewLockWasOn_guess )
			m_bViewLock_guess = false;
	}

	if ( bIronsightEdit_guess )
	{
		sprintf( szText, "Forward: %f - Right: %f - Up: %f Zoom: %i\n", vecIronsightOfs_guess[1], vecIronsightOfs_guess[0], vecIronsightOfs_guess[2], g_iGunMode );
		gHUD.m_SayText.SayTextPrint( szText, 128, -1 );
	}

	// camera sway
	if ( ( m_iCameraZoom_guess || pTelescopeEnt_guess ) && m_flTime > m_flNextSwayTime_guess && !m_iHoldBreath_guess )
	{
		vec3_t angles;

		gEngfuncs.GetViewAngles( angles );
		float flPitch = gEngfuncs.pfnRandomFloat( -0.25, 0.25 );
		float flYaw = gEngfuncs.pfnRandomFloat( -0.25, 0.25 );
		angles[0] -= flPitch;
		angles[1] -= flYaw;

		if ( !gHUD.m_bViewLock_guess )
			gEngfuncs.SetViewAngles( angles );

		m_flNextSwayTime_guess = m_flTime + 0.05;
	}

	if ( iDifficulty_guess <= 0 || iDifficulty_guess > 4 )
		iDifficulty_guess = CVAR_GET_FLOAT( "difficulty" );

	if ( !IsViewEntityActive_guess() )
	{
		if ( CVAR_GET_FLOAT( "host_framerate" ) == iSkipSceneFramerate_guess && strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) && !strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
			gEngfuncs.Cvar_SetValue( "host_framerate", 0 );
	}

	if ( IsViewEntityActive_guess() && CVAR_GET_FLOAT( "host_framerate" ) == iSkipSceneFramerate_guess )
	{
		if ( gViewPort->m_pCutsceneSkip_guess && !gViewPort->m_pCutsceneSkip_guess->isVisible() )
			gViewPort->m_pCutsceneSkip_guess->setVisible( true );
	}
	else if ( gViewPort && gViewPort->m_pCutsceneSkip_guess && gViewPort->m_pCutsceneSkip_guess->isVisible() )
	{
		gViewPort->m_pCutsceneSkip_guess->setVisible( false );
	}

	if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/video_stress_test.bsp" ) )
	{
		static float flLastFPSTime = 0;
		float flCurTime = gEngfuncs.GetClientTime();
		float flFrameTime = min( 1, flCurTime - flLastFPSTime );
		flFrameTime = max( 0, flFrameTime );
		flLastFPSTime = flCurTime;

		int iFPS = 1 / flFrameTime;
		m_iFPSAverage_guess = AverageFPS_guess( iFPS );
		int iMaxFPS = CVAR_GET_FLOAT( "fps_max" );

		if ( iFPS > 1 && gEngfuncs.GetClientTime() >= m_flNextFPSTime_guess )
		{
			if ( !m_bFPSToggle_guess )
			{
				m_iFPSMin_guess = iFPS;
				m_bFPSToggle_guess = true;
			}
			else
			{
				m_iFPSMax_guess = iFPS;
				m_bFPSToggle_guess = false;
			}
			gEngfuncs.Cvar_SetValue( "fps_current", iFPS );
			m_flNextFPSTime_guess = gEngfuncs.GetClientTime() + 1.0;

			if ( m_iFPSAverage_guess && m_iFPSMin_guess && m_iFPSMax_guess )
			{
				sprintf( szText, "FPS is %i with an average of %i and a max of %i", iFPS, m_iFPSAverage_guess, iMaxFPS );

				if ( gViewPort && gViewPort->m_pHUDControl_guess )
				{
					if ( strlen( szText ) > 30 )
						gViewPort->m_pHUDControl_guess->m_iMessageTime_guess = gEngfuncs.GetClientTime() + 6;
					else
						gViewPort->m_pHUDControl_guess->m_iMessageTime_guess = gEngfuncs.GetClientTime() + 4;
					gViewPort->m_pHUDControl_guess->m_szMessage2_guess[0] = 0;
					strcpy( gViewPort->m_pHUDControl_guess->m_szMessage_guess, szText );
				}
			}
			else
			{
				sprintf( szText, "FPS is %i with an average of %i and a max of %i", iFPS, iFPS, iMaxFPS );

				if ( gViewPort && gViewPort->m_pHUDControl_guess )
				{
					if ( strlen( szText ) > 30 )
						gViewPort->m_pHUDControl_guess->m_iMessageTime_guess = gEngfuncs.GetClientTime() + 6;
					else
						gViewPort->m_pHUDControl_guess->m_iMessageTime_guess = gEngfuncs.GetClientTime() + 4;
					gViewPort->m_pHUDControl_guess->m_szMessage2_guess[0] = 0;
					strcpy( gViewPort->m_pHUDControl_guess->m_szMessage_guess, szText );
				}
			}
		}
	}

	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static float m_flShotTime = 0;

	DrawMP3Frame_guess();
	DrawBlurTest(m_flTimeDelta);

	//LRC - handle fog fading effects. (is this the right place for it?)
	if (g_fFadeDuration)
	{
		// Nicer might be to use some kind of logarithmic fade-in?
		double fFraction = m_flTimeDelta/g_fFadeDuration;
//		g_fStartDist -= (FOG_LIMIT - g_iFinalStartDist)*fFraction;
		g_fEndDist -= (FOG_LIMIT - g_iFinalEndDist)*fFraction;

//		CONPRINT("FogFading: %f - %f, frac %f, time %f, final %d\n", g_fStartDist, g_fEndDist, fFraction, flTime, g_iFinalEndDist);

		// cap it
//		if (g_fStartDist > FOG_LIMIT)				g_fStartDist = FOG_LIMIT;
		if (g_fEndDist   > FOG_LIMIT)				g_fEndDist = FOG_LIMIT;
//		if (g_fStartDist < g_iFinalStartDist)	g_fStartDist = g_iFinalStartDist;
		if (g_fEndDist   < g_iFinalEndDist)		g_fEndDist   = g_iFinalEndDist;
	}

	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	// buz: draw gasmask
	if (m_iGasMask)
	{
		if (IEngineStudio.IsHardware())
			OrthoGasMask();
		else
			DrawHudString(XRES(10), YRES(350), XRES(600), "Using Gas Mask", 180, 180, 180);
	}

	if (g_iGunMode == 3) // buz: special sniper scope
	{
		if (IEngineStudio.IsHardware())
			OrthoScope();
	}

	if (!IEngineStudio.IsHardware() && g_HeadShieldState != 1)
		DrawHudString(XRES(10), YRES(350), XRES(600), "Using Head Shield", 180, 180, 180);

	OrthoVGUI(); // buz: panels background

	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if ( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
//			gViewPort->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideVGUIMenu();
			gViewPort->ShowScoreBoard();
//			gViewPort->UpdateSpectatorPanel();

			// Take a screenshot if the client's got the cvar set
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	// if no redrawing is necessary
	// return 0;

	if ( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while (pList)
		{
			if ( !intermission )
			{
				if ( (pList->p->m_iFlags & HUD_ACTIVE) && !(m_iHideHUDDisplay & HIDEHUD_ALL) )
					pList->p->Draw(flTime);
			}
			else
			{  // it's an intermission,  so only draw hud elements that are set to draw during intermissions
				if ( pList->p->m_iFlags & HUD_INTERMISSION )
					pList->p->Draw( flTime );
			}

			pList = pList->pNext;
		}
	}

	if ( g_iDickMode_guess )
	{
		HSPRITE hSprite = LoadSprite( "sprcache" );
		int x = gEngfuncs.pfnRandomLong( 0, ScreenWidth - 256 );
		int y = gEngfuncs.pfnRandomLong( 0, ScreenHeight - 256 );

		SPR_Set( hSprite, 250, 250, 250 );
		SPR_DrawAdditive( 0, x, y, NULL );
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );

		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

	// buz: draw crosshair
	if ((g_vSpread[0] && g_iGunMode != 3 && m_SpecTank_on == 0) && gHUD.m_pCvarDraw->value) // Wargon: Прицел рисуется только если hud_draw = 1.
	{
		if (gViewPort && gViewPort->m_pParanoiaText && gViewPort->m_pParanoiaText->isVisible())
			return 1;

		int barsize = XRES(g_iGunMode == 1 ? 9 : 6);
		int hW = ScreenWidth / 2;
		int hH = ScreenHeight / 2;
		float mod = (1/(tan(M_PI/180*(m_iFOV/2))));
		int dir = ((g_vSpread[0] * hW) / 500) * mod;
	//	gEngfuncs.Con_Printf("mod is %f, %d\n", mod, m_iFOV);

		if (g_CrosshairAngle[0] != 0 || g_CrosshairAngle[1] != 0)
		{
			// adjust for autoaim
			hW -= g_CrosshairAngle[1] * (ScreenWidth / m_iFOV);
			hH -= g_CrosshairAngle[0] * (ScreenWidth / m_iFOV);
		}

		// g_vSpread[2] - is redish [0..500]
		// gEngfuncs.Con_Printf("received spread: %f\n", g_vSpread[2]);
		int c = 255 - (g_vSpread[2] * 0.5);

		FillRGBA(hW - dir - barsize, hH, barsize, 1, 255, c, c, 200);
		FillRGBA(hW + dir, hH, barsize, 1, 255, c, c, 200);
		FillRGBA(hW, hH - dir - barsize, 1, barsize, 255, c, c, 200);
		FillRGBA(hW, hH + dir, 1, barsize, 255, c, c, 200);

	//	FillRGBA(hW - dir, hH - dir, dir*2, dir*2, 20, 150, 20, 100);
	}

	RendererDrawHud();

	/*
	if ( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );

		if (m_hsprCursor == 0)
		{
			char sz[256];
			sprintf( sz, "sprites/cursor.spr" );
			m_hsprCursor = SPR_Load( sz );
		}

		SPR_Set(m_hsprCursor, 250, 250, 250 );

		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

int CHud :: DrawHudString(int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	// draw the string until we hit the null character or a newline character
	for ( ; *szIt != 0 && *szIt != '\n'; szIt++ )
	{
		int next = xpos + gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
		if ( next > iMaxX )
			return xpos;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
		xpos = next;		
	}

	return xpos;
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

// draws a string from right to left (right-aligned)
int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	// find the end of the string
	char *szIt;
	for ( szIt = szString; *szIt != 0; szIt++ )
	{ // we should count the length?		
	}

	// iterate throug the string in reverse
	for ( szIt--;  szIt != (szString-1);  szIt-- )	
	{
		int next = xpos - gHUD.m_scrinfo.charWidths[ *szIt ]; // variable-width fonts look cool
		if ( next < iMinX )
			return xpos;
		xpos = next;

		TextMessageDrawChar( xpos, ypos, *szIt, r, g, b );
	}

	return xpos;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b)
{
	int iWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;
	int k;

	if (iNumber > 0)
	{
		// SPR_Draw 100's
		if (iNumber >= 100)
		{
			 k = iNumber/100;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if (iNumber >= 10)
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
		x += iWidth;
	} 
	else if (iFlags & DHN_DRAWZERO) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		// SPR_Draw 100's
		if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}

	return x;
}


int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if (iFlags & (DHN_3DIGITS))
		return 3;

	if (iFlags & (DHN_2DIGITS))
		return 2;

	if (iNumber <= 0)
	{
		if (iFlags & (DHN_DRAWZERO))
			return 1;
		else
			return 0;
	}

	if (iNumber < 10)
		return 1;

	if (iNumber < 100)
		return 2;

	return 3;

}	

extern int g_iBlurTexture_guess;
void CaptureBlurTexture_guess( int width, int height, int iUnused1, int iUnused2 );

// buz
void CHud :: DrawBlurTest( float frametime )
{
#ifdef COFR_XASH_ABI
	// The host renderer owns screen effects; no legacy GL callback exists in D3D9.
	if ( !gl.IsGLAllowed() )
		return;
#endif
	if ( !cv_screenblur_guess->value )
		return;

	bool bDamaged = false;
	if ( m_flDamageTime_guess > gEngfuncs.GetClientTime() )
		bDamaged = true;

	if ( ( cv_blurtest->value || gHUD.m_iBlur_guess || iUnknown1054506C_guess > 2 || bDamaged ) && frametime )
	{
		CaptureBlurTexture_guess( ScreenWidth, ScreenHeight, 0, 0 );

		if ( !g_iBlurTexture_guess )
			return;

		glDisable( GL_TEXTURE_GEN_S );
		glDisable( GL_TEXTURE_GEN_T );

		int iWidth = XRES( 32 );
		int iHeight = YRES( 24 );

		int iStepX = XRES( cv_blurstepsx_guess->value );
		int iStepY = YRES( cv_blurstepsy_guess->value );

		if ( iStepX < 1 )
			iStepX = 1;
		if ( iStepY < 1 )
			iStepY = 1;

		int iSteps = ScreenWidth / iStepX + 1;

		for ( int i = 0; i < iSteps; i++ )
		{
			gl.glBindTexture( GL_TEXTURE_2D, g_iBlurTexture_guess );
			gl.glEnable( GL_BLEND );
			gl.glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			gl.glColor4f( 1, 1, 1, cv_opacity_guess->value );

			iHeight += iStepY;
			iWidth += iStepX;

			glBegin( GL_QUADS );
			glTexCoord2f( 0, 1 );
			glVertex2f( ScreenWidth / 2 - iWidth / 2, ScreenHeight / 2 - iHeight / 2 );
			glTexCoord2f( 1, 1 );
			glVertex2f( ScreenWidth / 2 + iWidth / 2, ScreenHeight / 2 - iHeight / 2 );
			glTexCoord2f( 1, 0 );
			glVertex2f( ScreenWidth / 2 + iWidth / 2, ScreenHeight / 2 + iHeight / 2 );
			glTexCoord2f( 0, 0 );
			glVertex2f( ScreenWidth / 2 - iWidth / 2, ScreenHeight / 2 + iHeight / 2 );
			glEnd();

			gl.glDisable( GL_BLEND );
		}
	}
}

// screen fade of the NewFade message, never called in 1.6a
void CHud :: DrawNewFade_guess( float flTime )
{
	if ( !gl.IsGLAllowed() )
		return;

	if ( m_flFadeStart_guess + m_flFadeTime_guess > gEngfuncs.GetClientTime() || m_bFadeOut_guess || m_flFadeEnd_guess >= gEngfuncs.GetClientTime() )
	{
		float flMaxAlpha = m_iFadeAlpha_guess / 255.0f;
		float flAlpha = ( gEngfuncs.GetClientTime() - m_flFadeStart_guess ) / m_flFadeTime_guess;

		if ( flAlpha > 1 )
			flAlpha = 1;
		if ( flAlpha > flMaxAlpha )
			flAlpha = flMaxAlpha;
		if ( flAlpha < 0 )
			flAlpha = 0;

		float color[3];
		color[0] = m_iFadeR_guess / 255.0f;
		color[1] = m_iFadeG_guess / 255.0f;
		color[2] = m_iFadeB_guess / 255.0f;

		glEnable( GL_BLEND );
		glDisable( GL_TEXTURE_2D );

		if ( !m_bFadeHold_guess )
			glColor4f( color[0], color[1], color[2], flMaxAlpha - flAlpha );
		else
			glColor4f( color[0], color[1], color[2], flAlpha );
		glBegin( GL_QUADS );
		glVertex2f( 0, 0 );
		glVertex2f( ScreenWidth, 0 );
		glVertex2f( ScreenWidth, ScreenHeight );
		glVertex2f( 0, ScreenHeight );
		glEnd();
		glColor4f( 1, 1, 1, 1 );

		glEnable( GL_TEXTURE_2D );
		glDisable( GL_BLEND );
	}
}
