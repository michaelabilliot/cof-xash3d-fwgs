// cl_ents.c -- entity parsing and management

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "mem.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "GL_MODEL.H"
#include "pmove.h"
#include "protocol.h"
#include "cdll_int.h"
#include "con_nprint.h"
#include "vid.h"
#include "render.h"
#include "entity_types.h"
#include "studio.h"
#include "chase.h"
#include "r_efx.h"

// model flags (model.h)
#ifndef EF_ROCKET
#define	EF_ROCKET	1			// leave a trail
#define	EF_GRENADE	2			// leave a trail
#define	EF_GIB		4			// leave a trail
#define	EF_ROTATE	8			// rotate (bonus items)
#define	EF_TRACER	16			// green split trail
#define	EF_ZOMGIB	32			// small blood trail
#define	EF_TRACER2	64			// orange split trail + rotate
#define	EF_TRACER3	128			// purple trail
#endif

// r_main.c
extern	cvar_t	r_bmodelinterp;
extern	vec3_t	r_playerViewportAngles;

// pr_cmds.c
void	PVSMark( model_t *pmodel, byte *ppvs );
float	RandomFloat( float flLow, float flHigh );

// r_part.c
extern	int		cl_numbeamentities;
void	R_KillDeadBeams( int deadEntity );

// gl_rsurf.c
extern	mleaf_t	*r_oldviewleaf;
void	R_MarkLeaves( void );

// r_studio.c
void	R_UpdateLatchedVars( cl_entity_t *ent );
void	R_ResetLatched( cl_entity_t *ent, qboolean full_reset );

double	g_flLatency;

struct predicted_player
{
	int			movetype;
	int			solid;
	int			usehull;
	qboolean	active;
	vec3_t		origin;
	vec3_t		angles;
};

static struct predicted_player	predicted_players[MAX_CLIENTS];

/*
==================
CL_EntityTeleported

==================
*/
qboolean CL_EntityTeleported( cl_entity_t *ent )
{
	int		i;
	vec3_t	distance;

	VectorSubtract( ent->curstate.origin, ent->prevstate.origin, distance );

	// Check if any component is greater than the teleport threshold
	for ( i = 0; i < 3; i++ )
	{
		if ( distance[i] > 128.0 )
			return true;
	}

	return false;
}

/*
==================
CL_CompareTimestamps

==================
*/
qboolean CL_CompareTimestamps( float t1, float t2 )
{
	int		iTime1, iTime2;

	iTime1 = t1 * 1000.0;
	iTime2 = t2 * 1000.0;

	if ( abs( iTime1 - iTime2 ) <= 1 )
		return true;

	return false;
}

/*
==================
CL_ParametricMove

==================
*/
qboolean CL_ParametricMove( cl_entity_t *ent )
{
	vec3_t	delta;
	float	frac;
	float	dt;

	if ( ent->curstate.starttime == 0 || ent->curstate.impacttime == 0 )
		return false;

	VectorSubtract( ent->curstate.endpos, ent->curstate.startpos, delta );

	dt = ent->curstate.impacttime - ent->curstate.starttime;
	if ( dt != 0 )
	{
		float	t;

		t = cl.time;
		if ( ent->lastmove > cl.time )
			t = ent->lastmove;

		frac = ( t - ent->curstate.starttime ) / dt;
		frac = min( 1.0, frac );
		frac = max( 0.0, frac );

		VectorMA( ent->curstate.startpos, frac, delta, ent->curstate.origin );

		ent->lastmove = t;
	}

	VectorCopy( ent->curstate.origin, ent->origin );

	VectorNormalize( delta );
	if ( Length( delta ) > 0 )
	{
		VectorAngles( delta, ent->curstate.angles );
	}

	return true;
}

/*
==================
CL_ProcessEntityUpdate

==================
*/
void CL_ProcessEntityUpdate( cl_entity_t *ent )
{
	int		i;

	ent->index = ent->curstate.number;
	ent->model = CL_GetModelByIndex( ent->curstate.modelindex );

	COM_NormalizeAngles( ent->curstate.angles );

	if ( ent->model && ent->model->name[0] == '*' )
	{
		ent->curstate.animtime = ent->curstate.msg_time;
	}
	else if ( ent->curstate.movetype != MOVETYPE_STEP && ent->curstate.movetype != MOVETYPE_NONE &&
		ent->curstate.movetype != MOVETYPE_WALK && ent->curstate.movetype != MOVETYPE_FLY &&
		!ent->curstate.starttime && !ent->curstate.impacttime )
	{
		ent->curstate.animtime = ent->curstate.msg_time;
	}

	if ( !CL_CompareTimestamps( ent->curstate.animtime, ent->prevstate.animtime ) || ent->curstate.movetype == MOVETYPE_NONE )
	{
		R_UpdateLatchedVars( ent );
		CL_UpdatePositions( ent );
	}

	if ( ent->player )
	{
		ent->curstate.angles[PITCH] = ent->curstate.angles[PITCH] / -3.0;
	}

	VectorCopy( ent->curstate.origin, ent->origin );

	CL_ParametricMove( ent );

	VectorCopy( ent->curstate.angles, ent->angles );

	for ( i = 0; i < 4; i++ )
	{
		VectorCopy( ent->curstate.origin, ent->attachment[i] );
	}
}

/*
==================
CL_ParseDeltaHeader

==================
*/
int CL_ParseDeltaHeader( qboolean *remove, qboolean *custom, int *numbase, qboolean *newbl, int *newblindex, qboolean full, int *offset )
{
	int			num;
	int			delta;
	qboolean	isdelta = false;
	qboolean	isnext = false;

	*custom = false;
	*newbl = false;

	if ( full )
	{
		*remove = false;

		isnext = MSG_ReadBits( 1 ) ? true : false;
		if ( isnext )
		{
			num = *numbase + 1;
		}
		else
		{
			isdelta = !MSG_ReadBits( 1 );
			if ( isdelta )
			{
				delta = MSG_ReadBits( 6 );
				num = *numbase + delta;
			}
			else
			{
				num = MSG_ReadBits( 11 );
			}
		}
	}
	else
	{
		*remove = MSG_ReadBits( 1 ) ? true : false;

		isdelta = !MSG_ReadBits( 1 );
		if ( isdelta )
		{
			delta = MSG_ReadBits( 6 );
			num = *numbase + delta;
		}
		else
		{
			num = MSG_ReadBits( 11 );
		}
	}

	*numbase = num;

	if ( !*remove )
	{
		*custom = MSG_ReadBits( 1 ) ? true : false;

		if ( cl.instanced_baseline_number )
		{
			*newbl = MSG_ReadBits( 1 ) ? true : false;
			if ( *newbl )
			{
				*newblindex = MSG_ReadBits( 6 );
			}
		}
		else
		{
			*newbl = false;
		}

		if ( full && !*newbl )
		{
			*offset = MSG_ReadBits( 1 );
			if ( *offset )
			{
				*offset = MSG_ReadBits( 6 );
			}
		}
		else
		{
			*offset = 0;
		}
	}

	return num;
}

/*
==================
CL_FlushEntityPacket

==================
*/
void CL_FlushEntityPacket( qboolean needbitencodingframe )
{
	int				num;
	entity_state_t	olde, newe;
	unsigned int	peeklong;
	qboolean		remove = false;
	qboolean		custom = false;
	qboolean		newbl = false;
	int				newblindex = 0;
	int				numbase = 0;
	int				offset;
	con_nprint_t	np;
	delta_t			**ppentity = NULL, **ppcustom = NULL, **ppplayer = NULL;

	ppplayer = DELTA_LookupRegistration( "entity_state_player_t" );
	ppentity = DELTA_LookupRegistration( "entity_state_t" );
	ppcustom = DELTA_LookupRegistration( "custom_entity_state_t" );

	np.time_to_live = 1.0;
	np.index = 0;
	np.color[0] = 1.0;
	np.color[1] = 0.2;
	np.color[2] = 0.0;
	Con_NXPrintf( &np, "WARNING:  CL_FlushEntityPacket" );

	Q_memset( &olde, 0, sizeof( olde ) );

	cl.validsequence = 0;		// can't render a frame
	cl.frames[cl.parsecountmod].invalid = true;

	if ( needbitencodingframe )
	{
		MSG_StartBitReading( &net_message );
	}

	// read it all, but ignore it
	while ( 1 )
	{
		peeklong = MSG_PeekBits( 16 );
		if ( !peeklong )
		{
			num = MSG_ReadBits( 16 );
		}
		else
		{
			num = CL_ParseDeltaHeader( &remove, &custom, &numbase, &newbl, &newblindex, false, &offset );
		}

		if ( msg_badread )
		{
			Host_EndGame( "msg_badread in packetentities\n" );
			break;
		}

		if ( !num )
			break;	// done

		if ( remove )
			continue;

		if ( custom )
		{
			DELTA_ParseDelta( (byte *)&olde, (byte *)&newe, *ppcustom );
		}
		else
		{
			DELTA_ParseDelta( (byte *)&olde, (byte *)&newe, CL_IsPlayerIndex( num ) ? *ppplayer : *ppentity );
		}
	}

	if ( needbitencodingframe )
	{
		MSG_EndBitReading( &net_message );
	}
}

/*
==================
CL_ClearPacket

==================
*/
void CL_ClearPacket( packet_entities_t *pPacket )
{
	if ( pPacket->entities )
	{
		Mem_Free( pPacket->entities );
	}

	pPacket->entities = NULL;
	pPacket->num_entities = 0;
}

/*
==================
CL_InterpolateModel

==================
*/
int CL_InterpolateModel( cl_entity_t *e )
{
	int					i;
	vec3_t				delta;
	float				frac;
	float				t, t1, t2;
	position_history_t	*ph0, *ph1;
	qboolean			extrapolate;
	vec3_t				pos, angles;

	VectorCopy( e->curstate.origin, e->origin );
	VectorCopy( e->curstate.angles, e->angles );

	if ( cls.timedemo )
		return 1;

	if ( !e->model )
		return 1;

	if ( e->model->name[0] == '*' && !r_bmodelinterp.value )
		return 1;

	if ( cl.maxclients == 1 )
		return 1;

	if ( cl.moving && cl.onground == e->index )
		return 1;

	t = cl.time - ex_interp.value;

	extrapolate = CL_FindInterpolationUpdates( e, t, &ph0, &ph1, NULL );

	if ( !ph0 || !ph1 )
		return 0;

	t1 = ph1->animtime;
	t2 = ph0->animtime;

	if ( t - t1 < 0 )
		return 0;

	if ( t1 == 0 || ( VectorCompare( ph1->origin, vec3_origin ) && !VectorCompare( ph0->origin, vec3_origin ) ) )
	{
		VectorCopy( ph0->origin, e->origin );
		VectorCopy( ph0->angles, e->angles );
		return 0;
	}

	if ( t2 == t1 )
	{
		VectorCopy( ph0->origin, e->origin );
		VectorCopy( ph0->angles, e->angles );
		return 1;
	}

	VectorSubtract( ph0->origin, ph1->origin, delta );

	frac = ( t - t1 ) / ( t2 - t1 );
	if ( frac < 0.0 )
		return 0;

	if ( frac > 1.0 )
		frac = 1.0;

	VectorMA( ph1->origin, frac, delta, pos );

	for ( i = 0; i < 3; i++ )
	{
		float	ang1, ang2;
		float	d;

		ang1 = ph0->angles[i];
		ang2 = ph1->angles[i];

		d = ang1 - ang2;
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{
			d += 360;
		}

		angles[i] = ang2 + d * frac;
	}

	COM_NormalizeAngles( angles );

	VectorCopy( pos, e->origin );
	VectorCopy( angles, e->angles );

	return 1;
}

/*
==================
CL_ResetLatchedState

==================
*/
void CL_ResetLatchedState( int pnum, packet_entities_t *pack, cl_entity_t *ent )
{
	int		bytenum;
	int		bitnum;

	bytenum = pnum >> 3;
	bitnum = 1 << ( pnum & 7 );

	if ( !( pack->flags[bytenum] & bitnum ) )
		return;

	VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
	VectorCopy( ent->curstate.angles, ent->latched.prevangles );

	R_ResetLatched( ent, true );
	CL_ResetPositions( ent );

	if ( ent->curstate.starttime != 0 && ent->curstate.impacttime != 0 )
	{
		ent->lastmove = cl.time;
	}
}

/*
==================
CL_ProcessPlayerState

==================
*/
void CL_ProcessPlayerState( int playerindex, entity_state_t *state )
{
	entity_state_t	*ps;

	ps = &cl.frames[cl.parsecountmod].playerstate[playerindex];

	ps->number = state->number;
	ps->messagenum = cl.parsecount;
	ps->msg_time = cl.mtime[0];

	ClientDLL_ProcessPlayerState( ps, state );
}

/*
==================
CL_ProcessPacket

==================
*/
void CL_ProcessPacket( frame_t *frame )
{
	packet_entities_t	*pPacket;
	cl_entity_t			*ent;
	entity_state_t		*state;
	int					pnum;

	pPacket = &frame->packet_entities;

	for ( pnum = 0; pnum < pPacket->num_entities; pnum++ )
	{
		state = &pPacket->entities[pnum];

		state->messagenum = cl.parsecount;
		state->msg_time = cl.mtime[0];

		ent = &cl_entities[state->number];

		ent->player = state->number >= 1 && state->number <= cl.maxclients;

		if ( state->number - 1 == cl.playernum )
		{
			ClientDLL_TxferLocalOverrides( state, &frame->clientdata );
		}

		ent->prevstate = ent->curstate;
		ent->curstate = *state;

		CL_ProcessEntityUpdate( ent );

		CL_ResetLatchedState( pnum, pPacket, ent );

		if ( ent->player )
		{
			int		playerindex;

			playerindex = state->number - 1;

			CL_ProcessPlayerState( playerindex, state );

			if ( playerindex == cl.playernum )
			{
				CL_CheckPredictionError();
			}
		}
	}
}

/*
==================
CL_IsPlayerIndex

==================
*/
qboolean CL_IsPlayerIndex( int index )
{
	return index >= 1 && index <= cl.maxclients;
}

/*
==================
CL_SetPacketFlag

==================
*/
void CL_SetPacketFlag( packet_entities_t *pack, int index, int flag )
{
	if ( flag )
	{
		pack->flags[index >> 3] |= ( 1 << ( index & 7 ) );
	}
	else
	{
		pack->flags[index >> 3] &= ~( 1 << ( index & 7 ) );
	}
}

/*
==================
CL_ResetFrameStats

==================
*/
void CL_ResetFrameStats( frame_t *frame )
{
	frame->clientbytes = 0;
	frame->packetentitybytes = 0;
	frame->tentitybytes = 0;
	frame->playerinfobytes = 0;
	frame->soundbytes = 0;
	frame->usrbytes = 0;
	frame->eventbytes = 0;
	frame->voicebytes = 0;
	frame->msgbytes = 0;
}

/*
==================
CL_ParseClientdata

Server information pertaining to this client only, sent every frame
==================
*/
void CL_ParseClientdata( void )
{
	int				i, j;
	int				command_ack;
	frame_t			*frame;
	static int		last_incoming_sequence;
	static int		last_command_ack;
	int				bits;
	clientdata_t	baseline;
	clientdata_t	*from, *to;
	int				fromframe;
	delta_t			**ppdelta = NULL;
	delta_t			**ppweapon = NULL;

	ppdelta = DELTA_LookupRegistration( "clientdata_t" );
	ppweapon = DELTA_LookupRegistration( "weapon_data_t" );

	Q_memset( &baseline, 0, sizeof( baseline ) );

	// This is the last movement that the server ack'd
	command_ack = cls.netchan.incoming_acknowledged;

	// this is the frame update that this message corresponds to
	i = cls.netchan.incoming_sequence;

	// Did we drop some frames?
	if ( i > last_incoming_sequence + 1 )
	{
		// Mark as dropped
		for ( j = last_incoming_sequence + 1; j < i; j++ )
		{
			if ( cl.frames[j & CL_UPDATE_MASK].receivedtime >= 0.0 )
			{
				cl.frames[j & CL_UPDATE_MASK].receivedtime = -1;
				cl.frames[j & CL_UPDATE_MASK].latency = 0;
			}
		}
	}

	cl.parsecount = i;
	i &= CL_UPDATE_MASK;
	cl.parsecountmod = i;

	frame = &cl.frames[i];

	frame->time = cl.mtime[0];

	parsecounttime = cl.commands[command_ack & CL_UPDATE_MASK].senttime;

	cl.commands[command_ack & CL_UPDATE_MASK].receivedtime = realtime;

	frame->receivedtime = realtime;

	CL_ResetFrameStats( frame );

	if ( last_command_ack != -1 )
	{
		int				last_predicted;
		entity_state_t	*ps, *pps;
		clientdata_t	*pcd, *ppcd;
		weapon_data_t	*wd, *pwd;

		if ( cls.spectator )
		{
			ps = pps = &spectatorState.playerstate;
			pcd = ppcd = &spectatorState.client;
			wd = pwd = spectatorState.weapondata;
		}
		else
		{
			last_predicted = last_incoming_sequence + ( command_ack - last_command_ack );
			last_predicted &= CL_UPDATE_MASK;

			ps = &frame->playerstate[cl.playernum];
			pps = &cl.predicted_frames[last_predicted].playerstate;

			pcd = &frame->clientdata;
			ppcd = &cl.predicted_frames[last_predicted].client;

			wd = frame->weapondata;
			pwd = cl.predicted_frames[last_predicted].weapondata;
		}

		ClientDLL_TxferPredictionData( ps, pps, pcd, ppcd, wd, pwd );
	}

	last_command_ack = command_ack;
	last_incoming_sequence = cls.netchan.incoming_sequence;

	frame->latency = realtime - parsecounttime;
	g_flLatency = frame->latency;

	if ( cls.spectator )
	{
		cl.stats[STAT_HEALTH] = 1;
		return;
	}

	to = &frame->clientdata;

	MSG_StartBitReading( &net_message );

	bits = MSG_ReadBits( 1 );
	if ( !bits )
	{
		from = &baseline;
	}
	else
	{
		fromframe = MSG_ReadBits( 8 );
		from = &cl.frames[fromframe & CL_UPDATE_MASK].clientdata;
	}

	DELTA_ParseDelta( (byte *)from, (byte *)to, *ppdelta );

	{
		int				moreweapons = 1;
		int				idx;
		weapon_data_t	*fdata, *tdata;
		weapon_data_t	wbaseline;

		Q_memset( &wbaseline, 0, sizeof( wbaseline ) );

		while ( moreweapons )
		{
			moreweapons = MSG_ReadBits( 1 );
			if ( !moreweapons )
				break;

			idx = MSG_ReadBits( 6 );
			if ( !bits )
			{
				fdata = &wbaseline;
			}
			else
			{
				fdata = &cl.frames[fromframe & CL_UPDATE_MASK].weapondata[idx];
			}

			tdata = &frame->weapondata[idx];

			DELTA_ParseDelta( (byte *)fdata, (byte *)tdata, *ppweapon );
		}
	}

	MSG_EndBitReading( &net_message );

	// Copy in the physics info string
	Q_strncpy( cls.physinfo, to->physinfo, MAX_PHYSINFO_STRING - 1 );
	cls.physinfo[MAX_PHYSINFO_STRING - 1] = 0;

	cl.stats[STAT_HEALTH] = to->health;
	cl.weapons = to->weapons;
	cl.maxspeed = to->maxspeed;
	cl.pushmsec = to->pushmsec;
}

/*
==================
CL_ParsePacketEntities

An svc_packetentities has just been parsed, deal with the
rest of the data stream.
==================
*/
void CL_ParsePacketEntities( qboolean delta, int *playerbits )
{
	int					oldpacket, newpacket;
	packet_entities_t	*oldp, *newp, dummy;
	int					oldindex, newindex;
	int					num, newnum, oldnum;
	qboolean			full;
	byte				from;
	unsigned int		peeklong;
	int					newp_number;
	int					entsinpacket;
	qboolean			berror = false;
	int					backup = 0;
	int					parsed_bits;
	qboolean			remove = false;
	qboolean			custom = false;
	qboolean			newbl = false;
	int					newblindex = 0;
	int					numbase = 0;
	int					offset;
	delta_t				**ppentity = NULL, **ppcustom = NULL, **ppplayer = NULL;

	ppentity = DELTA_LookupRegistration( "entity_state_t" );
	ppplayer = DELTA_LookupRegistration( "entity_state_player_t" );
	ppcustom = DELTA_LookupRegistration( "custom_entity_state_t" );

	// first update is the final signon stage
	if ( cls.signon == 1 )
	{
		cls.signon = SIGNONS;
		CL_SignonReply();
	}

	newpacket = cl.parsecountmod;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	newp_number = MSG_ReadShort();
	entsinpacket = max( 1, newp_number );

	if ( newp->entities )
	{
		Mem_Free( newp->entities );
	}
	newp->entities = (entity_state_t *)Mem_ZeroMalloc( entsinpacket * sizeof( entity_state_t ) );
	newp->num_entities = newp_number;

	Q_memset( newp->flags, 0, sizeof( newp->flags ) );

	if ( delta )
	{
		from = MSG_ReadByte();
		oldpacket = from;
	}
	else
	{
		oldpacket = -1;
		if ( cls.demowaiting )
		{
			cls.demowaiting = false;
			MSG_WriteChar( &cls.netchan.message, clc_stringcmd );
			MSG_WriteString( &cls.netchan.message, "fullupdate" );
		}
	}

	full = false;

	if ( !cl.max_edicts )
	{
		CL_FlushEntityPacket( true );
		CL_ClearPacket( newp );
		return;
	}

	if ( oldpacket != -1 )
	{
		if ( ( ( ( cls.netchan.incoming_sequence & 0xFF ) - oldpacket ) & 0xFF ) >= CL_UPDATE_MASK )
		{	// we can't use this, it is too old
			CL_FlushEntityPacket( true );
			CL_ClearPacket( newp );
			return;
		}

		oldp = &cl.frames[oldpacket & CL_UPDATE_MASK].packet_entities;
	}
	else
	{	// this is a full update that we can start delta compressing from now
		Q_memset( &dummy, 0, sizeof( dummy ) );
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	cl.validsequence = cls.netchan.incoming_sequence;

	oldindex = newindex = 0;

	MSG_StartBitReading( &net_message );

	while ( 1 )
	{
		peeklong = MSG_PeekBits( 16 );
		if ( !peeklong )
		{
			num = MSG_ReadBits( 16 );
		}
		else
		{
			num = CL_ParseDeltaHeader( &remove, &custom, &numbase, &newbl, &newblindex, full, &offset );
		}

		if ( !num )
		{
			while ( oldindex < oldp->num_entities )
			{	// copy all the rest of the entities from the old packet
				if ( newindex >= MAX_PACKET_ENTITIES )
					Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );

				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}

		newnum = num;

		oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while ( newnum > oldnum )
		{
			if ( full )
			{
				Con_Printf( "WARNING: oldcopy invalid on non-delta compressed update\n" );
				CL_FlushEntityPacket( false );
				CL_ClearPacket( newp );
				berror = true;
				break;
			}

			if ( newindex >= MAX_PACKET_ENTITIES )
				Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );

			// copy one of the old entities over to the new packet unchanged
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;

			oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if ( newnum < oldnum )
		{	// new from baseline
			entity_state_t	*baseline;

			if ( remove )
			{
				if ( full )
				{
					Con_Printf( "WARNING: remove invalid on non-delta compressed update\n" );
					CL_FlushEntityPacket( false );
					CL_ClearPacket( newp );
					berror = true;
					break;
				}
				continue;
			}

			if ( newindex >= MAX_PACKET_ENTITIES )
				Host_EndGame( "CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES" );

			if ( newbl )
			{
				baseline = &cl.instanced_baseline[newblindex];
			}
			else if ( offset )
			{
				baseline = &newp->entities[newindex - offset];
			}
			else
			{
				baseline = &cl_entities[newnum].baseline;
			}

			CL_EntityNum( newnum );

			if ( custom )
			{
				DELTA_ParseDelta( (byte *)baseline, (byte *)&newp->entities[newindex], *ppcustom );
				newp->entities[newindex].entityType = ENTITY_BEAM;
			}
			else
			{
				parsed_bits = DELTA_ParseDelta( (byte *)baseline, (byte *)&newp->entities[newindex], CL_IsPlayerIndex( num ) ? *ppplayer : *ppentity );
				newp->entities[newindex].entityType = ENTITY_NORMAL;

				if ( CL_IsPlayerIndex( num ) && playerbits )
					*playerbits += parsed_bits;
			}

			newp->entities[newindex].number = num;

			if ( cl_needinstanced.value && num > cl.highentity && !newbl )
			{
				Con_DPrintf( "Non-instanced %i %s without baseline\n", num,
					newp->entities[newindex].modelindex ? cl.model_precache[newp->entities[newindex].modelindex]->name : "?" );
			}

			if ( !full || !cls.passive )
			{
				CL_SetPacketFlag( newp, newindex, 1 );
			}

			newindex++;
			continue;
		}

		if ( newnum == oldnum )
		{	// delta from previous
			if ( full )
			{
				cl.validsequence = 0;
				Con_Printf( "WARNING: delta on full update" );
			}

			if ( remove )
			{
				R_KillDeadBeams( oldnum );
				oldindex++;
				continue;
			}

			if ( custom )
			{
				DELTA_ParseDelta( (byte *)&oldp->entities[oldindex], (byte *)&newp->entities[newindex], *ppcustom );
				newp->entities[newindex].entityType = ENTITY_BEAM;
			}
			else
			{
				parsed_bits = DELTA_ParseDelta( (byte *)&oldp->entities[oldindex], (byte *)&newp->entities[newindex], CL_IsPlayerIndex( num ) ? *ppplayer : *ppentity );
				newp->entities[newindex].entityType = ENTITY_NORMAL;

				if ( CL_IsPlayerIndex( num ) && playerbits )
					*playerbits += parsed_bits;
			}

			newp->entities[newindex].number = num;
			newindex++;
			oldindex++;
		}
	}

	MSG_EndBitReading( &net_message );

	if ( msg_badread )
	{
		Host_EndGame( "msg_badread in packetentities\n" );
		return;
	}

	CL_ProcessPacket( &cl.frames[newpacket] );
}

/*
==================
CL_PrintEntity

==================
*/
void CL_PrintEntity( cl_entity_t *ent )
{
}

/*
==================
CL_Particle

==================
*/
void CL_Particle( vec_t *origin, int color, float life, int zpos, int zvel )
{
	extern particle_t	*active_particles, *free_particles;
	particle_t			*p;

	p = free_particles;
	if ( p )
	{
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = life + cl.time;
		p->color = color;
		p->packedColor = 0;
		p->type = pt_static;

		VectorCopy( vec3_origin, p->vel );
		p->vel[2] += zvel;

		VectorCopy( origin, p->org );
		p->org[2] += zpos;
	}
}

/*
==================
CL_TouchLight

==================
*/
void CL_TouchLight( dlight_t *dl )
{
	int		i;

	i = dl - cl_dlights;
	if ( i >= 0 && i < MAX_DLIGHTS )
	{
		r_dlightchanged |= ( 1 << i );
	}
}

/*
==================
CL_PlayerFlashlight

==================
*/
void CL_PlayerFlashlight( void )
{
	cl_entity_t	*ent;
	dlight_t	*dl;

	ent = cl_entities + cl.playernum + 1;

	if ( ( ent->curstate.effects & ( EF_BRIGHTLIGHT | EF_DIMLIGHT ) ) && cl.worldmodel )
	{
		if ( cl.pLight && cl.pLight->key == 1 )
		{
			dl = cl.pLight;
		}
		else
		{
			dl = efx.CL_AllocDlight( 1 );
		}

		if ( ent->curstate.effects & EF_BRIGHTLIGHT )
		{
			dl->color.r = dl->color.g = dl->color.b = 250;
			dl->radius = 400;
			VectorCopy( ent->origin, dl->origin );
			dl->origin[2] += 16;
		}
		else
		{
			pmtrace_t	trace;
			vec3_t		end;
			float		falloff;
			vec3_t		vecForward;

			AngleVectors( r_playerViewportAngles, vecForward, NULL, NULL );

			VectorCopy( ent->origin, dl->origin );
			VectorAdd( cl.viewheight, dl->origin, dl->origin );

			VectorMA( dl->origin, 2000, vecForward, end );

			pmove->usehull = 2;
			trace = PM_PlayerTrace( dl->origin, end, PM_STUDIO_BOX, -1 );

			if ( trace.ent > 0 && pmove->physents[trace.ent].studiomodel )
			{
				VectorCopy( pmove->physents[trace.ent].origin, trace.endpos );
			}

			VectorCopy( trace.endpos, dl->origin );

			falloff = trace.fraction * 2000;
			if ( falloff < 500 )
			{
				falloff = 1.0;
			}
			else
			{
				falloff = 500.0 / falloff;
			}

			falloff *= falloff;

			dl->radius = 80;
			dl->color.r = dl->color.g = dl->color.b = 255 * falloff;
		}

		cl.pLight = dl;
		dl->die = cl.time + 0.2;

		CL_TouchLight( dl );
	}
	else
	{
		if ( cl.pLight && cl.pLight->key == 1 )
		{
			cl.pLight->die = cl.time;
		}
		cl.pLight = NULL;
	}

	if ( cl_entities[cl.viewentity].curstate.effects & EF_MUZZLEFLASH )
	{
		cl.viewent.curstate.effects |= EF_MUZZLEFLASH;
	}
}

/*
==================
CL_AddModelEffects

==================
*/
void CL_AddModelEffects( int keynum, cl_entity_t *ent, model_t *model )
{
	vec3_t		oldorigin;
	dlight_t	*dl;

	if ( !model )
		return;

	VectorCopy( ent->latched.prevorigin, oldorigin );

	if ( ent->model->flags & EF_GIB )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 2 );
	}
	else if ( ent->model->flags & EF_ZOMGIB )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 4 );
	}
	else if ( ent->model->flags & EF_TRACER )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 3 );
	}
	else if ( ent->model->flags & EF_TRACER2 )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 5 );
	}
	else if ( ent->model->flags & EF_ROCKET )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 0 );

		dl = efx.CL_AllocDlight( keynum );
		VectorCopy( ent->origin, dl->origin );
		dl->radius = 200;
		dl->color.r = dl->color.g = dl->color.b = 200;
		dl->die = cl.time + 0.01;
	}
	else if ( ent->model->flags & EF_GRENADE )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 1 );
	}
	else if ( ent->model->flags & EF_TRACER3 )
	{
		efx.R_RocketTrail( oldorigin, ent->origin, 6 );
	}
}

/*
==================
CL_LinkCustomEntity

==================
*/
void CL_LinkCustomEntity( cl_entity_t *ent, entity_state_t *state )
{
	ent->curstate.movetype = state->modelindex;

	if ( ent->model->type != mod_sprite )
	{
		Sys_Error( "Bad model on beam ( not sprite ) " );
	}

	ent->prevstate = ent->curstate;

	ent->latched.prevsequence = ent->curstate.sequence;
	VectorCopy( ent->origin, ent->latched.prevorigin );
	VectorCopy( ent->angles, ent->latched.prevangles );

	if ( !ClientDLL_AddEntity( ET_BEAM, &cl_entities[ent->index] ) )
	{
		Con_DPrintf( "Overflow beam entity list!\n" );
	}
}

/*
==================
CL_LinkPacketEntities

==================
*/
void CL_LinkPacketEntities( void )
{
	cl_entity_t			*ent;
	packet_entities_t	*pack;
	entity_state_t		*state;
	float				f;
	dlight_t			*dl;
	int					pnum;
	float				saveTime;
	int					i;
	qboolean			parametric;

	pack = &cl.frames[cl.parsecountmod].packet_entities;

	saveTime = cl.time;
	f = 0;

	for ( pnum = 0; pnum < pack->num_entities; pnum++ )
	{
		state = &pack->entities[pnum];

		// Players are handled elsewhere
		if ( state->number >= 1 && state->number <= cl.maxclients )
			continue;

		// if set to invisible, skip
		if ( !state->modelindex || ( state->effects & EF_NODRAW ) )
			continue;

		ent = &cl_entities[state->number];

		ent->curstate = *state;

		if ( !ent->model )
		{
			Con_Printf( "Tried to link edict %i without model\n", ent->index );
			continue;
		}

		parametric = ent->curstate.impacttime != 0 && ent->curstate.starttime != 0;

		if ( !parametric )
		{
			if ( ent->curstate.animtime == ent->prevstate.animtime && !VectorCompare( ent->curstate.origin, ent->prevstate.origin ) )
			{
				ent->lastmove = cl.time + 0.2;
			}

			if ( ( ent->curstate.eflags & EFLAG_SLERP ) && ent->curstate.animtime != 0.0 && ent->model->type == mod_studio )
			{
				if ( ent->lastmove < cl.time )
				{
					ent->curstate.movetype = MOVETYPE_STEP;
				}
				else
				{
					ent->curstate.movetype = MOVETYPE_NONE;
					R_ResetLatched( ent, true );
					VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
					VectorCopy( ent->curstate.angles, ent->latched.prevangles );
				}
			}
		}

		if ( ent->model->name[0] == '*' )
		{
			CL_InterpolateModel( ent );
		}
		else if ( parametric )
		{
			CL_ParametricMove( ent );
			VectorCopy( ent->curstate.origin, ent->origin );
			VectorCopy( ent->curstate.angles, ent->angles );
		}
		else if ( ent->curstate.movetype != MOVETYPE_NONE && ent->curstate.movetype != MOVETYPE_WALK &&
			ent->curstate.movetype != MOVETYPE_FLY && ent->curstate.movetype != MOVETYPE_STEP )
		{
			if ( !CL_InterpolateModel( ent ) )
				continue;
		}
		else if ( ( g_bIsCStrike || g_bIsCZero ) && ent->curstate.movetype == MOVETYPE_STEP && cls.netchan.remote_address.type != NA_LOOPBACK )
		{
			if ( !CL_InterpolateModel( ent ) )
				continue;
		}
		else
		{
			VectorCopy( ent->curstate.origin, ent->origin );
			VectorCopy( ent->curstate.angles, ent->angles );
		}

		if ( !( state->entityType & ENTITY_NORMAL ) )
		{
			CL_LinkCustomEntity( ent, state );
			continue;
		}

		if ( cl_numvisedicts == MAX_VISEDICTS )
			break;	// object list is full

		if ( ent->curstate.aiment )
		{
			ent->curstate.movetype = MOVETYPE_FOLLOW;
		}

		if ( ent->curstate.effects & EF_NOINTERP )
		{
			R_ResetLatched( ent, false );
		}

		if ( CL_EntityTeleported( ent ) )
		{
			VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
			VectorCopy( ent->curstate.angles, ent->latched.prevangles );
			CL_ResetPositions( ent );
		}

		if ( ent->curstate.effects & EF_BRIGHTFIELD )
		{
			efx.R_EntityParticles( ent );
		}

		if ( ent->index != 1 )
		{
			if ( ent->curstate.effects & EF_BRIGHTLIGHT )
			{
				dl = efx.CL_AllocDlight( ent->index );
				VectorCopy( ent->origin, dl->origin );
				dl->origin[2] += 16;
				dl->color.r = dl->color.g = dl->color.b = 250;
				dl->radius = RandomFloat( 400, 431 );
				dl->die = cl.time + 0.001;
			}

			if ( ent->curstate.effects & EF_DIMLIGHT )
			{
				dl = efx.CL_AllocDlight( ent->index );
				VectorCopy( ent->origin, dl->origin );
				dl->color.r = dl->color.g = dl->color.b = 100;
				dl->radius = RandomFloat( 200, 231 );
				dl->die = cl.time + 0.001;
			}
		}

		if ( ent->curstate.effects & EF_LIGHT )
		{
			dlight_t	*dl;

			dl = efx.CL_AllocDlight( state->number );
			VectorCopy( ent->origin, dl->origin );
			dl->radius = 200;
			dl->color.r = dl->color.g = dl->color.b = 100;
			dl->die = cl.time + 0.001;

			efx.R_RocketFlare( ent->origin );
		}

		CL_AddModelEffects( state->number, ent, ent->model );

		for ( i = 0; i < 4; i++ )
		{
			VectorCopy( ent->origin, ent->attachment[i] );
		}

		ClientDLL_AddEntity( ET_NORMAL, ent );
	}
}

/*
==================
CL_LinkPlayers

Create visible entities in the correct position
for all current players
==================
*/
void CL_LinkPlayers( void )
{
	int				i, j;
	entity_state_t	*state;
	cl_entity_t		*ent;
	frame_t			*frame;
	dlight_t		*dl;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0, state = frame->playerstate; j < MAX_CLIENTS; j++, state++ )
	{
		if ( state->messagenum != cl.parsecount )
			continue;	// not present this frame

		if ( !ClientDLL_IsThirdPerson() && j == cl.viewentity - 1 && !chase_active.value )
			continue;

		if ( !state->modelindex || ( state->effects & EF_NODRAW ) )
			continue;

		// grab an entity to fill in
		if ( cl_numvisedicts >= MAX_VISEDICTS )
			break;	// object list is full

		ent = &cl_entities[j + 1];

		if ( ent->index != j + 1 )
		{
			ent->index = j + 1;
		}

		if ( j == cl.playernum )
		{
			VectorCopy( state->origin, ent->origin );
			VectorCopy( ent->origin, ent->curstate.origin );
			VectorCopy( ent->origin, ent->prevstate.origin );
			VectorCopy( ent->curstate.angles, ent->angles );
		}

		if ( ent->curstate.effects & EF_NOINTERP )
		{
			R_ResetLatched( ent, false );
		}

		if ( CL_EntityTeleported( ent ) )
		{
			VectorCopy( ent->curstate.origin, ent->latched.prevorigin );
			VectorCopy( ent->curstate.angles, ent->latched.prevangles );
			CL_ResetPositions( ent );
		}

		if ( j != cl.playernum )
		{
			VectorCopy( ent->curstate.origin, ent->origin );
			VectorCopy( ent->curstate.angles, ent->angles );

			CL_ComputePlayerOrigin( ent );
		}
		else
		{
			VectorCopy( cl.simorg, ent->origin );
		}

		if ( j != cl.viewentity - 1 )
		{
			if ( ent->curstate.effects & EF_BRIGHTLIGHT )
			{
				dl = efx.CL_AllocDlight( i );
				VectorCopy( ent->origin, dl->origin );
				dl->origin[2] += 16;
				dl->color.r = dl->color.g = dl->color.b = 250;
				dl->radius = RandomFloat( 400, 431 );
				dl->die = cl.time + 0.001;
			}

			if ( ent->curstate.effects & EF_DIMLIGHT )
			{
				dl = efx.CL_AllocDlight( i );
				VectorCopy( ent->origin, dl->origin );
				dl->color.r = dl->color.g = dl->color.b = 100;
				dl->radius = RandomFloat( 200, 231 );
				dl->die = cl.time + 0.001;
			}
		}

		for ( i = 0; i < 4; i++ )
		{
			VectorCopy( ent->origin, ent->attachment[i] );
		}

		ClientDLL_AddEntity( ET_PLAYER, ent );
	}

	CL_PlayerFlashlight();
}

/*
==================
CL_AddStateToEntlist

==================
*/
void CL_AddStateToEntlist( physent_t *pe, entity_state_t *state )
{
	pe->info = state->number;
	if ( pe->info >= 1 && pe->info <= cl.maxclients )
	{
		// This should be the client index
		pe->player = pe->info;
	}

	VectorCopy( state->origin, pe->origin );
	VectorCopy( state->angles, pe->angles );
	VectorCopy( state->mins, pe->mins );
	VectorCopy( state->maxs, pe->maxs );

	pe->frame = state->frame;
	pe->sequence = state->sequence;
	pe->rendermode = state->rendermode;
	pe->skin = state->skin;
	pe->solid = state->solid;

	Q_memcpy( &pe->controller[0], &state->controller[0], 4 * sizeof( byte ) );
	Q_memcpy( &pe->blending[0], &state->blending[0], 2 * sizeof( byte ) );

	pe->movetype = state->movetype;
	pe->takedamage = 0;
	pe->blooddecal = 0;

	if ( pe->solid == SOLID_BSP )
	{
		pe->model = CL_GetModelByIndex( state->modelindex );
		pe->studiomodel = NULL;
		Q_strncpy( pe->name, pe->model->name, 32 );
		pe->name[31] = 0;
	}
	else if ( pe->solid == SOLID_BBOX )
	{
		model_t	*pModel;

		pe->model = NULL;
		pe->studiomodel = NULL;

		pModel = CL_GetModelByIndex( state->modelindex );

		Q_strncpy( pe->name, pModel->name, 32 );
		pe->name[31] = 0;

		if ( pModel->flags & STUDIO_TRACE_HITBOX )
		{
			pe->studiomodel = pModel;
		}
	}
	else
	{
		if ( CL_GetModelByIndex( state->modelindex )->type == mod_studio )
		{
			pe->model = NULL;
			pe->studiomodel = CL_GetModelByIndex( state->modelindex );
			Q_strncpy( pe->name, pe->studiomodel->name, 32 );
			pe->name[31] = 0;
		}
		else
		{
			pe->model = CL_GetModelByIndex( state->modelindex );
			pe->studiomodel = NULL;
			Q_strncpy( pe->name, pe->model->name, 32 );
			pe->name[31] = 0;
		}
	}

	// Copy the mod-specific info
	pe->iuser1 = state->iuser1;
	pe->iuser2 = state->iuser2;
	pe->iuser3 = state->iuser3;
	pe->iuser4 = state->iuser4;
	pe->fuser1 = state->fuser1;
	pe->fuser2 = state->fuser2;
	pe->fuser3 = state->fuser3;
	pe->fuser4 = state->fuser4;
	VectorCopy( state->vuser1, pe->vuser1 );
	VectorCopy( state->vuser2, pe->vuser2 );
	VectorCopy( state->vuser3, pe->vuser3 );
	VectorCopy( state->vuser4, pe->vuser4 );

	pe->blending[0] = state->blending[0];
	pe->blending[1] = state->blending[1];

	pe->controller[0] = state->controller[0];
	pe->controller[1] = state->controller[1];
	pe->controller[2] = state->controller[2];
	pe->controller[3] = state->controller[3];

	pe->rendermode = state->rendermode;
	pe->sequence = state->sequence;
	pe->team = state->team;
	pe->classnumber = state->playerclass;
}

/*
==================
CL_SetSolidEntities

Builds all the pmove physents for the current frame
==================
*/
void CL_SetSolidEntities( void )
{
	int					i;
	frame_t				*frame;
	packet_entities_t	*pak;
	entity_state_t		*state;
	physent_t			*pe;
	physent_t			*ve;
	vec3_t				delta;
	model_t				*model;

	pmove = &g_clmove;

	Q_memset( &pmove->physents[0], 0, sizeof( physent_t ) );
	Q_memset( &pmove->visents[0], 0, sizeof( physent_t ) );

	pmove->physents[0].model = cl.worldmodel;
	if ( cl.worldmodel )
	{
		Q_strncpy( pmove->physents[0].name, cl.worldmodel->name, 32 );
		pmove->physents[0].name[31] = 0;
	}

	VectorCopy( vec3_origin, pmove->physents[0].origin );
	pmove->physents[0].info = 0;
	pmove->physents[0].solid = SOLID_BSP;
	pmove->physents[0].movetype = MOVETYPE_NONE;
	pmove->physents[0].takedamage = DAMAGE_YES;
	pmove->physents[0].blooddecal = 0;

	pmove->numphysent = 1;
	pmove->numvisent = 1;

	pmove->visents[0] = pmove->physents[0];

	pmove->nummoveent = 0;

	frame = &cl.frames[cl.parsecountmod];
	pak = &frame->packet_entities;

	for ( i = 0; i < pak->num_entities; i++ )
	{
		state = &pak->entities[i];

		// players are added elsewhere
		if ( state->number >= 1 && state->number <= cl.maxclients )
			continue;

		if ( !state->modelindex )
			continue;

		model = CL_GetModelByIndex( state->modelindex );
		if ( !model )
			continue;

		// don't collide with objects that the local player owns
		if ( state->owner != 0 && state->owner - 1 == cl.playernum )
			continue;

		if ( model->hulls[1].firstclipnode || model->type == mod_studio )
		{
			ve = &pmove->visents[pmove->numvisent];
			pmove->numvisent++;
			CL_AddStateToEntlist( ve, state );
		}

		// triggers and nonsolid entities other than ladders are not physents
		if ( state->solid == SOLID_TRIGGER || ( state->solid == SOLID_NOT && state->skin >= CONTENTS_EMPTY ) )
			continue;

		// dead monsters
		if ( state->mins[2] == 0.0 && state->maxs[2] == 1.0 )
			continue;

		// no size
		VectorSubtract( state->maxs, state->mins, delta );
		if ( Length( delta ) == 0.0 )
			continue;

		if ( model->hulls[1].firstclipnode || model->type == mod_studio )
		{
			if ( state->solid == SOLID_NOT && state->skin == CONTENTS_LADDER )
			{
				if ( pmove->nummoveent >= MAX_MOVEENTS )
				{
					Con_DPrintf( "CL_SetSolidEntities:  pmove->nummoveent >= MAX_MOVEENTS\n" );
					continue;
				}

				pe = &pmove->moveents[pmove->nummoveent];
				pmove->nummoveent++;
			}
			else
			{
				if ( pmove->numphysent >= MAX_PHYSENTS )
				{
					Con_DPrintf( "CL_SetSolidEntities:  pmove->numphysent >= MAX_PHYSENTS\n" );
					continue;
				}

				pe = &pmove->physents[pmove->numphysent];
				pmove->numphysent++;
			}

			CL_AddStateToEntlist( pe, state );
		}
	}
}

/*
===============
CL_SetUpPlayerPrediction

Calculate the new position of players, without other player clipping

We do this to set up real player prediction.
Players are predicted twice, first without clipping other players,
then with clipping against them.
This sets up the first phase.
===============
*/
void CL_SetUpPlayerPrediction( qboolean dopred, qboolean bIncludeLocalClient )
{
	int						j;
	entity_state_t			*state;
	frame_t					*frame;
	struct predicted_player	*pplayer;
	cl_entity_t				*ent;

	pmove = &g_clmove;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0, pplayer = predicted_players, state = frame->playerstate; j < MAX_CLIENTS; j++, pplayer++, state++ )
	{
		pplayer->active = false;

		if ( state->messagenum != cl.parsecount )
			continue;	// not present this frame

		if ( !state->modelindex )
			continue;

		if ( ( state->effects & EF_NODRAW ) && !bIncludeLocalClient && j == cl.playernum )
			continue;

		pplayer->active = true;
		pplayer->movetype = state->movetype;
		pplayer->solid = state->solid;
		pplayer->usehull = state->usehull;

		if ( j == cl.playernum )
		{
			VectorCopy( cl.frames[cl.parsecountmod].playerstate[cl.playernum].origin, pplayer->origin );
			VectorCopy( cl.frames[cl.parsecountmod].playerstate[cl.playernum].angles, pplayer->angles );
		}
		else
		{
			ent = &cl_entities[j + 1];

			CL_ComputePlayerOrigin( ent );

			VectorCopy( ent->origin, pplayer->origin );
			VectorCopy( ent->angles, pplayer->angles );
		}
	}
}

/*
===============
CL_SetSolidPlayers

Builds all the pmove physents for the current frame
Note that CL_SetUpPlayerPrediction() must be called first!
pmove must be setup with world and solid entity hulls before calling
(via CL_PredictMove)
===============
*/
void CL_SetSolidPlayers( int playernum )
{
	int						j;
	struct predicted_player	*pplayer;
	physent_t				*pent;
	entity_state_t			*state;
	frame_t					*frame;

	if ( !cl_solid_players.value )
		return;

	pent = pmove->physents + pmove->numphysent;

	frame = &cl.frames[cl.parsecountmod];

	for ( j = 0, pplayer = predicted_players, state = frame->playerstate; j < MAX_CLIENTS; j++, pplayer++, state++ )
	{
		if ( playernum == -1 )
		{
			if ( j != cl.playernum && !pplayer->active )
				continue;
		}
		else
		{
			if ( !pplayer->active )
				continue;	// not present this frame
		}

		// the player object never gets added
		if ( j == playernum && playernum != -1 )
			continue;

		if ( !pplayer->solid )
			continue;

		Q_memset( pent, 0, sizeof( physent_t ) );

		pent->player = 1;
		pent->model = NULL;

		VectorCopy( pplayer->origin, pent->origin );
		VectorCopy( player_mins[pplayer->usehull], pent->mins );
		VectorCopy( player_maxs[pplayer->usehull], pent->maxs );

		_snprintf( pent->name, sizeof( pent->name ), "player %i", j );

		pent->solid = pplayer->solid;
		pent->skin = 0;
		pent->info = j + 1;
		pent->movetype = pplayer->movetype;
		pent->blooddecal = 0;
		pent->takedamage = DAMAGE_YES;

		VectorCopy( pplayer->angles, pent->angles );

		// Copy the mod-specific info
		pent->iuser1 = state->iuser1;
		pent->iuser2 = state->iuser2;
		pent->iuser3 = state->iuser3;
		pent->iuser4 = state->iuser4;
		pent->fuser1 = state->fuser1;
		pent->fuser2 = state->fuser2;
		pent->fuser3 = state->fuser3;
		pent->fuser4 = state->fuser4;
		VectorCopy( state->vuser1, pent->vuser1 );
		VectorCopy( state->vuser2, pent->vuser2 );
		VectorCopy( state->vuser3, pent->vuser3 );
		VectorCopy( state->vuser4, pent->vuser4 );

		pent->blending[0] = state->blending[0];
		pent->blending[1] = state->blending[1];

		pent->controller[0] = state->controller[0];
		pent->controller[1] = state->controller[1];
		pent->controller[2] = state->controller[2];
		pent->controller[3] = state->controller[3];

		pent->frame = state->frame;
		pent->rendermode = state->rendermode;
		pent->sequence = state->sequence;
		pent->team = state->team;
		pent->classnumber = state->playerclass;

		pmove->numphysent++;
		pent++;
	}
}

/*
==================
CL_MoveAiments

==================
*/
void CL_MoveAiments( void )
{
	cl_entity_t	*ent;
	int			i;

	for ( i = 0; i < cl_numvisedicts; i++ )
	{
		ent = cl_visedicts[i];

		if ( ent->curstate.aiment && ent->curstate.movetype == MOVETYPE_FOLLOW )
		{
			VectorCopy( cl_entities[ent->curstate.aiment].origin, ent->origin );
		}
	}
}

/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float CL_LerpPoint( void )
{
	double	f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if ( !f || cls.timedemo )
	{
		float	fgap;

		fgap = cl.time - cl.oldtime;

		cl.time = cl.mtime[0];

		if ( cls.demoplayback )
		{
			cl.oldtime = cl.time - fgap;
		}

		return 1.0f;
	}

	if ( ex_interp.value > 0.001 )
	{
		frac = ( cl.time - cl.mtime[0] ) / ex_interp.value;
	}
	else
	{
		frac = 1.0;
	}

	return frac;
}

/*
===============
CL_EmitEntities

===============
*/
void CL_EmitEntities( void )
{
	mleaf_t	*pleaf;
	byte	*ppvs;

	cl_numvisedicts = 0;
	cl_numbeamentities = 0;

	if ( cls.state != ca_active )
		return;

	if ( !cl.validsequence )
		return;

	if ( cl.frames[cl.parsecountmod].invalid )
		return;

	cl.commands[( cls.netchan.outgoing_sequence - 1 ) & CL_UPDATE_MASK].frame_lerp = CL_LerpPoint();

	if ( cls.spectator )
	{
		pleaf = Mod_PointInLeaf( cl.simorg, cl.worldmodel );
		ppvs = Mod_LeafPVS( pleaf, cl.worldmodel );
		PVSMark( cl.worldmodel, ppvs );
	}

	CL_LinkPlayers();
	CL_LinkPacketEntities();
	CL_MoveAiments();

	ClientDLL_CreateEntities();

	CL_TempEntUpdate();

	if ( cls.spectator )
	{
		r_oldviewleaf = NULL;
		R_MarkLeaves();
	}

	CL_FireEvents();
}
