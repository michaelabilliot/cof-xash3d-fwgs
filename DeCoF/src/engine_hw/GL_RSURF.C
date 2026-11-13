// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "view.h"
#include "host.h"
#include "gl_water.h"

#include <math.h>

int32	RandomLong( int32 lLow, int32 lHigh );
void	GL_SelectTexture( GLenum target );
void	GL_Bind( int texnum );

void	R_RenderDynamicLightmaps( msurface_t *fa );
void	R_DrawDecals( qboolean bMultitexture );
void	DrawGLSolidPoly( glpoly_t *p );
void	DrawGLPolyScroll( msurface_t *psurface, cl_entity_t *pEntity );
float	ScrollOffset( msurface_t *psurface, cl_entity_t *pEntity );
int		DT_SetRenderState( int diffuseId );
void	DT_SetTextureCoordinates( float u, float v );
void	DT_ClearRenderState( void );

void	S_ExtraUpdate( void );
void	AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );
qboolean	R_CullBox( vec3_t mins, vec3_t maxs );
void	R_RotateForEntity( vec3_t origin, cl_entity_t *e );
void	R_MarkLights( dlight_t *light, int bit, mnode_t *node );
void	R_StoreEfrags( struct efrag_s **ppefrag );
void	R_SetRenderMode( cl_entity_t *pEntity );
void	R_RenderBrushPoly( msurface_t *fa );
void	R_BlendLightmaps( void );
void	R_DrawSequentialPoly( msurface_t *s, int face );
byte	*Mod_LeafPVS( mleaf_t *leaf, model_t *model );
int		CL_IsDevOverviewMode( void );
float	VectorNormalize( vec3_t v );
void	R_DecalCreate( msurface_t *psurface, int textureIndex, float scale, float x, float y );
int		R_DecalUnProject( decal_t *pdecal, vec_t *position );
void	R_InvalidateSurface( msurface_t *surface );
texture_t	*Draw_DecalTexture( int index );

extern	int		texture_extension_number_guess;

#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

extern	cvar_t	v_lightgamma;
extern	int		gl_FilterTexture;

int			skytexturenum;

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64

int			lightmap_bytes;		// 1, 2, or 4
int			lightmap_used;

int			lightmap_textures;

colorVec	blocklights[18*18];

int			active_lightmaps;

typedef struct glRect_s
{
	int		l, t, w, h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

#define MAX_DECALSURFS		500

static msurface_t	*gDecalSurfs[MAX_DECALSURFS];
static int			gDecalSurfCount;

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

msurface_t	*skychain = NULL;
msurface_t	*waterchain = NULL;

/*
===============
R_AddDynamicLights
===============
*/
static void R_AddDynamicLights( msurface_t *surf )
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		local, impact;
	int			s, t;
	int			smax, tmax;
	mtexinfo_t	*tex;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;
	tex = surf->texinfo;

	for ( lnum = 0; lnum < MAX_DLIGHTS; lnum++ )
	{
		if ( !( surf->dlightbits & ( 1 << lnum ) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		VectorSubtract( cl_dlights[lnum].origin, currententity->origin, impact );

		dist = DotProduct( impact, surf->plane->normal ) - surf->plane->dist;
		rad -= fabs( dist );
		minlight = cl_dlights[lnum].minlight;
		if ( rad < minlight )
			continue;
		minlight = rad - minlight;

		local[0] = DotProduct( impact, tex->vecs[0] ) + tex->vecs[0][3];
		local[1] = DotProduct( impact, tex->vecs[1] ) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for ( t = 0; t < tmax; t++ )
		{
			td = local[1] - t * 16;
			if ( td < 0 )
				td = -td;
			for ( s = 0; s < smax; s++ )
			{
				sd = local[0] - s * 16;
				if ( sd < 0 )
					sd = -sd;
				if ( sd > td )
					dist = sd + ( td >> 1 );
				else
					dist = td + ( sd >> 1 );
				if ( dist < minlight )
				{
					int32	delta;

					delta = ( rad - dist ) * 256;
					blocklights[ t * smax + s ].r += ( delta * cl_dlights[lnum].color.r ) >> 8;
					blocklights[ t * smax + s ].g += ( delta * cl_dlights[lnum].color.g ) >> 8;
					blocklights[ t * smax + s ].b += ( delta * cl_dlights[lnum].color.b ) >> 8;
				}
			}
		}
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap( msurface_t *psurf, byte *dest, int stride )
{
	int			smax, tmax;
	int			t;
	int			i, j, k, size;
	color24		*lightmap;
	colorVec	*bl;
	unsigned	scale;
	unsigned	lightscale;
	int			maps;
	int			maxSize;

	if ( gl_overbright.value && gl_texsort )
		lightscale = 256;
	else
		lightscale = pow( 2.0, 1.0 / v_lightgamma.value ) * 256 + 0.5;

	psurf->cached_dlight = ( psurf->dlightbits & r_dlightactive );
	psurf->dlightbits &= r_dlightactive;

	smax = ( psurf->extents[0] >> 4 ) + 1;
	tmax = ( psurf->extents[1] >> 4 ) + 1;
	size = smax * tmax;
	lightmap = psurf->samples;

	maxSize = 18 * 18;
	if ( size > maxSize )
	{
		Sys_Error( "Error: lightmap for texture %s too large (%d x %d = %d luxels); cannot exceed %d\n", psurf->texinfo->texture->name, smax, tmax, smax * tmax, maxSize );
		return;
	}

	if ( filterMode )
	{
		for ( i = 0; i < size; i++ )
		{
			blocklights[i].r = 0xff00 * filterColorRed * filterBrightness;
			blocklights[i].g = 0xff00 * filterColorGreen * filterBrightness;
			blocklights[i].b = 0xff00 * filterColorBlue * filterBrightness;
		}
	}
// set to full bright if no light data
	else if ( r_fullbright.value || !cl.worldmodel->lightdata )
	{
		for ( i = 0; i < size; i++ )
		{
			blocklights[i].r = 0xff00;
			blocklights[i].g = 0xff00;
			blocklights[i].b = 0xff00;
		}
	}
	else
	{
// clear to no light
		for ( i = 0; i < size; i++ )
		{
			blocklights[i].r = 0;
			blocklights[i].g = 0;
			blocklights[i].b = 0;
		}

// add all the lightmaps
		if ( lightmap )
		{
			for ( maps = 0; maps < MAXLIGHTMAPS && psurf->styles[maps] != 255; maps++ )
			{
				scale = d_lightstylevalue[ psurf->styles[maps] ];
				psurf->cached_light[maps] = scale;	// 8.8 fraction
				for ( i = 0; i < size; i++ )
				{
					blocklights[i].r += lightmap[i].r * scale;
					blocklights[i].g += lightmap[i].g * scale;
					blocklights[i].b += lightmap[i].b * scale;
				}
				lightmap += size;	// skip to next lightmap
			}
		}

// add all the dynamic lights
		if ( psurf->dlightframe == r_framecount )
			R_AddDynamicLights( psurf );
	}

// bound, invert, and shift
	switch ( gl_lightmap_format )
	{
	case GL_RGBA:
		stride -= ( smax << 2 );
		bl = blocklights;
		for ( i = 0; i < tmax; i++, dest += stride )
		{
			for ( j = 0; j < smax; j++ )
			{
				for ( k = 0; k < 3; k++ )
				{
					t = ( (int *)bl )[k];
					t = t * lightscale >> 14;
					if ( t > 1023 )
						t = 1023;
					dest[k] = lightgammatable[t] >> 2;
				}
				bl++;

				if ( gl_monolights.value )
				{
					dest[3] = 255 - ( ( dest[0] * 76 + dest[1] * 149 + dest[2] * 29 ) >> 8 );
					dest[0] = dest[1] = dest[2] = 255;
				}
				else
				{
					dest[3] = 255;
				}
				dest += 4;
			}
		}
		break;

	case GL_LUMINANCE:
	case GL_INTENSITY:
		bl = blocklights;
		for ( i = 0; i < tmax; i++, dest += stride )
		{
			for ( j = 0; j < smax; j++ )
			{
				t = bl->r;
				bl++;
				t >>= 8;
				if ( t > 255 )
					t = 255;
				dest[j] = 255 - t;
			}
		}
		break;

	default:
		Sys_Error( "Bad lightmap format" );
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation( msurface_t *s )
{
	texture_t	*base;
	int			reletive;
	int			count;
	static int	rtable[20][20];
	int			tu, tv;
	texture_t	*tm2;

	base = s->texinfo->texture;

	if ( !rtable[0][0] )
	{
		for ( tu = 0; tu < 20; tu++ )
			for ( tv = 0; tv < 20; tv++ )
				rtable[tu][tv] = RandomLong( 0, 0x7fff );
	}

	if ( currententity->curstate.frame && base->alternate_anims )
		base = base->alternate_anims;

	if ( !base->anim_total )
		return base;

	if ( base->name[0] == '-' && base->anim_total )
	{
		tu = s->texturemins[0] + ( base->width << 16 );
		tu = tu / base->width;
		tv = s->texturemins[1] + ( base->height << 16 );
		tv = tv / base->height;

		reletive = rtable[tu % 20][tv % 20] % base->anim_total;
		count = 0;
		tm2 = base;
		while ( tm2->anim_min > reletive || tm2->anim_max <= reletive )
		{
			tm2 = tm2->anim_next;
			if ( !tm2 )
				Sys_Error( "R_TextureAnimation: broken cycle" );
			if ( ++count > 100 )
				Sys_Error( "R_TextureAnimation: infinite cycle" );
		}
		base = tm2;
	}
	else
	{
		reletive = (int)( cl.time * 10 ) % base->anim_total;
		count = 0;
		while ( base->anim_min > reletive || base->anim_max <= reletive )
		{
			base = base->anim_next;
			if ( !base )
				Sys_Error( "R_TextureAnimation: broken cycle" );
			if ( ++count > 100 )
				Sys_Error( "R_TextureAnimation: infinite cycle" );
		}
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

lpMTexFUNC		glMTexCoord2fSGIS = NULL;
lpSelTexFUNC	qglSelectTextureSGIS = NULL;

qboolean	mtexenabled = false;

/*
================
GL_DisableMultitexture
================
*/
void GL_DisableMultitexture( void )
{
	if ( mtexenabled )
	{
		qglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( TEXTURE0_SGIS );
		mtexenabled = false;
	}
}

/*
================
GL_EnableMultitexture
================
*/
void GL_EnableMultitexture( void )
{
	if ( gl_mtexable )
	{
		GL_SelectTexture( TEXTURE1_SGIS );
		qglEnable( GL_TEXTURE_2D );
		mtexenabled = true;
	}
}

/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly( msurface_t *s, int face )
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;
	glRect_t	*theRect;
	int			renderDetailTexture;

	if ( currententity->curstate.rendermode == kRenderTransColor )
	{
		GL_DisableMultitexture();
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
		t = R_TextureAnimation( s );
		GL_Bind( t->gl_texturenum );
		qglDisable( GL_TEXTURE_2D );
		DrawGLSolidPoly( s->polys );
		qglEnable( GL_TEXTURE_2D );
		GL_EnableMultitexture();
		return;
	}

	//
	// normal lightmaped poly
	//
	if ( !( s->flags & ( SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER ) ) )
	{
		if ( !filterMode )
			R_RenderDynamicLightmaps( s );

		if ( gl_mtexable && ( currententity->curstate.rendermode == kRenderNormal || currententity->curstate.rendermode == kRenderTransAlpha ) )
		{
			p = s->polys;

			t = R_TextureAnimation( s );
			// Binds world to texture env 0
			GL_SelectTexture( TEXTURE0_SGIS );
			GL_Bind( t->gl_texturenum );
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

			if ( currententity->curstate.rendermode == kRenderTransColor )
				qglDisable( GL_TEXTURE_2D );

			// Binds lightmap to texenv 1
			GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
			if ( filterMode )
				GL_Bind( gl_FilterTexture );
			else
				GL_Bind( lightmap_textures + s->lightmaptexturenum );

			i = s->lightmaptexturenum;
			if ( lightmap_modified[i] )
			{
				lightmap_modified[i] = false;
				theRect = &lightmap_rectchange[i];
				qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, theRect->t, BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmaps + ( i * BLOCK_HEIGHT + theRect->t ) * BLOCK_WIDTH * lightmap_bytes );
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

			renderDetailTexture = DT_SetRenderState( t->gl_texturenum );

			qglBegin( GL_POLYGON );
			v = p->verts[0];
			if ( s->flags & SURF_DRAWTILED )
			{
				float	sOffset;

				sOffset = ScrollOffset( s, currententity );
				for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
				{
					qglMTexCoord2fSGIS( TEXTURE0_SGIS, v[3] + sOffset, v[4] );
					qglMTexCoord2fSGIS( TEXTURE1_SGIS, v[5], v[6] );
					if ( renderDetailTexture )
						DT_SetTextureCoordinates( v[3] + sOffset, v[4] );
					qglVertex3fv( v );
				}
			}
			else
			{
				for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
				{
					qglMTexCoord2fSGIS( TEXTURE0_SGIS, v[3], v[4] );
					qglMTexCoord2fSGIS( TEXTURE1_SGIS, v[5], v[6] );
					if ( renderDetailTexture )
						DT_SetTextureCoordinates( v[3], v[4] );
					qglVertex3fv( v );
				}
			}
			qglEnd();

			if ( renderDetailTexture )
				DT_ClearRenderState();

			if ( !gl_texsort && s->pdecals )
			{
				gDecalSurfs[ gDecalSurfCount ] = s;
				gDecalSurfCount++;
				if ( gDecalSurfCount > MAX_DECALSURFS )
					Sys_Error( "Too many decal surfaces!\n" );

				if ( currententity->curstate.rendermode == kRenderNormal )
					R_DrawDecals( true );
			}

			if ( gl_wireframe.value )
			{
				qglDisable( GL_TEXTURE_2D );
				qglColor3f( 1, 1, 1 );
				if ( gl_wireframe.value == 2 )
					qglDisable( GL_DEPTH_TEST );
				qglBegin( GL_LINE_LOOP );
				v = p->verts[0];
				for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
					qglVertex3fv( v );
				qglEnd();
				qglEnable( GL_TEXTURE_2D );
				if ( gl_wireframe.value == 2 )
					qglEnable( GL_DEPTH_TEST );
			}

			return;
		}
		else
		{
			p = s->polys;

			t = R_TextureAnimation( s );
			GL_DisableMultitexture();
			GL_Bind( t->gl_texturenum );

			if ( s->flags & SURF_DRAWTILED )
			{
				DrawGLPolyScroll( s, currententity );
			}
			else
			{
				qglBegin( GL_POLYGON );
				v = p->verts[0];
				for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
				{
					qglTexCoord2f( v[3], v[4] );
					qglVertex3fv( v );
				}
				qglEnd();
			}

			if ( !gl_texsort && s->pdecals )
			{
				gDecalSurfs[ gDecalSurfCount ] = s;
				gDecalSurfCount++;
				if ( gDecalSurfCount > MAX_DECALSURFS )
					Sys_Error( "Too many decal surfaces!\n" );

				R_DrawDecals( false );
			}

			if ( gl_wireframe.value )
			{
				qglDisable( GL_TEXTURE_2D );
				qglColor3f( 1, 1, 1 );
				if ( gl_wireframe.value == 2 )
					qglDisable( GL_DEPTH_TEST );
				qglBegin( GL_LINE_LOOP );
				v = p->verts[0];
				for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
					qglVertex3fv( v );
				qglEnd();
				qglEnable( GL_TEXTURE_2D );
				if ( gl_wireframe.value == 2 )
					qglEnable( GL_DEPTH_TEST );
			}

			if ( currententity->curstate.rendermode != kRenderNormal )
				return;

			if ( filterMode )
				GL_Bind( gl_FilterTexture );
			else
				GL_Bind( lightmap_textures + s->lightmaptexturenum );

			qglEnable( GL_BLEND );
			qglBegin( GL_POLYGON );
			v = p->verts[0];
			for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
			{
				qglTexCoord2f( v[5], v[6] );
				qglVertex3fv( v );
			}
			qglEnd();

			qglDisable( GL_BLEND );
		}

		return;
	}

	//
	// subdivided water surface warp
	//
	if ( s->flags & SURF_DRAWTURB )
	{
		GL_DisableMultitexture();
		GL_Bind( s->texinfo->texture->gl_texturenum );
		EmitWaterPolys( s, face );
		return;
	}
}


/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps( msurface_t *fa )
{
	byte		*base;
	int			maps;
	glRect_t	*theRect;
	int			smax, tmax;

	c_brush_polys++;

	if ( fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
		return;

	fa->polys->chain = lightmap_polys[ fa->lightmaptexturenum ];
	lightmap_polys[ fa->lightmaptexturenum ] = fa->polys;

	// check for lightmap modification
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
		if ( d_lightstylevalue[ fa->styles[maps] ] != fa->cached_light[maps] )
			goto dynamic;

	if ( fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight )			// dynamic previously
	{
dynamic:
		if ( r_dynamic.value )
		{
			lightmap_modified[ fa->lightmaptexturenum ] = true;
			theRect = &lightmap_rectchange[ fa->lightmaptexturenum ];
			if ( fa->light_t < theRect->t )
			{
				if ( theRect->h )
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if ( fa->light_s < theRect->l )
			{
				if ( theRect->w )
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = ( fa->extents[0] >> 4 ) + 1;
			tmax = ( fa->extents[1] >> 4 ) + 1;
			if ( ( theRect->w + theRect->l ) < ( fa->light_s + smax ) )
				theRect->w = ( fa->light_s - theRect->l ) + smax;
			if ( ( theRect->h + theRect->t ) < ( fa->light_t + tmax ) )
				theRect->h = ( fa->light_t - theRect->t ) + tmax;
			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap( fa, base, BLOCK_WIDTH * lightmap_bytes );
		}
	}
}

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly( glpoly_t *p )
{
	int		i;
	float	*v;
	vec3_t	nv;

	qglBegin( GL_TRIANGLE_FAN );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );

		nv[0] = v[0] + 8 * sin( v[1] * 0.05 + realtime ) * sin( v[2] * 0.05 + realtime );
		nv[1] = v[1] + 8 * sin( v[0] * 0.05 + realtime ) * sin( v[2] * 0.05 + realtime );
		nv[2] = v[2];

		qglVertex3fv( nv );
	}
	qglEnd();
}

void DrawGLWaterPolyLightmap( glpoly_t *p )
{
	int		i;
	float	*v;
	vec3_t	nv;

	qglBegin( GL_TRIANGLE_FAN );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[5], v[6] );

		nv[0] = v[0] + 8 * sin( v[1] * 0.05 + realtime ) * sin( v[2] * 0.05 + realtime );
		nv[1] = v[1] + 8 * sin( v[0] * 0.05 + realtime ) * sin( v[2] * 0.05 + realtime );
		nv[2] = v[2];

		qglVertex3fv( nv );
	}
	qglEnd();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly( glpoly_t *p )
{
	int		i;
	float	*v;

	qglBegin( GL_POLYGON );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}

/*
================
ScrollOffset
================
*/
float ScrollOffset( msurface_t *psurface, cl_entity_t *pEntity )
{
	float	speed, sOffset;

	speed = ( ( pEntity->curstate.rendercolor.g << 8 ) + pEntity->curstate.rendercolor.b ) * ( 1.0 / 16 );
	if ( pEntity->curstate.rendercolor.r == 0 )
		speed = -speed;

	sOffset = speed * cl.time * ( 1.0 / psurface->texinfo->texture->width );
	if ( sOffset < 0 )
		sOffset = fmod( sOffset, -1.0 );
	else
		sOffset = fmod( sOffset, 1.0 );

	return sOffset;
}

/*
================
DrawGLPolyScroll
================
*/
void DrawGLPolyScroll( msurface_t *psurface, cl_entity_t *pEntity )
{
	int			i;
	float		*v;
	float		sOffset;
	glpoly_t	*p;

	sOffset = ScrollOffset( psurface, pEntity );
	p = psurface->polys;

	qglBegin( GL_POLYGON );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3] + sOffset, v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}

/*
================
DrawGLSolidPoly
================
*/
void DrawGLSolidPoly( glpoly_t *p )
{
	int		i;
	float	*v;

	qglColor4f( (float)( currententity->curstate.rendercolor.r / 256.0 ), (float)( currententity->curstate.rendercolor.g / 256.0 ), (float)( currententity->curstate.rendercolor.b / 256.0 ), r_blend );

	qglBegin( GL_POLYGON );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		qglVertex3fv( v );
	qglEnd();

	if ( !gl_wireframe.value )
		return;

	if ( gl_wireframe.value == 2 )
		qglDisable( GL_DEPTH_TEST );

	qglColor3f( 1, 1, 1 );
	qglBegin( GL_LINE_LOOP );
	v = p->verts[0];
	for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		qglVertex3fv( v );
	qglEnd();

	if ( gl_wireframe.value == 2 )
		qglEnable( GL_DEPTH_TEST );
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps( void )
{
	int			i, j;
	glpoly_t	*p;
	float		*v;

	if ( filterMode )
		return;

	if ( r_fullbright.value )
		return;

	if ( !gl_texsort )
		return;

	qglDepthMask( 0 );		// don't bother writing Z

	if ( gl_lightmap_format == GL_LUMINANCE )
	{
		qglBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
	}
	else if ( gl_lightmap_format == GL_INTENSITY )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4f( 0, 0, 0, 1 );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else if ( gl_lightmap_format == GL_RGBA && !gl_monolights.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		if ( gl_overbright.value )
		{
			qglBlendFunc( GL_DST_COLOR, GL_SRC_COLOR );
			qglColor4f( 2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 1 );
		}
		else
		{
			qglBlendFunc( GL_ZERO, GL_SRC_COLOR );
		}
	}
	else
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglColor4f( 0, 0, 0, 1 );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	if ( !r_lightmap.value )
		qglEnable( GL_BLEND );

	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		p = lightmap_polys[i];
		if ( !p )
			continue;
		GL_Bind( lightmap_textures + i );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		if ( lightmap_modified[i] )
		{
			lightmap_modified[i] = false;
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, BLOCK_HEIGHT, gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps + i * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT );
		}
		for ( ; p; p = p->chain )
		{
			if ( p->flags & SURF_UNDERWATER )
			{
				DrawGLWaterPolyLightmap( p );
			}
			else if ( p->flags & SURF_DRAWTURB )
			{
				float		tempVert[3];
				glpoly_t	*wp;

				for ( wp = p; wp; wp = wp->next )
				{
					qglBegin( GL_POLYGON );
					v = wp->verts[0];
					qglColor3f( 1, 1, 1 );
					for ( j = 0; j < wp->numverts; j++, v += VERTEXSIZE )
					{
						qglTexCoord2f( v[5], v[6] );
						tempVert[0] = v[0];
						tempVert[1] = v[1];
						tempVert[2] = v[2];
						tempVert[2] += gl_wateramp.value * turbsin[ (int)( cl.time * 160.0 + tempVert[0] + tempVert[1] ) & 255 ];
						tempVert[2] += gl_wateramp.value * 0.8 * turbsin[ (int)( cl.time * 171.0 + tempVert[0] * 5 - tempVert[1] ) & 255 ];
						qglVertex3fv( tempVert );
					}
					qglEnd();
				}
			}
			else
			{
				qglBegin( GL_POLYGON );
				v = p->verts[0];
				for ( j = 0; j < p->numverts; j++, v += VERTEXSIZE )
				{
					qglTexCoord2f( v[5], v[6] );
					qglVertex3fv( v );
				}
				qglEnd();
			}
		}
	}

	qglDisable( GL_BLEND );
	if ( gl_lightmap_format == GL_LUMINANCE )
	{
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}
	else if ( gl_lightmap_format == GL_INTENSITY || gl_monolights.value )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		qglColor4f( 1, 1, 1, 1 );
	}

	qglDepthMask( 1 );		// back to normal Z buffering
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly( msurface_t *fa )
{
	texture_t	*t;
	byte		*base;
	int			maps;

	c_brush_polys++;

	if ( fa->flags & SURF_DRAWSKY )
		return;

	t = R_TextureAnimation( fa );
	GL_Bind( t->gl_texturenum );

	if ( fa->flags & SURF_DRAWTURB )
	{	// warp texture, no lightmaps
		EmitWaterPolys( fa, 0 );
		return;
	}

	if ( fa->flags & SURF_UNDERWATER )
		DrawGLWaterPoly( fa->polys );
	else if ( currententity->curstate.rendermode == kRenderTransColor )
	{
		qglDisable( GL_TEXTURE_2D );
		DrawGLSolidPoly( fa->polys );
		qglEnable( GL_TEXTURE_2D );
	}
	else if ( fa->flags & SURF_DRAWTILED )
		DrawGLPolyScroll( fa, currententity );
	else
		DrawGLPoly( fa->polys );

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[ fa->lightmaptexturenum ];
	lightmap_polys[ fa->lightmaptexturenum ] = fa->polys;

	if ( fa->pdecals )
	{
		gDecalSurfs[ gDecalSurfCount ] = fa;
		gDecalSurfCount++;
		if ( gDecalSurfCount > MAX_DECALSURFS )
			Sys_Error( "Too many decal surfaces!\n" );
	}

	// check for lightmap modification
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ )
		if ( d_lightstylevalue[ fa->styles[maps] ] != fa->cached_light[maps] )
			goto dynamic;

	if ( fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight )			// dynamic previously
	{
dynamic:
		if ( r_dynamic.value )
		{
			lightmap_modified[ fa->lightmaptexturenum ] = true;
			base = lightmaps + fa->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap( fa, base, BLOCK_WIDTH * lightmap_bytes );
		}
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain( msurface_t *s )
{
	if ( mirror )
		return;
	mirror = true;
	mirror_plane = s->plane;
}

/*
================
R_DrawWaterChain
================
*/
void R_DrawWaterChain( msurface_t *pChain )
{
	msurface_t	*s;

	if ( !pChain )
		return;

	qglColor4ub( 255, 255, 255, 255 );

	for ( s = pChain; s; s = s->texturechain )
	{
		GL_Bind( s->texinfo->texture->gl_texturenum );
		EmitWaterPolys( s, 0 );
	}
}

/*
================
DrawTextureChains
================
*/
void DrawTextureChains( void )
{
	int			i;
	msurface_t	*s;
	texture_t	*t;
	int			iSounds;

	iSounds = 100;

	currententity = &cl_entities[0];

	if ( !gl_texsort )
	{
		GL_DisableMultitexture();

		if ( skychain )
		{
			R_DrawSkyChain( skychain );
			skychain = NULL;
		}

		if ( waterchain )
		{
			R_DrawWaterChain( waterchain );
			waterchain = NULL;
		}
	}

	for ( i = 0; i < cl.worldmodel->numtextures; i++ )
	{
		t = cl.worldmodel->textures[i];
		if ( !t )
			continue;
		s = t->texturechain;
		if ( !s )
			continue;
		if ( i == skytexturenum )
			R_DrawSkyChain( s );
		else if ( i == mirrortexturenum && r_mirroralpha.value != 1.0 )
		{
			R_MirrorChain( s );
			continue;
		}
		else
		{
			if ( ( s->flags & SURF_DRAWTURB ) && r_wateralpha.value != 1.0 )
				continue;	// draw translucent water later
			for ( ; s; s = s->texturechain )
				R_RenderBrushPoly( s );
		}

		t->texturechain = NULL;

		if ( !iSounds-- )
		{
			S_ExtraUpdate();
			iSounds = 100;
		}
	}
}

/*
================
R_SetRenderMode
================
*/
void R_SetRenderMode( cl_entity_t *pEntity )
{
	if ( pEntity->curstate.rendermode != kRenderNormal )
	{
		if ( pEntity->curstate.rendermode == kRenderTransColor )
		{
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA );
		}
		else if ( pEntity->curstate.rendermode == kRenderTransAdd )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_ONE, GL_ONE );
			qglColor4f( r_blend, r_blend, r_blend, 1 );
			qglDepthMask( 0 );
		}
		else if ( pEntity->curstate.rendermode == kRenderTransAlpha )
		{
			qglEnable( GL_ALPHA_TEST );
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglColor4f( 1, 1, 1, 1 );
			qglDisable( GL_BLEND );
			qglAlphaFunc( GL_GREATER, gl_alphamin.value );
			return;
		}
		else
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			qglColor4f( 1, 1, 1, r_blend );
			qglDepthMask( 0 );
		}
		qglEnable( GL_BLEND );
	}
	else
	{
		qglColor4f( 1, 1, 1, 1 );
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel( cl_entity_t *e )
{
	int			k;
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if ( e->angles[0] || e->angles[1] || e->angles[2] )
	{
		rotated = true;
		for ( i = 0; i < 3; i++ )
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd( e->origin, clmodel->mins, mins );
		VectorAdd( e->origin, clmodel->maxs, maxs );
	}

	if ( R_CullBox( mins, maxs ) )
		return;

	qglColor3f( 1, 1, 1 );
	Q_memset( lightmap_polys, 0, sizeof( lightmap_polys ) );

	VectorSubtract( r_refdef.vieworg, e->origin, modelorg );
	if ( rotated )
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy( modelorg, temp );
		AngleVectors( e->angles, forward, right, up );
		modelorg[0] = DotProduct( temp, forward );
		modelorg[1] = -DotProduct( temp, right );
		modelorg[2] = DotProduct( temp, up );
	}

	psurf = &clmodel->surfaces[ clmodel->firstmodelsurface ];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if ( clmodel->firstmodelsurface != 0 && !gl_flashblend.value )
	{
		for ( k = 0; k < MAX_DLIGHTS; k++ )
		{
			if ( ( cl_dlights[k].die < cl.time ) || ( !cl_dlights[k].radius ) )
				continue;

			{
				vec3_t	saveOrigin;

				VectorCopy( cl_dlights[k].origin, saveOrigin );
				VectorSubtract( cl_dlights[k].origin, e->origin, cl_dlights[k].origin );
				R_MarkLights( &cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode );
				VectorCopy( saveOrigin, cl_dlights[k].origin );
			}
		}
	}

	qglPushMatrix();
	R_RotateForEntity( e->origin, e );

	R_SetRenderMode( e );

	//
	// draw texture
	//
	for ( i = 0; i < clmodel->nummodelsurfaces; i++, psurf++ )
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		if ( psurf->flags & SURF_DRAWTURB )
		{
			if ( ( pplane->type != PLANE_Z && !gl_watersides.value ) || mins[2] + 1 >= pplane->dist )
				continue;
		}

		dot = DotProduct( modelorg, pplane->normal ) - pplane->dist;

	// draw the polygon
		if ( ( ( psurf->flags & SURF_PLANEBACK ) && ( dot < -BACKFACE_EPSILON ) ) ||
			( !( psurf->flags & SURF_PLANEBACK ) && ( dot > BACKFACE_EPSILON ) ) )
		{
			if ( gl_texsort )
			{
				R_RenderBrushPoly( psurf );
			}
			else
			{
				R_SetRenderMode( e );
				if ( psurf->flags & SURF_DRAWTILED )
					R_DrawSequentialPoly( psurf, 0 );
				else
					R_DrawSequentialPoly( psurf, 0 );
			}
		}
		else if ( psurf->flags & SURF_DRAWTURB )
		{
			R_SetRenderMode( e );
			R_DrawSequentialPoly( psurf, 1 );
		}
	}

	if ( currententity->curstate.rendermode != kRenderNormal )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglDisable( GL_BLEND );
	}

	if ( gl_texsort || currententity->curstate.rendermode == kRenderTransColor )
	{
		if ( currententity->curstate.rendermode != kRenderTransAlpha )
		{
			R_DrawDecals( false );
			if ( currententity->curstate.rendermode == kRenderNormal )
				R_BlendLightmaps();
		}
		else if ( gl_lightholes.value )
		{
			qglDepthFunc( GL_EQUAL );
			R_BlendLightmaps();
			if ( !gl_ztrick.value || gldepthmin < 0.5 )
				qglDepthFunc( GL_LEQUAL );
			else
				qglDepthFunc( GL_GEQUAL );
		}
	}

	qglPopMatrix();
	qglDepthMask( 1 );
	qglDisable( GL_ALPHA_TEST );
	qglAlphaFunc( GL_NOTEQUAL, 0 );
	qglDisable( GL_BLEND );
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode( mnode_t *node )
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		dot;

	if ( node->contents == CONTENTS_SOLID )
		return;		// solid

	if ( node->visframe != r_visframecount )
		return;
	if ( R_CullBox( node->minmaxs, node->minmaxs + 3 ) )
		return;

// if a leaf node, draw stuff
	if ( node->contents < 0 )
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if ( c )
		{
			do
			{
				( *mark )->visframe = r_framecount;
				mark++;
			} while ( --c );
		}

	// deal with model fragments in this leaf
		if ( pleaf->efrags )
			R_StoreEfrags( &pleaf->efrags );

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch ( plane->type )
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct( modelorg, plane->normal ) - plane->dist;
		break;
	}

	if ( dot >= 0 )
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveWorldNode( node->children[side] );

// draw stuff
	c = node->numsurfaces;

	if ( c )
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if ( dot < 0 - BACKFACE_EPSILON )
			side = SURF_PLANEBACK;
		else if ( dot > BACKFACE_EPSILON )
			side = 0;
		{
			for ( ; c; c--, surf++ )
			{
				if ( surf->visframe != r_framecount )
					continue;

				// don't backface underwater surfaces, because they warp
				if ( !( surf->flags & SURF_UNDERWATER ) && ( ( dot < 0 ) ^ !!( surf->flags & SURF_PLANEBACK ) ) )
					continue;		// wrong side

				// if sorting by texture, just store it out
				if ( gl_texsort )
				{
					if ( !mirror
						|| surf->texinfo->texture != cl.worldmodel->textures[ mirrortexturenum ] )
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				}
				else if ( surf->flags & SURF_DRAWSKY )
				{
					surf->texturechain = skychain;
					skychain = surf;
				}
				else if ( surf->flags & SURF_DRAWTURB )
				{
					surf->texturechain = waterchain;
					waterchain = surf;
				}
				else
					R_DrawSequentialPoly( surf, 0 );
			}
		}
	}

// recurse down the back side
	R_RecursiveWorldNode( node->children[ !side ] );
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld( void )
{
	cl_entity_t	ent;

	Q_memset( &ent, 0, sizeof( ent ) );
	ent.model = cl.worldmodel;

	VectorCopy( r_refdef.vieworg, modelorg );

	currententity = &ent;
	currenttexture = -1;

	currententity->curstate.rendercolor.r = gWaterColor.r;
	currententity->curstate.rendercolor.g = gWaterColor.g;
	currententity->curstate.rendercolor.b = gWaterColor.b;

	qglColor3f( 1, 1, 1 );
	Q_memset( lightmap_polys, 0, sizeof( lightmap_polys ) );

	R_ClearSkyBox();

	if ( !gl_texsort )
	{
		if ( gl_lightmap_format == GL_LUMINANCE )
		{
			qglBlendFunc( GL_ZERO, GL_ONE_MINUS_SRC_COLOR );
		}
		else if ( gl_lightmap_format == GL_INTENSITY )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
			qglColor4f( 0, 0, 0, 1 );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		}
		else if ( gl_lightmap_format == GL_RGBA )
		{
			qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		}
	}

	gDecalSurfCount = 0;
	R_RecursiveWorldNode( cl.worldmodel->nodes );
	gDecalSurfCount = 0;

	DrawTextureChains();

	S_ExtraUpdate();

	if ( CL_IsDevOverviewMode() )
		return;

	R_DrawDecals( false );
	R_BlendLightmaps();
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves( void )
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	byte	solid[4096];

	if ( r_oldviewleaf == r_viewleaf && !r_novis.value )
		return;

	if ( mirror )
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if ( r_novis.value )
	{
		vis = solid;
		Q_memset( solid, 0xff, ( cl.worldmodel->numleafs + 7 ) >> 3 );
	}
	else
		vis = Mod_LeafPVS( r_viewleaf, cl.worldmodel );

	for ( i = 0; i < cl.worldmodel->numleafs; i++ )
	{
		if ( vis[ i >> 3 ] & ( 1 << ( i & 7 ) ) )
		{
			node = (mnode_t *)&cl.worldmodel->leafs[ i + 1 ];
			do
			{
				if ( node->visframe == r_visframecount )
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while ( node );
		}
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock( int w, int h, int *x, int *y )
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for ( texnum = 0; texnum < MAX_LIGHTMAPS; texnum++ )
	{
		best = BLOCK_HEIGHT;

		for ( i = 0; i < BLOCK_WIDTH - w; i++ )
		{
			best2 = 0;

			for ( j = 0; j < w; j++ )
			{
				if ( allocated[texnum][i + j] >= best )
					break;
				if ( allocated[texnum][i + j] > best2 )
					best2 = allocated[texnum][i + j];
			}
			if ( j == w )
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if ( best + h > BLOCK_HEIGHT )
			continue;

		for ( i = 0; i < w; i++ )
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error( "AllocBlock: full" );
	return 0;
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int			nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList( msurface_t *fa )
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	int			vertpage;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture
	//
	poly = (glpoly_t *)Hunk_Alloc( sizeof( glpoly_t ) + ( lnumverts - 4 ) * VERTEXSIZE * sizeof( float ) );
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for ( i = 0; i < lnumverts; i++ )
	{
		lindex = currentmodel->surfedges[ fa->firstedge + i ];

		if ( lindex > 0 )
		{
			r_pedge = &pedges[ lindex ];
			vec = r_pcurrentvertbase[ r_pedge->v[0] ].position;
		}
		else
		{
			r_pedge = &pedges[ -lindex ];
			vec = r_pcurrentvertbase[ r_pedge->v[1] ].position;
		}
		s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy( vec, poly->verts[i] );
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct( vec, fa->texinfo->vecs[0] ) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; //fa->texinfo->texture->width;

		t = DotProduct( vec, fa->texinfo->vecs[1] ) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if ( !gl_keeptjunctions.value && !( fa->flags & SURF_UNDERWATER ) )
	{
		for ( i = 0; i < lnumverts; ++i )
		{
			vec3_t	v1, v2;
			float	*prev, *thisPoint, *next;

			prev = poly->verts[ ( i + lnumverts - 1 ) % lnumverts ];
			thisPoint = poly->verts[i];
			next = poly->verts[ ( i + 1 ) % lnumverts ];

			VectorSubtract( thisPoint, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001
			if ( ( fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON ) &&
				( fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON ) &&
				( fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON ) )
			{
				int		j;

				for ( j = i + 1; j < lnumverts; ++j )
				{
					int		k;

					for ( k = 0; k < VERTEXSIZE; ++k )
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap( msurface_t *surf )
{
	int		smax, tmax;
	byte	*base;

	if ( surf->flags & ( SURF_DRAWSKY | SURF_DRAWTURB ) )
		return;

	if ( ( surf->flags & SURF_DRAWTILED ) && ( surf->texinfo->flags & TEX_SPECIAL ) )
		return;

	smax = ( surf->extents[0] >> 4 ) + 1;
	tmax = ( surf->extents[1] >> 4 ) + 1;

	surf->lightmaptexturenum = AllocBlock( smax, tmax, &surf->light_s, &surf->light_t );
	base = lightmaps + surf->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
	base += ( surf->light_t * BLOCK_WIDTH + surf->light_s ) * lightmap_bytes;
	R_BuildLightMap( surf, base, BLOCK_WIDTH * lightmap_bytes );
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps( void )
{
	int		i, j;
	model_t	*m;

	Q_memset( allocated, 0, sizeof( allocated ) );

	r_framecount = 1;		// no dlightcache

	if ( !lightmap_textures )
	{
		lightmap_textures = texture_extension_number_guess;
		texture_extension_number_guess += MAX_LIGHTMAPS;
	}

	gl_lightmap_format = GL_RGBA;

	switch ( gl_lightmap_format )
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		if ( gl_monolights.value )
			lightmap_used = 4;
		else
			lightmap_used = 3;
		break;
	case GL_RGBA4:
		lightmap_bytes = 2;
		lightmap_used = 2;
		break;
	case GL_LUMINANCE:
	case GL_INTENSITY:
	case GL_ALPHA:
		lightmap_bytes = 1;
		lightmap_used = 1;
		break;
	}

	for ( j = 1; j < MAX_MODELS; j++ )
	{
		m = cl.model_precache[j];
		if ( !m )
			break;
		if ( m->name[0] == '*' )
			continue;
		if ( !m->numsurfaces )
			continue;

		m = CL_GetModelByIndex( j );
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for ( i = 0; i < m->numsurfaces; i++ )
		{
			GL_CreateSurfaceLightmap( m->surfaces + i );
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
			BuildSurfaceDisplayList( m->surfaces + i );
		}
	}

	if ( !gl_texsort )
		GL_SelectTexture( TEXTURE1_SGIS );

	//
	// upload all lightmaps that were filled
	//
	for ( i = 0; i < MAX_LIGHTMAPS; i++ )
	{
		if ( !allocated[i][0] )
			break;		// no more used
		lightmap_modified[i] = false;
		GL_Bind( lightmap_textures + i );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexImage2D( GL_TEXTURE_2D, 0, lightmap_used, BLOCK_WIDTH, BLOCK_HEIGHT, 0, gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes );
	}

	if ( !gl_texsort )
		GL_SelectTexture( TEXTURE0_SGIS );
}


//-----------------------------------------------------------------------------
// Decal system
//-----------------------------------------------------------------------------

#define FDECAL_PERMANENT	0x01	// This decal should not be removed in favor of any new decals
#define FDECAL_CUSTOM		0x04	// This is a custom clan logo and should not be saved/restored

#define MAX_DECALS			4096
#define DECAL_DISTANCE		4

static decal_t		gDecalPool[MAX_DECALS];
static int			gDecalCount;
static vec3_t		gDecalPos;

static model_t		*gDecalModel;
static texture_t	*gDecalTexture;
static int			gDecalIndex, gDecalSize;
static int			gDecalFlags, gDecalEntity;
float				gDecalAppliedScale;

#define DECAL_CACHEENTRY	256		// MUST BE POWER OF 2 or code below needs to change!

typedef struct
{
	int		decalIndex;
	float	decalVert[4][VERTEXSIZE];
} decalcache_t;

decalcache_t	gDecalCache[DECAL_CACHEENTRY];

// Init the decal pool
void R_DecalInit( void )
{
	int		i;

	Q_memset( gDecalPool, 0, sizeof( gDecalPool ) );
	gDecalCount = 0;

	for ( i = 0; i < DECAL_CACHEENTRY; i++ )
		gDecalCache[i].decalIndex = -1;
}

int R_DecalIndex( decal_t *pdecal )
{
	return ( pdecal - gDecalPool );
}

int R_DecalCacheIndex( int index )
{
	return index & ( DECAL_CACHEENTRY - 1 );
}

decalcache_t *R_DecalCacheSlot( int decalIndex )
{
	int		cacheIndex;

	cacheIndex = R_DecalCacheIndex( decalIndex );	// Find the cache slot we want

	return gDecalCache + cacheIndex;
}

// Release the cache entry for this decal
void R_DecalCacheClear( decal_t *pdecal )
{
	int				index;
	decalcache_t	*pCache;

	index = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( index );		// Find the cache slot

	if ( pCache->decalIndex == index )		// If this is the decal that's cached here, clear it.
		pCache->decalIndex = -1;
}

// Unlink pdecal from any surface it's attached to
void R_DecalUnlink( decal_t *pdecal )
{
	decal_t		*tmp;

	R_DecalCacheClear( pdecal );
	if ( pdecal->psurface )
	{
		if ( pdecal->psurface->pdecals == pdecal )
		{
			pdecal->psurface->pdecals = pdecal->pnext;
		}
		else
		{
			tmp = pdecal->psurface->pdecals;
			if ( !tmp )
				Sys_Error( "Bad decal list" );
			while ( tmp->pnext )
			{
				if ( tmp->pnext == pdecal )
				{
					tmp->pnext = pdecal->pnext;
					break;
				}
				tmp = tmp->pnext;
			}
		}
	}
	pdecal->psurface = NULL;
}

// Just reuse next decal in list
// A decal that spans multiple surfaces will use multiple decal_t pool entries, as each surface needs
// it's own.
decal_t *R_DecalAlloc( decal_t *pdecal )
{
	int		limit;

	limit = MAX_DECALS;
	if ( r_decals.value < limit && !( gDecalFlags & 0x80 ) )
		limit = r_decals.value;

	if ( !limit )
		return NULL;

	if ( !pdecal )
	{
		int		count;

		count = 0;		// Check for the odd possiblity of infinte loop
		do
		{
			gDecalCount++;
			if ( gDecalCount >= limit )
				gDecalCount = 0;
			pdecal = gDecalPool + gDecalCount;	// reuse next decal
			count++;
		} while ( ( pdecal->flags & ( FDECAL_PERMANENT | 0x80 ) ) && count < limit );
	}

	// If decal is already linked to a surface, unlink it.
	R_DecalUnlink( pdecal );

	return pdecal;
}

// remove all decals with this texture
void R_DecalRemoveAll( int textureIndex )
{
	int			i;
	decal_t		*pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		if ( gDecalPool[i].texture == textureIndex )
		{
			pDecal = &gDecalPool[i];
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

// remove all decals with this texture that are not permanent
void R_DecalRemoveNonPermanent( int textureIndex )
{
	int			i;
	decal_t		*pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		if ( gDecalPool[i].texture == textureIndex )
		{
			pDecal = &gDecalPool[i];
			if ( pDecal && ( pDecal->flags & FDECAL_PERMANENT ) )
				continue;
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

// remove all decals that have one of these flags
void R_DecalRemoveWithFlag( int flags )
{
	int			i;
	decal_t		*pDecal;

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		if ( gDecalPool[i].flags & flags )
		{
			pDecal = &gDecalPool[i];
			R_DecalUnlink( pDecal );
			Q_memset( pDecal, 0, sizeof( decal_t ) );
		}
	}
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
void R_DecalNode( mnode_t *node, float flScale )
{
	mplane_t	*splitplane;
	float		dist;

	if ( !node || node->contents < 0 )
		return;

	splitplane = node->plane;
	dist = DotProduct( gDecalPos, splitplane->normal ) - splitplane->dist;

	// This is arbitrarily set to 10 right now.  In an ideal world we'd have the
	// exact surface but we don't so, this tells me which planes are "sort of
	// close" to the gunshot -- the gunshot is actually 4 units in front of the
	// wall (see dlls\weapons.cpp). We also need to check to see if the decal
	// actually intersects the texture space of the surface, as this method tags
	// parallel surfaces in the same node always.
	// JAY: This still tags faces that aren't correct at edges because we don't
	// have a surface normal

	if ( dist > gDecalSize )
	{
		R_DecalNode( node->children[0], flScale );
	}
	else if ( dist < -gDecalSize )
	{
		R_DecalNode( node->children[1], flScale );
	}
	else
	{
		if ( dist < DECAL_DISTANCE && dist > -DECAL_DISTANCE )
		{
			int			w, h;
			float		s, t, scale;
			msurface_t	*surf;
			int			i;
			mtexinfo_t	*tex;
			vec3_t		cross, normal;
			float		face;

			surf = gDecalModel->surfaces + node->firstsurface;

			// iterate over all surfaces in the node
			for ( i = 0; i < node->numsurfaces; i++, surf++ )
			{
				if ( surf->flags & ( SURF_DRAWTURB | SURF_DRAWTILED ) )
					continue;

				if ( ( surf->flags & 0x100 ) && ( gDecalFlags & FDECAL_CUSTOM ) )
					continue;

				tex = surf->texinfo;

				if ( flScale == -1 )
					scale = Length( tex->vecs[0] );
				else
					scale = flScale;

				if ( scale == 0 )
					continue;

				s = DotProduct( gDecalPos, tex->vecs[0] ) + tex->vecs[0][3] - surf->texturemins[0];
				t = DotProduct( gDecalPos, tex->vecs[1] ) + tex->vecs[1][3] - surf->texturemins[1];

				w = gDecalTexture->width * scale;
				h = gDecalTexture->height * scale;

				// move s,t to upper left corner
				s -= ( w * 0.5 );
				t -= ( h * 0.5 );

				if ( s <= -w || t <= -h || s > ( surf->extents[0] + w ) || t > ( surf->extents[1] + h ) )
					continue;	// nope

				scale = 1.0 / scale;
				s = ( s + surf->texturemins[0] ) / tex->texture->width;
				t = ( t + surf->texturemins[1] ) / tex->texture->height;

				if ( gDecalFlags & ( FDECAL_CUSTOM | 0x80 ) )
				{
					// Re-use the decal flags in surface for which side of the plane
					if ( surf->flags & SURF_PLANEBACK )
						VectorScale( surf->plane->normal, -1, normal );
					else
						VectorCopy( surf->plane->normal, normal );

					CrossProduct( surf->texinfo->vecs[0], normal, cross );
					face = DotProduct( cross, surf->texinfo->vecs[1] );
					if ( face < 0 )
						gDecalFlags |= 0x08;	// mirror horizontally
					else
						gDecalFlags &= ~0x08;

					CrossProduct( surf->texinfo->vecs[1], normal, cross );
					face = DotProduct( cross, surf->texinfo->vecs[0] );
					if ( gDecalFlags & 0x08 )
						face = -face;

					if ( face > 0 )
						gDecalFlags |= 0x10;	// mirror vertically
					else
						gDecalFlags &= ~0x10;
				}

				if ( ( gDecalFlags & 0x80 ) && gDecalAppliedScale == -1 )
					gDecalAppliedScale = Length( tex->vecs[0] );

				R_DecalCreate( surf, gDecalIndex, scale, s, t );
			}
		}

		R_DecalNode( node->children[0], flScale );
		R_DecalNode( node->children[1], flScale );
	}
}

// Add the decal to the list if it isn't already in it
int DecalListAdd( DECALLIST *pList, int count )
{
	int			i;
	vec3_t		tmp;
	DECALLIST	*pdecal;

	pdecal = pList + count;
	for ( i = 0; i < count; i++ )
	{
		if ( pdecal->name == pList[i].name && pdecal->entityIndex == pList[i].entityIndex )
		{
			VectorSubtract( pdecal->position, pList[i].position, tmp );	// Merge
			if ( Length( tmp ) < 2 )	// UNDONE: Tune this '2' constant
				return count;
		}
	}

	// This is a new decal
	return count + 1;
}

int DecalDepthCompare( const DECALLIST *elem1, const DECALLIST *elem2 )
{
	if ( elem1->depth > elem2->depth )
		return -1;
	if ( elem1->depth < elem2->depth )
		return 1;

	return 0;
}

// Called by CSaveRestore::SaveClientState
int DecalListCreate( DECALLIST *pList )
{
	int		total;
	int		i, depth;

	total = 0;

	if ( cl.worldmodel )
	{
		for ( i = 0; i < MAX_DECALS; i++ )
		{
			decal_t		*pdecals;
			texture_t	*ptexture;

			// Decal is in use and is not a custom decal
			if ( gDecalPool[i].psurface != NULL && !( gDecalPool[i].flags & FDECAL_CUSTOM ) && !( gDecalPool[i].flags & 0x80 ) )
			{
				// compute depth
				depth = 0;
				pdecals = gDecalPool[i].psurface->pdecals;
				while ( pdecals && pdecals != &gDecalPool[i] )
				{
					depth++;
					pdecals = pdecals->pnext;
				}
				pList[total].depth = depth;
				pList[total].flags = gDecalPool[i].flags;

				pList[total].entityIndex = R_DecalUnProject( &gDecalPool[i], pList[total].position );

				ptexture = Draw_DecalTexture( gDecalPool[i].texture );
				pList[total].name = ptexture->name;

				// Check to see if the decal should be added
				total = DecalListAdd( pList, total );
			}
		}
	}

	// Sort the decals lowest depth first
	qsort( pList, total, sizeof( DECALLIST ), ( int (*)( const void *, const void * ) )DecalDepthCompare );

	return total;
}

// ---------------------------------------------------------

int R_DecalUnProject( decal_t *pdecal, vec_t *position )
{
	float		s, t, scale, inverseScale;
	mtexinfo_t	*pTexinfo;
	texture_t	*ptexture;
	int			entityIndex;

	if ( !pdecal || !pdecal->psurface )
		return -1;

	s = pdecal->dx;
	t = pdecal->dy;
	pTexinfo = pdecal->psurface->texinfo;

	s = s * pTexinfo->texture->width - pdecal->psurface->texturemins[0];
	t = t * pTexinfo->texture->height - pdecal->psurface->texturemins[1];

	scale = Length( pTexinfo->vecs[0] ) * 0.5;

	ptexture = Draw_DecalTexture( pdecal->texture );

	s = ptexture->width * scale + s + pdecal->psurface->texturemins[0] - pTexinfo->vecs[0][3];
	t = ptexture->height * scale + t + pdecal->psurface->texturemins[1] - pTexinfo->vecs[1][3];

	scale = fabs( Length( pTexinfo->vecs[0] ) );
	if ( scale != 0 )
	{
		inverseScale = 1.0 / scale;
		inverseScale *= inverseScale;
	}

	VectorScale( pTexinfo->vecs[0], inverseScale * s, position );
	VectorMA( position, inverseScale * t, pTexinfo->vecs[1], position );
	VectorMA( position, pdecal->psurface->plane->dist, pdecal->psurface->plane->normal, position );

	entityIndex = pdecal->entityIndex;
	if ( entityIndex )
	{
		hull_t		*phull;
		vec3_t		temp;
		edict_t		*pEdict;
		model_t		*pModel;

		pEdict = &sv.edicts[ entityIndex ];
		if ( pEdict->v.modelindex )
			pModel = sv.models[ pEdict->v.modelindex ];
		else
			pModel = NULL;

		if ( !pModel || pModel->type != mod_brush )
			return 0;

		phull = &pModel->hulls[0];
		// Make sure we use the local coordinate space
		if ( pEdict->v.angles[0] || pEdict->v.angles[1] || pEdict->v.angles[2] )
		{
			vec3_t	forward, right, up;

			AngleVectorsTranspose( pEdict->v.angles, forward, right, up );
			VectorCopy( position, temp );
			position[0] = DotProduct( temp, forward );
			position[1] = DotProduct( temp, right );
			position[2] = DotProduct( temp, up );
		}

		if ( pModel->firstmodelsurface )
		{
			VectorAdd( position, phull->clip_mins, position );
			VectorAdd( position, pEdict->v.origin, position );
		}
	}

	return entityIndex;
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
// This is called from cl_parse.c, cl_tent.c
void R_DecalShoot_( texture_t *ptexture, int index, int entity, int modelIndex, vec_t *position, int flags, float flScale )
{
	mnode_t		*pnodes;
	cl_entity_t	*pent;

	if ( g_bUsingInGameAdvertisements && ptexture && !Q_stricmp( ptexture->name, "}lambda06" ) )
		flags |= FDECAL_CUSTOM;

	pent = &cl_entities[ entity ];
	VectorCopy( position, gDecalPos );	// Pass position in global

	if ( pent )
	{
		gDecalModel = pent->model;
		if ( !gDecalModel && modelIndex )
			gDecalModel = CL_GetModelByIndex( modelIndex );
		if ( !gDecalModel && sv.active )
			gDecalModel = sv.models[ sv.edicts[ entity ].v.modelindex ];
	}
	else
		gDecalModel = NULL;

	if ( !pent || !gDecalModel || gDecalModel->type != mod_brush || !ptexture )
	{
		Con_DPrintf( "Decals must hit mod_brush!\n" );
		return;
	}

	pnodes = gDecalModel->nodes;
	if ( entity )
	{
		hull_t	*phull;
		vec3_t	temp;

		phull = &gDecalModel->hulls[0];
		if ( gDecalModel->firstmodelsurface )
		{
			VectorSubtract( position, phull->clip_mins, gDecalPos );
			VectorSubtract( gDecalPos, pent->origin, gDecalPos );
			pnodes = pnodes + phull->firstclipnode;
		}

		// make sure the decal is in entity space
		if ( pent->angles[0] || pent->angles[1] || pent->angles[2] )
		{
			vec3_t	forward, right, up;

			AngleVectors( pent->angles, forward, right, up );
			VectorCopy( gDecalPos, temp );
			gDecalPos[0] = DotProduct( temp, forward );
			gDecalPos[1] = -DotProduct( temp, right );
			gDecalPos[2] = DotProduct( temp, up );
		}
	}

	// Pass texture in global
	gDecalTexture = ptexture;
	gDecalIndex = index;

	// set the flags
	if ( !( flags & FDECAL_CUSTOM ) )
		flags |= 0x20;
	gDecalFlags = flags;
	gDecalEntity = entity;
	gDecalSize = ptexture->width >> 1;
	gDecalAppliedScale = -1;
	if ( (int)( ptexture->height >> 1 ) > gDecalSize )
		gDecalSize = ptexture->height >> 1;

	if ( strstr( ptexture->name, "}z_" ) )
		R_DecalNode( pnodes, RandomFloat( 0.1, 0.15 ) );
	else if ( flScale != 1 )
		R_DecalNode( pnodes, flScale );
	else
		R_DecalNode( pnodes, -1 );
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
void R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t *position, int flags )
{
	texture_t	*ptexture;

	ptexture = Draw_DecalTexture( textureIndex );
	R_DecalShoot_( ptexture, textureIndex, entity, modelIndex, position, flags, 1.0 );
}

void R_DecalShootScaled( int textureIndex, int entity, int modelIndex, vec_t *position, int flags, float flScale )
{
	texture_t	*ptexture;

	ptexture = Draw_DecalTexture( textureIndex );
	R_DecalShoot_( ptexture, textureIndex, entity, modelIndex, position, flags, flScale );
}

void R_FireCustomDecal( int textureIndex, int entity, int modelIndex, vec_t *position, int flags, float scale )
{
	texture_t	*ptexture;
	mnode_t		*pnodes;
	cl_entity_t	*pent;

	ptexture = Draw_DecalTexture( textureIndex );

	pent = &cl_entities[ entity ];
	VectorCopy( position, gDecalPos );	// Pass position in global

	if ( pent )
	{
		gDecalModel = pent->model;
		if ( !gDecalModel && modelIndex )
			gDecalModel = CL_GetModelByIndex( modelIndex );
		if ( !gDecalModel && sv.active )
			gDecalModel = sv.models[ sv.edicts[ entity ].v.modelindex ];
	}
	else
		gDecalModel = NULL;

	if ( !pent || !gDecalModel || gDecalModel->type != mod_brush || !ptexture )
	{
		Con_DPrintf( "Decals must hit mod_brush!\n" );
		return;
	}

	pnodes = gDecalModel->nodes;
	if ( entity )
	{
		hull_t	*phull;
		vec3_t	temp;

		phull = &gDecalModel->hulls[0];
		if ( gDecalModel->firstmodelsurface )
		{
			VectorSubtract( position, phull->clip_mins, gDecalPos );
			VectorSubtract( gDecalPos, pent->origin, gDecalPos );
			pnodes = pnodes + phull->firstclipnode;
		}

		// make sure the decal is in entity space
		if ( pent->angles[0] || pent->angles[1] || pent->angles[2] )
		{
			vec3_t	forward, right, up;

			AngleVectors( pent->angles, forward, right, up );
			VectorCopy( gDecalPos, temp );
			gDecalPos[0] = DotProduct( temp, forward );
			gDecalPos[1] = -DotProduct( temp, right );
			gDecalPos[2] = DotProduct( temp, up );
		}
	}

	// Pass texture in global
	gDecalTexture = ptexture;
	gDecalIndex = textureIndex;

	// set the flags
	if ( !( flags & FDECAL_CUSTOM ) )
		flags |= 0x20;
	gDecalFlags = flags;
	gDecalEntity = entity;
	gDecalSize = ptexture->width >> 1;
	if ( (int)( ptexture->height >> 1 ) > gDecalSize )
		gDecalSize = ptexture->height >> 1;

	R_DecalNode( pnodes, scale );
}

// Shoots a decal onto the surface of the BSP.  position is the center of the decal in world coords
void R_CustomDecalShoot( texture_t *ptexture, int playernum, int entity, int modelIndex, vec_t *position, int flags )
{
	int		plindex;

	plindex = playernum + 1;
	R_DecalShoot_( ptexture, -plindex, entity, modelIndex, position, flags, 1.0 );
}

// Check for intersecting decals on this surface
decal_t *R_DecalIntersect( msurface_t *psurf, int *pcount, float x, float y )
{
	decal_t		*plist, *plast;
	int			dist, lastDist, dx, dy;
	texture_t	*ptexture;
	float		w, h, maxWidth;
	qboolean	bPermanent;

	plast = NULL;
	*pcount = 0;
	lastDist = 0xFFFF;

	w = psurf->texinfo->texture->width;
	h = psurf->texinfo->texture->height;
	plist = psurf->pdecals;

	// Compute the position of the decal center in texture space
	x = x * w + ( gDecalTexture->width >> 1 );
	y = y * h + ( gDecalTexture->height >> 1 );

	// Determine the maximum width of decals we need to check
	maxWidth = gDecalTexture->width * 1.5;

	while ( plist )
	{
		ptexture = Draw_DecalTexture( plist->texture );
		bPermanent = ( plist->flags & ( FDECAL_PERMANENT | 0x80 ) ) ? true : false;

		// Don't steal permanent decals
		if ( !bPermanent && ptexture->width <= maxWidth )
		{
			dx = x - ( plist->dx * w + ( ptexture->width >> 1 ) );
			if ( dx < 0 )
				dx = -dx;
			dy = y - ( plist->dy * h + ( ptexture->height >> 1 ) );
			if ( dy < 0 )
				dy = -dy;

			if ( dy > dx )
				dist = dy + dx * 0.5;
			else
				dist = dx + dy * 0.5;

			if ( dist * plist->scale < 8 )
			{
				( *pcount )++;
				if ( !plast || dist <= lastDist )
				{
					plast = plist;
					lastDist = dist;
				}
			}
		}
		plist = plist->pnext;
	}

	return plast;
}

// Insert the custom decal ahead of the normal ones on the surface
void R_InsertCustomDecal( decal_t *pdecal, msurface_t *surface )
{
	decal_t		*p, *prev;

	p = surface->pdecals;
	prev = NULL;

	while ( p )
	{
		if ( p->flags & 0x80 )
		{
			pdecal->pnext = p;
			if ( !prev )
			{
				surface->pdecals = pdecal;
				return;
			}
			break;
		}
		prev = p;
		p = p->pnext;
	}

	prev->pnext = pdecal;
}

// Allocate and initialize a decal from the pool, on surface with offsets x, y
void R_DecalCreate( msurface_t *psurface, int textureIndex, float scale, float x, float y )
{
	decal_t		*pdecal, *pold;
	int			count;

	pold = R_DecalIntersect( psurface, &count, x, y );
	if ( count < 4 )	// Allow up to 4 decals at a spot
		pold = NULL;

	pdecal = R_DecalAlloc( pold );

	pdecal->flags = gDecalFlags;
	pdecal->dx = x;
	pdecal->dy = y;
	pdecal->texture = textureIndex;
	pdecal->pnext = NULL;

	// Is this a second-pass decal?
	pold = psurface->pdecals;
	if ( !pold )
	{
		psurface->pdecals = pdecal;
	}
	else
	{
		if ( !( pdecal->flags & FDECAL_CUSTOM ) )
		{
			while ( pold->pnext )
				pold = pold->pnext;
			pold->pnext = pdecal;
		}
		else
			R_InsertCustomDecal( pdecal, psurface );
	}

	pdecal->psurface = psurface;
	pdecal->scale = scale;
	pdecal->entityIndex = gDecalEntity;

	R_InvalidateSurface( psurface );
}

//=============================================================================
//
// Decal polygon clipping
//
//=============================================================================

int Inside( float *vert, int edge )
{
	switch ( edge )
	{
	case 0:		// left
		if ( vert[3] > 0.0 )
			return 1;
		return 0;
	case 1:		// right
		if ( vert[3] < 1.0 )
			return 1;
		return 0;
	case 2:		// top
		if ( vert[4] > 0.0 )
			return 1;
		return 0;
	case 3:		// bottom
		if ( vert[4] < 1.0 )
			return 1;
		return 0;
	}
	return 0;
}

void Intersect( float *one, float *two, int edge, float *out )
{
	float	t;

	// t is the parameter of the intersection along the edge
	if ( edge < 2 )
	{
		if ( edge == 0 )	// left
		{
			t = one[3] / ( one[3] - two[3] );
			out[3] = 0;
		}
		else				// right
		{
			t = ( one[3] - 1.0 ) / ( one[3] - two[3] );
			out[3] = 1;
		}
		out[4] = one[4] + ( two[4] - one[4] ) * t;
	}
	else
	{
		if ( edge == 2 )	// top
		{
			t = one[4] / ( one[4] - two[4] );
			out[4] = 0;
		}
		else				// bottom
		{
			t = ( one[4] - 1.0 ) / ( one[4] - two[4] );
			out[4] = 1;
		}
		out[3] = one[3] + ( two[3] - one[3] ) * t;
	}
	out[0] = one[0] + ( two[0] - one[0] ) * t;
	out[1] = one[1] + ( two[1] - one[1] ) * t;
	out[2] = one[2] + ( two[2] - one[2] ) * t;
}

int SHClip( float *vert, int vertCount, float *out, int edge )
{
	int		j, outCount;
	float	*s, *p;

	outCount = 0;

	s = &vert[ ( vertCount - 1 ) * VERTEXSIZE ];
	for ( j = 0; j < vertCount; j++ )
	{
		p = &vert[ j * VERTEXSIZE ];
		if ( Inside( p, edge ) )
		{
			if ( Inside( s, edge ) )
			{
				// Add a vertex and advance out to next vertex
				Q_memcpy( out, p, sizeof( float ) * VERTEXSIZE );
				outCount++;
				out += VERTEXSIZE;
			}
			else
			{
				Intersect( s, p, edge, out );
				out += VERTEXSIZE;
				outCount++;
				Q_memcpy( out, p, sizeof( float ) * VERTEXSIZE );
				outCount++;
				out += VERTEXSIZE;
			}
		}
		else
		{
			if ( Inside( s, edge ) )
			{
				Intersect( p, s, edge, out );
				out += VERTEXSIZE;
				outCount++;
			}
		}
		s = p;
	}

	return outCount;
}

#define MAX_DECALCLIPVERT	32

static float	vert[MAX_DECALCLIPVERT][VERTEXSIZE];
static float	outvert[MAX_DECALCLIPVERT][VERTEXSIZE];

#pragma check_stack( on )

// Generate lighting coordinates at each vertex for decal vertices v[] on surface psurf
void R_DecalVertsLight( float *v, msurface_t *psurf, int vertCount )
{
	int		j;
	float	s, t;

	for ( j = 0; j < vertCount; j++, v += VERTEXSIZE )
	{
		// Lightmap texture coordinates (just like BuildSurfaceDisplayList)
		s = DotProduct( v, psurf->texinfo->vecs[0] ) + psurf->texinfo->vecs[0][3];
		s -= psurf->texturemins[0];
		s += psurf->light_s * 16;
		s += 8;
		s *= 1.0 / ( BLOCK_WIDTH * 16 );

		t = DotProduct( v, psurf->texinfo->vecs[1] ) + psurf->texinfo->vecs[1][3];
		t -= psurf->texturemins[1];
		t += psurf->light_t * 16;
		t += 8;
		t *= 1.0 / ( BLOCK_HEIGHT * 16 );

		v[5] = s;
		v[6] = t;
	}
}

// Check for an unclipped decal and generate the polygon vertices for it in poutVerts
float *R_DecalVertsClip( float *poutVerts, decal_t *pdecal, msurface_t *psurf, texture_t *ptexture, int *pvertCount )
{
	float	*v;
	float	scalex, scaley;
	int		j, outCount;

	scalex = ( psurf->texinfo->texture->width * pdecal->scale ) / ptexture->width;
	scaley = ( psurf->texinfo->texture->height * pdecal->scale ) / ptexture->height;

	if ( poutVerts == NULL )
		poutVerts = &vert[0][0];

	v = psurf->polys->verts[0];
	for ( j = 0; j < psurf->polys->numverts; j++, v += VERTEXSIZE )
	{
		VectorCopy( v, vert[j] );
		vert[j][3] = ( v[3] - pdecal->dx ) * scalex;
		vert[j][4] = ( v[4] - pdecal->dy ) * scaley;

		if ( pdecal->flags & 0x08 )		// mirrored horizontally
			vert[j][3] = 1.0 - vert[j][3];
		if ( pdecal->flags & 0x10 )		// mirrored vertically
			vert[j][4] = 1.0 - vert[j][4];
	}

	// Clip the polygon to the decal texture space
	outCount = SHClip( vert[0], psurf->polys->numverts, outvert[0], 0 );
	outCount = SHClip( outvert[0], outCount, vert[0], 1 );
	outCount = SHClip( vert[0], outCount, outvert[0], 2 );
	outCount = SHClip( outvert[0], outCount, poutVerts, 3 );

	if ( outCount )
	{
		if ( pdecal->flags & 0x20 )
		{
			pdecal->flags &= ~0x20;	// We're doing the test

			// If there are exactly 4 verts and they are all 0,1 tex coords, then we've got an unclipped decal
			// A more precise test would be to calculate the texture area and make sure it's one, but this
			// should work as well.
			if ( outCount == 4 )
			{
				int		clipped;

				clipped = 0;
				v = poutVerts;
				for ( j = 0; j < outCount && !clipped; j++, v += VERTEXSIZE )
				{
					float	s, t;

					s = v[3];
					t = v[4];

					if ( ( s != 0.0 && s != 1.0 ) || ( t != 0.0 && t != 1.0 ) )
						clipped = 1;
				}

				// We didn't need to clip this decal, it's a quad covering
				// the full texture space, optimize subsequent frames.
				if ( !clipped )
					pdecal->flags |= 0x40;
			}
		}
	}

	*pvertCount = outCount;
	return poutVerts;
}

// Generate clipped vertex list for decal pdecal projected onto polygon psurf
float *R_DecalVertsNoclip( decal_t *pdecal, msurface_t *psurf, texture_t *ptexture, qboolean bMultitexture )
{
	float			*vlist;
	decalcache_t	*pCache;
	int				decalIndex;
	int				outCount;

	decalIndex = R_DecalIndex( pdecal );
	pCache = R_DecalCacheSlot( decalIndex );

	// Is the decal cached?
	if ( pCache->decalIndex == decalIndex )
		return &pCache->decalVert[0][0];

	pCache->decalIndex = decalIndex;

	vlist = &pCache->decalVert[0][0];

	// Use the old code for now, and just cache them
	vlist = R_DecalVertsClip( vlist, pdecal, psurf, ptexture, &outCount );

	if ( bMultitexture )
		R_DecalVertsLight( vlist, psurf, 4 );

	return vlist;
}

// Renders a single decal
void R_DecalPoly( float *v, texture_t *ptexture, msurface_t *psurf, int vertCount )
{
	int		j;

	GL_Bind( ptexture->gl_texturenum );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	qglBegin( GL_POLYGON );
	for ( j = 0; j < vertCount; j++, v += VERTEXSIZE )
	{
		qglTexCoord2f( v[3], v[4] );
		qglVertex3fv( v );
	}
	qglEnd();
}

// Renders a single decal with the lightmap
void R_DecalMPoly( float *v, texture_t *ptexture, msurface_t *psurf, int vertCount )
{
	int		j;

	GL_SelectTexture( TEXTURE0_SGIS );
	GL_Bind( ptexture->gl_texturenum );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

	GL_EnableMultitexture();
	GL_Bind( lightmap_textures + psurf->lightmaptexturenum );

	qglBegin( GL_POLYGON );
	for ( j = 0; j < vertCount; j++, v += VERTEXSIZE )
	{
		qglMTexCoord2fSGIS( TEXTURE0_SGIS, v[3], v[4] );
		qglMTexCoord2fSGIS( TEXTURE1_SGIS, v[5], v[6] );
		qglVertex3fv( v );
	}
	qglEnd();
}

// Renders all decals
void R_DrawDecals( qboolean bMultitexture )
{
	decal_t		*plist;
	int			i, outCount;
	texture_t	*ptexture;
	msurface_t	*psurf;
	float		*v;

	if ( !gDecalSurfCount )
		return;

	qglEnable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglDepthMask( 0 );

	if ( gl_polyoffset.value )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		if ( !gl_ztrick.value || gldepthmin < 0.5 )
			qglPolygonOffset( -1, -gl_polyoffset.value );
		else
			qglPolygonOffset( 1, gl_polyoffset.value );
	}

	for ( i = 0; i < gDecalSurfCount; i++ )
	{
		psurf = gDecalSurfs[i];
		plist = psurf->pdecals;

		while ( plist )
		{
			ptexture = Draw_DecalTexture( plist->texture );

			if ( !( plist->flags & 0x40 ) )
			{
				v = R_DecalVertsClip( NULL, plist, psurf, ptexture, &outCount );
				if ( outCount && bMultitexture )
					R_DecalVertsLight( v, psurf, outCount );
			}

			if ( plist->flags & 0x40 )
			{
				v = R_DecalVertsNoclip( plist, psurf, ptexture, bMultitexture );
				outCount = 4;
			}

			if ( outCount )
			{
				if ( bMultitexture )
					R_DecalMPoly( v, ptexture, psurf, outCount );
				else
					R_DecalPoly( v, ptexture, psurf, outCount );
			}

			plist = plist->pnext;
		}
	}

	if ( gl_polyoffset.value )
		qglDisable( GL_POLYGON_OFFSET_FILL );

	qglDisable( GL_ALPHA_TEST );
	qglDisable( GL_BLEND );
	qglDepthMask( 1 );

	gDecalSurfCount = 0;
}

/*
================
R_InvalidateSurface
================
*/
void R_InvalidateSurface( msurface_t *surface )
{
}
