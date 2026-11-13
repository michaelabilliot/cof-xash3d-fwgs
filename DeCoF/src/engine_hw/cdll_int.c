// cdll_int.c -- the engine's calls into the client dll

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "host.h"
#include "client.h"
#include "net.h"
#include "GL_MODEL.H"
#include "cdll_int.h"
#include "cl_spectator.h"
#include "vgui_int.h"
#include "APIProxy.h"
#include "r_studioint.h"
#include "entity_types.h"
#include "triangleapi.h"
#include "event_flags.h"
#include "event_args.h"
#include "winquake.h"
#include "vid.h"
#include "pmove.h"
#include "view.h"
#include "info.h"

typedef struct client_data_s
{
	// fields that cannot be modified  (ie. have no effect if changed)
	vec3_t	origin;

	// fields that can be changed by the cldll
	vec3_t	viewangles;
	int		iWeaponBits;
	float	fov;	// field of view
} client_data_t;

#define CLDLL_INTERFACE_VERSION		7

typedef struct client_sprite_s
{
	char	szName[64];
	char	szSprite[64];
	int		hspr;
	int		iRes;
	wrect_t	rc;
} client_sprite_t;

// Function type declarations for client exports
typedef int (*INITIALIZE_FUNC)	( struct cl_enginefuncs_s*, int );
typedef void (*HUD_INIT_FUNC)		( void );
typedef int (*HUD_VIDINIT_FUNC)	( void );
typedef int (*HUD_REDRAW_FUNC)	( float, int );
typedef int (*HUD_UPDATECLIENTDATA_FUNC) ( struct client_data_s*, float );
typedef void (*HUD_RESET_FUNC)    ( void );
typedef void (*HUD_CLIENTMOVE_FUNC)( struct playermove_s *ppmove, qboolean server );
typedef void (*HUD_CLIENTMOVEINIT_FUNC)( struct playermove_s *ppmove );
typedef char (*HUD_TEXTURETYPE_FUNC)( char *name );
typedef void (*HUD_IN_ACTIVATEMOUSE_FUNC) ( void );
typedef void (*HUD_IN_DEACTIVATEMOUSE_FUNC)		( void );
typedef void (*HUD_IN_MOUSEEVENT_FUNC)		( int mstate );
typedef void (*HUD_IN_CLEARSTATES_FUNC)		( void );
typedef void (*HUD_IN_ACCUMULATE_FUNC ) ( void );
typedef void (*HUD_CL_CREATEMOVE_FUNC)		( float frametime, struct usercmd_s *cmd, int active );
typedef int (*HUD_CL_ISTHIRDPERSON_FUNC) ( void );
typedef void (*HUD_CL_GETCAMERAOFFSETS_FUNC )( float *ofs );
typedef struct kbutton_s * (*HUD_KB_FIND_FUNC) ( const char *name );
typedef void ( *HUD_CAMTHINK_FUNC )( void );
typedef void ( *HUD_CALCREF_FUNC ) ( struct ref_params_s *pparams );
typedef int	 ( *HUD_ADDENTITY_FUNC ) ( int type, struct cl_entity_s *ent, const char *modelname );
typedef void ( *HUD_CREATEENTITIES_FUNC ) ( void );
typedef void ( *HUD_DRAWNORMALTRIS_FUNC ) ( void );
typedef void ( *HUD_DRAWTRANSTRIS_FUNC ) ( void );
typedef void ( *HUD_STUDIOEVENT_FUNC ) ( const struct mstudioevent_s *event, const struct cl_entity_s *entity );
typedef void ( *HUD_POSTRUNCMD_FUNC ) ( struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed );
typedef void ( *HUD_SHUTDOWN_FUNC ) ( void );
typedef void ( *HUD_TXFERLOCALOVERRIDES_FUNC )( struct entity_state_s *state, const struct clientdata_s *client );
typedef void ( *HUD_PROCESSPLAYERSTATE_FUNC )( struct entity_state_s *dst, const struct entity_state_s *src );
typedef void ( *HUD_TXFERPREDICTIONDATA_FUNC ) ( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd );
typedef void ( *HUD_DEMOREAD_FUNC ) ( int size, unsigned char *buffer );
typedef int ( *HUD_CONNECTIONLESS_FUNC )( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
typedef	int	( *HUD_GETHULLBOUNDS_FUNC ) ( int hullnumber, float *mins, float *maxs );
typedef void (*HUD_FRAME_FUNC)		( double );
typedef int (*HUD_KEY_EVENT_FUNC ) ( int eventcode, int keynum, const char *pszCurrentBinding );
typedef void (*HUD_TEMPENTUPDATE_FUNC) ( double frametime, double client_time, double cl_gravity, struct tempent_s **ppTempEntFree, struct tempent_s **ppTempEntActive, 	int ( *Callback_AddVisibleEntity )( struct cl_entity_s *pEntity ),	void ( *Callback_TempEntPlaySound )( struct tempent_s *pTemp, float damp ) );
typedef struct cl_entity_s *(*HUD_GETUSERENTITY_FUNC ) ( int index );
typedef void (*HUD_VOICESTATUS_FUNC)(int entindex, qboolean bTalking);
typedef void (*HUD_DIRECTORMESSAGE_FUNC)( int iSize, void *pbuf );
typedef int ( *HUD_STUDIO_INTERFACE_FUNC )( int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio );
typedef void (*HUD_CHATINPUTPOSITION_FUNC)( int *x, int *y );
typedef int (*HUD_GETPLAYERTEAM)(int iplayer);
typedef void *(*CLIENTFACTORY)();

// Pointers to the exported client functions themselves
typedef struct
{
	INITIALIZE_FUNC						pInitFunc;
	HUD_INIT_FUNC						pHudInitFunc;
	HUD_VIDINIT_FUNC					pHudVidInitFunc;
	HUD_REDRAW_FUNC						pHudRedrawFunc;
	HUD_UPDATECLIENTDATA_FUNC			pHudUpdateClientDataFunc;
	HUD_RESET_FUNC						pHudResetFunc;
	HUD_CLIENTMOVE_FUNC					pClientMove;
	HUD_CLIENTMOVEINIT_FUNC				pClientMoveInit;
	HUD_TEXTURETYPE_FUNC				pClientTextureType;
	HUD_IN_ACTIVATEMOUSE_FUNC			pIN_ActivateMouse;
	HUD_IN_DEACTIVATEMOUSE_FUNC			pIN_DeactivateMouse;
	HUD_IN_MOUSEEVENT_FUNC				pIN_MouseEvent;
	HUD_IN_CLEARSTATES_FUNC				pIN_ClearStates;
	HUD_IN_ACCUMULATE_FUNC				pIN_Accumulate;
	HUD_CL_CREATEMOVE_FUNC				pCL_CreateMove;
	HUD_CL_ISTHIRDPERSON_FUNC			pCL_IsThirdPerson;
	HUD_CL_GETCAMERAOFFSETS_FUNC		pCL_GetCameraOffsets;
	HUD_KB_FIND_FUNC					pFindKey;
	HUD_CAMTHINK_FUNC					pCamThink;
	HUD_CALCREF_FUNC					pCalcRefdef;
	HUD_ADDENTITY_FUNC					pAddEntity;
	HUD_CREATEENTITIES_FUNC				pCreateEntities;
	HUD_DRAWNORMALTRIS_FUNC				pDrawNormalTriangles;
	HUD_DRAWTRANSTRIS_FUNC				pDrawTransparentTriangles;
	HUD_STUDIOEVENT_FUNC				pStudioEvent;
	HUD_POSTRUNCMD_FUNC					pPostRunCmd;
	HUD_SHUTDOWN_FUNC					pShutdown;
	HUD_TXFERLOCALOVERRIDES_FUNC		pTxferLocalOverrides;
	HUD_PROCESSPLAYERSTATE_FUNC			pProcessPlayerState;
	HUD_TXFERPREDICTIONDATA_FUNC		pTxferPredictionData;
	HUD_DEMOREAD_FUNC					pReadDemoBuffer;
	HUD_CONNECTIONLESS_FUNC				pConnectionlessPacket;
	HUD_GETHULLBOUNDS_FUNC				pGetHullBounds;
	HUD_FRAME_FUNC						pHudFrame;
	HUD_KEY_EVENT_FUNC					pKeyEvent;
	HUD_TEMPENTUPDATE_FUNC				pTempEntUpdate;
	HUD_GETUSERENTITY_FUNC				pGetUserEntity;
	HUD_VOICESTATUS_FUNC				pVoiceStatus;
	HUD_DIRECTORMESSAGE_FUNC			pDirectorMessage;
	HUD_STUDIO_INTERFACE_FUNC			pStudioInterface;
	HUD_CHATINPUTPOSITION_FUNC			pChatInputPosition;
	HUD_GETPLAYERTEAM					pGetPlayerTeam;
	CLIENTFACTORY						pClientFactory;
} cldll_func_t;

// LoadBlob.cpp
typedef struct BlobFootprint_s
{
	int		m_hDll;
} BlobFootprint_t;

void	FreeBlob( BlobFootprint_t *pblobfootprint );

#define MAX_BEAMENTS		64

cldll_func_t		cl_funcs;
BlobFootprint_t		g_blobfootprintClient_guess;
extern	cvar_t				cl_lw;
extern	cvar_t				*cl_righthand;
extern	float				scr_fov_value;
extern	int					cl_numbeamentities;
extern	cl_entity_t			*cl_beamentities[MAX_BEAMENTS];
extern	engine_studio_api_t	engine_studio_api;
extern	triangleapi_t		tri;

int		CL_IsSpectateOnly( void );
void	SPR_Init( void );
void	SPR_Shutdown( void );
void	R_ResetStudio( void );
mnode_t	*PVSNode( mnode_t *node, vec3_t emins, vec3_t emaxs );
void	CL_WriteDLLUpdate( client_data_t *cdat );
void	CL_DemoAnim( int anim, int body );
void	ClientDLL_CheckStudioInterface( HMODULE hClientDLL );
void	ClientDLL_Shutdown( void );
void	ClientDLL_ClientMoveInit( struct playermove_s *ppmove );
void	LoadSecureClient( char *pszDllName );
void	LoadInsecureClient( char *pszFullPathClientDLLName );

// LoadBlob.cpp
int		FIsBlob( char *pstFileName );
void	*NLoadBlobFile( char *pstFileName, BlobFootprint_t *pblobfootprint, void *pv, char fLoadAsDll );

// iga/IGA_Engine_guess.cpp
void	IGA_ClientFactory_guess( void *factory );

int		VGui_Key_Event( int down, int keynum, const char *pszCurrentBinding );
struct model_s	*SPR_GetModelPointer( HSPRITE hSprite );
void	CL_DemoEvent( int flags, int idx, float delay, event_args_t *pargs );
pfnUserMsgHook	HookServerMsg( const char *pszName, pfnUserMsgHook pfn );





struct hud_player_info_s;
struct SCREENINFO_s;
struct client_textmessage_s;

// cl_draw.c
void SPR_DisableScissor( void );
void SPR_Draw( int frame, int x, int y, const wrect_t *prc );
void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc );
void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc );
void SPR_EnableScissor( int x, int y, int width, int height );
int SPR_Frames( HSPRITE hSprite );
int SPR_Height( HSPRITE hSprite, int frame );
HSPRITE SPR_Load( const char *pTextureName );
void SPR_Set( HSPRITE hSprite, int r, int g, int b );
int SPR_Width( HSPRITE hSprite, int frame );
void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b );

// GL_DRAW.C
void Draw_FillRGBA( int x, int y, int w, int h, int r, int g, int b, int a );
void Draw_SetTextColor( float r, float g, float b );
int Draw_String( int x, int y, char *str );

// tmessage.c
int TextMessageDrawCharacter( int x, int y, int number, int r, int g, int b );
struct client_textmessage_s *TextMessageGet( const char *pName );

// KEYS.C
void Key_Event( int key, qboolean down );

// pr_cmds.c
int hudCheckParm( char *parm, char **ppnext );

// cdll_exp.c
int hudAddCommand( char *cmd_name, void ( *function )( void ) );
void hudCenterPrint( const char *string );
int hudClientCmd( char *pszCmdString );
void hudConsolePrint( const char *string );
void hudDrawConsoleStringLen( const char *string, int *width, int *height );
float hudGetCvarFloat( char *szName );
char *hudGetCvarString( char *szName );
void hudGetPlayerInfo( int ent_num, struct hud_player_info_s *pinfo );
int hudGetScreenInfo( struct SCREENINFO_s *pscrinfo );
int hudHookUserMsg( char *szMsgName, pfnUserMsgHook pfn );
void hudPlaySoundByIndex( int iSound, float volume );
void hudPlaySoundByName( char *szSound, float volume );
void hudPlaySoundByNameAtLocation( char *szSound, float volume, float *origin );
struct cvar_s *hudRegisterVariable( char *szName, char *szValue, int flags );
int hudServerCmd( char *pszCmdString );

// cdll_int.c
int CL_CreateVisibleEntity( int type, struct cl_entity_s *ent );
struct model_s *CL_LoadModel( const char *modelname, int *index );
client_sprite_t *SPR_GetList( char *psz, int *piCount );
int hudCenterX( void );
int hudCenterY( void );
float hudGetClientMaxspeed( void );
float hudGetClientTime( void );
struct cl_entity_s *hudGetEntityByIndex( int idx );
struct cl_entity_s *hudGetLocalPlayer( void );
int hudGetMaxClients( void );
void hudGetMousePosition( int *mx, int *my );
const struct model_s *hudGetSpritePointer( HSPRITE hSprite );
void hudGetViewAngles( float *va );
struct cl_entity_s *hudGetViewModel( void );
int hudIsNoClipping( void );
const char *hudPhysInfo_ValueForKey( const char *key );
const char *hudServerInfo_ValueForKey( const char *key );
void hudSetViewAngles( float *va );

// cdll_int.c
void COM_AddAppDirectoryToSearchPath( const char *pszBaseDir, const char *appName );
char *COM_ParseFile( char *data, char *token );
int ClientDLL_ExpandFileName( const char *fileName, char *nameOutBuffer, int nameOutBufferSize );
const char *GetCmdFunctionName( unsigned int cmdhandle );
struct cvar_s *GetFirstCVarPtr( void );
unsigned int GetFirstCmdFunctionHandle( void );
unsigned int GetNextCmdFunctionHandle( unsigned int cmdhandle );
int GetPlayerForTrackerID( int trackerID );
qboolean GetPlayerUniqueID( int iPlayer, char *playerID );
int GetTrackerIDForPlayer( int playerSlot );
const char *LocalPlayerInfo_ValueForKey( const char *key );
void PlayerInfo_SetValueForKey( const char *key, const char *value );
const char *PlayerInfo_ValueForKey( int playerNum, const char *key );
float hudGetClientOldTime( void );
const char *hudGetGameDir( void );
const char *hudGetLevelName( void );
struct model_s *hudGetModelByIndex( const int index );
void hudGetScreenFade( struct screenfade_s *fade );
float hudGetServerGravityValue( void );
void hudPlaybackEvent( int flags, const struct edict_s *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
unsigned short hudPrecacheEvent( int type, const char *psz );
void hudSetScreenFade( struct screenfade_s *fade );
void hudWeaponAnim( int iAnim, int body );

// vgui_intwrap2.cpp
void ConstructTutorMessageDecayBuffer( int *buffer, int bufferLength );
void ProcessTutorMessageDecayBuffer( int *buffer, int bufferLength );
void ResetTutorMessageDecayData( void );
void *VguiWrap2_GetCareerUI( void );
void VguiWrap2_GetMouseDelta( int *x, int *y );

// GL_DRAW.C
void Draw_FillRGBABlend( int x, int y, int w, int h, int r, int g, int b, int a );

// sv_main.c
int GetGameAppID( void );

// dinput_guess.cpp
void GetMousePos_guess( POINT *ppt );
void SetMouseEnable( qboolean fEnable );
void SetMousePos_guess( int x, int y );

// KEYS.C
const char *Key_NameForBinding( const char *pBinding );

// cd.cpp
void PrimeMusicStream( char *filename, int looping );

// GL_RMAIN.C
model_t *R_LoadMapSprite( const char *szFilename );

// pr_cmds.c
float RandomFloat( float flLow, float flHigh );
int32 RandomLong( int32 lLow, int32 lHigh );

// cl_draw.c
void SPR_DrawGeneric( int frame, int x, int y, const wrect_t *prc, int src, int dest, int width, int height );
void SetFilterBrightness( float brightness );
void SetFilterColor( float r, float g, float b );
void SetFilterMode( int mode );

// Sequence.c
struct sequenceEntry_s *SequenceGet( const char *fileName, const char *entryName );
struct sentenceEntry_s *SequencePickSentence( const char *groupName, int pickMethod, int *picked );

// vgui2/text_draw.cpp
int VGUI2_DrawStringClient( int x, int y, const char *str, int r, int g, int b );
int VGUI2_DrawStringReverseClient( int x, int y, const char *str, int r, int g, int b );
int VGUI2_Draw_Character( int x, int y, int ch, unsigned int font );
int VGUI2_Draw_CharacterAdd( int x, int y, int ch, int r, int g, int b, unsigned int font );

// cdll_exp.c
void hudPlaySoundByNameAtPitch( char *szSound, float volume, int pitch );
void hudPlaySoundVoiceByName( char *szSound, float volume, int pitch );
int hudServerCmdUnreliable( char *pszCmdString );
int hudFilteredClientCmd( char *pszCmdString );

// vgui_EngineSurfaceHW.cpp
void VGui_ViewportPaintBackground( int *extents );

// module.cpp
void NullDst( void );

cmdalias_t *GetAliasesList( void );

// ********************************************************
// Functions exported by the engine
// ********************************************************

// Function type declarations for engine exports
typedef HSPRITE						(*pfnEngSrc_pfnSPR_Load_t )			( const char *szPicName );
typedef int							(*pfnEngSrc_pfnSPR_Frames_t )			( HSPRITE hPic );
typedef int							(*pfnEngSrc_pfnSPR_Height_t )			( HSPRITE hPic, int frame );
typedef int							(*pfnEngSrc_pfnSPR_Width_t )			( HSPRITE hPic, int frame );
typedef void						(*pfnEngSrc_pfnSPR_Set_t )				( HSPRITE hPic, int r, int g, int b );
typedef void						(*pfnEngSrc_pfnSPR_Draw_t )			( int frame, int x, int y, const struct rect_s *prc );
typedef void						(*pfnEngSrc_pfnSPR_DrawHoles_t )		( int frame, int x, int y, const struct rect_s *prc );
typedef void						(*pfnEngSrc_pfnSPR_DrawAdditive_t )	( int frame, int x, int y, const struct rect_s *prc );
typedef void						(*pfnEngSrc_pfnSPR_EnableScissor_t )	( int x, int y, int width, int height );
typedef void						(*pfnEngSrc_pfnSPR_DisableScissor_t )	( void );
typedef struct client_sprite_s	*	(*pfnEngSrc_pfnSPR_GetList_t )			( char *psz, int *piCount );
typedef void						(*pfnEngSrc_pfnFillRGBA_t )			( int x, int y, int width, int height, int r, int g, int b, int a );
typedef int							(*pfnEngSrc_pfnGetScreenInfo_t ) 		( struct SCREENINFO_s *pscrinfo );
typedef void						(*pfnEngSrc_pfnSetCrosshair_t )		( HSPRITE hspr, wrect_t rc, int r, int g, int b );
typedef struct cvar_s *				(*pfnEngSrc_pfnRegisterVariable_t )	( char *szName, char *szValue, int flags );
typedef float						(*pfnEngSrc_pfnGetCvarFloat_t )		( char *szName );
typedef char*						(*pfnEngSrc_pfnGetCvarString_t )		( char *szName );
typedef int							(*pfnEngSrc_pfnAddCommand_t )			( char *cmd_name, void (*pfnEngSrc_function)(void) );
typedef int							(*pfnEngSrc_pfnHookUserMsg_t )			( char *szMsgName, pfnUserMsgHook pfn );
typedef int							(*pfnEngSrc_pfnServerCmd_t )			( char *szCmdString );
typedef int							(*pfnEngSrc_pfnClientCmd_t )			( char *szCmdString );
typedef void						(*pfnEngSrc_pfnPrimeMusicStream_t )	( char *szFilename, int looping );
typedef void						(*pfnEngSrc_pfnGetPlayerInfo_t )		( int ent_num, struct hud_player_info_s *pinfo );
typedef void						(*pfnEngSrc_pfnPlaySoundByName_t )		( char *szSound, float volume );
typedef void						(*pfnEngSrc_pfnPlaySoundByNameAtPitch_t )	( char *szSound, float volume, int pitch );
typedef void						(*pfnEngSrc_pfnPlaySoundVoiceByName_t )		( char *szSound, float volume, int pitch );
typedef void						(*pfnEngSrc_pfnPlaySoundByIndex_t )	( int iSound, float volume );
typedef void						(*pfnEngSrc_pfnAngleVectors_t )		( const float * vecAngles, float * forward, float * right, float * up );
typedef struct client_textmessage_s*(*pfnEngSrc_pfnTextMessageGet_t )		( const char *pName );
typedef int							(*pfnEngSrc_pfnDrawCharacter_t )		( int x, int y, int number, int r, int g, int b );
typedef int							(*pfnEngSrc_pfnDrawConsoleString_t )	( int x, int y, char *string );
typedef void						(*pfnEngSrc_pfnDrawSetTextColor_t )	( float r, float g, float b );
typedef void						(*pfnEngSrc_pfnDrawConsoleStringLen_t )(  const char *string, int *length, int *height );
typedef void						(*pfnEngSrc_pfnConsolePrint_t )		( const char *string );
typedef void						(*pfnEngSrc_pfnCenterPrint_t )			( const char *string );
typedef int							(*pfnEngSrc_GetWindowCenterX_t )		( void );
typedef int							(*pfnEngSrc_GetWindowCenterY_t )		( void );
typedef void						(*pfnEngSrc_GetViewAngles_t )			( float * );
typedef void						(*pfnEngSrc_SetViewAngles_t )			( float * );
typedef int							(*pfnEngSrc_GetMaxClients_t )			( void );
typedef void						(*pfnEngSrc_Cvar_SetValue_t )			( char *cvar, float value );
typedef int       					(*pfnEngSrc_Cmd_Argc_t)					(void);	
typedef char *						(*pfnEngSrc_Cmd_Argv_t )				( int arg );
typedef void						(*pfnEngSrc_Con_Printf_t )				( char *fmt, ... );
typedef void						(*pfnEngSrc_Con_DPrintf_t )			( char *fmt, ... );
typedef void						(*pfnEngSrc_Con_NPrintf_t )			( int pos, char *fmt, ... );
typedef void						(*pfnEngSrc_Con_NXPrintf_t )			( struct con_nprint_s *info, char *fmt, ... );
typedef const char *				(*pfnEngSrc_PhysInfo_ValueForKey_t )	( const char *key );
typedef const char *				(*pfnEngSrc_ServerInfo_ValueForKey_t )( const char *key );
typedef float						(*pfnEngSrc_GetClientMaxspeed_t )		( void );
typedef int							(*pfnEngSrc_CheckParm_t )				( char *parm, char **ppnext );
typedef void						(*pfnEngSrc_Key_Event_t )				( int key, int down );
typedef void						(*pfnEngSrc_GetMousePosition_t )		( int *mx, int *my );
typedef int							(*pfnEngSrc_IsNoClipping_t )			( void );
typedef struct cl_entity_s *		(*pfnEngSrc_GetLocalPlayer_t )		( void );
typedef struct cl_entity_s *		(*pfnEngSrc_GetViewModel_t )			( void );
typedef struct cl_entity_s *		(*pfnEngSrc_GetEntityByIndex_t )		( int idx );
typedef float						(*pfnEngSrc_GetClientTime_t )			( void );
typedef void						(*pfnEngSrc_V_CalcShake_t )			( void );
typedef void						(*pfnEngSrc_V_ApplyShake_t )			( float *origin, float *angles, float factor );
typedef int							(*pfnEngSrc_PM_PointContents_t )		( float *point, int *truecontents );
typedef int							(*pfnEngSrc_PM_WaterEntity_t )			( float *p );
typedef struct pmtrace_s *			(*pfnEngSrc_PM_TraceLine_t )			( float *start, float *end, int flags, int usehull, int ignore_pe );
typedef struct model_s *			(*pfnEngSrc_CL_LoadModel_t )			( const char *modelname, int *index );
typedef int							(*pfnEngSrc_CL_CreateVisibleEntity_t )	( int type, struct cl_entity_s *ent );
typedef const struct model_s *		(*pfnEngSrc_GetSpritePointer_t )		( HSPRITE hSprite );
typedef void						(*pfnEngSrc_pfnPlaySoundByNameAtLocation_t )	( char *szSound, float volume, float *origin );
typedef unsigned short				(*pfnEngSrc_pfnPrecacheEvent_t )		( int type, const char* psz );
typedef void						(*pfnEngSrc_pfnPlaybackEvent_t )		( int flags, const struct edict_s *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
typedef void						(*pfnEngSrc_pfnWeaponAnim_t )			( int iAnim, int body );
typedef float						(*pfnEngSrc_pfnRandomFloat_t )			( float flLow, float flHigh );
typedef int32						(*pfnEngSrc_pfnRandomLong_t )			( int32 lLow, int32 lHigh );
typedef void						(*pfnEngSrc_pfnHookEvent_t )			( char *name, void ( *pfnEvent )( struct event_args_s *args ) );
typedef int							(*pfnEngSrc_Con_IsVisible_t)			();
typedef const char *				(*pfnEngSrc_pfnGetGameDirectory_t )	( void );
typedef struct cvar_s *				(*pfnEngSrc_pfnGetCvarPointer_t )		( const char *szName );
typedef const char *				(*pfnEngSrc_Key_LookupBinding_t )		( const char *pBinding );
typedef const char *				(*pfnEngSrc_pfnGetLevelName_t )		( void );
typedef void						(*pfnEngSrc_pfnGetScreenFade_t )		( struct screenfade_s *fade );
typedef void						(*pfnEngSrc_pfnSetScreenFade_t )		( struct screenfade_s *fade );
typedef void *						(*pfnEngSrc_VGui_GetPanel_t )         ( );
typedef void                        (*pfnEngSrc_VGui_ViewportPaintBackground_t ) (int extents[4]);
typedef byte*						(*pfnEngSrc_COM_LoadFile_t )				( char *path, int usehunk, int *pLength );
typedef char*						(*pfnEngSrc_COM_ParseFile_t )			( char *data, char *token );
typedef void						(*pfnEngSrc_COM_FreeFile_t)				( void *buffer );
typedef struct triangleapi_s *		pTriAPI;
typedef struct efx_api_s *			pEfxAPI;
typedef struct event_api_s *		pEventAPI;
typedef struct demo_api_s *			pDemoAPI;
typedef struct net_api_s *			pNetAPI;
typedef struct IVoiceTweak_s *		pVoiceTweak;
typedef int							(*pfnEngSrc_IsSpectateOnly_t ) ( void );
typedef struct model_s *			(*pfnEngSrc_LoadMapSprite_t )			( const char *filename );
typedef void						(*pfnEngSrc_COM_AddAppDirectoryToSearchPath_t ) ( const char *pszBaseDir, const char *appName );
typedef int							(*pfnEngSrc_COM_ExpandFilename_t)				 ( const char *fileName, char *nameOutBuffer, int nameOutBufferSize );
typedef const char *				(*pfnEngSrc_PlayerInfo_ValueForKey_t )( int playerNum, const char *key );
typedef void						(*pfnEngSrc_PlayerInfo_SetValueForKey_t )( const char *key, const char *value );
typedef qboolean					(*pfnEngSrc_GetPlayerUniqueID_t)(int iPlayer, char playerID[16]);
typedef int							(*pfnEngSrc_GetTrackerIDForPlayer_t)(int playerSlot);
typedef int							(*pfnEngSrc_GetPlayerForTrackerID_t)(int trackerID);
typedef int							(*pfnEngSrc_pfnServerCmdUnreliable_t )( char *szCmdString );
typedef void						(*pfnEngSrc_GetMousePos_t )(struct tagPOINT *ppt);
typedef void						(*pfnEngSrc_SetMousePos_t )(int x, int y);
typedef void						(*pfnEngSrc_SetMouseEnable_t)(qboolean fEnable);
typedef struct cvar_s *				(*pfnEngSrc_GetFirstCVarPtr_t)();
typedef unsigned int				(*pfnEngSrc_GetFirstCmdFunctionHandle_t)();
typedef unsigned int				(*pfnEngSrc_GetNextCmdFunctionHandle_t)(unsigned int cmdhandle);
typedef const char *				(*pfnEngSrc_GetCmdFunctionName_t)(unsigned int cmdhandle);
typedef float						(*pfnEngSrc_GetClientOldTime_t)();
typedef float						(*pfnEngSrc_GetServerGravityValue_t)();
typedef struct model_s	*			(*pfnEngSrc_GetModelByIndex_t)( int index );
typedef void						(*pfnEngSrc_pfnSetFilterMode_t )( int mode );
typedef void						(*pfnEngSrc_pfnSetFilterColor_t )( float r, float g, float b );
typedef void						(*pfnEngSrc_pfnSetFilterBrightness_t )( float brightness );
typedef struct sequenceEntry_s*		(*pfnEngSrc_pfnSequenceGet_t )( const char *fileName, const char* entryName );
typedef void						(*pfnEngSrc_pfnSPR_DrawGeneric_t )( int frame, int x, int y, const struct rect_s *prc, int src, int dest, int w, int h );
typedef struct sentenceEntry_s*		(*pfnEngSrc_pfnSequencePickSentence_t )( const char *sentenceName, int pickMethod, int* entryPicked );
// draw a complete string
typedef int							(*pfnEngSrc_pfnDrawString_t )		( int x, int y, const char *str, int r, int g, int b );
typedef int							(*pfnEngSrc_pfnDrawStringReverse_t )		( int x, int y, const char *str, int r, int g, int b );
typedef const char *				(*pfnEngSrc_LocalPlayerInfo_ValueForKey_t )( const char *key );
typedef int							(*pfnEngSrc_pfnVGUI2DrawCharacter_t )		( int x, int y, int ch, unsigned int font );
typedef int							(*pfnEngSrc_pfnVGUI2DrawCharacterAdd_t )	( int x, int y, int ch, int r, int g, int b, unsigned int font);
typedef unsigned int		(*pfnEngSrc_COM_GetApproxWavePlayLength ) ( const char * filename);
typedef void *						(*pfnEngSrc_pfnGetCareerUI_t)();
typedef void						(*pfnEngSrc_Cvar_Set_t )			( char *cvar, char *value );
typedef int							(*pfnEngSrc_pfnIsPlayingCareerMatch_t)();
typedef double						(*pfnEngSrc_GetAbsoluteTime_t) ( void );
typedef void						(*pfnEngSrc_pfnProcessTutorMessageDecayBuffer_t)(int *buffer, int bufferLength);
typedef void						(*pfnEngSrc_pfnConstructTutorMessageDecayBuffer_t)(int *buffer, int bufferLength);
typedef void						(*pfnEngSrc_pfnResetTutorMessageDecayData_t)();
typedef void						(*pfnEngSrc_pfnFillRGBABlend_t )			( int x, int y, int width, int height, int r, int g, int b, int a );
typedef int						(*pfnEngSrc_pfnGetAppID_t)			( void );
typedef cmdalias_t*				(*pfnEngSrc_pfnGetAliases_t)		( void );
typedef void					(*pfnEngSrc_pfnVguiWrap2_GetMouseDelta_t) ( int *x, int *y );
typedef int							(*pfnEngSrc_pfnFilteredClientCmd_t) 	( char *szCmdString );

// Pointers to the exported engine functions themselves
typedef struct cl_enginefuncs_s
{
	pfnEngSrc_pfnSPR_Load_t					pfnSPR_Load;
	pfnEngSrc_pfnSPR_Frames_t				pfnSPR_Frames;
	pfnEngSrc_pfnSPR_Height_t				pfnSPR_Height;
	pfnEngSrc_pfnSPR_Width_t				pfnSPR_Width;
	pfnEngSrc_pfnSPR_Set_t					pfnSPR_Set;
	pfnEngSrc_pfnSPR_Draw_t					pfnSPR_Draw;
	pfnEngSrc_pfnSPR_DrawHoles_t			pfnSPR_DrawHoles;
	pfnEngSrc_pfnSPR_DrawAdditive_t			pfnSPR_DrawAdditive;
	pfnEngSrc_pfnSPR_EnableScissor_t		pfnSPR_EnableScissor;
	pfnEngSrc_pfnSPR_DisableScissor_t		pfnSPR_DisableScissor;
	pfnEngSrc_pfnSPR_GetList_t				pfnSPR_GetList;
	pfnEngSrc_pfnFillRGBA_t					pfnFillRGBA;
	pfnEngSrc_pfnGetScreenInfo_t			pfnGetScreenInfo;
	pfnEngSrc_pfnSetCrosshair_t				pfnSetCrosshair;
	pfnEngSrc_pfnRegisterVariable_t			pfnRegisterVariable;
	pfnEngSrc_pfnGetCvarFloat_t				pfnGetCvarFloat;
	pfnEngSrc_pfnGetCvarString_t			pfnGetCvarString;
	pfnEngSrc_pfnAddCommand_t				pfnAddCommand;
	pfnEngSrc_pfnHookUserMsg_t				pfnHookUserMsg;
	pfnEngSrc_pfnServerCmd_t				pfnServerCmd;
	pfnEngSrc_pfnClientCmd_t				pfnClientCmd;
	pfnEngSrc_pfnGetPlayerInfo_t			pfnGetPlayerInfo;
	pfnEngSrc_pfnPlaySoundByName_t			pfnPlaySoundByName;
	pfnEngSrc_pfnPlaySoundByIndex_t			pfnPlaySoundByIndex;
	pfnEngSrc_pfnAngleVectors_t				pfnAngleVectors;
	pfnEngSrc_pfnTextMessageGet_t			pfnTextMessageGet;
	pfnEngSrc_pfnDrawCharacter_t			pfnDrawCharacter;
	pfnEngSrc_pfnDrawConsoleString_t		pfnDrawConsoleString;
	pfnEngSrc_pfnDrawSetTextColor_t			pfnDrawSetTextColor;
	pfnEngSrc_pfnDrawConsoleStringLen_t		pfnDrawConsoleStringLen;
	pfnEngSrc_pfnConsolePrint_t				pfnConsolePrint;
	pfnEngSrc_pfnCenterPrint_t				pfnCenterPrint;
	pfnEngSrc_GetWindowCenterX_t			GetWindowCenterX;
	pfnEngSrc_GetWindowCenterY_t			GetWindowCenterY;
	pfnEngSrc_GetViewAngles_t				GetViewAngles;
	pfnEngSrc_SetViewAngles_t				SetViewAngles;
	pfnEngSrc_GetMaxClients_t				GetMaxClients;
	pfnEngSrc_Cvar_SetValue_t				Cvar_SetValue;
	pfnEngSrc_Cmd_Argc_t					Cmd_Argc;
	pfnEngSrc_Cmd_Argv_t					Cmd_Argv;
	pfnEngSrc_Con_Printf_t					Con_Printf;
	pfnEngSrc_Con_DPrintf_t					Con_DPrintf;
	pfnEngSrc_Con_NPrintf_t					Con_NPrintf;
	pfnEngSrc_Con_NXPrintf_t				Con_NXPrintf;
	pfnEngSrc_PhysInfo_ValueForKey_t		PhysInfo_ValueForKey;
	pfnEngSrc_ServerInfo_ValueForKey_t		ServerInfo_ValueForKey;
	pfnEngSrc_GetClientMaxspeed_t			GetClientMaxspeed;
	pfnEngSrc_CheckParm_t					CheckParm;
	pfnEngSrc_Key_Event_t					Key_Event;
	pfnEngSrc_GetMousePosition_t			GetMousePosition;
	pfnEngSrc_IsNoClipping_t				IsNoClipping;
	pfnEngSrc_GetLocalPlayer_t				GetLocalPlayer;
	pfnEngSrc_GetViewModel_t				GetViewModel;
	pfnEngSrc_GetEntityByIndex_t			GetEntityByIndex;
	pfnEngSrc_GetClientTime_t				GetClientTime;
	pfnEngSrc_V_CalcShake_t					V_CalcShake;
	pfnEngSrc_V_ApplyShake_t				V_ApplyShake;
	pfnEngSrc_PM_PointContents_t			PM_PointContents;
	pfnEngSrc_PM_WaterEntity_t				PM_WaterEntity;
	pfnEngSrc_PM_TraceLine_t				PM_TraceLine;
	pfnEngSrc_CL_LoadModel_t				CL_LoadModel;
	pfnEngSrc_CL_CreateVisibleEntity_t		CL_CreateVisibleEntity;
	pfnEngSrc_GetSpritePointer_t			GetSpritePointer;
	pfnEngSrc_pfnPlaySoundByNameAtLocation_t	pfnPlaySoundByNameAtLocation;
	pfnEngSrc_pfnPrecacheEvent_t			pfnPrecacheEvent;
	pfnEngSrc_pfnPlaybackEvent_t			pfnPlaybackEvent;
	pfnEngSrc_pfnWeaponAnim_t				pfnWeaponAnim;
	pfnEngSrc_pfnRandomFloat_t				pfnRandomFloat;
	pfnEngSrc_pfnRandomLong_t				pfnRandomLong;
	pfnEngSrc_pfnHookEvent_t				pfnHookEvent;
	pfnEngSrc_Con_IsVisible_t				Con_IsVisible;
	pfnEngSrc_pfnGetGameDirectory_t			pfnGetGameDirectory;
	pfnEngSrc_pfnGetCvarPointer_t			pfnGetCvarPointer;
	pfnEngSrc_Key_LookupBinding_t			Key_LookupBinding;
	pfnEngSrc_pfnGetLevelName_t				pfnGetLevelName;
	pfnEngSrc_pfnGetScreenFade_t			pfnGetScreenFade;
	pfnEngSrc_pfnSetScreenFade_t			pfnSetScreenFade;
	pfnEngSrc_VGui_GetPanel_t				VGui_GetPanel;
	pfnEngSrc_VGui_ViewportPaintBackground_t	VGui_ViewportPaintBackground;
	pfnEngSrc_COM_LoadFile_t				COM_LoadFile;
	pfnEngSrc_COM_ParseFile_t				COM_ParseFile;
	pfnEngSrc_COM_FreeFile_t				COM_FreeFile;
	struct triangleapi_s		*pTriAPI;
	struct efx_api_s			*pEfxAPI;
	struct event_api_s			*pEventAPI;
	struct demo_api_s			*pDemoAPI;
	struct net_api_s			*pNetAPI;
	struct IVoiceTweak_s		*pVoiceTweak;
	pfnEngSrc_IsSpectateOnly_t				IsSpectateOnly;
	pfnEngSrc_LoadMapSprite_t				LoadMapSprite;
	pfnEngSrc_COM_AddAppDirectoryToSearchPath_t		COM_AddAppDirectoryToSearchPath;
	pfnEngSrc_COM_ExpandFilename_t			COM_ExpandFilename;
	pfnEngSrc_PlayerInfo_ValueForKey_t		PlayerInfo_ValueForKey;
	pfnEngSrc_PlayerInfo_SetValueForKey_t	PlayerInfo_SetValueForKey;
	pfnEngSrc_GetPlayerUniqueID_t			GetPlayerUniqueID;
	pfnEngSrc_GetTrackerIDForPlayer_t		GetTrackerIDForPlayer;
	pfnEngSrc_GetPlayerForTrackerID_t		GetPlayerForTrackerID;
	pfnEngSrc_pfnServerCmdUnreliable_t		pfnServerCmdUnreliable;
	pfnEngSrc_GetMousePos_t					pfnGetMousePos;
	pfnEngSrc_SetMousePos_t					pfnSetMousePos;
	pfnEngSrc_SetMouseEnable_t				pfnSetMouseEnable;
	pfnEngSrc_GetFirstCVarPtr_t				GetFirstCvarPtr;
	pfnEngSrc_GetFirstCmdFunctionHandle_t	GetFirstCmdFunctionHandle;
	pfnEngSrc_GetNextCmdFunctionHandle_t	GetNextCmdFunctionHandle;
	pfnEngSrc_GetCmdFunctionName_t			GetCmdFunctionName;
	pfnEngSrc_GetClientOldTime_t			hudGetClientOldTime;
	pfnEngSrc_GetServerGravityValue_t		hudGetServerGravityValue;
	pfnEngSrc_GetModelByIndex_t				hudGetModelByIndex;
	pfnEngSrc_pfnSetFilterMode_t			pfnSetFilterMode;
	pfnEngSrc_pfnSetFilterColor_t			pfnSetFilterColor;
	pfnEngSrc_pfnSetFilterBrightness_t		pfnSetFilterBrightness;
	pfnEngSrc_pfnSequenceGet_t				pfnSequenceGet;
	pfnEngSrc_pfnSPR_DrawGeneric_t			pfnSPR_DrawGeneric;
	pfnEngSrc_pfnSequencePickSentence_t		pfnSequencePickSentence;
	pfnEngSrc_pfnDrawString_t				pfnDrawString;
	pfnEngSrc_pfnDrawStringReverse_t				pfnDrawStringReverse;
	pfnEngSrc_LocalPlayerInfo_ValueForKey_t		LocalPlayerInfo_ValueForKey;
	pfnEngSrc_pfnVGUI2DrawCharacter_t		pfnVGUI2DrawCharacter;
	pfnEngSrc_pfnVGUI2DrawCharacterAdd_t	pfnVGUI2DrawCharacterAdd;
	pfnEngSrc_COM_GetApproxWavePlayLength	COM_GetApproxWavePlayLength;
	pfnEngSrc_pfnGetCareerUI_t				pfnGetCareerUI;
	pfnEngSrc_Cvar_Set_t					Cvar_Set;
	pfnEngSrc_pfnIsPlayingCareerMatch_t		pfnIsCareerMatch;
	pfnEngSrc_pfnPlaySoundVoiceByName_t	pfnPlaySoundVoiceByName;
	pfnEngSrc_pfnPrimeMusicStream_t		pfnPrimeMusicStream;
	pfnEngSrc_GetAbsoluteTime_t				GetAbsoluteTime;
	pfnEngSrc_pfnProcessTutorMessageDecayBuffer_t		pfnProcessTutorMessageDecayBuffer;
	pfnEngSrc_pfnConstructTutorMessageDecayBuffer_t		pfnConstructTutorMessageDecayBuffer;
	pfnEngSrc_pfnResetTutorMessageDecayData_t		pfnResetTutorMessageDecayData;
	pfnEngSrc_pfnPlaySoundByNameAtPitch_t	pfnPlaySoundByNameAtPitch;
	pfnEngSrc_pfnFillRGBABlend_t					pfnFillRGBABlend;
	pfnEngSrc_pfnGetAppID_t					pfnGetAppID;
	pfnEngSrc_pfnGetAliases_t				pfnGetAliasList;
	pfnEngSrc_pfnVguiWrap2_GetMouseDelta_t pfnVguiWrap2_GetMouseDelta;
	pfnEngSrc_pfnFilteredClientCmd_t		pfnFilteredClientCmd;
} cl_enginefunc_t;

extern	struct demo_api_s	demoapi;
extern	struct net_api_s	netapi;
extern	struct IVoiceTweak_s	g_VoiceTweakAPI;
extern	struct efx_api_s	efx;
extern	struct event_api_s	eventapi;

cl_enginefunc_t cl_enginefuncs =
{
	SPR_Load,
	SPR_Frames,
	SPR_Height,
	SPR_Width,
	SPR_Set,
	SPR_Draw,
	SPR_DrawHoles,
	SPR_DrawAdditive,
	SPR_EnableScissor,
	SPR_DisableScissor,
	SPR_GetList,
	Draw_FillRGBA,
	hudGetScreenInfo,
	SetCrosshair,
	hudRegisterVariable,
	hudGetCvarFloat,
	hudGetCvarString,
	hudAddCommand,
	hudHookUserMsg,
	hudServerCmd,
	hudClientCmd,
	hudGetPlayerInfo,
	hudPlaySoundByName,
	hudPlaySoundByIndex,
	AngleVectors,
	TextMessageGet,
	TextMessageDrawCharacter,
	Draw_String,
	Draw_SetTextColor,
	hudDrawConsoleStringLen,
	hudConsolePrint,
	hudCenterPrint,
	hudCenterX,
	hudCenterY,
	hudGetViewAngles,
	hudSetViewAngles,
	hudGetMaxClients,
	Cvar_SetValue,
	Cmd_Argc,
	Cmd_Argv,
	Con_Printf,
	Con_DPrintf,
	Con_NPrintf,
	Con_NXPrintf,
	hudPhysInfo_ValueForKey,
	hudServerInfo_ValueForKey,
	hudGetClientMaxspeed,
	hudCheckParm,
	Key_Event,
	hudGetMousePosition,
	hudIsNoClipping,
	hudGetLocalPlayer,
	hudGetViewModel,
	hudGetEntityByIndex,
	hudGetClientTime,
	V_CalcShake,
	V_ApplyShake,
	PM_PointContents,
	PM_WaterEntity,
	PM_TraceLine,
	CL_LoadModel,
	CL_CreateVisibleEntity,
	hudGetSpritePointer,
	hudPlaySoundByNameAtLocation,
	hudPrecacheEvent,
	hudPlaybackEvent,
	hudWeaponAnim,
	RandomFloat,
	RandomLong,
	CL_HookEvent,
	Con_IsVisible,
	hudGetGameDir,
	Cvar_FindVar,
	Key_NameForBinding,
	hudGetLevelName,
	hudGetScreenFade,
	hudSetScreenFade,
	VGuiWrap_GetPanel,
	VGui_ViewportPaintBackground,
	COM_LoadFile,
	COM_ParseFile,
	COM_FreeFile,
	&tri,
	&efx,
	&eventapi,
	&demoapi,
	&netapi,
	&g_VoiceTweakAPI,
	CL_IsSpectateOnly,
	R_LoadMapSprite,
	COM_AddAppDirectoryToSearchPath,
	ClientDLL_ExpandFileName,
	PlayerInfo_ValueForKey,
	PlayerInfo_SetValueForKey,
	GetPlayerUniqueID,
	GetTrackerIDForPlayer,
	GetPlayerForTrackerID,
	hudServerCmdUnreliable,
	GetMousePos_guess,
	SetMousePos_guess,
	SetMouseEnable,
	GetFirstCVarPtr,
	GetFirstCmdFunctionHandle,
	GetNextCmdFunctionHandle,
	GetCmdFunctionName,
	hudGetClientOldTime,
	hudGetServerGravityValue,
	hudGetModelByIndex,
	SetFilterMode,
	SetFilterColor,
	SetFilterBrightness,
	SequenceGet,
	SPR_DrawGeneric,
	SequencePickSentence,
	VGUI2_DrawStringClient,
	VGUI2_DrawStringReverseClient,
	LocalPlayerInfo_ValueForKey,
	VGUI2_Draw_Character,
	VGUI2_Draw_CharacterAdd,
	COM_GetApproxWavePlayLength,
	VguiWrap2_GetCareerUI,
	Cvar_Set,
	VGuiWrap2_IsInCareerMatch,
	hudPlaySoundVoiceByName,
	PrimeMusicStream,
	Sys_FloatTime,
	ProcessTutorMessageDecayBuffer,
	ConstructTutorMessageDecayBuffer,
	ResetTutorMessageDecayData,
	hudPlaySoundByNameAtPitch,
	Draw_FillRGBABlend,
	GetGameAppID,
	GetAliasesList,
	VguiWrap2_GetMouseDelta,
	hudFilteredClientCmd,
};

cldll_func_dst_t g_cldstAddrs =
{
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
	NullDst,
};

int			cdll_unknown_guess = 4;	// unreferenced; follows g_cldstAddrs in the retail .data

qboolean	fClientLoaded = false;
HMODULE		hClientDLL = NULL;
int			g_bTamperTrap_guess = 0;
extern	cmdalias_t					*cmd_alias;
extern	cvar_t						sv_gravity;
extern	int							window_center_x, window_center_y;

/*
================
GetAliasesList

================
*/
cmdalias_t *GetAliasesList( void )
{
	return cmd_alias;
}

/*
================
ClientDLL_TamperTrap_guess

Executes an invalid lock prefixed instruction when the flag is set.
================
*/
void ClientDLL_TamperTrap_guess( void )
{
	if ( g_bTamperTrap_guess )
	{
		__asm
		{
			pushfd
			_emit 0xf0
			_emit 0xa8
			_emit 0x36
			_emit 0x00
			_emit 0x00
			_emit 0x00
			_emit 0x00
		}
	}
}

/*
================
ClientDLL_Init

================
*/
void ClientDLL_Init( void )
{
	char	szDllName[512];
	char	szFullName_guess[512];

	_snprintf( szDllName, sizeof( szDllName ), "cl_dlls\\client.dll" );

	if ( fClientLoaded )
		ClientDLL_Shutdown();

	COM_FixSlashes( szDllName );

	ClientDLL_TamperTrap_guess();

	strcpy( szFullName_guess, szDllName );
	COM_ExpandFilename( szFullName_guess );
	COM_FixSlashes( szFullName_guess );

	if ( FIsBlob( szDllName ) )
	{
		cls.fSecureClient = true;
		LoadSecureClient( szDllName );
	}
	else
	{
		cls.fSecureClient = false;
		LoadInsecureClient( szFullName_guess );
	}

	HookServerMsg( "ScreenShake", V_ScreenShake );
	HookServerMsg( "ScreenFade", V_ScreenFade );

	cl_funcs.pInitFunc( &cl_enginefuncs, CLDLL_INTERFACE_VERSION );

	ClientDLL_ClientMoveInit( &g_clmove );

	CL_GetPlayerHulls();

	if ( cl_funcs.pClientFactory && cl_funcs.pClientFactory() )
	{
		IGA_ClientFactory_guess( cl_funcs.pClientFactory() );
	}
}

/*
================
LoadSecureClient

================
*/
void LoadSecureClient( char *pszDllName )
{
	// the first two slots pass the module functions and the destination addresses to the blob
	cl_funcs.pInitFunc = (INITIALIZE_FUNC)&g_modfuncs;
	cl_funcs.pHudVidInitFunc = (HUD_VIDINIT_FUNC)&g_cldstAddrs;

	NLoadBlobFile( pszDllName, &g_blobfootprintClient_guess, &cl_funcs, false );

	fClientLoaded = true;
}

/*
================
LoadInsecureClient

================
*/
void LoadInsecureClient( char *pszFullPathClientDLLName )
{
	hClientDLL = FS_LoadLibrary( pszFullPathClientDLLName );
	if ( !hClientDLL )
		Sys_Error( "could not load library %s", pszFullPathClientDLLName );

	fClientLoaded = true;

	cl_funcs.pInitFunc = (INITIALIZE_FUNC)GetProcAddress( hClientDLL, "Initialize" );
	if ( !cl_funcs.pInitFunc )
		Sys_Error( "could not link client.dll function Initialize\n" );

	cl_funcs.pHudVidInitFunc = (HUD_VIDINIT_FUNC)GetProcAddress( hClientDLL, "HUD_VidInit" );
	if ( !cl_funcs.pHudVidInitFunc )
		Sys_Error( "could not link client.dll function HUD_VidInit\n" );

	cl_funcs.pHudInitFunc = (HUD_INIT_FUNC)GetProcAddress( hClientDLL, "HUD_Init" );
	if ( !cl_funcs.pHudInitFunc )
		Sys_Error( "could not link client.dll function HUD_Init\n" );

	cl_funcs.pHudRedrawFunc = (HUD_REDRAW_FUNC)GetProcAddress( hClientDLL, "HUD_Redraw" );
	if ( !cl_funcs.pHudRedrawFunc )
		Sys_Error( "could not link client.dll function HUD_Redraw\n" );

	cl_funcs.pHudUpdateClientDataFunc = (HUD_UPDATECLIENTDATA_FUNC)GetProcAddress( hClientDLL, "HUD_UpdateClientData" );
	if ( !cl_funcs.pHudUpdateClientDataFunc )
		Sys_Error( "could not link client.dll function HUD_UpdateClientData\n" );

	cl_funcs.pHudResetFunc = (HUD_RESET_FUNC)GetProcAddress( hClientDLL, "HUD_Reset" );
	if ( !cl_funcs.pHudResetFunc )
		Sys_Error( "could not link client.dll function HUD_Reset\n" );

	cl_funcs.pClientMove = (HUD_CLIENTMOVE_FUNC)GetProcAddress( hClientDLL, "HUD_PlayerMove" );
	if ( !cl_funcs.pClientMove )
		Sys_Error( "could not link client.dll function HUD_PlayerMove\n" );

	cl_funcs.pClientMoveInit = (HUD_CLIENTMOVEINIT_FUNC)GetProcAddress( hClientDLL, "HUD_PlayerMoveInit" );
	if ( !cl_funcs.pClientMoveInit )
		Sys_Error( "could not link client.dll function HUD_PlayerMoveInit\n" );

	cl_funcs.pClientTextureType = (HUD_TEXTURETYPE_FUNC)GetProcAddress( hClientDLL, "HUD_PlayerMoveTexture" );
	if ( !cl_funcs.pClientTextureType )
		Sys_Error( "could not link client.dll function HUD_PlayerMoveTexture\n" );

	cl_funcs.pIN_ActivateMouse = (HUD_IN_ACTIVATEMOUSE_FUNC)GetProcAddress( hClientDLL, "IN_ActivateMouse" );
	if ( !cl_funcs.pIN_ActivateMouse )
		Sys_Error( "could not link client.dll function IN_ActivateMouse\n" );

	cl_funcs.pIN_DeactivateMouse = (HUD_IN_DEACTIVATEMOUSE_FUNC)GetProcAddress( hClientDLL, "IN_DeactivateMouse" );
	if ( !cl_funcs.pIN_DeactivateMouse )
		Sys_Error( "could not link client.dll function IN_DeactivateMouse\n" );

	cl_funcs.pIN_MouseEvent = (HUD_IN_MOUSEEVENT_FUNC)GetProcAddress( hClientDLL, "IN_MouseEvent" );
	if ( !cl_funcs.pIN_MouseEvent )
		Sys_Error( "could not link client.dll function IN_MouseEvent\n" );

	cl_funcs.pIN_ClearStates = (HUD_IN_CLEARSTATES_FUNC)GetProcAddress( hClientDLL, "IN_ClearStates" );
	if ( !cl_funcs.pIN_ClearStates )
		Sys_Error( "could not link client.dll function IN_ClearStates\n" );

	cl_funcs.pIN_Accumulate = (HUD_IN_ACCUMULATE_FUNC)GetProcAddress( hClientDLL, "IN_Accumulate" );
	if ( !cl_funcs.pIN_Accumulate )
		Sys_Error( "could not link client.dll function IN_Accumulate\n" );

	cl_funcs.pCL_CreateMove = (HUD_CL_CREATEMOVE_FUNC)GetProcAddress( hClientDLL, "CL_CreateMove" );
	if ( !cl_funcs.pCL_CreateMove )
		Sys_Error( "could not link client.dll function CL_CreateMove\n" );

	cl_funcs.pCL_IsThirdPerson = (HUD_CL_ISTHIRDPERSON_FUNC)GetProcAddress( hClientDLL, "CL_IsThirdPerson" );
	if ( !cl_funcs.pCL_IsThirdPerson )
		Sys_Error( "could not link client.dll function CL_IsThirdPerson\n" );

	cl_funcs.pCL_GetCameraOffsets = (HUD_CL_GETCAMERAOFFSETS_FUNC)GetProcAddress( hClientDLL, "CL_CameraOffset" );
	if ( !cl_funcs.pCL_GetCameraOffsets )
		Sys_Error( "could not link client.dll function CL_CameraOffset\n" );

	cl_funcs.pCamThink = (HUD_CAMTHINK_FUNC)GetProcAddress( hClientDLL, "CAM_Think" );
	if ( !cl_funcs.pCamThink )
		Sys_Error( "could not link client.dll function CAM_Think\n" );

	cl_funcs.pFindKey = (HUD_KB_FIND_FUNC)GetProcAddress( hClientDLL, "KB_Find" );
	if ( !cl_funcs.pFindKey )
		Sys_Error( "could not link client.dll function KB_Find\n" );

	cl_funcs.pCalcRefdef = (HUD_CALCREF_FUNC)GetProcAddress( hClientDLL, "V_CalcRefdef" );
	if ( !cl_funcs.pCalcRefdef )
		Sys_Error( "could not link client.dll function V_CalcRefdef\n" );

	cl_funcs.pAddEntity = (HUD_ADDENTITY_FUNC)GetProcAddress( hClientDLL, "HUD_AddEntity" );
	if ( !cl_funcs.pAddEntity )
		Sys_Error( "could not link client.dll function HUD_AddEntity\n" );

	cl_funcs.pCreateEntities = (HUD_CREATEENTITIES_FUNC)GetProcAddress( hClientDLL, "HUD_CreateEntities" );
	if ( !cl_funcs.pCreateEntities )
		Sys_Error( "could not link client.dll function HUD_CreateEntities\n" );

	cl_funcs.pDrawNormalTriangles = (HUD_DRAWNORMALTRIS_FUNC)GetProcAddress( hClientDLL, "HUD_DrawNormalTriangles" );
	if ( !cl_funcs.pDrawNormalTriangles )
		Sys_Error( "could not link client.dll function HUD_DrawNormalTriangles\n" );

	cl_funcs.pDrawTransparentTriangles = (HUD_DRAWTRANSTRIS_FUNC)GetProcAddress( hClientDLL, "HUD_DrawTransparentTriangles" );
	if ( !cl_funcs.pDrawTransparentTriangles )
		Sys_Error( "could not link client.dll function HUD_DrawTransparentTriangles\n" );

	cl_funcs.pStudioEvent = (HUD_STUDIOEVENT_FUNC)GetProcAddress( hClientDLL, "HUD_StudioEvent" );
	if ( !cl_funcs.pStudioEvent )
		Sys_Error( "could not link client.dll function HUD_StudioEvent\n" );

	cl_funcs.pShutdown = (HUD_SHUTDOWN_FUNC)GetProcAddress( hClientDLL, "HUD_Shutdown" );
	if ( !cl_funcs.pShutdown )
		Sys_Error( "could not link client.dll function HUD_Shutdown\n" );

	cl_funcs.pTxferLocalOverrides = (HUD_TXFERLOCALOVERRIDES_FUNC)GetProcAddress( hClientDLL, "HUD_TxferLocalOverrides" );
	if ( !cl_funcs.pTxferLocalOverrides )
		Sys_Error( "could not link client.dll function HUD_TxferLocalOverrides\n" );

	cl_funcs.pProcessPlayerState = (HUD_PROCESSPLAYERSTATE_FUNC)GetProcAddress( hClientDLL, "HUD_ProcessPlayerState" );
	if ( !cl_funcs.pProcessPlayerState )
		Sys_Error( "could not link client.dll function HUD_ProcessPlayerState\n" );

	cl_funcs.pTxferPredictionData = (HUD_TXFERPREDICTIONDATA_FUNC)GetProcAddress( hClientDLL, "HUD_TxferPredictionData" );
	if ( !cl_funcs.pTxferPredictionData )
		Sys_Error( "could not link client.dll function HUD_TxferPredictionData\n" );

	cl_funcs.pReadDemoBuffer = (HUD_DEMOREAD_FUNC)GetProcAddress( hClientDLL, "Demo_ReadBuffer" );
	if ( !cl_funcs.pReadDemoBuffer )
		Sys_Error( "could not link client.dll function Demo_ReadBuffer\n" );

	cl_funcs.pConnectionlessPacket = (HUD_CONNECTIONLESS_FUNC)GetProcAddress( hClientDLL, "HUD_ConnectionlessPacket" );
	if ( !cl_funcs.pConnectionlessPacket )
		Sys_Error( "could not link client.dll function HUD_ConnectionlessPacket\n" );

	cl_funcs.pGetHullBounds = (HUD_GETHULLBOUNDS_FUNC)GetProcAddress( hClientDLL, "HUD_GetHullBounds" );
	if ( !cl_funcs.pGetHullBounds )
		Sys_Error( "could not link client.dll function HUD_GetHullBounds\n" );

	cl_funcs.pHudFrame = (HUD_FRAME_FUNC)GetProcAddress( hClientDLL, "HUD_Frame" );
	if ( !cl_funcs.pHudFrame )
		Sys_Error( "could not link client.dll function HUD_Frame\n" );

	cl_funcs.pKeyEvent = (HUD_KEY_EVENT_FUNC)GetProcAddress( hClientDLL, "HUD_Key_Event" );
	if ( !cl_funcs.pKeyEvent )
		Sys_Error( "could not link client.dll function HUD_Key_Event\n" );

	cl_funcs.pPostRunCmd = (HUD_POSTRUNCMD_FUNC)GetProcAddress( hClientDLL, "HUD_PostRunCmd" );
	if ( !cl_funcs.pPostRunCmd )
		Sys_Error( "could not link client.dll function HUD_PostRunCmd\n" );

	cl_funcs.pTempEntUpdate = (HUD_TEMPENTUPDATE_FUNC)GetProcAddress( hClientDLL, "HUD_TempEntUpdate" );
	if ( !cl_funcs.pTempEntUpdate )
		Sys_Error( "could not link client.dll function HUD_TempEntUpdate\n" );

	cl_funcs.pGetUserEntity = (HUD_GETUSERENTITY_FUNC)GetProcAddress( hClientDLL, "HUD_GetUserEntity" );
	if ( !cl_funcs.pGetUserEntity )
		Sys_Error( "could not link client.dll function HUD_GetUserEntity\n" );

	// optional exports
	cl_funcs.pVoiceStatus = (HUD_VOICESTATUS_FUNC)GetProcAddress( hClientDLL, "HUD_VoiceStatus" );
	cl_funcs.pDirectorMessage = (HUD_DIRECTORMESSAGE_FUNC)GetProcAddress( hClientDLL, "HUD_DirectorMessage" );
	cl_funcs.pStudioInterface = (HUD_STUDIO_INTERFACE_FUNC)GetProcAddress( hClientDLL, "HUD_GetStudioModelInterface" );
	cl_funcs.pChatInputPosition = (HUD_CHATINPUTPOSITION_FUNC)GetProcAddress( hClientDLL, "HUD_ChatInputPosition" );
	cl_funcs.pClientFactory = (CLIENTFACTORY)GetProcAddress( hClientDLL, "ClientFactory" );
}

/*
================
NCallDummy

================
*/
int NCallDummy( int ijump, int cnArg )
{
	return 0;
}

/*
================
ClientDLL_GetUserEntity

================
*/
cl_entity_t *ClientDLL_GetUserEntity( int index )
{
	if ( cl_funcs.pGetUserEntity )
		return cl_funcs.pGetUserEntity( index );

	return NULL;
}

/*
================
ClientDLL_VoiceStatus

================
*/
void ClientDLL_VoiceStatus( int entindex, qboolean bTalking )
{
	if ( cl_funcs.pVoiceStatus )
		cl_funcs.pVoiceStatus( entindex, bTalking );
}

/*
================
ClientDLL_ChatInputPosition

================
*/
void ClientDLL_ChatInputPosition( int *x, int *y )
{
	if ( cl_funcs.pChatInputPosition )
		cl_funcs.pChatInputPosition( x, y );
}

/*
================
ClientDLL_TempEntUpdate

================
*/
void ClientDLL_TempEntUpdate( double ft, double ct, double grav, struct tempent_s **ppFreeTE, struct tempent_s **ppActiveTE, int ( *addTEntity )( struct cl_entity_s *pEntity ), void ( *playTESound )( struct tempent_s *pTemp, float damp ) )
{
	if ( !cl_funcs.pTempEntUpdate )
		return;

	cl_funcs.pTempEntUpdate( ft, ct, grav, ppFreeTE, ppActiveTE, addTEntity, playTESound );
}

/*
================
ClientDLL_DirectorMessage

================
*/
void ClientDLL_DirectorMessage( int iSize, void *pbuf )
{
	if ( !cl_funcs.pDirectorMessage )
		return;

	cl_funcs.pDirectorMessage( iSize, pbuf );
}

/*
================
ClientDLL_Key_Event

================
*/
int ClientDLL_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	if ( !VGui_Key_Event( down, keynum, pszCurrentBinding ) )
		return 0;

	if ( cl_funcs.pKeyEvent )
		return cl_funcs.pKeyEvent( down, keynum, pszCurrentBinding );

	return 1;
}

/*
================
ClientDLL_GetHullBounds

================
*/
int ClientDLL_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	if ( cl_funcs.pGetHullBounds )
		return cl_funcs.pGetHullBounds( hullnumber, mins, maxs );

	return 0;
}

/*
================
ClientDLL_ConnectionlessPacket

================
*/
int ClientDLL_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	if ( cl_funcs.pConnectionlessPacket )
		return cl_funcs.pConnectionlessPacket( net_from, args, response_buffer, response_buffer_size );

	return 0;
}

/*
================
ClientDLL_PostRunCmd

================
*/
void ClientDLL_PostRunCmd( struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed )
{
	if ( cl_funcs.pPostRunCmd )
	{
		cl_funcs.pPostRunCmd( from, to, cmd, runfuncs, time, random_seed );
	}
	else
	{
		if ( cl_lw.value )
		{
			Cvar_SetValue( "cl_lw", 0 );
		}
	}
}

/*
================
ClientDLL_StudioEvent

================
*/
void ClientDLL_StudioEvent( const struct mstudioevent_s *event, const struct cl_entity_s *entity )
{
	if ( cl_funcs.pStudioEvent )
		cl_funcs.pStudioEvent( event, entity );
}

/*
================
ClientDLL_DrawNormalTriangles

================
*/
void ClientDLL_DrawNormalTriangles( void )
{
	if ( cl_funcs.pDrawNormalTriangles )
		cl_funcs.pDrawNormalTriangles();

	tri.RenderMode( kRenderNormal );
}

/*
================
ClientDLL_DrawTransparentTriangles

================
*/
void ClientDLL_DrawTransparentTriangles( void )
{
	if ( cl_funcs.pDrawTransparentTriangles )
		cl_funcs.pDrawTransparentTriangles();

	tri.RenderMode( kRenderNormal );
}

/*
================
ClientDLL_CreateEntities

================
*/
void ClientDLL_CreateEntities( void )
{
	if ( cl_funcs.pCreateEntities )
		cl_funcs.pCreateEntities();
}

/*
================
ClientDLL_AddEntity

================
*/
int ClientDLL_AddEntity( int type, struct cl_entity_s *ent )
{
	vec3_t	emins, emaxs;
	int		i;

	if ( !cl_funcs.pAddEntity )
		return 0;

	if ( !cl_funcs.pAddEntity( type, ent, ent->model ? ent->model->name : "" ) )
		return 0;

	if ( CL_IsSpectateOnly() && !CL_IsDevOverviewMode() && type != ET_TEMPENTITY && type != ET_BEAM && ent->curstate.movetype != MOVETYPE_FOLLOW )
	{
		for ( i = 0; i < 3; i++ )
		{
			emins[i] = ent->origin[i] + ent->model->mins[i];
			emaxs[i] = ent->origin[i] + ent->model->maxs[i];
		}

		if ( !PVSNode( cl.worldmodel->nodes, emins, emaxs ) )
			return 0;
	}

	switch ( type )
	{
	case ET_BEAM:
		if ( cl_numbeamentities < MAX_BEAMENTS )
		{
			cl_beamentities[cl_numbeamentities] = ent;
			cl_numbeamentities++;
		}
		else
		{
			return 0;
		}
		break;

	default:
		if ( cl_numvisedicts < MAX_VISEDICTS )
		{
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
		else
		{
			return 0;
		}
		break;
	}

	return 1;
}

/*
================
ClientDLL_CalcRefdef

================
*/
void ClientDLL_CalcRefdef( struct ref_params_s *pparams )
{
	if ( cl_funcs.pCalcRefdef )
		cl_funcs.pCalcRefdef( pparams );
}

/*
================
ClientDLL_CAM_Think

================
*/
void ClientDLL_CAM_Think( void )
{
	if ( cls.state == ca_dedicated || cls.state == ca_disconnected )
		return;

	if ( cl_funcs.pCamThink )
		cl_funcs.pCamThink();
}

/*
================
ClientDLL_FindKey

================
*/
struct kbutton_s *ClientDLL_FindKey( const char *name )
{
	if ( cl_funcs.pFindKey )
		return cl_funcs.pFindKey( name );

	return NULL;
}

/*
================
ClientDLL_GetCameraOffsets

================
*/
void ClientDLL_GetCameraOffsets( float *ofs )
{
	if ( cl_funcs.pCL_GetCameraOffsets )
		cl_funcs.pCL_GetCameraOffsets( ofs );
}

/*
================
ClientDLL_IsThirdPerson

================
*/
int ClientDLL_IsThirdPerson( void )
{
	if ( cl_funcs.pCL_IsThirdPerson )
		return cl_funcs.pCL_IsThirdPerson();

	return 0;
}

/*
================
ClientDLL_CreateMove

================
*/
void ClientDLL_CreateMove( float frametime, struct usercmd_s *cmd, int active )
{
	if ( cl_funcs.pCL_CreateMove )
		cl_funcs.pCL_CreateMove( frametime, cmd, active );
}

/*
================
ClientDLL_ActivateMouse

================
*/
void ClientDLL_ActivateMouse( void )
{
	if ( !fClientLoaded || !cl_funcs.pIN_ActivateMouse )
		return;

	cl_funcs.pIN_ActivateMouse();
}

/*
================
ClientDLL_DeactivateMouse

================
*/
void ClientDLL_DeactivateMouse( void )
{
	if ( !fClientLoaded || !cl_funcs.pIN_DeactivateMouse )
		return;

	cl_funcs.pIN_DeactivateMouse();
}

/*
================
ClientDLL_MouseEvent

================
*/
void ClientDLL_MouseEvent( int mstate )
{
	if ( !fClientLoaded || !cl_funcs.pIN_MouseEvent )
		return;

	cl_funcs.pIN_MouseEvent( mstate );
}

/*
================
ClientDLL_ClearStates

================
*/
void ClientDLL_ClearStates( void )
{
	if ( !fClientLoaded || !cl_funcs.pIN_ClearStates )
		return;

	cl_funcs.pIN_ClearStates();
}

/*
================
ClientDLL_IN_Accumulate

================
*/
void ClientDLL_IN_Accumulate( void )
{
	if ( !fClientLoaded || !cl_funcs.pIN_Accumulate )
		return;

	cl_funcs.pIN_Accumulate();
}

/*
================
ClientDLL_ClientMoveInit

================
*/
void ClientDLL_ClientMoveInit( struct playermove_s *ppmove )
{
	if ( cl_funcs.pClientMoveInit )
		cl_funcs.pClientMoveInit( ppmove );
}

/*
================
ClientDLL_ClientTextureType

================
*/
char ClientDLL_ClientTextureType( char *name )
{
	if ( cl_funcs.pClientTextureType )
		return cl_funcs.pClientTextureType( name );

	return 0;
}

/*
================
ClientDLL_MoveClient

================
*/
void ClientDLL_MoveClient( struct playermove_s *ppmove )
{
	if ( cl_funcs.pClientMove )
		cl_funcs.pClientMove( ppmove, false );
}

/*
================
ClientDLL_Shutdown

================
*/
void ClientDLL_Shutdown( void )
{
	Net_APIShutDown();
	SPR_Shutdown();

	if ( cl_funcs.pShutdown )
		cl_funcs.pShutdown();

	if ( cls.fSecureClient )
	{
		FreeBlob( &g_blobfootprintClient_guess );
	}
	else
	{
		FreeLibrary( hClientDLL );
		hClientDLL = NULL;
	}

	Q_memset( &cl_funcs, 0, sizeof( cl_funcs ) );

	Cvar_RemoveHudCvars();
	Cmd_RemoveHudCmds();
	CL_ShutDownUsrMessages();

	fClientLoaded = false;

	FreeLibrary( hClientDLL );
	hClientDLL = NULL;
}

/*
================
ClientDLL_HudVidInit

================
*/
void ClientDLL_HudVidInit( void )
{
	if ( !cl_funcs.pHudVidInitFunc )
		Sys_Error( "C:\\Documents and Settings\\XPMUser\\Desktop\\GoldSource\\engine\\cdll_int.c, line %d: could not link client DLL for HUD Vid initialization", 1074 );

	SPR_Init();

	cl_funcs.pHudVidInitFunc();
}

/*
================
ClientDLL_HudInit

================
*/
void ClientDLL_HudInit( void )
{
	if ( !cl_funcs.pHudInitFunc )
		Sys_Error( "C:\\Documents and Settings\\XPMUser\\Desktop\\GoldSource\\engine\\cdll_int.c, line %d: could not link client DLL for HUD initialization", 1093 );

	cl_funcs.pHudInitFunc();

	ClientDLL_CheckStudioInterface( hClientDLL );

	cl_righthand = Cvar_FindVar( "cl_righthand" );
}

/*
================
ClientDLL_HudRedraw

================
*/
void ClientDLL_HudRedraw( int intermission )
{
	if ( VGuiWrap2_IsGameUIVisible() )
		return;

	if ( cl_funcs.pHudRedrawFunc )
		cl_funcs.pHudRedrawFunc( (float)cl.time, intermission );
}

/*
================
ClientDLL_Frame

================
*/
void ClientDLL_Frame( double time )
{
	if ( !fClientLoaded || !cl_funcs.pHudFrame )
		return;

	cl_funcs.pHudFrame( time );
}

/*
================
ClientDLL_UpdateClientData

================
*/
void ClientDLL_UpdateClientData( void )
{
	client_data_t	oldcdat;
	client_data_t	cdat;

	if ( cls.state != ca_active )
		return;

	if ( cls.demoplayback )
		return;

	Q_memset( &cdat, 0, sizeof( cdat ) );

	VectorCopy( cl.viewangles, cdat.viewangles );
	VectorCopy( cl_entities[cl.viewentity].origin, cdat.origin );
	cdat.fov = scr_fov_value;
	cdat.iWeaponBits = cl.weapons;

	if ( cls.demorecording )
		oldcdat = cdat;

	if ( cl_funcs.pHudUpdateClientDataFunc && cl_funcs.pHudUpdateClientDataFunc( &cdat, cl.time ) )
	{
		VectorCopy( cdat.viewangles, cl.viewangles );
		scr_fov_value = cdat.fov;
	}

	if ( cls.demorecording )
		CL_WriteDLLUpdate( &oldcdat );
}

/*
================
ClientDLL_DemoUpdateClientData

================
*/
void ClientDLL_DemoUpdateClientData( client_data_t *cdat )
{
	if ( cl_funcs.pHudUpdateClientDataFunc )
	{
		if ( cl_funcs.pHudUpdateClientDataFunc( cdat, (float)cl.time ) && !cls.spectator )
		{
			VectorCopy( cdat->viewangles, cl.viewangles );
			scr_fov_value = cdat->fov;
		}
	}
}

/*
================
COM_ParseFile

================
*/
char *COM_ParseFile( char *data, char *token )
{
	char	*return_data;

	g_engdstAddrs.COM_ParseFile( &data, &token );

	return_data = COM_Parse( data );
	Q_strcpy( token, com_token );

	return return_data;
}

/*
================
ClientDLL_TxferLocalOverrides

================
*/
void ClientDLL_TxferLocalOverrides( struct entity_state_s *state, const struct clientdata_s *client )
{
	if ( cl_funcs.pTxferLocalOverrides )
		cl_funcs.pTxferLocalOverrides( state, client );
}

/*
================
ClientDLL_ProcessPlayerState

================
*/
void ClientDLL_ProcessPlayerState( struct entity_state_s *dst, const struct entity_state_s *src )
{
	if ( cl_funcs.pProcessPlayerState )
		cl_funcs.pProcessPlayerState( dst, src );
}

/*
================
ClientDLL_TxferPredictionData

================
*/
void ClientDLL_TxferPredictionData( struct entity_state_s *ps, const struct entity_state_s *pps, struct clientdata_s *pcd, const struct clientdata_s *ppcd, struct weapon_data_s *wd, const struct weapon_data_s *pwd )
{
	if ( cl_funcs.pTxferPredictionData )
		cl_funcs.pTxferPredictionData( ps, pps, pcd, ppcd, wd, pwd );
}

/*
================
ClientDLL_ReadDemoBuffer

================
*/
void ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer )
{
	if ( cl_funcs.pReadDemoBuffer )
		cl_funcs.pReadDemoBuffer( size, buffer );
}

/*
================
SPR_GetList

================
*/
client_sprite_t *SPR_GetList( char *psz, int *piCount )
{
	char			*pfile;
	int				iCount;
	int				i;
	client_sprite_t	*ps, *pret;

	g_engdstAddrs.pfnSPR_GetList( &psz, &piCount );

	pfile = (char *)COM_LoadFile( psz, 2, NULL );
	if ( !pfile )
		return NULL;

	pfile = COM_Parse( pfile );
	iCount = atoi( com_token );
	if ( !iCount )
		return NULL;

	ps = (client_sprite_t *)calloc( iCount * sizeof( client_sprite_t ), 1 );
	if ( !ps )
		return ps;

	pret = ps;

	for ( i = 0; i < iCount; i++ )
	{
		pfile = COM_Parse( pfile );
		Q_strncpy( ps->szName, com_token, sizeof( ps->szName ) - 1 );
		ps->szName[sizeof( ps->szName ) - 1] = 0;

		pfile = COM_Parse( pfile );
		ps->iRes = atoi( com_token );

		pfile = COM_Parse( pfile );
		Q_strncpy( ps->szSprite, com_token, sizeof( ps->szSprite ) - 1 );
		ps->szSprite[sizeof( ps->szSprite ) - 1] = 0;

		pfile = COM_Parse( pfile );
		ps->rc.left = atoi( com_token );

		pfile = COM_Parse( pfile );
		ps->rc.top = atoi( com_token );

		pfile = COM_Parse( pfile );
		ps->rc.right = atoi( com_token ) + ps->rc.left;

		pfile = COM_Parse( pfile );
		ps->rc.bottom = atoi( com_token ) + ps->rc.top;

		ps++;
	}

	if ( piCount )
		*piCount = iCount;

	return pret;
}

/*
================
hudCenterX

================
*/
int hudCenterX( void )
{
	g_engdstAddrs.GetWindowCenterX();

	return window_center_x;
}

/*
================
hudCenterY

================
*/
int hudCenterY( void )
{
	g_engdstAddrs.GetWindowCenterY();

	return window_center_y;
}

/*
================
hudGetViewAngles

================
*/
void hudGetViewAngles( float *va )
{
	g_engdstAddrs.GetViewAngles( &va );

	VectorCopy( cl.viewangles, va );
}

/*
================
hudSetViewAngles

================
*/
void hudSetViewAngles( float *va )
{
	g_engdstAddrs.SetViewAngles( &va );

	VectorCopy( va, cl.viewangles );
}

/*
================
hudGetMaxClients

================
*/
int hudGetMaxClients( void )
{
	g_engdstAddrs.GetMaxClients();

	return cl.maxclients;
}

/*
================
hudPhysInfo_ValueForKey

================
*/
const char *hudPhysInfo_ValueForKey( const char *key )
{
	g_engdstAddrs.PhysInfo_ValueForKey( &key );

	return Info_ValueForKey( cls.physinfo, key );
}

/*
================
hudServerInfo_ValueForKey

================
*/
const char *hudServerInfo_ValueForKey( const char *key )
{
	g_engdstAddrs.ServerInfo_ValueForKey( &key );

	return Info_ValueForKey( cl.serverinfo, key );
}

/*
================
hudGetMousePosition

================
*/
void hudGetMousePosition( int *mx, int *my )
{
	POINT	current_pos_guess;

	g_engdstAddrs.GetMousePosition( &mx, &my );

	GetCursorPos( &current_pos_guess );
	ScreenToClient( *pmainwindow, &current_pos_guess );

	*mx = current_pos_guess.x;
	*my = current_pos_guess.y;
}

/*
================
hudIsNoClipping

================
*/
int hudIsNoClipping( void )
{
	g_engdstAddrs.IsNoClipping();

	return cl.frames[cl.parsecountmod].playerstate[cl.playernum].movetype == MOVETYPE_NOCLIP;
}

/*
================
hudGetLocalPlayer

================
*/
struct cl_entity_s *hudGetLocalPlayer( void )
{
	g_engdstAddrs.GetLocalPlayer();

	return &cl_entities[cl.playernum + 1];
}

/*
================
hudGetViewModel

================
*/
struct cl_entity_s *hudGetViewModel( void )
{
	g_engdstAddrs.GetViewModel();

	return &cl.viewent;
}

/*
================
hudGetEntityByIndex

================
*/
struct cl_entity_s *hudGetEntityByIndex( int idx )
{
	g_engdstAddrs.GetEntityByIndex( &idx );

	if ( idx < 0 || idx >= cl.max_edicts )
		return NULL;

	return &cl_entities[idx];
}

/*
================
hudGetClientTime

================
*/
float hudGetClientTime( void )
{
	g_engdstAddrs.GetClientTime();

	return cl.time;
}

/*
================
hudGetClientOldTime

================
*/
float hudGetClientOldTime( void )
{
	return cl.oldtime;
}

/*
================
hudGetServerGravityValue

================
*/
float hudGetServerGravityValue( void )
{
	return sv_gravity.value;
}

/*
================
hudGetModelByIndex

================
*/
struct model_s *hudGetModelByIndex( const int index )
{
	return CL_GetModelByIndex( index );
}

/*
================
CL_LoadModel

================
*/
struct model_s *CL_LoadModel( const char *modelname, int *index )
{
	int		i = 1;

	g_engdstAddrs.CL_LoadModel( &modelname, &index );

	while ( 1 )
	{
		model_t	*mod = cl.model_precache[i];

		if ( !mod )
			break;

		if ( !Q_stricmp( modelname, mod->name ) )
		{
			mod = CL_GetModelByIndex( i );
			if ( index )
				*index = i;
			return mod;
		}

		i++;
	}

	if ( index )
		*index = -1;

	return NULL;
}

/*
================
CL_CreateVisibleEntity

================
*/
int CL_CreateVisibleEntity( int type, struct cl_entity_s *ent )
{
	int		iret = 0;

	g_engdstAddrs.CL_CreateVisibleEntity( &type, &ent );

	switch ( type )
	{
	case ET_BEAM:
		if ( cl_numbeamentities < MAX_BEAMENTS )
		{
			iret = 1;
			cl_beamentities[cl_numbeamentities] = ent;
			cl_numbeamentities++;
		}
		break;
	default:
		if ( cl_numvisedicts < MAX_VISEDICTS )
		{
			iret = 1;
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
		break;
	}

	return iret;
}

/*
================
hudGetSpritePointer

================
*/
const struct model_s *hudGetSpritePointer( HSPRITE hSprite )
{
	g_engdstAddrs.GetSpritePointer( &hSprite );

	return SPR_GetModelPointer( hSprite );
}

/*
================
hudPrecacheEvent

================
*/
unsigned short hudPrecacheEvent( int type, const char *psz )
{
	int		i;

	g_engdstAddrs.pfnPrecacheEvent( &type, &psz );

	// the loop condition never fails, the loop ends at the first free slot
	for ( i = 1; MAX_EVENTS; i++ )
	{
		if ( !cl.event_precache[i].filename )
			break;

		if ( !Q_stricmp( cl.event_precache[i].filename, psz ) )
			return (unsigned short)i;
	}

	return 0;
}

/*
================
hudPlaybackEvent

================
*/
void hudPlaybackEvent( int flags, const struct edict_s *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t	eargs;

	g_engdstAddrs.pfnPlaybackEvent( &flags, &pInvoker, &eventindex, &delay, &origin, &angles, &fparam1, &fparam2, &iparam1, &iparam2, &bparam1, &bparam2 );

	if ( flags & FEV_SERVER )
		return;

	Q_memset( &eargs, 0, sizeof( eargs ) );

	VectorCopy( origin, eargs.origin );
	VectorCopy( angles, eargs.angles );
	VectorCopy( cl.simvel, eargs.velocity );

	eargs.fparam1 = fparam1;
	eargs.fparam2 = fparam2;
	eargs.iparam1 = iparam1;
	eargs.iparam2 = iparam2;
	eargs.bparam1 = bparam1;
	eargs.bparam2 = bparam2;

	eargs.entindex = cl.playernum + 1;
	eargs.ducking = ( cl.usehull == 1 );

	CL_QueueEvent( flags, eventindex, delay, &eargs );

	if ( cls.demorecording )
		CL_DemoEvent( flags, eventindex, delay, &eargs );
}

/*
================
hudWeaponAnim

================
*/
void hudWeaponAnim( int iAnim, int body )
{
	g_engdstAddrs.pfnWeaponAnim( &iAnim, &body );

	cl.weaponsequence = iAnim;
	cl.weaponstarttime = 0;
	cl.viewent.curstate.body = body;

	if ( cls.demorecording )
		CL_DemoAnim( iAnim, body );
}

/*
================
hudGetClientMaxspeed

================
*/
float hudGetClientMaxspeed( void )
{
	g_engdstAddrs.GetClientMaxspeed();

	return cl.maxspeed;
}

/*
================
hudGetGameDir

================
*/
const char *hudGetGameDir( void )
{
	g_engdstAddrs.pfnGetGameDirectory();

	return com_gamedir;
}

/*
================
hudGetLevelName

================
*/
const char *hudGetLevelName( void )
{
	g_engdstAddrs.pfnGetLevelName();

	if ( cls.state >= ca_connected )
		return cl.levelname;

	return "";
}

/*
================
hudGetScreenFade

================
*/
void hudGetScreenFade( struct screenfade_s *fade )
{
	g_engdstAddrs.pfnGetScreenFade( &fade );

	*fade = cl.sf;
}

/*
================
hudSetScreenFade

================
*/
void hudSetScreenFade( struct screenfade_s *fade )
{
	g_engdstAddrs.pfnSetScreenFade( &fade );

	cl.sf = *fade;
}

/*
================
ClientDLL_CheckStudioInterface

================
*/
void ClientDLL_CheckStudioInterface( HMODULE hClientDLL )
{
	HUD_STUDIO_INTERFACE_FUNC	pStudioInterface;

	R_ResetStudio();

	if ( !fClientLoaded )
		return;

	pStudioInterface = (HUD_STUDIO_INTERFACE_FUNC)GetProcAddress( hClientDLL, "HUD_GetStudioModelInterface" );
	if ( cl_funcs.pStudioInterface )
	{
		if ( !cl_funcs.pStudioInterface( STUDIO_INTERFACE_VERSION, &pStudioAPI, &engine_studio_api ) )
		{
			Con_DPrintf( "Couldn't get client .dll studio model rendering interface.  Version mismatch?\n" );
			R_ResetStudio();
		}
	}
}

/*
================
COM_AddAppDirectoryToSearchPath

================
*/
void COM_AddAppDirectoryToSearchPath( const char *pszBaseDir, const char *appName )
{
	g_engdstAddrs.COM_AddAppDirectoryToSearchPath( &pszBaseDir, &appName );

	COM_AddAppDirectory( pszBaseDir, appName );
}

/*
================
ClientDLL_ExpandFileName

================
*/
int ClientDLL_ExpandFileName( const char *fileName, char *nameOutBuffer, int nameOutBufferSize )
{
	char	buf[512];

	g_engdstAddrs.COM_ExpandFilename( &fileName, &nameOutBuffer, &nameOutBufferSize );

	Q_strncpy( buf, fileName, sizeof( buf ) - 1 );
	buf[sizeof( buf ) - 1] = 0;

	if ( !COM_ExpandFilename( buf ) )
		return 0;

	COM_FixSlashes( buf );
	Q_strncpy( nameOutBuffer, buf, nameOutBufferSize );
	nameOutBuffer[nameOutBufferSize - 1] = 0;

	return 1;
}

/*
================
PlayerInfo_ValueForKey

================
*/
const char *PlayerInfo_ValueForKey( int playerNum, const char *key )
{
	g_engdstAddrs.PlayerInfo_ValueForKey( &playerNum, &key );

	if ( playerNum > cl.maxclients || playerNum < 1 || !cl.players[playerNum - 1].name || !cl.players[playerNum - 1].name[0] )
		return NULL;

	return Info_ValueForKey( cl.players[playerNum - 1].userinfo, key );
}

/*
================
LocalPlayerInfo_ValueForKey

================
*/
const char *LocalPlayerInfo_ValueForKey( const char *key )
{
	g_engdstAddrs.LocalPlayerInfo_ValueForKey( &key );

	return Info_ValueForKey( cls.userinfo, key );
}

/*
================
PlayerInfo_SetValueForKey

================
*/
void PlayerInfo_SetValueForKey( const char *key, const char *value )
{
	if ( !strcmp( Info_ValueForKey( cls.userinfo, key ), value ) )
		return;

	g_engdstAddrs.PlayerInfo_SetValueForKey( &key, &value );

	Info_SetValueForStarKey( cls.userinfo, key, value, MAX_INFO_STRING );

	if ( cls.state != ca_dedicated && cls.state >= ca_connected )
	{
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		SZ_Print( &cls.netchan.message, va( "setinfo \"%s\" \"%s\"\n", key, value ) );
	}
}

/*
================
GetPlayerUniqueID

================
*/
qboolean GetPlayerUniqueID( int iPlayer, char *playerID )
{
	g_engdstAddrs.GetPlayerUniqueID( &iPlayer, &playerID );

	iPlayer--;

	if ( iPlayer < 0 || iPlayer >= cl.maxclients )
		return false;

	if ( !cl.players[iPlayer].userinfo[0] || !cl.players[iPlayer].name[0] )
		return false;

	memcpy( playerID, cl.players[iPlayer].hashedcdkey, 16 );
	return true;
}

/*
================
GetTrackerIDForPlayer

================
*/
int GetTrackerIDForPlayer( int playerSlot )
{
	g_engdstAddrs.GetTrackerIDForPlayer( &playerSlot );

	return 0;
}

/*
================
GetPlayerForTrackerID

================
*/
int GetPlayerForTrackerID( int trackerID )
{
	g_engdstAddrs.GetPlayerForTrackerID( &trackerID );

	return 0;
}

/*
================
GetFirstCVarPtr

================
*/
struct cvar_s *GetFirstCVarPtr( void )
{
	return cvar_vars;
}

/*
================
GetFirstCmdFunctionHandle

================
*/
unsigned int GetFirstCmdFunctionHandle( void )
{
	return (unsigned int)Cmd_GetFirstCmd();
}

/*
================
GetNextCmdFunctionHandle

================
*/
unsigned int GetNextCmdFunctionHandle( unsigned int cmdhandle )
{
	cmd_function_t	*cmd;

	cmd = (cmd_function_t *)cmdhandle;
	return (unsigned int)cmd->next;
}

/*
================
GetCmdFunctionName

================
*/
const char *GetCmdFunctionName( unsigned int cmdhandle )
{
	cmd_function_t	*cmd;

	cmd = (cmd_function_t *)cmdhandle;
	return cmd->name;
}
