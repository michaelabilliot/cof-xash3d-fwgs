// screen.h -- the screen refresh (gl_screen.c)

#ifndef SCREEN_H
#define SCREEN_H

extern	float	scr_con_current;
extern	float	scr_centertime_off;
extern	int		clearnotify;	// set to 0 whenever notify text is drawn
extern	int		scr_copytop;

extern	cvar_t	scr_downloading;	// percent of the current download, -1 when none

void	SCR_EndLoadingPlaque( void );
void	SCR_CenterPrint( char *str );

extern	cvar_t	scr_graphheight;
extern	cvar_t	scr_viewsize;
extern	cvar_t	scr_connectmsg;
extern	cvar_t	scr_connectmsg1;
extern	cvar_t	scr_connectmsg2;

extern	qboolean	gfBackground;
extern	int		scr_fullupdate;
extern	qboolean	scr_skipupdate;
extern	qboolean	scr_skiponeupdate;
extern	qboolean	scr_initialized;
extern	qboolean	scr_drawloading;
extern	int		glx, gly, glwidth, glheight;
extern	int		sb_lines;

void	SCR_Init( void );
void	SCR_UpdateScreen( void );
void	SCR_CalcRefdef( void );
void	SCR_ScreenShot_f( void );
void	SCR_SizeUp_f( void );
void	SCR_SizeDown_f( void );
void	SCR_DrawCenterString( void );
void	SCR_CheckDrawCenterString( void );
void	SCR_DrawLoading( void );
void	SCR_DrawPause( void );
void	SCR_SetUpToDrawConsole( void );
void	SCR_DrawConsole( void );
void	SCR_TileClear( void );
void	SCR_ConnectMsg( void );
void	SCR_DrawDownloadText( void );
void	SCR_BeginLoadingPlaque( qboolean reconnect );
void	D_FillRect( struct vrect_s *r, unsigned char *color );
void	GL_BreakOnError( void );

// net_ws.c
void	SCR_DrawFPS( void );
void	SCR_NetGraph( void );

// hud.c
void	HudSizeUp( void );
void	HudSizeDown( void );
void	Sbar_Draw( void );

#endif // SCREEN_H
