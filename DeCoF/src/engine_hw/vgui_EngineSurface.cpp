// vgui_EngineSurface.cpp -- the engine surface interface (EngineSurface007) and
// its state that does not depend on the renderer

#include <windows.h>

#include "vgui_EngineSurface.h"

EXPOSE_SINGLE_INTERFACE( EngineSurface, IEngineSurface, ENGINESURFACE_INTERFACE_VERSION );

void EngineSurface::freeEngineSurface( void )
{
	if ( s_pEngineSurface )
	{
		delete s_pEngineSurface;
		s_pEngineSurface = NULL;
	}
}

// the texture id createNewTextureID hands out next
static int staticNextTextureID_guess = 5810;

EngineSurface *EngineSurface::s_pEngineSurface = NULL;

EngineSurface::~EngineSurface( void )
{
	deleteRenderPlat();
	_renderPlat = NULL;
	staticNextTextureID_guess = 5810;
	s_pEngineSurface = NULL;
}

void EngineSurface::drawSetColor( int r, int g, int b, int a )
{
	_drawColor[0] = r;
	_drawColor[1] = g;
	_drawColor[2] = b;
	_drawColor[3] = a;
}

void EngineSurface::drawSetTextColor( int r, int g, int b, int a )
{
	if ( _drawTextColor[0] == r && _drawTextColor[1] == g && _drawTextColor[2] == b && _drawTextColor[3] == a )
		return;

	drawFlushText();

	_drawTextColor[0] = r;
	_drawTextColor[1] = g;
	_drawTextColor[2] = b;
	_drawTextColor[3] = a;
}

void EngineSurface::drawSetTextPos( int x, int y )
{
	_drawTextPos[0] = x;
	_drawTextPos[1] = y;
}

void EngineSurface::drawGetTextPos( int &x, int &y )
{
	x = _drawTextPos[0];
	y = _drawTextPos[1];
}

int EngineSurface::createNewTextureID( void )
{
	return staticNextTextureID_guess++;
}
