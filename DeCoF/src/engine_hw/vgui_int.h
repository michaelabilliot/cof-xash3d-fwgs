// vgui_int.h -- the engine's side of the VGUI2 game UI (vgui_intwrap2.cpp, vgui2/text_draw.cpp)

#ifndef VGUI_INT_H
#define VGUI_INT_H

int		VGuiWrap2_IsConsoleVisible( void );
int		VGuiWrap2_IsGameUIVisible( void );
void	VGuiWrap2_ShowConsole( void );
void	VGuiWrap2_HideConsole( void );
void	VGuiWrap2_ClearConsole( void );
void	VGuiWrap2_ConPrintf( const char *msg );
void	VGuiWrap2_ConDPrintf( const char *msg );
int		VGuiWrap2_IsInCareerMatch( void );
void	VGuiWrap2_HideGameUI( void );
void	ValidateCDKey( int force, int inConnect );
void	VGUI2_OnDisconnectFromServer( int eLoginFailure );

int		VGui_GameUIKeyPressed( void );

// vgui_intwrap.cpp, the VGUI1 layer
void	VGuiWrap_Startup( void );
void	VGuiWrap_Shutdown( void );
int		VGuiWrap_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
void	*VGuiWrap_GetPanel( void );
void	VGuiWrap_ReleaseMouse( void );
void	VGuiWrap_GetMouse( void );
void	VGuiWrap_SetVisible( int state );
void	VGuiWrap_Paint( int paintAll );

// vgui_intwrap2.cpp, the VGUI2 layer
void	VGuiWrap2_Startup( void );
void	VGuiWrap2_Shutdown( void );
int		VGuiWrap2_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
int		VGuiWrap2_UseVGUI1( void );
void	*VGuiWrap2_GetPanel( void );
void	VGuiWrap2_ReleaseMouse( void );
void	VGuiWrap2_GetMouse( void );
void	VGuiWrap2_SetVisible( int state );
int		VGuiWrap2_GameUIKeyPressed( void );
int		VGuiWrap2_Key_Event( int down, int keynum, const char *pszCurrentBinding );
void	VGuiWrap2_Paint( void );

unsigned int	VGUI2_GetConsoleFont( void );
unsigned int	VGUI2_GetCreditsFont( void );
void	VGui_Paint( void );
int		VGUI2_GetFontTall( unsigned int font );

void	VGuiWrap2_NotifyOfServerConnect( const char *game, int IP, int port );
void	VGuiWrap2_NotifyOfServerDisconnect( void );
void	VGuiWrap2_LoadingStarted( const char *resourceType, const char *resourceName );
void	VGuiWrap2_LoadingFinished( const char *resourceType, const char *resourceName );
void	StartLoadingProgressBar( const char *loadingType, int numProgressPoints );
void	ContinueLoadingProgressBar( const char *loadingType, int progressPoint, float progressFraction );
void	SetLoadingProgressBarStatusText( const char *statusText );
void	StopLoadingProgressBar( void );
void	SetSecondaryProgressBar( float progress );
void	SetSecondaryProgressBarText( const char *statusText );

// C wrappers around the object at 0x01EDF808 (names unproven)
void	LevelLoaded_guess( struct model_s *worldmodel );	// 0x01D10DBF, CL_SignonReply
void	ResetLevelState_guess( void );						// 0x01D10E09, CL_Disconnect and CL_ParseServerInfo
struct texture_s;
struct texture_s	*AdsDecalTexture_guess( int index );	// 0x01D10E18, Draw_DecalTexture (decal indices with 0x4000 set)
struct ref_params_s;
void	AdsRenderView_guess( struct model_s *worldmodel, struct ref_params_s *pparams, float *viewangles );	// 0x01D10DD2, V_RenderView

#endif // VGUI_INT_H
