// cl_demo.c -- demo recording and playback

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "crc.h"
#include "host.h"
#include "client.h"
#include "net.h"
#include "GL_MODEL.H"
#include "protocol.h"
#include "sound.h"
#include "shake.h"
#include "tmessage.h"
#include "cdll_int.h"
#include "mem.h"
#include "ref_params.h"
#include "demo_api.h"
#include "../pm_shared/pm_movevars.h"
#include "cl_demo.h"

#define MAX_POSSIBLE_MSG	65536

// demo entry flags
#define FDEMO_TITLE			0x01	// Show title
#define FDEMO_PLAY			0x04	// Playing cd track
#define FDEMO_FADE_IN_SLOW	0x08	// Fade in (slow)
#define FDEMO_FADE_IN_FAST	0x10	// Fade in (fast)
#define FDEMO_FADE_OUT_SLOW	0x20	// Fade out (slow)
#define FDEMO_FADE_OUT_FAST	0x40	// Fade out (fast)

// cdll_int.c
typedef struct client_data_s
{
	// fields that cannot be modified  (ie. have no effect if changed)
	vec3_t	origin;

	// fields that can be changed by the cldll
	vec3_t	viewangles;
	int		iWeaponBits;
	float	fov;	// field of view
} client_data_t;

void	hudWeaponAnim( int iAnim, int body );
void	ClientDLL_DemoUpdateClientData( client_data_t *cdat );
void	ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer );

// DemoPlayerWrapper.cpp
void	DemoPlayer_SetActive( qboolean state );
int		DemoPlayer_IsActive( void );
int		DemoPlayer_LoadGame( char *filename );
int		DemoPlayer_ReadDemoMessage( unsigned char *buffer, int size );
void	DemoPlayer_ReadNetchanState( int *incoming_sequence, int *incoming_acknowledged, int *incoming_reliable_acknowledged, int *incoming_reliable_sequence, int *outgoing_sequence, int *reliable_sequence, int *last_reliable_sequence );
void	DemoPlayer_GetDemoViewInfo( ref_params_t *rp, float *view, int *viewmodel );
void	DemoPlayer_Stop( void );
void	DemoPlayer_ShowGUI( void );

// r_studio.c, view.c, tmessage.c, cl_main.c
struct sfx_s	*CL_LookupSound( const char *pName );
int		V_ScreenFade( const char *pszName, int iSize, void *pbuf );
void	SetDemoMessage( const char *pszMessage, float fFadeInTime, float fFadeOutTime, float fHoldTime );
int		CL_GetFragmentSize( void *state );

extern	char	*rgpszrawsentence[];
extern	float	scr_fov_value;
extern	float	scr_con_current;
extern	char	gpszVersionString[];

#pragma pack( push, 1 )

typedef struct sequence_info_s
{
	int		incoming_sequence;
	int		incoming_acknowledged;
	int		incoming_reliable_acknowledged;
	int		incoming_reliable_sequence;
	int		outgoing_sequence;
	int		reliable_sequence;
	int		last_reliable_sequence;
	int		length;
} sequence_info_t;

typedef struct demo_command_s
{
	byte	cmd;
	float	time;
	int		frame;
} demo_command_t;

typedef struct hud_command_s
{
	byte	cmd;
	float	time;
	int		frame;
	char	szNameBuf[64];
} hud_command_t;

typedef struct cl_demo_data_s
{
	byte	cmd;
	float	time;
	int		frame;

	vec3_t	origin;
	vec3_t	viewangles;
	int		iWeaponBits;
	float	fov;
} cl_demo_data_t;

typedef struct demo_anim_s
{
	byte	cmd;
	float	time;
	int		frame;
	int		anim;
	int		body;
} demo_anim_t;

typedef struct demo_event_s
{
	byte	cmd;
	float	time;
	int		frame;
	int		flags;
	int		idx;
	float	delay;
} demo_event_t;

typedef struct demo_sound1_s
{
	byte	cmd;
	float	time;
	int		frame;
	int		channel;
	int		length;
} demo_sound1_s;

typedef struct demo_sound2_s
{
	float	volume;
	float	attenuation;
	int		flags;
	int		pitch;
} demo_sound2_s;

#pragma pack( pop )

static float	g_demotimescale = 1.0;
cvar_t			cl_gamegauge = { "cl_gg", "0" };

ScreenFade		sf_demo;

char			gDemoMessageBuffer[512];

client_textmessage_t	tm_demomessage =
{
	0,				// effect
	255, 255, 255, 255,
	255, 255, 255, 255,
	-1,				// x
	-1,				// y
	0, 0,			// fadein, fadeout
	0,				// holdtime
	0,				// fxtime
	"__DEMOMESSAGE__",
	gDemoMessageBuffer
};

demoentry_t		*pCurrentEntry;
int				nCurEntryIndex;
demodirectory_t	demodir;
qboolean		bInFadeout;
demoheader_t	demoheader;

int		CL_DemoAPIRecording( void );
int		CL_DemoAPIPlayback( void );
int		CL_DemoAPITimedemo( void );
void	CL_WriteClientDLLMessage( int size, unsigned char *buf );
void	CL_FinishTimeDemo( void );
void	CL_FinishGameGauge( void );
qboolean	CL_ReadDemoMessage_OLD( void );

/*
====================
CL_DemoOutTime
====================
*/
float CL_DemoOutTime( void )
{
	return realtime;
}

/*
====================
CL_DemoInTime
====================
*/
float CL_DemoInTime( void )
{
	return g_demotimescale * realtime;
}

static demo_info_t	dem_frames[32];
demo_info_t			g_rp;

/*
====================
CL_FixPointers
====================
*/
void CL_FixPointers( demo_info_t *info )
{
	info->rp.cmd = &info->cmd;
	info->rp.movevars = &info->movevars;
}

/*
====================
CL_DemoCopyFileChunk
====================
*/
void CL_DemoCopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize )
{
	int		copysize;
	char	copybuf[1024];

	copysize = nSize;
	while ( copysize > 1024 )
	{
		FS_Read( copybuf, 1024, 1, src );
		FS_Write( copybuf, 1024, 1, dst );
		copysize -= 1024;
	}

	FS_Read( copybuf, copysize, 1, src );
	FS_Write( copybuf, copysize, 1, dst );

	FS_Flush( src );
	FS_Flush( dst );
}

/*
====================
CL_SetDemoViewInfo
====================
*/
void CL_SetDemoViewInfo( ref_params_t *rp, vec_t *view, int viewmodel )
{
	g_rp.rp = *rp;
	CL_FixPointers( &g_rp );

	g_rp.cmd = *rp->cmd;
	g_rp.movevars = *rp->movevars;

	VectorCopy( view, g_rp.view );
	g_rp.viewmodel = viewmodel;
}

/*
====================
CL_GetDemoViewInfo_OLD
====================
*/
void CL_GetDemoViewInfo_OLD( ref_params_t *rp, vec_t *view, int *viewmodel )
{
	float			targettime;
	int				i = 0;
	float			frac;
	vec3_t			delta;
	float			d;
	float			ang1, ang2;
	demo_info_t		*from = NULL, *to;
	usercmd_t		*oldcmd;
	movevars_t		*oldmv;
	int				oldviewport[4];

	for ( i = 0; i < 4; i++ )
		oldviewport[i] = rp->viewport[i];

	oldmv = rp->movevars;
	oldcmd = rp->cmd;

	*rp = dem_frames[0].rp;

	for ( i = 0; i < 4; i++ )
		rp->viewport[i] = oldviewport[i];

	rp->cmd = oldcmd;
	rp->movevars = oldmv;

	if ( cls.timedemo )
	{
		VectorCopy( dem_frames[0].view, view );
		*viewmodel = dem_frames[0].viewmodel;
		return;
	}

	targettime = CL_DemoInTime() - cls.demostarttime;
	targettime -= 0.02;

	to = &dem_frames[0];
	for ( i = 1; i < 31; i++ )
	{
		from = &dem_frames[i];
		CL_FixPointers( from );

		if ( from->timestamp == 0 )
			break;

		if ( from->timestamp < targettime )
			break;

		to = from;
	}

	if ( from && from->timestamp != 0 && from->timestamp != to->timestamp )
	{

		frac = ( targettime - from->timestamp ) / ( to->timestamp - from->timestamp );
		if ( frac > 1.0 )
			frac = 1.0;
		else if ( frac < 0.0 )
			frac = 0;

		VectorSubtract( to->rp.vieworg, from->rp.vieworg, delta );
		VectorMA( from->rp.vieworg, frac, delta, rp->vieworg );

		for ( i = 0; i < 3; i++ )
		{
			ang1 = to->rp.viewangles[i];
			ang2 = from->rp.viewangles[i];
			d = ang1 - ang2;
			if ( d > 180 )
				d -= 360;
			else if ( d < -180 )
				d += 360;

			rp->viewangles[i] = from->rp.viewangles[i] + d * frac;
		}

		COM_NormalizeAngles( rp->viewangles );

		VectorSubtract( to->rp.simvel, from->rp.simvel, delta );
		VectorMA( from->rp.simvel, frac, delta, rp->simvel );

		VectorSubtract( to->rp.simorg, from->rp.simorg, delta );
		VectorMA( from->rp.simorg, frac, delta, rp->simorg );

		for ( i = 0; i < 3; i++ )
			rp->viewheight[i] = from->rp.viewheight[i] + ( to->rp.viewheight[i] - from->rp.viewheight[i] ) * frac;

		VectorSubtract( to->view, from->view, delta );
		VectorMA( from->view, frac, delta, view );

		*viewmodel = to->viewmodel;
	}
}

/*
====================
CL_GetDemoViewInfo
====================
*/
void CL_GetDemoViewInfo( ref_params_t *rp, float *view, int *viewmodel )
{
	if ( !DemoPlayer_IsActive() )
		CL_GetDemoViewInfo_OLD( rp, view, viewmodel );
	else
		DemoPlayer_GetDemoViewInfo( rp, view, viewmodel );
}

/*
====================
CL_AppendDemo_f

appenddemo <demoname> <cd>
====================
*/
void CL_AppendDemo_f( void )
{
	char			name[MAX_PATH];
	int				track;
	int				c;
	byte			cmd;
	float			f;
	FileHandle_t	fp;
	int				copysize;
	char			szTempName[MAX_PATH];
	char			szMapName[MAX_PATH];
	int				frame;
	int				swlen;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Appenddemo only available when not running or recording a demo.\n" );
		return;
	}

	if ( cls.state != ca_active )
	{
		Con_Printf( "You must be in an active map spawned on a machine that allows creation of files in %s\n", com_gamedir );
		return;
	}

	c = Cmd_Argc();
	if ( c != 2 && c != 3 )
	{
		Con_Printf( "appenddemo <demoname> <cd track>\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	// get the cd track
	if ( c == 3 )
	{
		track = Q_atoi( Cmd_Argv( 2 ) );
		Con_Printf( "Forcing CD track to %i\n", track );
	}
	else
	{
		track = -1;
	}

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Appending to demo %s.\n", name );

	Q_strncpy( cls.demofilename, name, 255 );
	cls.demofilename[255] = 0;

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open demo file %s for appending.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".dm2" );

	cls.demofile = FS_Open( szTempName, "w+b" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	copysize = FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	CL_DemoCopyFileChunk( cls.demofile, fp, copysize );
	FS_Close( fp );

	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		_unlink( szTempName );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	COM_FileBase( cl.worldmodel->name, szMapName );
	if ( Q_strcmp( demoheader.szMapName, szMapName ) )
	{
		Con_Printf( "%s was recorded on a different map, cannot append.\n", name );
		_unlink( szTempName );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		_unlink( szTempName );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( int ), 1, cls.demofile );

	if ( demodir.nEntries < 1 || demodir.nEntries > 1024 )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * ( demodir.nEntries + 1 ) );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, cls.demofile );

	nCurEntryIndex = demodir.nEntries;
	demodir.nEntries++;
	pCurrentEntry = &demodir.p_rgEntries[ nCurEntryIndex ];

	Q_memset( pCurrentEntry, 0, sizeof( demoentry_t ) );

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );

	pCurrentEntry->nEntryType = 1;
	_snprintf( pCurrentEntry->szDescription, sizeof( pCurrentEntry->szDescription ), "Playback '%i'", demodir.nEntries - 1 );
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0;
	pCurrentEntry->nOffset = FS_Tell( cls.demofile );

	cls.demostarttime = CL_DemoOutTime();
	cls.demoframecount = 0;
	cls.demostartframe = host_framecount;

	// Demo playback should read this as an incoming message.
	cmd = 2;
	FS_Write( &cmd, sizeof( byte ), 1, cls.demofile );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( float ), 1, cls.demofile );
	frame = host_framecount - cls.demostartframe;
	swlen = LittleLong( frame );
	FS_Write( &swlen, sizeof( int ), 1, cls.demofile );

	cls.demowaiting = true;
	cls.demorecording = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
	cls.demoappending = true;
}

/*
====================
CL_SwapDemo_f

swapdemo <demoname> <seg#> <seg#>
====================
*/
void CL_SwapDemo_f( void )
{
	char			name[MAX_PATH];
	int				c;
	FileHandle_t	fp;
	char			szTempName[MAX_PATH];
	char			szOriginalName[MAX_PATH];
	int				nSegment1;
	int				nSegment2;
	demoentry_t		*oldentry, *newentry;
	demoentry_t		temp;
	int				n, i;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Swapdemo only available when not running or recording a demo.\n" );
		return;
	}

	c = Cmd_Argc();
	if ( c != 4 )
	{
		Con_Printf( "Swapdemo <demoname> <seg#> <seg#>\nSwaps segments, segment 1 cannot be moved\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	nSegment1 = Q_atoi( Cmd_Argv( 2 ) );
	if ( nSegment1 <= 1 )
	{
		Con_Printf( "Cannot swap the STARTUP segment.\n" );
		return;
	}

	nSegment2 = Q_atoi( Cmd_Argv( 3 ) );
	if ( nSegment1 <= 1 )
	{
		Con_Printf( "Cannot swap the STARTUP segment.\n" );
		return;
	}

	nSegment1--;
	nSegment2--;

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Swapping segment %i for %i from demo %s.\n", nSegment1 + 1, nSegment2 + 1, name );

	Q_strncpy( szOriginalName, name, 255 );
	szOriginalName[255] = 0;

	fp = FS_Open( szOriginalName, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open demo file %s for swapping.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".dm2" );

	cls.demofile = FS_Open( szTempName, "w+b" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	n = FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	CL_DemoCopyFileChunk( cls.demofile, fp, n );

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader_t ), 1, fp );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( int ), 1, fp );

	if ( demodir.nEntries < 1 || demodir.nEntries > 1024 )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	oldentry = &demodir.p_rgEntries[ nSegment1 ];
	temp = *oldentry;
	newentry = &demodir.p_rgEntries[ nSegment2 ];
	*oldentry = *newentry;
	*newentry = temp;

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demodir.nEntries, sizeof( int ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( szOriginalName );
	rename( szTempName, szOriginalName );

	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
}

/*
====================
CL_SetDemoInfo_f

setdemoinfo <demoname> <seg#> <info ...>
====================
*/
void CL_SetDemoInfo_f( void )
{
	char			name[MAX_PATH];
	int				c;
	FileHandle_t	fp;
	char			szTempName[MAX_PATH];
	char			szOriginalName[MAX_PATH];
	int				nSegment;
	demoentry_t		*newentry;
	int				n, i;
	qboolean		bDone;
	int				nCurArg;
	char			*pFlag;
	char			*pValue;
	char			*pDirection;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Setdemoinfo only available when not running or recording a demo.\n" );
		return;
	}

	c = Cmd_Argc();
	if ( c < 3 )
	{
		Con_Printf( "Setdemoinfo <demoname> <seg#> <info ...>\n" );
		Con_Printf( "   title \"text\"\n" );
		Con_Printf( "   play tracknum\n" );
		Con_Printf( "   fade <in | out> <fast | slow>\n\n" );
		Con_Printf( "Use -option to disable, e.g., -title\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	nSegment = Q_atoi( Cmd_Argv( 2 ) );
	if ( nSegment <= 1 )
	{
		Con_Printf( "Cannot Setdemoinfo the STARTUP segment.\n" );
		return;
	}

	nSegment--;

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Setting info for segment %i in demo %s.\n", nSegment + 1, name );

	Q_strncpy( szOriginalName, name, 255 );
	szOriginalName[255] = 0;

	fp = FS_Open( szOriginalName, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open demo file %s for Setdemoinfo.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".dm2" );

	cls.demofile = FS_Open( szTempName, "w+b" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	n = FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	CL_DemoCopyFileChunk( cls.demofile, fp, n );

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader_t ), 1, fp );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( int ), 1, fp );

	if ( demodir.nEntries < 1 || demodir.nEntries > 1024 )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		FS_Close( cls.demofile );
		FS_Close( fp );
		_unlink( szTempName );
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	newentry = &demodir.p_rgEntries[ nSegment ];

	bDone = false;
	nCurArg = 3;
	while ( !bDone )
	{
		pFlag = Cmd_Argv( nCurArg );
		if ( !pFlag || !pFlag[0] )
			break;

		nCurArg++;

		if ( !Q_stricmp( pFlag, "-TITLE" ) )
		{
			newentry->nFlags &= ~FDEMO_TITLE;
		}
		else if ( !Q_stricmp( pFlag, "-PLAY" ) )
		{
			newentry->nFlags &= ~FDEMO_PLAY;
		}
		else if ( !Q_stricmp( pFlag, "-FADE" ) )
		{
			newentry->nFlags &= ~( FDEMO_FADE_IN_SLOW | FDEMO_FADE_IN_FAST | FDEMO_FADE_OUT_SLOW | FDEMO_FADE_OUT_FAST );
		}
		else if ( !Q_stricmp( pFlag, "TITLE" ) )
		{
			pValue = Cmd_Argv( nCurArg );
			nCurArg++;
			if ( pValue && pValue[0] )
			{
				Q_strncpy( newentry->szDescription, pValue, sizeof( newentry->szDescription ) - 1 );
				newentry->szDescription[ sizeof( newentry->szDescription ) - 1 ] = 0;
				newentry->nFlags |= FDEMO_TITLE;
			}
			else
			{
				Con_Printf( "Title flag requires a double-quoted value.\n" );
			}
		}
		else if ( !Q_stricmp( pFlag, "PLAY" ) )
		{
			pValue = Cmd_Argv( nCurArg );
			nCurArg++;
			if ( pValue && pValue[0] )
			{
				newentry->nCDTrack = Q_atoi( pValue );
				newentry->nFlags |= FDEMO_PLAY;
			}
			else
			{
				Con_Printf( "Play flag requires a cd track #.\n" );
			}
		}
		else if ( !Q_stricmp( pFlag, "FADE" ) )
		{
			pDirection = Cmd_Argv( nCurArg );
			nCurArg++;
			if ( pDirection && pDirection[0] )
			{
				if ( !Q_stricmp( pDirection, "IN" ) )
				{
					pValue = Cmd_Argv( nCurArg );
					nCurArg++;
					if ( pValue && pValue[0] )
					{
						if ( !Q_stricmp( pValue, "FAST" ) )
							newentry->nFlags |= FDEMO_FADE_IN_FAST;
						else if ( !Q_stricmp( pValue, "SLOW" ) )
							newentry->nFlags |= FDEMO_FADE_IN_SLOW;
						else
							Con_Printf( "Fade flag requires a speed (fast or slow).\n" );
					}
					else
					{
						Con_Printf( "Fade flag requires a speed (fast or slow).\n" );
					}
				}
				else if ( !Q_stricmp( pDirection, "OUT" ) )
				{
					pValue = Cmd_Argv( nCurArg );
					nCurArg++;
					if ( pValue && pValue[0] )
					{
						if ( !Q_stricmp( pValue, "FAST" ) )
							newentry->nFlags |= FDEMO_FADE_OUT_FAST;
						else if ( !Q_stricmp( pValue, "SLOW" ) )
							newentry->nFlags |= FDEMO_FADE_OUT_SLOW;
						else
							Con_Printf( "Fade flag requires a speed (fast or slow).\n" );
					}
					else
					{
						Con_Printf( "Fade flag requires a speed (fast or slow).\n" );
					}
				}
				else
				{
					Con_Printf( "Fade flag requires a direction (in or out).\n" );
				}
			}
			else
			{
				Con_Printf( "Fade flag requires a direction and speed (in fast, e.g.)\n" );
			}
		}
		else
		{
			Con_Printf( "Setdemoinfo, unrecognized flag:  %s\n", pFlag );
			bDone = true;
		}
	}

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demodir.nEntries, sizeof( int ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( szOriginalName );
	rename( szTempName, szOriginalName );

	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
}

/*
====================
CL_RemoveDemo_f

removedemo <demoname> <segment to remove>
====================
*/
void CL_RemoveDemo_f( void )
{
	char			name[MAX_PATH];
	int				c;
	FileHandle_t	fp;
	char			szTempName[MAX_PATH];
	char			szOriginalName[MAX_PATH];
	int				nSegmentToRemove;
	demodirectory_t	newdir;
	demoentry_t		*oldentry, *newentry;
	int				n, i;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Removedemo only available when not running or recording a demo.\n" );
		return;
	}

	c = Cmd_Argc();
	if ( c != 3 )
	{
		Con_Printf( "removedemo <demoname> <segment to remove>\nSegment 1 cannot be removed\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	nSegmentToRemove = Q_atoi( Cmd_Argv( 2 ) );
	if ( nSegmentToRemove <= 1 )
	{
		Con_Printf( "Cannot remove the STARTUP segment.\n" );
		return;
	}

	nSegmentToRemove--;

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Removing segment %i from demo %s.\n", nSegmentToRemove + 1, name );

	Q_strncpy( szOriginalName, name, 255 );
	szOriginalName[255] = 0;

	fp = FS_Open( szOriginalName, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open demo file %s for segment removal.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".dm2" );

	cls.demofile = FS_Open( szTempName, "w+b" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader_t ), 1, fp );
	FS_Write( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		FS_Close( fp );
		cls.demofile = NULL;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		FS_Close( cls.demofile );
		FS_Close( fp );
		cls.demofile = NULL;
		return;
	}

	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( int ), 1, fp );

	if ( demodir.nEntries < 1 || demodir.nEntries > 1024 )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		FS_Close( cls.demofile );
		FS_Close( fp );
		cls.demofile = NULL;
		return;
	}

	if ( demodir.nEntries == 1 )
	{
		Con_Printf( "ERROR: Can't remove last directory entry.\n" );
		FS_Close( cls.demofile );
		FS_Close( fp );
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	newdir.nEntries = demodir.nEntries - 1;
	newdir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * newdir.nEntries );

	n = 0;
	for ( i = 0; i < demodir.nEntries; i++ )
	{
		if ( i == nSegmentToRemove )
			continue;

		oldentry = &demodir.p_rgEntries[i];
		newentry = &newdir.p_rgEntries[ n++ ];

		*newentry = *oldentry;

		newentry->nOffset = FS_Tell( cls.demofile );
		FS_Seek( fp, oldentry->nOffset, FILESYSTEM_SEEK_HEAD );

		CL_DemoCopyFileChunk( cls.demofile, fp, newentry->nFileLength );
	}

	demoheader.nDirectoryOffset = FS_Tell( cls.demofile );

	FS_Write( &newdir.nEntries, sizeof( int ), 1, cls.demofile );
	for ( i = 0; i < newdir.nEntries; i++ )
		FS_Write( &newdir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( szOriginalName );
	rename( szTempName, szOriginalName );

	Mem_Free( newdir.p_rgEntries );
	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
}

/*
====================
CL_ListDemo_f
====================
*/
void CL_ListDemo_f( void )
{
	int				nCurrent;
	demoheader_t	header;
	demodirectory_t	directory;
	demoentry_t		*entry;
	char			name[MAX_PATH];
	char			type[32];

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Listdemo only available when not running or recording.\n" );
		return;
	}

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Demo contents for %s.\n", name );

	cls.demofile = FS_Open( name, "rb" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		return;
	}

	FS_Read( &header, sizeof( demoheader_t ), 1, cls.demofile );

	if ( Q_strncmp( header.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	Con_Printf( "Demo file protocols %iN:%iD\nServer protocols at %iN:%iD\n", header.nNetProtocolVersion, header.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
	Con_Printf( "Map name :  %s\n", header.szMapName );
	Con_Printf( "Game .dll:  %s\n", header.szDllDir );

	FS_Seek( cls.demofile, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, sizeof( int ), 1, cls.demofile );

	if ( directory.nEntries < 1 || directory.nEntries > 1024 )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	directory.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( demoentry_t ) * directory.nEntries, 1, cls.demofile );

	Con_Printf( "\n" );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[ nCurrent ];

		if ( entry->nEntryType == 0 )
			_snprintf( type, sizeof( type ), "Start segment" );
		else
			_snprintf( type, sizeof( type ), "Normal segment" );

		Con_Printf( "%i:  %s\n  \"%20s\"\nTime:  %.2f s.\nFrames:  %i\nSize:  %.2fK\n",
			nCurrent + 1, type, entry->szDescription, entry->fTrackTime, entry->nFrames, entry->nFileLength / 1024.0 );

		if ( entry->nFlags )
			Con_Printf( "  Flags:\n" );

		if ( entry->nFlags & FDEMO_TITLE )
			Con_Printf( "     Title \"%s\"\n", entry->szDescription );
		if ( entry->nFlags & FDEMO_PLAY )
			Con_Printf( "     Play %i\n", entry->nCDTrack );
		if ( entry->nFlags & FDEMO_FADE_IN_SLOW )
			Con_Printf( "     Fade in slow\n" );
		if ( entry->nFlags & FDEMO_FADE_IN_FAST )
			Con_Printf( "     Fade in fast\n" );
		if ( entry->nFlags & FDEMO_FADE_OUT_SLOW )
			Con_Printf( "     Fade out slow\n" );
		if ( entry->nFlags & FDEMO_FADE_OUT_FAST )
			Con_Printf( "     Fade out fast\n" );

		Con_Printf( "\n" );
	}

	FS_Close( cls.demofile );
	cls.demofile = NULL;

	Mem_Free( directory.p_rgEntries );
}

/*
====================
CL_DemoSectionStart
====================
*/
void CL_DemoSectionStart( void )
{
	char	szCommand[128];

	if ( !pCurrentEntry )
		return;

	if ( pCurrentEntry->nFlags & FDEMO_PLAY )
	{
		_snprintf( szCommand, sizeof( szCommand ), "cd stop\ncd play %i\n", pCurrentEntry->nCDTrack );
		Cbuf_AddText( szCommand );
	}

	if ( pCurrentEntry->nFlags & FDEMO_FADE_IN_FAST )
	{
		sf_demo.duration = 0x0800;	// 0.5 s
		sf_demo.holdTime = 0;
		sf_demo.r = 0;
		sf_demo.g = 0;
		sf_demo.b = 0;
		sf_demo.a = 255;
		sf_demo.fadeFlags = FFADE_IN;
		V_ScreenFade( NULL, 0, &sf_demo );
	}

	if ( pCurrentEntry->nFlags & FDEMO_FADE_IN_SLOW )
	{
		sf_demo.duration = 0x2000;	// 2.0 s
		sf_demo.holdTime = 0;
		sf_demo.r = 0;
		sf_demo.g = 0;
		sf_demo.b = 0;
		sf_demo.a = 255;
		sf_demo.fadeFlags = FFADE_IN;
		V_ScreenFade( NULL, 0, &sf_demo );
	}
}

/*
====================
CL_SwitchToNextDemoSection
====================
*/
qboolean CL_SwitchToNextDemoSection( void )
{
	float	ft;

	if ( nCurEntryIndex >= 1 && pCurrentEntry && ( pCurrentEntry->nFlags & FDEMO_PLAY ) )
		Cbuf_AddText( "cd stop\n" );

	nCurEntryIndex++;
	if ( nCurEntryIndex >= demodir.nEntries )
	{
		ft = CL_DemoInTime();
		Host_EndGame( "End of demo.\n" );
		return false;
	}

	pCurrentEntry = &demodir.p_rgEntries[ nCurEntryIndex ];
	cls.forcetrack = pCurrentEntry->nCDTrack;

	FS_Seek( cls.demofile, pCurrentEntry->nOffset, FILESYSTEM_SEEK_HEAD );

	cls.demostarttime = CL_DemoInTime();
	cls.demostartframe = host_framecount;
	cls.demoframecount = 0;

	return true;
}

/*
====================
CL_DemoWriteSequenceInfo
====================
*/
void CL_DemoWriteSequenceInfo( FileHandle_t fp, int length )
{
	sequence_info_t	si;

	si.incoming_sequence = cls.netchan.incoming_sequence;
	si.incoming_acknowledged = cls.netchan.incoming_acknowledged;
	si.incoming_reliable_acknowledged = cls.netchan.incoming_reliable_acknowledged;
	si.incoming_reliable_sequence = cls.netchan.incoming_reliable_sequence;
	si.outgoing_sequence = cls.netchan.outgoing_sequence;
	si.reliable_sequence = cls.netchan.reliable_sequence;
	si.last_reliable_sequence = cls.netchan.last_reliable_sequence;
	si.length = LittleLong( length );

	FS_Write( &si, sizeof( sequence_info_t ), 1, fp );
}

/*
====================
CL_ReadNetchanState_OLD
====================
*/
void CL_ReadNetchanState_OLD( void )
{
	FS_Read( &cls.netchan.incoming_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_acknowledged, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_reliable_acknowledged, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_reliable_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.outgoing_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.reliable_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.last_reliable_sequence, sizeof( int ), 1, cls.demofile );
}

/*
====================
CL_ReadNetchanState
====================
*/
void CL_ReadNetchanState( void )
{
	if ( !DemoPlayer_IsActive() )
	{
		CL_ReadNetchanState_OLD();
	}
	else
	{
		DemoPlayer_ReadNetchanState( &cls.netchan.incoming_sequence, &cls.netchan.incoming_acknowledged,
			&cls.netchan.incoming_reliable_acknowledged, &cls.netchan.incoming_reliable_sequence,
			&cls.netchan.outgoing_sequence, &cls.netchan.reliable_sequence, &cls.netchan.last_reliable_sequence );
	}
}

/*
====================
CL_DemoWriteCmdInfo
====================
*/
void CL_DemoWriteCmdInfo( FileHandle_t fp )
{
	FS_Write( &g_rp, sizeof( demo_info_t ), 1, fp );
}

/*
====================
CL_DemoFrameInfo
====================
*/
void CL_DemoFrameInfo( demo_info_t *rp, float time )
{
	int		i;

	for ( i = 31; i >= 1; i-- )
		Q_memcpy( &dem_frames[i], &dem_frames[i - 1], sizeof( demo_info_t ) );

	dem_frames[0] = *rp;
	CL_FixPointers( &dem_frames[0] );
	dem_frames[0].timestamp = time;
}

/*
====================
CL_DemoReadCmdInfo
====================
*/
void CL_DemoReadCmdInfo( float time )
{
	demo_info_t	rp;

	FS_Read( &rp, sizeof( demo_info_t ), 1, cls.demofile );

	g_rp.cmd = rp.cmd;
	g_rp.movevars = rp.movevars;
	VectorCopy( rp.view, g_rp.view );
	g_rp.viewmodel = rp.viewmodel;

	CL_DemoFrameInfo( &rp, time );
}

/*
====================
CL_RecordHUDCommand
====================
*/
void CL_RecordHUDCommand( char *cmdname )
{
	hud_command_t	demcmd;

	demcmd.cmd = 3;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );

	Q_memset( demcmd.szNameBuf, 0, sizeof( demcmd.szNameBuf ) );
	Q_strncpy( demcmd.szNameBuf, cmdname, sizeof( demcmd.szNameBuf ) - 1 );
	demcmd.szNameBuf[ sizeof( demcmd.szNameBuf ) - 1 ] = 0;

	FS_Write( &demcmd, sizeof( hud_command_t ), 1, cls.demofile );
}

/*
====================
CL_WriteDLLUpdate
====================
*/
void CL_WriteDLLUpdate( client_data_t *cdata )
{
	cl_demo_data_t	demcmd;

	if ( !cls.demofile )
		return;

	demcmd.cmd = 4;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );

	VectorCopy( cdata->origin, demcmd.origin );
	VectorCopy( cdata->viewangles, demcmd.viewangles );
	demcmd.iWeaponBits = cdata->iWeaponBits;
	demcmd.fov = cdata->fov;

	FS_Write( &demcmd, sizeof( cl_demo_data_t ), 1, cls.demofile );
}

/*
====================
CL_DemoAnim
====================
*/
void CL_DemoAnim( int anim, int body )
{
	demo_anim_t	demcmd;

	if ( !cls.demofile )
		return;

	demcmd.cmd = 7;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );
	demcmd.anim = LittleLong( anim );
	demcmd.body = LittleLong( body );

	FS_Write( &demcmd, sizeof( demo_anim_t ), 1, cls.demofile );
}

/*
====================
CL_DemoQueueSound
====================
*/
void CL_DemoQueueSound( int channel, const char *sample, float volume, float attenuation, int flags, int pitch )
{
	demo_sound1_s	demcmd1;
	demo_sound2_s	demcmd2;

	if ( !cls.demofile )
		return;

	demcmd1.cmd = 8;
	demcmd1.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd1.frame = LittleLong( host_framecount - cls.demostartframe );
	demcmd1.channel = LittleLong( channel );
	demcmd1.length = LittleLong( Q_strlen( sample ) );

	FS_Write( &demcmd1, sizeof( demo_sound1_s ), 1, cls.demofile );
	FS_Write( sample, Q_strlen( sample ), 1, cls.demofile );

	demcmd2.volume = LittleFloat( volume );
	demcmd2.attenuation = LittleFloat( attenuation );
	demcmd2.flags = LittleLong( flags );
	demcmd2.pitch = LittleLong( pitch );

	FS_Write( &demcmd2, sizeof( demo_sound2_s ), 1, cls.demofile );
}

/*
====================
CL_DemoPlaySound
====================
*/
void CL_DemoPlaySound( int channel, char *sample, float attenuation, float volume, int flags, int pitch )
{
	sfx_t	*sfx;
	sfx_t	sfxsentence;

	if ( flags & SND_SENTENCE )
	{
		sfx = &sfxsentence;
		_snprintf( sfx->name, sizeof( sfx->name ), "!%s", rgpszrawsentence[ Q_atoi( &sample[1] ) ] );
	}
	else
	{
		sfx = CL_LookupSound( sample );
	}

	if ( !sfx )
		return;

	if ( channel == CHAN_STATIC )
		S_StartStaticSound( cl.playernum + 1, CHAN_STATIC, sfx, cl.simorg, volume, attenuation, flags, pitch );
	else
		S_StartDynamicSound( cl.playernum + 1, channel, sfx, cl.simorg, volume, attenuation, flags, pitch );
}

/*
====================
CL_DemoParseSound
====================
*/
void CL_DemoParseSound( void )
{
	int		channel;
	char	sample[256];
	float	volume;
	float	attenuation;
	int		flags;
	int		pitch;
	float	f;
	int		i;

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	channel = LittleLong( i );

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	FS_Read( sample, i, 1, cls.demofile );
	sample[i] = 0;

	FS_Read( &f, sizeof( float ), 1, cls.demofile );
	attenuation = LittleFloat( f );

	FS_Read( &f, sizeof( float ), 1, cls.demofile );
	volume = LittleFloat( f );

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	flags = LittleLong( i );

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	pitch = LittleLong( i );

	CL_DemoPlaySound( channel, sample, attenuation, volume, flags, pitch );
}

/*
====================
CL_DemoEvent
====================
*/
void CL_DemoEvent( int flags, int idx, float delay, event_args_t *pargs )
{
	demo_event_t	demcmd;

	if ( !cls.demofile )
		return;

	demcmd.cmd = 6;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );
	demcmd.flags = LittleLong( flags );
	demcmd.idx = LittleLong( idx );
	demcmd.delay = LittleFloat( delay );

	FS_Write( &demcmd, sizeof( demo_event_t ), 1, cls.demofile );
	FS_Write( pargs, sizeof( event_args_t ), 1, cls.demofile );
}

/*
====================
CL_DemoParseEvent
====================
*/
void CL_DemoParseEvent( void )
{
	int				flags;
	int				idx;
	float			delay;
	event_args_t	eargs;
	float			f;
	int				i;

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	flags = i;

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	idx = i;

	FS_Read( &f, sizeof( float ), 1, cls.demofile );
	f = LittleFloat( f );
	delay = f;

	FS_Read( &eargs, sizeof( event_args_t ), 1, cls.demofile );

	CL_QueueEvent( flags, idx, delay, &eargs );
}

/*
====================
CL_DemoParseAnim
====================
*/
void CL_DemoParseAnim( void )
{
	int		anim, body;
	int		i;

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	anim = i;

	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	body = i;

	hudWeaponAnim( anim, body );
}

/*
====================
CL_BeginDemoStartup
====================
*/
void CL_BeginDemoStartup( void )
{
	if ( cls.demoheader )
		FS_Close( cls.demoheader );

	cls.demoheader = FS_Open( "demoheader.dmf", "w+b" );
	if ( !cls.demoheader )
	{
		Con_DPrintf( "ERROR: couldn't open temporary header file.\n" );
		return;
	}

	Con_DPrintf( "Spooling demo header.\n" );
}

/*
====================
CL_WriteDemoStartup
====================
*/
void CL_WriteDemoStartup( int start, sizebuf_t *msg )
{
	int				len;
	demo_command_t	demcmd;

	if ( !cls.demoheader )
		return;

	len = msg->cursize - start;
	if ( len <= 0 )
		return;

	demcmd.cmd = 0;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );

	FS_Write( &demcmd, sizeof( demo_command_t ), 1, cls.demoheader );

	CL_DemoWriteCmdInfo( cls.demoheader );
	CL_DemoWriteSequenceInfo( cls.demoheader, len );

	FS_Write( msg->data + start, len, 1, cls.demoheader );
}

/*
====================
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
====================
*/
void CL_StopPlayback( void )
{
	if ( !cls.demoplayback )
		return;

	if ( DemoPlayer_IsActive() )
		DemoPlayer_Stop();
	else
		FS_Close( cls.demofile );

	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;
	cls.demoframecount = 0;

	if ( demodir.p_rgEntries )
		Mem_Free( demodir.p_rgEntries );

	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;

	if ( cl_gamegauge.value )
		CL_FinishGameGauge();

	if ( cls.timedemo )
		CL_FinishTimeDemo();

	Cbuf_AddText( "cd stop\n" );
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage( int start, sizebuf_t *msg )
{
	int				len;
	demo_command_t	demcmd;

	if ( !cls.demorecording || !cls.demofile )
		return;

	len = msg->cursize - start;
	if ( len < 0 )
		return;

	cls.demoframecount++;

	demcmd.cmd = 1;
	demcmd.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	demcmd.frame = LittleLong( host_framecount - cls.demostartframe );

	FS_Write( &demcmd, sizeof( demo_command_t ), 1, cls.demofile );

	CL_DemoWriteCmdInfo( cls.demofile );
	CL_DemoWriteSequenceInfo( cls.demofile, len );

	FS_Write( msg->data + start, len, 1, cls.demofile );
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f( void )
{
	byte	c;
	float	f;
	float	ft;
	int		frame;
	int		swlen;
	int		curpos;
	int		i;
	char	szNewName[MAX_PATH];
	char	szOldName[MAX_PATH];

	if ( cmd_source != src_command )
		return;

	if ( !cls.demorecording )
	{
		Con_Printf( "Not recording a demo.\n" );
		return;
	}

	// Demo playback should read this as an incoming message.
	c = 5;
	FS_Write( &c, sizeof( byte ), 1, cls.demofile );

	ft = CL_DemoOutTime();
	f = LittleFloat( ft - cls.demostarttime );
	FS_Write( &f, sizeof( float ), 1, cls.demofile );

	frame = host_framecount - cls.demostartframe;
	swlen = LittleLong( frame );
	FS_Write( &swlen, sizeof( int ), 1, cls.demofile );

	curpos = FS_Tell( cls.demofile );

	pCurrentEntry->nFileLength = curpos - pCurrentEntry->nOffset;
	pCurrentEntry->fTrackTime = ft - cls.demostarttime;
	pCurrentEntry->nFrames = cls.demoframecount;

	// Now write out the directory and free it and touch up the demo header.
	FS_Write( &demodir.nEntries, sizeof( int ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	if ( demodir.p_rgEntries )
		Mem_Free( demodir.p_rgEntries );

	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;

	demoheader.nDirectoryOffset = curpos;
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	cls.demorecording = false;
	cls.td_lastframe = host_framecount;

	if ( cls.demoappending )
	{
		Con_Printf( "Replacing old demo with appended version.\n" );

		Q_strncpy( szNewName, cls.demofilename, sizeof( szNewName ) - 1 );
		szNewName[ sizeof( szNewName ) - 1 ] = 0;

		_unlink( cls.demofilename );

		COM_StripExtension( cls.demofilename, szOldName );
		COM_DefaultExtension( szOldName, ".dm2" );

		rename( szOldName, szNewName );
	}

	Con_Printf( "Completed demo\n" );
	Con_DPrintf( "Recording time %.2f\nHost frames %i\n", ft - cls.demostarttime, cls.td_lastframe - cls.td_startframe );
}

/*
====================
CL_Record_f

record <demoname> <cd>
====================
*/
void CL_Record_f( void )
{
	int		c;
	char	name[MAX_PATH];
	char	szMapName[MAX_PATH];
	char	szDll[MAX_PATH];
	int		track;
	int32	copysize;
	int32	savepos;
	int32	curpos;
	byte	cmd;
	float	f;
	int		frame;
	int		swlen;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "record <demoname> <cd>\n" );
		return;
	}

	if ( cls.demorecording )
	{
		Con_Printf( "Already recording.\n" );
		return;
	}

	if ( cls.demoplayback )
	{
		Con_Printf( "Can't record during demo playback.\n" );
		return;
	}

	if ( !cls.demoheader || cls.state != ca_active )
	{
		Con_Printf( "You must be in an active map spawned on a machine that allows creation of files in %s\n", com_gamedir );
		return;
	}

	c = Cmd_Argc();
	if ( c != 2 && c != 3 )
	{
		Con_Printf( "record <demoname> <cd track>\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	// get the cd track
	if ( c == 3 )
	{
		track = Q_atoi( Cmd_Argv( 2 ) );
		Con_Printf( "Forcing CD track to %i\n", track );
	}
	else
	{
		track = -1;
	}

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_StripExtension( name, szDll );
	COM_DefaultExtension( szDll, ".dem" );
	strncpy( name, szDll, sizeof( name ) );

	// open the demo file
	Con_Printf( "recording to %s.\n", name );

	cls.demofile = FS_Open( name, "wb" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		return;
	}

	// Write the demo header
	Q_memset( &demoheader, 0, sizeof( demoheader_t ) );
	Q_strncpy( demoheader.szFileStamp, "HLDEMO", 6 );

	COM_FileBase( cl.worldmodel->name, szMapName );
	Q_strncpy( demoheader.szMapName, szMapName, sizeof( demoheader.szMapName ) - 1 );
	demoheader.szMapName[ sizeof( demoheader.szMapName ) - 1 ] = 0;

	Q_strncpy( szDll, com_gamedir, sizeof( szDll ) - 1 );
	szDll[ sizeof( szDll ) - 1 ] = 0;
	COM_FileBase( szDll, demoheader.szDllDir );

	demoheader.mapCRC = cl.serverCRC;
	demoheader.nDemoProtocol = DEMO_PROTOCOL;
	demoheader.nNetProtocolVersion = PROTOCOL_VERSION;
	demoheader.nDirectoryOffset = 0;

	FS_Write( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	// Now set up the directory
	Q_memset( &demodir, 0, sizeof( demodirectory_t ) );
	demodir.nEntries = 2;
	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	Q_memset( demodir.p_rgEntries, 0, sizeof( demoentry_t ) * demodir.nEntries );

	// Startup segment
	pCurrentEntry = &demodir.p_rgEntries[0];
	Q_strcpy( pCurrentEntry->szDescription, "LOADING" );
	pCurrentEntry->nEntryType = 0;
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0;
	pCurrentEntry->nOffset = FS_Tell( cls.demofile );

	cls.demowaiting = true;
	cls.demorecording = true;

	curpos = FS_Tell( cls.demofile );

	cmd = 5;
	FS_Write( &cmd, sizeof( byte ), 1, cls.demoheader );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( float ), 1, cls.demoheader );
	frame = host_framecount - cls.demostartframe;
	swlen = LittleLong( frame );
	FS_Write( &swlen, sizeof( int ), 1, cls.demoheader );

	FS_Flush( cls.demoheader );
	copysize = FS_Tell( cls.demoheader );
	savepos = copysize;
	FS_Seek( cls.demoheader, 0, FILESYSTEM_SEEK_HEAD );

	CL_DemoCopyFileChunk( cls.demofile, cls.demoheader, copysize );

	FS_Seek( cls.demoheader, savepos, FILESYSTEM_SEEK_HEAD );

	cls.demostarttime = CL_DemoOutTime();
	cls.demoframecount = 0;
	cls.demostartframe = host_framecount;

	curpos = FS_Tell( cls.demofile );
	pCurrentEntry->nFileLength = curpos - pCurrentEntry->nOffset;

	// Now open the playback segment
	pCurrentEntry = &demodir.p_rgEntries[1];
	pCurrentEntry->nEntryType = 1;
	_snprintf( pCurrentEntry->szDescription, sizeof( pCurrentEntry->szDescription ), "Playback" );
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0;
	pCurrentEntry->nOffset = FS_Tell( cls.demofile );

	// Demo playback should read this as an incoming message.
	cmd = 2;
	FS_Write( &cmd, sizeof( byte ), 1, cls.demofile );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( float ), 1, cls.demofile );
	frame = host_framecount - cls.demostartframe;
	swlen = LittleLong( frame );
	FS_Write( &swlen, sizeof( int ), 1, cls.demofile );

	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
	cls.demoappending = false;
}

/*
====================
CL_PlayDemo_f

play [demoname]
====================
*/
void CL_PlayDemo_f( void )
{
	char	name[256];

	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 3 )
	{
		Con_Printf( "playdemo <demoname> <replayspeed>: plays a demo\n" );
		Con_Printf( "replayspeed == 1.0 default\n" );
		return;
	}

	g_demotimescale = 1.0;
	if ( Cmd_Argc() == 3 )
	{
		g_demotimescale = Q_atof( Cmd_Argv( 2 ) );
		if ( g_demotimescale <= 0.001 )
			g_demotimescale = 1.0;
		Con_Printf( "Playdemo:  Time scale %f\n", g_demotimescale );
	}

	// disconnect from server
	CL_Disconnect();
	DemoPlayer_SetActive( false );

	// open the demo file
	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 5 );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Playing demo from %s.\n", name );

	cls.demofile = FS_Open( name, "rb" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		cls.demonum = -1;		// stop demo loop
		return;
	}

	FS_Read( &demoheader, sizeof( demoheader_t ), 1, cls.demofile );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", 6 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		cls.demonum = -1;		// stop demo loop
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "WARNING! demo protocol outdated.\nDemo file protocols %i:%i, Engine protocol is at %i:%i\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
	}

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( int ), 1, cls.demofile );

	if ( demodir.nEntries < 1 || demodir.nEntries > 1024 )
	{
		COM_ExplainDisconnection( true, "Error: Corrupt demo file." );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		cls.demonum = -1;		// stop demo loop
		CL_Disconnect();
		return;
	}

	demodir.p_rgEntries = (demoentry_t *)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, cls.demofile );

	nCurEntryIndex = 0;
	pCurrentEntry = &demodir.p_rgEntries[ nCurEntryIndex ];

	FS_Seek( cls.demofile, pCurrentEntry->nOffset, FILESYSTEM_SEEK_HEAD );

	cls.forcetrack = pCurrentEntry->nCDTrack;
	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.passive = false;
	cls.spectator = false;

	cls.demostarttime = CL_DemoInTime();
	cls.demostartframe = host_framecount;

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );
	NET_ClearLagData( true, false );

	cls.demoframecount = 0;
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = realtime;
}

/*
====================
CL_ViewDemo_f
====================
*/
void CL_ViewDemo_f( void )
{
	char	name[MAX_PATH];

	if ( cmd_source != src_command )
		return;

	DemoPlayer_SetActive( true );

	if ( !DemoPlayer_IsActive() )
	{
		Con_Printf( "viewdemo not available.\n" );
		return;
	}

	DemoPlayer_ShowGUI();

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "viewdemo <demoname>: shows a demo\n" );
		return;
	}

	// disconnect from server
	CL_Disconnect();

	// open the demo file
	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - 5 );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Playing demo from %s.\n", name );

	if ( !DemoPlayer_LoadGame( name ) )
	{
		Con_Printf( "Viewdemo: couldn't load demo %s.\n", name );
		cls.demonum = -1;		// stop demo loop
		return;
	}

	cls.demoplayback = true;
	cls.state = ca_connected;
	cls.passive = false;
	cls.spectator = false;

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );
	NET_ClearLagData( true, false );

	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = realtime;
}

/*
====================
CL_FinishTimeDemo
====================
*/
void CL_FinishTimeDemo( void )
{
	int		frames;
	float	time;

	cls.timedemo = false;

	// the first frame didn't count
	frames = ( host_framecount - cls.td_startframe ) - 1;
	time = realtime - cls.td_starttime;
	if ( !time )
		time = 1;

	Con_Printf( "%i frames %5.3f seconds %5.3f fps\n", frames, time, frames / time );
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "timedemo <demoname>: gets demo speeds\n" );
		return;
	}

	CL_PlayDemo_f();

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	cls.timedemo = true;
	cls.td_starttime = realtime;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

/*
====================
CL_WavePlayLen_f
====================
*/
void CL_WavePlayLen_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "waveplaylen <wave file name>: returns approximate number of milliseconds a wave file will take to play.\n" );
		return;
	}

	{
		unsigned int	length;

		length = COM_GetApproxWavePlayLength( Cmd_Argv( 1 ) );
		if ( !length )
			Con_Printf( "Unable to read %s, file may be missing or incorrectly formatted.\n", Cmd_Argv( 1 ) );
		else
			Con_Printf( "Play time is approximately %dms\n", length );
	}
}

int		g_iMaxFPS;
int		g_iMinFPS;
int		g_iFrames;
float	g_flTime;
int		g_rgFPS[300];
int		g_iSecond;

/*
====================
CL_GameGauge_f
====================
*/
void CL_GameGauge_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "gg <demoname> : Game Gauge 99\n" );
		return;
	}

	CL_PlayDemo_f();

	Cvar_Set( "cl_gg", "1" );

	g_iMaxFPS = 0;
	g_iMinFPS = 0x7FFFFFFF;
	g_iFrames = host_framecount;
	g_iSecond = 0;
	g_flTime = 0;

	cls.timedemo = true;
	cls.td_starttime = realtime;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame
}

/*
====================
CL_FinishGameGauge
====================
*/
void CL_FinishGameGauge( void )
{
	FileHandle_t	f;
	int				frames, i;
	float			time;

	Cvar_Set( "cl_gg", "0" );

	// the first frame didn't count
	frames = ( host_framecount - cls.td_startframe ) - 1;
	time = realtime - cls.td_starttime;
	if ( !time )
		time = 1;

	Con_Printf( "%5.2f Half-Life v%s\n", frames / time, gpszVersionString );
	Con_Printf( "%d Min\n", g_iMinFPS );
	Con_Printf( "%d Max\n", g_iMaxFPS );

	for ( i = 0; i < g_iSecond; i++ )
		Con_Printf( "%d Second %d\n", g_rgFPS[i], i + 1 );

	f = FS_Open( "fps.txt", "w" );
	FS_FPrintf( f, "%5.2f Half-Life v%s\n", frames / time, gpszVersionString );
	FS_FPrintf( f, "%d Min\n", g_iMinFPS );
	FS_FPrintf( f, "%d Max\n", g_iMaxFPS );

	for ( i = 0; i < g_iSecond; i++ )
		FS_FPrintf( f, "%d Second %d\n", g_rgFPS[i], i + 1 );

	FS_Close( f );

	if ( !COM_CheckParm( "-noquit" ) )
		Cbuf_AddText( "quit\n" );
}

/*
====================
CL_GGSpeeds
====================
*/
void CL_GGSpeeds( float flCurTime )
{
	if ( !g_flTime )
	{
		g_flTime = flCurTime;
	}
	else if ( flCurTime - g_flTime >= 1 )
	{
		if ( scr_con_current == 0 )
		{
			int		frames;

			frames = host_framecount - g_iFrames;

			if ( frames > g_iMaxFPS )
				g_iMaxFPS = frames;
			if ( frames < g_iMinFPS )
				g_iMinFPS = frames;

			if ( g_iSecond < 300 )
			{
				g_rgFPS[ g_iSecond ] = frames;
				g_iSecond++;
			}
		}
		else
		{
			cls.td_starttime = realtime;
			cls.td_startframe = host_framecount;
		}

		g_iFrames = host_framecount;
		g_flTime = flCurTime;
	}
}

/*
====================
CL_WriteClientDLLMessage
====================
*/
void CL_WriteClientDLLMessage( int size, unsigned char *buf )
{
	int		swlen;
	float	f;
	byte	cmd;
	int		frame;

	if ( !cls.demorecording || !cls.demofile )
		return;

	if ( size < 0 )
		return;

	cmd = 9;
	FS_Write( &cmd, sizeof( byte ), 1, cls.demofile );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( float ), 1, cls.demofile );
	frame = host_framecount - cls.demostartframe;
	swlen = LittleLong( frame );
	FS_Write( &swlen, sizeof( int ), 1, cls.demofile );

	swlen = LittleLong( size );
	FS_Write( &swlen, sizeof( int ), 1, cls.demofile );
	FS_Write( buf, size, 1, cls.demofile );
}

/*
====================
CL_ReadClientDLLData
====================
*/
void CL_ReadClientDLLData( void )
{
	unsigned char	data[32768];
	int				i;

	Q_memset( data, 0, sizeof( data ) );
	FS_Read( &i, sizeof( int ), 1, cls.demofile );
	i = LittleLong( i );
	FS_Read( data, i, 1, cls.demofile );

	ClientDLL_ReadDemoBuffer( i, data );
}

/*
====================
CL_DemoAPIRecording
====================
*/
int CL_DemoAPIRecording( void )
{
	return cls.demorecording != 0;
}

/*
====================
CL_DemoAPIPlayback
====================
*/
int CL_DemoAPIPlayback( void )
{
	return cls.demoplayback != 0;
}

/*
====================
CL_DemoAPITimedemo
====================
*/
int CL_DemoAPITimedemo( void )
{
	return cls.timedemo != 0;
}

demo_api_t	demoapi =
{
	CL_DemoAPIRecording,
	CL_DemoAPIPlayback,
	CL_DemoAPITimedemo,
	CL_WriteClientDLLMessage,
};

/*
====================
CL_ReadDemoMessage_OLD

Handles playback of demos
====================
*/
qboolean CL_ReadDemoMessage_OLD( void )
{
	int			msglen;
	byte		msgbuf[ MAX_POSSIBLE_MSG ];
	int			r;
	float		f;
	float		ft;
	byte		cmd;
	int32		curpos;
	float		fElapsedTime;
	int			frame;
	qboolean	bSkipMessage;
	int			nElapsedFrames;
	static int	tdlastdemoframe;

	bSkipMessage = false;
	msglen = 0;

	if ( !cls.demofile )
	{
		cls.demoplayback = false;
		CL_Disconnect_f();
		Con_Printf( "Tried to read a demo message with no demo file\n" );
		return false;
	}

	for ( ;; )
	{
		curpos = FS_Tell( cls.demofile );
		net_message.cursize = 0;

		// Read the command
		FS_Read( &cmd, sizeof( byte ), 1, cls.demofile );

		// Read the timestamp
		FS_Read( &f, sizeof( float ), 1, cls.demofile );
		f = LittleFloat( f );

		// Read the frame
		FS_Read( &frame, sizeof( int ), 1, cls.demofile );
		frame = LittleLong( frame );

		ft = CL_DemoInTime();
		fElapsedTime = ft - cls.demostarttime;
		nElapsedFrames = host_framecount - cls.demostartframe;

		if ( !cls.timedemo )
			bSkipMessage = f >= fElapsedTime;
		else
			bSkipMessage = false;

		// Not ready for a new message yet, put it back on the file.
		if ( cmd != 0 && cmd != 5 && bSkipMessage )
		{
			// Never skip first message in a segment
			FS_Seek( cls.demofile, curpos, FILESYSTEM_SEEK_HEAD );
			return false;
		}

		if ( cmd == 2 )
		{
			cls.demostarttime = CL_DemoInTime();
			cls.demostartframe = host_framecount;
			continue;
		}

		if ( cmd == 5 )
		{
			if ( CL_SwitchToNextDemoSection() )
				continue;
			break;
		}

		if ( cmd == 3 )
		{
			char	szCmdName[64];

			FS_Read( szCmdName, sizeof( szCmdName ), 1, cls.demofile );
			Cbuf_AddText( szCmdName );
			Cbuf_AddText( "\n" );
			continue;
		}

		if ( cmd == 4 )
		{
			client_data_t	cdat;

			FS_Read( &cdat, sizeof( client_data_t ), 1, cls.demofile );
			ClientDLL_DemoUpdateClientData( &cdat );
			scr_fov_value = cdat.fov;
			continue;
		}

		if ( cmd == 6 )
		{
			CL_DemoParseEvent();
			continue;
		}

		if ( cmd == 7 )
		{
			CL_DemoParseAnim();
			continue;
		}

		if ( cmd == 8 )
		{
			CL_DemoParseSound();
			continue;
		}

		if ( cmd == 9 )
		{
			CL_ReadClientDLLData();
			continue;
		}

		break;
	}

	// If we are playing back a timedemo, and we've already passed on a
	// frame update for this host_frame tag, then we'll just skip this message.
	if ( cls.timedemo && tdlastdemoframe == host_framecount )
	{
		FS_Seek( cls.demofile, FS_Tell( cls.demofile ) - sizeof( demo_command_t ), FILESYSTEM_SEEK_HEAD );
		return false;
	}

	tdlastdemoframe = host_framecount;

	// If this is the second frame of a timedemo, start the timer
	if ( host_framecount == cls.td_startframe + 1 )
		cls.td_starttime = realtime;

	if ( nCurEntryIndex && !cls.demoframecount )
		CL_DemoSectionStart();

	if ( nCurEntryIndex && cls.demoframecount == 1 )
	{
		bInFadeout = false;

		if ( pCurrentEntry->nFlags & FDEMO_TITLE )
		{
			SetDemoMessage( pCurrentEntry->szDescription, 0.01, 1.0, 2.0 );
			CL_HudMessage( "__DEMOMESSAGE__" );
		}

		if ( !cls.timedemo )
			cls.demostarttime = CL_DemoInTime();
	}

	cls.demoframecount++;

	if ( nCurEntryIndex && !bInFadeout )
	{
		if ( ( pCurrentEntry->nFlags & FDEMO_FADE_OUT_FAST ) && !bInFadeout &&
			( fElapsedTime + 0.5 >= pCurrentEntry->fTrackTime + 0.02 ) )
		{
			bInFadeout = true;

			sf_demo.duration = 0x0800;	// 0.5 s
			sf_demo.holdTime = 0x0199;	// 0.1 s
			sf_demo.r = 0;
			sf_demo.g = 0;
			sf_demo.b = 0;
			sf_demo.a = 255;
			sf_demo.fadeFlags = FFADE_OUT;
			V_ScreenFade( NULL, 0, &sf_demo );
		}

		if ( ( pCurrentEntry->nFlags & FDEMO_FADE_OUT_SLOW ) && !bInFadeout &&
			( fElapsedTime + 2.0 >= pCurrentEntry->fTrackTime + 0.02 ) )
		{
			bInFadeout = true;

			sf_demo.duration = 0x2000;	// 2.0 s
			sf_demo.holdTime = 0x0199;	// 0.1 s
			sf_demo.r = 0;
			sf_demo.g = 0;
			sf_demo.b = 0;
			sf_demo.a = 255;
			sf_demo.fadeFlags = FFADE_OUT;
			V_ScreenFade( NULL, 0, &sf_demo );
		}
	}

	CL_DemoReadCmdInfo( fElapsedTime );
	CL_ReadNetchanState();

	r = FS_Read( &msglen, sizeof( int ), 1, cls.demofile );
	if ( r != sizeof( int ) )
	{
		Host_EndGame( "Bad demo length.\n" );
		return false;
	}

	msglen = LittleLong( msglen );
	if ( msglen < 0 )
	{
		Host_EndGame( "Demo message length < 0.\n" );
		return false;
	}

	if ( msglen > MAX_POSSIBLE_MSG )
	{
		Host_EndGame( "Demo message > MAX_POSSIBLE_MSG" );
		return false;
	}

	if ( msglen > 0 )
	{
		r = FS_Read( msgbuf, msglen, 1, cls.demofile );
		if ( r != msglen )
		{
			Host_EndGame( "Error reading demo message data.\n" );
			return false;
		}
	}

	Q_memcpy( net_message.data, msgbuf, msglen );
	net_message.cursize = msglen;

	return true;
}

/*
====================
CL_ReadDemoMessage
====================
*/
qboolean CL_ReadDemoMessage( void )
{
	if ( !DemoPlayer_IsActive() )
		return CL_ReadDemoMessage_OLD();

	net_message.cursize = DemoPlayer_ReadDemoMessage( net_message.data, net_message.maxsize );
	if ( !net_message.cursize )
		return false;

	DemoPlayer_ReadNetchanState( &cls.netchan.incoming_sequence, &cls.netchan.incoming_acknowledged,
		&cls.netchan.incoming_reliable_acknowledged, &cls.netchan.incoming_reliable_sequence,
		&cls.netchan.outgoing_sequence, &cls.netchan.reliable_sequence, &cls.netchan.last_reliable_sequence );

	return true;
}
