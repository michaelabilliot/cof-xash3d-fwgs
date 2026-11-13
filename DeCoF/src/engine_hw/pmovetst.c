// pmovetst.c -- player movement collision tests: point contents, positions
// and traces against the physents of the current pmove

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "GL_MODEL.H"
#include "pmove.h"
#include "world.h"
#include "studio.h"
#include "APIProxy.h"

static int	g_contentsresult;

hull_t		box_hull;
dclipnode_t	box_clipnodes[6];
mplane_t	box_planes[6];

float PM_TraceModel( physent_t *pEnt, vec_t *start, vec_t *end, trace_t *trace )
{
	hull_t	*pHull;
	int		saveHull;
	vec3_t	start_l, end_l, offset;

	saveHull = pmove->usehull;
	pmove->usehull = 2;
	pHull = PM_HullForBsp( pEnt, offset );
	pmove->usehull = saveHull;

	start_l[0] = start[0] - offset[0];
	start_l[1] = start[1] - offset[1];
	start_l[2] = start[2] - offset[2];
	end_l[0] = end[0] - offset[0];
	end_l[1] = end[1] - offset[1];
	end_l[2] = end[2] - offset[2];

	SV_RecursiveHullCheck( pHull, pHull->firstclipnode, 0, 1, start_l, end_l, trace );
	trace->ent = NULL;
	return trace->fraction;
}

void PM_GetModelBounds( struct model_s *mod, vec_t *mins, vec_t *maxs )
{
	mins[0] = mod->mins[0];
	mins[1] = mod->mins[1];
	mins[2] = mod->mins[2];
	maxs[0] = mod->maxs[0];
	maxs[1] = mod->maxs[1];
	maxs[2] = mod->maxs[2];
}

int PM_GetModelType( struct model_s *mod )
{
	return mod->type;
}

/*
===================
PM_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void PM_InitBoxHull( void )
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for ( i = 0; i < 6; i++ )
	{
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if ( i != 5 )
			box_clipnodes[i].children[side^1] = i + 1;
		else
			box_clipnodes[i].children[side^1] = CONTENTS_SOLID;

		box_planes[i].type = i >> 1;
		box_planes[i].normal[i>>1] = 1;
	}
}

/*
===================
PM_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t *PM_HullForBox( vec_t *mins, vec_t *maxs )
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

/*
==================
PM_HullPointContents

==================
*/
int PM_HullPointContents( hull_t *hull, int num, vec_t *p )
{
	float		d;
	dclipnode_t	*node;
	mplane_t	*plane;

	if ( hull->firstclipnode >= hull->lastclipnode )
		return CONTENTS_EMPTY;

	while ( num >= 0 )
	{
		if ( num < hull->firstclipnode || num > hull->lastclipnode )
			Sys_Error( "PM_HullPointContents: bad node number" );

		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct( plane->normal, p ) - plane->dist;

		if ( d < 0 )
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

int PM_LinkContents( vec_t *p, int *pIndex )
{
	int			i;
	physent_t	*pe;
	vec3_t		test;
	hull_t		*hull;
	int			num;
	int			cont;

	for ( i = 1; i < pmove->numphysent; i++ )
	{
		pe = &pmove->physents[i];

		if ( pe->solid != SOLID_NOT )
			continue;

		if ( !pe->model )
			continue;

		hull = &pe->model->hulls[0];
		num = hull->firstclipnode;

		VectorSubtract( p, pe->origin, test );

		cont = PM_HullPointContents( hull, num, test );
		if ( cont == CONTENTS_EMPTY )
			continue;

		if ( pIndex )
			*pIndex = pe->info;
		return pe->skin;
	}

	return CONTENTS_EMPTY;
}

/*
==================
PM_PointContents

==================
*/
int PM_PointContents( vec_t *p, int *truecontents )
{
	hull_t	*hull;
	int		num;
	int		cont, entityContents;

	g_engdstAddrs.PM_PointContents( &p, &truecontents );

	hull = &pmove->physents[0].model->hulls[0];
	if ( !hull )
	{
		if ( truecontents )
			*truecontents = CONTENTS_EMPTY;
		return CONTENTS_SOLID;
	}

	num = hull->firstclipnode;

	cont = PM_HullPointContents( hull, num, p );
	if ( truecontents )
		*truecontents = cont;

	if ( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;

	if ( cont != CONTENTS_SOLID )
	{
		entityContents = PM_LinkContents( p, NULL );
		if ( entityContents != CONTENTS_EMPTY )
			return entityContents;
	}

	return cont;
}

int PM_WaterEntity( vec_t *p )
{
	hull_t	*hull;
	int		num, entityIndex;
	int		cont;

	g_engdstAddrs.PM_WaterEntity( &p );

	entityIndex = -1;

	hull = &pmove->physents[0].model->hulls[0];
	num = hull->firstclipnode;

	cont = PM_HullPointContents( hull, num, p );
	if ( cont < CONTENTS_SOLID )
		entityIndex = 0;

	if ( cont != CONTENTS_SOLID )
		PM_LinkContents( p, &entityIndex );

	return entityIndex;
}

/*
==================
PM_TruePointContents

==================
*/
int PM_TruePointContents( vec_t *p )
{
	hull_t	*hull;

	hull = &pmove->physents[0].model->hulls[0];
	if ( hull )
		return PM_HullPointContents( hull, hull->firstclipnode, p );

	return CONTENTS_EMPTY;
}

hull_t *PM_HullForStudioModel( model_t *pModel, vec_t *offset, float frame, int sequence, const vec_t *angles, const vec_t *origin, const byte *pcontroller, const byte *pblending, int *pNumHulls )
{
	vec3_t	size;

	VectorSubtract( player_maxs[pmove->usehull], player_mins[pmove->usehull], size );
	VectorScale( size, 0.5, size );

	offset[0] = 0;
	offset[1] = 0;
	offset[2] = 0;

	return R_StudioHull( pModel, frame, sequence, angles, origin, size, pcontroller, pblending, pNumHulls, NULL, 0 );
}

/*
==================
PM_HullForBsp

==================
*/
hull_t *PM_HullForBsp( physent_t *pe, vec_t *offset )
{
	hull_t	*hull;

	switch ( pmove->usehull )
	{
	default:
		hull = &pe->model->hulls[1];
		break;
	case 1:
		hull = &pe->model->hulls[3];
		break;
	case 2:
		hull = &pe->model->hulls[0];
		break;
	case 3:
		hull = &pe->model->hulls[2];
		break;
	}

	// calculate an offset value to center the origin
	VectorSubtract( hull->clip_mins, player_mins[pmove->usehull], offset );
	VectorAdd( offset, pe->origin, offset );

	return hull;
}

/*
================
_PM_TestPlayerPosition

Returns the index of the physent the position is stuck in, or -1
================
*/
int _PM_TestPlayerPosition( vec_t *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) )
{
	int			i;
	physent_t	*pe;
	vec3_t		mins, maxs, test;
	vec3_t		offset;
	hull_t		*hull;
	qboolean	rotated;
	int			j, numhulls;
	pmtrace_t	tr;

	tr = PM_PlayerTrace( pmove->origin, pmove->origin, PM_NORMAL, -1 );
	if ( ptrace )
		*ptrace = tr;

	for ( i = 0; i < pmove->numphysent; i++ )
	{
		pe = &pmove->physents[i];

		if ( pfnIgnore && pfnIgnore( pe ) )
			continue;

		// Don't test against the water and other non solid brush models
		if ( pe->model && pe->solid == SOLID_NOT && pe->skin != 0 )
			continue;

		VectorCopy( pe->origin, offset );
		numhulls = 1;

		if ( pe->model )
		{
			hull = PM_HullForBsp( pe, offset );
		}
		else if ( pe->studiomodel && pe->studiomodel->type == mod_studio
			&& ( ( pe->studiomodel->flags & STUDIO_TRACE_HITBOX ) || pmove->usehull == 2 ) )
		{
			hull = PM_HullForStudioModel( pe->studiomodel, offset, pe->frame, pe->sequence, pe->angles, pe->origin, pe->controller, pe->blending, &numhulls );
		}
		else
		{
			VectorSubtract( pe->mins, player_maxs[pmove->usehull], mins );
			VectorSubtract( pe->maxs, player_mins[pmove->usehull], maxs );
			hull = PM_HullForBox( mins, maxs );
		}

		// Offset the test point appropriately for this hull.
		VectorSubtract( pos, offset, test );

		// CHECK FOR ROTATION
		if ( pe->solid == SOLID_BSP && ( pe->angles[0] || pe->angles[1] || pe->angles[2] ) )
			rotated = true;
		else
			rotated = false;

		if ( rotated )
		{
			vec3_t	forward, right, up;
			vec3_t	temp;

			AngleVectors( pe->angles, forward, right, up );

			VectorCopy( test, temp );
			test[0] = DotProduct( temp, forward );
			test[1] = -DotProduct( temp, right );
			test[2] = DotProduct( temp, up );
		}

		// Test the player's hull for intersection with this model
		if ( numhulls == 1 )
		{
			g_contentsresult = PM_HullPointContents( hull, hull->firstclipnode, test );
			if ( g_contentsresult == CONTENTS_SOLID )
				return i;
		}
		else
		{
			for ( j = 0; j < numhulls; j++ )
			{
				g_contentsresult = PM_HullPointContents( &hull[j], hull[j].firstclipnode, test );
				if ( g_contentsresult == CONTENTS_SOLID )
					return i;
			}
		}
	}

	return -1;	// Not sitting in a solid
}

int PM_TestPlayerPosition( vec_t *pos, pmtrace_t *ptrace )
{
	return _PM_TestPlayerPosition( pos, ptrace, NULL );
}

int PM_TestPlayerPositionEx( vec_t *pos, pmtrace_t *ptrace, int ( *pfnIgnore )( physent_t *pe ) )
{
	return _PM_TestPlayerPosition( pos, ptrace, pfnIgnore );
}

/*
================
_PM_PlayerTrace

================
*/
pmtrace_t _PM_PlayerTrace( vec_t *start, vec_t *end, int traceFlags, int numphysent, physent_t *physents, int ignore_pe, int ( *pfnIgnore )( physent_t *pe ) )
{
	int			i;
	pmtrace_t	trace, total;
	vec3_t		offset;
	vec3_t		start_l, end_l;
	hull_t		*hull;
	physent_t	*pe;
	vec3_t		mins, maxs;
	qboolean	rotated;
	int			numhulls, j;

	// Fill in a default trace
	Q_memset( &total, 0, sizeof( pmtrace_t ) );
	total.fraction = 1.0;
	total.ent = -1;
	VectorCopy( end, total.endpos );

	for ( i = 0; i < numphysent; i++ )
	{
		pe = &physents[i];

		// Only want to clip to the world?
		if ( i != 0 && ( traceFlags & PM_WORLD_ONLY ) )
			break;

		// Run a custom test, or skip the physent the caller wants ignored
		if ( pfnIgnore )
		{
			if ( pfnIgnore( pe ) )
				continue;
		}
		else if ( ignore_pe != -1 && i == ignore_pe )
			continue;

		if ( pe->model && pe->solid == SOLID_NOT && pe->skin != 0 )
			continue;

		if ( ( traceFlags & PM_GLASS_IGNORE ) && pe->rendermode != kRenderNormal )
			continue;

		// PM_HullForEntity( ent, mins, maxs, offset );
		VectorCopy( pe->origin, offset );

		numhulls = 1;
		if ( pe->model )
		{
			switch ( pmove->usehull )
			{
			default:
				hull = &pe->model->hulls[1];
				break;
			case 1:
				hull = &pe->model->hulls[3];
				break;
			case 2:
				hull = &pe->model->hulls[0];
				break;
			case 3:
				hull = &pe->model->hulls[2];
				break;
			}

			VectorSubtract( hull->clip_mins, player_mins[pmove->usehull], offset );
			VectorAdd( offset, pe->origin, offset );
		}
		else
		{
			if ( pe->studiomodel )
			{
				if ( traceFlags & PM_STUDIO_IGNORE )
					continue;

				if ( pe->studiomodel->type == mod_studio && ( ( pe->studiomodel->flags & STUDIO_TRACE_HITBOX )
					|| ( pmove->usehull == 2 && !( traceFlags & PM_STUDIO_BOX ) ) ) )
				{
					hull = PM_HullForStudioModel( pe->studiomodel, offset, pe->frame, pe->sequence, pe->angles, pe->origin, pe->controller, pe->blending, &numhulls );
				}
				else
				{
					VectorSubtract( pe->mins, player_maxs[pmove->usehull], mins );
					VectorSubtract( pe->maxs, player_mins[pmove->usehull], maxs );
					hull = PM_HullForBox( mins, maxs );
				}
			}
			else
			{
				VectorSubtract( pe->mins, player_maxs[pmove->usehull], mins );
				VectorSubtract( pe->maxs, player_mins[pmove->usehull], maxs );
				hull = PM_HullForBox( mins, maxs );
			}
		}

		VectorSubtract( start, offset, start_l );
		VectorSubtract( end, offset, end_l );

		if ( pe->solid == SOLID_BSP && ( pe->angles[0] || pe->angles[1] || pe->angles[2] ) )
			rotated = true;
		else
			rotated = false;

		if ( rotated )
		{
			vec3_t	forward, right, up;
			vec3_t	temp;

			AngleVectors( pe->angles, forward, right, up );

			VectorCopy( start_l, temp );
			start_l[0] = DotProduct( temp, forward );
			start_l[1] = -DotProduct( temp, right );
			start_l[2] = DotProduct( temp, up );

			VectorCopy( end_l, temp );
			end_l[0] = DotProduct( temp, forward );
			end_l[1] = -DotProduct( temp, right );
			end_l[2] = DotProduct( temp, up );
		}

		// Fill in a default trace
		Q_memset( &trace, 0, sizeof( pmtrace_t ) );
		trace.fraction = 1;
		trace.allsolid = true;
		VectorCopy( end, trace.endpos );

		if ( numhulls <= 0 )
		{
			trace.allsolid = false;
		}
		else if ( numhulls == 1 )
		{
			PM_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, &trace );
		}
		else
		{
			int			closest;
			pmtrace_t	testtrace;

			closest = 0;
			for ( j = 0; j < numhulls; j++ )
			{
				Q_memset( &testtrace, 0, sizeof( pmtrace_t ) );
				testtrace.fraction = 1;
				testtrace.allsolid = true;
				VectorCopy( end, testtrace.endpos );

				PM_RecursiveHullCheck( &hull[j], hull[j].firstclipnode, 0, 1, start_l, end_l, &testtrace );

				if ( j == 0 || testtrace.allsolid || testtrace.startsolid || testtrace.fraction < trace.fraction )
				{
					if ( trace.startsolid )
					{
						trace = testtrace;
						trace.startsolid = true;
					}
					else
					{
						trace = testtrace;
					}
					closest = j;
				}
				trace.hitgroup = SV_HitgroupForStudioHull( closest );
			}
		}

		if ( trace.allsolid )
			trace.startsolid = true;

		if ( trace.startsolid )
			trace.fraction = 0;

		// Did we clip the move?
		if ( trace.fraction != 1.0 )
		{
			if ( rotated )
			{
				vec3_t	forward, right, up;
				vec3_t	temp;

				AngleVectorsTranspose( pe->angles, forward, right, up );

				VectorCopy( trace.plane.normal, temp );
				trace.plane.normal[0] = DotProduct( temp, forward );
				trace.plane.normal[1] = DotProduct( temp, right );
				trace.plane.normal[2] = DotProduct( temp, up );
			}

			trace.endpos[0] = start[0] + trace.fraction * ( end[0] - start[0] );
			trace.endpos[1] = start[1] + trace.fraction * ( end[1] - start[1] );
			trace.endpos[2] = start[2] + trace.fraction * ( end[2] - start[2] );
		}

		if ( trace.fraction < total.fraction )
		{
			// Copy the trace and remember the physent
			total = trace;
			total.ent = i;
		}
	}

	return total;
}

pmtrace_t PM_PlayerTrace( vec_t *start, vec_t *end, int traceFlags, int ignore_pe )
{
	pmtrace_t	tr;

	tr = _PM_PlayerTrace( start, end, traceFlags, pmove->numphysent, pmove->physents, ignore_pe, NULL );
	return tr;
}

pmtrace_t PM_PlayerTraceEx( vec_t *start, vec_t *end, int traceFlags, int ( *pfnIgnore )( physent_t *pe ) )
{
	pmtrace_t	tr;

	tr = _PM_PlayerTrace( start, end, traceFlags, pmove->numphysent, pmove->physents, -1, pfnIgnore );
	return tr;
}

struct pmtrace_s *PM_TraceLine( float *start, float *end, int flags, int usehull, int ignore_pe )
{
	int					oldhull;
	static pmtrace_t	tr;

	g_engdstAddrs.PM_TraceLine( &start, &end, &flags, &usehull, &ignore_pe );

	oldhull = pmove->usehull;
	pmove->usehull = usehull;

	if ( flags == PM_TRACELINE_PHYSENTSONLY )
		tr = _PM_PlayerTrace( start, end, PM_NORMAL, pmove->numphysent, pmove->physents, ignore_pe, NULL );
	else if ( flags == PM_TRACELINE_ANYVISIBLE )
		tr = _PM_PlayerTrace( start, end, PM_NORMAL, pmove->numvisent, pmove->visents, ignore_pe, NULL );

	pmove->usehull = oldhull;

	return &tr;
}

struct pmtrace_s *PM_TraceLineEx( float *start, float *end, int flags, int usehull, int ( *pfnIgnore )( physent_t *pe ) )
{
	int					oldhull;
	static pmtrace_t	tr;

	oldhull = pmove->usehull;
	pmove->usehull = usehull;

	if ( flags == PM_TRACELINE_PHYSENTSONLY )
		tr = PM_PlayerTraceEx( start, end, PM_NORMAL, pfnIgnore );
	else
		tr = _PM_PlayerTrace( start, end, PM_NORMAL, pmove->numvisent, pmove->visents, -1, pfnIgnore );

	pmove->usehull = oldhull;

	return &tr;
}

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

/*
==================
PM_RecursiveHullCheck

==================
*/
qboolean PM_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec_t *p1, vec_t *p2, pmtrace_t *trace )
{
	dclipnode_t	*node;
	mplane_t	*plane;
	float		t1, t2;
	float		frac;
	int			i;
	vec3_t		mid;
	int			side;
	int			otherside;
	float		midf;
	qboolean	retval;

	// check for empty
	if ( num < 0 )
	{
		if ( num != CONTENTS_SOLID )
		{
			trace->allsolid = false;
			if ( num == CONTENTS_EMPTY )
				trace->inopen = true;
			else
				trace->inwater = true;
		}
		else
			trace->startsolid = true;
		return true;	// empty
	}

	if ( hull->firstclipnode >= hull->lastclipnode )
	{
		trace->allsolid = false;
		trace->inopen = true;
		return true;
	}

	//
	// find the point distances
	//
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if ( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if ( t1 >= 0 && t2 >= 0 )
	{
		retval = PM_RecursiveHullCheck( hull, node->children[0], p1f, p2f, p1, p2, trace );
		return retval;
	}
	if ( t1 < 0 && t2 < 0 )
	{
		retval = PM_RecursiveHullCheck( hull, node->children[1], p1f, p2f, p1, p2, trace );
		return retval;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if ( t1 < 0 )
		frac = ( t1 + DIST_EPSILON ) / ( t1 - t2 );
	else
		frac = ( t1 - DIST_EPSILON ) / ( t1 - t2 );

	if ( frac < 0 )
		frac = 0;
	if ( frac > 1 )
		frac = 1;

	midf = p1f + ( p2f - p1f ) * frac;
	for ( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac * ( p2[i] - p1[i] );

	side = ( t1 < 0 );

	// move up to the node
	retval = PM_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace );
	if ( !retval )
		return false;

	otherside = side ^ 1;

	if ( PM_HullPointContents( hull, node->children[otherside], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		retval = PM_RecursiveHullCheck( hull, node->children[otherside], midf, p2f, mid, p2, trace );
		return retval;
	}

	if ( trace->allsolid )
		return false;	// never got out of the solid area

	//==================
	// the other side of the node is solid, this is the impact point
	//==================
	if ( !side )
	{
		VectorCopy( plane->normal, trace->plane.normal );
		trace->plane.dist = plane->dist;
	}
	else
	{
		VectorSubtract( vec3_origin, plane->normal, trace->plane.normal );
		trace->plane.dist = -plane->dist;
	}

	while ( PM_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		if ( ( frac -= 0.05 ) < 0 )
		{
			trace->fraction = midf;
			VectorCopy( mid, trace->endpos );
			Con_DPrintf( "Trace backed up past 0.0.\n" );
			return false;
		}

		midf = p1f + ( p2f - p1f ) * frac;
		for ( i = 0; i < 3; i++ )
			mid[i] = p1[i] + frac * ( p2[i] - p1[i] );
	}

	trace->fraction = midf;
	VectorCopy( mid, trace->endpos );

	return false;
}
