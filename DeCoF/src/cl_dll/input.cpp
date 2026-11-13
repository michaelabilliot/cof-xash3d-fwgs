//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// cl.input.c  -- builds an intended movement command to send to the server

//xxxxxx Move bob and pitch drifting code here and other stuff from view if needed

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.
#include <fstream>

#include "hud.h"
#include "cofr_inventory.h"
#include "cl_util.h"
#include "camera.h"
extern "C"
{
#include "kbutton.h"
}
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "camera.h"
#include "in_defs.h"
#include "view.h"
#include <string.h>
#include <ctype.h>

#include "vgui_TeamFortressViewport.h"
#include "Credits_guess.h"
#include "HUDControl_guess.h"
#include "RECBInventory_guess.h"
#include "HUDControl_guess.h"
#include "windows.h"

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

extern "C" __declspec( dllimport ) bool __cdecl SteamAPI_Init();
extern "C" __declspec( dllimport ) ISteamUser * __cdecl SteamUser();
//=========================================================


extern "C" 
{
	struct kbutton_s DLLEXPORT *KB_Find( const char *name );
	void DLLEXPORT CL_CreateMove ( float frametime, struct usercmd_s *cmd, int active );
	void DLLEXPORT HUD_Shutdown( void );
	int DLLEXPORT HUD_Key_Event( int eventcode, int keynum, const char *pszCurrentBinding );
}

extern int g_iAlive;

extern int g_weaponselect;
extern cl_enginefunc_t gEngfuncs;

// Defined in pm_math.c
extern "C" float anglemod( float a );

void IN_Init (void);
void IN_Move ( float frametime, usercmd_t *cmd);
void IN_Shutdown( void );
void V_Init( void );
void VectorAngles( const float *forward, float *angles );
int CL_ButtonBits( int );
void IrrKlangDestroy_guess( void );
void ParseHintTokens( char *pszName, char *pszFile );
int IsInDonatorGroup1_guess( void );	// Steam clan 0x29D6CA
int IsInDonatorGroup2_guess( void );	// Steam clan 0x46AE0A
void GetCurrentPath( char *path );

extern cl_entity_t *pTelescopeEnt_guess;
extern int iWheelchair_guess;
extern int iBoat_guess;
extern cl_entity_t *pAttachCamEnt_guess;
extern int iPlayerMode_guess;
extern int iGetUpTaps_guess;
extern int iGetUpTapCount_guess;
extern bool g_bUnlockShown_guess;
extern int g_iVisibleMouse;
extern int iMenuMode_guess;
extern bool bCanDodge_guess;
extern bool bXboxFix_guess;
extern vec3_t vecIronsightOfs_guess;
extern vec3_t vecWheelchairViewAngles_guess;
extern vec3_t vecWheelchairOrigin_guess;

// xxx need client dll function to get and clear impuse
extern cvar_t *in_joystick;

int	iDonator_guess;
bool	bIronsightEdit_guess;
bool	bSteamInit_guess;
int	in_impulse	= 0;
int	in_cancel	= 0;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookstrafe;
cvar_t	*lookspring;
cvar_t	*cl_pitchup;
cvar_t	*cl_pitchdown;
cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_backspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_movespeedkey;
cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_anglespeedkey;
cvar_t	*cl_vsmoothing;
cvar_t	*cg_water;
cvar_t	*r_weather;
cvar_t	*cl_ripples;
cvar_t	*fps_current;
cvar_t	*cl_noiseeffect;
cvar_t	*cl_noiseamount;
cvar_t	*cl_subtitles;
cvar_t	*cl_vignette;
cvar_t	*cl_fovmultiplier;
cvar_t	*cl_lazyweapon_force;
cvar_t	*cl_weapon_idle_dampener;
cvar_t	*cl_weapon_idle_speed;
cvar_t	*xbox_controller_sensitivityx;
cvar_t	*xbox_controller_sensitivityy;
cvar_t	*cl_smokeparticlelimit;
cvar_t	*cl_cheapweaponsmoke;
cvar_t	*cl_fastcredits;
cvar_t	*cl_waterspeed_x;
cvar_t	*cl_waterspeed_y;
cvar_t	*cl_coop_language;
cvar_t	*cl_lowflashlight;
cvar_t	*cl_propdistance;
cvar_t	*cl_nodoubletapdodge;
cvar_t	*cl_downwiththesickness;
cvar_t	*cl_bloodeffects;
cvar_t	*cl_crosshair;
cvar_t	*cl_cameraeffect;
/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t	in_mlook;
kbutton_t	in_klook;
kbutton_t	in_jlook;
kbutton_t	in_forward;
kbutton_t	in_back;
kbutton_t	in_lookup;
kbutton_t	in_lookdown;
kbutton_t	in_moveleft;
kbutton_t	in_moveright;
kbutton_t	in_strafe;
kbutton_t	in_speed;
kbutton_t	in_use;
kbutton_t	in_jump;
kbutton_t	in_attack;
kbutton_t	in_attack2;
kbutton_t	in_up;
kbutton_t	in_down;
kbutton_t	in_duck;
kbutton_t	in_reload;
kbutton_t	in_alt1;
kbutton_t	in_score;
kbutton_t	in_break;
kbutton_t	in_graph;  // Display the netgraph
kbutton_t	in_attack3;
kbutton_t	in_inventory;
kbutton_t	in_dodge;
kbutton_t	in_objectives;

typedef struct kblist_s
{
	struct kblist_s *next;
	kbutton_t *pkey;
	char name[32];
} kblist_t;

kblist_t *g_kbkeys = NULL;

/*
============
KB_ConvertString

Removes references to +use and replaces them with the keyname in the output string.  If
 a binding is unfound, then the original text is retained.
NOTE:  Only works for text with +word in it.
============
*/
int KB_ConvertString( char *in, char **ppout )
{
	char sz[ 4096 ];
	char binding[ 64 ];
	char *p;
	char *pOut;
	char *pEnd;
	const char *pBinding;

	if ( !ppout )
		return 0;

	*ppout = NULL;
	p = in;
	pOut = sz;
	while ( *p )
	{
		if ( *p == '+' )
		{
			pEnd = binding;
			while ( *p && ( isalnum( *p ) || ( pEnd == binding ) ) && ( ( pEnd - binding ) < 63 ) )
			{
				*pEnd++ = *p++;
			}

			*pEnd =  '\0';

			pBinding = NULL;
			if ( strlen( binding + 1 ) > 0 )
			{
				// See if there is a binding for binding?
				pBinding = gEngfuncs.Key_LookupBinding( binding + 1 );
			}

			if ( pBinding )
			{
				*pOut++ = '[';
				pEnd = (char *)pBinding;
			}
			else
			{
				pEnd = binding;
			}

			while ( *pEnd )
			{
				*pOut++ = *pEnd++;
			}

			if ( pBinding )
			{
				*pOut++ = ']';
			}
		}
		else
		{
			*pOut++ = *p++;
		}
	}

	*pOut = '\0';

	pOut = ( char * )malloc( strlen( sz ) + 1 );
	strcpy( pOut, sz );
	*ppout = pOut;

	return 1;
}

/*
============
KB_Find

Allows the engine to get a kbutton_t directly ( so it can check +mlook state, etc ) for saving out to .cfg files
============
*/
struct kbutton_s DLLEXPORT *KB_Find( const char *name )
{
	kblist_t *p;
	p = g_kbkeys;
	while ( p )
	{
		if ( !stricmp( name, p->name ) )
			return p->pkey;

		p = p->next;
	}
	return NULL;
}

/*
============
KB_Add

Add a kbutton_t * to the list of pointers the engine can retrieve via KB_Find
============
*/
void KB_Add( const char *name, kbutton_t *pkb )
{
	kblist_t *p;	
	kbutton_t *kb;

	kb = KB_Find( name );
	
	if ( kb )
		return;

	p = ( kblist_t * )malloc( sizeof( kblist_t ) );
	memset( p, 0, sizeof( *p ) );

	strcpy( p->name, name );
	p->pkey = pkb;

	p->next = g_kbkeys;
	g_kbkeys = p;
}

/*
============
KB_Init

Add kbutton_t definitions that the engine can query if needed
============
*/
void KB_Init( void )
{
	g_kbkeys = NULL;

	KB_Add( "in_graph", &in_graph );
	KB_Add( "in_mlook", &in_mlook );
	KB_Add( "in_jlook", &in_jlook );
}

/*
============
KB_Shutdown

Clear kblist
============
*/
void KB_Shutdown( void )
{
	kblist_t *p, *n;
	p = g_kbkeys;
	while ( p )
	{
		n = p->next;
		free( p );
		p = n;
	}
	g_kbkeys = NULL;
}

/*
============
KeyDown
============
*/
void KeyDown (kbutton_t *b)
{
	int		k;
	char	*c;

	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key
	
	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		gEngfuncs.Con_DPrintf ("Three keys down for a button '%c' '%c' '%c'!\n", b->down[0], b->down[1], c);
		return;
	}
	
	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

/*
============
KeyUp
============
*/
void KeyUp (kbutton_t *b)
{
	int		k;
	char	*c;
	
	c = gEngfuncs.Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
	{
		//Con_Printf ("Keys down for button: '%c' '%c' '%c' (%d,%d,%d)!\n", b->down[0], b->down[1], c, b->down[0], b->down[1], c);
		return;		// some other key is still holding it down
	}

	if (!(b->state & 1))
		return;		// still up (this should not happen)

	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

/*
============
HUD_Key_Event

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int DLLEXPORT HUD_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	if(down&&(keynum==13||keynum==169)&&gEngfuncs.GetMaxClients()>1&&gViewPort&&gViewPort->m_pHUDControl_guess){
 auto hud=gViewPort->m_pHUDControl_guess;if(hud->m_pIntroduction_guess&&hud->m_pIntroduction_guess->isVisible()&&hud->m_bIntroductionFadeIn_guess){gEngfuncs.pfnServerCmd("cof8_skipintro\n");return 0;}}
 if(CoFR_ClientKey(down,keynum)) return 0;
	if (gViewPort)
		return gViewPort->KeyInput(down, keynum, pszCurrentBinding);
	
	return 1;
}

void IN_BreakDown( void ) { KeyDown( &in_break );};
void IN_BreakUp( void ) { KeyUp( &in_break ); };
void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_JLookDown (void) {KeyDown(&in_jlook);}
void IN_JLookUp (void) {KeyUp(&in_jlook);}
void IN_MLookDown (void) {KeyDown(&in_mlook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}

void IN_ForwardDown(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	if ( iPlayerMode_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[1] += 0.25;
		return;
	}

	KeyDown(&in_forward);
	gHUD.m_Spectator.HandleButtonsDown( IN_FORWARD );

	if ( gHUD.m_flForwardTapTime_guess > gEngfuncs.GetClientTime() && cl_nodoubletapdodge->value == 0 && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeforward" );

	if ( ( in_dodge.state & 1 ) && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeforward" );

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "forwardpressed" );

	gHUD.m_flForwardTapTime_guess = gEngfuncs.GetClientTime() + 0.18;
}

void IN_ForwardUp(void)
{
	KeyUp(&in_forward);
	gHUD.m_Spectator.HandleButtonsUp( IN_FORWARD );

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "forwardreleased" );
}

void IN_BackDown(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	if ( iPlayerMode_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[1] -= 0.25;
		return;
	}

	KeyDown(&in_back);
	gHUD.m_Spectator.HandleButtonsDown( IN_BACK );

	if ( gHUD.m_flBackTapTime_guess > gEngfuncs.GetClientTime() && cl_nodoubletapdodge->value == 0 && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgebackward" );

	if ( ( in_dodge.state & 1 ) && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgebackward" );

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "backpressed" );

	gHUD.m_flBackTapTime_guess = gEngfuncs.GetClientTime() + 0.18;
}

void IN_BackUp(void)
{
	KeyUp(&in_back);
	gHUD.m_Spectator.HandleButtonsUp( IN_BACK );

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "backreleased" );
}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	if ( iPlayerMode_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[0] -= 0.25;
		return;
	}

	KeyDown(&in_moveleft);
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVELEFT );

	if ( gHUD.m_flLeftTapTime_guess > gEngfuncs.GetClientTime() && cl_nodoubletapdodge->value == 0 && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeleft" );

	if ( ( in_dodge.state & 1 ) && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeleft" );

	gHUD.m_flLeftTapTime_guess = gEngfuncs.GetClientTime() + 0.18;
}

void IN_MoveleftUp(void)
{
	KeyUp(&in_moveleft);
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVELEFT );
}

void IN_MoverightDown(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[0] += 0.25;
		return;
	}

	if ( iPlayerMode_guess )
		return;

	KeyDown(&in_moveright);
	gHUD.m_Spectator.HandleButtonsDown( IN_MOVERIGHT );

	if ( gHUD.m_flRightTapTime_guess > gEngfuncs.GetClientTime() && cl_nodoubletapdodge->value == 0 && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeright" );

	if ( ( in_dodge.state & 1 ) && bCanDodge_guess )
		gEngfuncs.pfnServerCmd( "dodgeright" );

	gHUD.m_flRightTapTime_guess = gEngfuncs.GetClientTime() + 0.18;
}

void IN_MoverightUp(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	KeyUp(&in_moveright);
	gHUD.m_Spectator.HandleButtonsUp( IN_MOVERIGHT );
}

void IN_SpeedDown(void)
{
	if ( strcmp( gEngfuncs.pfnGetLevelName(), "maps/c_nightmare.bsp" ) )
		KeyDown(&in_speed);
}

void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

// needs capture by hud/vgui also
extern void __CmdFunc_InputPlayerSpecial(void);

void IN_Attack2Down(void) 
{
	KeyDown(&in_attack2);

	if ( gViewPort && gViewPort->m_pCredits_guess && gViewPort->m_pCredits_guess->m_bShowStats_guess && !gViewPort->m_pCredits_guess->m_bTextOnly_guess )
	{
		g_bUnlockShown_guess = false;
		gViewPort->m_pCredits_guess->m_bContinue_guess = true;
		gViewPort->m_pCredits_guess->m_szUnlockText_guess[0] = 0;
		gViewPort->m_pCredits_guess->m_pUnlockLabel_guess->setFgColor( 255, 255, 255, 255 );
	}

	if ( bIronsightEdit_guess )
	{
		gEngfuncs.pfnServerCmd( "ironsightzoomout" );
		return;
	}

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "attack2pressed" );

	if ( iPlayerMode_guess <= 0 || iPlayerMode_guess > 5 )
	{
		if ( pTelescopeEnt_guess )
			gEngfuncs.pfnServerCmd( "telescopecancel" );

		gHUD.m_Spectator.HandleButtonsDown( IN_ATTACK2 );
	}
}

void IN_Attack2Up(void)
{
	KeyUp(&in_attack2);

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "attack2released" );
}

void IN_UseDown (void)
{
	if ( gViewPort && gViewPort->m_pCredits_guess && gViewPort->m_pCredits_guess->m_bShowStats_guess && !gViewPort->m_pCredits_guess->m_bTextOnly_guess )
	{
		g_bUnlockShown_guess = false;
		gViewPort->m_pCredits_guess->m_bContinue_guess = true;
		gViewPort->m_pCredits_guess->m_szUnlockText_guess[0] = 0;
		gViewPort->m_pCredits_guess->m_pUnlockLabel_guess->setFgColor( 255, 255, 255, 255 );
	}

	if ( gHUD.m_iFallOver_guess )
		return;

	if ( iPlayerMode_guess )
		return;

	if ( iBoat_guess )
		gEngfuncs.pfnServerCmd( "boatdisembark" );

	if ( pTelescopeEnt_guess )
		gEngfuncs.pfnServerCmd( "telescopecancel" );

	KeyDown(&in_use);
	gHUD.m_Spectator.HandleButtonsDown( IN_USE );
}
void IN_UseUp (void) {KeyUp(&in_use);}
void IN_JumpDown (void)
{
	if ( gHUD.m_iFallOver_guess )
	{
		if ( ++iGetUpTapCount_guess >= iGetUpTaps_guess )
			gEngfuncs.pfnServerCmd( "tapgetup" );
		return;
	}

	if ( iPlayerMode_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[2] += 0.25;
		return;
	}

	KeyDown(&in_jump);
	gHUD.m_Spectator.HandleButtonsDown( IN_JUMP );
}
void IN_JumpUp (void) {KeyUp(&in_jump);}
void IN_DuckDown(void)
{
	if ( gHUD.m_iFallOver_guess )
		return;

	if ( iPlayerMode_guess )
		return;

	if ( bIronsightEdit_guess )
	{
		vecIronsightOfs_guess[2] -= 0.25;
		return;
	}

	KeyDown(&in_duck);
	gHUD.m_Spectator.HandleButtonsDown( IN_DUCK );
}
void IN_DuckUp(void) {KeyUp(&in_duck);}
void IN_ReloadDown(void)
{
	if ( iWheelchair_guess )
		gEngfuncs.pfnServerCmd( "wheelchairreload" );

	KeyDown(&in_reload);
}
void IN_ReloadUp(void) {KeyUp(&in_reload);}
void IN_Alt1Down(void) {KeyDown(&in_alt1);}
void IN_Alt1Up(void) {KeyUp(&in_alt1);}
void IN_GraphDown(void) {KeyDown(&in_graph);}
void IN_GraphUp(void) {KeyUp(&in_graph);}

void IN_AttackDown(void)
{
	if ( bIronsightEdit_guess )
	{
		gEngfuncs.pfnServerCmd( "ironsightzoomin" );
		return;
	}

	if ( gViewPort )
	{
		if ( gViewPort->m_pHUDControl_guess && gViewPort->m_pHUDControl_guess->m_bLobby_guess )
		{
			gEngfuncs.pfnServerCmd( "readylobby" );
			return;
		}

		if ( gViewPort->m_pCredits_guess && gViewPort->m_pCredits_guess->m_bShowStats_guess && !gViewPort->m_pCredits_guess->m_bTextOnly_guess )
		{
			g_bUnlockShown_guess = false;
			gViewPort->m_pCredits_guess->m_bContinue_guess = true;
			gViewPort->m_pCredits_guess->m_szUnlockText_guess[0] = 0;
			gViewPort->m_pCredits_guess->m_pUnlockLabel_guess->setFgColor( 255, 255, 255, 255 );
		}
	}

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "attack1pressed" );

	if ( iPlayerMode_guess <= 0 || iPlayerMode_guess > 5 )
	{
		if ( iWheelchair_guess )
		{
			char szEndVec[64];
			char szFire[64];
			vec3_t forward, right;
			int x, y, z;

			x = vecWheelchairOrigin_guess[0];
			y = vecWheelchairOrigin_guess[1];
			z = vecWheelchairOrigin_guess[2];

			gEngfuncs.pfnAngleVectors( vecWheelchairViewAngles_guess, forward, right, NULL );

			sprintf( szEndVec, "wheelchairendvec %i %i %i", (int)( vecWheelchairOrigin_guess[0] + forward[0] * 4096 ), (int)( vecWheelchairOrigin_guess[1] + forward[1] * 4096 ), (int)( vecWheelchairOrigin_guess[2] + forward[2] * 4096 ) );
			gEngfuncs.pfnServerCmd( szEndVec );

			sprintf( szFire, "wheelchairfire1 %i %i %i", x, y, z );
			gEngfuncs.pfnServerCmd( szFire );
		}

		if ( pTelescopeEnt_guess )
			gEngfuncs.pfnServerCmd( "telescopecancel" );

		KeyDown( &in_attack );
		gHUD.m_Spectator.HandleButtonsDown( IN_ATTACK );
	}
}

void IN_AttackUp(void)
{
	KeyUp( &in_attack );
	in_cancel = 0;

	if ( pAttachCamEnt_guess )
		gEngfuncs.pfnServerCmd( "attack1released" );
}

void IN_Attack3Down(void)
{
	if ( gViewPort && gViewPort->m_pCredits_guess && gViewPort->m_pCredits_guess->m_bShowStats_guess && !gViewPort->m_pCredits_guess->m_bTextOnly_guess )
	{
		g_bUnlockShown_guess = false;
		gViewPort->m_pCredits_guess->m_bContinue_guess = true;
		gViewPort->m_pCredits_guess->m_szUnlockText_guess[0] = 0;
		gViewPort->m_pCredits_guess->m_pUnlockLabel_guess->setFgColor( 255, 255, 255, 255 );
	}

	if ( !iPlayerMode_guess )
		KeyDown( &in_attack3 );
}

void IN_Attack3Up(void) {KeyUp(&in_attack3);}
void IN_DodgeDown(void) {KeyDown(&in_dodge);}
void IN_DodgeUp(void) {KeyUp(&in_dodge);}

void IN_InventoryDown(void)
{
	KeyDown( &in_inventory );

	if ( iPlayerMode_guess > 4 )
		return;

	if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/c_nightmare.bsp" ) )
		return;

	if ( iWheelchair_guess )
		return;

	if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
		return;

	if ( gHUD.m_iLadder_guess )
		return;

	if ( gHUD.m_iFallOver_guess )
		return;

	if ( IsViewEntityActive_guess() )
		return;

	if ( !gViewPort || !gViewPort->m_pRECBInventory_guess )
		return;

	if ( gViewPort->m_pRECBInventory_guess->isVisible() )
	{
		gViewPort->HideTopMenu();
		PlaySound( "inventory/inventory_close.wav", 1 );
		return;
	}

	if ( g_iVisibleMouse )
		return;

	gViewPort->ShowVGUIMenu( 30 );
	PlaySound( "inventory/inventory_open.wav", 1 );
}

void IN_InventoryUp(void) {KeyUp(&in_inventory);}

void IN_ObjectivesDown(void)
{
	char szText[256];

	KeyDown( &in_objectives );

	if ( iPlayerMode_guess > 4 )
		return;

	if ( !strcmp( gEngfuncs.pfnGetLevelName(), "maps/c_nightmare.bsp" ) )
		return;

	if ( strstr( gEngfuncs.pfnGetLevelName(), "game_menu" ) )
		return;

	if ( IsViewEntityActive_guess() )
		return;

	if ( strlen( gHUD.m_szObjectiveTitle_guess ) > 2 && strlen( gHUD.m_szObjectiveText_guess ) > 2 )
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
}

void IN_ObjectivesUp(void) {KeyUp(&in_objectives);}

void IN_Documents(void)
{
}

void IN_PadFix_guess(void)
{
	bXboxFix_guess = false;
	gHUD.m_bXboxFix_guess[0] = true;
	gHUD.m_bXboxFix_guess[1] = true;
	gHUD.m_bXboxFix_guess[2] = true;
	gHUD.m_bXboxFix_guess[3] = true;
	gHUD.m_bXboxFix_guess[4] = true;
	gHUD.m_bXboxFix_guess[5] = true;
	gHUD.m_bXboxFix_guess[6] = true;
	gHUD.m_bXboxFix_guess[7] = true;
	gHUD.m_bXboxFix_guess[8] = true;
	gHUD.m_bXboxFix_guess[9] = true;
	gHUD.m_bXboxFix_guess[10] = true;
	gHUD.m_bXboxFix_guess[11] = true;
	gHUD.m_bXboxFix_guess[14] = true;
	gHUD.m_bXboxFix_guess[15] = true;
	gHUD.m_bXboxFix_guess[16] = true;
	gHUD.m_bXboxFix_guess[17] = true;
	gHUD.m_bXboxFix_guess[18] = true;
	gHUD.m_bXboxFix_guess[19] = true;
	gHUD.m_bXboxFix_guess[20] = true;
	gHUD.m_bXboxFix_guess[21] = true;
	gHUD.m_bXboxFix_guess[22] = true;
	gHUD.m_bXboxFix_guess[23] = true;
	gHUD.m_bXboxFix_guess[24] = true;
	gHUD.m_bXboxFix_guess[25] = true;
	gHUD.m_bXboxFix_guess[12] = true;
	gHUD.m_bXboxFix_guess[13] = true;
}

// Special handling
void IN_Cancel(void)
{
	in_cancel = 1;
}

void IN_Impulse (void)
{
	in_impulse = atoi( gEngfuncs.Cmd_Argv(1) );
}

void IN_ScoreDown(void)
{
	KeyDown(&in_score);
	if ( gViewPort )
	{
		gViewPort->ShowScoreBoard();
	}
}

void IN_ScoreUp(void)
{
	KeyUp(&in_score);
	if ( gViewPort )
	{
		gViewPort->HideScoreBoard();
	}
}

void IN_MLookUp (void)
{
	KeyUp( &in_mlook );
	if ( !( in_mlook.state & 1 ) && lookspring->value )
	{
		V_StartPitchDrift();
	}
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val = 0.0;
	int			impulsedown, impulseup, down;
	
	impulsedown = key->state & 2;
	impulseup	= key->state & 4;
	down		= key->state & 1;
	
	if ( impulsedown && !impulseup )
	{
		// pressed and held this frame?
		val = down ? 0.5 : 0.0;
	}

	if ( impulseup && !impulsedown )
	{
		// released this frame?
		val = down ? 0.0 : 0.0;
	}

	if ( !impulsedown && !impulseup )
	{
		// held the entire frame?
		val = down ? 1.0 : 0.0;
	}

	if ( impulsedown && impulseup )
	{
		if ( down )
		{
			// released and re-pressed this frame
			val = 0.75;	
		}
		else
		{
			// pressed and released this frame
			val = 0.25;	
		}
	}

	// clear impulses
	key->state &= 1;		
	return val;
}

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles ( float frametime, float *viewangles )
{
	float	speed;
	float	up, down;
	
	if (in_speed.state & 1)
	{
		speed = frametime * cl_anglespeedkey->value;
	}
	else
	{
		speed = frametime;
	}

	if (in_klook.state & 1)
	{
		V_StopPitchDrift ();
		viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_forward);
		viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_back);
	}
	
	down = CL_KeyState(&in_lookdown);
	up = CL_KeyState (&in_lookup);

	if (up || down)
		V_StopPitchDrift ();
		
	if (viewangles[PITCH] > cl_pitchdown->value)
		viewangles[PITCH] = cl_pitchdown->value;
	if (viewangles[PITCH] < -cl_pitchup->value)
		viewangles[PITCH] = -cl_pitchup->value;

	if (viewangles[ROLL] > 50)
		viewangles[ROLL] = 50;
	if (viewangles[ROLL] < -50)
		viewangles[ROLL] = -50;
}

/*
================
CL_CreateMove

Send the intended movement message to the server
if active == 1 then we are 1) not playing back demos ( where our commands are ignored ) and
2 ) we have finished signing on to server
================
*/
void DLLEXPORT CL_CreateMove ( float frametime, struct usercmd_s *cmd, int active )
{	
	static bool inventoryCaptured=false;
 bool inventoryOpen=gViewPort&&gViewPort->m_pRECBInventory_guess&&gViewPort->m_pRECBInventory_guess->isVisible();
 if(inventoryOpen!=inventoryCaptured){inventoryCaptured=inventoryOpen;
  for(kbutton_t* button:{&in_attack,&in_attack2,&in_attack3,&in_reload}){button->down[0]=button->down[1]=0;button->state=4;}
 }
 vec3_t viewangles;
	static vec3_t oldangles;

	if ( active )
	{
		//memset( viewangles, 0, sizeof( vec3_t ) );
		//viewangles[ 0 ] = viewangles[ 1 ] = viewangles[ 2 ] = 0.0;
		gEngfuncs.GetViewAngles( (float *)viewangles );

		CL_AdjustAngles ( frametime, viewangles );

		memset (cmd, 0, sizeof(*cmd));
		
		if ( !gHUD.m_bViewLock_guess )
			gEngfuncs.SetViewAngles( (float *)viewangles );

		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

		cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
		cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

		if ( !(in_klook.state & 1 ) )
		{	
			cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
			cmd->forwardmove -= cl_backspeed->value * CL_KeyState (&in_back);
		}	

		if ( ( in_dodge.state & 1 ) && bCanDodge_guess )
		{
			if ( in_forward.state & 1 )
				gEngfuncs.pfnServerCmd( "dodgeforward" );
			else if ( in_back.state & 1 )
				gEngfuncs.pfnServerCmd( "dodgebackward" );
			else if ( in_moveright.state & 1 )
				gEngfuncs.pfnServerCmd( "dodgeright" );
			else if ( in_moveleft.state & 1 )
				gEngfuncs.pfnServerCmd( "dodgeleft" );
		}

		// Allow mice and other controllers to add their inputs
		IN_Move ( frametime, cmd );
	}

	cmd->impulse = in_impulse;
	in_impulse = 0;

	cmd->weaponselect = g_weaponselect;
	g_weaponselect = 0;
	//
	// set button and flag bits
	//
	cmd->buttons = CL_ButtonBits( 1 );

	// If they're in a modal dialog, ignore the attack button.
	if(GetClientVoiceMgr()->IsInSquelchMode())
		cmd->buttons &= ~IN_ATTACK;

	// Using joystick?
	if ( in_joystick->value )
	{
		if ( cmd->forwardmove > 0 )
		{
			cmd->buttons |= IN_FORWARD;
		}
		else if ( cmd->forwardmove < 0 )
		{
			cmd->buttons |= IN_BACK;
		}
	}

	gEngfuncs.GetViewAngles( (float *)viewangles );
	// Set current view angles.
	
// buz: in spec tank mode check for bounds
//	gEngfuncs.Con_Printf("pitch: %f, yaw: %f\n", viewangles[PITCH], viewangles[YAW]);
	if (gHUD.m_SpecTank_on)
	{
	// check yaw
		float ofs = viewangles[YAW] - gHUD.m_SpecTank_defYaw;
		if (ofs > 180) ofs -= 360;
		else if (ofs < -180) ofs += 360;

		if (ofs < -gHUD.m_SpecTank_coneHor)
			viewangles[YAW] = gHUD.m_SpecTank_defYaw - gHUD.m_SpecTank_coneHor;
		else if (ofs > gHUD.m_SpecTank_coneHor)
			viewangles[YAW] = gHUD.m_SpecTank_defYaw + gHUD.m_SpecTank_coneHor;
	
	// check pitch
		if (viewangles[PITCH] > gHUD.m_SpecTank_coneVer)
			viewangles[PITCH] = gHUD.m_SpecTank_coneVer;
		else if (viewangles[PITCH] < -gHUD.m_SpecTank_coneVer)
			viewangles[PITCH] = -gHUD.m_SpecTank_coneVer;

		// всем спасибо, все свободны
		if ( !gHUD.m_bViewLock_guess )
			gEngfuncs.SetViewAngles( (float *)viewangles );
	}

	if ( iWheelchair_guess )
	{
		cmd->impact_position = vecWheelchairViewAngles_guess;
		cmd->impact_position[2] = 1;
	}
	else
	{
		cmd->impact_position[2] = 0;
	}
	
	if ( g_iAlive )
	{
		if ( iWheelchair_guess )
		{
			VectorCopy( vecWheelchairViewAngles_guess, cmd->viewangles );
			oldangles = vecWheelchairViewAngles_guess;
		}
		else
		{
			VectorCopy( viewangles, cmd->viewangles );
			oldangles = viewangles;
		}
	}
	else
	{
		if ( iWheelchair_guess )
		{
			VectorCopy( vecWheelchairViewAngles_guess, cmd->viewangles );
		}
		else
		{
			VectorCopy( oldangles, cmd->viewangles );
		}
	}

	if ( GetClientVoiceMgr()->m_bTalking )
	{
		if ( cmd->viewangles[ROLL] != 1 )
			cmd->viewangles[ROLL] = 1;
	}
	else if ( cmd->viewangles[ROLL] != 0 )
	{
		cmd->viewangles[ROLL] = 0;
	}

	cmd->is_donator = iDonator_guess;
}

/*
============
CL_IsDead

Returns 1 if health is <= 0
============
*/
int	CL_IsDead( void )
{
	return ( gHUD.m_Health.m_iHealth <= 0 ) ? 1 : 0;
}

/*
============
CL_ButtonBits

Returns appropriate button info for keyboard and mouse state
Set bResetState to 1 to clear old state info
============
*/
int CL_ButtonBits( int bResetState )
{
	int bits = 0;

	if ( in_attack.state & 3 )
	{
		bits |= IN_ATTACK;
	}
	
	if (in_duck.state & 3)
	{
		bits |= IN_DUCK;
	}
 
	if (in_jump.state & 3)
	{
		bits |= IN_JUMP;
	}

	if ( in_forward.state & 3 )
	{
		bits |= IN_FORWARD;
	}
	
	if (in_back.state & 3)
	{
		bits |= IN_BACK;
	}

	if (in_use.state & 3)
	{
		bits |= IN_USE;
	}

	if (in_attack3.state & 3)
	{
		bits |= IN_CANCEL;
	}

	if ( in_dodge.state & 3 )
	{
		bits |= IN_LEFT;
	}
	
	if ( in_moveleft.state & 3 )
	{
		bits |= IN_MOVELEFT;
	}
	
	if (in_moveright.state & 3)
	{
		bits |= IN_MOVERIGHT;
	}

	if (in_attack2.state & 3)
	{
		bits |= IN_ATTACK2;
	}

	if (in_reload.state & 3)
	{
		bits |= IN_RELOAD;
	}

	if (in_alt1.state & 3)
	{
		bits |= IN_ALT1;
	}

	if (in_speed.state & 3)
	{
		bits |= IN_RUN;
	}

	if ( in_score.state & 3 )
	{
		bits |= IN_SCORE;
	}

	if ( gEngfuncs.pfnGetLevelName() && strstr( gEngfuncs.pfnGetLevelName(), "cof_" ) )
	{
		bits |= IN_SCORE;
	}

	// Dead or in intermission? Shore scoreboard, too
	if ( CL_IsDead() || gHUD.m_iIntermission )
	{
		bits |= IN_SCORE;
	}

	if ( bResetState )
	{
		in_attack.state &= ~2;
		in_duck.state &= ~2;
		in_jump.state &= ~2;
		in_forward.state &= ~2;
		in_back.state &= ~2;
		in_use.state &= ~2;
		in_moveleft.state &= ~2;
		in_moveright.state &= ~2;
		in_attack2.state &= ~2;
		in_attack3.state &= ~2;
		in_reload.state &= ~2;
		in_alt1.state &= ~2;
		in_score.state &= ~2;
		in_inventory.state &= ~2;
		in_speed.state &= ~2;
	}

	return bits;
}

/*
============
CL_ResetButtonBits

============
*/
void CL_ResetButtonBits( int bits )
{
	int bitsNew = CL_ButtonBits( 0 ) ^ bits;
}

/*
============
InitInput
============
*/
void InitInput (void)
{
	gEngfuncs.pfnAddCommand ("+moveup",IN_UpDown);
	gEngfuncs.pfnAddCommand ("-moveup",IN_UpUp);
	gEngfuncs.pfnAddCommand ("+movedown",IN_DownDown);
	gEngfuncs.pfnAddCommand ("-movedown",IN_DownUp);
	gEngfuncs.pfnAddCommand ("+forward",IN_ForwardDown);
	gEngfuncs.pfnAddCommand ("-forward",IN_ForwardUp);
	gEngfuncs.pfnAddCommand ("+back",IN_BackDown);
	gEngfuncs.pfnAddCommand ("-back",IN_BackUp);
	gEngfuncs.pfnAddCommand ("+lookup", IN_LookupDown);
	gEngfuncs.pfnAddCommand ("-lookup", IN_LookupUp);
	gEngfuncs.pfnAddCommand ("+lookdown", IN_LookdownDown);
	gEngfuncs.pfnAddCommand ("-lookdown", IN_LookdownUp);
	gEngfuncs.pfnAddCommand ("+strafe", IN_StrafeDown);
	gEngfuncs.pfnAddCommand ("-strafe", IN_StrafeUp);
	gEngfuncs.pfnAddCommand ("+moveleft", IN_MoveleftDown);
	gEngfuncs.pfnAddCommand ("-moveleft", IN_MoveleftUp);
	gEngfuncs.pfnAddCommand ("+moveright", IN_MoverightDown);
	gEngfuncs.pfnAddCommand ("-moveright", IN_MoverightUp);
	gEngfuncs.pfnAddCommand ("+sprint", IN_SpeedDown);
	gEngfuncs.pfnAddCommand ("-sprint", IN_SpeedUp);
	gEngfuncs.pfnAddCommand ("+attack", IN_AttackDown);
	gEngfuncs.pfnAddCommand ("-attack", IN_AttackUp);
	gEngfuncs.pfnAddCommand ("+attack2", IN_Attack2Down);
	gEngfuncs.pfnAddCommand ("-attack2", IN_Attack2Up);
	gEngfuncs.pfnAddCommand ("+use", IN_UseDown);
	gEngfuncs.pfnAddCommand ("-use", IN_UseUp);
	gEngfuncs.pfnAddCommand ("+jump", IN_JumpDown);
	gEngfuncs.pfnAddCommand ("-jump", IN_JumpUp);
	gEngfuncs.pfnAddCommand ("impulse", IN_Impulse);
	gEngfuncs.pfnAddCommand ("+klook", IN_KLookDown);
	gEngfuncs.pfnAddCommand ("-klook", IN_KLookUp);
	gEngfuncs.pfnAddCommand ("+mlook", IN_MLookDown);
	gEngfuncs.pfnAddCommand ("-mlook", IN_MLookUp);
	gEngfuncs.pfnAddCommand ("+jlook", IN_JLookDown);
	gEngfuncs.pfnAddCommand ("-jlook", IN_JLookUp);
	gEngfuncs.pfnAddCommand ("+duck", IN_DuckDown);
	gEngfuncs.pfnAddCommand ("-duck", IN_DuckUp);
	gEngfuncs.pfnAddCommand ("+reload", IN_ReloadDown);
	gEngfuncs.pfnAddCommand ("-reload", IN_ReloadUp);
	gEngfuncs.pfnAddCommand ("+alt1", IN_Alt1Down);
	gEngfuncs.pfnAddCommand ("-alt1", IN_Alt1Up);
	gEngfuncs.pfnAddCommand ("+score", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-score", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+showscores", IN_ScoreDown);
	gEngfuncs.pfnAddCommand ("-showscores", IN_ScoreUp);
	gEngfuncs.pfnAddCommand ("+graph", IN_GraphDown);
	gEngfuncs.pfnAddCommand ("-graph", IN_GraphUp);
	gEngfuncs.pfnAddCommand ("+break",IN_BreakDown);
	gEngfuncs.pfnAddCommand ("-break",IN_BreakUp);
	gEngfuncs.pfnAddCommand ("+attack3", IN_Attack3Down);
	gEngfuncs.pfnAddCommand ("-attack3", IN_Attack3Up);
	gEngfuncs.pfnAddCommand ("+dodge", IN_DodgeDown);
	gEngfuncs.pfnAddCommand ("-dodge", IN_DodgeUp);
	gEngfuncs.pfnAddCommand ("xboxfix", IN_PadFix_guess);
	gEngfuncs.pfnAddCommand ("documents", IN_Documents);
	gEngfuncs.pfnAddCommand ("+inventory", IN_InventoryDown);
	gEngfuncs.pfnAddCommand ("-inventory", IN_InventoryUp);
	gEngfuncs.pfnAddCommand ("+objectives", IN_ObjectivesDown);
	gEngfuncs.pfnAddCommand ("-objectives", IN_ObjectivesUp);

	lookstrafe			= gEngfuncs.pfnRegisterVariable ( "lookstrafe", "0", FCVAR_ARCHIVE );
	lookspring			= gEngfuncs.pfnRegisterVariable ( "lookspring", "0", FCVAR_ARCHIVE );
	cl_anglespeedkey	= gEngfuncs.pfnRegisterVariable ( "cl_anglespeedkey", "0.67", 0 );
	cl_yawspeed			= gEngfuncs.pfnRegisterVariable ( "cl_yawspeed", "210", 0 );
	cl_pitchspeed		= gEngfuncs.pfnRegisterVariable ( "cl_pitchspeed", "225", 0 );
	cl_upspeed			= gEngfuncs.pfnRegisterVariable ( "cl_upspeed", "320", 0 );
	cl_forwardspeed		= gEngfuncs.pfnRegisterVariable ( "cl_forwardspeed", "230", FCVAR_ARCHIVE );
	cl_backspeed		= gEngfuncs.pfnRegisterVariable ( "cl_backspeed", "200", FCVAR_ARCHIVE );
	cl_sidespeed		= gEngfuncs.pfnRegisterVariable ( "cl_sidespeed", "210", FCVAR_ARCHIVE );
	cl_movespeedkey		= gEngfuncs.pfnRegisterVariable ( "cl_movespeedkey", "0.3", 0 );
	cl_pitchup			= gEngfuncs.pfnRegisterVariable ( "cl_pitchup", "89", 0 );
	cl_pitchdown		= gEngfuncs.pfnRegisterVariable ( "cl_pitchdown", "85", 0 );

	cl_vsmoothing		= gEngfuncs.pfnRegisterVariable ( "cl_vsmoothing", "0.05", FCVAR_ARCHIVE );

	cg_water			= gEngfuncs.pfnRegisterVariable ( "cg_water", "1", FCVAR_ARCHIVE );
	cl_ripples			= gEngfuncs.pfnRegisterVariable ( "cl_ripples", "1", FCVAR_ARCHIVE );
	cl_subtitles		= gEngfuncs.pfnRegisterVariable ( "cl_subtitles", "1", FCVAR_ARCHIVE );
	r_weather			= gEngfuncs.pfnRegisterVariable ( "r_weather", "1", FCVAR_ARCHIVE );
	fps_current			= gEngfuncs.pfnRegisterVariable ( "fps_current", "1", 0 );
	cl_noiseeffect		= gEngfuncs.pfnRegisterVariable ( "cl_noiseeffect", "0", FCVAR_ARCHIVE );
	cl_noiseamount		= gEngfuncs.pfnRegisterVariable ( "cl_noiseamount", "0.5", FCVAR_ARCHIVE );
	cl_vignette			= gEngfuncs.pfnRegisterVariable ( "cl_vignette", "1", FCVAR_ARCHIVE );
	cl_fovmultiplier	= gEngfuncs.pfnRegisterVariable ( "cl_fovmultiplier", "1", FCVAR_ARCHIVE );

	m_pitch			    = gEngfuncs.pfnRegisterVariable ( "m_pitch","0.022", FCVAR_ARCHIVE );
	m_yaw				= gEngfuncs.pfnRegisterVariable ( "m_yaw","0.022", FCVAR_ARCHIVE );
	m_forward			= gEngfuncs.pfnRegisterVariable ( "m_forward","1", FCVAR_ARCHIVE );
	m_side				= gEngfuncs.pfnRegisterVariable ( "m_side","0.8", FCVAR_ARCHIVE );

	cl_lazyweapon_force		= gEngfuncs.pfnRegisterVariable ( "cl_lazyweapon_force", "2", FCVAR_ARCHIVE );
	cl_weapon_idle_speed	= gEngfuncs.pfnRegisterVariable ( "cl_weapon_idle_speed", "4", FCVAR_ARCHIVE );
	cl_weapon_idle_dampener	= gEngfuncs.pfnRegisterVariable ( "cl_weapon_idle_dampener", "2", FCVAR_ARCHIVE );
	xbox_controller_sensitivityx	= gEngfuncs.pfnRegisterVariable ( "xbox_controller_sensitivityx", "10", FCVAR_ARCHIVE );
	xbox_controller_sensitivityy	= gEngfuncs.pfnRegisterVariable ( "xbox_controller_sensitivityy", "10", FCVAR_ARCHIVE );
	cl_smokeparticlelimit	= gEngfuncs.pfnRegisterVariable ( "cl_smokeparticlelimit", "5", FCVAR_ARCHIVE );
	cl_cheapweaponsmoke		= gEngfuncs.pfnRegisterVariable ( "cl_cheapweaponsmoke", "0", FCVAR_ARCHIVE );
	cl_bloodeffects			= gEngfuncs.pfnRegisterVariable ( "cl_bloodeffects", "1", FCVAR_ARCHIVE );
	cl_cameraeffect			= gEngfuncs.pfnRegisterVariable ( "cl_cameraeffect", "0", FCVAR_ARCHIVE );
	cl_crosshair			= gEngfuncs.pfnRegisterVariable ( "cl_crosshair", "1", FCVAR_ARCHIVE );
	cl_fastcredits			= gEngfuncs.pfnRegisterVariable ( "cl_fastcredits", "0", FCVAR_ARCHIVE );
	cl_waterspeed_x			= gEngfuncs.pfnRegisterVariable ( "cl_waterspeed_x", "32", FCVAR_ARCHIVE );
	cl_waterspeed_y			= gEngfuncs.pfnRegisterVariable ( "cl_waterspeed_y", "20", FCVAR_ARCHIVE );
	cl_coop_language		= gEngfuncs.pfnRegisterVariable ( "cl_coop_language", "1", FCVAR_ARCHIVE );
	cl_lowflashlight		= gEngfuncs.pfnRegisterVariable ( "cl_lowflashlight", "0", FCVAR_ARCHIVE );
	cl_propdistance			= gEngfuncs.pfnRegisterVariable ( "cl_propdistance", "0", FCVAR_ARCHIVE );
	cl_downwiththesickness	= gEngfuncs.pfnRegisterVariable ( "cl_downwiththesickness", "0", FCVAR_ARCHIVE );
	cl_nodoubletapdodge		= gEngfuncs.pfnRegisterVariable ( "cl_nodoubletapdodge", "0", FCVAR_ARCHIVE );

	// Initialize third person camera controls.
	CAM_Init();
	// Initialize inputs
	IN_Init();
	// Initialize keyboard
	KB_Init();
	// Initialize view system
	V_Init();

	if ( SteamAPI_Init() )
		bSteamInit_guess = true;

	iDonator_guess = 0;

	bool bGroup2 = false;
	if ( IsInDonatorGroup2_guess() )
		bGroup2 = true;

	if ( IsInDonatorGroup1_guess() )
	{
		byte *pFile = gEngfuncs.COM_LoadFile( "donatormsg.txt", 5, NULL );

		if ( !pFile )
		{
			char szDir[MAX_PATH];

			GetCurrentPath( szDir );

			for ( int i = 0; szDir[i]; i++ )
				szDir[i] = tolower( szDir[i] );

			MessageBox( NULL, "Thank you so much for donating!\nYour generosity has kept the team motivated, focused and happy\nWithout you, the mod would be so much less. We salute you, bronators\n:biden:", "Thank you", MB_OK );

			const char *pszGameDir = gEngfuncs.pfnGetGameDirectory();

			if ( pszGameDir )
			{
				char szFile[256];

				strcpy( szFile, pszGameDir );
				strcat( szFile, "/donatormsg.txt" );

				std::ofstream file( szFile );

				if ( file )
				{
					file << ":biden: :biden: :biden: :yow:\n";
					file.close();
				}
			}
		}
		else
			gEngfuncs.COM_FreeFile( pFile );

		if ( !bGroup2 )
			iDonator_guess = 1;
		else if ( bGroup2 == true )
			iDonator_guess = 2;
	}
	else
	{
		iDonator_guess = 0;

		if ( !bGroup2 )
			iDonator_guess = 0;
		else if ( bGroup2 == true )
			iDonator_guess = 3;
	}

	if ( SteamUser() )
	{
		CSteamID steamID = SteamUser()->GetSteamID();

		if ( steamID.ConvertToUint64() == 268893370.0 )
			iDonator_guess = 4;

		if ( steamID.ConvertToUint64() == 268992206.0 )
			iDonator_guess = 4;
	}

	iMenuMode_guess = 1;
	gEngfuncs.pfnClientCmd( "map c_game_menu1" );
}

/*
============
ShutdownInput
============
*/
void ShutdownInput (void)
{
	IN_Shutdown();
	KB_Shutdown();
}

void DLLEXPORT HUD_Shutdown( void )
{
	CoFR_ClientShutdown();
	ShutdownInput();
	IrrKlangDestroy_guess();
}
