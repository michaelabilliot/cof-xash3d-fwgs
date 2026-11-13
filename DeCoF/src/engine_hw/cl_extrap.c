// cl_extrap.c -- position history and interpolation of client entities

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"

cvar_t	ex_interp = { "ex_interp", "0.1" };

/*
=================
CL_InitExtrap
=================
*/
void CL_InitExtrap( void )
{
	Cvar_RegisterVariable( &ex_interp );
}

/*
=================
CL_FindInterpolationUpdates
=================
*/
qboolean CL_FindInterpolationUpdates( cl_entity_t *ent, float targettime, position_history_t **ph0, position_history_t **ph1, int *ph0Index )
{
	qboolean	extrapolate;
	int			i0, i1;
	int			i;
	float		at;
	int			imod;

	extrapolate = true;

	i0 = ent->current_position & HISTORY_MASK;
	i1 = ( ent->current_position - 1 ) & HISTORY_MASK;

	if ( targettime <= ent->ph[i0].animtime )
	{
		for ( i = 1; i < HISTORY_MAX - 1; i++ )
		{
			imod = ( ent->current_position - i ) & HISTORY_MASK;
			at = ent->ph[imod].animtime;

			if ( at == 0.0 )
				break;

			if ( targettime <= at )
				continue;

			i0 = ( ent->current_position - i + 1 ) & HISTORY_MASK;
			i1 = imod;
			extrapolate = false;
			break;
		}
	}

	if ( ph0 )
		*ph0 = &ent->ph[i0];

	if ( ph1 )
		*ph1 = &ent->ph[i1];

	if ( ph0Index )
		*ph0Index = i0;

	return extrapolate;
}

/*
=================
CL_BoundInterpolationFraction
=================
*/
void CL_BoundInterpolationFraction( float *frac )
{
	if ( *frac < 0.0 )
	{
		*frac = 0.0;
	}
	else if ( *frac > 1.2 )
	{
		*frac = 1.2;
	}
}

/*
=================
CL_UpdatePositions
=================
*/
void CL_UpdatePositions( cl_entity_t *ent )
{
	position_history_t	*ph;

	ent->current_position = ( ent->current_position + 1 ) & HISTORY_MASK;

	ph = &ent->ph[ent->current_position];

	VectorCopy( ent->curstate.angles, ph->angles );
	VectorCopy( ent->curstate.origin, ph->origin );
	ph->animtime = ent->curstate.animtime;
}

/*
=================
CL_ResetPositions
=================
*/
void CL_ResetPositions( cl_entity_t *ent )
{
	position_history_t	store;

	if ( !ent )
		return;

	store = ent->ph[ent->current_position];

	ent->current_position = 1;
	Q_memset( ent->ph, 0, sizeof( position_history_t ) * HISTORY_MAX );
	Q_memcpy( &ent->ph[1], &store, sizeof( position_history_t ) );
	Q_memcpy( &ent->ph[0], &store, sizeof( position_history_t ) );
}

/*
=================
CL_PureOrigin
=================
*/
void CL_PureOrigin( cl_entity_t *ent, float t, vec_t *outorigin, vec_t *outangles )
{
	qboolean			extrapolate;
	position_history_t	*ph0, *ph1;
	float				t1, t0;
	float				frac;
	vec3_t				delta;
	vec3_t				pos, angles;

	extrapolate = true;
	extrapolate = CL_FindInterpolationUpdates( ent, t, &ph0, &ph1, NULL );

	if ( !ph0 || !ph1 )
		return;

	t1 = ph1->animtime;
	t0 = ph0->animtime;

	if ( t0 == 0 )
	{
		VectorCopy( ph1->origin, outorigin );
		VectorCopy( ph1->angles, outangles );
		return;
	}

	VectorSubtract( ph0->origin, ph1->origin, delta );

	if ( t0 == t1 )
	{
		frac = 1.0;
	}
	else
	{
		frac = ( t - t1 ) / ( t0 - t1 );
	}

	CL_BoundInterpolationFraction( &frac );

	VectorMA( ph1->origin, frac, delta, pos );
	InterpolateAngles( ph1->angles, ph0->angles, angles, frac );

	VectorCopy( pos, outorigin );
	VectorCopy( angles, outangles );
}

/*
=================
CL_ComputePlayerOrigin
=================
*/
void CL_ComputePlayerOrigin( cl_entity_t *ent )
{
	vec3_t	target_origin;
	vec3_t	angles;
	float	targettime;

	if ( !ent->player )
		return;

	if ( ent->index - 1 == cl.playernum )
		return;

	targettime = cl.time - ex_interp.value;

	CL_PureOrigin( ent, targettime, target_origin, angles );

	VectorCopy( angles, ent->angles );
	VectorCopy( target_origin, ent->origin );
}

/*
=================
CL_InterpolateEntity
=================
*/
void CL_InterpolateEntity( cl_entity_t *ent )
{
	vec3_t	target_origin;
	vec3_t	angles;
	float	targettime;

	targettime = cl.time - ex_interp.value;

	CL_PureOrigin( ent, targettime, target_origin, angles );

	VectorCopy( angles, ent->angles );
	VectorCopy( target_origin, ent->origin );
}
