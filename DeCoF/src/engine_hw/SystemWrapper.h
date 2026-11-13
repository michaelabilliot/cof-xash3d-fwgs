// SystemWrapper.h -- the engine as an IBaseSystem for system modules (HLTV demo player)

#ifndef SYSTEMWRAPPER_H
#define SYSTEMWRAPPER_H

#include "ObjectList.h"
#include "TokenLine.h"
#include "BaseSystemModule.h"
#include "IEngineWrapper.h"

class EngineWrapper : public IEngineWrapper, public BaseSystemModule
{
public:
	EngineWrapper();
	virtual ~EngineWrapper();

	char	*GetStatusLine();
	char	*GetType();

	bool	GetViewOrigin( float *origin );
	bool	GetViewAngles( float *angles );
	int		GetTraceEntity();
	float	GetCvarFloat( char *szName );
	char	*GetCvarString( char *szName );
	void	SetCvar( char *szName, char *szValue );
	void	Cbuf_AddText( char *text );
	void	DemoUpdateClientData( struct client_data_s *cdat );
	void	CL_QueueEvent( int flags, int index, float delay, struct event_args_s *pargs );
	void	HudWeaponAnim( int iAnim, int body );
	void	CL_DemoPlaySound( int channel, char *sample, float attenuation, float volume, int flags, int pitch );
	void	ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer );
};

class SystemWrapper : public IBaseSystem, public BaseSystemModule
{
public:
	SystemWrapper();
	virtual ~SystemWrapper();

	// ISystemModule
	bool			Init( IBaseSystem *system, int serial, char *name );
	void			RunFrame( double time );
	void			ExecuteCommand( int commandID, char *commandLine );
	char			*GetStatusLine();
	char			*GetType();
	void			ShutDown();

	// IBaseSystem
	double			GetTime();
	unsigned int	GetTick();
	void			SetFPS( float fps ) {}

	void			Printf( char *fmt, ... );
	void			DPrintf( char *fmt, ... );

	void			RedirectOutput( char *buffer, int maxSize );

	IFileSystem		*GetFileSystem();

	unsigned char	*LoadFile( const char *name, int *length );
	void			FreeFile( unsigned char *fileHandle );

	void			SetTitle( char *text );
	void			SetStatusLine( char *text );

	void			ShowConsole( bool visible ) {}
	void			LogConsole( char *filename );

	bool			InitVGUI( IVGuiModule *module );
	Panel			*GetPanel();

	bool			RegisterCommand( char *name, ISystemModule *module, int commandID );
	void			GetCommandMatches( char *string, ObjectList *pMatchList );
	void			ExecuteString( char *commands );
	void			ExecuteFile( char *filename );
	void			Errorf( char *fmt, ... );

	char			*CheckParam( char *param );

	bool			AddModule( ISystemModule *module, char *name );
	ISystemModule	*GetModule( char *interfacename, char *library, char *instancename = NULL );
	bool			RemoveModule( ISystemModule *module );

	void			Stop();

protected:
	struct command_t
	{
		char			name[32];
		int				commandID;
		ISystemModule	*module;
	};

	struct library_t
	{
		char				name[MAX_PATH];
		CSysModule			*handle;
		CreateInterfaceFn	createInterfaceFn;
	};

	library_t		*GetLibrary( char *name );
	ISystemModule	*FindModule( char *type, char *name = NULL );
	bool			DispatchCommand( char *command );

	enum LocalCommandIDs
	{
		CMD_ID_MODULES = 1,
		CMD_ID_LOADMODULE,
		CMD_ID_UNLOADMODULE
	};

	void			CMD_Modules( char *cmdLine );
	void			CMD_UnloadModule( char *cmdLine );
	void			CMD_LoadModule( char *cmdLine );

private:
	ObjectList		m_Modules;
	ObjectList		m_Libraries;
	ObjectList		m_Commands;
	unsigned int	m_SerialCounter;
	unsigned int	m_Tick;
	double			m_LastTime;
	IEngineWrapper	*m_EngineWrapper;
};

extern SystemWrapper gSystemWrapper;

extern "C"
{
void	SystemWrapper_Init( void );
void	SystemWrapper_RunFrame( double time );
void	SystemWrapper_ShutDown( void );
int		SystemWrapper_LoadModule( char *interfacename, char *library, char *instancename );
void	SystemWrapper_ExecuteString( char *command );
void	SystemWrapperCommandForwarder( void );
}

int		COM_BuildNumber( void );
void	COM_RemoveEvilChars( char *string );

#endif // SYSTEMWRAPPER_H
