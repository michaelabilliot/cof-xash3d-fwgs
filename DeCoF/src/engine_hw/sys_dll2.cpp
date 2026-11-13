// sys_dll2.cpp -- the launcher, dedicated server and GameUI interfaces of the engine

#include <windows.h>
#include <winsock.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eh.h>

#include "interface.h"
#include "../launcher/engine_launcher_api.h"
#include "../launcher/iregistry.h"
#include "../public/engine_hlds_api.h"
#include "../public/idedicatedexports.h"
#include "../public/igameuifuncs.h"
#include "iengine.h"
#include "igame.h"
#include "ivideomode.h"

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "host.h"
#include "keys.h"
#include "net.h"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "modinfo.h"
#include "vgui_int.h"
#include "qgl.h"
#include "winquake.h"

extern qboolean	g_bPrintingKeepAliveDots;
extern int		gD3DMode;
extern qboolean	gHasMMXTechnology;
extern char		*g_pPostRestartCmdLineArgs;

// sys_dll.c
extern qboolean			g_PerfCounterInitialized_guess;
extern CRITICAL_SECTION	g_PerfCounterMutex_guess;
extern int				lowshift;
extern double			pfreq;
extern void				( *Launcher_ConsolePrintf )( char *, ... );
extern void				( *VID_FlipScreen_guess )( void );

// sys_wina.s
void	MaskExceptions( void );
void	Sys_SetFPCW( void );
void	Sys_InitFPU_guess( void );

// traceinit.cpp
void	TraceInit( const char *i, const char *s, int widget );
void	TraceShutdown( const char *s, int widget );

// filesystem.cpp
int		FileSystem_Init( char *basedir, void *filesystemFactory );
void	FileSystem_Shutdown( void );

// GL_VIDNT.C
int		GL_SetMode( HWND mainwindow, HDC *pmaindc, HGLRC *pbaseRC, int fD3D, const char *pszDriver, const char *pszCmdLine );
void	GL_Shutdown( HWND hwnd, HDC hdc, HGLRC hglrc );

// cdll_int.c
struct kbutton_s	*ClientDLL_FindKey( const char *name );
void	ClientDLL_ActivateMouse( void );
void	ClientDLL_DeactivateMouse( void );
void	ClientDLL_ClearStates( void );

// sys_getmodes.cpp
void	VideoMode_Create( void );
void	VideoMode_GetVideoModes( struct vmode_s **liststart, int *count );
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
void	VideoMode_GetCurrentRenderer( char *name, int namelen, int *windowed );

// this file
void		SetRateRegistrySetting( const char *pchRate );
const char	*GetRateRegistrySetting( const char *pchDef );
__declspec( dllexport ) void F( IEngineAPI **api );
void		Sys_VID_FlipScreen( void );
int			Sys_GetSurfaceCacheSize( int width, int height );
void		Legacy_ErrorMessage( int nLevel, const char *pszErrorMessage );
void		Legacy_Sys_Printf( char *fmt, ... );
void		Legacy_MP3subsys_Suspend_Audio( void );
void		Legacy_MP3subsys_Resume_Audio( void );
void		Sys_SetupLegacyAPIs( void );
int			Sys_IsWin95( void );
int			Sys_IsWin98( void );
void		Sys_CheckOSVersion( void );
void		Sys_Shutdown( void );
void		Sys_InitArgv( char *lpCmdLine );
void		Sys_ShutdownArgv( void );
void		Sys_InitMemory( void );
void		Sys_ShutdownMemory( void );
void		Sys_InitLauncherInterface( void );
void		Sys_ShutdownLauncherInterface( void );
void		Sys_InitAuthentication( void );
void		Sys_ShutdownAuthentication( void );
void		Sys_ShowProgressTicks( char *specialProgressMsg );
int			Sys_InitGame( char *lpOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated );
void		Sys_ShutdownGame( void );
void		ClearIOStates( void );
void		Sys_PlayStartupVideos_guess( void );
int			RunListenServer( void *instance, char *basedir, char *cmdline, char *postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory );
void		MiniDumpFunction_guess( unsigned int nExceptionCode, EXCEPTION_POINTERS *pException );
int			BuildMapCycleListHints( char **hints );

extern int	( *D_SurfaceCacheForRes )( int width, int height );
extern HGLRC	baseRC;
extern HDC		maindc;
extern qboolean	g_bIsWin95;
extern qboolean	g_bIsWin98;
extern char	*szCommonPreloads;
extern char	*szReslistsBaseDir;
extern char	*szReslistsExt;
}

#include "steam_api_c.h"

// procinfo.cpp
int		PROC_IsMMX( void );

// BaseUISurface.cpp
vgui2::KeyCode	GetVGUI2KeyCodeForBind( const char *bind );

#define MAX_NUM_ARGVS	50

static IDedicatedExports	*dedicated;

int		( *D_SurfaceCacheForRes )( int width, int height );

const char *GetCurrentSteamAppName( void )
{
	if ( !_stricmp( com_gamedir, "cstrike" ) || !_stricmp( com_gamedir, "cstrike_beta" ) )
		return "Counter-Strike";
	else if ( !_stricmp( com_gamedir, "valve" ) )
		return "Half-Life";
	else if ( !_stricmp( com_gamedir, "ricochet" ) )
		return "Ricochet";
	else if ( !_stricmp( com_gamedir, "dod" ) )
		return "Day of Defeat";
	else if ( !_stricmp( com_gamedir, "tfc" ) )
		return "Team Fortress Classic";
	else if ( !_stricmp( com_gamedir, "dmc" ) )
		return "Deathmatch Classic";
	else if ( !_stricmp( com_gamedir, "czero" ) )
		return "Condition Zero";
	else if ( !_stricmp( com_gamedir, "cryoffear" ) )
		return "Cry of Fear";

	return "Cry of Fear";
}

void SetRateRegistrySetting( const char *pchRate )
{
	registry->WriteString( "rate", pchRate );
}

const char *GetRateRegistrySetting( const char *pchDef )
{
	return registry->ReadString( "rate", pchDef );
}

__declspec( dllexport ) void F( IEngineAPI **api )
{
	*api = (IEngineAPI *)Sys_GetFactoryThis()( VENGINE_LAUNCHER_API_VERSION, NULL );
}

HGLRC	baseRC;
HDC		maindc;

void Sys_VID_FlipScreen( void )
{
	qwglSwapBuffers( maindc );
}

int Sys_GetSurfaceCacheSize( int width, int height )
{
	int size, pix;
	int i;

	if ( i = COM_CheckParm( "-surfcachesize" ) )
	{
		size = atoi( com_argv[ i + 1 ] ) * 1024;
		return size;
	}

	size = 0x300000;

	pix = width * height * 2;
	if ( pix > 64000 )
		size += ( pix - 64000 ) * 3;

	return size;
}

void Sys_GetCDKey( char *pszCDKey, int *nLength, int *bDedicated )
{
	char key[65];
	char hostname[MAX_PATH];

	if ( !gethostname( hostname, sizeof( hostname ) ) )
	{
		struct hostent *hostinfo;

		hostinfo = gethostbyname( hostname );
		if ( hostinfo && hostinfo->h_length == 4 && *hostinfo->h_addr_list )
		{
			_snprintf( key, sizeof( key ), "%u.%u.%u.%u",
				(unsigned char)( *hostinfo->h_addr_list )[0],
				(unsigned char)( *hostinfo->h_addr_list )[1],
				(unsigned char)( *hostinfo->h_addr_list )[2],
				(unsigned char)( *hostinfo->h_addr_list )[3] );
			key[64] = 0;
		}
	}

	strcpy( pszCDKey, key );

	if ( nLength )
		*nLength = strlen( key );

	if ( bDedicated )
		*bDedicated = 0;
}

void Legacy_ErrorMessage( int nLevel, const char *pszErrorMessage )
{
}

void Legacy_Sys_Printf( char *fmt, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start( argptr, fmt );
	_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( dedicated )
		dedicated->Sys_Printf( text );
}

void Legacy_MP3subsys_Suspend_Audio( void )
{
}

void Legacy_MP3subsys_Resume_Audio( void )
{
}

void Sys_SetupLegacyAPIs( void )
{
	VID_FlipScreen_guess = Sys_VID_FlipScreen;
	D_SurfaceCacheForRes = Sys_GetSurfaceCacheSize;
	Launcher_ConsolePrintf = Legacy_Sys_Printf;
}

qboolean	g_bIsWin95;
qboolean	g_bIsWin98;

int Sys_IsWin95( void )
{
	return g_bIsWin95;
}

int Sys_IsWin98( void )
{
	return g_bIsWin98;
}

void Sys_CheckOSVersion( void )
{
	OSVERSIONINFO	vinfo;

	memset( &vinfo, 0, sizeof( vinfo ) );
	vinfo.dwOSVersionInfoSize = sizeof( vinfo );

	if ( !GetVersionEx( &vinfo ) )
		Sys_Error( "Couldn't get OS info" );

	if ( vinfo.dwMajorVersion < 4 )
		WinNT = FALSE;
	else
		WinNT = TRUE;

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion == 0 )
	{
		g_bIsWin95 = TRUE;
	}
	else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && vinfo.dwMajorVersion == 4 && vinfo.dwMinorVersion > 0 && vinfo.dwMinorVersion < 90 )
	{
		g_bIsWin98 = TRUE;
	}
}

void Sys_Init( void )
{
	LARGE_INTEGER	PerformanceFreq;
	unsigned int	lowpart, highpart;

	if ( !g_PerfCounterInitialized_guess )
	{
		InitializeCriticalSection( &g_PerfCounterMutex_guess );
		g_PerfCounterInitialized_guess = 1;
	}

	MaskExceptions();
	Sys_SetFPCW();

	if ( !QueryPerformanceFrequency( &PerformanceFreq ) )
		Sys_Error( "No hardware timer available" );

// get 32 out of the 64 time bits such that we have around
// 1 microsecond resolution
	lowpart = (unsigned int)PerformanceFreq.LowPart;
	highpart = (unsigned int)PerformanceFreq.HighPart;
	lowshift = 0;

	while ( highpart || ( lowpart > 2000000.0 ) )
	{
		lowshift++;
		lowpart >>= 1;
		lowpart |= ( highpart & 1 ) << 31;
		highpart >>= 1;
	}

	pfreq = 1.0 / (double)lowpart;

	Sys_CheckOSVersion();
	Sys_InitFPU_guess();

	Sys_InitFloatTime();
}

void Sys_Shutdown( void )
{
	Sys_ShutdownFloatTime();
	Steam_ShutdownClient();

	if ( g_PerfCounterInitialized_guess )
	{
		DeleteCriticalSection( &g_PerfCounterMutex_guess );
		g_PerfCounterInitialized_guess = 0;
	}

	GL_Shutdown( *pmainwindow, maindc, baseRC );
}

static char	*empty_string = "";
static char	*argv[MAX_NUM_ARGVS];

void Sys_InitArgv( char *lpCmdLine )
{
	host_parms.argc = 1;
	argv[0] = empty_string;

	while ( *lpCmdLine && ( host_parms.argc < MAX_NUM_ARGVS ) )
	{
		while ( *lpCmdLine && ( ( *lpCmdLine <= 32 ) || ( *lpCmdLine > 126 ) ) )
			lpCmdLine++;

		if ( *lpCmdLine )
		{
			argv[host_parms.argc] = lpCmdLine;
			host_parms.argc++;

			while ( *lpCmdLine && ( ( *lpCmdLine > 32 ) && ( *lpCmdLine <= 126 ) ) )
				lpCmdLine++;

			if ( *lpCmdLine )
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	host_parms.argv = argv;

	COM_InitArgv( host_parms.argc, host_parms.argv );

	host_parms.argc = com_argc;
	host_parms.argv = com_argv;
}

void Sys_ShutdownArgv( void )
{
}

void Sys_InitMemory( void )
{
	int i;
	bool bDidDefault;

	i = COM_CheckParm( "-heapsize" );
	if ( i && i < com_argc - 1 )
		host_parms.memsize = atoi( com_argv[ i + 1 ] ) * 1024;

	bDidDefault = false;

	if ( host_parms.memsize < 0xE00000 )
	{
		MEMORYSTATUS lpBuffer;

		lpBuffer.dwLength = sizeof( MEMORYSTATUS );
		GlobalMemoryStatus( &lpBuffer );

		bDidDefault = true;

		if ( lpBuffer.dwTotalPhys > 0 && lpBuffer.dwTotalPhys < 0xF00000 )
			Sys_Error( "Available memory less than 15MB!!! %i\n", host_parms.memsize );

		if ( lpBuffer.dwTotalPhys <= 0 )
			host_parms.memsize = 0x8000000;
		else
			host_parms.memsize = lpBuffer.dwTotalPhys >> 1;
	}

	if ( host_parms.memsize < 0xE00000 )
		host_parms.memsize = 0xE00000;

	if ( !g_bIsDedicatedServer && host_parms.memsize > 0x8000000 )
		host_parms.memsize = 0x8000000;

	if ( g_bIsDedicatedServer && bDidDefault )
		host_parms.memsize = 0x2800000;

	if ( g_bIsDedicatedServer && host_parms.memsize > 0x8000000 )
		host_parms.memsize = 0x8000000;

	if ( COM_CheckParm( "-minmemory" ) )
		host_parms.memsize = 0xE00000;

	host_parms.membase = GlobalAlloc( GMEM_FIXED, host_parms.memsize );
	if ( !host_parms.membase )
		Sys_Error( "Unable to allocate %.2f MB\n", host_parms.memsize / ( 1024.0 * 1024.0 ) );
}

void Sys_ShutdownMemory( void )
{
	GlobalFree( host_parms.membase );
	host_parms.membase = NULL;
	host_parms.memsize = 0;
}

HINSTANCE	g_hinstOpenGlEarly;

void Sys_InitLauncherInterface( void )
{
	gHasMMXTechnology = PROC_IsMMX();
	g_hinstOpenGlEarly = GetModuleHandle( "opengl32.dll" );
	Sys_SetupLegacyAPIs();
}

void Sys_ShutdownLauncherInterface( void )
{
}

void Sys_InitAuthentication( void )
{
	Sys_Printf( "STEAM Auth Server\r\n" );
}

void Sys_ShutdownAuthentication( void )
{
}

static double	g_flLastSteamProgressUpdateTime;

void Sys_ShowProgressTicks( char *specialProgressMsg )
{
	static bool recursionGuard = false;
	static int numTics = 0;
	double currentTime;
	int numTicsToPrint;
	int i;
	char msg[128];

	if ( recursionGuard )
		return;

	recursionGuard = true;

	if ( !COM_CheckParm( "-steam" ) )
	{
		recursionGuard = false;
		return;
	}

	currentTime = Sys_FloatTime();
	if ( g_flLastSteamProgressUpdateTime + 2.0 > currentTime )
	{
		recursionGuard = false;
		return;
	}

	g_flLastSteamProgressUpdateTime = currentTime;
	numTics++;

	if ( g_bIsDedicatedServer )
	{
		if ( g_bMajorMapChange )
		{
			g_bPrintingKeepAliveDots = true;
			Sys_Printf( "." );
		}
	}
	else
	{
		strcpy( msg, "Updating game resources" );

		numTicsToPrint = numTics % 5 + 1;
		for ( i = 0; i < numTicsToPrint; i++ )
			strcat( msg, "." );

		SetLoadingProgressBarStatusText( msg );
	}

	recursionGuard = false;
}

int Sys_InitGame( char *lpOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated )
{
	char	szMod_guess[MAX_PATH];
	char	szPreloads_guess[MAX_PATH];

	host_initialized = FALSE;

	if ( !bIsDedicated )
	{
		pmainwindow = (HWND *)pwnd;
		videomode->UpdateWindowPosition();
	}

	g_bIsDedicatedServer = bIsDedicated;
	memset( &gmodinfo, 0, sizeof( modinfo_t ) );

	TraceInit( "Sys_Init()", "Sys_Shutdown()", 0 );
	Sys_Init();
	FS_LogLevelLoadStarted( "Launcher" );
	SeedRandomNumberGenerator();

	TraceInit( "Sys_InitMemory()", "Sys_ShutdownMemory()", 0 );
	Sys_InitMemory();

	TraceInit( "Sys_InitLauncherInterface()", "Sys_ShutdownLauncherInterface()", 0 );
	Sys_InitLauncherInterface();

	if ( !GL_SetMode( *pmainwindow, &maindc, &baseRC, gD3DMode, "opengl32.dll", lpOrgCmdLine ) )
		return 0;

	TraceInit( "Host_Init( &host_parms )", "Host_Shutdown()", 0 );
	Host_Init( &host_parms );
	if ( !host_initialized )
		return 0;

	COM_FileBase( com_gamedir, szMod_guess );

	if ( COM_CheckParm( "-steam" ) )
	{
		sprintf( szPreloads_guess, "reslists\\%s\\internal_preloads.lst", GetCurrentSteamAppName() );
		FS_HintResourceNeed( szPreloads_guess, 1 );
	}

	TraceInit( "Sys_InitAuthentication()", "Sys_ShutdownAuthentication()", 0 );
	Sys_InitAuthentication();

	if ( g_bIsDedicatedServer )
	{
		Host_InitializeGameDLL();
		NET_Config( TRUE );
	}
	else
	{
		ClientDLL_ActivateMouse();
	}

	return 1;
}

void Sys_ShutdownGame( void )
{
	if ( !g_bIsDedicatedServer )
		ClientDLL_DeactivateMouse();

	TraceShutdown( "Host_Shutdown()", 0 );
	Host_Shutdown();

	if ( g_bIsDedicatedServer )
		NET_Config( FALSE );

	TraceShutdown( "Sys_ShutdownLauncherInterface()", 0 );
	Sys_ShutdownLauncherInterface();

	TraceShutdown( "Sys_ShutdownAuthentication()", 0 );
	Sys_ShutdownAuthentication();

	TraceShutdown( "Sys_ShutdownMemory()", 0 );
	Sys_ShutdownMemory();

	TraceShutdown( "Sys_Shutdown()", 0 );
	Sys_Shutdown();
}

void ClearIOStates( void )
{
	int i;

	for ( i = 0; i < 256; i++ )
	{
		Key_Event( i, false );
	}

	Key_ClearStates();
	ClientDLL_ClearStates();
}

void Sys_PlayStartupVideos_guess( void )
{
	game->PlayStartupVideos();
}

int RunListenServer( void *instance, char *basedir, char *cmdline, char *postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	static char OrigCmd[1024];
	int result;
	MSG msg;

	result = 0;

	g_pPostRestartCmdLineArgs = postRestartCmdLineArgs;
	strcpy( OrigCmd, cmdline );

	TraceInit( "Sys_InitArgv( OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( OrigCmd );

	eng->SetQuitting( IEngine::QUIT_NOTQUITTING );
	registry->Init();
	Steam_InitClient();

	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	if ( !FileSystem_Init( basedir, (void *)filesystemFactory ) )
		return 0;

	VideoMode_Create();
	registry->WriteInt( "CrashInitializingVideoMode", 0 );

	if ( videomode->Init( instance ) )
	{
		registry->WriteInt( "CrashInitializingVideoMode", 0 );

		if ( game->Init( instance ) )
		{
			if ( eng->Load( false, basedir, cmdline ) )
			{
				while ( 1 )
				{
					while ( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
					{
						if ( !GetMessage( &msg, NULL, 0, 0 ) )
						{
							eng->SetQuitting( IEngine::QUIT_TODESKTOP );
							break;
						}

						TranslateMessage( &msg );
						DispatchMessage( &msg );
					}

					if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
					{
						if ( eng->GetQuitting() != IEngine::QUIT_TODESKTOP )
							result = 1;
						break;
					}

					eng->Frame();
				}

				eng->Unload();
			}
			else
			{
				result = 2;
			}

			game->Shutdown();
		}

		videomode->Shutdown();
	}
	else
	{
		result = 2;
	}

	TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();

	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	Sys_ShutdownArgv();

	return result;
}

void MiniDumpFunction_guess( unsigned int nExceptionCode, EXCEPTION_POINTERS *pException )
{
	char	comment[256];

	_snprintf( comment, sizeof( comment ), "hl: %s\n", com_gamedir );
	SteamAPI_SetMiniDumpComment( comment );
	SteamAPI_WriteMiniDump( nExceptionCode, pException, build_number() );
}

//-----------------------------------------------------------------------------
// Purpose: the interface the launcher runs the listen server through
//-----------------------------------------------------------------------------
class CEngineAPI : public IEngineAPI
{
public:
	virtual int		Run( void *instance, char *basedir, char *cmdline, char *postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory );
};

int CEngineAPI::Run( void *instance, char *basedir, char *cmdline, char *postRestartCmdLineArgs, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	int nRunResult = 0;
	_se_translator_function pfnOldTranslator_guess;

	// the date and time of this file's build, from __DATE__ and __TIME__
	if ( !strstr( cmdline, "-nobreakpad" ) )
		SteamAPI_UseBreakpadCrashHandler( va( "%d", build_number() ), "Apr  6 2013", "15:32:57", false, NULL, NULL );

	pfnOldTranslator_guess = _set_se_translator( MiniDumpFunction_guess );

	try
	{
		int nResult_guess = RunListenServer( instance, basedir, cmdline, postRestartCmdLineArgs, launcherFactory, filesystemFactory );
		nRunResult = nResult_guess;
	}
	catch ( ... )
	{
	}

	return nRunResult;
}

EXPOSE_SINGLE_INTERFACE( CEngineAPI, IEngineAPI, VENGINE_LAUNCHER_API_VERSION );

//-----------------------------------------------------------------------------
// Purpose: the interface the dedicated server launcher runs the engine through
//-----------------------------------------------------------------------------
class CDedicatedServerAPI : public IDedicatedServerAPI
{
private:
	char			m_OrigCmd[1024];

public:
	virtual bool	Init( char *basedir, char *cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory );
	virtual int		Shutdown( void );
	virtual bool	RunFrame( void );
	virtual void	AddConsoleText( char *text );
	virtual void	UpdateStatus( float *fps, int *nActive, int *nMaxPlayers, char *pszMap );
};

char	*szCommonPreloads = "multiplayer_preloads";
char	*szReslistsBaseDir = "reslists";
char	*szReslistsExt = ".lst";

int BuildMapCycleListHints( char **hints )
{
	char			szMap[262];
	unsigned int	length;
	char			*pFileList;
	char			szMod[MAX_PATH];
	char			cszMapCycleTxtFile[MAX_PATH];
	FileHandle_t	pFile;

	COM_FileBase( com_gamedir, szMod );

	// the mod directory is missing from the arguments (original bug)
	sprintf( cszMapCycleTxtFile, "%s\\%s", mapcyclefile.string );

	pFile = FS_Open( cszMapCycleTxtFile, "rb" );
	if ( !pFile )
	{
		Con_Printf( "Unable to open %s\n", cszMapCycleTxtFile );
		return 0;
	}

	sprintf( szMap, "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(), szCommonPreloads, szReslistsExt );

	*hints = (char *)malloc( strlen( szMap ) + 1 );
	if ( !*hints )
	{
		Con_Printf( "Unable to allocate memory for map cycle hints list" );
		return 0;
	}

	strcpy( *hints, szMap );

	length = FS_Size( pFile );
	if ( length )
	{
		pFileList = (char *)malloc( length );
		if ( pFileList )
		{
			if ( FS_Read( pFileList, length, 1, pFile ) == 1 )
			{
				while ( 1 )
				{
					pFileList = COM_Parse( pFileList );
					if ( strlen( com_token ) <= 0 )
						break;

					strncpy( szMap, com_token, sizeof( szMap ) - 1 );
					szMap[sizeof( szMap ) - 1] = 0;

					if ( COM_TokenWaiting( pFileList ) )
					{
						pFileList = COM_Parse( pFileList );
					}

					char	mapLine[sizeof( szMap )];

					_snprintf( mapLine, sizeof( mapLine ), "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(), szMap, szReslistsExt );

					*hints = (char *)realloc( *hints, strlen( *hints ) + strlen( mapLine ) + 2 );
					if ( !*hints )
					{
						Con_Printf( "Unable to reallocate memory for map cycle hints list" );
						return 0;
					}

					strcat( *hints, mapLine );
				}
			}

			free( pFileList );
		}
	}

	FS_Close( pFile );

	sprintf( szMap, "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName() );

	*hints = (char *)realloc( *hints, strlen( *hints ) + strlen( szMap ) + 2 );
	strcat( *hints, szMap );

	return 1;
}

bool CDedicatedServerAPI::Init( char *basedir, char *cmdline, CreateInterfaceFn launcherFactory, CreateInterfaceFn filesystemFactory )
{
	char	szCommand[256];

	dedicated = (IDedicatedExports *)launcherFactory( VENGINE_DEDICATEDEXPORTS_API_VERSION, NULL );
	if ( !dedicated )
		return false;

	strcpy( m_OrigCmd, cmdline );

	// the date and time of this file's build, from __DATE__ and __TIME__
	if ( !strstr( cmdline, "-nobreakpad" ) )
		SteamAPI_UseBreakpadCrashHandler( va( "%d", build_number() ), "Apr  6 2013", "15:32:57", false, NULL, NULL );

	TraceInit( "Sys_InitArgv( m_OrigCmd )", "Sys_ShutdownArgv()", 0 );
	Sys_InitArgv( m_OrigCmd );

	eng->SetQuitting( IEngine::QUIT_NOTQUITTING );
	registry->Init();

	g_bIsDedicatedServer = TRUE;

	TraceInit( "FileSystem_Init(basedir, (void *)filesystemFactory)", "FileSystem_Shutdown()", 0 );
	if ( !FileSystem_Init( basedir, (void *)filesystemFactory ) )
		return false;

	if ( game->Init( 0 ) && eng->Load( true, basedir, cmdline ) )
	{
		if ( COM_CheckParm( "-steam" ) )
		{
			char	*pszHints_guess = NULL;

			if ( BuildMapCycleListHints( &pszHints_guess ) )
				FS_HintResourceNeed( pszHints_guess, 1 );

			if ( pszHints_guess )
				free( pszHints_guess );
		}

		_snprintf( szCommand, sizeof( szCommand ) - 1, "exec %s\n", servercfgfile.string );
		szCommand[sizeof( szCommand ) - 1] = 0;
		Cbuf_InsertText( szCommand );
		return true;
	}

	return false;
}

int CDedicatedServerAPI::Shutdown( void )
{
	eng->Unload();
	game->Shutdown();

	TraceShutdown( "FileSystem_Shutdown()", 0 );
	FileSystem_Shutdown();
	registry->Shutdown();

	TraceShutdown( "Sys_ShutdownArgv()", 0 );
	Sys_ShutdownArgv();

	dedicated = NULL;
	return giActive;
}

bool CDedicatedServerAPI::RunFrame( void )
{
	if ( eng->GetQuitting() )
		return false;

	eng->Frame();
	return true;
}

void CDedicatedServerAPI::AddConsoleText( char *text )
{
	Cbuf_AddText( text );
}

void CDedicatedServerAPI::UpdateStatus( float *fps, int *nActive, int *nMaxPlayers, char *pszMap )
{
	Host_GetHostInfo( fps, nActive, NULL, nMaxPlayers, pszMap );
}

EXPOSE_SINGLE_INTERFACE( CDedicatedServerAPI, IDedicatedServerAPI, VENGINE_HLDS_API_VERSION );

//-----------------------------------------------------------------------------
// Purpose: the engine functions GameUI calls
//-----------------------------------------------------------------------------
class CGameUIFuncs : public IGameUIFuncs
{
public:
	virtual bool IsKeyDown( const char *keyname, bool &isdown )
	{
		kbutton_t *key = ClientDLL_FindKey( keyname );
		if ( !key )
			return false;

		isdown = ( key->state & 1 ) ? true : false;
		return true;
	}

	virtual const char *Key_NameForKey( int keynum )
	{
		return Key_KeynumToString( keynum );
	}

	virtual const char *Key_BindingForKey( int keynum )
	{
		return ::Key_BindingForKey( keynum );
	}

	virtual vgui2::KeyCode GetVGUI2KeyCodeForBind( const char *bind )
	{
		return ::GetVGUI2KeyCodeForBind( bind );
	}

	virtual void GetVideoModes( struct vmode_s **liststart, int *count )
	{
		VideoMode_GetVideoModes( liststart, count );
	}

	virtual void GetCurrentVideoMode( int *wide, int *tall, int *bpp )
	{
		VideoMode_GetCurrentVideoMode( wide, tall, bpp );
	}

	virtual void GetCurrentRenderer( char *name, int namelen, int *windowed )
	{
		VideoMode_GetCurrentRenderer( name, namelen, windowed );
	}

	virtual bool IsConnectedToVACSecureServer( void )
	{
		if ( cls.state == ca_connected || cls.state == ca_active )
			return cls.isVAC2Secure != 0;

		return false;
	}
};

EXPOSE_SINGLE_INTERFACE( CGameUIFuncs, IGameUIFuncs, VENGINE_GAMEUIFUNCS_VERSION );
