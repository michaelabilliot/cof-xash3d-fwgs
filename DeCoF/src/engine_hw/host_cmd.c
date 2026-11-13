// host_cmd.c -- console commands of the host, save and restore of games

#include <io.h>
#include <math.h>
#include <ctype.h>
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
#include "world.h"
#include "decal.h"
#include "hashpak.h"
#include "sv_log.h"
#include "pmove.h"
#include "cdll_int.h"
#include "vgui_int.h"
#include "eiface.h"
#include "usermsg.h"
#include "studio.h"
#include "r_studioint.h"
#include "dll_state.h"

typedef struct
{
	char	mapName[32];
	char	comment[80];
	int		mapCount;
} GAME_HEADER;

typedef struct
{
	int		saveId;
	int		version;
	int		skillLevel;
	int		entityCount;
	int		connectionCount;
	int		lightStyleCount;
	float	time;
	char	mapName[32];
	char	skyName[32];
	int		skyColor_r;
	int		skyColor_g;
	int		skyColor_b;
	float	skyVec_x;
	float	skyVec_y;
	float	skyVec_z;
} SAVE_HEADER;

typedef struct
{
	int		index;
	char	style[64];
} SAVELIGHTSTYLE;

typedef struct
{
	char	*pBSPName;
	char	*pTitleName;
} TITLECOMMENT;

#define SAVEFILE_HEADER		0x564C4156		// little-endian "VALV"
#define SAVEGAME_HEADER		0x5641534A		// little-endian "JSAV"
#define SAVEGAME_VERSION	0x0071			// Version 0.71

typedef void (*SV_SAVEGAMECOMMENT_FUNC)( char *, int );
typedef int (*SV_BLENDING_INTERFACE_FUNC)( int, struct sv_blending_interface_s **, struct server_studio_api_s *, float *, float * );

// gl_rsurf.c
int		DecalListCreate( DECALLIST *pList );

// pr_dlls.h
typedef struct
{
	unsigned int	pFunction;
	char	*pFunctionName;
} functiontable_t;

typedef struct extensiondll_s
{
	void	*lDLLHandle;
	functiontable_t	*functionTable;
	int		functionCount;
} extensiondll_t;

typedef void (*ENTITYINIT)( struct entvars_s * );

// pr_dlls.c
extern	extensiondll_t	g_rgextdll[50];
extern	int		g_iextdllMac;
void	LoadEntityDLLs( char *szBaseDir );
ENTITYINIT	GetEntityInit( char *pClassName );

// pr_cmds.c
void	PF_MessageBegin_I( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
void	PF_MessageEnd_I( void );
void	PF_WriteByte_I( int iValue );
void	PF_WriteString_I( const char *sz );

// r_studio.c
extern	int		r_dointerp;
extern	struct sv_blending_interface_s	*g_pSvBlendingAPI;
extern	struct server_studio_api_s	server_studio_api;
extern	float	rotationmatrix[3][4];
extern	float	bonetransform[MAXSTUDIOBONES][3][4];
void	R_ResetSvBlending( void );

// model.c
void	*Mod_Extradata( model_t *mod );
void	Mod_Print( void );

// cd.cpp
void	CD_Command_f( void );
void	MP3_Command_f( void );

// snd_dma.c
void	CareerAudio_Command_f( void );

// voice.cpp
void	Voice_Deinit( void );
qboolean	Voice_IsRecording( void );
qboolean	Voice_RecordStart( const char *pUncompressedFile, const char *pDecompressedFile, const char *pMicInputFile );
qboolean	Voice_RecordStop( void );

// master.c
void	Master_SetMaster_f( void );

// sys_dll2.cpp (names unproven: both store the choice in the registry for the next start)
void	VideoMode_SwitchMode( int hardware, int windowed );
void	VideoMode_SetVideoMode( int width, int height, int bpp );

// pr_edict.c
edict_t	*CreateNamedEntity( int className );

// sv_main.c
extern	cvar_t	sv_newunit;

// pr_cmds.c
qboolean	PF_IsDedicatedServer( void );
int		PF_IsMapValid_I( char *mapname );

// sys_dll.c
void	Sys_Quit( void );

// gl_screen.c
extern	qboolean	scr_skipupdate;
void	SCR_BeginLoadingPlaque( qboolean reconnect );
void	SCR_UpdateScreen( void );

// FileSystem.cpp
void	FS_LogLevelLoadStarted( const char *name );
int		FS_Write( const void *pInput, int size, int count, FileHandle_t file );
void	FS_RemoveFile( const char *pRelativePath, const char *pathID );
void	FS_CreateDirHierarchy( const char *path, const char *pathID );

// vgui_int.c
void	VGuiWrap2_LoadingStarted( const char *resourceType, const char *resourceName );

// sound
void	SetCareerAudioState( int state );

// steam_api flat interface
__declspec( dllimport ) unsigned int	ISteamUtils_GetAppID( void );

void	Host_SavegameComment( char *pszBuffer, int iSizeBuffer );
void	CL_Save( const char *name );
void	EntityPatchWrite( SAVERESTOREDATA *pSaveData, const char *level );
SAVERESTOREDATA	*SaveGamestate( void );
int		DirectoryCount( const char *pPath );
void	DirectoryCopy( const char *pPath, FileHandle_t pFile );
void	DirectoryExtract( FileHandle_t pFile, int fileCount );
void	Host_ClearGameState( void );
int		LoadGamestate( char *level, int createPlayers );
void	SV_CheckBlendingInterface( void );
void	SV_CheckSaveGameCommentInterface( void );

double		cpuPercent;
int			startTime;
int			current_skill;
CareerStateType	g_careerState;
int			gHostSpawnCount = 0;
qboolean	noclip_anglehack = false;

SV_SAVEGAMECOMMENT_FUNC	g_pSaveGameCommentFunc = Host_SavegameComment;

qboolean	g_bSaveAllowed_guess = false;	// set by the "savehack" commands of cd.cpp
qboolean	g_bMajorMapChange = false;

cvar_t	voice_recordtofile = { "voice_recordtofile", "0" };
cvar_t	voice_inputfromfile = { "voice_inputfromfile", "0" };

TYPEDESCRIPTION	gGameHeaderDescription[] =
{
	DEFINE_FIELD( GAME_HEADER, mapCount, FIELD_INTEGER ),
	DEFINE_ARRAY( GAME_HEADER, mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( GAME_HEADER, comment, FIELD_CHARACTER, 80 ),
};

TYPEDESCRIPTION	gSaveHeaderDescription[] =
{
	DEFINE_FIELD( SAVE_HEADER, skillLevel, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, entityCount, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, connectionCount, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, lightStyleCount, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, time, FIELD_TIME ),
	DEFINE_ARRAY( SAVE_HEADER, mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( SAVE_HEADER, skyName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( SAVE_HEADER, skyColor_r, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, skyColor_g, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, skyColor_b, FIELD_INTEGER ),
	DEFINE_FIELD( SAVE_HEADER, skyVec_x, FIELD_FLOAT ),
	DEFINE_FIELD( SAVE_HEADER, skyVec_y, FIELD_FLOAT ),
	DEFINE_FIELD( SAVE_HEADER, skyVec_z, FIELD_FLOAT ),
};

TYPEDESCRIPTION	gAdjacencyDescription[] =
{
	DEFINE_ARRAY( LEVELLIST, mapName, FIELD_CHARACTER, 32 ),
	DEFINE_ARRAY( LEVELLIST, landmarkName, FIELD_CHARACTER, 32 ),
	DEFINE_FIELD( LEVELLIST, pentLandmark, FIELD_EDICT ),
	DEFINE_FIELD( LEVELLIST, vecLandmarkOrigin, FIELD_VECTOR ),
};

TYPEDESCRIPTION	gEntityTableDescription[] =
{
	DEFINE_FIELD( ENTITYTABLE, id, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, location, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, size, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, flags, FIELD_INTEGER ),
	DEFINE_FIELD( ENTITYTABLE, classname, FIELD_STRING ),
};

TYPEDESCRIPTION	gLightstyleDescription[] =
{
	DEFINE_FIELD( SAVELIGHTSTYLE, index, FIELD_INTEGER ),
	DEFINE_ARRAY( SAVELIGHTSTYLE, style, FIELD_CHARACTER, 64 ),
};

cvar_t	gHostMap = { "HostMap", "C1A0" };

/*
==================
SV_GetPlayerHulls

==================
*/
void SV_GetPlayerHulls( void )
{
	int		i;
	int		iResult;

	for ( i = 0; i < 4; i++ )
	{
		iResult = gEntityInterface.pfnGetHullBounds( i, player_mins[i], player_maxs[i] );
		if ( !iResult )
			break;
	}
}

/*
==================
Host_InitializeGameDLL

==================
*/
void Host_InitializeGameDLL( void )
{
	char	szInsecure[10] = { '-', 'i', 'n', 's', 'e', 'c', 'u', 'r', 'e', '\0' };

	Cbuf_Execute();
	NET_Config( svs.maxclients > 1 );

	if ( svs.dll_initialized )
	{
		Con_DPrintf( "Sys_InitializeGameDLL called twice, skipping second call\n" );
		return;
	}

	svs.dll_initialized = true;
	LoadEntityDLLs( host_parms.basedir );

	gEntityInterface.pfnGameInit();
	gEntityInterface.pfnPM_Init( &g_svmove );
	gEntityInterface.pfnRegisterEncoders();

	SV_InitEncoders();
	SV_GetPlayerHulls();
	SV_CheckBlendingInterface();
	SV_CheckSaveGameCommentInterface();

	Cbuf_Execute();
}

/*
==================
Host_Motd_f

==================
*/
void Host_Motd_f( void )
{
	int		length;
	FileHandle_t	pFile;

	if ( motdfile.string[0] == '/' || Q_strstr( motdfile.string, ":" ) || Q_strstr( motdfile.string, ".." ) || Q_strstr( motdfile.string, "\\" ) )
	{
		Con_Printf( "Unable to open %s (contains illegal characters)\n", motdfile.string );
		return;
	}

	pFile = FS_Open( motdfile.string, "rb" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open %s\n", motdfile.string );
		return;
	}

	length = FS_Size( pFile );
	if ( length )
	{
		char	*buf;
		char	*now;
		char	*next;

		buf = (char *)malloc( length + 1 );
		if ( buf )
		{
			FS_Read( buf, length, 1, pFile );
			buf[length] = 0;

			Con_Printf( "motd:" );

			now = buf;
			while ( strchr( now, '\n' ) )
			{
				next = strchr( now, '\n' );
				*next++ = 0;
				Con_Printf( "%s\n", now );
				now = next;
			}

			if ( now )
				Con_Printf( "%s\n", now );

			free( buf );
		}
	}

	FS_Close( pFile );
}

/*
==================
Host_Motd_Write_f

==================
*/
void Host_Motd_Write_f( void )
{
	char	newFile[2048];
	unsigned int	i;
	FileHandle_t	pFile;

	if ( !sv.active )
		return;

	if ( cmd_source != src_command )
		return;

	if ( cls.state == ca_active )
		return;

	if ( cls.state != ca_dedicated )
		return;

	if ( IsSafeFileToDownload( motdfile.string ) )
	{
		pFile = FS_Open( motdfile.string, "wb+" );
		if ( !pFile )
		{
			Con_Printf( "Unable to open %s\n", motdfile.string );
			return;
		}

		Q_strncpy( newFile, Cmd_Args(), sizeof( newFile ) );

		for ( i = 0; i < strlen( newFile ); i++ )
		{
			if ( newFile[i] == '\\' && newFile[i + 1] == 'n' )
			{
				newFile[i] = '\n';
				strcpy( &newFile[i + 1], &newFile[i + 2] );
			}
		}

		FS_Write( newFile, strlen( newFile ), 1, pFile );
		FS_Close( pFile );

		Con_Printf( "Done.\n" );
	}
	else
	{
		Con_Printf( "Invalid motdfile name (%s)\n", motdfile.string );
	}
}

/*
==================
Host_GetStartTime

==================
*/
int Host_GetStartTime( void )
{
	return startTime;
}

/*
==================
Host_UpdateStats

==================
*/
void Host_UpdateStats( void )
{
	static float	last = 0;
	static float	lastAvg = 0;
	static __int64	lastcputicks = 0;
	static __int64	lastrunticks = 0;
	FILETIME	creationTime, exitTime, kernelTime, userTime, nowTime;
	__int64		totalTime, now;
	HANDLE		hProcess;

	if ( !startTime )
		startTime = Sys_FloatTime();

	if ( Sys_FloatTime() > last + 1.0f )
	{
		hProcess = GetCurrentProcess();
		GetProcessTimes( hProcess, &creationTime, &exitTime, &kernelTime, &userTime );
		GetSystemTimeAsFileTime( &nowTime );

		if ( !lastrunticks )
			memcpy( &lastrunticks, &creationTime, sizeof( __int64 ) );

		memcpy( &totalTime, &userTime, sizeof( __int64 ) );
		memcpy( &now, &kernelTime, sizeof( __int64 ) );
		totalTime += now;

		memcpy( &now, &nowTime, sizeof( __int64 ) );

		cpuPercent = (double)( totalTime - lastcputicks ) / (double)( now - lastrunticks );

		if ( Sys_FloatTime() > lastAvg + 5.0f )
		{
			memcpy( &lastrunticks, &nowTime, sizeof( __int64 ) );
			memcpy( &lastcputicks, &totalTime, sizeof( __int64 ) );
			lastAvg = last;
		}

		last = Sys_FloatTime();
	}
}

/*
==================
GetStatsString

==================
*/
void GetStatsString( char *buf, int bufSize )
{
	float	avgIn = 0;
	float	avgOut = 0;
	int		i;
	int		players = 0;

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( !host_client->active && !host_client->connected && !host_client->spawned ) || host_client->fakeclient )
			continue;

		avgIn += host_client->netchan.flow[FLOW_INCOMING].avgkbytespersec;
		avgOut += host_client->netchan.flow[FLOW_OUTGOING].avgkbytespersec;
		players++;
	}

	_snprintf( buf, bufSize - 1, "%5.2f %5.2f %5.2f %7i %5i %7.2f %7i", cpuPercent * 100.0, avgIn, avgOut, (int)( Sys_FloatTime() - startTime ) / 60, g_userid - 1, 1.0 / host_frametime, players );
	buf[bufSize - 1] = 0;
}

/*
==================
Host_Stats_f

==================
*/
void Host_Stats_f( void )
{
	char	stats[512];

	GetStatsString( stats, sizeof( stats ) );
	Con_Printf( "CPU   In    Out   Uptime  Users   FPS    Players\n%s\n", stats );
}

int		g_iQuitCommandIssued = 0;

/*
==================
Host_Quit_f

==================
*/
void Host_Quit_f( void )
{
	if ( Cmd_Argc() == 1 )
	{
		giActive = DLL_CLOSE;
		g_iQuitCommandIssued = 1;

		if ( cls.state != ca_dedicated )
			CL_Disconnect();

		Host_ShutdownServer( false );

		Sys_Quit();
	}
	else
	{
		giActive = DLL_PAUSED;
		giStateInfo = DLL_QUIT;
	}
}

char	*g_pPostRestartCmdLineArgs = NULL;

/*
==================
Host_Quit_Restart_f

==================
*/
void Host_Quit_Restart_f( void )
{
	giActive = 5;		// DLL_RESTART of this engine build
	giStateInfo = DLL_QUIT;

	if ( !sv.active && cls.state == ca_active && cls.trueaddress[0] && g_pPostRestartCmdLineArgs )
	{
		strcat( g_pPostRestartCmdLineArgs, " +connect " );
		strcat( g_pPostRestartCmdLineArgs, cls.servername );
	}

	if ( sv.active && svs.maxclients == 1 && cls.state == ca_active && g_pPostRestartCmdLineArgs )
	{
		g_bSaveAllowed_guess = true;
		Cbuf_AddText( "save quick\n" );
		Cbuf_Execute();
		strcat( g_pPostRestartCmdLineArgs, " +load quick" );
		g_bSaveAllowed_guess = false;
	}
}

/*
==================
Host_Status_Printf

==================
*/
void Host_Status_Printf( qboolean conprint, qboolean log, char *fmt, ... )
{
	char	string[4096];
	va_list	argptr;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	if ( conprint )
		Con_Printf( "%s", string );
	else
		SV_ClientPrintf( "%s", string );

	if ( log )
	{
		char	szfile[MAX_PATH];

		_snprintf( szfile, sizeof( szfile ), "%s", "status.log" );
		COM_Log( szfile, "%s", string );
	}
}

/*
==================
Host_Status_f

==================
*/
void Host_Status_f( void )
{
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hltv_slots = 0;
	int			hltv_specs = 0;
	int			hltv_delay = 0;
	char		*val;
	int			hours = 0;
	int			j;
	int			nClients;
	qboolean	log = false;
	qboolean	conprint = true;
	qboolean	bIsSecure = false;
	qboolean	bWantsToBeSecure = false;
	qboolean	bConnectedToSteam3 = false;
	const char	*pchConnectionString = "";
	const char	*pchSteamUniverse = "";

	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else
	{
		conprint = false;
	}

	if ( Cmd_Argc() == 2 && !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
	{
		char	szfile[MAX_PATH];

		_snprintf( szfile, sizeof( szfile ), "%s", "status.log" );
		_unlink( szfile );
		log = true;
	}

	Host_Status_Printf( conprint, log, "hostname:  %s\n", Cvar_VariableString( "hostname" ) );

	bIsSecure = Steam_GSBSecure();
	bWantsToBeSecure = Steam_GSBSecurePreference();
	bConnectedToSteam3 = Steam_GSBLoggedOn();

	if ( !bIsSecure && bWantsToBeSecure )
	{
		if ( bConnectedToSteam3 )
			pchConnectionString = "(secure mode enabled, connected to Steam3)";
		else
			pchConnectionString = "(secure mode enabled, disconnected from Steam3)";
	}

	if ( sv.active )
		pchSteamUniverse = Steam_GetGSUniverse();

	Host_Status_Printf( conprint, log, "version :  %i/%s %d %s %s%s (%d)\n", PROTOCOL_VERSION, gpszVersionString, build_number(), bIsSecure ? "secure" : "insecure", pchConnectionString, pchSteamUniverse, ISteamUtils_GetAppID() );

	if ( !noip )
		Host_Status_Printf( conprint, log, "tcp/ip  :  %s\n", NET_AdrToString( net_local_adr ) );

	if ( !noipx )
		Host_Status_Printf( conprint, log, "ipx     :  %s\n", NET_AdrToString( net_local_ipx_adr ) );

	Host_Status_Printf( conprint, log, "map     :  %s at: %d x, %d y, %d z\n", sv.name, (int)r_origin[0], (int)r_origin[1], (int)r_origin[2] );

	SV_CountPlayers( &nClients );
	Host_Status_Printf( conprint, log, "players :  %i active (%i max)\n\n", nClients, svs.maxclients );

	Host_Status_Printf( conprint, log, "#      name userid uniqueid frag time ping loss adr\n" );

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		hours = 0;
		seconds = realtime - client->netchan.connect_time;
		minutes = seconds / 60;
		if ( minutes )
		{
			seconds -= minutes * 60;
			hours = minutes / 60;
			if ( hours )
				minutes -= hours * 60;
		}

		Host_Status_Printf( conprint, log, "#%2i %8s %i %s", j + 1, va( "\"%s\"", client->name ), client->userid, client->fakeclient ? "BOT" : SV_GetClientIDString( client ) );

		if ( client->proxy )
		{
			val = Info_ValueForKey( client->userinfo, "hspecs" );
			if ( Q_strlen( val ) )
				hltv_specs = Q_atoi( val );

			val = Info_ValueForKey( client->userinfo, "hslots" );
			if ( Q_strlen( val ) )
				hltv_slots = Q_atoi( val );

			val = Info_ValueForKey( client->userinfo, "hdelay" );
			if ( Q_strlen( val ) )
				hltv_delay = Q_atoi( val );

			Host_Status_Printf( conprint, log, " hltv:%u/%u delay:%u", hltv_specs, hltv_slots, hltv_delay );
		}
		else
		{
			Host_Status_Printf( conprint, log, " %3i", (int)client->edict->v.frags );
		}

		if ( hours )
			Host_Status_Printf( conprint, log, " %2i:%02i:%02i", hours, minutes, seconds );
		else
			Host_Status_Printf( conprint, log, " %02i:%02i", minutes, seconds );

		if ( !client->proxy )
			Host_Status_Printf( conprint, log, " %4i  %3i", SV_CalcPing( client ), (int)client->packet_loss );

		if ( ( conprint || client->proxy ) && client->netchan.remote_address.type == NA_IP )
			Host_Status_Printf( conprint, log, " %s\n", NET_AdrToString( client->netchan.remote_address ) );
		else
			Host_Status_Printf( conprint, log, "\n" );
	}

	Host_Status_Printf( conprint, log, "%i users\n", nClients );
}

/*
==================
Host_Status_Formatted_f

==================
*/
void Host_Status_Formatted_f( void )
{
	client_t	*client;
	int			seconds;
	int			minutes;
	int			hours = 0;
	int			j;
	int			nClients;
	qboolean	log = false;
	qboolean	conprint = true;
	qboolean	bIsSecure = false;
	char		sz[32];

	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else
	{
		conprint = false;
	}

	if ( Cmd_Argc() == 2 && !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
	{
		char	szfile[MAX_PATH];

		_snprintf( szfile, sizeof( szfile ), "%s", "status.log" );
		_unlink( szfile );
		log = true;
	}

	bIsSecure = Steam_GSBSecure();

	Host_Status_Printf( conprint, log, "hostname:  %s\n", Cvar_VariableString( "hostname" ) );
	Host_Status_Printf( conprint, log, "version :  %i/%s %d %s\n", PROTOCOL_VERSION, gpszVersionString, build_number(), bIsSecure ? "secure" : "insecure" );

	if ( !noip )
		Host_Status_Printf( conprint, log, "tcp/ip  :  %s\n", NET_AdrToString( net_local_adr ) );

	if ( !noipx )
		Host_Status_Printf( conprint, log, "ipx     :  %s\n", NET_AdrToString( net_local_ipx_adr ) );

	Host_Status_Printf( conprint, log, "map     :  %s at: %d x, %d y, %d z\n", sv.name, (int)r_origin[0], (int)r_origin[1], (int)r_origin[2] );

	SV_CountPlayers( &nClients );
	Host_Status_Printf( conprint, log, "players :  %i active (%i max)\n\n", nClients, svs.maxclients );

	Host_Status_Printf( conprint, log, "%-2.2s\t%-9.9s\t%-7.7s\t%-20.20s\t%-4.4s\t%-8.8s\t%-4.4s\t%-4.4s\t%-21.21s\n", "# ", "name    ", "userid   ", "uniqueid ", "frag", "time    ", "ping", "loss", "adr" );

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		hours = 0;
		seconds = realtime - client->netchan.connect_time;
		minutes = seconds / 60;
		if ( minutes )
		{
			seconds -= minutes * 60;
			hours = minutes / 60;
			if ( hours )
				minutes -= hours * 60;
		}

		if ( hours )
			_snprintf( sz, sizeof( sz ), "%-2i:%02i:%02i", hours, minutes, seconds );
		else
			_snprintf( sz, sizeof( sz ), "%02i:%02i", minutes, seconds );

		Host_Status_Printf( conprint, log, "%-2.2s\t%-9.9s\t%-7.7s\t%-20.20s\t%-4.4s\t%-8.8s\t%-4.4s\t%-4.4s\t%-21.21s\n",
			va( "%-2i", j + 1 ),
			va( "\"%s\"", client->name ),
			va( "%-7i", client->userid ),
			SV_GetClientIDString( client ),
			va( "%-4i", (int)client->edict->v.frags ),
			sz,
			va( "%-4i", SV_CalcPing( client ) ),
			va( "%-4i", (int)client->packet_loss ),
			conprint ? NET_AdrToString( client->netchan.remote_address ) : "" );
	}

	Host_Status_Printf( conprint, log, "%i users\n", nClients );
}

/*
==================
Host_God_f

Sets client to godmode
==================
*/
void Host_God_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	if ( !allow_cheats )
		return;

	sv_player->v.flags = sv_player->v.flags ^ FL_GODMODE;
	if ( !( sv_player->v.flags & FL_GODMODE ) )
		SV_ClientPrintf( "godmode OFF\n" );
	else
		SV_ClientPrintf( "godmode ON\n" );
}

/*
==================
Host_Notarget_f

==================
*/
void Host_Notarget_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	sv_player->v.flags = sv_player->v.flags ^ FL_NOTARGET;
	if ( !( sv_player->v.flags & FL_NOTARGET ) )
		SV_ClientPrintf( "notarget OFF\n" );
	else
		SV_ClientPrintf( "notarget ON\n" );
}

/*
==================
FindPassableSpace

==================
*/
int FindPassableSpace( edict_t *pEdict, vec_t *direction, float step )
{
	int		i;

	for ( i = 0; i < 100; i++ )
	{
		VectorMA( pEdict->v.origin, step, direction, pEdict->v.origin );

		if ( !SV_TestEntityPosition( pEdict ) )
		{
			// Store old origin
			VectorCopy( pEdict->v.origin, pEdict->v.oldorigin );
			return 1;
		}
	}

	return 0;
}

/*
==================
Host_Noclip_f

==================
*/
void Host_Noclip_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	if ( !allow_cheats )
		return;

	if ( sv_player->v.movetype != MOVETYPE_NOCLIP )
	{
		noclip_anglehack = true;
		sv_player->v.movetype = MOVETYPE_NOCLIP;
		SV_ClientPrintf( "noclip ON\n" );
	}
	else
	{
		noclip_anglehack = false;
		sv_player->v.movetype = MOVETYPE_WALK;
		VectorCopy( sv_player->v.origin, sv_player->v.oldorigin );
		SV_ClientPrintf( "noclip OFF\n" );

		if ( SV_TestEntityPosition( sv_player ) )
		{
			vec3_t	forward, right, up;

			AngleVectors( sv_player->v.v_angle, forward, right, up );

			if ( !FindPassableSpace( sv_player, forward, 1.0 )
				&& !FindPassableSpace( sv_player, right, 1.0 )
				&& !FindPassableSpace( sv_player, right, -1.0 )
				&& !FindPassableSpace( sv_player, up, 1.0 )
				&& !FindPassableSpace( sv_player, up, -1.0 )
				&& !FindPassableSpace( sv_player, forward, -1.0 ) )
			{
				Con_DPrintf( "Can't find the world\n" );
			}

			VectorCopy( sv_player->v.oldorigin, sv_player->v.origin );
		}
	}
}

/*
==================
Host_Fly_f

Sets client to flymode
==================
*/
void Host_Fly_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( gGlobalVariables.deathmatch )
		return;

	if ( sv_player->v.movetype != MOVETYPE_FLY )
	{
		sv_player->v.movetype = MOVETYPE_FLY;
		SV_ClientPrintf( "flymode ON\n" );
	}
	else
	{
		sv_player->v.movetype = MOVETYPE_WALK;
		SV_ClientPrintf( "flymode OFF\n" );
	}
}

/*
==================
Host_Ping_f

==================
*/
void Host_Ping_f( void )
{
	int		i;
	client_t	*client;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	SV_ClientPrintf( "Client ping times:\n" );

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active )
			continue;

		SV_ClientPrintf( "%4i %s\n", SV_CalcPing( client ), client->name );
	}
}

/*
======================
Host_Map

handle a
map <servername>
command from the console.  Active clients are kicked off.
======================
*/
void Host_Map( qboolean bIsDemo, char *mapstring, char *mapName, qboolean loadGame )
{
	Host_ShutdownServer( false );

	key_dest = key_game;	// remove console or menu
	SCR_BeginLoadingPlaque( false );

	if ( !loadGame )
	{
		Host_ClearGameState();
		SV_InactivateClients();
		svs.serverflags = 0;	// haven't completed an episode yet
	}

	Q_strncpy( cls.mapstring, mapstring, sizeof( cls.mapstring ) - 1 );
	cls.mapstring[ sizeof( cls.mapstring ) - 1 ] = 0;

	if ( !SV_SpawnServer( bIsDemo, mapName, NULL ) )
		return;

	ContinueLoadingProgressBar( "Server", 7, 0.0 );

	if ( loadGame )
	{
		if ( !LoadGamestate( mapName, 1 ) )
			SV_LoadEntities();

		sv.paused = true;		// pause until all clients connect
		sv.loadgame = true;
		SV_ActivateServer( 0 );
	}
	else
	{
		SV_LoadEntities();
		SV_ActivateServer( 1 );

		if ( !sv.active )
			return;

		if ( cls.state != ca_dedicated )
		{
			int		i;

			Q_strcpy( cls.spawnparms, "" );
			for ( i = 2; i < Cmd_Argc(); i++ )
			{
				strncat( cls.spawnparms, Cmd_Argv( i ), sizeof( cls.spawnparms ) - strlen( cls.spawnparms ) - 1 );
				strncat( cls.spawnparms, " ", sizeof( cls.spawnparms ) - strlen( cls.spawnparms ) - 1 );
			}
		}
	}

	if ( sv_gpNewUserMsgs )
	{
		UserMsg	*pMsg;

		if ( !sv_gpUserMsgs )
		{
			sv_gpUserMsgs = sv_gpNewUserMsgs;
		}
		else
		{
			pMsg = sv_gpUserMsgs;
			while ( pMsg->next )
				pMsg = pMsg->next;
			pMsg->next = sv_gpNewUserMsgs;
		}

		sv_gpNewUserMsgs = NULL;
	}

	if ( cls.state != ca_dedicated )
		Cmd_ExecuteString( "connect local", src_command );
}

/*
==================
Host_Map_f

Goes to a new map, taking all clients along
==================
*/
void Host_Map_f( void )
{
	int		i;
	char	mapstring[64];
	char	name[64];
	CareerStateType	careerState = g_careerState;

	if ( cmd_source != src_command )
	{
		g_careerState = CAREER_NONE;
		return;
	}

	if ( Cmd_Argc() > 1 )
	{
		if ( Q_strlen( Cmd_Args() ) > 54 )
		{
			Con_Printf( "map change failed: command string is too long.\n" );
			g_careerState = CAREER_NONE;
			return;
		}
	}

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "map <levelname> : changes server to specified map\n" );
		g_careerState = CAREER_NONE;
		return;
	}

	CL_Disconnect();

	if ( careerState == CAREER_LOADING )
		g_careerState = CAREER_LOADING;

	if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
		g_bMajorMapChange = true;

	FS_LogLevelLoadStarted( "Map_Common" );

	mapstring[0] = 0;
	for ( i = 0; i < Cmd_Argc(); i++ )
	{
		strncat( mapstring, Cmd_Argv( i ), sizeof( mapstring ) - strlen( mapstring ) - 2 );
		strncat( mapstring, " ", sizeof( mapstring ) - strlen( mapstring ) - 2 );
	}
	Q_strcat( mapstring, "\n" );

	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 1 );
	name[ sizeof( name ) - 1 ] = 0;

	if ( !svs.dll_initialized )
		Host_InitializeGameDLL();

	i = Q_strlen( name );
	if ( i > 4 && !Q_strcasecmp( &name[i - 4], ".bsp" ) )
		name[i - 4] = 0;

	FS_LogLevelLoadStarted( name );
	VGuiWrap2_LoadingStarted( "level", name );
	SCR_UpdateScreen();
	SCR_UpdateScreen();

	if ( !PF_IsMapValid_I( name ) )
	{
		Con_Printf( "map change failed: '%s' not found on server.\n", name );
		if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
		{
			g_bMajorMapChange = false;
			Sys_Printf( "\n" );
		}
		return;
	}

	StartLoadingProgressBar( "Server", 24 );
	SetLoadingProgressBarStatusText( "#GameUI_StartingServer" );
	ContinueLoadingProgressBar( "Server", 1, 0.0 );

	Cvar_Set( "HostMap", name );
	Host_Map( false, mapstring, name, false );

	if ( COM_CheckParm( "-steam" ) && PF_IsDedicatedServer() )
	{
		g_bMajorMapChange = false;
		Sys_Printf( "\n" );
	}

	ContinueLoadingProgressBar( "Server", 11, 0.0 );
	NotifyDedicatedServerUI( "UpdateMap" );

	if ( g_careerState == CAREER_LOADING )
	{
		g_careerState = CAREER_PLAYING;
		SetCareerAudioState( 1 );
	}
	else
	{
		SetCareerAudioState( 0 );
	}
}

/*
==================
Host_Career_f

==================
*/
void Host_Career_f( void )
{
	if ( cmd_source != src_command )
		return;

	g_careerState = CAREER_LOADING;
	Host_Map_f();
}

/*
==================
Host_Maps_f

Lists the maps on the server
==================
*/
void Host_Maps_f( void )
{
	char	*pszSubString = NULL;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  maps <substring>\nmaps * for full listing\n" );
		return;
	}

	pszSubString = Cmd_Argv( 1 );
	if ( !pszSubString || !*pszSubString )
		return;

	if ( *pszSubString == '*' )
		pszSubString = NULL;

	COM_ListMaps( pszSubString );
}

/*
==================
Host_Changelevel_f

Goes to a new map, taking all clients along
==================
*/
void Host_Changelevel_f( void )
{
	char	level[64];
	char	_startspot[64];
	char	*startspot;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "changelevel <levelname> : continue game on a new level\n" );
		return;
	}

	if ( !sv.active || cls.demoplayback )
	{
		Con_Printf( "Only the server may changelevel\n" );
		return;
	}

	if ( !PF_IsMapValid_I( Cmd_Argv( 1 ) ) )
	{
		Con_Printf( "changelevel failed: '%s' not found on server.\n", Cmd_Argv( 1 ) );
		return;
	}

	SCR_BeginLoadingPlaque( true );
	S_StopAllSounds( true );

	Q_strncpy( level, Cmd_Argv( 1 ), sizeof( level ) - 1 );
	level[ sizeof( level ) - 1 ] = 0;

	if ( Cmd_Argc() == 2 )
	{
		startspot = NULL;
	}
	else
	{
		Q_strncpy( _startspot, Cmd_Argv( 2 ), sizeof( _startspot ) - 1 );
		_startspot[ sizeof( _startspot ) - 1 ] = 0;
		startspot = _startspot;
	}

	SV_InactivateClients();
	SV_ServerShutdown();
	SV_SpawnServer( false, level, startspot );
	SV_LoadEntities();
	SV_ActivateServer( 1 );
}

/*
==================
Host_FindRecentSave

==================
*/
const char *Host_FindRecentSave( char *pNameBuf )
{
	const char	*findfn;
	char	basefilename[MAX_PATH];
	int		found;
	int		newest;
	int		ft;
	char	szPath[MAX_PATH];

	sprintf( pNameBuf, "%s*.sav", Host_SaveGameDirectory() );
	_snprintf( szPath, sizeof( szPath ), "%s", Host_SaveGameDirectory() );

	findfn = Sys_FindFirst( pNameBuf, basefilename );
	found = 0;

	while ( findfn )
	{
		if ( Q_strlen( findfn ) && Q_strcasecmp( findfn, "HLSave.sav" ) )
		{
			_snprintf( szPath, sizeof( szPath ), "%s%s", Host_SaveGameDirectory(), findfn );

			ft = FS_GetFileTime( szPath );
			if ( ft > 0 && ( !found || newest < ft ) )
			{
				newest = ft;
				Q_strcpy( pNameBuf, findfn );
				found = 1;
			}
		}

		findfn = Sys_FindNext( basefilename );
	}

	Sys_FindClose();

	if ( found )
		return pNameBuf;

	return NULL;
}

/*
==================
Host_Restart_f

Restarts the current server for a dead player
==================
*/
void Host_Restart_f( void )
{
	char	name[MAX_PATH];

	if ( cls.demoplayback || !sv.active )
		return;

	if ( cmd_source != src_command )
		return;

	if ( cls.state != ca_dedicated )
		cls.state = ca_disconnected;

	Host_ClearGameState();
	SV_InactivateClients();

	Q_strncpy( name, sv.name, sizeof( name ) - 1 );
	name[ sizeof( name ) - 1 ] = 0;

	SV_ServerShutdown();
	SV_SpawnServer( false, name, NULL );
	SV_LoadEntities();
	SV_ActivateServer( 1 );
}

/*
==================
Host_Reload_f

Restarts the current server from the most recent save
==================
*/
void Host_Reload_f( void )
{
	const char	*pSaveName;
	char	name[MAX_PATH];

	if ( cls.demoplayback || !sv.active )
		return;

	if ( cmd_source != src_command )
		return;

	Host_ClearGameState();
	SV_InactivateClients();
	SV_ServerShutdown();

	pSaveName = Host_FindRecentSave( name );
	if ( !pSaveName || !Host_Load( pSaveName ) )
	{
		SV_SpawnServer( false, gHostMap.string, NULL );
		SV_LoadEntities();
		SV_ActivateServer( 1 );
	}
}

/*
=====================
Host_Reconnect_f

This command causes the client to wait for the signon messages again.
This is sent just before a server changes levels
=====================
*/
void Host_Reconnect_f( void )
{
	char	cmdString[128];

	if ( cls.state == ca_dedicated || cls.state == ca_disconnected || cls.state == ca_connecting )
		return;

	if ( cls.passive )
	{
		_snprintf( cmdString, sizeof( cmdString ), "listen %s\n", NET_AdrToString( cls.connect_stream ) );
		Cbuf_AddText( cmdString );
		return;
	}

	SCR_BeginLoadingPlaque( true );

	cls.signon = 0;		// need new connection messages
	cls.state = ca_connected;
	sys_timescale.value = 1.0f;

	Netchan_Clear( &cls.netchan );
	SZ_Clear( &cls.netchan.message );

	MSG_WriteChar( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, "new" );
}

/*
==================
Host_SaveGameDirectory

==================
*/
char *Host_SaveGameDirectory( void )
{
	static char	szDirectory[MAX_PATH];

	Q_memset( szDirectory, 0, sizeof( szDirectory ) );
	_snprintf( szDirectory, sizeof( szDirectory ), "SAVE/" );

	return szDirectory;
}

TITLECOMMENT	gTitleComments[] =
{
	{ "T0A0", "#T0A0TITLE" },
	{ "C0A0", "#C0A0TITLE" },
	{ "C1A0", "#C0A1TITLE" },
	{ "C1A1", "#C1A1TITLE" },
	{ "C1A2", "#C1A2TITLE" },
	{ "C1A3", "#C1A3TITLE" },
	{ "C1A4", "#C1A4TITLE" },
	{ "C2A1", "#C2A1TITLE" },
	{ "C2A2", "#C2A2TITLE" },
	{ "C2A3", "#C2A3TITLE" },
	{ "C2A4D", "#C2A4TITLE2" },
	{ "C2A4E", "#C2A4TITLE2" },
	{ "C2A4F", "#C2A4TITLE2" },
	{ "C2A4G", "#C2A4TITLE2" },
	{ "C2A4", "#C2A4TITLE1" },
	{ "C2A5", "#C2A5TITLE" },
	{ "C3A1", "#C3A1TITLE" },
	{ "C3A2", "#C3A2TITLE" },
	{ "C4A1A", "#C4A1ATITLE" },
	{ "C4A1B", "#C4A1ATITLE" },
	{ "C4A1C", "#C4A1ATITLE" },
	{ "C4A1D", "#C4A1ATITLE" },
	{ "C4A1E", "#C4A1ATITLE" },
	{ "C4A1", "#C4A1TITLE" },
	{ "C4A2", "#C4A2TITLE" },
	{ "C4A3", "#C4A3TITLE" },
	{ "C5A1", "#C5TITLE" },
};

/*
==================
Host_SavegameComment

==================
*/
void Host_SavegameComment( char *pszBuffer, int iSizeBuffer )
{
	const char	*pszName = NULL;
	const char	*pszMapName = pr_strings + gGlobalVariables.mapname;
	int		i = 0;

	for ( i = 0; i < ARRAYSIZE( gTitleComments ) && !pszName; i++ )
	{
		if ( !Q_strnicmp( pszMapName, gTitleComments[i].pBSPName, Q_strlen( gTitleComments[i].pBSPName ) ) )
			pszName = gTitleComments[i].pTitleName;
	}

	if ( !pszName )
	{
		pszName = pszMapName;
		if ( ( !pszMapName || !pszMapName[0] ) && cl.levelname && Q_strlen( cl.levelname ) )
			pszName = cl.levelname;
	}

	strncpy( pszBuffer, pszName, iSizeBuffer - 1 );
	pszBuffer[iSizeBuffer - 1] = 0;
}

/*
==================
Host_SaveAgeList

==================
*/
void Host_SaveAgeList( const char *pName, int count )
{
	char	newName[MAX_PATH];
	char	oldName[MAX_PATH];

	_snprintf( newName, sizeof( newName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count );
	COM_FixSlashes( newName );
	FS_RemoveFile( newName, NULL );

	while ( count > 0 )
	{
		if ( count == 1 )
			_snprintf( oldName, sizeof( oldName ), "%s%s.sav", Host_SaveGameDirectory(), pName );
		else
			_snprintf( oldName, sizeof( oldName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count - 1 );
		COM_FixSlashes( oldName );

		_snprintf( newName, sizeof( newName ), "%s%s%02d.sav", Host_SaveGameDirectory(), pName, count );
		COM_FixSlashes( newName );

		rename( oldName, newName );
		count--;
	}
}

/*
==================
Host_ValidSave

==================
*/
int Host_ValidSave( void )
{
	if ( cmd_source != src_command )
		return 0;

	if ( !g_bSaveAllowed_guess )
	{
		Con_Printf( "Use save points to save your progress.\n" );
		return 0;
	}

	if ( !sv.active )
	{
		Con_Printf( "Not playing a local game.\n" );
		return 0;
	}

	if ( svs.maxclients != 1 )
	{
		Con_Printf( "Can't save multiplayer games.\n" );
		return 0;
	}

	if ( cls.state != ca_active || cls.signon != SIGNONS )
	{
		Con_Printf( "Can't save during transition.\n" );
		return 0;
	}

	if ( cl.intermission )
	{
		Con_Printf( "Can't save in intermission.\n" );
		return 0;
	}

	if ( svs.clients->active && svs.clients->edict->v.health <= 0 )
	{
		Con_Printf( "Can't savegame with a dead player\n" );
		return 0;
	}

	return 1;
}

/*
==================
SaveInit

==================
*/
SAVERESTOREDATA *SaveInit( int size )
{
	int		i;
	edict_t	*pEdict;
	SAVERESTOREDATA	*pSaveData;

	if ( size <= 0 )
		size = 0x80000;		// Reserve 512K for now, UNDONE: Shrink this after compressing strings

	pSaveData = (SAVERESTOREDATA *)Mem_Calloc( sizeof( SAVERESTOREDATA ) + ( sizeof( ENTITYTABLE ) * sv.num_edicts ) + size, sizeof( char ) );
	pSaveData->pTable = (ENTITYTABLE *)( pSaveData + 1 );	// skip the save structure
	pSaveData->tokenSize = 0;		// Size of the linear list of tokens
	pSaveData->tokenCount = 0xFFF;	// Number of elements in the pTokens table
	pSaveData->pTokens = (char **)Mem_Calloc( pSaveData->tokenCount, sizeof( char * ) );

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		pEdict = &sv.edicts[i];

		pSaveData->pTable[i].id = i;
		pSaveData->pTable[i].pent = pEdict;
		pSaveData->pTable[i].flags = 0;
		pSaveData->pTable[i].location = 0;
		pSaveData->pTable[i].size = 0;
		pSaveData->pTable[i].classname = 0;
	}

	pSaveData->tableCount = sv.num_edicts;
	pSaveData->connectionCount = 0;
	pSaveData->pBaseData = (char *)( pSaveData->pTable + sv.num_edicts );	// skip the save structure
	pSaveData->pCurrentData = pSaveData->pBaseData;
	pSaveData->size = 0;
	pSaveData->bufferSize = size;

	pSaveData->time = gGlobalVariables.time;
	VectorCopy( vec3_origin, pSaveData->vecLandmarkOffset );
	pSaveData->fUseLandmark = 0;

	gGlobalVariables.pSaveData = pSaveData;

	return pSaveData;
}

/*
==================
SaveExit

==================
*/
void SaveExit( SAVERESTOREDATA *save )
{
	if ( save->pTokens )
	{
		Mem_Free( save->pTokens );
		save->pTokens = NULL;
		save->tokenCount = 0;
	}

	if ( save )
		Mem_Free( save );

	gGlobalVariables.pSaveData = NULL;
}

/*
==================
SaveGameSlot

==================
*/
qboolean SaveGameSlot( const char *pSaveName, const char *pSaveComment )
{
	char	hlPath[256];
	char	name[256];
	char	*pTokenData;
	int		tag;
	int		i;
	FileHandle_t	pFile;
	SAVERESTOREDATA	*pSaveData;
	GAME_HEADER	gameHeader;

	pSaveData = SaveGamestate();
	if ( !pSaveData )
		return false;

	SaveExit( pSaveData );

	pSaveData = SaveInit( 0 );

	_snprintf( hlPath, sizeof( hlPath ), "%s*.HL?", Host_SaveGameDirectory() );
	COM_FixSlashes( hlPath );

	gameHeader.mapCount = DirectoryCount( hlPath );
	Q_strncpy( gameHeader.mapName, sv.name, sizeof( gameHeader.mapName ) - 1 );
	gameHeader.mapName[ sizeof( gameHeader.mapName ) - 1 ] = 0;
	Q_strncpy( gameHeader.comment, pSaveComment, sizeof( gameHeader.comment ) - 1 );
	gameHeader.comment[ sizeof( gameHeader.comment ) - 1 ] = 0;

	gEntityInterface.pfnSaveWriteFields( pSaveData, "GameHeader", &gameHeader, gGameHeaderDescription, ARRAYSIZE( gGameHeaderDescription ) );
	gEntityInterface.pfnSaveGlobalState( pSaveData );

	// Write entity string token table
	pTokenData = pSaveData->pCurrentData;
	if ( pSaveData->pTokens )
	{
		for ( i = 0; i < pSaveData->tokenCount; i++ )
		{
			if ( pSaveData->pTokens[i] )
			{
				char	*pszToken;

				pszToken = pSaveData->pTokens[i];
				pSaveData->size += Q_strlen( pszToken ) + 1;
				if ( pSaveData->size > pSaveData->bufferSize )
				{
					Con_Printf( "Token Table Save/Restore overflow!" );
					pSaveData->size = pSaveData->bufferSize;
					break;
				}

				while ( *pSaveData->pCurrentData++ = *pszToken++ )
					;
			}
			else
			{
				pSaveData->size += 1;
				if ( pSaveData->size > pSaveData->bufferSize )
				{
					Con_Printf( "Token Table Save/Restore overflow!" );
					pSaveData->size = pSaveData->bufferSize;
					break;
				}

				*pSaveData->pCurrentData++ = 0;
			}
		}
	}

	pSaveData->tokenSize = pSaveData->pCurrentData - pTokenData;

	if ( pSaveData->size < pSaveData->bufferSize )
		pSaveData->size -= pSaveData->tokenSize;

	_snprintf( name, 252, "%s%s", Host_SaveGameDirectory(), pSaveName );
	COM_DefaultExtension( name, ".sav" );
	COM_FixSlashes( name );
	Con_DPrintf( "Saving game to %s...\n", name );

	if ( Q_stricmp( pSaveName, "quick" ) || Q_stricmp( pSaveName, "autosave" ) )
		Host_SaveAgeList( pSaveName, 1 );

	pFile = FS_Open( name, "wb" );

	tag = SAVEGAME_HEADER;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	tag = SAVEGAME_VERSION;
	FS_Write( &tag, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->size, sizeof( int ), 1, pFile );			// Does not include token table
	FS_Write( &pSaveData->tokenCount, sizeof( int ), 1, pFile );
	FS_Write( &pSaveData->tokenSize, sizeof( int ), 1, pFile );
	FS_Write( pTokenData, pSaveData->tokenSize, 1, pFile );
	FS_Write( pSaveData->pBaseData, pSaveData->size, 1, pFile );

	DirectoryCopy( hlPath, pFile );

	FS_Close( pFile );
	SaveExit( pSaveData );

	return true;
}

/*
==================
Host_Savegame_f

==================
*/
void Host_Savegame_f( void )
{
	char	szComment[80];
	char	szTemp[80];

	if ( !Host_ValidSave() )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_DPrintf( "save <savename> : save a game\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_DPrintf( "Relative pathnames are not allowed.\n" );
		return;
	}

	g_pSaveGameCommentFunc( szTemp, 80 );
	_snprintf( szComment, sizeof( szComment ) - 1, "%-64.64s %02d:%02d", szTemp, (int)( sv.time / 60.0 ), (int)fmod( sv.time, 60.0 ) );
	szComment[ sizeof( szComment ) - 1 ] = 0;

	SaveGameSlot( Cmd_Argv( 1 ), szComment );
	g_bSaveAllowed_guess = false;
}

/*
==================
Host_AutoSave_f

==================
*/
void Host_AutoSave_f( void )
{
	char	szComment[80];
	char	szTemp[80];

	if ( !Host_ValidSave() )
		return;

	g_pSaveGameCommentFunc( szTemp, 80 );
	_snprintf( szComment, sizeof( szComment ) - 1, "%-64.64s %02d:%02d", szTemp, (int)( sv.time / 60.0 ), (int)fmod( sv.time, 60.0 ) );
	szComment[ sizeof( szComment ) - 1 ] = 0;
	SaveGameSlot( "autosave", szComment );
}

/*
==================
SaveGame

==================
*/
qboolean SaveGame( const char *pszSlot, const char *pszComment )
{
	qboolean	qret;
	qboolean	q;

	q = scr_skipupdate;
	scr_skipupdate = true;
	qret = SaveGameSlot( pszSlot, pszComment );
	scr_skipupdate = q;

	return qret;
}

/*
==================
SaveReadHeader

==================
*/
int SaveReadHeader( FileHandle_t pFile, GAME_HEADER *pHeader, int readGlobalState )
{
	int		i;
	int		tag;
	int		size;
	int		tokenCount;
	int		tokenSize;
	char	*pszTokenList;
	SAVERESTOREDATA	*pSaveData;

	FS_Read( &tag, sizeof( int ), 1, pFile );
	if ( tag != SAVEGAME_HEADER )
	{
		FS_Close( pFile );
		return 0;
	}

	FS_Read( &tag, sizeof( int ), 1, pFile );
	if ( tag != SAVEGAME_VERSION )
	{
		FS_Close( pFile );
		return 0;
	}

	FS_Read( &size, sizeof( int ), 1, pFile );
	FS_Read( &tokenCount, sizeof( int ), 1, pFile );
	FS_Read( &tokenSize, sizeof( int ), 1, pFile );

	pSaveData = (SAVERESTOREDATA *)Mem_Calloc( sizeof( SAVERESTOREDATA ) + tokenSize + size, sizeof( char ) );
	pSaveData->tableCount = 0;
	pSaveData->pTable = NULL;
	pSaveData->connectionCount = 0;

	pszTokenList = (char *)( pSaveData + 1 );

	if ( tokenSize > 0 )
	{
		pSaveData->tokenCount = tokenCount;
		pSaveData->tokenSize = tokenSize;

		// Make sure the token strings pointed to by the pToken hashtable.
		FS_Read( pszTokenList, tokenSize, 1, pFile );

		if ( !pSaveData->pTokens )
			pSaveData->pTokens = (char **)Mem_Calloc( tokenCount, sizeof( char * ) );

		for ( i = 0; i < tokenCount; i++ )
		{
			pSaveData->pTokens[i] = *pszTokenList ? pszTokenList : NULL;
			while ( *pszTokenList++ )	// Find next token (after next null)
				;
		}
	}

	pSaveData->size = 0;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 0;
	pSaveData->time = 0;
	pSaveData->pCurrentData = pszTokenList;
	pSaveData->pBaseData = pszTokenList;

	FS_Read( pSaveData->pBaseData, size, 1, pFile );

	gEntityInterface.pfnSaveReadFields( pSaveData, "GameHeader", pHeader, gGameHeaderDescription, ARRAYSIZE( gGameHeaderDescription ) );
	if ( readGlobalState )
		gEntityInterface.pfnRestoreGlobalState( pSaveData );

	SaveExit( pSaveData );

	return 1;
}

/*
==================
SaveReadComment

==================
*/
void SaveReadComment( FileHandle_t f, char *name )
{
	GAME_HEADER	gameHeader;

	if ( SaveReadHeader( f, &gameHeader, 0 ) )
		Q_strcpy( name, gameHeader.comment );
}

/*
==================
Host_Loadgame_f

==================
*/
void Host_Loadgame_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "load <savename> : load a game\n" );
		return;
	}

	if ( !Host_Load( Cmd_Argv( 1 ) ) )
		Con_Printf( "Error loading saved game\n" );
}

/*
==================
LoadGame

==================
*/
int LoadGame( const char *pName )
{
	int			iRet;
	qboolean	q;

	q = scr_skipupdate;
	scr_skipupdate = true;
	iRet = Host_Load( pName );
	scr_skipupdate = q;

	return iRet;
}

/*
==================
Host_Load

==================
*/
int Host_Load( const char *pName )
{
	FileHandle_t	pFile;
	GAME_HEADER	gameHeader;
	char	name[256];
	int		c;
	char	*pTempNumber;
	char	szNumber[5];
	int		nSlot;

	if ( !pName || !pName[0] )
		return 0;

	if ( Q_strstr( pName, ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return 0;
	}

	if ( *pName == '_' )
	{
		pTempNumber = (char *)pName;
		pTempNumber++;

		c = 0;
		while ( *pTempNumber && isdigit( *pTempNumber ) && c < 5 )
		{
			szNumber[c] = *pTempNumber;
			c++;
			pTempNumber++;
		}
		szNumber[c] = 0;

		nSlot = Q_atoi( szNumber );
		if ( nSlot > 0 && nSlot <= 12 )
		{
			_snprintf( name, 252, "%sHalf-Life-%i", Host_SaveGameDirectory(), nSlot );
			name[251] = 0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		_snprintf( name, 252, "%s%s", Host_SaveGameDirectory(), pName );
		name[251] = 0;
	}

	if ( !svs.dll_initialized )
		Host_InitializeGameDLL();

	COM_DefaultExtension( name, ".sav" );
	COM_FixSlashes( name );
	name[255] = 0;

	Con_Printf( "Loading game from %s...\n", name );

	pFile = FS_Open( name, "rb" );
	if ( !pFile )
		return 0;

	Host_ClearGameState();

	if ( !SaveReadHeader( pFile, &gameHeader, 1 ) )
	{
		giStateInfo = 1;
		Cbuf_AddText( "\ndisconnect\n" );
		return 0;
	}

	cls.demonum = -1;		// stop demo loop in case this fails
	SV_InactivateClients();
	SCR_BeginLoadingPlaque( false );

	DirectoryExtract( pFile, gameHeader.mapCount );
	FS_Close( pFile );

	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );

	_snprintf( name, sizeof( name ), "map %s\n", gameHeader.mapName );
	CL_Disconnect();
	Host_Map( false, name, gameHeader.mapName, true );

	return 1;
}

/*
==================
SaveGamestate

==================
*/
SAVERESTOREDATA *SaveGamestate( void )
{
	char	name[256];
	char	*pTableData;
	char	*pTokenData;
	FileHandle_t	pFile;
	int		i;
	int		dataSize;
	int		tableSize;
	edict_t	*pent;
	SAVE_HEADER	header;
	SAVERESTOREDATA	*pSaveData;
	SAVELIGHTSTYLE	light;

	if ( !gEntityInterface.pfnParmsChangeLevel )
		return NULL;

	pSaveData = SaveInit( 0 );

	_snprintf( name, sizeof( name ), "%s%s.HL1", Host_SaveGameDirectory(), sv.name );
	COM_FixSlashes( name );

	gEntityInterface.pfnParmsChangeLevel();

	header.version = build_number();
	header.skillLevel = skill.value;	// This is created from an int even though it's a float
	header.entityCount = pSaveData->tableCount;
	header.connectionCount = pSaveData->connectionCount;
	header.time = sv.time;
	Q_strncpy( header.skyName, sv_skyname.string, sizeof( header.skyName ) - 1 );
	header.skyName[ sizeof( header.skyName ) - 1 ] = 0;
	header.skyColor_r = sv_skycolor_r.value;
	header.skyColor_g = sv_skycolor_g.value;
	header.skyColor_b = sv_skycolor_b.value;
	header.skyVec_x = sv_skyvec_x.value;
	header.skyVec_y = sv_skyvec_y.value;
	header.skyVec_z = sv_skyvec_z.value;

	pSaveData->time = 0;		// Base time is level time

	Q_strncpy( header.mapName, sv.name, sizeof( header.mapName ) - 1 );
	header.mapName[ sizeof( header.mapName ) - 1 ] = 0;

	header.lightStyleCount = 0;
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		if ( sv.lightstyles[i] )
			header.lightStyleCount++;
	}

	gEntityInterface.pfnSaveWriteFields( pSaveData, "Save Header", &header, gSaveHeaderDescription, ARRAYSIZE( gSaveHeaderDescription ) );

	pSaveData->time = header.time;

	// Write adjacency list
	for ( i = 0; i < pSaveData->connectionCount; i++ )
		gEntityInterface.pfnSaveWriteFields( pSaveData, "ADJACENCY", &pSaveData->levelList[i], gAdjacencyDescription, ARRAYSIZE( gAdjacencyDescription ) );

	// Write the lightstyles
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		if ( sv.lightstyles[i] )
		{
			light.index = i;
			Q_strncpy( light.style, sv.lightstyles[i], sizeof( light.style ) - 1 );
			light.style[ sizeof( light.style ) - 1 ] = 0;
			gEntityInterface.pfnSaveWriteFields( pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE( gLightstyleDescription ) );
		}
	}

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		pent = &sv.edicts[i];
		pSaveData->currentIndex = i;
		pSaveData->pTable[i].location = pSaveData->size;
		pSaveData->pTable[i].size = 0;

		if ( !pent->free )
		{
			gEntityInterface.pfnSave( pent, pSaveData );

			if ( i > 0 && i < svs.maxclients + 1 )
				pSaveData->pTable[i].flags |= FENTTABLE_PLAYER;
		}
	}

	// Write entity table
	dataSize = pSaveData->size;
	pTableData = pSaveData->pCurrentData;

	for ( i = 0; i < sv.num_edicts; i++ )
		gEntityInterface.pfnSaveWriteFields( pSaveData, "ETABLE", &pSaveData->pTable[i], gEntityTableDescription, ARRAYSIZE( gEntityTableDescription ) );

	tableSize = pSaveData->size - dataSize;

	// Write entity string token table
	pTokenData = pSaveData->pCurrentData;
	if ( pSaveData->pTokens )
	{
		for ( i = 0; i < pSaveData->tokenCount; i++ )
		{
			if ( pSaveData->pTokens[i] )
			{
				char	*pszToken;

				pszToken = pSaveData->pTokens[i];
				while ( *pSaveData->pCurrentData++ = *pszToken++ )
					;
			}
			else
			{
				*pSaveData->pCurrentData++ = 0;
			}
		}
	}

	pSaveData->tokenSize = pSaveData->pCurrentData - pTokenData;

	COM_CreatePath( name );

	pFile = FS_Open( name, "wb" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open save game file %s.", name );
		return NULL;
	}

	// Write the header -- THIS SHOULD NEVER CHANGE STRUCTURE, USE SAVE_HEADER FOR NEW HEADER INFORMATION
	// THIS IS ONLY HERE TO IDENTIFY THE FILE AND GET IT'S SIZE.
	i = SAVEFILE_HEADER;
	FS_Write( &i, sizeof( int ), 1, pFile );
	i = SAVEGAME_VERSION;
	FS_Write( &i, sizeof( int ), 1, pFile );

	// Write out the tokens and table FIRST so they are loaded in the right order, then write out the rest of the data in the file.
	FS_Write( &pSaveData->size, sizeof( int ), 1, pFile );			// total size of all data to initialize read buffer
	FS_Write( &pSaveData->tableCount, sizeof( int ), 1, pFile );	// entity table size
	FS_Write( &pSaveData->tokenCount, sizeof( int ), 1, pFile );	// num of tokens in the table
	FS_Write( &pSaveData->tokenSize, sizeof( int ), 1, pFile );		// size of token data
	FS_Write( pTokenData, pSaveData->tokenSize, 1, pFile );			// Write out the tokens first so we can load them before we load the entities
	FS_Write( pTableData, tableSize, 1, pFile );					// Write entity table
	FS_Write( pSaveData->pBaseData, dataSize, 1, pFile );			// Write entity data
	FS_Close( pFile );

	EntityPatchWrite( pSaveData, sv.name );

	_snprintf( name, sizeof( name ), "%s%s.HL2", Host_SaveGameDirectory(), sv.name );
	COM_FixSlashes( name );
	CL_Save( name );

	return pSaveData;
}

/*
==================
CL_Save

==================
*/
void CL_Save( const char *name )
{
	DECALLIST	decalList[4096];
	int		i;
	int		decalCount;
	int		temp;
	FileHandle_t	pFile;

	// Read decal list
	decalCount = DecalListCreate( decalList );

	pFile = FS_Open( name, "wb" );
	if ( pFile )
	{
		temp = SAVEFILE_HEADER;
		FS_Write( &temp, sizeof( int ), 1, pFile );
		temp = SAVEGAME_VERSION;
		FS_Write( &temp, sizeof( int ), 1, pFile );
		FS_Write( &decalCount, sizeof( int ), 1, pFile );

		for ( i = 0; i < decalCount; i++ )
		{
			FS_Write( decalList[i].name, 16, 1, pFile );
			FS_Write( &decalList[i].entityIndex, sizeof( short ), 1, pFile );
			FS_Write( &decalList[i].depth, sizeof( byte ), 1, pFile );
			FS_Write( &decalList[i].flags, sizeof( byte ), 1, pFile );
			FS_Write( decalList[i].position, sizeof( vec3_t ), 1, pFile );
		}

		FS_Close( pFile );
	}
}

/*
==================
EntityInit

==================
*/
void EntityInit( edict_t *pEdict, int className )
{
	ENTITYINIT	pEntityInit;

	if ( !className )
		Sys_Error( "Bad class!!\n" );

	ReleaseEntityDLLFields( pEdict );
	InitEntityDLLFields( pEdict );
	pEdict->v.classname = className;

	// Initialize the default values
	pEntityInit = GetEntityInit( pr_strings + className );
	if ( pEntityInit )
		pEntityInit( &pEdict->v );
}

/*
==================
LoadSaveData

==================
*/
SAVERESTOREDATA *LoadSaveData( const char *level )
{
	char	name[MAX_PATH];
	char	*pszTokenList;
	FileHandle_t	pFile;
	int		i;
	int		size;
	int		tokenCount;
	int		tokenSize;
	int		tableCount;
	SAVERESTOREDATA	*pSaveData;

	_snprintf( name, sizeof( name ), "%s%s.HL1", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );

	Con_Printf( "Loading game from %s...\n", name );

	pFile = FS_Open( name, "rb" );
	if ( !pFile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		return NULL;
	}

	// Read the header
	FS_Read( &i, sizeof( int ), 1, pFile );
	// Is this a valid save?
	if ( i != SAVEFILE_HEADER )
		return NULL;

	// Read the version
	FS_Read( &i, sizeof( int ), 1, pFile );
	// Is this a valid save?
	if ( i > SAVEGAME_VERSION )
		return NULL;

	FS_Read( &size, sizeof( int ), 1, pFile );
	FS_Read( &tableCount, sizeof( int ), 1, pFile );
	FS_Read( &tokenCount, sizeof( int ), 1, pFile );
	FS_Read( &tokenSize, sizeof( int ), 1, pFile );

	pSaveData = (SAVERESTOREDATA *)Mem_Calloc( sizeof( SAVERESTOREDATA ) + tokenSize + ( size + sizeof( ENTITYTABLE ) * tableCount ), sizeof( char ) );

	pSaveData->tableCount = tableCount;
	pSaveData->tokenCount = tokenCount;
	pSaveData->tokenSize = tokenSize;

	Q_strncpy( pSaveData->szCurrentMapName, level, sizeof( pSaveData->szCurrentMapName ) - 1 );
	pSaveData->szCurrentMapName[ sizeof( pSaveData->szCurrentMapName ) - 1 ] = 0;

	// Parse the tokens
	pszTokenList = (char *)( pSaveData + 1 );

	if ( tokenSize > 0 )
	{
		FS_Read( pszTokenList, pSaveData->tokenSize, 1, pFile );

		if ( !pSaveData->pTokens )
			pSaveData->pTokens = (char **)Mem_Calloc( tokenCount, sizeof( char * ) );

		for ( i = 0; i < tokenCount; i++ )
		{
			pSaveData->pTokens[i] = *pszTokenList ? pszTokenList : NULL;
			while ( *pszTokenList++ )	// Find next token (after next null)
				;
		}
	}

	// Set up the table
	pSaveData->pTable = (ENTITYTABLE *)pszTokenList;
	pSaveData->connectionCount = 0;
	pSaveData->pBaseData = (char *)( pSaveData->pTable + pSaveData->tableCount );
	pSaveData->pCurrentData = pSaveData->pBaseData;
	pSaveData->size = 0;
	pSaveData->bufferSize = size;
	pSaveData->fUseLandmark = 1;
	pSaveData->time = 0;
	VectorClear( pSaveData->vecLandmarkOffset );
	gGlobalVariables.pSaveData = pSaveData;

	FS_Read( pSaveData->pBaseData, size, 1, pFile );
	FS_Close( pFile );

	return pSaveData;
}

/*
==================
ParseSaveTables

==================
*/
void ParseSaveTables( SAVERESTOREDATA *pSaveData, SAVE_HEADER *pHeader, int updateGlobals )
{
	int		i;
	SAVELIGHTSTYLE	light;

	// Re-base the savedata since we re-ordered the entity/table / restore fields
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		gEntityInterface.pfnSaveReadFields( pSaveData, "ETABLE", &pSaveData->pTable[i], gEntityTableDescription, ARRAYSIZE( gEntityTableDescription ) );
		pSaveData->pTable[i].pent = NULL;
	}

	pSaveData->pBaseData = pSaveData->pCurrentData;
	pSaveData->size = 0;

	gEntityInterface.pfnSaveReadFields( pSaveData, "Save Header", pHeader, gSaveHeaderDescription, ARRAYSIZE( gSaveHeaderDescription ) );

	pSaveData->connectionCount = pHeader->connectionCount;
	pSaveData->time = pHeader->time;
	pSaveData->fUseLandmark = 1;
	VectorClear( pSaveData->vecLandmarkOffset );

	// Read adjacency list
	for ( i = 0; i < pSaveData->connectionCount; i++ )
		gEntityInterface.pfnSaveReadFields( pSaveData, "ADJACENCY", &pSaveData->levelList[i], gAdjacencyDescription, ARRAYSIZE( gAdjacencyDescription ) );

	if ( updateGlobals )
	{
		for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
			sv.lightstyles[i] = NULL;
	}

	for ( i = 0; i < pHeader->lightStyleCount; i++ )
	{
		gEntityInterface.pfnSaveReadFields( pSaveData, "LIGHTSTYLE", &light, gLightstyleDescription, ARRAYSIZE( gLightstyleDescription ) );

		if ( updateGlobals )
		{
			sv.lightstyles[light.index] = (char *)Hunk_Alloc( Q_strlen( light.style ) + 1 );
			Q_strncpy( sv.lightstyles[light.index], light.style, 3 );
			sv.lightstyles[light.index][3] = 0;
		}
	}
}

/*
==================
EntityPatchWrite

Write out the list of entities that are no longer in the save file for this level
(they've been moved to another level)
==================
*/
void EntityPatchWrite( SAVERESTOREDATA *pSaveData, const char *level )
{
	char	name[MAX_PATH];
	FileHandle_t	pFile;
	int		i;
	int		size;

	_snprintf( name, sizeof( name ), "%s%s.HL3", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );

	pFile = FS_Open( name, "wb" );
	if ( pFile )
	{
		size = 0;
		for ( i = 0; i < pSaveData->tableCount; i++ )
		{
			if ( pSaveData->pTable[i].flags & FENTTABLE_REMOVED )
				size++;
		}

		// Patch count
		FS_Write( &size, sizeof( int ), 1, pFile );

		for ( i = 0; i < pSaveData->tableCount; i++ )
		{
			if ( pSaveData->pTable[i].flags & FENTTABLE_REMOVED )
				FS_Write( &i, sizeof( int ), 1, pFile );
		}

		FS_Close( pFile );
	}
}

/*
==================
EntityPatchRead

Read the list of entities that are no longer in the save file for this level
(they've been moved to another level)
==================
*/
void EntityPatchRead( SAVERESTOREDATA *pSaveData, const char *level )
{
	char	name[MAX_PATH];
	FileHandle_t	pFile;
	int		i;
	int		size;
	int		entityId;

	_snprintf( name, sizeof( name ), "%s%s.HL3", Host_SaveGameDirectory(), level );
	COM_FixSlashes( name );

	pFile = FS_Open( name, "rb" );
	if ( pFile )
	{
		// Patch count
		FS_Read( &size, sizeof( int ), 1, pFile );

		for ( i = 0; i < size; i++ )
		{
			FS_Read( &entityId, sizeof( int ), 1, pFile );
			pSaveData->pTable[entityId].flags = FENTTABLE_REMOVED;
		}

		FS_Close( pFile );
	}
}

/*
==================
LoadGamestate

==================
*/
int LoadGamestate( char *level, int createPlayers )
{
	int		i;
	SAVE_HEADER	header;
	SAVERESTOREDATA	*pSaveData;
	edict_t	*pent;

	pSaveData = LoadSaveData( level );
	if ( !pSaveData )
		return 0;

	ParseSaveTables( pSaveData, &header, 1 );
	EntityPatchRead( pSaveData, level );

	Cvar_SetValue( "skill", header.skillLevel );

	Q_strncpy( sv.name, header.mapName, sizeof( sv.name ) - 1 );
	sv.name[ sizeof( sv.name ) - 1 ] = 0;

	Cvar_Set( "sv_skyname", header.skyName );
	Cvar_SetValue( "sv_skycolor_r", header.skyColor_r );
	Cvar_SetValue( "sv_skycolor_g", header.skyColor_g );
	Cvar_SetValue( "sv_skycolor_b", header.skyColor_b );
	Cvar_SetValue( "sv_skyvec_x", header.skyVec_x );
	Cvar_SetValue( "sv_skyvec_y", header.skyVec_y );
	Cvar_SetValue( "sv_skyvec_z", header.skyVec_z );

	// Create entity list
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		if ( pSaveData->pTable[i].classname && pSaveData->pTable[i].size && !( pSaveData->pTable[i].flags & FENTTABLE_REMOVED ) )
		{
			if ( !pSaveData->pTable[i].id )
			{
				pent = sv.edicts;
				EntityInit( pent, pSaveData->pTable[i].classname );
			}
			else if ( pSaveData->pTable[i].id < svs.maxclients + 1 )
			{
				if ( !( pSaveData->pTable[i].flags & FENTTABLE_PLAYER ) )
					Sys_Error( "ENTITY IS NOT A PLAYER: %d\n", i );

				pent = svs.clients[ pSaveData->pTable[i].id - 1 ].edict;
				if ( createPlayers && pent )
					EntityInit( pent, pSaveData->pTable[i].classname );
				else
					pent = NULL;
			}
			else
			{
				pent = CreateNamedEntity( pSaveData->pTable[i].classname );
			}

			pSaveData->pTable[i].pent = pent;
		}
		else
		{
			pSaveData->pTable[i].pent = NULL;
		}
	}

	// Now spawn entities
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pent = pSaveData->pTable[i].pent;
		pSaveData->currentIndex = i;
		pSaveData->size = pSaveData->pTable[i].location;
		pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;

		if ( pent )
		{
			if ( gEntityInterface.pfnRestore( pent, pSaveData, 0 ) < 0 )
			{
				pSaveData->pTable[i].pent = NULL;
				ED_Free( pent );
			}
			else
			{
				SV_LinkEdict( pent, false );
			}
		}
	}

	SaveExit( pSaveData );

	sv.time = header.time;

	return 1;
}

/*
==================
EntryInTable

==================
*/
int EntryInTable( SAVERESTOREDATA *pSaveData, const char *pMapName, int index )
{
	int		i;

	index++;
	for ( i = index; i < pSaveData->connectionCount; i++ )
	{
		if ( !Q_strcmp( pSaveData->levelList[i].mapName, pMapName ) )
			return i;
	}

	return -1;
}

/*
==================
LandmarkOrigin

==================
*/
void LandmarkOrigin( SAVERESTOREDATA *pSaveData, vec_t *output, const char *pLandmarkName )
{
	int		i;

	for ( i = 0; i < pSaveData->connectionCount; i++ )
	{
		if ( !Q_strcmp( pSaveData->levelList[i].landmarkName, pLandmarkName ) )
		{
			VectorCopy( pSaveData->levelList[i].vecLandmarkOrigin, output );
			return;
		}
	}

	output[0] = 0;
	output[1] = 0;
	output[2] = 0;
}

/*
==================
EntityInSolid

==================
*/
int EntityInSolid( edict_t *pent )
{
	vec3_t	point;

	if ( pent->v.movetype == MOVETYPE_FOLLOW && pent->v.aiment && ( pent->v.aiment->v.flags & FL_CLIENT ) )
		return 0;

	point[0] = ( pent->v.absmin[0] + pent->v.absmax[0] ) * 0.5;
	point[1] = ( pent->v.absmin[1] + pent->v.absmax[1] ) * 0.5;
	point[2] = ( pent->v.absmin[2] + pent->v.absmax[2] ) * 0.5;

	g_groupmask = pent->v.groupinfo;

	return SV_PointContents( point ) == CONTENTS_SOLID;
}

/*
==================
CreateEntityList

==================
*/
int CreateEntityList( SAVERESTOREDATA *pSaveData, int levelMask )
{
	int		i;
	int		movedCount;
	int		active;
	edict_t	*pent;

	movedCount = 0;

	// Create entity list
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pent = NULL;

		if ( pSaveData->pTable[i].classname && pSaveData->pTable[i].size && pSaveData->pTable[i].id > 0 )
		{
			if ( pSaveData->pTable[i].flags & levelMask )
				active = 1;
			else
				active = 0;

			// spawn players
			if ( pSaveData->pTable[i].id < svs.maxclients + 1 )
			{
				pent = svs.clients[ pSaveData->pTable[i].id - 1 ].edict;
				if ( active )
				{
					if ( !( pSaveData->pTable[i].flags & FENTTABLE_PLAYER ) )
						Sys_Error( "ENTITY IS NOT A PLAYER: %d\n", i );

					if ( svs.clients[ pSaveData->pTable[i].id - 1 ].active && pent )
						EntityInit( pent, pSaveData->pTable[i].classname );
					else
						pent = NULL;
				}
			}
			else if ( active )
			{
				pent = CreateNamedEntity( pSaveData->pTable[i].classname );
			}
		}

		pSaveData->pTable[i].pent = pent;
	}

	// Now spawn entities
	for ( i = 0; i < pSaveData->tableCount; i++ )
	{
		pent = pSaveData->pTable[i].pent;
		pSaveData->currentIndex = i;
		pSaveData->size = pSaveData->pTable[i].location;
		pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;

		if ( pent && ( pSaveData->pTable[i].flags & levelMask ) )	// Screen out the player if he's not to be spawned
		{
			if ( pSaveData->pTable[i].flags & FENTTABLE_GLOBAL )
			{
				Con_DPrintf( "Merging changes for global: %s\n", pr_strings + pSaveData->pTable[i].classname );

				// -------------------------------------------------------------------------
				// Pass the "global" flag to the DLL to indicate this entity should only override
				// a matching entity, not be spawned
				gEntityInterface.pfnRestore( pent, pSaveData, 1 );
				ED_Free( pent );
			}
			else
			{
				Con_DPrintf( "Transferring %s (%d)\n", pr_strings + pSaveData->pTable[i].classname, NUM_FOR_EDICT( pent ) );

				if ( gEntityInterface.pfnRestore( pent, pSaveData, 0 ) < 0 )
				{
					ED_Free( pent );
				}
				else
				{
					SV_LinkEdict( pent, false );

					// Remove any entities that were in the solid world
					if ( !( pSaveData->pTable[i].flags & FENTTABLE_PLAYER ) && EntityInSolid( pent ) )
					{
						Con_DPrintf( "Suppressing %s\n", pr_strings + pSaveData->pTable[i].classname );
						ED_Free( pent );
					}
					else
					{
						movedCount++;
						pSaveData->pTable[i].flags = FENTTABLE_REMOVED;
					}
				}
			}
		}
	}

	return movedCount;
}

/*
==================
LoadAdjacentEntities

==================
*/
void LoadAdjacentEntities( const char *pOldLevel, const char *pLandmarkName )
{
	SAVERESTOREDATA	currentLevelData;
	SAVERESTOREDATA	*pSaveData;
	int		i;
	int		test;
	int		flags;
	int		index;
	int		movedCount = 0;
	SAVE_HEADER	header;
	vec3_t	landmarkOrigin;

	Q_memset( &currentLevelData, 0, sizeof( SAVERESTOREDATA ) );
	gGlobalVariables.pSaveData = &currentLevelData;

	// Build the adjacent map list
	gEntityInterface.pfnParmsChangeLevel();

	for ( i = 0; i < currentLevelData.connectionCount; i++ )
	{
		// make sure the previous level is in the connection list so we can
		// bring over the player.
		for ( test = 0; test < i; test++ )
		{
			// Only do maps once
			if ( !Q_strcmp( currentLevelData.levelList[i].mapName, currentLevelData.levelList[test].mapName ) )
				break;
		}

		// Map was already in the list
		if ( test < i )
			continue;

		pSaveData = LoadSaveData( currentLevelData.levelList[i].mapName );
		if ( pSaveData )
		{
			ParseSaveTables( pSaveData, &header, 0 );
			EntityPatchRead( pSaveData, currentLevelData.levelList[i].mapName );

			pSaveData->time = sv.time;
			pSaveData->fUseLandmark = 1;

			flags = 0;

			// Calculate landmark offset
			LandmarkOrigin( &currentLevelData, landmarkOrigin, pLandmarkName );
			LandmarkOrigin( pSaveData, pSaveData->vecLandmarkOffset, pLandmarkName );
			VectorSubtract( landmarkOrigin, pSaveData->vecLandmarkOffset, pSaveData->vecLandmarkOffset );

			if ( !Q_strcmp( currentLevelData.levelList[i].mapName, pOldLevel ) )
				flags |= FENTTABLE_PLAYER;

			index = -1;
			while ( 1 )
			{
				index = EntryInTable( pSaveData, sv.name, index );
				if ( index < 0 )
					break;

				flags |= ( 1 << index );
			}

			if ( flags )
				movedCount = CreateEntityList( pSaveData, flags );

			// If ents were moved, rewrite entity table to save file
			if ( movedCount )
				EntityPatchWrite( pSaveData, currentLevelData.levelList[i].mapName );

			SaveExit( pSaveData );
		}
	}

	gGlobalVariables.pSaveData = NULL;
}

/*
==================
FileSize

==================
*/
int FileSize( FileHandle_t pFile )
{
	if ( !pFile )
		return 0;

	return FS_Size( pFile );
}

/*
==================
FileCopy

Copy one file to another file
==================
*/
void FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )
{
	char	buf[1024];
	int		size;

	while ( fileSize > 0 )
	{
		if ( fileSize > 1024 )
			size = 1024;
		else
			size = fileSize;

		FS_Read( buf, size, 1, pInput );
		FS_Write( buf, size, 1, pOutput );

		fileSize -= size;
	}
}

/*
==================
DirectoryCopy

Copy all of the files matching pPath into pFile
==================
*/
void DirectoryCopy( const char *pPath, FileHandle_t pFile )
{
	const char	*findfn;
	char	basefindfn[MAX_PATH];
	int		fileSize;
	FileHandle_t	pCopy;
	char	szName[MAX_PATH];

	findfn = Sys_FindFirst( pPath, basefindfn );
	while ( findfn )
	{
		_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), findfn );
		COM_FixSlashes( szName );

		pCopy = FS_Open( szName, "rb" );
		fileSize = FileSize( pCopy );

		FS_Write( findfn, MAX_PATH, 1, pFile );
		FS_Write( &fileSize, sizeof( int ), 1, pFile );
		FileCopy( pFile, pCopy, fileSize );

		FS_Close( pCopy );

		findfn = Sys_FindNext( basefindfn );
	}

	Sys_FindClose();
}

/*
==================
DirectoryExtract

Copy fileCount files from pFile into the save directory
==================
*/
void DirectoryExtract( FileHandle_t pFile, int fileCount )
{
	int		i;
	int		fileSize;
	FileHandle_t	pCopy;
	char	szName[MAX_PATH];
	char	fileName[MAX_PATH];

	for ( i = 0; i < fileCount; i++ )
	{
		// Filename
		FS_Read( fileName, MAX_PATH, 1, pFile );
		// Size
		FS_Read( &fileSize, sizeof( int ), 1, pFile );

		_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), fileName );
		COM_FixSlashes( szName );

		pCopy = FS_Open( szName, "wb" );
		FileCopy( pCopy, pFile, fileSize );
		FS_Close( pCopy );
	}
}

/*
==================
DirectoryCount

==================
*/
int DirectoryCount( const char *pPath )
{
	int		count;
	const char	*findfn;

	count = 0;
	findfn = Sys_FindFirst( pPath, NULL );
	while ( findfn )
	{
		count++;
		findfn = Sys_FindNext( NULL );
	}
	Sys_FindClose();

	return count;
}

/*
==================
Host_RemoveSaveFiles_guess

Deletes the files of the save directory that match pPath
==================
*/
void Host_RemoveSaveFiles_guess( const char *pPath )
{
	const char	*pfn;
	char	szName[MAX_PATH];

	pfn = Sys_FindFirst( pPath, NULL );
	while ( pfn )
	{
		_snprintf( szName, sizeof( szName ), "%s%s", Host_SaveGameDirectory(), pfn );
		FS_RemoveFile( szName, NULL );

		pfn = Sys_FindNext( NULL );
	}
	Sys_FindClose();
}

/*
==================
Host_ClearSaveDirectory

==================
*/
void Host_ClearSaveDirectory( void )
{
	char	szName[MAX_PATH];

	_snprintf( szName, sizeof( szName ), "%s", Host_SaveGameDirectory() );
	COM_FixSlashes( szName );
	FS_CreateDirHierarchy( szName, NULL );

	strncat( szName, "*.HL?", sizeof( szName ) - strlen( szName ) - 1 );
	Host_RemoveSaveFiles_guess( szName );
}

/*
==================
Host_ClearGameState

==================
*/
void Host_ClearGameState( void )
{
	S_StopAllSounds( true );
	Host_ClearSaveDirectory();
	gEntityInterface.pfnResetGlobalState();
}

/*
==================
Host_Changelevel2_f

changing levels within a unit
==================
*/
void Host_Changelevel2_f( void )
{
	char	level[64];
	char	oldlevel[64];
	char	_startspot[64];
	char	*startspot;
	SAVERESTOREDATA	*pSaveData;
	qboolean	newUnit = false;

	giActive = DLL_TRANS;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "changelevel2 <levelname> : continue game on a new level in the unit\n" );
		return;
	}

	if ( !sv.active || cls.demoplayback || sv.paused )
	{
		Con_Printf( "Only the server may changelevel\n" );
		return;
	}

	if ( svs.maxclients > 1 )
	{
		Con_Printf( "changelevel2 <levelname> : not for use with multiplayer games\n" );
		return;
	}

	SCR_BeginLoadingPlaque( true );
	S_StopAllSounds( true );

	Q_strncpy( level, Cmd_Argv( 1 ), sizeof( level ) - 1 );
	level[ sizeof( level ) - 1 ] = 0;

	if ( Cmd_Argc() == 2 )
	{
		startspot = NULL;
	}
	else
	{
		Q_strncpy( _startspot, Cmd_Argv( 2 ), sizeof( _startspot ) - 1 );
		_startspot[ sizeof( _startspot ) - 1 ] = 0;

		if ( !_startspot[0] )
			startspot = NULL;
		else
			startspot = _startspot;
	}

	Q_strncpy( oldlevel, sv.name, sizeof( oldlevel ) - 1 );
	oldlevel[ sizeof( oldlevel ) - 1 ] = 0;

	pSaveData = SaveGamestate();
	SV_ServerShutdown();
	FS_LogLevelLoadStarted( level );

	if ( !SV_SpawnServer( false, level, startspot ) )
	{
		Sys_Error( "Host_Changelevel2: Couldn't load map %s\n", level );
		return;
	}

	SaveExit( pSaveData );

	// Try to restore the new level
	if ( !LoadGamestate( level, 0 ) )
	{
		newUnit = true;
		SV_LoadEntities();
	}

	LoadAdjacentEntities( oldlevel, startspot );

	sv.paused = true;		// pause until all clients connect
	sv.loadgame = true;
	gGlobalVariables.time = sv.time;

	if ( newUnit && sv_newunit.value )
		Host_ClearSaveDirectory();

	SV_ActivateServer( 0 );
}

/*
==================
Host_Version_f

==================
*/
void Host_Version_f( void )
{
	Con_Printf( "Protocol version %i\nExe version %s (%s)\n", PROTOCOL_VERSION, gpszVersionString, gpszProductString );
	Con_Printf( "Exe build: 18:03:54 Apr 21 2013 (%i)\n", build_number() );
}

/*
==================
Host_FullInfo_f

Allow clients to change userinfo
==================
*/
void Host_FullInfo_f( void )
{
	char	key[512];
	char	value[512];
	char	*o;
	char	*s;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "fullinfo <complete info string>\n" );
		return;
	}

	s = Cmd_Argv( 1 );
	if ( *s == '\\' )
		s++;

	while ( *s )
	{
		o = key;
		while ( *s && *s != '\\' )
			*o++ = *s++;
		*o = 0;

		if ( !*s )
		{
			Con_Printf( "MISSING VALUE\n" );
			return;
		}

		o = value;
		s++;
		while ( *s && *s != '\\' )
			*o++ = *s++;
		*o = 0;

		if ( *s )
			s++;

		if ( cmd_source == src_command )
		{
			Info_SetValueForKey( cls.userinfo, key, value, MAX_INFO_STRING );
			Cmd_ForwardToServer();
			return;
		}

		Info_SetValueForKey( host_client->userinfo, key, value, MAX_INFO_STRING );
		host_client->sendinfo = true;
	}
}

/*
==================
Host_KillVoice_f

==================
*/
void Host_KillVoice_f( void )
{
	Voice_Deinit();
}

/*
==================
Host_SetInfo_f

Allow clients to change userinfo
==================
*/
void Host_SetInfo_f( void )
{
	if ( Cmd_Argc() == 1 )
	{
		Info_Print( cls.userinfo );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: setinfo [ <key> <value> ]\n" );
		return;
	}

	if ( cmd_source == src_command )
	{
		Info_SetValueForKey( cls.userinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_INFO_STRING );
		Cmd_ForwardToServer();
		return;
	}

	Info_SetValueForKey( host_client->userinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_INFO_STRING );
	host_client->sendinfo = true;
}

/*
==================
Host_Say

==================
*/
void Host_Say( qboolean teamonly )
{
	client_t	*client;
	client_t	*save;
	int		j;
	char	*p;
	char	text[128];
	qboolean	fromServer = false;

	if ( cls.state != ca_dedicated )
	{
		if ( cmd_source != src_command )
			return;

		Cmd_ForwardToServer();
		return;
	}

	if ( Cmd_Argc() < 2 )
		return;

	p = Cmd_Args();
	if ( !p )
		return;

	save = host_client;

	// remove quotes if present
	if ( *p == '"' )
	{
		p++;
		p[Q_strlen( p ) - 1] = 0;
	}

	_snprintf( text, sizeof( text ), "%c<%s> ", 1, Cvar_VariableString( "hostname" ) );

	// make sure the text is not too long
	if ( Q_strlen( p ) > 63 )
		p[63] = 0;

	j = sizeof( text ) - 2 - Q_strlen( text );	// -2 for /n and null terminator
	if ( Q_strlen( p ) > j )
		p[j] = 0;

	Q_strcat( text, p );
	Q_strcat( text, "\n" );

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client || !client->active || !client->spawned )
			continue;

		if ( client->fakeclient )
			continue;

		host_client = client;

		PF_MessageBegin_I( MSG_ONE, RegUserMsg( "SayText", -1 ), NULL, &sv.edicts[j + 1] );
		PF_WriteByte_I( 0 );
		PF_WriteString_I( text );
		PF_MessageEnd_I();
	}

	host_client = save;

	Sys_Printf( "%s", &text[1] );

	Log_Printf( "Server say \"%s\"\n", p );
}

/*
==================
Host_Say_f

==================
*/
void Host_Say_f( void )
{
	Host_Say( false );
}

/*
==================
Host_Say_Team_f

==================
*/
void Host_Say_Team_f( void )
{
	Host_Say( true );
}

/*
==================
Host_Tell_f

==================
*/
void Host_Tell_f( void )
{
	client_t	*client;
	client_t	*save;
	int		j;
	char	*p;
	char	text[64];
	char	*tellmsg;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( Cmd_Argc() < 3 )
		return;

	p = Cmd_Args();
	if ( !p )
		return;

	_snprintf( text, sizeof( text ), "%s TELL: ", host_client->name );

	if ( *p == '"' )
	{
		p++;
		p[Q_strlen( p ) - 1] = 0;
	}

	// check length & truncate if necessary
	j = sizeof( text ) - 2 - Q_strlen( text );	// -2 for /n and null terminator
	if ( Q_strlen( p ) > j )
		p[j] = 0;

	tellmsg = Q_strstr( p, Cmd_Argv( 1 ) );
	if ( tellmsg )
		Q_strcat( text, tellmsg + Q_strlen( Cmd_Argv( 1 ) ) );
	else
		Q_strcat( text, p );

	Q_strcat( text, "\n" );

	save = host_client;

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active || !client->spawned )
			continue;

		if ( client->fakeclient )
			continue;

		if ( Q_strcasecmp( client->name, Cmd_Argv( 1 ) ) )
			continue;

		host_client = client;

		PF_MessageBegin_I( MSG_ONE, RegUserMsg( "SayText", -1 ), NULL, &sv.edicts[j + 1] );
		PF_WriteByte_I( 0 );
		PF_WriteString_I( text );
		PF_MessageEnd_I();
		break;
	}

	host_client = save;
}

/*
==================
Host_Kill_f

==================
*/
void Host_Kill_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( sv_player->v.health <= 0 )
	{
		SV_ClientPrintf( "Can't suicide -- already dead!\n" );
		return;
	}

	gGlobalVariables.time = sv.time;
	gEntityInterface.pfnClientKill( sv_player );
}

/*
==================
Host_TogglePause_f

==================
*/
void Host_TogglePause_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
	{
		SV_ClientPrintf( "Pause not allowed.\n" );
		return;
	}

	sv.paused ^= 1;

	if ( sv.paused )
		SV_BroadcastPrintf( "%s paused the game\n", pr_strings + sv_player->v.netname );
	else
		SV_BroadcastPrintf( "%s unpaused the game\n", pr_strings + sv_player->v.netname );

	// send notification to all clients
	MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
	MSG_WriteByte( &sv.reliable_datagram, sv.paused );
}

/*
==================
Host_Pause_f

==================
*/
void Host_Pause_f( void )
{
	if ( !cl.levelname[0] )
		return;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
		return;

	sv.paused = true;

	// send notification to all clients
	MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
	MSG_WriteByte( &sv.reliable_datagram, sv.paused );
}

/*
==================
Host_Unpause_f

==================
*/
void Host_Unpause_f( void )
{
	if ( !cl.levelname[0] )
		return;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( !pausable.value )
		return;

	sv.paused = false;

	// send notification to all clients
	MSG_WriteByte( &sv.reliable_datagram, svc_setpause );
	MSG_WriteByte( &sv.reliable_datagram, sv.paused );
}

//===========================================================================

/*
==================
FindViewthing

==================
*/
edict_t *FindViewthing( void )
{
	int		i;
	edict_t	*e;

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		e = &sv.edicts[i];
		if ( !Q_strcmp( pr_strings + e->v.classname, "viewthing" ) )
			return e;
	}

	Con_Printf( "No viewthing on map\n" );
	return NULL;
}

/*
==================
Host_Viewmodel_f

==================
*/
void Host_Viewmodel_f( void )
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing();
	if ( !e )
		return;

	m = Mod_ForName( Cmd_Argv( 1 ), false, false );
	if ( !m )
	{
		Con_Printf( "Can't load %s\n", Cmd_Argv( 1 ) );
		return;
	}

	e->v.frame = 0;
	cl.model_precache[ (int)e->v.modelindex ] = m;
}

/*
==================
Host_Viewframe_f

==================
*/
void Host_Viewframe_f( void )
{
	edict_t	*e;
	int		f;
	model_t	*m;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	f = Q_atoi( Cmd_Argv( 1 ) );
	if ( f >= m->numframes )
		f = m->numframes - 1;

	e->v.frame = f;
}

/*
==================
PrintFrameName

==================
*/
void PrintFrameName( model_t *m, int frame )
{
	aliashdr_t	*hdr;
	maliasframedesc_t	*pframedesc;

	hdr = (aliashdr_t *)Mod_Extradata( m );
	if ( !hdr )
		return;

	pframedesc = &hdr->frames[frame];

	Con_Printf( "frame %i: %s\n", frame, pframedesc->name );
}

/*
==================
Host_Viewnext_f

==================
*/
void Host_Viewnext_f( void )
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	e->v.frame = e->v.frame + 1;
	if ( e->v.frame >= m->numframes )
		e->v.frame = m->numframes - 1;

	PrintFrameName( m, e->v.frame );
}

/*
==================
Host_Viewprev_f

==================
*/
void Host_Viewprev_f( void )
{
	edict_t	*e;
	model_t	*m;

	e = FindViewthing();
	if ( !e )
		return;

	m = CL_GetModelByIndex( e->v.modelindex );

	e->v.frame = e->v.frame - 1;
	if ( e->v.frame < 0 )
		e->v.frame = 0;

	PrintFrameName( m, e->v.frame );
}

/*
==================
Host_Interp_f

==================
*/
void Host_Interp_f( void )
{
	r_dointerp = !r_dointerp;

	if ( !r_dointerp )
		Con_Printf( "Frame Interpolation OFF\n" );
	else
		Con_Printf( "Frame Interpolation ON\n" );
}

/*
===============================================================================

DEMO LOOP CONTROL

===============================================================================
*/

/*
=====================
Host_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void Host_NextDemo( void )
{
	char	str[1024];

	if ( cls.demonum == -1 )
		return;		// don't play demos

	SCR_BeginLoadingPlaque( false );

	if ( !cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS )
	{
		cls.demonum = 0;
		if ( !cls.demos[cls.demonum][0] )
		{
			Con_Printf( "No demos listed with startdemos\n" );
			cls.demonum = -1;
			return;
		}
	}

	_snprintf( str, sizeof( str ), "playdemo %s\n", cls.demos[cls.demonum] );
	Cbuf_InsertText( str );
	cls.demonum++;
}

/*
==================
Host_Startdemos_f

==================
*/
void Host_Startdemos_f( void )
{
	int		i, c;

	if ( cls.state == ca_dedicated )
	{
		if ( !sv.active )
			Con_Printf( "Cannot play demos on a dedicated server.\n" );
		return;
	}

	c = Cmd_Argc() - 1;
	if ( c > MAX_DEMOS )
	{
		Con_Printf( "Max %i demos in demoloop\n", MAX_DEMOS );
		c = MAX_DEMOS;
	}
	Con_Printf( "%i demo(s) in loop\n", c );

	for ( i = 1; i < c + 1; i++ )
	{
		Q_strncpy( cls.demos[i - 1], Cmd_Argv( i ), sizeof( cls.demos[0] ) - 1 );
		cls.demos[i - 1][ sizeof( cls.demos[0] ) - 1 ] = 0;
	}

	if ( !sv.active && cls.demonum != -1 && !cls.demoplayback )
	{
		cls.demonum = 0;
		Host_NextDemo();
	}
	else
	{
		cls.demonum = -1;
	}
}

/*
==================
Host_Demos_f

Return to looping demos
==================
*/
void Host_Demos_f( void )
{
	if ( cls.state == ca_dedicated )
		return;

	if ( cls.demonum == -1 )
		cls.demonum = 0;

	CL_Disconnect_f();
	Host_NextDemo();
}

/*
==================
Host_Stopdemo_f

Return to looping demos
==================
*/
void Host_Stopdemo_f( void )
{
	if ( cls.state == ca_dedicated )
		return;

	if ( !cls.demoplayback )
		return;

	CL_StopPlayback();
	CL_Disconnect();
}

/*
==================
Host_EndSection

==================
*/
void Host_EndSection( const char *pszSection )
{
	giActive = DLL_PAUSED;
	giSubState = 1;
	giStateInfo = 1;

	if ( !pszSection || !*pszSection )
		Con_Printf( " endsection with no arguments\n" );
	else if ( !Q_stricmp( pszSection, "_oem_end_training" ) )
		giStateInfo = 1;
	else if ( !Q_stricmp( pszSection, "_oem_end_logo" ) )
		giStateInfo = 2;
	else if ( !Q_stricmp( pszSection, "_oem_end_demo" ) )
		giStateInfo = 3;
	else
		Con_DPrintf( " endsection with unknown Section keyvalue\n" );

	Cbuf_AddText( "\ndisconnect\n" );
}

/*
==================
Host_Soundfade_f

==================
*/
void Host_Soundfade_f( void )
{
	int		percent;
	int		inTime, holdTime, outTime;

	if ( Cmd_Argc() != 3 && Cmd_Argc() != 5 )
	{
		Con_Printf( "soundfade <percent> <hold> [<out> <int>]\n" );
		return;
	}

	percent = Q_atoi( Cmd_Argv( 1 ) );
	percent = min( 100, percent );
	percent = max( 0, percent );

	holdTime = Q_atoi( Cmd_Argv( 2 ) );
	if ( holdTime > 255 )
		holdTime = 255;

	inTime = 0;
	outTime = 0;

	if ( Cmd_Argc() == 5 )
	{
		outTime = Q_atoi( Cmd_Argv( 3 ) );
		if ( outTime > 255 )
			outTime = 255;

		inTime = Q_atoi( Cmd_Argv( 4 ) );
		if ( inTime > 255 )
			inTime = 255;
	}

	cls.soundfade.soundFadeStartTime = realtime;
	cls.soundfade.nStartPercent = percent;
	cls.soundfade.soundFadeOutTime = outTime;
	cls.soundfade.soundFadeHoldTime = holdTime;
	cls.soundfade.soundFadeInTime = inTime;
}

/*
==================
Host_KillServer_f

==================
*/
void Host_KillServer_f( void )
{
	if ( cls.state != ca_dedicated )
	{
		CL_Disconnect_f();
		return;
	}

	if ( sv.active )
		Host_ShutdownServer( false );

	if ( cls.state != ca_dedicated )
		NET_Config( false );
}

/*
==================
Host_VoiceRecordStart_f

==================
*/
void Host_VoiceRecordStart_f( void )
{
	if ( cls.state == ca_active )
	{
		const char	*pUncompressedFile = NULL;
		const char	*pDecompressedFile = NULL;
		const char	*pInputFile = NULL;

		if ( (int)voice_recordtofile.value )
		{
			pUncompressedFile = "voice_micdata.wav";
			pDecompressedFile = "voice_decompressed.wav";
		}

		if ( (int)voice_inputfromfile.value )
			pInputFile = "voice_input.wav";

		Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
	}
}

/*
==================
Host_VoiceRecordStop_f

==================
*/
void Host_VoiceRecordStop_f( void )
{
	if ( cls.state == ca_active )
	{
		if ( Voice_IsRecording() )
		{
			CL_AddVoiceToDatagram( true );
			Voice_RecordStop();
		}
	}
}

/*
==================
Host_SetRenderer_f

==================
*/
void Host_SetRenderer_f( void )
{
	int		hardware = 1;
	int		windowed = 0;

	if ( cls.state == ca_dedicated )
		return;

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 3 )
		return;

	if ( !stricmp( Cmd_Argv( 1 ), "software" ) )
		hardware = 0;
	else if ( !stricmp( Cmd_Argv( 1 ), "gl" ) )
		hardware = 1;
	else if ( !stricmp( Cmd_Argv( 1 ), "d3d" ) )
		hardware = 2;

	if ( Cmd_Argc() == 3 && !stricmp( Cmd_Argv( 2 ), "windowed" ) )
		windowed = 1;

	VideoMode_SwitchMode( hardware, windowed );
}

/*
==================
Host_SetVideoMode_f

==================
*/
void Host_SetVideoMode_f( void )
{
	int		w, h, bpp = 16;

	if ( cls.state == ca_dedicated )
		return;

	if ( Cmd_Argc() != 3 && Cmd_Argc() != 4 )
		return;

	w = Q_atoi( Cmd_Argv( 1 ) );
	h = Q_atoi( Cmd_Argv( 2 ) );

	if ( Cmd_Argc() == 4 )
		bpp = Q_atoi( Cmd_Argv( 3 ) );

	VideoMode_SetVideoMode( w, h, bpp );
}

/*
==================
Host_SetGameDir_f

==================
*/
void Host_SetGameDir_f( void )
{
	if ( Cmd_Argc() < 1 )
		return;

	if ( !g_pPostRestartCmdLineArgs )
		return;

	strcat( g_pPostRestartCmdLineArgs, " -game " );
	strcat( g_pPostRestartCmdLineArgs, Cmd_Argv( 1 ) );
}

/*
==================
Host_Crash_f

==================
*/
void Host_Crash_f( void )
{
	int		*p = NULL;

	*p = 0xdeadbeef;
}

/*
==================
Host_InitCommands

==================
*/
void Host_InitCommands( void )
{
	Cmd_AddCommand( "cd", CD_Command_f );
	Cmd_AddCommand( "mp3", MP3_Command_f );
	Cmd_AddCommand( "_careeraudio", CareerAudio_Command_f );
	Cmd_AddCommand( "shutdownserver", Host_KillServer_f );
	Cmd_AddCommand( "soundfade", Host_Soundfade_f );
	Cmd_AddCommand( "status", Host_Status_f );
	Cmd_AddCommand( "stat", Host_Status_Formatted_f );
	Cmd_AddCommand( "quit", Host_Quit_f );
	Cmd_AddCommand( "_restart", Host_Quit_Restart_f );
	Cmd_AddCommand( "_setrenderer", Host_SetRenderer_f );
	Cmd_AddCommand( "_setvideomode", Host_SetVideoMode_f );
	Cmd_AddCommand( "_setgamedir", Host_SetGameDir_f );
	Cmd_AddCommand( "exit", Host_Quit_f );
	Cmd_AddCommand( "map", Host_Map_f );
	Cmd_AddCommand( "career", Host_Career_f );
	Cmd_AddCommand( "maps", Host_Maps_f );
	Cmd_AddCommand( "restart", Host_Restart_f );
	Cmd_AddCommand( "reload", Host_Reload_f );
	Cmd_AddCommand( "changelevel", Host_Changelevel_f );
	Cmd_AddCommand( "changelevel2", Host_Changelevel2_f );
	Cmd_AddCommand( "reconnect", Host_Reconnect_f );
	Cmd_AddCommand( "version", Host_Version_f );
	Cmd_AddCommand( "say", Host_Say_f );
	Cmd_AddCommand( "say_team", Host_Say_Team_f );
	Cmd_AddCommand( "tell", Host_Tell_f );
	Cmd_AddCommand( "kill", Host_Kill_f );
	Cmd_AddCommand( "pause", Host_TogglePause_f );
	Cmd_AddCommand( "setpause", Host_Pause_f );
	Cmd_AddCommand( "unpause", Host_Unpause_f );
	Cmd_AddCommand( "kick", Host_Kick_f );
	Cmd_AddCommand( "ping", Host_Ping_f );
	Cmd_AddCommand( "motd", Host_Motd_f );
	Cmd_AddCommand( "motd_write", Host_Motd_Write_f );
	Cmd_AddCommand( "stats", Host_Stats_f );
	Cmd_AddCommand( "load", Host_Loadgame_f );
	Cmd_AddCommand( "save", Host_Savegame_f );
	Cmd_AddCommand( "writecfg", Host_WriteCustomConfig );
	Cmd_AddCommand( "+voicerecord", Host_VoiceRecordStart_f );
	Cmd_AddCommand( "-voicerecord", Host_VoiceRecordStop_f );
	Cmd_AddCommand( "startdemos", Host_Startdemos_f );
	Cmd_AddCommand( "demos", Host_Demos_f );
	Cmd_AddCommand( "stopdemo", Host_Stopdemo_f );
	Cmd_AddCommand( "setinfo", Host_SetInfo_f );
	Cmd_AddCommand( "fullinfo", Host_FullInfo_f );
	Cmd_AddCommand( "viewmodel", Host_Viewmodel_f );
	Cmd_AddCommand( "viewframe", Host_Viewframe_f );
	Cmd_AddCommand( "viewnext", Host_Viewnext_f );
	Cmd_AddCommand( "viewprev", Host_Viewprev_f );
	Cmd_AddCommand( "mcache", Mod_Print );
	Cmd_AddCommand( "interp", Host_Interp_f );
	Cmd_AddCommand( "setmaster", Master_SetMaster_f );
	Cmd_AddCommand( "heartbeat", Master_Heartbeat_f );

	Cvar_RegisterVariable( &gHostMap );
	Cvar_RegisterVariable( &voice_recordtofile );
	Cvar_RegisterVariable( &voice_inputfromfile );
}

/*
==================
SV_CheckBlendingInterface

==================
*/
void SV_CheckBlendingInterface( void )
{
	int		i = 0;
	SV_BLENDING_INTERFACE_FUNC	studio_interface;

	R_ResetSvBlending();

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		studio_interface = (SV_BLENDING_INTERFACE_FUNC)GetProcAddress( (HMODULE)g_rgextdll[i].lDLLHandle, "Server_GetBlendingInterface" );
		if ( studio_interface )
		{
			if ( !studio_interface( SV_BLENDING_INTERFACE_VERSION, &g_pSvBlendingAPI, &server_studio_api, (float *)rotationmatrix, (float *)bonetransform ) )
			{
				Con_DPrintf( "Couldn't get server .dll studio model blending interface. Version mismatch?\n" );
				R_ResetSvBlending();
			}
			else
			{
				break;
			}
		}
	}
}

/*
==================
SV_CheckSaveGameCommentInterface

==================
*/
void SV_CheckSaveGameCommentInterface( void )
{
	int		i = 0;
	const char	*pszFunction = "SV_SaveGameComment";
	SV_SAVEGAMECOMMENT_FUNC	pTemp = NULL;

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		pTemp = (SV_SAVEGAMECOMMENT_FUNC)GetProcAddress( (HMODULE)g_rgextdll[i].lDLLHandle, pszFunction );
		if ( pTemp )
		{
			g_pSaveGameCommentFunc = pTemp;
			break;
		}
	}
}
