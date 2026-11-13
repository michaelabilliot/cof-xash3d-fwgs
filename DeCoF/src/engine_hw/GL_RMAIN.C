// r_main.c

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "chase.h"
#include "shake.h"
#include "r_studioint.h"
#include "cshift.h"
#include "cl_spectator.h"
#include "APIProxy.h"
#include "mem.h"
#include "../pm_shared/pm_movevars.h"

#include <math.h>

mspriteframe_t	*R_GetSpriteFrame( msprite_t *pSprite, int frame );
void	R_GetSpriteAxes( cl_entity_t *pEntity, int type, float *vforward, float *vright, float *vup );
void	R_SpriteColor( colorVec *pColor, cl_entity_t *pEntity, int alpha );
void	GL_Bind( int texnum );
void	*Mod_Extradata( model_t *mod );
void	Sys_Warning( char *pszWarning, ... );
void	AddTEntity( cl_entity_t *pEnt );
void	R_DrawBrushModel( cl_entity_t *e );
float	*R_GetAttachmentPoint( int entity, int attachment );
int		ClientDLL_IsThirdPerson( void );
colorVec	R_LightPoint( vec3_t p );
int		V_FadeAlpha( void );
void	CL_SetDevOverView( refdef_t *refdef );

#define STUDIO_RENDER	1
#define STUDIO_EVENTS	2

#define	STAT_HEALTH		0

#define	PLANE_ANYZ		5

void	R_CheckVariables( void );
void	R_AnimateLight( void );
void	V_CalcBlend( void );
void	R_MarkLeaves( void );
void	R_DrawWorld( void );
void	S_ExtraUpdate( void );
void	R_RenderFinalFog( void );
void	ClientDLL_DrawNormalTriangles( void );
void	R_DrawTEntitiesOnList( qboolean clientOnly );
void	R_RenderDlights( void );
void	R_DrawParticles( void );
void	GL_BuildLightmaps( void );
void	D_FlushCaches( void );
mleaf_t	*Mod_PointInLeaf( vec3_t p, model_t *model );
void	Mod_SpriteTextureName( char *pszName, const char *pszModelName, int frame );
int		GL_LoadTexture( char *identifier, GL_TEXTURETYPE textureType, int width, int height, byte *data, int mipmap, int iType, byte *pPal );
void	LoadBMP8( FileHandle_t hFile, byte **pPalette, int *nPalette, byte **pImage, int *nWidth, int *nHeight );
double	Sys_FloatTime( void );
float	CalcFov( float fov_x, float width, float height );

extern	float		scr_fov_value;
extern	qboolean	g_bUserFogOn;
extern	qboolean	allow_cheats;
extern	int			gD3DMode;
extern	movevars_t	movevars;
extern	cvar_t		gl_ztrick;
extern	cvar_t		gl_max_size;
extern	cvar_t		vid_d3d;
extern	cvar_t		snd_show;
extern	cvar_t		v_lambert;
extern	cvar_t		v_lightgamma;

cl_entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
cl_entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = { -1, -1 };	// cached

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

alight_t	r_viewlighting;
vec3_t		r_plightvec;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];
float	gProjectionMatrix[16];
float	gWorldToScreen[16];
float	gScreenToWorld[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

/*
=============================================================================

  MATH

=============================================================================
*/

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}

#define DEG2RAD( a ) ( a * M_PI ) / 180.0

void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	Q_memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	Q_memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	zrot[0][0] = cos( DEG2RAD( degrees ) );
	zrot[0][1] = sin( DEG2RAD( degrees ) );
	zrot[1][0] = -sin( DEG2RAD( degrees ) );
	zrot[1][1] = cos( DEG2RAD( degrees ) );

	R_ConcatRotations( m, zrot, tmpmat );
	R_ConcatRotations( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ )
	{
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox( vec3_t mins, vec3_t maxs )
{
	int		i;

	for ( i = 0; i < 4; i++ )
		if ( BoxOnPlaneSide( mins, maxs, &frustum[i] ) == 2 )
			return true;
	return false;
}

void R_RotateForEntity( vec3_t origin, cl_entity_t *e )
{
	int		i;
	vec3_t	angles;
	vec3_t	modelpos;

	VectorCopy( origin, modelpos );

	angles[2] = e->angles[2];
	angles[0] = e->angles[0];
	angles[1] = e->angles[1];

	if ( e->curstate.movetype )
	{
		float	f = 0;
		float	d;

		// don't do it if the goalstarttime hasn't updated in a while.
		if ( e->curstate.animtime + 0.2 > cl.time && e->curstate.animtime != e->latched.prevanimtime )
		{
			f = ( cl.time - e->curstate.animtime ) / ( e->curstate.animtime - e->latched.prevanimtime );
		}

		for ( i = 0; i < 3; i++ )
		{
			modelpos[i] -= ( e->latched.prevorigin[i] - e->origin[i] ) * f;
		}

		if ( f > 0.0 && f < 1.5 )
		{
			for ( i = 0; i < 3; i++ )
			{
				d = e->latched.prevangles[i] - e->angles[i];
				if ( d > 180 )
					d -= 360;
				else if ( d < -180 )
					d += 360;

				angles[i] = angles[i] + d * ( 1.0 - f );
			}
		}
	}

	qglTranslatef( modelpos[0], modelpos[1], modelpos[2] );

	qglRotatef( angles[1], 0, 0, 1 );
	qglRotatef( angles[0], 0, 1, 0 );
	qglRotatef( angles[2], 1, 0, 0 );
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel( cl_entity_t *e )
{
	vec3_t	point, forward, right, up;
	mspriteframe_t	*frame;
	float		scale;
	msprite_t	*psprite;
	colorVec	color;

	psprite = (msprite_t *)e->model->cache.data;
	frame = R_GetSpriteFrame( psprite, e->curstate.frame );
	if ( !frame )
	{
		Sys_Warning( "R_DrawSpriteModel:  couldn't get sprite frame for %s\n", e->model->name );
		return;
	}

	if ( e->curstate.scale > 0 )
		scale = e->curstate.scale;
	else
		scale = 1.0;

	if ( e->curstate.rendermode == kRenderNormal )
		r_blend = 1.0;

	R_SpriteColor( &color, e, r_blend * 255 );

	if ( gl_spriteblend.value || e->curstate.rendermode )
	{
		if ( e->curstate.rendermode == kRenderTransColor )
		{
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
			qglColor4ub( color.r, color.g, color.b, r_blend * 255 );
		}
		else if ( e->curstate.rendermode == kRenderTransAdd )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4ub( color.r, color.g, color.b, 255 );
			qglDepthMask( 0 );
		}
		else if ( e->curstate.rendermode == kRenderGlow )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4ub( color.r, color.g, color.b, 255 );
			qglDisable( GL_DEPTH_TEST );
			qglDepthMask( 0 );
		}
		else if ( e->curstate.rendermode == kRenderTransAlpha )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4ub( color.r, color.g, color.b, r_blend * 255 );
			qglDepthMask( 0 );
		}
		else
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4ub( color.r, color.g, color.b, r_blend * 255 );
		}

		qglEnable( GL_BLEND );
	}
	else
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4ub( color.r, color.g, color.b, 255 );
		qglDisable( GL_BLEND );
	}

	R_GetSpriteAxes( e, psprite->type, forward, right, up );

	GL_DisableMultitexture();
	GL_Bind( frame->gl_texturenum );

	qglEnable( GL_ALPHA_TEST );
	qglBegin( GL_QUADS );

	qglTexCoord2f( 0, 1 );
	VectorMA( r_entorigin, scale * frame->down, up, point );
	VectorMA( point, scale * frame->left, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 0, 0 );
	VectorMA( r_entorigin, scale * frame->up, up, point );
	VectorMA( point, scale * frame->left, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 1, 0 );
	VectorMA( r_entorigin, scale * frame->up, up, point );
	VectorMA( point, scale * frame->right, right, point );
	qglVertex3fv( point );

	qglTexCoord2f( 1, 1 );
	VectorMA( r_entorigin, scale * frame->down, up, point );
	VectorMA( point, scale * frame->right, right, point );
	qglVertex3fv( point );

	qglEnd();

	qglDisable( GL_ALPHA_TEST );
	qglDepthMask( 1 );

	if ( e->curstate.rendermode || gl_spriteblend.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		qglDisable( GL_BLEND );
		qglEnable( GL_DEPTH_TEST );
	}
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "anorm_dots.h"
};

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame( aliashdr_t *paliashdr, int posenum )
{
	float 	l;
	trivertx_t	*verts;
	int		*order;
	int		count;

	lastposenum = posenum;

	verts = (trivertx_t *)( (byte *)paliashdr + paliashdr->posedata );
	verts += posenum * paliashdr->poseverts;
	order = (int *)( (byte *)paliashdr + paliashdr->commands );

	while ( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if ( !count )
			break;		// done
		if ( count < 0 )
		{
			count = -count;
			qglBegin( GL_TRIANGLE_FAN );
		}
		else
			qglBegin( GL_TRIANGLE_STRIP );

		do
		{
			// texture coordinates come from the draw list
			qglTexCoord2f( ( (float *)order )[0], ( (float *)order )[1] );
			order += 2;

			// normals and vertexes come from the frame list
			l = shadedots[verts->lightnormalindex] * shadelight;
			qglColor3f( l, l, l );
			qglVertex3f( verts->v[0], verts->v[1], verts->v[2] );
			verts++;
		} while ( --count );

		qglEnd();
	}
}

extern	vec3_t	lightspot;

/*
=============
GL_DrawAliasShadow
=============
*/
void GL_DrawAliasShadow( aliashdr_t *paliashdr, int posenum )
{
	trivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)( (byte *)paliashdr + paliashdr->posedata );
	verts += posenum * paliashdr->poseverts;
	order = (int *)( (byte *)paliashdr + paliashdr->commands );

	height = -lheight + 1.0;

	while ( 1 )
	{
		// get the vertex count and primitive type
		count = *order++;
		if ( !count )
			break;		// done
		if ( count < 0 )
		{
			count = -count;
			qglBegin( GL_TRIANGLE_FAN );
		}
		else
			qglBegin( GL_TRIANGLE_STRIP );

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * ( point[2] + lheight );
			point[1] -= shadevector[1] * ( point[2] + lheight );
			point[2] = height;
//			height -= 0.001;
			qglVertex3fv( point );

			verts++;
		} while ( --count );

		qglEnd();
	}
}

/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame( int frame, aliashdr_t *paliashdr )
{
	int		pose, numposes;
	float	interval;

	if ( ( frame >= paliashdr->numframes ) || ( frame < 0 ) )
	{
		Con_DPrintf( "R_AliasSetupFrame: no such frame %d\n", frame );
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if ( numposes > 1 )
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)( cl.time / interval ) % numposes;
	}

	GL_DrawAliasFrame( paliashdr, pose );
}

/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel( cl_entity_t *e )
{
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	float		an;

	clmodel = currententity->model;

	VectorAdd( currententity->origin, clmodel->mins, mins );
	VectorAdd( currententity->origin, clmodel->maxs, maxs );

	if ( R_CullBox( mins, maxs ) )
		return;

	VectorCopy( currententity->origin, r_entorigin );
	VectorSubtract( r_origin, r_entorigin, modelorg );

	//
	// get lighting information
	//

	// allways give the gun some light
	if ( e == &cl.viewent && ambientlight < 24 )
		ambientlight = shadelight = 24;

	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( cl_dlights[lnum].die >= cl.time )
		{
			VectorSubtract( currententity->origin, cl_dlights[lnum].origin, dist );
			add = cl_dlights[lnum].radius - Length( dist );

			if ( add > 0 )
				ambientlight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if ( ambientlight > 128 )
		ambientlight = 128;
	if ( ambientlight + shadelight > 192 )
		shadelight = 192 - ambientlight;

	if ( !Q_strcmp( clmodel->name, "progs/flame2.mdl" ) || !Q_strcmp( clmodel->name, "progs/flame.mdl" ) )
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[( (int)( e->angles[1] * ( SHADEDOT_QUANT / 360.0 ) ) ) & ( SHADEDOT_QUANT - 1 )];
	shadelight = shadelight / 200.0;

	an = e->angles[1] / 180 * M_PI;
	shadevector[0] = cos( -an );
	shadevector[1] = sin( -an );
	shadevector[2] = 1;
	VectorNormalize( shadevector );

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata( currententity->model );

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//
	qglPushMatrix();
	R_RotateForEntity( e->origin, e );

	qglTranslatef( paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] );
	qglScalef( paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2] );

	GL_Bind( paliashdr->gl_texturenum[currententity->curstate.skin] );

	qglShadeModel( GL_SMOOTH );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );

	R_SetupAliasFrame( currententity->curstate.frame, paliashdr );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	qglShadeModel( GL_FLAT );
	if ( gl_affinemodels.value )
		qglHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	qglPopMatrix();

	if ( r_shadows.value )
	{
		qglPushMatrix();
		R_RotateForEntity( e->origin, e );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_BLEND );
		qglColor4f( 0, 0, 0, 0.5 );
		GL_DrawAliasShadow( paliashdr, lastposenum );
		qglEnable( GL_TEXTURE_2D );
		qglDisable( GL_BLEND );
		qglColor4f( 1, 1, 1, 1 );
		qglPopMatrix();
	}
}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList( void )
{
	int		i, j;

	if ( !r_drawentities.value )
		return;

	// draw sprites seperately, because of alpha blending
	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];

		if ( currententity->curstate.rendermode != kRenderNormal )
		{
			AddTEntity( currententity );
			continue;
		}

		switch ( currententity->model->type )
		{
		case mod_brush:
			R_DrawBrushModel( currententity );
			break;

		case mod_studio:
			if ( currententity->player )
			{
				pStudioAPI->StudioDrawPlayer( STUDIO_RENDER | STUDIO_EVENTS, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
			}
			else
			{
				if ( currententity->curstate.movetype == MOVETYPE_FOLLOW )
				{
					for ( j = 0; j < cl_numvisedicts; j++ )
					{
						if ( cl_visedicts[j]->index == currententity->curstate.aiment )
						{
							currententity = cl_visedicts[j];
							if ( currententity->player )
							{
								pStudioAPI->StudioDrawPlayer( 0, &cl.frames[cl.parsecount & CL_UPDATE_MASK].playerstate[currententity->index - 1] );
							}
							else
							{
								pStudioAPI->StudioDrawModel( 0 );
							}
							currententity = cl_visedicts[i];
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

	r_blend = 1.0;

	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		currententity = cl_visedicts[i];

		if ( currententity->curstate.rendermode != kRenderNormal )
			continue;

		switch ( currententity->model->type )
		{
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
			R_DrawSpriteModel( currententity );
			break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel( void )
{
	float		lightvec[3];
	colorVec	c;
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add, oldShadows;
	dlight_t	*dl;

	lightvec[0] = -1;
	lightvec[1] = 0;
	lightvec[2] = 0;

	currententity = &cl.viewent;

	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || envmap || !r_drawentities.value ||
		cl.stats[STAT_HEALTH] <= 0 || !currententity->model || cl.viewentity > cl.maxclients )
	{
		c = R_LightPoint( currententity->origin );
		cl.light_level = ( c.r + c.g + c.b ) / 3;
		return;
	}

	// hack the depth range to prevent view model from poking into walls
	qglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ) );

	switch ( currententity->model->type )
	{
	case mod_alias:
		c = R_LightPoint( currententity->origin );
		j = ( c.r + c.g + c.b ) / 3;
		if ( j < 24 )
			j = 24;		// allways give some light on gun
		r_viewlighting.ambientlight = j;
		r_viewlighting.shadelight = j;

		// add dynamic lights
		for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
		{
			dl = &cl_dlights[lnum];
			if ( !dl->radius )
				continue;
			if ( !dl->radius )
				continue;
			if ( dl->die < cl.time )
				continue;

			VectorSubtract( currententity->origin, dl->origin, dist );
			add = dl->radius - Length( dist );
			if ( add > 0 )
				r_viewlighting.ambientlight += add;
		}

		// clamp lighting so it doesn't overbright as much
		if ( r_viewlighting.ambientlight > 128 )
			r_viewlighting.ambientlight = 128;
		if ( r_viewlighting.ambientlight + r_viewlighting.shadelight > 192 )
			r_viewlighting.shadelight = 192 - r_viewlighting.ambientlight;

		r_viewlighting.plightvec = lightvec;

		R_DrawAliasModel( currententity );
		break;

	case mod_brush:
		R_DrawBrushModel( currententity );
		break;

	case mod_studio:
		if ( !cl.weaponstarttime )
			cl.weaponstarttime = cl.time;

		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.frame = 0;
		currententity->curstate.framerate = 1;
		currententity->curstate.animtime = cl.weaponstarttime;

		currententity->curstate.colormap = cl.players[cl.playernum].topcolor & 0xFF;
		currententity->curstate.colormap |= ( cl.players[cl.playernum].bottomcolor & 0xFF ) << 8;

		c = R_LightPoint( currententity->origin );
		cl.light_level = ( c.r + c.g + c.b ) / 3;

		oldShadows = r_shadows.value;
		r_shadows.value = 0;
		pStudioAPI->StudioDrawModel( STUDIO_RENDER );
		r_shadows.value = oldShadows;
		break;
	}

	qglDepthRange( gldepthmin, gldepthmax );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}

void R_PreDrawViewModel( void )
{
	int		i;

	currententity = &cl.viewent;

	if ( !r_drawviewmodel.value || ClientDLL_IsThirdPerson() || chase_active.value || envmap || !r_drawentities.value ||
		cl.stats[STAT_HEALTH] <= 0 || !currententity->model || cl.viewentity > cl.maxclients )
		return;

	switch ( currententity->model->type )
	{
	case mod_studio:
		if ( !cl.weaponstarttime )
			cl.weaponstarttime = cl.time;

		currententity->curstate.sequence = cl.weaponsequence;
		currententity->curstate.frame = 0;
		currententity->curstate.framerate = 1;
		currententity->curstate.animtime = cl.weaponstarttime;

		for ( i = 0; i < 4; i++ )
		{
			VectorCopy( cl_entities[currententity->index].origin, currententity->attachment[i] );
		}

		pStudioAPI->StudioDrawModel( STUDIO_EVENTS );
		break;
	}
}

void AllowFog( int allowed )
{
	static int	isFogEnabled;

	if ( allowed )
	{
		if ( isFogEnabled == 1 )
			qglEnable( GL_FOG );
	}
	else
	{
		isFogEnabled = qglIsEnabled( GL_FOG );
		if ( isFogEnabled == 1 )
			qglDisable( GL_FOG );
	}
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend( void )
{
	byte	color[4];
	int		alpha;

	alpha = V_FadeAlpha();
	if ( !alpha )
		return;

	GL_DisableMultitexture();

	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_TEXTURE_2D );

	if ( cl.sf.fadeFlags & FFADE_MODULATE )
	{
		qglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		color[0] = ( cl.sf.fader * alpha + ( 255 - alpha ) * 255 ) >> 8;
		color[1] = ( cl.sf.fadeg * alpha + ( 255 - alpha ) * 255 ) >> 8;
		color[2] = ( cl.sf.fadeb * alpha + ( 255 - alpha ) * 255 ) >> 8;
		color[3] = 255;
	}
	else
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		color[0] = cl.sf.fader;
		color[1] = cl.sf.fadeg;
		color[2] = cl.sf.fadeb;
		color[3] = alpha;
	}

	AllowFog( false );

	qglViewport( glx, gly, glwidth, glheight );

	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, glwidth, glheight, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();

	qglDisable( GL_CULL_FACE );

	qglColor4ubv( color );

	qglBegin( GL_QUADS );

	qglVertex2f( 0, 0 );
	qglVertex2f( 0, glheight );
	qglVertex2f( glwidth, glheight );
	qglVertex2f( glwidth, 0 );

	qglEnd();

	qglPopMatrix();
	qglMatrixMode( GL_PROJECTION );
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_CULL_FACE );
	qglEnable( GL_TEXTURE_2D );
	qglEnable( GL_ALPHA_TEST );

	AllowFog( true );
}

int SignbitsForPlane( mplane_t *out )
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for ( j = 0; j < 3; j++ )
	{
		if ( out->normal[j] < 0 )
			bits |= 1 << j;
	}
	return bits;
}

void R_SetFrustum( void )
{
	int		i;
	float	fovx, fovy;

	fovx = scr_fov_value;
	fovy = CalcFov( fovx, glwidth, glheight );

	fovx *= 0.5;
	fovy *= 0.5;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -( 90 - fovx ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90 - fovx );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90 - fovy );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - fovy ) );

	for ( i = 0; i < 4; i++ )
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct( r_origin, frustum[i].normal );
		frustum[i].signbits = SignbitsForPlane( &frustum[i] );
	}
}

void R_ForceCVars( qboolean mp )
{
	if ( mp )
	{
		if ( r_lightmap.value != 0 )
			Cvar_DirectSet( &r_lightmap, "0" );

		if ( gl_clear.value != 0 )
			Cvar_DirectSet( &gl_clear, "0" );

		if ( r_novis.value != 0 )
			Cvar_DirectSet( &r_novis, "0" );

		if ( r_fullbright.value != 0 )
			Cvar_DirectSet( &r_fullbright, "0" );

		if ( snd_show.value != 0 )
			Cvar_DirectSet( &snd_show, "0" );

		if ( chase_active.value != 0 )
			Cvar_DirectSet( &chase_active, "0" );

		if ( v_lambert.value != 1.5 )
			Cvar_DirectSet( &v_lambert, "1.5" );

		if ( gl_monolights.value != 0 )
		{
			Cvar_DirectSet( &gl_monolights, "0" );
			GL_BuildLightmaps();
		}

		if ( gl_wireframe.value != 0 )
			Cvar_DirectSet( &gl_wireframe, "0" );

		if ( r_dynamic.value != 1.0 )
			Cvar_DirectSet( &r_dynamic, "1" );

		if ( gl_alphamin.value != 0.25 )
			Cvar_DirectSet( &gl_alphamin, "0.25" );

		if ( gl_max_size.value < 255 )
			Cvar_DirectSet( &gl_max_size, "256" );

		if ( gl_polyoffset.value > 0 )
		{
			if ( gl_polyoffset.value < 0.001 )
				Cvar_DirectSet( &gl_polyoffset, "0.001" );
			else if ( gl_polyoffset.value > 25 )
				Cvar_DirectSet( &gl_polyoffset, "25" );
		}
		else if ( gl_polyoffset.value < -0.001 )
			Cvar_DirectSet( &gl_polyoffset, "-0.001" );

		if ( !allow_cheats && r_drawentities.value != 1.0 )
			Cvar_DirectSet( &r_drawentities, "1.0" );

		if ( v_lightgamma.value < 1.8 )
		{
			Cvar_DirectSet( &v_lightgamma, "1.8" );
			GL_BuildLightmaps();
			D_FlushCaches();
		}
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame( void )
{
	R_ForceCVars( cl.maxclients > 1 );

	R_CheckVariables();

	R_AnimateLight();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy( r_refdef.vieworg, r_origin );

	AngleVectors( r_refdef.viewangles, vpn, vright, vup );

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf( r_origin, cl.worldmodel );

	if ( cl.waterlevel > 2 && !r_refdef.onlyClientDraws )
	{
		float	fogColor[4];

		fogColor[0] = cshift_water.destcolor[0] / 255.0;
		fogColor[1] = cshift_water.destcolor[1] / 255.0;
		fogColor[2] = cshift_water.destcolor[2] / 255.0;
		fogColor[3] = 1.0;

		qglFogi( GL_FOG_MODE, GL_LINEAR );
		qglFogfv( GL_FOG_COLOR, fogColor );
		qglFogf( GL_FOG_START, 0 );
		qglFogf( GL_FOG_END, 1536 - cshift_water.percent * 4 );
		qglEnable( GL_FOG );
	}

	V_CalcBlend();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble	xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

float CalcFov( float fov_x, float width, float height )
{
	float	a;
	float	x;

	if ( fov_x < 1 || fov_x > 179 )
		fov_x = 90;	// error, set to 90

	x = width / tan( fov_x / 360 * M_PI );

	a = atan( height / x );

	a = a * 360 / M_PI;

	return a;
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL( void )
{
	float	screenaspect;
	float	yfov;
	int		i, j, k;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();

	x = r_refdef.vrect.x;
	x2 = r_refdef.vrect.x + r_refdef.vrect.width;
	y = glheight - r_refdef.vrect.y;
	y2 = glheight - ( r_refdef.vrect.y + r_refdef.vrect.height );

	// fudge around because of frac screen scale
	if ( x > 0 )
		x--;
	if ( x2 < glwidth )
		x2++;
	if ( y2 < 0 )
		y2--;
	if ( y < glheight )
		y++;

	w = x2 - x;
	h = y - y2;

	if ( envmap )
	{
		x = y2 = 0;
		w = h = gl_envmapsize.value;
		glwidth = glheight = gl_envmapsize.value;
	}

	qglViewport( glx + x, gly + y2, w, h );

	screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;
	yfov = CalcFov( scr_fov_value, r_refdef.vrect.width, r_refdef.vrect.height );

	if ( r_refdef.onlyClientDraws )
	{
		MYgluPerspective( yfov, screenaspect, 4, 16000 );
	}
	else if ( CL_IsDevOverviewMode() )
	{
		x = 4096 / gDevOverview.zoom;
		y = 4096 / ( gDevOverview.zoom * screenaspect );

		qglOrtho( -x, x, -y, y, 16000 - gDevOverview.z_min, 16000 - gDevOverview.z_max );
	}
	else
	{
		MYgluPerspective( yfov, screenaspect, 4, movevars.zmax );
	}

	if ( mirror )
	{
		if ( mirror_plane->normal[2] )
			qglScalef( 1, -1, 1 );
		else
			qglScalef( -1, 1, 1 );
		qglCullFace( GL_BACK );
	}
	else
		qglCullFace( GL_FRONT );

	qglGetFloatv( GL_PROJECTION_MATRIX, gProjectionMatrix );

	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	qglRotatef( -90, 1, 0, 0 );	    // put Z going up
	qglRotatef( 90, 0, 0, 1 );	    // put Z going up
	qglRotatef( -r_refdef.viewangles[2], 1, 0, 0 );
	qglRotatef( -r_refdef.viewangles[0], 0, 1, 0 );
	qglRotatef( -r_refdef.viewangles[1], 0, 0, 1 );
	qglTranslatef( -r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2] );

	qglGetFloatv( GL_MODELVIEW_MATRIX, r_world_matrix );

	//
	// set drawing parms
	//
	if ( gl_cull.value )
		qglEnable( GL_CULL_FACE );
	else
		qglDisable( GL_CULL_FACE );

	qglDisable( GL_BLEND );
	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_DEPTH_TEST );

	if ( gl_flipmatrix.value )
	{
		for ( i = 0; i < 4; i++ )
		{
			for ( j = 0; j < 4; j++ )
			{
				gWorldToScreen[i * 4 + j] = 0;
				for ( k = 0; k < 4; k++ )
				{
					gWorldToScreen[i * 4 + j] += r_world_matrix[k * 4 + i] * gProjectionMatrix[j * 4 + k];
				}
			}
		}
	}
	else
	{
		for ( i = 0; i < 4; i++ )
		{
			for ( j = 0; j < 4; j++ )
			{
				gWorldToScreen[i * 4 + j] = 0;
				for ( k = 0; k < 4; k++ )
				{
					gWorldToScreen[i * 4 + j] += r_world_matrix[i * 4 + k] * gProjectionMatrix[k * 4 + j];
				}
			}
		}
	}

	InvertMatrix( gWorldToScreen, gScreenToWorld );
}

/*
=============
R_Clear
=============
*/
void R_Clear( void )
{
	R_ForceCVars( cl.maxclients > 1 );

	if ( r_mirroralpha.value != 1.0 )
	{
		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		else
			qglClear( GL_DEPTH_BUFFER_BIT );
		gldepthmin = 0;
		gldepthmax = 0.5;
		qglDepthFunc( GL_LEQUAL );
	}
	else if ( gl_ztrick.value )
	{
		static int trickframe;

		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT );

		trickframe++;
		if ( trickframe & 1 )
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc( GL_LEQUAL );
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc( GL_GEQUAL );
		}
	}
	else
	{
		if ( gl_clear.value )
			qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		else
			qglClear( GL_DEPTH_BUFFER_BIT );
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc( GL_LEQUAL );
	}

	qglDepthRange( gldepthmin, gldepthmax );
}

void R_LoadOverviewSpriteModel( model_t *mod, byte *pdata, int XTiles, int yTiles )
{
	mspriteframe_t	*pspriteframe;
	int		width, height, origin[2], textureType;
	char	name[256];
	int		numframes;
	int		size;
	int		i;
	msprite_t	*psprite;

	numframes = yTiles * XTiles;

	size = sizeof( msprite_t ) + ( numframes - 1 ) * sizeof( psprite->frames );

	psprite = Hunk_AllocName( size, mod->name );

	mod->cache.data = psprite;

	psprite->type = SPR_VP_PARALLEL_ORIENTED;
	psprite->texFormat = SPR_ALPHTEST;
	psprite->maxwidth = 128;
	psprite->maxheight = 128;
	psprite->beamlength = 0;
	mod->synctype = ST_SYNC;
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth / 2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth / 2;
	mod->mins[2] = -psprite->maxheight / 2;
	mod->maxs[2] = psprite->maxheight / 2;

	mod->numframes = numframes;
	mod->flags = 0;

	for ( i = 0; i < numframes; i++ )
	{
		psprite->frames[i].type = SPR_SINGLE;

		width = 128;
		height = 128;
		size = width * height;

		pspriteframe = Hunk_AllocName( sizeof( mspriteframe_t ), mod->name );
		Q_memset( pspriteframe, 0, sizeof( mspriteframe_t ) );

		psprite->frames[i].frameptr = pspriteframe;

		pspriteframe->width = width;
		pspriteframe->height = height;
		origin[0] = 0;
		origin[1] = 0;
		pspriteframe->up = origin[1];
		pspriteframe->down = origin[1] - height;
		pspriteframe->left = origin[0];
		pspriteframe->right = origin[0] + width;

		Mod_SpriteTextureName( name, mod->name, i );

		textureType = 4;
		pspriteframe->gl_texturenum = GL_LoadTexture( name, GLT_SPRITE, width, height, pdata, false, textureType, NULL );

		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

		pdata += 128 * 128 * 4;
	}

	mod->type = mod_sprite;
}

model_t *R_LoadMapSprite( const char *szFilename )
{
	char	filename[MAX_PATH];
	int		i, x, y, width, height, palCount;
	int		xStep, yStep;
	unsigned int	*yStart1;
	unsigned int	*yStart2;
	model_t	*mod;
	int		colorBytes;
	byte	*buffer1;
	byte	*buffer2;
	byte	*buffer3;

	colorBytes = 0;
	buffer1 = NULL;
	buffer2 = NULL;
	buffer3 = NULL;

	g_engdstAddrs.LoadMapSprite( &szFilename );

	Q_strncpy( filename, szFilename, sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;
	Q_strlwr( filename );

	mod = Mod_FindName( false, filename );
	mod->needload = 0;

	if ( !Q_strcmp( "tga", filename + Q_strlen( filename ) - 3 ) )
	{
		colorBytes = 4;
	}
	else if ( !Q_strcmp( "bmp", filename + Q_strlen( filename ) - 3 ) )
	{
		colorBytes = 1;
	}
	else
	{
		colorBytes = 0;
		Sys_Error( "Unsupported map image file format.\n" );
		return NULL;
	}

	buffer1 = Mem_Malloc( 1024 * 1024 * 3 );
	if ( !buffer1 )
	{
		Sys_Error( "Not enough memory to load map images (1).\n" );
		return NULL;
	}

	if ( colorBytes == 4 )
	{
		if ( !LoadTGA( filename, buffer1, 1024 * 1024 * 3, &width, &height ) )
		{
			Con_Printf( "Couldn't open %s\n", szFilename );
			Mem_Free( buffer1 );
			Mem_Free( buffer2 );
			return NULL;
		}

		for ( i = 0; i < width * height; i++ )
		{
			if ( buffer1[i * 4] == 0 && buffer1[i * 4 + 1] == 255 && buffer1[i * 4 + 2] == 0 )
				*(unsigned int *)&buffer1[i * 4] = 0;
		}
	}
	else
	{
		FileHandle_t	hFile;
		int		c, r, g, b;
		byte	*packPalette;

		packPalette = NULL;

		hFile = FS_Open( filename, "rb" );
		if ( !hFile )
		{
			Con_Printf( "Couldn't open %s\n", filename );
			Mem_Free( buffer1 );
			Mem_Free( buffer2 );
			return NULL;
		}

		LoadBMP8( hFile, &packPalette, &palCount, &buffer3, &width, &height );

		for ( i = 0; i < width * height; i++ )
		{
			c = buffer3[i];
			r = packPalette[c * 3];
			buffer1[i * 4] = r;
			g = packPalette[c * 3 + 1];
			buffer1[i * 4 + 1] = g;
			b = packPalette[c * 3 + 2];
			buffer1[i * 4 + 2] = b;

			if ( r == 0 && g == 255 && b == 0 )
				*(unsigned int *)&buffer1[i * 4] = 0;
			else
				buffer1[i * 4 + 3] = 255;
		}

		if ( buffer3 )
			Mem_Free( buffer3 );
		if ( packPalette )
			Mem_Free( packPalette );
	}

	if ( !( width * height ) || width % 128 || height % 128 )
	{
		Con_Printf( "Wrong map image dimesions in %s\n", filename );
		Mem_Free( buffer1 );
		Mem_Free( buffer2 );
		return NULL;
	}

	buffer2 = Mem_Malloc( 1024 * 1024 * 3 );
	if ( !buffer2 )
	{
		Sys_Error( "Not enough memory to load map images (2).\n" );
		return NULL;
	}

	yStart2 = (unsigned int *)buffer2;

	for ( yStep = 0; yStep < height; yStep += 128 )
	{
		for ( xStep = 0; xStep < width; xStep += 128 )
		{
			yStart1 = (unsigned int *)buffer1 + ( xStep + yStep * width );

			for ( y = 0; y < 128; y++ )
			{
				for ( x = 0; x < 128; x++ )
				{
					*yStart2++ = *yStart1++;
				}

				yStart1 += width - 128;
			}
		}
	}

	R_LoadOverviewSpriteModel( mod, buffer2, width / 128, height / 128 );

	Mem_Free( buffer1 );
	Mem_Free( buffer2 );

	return mod;
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene( void )
{
	if ( gD3DMode != vid_d3d.value )
		Cvar_DirectSet( &vid_d3d, gD3DMode ? "1" : "0" );

	if ( CL_IsDevOverviewMode() )
		CL_SetDevOverView( &r_refdef );

	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves();	// done here so we know if we're in water

	if ( !r_refdef.onlyClientDraws )
	{
		if ( CL_IsDevOverviewMode() )
		{
			qglClearColor( 0, 1, 0, 0 );
			qglClear( GL_COLOR_BUFFER_BIT );
		}

		R_DrawWorld();		// adds static entities to the list

		S_ExtraUpdate();	// don't let sound get messed up if going slow

		R_DrawEntitiesOnList();
	}

	if ( g_bUserFogOn )
		R_RenderFinalFog();

	AllowFog( false );
	ClientDLL_DrawNormalTriangles();
	AllowFog( true );

	if ( ( cl.waterlevel > 2 && !r_refdef.onlyClientDraws ) || !g_bUserFogOn )
		qglDisable( GL_FOG );

	R_DrawTEntitiesOnList( r_refdef.onlyClientDraws );

	S_ExtraUpdate();

	if ( !r_refdef.onlyClientDraws )
	{
		R_RenderDlights();

		GL_DisableMultitexture();

		R_DrawParticles();
	}
}

void R_SetStackBase( void )
{
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView( void )
{
	double	time1, time2;

	if ( r_norefresh.value )
		return;

	if ( !r_worldentity.model || !cl.worldmodel )
		Sys_Error( "R_RenderView: NULL worldmodel" );

	if ( r_speeds.value )
	{
		qglFinish();
		time1 = Sys_FloatTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	R_Clear();

	if ( !r_refdef.onlyClientDraws )
		R_PreDrawViewModel();

	// render normal view
	R_RenderScene();

	if ( !r_refdef.onlyClientDraws )
	{
		R_DrawViewModel();
		R_PolyBlend();
	}

	S_ExtraUpdate();

	if ( r_speeds.value )
	{
		float	framerate = cl.time - cl.oldtime;

		if ( framerate > 0 )
			framerate = 1 / framerate;

		time2 = Sys_FloatTime();

		Con_Printf( "%3ifps %3i ms  %4i wpoly %4i epoly\n", (int)( framerate + 0.5 ), (int)( ( time2 - time1 ) * 1000 ), c_brush_polys, c_alias_polys );
	}
}
