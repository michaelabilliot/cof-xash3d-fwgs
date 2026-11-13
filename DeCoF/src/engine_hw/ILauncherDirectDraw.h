// ILauncherDirectDraw.h -- the DirectDraw helper that lists the display modes
// (class names from the RTTI of hw.dll, 5 vtable slots; method names unproven)

#ifndef ILAUNCHERDIRECTDRAW_H
#define ILAUNCHERDIRECTDRAW_H
#ifdef _WIN32
#pragma once
#endif

class ILauncherDirectDraw
{
public:
	virtual				~ILauncherDirectDraw( void ) {}

	virtual bool		Init_guess( void ) = 0;
	virtual bool		Shutdown_guess( void ) = 0;
	// adds every display mode of this depth to the video mode list
	virtual void		EnumDisplayModes_guess( int bpp ) = 0;
	virtual const char	*ErrorString_guess( long hr ) = 0;
};

// 0x01ED004C
extern ILauncherDirectDraw	*ddraw_guess;

#endif // ILAUNCHERDIRECTDRAW_H
