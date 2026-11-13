// IBaseSystem.h -- interface of the system that owns and runs the modules

#ifndef IBASESYSTEM_H
#define IBASESYSTEM_H

#include "ISystemModule.h"

struct IVGuiModule;
struct Panel;
class ObjectList;
class IFileSystem;

class IBaseSystem : virtual public ISystemModule
{
public:
	virtual					~IBaseSystem() {}

	virtual double			GetTime() = 0;
	virtual unsigned int	GetTick() = 0;
	virtual void			SetFPS( float fps ) = 0;

	virtual void			Printf( char *fmt, ... ) = 0;
	virtual void			DPrintf( char *fmt, ... ) = 0;

	virtual void			RedirectOutput( char *buffer = NULL, int maxSize = 0 ) = 0;

	virtual IFileSystem		*GetFileSystem() = 0;

	virtual unsigned char	*LoadFile( const char *name, int *length = NULL ) = 0;
	virtual void			FreeFile( unsigned char *fileHandle ) = 0;

	virtual void			SetTitle( char *text ) = 0;
	virtual void			SetStatusLine( char *text ) = 0;

	virtual void			ShowConsole( bool visible ) = 0;
	virtual void			LogConsole( char *filename ) = 0;

	virtual bool			InitVGUI( IVGuiModule *module ) = 0;
	virtual Panel			*GetPanel() = 0;

	virtual bool			RegisterCommand( char *name, ISystemModule *module, int commandID ) = 0;
	virtual void			GetCommandMatches( char *string, ObjectList *pMatchList ) = 0;
	virtual void			ExecuteString( char *commands ) = 0;
	virtual void			ExecuteFile( char *filename ) = 0;
	virtual void			Errorf( char *fmt, ... ) = 0;

	virtual char			*CheckParam( char *param ) = 0;

	virtual bool			AddModule( ISystemModule *module, char *name ) = 0;
	virtual ISystemModule	*GetModule( char *interfacename, char *library, char *instancename = NULL ) = 0;
	virtual bool			RemoveModule( ISystemModule *module ) = 0;

	virtual void			Stop() = 0;
};

#define BASESYSTEM_INTERFACE_VERSION	"basesystem002"

#endif // IBASESYSTEM_H
