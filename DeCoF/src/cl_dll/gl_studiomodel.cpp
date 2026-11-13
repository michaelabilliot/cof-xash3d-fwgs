//
// written by BUzer for HL: Paranoia modification
//
//		2006

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "dlight.h"
#include "triangleapi.h"
#include "pm_defs.h"
#include "pmtrace.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "studio_util.h"
#include "r_studioint.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

#include "gl_renderer.h"
#include "gl_dlight.h"
#include "gl_texloader.h"

extern engine_studio_api_t IEngineStudio;
extern int g_iUnlockClothes_guess;
extern int g_iHeadShieldType_guess;
extern int iDoctorMode_guess;
extern cl_entity_t *pAttachCamEnt_guess;
extern int g_iDualLighting_guess;
extern int g_DrawHeadShied;
extern int g_iDickMode_guess;
extern int iBoat_guess;
extern cl_entity_t *pTelescopeEnt_guess;
extern int iUnknown1054506C_guess;
extern vec3_t vecBoatOrigin_guess;
extern cvar_t *cl_propdistance;
extern cvar_t *cl_cameraeffect;
extern cvar_t *cv_fullbrighttextures_guess;
extern bool bCustomFingerTexture_guess;
extern bool bCustomGloveTexture_guess;
extern int iGoro_guess;
extern int g_iDualWeapon1_guess;
extern int g_iDualWeapon2_guess;
extern float flUnknown105595A0_guess;
extern float flUnknown105595A4_guess;
bool IsViewEntityActive_guess( void );
extern int iDifficulty_guess;
extern cl_entity_t *g_pHeadShieldEnt_guess;
extern vec3_t vecDualAttachment0_guess, vecDualAttachment1_guess;
extern cvar_t *cl_lowflashlight;
extern vec3_t v_origin, v_angles;
extern "C" int CL_IsThirdPerson( void );

//
// Custom model rendering, based on modelviewer's code
//

vec3_t	g_xformverts[MAXSTUDIOVERTS];	// transformed vertices
vec3_t	g_lightvalues[MAXSTUDIOVERTS];	// light surface normals
vec3_t	*g_pxformverts;
vec3_t	*g_pvlightvalues;

//vec3_t	g_lightvec;						// light vector in model reference frame
vec3_t	g_blightvec[MAXSTUDIOBONES];	// light vectors in bone reference frames
//int		g_ambientlight;					// ambient world light
//float	g_shadelight;					// direct world light
//vec3_t	g_lightcolor;
lighting_ext light; // buz

// buz: dynamic lighting
vec3_t	g_bdynlightvec[MAX_MODEL_DYNLIGHTS][MAXSTUDIOBONES]; // light vectors in bone reference frames
vec3_t	g_dynlightcolor[MAX_MODEL_DYNLIGHTS];
int		g_numdynlights;

int		g_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
int		g_chromeage[MAXSTUDIOBONES];	// last time chrome vectors were updated
vec3_t	g_chromeup[MAXSTUDIOBONES];		// chrome vector "up" in bone reference frames
vec3_t	g_chromeright[MAXSTUDIOBONES];	// chrome vector "right" in bone reference frames

//vec3_t	g_vright;		// needs to be set to viewer's right in order for chrome to work
float	alpha = 1; // buz: current alpha
int		useblending = FALSE;

// Cry of Fear: replacement textures for the player and costume models
int		g_iHoodieTexture_guess;
int		g_iSleeveTexture_guess;
int		g_iPageLockedTexture_guess;
bool	g_bPageLockedLoaded_guess;
bool	g_bHoodieLoaded_guess;
bool	g_bSleeveLoaded_guess;
bool	g_bHandsLoaded_guess;
int		g_iHandsTexture_guess;
int		g_iGloveTexture_guess;
bool	g_bGloveLoaded_guess;
bool	g_bJeansLoaded_guess;
bool	g_bFaceLoaded_guess;
bool	g_bPoliceSleeveLoaded_guess;
int		g_iJeansTexture_guess;
int		g_iFaceTexture_guess;
int		g_iPoliceSleeveTexture_guess;
int		g_iIntroductionTexture_guess;
bool	g_bIntroductionLoaded_guess;
bool	g_bBlurTextureCaptured_guess;
bool	g_bScreenTextureCaptured_guess;
int		g_iBlurTexture_guess;
int		g_iOldTexture_guess;
int		g_iScreenTexture_guess;

void LoadFaceTexture_guess( void )
{
	char texture[256];

	if (!gHUD.m_iClothes_guess)
		return;

	if (!g_iFaceTexture_guess)
		g_iFaceTexture_guess = current_ext_texture_id++;

	if (g_bFaceLoaded_guess)
		return;

	g_bFaceLoaded_guess = false;

	switch (gHUD.m_iClothesState_guess)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
	case 9:
		return;
	case 7:
		sprintf(texture, "models/costumes/black_metal/Black_Metal_Face.tga");
		break;
	case 10:
		sprintf(texture, "models/costumes/sick_simon/Sick_Simon_Face.tga");
		break;
	case 11:
		sprintf(texture, "models/costumes/twitcher/Twitcher_Face.tga");
		break;
	}

	if (CreateTexture(texture, 0, g_iFaceTexture_guess))
		g_bFaceLoaded_guess = true;
}

void LoadGloveTexture_guess( int iUnused )
{
	int result;

	if (!g_iGloveTexture_guess)
		g_iGloveTexture_guess = current_ext_texture_id++;

	if (g_bGloveLoaded_guess)
		return;

	g_bGloveLoaded_guess = false;

	if (strlen(gHUD.m_szGloveTexture_guess) > 2)
		result = CreateTexture(gHUD.m_szGloveTexture_guess, 0, g_iGloveTexture_guess);

	if (result)
		g_bGloveLoaded_guess = true;
}

void LoadHoodieTexture_guess( bool bUnlock )
{
	char texture[256];

	if (!g_iHoodieTexture_guess)
		g_iHoodieTexture_guess = current_ext_texture_id++;

	if (g_bHoodieLoaded_guess)
		return;

	g_bHoodieLoaded_guess = false;

	if (bUnlock && g_iUnlockClothes_guess)
	{
		switch (g_iUnlockClothes_guess)
		{
		case 1:
			sprintf(texture, "models/costumes/Leatherhoff/Leatherhoff_Hoodie.tga");
			break;
		case 2:
			sprintf(texture, "models/costumes/ModDB/ModDB_Hoodie.tga");
			break;
		case 3:
			sprintf(texture, "models/costumes/Hello_Kitty/Hello_Kitty_Hoodie.tga");
			break;
		case 4:
			sprintf(texture, "models/costumes/AoM/AoM_Hoodie.tga");
			break;
		case 5:
			sprintf(texture, "models/costumes/Camo/Camo_Hoodie.tga");
			break;
		case 6:
			sprintf(texture, "models/costumes/HLC/HLC_Hoodie.tga");
			break;
		case 7:
			sprintf(texture, "models/costumes/black_metal/Black_Metal_Hoodie.tga");
			break;
		case 8:
			sprintf(texture, "models/costumes/Team_Psykskallar/Team_Psykskallar_Hoodie.tga");
			break;
		case 9:
			sprintf(texture, "models/costumes/Awesome/Awesome_Hoodie.tga");
			break;
		case 10:
			sprintf(texture, "models/costumes/Sick_Simon/Sick_Simon_Hoodie.tga");
			break;
		case 11:
			sprintf(texture, "models/costumes/Twitcher/Twitcher_hoodie.tga");
			break;
		}
	}
	else
	{
		switch (gHUD.m_iClothesState_guess)
		{
		case 1:
			sprintf(texture, "models/costumes/Leatherhoff/Leatherhoff_Hoodie.tga");
			break;
		case 2:
			sprintf(texture, "models/costumes/ModDB/ModDB_Hoodie.tga");
			break;
		case 3:
			sprintf(texture, "models/costumes/Hello_Kitty/Hello_Kitty_Hoodie.tga");
			break;
		case 4:
			sprintf(texture, "models/costumes/AoM/AoM_Hoodie.tga");
			break;
		case 5:
			sprintf(texture, "models/costumes/Camo/Camo_Hoodie.tga");
			break;
		case 6:
			sprintf(texture, "models/costumes/HLC/HLC_Hoodie.tga");
			break;
		case 7:
			sprintf(texture, "models/costumes/Black_Metal/Black_Metal_Hoodie.tga");
			break;
		case 8:
			sprintf(texture, "models/costumes/Team_Psykskallar/Team_Psykskallar_Hoodie.tga");
			break;
		case 9:
			sprintf(texture, "models/costumes/Awesome/Awesome_Hoodie.tga");
			break;
		case 10:
			sprintf(texture, "models/costumes/Sick_Simon/Sick_Simon_Hoodie.tga");
			break;
		case 11:
			sprintf(texture, "models/costumes/Twitcher/Twitcher_Hoodie.tga");
			break;
		case 12:
			{
				char *custom = gEngfuncs.pfnGetCvarString("sv_custom_hoodie");
				if (custom)
					sprintf(texture, "models/costumes/%s", custom);
			}
			break;
		}
	}

	if (CreateTexture(texture, 0, g_iHoodieTexture_guess))
		g_bHoodieLoaded_guess = true;
}

void LoadIntroductionTexture_guess( int type )
{
	int result;
	int language = CVAR_GET_FLOAT("cof_subtitlelanguage");

	if (language < 2)
		return;

	if (!g_iIntroductionTexture_guess)
		g_iIntroductionTexture_guess = current_ext_texture_id++;

	if (g_bIntroductionLoaded_guess)
		return;

	g_bIntroductionLoaded_guess = false;

	switch (language)
	{
	case 2:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/dutch/dutch_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/dutch/dutch_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/dutch/dutch_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	case 3:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/french/french_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/french/french_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/french/french_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	case 4:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/german/german_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/german/german_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/german/german_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	case 5:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/norwegian/norwegian_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/norwegian/norwegian_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/norwegian/norwegian_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	case 6:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/spanish/spanish_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/spanish/spanish_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/spanish/spanish_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	case 7:
		if (type == 1)
			result = CreateTexture("gfx/vgui/introductions/swedish/swedish_sp.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 2)
			result = CreateTexture("gfx/vgui/introductions/swedish/swedish_doc.TGA", 0, g_iIntroductionTexture_guess);
		else if (type == 3)
			result = CreateTexture("gfx/vgui/introductions/swedish/swedish_coop.TGA", 0, g_iIntroductionTexture_guess);
		else
			return;
		break;
	default:
		return;
	}

	if (result)
		g_bIntroductionLoaded_guess = true;
}

void LoadPageLockedTexture_guess( void )
{
	if (!g_iPageLockedTexture_guess)
		g_iPageLockedTexture_guess = current_ext_texture_id++;

	if (g_bPageLockedLoaded_guess)
		return;

	g_bPageLockedLoaded_guess = false;

	if (CreateTexture("models/page_locked.tga", 0, g_iPageLockedTexture_guess))
		g_bPageLockedLoaded_guess = true;
}

void LoadHandsTexture_guess( char *modelname )
{
	char texture[256];
	int result;

	if (!g_iHandsTexture_guess)
		g_iHandsTexture_guess = current_ext_texture_id++;

	if (g_bHandsLoaded_guess)
		return;

	g_bHandsLoaded_guess = false;

	if (!modelname)
		return;

	if (strstr(modelname, "police3"))
		result = CreateTexture("police/police3_hands.tga", 0, g_iHandsTexture_guess);
	else
		result = CreateTexture("police/police_hands.tga", 0, g_iHandsTexture_guess);

	if (strlen(gHUD.m_szFingerTexture_guess) > 2)
	{
		strcpy(texture, gHUD.m_szFingerTexture_guess);

		if (strstr(modelname, "police3"))
		{
			strcpy(texture, gHUD.m_szFingerTexture_guess - 4);
			strcat(texture, "_black.tga");
			result = CreateTexture(texture, 0, g_iHandsTexture_guess);
		}
		else
			result = CreateTexture(gHUD.m_szFingerTexture_guess, 0, g_iHandsTexture_guess);
	}

	if (result)
		g_bHandsLoaded_guess = true;
}

void LoadPoliceSleeveTexture_guess( int iUnused )
{
	if (!g_iPoliceSleeveTexture_guess)
		g_iPoliceSleeveTexture_guess = current_ext_texture_id++;

	if (g_bPoliceSleeveLoaded_guess)
		return;

	g_bPoliceSleeveLoaded_guess = false;

	int result;
	if (strlen(gHUD.m_szSleeveTexture_guess) > 2)
		result = CreateTexture(gHUD.m_szSleeveTexture_guess, 0, g_iPoliceSleeveTexture_guess);
	else
		result = CreateTexture("police/police_sleeve.tga", 0, g_iPoliceSleeveTexture_guess);

	if (result)
		g_bPoliceSleeveLoaded_guess = true;
}

void CaptureScreenTexture_guess( int width, int height )
{
	if (!g_iScreenTexture_guess)
	{
		unsigned char *data = new unsigned char[256*256*3];
		memset(data, 0, 256*256*3);

		gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_iScreenTexture_guess);
		gl.glBindTexture(GL_TEXTURE_2D, current_ext_texture_id);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl.glTexImage2D(GL_TEXTURE_2D, 0, 3, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		gl.glBindTexture(GL_TEXTURE_2D, g_iScreenTexture_guess);

		delete[] data;
		g_iScreenTexture_guess = current_ext_texture_id++;
	}

	gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_iOldTexture_guess);
	gl.glBindTexture(GL_TEXTURE_2D, g_iScreenTexture_guess);

	int x = ScreenWidth / 2;
	if (width)
		x = ScreenWidth / 2 - width / 2;

	int y = ScreenHeight / 2;
	if (height)
		y = ScreenHeight / 2 - height / 2;

	gl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, width, height, 0);
	g_bScreenTextureCaptured_guess = true;
}

void CaptureBlurTexture_guess( int width, int height, int iUnused1, int iUnused2 )
{
	if (!g_iBlurTexture_guess)
	{
		unsigned char *data = new unsigned char[256*256*3];
		memset(data, 0, 256*256*3);

		gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_iBlurTexture_guess);
		gl.glBindTexture(GL_TEXTURE_2D, current_ext_texture_id);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		gl.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gl.glTexImage2D(GL_TEXTURE_2D, 0, 3, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		gl.glBindTexture(GL_TEXTURE_2D, g_iBlurTexture_guess);

		delete[] data;
		g_iBlurTexture_guess = current_ext_texture_id++;
	}

	gl.glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_iOldTexture_guess);
	gl.glBindTexture(GL_TEXTURE_2D, g_iBlurTexture_guess);

	int x = ScreenWidth / 2;
	if (width)
		x = ScreenWidth / 2 - width / 2;

	int y = ScreenHeight / 2;
	if (height)
		y = ScreenHeight / 2 - height / 2;

	gl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, width, height, 0);
	g_bBlurTextureCaptured_guess = true;
}

void LoadSleeveTexture_guess( void )
{
	char texture[256];

	if (!gHUD.m_iClothes_guess && strlen(gHUD.m_szFingerTexture_guess) <= 2)
		return;

	if (!g_iSleeveTexture_guess)
		g_iSleeveTexture_guess = current_ext_texture_id++;

	if (g_bSleeveLoaded_guess)
		return;

	g_bSleeveLoaded_guess = false;

	switch (gHUD.m_iClothesState_guess)
	{
	case 1:
		sprintf(texture, "models/costumes/leatherhoff/Leatherhoff_Sleeve.tga");
		break;
	case 2:
		sprintf(texture, "models/costumes/moddb/ModDB_Sleeve.tga");
		break;
	case 3:
		sprintf(texture, "models/costumes/hello_kitty/Hello_Kitty_Sleeve.tga");
		break;
	case 5:
		sprintf(texture, "models/costumes/camo/Camo_Sleeve.tga");
		break;
	case 6:
		sprintf(texture, "models/costumes/hlc/HLC_Sleeve.tga");
		break;
	case 4:
	case 7:
	case 8:
	case 9:
		sprintf(texture, "models/costumes/awesome/Awesome_Sleeve.tga");
		break;
	case 10:
		sprintf(texture, "models/costumes/sick_simon/Sick_Simon_Sleeve.tga");
		break;
	case 11:
		sprintf(texture, "models/costumes/twitcher/twitcher_sleeve.tga");
		break;
	case 12:
		return;
	}

	if (strlen(gHUD.m_szSleeveTexture_guess) > 2)
		strcpy(texture, gHUD.m_szSleeveTexture_guess);

	if (CreateTexture(texture, 0, g_iSleeveTexture_guess))
		g_bSleeveLoaded_guess = true;
}

void LoadJeansTexture_guess( void )
{
	char texture[256];

	if (!gHUD.m_iClothes_guess && !g_iUnlockClothes_guess)
		return;

	if (!g_iJeansTexture_guess)
		g_iJeansTexture_guess = current_ext_texture_id++;

	if (g_bJeansLoaded_guess)
		return;

	g_bJeansLoaded_guess = false;

	int clothes = gHUD.m_iClothesState_guess;
	if (g_iUnlockClothes_guess)
		clothes = g_iUnlockClothes_guess;

	switch (clothes)
	{
	case 1:
		sprintf(texture, "models/costumes/leatherhoff/Leatherhoff_Jeans.tga");
		break;
	case 2:
	case 5:
	case 6:
	case 8:
		return;
	case 3:
		sprintf(texture, "models/costumes/hello_kitty/Hello_Kitty_Jeans.tga");
		break;
	case 4:
		sprintf(texture, "models/costumes/AoM/AoM_Jeans.tga");
		break;
	case 7:
		sprintf(texture, "models/costumes/black_metal/Black_Metal_Jeans.tga");
		break;
	case 9:
		sprintf(texture, "models/costumes/awesome/Awesome_Jeans.tga");
		break;
	case 10:
		sprintf(texture, "models/costumes/sick_simon/Sick_Simon_Jeans.tga");
		break;
	case 11:
		sprintf(texture, "models/costumes/twitcher/Twitcher_Jeans.tga");
		break;
	}

	if (CreateTexture(texture, 0, g_iJeansTexture_guess))
		g_bJeansLoaded_guess = true;
}

int		g_iWaterLoTexture_guess;
bool	g_bWaterLoLoaded_guess;
bool	g_bTakePhoto_guess;				// TakePhoto message, GL_DrawPoints captures the screen into the photo texture
bool	bUnknown104CE4A3_guess;			// read by V_CalcNormalRefdef with the attach camera

void LoadWaterLoTexture_guess( void )
{
	if (!g_iWaterLoTexture_guess)
		g_iWaterLoTexture_guess = current_ext_texture_id++;

	if (g_bWaterLoLoaded_guess)
		return;

	g_bWaterLoLoaded_guess = false;

	if (CreateTexture("textures/water_lo.tga", 0, g_iWaterLoTexture_guess))
		g_bWaterLoLoaded_guess = true;
}

// a copy of DistanceBetween_guess in rain.cpp (identical bytes at 100855B0)
float DistanceTo_guess( vec3_t a, vec3_t b )
{
	return Length(b - a);
}

//
// Cry of Fear: flashlights of the other players in multiplayer
// (called from StudioCalcAttachments, the light starts at attachment 3 of the player model)
//
void DrawPlayerFlashlights_guess( void )
{
	cl_entity_t *localplayer = gEngfuncs.GetLocalPlayer();

	if (pAttachCamEnt_guess)
		return;

	for (int i = 0; i < MAX_PLAYERS + 1; i++)
	{
		cl_entity_t *ent = gEngfuncs.GetEntityByIndex(i);

		if (!ent || !g_PlayerInfoList[i].name)
			continue;

		int flags = g_PlayerExtraInfo[i].frags;

		if ((flags & 1) || (flags & 8) || (flags & 32))
		{
			vec3_t origin, forward;
			origin = ent->attachment[3];

			if (ent == localplayer)
				gEngfuncs.pfnAngleVectors(v_angles, forward, NULL, NULL);
			else
			{
				vec3_t angles;
				VectorCopy(ent->angles, angles);
				angles[0] *= -1;
				gEngfuncs.pfnAngleVectors(angles, forward, NULL, NULL);
			}

			if (flags & 1)
			{
				vec3_t end = forward * 32 + origin;
				pmtrace_t *tr = gEngfuncs.PM_TraceLine(origin, end, PM_TRACELINE_PHYSENTSONLY, 1, -1);
				DynamicLight *dl = MY_AllocDlight(-i);
				dl->origin = tr->endpos;
				dl->angles = ent->angles;
				dl->radius = 60;
				dl->color = Vector(0.46, 0.46, 0.51);
				dl->die = gEngfuncs.GetClientTime() + 0.1;
			}

			if (flags & 8)
			{
				vec3_t end = forward * 4096 + origin;
				pmtrace_t *tr = gEngfuncs.PM_TraceLine(origin, end, PM_TRACELINE_PHYSENTSONLY, 1, -1);
				DynamicLight *dl = MY_AllocDlight(-i);
				dl->origin = tr->endpos;
				dl->angles = ent->angles;
				dl->radius = 60;
				dl->color = Vector(0.7, 0.65, 0.65);
				dl->die = gEngfuncs.GetClientTime() + 0.1;
			}

			if (flags & 32)
			{
				ent->curstate.body = 1;
				vec3_t end = forward * 128 + origin;
				gEngfuncs.PM_TraceLine(origin, end, PM_TRACELINE_PHYSENTSONLY, 1, -1);

				vec3_t angles;
				VectorCopy(ent->angles, angles);
				angles[0] *= -1;
				if (ent == localplayer)
					angles = v_angles;

				vec3_t lightorg;
				VectorCopy(ent->attachment[3], lightorg);
				if (localplayer == ent && !CL_IsThirdPerson())
					lightorg = v_origin;

				DynamicLight *dl = MY_AllocDlight(-i);
				dl->origin = lightorg;
				dl->angles = angles;
				dl->radius = 700;
				dl->color = Vector(0.7, 0.65, 0.65);
				dl->die = gEngfuncs.GetClientTime() + 0.1;
				dl->cone_hor = 50;
				dl->cone_ver = 50;
				if (cl_lowflashlight->value == 1)
					strcpy(dl->spot_texture, "gfx/low/flashlight2.tga");
				else if (cl_lowflashlight->value == 0)
					strcpy(dl->spot_texture, "gfx/flashlight2.tga");
			}
		}
		else
			ent->curstate.body = 0;
	}
}


//================================================
// GL_SetupTextureHeader
//
// gets access to external textures for model
// thanks to XaeroX
//================================================
void CStudioModelRenderer::GL_SetupTextureHeader()
{
	//Get texture header
	if (!m_pStudioHeader->numtextures || !m_pStudioHeader->textureindex)
	{
		char texturename[256];		

		strcpy( texturename, m_pRenderModel->name );
		strcpy( &texturename[strlen(texturename) - 4], "T.mdl" );

		model_t *textures = IEngineStudio.Mod_ForName(texturename, 0);
		if (!textures)
		{
			m_pTextureHeader = NULL;
			CONPRINT("Mod_ForName: failed for %s\n", texturename);
			return;
		}

		m_pTextureHeader = (studiohdr_t *)IEngineStudio.Mod_Extradata(textures);
		if (!m_pTextureHeader)
			CONPRINT("Mod_Extradata: failed for %s\n", texturename);
	}
	else
	{
		m_pTextureHeader = m_pStudioHeader;
	}
}



void CStudioModelRenderer::GL_SetupRenderer( int rendermode )
{
	g_pxformverts = &g_xformverts[0];
	g_pvlightvalues = &g_lightvalues[0];

/*	gl.glPushMatrix ();

    gl.glTranslatef (m_pCurrentEntity->origin[0], m_pCurrentEntity->origin[1], m_pCurrentEntity->origin[2]);

    gl.glRotatef (m_pCurrentEntity->angles[1],  0, 0, 1);
    gl.glRotatef (m_pCurrentEntity->angles[0],  0, 1, 0);
    gl.glRotatef (m_pCurrentEntity->angles[2],  1, 0, 0);*/

	gl.glShadeModel (GL_SMOOTH);
//	gl.glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	alpha = 1;
	useblending = FALSE;
	if (rendermode == kRenderTransAdd)
	{
		gl.glDepthMask(GL_FALSE);
		gl.glEnable(GL_BLEND);
		gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		alpha = (float)m_pCurrentEntity->curstate.renderamt / 255.0;
		useblending = TRUE;
	}
	else if (rendermode == kRenderTransAlpha || rendermode == kRenderTransTexture)
	{
		gl.glDepthMask(GL_FALSE);
		gl.glEnable(GL_BLEND);
		gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		alpha = (float)m_pCurrentEntity->curstate.renderamt / 255.0;
		useblending = TRUE;
	}

	// buz: DAMN! another texture_env_combine bug! It works perfectly until
	// blending is enabled. When i turning on blending, the things becomes much darker!
	// Maybe driver bug, i dunno.
	// On register combiners everything works fine in any case, so let it use
	// register combiners if they supported..

	if (!gl.NV_combiners_supported)
	{
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 2);

	/*	gl.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_TEXTURE);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_PRIMARY_COLOR_ARB);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);*/
	}
	else
	{
		gl.glEnable ( GL_REGISTER_COMBINERS_NV );
		gl.glCombinerParameteriNV ( GL_NUM_GENERAL_COMBINERS_NV, 1 );

		// RC 0 setup:
		//	spare0.rgb = (tex0.rgb * primary_color.rgb) * 2
		gl.glCombinerInputNV ( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB,
							   GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glCombinerInputNV ( GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV,
							   GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glCombinerOutputNV ( GL_COMBINER0_NV, GL_RGB,
								GL_SPARE0_NV,          // AB output
								GL_DISCARD_NV,         // CD output
								GL_DISCARD_NV,         // sum output
								GL_SCALE_BY_TWO_NV, GL_NONE,
								GL_FALSE,               // AB = A * B
								GL_FALSE, GL_FALSE );

		//	spare0.a = tex0.a * primary_color.a
		gl.glCombinerInputNV ( GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_A_NV, GL_TEXTURE0_ARB,
							   GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );
		gl.glCombinerInputNV ( GL_COMBINER0_NV, GL_ALPHA, GL_VARIABLE_B_NV, GL_PRIMARY_COLOR_NV,
							   GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );
		gl.glCombinerOutputNV ( GL_COMBINER0_NV, GL_ALPHA,
								GL_SPARE0_NV,          // AB output
								GL_DISCARD_NV,         // CD output
								GL_DISCARD_NV,         // sum output
								GL_NONE, GL_NONE,
								GL_FALSE,
								GL_FALSE, GL_FALSE );

		// Final RC setup:
		//	out.rgb = spare0.rgb
		//	out.a = spare0.a
		gl.glFinalCombinerInputNV ( GL_VARIABLE_A_NV, GL_ZERO,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_B_NV, GL_ZERO,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_C_NV, GL_ZERO,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_D_NV, GL_SPARE0_NV,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_E_NV, GL_ZERO,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_F_NV, GL_ZERO,
									GL_UNSIGNED_IDENTITY_NV, GL_RGB );
		gl.glFinalCombinerInputNV ( GL_VARIABLE_G_NV, GL_SPARE0_NV,
									GL_UNSIGNED_IDENTITY_NV, GL_ALPHA );
	}

	// buz special hack for transparent parts on head shield -
	// use less alpha
	if (g_iHeadShieldType_guess == 0)
	{
		if (!strcmp(m_pRenderModel->name, "models/nv_goggles.mdl"))
			alpha = 0.7;
	}
	else if (g_iHeadShieldType_guess == 10)
	{
		if (!strcmp(m_pRenderModel->name, "models/weapons/v_gasmask.mdl") || !strcmp(m_pRenderModel->name, "models/weapons/v_gasmask_simon.mdl"))
			alpha = 0.7;
	}
	else if (g_iHeadShieldType_guess == 1)
	{
		if (!strcmp(m_pRenderModel->name, "models/items/tv_frame.mdl"))
			alpha = 0.7;
	}
	else if (g_iHeadShieldType_guess == 2)
	{
		if (iDoctorMode_guess)
		{
			if (!strcmp(m_pRenderModel->name, "models/weapons/v_death_doctor.mdl"))
				alpha = 0.7;
		}
		else
		{
			if (!strcmp(m_pRenderModel->name, "models/weapons/v_death.mdl"))
				alpha = 0.7;
		}
	}
	else if (g_iHeadShieldType_guess == 5)
	{
		if (!strcmp(m_pRenderModel->name, "models/props/blandat/v_telescope.mdl"))
			alpha = 0.7;
	}
	else if (g_iHeadShieldType_guess == 6 && gHUD.m_pszHeadShieldSound_guess)
	{
		if (!strcmp(m_pRenderModel->name, gHUD.m_pszHeadShieldSound_guess))
			alpha = 0.7;
	}
}

void CStudioModelRenderer::GL_RestoreRenderer()
{
//	gl.glPopMatrix ();

	if (!gl.NV_combiners_supported)
		gl.glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
	else
		gl.glDisable ( GL_REGISTER_COMBINERS_NV );

	gl.glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	gl.glShadeModel (GL_FLAT);
//	glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	if (useblending)
	{
		gl.glDepthMask(GL_TRUE);
		gl.glDisable(GL_BLEND);
	}
}


// =================================================================

void VectorRotate (const vec3_t &in1, const float in2[3][4], vec3_t &out)
{
	out[0] = DotProduct(in1, in2[0]);
	out[1] = DotProduct(in1, in2[1]);
	out[2] = DotProduct(in1, in2[2]);
}


// rotate by the inverse of the matrix
void VectorIRotate (const vec3_t &in1, const float in2[3][4], vec3_t &out)
{
	out[0] = in1[0]*in2[0][0] + in1[1]*in2[1][0] + in1[2]*in2[2][0];
	out[1] = in1[0]*in2[0][1] + in1[1]*in2[1][1] + in1[2]*in2[2][1];
	out[2] = in1[0]*in2[0][2] + in1[1]*in2[1][2] + in1[2]*in2[2][2];
}


#define VectorMaximum(a) ( max( (a)[0], max( (a)[1], (a)[2] ) ) )


// =================================================================
/*
void CStudioModelRenderer::Lighting (float *lv, int bone, int flags, vec3_t normal)
{
	float 	illum;
	float	lightcos;

	illum = g_ambientlight;

	if (flags & STUDIO_NF_FLATSHADE)
	{
		illum += g_shadelight * 0.8;
	} 
	else 
	{
		float r;
		lightcos = DotProduct (normal, g_blightvec[bone]); // -1 colinear, 1 opposite

		if (lightcos > 1.0)
			lightcos = 1;

		illum += g_shadelight;

		r = g_lambert;
		if (r <= 1.0) r = 1.0;

		lightcos = (lightcos + (r - 1.0)) / r; 		// do modified hemispherical lighting
		if (lightcos > 0.0) 
		{
			illum -= g_shadelight * lightcos; 
		}
		if (illum <= 0)
			illum = 0;
	}

	if (illum > 255) 
		illum = 255;
	*lv = illum / 255.0;	// Light from 0 to 1.0
}*/


// buz: colored
void CStudioModelRenderer::Lighting (vec3_t &lv, int bone, int flags, vec3_t normal)
{
	vec3_t 	illum;
	float	lightcos;

	illum = light.ambientlight;

	if (flags & STUDIO_NF_FLATSHADE)
	{
		VectorMA(illum, 0.8, light.addlight, illum);
	} 
	else 
	{
		float r;
		lightcos = DotProduct (normal, g_blightvec[bone]); // -1 colinear, 1 opposite

		if (lightcos > 1.0)
			lightcos = 1;

		VectorAdd(illum, light.addlight, illum);

		r = cv_lambert->value;
		if (r <= 1.0) r = 1.0;

		lightcos = (lightcos + (r - 1.0)) / r; 		// do modified hemispherical lighting
		if (lightcos > 0.0) 
		{
			VectorMA(illum, -lightcos, light.addlight, illum);
		}
		
		if (illum[0] <= 0) illum[0] = 0;
		if (illum[1] <= 0) illum[1] = 0;
		if (illum[2] <= 0) illum[2] = 0;

		// buz: now add all dynamic lights
		for (int i = 0; i < g_numdynlights; i++)
		{
			lightcos = -(DotProduct (normal, g_bdynlightvec[i][bone]));
			if (lightcos > 0)
				VectorMA(illum, lightcos, g_dynlightcolor[i], illum);
		}
	}

	float max = VectorMaximum(illum);
	if (max > 1.0)
	{
		float scale = 1.0 / max;

		illum[0] *= scale;
		illum[1] *= scale;
		illum[2] *= scale;
	}

/*	if (illum[0] > 1.0) illum[0] = 1.0;
	if (illum[1] > 1.0) illum[1] = 1.0;
	if (illum[2] > 1.0) illum[2] = 1.0;*/

	lv = illum;

	// Cry of Fear: attachments of the left hand weapon (DualWield), for its muzzle flash and light
	cl_entity_t *pSavedEntity = m_pCurrentEntity;

	if (g_iDualLighting_guess && !(gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS) && g_pHeadShieldEnt_guess)
	{
		m_pCurrentEntity = g_pHeadShieldEnt_guess;
		StudioCalcAttachments();
		IEngineStudio.StudioClientEvents();

		if (m_pCurrentEntity->index > 0)
		{
			cl_entity_t *ent = gEngfuncs.GetEntityByIndex(m_pCurrentEntity->index);
			memcpy(ent->attachment, m_pCurrentEntity->attachment, sizeof(vec3_t) * 4);
		}

		if (g_iDualLighting_guess)
		{
			vecDualAttachment0_guess = m_pCurrentEntity->attachment[0];
			vecDualAttachment1_guess = m_pCurrentEntity->attachment[1];
			g_iDualLighting_guess = 0;
		}
	}

	m_pCurrentEntity = pSavedEntity;
}


void CStudioModelRenderer::Chrome (int *pchrome, int bone, vec3_t normal)
{
	float n;

	if (g_chromeage[bone] != *m_pStudioModelCount)
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		vec3_t chromeupvec;		// g_chrome t vector in world reference frame
		vec3_t chromerightvec;	// g_chrome s vector in world reference frame
		vec3_t tmp;				// vector pointing at bone in world reference frame
		VectorScale( m_pCurrentEntity->origin, -1, tmp );
		tmp[0] += (*m_pbonetransform)[bone][0][3];
		tmp[1] += (*m_pbonetransform)[bone][1][3];
		tmp[2] += (*m_pbonetransform)[bone][2][3];
		VectorNormalize( tmp );
		CrossProduct( tmp, m_vRight, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( tmp, chromeupvec, chromerightvec );
		VectorNormalize( chromerightvec );

		VectorIRotate( chromeupvec, (*m_pbonetransform)[bone], g_chromeup[bone] );
		VectorIRotate( chromerightvec, (*m_pbonetransform)[bone], g_chromeright[bone] );

		g_chromeage[bone] = *m_pStudioModelCount;
	}

	// calc s coord
	n = DotProduct( normal, g_chromeright[bone] );
	pchrome[0] = (n + 1.0) * 32; // FIX: make this a float

	// calc t coord
	n = DotProduct( normal, g_chromeup[bone] );
	pchrome[1] = (n + 1.0) * 32; // FIX: make this a float
}


/*
================
StudioModel::SetupLighting
	set some global variables based on entity position
inputs:
outputs:
	g_ambientlight
	g_shadelight
================
*/
void CStudioModelRenderer::GL_SetupLighting ()
{
	int isheadshield = FALSE;

	if (m_pCurrentEntity->curstate.renderfx == 60) // light tester entity
		return;

	// Cry of Fear: every head shield model gets the head shield lighting
	if (g_iHeadShieldType_guess == 0 && !strcmp(m_pRenderModel->name, "models/nv_goggles.mdl"))
		isheadshield = TRUE;
	else if (g_iHeadShieldType_guess == 7 && !strcmp(m_pRenderModel->name, "models/bookmenu.mdl"))
		isheadshield = TRUE;
	if (g_iHeadShieldType_guess == 10 && (!strcmp(m_pRenderModel->name, "models/weapons/v_gasmask.mdl") || !strcmp(m_pRenderModel->name, "models/weapons/v_gasmask_simon.mdl")))
		isheadshield = TRUE;
	if (g_iHeadShieldType_guess == 1 && !strcmp(m_pRenderModel->name, "models/items/tv_frame.mdl"))
		isheadshield = TRUE;
	if (g_iHeadShieldType_guess == 5 && !strcmp(m_pRenderModel->name, "models/props/blandat/v_telescope.mdl"))
		isheadshield = TRUE;
	if (g_iHeadShieldType_guess == 6 && gHUD.m_pszHeadShieldSound_guess && !strcmp(m_pRenderModel->name, gHUD.m_pszHeadShieldSound_guess))
		isheadshield = TRUE;

	vec3_t center = (m_pCurrentEntity->curstate.mins + m_pCurrentEntity->curstate.maxs) / 2;
	vec3_t point = m_pCurrentEntity->origin + center;

/*	light.addlight = Vector(0.5, 0.5, 0.5);
	light.ambientlight = Vector(0.5, 0.5, 0.5);
	light.lightdir = Vector(0, 0, -1);*/

	EXT_LightPoint(point, &light, (m_pCurrentEntity->curstate.effects & EF_INVLIGHT));

	// use inverted light vector for head shield (hack)
	if (!isheadshield)
		VectorInverse(light.lightdir); // we need vector FROM light, not TO light

//	CONPRINT("got amblight: %f, %f, %f\n", light.ambientlight[0], light.ambientlight[1], light.ambientlight[2]);
//	CONPRINT("got diffuse: %f, %f, %f\n", light.addlight[0], light.addlight[1], light.addlight[2]);
//	CONPRINT("vector: %f, %f, %f\n", light.lightdir[0], light.lightdir[1], light.lightdir[2]);

//	light.ambientlight = Vector(0, 0, 0);
//	light.addlight = Vector(1, 1, 1);
//	light.lightdir = Vector(0, -1, 0);

	// Cry of Fear: fixed lighting for some models and render effects
	if (!strcmp(m_pRenderModel->name, "models/flash1.mdl") || !strcmp(m_pRenderModel->name, "models/flash2.mdl"))
		light.ambientlight = Vector(0.15, 0.15, 0.15);

	if (g_DrawHeadShied && (g_iHeadShieldType_guess == 6 || g_iHeadShieldType_guess == 7))
		light.ambientlight = Vector(0.15, 0.15, 0.15);

	if (m_pCurrentEntity->curstate.renderfx == 100)
	{
		float bright = gEngfuncs.pfnGetCvarFloat("brightvalue");
		light.ambientlight = Vector(bright, bright, bright);
	}

	if (m_pCurrentEntity->curstate.renderfx == 137)
	{
		float bright = gEngfuncs.pfnGetCvarFloat("skybrightvalue");
		float r = gEngfuncs.pfnGetCvarFloat("skybrightcolourr");
		float g = gEngfuncs.pfnGetCvarFloat("skybrightcolourg");
		float b = gEngfuncs.pfnGetCvarFloat("skybrightcolourb");

		if (!strcmp(m_pRenderModel->name, "models/props/sky_stars.mdl"))
		{
			bright = gEngfuncs.pfnRandomFloat(-0.1, 0.1) + 0.5;
			r = gEngfuncs.pfnRandomFloat(-0.1, 0.1) + 0.1;
			light.ambientlight[0] = bright;
			light.ambientlight[1] = bright;
			light.ambientlight[2] = bright;
			light.addlight[0] = r;
			light.addlight[1] = r;
			light.addlight[2] = r;
		}
		else
		{
			light.ambientlight[0] = bright;
			light.ambientlight[1] = bright;
			light.ambientlight[2] = bright;
			light.addlight[0] = r;
			light.addlight[1] = g;
			light.addlight[2] = b;
		}
	}

	if (m_pCurrentEntity->curstate.renderfx > 100 && m_pCurrentEntity->curstate.renderfx <= 200 && m_pCurrentEntity->curstate.renderfx != 101 && m_pCurrentEntity->curstate.renderfx != 137)
	{
		float bright = (float)(m_pCurrentEntity->curstate.renderfx - 100) / 100;
		light.ambientlight = Vector(bright, bright, bright);
	}

	if (m_pCurrentEntity->curstate.renderfx == 101)
	{
		light.ambientlight = Vector(0.1, 0.1, 0.1);
		light.addlight = Vector(0.15, 0.1, 0.1);
	}

	if (m_pCurrentEntity->curstate.renderfx == 167)
	{
		light.ambientlight = Vector(0.1, 0.1, 0.1);
		light.addlight = Vector(0.1, 0.1, 0.1);
	}

	if (m_pCurrentEntity->curstate.renderfx == 99 || m_pCurrentEntity->curstate.renderfx == 123 || m_pCurrentEntity->curstate.renderfx == 245 || m_pCurrentEntity->curstate.renderfx == 165)
	{
		static bool bFlicker_guess;

		float dist = DistanceTo_guess(v_origin, m_pCurrentEntity->curstate.origin) - 64;
		if (dist < 0)
			dist = 0;
		if (dist > 255.0)
			dist = 255;

		float flicker;
		if (bFlicker_guess)
		{
			flicker = (float)((int)(gHUD.m_flTime * 10) % 25) / 100;
			if (flicker <= 0)
			{
				bFlicker_guess = false;
				flicker = 0;
			}
			else if (flicker >= 0.25)
				flicker = 0.25;
		}
		else
		{
			flicker = (float)((int)(gHUD.m_flTime * 10) % 25) / 100;
			if (flicker <= 0)
			{
				bFlicker_guess = true;
				flicker = 0;
			}
			else if (flicker >= 0.25)
				flicker = 0.25;
		}

		gHUD.m_flUnknown2A84_guess = flicker * 4;

		float add = 0.25 - gHUD.m_flUnknown2A84_guess;
		if (add > 0.25)
			add = 0.25;
		if (add < 0)
			add = 0;

		switch (iDifficulty_guess)
		{
		case 0:
		case 2:
			if (dist < 96)
			{
				light.ambientlight = Vector(0.25, 0.25, 0.25);
				if (bFlicker_guess)
					light.addlight = Vector(add, 0.0f, 0.0f);
				else
					light.addlight = Vector(gHUD.m_flUnknown2A84_guess, 0.0f, 0.0f);
			}
			break;
		case 1:
			if (dist < 96)
			{
				light.ambientlight = Vector(0.5, 0.5, 0.5);
				if (bFlicker_guess)
					light.addlight = Vector(add, 0.0f, 0.0f);
				else
					light.addlight = Vector(gHUD.m_flUnknown2A84_guess, 0.0f, 0.0f);
			}
			break;
		case 3:
			if (dist < 96)
			{
				light.ambientlight = Vector(0.1, 0.1, 0.1);
				if (bFlicker_guess)
					light.addlight = Vector(add, 0.0f, 0.0f);
				else
					light.addlight = Vector(gHUD.m_flUnknown2A84_guess, 0.0f, 0.0f);
			}
			break;
		}
	}

	// TODO: only do it for bones that actually have textures
	int i;
	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
	//	VectorIRotate( g_lightvec, (*m_pbonetransform)[i], g_blightvec[i] );
		VectorIRotate( light.lightdir, (*m_pbonetransform)[i], g_blightvec[i] );
	}

	//
	// buz: Dynamic lighting
	//

//	CONPRINT("model: %s\n", m_pRenderModel->name);

	g_numdynlights = 0;
	DynamicLight* dlights[32];

	if (isheadshield) // player will get crazy of all this full-screen flickering when somebody shoots..
		return;

	int isViewModel = (m_pCurrentEntity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;

	// Cry of Fear: the view model is brighter while the flashlight is on
	if ((gHUD.m_iFlashFlags_guess & 8) && isViewModel)
		light.addlight = Vector(0.2, 0.2, 0.2);

	int numlights;
	numlights = GetDlightsForPoint(m_pCurrentEntity->origin,
		m_pCurrentEntity->curstate.mins, m_pCurrentEntity->curstate.maxs, dlights, isViewModel);

	for (i = 0; i < numlights; i++)
	{
		if (g_numdynlights == MAX_MODEL_DYNLIGHTS)
			break;

		vec3_t forward; // light forward for spotlights check
		float spotcos;
		if (dlights[i]->spot_texture[0])
		{
			AngleVectors(dlights[i]->angles, forward, NULL, NULL);
			spotcos = cos((dlights[i]->cone_hor + dlights[i]->cone_ver)*0.3*(M_PI*2/360));
		}

		float r = dlights[i]->radius * dlights[i]->radius; // squared radius

		// use special hack view models when they lit by flashlight -
		// light should come from back of model, angles independent.
		// This just looks cool.
		int isViewModelHack = FALSE;
		vec3_t alternateOrigin;
		if (isViewModel && dlights[i]->key == -666)
		{
			isViewModelHack = TRUE;
			vec3_t forward, right, angles;
			angles = m_pCurrentEntity->angles;
			angles[0] = -angles[0];
			AngleVectors(angles, forward, right, NULL);
			alternateOrigin = m_pCurrentEntity->origin - (forward * 13) + (right * 5);
		}

		for (int b = 0; b < m_pStudioHeader->numbones; b++)
		{
			vec3_t vec;
			if (isViewModelHack)
			{
				vec[0] = (*m_pbonetransform)[b][0][3] - alternateOrigin[0];
				vec[1] = (*m_pbonetransform)[b][1][3] - alternateOrigin[1];
				vec[2] = (*m_pbonetransform)[b][2][3] - alternateOrigin[2];
			}
			else
			{
				vec[0] = (*m_pbonetransform)[b][0][3] - dlights[i]->origin[0];
				vec[1] = (*m_pbonetransform)[b][1][3] - dlights[i]->origin[1];
				vec[2] = (*m_pbonetransform)[b][2][3] - dlights[i]->origin[2];
			}

			float dist = DotProduct(vec, vec);
			float atten = (dist / r - 1) * -1;
			if (atten < 0) atten = 0;
			dist = sqrt(dist);
			if (dist)
			{
				dist = 1/dist;
				vec[0] *= dist;
				vec[1] *= dist;
				vec[2] *= dist;
			}

			VectorIRotate( vec, (*m_pbonetransform)[b], g_bdynlightvec[g_numdynlights][b] );
			VectorScale( g_bdynlightvec[g_numdynlights][b], atten, g_bdynlightvec[g_numdynlights][b] );

			if (dlights[i]->spot_texture[0])
			{
				// calc spotlight cone
				atten = DotProduct(forward, vec);
				if (atten < spotcos)
					VectorClear(g_bdynlightvec[g_numdynlights][b]);
				else
					VectorScale(g_bdynlightvec[g_numdynlights][b], (atten - spotcos)/(1 - spotcos), g_bdynlightvec[g_numdynlights][b]);
			}
		}

		g_dynlightcolor[g_numdynlights][0] = ApplyGamma(dlights[i]->color[0]) / 2;
		g_dynlightcolor[g_numdynlights][1] = ApplyGamma(dlights[i]->color[1]) / 2;
		g_dynlightcolor[g_numdynlights][2] = ApplyGamma(dlights[i]->color[2]) / 2;
		g_numdynlights++;
	}

	// Cry of Fear: render effects 31 to 40 darken the model
	if (m_pCurrentEntity->curstate.renderfx > 30 && m_pCurrentEntity->curstate.renderfx <= 40)
	{
		int dark = m_pCurrentEntity->curstate.renderfx - 30;
		if (dark < 0)
			dark = 0;
		if (dark > 10)
			dark = 10;

		vec3_t amb = light.ambientlight;
		float scale = dark / 10.0f;
		amb[0] -= scale;
		amb[1] -= scale;
		amb[2] -= scale;
		light.ambientlight = amb;
	}
}


/*
=================
StudioModel::SetupModel
	based on the body part, figure out which mesh it should be using.
inputs:
	currententity
outputs:
	pstudiomesh
	pmdl
=================
*/
void CStudioModelRenderer::GL_SetupModel ( int bodypart )
{
	int index;

	if (bodypart > m_pStudioHeader->numbodyparts)
	{
		CONPRINT("SetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = m_pCurrentEntity->curstate.body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}




void CStudioModelRenderer::GL_DrawPoints ( )
{
	// Cry of Fear: nothing is drawn in this mode
	if (g_iDickMode_guess)
		return;

	bool bMultiplayer = false;
	if (gEngfuncs.GetMaxClients() > 1)
		bMultiplayer = true;

	// Cry of Fear: no view model while the view is on the boat, the attach camera or the telescope
	int isViewModel = (m_pCurrentEntity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;
	if (isViewModel && (iBoat_guess || pAttachCamEnt_guess || pTelescopeEnt_guess))
		return;

	if (isViewModel && iUnknown1054506C_guess >= 3 && gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId != 17)
		return;

	// Cry of Fear: props with render effects 92 to 94 are not drawn far from the player
	if (m_pCurrentEntity->curstate.renderfx == 92 && !IsViewEntityActive_guess() && !pAttachCamEnt_guess)
	{
		const char *level = gEngfuncs.pfnGetLevelName();
		if (cl_propdistance->value || (level && (strstr(level, "college1dark") || strstr(level, "forest1") || strstr(level, "forest2") ||
			strstr(level, "forest3") || strstr(level, "forest4") || strstr(level, "forest5") || strstr(level, "forest6"))))
		{
			cl_entity_t *player = gEngfuncs.GetLocalPlayer();
			if (player)
			{
				vec3_t dist;
				if (iBoat_guess)
				{
					VectorSubtract(vecBoatOrigin_guess, m_pCurrentEntity->origin, dist);
				}
				else if (pTelescopeEnt_guess)
				{
					VectorSubtract(pTelescopeEnt_guess->origin, m_pCurrentEntity->origin, dist);
				}
				else
				{
					VectorSubtract(player->origin, m_pCurrentEntity->origin, dist);
				}

				if (level && (strstr(level, "college1dark") || strstr(level, "forest1") || strstr(level, "forest2") ||
					strstr(level, "forest3") || strstr(level, "forest4") || strstr(level, "forest5") || strstr(level, "forest6")))
				{
					if (Length(dist) > 800)
						return;
				}

				if (Length(dist) > 1000)
					return;
			}
		}
	}

	if (m_pCurrentEntity->curstate.renderfx == 93 && !IsViewEntityActive_guess() && !pAttachCamEnt_guess)
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		if (player)
		{
			vec3_t dist;
			VectorSubtract(player->origin, m_pCurrentEntity->origin, dist);
			if (Length(dist) > 1000)
				return;
		}
	}

	if (m_pCurrentEntity->curstate.renderfx == 94 && !IsViewEntityActive_guess() && !pAttachCamEnt_guess)
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		if (player)
		{
			vec3_t dist;
			VectorSubtract(player->origin, m_pCurrentEntity->origin, dist);
			if (Length(dist) > 600)
				return;
		}
	}

	byte *pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	byte *pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);

	if (!m_pTextureHeader)
		return;

	if (m_pCurrentEntity->curstate.renderfx == 60) // light tester entity
		return;

	mstudiotexture_t *ptexture = (mstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);

	mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);

	vec3_t *pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	vec3_t *pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);

	int skinnum = m_pCurrentEntity->curstate.skin; // for short..

	short *pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
	if (skinnum != 0 && skinnum < m_pTextureHeader->numskinfamilies)
		pskinref += (skinnum * m_pTextureHeader->numskinref);

	int i;
	for (i = 0; i < m_pSubModel->numverts; i++)
	{
		VectorTransform (pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], g_pxformverts[i]);
	}

//
// clip and draw all triangles
//

	bool bBlurTexture = false;

	vec3_t *lv = g_pvlightvalues;
	int j;
	for (j = 0; j < m_pSubModel->nummesh; j++)
	{
		int flags = ptexture[pskinref[pmesh[j].skinref]].flags;
		for (i = 0; i < pmesh[j].numnorms; i++, lv++, pstudionorms++, pnormbone++)
		{
		//	float lv_tmp;
		//	Lighting (&lv_tmp, *pnormbone, flags, (float *)pstudionorms);
			Lighting (*lv, *pnormbone, flags, (float *)pstudionorms); // buz

			// FIX: move this check out of the inner loop
			if (flags & STUDIO_NF_CHROME)
				Chrome( g_chrome[(float (*)[3])lv - (float (*)[3])g_pvlightvalues], *pnormbone, (float *)pstudionorms );

		//	lv[0] = lv_tmp * g_lightcolor[0];
		//	lv[1] = lv_tmp * g_lightcolor[1];
		//	lv[2] = lv_tmp * g_lightcolor[2];
		}
	}

	gl.glCullFace(GL_FRONT);

	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for (j = 0; j < m_pSubModel->nummesh; j++)
	{
		float s, t;
		short		*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		mstudiotexture_t *ptex = &ptexture[pskinref[pmesh->skinref]];

		s = 1.0/(float)ptex->width;
		t = 1.0/(float)ptex->height;

		// Cry of Fear: replacement textures for the clothes, the hands and the screens
		if (strstr(ptex->name, "hoodie_grey") || strstr(ptex->name, "hoodie_blood"))
		{
			if (gHUD.m_iClothes_guess && !strstr(m_pRenderModel->name, "unfolded_clothes") && !strstr(m_pRenderModel->name, "booksimon"))
			{
				LoadHoodieTexture_guess(false);
				if (g_bHoodieLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iHoodieTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else if (strstr(m_pRenderModel->name, "unfolded_clothes") && g_iUnlockClothes_guess > 0)
			{
				LoadHoodieTexture_guess(true);
				if (g_bHoodieLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iHoodieTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "jeans") && !strstr(ptex->name, "jeanss"))
		{
			if ((gHUD.m_iClothes_guess || g_iUnlockClothes_guess) &&
				!strstr(m_pRenderModel->name, "badkarne1") &&
				!strstr(m_pRenderModel->name, "sophie.mdl") &&
				!strstr(m_pRenderModel->name, "tortyrscen") &&
				!strstr(m_pRenderModel->name, "deadvictim") &&
				!strstr(m_pRenderModel->name, "booksimon") &&
				!strstr(m_pRenderModel->name, "coopend") &&
				!strstr(m_pRenderModel->name, "coop") &&
				!strstr(m_pRenderModel->name, "gettingshotgun") &&
				!strstr(m_pRenderModel->name, "booksimon") &&
				!strstr(m_pRenderModel->name, "sophie_dead"))
			{
				LoadJeansTexture_guess();
				if (g_bJeansLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iJeansTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "head_face"))
		{
			if (gHUD.m_iClothes_guess && !strstr(m_pRenderModel->name, "booksimon"))
			{
				LoadFaceTexture_guess();
				if (g_bFaceLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iFaceTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "page"))
		{
			// pages of the book menu that are not unlocked yet
			if (g_DrawHeadShied)
			{
				if (strstr(ptex->name, "page1") && !(gHUD.m_iUnlocks_guess & 0x2000000))
				{
					LoadPageLockedTexture_guess();
					if (g_bPageLockedLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPageLockedTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else if (strstr(ptex->name, "page2") && !(gHUD.m_iUnlocks_guess & 0x4000000))
				{
					LoadPageLockedTexture_guess();
					if (g_bPageLockedLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPageLockedTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else if (strstr(ptex->name, "page3") && !(gHUD.m_iUnlocks_guess & 0x8000000))
				{
					LoadPageLockedTexture_guess();
					if (g_bPageLockedLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPageLockedTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else if (strstr(ptex->name, "page4") && !(gHUD.m_iUnlocks_guess & 0x10000000))
				{
					LoadPageLockedTexture_guess();
					if (g_bPageLockedLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPageLockedTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else if (strstr(ptex->name, "page5") && !(gHUD.m_iUnlocks_guess & 0x20000000))
				{
					LoadPageLockedTexture_guess();
					if (g_bPageLockedLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPageLockedTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "sleeve"))
		{
			int isViewModel = (m_pCurrentEntity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;
			if (isViewModel && gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId == 20)
				isViewModel = FALSE;

			if (bMultiplayer)
			{
				if (g_DrawHeadShied || isViewModel || (pAttachCamEnt_guess && m_pCurrentEntity == pAttachCamEnt_guess))
				{
					LoadPoliceSleeveTexture_guess(0);
					if (g_bPoliceSleeveLoaded_guess)
						gl.glBindTexture( GL_TEXTURE_2D, g_iPoliceSleeveTexture_guess );
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else if (!gHUD.m_iClothes_guess && strlen(gHUD.m_szSleeveTexture_guess) <= 2)
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			else
			{
				LoadSleeveTexture_guess();
				if (g_bSleeveLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iSleeveTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
		}
		else if (strstr(ptex->name, "fingers"))
		{
			int isViewModel = (m_pCurrentEntity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;

			if (!bMultiplayer && !bCustomFingerTexture_guess)
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			else if (!g_DrawHeadShied && !isViewModel && (!pAttachCamEnt_guess || m_pCurrentEntity != pAttachCamEnt_guess))
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			else
			{
				// the hands of the player model
				cl_entity_t *player = gEngfuncs.GetLocalPlayer();
				if (player && player->model)
				{
					int index = player->index - 1;
					model_t *model;

					if (index >= 0 && index <= gEngfuncs.GetMaxClients() && (model = IEngineStudio.SetupPlayerModel(index)) != NULL)
					{
						LoadHandsTexture_guess(model->name);
						if (g_bHandsLoaded_guess)
							gl.glBindTexture( GL_TEXTURE_2D, g_iHandsTexture_guess );
						else
							gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
					}
					else
						gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
				}
			}
		}
		else if (strstr(ptex->name, "glove"))
		{
			if (m_pCurrentEntity == gEngfuncs.GetViewModel() && bCustomGloveTexture_guess)
			{
				LoadGloveTexture_guess(0);
				if (g_bGloveLoaded_guess)
					gl.glBindTexture( GL_TEXTURE_2D, g_iGloveTexture_guess );
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "cam_vfind"))
		{
			// camera viewfinder shows the blurred screen
			if (cl_cameraeffect->value)
			{
				CaptureBlurTexture_guess(ptex->width, ptex->height, 0, 0);
				if (g_bBlurTextureCaptured_guess)
				{
					bBlurTexture = true;
					gl.glBindTexture( GL_TEXTURE_2D, g_iBlurTexture_guess );
					g_bBlurTextureCaptured_guess = false;
				}
				else
					gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
			}
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else if (strstr(ptex->name, "diffusegay"))
		{
			// photo taken by the TakePhoto message
			if (g_bTakePhoto_guess)
			{
				CaptureScreenTexture_guess(ptex->width, ptex->height);
				g_bTakePhoto_guess = false;
			}

			if (g_bScreenTextureCaptured_guess)
				gl.glBindTexture( GL_TEXTURE_2D, g_iScreenTexture_guess );
			else
				gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );
		}
		else
			gl.glBindTexture( GL_TEXTURE_2D, (ptex->index & 0xFFFF) );

	//	byte *pal = (byte *)m_pStudioHeader + ptex->width * ptex->height + ((ptex->index>>16) & 0xFFFF);
	//	gl.glColorTableEXT(0x81fb,GL_RGB,256,GL_RGB,GL_UNSIGNED_BYTE,pal);
	//	CONPRINT("texture %s (low: %d, high: %d), %d x %d\n", ptex->name,
	//		(ptex->index & 0xFFFF), ((ptex->index>>16) & 0xFFFF),
	//		ptex->width, ptex->height);
	//	CONPRINT("texture %s, flags %d\n", ptex->name, ptex->flags);

		if (ptex->flags & STUDIO_NF_ADDITIVE && !useblending)	// buz
		{
			gl.glEnable(GL_BLEND);
			gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		if (ptex->flags & STUDIO_NF_ALPHATEST && !useblending)	// buz
		{
			gl.glEnable(GL_ALPHA_TEST);
			gl.glAlphaFunc(GL_GREATER, 0.5);
		}

		if (ptexture[pskinref[pmesh->skinref]].flags & STUDIO_NF_CHROME)
		{
			while (i = *(ptricmds++))
			{
				if (i < 0)
				{
					gl.glBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					gl.glBegin( GL_TRIANGLE_STRIP );
				}

				for( ; i > 0; i--, ptricmds += 4)
				{
					// FIX: put these in as integer coords, not floats
					gl.glTexCoord2f(g_chrome[ptricmds[1]][0]*s, g_chrome[ptricmds[1]][1]*t);

					if (strstr(ptex->name, "fullbright"))
					{
						float bright = cv_fullbrighttextures_guess->value;
						if (strstr(ptex->name, "corona_"))
							bright += gEngfuncs.pfnRandomFloat(-0.1, 0.1);
						gl.glColor4f( bright, bright, bright, alpha );
					}
					else
					{
						float *lv = (float*)&g_pvlightvalues[ptricmds[1]];
						gl.glColor4f( lv[0], lv[1], lv[2], alpha );
					}

					float *av = g_pxformverts[ptricmds[0]];
					gl.glVertex3f(av[0], av[1], av[2]);
				}
				gl.glEnd( );
			}
		}
		else
		{
			while (i = *(ptricmds++))
			{
				if (i < 0)
				{
					gl.glBegin( GL_TRIANGLE_FAN );
					i = -i;
				}
				else
				{
					gl.glBegin( GL_TRIANGLE_STRIP );
				}

				for( ; i > 0; i--, ptricmds += 4)
				{
					// Cry of Fear: hide one hand of the view model while the other one holds a second weapon
					if (gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId == 15 && !iGoro_guess)
					{
						if (g_iHeadShieldType_guess == 3 && g_DrawHeadShied)
						{
							if (g_iDualWeapon1_guess == 13)
							{
								if (strstr(pbones[pvertbone[ptricmds[0]]].name, "r_"))
									continue;
							}
							else
							{
								if (strstr(pbones[pvertbone[ptricmds[0]]].name, "l_"))
									continue;
							}
						}

						if (m_pCurrentEntity == gEngfuncs.GetViewModel())
						{
							if (g_iDualWeapon2_guess == 13)
							{
								if (strstr(pbones[pvertbone[ptricmds[0]]].name, "r_"))
									continue;
							}
							else
							{
								if (strstr(pbones[pvertbone[ptricmds[0]]].name, "l_"))
									continue;
							}
						}
					}

					// Cry of Fear: severed limbs (bits in curstate.playerclass)
					int severed = m_pCurrentEntity->curstate.playerclass;
					if (severed > 0)
					{
						if (severed & 0x100)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Leg1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Leg"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Foot"))
								continue;
						}
						else if (severed & 0x80)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Leg1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Foot"))
								continue;
						}

						if (severed & 0x400)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Leg1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Leg"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Foot"))
								continue;
						}
						else if (severed & 0x200)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Leg1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Foot"))
								continue;
						}

						if (severed & 0x10)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Arm1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Arm2"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Hand"))
								continue;
						}
						else if (severed & 8)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Arm2"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Hand"))
								continue;
						}
						else if (severed & 2)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 L Hand"))
								continue;
						}

						if (severed & 0x40)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Arm1"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Arm2"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Hand"))
								continue;
						}
						else if (severed & 0x20)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Arm2"))
								continue;
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Hand"))
								continue;
						}
						else if (severed & 4)
						{
							if (!strcmp(pbones[pvertbone[ptricmds[0]]].name, "Bip01 R Hand"))
								continue;
						}
					}

					// FIX: put these in as integer coords, not floats
					gl.glTexCoord2f(ptricmds[2]*s, ptricmds[3]*t);

					if (cv_fullbrighttextures_guess->value)
					{
						int isViewModel = (m_pCurrentEntity == gEngfuncs.GetViewModel()) ? TRUE : FALSE;

						if (strstr(ptex->name, "fullbright"))
						{
							if (strstr(ptex->name, "fullbright_mess"))
							{
								if (!gHUD.m_bPhoneOff_guess)
								{
									gl.glColor4f( cv_fullbrighttextures_guess->value, cv_fullbrighttextures_guess->value, cv_fullbrighttextures_guess->value, alpha );
								}
								else
								{
									float *lv = (float*)&g_pvlightvalues[ptricmds[1]];
									gl.glColor4f( lv[0], lv[1], lv[2], alpha );
								}
							}
							else
							{
								float bright = cv_fullbrighttextures_guess->value;
								if (strstr(ptex->name, "corona_"))
									bright += gEngfuncs.pfnRandomFloat(-0.1, 0.1);
								gl.glColor4f( bright, bright, bright, alpha );
							}
						}
						else if ((g_DrawHeadShied || isViewModel) && (strstr(ptex->name, "sleeve") || strstr(ptex->name, "glove")))
						{
							// the hands fade in after the view came back
							float scale;
							if (flUnknown105595A0_guess + flUnknown105595A4_guess > gEngfuncs.GetClientTime())
							{
								float frac = (gEngfuncs.GetClientTime() - flUnknown105595A0_guess) / flUnknown105595A4_guess;
								if (frac > 1)
									frac = 1;
								scale = (1 - frac) + 2;
							}
							else
								scale = 1;

							float *lv = (float*)&g_pvlightvalues[ptricmds[1]];
							gl.glColor4f( lv[0] / scale, lv[1] / scale, lv[2] / scale, alpha );
						}
						else
						{
							float *lv = (float*)&g_pvlightvalues[ptricmds[1]];
							gl.glColor4f( lv[0], lv[1], lv[2], alpha );
						}

						float *av = g_pxformverts[ptricmds[0]];
						gl.glVertex3f(av[0], av[1], av[2]);
					}
					else
					{
						float *lv = (float*)&g_pvlightvalues[ptricmds[1]];
						gl.glColor4f( lv[0], lv[1], lv[2], alpha );
					//	gl.glColor4f( 0.5, 0.5, 0.5, alpha );
					//	gl.glColor4f( 1, 1, 1, alpha );

						float *av = g_pxformverts[ptricmds[0]];
						gl.glVertex3f(av[0], av[1], av[2]);
					}
				}
				gl.glEnd( );
			}
		}

		if (ptex->flags & STUDIO_NF_ADDITIVE && !useblending)	// buz
		{
			gl.glDisable(GL_BLEND);
		}

		if (ptex->flags & STUDIO_NF_ALPHATEST && !useblending)	// buz
		{
			gl.glDisable(GL_ALPHA_TEST);
			gl.glAlphaFunc(GL_GREATER, 0); // if leave 0.5, then grass sprites looks strange, i dunno why
		}

		if (bBlurTexture)
			gl.glBindTexture( GL_TEXTURE_2D, g_iOldTexture_guess );
	}
}


//=============================
// EXT_LightPoint
//
// Gets extended lighting info from bump lightmaps.
// Light values should be multiplied by 2, if you want exact color value
//=============================
//int recursioncheck;
int EXT_RecursiveLightPoint (mnode_t *node, const vec3_t &start, const vec3_t &end, lighting_ext *lightinfo);

void EXT_LightPoint(vec3_t pos, lighting_ext *lightinfo, int invlight)
{
	vec3_t		end;
	model_t* world = gEngfuncs.GetEntityByIndex(0)->model;
	memset(lightinfo, 0, sizeof(lighting_ext));
	
	if (!world->lightdata)
	{
		lightinfo->ambientlight[0] = 0.5;
		lightinfo->ambientlight[1] = 0.5;
		lightinfo->ambientlight[2] = 0.5;
		return;
	}

	end[0] = pos[0];
	end[1] = pos[1];

	if (invlight)
		end[2] = pos[2] + 256;
	else
		end[2] = pos[2] - 256;

//	recursioncheck = 0;
	if (!EXT_RecursiveLightPoint (world->nodes, pos, end, lightinfo))
	{
		// Object is too high from ground (for example, helicopter).
		// Use sky color and direction
		cvar_t *sv_skycolor_r = gEngfuncs.pfnGetCvarPointer("sv_skycolor_r");
		cvar_t *sv_skycolor_g = gEngfuncs.pfnGetCvarPointer("sv_skycolor_g");
		cvar_t *sv_skycolor_b = gEngfuncs.pfnGetCvarPointer("sv_skycolor_b");
		lightinfo->ambientlight[0] = lightinfo->addlight[0] = ApplyGamma(sv_skycolor_r->value/255) / 4;
		lightinfo->ambientlight[1] = lightinfo->addlight[1] = ApplyGamma(sv_skycolor_g->value/255) / 4;
		lightinfo->ambientlight[2] = lightinfo->addlight[2] = ApplyGamma(sv_skycolor_b->value/255) / 4;
		lightinfo->lightdir = Vector(0,0,1);
	}
//	CONPRINT("calls: %d\n", recursioncheck);
}


void PARSE_COLOR (vec3_t &out, color24 *lightmap)
{
	out[0] = (float)lightmap->r / 255.0f;
	out[1] = (float)lightmap->g / 255.0f;
	out[2] = (float)lightmap->b / 255.0f;
}


int EXT_RecursiveLightPoint (mnode_t *node, const vec3_t &start, const vec3_t &end, lighting_ext *light)
{
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	color24		*lightmap;

//	recursioncheck++;

	if (node->contents < 0)
		return FALSE;		// didn't hit anything
	
// calculate mid point

// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;
	
	if ( (back < 0) == side)
		return EXT_RecursiveLightPoint (node->children[side], start, end, light);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	int r = EXT_RecursiveLightPoint (node->children[side], start, mid, light);
	if (r) return TRUE;		// hit something
		
	if ((back < 0) == side)
		return FALSE;		// didn't hit anuthing
		
// check for impact on this node
	model_t* world = gEngfuncs.GetEntityByIndex(0)->model;

	surf = world->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWTILED | SURF_DRAWSKY))
			continue;	// no lightmaps

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			continue;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		if (lightmap)
		{
			vec3_t origcolor = Vector(0, 0, 0);
			vec3_t ambientcolor = Vector(0, 0, 0);
			vec3_t diffusecolor = Vector(0, 0, 0);
			vec3_t lightvec;
			float dot;
			int not_normal = 0;

			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;
			
			for (int style = 0; style < MAXLIGHTMAPS && surf->styles[style] != 255; style++)
			{
				if (surf->styles[style] == 0)
				{
					PARSE_COLOR(origcolor, lightmap);
				}
				else if (surf->styles[style] == BUMP_LIGHTVECS_STYLE)
				{
					vec3_t tmp;
					PARSE_COLOR(tmp, lightmap);

					vec3_t vec_x;
					vec3_t vec_y;
					VectorCopy(tex->vecs[0], vec_x);
					VectorCopy(tex->vecs[1], vec_y);
					VectorNormalize(vec_x);
					VectorNormalize(vec_y);

					vec3_t pnormal;
					VectorCopy(surf->plane->normal, pnormal);
					if (surf->flags & SURF_PLANEBACK)
					{
						VectorInverse(pnormal);
					}

					dot = tmp[2]*2 - 1;

					VectorClear(lightvec);
					VectorMA(lightvec, tmp[0]*2 - 1, vec_x, lightvec);
					VectorMA(lightvec, tmp[1]*2 - 1, vec_y, lightvec);
					VectorMA(lightvec, tmp[2]*2 - 1, pnormal, lightvec);
				}
				else if (surf->styles[style] == BUMP_ADDLIGHT_STYLE)
				{
					PARSE_COLOR(diffusecolor, lightmap);
				}
				else if (surf->styles[style] == BUMP_BASELIGHT_STYLE)
				{
					PARSE_COLOR(ambientcolor, lightmap);
					not_normal = 1;
				}

				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}

			// apply gamma and write to ext_lightinfo struct
		//	if (diffusecolor[0] || diffusecolor[1] || diffusecolor[2] ||
		//		ambientcolor[0] || ambientcolor[1] || ambientcolor[2])
			if (not_normal)
			{
				vec3_t temp;
				vec3_t scale = Vector(0,0,0);
				VectorScale(diffusecolor, dot, temp);
				VectorAdd(temp, ambientcolor, temp);
				VectorScale(temp, 2, temp);

				if (temp[0]) scale[0] = ApplyGamma(temp[0]) / temp[0];
				if (temp[1]) scale[1] = ApplyGamma(temp[1]) / temp[1];
				if (temp[2]) scale[2] = ApplyGamma(temp[2]) / temp[2];

				light->addlight[0] = diffusecolor[0] * scale[0];
				light->addlight[1] = diffusecolor[1] * scale[1];
				light->addlight[2] = diffusecolor[2] * scale[2];

				light->ambientlight[0] = ambientcolor[0] * scale[0];
				light->ambientlight[1] = ambientcolor[1] * scale[1];
				light->ambientlight[2] = ambientcolor[2] * scale[2];

				VectorCopy(lightvec, light->lightdir);
			}
			else
			{
				light->ambientlight[0] = ApplyGamma(origcolor[0])/2;
				light->ambientlight[1] = ApplyGamma(origcolor[1])/2;
				light->ambientlight[2] = ApplyGamma(origcolor[2])/2;
			}
		}		
		return TRUE;
	}

// go down back side
	return EXT_RecursiveLightPoint (node->children[!side], mid, end, light);
}