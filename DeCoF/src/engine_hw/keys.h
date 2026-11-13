// keys.h -- key binding and console line editing (keys.c)

#ifndef KEYS_H
#define KEYS_H

#include "keydefs.h"

#define	MAXCMDLINE	256

typedef enum
{
	key_game,
	key_message,
	key_menu
} keydest_t;

extern	keydest_t	key_dest;

extern	char	key_lines[32][MAXCMDLINE];
extern	int		key_linepos;
extern	int		shift_down;
extern	int		edit_line;
extern	int		history_line;

extern	char	*keybindings[256];
extern	qboolean	consolekeys[256];	// if true, can't be rebound while in console
extern	qboolean	menubound[256];		// if true, can't be rebound while in menu
extern	int		keyshift[256];			// key to map to if shift held down in console
extern	int		key_repeats[256];		// if > 1, it is autorepeating
extern	qboolean	keydown[256];
extern	qboolean	keyGameUIdown[256];

extern	char	chat_buffer[];
extern	char	message_type[];
extern	int		chat_bufferlen;

qboolean	CheckForCommand( void );
void	CompleteCommand( void );
void	Key_UpLine( void );
void	Key_Console( int key );
void	Cbuf_ReplaceSemiColons( void );
void	Key_Message( int key );
int		Key_StringToKeynum( char *str );
char	*Key_KeynumToString( int keynum );
void	Key_SetBinding( int keynum, char *binding );
void	Key_Unbind_f( void );
void	Key_Unbindall_f( void );
void	Key_Escape_f( void );
void	Key_Bind_f( void );
int		Key_CountBindings( void );
void	Key_WriteBindings( FileHandle_t f );
const char	*Key_NameForBinding( const char *pBinding );
void	Key_Init( void );
void	Key_EventStub( int key, qboolean down );
void	Key_Event( int key, qboolean down );
void	Key_ClearStates( void );
const char	*Key_BindingForKey( int keynum );
void	Key_Shutdown( void );

#endif // KEYS_H
