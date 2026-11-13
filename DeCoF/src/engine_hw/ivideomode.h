// ivideomode.h -- the engine's video mode interface (sys_getmodes.cpp;
// declarations from the Linux hw.so DWARF, 14 vtable slots in the Windows binary)

#ifndef IVIDEOMODE_H
#define IVIDEOMODE_H
#ifdef _WIN32
#pragma once
#endif

struct vmode_s;

class IVideoMode
{
public:
	virtual const char		*GetName( void ) = 0;

	virtual bool			Init( void *pvInstance ) = 0;
	virtual void			Shutdown( void ) = 0;

	virtual bool			AddMode( int width, int height, int bpp ) = 0;
	virtual struct vmode_s	*GetCurrentMode( void ) = 0;
	virtual struct vmode_s	*GetMode( int num ) = 0;
	virtual int				GetModeCount( void ) = 0;

	virtual bool			IsWindowedMode( void ) = 0;

	virtual bool			GetInitialized( void ) = 0;
	virtual void			SetInitialized( bool init ) = 0;

	virtual void			UpdateWindowPosition( void ) = 0;

	virtual void			FlipScreen( void ) = 0;

	virtual void			RestoreVideo( void ) = 0;
	virtual void			ReleaseVideo( void ) = 0;
};

extern IVideoMode *videomode;

#endif // IVIDEOMODE_H
