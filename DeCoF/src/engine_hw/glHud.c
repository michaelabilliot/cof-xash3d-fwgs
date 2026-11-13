// glHud.c -- 2D projection setup for the HUD

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "screen.h"

void	VGUI2_ResetCurrentTexture( void );

/*
=================
GLBeginHud
=================
*/
void GLBeginHud( void )
{
	qglViewport( glx, gly, glwidth, glheight );

	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, glwidth, glheight, 0, -99999, 99999 );

	qglMatrixMode( GL_MODELVIEW );
	qglPushMatrix();
	qglLoadIdentity();

	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );
	qglEnable( GL_ALPHA_TEST );
	qglColor4f( 1, 1, 1, 1 );

	VGUI2_ResetCurrentTexture();
}

/*
=================
DrawWedge
=================
*/
void DrawWedge( float centerx, float centery, float angle1, float angle2, float radius )
{
	angle1 *= M_PI / 180;
	angle2 *= M_PI / 180;

	qglBegin( GL_TRIANGLES );
	qglVertex2f( centerx, centery );
	qglVertex2f( (float)( centerx - cos( angle1 ) * radius ), (float)( centery - sin( angle1 ) * radius ) );
	qglVertex2f( (float)( centerx - cos( angle2 ) * radius ), (float)( centery - sin( angle2 ) * radius ) );
	qglEnd();
}

/*
=================
GLFinishHud
=================
*/
void GLFinishHud( void )
{
	qglMatrixMode( GL_PROJECTION );
	qglPopMatrix();
	qglMatrixMode( GL_MODELVIEW );
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_CULL_FACE );
	qglEnable( GL_BLEND );
}
