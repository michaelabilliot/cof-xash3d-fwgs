// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "cmd.h"
#include "GL_MODEL.H"
#include "client.h"
#include "delta.h"
#include "server.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "qgl.h"
#include "mem.h"
#include "filesystem_.h"
#include "winquake.h"

extern int		atismoothing;

extern void		( *VID_FlipScreen_guess )( void );

extern GLenum	oldtarget;

void	GL_SelectTexture( GLenum target );
void	DT_Initialize( void );
void	GL_Config( void );
HINSTANCE	QGL_D3DInit( int unused_guess );
int		ReadPixels3Dfx_guess( int x, int y, int width, int height, byte *buffer );

#define GL_TEXTURE0_SGIS			0x835E
#define GL_TEXTURE1_SGIS			0x835F
#define GL_TEXTURE2_SGIS			0x8360
#define GL_TEXTURE0_ARB				0x84C0
#define GL_TEXTURE1_ARB				0x84C1
#define GL_TEXTURE2_ARB				0x84C2
#define GL_MAX_TEXTURE_UNITS_ARB	0x84E2

qboolean	scr_skiponeupdate;
int			gGLHardwareType = 0;
int			gD3DMode;
int			gl_mtexable;

qboolean	gfMiniDriver_guess;
RECT		window_rect;
int			window_center_x, window_center_y;

const char	*gl_vendor;
const char	*gl_renderer;
const char	*gl_version;
const char	*gl_extensions;

qboolean	scr_skipupdate;

float		gldepthmin, gldepthmax;

void	( APIENTRY * qglPNTrianglesiATI )( GLenum pname, GLint param );

cvar_t	gl_ztrick = { "gl_ztrick", "0" };
cvar_t	gl_d3dflip = { "gl_d3dflip", "0" };
cvar_t	vid_d3d = { "vid_d3d", "0" };

int		texture_mode = GL_LINEAR;

int		texture_extension_number_guess = 1;

int		TEXTURE0_SGIS = GL_TEXTURE0_SGIS;
int		TEXTURE1_SGIS = GL_TEXTURE1_SGIS;
int		TEXTURE2_SGIS = GL_TEXTURE2_SGIS;

/*
================
CheckTextureExtensions
================
*/
void CheckTextureExtensions( void )
{
	qboolean	texture_ext = FALSE;

	if ( vid_d3d.value )
		return;

	if ( Q_strstr( gl_extensions, "GL_EXT_paletted_texture" ) && Q_strstr( gl_extensions, "GL_EXT_shared_texture_palette" ) )
	{
		qglColorTableEXT = (void *)qwglGetProcAddress( "glColorTableEXT" );
		Con_Printf( "Found paletted texture extension.\n" );
	}
	else
	{
		qglColorTableEXT = NULL;
	}

	if ( Q_strstr( gl_extensions, "GL_EXT_texture_object " ) )
		texture_ext = TRUE;

	if ( !texture_ext || COM_CheckParm( "-gl11" ) )
		return;

	qglBindTexture = (void *)qwglGetProcAddress( "glBindTextureEXT" );
	if ( !qglBindTexture )
		Sys_Error( "GetProcAddress for BindTextureEXT failed" );
}

/*
================
CheckArrayExtensions
================
*/
void CheckArrayExtensions( void )
{
	char	*tmp;

	// check for texture extension
	tmp = (char *)qglGetString( GL_EXTENSIONS );
	while ( *tmp )
	{
		if ( Q_strncmp( tmp, "GL_EXT_vertex_array", Q_strlen( "GL_EXT_vertex_array" ) ) == 0 )
		{
			if ( 0 )
			{
				Sys_Error( "GetProcAddress for vertex extension failed" );
				return;
			}
			return;
		}
		tmp++;
	}

	Sys_Error( "Vertex array extension not present" );
}

/*
================
CheckMultiTextureExtensions
================
*/
void CheckMultiTextureExtensions( void )
{
	int		num_guess;

	if ( vid_d3d.value )
		return;

	if ( Q_strstr( gl_extensions, "GL_ARB_multitexture " ) )
	{
		Con_Printf( "ARB Multitexture extensions found.\n" );

		qglMTexCoord2fSGIS = (void *)qwglGetProcAddress( "glMultiTexCoord2fARB" );
		qglSelectTextureSGIS = (void *)qwglGetProcAddress( "glActiveTextureARB" );

		TEXTURE0_SGIS = GL_TEXTURE0_ARB;
		TEXTURE1_SGIS = GL_TEXTURE1_ARB;
		TEXTURE2_SGIS = GL_TEXTURE2_ARB;

		oldtarget = TEXTURE0_SGIS;
		GL_SelectTexture( TEXTURE0_SGIS );
		gl_mtexable = 2;

		if ( Q_strstr( gl_extensions, "GL_ARB_texture_env_combine " ) || Q_strstr( gl_extensions, "GL_EXT_texture_env_combine " ) )
		{
			qglGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &num_guess );

			if ( num_guess > 2 )
			{
				Con_Printf( "%d texture units.  Detail texture supported.\n", num_guess );
				gl_mtexable = num_guess;
				DT_Initialize();
			}
		}
	}
	else if ( Q_strstr( gl_extensions, "GL_SGIS_multitexture " ) )
	{
		Con_Printf( "Multitexture extensions found.\n" );

		qglMTexCoord2fSGIS = (void *)qwglGetProcAddress( "glMTexCoord2fSGIS" );
		qglSelectTextureSGIS = (void *)qwglGetProcAddress( "glSelectTextureSGIS" );

		TEXTURE0_SGIS = GL_TEXTURE0_SGIS;
		TEXTURE1_SGIS = GL_TEXTURE1_SGIS;
		TEXTURE2_SGIS = GL_TEXTURE2_SGIS;

		oldtarget = TEXTURE0_SGIS;
		gl_mtexable = 2;
		GL_SelectTexture( TEXTURE0_SGIS );
	}
	else
	{
		Con_Printf( "NO Multitexture extensions found.\n" );
	}
}

/*
================
CheckATINPatchExtensions
================
*/
void CheckATINPatchExtensions( void )
{
	if ( Q_strstr( gl_extensions, "GL_ATI_pn_triangles" ) )
	{
		qglPNTrianglesiATI = (void *)qwglGetProcAddress( "glPNTrianglesiATI" );
		atismoothing = qglPNTrianglesiATI != NULL;

		if ( atismoothing )
			Con_Printf( "ATI N-Patch Extensions found\n" );
	}
	else
	{
		atismoothing = FALSE;
	}
}

/*
===============
GL_Config
===============
*/
void GL_Config( void )
{
	char	renderer[256];

	if ( vid_d3d.value )
		Cbuf_InsertText( "exec hw/d3d.cfg\n" );
	else
		Cbuf_InsertText( "exec hw/opengl.cfg\n" );

	Q_strncpy( renderer, gl_renderer, sizeof( renderer ) - 1 );
	renderer[sizeof( renderer ) - 1] = 0;
	_strlwr( renderer );

	if ( Q_strstr( gl_vendor, "3Dfx" ) )
	{
		gGLHardwareType = 1;

		if ( Q_strstr( gl_renderer, "Voodoo(tm)" ) )
			Cbuf_InsertText( "exec hw/3DfxVoodoo1.cfg\n" );
		else if ( Q_strstr( gl_renderer, "Voodoo^2" ) )
			Cbuf_InsertText( "exec hw/3DfxVoodoo2.cfg\n" );
		else
			Cbuf_InsertText( "exec hw/3Dfx.cfg\n" );
	}
	else if ( Q_strstr( gl_vendor, "NVIDIA" ) )
	{
		if ( Q_strstr( gl_renderer, "RIVA 128" ) )
		{
			gGLHardwareType = 2;
			Cbuf_InsertText( "exec hw/riva128.cfg\n" );
		}
		else if ( Q_strstr( gl_renderer, "TNT" ) )
		{
			gGLHardwareType = 10;
			Cbuf_InsertText( "exec hw/rivaTNT.cfg\n" );
		}
		else if ( strstr( gl_renderer, "Quadro" ) || strstr( gl_renderer, "GeForce" ) )
		{
			gGLHardwareType = 11;
			Cbuf_InsertText( "exec hw/geforce.cfg\n" );
		}
	}
	else if ( Q_strstr( renderer, "riva tnt" ) || Q_strstr( renderer, "velocity 4400" ) )
	{
		gGLHardwareType = 10;
		Cbuf_InsertText( "exec hw/rivaTNT.cfg\n" );
	}
	else if ( Q_strstr( gl_vendor, "PCX2" ) )
	{
		Cbuf_InsertText( "exec hw/PowerVRPCX2.cfg\n" );
		gGLHardwareType = 3;
	}
	else if ( Q_strstr( gl_vendor, "PowerVR" ) )
	{
		Cbuf_InsertText( "exec hw/PowerVRSG.cfg\n" );
		gGLHardwareType = 4;
	}
	else if ( Q_strstr( gl_vendor, "V2200" ) )
	{
		Cbuf_InsertText( "exec hw/V2200.cfg\n" );
		gGLHardwareType = 5;
	}
	else if ( Q_strstr( gl_vendor, "3Dlabs" ) )
	{
		Cbuf_InsertText( "exec hw/3Dlabs.cfg\n" );
		gGLHardwareType = 6;
	}
	else if ( Q_strstr( gl_vendor, "Matrox" ) )
	{
		gGLHardwareType = 0;
		Cbuf_InsertText( "exec hw/matrox.cfg\n" );
	}
	else if ( Q_strstr( gl_vendor, "ATI" ) && ( Q_strstr( gl_renderer, "Rage" ) || Q_strstr( gl_renderer, "RAGE" ) ) && Q_strstr( gl_renderer, "128" ) )
	{
		gGLHardwareType = 0;
		Cbuf_InsertText( "exec hw/ATIRage128.cfg\n" );
	}
	else
	{
		gGLHardwareType = 0;

		if ( Q_strstr( gl_renderer, "Matrox" ) && Q_strstr( gl_renderer, "G200" ) )
			Cbuf_InsertText( "exec hw/G200d3d.cfg\n" );
		else if ( Q_strstr( gl_renderer, "ATI" ) && ( Q_strstr( gl_renderer, "Rage" ) || Q_strstr( gl_renderer, "RAGE" ) ) && Q_strstr( gl_renderer, "128" ) )
			Cbuf_InsertText( "exec hw/ATIRage128d3d.cfg\n" );
		else if ( Q_strstr( gl_vendor, "NVIDIA" ) )
			Cbuf_InsertText( "exec hw/nvidiad3d.cfg\n" );
	}
}

/*
===============
GL_Init
===============
*/
void GL_Init( void )
{
	gl_vendor = (const char *)qglGetString( GL_VENDOR );
	Con_DPrintf( "GL_VENDOR: %s\n", gl_vendor );
	gl_renderer = (const char *)qglGetString( GL_RENDERER );
	Con_DPrintf( "GL_RENDERER: %s\n", gl_renderer );
	gl_version = (const char *)qglGetString( GL_VERSION );
	Con_DPrintf( "GL_VERSION: %s\n", gl_version );
	gl_extensions = (const char *)qglGetString( GL_EXTENSIONS );

	CheckTextureExtensions();
	CheckMultiTextureExtensions();
	CheckATINPatchExtensions();

	qglClearColor( 1, 0, 0, 0 );
	qglCullFace( GL_FRONT );
	qglEnable( GL_TEXTURE_2D );

	qglEnable( GL_ALPHA_TEST );
	qglAlphaFunc( GL_NOTEQUAL, 0 );

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglShadeModel( GL_FLAT );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );

	GL_Config();
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering( int *x, int *y, int *width, int *height )
{
	*x = *y = 0;
	*width = window_rect.right - window_rect.left;
	*height = window_rect.bottom - window_rect.top;

	vid.width = vid.conwidth = *width;
	vid.height = vid.conheight = *height;

	GLimp_LogNewFrame();
}

/*
=================
GL_EndRendering
=================
*/
void GL_EndRendering( void )
{
	VID_FlipScreen_guess();
}

void VID_ShiftPalette( unsigned char *palette )
{
}

/*
=================
FreeFBOObjects
=================
*/
BOOL FreeFBOObjects( HDC hDC )
{
	static qboolean	bSetup_guess;
	static PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof( PIXELFORMATDESCRIPTOR ),	// size of this pfd
		1,							// version number
		PFD_DRAW_TO_WINDOW			// support window
		| PFD_SUPPORT_OPENGL		// support OpenGL
		| PFD_DOUBLEBUFFER,			// double buffered
		PFD_TYPE_RGBA,				// RGBA type
		32,							// 32-bit color depth
		0, 0, 0, 0, 0, 0,			// color bits ignored
		0,							// no alpha buffer
		0,							// shift bit ignored
		0,							// no accumulation buffer
		0, 0, 0, 0,					// accum bits ignored
		32,							// 32-bit z-buffer
		0,							// no stencil buffer
		0,							// no auxiliary buffer
		PFD_MAIN_PLANE,				// main layer
		0,							// reserved
		0, 0, 0						// layer masks ignored
	};
	int		pixelformat;

	if ( bSetup_guess )
		return TRUE;

	bSetup_guess = TRUE;

	if ( gfMiniDriver_guess || vid_d3d.value )
	{
		if ( ( pixelformat = qwglChoosePixelFormat( hDC, &pfd ) ) == 0 )
		{
			MessageBox( NULL, "ChoosePixelFormat failed", "Error", MB_OK );
			return FALSE;
		}

		if ( qwglSetPixelFormat( hDC, pixelformat, &pfd ) == FALSE )
		{
			MessageBox( NULL, "SetPixelFormat failed", "Error", MB_OK );
			return FALSE;
		}

		qwglDescribePixelFormat( hDC, pixelformat, sizeof( pfd ), &pfd );
	}
	else
	{
		if ( ( pixelformat = ChoosePixelFormat( hDC, &pfd ) ) == 0 )
		{
			MessageBox( NULL, "ChoosePixelFormat failed", "Error", MB_OK );
			return FALSE;
		}

		if ( SetPixelFormat( hDC, pixelformat, &pfd ) == FALSE )
		{
			MessageBox( NULL, "SetPixelFormat failed", "Error", MB_OK );
			return FALSE;
		}

		DescribePixelFormat( hDC, pixelformat, sizeof( pfd ), &pfd );

		if ( ( pfd.dwFlags & PFD_GENERIC_FORMAT ) && !( pfd.dwFlags & PFD_GENERIC_ACCELERATED ) )
			return FALSE;
	}

	return TRUE;
}

/*
=================
GL_SetMode
=================
*/
int GL_SetMode( HWND mainwindow, HDC *pmaindc, HGLRC *pbaseRC, int fD3D, const char *pszDriver, const char *pszCmdLine )
{
	HINSTANCE	hInst;

	gfMiniDriver_guess = FALSE;

	if ( fD3D )
	{
		gD3DMode = TRUE;
		vid_d3d.value = 1;
	}

	if ( pmaindc )
		*pmaindc = NULL;

	if ( pbaseRC )
		*pbaseRC = NULL;

	if ( fD3D )
	{
		hInst = QGL_D3DInit( 0 );
	}
	else
	{
		if ( pszDriver )
		{
			if ( Q_strstr( pszDriver, "opengl32" ) )
				gfMiniDriver_guess = FALSE;
			else
				gfMiniDriver_guess = TRUE;
		}

		hInst = QGL_Init( pszDriver, pszCmdLine );
	}

	if ( !hInst )
	{
		if ( !pszDriver )
			Sys_Error( "Error initializing gl driver, check that the GL driver file opengl32.dll exists" );
		else
			Sys_Error( "Error initializing gl driver, check that the file '%s' exists", pszDriver );

		goto fail;
	}

	*pmaindc = GetDC( mainwindow );

	if ( !FreeFBOObjects( *pmaindc ) )
		goto fail;

	*pbaseRC = qwglCreateContext( *pmaindc );
	if ( !*pbaseRC )
		goto fail;

	if ( !qwglMakeCurrent( *pmaindc, *pbaseRC ) )
		goto fail;

	return 1;

fail:
	if ( hInst )
	{
		qwglMakeCurrent( NULL, NULL );

		if ( *pbaseRC )
			qwglDeleteContext( *pbaseRC );

		if ( *pmaindc )
			ReleaseDC( mainwindow, *pmaindc );

		FreeLibrary( hInst );
	}

	return 0;
}

/*
=================
GL_Shutdown
=================
*/
void GL_Shutdown( HWND hwnd, HDC hdc, HGLRC hglrc )
{
	qwglMakeCurrent( NULL, NULL );

	if ( hglrc )
		qwglDeleteContext( hglrc );

	ReleaseDC( hwnd, hdc );
}

/*
=================
VID_Init
=================
*/
int VID_Init( unsigned short *palette )
{
	int		i_guess;

	Cvar_RegisterVariable( &gl_ztrick );
	Cvar_RegisterVariable( &vid_d3d );

	if ( gD3DMode )
		Cvar_Set( "vid_d3d", "1" );

	Cvar_RegisterVariable( &gl_d3dflip );

	i_guess = COM_CheckParm( "-gl_log" );
	if ( i_guess )
		Cmd_AddCommand( "gl_log", GLimp_EnableLogging );

	return 1;
}

/*
=================
VID_WriteBuffer
=================
*/
void VID_WriteBuffer( const char *pFilename )
{
	char			szFileName[MAX_PATH];
	static char		basefilename_guess[MAX_PATH];
	static int		filenumber_guess;

	if ( pFilename )
	{
		Q_strncpy( basefilename_guess, pFilename, sizeof( basefilename_guess ) - 1 );
		szFileName[sizeof( szFileName ) - 1] = 0;
		filenumber_guess = 1;
	}

	_snprintf( szFileName, sizeof( szFileName ) - 1, "%s%05d.bmp", basefilename_guess, filenumber_guess );
	szFileName[sizeof( szFileName ) - 1] = 0;

	VID_TakeSnapshot( szFileName );
	filenumber_guess++;
}

/*
=================
VID_TakeSnapshot
=================
*/
void VID_TakeSnapshot( const char *pFilename )
{
	FileHandle_t		file;
	BITMAPFILEHEADER	hdr;
	BITMAPINFOHEADER	bi;
	byte				*pb;
	int					i;
	byte				*p;
	byte				*hp;
	byte				b;
	int					imageSize;

	imageSize = vid.width * vid.height * 3;

	file = FS_Open( pFilename, "wb" );
	if ( !file )
		Sys_Error( "Couldn't create file for snapshot.\n" );

	hdr.bfType = 0x4D42;	// 'BM'
	hdr.bfSize = imageSize + sizeof( hdr ) + sizeof( bi );
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = sizeof( hdr ) + sizeof( bi );

	if ( FS_Write( &hdr, sizeof( hdr ), 1, file ) != sizeof( hdr ) )
		Sys_Error( "Couldn't write file header to snapshot.\n" );

	bi.biSize = sizeof( bi );
	bi.biWidth = vid.width;
	bi.biHeight = vid.height;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	if ( FS_Write( &bi, sizeof( bi ), 1, file ) != sizeof( bi ) )
		Sys_Error( "Couldn't write bitmap header to snapshot.\n" );

	hp = (byte *)Mem_Malloc( vid.width * 3 * vid.height );
	if ( !hp )
		Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );

	if ( gGLHardwareType != 1 || !ReadPixels3Dfx_guess( 0, 0, vid.width, vid.height, hp ) )
		qglReadPixels( 0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, hp );

	// swap rgb to bgr
	pb = hp;
	p = pb + 2;
	for ( i = 0; i < (int)( vid.width * vid.height ); i++ )
	{
		b = *pb;
		*pb = *p;
		*p = b;
		p += 3;
		pb += 3;
	}

	i = vid.width * 3 * vid.height;
	if ( i != FS_Write( hp, i, 1, file ) )
		Sys_Error( "Couldn't write bitmap data snapshot.\n" );

	Mem_Free( hp );
	FS_Close( file );
}

/*
=================
VID_TakeSnapshotRect
=================
*/
void VID_TakeSnapshotRect( const char *pFilename, int x, int y, int w, int h )
{
	FileHandle_t		file;
	BITMAPFILEHEADER	hdr;
	BITMAPINFOHEADER	bi;
	byte				*pb;
	int					i;
	byte				*p;
	byte				*hp;
	byte				b;
	int					imageSize;

	imageSize = w * h * 3;

	file = FS_Open( pFilename, "wb" );
	if ( !file )
		Sys_Error( "Couldn't create file for snapshot.\n" );

	hdr.bfType = 0x4D42;	// 'BM'
	hdr.bfSize = imageSize + sizeof( hdr ) + sizeof( bi );
	hdr.bfReserved1 = 0;
	hdr.bfReserved2 = 0;
	hdr.bfOffBits = sizeof( hdr ) + sizeof( bi );

	if ( FS_Write( &hdr, sizeof( hdr ), 1, file ) != sizeof( hdr ) )
		Sys_Error( "Couldn't write file header to snapshot.\n" );

	bi.biSize = sizeof( bi );
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	if ( FS_Write( &bi, sizeof( bi ), 1, file ) != sizeof( bi ) )
		Sys_Error( "Couldn't write bitmap header to snapshot.\n" );

	hp = (byte *)Mem_Malloc( imageSize );
	if ( !hp )
		Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );

	if ( gGLHardwareType != 1 || !ReadPixels3Dfx_guess( x, y, w, h, hp ) )
		qglReadPixels( x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, hp );

	// swap rgb to bgr
	pb = hp;
	p = pb + 2;
	for ( i = 0; i < w * h; i++ )
	{
		b = *pb;
		*pb = *p;
		*p = b;
		p += 3;
		pb += 3;
	}

	if ( imageSize != FS_Write( hp, 1, imageSize, file ) )
		Sys_Error( "Couldn't write bitmap data snapshot.\n" );

	Mem_Free( hp );
	FS_Close( file );
}

void VID_UpdateWindowVars( RECT *prc, int x, int y )
{
	window_rect = *prc;

	if ( pmainwindow )
	{
		GetWindowRect( *pmainwindow, prc );
		window_center_x = ( prc->left + prc->right ) / 2;
		window_center_y = ( prc->top + prc->bottom ) / 2;
	}
	else
	{
		window_center_x = x;
		window_center_y = y;
	}
}

int VID_AllocBuffers( void )
{
	return 1;
}
