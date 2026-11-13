// sv_user.c -- server code for moving users

#include <math.h>
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
#include "../pm_shared/pm_movevars.h"
#include "server.h"
#include "world.h"
#include "client.h"
#include "keys.h"
#include "crc.h"
#include "studio.h"

typedef struct command_s
{
	char	*command;
} command_t;

command_t	clcommands[23] =
{
	"status",
	"god",
	"notarget",
	"fly",
	"name",
	"noclip",
	"kill",
	"pause",
	"spawn",
	"new",
	"sendres",
	"dropclient",
	"kick",
	"ping",
	"dlfile",
	"nextdl",
	"setinfo",
	"showinfo",
	"sendents",
	"fullupdate",
	"setpause",
	"unpause",
	NULL
};

edict_t	*sv_player;

cvar_t	mp_consistency = { "mp_consistency", "1", FCVAR_SERVER };

cvar_t	sv_footsteps = { "mp_footsteps", "1", FCVAR_SERVER };

cvar_t	sv_edgefriction = { "edgefriction", "2", FCVAR_SERVER };
cvar_t	sv_maxspeed = { "sv_maxspeed", "320", FCVAR_SERVER };
cvar_t	sv_accelerate = { "sv_accelerate", "10", FCVAR_SERVER };
cvar_t	sv_rollspeed = { "sv_rollspeed", "200" };
cvar_t	sv_rollangle = { "sv_rollangle", "2.0" };

cvar_t	sv_unlag = { "sv_unlag", "1" };
cvar_t	sv_maxunlag = { "sv_maxunlag", "0.5" };
cvar_t	sv_unlagpush = { "sv_unlagpush", "0.0" };
cvar_t	sv_unlagsamples = { "sv_unlagsamples", "1" };

cvar_t	sv_voicecodec = { "sv_voicecodec", "voice_miles" };
cvar_t	sv_voiceenable = { "sv_voiceenable", "1", FCVAR_ARCHIVE | FCVAR_SERVER };
cvar_t	sv_voicequality = { "sv_voicequality", "3" };

/*
===================
SV_ParseConsistencyResponse

===================
*/
void SV_ParseConsistencyResponse( client_t *pSenderClient )
{
	int			idx;
	int			value;
	int			c;
	int			i;
	resource_t	*r;
	int			dropclient;
	vec3_t		mins, maxs;
	vec3_t		cmins, cmaxs;
	byte		readbuffer[32];
	byte		nullbuffer[32];
	byte		resbuffer[32];
	FORCE_TYPE	ft;
	int			length;

	c = 0;
	dropclient = 0;
	Q_memset( nullbuffer, 0, sizeof( nullbuffer ) );

	length = MSG_ReadShort();
	COM_UnMunge( net_message.data + msg_readcount, length, svs.spawncount );

	MSG_StartBitReading( &net_message );

	while ( MSG_ReadBits( 1 ) )
	{
		idx = MSG_ReadBits( 12 );
		if ( idx < 0 || idx >= sv.num_resources )
		{
			dropclient = -1;
			break;
		}

		r = &sv.resourcelist[idx];
		if ( !( r->ucFlags & RES_CHECKFILE ) )
		{
			dropclient = -1;
			break;
		}

		Q_memcpy( readbuffer, r->rguc_reserved, sizeof( readbuffer ) );
		if ( !Q_memcmp( readbuffer, nullbuffer, sizeof( readbuffer ) ) )
		{
			value = MSG_ReadBits( 32 );

			// Hash is in the first 4 bytes of the md5
			if ( value != *(int *)&r->rgucMD5_hash[0] )
			{
				dropclient = idx + 1;
			}
		}
		else
		{
			MSG_ReadBitData( cmins, sizeof( cmins ) );
			MSG_ReadBitData( cmaxs, sizeof( cmaxs ) );

			Q_memcpy( resbuffer, r->rguc_reserved, sizeof( resbuffer ) );
			COM_UnMunge( resbuffer, sizeof( resbuffer ), svs.spawncount );

			ft = resbuffer[0];

			switch ( ft )
			{
			case force_model_samebounds:
				Q_memcpy( mins, &resbuffer[1], sizeof( mins ) );
				Q_memcpy( maxs, &resbuffer[13], sizeof( maxs ) );

				if ( !VectorCompare( cmins, mins ) || !VectorCompare( cmaxs, maxs ) )
				{
					dropclient = idx + 1;
				}
				break;
			case force_model_specifybounds:
				Q_memcpy( mins, &resbuffer[1], sizeof( mins ) );
				Q_memcpy( maxs, &resbuffer[13], sizeof( maxs ) );

				for ( i = 0; i < 3; i++ )
				{
					if ( cmins[i] < mins[i] )
						break;
					if ( cmaxs[i] > maxs[i] )
						break;
				}

				if ( i < 3 )
				{
					dropclient = idx + 1;
				}
				break;
			case force_model_specifybounds_if_avail:
				Q_memcpy( mins, &resbuffer[1], sizeof( mins ) );
				Q_memcpy( maxs, &resbuffer[13], sizeof( maxs ) );

				if ( cmins[0] != -1.0 || cmins[1] != -1.0 || cmins[2] != -1.0 || cmaxs[0] != -1.0 || cmaxs[1] != -1.0 || cmaxs[2] != -1.0 )
				{
					for ( i = 0; i < 3; i++ )
					{
						if ( cmins[i] < mins[i] )
							break;
						if ( cmaxs[i] > maxs[i] )
							break;
					}

					if ( i < 3 )
					{
						dropclient = idx + 1;
					}
				}
				break;
			default:
				msg_badread = 1;
				dropclient = idx + 1;
				break;
			}
		}

		if ( msg_badread )
			break;

		c++;
	}

	MSG_EndBitReading( &net_message );

	if ( dropclient < 0 || c != sv.num_consistency )
	{
		msg_badread = 1;
		Con_Printf( "SV_ParseConsistencyResponse:  %s:%s sent bad file data\n", host_client->name, NET_AdrToString( host_client->netchan.remote_address ) );
		SV_DropClient( host_client, false, "Bad file data" );
		return;
	}

	if ( dropclient > 0 )
	{
		char	dropmessage[256];

		dropmessage[0] = 0;
		if ( gEntityInterface.pfnInconsistentFile( host_client->edict, sv.resourcelist[dropclient - 1].szFileName, dropmessage ) )
		{
			if ( Q_strlen( dropmessage ) > 0 )
			{
				SV_ClientPrintf( "%s", dropmessage );
			}

			SV_DropClient( host_client, false, "Bad file %s", dropmessage );
		}
	}
	else
	{
		// Reset has_force_unmodified if we have received response from the client
		host_client->has_force_unmodified = false;
	}
}

/*
===================
SV_FileInConsistencyList

===================
*/
qboolean SV_FileInConsistencyList( const char *filename, consistency_t **ppconsist )
{
	int		i;

	i = 0;
	while ( 1 )
	{
		if ( !sv.consistency_list[i].filename )
			break;

		if ( !Q_stricmp( filename, sv.consistency_list[i].filename ) )
		{
			if ( ppconsist )
			{
				*ppconsist = &sv.consistency_list[i];
			}
			return true;
		}

		i++;
	}

	return false;
}

/*
===================
SV_TransferConsistencyInfo

===================
*/
int SV_TransferConsistencyInfo( void )
{
	int				c;
	int				i;
	resource_t		*r;
	char			filename[MAX_PATH];
	consistency_t	*pc;
	vec3_t			mins, maxs;

	c = 0;
	for ( i = 0; i < sv.num_resources; i++ )
	{
		r = &sv.resourcelist[i];
		if ( !r )
			continue;

		if ( r->ucFlags & RES_CHECKFILE )
			continue;	// Already checked by Consistency List

		if ( !SV_FileInConsistencyList( r->szFileName, &pc ) )
			continue;

		r->ucFlags |= RES_CHECKFILE;

		if ( r->type == t_sound )
		{
			_snprintf( filename, sizeof( filename ), "sound/%s", r->szFileName );
		}
		else
		{
			Q_strncpy( filename, r->szFileName, sizeof( filename ) - 1 );
			filename[sizeof( filename ) - 1] = 0;
		}

		MD5_Hash_File( r->rgucMD5_hash, filename, false, false, NULL );

		if ( r->type == t_model )
		{
			switch ( pc->check_type )
			{
			case force_model_samebounds:
				if ( !R_GetStudioBounds( filename, mins, maxs ) )
				{
					Host_Error( "Server unable to get bounds for %s\n", filename );
					return 0;
				}

				Q_memcpy( &r->rguc_reserved[1], mins, sizeof( mins ) );
				Q_memcpy( &r->rguc_reserved[13], maxs, sizeof( maxs ) );

				r->rguc_reserved[0] = pc->check_type;
				COM_Munge( r->rguc_reserved, 32, svs.spawncount );
				break;
			case force_model_specifybounds:
			case force_model_specifybounds_if_avail:
				Q_memcpy( &r->rguc_reserved[1], pc->mins, sizeof( pc->mins ) );
				Q_memcpy( &r->rguc_reserved[13], pc->maxs, sizeof( pc->maxs ) );

				r->rguc_reserved[0] = pc->check_type;
				COM_Munge( r->rguc_reserved, 32, svs.spawncount );
				break;
			}
		}

		c++;
	}

	return c;
}

/*
===================
SV_SendConsistencyList

===================
*/
void SV_SendConsistencyList( sizebuf_t *msg )
{
	int			i;
	resource_t	*r;
	int			lastcheck;
	int			delta;

	lastcheck = 0;

	host_client->has_force_unmodified = false;

	if ( svs.maxclients == 1 || mp_consistency.value == 0 || sv.num_consistency == 0 || host_client->proxy )
	{
		MSG_WriteBits( 0, 1 );
		return;
	}

	host_client->has_force_unmodified = true;

	MSG_WriteBits( 1, 1 );

	for ( i = 0; i < sv.num_resources; i++ )
	{
		r = &sv.resourcelist[i];
		if ( !r )
			continue;

		if ( !( r->ucFlags & RES_CHECKFILE ) )
			continue;

		delta = i - lastcheck;

		MSG_WriteBits( 1, 1 );

		if ( delta < 32 )
		{
			// Write 5 bits index delta
			MSG_WriteBits( 1, 1 );
			MSG_WriteBits( delta, 5 );
		}
		else
		{
			MSG_WriteBits( 0, 1 );
			MSG_WriteBits( i, 10 );
		}

		lastcheck = i;
	}

	MSG_WriteBits( 0, 1 );
}

/*
===================
SV_PreRunCmd

Done before running a player command.  Clears the touch array
===================
*/
void SV_PreRunCmd( void )
{
}

/*
===================
SV_CopyEdictToPhysent

===================
*/
void SV_CopyEdictToPhysent( physent_t *pe, int e, edict_t *check )
{
	pe->info = e;

	pe->origin[0] = check->v.origin[0];
	pe->origin[1] = check->v.origin[1];
	pe->origin[2] = check->v.origin[2];

	if ( pe->info >= 1 && pe->info <= svs.maxclients )
	{
		SV_GetTrueOrigin( pe->info - 1, pe->origin );
		pe->player = pe->info;
	}
	else
	{
		pe->player = 0;
	}

	pe->angles[0] = check->v.angles[0];
	pe->angles[1] = check->v.angles[1];
	pe->angles[2] = check->v.angles[2];

	pe->studiomodel = NULL;

	pe->rendermode = check->v.rendermode;

	if ( check->v.solid == SOLID_BSP )
	{
		pe->model = sv.models[check->v.modelindex];
		Q_strncpy( pe->name, pe->model->name, sizeof( pe->name ) );
		pe->name[sizeof( pe->name ) - 1] = 0;
	}
	else if ( check->v.solid == SOLID_NOT )
	{
		if ( check->v.modelindex )
		{
			pe->model = sv.models[check->v.modelindex];
			Q_strncpy( pe->name, pe->model->name, sizeof( pe->name ) );
			pe->name[sizeof( pe->name ) - 1] = 0;
		}
		else
		{
			pe->model = NULL;
		}
	}
	else if ( check->v.solid == SOLID_BBOX )
	{
		model_t	*pModel;

		pe->model = NULL;

		if ( check->v.modelindex )
			pModel = sv.models[check->v.modelindex];
		else
			pModel = NULL;

		if ( pModel && ( pModel->flags & STUDIO_TRACE_HITBOX ) )
		{
			pe->studiomodel = pModel;
			Q_strncpy( pe->name, pe->studiomodel->name, sizeof( pe->name ) );
			pe->name[sizeof( pe->name ) - 1] = 0;
		}
		else if ( pModel )
		{
			Q_strncpy( pe->name, pModel->name, sizeof( pe->name ) );
			pe->name[sizeof( pe->name ) - 1] = 0;
		}

		pe->mins[0] = check->v.mins[0];
		pe->mins[1] = check->v.mins[1];
		pe->mins[2] = check->v.mins[2];

		pe->maxs[0] = check->v.maxs[0];
		pe->maxs[1] = check->v.maxs[1];
		pe->maxs[2] = check->v.maxs[2];
	}
	else
	{
		pe->model = NULL;

		pe->mins[0] = check->v.mins[0];
		pe->mins[1] = check->v.mins[1];
		pe->mins[2] = check->v.mins[2];

		pe->maxs[0] = check->v.maxs[0];
		pe->maxs[1] = check->v.maxs[1];
		pe->maxs[2] = check->v.maxs[2];

		if ( check->v.classname )
		{
			Q_strncpy( pe->name, pr_strings + check->v.classname, sizeof( pe->name ) );
			pe->name[sizeof( pe->name ) - 1] = 0;
		}
		else
		{
			sprintf( pe->name, "?" );
		}
		pe->name[sizeof( pe->name ) - 1] = 0;
	}

	pe->solid = check->v.solid;
	pe->skin = check->v.skin;
	pe->frame = check->v.frame;
	pe->sequence = check->v.sequence;

	Q_memcpy( &pe->controller[0], &check->v.controller[0], 4 * sizeof( byte ) );
	Q_memcpy( &pe->blending[0], &check->v.blending[0], 2 * sizeof( byte ) );

	pe->movetype = check->v.movetype;
	pe->takedamage = 0;
	pe->blooddecal = 0;

	pe->iuser1 = check->v.iuser1;
	pe->iuser2 = check->v.iuser2;
	pe->iuser3 = check->v.iuser3;
	pe->iuser4 = check->v.iuser4;
	pe->fuser1 = check->v.fuser1;
	pe->fuser2 = check->v.fuser2;
	pe->fuser3 = check->v.fuser3;
	pe->fuser4 = check->v.fuser4;

	pe->vuser1[0] = check->v.vuser1[0];
	pe->vuser1[1] = check->v.vuser1[1];
	pe->vuser1[2] = check->v.vuser1[2];

	pe->vuser2[0] = check->v.vuser2[0];
	pe->vuser2[1] = check->v.vuser2[1];
	pe->vuser2[2] = check->v.vuser2[2];

	pe->vuser3[0] = check->v.vuser3[0];
	pe->vuser3[1] = check->v.vuser3[1];
	pe->vuser3[2] = check->v.vuser3[2];

	pe->vuser4[0] = check->v.vuser4[0];
	pe->vuser4[1] = check->v.vuser4[1];
	pe->vuser4[2] = check->v.vuser4[2];
}

/*
====================
SV_AddLinksToPM_

====================
*/
void SV_AddLinksToPM_( areanode_t *node, float *pmove_mins, float *pmove_maxs )
{
	link_t		*l, *next;
	edict_t		*check;
	int			i;
	physent_t	*pe, *ve;
	float		*fmin, *fmax;
	int			e;

	// touch linked edicts
	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if ( check->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND )
			{
				if ( !( check->v.groupinfo & sv_player->v.groupinfo ) )
					continue;
			}
			else if ( g_groupop == GROUP_OP_NAND )
			{
				if ( check->v.groupinfo & sv_player->v.groupinfo )
					continue;
			}
		}

		// Don't collide with your owner
		if ( check->v.owner == sv_player )
			continue;

		if ( check->v.solid == SOLID_BSP || check->v.solid == SOLID_BBOX || check->v.solid == SOLID_SLIDEBOX || check->v.solid == SOLID_NOT )
		{
			e = NUM_FOR_EDICT( check );

			ve = &pmove->visents[pmove->numvisent];
			pmove->numvisent = pmove->numvisent + 1;

			SV_CopyEdictToPhysent( ve, e, check );

			// Only allow game .dll to set this flag for SOLID_NOT entities
			if ( check->v.solid == SOLID_NOT && ( !check->v.skin || !check->v.modelindex ) )
				continue;

			// Don't bother with clipping against monster clip brushes
			if ( ( check->v.flags & FL_MONSTERCLIP ) && check->v.solid == SOLID_BSP )
				continue;

			if ( check == sv_player )
				continue;

			// Dead players are non-solid
			if ( ( check->v.flags & FL_CLIENT ) && check->v.health <= 0 )
				continue;

			if ( check->v.mins[2] == 0.0 && check->v.maxs[2] == 1.0 )
				continue;

			// Is it a point entity?
			if ( Length( check->v.size ) == 0.0 )
				continue;

			fmin = check->v.absmin;
			fmax = check->v.absmax;

			if ( check->v.flags & FL_CLIENT )
			{
				SV_GetTrueMinMax( e - 1, &fmin, &fmax );
			}

			for ( i = 0; i < 3; i++ )
			{
				if ( fmin[i] > pmove_maxs[i] || fmax[i] < pmove_mins[i] )
					break;
			}

			if ( i != 3 )
				continue;

			if ( check->v.solid == SOLID_NOT && check->v.skin == CONTENTS_LADDER )
			{
				if ( pmove->nummoveent >= MAX_MOVEENTS )
				{
					Con_DPrintf( "SV_AddLinksToPM:  pmove->nummoveent >= MAX_MOVEENTS\n" );
					continue;
				}

				pe = &pmove->moveents[pmove->nummoveent];
				pmove->nummoveent = pmove->nummoveent + 1;
			}
			else
			{
				if ( pmove->numphysent >= MAX_PHYSENTS )
				{
					Con_DPrintf( "SV_AddLinksToPM:  pmove->numphysent >= MAX_PHYSENTS\n" );
					return;
				}

				pe = &pmove->physents[pmove->numphysent];
				pmove->numphysent = pmove->numphysent + 1;
			}

			*pe = *ve;
		}
	}

// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( pmove_maxs[node->axis] > node->dist )
		SV_AddLinksToPM_( node->children[0], pmove_mins, pmove_maxs );
	if ( pmove_mins[node->axis] < node->dist )
		SV_AddLinksToPM_( node->children[1], pmove_mins, pmove_maxs );
}

/*
====================
SV_AddLinksToPM

====================
*/
void SV_AddLinksToPM( areanode_t *node, vec_t *origin )
{
	int		i;
	vec3_t	pmove_mins, pmove_maxs;

	Q_memset( &pmove->physents[0], 0, sizeof( physent_t ) );
	Q_memset( &pmove->visents[0], 0, sizeof( physent_t ) );

	pmove->physents[0].model = sv.worldmodel;
	if ( sv.worldmodel )
	{
		Q_strncpy( pmove->physents[0].name, sv.worldmodel->name, sizeof( pmove->physents[0].name ) );
		pmove->physents[0].name[sizeof( pmove->physents[0].name ) - 1] = 0;
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

	for ( i = 0; i < 3; i++ )
	{
		pmove_mins[i] = origin[i] - 256;
		pmove_maxs[i] = origin[i] + 256;
	}

	SV_AddLinksToPM_( node, pmove_mins, pmove_maxs );
}

/*
===========
SV_PlayerRunPreThink

===========
*/
void SV_PlayerRunPreThink( edict_t *player, float time )
{
	gGlobalVariables.time = time;
	gEntityInterface.pfnPlayerPreThink( player );
}

/*
=============
SV_PlayerRunThink

Runs thinking code if player time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
qboolean SV_PlayerRunThink( edict_t *ent, float frametime, double clienttimebase )
{
	float	thinktime;

	if ( !( ent->v.flags & FL_KILLME ) && !( ent->v.flags & FL_DORMANT ) )
	{
		thinktime = ent->v.nextthink;
		if ( thinktime <= 0 || thinktime > frametime + clienttimebase )
			return TRUE;

		if ( thinktime < clienttimebase )
			thinktime = clienttimebase;	// don't let things stay in the past.
										// it is possible to start that way
										// by a trigger with a local time.
		ent->v.nextthink = 0;
		gGlobalVariables.time = thinktime;
		gEntityInterface.pfnThink( ent );
	}

	if ( ent->v.flags & FL_KILLME )
	{
		ED_Free( ent );
	}

	return !ent->free;
}

/*
=============
SV_CheckMovingGround

=============
*/
void SV_CheckMovingGround( edict_t *player, float frametime )
{
	edict_t	*groundentity;

	if ( player->v.flags & FL_ONGROUND )
	{
		groundentity = player->v.groundentity;
		if ( groundentity && ( groundentity->v.flags & FL_CONVEYOR ) )
		{
			if ( player->v.flags & FL_BASEVELOCITY )
				VectorMA( player->v.basevelocity, groundentity->v.speed, groundentity->v.movedir, player->v.basevelocity );
			else
				VectorScale( groundentity->v.movedir, groundentity->v.speed, player->v.basevelocity );
			player->v.flags |= FL_BASEVELOCITY;
		}
	}

	if ( !( player->v.flags & FL_BASEVELOCITY ) )
	{
		// Apply momentum (add in half of the previous frame of velocity first)
		VectorMA( player->v.velocity, (float)( 1.0 + ( frametime * 0.5 ) ), player->v.basevelocity, player->v.velocity );
		VectorClear( player->v.basevelocity );
	}

	player->v.flags &= ~FL_BASEVELOCITY;
}

/*
=============
SV_ConvertPMTrace

=============
*/
void SV_ConvertPMTrace( trace_t *dest, pmtrace_t *src, edict_t *ent )
{
	dest->allsolid = src->allsolid;
	dest->startsolid = src->startsolid;
	dest->inopen = src->inopen;
	dest->inwater = src->inwater;
	dest->fraction = src->fraction;
	dest->endpos[0] = src->endpos[0];
	dest->endpos[1] = src->endpos[1];
	dest->endpos[2] = src->endpos[2];
	dest->plane.normal[0] = src->plane.normal[0];
	dest->plane.normal[1] = src->plane.normal[1];
	dest->plane.normal[2] = src->plane.normal[2];
	dest->plane.dist = src->plane.dist;
	dest->hitgroup = src->hitgroup;
	dest->ent = ent;
}

/*
=============
SV_ForceFullClientsUpdate

=============
*/
void SV_ForceFullClientsUpdate( void )
{
	int				i;
	client_t		*client;
	unsigned char	data[9216];
	sizebuf_t		msg;

	Q_memset( &msg, 0, sizeof( msg ) );
	msg.buffername = "Force Update";
	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof( data );

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( client != host_client && !client->active && !client->connected && !client->spawned )
			continue;

		SV_FullClientUpdate( client, &msg );
	}

	Con_DPrintf( "Client %s started recording. Send full update.\n", host_client->name );
	Netchan_CreateFragments( TRUE, &host_client->netchan, &msg );
	Netchan_FragSend( &host_client->netchan );
}

/*
===========
SV_RunCmd

===========
*/
void SV_RunCmd( usercmd_t *ucmd, int random_seed )
{
	edict_t		*ent;
	int			i, n;
	trace_t		trace;
	float		frametime;
	usercmd_t	cmd;

	cmd = *ucmd;

	if ( host_client->ignorecmdtime > realtime )
	{
		host_client->cmdtime += ucmd->msec / 1000.0;
		return;
	}

	host_client->ignorecmdtime = 0;

	if ( cmd.msec > 50.0 )
	{
		float	oldmsec;

		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2.0;
		SV_RunCmd( &cmd, random_seed );
		cmd.msec = oldmsec / 2.0;
		cmd.impulse = 0;
		SV_RunCmd( &cmd, random_seed );
		return;
	}

	if ( !host_client->fakeclient )
	{
		SV_SetupMove( host_client );
	}

	gEntityInterface.pfnCmdStart( sv_player, ucmd, random_seed );

	frametime = ucmd->msec * 0.001;

	host_client->svtimebase += frametime;
	host_client->cmdtime += ucmd->msec / 1000.0;

	if ( ucmd->impulse )
	{
		sv_player->v.impulse = ucmd->impulse;

		if ( ucmd->impulse == 204 )
		{
			SV_ForceFullClientsUpdate();
		}
	}

	sv_player->v.clbasevelocity[0] = 0;
	sv_player->v.clbasevelocity[1] = 0;
	sv_player->v.clbasevelocity[2] = 0;

	sv_player->v.button = ucmd->buttons;

	SV_CheckMovingGround( sv_player, frametime );

	pmove->oldangles[0] = sv_player->v.v_angle[0];
	pmove->oldangles[1] = sv_player->v.v_angle[1];
	pmove->oldangles[2] = sv_player->v.v_angle[2];

	if ( !sv_player->v.fixangle )
	{
		sv_player->v.v_angle[0] = ucmd->viewangles[0];
		sv_player->v.v_angle[1] = ucmd->viewangles[1];
		sv_player->v.v_angle[2] = ucmd->viewangles[2];
	}

	sv_player->v.cof_end_extra_guess = ucmd->is_donator;

	SV_PlayerRunPreThink( sv_player, (float)host_client->svtimebase );
	SV_PlayerRunThink( sv_player, frametime, host_client->svtimebase );

	// If conveyor, or think, set basevelocity, then send to client asap too.
	if ( Length( sv_player->v.basevelocity ) > 0.0 )
	{
		VectorCopy( sv_player->v.basevelocity, sv_player->v.clbasevelocity );
	}

	pmove->server = TRUE;
	pmove->multiplayer = svs.maxclients > 1;
	pmove->time = 1000.0 * host_client->svtimebase;
	pmove->usehull = ( sv_player->v.flags & FL_DUCKING ) ? true : false;
	pmove->maxspeed = sv_maxspeed.value;
	pmove->clientmaxspeed = sv_player->v.maxspeed;
	pmove->flDuckTime = sv_player->v.flDuckTime;
	pmove->bInDuck = sv_player->v.bInDuck;
	pmove->flTimeStepSound = sv_player->v.flTimeStepSound;
	pmove->iStepLeft = sv_player->v.iStepLeft;
	pmove->flFallVelocity = sv_player->v.flFallVelocity;
	pmove->flSwimTime = sv_player->v.flSwimTime;
	pmove->oldbuttons = sv_player->v.oldbuttons;

	Q_strncpy( pmove->physinfo, host_client->physinfo, sizeof( pmove->physinfo ) - 1 );
	pmove->physinfo[sizeof( pmove->physinfo ) - 1] = 0;

	VectorCopy( sv_player->v.velocity, pmove->velocity );
	VectorCopy( sv_player->v.movedir, pmove->movedir );
	VectorCopy( sv_player->v.v_angle, pmove->angles );
	VectorCopy( sv_player->v.basevelocity, pmove->basevelocity );
	VectorCopy( sv_player->v.view_ofs, pmove->view_ofs );
	VectorCopy( sv_player->v.punchangle, pmove->punchangle );

	pmove->deadflag = sv_player->v.deadflag;
	pmove->effects = sv_player->v.effects;
	pmove->gravity = sv_player->v.gravity;
	pmove->friction = sv_player->v.friction;
	pmove->spectator = 0;
	pmove->waterjumptime = sv_player->v.teleport_time;

	pmove->cmd = cmd;

	pmove->dead = sv_player->v.health <= 0;
	pmove->movetype = sv_player->v.movetype;
	pmove->flags = sv_player->v.flags;
	pmove->player_index = NUM_FOR_EDICT( sv_player ) - 1;

	pmove->iuser1 = sv_player->v.iuser1;
	pmove->iuser2 = sv_player->v.iuser2;
	pmove->iuser3 = sv_player->v.iuser3;
	pmove->iuser4 = sv_player->v.iuser4;
	pmove->fuser1 = sv_player->v.fuser1;
	pmove->fuser2 = sv_player->v.fuser2;
	pmove->fuser3 = sv_player->v.fuser3;
	pmove->fuser4 = sv_player->v.fuser4;

	VectorCopy( sv_player->v.vuser1, pmove->vuser1 );
	VectorCopy( sv_player->v.vuser2, pmove->vuser2 );
	VectorCopy( sv_player->v.vuser3, pmove->vuser3 );
	VectorCopy( sv_player->v.vuser4, pmove->vuser4 );

	VectorCopy( sv_player->v.origin, pmove->origin );

	SV_AddLinksToPM( sv_areanodes, pmove->origin );

	pmove->frametime = frametime;
	pmove->runfuncs = TRUE;
	pmove->PM_PlaySound = PM_SV_PlaySound;
	pmove->PM_TraceTexture = PM_SV_TraceTexture;
	pmove->PM_PlaybackEventFull = PM_SV_PlaybackEventFull;

	gEntityInterface.pfnPM_Move( pmove, TRUE );

	sv_player->v.deadflag = pmove->deadflag;
	sv_player->v.effects = pmove->effects;
	sv_player->v.teleport_time = pmove->waterjumptime;
	sv_player->v.waterlevel = pmove->waterlevel;
	sv_player->v.watertype = pmove->watertype;
	sv_player->v.flags = pmove->flags;
	sv_player->v.friction = pmove->friction;
	sv_player->v.movetype = pmove->movetype;
	sv_player->v.maxspeed = pmove->clientmaxspeed;
	sv_player->v.iStepLeft = pmove->iStepLeft;

	VectorCopy( pmove->view_ofs, sv_player->v.view_ofs );
	VectorCopy( pmove->movedir, sv_player->v.movedir );
	VectorCopy( pmove->punchangle, sv_player->v.punchangle );

	if ( pmove->onground != -1 )
	{
		sv_player->v.flags |= FL_ONGROUND;
		sv_player->v.groundentity = EDICT_NUM( pmove->physents[pmove->onground].info );
	}
	else
	{
		sv_player->v.flags &= ~FL_ONGROUND;
	}

	VectorCopy( pmove->origin, sv_player->v.origin );
	VectorCopy( pmove->velocity, sv_player->v.velocity );
	VectorCopy( pmove->basevelocity, sv_player->v.basevelocity );

	if ( !sv_player->v.fixangle )
	{
		VectorCopy( pmove->angles, sv_player->v.v_angle );
		VectorCopy( pmove->angles, sv_player->v.angles );
		sv_player->v.angles[PITCH] = -pmove->angles[PITCH] / 3;
	}

	sv_player->v.bInDuck = pmove->bInDuck;
	sv_player->v.flDuckTime = pmove->flDuckTime;
	sv_player->v.flTimeStepSound = pmove->flTimeStepSound;
	sv_player->v.flFallVelocity = pmove->flFallVelocity;
	sv_player->v.flSwimTime = pmove->flSwimTime;
	sv_player->v.oldbuttons = pmove->cmd.buttons;

	sv_player->v.iuser1 = pmove->iuser1;
	sv_player->v.iuser2 = pmove->iuser2;
	sv_player->v.iuser3 = pmove->iuser3;
	sv_player->v.iuser4 = pmove->iuser4;
	sv_player->v.fuser1 = pmove->fuser1;
	sv_player->v.fuser2 = pmove->fuser2;
	sv_player->v.fuser3 = pmove->fuser3;
	sv_player->v.fuser4 = pmove->fuser4;

	VectorCopy( pmove->vuser1, sv_player->v.vuser1 );
	VectorCopy( pmove->vuser2, sv_player->v.vuser2 );
	VectorCopy( pmove->vuser3, sv_player->v.vuser3 );
	VectorCopy( pmove->vuser4, sv_player->v.vuser4 );

	SetMinMaxSize( sv_player, player_mins[pmove->usehull], player_maxs[pmove->usehull], false );

	if ( host_client->edict->v.solid )
	{
		vec3_t		vel;
		pmtrace_t	*tr;

		SV_LinkEdict( sv_player, TRUE );

		VectorCopy( sv_player->v.velocity, vel );

		for ( i = 0; i < pmove->numtouch; i++ )
		{
			n = pmove->physents[pmove->touchindex[i].ent].info;
			ent = EDICT_NUM( n );
			tr = &pmove->touchindex[i];
			SV_ConvertPMTrace( &trace, tr, ent );

			VectorCopy( tr->deltavelocity, sv_player->v.velocity );

			SV_Impact( ent, sv_player, &trace );
		}

		VectorCopy( vel, sv_player->v.velocity );
	}

	gGlobalVariables.time = host_client->svtimebase;
	gGlobalVariables.frametime = frametime;
	gEntityInterface.pfnPlayerPostThink( sv_player );
	gEntityInterface.pfnCmdEnd( sv_player );

	if ( !host_client->fakeclient )
	{
		SV_RestoreMove( host_client );
	}
}

/*
===================
SV_ValidateClientCommand

===================
*/
int SV_ValidateClientCommand( char *pszCommand )
{
	char	*p;
	int		i;

	i = 0;

	COM_Parse( pszCommand );

	p = clcommands[i].command;
	while ( p )
	{
		if ( !Q_strcasecmp( com_token, p ) )
		{
			return 1;
		}

		i++;
		p = clcommands[i].command;
	}

	return 0;
}

/*
===================
SV_CalcClientTime

Determines the current value of the client's latency
===================
*/
float SV_CalcClientTime( client_t *cl )
{
	float			ping;
	int				i;
	int				count;
	float			minping, maxping;
	client_frame_t	*frame;
	int				backtrack;

	ping = 0;
	count = 0;

	backtrack = (int)sv_unlagsamples.value;
	backtrack = max( 1, backtrack );
	backtrack = min( backtrack, min( 16, SV_UPDATE_BACKUP ) );

	for ( i = 0; i < backtrack; i++ )
	{
		frame = &cl->frames[( cl->netchan.incoming_acknowledged - i ) & SV_UPDATE_MASK];
		if ( frame->ping_time > 0 )
		{
			ping += frame->ping_time;
			count++;
		}
	}

	if ( !count )
		return 0;

	ping /= count;

	minping = 9999.0;
	maxping = -9999.0;

	for ( i = 0; i < min( 4, SV_UPDATE_BACKUP ); i++ )
	{
		frame = &cl->frames[( cl->netchan.incoming_acknowledged - i ) & SV_UPDATE_MASK];
		if ( frame->ping_time > 0 )
		{
			if ( frame->ping_time < minping )
				minping = frame->ping_time;

			if ( frame->ping_time > maxping )
				maxping = frame->ping_time;
		}
	}

	if ( maxping >= minping && fabs( maxping - minping ) > 0.2 )
		return 0;

	return ping;
}

/*
===================
SV_ComputeLatency

===================
*/
void SV_ComputeLatency( client_t *cl )
{
	cl->latency = SV_CalcClientTime( cl );
}

/*
===================
SV_UnlagCheckTeleport

===================
*/
int SV_UnlagCheckTeleport( vec_t *v1, vec_t *v2 )
{
	int		i;

	for ( i = 0; i < 3; i++ )
	{
		if ( fabs( v1[i] - v2[i] ) > 128 )
			return 1;
	}

	return 0;
}

typedef struct sv_adjusted_positions_s
{
	int		active;
	int		needrelink;
	vec3_t	neworg;
	vec3_t	oldorg;
	vec3_t	initial_correction_org;
	vec3_t	oldabsmin;
	vec3_t	oldabsmax;
	int		deadflag;
	vec3_t	temp_org;
	int		temp_org_setflag;
} sv_adjusted_positions_t;

static	sv_adjusted_positions_t	truepositions[MAX_CLIENTS];

int			giSkip;
qboolean	nofind;

/*
===================
SV_GetTrueOrigin

===================
*/
void SV_GetTrueOrigin( int player, vec_t *origin )
{
	if ( !host_client->lw || !host_client->lc || !sv_unlag.value || svs.maxclients <= 1 || !host_client->active
		|| player < 0 || player >= svs.maxclients || !truepositions[player].active || !truepositions[player].needrelink )
		return;

	VectorCopy( truepositions[player].oldorg, origin );
}

/*
===================
SV_GetTrueMinMax

===================
*/
void SV_GetTrueMinMax( int player, float **fmin, float **fmax )
{
	if ( !host_client->lw || !host_client->lc || !sv_unlag.value || svs.maxclients <= 1 || !host_client->active
		|| player < 0 || player >= svs.maxclients || !truepositions[player].active || !truepositions[player].needrelink )
		return;

	*fmin = truepositions[player].oldabsmin;
	*fmax = truepositions[player].oldabsmax;
}

/*
===================
SV_FindEntInPack

===================
*/
entity_state_t *SV_FindEntInPack( int index, packet_entities_t *pack )
{
	int		i;

	for ( i = 0; i < pack->num_entities; i++ )
	{
		if ( pack->entities[i].number == index )
			return &pack->entities[i];
	}

	return NULL;
}

/*
===================
SV_SetupMove

Move the other players back to where they were when the client issued the command
===================
*/
void SV_SetupMove( client_t *host_client )
{
	int					i, j;
	int					clindex;
	client_t			*cl;
	float				targettime;
	float				cl_interptime;
	float				correct;
	client_frame_t		*frame = NULL;
	client_frame_t		*pnext;
	int					msg;
	packet_entities_t	*ppack;
	float				frac;
	entity_state_t		*state;
	entity_state_t		*pnextstate;
	vec3_t				origin;
	sv_adjusted_positions_t	*pos;

	Q_memset( truepositions, 0, sizeof( truepositions ) );
	nofind = 1;

	if ( !gEntityInterface.pfnAllowLagCompensation() )
		return;

	if ( sv_unlag.value == 0 || !host_client->lw || !host_client->lc || svs.maxclients <= 1 || !host_client->active )
		return;

	nofind = 0;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		cl = &svs.clients[i];
		if ( cl == host_client || !cl->active )
			continue;

		pos = &truepositions[i];

		VectorCopy( cl->edict->v.origin, pos->oldorg );
		VectorCopy( cl->edict->v.absmin, pos->oldabsmin );
		VectorCopy( cl->edict->v.absmax, pos->oldabsmax );
		pos->active = 1;
	}

	correct = min( 1.5, host_client->latency );

	if ( sv_maxunlag.value )
	{
		if ( sv_maxunlag.value < 0.0 )
		{
			Cvar_SetValue( "sv_maxunlag", 0 );
		}

		correct = min( correct, sv_maxunlag.value );
	}

	cl_interptime = host_client->lastcmd.lerp_msec / 1000.0f;
	cl_interptime = min( 0.1f, cl_interptime );
	cl_interptime = max( host_client->next_messageinterval, cl_interptime );

	targettime = realtime - correct - cl_interptime + sv_unlagpush.value;
	targettime = min( realtime, targettime );

	pnext = NULL;

	for ( i = 0; i < SV_UPDATE_BACKUP; i++ )
	{
		msg = host_client->netchan.outgoing_sequence - 1 - i;
		frame = &host_client->frames[msg & SV_UPDATE_MASK];
		ppack = &frame->entities;

		for ( j = 0; j < ppack->num_entities; j++ )
		{
			state = &ppack->entities[j];

			if ( state->number < 1 || state->number > svs.maxclients )
				continue;

			pos = &truepositions[state->number - 1];
			if ( pos->deadflag )
				continue;

			if ( state->health <= 0 )
				pos->deadflag = 1;

			if ( state->effects & EF_NOINTERP )
				pos->deadflag = 1;

			if ( !pos->temp_org_setflag )
			{
				pos->temp_org_setflag = 1;
			}
			else if ( SV_UnlagCheckTeleport( state->origin, pos->temp_org ) )
			{
				pos->deadflag = 1;
			}

			VectorCopy( state->origin, pos->temp_org );
		}

		if ( targettime > frame->senttime )
			break;

		pnext = frame;
	}

	if ( i >= SV_UPDATE_BACKUP )
	{
		Q_memset( truepositions, 0, sizeof( truepositions ) );
		nofind = 1;
		return;
	}

	if ( targettime - frame->senttime > 1.0 )
	{
		Q_memset( truepositions, 0, sizeof( truepositions ) );
		nofind = 1;
		return;
	}

	ppack = &frame->entities;

	if ( !pnext )
	{
		pnext = frame;
		frac = 0;
	}
	else
	{
		float	dt;

		dt = pnext->senttime - frame->senttime;
		if ( dt != 0 )
		{
			frac = ( targettime - frame->senttime ) / dt;
			frac = min( 1.0, frac );
			frac = max( 0.0, frac );
		}
		else
		{
			frac = 0;
		}
	}

	for ( i = 0; i < ppack->num_entities; i++ )
	{
		state = &ppack->entities[i];

		if ( state->number < 1 || state->number > svs.maxclients )
			continue;

		cl = svs.clients + state->number - 1;
		clindex = state->number - 1;

		if ( cl == host_client )
			continue;

		if ( !cl->active )
			continue;

		pos = &truepositions[clindex];
		if ( pos->deadflag )
			continue;

		if ( !pos->active )
		{
			Con_DPrintf( "tried to store off position of bogus player %i/%s\n", i, cl->name );
			continue;
		}

		pnextstate = SV_FindEntInPack( state->number, &pnext->entities );
		if ( pnextstate )
		{
			vec3_t	delta;

			VectorSubtract( pnextstate->origin, state->origin, delta );
			VectorMA( state->origin, frac, delta, origin );
		}
		else
		{
			VectorCopy( state->origin, origin );
		}

		VectorCopy( origin, pos->neworg );
		VectorCopy( origin, pos->initial_correction_org );

		if ( VectorCompare( origin, cl->edict->v.origin ) )
			continue;

		VectorCopy( origin, cl->edict->v.origin );
		SV_LinkEdict( cl->edict, FALSE );
		pos->needrelink = 1;
	}
}

/*
===================
SV_RestoreMove

===================
*/
void SV_RestoreMove( client_t *host_client )
{
	int			i;
	client_t	*cl;
	sv_adjusted_positions_t	*pos;

	if ( nofind )
	{
		nofind = 0;
		return;
	}

	if ( !gEntityInterface.pfnAllowLagCompensation() )
		return;

	if ( svs.maxclients <= 1 || sv_unlag.value == 0 || !host_client->lw || !host_client->lc || !host_client->active )
		return;

	for ( i = 0; i < svs.maxclients; i++ )
	{
		cl = &svs.clients[i];
		if ( cl == host_client || !cl->active )
			continue;

		pos = &truepositions[i];

		if ( VectorCompare( pos->neworg, pos->oldorg ) )
			continue;

		if ( !pos->needrelink )
			continue;

		if ( !pos->active )
		{
			Con_DPrintf( "SV_RestoreMove:  Tried to restore 'inactive' player %i/%s\n", i, cl->name );
			continue;
		}

		if ( !VectorCompare( pos->initial_correction_org, cl->edict->v.origin ) )
			continue;

		VectorCopy( pos->oldorg, cl->edict->v.origin );
		SV_LinkEdict( cl->edict, FALSE );
	}
}

/*
===================
SV_ParseStringCommand

===================
*/
void SV_ParseStringCommand( client_t *pSenderClient )
{
	char	*s;
	int		ret;

	s = MSG_ReadString();
	ret = SV_ValidateClientCommand( s );
	switch ( ret )
	{
	case 2:
		Cbuf_InsertText( s );
		break;
	case 1:
		Cmd_ExecuteString( s, src_client );
		break;
	default:
		if ( Q_strlen( s ) > 127 )
			s[127] = 0;
		Cmd_TokenizeString( s );
		gEntityInterface.pfnClientCommand( sv_player );
		break;
	}
}

/*
===================
SV_ParseDelta

===================
*/
void SV_ParseDelta( client_t *pSenderClient )
{
	host_client->delta_sequence = MSG_ReadByte();
}

/*
===================
SV_EstablishTimeBase

===================
*/
void SV_EstablishTimeBase( client_t *cl, usercmd_t *cmds, int dropped, int numbackup, int numcmds )
{
	int		i;
	double	runcmd_time;
	double	time_at_end;

	runcmd_time = 0.0;
	time_at_end = sv.time + host_frametime;

	// If we haven't dropped too many packets, then run some commands
	if ( dropped < 24 )
	{
		while ( dropped > numbackup )
		{
			runcmd_time += cl->lastcmd.msec / 1000.0;
			dropped--;
		}

		while ( dropped > 0 )
		{
			int	cmdnum;

			cmdnum = dropped + numcmds - 1;
			runcmd_time += cmds[cmdnum].msec / 1000.0;
			dropped--;
		}
	}

	for ( i = numcmds - 1; i >= 0; i-- )
	{
		runcmd_time += cmds[i].msec / 1000.0;
	}

	cl->svtimebase = time_at_end - runcmd_time;
}

static	qboolean	g_balreadymoved;

/*
===================
SV_ParseMove

===================
*/
void SV_ParseMove( client_t *pSenderClient )
{
	usercmd_t		cmds[CMD_MAXBACKUP];
	usercmd_t		*from, *to;
	int				numbackup = 2;
	int				numcmds;
	int				totalcmds;
	float			packet_loss;
	usercmd_t		cmdNull;
	byte			cbpktchecksum;
	client_frame_t	*frame;
	int				bits;
	int				i;
	int				placeholder;
	byte			cbchecksum;
	int				mlen;
	int				packetLossByte;

	if ( g_balreadymoved )
	{
		msg_badread = 1;
		return;
	}

	g_balreadymoved = 1;

	frame = &host_client->frames[host_client->netchan.incoming_acknowledged & SV_UPDATE_MASK];

	Q_memset( &cmdNull, 0, sizeof( cmdNull ) );

	placeholder = msg_readcount + 1;

	mlen = MSG_ReadByte();
	cbpktchecksum = MSG_ReadByte();

	COM_UnMunge( net_message.data + placeholder + 1, mlen, host_client->netchan.incoming_sequence );

	packetLossByte = MSG_ReadByte();
	pSenderClient->m_bLoopback = ( packetLossByte & 0x80 ) ? true : false;
	packetLossByte &= ~0x80;
	packet_loss = packetLossByte;

	numbackup = MSG_ReadByte();
	numcmds = MSG_ReadByte();

	totalcmds = numbackup + numcmds;
	net_drop -= ( numcmds - 1 );

	if ( totalcmds < 0 || totalcmds >= CMD_MAXBACKUP - 1 )
	{
		Con_Printf( "SV_ReadClientMessage: too many cmds %i sent for %s/%s\n", totalcmds, host_client->name, NET_AdrToString( host_client->netchan.remote_address ) );
		SV_DropClient( host_client, false, "CMD_MAXBACKUP hit" );
		msg_badread = 1;
		return;
	}

	from = &cmdNull;
	for ( i = totalcmds - 1; i >= 0; i-- )
	{
		to = &cmds[i];
		MSG_ReadUsercmd( to, from );
		from = to;
	}

	if ( !sv.active )
		return;

	if ( !host_client->active && !host_client->spawned )
		return;

	if ( msg_badread )
	{
		Con_Printf( "Client %s:%s sent a bogus command packet\n", host_client->name, NET_AdrToString( host_client->netchan.remote_address ) );
		return;
	}

	cbchecksum = COM_BlockSequenceCRCByte( net_message.data + placeholder + 1, msg_readcount - placeholder - 1, host_client->netchan.incoming_sequence );
	if ( cbchecksum != cbpktchecksum )
	{
		Con_DPrintf( "Failed command checksum for %s:%s\n", host_client->name, NET_AdrToString( host_client->netchan.remote_address ) );
		msg_badread = 1;
		return;
	}

	host_client->packet_loss = packet_loss;

	// If we are paused, or we're not running in multiplayer and the console or menu is up, ignore the movement
	if ( sv.paused || ( svs.maxclients <= 1 && key_dest != key_game ) || ( sv_player->v.flags & FL_FROZEN ) )
	{
		for ( i = 0; i < numcmds; i++ )
		{
			cmds[i].msec = 0;
			cmds[i].forwardmove = 0;
			cmds[i].sidemove = 0;
			cmds[i].upmove = 0;
			cmds[i].buttons = 0;

			if ( sv_player->v.flags & FL_FROZEN )
			{
				cmds[i].impulse = 0;
			}

			VectorCopy( sv_player->v.v_angle, cmds[i].viewangles );
		}

		net_drop = 0;
	}
	else
	{
		VectorCopy( cmds[0].viewangles, sv_player->v.v_angle );
	}

	// Only copy the buttons over if not already set (i.e., need to make sure each set of buttons gets processed once)
	bits = cmds[0].buttons;
	sv_player->v.button = cmds[0].buttons;
	sv_player->v.light_level = cmds[0].lightlevel;
	sv_player->v.cof_end_extra_guess = cmds[0].is_donator;

	SV_EstablishTimeBase( host_client, cmds, net_drop, numbackup, numcmds );

	SV_PreRunCmd();

	// Run every command
	if ( net_drop < 24 )
	{
		if ( net_drop > numbackup )
		{
//			Con_Printf( "SV_ParseMove: lost %i cmds\n", net_drop - numbackup );
		}

		while ( net_drop > numbackup )
		{
			SV_RunCmd( &host_client->lastcmd, 0 );
			net_drop--;
		}

		while ( net_drop > 0 )
		{
			int	cmdnum;

			cmdnum = net_drop + numcmds - 1;
			SV_RunCmd( &cmds[cmdnum], host_client->netchan.incoming_sequence - cmdnum );
			net_drop--;
		}
	}

	for ( i = numcmds - 1; i >= 0; i-- )
	{
		SV_RunCmd( &cmds[i], host_client->netchan.incoming_sequence - i );
	}

	// copy the most recent command
	host_client->lastcmd = cmds[0];

	// adjust latency time by 1/2 last client frame since
	// the message probably arrived 1/2 through client's frame loop
	frame->ping_time -= host_client->lastcmd.msec * 0.5 / 1000.0;
	if ( frame->ping_time < 0.0 )
		frame->ping_time = 0;

	if ( sv_player->v.animtime > sv.time + host_frametime )
	{
		sv_player->v.animtime = sv.time + host_frametime;
	}
}

/*
===================
SV_ParseVoiceData

===================
*/
void SV_ParseVoiceData( client_t *cl )
{
	int			i, iClient;
	int			nDataLength;
	char		chReceived[4096];
	client_t	*pDestClient;

	iClient = cl - svs.clients;

	nDataLength = MSG_ReadShort();
	if ( nDataLength > sizeof( chReceived ) )
	{
		Con_DPrintf( "SV_ParseVoiceData: invalid incoming packet.\n" );
		SV_DropClient( cl, false, "Invalid voice data\n" );
		return;
	}

	MSG_ReadBuf( nDataLength, chReceived );
	cl->m_lastvoicetime = sv.time;

	if ( !sv_voiceenable.value )
		return;

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		qboolean	bLocal;
		int			nSendLength;

		pDestClient = &svs.clients[i];

		bLocal = ( pDestClient == cl );

		// Does the game code want cl sending to this client?
		if ( !( cl->m_VoiceStreams[i >> 5] & ( 1 << ( i & 31 ) ) ) && !bLocal )
			continue;

		// Is this a client we can send to?
		if ( !pDestClient->active && !pDestClient->connected && !bLocal )
			continue;

		nSendLength = nDataLength;

		// If loopback is not enabled, don't send them their own voice
		if ( bLocal && !pDestClient->m_bLoopback )
		{
			nSendLength = 0;
		}

		// Is there room to write this data in?
		if ( pDestClient->datagram.cursize + nDataLength + 6 < pDestClient->datagram.maxsize )
		{
			MSG_WriteByte( &pDestClient->datagram, svc_voicedata );
			MSG_WriteByte( &pDestClient->datagram, (byte)iClient );
			MSG_WriteShort( &pDestClient->datagram, nSendLength );
			MSG_WriteBuf( &pDestClient->datagram, nSendLength, chReceived );
		}
	}
}

/*
===================
SV_IgnoreHLTV

===================
*/
void SV_IgnoreHLTV( client_t *cl )
{
}

/*
===================
SV_ParseCvarValue

===================
*/
void SV_ParseCvarValue( client_t *cl )
{
	char	*value;

	value = MSG_ReadString();

	if ( gNewDLLFunctions.pfnCvarValue )
	{
		gNewDLLFunctions.pfnCvarValue( cl->edict, value );
	}

	Con_DPrintf( "Cvar query response: name:%s, value:%s\n", cl->name, value );
}

/*
===================
SV_ParseCvarValue2

===================
*/
void SV_ParseCvarValue2( client_t *cl )
{
	char	*value;
	char	cvarName[255];
	int		requestID;

	requestID = MSG_ReadLong();

	Q_strncpy( cvarName, MSG_ReadString(), sizeof( cvarName ) );
	cvarName[sizeof( cvarName ) - 1] = 0;

	value = MSG_ReadString();

	if ( gNewDLLFunctions.pfnCvarValue2 )
	{
		gNewDLLFunctions.pfnCvarValue2( cl->edict, requestID, cvarName, value );
	}

	Con_DPrintf( "Cvar query response: name:%s, request ID %d, cvar:%s, value:%s\n", cl->name, requestID, cvarName, value );
}

typedef struct clc_func_s
{
	unsigned char	opcode;
	char			*pszname;
	void			( *pfnParse )( client_t * );
} clc_func_t;

static clc_func_t	sv_clcfuncs[] =
{
	{ clc_bad, "clc_bad", NULL },
	{ clc_nop, "clc_nop", NULL },
	{ clc_move, "clc_move", SV_ParseMove },
	{ clc_stringcmd, "clc_stringcmd", SV_ParseStringCommand },
	{ clc_delta, "clc_delta", SV_ParseDelta },
	{ clc_resourcelist, "clc_resourcelist", SV_ParseResourceList },
	{ clc_tmove, "clc_tmove", NULL },
	{ clc_fileconsistency, "clc_fileconsistency", SV_ParseConsistencyResponse },
	{ clc_voicedata, "clc_voicedata", SV_ParseVoiceData },
	{ clc_hltv, "clc_hltv", SV_IgnoreHLTV },
	{ clc_cvarvalue, "clc_cvarvalue", SV_ParseCvarValue },
	{ clc_cvarvalue2, "clc_cvarvalue2", SV_ParseCvarValue2 },
	{ (unsigned char)-1, "End of List", NULL }
};

/*
===================
SV_ExecuteClientMessage

Parse incoming message from client
===================
*/
void SV_ExecuteClientMessage( client_t *cl )
{
	int				c;
	client_frame_t	*frame;

	g_balreadymoved = 0;

	frame = &cl->frames[cl->netchan.incoming_acknowledged & SV_UPDATE_MASK];

	frame->ping_time = realtime - frame->senttime - cl->next_messageinterval;
	if ( frame->senttime == 0.0 )
		frame->ping_time = 0;

	// On first frame ( no senttime ) don't skew ping
	if ( realtime - cl->connection_started < 2.0 && frame->ping_time > 0.0 )
		frame->ping_time = 0;

	SV_ComputeLatency( cl );

	host_client = cl;
	sv_player = host_client->edict;

	cl->delta_sequence = -1;	// no delta unless requested

	pmove = &g_svmove;

	// read optional clientCommand strings
	while ( 1 )
	{
		if ( msg_badread )
		{
			Con_Printf( "SV_ReadClientMessage: badread\n" );
			return;
		}

		c = MSG_ReadByte();
		if ( c == -1 )
			return;

		if ( c < 0 || c > clc_cvarvalue2 )
		{
			Con_Printf( "SV_ReadClientMessage: unknown command char (%d)\n", c );
			SV_DropClient( cl, false, "Bad command character in client command" );
			return;
		}

		if ( sv_clcfuncs[c].pfnParse )
		{
			void	( *func )( client_t * );

			func = sv_clcfuncs[c].pfnParse;
			func( cl );
		}
	}
}

/*
=================
SV_SetPlayer

Sets host_client and sv_player to the player with idnum Cmd_Argv(1)
=================
*/
qboolean SV_SetPlayer( int idnum )
{
	client_t	*cl;
	int			i;

	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		if ( !cl->spawned || !cl->active || !cl->connected )
			continue;

		if ( cl->userid == idnum )
		{
			host_client = cl;
			sv_player = host_client->edict;
			return TRUE;
		}
	}

	Con_Printf( "Userid %i is not on the server\n", idnum );
	return FALSE;
}

/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	Info_Print( Info_Serverinfo() );
}

/*
==================
SV_SendEnts_f

==================
*/
void SV_SendEnts_f( void )
{
	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( host_client->active && host_client->spawned && host_client->connected )
	{
		host_client->fully_connected = TRUE;
	}
}

static	float	s_LastFullUpdate[33];

/*
==================
SV_FullUpdate_f

==================
*/
void SV_FullUpdate_f( void )
{
	int		entIndex;
	float	ltime;

	if ( cmd_source == src_command )
	{
		Cmd_ForwardToServer();
		return;
	}

	if ( host_client->active )
	{
		entIndex = IndexOfEdict( host_client->edict );

		if ( s_LastFullUpdate[entIndex] > sv.time )
			s_LastFullUpdate[entIndex] = 0;

		ltime = sv.time - s_LastFullUpdate[entIndex];
		if ( ltime <= 0 )
			ltime = 0;

		if ( ltime < 0.45 && sv.time > 0.45 )
		{
			Con_DPrintf( "%s is spamming fullupdate: (%f) (%f) (%f)\n", host_client->name, sv.time, s_LastFullUpdate[entIndex], ltime );
			return;
		}

		s_LastFullUpdate[entIndex] = sv.time;
	}

	SV_ForceFullClientsUpdate();
	gEntityInterface.pfnClientCommand( sv_player );
}
