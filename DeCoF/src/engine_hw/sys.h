// sys.h -- non-portable functions (sys_dll.c)

#ifndef SYS_H
#define SYS_H

void	Sys_Error( char *error, ... );
void	Sys_Printf( char *fmt, ... );

const char	*Sys_FindFirst( const char *path, char *basename );
const char	*Sys_FindNext( char *basename );
void	Sys_FindClose( void );

double	Sys_FloatTime( void );
void	Sys_GetCDKey( char *pszCDKey, int *nLength, int *bDedicated );

extern	qboolean	g_bIsDedicatedServer;

extern	int		giActive;
extern	int		giStateInfo;
extern	int		giSubState;

// sys_dll2.cpp
const char	*GetCurrentSteamAppName( void );

// sys_dll.c
void	Sys_Sleep( int msec );
void	Sys_Warning( char *pszWarning, ... );
void	Sys_Quit( void );
void	Sys_InitFloatTime( void );
void	Sys_ShutdownFloatTime( void );
void	Sys_GetRegKeyValue( char *pszSubKey, char *pszElement, char *pszReturnString, int nReturnLength, char *pszDefaultValue );
void	Sys_SetRegKeyValue( char *pszSubKey, char *pszElement, char *pszValue );
void	Sys_SplitPath( const char *path, char *drive, char *dir, char *fname, char *ext );
void	Sys_DebugOutStraight( const char *pStr );
void	Dispatch_Substate( int iSubState );
void	GameSetSubState( int iSubState );
void	GameSetState( int iState );
void	GameSetBackground( qboolean bNewSetting );
void	ForceReloadProfile( void );
void	LoadEntityDLLs( char *szBaseDir );
void	ReleaseEntityDlls( void );
void	IN_DeactivateMouse( void );
void	IN_ActivateMouse( void );
void	IN_MouseEvent( int mstate );
void	IN_ClearStates( void );

#endif // SYS_H
