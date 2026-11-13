// igame.h -- the engine's game window interface (game_win32, declarations
// from the Linux hw.so DWARF; 14 vtable slots in the Windows binary)

#ifndef IGAME_H
#define IGAME_H
#ifdef _WIN32
#pragma once
#endif

class IGame
{
public:
	virtual ~IGame( void ) {}

	virtual bool Init( void *pvInstance ) = 0;
	virtual bool Shutdown( void ) = 0;
	virtual bool CreateGameWindow( void ) = 0;
	virtual void SleepUntilInput( int time ) = 0;
	virtual HWND GetMainWindow( void ) = 0;
	virtual HWND *GetMainWindowAddress( void ) = 0;
	virtual void SetWindowXY( int x, int y ) = 0;
	virtual void SetWindowSize( int w, int h ) = 0;
	virtual void GetWindowRect( int *x, int *y, int *w, int *h ) = 0;
	virtual bool IsActiveApp( void ) = 0;
	virtual bool IsMultiplayer( void ) = 0;
	virtual void PlayStartupVideos( void ) = 0;
	virtual void PlayAVIAndWait( const char *aviFile ) = 0;
#ifndef _WIN32
	// SDL builds only: the Windows vtable ends with PlayAVIAndWait
	virtual void SetCursorVisible( bool bState ) = 0;
#endif
};

extern IGame *game;

#endif // IGAME_H
