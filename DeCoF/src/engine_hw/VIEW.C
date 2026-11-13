// view.c -- player eye positioning

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "host.h"
#include "client.h"
#include "GL_MODEL.H"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "chase.h"
#include "shake.h"
#include "console.h"
#include "cdll_int.h"
#include "ref_params.h"
#include "../pm_shared/pm_movevars.h"
#include "cl_demo.h"
#include "APIProxy.h"
#include "view.h"
#include "screen.h"
#include "vgui_int.h"
#include "delta.h"
#include "server.h"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

float	v_lambert1 = 1.4953241;

cvar_t	lcd_x = { "lcd_x", "0" };
cvar_t	lcd_yaw = { "lcd_yaw", "0" };

cvar_t	v_dark = { "v_dark", "0" };
cvar_t	v_lambert = { "lambert", "1.5" };
cvar_t	crosshair = { "crosshair", "0", FCVAR_ARCHIVE };

cvar_t	v_gamma = { "gamma", "2.5", FCVAR_ARCHIVE };
cvar_t	v_brightness = { "brightness", "0.0", FCVAR_ARCHIVE };
cvar_t	v_lightgamma = { "lightgamma", "2.5" };
cvar_t	v_texgamma = { "texgamma", "2.0" };
cvar_t	v_direct = { "direct", "0.9" };

byte	texgammatable[256];	// palette is sent through this to convert to screen gamma
int		lightgammatable[1024];
int		lineargammatable[1024];
int		screengammatable[1024];

byte	ramps[3][256];
float	v_blend[4];			// rgba 0.0 - 1.0

vec3_t	r_soundOrigin;
vec3_t	r_playerViewportAngles;

void BuildGammaTable( float g )
{
	int		i, inf;
	float	g1, g2, g3;
	float	lightvalue_guess;
	float	texvalue_guess;

	if ( !Host_IsSinglePlayerGame() )
	{
		if ( v_brightness.value > 2.0 )
			v_brightness.value = 2.0;

		if ( v_lightgamma.value < 1.8 )
			v_lightgamma.value = 1.8;

		if ( g < 1.8 )
		{
			g = 1.8;
			Cvar_SetValue( "gamma", 1.8 );
		}
	}

	if ( g > 3.0 )
	{
		g = 3.0;
		Cvar_SetValue( "gamma", 3.0 );
	}

	g = 1.0 / g;

	texvalue_guess = min( v_texgamma.value, 3.0 );
	texvalue_guess = max( v_texgamma.value, 1.8 );
	v_texgamma.value = texvalue_guess;
	g1 = v_texgamma.value * g;

	lightvalue_guess = min( v_lightgamma.value, 3.0 );
	lightvalue_guess = max( v_lightgamma.value, 1.8 );
	v_lightgamma.value = lightvalue_guess;
	g2 = v_lightgamma.value * g;

	if ( v_brightness.value <= 0.0 )
		g3 = 0.125;
	else if ( v_brightness.value > 1.0 )
		g3 = 0.05;
	else
		g3 = 0.125 - ( v_brightness.value * v_brightness.value ) * 0.075;

	for ( i = 0; i < 256; i++ )
	{
		inf = pow( i / 255.0, g1 ) * 255;
		if ( inf < 0 )
			inf = 0;
		if ( inf > 255 )
			inf = 255;
		texgammatable[i] = inf;
	}

	for ( i = 0; i < 1024; i++ )
	{
		float	f;

		f = pow( i / 1023.0, v_lightgamma.value );

		// do magic
		if ( v_brightness.value > 1.0 )
			f = f * v_brightness.value;

		if ( f <= g3 )
			f = ( f / g3 ) * 0.125;
		else
			f = 0.125 + ( ( f - g3 ) / ( 1.0 - g3 ) ) * 0.875;

		inf = pow( f, g ) * 1023;
		if ( inf < 0 )
			inf = 0;
		if ( inf > 1023 )
			inf = 1023;
		lightgammatable[i] = inf;
	}

	for ( i = 0; i < 1024; i++ )
	{
		lineargammatable[i] = pow( i / 1023.0, v_gamma.value ) * 1023;
		screengammatable[i] = pow( i / 1023.0, 1.0 / v_gamma.value ) * 1023;
	}
}

qboolean V_CheckGamma( void )
{
	static float oldgammavalue;
	static float oldlightgamma;
	static float oldtexgamma;
	static float oldbrightness;

	if ( v_gamma.value == oldgammavalue && v_lightgamma.value == oldlightgamma && v_texgamma.value == oldtexgamma && v_brightness.value == oldbrightness )
		return false;

	BuildGammaTable( v_gamma.value );
	oldgammavalue = v_gamma.value;
	oldlightgamma = v_lightgamma.value;
	oldtexgamma = v_texgamma.value;
	oldbrightness = v_brightness.value;

	D_FlushCaches();
	vid.recalc_refdef = 1;	// force a surface cache flush

	return true;
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend( void )
{
	float	r, g, b, a;

	r = 0;
	g = 0;
	b = 0;
	a = 0;

	v_blend[0] = r / 255.0;
	v_blend[1] = g / 255.0;
	v_blend[2] = b / 255.0;
	v_blend[3] = a;
	if ( v_blend[3] > 1 )
		v_blend[3] = 1;
	if ( v_blend[3] < 0 )
		v_blend[3] = 0;
}

/*
=============
V_UpdatePalette
=============
*/
void V_UpdatePalette( void )
{
	int			i;
	qboolean	newFlag;
	float		r, g, b, a;
	int			ir, ig, ib;
	qboolean	force;

	newFlag = false;

	force = V_CheckGamma();
	if ( !newFlag && !force )
		return;

	V_CalcBlend();

	a = v_blend[3];
	r = 255 * v_blend[0] * a;
	g = 255 * v_blend[1] * a;
	b = 255 * v_blend[2] * a;

	a = 1 - a;
	for ( i = 0; i < 256; i++ )
	{
		ir = i * a + r;
		ig = i * a + g;
		ib = i * a + b;
		if ( ir > 255 )
			ir = 255;
		if ( ig > 255 )
			ig = 255;
		if ( ib > 255 )
			ib = 255;

		ramps[0][i] = texgammatable[ir];
		ramps[1][i] = texgammatable[ig];
		ramps[2][i] = texgammatable[ib];
	}
}

/*
==============================================================================

						VIEW RENDERING

==============================================================================
*/

static vec3_t forward, right, up;

void V_SetRefParams( ref_params_t *pparams )
{
	Q_memset( pparams, 0, sizeof( ref_params_t ) );

	// the view origin and angles
	VectorCopy( r_refdef.vieworg, pparams->vieworg );
	VectorCopy( r_refdef.viewangles, pparams->viewangles );

	VectorCopy( forward, pparams->forward );
	VectorCopy( right, pparams->right );
	VectorCopy( up, pparams->up );

	pparams->time = cl.time;
	pparams->frametime = host_frametime;
	pparams->intermission = ( cl.intermission != 0 );
	pparams->paused = ( cl.paused != 0 );
	pparams->spectator = ( cls.spectator != 0 );
	pparams->onground = ( cl.onground != -1 );
	pparams->waterlevel = cl.waterlevel;

	VectorCopy( cl.simvel, pparams->simvel );
	VectorCopy( cl.simorg, pparams->simorg );
	VectorCopy( cl.viewheight, pparams->viewheight );
	pparams->idealpitch = cl.idealpitch;

	VectorCopy( cl.viewangles, pparams->cl_viewangles );
	pparams->health = cl.stats[STAT_HEALTH];
	VectorCopy( cl.crosshairangle, pparams->crosshairangle );
	pparams->viewsize = scr_viewsize.value;

	VectorCopy( cl.punchangle, pparams->punchangle );
	pparams->maxclients = cl.maxclients;
	pparams->viewentity = cl.viewentity;
	pparams->playernum = cl.playernum;
	pparams->max_entities = cl.max_edicts;
	pparams->demoplayback = cls.demoplayback;

	pparams->cmd = &cl.cmd;
	pparams->movevars = &movevars;
	pparams->hardware = 1;
	pparams->smoothing = cl.pushmsec;

	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = vid.width;
	pparams->viewport[3] = vid.height;

	pparams->nextView = 0;
	pparams->onlyClientDraw = 0;
}

void V_GetRefParams( ref_params_t *pparams )
{
	// the view origin and angles
	VectorCopy( pparams->vieworg, r_refdef.vieworg );
	VectorCopy( pparams->viewangles, r_refdef.viewangles );

	VectorCopy( pparams->forward, forward );
	VectorCopy( pparams->right, right );
	VectorCopy( pparams->up, up );

	VectorCopy( pparams->simvel, cl.simvel );
	VectorCopy( pparams->simorg, cl.simorg );

	VectorCopy( pparams->cl_viewangles, cl.viewangles );
	VectorCopy( pparams->crosshairangle, cl.crosshairangle );
	VectorCopy( pparams->punchangle, cl.punchangle );

	VectorCopy( pparams->viewheight, cl.viewheight );

	r_refdef.vrect.x = pparams->viewport[0];
	r_refdef.vrect.y = pparams->viewport[1];
	r_refdef.vrect.width = pparams->viewport[2];
	r_refdef.vrect.height = pparams->viewport[3];

	r_refdef.onlyClientDraws = pparams->onlyClientDraw;
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView( void )
{
	ref_params_t	rp;
	cl_entity_t		*view;
	int				viewnum;

	viewnum = 0;

	r_soundOrigin[0] = r_soundOrigin[1] = r_soundOrigin[2] = 0;
	r_playerViewportAngles[0] = r_playerViewportAngles[1] = r_playerViewportAngles[2] = 0;

	if ( con_forcedup )
		return;

	// don't allow cheats in multiplayer
	if ( cls.state != ca_active )
		return;

	if ( cls.signon != SIGNONS )
		return;

	view = &cl.viewent;

	view->model = CL_GetModelByIndex( cl.stats[STAT_WEAPON] );
	view->curstate.modelindex = cl.stats[STAT_WEAPON];
	view->curstate.frame = 0;
	view->curstate.colormap = 0;
	view->index = cl.playernum + 1;

	V_SetRefParams( &rp );

	do
	{
		ClientDLL_CalcRefdef( &rp );

		if ( !viewnum )
		{
			if ( !cls.demoplayback )
				CL_SetDemoViewInfo( &rp, view->origin, cl.stats[STAT_WEAPON] );

			if ( cls.demoplayback && !cls.spectator )
			{
				vec3_t	angles;

				CL_GetDemoViewInfo( &rp, view->origin, &cl.stats[STAT_WEAPON] );

				VectorCopy( rp.viewangles, angles );
				angles[0] *= -1.0;

				VectorCopy( angles, view->angles );
				VectorCopy( angles, view->curstate.angles );
				VectorCopy( angles, view->latched.prevangles );

				rp.nextView = 0;
			}
		}

		V_GetRefParams( &rp );

		if ( rp.intermission )
		{
			view->model = NULL;
		}
		else if ( !rp.paused && chase_active.value )
		{
			Chase_Update();
		}

		R_PushDlights();

		if ( !viewnum && r_refdef.onlyClientDraws )
		{
			qglClearColor( 0, 0, 0, 0 );
			qglClear( GL_COLOR_BUFFER_BIT );
		}

		if ( lcd_x.value )
		{
			//
			// render two interleaved views
			//
			int		i;

			vid.rowbytes <<= 1;
			vid.aspect *= 0.5;

			r_refdef.viewangles[YAW] -= lcd_yaw.value;
			for ( i = 0; i < 3; i++ )
				r_refdef.vieworg[i] -= rp.right[i] * lcd_x.value;
			R_RenderView();

			vid.buffer += vid.rowbytes >> 1;

			R_PushDlights();

			r_refdef.viewangles[YAW] += lcd_yaw.value * 2;
			for ( i = 0; i < 3; i++ )
				r_refdef.vieworg[i] += 2 * rp.right[i] * lcd_x.value;
			R_RenderView();

			vid.buffer -= vid.rowbytes >> 1;

			r_refdef.vrect.height <<= 1;

			vid.rowbytes >>= 1;
			vid.aspect *= 2;
		}
		else
		{
			R_RenderView();
		}

		if ( !rp.onlyClientDraw )
		{
			VectorCopy( r_origin, r_soundOrigin );
			VectorCopy( rp.viewangles, r_playerViewportAngles );
		}

		viewnum++;
	} while ( rp.nextView );

	AdsRenderView_guess( cl.worldmodel, &rp, rp.viewangles );

	R_PolyBlend();
}

struct
{
	float	time;
	float	duration;
	float	amplitude;
	float	frequency;
	float	nextShake;
	vec3_t	offset;
	float	angle;
	vec3_t	appliedOffset;
	float	appliedAngle;
} gVShake;

void V_CalcShake( void )
{
	float	frametime;
	int		i;
	float	fraction, freq;

	g_engdstAddrs.V_CalcShake();

	if ( cl.time > gVShake.time || gVShake.duration <= 0 || gVShake.amplitude <= 0 || gVShake.frequency <= 0 )
	{
		if ( gVShake.time != 0 )
		{
			gVShake.time = 0;
			gVShake.appliedOffset[0] = 0;
			gVShake.appliedOffset[1] = 0;
			gVShake.appliedOffset[2] = 0;
			gVShake.appliedAngle = 0;
		}
		return;
	}

	frametime = cl.time - cl.oldtime;

	if ( cl.time > gVShake.nextShake )
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		gVShake.nextShake = cl.time + gVShake.duration / gVShake.frequency;

		// Compute random shake extents (the shake will settle down from this)
		for ( i = 0; i < 3; i++ )
			gVShake.offset[i] = RandomFloat( -gVShake.amplitude, gVShake.amplitude );

		gVShake.angle = RandomFloat( (float)( -gVShake.amplitude * 0.25 ), (float)( gVShake.amplitude * 0.25 ) );
	}

	// Ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( cl.time - gVShake.time ) / gVShake.duration;

	// Ramp up frequency over duration
	if ( fraction )
		freq = ( gVShake.frequency / fraction ) * gVShake.frequency;
	else
		freq = 0;

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin( cl.time * freq );

	// Add to view origin
	for ( i = 0; i < 3; i++ )
		gVShake.appliedOffset[i] = fraction * gVShake.offset[i];

	// Add to roll
	gVShake.appliedAngle = gVShake.angle * fraction;

	// Drop amplitude a bit, less for higher frequency shakes
	gVShake.amplitude -= gVShake.amplitude * ( frametime / ( gVShake.duration * gVShake.frequency ) );
}

void V_ApplyShake( float *origin, float *angles, float factor )
{
	g_engdstAddrs.V_ApplyShake( &origin, &angles, &factor );

	if ( origin )
		VectorMA( origin, factor, gVShake.appliedOffset, origin );

	if ( angles )
		angles[ROLL] += gVShake.appliedAngle * factor;
}

int V_ScreenShake( const char *pszName, int iSize, void *pbuf )
{
	ScreenShake	*pShake;
	float		amplitude;

	pShake = (ScreenShake *)pbuf;

	gVShake.duration = pShake->duration * ( 1.0 / ( 1 << 12 ) );
	gVShake.time = gVShake.duration + cl.time;

	amplitude = pShake->amplitude * ( 1.0 / ( 1 << 12 ) );
	if ( amplitude > gVShake.amplitude )
		gVShake.amplitude = amplitude;

	gVShake.frequency = pShake->frequency * ( 1.0 / ( 1 << 8 ) );
	gVShake.nextShake = 0;

	return 1;
}

int V_ScreenFade( const char *pszName, int iSize, void *pbuf )
{
	float		flScale;
	ScreenFade	*pFade;

	flScale = 1 << 12;
	pFade = (ScreenFade *)pbuf;

	if ( !pFade )
		return 1;

	if ( pFade->fadeFlags & FFADE_LONGFADE )
		flScale = 1 << 8;

	// Save the fade parameters
	cl.sf.fadeEnd = pFade->duration * ( 1.0 / flScale );
	cl.sf.fadeReset = pFade->holdTime * ( 1.0 / flScale );
	cl.sf.fader = pFade->r;
	cl.sf.fadeg = pFade->g;
	cl.sf.fadeb = pFade->b;
	cl.sf.fadealpha = pFade->a;
	cl.sf.fadeFlags = pFade->fadeFlags;
	cl.sf.fadeSpeed = 0;

	// Calc fade speed
	if ( pFade->duration > 0 )
	{
		if ( pFade->fadeFlags & FFADE_OUT )
		{
			if ( cl.sf.fadeEnd )
				cl.sf.fadeSpeed = -(float)cl.sf.fadealpha / cl.sf.fadeEnd;

			cl.sf.fadeEnd += cl.time;
			cl.sf.fadeTotalEnd = cl.sf.fadeEnd;
			cl.sf.fadeReset += cl.sf.fadeEnd;
		}
		else
		{
			if ( cl.sf.fadeEnd )
				cl.sf.fadeSpeed = (float)cl.sf.fadealpha / cl.sf.fadeEnd;

			cl.sf.fadeReset += cl.time;
			cl.sf.fadeEnd += cl.sf.fadeReset;
		}
	}

	return 1;
}

int V_FadeAlpha( void )
{
	int		alpha = 0;

	if ( cl.time > cl.sf.fadeReset && cl.time > cl.sf.fadeEnd && !( cl.sf.fadeFlags & FFADE_STAYOUT ) )
		return 0;

	if ( cl.sf.fadeFlags & FFADE_STAYOUT )
	{
		// Only stay out if the fade isn't fading in
		if ( ( cl.sf.fadeFlags & FFADE_OUT ) && cl.sf.fadeTotalEnd > cl.time )
		{
			alpha = cl.sf.fadealpha + cl.sf.fadeSpeed * ( cl.sf.fadeTotalEnd - cl.time );
		}
		else
		{
			cl.sf.fadeEnd = cl.time + 0.1;
			alpha = cl.sf.fadealpha;
		}
	}
	else
	{
		if ( cl.sf.fadeFlags & FFADE_OUT )
			alpha = cl.sf.fadealpha + cl.sf.fadeSpeed * ( cl.sf.fadeEnd - cl.time );
		else
			alpha = cl.sf.fadeSpeed * ( cl.sf.fadeEnd - cl.time );
	}

	if ( alpha > cl.sf.fadealpha )
		alpha = cl.sf.fadealpha;
	else if ( alpha < 0 )
		alpha = 0;

	return alpha;
}

/*
=============
V_Init
=============
*/
void V_Init( void )
{
	Cvar_RegisterVariable( &v_dark );
	Cvar_RegisterVariable( &crosshair );
	Cvar_RegisterVariable( &v_gamma );
	Cvar_RegisterVariable( &v_lightgamma );
	Cvar_RegisterVariable( &v_texgamma );
	Cvar_RegisterVariable( &v_brightness );
	Cvar_RegisterVariable( &v_lambert );
	Cvar_RegisterVariable( &v_direct );

	BuildGammaTable( 2.5 );	// no gamma yet
}

void V_InitLevel( void )
{
	Q_memset( &gVShake, 0, sizeof( gVShake ) );

	if ( v_dark.value )
	{
		cl.sf.fadeEnd = 5.0;
		cl.sf.fadeReset = 5.0;
		cl.sf.fader = cl.sf.fadeg = cl.sf.fadeb = 0;
		cl.sf.fadealpha = 255;
		cl.sf.fadeFlags = 0;

		if ( cl.sf.fadeEnd )
			cl.sf.fadeSpeed = (float)cl.sf.fadealpha / cl.sf.fadeEnd;

		cl.sf.fadeReset += cl.time;
		cl.sf.fadeEnd += cl.sf.fadeReset;

		v_dark.value = 0;
	}
	else
	{
		cl.sf.fadeReset = cl.sf.fadeEnd = cl.sf.fadeSpeed = 0;
		cl.sf.fader = cl.sf.fadeg = cl.sf.fadeb = cl.sf.fadealpha = 0;
		cl.sf.fadeFlags = 0;
	}
}
