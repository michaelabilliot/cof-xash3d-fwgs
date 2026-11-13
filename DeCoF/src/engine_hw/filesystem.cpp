// filesystem.cpp -- loads the filesystem module and sets up the search paths of the game directories

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "FileSystem.h"
#include "UtlVector.h"

extern "C"
{
#include "quakedef.h"
#include "cvardef.h"
#include "host.h"

int			GetGameAppID( void );
qboolean	IsGameSubscribed( const char *game );

int			FileSystem_Init( char *basedir, void *voidfilesystemFactory );
void		FileSystem_Shutdown( void );
}

#include "steam_api_c.h"

extern IFileSystem *g_pFileSystem;

CUtlVector<char *> g_fallbackLocalizationFiles;

static char s_pBaseDir[512];

static CSysModule *g_pFileSystemModule;
CreateInterfaceFn g_FileSystemFactory;

// set when -steam loads filesystem_steam.dll (name unproven)
static bool g_bUsingSteamFileSystem_guess;

const char *GetBaseDirectory( void )
{
	return s_pBaseDir;
}

void *GetFileSystemFactory( void )
{
	return (void *)g_FileSystemFactory;
}

static bool FileSystem_LoadDLL( CreateInterfaceFn filesystemFactory )
{
	char *fsmodule = "filesystem_stdio.dll";

	if ( COM_CheckParm( "-steam" ) )
	{
		fsmodule = "filesystem_steam.dll";
		g_bUsingSteamFileSystem_guess = true;
	}

	if ( filesystemFactory )
	{
		g_FileSystemFactory = filesystemFactory;
	}
	else
	{
		g_pFileSystemModule = Sys_LoadModule( fsmodule );
		if ( !g_pFileSystemModule )
			return false;

		g_FileSystemFactory = Sys_GetFactory( g_pFileSystemModule );
		if ( !g_FileSystemFactory )
			return false;
	}

	g_pFileSystem = (IFileSystem *)g_FileSystemFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
	if ( !g_pFileSystem )
		return false;

	return true;
}

static void FileSystem_UnloadDLL( void )
{
	if ( !g_pFileSystemModule )
		return;

	Sys_UnloadModule( g_pFileSystemModule );
	g_pFileSystemModule = NULL;
}

void CheckLiblistForFallbackDir( const char *pGameDir, bool bLanguage, const char *pLanguage, bool bLowViolenceBuild )
{
	char szTemp[512];
	bool bSuccess = false;
	FileHandle_t hFile;

	_snprintf( szTemp, sizeof( szTemp ) - 1, "%s/liblist.gam", pGameDir );
	COM_FixSlashes( szTemp );
	g_pFileSystem->GetLocalCopy( szTemp );

	if ( !_stricmp( com_gamedir, pGameDir ) )
	{
		hFile = FS_Open( "liblist.gam", "rt" );
	}
	else
	{
		_snprintf( szTemp, sizeof( szTemp ) - 1, "../%s/liblist.gam", pGameDir );
		COM_FixSlashes( szTemp );
		hFile = FS_Open( szTemp, "rt" );
	}

	if ( hFile )
	{
		char szFallback[128];
		char szLine[512];

		while ( !FS_EndOfFile( hFile ) )
		{
			int outputBufferSize = 0;

			szLine[0] = 0;
			FS_ReadLine( szLine, sizeof( szLine ) - 1, hFile );
			szLine[sizeof( szLine ) - 1] = 0;

			if ( !_strnicmp( szLine, "fallback_dir", 12 ) )
			{
				char *start;
				char *end;
				int bytesToCopy;

				start = strchr( szLine, '"' );
				if ( !start )
					break;

				end = strchr( start + 1, '"' );
				if ( !end )
					break;

				bytesToCopy = (int)( end - start ) - 1;
				if ( bytesToCopy >= sizeof( szFallback ) - 1 )
				{
					bytesToCopy = sizeof( szFallback ) - 1;
					break;
				}

				if ( bytesToCopy > 0 )
				{
					bSuccess = true;
					strncpy( szFallback, start + 1, bytesToCopy );
					szFallback[bytesToCopy] = 0;
					break;
				}
			}
		}

		if ( bSuccess && _stricmp( pGameDir, szFallback ) )
		{
			if ( bLanguage && pLanguage )
			{
				char baseDir[MAX_PATH];
				char *tempPtr;

				_snprintf( szTemp, sizeof( szTemp ) - 1, "%s/%s_%s", GetBaseDirectory(), szFallback, pLanguage );
				szTemp[sizeof( szTemp ) - 1] = 0;
				COM_FixSlashes( szTemp );
				g_pFileSystem->AddSearchPath( szTemp, "GAME_FALLBACK" );

				if ( !COM_CheckParm( "-steam" ) )
				{
					Q_strncpy( baseDir, GetBaseDirectory(), sizeof( baseDir ) - 1 );
					baseDir[sizeof( baseDir ) - 1] = 0;

					tempPtr = Q_strstr( baseDir, "\\game" );
					if ( tempPtr )
					{
						*tempPtr = 0;

						_snprintf( szTemp, sizeof( szTemp ) - 1, "%s\\localization\\%s_%s", baseDir, szFallback, pLanguage );
						szTemp[sizeof( szTemp ) - 1] = 0;
						COM_FixSlashes( szTemp );
						g_pFileSystem->AddSearchPath( szTemp, "GAME_FALLBACK" );
					}
				}
			}

			if ( bLowViolenceBuild )
			{
				_snprintf( szTemp, sizeof( szTemp ) - 1, "%s/%s_lv", GetBaseDirectory(), szFallback );
				szTemp[sizeof( szTemp ) - 1] = 0;
				COM_FixSlashes( szTemp );
				g_pFileSystem->AddSearchPath( szTemp, "GAME_FALLBACK" );
			}

			_snprintf( szTemp, sizeof( szTemp ) - 1, "%s/%s", GetBaseDirectory(), szFallback );
			szTemp[sizeof( szTemp ) - 1] = 0;
			COM_FixSlashes( szTemp );
			g_pFileSystem->AddSearchPath( szTemp, "GAME_FALLBACK" );

			if ( strcmp( szFallback, "cryoffear" ) )
			{
				const int BufLen = 128;
				char *szFileName = new char[BufLen];

				_snprintf( szFileName, BufLen - 1, "Resource/%s_%%language%%.txt", szFallback );
				szFileName[BufLen - 1] = 0;

				g_fallbackLocalizationFiles.AddToTail( szFileName );
				CheckLiblistForFallbackDir( szFallback, bLanguage, pLanguage, bLowViolenceBuild );
			}
		}

		FS_Close( hFile );
	}
}

int FileSystem_SetGameDirectory( const char *pDefaultDir, const char *pGameDir )
{
	char temp[512];
	bool writeset = false;
	char szBuffer[128];
	szBuffer[0] = 0;
	bool bLowViolenceBuild = false;
	char language[128];
	const char *pchLang;
	char skin_guess[128];

	g_pFileSystem->RemoveAllSearchPaths();
	language[0] = 0;

	Sys_GetRegKeyValue( "Software\\TeamPsykskallar\\Cry Of Fear\\Settings", "User Token 2", szBuffer, sizeof( szBuffer ) - 1, "" );
	bLowViolenceBuild = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	if ( !bLowViolenceBuild )
	{
		Sys_GetRegKeyValue( "Software\\TeamPsykskallar\\Cry Of Fear\\Settings", "User Token 3", szBuffer, sizeof( szBuffer ) - 1, "" );
		bLowViolenceBuild = ( Q_strlen( szBuffer ) > 0 ) ? true : false;
	}

	if ( !bLowViolenceBuild )
		bLowViolenceBuild = ISteamApps_BIsLowViolence();

	pchLang = ISteamApps_GetCurrentGameLanguage();
	if ( pchLang )
		strncpy( language, pchLang, sizeof( language ) );
	else
		strncpy( language, "english", sizeof( language ) );

	if ( g_bUsingSteamFileSystem_guess && !g_bIsDedicatedServer && !IsGameSubscribed( pGameDir ) )
		return 0;

	SteamAPI_SetBreakpadAppID( GetGameAppID() );

	bool bLanguage = strlen( language ) > 0 && _stricmp( language, "english" );

	if ( pGameDir && _stricmp( pGameDir, pDefaultDir ) )
	{
		if ( bLanguage )
		{
			_snprintf( temp, sizeof( temp ) - 1, "%s/%s_%s", GetBaseDirectory(), pGameDir, language );
			temp[sizeof( temp ) - 1] = 0;
			COM_FixSlashes( temp );
			g_pFileSystem->AddSearchPath( temp, "GAME" );

			if ( !COM_CheckParm( "-steam" ) )
			{
				char baseDir[MAX_PATH];
				char *tempPtr;

				Q_strncpy( baseDir, GetBaseDirectory(), sizeof( baseDir ) - 1 );
				baseDir[sizeof( baseDir ) - 1] = 0;

				tempPtr = Q_strstr( baseDir, "\\game" );
				if ( tempPtr )
				{
					*tempPtr = 0;

					_snprintf( temp, sizeof( temp ) - 1, "%s\\localization\\%s_%s", baseDir, pGameDir, language );
					temp[sizeof( temp ) - 1] = 0;
					COM_FixSlashes( temp );
					g_pFileSystem->AddSearchPath( temp, "GAME" );
				}
			}
		}

		if ( bLowViolenceBuild )
		{
			_snprintf( temp, sizeof( temp ) - 1, "%s/%s_lv", GetBaseDirectory(), pGameDir );
			temp[sizeof( temp ) - 1] = 0;
			COM_FixSlashes( temp );
			g_pFileSystem->AddSearchPath( temp, "GAME" );
		}

		_snprintf( temp, sizeof( temp ) - 1, "%s/%s", GetBaseDirectory(), pGameDir );
		temp[sizeof( temp ) - 1] = 0;
		COM_FixSlashes( temp );
		g_pFileSystem->AddSearchPath( temp, "GAME" );

		CheckLiblistForFallbackDir( pGameDir, bLanguage, language, bLowViolenceBuild );
	}

	if ( bLanguage )
	{
		_snprintf( temp, sizeof( temp ) - 1, "%s/%s_%s", GetBaseDirectory(), pDefaultDir, language );
		temp[sizeof( temp ) - 1] = 0;
		COM_FixSlashes( temp );
		g_pFileSystem->AddSearchPath( temp, "DEFAULTGAME" );

		if ( !COM_CheckParm( "-steam" ) )
		{
			char baseDir[MAX_PATH];
			char *tempPtr;

			Q_strncpy( baseDir, GetBaseDirectory(), sizeof( baseDir ) - 1 );
			baseDir[sizeof( baseDir ) - 1] = 0;

			tempPtr = Q_strstr( baseDir, "\\game" );
			if ( tempPtr )
			{
				*tempPtr = 0;

				_snprintf( temp, sizeof( temp ) - 1, "%s\\localization\\%s_%s", baseDir, pDefaultDir, language );
				temp[sizeof( temp ) - 1] = 0;
				COM_FixSlashes( temp );
				g_pFileSystem->AddSearchPath( temp, "DEFAULTGAME" );
			}
		}
	}

	if ( bLowViolenceBuild )
	{
		_snprintf( temp, sizeof( temp ) - 1, "%s/%s_lv", GetBaseDirectory(), pDefaultDir );
		temp[sizeof( temp ) - 1] = 0;
		COM_FixSlashes( temp );
		g_pFileSystem->AddSearchPath( temp, "DEFAULTGAME" );
	}

	_snprintf( temp, sizeof( temp ) - 1, "%s", GetBaseDirectory() );
	temp[sizeof( temp ) - 1] = 0;
	COM_FixSlashes( temp );
	g_pFileSystem->AddSearchPath( temp, "BASE" );

	_snprintf( temp, sizeof( temp ) - 1, "%s/%s", GetBaseDirectory(), pDefaultDir );
	temp[sizeof( temp ) - 1] = 0;
	COM_FixSlashes( temp );
	g_pFileSystem->AddSearchPath( temp, "DEFAULTGAME" );

	_snprintf( temp, sizeof( temp ) - 1, "%s/platform", GetBaseDirectory() );
	temp[sizeof( temp ) - 1] = 0;
	COM_FixSlashes( temp );
	g_pFileSystem->AddSearchPath( temp, "PLATFORM" );

	skin_guess[0] = 0;
	Sys_GetRegKeyValue( "Software\\Valve\\Steam", "Skin", skin_guess, sizeof( skin_guess ), "" );
	if ( strlen( skin_guess ) > 0 )
	{
		if ( getenv( "ValvePlatformMutex" ) )
		{
			char szPlatform_guess[MAX_PATH];
			int len_guess;

			strncpy( szPlatform_guess, getenv( "ValvePlatformMutex" ), sizeof( szPlatform_guess ) - 1 );
			szPlatform_guess[sizeof( szPlatform_guess ) - 1] = 0;

			for ( len_guess = strlen( szPlatform_guess ); len_guess > 0; len_guess-- )
			{
				if ( szPlatform_guess[len_guess] != '/' )
					szPlatform_guess[len_guess] = 0;
				else
					break;
			}

			sprintf( temp, "%sskins/%s", szPlatform_guess, skin_guess );
		}
		else
		{
			sprintf( temp, "%s/platform/skins/%s", GetBaseDirectory(), skin_guess );
		}

		COM_FixSlashes( temp );
		g_pFileSystem->AddSearchPath( temp, "SKIN" );
	}

	return 1;
}

int FileSystem_AddFallbackGameDir( const char *pGameDir )
{
	char language[128];
	const char *pchLang;

	language[0] = 0;

	pchLang = ISteamApps_GetCurrentGameLanguage();
	if ( pchLang )
		strncpy( language, pchLang, sizeof( language ) );
	else
		strncpy( language, "english", sizeof( language ) );

	if ( strlen( language ) > 0 && _stricmp( language, "english" ) )
	{
		char temp[MAX_PATH];

		sprintf( temp, "%s/%s_%s", GetBaseDirectory(), pGameDir, language );
		g_pFileSystem->AddSearchPath( temp, "GAME" );
	}

	g_pFileSystem->AddSearchPath( pGameDir, "GAME" );
	return 1;
}

int FileSystem_Init( char *basedir, void *voidfilesystemFactory )
{
	CreateInterfaceFn filesystemFactory = (CreateInterfaceFn)voidfilesystemFactory;

	strcpy( s_pBaseDir, basedir );
	host_parms.basedir = s_pBaseDir;

	if ( !FileSystem_LoadDLL( filesystemFactory ) )
		return 0;

	if ( !COM_SetupDirectories() )
		return 0;

	return 1;
}

void FileSystem_Shutdown( void )
{
	g_pFileSystem->RemoveAllSearchPaths();
	FileSystem_UnloadDLL();
}
