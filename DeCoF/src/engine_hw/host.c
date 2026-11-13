// host.c -- coordinates spawning and killing of local servers

#include <setjmp.h>
#include <time.h>
#include <io.h>
#include <sys/stat.h>
#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "info.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "gl_model.h"
#include "protocol.h"
#include "vid.h"
#include "render.h"
#include "draw.h"
#include "sound.h"
#include "screen.h"
#include "keys.h"
#include "view.h"
#include "chase.h"
#include "wad.h"
#include "decal.h"
#include "hashpak.h"
#include "sv_log.h"
#include "pmove.h"
#include "cdll_int.h"
#include "vgui_int.h"
#include "eiface.h"
#include "APIProxy.h"
#include "dll_state.h"

// cl_main.c
extern	cvar_t	rate;

// gl_screen.c
extern	cvar_t		scr_connectmsg;
extern	qboolean	scr_skipupdate;
extern	qboolean	gfBackground;
void	SCR_Init( void );
void	SCR_UpdateScreen( void );

// console.c
extern	cvar_t	console;

// view.c
extern	vec3_t	r_soundOrigin;
extern	vec3_t	r_playerViewportAngles;
void	V_Init( void );

// snd_dma.c
extern	cvar_t	suitvolume;
void	S_Init( void );
void	S_Shutdown( void );
void	S_Update( vec_t *origin, vec_t *forward, vec_t *right, vec_t *up );
void	S_PrintStats( void );

// cd.cpp
void	CDAudio_Init( void );
void	CDAudio_Shutdown( void );

// textures.c
extern	char	*wadpath;

// voice.cpp
void	Voice_RegisterCvars( void );
void	Voice_Deinit( void );
void	Voice_Idle( float frametime );
qboolean	Voice_IsRecording( void );
int		Voice_GetCompressedData( char *pchData, int maxsize, qboolean bFinal );

// cdll_int.c
void	ClientDLL_Init( void );
void	ClientDLL_Shutdown( void );
void	ClientDLL_UpdateClientData( void );
void	ClientDLL_Frame( double time );
void	ClientDLL_CAM_Think( void );
kbutton_t	*ClientDLL_FindKey( const char *name );

// in_win.c
void	IN_UpdateMouse_guess( void );	// 0x01D59ADE, first thing in _Host_Frame

// vgui_int.c
void	VGui_Startup( void );
void	VGui_Shutdown( void );

// C wrappers around the object at 0x01EDF808 (names unproven)
void	AdsStartup_guess( void );						// 0x01D10D8E, Host_Init
void	AdsShutdown_guess( void );						// 0x01D10D9D, Host_Shutdown
void	AdsFrame_guess( struct model_s *worldmodel );	// 0x01D10DAC, _Host_Frame

// sys_dll.c
void	Sys_SetRegKeyValue( char *pszSubKey, char *pszElement, char *pszValue );
void	Sys_GetRegKeyValue( char *pszSubKey, char *pszElement, char *pszReturnString, int nReturnLength, char *pszDefaultValue );

// SystemWrapper.cpp
void	SystemWrapper_Init( void );
void	SystemWrapper_RunFrame( double time );
void	SystemWrapper_ShutDown( void );

// FileSystem.cpp
void	FS_GetInterfaceVersion( char *p, int maxlen );

// sv_steam3.cpp
void	Steam_ClientRunFrame( void );

// download.cpp
void	CL_HTTPUpdate( void );

// cl_spectator.c
void	CL_MoveSpectatorCamera( void );

// cl_parse.c
void	CL_GGSpeeds( float flCurTime );

// cmodel.c
void	CM_FreePAS( void );

// model.c
void	Mod_Init( void );
void	Mod_ClearAll( void );

// gl_rmisc.c, gl_rmain.c, gl_vidnt.c, gl_draw.c
void	D_FlushCaches( void );
void	R_SetStackBase( void );
void	R_Init( void );
void	R_InitTextures( void );
void	GL_Init( void );
void	GL_Config( void );
int		VID_Init( unsigned short *palette );
void	Draw_Init( void );
void	Draw_Shutdown( void );
void	Draw_DecalShutdown( void );

// chase.c, wad.c, decals.c, net_ws.c, DemoPlayerWrapper.cpp, pr_edict.c, pmove.c, pr_dlls.c
void	Chase_Init( void );
void	W_LoadWadFile( char *filename );
void	W_Shutdown( void );
void	Decal_Init( void );
void	NET_Init( void );
void	NET_Shutdown( void );
void	DemoPlayer_Init( void );
void	FreeAllEntPrivateData( void );
void	PM_Init( playermove_t *ppm );
void	ReleaseEntityDlls( void );

// steam_api flat interface
__declspec( dllimport ) void	ISteamMasterServerUpdater_ForceHeartbeat( void );
__declspec( dllimport ) char	ISteamApps_BIsLowViolence( void );

/*

A server can always be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/

cvar_t	sys_ticrate = { "sys_ticrate", "100.0" };
cvar_t	sys_timescale = { "sys_timescale", "1.0" };
cvar_t	fps_max = { "fps_max", "72.0", FCVAR_ARCHIVE };
cvar_t	fps_modem = { "fps_modem", "0.0", FCVAR_ARCHIVE };
cvar_t	host_killtime = { "host_killtime", "0" };
cvar_t	sv_stats = { "sv_stats", "1" };

qboolean	g_bUsingInGameAdvertisements = false;

quakeparms_t	host_parms;

qboolean	host_initialized = false;	// true if into command execution
qboolean	gfNoMasterServer = false;

double		host_frametime;
double		realtime;				// without any filtering or bounding
double		oldrealtime;			// last frame run

int			host_framecount;
int			host_hunklevel;

int			minimum_memory;

client_t	*host_client;			// current client

jmp_buf		host_abortserver;
jmp_buf		host_enddemo;

unsigned short	*host_basepal;
unsigned char	*host_colormap;

static cvar_t	host_name = { "hostname", "Cry of Fear" };

cvar_t	host_framerate = { "host_framerate", "0" };	// set for slow motion

static cvar_t	host_speeds = { "host_speeds", "0" };	// set for running times
static cvar_t	host_profile = { "host_profile", "0" };

cvar_t	developer = { "developer", "0" };

cvar_t	host_limitlocal = { "host_limitlocal", "0" };

cvar_t	skill = { "skill", "1" };			// 0 - 3
cvar_t	deathmatch = { "deathmatch", "0", FCVAR_SERVER };	// 0, 1, or 2
cvar_t	coop = { "coop", "0", FCVAR_SERVER };		// 0 or 1

cvar_t	pausable = { "pausable", "1", FCVAR_SERVER };

/*
================
Host_EndGame
================
*/
void Host_EndGame( char *message, ... )
{
	int		oldn;
	va_list	argptr;
	char	string[1024];

	va_start( argptr, message );
	_vsnprintf( string, sizeof( string ), message, argptr );
	va_end( argptr );
	Con_DPrintf( "Host_EndGame: %s\n", string );

	oldn = cls.demonum;
	if ( sv.active )
		Host_ShutdownServer( false );
	cls.demonum = oldn;

	if ( cls.state == ca_dedicated )
		Sys_Error( "Host_EndGame: %s\n", string );	// dedicated servers exit

	if ( cls.demonum != -1 )
	{
		oldn = cls.demonum;
		CL_Disconnect_f();
		cls.demonum = oldn;
		Host_NextDemo();
		longjmp( host_enddemo, 1 );
	}

	CL_Disconnect();
	Cbuf_AddText( "cd stop\n" );
	Cbuf_Execute();
	longjmp( host_abortserver, 1 );
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error( char *error, ... )
{
	va_list	argptr;
	char	string[1024];
	static qboolean inerror = false;

	if ( inerror )
		Sys_Error( "Host_Error: recursively entered" );
	inerror = true;

	SCR_EndLoadingPlaque();		// reenable screen updates

	va_start( argptr, error );
	_vsnprintf( string, sizeof( string ), error, argptr );
	va_end( argptr );

	if ( !sv.active && developer.value )
		CL_WriteMessageHistory( 0, 0 );

	Con_Printf( "Host_Error: %s\n", string );

	if ( sv.active )
		Host_ShutdownServer( false );

	if ( cls.state == ca_dedicated )
		Sys_Error( "Host_Error: %s\n", string );	// dedicated servers exit

	CL_Disconnect();
	cls.demonum = -1;

	inerror = false;

	longjmp( host_abortserver, 1 );
}

/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal( void )
{
	Host_InitCommands();

	Cvar_RegisterVariable( &host_killtime );
	Cvar_RegisterVariable( &sys_ticrate );
	Cvar_RegisterVariable( &fps_max );
	Cvar_RegisterVariable( &fps_modem );
	Cvar_RegisterVariable( &host_name );
	Cvar_RegisterVariable( &host_limitlocal );

	sys_timescale.value = 1.0f;

	Cvar_RegisterVariable( &host_framerate );
	Cvar_RegisterVariable( &host_speeds );
	Cvar_RegisterVariable( &host_profile );

	Cvar_RegisterVariable( &mp_logfile );
	Cvar_RegisterVariable( &mp_logecho );
	Cvar_RegisterVariable( &sv_log_onefile );
	Cvar_RegisterVariable( &sv_log_singleplayer );
	Cvar_RegisterVariable( &sv_logsecret );
	Cvar_RegisterVariable( &sv_stats );

	Cvar_RegisterVariable( &developer );
	Cvar_RegisterVariable( &deathmatch );
	Cvar_RegisterVariable( &coop );
	Cvar_RegisterVariable( &pausable );
	Cvar_RegisterVariable( &skill );

	SV_SetMaxclients();
}

/*
===============
Info_WriteVars

Writes the userinfo keys that are not cvars to the config file
===============
*/
void Info_WriteVars( FileHandle_t fp )
{
	static char	value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static int	valueindex;
	cvar_t	*pcvar;
	char	*s;
	char	pkey[512];
	char	*o;

	s = cls.userinfo;

	valueindex = ( valueindex + 1 ) % 4;
	if ( *s == '\\' )
		s++;

	while ( 1 )
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while ( *s != '\\' && *s )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;

		pcvar = Cvar_FindVar( pkey );
		if ( !pcvar && pkey[0] != '*' )
			FS_FPrintf( fp, "setinfo \"%s\" \"%s\"\n", pkey, value[valueindex] );

		if ( !*s )
			return;
		s++;
	}
}

/*
===============
Host_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void Host_WriteConfiguration( void )
{
	FILE		*f;
	kbutton_t	*ml, *jl;
	qboolean	bSetFileToReadOnly;

	bSetFileToReadOnly = false;

	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if ( host_initialized && cls.state != ca_dedicated )
	{
		if ( cls.state != ca_dedicated )
		{
			Sys_SetRegKeyValue( "Software\\Valve\\Steam", "Rate", rate.string );
			if ( cl_name.string && Q_stricmp( cl_name.string, "unnamed" ) && Q_stricmp( cl_name.string, "player" ) && Q_strlen( cl_name.string ) )
				Sys_SetRegKeyValue( "Software\\Valve\\Steam", "LastGameNameUsed", cl_name.string );
		}

		if ( Key_CountBindings() <= 1 )
		{
			Con_Printf( "skipping config.cfg output, no keys bound\n" );
		}
		else
		{
			f = FS_Open( "config.cfg", "w" );
			if ( !f && developer.value )
			{
				char	nameBuf[MAX_PATH];

				FS_GetLocalPath( "config.cfg", nameBuf, sizeof( nameBuf ) );
				_chmod( nameBuf, _S_IREAD | _S_IWRITE );
				f = FS_Open( "config.cfg", "w" );
				bSetFileToReadOnly = true;
			}

			if ( !f )
			{
				Con_Printf( "Couldn't write config.cfg.\n" );
			}
			else
			{
				FS_FPrintf( f, "// This file is overwritten whenever you change your user settings in the game.\n" );
				FS_FPrintf( f, "// Add custom configurations to the file \"userconfig.cfg\".\n\n" );
				FS_FPrintf( f, "unbindall\n" );

				Key_WriteBindings( f );
				Cvar_WriteVariables( f );
				Info_WriteVars( f );

				ml = ClientDLL_FindKey( "in_mlook" );
				jl = ClientDLL_FindKey( "in_jlook" );

				if ( ml && ( ml->state & 1 ) )
					FS_FPrintf( f, "+mlook\n" );

				if ( jl && ( jl->state & 1 ) )
					FS_FPrintf( f, "+jlook\n" );

				FS_FPrintf( f, "exec userconfig.cfg\n" );

				FS_Close( f );

				if ( bSetFileToReadOnly )
				{
					char	nameBuf[MAX_PATH];

					FS_GetLocalPath( "config.cfg", nameBuf, sizeof( nameBuf ) );
					_chmod( nameBuf, _S_IREAD );
				}
			}
		}
	}
}

/*
===============
Host_WriteCustomConfig

Writes key bindings and archived cvars to a custom config file
===============
*/
void Host_WriteCustomConfig( void )
{
	FILE		*f;
	kbutton_t	*ml, *jl;
	char		configname[261];

	_snprintf( configname, 257, "%s", Cmd_Args() );

	if ( !configname )
	{
		Con_Printf( "writecfg: writecfg <filename>\n" );
		return;
	}

	if ( Q_strstr( configname, ".." )
		|| !Q_stricmp( configname, "config" )
		|| !Q_stricmp( configname, "autoexec" )
		|| !Q_stricmp( configname, "listenserver" )
		|| !Q_stricmp( configname, "server" )
		|| !Q_stricmp( configname, "userconfig" ) )
	{
		Con_Printf( "skipping writecfg output, invalid filename given\n" );
		return;
	}

	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if ( host_initialized && cls.state != ca_dedicated )
	{
		if ( Key_CountBindings() <= 1 )
		{
			Con_Printf( "skipping config.cfg output, no keys bound\n" );
			return;
		}

		Q_strcat( configname, ".cfg" );

		f = FS_Open( configname, "w" );
		if ( !f )
		{
			Con_Printf( "Couldn't write %s.\n", configname );
			return;
		}

		FS_FPrintf( f, "unbindall\n" );

		Key_WriteBindings( f );
		Cvar_WriteVariables( f );
		Info_WriteVars( f );

		ml = ClientDLL_FindKey( "in_mlook" );
		jl = ClientDLL_FindKey( "in_jlook" );

		if ( ml && ( ml->state & 1 ) )
			FS_FPrintf( f, "+mlook\n" );

		if ( jl && ( jl->state & 1 ) )
			FS_FPrintf( f, "+jlook\n" );

		FS_Close( f );
		Con_Printf( "%s successfully created!\n", configname );
	}
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf( char *fmt, ... )
{
	va_list	argptr;
	char	string[1024];

	if ( host_client->fakeclient )
		return;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	MSG_WriteByte( &host_client->netchan.message, svc_print );
	MSG_WriteString( &host_client->netchan.message, string );
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf( char *fmt, ... )
{
	va_list	argptr;
	char	string[1024];
	int		i;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	for ( i = 0; i < svs.maxclients; i++ )
	{
		if ( ( svs.clients[i].active || svs.clients[i].spawned ) && !svs.clients[i].fakeclient )
		{
			MSG_WriteByte( &svs.clients[i].netchan.message, svc_print );
			MSG_WriteString( &svs.clients[i].netchan.message, string );
		}
	}

	Con_DPrintf( "%s", string );
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands( char *fmt, ... )
{
	va_list	argptr;
	char	string[1024];

	if ( host_client->fakeclient )
		return;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	string[ sizeof( string ) - 1 ] = 0;
	va_end( argptr );

	MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
	MSG_WriteString( &host_client->netchan.message, string );
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient( client_t *cl, qboolean crash, char *fmt, ... )
{
	int		i;
	unsigned char	final[512];
	float	connection_time;
	va_list	argptr;
	char	string[1024];

	i = 0;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	if ( !crash )
	{
		// add the disconnect
		if ( !cl->fakeclient )
		{
			MSG_WriteByte( &cl->netchan.message, svc_disconnect );
			MSG_WriteString( &cl->netchan.message, string );
			final[0] = svc_disconnect;
			strncpy( (char *)&final[1], string, min( sizeof( final ) - 1, strlen( string ) + 1 ) );
			final[ sizeof( final ) - 1 ] = 0;
			i = min( sizeof( final ) - 1, strlen( string ) + 1 ) + 1;
		}

		// call the prog function for removing a client
		// this will set the body to a dead frame, among other things
		if ( cl->edict && cl->spawned )
			gEntityInterface.pfnClientDisconnect( cl->edict );

		Sys_Printf( "Dropped %s from server\nReason:  %s\n", cl->name, string );

		if ( !cl->fakeclient )
			Netchan_Transmit( &cl->netchan, i, final );
	}

	connection_time = realtime - cl->netchan.connect_time;
	if ( connection_time > 60.0 )
	{
		svs.stats.num_sessions++;
		svs.stats.cumulative_sessiontime += connection_time;
	}

	// Throw away any residual garbage in the channel.
	Netchan_Clear( &cl->netchan );

	Steam_NotifyClientDisconnect( cl );

	// Mark the client as inactive.
	cl->active = false;
	cl->connected = false;
	cl->hasusrmsgs = false;
	cl->fakeclient = false;
	cl->spawned = false;
	cl->fully_connected = false;
	cl->name[0] = 0;
	cl->connection_started = realtime;
	cl->proxy = false;

	COM_ClearCustomizationList( &cl->customdata, false );

	cl->edict = NULL;

	Q_memset( cl->userinfo, 0, sizeof( cl->userinfo ) );
	Q_memset( cl->physinfo, 0, sizeof( cl->physinfo ) );

	// send notification to all other clients
	SV_FullClientUpdate( cl, &sv.reliable_datagram );

	NotifyDedicatedServerUI( "UpdatePlayers" );
}

/*
==================
Host_ClearClients

==================
*/
void Host_ClearClients( qboolean bFramesOnly )
{
	int		i, j;
	client_frame_t	*frame;

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( host_client->frames )
		{
			for ( j = 0; j < SV_UPDATE_BACKUP; j++ )
			{
				frame = &host_client->frames[j];
				SV_ClearPacketEntities( frame );
				frame->senttime = 0;
				frame->ping_time = -1;
			}
		}

		if ( host_client->netchan.remote_address.type )
		{
			netadr_t	save;

			save = host_client->netchan.remote_address;
			Q_memset( &host_client->netchan, 0, sizeof( netchan_t ) );
			Netchan_Setup( NS_SERVER, &host_client->netchan, save, host_client - svs.clients, (void *)&host_client, SV_GetFragmentSize );
		}

		COM_ClearCustomizationList( &host_client->customdata, false );
	}

	if ( !bFramesOnly )
	{
		for ( i = 0, host_client = svs.clients; i < svs.maxclientslimit; i++, host_client++ )
			SV_ClearFrames( &host_client->frames );

		Q_memset( svs.clients, 0, svs.maxclientslimit * sizeof( client_t ) );
		SV_AllocClientFrames();
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer( qboolean crash )
{
	int		i;

	if ( !sv.active )
		return;

	SV_ServerShutdown();

	sv.active = false;
	NET_ClearLagData( true, true );

	// stop all client sounds immediately
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( host_client->active || host_client->connected )
			SV_DropClient( host_client, crash, "Server shutting down" );
	}

	CL_Disconnect();

	SV_ClearEntities();
	SV_ClearCaches();
	FreeAllEntPrivateData();

	Q_memset( &sv, 0, sizeof( sv ) );
	CL_ClearClientState();

	SV_ClearClientStates();
	Host_ClearClients( false );

	for ( i = 0, host_client = svs.clients; i < svs.maxclientslimit; i++, host_client++ )
		SV_ClearFrames( &host_client->frames );

	Q_memset( svs.clients, 0, svs.maxclientslimit * sizeof( client_t ) );

	HPAK_FlushHostQueue();
	Steam_Shutdown();

	Log_Printf( "Server shutdown\n" );
	Log_Close();
}

/*
================
SV_ClearClientStates

================
*/
void SV_ClearClientStates( void )
{
	int		i;
	client_t	*cl;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		COM_ClearCustomizationList( &cl->customdata, false );
		SV_ClearResourceLists( cl );
	}
}

/*
================
Host_CheckDyanmicStructures

================
*/
void Host_CheckDyanmicStructures( void )
{
	int		i;
	client_t	*cl;

	if ( !svs.clients )
		return;

	for ( i = 0, cl = svs.clients; i < svs.maxclientslimit; i++, cl++ )
	{
		if ( !cl )
			break;

		if ( cl->frames )
			SV_ClearFrames( &cl->frames );
	}
}

/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory( qboolean bQuiet )
{
	// Engine string pooling
	CM_FreePAS();
	SV_ClearEntities();

	if ( !bQuiet )
		Con_DPrintf( "Clearing memory\n" );

	D_FlushCaches();
	Mod_ClearAll();

	if ( host_hunklevel )
	{
		// FIXME: check this...
		Host_CheckDyanmicStructures();
		Hunk_FreeToLowMark( host_hunklevel );
	}

	cls.signon = 0;

	SV_ClearCaches();

	Q_memset( &sv, 0, sizeof( sv ) );

	CL_ClearClientState();
	SV_ClearClientStates();
}

//============================================================================

/*
===================
Host_FilterTime

Returns false if the time is too short to run a frame
===================
*/
qboolean Host_FilterTime( float time )
{
	float	fps;
	static int	command_line_ticrate = -1;

	if ( host_framerate.value > 0 && ( Host_IsSinglePlayerGame() || cls.demoplayback ) )
	{
		host_frametime = host_framerate.value * sys_timescale.value;
		realtime += host_frametime;
		return true;
	}

	realtime += time * sys_timescale.value;

	if ( !g_bIsDedicatedServer )
	{
		if ( !sv.active && fps_modem.value > 0.0 )
			fps = fps_modem.value;
		else
			fps = fps_max.value;

		// Limit fps while connecting
		if ( !sv.active && cls.state != ca_disconnected && cls.state != ca_active )
			fps = 30.0f;

		fps = max( 0.5, fps );

		if ( !developer.value )
			fps = min( 100.0, fps );

		if ( cl.maxclients > 1 )
		{
			if ( fps < 20.0 )
				fps = 20.0f;
		}

		if ( !cls.timedemo )
		{
			if ( realtime - oldrealtime < sys_timescale.value / fps )
				return false;		// framerate is too high
		}
	}
	else
	{
		if ( command_line_ticrate == -1 )
			command_line_ticrate = COM_CheckParm( "-sys_ticrate" );

		if ( command_line_ticrate > 0 )
			fps = atof( com_argv[ command_line_ticrate + 1 ] );
		else
			fps = sys_ticrate.value;

		if ( fps > 0.0 )
		{
			if ( realtime - oldrealtime < 1.0 / ( fps + 1.0f ) )
				return false;
		}
	}

	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;

	if ( host_frametime > 0.25 )
		host_frametime = 0.25;
	else if ( host_frametime < 0.001 )
		host_frametime = 0.001;

	return true;
}

/*
==================
Master_IsLanGame

==================
*/
qboolean Master_IsLanGame( void )
{
	if ( sv_lan.value )
		return true;

	return false;
}

/*
==================
Master_Heartbeat_f

==================
*/
void Master_Heartbeat_f( void )
{
	ISteamMasterServerUpdater_ForceHeartbeat();
}

static double	rolling_fps;

/*
==================
Host_ComputeFPS

==================
*/
void Host_ComputeFPS( double frametime )
{
	rolling_fps = 0.6 * rolling_fps + 0.4 * frametime;
}

/*
==================
Host_GetHostInfo

==================
*/
void Host_GetHostInfo( float *fps, int *nActive, int *unused, int *nMaxPlayers, char *pszMap )
{
	int		clients = 0;

	if ( rolling_fps <= 0.0 )
	{
		rolling_fps = 0;
		*fps = 0;
	}
	else
	{
		*fps = 1.0 / (float)rolling_fps;
	}

	SV_CountPlayers( &clients );
	*nActive = clients;

	if ( unused )
		*unused = 0;

	if ( pszMap )
	{
		if ( sv.name && sv.name[0] )
			Q_strcpy( pszMap, sv.name );
		else
			*pszMap = 0;
	}

	*nMaxPlayers = svs.maxclients;
}

/*
==================
Host_Speeds

==================
*/
void Host_Speeds( double *time )
{
	float	pass1, pass2, pass3, pass4, pass5;
	double	frameTime;
	double	fps;

	pass1 = ( time[1] - time[0] ) * 1000.0;
	pass2 = ( time[2] - time[1] ) * 1000.0;
	pass3 = ( time[3] - time[2] ) * 1000.0;
	pass4 = ( time[4] - time[3] ) * 1000.0;
	pass5 = ( time[5] - time[4] ) * 1000.0;

	frameTime = pass1 + pass2 + pass3 + pass4 + pass5;
	frameTime /= 1000.0;

	if ( frameTime < 0.0001 )
		fps = 999.0;
	else
		fps = 1.0 / frameTime;

	if ( host_speeds.value )
	{
		int		ent_count;
		int		i;

		ent_count = 0;

		// count used entities
		for ( i = 0; i < sv.num_edicts; i++ )
		{
			if ( !sv.edicts[i].free )
				ent_count++;
		}

		Con_Printf( "%3i fps -- host(%3.0f) sv(%3.0f) cl(%3.0f) gfx(%3.0f) snd(%3.0f) ents(%d)\n", (int)fps, pass1, pass2, pass3, pass4, pass5, ent_count );
	}

	if ( cl_gamegauge.value )
		CL_GGSpeeds( (float)time[3] );
}

/*
==================
Host_UpdateScreen

==================
*/
void Host_UpdateScreen( void )
{
	if ( gfBackground )
		return;

	// Refresh the screen
	SCR_UpdateScreen();

	if ( cl_inmovie && !scr_con_current )
		VID_WriteBuffer( NULL );
}

/*
==================
CL_AddVoiceToDatagram

==================
*/
void CL_AddVoiceToDatagram( qboolean bFinal )
{
	char	uchVoiceData[2048];
	int		nDataLength;
	sizebuf_t	*pBuf;

	if ( cls.state != ca_active || !Voice_IsRecording() )
		return;

	pBuf = &cls.datagram;
	nDataLength = Voice_GetCompressedData( uchVoiceData, sizeof( uchVoiceData ), bFinal );
	if ( !nDataLength )
		return;

	if ( pBuf->cursize + nDataLength + 3 < pBuf->maxsize )
	{
		MSG_WriteByte( pBuf, clc_voicedata );
		MSG_WriteShort( pBuf, (short)nDataLength );
		MSG_WriteBuf( pBuf, nDataLength, uchVoiceData );
	}
}

/*
==================
CL_VoiceIdle

==================
*/
void CL_VoiceIdle( void )
{
	Voice_Idle( (float)host_frametime );
}

/*
==================
Host_UpdateSounds

==================
*/
void Host_UpdateSounds( void )
{
	if ( gfBackground )
		return;

	// update audio
	if ( cls.signon == SIGNONS )
	{
		vec3_t	vSoundForward, vSoundRight, vSoundUp;

		AngleVectors( r_playerViewportAngles, vSoundForward, vSoundRight, vSoundUp );
		S_Update( r_soundOrigin, vSoundForward, vSoundRight, vSoundUp );
	}
	else
	{
		S_Update( vec3_origin, vec3_origin, vec3_origin, vec3_origin );
	}

	S_PrintStats();
}

/*
==================
Host_CheckConnectionFailure

==================
*/
void Host_CheckConnectionFailure( void )
{
	static int	frames = 5;

	if ( cls.state == ca_disconnected && !scr_connectmsg.string[0] && ( ( giSubState & 4 ) || !console.value ) )
	{
		if ( frames-- > 0 )
			return;

		giActive = DLL_PAUSED;
	}

	frames = 5;
}

/*
==================
_Host_Frame

Runs all active servers
==================
*/
void _Host_Frame( float time )
{
	static double	host_times[6];

	if ( setjmp( host_enddemo ) )
		return;			// demo finished.

	IN_UpdateMouse_guess();

	// decide the simulation time
	if ( !Host_FilterTime( time ) )
		return;			// don't run too fast, or packets will flood out

	SystemWrapper_RunFrame( host_frametime );

	if ( g_modfuncs.m_pfnFrameBegin )
		g_modfuncs.m_pfnFrameBegin();

	Host_ComputeFPS( host_frametime );

	R_SetStackBase();

	CL_CheckClientState();

	// process console commands
	Cbuf_Execute();

	ClientDLL_UpdateClientData();

	// if running the server locally, make intentions now
	if ( sv.active )
		CL_Move();

	//-------------------
	//
	// server operations
	//
	//-------------------

	host_times[1] = Sys_FloatTime();

	SV_Frame();

	host_times[2] = Sys_FloatTime();

	SV_CheckForRcon();

	//-------------------
	//
	// client operations
	//
	//-------------------

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
	if ( !sv.active )
		CL_Move();

	ClientDLL_Frame( host_frametime );

	CL_SetLastUpdate();

	// fetch results from server
	CL_ReadPackets();

	CL_CheckWaitForResources_guess();

	// Have client parse any network messages
	CL_RedoPrediction();

	CL_VoiceIdle();

	// Update entity list
	CL_EmitEntities();

	// Resend connection request if needed.
	CL_CheckForResend();

	while ( CL_RequestMissingResources() )
		;

	CL_UpdateSoundFade();

	Host_CheckConnectionFailure();

	CL_HTTPUpdate();

	Steam_ClientRunFrame();

	ClientDLL_CAM_Think();

	CL_MoveSpectatorCamera();

	AdsFrame_guess( cl.worldmodel );

	host_times[3] = Sys_FloatTime();

	// update video
	Host_UpdateScreen();

	host_times[4] = Sys_FloatTime();

	CL_DecayLights();

	Host_UpdateSounds();

	host_times[0] = host_times[5];
	host_times[5] = Sys_FloatTime();

	Host_Speeds( host_times );

	host_framecount++;

	CL_AdjustClock();

	if ( sv_stats.value == 1.0f )
		Host_UpdateStats();

	if ( host_killtime.value && host_killtime.value < sv.time )
		Host_Quit_f();
}

/*
==============================
Host_Frame

==============================
*/
int Host_Frame( float time, int iState, int *stateInfo )
{
	double	time1, time2;

	if ( setjmp( host_abortserver ) )
		return giActive;

	if ( giActive != DLL_CLOSE || !g_iQuitCommandIssued )
		giActive = iState;

	*stateInfo = 0;

	if ( host_profile.value )
		time1 = Sys_FloatTime();

	_Host_Frame( time );

	if ( host_profile.value )
		time2 = Sys_FloatTime();

	if ( giStateInfo )
	{
		*stateInfo = giStateInfo;
		giStateInfo = 0;
		Cbuf_Execute();
	}

	if ( host_profile.value )
	{
		static double	timetotal;
		static int		timecount;
		int		i, c, m;

		timetotal += time2 - time1;
		timecount++;

		if ( timecount < 1000 )
			return giActive;

		m = timetotal * 1000.0 / timecount;
		timecount = 0;
		timetotal = 0;
		c = 0;
		for ( i = 0; i < svs.maxclients; i++ )
		{
			if ( svs.clients[i].active )
				c++;
		}

		Con_Printf( "host_profile: %2i clients %2i msec\n", c, m );
	}

	return giActive;
}

/*
==================
CheckGore

==================
*/
void CheckGore( void )
{
	char	szSubKey[128];
	int		nBufferLen;
	char	szBuffer[128];
	qboolean	bGore;

	bGore = false;

	Q_memset( szBuffer, 0, sizeof( szBuffer ) );

	sprintf( szSubKey, "Software\\Valve\\%s\\Settings", gpszAppName );

	nBufferLen = sizeof( szBuffer ) - 1;
	Q_strcpy( szBuffer, "" );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer, nBufferLen, szBuffer );
	bGore = Q_strlen( szBuffer ) > 0;
	if ( !bGore )
	{
		Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer );
		bGore = Q_strlen( szBuffer ) > 0;
	}

	sprintf( szSubKey, "Software\\Valve\\%s\\%s\\Settings", gpszAppName, com_gamedir );

	Q_strcpy( szBuffer, "" );

	Sys_GetRegKeyValue( szSubKey, "User Token 2", szBuffer, nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
		bGore = true;

	Sys_GetRegKeyValue( szSubKey, "User Token 3", szBuffer, nBufferLen, szBuffer );
	if ( Q_strlen( szBuffer ) > 0 )
		bGore = true;

	if ( !bGore )
		bGore = ISteamApps_BIsLowViolence();

	if ( bGore )
	{
		Cvar_SetValue( "violence_hblood", 0 );
		Cvar_SetValue( "violence_hgibs", 0 );
		Cvar_SetValue( "violence_ablood", 0 );
		Cvar_SetValue( "violence_agibs", 0 );
	}
	else
	{
		Cvar_SetValue( "violence_hblood", 1 );
		Cvar_SetValue( "violence_hgibs", 1 );
		Cvar_SetValue( "violence_ablood", 1 );
		Cvar_SetValue( "violence_agibs", 1 );
	}
}

/*
==================
Host_IsSinglePlayerGame

==================
*/
qboolean Host_IsSinglePlayerGame( void )
{
	if ( !sv.active )
		return cl.maxclients == 1;
	else
		return svs.maxclients == 1;
}

/*
==================
Host_IsServerActive

==================
*/
qboolean Host_IsServerActive( void )
{
	return sv.active;
}

/*
==================
Host_Version

==================
*/
void Host_Version( void )
{
	char	szFileName[MAX_PATH];
	char	*buffer;
	int		bufsize = 0;
	FileHandle_t	fp = NULL;
	char	*pbuf = NULL;
	char	*pstart = NULL;
	int		gotKeys = 0;

	Q_strcpy( gpszVersionString, "1.0.1.4" );
	Q_strcpy( gpszProductString, "cryoffear" );

	sprintf( szFileName, "steam.inf" );

	// Mod's steam.inf is first option, the the steam.inf in the game GCF.
	fp = FS_Open( szFileName, "r" );
	if ( fp )
	{
		bufsize = FS_Size( fp );
		buffer = (char *)Mem_Malloc( bufsize + 1 );
		FS_Read( buffer, bufsize, 1, fp );
		FS_Close( fp );

		buffer[bufsize] = 0;

		pbuf = buffer;
		while ( 1 )
		{
			pbuf = COM_Parse( pbuf );
			if ( !pbuf )
				break;

			if ( Q_strlen( com_token ) <= 0 )
				break;

			if ( gotKeys >= 2 )
				break;

			if ( !Q_strnicmp( com_token, "PatchVersion=", Q_strlen( "PatchVersion=" ) ) )
			{
				Q_strncpy( gpszVersionString, &com_token[ Q_strlen( "PatchVersion=" ) ], sizeof( gpszVersionString ) - 1 );
				gpszVersionString[ sizeof( gpszVersionString ) - 1 ] = 0;

				if ( COM_CheckParm( "-steam" ) )
				{
					char	szSteamVersionId[32];

					FS_GetInterfaceVersion( szSteamVersionId, sizeof( szSteamVersionId ) - 1 );
					_snprintf( gpszVersionString, sizeof( gpszVersionString ), "%s/%s", &com_token[ Q_strlen( "PatchVersion=" ) ], szSteamVersionId );
					gpszVersionString[ sizeof( gpszVersionString ) - 1 ] = 0;
				}

				gotKeys++;
				continue;
			}

			if ( !Q_strnicmp( com_token, "ProductName=", Q_strlen( "ProductName=" ) ) )
			{
				Q_strncpy( gpszProductString, &com_token[ Q_strlen( "ProductName=" ) ], sizeof( gpszProductString ) - 1 );
				gpszProductString[ sizeof( gpszProductString ) - 1 ] = 0;

				gotKeys++;
				continue;
			}
		}

		if ( buffer )
			Mem_Free( buffer );
	}

	if ( cls.state != ca_dedicated )
	{
		Con_DPrintf( "Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_DPrintf( "Exe build: 22:48:21 Apr 10 2013 (%i)\n", build_number() );
	}
	else
	{
		Con_Printf( "Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
		Con_Printf( "Exe build: 22:48:21 Apr 10 2013 (%i)\n", build_number() );
	}
}

const char	*g_InGameAdsAllowed[] =
{
	"cstrike",
	"czero",
	"cstrike_beta",
};

/*
====================
Host_Init
====================
*/
int Host_Init( quakeparms_t *parms )
{
	char	versionString[256];

	srand( time( NULL ) );

	host_parms = *parms;

	com_argc = parms->argc;
	com_argv = parms->argv;

	realtime = 0;

	Memory_Init( parms->membase, parms->memsize );

	Voice_RegisterCvars();

	Cvar_RegisterVariable( &console );
	if ( COM_CheckParm( "-console" ) || COM_CheckParm( "-toconsole" ) || COM_CheckParm( "-dev" ) )
		Cvar_DirectSet( &console, "1.0" );

	Host_InitLocal();

	if ( COM_CheckParm( "-dev" ) )
		Cvar_SetValue( "developer", 1 );

	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	Cvar_CmdInit();
	V_Init();
	Chase_Init();
	COM_Init( parms->basedir );
	Host_ClearSaveDirectory();
	HPAK_Init();
	W_LoadWadFile( "gfx.wad" );
	W_LoadWadFile( "fonts.wad" );
	Key_Init();
	Con_Init();
	Decal_Init();
	Mod_Init();
	NET_Init();
	Netchan_Init();
	DELTA_Init();
	SV_Init();
	SystemWrapper_Init();
	Host_Version();

	_snprintf( versionString, sizeof( versionString ), "%s,%i,%i", gpszVersionString, PROTOCOL_VERSION, build_number() );
	Cvar_Set( "sv_version", versionString );

	Con_DPrintf( "%4.1f Mb heap\n", parms->memsize / ( 1024.0 * 1024.0 ) );

	R_InitTextures();
	HPAK_CheckIntegrity( "custom" );

	Q_memset( &g_module, 0, sizeof( g_module ) );

	if ( cls.state != ca_dedicated )
	{
		int		i;
		char	*disk_basepal;

		disk_basepal = (char *)COM_LoadHunkFile( "gfx/palette.lmp" );
		if ( !disk_basepal )
			Sys_Error( "Host_Init: Couldn't load gfx/palette.lmp" );

		host_basepal = (unsigned short *)Hunk_AllocName( 4 * 256 * sizeof( unsigned short ), "palette.lmp" );
		for ( i = 0; i < 256; i++ )
		{
			host_basepal[i * 4 + 0] = disk_basepal[i * 3 + 2];
			host_basepal[i * 4 + 1] = disk_basepal[i * 3 + 1];
			host_basepal[i * 4 + 2] = disk_basepal[i * 3 + 0];
			host_basepal[i * 4 + 3] = 0;
		}

		GL_Init();
		PM_Init( &g_clmove );
		CL_InitEventSystem();
		ClientDLL_Init();
		VGui_Startup();

		if ( !VID_Init( host_basepal ) )
		{
			VGui_Shutdown();
			return 0;
		}

		Draw_Init();
		SCR_Init();
		R_Init();
		S_Init();
		S_Init();
		CDAudio_Init();
		Voice_Init( "voice_miles", 1 );
		DemoPlayer_Init();
		CL_Init();

		{
			int		i;

			for ( i = 0; i < ARRAYSIZE( g_InGameAdsAllowed ); i++ )
			{
				if ( !Q_stricmp( com_gamedir, g_InGameAdsAllowed[i] ) )
				{
					g_bUsingInGameAdvertisements = true;
					break;
				}
			}
		}

		AdsStartup_guess();
	}
	else
	{
		Cvar_RegisterVariable( &suitvolume );
	}

	Cbuf_InsertText( "exec valve.rc\n" );

	if ( cls.state != ca_dedicated )
		GL_Config();

	Hunk_AllocName( 0, "-HOST_HUNKLEVEL-" );
	host_hunklevel = Hunk_LowMark();

	giActive = DLL_ACTIVE;
	scr_skipupdate = false;

	CheckGore();

	host_initialized = true;
	return 1;
}

/*
===============
Host_Shutdown

FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown( void )
{
	static qboolean	isdown = false;
	int		i;
	client_t	*pclient;

	if ( isdown )
	{
		printf( "recursive shutdown\n" );
		return;
	}
	isdown = true;

	// keep Con_Printf from trying to update the screen
	if ( host_initialized )
		Host_WriteConfiguration();

	SV_ServerShutdown();
	Voice_Deinit();

	host_initialized = false;

	if ( cls.state != ca_dedicated )
		AdsShutdown_guess();

	CDAudio_Shutdown();
	VGui_Shutdown();

	if ( cls.state != ca_dedicated )
		ClientDLL_Shutdown();

	Cmd_RemoveGameCmds();
	Cmd_Shutdown();
	Cvar_Shutdown();

	HPAK_FlushHostQueue();
	SV_DeallocateDynamicData();

	for ( i = 0, pclient = svs.clients; i < svs.maxclientslimit; i++, pclient++ )
		SV_ClearFrames( &pclient->frames );

	SV_Shutdown();
	SystemWrapper_ShutDown();
	NET_Shutdown();
	S_Shutdown();
	Con_Shutdown();
	ReleaseEntityDlls();
	CL_ShutDownClientStatic();
	CM_FreePAS();

	if ( wadpath )
	{
		Mem_Free( wadpath );
		wadpath = NULL;
	}

	if ( cls.state != ca_dedicated )
		Draw_Shutdown();

	Draw_DecalShutdown();
	W_Shutdown();

	Log_Printf( "Server shutdown\n" );
	Log_Close();

	COM_Shutdown();
	CL_Shutdown();
	DELTA_Shutdown();
	Key_Shutdown();

	realtime = 0;
	sv.time = 0;
	cl.time = 0;
}
