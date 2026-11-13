// cl_spectator.c -- spectator camera and the developer overview mode

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "pmove.h"
#include "APIProxy.h"
#include "cl_spectator.h"
#include "../pm_shared/pm_movevars.h"

extern	movevars_t	movevars;
extern	qboolean	allow_cheats;

overviewInfo_t	gDevOverview;
local_state_t	spectatorState;
cvar_t			dev_overview = { "dev_overview", "0" };

/*
===================
CL_IsSpectateOnly
===================
*/
int CL_IsSpectateOnly( void )
{
	g_engdstAddrs.IsSpectateOnly();

	if ( cls.spectator )
		return 1;
	return 0;
}

/*
===================
CL_IsDevOverviewMode
===================
*/
int CL_IsDevOverviewMode( void )
{
	return dev_overview.value && ( allow_cheats || cls.spectator );
}

/*
===================
CL_CalculateDevOverviewParameters

Fits the overview to the bounds of the world
===================
*/
void CL_CalculateDevOverviewParameters( void )
{
	vec3_t	min, max, v;
	float	xSize, ySize, xScale, yScale;
	int		i;

	min[0] = min[1] = min[2] = 32000;
	max[0] = max[1] = max[2] = -32000;

	for ( i = 0; i < cl.worldmodel->numvertexes; i++ )
	{
		VectorCopy( cl.worldmodel->vertexes[i].position, v );

		if ( v[0] > max[0] )
			max[0] = v[0];
		if ( v[1] > max[1] )
			max[1] = v[1];
		if ( v[2] > max[2] )
			max[2] = v[2];

		if ( v[0] < min[0] )
			min[0] = v[0];
		if ( v[1] < min[1] )
			min[1] = v[1];
		if ( v[2] < min[2] )
			min[2] = v[2];
	}

	gDevOverview.z_min = max[2] + 1;
	gDevOverview.z_max = min[2] - 1;

	gDevOverview.origin[0] = ( min[0] + max[0] ) / 2;
	gDevOverview.origin[1] = ( min[1] + max[1] ) / 2;
	gDevOverview.origin[2] = ( min[2] + max[2] ) / 2;

	xSize = max[0] - min[0];
	ySize = max[1] - min[1];

	if ( xSize < ySize )
	{
		gDevOverview.rotated = false;
		xScale = 8192 / ( xSize * 1.3333F );
		yScale = 8192 / ySize;
	}
	else
	{
		gDevOverview.rotated = true;
		xScale = 8192 / xSize;
		yScale = 8192 / ( ySize * 1.3333F );
	}

	gDevOverview.zoom = min( xScale, yScale );
}

/*
===================
CL_InitSpectator
===================
*/
void CL_InitSpectator( void )
{
	Q_memset( &spectatorState, 0, sizeof( spectatorState ) );
	Q_memset( &gDevOverview, 0, sizeof( gDevOverview ) );

	CL_CalculateDevOverviewParameters();

	if ( !cls.spectator )
		return;

	spectatorState.playerstate.number = cl.playernum + 1;
	spectatorState.playerstate.friction = 1;
	spectatorState.playerstate.usehull = 1;
	spectatorState.playerstate.movetype = MOVETYPE_NOCLIP;
	spectatorState.playerstate.gravity = 1;
	spectatorState.client.maxspeed = movevars.spectatormaxspeed;
}

/*
===================
CL_AddEntityToPhysList
===================
*/
qboolean CL_AddEntityToPhysList( int entIndex )
{
	physent_t	*pe = NULL;

	if ( pmove->numphysent >= MAX_PHYSENTS )
	{
		Con_DPrintf( "CL_AddEntityToPhysList:  pmove->numphysent >= MAX_PHYSENTS\n" );
		return false;
	}

	if ( !cl_entities[entIndex].curstate.modelindex )
		return false;

	pe = &pmove->physents[pmove->numphysent];
	pmove->numphysent++;

	CL_AddStateToEntlist( pe, &cl_entities[entIndex].curstate );
	return true;
}

/*
===================
CL_MoveSpectatorCamera
===================
*/
void CL_MoveSpectatorCamera( void )
{
	double		time;
	qboolean	added;

	time = cl.time;
	added = true;

	if ( cls.state != ca_active || !cls.spectator )
		return;

	CL_SetUpPlayerPrediction( false, true );
	CL_SetSolidPlayers( cl.playernum );
	CL_RunUsercmd( &spectatorState, &spectatorState, &cl.cmd, true, &time, (int)( time * 100 ) );

	VectorCopy( spectatorState.client.velocity, cl.simvel );
	VectorCopy( spectatorState.playerstate.origin, cl.simorg );
	VectorCopy( spectatorState.client.punchangle, cl.punchangle );
	VectorCopy( spectatorState.client.view_ofs, cl.viewheight );
}

/*
===================
CL_SetDevOverView

Moves the overview camera with the movement keys
===================
*/
void CL_SetDevOverView( refdef_t *refdef )
{
	gDevOverview.origin[0] -= cl.cmd.sidemove / 128;
	gDevOverview.origin[1] -= cl.cmd.forwardmove / 128;

	if ( cl.cmd.upmove > 0 )
		gDevOverview.z_min += 1;
	if ( cl.cmd.upmove < 0 )
		gDevOverview.z_min -= 1;

	if ( cl.cmd.buttons & IN_DUCK )
		gDevOverview.z_max -= 1;
	if ( cl.cmd.buttons & IN_JUMP )
		gDevOverview.z_max += 1;

	if ( cl.cmd.buttons & IN_ATTACK )
		gDevOverview.zoom += 0.01F;
	if ( cl.cmd.buttons & IN_ATTACK2 )
		gDevOverview.zoom -= 0.01F;

	refdef->onlyClientDraws = false;
	VectorCopy( gDevOverview.origin, refdef->vieworg );
	refdef->vieworg[2] = 16000;
	refdef->viewangles[0] = 90;
	refdef->viewangles[1] = gDevOverview.rotated ? 90.0F : 0.0F;
	refdef->viewangles[2] = 0;

	if ( dev_overview.value < 2 )
		Con_Printf( " Overview: Zoom %.2f, Map Origin (%.2f, %.2f, %.2f), Z Min %.2f, Z Max %.2f, Rotated %i\n",
			gDevOverview.zoom, gDevOverview.origin[0], gDevOverview.origin[1], gDevOverview.origin[2],
			gDevOverview.z_min, gDevOverview.z_max, gDevOverview.rotated );
}
