// console.h -- the developer console (console.c)

#ifndef CONSOLE_H
#define CONSOLE_H

#define	CON_TEXTSIZE		16384

#define	MAX_DBG_NOTIFY		32
#define	DBG_NOTIFY_TIMEOUT	4.0

extern	int			con_totallines;
extern	int			con_backscroll;
extern	qboolean	con_forcedup;	// because no entities to refresh
extern	qboolean	con_initialized;
extern	int			con_notifylines;	// scan lines to clear for notify lines

extern	int			con_linewidth;
extern	float		con_cursorspeed;
extern	int			con_current;	// where next message will be printed
extern	int			con_x;			// offset in current line for next print
extern	char		*con_text;

extern	qboolean	con_debuglog;

struct con_nprint_s;

void	Con_SetTimes_f( void );
void	Con_HideConsole_f( void );
void	Con_ToggleConsole_f( void );
void	Con_Clear_f( void );
void	Con_ClearNotify( void );
void	Con_MessageMode_f( void );
void	Con_MessageMode2_f( void );
void	Con_CheckResize( void );
void	Con_Debug_f( void );
void	Con_Init( void );
void	Con_Shutdown( void );
void	Con_Linefeed( void );
void	Con_DebugLog( char *file, char *fmt, ... );
void	Con_Printf( char *fmt, ... );
void	Con_DPrintf( char *fmt, ... );
void	Con_SafePrintf( char *fmt, ... );
void	Con_DrawNotify( void );
void	Con_DrawDebugArea( int idx );
void	Con_DrawDebugAreas( void );
void	Con_NPrintf( int idx, char *fmt, ... );
void	Con_NXPrintf( struct con_nprint_s *info, char *fmt, ... );
int		Con_IsVisible( void );

#endif // CONSOLE_H
