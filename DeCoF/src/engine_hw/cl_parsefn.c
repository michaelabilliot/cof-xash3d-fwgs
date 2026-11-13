// cl_parsefn.c -- the handlers of the server to client messages, the client
// side event queue

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "host.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "GL_MODEL.H"
#include "protocol.h"
#include "vid.h"
#include "draw.h"
#include "sound.h"
#include "screen.h"
#include "vgui_int.h"
#include "cdll_int.h"
#include "APIProxy.h"

// r_part.c
void	R_ParseParticleEffect( void );

// cd.cpp
void	CDAudio_Play( int track, int looping );
void	CDAudio_Pause( void );
void	CDAudio_Resume( void );

static double	g_clockdelta;

/*
==================
CL_AdjustClock

==================
*/
void CL_AdjustClock( void )
{
	double	msec;
	double	adjust;
	double	sign;

	if ( g_clockdelta == 0.0 || !cl_fixtimerate.value )
		return;

	if ( fabs( g_clockdelta ) < 0.001 )
		return;

	if ( cl_fixtimerate.value < 0.0 )
		cl_fixtimerate.value = 7.5;

	msec = (int)( 1000.0 * g_clockdelta );
	sign = ( msec >= 0 ) ? -1.0 : 1.0;

	adjust = fabs( msec );
	adjust = min( adjust, cl_fixtimerate.value );
	adjust /= 1000.0;
	adjust *= sign;

	if ( fabs( g_clockdelta ) > fabs( adjust ) )
	{
		g_clockdelta += adjust;
		cl.time += adjust;
	}

	if ( cl.time < cl.oldtime )
		cl.oldtime = cl.time;
}

/*
==================
CL_Parse_Time

==================
*/
void CL_Parse_Time( void )
{
	double	dt;

	cl.mtime[1] = cl.mtime[0];
	cl.mtime[0] = MSG_ReadFloat();

	if ( cl.maxclients == 1 )
		cl.time = cl.mtime[0];

	dt = cl.time - cl.mtime[0];

	if ( fabs( dt ) > cl_clockreset.value )
	{
		cl.time = cl.mtime[0];
		g_clockdelta = 0.0;
	}
	else if ( dt != 0.0 )
	{
		g_clockdelta = dt;
	}

	if ( cl.time < cl.oldtime )
		cl.oldtime = cl.time;
}

/*
==================
CL_Parse_ClientData

==================
*/
void CL_Parse_ClientData( void )
{
	CL_ParseClientdata();
}

/*
==================
CL_Parse_Disconnect

==================
*/
void CL_Parse_Disconnect( void )
{
	char	*msg;

	msg = MSG_ReadString();

	SCR_EndLoadingPlaque();

	if ( msg && strlen( msg ) > 0 )
	{
		COM_ExplainDisconnection( true, "#GameUI_DisconnectedFromServerExtended" );
		COM_ExtendedExplainDisconnection( true, msg );

		if ( !_strnicmp( msg, "Invalid VALVE CD Key\n", strlen( "Invalid VALVE CD Key\n" ) ) )
		{
			ValidateCDKey( true, true );
		}
		else if ( !_strnicmp( msg, "Invalid STEAM UserID Ticket\n", strlen( "Invalid STEAM UserID Ticket\n" ) ) )
		{
			VGUI2_OnDisconnectFromServer( 1 );
		}
		else if ( !Q_stricmp( msg, "No Steam logon\n" ) )
		{
			VGUI2_OnDisconnectFromServer( 2 );
		}
		else if ( !Q_stricmp( msg, "Steam connection lost\n" ) )
		{
			VGUI2_OnDisconnectFromServer( 5 );
		}
		else if ( !Q_stricmp( msg, "Unable to connect to Steam\n" ) )
		{
			VGUI2_OnDisconnectFromServer( 6 );
		}
		else if ( !Q_stricmp( msg, "This Steam account is being used in another location\n" ) )
		{
			VGUI2_OnDisconnectFromServer( 4 );
		}
		else if ( !Q_stricmp( msg, "VAC banned from secure server\n" ) )
		{
			VGUI2_OnDisconnectFromServer( 3 );
		}
		else
		{
			VGUI2_OnDisconnectFromServer( 0 );
		}
	}
	else
	{
		COM_ExplainDisconnection( true, "#GameUI_DisconnectedFromServer" );
	}

	Host_EndGame( "Server disconnected\n" );
}

/*
==================
CL_Parse_Version

==================
*/
void CL_Parse_Version( void )
{
	int		i;

	i = MSG_ReadLong();
	if ( i != PROTOCOL_VERSION )
	{
		Host_Error( "CL_Parse_Version: Server is protocol %i instead of %i\n", i, PROTOCOL_VERSION );
	}
}

/*
==================
CL_Set_ServerExtraInfo

==================
*/
void CL_Set_ServerExtraInfo( void )
{
	char	val[32];
	char	*dir;

	dir = MSG_ReadString();
	allow_cheats = MSG_ReadByte();

	_snprintf( val, sizeof( val ), "%d", allow_cheats );
	Cvar_DirectSet( &sv_cheats, val );

	COM_AddDefaultDir( dir );
}

/*
==================
CL_Parse_SetView

==================
*/
void CL_Parse_SetView( void )
{
	cl.viewentity = MSG_ReadShort();
}

/*
==================
CL_Parse_Print

==================
*/
void CL_Parse_Print( void )
{
	Con_Printf( "%s", MSG_ReadString() );
}

/*
==================
CL_Parse_CenterPrint

==================
*/
void CL_Parse_CenterPrint( void )
{
	SCR_CenterPrint( MSG_ReadString() );
}

/*
==================
ValidStuffText

==================
*/
qboolean ValidStuffText( const char *cmd )
{
	qboolean	bGoodCommand;

	bGoodCommand = _strnicmp( cmd, "alias ", 6 ) != 0;
	bGoodCommand &= ( Q_stristr( cmd, "connect " ) == NULL );

	return bGoodCommand;
}

/*
==================
CL_Parse_StuffText

==================
*/
void CL_Parse_StuffText( void )
{
	char	*s;

	s = MSG_ReadString();
	if ( !ValidStuffText( s ) )
	{
		Con_Printf( "Server tried to send invalid command:\"%s\"", s );
	}
	else
	{
		Cbuf_AddText( s );
	}
}

/*
==================
CL_Parse_ServerInfo

==================
*/
void CL_Parse_ServerInfo( void )
{
	CL_ParseServerInfo();
	vid.recalc_refdef = true;	// leave intermission full screen
	sys_timescale.value = 1.0;
}

/*
==================
CL_Parse_SetAngle

==================
*/
void CL_Parse_SetAngle( void )
{
	int		i;

	for ( i = 0; i < 3; i++ )
		cl.viewangles[i] = MSG_ReadHiresAngle();
}

/*
==================
CL_Parse_AddAngle

==================
*/
void CL_Parse_AddAngle( void )
{
	float	ang;

	ang = MSG_ReadHiresAngle();
	cl.viewangles[YAW] += ang;
}

/*
==================
CL_Parse_CrosshairAngle

==================
*/
void CL_Parse_CrosshairAngle( void )
{
	cl.crosshairangle[0] = MSG_ReadChar() * 0.2;
	cl.crosshairangle[1] = MSG_ReadChar() * 0.2;
}

/*
==================
CL_Parse_LightStyle

==================
*/
void CL_Parse_LightStyle( void )
{
	int		i;

	i = MSG_ReadByte();
	if ( i >= MAX_LIGHTSTYLES )
		Sys_Error( "svc_lightstyle > MAX_LIGHTSTYLES" );

	Q_strncpy( cl_lightstyle[i].map, MSG_ReadString(), MAX_STYLESTRING - 1 );
	cl_lightstyle[i].map[MAX_STYLESTRING - 1] = 0;
	cl_lightstyle[i].length = Q_strlen( cl_lightstyle[i].map );
}

/*
==================
CL_Parse_StopSound

==================
*/
void CL_Parse_StopSound( void )
{
	int		i;

	i = MSG_ReadShort();
	S_StopSound( i >> 3, i & 7 );
}

/*
==================
CL_Parse_Particle

==================
*/
void CL_Parse_Particle( void )
{
	R_ParseParticleEffect();
}

/*
==================
CL_Parse_SpawnBaseline

==================
*/
void CL_Parse_SpawnBaseline( void )
{
	CL_ParseBaseline();
}

/*
==================
CL_Parse_SpawnStatic

==================
*/
void CL_Parse_SpawnStatic( void )
{
	CL_ParseStatic();
}

/*
==================
CL_Parse_TempEntity

==================
*/
void CL_Parse_TempEntity( void )
{
	CL_ParseTEnt();
}

/*
==================
CL_Parse_SetPause

==================
*/
void CL_Parse_SetPause( void )
{
	cl.paused = MSG_ReadByte();

	if ( !VGuiWrap2_IsInCareerMatch() )
	{
		if ( cl.paused )
		{
			CDAudio_Pause();
		}
		else
		{
			CDAudio_Resume();
		}
	}
}

/*
==================
CL_Parse_SignonNum

==================
*/
void CL_Parse_SignonNum( void )
{
	int		i;

	i = MSG_ReadByte();
	if ( i <= cls.signon )
	{
		Con_Printf( "Received signon %i when at %i\n", i, cls.signon );
		CL_Disconnect_f();
		return;
	}

	cls.signon = i;
	CL_SignonReply();
}

/*
==================
CL_Parse_SpawnStaticSound

==================
*/
void CL_Parse_SpawnStaticSound( void )
{
	CL_ParseStaticSound();
}

/*
==================
CL_Parse_CDTrack

==================
*/
void CL_Parse_CDTrack( void )
{
	cl.cdtrack = MSG_ReadByte();
	cl.looptrack = MSG_ReadByte();

	if ( COM_CheckParm( "-nocdaudio" ) )
		return;

	if ( ( cls.demoplayback || cls.demorecording ) && ( cls.forcetrack != -1 ) )
		CDAudio_Play( cls.forcetrack, true );
	else
		CDAudio_Play( cl.cdtrack, true );
}

/*
==================
CL_Parse_Restore

==================
*/
void CL_Parse_Restore( void )
{
	CL_Restore( MSG_ReadString() );
}

/*
==================
CL_Parse_WeaponAnim

==================
*/
void CL_Parse_WeaponAnim( void )
{
	cl.weaponstarttime = 0;
	cl.weaponsequence = MSG_ReadByte();
	cl.viewent.curstate.body = MSG_ReadByte();
}

/*
==================
CL_Parse_DecalName

==================
*/
void CL_Parse_DecalName( void )
{
	int		i;

	i = MSG_ReadByte();
	Draw_DecalSetName( i, MSG_ReadString() );
}

/*
==================
CL_Parse_RoomType

==================
*/
void CL_Parse_RoomType( void )
{
	Cvar_SetValue( "room_type", MSG_ReadShort() );
}

/*
==================
CL_Parse_NewUserMsg

==================
*/
void CL_Parse_NewUserMsg( void )
{
	AddNewUserMsg();
}

/*
==================
CL_Parse_UpdateUserInfo

==================
*/
void CL_Parse_UpdateUserInfo( void )
{
	CL_UpdateUserinfo();
}

/*
==================
CL_Parse_Damage

==================
*/
void CL_Parse_Damage( void )
{
}

/*
==================
CL_Parse_KilledMonster

==================
*/
void CL_Parse_KilledMonster( void )
{
}

/*
==================
CL_Parse_FoundSecret

==================
*/
void CL_Parse_FoundSecret( void )
{
}

/*
==================
CL_Parse_Intermission

==================
*/
void CL_Parse_Intermission( void )
{
	cl.intermission = 1;
	vid.recalc_refdef = true;	// go to full screen
}

/*
==================
CL_Parse_Finale

==================
*/
void CL_Parse_Finale( void )
{
	cl.intermission = 2;
	vid.recalc_refdef = true;	// go to full screen
	SCR_CenterPrint( MSG_ReadString() );
}

/*
==================
CL_Parse_CutScene

==================
*/
void CL_Parse_CutScene( void )
{
	cl.intermission = 3;
	vid.recalc_refdef = true;	// go to full screen
	SCR_CenterPrint( MSG_ReadString() );
}

/*
==================
CL_Parse_PacketEntities

==================
*/
void CL_Parse_PacketEntities( void )
{
	cls.playerbits = 0;
	CL_ParsePacketEntities( false, &cls.playerbits );
	CL_SetSolidEntities();
}

/*
==================
CL_Parse_DeltaPacketEntities

==================
*/
void CL_Parse_DeltaPacketEntities( void )
{
	cls.playerbits = 0;
	CL_ParsePacketEntities( true, &cls.playerbits );
	CL_SetSolidEntities();
}

/*
==================
CL_Parse_Choke

==================
*/
void CL_Parse_Choke( void )
{
	cl.frames[cls.netchan.incoming_sequence & CL_UPDATE_MASK].choked = true;
}

/*
==================
CL_Parse_ResourceList

==================
*/
void CL_Parse_ResourceList( void )
{
	CL_ParseResourceList();
}

/*
==================
CL_Parse_ResourceLocation

==================
*/
void CL_Parse_ResourceLocation( void )
{
	CL_ParseResourceLocation();
}

/*
==================
CL_Parse_NewMoveVars

==================
*/
void CL_Parse_NewMoveVars( void )
{
	CL_ParseMovevars();
}

/*
==================
CL_Parse_ResourceRequest

==================
*/
void CL_Parse_ResourceRequest( void )
{
	CL_SendResourceListBlock();
}

/*
==================
CL_Parse_Customization

==================
*/
void CL_Parse_Customization( void )
{
	CL_ParseCustomization();
}

/*
==================
CL_Parse_SoundFade

==================
*/
void CL_Parse_SoundFade( void )
{
	CL_ParseSoundFade();
}

/*
=============
CL_ResetEvent

=============
*/
void CL_ResetEvent( event_info_t *ei )
{
	ei->index = 0;
	Q_memset( &ei->args, 0, sizeof( ei->args ) );
	ei->fire_time = 0.0;
	ei->flags = 0;
}

/*
=============
CL_CalcPlayerVelocity

=============
*/
void CL_CalcPlayerVelocity( int idx, vec_t *velocity )
{
	clientdata_t	*pcd;

	velocity[0] = 0;
	velocity[1] = 0;
	velocity[2] = 0;

	if ( idx < 1 || idx > cl.maxclients )
		return;

	if ( idx - 1 == cl.playernum )
	{
		pcd = &cl.frames[cl.parsecountmod].clientdata;
		VectorCopy( pcd->velocity, velocity );
	}
	else
	{
		vec3_t	delta;
		float	dt;

		VectorSubtract( cl_entities[idx].curstate.velocity, cl_entities[idx].prevstate.velocity, delta );
		dt = cl_entities[idx].curstate.animtime - cl_entities[idx].prevstate.animtime;

		if ( dt == 0 )
		{
			VectorCopy( cl_entities[idx].curstate.velocity, velocity );
		}
		else
		{
			VectorScale( delta, 1.0f / dt, velocity );
		}
	}
}

/*
=============
CL_DescribeEvent

=============
*/
void CL_DescribeEvent( int slot, char *eventname )
{
	int		idx;

	idx = slot & 31;

	if ( !cl_showevents.value )
		return;

	if ( !eventname )
		return;

	Con_NPrintf( idx, "%i %f %s", slot, cl.time, eventname );
}

/*
=============
CL_FireEvents

=============
*/
void CL_FireEvents( void )
{
	int				i;
	event_state_t	*es;
	event_info_t	*ei;
	event_hook_t	*hook;

	es = &cl.events;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];

		if ( ei->index == 0 )
			continue;

		// Delayed event!
		if ( ei->fire_time && ( ei->fire_time > cl.time ) )
			continue;

		hook = CL_FindEventHook( (char *)cl.event_precache[ei->index].filename );
		if ( hook )
		{
			CL_DescribeEvent( i, (char *)cl.event_precache[ei->index].filename );
			hook->pfnEvent( &ei->args );
		}
		else
		{
			Con_DPrintf( "CL_FireEvents:  Unknown event %i:%s\n", ei->index, cl.event_precache[ei->index].filename );
		}

		CL_ResetEvent( ei );
	}
}

/*
=============
CL_FindEmptyEvent

=============
*/
event_info_t *CL_FindEmptyEvent( void )
{
	int				i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;

	// Look for first slot where index is != 0
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if ( ei->index != 0 )
			continue;

		return ei;
	}

	// No slots available
	return NULL;
}

/*
=============
CL_FindUnreliableEvent

=============
*/
event_info_t *CL_FindUnreliableEvent( void )
{
	int				i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;

	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if ( ei->index != 0 )
		{
			// It's reliable, so skip it
			if ( ei->flags & FEV_RELIABLE )
				continue;
		}

		return ei;
	}

	// This should never happen
	return NULL;
}

/*
=============
CL_ClientQueueEvent

=============
*/
void CL_ClientQueueEvent( int flags, int index, int client_index, float delay, event_args_t *pargs )
{
	event_info_t	*ei;

	// Find a normal slot
	ei = CL_FindEmptyEvent();
	if ( !ei && !( flags & FEV_RELIABLE ) )
		return;

	// Reliable events can overwrite an unreliable one
	if ( !ei )
	{
		ei = CL_FindUnreliableEvent();
		if ( !ei )
			return;
	}

	ei->index = index;
	ei->packet_index = 0;
	ei->args = *pargs;
	if ( delay == 0 )
	{
		ei->fire_time = 0.0;
	}
	else
	{
		ei->fire_time = delay + cl.time;
	}
	ei->flags = flags;
}

/*
=============
CL_QueueEvent

=============
*/
void CL_QueueEvent( int flags, int index, float delay, event_args_t *pargs )
{
	event_info_t	*ei;

	// Find a normal slot
	ei = CL_FindEmptyEvent();
	if ( !ei && !( flags & FEV_RELIABLE ) )
		return;

	// Reliable events can overwrite an unreliable one
	if ( !ei )
	{
		ei = CL_FindUnreliableEvent();
		if ( !ei )
			return;
	}

	ei->index = index;
	ei->packet_index = 0;
	ei->args = *pargs;
	if ( delay == 0 )
	{
		ei->fire_time = 0.0;
	}
	else
	{
		ei->fire_time = delay + cl.time;
	}
	ei->flags = flags;
}

/*
=============
CL_ParseEvent

=============
*/
void CL_ParseEvent( void )
{
	int					i;
	int					num_events;
	int					event_index;
	int					referenced;
	int					packet_index;
	qboolean			hasargs, hasdelay;
	float				delay;
	event_args_t		nullargs;
	event_args_t		newargs;
	event_args_t		*from, *to;
	packet_entities_t	*pack;
	entity_state_t		*state;
	delta_t				**ppevent;

	delay = 0.0;
	ppevent = NULL;
	ppevent = DELTA_LookupRegistration( "event_t" );

	Q_memset( &nullargs, 0, sizeof( nullargs ) );
	Q_memset( &newargs, 0, sizeof( newargs ) );

	MSG_StartBitReading( &net_message );

	num_events = MSG_ReadBits( 5 );

	for ( i = 0; i < num_events; i++ )
	{
		event_index = MSG_ReadBits( 10 );

		referenced = MSG_ReadBits( 1 );
		packet_index = -1;
		if ( referenced )
		{
			packet_index = MSG_ReadBits( 11 );
		}

		hasargs = MSG_ReadBits( 1 ) ? true : false;
		if ( hasargs )
		{
			from = &nullargs;
			to = &newargs;
			DELTA_ParseDelta( (byte *)from, (byte *)to, *ppevent );
		}

		hasdelay = MSG_ReadBits( 1 ) ? true : false;
		if ( hasdelay )
		{
			delay = MSG_ReadBits( 16 ) / 100.0;
		}

		if ( packet_index != -1 )
		{
			pack = &cl.frames[cl.parsecountmod].packet_entities;

			if ( packet_index < pack->num_entities )
			{
				state = &pack->entities[packet_index];

				newargs.entindex = state->number;

				if ( VectorCompare( newargs.origin, vec3_origin ) )
				{
					VectorCopy( state->origin, newargs.origin );
				}

				if ( VectorCompare( newargs.angles, vec3_origin ) )
				{
					VectorCopy( state->angles, newargs.angles );
				}

				COM_NormalizeAngles( newargs.angles );

				if ( state->number >= 1 && state->number <= cl.maxclients )
				{
					newargs.angles[PITCH] *= -3.0;
					CL_CalcPlayerVelocity( state->number, newargs.velocity );
					newargs.ducking = ( state->usehull == 1 );
				}
			}
			else
			{
				if ( newargs.entindex )
				{
					if ( newargs.entindex >= 1 && newargs.entindex <= cl.maxclients )
					{
						newargs.angles[PITCH] *= -3.0;
					}
				}
				else
				{
					Con_DPrintf( "CL_ParseEvent:  Received non-packet entity index 0 for event\n" );
				}
			}

			CL_QueueEvent( 0, event_index, delay, &newargs );
		}
	}

	MSG_EndBitReading( &net_message );
}

/*
=============
CL_Parse_ReliableEvent

=============
*/
void CL_Parse_ReliableEvent( void )
{
	int				event_index;
	qboolean		hasdelay;
	float			delay;
	event_args_t	nullargs;
	event_args_t	newargs;
	event_args_t	*from, *to;
	delta_t			**ppevent;

	delay = 0.0;
	ppevent = NULL;
	ppevent = DELTA_LookupRegistration( "event_t" );

	Q_memset( &nullargs, 0, sizeof( nullargs ) );
	Q_memset( &newargs, 0, sizeof( newargs ) );

	MSG_StartBitReading( &net_message );

	event_index = MSG_ReadBits( 10 );

	from = &nullargs;
	to = &newargs;
	DELTA_ParseDelta( (byte *)from, (byte *)to, *ppevent );

	hasdelay = MSG_ReadBits( 1 ) ? true : false;
	if ( hasdelay )
	{
		delay = MSG_ReadBits( 16 ) / 100.0;
	}

	if ( newargs.entindex >= 1 && newargs.entindex <= cl.maxclients )
	{
		newargs.angles[PITCH] *= -3.0;
	}

	CL_QueueEvent( FEV_RELIABLE, event_index, delay, &newargs );

	MSG_EndBitReading( &net_message );
}

/*
=============
CL_Parse_Pings

=============
*/
void CL_Parse_Pings( void )
{
	int			i;
	qboolean	hasping;
	int			ping;
	int			packet_loss;

	MSG_StartBitReading( &net_message );

	while ( 1 )
	{
		hasping = MSG_ReadBits( 1 );
		if ( !hasping )
			break;

		i = MSG_ReadBits( 5 );
		ping = MSG_ReadBits( 12 );
		packet_loss = MSG_ReadBits( 7 );

		cl.players[i].ping = ping;
		cl.players[i].packet_loss = packet_loss;
	}

	MSG_EndBitReading( &net_message );
}

/*
=============
CL_Parse_HLTV

=============
*/
void CL_Parse_HLTV( void )
{
	CL_ParseHLTV();
}

/*
=============
CL_Parse_Director

=============
*/
void CL_Parse_Director( void )
{
	int		length;

	length = MSG_ReadByte();
	ClientDLL_DirectorMessage( length, &net_message.data[msg_readcount] );
	msg_readcount += length;
}

/*
=============
CL_Parse_Timescale

=============
*/
void CL_Parse_Timescale( void )
{
	sys_timescale.value = MSG_ReadFloat();

	if ( ( !cls.spectator && !cls.demoplayback ) || sys_timescale.value < 0.1 )
	{
		sys_timescale.value = 1.0;
	}
}

/*
=============
CL_Parse_VoiceInit

=============
*/
void CL_Parse_VoiceInit( void )
{
	char	*pCodec;
	int		quality;

	pCodec = MSG_ReadString();
	quality = MSG_ReadByte();

	if ( !pCodec[0] )
		return;

	Voice_Init( pCodec, quality );
}

/*
=============
CL_Parse_VoiceData

=============
*/
void CL_Parse_VoiceData( void )
{
	int		iEntity, nDataLength, nChannel;
	char	chReceived[8192];
	int		startReadCount;

	startReadCount = msg_readcount;

	iEntity = MSG_ReadByte() + 1;
	nDataLength = MSG_ReadShort();
	nDataLength = min( nDataLength, sizeof( chReceived ) );
	MSG_ReadBuf( nDataLength, chReceived );

	cl.frames[cl.parsecountmod].voicebytes += msg_readcount - startReadCount;

	if ( iEntity < 0 || iEntity >= cl.num_entities )
		return;

	if ( iEntity == cl.playernum + 1 )
	{
		Voice_LocalPlayerTalkingAck( 1 );
	}

	if ( !nDataLength )
		return;

	nChannel = Voice_GetChannel( iEntity );
	if ( nChannel == -1 )
	{
		nChannel = Voice_AssignChannel( iEntity );
		if ( nChannel == -1 )
		{
			Con_DPrintf( "CL_ParseVoiceData: Voice_AssignChannel failed for client %d!\n", iEntity - 1 );
			return;
		}
	}

	if ( nDataLength )
	{
		Voice_AddIncomingData( nChannel, chReceived, nDataLength, cls.netchan.incoming_sequence );
	}
}

static event_hook_t	*g_pEventHooks;

/*
=============
CL_FindEventHook

=============
*/
event_hook_t *CL_FindEventHook( char *name )
{
	event_hook_t	*hook;

	hook = g_pEventHooks;
	while ( hook )
	{
		if ( !Q_stricmp( name, hook->name ) )
			return hook;

		hook = hook->next;
	}

	return NULL;
}

/*
=============
CL_HookEvent

=============
*/
void CL_HookEvent( char *name, void ( *pfnEvent )( event_args_t *args ) )
{
	event_hook_t	*hook;

	g_engdstAddrs.pfnHookEvent( &name, &pfnEvent );

	if ( !name || !name[0] )
	{
		Con_Printf( "CL_HookEvent:  Must provide a valid event name\n" );
		return;
	}

	if ( !pfnEvent )
	{
		Con_Printf( "CL_HookEvent:  Must provide an event hook callback\n" );
		return;
	}

	hook = CL_FindEventHook( name );
	if ( hook )
	{
		Con_DPrintf( "CL_HookEvent:  Called on existing hook, updating event hook\n" );
		hook->pfnEvent = pfnEvent;
		return;
	}

	hook = (event_hook_t *)Mem_ZeroMalloc( sizeof( event_hook_t ) );
	hook->name = Mem_Strdup( name );
	hook->pfnEvent = pfnEvent;

	hook->next = g_pEventHooks;
	g_pEventHooks = hook;
}

/*
=============
CL_InitEventSystem

=============
*/
void CL_InitEventSystem( void )
{
	g_pEventHooks = NULL;
}

/*
=============
CL_FlushEventSystem

=============
*/
void CL_FlushEventSystem( void )
{
	event_hook_t	*ev, *next;

	ev = g_pEventHooks;
	while ( ev )
	{
		next = ev->next;
		Mem_Free( ev->name );
		Mem_Free( ev );
		ev = next;
	}

	g_pEventHooks = NULL;
}

/*
=============
CL_ParseFileTxferFailed

=============
*/
void CL_ParseFileTxferFailed( void )
{
	char	*name;

	name = MSG_ReadString();
	if ( !name || !name[0] )
	{
		Host_Error( "CL_ParseFileTxferFailed:  empty filename\n" );
		return;
	}

	if ( cls.demoplayback )
		return;

	CL_ProcessFile( false, name );
}

/*
=============
CL_Send_CvarValue

=============
*/
void CL_Send_CvarValue( void )
{
	char	*name;
	cvar_t	*cvar = NULL;

	name = MSG_ReadString();
	if ( strlen( name ) < 255 )
	{
		cvar = Cvar_FindVar( name );
		if ( cvar )
		{
			MSG_WriteByte( &cls.netchan.message, clc_cvarvalue );
			MSG_WriteString( &cls.netchan.message, cvar->string );
		}
		else
		{
			MSG_WriteByte( &cls.netchan.message, clc_cvarvalue );
			MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
		}
	}
	else
	{
		MSG_WriteByte( &cls.netchan.message, clc_cvarvalue );
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
	}
}

/*
=============
CL_Send_CvarValue2

=============
*/
void CL_Send_CvarValue2( void )
{
	char	*name;
	cvar_t	*cvar = NULL;
	int		requestID = 0;

	requestID = MSG_ReadLong();
	name = MSG_ReadString();
	if ( strlen( name ) < 255 )
	{
		cvar = Cvar_FindVar( name );
		if ( cvar )
		{
			MSG_WriteByte( &cls.netchan.message, clc_cvarvalue2 );
			MSG_WriteLong( &cls.netchan.message, requestID );
			MSG_WriteString( &cls.netchan.message, name );
			MSG_WriteString( &cls.netchan.message, cvar->string );
		}
		else
		{
			MSG_WriteByte( &cls.netchan.message, clc_cvarvalue2 );
			MSG_WriteLong( &cls.netchan.message, requestID );
			MSG_WriteString( &cls.netchan.message, name );
			MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
		}
	}
	else
	{
		MSG_WriteByte( &cls.netchan.message, clc_cvarvalue2 );
		MSG_WriteLong( &cls.netchan.message, requestID );
		MSG_WriteString( &cls.netchan.message, name );
		MSG_WriteString( &cls.netchan.message, "Bad CVAR request" );
	}
}
