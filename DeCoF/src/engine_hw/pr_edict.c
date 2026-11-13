// pr_edict.c -- entity dictionary

#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvardef.h"
#include "cvar.h"
#include "mem.h"
#include "zone.h"
#include "host.h"
#include "sys.h"
#include "net.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"
#include "world.h"
#include "pr_dlls.h"
#include "filesystem_.h"

void	*Mod_Extradata( model_t *mod );

/*
=================
ED_ClearEdict

Sets everything to NULL
=================
*/
void ED_ClearEdict( edict_t *e )
{
	Q_memset( &e->v, 0, sizeof( e->v ) );
	e->free = FALSE;
	ReleaseEntityDLLFields( e );
	InitEntityDLLFields( e );
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc( void )
{
	int			i;
	edict_t		*e;

	for ( i = svs.maxclients + 1; i < sv.num_edicts; i++ )
	{
		e = &sv.edicts[i];
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if ( e->free && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) )
		{
			ED_ClearEdict( e );
			return e;
		}
	}

	if ( i >= sv.max_edicts )
	{
		if ( !sv.max_edicts )
			Sys_Error( "ED_Alloc: No edicts yet" );
		Sys_Error( "ED_Alloc: no free edicts" );
	}

	sv.num_edicts++;
	e = &sv.edicts[i];

	ED_ClearEdict( e );

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free( edict_t *ed )
{
	if ( ed->free )
		return;

	SV_UnlinkEdict( ed );		// unlink from world bsp

	FreeEntPrivateData( ed );

	ed->free = TRUE;
	ed->serialnumber++;

	ed->v.flags = 0;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	ed->v.scale = 0;
	ed->v.gravity = 0;
	VectorCopy( vec3_origin, ed->v.origin );
	VectorCopy( vec3_origin, ed->v.angles );
	ed->v.nextthink = -1;
	ed->v.solid = 0;

	ed->freetime = sv.time;
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count( void )
{
	int		i;
	edict_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for ( i = 0; i < sv.num_edicts; i++ )
	{
		ent = &sv.edicts[i];
		if ( ent->free )
			continue;
		active++;
		if ( ent->v.solid )
			solid++;
		if ( ent->v.model )
			models++;
		if ( ent->v.movetype == MOVETYPE_STEP )
			step++;
	}

	Con_Printf( "num_edicts:%3i\n", sv.num_edicts );
	Con_Printf( "active    :%3i\n", active );
	Con_Printf( "view      :%3i\n", models );
	Con_Printf( "touch     :%3i\n", solid );
	Con_Printf( "step      :%3i\n", step );
}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_NewString
=============
*/
char *ED_NewString( const char *string )
{
	char	*new_s, *new_p;
	int		i, l;

	l = Q_strlen( string ) + 1;
	new_s = Hunk_Alloc( l );
	new_p = new_s;

	for ( i = 0; i < l; i++ )
	{
		if ( string[i] == '\\' && i < l - 1 )
		{
			i++;
			if ( string[i] == 'n' )
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}

	return new_s;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char *ED_ParseEdict( char *data, edict_t *ent )
{
	qboolean		init;
	char			keyname[256];
	int				n;
	ENTITYINIT		pEntityInit;
	char			*className;
	KeyValueData	kvd;

	// clear it
	if ( ent != sv.edicts )		// hack
		Q_memset( &ent->v, 0, sizeof( ent->v ) );

	InitEntityDLLFields( ent );

	SuckOutClassname( data, ent );

	init = FALSE;
	className = pr_strings + ent->v.classname;

	pEntityInit = GetEntityInit( className );
	if ( pEntityInit )
	{
		pEntityInit( &ent->v );
		init = TRUE;
	}
	else
	{
		className = "custom";
		pEntityInit = GetEntityInit( className );
		if ( pEntityInit )
		{
			pEntityInit( &ent->v );
			kvd.szClassName = className;
			kvd.szKeyName = "customclass";
			kvd.szValue = pr_strings + ent->v.classname;
			kvd.fHandled = FALSE;
			gEntityInterface.pfnKeyValue( ent, &kvd );
			init = TRUE;
		}
		else
		{
			Con_DPrintf( "Can't init %s\n", pr_strings + ent->v.classname );
		}
	}

	// go through all the dictionary pairs
	while ( 1 )
	{
		// parse key
		data = COM_Parse( data );
		if ( com_token[0] == '}' )
			break;
		if ( !data )
			Sys_Error( "ED_ParseEntity: EOF without closing brace" );

		Q_strncpy( keyname, com_token, sizeof( keyname ) - 1 );
		keyname[ sizeof( keyname ) - 1 ] = 0;

		// another hack to fix keynames with trailing spaces
		n = Q_strlen( keyname );
		while ( n && keyname[n - 1] == ' ' )
		{
			keyname[n - 1] = 0;
			n--;
		}

		// parse value
		data = COM_Parse( data );
		if ( !data )
			Sys_Error( "ED_ParseEntity: EOF without closing brace" );

		if ( com_token[0] == '}' )
			Sys_Error( "ED_ParseEntity: closing brace without data" );

		if ( COM_CheckParm( "-steam" ) && svs.maxclients <= 1 && !strcmp( className, "trigger_changelevel" ) && !strcmp( keyname, "map" ) && strcmp( com_token, sv.oldname ) )
		{
			char	szGameDir_guess[MAX_PATH];
			char	szResList_guess[MAX_PATH];

			COM_FileBase( com_gamedir, szGameDir_guess );
			_snprintf( szResList_guess, sizeof( szResList_guess ), "reslists\\%s\\%s.lst", GetCurrentSteamAppName(), com_token );
			szResList_guess[ sizeof( szResList_guess ) - 1 ] = 0;
			FS_HintResourceNeed( szResList_guess, 0 );
		}

		if ( ( pr_strings + ent->v.classname ) && !Q_strcmp( pr_strings + ent->v.classname, com_token ) )
			continue;

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if ( !Q_strcmp( keyname, "angle" ) )
		{
			float	y;

			y = Q_atof( com_token );
			if ( y >= 0 )
			{
				_snprintf( com_token, sizeof( com_token ), "%f %f %f", ent->v.angles[0], y, ent->v.angles[2] );
			}
			else if ( (int)y == -1 )
			{
				_snprintf( com_token, sizeof( com_token ), "-90 0 0" );
			}
			else
			{
				_snprintf( com_token, sizeof( com_token ), "90 0 0" );
			}

			Q_strcpy( keyname, "angles" );
		}

		kvd.szClassName = pr_strings + ent->v.classname;
		kvd.szKeyName = keyname;
		kvd.szValue = com_token;
		kvd.fHandled = FALSE;
		gEntityInterface.pfnKeyValue( ent, &kvd );

		if ( kvd.fHandled )
			continue;

		if ( keyname[0] == '_' )
			continue;
	}

	if ( !init )
	{
		ent->free = TRUE;
		ent->serialnumber++;
	}

	return data;
}

/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile( char *data )
{
	edict_t		*ent;
	int			inhibit;

	ent = NULL;
	inhibit = 0;

	gGlobalVariables.time = sv.time;

	// parse ents
	while ( 1 )
	{
		// parse the opening brace
		data = COM_Parse( data );
		if ( !data )
			break;
		if ( com_token[0] != '{' )
			Sys_Error( "ED_LoadFromFile: found %s when expecting {", com_token );

		if ( !ent )
		{
			ent = sv.edicts;
			ReleaseEntityDLLFields( ent );	// make sure the private data is freed
			InitEntityDLLFields( ent );
		}
		else
		{
			ent = ED_Alloc();
		}

		data = ED_ParseEdict( data, ent );

		// remove things from different skill levels or deathmatch
		if ( ent->free )
			continue;

		if ( deathmatch.value && ( ent->v.spawnflags & SF_NOTINDEATHMATCH ) )
		{
			ED_Free( ent );
			inhibit++;
			continue;
		}

		//
		// immediately call spawn function
		//
		if ( !ent->v.classname )
		{
			Con_Printf( "No classname for:\n" );
			ED_Free( ent );
			continue;
		}

		// look for the spawn function
		if ( gEntityInterface.pfnSpawn( ent ) < 0 || ( ent->v.flags & FL_KILLME ) )
			ED_Free( ent );
	}

	Con_DPrintf( "%i entities inhibited\n", inhibit );
}

/*
===============
PR_Init
===============
*/
void PR_Init( void )
{
}

edict_t *EDICT_NUM( int n )
{
	if ( n < 0 || n >= sv.max_edicts )
		Sys_Error( "EDICT_NUM: bad number %i", n );
	return &sv.edicts[n];
}

int NUM_FOR_EDICT( const edict_t *e )
{
	int		b;

	b = (byte *)e - (byte *)sv.edicts;
	b = b / sizeof( edict_t );

	if ( b < 0 || b >= sv.num_edicts )
		Sys_Error( "NUM_FOR_EDICT: bad pointer" );
	return b;
}

void SuckOutClassname( char *szInputStream, edict_t *pEdict )
{
	char			szKeyName[256];
	KeyValueData	kvd;

	// key
	for ( ;; )
	{
		szInputStream = COM_Parse( szInputStream );
		if ( com_token[0] == '}' )
			return;

		Q_strncpy( szKeyName, com_token, sizeof( szKeyName ) - 1 );
		szKeyName[ sizeof( szKeyName ) - 1 ] = 0;

		// value
		szInputStream = COM_Parse( szInputStream );

		if ( !Q_strcmp( szKeyName, "classname" ) )
			break;
	}

	kvd.szClassName = NULL;
	kvd.szKeyName = szKeyName;
	kvd.szValue = com_token;
	kvd.fHandled = FALSE;
	gEntityInterface.pfnKeyValue( pEdict, &kvd );
	if ( !kvd.fHandled )
		Host_Error( "SuckOutClassname: parse error" );
}

void ReleaseEntityDLLFields( edict_t *pEdict )
{
	FreeEntPrivateData( pEdict );
}

void InitEntityDLLFields( edict_t *pEdict )
{
	pEdict->v.pContainingEntity = pEdict;
}

void *PvAllocEntPrivateData( edict_t *pEdict, int32 cb )
{
	FreeEntPrivateData( pEdict );

	if ( cb <= 0 )
		return NULL;

	pEdict->pvPrivateData = Mem_Calloc( 1, cb );
	return pEdict->pvPrivateData;
}

void *PvEntPrivateData( edict_t *pEdict )
{
	if ( pEdict )
		return pEdict->pvPrivateData;
	return NULL;
}

void FreeEntPrivateData( edict_t *pEdict )
{
	if ( pEdict->pvPrivateData )
	{
		if ( gNewDLLFunctions.pfnOnFreeEntPrivateData )
			gNewDLLFunctions.pfnOnFreeEntPrivateData( pEdict );

		Mem_Free( pEdict->pvPrivateData );
	}
	pEdict->pvPrivateData = NULL;
}

void FreeAllEntPrivateData( void )
{
	int		i;

	for ( i = 0; i < sv.num_edicts; i++ )
		FreeEntPrivateData( &sv.edicts[i] );
}

edict_t *PEntityOfEntOffset( int iEntOffset )
{
	return (edict_t *)( (char *)sv.edicts + iEntOffset );
}

int EntOffsetOfPEntity( const edict_t *pEdict )
{
	return (char *)pEdict - (char *)sv.edicts;
}

int IndexOfEdict( const edict_t *pEdict )
{
	int		index;

	if ( !pEdict )
		return 0;

	index = pEdict - sv.edicts;
	if ( index < 0 || index > sv.max_edicts )
		Sys_Error( "Bad entity in IndexOfEdict()" );
	return index;
}

edict_t *PEntityOfEntIndex( int iEntIndex )
{
	if ( iEntIndex >= 0 && iEntIndex < sv.max_edicts )
	{
		edict_t	*pEdict;

		pEdict = EDICT_NUM( iEntIndex );
		if ( pEdict && !pEdict->free && pEdict->pvPrivateData )
			return pEdict;

		if ( iEntIndex < svs.maxclients && !pEdict->free )
			return pEdict;
	}
	return NULL;
}

const char *SzFromIndex( int iString )
{
	return pr_strings + iString;
}

entvars_t *GetVarsOfEnt( edict_t *pEdict )
{
	return &pEdict->v;
}

edict_t *FindEntityByVars( entvars_t *pvars )
{
	int		i;

	for ( i = 0; i < sv.num_edicts; i++ )
	{
		edict_t	*pEdict = &sv.edicts[i];
		if ( &pEdict->v == pvars )
			return pEdict;
	}
	return NULL;
}

float CVarGetFloat( const char *szVarName )
{
	return Cvar_VariableValue( (char *)szVarName );
}

const char *CVarGetString( const char *szVarName )
{
	return Cvar_VariableString( (char *)szVarName );
}

cvar_t *CVarGetPointer( const char *szVarName )
{
	return Cvar_FindVar( szVarName );
}

void CVarSetFloat( const char *szVarName, float flValue )
{
	Cvar_SetValue( (char *)szVarName, flValue );
}

void CVarSetString( const char *szVarName, const char *szValue )
{
	Cvar_Set( (char *)szVarName, (char *)szValue );
}

void CVarRegister( cvar_t *pCvar )
{
	if ( pCvar )
	{
		pCvar->flags |= FCVAR_EXTDLL;
		Cvar_RegisterVariable( pCvar );
	}
}

int AllocEngineString( const char *szValue )
{
	return ED_NewString( szValue ) - pr_strings;
}

void SaveSpawnParms( edict_t *pEdict )
{
	int			eoffset;
	client_t	*client;

	eoffset = NUM_FOR_EDICT( pEdict );
	if ( eoffset < 1 || eoffset > svs.maxclients )
		Host_Error( "Entity is not a client" );

	// Nothing more for this function even on client-side
	client = &svs.clients[ eoffset - 1 ];
}

void *GetModelPtr( edict_t *pEdict )
{
	if ( !pEdict )
		return NULL;

	return Mod_Extradata( sv.models[ pEdict->v.modelindex ] );
}
