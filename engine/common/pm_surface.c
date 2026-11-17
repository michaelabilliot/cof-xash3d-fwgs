/*
pm_surface.c - surface tracing
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "xash3d_mathlib.h"
#include "pm_local.h"
#include "ref_common.h"

#undef FRAC_EPSILON
#define FRAC_EPSILON	(1.0f / 32.0f)

typedef struct
{
	float		fraction;
	int		contents;
	msurface_t	*surface;
} linetrace_t;

/*
==============
fix_coord

converts the reletive tex coords to absolute
==============
*/
static uint fix_coord( vec_t in, uint width )
{
	if( in > 0 ) return (uint)in % width;
	return width - ((uint)fabs( in ) % width);
}

/*
=============
SampleMiptex

fence texture testing
=============
*/
static int PM_SampleMiptex( const msurface_t *surf, const vec3_t point )
{
	mextrasurf_t	*info = surf->info;
	mfacebevel_t	*fb = info->bevel;
	int		contents;

	// fill the default contents
	if( fb ) contents = fb->contents;
	else contents = CONTENTS_SOLID;

	if( !surf->texinfo || !surf->texinfo->texture )
		return contents;

	mtexinfo_t *tx = surf->texinfo;
	texture_t *mt = tx->texture;

	if( mt->name[0] != '{' )
		return contents;

	// TODO: this won't work under dedicated
	// should we bring up imagelib and keep original buffers?
#if !XASH_DEDICATED
	if( !Host_IsDedicated() )
	{
		const byte *data = ref.dllFuncs.R_GetTextureOriginalBuffer( mt->gl_texturenum );

		if( !data ) return contents; // original doesn't kept

		vec_t ds = DotProduct( point, tx->vecs[0] ) + tx->vecs[0][3];
		vec_t dt = DotProduct( point, tx->vecs[1] ) + tx->vecs[1][3];

		// convert ST to real pixels position
		int x = fix_coord( ds, mt->width - 1 );
		int y = fix_coord( dt, mt->height - 1 );

		ASSERT( x >= 0 && y >= 0 );

		if( data[(mt->width * y) + x] == 255 )
			return CONTENTS_EMPTY;
		return CONTENTS_SOLID;
	}
#endif // !XASH_DEDICATED

	return contents;
}

/*
==================
PM_RecursiveSurfCheck

==================
*/
msurface_t *PM_RecursiveSurfCheck( model_t *mod, mnode_t *node, vec3_t p1, vec3_t p2 )
{
loc0:
	if( node->contents < 0 )
		return NULL;

	float t1 = PlaneDiff( p1, node->plane );
	float t2 = PlaneDiff( p2, node->plane );

	if( t1 >= -FRAC_EPSILON && t2 >= -FRAC_EPSILON )
	{
		node = node_child( node, 0, mod );
		goto loc0;
	}

	if( t1 < FRAC_EPSILON && t2 < FRAC_EPSILON )
	{
		node = node_child( node, 1, mod );
		goto loc0;
	}

	int side = (t1 < 0.0f);
	float frac = t1 / ( t1 - t2 );
	frac = bound( 0.0f, frac, 1.0f );

	vec3_t mid;
	VectorLerp( p1, frac, p2, mid );

	msurface_t *surf = PM_RecursiveSurfCheck( mod, node_child( node, side, mod ), p1, mid );
	if( surf != NULL )
		return surf;

	// walk through real faces
	int numsurfaces = node_numsurfaces( node, mod );
	int firstsurface = node_firstsurface( node, mod );
	for( int i = 0; i < numsurfaces; i++ )
	{
		msurface_t	*surf = &mod->surfaces[firstsurface + i];
		mextrasurf_t	*info = surf->info;
		mfacebevel_t	*fb = info->bevel;
		vec3_t		delta;

		if( !fb ) continue;	// ???

		VectorSubtract( mid, fb->origin, delta );
		if( DotProduct( delta, delta ) >= fb->radius )
			continue;	// no intersection

		int j;
		for( j = 0; j < fb->numedges; j++ )
		{
			if( PlaneDiff( mid, &fb->edges[j] ) > FRAC_EPSILON )
				break; // outside the bounds
		}

		if( j != fb->numedges )
			continue; // we are outside the bounds of the facet

		// hit the surface
		int contents = PM_SampleMiptex( surf, mid );

		if( contents != CONTENTS_EMPTY )
			return surf;
		return NULL; // through the fence
	}

	return PM_RecursiveSurfCheck( mod, node_child( node, side^1, mod ), mid, p2 );
}

/*
==================
PM_TraceTexture

find the face where the traceline hit
assume physentity is valid
==================
*/
msurface_t *PM_TraceSurface( physent_t *pe, vec3_t start, vec3_t end )
{
	model_t *bmodel = pe->model;

	if( !bmodel || bmodel->type != mod_brush )
		return NULL;

	hull_t *hull = &pe->model->hulls[0];
	vec3_t offset;
	VectorSubtract( hull->clip_mins, vec3_origin, offset );
	VectorAdd( offset, pe->origin, offset );

	vec3_t start_l, end_l;
	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if( !VectorIsNull( pe->angles ))
	{
		matrix4x4 matrix;
		Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
		Matrix4x4_VectorITransform( matrix, start, start_l );
		Matrix4x4_VectorITransform( matrix, end, end_l );
	}

	return PM_RecursiveSurfCheck( bmodel, &bmodel->nodes[hull->firstclipnode], start_l, end_l );
}

/*
==================
PM_TestLine_r

optimized trace for light gathering
==================
*/
static int PM_TestLine_r( model_t *mod, mnode_t *node, vec_t p1f, vec_t p2f, const vec3_t start, const vec3_t stop, linetrace_t *trace )
{
loc0:
	if( node->contents < 0 )
	{
		// water, slime or lava interpret as empty
		if( node->contents == CONTENTS_SOLID )
			return CONTENTS_SOLID;
		if( node->contents == CONTENTS_SKY )
			return CONTENTS_SKY;
		trace->fraction = 1.0f;
		return CONTENTS_EMPTY;
	}

	float front = PlaneDiff( start, node->plane );
	float back = PlaneDiff( stop, node->plane );

	if( front >= -FRAC_EPSILON && back >= -FRAC_EPSILON )
	{
		node = node_child( node, 0, mod );
		goto loc0;
	}

	if( front < FRAC_EPSILON && back < FRAC_EPSILON )
	{
		node = node_child( node, 1, mod );
		goto loc0;
	}

	int side = (front < 0);
	float frac = front / (front - back);
	frac = bound( 0.0f, frac, 1.0f );

	vec3_t mid;
	VectorLerp( start, frac, stop, mid );
	float midf = p1f + ( p2f - p1f ) * frac;

	int r = PM_TestLine_r( mod, node_child( node, side, mod ), p1f, midf, start, mid, trace );

	if( r != CONTENTS_EMPTY )
	{
		if( trace->surface == NULL )
			trace->fraction = midf;
		trace->contents = r;
		return r;
	}

	// walk through real faces
	int numsurfaces = node_numsurfaces( node, mod );
	int firstsurface = node_firstsurface( node, mod );
	for( int i = 0; i < numsurfaces; i++ )
	{
		msurface_t	*surf = &mod->surfaces[firstsurface + i];
		mextrasurf_t	*info = surf->info;
		mfacebevel_t	*fb = info->bevel;
		vec3_t		delta;

		if( !fb ) continue;

		VectorSubtract( mid, fb->origin, delta );
		if( DotProduct( delta, delta ) >= fb->radius )
			continue;	// no intersection

		int j;
		for( j = 0; j < fb->numedges; j++ )
		{
			if( PlaneDiff( mid, &fb->edges[j] ) > FRAC_EPSILON )
				break; // outside the bounds
		}

		if( j != fb->numedges )
			continue; // we are outside the bounds of the facet

		// hit the surface
		int contents = PM_SampleMiptex( surf, mid );

		// fill the trace and out
		trace->contents = contents;
		trace->fraction = midf;

		if( contents != CONTENTS_EMPTY )
			trace->surface = surf;

		return contents;
	}

	return PM_TestLine_r( mod, node_child( node, !side, mod ), midf, p2f, mid, stop, trace );
}

int PM_TestLineExt( playermove_t *pmove, physent_t *ents, int numents, const vec3_t start, const vec3_t end, int flags )
{
	linetrace_t trace =
	{
		.contents = CONTENTS_EMPTY,
		.fraction = 1.0f,
	};

	for( int i = 0; i < numents; i++ )
	{
		physent_t *pe = &ents[i];

		if( i != 0 && FBitSet( flags, PM_WORLD_ONLY ))
			break;

		if( !pe->model || pe->model->type != mod_brush || pe->solid != SOLID_BSP )
			continue;

		if( FBitSet( flags, PM_GLASS_IGNORE ) && pe->rendermode != kRenderNormal )
			continue;

		hull_t *hull = &pe->model->hulls[0];
		vec3_t offset;

		hull = PM_HullForBsp( pe, pmove, offset );

		qboolean rotated;
		if( pe->solid == SOLID_BSP && !VectorIsNull( pe->angles ))
			rotated = true;
		else rotated = false;

		vec3_t start_l, end_l;
		if( rotated )
		{
			matrix4x4 matrix;
			Matrix4x4_CreateFromEntity( matrix, pe->angles, offset, 1.0f );
			Matrix4x4_VectorITransform( matrix, start, start_l );
			Matrix4x4_VectorITransform( matrix, end, end_l );
		}
		else
		{
			VectorSubtract( start, pe->origin, start_l );
			VectorSubtract( end, pe->origin, end_l );
		}

		linetrace_t trace_bbox =
		{
			.contents = CONTENTS_EMPTY,
			.fraction = 1.0f,
		};

		PM_TestLine_r( pe->model, &pe->model->nodes[hull->firstclipnode], 0.0f, 1.0f, start_l, end_l, &trace_bbox );

		if( trace_bbox.contents != CONTENTS_EMPTY || trace_bbox.fraction < trace.fraction )
		{
			trace = trace_bbox;
		}
	}

	return trace.contents;
}

/*
===============================================================================

SOUND RAY TRACING

Walks the render BSP (hull 0) of the world and of solid brush entities and
reports every solid span crossed by a segment, front to back, with the entry
plane normal and the face that was hit (for its material). Used by the
raytraced audio system (engine/client/sound/s_raytrace.c).

===============================================================================
*/
typedef struct
{
	model_t          *mod;
	pm_soundspan_t   *spans;
	int              numspans;
	int              maxspans;
	int              entindex;
	qboolean         first_only;
	qboolean         startsolid; // still inside the solid the segment started in

	// last plane crossed, for the span entered next
	const mnode_t    *cross_node;
	int              cross_side;
	vec3_t           cross_point;
} pm_soundtrace_t;

static msurface_t *PM_SoundFaceAtPoint( model_t *mod, const mnode_t *node, const vec3_t point )
{
	int numsurfaces = node_numsurfaces( node, mod );
	int firstsurface = node_firstsurface( node, mod );

	for( int i = 0; i < numsurfaces; i++ )
	{
		msurface_t   *surf = &mod->surfaces[firstsurface + i];
		mfacebevel_t *fb = surf->info ? surf->info->bevel : NULL;
		vec3_t       delta;
		int          j;

		if( !fb ) continue;

		VectorSubtract( point, fb->origin, delta );
		if( DotProduct( delta, delta ) >= fb->radius )
			continue;

		for( j = 0; j < fb->numedges; j++ )
		{
			if( PlaneDiff( point, &fb->edges[j] ) > FRAC_EPSILON )
				break;
		}

		if( j == fb->numedges )
			return surf;
	}

	return NULL;
}

static qboolean PM_SoundTrace_r( pm_soundtrace_t *tr, mnode_t *node, float p1f, float p2f, const vec3_t p1, const vec3_t p2 )
{
loc0:
	if( node->contents < 0 )
	{
		pm_soundspan_t *last;

		if( node->contents != CONTENTS_SOLID && node->contents != CONTENTS_SKY )
		{
			tr->startsolid = false;
			return true; // empty, water, slime, lava: sound passes
		}

		if( tr->startsolid )
			return true; // still inside the solid we started in, ignore it

		last = tr->numspans > 0 ? &tr->spans[tr->numspans - 1] : NULL;

		// adjacent solid leaves form one wall
		if( last && last->entindex == tr->entindex && last->contents == node->contents && last->frac_out >= p1f - 0.0001f )
		{
			last->frac_out = p2f;
			return true;
		}

		if( tr->numspans >= tr->maxspans )
			return false;

		last = &tr->spans[tr->numspans++];
		last->frac_in = p1f;
		last->frac_out = p2f;
		last->contents = node->contents;
		last->entindex = tr->entindex;
		last->surf = NULL;
		VectorClear( last->normal );

		if( tr->cross_node )
		{
			const mplane_t *plane = tr->cross_node->plane;

			if( tr->cross_side ) VectorNegate( plane->normal, last->normal );
			else VectorCopy( plane->normal, last->normal );

			last->surf = PM_SoundFaceAtPoint( tr->mod, tr->cross_node, tr->cross_point );
		}

		return !tr->first_only;
	}

	float t1 = PlaneDiff( p1, node->plane );
	float t2 = PlaneDiff( p2, node->plane );

	if( t1 >= 0.0f && t2 >= 0.0f )
	{
		node = node_child( node, 0, tr->mod );
		goto loc0;
	}

	if( t1 < 0.0f && t2 < 0.0f )
	{
		node = node_child( node, 1, tr->mod );
		goto loc0;
	}

	int side = ( t1 < 0.0f );
	float frac = t1 / ( t1 - t2 );
	if( !( frac >= 0.0f )) frac = 0.0f; // also catches NaN
	if( frac > 1.0f ) frac = 1.0f;

	vec3_t mid;
	VectorLerp( p1, frac, p2, mid );
	float midf = p1f + ( p2f - p1f ) * frac;

	if( !PM_SoundTrace_r( tr, node_child( node, side, tr->mod ), p1f, midf, p1, mid ))
		return false;

	tr->cross_node = node;
	tr->cross_side = side;
	VectorCopy( mid, tr->cross_point );

	return PM_SoundTrace_r( tr, node_child( node, !side, tr->mod ), midf, p2f, mid, p2 );
}

static int PM_SoundSpanCompare( const void *a, const void *b )
{
	const pm_soundspan_t *sa = a, *sb = b;

	if( sa->frac_in < sb->frac_in ) return -1;
	if( sa->frac_in > sb->frac_in ) return 1;
	return 0;
}

/*
==================
PM_SoundTrace

collects solid spans along start->end from the world (physent 0) and
solid brush entities, sorted front to back. In first_only mode only the
nearest span is returned. Solid the segment starts in is ignored.
==================
*/
int PM_SoundTrace( playermove_t *pmove, const vec3_t start, const vec3_t end, pm_soundspan_t *spans, int maxspans, qboolean first_only, qboolean world_only )
{
	pm_soundspan_t local[16];
	int total = 0;

	if( !pmove || maxspans <= 0 )
		return 0;

	for( int i = 0; i < pmove->numphysent; i++ )
	{
		physent_t *pe = &pmove->physents[i];
		pm_soundtrace_t tr;
		vec3_t start_l, end_l;
		matrix4x4 matrix;
		qboolean rotated;

		if( i != 0 && world_only )
			break;

		if( !pe->model || pe->model->type != mod_brush || pe->solid != SOLID_BSP )
			continue;

		if( !pe->model->nodes || pe->model->hulls[0].firstclipnode < 0 )
			continue;

		rotated = !VectorIsNull( pe->angles ) ? true : false;

		if( rotated )
		{
			Matrix4x4_CreateFromEntity( matrix, pe->angles, pe->origin, 1.0f );
			Matrix4x4_VectorITransform( matrix, start, start_l );
			Matrix4x4_VectorITransform( matrix, end, end_l );
		}
		else
		{
			VectorSubtract( start, pe->origin, start_l );
			VectorSubtract( end, pe->origin, end_l );
		}

		memset( &tr, 0, sizeof( tr ));
		tr.mod = pe->model;
		tr.spans = local;
		tr.maxspans = first_only ? 1 : ARRAYSIZE( local );
		tr.entindex = i;
		tr.first_only = first_only;
		tr.startsolid = true;

		PM_SoundTrace_r( &tr, &pe->model->nodes[pe->model->hulls[0].firstclipnode], 0.0f, 1.0f, start_l, end_l );

		for( int j = 0; j < tr.numspans; j++ )
		{
			pm_soundspan_t *s = &local[j];

			if( rotated && !VectorIsNull( s->normal ))
			{
				vec3_t n;
				Matrix4x4_VectorRotate( matrix, s->normal, n );
				VectorCopy( n, s->normal );
			}

			if( first_only )
			{
				if( total == 0 )
				{
					spans[0] = *s;
					total = 1;
				}
				else if( s->frac_in < spans[0].frac_in )
					spans[0] = *s;
			}
			else if( total < maxspans )
				spans[total++] = *s;
		}
	}

	if( total > 1 )
		qsort( spans, total, sizeof( *spans ), PM_SoundSpanCompare );

	return total;
}
