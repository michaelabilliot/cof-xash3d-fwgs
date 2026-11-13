//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the VGUI1 surface of the engine, drawn through IEngineSurface
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUI_ENGINESURFACEWRAP_H
#define VGUI_ENGINESURFACEWRAP_H

#ifdef _WIN32
#pragma once
#endif

#include <VGUI_SurfaceBase.h>

namespace vgui
{
class Panel;
class Cursor;
class Font;
class BitmapTGA;
}

namespace vgui2
{
class VGuiVertex;
}

class IEngineSurface;

class EngineSurfaceWrap : public vgui::SurfaceBase
{
public:
	EngineSurfaceWrap( vgui::Panel *embeddedPanel, IEngineSurface *engineSurface );
	~EngineSurfaceWrap();

	virtual void AppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam );
	virtual void setCursor( vgui::Cursor *cursor );
	virtual void lockCursor();
	virtual void unlockCursor();

	virtual void pushMakeCurrent( vgui::Panel *panel, bool useInsets );
	virtual void popMakeCurrent( vgui::Panel *panel );

	virtual void setTitle( const char *title );
	virtual bool setFullscreenMode( int wide, int tall, int bpp );
	virtual void setWindowedMode();
	virtual void setAsTopMost( bool state );
	virtual bool hasFocus();
	virtual bool isWithin( int x, int y );
	virtual void createPopup( vgui::Panel *embeddedPanel );
	virtual void invalidate( vgui::Panel *panel );

	virtual void drawPrintText( const char *text, int textLen );
	virtual void drawSetTextFont( vgui::Font *font );
	virtual void enableMouseCapture( bool state );
	virtual void swapBuffers();
	virtual void applyChanges();

	virtual void drawFilledRect( int x0, int y0, int x1, int y1 );
	virtual void drawOutlinedRect( int x0, int y0, int x1, int y1 );
	virtual void drawLine( int x1, int y1, int x2, int y2 );
	virtual void drawPolyLine( int *px, int *py, int n );
	virtual void drawTexturedPolygon( vgui2::VGuiVertex *pVertices, int n );
	virtual void drawSetTextureRGBA( int id, const char *rgba, int wide, int tall );
	virtual void drawSetTexture( int id );
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 );
	virtual int createNewTextureID();
	virtual void drawSetColor( int r, int g, int b, int a );
	virtual void drawSetTextColor( int r, int g, int b, int a );
	virtual void drawSetTextPos( int x, int y );
	virtual void drawGetTextPos( int &x, int &y );
	virtual void drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 );
	virtual void drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 );

private:
	vgui::BitmapTGA *_emulatedMouseImage;
	IEngineSurface *_engineSurface;
	bool _cursorLocked;
};

#endif // VGUI_ENGINESURFACEWRAP_H
