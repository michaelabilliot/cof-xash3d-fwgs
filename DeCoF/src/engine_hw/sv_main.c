// sv_main.c -- server main program

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "info.h"
#include "host.h"
#include "net.h"
#include "protocol.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "pmove.h"
#include "../pm_shared/pm_movevars.h"
#include "sound.h"
#include "server.h"
#include "world.h"
#include "filter.h"
#include "usermsg.h"
#include "cdll_int.h"
#include "inst_baseline.h"
#include "decal.h"
#include "modinfo.h"
#include "client.h"
#include "hltv.h"
#include "tmessage.h"
#include "sv_log.h"
#include "crc.h"
#include "hashpak.h"
#include "bspfile.h"
#include "in_buttons.h"
#include "screen.h"
#include "vgui_int.h"
#include "keys.h"
#include "dll_state.h"

#include <stdarg.h>
#include <malloc.h>
#include <ctype.h>

typedef struct
{
	int				num_entities;
	entity_state_t	entities[MAX_PACKET_ENTITIES];
} full_packet_entities_t;

typedef struct delta_info_s
{
	struct delta_info_s	*next;
	char			*name;
	char			*loadfile;
	delta_t			*delta;
} delta_info_t;

server_t		sv;					// local server
server_static_t	svs;				// persistant server info

static int		sv_lastnum;
static extra_baselines_t	g_sv_instance_baselines;
qboolean		g_bOutOfDateRestart;

int				g_userid = 1;

playermove_t	g_svmove;

delta_info_t	*g_sv_delta;

/*
================
SV_LookupDelta

================
*/
delta_t *SV_LookupDelta( char *name )
{
	delta_info_t	*p;

	p = g_sv_delta;
	while ( p )
	{
		if ( !Q_stricmp( name, p->name ) )
			return p->delta;
		p = p->next;
	}

	Sys_Error( "Couldn't find delta for %s\n", name );
	return NULL;
}

static delta_t	*g_pplayerdelta;
static delta_t	*g_pentitydelta;
static delta_t	*g_pcustomentitydelta;
static delta_t	*g_pclientdelta;
static delta_t	*g_pweapondelta;
delta_t			*g_peventdelta;

char			*pr_strings, *gNullString = "";
globalvars_t	gGlobalVariables;

decalname_t		sv_decalnames[MAX_DECALS];

int				num_servers;
int				gPacketSuppressed;
int				sv_decalnamecount;
char			localmodels[MAX_MODELS][5];			// inline model names for precache
char			localinfo[MAX_LOCALINFO];

UserMsg			*sv_gpNewUserMsgs;
UserMsg			*sv_gpUserMsgs;
static int		giNextUserMsg = 64;

ipfilter_t		ipfilters[MAX_IPFILTERS];
int				numipfilters;
userfilter_t	userfilters[MAX_USERFILTERS];
int				numuserfilters;

cvar_t	rcon_password = { "rcon_password", "" };
cvar_t	sv_enableoldqueries = { "sv_enableoldqueries", "0" };
cvar_t	sv_instancedbaseline = { "sv_instancedbaseline", "1" };
cvar_t	sv_contact = { "sv_contact", "", FCVAR_SERVER };
cvar_t	sv_region = { "sv_region", "-1", FCVAR_SERVER };
cvar_t	sv_maxupdaterate = { "sv_maxupdaterate", "30.0" };
cvar_t	sv_minupdaterate = { "sv_minupdaterate", "10.0" };
cvar_t	sv_filterban = { "sv_filterban", "1" };
cvar_t	sv_minrate = { "sv_minrate", "0", FCVAR_SERVER };
cvar_t	sv_maxrate = { "sv_maxrate", "0", FCVAR_SERVER };
cvar_t	sv_logrelay = { "sv_logrelay", "0" };
cvar_t	sv_language = { "sv_language", "0" };
cvar_t	violence_hblood = { "violence_hblood", "1" };
cvar_t	violence_ablood = { "violence_ablood", "1" };
cvar_t	violence_hgibs = { "violence_hgibs", "1" };
cvar_t	violence_agibs = { "violence_agibs", "1" };
cvar_t	sv_newunit = { "sv_newunit", "0" };
cvar_t	sv_aim = { "sv_aim", "1", FCVAR_ARCHIVE | FCVAR_SERVER };
cvar_t	laddermode = { "laddermode", "0" };
cvar_t	sv_clienttrace = { "sv_clienttrace", "1", FCVAR_SERVER };
cvar_t	sv_timeout = { "sv_timeout", "60" };
cvar_t	sv_failuretime = { "sv_failuretime", "0.5" };
cvar_t	sv_cheats = { "sv_cheats", "0", FCVAR_SERVER };
cvar_t	sv_password = { "sv_password", "", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t	sv_lan = { "sv_lan", "0" };
cvar_t	sv_lan_rate = { "sv_lan_rate", "20000.0" };
cvar_t	sv_proxies = { "sv_proxies", "1", FCVAR_SERVER };
cvar_t	sv_outofdatetime = { "sv_outofdatetime", "1800" };
cvar_t	mapcyclefile = { "mapcyclefile", "mapcycle.txt" };
cvar_t	motdfile = { "motdfile", "motd.txt" };
cvar_t	servercfgfile = { "servercfgfile", "server.cfg" };
cvar_t	mapchangecfgfile = { "mapchangecfgfile", "" };
cvar_t	lservercfgfile = { "lservercfgfile", "listenserver.cfg" };
cvar_t	logsdir = { "logsdir", "logs" };
cvar_t	bannedcfgfile = { "bannedcfgfile", "banned.cfg" };
cvar_t	sv_allow_download = { "sv_allowdownload", "1" };
cvar_t	sv_send_logos = { "sv_send_logos", "1" };
cvar_t	sv_send_resources = { "sv_send_resources", "1" };
cvar_t	sv_logbans = { "sv_logbans", "0" };
cvar_t	sv_allow_upload = { "sv_allowupload", "1", FCVAR_SERVER };
cvar_t	sv_max_upload = { "sv_uploadmax", "0.5", FCVAR_SERVER };
cvar_t	hpk_maxsize = { "hpk_maxsize", "4", FCVAR_ARCHIVE };
cvar_t	sv_skycolor_r = { "sv_skycolor_r", "0" };
cvar_t	sv_skycolor_g = { "sv_skycolor_g", "0" };
cvar_t	sv_skycolor_b = { "sv_skycolor_b", "0" };
cvar_t	sv_skyvec_x = { "sv_skyvec_x", "0" };
cvar_t	sv_skyvec_y = { "sv_skyvec_y", "0" };
cvar_t	sv_skyvec_z = { "sv_skyvec_z", "0" };
cvar_t	sv_visiblemaxplayers = { "sv_visiblemaxplayers", "-1" };
cvar_t	max_queries_sec = { "max_queries_sec", "3.0", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t	max_queries_sec_global = { "max_queries_sec_global", "30", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t	max_queries_window = { "max_queries_window", "60", FCVAR_SERVER | FCVAR_PROTECTED };
cvar_t	sv_logblocks = { "sv_logblocks", "0", FCVAR_SERVER };
cvar_t	sv_downloadurl = { "sv_downloadurl", "", FCVAR_PROTECTED };
cvar_t	sv_allow_dlfile = { "sv_allow_dlfile", "1" };
cvar_t	sv_version = { "sv_version", "" };

int		sv_playermodel;

int		SV_UPDATE_BACKUP = 8;
int		SV_UPDATE_MASK = 7;

/*
================
SV_DownloadingModules

================
*/
void SV_DownloadingModules( void )
{
	char	message[] = { 'D', 'o', 'w', 'n', 'l', 'o', 'a', 'd', 'i', 'n', 'g', ' ',
		'S', 'e', 'c', 'u', 'r', 'i', 't', 'y', ' ', 'M', 'o', 'd', 'u', 'l', 'e', ' ',
		'f', 'r', 'o', 'm', ' ', 'S', 'p', 'e', 'a', 'k', 'e', 'a', 's', 'y', '.', 'n', 'e', 't', ' ',
		'.', '.', '.', '\n', 0 };

	Con_Printf( message );
}

/*
================
SV_GatherStatistics

================
*/
void SV_GatherStatistics( void )
{
	int		players;
	float	occup;
	int		i, c;
	float	v;
	client_t	*cl;

	if ( realtime > svs.next_cleartime )
	{
		SV_CountPlayers( &players );
		svs.next_cleartime = realtime + 3600.0;
		Q_memset( &svs.stats, 0, sizeof( svs.stats ) );
		svs.stats.minusers = svs.stats.maxusers = players;
		return;
	}

	if ( realtime < svs.next_sampletime )
		return;

	svs.stats.num_samples++;
	svs.next_sampletime = realtime + 60.0;

	SV_CountPlayers( &players );

	occup = (float)players * 100.0 / (float)svs.maxclients;
	svs.stats.cumulative_occupancy += occup;

	if ( svs.stats.num_samples >= 1 )
		svs.stats.occupancy = svs.stats.cumulative_occupancy / (float)svs.stats.num_samples;

	if ( players < svs.stats.minusers )
		svs.stats.minusers = players;
	else if ( players > svs.stats.maxusers )
		svs.stats.maxusers = players;

	if ( players >= svs.maxclients - 1 )
		svs.stats.at_capacity++;
	if ( players <= 1 )
		svs.stats.at_empty++;

	if ( svs.stats.num_samples >= 1 )
	{
		svs.stats.capacity_percent = (float)svs.stats.at_capacity * 100.0 / (float)svs.stats.num_samples;
		svs.stats.empty_percent = (float)svs.stats.at_empty * 100.0 / (float)svs.stats.num_samples;
	}

	c = 0;
	v = 0.0f;
	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active || cl->fakeclient )
			continue;
		c++;
		v += cl->latency;
	}

	if ( !c )
		v = 0.0f;
	else
		v = v / (float)c;

	svs.stats.cumulative_latency += v;
	if ( svs.stats.num_samples >= 1 )
		svs.stats.average_latency = svs.stats.cumulative_latency / (float)svs.stats.num_samples;
	if ( svs.stats.num_sessions >= 1 )
		svs.stats.average_session_len = svs.stats.cumulative_sessiontime / (float)svs.stats.num_sessions;
}

/*
================
SV_DeallocateDynamicData

================
*/
void SV_DeallocateDynamicData( void )
{
	if ( g_moved_edict )
		Mem_Free( g_moved_edict );
	if ( g_moved_from )
		Mem_Free( g_moved_from );
	g_moved_edict = NULL;
	g_moved_from = NULL;
}

/*
================
SV_ReallocateDynamicData

================
*/
void SV_ReallocateDynamicData( void )
{
	int		nSize;

	nSize = sv.max_edicts;
	if ( !nSize )
	{
		Con_DPrintf( "SV_ReallocateDynamicData with sv.max_edicts == 0" );
		return;
	}

	if ( g_moved_edict )
		Con_Printf( "Reallocate on moved_edict\n" );
	g_moved_edict = (edict_t **)Mem_ZeroMalloc( nSize * sizeof( edict_t * ) );

	if ( g_moved_from )
		Con_Printf( "Reallocate on moved_from\n" );
	g_moved_from = (vec3_t *)Mem_ZeroMalloc( nSize * sizeof( vec3_t ) );
}

/*
================
SV_AllocClientFrames

================
*/
void SV_AllocClientFrames( void )
{
	int			i;
	client_t	*cl;

	for ( i = 0, cl = svs.clients; i < svs.maxclientslimit; i++, cl++ )
	{
		if ( cl->frames )
			Con_DPrintf( "Allocating over frame pointer?\n" );
		cl->frames = (client_frame_t *)Mem_ZeroMalloc( SV_UPDATE_BACKUP * sizeof( client_frame_t ) );
	}
}

/*
================
SV_IsPlayerIndex

================
*/
qboolean SV_IsPlayerIndex( int index )
{
	if ( index >= 1 && index <= svs.maxclients )
		return true;
	return false;
}

/*
================
SV_ClearPacketEntities

================
*/
void SV_ClearPacketEntities( client_frame_t *frame )
{
	packet_entities_t	*pack;

	if ( !frame )
		return;

	pack = &frame->entities;
	if ( pack->entities )
		Mem_Free( pack->entities );
	pack->entities = NULL;
	pack->num_entities = 0;
}

/*
================
SV_AllocPacketEntities

================
*/
void SV_AllocPacketEntities( client_frame_t *frame, int numents )
{
	packet_entities_t	*pack;
	int		allocatedslots;

	if ( !frame )
		return;

	pack = &frame->entities;
	if ( pack->entities )
		SV_ClearPacketEntities( frame );

	allocatedslots = max( numents, 1 );
	pack->entities = (entity_state_t *)Mem_ZeroMalloc( allocatedslots * sizeof( entity_state_t ) );
	pack->num_entities = numents;
}

/*
================
SV_ClearFrames

================
*/
void SV_ClearFrames( client_frame_t **frames )
{
	int		i;
	client_frame_t	*pframe;

	if ( !*frames )
		return;

	for ( i = 0, pframe = *frames; i < SV_UPDATE_BACKUP; i++, pframe++ )
	{
		SV_ClearPacketEntities( pframe );
		pframe->senttime = 0;
		pframe->ping_time = -1;
	}

	pframe = *frames;
	Mem_Free( pframe );
	*frames = NULL;
}

/*
================
SV_Serverinfo_f

  Examine or change the serverinfo string
================
*/
void SV_Serverinfo_f( void )
{
	cvar_t	*var;

	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "Server info settings:\n" );
		Info_Print( Info_Serverinfo() );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: serverinfo [ <key> <value> ]\n" );
		return;
	}

	if ( Cmd_Argv( 1 )[0] == '*' )
	{
		Con_Printf( "Star variables cannot be changed.\n" );
		return;
	}

	Info_SetValueForKey( Info_Serverinfo(), Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_SERVERINFO_STRING );

	// if this is a cvar, change it too
	var = Cvar_FindVar( Cmd_Argv( 1 ) );
	if ( var )
	{
		Z_Free( var->string );	// free the old value string
		var->string = CopyString( Cmd_Argv( 2 ) );
		var->value = Q_atof( var->string );
	}

	SV_BroadcastCommand( "fullserverinfo \"%s\"\n", Info_Serverinfo() );
}

/*
================
SV_Localinfo_f

  Examine or change the localinfo string
================
*/
void SV_Localinfo_f( void )
{
	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "Local info settings:\n" );
		Info_Print( localinfo );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "usage: localinfo [ <key> <value> ]\n" );
		return;
	}

	if ( Cmd_Argv( 1 )[0] == '*' )
	{
		Con_Printf( "Star variables cannot be changed.\n" );
		return;
	}

	Info_SetValueForKey( localinfo, Cmd_Argv( 1 ), Cmd_Argv( 2 ), MAX_LOCALINFO );
}

/*
===========
SV_User_f

Examine a users info strings
===========
*/
void SV_User_f( void )
{
	int		uid;
	int		i;
	client_t	*cl;

	if ( !sv.active )
	{
		Con_Printf( "Can't 'user', not running a server\n" );
		return;
	}

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage: user <username / userid>\n" );
		return;
	}

	uid = Q_atoi( Cmd_Argv( 1 ) );

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active && !cl->spawned && !cl->connected )
			continue;
		if ( cl->fakeclient )
			continue;
		if ( !cl->name[0] )
			continue;

		if ( cl->userid == uid || !Q_strcmp( cl->name, Cmd_Argv( 1 ) ) )
		{
			Info_Print( cl->userinfo );
			return;
		}
	}

	Con_Printf( "User not in server.\n" );
}

/*
================
SV_Users_f

Displays a list of users
================
*/
void SV_Users_f( void )
{
	int		i;
	int		c;
	client_t	*cl;

	if ( !sv.active )
	{
		Con_Printf( "Can't 'users', not running a server\n" );
		return;
	}

	c = 0;
	Con_Printf( "userid : uniqueid : name\n" );
	Con_Printf( "------ : ---------: ----\n" );

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active && !cl->spawned && !cl->connected )
			continue;
		if ( cl->fakeclient )
			continue;

		if ( cl->name[0] )
		{
			Con_Printf( "%6i : %s : %s\n", cl->userid, SV_GetClientIDString( cl ), cl->name );
			c++;
		}
	}

	Con_Printf( "%i users\n", c );
}

/*
================
SV_CountPlayers

Counts the number of active players on the server
================
*/
void SV_CountPlayers( int *clients )
{
	int		i;
	client_t	*cl;

	*clients = 0;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->active || cl->spawned || cl->connected )
			(*clients)++;
	}
}

/*
================
SV_CountProxies

Counts the number of HLTV proxies on the server
================
*/
void SV_CountProxies( int *proxies )
{
	int		i;
	client_t	*cl;

	*proxies = 0;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->active || cl->spawned || cl->connected )
		{
			if ( cl->proxy )
				(*proxies)++;
		}
	}
}

/*
================
SV_FindModelNumbers

================
*/
void SV_FindModelNumbers( void )
{
	int		i;

	sv_playermodel = -1;

	for ( i = 0; i < MAX_MODELS; i++ )
	{
		if ( !sv.model_precache[i] )
			break;

		if ( !Q_strcasecmp( sv.model_precache[i], "models/player.mdl" ) )
			sv_playermodel = i;
	}
}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle( const vec_t *org, const vec_t *dir, int color, int count )
{
	int		i, v;

	if ( sv.datagram.cursize > MAX_DATAGRAM - 16 )
		return;

	MSG_WriteByte( &sv.datagram, svc_particle );
	MSG_WriteCoord( &sv.datagram, org[0] );
	MSG_WriteCoord( &sv.datagram, org[1] );
	MSG_WriteCoord( &sv.datagram, org[2] );
	for ( i = 0; i < 3; i++ )
	{
		v = dir[i] * 16;
		if ( v > 127 )
			v = 127;
		else if ( v < -128 )
			v = -128;
		MSG_WriteChar( &sv.datagram, v );
	}
	MSG_WriteByte( &sv.datagram, count );
	MSG_WriteByte( &sv.datagram, color );
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Pitch should be PITCH_NORM (100) for no pitch shift. Values over 100 (up to 255)
shift pitch higher, values lower than 100 lower the pitch.
==================
*/
void SV_StartSound( int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch )
{
	int		i;
	qboolean	sendPAS;
	vec3_t	origin;

	for ( i = 0; i < 3; i++ )
		origin[i] = entity->v.origin[i] + ( entity->v.mins[i] + entity->v.maxs[i] ) * 0.5;

	if ( SV_BuildSoundMsg( entity, channel, sample, volume, attenuation, fFlags, pitch, origin, &sv.multicast ) )
	{
		int		flags = 0;

		if ( channel == CHAN_STATIC || ( fFlags & SND_STOP ) )
			sendPAS = FALSE;
		else
			sendPAS = TRUE;

		if ( recipients == 1 )
			flags = MSG_FL_ONE;

		if ( sendPAS )
		{
			SV_Multicast( entity, origin, flags | MSG_FL_PAS, FALSE );
		}
		else
		{
			if ( Host_IsSinglePlayerGame() )
				SV_Multicast( entity, origin, flags | MSG_FL_BROADCAST, FALSE );
			else
				SV_Multicast( entity, origin, flags | MSG_FL_BROADCAST, TRUE );
		}
	}
}

/*
==================
SV_BuildSoundMsg

==================
*/
qboolean SV_BuildSoundMsg( edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch, const float *origin, sizebuf_t *buffer )
{
	int		sound_num;
	int		field_mask;
	int		ent;

	if ( volume < 0 || volume > 255 )
		Sys_Error( "SV_StartSound: volume = %i", volume );

	if ( attenuation < 0 || attenuation > 4 )
		Sys_Error( "SV_StartSound: attenuation = %f", attenuation );

	if ( channel < 0 || channel > 7 )
		Sys_Error( "SV_StartSound: channel = %i", channel );

	if ( pitch < 0 || pitch > 255 )
		Sys_Error( "SV_StartSound: pitch = %i", pitch );

	// find precache number for sound
	if ( sample && sample[0] == '!' )
	{
		fFlags |= SND_SENTENCE;
		sound_num = Q_atoi( sample + 1 );
		if ( sound_num >= CVOXFILESENTENCEMAX )
		{
			Con_Printf( "invalid sentence number: %s", sample + 1 );
			return FALSE;
		}
	}
	else if ( sample && sample[0] == '#' )
	{
		fFlags |= SND_SENTENCE;
		sound_num = Q_atoi( sample + 1 );
		sound_num += CVOXFILESENTENCEMAX;
	}
	else
	{
		sound_num = SV_LookupSoundIndex( sample );
		if ( !sound_num || !sv.sound_precache[sound_num] )
		{
			Con_Printf( "SV_StartSound: %s not precached (%d)\n", sample, sound_num );
			return FALSE;
		}
	}

	ent = NUM_FOR_EDICT( entity );

	field_mask = fFlags;

	if ( volume != DEFAULT_SOUND_PACKET_VOLUME )
		field_mask |= SND_VOLUME;
	if ( attenuation != DEFAULT_SOUND_PACKET_ATTENUATION )
		field_mask |= SND_ATTENUATION;
	if ( pitch != DEFAULT_SOUND_PACKET_PITCH )
		field_mask |= SND_PITCH;
	if ( sound_num > 255 )
		field_mask |= SND_LARGE_INDEX;

	// directed messages go only to the entity the are targeted on
	MSG_WriteByte( buffer, svc_sound );
	MSG_StartBitWriting( buffer );
	MSG_WriteBits( field_mask, 9 );
	if ( field_mask & SND_VOLUME )
		MSG_WriteBits( volume, 8 );
	if ( field_mask & SND_ATTENUATION )
		MSG_WriteBits( attenuation * 64.0, 8 );
	MSG_WriteBits( channel, 3 );
	MSG_WriteBits( ent, 11 );
	if ( sound_num > 255 )
		MSG_WriteBits( sound_num, 16 );
	else
		MSG_WriteBits( sound_num, 8 );
	MSG_WriteBitVec3Coord( origin );
	if ( field_mask & SND_PITCH )
		MSG_WriteBits( pitch, 8 );
	MSG_EndBitWriting( buffer );

	return TRUE;
}

static int	hashstrings_collisions;

/*
==================
SV_HashString

==================
*/
int SV_HashString( const char *string, int iBounds )
{
	unsigned int	hash;

	hash = 0;
	for ( ; *string; string++ )
	{
		hash = tolower( *string ) + hash * 2;
	}

	return hash % iBounds;
}

/*
==================
SV_LookupSoundIndex

==================
*/
int SV_LookupSoundIndex( const char *sample )
{
	int		index;
	int		starting_index;

	index = SV_HashString( sample, MAX_SOUNDS_HASHLOOKUP_SIZE );
	starting_index = index;

	if ( !sv.sound_precache_hashedlookup_built )
	{
		if ( sv.state != ss_loading )
		{
			SV_BuildHashedSoundLookupTable();
		}
		else
		{
			for ( index = 1; index < MAX_SOUNDS && sv.sound_precache[index]; index++ )
			{
				if ( !Q_strcasecmp( sample, sv.sound_precache[index] ) )
					return index;
			}
			return 0;
		}
	}

	while ( sv.sound_precache_hashedlookup[index] )
	{
		if ( !Q_strcasecmp( sample, sv.sound_precache[sv.sound_precache_hashedlookup[index]] ) )
			return sv.sound_precache_hashedlookup[index];

		index++;
		if ( index >= MAX_SOUNDS_HASHLOOKUP_SIZE )
			index = 0;
		if ( index == starting_index )
			break;
	}

	return 0;
}

/*
==================
SV_BuildHashedSoundLookupTable

==================
*/
void SV_BuildHashedSoundLookupTable( void )
{
	int		sound_num;

	Q_memset( sv.sound_precache_hashedlookup, 0, sizeof( sv.sound_precache_hashedlookup ) );

	for ( sound_num = 0; sound_num < MAX_SOUNDS && sv.sound_precache[sound_num]; sound_num++ )
		SV_AddSampleToHashedLookupTable( sv.sound_precache[sound_num], sound_num );

	sv.sound_precache_hashedlookup_built = TRUE;
}

/*
==================
SV_AddSampleToHashedLookupTable

==================
*/
void SV_AddSampleToHashedLookupTable( const char *pszSample, int iSampleIndex )
{
	int		index;
	int		starting_index;

	index = SV_HashString( pszSample, MAX_SOUNDS_HASHLOOKUP_SIZE );
	starting_index = index;

	while ( sv.sound_precache_hashedlookup[index] )
	{
		index++;
		hashstrings_collisions++;

		if ( index >= MAX_SOUNDS_HASHLOOKUP_SIZE )
			index = 0;
		if ( index == starting_index )
			Sys_Error( "SV_AddSampleToHashedLookupTable: NO FREE SLOTS IN SOUND LOOKUP TABLE" );
	}

	sv.sound_precache_hashedlookup[index] = iSampleIndex;
}

/*
==================
SV_ValidClientMulticast

==================
*/
qboolean SV_ValidClientMulticast( client_t *client, int soundLeaf, int to )
{
	byte	*mask;
	int		destination;

	mask = NULL;
	destination = to & ~MSG_FL_ONE;

	if ( !Host_IsSinglePlayerGame() && !client->proxy )
	{
		switch ( destination )
		{
		case MSG_FL_BROADCAST:
			break;

		case MSG_FL_PAS:
			mask = CM_LeafPAS( soundLeaf );
			break;

		case MSG_FL_PVS:
			mask = CM_LeafPVS( soundLeaf );
			break;

		default:
			Con_Printf( "MULTICAST: Error %d!\n", to );
			return FALSE;
		}

		if ( mask )
		{
			int		bitNumber;

			bitNumber = SV_PointLeafnum( client->edict->v.origin ) - 1;
			if ( !( mask[bitNumber >> 3] & ( 1 << ( bitNumber & 7 ) ) ) )
				return FALSE;
		}
	}

	return TRUE;
}

/*
==================
SV_Multicast

Sends the contents of sv.multicast to a subset of the clients,
then clears sv.multicast.

MSG_FL_BROADCAST	all clients
MSG_FL_PVS			clients in the potentially visible set of origin
MSG_FL_PAS			clients in the potentially audible set of origin
MSG_FL_ONE			not to the client of the entity
==================
*/
void SV_Multicast( edict_t *ent, vec_t *origin, int to, qboolean reliable )
{
	client_t	*client;
	int			leafnum;
	int			j;
	sizebuf_t	*pBuffer;
	client_t	*save;

	save = host_client;
	leafnum = SV_PointLeafnum( origin );

	if ( ent && !( host_client && host_client->edict == ent ) )
	{
		for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
		{
			if ( client->edict == ent )
			{
				host_client = client;
				break;
			}
		}
	}

	// send the data to all relevent clients
	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( !client->active )
			continue;

		if ( ( to & MSG_FL_ONE ) && client == host_client )
			continue;

		if ( ent && ent->v.groupinfo && client->edict->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( ent->v.groupinfo & client->edict->v.groupinfo ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( ent->v.groupinfo & client->edict->v.groupinfo )
					continue;
			}
		}

		if ( SV_ValidClientMulticast( client, leafnum, to ) )
		{
			if ( reliable )
				pBuffer = &client->netchan.message;
			else
				pBuffer = &client->datagram;

			if ( pBuffer->maxsize - pBuffer->cursize > sv.multicast.cursize )
				SZ_Write( pBuffer, sv.multicast.data, sv.multicast.cursize );
		}
		else
		{
			gPacketSuppressed += sv.multicast.cursize;
		}
	}

	SZ_Clear( &sv.multicast );
	host_client = save;
}

/*
==================
SV_WriteMovevarsToClient

==================
*/
void SV_WriteMovevarsToClient( sizebuf_t *message )
{
	MSG_WriteByte( message, svc_newmovevars );
	MSG_WriteFloat( message, movevars.gravity );
	MSG_WriteFloat( message, movevars.stopspeed );
	MSG_WriteFloat( message, movevars.maxspeed );
	MSG_WriteFloat( message, movevars.spectatormaxspeed );
	MSG_WriteFloat( message, movevars.accelerate );
	MSG_WriteFloat( message, movevars.airaccelerate );
	MSG_WriteFloat( message, movevars.wateraccelerate );
	MSG_WriteFloat( message, movevars.friction );
	MSG_WriteFloat( message, movevars.edgefriction );
	MSG_WriteFloat( message, movevars.waterfriction );
	MSG_WriteFloat( message, movevars.entgravity );
	MSG_WriteFloat( message, movevars.bounce );
	MSG_WriteFloat( message, movevars.stepsize );
	MSG_WriteFloat( message, movevars.maxvelocity );
	MSG_WriteFloat( message, movevars.zmax );
	MSG_WriteFloat( message, movevars.waveHeight );
	MSG_WriteByte( message, movevars.footsteps != 0 );
	MSG_WriteFloat( message, movevars.rollangle );
	MSG_WriteFloat( message, movevars.rollspeed );
	MSG_WriteFloat( message, movevars.skycolor_r );
	MSG_WriteFloat( message, movevars.skycolor_g );
	MSG_WriteFloat( message, movevars.skycolor_b );
	MSG_WriteFloat( message, movevars.skyvec_x );
	MSG_WriteFloat( message, movevars.skyvec_y );
	MSG_WriteFloat( message, movevars.skyvec_z );
	MSG_WriteString( message, movevars.skyName );
}

/*
==================
SV_WriteDeltaDescriptionsToClient

==================
*/
void SV_WriteDeltaDescriptionsToClient( sizebuf_t *msg )
{
	int		c;
	int		i;
	delta_info_t	*p;
	delta_description_t	nulldesc;
	delta_description_t	*from, *to;

	Q_memset( &nulldesc, 0, sizeof( nulldesc ) );

	p = g_sv_delta;
	while ( p )
	{
		MSG_WriteByte( msg, svc_deltadescription );
		MSG_WriteString( msg, p->name );

		from = &nulldesc;
		c = p->delta->fieldCount;

		MSG_StartBitWriting( msg );
		MSG_WriteBits( c, 16 );

		for ( i = 0; i < c; i++ )
		{
			to = &p->delta->pdd[i];
			DELTA_WriteDelta( (byte *)from, (byte *)to, TRUE, &g_MetaDelta, NULL );
		}

		MSG_EndBitWriting( msg );

		p = p->next;
	}
}

/*
==================
SV_SetMoveVars

==================
*/
void SV_SetMoveVars( void )
{
	movevars.gravity			= sv_gravity.value;
	movevars.stopspeed			= sv_stopspeed.value;
	movevars.maxspeed			= sv_maxspeed.value;
	movevars.spectatormaxspeed	= sv_spectatormaxspeed.value;
	movevars.accelerate			= sv_accelerate.value;
	movevars.airaccelerate		= sv_airaccelerate.value;
	movevars.wateraccelerate	= sv_wateraccelerate.value;
	movevars.friction			= sv_friction.value;
	movevars.edgefriction		= sv_edgefriction.value;
	movevars.waterfriction		= sv_waterfriction.value;
	movevars.entgravity			= 1.0f;
	movevars.bounce				= sv_bounce.value;
	movevars.stepsize			= sv_stepsize.value;
	movevars.maxvelocity		= sv_maxvelocity.value;
	movevars.zmax				= sv_zmax.value;
	movevars.waveHeight			= sv_wateramp.value;
	movevars.footsteps			= sv_footsteps.value;
	movevars.rollangle			= sv_rollangle.value;
	movevars.rollspeed			= sv_rollspeed.value;
	movevars.skycolor_r			= sv_skycolor_r.value;
	movevars.skycolor_g			= sv_skycolor_g.value;
	movevars.skycolor_b			= sv_skycolor_b.value;
	movevars.skyvec_x			= sv_skyvec_x.value;
	movevars.skyvec_y			= sv_skyvec_y.value;
	movevars.skyvec_z			= sv_skyvec_z.value;

	Q_strncpy( movevars.skyName, sv_skyname.string, sizeof( movevars.skyName ) - 1 );
	movevars.skyName[sizeof( movevars.skyName ) - 1] = 0;
}

/*
==================
SV_QueryMovevarsChanged

==================
*/
void SV_QueryMovevarsChanged( void )
{
	int		i;
	client_t	*cl;

	if ( sv_maxspeed.value == movevars.maxspeed &&
		sv_gravity.value == movevars.gravity &&
		sv_stopspeed.value == movevars.stopspeed &&
		sv_spectatormaxspeed.value == movevars.spectatormaxspeed &&
		sv_accelerate.value == movevars.accelerate &&
		sv_airaccelerate.value == movevars.airaccelerate &&
		sv_wateraccelerate.value == movevars.wateraccelerate &&
		sv_friction.value == movevars.friction &&
		sv_edgefriction.value == movevars.edgefriction &&
		sv_waterfriction.value == movevars.waterfriction &&
		movevars.entgravity == 1.0 &&
		sv_bounce.value == movevars.bounce &&
		sv_stepsize.value == movevars.stepsize &&
		sv_maxvelocity.value == movevars.maxvelocity &&
		sv_zmax.value == movevars.zmax &&
		sv_wateramp.value == movevars.waveHeight &&
		movevars.footsteps == sv_footsteps.value &&
		sv_rollangle.value == movevars.rollangle &&
		sv_rollspeed.value == movevars.rollspeed &&
		sv_skycolor_r.value == movevars.skycolor_r &&
		sv_skycolor_g.value == movevars.skycolor_g &&
		sv_skycolor_b.value == movevars.skycolor_b &&
		sv_skyvec_x.value == movevars.skyvec_x &&
		sv_skyvec_y.value == movevars.skyvec_y &&
		sv_skyvec_z.value == movevars.skyvec_z &&
		!Q_strcmp( sv_skyname.string, movevars.skyName ) )
		return;

	SV_SetMoveVars();

	// Tell each client
	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->fakeclient )
			continue;

		if ( cl->active || cl->spawned || cl->connected )
			SV_WriteMovevarsToClient( &cl->netchan.message );
	}
}

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo( sizebuf_t *msg, client_t *client )
{
	char	message[2048];
	int		ffirstsent;
	int		playernum;
	char	*mapcyclelist;
	int		len;

	ffirstsent = 0;

	if ( developer.value || svs.maxclients > 1 )
	{
		MSG_WriteByte( msg, svc_print );
		_snprintf( message, sizeof( message ), "%c\nBUILD %d SERVER (%i CRC)\nServer # %i\n", 2, build_number(), 0, svs.spawncount );
		MSG_WriteString( msg, message );
	}

	MSG_WriteByte( msg, svc_serverinfo );
	MSG_WriteLong( msg, PROTOCOL_VERSION );
	MSG_WriteLong( msg, svs.spawncount );

	playernum = NUM_FOR_EDICT( client->edict ) - 1;

	{
		int		mungebuffer;

		mungebuffer = sv.worldmapCRC;
		COM_Munge3( (unsigned char *)&mungebuffer, sizeof( mungebuffer ), ( 0xFF - playernum ) & 0xFF );
		MSG_WriteLong( msg, mungebuffer );
	}

	MSG_WriteBuf( msg, sizeof( sv.clientdllmd5 ), sv.clientdllmd5 );
	MSG_WriteByte( msg, svs.maxclients );
	MSG_WriteByte( msg, playernum );

	if ( !coop.value && deathmatch.value )
		MSG_WriteByte( msg, 1 );
	else
		MSG_WriteByte( msg, 0 );

	COM_FileBase( com_gamedir, message );
	MSG_WriteString( msg, message );
	MSG_WriteString( msg, Cvar_VariableString( "hostname" ) );
	MSG_WriteString( msg, sv.modelname );

	mapcyclelist = (char *)COM_LoadFileForMe( mapcyclefile.string, &len );
	if ( mapcyclelist && len )
	{
		MSG_WriteString( msg, mapcyclelist );
		COM_FreeFile( mapcyclelist );
	}
	else
	{
		MSG_WriteString( msg, "mapcycle failure" );
	}

	MSG_WriteByte( msg, 0 );

	MSG_WriteByte( msg, svc_sendextrainfo );
	MSG_WriteString( msg, com_clientfallback );
	MSG_WriteByte( msg, allow_cheats );

	SV_WriteDeltaDescriptionsToClient( msg );
	SV_SetMoveVars();
	SV_WriteMovevarsToClient( msg );

	// send music
	MSG_WriteByte( msg, svc_cdtrack );
	MSG_WriteByte( msg, gGlobalVariables.cdAudioTrack );
	MSG_WriteByte( msg, gGlobalVariables.cdAudioTrack );

	// set view
	MSG_WriteByte( msg, svc_setview );
	MSG_WriteShort( msg, NUM_FOR_EDICT( client->edict ) );

	client->spawned = FALSE;
	client->connected = TRUE;
	client->fully_connected = FALSE;
}

/*
================
SV_SendResources

================
*/
void SV_SendResources( sizebuf_t *msg )
{
	int		i;
	byte	nullbuffer[32];

	Q_memset( nullbuffer, 0, sizeof( nullbuffer ) );

	MSG_WriteByte( msg, svc_resourcerequest );
	MSG_WriteLong( msg, svs.spawncount );
	MSG_WriteLong( msg, 0 );

	if ( sv_downloadurl.string && sv_downloadurl.string[0] && strlen( sv_downloadurl.string ) < 128 )
	{
		MSG_WriteByte( msg, svc_resourcelocation );
		MSG_WriteString( msg, sv_downloadurl.string );
	}

	MSG_WriteByte( msg, svc_resourcelist );
	MSG_StartBitWriting( msg );
	MSG_WriteBits( sv.num_resources, 12 );

	for ( i = 0; i < sv.num_resources; i++ )
	{
		MSG_WriteBits( sv.resourcelist[i].type, 4 );
		MSG_WriteBitString( sv.resourcelist[i].szFileName );
		MSG_WriteBits( sv.resourcelist[i].nIndex, 12 );
		MSG_WriteBits( sv.resourcelist[i].nDownloadSize, 24 );
		MSG_WriteBits( sv.resourcelist[i].ucFlags & ( RES_WASMISSING | RES_FATALIFMISSING ), 3 );
		if ( sv.resourcelist[i].ucFlags & RES_CUSTOM )
			MSG_WriteBitData( sv.resourcelist[i].rgucMD5_hash, sizeof( sv.resourcelist[i].rgucMD5_hash ) );

		if ( Q_memcmp( sv.resourcelist[i].rguc_reserved, nullbuffer, sizeof( nullbuffer ) ) )
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBitData( sv.resourcelist[i].rguc_reserved, sizeof( sv.resourcelist[i].rguc_reserved ) );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
		}
	}

	SV_SendConsistencyList( msg );
	MSG_EndBitWriting( msg );
}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage( client_t *client, sizebuf_t *msg )
{
	int		bits;
	int		i;
	edict_t	*ent;
	client_frame_t	*frame;
	clientdata_t	*from, *to;
	clientdata_t	baseline;
	qboolean	force;

	force = FALSE;
	Q_memset( &baseline, 0, sizeof( baseline ) );

	ent = client->edict;
	bits = 0;

	frame = &client->frames[client->netchan.outgoing_sequence & SV_UPDATE_MASK];
	frame->senttime = realtime;
	frame->ping_time = -1;

	if ( client->chokecount )
	{
		MSG_WriteByte( msg, svc_choke );
		client->chokecount = 0;
	}

	// send a fixangle over the reliable channel to make sure it gets there
	// Never send a roll angle, because savegames can catch the server
	// in a state where it is expecting the client to correct the angle
	// and it won't happen if the game was just loaded, so you wind up
	// with a permanent head tilt
	if ( ent->v.fixangle )
	{
		if ( ent->v.fixangle == 2 )
		{
			MSG_WriteByte( msg, svc_addangle );
			MSG_WriteHiresAngle( msg, ent->v.avelocity[1] );
			ent->v.avelocity[1] = 0;
		}
		else
		{
			MSG_WriteByte( msg, svc_setangle );
			for ( i = 0; i < 3; i++ )
				MSG_WriteHiresAngle( msg, ent->v.angles[i] );
		}
		ent->v.fixangle = 0;
	}

	Q_memset( &frame->clientdata, 0, sizeof( frame->clientdata ) );

	// Update client data from the game dll
	gEntityInterface.pfnUpdateClientData( ent, host_client->lw != 0, &frame->clientdata );

	MSG_WriteByte( msg, svc_clientdata );

	if ( client->proxy )
		return;

	MSG_StartBitWriting( msg );

	if ( host_client->delta_sequence == -1 )
		from = &baseline;
	else
		from = &host_client->frames[host_client->delta_sequence & SV_UPDATE_MASK].clientdata;

	to = &frame->clientdata;
	force = TRUE;

	if ( host_client->delta_sequence == -1 )
	{
		MSG_WriteBits( 0, 1 );
	}
	else
	{
		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( host_client->delta_sequence, 8 );
	}

	DELTA_WriteDelta( (byte *)from, (byte *)to, force, g_pclientdelta, NULL );

	if ( host_client->lw )
	{
		weapon_data_t	*fdata, *tdata;
		weapon_data_t	wbaseline;

		Q_memset( &wbaseline, 0, sizeof( wbaseline ) );

		if ( gEntityInterface.pfnGetWeaponData( host_client->edict, frame->weapondata ) )
		{
			for ( i = 0; i < 64; i++ )
			{
				if ( host_client->delta_sequence == -1 )
					fdata = &wbaseline;
				else
					fdata = &host_client->frames[host_client->delta_sequence & SV_UPDATE_MASK].weapondata[i];

				tdata = &frame->weapondata[i];

				if ( DELTA_CheckDelta( (byte *)fdata, (byte *)tdata, g_pweapondelta ) )
				{
					MSG_WriteBits( 1, 1 );
					MSG_WriteBits( i, 6 );
					DELTA_WriteDelta( (byte *)fdata, (byte *)tdata, TRUE, g_pweapondelta, NULL );
				}
			}
		}
	}

	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( msg );
}

/*
==================
SV_WriteSpawn

==================
*/
void SV_WriteSpawn( sizebuf_t *msg )
{
	int		i;
	char	name[256];
	SAVERESTOREDATA	currentLevelData;
	client_t	*client;

	// run the entrance script
	if ( sv.loadgame )
	{
		// loaded games are fully inited allready
		// if this is the last client on a loadgame, restore the players
		if ( host_client->proxy )
		{
			Con_Printf( "ERROR! Spectator mode doesn't work with saved game.\n" );
			return;
		}

		sv.paused = FALSE;
	}
	else
	{
		sv.state = ss_loading;

		ReleaseEntityDLLFields( sv_player );
		Q_memset( &sv_player->v, 0, sizeof( sv_player->v ) );
		InitEntityDLLFields( sv_player );

		sv_player->v.colormap = NUM_FOR_EDICT( sv_player );
		sv_player->v.netname = host_client->name - pr_strings;
		if ( host_client->proxy )
			sv_player->v.flags |= FL_PROXY;

		// call the spawn function
		gGlobalVariables.time = sv.time;
		gEntityInterface.pfnClientPutInServer( sv_player );

		sv.state = ss_active;
	}

	// send all current names, colors, and frag counts
	SZ_Clear( &host_client->netchan.message );
	SZ_Clear( &host_client->datagram );

	// send time of update
	MSG_WriteByte( msg, svc_time );
	MSG_WriteFloat( msg, sv.time );

	host_client->sendinfo = TRUE;

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client != host_client && !client->active && !client->connected && !client->spawned )
			continue;

		SV_FullClientUpdate( client, msg );
	}

	// send all current light styles
	for ( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		MSG_WriteByte( msg, svc_lightstyle );
		MSG_WriteByte( msg, (char)i );
		MSG_WriteString( msg, sv.lightstyles[i] );
	}

	if ( !host_client->proxy )
	{
		// set view
		MSG_WriteByte( msg, svc_setangle );
		for ( i = 0; i < 2; i++ )
			MSG_WriteHiresAngle( msg, sv_player->v.v_angle[i] );
		MSG_WriteHiresAngle( msg, 0 );

		SV_WriteClientdataToMessage( host_client, msg );

		if ( sv.loadgame )
		{
			Q_memset( &currentLevelData, 0, sizeof( currentLevelData ) );
			gGlobalVariables.pSaveData = &currentLevelData;
			gEntityInterface.pfnParmsChangeLevel();

			MSG_WriteByte( msg, svc_restore );
			_snprintf( name, sizeof( name ), "%s%s.HL2", Host_SaveGameDirectory(), sv.name );
			COM_FixSlashes( name );
			MSG_WriteString( msg, name );
			MSG_WriteByte( msg, currentLevelData.connectionCount );

			for ( i = 0; i < currentLevelData.connectionCount; i++ )
				MSG_WriteString( msg, currentLevelData.levelList[i].mapName );

			sv.loadgame = FALSE;
			gGlobalVariables.pSaveData = NULL;
		}
	}

	MSG_WriteByte( msg, svc_signonnum );
	MSG_WriteByte( msg, 1 );

	host_client->active = TRUE;
	host_client->connecttime = 0;
	host_client->ignorecmdtime = 0;
	host_client->cmdtime = 0;
	host_client->spawned = TRUE;
	host_client->connected = TRUE;
	host_client->fully_connected = FALSE;

	NotifyDedicatedServerUI( "UpdatePlayers" );
}

/*
==================
SV_SendUserReg

==================
*/
void SV_SendUserReg( sizebuf_t *msg )
{
	if ( sv_gpNewUserMsgs )
	{
		UserMsg	*pMsg;

		pMsg = sv_gpNewUserMsgs;
		while ( pMsg )
		{
			MSG_WriteByte( msg, svc_newusermsg );
			MSG_WriteByte( msg, pMsg->iMsg );
			MSG_WriteByte( msg, pMsg->iSize );
			MSG_WriteLong( msg, *(int *)&pMsg->szName[0] );
			MSG_WriteLong( msg, *(int *)&pMsg->szName[4] );
			MSG_WriteLong( msg, *(int *)&pMsg->szName[8] );
			MSG_WriteLong( msg, *(int *)&pMsg->szName[12] );
			pMsg = pMsg->next;
		}
	}
}

/*
==================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
==================
*/
void SV_New_f( void )
{
	int		i;
	client_t	*client;
	unsigned char	data[65536];
	sizebuf_t	msg;
	edict_t	*ent;
	char	szRejectReason[128];
	char	szAddress[256];
	char	szName[64];

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "New Connection";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	// Not valid on the client
	if ( cmd_source == src_command )
		return;

	if ( host_client->spawned && !host_client->active )
		return;

	ent = host_client->edict;

	host_client->connected = TRUE;
	host_client->connection_started = realtime;
	host_client->m_sendrescount = 0;

	SZ_Clear( &host_client->netchan.message );
	SZ_Clear( &host_client->datagram );

	Netchan_Clear( &host_client->netchan );

	SV_SendServerinfo( &msg, host_client );

	if ( sv_gpUserMsgs )
	{
		UserMsg	*pTemp;

		pTemp = sv_gpNewUserMsgs;
		sv_gpNewUserMsgs = sv_gpUserMsgs;
		SV_SendUserReg( &msg );
		sv_gpNewUserMsgs = pTemp;
	}

	host_client->hasusrmsgs = TRUE;

	// If the client was connected, tell the game dll to disconnect him/her.
	if ( ( host_client->active || host_client->spawned ) && host_client->edict )
		gEntityInterface.pfnClientDisconnect( host_client->edict );

	_snprintf( szName, sizeof( szName ), "%s", host_client->name );
	_snprintf( szAddress, sizeof( szAddress ), "%s", NET_AdrToString( host_client->netchan.remote_address ) );
	_snprintf( szRejectReason, sizeof( szRejectReason ), "Connection rejected by game\n" );

	// Allow the game dll to reject this client.
	if ( !gEntityInterface.pfnClientConnect( ent, szName, szAddress, szRejectReason ) )
	{
		// Reject the connection and drop the client.
		MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
		MSG_WriteString( &host_client->netchan.message, va( "echo %s\n", szRejectReason ) );
		SV_DropClient( host_client, FALSE, "Server refused connection because:  %s", szRejectReason );
		return;
	}

	MSG_WriteByte( &msg, svc_stufftext );
	MSG_WriteString( &msg, va( "fullserverinfo \"%s\"\n", Info_Serverinfo() ) );

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client != host_client && !client->active && !client->connected && !client->spawned )
			continue;

		SV_FullClientUpdate( client, &msg );
	}

	Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
	Netchan_FragSend( &host_client->netchan );
}

/*
==================
SV_SendRes_f

==================
*/
void SV_SendRes_f( void )
{
	unsigned char	data[65536];
	sizebuf_t	msg;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "SendResources";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	// Not valid on the client
	if ( cmd_source == src_command )
		return;

	if ( host_client->spawned && !host_client->active )
		return;

	host_client->m_sendrescount = host_client->m_sendrescount + 1;

	SV_SendResources( &msg );
	Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
	Netchan_FragSend( &host_client->netchan );
}

/*
==================
SV_Spawn_f

==================
*/
void SV_Spawn_f( void )
{
	unsigned char	data[65536];
	sizebuf_t	msg;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Spawning";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "spawn is not valid\n" );
		return;
	}

	host_client->crcValue = atoi( Cmd_Argv( 2 ) );

	COM_UnMunge2( (unsigned char *)&host_client->crcValue, 4, ( 0xFF - svs.spawncount ) & 0xFF );

	if ( cmd_source == src_command )
	{
		Con_Printf( "spawn is not valid from the console\n" );
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( !cls.demoplayback && Q_atoi( Cmd_Argv( 1 ) ) != svs.spawncount )
	{
		SV_New_f();
		return;
	}

	SZ_Write( &msg, sv.signon.data, sv.signon.cursize );

	SV_WriteSpawn( &msg );
	SV_WriteVoiceCodec( &msg );

	Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
	Netchan_FragSend( &host_client->netchan );
}

/*
==================
SV_CheckVoiceChanges

==================
*/
void SV_CheckVoiceChanges( void )
{
	static char		voice_codec[64];
	static qboolean	initialised;
	static int		quality;
	int			i;
	sizebuf_t	buf_guess;
	byte		data[128];

	if ( !initialised )
	{
		initialised = TRUE;
		strncpy( voice_codec, sv_voicecodec.string, sizeof( voice_codec ) );
		quality = sv_voicequality.value;
	}

	if ( _stricmp( voice_codec, sv_voicecodec.string ) || quality != (int)sv_voicequality.value )
	{
		quality = sv_voicequality.value;
		strncpy( voice_codec, sv_voicecodec.string, sizeof( voice_codec ) );

		Q_memset( &buf_guess, 0, sizeof( buf_guess ) );
		buf_guess.buffername = "Voice";
		buf_guess.data = data;
		buf_guess.cursize = 0;
		buf_guess.maxsize = sizeof( data );

		SV_WriteVoiceCodec( &buf_guess );

		for ( i = 0; i < svs.maxclients; i++ )
		{
			client_t	*cl;

			cl = &svs.clients[i];
			if ( !cl->connected )
				continue;
			if ( cl->fakeclient )
				continue;

			Netchan_CreateFragments( TRUE, &cl->netchan, &buf_guess );
			Netchan_FragSend( &cl->netchan );
		}
	}
}

/*
=================
SV_CheckUpdateRate

=================
*/
void SV_CheckUpdateRate( double *rate )
{
	if ( *rate == 0.0 )
	{
		*rate = 0.05;
		return;
	}

	if ( sv_maxupdaterate.value <= 0.001f && sv_maxupdaterate.value != 0.0 )
		Cvar_Set( "sv_maxupdaterate", "30.0" );

	if ( sv_minupdaterate.value <= 0.001f && sv_minupdaterate.value != 0.0 )
		Cvar_Set( "sv_minupdaterate", "1.0" );

	if ( sv_maxupdaterate.value != 0.0 )
	{
		if ( *rate < 1.0 / sv_maxupdaterate.value )
			*rate = 1.0 / sv_maxupdaterate.value;
	}

	if ( sv_minupdaterate.value != 0.0 )
	{
		if ( *rate > 1.0 / sv_minupdaterate.value )
			*rate = 1.0 / sv_minupdaterate.value;
	}
}

/*
=================
SV_RejectConnection

Rejects connection request and sends back a message
=================
*/
void SV_RejectConnection( netadr_t *adr, char *fmt, ... )
{
	va_list		argptr;
	char		text[1024];

	va_start( argptr, fmt );
	_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 );
	MSG_WriteByte( &net_message, S2C_ERROR );
	MSG_WriteString( &net_message, text );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, *adr );
	SZ_Clear( &net_message );
}

/*
=================
SV_RejectConnectionForPassword

Rejects connection request and sends back a message
=================
*/
void SV_RejectConnectionForPassword( netadr_t *adr )
{
	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 );
	MSG_WriteByte( &net_message, S2C_BADPASSWORD );
	MSG_WriteString( &net_message, "BADPASSWORD" );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, *adr );
	SZ_Clear( &net_message );
}

/*
=================
SV_GetFragmentSize

Returns the size of a file fragment the client accepts
=================
*/
int SV_GetFragmentSize( void *state )
{
	client_t	*cl;

	cl = (client_t *)state;

	if ( cl->active && cl->spawned && cl->connected && cl->fully_connected )
	{
		char	*val;
		int		size;

		val = Info_ValueForKey( cl->userinfo, "cl_dlmax" );
		if ( Q_strlen( val ) )
		{
			size = Q_atoi( val );
			size = max( FRAGMENT_S2C_MIN_SIZE, size );
			size = min( FRAGMENT_S2C_MAX_SIZE, size );
			return size;
		}

		return FRAGMENT_S2C_MIN_SIZE;
	}

	return FRAGMENT_S2C_MAX_SIZE;
}

/*
=================
SV_FilterUser

=================
*/
qboolean SV_FilterUser( USERID_t *userid )
{
	int		i, j;

	for ( i = numuserfilters - 1; i >= 0; i-- )
	{
		if ( userfilters[i].banEndTime != 0.0f && userfilters[i].banEndTime <= realtime )
		{
			// remove the expired ban
			for ( j = i + 1; j < numuserfilters; j++ )
				userfilters[j - 1] = userfilters[j];
			numuserfilters--;
			continue;
		}

		if ( SV_CompareUserID( userid, &userfilters[i].userid ) )
			return (qboolean)sv_filterban.value;
	}

	return sv_filterban.value == 0.0f;
}

/*
=================
SV_CheckProtocol

=================
*/
int SV_CheckProtocol( netadr_t *adr, int nProtocol )
{
	if ( !adr )
	{
		Sys_Error( "SV_CheckProtocol:  Null address\n" );
		return 0;
	}

	if ( nProtocol != PROTOCOL_VERSION )
	{
		if ( nProtocol > PROTOCOL_VERSION )
		{
			SV_RejectConnection( adr, "This server is using an older protocol ( %i ) than your client ( %i ).  If you believe this server is outdated, you can contact the server administrator at %s.\n",
				PROTOCOL_VERSION, nProtocol, Q_strlen( sv_contact.string ) > 0 ? sv_contact.string : "(no email address specified)" );
		}
		else
		{
			SV_RejectConnection( adr, "This server is using a newer protocol ( %i ) than your client ( %i ).  You should check for updates to your client.\n",
				PROTOCOL_VERSION, nProtocol );
		}
		return 0;
	}

	return 1;
}

typedef struct
{
	netadr_t	adr;		// Address where challenge value was sent to.
	int			challenge;	// To connect, adr IP address must respond with this #
	int			time;		// # is valid for only a short duration.
} challenge_t;

challenge_t	g_rg_sv_challenges[MAX_CHALLENGES];

qboolean	allow_cheats;

/*
=================
SV_CheckChallenge

Make sure connecting client is using proper challenge value
=================
*/
int SV_CheckChallenge( netadr_t *adr, int nChallengeValue )
{
	int		i;

	if ( !adr )
	{
		Sys_Error( "SV_CheckChallenge:  Null address\n" );
		return 0;
	}

	if ( NET_IsLocalAddress( *adr ) )
		return 1;

	// see if the challenge is valid
	for ( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[i].adr ) )
		{
			if ( nChallengeValue == g_rg_sv_challenges[i].challenge )
				break;

			SV_RejectConnection( adr, "Bad challenge.\n" );
			return 0;
		}
	}

	if ( i == MAX_CHALLENGES )
	{
		SV_RejectConnection( adr, "No challenge for your address.\n" );
		return 0;
	}

	return 1;
}

/*
=================
SV_CheckIPRestrictions

Determine if client is outside appropriate address range
=================
*/
int SV_CheckIPRestrictions( netadr_t *adr, int nAuthProtocol )
{
	if ( !sv_lan.value && nAuthProtocol == 3 )
		return 1;

	if ( nAuthProtocol == 2 )
		return 1;

	if ( sv_lan.value && ( NET_CompareClassBAdr( *adr, net_local_adr ) || NET_IsReservedAdr( *adr ) ) )
		return 1;

	return 0;
}

/*
=================
SV_CheckIPConnectionReuse

Determine if client is outside appropriate address range
=================
*/
int SV_CheckIPConnectionReuse( netadr_t *adr )
{
	int		count;
	int		i;

	count = 0;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		client_t	*cl;

		cl = &svs.clients[i];
		if ( cl->connected && !cl->fully_connected && NET_CompareBaseAdr( *adr, cl->netchan.remote_address ) )
			count++;
	}

	if ( count > 5 )
	{
		Log_Printf( "Too many connect packets from %s\n", NET_AdrToString( *adr ) );
		return 0;
	}

	return 1;
}

/*
=================
SV_FinishCertificateCheck

For LAN connections, make sure we don't have too many people with same cd key hash
For Authenticated net connections, check the certificate and also double check won userid
from that certificate
=================
*/
int SV_FinishCertificateCheck( netadr_t *adr, int nAuthProtocol, char *szRawCertificate, char *userinfo )
{
	int		nHashCount;
	char	*val;

	nHashCount = 0;

	switch ( nAuthProtocol )
	{
	default:
		if ( _stricmp( szRawCertificate, "steam" ) )
		{
			SV_RejectConnection( adr, "Expecting STEAM authentication USERID ticket!\n" );
			return 0;
		}
		return 1;

	case 2:
		if ( Q_strlen( szRawCertificate ) != 32 )
		{
			SV_RejectConnection( adr, "Invalid CD Key.\n" );
			return 0;
		}

		if ( adr->type != NA_LOOPBACK )
		{
			val = Info_ValueForKey( userinfo, "*hltv" );
			if ( Q_strlen( val ) )
			{
				int		i;

				i = Q_atoi( val );
				if ( i != TYPE_PROXY )
				{
					SV_RejectConnection( adr, "Invalid CD Key.\n" );
					return 0;
				}
			}
			else
			{
				SV_RejectConnection( adr, "Invalid CD Key.\n" );
				return 0;
			}
		}
	}

	return 1;
}

/*
=================
SV_CheckKeyInfo

Make sure connecting client is not using a bogus cdkey
=================
*/
int SV_CheckKeyInfo( netadr_t *adr, char *protinfo, unsigned short *port, int *pAuthProtocol, char *pszRaw, char *cdkey )
{
	const char	*s;
	int		nAuthProtocol;

	nAuthProtocol = -1;

	s = Info_ValueForKey( protinfo, "prot" );
	nAuthProtocol = Q_atoi( s );

	if ( nAuthProtocol <= 0 || nAuthProtocol > 4 )
	{
		SV_RejectConnection( adr, "Invalid connection.\n" );
		return 0;
	}

	s = Info_ValueForKey( protinfo, "raw" );

	if ( Q_strlen( s ) <= 0 || ( nAuthProtocol == 2 && Q_strlen( s ) != 32 ) )
	{
		SV_RejectConnection( adr, "Invalid authentication certificate length.\n" );
		return 0;
	}

	Q_strcpy( pszRaw, s );

	if ( nAuthProtocol == 2 )
	{
		_snprintf( cdkey, 64, "%s", s );
	}
	else
	{
		s = Info_ValueForKey( protinfo, "cdkey" );
		if ( Q_strlen( s ) != 32 )
		{
			SV_RejectConnection( adr, "Invalid hashed CD key.\n" );
			return 0;
		}

		_snprintf( cdkey, 64, "%s", s );
	}

	*port = Q_atoi( "27005" );
	*pAuthProtocol = nAuthProtocol;

	return 1;
}

/*
=================
SV_CheckForDuplicateSteamID

=================
*/
int SV_CheckForDuplicateSteamID( client_t *client )
{
	int		nRet;
	int		nCount;

	nRet = -1;
	nCount = 0;

	if ( sv_lan.value )
		return nRet;

	for ( nCount = 0; nCount < svs.maxclients; nCount++ )
	{
		client_t	*cl;

		cl = &svs.clients[nCount];

		if ( !cl->connected )
			continue;
		if ( cl->fakeclient )
			continue;
		if ( cl->network_userid.idtype != AUTH_IDTYPE_STEAM && cl->network_userid.idtype != AUTH_IDTYPE_VALVE )
			continue;
		if ( client == cl )
			continue;
		if ( !SV_CompareUserID( &client->network_userid, &cl->network_userid ) )
			continue;

		nRet = nCount;
		break;
	}

	return nRet;
}

/*
=================
SV_CheckForDuplicateNames

=================
*/
int SV_CheckForDuplicateNames( char *userinfo, qboolean bIsReconnecting, int nExcludeSlot )
{
	char	newname[32];
	int		i;
	client_t	*client;
	char	*val;
	char	*p;
	int		dupc;
	int		retval;

	dupc = 1;
	retval = FALSE;

	val = (char *)Info_ValueForKey( userinfo, "name" );

	if ( !val || !val[0] || Q_strstr( val, ".." ) )
	{
		Info_SetValueForKey( userinfo, "name", "unamed", MAX_INFO_STRING );
		return TRUE;
	}

	while ( 1 )
	{
		for ( i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++ )
		{
			if ( bIsReconnecting && i == nExcludeSlot )
				continue;
			if ( client->connected && !Q_stricmp( client->name, val ) )
				break;
		}

		if ( i >= MAX_CLIENTS )
			break;

		p = val;
		if ( val[0] == '(' )
		{
			if ( val[2] == ')' )
				p = val + 3;
			else if ( val[3] == ')' )
				p = val + 4;
		}

		_snprintf( newname, sizeof( newname ), "(%d)%-0.*s", dupc++, 28, p );
		Info_SetValueForKey( userinfo, "name", newname, MAX_INFO_STRING );

		val = (char *)Info_ValueForKey( userinfo, "name" );
		retval = TRUE;
	}

	return retval;
}

/*
=================
SV_CheckUserInfo

Check connection password and remove it from userinfo
=================
*/
int SV_CheckUserInfo( netadr_t *adr, char *userinfo, qboolean bIsReconnecting, int nReconnectSlot, char *name )
{
	const char	*s;
	char	*pChar;
	char	newname[32];
	int		proxies;
	int		i;

	i = 0;

	if ( !NET_IsLocalAddress( *adr ) && sv_password.string[0] && Q_stricmp( sv_password.string, "none" ) )
	{
		if ( Q_strcmp( sv_password.string, Info_ValueForKey( userinfo, "password" ) ) )
		{
			Con_Printf( "%s:  password failed\n", NET_AdrToString( *adr ) );
			SV_RejectConnectionForPassword( adr );
			return 0;
		}
	}

	if ( strlen( userinfo ) < 5 || strstr( userinfo, "\\\\" ) || userinfo[strlen( userinfo ) - 1] == '\\' )
	{
		SV_RejectConnection( adr, "Unknown HLTV client type.\n" );
		return 0;
	}

	Info_RemoveKey( userinfo, "password" );

	s = Info_ValueForKey( userinfo, "name" );
	if ( Q_strlen( s ) )
	{
		Q_strncpy( newname, s, sizeof( newname ) );
		newname[sizeof( newname ) - 1] = 0;
	}
	else
	{
		Q_strcpy( newname, "unnamed" );
	}

	for ( pChar = newname; pChar && *pChar; pChar++ )
	{
		if ( *pChar == '%' || *pChar == '&' )
			*pChar = ' ';
	}

	i = 0;
	while ( newname[i] == '#' )
	{
		newname[i] = ' ';
		i++;
	}

	TrimSpace( newname, name );

	Info_SetValueForKey( userinfo, "name", name, MAX_INFO_STRING );

	if ( SV_CheckForDuplicateNames( userinfo, bIsReconnecting, nReconnectSlot ) )
	{
		s = Info_ValueForKey( userinfo, "name" );
		Q_strncpy( name, s, 32 );
		name[31] = 0;
	}

	s = Info_ValueForKey( userinfo, "*hltv" );
	if ( Q_strlen( s ) )
	{
		switch ( Q_atoi( s ) )
		{
		case TYPE_CLIENT:
			break;

		case TYPE_PROXY:
			SV_CountProxies( &proxies );
			if ( proxies >= sv_proxies.value && !bIsReconnecting )
			{
				SV_RejectConnection( adr, "Proxy slots are full.\n" );
				return 0;
			}
			break;

		case TYPE_COMMENTATOR:
			SV_RejectConnection( adr, "Please connect to HLTV master proxy.\n" );
			return 0;

		default:
			SV_RejectConnection( adr, "Unknown HLTV client type.\n" );
			return 0;
		}
	}

	return 1;
}

/*
=================
SV_FindEmptySlot

=================
*/
int SV_FindEmptySlot( netadr_t *adr, int *pslot, client_t **ppClient )
{
	client_t	*client;
	int		slot;
	int		clients;

	clients = 0;
	SV_CountPlayers( &clients );

	for ( slot = 0; slot < svs.maxclients; slot++ )
	{
		client = &svs.clients[slot];
		if ( !client->active && !client->spawned && !client->connected )
			break;
	}

	// if at server limits, refuse connection
	if ( slot >= svs.maxclients )
	{
		SV_RejectConnection( adr, "Server is full.\n" );
		return 0;
	}

	*pslot = slot;
	*ppClient = client;
	return 1;
}

/*
==================
SV_ConnectClient

A connection request that did not come from the master
==================
*/
void SV_ConnectClient( void )
{
	edict_t		*ent;
	client_t	*client;
	netadr_t	adr;
	int			edictnum;
	int			nClientSlot;
	char		userinfo[1024];
	char		protinfo[1024];
	char		cdkey[64];
	char		*s;
	char		name[32];
	char		szRawCertificate[512];
	int			nAuthProtocol;
	unsigned short	port;
	qboolean	reconnect;
	qboolean	bIsSecure;

	client = NULL;
	nAuthProtocol = -1;
	reconnect = FALSE;
	bIsSecure = FALSE;

	adr = net_from;
	port = Q_atoi( "27005" );

	if ( Cmd_Argc() < 5 )
	{
		SV_RejectConnection( &adr, "Insufficient connection info\n" );
		return;
	}

	if ( !SV_CheckProtocol( &adr, Q_atoi( Cmd_Argv( 1 ) ) ) )
		return;

	if ( !SV_CheckChallenge( &adr, Q_atoi( Cmd_Argv( 2 ) ) ) )
		return;

	Q_memset( szRawCertificate, 0, sizeof( szRawCertificate ) );

	s = Cmd_Argv( 3 );
	if ( !Info_IsValid( s ) )
	{
		SV_RejectConnection( &adr, "Invalid protinfo in connect command\n" );
		return;
	}

	Q_strncpy( protinfo, s, sizeof( protinfo ) - 1 );
	protinfo[sizeof( protinfo ) - 1] = 0;

	if ( !SV_CheckKeyInfo( &adr, protinfo, &port, &nAuthProtocol, szRawCertificate, cdkey ) )
		return;

	if ( !SV_CheckIPRestrictions( &adr, nAuthProtocol ) )
	{
		SV_RejectConnection( &adr, "LAN servers are restricted to local clients (class C).\n" );
		return;
	}

	s = Cmd_Argv( 4 );
	if ( Q_strlen( s ) > 256 || !Info_IsValid( s ) )
	{
		SV_RejectConnection( &adr, "Invalid userinfo in connect command\n" );
		return;
	}

	Q_strncpy( userinfo, s, sizeof( userinfo ) - 1 );
	userinfo[sizeof( userinfo ) - 1] = 0;

	if ( Master_IsLanGame() || nAuthProtocol == 2 || nAuthProtocol == 3 )
	{
		for ( nClientSlot = 0; nClientSlot < svs.maxclients; nClientSlot++ )
		{
			client = &svs.clients[nClientSlot];
			if ( NET_CompareAdr( adr, client->netchan.remote_address ) )
			{
				reconnect = TRUE;
				break;
			}
		}
	}

	if ( !SV_CheckUserInfo( &adr, userinfo, reconnect, nClientSlot, name ) )
		return;

	if ( !SV_FinishCertificateCheck( &adr, nAuthProtocol, szRawCertificate, userinfo ) )
		return;

	if ( !reconnect )
	{
		if ( !SV_FindEmptySlot( &adr, &nClientSlot, &client ) )
			return;
	}
	else
	{
		if ( ( client->active || client->spawned ) && client->edict )
		{
			Steam_NotifyClientDisconnect( client );
			gEntityInterface.pfnClientDisconnect( client->edict );
		}

		Con_Printf( "%s:reconnect\n", NET_AdrToString( adr ) );
	}

	if ( !SV_CheckIPConnectionReuse( &adr ) )
		return;

	// build a new connection
	// accept the new client
	// this is the only place a client_t is ever initialized
	host_client = client;
	host_client->userid = g_userid++;

	if ( nAuthProtocol == 3 )
	{
		int		len;
		char	szSteamAuthBuf[1024];

		len = net_message.cursize - msg_readcount;
		if ( len <= 0 || len >= sizeof( szSteamAuthBuf ) )
		{
			SV_RejectConnection( &adr, "STEAM certificate length error! %i/%i\n", len, sizeof( szSteamAuthBuf ) );
			return;
		}

		memcpy( szSteamAuthBuf, &net_message.data[msg_readcount], len );

		host_client->network_userid.clientip = *(unsigned int *)&adr.ip[0];
		if ( adr.type == NA_LOOPBACK )
		{
			if ( sv_lan.value > 0 )
				host_client->network_userid.clientip = 0x7F000001;	// 127.0.0.1
			else
				host_client->network_userid.clientip = *(unsigned int *)&net_local_adr.ip[0];
		}

		host_client->netchan.remote_address.port = adr.port ? adr.port : port;

		if ( !Steam_NotifyClientConnect( client, szSteamAuthBuf, len ) )
		{
			if ( !sv_lan.value )
			{
				SV_RejectConnection( &adr, "STEAM validation rejected\n" );
				return;
			}

			host_client->network_userid.idtype = AUTH_IDTYPE_STEAM;
			host_client->network_userid.m_SteamID = 0;
		}
	}
	else if ( nAuthProtocol == 2 )
	{
		char	*val;

		val = (char *)Info_ValueForKey( userinfo, "*hltv" );
		if ( Q_strlen( val ) )
		{
			int		i;

			i = Q_atoi( val );
			if ( i != TYPE_PROXY )
			{
				SV_RejectConnection( &adr, "Invalid validation type\n" );
				return;
			}
		}
		else
		{
			SV_RejectConnection( &adr, "Invalid validation type\n" );
			return;
		}

		host_client->network_userid.idtype = AUTH_IDTYPE_LOCAL;
		host_client->network_userid.m_SteamID = 0;
		host_client->network_userid.clientip = *(unsigned int *)&adr.ip[0];

		Steam_NotifyBotConnect( client );
	}
	else
	{
		SV_RejectConnection( &adr, "Invalid authentication type\n" );
		return;
	}

	SV_ClearResourceLists( host_client );
	SV_ClearFrames( &host_client->frames );
	host_client->frames = (client_frame_t *)Mem_ZeroMalloc( SV_UPDATE_BACKUP * sizeof( client_frame_t ) );

	host_client->resourcesneeded.pNext = host_client->resourcesneeded.pPrev = &host_client->resourcesneeded;
	host_client->resourcesonhand.pNext = host_client->resourcesonhand.pPrev = &host_client->resourcesonhand;

	edictnum = nClientSlot + 1;
	ent = EDICT_NUM( edictnum );
	host_client->edict = ent;

	if ( g_modfuncs.m_pfnConnectClient )
		g_modfuncs.m_pfnConnectClient( nClientSlot );

	Netchan_Setup( NS_SERVER, &host_client->netchan, adr, client - svs.clients, client, SV_GetFragmentSize );

	host_client->next_messageinterval = 0.05;
	host_client->next_messagetime = realtime + host_client->next_messageinterval;
	host_client->delta_sequence = -1;
	Q_memset( &host_client->lastcmd, 0, sizeof( host_client->lastcmd ) );
	host_client->nextping = -1;

	if ( host_client->netchan.remote_address.type == NA_LOOPBACK )
		Con_DPrintf( "Local connection.\n" );
	else
		Con_DPrintf( "Client %s connected\nAdr: %s\n", name, NET_AdrToString( host_client->netchan.remote_address ) );

	Q_strncpy( host_client->hashedcdkey, cdkey, 32 );
	host_client->hashedcdkey[32] = 0;

	host_client->active = FALSE;
	host_client->spawned = FALSE;
	host_client->connected = TRUE;
	host_client->uploading = FALSE;
	host_client->fully_connected = FALSE;

	bIsSecure = Steam_GSBSecure();

	Netchan_OutOfBandPrint( NS_SERVER, adr, "%c %i \"%s\" %i", S2C_CONNECTION, host_client->userid, NET_AdrToString( host_client->netchan.remote_address ), bIsSecure );

	Log_Printf( "\"%s<%i><%s><>\" connected, address \"%s\"\n", name, host_client->userid, SV_GetClientIDString( host_client ), NET_AdrToString( host_client->netchan.remote_address ) );

	Q_strncpy( host_client->userinfo, userinfo, MAX_INFO_STRING );
	host_client->userinfo[MAX_INFO_STRING - 1] = 0;

	SV_ExtractFromUserinfo( host_client );

	Info_SetValueForStarKey( host_client->userinfo, "*sid", va( "%I64d", host_client->network_userid.m_SteamID ), MAX_INFO_STRING );

	host_client->datagram.flags = FSB_ALLOWOVERFLOW;
	host_client->datagram.data = host_client->datagram_buf;
	host_client->datagram.maxsize = sizeof( host_client->datagram_buf );
	host_client->datagram.buffername = host_client->name;

	host_client->sendinfo_time = 0;
}

/*
================
SVC_Ping

Just responds with an acknowledgement
================
*/
void SVC_Ping( void )
{
	char	data[6];

	data[0] = 0xff;
	data[1] = 0xff;
	data[2] = 0xff;
	data[3] = 0xff;
	data[4] = A2A_ACK;
	data[5] = 0;

	NET_SendPacket( NS_SERVER, sizeof( data ), data, net_from );
}

/*
=================
SVC_GetChallenge

Returns a challenge number that can be used
in a subsequent client_connect command.
We do this to prevent denial of service attacks that
flood the server with invalid connection IPs.  With a
challenge, they must give a valid IP address.
=================
*/
void SVC_GetChallenge( void )
{
	int		i;
	int		oldest;
	int		oldestTime;
	char	data[1024];
	int		protocol;
	qboolean	steam;

	oldest = 0;
	oldestTime = 0x7fffffff;
	steam = FALSE;

	if ( Cmd_Argc() == 2 && !Q_stricmp( Cmd_Argv( 1 ), "steam" ) )
		steam = TRUE;

	// see if we already have a challenge for this ip
	for ( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[i].adr ) )
			break;

		if ( g_rg_sv_challenges[i].time < oldestTime )
		{
			oldestTime = g_rg_sv_challenges[i].time;
			oldest = i;
		}
	}

	if ( i == MAX_CHALLENGES )
	{
		// overwrite the oldest
		g_rg_sv_challenges[oldest].challenge = ( RandomLong( 0, 36863 ) << 16 ) | RandomLong( 0, 65535 );
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = realtime;
		i = oldest;
	}

	// send it back
	if ( steam )
	{
		_snprintf( data, sizeof( data ), "%c%c%c%c%c00000000 %u %i %I64i %d\n", 255, 255, 255, 255, S2C_CHALLENGE, g_rg_sv_challenges[i].challenge, 3, Steam_GSGetSteamID(), Steam_GSBSecure() );
	}
	else
	{
		protocol = 0;
		Con_DPrintf( "Server requiring authentication\n" );
		protocol = 2;
		_snprintf( data, sizeof( data ), "%c%c%c%c%c00000000 %u %i\n", 255, 255, 255, 255, S2C_CHALLENGE, g_rg_sv_challenges[i].challenge, protocol );
	}

	NET_SendPacket( NS_SERVER, Q_strlen( data ) + 1, data, net_from );
}

/*
=================
SVC_ServiceChallenge

Returns a challenge number that can be used
in a subsequent rcon command.
=================
*/
void SVC_ServiceChallenge( void )
{
	int		i;
	int		oldest;
	int		oldestTime;
	char	data[128];
	char	*type;

	type = NULL;
	oldest = 0;
	oldestTime = 0x7fffffff;

	if ( Cmd_Argc() != 2 )
		return;

	type = Cmd_Argv( 1 );
	if ( !type || !type[0] || Q_stricmp( type, "rcon" ) )
		return;

	// see if we already have a challenge for this ip
	for ( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if ( NET_CompareBaseAdr( net_from, g_rg_sv_challenges[i].adr ) )
			break;

		if ( g_rg_sv_challenges[i].time < oldestTime )
		{
			oldestTime = g_rg_sv_challenges[i].time;
			oldest = i;
		}
	}

	if ( i == MAX_CHALLENGES )
	{
		// overwrite the oldest
		g_rg_sv_challenges[oldest].challenge = ( RandomLong( 0, 36863 ) << 16 ) | RandomLong( 0, 65535 );
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = realtime;
		i = oldest;
	}

	// send it back
	_snprintf( data, sizeof( data ), "%c%c%c%cchallenge %s %u\n", 255, 255, 255, 255, type, g_rg_sv_challenges[i].challenge );

	NET_SendPacket( NS_SERVER, Q_strlen( data ) + 1, data, net_from );
}

/*
=================
SV_ResetModInfo

=================
*/
void SV_ResetModInfo( void )
{
	Q_memset( &gmodinfo, 0, sizeof( modinfo_t ) );
	gmodinfo.version = 1;
	gmodinfo.svonly = TRUE;
}

/*
=================
SV_GetFakeClientCount

=================
*/
int SV_GetFakeClientCount( void )
{
	int		i, fakeclients;

	fakeclients = 0;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		client_t	*client;

		client = &svs.clients[i];
		if ( client->fakeclient )
			fakeclients++;
	}

	return fakeclients;
}

/*
=================
SV_GetModInfo

=================
*/
qboolean SV_GetModInfo( char *pszInfo, char *pszDL, int *version, int *size, qboolean *svonly, qboolean *cldll, char *pszHLVersion )
{
	if ( gmodinfo.bIsMod )
	{
		Q_strcpy( pszInfo, gmodinfo.szInfo );
		Q_strcpy( pszDL, gmodinfo.szDL );
		Q_strcpy( pszHLVersion, gmodinfo.szHLVersion );
		*version = gmodinfo.version;
		*size = gmodinfo.size;
		*svonly = gmodinfo.svonly;
		*cldll = gmodinfo.cldll;
	}
	else
	{
		Q_strcpy( pszInfo, "" );
		Q_strcpy( pszDL, "" );
		Q_strcpy( pszHLVersion, "" );
		*version = 1;
		*size = 0;
		*svonly = TRUE;
		*cldll = FALSE;
	}

	return gmodinfo.bIsMod;
}

/*
=================
RequireValidChallenge

=================
*/
qboolean RequireValidChallenge( netadr_t *adr )
{
	if ( sv_enableoldqueries.value )
		return FALSE;
	return TRUE;
}

/*
=================
ValidInfoChallenge

=================
*/
qboolean ValidInfoChallenge( netadr_t *adr, const char *nugget )
{
	if ( !sv.active || !nugget )
		return FALSE;

	if ( svs.maxclients <= 1 )
		return FALSE;

	if ( RequireValidChallenge( adr ) && Q_stricmp( nugget, "Source Engine Query" ) )
		return FALSE;

	return TRUE;
}

/*
=================
GetChallengeNr

=================
*/
int GetChallengeNr( netadr_t *adr )
{
	int		oldest;
	int		oldestTime;
	int		i;

	oldest = 0;
	oldestTime = 0x7fffffff;

	// see if we already have a challenge for this ip
	for ( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if ( NET_CompareBaseAdr( *adr, g_rg_sv_challenges[i].adr ) )
		{
			g_rg_sv_challenges[i].time = realtime;
			break;
		}

		if ( g_rg_sv_challenges[i].time < oldestTime )
		{
			oldestTime = g_rg_sv_challenges[i].time;
			oldest = i;
		}
	}

	if ( i == MAX_CHALLENGES )
	{
		// overwrite the oldest
		g_rg_sv_challenges[oldest].challenge = ( RandomLong( 0, 36863 ) << 16 ) | RandomLong( 0, 65535 );
		g_rg_sv_challenges[oldest].adr = net_from;
		g_rg_sv_challenges[oldest].time = realtime;
		i = oldest;
	}

	return g_rg_sv_challenges[i].challenge;
}

/*
=================
CheckChallengeNr

=================
*/
qboolean CheckChallengeNr( netadr_t *adr, int nChallengeValue )
{
	int		i;

	if ( !adr || nChallengeValue == -1 )
		return FALSE;

	// see if the challenge is valid
	for ( i = 0; i < MAX_CHALLENGES; i++ )
	{
		if ( NET_CompareBaseAdr( *adr, g_rg_sv_challenges[i].adr ) )
		{
			if ( nChallengeValue == g_rg_sv_challenges[i].challenge )
			{
				// allow the challenge to be valid for an hour
				if ( g_rg_sv_challenges[i].time + 3600.0f < realtime )
					i = MAX_CHALLENGES;
				break;
			}
			return FALSE;
		}
	}

	if ( i == MAX_CHALLENGES )
		return FALSE;

	return TRUE;
}

/*
=================
ReplyServerChallenge

=================
*/
void ReplyServerChallenge( netadr_t *adr )
{
	char		buffer[16];
	sizebuf_t	buf;

	buf.buffername = "SVC_Challenge";
	buf.maxsize = sizeof( buffer );
	buf.cursize = 0;
	buf.data = (byte *)buffer;
	buf.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteLong( &buf, 0xffffffff );
	MSG_WriteByte( &buf, S2C_CHALLENGE );
	MSG_WriteLong( &buf, GetChallengeNr( adr ) );

	NET_SendPacket( NS_SERVER, buf.cursize, buf.data, *adr );
}

/*
=================
ValidChallenge

=================
*/
qboolean ValidChallenge( netadr_t *adr, int challengeNr )
{
	if ( !sv.active )
		return FALSE;

	if ( svs.maxclients <= 1 )
		return FALSE;

	if ( RequireValidChallenge( adr ) && !CheckChallengeNr( adr, challengeNr ) )
	{
		ReplyServerChallenge( adr );
		return FALSE;
	}

	return TRUE;
}

/*
================
SVC_InfoString

Responds with all the info that qplug or qspy can see
This message can be up to around 5k with worst case string lengths.
================
*/
void SVC_InfoString( void )
{
	int		i;
	int		count;
	int		proxy;
	sizebuf_t	buf;
	byte	data[1400];
	char	address[256];
	char	gd[MAX_PATH];
	char	info[2048];
	int		iHasPW;
	char	szOS[2];

	buf.buffername = "SVC_InfoString";
	buf.maxsize = sizeof( data );
	buf.cursize = 0;
	buf.data = data;
	buf.flags = FSB_ALLOWOVERFLOW;

	if ( noip && noipx )
		return;

	if ( !sv.active )
		return;

	if ( svs.maxclients <= 1 )
		return;

	count = 0;
	proxy = 0;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		if ( svs.clients[i].active || svs.clients[i].spawned || svs.clients[i].connected )
		{
			if ( svs.clients[i].proxy )
				proxy++;
			count++;
		}
	}

	address[0] = 0;

	if ( !noip )
	{
		Q_strncpy( address, NET_AdrToString( net_local_adr ), sizeof( address ) - 1 );
		address[sizeof( address ) - 1] = 0;
	}
	else if ( !noipx )
	{
		Q_strncpy( address, NET_AdrToString( net_local_ipx_adr ), sizeof( address ) - 1 );
		address[sizeof( address ) - 1] = 0;
	}

	iHasPW = sv_password.string[0] && Q_stricmp( sv_password.string, "none" );

	Q_strcpy( szOS, "w" );

	info[0] = 0;

	Info_SetValueForKey( info, "protocol", va( "%i", PROTOCOL_VERSION ), sizeof( info ) );
	Info_SetValueForKey( info, "address", address, sizeof( info ) );
	Info_SetValueForKey( info, "players", va( "%i", count ), sizeof( info ) );
	if ( proxy >= 0 )
		Info_SetValueForKey( info, "proxytarget", va( "%i", proxy ), sizeof( info ) );
	Info_SetValueForKey( info, "lan", va( "%i", Master_IsLanGame() != 0 ), sizeof( info ) );

	{
		int		maxplayers;

		maxplayers = sv_visiblemaxplayers.value;
		if ( maxplayers < 0 )
			maxplayers = svs.maxclients;
		Info_SetValueForKey( info, "max", va( "%i", maxplayers ), sizeof( info ) );
	}

	Info_SetValueForKey( info, "bots", va( "%i", SV_GetFakeClientCount() ), sizeof( info ) );

	COM_FileBase( com_gamedir, gd );
	Info_SetValueForKey( info, "gamedir", gd, sizeof( info ) );
	Info_SetValueForKey( info, "description", gEntityInterface.pfnGetGameDescription(), sizeof( info ) );
	Info_SetValueForKey( info, "hostname", Cvar_VariableString( "hostname" ), sizeof( info ) );
	Info_SetValueForKey( info, "map", sv.name, sizeof( info ) );
	Info_SetValueForKey( info, "type", cls.state == ca_dedicated ? "d" : "l", sizeof( info ) );
	Info_SetValueForKey( info, "password", va( "%i", iHasPW ), sizeof( info ) );
	Info_SetValueForKey( info, "os", szOS, sizeof( info ) );
	Info_SetValueForKey( info, "secure", Steam_GSBSecure() ? "1" : "0", sizeof( info ) );

	if ( gmodinfo.bIsMod )
	{
		Info_SetValueForKey( info, "mod", va( "%i", 1 ), sizeof( info ) );
		Info_SetValueForKey( info, "modversion", va( "%i", gmodinfo.version ), sizeof( info ) );
		Info_SetValueForKey( info, "svonly", va( "%i", gmodinfo.svonly ), sizeof( info ) );
		Info_SetValueForKey( info, "cldll", va( "%i", gmodinfo.cldll ), sizeof( info ) );
	}

	MSG_WriteLong( &buf, 0xffffffff );
	MSG_WriteString( &buf, "infostringresponse" );
	MSG_WriteString( &buf, info );

	NET_SendPacket( NS_SERVER, buf.cursize, buf.data, net_from );
}

/*
================
SVC_Info

Responds with short info for broadcast scans
The second parameter should be the current protocol version number.
================
*/
void SVC_Info( qboolean bDetailed )
{
	int		i, count;
	sizebuf_t	buf;
	byte	data[1400];
	char	szModURL_Info[512];
	char	szModURL_DL[512];
	int		mod_version;
	int		mod_size;
	int		isMod;
	char	gd[MAX_PATH];
	qboolean	cldll, svonly;
	char	szHLVersion[32];

	buf.buffername = "SVC_Info";
	buf.maxsize = sizeof( data );
	buf.cursize = 0;
	buf.data = data;
	buf.flags = FSB_ALLOWOVERFLOW;

	if ( !sv.active )
		return;

	if ( svs.maxclients <= 1 )
		return;

	count = 0;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		if ( svs.clients[i].active || svs.clients[i].spawned || svs.clients[i].connected )
			count++;
	}

	MSG_WriteLong( &buf, 0xffffffff );

	if ( bDetailed )
		MSG_WriteByte( &buf, S2A_INFO_DETAILED );
	else
		MSG_WriteByte( &buf, S2A_INFO );

	if ( !noip )
		MSG_WriteString( &buf, NET_AdrToString( net_local_adr ) );
	else if ( !noipx )
		MSG_WriteString( &buf, NET_AdrToString( net_local_ipx_adr ) );
	else
		MSG_WriteString( &buf, "LOOPBACK" );

	MSG_WriteString( &buf, Cvar_VariableString( "hostname" ) );
	MSG_WriteString( &buf, sv.name );

	COM_FileBase( com_gamedir, gd );
	MSG_WriteString( &buf, gd );
	MSG_WriteString( &buf, gEntityInterface.pfnGetGameDescription() );

	MSG_WriteByte( &buf, count );

	{
		int		maxplayers;

		maxplayers = sv_visiblemaxplayers.value;
		if ( maxplayers < 0 )
			maxplayers = svs.maxclients;
		MSG_WriteByte( &buf, maxplayers );
	}

	MSG_WriteByte( &buf, PROTOCOL_VERSION );

	if ( bDetailed )
	{
		if ( cls.state == ca_dedicated )
			MSG_WriteByte( &buf, 'd' );
		else
			MSG_WriteByte( &buf, 'l' );

		MSG_WriteByte( &buf, 'w' );

		if ( Q_strlen( sv_password.string ) > 0 && Q_stricmp( sv_password.string, "none" ) )
			MSG_WriteByte( &buf, 1 );
		else
			MSG_WriteByte( &buf, 0 );

		isMod = SV_GetModInfo( szModURL_Info, szModURL_DL, &mod_version, &mod_size, &svonly, &cldll, szHLVersion );
		if ( isMod )
		{
			MSG_WriteByte( &buf, 1 );
			MSG_WriteString( &buf, szModURL_Info );
			MSG_WriteString( &buf, szModURL_DL );
			MSG_WriteString( &buf, "" );
			MSG_WriteLong( &buf, mod_version );
			MSG_WriteLong( &buf, mod_size );
			MSG_WriteByte( &buf, svonly );
			MSG_WriteByte( &buf, cldll );
		}
		else
		{
			MSG_WriteByte( &buf, 0 );
		}

		MSG_WriteByte( &buf, Steam_GSBSecure() != 0 );
		MSG_WriteByte( &buf, SV_GetFakeClientCount() );
	}

	NET_SendPacket( NS_SERVER, buf.cursize, buf.data, net_from );
}

int		player_datacounts[32];

typedef struct
{
	int		entdata;
	int		playerdata;
} entcount_t;

entcount_t	ent_datacounts[32];

/*
================
SVC_PlayerInfo

================
*/
void SVC_PlayerInfo( void )
{
	int		i, count;
	client_t	*client;
	sizebuf_t	buf;
	byte	data[2048];

	buf.buffername = "SVC_PlayerInfo";
	buf.maxsize = sizeof( data );
	buf.cursize = 0;
	buf.data = data;
	buf.flags = FSB_ALLOWOVERFLOW;

	if ( !sv.active )
		return;

	if ( svs.maxclients <= 1 )
		return;

	MSG_WriteLong( &buf, 0xffffffff );
	MSG_WriteByte( &buf, S2A_PLAYER );

	count = 0;
	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !svs.clients[i].active )
			continue;
		count++;
	}

	MSG_WriteByte( &buf, count );

	count = 0;
	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !svs.clients[i].active )
			continue;

		count++;
		MSG_WriteByte( &buf, count );
		MSG_WriteString( &buf, client->name );
		MSG_WriteLong( &buf, client->edict->v.frags );
		MSG_WriteFloat( &buf, realtime - client->netchan.connect_time );
	}

	NET_SendPacket( NS_SERVER, buf.cursize, buf.data, net_from );
}

/*
================
SVC_RuleInfo

================
*/
void SVC_RuleInfo( void )
{
	int		nNumRules;
	cvar_t	*var;
	sizebuf_t	buf;
	byte	data[8192];

	buf.buffername = "SVC_RuleInfo";
	buf.maxsize = sizeof( data );
	buf.cursize = 0;
	buf.flags = FSB_ALLOWOVERFLOW;
	buf.data = data;

	if ( !sv.active )
		return;

	if ( svs.maxclients <= 1 )
		return;

	nNumRules = Cvar_CountServerVariables();
	if ( nNumRules <= 0 )
		return;

	MSG_WriteLong( &buf, 0xffffffff );
	MSG_WriteByte( &buf, S2A_RULES );
	MSG_WriteShort( &buf, nNumRules );

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( !( var->flags & FCVAR_SERVER ) )
			continue;

		MSG_WriteString( &buf, var->name );

		if ( var->flags & FCVAR_PROTECTED )
		{
			if ( Q_strlen( var->string ) > 0 && Q_stricmp( var->string, "none" ) )
				MSG_WriteString( &buf, "1" );
			else
				MSG_WriteString( &buf, "0" );
		}
		else
		{
			MSG_WriteString( &buf, var->string );
		}
	}

	NET_SendPacket( NS_SERVER, buf.cursize, buf.data, net_from );
}

/*
================
SVC_GameDllQuery

================
*/
int SVC_GameDllQuery( const char *s )
{
	int		len;
	int		maxsize;
	byte	data[2048];
	int		valid;

	maxsize = sizeof( data );

	if ( !sv.active )
		return 0;

	if ( svs.maxclients <= 1 )
		return 0;

	Q_memset( data, 0, maxsize );
	len = maxsize - 4;

	valid = gEntityInterface.pfnConnectionlessPacket( &net_from, s, (char *)&data[4], &len );

	if ( len && len <= maxsize - 4 )
	{
		*(int *)data = 0xffffffff;
		NET_SendPacket( NS_SERVER, len + 4, data, net_from );
	}

	return valid;
}

/*
==============================================================================

REDIRECTION

==============================================================================
*/

char		outputbuf[1400];
redirect_t	sv_redirected;
netadr_t	sv_redirectto;

/*
==================
SV_FlushRedirect

==================
*/
void SV_FlushRedirect( void )
{
	if ( sv_redirected == RD_PACKET )
	{
		sizebuf_t	buf;
		byte		*data;

		data = (byte *)_alloca( Q_strlen( outputbuf ) + 7 );

		buf.buffername = "Redirected Text";
		buf.maxsize = Q_strlen( outputbuf ) + 7;
		buf.cursize = 0;
		buf.data = data;
		buf.flags = FSB_ALLOWOVERFLOW;

		MSG_WriteLong( &buf, -1 );
		MSG_WriteByte( &buf, A2A_PRINT );
		MSG_WriteString( &buf, outputbuf );
		MSG_WriteByte( &buf, 0 );

		NET_SendPacket( NS_SERVER, buf.cursize, buf.data, sv_redirectto );
	}
	else if ( sv_redirected == RD_CLIENT )
	{
		MSG_WriteByte( &host_client->netchan.message, svc_print );
		MSG_WriteString( &host_client->netchan.message, outputbuf );
	}

	// clear it
	outputbuf[0] = 0;
}

/*
==================
SV_BeginRedirect

  Send Con_Printf data to the remote client
  instead of the console
==================
*/
void SV_BeginRedirect( redirect_t rd, netadr_t *addr )
{
	sv_redirected = rd;
	sv_redirectto = *addr;
	outputbuf[0] = 0;
}

/*
==================
SV_EndRedirect

==================
*/
void SV_EndRedirect( void )
{
	SV_FlushRedirect();
	sv_redirected = RD_NONE;
}

cvar_t	sv_rcon_minfailures = { "sv_rcon_minfailures", "5" };
cvar_t	sv_rcon_maxfailures = { "sv_rcon_maxfailures", "10" };
cvar_t	sv_rcon_minfailuretime = { "sv_rcon_minfailuretime", "30" };
cvar_t	sv_rcon_banpenalty = { "sv_rcon_banpenalty", "0" };

#define MAX_RCON_FAILURES_STORAGE	32
#define MAX_RCON_FAILURES			20

typedef struct
{
	qboolean	active;
	qboolean	shouldreject;
	netadr_t	adr;
	int			num_failures;
	float		last_update;
	float		failure_times[MAX_RCON_FAILURES];
} rcon_failure_t;

static rcon_failure_t	g_rgRconFailures[MAX_RCON_FAILURES_STORAGE];

/*
==================
SV_ResetRcon_f

==================
*/
void SV_ResetRcon_f( void )
{
	Q_memset( g_rgRconFailures, 0, sizeof( g_rgRconFailures ) );
}

/*
==================
SV_AddFailedRcon

==================
*/
void SV_AddFailedRcon( netadr_t *adr )
{
	int		i;
	int		best;
	float	best_update;
	float	time;
	qboolean	found;
	rcon_failure_t	*r;
	int		failed;

	best = 0;
	best_update = -99999.0f;
	found = FALSE;

	sv_rcon_minfailures.value = (int)sv_rcon_minfailures.value;
	sv_rcon_maxfailures.value = (int)sv_rcon_maxfailures.value;
	sv_rcon_minfailuretime.value = (int)sv_rcon_minfailuretime.value;

	if ( sv_rcon_minfailures.value < 1 )
		Cvar_SetValue( "sv_rcon_minfailures", 1 );
	if ( sv_rcon_minfailures.value > 20 )
		Cvar_SetValue( "sv_rcon_minfailures", 20 );
	if ( sv_rcon_maxfailures.value < 1 )
		Cvar_SetValue( "sv_rcon_maxfailures", 1 );
	if ( sv_rcon_maxfailures.value > 20 )
		Cvar_SetValue( "sv_rcon_maxfailures", 20 );
	if ( sv_rcon_maxfailures.value < sv_rcon_minfailures.value )
	{
		float	temp;

		temp = sv_rcon_maxfailures.value;
		Cvar_SetValue( "sv_rcon_maxfailures", sv_rcon_minfailures.value );
		Cvar_SetValue( "sv_rcon_minfailures", temp );
	}
	if ( sv_rcon_minfailuretime.value < 1 )
		Cvar_SetValue( "sv_rcon_minfailuretime", 1 );

	// Find an empty slot or the matching address
	for ( i = 0; i < MAX_RCON_FAILURES_STORAGE; i++ )
	{
		r = &g_rgRconFailures[i];
		if ( !r->active )
			break;

		if ( NET_CompareAdr( r->adr, *adr ) )
		{
			found = TRUE;
			break;
		}

		time = realtime - r->last_update;
		if ( time > best_update )
		{
			best = i;
			best_update = time;
		}
	}

	// If no match found, take the oldest entry for usage
	if ( i >= MAX_RCON_FAILURES_STORAGE )
		r = &g_rgRconFailures[best];

	if ( !r )
		return;

	if ( found && r->shouldreject )
		return;

	r->active = TRUE;
	r->shouldreject = FALSE;
	r->last_update = realtime;
	r->adr = *adr;

	if ( !found )
		r->num_failures = 0;

	if ( r->num_failures >= sv_rcon_maxfailures.value )
	{
		for ( i = 1; i < sv_rcon_maxfailures.value; i++ )
			r->failure_times[i - 1] = r->failure_times[i];
		r->num_failures--;
	}

	r->failure_times[r->num_failures] = realtime;
	r->num_failures++;

	failed = 0;
	for ( i = 0; i < r->num_failures; i++ )
	{
		if ( ( time = realtime - r->failure_times[i] ) < sv_rcon_minfailuretime.value )
			failed++;
	}

	if ( failed >= sv_rcon_minfailures.value )
	{
		Con_Printf( "User %s will be banned for rcon hacking\n", NET_AdrToString( *adr ) );
		r->shouldreject = TRUE;
	}
}

/*
==================
SV_CheckRconFailure

==================
*/
qboolean SV_CheckRconFailure( netadr_t *adr )
{
	int		i;
	rcon_failure_t	*r;

	for ( i = 0; i < MAX_RCON_FAILURES_STORAGE; i++ )
	{
		r = &g_rgRconFailures[i];
		if ( !r->active )
			continue;

		if ( !NET_CompareAdr( *adr, r->adr ) )
			continue;

		if ( r->shouldreject )
			return TRUE;
	}

	return FALSE;
}

/*
===============
SV_Rcon_Validate

===============
*/
int SV_Rcon_Validate( void )
{
	if ( Cmd_Argc() < 3 )
		return 1;

	if ( !Q_strlen( rcon_password.string ) )
		return 1;

	if ( sv_rcon_banpenalty.value < 0 )
		Cvar_SetValue( "sv_rcon_banpenalty", 0 );

	if ( SV_CheckRconFailure( &net_from ) )
	{
		Con_Printf( "Banning %s for rcon hacking attempts\n", NET_AdrToString( net_from ) );
		Cbuf_AddText( va( "addip %i %s\n", (int)sv_rcon_banpenalty.value, NET_BaseAdrToString( net_from ) ) );
		return 3;
	}

	if ( !SV_CheckChallenge( &net_from, Q_atoi( Cmd_Argv( 1 ) ) ) )
		return 2;

	if ( Q_strcmp( Cmd_Argv( 2 ), rcon_password.string ) )
	{
		SV_AddFailedRcon( &net_from );
		return 1;
	}

	return 0;
}

/*
===============
SV_Rcon

A client issued an rcon command.
Shift down the remaining args
Redirect all printfs
===============
*/
void SV_Rcon( netadr_t *net_from )
{
	int		invalid;
	char	remaining[1024];
	char	rcon_buff[512];
	int		len;
	char	*data;

	data = NULL;

	invalid = SV_Rcon_Validate();

	len = net_message.cursize - Q_strlen( "rcon" );
	if ( len <= 0 )
		return;
	if ( len >= 511 )
		return;

	Q_memcpy( rcon_buff, &net_message.data[Q_strlen( "rcon" )], len );
	rcon_buff[len] = 0;

	if ( invalid )
	{
		Con_Printf( "Bad Rcon from %s:\n%s\n", NET_AdrToString( *net_from ), rcon_buff );
		Log_Printf( "Bad Rcon: \"%s\" from \"%s\"\n", rcon_buff, NET_AdrToString( *net_from ) );
	}

	if ( !invalid )
	{
		Con_Printf( "Rcon from %s:\n%s\n", NET_AdrToString( *net_from ), rcon_buff );
		Log_Printf( "Rcon: \"%s\" from \"%s\"\n", rcon_buff, NET_AdrToString( *net_from ) );
	}

	SV_BeginRedirect( RD_PACKET, net_from );

	if ( invalid )
	{
		switch ( invalid )
		{
		default:
			if ( strlen( rcon_password.string ) > 0 )
				Con_Printf( "Bad rcon_password.\n" );
			else
				Con_Printf( "Bad rcon_password.\nNo password set for this server.\n" );
			break;

		case 2:
			Con_Printf( "Bad rcon_password.\n" );
			break;
		}
	}
	else
	{
		char	*data;

		data = rcon_buff;
		data = COM_Parse( data );
		data = COM_Parse( data );
		data = COM_Parse( data );

		if ( !data )
		{
			Con_Printf( "Empty rcon\n" );
			return;
		}

		Q_strncpy( remaining, data, sizeof( remaining ) - 1 );
		remaining[sizeof( remaining ) - 1] = 0;

		Cmd_ExecuteString( remaining, src_command );
	}

	SV_EndRedirect();
}

/*
=================
SV_ConnectionlessPacket

A connectionless packet has four leading 0xff
characters to distinguish it from a game channel.
Clients that are in the game can still send
connectionless packets.
=================
*/
void SV_ConnectionlessPacket( void )
{
	char	*args;
	char	*s;
	char	*c;

	MSG_BeginReading();
	MSG_ReadLong();		// skip the -1 marker

	args = s = MSG_ReadStringLine();
	Cmd_TokenizeString( s );

	c = Cmd_Argv( 0 );

	if ( !Q_strcmp( c, "ping" ) || ( c[0] == A2A_PING && ( c[1] == 0 || c[1] == '\n' ) ) )
	{
		SVC_Ping();
	}
	else if ( c[0] == A2A_ACK && ( c[1] == 0 || c[1] == '\n' ) )
	{
		Con_Printf( "A2A_ACK from %s\n", NET_AdrToString( net_from ) );
	}
	else if ( c[0] == A2A_GETCHALLENGE || c[0] == A2S_INFO || c[0] == A2S_PLAYER || c[0] == A2S_RULES ||
		c[0] == S2A_LOGSTRING || c[0] == M2S_REQUESTRESTART || c[0] == M2A_CHALLENGE )
	{
		return;
	}
	else if ( !Q_stricmp( c, "log" ) )
	{
		if ( sv_logrelay.value && args && Q_strlen( args ) > 4 )
		{
			s = &args[Q_strlen( "log " )];
			if ( s && s[0] )
				Con_Printf( "%s\n", s );
		}
	}
	else if ( !Q_strcmp( c, "getchallenge" ) )
	{
		SVC_GetChallenge();
	}
	else if ( !Q_stricmp( c, "challenge" ) )
	{
		SVC_ServiceChallenge();
	}
	else if ( !Q_strcmp( c, "connect" ) )
	{
		SV_ConnectClient();
	}
	else if ( !Q_strcmp( c, "pstat" ) )
	{
		if ( g_modfuncs.m_pfnPlayerStatus )
			g_modfuncs.m_pfnPlayerStatus( net_message.data, net_message.cursize );
	}
	else if ( !Q_strcmp( c, "rcon" ) )
	{
		SV_Rcon( &net_from );
	}
	else
	{
		SVC_GameDllQuery( args );
	}
}

/*
=================
SV_CheckRate

Clamp rate based on server cvars
=================
*/
void SV_CheckRate( client_t *cl )
{
	if ( sv_maxrate.value > 0 && cl->netchan.rate > sv_maxrate.value )
		cl->netchan.rate = min( sv_maxrate.value, 20000.0f );

	if ( sv_minrate.value > 0 && cl->netchan.rate < sv_minrate.value )
		cl->netchan.rate = max( sv_minrate.value, 1000.0f );
}

/*
=================
SV_ProcessFile

=================
*/
void SV_ProcessFile( client_t *cl, char *filename )
{
	customization_t	*pList;
	resource_t	*resource;
	resource_t	*next;
	unsigned char	md5[16];
	qboolean	bFound;
	qboolean	bNoError;

	bFound = FALSE;
	bNoError = TRUE;

	if ( filename[0] != '!' )
	{
		Con_Printf( "Ignoring non-customization file upload of %s\n", filename );
		return;
	}

	COM_HexConvert( filename + 4, 32, md5 );

	for ( resource = cl->resourcesneeded.pNext; resource != &cl->resourcesneeded; resource = next )
	{
		next = resource->pNext;
		if ( Q_memcmp( resource->rgucMD5_hash, md5, 16 ) )
			continue;

		bFound = TRUE;
		break;
	}

	if ( !bFound )
	{
		Con_Printf( "SV_ProcessFile:  Unrequested decal\n" );
		return;
	}

	if ( resource->nDownloadSize != cl->netchan.tempbuffersize )
	{
		Con_Printf( "SV_ProcessFile:  Downloaded %i bytes for purported %i byte file\n", cl->netchan.tempbuffersize, resource->nDownloadSize );
		return;
	}

	bFound = FALSE;

	HPAK_AddLump( TRUE, "custom.hpk", resource, cl->netchan.tempbuffer, NULL );

	resource->ucFlags &= ~RES_WASMISSING;
	SV_MoveToOnHandList( resource );

	pList = cl->customdata.pNext;
	while ( pList )
	{
		if ( !Q_memcmp( pList->resource.rgucMD5_hash, resource->rgucMD5_hash, 16 ) )
		{
			bFound = TRUE;
			break;
		}
		pList = pList->pNext;
	}

	if ( !bFound )
	{
		bNoError = COM_CreateCustomization( &cl->customdata, resource, -1, FCUST_FROMHPAK | FCUST_WIPEDATA | RES_CUSTOM, NULL, NULL );
		if ( !bNoError )
			Con_Printf( "Error parsing custom decal from %s\n", cl->name );
	}
	else
	{
		Con_DPrintf( "Duplicate resource received and ignored.\n" );
	}
}

/*
=================
SV_FilterPacket

=================
*/
qboolean SV_FilterPacket( void )
{
	int		i, j;
	unsigned	in;

	in = *(unsigned *)net_from.ip;

	for ( i = numipfilters - 1; i >= 0; i-- )
	{
		if ( ipfilters[i].compare != 0xffffffff && ipfilters[i].banEndTime != 0.0f && ipfilters[i].banEndTime <= realtime )
		{
			// remove the expired filter
			for ( j = i + 1; j < numipfilters; j++ )
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			continue;
		}

		if ( ( in & ipfilters[i].mask ) == ipfilters[i].compare )
			return (qboolean)sv_filterban.value;
	}

	return sv_filterban.value == 0.0f;
}

/*
=================
SV_SendBan

=================
*/
void SV_SendBan( void )
{
	char	szMessage[64];

	_snprintf( szMessage, sizeof( szMessage ), "You have been banned from this server.\n" );

	SZ_Clear( &net_message );
	MSG_WriteLong( &net_message, -1 );
	MSG_WriteByte( &net_message, A2A_PRINT );
	MSG_WriteString( &net_message, szMessage );
	NET_SendPacket( NS_SERVER, net_message.cursize, net_message.data, net_from );
	SZ_Clear( &net_message );
}

/*
=================
SV_ReadPackets

=================
*/
void SV_ReadPackets( void )
{
	int		i;
	client_t	*cl;
	qboolean	file;

	file = FALSE;

	while ( NET_GetPacket( NS_SERVER ) )
	{
		if ( SV_FilterPacket() )
		{
			SV_SendBan();
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if ( *(int *)net_message.data == -1 )
		{
			if ( !CheckIP( net_from ) )
			{
				if ( sv_logblocks.value )
					Log_Printf( "Traffic from %s was blocked for exceeding rate limits\n", NET_AdrToString( net_from ) );
				continue;
			}

			ISteamMasterServerUpdater_HandleIncomingPacket( net_message.data, net_message.cursize, ntohl( *(unsigned long *)&net_from.ip[0] ), ntohs( net_from.port ) );
			SV_ConnectionlessPacket();
			continue;
		}

		// check for packets from connected clients
		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->connected && !cl->active && !cl->spawned )
				continue;

			if ( !NET_CompareAdr( net_from, cl->netchan.remote_address ) )
				continue;

			if ( Netchan_Process( &cl->netchan ) )
			{
				if ( svs.maxclients == 1 )
					cl->send_message = TRUE;	// reply at end of frame

				if ( !cl->active || !cl->spawned || !cl->fully_connected )
					cl->send_message = TRUE;

				SV_ExecuteClientMessage( cl );
				gGlobalVariables.frametime = host_frametime;
			}

			// Fragmentation/reassembly sending takes priority over all game messages, want this in the future?
			if ( Netchan_IncomingReady( &cl->netchan ) )
			{
				if ( Netchan_CopyNormalFragments( &cl->netchan ) )
				{
					MSG_BeginReading();
					SV_ExecuteClientMessage( cl );
				}

				if ( Netchan_CopyFileFragments( &cl->netchan ) )
				{
					host_client = cl;
					SV_ProcessFile( cl, cl->netchan.incomingfilename );
				}
			}
		}

		if ( i != svs.maxclientslimit )
			continue;
	}
}

/*
==================
SV_CheckTimeouts

If a packet has not been received from a client in sv_timeout.value
seconds, drop the conneciton.

When a client is normally dropped, the client_t goes into a zombie state
for a few seconds to make sure any final reliable message gets resent
if necessary
==================
*/
void SV_CheckTimeouts( void )
{
	int		i;
	client_t	*cl;
	float	droptime;

	droptime = realtime - sv_timeout.value;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->fakeclient )
			continue;

		if ( ( cl->connected || cl->active || cl->spawned ) && cl->netchan.last_received < droptime )
		{
			SV_BroadcastPrintf( "%s timed out\n", cl->name );
			SV_DropClient( cl, FALSE, "Timed out" );
		}
	}
}

/*
===================
SV_CalcPing

===================
*/
int SV_CalcPing( client_t *cl )
{
	float	ping;
	int		i;
	int		count;
	int		back;
	client_frame_t	*frame;
	int		idx;

	if ( cl->fakeclient )
		return 0;

	idx = cl - svs.clients;
	ping = 0;
	count = 0;

	back = min( SV_UPDATE_BACKUP / 2, 16 );

	for ( i = 0; i < back; i++ )
	{
		frame = &cl->frames[( cl->netchan.incoming_acknowledged - i - 1 ) & SV_UPDATE_MASK];
		if ( frame->ping_time > 0 )
		{
			ping += frame->ping_time;
			count++;
		}
	}

	if ( !count )
		return 0;

	if ( ( ping /= count ) < 0 )
		ping = 0;

	return ping * 1000;
}

/*
===================
SV_FullClientUpdate

Writes all update values to a bf_write
===================
*/
void SV_FullClientUpdate( client_t *cl, sizebuf_t *sb )
{
	int		i;
	char	info[MAX_INFO_STRING];
	char	digest[16];
	MD5Context_t	ctx;

	i = cl - svs.clients;

	Q_strncpy( info, cl->userinfo, sizeof( info ) - 1 );
	info[sizeof( info ) - 1] = 0;
	Info_RemovePrefixedKeys( info, '_' );

	MSG_WriteByte( sb, svc_updateuserinfo );
	MSG_WriteByte( sb, i );
	MSG_WriteLong( sb, cl->userid );
	MSG_WriteString( sb, info );

	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char *)cl->hashedcdkey, sizeof( cl->hashedcdkey ) );
	MD5Final( (unsigned char *)digest, &ctx );

	MSG_WriteBuf( sb, sizeof( digest ), digest );
}

/*
=============
SV_EmitEvents

=============
*/
void SV_EmitEvents( client_t *cl, packet_entities_t *pack, sizebuf_t *msg )
{
	int		i, ev;
	event_state_t	*es;
	event_info_t	*info;
	entity_state_t	*state;
	int		ev_count;
	int		etofind;
	int		c;
	event_args_t	nullargs;

	ev_count = 0;

	Q_memset( &nullargs, 0, sizeof( nullargs ) );

	es = &cl->events;

	// Count events
	for ( ev = 0; ev < MAX_EVENT_QUEUE; ev++ )
	{
		info = &es->ei[ev];
		if ( !info->index )
			continue;
		ev_count++;
	}

	// Nothing to send
	if ( !ev_count )
		return;

	if ( ev_count > 31 )
		ev_count = 31;

	for ( ev = 0; ev < MAX_EVENT_QUEUE; ev++ )
	{
		info = &es->ei[ev];
		if ( !info->index )
			continue;

		etofind = info->entity_index;

		// Find state info for the entity
		for ( i = 0; i < pack->num_entities; i++ )
		{
			state = &pack->entities[i];
			if ( state->number == etofind )
				break;
		}

		if ( i >= pack->num_entities )
		{
			// couldn't find
			info->packet_index = pack->num_entities;
			info->args.entindex = etofind;
		}
		else
		{
			info->packet_index = i;
			info->args.ducking = 0;
			if ( !( info->args.flags & FEVENT_ORIGIN ) )
				VectorClear( info->args.origin );
			if ( !( info->args.flags & FEVENT_ANGLES ) )
				VectorClear( info->args.angles );
		}
	}

	MSG_WriteByte( msg, svc_event );
	MSG_StartBitWriting( msg );
	MSG_WriteBits( ev_count, 5 );

	c = 0;
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		info = &es->ei[i];
		if ( !info->index )
		{
			info->packet_index = -1;
			info->entity_index = -1;
			continue;
		}

		// Only send if there's room
		if ( c < ev_count )
		{
			MSG_WriteBits( info->index, 10 );

			if ( info->packet_index == -1 )
			{
				MSG_WriteBits( 0, 1 );
			}
			else
			{
				MSG_WriteBits( 1, 1 );
				MSG_WriteBits( info->packet_index, 11 );

				if ( Q_memcmp( &nullargs, &info->args, sizeof( event_args_t ) ) )
				{
					MSG_WriteBits( 1, 1 );
					DELTA_WriteDelta( (byte *)&nullargs, (byte *)&info->args, TRUE, g_peventdelta, NULL );
				}
				else
				{
					MSG_WriteBits( 0, 1 );
				}
			}

			if ( info->fire_time == 0.0 )
			{
				MSG_WriteBits( 0, 1 );
			}
			else
			{
				MSG_WriteBits( 1, 1 );
				MSG_WriteBits( info->fire_time * 100.0, 16 );
			}
		}

		info->index = 0;
		info->packet_index = -1;
		info->entity_index = -1;
		c++;
	}

	MSG_EndBitWriting( msg );
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

int		fatbytes;
byte	fatpvs[MAX_MAP_LEAFS / 8];

/*
=============
SV_AddToFatPVS

=============
*/
void SV_AddToFatPVS( vec_t *org, mnode_t *node )
{
	int		i;
	byte	*pvs;
	mplane_t	*plane;
	float	d;

	while ( 1 )
	{
		// if this is a leaf, accumulate the pvs bits
		if ( node->contents < 0 )
		{
			if ( node->contents != CONTENTS_SOLID )
			{
				pvs = Mod_LeafPVS( (mleaf_t *)node, sv.worldmodel );
				for ( i = 0; i < fatbytes; i++ )
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if ( d > 8 )
			node = node->children[0];
		else if ( d < -8 )
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS( org, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPVS( float *org )
{
	fatbytes = ( sv.worldmodel->numleafs + 31 ) >> 3;
	Q_memset( fatpvs, 0, fatbytes );
	SV_AddToFatPVS( org, sv.worldmodel->nodes );
	return fatpvs;
}

int		fatpasbytes;
byte	fatpas[MAX_MAP_LEAFS / 8];

/*
=============
SV_AddToFatPAS

=============
*/
void SV_AddToFatPAS( vec_t *org, mnode_t *node )
{
	int		i;
	byte	*pas;
	mplane_t	*plane;
	float	d;

	while ( 1 )
	{
		// if this is a leaf, accumulate the pas bits
		if ( node->contents < 0 )
		{
			if ( node->contents != CONTENTS_SOLID )
			{
				int		leafnum;

				leafnum = (mleaf_t *)node - sv.worldmodel->leafs;
				pas = CM_LeafPAS( leafnum );
				for ( i = 0; i < fatpasbytes; i++ )
					fatpas[i] |= pas[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct( org, plane->normal ) - plane->dist;
		if ( d > 8 )
			node = node->children[0];
		else if ( d < -8 )
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPAS( org, node->children[0] );
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPAS

Calculates a PAS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPAS( float *org )
{
	fatpasbytes = ( sv.worldmodel->numleafs + 31 ) >> 3;
	Q_memset( fatpas, 0, fatpasbytes );
	SV_AddToFatPAS( org, sv.worldmodel->nodes );
	return fatpas;
}

/*
=============
SV_PointLeafnum

=============
*/
int SV_PointLeafnum( vec_t *p )
{
	mleaf_t	*pLeaf;

	pLeaf = Mod_PointInLeaf( p, sv.worldmodel );
	if ( pLeaf )
		return pLeaf - sv.worldmodel->leafs;

	return 0;
}

/*
=============
TRACE_DELTA

=============
*/
void TRACE_DELTA( char *fmt, ... )
{
}

typedef struct
{
	int			*numbase;
	int			num;
	qboolean	remove;
	qboolean	custom;
	qboolean	newbl;
	int			newblindex;
	qboolean	full;
	int			offset;
} deltacallback_t;

deltacallback_t	g_svdeltacallback;

/*
=============
SV_SetCallback

=============
*/
void SV_SetCallback( int num, qboolean remove, qboolean custom, int *numbase, qboolean full, int offset )
{
	g_svdeltacallback.num = num;
	g_svdeltacallback.remove = remove;
	g_svdeltacallback.custom = custom;
	g_svdeltacallback.numbase = numbase;
	g_svdeltacallback.newbl = FALSE;
	g_svdeltacallback.newblindex = 0;
	g_svdeltacallback.full = full;
	g_svdeltacallback.offset = offset;
}

/*
=============
SV_SetNewInfo

=============
*/
void SV_SetNewInfo( int newblindex )
{
	g_svdeltacallback.newbl = TRUE;
	g_svdeltacallback.newblindex = newblindex;
}

/*
=============
SV_WriteDeltaHeader

=============
*/
void SV_WriteDeltaHeader( int num, qboolean remove, qboolean custom, int *numbase, qboolean newbl, int newblindex, qboolean full, int offset )
{
	int		delta;

	delta = num - *numbase;

	if ( full )
	{
		if ( delta == 1 )
		{
			MSG_WriteBits( 1, 1 );
		}
		else
		{
			MSG_WriteBits( 0, 1 );

			if ( delta > 0 && delta <= 63 )
			{
				MSG_WriteBits( 0, 1 );
				MSG_WriteBits( delta, 6 );
			}
			else
			{
				MSG_WriteBits( 1, 1 );
				MSG_WriteBits( num, 11 );
			}
		}
	}
	else
	{
		MSG_WriteBits( remove != 0, 1 );

		if ( delta > 0 && delta <= 63 )
		{
			MSG_WriteBits( 0, 1 );
			MSG_WriteBits( delta, 6 );
		}
		else
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( num, 11 );
		}
	}

	*numbase = num;

	if ( remove )
		return;

	MSG_WriteBits( custom != 0, 1 );

	if ( sv.instance_baselines->number )
	{
		if ( newbl )
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( newblindex, 6 );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
		}
	}

	if ( full && !newbl )
	{
		if ( offset )
		{
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( offset, 6 );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
		}
	}
}

/*
=============
SV_InvokeCallback

=============
*/
void SV_InvokeCallback( void )
{
	SV_WriteDeltaHeader( g_svdeltacallback.num, g_svdeltacallback.remove, g_svdeltacallback.custom,
		g_svdeltacallback.numbase, g_svdeltacallback.newbl, g_svdeltacallback.newblindex,
		g_svdeltacallback.full, g_svdeltacallback.offset );
}

/*
=============
SV_FindBestBaseline

=============
*/
int SV_FindBestBaseline( int index, entity_state_t **baseline, entity_state_t *to, int num, qboolean custom )
{
	int		bestbitnumber, bitnumber;
	int		bestfound;
	int		i;

	bestfound = index;
	i = index - 1;

	if ( custom )
		bestbitnumber = DELTA_TestDelta( (byte *)*baseline, (byte *)&to[index], g_pcustomentitydelta );
	else
		bestbitnumber = DELTA_TestDelta( (byte *)*baseline, (byte *)&to[index], SV_IsPlayerIndex( num ) ? g_pplayerdelta : g_pentitydelta );

	bestbitnumber -= 6;

	while ( bestbitnumber > 0 && i >= 0 && ( index - i ) < 63 )
	{
		if ( to[index].entityType == to[i].entityType )
		{
			if ( custom )
				bitnumber = DELTA_TestDelta( (byte *)&to[i], (byte *)&to[index], g_pcustomentitydelta );
			else
				bitnumber = DELTA_TestDelta( (byte *)&to[i], (byte *)&to[index], SV_IsPlayerIndex( num ) ? g_pplayerdelta : g_pentitydelta );

			if ( bitnumber < bestbitnumber )
			{
				bestbitnumber = bitnumber;
				bestfound = i;
			}
		}

		i--;
	}

	i = index - bestfound;
	if ( i )
		*baseline = &to[bestfound];

	return i;
}

/*
=============
SV_CreatePacketEntities

Computes either a compressed, or uncompressed delta buffer for the client.
Returns the size IN BITS of the message buffer created.
=============
*/
int SV_CreatePacketEntities( sv_delta_t type, client_t *client, packet_entities_t *to, sizebuf_t *msg )
{
	edict_t	*ent;
	client_frame_t	*fromframe;
	packet_entities_t	*from;
	int		oldindex, newindex;
	int		oldnum, newnum;
	int		oldmax;
	qboolean	custom;
	int		offset;
	int		numbase;

	custom = FALSE;
	numbase = 0;

	// See if this is a full update
	if ( type == sv_packet_delta )
	{
		// This is the frame that we are going to delta update from
		fromframe = &client->frames[client->delta_sequence & SV_UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;

		MSG_WriteByte( msg, svc_deltapacketentities );	// This is a delta
		MSG_WriteShort( msg, to->num_entities );		// This is how many ents are in the new packet.
		MSG_WriteByte( msg, client->delta_sequence );	// This is the sequence # that we are updating from.
	}
	else
	{
		oldmax = 0;	// no delta update
		from = NULL;

		MSG_WriteByte( msg, svc_packetentities );		// Just a packet update.
		MSG_WriteShort( msg, to->num_entities );		// This is the # of entities we are sending.
	}

	newindex = 0;
	oldindex = 0;

	MSG_StartBitWriting( msg );

	while ( newindex < to->num_entities || oldindex < oldmax )
	{
		newnum = ( newindex >= to->num_entities ) ? 9999 : to->entities[newindex].number;
		oldnum = ( oldindex >= oldmax ) ? 9999 : from->entities[oldindex].number;

		if ( newnum == oldnum )
		{
			// delta update from old position
			// because the force parm is false, this will not result
			// in any bytes being emitted if the entity has not changed at all
			// note that players are always 'newentities', this updates their oldorigin always
			// and prevents warping
			custom = ( to->entities[newindex].entityType & ENTITY_BEAM ) != 0;

			SV_SetCallback( newnum, FALSE, custom, &numbase, FALSE, 0 );

			if ( custom )
				DELTA_WriteDelta( (byte *)&from->entities[oldindex], (byte *)&to->entities[newindex], FALSE, g_pcustomentitydelta, SV_InvokeCallback );
			else
				DELTA_WriteDelta( (byte *)&from->entities[oldindex], (byte *)&to->entities[newindex], FALSE, SV_IsPlayerIndex( newnum ) ? g_pplayerdelta : g_pentitydelta, SV_InvokeCallback );

			oldindex++;
			newindex++;
			continue;
		}

		if ( newnum < oldnum )
		{
			entity_state_t	*baseline;

			// this is a new entity, send it from the baseline
			ent = EDICT_NUM( newnum );

			TRACE_DELTA( "new from baseline %i\n", newnum );

			custom = ( to->entities[newindex].entityType & ENTITY_BEAM ) != 0;

			SV_SetCallback( newnum, FALSE, custom, &numbase, from == NULL, 0 );

			baseline = &sv.baselines[newnum];

			if ( sv_instancedbaseline.value && sv.instance_baselines->number && newnum > sv_lastnum )
			{
				int		i;

				for ( i = 0; i < sv.instance_baselines->number; i++ )
				{
					if ( sv.instance_baselines->classname[i] == ent->v.classname )
					{
						SV_SetNewInfo( i );
						baseline = &sv.instance_baselines->baseline[i];
						break;
					}
				}
			}
			else
			{
				if ( !from )
				{
					offset = SV_FindBestBaseline( newindex, &baseline, to->entities, newnum, custom );
					if ( offset )
						SV_SetCallback( newnum, FALSE, custom, &numbase, TRUE, offset );
				}
			}

			if ( custom )
				DELTA_WriteDelta( (byte *)baseline, (byte *)&to->entities[newindex], TRUE, g_pcustomentitydelta, SV_InvokeCallback );
			else
				DELTA_WriteDelta( (byte *)baseline, (byte *)&to->entities[newindex], TRUE, SV_IsPlayerIndex( newnum ) ? g_pplayerdelta : g_pentitydelta, SV_InvokeCallback );

			newindex++;
			continue;
		}

		if ( newnum > oldnum )
		{
			// the old entity isn't present in the new message
			custom = FALSE;

			SV_WriteDeltaHeader( oldnum, TRUE, custom, &numbase, FALSE, 0, FALSE, 0 );

			TRACE_DELTA( "remove%i\n", oldnum );

			oldindex++;
			continue;
		}
	}

	// No more entities.. (end of packet entities)
	MSG_WriteBits( 0, 16 );
	MSG_EndBitWriting( msg );

	return msg->cursize;
}

/*
=============
SV_EmitPacketEntities

=============
*/
void SV_EmitPacketEntities( client_t *client, packet_entities_t *to, sizebuf_t *msg )
{
	if ( client->delta_sequence != -1 )
		SV_CreatePacketEntities( sv_packet_delta, client, to, msg );
	else
		SV_CreatePacketEntities( sv_packet_nodelta, client, to, msg );
}

/*
=============
SV_ShouldUpdatePing

=============
*/
qboolean SV_ShouldUpdatePing( client_t *client )
{
	if ( client->proxy )
	{
		if ( realtime >= client->nextping )
		{
			client->nextping = realtime + 2.0;
			return TRUE;
		}
		return FALSE;
	}

	SV_CalcPing( client );
	return client->lastcmd.buttons & IN_SCORE;
}

/*
=============
SV_HasEventsInQueue

=============
*/
qboolean SV_HasEventsInQueue( client_t *client )
{
	int		i;
	event_state_t	*es;
	event_info_t	*ei;

	if ( !client )
		return FALSE;

	es = &client->events;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if ( ei->index )
			return TRUE;
	}

	return FALSE;
}

/*
=============
SV_GetNetInfo

=============
*/
void SV_GetNetInfo( client_t *client, int *ping, int *packet_loss )
{
	static unsigned short	lastping[32];
	static byte				lastloss[32];
	int		i;

	i = client - svs.clients;

	if ( realtime >= client->nextping )
	{
		client->nextping = realtime + 2.0;
		lastping[i] = SV_CalcPing( client );
		lastloss[i] = client->packet_loss;
	}

	*ping = lastping[i];
	*packet_loss = lastloss[i];
}

/*
=============
SV_CheckVisibility

=============
*/
int SV_CheckVisibility( edict_t *entity, unsigned char *pset )
{
	int		i;
	qboolean	visible;
	edict_t	*e;

	if ( !pset )
		return 1;

	if ( entity->headnode >= 0 )
	{
		int		leaf;

		i = 0;
		do
		{
			leaf = entity->leafnums[i];
			if ( leaf == -1 )
				break;

			if ( pset[leaf >> 3] & ( 1 << ( leaf & 7 ) ) )
				return 1;

			i++;
			if ( i >= MAX_ENT_LEAFS )
				break;
		} while ( 1 );

		// too many leafs for individual check, go by headnode
		visible = CM_HeadnodeVisible( &sv.worldmodel->nodes[entity->headnode], pset, &leaf );
		if ( !visible )
			return 0;

		// add it to the leaf list
		e = entity;
		e->leafnums[entity->num_leafs] = leaf;
		e->num_leafs = ( entity->num_leafs + 1 ) % MAX_ENT_LEAFS;
		return 2;
	}
	else
	{
		// we use num_leafs as a sentinel
		for ( i = 0; i < entity->num_leafs; i++ )
		{
			if ( pset[entity->leafnums[i] >> 3] & ( 1 << ( entity->leafnums[i] & 7 ) ) )
				break;
		}

		visible = i < entity->num_leafs;
		if ( !visible )
			return 0;

		return 1;
	}
}

/*
=============
SV_EmitPings

=============
*/
void SV_EmitPings( client_t *client, sizebuf_t *msg )
{
	client_t	*cl;
	int		i;
	int		ping;
	int		packet_loss;

	MSG_WriteByte( msg, svc_pings );
	MSG_StartBitWriting( msg );

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active )
			continue;

		SV_GetNetInfo( cl, &ping, &packet_loss );

		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( i, 5 );
		MSG_WriteBits( ping, 12 );
		MSG_WriteBits( packet_loss, 7 );
	}

	MSG_WriteBits( 0, 1 );
	MSG_EndBitWriting( msg );
}

/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient( client_t *client, sizebuf_t *msg )
{
	full_packet_entities_t	fullpack;
	packet_entities_t	*pack;
	edict_t	*ent;
	client_frame_t	*frame;
	qboolean	sendping = FALSE;
	int		e;
	byte	*pvs = NULL;
	byte	*pas = NULL;
	int		ev_count = 0;
	int		skipents = 0;
	byte	*pSet = NULL;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.outgoing_sequence & SV_UPDATE_MASK];

	// find the client's PVS
	ent = client->edict;
	gEntityInterface.pfnSetupVisibility( (edict_t *)client->pViewEntity, ent, &pvs, &pas );

	SV_ClearPacketEntities( frame );

	// send over all entities (excpet the client) that touch the pvs
	pack = &frame->entities;
	fullpack.num_entities = 0;

	sendping = SV_ShouldUpdatePing( client );

	skipents = client->lw != 0;
	pSet = pvs;

	for ( e = 1; e < sv.num_edicts; e++ )
	{
		int		player = 0;
		client_t	*client = NULL;
		edict_t	*ent = &sv.edicts[e];
		entity_state_t	*state;

		player = e >= 1 && e <= svs.maxclients;

		if ( player )
		{
			client = &svs.clients[e - 1];
			if ( !client->active && !client->spawned )
				continue;
			if ( client->proxy )
				continue;
		}

		// Ran out of space?
		if ( fullpack.num_entities >= MAX_PACKET_ENTITIES )
		{
			Con_DPrintf( "Too many entities in visible packet list.\n" );
			break;
		}

		state = &fullpack.entities[fullpack.num_entities];

		// Have the game dll fill it in
		if ( gEntityInterface.pfnAddToFullPack( state, e, ent, host_client->edict, skipents, player, pSet ) )
			fullpack.num_entities++;
	}

	// Now copy into packet_entity_t
	SV_AllocPacketEntities( frame, fullpack.num_entities );
	if ( pack->num_entities )
		Q_memcpy( pack->entities, fullpack.entities, pack->num_entities * sizeof( entity_state_t ) );

	// Encode the packet entities as a delta from the
	// last packetentities acknowledged by the client
	SV_EmitPacketEntities( client, pack, msg );
	SV_EmitEvents( client, pack, msg );

	if ( sendping )
		SV_EmitPings( client, msg );
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts( void )
{
	int		e;
	edict_t	*ent;

	for ( e = 1; e < sv.num_edicts; e++ )
	{
		ent = &sv.edicts[e];
		ent->v.effects &= ~EF_MUZZLEFLASH;
		ent->v.effects &= ~EF_NOINTERP;
	}
}

/*
=============
SV_SendClientDatagram

=============
*/
qboolean SV_SendClientDatagram( client_t *client )
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;

	msg.buffername = "Client Datagram";
	msg.data = buf;
	msg.maxsize = sizeof( buf );
	msg.cursize = 0;
	msg.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteByte( &msg, svc_time );
	MSG_WriteFloat( &msg, sv.time );

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage( client, &msg );

	// send over all the objects that are in the PVS
	// this will include clients, a packetentities, and
	// possibly a nails update
	SV_WriteEntitiesToClient( client, &msg );

	// copy the accumulated multicast datagram
	// for this client out to the message
	if ( client->datagram.flags & FSB_OVERFLOWED )
		Con_Printf( "WARNING: datagram overflowed for %s\n", client->name );
	else
		SZ_Write( &msg, client->datagram.data, client->datagram.cursize );

	SZ_Clear( &client->datagram );

	if ( msg.flags & FSB_OVERFLOWED )
	{
		Con_Printf( "WARNING: msg overflowed for %s\n", client->name );
		SZ_Clear( &msg );
	}

	// send the datagram
	Netchan_Transmit( &client->netchan, msg.cursize, buf );

	return TRUE;
}

/*
=======================
SV_UpdateToReliableMessages

=======================
*/
void SV_UpdateToReliableMessages( void )
{
	int		i, j;
	client_t	*client;

	// check for changes to be sent over the reliable streams to all clients
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( !host_client->edict )
			continue;

		if ( host_client->sendinfo && host_client->sendinfo_time <= realtime )
		{
			host_client->sendinfo = FALSE;
			host_client->sendinfo_time = realtime + 1.0;
			SV_ExtractFromUserinfo( host_client );
			SV_FullClientUpdate( host_client, &sv.reliable_datagram );
		}

		// Never send a new user messages to bots
		if ( host_client->fakeclient )
			continue;

		if ( !host_client->active && !host_client->connected )
			continue;

		if ( sv_gpNewUserMsgs )
			SV_SendUserReg( &host_client->netchan.message );
	}

	// Link new user messages to the sent chain
	if ( sv_gpNewUserMsgs )
	{
		if ( !sv_gpUserMsgs )
		{
			sv_gpUserMsgs = sv_gpNewUserMsgs;
		}
		else
		{
			UserMsg	*pMsg;

			pMsg = sv_gpUserMsgs;
			while ( pMsg->next )
				pMsg = pMsg->next;
			pMsg->next = sv_gpNewUserMsgs;
		}

		sv_gpNewUserMsgs = NULL;
	}

	if ( sv.datagram.flags & FSB_OVERFLOWED )
	{
		Con_DPrintf( "sv.datagram overflowed!\n" );
		SZ_Clear( &sv.datagram );
	}

	if ( sv.spectator.flags & FSB_OVERFLOWED )
	{
		Con_DPrintf( "sv.spectator overflowed!\n" );
		SZ_Clear( &sv.spectator );
	}

	// append the broadcast messages to each client messages
	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( client->fakeclient )
			continue;

		if ( !client->active )
			continue;

		if ( client->netchan.message.cursize + sv.reliable_datagram.cursize >= client->netchan.message.maxsize )
			Netchan_CreateFragments( TRUE, &client->netchan, &sv.reliable_datagram );
		else
			SZ_Write( &client->netchan.message, sv.reliable_datagram.data, sv.reliable_datagram.cursize );

		if ( client->datagram.cursize + sv.datagram.cursize < client->datagram.maxsize )
			SZ_Write( &client->datagram, sv.datagram.data, sv.datagram.cursize );
		else
			Con_DPrintf( "Warning:  Ignoring unreliable datagram for %s, would overflow\n", client->name );

		if ( client->proxy && client->datagram.cursize + sv.spectator.cursize < client->datagram.maxsize )
			SZ_Write( &client->datagram, sv.spectator.data, sv.spectator.cursize );
	}

	SZ_Clear( &sv.reliable_datagram );
	SZ_Clear( &sv.datagram );
	SZ_Clear( &sv.spectator );
}

/*
=======================
SV_SkipUpdates

=======================
*/
void SV_SkipUpdates( void )
{
	int		i;
	client_t	*cl;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( ( !cl->active && !cl->connected && !cl->spawned ) || host_client->fakeclient )
			continue;

		cl->skip_message = TRUE;
	}
}

/*
=======================
SV_SendClientMessages

=======================
*/
void SV_SendClientMessages( void )
{
	int		i;
	char	reason_guess[256];

	SV_UpdateToReliableMessages();

	// build individual updates
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( !host_client->active && !host_client->connected && !host_client->spawned ) || host_client->fakeclient )
			continue;

		if ( host_client->skip_message )
		{
			host_client->skip_message = FALSE;
			continue;
		}

		if ( !host_limitlocal.value && host_client->netchan.remote_address.type == NA_LOOPBACK )
			host_client->send_message = TRUE;

		if ( host_client->active && host_client->spawned && host_client->fully_connected && realtime + host_frametime >= host_client->next_messagetime )
			host_client->send_message = TRUE;

		// if the reliable message overflowed, drop the client
		if ( host_client->netchan.message.flags & FSB_OVERFLOWED )
		{
			sprintf( reason_guess, "Reliable channel overflow - %s", host_client->netchan.message.data );
			SZ_Clear( &host_client->netchan.message );
			SZ_Clear( &host_client->datagram );
			SV_BroadcastPrintf( "%s overflowed\n", host_client->name );
			Con_Printf( "WARNING: reliable overflow for %s\n", host_client->name );
			SV_DropClient( host_client, FALSE, reason_guess );
			host_client->send_message = TRUE;
			host_client->netchan.cleartime = 0;	// don't choke this message
		}
		else if ( host_client->send_message )
		{
			// only send messages if the client has sent one
			// and the bandwidth is not choked
			if ( realtime - host_client->netchan.last_received > sv_failuretime.value )
				host_client->send_message = FALSE;
		}

		if ( !host_client->send_message )
			continue;

		if ( !Netchan_CanPacket( &host_client->netchan ) )
		{
			host_client->chokecount++;
			continue;
		}

		host_client->send_message = FALSE;
		host_client->next_messagetime = realtime + host_frametime + host_client->next_messageinterval;

		if ( host_client->active && host_client->spawned && host_client->fully_connected )
			SV_SendClientDatagram( host_client );
		else
			Netchan_Transmit( &host_client->netchan, 0, NULL );
	}

	// Allow game .dll to run code, including unsetting EF_MUZZLEFLASH and EF_NOINTERP on effects fields
	SV_CleanupEnts();
}

/*
=================
SV_ExtractFromUserinfo

Pull specific info from a newly changed userinfo string
into a more C freindly form.
=================
*/
void SV_ExtractFromUserinfo( client_t *cl )
{
	char	*val;
	char	*p;
	int		i;
	client_t	*client;
	int		dupc;
	char	newname[32];
	char	rawname[32];

	dupc = 1;

	// name for C code
	val = (char *)Info_ValueForKey( cl->userinfo, "name" );

	// trim user name
	Q_strncpy( newname, val, sizeof( newname ) - 1 );
	newname[sizeof( newname ) - 1] = 0;

	Q_strcpy( rawname, newname );

	i = 0;
	while ( rawname[i] == '#' )
	{
		rawname[i] = ' ';
		i++;
	}

	TrimSpace( rawname, newname );

	if ( Q_strlen( newname ) <= 0 || !Q_stricmp( newname, "console" ) )
	{
		Info_SetValueForKey( cl->userinfo, "name", "unnamed", MAX_INFO_STRING );
		val = (char *)Info_ValueForKey( cl->userinfo, "name" );
	}
	else if ( Q_strcmp( val, newname ) )
	{
		Info_SetValueForKey( cl->userinfo, "name", newname, MAX_INFO_STRING );
		val = (char *)Info_ValueForKey( cl->userinfo, "name" );
	}

	// check to see if another user by the same name exists
	while ( 1 )
	{
		for ( i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++ )
		{
			if ( !client->active || !client->spawned || client == cl )
				continue;
			if ( !Q_stricmp( client->name, val ) )
				break;
		}

		if ( i >= MAX_CLIENTS )
			break;

		if ( Q_strlen( val ) > sizeof( newname ) - 1 )
			val[28] = 0;

		p = val;
		if ( val[0] == '(' )
		{
			if ( val[2] == ')' )
				p = val + 3;
			else if ( val[3] == ')' )
				p = val + 4;
		}

		_snprintf( newname, sizeof( newname ), "(%d)%-0.*s", dupc++, 28, p );
		Info_SetValueForKey( cl->userinfo, "name", newname, MAX_INFO_STRING );
		val = (char *)Info_ValueForKey( cl->userinfo, "name" );
	}

	gEntityInterface.pfnClientUserInfoChanged( cl->edict, cl->userinfo );

	Q_strncpy( cl->name, Info_ValueForKey( cl->userinfo, "name" ), sizeof( cl->name ) - 1 );
	cl->name[sizeof( cl->name ) - 1] = 0;

	ISteamGameServer_BUpdateUserData( cl->network_userid.m_SteamID, cl->name, 0 );

	// rate command
	val = (char *)Info_ValueForKey( cl->userinfo, "rate" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		if ( i < 1000 )
			i = 1000;
		else if ( i > 20000 )
			i = 20000;
		cl->netchan.rate = i;
	}

	// colors
	val = (char *)Info_ValueForKey( cl->userinfo, "topcolor" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		cl->topcolor = i;
	}
	else
	{
		Con_DPrintf( "topcolor unchanged for %s\n", cl->name );
	}

	val = (char *)Info_ValueForKey( cl->userinfo, "bottomcolor" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		cl->bottomcolor = i;
	}
	else
	{
		Con_DPrintf( "bottomcolor unchanged for %s\n", cl->name );
	}

	// update rate
	val = (char *)Info_ValueForKey( cl->userinfo, "cl_updaterate" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		if ( i < 10 )
			i = 10;
		if ( i > 0 )
			cl->next_messageinterval = 1.0f / i;
	}

	// predict weapons locally
	val = (char *)Info_ValueForKey( cl->userinfo, "cl_lw" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		cl->lw = i != 0;
	}
	else
	{
		cl->lw = 0;
	}

	// lag compensation
	val = (char *)Info_ValueForKey( cl->userinfo, "cl_lc" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		cl->lc = i != 0;
	}
	else
	{
		cl->lc = 0;
	}

	val = (char *)Info_ValueForKey( cl->userinfo, "*hltv" );
	if ( Q_strlen( val ) )
	{
		i = Q_atoi( val );
		cl->proxy = i == TYPE_PROXY;
	}
	else
	{
		cl->proxy = 0;
	}

	SV_CheckUpdateRate( &cl->next_messageinterval );
	SV_CheckRate( cl );
}

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex( const char *name )
{
	int		i;

	if ( !name || !name[0] )
		return 0;

	for ( i = 0; i < MAX_MODELS && sv.model_precache[i]; i++ )
	{
		if ( !Q_strcasecmp( sv.model_precache[i], name ) )
			return i;
	}

	if ( i == MAX_MODELS || !sv.model_precache[i] )
		Sys_Error( "SV_ModelIndex: model %s not precached", name );

	return i;
}

/*
================
SV_AddResource

================
*/
void SV_AddResource( resourcetype_t type, char *name, int size, unsigned char flags, int index )
{
	resource_t	*r;

	r = &sv.resourcelist[sv.num_resources];

	if ( sv.num_resources >= MAX_MODELS + MAX_SOUNDS + MAX_EVENTS )
		Sys_Error( "Too many resources on server." );

	sv.num_resources++;

	r->type = type;
	Q_strncpy( r->szFileName, name, sizeof( r->szFileName ) - 1 );
	r->szFileName[sizeof( r->szFileName ) - 1] = 0;
	r->nDownloadSize = size;
	r->ucFlags = flags;
	r->nIndex = index;
}

/*
================
SV_CreateGenericResources

================
*/
void SV_CreateGenericResources( void )
{
	char	filename[MAX_PATH];
	char	*buffer;
	char	*data;

	COM_StripExtension( sv.modelname, filename );
	COM_DefaultExtension( filename, ".res" );
	COM_FixSlashes( filename );

	buffer = (char *)COM_LoadFile( filename, 5, NULL );
	if ( !buffer )
		return;

	Con_DPrintf( "Precaching from %s\n", filename );
	Con_DPrintf( "----------------------------------\n" );

	sv.num_generic_names = 0;

	data = buffer;
	while ( 1 )
	{
		data = COM_Parse( data );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		if ( Q_strstr( com_token, ".." ) )
		{
			Con_Printf( "Can't precache resource with invalid relative path %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ":" ) )
		{
			Con_Printf( "Can't precache resource with absolute path %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, "\\" ) )
		{
			Con_Printf( "Can't precache resource with invalid relative path %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".cfg" ) )
		{
			Con_Printf( "Can't precache .cfg files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".lst" ) )
		{
			Con_Printf( "Can't precache .lst files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".exe" ) )
		{
			Con_Printf( "Can't precache .exe files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".vbs" ) )
		{
			Con_Printf( "Can't precache .vbs files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".com" ) )
		{
			Con_Printf( "Can't precache .com files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".bat" ) )
		{
			Con_Printf( "Can't precache .bat files:  %s\n", com_token );
			continue;
		}

		if ( Q_strstr( com_token, ".dll" ) )
		{
			Con_Printf( "Can't precache .dll files:  %s\n", com_token );
			continue;
		}

		Q_strncpy( sv.generic_precache_names[sv.num_generic_names], com_token, sizeof( sv.generic_precache_names[0] ) - 1 );
		sv.generic_precache_names[sv.num_generic_names][sizeof( sv.generic_precache_names[0] ) - 1] = 0;
		PF_precache_generic_I( sv.generic_precache_names[sv.num_generic_names] );
		Con_DPrintf( "  %s\n", sv.generic_precache_names[sv.num_generic_names] );
		sv.num_generic_names++;
	}

	Con_DPrintf( "----------------------------------\n" );
	COM_FreeFile( buffer );
}

/*
================
SV_CreateResourceList

Add resources to precache list
================
*/
void SV_CreateResourceList( void )
{
	char	**s;
	int		ffirstsent;
	int		i;
	int		nSize;

	ffirstsent = 0;
	sv.num_resources = 0;

	for ( s = &sv.generic_precache[1], i = 1; *s; s++, i++ )
	{
		if ( svs.maxclients <= 1 )
			nSize = 0;
		else
			nSize = FS_FileSize( *s );

		SV_AddResource( t_generic, *s, nSize, RES_FATALIFMISSING, i );
	}

	for ( s = &sv.sound_precache[1], i = 1; *s; s++, i++ )
	{
		if ( **s != '!' )
		{
			if ( svs.maxclients <= 1 )
				nSize = 0;
			else
				nSize = FS_FileSize( va( "sound/%s", *s ) );

			SV_AddResource( t_sound, *s, nSize, 0, i );
		}
		else if ( !ffirstsent )
		{
			ffirstsent = 1;
			SV_AddResource( t_sound, "!", 0, RES_FATALIFMISSING, i );
		}
	}

	for ( s = &sv.model_precache[1], i = 1; *s; s++, i++ )
	{
		if ( svs.maxclients <= 1 )
			nSize = 0;
		else if ( **s == '*' )
			nSize = 0;
		else
			nSize = FS_FileSize( *s );

		SV_AddResource( t_model, *s, nSize, sv.model_precache_flags[i], i );
	}

	for ( i = 0; i < sv_decalnamecount; i++ )
		SV_AddResource( t_decal, sv_decalnames[i].name, Draw_DecalSize( i ), 0, i );

	for ( i = 1; i < MAX_EVENTS; i++ )
	{
		if ( !sv.event_precache[i].filename )
			break;

		SV_AddResource( t_eventscript, (char *)sv.event_precache[i].filename, sv.event_precache[i].filesize, RES_FATALIFMISSING, i );
	}
}

/*
================
SV_ClearCaches

================
*/
void SV_ClearCaches( void )
{
	int		i;

	for ( i = 1; i < MAX_EVENTS; i++ )
	{
		if ( !sv.event_precache[i].filename )
			break;

		sv.event_precache[i].filename = NULL;
		if ( sv.event_precache[i].pszScript )
			Mem_Free( (void *)sv.event_precache[i].pszScript );
		sv.event_precache[i].pszScript = NULL;
	}
}

/*
================
SV_PropagateCustomizations

Sends customizations from all active players to the current player.
================
*/
void SV_PropagateCustomizations( void )
{
	client_t	*pHost;
	customization_t	*pCust;
	resource_t	*pResource;
	int		i;

	pHost = host_client;

	// For each active player
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( !host_client->active && !host_client->spawned )
			continue;
		if ( host_client->fakeclient )
			continue;

		// Send each customization to current player
		pCust = host_client->customdata.pNext;
		while ( pCust )
		{
			if ( !pCust->bInUse )
			{
				pCust = pCust->pNext;
				continue;
			}

			pResource = &pCust->resource;

			MSG_WriteByte( &pHost->netchan.message, svc_customization );
			MSG_WriteByte( &pHost->netchan.message, i );
			MSG_WriteByte( &pHost->netchan.message, pResource->type );
			MSG_WriteString( &pHost->netchan.message, pResource->szFileName );
			MSG_WriteShort( &pHost->netchan.message, pResource->nIndex );
			MSG_WriteLong( &pHost->netchan.message, pResource->nDownloadSize );
			MSG_WriteByte( &pHost->netchan.message, pResource->ucFlags );
			if ( pResource->ucFlags & RES_CUSTOM )
				SZ_Write( &pHost->netchan.message, pResource->rgucMD5_hash, sizeof( pResource->rgucMD5_hash ) );

			pCust = pCust->pNext;
		}
	}

	host_client = pHost;
}

/*
================
SV_WriteVoiceCodec

================
*/
void SV_WriteVoiceCodec( sizebuf_t *pBuf )
{
	MSG_WriteByte( pBuf, svc_voiceinit );

	if ( svs.maxclients <= 1 )
		MSG_WriteString( pBuf, "" );
	else
		MSG_WriteString( pBuf, sv_voicecodec.string );

	MSG_WriteByte( pBuf, (int)sv_voicequality.value );
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void )
{
	edict_t	*svent;
	int		entnum;
	qboolean	player;
	qboolean	custom;
	entity_state_t	nullstate;

	sv.instance_baselines = &g_sv_instance_baselines;

	Q_memset( &nullstate, 0, sizeof( nullstate ) );

	SV_FindModelNumbers();

	for ( entnum = 0; entnum < sv.num_edicts; entnum++ )
	{
		// get the current server version
		svent = &sv.edicts[entnum];
		if ( svent->free )
			continue;
		if ( entnum > svs.maxclients && !svent->v.modelindex )
			continue;

		player = entnum >= 1 && entnum <= svs.maxclients;

		// create entity baseline
		sv.baselines[entnum].number = entnum;
		sv.baselines[entnum].entityType = ENTITY_NORMAL;
		if ( svent->v.flags & FL_CUSTOMENTITY )
			sv.baselines[entnum].entityType = ENTITY_BEAM;

		gEntityInterface.pfnCreateBaseline( player, entnum, &sv.baselines[entnum], svent, sv_playermodel, player_mins[0], player_maxs[0] );

		sv_lastnum = entnum;
	}

	gEntityInterface.pfnCreateInstancedBaselines();

	MSG_WriteByte( &sv.signon, svc_spawnbaseline );
	MSG_StartBitWriting( &sv.signon );

	for ( entnum = 0; entnum < sv.num_edicts; entnum++ )
	{
		svent = &sv.edicts[entnum];
		if ( svent->free )
			continue;
		if ( entnum > svs.maxclients && !svent->v.modelindex )
			continue;

		MSG_WriteBits( entnum, 11 );

		custom = !( sv.baselines[entnum].entityType & ENTITY_NORMAL );

		MSG_WriteBits( sv.baselines[entnum].entityType, 2 );

		if ( custom )
			DELTA_WriteDelta( (byte *)&nullstate, (byte *)&sv.baselines[entnum], TRUE, g_pcustomentitydelta, NULL );
		else
			DELTA_WriteDelta( (byte *)&nullstate, (byte *)&sv.baselines[entnum], TRUE, SV_IsPlayerIndex( entnum ) ? g_pplayerdelta : g_pentitydelta, NULL );
	}

	MSG_WriteBits( 0xFFFF, 16 );
	MSG_WriteBits( sv.instance_baselines->number, 6 );

	for ( entnum = 0; entnum < sv.instance_baselines->number; entnum++ )
		DELTA_WriteDelta( (byte *)&nullstate, (byte *)&sv.instance_baselines->baseline[entnum], TRUE, g_pentitydelta, NULL );

	MSG_EndBitWriting( &sv.signon );
}

/*
=================
SV_BroadcastCommand

Sends text to all active clients
=================
*/
void SV_BroadcastCommand( char *fmt, ... )
{
	va_list		argptr;
	char		string[1024];
	char		data[128];
	sizebuf_t	msg;
	int			i;
	client_t	*cl;

	msg.buffername = "Broadcast Command";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );
	msg.flags = FSB_ALLOWOVERFLOW;

	if ( !sv.active )
		return;

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	MSG_WriteByte( &msg, svc_stufftext );
	MSG_WriteString( &msg, string );

	if ( msg.flags & FSB_OVERFLOWED )
	{
		Sys_Error( "SV_BroadcastCommand:  Overflowed on %s, %i is max size\n", string, msg.maxsize );
		return;
	}

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( ( !cl->active && !cl->connected && !cl->spawned ) || cl->fakeclient )
			continue;

		SZ_Write( &cl->netchan.message, msg.data, msg.cursize );
	}
}

/*
================
SV_BuildReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_BuildReconnect( sizebuf_t *msg )
{
	MSG_WriteByte( msg, svc_stufftext );
	MSG_WriteString( msg, "reconnect\n" );
}

/*
================
SV_ReconnectAllClients

================
*/
void SV_ReconnectAllClients( void )
{
	int		i;
	char	message[] = { 'S', 'e', 'r', 'v', 'e', 'r', ' ', 'u', 'p', 'd', 'a', 't', 'i', 'n', 'g', ' ',
		'S', 'e', 'c', 'u', 'r', 'i', 't', 'y', ' ', 'M', 'o', 'd', 'u', 'l', 'e', '.', '\n', 0 };

	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( host_client->active || host_client->connected ) && !host_client->fakeclient )
		{
			Netchan_Clear( &host_client->netchan );

			MSG_WriteByte( &host_client->netchan.message, svc_print );
			MSG_WriteString( &host_client->netchan.message, message );

			MSG_WriteByte( &host_client->netchan.message, svc_stufftext );
			MSG_WriteString( &host_client->netchan.message, "retry\n" );

			SV_DropClient( host_client, FALSE, message );
		}
	}
}

int		g_bIsCStrike;
int		g_bIsCZero;
int		g_bIsTerrorStrike;
int		g_bIsCZeroRitual;
int		g_bCS_CZ_Flags_Initialized;

/*
================
SetCStrikeFlags

================
*/
void SetCStrikeFlags( void )
{
	if ( !g_bCS_CZ_Flags_Initialized )
	{
		if ( !_stricmp( com_gamedir, "cstrike" ) || !_stricmp( com_gamedir, "cstrike_beta" ) )
			g_bIsCStrike = TRUE;
		else if ( !_stricmp( com_gamedir, "czero" ) )
			g_bIsCZero = TRUE;
		else if ( !_stricmp( com_gamedir, "czeror" ) )
			g_bIsCZeroRitual = TRUE;
		else if ( !_stricmp( com_gamedir, "terror" ) )
			g_bIsTerrorStrike = TRUE;

		g_bCS_CZ_Flags_Initialized = TRUE;
	}
}

/*
================
SV_ActivateServer

Tell all the clients that the server is changing levels
================
*/
void SV_ActivateServer( int runPhysics )
{
	int			i;
	unsigned char	data[65536];
	sizebuf_t	msg;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Activate Server";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	g_bIsCStrike = FALSE;
	g_bIsCZero = FALSE;
	g_bIsTerrorStrike = FALSE;
	g_bIsCZeroRitual = FALSE;

	if ( !_stricmp( com_gamedir, "cstrike" ) || !_stricmp( com_gamedir, "cstrike_beta" ) )
		g_bIsCStrike = TRUE;
	else if ( !_stricmp( com_gamedir, "czero" ) )
		g_bIsCZero = TRUE;
	else if ( !_stricmp( com_gamedir, "czeror" ) )
		g_bIsCZeroRitual = TRUE;
	else if ( !_stricmp( com_gamedir, "terror" ) )
		g_bIsTerrorStrike = TRUE;

	g_bCS_CZ_Flags_Initialized = TRUE;

	Cvar_Set( "sv_newunit", "0" );

	ContinueLoadingProgressBar( "Server", 8, 0.0f );

	// Activate the DLL server code
	gEntityInterface.pfnServerActivate( sv.edicts, sv.num_edicts, svs.maxclients );

	Steam_Activate();

	ContinueLoadingProgressBar( "Server", 9, 0.0f );

	SV_CreateGenericResources();

	sv.active = TRUE;
	// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

	ContinueLoadingProgressBar( "Server", 10, 0.0f );

	// run two frames to allow everything to settle
	if ( runPhysics )
	{
		if ( svs.maxclients > 1 )
		{
			host_frametime = 0.8;
			for ( i = 0; i < 16; i++ )
				SV_Physics();
		}
		else
		{
			host_frametime = 0.1;
			SV_Physics();
			SV_Physics();
		}
	}
	else
	{
		host_frametime = 0.001;
		SV_Physics();
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// Create the resource list
	SV_CreateResourceList();

	sv.num_consistency = SV_TransferConsistencyInfo();

	// send serverinfo to all connected clients
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( ( host_client->active || host_client->connected ) && !host_client->fakeclient )
		{
			Netchan_Clear( &host_client->netchan );

			if ( svs.maxclients <= 1 )
			{
				SV_SendServerinfo( &msg, host_client );
			}
			else
			{
				SV_BuildReconnect( &host_client->netchan.message );
				Netchan_Transmit( &host_client->netchan, 0, NULL );
			}

			if ( sv_gpUserMsgs )
			{
				UserMsg	*pTemp;

				pTemp = sv_gpNewUserMsgs;
				sv_gpNewUserMsgs = sv_gpUserMsgs;
				SV_SendUserReg( &msg );
				sv_gpNewUserMsgs = pTemp;
			}

			host_client->hasusrmsgs = TRUE;

			Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
			Netchan_FragSend( &host_client->netchan );
			SZ_Clear( &msg );
		}
	}

	HPAK_FlushHostQueue();

	if ( svs.maxclients > 1 )
		Con_DPrintf( "%i player server started\n", svs.maxclients );
	else
		Con_DPrintf( "Game started\n" );

	Log_Printf( "Started map \"%s\" (CRC \"%i\")\n", sv.name, sv.worldmapCRC );

	if ( mapchangecfgfile.string && mapchangecfgfile.string[0] )
	{
		char	szCommand[256];

		AlertMessage( at_console, "Executing map change config file\n" );
		sprintf( szCommand, "exec %s\n", mapchangecfgfile.string );
		Cbuf_AddText( szCommand );
	}
}

/*
================
SV_ServerShutdown

================
*/
void SV_ServerShutdown( void )
{
	Steam_NotifyOfLevelChange();

	gGlobalVariables.time = sv.time;

	if ( svs.dll_initialized && sv.active )
		gEntityInterface.pfnServerDeactivate();
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
int SV_SpawnServer( qboolean bIsDemo, char *server, char *startspot )
{
	client_t	*cl;
	edict_t		*ent;
	int			i;
	char		szDllName[64];
	char		*pszhost;
	char		oldname[64];

	if ( sv.active && svs.maxclients > 1 )
	{
		// Tell game DLL that the clients have been dropped
		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( ( !cl->active && !cl->spawned && !cl->connected ) || !cl->edict || cl->edict->free )
				continue;

			if ( !cl->edict->pvPrivateData )
			{
				Con_Printf( "Skipping reconnect on %s, no pvPrivateData\n", cl->name );
				continue;
			}

			gEntityInterface.pfnClientDisconnect( cl->edict );
		}
	}

	if ( g_bOutOfDateRestart == TRUE )
	{
		g_bOutOfDateRestart = FALSE;
		Cmd_ExecuteString( "quit\n", src_command );
	}

	Log_Open();
	Log_Printf( "Loading map \"%s\"\n", server );
	Log_PrintServerVars();

	NET_Config( svs.maxclients > 1 );

	pszhost = Cvar_VariableString( "hostname" );
	if ( pszhost && !pszhost[0] )
	{
		if ( gEntityInterface.pfnGetGameDescription )
			Cvar_Set( "hostname", (char *)gEntityInterface.pfnGetGameDescription() );
		else
			Cvar_Set( "hostname", "Cry of Fear" );
	}

	scr_centertime_off = 0;

	if ( startspot )
		Con_DPrintf( "Spawn Server %s: [%s]\n", server, startspot );
	else
		Con_DPrintf( "Spawn Server %s\n", server );

	g_LastScreenUpdateTime = 0;

	svs.spawncount = ++gHostSpawnCount;

	// make cvars consistant
	if ( coop.value )
		Cvar_SetValue( "deathmatch", 0 );

	current_skill = (int)( skill.value + 0.5 );
	if ( current_skill < 0 )
		current_skill = 0;
	if ( current_skill > 3 )
		current_skill = 3;

	Cvar_SetValue( "skill", (float)current_skill );

	ContinueLoadingProgressBar( "Server", 2, 0.0f );

	// Make sure the custom resources are not too big
	HPAK_CheckSize( "custom" );

	oldname[0] = 0;
	if ( sv.name )
	{
		Q_strncpy( oldname, sv.name, sizeof( oldname ) - 1 );
		oldname[sizeof( oldname ) - 1] = 0;
	}

	// clear the memory
	Host_ClearMemory( FALSE );

	for ( i = 0, cl = svs.clients; i < svs.maxclientslimit; i++, cl++ )
		SV_ClearFrames( &cl->frames );

	if ( svs.maxclients == 1 )
		SV_UPDATE_BACKUP = 8;
	else
		SV_UPDATE_BACKUP = 64;

	SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;

	SV_AllocClientFrames();

	Q_memset( &sv, 0, sizeof( sv ) );

	Q_strncpy( sv.oldname, oldname, sizeof( oldname ) - 1 );
	sv.oldname[sizeof( oldname ) - 1] = 0;
	Q_strncpy( sv.name, server, sizeof( sv.name ) - 1 );
	sv.name[sizeof( sv.name ) - 1] = 0;

	if ( startspot )
	{
		Q_strncpy( sv.startspot, startspot, sizeof( sv.startspot ) - 1 );
		sv.startspot[sizeof( sv.startspot ) - 1] = 0;
	}
	else
	{
		sv.startspot[0] = 0;
	}

	pr_strings = gNullString;
	gGlobalVariables.pStringBase = pr_strings;

	if ( svs.maxclients == 1 )
		Cvar_SetValue( "sv_clienttrace", 1 );

	// allocate server memory
	sv.max_edicts = COM_EntsForPlayerSlots( svs.maxclients );

	SV_DeallocateDynamicData();
	SV_ReallocateDynamicData();

	gGlobalVariables.maxEntities = sv.max_edicts;

	sv.edicts = (edict_t *)Hunk_AllocName( sv.max_edicts * sizeof( edict_t ), "edicts" );
	sv.baselines = (struct entity_state_s *)Hunk_AllocName( sv.max_edicts * sizeof( entity_state_t ), "baselines" );

	sv.datagram.maxsize = sizeof( sv.datagram_buf );
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.buffername = "Server Datagram";

	sv.reliable_datagram.maxsize = sizeof( sv.reliable_datagram_buf );
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.reliable_datagram.buffername = "Server Reliable Datagram";

	sv.multicast.maxsize = sizeof( sv.multicast_buf );
	sv.multicast.data = sv.multicast_buf;
	sv.multicast.buffername = "Server Multicast Buffer";

	sv.spectator.maxsize = sizeof( sv.spectator_buf );
	sv.spectator.data = sv.spectator_buf;
	sv.spectator.buffername = "Server Spectator Buffer";

	sv.signon.data = sv.signon_data;
	sv.signon.maxsize = sizeof( sv.signon_data );
	sv.signon.cursize = 0;
	sv.signon.buffername = "Server Signon Buffer";

	// leave slots at start for clients only
	sv.num_edicts = svs.maxclients + 1;
	for ( i = 0; i < svs.maxclients; i++ )
	{
		ent = sv.edicts + i + 1;
		svs.clients[i].edict = ent;
	}

	gGlobalVariables.maxClients = svs.maxclients;

	sv.state = ss_loading;
	sv.paused = FALSE;
	sv.time = 1.0;

	gGlobalVariables.time = sv.time;

	R_ForceCVars( svs.maxclients > 1 );

	ContinueLoadingProgressBar( "Server", 3, 0.0f );

	_snprintf( sv.modelname, sizeof( sv.modelname ), "maps/%s.bsp", server );
	sv.worldmodel = Mod_ForName( sv.modelname, FALSE, FALSE );
	if ( !sv.worldmodel )
	{
		Con_Printf( "Couldn't spawn server %s\n", sv.modelname );
		sv.active = FALSE;
		return 0;
	}

	Sequence_OnLevelLoad( server );

	ContinueLoadingProgressBar( "Server", 4, 0.0f );

	_snprintf( szDllName, sizeof( szDllName ), "cl_dlls\\client.dll" );
	COM_FixSlashes( szDllName );
	if ( !MD5_Hash_File( sv.clientdllmd5, szDllName, FALSE, FALSE, NULL ) )
	{
		Con_Printf( "Couldn't CRC client side dll:  %s\n", szDllName );
		sv.active = FALSE;
		return 0;
	}

	ContinueLoadingProgressBar( "Server", 6, 0.0f );

	// DON'T do this in single player
	if ( svs.maxclients > 1 )
	{
		CRC32_Init( &sv.worldmapCRC );
		if ( !CRC_MapFile( &sv.worldmapCRC, sv.modelname ) )
		{
			Con_Printf( "Couldn't CRC server map:  %s\n", sv.modelname );
			sv.active = FALSE;
			return 0;
		}

		CM_CalcPAS( sv.worldmodel );
	}
	else
	{
		sv.worldmapCRC = 0;
	}

	sv.models[1] = sv.worldmodel;

	// clear world interaction links
	SV_ClearWorld();

	sv.sound_precache[0] = pr_strings;
	sv.model_precache[0] = pr_strings;
	sv.model_precache[1] = sv.modelname;
	sv.model_precache_flags[1] |= RES_FATALIFMISSING;
	sv.generic_precache[0] = pr_strings;

	for ( i = 1; i < sv.worldmodel->numsubmodels; i++ )
	{
		sv.model_precache[i + 1] = localmodels[i];
		sv.models[i + 1] = Mod_ForName( localmodels[i], FALSE, FALSE );
		sv.model_precache_flags[i + 1] |= RES_FATALIFMISSING;
	}

	// load the rest of the entities
	ent = sv.edicts;
	Q_memset( &ent->v, 0, sizeof( entvars_t ) );
	ent->free = FALSE;
	ent->v.model = (char *)sv.worldmodel - pr_strings;
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	if ( coop.value )
		gGlobalVariables.coop = coop.value;
	else
		gGlobalVariables.deathmatch = deathmatch.value;

	gGlobalVariables.mapname = sv.name - pr_strings;
	gGlobalVariables.startspot = sv.startspot - pr_strings;

	// serverflags are for cross level information (sigils)
	gGlobalVariables.serverflags = svs.serverflags;

	allow_cheats = sv_cheats.value;

	SV_SetMoveVars();

	return 1;
}

/*
================
SV_LoadEntities

================
*/
void SV_LoadEntities( void )
{
	ED_LoadFromFile( sv.worldmodel->entities );
}

/*
================
SV_ClearEntities

================
*/
void SV_ClearEntities( void )
{
	int		i;

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		edict_t	*pEdict;

		pEdict = &sv.edicts[i];
		if ( !pEdict->free )
			ReleaseEntityDLLFields( pEdict );
	}
}

/*
================
RegUserMsg

================
*/
int RegUserMsg( const char *pszName, int iSize )
{
	UserMsg	*pUserMsgs;
	UserMsg	*pNewMsg;
	int		iFound = 0;
	int		iRet = 0;

	if ( giNextUserMsg > 255 )
		return 0;

	if ( !pszName )
		return 0;

	if ( Q_strlen( pszName ) > 11 )
		return 0;

	if ( iSize > 192 )
		return 0;

	// Iterate through the list
	pUserMsgs = sv_gpUserMsgs;
	while ( pUserMsgs )
	{
		if ( !Q_strcmp( pszName, pUserMsgs->szName ) )
		{
			iFound = 1;
			break;
		}
		pUserMsgs = pUserMsgs->next;
	}

	if ( iFound )
	{
		iRet = pUserMsgs->iMsg;
	}
	else
	{
		pNewMsg = (UserMsg *)Mem_ZeroMalloc( sizeof( UserMsg ) );
		pNewMsg->iMsg = giNextUserMsg++;
		pNewMsg->iSize = iSize;
		Q_strcpy( pNewMsg->szName, pszName );

		if ( !pNewMsg )
			return 0;

		pNewMsg->next = sv_gpNewUserMsgs;
		sv_gpNewUserMsgs = pNewMsg;

		iRet = pNewMsg->iMsg;
	}

	return iRet;
}

/*
=================
StringToFilter

=================
*/
qboolean StringToFilter( char *s, ipfilter_t *f )
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];

	for ( i = 0; i < 4; i++ )
	{
		b[i] = 0;
		m[i] = 0;
	}

	for ( i = 0; i < 4; i++ )
	{
		if ( *s < '0' || *s > '9' )
		{
			Con_Printf( "Bad filter address: %s\n", s );
			return FALSE;
		}

		j = 0;
		while ( *s >= '0' && *s <= '9' )
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = Q_atoi( num );
		if ( b[i] != 0 )
			m[i] = 255;

		if ( !*s )
			break;
		s++;
	}

	f->mask = *(unsigned *)m;
	f->compare = *(unsigned *)b;

	return TRUE;
}

/*
=================
SV_StringToUserID

=================
*/
USERID_t *SV_StringToUserID( const char *str )
{
	static USERID_t	id;

	Q_memset( &id, 0, sizeof( id ) );

	if ( str && str[0] )
	{
		char	szTemp[128];
		char	*pszUserID = NULL;

		if ( !Q_strnicmp( str, "STEAM_", strlen( "STEAM_" ) ) )
		{
			strncpy( szTemp, str + strlen( "STEAM_" ), sizeof( szTemp ) - 1 );
			id.idtype = AUTH_IDTYPE_STEAM;
		}
		else
		{
			strncpy( szTemp, str + strlen( "VALVE_" ), sizeof( szTemp ) - 1 );
			id.idtype = AUTH_IDTYPE_VALVE;
		}

		szTemp[sizeof( szTemp ) - 1] = 0;
		id.m_SteamID = Steam_StringToSteamID( szTemp );
	}

	return &id;
}


/*
=================
SV_BanId_f

=================
*/
void SV_BanId_f( void )
{
	int			i;
	float		banTime;
	USERID_t	*id = NULL;
	char		*pszCmdGiver;
	char		szreason[256];
	char		idstring[64];
	qboolean	bKick = FALSE;

	if ( Cmd_Argc() < 3 || Cmd_Argc() > 8 )
	{
		Con_Printf( "Usage:  banid <minutes> <uniqueid or #userid> { kick }\n" );
		Con_Printf( "Use 0 minutes for permanent\n" );
		return;
	}

	banTime = Q_atof( Cmd_Argv( 1 ) );
	if ( banTime < 0.01 )
		banTime = 0;

	strncpy( idstring, Cmd_Argv( 2 ), sizeof( idstring ) - 1 );
	idstring[sizeof( idstring ) - 1] = 0;

	bKick = ( Cmd_Argc() >= 4 && !Q_strcasecmp( Cmd_Argv( Cmd_Argc() - 1 ), "kick" ) );

	if ( idstring[0] == '#' )
	{
		client_t	*cl;
		int			search = -1;

		if ( Q_strlen( idstring ) != 1 )
		{
			search = Q_atoi( &idstring[1] );
		}
		else if ( Cmd_Argc() >= 3 )
		{
			search = Q_atoi( Cmd_Argv( 3 ) );
		}
		else
		{
			Con_Printf( "Insufficient arguments to banid\n" );
			return;
		}

		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->active && !cl->connected && !cl->spawned )
				continue;

			if ( cl->fakeclient )
				continue;

			if ( cl->userid != search )
				continue;

			id = &cl->network_userid;
			break;
		}

		if ( !id )
		{
			Con_Printf( "SV_BanId_f:  Couldn't find #userid %u\n", search );
			return;
		}
	}
	else
	{
		client_t	*cl;

		if ( !Q_strnicmp( idstring, "STEAM_", strlen( "STEAM_" ) ) || !Q_strnicmp( idstring, "VALVE_", strlen( "VALVE_" ) ) )
		{
			_snprintf( idstring, sizeof( idstring ) - 1, "%s:%s:%s", Cmd_Argv( 2 ), Cmd_Argv( 4 ), Cmd_Argv( 6 ) );
			idstring[sizeof( idstring ) - 1] = 0;
		}

		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->active && !cl->connected && !cl->spawned )
				continue;

			if ( cl->fakeclient )
				continue;

			if ( Q_stricmp( SV_GetClientIDString( cl ), idstring ) )
				continue;

			id = &cl->network_userid;
			break;
		}

		if ( !id )
		{
			id = SV_StringToUserID( idstring );
			if ( !id )
			{
				Con_Printf( "SV_BanId_f:  Couldn't resolve uniqueid %s.\n", idstring );
				Con_Printf( "Usage:  banid <minutes> <uniqueid or #userid> { kick }\n" );
				Con_Printf( "Use 0 minutes for permanent\n" );
				return;
			}
		}
	}

	if ( !id )
	{
		Con_Printf( "SV_BanId_f:  id == NULL\n" );
		return;
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( SV_CompareUserID( &userfilters[i].userid, id ) )
			break;
	}

	if ( i >= numuserfilters )
	{
		if ( numuserfilters >= MAX_USERFILTERS )
		{
			Con_Printf( "SV_BanId_f:  User filter list is full\n" );
			return;
		}
		numuserfilters++;
	}

	userfilters[i].banTime = banTime;
	userfilters[i].banEndTime = ( banTime != 0.0 ) ? ( banTime * 60.0 + realtime ) : 0.0;
	userfilters[i].userid = *id;

	if ( banTime == 0.0 )
		sprintf( szreason, "permanently" );
	else
		_snprintf( szreason, sizeof( szreason ), "for %.2f minutes", banTime );

	if ( cmd_source == src_command )
	{
		if ( cls.state == ca_dedicated )
			pszCmdGiver = "Console";
		else
			pszCmdGiver = cl_name.string;
	}
	else
	{
		pszCmdGiver = host_client->name;
	}

	if ( bKick )
	{
		client_t	*cl;
		client_t	*save;

		save = host_client;

		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->active && !cl->connected && !cl->spawned )
				continue;

			if ( cl->fakeclient )
				continue;

			if ( !SV_CompareUserID( &cl->network_userid, id ) )
				continue;

			host_client = cl;

			if ( sv_logbans.value )
				Log_Printf( "Ban: \"%s<%i><%s><>\" was kicked and banned \"%s\" by \"%s\"\n", host_client->name, host_client->userid, SV_GetClientIDString( host_client ), szreason, pszCmdGiver );

			SV_ClientPrintf( "You have been kicked and banned %s by the server op.\n", szreason );
			SV_DropClient( host_client, FALSE, "Kicked and banned" );
			break;
		}

		host_client = save;
	}
	else
	{
		if ( sv_logbans.value )
			Log_Printf( "Ban: \"<><%s><>\" was banned \"%s\" by \"%s\"\n", SV_GetIDString( id ), szreason, pszCmdGiver );
	}
}

/*
==================
Host_Kick_f

Kicks a user off of the server
==================
*/
void Host_Kick_f( void )
{
	char		*who;
	char		*message = NULL;
	client_t	*save;
	char		*p;
	int			i;
	qboolean	byNumber;
	char		idstring[64];
	qboolean	isSteam = FALSE;
	int			argsStartNum = 0;

	if ( Cmd_Argc() <= 1 )
	{
		Con_Printf( "usage:  kick < name > | < # userid >\n" );
		return;
	}

	if ( cmd_source == src_command )
	{
		if ( !sv.active )
		{
			Cmd_ForwardToServer();
			return;
		}
	}
	else if ( host_client->netchan.remote_address.type != NA_LOOPBACK )
	{
		SV_ClientPrintf( "You can't 'kick' because you are not a server operator\n" );
		return;
	}

	save = host_client;

	p = Cmd_Argv( 1 );
	if ( p && *p == '#' )
	{
		int		searchid;

		if ( Cmd_Argc() > 2 && !p[1] )
		{
			searchid = Q_atoi( Cmd_Argv( 2 ) );
			p = Cmd_Argv( 2 );
			argsStartNum = 3;
		}
		else
		{
			p++;
			searchid = Q_atoi( p );
			argsStartNum = 2;
		}

		strncpy( idstring, p, sizeof( idstring ) - 1 );
		idstring[sizeof( idstring ) - 1] = 0;

		if ( !Q_strnicmp( idstring, "STEAM_", strlen( "STEAM_" ) ) || !Q_strnicmp( idstring, "VALVE_", strlen( "VALVE_" ) ) )
		{
			_snprintf( idstring, sizeof( idstring ) - 1, "%s:%s:%s", p, Cmd_Argv( argsStartNum + 1 ), Cmd_Argv( argsStartNum + 3 ) );
			idstring[sizeof( idstring ) - 1] = 0;
			isSteam = TRUE;
			argsStartNum += 4;
		}

		for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
		{
			if ( !host_client->active && !host_client->connected )
				continue;

			if ( searchid && host_client->userid == searchid )
				break;

			if ( Q_stricmp( SV_GetClientIDString( host_client ), idstring ) )
				continue;

			break;
		}

		byNumber = TRUE;
	}
	else
	{
		for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
		{
			if ( !host_client->active && !host_client->connected )
				continue;

			if ( !Q_strcasecmp( host_client->name, Cmd_Argv( 1 ) ) )
				break;
		}

		argsStartNum = 2;
		byNumber = FALSE;
	}

	if ( i < svs.maxclients )
	{
		if ( cmd_source == src_command )
		{
			if ( cls.state == ca_dedicated )
				who = "Console";
			else
				who = cl_name.string;
		}
		else
		{
			who = save->name;
		}

		if ( host_client->netchan.remote_address.type == NA_LOOPBACK )
		{
			Con_Printf( "The local player cannot be kicked!\n" );
			return;
		}

		if ( Cmd_Argc() > argsStartNum )
		{
			int		i;
			int		dataLen = 0;

			message = Cmd_Args();

			for ( i = 1; i < argsStartNum; i++ )
				dataLen += strlen( Cmd_Argv( i ) ) + 1;

			if ( isSteam )
				dataLen -= 4;

			if ( dataLen > strlen( message ) )
				message = NULL;
			else
				message += dataLen;
		}

		if ( message )
		{
			SV_ClientPrintf( "Kicked by %s: %s\n", who, message );
			Log_Printf( "Kick: \"%s<%i><%s><>\" was kicked by \"%s\" (message \"%s\")\n", host_client->name, host_client->userid, SV_GetClientIDString( host_client ), who, message );
			SV_DropClient( host_client, FALSE, va( "Kicked :%s", message ) );
		}
		else
		{
			SV_ClientPrintf( "Kicked by %s\n", who );
			Log_Printf( "Kick: \"%s<%i><%s><>\" was kicked by \"%s\"\n", host_client->name, host_client->userid, SV_GetClientIDString( host_client ), who );
			SV_DropClient( host_client, FALSE, "Kicked" );
		}
	}

	host_client = save;
}

/*
=================
SV_RemoveId_f

=================
*/
void SV_RemoveId_f( void )
{
	int		i = 0, j = 0;
	char	idstring[64];

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 6 )
	{
		Con_Printf( "Usage:  removeid <uniqueid | #slotnumber>\n" );
		return;
	}

	strncpy( idstring, Cmd_Argv( 1 ), sizeof( idstring ) - 1 );
	idstring[sizeof( idstring ) - 1] = 0;

	if ( !idstring[0] )
	{
		Con_Printf( "SV_RemoveId_f:  Id string is empty!\n" );
		return;
	}

	if ( idstring[0] == '#' )
	{
		int		slot;

		slot = Q_atoi( &idstring[1] );
		if ( slot > 0 && slot <= numuserfilters )
		{
			USERID_t	id;

			slot--;
			id = userfilters[slot].userid;

			for ( j = slot + 1; j < numuserfilters; j++ )
				userfilters[j - 1] = userfilters[j];

			numuserfilters--;
			Con_Printf( "UserID filter removed for %s, id %s\n", idstring, SV_GetIDString( &id ) );
			return;
		}

		Con_Printf( "SV_RemoveId_f:  invalid slot #%i\n", slot );
		return;
	}

	if ( !Q_strnicmp( idstring, "STEAM_", strlen( "STEAM_" ) ) || !Q_strnicmp( idstring, "VALVE_", strlen( "VALVE_" ) ) )
	{
		_snprintf( idstring, sizeof( idstring ) - 1, "%s:%s:%s", Cmd_Argv( 1 ), Cmd_Argv( 3 ), Cmd_Argv( 5 ) );
		idstring[sizeof( idstring ) - 1] = 0;
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( Q_stricmp( SV_GetIDString( &userfilters[i].userid ), idstring ) )
			continue;

		for ( j = i + 1; j < numuserfilters; j++ )
			userfilters[j - 1] = userfilters[j];

		numuserfilters--;
		Con_Printf( "UserID filter removed for %s\n", idstring );
		return;
	}

	Con_Printf( "removeid: couldn't find %s\n", idstring );
}

/*
=================
SV_WriteId_f

=================
*/
void SV_WriteId_f( void )
{
	FileHandle_t	f;
	char	name[MAX_PATH];
	int		i;
	float	banTime;

	_snprintf( name, sizeof( name ), "%s", bannedcfgfile.string );

	Con_Printf( "Writing %s.\n", name );

	f = FS_Open( name, "wt" );
	if ( !f )
	{
		Con_Printf( "Couldn't open %s\n", name );
		return;
	}

	for ( i = 0; i < numuserfilters; i++ )
	{
		banTime = userfilters[i].banTime;
		if ( banTime != 0.0f )
			continue;

		FS_FPrintf( f, "banid 0.0 %s\n", SV_GetIDString( &userfilters[i].userid ) );
	}

	FS_Close( f );
}

/*
=================
SV_ListId_f

=================
*/
void SV_ListId_f( void )
{
	int		i;

	if ( numuserfilters <= 0 )
	{
		Con_Printf( "UserID filter list: empty\n" );
		return;
	}

	Con_Printf( "UserID filter list: %i entries\n", numuserfilters );

	for ( i = 0; i < numuserfilters; i++ )
	{
		if ( userfilters[i].banTime != 0.0 )
			Con_Printf( "%i %s : %.3f min\n", i + 1, SV_GetIDString( &userfilters[i].userid ), userfilters[i].banTime );
		else
			Con_Printf( "%i %s : permanent\n", i + 1, SV_GetIDString( &userfilters[i].userid ) );
	}
}

/*
=================
SV_AddIP_f

=================
*/
void SV_AddIP_f( void )
{
	int			i = 0;
	float		banTime = 0;
	ipfilter_t	tempFilter;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n" );
		return;
	}

	banTime = Q_atof( Cmd_Argv( 1 ) );

	if ( !StringToFilter( Cmd_Argv( 2 ), &tempFilter ) )
	{
		Con_Printf( "Invalid IP address!\nUsage: addip <minutes> <ipaddress>\nUse 0 minutes for permanent\n" );
		return;
	}

	for ( i = 0; i < numipfilters; i++ )
	{
		if ( ipfilters[i].mask == tempFilter.mask && ipfilters[i].compare == tempFilter.compare )
		{
			ipfilters[i].banTime = banTime;
			ipfilters[i].banEndTime = ( banTime != 0.0 ) ? ( banTime * 60.0 + realtime ) : 0.0;
			return;
		}
	}

	if ( i >= numipfilters )
	{
		if ( numipfilters >= MAX_IPFILTERS )
		{
			Con_Printf( "IP filter list is full\n" );
			return;
		}
		numipfilters++;
	}

	if ( banTime < 0.01f )
		banTime = 0;

	ipfilters[i].banTime = banTime;
	ipfilters[i].banEndTime = ( banTime != 0.0 ) ? ( banTime * 60.0 + realtime ) : 0.0;
	ipfilters[i].compare = tempFilter.compare;
	ipfilters[i].mask = tempFilter.mask;

	// Kick anyone who matches the new filter
	for ( i = 0, host_client = svs.clients; i < svs.maxclients; i++, host_client++ )
	{
		if ( !host_client->connected || !host_client->active || !host_client->spawned )
			continue;

		if ( host_client->fakeclient )
			continue;

		net_from = host_client->netchan.remote_address;
		if ( SV_FilterPacket() )
		{
			SV_ClientPrintf( "The server operator has added you to banned list\n" );
			SV_DropClient( host_client, FALSE, "Added to banned list" );
		}
	}
}

/*
=================
SV_RemoveIP_f

=================
*/
void SV_RemoveIP_f( void )
{
	ipfilter_t	f;
	int			i, j;

	if ( !StringToFilter( Cmd_Argv( 1 ), &f ) )
		return;

	for ( i = 0; i < numipfilters; i++ )
	{
		if ( ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare )
		{
			for ( j = i + 1; j < numipfilters; j++ )
				ipfilters[j - 1] = ipfilters[j];

			numipfilters--;
			ipfilters[numipfilters].banTime = 0;
			ipfilters[numipfilters].banEndTime = 0;
			ipfilters[numipfilters].compare = 0;
			ipfilters[numipfilters].mask = 0;
			Con_Printf( "IP filter removed.\n" );
			return;
		}
	}

	Con_Printf( "removeip: couldn't find %s.\n", Cmd_Argv( 1 ) );
}

/*
=================
SV_ListIP_f

=================
*/
void SV_ListIP_f( void )
{
	int		i;
	byte	b[4];

	if ( numipfilters <= 0 )
	{
		Con_Printf( "IP filter list: empty\n" );
		return;
	}

	Con_Printf( "IP filter list:\n" );

	for ( i = 0; i < numipfilters; i++ )
	{
		*(unsigned *)b = ipfilters[i].compare;
		if ( ipfilters[i].banTime != 0.0f )
			Con_Printf( "%3i.%3i.%3i.%3i : %.3f min\n", b[0], b[1], b[2], b[3], ipfilters[i].banTime );
		else
			Con_Printf( "%3i.%3i.%3i.%3i : permanent\n", b[0], b[1], b[2], b[3] );
	}
}

/*
=================
SV_WriteIP_f

=================
*/
void SV_WriteIP_f( void )
{
	FileHandle_t	f;
	char	name[MAX_PATH];
	byte	b[4];
	int		i;
	float	banTime;

	_snprintf( name, sizeof( name ), "listip.cfg" );

	Con_Printf( "Writing %s.\n", name );

	f = FS_Open( name, "wb" );
	if ( !f )
	{
		Con_Printf( "Couldn't open %s\n", name );
		return;
	}

	for ( i = 0; i < numipfilters; i++ )
	{
		*(unsigned *)b = ipfilters[i].compare;
		banTime = ipfilters[i].banTime;
		if ( banTime != 0.0f )
			continue;

		FS_FPrintf( f, "addip 0.0 %i.%i.%i.%i\n", b[0], b[1], b[2], b[3] );
	}

	FS_Close( f );
}

/*
=================
SV_KickPlayer

=================
*/
void SV_KickPlayer( int nPlayerSlot, int nReason )
{
	client_t	*client;
	USERID_t	id;
	int			i;
	char		rgchT[1024];

	if ( nPlayerSlot < 0 || nPlayerSlot >= svs.maxclients )
		return;

	client = &svs.clients[nPlayerSlot];
	if ( !client->connected )
		return;

	if ( !svs.isSecure )
		return;

	id = client->network_userid;

	Log_Printf( "Secure: \"%s<%i><%s><>\" was detected cheating and dropped from the server.\n", client->name, client->userid, SV_GetIDString( &id ), nReason );

	rgchT[0] = svc_print;
	sprintf( &rgchT[1], "\n********************************************\nYou have been automatically disconnected\nfrom this secure server because an illegal\ncheat was detected on your computer.\nRepeat violators may be permanently banned\nfrom all secure servers.\n\nFor help cleaning your system of cheats, visit:\nhttp://www.counter-strike.net/cheat.html\n********************************************\n\n" );
	Netchan_Transmit( &svs.clients[nPlayerSlot].netchan, strlen( rgchT ) + 1, (byte *)rgchT );

	sprintf( rgchT, "%s was automatically disconnected\nfrom this secure server.\n", svs.clients[nPlayerSlot].name );

	for ( i = 0; i < svs.maxclients; i++ )
	{
		if ( ( svs.clients[i].active || svs.clients[i].spawned ) && !svs.clients[i].fakeclient )
		{
			MSG_WriteByte( &svs.clients[i].netchan.message, svc_centerprint );
			MSG_WriteString( &svs.clients[i].netchan.message, rgchT );
		}
	}

	SV_DropClient( &svs.clients[nPlayerSlot], FALSE, "Automatically dropped by cheat detector" );
}

/*
==================
SV_InactivateClients

Purpose: Prepare for level transition, etc.
==================
*/
void SV_InactivateClients( void )
{
	int			i;
	client_t	*cl;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active && !cl->connected && !cl->spawned )
			continue;

		if ( cl->fakeclient )
		{
			SV_DropClient( cl, FALSE, "Dropping fakeclient on level change" );
			continue;
		}

		// Doesn't count as "active" until spawned
		cl->active = FALSE;
		cl->connected = TRUE;
		cl->spawned = FALSE;
		cl->fully_connected = FALSE;

		SZ_Clear( &cl->netchan.message );
		SZ_Clear( &cl->datagram );

		COM_ClearCustomizationList( &cl->customdata, FALSE );
		Q_memset( cl->physinfo, 0, MAX_PHYSINFO_STRING );
	}
}

/*
=================
SV_FailDownload

=================
*/
void SV_FailDownload( char *filename )
{
	if ( !filename || !filename[0] )
		return;

	MSG_WriteByte( &host_client->netchan.message, svc_filetxferfailed );
	MSG_WriteString( &host_client->netchan.message, filename );
}

/*
=================
Q_stristr

=================
*/
const char *Q_stristr( const char *pStr, const char *pSearch )
{
	const char	*pLetter = pStr;

	if ( !pStr || !pSearch )
		return 0;

	// Check the entire string
	while ( *pLetter != 0 )
	{
		// Skip over non-matches
		if ( tolower( (unsigned char)*pLetter ) == tolower( (unsigned char)*pSearch ) )
		{
			// Check for match
			const char	*pMatch = pLetter + 1;
			const char	*pTest = pSearch + 1;

			while ( *pTest != 0 )
			{
				// We've run off the end; don't bother.
				if ( *pMatch == 0 )
					return 0;

				if ( tolower( (unsigned char)*pMatch ) != tolower( (unsigned char)*pTest ) )
					break;

				++pMatch;
				++pTest;
			}

			// Found a match!
			if ( *pTest == 0 )
				return pLetter;
		}

		++pLetter;
	}

	return 0;
}

/*
=================
IsSafeFileToDownload

=================
*/
qboolean IsSafeFileToDownload( const char *filename )
{
	char	*first = NULL, *last = NULL;

	if ( !filename )
		return FALSE;

	if ( !Q_strncmp( filename, "!MD5", 4 ) )
		return TRUE;

	if ( Q_strstr( filename, "\\" ) )
		return FALSE;

	if ( Q_strstr( filename, ":" ) )
		return FALSE;

	if ( Q_strstr( filename, ".." ) )
		return FALSE;

	first = strchr( filename, '.' );
	last = strrchr( filename, '.' );

	if ( first != last )
		return FALSE;

	if ( !first || strlen( first ) != 4 )
		return FALSE;

	if ( Q_stristr( filename, ".cfg" ) )
		return FALSE;
	if ( Q_stristr( filename, ".lst" ) )
		return FALSE;
	if ( Q_stristr( filename, ".exe" ) )
		return FALSE;
	if ( Q_stristr( filename, ".vbs" ) )
		return FALSE;
	if ( Q_stristr( filename, ".com" ) )
		return FALSE;
	if ( Q_stristr( filename, ".bat" ) )
		return FALSE;
	if ( Q_stristr( filename, ".dll" ) )
		return FALSE;
	if ( Q_stristr( filename, ".ini" ) )
		return FALSE;
	if ( Q_stristr( filename, ".log" ) )
		return FALSE;
	if ( Q_stristr( filename, "halflife.wad" ) )
		return FALSE;
	if ( Q_stristr( filename, "pak0.pak" ) )
		return FALSE;
	if ( Q_stristr( filename, "xeno.wad" ) )
		return FALSE;
	if ( Q_stristr( filename, ".so" ) )
		return FALSE;
	if ( Q_stristr( filename, ".dylib" ) )
		return FALSE;
	if ( Q_stristr( filename, ".sys" ) )
		return FALSE;

	return TRUE;
}

/*
=================
SV_BeginFileDownload_f

=================
*/
void SV_BeginFileDownload_f( void )
{
	char	*name;
	char	szModuleC[] = { '!', 'M', 'o', 'd', 'u', 'l', 'e', 'C', '.', 'd', 'l', 'l', 0 };
	int		length_guess;	// unused

	if ( Cmd_Argc() < 2 )
		return;

	if ( cmd_source == src_command )
		return;

	name = Cmd_Argv( 1 );
	if ( !name || !name[0] )
		return;

	if ( !Q_strncmp( name, szModuleC, 12 ) && svs.isSecure )
		return;

	if ( !IsSafeFileToDownload( name ) || !sv_allow_download.value || ( host_client->fully_connected && name[0] != '!' ) )
	{
		SV_FailDownload( name );
		return;
	}

	if ( name[0] == '!' )
	{
		if ( Q_strlen( name ) == 36 && !Q_strnicmp( name, "!MD5", 4 ) )
		{
			unsigned char	md5[16];
			resource_t		custResource;
			unsigned char	*pbuf = NULL;
			int				size = 0;

			if ( !sv_send_logos.value )
			{
				SV_FailDownload( name );
				return;
			}

			Q_memset( &custResource, 0, sizeof( custResource ) );
			COM_HexConvert( name + 4, 32, md5 );
			if ( HPAK_ResourceForHash( "custom.hpk", md5, &custResource ) )
			{
				HPAK_GetDataPointer( "custom.hpk", &custResource, &pbuf, &size );
				if ( pbuf && size )
				{
					Netchan_CreateFileFragmentsFromBuffer( TRUE, &host_client->netchan, name, pbuf, size );
					Netchan_FragSend( &host_client->netchan );
					Mem_Free( pbuf );
				}
			}
		}
		else
		{
			SV_FailDownload( name );
		}
	}
	else
	{
		if ( !sv_send_resources.value
			|| ( sv_downloadurl.string && sv_downloadurl.string[0] && strlen( sv_downloadurl.string ) < 128 && !sv_allow_dlfile.value )
			|| !Netchan_CreateFileFragments( TRUE, &host_client->netchan, name ) )
		{
			SV_FailDownload( name );
			return;
		}

		Netchan_FragSend( &host_client->netchan );
	}
}

/*
================
SV_SetMaxclients

================
*/
void SV_SetMaxclients( void )
{
	int			i;
	client_t	*cl;

	for ( i = 0, cl = svs.clients; i < svs.maxclientslimit; i++, cl++ )
		SV_ClearFrames( &cl->frames );

	svs.maxclients = 1;

	i = COM_CheckParm( "-maxplayers" );
	if ( i )
		svs.maxclients = Q_atoi( com_argv[i + 1] );
	else if ( g_bIsDedicatedServer )
		svs.maxclients = 6;

	cls.state = g_bIsDedicatedServer ? ca_dedicated : ca_disconnected;

	if ( svs.maxclients < 1 )
		svs.maxclients = 6;
	else if ( svs.maxclients > MAX_CLIENTS )
		svs.maxclients = MAX_CLIENTS;

	if ( !g_bIsDedicatedServer && host_parms.memsize <= 0x1000000 )
		svs.maxclientslimit = 4;
	else
		svs.maxclientslimit = MAX_CLIENTS;

	SV_UPDATE_BACKUP = ( svs.maxclients == 1 ) ? 8 : 64;
	SV_UPDATE_MASK = SV_UPDATE_BACKUP - 1;

	svs.clients = (client_t *)Hunk_AllocName( svs.maxclientslimit * sizeof( client_t ), "clients" );
	for ( i = 0, cl = svs.clients; i < svs.maxclientslimit; i++, cl++ )
	{
		Q_memset( cl, 0, sizeof( *cl ) );
		cl->resourcesneeded.pNext = cl->resourcesneeded.pPrev = &cl->resourcesneeded;
		cl->resourcesonhand.pNext = cl->resourcesonhand.pPrev = &cl->resourcesonhand;
	}

	Cvar_SetValue( "deathmatch", ( svs.maxclients > 1 ) ? 1.0 : 0.0 );

	SV_AllocClientFrames();

	svs.maxclients = min( svs.maxclientslimit, svs.maxclients );
}

/*
=================
SV_HandleRconPacket

=================
*/
void SV_HandleRconPacket( void )
{
	char	*s;
	char	*c;

	MSG_BeginReading();
	MSG_ReadLong();		// skip the -1 marker

	s = MSG_ReadStringLine();

	Cmd_TokenizeString( s );

	c = Cmd_Argv( 0 );

	if ( !Q_strcmp( c, "getchallenge" ) )
		SVC_GetChallenge();
	else if ( !Q_stricmp( c, "challenge" ) )
		SVC_ServiceChallenge();
	else if ( !Q_strcmp( c, "rcon" ) )
		SV_Rcon( &net_from );
}

/*
=================
SV_CheckCmdTimes

=================
*/
void SV_CheckCmdTimes( void )
{
	int				i;
	client_t		*cl;
	static double	lastreset = 0;
	float			dif;

	if ( realtime - lastreset < 1.0 )
		return;

	lastreset = realtime;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		cl = &svs.clients[i];

		if ( !cl->connected || !cl->active )
			continue;

		if ( cl->connecttime == 0.0 )
			cl->connecttime = realtime;

		dif = cl->cmdtime + cl->connecttime - realtime;
		if ( dif > clockwindow.value )
		{
			cl->ignorecmdtime = clockwindow.value + realtime;
			cl->cmdtime = realtime - cl->connecttime;
			continue;
		}

		if ( dif < -clockwindow.value )
			cl->cmdtime = realtime - cl->connecttime;
	}
}

/*
=================
SV_CheckForRcon

=================
*/
void SV_CheckForRcon( void )
{
	if ( sv.active || cls.state != ca_dedicated || giActive == DLL_CLOSE || !host_initialized )
		return;

	while ( NET_GetPacket( NS_SERVER ) )
	{
		if ( SV_FilterPacket() )
		{
			SV_SendBan();
			continue;
		}

		if ( *(int *)net_message.data == -1 )
		{
			SV_HandleRconPacket();
			continue;
		}
	}
}

/*
=================
SV_IsSimulating

=================
*/
qboolean SV_IsSimulating( void )
{
	if ( !sv.paused && ( svs.maxclients > 1 || ( key_dest == key_game && ( cls.state == ca_active || cls.state == ca_dedicated ) ) ) )
		return TRUE;

	return FALSE;
}

/*
=================
SV_CheckMapDifferences

=================
*/
void SV_CheckMapDifferences( void )
{
	client_t		*cl;
	int				i;
	static double	lastcheck;

	if ( realtime - lastcheck < 5.0 )
		return;

	lastcheck = realtime;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( !cl->active )
			continue;

		if ( !cl->crcValue )
			continue;

		if ( cl->netchan.remote_address.type == NA_LOOPBACK )
			continue;

		if ( cl->crcValue != sv.worldmapCRC )
			cl->netchan.message.flags |= FSB_OVERFLOWED;
	}
}

/*
==================
SV_Frame

==================
*/
void SV_Frame( void )
{
	if ( !sv.active )
		return;

	gGlobalVariables.frametime = host_frametime;
	sv.oldtime = sv.time;

	SV_CheckCmdTimes();
	SV_ReadPackets();

	if ( SV_IsSimulating() )
	{
		SV_Physics();
		sv.time += host_frametime;
	}

	SV_QueryMovevarsChanged();
	SV_RequestMissingResourcesFromClients();
	SV_CheckTimeouts();
	SV_SendClientMessages();
	SV_CheckMapDifferences();
	SV_GatherStatistics();
	SV_CheckVoiceChanges();
	Steam_RunFrame();
}

/*
==================
SV_Drop_f

The client is going to disconnect, so remove the connection immediately
==================
*/
void SV_Drop_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	SV_EndRedirect();
	SV_BroadcastPrintf( "%s dropped\n", host_client->name );
	SV_DropClient( host_client, FALSE, "Client sent 'drop'" );
}

/*
==================
SV_RegisterDelta

==================
*/
void SV_RegisterDelta( char *name, char *loadfile )
{
	delta_t			*pdesc = NULL;
	delta_info_t	*p;

	if ( !DELTA_Load( name, &pdesc, loadfile ) )
	{
		Sys_Error( "Error parsing %s!!!\n", loadfile );
		return;
	}

	p = (delta_info_t *)Mem_ZeroMalloc( sizeof( delta_info_t ) );
	p->loadfile = Mem_Strdup( loadfile );
	p->name = Mem_Strdup( name );
	p->delta = pdesc;
	p->next = g_sv_delta;
	g_sv_delta = p;
}

/*
==================
SV_InitDeltas

==================
*/
void SV_InitDeltas( void )
{
	Con_DPrintf( "Initializing deltas\n" );

	SV_RegisterDelta( "clientdata_t", "delta.lst" );
	SV_RegisterDelta( "entity_state_t", "delta.lst" );
	SV_RegisterDelta( "entity_state_player_t", "delta.lst" );
	SV_RegisterDelta( "custom_entity_state_t", "delta.lst" );
	SV_RegisterDelta( "usercmd_t", "delta.lst" );
	SV_RegisterDelta( "weapon_data_t", "delta.lst" );
	SV_RegisterDelta( "event_t", "delta.lst" );

	g_pplayerdelta = SV_LookupDelta( "entity_state_player_t" );
	if ( !g_pplayerdelta )
		Sys_Error( "No entity_state_player_t encoder on server!\n" );

	g_pentitydelta = SV_LookupDelta( "entity_state_t" );
	if ( !g_pentitydelta )
		Sys_Error( "No entity_state_t encoder on server!\n" );

	g_pcustomentitydelta = SV_LookupDelta( "custom_entity_state_t" );
	if ( !g_pcustomentitydelta )
		Sys_Error( "No custom_entity_state_t encoder on server!\n" );

	g_pclientdelta = SV_LookupDelta( "clientdata_t" );
	if ( !g_pclientdelta )
		Sys_Error( "No clientdata_t encoder on server!\n" );

	g_pweapondelta = SV_LookupDelta( "weapon_data_t" );
	if ( !g_pweapondelta )
		Sys_Error( "No weapon_data_t encoder on server!\n" );

	g_peventdelta = SV_LookupDelta( "event_t" );
	if ( !g_peventdelta )
		Sys_Error( "No event_t encoder on server!\n" );
}

/*
==================
SV_InitEncoders

==================
*/
void SV_InitEncoders( void )
{
	delta_t			*pdesc = NULL;
	delta_info_t	*p;

	p = g_sv_delta;
	while ( p )
	{
		pdesc = p->delta;
		if ( Q_strlen( pdesc->conditionalencodename ) > 0 )
			pdesc->conditionalencode = DELTA_LookupEncoder( pdesc->conditionalencodename );

		p = p->next;
	}
}

/*
===============
SV_Init

===============
*/
void SV_Init( void )
{
	int			i;
	client_t	*client;

	Cmd_AddCommand( "banid", SV_BanId_f );
	Cmd_AddCommand( "removeid", SV_RemoveId_f );
	Cmd_AddCommand( "listid", SV_ListId_f );
	Cmd_AddCommand( "writeid", SV_WriteId_f );
	Cmd_AddCommand( "resetrcon", SV_ResetRcon_f );
	Cmd_AddCommand( "logaddress", SV_SetLogAddress_f );
	Cmd_AddCommand( "logaddress_add", SV_AddLogAddress_f );
	Cmd_AddCommand( "logaddress_del", SV_DelLogAddress_f );
	Cmd_AddCommand( "log", SV_ServerLog_f );
	Cmd_AddCommand( "serverinfo", SV_Serverinfo_f );
	Cmd_AddCommand( "localinfo", SV_Localinfo_f );
	Cmd_AddCommand( "showinfo", SV_ShowServerinfo_f );
	Cmd_AddCommand( "user", SV_User_f );
	Cmd_AddCommand( "users", SV_Users_f );
	Cmd_AddCommand( "dlfile", SV_BeginFileDownload_f );
	Cmd_AddCommand( "addip", SV_AddIP_f );
	Cmd_AddCommand( "removeip", SV_RemoveIP_f );
	Cmd_AddCommand( "listip", SV_ListIP_f );
	Cmd_AddCommand( "writeip", SV_WriteIP_f );
	Cmd_AddCommand( "dropclient", SV_Drop_f );
	Cmd_AddCommand( "spawn", SV_Spawn_f );
	Cmd_AddCommand( "new", SV_New_f );
	Cmd_AddCommand( "sendres", SV_SendRes_f );
	Cmd_AddCommand( "sendents", SV_SendEnts_f );
	Cmd_AddCommand( "fullupdate", SV_FullUpdate_f );

	Cvar_RegisterVariable( &sv_failuretime );
	Cvar_RegisterVariable( &sv_voicecodec );
	Cvar_RegisterVariable( &sv_voiceenable );
	Cvar_RegisterVariable( &sv_voicequality );
	Cvar_RegisterVariable( &rcon_password );
	Cvar_RegisterVariable( &sv_enableoldqueries );
	Cvar_RegisterVariable( &mp_consistency );
	Cvar_RegisterVariable( &sv_instancedbaseline );
	Cvar_RegisterVariable( &sv_contact );
	Cvar_RegisterVariable( &sv_region );
	Cvar_RegisterVariable( &sv_unlag );
	Cvar_RegisterVariable( &sv_maxunlag );
	Cvar_RegisterVariable( &sv_unlagpush );
	Cvar_RegisterVariable( &sv_unlagsamples );
	Cvar_RegisterVariable( &sv_filterban );
	Cvar_RegisterVariable( &sv_maxupdaterate );
	Cvar_RegisterVariable( &sv_minupdaterate );
	Cvar_RegisterVariable( &sv_logrelay );
	Cvar_RegisterVariable( &sv_lan );

	if ( PF_IsDedicatedServer() )
		Cvar_DirectSet( &sv_lan, "0" );
	else
		Cvar_DirectSet( &sv_lan, "1" );

	Cvar_RegisterVariable( &sv_lan_rate );
	Cvar_RegisterVariable( &sv_proxies );
	Cvar_RegisterVariable( &sv_outofdatetime );
	Cvar_RegisterVariable( &sv_visiblemaxplayers );
	Cvar_RegisterVariable( &sv_password );
	Cvar_RegisterVariable( &sv_aim );
	Cvar_RegisterVariable( &violence_hblood );
	Cvar_RegisterVariable( &violence_ablood );
	Cvar_RegisterVariable( &violence_hgibs );
	Cvar_RegisterVariable( &violence_agibs );
	Cvar_RegisterVariable( &sv_newunit );
	Cvar_RegisterVariable( &sv_gravity );
	Cvar_RegisterVariable( &sv_friction );
	Cvar_RegisterVariable( &sv_edgefriction );
	Cvar_RegisterVariable( &sv_stopspeed );
	Cvar_RegisterVariable( &sv_maxspeed );
	Cvar_RegisterVariable( &sv_footsteps );
	Cvar_RegisterVariable( &sv_accelerate );
	Cvar_RegisterVariable( &sv_stepsize );
	Cvar_RegisterVariable( &sv_clipmode );
	Cvar_RegisterVariable( &sv_bounce );
	Cvar_RegisterVariable( &sv_airmove );
	Cvar_RegisterVariable( &sv_airaccelerate );
	Cvar_RegisterVariable( &sv_wateraccelerate );
	Cvar_RegisterVariable( &sv_waterfriction );
	Cvar_RegisterVariable( &sv_skycolor_r );
	Cvar_RegisterVariable( &sv_skycolor_g );
	Cvar_RegisterVariable( &sv_skycolor_b );
	Cvar_RegisterVariable( &sv_skyvec_x );
	Cvar_RegisterVariable( &sv_skyvec_y );
	Cvar_RegisterVariable( &sv_skyvec_z );
	Cvar_RegisterVariable( &sv_timeout );
	Cvar_RegisterVariable( &sv_clienttrace );
	Cvar_RegisterVariable( &sv_zmax );
	Cvar_RegisterVariable( &sv_wateramp );
	Cvar_RegisterVariable( &sv_skyname );
	Cvar_RegisterVariable( &sv_maxvelocity );
	Cvar_RegisterVariable( &sv_cheats );

	if ( COM_CheckParm( "-dev" ) )
		Cvar_SetValue( "sv_cheats", 1 );

	Cvar_RegisterVariable( &sv_spectatormaxspeed );
	Cvar_RegisterVariable( &sv_allow_download );
	Cvar_RegisterVariable( &sv_allow_upload );
	Cvar_RegisterVariable( &sv_max_upload );
	Cvar_RegisterVariable( &sv_send_logos );
	Cvar_RegisterVariable( &sv_send_resources );
	Cvar_RegisterVariable( &sv_logbans );
	Cvar_RegisterVariable( &hpk_maxsize );
	Cvar_RegisterVariable( &mapcyclefile );
	Cvar_RegisterVariable( &motdfile );
	Cvar_RegisterVariable( &servercfgfile );
	Cvar_RegisterVariable( &mapchangecfgfile );
	Cvar_RegisterVariable( &lservercfgfile );
	Cvar_RegisterVariable( &logsdir );
	Cvar_RegisterVariable( &bannedcfgfile );
	Cvar_RegisterVariable( &sv_rcon_minfailures );
	Cvar_RegisterVariable( &sv_rcon_maxfailures );
	Cvar_RegisterVariable( &sv_rcon_minfailuretime );
	Cvar_RegisterVariable( &sv_rcon_banpenalty );
	Cvar_RegisterVariable( &sv_minrate );
	Cvar_RegisterVariable( &sv_maxrate );
	Cvar_RegisterVariable( &max_queries_sec );
	Cvar_RegisterVariable( &max_queries_sec_global );
	Cvar_RegisterVariable( &max_queries_window );
	Cvar_RegisterVariable( &sv_logblocks );
	Cvar_RegisterVariable( &sv_downloadurl );
	Cvar_RegisterVariable( &sv_version );
	Cvar_RegisterVariable( &sv_allow_dlfile );

	for ( i = 0; i < MAX_MODELS; i++ )
		_snprintf( localmodels[i], sizeof( localmodels[i] ), "*%i", i );

	svs.isSecure = FALSE;

	for ( i = 0; i < svs.maxclientslimit; i++ )
	{
		client = &svs.clients[i];

		SV_ClearFrames( &client->frames );
		Q_memset( client, 0, sizeof( *client ) );
		client->resourcesonhand.pNext = client->resourcesonhand.pPrev = &client->resourcesonhand;
		client->resourcesneeded.pNext = client->resourcesneeded.pPrev = &client->resourcesneeded;
	}

	PM_Init( &g_svmove );

	SV_AllocClientFrames();
	SV_InitDeltas();
}

/*
===============
SV_Shutdown

===============
*/
void SV_Shutdown( void )
{
	delta_info_t	*p, *n;

	p = g_sv_delta;
	while ( p )
	{
		n = p->next;

		if ( p->delta )
			DELTA_FreeDescription( &p->delta );

		Mem_Free( p->name );
		Mem_Free( p->loadfile );
		Mem_Free( p );

		p = n;
	}

	g_sv_delta = NULL;
}

/*
================
SV_CompareUserID

================
*/
qboolean SV_CompareUserID( USERID_t *id1, USERID_t *id2 )
{
	char	szID1_guess[64];
	char	szID2_guess[64];

	if ( !id1 || !id2 )
		return FALSE;

	if ( id1->idtype != id2->idtype )
		return FALSE;

	switch ( id1->idtype )
	{
	case AUTH_IDTYPE_STEAM:
	case AUTH_IDTYPE_VALVE:
		strncpy( szID1_guess, SV_GetIDString( id1 ), sizeof( szID1_guess ) - 1 );
		szID1_guess[sizeof( szID1_guess ) - 1] = 0;

		strncpy( szID2_guess, SV_GetIDString( id2 ), sizeof( szID2_guess ) - 1 );
		szID2_guess[sizeof( szID2_guess ) - 1] = 0;

		return _stricmp( szID1_guess, szID2_guess ) ? FALSE : TRUE;
	}

	return FALSE;
}

/*
================
SV_GetIDString

================
*/
char *SV_GetIDString( USERID_t *id )
{
	static char	idstr[64];

	idstr[0] = 0;

	if ( id )
	{
		switch ( id->idtype )
		{
		case AUTH_IDTYPE_STEAM:
			if ( sv_lan.value )
			{
				strcpy( idstr, "STEAM_ID_LAN" );
			}
			else if ( !id->m_SteamID )
			{
				strcpy( idstr, "STEAM_ID_PENDING" );
			}
			else
			{
				TSteamGlobalUserID	steam2ID;

				steam2ID = Steam_Steam3IDtoSteam2( id->m_SteamID );
				_snprintf( idstr, sizeof( idstr ) - 1, "STEAM_%u:%u:%u", steam2ID.m_SteamInstanceID, steam2ID.m_SteamLocalUserID.Split.High32bits, steam2ID.m_SteamLocalUserID.Split.Low32bits );
				idstr[sizeof( idstr ) - 1] = 0;
			}
			break;

		case AUTH_IDTYPE_VALVE:
			if ( sv_lan.value )
			{
				strcpy( idstr, "VALVE_ID_LAN" );
			}
			else if ( !id->m_SteamID )
			{
				strcpy( idstr, "VALVE_ID_PENDING" );
			}
			else
			{
				TSteamGlobalUserID	steam2ID;

				steam2ID = Steam_Steam3IDtoSteam2( id->m_SteamID );
				_snprintf( idstr, sizeof( idstr ) - 1, "VALVE_%u:%u:%u", steam2ID.m_SteamInstanceID, steam2ID.m_SteamLocalUserID.Split.High32bits, steam2ID.m_SteamLocalUserID.Split.Low32bits );
				idstr[sizeof( idstr ) - 1] = 0;
			}
			break;

		case AUTH_IDTYPE_LOCAL:
			strcpy( idstr, "HLTV" );
			break;

		default:
			strcpy( idstr, "UNKNOWN" );
			break;
		}
	}

	return idstr;
}

/*
================
SV_GetClientIDString

================
*/
char *SV_GetClientIDString( client_t *client )
{
	static char	idstr[64];

	idstr[0] = 0;

	if ( client )
	{
		if ( client->netchan.remote_address.type == NA_LOOPBACK && client->network_userid.idtype == AUTH_IDTYPE_VALVE )
		{
			_snprintf( idstr, sizeof( idstr ) - 1, "VALVE_ID_LOOPBACK" );
		}
		else
		{
			USERID_t	*id;

			id = &client->network_userid;
			_snprintf( idstr, sizeof( idstr ) - 1, "%s", SV_GetIDString( &client->network_userid ) );
		}
	}

	return idstr;
}

typedef struct
{
	unsigned int	iAppID;
	const char		*pGameDir;
} GameToAppIDMapItem_t;

GameToAppIDMapItem_t g_GameToAppIDMap[] =
{
	{ 10, "cstrike" },
	{ 20, "tfc" },
	{ 30, "dod" },
	{ 40, "dmc" },
	{ 50, "gearbox" },
	{ 60, "ricochet" },
	{ 70, "valve" },
	{ 80, "czero" },
	{ 100, "czeror" },
	{ 130, "bshift" },
	{ 150, "cstrike_beta" },
	{ 223710, "cryoffear" },
};

/*
================
GetGameAppID

================
*/
int GetGameAppID( void )
{
	char	gd[MAX_PATH];
	int		i;

	COM_FileBase( com_gamedir, gd );

	for ( i = 0; i < sizeof( g_GameToAppIDMap ) / sizeof( g_GameToAppIDMap[0] ); i++ )
	{
		if ( !_stricmp( g_GameToAppIDMap[i].pGameDir, gd ) )
			return g_GameToAppIDMap[i].iAppID;
	}

	return 223710;
}

/*
================
IsGameSubscribed

================
*/
qboolean IsGameSubscribed( const char *game )
{
	unsigned int	appID = 223710;
	int				i;

	for ( i = 0; i < sizeof( g_GameToAppIDMap ) / sizeof( g_GameToAppIDMap[0] ); i++ )
	{
		if ( !_stricmp( g_GameToAppIDMap[i].pGameDir, game ) )
		{
			appID = g_GameToAppIDMap[i].iAppID;
			break;
		}
	}

	if ( ISteamApps_BIsSubscribedApp( appID ) )
		return TRUE;

	return FALSE;
}
