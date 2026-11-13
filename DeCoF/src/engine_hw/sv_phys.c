// sv_phys.c

#include <math.h>
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "host.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "world.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t	sv_friction = { "sv_friction", "4", FCVAR_SERVER };
cvar_t	sv_stopspeed = { "sv_stopspeed", "100", FCVAR_SERVER };
cvar_t	sv_gravity = { "sv_gravity", "800", FCVAR_SERVER };
cvar_t	sv_maxvelocity = { "sv_maxvelocity", "2000" };
cvar_t	sv_stepsize = { "sv_stepsize", "18", FCVAR_SERVER };
cvar_t	sv_clipmode = { "sv_clipmode", "0", FCVAR_SERVER };
cvar_t	sv_bounce = { "sv_bounce", "1", FCVAR_SERVER };
cvar_t	sv_airmove = { "sv_airmove", "1", FCVAR_SERVER };
cvar_t	sv_spectatormaxspeed = { "sv_spectatormaxspeed", "500" };
cvar_t	sv_airaccelerate = { "sv_airaccelerate", "10", FCVAR_SERVER };
cvar_t	sv_wateraccelerate = { "sv_wateraccelerate", "10", FCVAR_SERVER };
cvar_t	sv_waterfriction = { "sv_waterfriction", "1", FCVAR_SERVER };
cvar_t	sv_zmax = { "sv_zmax", "4096", FCVAR_SPONLY };
cvar_t	sv_wateramp = { "sv_wateramp", "0" };

// Sky name
cvar_t	sv_skyname = { "sv_skyname", "black" };

static	vec3_t	vec_origin;

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts( void )
{
	int			e;
	edict_t		*check;

// see if any solid entities are inside the final position
	for ( e = 1; e < sv.num_edicts; e++ )
	{
		check = &sv.edicts[e];
		if ( check->free )
			continue;
		if ( check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_FOLLOW
		|| check->v.movetype == MOVETYPE_NOCLIP )
			continue;

		if ( SV_TestEntityPosition( check ) )
			Con_Printf( "entity in invalid position\n" );
	}
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity( edict_t *ent )
{
	int		i;

//
// bound velocity
//
	for ( i = 0; i < 3; i++ )
	{
		if ( IS_NAN( ent->v.velocity[i] ) )
		{
			Con_Printf( "Got a NaN velocity on %s\n", pr_strings + ent->v.classname );
			ent->v.velocity[i] = 0;
		}
		if ( IS_NAN( ent->v.origin[i] ) )
		{
			Con_Printf( "Got a NaN origin on %s\n", pr_strings + ent->v.classname );
			ent->v.origin[i] = 0;
		}
		if ( ent->v.velocity[i] > sv_maxvelocity.value )
		{
			Con_DPrintf( "Got a velocity too high on %s\n", pr_strings + ent->v.classname );
			ent->v.velocity[i] = sv_maxvelocity.value;
		}
		else if ( ent->v.velocity[i] < -sv_maxvelocity.value )
		{
			Con_DPrintf( "Got a velocity too low on %s\n", pr_strings + ent->v.classname );
			ent->v.velocity[i] = -sv_maxvelocity.value;
		}
	}
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean SV_RunThink( edict_t *ent )
{
	float	thinktime;

	if ( !( ent->v.flags & FL_KILLME ) )
	{
		thinktime = ent->v.nextthink;
		if ( thinktime <= 0 || thinktime > sv.time + host_frametime )
			return TRUE;

		if ( thinktime < sv.time )
			thinktime = sv.time;	// don't let things stay in the past.
								// it is possible to start that way
								// by a trigger with a local time.
		ent->v.nextthink = 0;
		gGlobalVariables.time = thinktime;
		gEntityInterface.pfnThink( ent );
	}

	if ( ent->v.flags & FL_KILLME )
	{
		ED_Free( ent );
	}

	return !ent->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact( edict_t *e1, edict_t *e2, trace_t *ptrace )
{
	gGlobalVariables.time = sv.time;

	if ( ( e1->v.flags | e2->v.flags ) & FL_KILLME )
		return;

	if ( e1->v.groupinfo && e2->v.groupinfo )
	{
		if ( g_groupop == GROUP_OP_AND )
		{
			if ( !( e1->v.groupinfo & e2->v.groupinfo ) )
				return;
		}
		else if ( g_groupop == GROUP_OP_NAND )
		{
			if ( e1->v.groupinfo & e2->v.groupinfo )
				return;
		}
	}

	if ( e1->v.solid != SOLID_NOT )
	{
		SV_SetGlobalTrace( ptrace );
		gEntityInterface.pfnTouch( e1, e2 );
	}

	if ( e2->v.solid != SOLID_NOT )
	{
		SV_SetGlobalTrace( ptrace );
		gEntityInterface.pfnTouch( e2, e1 );
	}
}

/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define	STOP_EPSILON	0.1

int ClipVelocity( vec_t *in, vec_t *normal, vec_t *out, float overbounce )
{
	float	backoff;
	float	change;
	float	angle;
	int		i, blocked;

	blocked = 0;
	angle = normal[2];

	if ( angle > 0 )
		blocked |= 1;		// floor
	if ( !angle )
		blocked |= 2;		// step

	backoff = DotProduct( in, normal ) * overbounce;

	for ( i = 0; i < 3; i++ )
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;
		if ( out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON )
			out[i] = 0;
	}

	return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not NULL, the trace of any vertical wall hit will be stored
============
*/
#define	MAX_CLIP_PLANES	5

int SV_FlyMove( edict_t *ent, float time, trace_t *steptrace )
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int			i, j;
	trace_t		trace, test;
	vec3_t		end;
	float		time_left;
	int			blocked;
	qboolean	monsterClip;

	if ( ent->v.flags & FL_MONSTERCLIP )
		monsterClip = TRUE;
	else
		monsterClip = FALSE;

	numbumps = 4;

	blocked = 0;
	VectorCopy( ent->v.velocity, original_velocity );
	VectorCopy( ent->v.velocity, primal_velocity );
	numplanes = 0;

	time_left = time;

	for ( bumpcount = 0; bumpcount < numbumps; bumpcount++ )
	{
		if ( !ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2] )
			break;

		for ( i = 0; i < 3; i++ )
			end[i] = ent->v.origin[i] + time_left * ent->v.velocity[i];

		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, monsterClip );

		if ( trace.allsolid )
		{	// entity is trapped in another solid
			VectorCopy( vec3_origin, ent->v.velocity );
			return 4;
		}

		if ( trace.fraction > 0 )
		{	// actually covered some distance
			test = SV_Move( trace.endpos, ent->v.mins, ent->v.maxs, trace.endpos, MOVE_NORMAL, ent, monsterClip );
			if ( test.allsolid != TRUE )
			{
				VectorCopy( trace.endpos, ent->v.origin );
				VectorCopy( ent->v.velocity, original_velocity );
				numplanes = 0;
			}
		}

		if ( trace.fraction == 1 )
			 break;		// moved the entire distance

		if ( !trace.ent )
			Sys_Error( "SV_FlyMove: !trace.ent" );

		if ( trace.plane.normal[2] > 0.7 )
		{
			blocked |= 1;		// floor
			if ( trace.ent->v.solid == SOLID_BSP
			|| trace.ent->v.movetype == MOVETYPE_PUSHSTEP
			|| trace.ent->v.solid == SOLID_SLIDEBOX
			|| ent->v.flags & FL_CLIENT )
			{
				ent->v.flags |= FL_ONGROUND;
				ent->v.groundentity = trace.ent;
			}
		}
		if ( !trace.plane.normal[2] )
		{
			blocked |= 2;		// step
			if ( steptrace )
				*steptrace = trace;	// save for player extrafriction
		}

//
// run the impact function
//
		SV_Impact( ent, trace.ent, &trace );
		if ( ent->free )
			break;		// removed by the impact function

		time_left -= time_left * trace.fraction;

	// cliped to another plane
		if ( numplanes >= MAX_CLIP_PLANES )
		{	// this shouldn't really happen
			VectorCopy( vec3_origin, ent->v.velocity );
			return blocked;
		}

		VectorCopy( trace.plane.normal, planes[numplanes] );
		numplanes++;

//
// modify original_velocity so it parallels all of the clip planes
//
		if ( numplanes == 1 && ent->v.movetype == MOVETYPE_WALK && ( !( ent->v.flags & FL_ONGROUND ) || ent->v.friction != 1 ) )
		{
			for ( i = 0; i < numplanes; i++ )
			{
				if ( planes[i][2] > 0.7 )
				{	// floor or slope
					ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
					VectorCopy( new_velocity, original_velocity );
				}
				else
					ClipVelocity( original_velocity, planes[i], new_velocity, (float)( 1.0 + sv_bounce.value * ( 1 - ent->v.friction ) ) );
			}

			VectorCopy( new_velocity, ent->v.velocity );
			VectorCopy( new_velocity, original_velocity );
		}
		else
		{
			for ( i = 0; i < numplanes; i++ )
			{
				ClipVelocity( original_velocity, planes[i], new_velocity, 1 );
				for ( j = 0; j < numplanes; j++ )
				{
					if ( j != i )
					{
						if ( DotProduct( new_velocity, planes[j] ) < 0 )
							break;	// not ok
					}
				}
				if ( j == numplanes )
					break;
			}

			if ( i != numplanes )
			{	// go along this plane
				VectorCopy( new_velocity, ent->v.velocity );
			}
			else
			{	// go along the crease
				if ( numplanes != 2 )
				{
//					Con_Printf( "clip velocity, numplanes == %i\n", numplanes );
					return blocked;
				}
				CrossProduct( planes[0], planes[1], dir );
				d = DotProduct( dir, ent->v.velocity );
				VectorScale( dir, d, ent->v.velocity );
			}

	//
	// if original velocity is against the original velocity, stop dead
	// to avoid tiny occilations in sloping corners
	//
			if ( DotProduct( ent->v.velocity, primal_velocity ) <= 0 )
			{
				VectorCopy( vec3_origin, ent->v.velocity );
				return blocked;
			}
		}
	}

	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity( edict_t *ent )
{
	float	ent_gravity;

	if ( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else
		ent_gravity = 1.0;

	// Add gravity incorrectly
	ent->v.velocity[2] -= ( ent_gravity * sv_gravity.value * host_frametime );
	ent->v.velocity[2] += ( ent->v.basevelocity[2] * host_frametime );
	ent->v.basevelocity[2] = 0;

	SV_CheckVelocity( ent );
}

/*
============
SV_AddCorrectGravity

============
*/
void SV_AddCorrectGravity( edict_t *ent )
{
	float	ent_gravity;

	if ( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else
		ent_gravity = 1.0;

	// Add 1/2 of the total gravitational effects over this timestep
	ent->v.velocity[2] -= ( ent_gravity * sv_gravity.value * 0.5 * host_frametime );
	ent->v.velocity[2] += ( ent->v.basevelocity[2] * host_frametime );
	ent->v.basevelocity[2] = 0;

	SV_CheckVelocity( ent );
}

/*
============
SV_FixupGravityVelocity

============
*/
void SV_FixupGravityVelocity( edict_t *ent )
{
	float	ent_gravity;

	if ( ent->v.gravity )
		ent_gravity = ent->v.gravity;
	else
		ent_gravity = 1.0;

	// Get the correct velocity for the end of the dt
	ent->v.velocity[2] -= ( ent_gravity * sv_gravity.value * host_frametime * 0.5 );

	SV_CheckVelocity( ent );
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity( edict_t *ent, vec_t *push )
{
	trace_t		trace;
	vec3_t		end;
	qboolean	monsterClip;
	int			moveType;

	if ( ent->v.flags & FL_MONSTERCLIP )
		monsterClip = TRUE;
	else
		monsterClip = FALSE;

	VectorAdd( ent->v.origin, push, end );

	if ( ent->v.movetype == MOVETYPE_FLYMISSILE )
		moveType = MOVE_MISSILE;
	else if ( ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT )
	// only clip against bmodels
		moveType = MOVE_NOMONSTERS;
	else
		moveType = MOVE_NORMAL;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, moveType, ent, monsterClip );

	if ( trace.fraction != 0 )
	{
		VectorCopy( trace.endpos, ent->v.origin );
	}

	SV_LinkEdict( ent, TRUE );

	if ( trace.ent )
		SV_Impact( ent, trace.ent, &trace );

	return trace;
}

edict_t	**g_moved_edict;
vec3_t	*g_moved_from;

/*
============
SV_PushMove

============
*/
void SV_PushMove( edict_t *pusher, float movetime )
{
	int			i, e;
	edict_t		*check;
	vec3_t		mins, maxs, move;
	vec3_t		entorig, pushorig;
	int			num_moved;

	if ( !pusher->v.velocity[0] && !pusher->v.velocity[1] && !pusher->v.velocity[2] )
	{
		pusher->v.ltime += movetime;
		return;
	}

	VectorCopy( pusher->v.origin, pushorig );

	for ( i = 0; i < 3; i++ )
	{
		move[i] = pusher->v.velocity[i] * movetime;
		mins[i] = pusher->v.absmin[i] + move[i];
		maxs[i] = pusher->v.absmax[i] + move[i];

		// move the pusher to it's final position
		pusher->v.origin[i] += move[i];
	}

	pusher->v.ltime += movetime;
	SV_LinkEdict( pusher, FALSE );

	// non-solid pushers can't push anything
	if ( pusher->v.solid == SOLID_NOT )
		return;

// see if any solid entities are inside the final position
	num_moved = 0;
	for ( e = 1; e < sv.num_edicts; e++ )
	{
		check = &sv.edicts[e];
		if ( check->free )
			continue;
		if ( check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_FOLLOW
		|| check->v.movetype == MOVETYPE_NOCLIP )
			continue;

	// if the entity is standing on the pusher, it will definately be moved
		if ( !( ( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ) )
		{
			if ( check->v.absmin[0] >= maxs[0]
			|| check->v.absmin[1] >= maxs[1]
			|| check->v.absmin[2] >= maxs[2]
			|| check->v.absmax[0] <= mins[0]
			|| check->v.absmax[1] <= mins[1]
			|| check->v.absmax[2] <= mins[2] )
				continue;

		// see if the ent's bbox is inside the pusher's final position
			if ( !SV_TestEntityPosition( check ) )
				continue;
		}

	// remove the onground flag for non-players
		if ( check->v.movetype != MOVETYPE_WALK )
			check->v.flags &= ~FL_ONGROUND;

		VectorCopy( check->v.origin, entorig );
		VectorCopy( check->v.origin, g_moved_from[num_moved] );
		g_moved_edict[num_moved] = check;
		num_moved++;

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity( check, move );
		pusher->v.solid = SOLID_BSP;

	// if it is still inside the pusher, block
		if ( SV_TestEntityPosition( check ) )
		{	// fail the move
			if ( check->v.mins[0] == check->v.maxs[0] )
				continue;
			if ( check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER )
			{	// corpse
				check->v.mins[0] = check->v.mins[1] = 0;
				VectorCopy( check->v.mins, check->v.maxs );
				continue;
			}

			VectorCopy( entorig, check->v.origin );
			SV_LinkEdict( check, TRUE );

			VectorCopy( pushorig, pusher->v.origin );
			SV_LinkEdict( pusher, FALSE );
			pusher->v.ltime -= movetime;

			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			gEntityInterface.pfnBlocked( pusher, check );

		// move back any entities we already moved
			for ( i = 0; i < num_moved; i++ )
			{
				VectorCopy( g_moved_from[i], g_moved_edict[i]->v.origin );
				SV_LinkEdict( g_moved_edict[i], FALSE );
			}
			return;
		}
	}
}

/*
============
SV_PushRotate

============
*/
int SV_PushRotate( edict_t *pusher, float movetime )
{
	int			i, e;
	edict_t		*check;
	vec3_t		move, amove;
	vec3_t		entorig, pushorig;
	int			num_moved;
	vec3_t		org, start, end;
	vec3_t		forward, right, up;
	vec3_t		forwardNow, rightNow, upNow;

	if ( !pusher->v.avelocity[0] && !pusher->v.avelocity[1] && !pusher->v.avelocity[2] )
	{
		pusher->v.ltime += movetime;
		return TRUE;
	}

	for ( i = 0; i < 3; i++ )
		amove[i] = pusher->v.avelocity[i] * movetime;

	AngleVectors( pusher->v.angles, forwardNow, rightNow, upNow );

	VectorCopy( pusher->v.angles, pushorig );

// move the pusher to it's final position
	VectorAdd( pusher->v.angles, amove, pusher->v.angles );

	AngleVectorsTranspose( pusher->v.angles, forward, right, up );

	pusher->v.ltime += movetime;
	SV_LinkEdict( pusher, FALSE );

	// non-solid pushers can't push anything
	if ( pusher->v.solid == SOLID_NOT )
		return TRUE;

	// only yaw rotation carries entities along
	amove[0] = 0;
	amove[2] = 0;

// see if any solid entities are inside the final position
	num_moved = 0;
	for ( e = 1; e < sv.num_edicts; e++ )
	{
		check = &sv.edicts[e];
		if ( check->free )
			continue;
		if ( check->v.movetype == MOVETYPE_PUSH
		|| check->v.movetype == MOVETYPE_NONE
		|| check->v.movetype == MOVETYPE_FOLLOW
		|| check->v.movetype == MOVETYPE_NOCLIP )
			continue;

	// if the entity is standing on the pusher, it will definately be moved
		if ( !( ( check->v.flags & FL_ONGROUND ) && check->v.groundentity == pusher ) )
		{
			if ( check->v.absmin[0] >= pusher->v.absmax[0]
			|| check->v.absmin[1] >= pusher->v.absmax[1]
			|| check->v.absmin[2] >= pusher->v.absmax[2]
			|| check->v.absmax[0] <= pusher->v.absmin[0]
			|| check->v.absmax[1] <= pusher->v.absmin[1]
			|| check->v.absmax[2] <= pusher->v.absmin[2] )
				continue;

		// see if the ent's bbox is inside the pusher's final position
			if ( !SV_TestEntityPosition( check ) )
				continue;
		}

	// remove the onground flag for non-players
		if ( check->v.movetype != MOVETYPE_WALK )
			check->v.flags &= ~FL_ONGROUND;

		VectorCopy( check->v.origin, entorig );
		VectorCopy( check->v.origin, g_moved_from[num_moved] );
		g_moved_edict[num_moved] = check;
		num_moved++;

		if ( num_moved >= sv.max_edicts )
			Sys_Error( "Out of edicts in simulator!\n" );

		if ( check->v.movetype == MOVETYPE_PUSHSTEP )
		{
			org[0] = ( check->v.absmin[0] + check->v.absmax[0] ) * 0.5;
			org[1] = ( check->v.absmin[1] + check->v.absmax[1] ) * 0.5;
			org[2] = ( check->v.absmin[2] + check->v.absmax[2] ) * 0.5;
			VectorSubtract( org, pusher->v.origin, start );
		}
		else
		{
			VectorSubtract( check->v.origin, pusher->v.origin, start );
		}

		// calculate destination position
		org[0] = DotProduct( start, forwardNow );
		org[1] = -DotProduct( start, rightNow );
		org[2] = DotProduct( start, upNow );

		end[0] = DotProduct( org, forward );
		end[1] = DotProduct( org, right );
		end[2] = DotProduct( org, up );

		VectorSubtract( end, start, move );

		// try moving the contacted entity
		pusher->v.solid = SOLID_NOT;
		SV_PushEntity( check, move );
		pusher->v.solid = SOLID_BSP;

		if ( check->v.movetype != MOVETYPE_PUSHSTEP )
		{
			if ( check->v.flags & FL_CLIENT )
			{
				check->v.avelocity[1] += amove[1];
				check->v.fixangle = 2;
			}
			else
			{
				VectorAdd( check->v.angles, amove, check->v.angles );
			}
		}

	// if it is still inside the pusher, block
		if ( SV_TestEntityPosition( check ) )
		{	// fail the move
			if ( check->v.mins[0] == check->v.maxs[0] )
				continue;
			if ( check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER )
			{	// corpse
				check->v.mins[0] = check->v.mins[1] = 0;
				VectorCopy( check->v.mins, check->v.maxs );
				continue;
			}

			VectorCopy( entorig, check->v.origin );
			SV_LinkEdict( check, TRUE );

			VectorCopy( pushorig, pusher->v.angles );
			SV_LinkEdict( pusher, FALSE );
			pusher->v.ltime -= movetime;

			// if the pusher has a "blocked" function, call it
			// otherwise, just stay in place until the obstacle is gone
			gEntityInterface.pfnBlocked( pusher, check );

		// move back any entities we already moved
			for ( i = 0; i < num_moved; i++ )
			{
				VectorCopy( g_moved_from[i], g_moved_edict[i]->v.origin );
				if ( g_moved_edict[i]->v.flags & FL_CLIENT )
				{
					g_moved_edict[i]->v.avelocity[1] = 0;
				}
				else if ( g_moved_edict[i]->v.movetype != MOVETYPE_PUSHSTEP )
				{
					VectorSubtract( g_moved_edict[i]->v.angles, amove, g_moved_edict[i]->v.angles );
				}
				SV_LinkEdict( g_moved_edict[i], FALSE );
			}
			return FALSE;
		}
	}

	return TRUE;
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher( edict_t *ent )
{
	float	thinktime;
	float	oldltime;
	float	movetime;

	oldltime = ent->v.ltime;

	thinktime = ent->v.nextthink;
	if ( thinktime < ent->v.ltime + host_frametime )
	{
		movetime = thinktime - ent->v.ltime;
		if ( movetime < 0 )
			movetime = 0;
	}
	else
		movetime = host_frametime;

	if ( movetime )
	{
		if ( ent->v.avelocity[0] || ent->v.avelocity[1] || ent->v.avelocity[2] )
		{
			if ( ent->v.velocity[0] || ent->v.velocity[1] || ent->v.velocity[2] )
			{
				if ( SV_PushRotate( ent, movetime ) )
				{
					float	savetime;

					savetime = ent->v.ltime;
					// reset the local time to what it was before we rotated
					ent->v.ltime = oldltime;
					SV_PushMove( ent, movetime );
					if ( ent->v.ltime < savetime )
						ent->v.ltime = savetime;
				}
			}
			else
			{
				SV_PushRotate( ent, movetime );
			}
		}
		else
		{
			SV_PushMove( ent, movetime );	// advances ent->v.ltime if not blocked
		}
	}

	if ( thinktime > oldltime && ( ( ent->v.flags & FL_ALWAYSTHINK ) || thinktime <= ent->v.ltime ) )
	{
		ent->v.nextthink = 0;
		gGlobalVariables.time = sv.time;
		gEntityInterface.pfnThink( ent );
	}
}

/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckWater
=============
*/
qboolean SV_CheckWater( edict_t *ent )
{
	vec3_t	point;
	int		cont;
	int		truecont;

	point[0] = ( ent->v.absmin[0] + ent->v.absmax[0] ) * 0.5;
	point[1] = ( ent->v.absmin[1] + ent->v.absmax[1] ) * 0.5;
	point[2] = ent->v.absmin[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;

	g_groupmask = ent->v.groupinfo;
	cont = SV_PointContents( point );
	if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		truecont = cont;
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		if ( ent->v.absmin[2] == ent->v.absmax[2] )
		{
			// a point entity
			ent->v.waterlevel = 3;
		}
		else
		{
			point[2] = ( ent->v.absmin[2] + ent->v.absmax[2] ) * 0.5;

			g_groupmask = ent->v.groupinfo;
			cont = SV_PointContents( point );
			if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
			{
				ent->v.waterlevel = 2;

				// Now check the eye position.  (view_ofs is relative to the origin)
				VectorAdd( point, ent->v.view_ofs, point );

				g_groupmask = ent->v.groupinfo;
				cont = SV_PointContents( point );
				if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
					ent->v.waterlevel = 3;
			}
		}

		if ( truecont <= CONTENTS_CURRENT_0 && truecont >= CONTENTS_CURRENT_DOWN )
		{
			static vec3_t current_table[] =
			{
				{ 1, 0, 0 },
				{ 0, 1, 0 },
				{ -1, 0, 0 },
				{ 0, -1, 0 },
				{ 0, 0, 1 },
				{ 0, 0, -1 }
			};

			VectorMA( ent->v.basevelocity, (float)( 50.0 * ent->v.waterlevel ), current_table[CONTENTS_CURRENT_0 - truecont], ent->v.basevelocity );
		}
	}

	return ent->v.waterlevel > 1;
}

/*
=============
SV_RecursiveWaterLevel
=============
*/
float SV_RecursiveWaterLevel( vec_t *center, float out, float in, int count )
{
	vec3_t	test;
	float	offset;

	offset = in + ( ( out - in ) * 0.5 );
	count++;
	if ( count >= 6 )
		return offset;

	test[0] = center[0];
	test[1] = center[1];
	test[2] = center[2] + offset;

	if ( SV_PointContents( test ) == CONTENTS_WATER )
		return SV_RecursiveWaterLevel( center, out, offset, count );
	else
		return SV_RecursiveWaterLevel( center, offset, in, count );
}

/*
=============
SV_Submerged

Returns the depth of the entity below the water surface
=============
*/
float SV_Submerged( edict_t *ent )
{
	float	start, end, bottom;
	vec3_t	center;

	center[0] = ( ent->v.absmin[0] + ent->v.absmax[0] ) * 0.5;
	center[1] = ( ent->v.absmin[1] + ent->v.absmax[1] ) * 0.5;
	center[2] = ( ent->v.absmin[2] + ent->v.absmax[2] ) * 0.5;

	bottom = ent->v.absmin[2] - center[2];

	switch ( ent->v.waterlevel )
	{
	case 1:
		start = bottom;
		end = 0;
		return SV_RecursiveWaterLevel( center, end, start, 0 ) - bottom;

	case 3:
		{
			vec3_t	point;

			point[0] = center[0];
			point[1] = center[1];
			point[2] = ent->v.absmax[2];

			g_groupmask = ent->v.groupinfo;
			if ( SV_PointContents( point ) == CONTENTS_WATER )
				return ent->v.maxs[2] - ent->v.mins[2];
		}

	case 2:
		start = 0;
		end = ent->v.absmax[2] - center[2];
		return SV_RecursiveWaterLevel( center, end, start, 0 ) - bottom;
	}

	return 0;
}

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None( edict_t *ent )
{
// regular thinking
	SV_RunThink( ent );
}

/*
=============
SV_Physics_Follow

Entities that are "stuck" to another entity
=============
*/
void SV_Physics_Follow( edict_t *ent )
{
// regular thinking
	if ( !SV_RunThink( ent ) )
		return;

	if ( ent->v.aiment )
	{
		VectorAdd( ent->v.aiment->v.origin, ent->v.v_angle, ent->v.origin );
		VectorCopy( ent->v.aiment->v.angles, ent->v.angles );
	}
	else
	{
		Con_DPrintf( "%s movetype FOLLOW with NULL aiment\n", pr_strings + ent->v.classname );
		ent->v.movetype = MOVETYPE_NONE;
		return;
	}

	SV_LinkEdict( ent, TRUE );
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip( edict_t *ent )
{
// regular thinking
	if ( !SV_RunThink( ent ) )
		return;

	VectorMA( ent->v.angles, (float)host_frametime, ent->v.avelocity, ent->v.angles );
	VectorMA( ent->v.origin, (float)host_frametime, ent->v.velocity, ent->v.origin );

	SV_LinkEdict( ent, FALSE );	// force retouch even for stationary
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition( edict_t *ent )
{
	int		cont;
	vec3_t	point;

	point[0] = ( ent->v.absmin[0] + ent->v.absmax[0] ) * 0.5;
	point[1] = ( ent->v.absmin[1] + ent->v.absmax[1] ) * 0.5;
	point[2] = ent->v.absmin[2] + 1;

	g_groupmask = ent->v.groupinfo;
	cont = SV_PointContents( point );

	if ( !ent->v.watertype )
	{	// just spawned here
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		return;
	}

	if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		if ( ent->v.watertype == CONTENTS_EMPTY )
		{	// just crossed into water
			SV_StartSound( 0, ent, CHAN_AUTO, "player/pl_wade1.wav", 255, 1, 0, PITCH_NORM );
			ent->v.velocity[2] *= 0.5;
		}

		ent->v.watertype = cont;
		ent->v.waterlevel = 1;

		if ( ent->v.absmin[2] == ent->v.absmax[2] )
		{
			// a point entity
			ent->v.waterlevel = 3;
			return;
		}

		point[2] = ( ent->v.absmin[2] + ent->v.absmax[2] ) * 0.5;

		g_groupmask = ent->v.groupinfo;
		cont = SV_PointContents( point );
		if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			ent->v.waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			VectorAdd( point, ent->v.view_ofs, point );

			g_groupmask = ent->v.groupinfo;
			cont = SV_PointContents( point );
			if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
				ent->v.waterlevel = 3;
		}
	}
	else
	{
		if ( ent->v.watertype != CONTENTS_EMPTY )
		{	// just crossed into water
			SV_StartSound( 0, ent, CHAN_AUTO, "player/pl_wade2.wav", 255, 1, 0, PITCH_NORM );
		}
		ent->v.watertype = CONTENTS_EMPTY;
		ent->v.waterlevel = 0;
	}
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss( edict_t *ent )
{
	trace_t	trace;
	vec3_t	move;
	float	backoff;

	SV_CheckWater( ent );

// regular thinking
	if ( !SV_RunThink( ent ) )
		return;

	if ( ent->v.velocity[2] > 0 || !ent->v.groundentity || ( ent->v.groundentity->v.flags & ( FL_MONSTER | FL_CLIENT ) ) )
	{
		ent->v.flags &= ~FL_ONGROUND;
	}

// if onground, return without moving
	if ( ( ent->v.flags & FL_ONGROUND ) && VectorCompare( ent->v.velocity, vec_origin ) )
	{
		VectorCopy( vec3_origin, ent->v.avelocity );

		if ( VectorCompare( ent->v.basevelocity, vec_origin ) )
			return;	// at rest
	}

	SV_CheckVelocity( ent );

// add gravity
	if ( ent->v.movetype != MOVETYPE_FLY && ent->v.movetype != MOVETYPE_BOUNCEMISSILE && ent->v.movetype != MOVETYPE_FLYMISSILE )
		SV_AddGravity( ent );

// move angles
	VectorMA( ent->v.angles, (float)host_frametime, ent->v.avelocity, ent->v.angles );

// move origin
	// Base velocity is not properly accounted for since this entity will move again after the bounce without
	// taking it into account
	VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	SV_CheckVelocity( ent );

	VectorScale( ent->v.velocity, (float)host_frametime, move );
	VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );

	trace = SV_PushEntity( ent, move );	// Should this clear basevelocity

	SV_CheckVelocity( ent );

	if ( trace.allsolid )
	{	// entity is trapped in another solid
		VectorCopy( vec3_origin, ent->v.velocity );
		VectorCopy( vec3_origin, ent->v.avelocity );
		return;
	}

	if ( trace.fraction == 1 )
	{
		SV_CheckWaterTransition( ent );
		return;
	}

	if ( ent->free )
		return;

	if ( ent->v.movetype == MOVETYPE_BOUNCE )
		backoff = 2.0 - ent->v.friction;
	else if ( ent->v.movetype == MOVETYPE_BOUNCEMISSILE )
		backoff = 2.0;	// A backoff of 2.0 is a reflection
	else
		backoff = 1;

	ClipVelocity( ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff );

// stop if on ground
	if ( trace.plane.normal[2] > 0.7 )
	{
		float	vel;

		VectorAdd( ent->v.velocity, ent->v.basevelocity, move );
		vel = DotProduct( move, move );

		if ( move[2] < sv_gravity.value * host_frametime )
		{
			// we're rolling on the ground, add static friction.
			ent->v.flags |= FL_ONGROUND;
			ent->v.groundentity = trace.ent;
			ent->v.velocity[2] = 0;
		}

		if ( vel < 900 || ( ent->v.movetype != MOVETYPE_BOUNCE && ent->v.movetype != MOVETYPE_BOUNCEMISSILE ) )
		{
			ent->v.flags |= FL_ONGROUND;
			ent->v.groundentity = trace.ent;
			VectorCopy( vec3_origin, ent->v.velocity );
			VectorCopy( vec3_origin, ent->v.avelocity );
		}
		else
		{
			VectorScale( ent->v.velocity, (float)( ( 1.0 - trace.fraction ) * host_frametime * 0.9 ), move );
			VectorMA( move, (float)( ( 1.0 - trace.fraction ) * host_frametime * 0.9 ), ent->v.basevelocity, move );
			trace = SV_PushEntity( ent, move );
		}
	}

// check for in water
	SV_CheckWaterTransition( ent );
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
============
PF_WaterMove

============
*/
void PF_WaterMove( edict_t *pSelf )
{
	int		flags;
	int		waterlevel;
	int		watertype;
	float	drownlevel;
	float	dmgamount_guess;

	dmgamount_guess = 0;

	if ( pSelf->v.movetype == MOVETYPE_NOCLIP )
	{
		pSelf->v.air_finished = sv.time + 12;
		return;
	}

	if ( pSelf->v.health < 0 )
		return;

	if ( pSelf->v.deadflag == DEAD_NO )
		drownlevel = 3;
	else
		drownlevel = 1;

	flags = pSelf->v.flags;
	waterlevel = pSelf->v.waterlevel;
	watertype = pSelf->v.watertype;

	if ( !( flags & ( FL_IMMUNE_WATER | FL_GODMODE ) ) )
	{
		if ( ( ( flags & FL_SWIM ) && ( waterlevel < drownlevel ) ) || ( waterlevel >= drownlevel ) )
		{
			if ( pSelf->v.air_finished < sv.time && pSelf->v.pain_finished < sv.time )
			{
				// drown!
				pSelf->v.dmg += 2;
				if ( pSelf->v.dmg > 15 )
					pSelf->v.dmg = 10;
				dmgamount_guess = pSelf->v.dmg;
				pSelf->v.pain_finished = sv.time + 1;
			}
		}
		else
		{
			if ( pSelf->v.air_finished < sv.time )
			{
				// gasp for air
			}
			else if ( pSelf->v.air_finished < sv.time + 9 )
			{
				// just a little out of breath
			}
			pSelf->v.air_finished = sv.time + 12;
			pSelf->v.dmg = 2;
		}
	}

	if ( !waterlevel )
	{
		if ( flags & FL_INWATER )
		{
			// play leave water sound
			switch ( RandomLong( 0, 3 ) )
			{
			case 0:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade1.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 1:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade2.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 2:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade3.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 3:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade4.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			}

			pSelf->v.flags = flags & ~FL_INWATER;
		}

		pSelf->v.air_finished = sv.time + 12;
		return;
	}

	if ( watertype == CONTENTS_LAVA )
	{	// do damage
		if ( !( flags & ( FL_IMMUNE_LAVA | FL_GODMODE ) ) && pSelf->v.dmgtime < sv.time )
		{
			if ( pSelf->v.radsuit_finished < sv.time )
				pSelf->v.dmgtime = sv.time + 0.2;
			else
				pSelf->v.dmgtime = sv.time + 1;

			dmgamount_guess = 10 * waterlevel;
		}
	}
	else if ( watertype == CONTENTS_SLIME )
	{	// do damage
		if ( !( flags & ( FL_IMMUNE_SLIME | FL_GODMODE ) ) && pSelf->v.dmgtime < sv.time && pSelf->v.radsuit_finished < sv.time )
		{
			pSelf->v.dmgtime = sv.time + 1;
			dmgamount_guess = 4 * waterlevel;
		}
	}

	if ( !( flags & FL_INWATER ) )
	{
		// player enter water sound
		if ( watertype == CONTENTS_WATER )
		{
			switch ( RandomLong( 0, 3 ) )
			{
			case 0:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade1.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 1:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade2.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 2:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade3.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			case 3:
				SV_StartSound( 0, pSelf, CHAN_BODY, "player/pl_wade4.wav", 255, ATTN_NORM, 0, PITCH_NORM );
				break;
			}
		}

		pSelf->v.flags = flags | FL_INWATER;
		pSelf->v.dmgtime = 0;
	}

	if ( !( flags & FL_WATERJUMP ) )
	{
		VectorMA( pSelf->v.velocity, (float)( -0.8 * pSelf->v.waterlevel * host_frametime ), pSelf->v.velocity, pSelf->v.velocity );
	}
}

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step( edict_t *ent )
{
	qboolean	wasonground;
	qboolean	inwater;
	qboolean	hitsound = false;
	float		*vel;
	float		speed, newspeed, control;
	float		friction;

	PF_WaterMove( ent );
	SV_CheckVelocity( ent );

	wasonground = ent->v.flags & FL_ONGROUND;
	inwater = SV_CheckWater( ent );

	if ( ( ent->v.flags & FL_FLOAT ) && ent->v.waterlevel > 0 )
	{
		float	waterLevel;

		waterLevel = SV_Submerged( ent ) * ent->v.skin * host_frametime;
		SV_AddGravity( ent );
		ent->v.velocity[2] += waterLevel;
	}

	// add gravity except:
	//   flying monsters
	//   swimming monsters who are in the water
	if ( !wasonground && !( ent->v.flags & FL_FLY ) && !( ( ent->v.flags & FL_SWIM ) && ( ent->v.waterlevel > 0 ) ) )
	{
		if ( ent->v.velocity[2] < sv_gravity.value * -0.1 )
			hitsound = true;

		if ( !inwater )
			SV_AddGravity( ent );
	}

	if ( !VectorCompare( ent->v.velocity, vec_origin ) || !VectorCompare( ent->v.basevelocity, vec_origin ) )
	{
		ent->v.flags &= ~FL_ONGROUND;

		// apply friction
		// let dead monsters who aren't completely onground slide
		if ( wasonground && ( ent->v.health > 0.0 || SV_CheckBottom( ent ) ) )
		{
			vel = ent->v.velocity;
			speed = sqrt( vel[0] * vel[0] + vel[1] * vel[1] );
			if ( speed )
			{
				friction = sv_friction.value * ent->v.friction;
				ent->v.friction = 1;

				control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
				newspeed = speed - host_frametime * control * friction;

				if ( newspeed < 0 )
					newspeed = 0;
				newspeed /= speed;

				vel[0] = vel[0] * newspeed;
				vel[1] = vel[1] * newspeed;
			}
		}

		VectorAdd( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		SV_CheckVelocity( ent );

		SV_FlyMove( ent, (float)host_frametime, NULL );
		SV_CheckVelocity( ent );

		VectorSubtract( ent->v.velocity, ent->v.basevelocity, ent->v.velocity );
		SV_CheckVelocity( ent );

		// determine if it's on solid ground at all
		{
			vec3_t	mins, maxs, point;
			int		x, y;

			VectorAdd( ent->v.origin, ent->v.mins, mins );
			VectorAdd( ent->v.origin, ent->v.maxs, maxs );

			point[2] = mins[2] - 1;
			for ( x = 0; x <= 1; x++ )
			{
				for ( y = 0; y <= 1; y++ )
				{
					point[0] = x ? maxs[0] : mins[0];
					point[1] = y ? maxs[1] : mins[1];

					g_groupmask = ent->v.groupinfo;
					if ( SV_PointContents( point ) == CONTENTS_SOLID )
					{
						ent->v.flags |= FL_ONGROUND;
						break;
					}
				}
			}
		}

		SV_LinkEdict( ent, TRUE );

		if ( ent->v.flags & FL_ONGROUND )	// just hit ground
		{
			if ( hitsound )
			{
//				SV_StartSound( 0, ent, CHAN_AUTO, "demon/dland2.wav", 255, 1, 0, PITCH_NORM );
			}
		}
	}
	else
	{
		if ( gGlobalVariables.force_retouch )
		{
			trace_t	trace;

			trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent, ( ent->v.flags & FL_MONSTERCLIP ) ? true : false );

			// hentacle impact code
			if ( ( trace.fraction < 1.0f || trace.startsolid ) && trace.ent )
			{
				SV_Impact( ent, trace.ent, &trace );
			}
		}
	}

// regular thinking
	SV_RunThink( ent );
	SV_CheckWaterTransition( ent );
}

//============================================================================

/*
================
SV_Physics

================
*/
void SV_Physics( void )
{
	int		i;
	edict_t	*ent;
	edict_t	*groundentity;

// let the progs know that a new frame has started
	gGlobalVariables.time = sv.time;
	gEntityInterface.pfnStartFrame();

//
// treat each object in turn
//
	for ( i = 0; i < sv.num_edicts; i++ )
	{
		ent = &sv.edicts[i];
		if ( ent->free )
			continue;

		if ( gGlobalVariables.force_retouch )
		{
			SV_LinkEdict( ent, TRUE );	// force retouch even for stationary
		}

		if ( i > 0 && i <= svs.maxclients )
			continue;

		if ( ent->v.flags & FL_ONGROUND )
		{
			groundentity = ent->v.groundentity;
			if ( groundentity && ( groundentity->v.flags & FL_CONVEYOR ) )
			{
				if ( ent->v.flags & FL_BASEVELOCITY )
					VectorMA( ent->v.basevelocity, groundentity->v.speed, groundentity->v.movedir, ent->v.basevelocity );
				else
					VectorScale( groundentity->v.movedir, groundentity->v.speed, ent->v.basevelocity );
				ent->v.flags |= FL_BASEVELOCITY;
			}
		}

		if ( !( ent->v.flags & FL_BASEVELOCITY ) )
		{
			// Apply momentum (add in half of the previous frame of velocity first)
			VectorMA( ent->v.velocity, (float)( 1.0 + ( host_frametime * 0.5 ) ), ent->v.basevelocity, ent->v.velocity );
			VectorCopy( vec3_origin, ent->v.basevelocity );
		}

		ent->v.flags &= ~FL_BASEVELOCITY;

		if ( ent->v.movetype == MOVETYPE_PUSH )
			SV_Physics_Pusher( ent );
		else if ( ent->v.movetype == MOVETYPE_NONE )
			SV_Physics_None( ent );
		else if ( ent->v.movetype == MOVETYPE_FOLLOW )
			SV_Physics_Follow( ent );
		else if ( ent->v.movetype == MOVETYPE_NOCLIP )
			SV_Physics_Noclip( ent );
		else if ( ent->v.movetype == MOVETYPE_STEP )
			SV_Physics_Step( ent );
		else if ( ent->v.movetype == MOVETYPE_PUSHSTEP )
			SV_Physics_Step( ent );
		else if ( ent->v.movetype == MOVETYPE_TOSS
			|| ent->v.movetype == MOVETYPE_BOUNCE
			|| ent->v.movetype == MOVETYPE_BOUNCEMISSILE
			|| ent->v.movetype == MOVETYPE_FLY
			|| ent->v.movetype == MOVETYPE_FLYMISSILE )
			SV_Physics_Toss( ent );
		else
			Sys_Error( "SV_Physics: %s bad movetype %d", pr_strings + ent->v.classname, ent->v.movetype );

		if ( ent->v.flags & FL_KILLME )
			ED_Free( ent );
	}

	if ( gGlobalVariables.force_retouch )
		gGlobalVariables.force_retouch--;
}

/*
================
SV_Trace_Toss

================
*/
trace_t SV_Trace_Toss( edict_t *ent, edict_t *ignore )
{
	edict_t	tempent, *tent;
	trace_t	trace;
	vec3_t	move;
	vec3_t	end;
	double	save_frametime;

	save_frametime = host_frametime;
	host_frametime = 0.05;

	Q_memcpy( &tempent, ent, sizeof( edict_t ) );
	tent = &tempent;

	while ( 1 )
	{
		SV_CheckVelocity( tent );
		SV_AddGravity( tent );
		VectorMA( tent->v.angles, (float)host_frametime, tent->v.avelocity, tent->v.angles );
		VectorScale( tent->v.velocity, (float)host_frametime, move );
		VectorAdd( tent->v.origin, move, end );
		trace = SV_Move( tent->v.origin, tent->v.mins, tent->v.maxs, end, MOVE_NORMAL, tent, FALSE );
		VectorCopy( trace.endpos, tent->v.origin );

		if ( trace.ent && trace.ent != ignore )
			break;
	}

	host_frametime = save_frametime;
	return trace;
}
