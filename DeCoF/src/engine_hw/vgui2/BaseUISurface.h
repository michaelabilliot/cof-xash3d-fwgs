//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's implementation of the VGUI2 surface (BaseUISurface.cpp,
//			BaseUISurface_Font.cpp)
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEUISURFACE_H
#define BASEUISURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include <windows.h>

#include <vgui/VGUI.h>
#include <vgui/ISurface.h>
#include <vgui/Dar.h>
#include "UtlVector.h"
#include "utlsymbol.h"
#include "IMouseControl.h"

#ifdef CreateFont
#undef CreateFont
#endif

#ifdef PlaySound
#undef PlaySound
#endif

#ifdef SetCursor
#undef SetCursor
#endif

using namespace vgui2;

class IEngineSurface;
class HtmlWindow;

namespace vgui2
{
class IImage;
class Image;
}

class BaseUISurface : public ISurface, public IMouseControl
{
public:
	BaseUISurface();
	virtual ~BaseUISurface();

	void Init( VPANEL embeddedPanel, IEngineSurface *engineSurface );
	virtual void Shutdown();

	void AppHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	virtual bool IsMinimized( VPANEL panel );
	virtual void RestrictPaintToSinglePanel( VPANEL panel );

	virtual VPANEL GetEmbeddedPanel();
	virtual void SetEmbeddedPanel( VPANEL panel );
	virtual void RunFrame();

	virtual void GetScreenSize( int &wide, int &tall );
	virtual void SetAsTopMost( VPANEL panel, bool state ) {}
	virtual void SetTitle( VPANEL panel, const wchar_t *title );
	virtual void SetAsToolBar( VPANEL panel, bool state ) {}
	virtual VPANEL GetNotifyPanel() { return NULL; }
	virtual void SetNotifyIcon( VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char *text ) {}
	virtual bool RecreateContext( VPANEL panel ) { return true; }
	virtual void SetTopLevelFocus( VPANEL subFocus );
	virtual int GetPopupCount();
	virtual VPANEL GetPopup( int index );
	virtual void SolveTraverse( VPANEL panel, bool forceApplySchemeSettings );
	virtual void PaintTraverse( VPANEL panel );
	virtual void SetCursor( HCursor cursor );
	virtual bool IsCursorVisible();
	virtual void LockCursor();
	virtual void UnlockCursor();
	virtual void PushMakeCurrent( VPANEL panel, bool useInsets );
	virtual void PopMakeCurrent( VPANEL panel );
	virtual bool setFullscreenMode( int wide, int tall, int bpp );
	virtual void setWindowedMode();
	virtual void SetAsTopMost( bool state );
	virtual void SetAsToolBar( bool state );
	virtual bool SupportsFeature( SurfaceFeature_e feature );
	virtual bool HasFocus();
	virtual bool IsWithin( int x, int y );
	virtual void CreatePopup( VPANEL panel, bool minimised, bool showTaskbarIcon = true, bool disabled = false, bool mouseInput = true, bool kbInput = true );
	virtual void Invalidate( VPANEL panel );
	virtual void DrawPrintText( const wchar_t *text, int textLen );
	virtual void DrawUnicodeChar( wchar_t ch );
	virtual void DrawUnicodeCharAdd( wchar_t wch );
	virtual void DrawSetTextFont( HFont font );
	virtual void AddPanel( VPANEL panel );
	virtual void ReleasePanel( VPANEL panel );
	virtual void MovePopupToFront( VPANEL panel );
	virtual void MovePopupToBack( VPANEL panel );
	virtual bool ShouldPaintChildPanel( VPANEL panel );
	virtual void PanelRequestFocus( VPANEL panel );
	virtual void SetPanelVisible( VPANEL panel, bool state );
	virtual void EnableMouseCapture( bool state );
	virtual void ApplyChanges();
	virtual void DrawFilledRect( int x0, int y0, int x1, int y1 );
	virtual void DrawOutlinedRect( int x0, int y0, int x1, int y1 );
	virtual void DrawLine( int x0, int y0, int x1, int y1 );
	virtual void DrawPolyLine( int *px, int *py, int n );
	virtual void DrawTexturedPolygon( VGuiVertex *pVertices, int n );
	virtual void DrawSetTextureRGBA( int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload );
	virtual void DrawSetTexture( int id );
	virtual void DrawTexturedRect( int x0, int y0, int x1, int y1 );
	virtual int CreateNewTextureID( bool procedural = false );
	virtual bool IsTextureIDValid( int id );
	virtual void DrawSetColor( int r, int g, int b, int a );
	virtual void DrawSetColor( Color col );
	virtual void DrawSetTextColor( int r, int g, int b, int a );
	virtual void DrawSetTextColor( Color col );
	virtual void DrawSetTextPos( int x, int y );
	virtual void DrawGetTextPos( int &x, int &y );
	virtual void DrawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 );
	virtual void DrawFlushText();
	virtual void DrawSetTextureFile( int id, const char *filename, int hardwareFilter, bool forceReload );
	virtual void DrawGetTextureSize( int id, int &wide, int &tall );
	virtual void EnableMouseCapture( VPANEL panel, bool state );
	virtual void SwapBuffers( VPANEL panel );
	virtual void PlaySound( const char *fileName );
	virtual void SetNotifyIcon( Image *, VPANEL, const char * );
	virtual void SetMinimized( VPANEL VPanel, bool state );
	virtual void FlashWindow( VPANEL, bool );
	virtual void BringToFront( VPANEL panel );
	virtual void SetForegroundWindow( VPANEL panel );
	virtual void SetModalPanel( VPANEL );
	virtual VPANEL GetModalPanel();
	virtual bool SetWatchForComputerUse( bool state );
	virtual double GetTimeSinceLastUse();
	virtual void GetWorkspaceBounds( int &x, int &y, int &wide, int &tall );

	// fonts
	virtual HFont CreateFont();
	virtual bool AddGlyphSetToFont( HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int lowRange, int highRange );
	virtual int GetFontTall( HFont font );
	virtual int GetFontAscent( HFont font, wchar_t wch );
	virtual void GetCharABCwide( HFont font, int ch, int &a, int &b, int &c );
	virtual int GetCharacterWidth( HFont font, int ch );
	virtual void GetTextSize( HFont font, const wchar_t *text, int &wide, int &tall );
	virtual bool AddCustomFontFile( const char *fontFileName );

	void DrawSetSubTextureRGBA( int textureID, int drawX, int drawY, const unsigned char *rgba, int subTextureWide, int subTextureTall );
	void SetScreenBounds( int x, int y, int wide, int tall );
	virtual void GetAbsoluteWindowBounds( int &x, int &y, int &wide, int &tall );

	virtual VPANEL GetTopmostPopup();
	virtual void SetTranslateExtendedKeys( bool state ) { m_bTranslateExtendedKeys = state; }

	virtual void GetProportionalBase( int &width, int &height ) { width = 640; height = 480; }
	virtual void CalculateMouseVisible();
	virtual bool NeedKBInput();
	virtual bool HasCursorPosFunctions() { return false; }
	virtual void SurfaceGetCursorPos( int &x, int &y ) {}
	virtual void SurfaceSetCursorPos( int x, int y ) {}

	virtual bool VGUI2MouseControl() { return m_bVGUI2MouseControl; }
	virtual void SetVGUI2MouseControl( bool state ) { m_bVGUI2MouseControl = state; }

	virtual void SetAllowHTMLJavaScript( bool state ) { m_bAllowJavaScript = state; }

	void ResetCurrentTexture() { m_iCurrentTexture = 0; }
	void ResetViewPort();	// inline, defined where IEngineSurface is complete (text_draw.cpp)
	void DrawSetTextureRGBAWithAlphaChannel( int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload );

	virtual void SetLanguage( const char *pchLang )
	{
		if ( pchLang )
			strncpy( m_szLanguage, pchLang, sizeof( m_szLanguage ) );
		else
			strcpy( m_szLanguage, "english" );
	}
	virtual const char *GetLanguage() { return m_szLanguage; }

	virtual IHTML *CreateHTMLWindow( IHTMLEvents *events, VPANEL context );
	virtual void PaintHTMLWindow( IHTML *htmlwin );
	virtual void DeleteHTMLWindow( IHTML *htmlwin );

	int GetHTMLWindowCount_guess() { return m_HtmlWindows_guess.GetCount(); }
	HtmlWindow *GetHTMLWindow_guess( int i ) { return m_HtmlWindows_guess[i]; }

	// the mouse movement since the last call (VguiWrap2_GetMouseDelta)
	void GetMouseDelta( int *x, int *y )
	{
		*x = m_deltax;
		*y = m_deltay;
		m_deltax = m_deltay = 0;
	}

private:
	void InternalSolveTraverse( VPANEL panel );
	void InternalThinkTraverse( VPANEL panel );
	void InternalSchemeSettingsTraverse( VPANEL panel, bool forceApplySchemeSettings );

	IImage *m_pEmulatedMouseImage;
	IEngineSurface *_engineSurface;
	VPANEL _embeddedPanel;
	HCursor _currentCursor;
	VPANEL _restrictedPanel;
	HFont m_hCurrentFont;
	int m_iCurrentTexture;
	int m_iSurfaceBounds[4];
	Dar<HtmlWindow *> m_HtmlWindows_guess;
	CUtlVector<CUtlSymbol> m_CustomFontFileNames;
	bool _cursorLocked;
	static bool m_bTranslateExtendedKeys;
	bool _needMouse;
	bool _needKB;
	bool m_bVGUI2MouseControl;
	bool m_bAllowJavaScript;
	char m_szLanguage[32];
	bool m_bFullScreen;
	int m_deltax;
	int m_deltay;
};

extern BaseUISurface g_BaseUISurface;

#endif // BASEUISURFACE_H
