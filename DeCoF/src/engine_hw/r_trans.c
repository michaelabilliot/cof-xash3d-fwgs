// r_trans.c -- sorted list of the translucent entities of a frame

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "r_studioint.h"
#include "pmove.h"
#include "mem.h"

#include <math.h>

#define STUDIO_RENDER	1
#define STUDIO_EVENTS	2

#define GLARE_FALLOFF	19000.0

typedef struct
{
	cl_entity_t	*pEnt;
	float		distance;
} transObjRef;

void	R_DrawBrushModel( cl_entity_t *e );
void	R_DrawSpriteModel( cl_entity_t *e );
void	R_DrawAliasModel( cl_entity_t *e );
float	*R_GetAttachmentPoint( int entity, int attachment );
int		CL_FxBlend( cl_entity_t *ent );
void	ClientDLL_DrawTransparentTriangles( void );

extern	qboolean	g_bUserFogOn;

qboolean	r_intentities = false;

static int			numTransObjs;
static int			maxTransObjs;
static transObjRef	*transObjects;

/*
=================
R_AllocObjects
=================
*/
void R_AllocObjects( int nMax )
{
	if ( transObjects )
		Con_Printf( "Transparent objects reallocate\n" );

	transObjects = Mem_Malloc( nMax * sizeof( transObjRef ) );
	Q_memset( transObjects, 0, nMax * sizeof( transObjRef ) );
	maxTransObjs = nMax;
}

/*
=================
R_DestroyObjects
=================
*/
void R_DestroyObjects( void )
{
	if ( transObjects )
	{
		Mem_Free( transObjects );
		transObjects = NULL;
	}
	maxTransObjs = 0;
}

/*
=================
GlowBlend
=================
*/
float GlowBlend( cl_entity_t *pEntity )
{
	vec3_t		tmp;
	float		dist, brightness;
	pmtrace_t	trace;

	VectorSubtract( r_entorigin, r_origin, tmp );
	dist = Length( tmp );

	pmove->usehull = 2;
	if ( r_traceglow.value )
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE, -1 );
	else
		trace = PM_PlayerTrace( r_origin, r_entorigin, PM_GLASS_IGNORE | PM_STUDIO_IGNORE, -1 );

	if ( ( 1.0 - trace.fraction ) * dist > 8 )
		return 0;

	if ( pEntity->curstate.renderfx == kRenderFxNoDissipation )
		return pEntity->curstate.renderamt * ( 1.0 / 255.0 );

	brightness = GLARE_FALLOFF / ( dist * dist );
	if ( brightness < 0.05 )
		brightness = 0.05F;
	if ( brightness > 1.0 )
		brightness = 1.0F;

	pEntity->curstate.scale = dist * ( 1.0 / 200.0 );

	return brightness;
}

/*
=================
RotatedBBox
=================
*/
void RotatedBBox( vec_t *mins, vec_t *maxs, vec_t *angles, vec_t *tmins, vec_t *tmaxs )
{
	int		i;
	float	v, max;

	if ( !angles[0] && !angles[1] && !angles[2] )
	{
		VectorCopy( mins, tmins );
		VectorCopy( maxs, tmaxs );
		return;
	}

	max = 0;
	for ( i = 0; i < 3; i++ )
	{
		v = fabs( mins[i] );
		if ( v > max )
			max = v;
		v = fabs( maxs[i] );
		if ( v > max )
			max = v;
	}

	for ( i = 0; i < 3; i++ )
	{
		tmins[i] = -max;
		tmaxs[i] = max;
	}
}

/*
=================
AddTEntity
=================
*/
void AddTEntity( cl_entity_t *pEnt )
{
	int		i;
	float	dist;
	vec3_t	v;

	dist = 1e9F;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	if ( pEnt->model && pEnt->model->type == mod_brush && pEnt->curstate.rendermode == kRenderTransAlpha )
	{
		dist = 1e9F;
	}
	else
	{
		VectorAdd( pEnt->model->mins, pEnt->model->maxs, v );
		VectorScale( v, 0.5, v );
		VectorAdd( v, pEnt->origin, v );
		VectorSubtract( r_origin, v, v );
		dist = DotProduct( v, v );
	}

	for ( i = numTransObjs; i > 0; i-- )
	{
		if ( dist > transObjects[i - 1].distance )
		{
			transObjects[i].pEnt = transObjects[i - 1].pEnt;
			transObjects[i].distance = transObjects[i - 1].distance;
		}
		else
		{
			break;
		}
	}

	transObjects[i].pEnt = pEnt;
	transObjects[i].distance = dist;
	numTransObjs++;
}

/*
=================
AppendTEntity
=================
*/
void AppendTEntity( cl_entity_t *pEnt )
{
	float	dist;
	vec3_t	v;

	dist = 1e9F;

	if ( numTransObjs >= maxTransObjs )
		Sys_Error( "AddTentity: Too many objects" );

	VectorAdd( pEnt->model->mins, pEnt->model->maxs, v );
	VectorScale( v, 0.5, v );
	VectorAdd( v, pEnt->origin, v );
	VectorSubtract( r_origin, v, v );
	dist = DotProduct( v, v );

	transObjects[numTransObjs].pEnt = pEnt;
	transObjects[numTransObjs].distance = dist;
	numTransObjs++;
}

float	r_blend;

/*
=================
R_DrawTEntitiesOnList
=================
*/
void R_DrawTEntitiesOnList( qboolean clientOnly )
{
	int		i, j;

	if ( !r_drawentities.value )
		return;

	if ( !clientOnly )
	{
		// Handle all trans surfaces here
		for ( i = 0; i < numTransObjs; i++ )
		{
			qglDisable( GL_FOG );

			currententity = transObjects[i].pEnt;

			if ( ( r_blend = CL_FxBlend( currententity ) ) <= 0 )
				continue;

			r_blend *= ( 1.0 / 255.0 );

			if ( currententity->curstate.rendermode == kRenderGlow && currententity->model->type != mod_sprite )
				Con_DPrintf( "Non-sprite set to glow!\n" );

			switch ( currententity->model->type )
			{
			case mod_alias:
				R_DrawAliasModel( currententity );
				break;

			case mod_brush:
				if ( g_bUserFogOn && currententity->curstate.rendermode != kRenderGlow && currententity->curstate.rendermode != kRenderTransAdd )
					qglEnable( GL_FOG );
				R_DrawBrushModel( currententity );
				break;

			case mod_sprite:
				if ( currententity->curstate.body )
				{
					float	*pAttachment;

					pAttachment = R_GetAttachmentPoint( currententity->curstate.skin, currententity->curstate.body );
					VectorCopy( pAttachment, r_entorigin );
				}
				else
				{
					VectorCopy( currententity->origin, r_entorigin );
				}

				// Glow sprites fade with distance and occlusion
				if ( currententity->curstate.rendermode == kRenderGlow )
					r_blend *= GlowBlend( currententity );

				if ( r_blend != 0 )
					R_DrawSpriteModel( currententity );
				break;

			case mod_studio:
				if ( !currententity->curstate.renderamt )
					continue;

				if ( currententity->player )
				{
					pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
				}
				else
				{
					if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
					{
						for ( j = 0; j < numTransObjs; j++ )
						{
							if ( transObjects[j].pEnt->index == currententity->curstate.aiment )
							{
								currententity = transObjects[j].pEnt;
								if ( currententity->player )
								{
									pStudioAPI->StudioDrawPlayer( 0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
								}
								else
								{
									pStudioAPI->StudioDrawModel( 0 );
								}
								currententity = transObjects[i].pEnt;
								pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
								break;
							}
						}
					}
					else
					{
						pStudioAPI->StudioDrawModel( STUDIO_RENDER | STUDIO_EVENTS );
					}
				}
				break;
			}
		}
	}

	GL_DisableMultitexture();
	qglEnable( GL_ALPHA_TEST );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( g_bUserFogOn )
		qglDisable( GL_FOG );

	ClientDLL_DrawTransparentTriangles();

	if ( g_bUserFogOn )
		qglEnable( GL_FOG );

	numTransObjs = 0;
	r_blend = 1.0;
}
