// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "host.h"
#include "sys.h"
#include "client.h"
#include "GL_MODEL.H"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "wad.h"
#include "draw.h"
#include "keys.h"
#include "console.h"
#include "sound.h"
#include "net.h"
#include "filesystem_.h"
#include "cdll_int.h"
#include "APIProxy.h"
#include "vgui_int.h"
#include "view.h"
#include "screen.h"

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full


*/


int			glx, gly, glwidth, glheight;

// only the refresh window will be updated unless these variables are flagged
int			scr_copytop;
int			scr_copyeverything;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

float		oldscreensize, oldfov;
cvar_t		scr_viewsize = { "viewsize", "120", FCVAR_ARCHIVE };
float		scr_fov_value = 90;
cvar_t		scr_conspeed = { "scr_conspeed", "600" };
cvar_t		scr_centertime = { "scr_centertime", "2" };
cvar_t		scr_showpause = { "showpause", "1" };
cvar_t		scr_printspeed = { "scr_printspeed", "8" };
cvar_t		scr_graphheight = { "graphheight", "64.0" };
cvar_t		scr_downloading = { "scr_downloading", "-1.0" };
cvar_t		scr_connectmsg = { "scr_connectmsg", "0" };
cvar_t		scr_connectmsg1 = { "scr_connectmsg1", "0" };
cvar_t		scr_connectmsg2 = { "scr_connectmsg2", "0" };

qboolean	scr_initialized;	// ready to draw

qpic_t		*scr_paused;

int			scr_fullupdate;

int			clearconsole;
int			clearnotify;

int			sb_lines;

viddef_t	vid;				// global video state

vrect_t		scr_vrect;

qboolean	scr_drawloading;

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

char		scr_centerstring[1024];
float		scr_centertime_start;	// for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint( char *str )
{
	Q_strncpy( scr_centerstring, str, sizeof( scr_centerstring ) - 1 );
	scr_centerstring[ sizeof( scr_centerstring ) - 1 ] = 0;
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while ( *str )
	{
		if ( *str == '\n' )
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString( void )
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;

	// the finale prints the characters one at a time
	if ( cl.intermission )
	{
		remaining = scr_printspeed.value * ( cl.time - scr_centertime_start );
		if ( VGuiWrap2_IsInCareerMatch() )
			return;
	}
	else
	{
		remaining = 9999;
	}

	Draw_GetDefaultColor();

	scr_erase_center = 0;
	start = scr_centerstring;

	if ( scr_center_lines <= 4 )
		y = glheight * 0.35;
	else
		y = 48;

	do
	{
		// scan the width of the line
		for ( l = 0; l < 40; l++ )
		{
			if ( start[l] == '\n' || !start[l] )
				break;
		}

		{
			char	line[80];

			Q_strncpy( line, start, l );
			line[l] = 0;
			x = ( glwidth - Draw_StringLen( line, VGUI2_GetConsoleFont() ) ) / 2;
		}

		for ( j = 0; j < l; j++ )
		{
			x += Draw_Character( x, y, start[j], VGUI2_GetConsoleFont() );
			if ( !remaining-- )
				return;
		}

		y += 12;

		while ( *start && *start != '\n' )
			start++;

		if ( !*start )
			break;
		start++;		// skip the \n
	} while ( 1 );
}

void SCR_CheckDrawCenterString( void )
{
	scr_copytop = 1;
	if ( scr_center_lines > scr_erase_lines )
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= host_frametime;

	if ( scr_centertime_off <= 0 && !cl.intermission )
		return;
	if ( key_dest != key_game )
		return;

	SCR_DrawCenterString();
}

//=============================================================================

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
void SCR_CalcRefdef( void )
{
	float	size;
	int		h;

	scr_fullupdate = 0;		// force a background redraw
	vid.recalc_refdef = 0;

	scr_viewsize.value = 120;

	// bound viewsize
	if ( scr_viewsize.value < 30 )
		Cvar_Set( "viewsize", "30" );
	if ( scr_viewsize.value > 120 )
		Cvar_Set( "viewsize", "120" );

	// bound field of view
	if ( scr_fov_value < 10 )
		scr_fov_value = 10;
	if ( scr_fov_value > 150 )
		scr_fov_value = 150;

	// intermission is always full screen
	if ( cl.intermission )
		size = 120;
	else
		size = scr_viewsize.value;

	if ( size >= 120 )
		sb_lines = 0;		// no status bar at all
	else if ( size >= 110 )
		sb_lines = 24;		// no inventory
	else
		sb_lines = 24 + 16 + 8;

	size = ( scr_viewsize.value > 100 ) ? 100 : scr_viewsize.value;
	if ( cl.intermission )
	{
		size = 100;
		sb_lines = 0;
	}
	size /= 100;

	h = glheight - sb_lines;

	r_refdef.vrect.width = glwidth * size;
	if ( r_refdef.vrect.width < 96 )
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;	// min for icons
	}

	r_refdef.vrect.height = glheight * size;
	if ( r_refdef.vrect.height > glheight - sb_lines )
		r_refdef.vrect.height = glheight - sb_lines;

	r_refdef.vrect.x = ( glwidth - r_refdef.vrect.width ) / 2;
	r_refdef.vrect.y = ( h - r_refdef.vrect.height ) / 2;

	scr_vrect = r_refdef.vrect;
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f( void )
{
	HudSizeUp();
	vid.recalc_refdef = 1;
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f( void )
{
	HudSizeDown();
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_Init
==================
*/
void SCR_Init( void )
{
	Cvar_RegisterVariable( &scr_viewsize );
	Cvar_RegisterVariable( &scr_showpause );
	Cvar_RegisterVariable( &scr_conspeed );
	Cvar_RegisterVariable( &scr_centertime );
	Cvar_RegisterVariable( &scr_printspeed );
	Cvar_RegisterVariable( &scr_graphheight );
	Cvar_RegisterVariable( &scr_connectmsg );
	Cvar_RegisterVariable( &scr_connectmsg1 );
	Cvar_RegisterVariable( &scr_connectmsg2 );

	//
	// register our commands
	//
	Cmd_AddCommand( "screenshot", SCR_ScreenShot_f );
	Cmd_AddCommand( "sizeup", SCR_SizeUp_f );
	Cmd_AddCommand( "sizedown", SCR_SizeDown_f );

	scr_paused = Draw_PicFromWad( "paused" );

	scr_initialized = true;
}

void Draw_CenterPic( qpic_t *pPic )
{
	int		x, y;

	x = glwidth / 2;
	y = glheight / 2;

	x -= pPic->width / 2;
	y -= pPic->height / 2;

	Draw_Pic( x, y, pPic );
}

/*
==============
SCR_DrawPause
==============
*/
void SCR_DrawPause( void )
{
	if ( !scr_showpause.value )		// turn off for screenshots
		return;

	if ( !cl.paused )
		return;

	Draw_CenterPic( scr_paused );
}

/*
==============
SCR_DrawLoading
==============
*/
void SCR_DrawLoading( void )
{
	if ( !scr_drawloading )
		return;

	Draw_BeginDisc();
}

//=============================================================================

/*
==================
SCR_SetUpToDrawConsole
==================
*/
void SCR_SetUpToDrawConsole( void )
{
	Con_CheckResize();
}

/*
==================
SCR_DrawConsole
==================
*/
void SCR_DrawConsole( void )
{
	Con_DrawNotify();
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_ScreenShot_f
==================
*/
void SCR_ScreenShot_f( void )
{
	byte	*buffer;
	char	pcxname[80];
	int		i, c, temp;

	//
	// find a file name to save it to
	//
	Q_strcpy( pcxname, "HalfLife__.tga" );

	for ( i = 0; i <= 99; i++ )
	{
		pcxname[8] = i / 10 + '0';
		pcxname[9] = i % 10 + '0';
		if ( !FS_FileExists( pcxname ) )
			break;	// file doesn't exist
	}
	if ( i == 100 )
	{
		Con_Printf( "SCR_ScreenShot_f: Couldn't create a TGA file\n" );
		return;
	}

	buffer = Mem_Malloc( glwidth * glheight * 3 + 18 );
	Q_memset( buffer, 0, 18 );
	buffer[2] = 2;		// uncompressed type
	buffer[12] = glwidth & 255;
	buffer[13] = glwidth >> 8;
	buffer[14] = glheight & 255;
	buffer[15] = glheight >> 8;
	buffer[16] = 24;	// pixel size

	qglReadPixels( glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer + 18 );

	// swap rgb to bgr
	c = glwidth * glheight * 3 + 18;
	for ( i = 18; i < c; i += 3 )
	{
		temp = buffer[i];
		buffer[i] = buffer[i + 2];
		buffer[i + 2] = temp;
	}
	COM_WriteFile( pcxname, buffer, glwidth * glheight * 3 + 18 );

	Mem_Free( buffer );
	Con_Printf( "Wrote %s\n", pcxname );
}

//=============================================================================

/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque( qboolean reconnect )
{
	S_StopAllSounds( true );

	if ( cls.state != ca_connected && cls.state != ca_uninitialized && cls.state != ca_active )
		return;
	if ( cls.signon != SIGNONS )
		return;

	// redraw with no console and the loading plaque
	Con_ClearNotify();
	scr_centertime_off = 0;
	scr_con_current = 0;

	scr_drawloading = true;
	scr_fullupdate = 0;

	StartLoadingProgressBar( "Connecting", 12 );
	SetLoadingProgressBarStatusText( "#GameUI_EstablishingConnection" );

	SCR_UpdateScreen();
	SCR_UpdateScreen();

	scr_fullupdate = 0;

	VGuiWrap2_LoadingStarted( "transition", "" );
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque( void )
{
	scr_fullupdate = 0;
	scr_drawloading = false;
	Con_ClearNotify();
	VGuiWrap2_LoadingFinished( "transition", "" );
}

//=============================================================================

void SCR_TileClear( void )
{
	if ( r_refdef.vrect.x > 0 )
	{
		// left
		Draw_TileClear( 0, 0, r_refdef.vrect.x, 152 );
		// right
		Draw_TileClear( r_refdef.vrect.x + r_refdef.vrect.width, 0,
			320 - r_refdef.vrect.x + r_refdef.vrect.width, 152 );
	}
	if ( r_refdef.vrect.height < 152 )
	{
		// top
		Draw_TileClear( r_refdef.vrect.x, 0,
			r_refdef.vrect.width, r_refdef.vrect.y );
		// bottom
		Draw_TileClear( r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height,
			r_refdef.vrect.width, 152 - ( r_refdef.vrect.y + r_refdef.vrect.height ) );
	}
}

void GL_BreakOnError( void )
{
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen( void )
{
	static float	oldscr_viewsize;
	static qboolean	recursionGuard;
	double			fCurrentTime = 0.0;

	if ( recursionGuard )
		return;

	recursionGuard = true;

	V_CheckGamma();

	if ( gfBackground )
	{
		recursionGuard = false;
		return;
	}

	if ( scr_skipupdate )
	{
		recursionGuard = false;
		return;
	}

	if ( scr_skiponeupdate )
	{
		scr_skiponeupdate = false;
		recursionGuard = false;
		return;
	}

	scr_copytop = 0;
	scr_copyeverything = 0;

	if ( cls.state == ca_dedicated )
	{
		recursionGuard = false;
		return;		// stdout only
	}

	if ( !scr_initialized || !con_initialized )
	{
		recursionGuard = false;
		return;		// not initialized yet
	}

	fCurrentTime = Sys_FloatTime();

	if ( ( cls.state == ca_connected || cls.state == ca_connecting || cls.state == ca_active ) && g_LastScreenUpdateTime > 0 && ( fCurrentTime - g_LastScreenUpdateTime ) > 120 )
	{
		Con_Printf( "load failed.\n" );
		COM_ExplainDisconnection( true, "Connection to server lost during level change." );
		CL_Disconnect();
	}

	if ( g_modfuncs.m_pfnFrameRender1 )
		g_modfuncs.m_pfnFrameRender1();

	V_UpdatePalette();

	GL_BeginRendering( &glx, &gly, &glwidth, &glheight );

	//
	// determine size of refresh window
	//
	if ( vid.recalc_refdef )
		SCR_CalcRefdef();

	//
	// do 3D refresh drawing, and then update the screen
	//
	SCR_SetUpToDrawConsole();

	GLBeginHud();

	SCR_TileClear();

	if ( scr_drawloading )
	{
		SCR_DrawLoading();
		Sbar_Draw();
	}
	else if ( cl.intermission == 1 && key_dest == key_game )
	{
	}
	else if ( cl.intermission == 2 && key_dest == key_game )
	{
		SCR_CheckDrawCenterString();
	}
	else if ( cl.intermission == 3 && key_dest == key_game )
	{
		SCR_CheckDrawCenterString();
	}
	else
	{
		GL_Bind( r_notexture_mip->gl_texturenum );
		if ( scr_con_current < vid.height )
			Sbar_Draw();

		if ( developer.value )
		{
			GL_Bind( r_notexture_mip->gl_texturenum );
			Con_DrawNotify();
		}
	}

	SCR_ConnectMsg();

	VGui_Paint();

	GLFinishHud();

	GLBeginHud();

	SCR_DrawFPS();
	SCR_NetGraph();

	if ( !VGuiWrap2_IsGameUIVisible() )
		SCR_CheckDrawCenterString();

	GLFinishHud();

	if ( g_modfuncs.m_pfnFrameRender2 )
		g_modfuncs.m_pfnFrameRender2();

	GL_EndRendering();

	g_LastScreenUpdateTime = fCurrentTime;
	recursionGuard = false;
}

void D_FillRect( vrect_t *r, unsigned char *color )
{
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglEnable( GL_ALPHA_TEST );
	qglColor4f( (float)( color[0] / 255.0 ), (float)( color[1] / 255.0 ), (float)( color[2] / 255.0 ), 1.0 );
	qglDisable( GL_DEPTH_TEST );

	qglBegin( GL_QUADS );
	qglVertex2f( r->x, r->y );
	qglVertex2f( r->x + r->width, r->y );
	qglVertex2f( r->x + r->width, r->y + r->height );
	qglVertex2f( r->x, r->y + r->height );
	qglEnd();

	qglDisable( GL_ALPHA_TEST );
	qglEnable( GL_DEPTH_TEST );
	qglColor3f( 1, 1, 1 );
	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
}

void SCR_ConnectMsg( void )
{
	int		w, h, x, y;
	vrect_t	rcFill;
	byte	color[3];
	int		wid[3];
	int		i;
	int		r, g, b;

	if ( scr_connectmsg.string[0] == '0' )
		return;

	r = 192;
	g = 192;
	b = 192;

	wid[0] = Draw_StringLen( scr_connectmsg.string, VGUI2_GetConsoleFont() );
	wid[1] = Draw_StringLen( scr_connectmsg1.string, VGUI2_GetConsoleFont() );
	wid[2] = Draw_StringLen( scr_connectmsg2.string, VGUI2_GetConsoleFont() );

	if ( scr_vrect.width <= 270 )
	{
		w = scr_vrect.width - 2;
	}
	else
	{
		w = -1;
		for ( i = 0; i < 3; i++ )
		{
			if ( wid[i] > w )
				w = wid[i];
		}

		if ( w == -1 )
			return;

		if ( w > scr_vrect.width - 2 )
			w = scr_vrect.width - 2;
	}

	if ( scr_vrect.height <= 14 )
		h = scr_vrect.height - 2;
	else
		h = 14;

	x = scr_vrect.x + ( scr_vrect.width - w ) / 2 + 1;
	y = scr_vrect.height - h * 5 - h / 2;

	rcFill.x = x - 5;
	rcFill.y = y - h / 2;
	rcFill.width = w + 10;
	rcFill.height = h * 4;

	color[0] = color[1] = color[2] = 0;

	D_FillRect( &rcFill, color );

	Draw_SetTextColor( r, g, b );
	Draw_String( x, y, scr_connectmsg.string );

	if ( scr_connectmsg1.string[0] != '0' )
	{
		y += h;
		Draw_SetTextColor( r, g, b );
		Draw_String( x, y, scr_connectmsg1.string );
	}

	if ( scr_connectmsg2.string[0] != '0' )
	{
		y += h;
		Draw_SetTextColor( r, g, b );
		Draw_String( x, y, scr_connectmsg2.string );
	}
}

void SCR_DrawDownloadText( void )
{
	char	szStatusText[128];
	int		w, h, x, y;
	vrect_t	rcFill;
	byte	color[3];

	if ( cls.state == ca_active )
		return;

	if ( Q_strlen( gDownloadFile ) <= 0 )
		return;

	if ( vid.width <= 250 )
		w = vid.width - 2;
	else
		w = 250;

	if ( vid.height <= 10 )
		h = vid.height - 2;
	else
		h = 14;

	_snprintf( szStatusText, sizeof( szStatusText ), "Downloading %s", gDownloadFile );

	w = Draw_StringLen( szStatusText, VGUI2_GetConsoleFont() );

	x = ( vid.width - w ) / 2 + 1;
	y = vid.height - 40;
	if ( y <= 22 )
		y = 22;

	rcFill.x = x - 1;
	rcFill.y = y;
	rcFill.width = w + 2;
	rcFill.height = h;

	color[0] = color[1] = color[2] = 0;

	D_FillRect( &rcFill, color );

	Draw_SetTextColor( 0.9, 0.9, 0.9 );
	Draw_String( x, y, szStatusText );
}
