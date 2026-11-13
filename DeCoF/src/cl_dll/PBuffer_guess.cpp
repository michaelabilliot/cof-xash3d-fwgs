// Cry of Fear pixel buffer (no build records the name of this file)

#include <string>
#include <list>

#include "hud.h"
#include "cl_util.h"
#include "glmanager.h"
#include "TextureManager_guess.h"
#include "PBuffer_guess.h"

//=========================================================
// Declarations this file takes from wglext.h
//=========================================================
#define WGL_DRAW_TO_PBUFFER_ARB			0x202D
#define WGL_PBUFFER_WIDTH_ARB			0x2034
#define WGL_PBUFFER_HEIGHT_ARB			0x2035
#define WGL_PBUFFER_LOST_ARB			0x2036
#define WGL_SUPPORT_OPENGL_ARB			0x2010
#define WGL_DOUBLE_BUFFER_ARB			0x2011
#define WGL_RED_BITS_ARB				0x2015
#define WGL_GREEN_BITS_ARB				0x2017
#define WGL_BLUE_BITS_ARB				0x2019
#define WGL_ALPHA_BITS_ARB				0x201B
#define WGL_DEPTH_BITS_ARB				0x2022
#define WGL_BIND_TO_TEXTURE_RGBA_ARB	0x2071
#define WGL_TEXTURE_FORMAT_ARB			0x2072
#define WGL_TEXTURE_TARGET_ARB			0x2073
#define WGL_TEXTURE_RGBA_ARB			0x2076
#define WGL_TEXTURE_2D_ARB				0x207A
#define WGL_FRONT_LEFT_ARB				0x2083

typedef const char *( WINAPI *PFNWGLGETEXTENSIONSSTRINGARBPROC )( HDC hdc );
typedef HPBUFFERARB ( WINAPI *PFNWGLCREATEPBUFFERARBPROC )( HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList );
typedef HDC ( WINAPI *PFNWGLGETPBUFFERDCARBPROC )( HPBUFFERARB hPbuffer );
typedef int ( WINAPI *PFNWGLRELEASEPBUFFERDCARBPROC )( HPBUFFERARB hPbuffer, HDC hDC );
typedef BOOL ( WINAPI *PFNWGLDESTROYPBUFFERARBPROC )( HPBUFFERARB hPbuffer );
typedef BOOL ( WINAPI *PFNWGLQUERYPBUFFERARBPROC )( HPBUFFERARB hPbuffer, int iAttribute, int *piValue );
typedef BOOL ( WINAPI *PFNWGLGETPIXELFORMATATTRIBIVARBPROC )( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues );
typedef BOOL ( WINAPI *PFNWGLGETPIXELFORMATATTRIBFVARBPROC )( HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues );
typedef BOOL ( WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC )( HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats );
typedef BOOL ( WINAPI *PFNWGLBINDTEXIMAGEARBPROC )( HPBUFFERARB hPbuffer, int iBuffer );
typedef BOOL ( WINAPI *PFNWGLRELEASETEXIMAGEARBPROC )( HPBUFFERARB hPbuffer, int iBuffer );

PFNWGLGETEXTENSIONSSTRINGARBPROC	wglGetExtensionsStringARB = NULL;
PFNWGLCREATEPBUFFERARBPROC			wglCreatePbufferARB = NULL;
PFNWGLGETPBUFFERDCARBPROC			wglGetPbufferDCARB = NULL;
PFNWGLRELEASEPBUFFERDCARBPROC		wglReleasePbufferDCARB = NULL;
PFNWGLDESTROYPBUFFERARBPROC			wglDestroyPbufferARB = NULL;
PFNWGLQUERYPBUFFERARBPROC			wglQueryPbufferARB = NULL;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC	wglGetPixelFormatAttribivARB = NULL;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC	wglGetPixelFormatAttribfvARB = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC		wglChoosePixelFormatARB = NULL;
PFNWGLBINDTEXIMAGEARBPROC			wglBindTexImageARB = NULL;
PFNWGLRELEASETEXIMAGEARBPROC		wglReleaseTexImageARB = NULL;

// the string is not stringized: the message prints "#funcname()" (original bug)
#define INIT_ENTRY_POINT_guess( funcname, type )	\
	funcname = (type)wglGetProcAddress( #funcname );	\
	if ( !funcname )	\
		gEngfuncs.Con_Printf( "#funcname() not Initialized!\n" );

CPBuffer_guess g_PBuffer_guess;

CPBuffer_guess::CPBuffer_guess()
{
	m_bValid_guess = true;
}

CPBuffer_guess::~CPBuffer_guess()
{
}

void CPBuffer_guess::BindTexture_guess( void )
{
	if ( !wglBindTexImageARB( m_hPBuffer_guess, WGL_FRONT_LEFT_ARB ) )
		gEngfuncs.Con_Printf( "Couldnt bind pbuffer to render texture\n" );
}

void CPBuffer_guess::Create_guess( void )
{
	m_hWindowDC_guess = wglGetCurrentDC();
	m_hWindowRC_guess = wglGetCurrentContext();

	int attribs[] =
	{
		WGL_SUPPORT_OPENGL_ARB, TRUE,
		WGL_DRAW_TO_PBUFFER_ARB, TRUE,
		WGL_BIND_TO_TEXTURE_RGBA_ARB, TRUE,
		WGL_RED_BITS_ARB, 8,
		WGL_GREEN_BITS_ARB, 8,
		WGL_BLUE_BITS_ARB, 8,
		WGL_ALPHA_BITS_ARB, 8,
		WGL_DEPTH_BITS_ARB, 16,
		WGL_DOUBLE_BUFFER_ARB, FALSE,
		0
	};
	int format;
	UINT count = 0;

	wglChoosePixelFormatARB( m_hWindowDC_guess, attribs, NULL, 1, &format, &count );

	if ( !count )
	{
		gEngfuncs.Con_Printf( "Problem Finding Pixel Format\n" );
		m_bValid_guess = false;
	}

	int pbattribs[] =
	{
		WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
		WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB,
		0
	};

	m_iWidth_guess = 1;
	while ( m_iWidth_guess < ScreenWidth )
		m_iWidth_guess *= 2;
	m_iHeight_guess = m_iWidth_guess;

	g_TextureManager_guess.CreateTexture_guess( m_iWidth_guess, m_iHeight_guess, &m_iTexture_guess, GL_TEXTURE_2D, GL_RGBA );

	m_hPBuffer_guess = wglCreatePbufferARB( m_hWindowDC_guess, format, m_iWidth_guess, m_iHeight_guess, pbattribs );
	m_hDC_guess = wglGetPbufferDCARB( m_hPBuffer_guess );
	m_hRC_guess = wglCreateContext( m_hDC_guess );

	if ( !m_hPBuffer_guess )
	{
		gEngfuncs.Con_Printf( "Problem Creating PBuffer\n" );
		m_bValid_guess = false;
	}

	int width, height;
	wglQueryPbufferARB( m_hPBuffer_guess, WGL_PBUFFER_WIDTH_ARB, &width );
	wglQueryPbufferARB( m_hPBuffer_guess, WGL_PBUFFER_HEIGHT_ARB, &height );

	if ( height != m_iHeight_guess || width != m_iWidth_guess )
	{
		gEngfuncs.Con_Printf( "Pbuffer Size doesnt match\n" );
		m_bValid_guess = false;
	}

	if ( !wglMakeCurrent( m_hDC_guess, m_hRC_guess ) )
	{
		gEngfuncs.Con_Printf( "Couldnt make Pbuffer Current\n" );
		m_bValid_guess = false;
	}

	SwitchToWindow_guess();
}

void CPBuffer_guess::MakeCurrent_guess( void )
{
	int lost = 0;

	wglQueryPbufferARB( m_hPBuffer_guess, WGL_PBUFFER_LOST_ARB, &lost );

	if ( lost )
		gEngfuncs.Con_Printf( "Pbuffer was Lost\n" );

	if ( !wglMakeCurrent( m_hDC_guess, m_hRC_guess ) )
		gEngfuncs.Con_Printf( "Couldnt make Pbuffer Current\n" );
}

void CPBuffer_guess::Prepare_guess( void )
{
	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress( "wglGetExtensionsStringARB" );

	if ( !wglGetExtensionsStringARB )
		return;

	const char *extensions = wglGetExtensionsStringARB( wglGetCurrentDC() );

	if ( !isExtensionSupported_guess( "WGL_ARB_pixel_format", extensions ) || !isExtensionSupported_guess( "WGL_ARB_pbuffer", extensions ) )
	{
		m_bValid_guess = false;
		return;
	}

	if ( !m_bValid_guess )
		return;

	INIT_ENTRY_POINT_guess( wglCreatePbufferARB, PFNWGLCREATEPBUFFERARBPROC );
	INIT_ENTRY_POINT_guess( wglGetPbufferDCARB, PFNWGLGETPBUFFERDCARBPROC );
	INIT_ENTRY_POINT_guess( wglReleasePbufferDCARB, PFNWGLRELEASEPBUFFERDCARBPROC );
	INIT_ENTRY_POINT_guess( wglDestroyPbufferARB, PFNWGLDESTROYPBUFFERARBPROC );
	INIT_ENTRY_POINT_guess( wglQueryPbufferARB, PFNWGLQUERYPBUFFERARBPROC );
	INIT_ENTRY_POINT_guess( wglGetPixelFormatAttribivARB, PFNWGLGETPIXELFORMATATTRIBIVARBPROC );
	INIT_ENTRY_POINT_guess( wglGetPixelFormatAttribfvARB, PFNWGLGETPIXELFORMATATTRIBFVARBPROC );
	INIT_ENTRY_POINT_guess( wglChoosePixelFormatARB, PFNWGLCHOOSEPIXELFORMATARBPROC );
	INIT_ENTRY_POINT_guess( wglBindTexImageARB, PFNWGLBINDTEXIMAGEARBPROC );
	INIT_ENTRY_POINT_guess( wglReleaseTexImageARB, PFNWGLRELEASETEXIMAGEARBPROC );

	Create_guess();
}

void CPBuffer_guess::ReleaseTexture_guess( void )
{
	if ( !wglReleaseTexImageARB( m_hPBuffer_guess, WGL_FRONT_LEFT_ARB ) )
		gEngfuncs.Con_Printf( "Couldnt release pbuffer to render texture\n" );
}

void CPBuffer_guess::Shutdown_guess( void )
{
}

void CPBuffer_guess::SwitchToWindow_guess( void )
{
	if ( !wglMakeCurrent( m_hWindowDC_guess, m_hWindowRC_guess ) )
		gEngfuncs.Con_Printf( "Couldnt Restore Renderer\n" );
}
