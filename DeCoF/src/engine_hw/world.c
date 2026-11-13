// world.c -- world query functions

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "world.h"
#include "studio.h"

#include <math.h>

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

// ClearLink is used for new headnodes
void ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

void RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore( link_t *l, link_t *before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter( link_t *l, link_t *after )
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============================================================================

HULL BOXES

===============================================================================
*/


static	hull_t		box_hull, beam_hull;
static	dclipnode_t	box_clipnodes[6];
static	mplane_t	box_planes[6], beam_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull( void )
{
	int		i;
	int		side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	beam_hull = box_hull;
	beam_hull.planes = beam_planes;

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

		beam_planes[i].type = 5;
	}
}

/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t *SV_HullForBox( const vec_t *mins, const vec_t *maxs )
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
===================
SV_HullForBeam

A box around the segment start..end, grown by size
===================
*/
hull_t *SV_HullForBeam( const vec_t *start, const vec_t *end, const vec_t *size )
{
	vec3_t	tmp;

	beam_planes[0].normal[0] = end[0] - start[0];
	beam_planes[0].normal[1] = end[1] - start[1];
	beam_planes[0].normal[2] = end[2] - start[2];
	VectorNormalize( beam_planes[0].normal );
	VectorCopy( beam_planes[0].normal, beam_planes[1].normal );

	beam_planes[0].dist = DotProduct( beam_planes[0].normal, end );
	beam_planes[1].dist = DotProduct( beam_planes[1].normal, start );

	if ( fabs( beam_planes[0].normal[2] ) < 0.9 )
	{
		tmp[2] = 1;
		tmp[0] = tmp[1] = 0;
	}
	else
	{
		tmp[0] = 1;
		tmp[1] = tmp[2] = 0;
	}

	CrossProduct( beam_planes[0].normal, tmp, beam_planes[2].normal );
	VectorNormalize( beam_planes[2].normal );
	VectorCopy( beam_planes[2].normal, beam_planes[3].normal );

	VectorAdd( beam_planes[2].normal, start, tmp );
	beam_planes[2].dist = DotProduct( tmp, beam_planes[2].normal );
	VectorSubtract( start, beam_planes[2].normal, tmp );
	beam_planes[3].dist = DotProduct( tmp, beam_planes[2].normal );

	CrossProduct( beam_planes[2].normal, beam_planes[0].normal, beam_planes[4].normal );
	VectorNormalize( beam_planes[4].normal );
	VectorCopy( beam_planes[4].normal, beam_planes[5].normal );

	VectorAdd( beam_planes[4].normal, start, tmp );
	beam_planes[4].dist = DotProduct( tmp, beam_planes[4].normal );
	VectorSubtract( start, beam_planes[4].normal, tmp );
	beam_planes[5].dist = DotProduct( tmp, beam_planes[4].normal );

	beam_planes[0].dist += fabs( beam_planes[0].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[0].normal[1] ) + fabs( (double)size[2] * beam_planes[0].normal[2] );
	beam_planes[1].dist -= fabs( beam_planes[1].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[1].normal[1] ) + fabs( (double)size[2] * beam_planes[1].normal[2] );
	beam_planes[2].dist += fabs( beam_planes[2].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[2].normal[1] ) + fabs( (double)size[2] * beam_planes[2].normal[2] );
	beam_planes[3].dist -= fabs( beam_planes[3].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[3].normal[1] ) + fabs( (double)size[2] * beam_planes[3].normal[2] );
	beam_planes[4].dist += fabs( beam_planes[4].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[4].normal[1] ) + fabs( (double)size[2] * beam_planes[4].normal[2] );
	beam_planes[5].dist -= fabs( beam_planes[5].normal[0] * size[0] ) + fabs( (double)size[1] * beam_planes[5].normal[1] ) + fabs( (double)size[2] * beam_planes[5].normal[2] );

	return &beam_hull;
}

/*
================
SV_HullForBsp

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
struct hull_s *SV_HullForBsp( edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset )
{
	model_t	*model;
	hull_t	*hull;
	vec3_t	size;

	model = sv.models[ent->v.modelindex];
	if ( !model )
		Sys_Error( "Hit a %s with no model (%s)", &pr_strings[ent->v.classname], &pr_strings[ent->v.model] );

	if ( !model || model->type != mod_brush )
	{
		Sys_Error( "Hit a %s with no model (%s)", &pr_strings[ent->v.classname], &pr_strings[ent->v.model] );
		Sys_Error( "MOVETYPE_PUSH with a non bsp model" );
	}

	VectorSubtract( maxs, mins, size );
	if ( size[0] <= 8 )
	{
		hull = &model->hulls[0];
		VectorCopy( hull->clip_mins, offset );
	}
	else
	{
		if ( size[0] <= 36 && size[2] <= 36 )
			hull = &model->hulls[3];
		else if ( size[0] <= 36 )
			hull = &model->hulls[1];
		else
			hull = &model->hulls[2];

		VectorSubtract( hull->clip_mins, mins, offset );
	}

	// calculate an offset value to center the origin
	VectorAdd( offset, ent->v.origin, offset );

	return hull;
}

/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t *SV_HullForEntity( edict_t *ent, const vec_t *mins, const vec_t *maxs, vec_t *offset )
{
	vec3_t	hullmins, hullmaxs;
	hull_t	*hull;

	// decide which clipping hull to use, based on the size
	if ( ent->v.solid == SOLID_BSP )
	{	// explicit hulls in the BSP model
		if ( ent->v.movetype != MOVETYPE_PUSH && ent->v.movetype != MOVETYPE_PUSHSTEP )
			Sys_Error( "SOLID_BSP without MOVETYPE_PUSH" );

		hull = SV_HullForBsp( ent, mins, maxs, offset );
	}
	else
	{	// create a temp hull from bounding box sizes
		VectorSubtract( ent->v.mins, maxs, hullmins );
		VectorSubtract( ent->v.maxs, mins, hullmaxs );
		hull = SV_HullForBox( hullmins, hullmaxs );

		VectorCopy( ent->v.origin, offset );
	}

	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

areanode_t	sv_areanodes[AREA_NODES];
int			sv_numareanodes;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t *SV_CreateAreaNode( int depth, vec_t *mins, vec_t *maxs )
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink( &anode->trigger_edicts );
	ClearLink( &anode->solid_edicts );

	if ( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract( maxs, mins, size );
	if ( size[0] > size[1] )
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = 0.5 * ( maxs[anode->axis] + mins[anode->axis] );
	VectorCopy( mins, mins1 );
	VectorCopy( mins, mins2 );
	VectorCopy( maxs, maxs1 );
	VectorCopy( maxs, maxs2 );

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode( depth + 1, mins2, maxs2 );
	anode->children[1] = SV_CreateAreaNode( depth + 1, mins1, maxs1 );

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld( void )
{
	SV_InitBoxHull();

	Q_memset( sv_areanodes, 0, sizeof( sv_areanodes ) );
	sv_numareanodes = 0;
	SV_CreateAreaNode( 0, sv.worldmodel->mins, sv.worldmodel->maxs );
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict( edict_t *ent )
{
	if ( !ent->area.prev )
		return;		// not linked in anywhere
	RemoveLink( &ent->area );
	ent->area.prev = ent->area.next = NULL;
}


/*
====================
SV_TouchLinks
====================
*/
void SV_TouchLinks( edict_t *ent, areanode_t *node )
{
	link_t		*l, *next;
	edict_t		*touch;
	model_t		*pModel;

	// touch linked edicts
	for ( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		if ( touch == ent )
			continue;

		if ( touch->v.groupinfo && ent->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( touch->v.groupinfo & ent->v.groupinfo ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( touch->v.groupinfo & ent->v.groupinfo )
					continue;
			}
		}

		if ( touch->v.solid != SOLID_TRIGGER )
			continue;

		if ( ent->v.absmin[0] > touch->v.absmax[0]
		|| ent->v.absmin[1] > touch->v.absmax[1]
		|| ent->v.absmin[2] > touch->v.absmax[2]
		|| ent->v.absmax[0] < touch->v.absmin[0]
		|| ent->v.absmax[1] < touch->v.absmin[1]
		|| ent->v.absmax[2] < touch->v.absmin[2] )
			continue;

		// check brush triggers accuracy
		pModel = sv.models[touch->v.modelindex];
		if ( pModel && pModel->type == mod_brush )
		{
			int		contents;
			hull_t	*hull;
			vec3_t	offset, localPosition;

			// force to select bsp-hull
			hull = SV_HullForBsp( touch, ent->v.mins, ent->v.maxs, offset );

			VectorSubtract( ent->v.origin, offset, localPosition );

			// test hull for intersection with this model
			contents = SV_HullPointContents( hull, hull->firstclipnode, localPosition );
			if ( contents != CONTENTS_SOLID )
				continue;
		}

		gGlobalVariables.time = sv.time;
		gEntityInterface.pfnTouch( touch, ent );
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( ent->v.absmax[node->axis] > node->dist )
		SV_TouchLinks( ent, node->children[0] );
	if ( ent->v.absmin[node->axis] < node->dist )
		SV_TouchLinks( ent, node->children[1] );
}


/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs( edict_t *ent, mnode_t *node, int *topnode )
{
	mplane_t	*splitplane;
	mleaf_t		*leaf;
	int			sides;
	int			leafnum;

	if ( node->contents == CONTENTS_SOLID )
		return;

	// add an efrag if the node is a leaf
	if ( node->contents < 0 )
	{
		if ( ent->num_leafs >= MAX_ENT_LEAFS )
		{
			ent->num_leafs = MAX_ENT_LEAFS + 1;	// must use headnode
			return;
		}

		leaf = (mleaf_t *)node;
		leafnum = leaf - sv.worldmodel->leafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}

	// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE( ent->v.absmin, ent->v.absmax, splitplane );

	if ( sides == 3 && *topnode == -1 )
		*topnode = node - sv.worldmodel->nodes;

	// recurse down the contacted sides
	if ( sides & 1 )
		SV_FindTouchedLeafs( ent, node->children[0], topnode );

	if ( sides & 2 )
		SV_FindTouchedLeafs( ent, node->children[1], topnode );
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict( edict_t *ent, qboolean touch_triggers )
{
	static int	iTouchLinkSemaphore = 0;
	areanode_t	*node;
	int			topnode;

	if ( ent->area.prev )
		SV_UnlinkEdict( ent );	// unlink from old position

	if ( ent == sv.edicts )
		return;		// don't add the world

	if ( ent->free )
		return;

	// set the abs box
	gEntityInterface.pfnSetAbsBox( ent );

	if ( ent->v.movetype == MOVETYPE_FOLLOW && ent->v.aiment )
	{
		ent->headnode = ent->v.aiment->headnode;
		ent->num_leafs = ent->v.aiment->num_leafs;
		Q_memcpy( ent->leafnums, ent->v.aiment->leafnums, sizeof( ent->leafnums ) );
	}
	else
	{
		// link to PVS leafs
		ent->num_leafs = 0;
		ent->headnode = -1;
		topnode = -1;
		if ( ent->v.modelindex )
			SV_FindTouchedLeafs( ent, sv.worldmodel->nodes, &topnode );

		if ( ent->num_leafs > MAX_ENT_LEAFS )
		{
			ent->num_leafs = 0;
			ent->headnode = topnode;
			Q_memset( ent->leafnums, 0xff, sizeof( ent->leafnums ) );
		}
	}

	// ignore non-solid bodies
	if ( ent->v.solid == SOLID_NOT && ent->v.skin >= -1 )
		return;

	if ( ent->v.solid == SOLID_BSP )
	{
		model_t	*model;

		model = sv.models[ent->v.modelindex];
		if ( !model && !Q_strlen( &pr_strings[ent->v.model] ) )
		{
			Con_DPrintf( "Inserted %s with no model\n", &pr_strings[ent->v.classname] );
			return;
		}
	}

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while ( 1 )
	{
		if ( node->axis == -1 )
			break;
		if ( ent->v.absmin[node->axis] > node->dist )
			node = node->children[0];
		else if ( ent->v.absmax[node->axis] < node->dist )
			node = node->children[1];
		else
			break;		// crosses the node
	}

	// link it in

	if ( ent->v.solid == SOLID_TRIGGER )
		InsertLinkBefore( &ent->area, &node->trigger_edicts );
	else
		InsertLinkBefore( &ent->area, &node->solid_edicts );

	// if touch_triggers, touch all entities at this node and decend for more
	if ( touch_triggers && !iTouchLinkSemaphore )
	{
		iTouchLinkSemaphore = 1;
		SV_TouchLinks( ent, sv_areanodes );
		iTouchLinkSemaphore = 0;
	}
}



/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

int SV_LinkContents( areanode_t *node, const vec_t *pos )
{
	link_t		*l, *next;
	edict_t		*touch;
	int			contents;
	hull_t		*hull;
	vec3_t		offset, localPosition;
	model_t		*pModel;

	// get water edicts
	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if ( touch->v.solid != SOLID_NOT )
			continue;

		if ( touch->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( touch->v.groupinfo & g_groupmask ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( touch->v.groupinfo & g_groupmask )
					continue;
			}
		}

		pModel = sv.models[touch->v.modelindex];
		if ( !pModel || pModel->type != mod_brush )
			continue;

		if ( pos[0] > touch->v.absmax[0]
		|| pos[1] > touch->v.absmax[1]
		|| pos[2] > touch->v.absmax[2]
		|| pos[0] < touch->v.absmin[0]
		|| pos[1] < touch->v.absmin[1]
		|| pos[2] < touch->v.absmin[2] )
			continue;

		if ( touch->v.skin < -100 || touch->v.skin > 100 )
			Con_DPrintf( "Invalid contents on trigger field: %s\n", &pr_strings[touch->v.classname] );

		hull = SV_HullForBsp( touch, vec3_origin, vec3_origin, offset );

		// offset the test point appropriately for this hull.
		VectorSubtract( pos, offset, localPosition );

		// test hull for intersection with this model
		contents = SV_HullPointContents( hull, hull->firstclipnode, localPosition );
		if ( contents == CONTENTS_EMPTY )
			continue;

		return touch->v.skin;
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return CONTENTS_EMPTY;

	if ( pos[node->axis] > node->dist )
		return SV_LinkContents( node->children[0], pos );
	if ( pos[node->axis] < node->dist )
		return SV_LinkContents( node->children[1], pos );

	return CONTENTS_EMPTY;
}

/*
==================
SV_PointContents

==================
*/
int SV_PointContents( const vec_t *p )
{
	int		cont, entityContents;

	cont = SV_HullPointContents( &sv.worldmodel->hulls[0], 0, p );
	if ( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;

	if ( cont != CONTENTS_SOLID )
	{
		entityContents = SV_LinkContents( sv_areanodes, p );
		if ( entityContents != CONTENTS_EMPTY )
			return entityContents;
	}

	return cont;
}

//===========================================================================

/*
============
SV_TestEntityPosition

A small wrapper around SV_BoxInSolidEntity that never clips against the
supplied entity.
============
*/
edict_t *SV_TestEntityPosition( edict_t *ent )
{
	trace_t		trace;
	qboolean	monsterClip;

	if ( ent->v.flags & FL_MONSTERCLIP )
		monsterClip = true;
	else
		monsterClip = false;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent, monsterClip );
	if ( trace.startsolid )
	{
		SV_SetGlobalTrace( &trace );
		return trace.ent;
	}

	return NULL;
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

/*
==================
SV_RecursiveHullCheck

==================
*/
qboolean SV_RecursiveHullCheck( hull_t *hull, int num, float p1f, float p2f, vec_t *p1, vec_t *p2, trace_t *trace )
{
	dclipnode_t	*node;
	mplane_t	*plane;
	float		t1, t2;
	float		frac;
	int			i;
	vec3_t		mid;
	int			side;
	float		midf;

	frac = 0;

	// check for empty
	if ( num < 0 )
	{
		if ( num != CONTENTS_SOLID )
		{
			trace->allsolid = false;
			if ( num == CONTENTS_EMPTY )
				trace->inopen = true;
			else if ( num != CONTENTS_TRANSLUCENT )
				trace->inwater = true;
		}
		else
			trace->startsolid = true;
		return true;	// empty
	}

	if ( num < hull->firstclipnode || num > hull->lastclipnode || !hull->planes )
		Sys_Error( "SV_RecursiveHullCheck: bad node number" );

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
		return SV_RecursiveHullCheck( hull, node->children[0], p1f, p2f, p1, p2, trace );
	if ( t1 < 0 && t2 < 0 )
		return SV_RecursiveHullCheck( hull, node->children[1], p1f, p2f, p1, p2, trace );

	// put the crosspoint DIST_EPSILON pixels on the near side
	if ( t1 < 0 )
		frac = ( t1 + DIST_EPSILON ) / ( t1 - t2 );
	else
		frac = ( t1 - DIST_EPSILON ) / ( t1 - t2 );

	if ( frac < 0 )
		frac = 0;
	if ( frac > 1 )
		frac = 1;

	if ( IS_NAN( frac ) )
		return false;

	midf = p1f + ( p2f - p1f ) * frac;
	for ( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac * ( p2[i] - p1[i] );

	side = ( t1 < 0 );

	// move up to the node
	if ( !SV_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ) )
		return false;

	if ( SV_HullPointContents( hull, node->children[side^1], mid ) != CONTENTS_SOLID )
		// go past the node
		return SV_RecursiveHullCheck( hull, node->children[side^1], midf, p2f, mid, p2, trace );

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

	while ( SV_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		if ( ( frac -= 0.1 ) < 0 )
		{
			trace->fraction = midf;
			VectorCopy( mid, trace->endpos );
			Con_DPrintf( "backup past 0\n" );
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


/*
==================
SV_SingleClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
void SV_SingleClipMoveToEntity( edict_t *ent, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, trace_t *trace )
{
	vec3_t		offset;
	vec3_t		start_l, end_l;
	hull_t		*hull;
	model_t		*pmodel;
	int			numhulls, i, rotated;

	// fill in a default trace
	Q_memset( trace, 0, sizeof( trace_t ) );
	trace->fraction = 1;
	trace->allsolid = true;
	VectorCopy( end, trace->endpos );

	pmodel = sv.models[ent->v.modelindex];
	if ( pmodel->type == mod_studio )
	{
		// get the clipping hull
		hull = SV_HullForStudioModel( ent, mins, maxs, offset, &numhulls );
	}
	else
	{
		hull = SV_HullForEntity( ent, mins, maxs, offset );
		numhulls = 1;
	}

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if ( ent->v.solid == SOLID_BSP && ( ent->v.angles[0] || ent->v.angles[1] || ent->v.angles[2] ) )
		rotated = true;
	else
		rotated = false;

	if ( rotated )
	{
		vec3_t	forward, right, up;
		vec3_t	temp;

		AngleVectors( ent->v.angles, forward, right, up );

		VectorCopy( start_l, temp );
		start_l[0] = DotProduct( temp, forward );
		start_l[1] = -DotProduct( temp, right );
		start_l[2] = DotProduct( temp, up );

		VectorCopy( end_l, temp );
		end_l[0] = DotProduct( temp, forward );
		end_l[1] = -DotProduct( temp, right );
		end_l[2] = DotProduct( temp, up );
	}

	// trace a line through the apropriate clipping hull
	if ( numhulls == 1 )
	{
		SV_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, trace );
	}
	else
	{
		int		closest;
		trace_t	testtrace;

		closest = 0;
		for ( i = 0; i < numhulls; i++ )
		{
			Q_memset( &testtrace, 0, sizeof( trace_t ) );
			testtrace.fraction = 1;
			testtrace.allsolid = true;
			VectorCopy( end, testtrace.endpos );

			SV_RecursiveHullCheck( &hull[i], hull[i].firstclipnode, 0, 1, start_l, end_l, &testtrace );

			if ( i == 0 || testtrace.allsolid || testtrace.startsolid || testtrace.fraction < trace->fraction )
			{
				if ( trace->startsolid )
				{
					*trace = testtrace;
					trace->startsolid = true;
				}
				else
					*trace = testtrace;
				closest = i;
			}
		}

		trace->hitgroup = SV_HitgroupForStudioHull( closest );
	}

	if ( trace->fraction != 1.0 )
	{
		// compute endpos (generic)
		if ( rotated )
		{
			vec3_t	forward, right, up;
			vec3_t	temp;

			AngleVectorsTranspose( ent->v.angles, forward, right, up );

			VectorCopy( trace->plane.normal, temp );
			trace->plane.normal[0] = DotProduct( temp, forward );
			trace->plane.normal[1] = DotProduct( temp, right );
			trace->plane.normal[2] = DotProduct( temp, up );
		}

		trace->endpos[0] = start[0] + trace->fraction * ( end[0] - start[0] );
		trace->endpos[1] = start[1] + trace->fraction * ( end[1] - start[1] );
		trace->endpos[2] = start[2] + trace->fraction * ( end[2] - start[2] );
	}

	// did we clip the move?
	if ( trace->fraction < 1.0 || trace->startsolid )
		trace->ent = ent;
}

/*
==================
SV_ClipMoveToEntity

==================
*/
trace_t SV_ClipMoveToEntity( edict_t *ent, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end )
{
	trace_t	goodtrace;

	if ( 1 )
		SV_SingleClipMoveToEntity( ent, start, mins, maxs, end, &goodtrace );

	return goodtrace;
}

//===========================================================================

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;

	// touch linked edicts
	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if ( touch->v.groupinfo && clip->passedict && clip->passedict->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( touch->v.groupinfo & clip->passedict->v.groupinfo ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( touch->v.groupinfo & clip->passedict->v.groupinfo )
					continue;
			}
		}

		if ( touch->v.solid == SOLID_NOT )
			continue;
		if ( touch == clip->passedict )
			continue;
		if ( touch->v.solid == SOLID_TRIGGER )
			Sys_Error( "Trigger in clipping list" );

		if ( gNewDLLFunctions.pfnShouldCollide && !gNewDLLFunctions.pfnShouldCollide( touch, clip->passedict ) )
			return;

		// monsterclip filter
		if ( touch->v.solid == SOLID_BSP )
		{
			if ( ( touch->v.flags & FL_MONSTERCLIP ) && !clip->monsterClipBrush )
				continue;
		}
		else
		{
			// ignore all monsters but pushables
			if ( clip->type == MOVE_NOMONSTERS && touch->v.movetype != MOVETYPE_PUSHSTEP )
				continue;
		}

		if ( clip->ignoretrans && touch->v.rendermode != kRenderNormal && !( touch->v.flags & FL_WORLDBRUSH ) )
			continue;

		if ( clip->boxmins[0] > touch->v.absmax[0]
		|| clip->boxmins[1] > touch->v.absmax[1]
		|| clip->boxmins[2] > touch->v.absmax[2]
		|| clip->boxmaxs[0] < touch->v.absmin[0]
		|| clip->boxmaxs[1] < touch->v.absmin[1]
		|| clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		if ( touch->v.solid != SOLID_SLIDEBOX && !SV_CheckSphereIntersection( touch, clip->start, clip->end ) )
			continue;

		if ( clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0] )
			continue;	// points never interact

		// might intersect, so do an exact clip
		if ( clip->trace.allsolid )
			return;

		if ( clip->passedict )
		{
			if ( touch->v.owner == clip->passedict )
				continue;	// don't clip against own missiles
			if ( clip->passedict->v.owner == touch )
				continue;	// don't clip against owner
		}

		if ( touch->v.flags & FL_MONSTER )
			trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins2, clip->maxs2, clip->end );
		else
			trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end );

		if ( trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction )
		{
			trace.ent = touch;
			if ( clip->trace.startsolid )
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if ( trace.startsolid )
			clip->trace.startsolid = true;
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks( node->children[1], clip );
}

/*
====================
SV_ClipToWorldbrush

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToWorldbrush( areanode_t *node, moveclip_t *clip )
{
	link_t		*l, *next;
	edict_t		*touch;
	trace_t		trace;

	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if ( touch->v.solid != SOLID_BSP )
			continue;
		if ( touch == clip->passedict )
			continue;
		if ( !( touch->v.flags & FL_WORLDBRUSH ) )
			continue;

		if ( clip->boxmins[0] > touch->v.absmax[0]
		|| clip->boxmins[1] > touch->v.absmax[1]
		|| clip->boxmins[2] > touch->v.absmax[2]
		|| clip->boxmaxs[0] < touch->v.absmin[0]
		|| clip->boxmaxs[1] < touch->v.absmin[1]
		|| clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		if ( clip->trace.allsolid )
			return;

		trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end );

		if ( trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction )
		{
			trace.ent = touch;
			if ( clip->trace.startsolid )
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else
				clip->trace = trace;
		}
		else if ( trace.startsolid )
			clip->trace.startsolid = true;
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToWorldbrush( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToWorldbrush( node->children[1], clip );
}

/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds( const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, vec_t *boxmins, vec_t *boxmaxs )
{
	int		i;

	for ( i = 0; i < 3; i++ )
	{
		if ( end[i] > start[i] )
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
SV_MoveNoEnts
==================
*/
trace_t SV_MoveNoEnts( const vec_t *start, vec_t *mins, vec_t *maxs, const vec_t *end, int type, edict_t *passedict )
{
	moveclip_t	clip;
	vec3_t		worldEndPoint;
	float		worldFraction;

	Q_memset( &clip, 0, sizeof( moveclip_t ) );

	// clip to world
	clip.trace = SV_ClipMoveToEntity( sv.edicts, start, mins, maxs, end );
	if ( !clip.trace.fraction )
		return clip.trace;

	VectorCopy( clip.trace.endpos, worldEndPoint );
	clip.end = worldEndPoint;
	worldFraction = clip.trace.fraction;
	clip.trace.fraction = 1.0;

	clip.start = start;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type & 0xff;
	clip.ignoretrans = type >> 8;
	clip.passedict = passedict;
	clip.monsterClipBrush = false;

	VectorCopy( mins, clip.mins2 );
	VectorCopy( maxs, clip.maxs2 );

	// create the bounding box of the entire move
	SV_MoveBounds( start, clip.mins2, clip.maxs2, worldEndPoint, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToWorldbrush( sv_areanodes, &clip );

	clip.trace.fraction *= worldFraction;

	if ( clip.trace.ent )
		gGlobalVariables.trace_ent = clip.trace.ent;
	else
		gGlobalVariables.trace_ent = NULL;

	return clip.trace;
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move( const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end, int type, edict_t *passedict, qboolean monsterClipBrush )
{
	moveclip_t	clip;
	int			i;
	vec3_t		worldEndPoint;
	float		worldFraction;

	Q_memset( &clip, 0, sizeof( moveclip_t ) );

	// clip to world
	clip.trace = SV_ClipMoveToEntity( sv.edicts, start, mins, maxs, end );
	if ( !clip.trace.fraction )
		return clip.trace;

	VectorCopy( clip.trace.endpos, worldEndPoint );
	clip.end = worldEndPoint;
	worldFraction = clip.trace.fraction;
	clip.trace.fraction = 1.0;

	clip.start = start;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type & 0xff;
	clip.ignoretrans = type >> 8;
	clip.passedict = passedict;
	clip.monsterClipBrush = monsterClipBrush;

	if ( type == MOVE_MISSILE )
	{
		for ( i = 0; i < 3; i++ )
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy( mins, clip.mins2 );
		VectorCopy( maxs, clip.maxs2 );
	}

	// create the bounding box of the entire move
	SV_MoveBounds( start, clip.mins2, clip.maxs2, worldEndPoint, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToLinks( sv_areanodes, &clip );

	clip.trace.fraction *= worldFraction;

	if ( clip.trace.ent )
		gGlobalVariables.trace_ent = clip.trace.ent;
	else
		gGlobalVariables.trace_ent = NULL;

	return clip.trace;
}
