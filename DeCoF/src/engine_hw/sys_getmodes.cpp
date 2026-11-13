// sys_getmodes.cpp -- video mode selection, the game window and the startup graphic

#include <windows.h>
#include <malloc.h>
#include <math.h>
#include <ddraw.h>

extern "C"
{
#include "quakedef.h"
#include "vid.h"
#include "qgl.h"

extern viddef_t	vid;

int		LoadTGA( char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height );
void	VID_UpdateWindowVars( RECT *prc, int x, int y );
char	*FS_ParseFile( char *pFileBytes, char *pToken, int *pWasQuoted );

// SND_DMA.C, snd_win.c
void	VOX_Init( void );
void	Snd_AcquireBuffer( void );
void	Snd_ReleaseBuffer( void );

// sys_getmodes.cpp
void	VideoMode_Create( void );
void	VideoMode_SwitchMode( int hardware, int windowed );
void	VideoMode_SetVideoMode( int width, int height, int bpp );
void	VideoMode_GetVideoModes( struct vmode_s **liststart, int *count );
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
void	VideoMode_GetCurrentRenderer( char *name, int namelen, int *windowed );
int		VideoMode_IsWindowed( void );
}

#include "../launcher/iregistry.h"
#include "../public/UtlVector.h"
#include "igame.h"
#include "ivideomode.h"
#include "ILauncherDirectDraw.h"

typedef struct vmode_s
{
	int			width;
	int			height;
	int			bpp;
} vmode_t;

#define MAX_MODES	32

// what QGL_D3DShared hands to the OpenGL to Direct3D wrapper (opengl2d3d)
typedef struct tagD3DGlobals
{
	HWND			hWnd_guess;
	int				bFullScreen_guess;
	int				bOwnDisplayMode_guess;
	LPDIRECTDRAW2	lpDD_guess;
	int				unknown10_guess;
	int				unknown14_guess;
	int				unknown18_guess;
	int				bStencil_guess;
} D3DGlobals;

extern "C" void	QGL_D3DShared( D3DGlobals *d3dGShared );

//-----------------------------------------------------------------------------
// Purpose: the video mode code shared by the OpenGL and Direct3D modes
//-----------------------------------------------------------------------------
class CVideoMode_Common : public IVideoMode
{
public:
	CVideoMode_Common( void );
	virtual ~CVideoMode_Common( void );

	virtual bool		Init( void *pvInstance );
	virtual void		Shutdown( void );

	virtual bool		AddMode( int width, int height, int bpp );
	virtual vmode_t		*GetCurrentMode( void );
	virtual vmode_t		*GetMode( int num );
	virtual int			GetModeCount( void );

	virtual bool		IsWindowedMode( void );

	virtual bool		GetInitialized( void );
	virtual void		SetInitialized( bool init );

	virtual void		UpdateWindowPosition( void );

	virtual void		FlipScreen( void );

	virtual void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );

	virtual int			MaxBitsPerPixel( void );
	virtual void		ReleaseFullScreen( void );
	virtual void		ChangeDisplaySettingsToFullscreen( void );

protected:
	void				AdjustWindowForMode( void );
	void				CenterEngineWindow( HWND hWndCenter, int width, int height );
	void				LoadStartupGraphic( void );
	void				DrawStartupGraphic( HWND window );
	void				DrawImage_guess( HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1 );

	struct bimage_t
	{
		byte		*buffer;
		int			x, y, width, height;
		bool		scaled;
	};

	vmode_t			m_rgModeList[ MAX_MODES ];
	vmode_t			m_safeMode;
	int				m_nNumModes;
	int				m_nGameMode;
	bool			m_bInitialized;
	CUtlVector< bimage_t > m_ImageID;
	int				m_iBaseResX;
	int				m_iBaseResY;
	bool			m_bWindowed;
};

CVideoMode_Common::CVideoMode_Common( void )
{
	m_nNumModes = 0;
	m_nGameMode = 0;
	m_bWindowed = false;
	m_bInitialized = false;
	m_safeMode.width = 640;
	m_safeMode.height = 480;
	m_safeMode.bpp = 16;
}

CVideoMode_Common::~CVideoMode_Common( void )
{
}

bool CVideoMode_Common::GetInitialized( void )
{
	return m_bInitialized;
}

void CVideoMode_Common::SetInitialized( bool init )
{
	m_bInitialized = init;
}

bool CVideoMode_Common::IsWindowedMode( void )
{
	return m_bWindowed;
}

bool CVideoMode_Common::AddMode( int width, int height, int bpp )
{
	vmode_t *mode;

	if ( m_nNumModes >= MAX_MODES )
		return false;

	mode = &m_rgModeList[ m_nNumModes++ ];
	mode->width = width;
	mode->height = height;
	mode->bpp = bpp;
	return true;
}

vmode_t *CVideoMode_Common::GetMode( int num )
{
	if ( num >= m_nNumModes || num < 0 )
		return &m_safeMode;

	return &m_rgModeList[ num ];
}

int CVideoMode_Common::GetModeCount( void )
{
	return m_nNumModes;
}

int CVideoMode_Common::MaxBitsPerPixel( void )
{
	return 32;
}

vmode_t *CVideoMode_Common::GetCurrentMode( void )
{
	return GetMode( m_nGameMode );
}

static int VideoModeCompare( const void *arg1, const void *arg2 );

bool CVideoMode_Common::Init( void *pvInstance )
{
	int x = 640;
	int y = 480;

	x = registry->ReadInt( "ScreenWidth", 640 );
	y = registry->ReadInt( "ScreenHeight", 480 );

	int bitsperpixel = 16;

	bitsperpixel = registry->ReadInt( "ScreenBPP", 16 );

	if ( COM_CheckParm( "-16bpp" ) )
		bitsperpixel = 16;
	if ( COM_CheckParm( "-24bpp" ) )
		bitsperpixel = 24;
	if ( COM_CheckParm( "-32bpp" ) )
		bitsperpixel = 32;

	int maxbits = MaxBitsPerPixel();
	if ( bitsperpixel > maxbits )
	{
		bitsperpixel = min( bitsperpixel, maxbits );
	}

	m_nNumModes = 0;

	if ( ddraw_guess->Init_guess() )
	{
		ddraw_guess->EnumDisplayModes_guess( bitsperpixel );
		ddraw_guess->Shutdown_guess();
	}

	if ( m_nNumModes >= 2 )
	{
		qsort( m_rgModeList, m_nNumModes, sizeof( vmode_t ), VideoModeCompare );
	}

	int iOK = -1;
	int i;

	if ( i = COM_CheckParm( "-width" ) )
		x = atoi( com_argv[ i + 1 ] );
	if ( i = COM_CheckParm( "-w" ) )
		x = atoi( com_argv[ i + 1 ] );
	if ( i = COM_CheckParm( "-height" ) )
		y = atoi( com_argv[ i + 1 ] );
	if ( i = COM_CheckParm( "-h" ) )
		y = atoi( com_argv[ i + 1 ] );

	if ( IsWindowedMode() )
	{
		HDC hdc = GetDC( game->GetMainWindow() );
		int wide = GetDeviceCaps( hdc, HORZRES );
		int tall = GetDeviceCaps( hdc, VERTRES );

		if ( x > wide )
			x = wide;
		if ( y > tall )
			y = tall;
	}

	if ( x > 0 && y < 0 )
	{
		y = (int)( x * 3.0 / 4.0 );
	}

	for ( i = 0; i < m_nNumModes; i++ )
	{
		if ( m_rgModeList[ i ].width != x )
			continue;

		iOK = i;

		if ( m_rgModeList[ i ].height != y )
			continue;

		break;
	}

	if ( i >= m_nNumModes )
	{
		if ( iOK != -1 )
			i = iOK;
		else
			i = 0;
	}

	m_nGameMode = i;

	if ( IsWindowedMode() && ( COM_CheckParm( "-forceres" ) || !m_nNumModes ) )
	{
		m_rgModeList[ i ].width = x;
		m_rgModeList[ i ].height = y;

		if ( !m_nNumModes )
			m_nNumModes = 1;
	}

	registry->WriteInt( "ScreenWidth", m_rgModeList[ i ].width );
	registry->WriteInt( "ScreenHeight", m_rgModeList[ i ].height );
	registry->WriteInt( "ScreenBPP", m_rgModeList[ i ].bpp );

	LoadStartupGraphic();

	if ( game->CreateGameWindow() )
	{
		AdjustWindowForMode();
		game->PlayStartupVideos();
		DrawStartupGraphic( game->GetMainWindow() );
		return true;
	}

	return false;
}

static int VideoModeCompare( const void *arg1, const void *arg2 )
{
	vmode_t *m1, *m2;

	m1 = (vmode_t *)arg1;
	m2 = (vmode_t *)arg2;

	if ( m1->width < m2->width )
		return -1;

	if ( m1->width == m2->width )
	{
		if ( m1->height < m2->height )
			return -1;
		else if ( m1->height > m2->height )
			return 1;
		else
			return 0;
	}

	return 1;
}

void CVideoMode_Common::UpdateWindowPosition( void )
{
	RECT rect;
	int x, y, w, h;
	int y_center_guess, x_center_guess;

	game->GetWindowRect( &x, &y, &w, &h );

	rect.left = x;
	rect.right = x + w;
	rect.top = y;
	rect.bottom = y + h;

	x_center_guess = x + w / 2;
	y_center_guess = y + h / 2;

	VID_UpdateWindowVars( &rect, x_center_guess, y_center_guess );
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen( void )
{
}

void CVideoMode_Common::ReleaseFullScreen( void )
{
}

void CVideoMode_Common::AdjustWindowForMode( void )
{
	vmode_t *mode = GetCurrentMode();

	Snd_ReleaseBuffer();

	ChangeDisplaySettingsToFullscreen();

	RECT WindowRect;
	WindowRect.top = 0;
	WindowRect.left = 0;
	WindowRect.right = mode->width;
	WindowRect.bottom = mode->height;

	vid.width = mode->width;
	vid.height = mode->height;

	DWORD windowStyle_guess = GetWindowLong( game->GetMainWindow(), GWL_STYLE );
	AdjustWindowRectEx( &WindowRect, windowStyle_guess, FALSE, 0 );

	SetWindowPos( game->GetMainWindow(), NULL, 0, 0, WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top, SWP_NOZORDER | SWP_NOREDRAW );

	CenterEngineWindow( game->GetMainWindow(), WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top );

	game->SetWindowSize( mode->width, mode->height );

	Snd_AcquireBuffer();
	VOX_Init();

	UpdateWindowPosition();
}

void CVideoMode_Common::Shutdown( void )
{
	ReleaseFullScreen();

	if ( !GetInitialized() )
		return;

	SetInitialized( false );

	m_nGameMode = 0;
	videomode = NULL;

	delete this;
}

void CVideoMode_Common::FlipScreen( void )
{
}

void CVideoMode_Common::CenterEngineWindow( HWND hWndCenter, int width, int height )
{
	int CenterX, CenterY;

	CenterX = ( GetSystemMetrics( SM_CXSCREEN ) - width ) / 2;
	CenterY = ( GetSystemMetrics( SM_CYSCREEN ) - height ) / 2;
	CenterX = ( CenterX < 0 ) ? 0 : CenterX;
	CenterY = ( CenterY < 0 ) ? 0 : CenterY;

	game->SetWindowXY( CenterX, CenterY );

	SetWindowPos( hWndCenter, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME );
}

void CVideoMode_Common::LoadStartupGraphic( void )
{
	FileHandle_t file = FS_Open( "resource/BackgroundLoadingLayout.txt", "rt" );
	if ( !file )
		return;

	int fileSize = FS_Size( file );
	char *buffer = (char *)_alloca( fileSize + 1 );
	memset( buffer, 0, fileSize );
	FS_Read( buffer, fileSize, 1, file );
	FS_Close( file );

	char token[512];

	while ( buffer && *buffer && ( buffer = FS_ParseFile( buffer, token, NULL ) ) && token[0] )
	{
		if ( !_stricmp( token, "resolution" ) )
		{
			buffer = FS_ParseFile( buffer, token, NULL );
			m_iBaseResX = atoi( token );
			buffer = FS_ParseFile( buffer, token, NULL );
			m_iBaseResY = atoi( token );
		}
		else
		{
			bimage_t &bimage = m_ImageID[ m_ImageID.AddToTail() ];
			bimage.buffer = (byte *)malloc( 256 * 256 * 4 );

			if ( !LoadTGA( token, bimage.buffer, 256 * 256 * 4, &bimage.width, &bimage.height ) )
			{
				m_ImageID.Remove( m_ImageID.Count() - 1 );
				break;
			}

			buffer = FS_ParseFile( buffer, token, NULL );
			bimage.scaled = !_stricmp( token, "scaled" );
			buffer = FS_ParseFile( buffer, token, NULL );
			bimage.x = atoi( token );
			buffer = FS_ParseFile( buffer, token, NULL );
			bimage.y = atoi( token );
		}
	}
}

void CVideoMode_Common::DrawStartupGraphic( HWND window )
{
	if ( !m_ImageID.Count() )
		return;

	vmode_t *mode = GetCurrentMode();

	HDC hdc = GetDC( window );
	HDC memDC_guess = CreateCompatibleDC( hdc );
	HBITMAP bitmap_guess = CreateCompatibleBitmap( hdc, mode->width, mode->height );
	SelectObject( memDC_guess, bitmap_guess );

	float xScale = (float)mode->width / m_iBaseResX;
	float yScale = (float)mode->height / m_iBaseResY;

	for ( int x = 0; x < m_ImageID.Count(); x++ )
	{
		bimage_t &bimage = m_ImageID[ x ];

		int dx = bimage.x ? (int)ceil( bimage.x * xScale ) : 0;
		int dy = bimage.y ? (int)ceil( bimage.y * yScale ) : 0;
		int dw, dt;

		if ( bimage.scaled )
		{
			dw = (int)ceil( ( bimage.x + bimage.width ) * xScale );
			dt = (int)ceil( ( bimage.y + bimage.height ) * yScale );
		}
		else
		{
			dw = dx + bimage.width;
			dt = dy + bimage.height;
		}

		DrawImage_guess( memDC_guess, bimage.buffer, bimage.width, bimage.height, dx, dy, dw, dt );
	}

	SetViewportOrgEx( hdc, 0, 0, NULL );
	BitBlt( hdc, 0, 0, mode->width, mode->height, memDC_guess, 0, 0, SRCCOPY );
	DeleteObject( bitmap_guess );
	DeleteDC( memDC_guess );
	ReleaseDC( window, hdc );

	for ( int i = 0; i < m_ImageID.Count(); i++ )
	{
		if ( m_ImageID[ i ].buffer )
			free( m_ImageID[ i ].buffer );
	}

	m_ImageID.Purge();
}

void CVideoMode_Common::DrawImage_guess( HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1 )
{
	int x0_guess = x0;
	int ypos_guess = y0;
	int width_guess = x1 - x0;
	int height_guess = y1 - y0;
	float flScaleX_guess = (float)imageWidth / width_guess;
	float flScaleY_guess = (float)imageHeight / height_guess;
	int xInc_guess = (int)( flScaleX_guess * 256.0 );
	int pitch_guess = imageWidth * 4;

	for ( int j_guess = 0; j_guess < height_guess; j_guess++ )
	{
		byte *src = rgba + (int)( j_guess * flScaleY_guess ) * pitch_guess;
		int xacc = 0;

		for ( int x = 0; x < width_guess; x++ )
		{
			byte *p = &src[ ( xacc >> 8 ) * 4 ];
			SetPixel( hdc, x0_guess + x, ypos_guess + j_guess, RGB( p[0], p[1], p[2] ) );
			xacc += xInc_guess;
		}
	}
}

void CVideoMode_Common::RestoreVideo( void )
{
}

void CVideoMode_Common::ReleaseVideo( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: the OpenGL video mode
//-----------------------------------------------------------------------------
class CVideoMode_OpenGL : public CVideoMode_Common
{
public:
	CVideoMode_OpenGL( bool windowed );

	virtual const char	*GetName( void ) { return "gl"; }
	virtual bool		Init( void *pvInstance );
	virtual void		FlipScreen( void );
	virtual void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );
	virtual void		ReleaseFullScreen( void );
	virtual void		ChangeDisplaySettingsToFullscreen( void );
};

CVideoMode_OpenGL::CVideoMode_OpenGL( bool windowed )
{
	m_bWindowed = windowed;
}

bool CVideoMode_OpenGL::Init( void *pvInstance )
{
	bool bret = CVideoMode_Common::Init( pvInstance );
	SetInitialized( bret );
	return bret;
}

void CVideoMode_OpenGL::ReleaseVideo( void )
{
	if ( IsWindowedMode() )
		return;

	ReleaseFullScreen();
}

void CVideoMode_OpenGL::RestoreVideo( void )
{
	if ( IsWindowedMode() )
		return;

	AdjustWindowForMode();
	ShowWindow( game->GetMainWindow(), SW_SHOWNORMAL );

	if ( qwglGetCurrentContext && qwglMakeCurrent )
	{
		HDC hdc = GetDC( game->GetMainWindow() );
		HGLRC hglrc = qwglGetCurrentContext();

		if ( hglrc )
		{
			if ( !qwglMakeCurrent( hdc, hglrc ) )
				Sys_Error( "Unable to set OpenGL context.\n" );
		}
		else
		{
			Sys_Error( "Unable to get current OpenGL context.\n" );
		}
	}
}

void CVideoMode_OpenGL::ReleaseFullScreen( void )
{
	if ( IsWindowedMode() )
		return;

	ChangeDisplaySettings( NULL, 0 );
	ShowWindow( game->GetMainWindow(), SW_MINIMIZE );
}

void CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen( void )
{
	if ( IsWindowedMode() )
		return;

	vmode_t *mode = GetCurrentMode();
	if ( !mode )
		return;

	DEVMODE dm;
	memset( &dm, 0, sizeof( dm ) );

	dm.dmSize = sizeof( dm );
	dm.dmPelsWidth = mode->width;
	dm.dmPelsHeight = mode->height;
	dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
	dm.dmBitsPerPel = mode->bpp;

	int i;
	if ( i = COM_CheckParm( "-freq" ) )
	{
		int freq = atoi( com_argv[ i + 1 ] );
		dm.dmDisplayFrequency = freq;
		dm.dmFields |= DM_DISPLAYFREQUENCY;
	}

	ChangeDisplaySettings( &dm, CDS_FULLSCREEN );
}

void CVideoMode_OpenGL::FlipScreen( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: the Direct3D video mode, full screen through DirectDraw
//-----------------------------------------------------------------------------
class CVideoMode_Direct3DFullScreen : public CVideoMode_Common
{
public:
	CVideoMode_Direct3DFullScreen( void );
	virtual ~CVideoMode_Direct3DFullScreen( void );

	virtual const char	*GetName( void ) { return "d3d"; }
	virtual bool		Init( void *pvInstance );
	virtual void		Shutdown( void );
	virtual void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );

private:
	bool				IsWindowsNT_guess( void );
	bool				InitDirectDraw_guess( void );
	void				SetFullScreenMode_guess( void );
	void				ShutdownDirectDraw_guess( void );

	LPDIRECTDRAW		m_pDirectDraw_guess;
	LPDIRECTDRAW2		m_pDirectDraw2_guess;
	D3DGlobals			m_D3DGlobals_guess;
};

CVideoMode_Direct3DFullScreen::CVideoMode_Direct3DFullScreen( void )
{
	m_pDirectDraw_guess = NULL;
	m_pDirectDraw2_guess = NULL;
	memset( &m_D3DGlobals_guess, 0, sizeof( m_D3DGlobals_guess ) );
	m_bWindowed = false;
}

CVideoMode_Direct3DFullScreen::~CVideoMode_Direct3DFullScreen( void )
{
}

bool CVideoMode_Direct3DFullScreen::Init( void *pvInstance )
{
	bool bret = CVideoMode_Common::Init( pvInstance );

	if ( bret )
		bret = InitDirectDraw_guess();

	SetInitialized( bret );
	return bret;
}

void CVideoMode_Direct3DFullScreen::RestoreVideo( void )
{
	ShowWindow( game->GetMainWindow(), SW_SHOWNORMAL );
	AdjustWindowForMode();

	if ( GetInitialized() )
		SetFullScreenMode_guess();
}

bool CVideoMode_Direct3DFullScreen::IsWindowsNT_guess( void )
{
	OSVERSIONINFO vinfo;

	vinfo.dwOSVersionInfoSize = sizeof( vinfo );
	GetVersionEx( &vinfo );

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
		return false;

	if ( vinfo.dwPlatformId != VER_PLATFORM_WIN32_NT )
		return false;

	return true;
}

bool CVideoMode_Direct3DFullScreen::InitDirectDraw_guess( void )
{
	HRESULT hr;

	hr = DirectDrawCreate( NULL, &m_pDirectDraw_guess, NULL );
	if ( hr != DD_OK )
		return false;

	if ( !IsWindowsNT_guess() )
	{
		hr = m_pDirectDraw_guess->QueryInterface( IID_IDirectDraw2, (LPVOID *)&m_pDirectDraw2_guess );
		if ( hr != DD_OK )
			return false;
	}

	vmode_t *mode = GetCurrentMode();
	if ( !mode )
		return false;

	hr = m_pDirectDraw_guess->SetCooperativeLevel( NULL, DDSCL_NORMAL );
	if ( hr != DD_OK )
		return false;

	hr = m_pDirectDraw_guess->SetDisplayMode( mode->width, mode->height, mode->bpp );
	if ( hr != DD_OK )
		return false;

	m_pDirectDraw_guess->FlipToGDISurface();

	m_D3DGlobals_guess.bFullScreen_guess = 1;
	m_D3DGlobals_guess.bOwnDisplayMode_guess = 1;
	m_D3DGlobals_guess.lpDD_guess = m_pDirectDraw2_guess;
	m_D3DGlobals_guess.bStencil_guess = 0;

	QGL_D3DShared( &m_D3DGlobals_guess );
	return true;
}

void CVideoMode_Direct3DFullScreen::ReleaseVideo( void )
{
	if ( m_pDirectDraw_guess )
		m_pDirectDraw_guess->RestoreDisplayMode();
}

void CVideoMode_Direct3DFullScreen::SetFullScreenMode_guess( void )
{
	if ( !m_pDirectDraw_guess )
		return;

	vmode_t *mode = GetCurrentMode();

	m_pDirectDraw_guess->SetCooperativeLevel( game->GetMainWindow(), DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT );
	m_pDirectDraw_guess->SetDisplayMode( mode->width, mode->height, mode->bpp );
}

void CVideoMode_Direct3DFullScreen::Shutdown( void )
{
	ShutdownDirectDraw_guess();
	CVideoMode_Common::Shutdown();
}

void CVideoMode_Direct3DFullScreen::ShutdownDirectDraw_guess( void )
{
	if ( !m_pDirectDraw_guess )
		return;

	if ( m_pDirectDraw_guess )
		m_pDirectDraw_guess->FlipToGDISurface();

	if ( m_pDirectDraw2_guess )
	{
		m_pDirectDraw2_guess->Release();
		m_pDirectDraw2_guess = NULL;
	}

	m_pDirectDraw_guess->Release();
	m_pDirectDraw_guess = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: the Direct3D video mode in a window
//-----------------------------------------------------------------------------
class VideoMode_Direct3DWindowed : public CVideoMode_Common
{
public:
	VideoMode_Direct3DWindowed( void );
	virtual ~VideoMode_Direct3DWindowed( void );

	virtual const char	*GetName( void ) { return "d3d"; }
	virtual bool		Init( void *pvInstance );
};

VideoMode_Direct3DWindowed::VideoMode_Direct3DWindowed( void )
{
	m_bWindowed = true;
}

VideoMode_Direct3DWindowed::~VideoMode_Direct3DWindowed( void )
{
}

bool VideoMode_Direct3DWindowed::Init( void *pvInstance )
{
	bool bret = CVideoMode_Common::Init( pvInstance );
	SetInitialized( bret );
	return bret;
}

IVideoMode *videomode;

void VideoMode_Create( void )
{
	bool windowed = false;

	windowed = registry->ReadInt( "ScreenWindowed", 0 ) ? true : false;

	if ( COM_CheckParm( "-sw" ) || COM_CheckParm( "-startwindowed" ) || COM_CheckParm( "-windowed" ) || COM_CheckParm( "-window" ) )
	{
		windowed = true;
	}
	else if ( COM_CheckParm( "-full" ) || COM_CheckParm( "-fullscreen" ) )
	{
		windowed = false;
	}

	registry->WriteInt( "ScreenWindowed", windowed ? 1 : 0 );

	videomode = new CVideoMode_OpenGL( windowed );
}

void VideoMode_SwitchMode( int hardware, int windowed )
{
	registry->WriteString( "EngineDLL", "hw.dll" );
	registry->WriteInt( "EngineD3D", 0 );
	registry->WriteInt( "ScreenWindowed", windowed );
}

void VideoMode_SetVideoMode( int width, int height, int bpp )
{
	registry->WriteInt( "ScreenWidth", width );
	registry->WriteInt( "ScreenHeight", height );
	registry->WriteInt( "ScreenBPP", bpp );
}

void VideoMode_GetVideoModes( struct vmode_s **liststart, int *count )
{
	*count = videomode->GetModeCount();
	*liststart = videomode->GetMode( 0 );
}

void VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp )
{
	vmode_t *mode = videomode->GetCurrentMode();
	if ( !mode )
		return;

	if ( wide )
		*wide = mode->width;
	if ( tall )
		*tall = mode->height;
	if ( bpp )
		*bpp = mode->bpp;
}

void VideoMode_GetCurrentRenderer( char *name, int namelen, int *windowed )
{
	strncpy( name, videomode->GetName(), namelen );

	if ( windowed )
		*windowed = videomode->IsWindowedMode() ? 1 : 0;
}

int VideoMode_IsWindowed( void )
{
	return videomode && videomode->IsWindowedMode();
}
