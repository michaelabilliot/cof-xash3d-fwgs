// vgui_EngineSurfaceWin32.cpp -- the Windows construction of the engine surface

#include <windows.h>

#include "vgui_EngineSurface.h"

EngineSurface::EngineSurface( void )
{
	_renderPlat = NULL;
	createRenderPlat();
}
