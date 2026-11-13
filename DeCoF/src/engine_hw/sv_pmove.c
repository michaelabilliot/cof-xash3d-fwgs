// sv_pmove.c -- server side callbacks of the shared player movement code

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "host.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "pmove.h"
#include "server.h"

void		EV_SV_Playback( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );
const char	*TraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 );

/*
=================
PM_SV_PlaybackEventFull
=================
*/
void PM_SV_PlaybackEventFull( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	EV_SV_Playback( flags | FEV_NOTHOST, clientindex, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2 );
}

/*
=================
PM_SV_PlaySound
=================
*/
void PM_SV_PlaySound( int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	SV_StartSound( 1, host_client->edict, channel, sample, volume * 255, attenuation, fFlags, pitch );
}

/*
=================
PM_SV_TraceTexture
=================
*/
const char *PM_SV_TraceTexture( int ground, vec_t *vstart, vec_t *vend )
{
	physent_t	*pe;
	edict_t		*pent;

	if ( ground < 0 || ground >= pmove->numphysent )
		return NULL;

	pe = &pmove->physents[ground];
	if ( !pe->model )
		return NULL;

	if ( pe->info < 0 || pe->info >= sv.max_edicts )
		return NULL;

	pent = &sv.edicts[pe->info];
	if ( !pent )
		return NULL;

	return TraceTexture( pent, vstart, vend );
}
