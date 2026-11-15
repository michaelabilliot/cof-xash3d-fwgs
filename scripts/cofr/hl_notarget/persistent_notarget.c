/*
 * Persistent notarget for Cry of Fear Remixed (single player).
 *
 * The engine's "notarget" command only XORs FL_NOTARGET on the player edict.
 * The flag is lost on every level change (the player edict is rebuilt) and
 * CoF's cutscene code clears it when a cutscene ends. This code is linked into
 * a new section of the hl.dll server proxy by patch_server_notarget.py:
 *
 *  - GetEntityAPI/GetEntityAPI2 exports are redirected here. After hl.dll has
 *    built its table, Think, PlayerPreThink, PlayerPostThink and ClientCommand
 *    are wrapped.
 *  - "cofr_notarget" (the console alias "notarget" points to it) toggles a
 *    sticky state that lives for the whole process. While it is on, the flag
 *    is put back before every entity think and around the player think, so a
 *    cutscene or map change can't leave monsters able to see the player.
 *  - "cof8_notarget" (creative menu) still runs normally; the sticky state
 *    then follows the resulting flag.
 *
 * Multiplayer keeps the engine behaviour (plain toggle on the caller).
 *
 * Built with: cl /c /O1 /GS- /Zl /Oi- /TC  (no CRT, no imports).
 */

#define FL_NOTARGET 0x80

/* edict_t layout (verified against the deployed xash.dll SV_Notarget_f). */
#define ED_FREE( e )  ( *(int *)( (char *)( e ) + 0x000 ))
#define ED_PRIV( e )  ( *(void **)( (char *)( e ) + 0x07c ))
#define ED_FLAGS( e ) ( *(int *)( (char *)( e ) + 0x224 ))

/* enginefuncs_t indices (HLSDK eiface.h). */
#define EF_CVARGETFLOAT      57
#define EF_PENTITYOFENTINDEX 72
#define EF_CLIENTPRINTF      80
#define EF_CMD_ARGV          83

#define PRINT_CONSOLE 0

/* Resolved by the patcher to fixed locations inside hl.dll. */
extern void *hl_engfuncs[];   /* hl.dll's copy of enginefuncs_t */
extern char *hl_gpGlobals;    /* globalvars_t * stored by GiveFnptrsToDll */
extern int hl_GetEntityAPI( void **table, int version );
extern int hl_GetEntityAPI2( void **table, int *version );

typedef void ( *edict_fn )( void *ent );
typedef float ( *cvar_get_float_fn )( const char *name );
typedef void *( *ent_of_index_fn )( int index );
typedef void ( *client_printf_fn )( void *ent, int type, const char *msg );
typedef const char *( *cmd_argv_fn )( int arg );

/* DLL_FUNCTIONS indices. */
#define DF_THINK            2
#define DF_CLIENTCOMMAND    19
#define DF_PLAYERPRETHINK   23
#define DF_PLAYERPOSTTHINK  24

static int      g_sticky;
static edict_fn g_think, g_preThink, g_postThink, g_clientCommand;

static int StrEq( const char *a, const char *b )
{
	if( !a ) return 0;
	while( *a && *a == *b ) { a++; b++; }
	return *a == *b;
}

static int SinglePlayer( void )
{
	return !hl_gpGlobals || *(int *)( hl_gpGlobals + 0x90 ) <= 1; /* maxClients */
}

static int ValidEdict( void *ent )
{
	return ent && !ED_FREE( ent ) && ED_PRIV( ent );
}

static void Enforce( void )
{
	void *player;

	if( !g_sticky || !SinglePlayer( ))
		return;

	player = (( ent_of_index_fn )hl_engfuncs[EF_PENTITYOFENTINDEX] )( 1 );
	if( ValidEdict( player ))
		ED_FLAGS( player ) |= FL_NOTARGET;
}

static void Print( void *ent, const char *msg )
{
	(( client_printf_fn )hl_engfuncs[EF_CLIENTPRINTF] )( ent, PRINT_CONSOLE, msg );
}

static void ToggleNotarget( void *ent )
{
	int on;

	if( !ValidEdict( ent ))
		return;

	on = SinglePlayer( ) ? !g_sticky : !( ED_FLAGS( ent ) & FL_NOTARGET );

	/* Turning it off is always allowed so it can't get stuck on. */
	if( on && (( cvar_get_float_fn )hl_engfuncs[EF_CVARGETFLOAT] )( "sv_cheats" ) == 0.0f )
	{
		Print( ent, "notarget requires sv_cheats 1\n" );
		return;
	}

	if( SinglePlayer( ))
		g_sticky = on;

	if( on )
	{
		ED_FLAGS( ent ) |= FL_NOTARGET;
		Print( ent, SinglePlayer( ) ? "notarget ON (stays on until you type notarget again)\n" : "notarget ON\n" );
	}
	else
	{
		ED_FLAGS( ent ) &= ~FL_NOTARGET;
		Print( ent, "notarget OFF\n" );
	}
}

static void Hook_Think( void *ent )
{
	Enforce( );
	g_think( ent );
}

static void Hook_PlayerPreThink( void *ent )
{
	Enforce( );
	g_preThink( ent );
	Enforce( );
}

static void Hook_PlayerPostThink( void *ent )
{
	Enforce( );
	g_postThink( ent );
	Enforce( );
}

static void Hook_ClientCommand( void *ent )
{
	const char *cmd = (( cmd_argv_fn )hl_engfuncs[EF_CMD_ARGV] )( 0 );

	if( StrEq( cmd, "cofr_notarget" ) || StrEq( cmd, "notarget" ))
	{
		ToggleNotarget( ent );
		return;
	}

	g_clientCommand( ent );

	/* Keep the creative menu's toggle and the sticky state in agreement.
	   Re-read argv: the game code may have tokenized another string. */
	cmd = (( cmd_argv_fn )hl_engfuncs[EF_CMD_ARGV] )( 0 );
	if( StrEq( cmd, "cof8_notarget" ) && SinglePlayer( ) && ValidEdict( ent ))
		g_sticky = ( ED_FLAGS( ent ) & FL_NOTARGET ) != 0;
}

static void Wrap( void **table, int index, edict_fn *saved, edict_fn hook )
{
	if( !table[index] || table[index] == (void *)hook )
		return;
	*saved = ( edict_fn )table[index];
	table[index] = (void *)hook;
}

static void HookTable( void **table )
{
	Wrap( table, DF_THINK, &g_think, Hook_Think );
	Wrap( table, DF_PLAYERPRETHINK, &g_preThink, Hook_PlayerPreThink );
	Wrap( table, DF_PLAYERPOSTTHINK, &g_postThink, Hook_PlayerPostThink );
	Wrap( table, DF_CLIENTCOMMAND, &g_clientCommand, Hook_ClientCommand );
}

int CofrNt_GetEntityAPI( void **table, int version )
{
	int result = hl_GetEntityAPI( table, version );
	if( result )
		HookTable( table );
	return result;
}

int CofrNt_GetEntityAPI2( void **table, int *version )
{
	int result = hl_GetEntityAPI2( table, version );
	if( result )
		HookTable( table );
	return result;
}
