// module.cpp -- loads the security module (a blob or a plain DLL) and hands it the engine's tables

#include <windows.h>
#include <stdlib.h>

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
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
#include "console.h"
#include "filesystem_.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "LoadBlob.h"

void	NullDst( void );
void	CommonLoadSecurityModule( void *nAddrModule );
void	LoadSecurityModuleFromDisk( char *pszDllName );
void	LoadSecurityModuleFromMemory( unsigned char *pCode, int nSize );

extern	struct cl_enginefuncs_s		cl_enginefuncs;
extern	struct cldll_func_s			cl_funcs;
extern	cldll_func_dst_t			g_cldstAddrs;
extern	struct cmd_function_s		*cmd_functions;
extern	struct glwstate_t			glw_state;

extern	modshelpers_t				g_modshelpers;
extern	BlobFootprint_t				g_blobfootprintModule;
extern	void						*g_pvModuleSpecial;
}

extern HINSTANCE g_hinstOpenGlEarly;

// ********************************************************
// Information about the engine
// ********************************************************
typedef struct engdata_s
{
	struct cl_enginefuncs_s *pcl_enginefuncs;		// functions exported by the engine
	cl_enginefunc_dst_t *pg_engdstAddrs;	// destination handlers for engine exports
	struct cldll_func_s *pcl_funcs;				// client exports
	cldll_func_dst_t *pg_cldstAddrs;		// client export destination handlers
	struct modfuncs_s *pg_modfuncs;			// engine's pointer to module functions
	struct cmd_function_s **pcmd_functions;	// list of all registered commands
	void *pkeybindings;						// all key bindings (not really a void *, but easier this way)
	void (*pfnConPrintf)(char *, ...);		// dump to console
	struct cvar_s **pcvar_vars;				// pointer to head of cvar list
	struct glwstate_t *pglwstate;			// OpenGl information
	void *(*pfnSZ_GetSpace)(struct sizebuf_s *, int); // pointer to SZ_GetSpace
	struct modfuncs_s *pmodfuncs;			// &g_modfuncs
	void *pfnGetProcAddress;				// &GetProcAddress
	void *pfnGetModuleHandle;				// &GetModuleHandle
	struct server_static_s *psvs;			// &svs
	struct client_static_s *pcls;			// &cls
	void (*pfnSV_DropClient)(struct client_s *, qboolean, char *, ...);	// pointer to SV_DropClient
	void (*pfnNetchan_Transmit)(struct netchan_s *, int, byte *);		// pointer to Netchan_Transmit
	void (*pfnNET_SendPacket)(netsrc_t sock, int length, void *data, netadr_t to); // &NET_SendPacket
	struct cvar_s *(*pfnCvarFindVar)(const char *pchName);				// pointer to Cvar_FindVar
	int *phinstOpenGlEarly;					// &g_hinstOpenGlEarly

	// Reserved for future expansion
	void *pVoid0;							// reserved for future expan
	void *pVoid1;							// reserved for future expan
	void *pVoid2;							// reserved for future expan
	void *pVoid3;							// reserved for future expan
	void *pVoid4;							// reserved for future expan
	void *pVoid5;							// reserved for future expan
	void *pVoid6;							// reserved for future expan
	void *pVoid7;							// reserved for future expan
	void *pVoid8;							// reserved for future expan
	void *pVoid9;							// reserved for future expan
} engdata_t;

#define MODULE_VERSION_guess	0x43210004

void NullDst( void )
{
}

// Helper functions the module can call
modshelpers_t g_modshelpers =
{
	SV_KickPlayer,
	0, 0, 0, 0, 0, 0, 0, 0, 0
};

modfuncs_t g_modfuncs;
module_t g_module;

// Engine export destinations, filled in by the module
cl_enginefunc_dst_t g_engdstAddrs =
{
	(pfnEngDst_pfnSPR_Load_t)NullDst,
	(pfnEngDst_pfnSPR_Frames_t)NullDst,
	(pfnEngDst_pfnSPR_Height_t)NullDst,
	(pfnEngDst_pfnSPR_Width_t)NullDst,
	(pfnEngDst_pfnSPR_Set_t)NullDst,
	(pfnEngDst_pfnSPR_Draw_t)NullDst,
	(pfnEngDst_pfnSPR_DrawHoles_t)NullDst,
	(pfnEngDst_pfnSPR_DrawAdditive_t)NullDst,
	(pfnEngDst_pfnSPR_EnableScissor_t)NullDst,
	(pfnEngDst_pfnSPR_DisableScissor_t)NullDst,
	(pfnEngDst_pfnSPR_GetList_t)NullDst,
	(pfnEngDst_pfnFillRGBA_t)NullDst,
	(pfnEngDst_pfnGetScreenInfo_t)NullDst,
	(pfnEngDst_pfnSetCrosshair_t)NullDst,
	(pfnEngDst_pfnRegisterVariable_t)NullDst,
	(pfnEngDst_pfnGetCvarFloat_t)NullDst,
	(pfnEngDst_pfnGetCvarString_t)NullDst,
	(pfnEngDst_pfnAddCommand_t)NullDst,
	(pfnEngDst_pfnHookUserMsg_t)NullDst,
	(pfnEngDst_pfnServerCmd_t)NullDst,
	(pfnEngDst_pfnClientCmd_t)NullDst,
	(pfnEngDst_pfnGetPlayerInfo_t)NullDst,
	(pfnEngDst_pfnPlaySoundByName_t)NullDst,
	(pfnEngDst_pfnPlaySoundByIndex_t)NullDst,
	(pfnEngDst_pfnAngleVectors_t)NullDst,
	(pfnEngDst_pfnTextMessageGet_t)NullDst,
	(pfnEngDst_pfnDrawCharacter_t)NullDst,
	(pfnEngDst_pfnDrawConsoleString_t)NullDst,
	(pfnEngDst_pfnDrawSetTextColor_t)NullDst,
	(pfnEngDst_pfnDrawConsoleStringLen_t)NullDst,
	(pfnEngDst_pfnConsolePrint_t)NullDst,
	(pfnEngDst_pfnCenterPrint_t)NullDst,
	(pfnEngDst_GetWindowCenterX_t)NullDst,
	(pfnEngDst_GetWindowCenterY_t)NullDst,
	(pfnEngDst_GetViewAngles_t)NullDst,
	(pfnEngDst_SetViewAngles_t)NullDst,
	(pfnEngDst_GetMaxClients_t)NullDst,
	(pfnEngDst_Cvar_SetValue_t)NullDst,
	(pfnEngDst_Cmd_Argc_t)NullDst,
	(pfnEngDst_Cmd_Argv_t)NullDst,
	(pfnEngDst_Con_Printf_t)NullDst,
	(pfnEngDst_Con_DPrintf_t)NullDst,
	(pfnEngDst_Con_NPrintf_t)NullDst,
	(pfnEngDst_Con_NXPrintf_t)NullDst,
	(pfnEngDst_PhysInfo_ValueForKey_t)NullDst,
	(pfnEngDst_ServerInfo_ValueForKey_t)NullDst,
	(pfnEngDst_GetClientMaxspeed_t)NullDst,
	(pfnEngDst_CheckParm_t)NullDst,
	(pfnEngDst_Key_Event_t)NullDst,
	(pfnEngDst_GetMousePosition_t)NullDst,
	(pfnEngDst_IsNoClipping_t)NullDst,
	(pfnEngDst_GetLocalPlayer_t)NullDst,
	(pfnEngDst_GetViewModel_t)NullDst,
	(pfnEngDst_GetEntityByIndex_t)NullDst,
	(pfnEngDst_GetClientTime_t)NullDst,
	(pfnEngDst_V_CalcShake_t)NullDst,
	(pfnEngDst_V_ApplyShake_t)NullDst,
	(pfnEngDst_PM_PointContents_t)NullDst,
	(pfnEngDst_PM_WaterEntity_t)NullDst,
	(pfnEngDst_PM_TraceLine_t)NullDst,
	(pfnEngDst_CL_LoadModel_t)NullDst,
	(pfnEngDst_CL_CreateVisibleEntity_t)NullDst,
	(pfnEngDst_GetSpritePointer_t)NullDst,
	(pfnEngDst_pfnPlaySoundByNameAtLocation_t)NullDst,
	(pfnEngDst_pfnPrecacheEvent_t)NullDst,
	(pfnEngDst_pfnPlaybackEvent_t)NullDst,
	(pfnEngDst_pfnWeaponAnim_t)NullDst,
	(pfnEngDst_pfnRandomFloat_t)NullDst,
	(pfnEngDst_pfnRandomLong_t)NullDst,
	(pfnEngDst_pfnHookEvent_t)NullDst,
	(pfnEngDst_Con_IsVisible_t)NullDst,
	(pfnEngDst_pfnGetGameDirectory_t)NullDst,
	(pfnEngDst_pfnGetCvarPointer_t)NullDst,
	(pfnEngDst_Key_LookupBinding_t)NullDst,
	(pfnEngDst_pfnGetLevelName_t)NullDst,
	(pfnEngDst_pfnGetScreenFade_t)NullDst,
	(pfnEngDst_pfnSetScreenFade_t)NullDst,
	(pfnEngDst_VGui_GetPanel_t)NullDst,
	(pfnEngDst_VGui_ViewportPaintBackground_t)NullDst,
	(pfnEngDst_COM_LoadFile_t)NullDst,
	(pfnEngDst_COM_ParseFile_t)NullDst,
	(pfnEngDst_COM_FreeFile_t)NullDst,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	(pfnEngDst_IsSpectateOnly_t)NullDst,
	(pfnEngDst_LoadMapSprite_t)NullDst,
	(pfnEngDst_COM_AddAppDirectoryToSearchPath_t)NullDst,
	(pfnEngDst_COM_ExpandFilename_t)NullDst,
	(pfnEngDst_PlayerInfo_ValueForKey_t)NullDst,
	(pfnEngDst_PlayerInfo_SetValueForKey_t)NullDst,
	(pfnEngDst_GetPlayerUniqueID_t)NullDst,
	(pfnEngDst_GetTrackerIDForPlayer_t)NullDst,
	(pfnEngDst_GetPlayerForTrackerID_t)NullDst,
	(pfnEngDst_pfnServerCmdUnreliable_t)NullDst,
	(pfnEngDst_GetMousePos_t)NullDst,
	(pfnEngDst_SetMousePos_t)NullDst,
	(pfnEngDst_SetMouseEnable_t)NullDst,
	(pfnEngDst_pfnSetFilterMode_t)NullDst,
	(pfnEngDst_pfnSetFilterColor_t)NullDst,
	(pfnEngDst_pfnSetFilterBrightness_t)NullDst,
	(pfnEngDst_pfnSequenceGet_t)NullDst,
	(pfnEngDst_pfnSPR_DrawGeneric_t)NullDst,
	(pfnEngDst_pfnSequencePickSentence_t)NullDst,
	(pfnEngDst_pfnDrawString_t)NullDst,
	(pfnEngDst_pfnDrawString_t)NullDst,
	(pfnEngDst_LocalPlayerInfo_ValueForKey_t)NullDst,
	(pfnEngDst_pfnVGUI2DrawCharacter_t)NullDst,
	(pfnEngDst_pfnVGUI2DrawCharacterAdd_t)NullDst,
	(pfnEngDst_pfnPlaySoundVoiceByName_t)NullDst,
	(pfnEngDst_pfnPrimeMusicStream_t)NullDst,
	(pfnEngDst_pfnProcessTutorMessageDecayBuffer_t)NullDst,
	(pfnEngDst_pfnConstructTutorMessageDecayBuffer_t)NullDst,
	(pfnEngDst_pfnResetTutorMessageDecayData_t)NullDst,
	(pfnEngDst_pfnPlaySoundByNameAtPitch_t)NullDst,
	(pfnEngDst_pfnFillRGBABlend_t)NullDst,
	(pfnEngDst_pfnGetAppID_t)NullDst,
	(pfnEngDst_pfnGetAliases_t)NullDst,
	(pfnEngDst_pfnVguiWrap2_GetMouseDelta_t)NullDst,
	(pfnEngDst_pfnFilteredClientCmd_t)NullDst
};

BlobFootprint_t g_blobfootprintModule;
void *g_pvModuleSpecial;

void CommonLoadSecurityModule( void *nAddrModule )
{
	if ( g_modfuncs.m_nVersion != MODULE_VERSION_guess )
	{
		CloseSecurityModule();
		return;
	}

	if ( g_modfuncs.m_pfnSetEngineVersion )
		g_modfuncs.m_pfnSetEngineVersion( 3 );

	g_modfuncs.m_pfnLoadMod( (char *)nAddrModule );

	g_modfuncs.m_pfnGetClDstAddrs( &g_cldstAddrs );
	g_modfuncs.m_pfnGetEngDstAddrs( &g_engdstAddrs );

	if ( g_modfuncs.m_pfnSetModSHelpers )
		g_modfuncs.m_pfnSetModSHelpers( &g_modshelpers );

	engdata_t engdata;
	engdata.pcl_enginefuncs = &cl_enginefuncs;
	engdata.pg_engdstAddrs = &g_engdstAddrs;
	engdata.pcl_funcs = &cl_funcs;
	engdata.pg_cldstAddrs = &g_cldstAddrs;
	engdata.pg_modfuncs = &g_modfuncs;
	engdata.pcmd_functions = &cmd_functions;
	engdata.pkeybindings = keybindings;
	engdata.pfnConPrintf = Con_Printf;
	engdata.pcvar_vars = &cvar_vars;
	engdata.pglwstate = &glw_state;
	engdata.pfnSZ_GetSpace = SZ_GetSpace;
	engdata.pmodfuncs = &g_modfuncs;
	engdata.pfnGetProcAddress = GetProcAddress;
	engdata.pfnGetModuleHandle = GetModuleHandle;
	engdata.psvs = &svs;
	engdata.pcls = &cls;
	engdata.pfnSV_DropClient = SV_DropClient;
	engdata.pfnNetchan_Transmit = Netchan_Transmit;
	engdata.pfnNET_SendPacket = NET_SendPacket;
	engdata.pfnCvarFindVar = Cvar_FindVar;
	engdata.phinstOpenGlEarly = (int *)&g_hinstOpenGlEarly;
	engdata.pVoid0 = NULL;
	engdata.pVoid1 = NULL;
	engdata.pVoid2 = NULL;
	engdata.pVoid3 = NULL;
	engdata.pVoid4 = NULL;
	engdata.pVoid5 = NULL;
	engdata.pVoid6 = NULL;
	engdata.pVoid7 = NULL;
	engdata.pVoid8 = NULL;
	engdata.pVoid9 = NULL;
	g_modfuncs.m_pfnSetEngData( &engdata );

	g_modfuncs.m_pfnModuleLoaded();

	g_module.fLoaded = true;
}

void LoadSecurityModuleFromDisk( char *pszDllName )
{
	void *nAddrModule;

	g_modfuncs.m_nVersion = MODULE_VERSION_guess;

	NLoadBlobFile( pszDllName, &g_blobfootprintModule, &g_modfuncs, TRUE );

	if ( !g_blobfootprintModule.m_hDll )
		return;

	void *pvPco = malloc( 0x10000 );
	nAddrModule = pvPco;
	g_pvModuleSpecial = pvPco;

	// the p-code of the module sits next to it in a .pco file
	char szFileName_guess[ MAX_PATH ];
	COM_StripExtension( pszDllName, szFileName_guess );
	COM_DefaultExtension( szFileName_guess, ".pco" );

	FileHandle_t fp_guess = FS_Open( szFileName_guess, "rb" );
	FS_Read( pvPco, 0x10000, 1, fp_guess );
	FS_Close( fp_guess );

	CommonLoadSecurityModule( nAddrModule );
}

void LoadSecurityModuleFromMemory( unsigned char *pCode, int nSize )
{
	if ( nSize <= 0 || !pCode )
		return;

	g_modfuncs.m_nVersion = MODULE_VERSION_guess;

	void *nAddrModule = LoadBlobFile_guess( (char *)pCode, &g_blobfootprintModule, &g_modfuncs, nSize );
	CommonLoadSecurityModule( nAddrModule );
}

void CloseSecurityModule( void )
{
	if ( !g_module.fLoaded )
		return;

	cldll_func_dst_t cldstNull =
	{
		(DST_INITIALIZE_FUNC)NullDst,
		(DST_HUD_INIT_FUNC)NullDst,
		(DST_HUD_VIDINIT_FUNC)NullDst,
		(DST_HUD_REDRAW_FUNC)NullDst,
		(DST_HUD_UPDATECLIENTDATA_FUNC)NullDst,
		(DST_HUD_RESET_FUNC)NullDst,
		(DST_HUD_CLIENTMOVE_FUNC)NullDst,
		(DST_HUD_CLIENTMOVEINIT_FUNC)NullDst,
		(DST_HUD_TEXTURETYPE_FUNC)NullDst,
		(DST_HUD_IN_ACTIVATEMOUSE_FUNC)NullDst,
		(DST_HUD_IN_DEACTIVATEMOUSE_FUNC)NullDst,
		(DST_HUD_IN_MOUSEEVENT_FUNC)NullDst,
		(DST_HUD_IN_CLEARSTATES_FUNC)NullDst,
		(DST_HUD_IN_ACCUMULATE_FUNC)NullDst,
		(DST_HUD_CL_CREATEMOVE_FUNC)NullDst,
		(DST_HUD_CL_ISTHIRDPERSON_FUNC)NullDst,
		(DST_HUD_CL_GETCAMERAOFFSETS_FUNC)NullDst,
		(DST_HUD_KB_FIND_FUNC)NullDst,
		(DST_HUD_CAMTHINK_FUNC)NullDst,
		(DST_HUD_CALCREF_FUNC)NullDst,
		(DST_HUD_ADDENTITY_FUNC)NullDst,
		(DST_HUD_CREATEENTITIES_FUNC)NullDst,
		(DST_HUD_DRAWNORMALTRIS_FUNC)NullDst,
		(DST_HUD_DRAWTRANSTRIS_FUNC)NullDst,
		(DST_HUD_STUDIOEVENT_FUNC)NullDst,
		(DST_HUD_POSTRUNCMD_FUNC)NullDst,
		(DST_HUD_SHUTDOWN_FUNC)NullDst,
		(DST_HUD_TXFERLOCALOVERRIDES_FUNC)NullDst,
		(DST_HUD_PROCESSPLAYERSTATE_FUNC)NullDst,
		(DST_HUD_TXFERPREDICTIONDATA_FUNC)NullDst,
		(DST_HUD_DEMOREAD_FUNC)NullDst,
		(DST_HUD_CONNECTIONLESS_FUNC)NullDst,
		(DST_HUD_GETHULLBOUNDS_FUNC)NullDst,
		(DST_HUD_FRAME_FUNC)NullDst,
		(DST_HUD_KEY_EVENT_FUNC)NullDst,
		(DST_HUD_TEMPENTUPDATE_FUNC)NullDst,
		(DST_HUD_GETUSERENTITY_FUNC)NullDst,
		(DST_HUD_VOICESTATUS_FUNC)NullDst,
		(DST_HUD_DIRECTORMESSAGE_FUNC)NullDst,
		(DST_HUD_STUDIO_INTERFACE_FUNC)NullDst,
		(DST_HUD_CHATINPUTPOSITION_FUNC)NullDst,
		(DST_HUD_GETPLAYERTEAM)NullDst
	};

	cl_enginefunc_dst_t engdstNull =
	{
		(pfnEngDst_pfnSPR_Load_t)NullDst,
		(pfnEngDst_pfnSPR_Frames_t)NullDst,
		(pfnEngDst_pfnSPR_Height_t)NullDst,
		(pfnEngDst_pfnSPR_Width_t)NullDst,
		(pfnEngDst_pfnSPR_Set_t)NullDst,
		(pfnEngDst_pfnSPR_Draw_t)NullDst,
		(pfnEngDst_pfnSPR_DrawHoles_t)NullDst,
		(pfnEngDst_pfnSPR_DrawAdditive_t)NullDst,
		(pfnEngDst_pfnSPR_EnableScissor_t)NullDst,
		(pfnEngDst_pfnSPR_DisableScissor_t)NullDst,
		(pfnEngDst_pfnSPR_GetList_t)NullDst,
		(pfnEngDst_pfnFillRGBA_t)NullDst,
		(pfnEngDst_pfnGetScreenInfo_t)NullDst,
		(pfnEngDst_pfnSetCrosshair_t)NullDst,
		(pfnEngDst_pfnRegisterVariable_t)NullDst,
		(pfnEngDst_pfnGetCvarFloat_t)NullDst,
		(pfnEngDst_pfnGetCvarString_t)NullDst,
		(pfnEngDst_pfnAddCommand_t)NullDst,
		(pfnEngDst_pfnHookUserMsg_t)NullDst,
		(pfnEngDst_pfnServerCmd_t)NullDst,
		(pfnEngDst_pfnClientCmd_t)NullDst,
		(pfnEngDst_pfnGetPlayerInfo_t)NullDst,
		(pfnEngDst_pfnPlaySoundByName_t)NullDst,
		(pfnEngDst_pfnPlaySoundByIndex_t)NullDst,
		(pfnEngDst_pfnAngleVectors_t)NullDst,
		(pfnEngDst_pfnTextMessageGet_t)NullDst,
		(pfnEngDst_pfnDrawCharacter_t)NullDst,
		(pfnEngDst_pfnDrawConsoleString_t)NullDst,
		(pfnEngDst_pfnDrawSetTextColor_t)NullDst,
		(pfnEngDst_pfnDrawConsoleStringLen_t)NullDst,
		(pfnEngDst_pfnConsolePrint_t)NullDst,
		(pfnEngDst_pfnCenterPrint_t)NullDst,
		(pfnEngDst_GetWindowCenterX_t)NullDst,
		(pfnEngDst_GetWindowCenterY_t)NullDst,
		(pfnEngDst_GetViewAngles_t)NullDst,
		(pfnEngDst_SetViewAngles_t)NullDst,
		(pfnEngDst_GetMaxClients_t)NullDst,
		(pfnEngDst_Cvar_SetValue_t)NullDst,
		(pfnEngDst_Cmd_Argc_t)NullDst,
		(pfnEngDst_Cmd_Argv_t)NullDst,
		(pfnEngDst_Con_Printf_t)NullDst,
		(pfnEngDst_Con_DPrintf_t)NullDst,
		(pfnEngDst_Con_NPrintf_t)NullDst,
		(pfnEngDst_Con_NXPrintf_t)NullDst,
		(pfnEngDst_PhysInfo_ValueForKey_t)NullDst,
		(pfnEngDst_ServerInfo_ValueForKey_t)NullDst,
		(pfnEngDst_GetClientMaxspeed_t)NullDst,
		(pfnEngDst_CheckParm_t)NullDst,
		(pfnEngDst_Key_Event_t)NullDst,
		(pfnEngDst_GetMousePosition_t)NullDst,
		(pfnEngDst_IsNoClipping_t)NullDst,
		(pfnEngDst_GetLocalPlayer_t)NullDst,
		(pfnEngDst_GetViewModel_t)NullDst,
		(pfnEngDst_GetEntityByIndex_t)NullDst,
		(pfnEngDst_GetClientTime_t)NullDst,
		(pfnEngDst_V_CalcShake_t)NullDst,
		(pfnEngDst_V_ApplyShake_t)NullDst,
		(pfnEngDst_PM_PointContents_t)NullDst,
		(pfnEngDst_PM_WaterEntity_t)NullDst,
		(pfnEngDst_PM_TraceLine_t)NullDst,
		(pfnEngDst_CL_LoadModel_t)NullDst,
		(pfnEngDst_CL_CreateVisibleEntity_t)NullDst,
		(pfnEngDst_GetSpritePointer_t)NullDst,
		(pfnEngDst_pfnPlaySoundByNameAtLocation_t)NullDst,
		(pfnEngDst_pfnPrecacheEvent_t)NullDst,
		(pfnEngDst_pfnPlaybackEvent_t)NullDst,
		(pfnEngDst_pfnWeaponAnim_t)NullDst,
		(pfnEngDst_pfnRandomFloat_t)NullDst,
		(pfnEngDst_pfnRandomLong_t)NullDst,
		(pfnEngDst_pfnHookEvent_t)NullDst,
		(pfnEngDst_Con_IsVisible_t)NullDst,
		(pfnEngDst_pfnGetGameDirectory_t)NullDst,
		(pfnEngDst_pfnGetCvarPointer_t)NullDst,
		(pfnEngDst_Key_LookupBinding_t)NullDst,
		(pfnEngDst_pfnGetLevelName_t)NullDst,
		(pfnEngDst_pfnGetScreenFade_t)NullDst,
		(pfnEngDst_pfnSetScreenFade_t)NullDst,
		(pfnEngDst_VGui_GetPanel_t)NullDst,
		(pfnEngDst_VGui_ViewportPaintBackground_t)NullDst,
		(pfnEngDst_COM_LoadFile_t)NullDst,
		(pfnEngDst_COM_ParseFile_t)NullDst,
		(pfnEngDst_COM_FreeFile_t)NullDst,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		(pfnEngDst_IsSpectateOnly_t)NullDst,
		(pfnEngDst_LoadMapSprite_t)NullDst,
		(pfnEngDst_COM_AddAppDirectoryToSearchPath_t)NullDst,
		(pfnEngDst_COM_ExpandFilename_t)NullDst,
		(pfnEngDst_PlayerInfo_ValueForKey_t)NullDst,
		(pfnEngDst_PlayerInfo_SetValueForKey_t)NullDst,
		(pfnEngDst_GetPlayerUniqueID_t)NullDst,
		(pfnEngDst_GetTrackerIDForPlayer_t)NullDst,
		(pfnEngDst_GetPlayerForTrackerID_t)NullDst,
		(pfnEngDst_pfnServerCmdUnreliable_t)NullDst,
		(pfnEngDst_GetMousePos_t)NullDst,
		(pfnEngDst_SetMousePos_t)NullDst,
		(pfnEngDst_SetMouseEnable_t)NullDst,
		(pfnEngDst_pfnSetFilterMode_t)NullDst,
		(pfnEngDst_pfnSetFilterColor_t)NullDst,
		(pfnEngDst_pfnSetFilterBrightness_t)NullDst,
		(pfnEngDst_pfnSequenceGet_t)NullDst,
		(pfnEngDst_pfnSPR_DrawGeneric_t)NullDst,
		(pfnEngDst_pfnSequencePickSentence_t)NullDst,
		(pfnEngDst_pfnDrawString_t)NullDst,
		(pfnEngDst_pfnDrawString_t)NullDst,
		(pfnEngDst_LocalPlayerInfo_ValueForKey_t)NullDst,
		(pfnEngDst_pfnVGUI2DrawCharacter_t)NullDst,
		(pfnEngDst_pfnVGUI2DrawCharacterAdd_t)NullDst,
		(pfnEngDst_pfnPlaySoundVoiceByName_t)NullDst,
		(pfnEngDst_pfnPrimeMusicStream_t)NullDst,
		(pfnEngDst_pfnProcessTutorMessageDecayBuffer_t)NullDst,
		(pfnEngDst_pfnConstructTutorMessageDecayBuffer_t)NullDst,
		(pfnEngDst_pfnResetTutorMessageDecayData_t)NullDst,
		(pfnEngDst_pfnPlaySoundByNameAtPitch_t)NullDst,
		(pfnEngDst_pfnFillRGBABlend_t)NullDst,
		(pfnEngDst_pfnGetAppID_t)NullDst,
		(pfnEngDst_pfnGetAliases_t)NullDst,
		(pfnEngDst_pfnVguiWrap2_GetMouseDelta_t)NullDst,
		(pfnEngDst_pfnFilteredClientCmd_t)NullDst
	};

	if ( g_modfuncs.m_pfnCloseMod )
		g_modfuncs.m_pfnCloseMod();

	FreeBlob( &g_blobfootprintModule );

	if ( g_pvModuleSpecial )
	{
		free( g_pvModuleSpecial );
		g_pvModuleSpecial = NULL;
	}

	Q_memset( &g_modfuncs, 0, sizeof( g_modfuncs ) );

	memcpy( &g_engdstAddrs, &engdstNull, sizeof( engdstNull ) );
	memcpy( &g_cldstAddrs, &cldstNull, sizeof( cldstNull ) );

	Q_memset( &g_module, 0, sizeof( g_module ) );
}
