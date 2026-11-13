#include "cofr_legacy.h"
//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// view/refresh setup functions

#include "hud.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h" // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"
#include "gl_renderer.h" // buz
#include "gl_dlight.h" // buz

extern int g_flashlight;
void SetupFlashlight(vec3_t origin, vec3_t angles, float time, float frametime);


// buz
vec3_t	render_origin;
vec3_t	render_angles;

vec3_t g_vecUp;
vec3_t g_vecRight;
vec3_t g_vecForward;



// Spectator Mode
extern "C" 
{
	float	vecNewViewAngles[3];
	int		iHasNewViewAngles;
	float	vecNewViewOrigin[3];
	int		iHasNewViewOrigin;
	int		iIsSpectator;
}

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

extern "C" 
{
	int CL_IsThirdPerson( void );
	void CL_CameraOffset( float *ofs );

	void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams );

	void PM_ParticleLine( float *start, float *end, int pcolor, float life, float vert);
	int		PM_GetVisEntInfo( int ent );
	int		PM_GetPhysEntInfo( int ent );
	void	InterpolateAngles(  float * start, float * end, float * output, float frac );
	void	NormalizeAngles( float * angles );
	float	Distance(const float * v1, const float * v2);
	float	AngleBetweenVectors(  const float * v1,  const float * v2 );

	float	vJumpOrigin[3];
	float	vJumpAngles[3];
}

void V_DropPunchAngle ( float frametime, float *ev_punchangle );
void VectorAngles( const float *forward, float *angles );

#include "r_studioint.h"
#include "parsemsg.h"
#include "com_model.h"
#include "vgui_TeamFortressViewport.h"
#include "RECBInventory_guess.h"

extern engine_studio_api_t IEngineStudio;

/*
The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.
*/

extern cvar_t	*cl_forwardspeed;
extern cvar_t	*chase_active;
extern cvar_t	*scr_ofsx, *scr_ofsy, *scr_ofsz;
extern cvar_t	*cl_vsmoothing;

#define	CAM_MODE_RELAX		1
#define CAM_MODE_FOCUS		2

vec3_t		v_origin, v_angles, v_cl_angles, v_sim_org, v_lastAngles;
float		v_frametime, v_lastDistance;	
float		v_cameraRelaxAngle	= 5.0f;
float		v_cameraFocusAngle	= 35.0f;
int			v_cameraMode = CAM_MODE_FOCUS;
qboolean	v_resetCamera = 1;

int		g_iWaterLevel; // buz
vec3_t	g_CrosshairAngle; // buz

//extern int g_DontDrawWorld; // buz

vec3_t ev_punchangle;
vec3_t ev_punchanglevel_guess;
int iPaused_guess;

bool IrrKlangIsPaused_guess( void );
void IrrKlangSetPaused_guess( bool bPaused );

cvar_t	*scr_ofsx;
cvar_t	*scr_ofsy;
cvar_t	*scr_ofsz;

cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;

// These cvars are not registered (so users can't cheat), so set the ->value field directly
// Register these cvars in V_Init() if needed for easy tweaking
cvar_t	v_iyaw_cycle		= {"v_iyaw_cycle", "2", 0, 2};
cvar_t	v_iroll_cycle		= {"v_iroll_cycle", "0.5", 0, 0.5};
cvar_t	v_ipitch_cycle		= {"v_ipitch_cycle", "1", 0, 1};
cvar_t	v_iyaw_level		= {"v_iyaw_level", "0.3", 0, 0.3};
cvar_t	v_iroll_level		= {"v_iroll_level", "0.1", 0, 0.1};
cvar_t	v_ipitch_level		= {"v_ipitch_level", "0.3", 0, 0.3};

float	v_idlescale;  // used by TFC for concussion grenade effect

//=============================================================================
/*
void V_NormalizeAngles( float *angles )
{
	int i;
	// Normalize angles
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

/*
===================
V_InterpolateAngles

Interpolate Euler angles.
FIXME:  Use Quaternions to avoid discontinuities
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================

void V_InterpolateAngles( float *start, float *end, float *output, float frac )
{
	int i;
	float ang1, ang2;
	float d;
	
	V_NormalizeAngles( start );
	V_NormalizeAngles( end );

	for ( i = 0 ; i < 3 ; i++ )
	{
		ang1 = start[i];
		ang2 = end[i];

		d = ang2 - ang1;
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		output[i] = ang1 + d * frac;
	}

	V_NormalizeAngles( output );
} */

// Quakeworld bob code, this fixes jitters in the mutliplayer since the clock (pparams->time) isn't quite linear
float V_CalcBob ( struct ref_params_s *pparams )
{
	static	double	bobtime;
	static float	bob;
	float	cycle;
	static float	lasttime;
	vec3_t	vel;
	

	if ( pparams->onground == -1 ||
		 pparams->time == lasttime )
	{
		// just use old value
		return bob;	
	}

	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if ( cycle < cl_bobup->value )
	{
		cycle = M_PI * cycle / cl_bobup->value;
	}
	else
	{
		cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );
	}

	// bob is proportional to simulated velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	VectorCopy( pparams->simvel, vel );
	vel[2] = 0;

	bob = sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = min( bob, 4 );
	bob = max( bob, -7 );
	return bob;
	
}

/*
===============
V_CalcRoll
Used by view and sv_user
===============
*/
float V_CalcRoll (vec3_t angles, vec3_t velocity, float rollangle, float rollspeed )
{
    float   sign;
    float   side;
    float   value;
	vec3_t  forward, right, up;
    
	AngleVectors ( angles, forward, right, up );
    
	side = DotProduct (velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabsf( side );
    
	value = rollangle;
    if (side < rollspeed)
	{
		side = side * value / rollspeed;
	}
    else
	{
		side = value;
	}
	return side * sign;
}

typedef struct pitchdrift_s
{
	float		pitchvel;
	int			nodrift;
	float		driftmove;
	double		laststop;
} pitchdrift_t;

static pitchdrift_t pd;

void V_StartPitchDrift( void )
{
	return;

	if ( pd.laststop == gEngfuncs.GetClientTime() )
	{
		return;		// something else is keeping it from drifting
	}

	if ( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.nodrift = 0;
		pd.driftmove = 0;
	}
}

void V_StopPitchDrift ( void )
{
	pd.laststop = gEngfuncs.GetClientTime();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.
===============
*/
void V_DriftPitch ( struct ref_params_s *pparams )
{
	float		delta, move;

	if ( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback || pparams->spectator )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (pd.nodrift)
	{
		if ( (fabs)( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
			pd.driftmove = 0;
		else
			pd.driftmove += pparams->frametime;
	
		if ( pd.driftmove > v_centermove->value)
		{
			V_StartPitchDrift ();
		}
		return;
	}
	
	delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if (!delta)
	{
		pd.pitchvel = 0;
		return;
	}

	move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if (delta > 0)
	{
		if (move > delta)
		{
			pd.pitchvel = 0;
			move = delta;
		}
		pparams->cl_viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			pd.pitchvel = 0;
			move = -delta;
		}
		pparams->cl_viewangles[PITCH] -= move;
	}
}

/* 
============================================================================== 
						VIEW RENDERING 
============================================================================== 
*/ 

/*
==================
V_CalcGunAngle
==================
*/
void V_CalcGunAngle ( struct ref_params_s *pparams )
{	
	cl_entity_t *viewent;
	
	viewent = gEngfuncs.GetViewModel();
	if ( !viewent )
		return;

	viewent->angles[YAW]   =  pparams->viewangles[YAW]   + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = -pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25;
	viewent->angles[ROLL]  -= v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * (v_ipitch_level.value * 0.5);
	viewent->angles[YAW]   -= v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;

	// buz: add half of the punchangle to viewmodel
	viewent->angles[0] -= pparams->punchangle[0]/2;
	viewent->angles[1] += pparams->punchangle[1]/2;
	viewent->angles[2] += pparams->punchangle[2]/2;

	viewent->angles[PITCH] += gHUD.m_vecViewOfs_guess[2];

	if ( viewent->model && gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId == 1 )
	{
		viewent->angles[PITCH] -= gHUD.m_flYawOfs_guess;
		viewent->angles[YAW] -= gHUD.m_flPitchOfs_guess;
	}

	VectorCopy( viewent->angles, viewent->curstate.angles );
	VectorCopy( viewent->angles, viewent->latched.prevangles );	
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle ( struct ref_params_s *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll ( struct ref_params_s *pparams )
{
	float		side;
	cl_entity_t *viewentity;
	
	viewentity = gEngfuncs.GetEntityByIndex( pparams->viewentity );
	if ( !viewentity )
		return;

	side = V_CalcRoll ( viewentity->angles, pparams->simvel, pparams->movevars->rollangle, pparams->movevars->rollspeed );
//	side = V_CalcRoll ( viewentity->angles, pparams->simvel, 5, 200 ); // buz test

	pparams->viewangles[ROLL] += side;

//	if ( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ) )
//	{
//		// only roll the view if the player is dead and the viewheight[2] is nonzero 
//		// this is so deadcam in multiplayer will work.
//		pparams->viewangles[ROLL] = 80;	// dead view angle
//		return;
//	}
}


/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef ( struct ref_params_s *pparams )
{
	cl_entity_t	*ent, *view;
	float		old;

	// ent is the player model ( visible when out of body )
	ent = gEngfuncs.GetLocalPlayer();
	
	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	VectorCopy ( pparams->simorg, pparams->vieworg );
	VectorCopy ( pparams->cl_viewangles, pparams->viewangles );

	view->model = NULL;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;

	V_AddIdle ( pparams );

	if ( gEngfuncs.IsSpectateOnly() )
	{
		// in HLTV we must go to 'intermission' position by ourself
		VectorCopy( gHUD.m_Spectator.m_cameraOrigin, pparams->vieworg );
		VectorCopy( gHUD.m_Spectator.m_cameraAngles, pparams->viewangles );
	}

	v_idlescale = old;

	v_cl_angles = pparams->cl_viewangles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

#define ORIGIN_BACKUP 64
#define ORIGIN_MASK ( ORIGIN_BACKUP - 1 )

typedef struct 
{
	float Origins[ ORIGIN_BACKUP ][3];
	float OriginTime[ ORIGIN_BACKUP ];

	float Angles[ ORIGIN_BACKUP ][3];
	float AngleTime[ ORIGIN_BACKUP ];

	int CurrentOrigin;
	int CurrentAngle;
} viewinterp_t;

/*
==================
V_CalcRefdef

==================
*/
extern void RenderFog( void ); //LRC
extern int g_DrawHeadShied; // buz
extern int HeadShieldThink( void ); // buz

// cof
extern "C" int iStepPunch_guess;		// pm_shared.c
extern "C" int iStepPunchRun_guess;

void PunchViewAngles_guess( float x, float y, float z );
void MuzzleFlash_guess( cl_entity_t *pEntity, int iAttachment, bool bAlt, float flScale );
void AddViewmodelBob_guess( struct ref_params_s *pparams, Vector &origin, Vector &angles, float frametime, float curtime, cl_entity_t *view, bool bLeft );
void CalcViewModelLag_guess( float frametime, Vector &origin, Vector &angles, Vector &original_angles, cl_entity_t *view, bool bLeft );
void CreateMagliteLight_guess( vec3_t origin, vec3_t angles, float time, float frametime );
void SetupViewModelLight_guess( vec3_t origin, vec3_t angles, float time, float frametime );

extern int iUnknown1054506C_guess;
extern int iWheelchair_guess;			// the wheelchair entity (0 when off)
extern int iBoat_guess;					// the boat entity (0 when off)
extern vec3_t vecWheelchairOrigin_guess;	// attachment 0 of the wheelchair, set by the studio renderer
extern vec3_t vecBoatOrigin_guess;		// attachment 0 of the boat, set by the studio renderer
extern cl_entity_t *pTelescopeEnt_guess;
extern int iTelescopeFlag_guess;
extern cl_entity_t *pAttachCamEnt_guess;
extern int iAttachCamFlag_guess;
extern int iUnknown10542BC8_guess;
extern bool bAttachCam_guess;
extern bool bUnknown104CE4A3_guess;
extern vec3_t vecCameraAttachment_guess;
extern vec3_t vecCameraAngles_guess;
extern vec3_t vecModelAttachOrigin_guess;	// attachment of the ModelAttach entity, set by the studio renderer
extern vec3_t vecDualAttachment0_guess;		// attachments 0 and 1 of the left hand weapon, set by the studio renderer
extern vec3_t vecDualAttachment1_guess;
extern int iModelAttachEnt_guess;
extern int iModelAttachNum_guess;
extern int g_iHeadShieldType_guess;
extern int g_iDualWeapon1_guess;
extern int g_iDualWeapon2_guess;
extern cl_entity_t g_HeadShieldEnt;

vec3_t vecIronsightOfs_guess;
vec3_t vecWheelchairAngles_guess;		// added to the wheelchair angles (the knock back subtracts from it)
float flMouseOfs2X_guess;				// second mouse look pair of IN_MouseMove
float flMouseOfs2Y_guess;
cl_entity_t *pCamTargetEnt_guess;
cl_entity_t *pDualEnt_guess;			// the left hand weapon entity of the dual wield mode
vec3_t vecWheelchairViewAngles_guess;	// the view angles in the wheelchair
float flUnknown105595A0_guess;
float flUnknown105595A4_guess;
float flUnknown105595A8_guess;
int iUnknown105595AC_guess;
bool bUnknown10559514_guess;			// cleared by the AttachCam message
bool bCanDodge_guess;					// on the ground and not noclipping

void V_CalcNormalRefdef ( struct ref_params_s *pparams )
{
	cl_entity_t		*ent, *view;
	int				i;
	vec3_t			angles;
	float			bob, waterOffset;
	static viewinterp_t		ViewInterp;

	static float oldz = 0;
	static float lasttime;

	vec3_t camAngles, camForward, camRight, camUp;
	cl_entity_t *pwater;

	static struct model_s *savedviewmodel;

//	CONPRINT("calcrefdef\n");

	iUnknown1054506C_guess = pparams->waterlevel;

	// cof: view punch of a multiplayer step
	if ( iStepPunch_guess )
	{
		iStepPunch_guess = 0;

		if ( iStepPunchRun_guess )
			PunchViewAngles_guess( -3, 1, 0 );
		else
			PunchViewAngles_guess( -0.75, 0.5, 0 );
	}

	CoFR_RefreshCamera();
	// cof: wheelchair view
	if ( iWheelchair_guess && ( vecWheelchairOrigin_guess[0] || vecWheelchairOrigin_guess[1] || vecWheelchairOrigin_guess[2] ) )
	{
		vec3_t forward, right, up;
		cl_entity_t *player, *viewmodel;

		vecWheelchairOrigin_guess.z -= 24;

		VectorCopy( vecWheelchairOrigin_guess, pparams->simorg );
		VectorCopy( vecWheelchairOrigin_guess, pparams->vieworg );

		viewmodel = gEngfuncs.GetViewModel();

		vecWheelchairViewAngles_guess[0] = ((cl_entity_t *)iWheelchair_guess)->curstate.angles[0] + vecWheelchairAngles_guess[0];
		vecWheelchairViewAngles_guess[1] = ((cl_entity_t *)iWheelchair_guess)->curstate.angles[1] - vecWheelchairAngles_guess[1];
		vecWheelchairViewAngles_guess[2] = ((cl_entity_t *)iWheelchair_guess)->curstate.angles[2];

		VectorCopy( vecWheelchairViewAngles_guess, pparams->viewangles );
		VectorCopy( pparams->viewangles, pparams->cl_viewangles );

		player = gEngfuncs.GetLocalPlayer();

		if ( player )
		{
			VectorCopy( pparams->viewangles, player->angles );
			VectorCopy( pparams->viewangles, player->curstate.angles );
		}

		if ( viewmodel )
		{
			viewmodel->curstate.origin = vecWheelchairOrigin_guess;
			viewmodel->origin = vecWheelchairOrigin_guess;
			viewmodel->angles = vecWheelchairViewAngles_guess;
		}

		gEngfuncs.pfnAngleVectors( vecWheelchairViewAngles_guess, forward, right, up );
	}

	if ( gEngfuncs.IsNoClipping() || !pparams->onground )
		bCanDodge_guess = false;

	if ( !gEngfuncs.IsNoClipping() && pparams->onground && !bCanDodge_guess )
		bCanDodge_guess = true;

	// buz
	g_DrawHeadShied = 0;
	if (pparams->nextView == 1)
	{
		pparams->onlyClientDraw = 1;
		pparams->nextView = 0;
		g_DrawHeadShied = 1;
		return;
	}
	else if ( ( pparams->nextView == 0 && HeadShieldThink() && g_iHeadShieldType_guess == 6 ) ||
		( pparams->nextView == 0 && HeadShieldThink() && g_iHeadShieldType_guess == 7 ) ||
		( pparams->nextView == 0 && HeadShieldThink() && !(pparams->viewentity > pparams->maxclients) ) )
	{
		pparams->nextView = 1;
	}

//	gEngfuncs.Con_Printf("cross angle: %f, %f, %f\n", pparams->crosshairangle[0], pparams->crosshairangle[1], pparams->crosshairangle[2]);
	VectorCopy(pparams->crosshairangle, g_CrosshairAngle); // save it for crosshair rendering

	V_DriftPitch ( pparams );

	if ( gEngfuncs.IsSpectateOnly() )
	{
		ent = gEngfuncs.GetEntityByIndex( g_iUser2 );
	}
	else
	{
		// ent is the player model ( visible when out of body )
		ent = gEngfuncs.GetLocalPlayer();
	}

	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	// cof: the weapon model takes the render color and amount of the player
	view->curstate.rendercolor.r = ent->curstate.rendercolor.r;
	view->curstate.rendercolor.g = ent->curstate.rendercolor.g;
	view->curstate.rendercolor.b = ent->curstate.rendercolor.b;
	view->curstate.renderamt = ent->curstate.renderamt;

	//LRC - don't show weapon models when we're drawing the sky.
	if (gHUD.m_iSkyMode == SKY_ON)
	{
		savedviewmodel = view->model;
		view->model = NULL;
	}

	// cof: the weapon model is in the water
	if ( gEngfuncs.PM_PointContents( view->origin, NULL ) == CONTENTS_WATER )
	{
		flUnknown105595A0_guess = gEngfuncs.GetClientTime();
		flUnknown105595A4_guess = 10;
		flUnknown105595A8_guess = gEngfuncs.GetClientTime();
		iUnknown105595AC_guess = 0;
	}

	// transform the view offset by the model's matrix to get the offset from
	// model origin for the view
	bob = V_CalcBob ( pparams );

	// refresh position
	VectorCopy ( pparams->simorg, pparams->vieworg );
	pparams->vieworg[2] += ( bob );
	VectorAdd( pparams->vieworg, pparams->viewheight, pparams->vieworg );

	VectorCopy ( pparams->cl_viewangles, pparams->viewangles );

	gEngfuncs.V_CalcShake();
	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );

	// cof: telescope view
	if ( pTelescopeEnt_guess )
	{
		VectorCopy( pTelescopeEnt_guess->origin, pparams->vieworg );

		if ( !iTelescopeFlag_guess )
		{
			vec3_t vecAngles, forward, right;
			vec3_t vecStart, vecEnd;
			pmtrace_t *tr;

			VectorCopy( pparams->viewangles, vecAngles );
			gEngfuncs.pfnAngleVectors( vecAngles, forward, right, NULL );

			vecEnd = pTelescopeEnt_guess->origin + forward * 4096;
			vecStart = forward * 128 + pTelescopeEnt_guess->origin;

			tr = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 1, -1 );

			if ( tr->ent )
			{
				cl_entity_t *pHit = gEngfuncs.GetEntityByIndex( PM_GetPhysEntInfo( tr->ent ) );

				if ( pHit && pHit->curstate.solid == SOLID_BSP && pHit->curstate.renderfx == 162 )
				{
					gEngfuncs.pfnServerCmd( "telescopefinish" );
					iTelescopeFlag_guess = 1;
				}
			}
		}
	}

	// cof: boat view
	if ( iBoat_guess && ( vecBoatOrigin_guess[0] || vecBoatOrigin_guess[1] || vecBoatOrigin_guess[2] ) )
	{
		VectorCopy( vecBoatOrigin_guess, pparams->vieworg );
		VectorCopy( ((cl_entity_t *)iBoat_guess)->curstate.angles, pparams->viewangles );
	}

	// cof: camera attached to an entity
	if ( pAttachCamEnt_guess && bAttachCam_guess && !bUnknown104CE4A3_guess )
	{
		iUnknown10542BC8_guess = 0;

		VectorCopy( vecCameraAttachment_guess, pparams->vieworg );
		VectorCopy( vecCameraAngles_guess, pparams->viewangles );

		if ( !bUnknown10559514_guess )
		{
			bUnknown10559514_guess = true;
			gEngfuncs.SetViewAngles( pparams->viewangles );
		}
	}
	else if ( bUnknown104CE4A3_guess )
	{
		VectorAdd( pparams->viewangles, vecCameraAngles_guess, pparams->viewangles );

		if ( !bUnknown10559514_guess )
		{
			bUnknown10559514_guess = true;
			gEngfuncs.SetViewAngles( pparams->viewangles );
		}
	}

	iUnknown10542BC8_guess = 0;

	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis

	pparams->vieworg[0] += 1.0/32;
	pparams->vieworg[1] += 1.0/32;
	pparams->vieworg[2] += 1.0/32;

	// Check for problems around water, move the viewer artificially if necessary
	// -- this prevents drawing errors in GL due to waves

	waterOffset = 0;
	if ( pparams->waterlevel >= 2 )
	{
		int		i, contents, waterDist, waterEntity;
		vec3_t	point;
		waterDist = cl_waterdist->value;

		if ( pparams->hardware )
		{
			waterEntity = gEngfuncs.PM_WaterEntity( pparams->simorg );
			if ( waterEntity >= 0 && waterEntity < pparams->max_entities )
			{
				pwater = gEngfuncs.GetEntityByIndex( waterEntity );
				if ( pwater && ( pwater->model != NULL ) )
				{
					waterDist += ( pwater->curstate.scale * 16 );	// Add in wave height
				}
			}
		}
		else
		{
			waterEntity = 0;	// Don't need this in software
		}

		VectorCopy( pparams->vieworg, point );

		// Eyes are above water, make sure we're above the waves
		if ( pparams->waterlevel == 2 )
		{
			point[2] -= waterDist;
			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents > CONTENTS_WATER )
					break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water.  Make sure we're far enough under
			point[2] += waterDist;

			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents <= CONTENTS_WATER )
					break;
				point[2] -= 1;
			}
			waterOffset = (point[2] - waterDist) - pparams->vieworg[2];
		}
	}

	pparams->vieworg[2] += waterOffset;

	V_CalcViewRoll ( pparams );

	V_AddIdle ( pparams );

	// offsets
	VectorCopy( pparams->cl_viewangles, angles );

	AngleVectors ( angles, pparams->forward, pparams->right, pparams->up );

	// cof: the flashlight is set up at the end, from the weapon model
	gHUD.m_vecSpecTankAngles_guess = pparams->viewangles;
	gHUD.m_flSpecTankTime_guess = pparams->time;
	gHUD.m_flSpecTankFrametime_guess = pparams->frametime;

	// buz: spec tank view
	if (gHUD.m_SpecTank_on)
	{
		VectorCopy(gHUD.m_SpecTank_point, pparams->vieworg);
		VectorMA(pparams->vieworg, -gHUD.m_SpecTank_distFwd, pparams->forward, pparams->vieworg);
		VectorMA(pparams->vieworg, gHUD.m_SpecTank_distUp, pparams->up, pparams->vieworg);
	}


	// don't allow cheats in multiplayer
	if ( pparams->maxclients <= 1 )
	{
		for ( i=0 ; i<3 ; i++ )
		{
			pparams->vieworg[i] += scr_ofsx->value*pparams->forward[i] + scr_ofsy->value*pparams->right[i] + scr_ofsz->value*pparams->up[i];
		}
	}

	// Treating cam_ofs[2] as the distance
	if( CL_IsThirdPerson() )
	{
		vec3_t ofs;
		vec3_t vecStart, vecEnd, vecDir, vecMove, vecUp, vecDown;
		pmtrace_t tr, tr2;

		vecStart = pparams->vieworg;

		ofs[0] = ofs[1] = ofs[2] = 0.0;

		CL_CameraOffset( (float *)&ofs );

		VectorCopy( ofs, camAngles );
		camAngles[ ROLL ]	= 0;

		AngleVectors( camAngles, camForward, camRight, camUp );

		for ( i = 0; i < 3; i++ )
		{
			pparams->vieworg[ i ] -= ofs[2] * camForward[ i ];
		}

		// cof: keep the camera out of the walls
		VectorSubtract( pparams->vieworg, vecStart, vecDir );
		VectorNormalize( vecDir );
		vecMove = vecDir * 16;
		VectorAdd( pparams->vieworg, vecMove, vecEnd );

		gEngfuncs.pEventAPI->EV_PushPMStates();

		do
		{
			gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
			gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, vecEnd, PM_WORLD_ONLY, -1, &tr );

			if ( tr.fraction == 0 )
				vecStart[2] -= 1;
		} while ( tr.fraction == 0 );

		VectorSubtract( tr.endpos, vecMove, pparams->vieworg );

		while ( gEngfuncs.PM_PointContents( pparams->vieworg, NULL ) == CONTENTS_SKY )
		{
			vec3_t vecBack = ( vecStart - vecEnd ).Normalize();
			VectorAdd( pparams->vieworg, vecBack, pparams->vieworg );
		}

		vecUp = Vector( pparams->vieworg ) + Vector( 0, 0, 4096 );
		vecDown = Vector( pparams->vieworg ) - Vector( 0, 0, 8 );

		gEngfuncs.pEventAPI->EV_PlayerTrace( vecDown, vecUp, PM_WORLD_ONLY, -1, &tr2 );

		if ( tr.endpos[2] == tr2.endpos[2] )
			pparams->vieworg[2] -= 20;

		gEngfuncs.pEventAPI->EV_PopPMStates();
	}

	// Give gun our viewangles
	VectorCopy ( pparams->cl_viewangles, view->angles );

	// set up gun position
	V_CalcGunAngle ( pparams );

	// Use predicted origin as view origin.
	VectorCopy ( pparams->simorg, view->origin );
	view->origin[2] += ( waterOffset );
	VectorAdd( view->origin, pparams->viewheight, view->origin );

	// Let the viewmodel shake at about 10% of the amplitude
	gEngfuncs.V_ApplyShake( view->origin, view->angles, 0.9 );

	for ( i = 0; i < 3; i++ )
	{
		view->angles[ i ] += bob * 0.4 * pparams->forward[ i ];
	}
	view->origin[2] += bob;

	// throw in a little tilt.
	view->angles[YAW]   -= bob * 0.5;
	view->angles[ROLL]  -= bob * 1;
	view->angles[PITCH] -= bob * 0.3;

	// pushing the view origin down off of the same X/Z plane as the ent's origin will give the
	// gun a very nice 'shifting' effect when the player looks up/down. If there is a problem
	// with view model distortion, this may be a cause. (SJB).
	view->origin[2] -= 1;

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (pparams->viewsize == 110)
	{
		view->origin[2] += 1;
	}
	else if (pparams->viewsize == 100)
	{
		view->origin[2] += 2;
	}
	else if (pparams->viewsize == 90)
	{
		view->origin[2] += 1;
	}
	else if (pparams->viewsize == 80)
	{
		view->origin[2] += 0.5;
	}

	// Add in the punchangle, if any
	VectorAdd ( pparams->viewangles, pparams->punchangle, pparams->viewangles );

	// Include client side punch, too
	VectorAdd ( pparams->viewangles, (float *)&ev_punchangle, pparams->viewangles);

	V_DropPunchAngle ( pparams->frametime, (float *)&ev_punchangle );

	// smooth out stair step ups
#if 1
	if ( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0)
	{
		float steptime;

		steptime = pparams->time - lasttime;
		if (steptime < 0)
	//FIXME		I_Error ("steptime < 0");
			steptime = 0;

		oldz += steptime * 150;
		if (oldz > pparams->simorg[2])
			oldz = pparams->simorg[2];
		if (pparams->simorg[2] - oldz > 18)
			oldz = pparams->simorg[2]- 18;
		pparams->vieworg[2] += oldz - pparams->simorg[2];
		view->origin[2] += oldz - pparams->simorg[2];
	}
	else
	{
		oldz = pparams->simorg[2];
	}
#endif

	{
		static float lastorg[3];
		vec3_t delta;

		VectorSubtract( pparams->simorg, lastorg, delta );

		if ( Length( delta ) != 0.0 )
		{
			VectorCopy( pparams->simorg, ViewInterp.Origins[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] );
			ViewInterp.OriginTime[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] = pparams->time;
			ViewInterp.CurrentOrigin++;

			VectorCopy( pparams->simorg, lastorg );
		}
	}

	// Smooth out whole view in multiplayer when on trains, lifts
	if ( cl_vsmoothing && cl_vsmoothing->value &&
		( pparams->smoothing && ( pparams->maxclients > 1 ) ) )
	{
		int foundidx;
		int i;
		float t;

		if ( cl_vsmoothing->value < 0.0 )
		{
			gEngfuncs.Cvar_SetValue( "cl_vsmoothing", 0.0 );
		}

		t = pparams->time - cl_vsmoothing->value;

		for ( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;
			if ( ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] <= t )
				break;
		}

		if ( i < ORIGIN_MASK &&  ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] != 0.0 )
		{
			// Interpolate
			vec3_t delta;
			double frac;
			double dt;
			vec3_t neworg;

			dt = ViewInterp.OriginTime[ (foundidx + 1) & ORIGIN_MASK ] - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ];
			if ( dt > 0.0 )
			{
				frac = ( t - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK] ) / dt;
				frac = min( 1.0, frac );
				VectorSubtract( ViewInterp.Origins[ ( foundidx + 1 ) & ORIGIN_MASK ], ViewInterp.Origins[ foundidx & ORIGIN_MASK ], delta );
				VectorMA( ViewInterp.Origins[ foundidx & ORIGIN_MASK ], frac, delta, neworg );

				// Dont interpolate large changes
				if ( Length( delta ) < 64 )
				{
					VectorSubtract( neworg, pparams->simorg, delta );

					VectorAdd( pparams->simorg, delta, pparams->simorg );
					VectorAdd( pparams->vieworg, delta, pparams->vieworg );
					VectorAdd( view->origin, delta, view->origin );

				}
			}
		}
	}

	// Store off v_angles before munging for third person
	v_angles = pparams->viewangles;
	v_lastAngles = pparams->viewangles;
//	v_cl_angles = pparams->cl_viewangles;	// keep old user mouse angles !
	if ( CL_IsThirdPerson() )
	{
		VectorCopy( camAngles, pparams->viewangles);
		float pitch = camAngles[ 0 ];

		// Normalize angles
		if ( pitch > 180 )
			pitch -= 360.0;
		else if ( pitch < -180 )
			pitch += 360;

		// Player pitch is inverted
		pitch /= -3.0;

		// Slam local player's pitch value
		ent->angles[ 0 ] = pitch;
		ent->curstate.angles[ 0 ] = pitch;
		ent->prevstate.angles[ 0 ] = pitch;
		ent->latched.prevangles[ 0 ] = pitch;
	}

	// override all previous settings if the viewent isn't the client
	if ( pparams->viewentity > pparams->maxclients )
	{
		cl_entity_t *viewentity;
		viewentity = gEngfuncs.GetEntityByIndex( pparams->viewentity );
		if ( viewentity )
		{
			// cof: a ModelAttach message puts the view on an attachment of an entity
			if ( iModelAttachEnt_guess == -1 || iModelAttachNum_guess == -1 )
			{
				VectorCopy( viewentity->origin, pparams->vieworg );
				VectorCopy( viewentity->angles, pparams->viewangles );

				// Store off overridden viewangles
				v_angles = pparams->viewangles;
			}
			else
			{
				VectorCopy( vecModelAttachOrigin_guess, pparams->vieworg );
			}
		}
	}

	lasttime = pparams->time;

	v_origin = pparams->vieworg;

	//LRC
	RenderFog();

	// LRC - override the view position if we're drawing a sky, rather than the player's view
/*	if (gHUD.m_iSkyMode == SKY_ON && pparams->nextView == 0)
	{
		pparams->vieworg[0] = gHUD.m_vecSkyPos.x;
		pparams->vieworg[1] = gHUD.m_vecSkyPos.y;
		pparams->vieworg[2] = gHUD.m_vecSkyPos.z;
		pparams->nextView = 1;
	}*/

	// buz
	g_iWaterLevel = pparams->waterlevel;

	// cof: weapon model bob and lag
	AddViewmodelBob_guess( pparams, view->origin, view->angles, pparams->frametime, gEngfuncs.GetClientTime(), view, false );
	CalcViewModelLag_guess( pparams->frametime, view->origin, view->angles, v_angles, view, false );

	// cof: flashlight in the left hand
	if ( !iPaused_guess && g_iHeadShieldType_guess == 3 && g_iDualWeapon1_guess == 10 && !IsViewEntityActive_guess() && !iBoat_guess && !iWheelchair_guess && !pTelescopeEnt_guess )
	{
		pmtrace_t *tr = gEngfuncs.PM_TraceLine( vecDualAttachment0_guess, vecDualAttachment1_guess, PM_TRACELINE_PHYSENTSONLY, 1, -1 );

		if ( !g_HeadShieldEnt.curstate.rendercolor.r && !g_HeadShieldEnt.curstate.rendercolor.g && !g_HeadShieldEnt.curstate.rendercolor.b &&
			( g_HeadShieldEnt.curstate.renderamt == 255 || !g_HeadShieldEnt.curstate.renderamt ) )
		{
			DynamicLight *dl = MY_AllocDlight( -50 );
			dl->origin = tr->endpos;
			dl->radius = gEngfuncs.pfnRandomLong( -5, 5 ) + 220;
			dl->die = gEngfuncs.GetClientTime() + 0.01;
			dl->color = Vector( 0.4, 0.4, 0.4 );
		}
		else
		{
			float r = g_HeadShieldEnt.curstate.rendercolor.r / 255;
			float g = g_HeadShieldEnt.curstate.rendercolor.g / 255;
			float b = g_HeadShieldEnt.curstate.rendercolor.b / 255;

			DynamicLight *dl = MY_AllocDlight( -50 );
			dl->origin = tr->endpos;
			dl->radius = gEngfuncs.pfnRandomLong( -5, 5 ) + g_HeadShieldEnt.curstate.renderamt;
			dl->die = gEngfuncs.GetClientTime() + 0.01;
			dl->color = Vector( r, g, b );
		}
	}

	if ( view->model && !iPaused_guess )
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		WEAPON *pWeapon = gHUD.m_Ammo.m_pWeapon;

		// cof: flashlight in the right hand
		if ( !iBoat_guess && pWeapon && !iWheelchair_guess && !pTelescopeEnt_guess && !IsViewEntityActive_guess() && player &&
			( pWeapon->iId == 10 || g_iDualWeapon2_guess == 10 ) )
		{
			pmtrace_t *tr = gEngfuncs.PM_TraceLine( view->attachment[0], view->attachment[1], PM_TRACELINE_PHYSENTSONLY, 1, -1 );

			if ( !view->curstate.rendercolor.r && !view->curstate.rendercolor.g && !view->curstate.rendercolor.b &&
				( view->curstate.renderamt == 255 || !view->curstate.renderamt ) )
			{
				DynamicLight *dl = MY_AllocDlight( ent->index );
				dl->origin = tr->endpos;
				dl->radius = gEngfuncs.pfnRandomLong( -5, 5 ) + 220;
				dl->die = gEngfuncs.GetClientTime() + 0.01;
				dl->color = Vector( 0.4, 0.4, 0.4 );
			}
			else
			{
				float r = view->curstate.rendercolor.r / 255;
				float g = view->curstate.rendercolor.g / 255;
				float b = view->curstate.rendercolor.b / 255;

				DynamicLight *dl = MY_AllocDlight( view->index );
				dl->origin = tr->endpos;
				dl->radius = gEngfuncs.pfnRandomLong( -5, 5 ) + view->curstate.renderamt;
				dl->die = gEngfuncs.GetClientTime() + 0.01;
				dl->color = Vector( r, g, b );
			}
		}

		// cof: muzzle light
		if ( !iPaused_guess && !iBoat_guess && !iWheelchair_guess && !pTelescopeEnt_guess && !IsViewEntityActive_guess() && player &&
			( gHUD.m_iFlashFlags_guess & 16 ) && pWeapon && ( pWeapon->iId == 13 || pWeapon->iId == 25 || g_iDualWeapon2_guess == 13 ) )
		{
			DynamicLight *dl = MY_AllocDlight( ent->index );

			float flRed = gEngfuncs.pfnRandomFloat( -0.5, 0.5 );
			float flGreen = gEngfuncs.pfnRandomFloat( -0.1, 0.05 );

			dl->origin = view->attachment[0];
			dl->radius = gEngfuncs.pfnRandomLong( -15, 15 ) + 220;
			dl->die = gEngfuncs.GetClientTime() + 0.01;

			if ( pWeapon->iId == 25 )
			{
				dl->color = Vector( flRed + 1.15f, flGreen + 0.6f, flGreen + 0.1f );
			}
			else
			{
				dl->color = Vector( flRed + 1.15f, flGreen + 0.2f, flGreen + 0.1f );
				MuzzleFlash_guess( view, 0, true, 0.3 );
			}
		}
	}

	// cof: green light around the player
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	bool bCamera = false;
	if ( pAttachCamEnt_guess )
		bCamera = !iAttachCamFlag_guess;

	if ( !iPaused_guess && !bCamera && !iBoat_guess && !iWheelchair_guess && !pTelescopeEnt_guess && !IsViewEntityActive_guess() && player &&
		( gHUD.m_iFlashFlags_guess & 64 ) )
	{
		DynamicLight *dl = MY_AllocDlight( 0 );
		dl->origin = player->origin;
		dl->radius = 900;
		dl->color.x = 0.1;
		dl->color.y = 0.9;
		dl->color.z = 0.1;
		dl->die = pparams->time + 0.01;
	}

	// cof: maglite
	cl_entity_t *viewmodel = gEngfuncs.GetViewModel();

	if ( viewmodel && gHUD.m_iMaglite_guess && ( ( gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId == 1 ) || g_iDualWeapon2_guess == 1 ) )
	{
		cl_entity_t *localplayer = gEngfuncs.GetLocalPlayer();
		pmtrace_t *tr = gEngfuncs.PM_TraceLine( viewmodel->attachment[0], viewmodel->attachment[1], PM_TRACELINE_PHYSENTSONLY, 1, -1 );

		if ( !iBoat_guess && !iWheelchair_guess && !pTelescopeEnt_guess && !IsViewEntityActive_guess() && localplayer && tr->endpos && gEngfuncs.GetViewModel() )
		{
			vec3_t dir;
			VectorSubtract( viewmodel->attachment[1], viewmodel->attachment[0], dir );
			VectorAngles( dir, dir );
			dir[0] = -dir[0];

			CreateMagliteLight_guess( tr->endpos, dir, pparams->time, pparams->frametime );
		}
	}

	SetupFlashlight( view->attachment[1], pparams->viewangles, pparams->time, pparams->frametime ); // buz
	SetupViewModelLight_guess( view->attachment[1], pparams->viewangles, pparams->time, pparams->frametime );

	// cof: the left hand weapon follows the weapon model
	if ( g_iHeadShieldType_guess == 3 )
	{
		VectorCopy( view->origin, g_HeadShieldEnt.origin );
		VectorCopy( view->origin, g_HeadShieldEnt.curstate.origin );
		VectorCopy( view->angles, g_HeadShieldEnt.angles );
		VectorCopy( view->angles, g_HeadShieldEnt.curstate.angles );

		AddViewmodelBob_guess( pparams, g_HeadShieldEnt.origin, g_HeadShieldEnt.angles, pparams->frametime, gEngfuncs.GetClientTime(), &g_HeadShieldEnt, true );
		CalcViewModelLag_guess( pparams->frametime, g_HeadShieldEnt.origin, g_HeadShieldEnt.angles, g_HeadShieldEnt.angles, &g_HeadShieldEnt, true );
	}
}

void V_SmoothInterpolateAngles( float * startAngle, float * endAngle, float * finalAngle, float degreesPerSec )
{
	float absd,frac,d,threshhold;
	
	NormalizeAngles( startAngle );
	NormalizeAngles( endAngle );

	for ( int i = 0 ; i < 3 ; i++ )
	{
		d = endAngle[i] - startAngle[i];

		if ( d > 180.0f )
		{
			d -= 360.0f;
		}
		else if ( d < -180.0f )
		{	
			d += 360.0f;
		}

		absd = fabsf(d);

		if ( absd > 0.01f )
		{
			frac = degreesPerSec * v_frametime;

			threshhold= degreesPerSec / 4;

			if ( absd < threshhold )
			{
				float h = absd / threshhold;
				h *= h;
				frac*= h;  // slow down last degrees
			}

			if ( frac >  absd )
			{
				finalAngle[i] = endAngle[i];
			}
			else
			{
				if ( d>0)
					finalAngle[i] = startAngle[i] + frac;
				else
					finalAngle[i] = startAngle[i] - frac;
			}
		}
		else
		{
			finalAngle[i] = endAngle[i];
		}

	}

	NormalizeAngles( finalAngle );
}

// Get the origin of the Observer based around the target's position and angles
void V_GetChaseOrigin( float * angles, float * origin, float distance, float * returnvec )
{
	vec3_t	vecEnd;
	vec3_t	forward;
	vec3_t	vecStart;
	pmtrace_t * trace;
	int maxLoops = 8;

	int ignoreent = -1;	// first, ignore no entity
	
	cl_entity_t	 *	ent = NULL;
	
	// Trace back from the target using the player's view angles
	AngleVectors(angles, forward, NULL, NULL);
	
	VectorScale(forward,-1,forward);

	VectorCopy( origin, vecStart );

	VectorMA(vecStart, distance , forward, vecEnd);

	while ( maxLoops > 0)
	{
		trace = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent );

		// WARNING! trace->ent is is the number in physent list not the normal entity number

		if ( trace->ent <= 0)
			break;	// we hit the world or nothing, stop trace

		ent = gEngfuncs.GetEntityByIndex( PM_GetPhysEntInfo( trace->ent ) );

		if ( ent == NULL )
			break;

		// hit non-player solid BSP , stop here
		if ( ent->curstate.solid == SOLID_BSP && !ent->player ) 
			break;

		// if close enought to end pos, stop, otherwise continue trace
		if( Distance(trace->endpos, vecEnd ) < 1.0f )
		{
			break;
		}
		else
		{
			ignoreent = trace->ent;	// ignore last hit entity
			VectorCopy( trace->endpos, vecStart);
		}

		maxLoops--;
	}  

/*	if ( ent )
	{
		gEngfuncs.Con_Printf("Trace loops %i , entity %i, model %s, solid %i\n",(8-maxLoops),ent->curstate.number, ent->model->name , ent->curstate.solid ); 
	} */

	VectorMA( trace->endpos, 4, trace->plane.normal, returnvec );

	v_lastDistance = Distance(trace->endpos, origin);	// real distance without offset
}

/*void V_GetDeathCam(cl_entity_t * ent1, cl_entity_t * ent2, float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; 

	float distance = 168.0f;

	v_lastDistance+= v_frametime * 96.0f;	// move unit per seconds back

	if ( v_resetCamera )
		v_lastDistance = 64.0f;

	if ( distance > v_lastDistance )
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
		newOrigin[2]+= 17; // head level of living player

	// get new angle towards second target
	if ( ent2 )
	{
		VectorSubtract( ent2->origin, ent1->origin, newAngle );
		VectorAngles( newAngle, newAngle );
		newAngle[0] = -newAngle[0];
	}
	else
	{
		// if no second target is given, look down to dead player
		newAngle[0] = 90.0f;
		newAngle[1] = 0.0f;
		newAngle[2] = 0;
	}

	// and smooth view
	V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 120.0f );
			
	V_GetChaseOrigin( angle, newOrigin, distance, origin );

	VectorCopy(angle, v_lastAngles);
}*/

void V_GetSingleTargetCam(cl_entity_t * ent1, float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; 
	
	int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

	// see is target is a dead player
	qboolean deadPlayer = ent1->player && (ent1->curstate.solid == SOLID_NOT);
	
	float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

	float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
	// go away in final scenes or if player just died
	if ( flags & DRC_FLAG_FINAL )
		distance*=2.0f;	
	else if ( deadPlayer )
		distance*=1.5f;	

	// let v_lastDistance float smoothly away
	v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

	if ( distance > v_lastDistance )
		distance = v_lastDistance;
	
	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
	{
		if ( deadPlayer )  
			newOrigin[2]+= 2;	//laying on ground
		else
			newOrigin[2]+= 17; // head level of living player
			
	}
	else
		newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

	// we have no second target, choose view direction based on
	// show front of primary target
	VectorCopy(ent1->angles, newAngle);

	// show dead players from front, normal players back
	if ( flags & DRC_FLAG_FACEPLAYER )
		newAngle[1]+= 180.0f;


	newAngle[0]+= 12.5f * dfactor; // lower angle if dramatic

	// if final scene (bomb), show from real high pos
	if ( flags & DRC_FLAG_FINAL )
		newAngle[0] = 22.5f; 

	// choose side of object/player			
	if ( flags & DRC_FLAG_SIDE )
		newAngle[1]+=22.5f;
	else
		newAngle[1]-=22.5f;

	V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 120.0f );

	// HACK, if player is dead don't clip against his dead body, can't check this
	V_GetChaseOrigin( angle, newOrigin, distance, origin );
}

float MaxAngleBetweenAngles(  float * a1, float * a2 )
{
	float d, maxd = 0.0f;

	NormalizeAngles( a1 );
	NormalizeAngles( a2 );

	for ( int i = 0 ; i < 3 ; i++ )
	{
		d = a2[i] - a1[i];
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		d = fabsf(d);

		if ( d > maxd )
			maxd=d;
	}

	return maxd;
}

void V_GetDoubleTargetsCam(cl_entity_t	 * ent1, cl_entity_t * ent2,float * angle, float * origin)
{
	float newAngle[3]; float newOrigin[3]; float tempVec[3];

	int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

	float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

	float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
	// go away in final scenes or if player just died
	if ( flags & DRC_FLAG_FINAL )
		distance*=2.0f;	
	
	// let v_lastDistance float smoothly away
	v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

	if ( distance > v_lastDistance )
		distance = v_lastDistance;

	VectorCopy(ent1->origin, newOrigin);

	if ( ent1->player )
		newOrigin[2]+= 17; // head level of living player
	else
		newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

	// get new angle towards second target
	VectorSubtract( ent2->origin, ent1->origin, newAngle );

	VectorAngles( newAngle, newAngle );
	newAngle[0] = -newAngle[0];

	// set angle diffrent in Dramtaic scenes
	newAngle[0]+= 12.5f * dfactor; // lower angle if dramatic
			
	if ( flags & DRC_FLAG_SIDE )
		newAngle[1]+=22.5f;
	else
		newAngle[1]-=22.5f;

	float d = MaxAngleBetweenAngles( v_lastAngles, newAngle );

	if ( ( d < v_cameraFocusAngle) && ( v_cameraMode == CAM_MODE_RELAX ) )
	{
		// difference is to small and we are in relax camera mode, keep viewangles
		VectorCopy(v_lastAngles, newAngle );
	}
	else if ( (d < v_cameraRelaxAngle) && (v_cameraMode == CAM_MODE_FOCUS) )
	{
		// we catched up with our target, relax again
		v_cameraMode = CAM_MODE_RELAX;
	}
	else
	{
		// target move too far away, focus camera again
		v_cameraMode = CAM_MODE_FOCUS;
	}

	// and smooth view, if not a scene cut
	if ( v_resetCamera || (v_cameraMode == CAM_MODE_RELAX) )
	{
		VectorCopy( newAngle, angle );
	}
	else
	{
		V_SmoothInterpolateAngles( v_lastAngles, newAngle, angle, 180.0f );
	}

	V_GetChaseOrigin( newAngle, newOrigin, distance, origin );

	// move position up, if very close at target
	if ( v_lastDistance < 64.0f )
		origin[2]+= 16.0f*( 1.0f - (v_lastDistance / 64.0f ) );

	// calculate angle to second target
	VectorSubtract( ent2->origin, origin, tempVec );
	VectorAngles( tempVec, tempVec );
	tempVec[0] = -tempVec[0];

	/* take middle between two viewangles
	InterpolateAngles( newAngle, tempVec, newAngle, 0.5f); */
	
	

}

void V_GetDirectedChasePosition(cl_entity_t	 * ent1, cl_entity_t * ent2,float * angle, float * origin)
{

	if ( v_resetCamera )
	{
		v_lastDistance = 4096.0f;
		// v_cameraMode = CAM_MODE_FOCUS;
	}

	if ( ( ent2 == (cl_entity_t*)0xFFFFFFFF ) || ( ent1->player && (ent1->curstate.solid == SOLID_NOT) ) )
	{
		// we have no second target or player just died
		V_GetSingleTargetCam(ent1, angle, origin);
	}
	else if ( ent2 )
	{
		// keep both target in view
		V_GetDoubleTargetsCam( ent1, ent2, angle, origin );
	}
	else
	{
		// second target disappeard somehow (dead)

		// keep last good viewangle
		float newOrigin[3];

		int flags 	   = gHUD.m_Spectator.m_iObserverFlags;

		float dfactor   = ( flags & DRC_FLAG_DRAMATIC )? -1.0f : 1.0f;

		float distance = 112.0f + ( 16.0f * dfactor ); // get close if dramatic;
	
		// go away in final scenes or if player just died
		if ( flags & DRC_FLAG_FINAL )
			distance*=2.0f;	
	
		// let v_lastDistance float smoothly away
		v_lastDistance+= v_frametime * 32.0f;	// move unit per seconds back

		if ( distance > v_lastDistance )
			distance = v_lastDistance;
		
		VectorCopy(ent1->origin, newOrigin);

		if ( ent1->player )
			newOrigin[2]+= 17; // head level of living player
		else
			newOrigin[2]+= 8;	// object, tricky, must be above bomb in CS

		V_GetChaseOrigin( angle, newOrigin, distance, origin );
	}

	VectorCopy(angle, v_lastAngles);
}

void V_GetChasePos(int target, float * cl_angles, float * origin, float * angles)
{
	cl_entity_t	 *	ent = NULL;
	
	if ( target ) 
	{
		ent = gEngfuncs.GetEntityByIndex( target );
	};
	
	if (!ent)
	{
		// just copy a save in-map position
		VectorCopy ( vJumpAngles, angles );
		VectorCopy ( vJumpOrigin, origin );
		return;
	}
	
	
	
	if ( gHUD.m_Spectator.m_autoDirector->value )
	{
		if ( g_iUser3 )
			V_GetDirectedChasePosition( ent, gEngfuncs.GetEntityByIndex( g_iUser3 ),
				angles, origin );
		else
			V_GetDirectedChasePosition( ent, ( cl_entity_t*)0xFFFFFFFF,
				angles, origin );
	}
	else
	{
		if ( cl_angles == NULL )	// no mouse angles given, use entity angles ( locked mode )
		{
			VectorCopy ( ent->angles, angles);
			angles[0]*=-1;
		}
		else
			VectorCopy ( cl_angles, angles);


		VectorCopy ( ent->origin, origin);
		
		origin[2]+= 28; // DEFAULT_VIEWHEIGHT - some offset

		V_GetChaseOrigin( angles, origin, cl_chasedist->value, origin );
	}

	v_resetCamera = false;	
}

void V_ResetChaseCam()
{
	v_resetCamera = true;
}


void V_GetInEyePos(int target, float * origin, float * angles )
{
	if ( !target)
	{
		// just copy a save in-map position
		VectorCopy ( vJumpAngles, angles );
		VectorCopy ( vJumpOrigin, origin );
		return;
	};


	cl_entity_t	 * ent = gEngfuncs.GetEntityByIndex( target );

	if ( !ent )
		return;

	VectorCopy ( ent->origin, origin );
	VectorCopy ( ent->angles, angles );

	angles[PITCH]*=-3.0f;	// see CL_ProcessEntityUpdate()

	if ( ent->curstate.solid == SOLID_NOT )
	{
		angles[ROLL] = 80;	// dead view angle
		origin[2]+= -8 ; // PM_DEAD_VIEWHEIGHT
	}
	else if (ent->curstate.usehull == 1 )
		origin[2]+= 12; // VEC_DUCK_VIEW;
	else
		// exacty eye position can't be caluculated since it depends on
		// client values like cl_bobcycle, this offset matches the default values
		origin[2]+= 28; // DEFAULT_VIEWHEIGHT
}

void V_GetMapFreePosition( float * cl_angles, float * origin, float * angles )
{
	vec3_t forward;
	vec3_t zScaledTarget;

	VectorCopy(cl_angles, angles);

	// modify angles since we don't wanna see map's bottom
	angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);

	zScaledTarget[0] = gHUD.m_Spectator.m_mapOrigin[0];
	zScaledTarget[1] = gHUD.m_Spectator.m_mapOrigin[1];
	zScaledTarget[2] = gHUD.m_Spectator.m_mapOrigin[2] * (( 90.0f - angles[0] ) / 90.0f );
	

	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(zScaledTarget, -( 4096.0f / gHUD.m_Spectator.m_mapZoom ), forward , origin);
}

void V_GetMapChasePosition(int target, float * cl_angles, float * origin, float * angles)
{
	vec3_t forward;

	if ( target )
	{
		cl_entity_t	 *	ent = gEngfuncs.GetEntityByIndex( target );

		if ( gHUD.m_Spectator.m_autoDirector->value )
		{
			// this is done to get the angles made by director mode
			V_GetChasePos(target, cl_angles, origin, angles);
			VectorCopy(ent->origin, origin);
			
			// keep fix chase angle horizontal
			angles[0] = 45.0f;
		}
		else
		{
			VectorCopy(cl_angles, angles);
			VectorCopy(ent->origin, origin);

			// modify angles since we don't wanna see map's bottom
			angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);
		}
	}
	else
	{
		// keep out roaming position, but modify angles
		VectorCopy(cl_angles, angles);
		angles[0] = 51.25f + 38.75f*(angles[0]/90.0f);
	}

	origin[2] *= (( 90.0f - angles[0] ) / 90.0f );
	angles[2] = 0.0f;	// don't roll angle (if chased player is dead)

	AngleVectors(angles, forward, NULL, NULL);

	VectorNormalize(forward);

	VectorMA(origin, -1536, forward, origin); 
}

int V_FindViewModelByWeaponModel(int weaponindex)
{

	// Cry of Fear maps only the flashlight (1.6a data)
	static char * modelmap[][2] =	{
		{ "models/weapons/flashlight/p_flashlight.mdl",	"models/weapons/flashlight/v_flashlight.mdl" },
		{ NULL, NULL } };

	struct model_s * weaponModel = IEngineStudio.GetModelByIndex( weaponindex );

	if ( weaponModel )
	{
		int len = strlen( weaponModel->name );
		int i = 0;

		while ( modelmap[i] != NULL )
		{
			if ( !strnicmp( weaponModel->name, modelmap[i][0], len ) )
			{
				return gEngfuncs.pEventAPI->EV_FindModelIndex( modelmap[i][1] );
			}
			i++;
		}

		return 0;
	}
	else
		return 0;

}


/*
==================
V_CalcSpectatorRefdef

==================
*/
void V_CalcSpectatorRefdef ( struct ref_params_s * pparams )
{
	static vec3_t			velocity ( 0.0f, 0.0f, 0.0f);

	static int lastWeaponModelIndex = 0;
	static int lastViewModelIndex = 0;
		
	cl_entity_t	 * ent = gEngfuncs.GetEntityByIndex( g_iUser2 );
	
	pparams->onlyClientDraw = false;

	// refresh position
	VectorCopy ( pparams->simorg, v_sim_org );

	// get old values
	VectorCopy ( pparams->cl_viewangles, v_cl_angles );
	VectorCopy ( pparams->viewangles, v_angles );
	VectorCopy ( pparams->vieworg, v_origin );

	if (  ( g_iUser1 == OBS_IN_EYE || gHUD.m_Spectator.m_pip->value == INSET_IN_EYE ) && ent )
	{
		// calculate player velocity
		float timeDiff = ent->curstate.msg_time - ent->prevstate.msg_time;

		if ( timeDiff > 0 )
		{
			vec3_t distance;
			VectorSubtract(ent->prevstate.origin, ent->curstate.origin, distance);
			VectorScale(distance, 1/timeDiff, distance );

			velocity[0] = velocity[0]*0.9f + distance[0]*0.1f;
			velocity[1] = velocity[1]*0.9f + distance[1]*0.1f;
			velocity[2] = velocity[2]*0.9f + distance[2]*0.1f;
			
			VectorCopy(velocity, pparams->simvel);
		}

		// predict missing client data and set weapon model ( in HLTV mode or inset in eye mode )
		if ( gEngfuncs.IsSpectateOnly() )
		{
			V_GetInEyePos( g_iUser2, pparams->simorg, pparams->cl_viewangles );

			pparams->health = 1;

			cl_entity_t	 * gunModel = gEngfuncs.GetViewModel();

			if ( lastWeaponModelIndex != ent->curstate.weaponmodel )
			{
				// weapon model changed

				lastWeaponModelIndex = ent->curstate.weaponmodel;
				lastViewModelIndex = V_FindViewModelByWeaponModel( lastWeaponModelIndex );
				if ( lastViewModelIndex )
				{
					gEngfuncs.pfnWeaponAnim(0,0);	// reset weapon animation
				}
				else
				{
					// model not found
					gunModel->model = NULL;	// disable weapon model
					lastWeaponModelIndex = lastViewModelIndex = 0;
				}
			}

			if ( lastViewModelIndex )
			{
				gunModel->model = IEngineStudio.GetModelByIndex( lastViewModelIndex );
				gunModel->curstate.modelindex = lastViewModelIndex;
				gunModel->curstate.frame = 0;
				gunModel->curstate.colormap = 0; 
				gunModel->index = g_iUser2;
			}
			else
			{
				gunModel->model = NULL;	// disable weaopn model
			}
		}
		else
		{
			// only get viewangles from entity
			VectorCopy ( ent->angles, pparams->cl_viewangles );
			pparams->cl_viewangles[PITCH]*=-3.0f;	// see CL_ProcessEntityUpdate()
		}
	}

	v_frametime = pparams->frametime;

	if ( pparams->nextView == 0 )
	{
		// first renderer cycle, full screen

		switch ( g_iUser1 )
		{
			case OBS_CHASE_LOCKED:	V_GetChasePos( g_iUser2, NULL, v_origin, v_angles );
									break;

			case OBS_CHASE_FREE:	V_GetChasePos( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;

			case OBS_ROAMING	:	VectorCopy (v_cl_angles, v_angles);
									VectorCopy (v_sim_org, v_origin);
									break;

			case OBS_IN_EYE		:   V_CalcNormalRefdef ( pparams );
									break;
				
			case OBS_MAP_FREE  :	pparams->onlyClientDraw = true;
									V_GetMapFreePosition( v_cl_angles, v_origin, v_angles );
									break;

			case OBS_MAP_CHASE  :	pparams->onlyClientDraw = true;
									V_GetMapChasePosition( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;
		}

		if ( gHUD.m_Spectator.m_pip->value )
			pparams->nextView = 1;	// force a second renderer view

		gHUD.m_Spectator.m_iDrawCycle = 0;

	}
	else
	{
		// second renderer cycle, inset window

		// set inset parameters
		pparams->viewport[0] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);	// change viewport to inset window
		pparams->viewport[1] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowY);
		pparams->viewport[2] = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth);
		pparams->viewport[3] = YRES(gHUD.m_Spectator.m_OverviewData.insetWindowHeight);
		pparams->nextView	 = 0;	// on further view

		// override some settings in certain modes
		switch ( (int)gHUD.m_Spectator.m_pip->value )
		{
			case INSET_CHASE_FREE : V_GetChasePos( g_iUser2, v_cl_angles, v_origin, v_angles );
									break;	

			case INSET_IN_EYE	 :	V_CalcNormalRefdef ( pparams );
									break;

			case INSET_MAP_FREE  :	pparams->onlyClientDraw = true;
									V_GetMapFreePosition( v_cl_angles, v_origin, v_angles );
									break;

			case INSET_MAP_CHASE  :	pparams->onlyClientDraw = true;

									if ( g_iUser1 == OBS_ROAMING )
										V_GetMapChasePosition( 0, v_cl_angles, v_origin, v_angles );
									else
										V_GetMapChasePosition( g_iUser2, v_cl_angles, v_origin, v_angles );

									break;
		}

		gHUD.m_Spectator.m_iDrawCycle = 1;
	}

	// write back new values into pparams
	VectorCopy ( v_cl_angles, pparams->cl_viewangles );
	VectorCopy ( v_angles, pparams->viewangles )
	VectorCopy ( v_origin, pparams->vieworg );

}


cvar_t *cl_forcepause;

// Cry of Fear: an empty function nothing calls (retail 100B3330, the slot
// just before V_CalcRefdef)
extern "C" void V_CalcEmpty_guess( void )
{
}

void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams )
{
 CoFR_PublishCrouch(pparams);
	// intermission / finale rendering
/*	if ( pparams->intermission )
	{	
		V_CalcIntermissionRefdef ( pparams );	
	}
	else if ( pparams->spectator || g_iUser1 )	// g_iUser true if in spectator mode
	{
		V_CalcSpectatorRefdef ( pparams );	
	}
	else if ( !pparams->paused )
	{
		V_CalcNormalRefdef ( pparams );
	}*/

/*	if (pparams->intermission)
		CONPRINT("=|=|=|=| intermission!\n");

	if (pparams->paused)
		CONPRINT("=|=|=|=| paused!\n");
	
	if (pparams->spectator)
		CONPRINT("=|=|=|=| paused!\n");*/

	if ( pparams->paused )
	{
		if ( iPaused_guess != 1 )
			iPaused_guess = 1;

		if ( !IrrKlangIsPaused_guess() )
			IrrKlangSetPaused_guess( true );
	}
	else
	{
		iPaused_guess = 0;

		if ( IrrKlangIsPaused_guess() )
			IrrKlangSetPaused_guess( false );
	}

	V_CalcNormalRefdef ( pparams );

	// buz
	render_origin = pparams->vieworg;
	render_angles = pparams->viewangles;
	AngleVectors(render_angles, g_vecForward, g_vecRight, g_vecUp); // for XaeroX's sprite rendering..

	RendererCalcRefDef( pparams ); // buz

//	if (cl_forcepause->value && !pparams->paused) ClientCmd("pause");

/*
// Example of how to overlay the whole screen with red at 50 % alpha
#define SF_TEST
#if defined SF_TEST
	{
		screenfade_t sf;
		gEngfuncs.pfnGetScreenFade( &sf );

		sf.fader = 255;
		sf.fadeg = 0;
		sf.fadeb = 0;
		sf.fadealpha = 128;
		sf.fadeFlags = FFADE_STAYOUT | FFADE_OUT;

		gEngfuncs.pfnSetScreenFade( &sf );
	}
#endif
*/

	CoFR_PublishView(pparams->vieworg,pparams->viewangles);
}

/*
=============
V_DropPunchAngle

=============
*/
#define PUNCH_DAMPING		9.0f		// bigger number makes the response more damped, smaller is less damped
#define PUNCH_SPRING_CONSTANT	65.0f	// bigger number increases the speed at which the view corrects

#define clamp( val, min, max ) ( ((val) > (max)) ? (max) : ( ((val) < (min)) ? (min) : (val) ) )

void V_DropPunchAngle ( float frametime, float *ev_punchangle )
{
	float	len;
	float	damping;
	float	springForceMagnitude;

	if ( gEngfuncs.GetMaxClients() <= 1 )
	{
		len = VectorNormalize ( ev_punchangle );
		len -= (10.0 + len * 0.5) * frametime;
		len = max( len, 0.0 );
		VectorScale ( ev_punchangle, len, ev_punchangle );
		return;
	}

	if ( Length( ev_punchangle ) > 0.001 || Length( ev_punchanglevel_guess ) > 0.001 )
	{
		VectorMA( ev_punchangle, frametime, ev_punchanglevel_guess, ev_punchangle );
		damping = 1 - (PUNCH_DAMPING * frametime);

		if ( damping < 0 )
		{
			damping = 0;
		}
		VectorScale( ev_punchanglevel_guess, damping, ev_punchanglevel_guess );

		// torsional spring
		springForceMagnitude = PUNCH_SPRING_CONSTANT * frametime;
		springForceMagnitude = clamp( springForceMagnitude, 0, 2 );

		VectorMA( ev_punchanglevel_guess, -springForceMagnitude, ev_punchangle, ev_punchanglevel_guess );

		// don't wrap around
		ev_punchangle[0] = clamp( ev_punchangle[0], -89, 89 );
		ev_punchangle[1] = clamp( ev_punchangle[1], -179, 179 );
		ev_punchangle[2] = clamp( ev_punchangle[2], -89, 89 );
	}
}

/*
=============
V_PunchAxis

Client side punch effect
=============
*/
void V_PunchAxis( int axis, float punch )
{
//	ev_punchangle[ axis ] = punch; // buz - disable
}

/*
=============
V_Init
=============
*/
extern void CAM_ToFirstPerson( void );
extern void CAM_ToThirdPerson( void );
extern cl_entity_t *pTelescopeEnt_guess;
extern cl_entity_t *pAttachCamEnt_guess;

int iModelAttachEnt_guess = -1;
int iModelAttachNum_guess = -1;

int __MsgFunc_ModelAttach_guess( const char *pszName, int iSize, void *pbuf );
int __MsgFunc_ThirdPerson_guess( const char *pszName, int iSize, void *pbuf );
int __MsgFunc_CoopCamAnim_guess( const char *pszName, int iSize, void *pbuf );

void V_Init (void)
{
	gEngfuncs.pfnAddCommand ("centerview", V_StartPitchDrift );

	scr_ofsx			= gEngfuncs.pfnRegisterVariable( "scr_ofsx","0", 0 );
	scr_ofsy			= gEngfuncs.pfnRegisterVariable( "scr_ofsy","0", 0 );
	scr_ofsz			= gEngfuncs.pfnRegisterVariable( "scr_ofsz","0", 0 );

	v_centermove		= gEngfuncs.pfnRegisterVariable( "v_centermove", "0.15", 0 );
	v_centerspeed		= gEngfuncs.pfnRegisterVariable( "v_centerspeed","500", 0 );

	cl_bobcycle			= gEngfuncs.pfnRegisterVariable( "cl_bobcycle","0.8", 0 );// best default for my experimental gun wag (sjb)
	cl_bob				= gEngfuncs.pfnRegisterVariable( "cl_bob","0.01", 0 );// best default for my experimental gun wag (sjb)
	cl_bobup			= gEngfuncs.pfnRegisterVariable( "cl_bobup","0.5", 0 );
	cl_waterdist		= gEngfuncs.pfnRegisterVariable( "cl_waterdist","4", 0 );
	cl_chasedist		= gEngfuncs.pfnRegisterVariable( "cl_chasedist","112", 0 );
	cl_forcepause		= gEngfuncs.pfnRegisterVariable( "cl_forcepause","0", 0 ); // buz test

	gEngfuncs.pfnHookUserMsg( "ModelAttach", __MsgFunc_ModelAttach_guess );
	gEngfuncs.pfnHookUserMsg( "ThirdPerson", __MsgFunc_ThirdPerson_guess );
	gEngfuncs.pfnHookUserMsg( "CoopCamAnim", __MsgFunc_CoopCamAnim_guess );
}

int __MsgFunc_ModelAttach_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	iModelAttachEnt_guess = READ_LONG();
	iModelAttachNum_guess = READ_BYTE();

	return 1;
}

int __MsgFunc_ThirdPerson_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int iThirdPerson = READ_BYTE();
	int iEntity = READ_SHORT();

	if ( iEntity == 255 )
		pCamTargetEnt_guess = NULL;
	else
		pCamTargetEnt_guess = gEngfuncs.GetEntityByIndex( iEntity );

	if ( iThirdPerson )
		CAM_ToThirdPerson();
	else
		CAM_ToFirstPerson();

	return 1;
}

int __MsgFunc_CoopCamAnim_guess( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	if ( pAttachCamEnt_guess )
	{
		int iSequence = READ_BYTE();

		if ( iSequence >= 0 )
		{
			pAttachCamEnt_guess->curstate.animtime = gEngfuncs.GetClientTime();
			pAttachCamEnt_guess->curstate.sequence = iSequence;
		}
	}

	return 1;
}

extern int g_iDualEntIndex_guess;

cl_entity_t *GetDualEntity_guess( void )
{
	return gEngfuncs.GetEntityByIndex( g_iDualEntIndex_guess );
}

void AngleNormalize360_guess( float pitch, float yaw )
{
	while ( pitch > 360 )
		pitch -= 360;
	while ( pitch < 0 )
		pitch += 360;

	while ( yaw > 360 )
		yaw -= 360;
	while ( yaw < 0 )
		yaw += 360;
}

inline bool IsFinite_guess( float f )
{
	return ( *(unsigned long *)&f & 0x7F800000 ) != 0x7F800000;
}

bool VectorIsValid_guess( float x, float y, float z )
{
	return IsFinite_guess( x ) && IsFinite_guess( y ) && IsFinite_guess( z );
}

void PunchAngleDecay_guess( Vector punch, struct ref_params_s *pparams )
{
	float	damping;
	float	springForceMagnitude;

	if ( Length( punch ) > 0.001 || Length( ev_punchanglevel_guess ) > 0.001 )
	{
		VectorMA( punch, pparams->frametime, ev_punchanglevel_guess, punch );
		damping = 1 - (PUNCH_DAMPING * pparams->frametime);

		if ( damping < 0 )
		{
			damping = 0;
		}
		VectorScale( ev_punchanglevel_guess, damping, ev_punchanglevel_guess );

		// torsional spring
		springForceMagnitude = PUNCH_SPRING_CONSTANT * pparams->frametime;
		springForceMagnitude = clamp( springForceMagnitude, 0, 2 );

		VectorMA( ev_punchanglevel_guess, -springForceMagnitude, punch, ev_punchanglevel_guess );

		// don't wrap around
		ev_punchangle[0] = clamp( ev_punchangle[0], -89, 89 );
		ev_punchangle[1] = clamp( ev_punchangle[1], -179, 179 );
		ev_punchangle[2] = clamp( ev_punchangle[2], -89, 89 );
	}
}

void PunchViewAngles_guess( float x, float y, float z )
{
	if ( !pAttachCamEnt_guess || iAttachCamFlag_guess > 0 )
	{
		ev_punchanglevel_guess[0] -= x * 20;
		ev_punchanglevel_guess[1] += y * 20;
		ev_punchanglevel_guess[2] += z * 20;
	}
}

//===============================
// view model lag and bob
//===============================
#define HL2_BOB_CYCLE_MAX	0.45f
#define HL2_BOB_UP			0.5f

extern float flWeaponIdleSpeed_guess;
extern float flWeaponIdleDampener_guess;
extern float flGunParam1_guess;
extern float flSprintFlag_guess;
extern int g_iHeadShieldType_guess;
extern int iAttachCamFlag_guess;

float g_fMaxViewModelLag_guess = 1.5f;
Vector vecLastFacing_guess;

float g_verticalBob_guess;
float g_lateralBob_guess;
float flWeaponLower_guess;
float flUnknown1055959C_guess;

int TestBobMode_guess( struct ref_params_s *pparams )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	return 0;
}

float CalcViewmodelBob_guess( struct ref_params_s *pparams, float frametime, float curtime )
{
	static	float bobtime;
	static	float lastbobtime;
	float	cycle;
	float	speed;

	if ( flUnknown1055959C_guess > 5 )
		flUnknown1055959C_guess = 5;
	else if ( flUnknown1055959C_guess < 0 )
		flUnknown1055959C_guess = 0;

	cl_entity_t *player = gEngfuncs.GetLocalPlayer();

	if ( ( !frametime ) || ( player == NULL ) )
	{
		return 0.0f;
	}

	if ( TestBobMode_guess( pparams ) == 1 || gHUD.m_iLadder_guess || gHUD.m_iFallOver_guess || iAttachCamFlag_guess )
	{
		if ( gHUD.m_iLadder_guess || gHUD.m_iFallOver_guess || iAttachCamFlag_guess )
		{
			if ( gViewPort && gViewPort->m_pRECBInventory_guess && gViewPort->m_pRECBInventory_guess->isVisible() )
				gViewPort->HideTopMenu();
		}

		if ( gHUD.m_iLadder_guess || gHUD.m_iFallOver_guess || iAttachCamFlag_guess )
		{
			if ( flWeaponLower_guess < 20 )
				flWeaponLower_guess += gHUD.m_flTimeDelta * 100;
			if ( flWeaponLower_guess > 20 )
				flWeaponLower_guess = 20;
		}
		else if ( g_iHeadShieldType_guess == 3 )
		{
			if ( flWeaponLower_guess < 20 )
				flWeaponLower_guess += gHUD.m_flTimeDelta * 100;
			if ( flWeaponLower_guess > 20 )
				flWeaponLower_guess = 20;
		}
		else if ( flWeaponLower_guess != 0 )
		{
			flWeaponLower_guess = 0;
		}

		speed = flGunParam1_guess * 8;
		speed = clamp( speed, -400, 400 );
	}
	else
	{
		if ( flWeaponLower_guess > 0 )
			flWeaponLower_guess -= gHUD.m_flTimeDelta * 100;
		if ( flWeaponLower_guess < 0 )
			flWeaponLower_guess = 0;

		if ( flSprintFlag_guess > 0 )
		{
			speed = 0;
		}
		else
		{
			speed = flGunParam1_guess * 2;
			speed = clamp( speed, -320, 320 );
		}
	}

	float bob_offset = ( speed - 0.0f ) / ( 320.0f - 0.0f ) + 0.0f;

	bobtime += ( curtime - lastbobtime ) * bob_offset;
	lastbobtime = curtime;

	//Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX)*HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}

	g_verticalBob_guess = speed*0.005f;
	g_verticalBob_guess = g_verticalBob_guess*0.3 + g_verticalBob_guess*0.7*sin(cycle);

	if ( g_verticalBob_guess > 4.0f )
		g_verticalBob_guess = 4.0f;
	else if ( g_verticalBob_guess < -7.0f )
		g_verticalBob_guess = -7.0f;

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX*2)*HL2_BOB_CYCLE_MAX*2;
	cycle /= HL2_BOB_CYCLE_MAX*2;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}

	g_lateralBob_guess = speed*0.005f;
	g_lateralBob_guess = g_lateralBob_guess*0.3 + g_lateralBob_guess*0.7*sin(cycle);

	if ( g_lateralBob_guess > 4.0f )
		g_lateralBob_guess = 4.0f;
	else if ( g_lateralBob_guess < -7.0f )
		g_lateralBob_guess = -7.0f;

	if ( flSprintFlag_guess > 0 )
		g_lateralBob_guess *= 2;

	return 0.0f;
}

void AddViewmodelBob_guess( struct ref_params_s *pparams, Vector &origin, Vector &angles, float frametime, float curtime, cl_entity_t *view, bool bLeft )
{
	Vector	forward, right, up;
	AngleVectors( angles, forward, right, up );

	Vector	vforward, vright, vup;
	AngleVectors( pparams->cl_viewangles, vforward, vright, vup );

	CalcViewmodelBob_guess( pparams, frametime, curtime );

	if ( gHUD.m_bIronSights_guess )
	{
		g_lateralBob_guess = 0;
		g_verticalBob_guess = 0;
	}

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob_guess * 0.1f, forward, origin );

	// Z bob a bit more
	origin[2] += g_verticalBob_guess * 0.1f;

	// bob the angles
	angles[ ROLL ]	+= g_verticalBob_guess * 0.5f;
	angles[ PITCH ]	-= g_verticalBob_guess * 0.4f;

	angles[ YAW ]	-= g_lateralBob_guess  * 0.3f;

	VectorMA( origin, g_lateralBob_guess * 0.8f, right, origin );

	if ( !view )
		return;

	view->origin = origin;
	view->angles = angles;

	view->origin = view->origin - vforward * flUnknown1055959C_guess;

	if ( !gHUD.m_iLadder_guess && !gHUD.m_iFallOver_guess && !iAttachCamFlag_guess )
	{
		// both branches are the same in retail; only the m_pWeapon load of the condition is left
		if ( gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId )
			view->curstate.angles[0] -= flWeaponLower_guess;
		else
			view->curstate.angles[0] -= flWeaponLower_guess;

		if ( bLeft )
		{
			view->curstate.angles[1] += flWeaponLower_guess * 0.5f;
			view->curstate.angles[2] += flWeaponLower_guess * 0.5f;
		}
		else
		{
			view->curstate.angles[1] -= flWeaponLower_guess * 0.5f;
			view->curstate.angles[2] -= flWeaponLower_guess * 0.5f;
		}
	}
	else
	{
		if ( gHUD.m_Ammo.m_pWeapon && gHUD.m_Ammo.m_pWeapon->iId )
			view->curstate.angles[0] -= flWeaponLower_guess * 2;
		else
			view->curstate.angles[0] -= flWeaponLower_guess * 2;

		if ( bLeft )
			view->curstate.angles[1] += flWeaponLower_guess * 0.5f;
		else
			view->curstate.angles[1] -= flWeaponLower_guess * 0.5f;
	}

	float flSin = sin( gEngfuncs.GetClientTime() / flWeaponIdleSpeed_guess );
	float flCos = cos( gEngfuncs.GetClientTime() / flWeaponIdleSpeed_guess );

	flSin = flSin / flWeaponIdleDampener_guess;
	flCos = flCos / flWeaponIdleDampener_guess;

	if ( gHUD.m_bIronSights_guess )
	{
		flCos = 0;
		flSin = 0;
	}

	view->origin = view->origin + vright * flSin;
	view->origin = view->origin + vforward * ( flSin * 0.5f );
	view->origin = view->origin + vup * ( flCos * 0.5f );
}

void CalcViewModelLag_guess( float frametime, Vector &origin, Vector &angles, Vector &original_angles, cl_entity_t *view, bool bLeft )
{
	if ( gHUD.m_bIronSights_guess )
		return;

	Vector vOriginalOrigin = origin;
	Vector vOriginalAngles = angles;

	// Calculate our drift
	Vector	forward, right, up;
	AngleVectors( angles, forward, NULL, NULL );

	if ( frametime != 0.0f )
	{
		Vector vDifference;
		VectorSubtract( forward, vecLastFacing_guess, vDifference );

		float flForce = CVAR_GET_FLOAT( "cl_lazyweapon_force" );
		flForce = clamp( flForce, 0, 10 );

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.  Solves the problem with fast cl_yawspeed, m_yaw or joysticks
		//  rotating quickly.  The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if ( (flDiff > g_fMaxViewModelLag_guess) && (g_fMaxViewModelLag_guess > 0.0f) )
		{
			float flScale = flDiff / g_fMaxViewModelLag_guess;
			flSpeed *= flScale;
		}

		VectorMA( vecLastFacing_guess, flSpeed * frametime, vDifference, vecLastFacing_guess );
		// Make sure it doesn't grow out of control!!!
		VectorNormalize( vecLastFacing_guess );
		VectorMA( origin, flForce, vDifference * -1.0f, origin );
	}

	AngleVectors( original_angles, forward, right, up );

	if ( g_fMaxViewModelLag_guess == 0.0f )
	{
		origin = vOriginalOrigin;
		angles = vOriginalAngles;
	}
}

//===============================
// cof: lights
//===============================
extern cvar_t *cl_lowflashlight;
extern cl_entity_t *pAttachCamEnt_guess;
extern int iBoat_guess;
extern int iWheelchair_guess;

void CreateMagliteLight_guess( vec3_t origin, vec3_t angles, float time, float frametime )
{
	if ( !( gHUD.m_iFlashFlags_guess & 8 ) )
		return;

	if ( IsViewEntityActive_guess() || pAttachCamEnt_guess || iBoat_guess || pTelescopeEnt_guess || iWheelchair_guess )
		return;

	static float add = 0;
	float addideal = 0;
	pmtrace_t tr;
	vec3_t fwd, right, up;
	AngleVectors(angles, fwd, right, up);
	fwd = origin + (fwd*150);
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( origin, fwd, PM_NORMAL, -1, &tr );
	if (tr.fraction < 1.0)
		addideal = (1 - tr.fraction)*30;
	float speed = (add - addideal)*10;
	if (speed < 0) speed *= -1;
	if (add < addideal)
	{
		add += frametime*speed;
		if (add > addideal) add = addideal;
	}
	else if (add > addideal)
	{
		add -= frametime*speed;
		if (add < addideal) add = addideal;
	}

	DynamicLight *flashlight = MY_AllocDlight(-8);
	flashlight->origin = origin;
	flashlight->radius = 700;
	flashlight->die = time + 0.01;
	flashlight->angles = angles;
	if ( cl_lowflashlight->value == 1 )
		strcpy(flashlight->spot_texture, "gfx/low/flashlight2.tga");
	else if ( cl_lowflashlight->value == 0 )
		strcpy(flashlight->spot_texture, "gfx/flashlight2.tga");
	flashlight->cone_hor = 50+add;
	flashlight->cone_ver = 50+add;
	flashlight->color = Vector(1.4, 1.4, 1.4); // make model dymanic lighting happy
}

void SetupViewModelLight_guess( vec3_t origin, vec3_t angles, float time, float frametime )
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	cl_entity_t *view = gEngfuncs.GetViewModel();

	if ( !player || !view )
		return;

	if ( !( gHUD.m_iFlashFlags_guess & 4 ) )
		return;

	if ( IsViewEntityActive_guess() || pAttachCamEnt_guess || iBoat_guess || pTelescopeEnt_guess || iWheelchair_guess )
		return;

	origin = gEngfuncs.PM_TraceLine( v_origin, view->attachment[1], PM_TRACELINE_PHYSENTSONLY, 1, -1 )->endpos;

	static float add = 0;
	float addideal = 0;
	pmtrace_t tr;
	vec3_t fwd, right, up;
	AngleVectors(angles, fwd, right, up);
	fwd = view->attachment[2];
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( origin, fwd, PM_NORMAL, -1, &tr );
	if (tr.fraction < 1.0)
		addideal = (1 - tr.fraction)*30;
	float speed = (add - addideal)*10;
	if (speed < 0) speed *= -1;
	if (add < addideal)
	{
		add += frametime*speed;
		if (add > addideal) add = addideal;
	}
	else if (add > addideal)
	{
		add -= frametime*speed;
		if (add < addideal) add = addideal;
	}

	if ( gHUD.m_iFlashFlags_guess & 4 )
	{
		view = gEngfuncs.GetViewModel();

		if ( view->model )
		{
			vec3_t dir;
			VectorSubtract( view->attachment[2], view->attachment[1], dir );
			VectorAngles( dir, dir );
			dir[0] = -dir[0];

			DynamicLight *light = MY_AllocDlight(-66);
			light->origin = origin;
			light->radius = 700;
			light->die = time + 0.01;
			light->angles = dir;
			if ( cl_lowflashlight->value == 1 )
				strcpy(light->spot_texture, "gfx/low/flashlight2.tga");
			else if ( cl_lowflashlight->value == 0 )
				strcpy(light->spot_texture, "gfx/flashlight2.tga");
			light->cone_hor = 70;
			light->cone_ver = 70;
			light->color = Vector(0.5, 0.5, 0.5);
		}
	}
}


//#define TRACE_TEST
#if defined( TRACE_TEST )

extern float in_fov;
/*
====================
CalcFov
====================
*/
float CalcFov (float fov_x, float width, float height)
{
	float	a;
	float	x;

	if (fov_x < 1 || fov_x > 179)
		fov_x = 90;	// error, set to 90

	x = width/tan(fov_x/360*M_PI);

	a = atan (height/x);

	a = a*360/M_PI;

	return a;
}

int hitent = -1;

void V_Move( int mx, int my )
{
	float fov;
	float fx, fy;
	float dx, dy;
	float c_x, c_y;
	float dX, dY;
	vec3_t forward, up, right;
	vec3_t newangles;

	vec3_t farpoint;
	pmtrace_t tr;

	fov = CalcFov( in_fov, (float)ScreenWidth, (float)ScreenHeight );

	c_x = (float)ScreenWidth / 2.0;
	c_y = (float)ScreenHeight / 2.0;

	dx = (float)mx - c_x;
	dy = (float)my - c_y;

	// Proportion we moved in each direction
	fx = dx / c_x;
	fy = dy / c_y;

	dX = fx * in_fov / 2.0 ;
	dY = fy * fov / 2.0;

	newangles = v_angles;

	newangles[ YAW ] -= dX;
	newangles[ PITCH ] += dY;

	// Now rotate v_forward around that point
	AngleVectors ( newangles, forward, right, up );

	farpoint = v_origin + 8192 * forward;

	// Trace
	tr = *(gEngfuncs.PM_TraceLine( (float *)&v_origin, (float *)&farpoint, PM_TRACELINE_PHYSENTSONLY, 2 /*point sized hull*/, -1 ));

	if ( tr.fraction != 1.0 && tr.ent != 0 )
	{
		hitent = PM_GetPhysEntInfo( tr.ent );
		PM_ParticleLine( (float *)&v_origin, (float *)&tr.endpos, 5, 1.0, 0.0 );
	}
	else
	{
		hitent = -1;
	}
}

#endif




//===============================
// buz: flashlight managenemt
//===============================
void SetupFlashlight(vec3_t origin, vec3_t angles, float time, float frametime)
{
	cl_entity_t *player = gEngfuncs.GetLocalPlayer();
	if ( !player )
		return;

	if ( !( gHUD.m_iFlashFlags_guess & 3 ) )
		return;

	if ( IsViewEntityActive_guess() || pAttachCamEnt_guess || iBoat_guess || pTelescopeEnt_guess || iWheelchair_guess )
		return;

	if ( gHUD.m_iFlashFlags_guess & 2 )
	{
		origin = player->curstate.origin;
		angles[0] = CVAR_GET_FLOAT( "cof_reducedlightxangle" );
		angles[2] = CVAR_GET_FLOAT( "cof_reducedlightzangle" );
	}

	static float add = 0;
	float addideal = 0;
	pmtrace_t tr;
	vec3_t fwd, right, up;
	AngleVectors(angles, fwd, right, up);
	fwd = origin + (fwd*150);
	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	gEngfuncs.pEventAPI->EV_PlayerTrace( origin, fwd, PM_NORMAL, -1, &tr );
	if (tr.fraction < 1.0)
	{
		if ( gHUD.m_iFlashFlags_guess & 2 )
			addideal = (1 - tr.fraction)*CVAR_GET_FLOAT( "cof_reducedlightradius" );
		else
			addideal = (1 - tr.fraction)*30;
	}
	float speed = (add - addideal)*10;
	if (speed < 0) speed *= -1;
	if (add < addideal)
	{
		add += frametime*speed;
		if (add > addideal) add = addideal;
	}
	else if (add > addideal)
	{
		add -= frametime*speed;
		if (add < addideal) add = addideal;
	}

	if ( gHUD.m_iFlashFlags_guess & 1 )
	{
		DynamicLight *flashlight = MY_AllocDlight(-1);
		flashlight->origin = origin;
		flashlight->radius = 700;
		flashlight->die = time + 0.01;
		flashlight->angles = angles;
		if ( cl_lowflashlight->value == 1 )
			strcpy(flashlight->spot_texture, "gfx/low/flashlight.tga");
		else if ( cl_lowflashlight->value == 0 )
			strcpy(flashlight->spot_texture, "gfx/flashlight.tga");
		flashlight->cone_hor = 50+add;
		flashlight->cone_ver = 50+add;
		flashlight->color = Vector(0.46, 0.46, 0.51);
	}

	if ( ( gHUD.m_iFlashFlags_guess & 2 ) && !( gHUD.m_iFlashFlags_guess & 4 ) )
	{
		gEngfuncs.PM_TraceLine( origin, origin + Vector( 0, 0, -1024 ), PM_TRACELINE_PHYSENTSONLY, 1, -1 );

		DynamicLight *reducedlight = MY_AllocDlight(-2);
		reducedlight->origin = origin;
		reducedlight->radius = 700;
		reducedlight->die = time + 0.01;
		reducedlight->angles = angles;
		if ( cl_lowflashlight->value == 1 )
			strcpy(reducedlight->spot_texture, "gfx/low/flashlight.tga");
		else if ( cl_lowflashlight->value == 0 )
			strcpy(reducedlight->spot_texture, "gfx/flashlight.tga");
		reducedlight->cone_hor = 50+add;
		reducedlight->cone_ver = 50+add;
		reducedlight->color = Vector( CVAR_GET_FLOAT( "cof_reducedlightcolour1" ), CVAR_GET_FLOAT( "cof_reducedlightcolour2" ), CVAR_GET_FLOAT( "cof_reducedlightcolour3" ) );
	}
}