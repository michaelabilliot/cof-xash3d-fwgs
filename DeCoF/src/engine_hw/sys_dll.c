// sys_dll.c -- system services and the game dll interface

#include <setjmp.h>
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "info.h"
#include "host.h"
#include "net.h"
#include "protocol.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "pmove.h"
#include "server.h"
#include "client.h"
#include "keys.h"
#include "sv_log.h"
#include "dll_state.h"
#include "cdll_int.h"
#include "modinfo.h"
#include "pr_dlls.h"

typedef struct functiontable_s
{
	uint32		pFunction;
	char		*pFunctionName;
} functiontable_t;

typedef struct extensiondll_s
{
	void			*lDLLHandle;
	functiontable_t	*functionTable;
	int				functionCount;
} extensiondll_t;

typedef void (*DISPATCHFUNCTION)( struct entvars_s *, void * );
typedef void (*FIELDIOFUNCTION)( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );
typedef int (*APIFUNCTION)( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
typedef int (*APIFUNCTION2)( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
typedef int (*NEW_DLL_FUNCTIONS_FN)( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );
typedef void (__stdcall *PFN_GiveFnptrsToDll)( enginefuncs_t *, globalvars_t * );

#define MAX_EXTENSION_DLL	50

// cl_main.c
extern	cvar_t	rate;

// sys_wina.asm
void	MaskExceptions( void );
void	Sys_SetFPCW( void );
void	Sys_PushFPCW_SetHigh( void );
void	Sys_PopFPCW( void );

// sys_getmodes.cpp
int		VideoMode_IsWindowed( void );

// keys.c
void	IN_UpdateMouse_guess( void );	// 0x01D59ADE

// fills pextdll->functionTable from the export directory of the dll (0x01D9FD45)
void	BuildExportTable_guess( extensiondll_t *pextdll, char *pszDllFilename );

// the engine functions handed to the game dll, from the other engine files
void	PF_precache_model_I( void );
void	PF_precache_sound_I( void );
void	PF_setmodel_I( void );
void	PF_modelindex( void );
void	ModelFrames( void );
void	PF_setsize_I( void );
void	PF_changelevel_I( void );
void	PF_setspawnparms_I( void );
void	SaveSpawnParms( void );
void	PF_vectoyaw_I( void );
void	PF_vectoangles_I( void );
void	SV_MoveToOrigin_I( void );
void	PF_changeyaw_I( void );
void	PF_changepitch_I( void );
void	FindEntityByString( void );
void	GetEntityIllum( void );
void	FindEntityInSphere( void );
void	PF_checkclient_I( void );
void	PVSFindEntities( void );
void	PF_makevectors_I( void );
void	PF_Spawn_I( void );
void	PF_Remove_I( void );
void	CreateNamedEntity( void );
void	PF_makestatic_I( void );
void	PF_checkbottom_I( void );
void	PF_droptofloor_I( void );
void	PF_walkmove_I( void );
void	PF_setorigin_I( void );
void	PF_sound_I( void );
void	PF_ambientsound_I( void );
void	PF_traceline_DLL( void );
void	PF_TraceToss_DLL( void );
void	TraceMonsterHull( void );
void	TraceHull( void );
void	TraceModel( void );
void	TraceTexture( void );
void	TraceSphere( void );
void	PF_aim_I( void );
void	PF_localcmd_I( void );
void	PF_localexec_I( void );
void	PF_stuffcmd_I( void );
void	PF_particle_I( void );
void	PF_lightstyle_I( void );
void	PF_DecalIndex( void );
void	PF_pointcontents_I( void );
void	PF_MessageBegin_I( void );
void	PF_MessageEnd_I( void );
void	PF_WriteByte_I( void );
void	PF_WriteChar_I( void );
void	PF_WriteShort_I( void );
void	PF_WriteLong_I( void );
void	PF_WriteAngle_I( void );
void	PF_WriteCoord_I( void );
void	PF_WriteString_I( void );
void	PF_WriteEntity_I( void );
void	CVarRegister( void );
void	CVarGetFloat( void );
void	CVarGetString( void );
void	CVarSetFloat( void );
void	CVarSetString( void );
void	FreeEntPrivateData( void );
void	AnimationAutomove( void );
void	GetBonePosition( void );
void	ClientPrintf( void );
void	ServerPrint( void );
void	GetAttachment( void );
void	CRC32_Init( void );
void	CRC32_ProcessBuffer( void );
void	CRC32_ProcessByte( void );
void	PF_setview_I( void );
void	PF_Time( void );
void	PF_crosshairangle_I( void );
void	COM_FreeFile( void );
void	Host_EndSection( void );
void	COM_GetGameDir( void );
void	PF_FadeVolume( void );
void	PF_SetClientMaxspeed( void );
void	PF_CreateFakeClient_I( void );
void	PF_RunPlayerMove_I( void );
void	PF_NumberOfEntities_I( void );
void	PF_GetInfoKeyBuffer_I( void );
void	PF_InfoKeyValue_I( void );
void	PF_SetKeyValue_I( void );
void	PF_SetClientKeyValue_I( void );
void	PF_IsMapValid_I( void );
void	PF_StaticDecal( void );
void	PF_GetPlayerUserId( void );
void	PF_BuildSoundMsg_I( void );
void	CVarGetPointer( void );
void	PF_GetPlayerWONId( void );
void	PF_RemoveKey_I( void );
void	PF_GetPhysicsKeyValue( void );
void	PF_SetPhysicsKeyValue( void );
void	PF_GetPhysicsInfoString( void );
void	EV_Precache( void );
void	EV_Playback( void );
void	DELTA_SetField( void );
void	DELTA_UnsetField( void );
void	DELTA_AddEncoder( void );
void	PF_GetCurrentPlayer( void );
void	PF_CanSkipPlayer( void );
void	DELTA_SetFieldByIndex( void );
void	DELTA_UnsetFieldByIndex( void );
void	PF_SetGroupMask( void );
void	PF_CreateInstancedBaseline( void );
void	PF_Cvar_DirectSet( void );
void	PF_ForceUnmodified( void );
void	PF_GetPlayerStats( void );
void	Cmd_AddGameCommand( void );
void	PF_GetPlayerAuthId( void );
void	SequenceGet( void );
void	SequencePickSentence( void );
void	VGuiWrap2_IsInCareerMatch( void );
void	VGuiWrap2_GetLocalizedStringLength( void );
void	RegisterTutorMessageShown( void );
void	GetTimesTutorMessageShown( void );
void	ProcessTutorMessageDecayBuffer( void );
void	ConstructTutorMessageDecayBuffer( void );
void	ResetTutorMessageDecayData( void );
void	QueryClientCvarValue( void );
void	QueryClientCvarValue2( void );
void	EngCheckParm( void );

qboolean	Voice_GetClientListening( int iReceiver, int iSender );
qboolean	Voice_SetClientListening( int iReceiver, int iSender, qboolean bListen );
uint32		FunctionFromName( const char *pName );
const char	*NameForFunction( uint32 function );
void		AlertMessage( ALERT_TYPE atype, char *szFmt, ... );
void		EngineFprintf( void *pfile, char *szFmt, ... );
void		LoadThisDll( char *szDllFilename );

volatile int	sys_checksum;

static FileFindHandle_t	g_hfind = -1;

// set once the performance counter lock exists (Sys_Init; sys_dll2.cpp
// deletes the lock in Sys_Shutdown)
qboolean			g_PerfCounterInitialized_guess;
CRITICAL_SECTION	g_PerfCounterMutex_guess;

qboolean	WinNT;

int			lowshift;
double		pfreq;
static double	curtime = 0.0;
static double	lastcurtime = 0.0;

void		(*Launcher_ConsolePrintf)( char *, ... );
void		(*VID_FlipScreen_guess)( void );

// set and read by sys_dll2.cpp
qboolean	g_bPrintingKeepAliveDots;
qboolean	gHasMMXTechnology;

qboolean	g_bIsDedicatedServer;

int			giActive;
int			giStateInfo;
int			giSubState;
qboolean	gfBackground;

char		gszDisconnectReason[256];
char		gszExtendedDisconnectReason[256];
qboolean	gfExtendedError;

modinfo_t			gmodinfo;
DLL_FUNCTIONS		gEntityInterface;
NEW_DLL_FUNCTIONS	gNewDLLFunctions;

extensiondll_t	g_rgextdll[MAX_EXTENSION_DLL];
int				g_iextdllMac;

static const char g_szExtensionDllSubDir[] = "cryoffear\\cl_dlls";

/*
================
Sys_PageIn
================
*/
void Sys_PageIn( void *ptr, int size )
{
	byte	*x;
	int		m, n;

// touch all the memory to make sure it's there. The 16-page skip is to
// keep Win 95 from thinking we're trying to page ourselves in (we are
// doing that, of course, but there's no reason we shouldn't)
	x = (byte *)ptr;

	for ( n = 0; n < 4; n++ )
	{
		for ( m = 0; m < ( size - 16 * 0x1000 ); m += 4 )
		{
			sys_checksum += *(int *)&x[m];
			sys_checksum += *(int *)&x[m + 16 * 0x1000];
		}
	}
}

/*
================
Sys_FindFirst
================
*/
const char *Sys_FindFirst( const char *path, char *basename )
{
	const char	*psz;

	if ( g_hfind != -1 )
	{
		Sys_Error( "Sys_FindFirst without close" );
		FS_FindClose( g_hfind );
	}

	psz = FS_FindFirst( path, &g_hfind, NULL );
	if ( basename && psz )
	{
		COM_FileBase( (char *)psz, basename );
	}

	return psz;
}

/*
================
Sys_FindNext
================
*/
const char *Sys_FindNext( char *basename )
{
	const char	*psz;

	psz = FS_FindNext( g_hfind );
	if ( basename && psz )
	{
		COM_FileBase( (char *)psz, basename );
	}

	return psz;
}

/*
================
Sys_FindClose
================
*/
void Sys_FindClose( void )
{
	if ( g_hfind != -1 )
	{
		FS_FindClose( g_hfind );
		g_hfind = -1;
	}
}

static int glob_match_after_star( char *pattern, char *text );

/* Match the pattern PATTERN against the string TEXT;
   return 1 if it matches, 0 otherwise.

   A match means the entire string TEXT is used up in matching.

   In the pattern string, `*' matches any sequence of characters,
   `?' matches any character, [SET] matches any character in the specified set,
   [!SET] matches any character not in the specified set.

   A set is composed of characters or ranges; a range looks like
   character hyphen character (as in 0-9 or A-Z).
   [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
   Any other character in the pattern must be matched exactly.

   To suppress the special syntactic significance of any of `[]*?!-\',
   and match the character exactly, precede it with a `\'.
*/
int glob_match( char *pattern, char *text )
{
	register char *p = pattern, *t = text;
	register char c;

	while ( ( c = *p++ ) != '\0' )
		switch ( c )
		{
		case '?':
			if ( *t == '\0' )
				return 0;
			else
				++t;
			break;

		case '\\':
			if ( *p++ != *t++ )
				return 0;
			break;

		case '*':
			return glob_match_after_star( p, t );

		case '[':
			{
				register char c1 = *t++;
				int invert;

				if ( !c1 )
					return ( 0 );

				invert = ( ( *p == '!' ) || ( *p == '^' ) );
				if ( invert )
					p++;

				c = *p++;
				while ( 1 )
				{
					register char cstart = c, cend = c;

					if ( c == '\\' )
					{
						cstart = *p++;
						cend = cstart;
					}
					if ( c == '\0' )
						return 0;

					c = *p++;
					if ( c == '-' && *p != ']' )
					{
						cend = *p++;
						if ( cend == '\\' )
							cend = *p++;
						if ( cend == '\0' )
							return 0;
						c = *p++;
					}
					if ( c1 >= cstart && c1 <= cend )
						goto match;
					if ( c == ']' )
						break;
				}
				if ( !invert )
					return 0;
				break;

			match:
				/* Skip the rest of the [...] construct that already matched.  */
				while ( c != ']' )
				{
					if ( c == '\0' )
						return 0;
					c = *p++;
					if ( c == '\0' )
						return 0;
					else if ( c == '\\' )
						++p;
				}
				if ( invert )
					return 0;
				break;
			}

		default:
			if ( c != *t++ )
				return 0;
		}

	return *t == '\0';
}

/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
static int glob_match_after_star( char *pattern, char *text )
{
	register char *p = pattern, *t = text;
	register char c, c1;

	while ( ( c = *p++ ) == '?' || c == '*' )
		if ( c == '?' && *t++ == '\0' )
			return 0;

	if ( c == '\0' )
		return 1;

	if ( c == '\\' )
		c1 = *p;
	else
		c1 = c;

	while ( 1 )
	{
		if ( ( c == '[' || *t == c1 ) && glob_match( p - 1, t ) )
			return 1;
		if ( *t++ == '\0' )
			return 0;
	}
}

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable( uint32 startaddr, uint32 length )
{
	DWORD	flOldProtect;

//@@@ copy on write or just read-write?
	if ( !VirtualProtect( (LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect ) )
		Sys_Error( "Protection change failed.\n" );
}

/*
================
Sys_Init
================
*/
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

	Sys_InitFloatTime();
}

/*
================
Sys_Sleep
================
*/
void Sys_Sleep( int msec )
{
	Sleep( msec );
}

/*
================
Sys_DebugOutStraight
================
*/
void Sys_DebugOutStraight( const char *pStr )
{
	OutputDebugString( pStr );
}

/*
================
Sys_Error
================
*/
void Sys_Error( char *error, ... )
{
	va_list		argptr;
	char		text[1024];
	static qboolean	bReentry;

	va_start( argptr, error );
	_vsnprintf( text, sizeof( text ), error, argptr );
	va_end( argptr );

	if ( bReentry )
	{
		fprintf( stderr, "%s\n", text );
		longjmp( host_abortserver, 2 );
	}

	bReentry = true;

	if ( svs.dll_initialized && gEntityInterface.pfnSys_Error )
		gEntityInterface.pfnSys_Error( text );

	Log_Printf( "FATAL ERROR (shutting down): %s\n", text );

	if ( g_bIsDedicatedServer )
	{
		MessageBox( NULL, text, "Fatal error - Dedicated server", MB_ICONERROR );
		_exit( -1 );
	}

	Sys_Printf( text );
	DestroyWindow( GetForegroundWindow() );
	MessageBox( NULL, text, "Fatal Error", MB_ICONERROR );
	VideoMode_IsWindowed();
	_exit( -1 );
}

/*
================
Sys_Warning
================
*/
void Sys_Warning( char *pszWarning, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start( argptr, pszWarning );
	_vsnprintf( text, sizeof( text ), pszWarning, argptr );
	va_end( argptr );

	Sys_Printf( text );
}

/*
================
Sys_Printf
================
*/
void Sys_Printf( char *fmt, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start( argptr, fmt );
	_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	if ( g_bIsDedicatedServer && Launcher_ConsolePrintf )
		Launcher_ConsolePrintf( "%s", text );

	OutputDebugString( text );
}

/*
================
Sys_Quit
================
*/
void Sys_Quit( void )
{
	giActive = DLL_CLOSE;
}

/*
================
Sys_FloatTime
================
*/
double Sys_FloatTime( void )
{
	static int			sametimecount;
	static unsigned int	oldtime;
	static int			first = 1;
	LARGE_INTEGER		PerformanceCount;
	unsigned int		temp, t2;
	double				time;

	if ( !g_PerfCounterInitialized_guess )
		return 1.0;

	EnterCriticalSection( &g_PerfCounterMutex_guess );

	Sys_PushFPCW_SetHigh();

	QueryPerformanceCounter( &PerformanceCount );

	if ( lowshift == 0 )
		temp = (unsigned int)PerformanceCount.LowPart;
	else
		temp = ( (unsigned int)PerformanceCount.LowPart >> lowshift ) |
			   ( (unsigned int)PerformanceCount.HighPart << ( 32 - lowshift ) );

	if ( first )
	{
		oldtime = temp;
		first = 0;
	}
	else
	{
	// check for turnover or backward time
		if ( ( temp <= oldtime ) && ( ( oldtime - temp ) < 0x10000000 ) )
		{
			oldtime = temp;	// so we can't get stuck
		}
		else
		{
			t2 = temp - oldtime;

			time = (double)t2 * pfreq;
			oldtime = temp;

			curtime += time;

			if ( curtime == lastcurtime )
			{
				sametimecount++;

				if ( sametimecount > 100000 )
				{
					curtime += 1.0;
					sametimecount = 0;
				}
			}
			else
			{
				sametimecount = 0;
			}

			lastcurtime = curtime;
		}
	}

	Sys_PopFPCW();

	LeaveCriticalSection( &g_PerfCounterMutex_guess );

	return curtime;
}

/*
================
Sys_InitFloatTime
================
*/
void Sys_InitFloatTime( void )
{
	int		j;

	Sys_FloatTime();

	j = COM_CheckParm( "-starttime" );

	if ( j )
	{
		curtime = (double)( Q_atof( com_argv[j + 1] ) );
	}
	else
	{
		curtime = 0.0;
	}

	lastcurtime = curtime;
}

/*
================
Sys_ShutdownFloatTime
================
*/
void Sys_ShutdownFloatTime( void )
{
	curtime = lastcurtime = 0.0;
}

void Dispatch_Substate( int iSubState )
{
	giSubState = iSubState;
}

void GameSetSubState( int iSubState )
{
	if ( iSubState & 2 )
	{
		Dispatch_Substate( 1 );
	}
	else if ( iSubState != 1 )
	{
		Dispatch_Substate( iSubState );
	}
}

void GameSetState( int iState )
{
	giActive = iState;
}

void GameSetBackground( qboolean bNewSetting )
{
	gfBackground = bNewSetting;
}

void ForceReloadProfile( void )
{
	Cbuf_AddText( "exec config.cfg\n" );
	Cbuf_AddText( "+mlook\n" );
	Cbuf_Execute();

	Key_SetBinding( '~', "toggleconsole" );
	Key_SetBinding( '`', "toggleconsole" );
	Key_SetBinding( K_ESCAPE, "cancelselect" );

	if ( cls.state != ca_dedicated )
	{
		char	szRate[32];
		DWORD	dataSize;

		dataSize = sizeof( szRate );
		Sys_GetRegKeyValue( "Software\\Valve\\Steam", "Rate", szRate, dataSize, rate.string );
		Cvar_DirectSet( &rate, szRate );
	}
}

/*
==============
Sys_GetRegKeyValue
==============
*/
void Sys_GetRegKeyValue( char *pszSubKey, char *pszElement, char *pszReturnString, int nReturnLength, char *pszDefaultValue )
{
	LONG	lResult;
	HKEY	hKey;
	char	szBuff[1024];
	DWORD	dwDisposition;
	DWORD	dwType;
	DWORD	dwSize;
	HKEY	hkeyRoot;

	sprintf( pszReturnString, "%s", pszDefaultValue );

	if ( nReturnLength > 1024 )
		return;

	hkeyRoot = HKEY_CURRENT_USER;
	if ( !_strnicmp( pszSubKey, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		pszSubKey += 19;
		hkeyRoot = HKEY_LOCAL_MACHINE;
	}

	lResult = RegCreateKeyEx( hkeyRoot, pszSubKey, 0, "String", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	if ( lResult != ERROR_SUCCESS )
		return;

	if ( dwDisposition == REG_CREATED_NEW_KEY )
	{
		lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen( pszDefaultValue ) + 1 );
	}
	else
	{
		dwSize = nReturnLength;
		lResult = RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)szBuff, &dwSize );

		if ( lResult == ERROR_SUCCESS )
		{
			if ( dwType == REG_SZ )
			{
				Q_strncpy( pszReturnString, szBuff, nReturnLength );
				pszReturnString[nReturnLength - 1] = '\0';
			}
		}
		else
		{
			lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen( pszDefaultValue ) + 1 );
		}
	}

	RegCloseKey( hKey );
}

/*
==============
Sys_RegKeyValueExists_guess

True when the value exists under the key (nothing calls it)
==============
*/
qboolean Sys_RegKeyValueExists_guess( char *pszSubKey, char *pszElement )
{
	LONG	lResult;
	HKEY	hKey;
	DWORD	dwType;
	DWORD	dwSize;
	HKEY	hkeyRoot;

	hkeyRoot = HKEY_CURRENT_USER;
	if ( !_strnicmp( pszSubKey, "HKEY_LOCAL_MACHINE", 18 ) )
	{
		pszSubKey += 19;
		hkeyRoot = HKEY_LOCAL_MACHINE;
	}

	lResult = RegOpenKeyEx( hkeyRoot, pszSubKey, 0, KEY_ALL_ACCESS, &hKey );
	if ( lResult != ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return false;
	}

	if ( lResult != ERROR_SUCCESS )
	{
		RegCloseKey( hKey );
		return false;
	}

	dwSize = 0;
	lResult = RegQueryValueEx( hKey, pszElement, 0, &dwType, NULL, &dwSize );
	RegCloseKey( hKey );

	if ( lResult != ERROR_SUCCESS )
		return false;

	return true;
}

/*
==============
Sys_SetRegKeyValue
==============
*/
void Sys_SetRegKeyValue( char *pszSubKey, char *pszElement, char *pszValue )
{
	LONG	lResult;
	HKEY	hKey;
	DWORD	dwDisposition;

	lResult = RegCreateKeyEx( HKEY_CURRENT_USER, pszSubKey, 0, "String", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition );
	if ( lResult != ERROR_SUCCESS )
		return;

	lResult = RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszValue, Q_strlen( pszValue ) + 1 );

	RegCloseKey( hKey );
}

qboolean Voice_GetClientListening( int iReceiver, int iSender )
{
	--iReceiver;
	--iSender;

	if ( iReceiver < 0 || iReceiver >= svs.maxclients || iSender < 0 || iSender >= svs.maxclients )
		return 0;

	return ( svs.clients[iSender].m_VoiceStreams[iReceiver >> 5] != 0 ) & ( 1 << ( iReceiver & 31 ) );
}

qboolean Voice_SetClientListening( int iReceiver, int iSender, qboolean bListen )
{
	uint32	*pDest;

	--iReceiver;
	--iSender;

	if ( iReceiver < 0 || iReceiver >= svs.maxclients || iSender < 0 || iSender >= svs.maxclients )
		return 0;

	pDest = &svs.clients[iSender].m_VoiceStreams[iReceiver >> 5];
	if ( bListen )
		*pDest |= 1 << ( iReceiver & 31 );
	else
		*pDest &= ~( 1 << ( iReceiver & 31 ) );

	return 1;
}

static enginefuncs_t g_engfuncsExportedToDlls =
{
	PF_precache_model_I,
	PF_precache_sound_I,
	PF_setmodel_I,
	PF_modelindex,
	ModelFrames,
	PF_setsize_I,
	PF_changelevel_I,
	PF_setspawnparms_I,
	SaveSpawnParms,
	PF_vectoyaw_I,
	PF_vectoangles_I,
	SV_MoveToOrigin_I,
	PF_changeyaw_I,
	PF_changepitch_I,
	FindEntityByString,
	GetEntityIllum,
	FindEntityInSphere,
	PF_checkclient_I,
	PVSFindEntities,
	PF_makevectors_I,
	AngleVectors,
	PF_Spawn_I,
	PF_Remove_I,
	CreateNamedEntity,
	PF_makestatic_I,
	PF_checkbottom_I,
	PF_droptofloor_I,
	PF_walkmove_I,
	PF_setorigin_I,
	PF_sound_I,
	PF_ambientsound_I,
	PF_traceline_DLL,
	PF_TraceToss_DLL,
	TraceMonsterHull,
	TraceHull,
	TraceModel,
	TraceTexture,
	TraceSphere,
	PF_aim_I,
	PF_localcmd_I,
	PF_localexec_I,
	PF_stuffcmd_I,
	PF_particle_I,
	PF_lightstyle_I,
	PF_DecalIndex,
	PF_pointcontents_I,
	PF_MessageBegin_I,
	PF_MessageEnd_I,
	PF_WriteByte_I,
	PF_WriteChar_I,
	PF_WriteShort_I,
	PF_WriteLong_I,
	PF_WriteAngle_I,
	PF_WriteCoord_I,
	PF_WriteString_I,
	PF_WriteEntity_I,
	CVarRegister,
	CVarGetFloat,
	CVarGetString,
	CVarSetFloat,
	CVarSetString,
	AlertMessage,
	EngineFprintf,
	PvAllocEntPrivateData,
	PvEntPrivateData,
	FreeEntPrivateData,
	SzFromIndex,
	AllocEngineString,
	GetVarsOfEnt,
	PEntityOfEntOffset,
	EntOffsetOfPEntity,
	IndexOfEdict,
	PEntityOfEntIndex,
	FindEntityByVars,
	GetModelPtr,
	RegUserMsg,
	AnimationAutomove,
	GetBonePosition,
	FunctionFromName,
	NameForFunction,
	ClientPrintf,
	ServerPrint,
	Cmd_Args,
	Cmd_Argv,
	Cmd_Argc,
	GetAttachment,
	CRC32_Init,
	CRC32_ProcessBuffer,
	CRC32_ProcessByte,
	CRC32_Final,
	RandomLong,
	RandomFloat,
	PF_setview_I,
	PF_Time,
	PF_crosshairangle_I,
	COM_LoadFileForMe,
	COM_FreeFile,
	Host_EndSection,
	COM_CompareFileTime,
	COM_GetGameDir,
	Cvar_RegisterVariable,
	PF_FadeVolume,
	PF_SetClientMaxspeed,
	PF_CreateFakeClient_I,
	PF_RunPlayerMove_I,
	PF_NumberOfEntities_I,
	PF_GetInfoKeyBuffer_I,
	PF_InfoKeyValue_I,
	PF_SetKeyValue_I,
	PF_SetClientKeyValue_I,
	PF_IsMapValid_I,
	PF_StaticDecal,
	PF_precache_generic_I,
	PF_GetPlayerUserId,
	PF_BuildSoundMsg_I,
	PF_IsDedicatedServer,
	CVarGetPointer,
	PF_GetPlayerWONId,
	PF_RemoveKey_I,
	PF_GetPhysicsKeyValue,
	PF_SetPhysicsKeyValue,
	PF_GetPhysicsInfoString,
	EV_Precache,
	EV_Playback,
	SV_FatPVS,
	SV_FatPAS,
	SV_CheckVisibility,
	DELTA_SetField,
	DELTA_UnsetField,
	DELTA_AddEncoder,
	PF_GetCurrentPlayer,
	PF_CanSkipPlayer,
	DELTA_FindFieldIndex,
	DELTA_SetFieldByIndex,
	DELTA_UnsetFieldByIndex,
	PF_SetGroupMask,
	PF_CreateInstancedBaseline,
	PF_Cvar_DirectSet,
	PF_ForceUnmodified,
	PF_GetPlayerStats,
	Cmd_AddGameCommand,
	Voice_GetClientListening,
	Voice_SetClientListening,
	PF_GetPlayerAuthId,
	SequenceGet,
	SequencePickSentence,
	COM_FileSize,
	COM_GetApproxWavePlayLength,
	VGuiWrap2_IsInCareerMatch,
	VGuiWrap2_GetLocalizedStringLength,
	RegisterTutorMessageShown,
	GetTimesTutorMessageShown,
	ProcessTutorMessageDecayBuffer,
	ConstructTutorMessageDecayBuffer,
	ResetTutorMessageDecayData,
	QueryClientCvarValue,
	QueryClientCvarValue2,
	EngCheckParm
};

DISPATCHFUNCTION GetDispatch( char *pname )
{
	int		i;
	DISPATCHFUNCTION	pDispatch;

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		pDispatch = (DISPATCHFUNCTION)GetProcAddress( (HMODULE)g_rgextdll[i].lDLLHandle, pname );
		if ( pDispatch )
		{
			return pDispatch;
		}
	}

	return NULL;
}

const char *FindAddressInTable( extensiondll_t *pDll, uint32 function )
{
	int		i;

	for ( i = 0; i < pDll->functionCount; i++ )
	{
		if ( pDll->functionTable[i].pFunction == function )
			return pDll->functionTable[i].pFunctionName;
	}

	return NULL;
}

uint32 FindNameInTable( extensiondll_t *pDll, const char *pName )
{
	int		i;

	for ( i = 0; i < pDll->functionCount; i++ )
	{
		if ( !Q_strcmp( pName, pDll->functionTable[i].pFunctionName ) )
			return pDll->functionTable[i].pFunction;
	}

	return 0;
}

uint32 FunctionFromName( const char *pName )
{
	int		i;
	uint32	function;

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		function = FindNameInTable( &g_rgextdll[i], pName );
		if ( function )
			return function;
	}

	Con_Printf( "Can't find proc: %s\n", pName );
	return 0;
}

const char *NameForFunction( uint32 function )
{
	int		i;
	const char	*pName;

	for ( i = 0; i < g_iextdllMac; i++ )
	{
		pName = FindAddressInTable( &g_rgextdll[i], function );
		if ( pName )
			return pName;
	}

	Con_Printf( "Can't find address: %08lx\n", function );
	return NULL;
}

ENTITYINIT GetEntityInit( char *pClassName )
{
	return (ENTITYINIT)GetDispatch( pClassName );
}

FIELDIOFUNCTION GetIOFunction( char *pName )
{
	return (FIELDIOFUNCTION)GetDispatch( pName );
}

void DLL_SetModKey( modinfo_t *pinfo, char *pkey, char *pvalue )
{
	if ( !Q_stricmp( pkey, "url_info" ) )
	{
		pinfo->bIsMod = 1;
		Q_strncpy( pinfo->szInfo, pvalue, sizeof( pinfo->szInfo ) - 1 );
		pinfo->szInfo[sizeof( pinfo->szInfo ) - 1] = 0;
	}
	else if ( !Q_stricmp( pkey, "url_dl" ) )
	{
		pinfo->bIsMod = 1;
		Q_strncpy( pinfo->szDL, pvalue, sizeof( pinfo->szDL ) - 1 );
		pinfo->szDL[sizeof( pinfo->szDL ) - 1] = 0;
	}
	else if ( !Q_stricmp( pkey, "version" ) )
	{
		pinfo->bIsMod = 1;
		pinfo->version = atoi( pvalue );
	}
	else if ( !Q_stricmp( pkey, "size" ) )
	{
		pinfo->bIsMod = 1;
		pinfo->size = atoi( pvalue );
	}
	else if ( !Q_stricmp( pkey, "svonly" ) )
	{
		pinfo->bIsMod = 1;
		pinfo->svonly = atoi( pvalue ) != 0;
	}
	else if ( !Q_stricmp( pkey, "cldll" ) )
	{
		pinfo->bIsMod = 1;
		pinfo->cldll = atoi( pvalue ) != 0;
	}
	else if ( !Q_stricmp( pkey, "secure" ) )
	{
		pinfo->bIsMod = 1;
		pinfo->secure = atoi( pvalue ) != 0;
	}
	else if ( !Q_stricmp( pkey, "hlversion" ) )
	{
		Q_strncpy( pinfo->szHLVersion, pvalue, sizeof( pinfo->szHLVersion ) - 1 );
		pinfo->szHLVersion[sizeof( pinfo->szHLVersion ) - 1] = 0;
	}
	else if ( !Q_stricmp( pkey, "type" ) )
	{
		if ( !_stricmp( pvalue, "singleplayer_only" ) )
			pinfo->type = SINGLEPLAYER_ONLY;
		else if ( !_stricmp( pvalue, "multiplayer_only" ) )
			pinfo->type = MULTIPLAYER_ONLY;
		else
			pinfo->type = BOTH;
	}
	else if ( !Q_stricmp( pkey, "fallback_dir" ) )
	{
		COM_AddDefaultDir( pvalue );
	}
}

void LoadEntityDLLs( char *szBaseDir )
{
	char		szDllWildcard[MAX_PATH];
	APIFUNCTION	pfnGetAPI;
	APIFUNCTION2	pfnGetAPI2;
	char		szDllFilename[MAX_PATH];
	char		*pszInputStream;
	char		*pStreamPos;
	char		szDllListFile[MAX_PATH];
	int			nFileSize;
	int			nBytesRead;
	FileHandle_t	hLibListFile;
	char		szGameDir[64];
	char		szKey[64];
	char		szValue[256];
	NEW_DLL_FUNCTIONS_FN	pNewAPI;
	int			interface_version;

	SV_ResetModInfo();

	g_iextdllMac = 0;
	Q_memset( g_rgextdll, 0, sizeof( g_rgextdll ) );

	Q_strncpy( szGameDir, com_gamedir, sizeof( szGameDir ) - 1 );
	if ( Q_stricmp( szGameDir, "valve" ) )
	{
		gmodinfo.bIsMod = 1;
	}

	_snprintf( szDllListFile, sizeof( szDllListFile ), "%s", "liblist.gam" );

	hLibListFile = FS_Open( szDllListFile, "rb" );
	if ( hLibListFile )
	{
		nFileSize = FS_Size( hLibListFile );
		if ( !nFileSize || nFileSize > 256 * 1024 )
		{
			Sys_Error( "Game listing file size is bogus [%s: size %i]", "liblist.gam", nFileSize );
		}

		pszInputStream = (char *)Mem_Malloc( nFileSize + 1 );
		if ( !pszInputStream )
		{
			Sys_Error( "Could not allocate space for game listing file of %i bytes", nFileSize + 1 );
		}

		nBytesRead = FS_Read( pszInputStream, nFileSize, 1, hLibListFile );
		if ( nBytesRead != nFileSize )
		{
			Sys_Error( "Error reading in game listing file, expected %i bytes, read %i", nFileSize, nBytesRead );
		}

		pszInputStream[nFileSize] = 0;
		pStreamPos = pszInputStream;

		com_ignorecolons = 1;
		while ( 1 )
		{
			pStreamPos = COM_Parse( pStreamPos );
			if ( Q_strlen( com_token ) <= 0 )
				break;

			Q_strncpy( szKey, com_token, sizeof( szKey ) - 1 );
			szKey[sizeof( szKey ) - 1] = 0;

			pStreamPos = COM_Parse( pStreamPos );
			Q_strncpy( szValue, com_token, sizeof( szValue ) - 1 );
			szValue[sizeof( szValue ) - 1] = 0;

			if ( Q_stricmp( szKey, "gamedll" ) )
			{
				DLL_SetModKey( &gmodinfo, szKey, szValue );
				continue;
			}
			else
			{
				int		index;

				index = COM_CheckParm( "-dll" );
				if ( index && index < com_argc - 1 )
				{
					Q_strncpy( szValue, com_argv[index + 1], sizeof( szValue ) - 1 );
					szValue[sizeof( szValue ) - 1] = 0;
				}

				_snprintf( szDllFilename, sizeof( szDllFilename ), "%s\\%s\\%s", szBaseDir, szGameDir, szValue );

				if ( !Q_strstr( szDllFilename, ".dll" ) )
				{
					Con_DPrintf( "Skipping non-dll:  %s\n", szDllFilename );
					continue;
				}

				Con_DPrintf( "\nAdding:  %s/%s\n", szGameDir, szValue );
				LoadThisDll( szDllFilename );
			}
		}
		com_ignorecolons = 0;

		Mem_Free( pszInputStream );
		FS_Close( hLibListFile );
	}
	else
	{
		const char	*findfn;

		_snprintf( szDllWildcard, sizeof( szDllWildcard ), "%s\\*.dll", g_szExtensionDllSubDir );

		findfn = Sys_FindFirst( szDllWildcard, NULL );
		while ( findfn )
		{
			_snprintf( szDllFilename, sizeof( szDllFilename ), "%s/%s/%s", szBaseDir, g_szExtensionDllSubDir, findfn );
			LoadThisDll( szDllFilename );

			findfn = Sys_FindNext( NULL );
		}

		Sys_FindClose();
	}

	memset( &gNewDLLFunctions, 0, sizeof( gNewDLLFunctions ) );

	pNewAPI = (NEW_DLL_FUNCTIONS_FN)GetDispatch( "GetNewDLLFunctions" );
	if ( pNewAPI )
	{
		interface_version = NEW_DLL_FUNCTIONS_VERSION;
		pNewAPI( &gNewDLLFunctions, &interface_version );
	}

	pfnGetAPI2 = (APIFUNCTION2)GetDispatch( "GetEntityAPI2" );
	if ( pfnGetAPI2 )
	{
		interface_version = INTERFACE_VERSION;
		if ( !pfnGetAPI2( &gEntityInterface, &interface_version ) )
		{
			Con_Printf( "==================\n" );
			Con_Printf( "Game DLL version mismatch\n" );
			Con_Printf( "DLL version is %i, engine version is %i\n", interface_version, INTERFACE_VERSION );
			if ( interface_version > INTERFACE_VERSION )
				Con_Printf( "Engine appears to be outdated, check for updates\n" );
			else
				Con_Printf( "The game DLL for %s appears to be outdated, check for updates\n", szGameDir );
			Con_Printf( "==================\n" );
			Host_Error( "\n" );
			return;
		}
	}
	else
	{
		pfnGetAPI = (APIFUNCTION)GetDispatch( "GetEntityAPI" );
		if ( !pfnGetAPI )
		{
			Host_Error( "Couldn't get DLL API from %s!", szDllFilename );
			return;
		}

		interface_version = INTERFACE_VERSION;
		if ( !pfnGetAPI( &gEntityInterface, interface_version ) )
		{
			Con_Printf( "==================\n" );
			Con_Printf( "Game DLL version mismatch\n" );
			Con_Printf( "The game DLL for %s appears to be outdated, check for updates\n", szGameDir );
			Con_Printf( "==================\n" );
			Host_Error( "\n" );
			return;
		}
	}

	Con_DPrintf( "Dll loaded for %s %s\n", gmodinfo.bIsMod ? "mod" : "game", gEntityInterface.pfnGetGameDescription() );
}

void LoadThisDll( char *szDllFilename )
{
	HMODULE				hDLL = NULL;
	PFN_GiveFnptrsToDll	pfnGiveFnptrsToDll;
	extensiondll_t		*pextdll;

	hDLL = (HMODULE)FS_LoadLibrary( szDllFilename );
	if ( !hDLL )
	{
		int		iError_guess;

		iError_guess = GetLastError();
		Con_Printf( "LoadLibrary failed on %s (%d)\n", szDllFilename, iError_guess );
		goto IgnoreThisDLL;
	}

	pfnGiveFnptrsToDll = (PFN_GiveFnptrsToDll)GetProcAddress( hDLL, "GiveFnptrsToDll" );
	if ( pfnGiveFnptrsToDll == NULL )
	{
		Con_Printf( "Couldn't get GiveFnptrsToDll in %s\n", szDllFilename );
		goto IgnoreThisDLL;
	}

	pfnGiveFnptrsToDll( &g_engfuncsExportedToDlls, &gGlobalVariables );

	if ( g_iextdllMac == MAX_EXTENSION_DLL )
	{
		Con_Printf( "Too many DLLs, ignoring remainder\n" );
		goto IgnoreThisDLL;
	}

	pextdll = &g_rgextdll[g_iextdllMac++];
	Q_memset( pextdll, 0, sizeof( *pextdll ) );
	pextdll->lDLLHandle = hDLL;
	BuildExportTable_guess( pextdll, szDllFilename );
	return;

IgnoreThisDLL:
	if ( hDLL )
	{
		FreeLibrary( hDLL );
	}
}

void ReleaseEntityDlls( void )
{
	extensiondll_t	*pextdll;
	extensiondll_t	*pextdllMac;

	pextdll = &g_rgextdll[0];
	pextdllMac = &g_rgextdll[g_iextdllMac];

	if ( !svs.dll_initialized )
		return;

	FreeAllEntPrivateData();

	if ( gNewDLLFunctions.pfnGameShutdown )
		gNewDLLFunctions.pfnGameShutdown();

	Cvar_UnlinkExternals();

	for ( ; pextdll < pextdllMac; pextdll++ )
	{
		FreeLibrary( (HMODULE)pextdll->lDLLHandle );
		pextdll->lDLLHandle = NULL;

		if ( pextdll->functionTable )
			Mem_Free( pextdll->functionTable );
		pextdll->functionTable = NULL;
	}

	svs.dll_initialized = false;
}

void EngineFprintf( void *pfile, char *szFmt, ... )
{
	AlertMessage( at_console, "EngineFprintf:  Obsolete API\n" );
}

void AlertMessage( ALERT_TYPE atype, char *szFmt, ... )
{
	int			uType_guess;
	va_list		argptr;
	static char	szOut[1024];

	uType_guess = 0;

	if ( atype == at_logged && svs.maxclients > 1 )
	{
		va_start( argptr, szFmt );
		_vsnprintf( szOut, sizeof( szOut ), szFmt, argptr );
		va_end( argptr );

		Log_Printf( "%s", szOut );
		return;
	}

	if ( developer.value == 0.0f )
		return;

	switch ( atype )
	{
	case at_notice:
		Q_strcpy( szOut, "NOTE:  " );
		uType_guess |= MB_ICONINFORMATION;
		break;
	case at_console:
		szOut[0] = 0;
		break;
	case at_aiconsole:
		if ( developer.value < 2.0f )
			return;
		szOut[0] = 0;
		break;
	case at_warning:
		Q_strcpy( szOut, "WARNING:  " );
		uType_guess |= MB_ICONWARNING;
		break;
	case at_error:
		Q_strcpy( szOut, "ERROR:  " );
		uType_guess |= MB_ICONERROR;
		break;
	}

	va_start( argptr, szFmt );
	_vsnprintf( szOut + Q_strlen( szOut ), sizeof( szOut ) - Q_strlen( szOut ), szFmt, argptr );
	va_end( argptr );

	Con_Printf( "%s", szOut );
}

void IN_DeactivateMouse( void )
{
	ClientDLL_DeactivateMouse();
}

void IN_ActivateMouse( void )
{
	ClientDLL_ActivateMouse();
}

void IN_MouseEvent( int mstate )
{
	if ( g_fUseDInput )
		IN_UpdateMouse_guess();
	else
		ClientDLL_MouseEvent( mstate );
}

void IN_ClearStates( void )
{
	ClientDLL_ClearStates();
}

/*
================
Sys_GetClipboardData_guess

Copies the text on the clipboard into zone memory (nothing calls it)
================
*/
void Sys_GetClipboardData_guess( char **ppData )
{
	char	*lpText = NULL;
	char	*data = NULL;
	HANDLE	hClipboardData;

	if ( !ppData )
		return;

	if ( OpenClipboard( NULL ) )
	{
		hClipboardData = GetClipboardData( CF_TEXT );
		if ( hClipboardData )
		{
			lpText = (char *)GlobalLock( hClipboardData );
			if ( lpText )
			{
				data = (char *)Mem_Malloc( GlobalSize( hClipboardData ) + 1 );
				Q_strcpy( data, lpText );
			}
			GlobalUnlock( hClipboardData );
		}
		CloseClipboard();
	}

	*ppData = data;
}

/*
================
Sys_FreeClipboardData_guess
================
*/
void Sys_FreeClipboardData_guess( char **ppData )
{
	if ( !ppData )
		return;

	if ( !*ppData )
		return;

	Mem_Free( *ppData );
	*ppData = NULL;
}

/*
================
Sys_IsCtrlDown_guess

True while a control key is held (nothing calls it)
================
*/
qboolean Sys_IsCtrlDown_guess( void )
{
	if ( GetKeyState( VK_CONTROL ) < 0 )
		return 1;
	return 0;
}

void Sys_SplitPath( const char *path, char *drive, char *dir, char *fname, char *ext )
{
	_splitpath( path, drive, dir, fname, ext );
}
