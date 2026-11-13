// keys.c -- key up events are sent even if in console mode

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "zone.h"
#include "APIProxy.h"
#include "keys.h"
#include "vgui_int.h"
#include "mathlib.h"
#include "client.h"
#include "cdll_int.h"

/*

key up events are sent even if in console mode

*/

char	key_lines[32][MAXCMDLINE];
int		key_linepos;
int		shift_down;

int		edit_line;
int		history_line;

keydest_t	key_dest;

char	*keybindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];		// if true, can't be rebound while in menu
int		keyshift[256];			// key to map to if shift held down in console
int		key_repeats[256];		// if > 1, it is autorepeating
qboolean	keydown[256];
qboolean	keyGameUIdown[256];

typedef struct
{
	char	*name;
	int		keynum;
} keyname_t;

keyname_t keynames[] =
{
	{ "TAB", K_TAB },
	{ "ENTER", K_ENTER },
	{ "ESCAPE", K_ESCAPE },
	{ "SPACE", K_SPACE },
	{ "BACKSPACE", K_BACKSPACE },
	{ "UPARROW", K_UPARROW },
	{ "DOWNARROW", K_DOWNARROW },
	{ "LEFTARROW", K_LEFTARROW },
	{ "RIGHTARROW", K_RIGHTARROW },

	{ "ALT", K_ALT },
	{ "CTRL", K_CTRL },
	{ "SHIFT", K_SHIFT },

	{ "F1", K_F1 },
	{ "F2", K_F2 },
	{ "F3", K_F3 },
	{ "F4", K_F4 },
	{ "F5", K_F5 },
	{ "F6", K_F6 },
	{ "F7", K_F7 },
	{ "F8", K_F8 },
	{ "F9", K_F9 },
	{ "F10", K_F10 },
	{ "F11", K_F11 },
	{ "F12", K_F12 },

	{ "INS", K_INS },
	{ "DEL", K_DEL },
	{ "PGDN", K_PGDN },
	{ "PGUP", K_PGUP },
	{ "HOME", K_HOME },
	{ "END", K_END },

	{ "MOUSE1", K_MOUSE1 },
	{ "MOUSE2", K_MOUSE2 },
	{ "MOUSE3", K_MOUSE3 },
	{ "MOUSE4", K_MOUSE4 },
	{ "MOUSE5", K_MOUSE5 },

	{ "JOY1", K_JOY1 },
	{ "JOY2", K_JOY2 },
	{ "JOY3", K_JOY3 },
	{ "JOY4", K_JOY4 },

	{ "AUX1", K_AUX1 },
	{ "AUX2", K_AUX2 },
	{ "AUX3", K_AUX3 },
	{ "AUX4", K_AUX4 },
	{ "AUX5", K_AUX5 },
	{ "AUX6", K_AUX6 },
	{ "AUX7", K_AUX7 },
	{ "AUX8", K_AUX8 },
	{ "AUX9", K_AUX9 },
	{ "AUX10", K_AUX10 },
	{ "AUX11", K_AUX11 },
	{ "AUX12", K_AUX12 },
	{ "AUX13", K_AUX13 },
	{ "AUX14", K_AUX14 },
	{ "AUX15", K_AUX15 },
	{ "AUX16", K_AUX16 },
	{ "AUX17", K_AUX17 },
	{ "AUX18", K_AUX18 },
	{ "AUX19", K_AUX19 },
	{ "AUX20", K_AUX20 },
	{ "AUX21", K_AUX21 },
	{ "AUX22", K_AUX22 },
	{ "AUX23", K_AUX23 },
	{ "AUX24", K_AUX24 },
	{ "AUX25", K_AUX25 },
	{ "AUX26", K_AUX26 },
	{ "AUX27", K_AUX27 },
	{ "AUX28", K_AUX28 },
	{ "AUX29", K_AUX29 },
	{ "AUX30", K_AUX30 },
	{ "AUX31", K_AUX31 },
	{ "AUX32", K_AUX32 },

	{ "KP_HOME", K_KP_HOME },
	{ "KP_UPARROW", K_KP_UPARROW },
	{ "KP_PGUP", K_KP_PGUP },
	{ "KP_LEFTARROW", K_KP_LEFTARROW },
	{ "KP_5", K_KP_5 },
	{ "KP_RIGHTARROW", K_KP_RIGHTARROW },
	{ "KP_END", K_KP_END },
	{ "KP_DOWNARROW", K_KP_DOWNARROW },
	{ "KP_PGDN", K_KP_PGDN },
	{ "KP_ENTER", K_KP_ENTER },
	{ "KP_INS", K_KP_INS },
	{ "KP_DEL", K_KP_DEL },
	{ "KP_SLASH", K_KP_SLASH },
	{ "KP_MINUS", K_KP_MINUS },
	{ "KP_PLUS", K_KP_PLUS },
	{ "CAPSLOCK", K_CAPSLOCK },

	{ "MWHEELUP", K_MWHEELUP },
	{ "MWHEELDOWN", K_MWHEELDOWN },

	{ "PAUSE", K_PAUSE },

	{ "SEMICOLON", ';' },	// because a raw semicolon seperates commands

	{ NULL, 0 }
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/

qboolean CheckForCommand( void )
{
	char	command[128];
	char	*cmd, *s;
	int		i;

	s = key_lines[edit_line] + 1;

	for ( i = 0; i < 127; i++ )
	{
		if ( s[i] <= ' ' )
			break;
		else
			command[i] = s[i];
	}
	command[i] = 0;

	cmd = Cmd_CompleteCommand( command, shift_down ? -1 : 1 );
	if ( !cmd || Q_strcmp( cmd, command ) )
		cmd = Cvar_CompleteVariable( command, shift_down ? -1 : 1 );
	if ( !cmd || Q_strcmp( cmd, command ) )
		return FALSE;		// just a chat message
	return TRUE;
}

void CompleteCommand( void )
{
	char	*cmd, *s;

	s = key_lines[edit_line] + 1;
	if ( *s == '\\' || *s == '/' )
		s++;

	cmd = Cmd_CompleteCommand( s, shift_down ? -1 : 1 );
	if ( !cmd )
		cmd = Cvar_CompleteVariable( s, shift_down ? -1 : 1 );
	if ( cmd )
	{
		key_lines[edit_line][1] = '/';
		_snprintf( key_lines[edit_line] + 2, MAXCMDLINE - 2, "%s", cmd );
		key_linepos = Q_strlen( cmd ) + 2;
		key_lines[edit_line][key_linepos] = ' ';
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
		return;
	}
}

void Key_UpLine( void )
{
	do
	{
		history_line = ( history_line - 1 ) & 31;
	} while ( history_line != edit_line && !key_lines[history_line][1] );

	if ( history_line == edit_line )
		history_line = ( edit_line + 1 ) & 31;

	_snprintf( key_lines[edit_line], MAXCMDLINE, "%s", key_lines[history_line] );
	key_linepos = Q_strlen( key_lines[edit_line] );
}

/*
====================
Key_Console

Interactive line editing and console scrollback.
The console input moved to the VGUI2 console; the old handler is no longer reached.
====================
*/
void Key_Console( int key )
{
	char	*cmd;
	int		i;
	HANDLE	th;
	char	*clipText, *textCopied;

	return;

	if ( key == K_ENTER )
	{
		// backslash text are commands, else chat
		if ( key_lines[edit_line][1] == '\\' || key_lines[edit_line][1] == '/' )
			Cbuf_AddText( key_lines[edit_line] + 2 );	// skip the >
		else if ( CheckForCommand() )
			Cbuf_AddText( key_lines[edit_line] + 1 );	// valid command
		else
		{	// convert to a chat message
			cmd = key_lines[edit_line] + 1;
			Cbuf_AddText( "say " );
			Cbuf_AddText( cmd );
		}

		Cbuf_AddText( "\n" );
		Con_Printf( "%s\n", key_lines[edit_line] );
		edit_line = ( edit_line + 1 ) & 31;
		history_line = edit_line;
		key_lines[edit_line][0] = ']';
		key_linepos = 1;
		return;
	}

	if ( key == K_TAB )
	{	// command completion
		CompleteCommand();
		return;
	}

	if ( key == K_UPARROW )
	{
		do
		{
			history_line = ( history_line - 1 ) & 31;
		} while ( history_line != edit_line && !key_lines[history_line][1] );

		if ( history_line == edit_line )
			history_line = ( edit_line + 1 ) & 31;

		_snprintf( key_lines[edit_line], MAXCMDLINE, "%s", key_lines[history_line] );
		key_linepos = Q_strlen( key_lines[edit_line] );
		return;
	}

	if ( key == K_DOWNARROW )
	{
		if ( history_line == edit_line )
			return;

		do
		{
			history_line = ( history_line + 1 ) & 31;
		} while ( history_line != edit_line && !key_lines[history_line][1] );

		if ( history_line == edit_line )
		{
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
		}
		else
		{
			_snprintf( key_lines[edit_line], MAXCMDLINE, "%s", key_lines[history_line] );
			key_linepos = Q_strlen( key_lines[edit_line] );
		}
		return;
	}

	if ( ( key == 'V' || key == 'v' ) && GetKeyState( VK_CONTROL ) < 0 )
	{
		if ( OpenClipboard( NULL ) )
		{
			th = GetClipboardData( CF_TEXT );
			if ( th )
			{
				clipText = (char *)GlobalLock( th );
				if ( clipText )
				{
					textCopied = (char *)malloc( GlobalSize( th ) + 1 );
					strcpy( textCopied, clipText );
					// substitutes a NULL for every token
					strtok( textCopied, "\n\r\b" );
					i = strlen( textCopied );
					if ( i + key_linepos >= MAXCMDLINE )
						i = MAXCMDLINE - key_linepos;
					if ( i > 0 )
					{
						textCopied[i] = 0;
						strcat( key_lines[edit_line], textCopied );
						key_linepos += i;
					}
					free( textCopied );
				}
				GlobalUnlock( th );
			}
			CloseClipboard();
			return;
		}
	}

	if ( key < 32 || key > 127 )
		return;	// non printable

	if ( key_linepos < MAXCMDLINE - 1 )
	{
		key_lines[edit_line][key_linepos] = key;
		key_linepos++;
		key_lines[edit_line][key_linepos] = 0;
	}
}

//============================================================================

char		chat_buffer[120];
char		message_type[32] = "say";
int			chat_bufferlen;

void Cbuf_ReplaceSemiColons( void )
{
	char	*temp = NULL;

	for ( temp = chat_buffer; temp && *temp; temp++ )
	{
		if ( *temp == ';' )
			*temp = ',';
	}
}

void Key_Message( int key )
{
	if ( key == K_ENTER || key == K_KP_ENTER )
	{
		Cbuf_ReplaceSemiColons();

		Cbuf_AddText( message_type );
		Cbuf_AddText( " \"" );
		Cbuf_AddText( chat_buffer );
		Cbuf_AddText( "\"\n" );

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		Q_strcpy( message_type, "say" );
		return;
	}

	if ( key == K_ESCAPE )
	{
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if ( key < 32 || key > 127 )
		return;	// non printable

	if ( key == K_BACKSPACE )
	{
		if ( chat_bufferlen )
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if ( chat_bufferlen == sizeof( chat_buffer ) )
		return; // all full

	chat_buffer[chat_bufferlen] = key;
	chat_bufferlen++;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum( char *str )
{
	keyname_t	*kn;

	if ( !str || !str[0] )
		return -1;
	if ( !str[1] )
		return str[0];

	for ( kn = keynames; kn->name; kn++ )
	{
		if ( !Q_strcasecmp( str, kn->name ) )
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString( int keynum )
{
	keyname_t	*kn;
	static	char	tinystr[2];

	if ( keynum == -1 )
		return "<KEY NOT FOUND>";
	if ( keynum > 32 && keynum < 127 )
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}

	for ( kn = keynames; kn->name; kn++ )
	{
		if ( keynum == kn->keynum )
			return kn->name;
	}

	return "<UNKNOWN KEYNUM>";
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding( int keynum, char *binding )
{
	char	*newbinding;
	int		l;

	if ( keynum == -1 )
		return;

	// free old bindings
	if ( keybindings[keynum] )
	{
		// exactly the same, don't re-bind and fragment memory
		if ( !Q_strcmp( keybindings[keynum], binding ) )
			return;

		Z_Free( keybindings[keynum] );
		keybindings[keynum] = NULL;
	}

	// allocate memory for new binding
	l = Q_strlen( binding );
	newbinding = (char *)Z_Malloc( l + 1 );
	Q_strcpy( newbinding, binding );
	newbinding[l] = 0;
	keybindings[keynum] = newbinding;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f( void )
{
	int		b;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "unbind <key> : remove commands from a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( b == -1 )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( b == K_ESCAPE )
	{
		Con_Printf( "Can't unbind ESCAPE key\n" );
		return;
	}

	Key_SetBinding( b, "" );
}

void Key_Unbindall_f( void )
{
	int		i;

	for ( i = 0; i < 256; i++ )
	{
		if ( keybindings[i] && i != K_ESCAPE )
			Key_SetBinding( i, "" );
	}

	Key_SetBinding( '~', "toggleconsole" );
	Key_SetBinding( '`', "toggleconsole" );
	Key_SetBinding( K_ESCAPE, "cancelselect" );
}

/*
===================
Key_Escape_f
===================
*/
void Key_Escape_f( void )
{
	if ( giSubState & 0x10 )
	{
		Cbuf_AddText( "disconnect\n" );
		giActive = 2;
		giStateInfo = 2;
		giSubState = 1;
		Cbuf_Execute();
		return;
	}

	switch ( key_dest )
	{
	case key_message:
		Key_Message( K_ESCAPE );
		break;
	case key_game:
		VGuiWrap2_HideGameUI();
		giActive = 2;
		break;
	default:
		Sys_Error( "Bad key_dest" );
	}
}

/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f( void )
{
	int		i, c, b;
	char	cmd[1024];

	c = Cmd_Argc();

	if ( c != 2 && c != 3 )
	{
		Con_Printf( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = Key_StringToKeynum( Cmd_Argv( 1 ) );
	if ( b == -1 )
	{
		Con_Printf( "\"%s\" isn't a valid key\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( c == 2 )
	{
		if ( keybindings[b] )
			Con_Printf( "\"%s\" = \"%s\"\n", Cmd_Argv( 1 ), keybindings[b] );
		else
			Con_Printf( "\"%s\" is not bound\n", Cmd_Argv( 1 ) );
		return;
	}

	if ( b == K_ESCAPE )
	{
		Q_strcpy( cmd, "cancelselect" );
	}
	else
	{
		// copy the rest of the command line
		cmd[0] = 0;		// start out with a null string
		for ( i = 2; i < c; i++ )
		{
			if ( i > 2 )
				strncat( cmd, " ", sizeof( cmd ) - strlen( cmd ) - 1 );
			strncat( cmd, Cmd_Argv( i ), sizeof( cmd ) - strlen( cmd ) - 1 );
		}
	}

	Key_SetBinding( b, cmd );
}

int Key_CountBindings( void )
{
	int		i;
	int		c;

	c = 0;
	for ( i = 0; i < 256; i++ )
	{
		if ( !keybindings[i] )
			continue;
		if ( !*keybindings[i] )
			continue;
		c++;
	}

	return c;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( FileHandle_t f )
{
	int		i;

	for ( i = 0; i < 256; i++ )
	{
		if ( !keybindings[i] )
			continue;
		if ( !*keybindings[i] )
			continue;

		FS_FPrintf( f, "bind \"%s\" \"%s\"\n", Key_KeynumToString( i ), keybindings[i] );
	}
}

/*
============
Key_NameForBinding

Returns the key that a command is bound to
============
*/
const char *Key_NameForBinding( const char *pBinding )
{
	int		i;

	g_engdstAddrs.Key_LookupBinding( &pBinding );

	for ( i = 0; i < 256; i++ )
	{
		if ( keybindings[i] && *keybindings[i] )
		{
			if ( keybindings[i][0] == '+' )
			{
				if ( !Q_strncasecmp( keybindings[i] + 1, pBinding, Q_strlen( pBinding ) ) )
					return Key_KeynumToString( i );
			}
			else
			{
				if ( !Q_strncasecmp( keybindings[i], pBinding, Q_strlen( pBinding ) ) )
					return Key_KeynumToString( i );
			}
		}
	}

	return NULL;
}

/*
===================
Key_Init
===================
*/
void Key_Init( void )
{
	int		i;

	for ( i = 0; i < 32; i++ )
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;

	//
	// init ascii characters in console mode
	//
	for ( i = 32; i < 128; i++ )
		consolekeys[i] = TRUE;
	consolekeys[K_ENTER] = TRUE;
	consolekeys[K_KP_ENTER] = TRUE;
	consolekeys[K_TAB] = TRUE;
	consolekeys[K_LEFTARROW] = TRUE;
	consolekeys[K_KP_LEFTARROW] = TRUE;
	consolekeys[K_RIGHTARROW] = TRUE;
	consolekeys[K_KP_RIGHTARROW] = TRUE;
	consolekeys[K_UPARROW] = TRUE;
	consolekeys[K_KP_UPARROW] = TRUE;
	consolekeys[K_DOWNARROW] = TRUE;
	consolekeys[K_KP_DOWNARROW] = TRUE;
	consolekeys[K_BACKSPACE] = TRUE;
	consolekeys[K_PGUP] = TRUE;
	consolekeys[K_KP_PGUP] = TRUE;
	consolekeys[K_PGDN] = TRUE;
	consolekeys[K_KP_PGDN] = TRUE;
	consolekeys[K_HOME] = TRUE;
	consolekeys[K_KP_HOME] = TRUE;
	consolekeys[K_END] = TRUE;
	consolekeys[K_KP_END] = TRUE;
	consolekeys[K_SHIFT] = TRUE;
	consolekeys[K_ALT] = TRUE;
	consolekeys[K_SPACE] = TRUE;
	consolekeys[K_CTRL] = TRUE;
	consolekeys[K_KP_SLASH] = TRUE;
	consolekeys[K_KP_PLUS] = TRUE;
	consolekeys[K_KP_MINUS] = TRUE;
	consolekeys[K_KP_5] = TRUE;
	consolekeys['`'] = FALSE;
	consolekeys['~'] = FALSE;

	for ( i = 0; i < 256; i++ )
		keyshift[i] = i;
	for ( i = 'a'; i <= 'z'; i++ )
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = TRUE;
	for ( i = 0; i < 12; i++ )
		menubound[K_F1 + i] = TRUE;

	//
	// register our functions
	//
	Cmd_AddCommand( "bind", Key_Bind_f );
	Cmd_AddCommand( "unbind", Key_Unbind_f );
	Cmd_AddCommand( "unbindall", Key_Unbindall_f );
	Cmd_AddCommand( "escape", Key_Escape_f );
}

void Key_EventStub( int key, qboolean down )
{
	if ( !g_fUseDInput )
		Key_Event( key, down );
}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event( int key, qboolean down )
{
	char	*kb;
	char	cmd[1024];

	g_engdstAddrs.Key_Event( &key, &down );

	keydown[key] = down;

	// handle ctrl-alt-del
	if ( keydown[K_CTRL] && keydown[K_ALT] && keydown[K_DEL] )
	{
		Sys_Error( "ctrl-alt-del pressed" );
	}

	// let the client dll see the key first
	if ( key_dest == key_game )
	{
		if ( ( !down && keyGameUIdown[key] ) || down )
		{
			if ( !ClientDLL_Key_Event( down ? 1 : 0, key, keybindings[key] ) )
			{
				if ( !key_repeats[key] )
				{
					if ( down )
						keyGameUIdown[key] = TRUE;
					else
						keyGameUIdown[key] = FALSE;
				}

				if ( !down )
				{
					key_repeats[key] = 0;
					keyGameUIdown[key] = FALSE;
				}
				return;
			}
		}
	}

	if ( !down )
	{
		key_repeats[key] = 0;
		keyGameUIdown[key] = FALSE;
	}

	// update auto-repeat status
	if ( down )
	{
		key_repeats[key]++;

		if ( key != K_BACKSPACE &&
			key != K_PAUSE &&
			key != K_PGUP &&
			key != K_KP_PGUP &&
			key != K_PGDN &&
			key != K_KP_PGDN &&
			key_repeats[key] > 1 )
		{
			return;	// ignore most autorepeats
		}

		if ( key >= 200 && !keybindings[key] )
		{
			Con_Printf( "%s is unbound.\n", Key_KeynumToString( key ) );
		}
	}

	if ( key == K_SHIFT )
	{
		shift_down = down;
	}

	//
	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	//
	if ( !down )
	{
		kb = keybindings[key];
		if ( kb && kb[0] == '+' )
		{
			_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb + 1, key );
			Cbuf_AddText( cmd );
		}

		if ( keyshift[key] != key )
		{
			kb = keybindings[keyshift[key]];
			if ( kb && kb[0] == '+' )
			{
				_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb + 1, key );
				Cbuf_AddText( cmd );
			}
		}
		return;
	}

	if ( ( giSubState & 0x10 ) && key != K_ESCAPE )
		return;

	if ( key_dest == key_game && key == K_ESCAPE && VGui_GameUIKeyPressed() )
		return;

	//
	// during demo playback, most keys bring up the main menu
	//
	if ( cls.demoplayback && !cls.spectator && down && consolekeys[key] && key_dest == key_game )
	{
		Con_ToggleConsole_f();
		return;
	}

	//
	// if not a consolekey, send to the interpreter no matter what mode is
	//
	if ( ( key_dest == key_menu && menubound[key] ) ||
		( key_dest == key_game && ( !con_forcedup || !consolekeys[key] ) ) )
	{
		kb = keybindings[key];
		if ( kb )
		{
			if ( kb[0] == '+' )
			{	// button commands add keynum as a parm
				_snprintf( cmd, sizeof( cmd ), "%s %i\n", kb, key );
				Cbuf_AddText( cmd );
			}
			else
			{
				Cbuf_AddText( kb );
				Cbuf_AddText( "\n" );
			}
		}
		return;
	}

	if ( !down )
		return;		// other systems only care about key down events

	if ( shift_down )
	{
		key = keyshift[key];
	}

	switch ( key_dest )
	{
	case key_message:
		Key_Message( key );
		break;
	case key_game:
		Key_Console( key );
		break;
	default:
		Sys_Error( "Bad key_dest" );
	}
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates( void )
{
	int		i;

	for ( i = 0; i < 256; i++ )
	{
		keydown[i] = FALSE;
		keyGameUIdown[i] = FALSE;
		key_repeats[i] = 0;
	}
}

const char *Key_BindingForKey( int keynum )
{
	if ( keynum < 0 || keynum > 255 )
		return NULL;

	if ( !keybindings[keynum] )
		return NULL;

	return keybindings[keynum];
}

void Key_Shutdown( void )
{
	int		i;

	for ( i = 0; i < 256; i++ )
		keybindings[i] = NULL;
}
