// r_triangle.c -- triangle api for the client dll, and the client fog

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "GLQUAKE.H"
#include "qgl.h"
#include "triangleapi.h"

BOOL	g_bUserFogOn;
BOOL	g_bFogSkybox = TRUE;
float	flFinalFogColor[4];
float	flFogDensity;
float	flFogStart;
float	flFogEnd;

static int	gRenderMode;

float	gGlR, gGlG, gGlB, gGlW;

int		g_GL_Modes[7] = { GL_TRIANGLES, GL_TRIANGLE_FAN, GL_QUADS, GL_POLYGON, GL_LINES, GL_TRIANGLE_STRIP, GL_QUAD_STRIP };

// gl_rmisc.c
extern	cvar_t		gl_fog;

// gl_vidnt.c
extern	int			gD3DMode;

// gl_draw.c
void	VGUI2_ResetCurrentTexture( void );

// cl_tent.c
mspriteframe_t	*R_GetSpriteFrame( msprite_t *pSprite, int frame );

// r_part.c
int		ScreenTransform( vec_t *point, vec_t *screen );
void	WorldTransform( vec_t *screen, vec_t *point );

// pr_cmds.c
mnode_t	*PVSNode( mnode_t *node, vec_t *emins, vec_t *emaxs );

// gl_rlight.c
colorVec	R_LightPoint( vec3_t p );

/*
=================
R_RenderFinalFog
=================
*/
void R_RenderFinalFog( void )
{
	if ( gD3DMode == 1 )
		return;

	qglEnable( GL_FOG );
	qglFogi( GL_FOG_MODE, GL_EXP2 );
	qglFogf( GL_FOG_DENSITY, flFogDensity );
	qglHint( GL_FOG_HINT, GL_NICEST );
	qglFogfv( GL_FOG_COLOR, flFinalFogColor );
	qglFogf( GL_FOG_START, flFogStart );
	qglFogf( GL_FOG_END, flFogEnd );
}

/*
=================
R_FogParams
=================
*/
void R_FogParams( float flDensity, int iFogSkybox )
{
	flFogDensity = flDensity;
	g_bFogSkybox = iFogSkybox;
}

/*
=================
R_RenderFog
=================
*/
void R_RenderFog( float *flFogColor, float flStart, float flEnd, BOOL bOn )
{
	g_bUserFogOn = bOn && gl_fog.value > 0;

	if ( !g_bUserFogOn )
		return;

	flFinalFogColor[0] = flFogColor[0] / 255;
	flFinalFogColor[1] = flFogColor[1] / 255;
	flFinalFogColor[2] = flFogColor[2] / 255;
	flFinalFogColor[3] = 1.0;

	flFogStart = flStart;
	flFogEnd = flEnd;
}

/*
=================
tri_GL_Begin
=================
*/
void tri_GL_Begin( int primitiveCode )
{
	VGUI2_ResetCurrentTexture();
	qglBegin( g_GL_Modes[primitiveCode] );
}

/*
=================
tri_GL_End
=================
*/
void tri_GL_End( void )
{
	qglEnd();
}

/*
=================
tri_GL_TexCoord2f
=================
*/
void tri_GL_TexCoord2f( float u, float v )
{
	qglTexCoord2f( u, v );
}

/*
=================
tri_GL_Vertex3fv
=================
*/
void tri_GL_Vertex3fv( float *worldPnt )
{
	qglVertex3fv( worldPnt );
}

/*
=================
tri_GL_Vertex3f
=================
*/
void tri_GL_Vertex3f( float x, float y, float z )
{
	qglVertex3f( x, y, z );
}

/*
=================
tri_GL_Color4f
=================
*/
void tri_GL_Color4f( float x, float y, float z, float w )
{
	if ( gRenderMode == kRenderTransAlpha )
	{
		float	alpha;

		alpha = w * 255;
		qglColor4ub( x * 255.9f, y * 255.9f, z * 255.9f, alpha );
	}
	else
	{
		qglColor4f( x * w, y * w, z * w, 1.0 );
	}

	gGlR = x;
	gGlG = y;
	gGlB = z;
	gGlW = w;
}

/*
=================
tri_GL_Color4fRendermode
=================
*/
void tri_GL_Color4fRendermode( float x, float y, float z, float w, int rendermode )
{
	if ( rendermode == kRenderTransAlpha )
	{
		gGlW = w * ( 1.0 / 255.0 );
		qglColor4f( x, y, z, w );
	}
	else
	{
		qglColor4f( x * w, y * w, z * w, 1.0 );
	}
}

/*
=================
tri_GL_Color4ub
=================
*/
void tri_GL_Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	gGlR = r * ( 1.0 / 255.0 );
	gGlG = g * ( 1.0 / 255.0 );
	gGlB = b * ( 1.0 / 255.0 );
	gGlW = a * ( 1.0 / 255.0 );

	qglColor4f( gGlR, gGlG, gGlB, 1.0 );
}

/*
=================
tri_GL_Brightness
=================
*/
void tri_GL_Brightness( float x )
{
	qglColor4f( gGlR * x * gGlW, gGlG * x * gGlW, gGlB * x * gGlW, 1.0 );
}

/*
=================
tri_GL_RenderMode
=================
*/
void tri_GL_RenderMode( int mode )
{
	switch ( mode )
	{
	case kRenderNormal:
		qglDisable( GL_BLEND );
		qglDepthMask( GL_TRUE );
		qglShadeModel( GL_FLAT );
		break;

	case kRenderTransColor:
	case kRenderTransTexture:
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglEnable( GL_BLEND );
		qglShadeModel( GL_SMOOTH );
		break;

	case kRenderTransAdd:
		qglBlendFunc( GL_ONE, GL_ONE );
		qglEnable( GL_BLEND );
		qglDepthMask( GL_FALSE );
		qglShadeModel( GL_SMOOTH );
		break;

	case kRenderTransAlpha:
		qglEnable( GL_BLEND );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglShadeModel( GL_SMOOTH );
		qglDepthMask( GL_FALSE );
		break;
	}

	gRenderMode = mode;
}

/*
=================
tri_GL_CullFace
=================
*/
void tri_GL_CullFace( TRICULLSTYLE style )
{
	switch ( style )
	{
	case TRI_FRONT:
		qglEnable( GL_CULL_FACE );
		qglCullFace( GL_FRONT );
		break;

	case TRI_NONE:
		qglDisable( GL_CULL_FACE );
		break;
	}
}

/*
=================
R_TriangleSpriteTexture
=================
*/
int R_TriangleSpriteTexture( model_t *pSpriteModel, int frame )
{
	mspriteframe_t	*pSpriteFrame;

	VGUI2_ResetCurrentTexture();

	pSpriteFrame = R_GetSpriteFrame( (msprite_t *)pSpriteModel->cache.data, frame );
	if ( !pSpriteFrame )
		return 0;

	GL_Bind( pSpriteFrame->gl_texturenum );

	return 1;
}

/*
=================
tri_GetMatrix
=================
*/
void tri_GetMatrix( const int pname, float *matrix )
{
	qglGetFloatv( pname, matrix );
}

/*
=================
tri_ScreenTransform
=================
*/
int tri_ScreenTransform( float *world, float *screen )
{
	return ScreenTransform( world, screen );
}

/*
=================
tri_WorldTransform
=================
*/
void tri_WorldTransform( float *screen, float *world )
{
	WorldTransform( screen, world );
}

/*
=================
tri_BoxinPVS
=================
*/
int tri_BoxinPVS( float *mins, float *maxs )
{
	if ( PVSNode( cl.worldmodel->nodes, mins, maxs ) )
		return 1;

	return 0;
}

/*
=================
tri_LightAtPoint
=================
*/
void tri_LightAtPoint( float *vPos, float *value )
{
	colorVec	vLightColor;

	vLightColor = R_LightPoint( vPos );

	value[0] = vLightColor.r;
	value[1] = vLightColor.g;
	value[2] = vLightColor.b;
}

triangleapi_t tri =
{
	TRI_API_VERSION,
	tri_GL_RenderMode,
	tri_GL_Begin,
	tri_GL_End,
	tri_GL_Color4f,
	tri_GL_Color4ub,
	tri_GL_TexCoord2f,
	tri_GL_Vertex3fv,
	tri_GL_Vertex3f,
	tri_GL_Brightness,
	tri_GL_CullFace,
	R_TriangleSpriteTexture,
	tri_ScreenTransform,
	R_RenderFog,
	tri_WorldTransform,
	tri_GetMatrix,
	tri_BoxinPVS,
	tri_LightAtPoint,
	tri_GL_Color4fRendermode,
	R_FogParams
};
