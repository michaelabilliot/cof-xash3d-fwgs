// cl_parse.c  -- parse a message received from the server

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "crc.h"
#include "info.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "progs.h"
#include "eiface.h"
#include "server.h"
#include "GL_MODEL.H"
#include "protocol.h"
#include "vid.h"
#include "render.h"
#include "draw.h"
#include "sound.h"
#include "screen.h"
#include "usermsg.h"
#include "pmove.h"
#include "vgui_int.h"
#include "cdll_int.h"
#include "hashpak.h"
#include "APIProxy.h"
#include "hltv.h"
#include "gl_cvars.h"
#include "gl_water.h"
#include "../pm_shared/pm_movevars.h"

// gl_model.c
qboolean	Mod_ValidateCRC( const char *name, CRC32_t crc );
void	Mod_NeedCRC( const char *name, qboolean needCRC );

// gl_rsurf.c
extern	cvar_t	r_decals;
extern	cvar_t	mp_decals;
void	R_DecalRemoveAll( int textureIndex );
void	R_DecalShoot( int textureIndex, int entity, int modelIndex, vec_t *position, int flags );

// decals.c
int		Draw_DecalIndex( int id );
int		Draw_DecalIndexFromName( char *name );


// r_trans.c
void	R_AllocObjects( int nMax );
void	R_DestroyObjects( void );

// pr_cmds.c
int		PF_IsMapValid_I( char *mapname );

// gl_rmisc.c
void	R_NewMap( void );

// r_studio.c
int		R_GetStudioBounds( const char *filename, float *mins, float *maxs );

typedef void ( *pfnParse )( void );

typedef struct svc_func_s
{
	unsigned char	opcode;		// Opcode
	char			*pszname;	// Display Name
	pfnParse		pfnParse;	// Parse function
} svc_func_t;

int			CL_UPDATE_BACKUP = MULTIPLAYER_BACKUP;
int			CL_UPDATE_MASK = ( MULTIPLAYER_BACKUP - 1 );

// security module of the client dll
qboolean		g_bUpdatingSecurityModules;
unsigned char	g_ucModuleC_MD5[16];

int			msg_buckets[64];

static UserMsg	*gClientUserMsgs = NULL;

double		parsecounttime;

static svc_func_t	cl_parsefuncs[] =
{
	{ svc_bad, "svc_bad", NULL },
	{ svc_nop, "svc_nop", NULL },
	{ svc_disconnect, "svc_disconnect", CL_Parse_Disconnect },
	{ svc_event, "svc_event", CL_ParseEvent },
	{ svc_version, "svc_version", CL_Parse_Version },
	{ svc_setview, "svc_setview", CL_Parse_SetView },
	{ svc_sound, "svc_sound", CL_Parse_Sound },
	{ svc_time, "svc_time", CL_Parse_Time },
	{ svc_print, "svc_print", CL_Parse_Print },
	{ svc_stufftext, "svc_stufftext", CL_Parse_StuffText },
	{ svc_setangle, "svc_setangle", CL_Parse_SetAngle },
	{ svc_serverinfo, "svc_serverinfo", CL_Parse_ServerInfo },
	{ svc_lightstyle, "svc_lightstyle", CL_Parse_LightStyle },
	{ svc_updateuserinfo, "svc_updateuserinfo", CL_Parse_UpdateUserInfo },
	{ svc_deltadescription, "svc_deltadescription", CL_Parse_DeltaDescription },
	{ svc_clientdata, "svc_clientdata", CL_Parse_ClientData },
	{ svc_stopsound, "svc_stopsound", CL_Parse_StopSound },
	{ svc_pings, "svc_pings", CL_Parse_Pings },
	{ svc_particle, "svc_particle", CL_Parse_Particle },
	{ svc_damage, "svc_damage", CL_Parse_Damage },
	{ svc_spawnstatic, "svc_spawnstatic", CL_Parse_SpawnStatic },
	{ svc_event_reliable, "svc_event_reliable", CL_Parse_ReliableEvent },
	{ svc_spawnbaseline, "svc_spawnbaseline", CL_Parse_SpawnBaseline },
	{ svc_temp_entity, "svc_temp_entity", CL_Parse_TempEntity },
	{ svc_setpause, "svc_setpause", CL_Parse_SetPause },
	{ svc_signonnum, "svc_signonnum", CL_Parse_SignonNum },
	{ svc_centerprint, "svc_centerprint", CL_Parse_CenterPrint },
	{ svc_killedmonster, "svc_killedmonster", CL_Parse_KilledMonster },
	{ svc_foundsecret, "svc_foundsecret", CL_Parse_FoundSecret },
	{ svc_spawnstaticsound, "svc_spawnstaticsound", CL_Parse_SpawnStaticSound },
	{ svc_intermission, "svc_intermission", CL_Parse_Intermission },
	{ svc_finale, "svc_finale", CL_Parse_Finale },
	{ svc_cdtrack, "svc_cdtrack", CL_Parse_CDTrack },
	{ svc_restore, "svc_restore", CL_Parse_Restore },
	{ svc_cutscene, "svc_cutscene", CL_Parse_CutScene },
	{ svc_weaponanim, "svc_weaponanim", CL_Parse_WeaponAnim },
	{ svc_decalname, "svc_decalname", CL_Parse_DecalName },
	{ svc_roomtype, "svc_roomtype", CL_Parse_RoomType },
	{ svc_addangle, "svc_addangle", CL_Parse_AddAngle },
	{ svc_newusermsg, "svc_newusermsg", CL_Parse_NewUserMsg },
	{ svc_packetentities, "svc_packetentities", CL_Parse_PacketEntities },
	{ svc_deltapacketentities, "svc_deltapacketentities", CL_Parse_DeltaPacketEntities },
	{ svc_choke, "svc_choke", CL_Parse_Choke },
	{ svc_resourcelist, "svc_resourcelist", CL_Parse_ResourceList },
	{ svc_newmovevars, "svc_newmovevars", CL_Parse_NewMoveVars },
	{ svc_resourcerequest, "svc_resourcerequest", CL_Parse_ResourceRequest },
	{ svc_customization, "svc_customization", CL_Parse_Customization },
	{ svc_crosshairangle, "svc_crosshairangle", CL_Parse_CrosshairAngle },
	{ svc_soundfade, "svc_soundfade", CL_Parse_SoundFade },
	{ svc_filetxferfailed, "svc_filetxferfailed", CL_ParseFileTxferFailed },
	{ svc_hltv, "svc_hltv", CL_Parse_HLTV },
	{ svc_director, "svc_director", CL_Parse_Director },
	{ svc_voiceinit, "svc_voiceinit", CL_Parse_VoiceInit },
	{ svc_voicedata, "svc_voicedata", CL_Parse_VoiceData },
	{ svc_sendextrainfo, "svc_sendextrainfo", CL_Set_ServerExtraInfo },
	{ svc_timescale, "svc_timescale", CL_Parse_Timescale },
	{ svc_resourcelocation, "svc_resourcelocation", CL_Parse_ResourceLocation },
	{ svc_sendcvarvalue, "svc_sendcvarvalue", CL_Send_CvarValue },
	{ svc_sendcvarvalue2, "svc_sendcvarvalue2", CL_Send_CvarValue2 },
	{ (unsigned char)-1, "End of List", NULL }
};

/*
==================
CL_PlayerDropped

==================
*/
void CL_PlayerDropped( int nPlayerNumber )
{
	COM_ClearCustomizationList( &cl.players[nPlayerNumber].customdata, true );
}

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
cl_entity_t *CL_EntityNum( int num )
{
	if ( num >= cl.num_entities )
	{
		if ( num >= cl.max_edicts )
			Host_Error( "CL_EntityNum: %i is an invalid number, cl.max_edicts is %i", num, cl.max_edicts );

		while ( cl.num_entities <= num )
		{
			cl_entities[num].index = num;
			cl.num_entities++;
		}
	}

	cl_entities[num].index = num;
	return &cl_entities[num];
}

/*
=============
AddNewUserMsg

=============
*/
void AddNewUserMsg( void )
{
	UserMsg		*pList = NULL;
	UserMsg		umsg;
	int			i;
	int			fFound = 0;

	umsg.iMsg = MSG_ReadByte();
	umsg.iSize = MSG_ReadByte();
	umsg.pfn = NULL;

	if ( umsg.iSize == 255 )
		umsg.iSize = -1;

	i = MSG_ReadLong();
	Q_strncpy( &umsg.szName[0], (char *)&i, 4 );
	i = MSG_ReadLong();
	Q_strncpy( &umsg.szName[4], (char *)&i, 4 );
	i = MSG_ReadLong();
	Q_strncpy( &umsg.szName[8], (char *)&i, 4 );
	i = MSG_ReadLong();
	Q_strncpy( &umsg.szName[12], (char *)&i, 4 );
	umsg.szName[15] = 0;

	if ( gClientUserMsgs )
	{
		pList = gClientUserMsgs;
		while ( pList )
		{
			if ( !Q_stricmp( pList->szName, umsg.szName ) )
			{
				fFound = 1;
				pList->iMsg = umsg.iMsg;
				pList->iSize = umsg.iSize;
			}
			pList = pList->next;
		}
	}

	if ( !fFound )
	{
		UserMsg	*pumsg;

		pumsg = (UserMsg *)Mem_Malloc( sizeof( UserMsg ) );
		Q_memcpy( pumsg, &umsg, sizeof( UserMsg ) );
		pumsg->next = gClientUserMsgs;
		gClientUserMsgs = pumsg;
	}
}

/*
=============
DispatchUserMsg

=============
*/
void DispatchUserMsg( int iMsg )
{
	static char	buf[192];
	int			MsgSize = 0;
	int			fFound = 0;
	UserMsg		*pList;

	if ( iMsg <= svc_sendcvarvalue2 || iMsg >= 256 )
	{
		Host_Error( "DispatchUserMsg:  Illegal User Msg %d\n", iMsg );
		return;
	}

	for ( pList = gClientUserMsgs; pList; pList = pList->next )
	{
		if ( pList->iMsg != iMsg )
			continue;

		MsgSize = pList->iSize;
		if ( MsgSize == -1 )
			MsgSize = MSG_ReadByte();

		if ( cl_showmessages.value )
			Con_DPrintf( "UserMsg: %s (size %i)\n", pList->szName, MsgSize );

		if ( MsgSize > 192 )
		{
			Host_Error( "DispatchUserMsg:  User Msg %s/%d sent too much data (%i bytes), %i bytes max.\n", pList->szName, pList->iMsg, MsgSize, 192 );
			return;
		}

		MSG_ReadBuf( MsgSize, buf );
		fFound = 1;

		if ( !pList->pfn )
		{
			Con_DPrintf( "UserMsg: No pfn %s %d\n", pList->szName, iMsg );
		}
		else if ( !Q_stricmp( pList->szName, "WeaponList" ) && MsgSize > 123 &&
			( GetGameAppID() != 223710 || !Q_stricmp( com_gamedir, "cryoffear" ) || cl_weaponlistfix.value > 0 ) )
		{
			Con_Printf( "Malformed WeaponList request, ignoring\n" );
		}
		else
		{
			pList->pfn( pList->szName, MsgSize, buf );
		}
		break;
	}

	if ( !fFound )
		Host_Error( "UserMsg: Not Present on Client %d\n", iMsg );
}

/*
=============
DispatchDirectUserMsg

=============
*/
int DispatchDirectUserMsg( const char *pszName, int iSize, void *pBuf )
{
	int				fFound = 0;
	UserMsg			*pList;
	pfnUserMsgHook	pfnRet = NULL;
	int				iMsgSize;

	pList = gClientUserMsgs;
	while ( pList )
	{
		if ( !Q_stricmp( pszName, pList->szName ) )
		{
			fFound = 1;

			iMsgSize = pList->iSize;
			if ( pList->iSize == -1 )
				iMsgSize = iSize;

			if ( !pList->pfn )
			{
				Con_DPrintf( "UserMsg: No pfn %s %d\n", pList->szName, pList->iMsg );
			}
			else
			{
				pList->pfn( pList->szName, iMsgSize, pBuf );
			}
		}
		pList = pList->next;
	}

	return fFound;
}

/*
=============
HookServerMsg

=============
*/
pfnUserMsgHook HookServerMsg( const char *pszName, pfnUserMsgHook pfn )
{
	UserMsg			*pList;
	UserMsg			*pLastMatch = NULL;
	pfnUserMsgHook	pfnRet = NULL;

	pList = gClientUserMsgs;
	while ( pList )
	{
		if ( !Q_stricmp( pszName, pList->szName ) )
		{
			pLastMatch = pList;
			if ( pList->pfn == pfn )
				break;
		}
		pList = pList->next;
	}

	if ( pList )
	{
		pfnRet = pList->pfn;
	}
	else
	{
		pList = (UserMsg *)Mem_ZeroMalloc( sizeof( UserMsg ) );
		if ( pLastMatch )
		{
			Q_memcpy( pList, pLastMatch, sizeof( UserMsg ) );
		}
		else
		{
			Q_strncpy( pList->szName, pszName, sizeof( pList->szName ) - 1 );
			pList->szName[sizeof( pList->szName ) - 1] = 0;
		}

		pList->pfn = pfn;
		pList->next = gClientUserMsgs;
		gClientUserMsgs = pList;
	}

	return pfnRet;
}

/*
=============
CL_ShutDownUsrMessages

=============
*/
void CL_ShutDownUsrMessages( void )
{
	UserMsg		*pList, *pNext;

	if ( !gClientUserMsgs )
		return;

	pList = gClientUserMsgs;
	while ( pList )
	{
		pNext = pList->next;
		Mem_Free( pList );
		pList = pNext;
	}

	gClientUserMsgs = NULL;
}

/*
==================
CL_Parse_Sound

==================
*/
void CL_Parse_Sound( void )
{
	vec3_t	pos;
	int		channel, ent;
	int		sound_num;
	float	volume;
	int		field_mask;
	float	attenuation;
	int		pitch;
	sfx_t	*sfx;
	sfx_t	sfxsentence;

	MSG_StartBitReading( &net_message );

	field_mask = MSG_ReadBits( 9 );

	if ( field_mask & SND_VOLUME )
		volume = MSG_ReadBits( 8 ) / 255.0;
	else
		volume = 1.0;

	if ( field_mask & SND_ATTENUATION )
		attenuation = MSG_ReadBits( 8 ) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	channel = MSG_ReadBits( 3 );
	ent = MSG_ReadBits( MAX_EDICT_BITS );

	if ( field_mask & SND_LARGE_INDEX )
		sound_num = MSG_ReadBits( 16 );
	else
		sound_num = MSG_ReadBits( 8 );

	MSG_ReadBitVec3Coord( pos );

	if ( field_mask & SND_PITCH )
		pitch = MSG_ReadBits( 8 );
	else
		pitch = DEFAULT_SOUND_PACKET_PITCH;

	MSG_EndBitReading( &net_message );

	if ( ent < cl.max_edicts )
	{
		if ( field_mask & SND_SENTENCE )
		{
			sfx = &sfxsentence;

			if ( sound_num >= CVOXFILESENTENCEMAX )
				_snprintf( sfx->name, sizeof( sfx->name ), "!#%d", sound_num - CVOXFILESENTENCEMAX );
			else
				_snprintf( sfx->name, sizeof( sfx->name ), "!%s", rgpszrawsentence[sound_num] );
		}
		else
		{
			sfx = cl.sound_precache[sound_num];
		}

		if ( channel == CHAN_STATIC )
			S_StartStaticSound( ent, CHAN_STATIC, sfx, pos, volume, attenuation, field_mask, pitch );
		else
			S_StartDynamicSound( ent, channel, sfx, pos, volume, attenuation, field_mask, pitch );
	}
	else
	{
		Con_DPrintf( "CL_Parse_Sound: ent = %i, cl.max_edicts %i\n", ent, cl.max_edicts );
	}
}

/*
==============
CL_UpdateUserinfo

==============
*/
void CL_UpdateUserinfo( void )
{
	int				slot;
	player_info_t	*player;

	slot = MSG_ReadByte();
	if ( slot >= MAX_CLIENTS )
		Host_EndGame( "CL_ParseServerMessage: svc_updateuserinfo > MAX_CLIENTS" );

	player = &cl.players[slot];

	player->userid = MSG_ReadLong();
	Q_strncpy( player->userinfo, MSG_ReadString(), MAX_INFO_STRING - 1 );
	player->userinfo[MAX_INFO_STRING - 1] = 0;

	MSG_ReadBuf( sizeof( player->hashedcdkey ), player->hashedcdkey );

	Q_strncpy( player->name, Info_ValueForKey( player->userinfo, "name" ), sizeof( player->name ) - 1 );
	player->name[sizeof( player->name ) - 1] = 0;

	Q_strncpy( player->model, Info_ValueForKey( player->userinfo, "model" ), sizeof( player->model ) - 1 );
	player->model[sizeof( player->model ) - 1] = 0;

	player->topcolor = Q_atoi( Info_ValueForKey( player->userinfo, "topcolor" ) );
	player->bottomcolor = Q_atoi( Info_ValueForKey( player->userinfo, "bottomcolor" ) );
	player->spectator = Q_atoi( Info_ValueForKey( player->userinfo, "*hltv" ) );
	player->m_nSteamID = _atoi64( Info_ValueForKey( player->userinfo, "*sid" ) );

	if ( !player->userinfo[0] || !player->name[0] )
	{
		CL_PlayerDropped( slot );
	}
}

/*
===============
CL_CheckFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean CL_CheckFile( sizebuf_t *msg, char *filename )
{
	char	file[MAX_PATH];

	Q_strncpy( file, filename, sizeof( file ) - 1 );
	file[sizeof( file ) - 1] = 0;

	if ( Q_strstr( file, ".." ) || Q_strstr( file, "server.cfg" ) )
	{
		Con_DPrintf( "Refusing to download a path with '..'\n" );
		return true;
	}

	if ( !cl_allow_download.value )
	{
		Con_DPrintf( "Download refused, cl_allow_download is 0\n" );
		return true;
	}

	if ( cls.state == ca_active && !cl_download_ingame.value )
	{
		Con_DPrintf( "In-game download refused...\n" );
		return true;
	}

	if ( FS_FileExists( file ) )
		return true;

	if ( cls.passive || cls.demoplayback )
	{
		Con_Printf( "Warning! File %s missing during demo playback.\n", file );
		return true;
	}

	if ( CL_CanUseHTTPDownload() )
	{
		CL_QueueHTTPDownload( file );
	}
	else
	{
		MSG_WriteByte( msg, clc_stringcmd );
		MSG_WriteString( msg, va( "dlfile %s", file ) );
	}

	return false;
}

/*
==================
CL_ClearResourceList

==================
*/
void CL_ClearResourceList( resource_t *pList )
{
	resource_t	*p, *n;

	p = pList->pNext;
	while ( p && p != pList )
	{
		n = p->pNext;

		CL_RemoveFromResourceList( p );
		Mem_Free( p );

		p = n;
	}

	pList->pPrev = pList;
	pList->pNext = pList;
}

/*
==================
CL_ClearResourceLists

==================
*/
void CL_ClearResourceLists( void )
{
	cl.downloadUrl[0] = 0;

	CL_ClearResourceList( &cl.resourcesneeded );
	CL_ClearResourceList( &cl.resourcesonhand );
}

/*
==================
CL_CheckCRCs

==================
*/
qboolean CL_CheckCRCs( char *pszMap )
{
	char	szDllName[64];

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_CheckCRCs()" );

	ContinueLoadingProgressBar( "ClientConnect", 4, 0.0 );
	SetLoadingProgressBarStatusText( "#GameUI_CheckCRCs" );

	// Don't verify CRC if we are running a local server (i.e., we are playing single player, or we are the server in multiplay
	if ( !Host_IsServerActive() )
	{
		// Check to see that our copy of the map is the same as the server's
		CRC32_Init( &cl.mapCRC );
		if ( !CRC_MapFile( &cl.mapCRC, pszMap ) )
		{
			// Does the file exist?
			if ( FS_FileExists( pszMap ) )
			{
				COM_ExplainDisconnection( true, "#GameUI_ConnectionFailed", pszMap );
				Host_Error( "Disconnected" );
				return false;
			}

			if ( !cl_allow_download.value )
			{
				COM_ExplainDisconnection( true, "Refusing to download map %s, (cl_allowdownload is 0 ) disconnecting.\n", pszMap );
				Host_Error( "Disconnected" );
				return false;
			}

			Con_Printf( "Couldn't find map %s, server will download the map\n", pszMap );

			// Mark the map as matching so we can download it
			cl.mapCRC = cl.serverCRC;
		}

		if ( cl.serverCRC != cl.mapCRC && !cls.demoplayback )
		{
			COM_ExplainDisconnection( true, "Your map [%s] differs from the server's.", pszMap );
			Host_Error( "Disconnected" );
			return false;
		}
	}

	// Check to see that our copy of the client side dll matches the server's.
	if ( _stricmp( com_gamedir, "cstrike" ) && _stricmp( com_gamedir, "cstrike_beta" ) && _stricmp( com_gamedir, "czero" ) &&
		_stricmp( com_gamedir, "tfc" ) && _stricmp( com_gamedir, "valve" ) && _stricmp( com_gamedir, "dod" ) &&
		_stricmp( com_gamedir, "ricochet" ) && _stricmp( com_gamedir, "dmc" ) && _stricmp( com_gamedir, "cryoffear" ) )
	{
		_snprintf( szDllName, sizeof( szDllName ), "cl_dlls\\client.dll" );

		if ( !MD5_Hash_File( cls.md5_clientdll, szDllName, false, false, NULL ) )
		{
			COM_ExplainDisconnection( true, "Couldn't CRC client side dll %s.", szDllName );
			Host_Error( "Disconnected" );
			return false;
		}

		if ( Q_memcmp( cl.clientdllmd5, cls.md5_clientdll, sizeof( cl.clientdllmd5 ) ) )
		{
			if ( cls.demoplayback || cls.spectator )
			{
				Con_Printf( "Your client side .dll [%s] failed the CRC check.\nThe .dll may be out of date, or the demo may have been recorded using an old version of the .dll.\nConsider obtaining an updated version of the .dll from the server operator.\nDemo playback proceeding.\n", szDllName );
			}
			else
			{
				COM_ExplainDisconnection( true, "Your .dll [%s] differs from the server's.", szDllName );
				Host_Error( "Disconnected" );
				return false;
			}
		}
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_CheckCRCs()" );

	return true;
}

/*
==================
CL_PrecacheBSPModels

==================
*/
void CL_PrecacheBSPModels( char *pfilename )
{
	resource_t	*p, *n;

	if ( !pfilename || Q_strnicmp( pfilename, "maps/", Q_strlen( "maps/" ) ) || !Q_strstr( pfilename, ".bsp" ) )
		return;

	ContinueLoadingProgressBar( "ClientConnect", 9, 0.0 );

	for ( p = cl.resourcesonhand.pNext; p != &cl.resourcesonhand; p = n )
	{
		n = p->pNext;

		if ( p->type != t_model )
			continue;

		if ( p->szFileName[0] != '*' )
			continue;

		cl.model_precache[p->nIndex] = Mod_ForName( p->szFileName, false, false );
		if ( !cl.model_precache[p->nIndex] )
		{
			Con_Printf( "Model %s not found\n", p->szFileName );

			if ( p->ucFlags & RES_FATALIFMISSING )
			{
				COM_ExplainDisconnection( true, "Cannot continue without model %s, disconnecting.", p->szFileName );
				CL_Disconnect();
				return;
			}
		}
	}
}

/*
==================
CL_SendConsistencyInfo

==================
*/
void CL_SendConsistencyInfo( sizebuf_t *msg )
{
	int				i;
	consistency_t	*pc;
	byte			md5[16];
	char			filename[MAX_PATH];
	vec3_t			mins, maxs;
	int				savepos, len;
	char			szSteamFilename[MAX_PATH];
	int				iFileAvailable = 1;
	int				iRunningSteam;

	iRunningSteam = COM_CheckParm( "-steam" );

	if ( !cl.need_force_consistency_response )
		return;

	cl.need_force_consistency_response = false;

	MSG_WriteByte( msg, clc_fileconsistency );

	savepos = msg->cursize;
	MSG_WriteShort( msg, 0 );

	MSG_StartBitWriting( msg );

	for ( i = 0; i < cl.num_consistency; i++ )
	{
		qboolean	user_changed_diskfile = false;

		iFileAvailable = 1;

		MSG_WriteBits( 1, 1 );

		pc = &cl.consistency_list[i];

		MSG_WriteBits( pc->orig_index, 12 );

		if ( pc->issound )
		{
			_snprintf( filename, sizeof( filename ), "sound/%s", pc->filename );
		}
		else
		{
			Q_strncpy( filename, pc->filename, sizeof( filename ) - 1 );
			filename[sizeof( filename ) - 1] = 0;
		}

		if ( iRunningSteam )
		{
			_snprintf( szSteamFilename, sizeof( szSteamFilename ), "%s", filename );
			COM_FixSlashes( szSteamFilename );

			if ( !FS_FileExists( szSteamFilename ) )
				iFileAvailable = 0;
		}

		if ( strstr( filename, "models/" ) && iFileAvailable )
		{
			CRC32_t	crcFile;

			CRC32_Init( &crcFile );
			CRC_File( &crcFile, filename );
			crcFile = CRC32_Final( crcFile );

			if ( !Mod_ValidateCRC( filename, crcFile ) )
				user_changed_diskfile = true;
		}

		switch ( pc->check_type )
		{
		case force_exactfile:
			MD5_Hash_File( md5, filename, false, false, NULL );
			pc->value = *(int *)&md5[0];

			if ( user_changed_diskfile )
				MSG_WriteBits( 0, 32 );
			else
				MSG_WriteBits( pc->value, 32 );
			break;

		case force_model_samebounds:
		case force_model_specifybounds:
			if ( !R_GetStudioBounds( filename, mins, maxs ) )
			{
				Host_Error( "Unable to find %s\n", filename );
				return;
			}

			if ( user_changed_diskfile )
			{
				maxs[0] = 9999.9;
				maxs[1] = 9999.9;
				maxs[2] = 9999.9;
				mins[0] = -9999.9;
				mins[1] = -9999.9;
				mins[2] = -9999.9;
			}

			MSG_WriteBitData( mins, 12 );
			MSG_WriteBitData( maxs, 12 );
			break;

		case force_model_specifybounds_if_avail:
			if ( iFileAvailable )
			{
				if ( !R_GetStudioBounds( filename, mins, maxs ) )
				{
					Host_Error( "Unable to find %s\n", filename );
					return;
				}

				if ( user_changed_diskfile )
				{
					maxs[0] = 9999.9;
					maxs[1] = 9999.9;
					maxs[2] = 9999.9;
					mins[0] = -9999.9;
					mins[1] = -9999.9;
					mins[2] = -9999.9;
				}
			}
			else
			{
				maxs[0] = -1;
				maxs[1] = -1;
				maxs[2] = -1;
				mins[0] = -1;
				mins[1] = -1;
				mins[2] = -1;
			}

			MSG_WriteBitData( mins, 12 );
			MSG_WriteBitData( maxs, 12 );
			break;

		default:
			Host_Error( "Unknown consistency type %i\n", pc->check_type );
			return;
		}
	}

	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( msg );

	len = msg->cursize - savepos - 2;
	*(short *)&msg->data[savepos] = len;

	COM_Munge( &msg->data[savepos + 2], len, cl.servercount );
}

/*
==================
CL_RegisterResources

Clean up and move to next part of sequence.
==================
*/
void CL_RegisterResources( sizebuf_t *msg )
{
	if ( cls.dl.custom || ( cls.signon == SIGNONS && cls.state == ca_active ) )
	{
		cls.dl.custom = false;
		return;
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_RegisterResources()" );

	ContinueLoadingProgressBar( "ClientConnect", 8, 0.0 );

	if ( !cls.demoplayback )
		CL_SendConsistencyInfo( msg );

	// All done precaching.
	cl.worldmodel = cl.model_precache[1];
	cl_entities[0].model = cl.worldmodel;

	if ( !cl.worldmodel )
		Sys_Error( "Client world model is NULL\n" );

	CL_PrecacheBSPModels( cl.model_precache[1]->name );

	if ( cls.state == ca_disconnected )
		Con_DPrintf( "Setting up renderer...\n" );

	R_NewMap();

	Hunk_Check();	// make sure nothing is hurt

	noclip_anglehack = false;

	if ( !cls.passive )
	{
		int		mungebuffer;

		mungebuffer = cl.mapCRC;

		// Tell the server we are ready to spawn
		MSG_WriteByte( msg, clc_stringcmd );
		COM_Munge2( (byte *)&mungebuffer, sizeof( mungebuffer ), ( 0xff - cl.servercount ) & 0xff );
		MSG_WriteString( msg, va( "spawn %i %i", cl.servercount, mungebuffer ) );
	}

	CL_InitSpectator();

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_RegisterResources()" );
}

/*
==================
CL_AddToResourceList

==================
*/
void CL_AddToResourceList( resource_t *pResource, resource_t *pList )
{
	if ( pResource->pPrev || pResource->pNext )
	{
		Con_Printf( "Resource already linked\n" );
		return;
	}

	if ( !pList->pPrev || !pList->pNext )
	{
		Sys_Error( "Resource list corrupted.\n" );
		return;
	}

	pResource->pPrev = pList->pPrev;
	pList->pPrev->pNext = pResource;
	pList->pPrev = pResource;
	pResource->pNext = pList;
}

/*
==================
CL_RemoveFromResourceList

==================
*/
void CL_RemoveFromResourceList( resource_t *pResource )
{
	if ( !pResource->pPrev || !pResource->pNext )
		Sys_Error( "Mislinked resource in CL_RemoveFromResourceList" );

	if ( pResource->pNext == pResource || pResource->pPrev == pResource )
		Sys_Error( "Attempt to free last entry in list." );

	pResource->pPrev->pNext = pResource->pNext;
	pResource->pNext->pPrev = pResource->pPrev;

	pResource->pPrev = NULL;
	pResource->pNext = NULL;
}

/*
==================
CL_EstimateNeededResources

==================
*/
int CL_EstimateNeededResources( void )
{
	resource_t	*p;
	int			nTotalSize;
	int			nSize;

	p = cl.resourcesneeded.pNext;
	nTotalSize = 0;

	while ( p != &cl.resourcesneeded )
	{
		nSize = 0;

		switch ( p->type )
		{
		case t_sound:
			if ( p->szFileName[0] == '*' )
				break;

			nSize = FS_FileSize( va( "sound/%s", p->szFileName ) );
			if ( !nSize )
				nSize = -1;
			break;

		case t_model:
			if ( p->szFileName[0] == '*' )
				break;

			if ( !p->ucFlags )
				break;

			nSize = FS_FileSize( p->szFileName );
			if ( !nSize )
				nSize = -1;
			break;

		case t_skin:
			nSize = FS_FileSize( p->szFileName );
			if ( !nSize )
				nSize = -1;
			break;

		case t_decal:
			if ( p->ucFlags & RES_CUSTOM )
			{
				nSize = -1;
			}
			break;

		case t_generic:
			nSize = FS_FileSize( p->szFileName );
			if ( !nSize )
				nSize = -1;
			break;

		case t_eventscript:
			nSize = FS_FileSize( p->szFileName );
			if ( !nSize )
				nSize = -1;
			break;
		}

		if ( nSize == -1 )
		{
			p->ucFlags |= RES_WASMISSING;
			nSize = p->nDownloadSize;
		}
		else
		{
			nSize = 0;
		}

		nTotalSize += nSize;

		p = p->pNext;
	}

	return nTotalSize;
}

/*
==================
CL_BatchResourceRequest

==================
*/
void CL_BatchResourceRequest( void )
{
	byte		data[65536];
	sizebuf_t	msg;
	resource_t	*p, *n;

	Q_memset( &msg, 0, sizeof( msg ) );

	msg.buffername = "Client to Server Resource Batch";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	CL_HTTPStop_f();

	for ( p = cl.resourcesneeded.pNext; p && p != &cl.resourcesneeded; p = n )
	{
		n = p->pNext;

		if ( !( p->ucFlags & RES_WASMISSING ) )
		{
			CL_MoveToOnHandList( p );
			continue;
		}

		if ( cls.state == ca_active && !cl_download_ingame.value )
		{
			Con_Printf( "Skipping in game download of %s\n", p->szFileName );
			CL_MoveToOnHandList( p );
			continue;
		}

		switch ( p->type )
		{
		case t_sound:
			if ( p->szFileName[0] == '*' )
			{
				CL_MoveToOnHandList( p );
				break;
			}

			if ( CL_CheckFile( &msg, va( "sound/%s", p->szFileName ) ) )
			{
				CL_MoveToOnHandList( p );
			}
			break;

		case t_model:
			if ( p->szFileName[0] == '*' )
			{
				CL_MoveToOnHandList( p );
				break;
			}

			if ( CL_CheckFile( &msg, p->szFileName ) )
			{
				CL_MoveToOnHandList( p );
			}
			break;

		case t_skin:
			CL_MoveToOnHandList( p );
			break;

		case t_decal:
			if ( HPAK_GetDataPointer( "custom.hpk", p, NULL, NULL ) )
			{
				CL_MoveToOnHandList( p );
			}
			else if ( !( p->ucFlags & RES_REQUESTED ) )
			{
				char	filename[256];

				_snprintf( filename, sizeof( filename ), "!MD5%s", COM_BinPrintf( p->rgucMD5_hash, 16 ) );

				MSG_WriteByte( &msg, clc_stringcmd );
				MSG_WriteString( &msg, va( "dlfile %s", filename ) );

				p->ucFlags |= RES_REQUESTED;
			}
			break;

		case t_generic:
			if ( !IsSafeFileToDownload( p->szFileName ) )
			{
				CL_RemoveFromResourceList( p );
				Con_Printf( "Invalid file type...skipping download of %s\n", p->szFileName );
				Mem_Free( p );
			}
			else if ( CL_CheckFile( &msg, p->szFileName ) )
			{
				CL_MoveToOnHandList( p );
			}
			break;

		case t_eventscript:
			if ( CL_CheckFile( &msg, p->szFileName ) )
			{
				CL_MoveToOnHandList( p );
			}
			break;
		}
	}

	if ( CL_GetDownloadQueueSize() > 0 )
	{
		char	tmpUrl[128];

		memcpy( tmpUrl, cl.downloadUrl, sizeof( tmpUrl ) );

		CL_MarkMapAsUsingHTTPDownload();
		CL_Disconnect();

		StartLoadingProgressBar( tmpUrl, CL_GetDownloadQueueSize() );
		SetLoadingProgressBarStatusText( "#GameUI_VerifyingAndDownloading" );
	}

	if ( cls.state == ca_disconnected )
		return;

	if ( !msg.cursize && CL_PrecacheResources() )
	{
		CL_RegisterResources( &msg );
	}

	if ( !cls.passive )
	{
		Netchan_CreateFragments( false, &cls.netchan, &msg );
		Netchan_FragSend( &cls.netchan );
	}
}

/*
==================
CL_StartResourceDownloading

==================
*/
void CL_StartResourceDownloading( char *pszMessage, qboolean bCustom )
{
	resourceinfo_t	ri;

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_StartResourceDownloading()" );

	if ( pszMessage )
		Con_DPrintf( "%s", pszMessage );

	cls.dl.nTotalSize = COM_SizeofResourceList( &cl.resourcesneeded, &ri );
	cls.dl.nTotalToTransfer = CL_EstimateNeededResources();

	if ( !bCustom )
	{
		cls.state = ca_uninitialized;
		cls.dl.custom = false;
		gfExtendedError = false;
	}
	else
	{
		cls.dl.custom = true;
	}

	cls.dl.doneregistering = false;
	cls.dl.fLastStatusUpdate = realtime;
	cls.dl.nRemainingToTransfer = cls.dl.nTotalToTransfer;
	Q_memset( cls.dl.rgStats, 0, sizeof( cls.dl.rgStats ) );
	cls.dl.nCurStat = 0;

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_StartResourceDownloading()" );

	CL_BatchResourceRequest();
}

/*
==================
CL_ParseConsistencyInfo

==================
*/
void CL_ParseConsistencyInfo( void )
{
	int				lastcheck = 0;
	int				delta;
	int				i;
	int				isdelta;
	resource_t		*r;
	resource_t		*skip_crc_change = NULL;
	int				skip;
	qboolean		hasinfo;
	consistency_t	*pc;
	byte			nullbuffer[32];

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_ParseConsistencyInfo()" );

	Q_memset( nullbuffer, 0, sizeof( nullbuffer ) );

	r = cl.resourcesneeded.pNext;

	hasinfo = MSG_ReadBits( 1 );
	cl.need_force_consistency_response = ( hasinfo != 0 );

	if ( !cl.need_force_consistency_response )
		return;

	while ( MSG_ReadBits( 1 ) )
	{
		isdelta = MSG_ReadBits( 1 );
		if ( isdelta )
		{
			i = lastcheck + MSG_ReadBits( 5 );
		}
		else
		{
			i = MSG_ReadBits( 10 );
		}

		delta = i - lastcheck;
		lastcheck = i;
		skip = delta;

		while ( delta-- > 0 )
		{
			if ( r != skip_crc_change && strstr( r->szFileName, "models/" ) )
			{
				Mod_NeedCRC( r->szFileName, false );
			}
			r = r->pNext;
		}

		skip_crc_change = r;

		if ( strstr( r->szFileName, "models/" ) )
		{
			Mod_NeedCRC( r->szFileName, true );
		}

		if ( cl.num_consistency >= MAX_CONSISTENCY_LIST )
		{
			Host_Error( "Consistency:  server sent too many filenames\n" );
			return;
		}

		pc = &cl.consistency_list[cl.num_consistency];
		cl.num_consistency++;

		Q_memset( pc, 0, sizeof( consistency_t ) );

		pc->issound = ( r->type == t_sound );
		pc->filename = r->szFileName;
		pc->orig_index = i;
		pc->value = 0;

		if ( r->type == t_model && Q_memcmp( nullbuffer, r->rguc_reserved, 32 ) )
		{
			COM_UnMunge( r->rguc_reserved, 32, cl.servercount );
			pc->check_type = r->rguc_reserved[0];
		}
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_ParseConsistencyInfo()" );
}

/*
==================
CL_ParseResourceList

==================
*/
void CL_ParseResourceList( void )
{
	int			total;
	int			i;
	resource_t	*resource;

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_ParseResourceList()" );

	MSG_StartBitReading( &net_message );

	total = MSG_ReadBits( 12 );

	for ( i = 0; i < total; i++ )
	{
		resource = (resource_t *)Mem_ZeroMalloc( sizeof( resource_t ) );

		resource->type = MSG_ReadBits( 4 );

		Q_strncpy( resource->szFileName, MSG_ReadBitString(), sizeof( resource->szFileName ) - 1 );
		resource->szFileName[sizeof( resource->szFileName ) - 1] = 0;

		resource->nIndex = MSG_ReadBits( 12 );
		resource->nDownloadSize = MSG_ReadBits( 24 );
		resource->ucFlags = MSG_ReadBits( 3 );
		resource->ucFlags &= ~RES_WASMISSING;

		if ( resource->ucFlags & RES_CUSTOM )
		{
			MSG_ReadBitData( resource->rgucMD5_hash, 16 );
		}

		if ( MSG_ReadBits( 1 ) )
		{
			MSG_ReadBitData( resource->rguc_reserved, sizeof( resource->rguc_reserved ) );
		}

		CL_AddToResourceList( resource, &cl.resourcesneeded );
	}

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_ParseResourceList()" );

	CL_ParseConsistencyInfo();

	MSG_EndBitReading( &net_message );

	SetLoadingProgressBarStatusText( "#GameUI_VerifyingResources" );
	CL_StartResourceDownloading( "Verifying and downloading resources...\n", false );
}

/*
==================
CL_ParseResourceLocation

==================
*/
void CL_ParseResourceLocation( void )
{
	char	*url;

	url = MSG_ReadString();
	if ( url )
	{
		if ( !_strnicmp( "http://", url, 7 ) || !_strnicmp( "https://", url, 8 ) )
		{
			const char	*lastSlash;

			lastSlash = strrchr( url, '/' );
			if ( lastSlash && !lastSlash[1] )
			{
				strncpy( cl.downloadUrl, url, sizeof( cl.downloadUrl ) );
				cl.downloadUrl[sizeof( cl.downloadUrl ) - 1] = 0;
			}
			else
			{
				_snprintf( cl.downloadUrl, sizeof( cl.downloadUrl ), "%s/", url );
			}

			Con_DPrintf( "Using %s as primary download location\n", cl.downloadUrl );
		}
	}
}

/*
==================
CL_PlayerHasCustomization

==================
*/
customization_t *CL_PlayerHasCustomization( int nPlayerNum, resourcetype_t type )
{
	customization_t	*pList;

	pList = cl.players[nPlayerNum].customdata.pNext;
	while ( pList )
	{
		if ( pList->resource.type == type )
			return pList;

		pList = pList->pNext;
	}

	return NULL;
}

/*
==================
CL_RemoveCustomization

==================
*/
void CL_RemoveCustomization( int nPlayerNum, customization_t *pRemove )
{
	customization_t	*pList;
	int				i;

	pList = &cl.players[nPlayerNum].customdata;

	while ( pList->pNext )
	{
		if ( pList->pNext == pRemove )
		{
			customization_t	*pNext;

			pNext = pList->pNext->pNext;

			if ( pRemove->bInUse && pRemove->pBuffer )
			{
				Mem_Free( pRemove->pBuffer );
			}

			if ( pRemove->bInUse && pRemove->pInfo )
			{
				if ( pRemove->resource.type == t_decal )
				{
					cachewad_t		*pWad;
					cacheentry_t	*pic;

					if ( cls.state == ca_active )
					{
						R_DecalRemoveAll( -( nPlayerNum + 1 ) );
					}

					pWad = (cachewad_t *)pRemove->pInfo;

					Mem_Free( pWad->lumps );

					pic = pWad->cache;
					for ( i = 0; i < pWad->cacheCount; i++, pic++ )
					{
						if ( Cache_Check( &pic->cache ) )
						{
							Cache_Free( &pic->cache );
						}
					}

					Mem_Free( pWad->name );
					Mem_Free( pWad->cache );
				}

				Mem_Free( pRemove->pInfo );
			}

			Mem_Free( pRemove );

			pList->pNext = pNext;
		}
		else
		{
			pList = pList->pNext;
		}
	}
}

/*
==================
CL_ParseCustomization

==================
*/
void CL_ParseCustomization( void )
{
	resource_t		*resource;
	int				i;
	customization_t	*pExistingCustomization;
	customization_t	*pList;
	qboolean		bFound = false;

	i = MSG_ReadByte();
	if ( i < 0 || i >= MAX_CLIENTS )
	{
		Host_Error( "Bogus player index during customization parsing.\n" );
		return;
	}

	resource = (resource_t *)Mem_ZeroMalloc( sizeof( resource_t ) );

	resource->type = MSG_ReadByte();

	Q_strncpy( resource->szFileName, MSG_ReadString(), sizeof( resource->szFileName ) - 1 );
	resource->szFileName[sizeof( resource->szFileName ) - 1] = 0;

	resource->nIndex = MSG_ReadShort();
	resource->nDownloadSize = MSG_ReadLong();
	resource->ucFlags = MSG_ReadByte();
	resource->pNext = NULL;
	resource->pPrev = NULL;
	resource->ucFlags &= ~RES_WASMISSING;

	if ( resource->ucFlags & RES_CUSTOM )
	{
		MSG_ReadBuf( 16, resource->rgucMD5_hash );
	}

	resource->playernum = i;

	if ( !cl_allow_download.value )
	{
		Con_DPrintf( "Refusing new resource, cl_allow_download set to 0\n" );
		Mem_Free( resource );
		return;
	}

	if ( cls.state == ca_active && !cl_download_ingame.value )
	{
		Con_DPrintf( "Refusing new resource, cl_download_ingame set to 0\n" );
		Mem_Free( resource );
		return;
	}

	pExistingCustomization = CL_PlayerHasCustomization( resource->playernum, resource->type );
	if ( pExistingCustomization )
	{
		CL_RemoveCustomization( resource->playernum, pExistingCustomization );
	}

	// Check for un-customization
	pList = cl.players[resource->playernum].customdata.pNext;
	while ( pList )
	{
		if ( !Q_memcmp( pList->resource.rgucMD5_hash, resource->rgucMD5_hash, 16 ) )
		{
			bFound = true;
			break;
		}

		pList = pList->pNext;
	}

	if ( HPAK_GetDataPointer( "custom.hpk", resource, NULL, NULL ) )
	{
		qboolean	bNoError;

		if ( !bFound )
		{
			bNoError = COM_CreateCustomization( &cl.players[resource->playernum].customdata, resource, resource->playernum, FCUST_FROMHPAK, NULL, NULL );
			if ( !bNoError )
			{
				Con_DPrintf( "Error loading customization\n" );
			}
		}
		else
		{
			Con_DPrintf( "Duplicate resource ignored for local client\n" );
		}
	}
	else
	{
		resource->ucFlags |= RES_WASMISSING;
		CL_AddToResourceList( resource, &cl.resourcesneeded );
		Con_Printf( "Requesting %s from server\n", resource->szFileName );
		CL_StartResourceDownloading( "Custom resource propagation...\n", true );
		return;
	}

	Mem_Free( resource );
}

/*
==================
CL_RequestMissingResources

==================
*/
qboolean CL_RequestMissingResources( void )
{
	resource_t	*p;

	if ( cls.dl.doneregistering )
		return false;

	if ( !cls.dl.custom && cls.state != ca_uninitialized )
		return false;

	p = cl.resourcesneeded.pNext;
	if ( p == &cl.resourcesneeded )
	{
		cls.dl.custom = false;
		cls.dl.doneregistering = true;
		return false;
	}

	if ( !( p->ucFlags & RES_WASMISSING ) )
	{
		CL_MoveToOnHandList( p );
		return true;
	}

	return false;
}

/*
==================
CL_DeallocateDynamicData

==================
*/
void CL_DeallocateDynamicData( void )
{
	if ( cl_entities )
	{
		Mem_Free( cl_entities );
		cl_entities = NULL;
	}

	R_DestroyObjects();
}

/*
==================
CL_ReallocateDynamicData

==================
*/
void CL_ReallocateDynamicData( int nMaxClients )
{
	cl.max_edicts = COM_EntsForPlayerSlots( nMaxClients );

	if ( cl.max_edicts <= 0 )
		Sys_Error( "CL_ReallocateDynamicData allocating 0 entities" );

	if ( cl_entities )
		Con_Printf( "CL_Reallocate cl_entities\n" );

	cl_entities = (cl_entity_t *)Mem_ZeroMalloc( cl.max_edicts * sizeof( cl_entity_t ) );

	R_AllocObjects( cl.max_edicts );

	Q_memset( cl.frames, 0, CL_UPDATE_BACKUP * sizeof( frame_t ) );
}

/*
==================
CL_CreateCustomizationList

==================
*/
void CL_CreateCustomizationList( void )
{
	int				i;
	resource_t		*pResource;
	player_info_t	*pPlayer;

	pPlayer = &cl.players[cl.playernum];
	pPlayer->customdata.pNext = NULL;

	for ( i = 0; i < cl.num_resources; i++ )
	{
		pResource = &cl.resourcelist[i];

		if ( !COM_CreateCustomization( &pPlayer->customdata, pResource, cl.playernum, 0, NULL, NULL ) )
		{
			Con_DPrintf( "Problem with client customization %i, ignoring...", i );
		}
	}
}

/*
==================
CL_CheckGameDirectory

==================
*/
qboolean CL_CheckGameDirectory( char *gamedir )
{
	char	szGD[MAX_PATH];

	if ( !gamedir || !gamedir[0] )
	{
		Con_Printf( "Server didn't specify a gamedir, assuming no change\n" );
		return true;
	}

	COM_FileBase( com_gamedir, szGD );

	if ( Q_stricmp( szGD, gamedir ) )
	{
		Con_Printf( "Server is running game %s.  Restart in that game to connect.\n", gamedir );
		CL_Disconnect_f();
		return false;
	}

	return true;
}

extern	char	*szCommonPreloads;
extern	char	*szReslistsBaseDir;
extern	char	*szReslistsExt;

/*
==================
CL_BuildMapCycleListHints

==================
*/
int CL_BuildMapCycleListHints( char **hints, char *mapCycleMsg, char *firstMap )
{
	char	szMap[MAX_PATH * 2 + 4];
	int		length;
	char	*pFileList;
	char	*aFileList;
	char	szMod[MAX_PATH];

	length = strlen( mapCycleMsg );
	pFileList = mapCycleMsg;
	aFileList = pFileList;

	COM_FileBase( com_gamedir, szMod );

	if ( pFileList && length )
	{
		sprintf( szMap, "%s\\%s\\%s%s\r\n%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(), szCommonPreloads, szReslistsExt, szReslistsBaseDir, GetCurrentSteamAppName(), firstMap, szReslistsExt );

		*hints = (char *)Mem_ZeroMalloc( strlen( szMap ) + 1 );
		if ( !*hints )
			return 0;

		strcpy( *hints, szMap );

		while ( 1 )
		{
			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strncpy( szMap, com_token, sizeof( szMap ) - 1 );
			szMap[sizeof( szMap ) - 1] = 0;

			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
			}

			if ( PF_IsMapValid_I( szMap ) && strcmp( firstMap, szMap ) )
			{
				char	mapLine[MAX_PATH * 2 + 4];

				sprintf( mapLine, "%s\\%s\\%s%s\r\n", szReslistsBaseDir, GetCurrentSteamAppName(), szMap, szReslistsExt );

				*hints = (char *)Mem_Realloc( *hints, strlen( *hints ) + strlen( mapLine ) + 2 );
				if ( !*hints )
					return 0;

				strcat( *hints, mapLine );
			}
		}

		sprintf( szMap, "%s\\%s\\mp_maps.txt\r\n", szReslistsBaseDir, GetCurrentSteamAppName() );

		*hints = (char *)realloc( *hints, strlen( *hints ) + strlen( szMap ) + 2 );
		strcat( *hints, szMap );
	}

	return 1;
}

/*
==================
CL_ParseServerInfo

==================
*/
void CL_ParseServerInfo( void )
{
	char	*str;
	char	*gamedir;
	int		i;
	int		mungebuffer;
	char	mapBaseName[MAX_PATH];

	if ( fs_startup_timings.value )
		AddStartupTiming( "begin CL_ParseServerInfo()" );

	HPAK_CheckSize( "custom" );

	Con_DPrintf( "Serverinfo packet received.\n" );

	ContinueLoadingProgressBar( "ClientConnect", 3, 0.0 );
	SetLoadingProgressBarStatusText( "#GameUI_ParseServerInfo" );

	if ( cls.passive )
	{
		NET_LeaveGroup( cls.netchan.sock, cls.connect_stream );
	}

	//
	// wipe the client_state_t struct
	//
	CL_ClearState( false );

	ClientDLL_HudVidInit();

	cls.demowaiting = false;	// we can start recording now

	// parse protocol version number
	i = MSG_ReadLong();
	if ( i != PROTOCOL_VERSION && !cls.demoplayback )
	{
		Con_Printf( "Server returned version %i, not %i\n", i, PROTOCOL_VERSION );
		return;
	}

	cl.servercount = MSG_ReadLong();

	if ( cls.demoplayback )
	{
		// Override the server count if we are playing back a demo.
		gHostSpawnCount++;
		cl.servercount = gHostSpawnCount;
	}

	// The CRC of the server map must match the CRC of the client map, or else
	// the client is probably cheating.
	mungebuffer = MSG_ReadLong();
	cl.serverCRC = mungebuffer;

	// The MD5 hash of the client dll the server expects
	MSG_ReadBuf( sizeof( cl.clientdllmd5 ), cl.clientdllmd5 );

	cl.maxclients = MSG_ReadByte();
	if ( cl.maxclients < 1 || cl.maxclients > MAX_CLIENTS )
	{
		Con_Printf( "Bad maxclients (%u) from server\n", cl.maxclients );
		return;
	}

	if ( cl.maxclients > 1 && mp_decals.value < r_decals.value )
	{
		Cvar_SetValue( "r_decals", mp_decals.value );
	}

	R_ForceCVars( cl.maxclients > 1 );

	CL_DeallocateDynamicData();
	CL_ReallocateDynamicData( cl.maxclients );

	cl.playernum = MSG_ReadByte();

	COM_UnMunge3( (byte *)&cl.serverCRC, sizeof( cl.serverCRC ), ( 0xff - cl.playernum ) & 0xff );

	// wipe the customizations
	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		COM_ClearCustomizationList( &cl.players[i].customdata, true );
	}

	CL_CreateCustomizationList();

	// gametype
	cl.gametype = MSG_ReadByte();

	// parse gamedir
	gamedir = MSG_ReadString();
	if ( !CL_CheckGameDirectory( gamedir ) )
		return;

	str = MSG_ReadString();
	Con_DPrintf( "Remote host:  %s\n", str );

	// parse signon message
	str = MSG_ReadString();
	Q_strncpy( cl.levelname, str, sizeof( cl.levelname ) - 1 );
	cl.levelname[sizeof( cl.levelname ) - 1] = 0;

	str = MSG_ReadString();

	COM_FileBase( cl.levelname, mapBaseName );

	if ( cl.maxclients > 1 )
	{
		char	*mapcycle = NULL;

		if ( CL_BuildMapCycleListHints( &mapcycle, str, mapBaseName ) )
		{
			FS_HintResourceNeed( mapcycle, true );
		}

		if ( mapcycle )
		{
			Mem_Free( mapcycle );
		}

		FS_LogLevelLoadStarted( mapBaseName );
		VGuiWrap2_LoadingStarted( "level", mapBaseName );
	}

	cls.isVAC2Secure = MSG_ReadByte();

	if ( !CL_CheckCRCs( cl.levelname ) )
		return;

	gHostSpawnCount = cl.servercount;

	cls.state = ca_connected;

	CL_BeginDemoStartup();

	CL_WaitForResources_guess();

	if ( fs_startup_timings.value )
		AddStartupTiming( "end   CL_ParseServerInfo()" );

	ResetLevelState_guess();
}

/*
==================
CL_ParseListen

==================
*/
void CL_ParseListen( void )
{
	char	*stream_IP;

	cls.signon = SIGNONS;

	stream_IP = MSG_ReadString();
	Net_StringToAdr( stream_IP, &cls.game_stream );

	NET_JoinGroup( cls.netchan.sock, cls.game_stream );

	SCR_EndLoadingPlaque();
}

/*
==================
CL_ParseBaseline

==================
*/
void CL_ParseBaseline( void )
{
	int				i;
	qboolean		custom;
	unsigned char	type;
	entity_state_t	nullstate;
	delta_t			**ppentity = NULL;
	delta_t			**ppcustom = NULL;
	delta_t			**ppplayer = NULL;
	int				index;
	cl_entity_t		*ent;

	ppentity = DELTA_LookupRegistration( "entity_state_t" );
	ppplayer = DELTA_LookupRegistration( "entity_state_player_t" );
	ppcustom = DELTA_LookupRegistration( "custom_entity_state_t" );

	Q_memset( &nullstate, 0, sizeof( nullstate ) );

	MSG_StartBitReading( &net_message );

	ContinueLoadingProgressBar( "ClientConnect", 11, 0.0 );
	SetLoadingProgressBarStatusText( "#GameUI_ParseBaseline" );

	cl.highentity = 0;

	while ( 1 )
	{
		if ( MSG_PeekBits( 16 ) == 0xFFFF )
			break;

		index = MSG_ReadBits( MAX_EDICT_BITS );

		cl.highentity = max( index, cl.highentity );

		ent = CL_EntityNum( index );

		type = MSG_ReadBits( 2 );
		custom = ( type & ENTITY_BEAM ) ? true : false;

		if ( custom )
		{
			DELTA_ParseDelta( (byte *)&nullstate, (byte *)&ent->baseline, *ppcustom );
		}
		else
		{
			DELTA_ParseDelta( (byte *)&nullstate, (byte *)&ent->baseline, CL_IsPlayerIndex( index ) ? *ppplayer : *ppentity );
		}

		ent->baseline.entityType = type;
	}

	MSG_ReadBits( 16 );

	Q_memset( cl.instanced_baseline, 0, sizeof( cl.instanced_baseline ) );

	cl.instanced_baseline_number = MSG_ReadBits( 6 );

	for ( i = 0; i < cl.instanced_baseline_number; i++ )
	{
		DELTA_ParseDelta( (byte *)&nullstate, (byte *)&cl.instanced_baseline[i], *ppentity );
	}

	MSG_EndBitReading( &net_message );

	ent->curstate = ent->baseline;
	ent->prevstate = ent->baseline;
}

/*
==================
CL_ParseStatic

==================
*/
void CL_ParseStatic( void )
{
}

/*
==================
CL_ParseStaticSound

==================
*/
void CL_ParseStaticSound( void )
{
	vec3_t	org;
	int		sound_num;
	float	vol, atten;
	int		i;
	int		ent;
	int		pitch;
	int		flags;
	sfx_t	*sfx;
	sfx_t	sfxsentence;

	for ( i = 0; i < 3; i++ )
		org[i] = MSG_ReadCoord( &net_message );

	sound_num = MSG_ReadShort();
	vol = MSG_ReadByte() / 255.0;
	atten = MSG_ReadByte() / 64.0;
	ent = MSG_ReadShort();
	pitch = MSG_ReadByte();
	flags = MSG_ReadByte();

	if ( flags & SND_SENTENCE )
	{
		sfx = &sfxsentence;
		_snprintf( sfx->name, sizeof( sfx->name ), "!%s", rgpszrawsentence[sound_num] );
	}
	else
	{
		sfx = cl.sound_precache[sound_num];
	}

	S_StartStaticSound( ent, CHAN_STATIC, sfx, org, vol, atten, flags, pitch );
}

/*
==================
CL_ParseMovevars

==================
*/
void CL_ParseMovevars( void )
{
	movevars.gravity = MSG_ReadFloat();
	movevars.stopspeed = MSG_ReadFloat();
	movevars.maxspeed = MSG_ReadFloat();
	movevars.spectatormaxspeed = MSG_ReadFloat();
	movevars.accelerate = MSG_ReadFloat();
	movevars.airaccelerate = MSG_ReadFloat();
	movevars.wateraccelerate = MSG_ReadFloat();
	movevars.friction = MSG_ReadFloat();
	movevars.edgefriction = MSG_ReadFloat();
	movevars.waterfriction = MSG_ReadFloat();
	movevars.entgravity = MSG_ReadFloat();
	movevars.bounce = MSG_ReadFloat();
	movevars.stepsize = MSG_ReadFloat();
	movevars.maxvelocity = MSG_ReadFloat();
	movevars.zmax = MSG_ReadFloat();
	movevars.waveHeight = MSG_ReadFloat();
	movevars.footsteps = MSG_ReadByte();
	movevars.rollangle = MSG_ReadFloat();
	movevars.rollspeed = MSG_ReadFloat();
	movevars.skycolor_r = MSG_ReadFloat();
	movevars.skycolor_g = MSG_ReadFloat();
	movevars.skycolor_b = MSG_ReadFloat();
	movevars.skyvec_x = MSG_ReadFloat();
	movevars.skyvec_y = MSG_ReadFloat();
	movevars.skyvec_z = MSG_ReadFloat();

	Q_strncpy( movevars.skyName, MSG_ReadString(), sizeof( movevars.skyName ) - 1 );
	movevars.skyName[sizeof( movevars.skyName ) - 1] = 0;

	// Adjust the clipping plane
	if ( movevars.zmax != gl_zmax.value )
	{
		Cvar_SetValue( "gl_zmax", movevars.zmax );
	}

	// Set the water wave amplitude
	if ( movevars.waveHeight != gl_wateramp.value )
	{
		Cvar_SetValue( "gl_wateramp", movevars.waveHeight );
	}

	if ( cl_entities )
	{
		cl_entities->curstate.scale = gl_wateramp.value;
	}
}

/*
==================
CL_ParseSoundFade

==================
*/
void CL_ParseSoundFade( void )
{
	int		percent;
	int		inTime, holdTime, outTime;

	percent = MSG_ReadByte();
	holdTime = MSG_ReadByte();
	inTime = MSG_ReadByte();
	outTime = MSG_ReadByte();

	cls.soundfade.soundFadeStartTime = realtime;
	cls.soundfade.nStartPercent = percent;
	cls.soundfade.soundFadeOutTime = outTime;
	cls.soundfade.soundFadeHoldTime = holdTime;
	cls.soundfade.soundFadeInTime = inTime;
}

/*
==================
CL_Parse_DeltaDescription

==================
*/
void CL_Parse_DeltaDescription( void )
{
	char				*s;
	char				szDesc[256];
	delta_t				**ppdelta;
	delta_description_t	*pdesc;
	delta_description_t	nulldesc;
	delta_description_t	*from, *to;
	int					i, c;

	Q_memset( &nulldesc, 0, sizeof( nulldesc ) );
	from = &nulldesc;

	s = MSG_ReadString();
	if ( !s || !s[0] )
	{
		Host_Error( "CL_Parse_DeltaDescription:  Illegible description name\n" );
		return;
	}

	Q_strncpy( szDesc, s, sizeof( szDesc ) - 1 );
	szDesc[sizeof( szDesc ) - 1] = 0;

	ppdelta = DELTA_LookupRegistration( szDesc );
	if ( ppdelta && *ppdelta )
	{
		DELTA_FreeDescription( ppdelta );
	}

	MSG_StartBitReading( &net_message );

	c = MSG_ReadBits( 16 );

	*ppdelta = (delta_t *)Mem_ZeroMalloc( sizeof( delta_t ) );
	pdesc = (delta_description_t *)Mem_ZeroMalloc( c * sizeof( delta_description_t ) );

	( *ppdelta )->dynamic = 1;
	( *ppdelta )->fieldCount = c;

	for ( i = 0; i < c; i++ )
	{
		to = &pdesc[i];
		DELTA_ParseDelta( (byte *)from, (byte *)to, &g_MetaDelta );
	}

	( *ppdelta )->pdd = pdesc;

	MSG_EndBitReading( &net_message );
}

/*
==================
CL_Restore

==================
*/
void CL_Restore( const char *fileName )
{
	DECALLIST		decalList;
	int				i, decalCount, temp, mapCount;
	FileHandle_t	pFile;
	char			name[16];
	char			*pMapName;

	pFile = FS_Open( fileName, "rb" );
	if ( pFile )
	{
		FS_SetVBuf( pFile, NULL, 0, 4096 );

		FS_Read( &temp, sizeof( int ), 1, pFile );
		FS_Read( &i, sizeof( int ), 1, pFile );

		if ( temp == ( ( 'V' << 24 ) | ( 'L' << 16 ) | ( 'A' << 8 ) | 'V' ) )
		{
			FS_Read( &decalCount, sizeof( int ), 1, pFile );

			for ( i = 0; i < decalCount; i++ )
			{
				FS_Read( name, sizeof( name ), 1, pFile );
				FS_Read( &decalList.entityIndex, sizeof( short ), 1, pFile );
				FS_Read( &decalList.depth, sizeof( byte ), 1, pFile );
				FS_Read( &decalList.flags, sizeof( byte ), 1, pFile );
				FS_Read( decalList.position, sizeof( vec3_t ), 1, pFile );

				if ( r_decals.value )
				{
					temp = Draw_DecalIndexFromName( name );
					R_DecalShoot( Draw_DecalIndex( temp ), decalList.entityIndex, 0, decalList.position, decalList.flags );
				}
			}
		}

		FS_Close( pFile );
	}

	mapCount = MSG_ReadByte();
	for ( i = 0; i < mapCount; i++ )
	{
		pMapName = MSG_ReadString();
		Con_Printf( "Loading decals from %s\n", pMapName );
	}
}

int			total_data[64];
int			last_data[64];

/*
==================
CL_DumpMessageLoad_f

==================
*/
void CL_DumpMessageLoad_f( void )
{
	int		i;
	int		total = 0;

	Con_Printf( "-------- Message Load ---------\n" );

	for ( i = 0; i < 63; i++ )
	{
		if ( !msg_buckets[i] )
			continue;

		Con_Printf( "%i:%s: %i msgs:%.2fK\n", i, ( i <= svc_sendcvarvalue2 ) ? cl_parsefuncs[i].pszname : "bogus #", msg_buckets[i], total_data[i] / 1024.0f );

		total += msg_buckets[i];
	}

	Con_Printf( "User messages:  %i:%.2fK\n", msg_buckets[63], total_data[63] / 1024.0f );
	total += msg_buckets[63];

	Con_Printf( "------ End:  %i Total----\n", total );
}

/*
==================
CL_TransferMessageData

==================
*/
void CL_TransferMessageData( void )
{
	int		i;

	for ( i = 0; i < 64; i++ )
	{
		total_data[i] += last_data[i];
	}
}

/*
==================
CL_ResetFrame

==================
*/
void CL_ResetFrame( frame_t *frame )
{
	frame->invalid = false;
	frame->choked = false;
	frame->latency = 0.0;
	frame->receivedtime = realtime;

	frame->clientbytes = 0;
	frame->packetentitybytes = 0;
	frame->tentitybytes = 0;
	frame->playerinfobytes = 0;
	frame->soundbytes = 0;
	frame->usrbytes = 0;
	frame->eventbytes = 0;
	frame->msgbytes = 0;

	frame->time = cl.mtime[0];
}

/*
==================
CL_MsgInfo

==================
*/
char *CL_MsgInfo( int cmd )
{
	static char	sz[256];

	Q_strcpy( sz, "???" );

	if ( cmd <= svc_sendcvarvalue2 )
	{
		Q_strncpy( sz, cl_parsefuncs[cmd].pszname, sizeof( sz ) - 1 );
		sz[sizeof( sz ) - 1] = 0;
	}
	else
	{
		UserMsg	*pList;

		for ( pList = gClientUserMsgs; pList; pList = pList->next )
		{
			if ( pList->iMsg != cmd )
				continue;

			Q_strncpy( sz, pList->szName, sizeof( sz ) - 1 );
			sz[sizeof( sz ) - 1] = 0;
			break;
		}
	}

	return sz;
}

/*
==================
CL_WriteErrorMessage

==================
*/
void CL_WriteErrorMessage( int starting_count, int current_count, sizebuf_t *msg )
{
	FileHandle_t	fp;
	char			name[MAX_PATH];
	const char		*buffer_file = "buffer.dat";

	_snprintf( name, sizeof( name ), "%s", buffer_file );

	COM_FixSlashes( name );
	COM_CreatePath( name );

	fp = FS_Open( name, "wb" );
	if ( !fp )
		return;

	FS_Write( &starting_count, sizeof( int ), 1, fp );
	FS_Write( &current_count, sizeof( int ), 1, fp );
	FS_Write( msg->data, msg->cursize, 1, fp );
	FS_Close( fp );

	Con_Printf( "Wrote erroneous message to %s\n", buffer_file );
}

#define	MSG_BACKUP	32
#define	MSG_MASK	( MSG_BACKUP - 1 )

typedef struct
{
	int		command;
	int		starting_offset;
	int		frame_number;
} oldcmd_t;

static oldcmd_t	oldcmd[MSG_BACKUP];
static int		currentcmd;

/*
==================
CL_WriteMessageHistory

==================
*/
void CL_WriteMessageHistory( int starting_count, int cmd )
{
	int		i;
	int		thecmd;

	Con_Printf( "Last %i messages parsed.\n", MSG_BACKUP );

	// Finish here
	for ( i = MSG_BACKUP - 1; i >= 1; i-- )
	{
		thecmd = currentcmd - i - 1;
		thecmd &= MSG_MASK;

		Con_Printf( "%i %04i %s\n", oldcmd[thecmd].frame_number, oldcmd[thecmd].starting_offset, CL_MsgInfo( oldcmd[thecmd].command ) );
	}

	Con_Printf( "BAD:  %3i:%s\n", msg_readcount - 1, CL_MsgInfo( cmd ) );

	if ( developer.value )
	{
		CL_WriteErrorMessage( starting_count, msg_readcount - 1, &net_message );
	}
}

#define SHOWNET( x ) if ( cl_shownet.value == 2 && Q_strlen( x ) > 1 ) Con_Printf( "%3i:%s\n", msg_readcount - 1, x );

/*
=====================
CL_ParseServerMessage

=====================
*/
void CL_ParseServerMessage( qboolean normal_message )
{
	int		cmd;
	int		playerbits = 0;
	int		starting_count = msg_readcount;
	int		bufStart, bufEnd;

	//
	// if recording demos, copy the message out
	//
	if ( cl_shownet.value == 1 )
	{
		Con_Printf( "%i ", net_message.cursize );
	}
	else if ( cl_shownet.value == 2 )
	{
		Con_Printf( "------------------\n" );
	}

	Q_memset( last_data, 0, sizeof( last_data ) );

	if ( normal_message )
	{
		if ( cls.state == ca_active )
		{
			cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK].invalid = true;
			cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK].choked = false;
		}
		else
		{
			CL_ResetFrame( &cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK] );
		}
	}

	//
	// parse the message
	//
	while ( 1 )
	{
		if ( msg_badread )
		{
			Host_Error( "CL_ParseServerMessage: Bad server message" );
		}

		bufStart = msg_readcount;

		cmd = MSG_ReadByte();
		if ( cmd == -1 )
		{
			SHOWNET( "END OF MESSAGE" );
			break;
		}

		if ( cmd != svc_nop )
		{
			oldcmd[currentcmd & MSG_MASK].command = cmd;
			oldcmd[currentcmd & MSG_MASK].starting_offset = bufStart;
			oldcmd[currentcmd & MSG_MASK].frame_number = host_framecount;
			currentcmd++;
		}

		if ( cmd > svc_sendcvarvalue2 )
		{
			msg_buckets[63]++;

			DispatchUserMsg( cmd );

			bufEnd = msg_readcount;
			last_data[63] += bufEnd - bufStart;
			cl.frames[cl.parsecountmod].usrbytes += bufEnd - bufStart;
			continue;
		}

		SHOWNET( cl_parsefuncs[cmd].pszname );

		if ( cmd <= 63 && cmd >= 0 )
		{
			msg_buckets[cmd]++;
		}

		if ( cmd == svc_bad )
		{
			CL_WriteMessageHistory( starting_count, cmd );
			Host_Error( "CL_ParseServerMessage: Illegible server message - %s\n", cl_parsefuncs[cmd].pszname );
		}
		else if ( cl_parsefuncs[cmd].pfnParse )
		{
			void	( *func )( void );

			func = cl_parsefuncs[cmd].pfnParse;

			if ( cl_showmessages.value )
			{
				Con_DPrintf( "Msg: %s\n", cl_parsefuncs[cmd].pszname );
			}

			func();
		}

		bufEnd = msg_readcount;
		last_data[cmd] += bufEnd - bufStart;

		if ( cmd == svc_packetentities || cmd == svc_deltapacketentities )
		{
			int		playerbytes;

			playerbytes = cls.playerbits / 8;

			cl.frames[cl.parsecountmod].packetentitybytes += bufEnd - bufStart;
			cl.frames[cl.parsecountmod].packetentitybytes -= playerbytes;
			cl.frames[cl.parsecountmod].playerinfobytes += playerbytes;
		}
		else if ( cmd == svc_temp_entity )
		{
			cl.frames[cl.parsecountmod].tentitybytes += bufEnd - bufStart;
		}
		else if ( cmd == svc_sound )
		{
			cl.frames[cl.parsecountmod].soundbytes += bufEnd - bufStart;
		}
		else if ( cmd == svc_clientdata )
		{
			cl.frames[cl.parsecountmod].clientbytes += bufEnd - bufStart;
		}
		else if ( cmd == svc_event || cmd == svc_event_reliable )
		{
			cl.frames[cl.parsecountmod].eventbytes += bufEnd - bufStart;
		}
	}

	if ( cls.state == ca_active )
	{
		cl.frames[cl.parsecountmod].msgbytes += net_message.cursize;
	}

	CL_TransferMessageData();

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if ( !cls.demoplayback )
	{
		if ( cls.state != ca_active )
		{
			CL_WriteDemoStartup( starting_count, &net_message );
		}

		if ( cls.demorecording && !cls.demowaiting )
		{
			CL_WriteDemoMessage( starting_count, &net_message );
		}
	}
}

/*
==================
CL_ParseHLTV

==================
*/
void CL_ParseHLTV( void )
{
	int				cmd;
	unsigned int	flags;

	cmd = MSG_ReadByte();
	flags = 0;

	switch ( cmd )
	{
	case HLTV_ACTIVE:
		cls.spectator = true;
		g_bRedirectedToProxy = false;
		break;

	case HLTV_STATUS:
		MSG_ReadLong();
		MSG_ReadShort();
		MSG_ReadWord();
		MSG_ReadLong();
		MSG_ReadLong();
		MSG_ReadWord();
		break;

	case HLTV_LISTEN:
		CL_ParseListen();
		break;

	default:
		Con_Printf( " CL_Parse_HLTV: unknown HLTV command.\n" );
		break;
	}
}
