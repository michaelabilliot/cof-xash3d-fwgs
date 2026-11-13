// cdll_exp.c -- engine functions exported to the client dll (HUD services)

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "sound.h"
#include "screen.h"
#include "draw.h"
#include "host.h"
#include "vgui_int.h"
#include "APIProxy.h"

// from the client dll interface (common/cdll_int.h)
#define SCRINFO_SCREENFLASH	1

typedef struct SCREENINFO_s
{
	int		iSize;
	int		iWidth;
	int		iHeight;
	int		iFlags;
	int		iCharHeight;
	short	charWidths[256];
} SCREENINFO;

typedef struct hud_player_info_s
{
	char	*name;
	short	ping;
	byte	thisplayer;	// TRUE if this is the calling player

	// stuff that's unused at the moment,  but should be done
	byte	spectator;
	byte	packetloss;

	char	*model;
	short	topcolor;
	short	bottomcolor;

	unsigned __int64	m_nSteamID;
} hud_player_info_t;

typedef int ( *pfnUserMsgHook )( const char *pszName, int iSize, void *pbuf );

// cl_parse.c
pfnUserMsgHook	HookServerMsg( const char *pszName, pfnUserMsgHook pfn );

// r_studio.c
struct sfx_s	*CL_LookupSound( const char *pName );

// host_cmd.c
void	Host_Quit_Restart_f( void );
void	Host_Savegame_f( void );

// sv_main.c
extern	qboolean	g_bIsCStrike;
extern	qboolean	g_bIsCZero;

// vgui_intwrap2.cpp
int		VGUI2_MessageFontInfo( short *pWidth, unsigned int fontHandle );
void	VGuiWrap2_OpenDialog_guess( int dialog );

/*
=================
hudGetScreenInfo
=================
*/
int hudGetScreenInfo( SCREENINFO *pscrinfo )
{
	g_engdstAddrs.pfnGetScreenInfo( &pscrinfo );

	if ( !pscrinfo )
		return 0;

	if ( pscrinfo->iSize != sizeof( SCREENINFO ) )
		return 0;

	pscrinfo->iWidth = vid.width;
	pscrinfo->iHeight = vid.height;
	pscrinfo->iFlags = SCRINFO_SCREENFLASH;
	pscrinfo->iCharHeight = VGUI2_MessageFontInfo( pscrinfo->charWidths, VGUI2_GetCreditsFont() );

	return sizeof( SCREENINFO );
}

/*
=================
hudRegisterVariable
=================
*/
struct cvar_s *hudRegisterVariable( char *szName, char *szValue, int flags )
{
	cvar_t	*cv;

	g_engdstAddrs.pfnRegisterVariable( &szName, &szValue, &flags );

	cv = Z_Malloc( sizeof( cvar_t ) );
	cv->name = szName;
	cv->string = szValue;
	cv->flags = flags;
	cv->flags |= FCVAR_CLIENTDLL;

	Cvar_RegisterVariable( cv );

	return cv;
}

/*
=================
hudGetCvarFloat
=================
*/
float hudGetCvarFloat( char *szName )
{
	cvar_t	*cv;

	g_engdstAddrs.pfnGetCvarFloat( &szName );

	cv = Cvar_FindVar( szName );
	if ( cv )
		return cv->value;

	return 0.0f;
}

/*
=================
hudGetCvarString
=================
*/
char *hudGetCvarString( char *szName )
{
	cvar_t	*cv;

	g_engdstAddrs.pfnGetCvarString( &szName );

	cv = Cvar_FindVar( szName );
	if ( cv )
		return cv->string;

	return NULL;
}

/*
=================
hudAddCommand
=================
*/
int hudAddCommand( char *cmd_name, void ( *function )( void ) )
{
	g_engdstAddrs.pfnAddCommand( &cmd_name, &function );

	Cmd_AddHUDCommand( cmd_name, function );

	return 1;
}

/*
=================
hudServerCmd
=================
*/
int hudServerCmd( char *pszCmdString )
{
	char	buf[2048];

	if ( strstr( pszCmdString, "optionshack" ) )
	{
		char	*pszDialog_guess;

		Cmd_TokenizeString( pszCmdString );
		pszDialog_guess = Cmd_Argv( 1 );
		if ( pszDialog_guess )
			VGuiWrap2_OpenDialog_guess( atoi( pszDialog_guess ) );
		return 0;
	}

	if ( strstr( pszCmdString, "savehack" ) )
	{
		g_bSaveAllowed_guess = true;
		Cmd_TokenizeString( pszCmdString );
		Host_Savegame_f();
		return 0;
	}

	g_engdstAddrs.pfnServerCmd( &pszCmdString );

	_snprintf( buf, sizeof( buf ), "cmd %s", pszCmdString );
	Cmd_TokenizeString( buf );
	Cmd_ForwardToServer();

	return 0;
}

/*
=================
hudServerCmdUnreliable
=================
*/
int hudServerCmdUnreliable( char *pszCmdString )
{
	char	buf[2048];

	if ( strstr( pszCmdString, "savehack" ) )
	{
		g_bSaveAllowed_guess = true;
		Cmd_TokenizeString( pszCmdString );
		Host_Savegame_f();
		return 0;
	}

	g_engdstAddrs.pfnServerCmdUnreliable( &pszCmdString );

	_snprintf( buf, sizeof( buf ), "cmd %s", pszCmdString );
	Cmd_TokenizeString( buf );

	return Cmd_ForwardToServerUnreliable();
}

/*
=================
hudClientCmd
=================
*/
int hudClientCmd( char *pszCmdString )
{
	g_engdstAddrs.pfnClientCmd( &pszCmdString );

	if ( !pszCmdString )
		return 0;

	Cbuf_AddText( pszCmdString );
	Cbuf_AddText( "\n" );

	return 1;
}

/*
=================
hudFilteredClientCmd

Cry of Fear uses this export to restart the game; the command string is
ignored and nothing is returned
=================
*/
int hudFilteredClientCmd( char *pszCmdString )
{
	g_engdstAddrs.pfnFilteredClientCmd();

	Host_Quit_Restart_f();
}

/*
=================
hudHookUserMsg
=================
*/
int hudHookUserMsg( char *szMsgName, pfnUserMsgHook pfn )
{
	g_engdstAddrs.pfnHookUserMsg( &szMsgName, &pfn );

	return HookServerMsg( szMsgName, pfn ) ? 1 : 0;
}

/*
=================
hudGetPlayerInfo
=================
*/
void hudGetPlayerInfo( int ent_num, hud_player_info_t *pinfo )
{
	g_engdstAddrs.pfnGetPlayerInfo( &ent_num, &pinfo );

	if ( ent_num > cl.maxclients || ent_num < 1 || !cl.players[ent_num - 1].name || !cl.players[ent_num - 1].name[0] )
	{
		pinfo->name = NULL;
		pinfo->thisplayer = false;
		return;
	}

	ent_num--;

	pinfo->name = cl.players[ent_num].name;
	pinfo->ping = cl.players[ent_num].ping;
	pinfo->spectator = cl.players[ent_num].spectator;
	pinfo->packetloss = cl.players[ent_num].packet_loss;
	pinfo->thisplayer = ( ent_num == cl.playernum );
	pinfo->topcolor = cl.players[ent_num].topcolor;
	pinfo->bottomcolor = cl.players[ent_num].bottomcolor;
	pinfo->model = cl.players[ent_num].model;

	if ( g_bIsCStrike || g_bIsCZero )
	{
		pinfo->m_nSteamID = cl.players[ent_num].m_nSteamID;
	}
}

/*
=================
hudPlaySoundByName
=================
*/
void hudPlaySoundByName( char *szSound, float volume )
{
	sfx_t	*sfx;

	g_engdstAddrs.pfnPlaySoundByName( &szSound, &volume );

	volume = max( volume, 0.0 );
	volume = min( volume, 1.0 );

	sfx = S_PrecacheSound( szSound );
	if ( !sfx )
	{
		Con_DPrintf( "invalid sound %s\n", szSound );
		return;
	}

	S_StartDynamicSound( cl.viewentity, CHAN_ITEM, sfx, r_origin, volume, 1.0, 0, PITCH_NORM );
}

/*
=================
hudPlaySoundByNameAtPitch
=================
*/
void hudPlaySoundByNameAtPitch( char *szSound, float volume, int pitch )
{
	sfx_t	*sfx;

	g_engdstAddrs.pfnPlaySoundByName( &szSound, &volume );

	volume = max( volume, 0.0 );
	volume = min( volume, 1.0 );
	pitch = max( pitch, 0 );

	sfx = S_PrecacheSound( szSound );
	if ( !sfx )
	{
		Con_DPrintf( "invalid sound %s\n", szSound );
		return;
	}

	S_StartDynamicSound( cl.viewentity, CHAN_ITEM, sfx, r_origin, volume, 1.0, 0, pitch );
}

/*
=================
hudPlaySoundVoiceByName
=================
*/
void hudPlaySoundVoiceByName( char *szSound, float volume, int pitch )
{
	sfx_t	*sfx;

	g_engdstAddrs.pfnPlaySoundVoiceByName( &szSound, &volume );

	volume = max( volume, 0.0 );
	volume = min( volume, 1.0 );
	pitch = max( pitch, 0 );

	sfx = S_PrecacheSound( szSound );
	if ( !sfx )
	{
		Con_DPrintf( "invalid sound %s\n", szSound );
		return;
	}

	S_StartDynamicSound( cl.viewentity, CHAN_BOT, sfx, r_origin, volume, 1.0, 0, pitch );
}

/*
=================
hudPlaySoundByIndex
=================
*/
void hudPlaySoundByIndex( int iSound, float volume )
{
	g_engdstAddrs.pfnPlaySoundByIndex( &iSound, &volume );

	volume = max( volume, 0.0 );
	volume = min( volume, 1.0 );

	if ( iSound > MAX_SOUNDS || iSound < 0 )
	{
		Con_DPrintf( "invalid sound %i\n", iSound );
		return;
	}

	S_StartDynamicSound( cl.viewentity, CHAN_ITEM, cl.sound_precache[iSound], r_origin, volume, 1.0, 0, PITCH_NORM );
}

/*
=================
hudPlaySoundByNameAtLocation
=================
*/
void hudPlaySoundByNameAtLocation( char *szSound, float volume, float *origin )
{
	struct sfx_s	*sfx;

	g_engdstAddrs.pfnPlaySoundByNameAtLocation( &szSound, &volume, &origin );

	volume = max( volume, 0.0 );
	volume = min( volume, 1.0 );

	sfx = CL_LookupSound( szSound );
	if ( sfx )
		S_StartDynamicSound( cl.viewentity, CHAN_AUTO, sfx, origin, volume, 1.0, 0, PITCH_NORM );
}

/*
=================
hudDrawConsoleStringLen
=================
*/
void hudDrawConsoleStringLen( const char *string, int *width, int *height )
{
	g_engdstAddrs.pfnDrawConsoleStringLen( &string, &width, &height );

	*width = Draw_StringLen( string, VGUI2_GetConsoleFont() );
	*height = VGUI2_GetFontTall( VGUI2_GetConsoleFont() );
}

/*
=================
hudConsolePrint
=================
*/
void hudConsolePrint( const char *string )
{
	g_engdstAddrs.pfnConsolePrint( &string );

	Con_Printf( "%s", string );
}

/*
=================
hudCenterPrint
=================
*/
void hudCenterPrint( const char *string )
{
	g_engdstAddrs.pfnConsolePrint( &string );

	SCR_CenterPrint( (char *)string );
}

/*
=================
hudGetClientOrigin
=================
*/
void hudGetClientOrigin( vec_t *origin )
{
	cl_entity_t	*ent;

	ent = cl_entities + cl.viewentity;

	if ( ent->index - 1 == cl.playernum )
	{
		VectorCopy( cl.simorg, origin );
	}
	else
	{
		VectorCopy( ent->origin, origin );
	}
}
