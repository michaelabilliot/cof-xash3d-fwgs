//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "SteamAppStartup_guess.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define STEAM_PARM "-steam"

static void LaunchSelfViaSteam_guess( const char *params );

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *fileName -
// Output : true if the file exists
//-----------------------------------------------------------------------------
bool FileExists_guess( const char *fileName )
{
	struct _stat statbuf;
	return ( _stat( fileName, &statbuf ) == 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Call this first thing at startup
//			Works out if the app is a steam app that is being ran outside of steam,
//			and if so, launches steam and tells it to run us as a steam app
//
//			if it returns true, then exit
//			if it returns false, then continue with normal startup
//-----------------------------------------------------------------------------
bool ShouldLaunchAppViaSteam_guess( const char *lpCmdLine, const char *steamFilesystemDllName, const char *stdioFilesystemDllName )
{
	// see if steam is on the command line
	const char *steamStr = strstr( lpCmdLine, STEAM_PARM );

	// check the character following it is a whitespace or null
	if ( steamStr )
	{
		const char *postChar = steamStr + strlen( STEAM_PARM );
		if ( *postChar == 0 || isspace( *postChar ) )
		{
			// we're running under steam already, let the app continue
			return false;
		}
	}

	// we're not running under steam, see which filesystems are available
	if ( FileExists_guess( stdioFilesystemDllName ) )
	{
		// we're being run with a stdio filesystem, so we can continue without steam
		return false;
	}

	// make sure we have a steam filesystem available
	if ( !FileExists_guess( steamFilesystemDllName ) )
	{
		// no steam filesystem available, let the app continue
		return false;
	}

	// we have the steam filesystem, and no stdio filesystem, so we must need to be run under steam
	// launch steam
	LaunchSelfViaSteam_guess( lpCmdLine );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Handles launching the game indirectly via steam
//-----------------------------------------------------------------------------
static void LaunchSelfViaSteam_guess( const char *params )
{
	// calculate the details of our launch
	char appPath[ MAX_PATH ];
	::GetModuleFileName( (HINSTANCE)GetModuleHandle( NULL ), appPath, sizeof( appPath ) );

	// strip out the exe name
	char *slash = strrchr( appPath, '\\' );
	if ( slash )
	{
		*slash = 0;
	}

	// save out our details to the registry
	HKEY hKey;
	if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam", &hKey ) )
	{
		DWORD dwType = REG_SZ;
		DWORD dwSize = strlen( appPath ) + 1;
		RegSetValueEx( hKey, "TempAppPath", NULL, dwType, (LPBYTE)appPath, dwSize );
		dwSize = strlen( params ) + 1;
		RegSetValueEx( hKey, "TempAppCmdLine", NULL, dwType, (LPBYTE)params, dwSize );
		// clear out the appID (since we don't know it yet)
		dwType = REG_DWORD;
		int appID = -1;
		RegSetValueEx( hKey, "TempAppID", NULL, dwType, (LPBYTE)&appID, sizeof( appID ) );
		RegCloseKey( hKey );
	}

	// search for an active steam instance
	HWND hwnd = ::FindWindow( "Valve_SteamIPC_Class", "Hidden Window" );
	if ( hwnd )
	{
		::PostMessage( hwnd, WM_USER + 3, 0, 0 );
	}
	else
	{
		// couldn't find steam, find and launch it

		// first, search backwards through our current set of directories
		char steamExe[ MAX_PATH ];
		steamExe[ 0 ] = 0;
		char dir[ MAX_PATH ];
		if ( ::GetCurrentDirectoryA( sizeof( dir ), dir ) )
		{
			char *slash = strrchr( dir, '\\' );
			while ( slash )
			{
				// see if steam_dev.exe is in the directory first
				slash[ 1 ] = 0;
				strcat( slash, "steam_dev.exe" );
				FILE *f = fopen( dir, "rb" );
				if ( f )
				{
					// found it
					fclose( f );
					strcpy( steamExe, dir );
					break;
				}

				// see if steam.exe is in the directory
				slash[ 1 ] = 0;
				strcat( slash, "steam.exe" );
				f = fopen( dir, "rb" );
				if ( f )
				{
					// found it
					fclose( f );
					strcpy( steamExe, dir );
					break;
				}

				// kill the string at the slash
				slash[ 0 ] = 0;

				// move to the previous slash
				slash = strrchr( dir, '\\' );
			}
		}

		if ( !steamExe[ 0 ] )
		{
			// still not found, use the one in the registry
			HKEY hKey;
			if ( ERROR_SUCCESS == RegOpenKey( HKEY_CURRENT_USER, "Software\\Valve\\Steam", &hKey ) )
			{
				DWORD dwType;
				DWORD dwSize = sizeof( steamExe );
				RegQueryValueEx( hKey, "SteamExe", NULL, &dwType, (LPBYTE)steamExe, &dwSize );
				RegCloseKey( hKey );
			}
		}

		if ( !steamExe[ 0 ] )
		{
			// still no path, error
			::MessageBox( NULL, "Error running game: could not find steam.exe to launch", "Fatal Error", MB_OK | MB_ICONERROR );
			return;
		}

		// fix any slashes
		for ( char *slash = steamExe; *slash; slash++ )
		{
			if ( *slash == '/' )
			{
				*slash = '\\';
			}
		}

		// change to the steam directory
		strcpy( dir, steamExe );
		char *delimiter = strrchr( dir, '\\' );
		if ( delimiter )
		{
			*delimiter = 0;
			_chdir( dir );
		}

		// exec steam.exe, in silent mode, with the launch app param
		char *args[ 4 ] = { steamExe, "-silent", "-applaunch", NULL };
		_spawnv( _P_NOWAIT, steamExe, args );
	}
}
