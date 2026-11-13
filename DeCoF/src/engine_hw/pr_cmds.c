// pr_cmds.c -- the engine functions exported to the game dll

#include <math.h>
#include <time.h>
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "info.h"
#include "host.h"
#include "net.h"
#include "protocol.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "pmove.h"
#include "sound.h"
#include "server.h"
#include "world.h"
#include "client.h"
#include "decal.h"
#include "usermsg.h"
#include "inst_baseline.h"
#include "pr_dlls.h"
#include "APIProxy.h"
#include "../common/event_flags.h"

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/

void PF_makevectors_I( const float *rgflVector )
{
	AngleVectors( rgflVector, gGlobalVariables.v_forward, gGlobalVariables.v_right, gGlobalVariables.v_up );
}

float PF_Time( void )
{
	return (float)Sys_FloatTime();
}

/*
=================
PF_setorigin_I

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.
=================
*/
void PF_setorigin_I( edict_t *e, const float *org )
{
	e->v.origin[0] = org[0];
	e->v.origin[1] = org[1];
	e->v.origin[2] = org[2];

	SV_LinkEdict( e, FALSE );
}

void SetMinMaxSize( edict_t *e, const float *min, const float *max, qboolean rotate )
{
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;

	for ( i = 0; i < 3; i++ )
		if ( min[i] > max[i] )
			Host_Error( "backwards mins/maxs" );

	rotate = FALSE;		// FIXME: implement rotation properly again

	if ( !rotate )
	{
		VectorCopy( min, rmin );
		VectorCopy( max, rmax );
	}
	else
	{
	// find min / max for rotations
		angles = e->v.angles;

		a = angles[1] / 180 * M_PI;

		xvector[0] = cos( a );
		xvector[1] = sin( a );
		yvector[0] = -sin( a );
		yvector[1] = cos( a );

		VectorCopy( min, bounds[0] );
		VectorCopy( max, bounds[1] );

		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;

		for ( i = 0; i <= 1; i++ )
		{
			base[0] = bounds[i][0];
			for ( j = 0; j <= 1; j++ )
			{
				base[1] = bounds[j][1];
				for ( k = 0; k <= 1; k++ )
				{
					base[2] = bounds[k][2];

				// transform the point
					transformed[0] = xvector[0] * base[0] + yvector[0] * base[1];
					transformed[1] = xvector[1] * base[0] + yvector[1] * base[1];
					transformed[2] = base[2];

					for ( l = 0; l < 3; l++ )
					{
						if ( transformed[l] < rmin[l] )
							rmin[l] = transformed[l];
						if ( transformed[l] > rmax[l] )
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

// set derived values
	VectorCopy( rmin, e->v.mins );
	VectorCopy( rmax, e->v.maxs );
	VectorSubtract( max, min, e->v.size );

	SV_LinkEdict( e, FALSE );
}

void PF_setsize_I( edict_t *e, const float *rgflMin, const float *rgflMax )
{
	SetMinMaxSize( e, rgflMin, rgflMax, FALSE );
}

char *SzFromIndex_guess( int iString )
{
	char	*pszBase_guess = "";

	return pszBase_guess + iString;
}

int IndexFromSz_guess( char *psz )
{
	return psz - SzFromIndex_guess( 0 );
}

void PF_setmodel_I( edict_t *e, const char *m )
{
	int		i;
	char	**check;
	model_t	*mod;

// check to see if model was properly precached
	for ( i = 0, check = sv.model_precache; *check; i++, check++ )
		if ( !Q_stricmp( *check, m ) )
			break;

	if ( !*check )
	{
		Con_Printf( "Warning! No precache on model %s\n", m );
		PF_setmodel_I( e, "models/error.mdl" );
		return;
	}

	e->v.model = m - pr_strings;
	e->v.modelindex = i;

	mod = sv.models[e->v.modelindex];

	if ( mod )
		SetMinMaxSize( e, mod->mins, mod->maxs, TRUE );
	else
		SetMinMaxSize( e, vec3_origin, vec3_origin, TRUE );
}

int PF_modelindex( const char *pstr )
{
	return SV_ModelIndex( pstr );
}

int ModelFrames( int modelIndex )
{
	model_t	*pModel;

	if ( modelIndex > 0 && modelIndex < MAX_MODELS )
	{
		pModel = sv.models[modelIndex];
		return ModelFrameCount( pModel );
	}

	Con_DPrintf( "Bad sprite index!\n" );
	return 1;
}

/*
=================
PF_bprint

broadcast print to everyone on server
=================
*/
void PF_bprint( char *s )
{
	SV_BroadcastPrintf( "%s", s );
}

/*
=================
PF_sprint

single print to a specific client
=================
*/
void PF_sprint( char *s, int entnum )
{
	client_t	*client;

	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to sprint to a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	if ( client->fakeclient )
		return;

	MSG_WriteChar( &client->netchan.message, svc_print );
	MSG_WriteString( &client->netchan.message, s );
}

void ServerPrint( const char *szMsg )
{
	Con_Printf( "%s", szMsg );
}

void ClientPrintf( edict_t *pEdict, PRINT_TYPE ptype, const char *szMsg )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( pEdict );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to sprint to a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	if ( client->fakeclient )
		return;

	switch ( ptype )
	{
	case print_center:
		MSG_WriteChar( &client->netchan.message, svc_centerprint );
		MSG_WriteString( &client->netchan.message, szMsg );
		break;
	case print_console:
	case print_chat:
		MSG_WriteByte( &client->netchan.message, svc_print );
		MSG_WriteString( &client->netchan.message, szMsg );
		break;
	default:
		Con_Printf( "invalid PRINT_TYPE %i\n", ptype );
		break;
	}
}

float PF_vectoyaw_I( const float *rgflVector )
{
	float	yaw;

	if ( rgflVector[1] == 0 && rgflVector[0] == 0 )
		yaw = 0;
	else
	{
		yaw = (int)( atan2( rgflVector[1], rgflVector[0] ) * 180 / M_PI );
		if ( yaw < 0 )
			yaw += 360;
	}

	return yaw;
}

void PF_vectoangles_I( const float *rgflVectorIn, float *rgflVectorOut )
{
	VectorAngles( rgflVectorIn, rgflVectorOut );
}

void PF_particle_I( const float *org, const float *dir, float color, float count )
{
	SV_StartParticle( org, dir, color, count );
}

void PF_ambientsound_I( edict_t *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	char		**check;
	int			i, soundnum;
	int			ent;
	sizebuf_t	*pout;

	if ( samp[0] == '!' )
	{
		fFlags |= SND_SENTENCE;
		soundnum = Q_atoi( samp + 1 );
		if ( soundnum >= CVOXFILESENTENCEMAX )
		{
			Con_Printf( "invalid sentence number: %s", samp + 1 );
			return;
		}
	}
	else
	{
	// check to see if samp was properly precached
		for ( soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++ )
			if ( !Q_stricmp( *check, samp ) )
				break;

		if ( !*check )
		{
			Con_Printf( "no precache: %s\n", samp );
			return;
		}
	}

	ent = NUM_FOR_EDICT( entity );

	if ( fFlags & SND_SPAWNING )
		pout = &sv.signon;
	else
		pout = &sv.datagram;

// add an svc_spawnambient command to the level signon packet
	MSG_WriteByte( pout, svc_spawnstaticsound );
	for ( i = 0; i < 3; i++ )
		MSG_WriteCoord( pout, pos[i] );

	MSG_WriteShort( pout, soundnum );
	MSG_WriteByte( pout, vol * 255 );
	MSG_WriteByte( pout, attenuation * 64 );
	MSG_WriteShort( pout, ent );
	MSG_WriteByte( pout, pitch );
	MSG_WriteByte( pout, fFlags & 0xFF );
}

/*
=================
PF_sound_I

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.
=================
*/
void PF_sound_I( edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch )
{
	if ( volume < 0 || volume > 255 )
		Sys_Error( "EMIT_SOUND: volume = %i", volume );

	if ( attenuation < 0 || attenuation > 4 )
		Sys_Error( "EMIT_SOUND: attenuation = %f", attenuation );

	if ( channel < 0 || channel > 7 )
		Sys_Error( "EMIT_SOUND: channel = %i", channel );

	if ( pitch < 0 || pitch > 255 )
		Sys_Error( "EMIT_SOUND: pitch = %i", pitch );

	SV_StartSound( 0, entity, channel, sample, volume * 255, attenuation, fFlags, pitch );
}

/*
=================
PF_traceline_Shared

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.
=================
*/
void PF_traceline_Shared( const float *v1, const float *v2, int nomonsters, edict_t *ent )
{
	trace_t	trace;

	trace = SV_Move( v1, vec3_origin, vec3_origin, v2, nomonsters, ent, FALSE );

	gGlobalVariables.trace_flags = 0;
	SV_SetGlobalTrace( &trace );
}

void PF_traceline_DLL( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	PF_traceline_Shared( v1, v2, fNoMonsters, pentToSkip ? pentToSkip : sv.edicts );

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;
	VectorCopy( gGlobalVariables.trace_endpos, ptr->vecEndPos );
	VectorCopy( gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

static vec3_t gHullMins[4] =
{
	{ 0, 0, 0 },
	{ -16, -16, -36 },
	{ -32, -32, -32 },
	{ -16, -16, -18 },
};

static vec3_t gHullMaxs[4] =
{
	{ 0, 0, 0 },
	{ 16, 16, 36 },
	{ 32, 32, 32 },
	{ 16, 16, 18 },
};

void TraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	trace;

	if ( hullNumber < 0 || hullNumber > 3 )
		hullNumber = 0;

	trace = SV_Move( v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2, fNoMonsters, pentToSkip, FALSE );

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	ptr->iHitgroup = trace.hitgroup;
	VectorCopy( trace.endpos, ptr->vecEndPos );
	VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
}

void TraceSphere( const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr )
{
	Sys_Error( "TraceSphere not yet implemented!\n" );
}

void TraceModel( const float *v1, const float *v2, int hullNumber, edict_t *pent, TraceResult *ptr )
{
	trace_t	trace;
	int		oldSolid, oldMovetype;
	model_t	*pmodel;

	if ( hullNumber < 0 || hullNumber > 3 )
		hullNumber = 0;

	pmodel = sv.models[pent->v.modelindex];
	if ( pmodel && pmodel->type == mod_brush )
	{
		// Store off the old solid and movetype
		oldSolid = pent->v.solid;
		oldMovetype = pent->v.movetype;
		pent->v.solid = SOLID_BSP;
		pent->v.movetype = MOVETYPE_PUSH;
	}

	trace = SV_ClipMoveToEntity( pent, v1, gHullMins[hullNumber], gHullMaxs[hullNumber], v2 );

	if ( pmodel && pmodel->type == mod_brush )
	{
		pent->v.solid = oldSolid;
		pent->v.movetype = oldMovetype;
	}

	ptr->fAllSolid = trace.allsolid;
	ptr->fStartSolid = trace.startsolid;
	ptr->fInOpen = trace.inopen;
	ptr->fInWater = trace.inwater;
	ptr->flFraction = trace.fraction;
	ptr->flPlaneDist = trace.plane.dist;
	ptr->pHit = trace.ent;
	ptr->iHitgroup = trace.hitgroup;
	VectorCopy( trace.endpos, ptr->vecEndPos );
	VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
}

msurface_t *SurfaceAtPoint( model_t *pModel, mnode_t *node, vec_t *start, vec_t *end )
{
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;

	if ( node->contents < 0 )
		return NULL;

	plane = node->plane;
	front = DotProduct( start, plane->normal ) - plane->dist;
	back = DotProduct( end, plane->normal ) - plane->dist;
	side = front < 0;

	if ( ( back < 0 ) == side )
	{
		surf = SurfaceAtPoint( pModel, node->children[side], start, end );
		return surf;
	}

	frac = front / ( front - back );
	mid[0] = start[0] + ( end[0] - start[0] ) * frac;
	mid[1] = start[1] + ( end[1] - start[1] ) * frac;
	mid[2] = start[2] + ( end[2] - start[2] ) * frac;

// go down front side
	surf = SurfaceAtPoint( pModel, node->children[side], start, mid );
	if ( surf )
		return surf;

	if ( ( back < 0 ) == side )
		return surf;

// check for impact on this node
	surf = pModel->surfaces + node->firstsurface;
	for ( i = 0; i < node->numsurfaces; i++, surf++ )
	{
		tex = surf->texinfo;

		s = DotProduct( mid, tex->vecs[0] ) + tex->vecs[0][3];
		t = DotProduct( mid, tex->vecs[1] ) + tex->vecs[1][3];

		if ( s < surf->texturemins[0] || t < surf->texturemins[1] )
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		return surf;
	}

// go down back side
	surf = SurfaceAtPoint( pModel, node->children[!side], mid, end );
	return surf;
}

const char *TraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	model_t		*pmodel;
	msurface_t	*psurf;
	int			firstnode;
	vec3_t		start, end;

	firstnode = 0;

	if ( pTextureEntity )
	{
		vec3_t	offset;
		hull_t	*phull;

		pmodel = sv.models[pTextureEntity->v.modelindex];
		if ( !pmodel || pmodel->type != mod_brush )
			return NULL;

		phull = SV_HullForBsp( pTextureEntity, vec3_origin, vec3_origin, offset );

		VectorSubtract( v1, offset, start );
		VectorSubtract( v2, offset, end );

		firstnode = phull->firstclipnode;

		// rotate start and end into the models frame of reference
		if ( pTextureEntity->v.angles[0] || pTextureEntity->v.angles[1] || pTextureEntity->v.angles[2] )
		{
			vec3_t	forward, right, up;
			vec3_t	temp;

			AngleVectors( pTextureEntity->v.angles, forward, right, up );

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
		pmodel = sv.worldmodel;
		VectorCopy( v1, start );
		VectorCopy( v2, end );
	}

	if ( pmodel && pmodel->type == mod_brush && pmodel->nodes )
	{
		psurf = SurfaceAtPoint( pmodel, &pmodel->nodes[firstnode], start, end );
		if ( psurf )
			return psurf->texinfo->texture->name;
	}

	return NULL;
}

/*
=================
PF_TraceToss_Shared

=================
*/
void PF_TraceToss_Shared( edict_t *ent, edict_t *ignore )
{
	trace_t	trace;

	trace = SV_Trace_Toss( ent, ignore );
	SV_SetGlobalTrace( &trace );
}

void SV_SetGlobalTrace( trace_t *ptrace )
{
	gGlobalVariables.trace_allsolid = ptrace->allsolid;
	gGlobalVariables.trace_startsolid = ptrace->startsolid;
	gGlobalVariables.trace_fraction = ptrace->fraction;
	gGlobalVariables.trace_inwater = ptrace->inwater;
	gGlobalVariables.trace_inopen = ptrace->inopen;
	VectorCopy( ptrace->endpos, gGlobalVariables.trace_endpos );
	VectorCopy( ptrace->plane.normal, gGlobalVariables.trace_plane_normal );
	gGlobalVariables.trace_plane_dist = ptrace->plane.dist;
	if ( ptrace->ent )
		gGlobalVariables.trace_ent = ptrace->ent;
	else
		gGlobalVariables.trace_ent = sv.edicts;
	gGlobalVariables.trace_hitgroup = ptrace->hitgroup;
}

void PF_TraceToss_DLL( edict_t *pent, edict_t *pentToIgnore, TraceResult *ptr )
{
	PF_TraceToss_Shared( pent, pentToIgnore ? pentToIgnore : sv.edicts );

	ptr->fAllSolid = (int)gGlobalVariables.trace_allsolid;
	ptr->fStartSolid = (int)gGlobalVariables.trace_startsolid;
	ptr->fInOpen = (int)gGlobalVariables.trace_inopen;
	ptr->fInWater = (int)gGlobalVariables.trace_inwater;
	ptr->flFraction = gGlobalVariables.trace_fraction;
	ptr->flPlaneDist = gGlobalVariables.trace_plane_dist;
	ptr->pHit = gGlobalVariables.trace_ent;
	VectorCopy( gGlobalVariables.trace_endpos, ptr->vecEndPos );
	VectorCopy( gGlobalVariables.trace_plane_normal, ptr->vecPlaneNormal );
	ptr->iHitgroup = gGlobalVariables.trace_hitgroup;
}

int TraceMonsterHull( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t		trace;
	qboolean	monsterClip;

	if ( pEdict->v.flags & FL_MONSTERCLIP )
		monsterClip = TRUE;
	else
		monsterClip = FALSE;

	trace = SV_Move( v1, pEdict->v.mins, pEdict->v.maxs, v2, fNoMonsters, pentToSkip, monsterClip );

	if ( ptr )
	{
		ptr->fAllSolid = trace.allsolid;
		ptr->fStartSolid = trace.startsolid;
		ptr->fInOpen = trace.inopen;
		ptr->fInWater = trace.inwater;
		ptr->flFraction = trace.fraction;
		ptr->flPlaneDist = trace.plane.dist;
		ptr->pHit = trace.ent;
		ptr->iHitgroup = trace.hitgroup;
		VectorCopy( trace.endpos, ptr->vecEndPos );
		VectorCopy( trace.plane.normal, ptr->vecPlaneNormal );
	}

	if ( trace.allsolid || trace.fraction != 1.0f )
		return TRUE;

	return FALSE;
}

//============================================================================

byte	checkpvs[1024];

int PF_newcheckclient( int check )
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

// cycle to the next one

	if ( check < 1 )
		check = 1;
	if ( check > svs.maxclients )
		check = svs.maxclients;

	if ( check == svs.maxclients )
		i = 1;
	else
		i = check + 1;

	for ( ; ; i++ )
	{
		if ( i == svs.maxclients + 1 )
			i = 1;

		ent = sv.edicts + i;

		if ( i == check )
			break;	// didn't find anything else

		if ( ent->free || !ent->pvPrivateData )
			continue;
		if ( ent->v.flags & FL_NOTARGET )
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd( ent->v.origin, ent->v.view_ofs, org );
	leaf = Mod_PointInLeaf( org, sv.worldmodel );
	pvs = Mod_LeafPVS( leaf, sv.worldmodel );
	Q_memcpy( checkpvs, pvs, ( sv.worldmodel->numleafs + 7 ) >> 3 );

	return i;
}

/*
=================
PF_checkclient_I

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.
=================
*/
int		c_invis, c_notvis;

edict_t *PF_checkclient_I( edict_t *pEdict )
{
	edict_t	*ent;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;

// find a new check if on a new frame
	if ( sv.time - sv.lastchecktime >= 0.1 )
	{
		sv.lastcheck = PF_newcheckclient( sv.lastcheck );
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible
	ent = sv.edicts + sv.lastcheck;
	if ( ent->free || !ent->pvPrivateData )
		return sv.edicts;

// if current entity can't possibly see the check entity, return 0
	VectorAdd( pEdict->v.origin, pEdict->v.view_ofs, view );
	leaf = Mod_PointInLeaf( view, sv.worldmodel );
	l = ( leaf - sv.worldmodel->leafs ) - 1;
	if ( ( l < 0 ) || !( checkpvs[l >> 3] & ( 1 << ( l & 7 ) ) ) )
	{
		c_notvis++;
		return sv.edicts;
	}

// might be able to see it
	c_invis++;
	return ent;
}

mnode_t *PVSNode( mnode_t *node, vec_t *emins, vec_t *emaxs )
{
	mplane_t	*splitplane;
	int			sides;
	mnode_t		*splitNode;

	if ( node->visframe != r_visframecount )
		return NULL;

	// add an efrag if the node is a leaf
	if ( node->contents < 0 )
	{
		if ( node->contents != CONTENTS_SOLID )
			return node;
		return NULL;
	}

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE( emins, emaxs, splitplane );

	splitNode = NULL;
	if ( sides & 1 )
		splitNode = PVSNode( node->children[0], emins, emaxs );

	if ( !splitNode && ( sides & 2 ) )
		splitNode = PVSNode( node->children[1], emins, emaxs );

	return splitNode;
}

void PVSMark( model_t *pmodel, byte *ppvs )
{
	mnode_t	*node;
	int		i;

	r_visframecount++;
	for ( i = 0; i < pmodel->numleafs; i++ )
	{
		if ( ppvs[i >> 3] & ( 1 << ( i & 7 ) ) )
		{
			node = (mnode_t *)&pmodel->leafs[i + 1];
			do
			{
				if ( node->visframe == r_visframecount )
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while ( node );
		}
	}
}

edict_t *PVSFindEntities( edict_t *pplayer )
{
	int		i;
	edict_t	*pent, *pchain, *pentPVS;
	vec3_t	org;
	byte	*ppvs;
	mleaf_t	*pleaf;

	VectorAdd( pplayer->v.origin, pplayer->v.view_ofs, org );
	pleaf = Mod_PointInLeaf( org, sv.worldmodel );
	ppvs = Mod_LeafPVS( pleaf, sv.worldmodel );
	PVSMark( sv.worldmodel, ppvs );
	pchain = sv.edicts;

	for ( i = 1; i < sv.num_edicts; i++ )
	{
		pent = &sv.edicts[i];
		if ( pent->free )
			continue;

		if ( pent->v.movetype == MOVETYPE_FOLLOW && pent->v.aiment )
			pentPVS = pent->v.aiment;
		else
			pentPVS = pent;

		if ( !PVSNode( sv.worldmodel->nodes, pentPVS->v.absmin, pentPVS->v.absmax ) )
			continue;

		pent->v.chain = pchain;
		pchain = pent;
	}

	if ( cl.worldmodel )
	{
		r_oldviewleaf = NULL;
		R_MarkLeaves();
	}

	return pchain;
}

//============================================================================

qboolean ValidCmd( const char *pCmd )
{
	int		len;

	len = Q_strlen( pCmd );

	// Valid commands all have a ';' or newline '\n' as their last character
	if ( len && ( pCmd[len - 1] == '\n' || pCmd[len - 1] == ';' ) )
		return TRUE;

	return FALSE;
}

/*
=================
PF_stuffcmd_I

Sends text over to the client's execution buffer
=================
*/
void PF_stuffcmd_I( edict_t *pEdict, char *szFmt, ... )
{
	int			entnum;
	client_t	*old;
	va_list		argptr;
	static char	szOut[1024];

	entnum = NUM_FOR_EDICT( pEdict );

	va_start( argptr, szFmt );
	_vsnprintf( szOut, sizeof( szOut ), szFmt, argptr );
	szOut[sizeof( szOut ) - 1] = 0;
	va_end( argptr );

	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "\n!!!\n\nStuffCmd:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n", szOut, entnum, svs.maxclients );
		return;
	}

	if ( ValidCmd( szOut ) )
	{
		old = host_client;
		host_client = &svs.clients[entnum - 1];
		Host_ClientCommands( "%s", szOut );
		host_client = old;
	}
	else
	{
		Con_Printf( "Tried to stuff bad command %s\n", szOut );
	}
}

/*
=================
PF_localcmd_I

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd_I( char *str )
{
	if ( ValidCmd( str ) )
		Cbuf_AddText( str );
	else
		Con_Printf( "Error, bad server command %s\n", str );
}

void PF_localexec_I( void )
{
	Cbuf_Execute();
}

edict_t *FindEntityInSphere( edict_t *pEdictStartSearchAfter, const float *org, float rad )
{
	edict_t	*ent;
	float	eorg;
	int		e, i, j;
	float	distSquared;

	if ( pEdictStartSearchAfter )
		e = NUM_FOR_EDICT( pEdictStartSearchAfter );
	else
		e = 0;

	rad *= rad;

	for ( i = e + 1; i < sv.num_edicts; i++ )
	{
		ent = &sv.edicts[i];
		if ( ent->free )
			continue;
		if ( !ent->v.classname )
			continue;

		if ( i <= svs.maxclients && !svs.clients[i - 1].active )
			continue;

		distSquared = 0;
		for ( j = 0; j < 3 && distSquared <= rad; j++ )
		{
			if ( org[j] < ent->v.absmin[j] )
				eorg = org[j] - ent->v.absmin[j];
			else if ( org[j] > ent->v.absmax[j] )
				eorg = org[j] - ent->v.absmax[j];
			else
				eorg = 0;

			distSquared = eorg * eorg + distSquared;
		}

		if ( distSquared > rad )
			continue;

		return ent;
	}

	return sv.edicts;
}

edict_t *PF_Spawn_I( void )
{
	edict_t	*pedict;

	pedict = ED_Alloc();
	return pedict;
}

edict_t *CreateNamedEntity( int className )
{
	edict_t		*pedict;
	ENTITYINIT	pEntityInit;

	if ( !className )
		Sys_Error( "Spawned a NULL entity!" );

	pedict = ED_Alloc();
	pedict->v.classname = className;

	pEntityInit = GetEntityInit( pr_strings + className );
	if ( pEntityInit )
	{
		pEntityInit( &pedict->v );
	}
	else
	{
		ED_Free( pedict );
		pedict = NULL;
		Con_DPrintf( "Can't create entity: %s\n", pr_strings + className );
	}

	return pedict;
}

void PF_Remove_I( edict_t *ed )
{
	ED_Free( ed );
}

int iGetIndex( const char *pszField )
{
	char		*p;
	char		sz[512];
	entvars_t	t;

	Q_strncpy( sz, pszField, sizeof( sz ) - 1 );
	sz[sizeof( sz ) - 1] = 0;
	Q_strlwr( sz );

	if ( !Q_strcmp( sz, "classname" ) )
		p = (char *)&t.classname;
	else if ( !Q_strcmp( sz, "model" ) )
		p = (char *)&t.model;
	else if ( !Q_strcmp( sz, "viewmodel" ) )
		p = (char *)&t.viewmodel;
	else if ( !Q_strcmp( sz, "weaponmodel" ) )
		p = (char *)&t.weaponmodel;
	else if ( !Q_strcmp( sz, "netname" ) )
		p = (char *)&t.netname;
	else if ( !Q_strcmp( sz, "target" ) )
		p = (char *)&t.target;
	else if ( !Q_strcmp( sz, "targetname" ) )
		p = (char *)&t.targetname;
	else if ( !Q_strcmp( sz, "message" ) )
		p = (char *)&t.message;
	else if ( !Q_strcmp( sz, "noise" ) )
		p = (char *)&t.noise;
	else if ( !Q_strcmp( sz, "noise1" ) )
		p = (char *)&t.noise1;
	else if ( !Q_strcmp( sz, "noise2" ) )
		p = (char *)&t.noise2;
	else if ( !Q_strcmp( sz, "noise3" ) )
		p = (char *)&t.noise3;
	else if ( !Q_strcmp( sz, "globalname" ) )
		p = (char *)&t.globalname;
	else
		return -1;

	return p - (char *)&t;
}

edict_t *PF_find_Shared( int eStartSearchAfter, int iFieldToMatch, const char *szValueToFind );

edict_t *FindEntityByString( edict_t *pEdictStartSearchAfter, const char *pszField, const char *pszValue )
{
	int		iField;

	iField = iGetIndex( pszField );
	if ( iField == -1 )
		return NULL;

	if ( !pszValue )
		return NULL;

	return PF_find_Shared( pEdictStartSearchAfter ? NUM_FOR_EDICT( pEdictStartSearchAfter ) : 0, iField, pszValue );
}

edict_t *PF_find_Shared( int eStartSearchAfter, int iFieldToMatch, const char *szValueToFind )
{
	int		e;
	char	*t;
	edict_t	*ed;

	e = eStartSearchAfter;
	for ( e++; e < sv.num_edicts; e++ )
	{
		ed = &sv.edicts[e];
		if ( ed->free )
			continue;

		t = pr_strings + *(string_t *)( (byte *)&ed->v + iFieldToMatch );
		if ( !t || t == pr_strings )
			continue;

		if ( !Q_strcmp( t, szValueToFind ) )
			return ed;
	}

	return sv.edicts;
}

int GetEntityIllum( edict_t *pEnt )
{
	int			iReturn;
	colorVec	cvFloorColor;
	int			iIndex;

	if ( !pEnt )
		return -1;

	iIndex = NUM_FOR_EDICT( pEnt );
	if ( iIndex <= svs.maxclients )
		return pEnt->v.light_level;

	if ( cls.state != ca_connected && cls.state != ca_uninitialized && cls.state != ca_active )
		return 0x80;

	cvFloorColor = cl_entities[iIndex].cvFloorColor;
	iReturn = cvFloorColor.r + cvFloorColor.g + cvFloorColor.b;
	iReturn /= 3;
	return iReturn;
}

/*
=================
PR_IsEmptyString

=================
*/
qboolean PR_IsEmptyString( const char *s )
{
	if ( s[0] <= ' ' )
		return TRUE;
	return FALSE;
}

int PF_precache_sound_I( char *s )
{
	int		i;

	if ( !s )
	{
		Host_Error( "PF_precache_sound_I: NULL pointer" );
		return 0;
	}

	if ( PR_IsEmptyString( s ) )
	{
		Host_Error( "PF_precache_sound_I: Bad string '%s'", s );
		return 0;
	}

	if ( s[0] == '!' )
	{
		Host_Error( "PF_precache_sound_I: '%s' do not precache sentence names!", s );
		return 0;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_SOUNDS; i++ )
		{
			if ( sv.sound_precache[i] && !Q_stricmp( sv.sound_precache[i], s ) )
				return i;
		}

		Host_Error( "PF_precache_sound_I: '%s' Precache can only be done in spawn functions", s );
	}

	// precache the sound and remember its index
	sv.sound_precache_hashedlookup_built = FALSE;

	for ( i = 0; i < MAX_SOUNDS; i++ )
	{
		if ( !sv.sound_precache[i] )
		{
			sv.sound_precache[i] = s;
			return i;
		}

		if ( !Q_stricmp( sv.sound_precache[i], s ) )
			return i;
	}

	Host_Error( "PF_precache_sound_I: Sound '%s' failed to precache because the item count is over the %d limit.\nReduce the number of brush models and/or regular models in the map to correct this.", s, MAX_SOUNDS );
	return 0;
}

unsigned short EV_Precache( int type, const char *psz )
{
	int		i;
	char	*pszRaw = NULL;

	if ( !psz )
	{
		Host_Error( "EV_Precache: NULL pointer" );
		return 0;
	}

	if ( PR_IsEmptyString( psz ) )
	{
		Host_Error( "EV_Precache: Bad string '%s'", psz );
		return 0;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 1; i < MAX_EVENTS; i++ )
		{
			if ( sv.event_precache[i].filename && !Q_stricmp( sv.event_precache[i].filename, psz ) )
				return i;
		}

		Host_Error( "EV_Precache: '%s' Precache can only be done in spawn functions", psz );
	}

	for ( i = 1; i < MAX_EVENTS; i++ )
	{
		if ( !sv.event_precache[i].filename )
		{
			int	size;

			if ( type == 1 )
			{
				char	szpath[MAX_PATH];

				_snprintf( szpath, sizeof( szpath ), "%s", psz );
				COM_FixSlashes( szpath );

				pszRaw = COM_LoadFile( szpath, 5, &size );
				if ( !pszRaw )
				{
					Host_Error( "EV_Precache:  file %s missing from server\n", psz );
					return 0;
				}
			}
			else
			{
				Host_Error( "EV_Precache:  only file type 1 supported currently\n" );
				return 0;
			}

			sv.event_precache[i].filename = psz;
			sv.event_precache[i].filesize = size;
			sv.event_precache[i].pszScript = pszRaw;
			sv.event_precache[i].index = i;
			return i;
		}

		if ( !Q_stricmp( sv.event_precache[i].filename, psz ) )
			return i;
	}

	Host_Error( "EV_Precache: '%s' overflow", psz );
	return 0;
}

void EV_PlayReliableEvent( client_t *cl, int entindex, unsigned short eventindex, float delay, event_args_t *pargs )
{
	unsigned char	data[1024];
	sizebuf_t		msg;
	event_args_t	eargs;
	event_args_t	nullargs;

	if ( cl->fakeclient )
		return;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Reliable Event";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	Q_memset( &nullargs, 0, sizeof( nullargs ) );

	eargs = *pargs;
	eargs.entindex = entindex;

	MSG_WriteByte( &msg, svc_event_reliable );
	MSG_StartBitWriting( &msg );

	MSG_WriteBits( eventindex, 10 );

	DELTA_WriteDelta( (byte *)&nullargs, (byte *)&eargs, TRUE, g_peventdelta, NULL );

	if ( delay == 0.0 )
	{
		MSG_WriteBits( 0, 1 );
	}
	else
	{
		MSG_WriteBits( 1, 1 );
		MSG_WriteBits( delay * 100.0, 16 );
	}

	MSG_EndBitWriting( &msg );

	if ( cl->netchan.message.cursize + msg.cursize > cl->netchan.message.maxsize )
		Netchan_CreateFragments( TRUE, &cl->netchan, &msg );
	else
		SZ_Write( &cl->netchan.message, msg.data, msg.cursize );
}

void EV_Playback( int flags, const edict_t *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	client_t		*cl;
	int				slot;
	vec3_t			event_origin;
	int				multicast_flag;
	int				leafnum;
	event_state_t	*es;
	event_info_t	*ei;
	int				bestslot;
	event_args_t	eargs;
	qboolean		skiphost = FALSE;
	int				invoker = -1;
	int				j;

	if ( flags & FEV_CLIENT )
		return;

	Q_memset( &eargs, 0, sizeof( eargs ) );

	if ( !VectorCompare( origin, vec3_origin ) )
	{
		VectorCopy( origin, eargs.origin );
		eargs.flags |= FEVENT_ORIGIN;
	}

	if ( !VectorCompare( angles, vec3_origin ) )
	{
		VectorCopy( angles, eargs.angles );
		eargs.flags |= FEVENT_ANGLES;
	}

	eargs.fparam1 = fparam1;
	eargs.fparam2 = fparam2;
	eargs.iparam1 = iparam1;
	eargs.iparam2 = iparam2;
	eargs.bparam1 = bparam1;
	eargs.bparam2 = bparam2;

	if ( eventindex < 1 || eventindex >= MAX_EVENTS )
	{
		Con_DPrintf( "EV_Playback:  index out of range %i\n", eventindex );
		return;
	}

	if ( !sv.event_precache[eventindex].pszScript )
	{
		Con_DPrintf( "EV_Playback:  no event for index %i\n", eventindex );
		return;
	}

	if ( pInvoker )
	{
		VectorCopy( pInvoker->v.origin, event_origin );

		invoker = NUM_FOR_EDICT( pInvoker );
		if ( invoker >= 1 && invoker <= svs.maxclients && ( pInvoker->v.flags & FL_DUCKING ) )
			eargs.ducking = 1;

		if ( !( eargs.flags & FEVENT_ORIGIN ) )
		{
			VectorCopy( pInvoker->v.origin, eargs.origin );
		}

		if ( !( eargs.flags & FEVENT_ANGLES ) )
		{
			VectorCopy( pInvoker->v.angles, eargs.angles );
		}
	}
	else
	{
		VectorCopy( eargs.origin, event_origin );
	}

	multicast_flag = MSG_FL_PAS;
	leafnum = SV_PointLeafnum( event_origin );

	for ( slot = 0; slot < svs.maxclients; slot++ )
	{
		cl = &svs.clients[slot];
		if ( !cl->active || !cl->spawned || !cl->connected || !cl->fully_connected || cl->fakeclient )
			continue;

		if ( pInvoker && pInvoker->v.groupinfo && cl->edict->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( pInvoker->v.groupinfo & cl->edict->v.groupinfo ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( pInvoker->v.groupinfo & cl->edict->v.groupinfo )
					continue;
			}
		}

		// Send to everybody if FEV_GLOBAL, otherwise only to the clients that can see the invoker
		if ( !( flags & FEV_GLOBAL ) && pInvoker && !SV_ValidClientMulticast( cl, leafnum, multicast_flag ) )
			continue;

		// Don't send to the host if FEV_NOTHOST is set and the host predicts weapons
		if ( cl == host_client && ( flags & FEV_NOTHOST ) && cl->lw )
			continue;

		// Only send to the host if FEV_HOSTONLY is set
		if ( ( flags & FEV_HOSTONLY ) && cl->edict != pInvoker )
			continue;

		if ( flags & FEV_RELIABLE )
		{
			EV_PlayReliableEvent( cl, pInvoker ? NUM_FOR_EDICT( pInvoker ) : 0, eventindex, delay, &eargs );
			continue;
		}

		// Try to find an existing slot to update
		es = &cl->events;
		bestslot = -1;

		if ( flags & FEV_UPDATE )
		{
			for ( j = 0; j < MAX_EVENT_QUEUE; j++ )
			{
				ei = &es->ei[j];

				if ( ei->index != eventindex )
					continue;
				if ( invoker == -1 )
					continue;
				if ( ei->entity_index != invoker )
					continue;

				bestslot = j;
				break;
			}
		}

		// No slot found, try an empty one
		if ( bestslot == -1 )
		{
			for ( j = 0; j < MAX_EVENT_QUEUE; j++ )
			{
				ei = &es->ei[j];
				if ( ei->index == 0 )
				{
					bestslot = j;
					break;
				}
			}

			if ( j >= MAX_EVENT_QUEUE && bestslot != -1 )
				ei = &es->ei[bestslot];
		}

		// no slots found?
		if ( bestslot == -1 )
			continue;

		ei->index = eventindex;
		ei->packet_index = -1;

		if ( pInvoker )
			ei->entity_index = invoker;
		else
			ei->entity_index = -1;

		ei->args = eargs;
		ei->fire_time = delay;
	}
}

void EV_SV_Playback( int flags, int clientindex, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	edict_t	*pEdict;

	if ( flags & FEV_CLIENT )
		return;

	if ( clientindex < 0 || clientindex >= svs.maxclients )
	{
		Host_Error( "EV_SV_Playback:  Client index %i out of range\n", clientindex );
		return;
	}

	pEdict = svs.clients[clientindex].edict;

	EV_Playback( flags, pEdict, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2 );
}

int PF_precache_model_I( char *s )
{
	int		i;
	int		iOptional = 0;

	if ( !s )
	{
		Host_Error( "PF_precache_model_I: NULL pointer" );
		return 0;
	}

	if ( PR_IsEmptyString( s ) )
	{
		Host_Error( "PF_precache_model_I: Bad string '%s'", s );
		return 0;
	}

	if ( *s == '!' )
	{
		s++;
		iOptional = 1;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_MODELS; i++ )
		{
			if ( sv.model_precache[i] && !Q_stricmp( sv.model_precache[i], s ) )
				return i;
		}

		Host_Error( "PF_precache_model_I: '%s' Precache can only be done in spawn functions", s );
	}

	for ( i = 0; i < MAX_MODELS; i++ )
	{
		if ( !sv.model_precache[i] )
		{
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName( s, TRUE, TRUE );
			if ( !iOptional )
				sv.model_precache_flags[i] |= RES_FATALIFMISSING;
			return i;
		}

		if ( !Q_stricmp( sv.model_precache[i], s ) )
			return i;
	}

	Host_Error( "PF_precache_model_I: Model '%s' failed to precache because the item count is over the %d limit.\nReduce the number of brush models and/or regular models in the map to correct this.", s, MAX_MODELS );
	return 0;
}

int PF_precache_generic_I( char *s )
{
	int		i;

	if ( !s )
	{
		Host_Error( "PF_precache_generic_I: NULL pointer" );
		return 0;
	}

	if ( PR_IsEmptyString( s ) )
	{
		Host_Error( "PF_precache_generic_I: Bad string '%s'", s );
		return 0;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_GENERIC; i++ )
		{
			if ( sv.generic_precache[i] && !Q_strcmp( sv.generic_precache[i], s ) )
				return i;
		}

		Host_Error( "PF_precache_generic_I: '%s' Precache can only be done in spawn functions", s );
	}

	for ( i = 0; i < MAX_GENERIC; i++ )
	{
		if ( !sv.generic_precache[i] )
		{
			sv.generic_precache[i] = s;
			return i;
		}

		if ( !Q_strcasecmp( sv.generic_precache[i], s ) )
			return i;
	}

	Host_Error( "PF_precache_generic_I: Generic item '%s' failed to precache because the item count is over the %d limit.\nReduce the number of brush models and/or regular models in the map to correct this.", s, MAX_GENERIC );
	return 0;
}

int PF_IsMapValid_I( char *mapname )
{
	char	cBuf[MAX_PATH];

	if ( !mapname || !strlen( mapname ) )
		return 0;

	_snprintf( cBuf, sizeof( cBuf ), "maps/%.32s.bsp", mapname );
	return FS_FileExists( cBuf );
}

int PF_NumberOfEntities_I( void )
{
	int		ent_count, i;

	ent_count = 0;
	for ( i = 0; i < sv.num_edicts; i++ )
	{
		if ( !sv.edicts[i].free )
			ent_count++;
	}

	return ent_count;
}

char *PF_GetInfoKeyBuffer_I( edict_t *e )
{
	int		e1;
	char	*value;

	if ( !e )
	{
		value = localinfo;
		return value;
	}

	e1 = NUM_FOR_EDICT( e );
	if ( e1 == 0 )
		value = Info_Serverinfo();
	else if ( e1 <= MAX_CLIENTS )
		value = svs.clients[e1 - 1].userinfo;
	else
		value = "";

	return value;
}

char *PF_InfoKeyValue_I( char *infobuffer, char *key )
{
	return Info_ValueForKey( infobuffer, key );
}

void PF_SetKeyValue_I( char *infobuffer, char *key, char *value )
{
	if ( infobuffer == localinfo )
		Info_SetValueForKey( infobuffer, key, value, MAX_LOCALINFO );
	else if ( infobuffer == Info_Serverinfo() )
		Info_SetValueForKey( infobuffer, key, value, MAX_SERVERINFO_STRING );
	else
		Sys_Error( "Can't set client keys with SetKeyValue" );
}

void PF_RemoveKey_I( char *s, const char *key )
{
	Info_RemoveKey( s, key );
}

void PF_SetClientKeyValue_I( int clientIndex, char *infobuffer, char *key, char *value )
{
	client_t	*pClient;

	if ( infobuffer == localinfo || infobuffer == Info_Serverinfo() || clientIndex < 1 || clientIndex > svs.maxclients )
		return;

	if ( !Q_strcmp( Info_ValueForKey( infobuffer, key ), value ) )
		return;

	Info_SetValueForStarKey( infobuffer, key, value, MAX_INFO_STRING );

	pClient = svs.clients + clientIndex - 1;
	pClient->sendinfo = TRUE;
	pClient->sendinfo_time = 0;
}

/*
===============
PF_walkmove_I

float(float yaw, float dist) walkmove
===============
*/
int PF_walkmove_I( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t	move;
	int		returnValue;

	if ( !( ent->v.flags & ( FL_ONGROUND | FL_FLY | FL_SWIM ) ) )
		return 0;

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos( yaw ) * dist;
	move[1] = sin( yaw ) * dist;
	move[2] = 0;

	if ( iMode == WALKMOVE_WORLDONLY )
		returnValue = SV_movetest( ent, move, TRUE );
	else if ( iMode == WALKMOVE_CHECKONLY )
		returnValue = SV_movestep( ent, move, FALSE );
	else
		returnValue = SV_movestep( ent, move, TRUE );

	return returnValue;
}

/*
===============
PF_droptofloor_I

void() droptofloor
===============
*/
int PF_droptofloor_I( edict_t *ent )
{
	vec3_t		end;
	trace_t		trace;
	qboolean	monsterClip;

	if ( ent->v.flags & FL_MONSTERCLIP )
		monsterClip = TRUE;
	else
		monsterClip = FALSE;

	VectorCopy( ent->v.origin, end );
	end[2] -= 256;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent, monsterClip );

	if ( trace.allsolid )
		return -1;

	if ( trace.fraction == 1 )
		return 0;

	VectorCopy( trace.endpos, ent->v.origin );
	SV_LinkEdict( ent, FALSE );
	ent->v.flags |= FL_ONGROUND;
	ent->v.groundentity = trace.ent;

	return 1;
}

int PF_DecalIndex( const char *name )
{
	int		i;

	for ( i = 0; i < sv_decalnamecount; i++ )
	{
		if ( !Q_stricmp( sv_decalnames[i].name, name ) )
			return i;
	}

	return -1;
}

/*
===============
PF_lightstyle_I

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle_I( int style, char *val )
{
	client_t	*client;
	int			j;

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if ( sv.state != ss_active )
		return;

	for ( j = 0, client = svs.clients; j < svs.maxclients; j++, client++ )
	{
		if ( ( client->active || client->spawned ) && !client->fakeclient )
		{
			MSG_WriteChar( &client->netchan.message, svc_lightstyle );
			MSG_WriteChar( &client->netchan.message, style );
			MSG_WriteString( &client->netchan.message, val );
		}
	}
}

int PF_checkbottom_I( edict_t *pEdict )
{
	return SV_CheckBottom( pEdict );
}

int PF_pointcontents_I( const float *rgflVector )
{
	return SV_PointContents( rgflVector );
}

/*
=============
PF_aim_I

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim_I( edict_t *ent, float speed, float *rgflReturn )
{
	edict_t	*check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;

	if ( !ent || ( ent->v.flags & FL_FAKECLIENT ) )
	{
		VectorCopy( gGlobalVariables.v_forward, rgflReturn );
		return;
	}

	VectorCopy( ent->v.origin, start );
	VectorAdd( start, ent->v.view_ofs, start );

// try sending a trace straight
	VectorCopy( gGlobalVariables.v_forward, dir );
	VectorMA( start, 2048, dir, end );
	tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE );

	if ( tr.ent && tr.ent->v.takedamage == DAMAGE_AIM && ( ent->v.team <= 0 || ent->v.team != tr.ent->v.team ) )
	{
		VectorCopy( gGlobalVariables.v_forward, rgflReturn );
		return;
	}

// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = sv_aim.value;
	bestent = NULL;

	for ( i = 1; i < sv.num_edicts; i++ )
	{
		check = &sv.edicts[i];
		if ( check->v.takedamage != DAMAGE_AIM )
			continue;
		if ( check->v.flags & FL_FAKECLIENT )
			continue;
		if ( check == ent )
			continue;
		if ( ent->v.team > 0 && ent->v.team == check->v.team )
			continue;	// don't aim at teammate

		for ( j = 0; j < 3; j++ )
			end[j] = check->v.origin[j] + 0.75 * ( check->v.mins[j] + check->v.maxs[j] ) + ent->v.view_ofs[j] * 0.0;

		VectorSubtract( end, start, dir );
		VectorNormalize( dir );
		dist = DotProduct( dir, gGlobalVariables.v_forward );
		if ( dist < bestdist )
			continue;	// to far to turn

		tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, FALSE );
		if ( tr.ent == check )
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
			VectorCopy( dir, bestdir );
		}
	}

	VectorCopy( bestdir, rgflReturn );
}

/*
==============
PF_changeyaw_I

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw_I( edict_t *ent )
{
	float	ideal, current, move, speed;

	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if ( current == ideal )
		return;

	move = ideal - current;
	if ( ideal > current )
	{
		if ( move >= 180 )
			move = move - 360;
	}
	else
	{
		if ( move <= -180 )
			move = move + 360;
	}

	if ( move > 0 )
	{
		if ( move > speed )
			move = speed;
	}
	else
	{
		if ( move < -speed )
			move = -speed;
	}

	ent->v.angles[1] = anglemod( current + move );
}

void PF_changepitch_I( edict_t *ent )
{
	float	ideal, current, move, speed;

	current = anglemod( ent->v.angles[0] );
	ideal = ent->v.idealpitch;
	speed = ent->v.pitch_speed;

	if ( current == ideal )
		return;

	move = ideal - current;
	if ( ideal > current )
	{
		if ( move >= 180 )
			move = move - 360;
	}
	else
	{
		if ( move <= -180 )
			move = move + 360;
	}

	if ( move > 0 )
	{
		if ( move > speed )
			move = speed;
	}
	else
	{
		if ( move < -speed )
			move = -speed;
	}

	ent->v.angles[0] = anglemod( current + move );
}

void PF_setview_I( const edict_t *clientent, const edict_t *viewent )
{
	client_t	*client;
	int			clientnum;

	clientnum = NUM_FOR_EDICT( clientent );
	if ( clientnum < 1 || clientnum > svs.maxclients )
		Host_Error( "PF_setview_I: not a client" );

	client = &svs.clients[clientnum - 1];
	if ( client->fakeclient )
		return;

	client->pViewEntity = viewent;

	MSG_WriteByte( &client->netchan.message, svc_setview );
	MSG_WriteShort( &client->netchan.message, NUM_FOR_EDICT( viewent ) );
}

void PF_crosshairangle_I( const edict_t *clientent, float pitch, float yaw )
{
	client_t	*client;
	int			clientnum;

	clientnum = NUM_FOR_EDICT( clientent );
	if ( clientnum < 1 || clientnum > svs.maxclients )
		return;

	client = &svs.clients[clientnum - 1];
	if ( client->fakeclient )
		return;

	if ( pitch > 180 )
		pitch -= 360;
	if ( pitch < -180 )
		pitch += 360;
	if ( yaw > 180 )
		yaw -= 360;
	if ( yaw < -180 )
		yaw += 360;

	MSG_WriteByte( &client->netchan.message, svc_crosshairangle );
	MSG_WriteChar( &client->netchan.message, pitch * 5 );
	MSG_WriteChar( &client->netchan.message, yaw * 5 );
}

edict_t *PF_CreateFakeClient_I( const char *netname )
{
	client_t	*fakeclient;
	edict_t		*ent;
	int			i;
	extern int	g_userid;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		fakeclient = &svs.clients[i];
		if ( !fakeclient->active && !fakeclient->spawned && !fakeclient->connected )
			break;
	}

	// couldn't find a slot
	if ( i == svs.maxclients )
		return NULL;

	ent = EDICT_NUM( i + 1 );

	if ( fakeclient->frames )
		SV_ClearFrames( &fakeclient->frames );

	Q_memset( fakeclient, 0, sizeof( client_t ) );

	fakeclient->resourcesneeded.pNext = fakeclient->resourcesneeded.pPrev = &fakeclient->resourcesneeded;
	fakeclient->resourcesonhand.pNext = fakeclient->resourcesonhand.pPrev = &fakeclient->resourcesonhand;

	Q_strncpy( fakeclient->name, netname, sizeof( fakeclient->name ) );
	fakeclient->name[sizeof( fakeclient->name ) - 1] = 0;

	fakeclient->active = TRUE;
	fakeclient->spawned = TRUE;
	fakeclient->fully_connected = TRUE;
	fakeclient->connected = TRUE;
	fakeclient->uploading = FALSE;
	fakeclient->edict = ent;
	fakeclient->fakeclient = TRUE;
	fakeclient->userid = g_userid++;

	ent->v.netname = fakeclient->name - pr_strings;
	ent->v.pContainingEntity = ent;
	ent->v.flags = FL_FAKECLIENT | FL_CLIENT;

	Info_SetValueForKey( fakeclient->userinfo, "name", netname, MAX_INFO_STRING );
	Info_SetValueForKey( fakeclient->userinfo, "model", "gordon", MAX_INFO_STRING );
	Info_SetValueForKey( fakeclient->userinfo, "topcolor", "1", MAX_INFO_STRING );
	Info_SetValueForKey( fakeclient->userinfo, "bottomcolor", "1", MAX_INFO_STRING );
	fakeclient->sendinfo = TRUE;

	SV_ExtractFromUserinfo( fakeclient );

	fakeclient->network_userid.m_SteamID = ISteamGameServer_CreateUnauthenticatedUserConnection();
	fakeclient->network_userid.idtype = AUTH_IDTYPE_STEAM;
	ISteamGameServer_BUpdateUserData( fakeclient->network_userid.m_SteamID, netname, 0 );

	return ent;
}

void PF_RunPlayerMove_I( edict_t *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec )
{
	usercmd_t	cmd;
	edict_t		*oldclient;
	client_t	*old;
	int			entnum;

	oldclient = sv_player;
	old = host_client;

	entnum = NUM_FOR_EDICT( fakeclient );

	sv_player = fakeclient;
	host_client = &svs.clients[entnum - 1];

	host_client->svtimebase = sv.time + host_frametime - msec / 1000.0;

	Q_memset( &cmd, 0, sizeof( cmd ) );

	VectorCopy( viewangles, cmd.viewangles );
	cmd.forwardmove = forwardmove;
	cmd.sidemove = sidemove;
	cmd.upmove = upmove;
	cmd.buttons = buttons;
	cmd.impulse = impulse;
	cmd.lightlevel = 0;
	cmd.msec = msec;

	pmove = &g_svmove;

	SV_PreRunCmd();
	SV_RunCmd( &cmd, 0 );

	host_client->lastcmd = cmd;

	sv_player = oldclient;
	host_client = old;
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

static byte		gMsgData[512];
static sizebuf_t	gMsgBuffer = { "MessageBegin/End", 0, gMsgData, sizeof( gMsgData ), 0 };
static edict_t	*gMsgEntity;
static int		gMsgDest;
static int		gMsgType;
static qboolean	gMsgStarted;
static vec3_t	gMsgOrigin;

sizebuf_t *WriteDest_Parm( int dest )
{
	int		entnum;

	switch ( dest )
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	case MSG_ONE:
	case MSG_ONE_UNRELIABLE:
		entnum = NUM_FOR_EDICT( gMsgEntity );
		if ( entnum < 1 || entnum > svs.maxclients )
			Host_Error( "WriteDest_Parm: not a client" );
		if ( dest == MSG_ONE )
			return &svs.clients[entnum - 1].netchan.message;
		else
			return &svs.clients[entnum - 1].datagram;
	case MSG_ALL:
		return &sv.reliable_datagram;
	case MSG_INIT:
		return &sv.signon;
	case MSG_PVS:
	case MSG_PAS:
		return &sv.multicast;
	case MSG_SPEC:
		return &sv.spectator;
	default:
		Host_Error( "WriteDest_Parm: bad destination=%d", dest );
		return NULL;
	}
}

void PF_MessageBegin_I( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed )
{
	// some messages need entity parameters
	if ( ( msg_dest == MSG_ONE || msg_dest == MSG_ONE_UNRELIABLE ) && !ed )
		Sys_Error( "MSG_ONE or MSG_ONE_UNRELIABLE with no target entity\n" );

	if ( msg_dest != MSG_ONE_UNRELIABLE && msg_dest != MSG_ONE && ed )
		Sys_Error( "Invalid message;  cannot use broadcast message with a target entity" );

	if ( gMsgStarted )
		Sys_Error( "New message started when msg '%d' has not been sent yet", gMsgType );

	if ( msg_type == 0 )
		Sys_Error( "Tried to create a message with a bogus message type ( 0 )" );

	gMsgStarted = TRUE;
	gMsgType = msg_type;
	gMsgEntity = ed;
	gMsgDest = msg_dest;

	if ( msg_dest == MSG_PVS || msg_dest == MSG_PAS )
	{
		if ( pOrigin )
		{
			VectorCopy( pOrigin, gMsgOrigin );
		}

		// Server is in single player mode
		if ( Host_IsSinglePlayerGame() )
			msg_dest = MSG_BROADCAST;
	}

	gMsgBuffer.cursize = 0;
	gMsgBuffer.flags = FSB_ALLOWOVERFLOW;
}

void PF_MessageEnd_I( void )
{
	qboolean	MsgIsVarLength;
	sizebuf_t	*pBuffer;
	int			entnum;

	MsgIsVarLength = FALSE;

	if ( !gMsgStarted )
		Sys_Error( "MESSAGE_END called with no active message\n" );
	gMsgStarted = FALSE;

	// don't send anything to bots
	if ( gMsgEntity && ( gMsgEntity->v.flags & FL_FAKECLIENT ) )
		return;

	if ( gMsgBuffer.flags & FSB_OVERFLOWED )
	{
		Sys_Error( "MESSAGE_END called, but message buffer from .dll had overflowed\n" );
		return;
	}

	if ( gMsgType > SVC_LASTMSG )
	{
		UserMsg	*pUserMsg;

		pUserMsg = sv_gpUserMsgs;
		while ( pUserMsg )
		{
			if ( pUserMsg->iMsg == gMsgType )
				break;
			pUserMsg = pUserMsg->next;
		}

		if ( !pUserMsg && gMsgDest == MSG_INIT )
		{
			pUserMsg = sv_gpNewUserMsgs;
			while ( pUserMsg )
			{
				if ( pUserMsg->iMsg == gMsgType )
					break;
				pUserMsg = pUserMsg->next;
			}
		}

		if ( !pUserMsg )
		{
			Con_DPrintf( "PF_MessageEnd_I:  Unknown User Msg %d\n", gMsgType );
			return;
		}

		if ( pUserMsg->iSize == -1 )
		{
			MsgIsVarLength = TRUE;

			// Limit packet sizes
			if ( gMsgBuffer.cursize > MAX_USER_MSG_DATA )
			{
				Host_Error( "PF_MessageEnd_I:  Refusing to send user message %s of %i bytes to client, user message size limit is %i bytes\n", pUserMsg->szName, gMsgBuffer.cursize, MAX_USER_MSG_DATA );
				return;
			}
		}
		else if ( pUserMsg->iSize != gMsgBuffer.cursize )
		{
			Sys_Error( "User Msg '%s': %d bytes written, expected %d\n", pUserMsg->szName, gMsgBuffer.cursize, pUserMsg->iSize );
			return;
		}
	}

	pBuffer = WriteDest_Parm( gMsgDest );

	if ( gMsgDest == MSG_BROADCAST && pBuffer->cursize + gMsgBuffer.cursize > pBuffer->maxsize )
		return;

	if ( !pBuffer->data )
		return;

	if ( gMsgType > SVC_LASTMSG && ( gMsgDest == MSG_ONE || gMsgDest == MSG_ONE_UNRELIABLE ) )
	{
		client_t	*cl;

		entnum = NUM_FOR_EDICT( gMsgEntity );
		if ( entnum < 1 || entnum > svs.maxclients )
			Host_Error( "WriteDest_Parm: not a client" );

		cl = &svs.clients[entnum - 1];
		if ( !cl->hasusrmsgs || ( !cl->active && !cl->spawned ) || cl->fakeclient )
			return;
	}

	MSG_WriteByte( pBuffer, gMsgType );
	if ( MsgIsVarLength )
		MSG_WriteByte( pBuffer, gMsgBuffer.cursize );
	MSG_WriteBuf( pBuffer, gMsgBuffer.cursize, gMsgBuffer.data );

	switch ( gMsgDest )
	{
	case MSG_PVS:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PVS, FALSE );
		break;
	case MSG_PAS:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PAS, FALSE );
		break;
	case MSG_PVS_R:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PAS, TRUE );
		break;
	case MSG_PAS_R:
		SV_Multicast( gMsgEntity, gMsgOrigin, MSG_FL_PAS, TRUE );
		break;
	}
}

void PF_WriteByte_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_BYTE called with no active message\n" );

	MSG_WriteByte( &gMsgBuffer, iValue );
}

void PF_WriteChar_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_CHAR called with no active message\n" );

	MSG_WriteChar( &gMsgBuffer, iValue );
}

void PF_WriteShort_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "WRITE_SHORT called with no active message\n" );

	MSG_WriteShort( &gMsgBuffer, iValue );
}

void PF_WriteLong_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteLong_I called with no active message\n" );

	MSG_WriteLong( &gMsgBuffer, iValue );
}

void PF_WriteAngle_I( float flValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteAngle_I called with no active message\n" );

	MSG_WriteAngle( &gMsgBuffer, flValue );
}

void PF_WriteCoord_I( float flValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteCoord_I called with no active message\n" );

	MSG_WriteShort( &gMsgBuffer, (int)( flValue * 8 ) );
}

void PF_WriteString_I( const char *sz )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteString_I called with no active message\n" );

	MSG_WriteString( &gMsgBuffer, sz );
}

void PF_WriteEntity_I( int iValue )
{
	if ( !gMsgStarted )
		Sys_Error( "PF_WriteEntity_I called with no active message\n" );

	MSG_WriteShort( &gMsgBuffer, iValue );
}

//=============================================================================

void PF_makestatic_I( edict_t *ent )
{
	int		i;

	MSG_WriteByte( &sv.signon, svc_spawnstatic );
	MSG_WriteShort( &sv.signon, SV_ModelIndex( pr_strings + ent->v.model ) );
	MSG_WriteByte( &sv.signon, ent->v.sequence );
	MSG_WriteByte( &sv.signon, ent->v.frame );
	MSG_WriteWord( &sv.signon, ent->v.colormap );
	MSG_WriteByte( &sv.signon, ent->v.skin );

	for ( i = 0; i < 3; i++ )
	{
		MSG_WriteCoord( &sv.signon, ent->v.origin[i] );
		MSG_WriteAngle( &sv.signon, ent->v.angles[i] );
	}

	MSG_WriteByte( &sv.signon, ent->v.rendermode );
	if ( ent->v.rendermode != kRenderNormal )
	{
		MSG_WriteByte( &sv.signon, ent->v.renderamt );
		MSG_WriteByte( &sv.signon, ent->v.rendercolor[0] );
		MSG_WriteByte( &sv.signon, ent->v.rendercolor[1] );
		MSG_WriteByte( &sv.signon, ent->v.rendercolor[2] );
		MSG_WriteByte( &sv.signon, ent->v.renderfx );
	}

// throw the entity away now
	ED_Free( ent );
}

void PF_StaticDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex )
{
	MSG_WriteByte( &sv.signon, svc_temp_entity );
	MSG_WriteByte( &sv.signon, TE_BSPDECAL );
	MSG_WriteCoord( &sv.signon, origin[0] );
	MSG_WriteCoord( &sv.signon, origin[1] );
	MSG_WriteCoord( &sv.signon, origin[2] );
	MSG_WriteShort( &sv.signon, decalIndex );
	MSG_WriteShort( &sv.signon, entityIndex );
	if ( entityIndex )
		MSG_WriteShort( &sv.signon, modelIndex );
}

void PF_setspawnparms_I( edict_t *ent )
{
	int			i;
	client_t	*client;

	i = NUM_FOR_EDICT( ent );
	if ( i < 1 || i > svs.maxclients )
		Host_Error( "Entity is not a client" );

// copy spawn parms out of the client_t
	client = svs.clients + ( i - 1 );
}

void PF_changelevel_I( char *s1, char *s2 )
{
	static int	last_spawncount;

// make sure we don't issue two changelevels
	if ( svs.spawncount == last_spawncount )
		return;
	last_spawncount = svs.spawncount;

	SV_SkipUpdates();

	if ( !s2 )
		Cbuf_AddText( va( "changelevel %s\n", s1 ) );
	else
		Cbuf_AddText( va( "changelevel2 %s %s\n", s1, s2 ) );
}

#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB	32
#define NDIV	( 1 + ( IM - 1 ) / NTAB )
#define MAX_RANDOM_RANGE	0x7FFFFFFFUL

// fran1 -- return a random floating-point number on the interval [0,1)
//
#define AM		( 1.0 / IM )
#define EPS		1.2e-7
#define RNMX	( 1.0 - EPS )

static int32	idum;

void SeedRandomNumberGenerator( void )
{
	idum = -time( NULL );
	if ( idum > 1000 )
	{
		idum = -idum;
	}
	else if ( idum > -1000 )
	{
		idum -= 22261048;
	}
}

int32 ran1( void )
{
	int				j;
	int32			k;
	static int32	iy = 0;
	static int32	iv[NTAB];

	if ( idum <= 0 || !iy )
	{
		if ( -( idum ) < 1 )
			idum = 1;
		else
			idum = -( idum );

		for ( j = NTAB + 7; j >= 0; j-- )
		{
			k = ( idum ) / IQ;
			idum = IA * ( idum - k * IQ ) - IR * k;
			if ( idum < 0 )
				idum += IM;
			if ( j < NTAB )
				iv[j] = idum;
		}
		iy = iv[0];
	}

	k = ( idum ) / IQ;
	idum = IA * ( idum - k * IQ ) - IR * k;
	if ( idum < 0 )
		idum += IM;

	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

float fran1( void )
{
	float	temp;

	temp = AM * ran1();
	if ( temp > RNMX )
		return (float)RNMX;
	else
		return temp;
}

float RandomFloat( float flLow, float flHigh )
{
	float	fl;

	g_engdstAddrs.pfnRandomFloat( &flLow, &flHigh );

	fl = fran1();	// float in [0,1)
	return ( fl * ( flHigh - flLow ) ) + flLow;	// float in [low,high)
}

int32 RandomLong( int32 lLow, int32 lHigh )
{
	uint32	maxAcceptable;
	uint32	x;
	uint32	n;

	g_engdstAddrs.pfnRandomLong( &lLow, &lHigh );

	x = lHigh - lLow + 1;
	if ( x <= 0 || MAX_RANDOM_RANGE < x - 1 )
	{
		return lLow;
	}

	// The following maps a uniform distribution on the interval [0,MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	//
	maxAcceptable = MAX_RANDOM_RANGE - ( ( MAX_RANDOM_RANGE + 1 ) % x );
	do
	{
		n = ran1();
	} while ( n > maxAcceptable );

	return lLow + ( n % x );
}

void PF_FadeVolume( const edict_t *clientent, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( clientent );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_FadeVolume a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	if ( client->fakeclient )
		return;

	MSG_WriteChar( &client->netchan.message, svc_soundfade );
	MSG_WriteByte( &client->netchan.message, (byte)fadePercent );
	MSG_WriteByte( &client->netchan.message, (byte)holdTime );
	MSG_WriteByte( &client->netchan.message, (byte)fadeOutSeconds );
	MSG_WriteByte( &client->netchan.message, (byte)fadeInSeconds );
}

void PF_SetClientMaxspeed( const edict_t *clientent, float fNewMaxspeed )
{
	client_t	*client;
	entvars_t	*ev;
	int			entnum;

	entnum = NUM_FOR_EDICT( clientent );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_SetClientMaxspeed a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	ev = &clientent->v;
	ev->maxspeed = fNewMaxspeed;
}

int PF_GetPlayerUserId( edict_t *e )
{
	client_t	*cl;
	int			i;

	if ( !sv.active )
		return -1;

	if ( !e )
		return -1;

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->edict == e )
			return cl->userid;
	}

	// Couldn't find it
	return -1;
}

unsigned int PF_GetPlayerWONId( edict_t *e )
{
	return 0xFFFFFFFF;
}

const char *PF_GetPlayerAuthId( edict_t *e )
{
	static char	szAuthID[5][64];
	static int	count = -1;
	client_t	*cl;
	int			i = 0;

	count = ( count + 1 ) % 5;
	szAuthID[count][0] = 0;

	if ( !sv.active )
		return szAuthID[count];

	if ( !e )
		return szAuthID[count];

	for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
	{
		if ( cl->edict == e )
		{
			if ( cl->fakeclient )
			{
				_snprintf( szAuthID[count], sizeof( szAuthID[count] ) - 1, "%s", "BOT" );
			}
			else
			{
				switch ( cl->network_userid.idtype )
				{
				case AUTH_IDTYPE_LOCAL:
					_snprintf( szAuthID[count], sizeof( szAuthID[count] ) - 1, "%s", "HLTV" );
					break;
				default:
					_snprintf( szAuthID[count], sizeof( szAuthID[count] ) - 1, "%s", SV_GetClientIDString( cl ) );
					break;
				}
			}
		}
	}

	szAuthID[count][sizeof( szAuthID[count] ) - 1] = 0;
	return szAuthID[count];
}

void PF_BuildSoundMsg_I( edict_t *entity, int channel, const char *sample, float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t *ed )
{
	PF_MessageBegin_I( msg_dest, msg_type, pOrigin, ed );
	SV_BuildSoundMsg( entity, channel, sample, volume, attenuation, fFlags, pitch, pOrigin, &gMsgBuffer );
	PF_MessageEnd_I();
}

int PF_IsDedicatedServer( void )
{
	return g_bIsDedicatedServer;
}

const char *PF_GetPhysicsInfoString( const edict_t *pClient )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPhysicsInfoString a non-client\n" );
		return "";
	}

	client = &svs.clients[entnum - 1];
	return client->physinfo;
}

const char *PF_GetPhysicsKeyValue( const edict_t *pClient, const char *key )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPhysicsKeyValue a non-client\n" );
		return "";
	}

	client = &svs.clients[entnum - 1];
	return Info_ValueForKey( client->physinfo, key );
}

void PF_SetPhysicsKeyValue( const edict_t *pClient, const char *key, const char *value )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_SetPhysicsKeyValue a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	Info_SetValueForKey( client->physinfo, key, value, MAX_PHYSINFO_STRING );
}

int PF_GetCurrentPlayer( void )
{
	int		idx;

	idx = host_client - svs.clients;
	if ( idx < 0 || idx >= svs.maxclients )
		return -1;

	return idx;
}

int PF_CanSkipPlayer( const edict_t *pClient )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_CanSkipPlayer a non-client\n" );
		return 0;
	}

	client = &svs.clients[entnum - 1];
	return client->lw != 0;
}

int	g_groupmask;
int	g_groupop;

void PF_SetGroupMask( int mask, int op )
{
	g_groupmask = mask;
	g_groupop = op;
}

int PF_CreateInstancedBaseline( int classname, struct entity_state_s *baseline )
{
	if ( sv.instance_baselines->number >= NUM_BASELINES - 1 )
		return 0;

	sv.instance_baselines->classname[sv.instance_baselines->number] = classname;
	sv.instance_baselines->baseline[sv.instance_baselines->number] = *baseline;
	sv.instance_baselines->number++;

	return sv.instance_baselines->number;
}

void PF_Cvar_DirectSet( struct cvar_s *var, char *value )
{
	Cvar_DirectSet( var, value );
}

void PF_ForceUnmodified( FORCE_TYPE type, float *mins, float *maxs, const char *filename )
{
	int		i;

	if ( !filename )
	{
		Host_Error( "PF_ForceUnmodified: NULL pointer" );
		return;
	}

	if ( PR_IsEmptyString( filename ) )
	{
		Host_Error( "PF_ForceUnmodified: Bad string '%s'", filename );
		return;
	}

	if ( sv.state != ss_loading )
	{
		for ( i = 0; i < MAX_CONSISTENCY_LIST; i++ )
		{
			if ( sv.consistency_list[i].filename && !Q_stricmp( sv.consistency_list[i].filename, filename ) )
				return;
		}

		Host_Error( "ForceUnmodified: '%s' Precache can only be done in spawn functions", filename );
	}

	for ( i = 0; i < MAX_CONSISTENCY_LIST; i++ )
	{
		if ( !sv.consistency_list[i].filename )
		{
			sv.consistency_list[i].filename = (char *)filename;
			sv.consistency_list[i].check_type = type;

			if ( mins )
			{
				VectorCopy( mins, sv.consistency_list[i].mins );
			}

			if ( maxs )
			{
				VectorCopy( maxs, sv.consistency_list[i].maxs );
			}
			return;
		}

		if ( !Q_stricmp( sv.consistency_list[i].filename, filename ) )
			return;
	}

	Host_Error( "ForceUnmodified: '%s' overflow", filename );
}

void PF_GetPlayerStats( const edict_t *pClient, int *ping, int *packet_loss )
{
	client_t	*client;
	int			entnum;

	*ping = 0;
	*packet_loss = 0;

	entnum = NUM_FOR_EDICT( pClient );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		Con_Printf( "tried to PF_GetPlayerStats a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];

	*ping = client->latency * 1000.0;
	*packet_loss = client->packet_loss;
}

void QueryClientCvarValueCmd( void )
{
	if ( Cmd_Argc() >= 2 )
	{
		int			i;
		client_t	*cl;

		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->active && !cl->connected )
				continue;

			if ( !Q_strcasecmp( cl->name, Cmd_Argv( 1 ) ) )
				break;
		}

		if ( i < svs.maxclients )
			QueryClientCvarValue( cl->edict, Cmd_Argv( 2 ) );
	}
	else
	{
		Con_Printf( "%s <player name> <cvar>\n", Cmd_Argv( 0 ) );
	}
}

void QueryClientCvarValueCmd2( void )
{
	if ( Cmd_Argc() >= 3 )
	{
		int			i;
		client_t	*cl;
		int			requestID;

		requestID = atoi( Cmd_Argv( 3 ) );

		for ( i = 0, cl = svs.clients; i < svs.maxclients; i++, cl++ )
		{
			if ( !cl->active && !cl->connected )
				continue;

			if ( !Q_strcasecmp( cl->name, Cmd_Argv( 1 ) ) )
				break;
		}

		if ( i < svs.maxclients )
			QueryClientCvarValue2( cl->edict, Cmd_Argv( 2 ), atoi( Cmd_Argv( 3 ) ) );
	}
	else
	{
		Con_Printf( "%s <player name> <cvar> <requestID>\n", Cmd_Argv( 0 ) );
	}
}

void QueryClientCvarValue( const edict_t *player, const char *cvarName )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( player );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		if ( gNewDLLFunctions.pfnCvarValue )
			gNewDLLFunctions.pfnCvarValue( player, "Bad Player" );

		Con_Printf( "tried to QueryClientCvarValue a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	MSG_WriteChar( &client->netchan.message, svc_sendcvarvalue );
	MSG_WriteString( &client->netchan.message, cvarName );
}

void QueryClientCvarValue2( const edict_t *player, const char *cvarName, int requestID )
{
	client_t	*client;
	int			entnum;

	entnum = NUM_FOR_EDICT( player );
	if ( entnum < 1 || entnum > svs.maxclients )
	{
		if ( gNewDLLFunctions.pfnCvarValue2 )
			gNewDLLFunctions.pfnCvarValue2( player, requestID, cvarName, "Bad Player" );

		Con_Printf( "tried to QueryClientCvarValue a non-client\n" );
		return;
	}

	client = &svs.clients[entnum - 1];
	MSG_WriteChar( &client->netchan.message, svc_sendcvarvalue2 );
	MSG_WriteLong( &client->netchan.message, requestID );
	MSG_WriteString( &client->netchan.message, cvarName );
}

int hudCheckParm( char *parm, char **ppnext )
{
	int		i;

	g_engdstAddrs.CheckParm( &parm, &ppnext );

	i = COM_CheckParm( parm );
	if ( ppnext )
	{
		if ( i && i < com_argc - 1 )
			*ppnext = com_argv[i + 1];
		else
			*ppnext = NULL;
	}

	return i;
}

int EngCheckParm( const char *pchCmdLineToken, char **pchNextVal )
{
	return hudCheckParm( (char *)pchCmdLineToken, pchNextVal );
}
