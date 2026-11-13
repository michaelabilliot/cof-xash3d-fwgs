//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Half-Life launcher: picks the engine and filesystem DLLs and runs
//			the engine until it stops asking for a restart.
//
// $NoKeywords: $
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <malloc.h>
#include "interface.h"
#include "FileSystem.h"
#include "engine_launcher_api.h"
#include "icommandline.h"
#include "iregistry.h"
#include "SteamAppStartup_guess.h"

IFileSystem *g_pFileSystem = NULL;

// The three arrays below span exactly 0x2100000 bytes of .bss (0x014135C8 to
// 0x035135C8 in the retail image with the 0x01400000 base), address space that
// LoadBlobFile_guess copies the sections of a blob engine into. Only the byte
// at offset 1000 is referenced (cleared by UTIL_GetBaseDir), so how the space is
// split is not visible in the binary. Zero-initialized definitions: VC6 puts
// them after the uninitialized globals of the file, in definition order, which
// is the retail .bss order (g_pFileSystem, these, szDefaultEngineDLL_guess).
char g_rgchBlobSpaceLow_guess[ 1000 ] = { 0 };
char g_szEXEName_guess[ MAX_PATH ] = "";
char g_rgchBlobSpace_guess[ 0x2100000 - 1000 - MAX_PATH ] = { 0 };

char com_gamedir[ MAX_PATH ];
char szBaseDir[ MAX_PATH ];
char szNewCommandParams[ 2048 ];
char szDefaultEngineDLL_guess[ 8 ] = "";		// never written: an empty default for the EngineDLL key

//-----------------------------------------------------------------------------
// Purpose: Returns the directory the launcher runs from, without a trailing slash
// Output : char
//-----------------------------------------------------------------------------
char *UTIL_GetBaseDir( void )
{
	char szBuffer[ MAX_PATH ];
	char *pBuffer;
	int j;

	g_szEXEName_guess[ 0 ] = 0;
	szBuffer[ 0 ] = 0;
	szBaseDir[ 0 ] = 0;

	if ( GetModuleFileName( NULL, szBuffer, MAX_PATH ) )
	{
		GetLongPathName( szBuffer, szBaseDir, MAX_PATH );

		pBuffer = strrchr( szBaseDir, '\\' );
		if ( *pBuffer )
		{
			*( pBuffer + 1 ) = '\0';
		}

		j = strlen( szBaseDir );
		if ( j > 0 )
		{
			if ( ( szBaseDir[ j - 1 ] == '\\' ) || ( szBaseDir[ j - 1 ] == '/' ) )
			{
				szBaseDir[ j - 1 ] = 0;
			}
		}
	}

	return szBaseDir;
}

//-----------------------------------------------------------------------------
// Purpose: Picks the engine DLL from the registry and the command line
// Input  : **ppEngineDLL -
//-----------------------------------------------------------------------------
void SetEngineDLL( const char **ppEngineDLL )
{
	*ppEngineDLL = "hw.dll";

	const char *pEngineDLLSetting = registry->ReadString( "EngineDLL", "hw.dll" );
	if ( !stricmp( pEngineDLLSetting, "hw.dll" ) )
	{
		*ppEngineDLL = "hw.dll";
	}
	else if ( !stricmp( pEngineDLLSetting, "sw.dll" ) )
	{
		*ppEngineDLL = "sw.dll";
	}

	// Override the registry with the command line
	if ( cmdline->CheckParm( "-soft", NULL ) || cmdline->CheckParm( "-software", NULL ) )
	{
		*ppEngineDLL = "sw.dll";
	}
	else if ( cmdline->CheckParm( "-gl", NULL ) || cmdline->CheckParm( "-d3d", NULL ) )
	{
		*ppEngineDLL = "hw.dll";
	}

	registry->WriteString( "EngineDLL", *ppEngineDLL );
}

//-----------------------------------------------------------------------------
// Purpose: Falls back to software mode after the engine failed to set the video mode
// Output : true to restart the engine
//-----------------------------------------------------------------------------
bool OnVideoModeFailed( void )
{
	registry->WriteInt( "ScreenBPP", 16 );
	registry->WriteInt( "ScreenHeight", 640 );
	registry->WriteInt( "ScreenWidth", 480 );
	registry->WriteString( "EngineDLL", "sw.dll" );

	return MessageBox( NULL, "The specified video mode is not supported.\nThe game will now run in software mode.", "Video mode change failure", MB_OKCANCEL | MB_ICONWARNING ) == IDOK;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *out -
//			len -
// Output : int
//-----------------------------------------------------------------------------
int Sys_GetExecutableName( char *out, int len )
{
	if ( !::GetModuleFileName( (HINSTANCE)GetModuleHandle( NULL ), out, len ) )
	{
		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the stdio or the Steam filesystem
// Input  : *exename -
//			bRunningSteam -
// Output : CSysModule
//-----------------------------------------------------------------------------
CSysModule *LoadFilesystemModule( const char *exename, bool bRunningSteam )
{
	CSysModule *filesystemModule;
	const char *filesystemModuleName;
	struct _finddata_t findData;
	long hFind;

	if ( bRunningSteam )
	{
		filesystemModuleName = "filesystem_steam.dll";

		// Put the directory of steam.dll on the path when it does not load from here
		HMODULE hSteamDLL = LoadLibrary( "steam.dll" );
		if ( !hSteamDLL )
		{
			char szSteamDLLPath[ MAX_PATH ];
			char *pSlash;

			const char *pBaseDir = UTIL_GetBaseDir();
			strncpy( szSteamDLLPath, pBaseDir, sizeof( szSteamDLLPath ) );
			szSteamDLLPath[ sizeof( szSteamDLLPath ) - 1 ] = 0;

			while ( 1 )
			{
				if ( !szSteamDLLPath[ 0 ] )
					break;

				pSlash = strrchr( szSteamDLLPath, '\\' );
				if ( !pSlash )
					break;

				pSlash[ 1 ] = 0;
				strcpy( pSlash + 1, "steam.dll" );

				hFind = _findfirst( szSteamDLLPath, &findData );
				if ( hFind != -1 )
				{
					char szNewPath[ 1024 ];

					*pSlash = 0;
					sprintf( szNewPath, "PATH=%s;%s", szSteamDLLPath, getenv( "path" ) );
					_putenv( szNewPath );
					_findclose( hFind );
					break;
				}

				*pSlash = 0;
			}
		}

		filesystemModule = Sys_LoadModule( filesystemModuleName );

		if ( hSteamDLL )
		{
			FreeLibrary( hSteamDLL );
		}
	}
	else
	{
		filesystemModuleName = "filesystem_stdio.dll";
		filesystemModule = Sys_LoadModule( filesystemModuleName );
	}

	if ( !filesystemModule )
	{
		if ( strchr( exename, ';' ) )
		{
			MessageBox( NULL, "Game cannot be run from directories containing the semicolon character (';').", "Fatal Error", MB_ICONERROR );
			return NULL;
		}

		hFind = _findfirst( filesystemModuleName, &findData );
		if ( hFind != -1 )
		{
			MessageBox( NULL, "Could not load filesystem dll.\nFileSystem crashed during construction.", "Fatal Error", MB_ICONERROR );
			_findclose( hFind );
		}
		else
		{
			MessageBox( NULL, "Could not find filesystem dll to load.", "Fatal Error", MB_ICONERROR );
		}
	}

	return filesystemModule;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : hInstance -
//			hPrevInstance -
//			lpCmdLine -
//			nCmdShow -
// Output : int APIENTRY
//-----------------------------------------------------------------------------
int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	HANDLE hMutex;
	DWORD dwMutexResult;
	WSADATA WSAData;
	bool bRunningSteam;
	char exename[ 256 ];
	char *szExeName;
	const char *pszGame;
	bool restart;

	if ( ShouldLaunchAppViaSteam_guess( lpCmdLine, "filesystem_steam.dll", "filesystem_stdio.dll" ) )
		return 0;

	// Only one instance of the game may run at a time
	hMutex = CreateMutex( NULL, FALSE, "ValveHalfLifeLauncherMutex" );
	if ( hMutex )
	{
		GetLastError();
	}

	dwMutexResult = WaitForSingleObject( hMutex, 0 );
	if ( dwMutexResult != WAIT_OBJECT_0 && dwMutexResult != WAIT_ABANDONED )
	{
		MessageBox( NULL, "Could not launch game.\nOnly one instance of this game can be run at a time.", "Error", MB_ICONERROR );
		return 0;
	}

	char szForceValve[] = { '\0', '-', 'f', 'o', 'r', 'c', 'e', 'v', 'a', 'l', 'v', 'e', '\0' };

	WSAStartup( 2, &WSAData );

	registry->Init();

	cmdline->CreateCmdLine( GetCommandLine() );

	bRunningSteam = cmdline->CheckParm( "-steam", NULL ) != NULL;

	// A renamed launcher runs the mod of the same name
	Sys_GetExecutableName( exename, sizeof( exename ) );
	szExeName = strrchr( exename, '\\' ) + 1;
	if ( stricmp( "hl.exe", szExeName ) && !cmdline->CheckParm( "-game", NULL ) )
	{
		szExeName[ strlen( szExeName ) - 4 ] = '\0';
		cmdline->AppendParm( "-game", szExeName );
	}

	pszGame = cmdline->CheckParm( "-game", NULL );
	if ( pszGame )
	{
		if ( !strnicmp( &pszGame[ 6 ], "czero", 5 ) )
		{
			cmdline->AppendParm( &szForceValve[ 1 ], NULL );
		}

		strncpy( com_gamedir, pszGame, sizeof( com_gamedir ) );
	}
	else
	{
		strcpy( com_gamedir, "valve" );
	}

	_set_sbh_threshold( 0 );

	// The engine crashed while setting the video mode last time
	if ( registry->ReadInt( "CrashInitializingVideoMode", FALSE ) )
	{
		registry->WriteInt( "CrashInitializingVideoMode", FALSE );

		if ( !stricmp( registry->ReadString( "EngineDLL", szDefaultEngineDLL_guess ), "hw.dll" ) )
		{
			int nResult;

			if ( registry->ReadInt( "EngineD3D", FALSE ) == 1 )
			{
				registry->WriteInt( "EngineD3D", FALSE );
				nResult = MessageBox( NULL, "The game has detected that the previous attempt to start in D3D video mode failed.\nThe game will now run attempt to run in openGL mode.", "Video mode change failure", MB_OKCANCEL | MB_ICONWARNING );
			}
			else
			{
				registry->WriteString( "EngineDLL", "sw.dll" );
				nResult = MessageBox( NULL, "The game has detected that the previous attempt to start in openGL video mode failed.\nThe game will now run in software mode.", "Video mode change failure", MB_OKCANCEL | MB_ICONWARNING );
			}

			if ( nResult != IDOK )
				return 0;

			registry->WriteInt( "ScreenBPP", 16 );
			registry->WriteInt( "ScreenHeight", 640 );
			registry->WriteInt( "ScreenWidth", 480 );
		}
	}

	while ( 1 )
	{
		CSysModule *filesystemModule = LoadFilesystemModule( exename, bRunningSteam );
		if ( !filesystemModule )
			break;

		g_pFileSystem = (IFileSystem *)Sys_GetFactory( filesystemModule )( FILESYSTEM_INTERFACE_VERSION, NULL );
		g_pFileSystem->Mount();
		g_pFileSystem->AddSearchPath( UTIL_GetBaseDir(), "ROOT" );

		restart = false;
		int engineResult = 0;
		szNewCommandParams[ 0 ] = 0;

		const char *enginedll;
		SetEngineDLL( &enginedll );

		CSysModule *engineModule = Sys_LoadModule( enginedll );
		if ( engineModule )
		{
			CreateInterfaceFn engineFactory = Sys_GetFactory( engineModule );
			if ( engineFactory )
			{
				IEngineAPI *engineAPI = (IEngineAPI *)engineFactory( VENGINE_LAUNCHER_API_VERSION, NULL );
				if ( engineAPI )
				{
					engineResult = engineAPI->Run( hInstance, UTIL_GetBaseDir(), (char *)cmdline->GetCmdLine(), szNewCommandParams, Sys_GetFactoryThis(), Sys_GetFactory( filesystemModule ) );
				}
			}

			Sys_UnloadModule( engineModule );
		}
		else
		{
			char szMessage[ 512 ];
			sprintf( szMessage, "Could not load %s.\nPlease try again at a later time.", enginedll );
			MessageBox( NULL, szMessage, "Fatal Error", MB_ICONERROR );
		}

		switch ( engineResult )
		{
		case 0:
			restart = false;
			break;
		case 1:
			restart = true;
			break;
		case 2:
			restart = OnVideoModeFailed();
			break;
		}

		// Drop the video and connection settings before a restart
		cmdline->RemoveParm( "-sw" );
		cmdline->RemoveParm( "-startwindowed" );
		cmdline->RemoveParm( "-windowed" );
		cmdline->RemoveParm( "-window" );
		cmdline->RemoveParm( "-full" );
		cmdline->RemoveParm( "-fullscreen" );
		cmdline->RemoveParm( "-soft" );
		cmdline->RemoveParm( "-software" );
		cmdline->RemoveParm( "-gl" );
		cmdline->RemoveParm( "-d3d" );
		cmdline->RemoveParm( "-w" );
		cmdline->RemoveParm( "-width" );
		cmdline->RemoveParm( "-h" );
		cmdline->RemoveParm( "-height" );
		cmdline->RemoveParm( "+connect" );
		cmdline->SetParm( "-novid", 0 );

		if ( strstr( szNewCommandParams, "-game" ) )
		{
			cmdline->RemoveParm( "-game" );
		}

		if ( strstr( szNewCommandParams, "+load" ) )
		{
			cmdline->RemoveParm( "+load" );
		}

		cmdline->AppendParm( szNewCommandParams, NULL );

		g_pFileSystem->Unmount();
		Sys_UnloadModule( filesystemModule );

		if ( !restart )
			break;
	}

	registry->Shutdown();

	ReleaseMutex( hMutex );
	CloseHandle( hMutex );

	WSACleanup();

	return 0;
}
