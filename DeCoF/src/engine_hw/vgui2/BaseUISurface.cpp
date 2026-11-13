//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's VGUI2 surface: popups, cursor, Windows input and the
//			drawing calls, which go to the engine surface (IEngineSurface)
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <string.h>

#include "BaseUISurface.h"
#include "FontTextureCache.h"
#include "HtmlWindow.h"
#include "EngineSurface.h"
#include "vgui_surfacelib/FontManager.h"

#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui/IImage.h>
#include <vgui/Cursor.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Controls.h>
#include "FileSystem.h"
#include "Color.h"

#include "mathlib.h"
#include "const.h"
#include "wrect.h"
typedef int ( *pfnUserMsgHook )( const char *pszName, int iSize, void *pbuf );
#include "cdll_int.h"
#include "cvardef.h"
#include "keydefs.h"
#include "igame.h"

#ifdef CreateFont
#undef CreateFont
#endif

#ifdef PlaySound
#undef PlaySound
#endif

#ifndef IDC_HAND
#define IDC_HAND	MAKEINTRESOURCE(32649)
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x020A
#endif

#ifndef WHEEL_DELTA
#define WHEEL_DELTA		120
#endif

#define WM_XBUTTONDOWN	0x020B
#define WM_XBUTTONUP	0x020C
#define XBUTTON1		0x0001

extern "C"
{
const char	*Key_NameForBinding( const char *pBinding );
int			Key_StringToKeynum( char *str );
int			COM_CheckParm( char *parm );
int			Sys_IsWin95( void );
int			Sys_IsWin98( void );
}

extern "C" __declspec( dllimport ) const char *ISteamApps_GetCurrentGameLanguage( void );

extern cl_enginefunc_t gEngfuncs;
extern cvar_t vgui_emulatemouse;

static KeyCode s_pVirtualKeyToVGUI[256];

static HCURSOR staticDefaultCursor[20];
static HCURSOR staticCurrentCursor = NULL;
static int staticContextCount = 0;
static HWND thisWindow = NULL;

static Dar<VPANEL> staticPopupList;
static CFontTextureCache g_FontTextureCache;

BaseUISurface g_BaseUISurface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( BaseUISurface, ISurface, VGUI_SURFACE_INTERFACE_VERSION, g_BaseUISurface );

IInputInternal *g_InputInternal;

IInputInternal *inputinternal()
{
	return g_InputInternal;
}

bool BaseUISurface::m_bTranslateExtendedKeys = true;

//-----------------------------------------------------------------------------
// Purpose: the VGUI2 key code of the key bound to a command
//-----------------------------------------------------------------------------
KeyCode GetVGUI2KeyCodeForBind( const char *bind )
{
	int num = 0;
	char *keyname = (char *)Key_NameForBinding( bind );
	if ( keyname )
	{
		num = Key_StringToKeynum( _strupr( keyname ) );
	}

	if ( keyname && num > 0 && num < sizeof( s_pVirtualKeyToVGUI ) )
		return s_pVirtualKeyToVGUI[num];

	return KEY_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
BaseUISurface::BaseUISurface()
{
	_restrictedPanel = NULL;
	m_hCurrentFont = NULL;
	_cursorLocked = false;
	m_bTranslateExtendedKeys = true;
	m_iCurrentTexture = 0;
	_needMouse = true;
	_needKB = true;
	m_bVGUI2MouseControl = false;
	m_bFullScreen = false;
	m_deltax = m_deltay = 0;

	staticContextCount++;

	// load up all default cursors
	staticDefaultCursor[dc_none]     = NULL;
	staticDefaultCursor[dc_arrow]    = LoadCursor( NULL, IDC_ARROW );
	staticDefaultCursor[dc_ibeam]    = LoadCursor( NULL, IDC_IBEAM );
	staticDefaultCursor[dc_hourglass]= LoadCursor( NULL, IDC_WAIT );
	staticDefaultCursor[dc_crosshair]= LoadCursor( NULL, IDC_CROSS );
	staticDefaultCursor[dc_up]       = LoadCursor( NULL, IDC_UPARROW );
	staticDefaultCursor[dc_sizenwse] = LoadCursor( NULL, IDC_SIZENWSE );
	staticDefaultCursor[dc_sizenesw] = LoadCursor( NULL, IDC_SIZENESW );
	staticDefaultCursor[dc_sizewe]   = LoadCursor( NULL, IDC_SIZEWE );
	staticDefaultCursor[dc_sizens]   = LoadCursor( NULL, IDC_SIZENS );
	staticDefaultCursor[dc_sizeall]  = LoadCursor( NULL, IDC_SIZEALL );
	staticDefaultCursor[dc_no]       = LoadCursor( NULL, IDC_NO );
	staticDefaultCursor[dc_hand]     = LoadCursor( NULL, IDC_HAND );

	staticCurrentCursor = staticDefaultCursor[dc_arrow];

	// virtual key to vgui key code
	s_pVirtualKeyToVGUI['0'] = KEY_0;
	s_pVirtualKeyToVGUI['1'] = KEY_1;
	s_pVirtualKeyToVGUI['2'] = KEY_2;
	s_pVirtualKeyToVGUI['3'] = KEY_3;
	s_pVirtualKeyToVGUI['4'] = KEY_4;
	s_pVirtualKeyToVGUI['5'] = KEY_5;
	s_pVirtualKeyToVGUI['6'] = KEY_6;
	s_pVirtualKeyToVGUI['7'] = KEY_7;
	s_pVirtualKeyToVGUI['8'] = KEY_8;
	s_pVirtualKeyToVGUI['9'] = KEY_9;
	s_pVirtualKeyToVGUI['A'] = KEY_A;
	s_pVirtualKeyToVGUI['B'] = KEY_B;
	s_pVirtualKeyToVGUI['C'] = KEY_C;
	s_pVirtualKeyToVGUI['D'] = KEY_D;
	s_pVirtualKeyToVGUI['E'] = KEY_E;
	s_pVirtualKeyToVGUI['F'] = KEY_F;
	s_pVirtualKeyToVGUI['G'] = KEY_G;
	s_pVirtualKeyToVGUI['H'] = KEY_H;
	s_pVirtualKeyToVGUI['I'] = KEY_I;
	s_pVirtualKeyToVGUI['J'] = KEY_J;
	s_pVirtualKeyToVGUI['K'] = KEY_K;
	s_pVirtualKeyToVGUI['L'] = KEY_L;
	s_pVirtualKeyToVGUI['M'] = KEY_M;
	s_pVirtualKeyToVGUI['N'] = KEY_N;
	s_pVirtualKeyToVGUI['O'] = KEY_O;
	s_pVirtualKeyToVGUI['P'] = KEY_P;
	s_pVirtualKeyToVGUI['Q'] = KEY_Q;
	s_pVirtualKeyToVGUI['R'] = KEY_R;
	s_pVirtualKeyToVGUI['S'] = KEY_S;
	s_pVirtualKeyToVGUI['T'] = KEY_T;
	s_pVirtualKeyToVGUI['U'] = KEY_U;
	s_pVirtualKeyToVGUI['V'] = KEY_V;
	s_pVirtualKeyToVGUI['W'] = KEY_W;
	s_pVirtualKeyToVGUI['X'] = KEY_X;
	s_pVirtualKeyToVGUI['Y'] = KEY_Y;
	s_pVirtualKeyToVGUI['Z'] = KEY_Z;
	s_pVirtualKeyToVGUI[VK_NUMPAD0] = KEY_PAD_0;
	s_pVirtualKeyToVGUI[VK_NUMPAD1] = KEY_PAD_1;
	s_pVirtualKeyToVGUI[VK_NUMPAD2] = KEY_PAD_2;
	s_pVirtualKeyToVGUI[VK_NUMPAD3] = KEY_PAD_3;
	s_pVirtualKeyToVGUI[VK_NUMPAD4] = KEY_PAD_4;
	s_pVirtualKeyToVGUI[VK_NUMPAD5] = KEY_PAD_5;
	s_pVirtualKeyToVGUI[VK_NUMPAD6] = KEY_PAD_6;
	s_pVirtualKeyToVGUI[VK_NUMPAD7] = KEY_PAD_7;
	s_pVirtualKeyToVGUI[VK_NUMPAD8] = KEY_PAD_8;
	s_pVirtualKeyToVGUI[VK_NUMPAD9] = KEY_PAD_9;
	s_pVirtualKeyToVGUI[VK_DIVIDE] = KEY_PAD_DIVIDE;
	s_pVirtualKeyToVGUI[VK_MULTIPLY] = KEY_PAD_MULTIPLY;
	s_pVirtualKeyToVGUI[VK_SUBTRACT] = KEY_PAD_MINUS;
	s_pVirtualKeyToVGUI[VK_ADD] = KEY_PAD_PLUS;
	s_pVirtualKeyToVGUI[VK_RETURN] = KEY_PAD_ENTER;
	s_pVirtualKeyToVGUI[VK_DECIMAL] = KEY_PAD_DECIMAL;
	s_pVirtualKeyToVGUI[0xdb] = KEY_LBRACKET;
	s_pVirtualKeyToVGUI[0xdd] = KEY_RBRACKET;
	s_pVirtualKeyToVGUI[0xba] = KEY_SEMICOLON;
	s_pVirtualKeyToVGUI[0xde] = KEY_APOSTROPHE;
	s_pVirtualKeyToVGUI[0xc0] = KEY_BACKQUOTE;
	s_pVirtualKeyToVGUI[0xbc] = KEY_COMMA;
	s_pVirtualKeyToVGUI[0xbe] = KEY_PERIOD;
	s_pVirtualKeyToVGUI[0xbf] = KEY_SLASH;
	s_pVirtualKeyToVGUI[0xdc] = KEY_BACKSLASH;
	s_pVirtualKeyToVGUI[0xbd] = KEY_MINUS;
	s_pVirtualKeyToVGUI[0xbb] = KEY_EQUAL;
	s_pVirtualKeyToVGUI[VK_RETURN] = KEY_ENTER;
	s_pVirtualKeyToVGUI[VK_SPACE] = KEY_SPACE;
	s_pVirtualKeyToVGUI[VK_BACK] = KEY_BACKSPACE;
	s_pVirtualKeyToVGUI[VK_TAB] = KEY_TAB;
	s_pVirtualKeyToVGUI[VK_CAPITAL] = KEY_CAPSLOCK;
	s_pVirtualKeyToVGUI[VK_NUMLOCK] = KEY_NUMLOCK;
	s_pVirtualKeyToVGUI[VK_ESCAPE] = KEY_ESCAPE;
	s_pVirtualKeyToVGUI[VK_SCROLL] = KEY_SCROLLLOCK;
	s_pVirtualKeyToVGUI[VK_INSERT] = KEY_INSERT;
	s_pVirtualKeyToVGUI[VK_DELETE] = KEY_DELETE;
	s_pVirtualKeyToVGUI[VK_HOME] = KEY_HOME;
	s_pVirtualKeyToVGUI[VK_END] = KEY_END;
	s_pVirtualKeyToVGUI[VK_PRIOR] = KEY_PAGEUP;
	s_pVirtualKeyToVGUI[VK_NEXT] = KEY_PAGEDOWN;
	s_pVirtualKeyToVGUI[VK_PAUSE] = KEY_BREAK;
	s_pVirtualKeyToVGUI[VK_SHIFT] = KEY_LSHIFT;
	s_pVirtualKeyToVGUI[VK_SHIFT] = KEY_RSHIFT;
	s_pVirtualKeyToVGUI[VK_MENU] = KEY_LALT;
	s_pVirtualKeyToVGUI[VK_MENU] = KEY_RALT;
	s_pVirtualKeyToVGUI[VK_CONTROL] = KEY_LCONTROL;
	s_pVirtualKeyToVGUI[VK_CONTROL] = KEY_RCONTROL;
	s_pVirtualKeyToVGUI[VK_LWIN] = KEY_LWIN;
	s_pVirtualKeyToVGUI[VK_RWIN] = KEY_RWIN;
	s_pVirtualKeyToVGUI[VK_APPS] = KEY_APP;
	s_pVirtualKeyToVGUI[VK_UP] = KEY_UP;
	s_pVirtualKeyToVGUI[VK_LEFT] = KEY_LEFT;
	s_pVirtualKeyToVGUI[VK_DOWN] = KEY_DOWN;
	s_pVirtualKeyToVGUI[VK_RIGHT] = KEY_RIGHT;
	s_pVirtualKeyToVGUI[VK_F1] = KEY_F1;
	s_pVirtualKeyToVGUI[VK_F2] = KEY_F2;
	s_pVirtualKeyToVGUI[VK_F3] = KEY_F3;
	s_pVirtualKeyToVGUI[VK_F4] = KEY_F4;
	s_pVirtualKeyToVGUI[VK_F5] = KEY_F5;
	s_pVirtualKeyToVGUI[VK_F6] = KEY_F6;
	s_pVirtualKeyToVGUI[VK_F7] = KEY_F7;
	s_pVirtualKeyToVGUI[VK_F8] = KEY_F8;
	s_pVirtualKeyToVGUI[VK_F9] = KEY_F9;
	s_pVirtualKeyToVGUI[VK_F10] = KEY_F10;
	s_pVirtualKeyToVGUI[VK_F11] = KEY_F11;
	s_pVirtualKeyToVGUI[VK_F12] = KEY_F12;

	// GetVGUI2KeyCodeForBind looks the engine key number up in this table
	s_pVirtualKeyToVGUI[K_CTRL] = KEY_RCONTROL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
BaseUISurface::~BaseUISurface()
{
	m_bAllowJavaScript = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::Init( VPANEL embeddedPanel, IEngineSurface *engineSurface )
{
	_engineSurface = engineSurface;
	SetEmbeddedPanel( embeddedPanel );
	SetLanguage( ISteamApps_GetCurrentGameLanguage() );
	m_pEmulatedMouseImage = scheme()->GetImage( "gfx/vgui/mouse", false );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the main embedded panel
//-----------------------------------------------------------------------------
void BaseUISurface::SetEmbeddedPanel( VPANEL panel )
{
	_embeddedPanel = panel;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::Shutdown()
{
	// delete all the html windows
	for ( int i = 0; i < GetHTMLWindowCount_guess(); i++ )
	{
		delete GetHTMLWindow_guess( i );
	}

	thisWindow = NULL;

	for ( int j = 0; j < m_CustomFontFileNames.Count(); j++ )
	{
		RemoveFontResource( m_CustomFontFileNames[j].String() );
	}
	m_CustomFontFileNames.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: the popup that was moved to the front last
//-----------------------------------------------------------------------------
VPANEL BaseUISurface::GetTopmostPopup()
{
	if ( staticPopupList.GetCount() )
		return staticPopupList[staticPopupList.GetCount() - 1];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the main embedded panel
//-----------------------------------------------------------------------------
VPANEL BaseUISurface::GetEmbeddedPanel()
{
	return _embeddedPanel;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::RunFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetScreenBounds( int x, int y, int wide, int tall )
{
	m_iSurfaceBounds[0] = x;
	m_iSurfaceBounds[1] = y;
	m_iSurfaceBounds[2] = wide;
	m_iSurfaceBounds[3] = tall;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::GetAbsoluteWindowBounds( int &x, int &y, int &wide, int &tall )
{
	x = m_iSurfaceBounds[0];
	y = m_iSurfaceBounds[1];
	wide = m_iSurfaceBounds[2];
	tall = m_iSurfaceBounds[3];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::GetScreenSize( int &wide, int &tall )
{
	SCREENINFO screenInfo;
	screenInfo.iSize = sizeof( screenInfo );
	gEngfuncs.pfnGetScreenInfo( &screenInfo );
	wide = screenInfo.iWidth;
	tall = screenInfo.iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: the window messages of the engine window, passed on to VGUI2
//-----------------------------------------------------------------------------
void BaseUISurface::AppHandler( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if ( !thisWindow )
	{
		thisWindow = hwnd;
	}

	switch ( uMsg )
	{
	case WM_XBUTTONDOWN:
		{
			if ( HIWORD( wParam ) == XBUTTON1 )
			{
				inputinternal()->InternalMousePressed( MOUSE_4 );
			}
			else
			{
				inputinternal()->InternalMousePressed( MOUSE_5 );
			}
			break;
		}
	case WM_XBUTTONUP:
		{
			if ( HIWORD( wParam ) == XBUTTON1 )
			{
				inputinternal()->InternalMouseReleased( MOUSE_4 );
			}
			else
			{
				inputinternal()->InternalMouseReleased( MOUSE_5 );
			}
			break;
		}
	case WM_SETFOCUS:
		{
			ipanel()->RequestFocus( GetEmbeddedPanel(), 0 );
			break;
		}
	case WM_APP:
		{
			SetFocus( hwnd );
		}
	case WM_SETCURSOR:
		{
			if ( VGUI2MouseControl() )
			{
				g_BaseUISurface.SetCursor( g_BaseUISurface._currentCursor );
			}
			break;
		}
	case WM_MOUSEMOVE:
		{
			inputinternal()->InternalCursorMoved( (short)LOWORD( lParam ), (short)HIWORD( lParam ) );
			break;
		}
	case WM_LBUTTONDOWN:
		{
			SetCapture( hwnd );
			inputinternal()->InternalMousePressed( MOUSE_LEFT );
			break;
		}
	case WM_RBUTTONDOWN:
		{
			SetCapture( hwnd );
			inputinternal()->InternalMousePressed( MOUSE_RIGHT );
			break;
		}
	case WM_MBUTTONDOWN:
		{
			inputinternal()->InternalMousePressed( MOUSE_MIDDLE );
			break;
		}
	case WM_LBUTTONUP:
		{
			ReleaseCapture();
			inputinternal()->InternalMouseReleased( MOUSE_LEFT );
			break;
		}
	case WM_RBUTTONUP:
		{
			ReleaseCapture();
			inputinternal()->InternalMouseReleased( MOUSE_RIGHT );
			break;
		}
	case WM_MBUTTONUP:
		{
			inputinternal()->InternalMouseReleased( MOUSE_MIDDLE );
			break;
		}
	case WM_LBUTTONDBLCLK:
		{
			inputinternal()->InternalMouseDoublePressed( MOUSE_LEFT );
			break;
		}
	case WM_RBUTTONDBLCLK:
		{
			inputinternal()->InternalMouseDoublePressed( MOUSE_RIGHT );
			break;
		}
	case WM_MBUTTONDBLCLK:
		{
			inputinternal()->InternalMouseDoublePressed( MOUSE_MIDDLE );
			break;
		}
	case WM_MOUSEWHEEL:
		{
			inputinternal()->InternalMouseWheeled( ( (short)HIWORD( wParam ) ) / WHEEL_DELTA );
			break;
		}
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			int code = wParam;
			KeyCode keyCode = s_pVirtualKeyToVGUI[code];

			if ( !m_bTranslateExtendedKeys && ( lParam & ( 1 << 24 ) ) && keyCode == KEY_ENTER )
			{
				keyCode = KEY_PAD_ENTER;
			}

			if ( !( lParam & ( 1 << 30 ) ) )
			{
				inputinternal()->InternalKeyCodePressed( keyCode );
			}
			inputinternal()->InternalKeyCodeTyped( keyCode );
			break;
		}
	case WM_CHAR:
	case WM_SYSCHAR:
		{
			int unichar = wParam;
			inputinternal()->InternalKeyTyped( unichar );
			break;
		}
	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			int code = wParam;
			inputinternal()->InternalKeyCodeReleased( s_pVirtualKeyToVGUI[code] );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the cursor and frees the mouse while a visible popup wants
//			mouse input
//-----------------------------------------------------------------------------
void BaseUISurface::CalculateMouseVisible()
{
	int i;
	_needMouse = false;
	_needKB = false;

	for ( i = 0; i < surface()->GetPopupCount(); i++ )
	{
		VPANEL pop = surface()->GetPopup( i );

		bool isVisible = ipanel()->IsVisible( pop );
		VPANEL p = ipanel()->GetParent( pop );

		while ( p && isVisible )
		{
			if ( ipanel()->IsVisible( p ) == false )
			{
				isVisible = false;
				break;
			}
			p = ipanel()->GetParent( p );
		}

		if ( isVisible )
		{
			_needMouse |= ipanel()->IsMouseInputEnabled( pop );
			_needKB |= ipanel()->IsKeyBoardInputEnabled( pop );
		}
	}

	if ( _needMouse )
	{
		SetCursor( dc_arrow );
		UnlockCursor();
	}
	else
	{
		UnlockCursor();
		SetCursor( dc_none );
		LockCursor();

		if ( gEngfuncs.GetWindowCenterX && gEngfuncs.GetWindowCenterY )
		{
			SetCursorPos( gEngfuncs.GetWindowCenterX(), gEngfuncs.GetWindowCenterY() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::NeedKBInput()
{
	return _needKB;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::IsCursorVisible()
{
	return _currentCursor != dc_none;
}

bool s_bCursorVisible = true;
static HCURSOR s_hLastCursor_guess = (HCURSOR)-1;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetCursor( HCursor cursor )
{
	_currentCursor = cursor;

	if ( cursor == dc_user )
	{
		staticCurrentCursor = NULL;
	}
	else
	{
		switch ( cursor )
		{
		case dc_none:
		case dc_arrow:
		case dc_ibeam:
		case dc_hourglass:
		case dc_crosshair:
		case dc_up:
		case dc_sizenwse:
		case dc_sizenesw:
		case dc_sizewe:
		case dc_sizens:
		case dc_sizeall:
		case dc_no:
		case dc_hand:
			staticCurrentCursor = staticDefaultCursor[cursor];
			break;
		}
	}

	if ( !_cursorLocked )
	{
		if ( !vgui_emulatemouse.value )
		{
			if ( staticCurrentCursor != s_hLastCursor_guess )
			{
				s_hLastCursor_guess = staticCurrentCursor;
			}
			::SetCursor( staticCurrentCursor );
		}
		else
		{
			::SetCursor( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::Invalidate( VPANEL panel )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextFont( HFont font )
{
	m_hCurrentFont = font;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawUnicodeChar( wchar_t ch )
{
	if ( !m_hCurrentFont )
		return;

	int x, y;
	DrawGetTextPos( x, y );

	int tall = GetFontTall( m_hCurrentFont );
	int currentTextureID = -1;

	int abcA, abcB, abcC;
	GetCharABCwide( m_hCurrentFont, ch, abcA, abcB, abcC );

	bool bUnderlined = FontManager().GetFontUnderlined( m_hCurrentFont );

	x += abcA;
	int wide = abcB;

	if ( bUnderlined )
	{
		wide += ( abcA + abcC );
		x -= abcA;
	}

	int newTexID = 0;
	float texCoords[4];
	if ( !g_FontTextureCache.GetTextureForChar( m_hCurrentFont, ch, &newTexID, texCoords ) )
		return;

	if ( newTexID != currentTextureID )
	{
		currentTextureID = newTexID;
		DrawSetTexture( newTexID );
	}

	DrawPrintChar( x, y, wide, tall, texCoords[0], texCoords[1], texCoords[2], texCoords[3] );

	x += wide;
	if ( !bUnderlined )
	{
		x += abcC;
	}

	DrawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawUnicodeCharAdd( wchar_t wch )
{
	if ( !m_hCurrentFont )
		return;

	int x, y;
	DrawGetTextPos( x, y );

	int tall = GetFontTall( m_hCurrentFont );
	int currentTextureID = -1;

	int abcA, abcB, abcC;
	GetCharABCwide( m_hCurrentFont, wch, abcA, abcB, abcC );

	x += abcA;
	int wide = abcB;

	int newTexID = 0;
	float texCoords[4];
	if ( !g_FontTextureCache.GetTextureForChar( m_hCurrentFont, wch, &newTexID, texCoords ) )
		return;

	if ( newTexID != currentTextureID )
	{
		currentTextureID = newTexID;
		DrawSetTexture( newTexID );
	}

	_engineSurface->drawPrintCharAdd( x, y, wide, tall, texCoords[0], texCoords[1], texCoords[2], texCoords[3] );

	x += wide;
	x += abcC;

	DrawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawFlushText()
{
	_engineSurface->drawFlushText();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IHTML *BaseUISurface::CreateHTMLWindow( IHTMLEvents *events, VPANEL context )
{
	HtmlWindow *IE = new HtmlWindow( events, context, game->GetMainWindow(), m_bAllowJavaScript, surface()->SupportsFeature( ISurface::DIRECT_HWND_RENDER ) );
	IE->SetVisible( false );
	m_HtmlWindows_guess.AddElement( IE );
	return IE;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DeleteHTMLWindow( IHTML *htmlwin )
{
	HtmlWindow *IE = dynamic_cast<HtmlWindow *>( htmlwin );
	if ( IE )
	{
		m_HtmlWindows_guess.RemoveElement( IE );
		delete IE;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PaintHTMLWindow( IHTML *htmlwin )
{
	HtmlWindow *IE = dynamic_cast<HtmlWindow *>( htmlwin );
	double time_guess = system()->GetCurrentTime();
	POINT ptOrigin_guess = { 0, 0 };
	RECT rcClip_guess = { 0, 0, 0, 0 };
	POINT ptDest_guess = { 0, 0 };

	if ( IE )
	{
		HDC hdc = ::GetDC( thisWindow );
		IE->OnPaint( hdc );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawPrintText( const wchar_t *text, int textLen )
{
	if ( !text )
		return;

	for ( int i = 0; i < textLen; i++ )
	{
		DrawUnicodeChar( text[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::CreatePopup( VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput, bool kbInput )
{
	if ( !ipanel()->GetParent( panel ) )
	{
		ipanel()->SetParent( panel, GetEmbeddedPanel() );
	}
	ipanel()->SetPopup( panel, true );
	staticPopupList.PutElement( panel );
	ipanel()->SetKeyBoardInputEnabled( panel, kbInput );
	ipanel()->SetMouseInputEnabled( panel, mouseInput );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::LockCursor()
{
	_cursorLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::UnlockCursor()
{
	_cursorLocked = false;

	if ( _currentCursor <= dc_none )
	{
		_currentCursor = dc_arrow;
	}

	SetCursor( _currentCursor );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetTitle( VPANEL panel, const wchar_t *title )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::setFullscreenMode( int wide, int tall, int bpp )
{
	m_bFullScreen = true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::setWindowedMode()
{
	m_bFullScreen = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetAsTopMost( bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetAsToolBar( bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::SupportsFeature( SurfaceFeature_e feature )
{
	switch ( feature )
	{
	case ANTIALIASED_FONTS:
		if ( Sys_IsWin95() || Sys_IsWin98() || COM_CheckParm( "-noaafonts" ) )
			return false;
		return true;

	case DROPSHADOW_FONTS:
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::HasFocus()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetTopLevelFocus( VPANEL subFocus )
{
	// walk up the hierarchy until we find what popup panel belongs to
	while ( subFocus )
	{
		if ( ipanel()->IsPopup( subFocus ) )
		{
			BringToFront( subFocus );
			break;
		}

		subFocus = ipanel()->GetParent( subFocus );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::IsWithin( int x, int y )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PushMakeCurrent( VPANEL panel, bool useInsets )
{
	m_iCurrentTexture = 0;

	int insets[4] = { 0, 0, 0, 0 };
	int absExtents[4];
	int clipRect[4];

	if ( useInsets )
	{
		ipanel()->GetInset( panel, insets[0], insets[1], insets[2], insets[3] );
	}

	ipanel()->GetAbsPos( panel, absExtents[0], absExtents[1] );

	int wide, tall;
	ipanel()->GetSize( panel, wide, tall );
	absExtents[2] = absExtents[0] + wide;
	absExtents[3] = absExtents[1] + tall;

	ipanel()->GetClipRect( panel, clipRect[0], clipRect[1], clipRect[2], clipRect[3] );

	_engineSurface->pushMakeCurrent( insets, absExtents, clipRect, false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PopMakeCurrent( VPANEL panel )
{
	_engineSurface->popMakeCurrent();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::EnableMouseCapture( bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SwapBuffers( VPANEL panel )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::ApplyChanges()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawFilledRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawFilledRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawOutlinedRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawLine( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawLine( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawPolyLine( int *px, int *py, int n )
{
	_engineSurface->drawPolyLine( px, py, n );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawTexturedPolygon( VGuiVertex *pVertices, int n )
{
	_engineSurface->drawTexturedPolygon( pVertices, n );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetSubTextureRGBA( int textureID, int drawX, int drawY, const unsigned char *rgba, int subTextureWide, int subTextureTall )
{
	_engineSurface->drawSetSubTextureRGBA( textureID, drawX, drawY, rgba, subTextureWide, subTextureTall );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTexture( int id )
{
	if ( id != m_iCurrentTexture )
	{
		_engineSurface->drawSetTexture( id );
		m_iCurrentTexture = id;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawTexturedRect( x0, y0, x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int BaseUISurface::CreateNewTextureID( bool procedural )
{
	return _engineSurface->createNewTextureID();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetColor( r, g, b, 255 - a );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetColor( Color col )
{
	DrawSetColor( col[0], col[1], col[2], col[3] );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetTextColor( r, g, b, 255 - a );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextColor( Color col )
{
	DrawSetTextColor( col[0], col[1], col[2], col[3] );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextPos( int x, int y )
{
	_engineSurface->drawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawGetTextPos( int &x, int &y )
{
	_engineSurface->drawGetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintChar( x, y, wide, tall, s0, t0, s1, t1 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::AddPanel( VPANEL panel )
{
	if ( ipanel()->IsPopup( panel ) )
	{
		// turn it into a popup menu
		CreatePopup( panel, false, false, false, true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::ReleasePanel( VPANEL panel )
{
	// Remove from popup list if needed and remove any dead popups while we're at it
	staticPopupList.RemoveElement( panel );

	if ( panel == _restrictedPanel )
	{
		_restrictedPanel = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::MovePopupToFront( VPANEL panel )
{
	staticPopupList.MoveElementToEnd( panel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::MovePopupToBack( VPANEL panel )
{
	staticPopupList.RemoveElement( panel );
	staticPopupList.InsertElementAt( panel, 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetPanelVisible( VPANEL panel, bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::ShouldPaintChildPanel( VPANEL panel )
{
	if ( _restrictedPanel && !ipanel()->HasParent( panel, _restrictedPanel ) )
	{
		return false;
	}

	bool isPopup = ipanel()->IsPopup( panel );
	if ( isPopup )
	{
		int index = staticPopupList.FindElement( panel );
		if ( index != -1 )
		{
			ipanel()->Render_SetPopupVisible( staticPopupList[index], true );
		}
	}

	return !isPopup;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PanelRequestFocus( VPANEL panel )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextureFile( int id, const char *filename, int hardwareFilter, bool forceReload )
{
	_engineSurface->drawSetTextureFile( id, filename, hardwareFilter, forceReload );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextureRGBA( int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload )
{
	_engineSurface->drawSetTextureRGBA( id, (const char *)rgba, wide, tall, hardwareFilter, false );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawSetTextureRGBAWithAlphaChannel( int id, const unsigned char *rgba, int wide, int tall, int hardwareFilter, bool forceReload )
{
	_engineSurface->drawSetTextureRGBA( id, (const char *)rgba, wide, tall, hardwareFilter, true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::DrawGetTextureSize( int id, int &wide, int &tall )
{
	_engineSurface->drawGetTextureSize( id, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::IsTextureIDValid( int id )
{
	return _engineSurface->isTextureIdValid( id );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::EnableMouseCapture( VPANEL panel, bool state )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PlaySound( const char *fileName )
{
	gEngfuncs.pfnPlaySoundByName( (char *)fileName, 1.0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetNotifyIcon( Image *, VPANEL, const char * )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::IsMinimized( VPANEL panel )
{
	return ipanel()->Plat( panel ) == (SurfacePlat *)1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetMinimized( VPANEL VPanel, bool state )
{
	if ( state )
	{
		ipanel()->SetPlat( VPanel, (SurfacePlat *)1 );
		ipanel()->SetVisible( VPanel, false );
	}
	else
	{
		ipanel()->SetPlat( VPanel, NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::FlashWindow( VPANEL, bool )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::BringToFront( VPANEL panel )
{
	// move panel to top of list
	ipanel()->MoveToFront( panel );

	// move panel to top of popup list
	if ( ipanel()->IsPopup( panel ) )
	{
		staticPopupList.MoveElementToEnd( panel );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetForegroundWindow( VPANEL panel )
{
	BringToFront( panel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SetModalPanel( VPANEL )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
VPANEL BaseUISurface::GetModalPanel()
{
	return _restrictedPanel;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::SetWatchForComputerUse( bool state )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
double BaseUISurface::GetTimeSinceLastUse()
{
	return 0.0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int BaseUISurface::GetPopupCount()
{
	return staticPopupList.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
VPANEL BaseUISurface::GetPopup( int index )
{
	return staticPopupList[index];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::InternalSolveTraverse( VPANEL panel )
{
	// solve the parent
	ipanel()->Solve( panel );

	// now we can solve the children
	for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
	{
		VPANEL child = ipanel()->GetChild( panel, i );
		if ( ipanel()->IsVisible( child ) )
		{
			InternalSolveTraverse( child );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::InternalThinkTraverse( VPANEL panel )
{
	// think the parent
	ipanel()->Think( panel );

	// and then the children...
	for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
	{
		VPANEL child = ipanel()->GetChild( panel, i );
		if ( ipanel()->IsVisible( child ) )
		{
			InternalThinkTraverse( child );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::InternalSchemeSettingsTraverse( VPANEL panel, bool forceApplySchemeSettings )
{
	for ( int i = 0; i < ipanel()->GetChildCount( panel ); i++ )
	{
		VPANEL child = ipanel()->GetChild( panel, i );
		if ( forceApplySchemeSettings || ipanel()->IsVisible( child ) )
		{
			InternalSchemeSettingsTraverse( child, forceApplySchemeSettings );
		}
	}

	ipanel()->PerformApplySchemeSettings( panel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::SolveTraverse( VPANEL panel, bool forceApplySchemeSettings )
{
	InternalSchemeSettingsTraverse( panel, forceApplySchemeSettings );
	InternalThinkTraverse( panel );
	InternalSolveTraverse( panel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::RestrictPaintToSinglePanel( VPANEL panel )
{
	_restrictedPanel = panel;
	input()->SetAppModalSurface( panel );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::PaintTraverse( VPANEL panel )
{
	if ( !ipanel()->IsVisible( panel ) )
		return;

	if ( panel == GetEmbeddedPanel() )
	{
		VPANEL paintPanel = panel;
		int i;

		if ( _restrictedPanel )
		{
			paintPanel = _restrictedPanel;
		}

		for ( i = 0; i < staticPopupList.GetCount(); i++ )
		{
			ipanel()->Render_SetPopupVisible( staticPopupList[i], false );
		}

		ipanel()->PaintTraverse( paintPanel, true, true );

		for ( i = 0; i < staticPopupList.GetCount(); i++ )
		{
			if ( ipanel()->Render_GetPopupVisible( staticPopupList[i] ) )
			{
				ipanel()->PaintTraverse( staticPopupList[i], true, true );
			}
		}

		if ( _currentCursor != dc_none && vgui_emulatemouse.value )
		{
			int x, y;
			input()->GetCursorPos( x, y );
			m_pEmulatedMouseImage->SetPos( x, y );
			m_pEmulatedMouseImage->Paint();
		}
	}
	else
	{
		ipanel()->PaintTraverse( panel, true, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
HFont BaseUISurface::CreateFont()
{
	return FontManager().CreateFont();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool BaseUISurface::AddGlyphSetToFont( HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int lowRange, int highRange )
{
	return FontManager().AddGlyphSetToFont( font, windowsFontName, tall, weight, blur, scanlines, flags );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int BaseUISurface::GetFontTall( HFont font )
{
	return FontManager().GetFontTall( font );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int BaseUISurface::GetFontAscent( HFont font, wchar_t wch )
{
	return FontManager().GetFontAscent( font, wch );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::GetCharABCwide( HFont font, int ch, int &a, int &b, int &c )
{
	FontManager().GetCharABCwide( font, ch, a, b, c );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int BaseUISurface::GetCharacterWidth( HFont font, int ch )
{
	return FontManager().GetCharacterWidth( font, ch );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::GetTextSize( HFont font, const wchar_t *text, int &wide, int &tall )
{
	FontManager().GetTextSize( font, text, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: adds a custom font file (only supports true type font files (.ttf) for now)
//-----------------------------------------------------------------------------
bool BaseUISurface::AddCustomFontFile( const char *fontFileName )
{
	char fullPath[MAX_PATH];
	// windows needs an absolute path for ttf
	filesystem()->GetLocalPath( fontFileName, fullPath, sizeof( fullPath ) );
	m_CustomFontFileNames.AddToTail( fontFileName );
	return ( AddFontResource( fullPath ) > 0 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void BaseUISurface::GetWorkspaceBounds( int &x, int &y, int &wide, int &tall )
{
	x = 0;
	y = 0;
	ipanel()->GetSize( _embeddedPanel, wide, tall );
}
