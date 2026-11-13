// SystemWrapper.cpp -- the engine as an IBaseSystem for system modules (HLTV demo player)

extern "C"
{
#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "mathlib.h"
#include "vid.h"
#include "render.h"
#include "event_args.h"

typedef struct client_data_s
{
	// fields that cannot be modified  (ie. have no effect if changed)
	vec3_t origin;

	// fields that can be changed by the cldll
	vec3_t viewangles;
	int		iWeaponBits;
	float	fov;	// field of view
} client_data_t;

extern	float	scr_fov_value;

void	ClientDLL_DemoUpdateClientData( client_data_t *cdat );
void	ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer );
void	CL_QueueEvent( int flags, int index, float delay, event_args_t *pargs );
void	hudWeaponAnim( int iAnim, int body );
void	CL_DemoPlaySound( int channel, char *sample, float attenuation, float volume, int flags, int pitch );
void	FS_GetLocalCopy( char *pszFileName );
void	*VGuiWrap2_GetPanel( void );
}

#include "interface.h"
#include "SystemWrapper.h"

extern IFileSystem *g_pFileSystem;

int COM_BuildNumber( void )
{
	return build_number();
}

void COM_RemoveEvilChars( char *string )
{
	char *c;

	for ( c = string; c && *c; c++ )
	{
		if ( *c == '%' )
			*c = ' ';
		else if ( *c < ' ' )
			*c = ' ';
	}
}

SystemWrapper gSystemWrapper;

void SystemWrapper_Init( void )
{
	gSystemWrapper.Init( &gSystemWrapper, 0, "SystemWrapper" );
}

void SystemWrapper_RunFrame( double time )
{
	gSystemWrapper.RunFrame( time );
}

void SystemWrapper_ShutDown( void )
{
	gSystemWrapper.ShutDown();
}

int SystemWrapper_LoadModule( char *interfacename, char *library, char *instancename )
{
	return gSystemWrapper.GetModule( interfacename, library, instancename ) != NULL;
}

void SystemWrapper_ExecuteString( char *command )
{
	gSystemWrapper.ExecuteString( command );
}

void SystemWrapperCommandForwarder( void )
{
	char cmd[1024];

	strcpy( cmd, Cmd_Argv( 0 ) );

	if ( Cmd_Argc() > 1 )
	{
		strcat( cmd, " " );
		strcat( cmd, Cmd_Args() );
	}

	cmd[sizeof( cmd ) - 1] = 0;

	gSystemWrapper.ExecuteString( cmd );
}

EngineWrapper::EngineWrapper()
{
}

EngineWrapper::~EngineWrapper()
{
}

char *EngineWrapper::GetType()
{
	return ENGINEWRAPPER_INTERFACE_VERSION;
}

char *EngineWrapper::GetStatusLine()
{
	return "No status available.\n";
}

bool EngineWrapper::GetViewOrigin( float *origin )
{
	origin[0] = r_refdef.vieworg[0];
	origin[1] = r_refdef.vieworg[1];
	origin[2] = r_refdef.vieworg[2];

	return true;
}

bool EngineWrapper::GetViewAngles( float *angles )
{
	angles[0] = r_refdef.viewangles[0];
	angles[1] = r_refdef.viewangles[1];
	angles[2] = r_refdef.viewangles[2];

	return true;
}

float EngineWrapper::GetCvarFloat( char *szName )
{
	cvar_t *cv = Cvar_FindVar( szName );

	if ( cv )
		return cv->value;

	return 0;
}

char *EngineWrapper::GetCvarString( char *szName )
{
	cvar_t *cv = Cvar_FindVar( szName );

	if ( cv )
		return cv->string;

	return NULL;
}

void EngineWrapper::SetCvar( char *szName, char *szValue )
{
	Cvar_Set( szName, szValue );
}

void EngineWrapper::Cbuf_AddText( char *text )
{
	::Cbuf_AddText( text );
}

void EngineWrapper::DemoUpdateClientData( client_data_t *cdat )
{
	ClientDLL_DemoUpdateClientData( cdat );
	scr_fov_value = cdat->fov;
}

void EngineWrapper::CL_QueueEvent( int flags, int index, float delay, event_args_t *pargs )
{
	::CL_QueueEvent( flags, index, delay, pargs );
}

void EngineWrapper::HudWeaponAnim( int iAnim, int body )
{
	hudWeaponAnim( iAnim, body );
}

void EngineWrapper::CL_DemoPlaySound( int channel, char *sample, float attenuation, float volume, int flags, int pitch )
{
	::CL_DemoPlaySound( channel, sample, attenuation, volume, flags, pitch );
}

void EngineWrapper::ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer )
{
	::ClientDLL_ReadDemoBuffer( size, buffer );
}

int EngineWrapper::GetTraceEntity()
{
	return 0;
}

SystemWrapper::SystemWrapper()
{
}

SystemWrapper::~SystemWrapper()
{
}

char *SystemWrapper::CheckParam( char *param )
{
	int pos = COM_CheckParm( param );

	if ( pos )
	{
		if ( ( pos + 1 ) < com_argc )
			return com_argv[pos + 1];
		else
			return "";
	}

	return NULL;
}

void SystemWrapper::Errorf( char *fmt, ... )
{
	va_list		argptr;
	static char	string[1024];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Printf( "***** FATAL ERROR *****\n" );
	Printf( "%s", string );
	Printf( "*** STOPPING SYSTEM ***\n" );

	Stop();
}

char *SystemWrapper::GetStatusLine()
{
	return "No status available.\n";
}

bool SystemWrapper::Init( IBaseSystem *system, int serial, char *name )
{
	BaseSystemModule::Init( system, serial, name );

	m_Commands.Init();
	m_Modules.Init();
	m_Libraries.Init();
	m_Listener.Init();

	m_EngineWrapper = new EngineWrapper;
	AddModule( (ISystemModule *)m_EngineWrapper, ENGINEWRAPPER_INTERFACE_VERSION );

	m_State = MODULE_RUNNING;

	return true;
}

void SystemWrapper::Stop()
{
	m_State = MODULE_DISCONNECTED;
}

void SystemWrapper::ShutDown()
{
	m_Listener.Clear();
	m_Commands.Clear( true );

	ISystemModule *module;

	while ( ( module = (ISystemModule *)m_Modules.GetFirst() ) != NULL )
	{
		module->ShutDown();
	}

	library_t *lib;

	while ( ( lib = (library_t *)m_Libraries.RemoveTail() ) != NULL )
	{
		if ( lib->handle )
			Sys_UnloadModule( lib->handle );

		Mem_Free( lib );
	}

	delete m_EngineWrapper;

	Cmd_RemoveWrapperCmds();

	m_State = MODULE_DISCONNECTED;
}

void SystemWrapper::ExecuteCommand( int commandID, char *commandLine )
{
	switch ( commandID )
	{
	case CMD_ID_MODULES:
		CMD_Modules( commandLine );
		break;

	case CMD_ID_LOADMODULE:
		CMD_LoadModule( commandLine );
		break;

	case CMD_ID_UNLOADMODULE:
		CMD_UnloadModule( commandLine );
		break;

	default:
		Printf( "SystemWrapper::ExecuteCommand: unknown command ID %i.\n", commandID );
		break;
	}
}

void SystemWrapper::CMD_LoadModule( char *cmdLine )
{
	TokenLine params( cmdLine );

	if ( params.CountToken() < 2 )
	{
		Printf( "Syntax: loadmodule <module> [<library>] [<name>]\n" );
		return;
	}

	if ( params.CountToken() == 2 )
		GetModule( params.GetToken( 1 ), params.GetToken( 1 ) );
	else if ( params.CountToken() == 3 )
		GetModule( params.GetToken( 1 ), params.GetToken( 2 ) );
	else
		GetModule( params.GetToken( 1 ), params.GetToken( 2 ), params.GetToken( 3 ) );
}

void SystemWrapper::CMD_UnloadModule( char *cmdLine )
{
	ISystemModule *module = NULL;
	TokenLine params( cmdLine );

	if ( params.CountToken() < 2 )
	{
		Printf( "Syntax: unloadmodule <module> [<name>]\n" );
		return;
	}

	if ( params.CountToken() == 2 )
		module = FindModule( params.GetToken( 1 ) );
	else if ( params.CountToken() == 3 )
		module = FindModule( params.GetToken( 1 ), params.GetToken( 2 ) );

	if ( !module )
	{
		Printf( "Module not found.\n" );
		return;
	}

	RemoveModule( module );
}

SystemWrapper::library_t *SystemWrapper::GetLibrary( char *name )
{
	char fixedname[MAX_PATH];

	strncpy( fixedname, name, sizeof( fixedname ) - 1 );
	fixedname[sizeof( fixedname ) - 1] = 0;
	COM_FixSlashes( fixedname );

	library_t *lib = (library_t *)m_Libraries.GetFirst();

	while ( lib )
	{
		if ( !_stricmp( name, lib->name ) )
			return lib;

		lib = (library_t *)m_Libraries.GetNext();
	}

	lib = (library_t *)Mem_Malloc( sizeof( library_t ) );

	if ( !lib )
	{
		DPrintf( "ERROR! System::GetLibrary: out of memory (%s).\n", name );
		return NULL;
	}

	_snprintf( lib->name, sizeof( lib->name ), "%s.dll", fixedname );
	FS_GetLocalCopy( lib->name );

	lib->handle = Sys_LoadModule( lib->name );

	if ( !lib->handle )
	{
		DPrintf( "ERROR! System::GetLibrary: coulnd't load library (%s).\n", lib->name );
		Mem_Free( lib );
		return NULL;
	}

	lib->createInterfaceFn = Sys_GetFactory( lib->handle );

	if ( !lib->createInterfaceFn )
	{
		DPrintf( "ERROR! System::GetLibrary: coulnd't get object factory(%s).\n", lib->name );
		Mem_Free( lib );
		return NULL;
	}

	m_Libraries.Add( lib );
	DPrintf( "Loaded library %s.\n", lib->name );

	return lib;
}

void SystemWrapper::SetTitle( char *text )
{
	Con_Printf( "TODO: SystemWrapper::SetTitle ?\n" );
}

void SystemWrapper::SetStatusLine( char *text )
{
	Con_Printf( "TODO: SystemWrapper::SetStatusLine ?\n" );
}

double SystemWrapper::GetTime()
{
	return m_SystemTime;
}

void SystemWrapper::RedirectOutput( char *buffer, int maxSize )
{
	Con_Printf( "WARNIG! SystemWrapper::RedirectOutput: not implemented.\n" );
}

void SystemWrapper::Printf( char *fmt, ... )
{
	va_list		argptr;
	static char	string[8192];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Con_Printf( "%s", string );
}

void SystemWrapper::RunFrame( double time )
{
	m_SystemTime += time;
	m_Tick++;

	if ( m_State != MODULE_RUNNING )
		return;

	ISystemModule *module = (ISystemModule *)m_Modules.GetFirst();

	while ( module && m_State != MODULE_DISCONNECTED )
	{
		module->RunFrame( m_SystemTime );
		module = (ISystemModule *)m_Modules.GetNext();
	}

	m_LastTime = m_SystemTime;
}

void SystemWrapper::ExecuteFile( char *filename )
{
	char cmd[1024];

	_snprintf( cmd, sizeof( cmd ) - 1, "exec %s\n", filename );
	cmd[sizeof( cmd ) - 1] = 0;

	Cmd_ExecuteString( cmd, src_command );
}

void SystemWrapper::DPrintf( char *fmt, ... )
{
	va_list		argptr;
	static char	string[8192];

	va_start( argptr, fmt );
	_vsnprintf( string, sizeof( string ), fmt, argptr );
	va_end( argptr );

	Con_DPrintf( string );
}

bool SystemWrapper::RegisterCommand( char *name, ISystemModule *module, int commandID )
{
	command_t *cmd = (command_t *)m_Commands.GetFirst();

	while ( cmd && _stricmp( cmd->name, name ) )
		cmd = (command_t *)m_Commands.GetNext();

	if ( cmd )
	{
		Printf( "WARNING! System::RegisterCommand: command \"%s\" already exists.\n", name );
		return false;
	}

	cmd = (command_t *)Mem_ZeroMalloc( sizeof( command_t ) );

	strncpy( cmd->name, name, sizeof( cmd->name ) );
	cmd->name[sizeof( cmd->name ) - 1] = 0;

	cmd->module = module;
	cmd->commandID = commandID;

	m_Commands.Add( cmd );
	Cmd_AddWrapperCommand( cmd->name, SystemWrapperCommandForwarder );

	return true;
}

IFileSystem *SystemWrapper::GetFileSystem()
{
	return g_pFileSystem;
}

unsigned char *SystemWrapper::LoadFile( const char *name, int *length )
{
	unsigned char *buffer = COM_LoadFile( (char *)name, 5, length );

	return buffer;
}

void SystemWrapper::FreeFile( unsigned char *fileHandle )
{
	COM_FreeFile( fileHandle );
}

unsigned int SystemWrapper::GetTick()
{
	return m_Tick;
}

void SystemWrapper::CMD_Modules( char *cmdLine )
{
	ISystemModule *module = (ISystemModule *)m_Modules.GetFirst();

	while ( module )
	{
		char *status = module->GetStatusLine();
		char *type = module->GetType();
		char *name = module->GetName();

		Printf( "%s(%s):%s", name, type, status );

		module = (ISystemModule *)m_Modules.GetNext();
	}

	Printf( "--- %i modules in total ---\n", m_Modules.CountElements() );
}

char *SystemWrapper::GetType()
{
	return BASESYSTEM_INTERFACE_VERSION;
}

void SystemWrapper::GetCommandMatches( char *string, ObjectList *pMatchList )
{
	pMatchList->Clear( true );
}

ISystemModule *SystemWrapper::FindModule( char *type, char *name )
{
	if ( !type || !type[0] )
		return NULL;

	ISystemModule *module = (ISystemModule *)m_Modules.GetFirst();

	while ( module )
	{
		if ( !_stricmp( type, module->GetType() ) )
		{
			if ( name )
			{
				if ( !_stricmp( name, module->GetName() ) )
					return module;
			}
			else
			{
				return module;
			}
		}

		module = (ISystemModule *)m_Modules.GetNext();
	}

	return NULL;
}

void SystemWrapper::LogConsole( char *filename )
{
	if ( filename )
		Cmd_ExecuteString( "log on", src_command );
	else
		Cmd_ExecuteString( "log off", src_command );
}

Panel *SystemWrapper::GetPanel()
{
	return (Panel *)VGuiWrap2_GetPanel();
}

bool SystemWrapper::InitVGUI( IVGuiModule *module )
{
	return false;
}

bool SystemWrapper::AddModule( ISystemModule *module, char *name )
{
	if ( !module )
		return false;

	if ( !module->Init( this, m_SerialCounter, name ) )
	{
		Printf( "ERROR! System::AddModule: couldn't initialize module %s.\n", name );
		return false;
	}

	m_Modules.AddHead( module );
	m_SerialCounter++;

	return true;
}

ISystemModule *SystemWrapper::GetModule( char *interfacename, char *library, char *instancename )
{
	ISystemModule *module = FindModule( interfacename, instancename );

	if ( module )
		return module;

	library_t *lib = GetLibrary( library );

	if ( !lib )
		return NULL;

	module = (ISystemModule *)lib->createInterfaceFn( interfacename, NULL );

	if ( !module )
	{
		Printf( "ERROR! System::GetModule: interface \"%s\" not found in library %s.\n", interfacename, lib->name );
		return NULL;
	}

	AddModule( module, instancename );

	return module;
}

bool SystemWrapper::RemoveModule( ISystemModule *module )
{
	if ( !module )
		return true;

	module->ShutDown();

	command_t *cmd = (command_t *)m_Commands.GetFirst();

	while ( cmd )
	{
		if ( cmd->module->GetSerial() == module->GetSerial() )
		{
			m_Commands.Remove( cmd );
			Mem_Free( cmd );
		}

		cmd = (command_t *)m_Commands.GetNext();
	}

	ISystemModule *mod = (ISystemModule *)m_Modules.GetFirst();

	while ( mod )
	{
		if ( mod->GetSerial() == module->GetSerial() )
		{
			m_Modules.Remove( mod );
			return true;
		}

		mod = (ISystemModule *)m_Modules.GetNext();
	}

	return false;
}

void SystemWrapper::ExecuteString( char *commands )
{
	char	singleCmd[256];
	bool	quotes = false;
	int		size = 0;

	if ( !commands )
		return;

	if ( !commands[0] )
		return;

	// remove format characters to block format string attacks
	COM_RemoveEvilChars( commands );

	char *p = commands;
	char *c = singleCmd;

	while ( 1 )
	{
		*c = *p;
		size++;

		if ( size >= sizeof( singleCmd ) )
		{
			Printf( "WARNING! System::ExecuteString: Command token too long.\n" );
			return;
		}

		if ( *p == '"' )
			quotes = !quotes;

		if ( ( *p == ';' && !quotes ) || !*p )
		{
			*c = 0;

			// skip leading white spaces
			c = singleCmd;
			while ( *c == ' ' )
				c++;

			DispatchCommand( c );

			c = singleCmd;
			size = 0;
		}
		else
		{
			c++;
		}

		if ( !*p )
			return;

		p++;
	}
}

bool SystemWrapper::DispatchCommand( char *command )
{
	if ( !command )
		return true;

	if ( !command[0] )
		return true;

	command_t *cmd = (command_t *)m_Commands.GetFirst();

	while ( cmd )
	{
		if ( !_strnicmp( cmd->name, command, strlen( cmd->name ) ) )
		{
			cmd->module->ExecuteCommand( cmd->commandID, command );
			return true;
		}

		cmd = (command_t *)m_Commands.GetNext();
	}

	Cmd_ExecuteString( command, src_command );

	return true;
}
