// client.h -- the client state (cl_main.c and friends)
//
// Include after quakedef.h and mathlib.h.

#ifndef CLIENT_H
#define CLIENT_H

#include "const.h"
#include "info.h"
#include "usercmd.h"
#include "entity_state.h"
#include "progs.h"
#include "cl_entity.h"
#include "dlight.h"
#include "custom.h"
#include "screenfade.h"
#include "net.h"
#include "event.h"
#include "consistency.h"
#include "delta_packet.h"

#define	MAX_DEMOS			32

#define MAX_CLIENTS			32

#define	MAX_EDICT_BITS		11		// # of bits needed to represent max edicts

#define MAX_MODELS			768		// this engine build: cl.model_precache has 768 entries
#define MAX_SOUNDS			768		// and so has cl.sound_precache
#define MAX_RESOURCE_LIST	1792	// MAX_MODELS + MAX_SOUNDS + MAX_EVENTS

#define MAX_DLIGHTS			32
#define MAX_ELIGHTS			64
#define MAX_LIGHTSTYLES		64
#define MAX_STYLESTRING		64
#define MAX_EFRAGS			640
#define MAX_VISEDICTS		512
#define MAX_STATIC_ENTITIES	32
#define MAX_BASELINES		64

#define MAX_CL_STATS		32

#define	MULTIPLAYER_BACKUP	64		// how many data slots to use when in multiplayer (must be power of 2)

#define	SIGNONS				2		// signon messages to receive before connected

//
// stats are integers communicated to the client by the server
//
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster

#ifndef PITCH
// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2
#endif

#ifndef ON_EPSILON
#define	ON_EPSILON		0.1			// point on plane side epsilon
#endif

typedef struct cmd_s
{
	usercmd_t	cmd;
	float		senttime;		// Time sent
	float		receivedtime;	// Time ack'd
	float		frame_lerp;		// lerp value when packet was sent
	qboolean	processedfuncs;	// whether the frame's functions have been processed
	qboolean	heldback;		// held back by the client
	int			sendsize;		// size of the packet
} cmd_t;

typedef struct frame_s
{
	double				receivedtime;	// time message was received, or -1
	double				latency;
	qboolean			invalid;		// true if the packet_entities delta was invalid
	qboolean			choked;

	entity_state_t		playerstate[MAX_CLIENTS];

	double				time;
	clientdata_t		clientdata;
	weapon_data_t		weapondata[64];
	packet_entities_t	packet_entities;

	unsigned short		clientbytes;
	unsigned short		playerinfobytes;
	unsigned short		packetentitybytes;
	unsigned short		tentitybytes;
	unsigned short		soundbytes;
	unsigned short		eventbytes;
	unsigned short		usrbytes;
	unsigned short		voicebytes;
	unsigned short		msgbytes;
} frame_t;

typedef struct
{
	int		length;
	char	map[MAX_STYLESTRING];
} lightstyle_t;

typedef struct player_info_s
{
	// User id on server
	int				userid;

	// User info string
	char			userinfo[MAX_INFO_STRING];

	// Name
	char			name[32];

	// Spectator or not, unused
	int				spectator;

	int				ping;
	int				packet_loss;

	// skin information
	char			model[64];
	int				topcolor;
	int				bottomcolor;

	// last frame rendered
	int				renderframe;

	// Gait frame estimation
	int				gaitsequence;
	float			gaitframe;
	float			gaityaw;
	vec3_t			prevgaitorigin;

	customization_t	customdata;

	char			hashedcdkey[16];
	unsigned __int64	m_nSteamID;
} player_info_t;

typedef enum
{
	ca_dedicated,		// This is a dedicated server, client code is inactive
	ca_disconnected,	// full screen console with no connection
	ca_connecting,		// netchan_t established, waiting for svc_serverdata
	ca_connected,		// processing data lists, donwloading, etc
	ca_uninitialized,
	ca_active			// everything is in, so frames can be rendered
} cactive_t;

typedef struct
{
	qboolean	bUsed;
	float		fTime;
	int			nBytesRemaining;
} downloadtime_t;

typedef struct
{
	qboolean		doneregistering;
	int				percent;
	qboolean		downloadrequested;
	downloadtime_t	rgStats[8];
	int				nCurStat;
	int				nTotalSize;
	int				nTotalToTransfer;
	int				nRemainingToTransfer;
	float			fLastStatusUpdate;
	qboolean		custom;
} incomingtransfer_t;

typedef struct
{
	int		nStartPercent;
	int		nClientSoundFadePercent;
	double	soundFadeStartTime;
	int		soundFadeOutTime;
	int		soundFadeHoldTime;
	int		soundFadeInTime;
} soundfade_t;

// the client_static_t structure is persistant through an arbitrary number
// of server connections
//
// Checked against the Windows binary (cls at 0x02DCF280: netchan 0x02DCF288,
// netchan.message 0x02DCF2E4, datagram 0x02DCF7A8, connect_time 0x02DD2760,
// userinfo 0x02DD30E4, demoplayback 0x02DD33F4, dl.nCurStat 0x02DD3598,
// packet_loss 0x02DD35C0, soundfade 0x02DD35D8, md5_clientdll 0x02DD36F8,
// connect_stream 0x02DD371C, passive 0x02DD3730, spectator 0x02DD3734,
// GameServerSteamID 0x02DD3748; this build has no build_num, the int at
// 0x02DD3750 is the global cl_playerindex). Between dl and
// packet_loss this build has 16 bytes the Linux build lacks: the state of
// the Steam resource wait of CL_MAIN.C (CL_WaitForResources_guess starts it
// with the file system's WaitForResources, CL_CheckWaitForResources_guess
// polls it, CL_WaitForResourcesDone_guess ends it, CL_Disconnect cancels it).
typedef struct client_static_s
{
	cactive_t			state;

	// network stuff
	netchan_t			netchan;

	// private userinfo for sending to masterless servers
	sizebuf_t			datagram;			// unreliable stuff gets sent in this
	byte				datagram_buf[MAX_DATAGRAM];

	double				connect_time;		// for connection retransmits
	int					connect_retry;

	int					challenge;

	byte				authprotocol;

	int					userid;

	char				trueaddress[32];

	float				slist_time;

	// connection information
	int					signon;				// 0 to SIGNONS

	char				servername[MAX_PATH];	// name of server from original connect
	char				mapstring[64];

	char				spawnparms[2048];

	// private userinfo for sending to masterless servers
	char				userinfo[MAX_INFO_STRING];

	float				nextcmdtime;		// When can we send the next command packet?
	int					lastoutgoingcommand;	// Sequence number of last outgoing command

	// demo loop control
	int					demonum;			// -1 = don't play demos
	char				demos[MAX_DEMOS][16];	// when not playing

	// demo recording info must be here, because record is started before
	// entering a map (and clearing client_state_t)
	qboolean			demorecording;
	qboolean			demoplayback;
	qboolean			timedemo;

	float				demostarttime;
	int					demostartframe;

	int					forcetrack;			// -1 = use normal cd track

	FileHandle_t		demofile;
	FileHandle_t		demoheader;			// For saving startup data to start playing a demo midstream.
	qboolean			demowaiting;		// don't record until a non-delta message is sent
	qboolean			demoappending;		// Appending to a previous recording.
	char				demofilename[MAX_PATH];
	int					demoframecount;

	int					td_lastframe;		// to meter out one message a frame
	int					td_startframe;		// host_framecount at start
	float				td_starttime;		// realtime at second frame of timedemo

	incomingtransfer_t	dl;					// Status of resource download

	qboolean			waitingforresources_guess;	// a Steam resource wait is running
	int					waitforresources_guess;		// its handle (WaitForResourcesHandle_t)
	double				waitforresourcestime_guess;	// Sys_FloatTime() when it started

	float				packet_loss;
	double				packet_loss_recalc_time;

	int					playerbits;			// player bits for the current player

	soundfade_t			soundfade;

	char				physinfo[MAX_PHYSINFO_STRING];	// Physics info string

	unsigned char		md5_clientdll[16];

	// Location of the last server we've connected to
	netadr_t			game_stream;
	// Location of the server we are connecting to
	netadr_t			connect_stream;

	qboolean			passive;
	qboolean			spectator;
	qboolean			director;
	qboolean			fSecureClient;
	qboolean			isVAC2Secure;

	unsigned __int64	GameServerSteamID;
} client_static_t;

extern	client_static_t	cls;

// the client_state_t structure is wiped completely at every
// server signon
//
// Checked against the Windows binary (cl at 0x02DD5A80: resourcesonhand
// 0x02DD5A84, num_resources 0x02E11394, parsecount 0x02E115A8, viewangles
// 0x02E1166C, light_level 0x02E11A00, intermission 0x02E11A08, mtime
// 0x02E11A10, time 0x02E11A20, frames 0x02E11A30, commands 0x02F1CA30,
// predicted_frames 0x02F1DE30, playernum 0x02F82B34, model_precache
// 0x02F83B38, sound_precache 0x02F8473C, consistency_list 0x02F8533C,
// maxclients 0x02F8AB6C, viewent 0x02F8AB88, players 0x02F8B770,
// instanced_baseline 0x02F90170, events 0x02F95678, downloadUrl 0x02F96C78).
// Against the Linux layout: usercmd_t and clientdata_t have 4 more bytes,
// local_state_t keeps 64 weapons, the resource and precache tables are
// larger, and 4 bytes follow light_level.
typedef struct
{
	int				max_edicts;

	resource_t		resourcesonhand;
	resource_t		resourcesneeded;
	resource_t		resourcelist[MAX_RESOURCE_LIST];
	int				num_resources;

	qboolean		need_force_consistency_response;

	char			serverinfo[MAX_SERVERINFO_STRING];

	int				servercount;		// server identification for prespawns

	int				validsequence;		// this is the sequence number of the last good
										// world snapshot/update we got. If this is 0, we can't
										// render a frame yet

	int				parsecount;			// server message counter
	int				parsecountmod;		// modulo with network window

	int				stats[MAX_CL_STATS];	// health, etc
	int				weapons;			// bit flags

	usercmd_t		cmd;				// sent to server

	vec3_t			viewangles;
	vec3_t			punchangle;
	vec3_t			crosshairangle;
	vec3_t			simorg;
	vec3_t			simvel;
	vec3_t			simangles;
	vec_t			predicted_origins[64][3];
	vec3_t			prediction_error;
	float			idealpitch;
	vec3_t			viewheight;

	screenfade_t	sf;

	qboolean		paused;				// send over by server

	int				onground;
	int				moving;
	int				waterlevel;
	int				usehull;
	float			maxspeed;

	int				pushmsec;
	int				light_level;
	int				cof_light_extra_guess;	// CoF engine: 4 more bytes here, no code reads them

	int				intermission;		// don't change view angle, full screen, et

	double			mtime[2];			// the timestamp of the last two messages
	double			time;				// clients view of time, time should be between
										// servertime and oldservertime to generate
										// a lerp point for other data
	double			oldtime;			// previous cl.time, time-oldtime is used
										// to decay light values and smooth step ups

	frame_t			frames[MULTIPLAYER_BACKUP];
	cmd_t			commands[MULTIPLAYER_BACKUP];
	local_state_t	predicted_frames[MULTIPLAYER_BACKUP];
	int				delta_sequence;

	int				playernum;
	event_t			event_precache[MAX_EVENTS];
	struct model_s	*model_precache[MAX_MODELS];
	int				model_precache_count;
	struct sfx_s	*sound_precache[MAX_SOUNDS];
	consistency_t	consistency_list[MAX_CONSISTENCY_LIST];
	int				num_consistency;

	int				highentity;

	char			levelname[40];		// for display on solo scoreboard
	int				maxclients;
	int				gametype;
	int				viewentity;

	// refresh related state
	struct model_s	*worldmodel;		// cl_entitites[0].model
	efrag_t			*free_efrags;
	int				num_entities;		// held in cl_entities array
	int				num_statics;		// held in cl_staticentities array
	cl_entity_t		viewent;			// the gun model

	int				cdtrack;			// cd audio
	int				looptrack;

	CRC32_t			serverCRC;			// To determine if client is playing hacked .map. (entities lump is skipped)
	unsigned char	clientdllmd5[16];

	float			weaponstarttime;
	int				weaponsequence;

	int				fPrecaching;

	dlight_t		*pLight;

	player_info_t	players[MAX_CLIENTS];
	entity_state_t	instanced_baseline[MAX_BASELINES];
	int				instanced_baseline_number;

	CRC32_t			mapCRC;

	event_state_t	events;

	char			downloadUrl[128];
} client_state_t;

extern	client_state_t	cl;

extern	efrag_t			cl_efrags[MAX_EFRAGS];
extern	cl_entity_t		*cl_entities;
extern	cl_entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
extern	lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
extern	dlight_t		cl_dlights[MAX_DLIGHTS];
extern	dlight_t		cl_elights[MAX_ELIGHTS];

extern	int				cl_numvisedicts;
extern	cl_entity_t		*cl_visedicts[MAX_VISEDICTS];

extern	cvar_t	cl_name;
extern	cvar_t	cl_shownet;
extern	cvar_t	cl_timeout;
extern	cvar_t	cl_nodelta;
extern	cvar_t	cl_idealpitchscale;
extern	cvar_t	cl_himodels;
extern	cvar_t	cl_gaitestimation;
extern	cvar_t	cl_showevents;
extern	cvar_t	cl_clockreset;
extern	cvar_t	cl_needinstanced;
extern	cvar_t	cl_solid_players;
extern	cvar_t	cl_showmessages;
extern	cvar_t	cl_weaponlistfix;
extern	cvar_t	cl_allow_download;
extern	cvar_t	cl_download_ingame;
extern	cvar_t	cl_fixtimerate;
extern	cvar_t	cl_updaterate;
extern	cvar_t	cl_showfps;
extern	cvar_t	cl_cmdrate;
extern	cvar_t	fs_lazy_precache;
extern	cvar_t	fs_precache_timings;
extern	cvar_t	fs_perf_warnings;
extern	cvar_t	fs_startup_timings;

extern	int			cl_playerindex;
extern	qboolean	g_bRedirectedToProxy;
extern	qboolean	cl_inmovie;
extern	float		g_LastScreenUpdateTime;

typedef enum
{
	CAREER_NONE = 0,
	CAREER_LOADING,
	CAREER_PLAYING,
} CareerStateType;

extern	CareerStateType	g_careerState;	// host_cmd.c

//
// cl_main.c
//
void	AddStartupTiming( const char *name );
void	SetupStartupTimings( void );
void	PrintStartupTimings( void );
void	CL_Init( void );
void	CL_Shutdown( void );
void	CL_CheckClientState( void );
void	CL_UpdateSoundFade( void );
void	CL_ConnectionlessPacket( void );
qboolean	CL_PrecacheResources( void );
void	CL_MoveToOnHandList( resource_t *pResource );
void	CL_RegisterCustomization( resource_t *resource );
void	CL_ProcessFile( qboolean successfully_received, const char *filename );
void	CL_ReadPackets( void );
resource_t	*CL_AddResource( resourcetype_t type, char *name, int size, qboolean bFatalIfMissing, int index );
void	CL_ClearClientState( void );
void	CL_ClearState( qboolean bQuiet );
void	CL_Disconnect( void );
void	CL_Disconnect_f( void );
void	CL_ShutDownClientStatic( void );
void	CL_SendConnectPacket( void );
void	CL_CheckForResend( void );
void	CL_SignonReply( void );
void	CL_WaitForResources_guess( void );
void	CL_CheckWaitForResources_guess( void );
dlight_t	*CL_AllocDlight( int key );
dlight_t	*CL_AllocElight( int key );
void	CL_DecayLights( void );
qboolean	UserIsConnectedOnLoopback( void );
void	CL_Move( void );
void	CL_SendResourceListBlock( void );
void	CL_HudMessage( const char *pMessage );
void	CL_GetPlayerHulls( void );
void	CL_KeepConnectionActive( void );
struct model_s	*CL_GetModelByIndex( int index );
struct model_s	*CL_GetWorldModel( void );
int		CL_GetServerCount( void );

//
// cl_demo.c
//
extern	cvar_t	cl_gamegauge;

void	CL_RecordHUDCommand( char *cmdname );
qboolean	CL_ReadDemoMessage( void );
void	CL_StopPlayback( void );
void	CL_Stop_f( void );
void	CL_Record_f( void );
void	CL_PlayDemo_f( void );
void	CL_ViewDemo_f( void );
void	CL_TimeDemo_f( void );
void	CL_GameGauge_f( void );
void	CL_ListDemo_f( void );
void	CL_AppendDemo_f( void );
void	CL_RemoveDemo_f( void );
void	CL_SwapDemo_f( void );
void	CL_SetDemoInfo_f( void );
void	CL_BeginDemoStartup( void );
void	CL_WriteDemoStartup( int start, sizebuf_t *msg );
void	CL_WriteDemoMessage( int start, sizebuf_t *msg );
void	CL_WavePlayLen_f( void );

//
// cl_parse.c
//
extern	int			msg_buckets[64];
extern	double		parsecounttime;
extern	int			last_data[64];
extern	int			total_data[64];
extern	qboolean	g_bUpdatingSecurityModules;

cl_entity_t	*CL_EntityNum( int num );
void	AddNewUserMsg( void );
void	DispatchUserMsg( int iMsg );
int		DispatchDirectUserMsg( const char *pszName, int iSize, void *pBuf );
void	CL_ShutDownUsrMessages( void );
void	CL_Parse_Sound( void );
void	CL_UpdateUserinfo( void );
qboolean	CL_CheckFile( sizebuf_t *msg, char *filename );
void	CL_ClearResourceList( resource_t *pList );
void	CL_ClearResourceLists( void );
qboolean	CL_CheckCRCs( char *pszMap );
void	CL_PrecacheBSPModels( char *pfilename );
void	CL_SendConsistencyInfo( sizebuf_t *msg );
void	CL_RegisterResources( sizebuf_t *msg );
void	CL_AddToResourceList( resource_t *pResource, resource_t *pList );
void	CL_RemoveFromResourceList( resource_t *pResource );
int		CL_EstimateNeededResources( void );
void	CL_BatchResourceRequest( void );
void	CL_StartResourceDownloading( char *pszMessage, qboolean bCustom );
void	CL_ParseConsistencyInfo( void );
void	CL_ParseResourceList( void );
void	CL_ParseResourceLocation( void );
customization_t	*CL_PlayerHasCustomization( int nPlayerNum, resourcetype_t type );
void	CL_RemoveCustomization( int nPlayerNum, customization_t *pRemove );
void	CL_ParseCustomization( void );
qboolean	CL_RequestMissingResources( void );
void	CL_DeallocateDynamicData( void );
void	CL_ReallocateDynamicData( int nMaxClients );
void	CL_CreateCustomizationList( void );
qboolean	CL_CheckGameDirectory( char *gamedir );
int		CL_BuildMapCycleListHints( char **hints, char *mapCycleMsg, char *firstMap );
void	CL_ParseServerInfo( void );
void	CL_ParseListen( void );
void	CL_ParseBaseline( void );
void	CL_ParseStatic( void );
void	CL_ParseStaticSound( void );
void	CL_ParseMovevars( void );
void	CL_ParseSoundFade( void );
void	CL_Parse_DeltaDescription( void );
void	CL_Restore( const char *fileName );
void	CL_DumpMessageLoad_f( void );
void	CL_TransferMessageData( void );
void	CL_ResetFrame( frame_t *frame );
char	*CL_MsgInfo( int cmd );
void	CL_WriteErrorMessage( int starting_count, int current_count, sizebuf_t *msg );
void	CL_WriteMessageHistory( int starting_count, int cmd );
void	CL_ParseServerMessage( qboolean normal_message );
void	CL_ParseHLTV( void );

//
// cl_parsefn.c
//
typedef struct event_hook_s
{
	struct event_hook_s	*next;
	char				*name;
	void				( *pfnEvent )( struct event_args_s *args );
} event_hook_t;

void	CL_AdjustClock( void );
void	CL_Parse_Time( void );
void	CL_Parse_ClientData( void );
void	CL_Parse_Disconnect( void );
void	CL_Parse_Version( void );
void	CL_Set_ServerExtraInfo( void );
void	CL_Parse_SetView( void );
void	CL_Parse_Print( void );
void	CL_Parse_CenterPrint( void );
qboolean	ValidStuffText( const char *cmd );
void	CL_Parse_StuffText( void );
void	CL_Parse_ServerInfo( void );
void	CL_Parse_SetAngle( void );
void	CL_Parse_AddAngle( void );
void	CL_Parse_CrosshairAngle( void );
void	CL_Parse_LightStyle( void );
void	CL_Parse_StopSound( void );
void	CL_Parse_Particle( void );
void	CL_Parse_SpawnBaseline( void );
void	CL_Parse_SpawnStatic( void );
void	CL_Parse_TempEntity( void );
void	CL_Parse_SetPause( void );
void	CL_Parse_SignonNum( void );
void	CL_Parse_SpawnStaticSound( void );
void	CL_Parse_CDTrack( void );
void	CL_Parse_Restore( void );
void	CL_Parse_WeaponAnim( void );
void	CL_Parse_DecalName( void );
void	CL_Parse_RoomType( void );
void	CL_Parse_NewUserMsg( void );
void	CL_Parse_UpdateUserInfo( void );
void	CL_Parse_Damage( void );
void	CL_Parse_KilledMonster( void );
void	CL_Parse_FoundSecret( void );
void	CL_Parse_Intermission( void );
void	CL_Parse_Finale( void );
void	CL_Parse_CutScene( void );
void	CL_Parse_PacketEntities( void );
void	CL_Parse_DeltaPacketEntities( void );
void	CL_Parse_Choke( void );
void	CL_Parse_ResourceList( void );
void	CL_Parse_ResourceLocation( void );
void	CL_Parse_NewMoveVars( void );
void	CL_Parse_ResourceRequest( void );
void	CL_Parse_Customization( void );
void	CL_Parse_SoundFade( void );
void	CL_ResetEvent( event_info_t *ei );
void	CL_CalcPlayerVelocity( int idx, vec_t *velocity );
void	CL_DescribeEvent( int slot, char *eventname );
void	CL_FireEvents( void );
event_info_t	*CL_FindEmptyEvent( void );
event_info_t	*CL_FindUnreliableEvent( void );
void	CL_ClientQueueEvent( int flags, int index, int client_index, float delay, struct event_args_s *pargs );
void	CL_QueueEvent( int flags, int index, float delay, struct event_args_s *pargs );
void	CL_ParseEvent( void );
void	CL_Parse_ReliableEvent( void );
void	CL_Parse_Pings( void );
void	CL_Parse_HLTV( void );
void	CL_Parse_Director( void );
void	CL_Parse_Timescale( void );
void	CL_Parse_VoiceInit( void );
void	CL_Parse_VoiceData( void );
event_hook_t	*CL_FindEventHook( char *name );
void	CL_HookEvent( char *name, void ( *pfnEvent )( struct event_args_s *args ) );
void	CL_InitEventSystem( void );
void	CL_FlushEventSystem( void );
void	CL_ParseFileTxferFailed( void );
void	CL_Send_CvarValue( void );
void	CL_Send_CvarValue2( void );

//
// cl_ents.c
//
qboolean	CL_ParametricMove( cl_entity_t *ent );
void	CL_ProcessEntityUpdate( cl_entity_t *ent );
int		CL_ParseDeltaHeader( qboolean *remove, qboolean *custom, int *numbase, qboolean *newbl, int *newblindex, qboolean full, int *offset );
void	CL_FlushEntityPacket( qboolean needbitencodingframe );
void	CL_ClearPacket( packet_entities_t *pPacket );
int		CL_InterpolateModel( cl_entity_t *e );
void	CL_ResetLatchedState( int pnum, packet_entities_t *pack, cl_entity_t *ent );
void	CL_ProcessPlayerState( int playerindex, entity_state_t *state );
void	CL_ProcessPacket( frame_t *frame );
qboolean	CL_IsPlayerIndex( int index );
void	CL_SetPacketFlag( packet_entities_t *pack, int index, int flag );
void	CL_ResetFrameStats( frame_t *frame );
void	CL_ParseClientdata( void );
void	CL_ParsePacketEntities( qboolean delta, int *playerbits );
void	CL_PrintEntity( cl_entity_t *ent );
void	CL_Particle( vec_t *origin, int color, float life, int zpos, int zvel );
void	CL_TouchLight( dlight_t *dl );
void	CL_PlayerFlashlight( void );
void	CL_AddModelEffects( int keynum, cl_entity_t *ent, struct model_s *model );
void	CL_LinkCustomEntity( cl_entity_t *ent, entity_state_t *state );
void	CL_LinkPacketEntities( void );
void	CL_LinkPlayers( void );
void	CL_AddStateToEntlist( struct physent_s *pe, entity_state_t *state );
void	CL_SetSolidEntities( void );
void	CL_SetUpPlayerPrediction( qboolean dopred, qboolean bIncludeLocalClient );
void	CL_SetSolidPlayers( int playernum );
void	CL_MoveAiments( void );
float	CL_LerpPoint( void );
void	CL_EmitEntities( void );

//
// cl_pred.c
//
extern	cvar_t	cl_showerror;
extern	cvar_t	cl_nosmooth;
extern	cvar_t	cl_smoothtime;
extern	int		g_i;

void	CL_CheckPredictionError( void );
void	CL_RunUsercmd( local_state_t *from, local_state_t *to, usercmd_t *u, qboolean runfuncs, double *pfElapsed, unsigned int random_seed );
void	CL_SetIdealPitch( void );
void	CL_InterpolateVector( float frac, vec_t *src, vec_t *dest, vec_t *output );
void	CL_GetTargetTime( float *target );
void	CL_PredictMove( qboolean repredicting );
void	CL_PushPMStates( void );
void	CL_PopPMStates( void );
void	CL_SetLastUpdate( void );
void	CL_RedoPrediction( void );
void	CL_InitPrediction( void );

//
// cl_pmove.c
//
void	PM_CL_PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
const char	*PM_CL_TraceTexture( int ground, float *vstart, float *vend );
void	PM_CL_PlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch );

//
// cl_tent.c, cl_extrap.c, cl_spectator.c
//
extern	cvar_t	ex_interp;
extern	cvar_t	dev_overview;
extern	local_state_t	spectatorState;

void	CL_InitSpectator( void );

void	CL_TempEntInit( void );
void	CL_InitTEnts( void );
void	CL_ParseTEnt( void );
void	CL_InitExtrap( void );
qboolean	CL_FindInterpolationUpdates( cl_entity_t *ent, float targettime, position_history_t **ph0, position_history_t **ph1, int *ph0Index );
void	CL_UpdatePositions( cl_entity_t *ent );
void	CL_ResetPositions( cl_entity_t *ent );
void	CL_ComputePlayerOrigin( cl_entity_t *ent );
void	CL_TempEntUpdate( void );

//
// download.cpp, voice.cpp
//
void	CL_HTTPStop_f( void );
void	CL_QueueHTTPDownload( const char *filename );
int		CL_CanUseHTTPDownload( void );
int		CL_GetDownloadQueueSize( void );
void	CL_MarkMapAsUsingHTTPDownload( void );
qboolean	Voice_GetLoopback( void );
qboolean	Voice_Init( const char *pCodecName, int quality );
void	Voice_LocalPlayerTalkingAck( int );	// this build passes an argument the function never reads
int		Voice_AssignChannel( int nEntity );
int		Voice_GetChannel( int nEntity );
int		Voice_AddIncomingData( int nChannel, const char *pchData, int nCount, int iSequenceNumber );

#endif // CLIENT_H
