// IEngineWrapper.h -- the engine functions a system module (HLTV demo player) may call

#ifndef IENGINEWRAPPER_H
#define IENGINEWRAPPER_H

#include "ISystemModule.h"

struct client_data_s;
struct event_args_s;

class IEngineWrapper : virtual public ISystemModule
{
public:
	virtual			~IEngineWrapper() {}

	virtual bool	GetViewOrigin( float *origin ) = 0;
	virtual bool	GetViewAngles( float *angles ) = 0;
	virtual int		GetTraceEntity() = 0;
	virtual float	GetCvarFloat( char *szName ) = 0;
	virtual char	*GetCvarString( char *szName ) = 0;
	virtual void	SetCvar( char *szName, char *szValue ) = 0;
	virtual void	Cbuf_AddText( char *text ) = 0;
	virtual void	DemoUpdateClientData( struct client_data_s *cdat ) = 0;
	virtual void	CL_QueueEvent( int flags, int index, float delay, struct event_args_s *pargs ) = 0;
	virtual void	HudWeaponAnim( int iAnim, int body ) = 0;
	virtual void	CL_DemoPlaySound( int channel, char *sample, float attenuation, float volume, int flags, int pitch ) = 0;
	virtual void	ClientDLL_ReadDemoBuffer( int size, unsigned char *buffer ) = 0;
};

#define ENGINEWRAPPER_INTERFACE_VERSION	"enginewrapper001"

#endif // IENGINEWRAPPER_H
