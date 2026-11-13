// cl_pred.c -- client side player movement prediction

#include "quakedef.h"
#include <math.h>
#include "mathlib.h"
#include "cvar.h"
#include "host.h"
#include "client.h"
#include "GL_MODEL.H"
#include "pmove.h"
#include "../pm_shared/pm_movevars.h"
#include "cdll_int.h"

cvar_t	cl_showerror = { "cl_showerror", "0" };
cvar_t	cl_nosmooth = { "cl_nosmooth", "0" };
cvar_t	cl_smoothtime = { "cl_smoothtime", "0.1" };

static int	g_lastground = -1;

int			g_i;

static double	cl_correction_time;

/*
==================
CL_CheckPredictionError

==================
*/
void CL_CheckPredictionError( void )
{
	int		frame;
	int		command;
	vec3_t	delta;
	float	len;
	static int	pos = 0;

	// calculate the last usercmd_t we sent that the server has processed
	frame = cl.parsecountmod;
	command = cls.netchan.incoming_acknowledged & CL_UPDATE_MASK;

	// compare what the server returned with what we had predicted it to be
	VectorSubtract( cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[command], delta );

	// save the prediction error for interpolation
	len = Length( delta );
	if ( len > 64 )
	{
		// a teleport or something
		VectorCopy( vec3_origin, cl.prediction_error );
		len = 0;
	}
	else
	{
		if ( cl_showerror.value && len > 0.5 )
		{
			Con_NPrintf( 10 + ( ++pos & 3 ), "pred. error %.3f units", len );
		}

		VectorCopy( cl.frames[frame].playerstate[cl.playernum].origin, cl.predicted_origins[frame] );

		// save for error interpolation
		VectorCopy( delta, cl.prediction_error );
	}

	if ( len > 0.25 && cl.maxclients != 1 )
	{
		cl_correction_time = cl_smoothtime.value;
	}
}

/*
=============
CL_RunUsercmd

=============
*/
void CL_RunUsercmd( local_state_t *from, local_state_t *to, usercmd_t *u, qboolean runfuncs, double *pfElapsed, unsigned int random_seed )
{
	usercmd_t		cmd;
	entity_state_t	*fs, *ts;
	clientdata_t	*fcd, *tcd;

	if ( u->msec > 50.0 )
	{
		local_state_t	temp;
		usercmd_t		split;

		split = *u;
		split.msec /= 2.0;

		CL_RunUsercmd( from, &temp, &split, runfuncs, pfElapsed, random_seed );

		split.impulse = 0;
		split.weaponselect = 0;

		CL_RunUsercmd( &temp, to, &split, runfuncs, pfElapsed, random_seed );
		return;
	}

	cmd = *u;

	pmove->time = 1000.0 * *pfElapsed;
	pmove->server = 0;
	pmove->multiplayer = ( cl.maxclients > 1 ) ? true : false;
	pmove->runfuncs = runfuncs;
	pmove->frametime = cmd.msec / 1000.0;

	*to = *from;

	fs = &from->playerstate;
	ts = &to->playerstate;
	fcd = &from->client;
	tcd = &to->client;

	pmove->player_index = fs->number - 1;

	pmove->flags = fcd->flags;
	pmove->bInDuck = fcd->bInDuck;
	pmove->flTimeStepSound = fcd->flTimeStepSound;
	pmove->flDuckTime = fcd->flDuckTime;
	pmove->flSwimTime = fcd->flSwimTime;
	pmove->waterjumptime = fcd->waterjumptime;
	pmove->waterlevel = fcd->waterlevel;
	pmove->watertype = fcd->watertype;
	pmove->onground = fcd->flags & FL_ONGROUND;
	pmove->deadflag = fcd->deadflag;

	VectorCopy( fcd->velocity, pmove->velocity );
	VectorCopy( fcd->view_ofs, pmove->view_ofs );
	VectorCopy( fs->origin, pmove->origin );
	VectorCopy( fs->basevelocity, pmove->basevelocity );
	VectorCopy( fcd->punchangle, pmove->punchangle );
	VectorCopy( fs->angles, pmove->angles );
	VectorCopy( fs->angles, pmove->oldangles );

	pmove->friction = fs->friction;
	pmove->usehull = fs->usehull;
	pmove->oldbuttons = fs->oldbuttons;
	pmove->dead = ( cl.stats[STAT_HEALTH] <= 0 ) ? true : false;
	pmove->spectator = ( cls.spectator != 0 );
	pmove->movetype = fs->movetype;
	pmove->gravity = fs->gravity;
	pmove->effects = fs->effects;
	pmove->iStepLeft = fs->iStepLeft;
	pmove->flFallVelocity = fs->flFallVelocity;

	Q_strncpy( pmove->physinfo, cls.physinfo, MAX_PHYSINFO_STRING - 1 );
	pmove->physinfo[MAX_PHYSINFO_STRING - 1] = 0;

	pmove->maxspeed = movevars.maxspeed;
	pmove->clientmaxspeed = fcd->maxspeed;

	pmove->cmd = cmd;

	pmove->iuser1 = fcd->iuser1;
	pmove->iuser2 = fcd->iuser2;
	pmove->iuser3 = fcd->iuser3;
	pmove->iuser4 = fcd->iuser4;
	pmove->fuser1 = fcd->fuser1;
	pmove->fuser2 = fcd->fuser2;
	pmove->fuser3 = fcd->fuser3;
	pmove->fuser4 = fcd->fuser4;
	VectorCopy( fcd->vuser1, pmove->vuser1 );
	VectorCopy( fcd->vuser2, pmove->vuser2 );
	VectorCopy( fcd->vuser3, pmove->vuser3 );
	VectorCopy( fcd->vuser4, pmove->vuser4 );

	pmove->PM_PlaySound = PM_CL_PlaySound;
	pmove->PM_TraceTexture = PM_CL_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_CL_PlaybackEventFull;

	ClientDLL_MoveClient( pmove );

	g_lastground = pmove->onground;
	if ( g_lastground > 0 )
		g_lastground = pmove->physents[g_lastground].info;

	tcd->flags = pmove->flags;
	tcd->bInDuck = pmove->bInDuck;
	tcd->flTimeStepSound = pmove->flTimeStepSound;
	tcd->flDuckTime = pmove->flDuckTime;
	tcd->flSwimTime = pmove->flSwimTime;
	tcd->waterjumptime = pmove->waterjumptime;
	tcd->waterlevel = pmove->waterlevel;
	tcd->watertype = pmove->watertype;
	tcd->maxspeed = pmove->clientmaxspeed;
	tcd->deadflag = pmove->deadflag;

	VectorCopy( pmove->velocity, tcd->velocity );
	VectorCopy( pmove->view_ofs, tcd->view_ofs );
	VectorCopy( pmove->origin, ts->origin );
	VectorCopy( pmove->basevelocity, ts->basevelocity );
	VectorCopy( pmove->punchangle, tcd->punchangle );

	ts->oldbuttons = pmove->cmd.buttons;
	ts->friction = pmove->friction;
	ts->movetype = pmove->movetype;
	ts->effects = pmove->effects;
	ts->usehull = pmove->usehull;
	ts->iStepLeft = pmove->iStepLeft;
	ts->flFallVelocity = pmove->flFallVelocity;

	tcd->iuser1 = pmove->iuser1;
	tcd->iuser2 = pmove->iuser2;
	tcd->iuser3 = pmove->iuser3;
	tcd->iuser4 = pmove->iuser4;
	tcd->fuser1 = pmove->fuser1;
	tcd->fuser2 = pmove->fuser2;
	tcd->fuser3 = pmove->fuser3;
	tcd->fuser4 = pmove->fuser4;
	VectorCopy( pmove->vuser1, tcd->vuser1 );
	VectorCopy( pmove->vuser2, tcd->vuser2 );
	VectorCopy( pmove->vuser3, tcd->vuser3 );
	VectorCopy( pmove->vuser4, tcd->vuser4 );

	ClientDLL_PostRunCmd( from, to, &cmd, runfuncs, *pfElapsed, random_seed );

	*pfElapsed += cmd.msec / 1000.0;
}

/*
===============
CL_SetIdealPitch

===============
*/
#define	MAX_FORWARD	6
void CL_SetIdealPitch( void )
{
	float		angleval, sinval, cosval;
	vec3_t		top, bottom;
	float		z[MAX_FORWARD];
	int			i, j;
	int			step, dir, steps;
	pmtrace_t	tr;

	if ( cl.onground == -1 )
		return;

	angleval = cl.simangles[YAW] * M_PI * 2 / 360;
	sinval = sin( angleval );
	cosval = cos( angleval );

	for ( i = 0; i < MAX_FORWARD; i++ )
	{
		top[0] = cl.simorg[0] + cosval * ( i + 3 ) * 12;
		top[1] = cl.simorg[1] + sinval * ( i + 3 ) * 12;
		top[2] = cl.simorg[2] + cl.viewheight[2];

		bottom[0] = top[0];
		bottom[1] = top[1];
		bottom[2] = top[2] - 160;

		tr = PM_PlayerTrace( top, bottom, PM_STUDIO_BOX, -1 );
		if ( tr.allsolid )
			return;	// looking at a wall, leave ideal the way is was

		if ( tr.fraction == 1 )
			return;	// near a dropoff

		z[i] = top[2] + tr.fraction * ( bottom[2] - top[2] );
	}

	dir = 0;
	steps = 0;
	for ( j = 1; j < i; j++ )
	{
		step = z[j] - z[j - 1];
		if ( step > -ON_EPSILON && step < ON_EPSILON )
			continue;

		if ( dir && ( step - dir > ON_EPSILON || step - dir < -ON_EPSILON ) )
			return;		// mixed changes

		steps++;
		dir = step;
	}

	if ( !dir )
	{
		cl.idealpitch = 0;
		return;
	}

	if ( steps < 2 )
		return;
	cl.idealpitch = -dir * cl_idealpitchscale.value;
}

/*
==============
CL_InterpolateVector

==============
*/
void CL_InterpolateVector( float frac, vec_t *src, vec_t *dest, vec_t *output )
{
	int		i;

	for ( i = 0; i < 3; i++ )
	{
		output[i] = src[i] + frac * ( dest[i] - src[i] );
	}
}

/*
==============
CL_GetTargetTime

==============
*/
void CL_GetTargetTime( float *target )
{
	*target = realtime;
}

/*
==============
CL_PredictMove

==============
*/
void CL_PredictMove( qboolean repredicting )
{
	int				i;
	float			f;
	cmd_t			*fcmd;
	cmd_t			*tcmd = NULL;
	float			targettime;
	frame_t			*fromframe;
	local_state_t	*from;
	local_state_t	*to = NULL;
	entity_state_t	*ts, *fs;
	clientdata_t	*fcd, *tcd;
	double			dTime;
	int				stoppoint;
	static vec3_t	lastsimorg;

	if ( cls.state != ca_active || cls.spectator )
		return;

	pmove = &g_clmove;

	CL_SetUpPlayerPrediction( false, false );

	CL_GetTargetTime( &targettime );

	if ( cl.intermission || !cl.validsequence || ( cls.netchan.outgoing_sequence + 1 - cls.netchan.incoming_acknowledged ) >= CL_UPDATE_MASK )
		return;

	VectorCopy( cl.viewangles, cl.simangles );

	if ( cls.demoplayback )
		return;

	fromframe = &cl.frames[cl.parsecountmod];

	// this is the last frame received from the server
	cl.predicted_frames[cl.parsecountmod].playerstate = fromframe->playerstate[cl.playernum];
	cl.predicted_frames[cl.parsecountmod].client = fromframe->clientdata;
	Q_memcpy( cl.predicted_frames[cl.parsecountmod].weapondata, fromframe->weapondata, sizeof( fromframe->weapondata ) );

	from = &cl.predicted_frames[cl.parsecountmod];

	// we can now render a frame
	if ( fromframe->invalid )
		return;

	dTime = fromframe->time;

	fcmd = &cl.commands[cls.netchan.incoming_acknowledged & CL_UPDATE_MASK];

	cl.onground = -1;

	// Set up prediction for other players
	CL_PushPMStates();
	CL_SetSolidPlayers( cl.playernum );

	i = 1;
	stoppoint = 1;
	if ( repredicting )
		stoppoint = 0;

	while ( 1 )
	{
		g_i = i;

		if ( i >= CL_UPDATE_BACKUP - 1 )
			break;

		if ( cls.netchan.incoming_acknowledged + i >= cls.netchan.outgoing_sequence + stoppoint )
			break;

		tcmd = &cl.commands[( cls.netchan.incoming_acknowledged + i ) & CL_UPDATE_MASK];
		to = &cl.predicted_frames[( cl.parsecountmod + i ) & CL_UPDATE_MASK];

		CL_RunUsercmd( from, to, &tcmd->cmd, !repredicting && !tcmd->processedfuncs, &dTime, cls.netchan.incoming_acknowledged + i );

		tcmd->processedfuncs = true;

		VectorCopy( to->playerstate.origin, cl.predicted_origins[( cls.netchan.incoming_acknowledged + i ) & CL_UPDATE_MASK] );

		if ( tcmd->senttime >= targettime )
			break;

		from = to;
		fcmd = tcmd;

		i++;
	}

	CL_PopPMStates();

	if ( i >= CL_UPDATE_MASK )
		return;

	if ( !to )
	{
		if ( repredicting )
		{
			to = from;
			tcmd = fcmd;
		}
		else
			return;
	}

	if ( tcmd->senttime == fcmd->senttime )
	{
		f = 0;
	}
	else
	{
		f = max( 0.0, ( targettime - fcmd->senttime ) / ( tcmd->senttime - fcmd->senttime ) );
	}

	ts = &to->playerstate;
	fs = &from->playerstate;
	fcd = &from->client;
	tcd = &to->client;

	for ( i = 0; i < 3; i++ )
	{
		if ( fabs( ts->origin[i] - fs->origin[i] ) > 128 )
			break;
	}

	if ( i != 3 )
	{
		// teleported, so don't lerp
		VectorCopy( tcd->velocity, cl.simvel );
		VectorCopy( ts->origin, cl.simorg );
		VectorCopy( tcd->punchangle, cl.punchangle );
		VectorCopy( tcd->view_ofs, cl.viewheight );
	}
	else
	{
		CL_InterpolateVector( f, fs->origin, ts->origin, cl.simorg );
		CL_InterpolateVector( f, fcd->velocity, tcd->velocity, cl.simvel );
		CL_InterpolateVector( f, fcd->punchangle, tcd->punchangle, cl.punchangle );

		if ( fs->usehull == ts->usehull )
		{
			CL_InterpolateVector( f, fcd->view_ofs, tcd->view_ofs, cl.viewheight );
		}
		else
		{
			VectorCopy( tcd->view_ofs, cl.viewheight );
		}
	}

	cl.onground = ( tcd->flags & FL_ONGROUND ) ? g_lastground : -1;
	cl.waterlevel = tcd->waterlevel;
	cl.usehull = ts->usehull;
	cl.stats[STAT_WEAPON] = tcd->viewmodel;
	cl.moving = 0;

	if ( cl.onground > 0 && cl.onground < cl.num_entities )
	{
		vec3_t	delta;

		VectorSubtract( cl_entities[cl.onground].curstate.origin, cl_entities[cl.onground].prevstate.origin, delta );
		delta[2] = 0.0;

		if ( Length( delta ) > 0.0 )
		{
			cl.moving = 1;
		}
	}

	if ( cl.moving )
	{
		cl_correction_time = 0.0;
	}

	if ( cl_correction_time > 0.0 && !cl_nosmooth.value && cl_smoothtime.value )
	{
		float	frac;
		vec3_t	delta;

		if ( !repredicting )
		{
			cl_correction_time -= host_frametime;
		}

		if ( cl_smoothtime.value <= 0.0 )
		{
			Cvar_DirectSet( &cl_smoothtime, "0.1" );
		}

		cl_correction_time = max( 0.0, cl_correction_time );
		cl_correction_time = min( cl_correction_time, cl_smoothtime.value );

		frac = cl_correction_time / cl_smoothtime.value;
		frac = 1.0 - frac;

		VectorSubtract( cl.simorg, lastsimorg, delta );
		VectorScale( delta, frac, delta );
		VectorAdd( lastsimorg, delta, cl.simorg );
	}

	VectorCopy( cl.simorg, lastsimorg );

	CL_SetIdealPitch();
}

static int	oldphysent;
static int	oldvisent;
static int	pushed;

/*
==============
CL_PushPMStates

==============
*/
void CL_PushPMStates( void )
{
	if ( pushed )
	{
		Con_Printf( "CL_PushPMStates called with pushed stack\n" );
	}
	else
	{
		oldphysent = pmove->numphysent;
		oldvisent = pmove->numvisent;
		pushed++;
	}
}

/*
==============
CL_PopPMStates

==============
*/
void CL_PopPMStates( void )
{
	if ( !pushed )
	{
		Con_Printf( "CL_PopPMStates called without stack\n" );
	}
	else
	{
		pmove->numphysent = oldphysent;
		pmove->numvisent = oldvisent;
		pushed--;
	}
}

static int	g_lastupdate_sequence;

/*
==============
CL_SetLastUpdate

==============
*/
void CL_SetLastUpdate( void )
{
	g_lastupdate_sequence = cls.netchan.incoming_sequence;
}

/*
==============
CL_RedoPrediction

==============
*/
void CL_RedoPrediction( void )
{
	if ( cls.netchan.incoming_sequence == g_lastupdate_sequence )
		return;

	CL_PredictMove( true );
	CL_CheckPredictionError();
}

/*
==============
CL_InitPrediction

==============
*/
void CL_InitPrediction( void )
{
	Cvar_RegisterVariable( &cl_smoothtime );
	Cvar_RegisterVariable( &cl_nosmooth );
	Cvar_RegisterVariable( &cl_showerror );
}
