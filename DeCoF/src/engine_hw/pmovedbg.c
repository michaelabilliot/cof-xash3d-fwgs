// pmovedbg.c -- particle outlines of the player movement boxes

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "client.h"

// Expand debugging BBOX particle hulls by this many units.
#define BOX_GAP 0.0f

static int PM_boxpnt[6][4] =
{
	{ 0, 4, 6, 2 }, // +X
	{ 0, 1, 5, 4 }, // +Y
	{ 0, 2, 3, 1 }, // +Z
	{ 7, 5, 1, 3 }, // -X
	{ 7, 3, 2, 6 }, // -Y
	{ 7, 6, 4, 5 }, // -Z
};

/*
===============
PM_ParticleLine

===============
*/
void PM_ParticleLine( vec_t *start, vec_t *end, int pcolor, float life, float vert )
{
	float	linestep = 2.0f;
	float	curdist;
	float	len;
	vec3_t	curpos;
	vec3_t	diff;
	int		i;

	// Determine distance;
	VectorSubtract( end, start, diff );

	len = VectorNormalize( diff );

	curdist = 0;
	while ( curdist <= len )
	{
		for ( i = 0; i < 3; i++ )
			curpos[i] = start[i] + curdist * diff[i];

		CL_Particle( curpos, pcolor, life, 0, vert );
		curdist += linestep;
	}
}

/*
================
PM_DrawRectangle

================
*/
void PM_DrawRectangle( vec_t *tl, vec_t *bl, vec_t *tr, vec_t *br, int pcolor, float life )
{
	PM_ParticleLine( tl, bl, pcolor, life, 0 );
	PM_ParticleLine( bl, br, pcolor, life, 0 );
	PM_ParticleLine( br, tr, pcolor, life, 0 );
	PM_ParticleLine( tr, tl, pcolor, life, 0 );
}

/*
================
PM_DrawBBox

================
*/
void PM_DrawBBox( vec_t *mins, vec_t *maxs, vec_t *origin, int pcolor, float life )
{
	int		j;

	vec3_t	tmp;
	vec3_t	p[8];
	float	gap = BOX_GAP;

	for ( j = 0; j < 8; j++ )
	{
		tmp[0] = ( j & 1 ) ? mins[0] - gap : maxs[0] + gap;
		tmp[1] = ( j & 2 ) ? mins[1] - gap : maxs[1] + gap;
		tmp[2] = ( j & 4 ) ? mins[2] - gap : maxs[2] + gap;

		VectorAdd( tmp, origin, tmp );
		VectorCopy( tmp, p[j] );
	}

	for ( j = 0; j < 6; j++ )
	{
		PM_DrawRectangle(
			p[PM_boxpnt[j][1]],
			p[PM_boxpnt[j][0]],
			p[PM_boxpnt[j][2]],
			p[PM_boxpnt[j][3]],
			pcolor, life );
	}
}
