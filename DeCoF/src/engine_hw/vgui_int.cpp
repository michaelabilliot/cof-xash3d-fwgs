//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's VGUI entry points: each one hands the call to the
//			VGUI1 layer (vgui_intwrap.cpp) and the VGUI2 layer (vgui_intwrap2.cpp)
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>

#include "vgui_EngineSurface.h"
#include <VGUI_Frame.h>

extern "C"
{
#include "quakedef.h"
#include "filesystem_.h"
#include "vgui_int.h"

void	VGui_Startup( void );
void	VGui_Shutdown( void );
void	VGui_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
void	*VGui_GetPanel( void );
void	VGui_ReleaseMouse( void );
void	VGui_GetMouse( void );
void	VGui_SetVisible( int state );
int		VGui_Key_Event( int down, int keynum, const char *pszCurrentBinding );
int		VGui_LoadBMP( FileHandle_t file, unsigned char *buffer, int bufsize, int *width, int *height );
}

#define DIB_HEADER_MARKER	( (WORD)( 'M' << 8 ) | 'B' )

void VGui_Startup( void )
{
	VGuiWrap_Startup();
	VGuiWrap2_Startup();
}

void VGui_Shutdown( void )
{
	VGuiWrap_Shutdown();
	VGuiWrap2_Shutdown();
	EngineSurface::freeEngineSurface();
}

void VGui_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam )
{
	if ( !VGuiWrap2_CallEngineSurfaceAppHandler( hwnd, msg, wparam, lparam ) )
		VGuiWrap_CallEngineSurfaceAppHandler( hwnd, msg, wparam, lparam );
}

void *VGui_GetPanel( void )
{
	return VGuiWrap_GetPanel();
}

void VGui_ReleaseMouse( void )
{
	VGuiWrap_ReleaseMouse();
	VGuiWrap2_ReleaseMouse();
}

void VGui_GetMouse( void )
{
	VGuiWrap_GetMouse();
	VGuiWrap2_GetMouse();
}

void VGui_SetVisible( int state )
{
	VGuiWrap_SetVisible( state );
	VGuiWrap2_SetVisible( state );
}

void VGui_Paint( void )
{
	if ( VGuiWrap2_UseVGUI1() )
		VGuiWrap_Paint( !VGuiWrap2_IsGameUIVisible() );
	else
		VGuiWrap_Paint( 0 );

	VGuiWrap2_Paint();
}

int VGui_GameUIKeyPressed( void )
{
	return VGuiWrap2_GameUIKeyPressed();
}

int VGui_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	if ( VGuiWrap2_Key_Event( down, keynum, pszCurrentBinding ) )
		return 1;

	return 0;
}

int VGui_LoadBMP( FileHandle_t file, unsigned char *buffer, int bufsize, int *width, int *height )
{
	int success = false;

	// read the file header
	BITMAPFILEHEADER bmfHeader;
	DWORD dwBitsSize, dwFileSize;
	LPBITMAPINFO lpbmi;

	dwFileSize = FS_Size( file );

	FS_Read( &bmfHeader, sizeof( bmfHeader ), 1, file );
	if ( bmfHeader.bfType == DIB_HEADER_MARKER )
	{
		dwBitsSize = dwFileSize - sizeof( bmfHeader );

		HGLOBAL hDIB_guess = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize );
		char *pDIB = (char *)GlobalLock( hDIB_guess );

		FS_Read( pDIB, dwBitsSize, 1, file );

		lpbmi = (LPBITMAPINFO)pDIB;

		*width = lpbmi->bmiHeader.biWidth;
		*height = lpbmi->bmiHeader.biHeight;

		{
			int i, j;
			unsigned char *rgba = (unsigned char *)( pDIB + sizeof( BITMAPINFOHEADER ) + 256 * sizeof( RGBQUAD ) );

			// the palette indices are stored bottom up
			for ( j = 0; j < *height; j++ )
			{
				for ( i = 0; i < *width; i++ )
				{
					int y = *height - j - 1;
					int offs = y * *width + i;
					int offsdest = ( j * *width + i ) * 4;
					unsigned char *src = rgba + offs;
					char *dst = (char *)buffer + offsdest;

					dst[0] = lpbmi->bmiColors[*src].rgbRed;
					dst[1] = lpbmi->bmiColors[*src].rgbGreen;
					dst[2] = lpbmi->bmiColors[*src].rgbBlue;
					dst[3] = (char)255;
				}
			}
		}

		success = true;

		GlobalUnlock( hDIB_guess );
		GlobalFree( hDIB_guess );
	}

	FS_Close( file );

	return success;
}
