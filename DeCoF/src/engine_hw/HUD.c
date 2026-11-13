// HUD.c -- view size commands, geiger counter and crosshair

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "screen.h"
#include "sound.h"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"

extern	cvar_t	crosshair;
extern	int		scr_copyeverything;
extern	vrect_t	scr_vrect;

extern	sfx_t	*cl_sfx_geiger1;
extern	sfx_t	*cl_sfx_geiger2;
extern	sfx_t	*cl_sfx_geiger3;
extern	sfx_t	*cl_sfx_geiger4;
extern	sfx_t	*cl_sfx_geiger5;
extern	sfx_t	*cl_sfx_geiger6;

int		ScreenTransform( vec_t *point, vec_t *screen );
void	DrawCrosshair( int x, int y );

int		gHealth = 100;
double	gfFade;
int		gAmmo;
double	gfAmmoFade;

int		giHudLevel = 1;
int		giSelectMode = 1;
int		giSelAmmo1 = -1;
int		giSelAmmo2 = -1;
int		giClipAmmo;
int		giTotalAmmo;
int		giSecondAmmo;
int		giAmmoDigits;
int		giHealthWidth;
int		giAmmoWidth;
int		giGeigerRange = -1;
double	gflGeigerDelay = -1.0;

int		sb_lines;

/*
=================
HudSizeUp
=================
*/
void HudSizeUp( void )
{
	if ( scr_viewsize.value < 120 )
		Cvar_SetValue( "viewsize", scr_viewsize.value + 10 );
}

/*
=================
HudSizeDown
=================
*/
void HudSizeDown( void )
{
	if ( scr_viewsize.value > 30 )
		Cvar_SetValue( "viewsize", scr_viewsize.value - 10 );
}

/*
=================
Sbar_Geiger
=================
*/
void Sbar_Geiger( void )
{
	int		pct;
	float	flvol;
	sfx_t	*rgsfx[3];
	int		i;

	if ( cl.time < gflGeigerDelay )
		return;

	gflGeigerDelay = cl.time + 0.1;

	if ( giGeigerRange < 1000 && giGeigerRange > 0 )
	{
		// peicewise linear is better than continuous formula for this
		if ( giGeigerRange > 800 )
		{
			pct = 0;
		}
		else if ( giGeigerRange > 600 )
		{
			pct = 2;
			flvol = 0.4;
			rgsfx[0] = cl_sfx_geiger1;
			rgsfx[1] = cl_sfx_geiger1;
			i = 2;
		}
		else if ( giGeigerRange > 500 )
		{
			pct = 4;
			flvol = 0.5;
			rgsfx[0] = cl_sfx_geiger1;
			rgsfx[1] = cl_sfx_geiger2;
			i = 2;
		}
		else if ( giGeigerRange > 400 )
		{
			pct = 8;
			flvol = 0.6;
			rgsfx[0] = cl_sfx_geiger1;
			rgsfx[1] = cl_sfx_geiger2;
			rgsfx[2] = cl_sfx_geiger3;
			i = 3;
		}
		else if ( giGeigerRange > 300 )
		{
			pct = 8;
			flvol = 0.7;
			rgsfx[0] = cl_sfx_geiger2;
			rgsfx[1] = cl_sfx_geiger3;
			rgsfx[2] = cl_sfx_geiger4;
			i = 3;
		}
		else if ( giGeigerRange > 200 )
		{
			pct = 28;
			flvol = 0.78;
			rgsfx[0] = cl_sfx_geiger2;
			rgsfx[1] = cl_sfx_geiger3;
			rgsfx[2] = cl_sfx_geiger4;
			i = 3;
		}
		else if ( giGeigerRange > 150 )
		{
			pct = 40;
			flvol = 0.80;
			rgsfx[0] = cl_sfx_geiger3;
			rgsfx[1] = cl_sfx_geiger4;
			rgsfx[2] = cl_sfx_geiger5;
			i = 3;
		}
		else if ( giGeigerRange > 100 )
		{
			pct = 60;
			flvol = 0.85;
			rgsfx[0] = cl_sfx_geiger3;
			rgsfx[1] = cl_sfx_geiger4;
			rgsfx[2] = cl_sfx_geiger5;
			i = 3;
		}
		else if ( giGeigerRange > 75 )
		{
			pct = 80;
			flvol = 0.9;
			rgsfx[0] = cl_sfx_geiger4;
			rgsfx[1] = cl_sfx_geiger5;
			rgsfx[2] = cl_sfx_geiger6;
			i = 3;
		}
		else if ( giGeigerRange > 50 )
		{
			pct = 90;
			flvol = 0.95;
			rgsfx[0] = cl_sfx_geiger5;
			rgsfx[1] = cl_sfx_geiger6;
			i = 2;
		}
		else
		{
			pct = 95;
			flvol = 1.0;
			rgsfx[0] = cl_sfx_geiger5;
			rgsfx[1] = cl_sfx_geiger6;
			i = 2;
		}

		flvol = ( flvol * RandomFloat( 0, 0.5 ) ) + 0.25;

		if ( RandomLong( 0, 127 ) < pct || RandomLong( 0, 127 ) < pct )
		{
			S_StartDynamicSound( -1, 0, rgsfx[RandomLong( 0, i - 1 )], r_origin, flvol, 1.0, 0, 100 );
		}
	}
}

/*
=================
Sbar_Draw
=================
*/
void Sbar_Draw( void )
{
	if ( !giHudLevel )
		return;

	if ( scr_con_current == vid.height )
		return;		// console is full screen

	scr_copyeverything = 1;

	if ( crosshair.value )
	{
		float	x, y;
		vec3_t	angles;
		vec3_t	forward;
		vec3_t	point, screen;

		x = scr_vrect.x + scr_vrect.width / 2;
		y = scr_vrect.y + scr_vrect.height / 2;

		VectorAdd( r_refdef.viewangles, cl.crosshairangle, angles );
		AngleVectors( angles, forward, NULL, NULL );
		VectorAdd( r_origin, forward, point );
		ScreenTransform( point, screen );

		x += 0.5 * screen[0] * scr_vrect.width + 0.5;
		y += 0.5 * screen[1] * scr_vrect.height + 0.5;

		DrawCrosshair( x, y );
	}
}
