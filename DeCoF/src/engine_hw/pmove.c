// pmove.c -- the engine's side of the shared player movement code: touch
// list, and the callbacks the game DLL's pmove calls back into

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "info.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "client.h"
#include "pmove.h"
#include "../pm_shared/pm_movevars.h"

float	RandomFloat( float flLow, float flHigh );
int32	RandomLong( int32 lLow, int32 lHigh );
char	*memfgets( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufferSize );

movevars_t		movevars;
playermove_t	*pmove;

cvar_t	pm_showclip = { "pm_showclip", "0" };

vec3_t	player_mins[4] =
{
	{ -16, -16, -36 },
	{ -16, -16, -18 },
	{ 0, 0, 0 },
	{ -32, -32, -32 },
};

vec3_t	player_maxs[4] =
{
	{ 16, 16, 36 },
	{ 16, 16, 18 },
	{ 0, 0, 0 },
	{ 32, 32, 32 },
};

/*
================
PM_AddToTouched

Add's the trace result to touch list, if contact is not already in list.
================
*/
qboolean PM_AddToTouched( pmtrace_t tr, vec_t *impactvelocity )
{
	int		i;

	for ( i = 0; i < pmove->numtouch; i++ )
	{
		if ( pmove->touchindex[i].ent == tr.ent )
			break;
	}

	if ( i != pmove->numtouch )  // Already in list.
		return false;

	VectorCopy( impactvelocity, tr.deltavelocity );

	if ( pmove->numtouch >= MAX_PHYSENTS )
		pmove->Con_DPrintf( "Too many entities were touched!\n" );

	pmove->touchindex[pmove->numtouch] = tr;
	pmove->numtouch++;
	return true;
}

/*
================
PM_StuckTouch

================
*/
void PM_StuckTouch( int hitent, pmtrace_t *ptraceresult )
{
	edict_t	*info;
	int		n;

	if ( !pmove->server )
		return;

	n = pmove->physents[hitent].info;
	info = EDICT_NUM( n );

	PM_AddToTouched( *ptraceresult, pmove->velocity );
}

/*
================
PM_Init

================
*/
void PM_Init( playermove_t *ppm )
{
	int		i;

	PM_InitBoxHull();

	for ( i = 0; i < 4; i++ )
	{
		VectorCopy( player_mins[i], ppm->player_mins[i] );
		VectorCopy( player_maxs[i], ppm->player_maxs[i] );
	}

	ppm->movevars = &movevars;

	ppm->PM_Info_ValueForKey = Info_ValueForKey;
	ppm->PM_Particle = CL_Particle;
	ppm->PM_TestPlayerPosition = PM_TestPlayerPosition;
	ppm->Con_NPrintf = Con_NPrintf;
	ppm->Con_DPrintf = Con_DPrintf;
	ppm->Con_Printf = Con_Printf;
	ppm->Sys_FloatTime = Sys_FloatTime;
	ppm->PM_StuckTouch = PM_StuckTouch;
	ppm->PM_PointContents = PM_PointContents;
	ppm->PM_TruePointContents = PM_TruePointContents;
	ppm->PM_HullPointContents = PM_HullPointContents;
	ppm->PM_PlayerTrace = PM_PlayerTrace;
	ppm->PM_TraceLine = PM_TraceLine;
	ppm->PM_TraceModel = PM_TraceModel;
	ppm->RandomLong = RandomLong;
	ppm->RandomFloat = RandomFloat;
	ppm->PM_GetModelType = PM_GetModelType;
	ppm->PM_HullForBsp = PM_HullForBsp;
	ppm->PM_GetModelBounds = PM_GetModelBounds;
	ppm->COM_FileSize = COM_FileSize;
	ppm->COM_LoadFile = COM_LoadFile;
	ppm->COM_FreeFile = COM_FreeFile;
	ppm->memfgets = memfgets;
	ppm->PM_PlayerTraceEx = PM_PlayerTraceEx;
	ppm->PM_TestPlayerPositionEx = PM_TestPlayerPositionEx;
	ppm->PM_TraceLineEx = PM_TraceLineEx;
}
