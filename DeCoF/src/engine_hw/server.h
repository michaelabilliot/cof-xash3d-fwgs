// server.h -- the local server (sv_main.c and friends)

#ifndef SERVER_H
#define SERVER_H

#include "mathlib.h"
#include "const.h"
#include "progs.h"
#include "usercmd.h"
#include "delta_packet.h"
#include "custom.h"
#include "net.h"
#include "consistency.h"
#include "event.h"
#include "userid.h"
#include "info.h"
#include "cvar.h"

delta_t	*SV_LookupDelta( char *name );

extern	int		g_bIsCStrike;
extern	int		g_bIsCZero;

void	SetCStrikeFlags( void );

typedef enum
{
	RD_NONE,
	RD_CLIENT,
	RD_PACKET
} redirect_t;

extern	redirect_t	sv_redirected;
extern	char		outputbuf[1400];

void	SV_FlushRedirect( void );

typedef enum
{
	ss_dead,	// no map loaded
	ss_loading,	// spawning level edicts
	ss_active	// actively running
} server_state_t;

// server_t, checked against the Windows binary (sv at 0x025203A0). Doubles
// are 8 byte aligned here, unlike the Linux build: time at +0x10, worldmodel
// at +0x130. The precache limits of this build are larger than the Linux
// ones (768 models, sounds and generic files): num_resources at +0x3B948
// (SV_SendResources), models at +0x41D50 (SV_HullForBsp), edicts at +0x52160
// (SV_LinkEdict), signon at +0x54900.
struct model_s;
typedef struct edict_s edict_t;

typedef struct
{
	qboolean		active;				// false if only a net client
	qboolean		paused;
	qboolean		loadgame;			// handle connections specially

	double			time;
	double			oldtime;

	int				lastcheck;			// used by PF_checkclient
	double			lastchecktime;		// for monster ai

	char			name[64];			// map name
	char			oldname[64];
	char			startspot[64];
	char			modelname[64];		// maps/<name>.bsp, for model_precache[0]

	struct model_s	*worldmodel;
	CRC32_t			worldmapCRC;		// For detecting that client has a hacked local copy of map, the client will be dropped if this occurs.
	unsigned char	clientdllmd5[16];	// For detecting that client has a hacked local copy of the client dll

	resource_t		resourcelist[MAX_MODELS + MAX_SOUNDS + MAX_EVENTS];	// 1792 in this build
	int				num_resources;

	consistency_t	consistency_list[MAX_CONSISTENCY_LIST];
	int				num_consistency;

	char			*model_precache[MAX_MODELS];
	struct model_s	*models[MAX_MODELS];
	unsigned char	model_precache_flags[MAX_MODELS];
	struct event_s	event_precache[MAX_EVENTS];

	char			*sound_precache[MAX_SOUNDS];
	short			sound_precache_hashedlookup[MAX_SOUNDS_HASHLOOKUP_SIZE];
	qboolean		sound_precache_hashedlookup_built;

	char			*generic_precache[MAX_GENERIC];
	char			generic_precache_names[MAX_GENERIC][64];
	int				num_generic_names;

	char			*lightstyles[MAX_LIGHTSTYLES];

	int				num_edicts;
	int				max_edicts;
	edict_t			*edicts;			// can NOT be array indexed, because
										// edict_t is variable sized, but can
										// be used to reference the world ent
	struct entity_state_s	*baselines;		// parallel array to edicts, storing entity baselines
	struct extra_baselines_s	*instance_baselines;

	server_state_t	state;				// some actions are only valid during load

	sizebuf_t		datagram;
	byte			datagram_buf[MAX_DATAGRAM];

	sizebuf_t		reliable_datagram;	// copied to all clients at end of frame
	byte			reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t		multicast;			// Longest multicast message
	byte			multicast_buf[1024];

	sizebuf_t		spectator;			// Spectator channel
	byte			spectator_buf[1024];

	sizebuf_t		signon;
	byte			signon_data[32768];
} server_t;

// server_static_t, checked against the Windows binary (svs at 0x0251F320:
// maxclients +0x08, log +0x18, log.file +0x34, next_cleartime +0x38,
// stats +0x48, isSecure +0x80).
typedef struct
{
	qboolean	active;
	qboolean	net_log;
	netadr_t	net_address;
	void		*file;
} server_log_t;

typedef struct
{
	int		num_samples;
	int		at_capacity;
	int		at_empty;
	float	capacity_percent;
	float	empty_percent;
	int		minusers;
	int		maxusers;
	float	cumulative_occupancy;
	float	occupancy;
	int		num_sessions;
	float	cumulative_sessiontime;
	float	average_session_len;
	float	cumulative_latency;
	float	average_latency;
} server_stats_t;

typedef struct server_static_s
{
	qboolean		dll_initialized;	// Have we loaded the game dll.
	struct client_s	*clients;			// array of up to [maxclients] client slots.
	int				maxclients;			// Current max #
	int				maxclientslimit;	// Max allowed on server.
	int				spawncount;			// Number of servers spawned since start,
										// used to check late spawns (e.g., when d/l'ing lots of
										// data)
	int				serverflags;		// Episode completion information

	server_log_t	log;				// Log file information.

	double			next_cleartime;		// Next time to clear the stats
	double			next_sampletime;	// Next time to sample the stats
	server_stats_t	stats;				// Server statistics

	qboolean		isSecure;			// Is this a secure server?
} server_static_t;

// client_t, checked against the Windows binary (svs.clients stride 0x5018):
// netchan at +0x20, fakeclient +0x2548, edict +0x4B9C, name +0x4D08,
// resourcesonhand +0x4D34, resourcesneeded +0x4DBC, customdata +0x4E4C.
// usercmd_t is 4 bytes longer than in the Linux build and doubles are 8
// byte aligned, so the members sit further than the Linux offsets.
typedef struct
{
	double			senttime;
	float			ping_time;
	clientdata_t	clientdata;
	weapon_data_t	weapondata[64];
	packet_entities_t	entities;
} client_frame_t;

typedef struct client_s
{
	qboolean		active;				// false = client is free
	qboolean		spawned;			// false = don't send datagrams
	qboolean		fully_connected;	// true = client has fully connected, set after sendents command is received
	qboolean		connected;			// On server, but not spawned.
	qboolean		uploading;			// true = client uploading custom resources
	qboolean		hasusrmsgs;			// Whether this client has received the list of user messages
	qboolean		has_force_unmodified;	// true = mp_consistency is set and at least one file is forced to be consistent

	netchan_t		netchan;			// The client's net connection.

	int				chokecount;			// amount of choke since last client message
	int				delta_sequence;		// -1 = no compression.  This is where the server is creating the
										// compressed info from.

	qboolean		fakeclient;			// Bot
	qboolean		proxy;				// HLTV proxy

	usercmd_t		lastcmd;			// for filling in big drops and partial predictions

	double			connecttime;		// Time client connected
	double			cmdtime;			// Time we received last command
	double			ignorecmdtime;		// Time we should start accepting commands again

	float			latency;			// Average latency
	float			packet_loss;		// Average packet loss

	double			localtime;			// of last message
	double			nextping;			// next time to recalculate ping
	double			svtimebase;			// Server time base for this client

	// the datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.
	sizebuf_t		datagram;
	byte			datagram_buf[MAX_DATAGRAM];

	double			connection_started;	// Or time of disconnect for zombies
	double			next_messagetime;	// Time when we should send next world state update
	double			next_messageinterval;	// Default time to wait for next message

	qboolean		send_message;		// false - only send messages if the client has sent one
										//  and the bandwidth is not choked
	qboolean		skip_message;		// Defer message sending for one frame

	client_frame_t	*frames;			// updates can be deltad from here

	event_state_t	events;				// Per edict events

	edict_t			*edict;				// EDICT_NUM(clientnum+1)
	const edict_t	*pViewEntity;		// View Entity (camera or the client itself)

	int				userid;				// identifying number
	USERID_t		network_userid;

	char			userinfo[MAX_INFO_STRING];	// infostring
	qboolean		sendinfo;			// at end of frame, send info to all
										// this prevents malicious multiple broadcasts
	float			sendinfo_time;		// Time when userinfo was sent

	char			hashedcdkey[64];	// MD5 hash is 32 hex #'s, plus trailing 0
	char			name[32];			// for printing to other people
										// extracted from userinfo
	int				topcolor;
	int				bottomcolor;

	int				entityId;

	resource_t		resourcesonhand;	// Downloaded
	resource_t		resourcesneeded;	// Need to download
	FileHandle_t	upload;				// Handle for file upload
	qboolean		uploaddoneregistering;	// If client files have finished uploading
	customization_t	customdata;			// Head of custom (client) data list

	int				crcValue;			// For consistency checking

	int				lw;					// If user is predicting weapons locally
	int				lc;					//  If user is lag compensating

	char			physinfo[MAX_PHYSINFO_STRING];	// Physics info string

	qboolean		m_bLoopback;		// Does this client want to hear his own voice?
	uint32			m_VoiceStreams[2];	// Which other clients does this guy's voice stream go to?

	double			m_lastvoicetime;

	int				m_sendrescount;		// Count of times we've sent the resources to the client
	qboolean		m_bSentNewResponse;
} client_t;

// sv_main.c
extern	cvar_t	sv_allow_upload;
extern	cvar_t	sv_max_upload;

extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;				// local server

// pr_cmds.c
#define	SVC_LASTMSG			58		// last engine message (svc_sendcvarvalue2), user messages follow
#define	MAX_USER_MSG_DATA	192		// largest variable length user message

#ifndef WALKMOVE_NORMAL
#define WALKMOVE_NORMAL		0		// normal walkmove
#define WALKMOVE_WORLDONLY	1		// doesn't hit ANY entities, no matter what the solid type
#define WALKMOVE_CHECKONLY	2		// move, but don't touch triggers
#endif

int32	RandomLong( int32 lLow, int32 lHigh );
float	RandomFloat( float flLow, float flHigh );
int32	ran1( void );
float	fran1( void );
void	SeedRandomNumberGenerator( void );
void	SetMinMaxSize( edict_t *e, const float *min, const float *max, qboolean rotate );
void	PF_MessageBegin_I( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed );
void	PF_MessageEnd_I( void );
void	QueryClientCvarValue( const edict_t *player, const char *cvarName );
void	QueryClientCvarValue2( const edict_t *player, const char *cvarName, int requestID );
void	AlertMessage( int atype, char *szFmt, ... );
int		PF_precache_generic_I( char *s );
int		PF_IsDedicatedServer( void );

extern	int		g_groupmask;
extern	int		g_groupop;

#define GROUP_OP_AND	0
#define GROUP_OP_NAND	1

struct client_s;

extern	struct client_s	*host_client;	// current client

extern	cvar_t	sv_lan;
extern	cvar_t	sv_lan_rate;

void	SV_DropClient( struct client_s *cl, qboolean crash, char *fmt, ... );

// sv_main.c
void	SV_PropagateCustomizations( void );
void	SV_BroadcastPrintf( char *fmt, ... );
void	SV_BroadcastCommand( char *fmt, ... );

extern	int		num_servers;
extern	qboolean	allow_cheats;
extern	cvar_t	rcon_password;

#define	MAX_PACKET_ENTITIES	256		// doesn't count nails

// SV_Multicast destinations
#define	MSG_FL_NONE			0		// No flags
#define	MSG_FL_BROADCAST	(1<<0)	// Broadcast?
#define	MSG_FL_PVS			(1<<1)	// Send to PVS
#define	MSG_FL_PAS			(1<<2)	// Send to PAS
#define	MSG_FL_ONE			(1<<7)	// Send to single client

typedef enum
{
	sv_packet_nodelta,
	sv_packet_delta
} sv_delta_t;

// cmodel.c
byte	*CM_LeafPVS( int leafnum );
byte	*CM_LeafPAS( int leafnum );
void	CM_CalcPAS( struct model_s *pModel );
qboolean	CM_HeadnodeVisible( struct mnode_s *node, byte *visbits, int *first_visible_leafnum );
byte	*Mod_LeafPVS( struct mleaf_s *leaf, struct model_s *model );

// model.c
struct mleaf_s	*Mod_PointInLeaf( vec3_t p, struct model_s *model );
struct model_s	*Mod_ForName( const char *name, qboolean crash, qboolean trackCRC );

// r_misc.c
void	R_ForceCVars( qboolean mp );

// sv_phys.c
void	SV_Physics( void );
void	SV_CheckAllEnts( void );
void	SV_CheckVelocity( edict_t *ent );
qboolean	SV_RunThink( edict_t *ent );
void	SV_Impact( edict_t *e1, edict_t *e2, trace_t *ptrace );
int		ClipVelocity( vec_t *in, vec_t *normal, vec_t *out, float overbounce );
int		SV_FlyMove( edict_t *ent, float time, trace_t *steptrace );
void	SV_AddGravity( edict_t *ent );
void	SV_AddCorrectGravity( edict_t *ent );
void	SV_FixupGravityVelocity( edict_t *ent );
trace_t	SV_PushEntity( edict_t *ent, vec_t *push );
void	SV_PushMove( edict_t *pusher, float movetime );
int		SV_PushRotate( edict_t *pusher, float movetime );
void	SV_Physics_Pusher( edict_t *ent );
qboolean	SV_CheckWater( edict_t *ent );
float	SV_RecursiveWaterLevel( vec_t *center, float out, float in, int count );
float	SV_Submerged( edict_t *ent );
void	SV_Physics_None( edict_t *ent );
void	SV_Physics_Follow( edict_t *ent );
void	SV_Physics_Noclip( edict_t *ent );
void	SV_CheckWaterTransition( edict_t *ent );
void	SV_Physics_Toss( edict_t *ent );
void	PF_WaterMove( edict_t *pSelf );
void	SV_Physics_Step( edict_t *ent );
trace_t	SV_Trace_Toss( edict_t *ent, edict_t *ignore );

// sv_move.c
qboolean	SV_CheckBottom( edict_t *ent );
qboolean	SV_movetest( edict_t *ent, vec_t *move, qboolean relink );
qboolean	SV_movestep( edict_t *ent, vec_t *move, qboolean relink );

extern	edict_t	**g_moved_edict;
extern	vec3_t	*g_moved_from;

extern	cvar_t	sv_friction;
extern	cvar_t	sv_stopspeed;
extern	cvar_t	sv_gravity;
extern	cvar_t	sv_maxvelocity;
extern	cvar_t	sv_stepsize;
extern	cvar_t	sv_bounce;
extern	cvar_t	sv_clipmode;
extern	cvar_t	sv_airmove;
extern	cvar_t	sv_spectatormaxspeed;
extern	cvar_t	sv_airaccelerate;
extern	cvar_t	sv_wateraccelerate;
extern	cvar_t	sv_waterfriction;
extern	cvar_t	sv_zmax;
extern	cvar_t	sv_wateramp;
extern	cvar_t	sv_skyname;

// sv_user.c
extern	edict_t	*sv_player;

extern	cvar_t	sv_footsteps;
extern	cvar_t	sv_edgefriction;
extern	cvar_t	sv_maxspeed;
extern	cvar_t	sv_accelerate;
extern	cvar_t	sv_rollspeed;
extern	cvar_t	sv_rollangle;
extern	cvar_t	sv_voicecodec;
extern	cvar_t	sv_voicequality;
extern	cvar_t	sv_voiceenable;
extern	cvar_t	clockwindow;
extern	cvar_t	mp_consistency;
extern	cvar_t	sv_unlag;
extern	cvar_t	sv_maxunlag;
extern	cvar_t	sv_unlagpush;
extern	cvar_t	sv_unlagsamples;

void	SV_ShowServerinfo_f( void );
void	SV_SendEnts_f( void );
void	SV_FullUpdate_f( void );

void	SV_SendConsistencyList( sizebuf_t *msg );
int		SV_TransferConsistencyInfo( void );

#define	CMD_MAXBACKUP	30	// cmds[] of SV_ParseMove in this build

struct areanode_s;
struct pmtrace_s;
struct physent_s;
struct entity_state_s;
struct usercmd_s;

extern	qboolean	nofind;
extern	int			giSkip;

void	SV_ParseConsistencyResponse( client_t *pSenderClient );
qboolean	SV_FileInConsistencyList( const char *filename, consistency_t **ppconsist );
void	SV_PreRunCmd( void );
void	SV_CopyEdictToPhysent( struct physent_s *pe, int e, edict_t *check );
void	SV_AddLinksToPM_( struct areanode_s *node, float *pmove_mins, float *pmove_maxs );
void	SV_AddLinksToPM( struct areanode_s *node, vec_t *origin );
void	SV_PlayerRunPreThink( edict_t *player, float time );
qboolean	SV_PlayerRunThink( edict_t *ent, float frametime, double clienttimebase );
void	SV_CheckMovingGround( edict_t *player, float frametime );
void	SV_ConvertPMTrace( trace_t *dest, struct pmtrace_s *src, edict_t *ent );
void	SV_ForceFullClientsUpdate( void );
void	SV_RunCmd( struct usercmd_s *ucmd, int random_seed );
int		SV_ValidateClientCommand( char *pszCommand );
float	SV_CalcClientTime( client_t *cl );
void	SV_ComputeLatency( client_t *cl );
int		SV_UnlagCheckTeleport( vec_t *v1, vec_t *v2 );
void	SV_GetTrueOrigin( int player, vec_t *origin );
void	SV_GetTrueMinMax( int player, float **fmin, float **fmax );
struct entity_state_s	*SV_FindEntInPack( int index, packet_entities_t *pack );
void	SV_SetupMove( client_t *host_client );
void	SV_RestoreMove( client_t *host_client );
void	SV_ParseStringCommand( client_t *pSenderClient );
void	SV_ParseDelta( client_t *pSenderClient );
void	SV_EstablishTimeBase( client_t *cl, struct usercmd_s *cmds, int dropped, int numbackup, int numcmds );
void	SV_ParseMove( client_t *pSenderClient );
void	SV_ParseVoiceData( client_t *cl );
void	SV_IgnoreHLTV( client_t *cl );
void	SV_ParseCvarValue( client_t *cl );
void	SV_ParseCvarValue2( client_t *cl );
qboolean	SV_SetPlayer( int idnum );

// sv_pmove.c
void	PM_SV_PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
void	PM_SV_PlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch );
const char	*PM_SV_TraceTexture( int ground, float *vstart, float *vend );

// r_studio.c
int		R_GetStudioBounds( const char *filename, float *mins, float *maxs );
int		ModelFrameCount( struct model_s *model );

// gl_rmain.c, gl_rsurf.c
extern	int		r_visframecount;
extern	struct mleaf_s	*r_oldviewleaf;
void	R_MarkLeaves( void );

// host.c
extern	cvar_t	skill;
extern	cvar_t	deathmatch;
extern	cvar_t	coop;

// host_cmd.c
char	*Host_SaveGameDirectory( void );
extern	int		current_skill;
extern	int		gHostSpawnCount;

// pr_edict.c
int		IndexOfEdict( const edict_t *pEdict );
void	ReleaseEntityDLLFields( edict_t *pEdict );
void	InitEntityDLLFields( edict_t *pEdict );

// sys_dll.c
void	NotifyDedicatedServerUI( const char *message );

// sv_main.c
extern	cvar_t	sv_maxupdaterate;
extern	cvar_t	sv_minupdaterate;
extern	cvar_t	sv_filterban;
extern	cvar_t	sv_minrate;
extern	cvar_t	sv_maxrate;
extern	cvar_t	sv_timeout;
extern	cvar_t	sv_password;
extern	cvar_t	sv_proxies;
extern	cvar_t	sv_cheats;
extern	cvar_t	sv_aim;
extern	cvar_t	sv_clienttrace;
extern	cvar_t	sv_allow_download;
extern	cvar_t	sv_send_resources;
extern	cvar_t	sv_send_logos;
extern	cvar_t	sv_instancedbaseline;
extern	cvar_t	sv_contact;
extern	cvar_t	sv_visiblemaxplayers;
extern	cvar_t	sv_downloadurl;
extern	cvar_t	sv_allow_dlfile;
extern	cvar_t	mapcyclefile;
extern	cvar_t	motdfile;
extern	cvar_t	servercfgfile;
extern	cvar_t	lservercfgfile;
extern	cvar_t	mapchangecfgfile;
extern	cvar_t	logsdir;
extern	cvar_t	bannedcfgfile;
extern	cvar_t	hpk_maxsize;
extern	cvar_t	violence_hblood;
extern	cvar_t	violence_ablood;
extern	cvar_t	violence_hgibs;
extern	cvar_t	violence_agibs;
extern	cvar_t	sv_skycolor_r;
extern	cvar_t	sv_skycolor_g;
extern	cvar_t	sv_skycolor_b;
extern	cvar_t	sv_skyvec_x;
extern	cvar_t	sv_skyvec_y;
extern	cvar_t	sv_skyvec_z;

extern	int		sv_playermodel;
extern	int		g_userid;
extern	char	localinfo[];
extern	struct delta_s	*g_peventdelta;

void	SV_DownloadingModules( void );
void	SV_GatherStatistics( void );
void	SV_DeallocateDynamicData( void );
void	SV_ReallocateDynamicData( void );
void	SV_AllocClientFrames( void );
qboolean	SV_IsPlayerIndex( int index );
void	SV_ClearPacketEntities( client_frame_t *frame );
void	SV_AllocPacketEntities( client_frame_t *frame, int numents );
void	SV_ClearFrames( client_frame_t **frames );
void	SV_Serverinfo_f( void );
void	SV_Localinfo_f( void );
void	SV_User_f( void );
void	SV_Users_f( void );
void	SV_CountPlayers( int *clients );
void	SV_CountProxies( int *proxies );
void	SV_FindModelNumbers( void );
void	SV_StartParticle( const vec_t *org, const vec_t *dir, int color, int count );
void	SV_StartSound( int recipients, edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch );
qboolean	SV_BuildSoundMsg( edict_t *entity, int channel, const char *sample, int volume, float attenuation, int fFlags, int pitch, const float *origin, sizebuf_t *buffer );
int		SV_HashString( const char *string, int iBounds );
int		SV_LookupSoundIndex( const char *sample );
void	SV_BuildHashedSoundLookupTable( void );
void	SV_AddSampleToHashedLookupTable( const char *pszSample, int iSampleIndex );
qboolean	SV_ValidClientMulticast( client_t *client, int soundLeaf, int to );
void	SV_Multicast( edict_t *ent, vec_t *origin, int to, qboolean reliable );
void	SV_WriteMovevarsToClient( sizebuf_t *message );
void	SV_WriteDeltaDescriptionsToClient( sizebuf_t *msg );
void	SV_SetMoveVars( void );
void	SV_QueryMovevarsChanged( void );
void	SV_SendServerinfo( sizebuf_t *msg, client_t *client );
void	SV_SendResources( sizebuf_t *msg );
void	SV_WriteClientdataToMessage( client_t *client, sizebuf_t *msg );
void	SV_WriteSpawn( sizebuf_t *msg );
void	SV_SendUserReg( sizebuf_t *msg );
void	SV_New_f( void );
void	SV_SendRes_f( void );
void	SV_Spawn_f( void );
void	SV_CheckVoiceChanges( void );
void	SV_CheckUpdateRate( double *rate );
void	SV_RejectConnection( netadr_t *adr, char *fmt, ... );
void	SV_RejectConnectionForPassword( netadr_t *adr );
int		SV_GetFragmentSize( void *state );
qboolean	SV_FilterUser( USERID_t *userid );
int		SV_CheckProtocol( netadr_t *adr, int nProtocol );
int		SV_CheckChallenge( netadr_t *adr, int nChallengeValue );
int		SV_CheckIPRestrictions( netadr_t *adr, int nAuthProtocol );
int		SV_CheckIPConnectionReuse( netadr_t *adr );
int		SV_FinishCertificateCheck( netadr_t *adr, int nAuthProtocol, char *szRawCertificate, char *userinfo );
int		SV_CheckKeyInfo( netadr_t *adr, char *protinfo, unsigned short *port, int *pAuthProtocol, char *pszRaw, char *cdkey );
int		SV_CheckForDuplicateSteamID( client_t *client );
int		SV_CheckForDuplicateNames( char *userinfo, qboolean bIsReconnecting, int nExcludeSlot );
int		SV_CheckUserInfo( netadr_t *adr, char *userinfo, qboolean bIsReconnecting, int nReconnectSlot, char *name );
int		SV_FindEmptySlot( netadr_t *adr, int *pslot, client_t **ppClient );
void	SV_ConnectClient( void );
void	SVC_Ping( void );
void	SVC_GetChallenge( void );
void	SVC_ServiceChallenge( void );
void	SV_ResetModInfo( void );
int		SV_GetFakeClientCount( void );
qboolean	SV_GetModInfo( char *pszInfo, char *pszDL, int *version, int *size, qboolean *svonly, qboolean *cldll, char *pszHLVersion );
qboolean	RequireValidChallenge( netadr_t *adr );
qboolean	ValidInfoChallenge( netadr_t *adr, const char *nugget );
int		GetChallengeNr( netadr_t *adr );
qboolean	CheckChallengeNr( netadr_t *adr, int nChallengeValue );
void	ReplyServerChallenge( netadr_t *adr );
qboolean	ValidChallenge( netadr_t *adr, int challengeNr );
void	SVC_InfoString( void );
void	SVC_Info( qboolean bDetailed );
void	SVC_PlayerInfo( void );
void	SVC_RuleInfo( void );
int		SVC_GameDllQuery( const char *s );
void	SV_BeginRedirect( redirect_t rd, netadr_t *addr );
void	SV_EndRedirect( void );
void	SV_ResetRcon_f( void );
void	SV_AddFailedRcon( netadr_t *adr );
qboolean	SV_CheckRconFailure( netadr_t *adr );
int		SV_Rcon_Validate( void );
void	SV_Rcon( netadr_t *net_from );
void	SV_ConnectionlessPacket( void );
void	SV_CheckRate( client_t *cl );
void	SV_ProcessFile( client_t *cl, char *filename );
qboolean	SV_FilterPacket( void );
void	SV_SendBan( void );
void	SV_ReadPackets( void );
void	SV_CheckTimeouts( void );
int		SV_CalcPing( client_t *cl );
void	SV_FullClientUpdate( client_t *cl, sizebuf_t *sb );
void	SV_EmitEvents( client_t *cl, packet_entities_t *pack, sizebuf_t *msg );
void	SV_AddToFatPVS( vec_t *org, struct mnode_s *node );
byte	*SV_FatPVS( float *org );
void	SV_AddToFatPAS( vec_t *org, struct mnode_s *node );
byte	*SV_FatPAS( float *org );
int		SV_PointLeafnum( vec_t *p );
void	TRACE_DELTA( char *fmt, ... );
void	SV_SetCallback( int num, qboolean remove, qboolean custom, int *numbase, qboolean full, int offset );
void	SV_SetNewInfo( int newblindex );
void	SV_WriteDeltaHeader( int num, qboolean remove, qboolean custom, int *numbase, qboolean newbl, int newblindex, qboolean full, int offset );
void	SV_InvokeCallback( void );
int		SV_FindBestBaseline( int index, struct entity_state_s **baseline, struct entity_state_s *to, int num, qboolean custom );
int		SV_CreatePacketEntities( sv_delta_t type, client_t *client, packet_entities_t *to, sizebuf_t *msg );
void	SV_EmitPacketEntities( client_t *client, packet_entities_t *to, sizebuf_t *msg );
qboolean	SV_ShouldUpdatePing( client_t *client );
qboolean	SV_HasEventsInQueue( client_t *client );
void	SV_GetNetInfo( client_t *client, int *ping, int *packet_loss );
int		SV_CheckVisibility( edict_t *entity, unsigned char *pset );
void	SV_EmitPings( client_t *client, sizebuf_t *msg );
void	SV_WriteEntitiesToClient( client_t *client, sizebuf_t *msg );
void	SV_CleanupEnts( void );
qboolean	SV_SendClientDatagram( client_t *client );
void	SV_UpdateToReliableMessages( void );
void	SV_SkipUpdates( void );
void	SV_SendClientMessages( void );
void	SV_ExtractFromUserinfo( client_t *cl );
int		SV_ModelIndex( const char *name );
void	SV_AddResource( resourcetype_t type, char *name, int size, unsigned char flags, int index );
void	SV_CreateGenericResources( void );
void	SV_CreateResourceList( void );
void	SV_ClearCaches( void );
void	SV_WriteVoiceCodec( sizebuf_t *pBuf );
void	SV_CreateBaseline( void );
void	SV_BuildReconnect( sizebuf_t *msg );
void	SV_ReconnectAllClients( void );
void	SV_ActivateServer( int runPhysics );
void	SV_ServerShutdown( void );
int		SV_SpawnServer( qboolean bIsDemo, char *server, char *startspot );
void	SV_LoadEntities( void );
void	SV_ClearEntities( void );
int		RegUserMsg( const char *pszName, int iSize );
USERID_t	*SV_StringToUserID( const char *str );
void	SV_BanId_f( void );
void	Host_Kick_f( void );
void	SV_RemoveId_f( void );
void	SV_WriteId_f( void );
void	SV_ListId_f( void );
void	SV_AddIP_f( void );
void	SV_RemoveIP_f( void );
void	SV_ListIP_f( void );
void	SV_WriteIP_f( void );
void	SV_KickPlayer( int nPlayerSlot, int nReason );
void	SV_InactivateClients( void );
void	SV_FailDownload( char *filename );
const char	*Q_stristr( const char *pStr, const char *pSearch );
qboolean	IsSafeFileToDownload( const char *filename );
void	SV_BeginFileDownload_f( void );
void	SV_SetMaxclients( void );
void	SV_HandleRconPacket( void );
void	SV_CheckCmdTimes( void );
void	SV_CheckForRcon( void );
qboolean	SV_IsSimulating( void );
void	SV_CheckMapDifferences( void );
void	SV_Frame( void );
void	SV_Drop_f( void );
void	SV_RegisterDelta( char *name, char *loadfile );
void	SV_InitDeltas( void );
void	SV_InitEncoders( void );
void	SV_Init( void );
void	SV_Shutdown( void );
qboolean	SV_CompareUserID( USERID_t *id1, USERID_t *id2 );
char	*SV_GetIDString( USERID_t *id );
char	*SV_GetClientIDString( client_t *client );
int		GetGameAppID( void );
qboolean	IsGameSubscribed( const char *game );

// sv_steam3.cpp
typedef unsigned short		SteamInstanceID_t;
typedef unsigned __int64	SteamLocalUserID_t;

typedef struct
{
	unsigned int	Low32bits;
	unsigned int	High32bits;
} TSteamSplitLocalUserID;

typedef struct TSteamGlobalUserID
{
	SteamInstanceID_t	m_SteamInstanceID;

	union
	{
		SteamLocalUserID_t		As64bits;
		TSteamSplitLocalUserID	Split;
	} m_SteamLocalUserID;
} TSteamGlobalUserID;

const char	*Steam_GetCommunityName( void );
int		Steam_GSInitiateGameConnection( void *pData, int cbMaxData, unsigned __int64 steamID, unsigned int unIPServer, unsigned short usPortServer, qboolean bSecure );
void	Steam_GSTerminateGameConnection( unsigned int unIPServer, unsigned short usPortServer );
void	Steam_SetCVar( const char *pchKey, const char *pchValue );
qboolean	Steam_NotifyClientConnect( client_t *cl, const void *pvSteam2Key, unsigned int ucbSteam2Key );
qboolean	Steam_NotifyBotConnect( client_t *cl );
void	Steam_NotifyClientDisconnect( client_t *cl );
void	Steam_NotifyOfLevelChange( void );
void	Steam_Shutdown( void );
void	Steam_Activate( void );
void	Steam_RunFrame( void );
void	Steam_ClientRunFrame( void );
void	Steam_InitClient( void );
void	Steam_ShutdownClient( void );
uint64	Steam_GSGetSteamID( void );
qboolean	Steam_GSBSecure( void );
qboolean	Steam_GSBLoggedOn( void );
qboolean	Steam_GSBSecurePreference( void );
uint64	Steam_StringToSteamID( const char *pStr );
TSteamGlobalUserID	Steam_Steam3IDtoSteam2( uint64 unSteamID );
const char	*Steam_GetGSUniverse( void );
void	Steam_HandleIncomingPacket( byte *data, int len, int fromip, unsigned short port );
void	Master_SetMaster_f( void );

// sv_upld.c
void	SV_ParseResourceList( client_t *pSenderClient );
void	SV_ClearResourceLists( client_t *cl );
void	SV_MoveToOnHandList( resource_t *pResource );
void	SV_RequestMissingResourcesFromClients( void );

// sv_user.c
void	SV_ExecuteClientMessage( client_t *cl );

// ipratelimit.cpp
qboolean	CheckIP( netadr_t adr );

// steam_api flat interface
__declspec( dllimport ) int	ISteamMasterServerUpdater_HandleIncomingPacket( const void *pData, int cbData, unsigned int srcIP, unsigned short srcPort );
__declspec( dllimport ) char	ISteamApps_BIsSubscribedApp( unsigned int appID );
__declspec( dllimport ) uint64	ISteamGameServer_CreateUnauthenticatedUserConnection( void );
__declspec( dllimport ) int	ISteamGameServer_BUpdateUserData( uint64 steamIDUser, const char *pchPlayerName, unsigned int uScore );

// master.c
qboolean	Master_IsLanGame( void );

#endif // SERVER_H
