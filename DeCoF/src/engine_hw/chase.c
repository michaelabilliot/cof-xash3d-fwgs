// chase.c -- chase camera code

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "world.h"
#include "chase.h"

cvar_t	chase_back = { "chase_back", "100", FCVAR_SPONLY };
cvar_t	chase_up = { "chase_up", "16", FCVAR_SPONLY };
cvar_t	chase_right = { "chase_right", "0", FCVAR_SPONLY };
cvar_t	chase_active = { "chase_active", "0", FCVAR_SPONLY };

vec3_t	chase_pos;
vec3_t	chase_angles;

vec3_t	chase_dest;
vec3_t	chase_dest_angles;

/*
=================
Chase_Init
=================
*/
void Chase_Init( void )
{
	Cvar_RegisterVariable( &chase_back );
	Cvar_RegisterVariable( &chase_up );
	Cvar_RegisterVariable( &chase_right );
	Cvar_RegisterVariable( &chase_active );
}

/*
=================
Chase_Reset
=================
*/
void Chase_Reset( void )
{
	// for respawning and teleporting
//	start position 12 units behind head
}

/*
=================
TraceLine
=================
*/
void TraceLine( vec_t *start, vec_t *end, vec_t *impact )
{
	trace_t	trace;

	Q_memset( &trace, 0, sizeof( trace ) );
	SV_RecursiveHullCheck( cl.worldmodel->hulls, 0, 0, 1, start, end, &trace );

	VectorCopy( trace.endpos, impact );
}

/*
=================
Chase_Update
=================
*/
void Chase_Update( void )
{
	int		i;
	float	dist;
	vec3_t	forward, up, right;
	vec3_t	dest, stop;

	// if can't see player, reset
	AngleVectors( cl.viewangles, forward, right, up );

	// calc exact destination
	for ( i = 0; i < 3; i++ )
		chase_dest[i] = r_refdef.vieworg[i]
		- forward[i] * chase_back.value
		- right[i] * chase_right.value;
	chase_dest[2] = r_refdef.vieworg[2] + chase_up.value;

	// find the spot the player is looking at
	VectorMA( r_refdef.vieworg, 4096, forward, dest );
	TraceLine( r_refdef.vieworg, dest, stop );

	// calculate pitch to look at the same spot from camera
	VectorSubtract( stop, r_refdef.vieworg, stop );
	dist = DotProduct( stop, forward );
	if ( dist < 1 )
		dist = 1;
	r_refdef.viewangles[PITCH] = -atan( stop[2] / dist ) / M_PI * 180;

	// move towards destination
	VectorCopy( chase_dest, r_refdef.vieworg );
}
