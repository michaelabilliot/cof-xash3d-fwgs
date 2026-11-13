// APIProxy.h -- the engine destination function table (Half-Life SDK engine/APIProxy.h)
//
// Engine calls that the client can observe go through g_engdstAddrs first:
// every entry receives pointers to the arguments of the call.

#ifndef APIPROXY_H
#define APIPROXY_H

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	struct cmdalias_s	*next;
	char	name[MAX_ALIAS_NAME];
	char	*value;
} cmdalias_t;

typedef int HSPRITE;
typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );


// Function type declarations for engine destination functions
typedef void	(*pfnEngDst_pfnSPR_Load_t )				( const char ** );
typedef void	(*pfnEngDst_pfnSPR_Frames_t )			( HSPRITE * );
typedef void	(*pfnEngDst_pfnSPR_Height_t )			( HSPRITE *, int * );
typedef void	(*pfnEngDst_pfnSPR_Width_t )			( HSPRITE *, int * );
typedef void	(*pfnEngDst_pfnSPR_Set_t )				( HSPRITE *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnSPR_Draw_t )				( int *, int *, int *, const struct rect_s ** );
typedef void	(*pfnEngDst_pfnSPR_DrawHoles_t )		( int *, int *, int *, const struct rect_s ** );
typedef void	(*pfnEngDst_pfnSPR_DrawAdditive_t )		( int *, int *, int *, const struct rect_s ** );
typedef void	(*pfnEngDst_pfnSPR_EnableScissor_t )	( int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnSPR_DisableScissor_t )	( void );
typedef void	(*pfnEngDst_pfnSPR_GetList_t )			( char **, int ** );
typedef void	(*pfnEngDst_pfnFillRGBA_t )				( int *, int *, int *, int *, int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnGetScreenInfo_t ) 		( struct SCREENINFO_s ** );
typedef void	(*pfnEngDst_pfnSetCrosshair_t )			( HSPRITE *, struct rect_s *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnRegisterVariable_t )		( char **, char **, int * );
typedef void	(*pfnEngDst_pfnGetCvarFloat_t )			( char ** );
typedef void	(*pfnEngDst_pfnGetCvarString_t )		( char ** );
typedef void	(*pfnEngDst_pfnAddCommand_t )			( char **, void (**pfnEngDst_function)(void) );
typedef void	(*pfnEngDst_pfnHookUserMsg_t )			( char **, pfnUserMsgHook * );
typedef void	(*pfnEngDst_pfnServerCmd_t )			( char ** );
typedef void	(*pfnEngDst_pfnClientCmd_t )			( char ** );
typedef void	(*pfnEngDst_pfnPrimeMusicStream_t )	( char **, int *);
typedef void	(*pfnEngDst_pfnGetPlayerInfo_t )		( int *, struct hud_player_info_s ** );
typedef void	(*pfnEngDst_pfnPlaySoundByName_t )		( char **, float * );
typedef void	(*pfnEngDst_pfnPlaySoundByNameAtPitch_t )	( char **, float *, int * );
typedef void	(*pfnEngDst_pfnPlaySoundVoiceByName_t )	(char **, float * );
typedef void	(*pfnEngDst_pfnPlaySoundByIndex_t )		( int *, float * );
typedef void	(*pfnEngDst_pfnAngleVectors_t )			( const float * *, float * *, float * *, float * * );
typedef void	(*pfnEngDst_pfnTextMessageGet_t )		( const char ** );
typedef void	(*pfnEngDst_pfnDrawCharacter_t )		( int *, int *, int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnDrawConsoleString_t )	( int *, int *, char ** );
typedef void	(*pfnEngDst_pfnDrawSetTextColor_t )		( float *, float *, float * );
typedef void	(*pfnEngDst_pfnDrawConsoleStringLen_t )	(  const char **, int **, int ** );
typedef void	(*pfnEngDst_pfnConsolePrint_t )			( const char ** );
typedef void	(*pfnEngDst_pfnCenterPrint_t )			( const char ** );
typedef void	(*pfnEngDst_GetWindowCenterX_t )		( void );
typedef void	(*pfnEngDst_GetWindowCenterY_t )		( void );
typedef void	(*pfnEngDst_GetViewAngles_t )			( float ** );
typedef void	(*pfnEngDst_SetViewAngles_t )			( float ** );
typedef void	(*pfnEngDst_GetMaxClients_t )			( void );
typedef void	(*pfnEngDst_Cvar_SetValue_t )			( char **, float * );
typedef void    (*pfnEngDst_Cmd_Argc_t)					(void);	
typedef void	(*pfnEngDst_Cmd_Argv_t )				( int * );
typedef void	(*pfnEngDst_Con_Printf_t )				( char **);
typedef void	(*pfnEngDst_Con_DPrintf_t )				( char **);
typedef void	(*pfnEngDst_Con_NPrintf_t )				( int *, char ** );
typedef void	(*pfnEngDst_Con_NXPrintf_t )			( struct con_nprint_s **, char **);
typedef void	(*pfnEngDst_PhysInfo_ValueForKey_t )	( const char ** );
typedef void	(*pfnEngDst_ServerInfo_ValueForKey_t )	( const char ** );
typedef void	(*pfnEngDst_GetClientMaxspeed_t )		( void );
typedef void	(*pfnEngDst_CheckParm_t )				( char **, char *** );
typedef void	(*pfnEngDst_Key_Event_t )				( int *, int * );
typedef void	(*pfnEngDst_GetMousePosition_t )		( int **, int ** );
typedef void	(*pfnEngDst_IsNoClipping_t )			( void );
typedef void	(*pfnEngDst_GetLocalPlayer_t )			( void );
typedef void	(*pfnEngDst_GetViewModel_t )			( void );
typedef void	(*pfnEngDst_GetEntityByIndex_t )		( int * );
typedef void	(*pfnEngDst_GetClientTime_t )			( void );
typedef void	(*pfnEngDst_V_CalcShake_t )				( void );
typedef void	(*pfnEngDst_V_ApplyShake_t )			( float **, float **, float * );
typedef void	(*pfnEngDst_PM_PointContents_t )		( float **, int ** );
typedef void	(*pfnEngDst_PM_WaterEntity_t )			( float ** );
typedef void	(*pfnEngDst_PM_TraceLine_t )			( float **, float **, int *, int *, int * );
typedef void	(*pfnEngDst_CL_LoadModel_t )			( const char **, int ** );
typedef void	(*pfnEngDst_CL_CreateVisibleEntity_t )	( int *, struct cl_entity_s ** );
typedef void	(*pfnEngDst_GetSpritePointer_t )		( HSPRITE * );
typedef void	(*pfnEngDst_pfnPlaySoundByNameAtLocation_t )	( char **, float *, float ** );
typedef void	(*pfnEngDst_pfnPrecacheEvent_t )		( int *, const char* * );
typedef void	(*pfnEngDst_pfnPlaybackEvent_t )		( int *, const struct edict_s **, unsigned short *, float *, float **, float **, float *, float *, int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnWeaponAnim_t )			( int *, int * );
typedef void	(*pfnEngDst_pfnRandomFloat_t )			( float *, float * );
typedef void	(*pfnEngDst_pfnRandomLong_t )			( int32 *, int32 * );
typedef void	(*pfnEngDst_pfnHookEvent_t )			( char **, void ( **pfnEvent )( struct event_args_s *args ) );
typedef void	(*pfnEngDst_Con_IsVisible_t)			();
typedef void	(*pfnEngDst_pfnGetGameDirectory_t )		( void );
typedef void	(*pfnEngDst_pfnGetCvarPointer_t )		( const char ** );
typedef void	(*pfnEngDst_Key_LookupBinding_t )		( const char ** );
typedef void	(*pfnEngDst_pfnGetLevelName_t )			( void );
typedef void	(*pfnEngDst_pfnGetScreenFade_t )		( struct screenfade_s ** );
typedef void	(*pfnEngDst_pfnSetScreenFade_t )		( struct screenfade_s ** );
typedef void	(*pfnEngDst_VGui_GetPanel_t )			( );
typedef void	(*pfnEngDst_VGui_ViewportPaintBackground_t ) (int **);
typedef void	(*pfnEngDst_COM_LoadFile_t )			( char **, int *, int ** );
typedef void	(*pfnEngDst_COM_ParseFile_t )			( char **, char ** );
typedef void	(*pfnEngDst_COM_FreeFile_t)				( void ** );
typedef void	(*pfnEngDst_IsSpectateOnly_t )			( void );
typedef void	(*pfnEngDst_LoadMapSprite_t )			( const char ** );
typedef void	(*pfnEngDst_COM_AddAppDirectoryToSearchPath_t ) ( const char **, const char ** );
typedef void	(*pfnEngDst_COM_ExpandFilename_t)		( const char **, char **, int * );
typedef void	(*pfnEngDst_PlayerInfo_ValueForKey_t )	( int *, const char ** );
typedef void	(*pfnEngDst_PlayerInfo_SetValueForKey_t )( const char **, const char ** );
typedef void	(*pfnEngDst_GetPlayerUniqueID_t)		(int *, char **);
typedef void	(*pfnEngDst_GetTrackerIDForPlayer_t)	(int *);
typedef void	(*pfnEngDst_GetPlayerForTrackerID_t)	(int *);
typedef void	(*pfnEngDst_pfnServerCmdUnreliable_t )	( char ** );
typedef void	(*pfnEngDst_GetMousePos_t )				(struct tagPOINT **);
typedef void	(*pfnEngDst_SetMousePos_t )				(int *, int *);
typedef void	(*pfnEngDst_SetMouseEnable_t )			(qboolean *);
typedef void	(*pfnEngDst_pfnSetFilterMode_t)			( int * );
typedef void	(*pfnEngDst_pfnSetFilterColor_t)		( float *, float *, float * );
typedef void	(*pfnEngDst_pfnSetFilterBrightness_t)	( float * );
typedef void	(*pfnEngDst_pfnSequenceGet_t )			( const char**, const char** );
typedef void	(*pfnEngDst_pfnSPR_DrawGeneric_t )		( int *, int *, int *, const struct rect_s **, int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnSequencePickSentence_t )	( const char**, int *, int ** );
typedef void	(*pfnEngDst_pfnDrawString_t )			( int *, int *, const char *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnDrawStringReverse_t )			( int *, int *, const char *, int *, int *, int * );
typedef void	(*pfnEngDst_LocalPlayerInfo_ValueForKey_t )( const char **);
typedef void	(*pfnEngDst_pfnVGUI2DrawCharacter_t )		( int *, int *, int *, unsigned int * );
typedef void	(*pfnEngDst_pfnVGUI2DrawCharacterAdd_t )	( int *, int *, int *, int *, int *, int *, unsigned int *);
typedef void	(*pfnEngDst_pfnProcessTutorMessageDecayBuffer_t )(int **, int *);
typedef void	(*pfnEngDst_pfnConstructTutorMessageDecayBuffer_t )(int **, int *);
typedef void	(*pfnEngDst_pfnResetTutorMessageDecayData_t)();
typedef void	(*pfnEngDst_pfnFillRGBABlend_t )				( int *, int *, int *, int *, int *, int *, int *, int * );
typedef void	(*pfnEngDst_pfnGetAppID_t )				( void );
typedef void	(*pfnEngDst_pfnGetAliases_t )				( void );
typedef void	(*pfnEngDst_pfnVguiWrap2_GetMouseDelta_t) ( int *x, int *y );
typedef void	(*pfnEngDst_pfnFilteredClientCmd_t )	( void );	// no argument in this build (cdll_exp.c hudFilteredClientCmd)


// Pointers to the engine destination functions
typedef struct
{
	pfnEngDst_pfnSPR_Load_t					pfnSPR_Load;
	pfnEngDst_pfnSPR_Frames_t				pfnSPR_Frames;
	pfnEngDst_pfnSPR_Height_t				pfnSPR_Height;
	pfnEngDst_pfnSPR_Width_t				pfnSPR_Width;
	pfnEngDst_pfnSPR_Set_t					pfnSPR_Set;
	pfnEngDst_pfnSPR_Draw_t					pfnSPR_Draw;
	pfnEngDst_pfnSPR_DrawHoles_t			pfnSPR_DrawHoles;
	pfnEngDst_pfnSPR_DrawAdditive_t			pfnSPR_DrawAdditive;
	pfnEngDst_pfnSPR_EnableScissor_t		pfnSPR_EnableScissor;
	pfnEngDst_pfnSPR_DisableScissor_t		pfnSPR_DisableScissor;
	pfnEngDst_pfnSPR_GetList_t				pfnSPR_GetList;
	pfnEngDst_pfnFillRGBA_t					pfnFillRGBA;
	pfnEngDst_pfnGetScreenInfo_t			pfnGetScreenInfo;
	pfnEngDst_pfnSetCrosshair_t				pfnSetCrosshair;
	pfnEngDst_pfnRegisterVariable_t			pfnRegisterVariable;
	pfnEngDst_pfnGetCvarFloat_t				pfnGetCvarFloat;
	pfnEngDst_pfnGetCvarString_t			pfnGetCvarString;
	pfnEngDst_pfnAddCommand_t				pfnAddCommand;
	pfnEngDst_pfnHookUserMsg_t				pfnHookUserMsg;
	pfnEngDst_pfnServerCmd_t				pfnServerCmd;
	pfnEngDst_pfnClientCmd_t				pfnClientCmd;
	pfnEngDst_pfnGetPlayerInfo_t			pfnGetPlayerInfo;
	pfnEngDst_pfnPlaySoundByName_t			pfnPlaySoundByName;
	pfnEngDst_pfnPlaySoundByIndex_t			pfnPlaySoundByIndex;
	pfnEngDst_pfnAngleVectors_t				pfnAngleVectors;
	pfnEngDst_pfnTextMessageGet_t			pfnTextMessageGet;
	pfnEngDst_pfnDrawCharacter_t			pfnDrawCharacter;
	pfnEngDst_pfnDrawConsoleString_t		pfnDrawConsoleString;
	pfnEngDst_pfnDrawSetTextColor_t			pfnDrawSetTextColor;
	pfnEngDst_pfnDrawConsoleStringLen_t		pfnDrawConsoleStringLen;
	pfnEngDst_pfnConsolePrint_t				pfnConsolePrint;
	pfnEngDst_pfnCenterPrint_t				pfnCenterPrint;
	pfnEngDst_GetWindowCenterX_t			GetWindowCenterX;
	pfnEngDst_GetWindowCenterY_t			GetWindowCenterY;
	pfnEngDst_GetViewAngles_t				GetViewAngles;
	pfnEngDst_SetViewAngles_t				SetViewAngles;
	pfnEngDst_GetMaxClients_t				GetMaxClients;
	pfnEngDst_Cvar_SetValue_t				Cvar_SetValue;
	pfnEngDst_Cmd_Argc_t					Cmd_Argc;
	pfnEngDst_Cmd_Argv_t					Cmd_Argv;
	pfnEngDst_Con_Printf_t					Con_Printf;
	pfnEngDst_Con_DPrintf_t					Con_DPrintf;
	pfnEngDst_Con_NPrintf_t					Con_NPrintf;
	pfnEngDst_Con_NXPrintf_t				Con_NXPrintf;
	pfnEngDst_PhysInfo_ValueForKey_t		PhysInfo_ValueForKey;
	pfnEngDst_ServerInfo_ValueForKey_t		ServerInfo_ValueForKey;
	pfnEngDst_GetClientMaxspeed_t			GetClientMaxspeed;
	pfnEngDst_CheckParm_t					CheckParm;
	pfnEngDst_Key_Event_t					Key_Event;
	pfnEngDst_GetMousePosition_t			GetMousePosition;
	pfnEngDst_IsNoClipping_t				IsNoClipping;
	pfnEngDst_GetLocalPlayer_t				GetLocalPlayer;
	pfnEngDst_GetViewModel_t				GetViewModel;
	pfnEngDst_GetEntityByIndex_t			GetEntityByIndex;
	pfnEngDst_GetClientTime_t				GetClientTime;
	pfnEngDst_V_CalcShake_t					V_CalcShake;
	pfnEngDst_V_ApplyShake_t				V_ApplyShake;
	pfnEngDst_PM_PointContents_t			PM_PointContents;
	pfnEngDst_PM_WaterEntity_t				PM_WaterEntity;
	pfnEngDst_PM_TraceLine_t				PM_TraceLine;
	pfnEngDst_CL_LoadModel_t				CL_LoadModel;
	pfnEngDst_CL_CreateVisibleEntity_t		CL_CreateVisibleEntity;
	pfnEngDst_GetSpritePointer_t			GetSpritePointer;
	pfnEngDst_pfnPlaySoundByNameAtLocation_t	pfnPlaySoundByNameAtLocation;
	pfnEngDst_pfnPrecacheEvent_t			pfnPrecacheEvent;
	pfnEngDst_pfnPlaybackEvent_t			pfnPlaybackEvent;
	pfnEngDst_pfnWeaponAnim_t				pfnWeaponAnim;
	pfnEngDst_pfnRandomFloat_t				pfnRandomFloat;
	pfnEngDst_pfnRandomLong_t				pfnRandomLong;
	pfnEngDst_pfnHookEvent_t				pfnHookEvent;
	pfnEngDst_Con_IsVisible_t				Con_IsVisible;
	pfnEngDst_pfnGetGameDirectory_t			pfnGetGameDirectory;
	pfnEngDst_pfnGetCvarPointer_t			pfnGetCvarPointer;
	pfnEngDst_Key_LookupBinding_t			Key_LookupBinding;
	pfnEngDst_pfnGetLevelName_t				pfnGetLevelName;
	pfnEngDst_pfnGetScreenFade_t			pfnGetScreenFade;
	pfnEngDst_pfnSetScreenFade_t			pfnSetScreenFade;
	pfnEngDst_VGui_GetPanel_t				VGui_GetPanel;
	pfnEngDst_VGui_ViewportPaintBackground_t	VGui_ViewportPaintBackground;
	pfnEngDst_COM_LoadFile_t				COM_LoadFile;
	pfnEngDst_COM_ParseFile_t				COM_ParseFile;
	pfnEngDst_COM_FreeFile_t				COM_FreeFile;
	struct triangleapi_s		*pTriAPI;
	struct efx_api_s			*pEfxAPI;
	struct event_api_s			*pEventAPI;
	struct demo_api_s			*pDemoAPI;
	struct net_api_s			*pNetAPI;
	struct IVoiceTweak_s		*pVoiceTweak;
	pfnEngDst_IsSpectateOnly_t				IsSpectateOnly;
	pfnEngDst_LoadMapSprite_t				LoadMapSprite;
	pfnEngDst_COM_AddAppDirectoryToSearchPath_t		COM_AddAppDirectoryToSearchPath;
	pfnEngDst_COM_ExpandFilename_t			COM_ExpandFilename;
	pfnEngDst_PlayerInfo_ValueForKey_t		PlayerInfo_ValueForKey;
	pfnEngDst_PlayerInfo_SetValueForKey_t	PlayerInfo_SetValueForKey;
	pfnEngDst_GetPlayerUniqueID_t			GetPlayerUniqueID;
	pfnEngDst_GetTrackerIDForPlayer_t		GetTrackerIDForPlayer;
	pfnEngDst_GetPlayerForTrackerID_t		GetPlayerForTrackerID;
	pfnEngDst_pfnServerCmdUnreliable_t		pfnServerCmdUnreliable;
	pfnEngDst_GetMousePos_t					pfnGetMousePos;
	pfnEngDst_SetMousePos_t					pfnSetMousePos;
	pfnEngDst_SetMouseEnable_t				pfnSetMouseEnable;
	pfnEngDst_pfnSetFilterMode_t			pfnSetFilterMode ;
	pfnEngDst_pfnSetFilterColor_t			pfnSetFilterColor ;
	pfnEngDst_pfnSetFilterBrightness_t		pfnSetFilterBrightness ;
	pfnEngDst_pfnSequenceGet_t				pfnSequenceGet;
	pfnEngDst_pfnSPR_DrawGeneric_t			pfnSPR_DrawGeneric;
	pfnEngDst_pfnSequencePickSentence_t		pfnSequencePickSentence;
	pfnEngDst_pfnDrawString_t				pfnDrawString;
	pfnEngDst_pfnDrawString_t				pfnDrawStringReverse;
	pfnEngDst_LocalPlayerInfo_ValueForKey_t	LocalPlayerInfo_ValueForKey;
	pfnEngDst_pfnVGUI2DrawCharacter_t		pfnVGUI2DrawCharacter;
	pfnEngDst_pfnVGUI2DrawCharacterAdd_t	pfnVGUI2DrawCharacterAdd;
	pfnEngDst_pfnPlaySoundVoiceByName_t	pfnPlaySoundVoiceByName;
	pfnEngDst_pfnPrimeMusicStream_t			pfnPrimeMusicStream;
	pfnEngDst_pfnProcessTutorMessageDecayBuffer_t		pfnProcessTutorMessageDecayBuffer;
	pfnEngDst_pfnConstructTutorMessageDecayBuffer_t		pfnConstructTutorMessageDecayBuffer;
	pfnEngDst_pfnResetTutorMessageDecayData_t		pfnResetTutorMessageDecayData;
	pfnEngDst_pfnPlaySoundByNameAtPitch_t	pfnPlaySoundByNameAtPitch;
	pfnEngDst_pfnFillRGBABlend_t					pfnFillRGBABlend;
	pfnEngDst_pfnGetAppID_t							pfnGetAppID;
	pfnEngDst_pfnGetAliases_t				pfnGetAliasList;
	pfnEngDst_pfnVguiWrap2_GetMouseDelta_t	pfnVguiWrap2_GetMouseDelta;
	pfnEngDst_pfnFilteredClientCmd_t		pfnFilteredClientCmd;
} cl_enginefunc_dst_t;

extern cl_enginefunc_dst_t	g_engdstAddrs;

// Function type declarations for client destination functions
typedef void (*DST_INITIALIZE_FUNC)	( struct cl_enginefuncs_s**, int *);
typedef void (*DST_HUD_INIT_FUNC)		( void );
typedef void (*DST_HUD_VIDINIT_FUNC)	( void );
typedef void (*DST_HUD_REDRAW_FUNC)	( float*, int* );
typedef void (*DST_HUD_UPDATECLIENTDATA_FUNC) ( struct client_data_s**, float* );
typedef void (*DST_HUD_RESET_FUNC)    ( void );
typedef void (*DST_HUD_CLIENTMOVE_FUNC)( struct playermove_s **, qboolean * );
typedef void (*DST_HUD_CLIENTMOVEINIT_FUNC)( struct playermove_s ** );
typedef void (*DST_HUD_TEXTURETYPE_FUNC)( char ** );
typedef void (*DST_HUD_IN_ACTIVATEMOUSE_FUNC) ( void );
typedef void (*DST_HUD_IN_DEACTIVATEMOUSE_FUNC)		( void );
typedef void (*DST_HUD_IN_MOUSEEVENT_FUNC)		( int * );
typedef void (*DST_HUD_IN_CLEARSTATES_FUNC)		( void );
typedef void (*DST_HUD_IN_ACCUMULATE_FUNC ) ( void );
typedef void (*DST_HUD_CL_CREATEMOVE_FUNC)		( float *, struct usercmd_s **, int * );
typedef void (*DST_HUD_CL_ISTHIRDPERSON_FUNC) ( void );
typedef void (*DST_HUD_CL_GETCAMERAOFFSETS_FUNC )( float ** );
typedef void (*DST_HUD_KB_FIND_FUNC) ( const char ** );
typedef void (*DST_HUD_CAMTHINK_FUNC )( void );
typedef void (*DST_HUD_CALCREF_FUNC ) ( struct ref_params_s ** );
typedef void (*DST_HUD_ADDENTITY_FUNC ) ( int *, struct cl_entity_s **, const char ** );
typedef void (*DST_HUD_CREATEENTITIES_FUNC ) ( void );
typedef void (*DST_HUD_DRAWNORMALTRIS_FUNC ) ( void );
typedef void (*DST_HUD_DRAWTRANSTRIS_FUNC ) ( void );
typedef void (*DST_HUD_STUDIOEVENT_FUNC ) ( const struct mstudioevent_s **, const struct cl_entity_s ** );
typedef void (*DST_HUD_POSTRUNCMD_FUNC ) ( struct local_state_s **, struct local_state_s **, struct usercmd_s **, int *, double *, unsigned int * );
typedef void (*DST_HUD_SHUTDOWN_FUNC ) ( void );
typedef void (*DST_HUD_TXFERLOCALOVERRIDES_FUNC )( struct entity_state_s **, const struct clientdata_s ** );
typedef void (*DST_HUD_PROCESSPLAYERSTATE_FUNC )( struct entity_state_s **, const struct entity_state_s ** );
typedef void (*DST_HUD_TXFERPREDICTIONDATA_FUNC ) ( struct entity_state_s **, const struct entity_state_s **, struct clientdata_s **, const struct clientdata_s **, struct weapon_data_s **, const struct weapon_data_s ** );
typedef void (*DST_HUD_DEMOREAD_FUNC ) ( int *, unsigned char ** );
typedef void (*DST_HUD_CONNECTIONLESS_FUNC )( const struct netadr_s **, const char **, char **, int ** );
typedef void (*DST_HUD_GETHULLBOUNDS_FUNC ) ( int *, float **, float ** );
typedef void (*DST_HUD_FRAME_FUNC)		( double * );
typedef void (*DST_HUD_KEY_EVENT_FUNC ) ( int *, int *, const char ** );
typedef void (*DST_HUD_TEMPENTUPDATE_FUNC) ( double *, double *, double *, struct tempent_s ***, struct tempent_s ***, int ( **Callback_AddVisibleEntity )( struct cl_entity_s *pEntity ),	void ( **Callback_TempEntPlaySound )( struct tempent_s *pTemp, float damp ) );
typedef void (*DST_HUD_GETUSERENTITY_FUNC ) ( int * );
typedef void (*DST_HUD_VOICESTATUS_FUNC)(int *, qboolean *);
typedef void (*DST_HUD_DIRECTORMESSAGE_FUNC)( int *, void ** );
typedef void (*DST_HUD_STUDIO_INTERFACE_FUNC ) ( int *, struct r_studio_interface_s ***, struct engine_studio_api_s ** );
typedef void (*DST_HUD_CHATINPUTPOSITION_FUNC)( int **, int ** );
typedef void (*DST_HUD_GETPLAYERTEAM)(int);

// Pointers to the client destination functions
typedef struct cldll_func_dst_s
{
	DST_INITIALIZE_FUNC						pInitFunc;
	DST_HUD_INIT_FUNC						pHudInitFunc;
	DST_HUD_VIDINIT_FUNC					pHudVidInitFunc;
	DST_HUD_REDRAW_FUNC						pHudRedrawFunc;
	DST_HUD_UPDATECLIENTDATA_FUNC			pHudUpdateClientDataFunc;
	DST_HUD_RESET_FUNC						pHudResetFunc;
	DST_HUD_CLIENTMOVE_FUNC					pClientMove;
	DST_HUD_CLIENTMOVEINIT_FUNC				pClientMoveInit;
	DST_HUD_TEXTURETYPE_FUNC				pClientTextureType;
	DST_HUD_IN_ACTIVATEMOUSE_FUNC			pIN_ActivateMouse;
	DST_HUD_IN_DEACTIVATEMOUSE_FUNC			pIN_DeactivateMouse;
	DST_HUD_IN_MOUSEEVENT_FUNC				pIN_MouseEvent;
	DST_HUD_IN_CLEARSTATES_FUNC				pIN_ClearStates;
	DST_HUD_IN_ACCUMULATE_FUNC				pIN_Accumulate;
	DST_HUD_CL_CREATEMOVE_FUNC				pCL_CreateMove;
	DST_HUD_CL_ISTHIRDPERSON_FUNC			pCL_IsThirdPerson;
	DST_HUD_CL_GETCAMERAOFFSETS_FUNC		pCL_GetCameraOffsets;
	DST_HUD_KB_FIND_FUNC					pFindKey;
	DST_HUD_CAMTHINK_FUNC					pCamThink;
	DST_HUD_CALCREF_FUNC					pCalcRefdef;
	DST_HUD_ADDENTITY_FUNC					pAddEntity;
	DST_HUD_CREATEENTITIES_FUNC				pCreateEntities;
	DST_HUD_DRAWNORMALTRIS_FUNC				pDrawNormalTriangles;
	DST_HUD_DRAWTRANSTRIS_FUNC				pDrawTransparentTriangles;
	DST_HUD_STUDIOEVENT_FUNC				pStudioEvent;
	DST_HUD_POSTRUNCMD_FUNC					pPostRunCmd;
	DST_HUD_SHUTDOWN_FUNC					pShutdown;
	DST_HUD_TXFERLOCALOVERRIDES_FUNC		pTxferLocalOverrides;
	DST_HUD_PROCESSPLAYERSTATE_FUNC			pProcessPlayerState;
	DST_HUD_TXFERPREDICTIONDATA_FUNC		pTxferPredictionData;
	DST_HUD_DEMOREAD_FUNC					pReadDemoBuffer;
	DST_HUD_CONNECTIONLESS_FUNC				pConnectionlessPacket;
	DST_HUD_GETHULLBOUNDS_FUNC				pGetHullBounds;
	DST_HUD_FRAME_FUNC						pHudFrame;
	DST_HUD_KEY_EVENT_FUNC					pKeyEvent;
	DST_HUD_TEMPENTUPDATE_FUNC				pTempEntUpdate;
	DST_HUD_GETUSERENTITY_FUNC				pGetUserEntity;
	DST_HUD_VOICESTATUS_FUNC				pVoiceStatus;	// Possibly null on old client dlls.
	DST_HUD_DIRECTORMESSAGE_FUNC			pDirectorMessage;	// Possibly null on old client dlls.
	DST_HUD_STUDIO_INTERFACE_FUNC			pStudioInterface;  // Not used by all clients
	DST_HUD_CHATINPUTPOSITION_FUNC			pChatInputPosition;  // Not used by all clients
	DST_HUD_GETPLAYERTEAM					pGetPlayerTeam; // Not used by all clients
} cldll_func_dst_t;

// Module exports
typedef void (*PFN_LOADMOD)( char *pchModule );
typedef void (*PFN_CLOSEMOD)( void );
typedef int (*PFN_NCALL)( int ijump, int cnArg, ... );

typedef void (*PFN_GETCLDSTADDRS)( struct cldll_func_dst_s *pcldstAddrs );
typedef void (*PFN_GETENGDSTADDRS)( cl_enginefunc_dst_t *pengdstAddrs );
typedef void (*PFN_MODULELOADED)( void );

typedef void (*PFN_PROCESSOUTGOINGNET)( struct netchan_s *pchan, struct sizebuf_s *psizebuf );
typedef qboolean (*PFN_PROCESSINCOMINGNET)( struct netchan_s *pchan, struct sizebuf_s *psizebuf );

typedef void (*PFN_TEXTURELOAD)( char *pszName, int dxWidth, int dyHeight, char *pbData );
typedef void (*PFN_MODELLOAD)( struct model_s *pmodel, void *pvBuf );

typedef void (*PFN_FRAMEBEGIN)( void );
typedef void (*PFN_FRAMERENDER1)( void );
typedef void (*PFN_FRAMERENDER2)( void );

typedef void (*PFN_SETMODSHELPERS)( struct modshelpers_s *pmodshelpers );
typedef void (*PFN_SETMODCHELPERS)( struct modchelpers_s *pmodchelpers );
typedef void (*PFN_SETENGDATA)( struct engdata_s *pengdata );

typedef void (*PFN_CONNECTCLIENT)( int iPlayer );
typedef void (*PFN_RECORDIP)( unsigned int pnIP );
typedef void (*PFN_PLAYERSTATUS)( unsigned char *pbData, int cbData );

typedef void (*PFN_SETENGINEVERSION)( int nVersion );

typedef struct modfuncs_s
{
	// Functions for the pcode interpreter
	PFN_LOADMOD m_pfnLoadMod;
	PFN_CLOSEMOD m_pfnCloseMod;
	PFN_NCALL m_pfnNCall;

	// API destination functions
	PFN_GETCLDSTADDRS m_pfnGetClDstAddrs;
	PFN_GETENGDSTADDRS m_pfnGetEngDstAddrs;

	// Miscellaneous functions
	PFN_MODULELOADED m_pfnModuleLoaded;		// Called right after the module is loaded

	// Functions for processing network traffic
	PFN_PROCESSOUTGOINGNET m_pfnProcessOutgoingNet;		// Every outgoing packet gets run through this
	PFN_PROCESSINCOMINGNET m_pfnProcessIncomingNet;		// Every incoming packet gets run through this

	// Resource functions
	PFN_TEXTURELOAD m_pfnTextureLoad;		// Called as each texture is loaded
	PFN_MODELLOAD m_pfnModelLoad;			// Called as each model is loaded

	// Functions called every frame
	PFN_FRAMEBEGIN m_pfnFrameBegin;			// Called at the beginning of each frame cycle
	PFN_FRAMERENDER1 m_pfnFrameRender1;		// Called at the beginning of the render loop
	PFN_FRAMERENDER2 m_pfnFrameRender2;		// Called at the end of the render loop

	// Module helper transfer
	PFN_SETMODSHELPERS m_pfnSetModSHelpers;
	PFN_SETMODCHELPERS m_pfnSetModCHelpers;
	PFN_SETENGDATA m_pfnSetEngData;

	// Which version are we?
	int m_nVersion;

	// Miscellaneous game stuff
	PFN_CONNECTCLIENT m_pfnConnectClient;	// Called whenever a new client connects
	PFN_RECORDIP m_pfnRecordIP;				// Secure master has reported a new IP for us
	PFN_PLAYERSTATUS m_pfnPlayerStatus;		// Called whenever we receive a PlayerStatus packet

	// Recent additions
	PFN_SETENGINEVERSION m_pfnSetEngineVersion;	// 1 = patched engine

	// reserved for future expansion
	int m_nVoid2;
	int m_nVoid3;
	int m_nVoid4;
	int m_nVoid5;
	int m_nVoid6;
	int m_nVoid7;
	int m_nVoid8;
	int m_nVoid9;
} modfuncs_t;

// Functions for ModuleS
typedef void (*PFN_KICKPLAYER)(int nPlayerSlot, int nReason);

typedef struct modshelpers_s
{
	PFN_KICKPLAYER m_pfnKickPlayer;

	// reserved for future expansion
	int m_nVoid1;
	int m_nVoid2;
	int m_nVoid3;
	int m_nVoid4;
	int m_nVoid5;
	int m_nVoid6;
	int m_nVoid7;
	int m_nVoid8;
	int m_nVoid9;
} modshelpers_t;

// Functions for moduleC
typedef struct modchelpers_s
{
	// reserved for future expansion
	int m_nVoid0;
	int m_nVoid1;
	int m_nVoid2;
	int m_nVoid3;
	int m_nVoid4;
	int m_nVoid5;
	int m_nVoid6;
	int m_nVoid7;
	int m_nVoid8;
	int m_nVoid9;
} modchelpers_t;

#endif // APIPROXY_H
