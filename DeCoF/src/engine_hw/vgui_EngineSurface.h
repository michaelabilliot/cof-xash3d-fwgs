// vgui_EngineSurface.h -- the OpenGL implementation of IEngineSurface
// (vgui_EngineSurface.cpp, vgui_EngineSurfaceHW.cpp)

#ifndef VGUI_ENGINESURFACE_H
#define VGUI_ENGINESURFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "EngineSurface.h"

struct EngineSurfaceRenderPlat;

class EngineSurface : public IEngineSurface
{
private:
	EngineSurfaceRenderPlat	*_renderPlat;

	int _drawColor[4];
	int _drawTextColor[4];
	int _drawTextPos[2];

public:
	static EngineSurface *s_pEngineSurface;

	EngineSurface( void );
	virtual ~EngineSurface( void );

	static void freeEngineSurface( void );

	virtual void pushMakeCurrent( int *insets, int *absExtents, int *clipRect, bool translateToScreenSpace );
	virtual void popMakeCurrent( void );
	virtual void drawFilledRect( int x0, int y0, int x1, int y1 );
	virtual void drawOutlinedRect( int x0, int y0, int x1, int y1 );
	virtual void drawLine( int x1, int y1, int x2, int y2 );
	virtual void drawPolyLine( int *px, int *py, int numPoints );
	virtual void drawTexturedPolygon( vgui2::VGuiVertex *pVertices, int n );
	virtual void drawSetTextureRGBA( int id, const char *rgba, int wide, int tall, int hardwareFilter, int hasAlphaChannel );
	virtual void drawSetTexture( int id );
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 );
	virtual int createNewTextureID( void );
	virtual void drawSetColor( int r, int g, int b, int a );
	virtual void drawSetTextColor( int r, int g, int b, int a );
	virtual void drawSetTextPos( int x, int y );
	virtual void drawGetTextPos( int &x, int &y );
	virtual void drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 );
	virtual void drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 );
	virtual void drawSetTextureFile( int id, const char *filename, int hardwareFilter, bool forceReload );
	virtual void drawGetTextureSize( int id, int &wide, int &tall );
	virtual int isTextureIdValid( int id );
	virtual void drawSetSubTextureRGBA( int textureID, int drawX, int drawY, const unsigned char *rgba, int subTextureWide, int subTextureTall );
	virtual void drawFlushText( void );
	virtual void resetViewPort( void );

private:
	void createRenderPlat( void );
	void deleteRenderPlat( void );
};

#endif // VGUI_ENGINESURFACE_H
