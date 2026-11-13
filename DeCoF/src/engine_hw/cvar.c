// cvar.c -- dynamic variable tracking

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "APIProxy.h"
#include "mathlib.h"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "sv_log.h"

#include <math.h>

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

void Cvar_Init( void )
{
}

void Cvar_Shutdown( void )
{
	cvar_vars = NULL;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar( const char *var_name )
{
	cvar_t	*var;

	g_engdstAddrs.pfnGetCvarPointer( &var_name );

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( !Q_stricmp( var_name, var->name ) )
			return var;
	}

	return NULL;
}

/*
============
Cvar_FindPrevVar
============
*/
cvar_t *Cvar_FindPrevVar( char *var_name )
{
	cvar_t	*var;

	var = cvar_vars;
	while ( var->next )
	{
		if ( !Q_stricmp( var_name, var->next->name ) )
			return var;
		var = var->next;
	}

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue( char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if ( !var )
		return 0;

	return Q_atof( var->string );
}

/*
============
Cvar_VariableInt
============
*/
int Cvar_VariableInt( char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if ( !var )
		return 0;

	return Q_atoi( var->string );
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString( char *var_name )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if ( !var )
		return cvar_null_string;

	return var->string;
}

/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable( char *search, int forward )
{
	static char	lastpartial[256];
	char		partial[256];
	cvar_t		*cvar;
	int			len;
	char		*pPartial;

	pPartial = NULL;

	Q_strncpy( partial, search, 255 );
	partial[255] = 0;
	len = Q_strlen( partial );

	if ( !len )
		return NULL;

	// trim any whitespace from the end of the string
	while ( partial[len - 1] == ' ' )
	{
		partial[len - 1] = 0;
		len--;
	}

	// use the last match unless this is a new search
	if ( !Q_stricmp( partial, lastpartial ) )
	{
		cvar = Cvar_FindVar( partial );
		if ( cvar )
		{
			if ( forward == 1 )
				cvar = cvar->next;
			else
				cvar = Cvar_FindPrevVar( cvar->name );

			if ( cvar )
			{
				Q_strncpy( lastpartial, cvar->name, 255 );
				lastpartial[255] = 0;
				return cvar->name;
			}
		}
	}

	// check for a partial match
	for ( cvar = cvar_vars; cvar; cvar = cvar->next )
	{
		if ( !Q_strncmp( partial, cvar->name, len ) )
		{
			// an exact match is complete
			if ( Q_strlen( cvar->name ) == len )
			{
				Q_strncpy( lastpartial, cvar->name, 255 );
				lastpartial[255] = 0;
				return cvar->name;
			}

			pPartial = cvar->name;
			break;
		}
	}

	if ( cvar )
	{
		Q_strncpy( lastpartial, cvar->name, 255 );
		lastpartial[255] = 0;
	}

	return pPartial;
}

/*
============
Cvar_DirectSet
============
*/
void Cvar_DirectSet( struct cvar_s *var, char *value )
{
	qboolean	changed;
	char		*pszValue = value;
	char		szNew[1024];

	if ( !var || !value )
		return;

	if ( var->flags & FCVAR_PRINTABLEONLY )
	{
		char	*pS_guess;
		char	*pD_guess;

		// Clear out new string
		szNew[0] = 0;

		pS_guess = pszValue;
		pD_guess = szNew;

		// Step through the string, only copying back in characters that are printable
		while ( *pS_guess )
		{
			if ( *pS_guess < 32 || *pS_guess > 126 )
			{
				pS_guess++;
				continue;
			}

			*pD_guess++ = *pS_guess++;
		}

		// Terminate the new string
		*pD_guess = 0;

		// If it's empty, then insert a marker string
		if ( !Q_strlen( szNew ) )
		{
			Q_strcpy( szNew, "empty" );
		}

		// Point the value here.
		pszValue = szNew;
	}

	changed = Q_strcmp( var->string, pszValue );

	if ( var->flags & FCVAR_USERINFO )
	{
		if ( cls.state == ca_dedicated )
		{
			Info_SetValueForKey( Info_Serverinfo(), var->name, pszValue, MAX_SERVERINFO_STRING );
			SV_BroadcastCommand( "fullserverinfo \"%s\"\n", Info_Serverinfo() );
		}

		if ( cls.state != ca_dedicated )
		{
			Info_SetValueForKey( cls.userinfo, var->name, pszValue, MAX_INFO_STRING );

			if ( changed && cls.state >= ca_connected )
			{
				MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
				SZ_Print( &cls.netchan.message, va( "setinfo \"%s\" \"%s\"\n", var->name, pszValue ) );
			}
		}
	}

	if ( ( var->flags & FCVAR_SERVER ) && changed )
	{
		if ( !( var->flags & FCVAR_UNLOGGED ) )
		{
			if ( var->flags & FCVAR_PROTECTED )
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, "***PROTECTED***" );
				SV_BroadcastPrintf( "\"%s\" changed to \"%s\"\n", var->name, "***PROTECTED***" );
			}
			else
			{
				Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, pszValue );
				SV_BroadcastPrintf( "\"%s\" changed to \"%s\"\n", var->name, pszValue );
			}
		}

		{
			char	*szVal;

			if ( var->flags & FCVAR_PROTECTED )
			{
				if ( Q_strlen( pszValue ) > 0 && Q_stricmp( pszValue, "none" ) )
					szVal = "1";
				else
					szVal = "0";
			}
			else
			{
				szVal = pszValue;
			}

			Steam_SetCVar( var->name, szVal );
		}
	}

	Z_Free( var->string );	// free the old value string

	var->string = (char *)Z_Malloc( Q_strlen( pszValue ) + 1 );
	Q_strcpy( var->string, pszValue );
	var->value = Q_atof( var->string );
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( char *var_name, char *value )
{
	cvar_t	*var;

	var = Cvar_FindVar( var_name );
	if ( !var )
	{
		// there is an error in C code if this happens
		Con_DPrintf( "Cvar_Set: variable %s not found\n", var_name );
		return;
	}

	Cvar_DirectSet( var, value );
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( char *var_name, float value )
{
	char	val[32];

	g_engdstAddrs.Cvar_SetValue( &var_name, &value );

	if ( fabs( value - (int)value ) < 0.000001 )
		_snprintf( val, sizeof( val ), "%d", (int)value );
	else
		_snprintf( val, sizeof( val ), "%f", value );

	Cvar_Set( var_name, val );
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable( cvar_t *variable )
{
	char	*oldstr;
	cvar_t	*dummy, *v, *n;
	cvar_t	dummyvar;

	// first check to see if it has allready been defined
	if ( Cvar_FindVar( variable->name ) )
	{
		Con_Printf( "Can't register variable %s, already defined\n", variable->name );
		return;
	}

	// check for overlap with a command
	if ( Cmd_Exists( variable->name ) )
	{
		Con_Printf( "Cvar_RegisterVariable: %s is a command\n", variable->name );
		return;
	}

	// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = (char *)Z_Malloc( Q_strlen( variable->string ) + 1 );
	Q_strcpy( variable->string, oldstr );
	variable->value = Q_atof( variable->string );

	// link the variable in, keeping the list sorted by name
	dummy = &dummyvar;
	dummy->next = cvar_vars;
	dummy->name = " ";

	v = dummy;
	while ( v->next )
	{
		if ( _stricmp( v->next->name, variable->name ) > 0 )
		{
			n = v->next;
			v->next = variable;
			variable->next = n;
			cvar_vars = dummy->next;
			return;
		}

		v = v->next;
	}

	v->next = variable;
	variable->next = NULL;
	cvar_vars = dummy->next;
}

/*
============
Cvar_RemoveHudCvars

Frees the variables the client dll registered.
============
*/
void Cvar_RemoveHudCvars( void )
{
	cvar_t	*pNewList;
	cvar_t	*pCvar, *pNext;

	pNewList = NULL;

	pCvar = cvar_vars;
	while ( pCvar )
	{
		pNext = pCvar->next;
		if ( pCvar->flags & FCVAR_CLIENTDLL )
		{
			Z_Free( pCvar->string );
			Z_Free( pCvar );
		}
		else
		{
			pCvar->next = pNewList;
			pNewList = pCvar;
		}
		pCvar = pNext;
	}

	// put the list back in its original order
	pCvar = pNewList;
	cvar_vars = NULL;
	while ( pCvar )
	{
		pNext = pCvar->next;
		pCvar->next = cvar_vars;
		cvar_vars = pCvar;
		pCvar = pNext;
	}
}

/*
============
Cvar_IsMultipleTokens

Returns the first token when the string holds more or fewer than one token.
============
*/
const char *Cvar_IsMultipleTokens( const char *varname )
{
	qboolean	bret;
	static char	firstToken[516];
	int			tokens;
	char		*name;

	bret = FALSE;
	tokens = 0;
	name = (char *)varname;
	firstToken[0] = 0;

	while ( 1 )
	{
		name = COM_Parse( name );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		if ( !tokens )
		{
			Q_strncpy( firstToken, com_token, sizeof( firstToken ) - 1 );
			firstToken[sizeof( firstToken ) - 1] = 0;
		}

		tokens++;
	}

	bret = ( tokens != 1 );

	return bret ? firstToken : NULL;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command( void )
{
	cvar_t		*v;
	const char	*firstToken = NULL;

	firstToken = Cvar_IsMultipleTokens( Cmd_Argv( 0 ) );
	if ( firstToken )
	{
		v = Cvar_FindVar( firstToken );
		if ( !v )
			return false;

		Con_Printf( "\"%s\" is \"%s\"\n", v->name, v->string );
		return true;
	}

	// check variables
	v = Cvar_FindVar( Cmd_Argv( 0 ) );
	if ( !v )
		return false;

	// perform a variable print or set
	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "\"%s\" is \"%s\"\n", v->name, v->string );
		return true;
	}

	if ( ( v->flags & FCVAR_SPONLY ) && cls.state != ca_dedicated && cls.state != ca_disconnected && cl.maxclients > 1 )
	{
		Con_Printf( "Can't set %s in multiplayer\n", v->name );
		return true;
	}

	Cvar_Set( v->name, Cmd_Argv( 1 ) );
	return true;
}

/*
============
Cvar_WriteVariables

Writes lines containing "variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables( FileHandle_t f )
{
	cvar_t	*var;

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( var->flags & FCVAR_ARCHIVE )
			FS_FPrintf( f, "%s \"%s\"\n", var->name, var->string );
	}
}

/*
============
Cmd_CvarListPrintCvar
============
*/
void Cmd_CvarListPrintCvar( cvar_t *var, FileHandle_t f )
{
	char	szOutstr[256];

	if ( var->value == (int)var->value )
		_snprintf( szOutstr, sizeof( szOutstr ) - 11, "%-15s : %8i", var->name, (int)var->value );
	else
		_snprintf( szOutstr, sizeof( szOutstr ) - 11, "%-15s : %8.3f", var->name, var->value );

	if ( var->flags & FCVAR_ARCHIVE )
		Q_strcat( szOutstr, ", a" );
	if ( var->flags & FCVAR_SERVER )
		Q_strcat( szOutstr, ", sv" );
	if ( var->flags & FCVAR_USERINFO )
		Q_strcat( szOutstr, ", i" );

	Q_strcat( szOutstr, "\n" );

	Con_Printf( "%s", szOutstr );

	if ( f )
		FS_FPrintf( f, "%s", szOutstr );
}

/*
============
Cmd_CvarList_f
============
*/
void Cmd_CvarList_f( void )
{
	cvar_t		*var;
	int			iCvars;
	int			iArgs;
	char		*partial;
	int			ipLen;
	qboolean	bAOnly;
	qboolean	bSOnly;
	char		szTemp[256];
	FileHandle_t	f;
	FileHandle_t	fp;
	qboolean	bLogging;

	iCvars = 0;
	partial = NULL;
	ipLen = 0;
	bAOnly = FALSE;
	bSOnly = FALSE;
	f = NULL;
	fp = NULL;
	bLogging = FALSE;

	iArgs = Cmd_Argc();
	if ( iArgs >= 2 )
	{
		if ( !Q_strcasecmp( Cmd_Argv( 1 ), "?" ) )
		{
			Con_Printf( "CvarList           : List all cvars\nCvarList [Partial] : List cvars starting with 'Partial'\nCvarList log [Partial] : Logs cvars to file \"cvarlist.txt\" in the gamedir.\n" );
			return;
		}

		if ( !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
		{
			int i;
			qboolean found;

			i = 0;
			found = FALSE;
			for ( i = 0; i < 100; i++ )
			{
				_snprintf( szTemp, sizeof( szTemp ), "cvarlist%02d.txt", i );
				COM_FixSlashes( szTemp );
				fp = FS_Open( szTemp, "r" );
				if ( !fp )
				{
					found = TRUE;
					break;
				}
				FS_Close( fp );
			}

			if ( !found )
			{
				Con_Printf( "Can't cvarlist! Too many existing cvarlist output files in the gamedir!\n" );
				return;
			}

			f = FS_Open( szTemp, "wt" );
			if ( f )
			{
				bLogging = TRUE;
			}
			else
			{
				Con_Printf( "Couldn't open [%s] for writing!\n", szTemp );
				return;
			}

			if ( iArgs == 3 )
			{
				partial = Cmd_Argv( 2 );
				ipLen = Q_strlen( partial );
			}
		}
		else if ( !Q_stricmp( Cmd_Argv( 1 ), "-a" ) )
		{
			bAOnly = TRUE;
		}
		else if ( !Q_stricmp( Cmd_Argv( 1 ), "-s" ) )
		{
			bSOnly = TRUE;
		}
		else
		{
			partial = Cmd_Argv( 1 );
			ipLen = Q_strlen( partial );
		}
	}

	Con_Printf( "CVar List\n--------------\n" );

	for ( var = cvar_vars; var; var = var->next )
	{
		if ( bAOnly && !( var->flags & FCVAR_ARCHIVE ) )
			continue;
		if ( bSOnly && !( var->flags & FCVAR_SERVER ) )
			continue;

		if ( partial )
		{
			if ( !Q_strncasecmp( var->name, partial, ipLen ) )
			{
				Cmd_CvarListPrintCvar( var, f );
				iCvars++;
			}
		}
		else
		{
			Cmd_CvarListPrintCvar( var, f );
			iCvars++;
		}
	}

	if ( iArgs == 2 && partial && *partial )
		Con_Printf( "--------------\n%3i CVars for [%s]\nCvarList ? for syntax\n", iCvars, partial );
	else
		Con_Printf( "--------------\n%3i Total CVars\nCvarList ? for syntax\n", iCvars );

	if ( bLogging )
	{
		FS_Close( f );
		Con_Printf( "cvarlist logged to %s\n", szTemp );
	}
}

/*
============
Cvar_CountServerVariables
============
*/
int Cvar_CountServerVariables( void )
{
	int		i;
	cvar_t	*var;

	i = 0;
	for ( var = cvar_vars; var; var = var->next )
	{
		if ( var->flags & FCVAR_SERVER )
			i++;
	}

	return i;
}

/*
============
Cvar_UnlinkExternals

Unlinks the variables an external dll registered.
============
*/
void Cvar_UnlinkExternals( void )
{
	cvar_t	*pVar, **pList;

	pList = &cvar_vars;
	pVar = *pList;

	while ( *pList )
	{
		if ( pVar->flags & FCVAR_EXTDLL )
			*pList = pVar->next;
		else
			pList = &pVar->next;

		pVar = pVar->next;
	}
}

/*
============
Cvar_CmdInit
============
*/
void Cvar_CmdInit( void )
{
	Cmd_AddCommand( "cvarlist", Cmd_CvarList_f );
}
