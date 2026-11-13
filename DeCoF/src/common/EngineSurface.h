//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's drawing interface under the VGUI and VGUI2 surfaces
//
// $NoKeywords: $
//=============================================================================

#ifndef ENGINESURFACE_H
#define ENGINESURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

namespace vgui2
{
class VGuiVertex;
}

class IEngineSurface : public IBaseInterface
{
public:
	virtual void pushMakeCurrent( int *insets, int *absExtents, int *clipRect, bool translateToScreenSpace ) = 0;
	virtual void popMakeCurrent( void ) = 0;
	virtual void drawFilledRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual void drawOutlinedRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual void drawLine( int x0, int y0, int x1, int y1 ) = 0;
	virtual void drawPolyLine( int *px, int *py, int numPoints ) = 0;
	virtual void drawTexturedPolygon( vgui2::VGuiVertex *pVertices, int n ) = 0;
	virtual void drawSetTextureRGBA( int id, const char *rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel ) = 0;
	virtual void drawSetTexture( int id ) = 0;
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual int createNewTextureID( void ) = 0;
	virtual void drawSetColor( int r, int g, int b, int a ) = 0;
	virtual void drawSetTextColor( int r, int g, int b, int a ) = 0;
	virtual void drawSetTextPos( int x, int y ) = 0;
	virtual void drawGetTextPos( int &x, int &y ) = 0;
	virtual void drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 ) = 0;
	virtual void drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 ) = 0;
	virtual void drawSetTextureFile( int id, const char *filename, int hardwareFilter, bool forceReload ) = 0;
	virtual void drawGetTextureSize( int id, int &wide, int &tall ) = 0;
	virtual int isTextureIdValid( int id ) = 0;
	virtual void drawSetSubTextureRGBA( int textureID, int drawX, int drawY, const unsigned char *rgba, int subTextureWide, int subTextureTall ) = 0;
	virtual void drawFlushText( void ) = 0;
	virtual void resetViewPort( void ) = 0;
};

#define ENGINESURFACE_INTERFACE_VERSION "EngineSurface007"

#endif // ENGINESURFACE_H
