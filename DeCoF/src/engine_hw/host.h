// host.h -- the main loop (host.c, host_cmd.c)

#ifndef HOST_H
#define HOST_H

#include <setjmp.h>

typedef struct
{
	char	*basedir;		// base of directory tree
	char	*cachedir;		// for development over ISDN lines
	int		argc;
	char	**argv;
	void	*membase;
	int		memsize;
} quakeparms_t;

extern	quakeparms_t	host_parms;

// kbutton.h
typedef struct kbutton_s
{
	int		down[2];		// key nums holding it down
	int		state;			// low bit is down state
} kbutton_t;

extern	double	realtime;			// not bounded in any way, changed at
									// start of every frame, never reset
extern	double	oldrealtime;

extern	cvar_t	developer;

extern	double	host_frametime;		// time of the current frame

extern	cvar_t	sys_ticrate;
extern	cvar_t	sys_timescale;
extern	cvar_t	fps_max;
extern	cvar_t	fps_modem;
extern	cvar_t	host_killtime;
extern	cvar_t	sv_stats;
extern	cvar_t	host_framerate;
extern	cvar_t	host_limitlocal;
extern	cvar_t	pausable;

extern	int		host_framecount;	// incremented every frame, never reset
extern	int		host_hunklevel;

extern	jmp_buf	host_abortserver;
extern	jmp_buf	host_enddemo;

extern	unsigned short	*host_basepal;
extern	unsigned char	*host_colormap;

extern	qboolean	g_bUsingInGameAdvertisements;

// host.c
void	Host_EndGame( char *message, ... );
void	Host_Error( char *error, ... );
void	Host_InitLocal( void );
void	Info_WriteVars( FileHandle_t fp );
void	Host_WriteConfiguration( void );
void	Host_WriteCustomConfig( void );
void	SV_ClientPrintf( char *fmt, ... );
void	Host_ClientCommands( char *fmt, ... );
void	Host_ClearClients( qboolean bFramesOnly );
void	Host_ShutdownServer( qboolean crash );
void	SV_ClearClientStates( void );
void	Host_CheckDyanmicStructures( void );
void	Host_ClearMemory( qboolean bQuiet );
qboolean	Host_FilterTime( float time );
void	Master_Heartbeat_f( void );
void	Host_ComputeFPS( double frametime );
void	Host_GetHostInfo( float *fps, int *nActive, int *unused, int *nMaxPlayers, char *pszMap );
void	Host_Speeds( double *time );
void	Host_UpdateScreen( void );
void	CL_AddVoiceToDatagram( qboolean bFinal );
void	CL_VoiceIdle( void );
void	Host_UpdateSounds( void );
void	Host_CheckConnectionFailure( void );
void	_Host_Frame( float time );
int		Host_Frame( float time, int iState, int *stateInfo );
void	CheckGore( void );
qboolean	Host_IsSinglePlayerGame( void );
qboolean	Host_IsServerActive( void );
void	Host_Version( void );
int		Host_Init( quakeparms_t *parms );
void	Host_Shutdown( void );

// host_cmd.c
extern	int		current_skill;
extern	int		gHostSpawnCount;
extern	qboolean	noclip_anglehack;
extern	qboolean	g_bMajorMapChange;
extern	qboolean	g_bSaveAllowed_guess;	// 0x023C9F1C, set by the "savehack" commands of cd.cpp, read by Host_ValidSave
extern	int		g_iQuitCommandIssued;
extern	char	*g_pPostRestartCmdLineArgs;

void	Host_InitializeGameDLL( void );
int		Host_GetStartTime( void );
void	Host_UpdateStats( void );
void	Host_Quit_f( void );
void	Host_Map( qboolean bIsDemo, char *mapstring, char *mapName, qboolean loadGame );
char	*Host_SaveGameDirectory( void );
void	Host_ClearSaveDirectory( void );
int		Host_Load( const char *pName );
void	Host_NextDemo( void );
void	Host_EndSection( const char *pszSection );
void	Host_InitCommands( void );

#endif // HOST_H
