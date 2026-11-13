// eventapi.c -- event services exported to the client dll

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "pmove.h"
#include "sound.h"
#include "progs.h"
#include "event.h"
#include "event_api.h"

struct sfx_s	*CL_LookupSound( const char *pName );

/*
=================
EV_PlaySound
=================
*/
void EV_PlaySound( int ent, float *origin, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	sfx_t	*sfx;
	sfx_t	sfxsentence;

	if ( fFlags & SND_SENTENCE )
	{
		sfx = &sfxsentence;
		_snprintf( sfx->name, sizeof( sfx->name ), "!%s", rgpszrawsentence[Q_atoi( sample + 1 )] );
	}
	else
	{
		sfx = CL_LookupSound( sample );
	}

	if ( channel == CHAN_STATIC )
		S_StartStaticSound( ent, CHAN_STATIC, sfx, origin, volume, attenuation, fFlags, pitch );
	else
		S_StartDynamicSound( ent, channel, sfx, origin, volume, attenuation, fFlags, pitch );
}

/*
=================
EV_StopSound
=================
*/
void EV_StopSound( int ent, int channel, const char *sample )
{
	EV_PlaySound( ent, vec3_origin, channel, sample, 0, 0, SND_STOP, PITCH_NORM );
}

/*
=================
EV_FindModelIndex
=================
*/
int EV_FindModelIndex( const char *pmodel )
{
	int		i;

	for ( i = 1; i < cl.model_precache_count; i++ )
	{
		if ( !cl.model_precache[i] )
			continue;

		if ( !Q_stricmp( pmodel, cl.model_precache[i]->name ) )
			return i;
	}

	return 0;
}

/*
=================
EV_LocalPlayerDucking
=================
*/
int EV_LocalPlayerDucking( void )
{
	return cl.usehull == 1;
}

/*
=================
EV_LocalPlayerViewheight
=================
*/
void EV_LocalPlayerViewheight( float *viewheight )
{
	VectorCopy( cl.viewheight, viewheight );
}

/*
=================
EV_LocalPlayerBounds
=================
*/
void EV_LocalPlayerBounds( int hull, float *mins, float *maxs )
{
	if ( mins )
	{
		VectorCopy( player_mins[hull], mins );
	}

	if ( maxs )
	{
		VectorCopy( player_maxs[hull], maxs );
	}
}

/*
=================
EV_IndexFromTrace
=================
*/
int EV_IndexFromTrace( struct pmtrace_s *pTrace )
{
	if ( !pTrace )
		return 0;

	return pmove->physents[pTrace->ent].info;
}

/*
=================
EV_GetPhysent
=================
*/
struct physent_s *EV_GetPhysent( int idx )
{
	if ( idx < 0 || idx >= pmove->numphysent )
		return NULL;

	return &pmove->physents[idx];
}

/*
=================
EV_SetUpPlayerPrediction
=================
*/
void EV_SetUpPlayerPrediction( int dopred, int bIncludeLocalClient )
{
	CL_SetUpPlayerPrediction( dopred, bIncludeLocalClient );
}

/*
=================
EV_SetTraceHull
=================
*/
void EV_SetTraceHull( int hull )
{
	pmove->usehull = hull;
}

/*
=================
EV_PlayerTrace
=================
*/
void EV_PlayerTrace( float *start, float *end, int traceFlags, int ignore_pe, struct pmtrace_s *tr )
{
	*tr = PM_PlayerTrace( start, end, traceFlags, ignore_pe );
}

/*
=================
EV_IsLocal
=================
*/
int EV_IsLocal( int playernum )
{
	return playernum == cl.playernum;
}

/*
=================
EV_WeaponAnimation
=================
*/
void EV_WeaponAnimation( int sequence, int body )
{
	cl.weaponsequence = sequence;
	cl.weaponstarttime = 0;
	cl.viewent.curstate.body = body;
}

/*
=================
EV_PrecacheEvent
=================
*/
unsigned short EV_PrecacheEvent( int type, const char *psz )
{
	int		i;

	for ( i = 1; MAX_EVENTS; i++ )
	{
		if ( !cl.event_precache[i].filename )
			break;

		if ( !Q_stricmp( cl.event_precache[i].filename, psz ) )
			return i;
	}

	return 0;
}

/*
=================
EV_PlaybackEvent
=================
*/
void EV_PlaybackEvent( int flags, const struct edict_s *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t	eargs;

	if ( flags & FEV_SERVER )
		return;

	Q_memset( &eargs, 0, sizeof( eargs ) );

	VectorCopy( origin, eargs.origin );
	VectorCopy( angles, eargs.angles );

	eargs.fparam1 = fparam1;
	eargs.fparam2 = fparam2;
	eargs.iparam1 = iparam1;
	eargs.iparam2 = iparam2;
	eargs.bparam1 = bparam1;
	eargs.bparam2 = bparam2;

	eargs.entindex = flags;	// the retail build stores the flags here

	CL_QueueEvent( flags, eventindex, delay, &eargs );
}

/*
=================
EV_KillEvents
=================
*/
void EV_KillEvents( int entnum, const char *eventname )
{
	int				i;
	event_state_t	*es;
	event_info_t	*ei;

	es = &cl.events;
	for ( i = 0; i < MAX_EVENT_QUEUE; i++ )
	{
		ei = &es->ei[i];
		if ( !ei->index )
			continue;

		if ( ei->args.entindex != entnum )
			continue;

		if ( Q_stricmp( eventname, cl.event_precache[ei->index].filename ) )
			continue;

		CL_ResetEvent( ei );
	}
}

event_api_t eventapi =
{
	EVENT_API_VERSION,
	EV_PlaySound,
	EV_StopSound,
	EV_FindModelIndex,
	EV_IsLocal,
	EV_LocalPlayerDucking,
	EV_LocalPlayerViewheight,
	EV_LocalPlayerBounds,
	EV_IndexFromTrace,
	EV_GetPhysent,
	EV_SetUpPlayerPrediction,
	CL_PushPMStates,
	CL_PopPMStates,
	CL_SetSolidPlayers,
	EV_SetTraceHull,
	EV_PlayerTrace,
	EV_WeaponAnimation,
	EV_PrecacheEvent,
	EV_PlaybackEvent,
	PM_CL_TraceTexture,
	S_StopSound,
	EV_KillEvents
};
