// cmd.c -- Quake script command processing module

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "mem.h"
#include "APIProxy.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "sys.h"

#define	MAX_ARGS		80
#define	MAX_CMD_LINE	1024

cmdalias_t	*cmd_alias;

qboolean	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
void Cmd_Wait_f( void )
{
	cmd_wait = TRUE;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init( void )
{
	SZ_Alloc( "cmd_text", &cmd_text, 8192 * 2 );
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText( char *text )
{
	int		addLen;

	addLen = Q_strlen( text );
	if ( cmd_text.cursize + addLen >= cmd_text.maxsize )
	{
		Con_Printf( "Cbuf_AddText: overflow\n" );
		return;
	}

	SZ_Write( &cmd_text, text, Q_strlen( text ) );
}

/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void Cbuf_InsertText( char *text )
{
	char	*temp;
	int		currLen;
	int		addLen;

	currLen = cmd_text.cursize;
	addLen = Q_strlen( text );
	if ( currLen + addLen >= cmd_text.maxsize )
	{
		Con_Printf( "Cbuf_InsertText: overflow\n" );
		return;
	}

	// copy off any commands still remaining in the exec buffer
	if ( currLen )
	{
		temp = (char *)Z_Malloc( currLen );
		Q_memcpy( temp, cmd_text.data, currLen );
		SZ_Clear( &cmd_text );
	}
	else
		temp = NULL;

	// add the entire text of the file
	Cbuf_AddText( text );

	// add the copied off data
	if ( currLen )
	{
		SZ_Write( &cmd_text, temp, currLen );
		Z_Free( temp );
	}
}

/*
============
Cbuf_InsertTextLines

Like Cbuf_InsertText, but puts the text on lines of its own
============
*/
void Cbuf_InsertTextLines( char *text )
{
	char	*temp;
	int		currLen;
	int		addLen;

	currLen = cmd_text.cursize;
	addLen = Q_strlen( text ) + 2;
	if ( currLen + addLen >= cmd_text.maxsize )
	{
		Con_Printf( "Cbuf_InsertTextLines: overflow\n" );
		return;
	}

	// copy off any commands still remaining in the exec buffer
	if ( currLen )
	{
		temp = (char *)Z_Malloc( currLen );
		Q_memcpy( temp, cmd_text.data, currLen );
		SZ_Clear( &cmd_text );
	}
	else
		temp = NULL;

	// add the entire text of the file
	Cbuf_AddText( "\n" );
	Cbuf_AddText( text );
	Cbuf_AddText( "\n" );

	// add the copied off data
	if ( currLen )
	{
		SZ_Write( &cmd_text, temp, currLen );
		Z_Free( temp );
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute( void )
{
	int		i;
	char	*text;
	char	line[MAX_CMD_LINE];
	int		quotes;
	int		amount;

	amount = 0;

	while ( cmd_text.cursize )
	{
		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for ( i = 0; i < cmd_text.cursize; i++ )
		{
			if ( text[i] == '"' )
				quotes++;
			if ( !( quotes & 1 ) && text[i] == ';' )
				break;	// don't break if inside a quoted string
			if ( text[i] == '\n' )
				break;
		}

		// don't overflow the line buffer
		if ( i < MAX_CMD_LINE )
			amount = i;
		else
			amount = MAX_CMD_LINE - 1;

		Q_memcpy( line, text, amount );
		line[amount] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if ( i == cmd_text.cursize )
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			Q_memcpy( text, text + i, cmd_text.cursize );
		}

		// execute the command line
		Cmd_ExecuteString( line, src_command );

		if ( cmd_wait )
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = FALSE;
			break;
		}
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_StuffCmds_f

Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
===============
*/
void Cmd_StuffCmds_f( void )
{
	int		i, j;
	int		s;
	char	*text, *build, c;

	if ( Cmd_Argc() != 1 )
	{
		Con_Printf( "stuffcmds : execute command line parameters\n" );
		return;
	}

	// build the combined string to parse from
	s = 0;
	for ( i = 1; i < com_argc; i++ )
	{
		if ( !com_argv[i] )
			continue;		// NEXTSTEP nulls out -NXHost
		s += Q_strlen( com_argv[i] ) + 1;
	}
	if ( !s )
		return;

	text = (char *)Z_Malloc( s + 1 );
	text[0] = 0;
	for ( i = 1; i < com_argc; i++ )
	{
		if ( !com_argv[i] )
			continue;		// NEXTSTEP nulls out -NXHost
		strncat( text, com_argv[i], s - strlen( text ) - 1 );
		if ( i != com_argc - 1 )
			Q_strcat( text, " " );
	}

	// pull out the commands
	build = (char *)Z_Malloc( s + 1 );
	build[0] = 0;

	for ( i = 0; i < s - 1; i++ )
	{
		if ( text[i] == '+' )
		{
			i++;

			for ( j = i; ( text[j] != '+' ) && ( text[j] != '-' ) && ( text[j] != 0 ); j++ )
				;

			c = text[j];
			text[j] = 0;

			strncat( build, text + i, s - strlen( build ) - 2 );
			Q_strcat( build, "\n" );
			text[j] = c;
			i = j - 1;
		}
	}

	if ( build[0] )
		Cbuf_InsertText( build );

	Z_Free( text );
	Z_Free( build );
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f( void )
{
	char	*pszFileName;
	char	*pszFileData;
	int		nFileLength;
	char	*pszFirstDot;
	char	*pszLastDot;
	int		nCurrentLen;
	int		nAddLen;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "exec <filename> : execute a script file\n" );
		return;
	}

	pszFileName = Cmd_Argv( 1 );
	if ( !pszFileName )
		return;

	if ( Q_strstr( pszFileName, "\\" ) || Q_strstr( pszFileName, ":" ) || Q_strstr( pszFileName, ".." ) )
	{
		Con_Printf( "exec %s: invalid path.\n", pszFileName );
		return;
	}

	pszFirstDot = strchr( pszFileName, '.' );
	pszLastDot = strrchr( pszFileName, '.' );
	if ( pszFirstDot != pszLastDot )
	{
		Con_Printf( "exec %s: invalid filename.\n", pszFileName );
		return;
	}

	if ( Q_strcmp( COM_FileExtension( pszFileName ), "cfg" ) && Q_strcmp( COM_FileExtension( pszFileName ), "rc" ) )
	{
		Con_Printf( "exec %s: not a .cfg or .rc file\n", pszFileName );
		return;
	}

	pszFileData = (char *)COM_LoadFileForMe( pszFileName, &nFileLength );
	if ( !pszFileData )
	{
		if ( !Q_strstr( pszFileName, "autoexec.cfg" ) &&
			!Q_strstr( pszFileName, "userconfig.cfg" ) &&
			!Q_strstr( pszFileName, "hw/opengl.cfg" ) &&
			!Q_strstr( pszFileName, "joystick.cfg" ) &&
			!Q_strstr( pszFileName, "game.cfg" ) )
		{
			Con_Printf( "couldn't exec %s\n", pszFileName );
		}
		return;
	}

	Con_DPrintf( "execing %s\n", pszFileName );

	nCurrentLen = cmd_text.cursize;
	nAddLen = nFileLength + 2;
	if ( nCurrentLen + nAddLen >= cmd_text.maxsize )
	{
		// too big for the buffer, feed it a line at a time
		char *pszDataPtr = pszFileData;

		while ( 1 )
		{
			Cbuf_Execute();
			pszDataPtr = COM_ParseLine( pszDataPtr );
			if ( Q_strlen( com_token ) <= 0 )
				break;
			Cbuf_InsertTextLines( com_token );
		}
	}
	else
	{
		Cbuf_InsertTextLines( pszFileData );
	}

	Mem_Free( pszFileData );
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
void Cmd_Echo_f( void )
{
	int		i;

	for ( i = 1; i < Cmd_Argc(); i++ )
		Con_Printf( "%s ", Cmd_Argv( i ) );
	Con_Printf( "\n" );
}

char *CopyString( char *in )
{
	char	*out;

	out = (char *)Z_Malloc( Q_strlen( in ) + 1 );
	Q_strcpy( out, in );
	return out;
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
void Cmd_Alias_f( void )
{
	cmdalias_t	*a;
	char		cmd[1024];
	int			i, c;
	char		*s;
	cvar_t		*cv;

	if ( Cmd_Argc() == 1 )
	{
		Con_Printf( "Current alias commands:\n" );
		for ( a = cmd_alias; a; a = a->next )
			Con_Printf( "%s : %s\n", a->name, a->value );
		return;
	}

	s = Cmd_Argv( 1 );
	if ( Q_strlen( s ) >= MAX_ALIAS_NAME )
	{
		Con_Printf( "Alias name is too long\n" );
		return;
	}

	cv = Cvar_FindVar( s );
	if ( cv )
	{
		Con_Printf( "Alias name is invalid\n" );
		return;
	}

	SetCStrikeFlags();
	if ( ( g_bIsCStrike || g_bIsCZero ) &&
		( !_stricmp( s, "cl_autobuy" ) ||
		!_stricmp( s, "cl_rebuy" ) ||
		!_stricmp( s, "special" ) ||
		!_stricmp( s, "_special" ) ||
		!_stricmp( s, "gl_ztrick" ) ||
		!_stricmp( s, "gl_d3dflip" ) ) )
	{
		Con_Printf( "Alias name is invalid\n" );
		return;
	}

	// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for ( i = 2; i < c; i++ )
	{
		strncat( cmd, Cmd_Argv( i ), sizeof( cmd ) - strlen( cmd ) - 2 );
		if ( i != c )
			strncat( cmd, " ", sizeof( cmd ) - strlen( cmd ) - 2 );
	}
	Q_strcat( cmd, "\n" );

	// if the alias already exists, reuse it
	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !Q_strcmp( s, a->name ) )
		{
			if ( !Q_strcmp( a->value, cmd ) )
				return;		// same as the old one
			Z_Free( a->value );
			break;
		}
	}

	if ( !a )
	{
		a = (cmdalias_t *)Z_Malloc( sizeof( cmdalias_t ) );
		a->next = cmd_alias;
		cmd_alias = a;
	}

	Q_strncpy( a->name, s, sizeof( a->name ) - 1 );
	a->name[sizeof( a->name ) - 1] = 0;

	a->value = CopyString( cmd );
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

#define	FCMD_HUD_COMMAND		( 1 << 0 )
#define	FCMD_GAME_COMMAND		( 1 << 1 )
#define	FCMD_WRAPPER_COMMAND	( 1 << 2 )

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = NULL;

cmd_source_t	cmd_source;

cmd_function_t	*cmd_functions;		// possible commands to execute (module.cpp reads it)

cmd_function_t *Cmd_GetFirstCmd( void )
{
	return cmd_functions;
}

/*
============
Cmd_Init
============
*/
void Cmd_Init( void )
{
	//
	// register our commands
	//
	Cmd_AddCommand( "stuffcmds", Cmd_StuffCmds_f );
	Cmd_AddCommand( "exec", Cmd_Exec_f );
	Cmd_AddCommand( "echo", Cmd_Echo_f );
	Cmd_AddCommand( "alias", Cmd_Alias_f );
	Cmd_AddCommand( "cmd", Cmd_ForwardToServer );
	Cmd_AddCommand( "wait", Cmd_Wait_f );
	Cmd_AddCommand( "cmdlist", Cmd_CmdList_f );
}

void Cmd_Shutdown( void )
{
	cmd_functions = NULL;
	cmd_argc = 0;
	memset( cmd_argv, 0, sizeof( cmd_argv ) );
	cmd_args = NULL;
}

/*
============
Cmd_Argc
============
*/
int Cmd_Argc( void )
{
	g_engdstAddrs.Cmd_Argc();

	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char *Cmd_Argv( int arg )
{
	g_engdstAddrs.Cmd_Argv( &arg );

	if ( (unsigned)arg >= (unsigned)cmd_argc )
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
char *Cmd_Args( void )
{
	return cmd_args;
}

/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
void Cmd_TokenizeString( char *text )
{
	int		i;
	int		arglen;

	// clear the args from the last string
	for ( i = 0; i < cmd_argc; i++ )
		Z_Free( cmd_argv[i] );

	cmd_argc = 0;
	cmd_args = NULL;

	while ( 1 )
	{
		// skip whitespace up to a /n
		while ( *text && *text <= ' ' && *text != '\n' )
			text++;

		if ( *text == '\n' )
		{	// a newline seperates commands in the buffer
			text++;
			break;
		}

		if ( !*text )
			return;

		if ( cmd_argc == 1 )
			cmd_args = text;

		text = COM_Parse( text );
		if ( !text )
			return;

		arglen = Q_strlen( com_token ) + 1;
		if ( arglen >= 516 )
			return;

		if ( cmd_argc < MAX_ARGS )
		{
			cmd_argv[cmd_argc] = (char *)Z_Malloc( arglen );
			Q_strcpy( cmd_argv[cmd_argc], com_token );
			cmd_argc++;
		}
	}
}

/*
============
Cmd_FindCmd
============
*/
cmd_function_t *Cmd_FindCmd( char *cmd_name )
{
	cmd_function_t	*cmd;

	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcmp( cmd_name, cmd->name ) )
			return cmd;
	}

	return NULL;
}

/*
============
Cmd_FindCmdPrev
============
*/
cmd_function_t *Cmd_FindCmdPrev( char *cmd_name )
{
	cmd_function_t	*cmd;

	cmd = cmd_functions;
	while ( cmd->next )
	{
		if ( !Q_strcmp( cmd_name, cmd->next->name ) )
			return cmd;
		cmd = cmd->next;
	}

	return NULL;
}

/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand( char *cmd_name, xcommand_t function )
{
	cmd_function_t	*cmd;
	cmd_function_t	*dummy, *c, *n;
	cmd_function_t	dummycmd;

	if ( host_initialized )	// because hunk allocation would get stomped
		Sys_Error( "Cmd_AddCommand after host_initialized" );

	// fail if the command is a variable name
	if ( Cvar_VariableString( cmd_name )[0] )
	{
		Con_Printf( "Cmd_AddCommand: %s already defined as a var\n", cmd_name );
		return;
	}

	// fail if the command already exists
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcmp( cmd_name, cmd->name ) )
		{
			Con_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
			return;
		}
	}

	cmd = (cmd_function_t *)Hunk_Alloc( sizeof( cmd_function_t ) );
	cmd->name = cmd_name;
	if ( !function )
		cmd->function = Cmd_ForwardToServer;
	else
		cmd->function = function;
	cmd->flags = 0;

	// link the command in, keeping the list sorted by name
	dummy = &dummycmd;
	dummy->next = cmd_functions;
	dummy->name = " ";

	c = dummy;
	while ( c->next )
	{
		if ( _stricmp( c->next->name, cmd->name ) > 0 )
		{
			n = c->next;
			c->next = cmd;
			cmd->next = n;
			cmd_functions = dummy->next;
			return;
		}

		c = c->next;
	}

	c->next = cmd;
	cmd->next = NULL;
	cmd_functions = dummy->next;
}

/*
============
Cmd_AddMallocCommand

Adds a command that can be removed again (client dll, game dll, wrapper)
============
*/
void Cmd_AddMallocCommand( char *cmd_name, xcommand_t function, int flag )
{
	cmd_function_t	*cmd;

	// fail if the command is a variable name
	if ( Cvar_VariableString( cmd_name )[0] )
	{
		Con_Printf( "Cmd_AddCommand: %s already defined as a var\n", cmd_name );
		return;
	}

	// fail if the command already exists
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcmp( cmd_name, cmd->name ) )
		{
			Con_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
			return;
		}
	}

	cmd = (cmd_function_t *)Mem_ZeroMalloc( sizeof( cmd_function_t ) );
	cmd->name = cmd_name;
	if ( !function )
		cmd->function = Cmd_ForwardToServer;
	else
		cmd->function = function;
	cmd->flags = flag;

	cmd->next = cmd_functions;
	cmd_functions = cmd;
}

void Cmd_AddHUDCommand( char *cmd_name, xcommand_t function )
{
	Cmd_AddMallocCommand( cmd_name, function, FCMD_HUD_COMMAND );
}

void Cmd_AddWrapperCommand( char *cmd_name, xcommand_t function )
{
	Cmd_AddMallocCommand( cmd_name, function, FCMD_WRAPPER_COMMAND );
}

void Cmd_AddGameCommand( char *cmd_name, xcommand_t function )
{
	Cmd_AddMallocCommand( cmd_name, function, FCMD_GAME_COMMAND );
}

/*
============
Cmd_RemoveMallocedCmds
============
*/
void Cmd_RemoveMallocedCmds( int flag )
{
	cmd_function_t	*pNewList;
	cmd_function_t	*pCmd, *pNext;

	pNewList = NULL;

	pCmd = cmd_functions;
	while ( pCmd )
	{
		pNext = pCmd->next;
		if ( pCmd->flags & flag )
		{
			Mem_Free( pCmd );
		}
		else
		{
			pCmd->next = pNewList;
			pNewList = pCmd;
		}
		pCmd = pNext;
	}

	cmd_functions = pNewList;
}

void Cmd_RemoveHudCmds( void )
{
	Cmd_RemoveMallocedCmds( FCMD_HUD_COMMAND );
}

void Cmd_RemoveGameCmds( void )
{
	Cmd_RemoveMallocedCmds( FCMD_GAME_COMMAND );
}

void Cmd_RemoveWrapperCmds( void )
{
	Cmd_RemoveMallocedCmds( FCMD_WRAPPER_COMMAND );
}

/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists( char *cmd_name )
{
	cmd_function_t	*cmd;

	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcmp( cmd_name, cmd->name ) )
			return TRUE;
	}

	return FALSE;
}

/*
============
Cmd_CompleteCommand
============
*/
char *Cmd_CompleteCommand( char *search, int forward )
{
	static char		lastpartial[256];
	char			partial[256];
	cmd_function_t	*cmd;
	int				len;
	char			*pPartial;

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
		cmd = Cmd_FindCmd( partial );
		if ( cmd )
		{
			if ( forward == 1 )
				cmd = cmd->next;
			else
				cmd = Cmd_FindCmdPrev( cmd->name );

			if ( cmd )
			{
				Q_strncpy( lastpartial, cmd->name, 255 );
				lastpartial[255] = 0;
				return cmd->name;
			}
		}
	}

	// check for a partial match
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strncmp( partial, cmd->name, len ) )
		{
			// an exact match is complete
			if ( Q_strlen( cmd->name ) == len )
			{
				Q_strncpy( lastpartial, cmd->name, 255 );
				lastpartial[255] = 0;
				return cmd->name;
			}

			pPartial = cmd->name;
			break;
		}
	}

	if ( cmd )
	{
		Q_strncpy( lastpartial, cmd->name, 255 );
		lastpartial[255] = 0;
	}

	return pPartial;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void Cmd_ExecuteString( char *text, cmd_source_t src )
{
	cmd_function_t	*cmd;
	cmdalias_t		*a;

	cmd_source = src;
	Cmd_TokenizeString( text );

	// execute the command line
	if ( !Cmd_Argc() )
		return;		// no tokens

	// check functions
	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( !Q_strcasecmp( cmd_argv[0], cmd->name ) )
		{
			cmd->function();

			if ( cls.demorecording && ( cmd->flags & FCMD_HUD_COMMAND ) && !cls.spectator )
			{
				CL_RecordHUDCommand( cmd->name );
			}
			return;
		}
	}

	// check alias
	for ( a = cmd_alias; a; a = a->next )
	{
		if ( !Q_strcasecmp( cmd_argv[0], a->name ) )
		{
			Cbuf_InsertText( a->value );
			return;
		}
	}

	// check cvars
	if ( Cvar_Command() )
		return;

	if ( cls.state == ca_active || cls.state == ca_connected || cls.state == ca_uninitialized )
	{
		Cmd_ForwardToServer();
	}
}

/*
===================
Cmd_ForwardToServerInternal

Sends the entire command line over to the server
===================
*/
int Cmd_ForwardToServerInternal( sizebuf_t *pBuf )
{
	sizebuf_t	tempBuf;
	char		tempData[4096];

	tempBuf.buffername = "Cmd_ForwardToServerInternal::tempBuf";
	tempBuf.flags = FSB_ALLOWOVERFLOW;
	tempBuf.data = (byte *)tempData;
	tempBuf.maxsize = sizeof( tempData );
	tempBuf.cursize = 0;

	if ( cls.state != ca_connected && cls.state != ca_uninitialized && cls.state != ca_active )
	{
		if ( Q_stricmp( Cmd_Argv( 0 ), "setinfo" ) )
		{
			Con_Printf( "Can't \"%s\", not connected\n", Cmd_Argv( 0 ) );
		}
		return 0;
	}

	if ( cls.demoplayback )
		return 0;		// not really connected

	if ( g_bIsDedicatedServer )
		return 0;

	MSG_WriteByte( &tempBuf, clc_stringcmd );

	if ( Q_strcasecmp( Cmd_Argv( 0 ), "cmd" ) )
	{
		SZ_Print( &tempBuf, Cmd_Argv( 0 ) );
		SZ_Print( &tempBuf, " " );
	}

	if ( Cmd_Argc() > 1 )
	{
		SZ_Print( &tempBuf, Cmd_Args() );
	}
	else
	{
		SZ_Print( &tempBuf, "\n" );
	}

	if ( tempBuf.flags & FSB_OVERFLOWED )
		return 0;

	if ( pBuf->cursize + tempBuf.cursize > pBuf->maxsize )
		return 0;

	SZ_Write( pBuf, tempBuf.data, tempBuf.cursize );

	return 1;
}

/*
===================
Cmd_ForwardToServer

===================
*/
void Cmd_ForwardToServer( void )
{
	// "cmd dlfile" is not forwarded
	if ( !Q_strcasecmp( Cmd_Argv( 0 ), "cmd" ) && !Q_strcasecmp( Cmd_Argv( 1 ), "dlfile" ) )
		return;

	Cmd_ForwardToServerInternal( &cls.netchan.message );
}

/*
===================
Cmd_ForwardToServerUnreliable

===================
*/
int Cmd_ForwardToServerUnreliable( void )
{
	return Cmd_ForwardToServerInternal( &cls.datagram );
}

/*
================
Cmd_CheckParm

Returns the position (1 to argc-1) in the command's argument list
where the given parameter apears, or 0 if not present
================
*/
int Cmd_CheckParm( char *parm )
{
	int		i;

	if ( !parm )
		Sys_Error( "Cmd_CheckParm: NULL" );

	for ( i = 1; i < Cmd_Argc(); i++ )
	{
		if ( !Q_strcasecmp( parm, Cmd_Argv( i ) ) )
			return i;
	}

	return 0;
}

/*
================
Cmd_CmdList_f

List all the commands, or the ones starting with a partial name
================
*/
void Cmd_CmdList_f( void )
{
	cmd_function_t	*cmd;
	int				iCmds;
	int				iArgs;
	char			*partial;
	int				ipLen;
	FileHandle_t	f;
	qboolean		bLogging;

	iCmds = 0;
	partial = NULL;
	bLogging = FALSE;

	iArgs = Cmd_Argc();
	if ( iArgs >= 2 )
	{
		if ( !Q_strcasecmp( Cmd_Argv( 1 ), "?" ) )
		{
			Con_Printf( "CmdList           : List all commands\nCmdList [Partial] : List commands starting with 'Partial'\nCmdList log [Partial] : Logs commands to file \"cmdlist.txt\" in the gamedir.\n" );
			return;
		}

		if ( !Q_strcasecmp( Cmd_Argv( 1 ), "log" ) )
		{
			char szTemp[256];

			_snprintf( szTemp, sizeof( szTemp ), "cmdlist.txt" );
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
		else
		{
			partial = Cmd_Argv( 1 );
			ipLen = Q_strlen( partial );
		}
	}

	Con_Printf( "Command List\n--------------\n" );

	for ( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if ( partial )
		{
			if ( !Q_strncasecmp( cmd->name, partial, ipLen ) )
			{
				Con_Printf( "%-16.16s\n", cmd->name );
				if ( bLogging )
					FS_FPrintf( f, "%-16.16s\n", cmd->name );
				iCmds++;
			}
		}
		else
		{
			Con_Printf( "%-16.16s\n", cmd->name );
			if ( bLogging )
				FS_FPrintf( f, "%-16.16s\n", cmd->name );
			iCmds++;
		}
	}

	if ( iArgs == 2 && partial && *partial )
		Con_Printf( "--------------\n%3i Commands for [%s]\nCmdList ? for syntax\n", iCmds, partial );
	else
		Con_Printf( "--------------\n%3i Total Commands\nCmdList ? for syntax\n", iCmds );

	if ( bLogging )
		FS_Close( f );
}
