// cl_pmove.c -- client side callbacks of the shared player movement code

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "pmove.h"
#include "sound.h"
#include "event.h"

struct sfx_s	*CL_LookupSound( const char *pName );
void		CL_DemoQueueSound( int channel, const char *sample, float volume, float attenuation, int flags, int pitch );
msurface_t	*SurfaceAtPoint( model_t *pModel, mnode_t *node, vec_t *start, vec_t *end );

/*
=================
PM_CL_PlaybackEventFull
=================
*/
void PM_CL_PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
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

	CL_ClientQueueEvent( flags, eventindex, clientindex, delay, &eargs );
}

/*
=================
PM_CL_TraceTexture
=================
*/
const char *PM_CL_TraceTexture( int ground, float *vstart, float *vend )
{
	physent_t	*pe;
	msurface_t	*psurf;
	int			firstnode;
	vec3_t		start, end;

	firstnode = 0;

	if ( ground < 0 || ground >= pmove->numphysent )
		return NULL;

	pe = &pmove->physents[ground];
	if ( !pe->model )
		return NULL;

	if ( ground >= 1 )
	{
		vec3_t	offset;
		hull_t	*phull;

		phull = &pe->model->hulls[0];

		VectorCopy( phull->clip_mins, offset );
		VectorAdd( offset, pe->origin, offset );

		VectorSubtract( vstart, offset, start );
		VectorSubtract( vend, offset, end );

		firstnode = phull->firstclipnode;

		// rotate start and end into the models frame of reference
		if ( pe->angles[0] || pe->angles[1] || pe->angles[2] )
		{
			vec3_t	forward, right, up;
			vec3_t	temp;

			AngleVectors( pe->angles, forward, right, up );

			VectorCopy( start, temp );
			start[0] = DotProduct( temp, forward );
			start[1] = -DotProduct( temp, right );
			start[2] = DotProduct( temp, up );

			VectorCopy( end, temp );
			end[0] = DotProduct( temp, forward );
			end[1] = -DotProduct( temp, right );
			end[2] = DotProduct( temp, up );
		}
	}
	else
	{
		VectorCopy( vstart, start );
		VectorCopy( vend, end );
	}

	if ( pe->model && pe->model->type == mod_brush && pe->model->nodes )
	{
		psurf = SurfaceAtPoint( pe->model, &pe->model->nodes[firstnode], start, end );
		if ( psurf )
			return psurf->texinfo->texture->name;
	}

	return NULL;
}

float	g_laststepgap;

/*
=================
PM_CL_PlaySound
=================
*/
void PM_CL_PlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	sfx_t	*sfx;
	sfx_t	sfxsentence;

	if ( !pmove->runfuncs )
		return;

	{
		static float last;

		g_laststepgap = cl.time - last;
		last = cl.time;
	}

	if ( fFlags & SND_SENTENCE )
	{
		sfx = &sfxsentence;
		_snprintf( sfx->name, sizeof( sfx->name ), "!%s", rgpszrawsentence[Q_atoi( sample + 1 )] );
	}
	else
	{
		sfx = CL_LookupSound( sample );
	}

	if ( cls.demorecording )
		CL_DemoQueueSound( channel, sample, volume, attenuation, fFlags, pitch );

	if ( channel == CHAN_STATIC )
		S_StartStaticSound( pmove->player_index + 1, CHAN_STATIC, sfx, pmove->origin, volume, attenuation, fFlags, pitch );
	else
		S_StartDynamicSound( pmove->player_index + 1, channel, sfx, pmove->origin, volume, attenuation, fFlags, pitch );
}
