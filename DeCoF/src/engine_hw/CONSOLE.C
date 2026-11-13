// console.c -- the developer console, the notify lines and the debug overlay

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "con_nprint.h"
#include "APIProxy.h"
#include "vid.h"
#include "screen.h"
#include "draw.h"
#include "keys.h"
#include "host.h"
#include "vgui_int.h"
#include "cdll_int.h"
#include "mathlib.h"
#include "client.h"
#include "delta.h"
#include "server.h"

#include <io.h>
#include <fcntl.h>

qboolean	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display

cvar_t		console = { "console", "0.0", FCVAR_ARCHIVE };

int			con_linewidth = 1;

float		con_cursorspeed = 4;

int			con_current;		// where next message will be printed
int			con_x;				// offset in current line for next print
char		*con_text = NULL;

cvar_t		con_fastmode = { "con_fastmode", "1" };
cvar_t		con_notifytime = { "con_notifytime", "4" };		//seconds
cvar_t		con_color = { "con_color", "255 255 255", FCVAR_ARCHIVE };

int			con_num_times = 4;
float		*con_times;			// realtime time the line was generated
								// for transparent notify lines
int			*con_notifypos;

static char	g_szNotifyAreaString[256];

qboolean	con_debuglog;

qboolean	con_initialized;

int			con_notifylines;	// scan lines to clear for notify lines

/*
================
Con_SetTimes_f

Sets the number of notify lines
================
*/
void Con_SetTimes_f( void )
{
	int		newtimes;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "contimes <n>\nShow <n> overlay lines [4-64].\n%i current overlay lines.\n", con_num_times );
		return;
	}

	newtimes = Q_atoi( Cmd_Argv( 1 ) );
	if ( newtimes < 4 )
		newtimes = 4;
	if ( newtimes > 64 )
		newtimes = 64;

	if ( con_times )
		Mem_Free( con_times );
	if ( con_notifypos )
		Mem_Free( con_notifypos );

	con_times = (float *)Mem_Malloc( newtimes * sizeof( float ) );
	con_notifypos = (int *)Mem_Malloc( newtimes * sizeof( int ) );

	if ( !con_times || !con_notifypos )
		Sys_Error( "Couldn't allocate space for %i console overlays.", newtimes );

	con_num_times = newtimes;
	Con_Printf( "%i lines will overlay.\n", newtimes );
}

/*
================
Con_HideConsole_f
================
*/
void Con_HideConsole_f( void )
{
	VGuiWrap2_HideConsole();
}

/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f( void )
{
	if ( VGuiWrap2_IsConsoleVisible() )
		VGuiWrap2_HideConsole();
	else
		VGuiWrap2_ShowConsole();
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f( void )
{
	if ( con_text )
		Q_memset( con_text, ' ', CON_TEXTSIZE );

	VGuiWrap2_ClearConsole();
}

/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void )
{
	int		i;

	if ( !con_times )
		return;

	for ( i = 0; i < con_num_times; i++ )
		con_times[i] = 0;

	g_szNotifyAreaString[0] = 0;
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f( void )
{
	if ( VGuiWrap2_IsInCareerMatch() )
		return;

	key_dest = key_message;

	if ( Cmd_Argc() == 2 )
	{
		Q_strncpy( message_type, Cmd_Argv( 1 ), 31 );
		message_type[31] = 0;
	}
	else
	{
		Q_strcpy( message_type, "say" );
	}
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f( void )
{
	if ( VGuiWrap2_IsInCareerMatch() )
		return;

	key_dest = key_message;
	Q_strcpy( message_type, "say_team" );
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize( void )
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = ( vid.width >> 3 ) - 2;

	if ( width == con_linewidth )
		return;

	if ( width < 1 )			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		Q_memset( con_text, ' ', CON_TEXTSIZE );
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if ( con_totallines < numlines )
			numlines = con_totallines;

		numchars = oldwidth;

		if ( con_linewidth < numchars )
			numchars = con_linewidth;

		Q_memcpy( tbuf, con_text, CON_TEXTSIZE );
		Q_memset( con_text, ' ', CON_TEXTSIZE );

		for ( i = 0; i < numlines; i++ )
		{
			for ( j = 0; j < numchars; j++ )
			{
				con_text[( con_totallines - 1 - i ) * con_linewidth + j] =
					tbuf[( ( con_current - i + oldtotallines ) % oldtotallines ) * oldwidth + j];
			}
		}

		Con_ClearNotify();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}

/*
================
Con_Debug_f
================
*/
void Con_Debug_f( void )
{
	if ( con_debuglog )
	{
		Con_Printf( "condebug disabled\n" );
		con_debuglog = FALSE;
	}
	else
	{
		con_debuglog = TRUE;
		Con_Printf( "condebug enabled\n" );
	}
}

/*
================
Con_Init
================
*/
void Con_Init( void )
{
	con_debuglog = COM_CheckParm( "-condebug" );
	if ( con_debuglog )
		FS_RemoveFile( "qconsole.log", NULL );

	con_text = (char *)Hunk_AllocName( CON_TEXTSIZE, "context" );
	Q_memset( con_text, ' ', CON_TEXTSIZE );
	con_linewidth = -1;

	con_times = (float *)Mem_Malloc( con_num_times * sizeof( float ) );
	con_notifypos = (int *)Mem_Malloc( con_num_times * sizeof( int ) );
	if ( !con_times || !con_notifypos )
		Sys_Error( "Couldn't allocate space for %i console overlays.", con_num_times );

	Con_CheckResize();

	Con_DPrintf( "Console initialized.\n" );

	//
	// register our commands
	//
	Cvar_RegisterVariable( &con_fastmode );
	Cvar_RegisterVariable( &con_notifytime );
	Cvar_RegisterVariable( &con_color );

	Cmd_AddCommand( "contimes", Con_SetTimes_f );
	Cmd_AddCommand( "toggleconsole", Con_ToggleConsole_f );
	Cmd_AddCommand( "hideconsole", Con_HideConsole_f );
	Cmd_AddCommand( "messagemode", Con_MessageMode_f );
	Cmd_AddCommand( "messagemode2", Con_MessageMode2_f );
	Cmd_AddCommand( "clear", Con_Clear_f );
	Cmd_AddCommand( "condebug", Con_Debug_f );

	con_initialized = TRUE;
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown( void )
{
	if ( con_times )
		Mem_Free( con_times );
	if ( con_notifypos )
		Mem_Free( con_notifypos );

	con_times = NULL;
	con_notifypos = NULL;
	con_initialized = FALSE;
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed( void )
{
	con_x = 0;
	con_current++;
	Q_memset( &con_text[( con_current % con_totallines ) * con_linewidth], ' ', con_linewidth );
}

/*
================
Con_DebugLog
================
*/
void Con_DebugLog( char *file, char *fmt, ... )
{
	va_list		argptr;
	static char	data[1024];
	int			fd;

	va_start( argptr, fmt );
	_vsnprintf( data, sizeof( data ), fmt, argptr );
	va_end( argptr );

	fd = _open( file, _O_WRONLY | _O_CREAT | _O_APPEND, 0666 );
	_write( fd, data, Q_strlen( data ) );
	_close( fd );
}

#define	MAXPRINTMSG	4096

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
void Con_Printf( char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	// also echo to debugging console
	Sys_Printf( "%s", msg );

	if ( sv_redirected )
	{
		// rcon redirect
		if ( Q_strlen( msg ) + Q_strlen( outputbuf ) > sizeof( outputbuf ) - 1 )
		{
			SV_FlushRedirect();
		}

		strncat( outputbuf, msg, sizeof( outputbuf ) - 1 );
		return;
	}

	// log all messages to file
	if ( con_debuglog )
	{
		Con_DebugLog( "qconsole.log", "%s", msg );
	}

	if ( !host_initialized )
		return;

	if ( !con_initialized )
		return;

	if ( cls.state == ca_dedicated )
		return;		// no graphics mode

	if ( developer.value )
	{
		strncpy( g_szNotifyAreaString, msg, sizeof( g_szNotifyAreaString ) );
		g_szNotifyAreaString[sizeof( g_szNotifyAreaString ) - 1] = 0;
		*con_times = realtime;
	}

	VGuiWrap2_ConPrintf( msg );
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf( char *fmt, ... )
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !developer.value )
		return;			// don't confuse non-developers with techie stuff...

	if ( scr_con_current && cls.state == ca_active )
		return;

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	OutputDebugString( msg );

	VGuiWrap2_ConDPrintf( msg );
}

qboolean	flIsDebugPrint;

/*
==================
Con_SafePrintf

Okay to call even when the screen can't be updated
==================
*/
void Con_SafePrintf( char *fmt, ... )
{
	va_list		argptr;
	char		msg[1024];

	va_start( argptr, fmt );
	_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	flIsDebugPrint = TRUE;
	Con_Printf( "%s", msg );
	flIsDebugPrint = FALSE;
}

/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify( void )
{
	int		x, v;
	char	*text;
	int		i;
	int		charpos;
	float	time;

	v = 0;

	Con_DrawDebugAreas();
	Draw_ResetTextColor();

	for ( i = con_current - con_num_times + 1; i <= con_current; i++ )
	{
		if ( i < 0 )
			continue;
		time = con_times[i % con_num_times];
		if ( time == 0 )
			continue;
		time = realtime - time;
		if ( time > con_notifytime.value )
			continue;

		text = g_szNotifyAreaString;

		clearnotify = 0;
		scr_copytop = 1;

		charpos = 8;
		for ( x = 0; x < con_linewidth; x++ )
			charpos += Draw_Character( charpos, v, text[x], VGUI2_GetConsoleFont() );

		v += VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	}

	if ( key_dest == key_message )
	{
		int		showsize;
		int		xoffset;
		int		chatx;

		showsize = vid.width / 10;
		chatx = 8;

		clearnotify = 0;
		scr_copytop = 1;

		x = 0;

		ClientDLL_ChatInputPosition( &chatx, &v );

		if ( chat_bufferlen > showsize )
			x = chat_bufferlen - showsize;

		xoffset = chatx;
		xoffset = Draw_String( xoffset, v, message_type );
		xoffset += 3;
		xoffset = Draw_String( xoffset, v, ":" );
		xoffset += 3;
		xoffset = Draw_String( xoffset, v, chat_buffer + x );

		Draw_Character( xoffset + 1, v, 10 + ( (int)( con_cursorspeed * realtime ) & 1 ), VGUI2_GetConsoleFont() );
		v += VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	}

	if ( v > con_notifylines )
		con_notifylines = v;
}

typedef struct
{
	char	szNotify[80];
	float	expire;
	float	color[3];
} da_notify_t;

static da_notify_t	da_notify[MAX_DBG_NOTIFY];
float				da_default_color[3] = { 1.0, 1.0, 1.0 };

/*
================
Con_DrawDebugArea
================
*/
void Con_DrawDebugArea( int idx )
{
	int		len;
	int		x;
	int		line_height;

	if ( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	line_height = VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
	len = Draw_StringLen( da_notify[idx].szNotify, VGUI2_GetConsoleFont() );
	x = vid.width - 10 - len;

	if ( line_height * idx + 20 > (int)vid.height - 20 )
		return;

	Draw_SetTextColor( da_notify[idx].color[0], da_notify[idx].color[1], da_notify[idx].color[2] );
	Draw_String( x, line_height * idx + 20, da_notify[idx].szNotify );
}

/*
================
Con_DrawDebugAreas
================
*/
void Con_DrawDebugAreas( void )
{
	int		i;

	for ( i = 0; i < MAX_DBG_NOTIFY; i++ )
	{
		if ( da_notify[i].expire > realtime )
			Con_DrawDebugArea( i );
	}
}

/*
================
Con_NPrintf
================
*/
void Con_NPrintf( int idx, char *fmt, ... )
{
	va_list		argptr;

	g_engdstAddrs.Con_NPrintf( &idx, &fmt );

	if ( idx < 0 || idx >= MAX_DBG_NOTIFY )
		return;

	va_start( argptr, fmt );
	_vsnprintf( da_notify[idx].szNotify, sizeof( da_notify[idx].szNotify ), fmt, argptr );
	va_end( argptr );

	// reset values
	da_notify[idx].expire = realtime + DBG_NOTIFY_TIMEOUT;
	da_notify[idx].color[0] = da_default_color[0];
	da_notify[idx].color[1] = da_default_color[1];
	da_notify[idx].color[2] = da_default_color[2];
}

/*
================
Con_NXPrintf
================
*/
void Con_NXPrintf( struct con_nprint_s *info, char *fmt, ... )
{
	va_list		argptr;

	g_engdstAddrs.Con_NXPrintf( &info, &fmt );

	if ( !info )
		return;

	if ( info->index < 0 || info->index >= MAX_DBG_NOTIFY )
		return;

	va_start( argptr, fmt );
	_vsnprintf( da_notify[info->index].szNotify, sizeof( da_notify[info->index].szNotify ), fmt, argptr );
	// null terminate
	da_notify[info->index].szNotify[sizeof( da_notify[info->index].szNotify ) - 1] = 0;
	va_end( argptr );

	da_notify[info->index].expire = realtime + info->time_to_live;
	da_notify[info->index].color[0] = info->color[0];
	da_notify[info->index].color[1] = info->color[1];
	da_notify[info->index].color[2] = info->color[2];
}

/*
================
Con_IsVisible
================
*/
int Con_IsVisible( void )
{
	g_engdstAddrs.Con_IsVisible();

	return (int)scr_con_current;
}
