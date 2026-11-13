// sys_sdlwind.cpp -- the game window: window class, message handler, startup videos
// (the Linux build of the same file uses SDL; this is the Win32 one)

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <mmsystem.h>

#include "interface.h"
#include "vgui2/BaseUISurface.h"

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "net.h"
#include "client.h"
#include "keydefs.h"
#include "sound.h"
#include "vgui_int.h"

void	ClearIOStates( void );
void	ClientDLL_ActivateMouse( void );
void	ClientDLL_DeactivateMouse( void );
void	VGui_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
void	S_BlockSound( void );
void	S_UnblockSound( void );
void	FS_GetLocalCopy( const char *filename );
char	*FS_ReadLine( char *pOutput, int maxChars, FileHandle_t file );
}

#include "dll_state.h"
#include "igame.h"
#include "iengine.h"
#include "ivideomode.h"
#include "cd.h"

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL		0x020A
#endif
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN		0x020B
#define WM_XBUTTONUP		0x020C
#endif
#ifndef MK_XBUTTON1
#define MK_XBUTTON1			0x0020
#define MK_XBUTTON2			0x0040
#endif
#ifndef XBUTTON1
#define XBUTTON1			0x0001
#define XBUTTON2			0x0002
#endif

extern BaseUISurface g_BaseUISurface;

void GetWindowNameFromGameDir( char *output, int outputBufferSize );
LRESULT WINAPI HLEngineWindowProc_guess( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

class CGame : public IGame
{
public:
	CGame( void );
	virtual ~CGame( void );

	bool	Init( void *pvInstance );
	bool	Shutdown( void );
	bool	CreateGameWindow( void );
	void	SleepUntilInput( int time );
	HWND	GetMainWindow( void );
	HWND	*GetMainWindowAddress( void );
	void	SetWindowXY( int x, int y );
	void	SetWindowSize( int w, int h );
	void	GetWindowRect( int *x, int *y, int *w, int *h );
	bool	IsActiveApp( void );
	bool	IsMultiplayer( void );
	void	PlayStartupVideos( void );
	void	PlayAVIAndWait( const char *aviFile );

	void	AppActivate( bool fActive );
	int		MapKey_guess( LPARAM lParam );
	int		FieldsFromMouseWParam_guess( WPARAM wParam );
	int		FieldsFromMouseUpMsg_guess( UINT uMsg, WPARAM wParam );
	LRESULT	WindowProc_guess( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
	void	SetMainWindow_guess( HWND window );
	void	SetActiveApp( bool active );

private:
	// cleared by the constructor, never read
	bool		m_bUnused_guess;
	// set by the message loop to wake SleepUntilInput
	HANDLE		m_hEvent_guess;
	HWND		m_hWindow_guess;
	HINSTANCE	m_hInstance_guess;
	int			m_x;
	int			m_y;
	int			m_width;
	int			m_height;
	bool		m_bActiveApp;
	bool		m_bMultiplayer;
	// the mouse wheel message of old IntelliMouse drivers
	UINT		m_uiMouseWheel_guess;

	static const char CLASSNAME[];
};

const char CGame::CLASSNAME[] = "Valve001";

static CGame g_Game;
IGame *game = (IGame *)&g_Game;

static bool bPausedByCareer = false;

static byte scantokey[128] =
{
//  0           1       2       3       4       5       6       7
//  8           9       A       B       C       D       E       F
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9,		// 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    13 ,    K_CTRL, 'a',    's',		// 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'' ,  '`',    K_SHIFT,'\\',   'z',    'x',    'c',    'v',		// 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*',
	K_ALT,  ' ',    K_CAPSLOCK, K_F1, K_F2, K_F3,   K_F4,   K_F5,		// 3
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  K_PAUSE, 0,     K_HOME,
	K_UPARROW, K_PGUP, K_KP_MINUS, K_LEFTARROW, K_KP_5, K_RIGHTARROW, K_KP_PLUS, K_END,	// 4
	K_DOWNARROW, K_PGDN, K_INS, K_DEL, 0,   0,      0,      K_F11,
	K_F12,  0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,			// 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,			// 6
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0			// 7
};

//-----------------------------------------------------------------------------
// Purpose: The application lost or got the focus
//-----------------------------------------------------------------------------
void CGame::AppActivate( bool fActive )
{
	if ( fActive )
	{
		OutputDebugString( "AppActive: active\n" );

		if ( host_initialized )
		{
			videomode->RestoreVideo();
			ClearIOStates();
			cdaudio->Resume();

			if ( videomode->GetInitialized() )
			{
				ClientDLL_ActivateMouse();
				g_BaseUISurface.CalculateMouseVisible();
			}

			if ( VGuiWrap2_IsInCareerMatch() && bPausedByCareer )
			{
				Cmd_ExecuteString( "unpause", src_command );
				bPausedByCareer = false;
			}
		}
	}
	else
	{
		OutputDebugString( "AppActive: not active\n" );

		if ( host_initialized )
		{
			if ( videomode->GetInitialized() )
			{
				ClientDLL_DeactivateMouse();
			}

			cdaudio->Pause();
			videomode->ReleaseVideo();

			if ( VGuiWrap2_IsInCareerMatch() && !cl.paused )
			{
				Cmd_ExecuteString( "setpause", src_command );
				bPausedByCareer = true;
			}
		}
	}

	SetActiveApp( fActive );
}

//-----------------------------------------------------------------------------
// Purpose: Maps a windows key (the scan code in lParam) to an engine key
//-----------------------------------------------------------------------------
int CGame::MapKey_guess( LPARAM lParam )
{
	int result;
	int modified = ( lParam >> 16 ) & 255;
	bool is_extended = false;

	if ( modified > 127 )
		return 0;

	if ( lParam & ( 1 << 24 ) )
		is_extended = true;

	result = scantokey[modified];

	if ( !is_extended )
	{
		switch ( result )
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		}
	}
	else
	{
		switch ( result )
		{
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
// Purpose: The mouse buttons held down, from the wParam of a mouse message
//-----------------------------------------------------------------------------
int CGame::FieldsFromMouseWParam_guess( WPARAM wParam )
{
	int temp = 0;

	if ( wParam & MK_LBUTTON )
		temp |= 1;

	if ( wParam & MK_RBUTTON )
		temp |= 2;

	if ( wParam & MK_MBUTTON )
		temp |= 4;

	if ( wParam & MK_XBUTTON1 )
		temp |= 8;

	if ( wParam & MK_XBUTTON2 )
		temp |= 16;

	return temp;
}

//-----------------------------------------------------------------------------
// Purpose: The mouse button that a button up message releases
//-----------------------------------------------------------------------------
int CGame::FieldsFromMouseUpMsg_guess( UINT uMsg, WPARAM wParam )
{
	int temp = 0;

	if ( uMsg == WM_LBUTTONUP )
		temp |= 1;

	if ( uMsg == WM_RBUTTONUP )
		temp |= 2;

	if ( uMsg == WM_MBUTTONUP )
		temp |= 4;

	if ( uMsg == WM_XBUTTONUP )
	{
		if ( HIWORD( wParam ) == XBUTTON1 )
			temp |= 8;
		else if ( HIWORD( wParam ) == XBUTTON2 )
			temp |= 16;
	}

	return temp;
}

//-----------------------------------------------------------------------------
// Purpose: The message handler of the game window
//-----------------------------------------------------------------------------
LRESULT CGame::WindowProc_guess( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	LRESULT			lRet = 0;
	HDC				hdc;
	PAINTSTRUCT		ps;
	int				temp = 0;
	bool			bAltDown_guess;
	bool			fKeyUp_guess;
	bool			bSwitchAway_guess;

	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		return DefWindowProc( hWnd, uMsg, wParam, lParam );

	if ( uMsg == m_uiMouseWheel_guess )
	{
		eng->TrapKey_Event( ( (int)wParam > 0 ) ? K_MWHEELUP : K_MWHEELDOWN, true );
		eng->TrapKey_Event( ( (int)wParam > 0 ) ? K_MWHEELUP : K_MWHEELDOWN, false );
	}

	switch ( uMsg )
	{
	case WM_CREATE:
		::SetForegroundWindow( hWnd );
		break;

	case WM_MOUSEWHEEL:
		eng->TrapKey_Event( ( (short)HIWORD( wParam ) > 0 ) ? K_MWHEELUP : K_MWHEELDOWN, true );
		eng->TrapKey_Event( ( (short)HIWORD( wParam ) > 0 ) ? K_MWHEELUP : K_MWHEELDOWN, false );
		break;

	case WM_SYSCOMMAND:
		if ( wParam == SC_SCREENSAVE )
			return lRet;

		if ( wParam == SC_CLOSE )
		{
			if ( eng->GetState() == DLL_ACTIVE )
				eng->SetQuitting( IEngine::QUIT_TODESKTOP );

			lRet = 0;
			return lRet;
		}

		S_BlockSound();
		S_ClearBuffer();

		lRet = DefWindowProc( hWnd, uMsg, wParam, lParam );

		S_UnblockSound();
		break;

	case WM_SIZE:
		if ( wParam == SIZE_MINIMIZED )
		{
			MoveWindow( hWnd, 0, -20, 0, 20, FALSE );
		}
		break;

	case WM_MOVE:
		game->SetWindowXY( LOWORD( lParam ), HIWORD( lParam ) );
		videomode->UpdateWindowPosition();
		break;

	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	case WM_ACTIVATEAPP:
		AppActivate( wParam ? true : false );
		break;

	case WM_PAINT:
		hdc = BeginPaint( hWnd, &ps );
		EndPaint( hWnd, &ps );
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		eng->TrapKey_Event( MapKey_guess( lParam ), true );
		break;

	case WM_SYSKEYUP:
		bAltDown_guess = ( HIWORD( lParam ) & KF_ALTDOWN ) ? true : false;
		fKeyUp_guess = ( HIWORD( lParam ) & KF_UP ) ? true : false;
		bSwitchAway_guess = false;

		if ( wParam == VK_TAB && bAltDown_guess && fKeyUp_guess )
			bSwitchAway_guess = true;

		if ( wParam == VK_ESCAPE && bAltDown_guess && fKeyUp_guess )
			bSwitchAway_guess = true;

		if ( bSwitchAway_guess )
		{
			if ( videomode->IsWindowedMode() )
			{
			}
		}
		// fall through

	case WM_KEYUP:
		eng->TrapKey_Event( MapKey_guess( lParam ), false );
		break;

	case WM_MOUSEMOVE:
		temp |= FieldsFromMouseWParam_guess( wParam );
		eng->TrapMouse_Event( temp, true );
		break;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		temp |= FieldsFromMouseWParam_guess( wParam );
		eng->TrapMouse_Event( temp, true );
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		temp = FieldsFromMouseUpMsg_guess( uMsg, wParam ) | FieldsFromMouseWParam_guess( wParam );
		eng->TrapMouse_Event( temp, false );
		break;

	case WM_CLOSE:
		if ( eng->GetState() == DLL_ACTIVE )
			eng->SetQuitting( IEngine::QUIT_TODESKTOP );

		lRet = 0;
		break;

	default:
		if ( uMsg != m_uiMouseWheel_guess )
			lRet = DefWindowProc( hWnd, uMsg, wParam, lParam );
		break;
	}

	if ( !eng->IsTrapping() )
	{
		VGui_CallEngineSurfaceAppHandler( hWnd, uMsg, wParam, lParam );
	}

	return lRet;
}

//-----------------------------------------------------------------------------
// Purpose: Waits for input or until the time (ms) runs out
//-----------------------------------------------------------------------------
void CGame::SleepUntilInput( int time )
{
	MsgWaitForMultipleObjects( 1, &m_hEvent_guess, FALSE, time, QS_KEY );
}

//-----------------------------------------------------------------------------
// Purpose: Registers the window class and creates the game window
//-----------------------------------------------------------------------------
bool CGame::CreateGameWindow( void )
{
	WNDCLASS		wc;
	char			localPath[ MAX_PATH ];
	DWORD			style;
	int				w, h;
	char			gameWindowName[ 512 ];
	HWND			hWnd;

	m_uiMouseWheel_guess = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );

	memset( &wc, 0, sizeof( wc ) );

	wc.style			= CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc		= HLEngineWindowProc_guess;
	wc.hInstance		= m_hInstance_guess;
	wc.lpszClassName	= CLASSNAME;

	// find the icon file in the filesystem
	if ( FS_GetLocalPath( "game.ico", localPath, sizeof( localPath ) ) )
	{
		FS_GetLocalCopy( localPath );
		wc.hIcon = (HICON)::LoadImage( NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
	}
	else
	{
		wc.hIcon = (HICON)::LoadImage( NULL, "valve.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE );
	}

	// Get the class name for this game
	::UnregisterClass( CLASSNAME, m_hInstance_guess );
	::RegisterClass( &wc );

	style = WS_POPUP | WS_CLIPSIBLINGS;

	if ( videomode->IsWindowedMode() )
	{
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
	}

	style &= ~WS_MAXIMIZEBOX;

	w = GetSystemMetrics( SM_CXSCREEN );
	h = GetSystemMetrics( SM_CYSCREEN );

	strcpy( gameWindowName, "Cry of Fear" );
	GetWindowNameFromGameDir( gameWindowName, sizeof( gameWindowName ) );

	hWnd = CreateWindowEx( 0, CLASSNAME, gameWindowName, style, 0, 0, w, h, NULL, NULL, m_hInstance_guess, NULL );
	g_Game.SetMainWindow_guess( hWnd );

	return hWnd ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: The window procedure of the window class
//-----------------------------------------------------------------------------
LRESULT WINAPI HLEngineWindowProc_guess( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_Game.WindowProc_guess( hWnd, uMsg, wParam, lParam );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CGame::CGame( void )
{
	m_bUnused_guess = false;
	m_hWindow_guess = 0;
	m_x = m_y = 0;
	m_width = m_height = 0;
	m_bActiveApp = true;
	m_bMultiplayer = false;
	m_uiMouseWheel_guess = 0;
	m_hInstance_guess = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CGame::~CGame( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGame::Init( void *pvInstance )
{
	OSVERSIONINFO	vinfo;

	m_hInstance_guess = (HINSTANCE)pvInstance;

	m_hEvent_guess = CreateEvent( NULL, FALSE, FALSE, NULL );
	if ( !m_hEvent_guess )
		return false;

	vinfo.dwOSVersionInfoSize = sizeof( vinfo );

	if ( !GetVersionEx( &vinfo ) )
		return false;

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGame::Shutdown( void )
{
	CloseHandle( m_hEvent_guess );

	if ( m_hWindow_guess )
	{
		DestroyWindow( m_hWindow_guess );
		m_hWindow_guess = 0;
	}

	UnregisterClass( CLASSNAME, m_hInstance_guess );
	m_hInstance_guess = 0;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
HWND CGame::GetMainWindow( void )
{
	return m_hWindow_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
HWND *CGame::GetMainWindowAddress( void )
{
	return &m_hWindow_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGame::SetMainWindow_guess( HWND window )
{
	m_hWindow_guess = window;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGame::GetWindowRect( int *x, int *y, int *w, int *h )
{
	if ( x )
	{
		*x = m_x;
	}
	if ( y )
	{
		*y = m_y;
	}
	if ( w )
	{
		*w = m_width;
	}
	if ( h )
	{
		*h = m_height;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGame::IsActiveApp( void )
{
	return m_bActiveApp;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CGame::IsMultiplayer( void )
{
	return m_bMultiplayer;
}

//-----------------------------------------------------------------------------
// Purpose: Plays the videos listed in media/StartupVids.txt
//-----------------------------------------------------------------------------
void CGame::PlayStartupVideos( void )
{
	if ( COM_CheckParm( "-novid" ) || COM_CheckParm( "-dev" ) )
		return;

	int fileSize_guess = 0;
	char *pFile_guess = (char *)COM_LoadFile( "media/StartupVids.txt", 5, &fileSize_guess );

	if ( !pFile_guess || !fileSize_guess )
		return;

	ShowCursor( FALSE );

	char *pData_guess = pFile_guess;
	while ( 1 )
	{
		pData_guess = COM_Parse( pData_guess );
		if ( Q_strlen( com_token ) <= 0 )
			break;

		PlayAVIAndWait( com_token );
	}

	Mem_Free( pFile_guess );
	ShowCursor( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: Plays an AVI file in the game window until it ends or ESC is pressed
//-----------------------------------------------------------------------------
void CGame::PlayAVIAndWait( const char *aviFile )
{
	char		szCommand_guess[ 512 ];
	MCIDEVICEID	wDeviceID_guess;
	UINT		uBufferSize_guess;
	char		szAlias_guess[ 32 ];
	char		szReply_guess[ 1024 ];
	MCIERROR	result_guess;

	if ( !aviFile || !aviFile[0] )
		return;

	sprintf( szAlias_guess, "video" );
	uBufferSize_guess = sizeof( szReply_guess );

	sprintf( szCommand_guess, "open %s type AVIVideo alias %s parent %d style child wait", aviFile, szAlias_guess, GetMainWindow() );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
	if ( result_guess )
		return;

	wDeviceID_guess = mciGetDeviceID( szAlias_guess );
	if ( !wDeviceID_guess )
		return;

	sprintf( szCommand_guess, "window %s handle %d state restore wait", szAlias_guess, GetMainWindow() );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
	if ( result_guess )
		return;

	sprintf( szCommand_guess, "put %s destination at %i %i %i %i wait", szAlias_guess, 0, 0, m_width, m_height );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
	if ( result_guess )
		return;

	sprintf( szCommand_guess, "seek %s to start wait", szAlias_guess );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
	if ( result_guess )
		return;

	// ESC stops the video
	sprintf( szCommand_guess, "break %s on 27 wait", szAlias_guess );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );

	sprintf( szCommand_guess, "play %s wait", szAlias_guess );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
	if ( result_guess )
		return;

	sprintf( szCommand_guess, "stop %s wait", szAlias_guess );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );

	sprintf( szCommand_guess, "close %s wait", szAlias_guess );
	result_guess = mciSendString( szCommand_guess, szReply_guess, uBufferSize_guess, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: The game name from the "game" line of liblist.gam
//-----------------------------------------------------------------------------
void GetWindowNameFromGameDir( char *output, int outputBufferSize )
{
	FileHandle_t f = FS_Open( "liblist.gam", "rt" );
	if ( !f )
		return;

	char line[512];
	while ( !FS_EndOfFile( f ) )
	{
		line[0] = 0;
		FS_ReadLine( line, sizeof( line ) - 1, f );
		line[ sizeof( line ) - 1 ] = 0;

		if ( !_strnicmp( line, "game", 4 ) )
		{
			char *start = strchr( line, '"' );
			if ( !start )
				break;

			char *end = strchr( start + 1, '"' );
			if ( !end )
				break;

			int bytesToCopy = end - start - 1;
			if ( bytesToCopy >= outputBufferSize )
				break;

			strncpy( output, start + 1, bytesToCopy );
			output[ bytesToCopy ] = 0;
			break;
		}
	}

	FS_Close( f );
}
