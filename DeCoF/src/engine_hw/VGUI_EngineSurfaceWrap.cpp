//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the VGUI1 surface of the engine: Windows input for the VGUI1 app,
//			cursors, and font pages and drawing through IEngineSurface
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <string.h>

#include "VGUI_EngineSurfaceWrap.h"
#include "EngineSurface.h"
#include "vgui2/IMouseControl.h"
#include <vgui/VGuiVertex.h>

#include <VGUI.h>
#include <VGUI_App.h>
#include <VGUI_BitmapTGA.h>
#include <VGUI_Cursor.h>
#include <VGUI_Dar.h>
#include <VGUI_FileInputStream.h>
#include <VGUI_Font.h>
#include <VGUI_ImagePanel.h>
#include <VGUI_KeyCode.h>
#include <VGUI_MouseCode.h>
#include <VGUI_Panel.h>

#include "cvardef.h"

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x020A
#endif

#ifndef WHEEL_DELTA
#define WHEEL_DELTA		120
#endif

using namespace vgui;

extern cvar_t vgui_emulatemouse;

static HCURSOR staticDefaultCursor[20];
static HCURSOR staticCurrentCursor = NULL;
static int staticContextCount = 0;
static KeyCode staticKeyTable_guess[256];

static unsigned char staticRGBA[256 * 256 * 4];

//-----------------------------------------------------------------------------
// Purpose: the texture pages of one VGUI1 font
//-----------------------------------------------------------------------------
class FontInfoVGUI
{
public:
	FontInfoVGUI()
	{
		id = 0;
		pageCount = 0;
		bindIndex[0] = 0;
		bindIndex[1] = 0;
		bindIndex[2] = 0;
		bindIndex[3] = 0;

		for ( int i = 0; i < 256; i++ )
		{
			pageForChar[i] = 0;
		}

		contextCount = -1;
	}

	int id;
	int pageCount;
	int pageForChar[256];
	int bindIndex[4];
	float texCoord[256][4];
	int contextCount;
};

static Font *staticFont;
static FontInfoVGUI *staticFontInfoVGUI;
static Dar<FontInfoVGUI *> staticFontInfoVGUIDar;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
EngineSurfaceWrap::EngineSurfaceWrap( Panel *embeddedPanel, IEngineSurface *engineSurface ) : SurfaceBase( embeddedPanel )
{
	_engineSurface = engineSurface;

	staticFont = NULL;
	staticFontInfoVGUI = NULL;
	staticFontInfoVGUIDar.setCount( 0 );
	_cursorLocked = false;
	staticContextCount++;

	FileInputStream *fis = new FileInputStream( "cryoffear/gfx/vgui/mouse.tga", false );
	_emulatedMouseImage = new BitmapTGA( fis, true );
	fis->close();
	delete fis;

	staticDefaultCursor[Cursor::dc_none] = NULL;
	staticDefaultCursor[Cursor::dc_arrow] = LoadCursor( NULL, IDC_ARROW );
	staticDefaultCursor[Cursor::dc_ibeam] = LoadCursor( NULL, IDC_IBEAM );
	staticDefaultCursor[Cursor::dc_hourglass] = LoadCursor( NULL, IDC_WAIT );
	staticDefaultCursor[Cursor::dc_crosshair] = LoadCursor( NULL, IDC_CROSS );
	staticDefaultCursor[Cursor::dc_up] = LoadCursor( NULL, IDC_UPARROW );
	staticDefaultCursor[Cursor::dc_sizenwse] = LoadCursor( NULL, IDC_SIZENWSE );
	staticDefaultCursor[Cursor::dc_sizenesw] = LoadCursor( NULL, IDC_SIZENESW );
	staticDefaultCursor[Cursor::dc_sizewe] = LoadCursor( NULL, IDC_SIZEWE );
	staticDefaultCursor[Cursor::dc_sizens] = LoadCursor( NULL, IDC_SIZENS );
	staticDefaultCursor[Cursor::dc_sizeall] = LoadCursor( NULL, IDC_SIZEALL );
	staticDefaultCursor[Cursor::dc_no] = LoadCursor( NULL, IDC_NO );
	staticDefaultCursor[Cursor::dc_hand] = LoadCursor( NULL, MAKEINTRESOURCE( 32649 ) );

	staticCurrentCursor = staticDefaultCursor[Cursor::dc_arrow];

	// virtual key to vgui key code
	staticKeyTable_guess['0'] = KEY_0;
	staticKeyTable_guess['1'] = KEY_1;
	staticKeyTable_guess['2'] = KEY_2;
	staticKeyTable_guess['3'] = KEY_3;
	staticKeyTable_guess['4'] = KEY_4;
	staticKeyTable_guess['5'] = KEY_5;
	staticKeyTable_guess['6'] = KEY_6;
	staticKeyTable_guess['7'] = KEY_7;
	staticKeyTable_guess['8'] = KEY_8;
	staticKeyTable_guess['9'] = KEY_9;
	staticKeyTable_guess['A'] = KEY_A;
	staticKeyTable_guess['B'] = KEY_B;
	staticKeyTable_guess['C'] = KEY_C;
	staticKeyTable_guess['D'] = KEY_D;
	staticKeyTable_guess['E'] = KEY_E;
	staticKeyTable_guess['F'] = KEY_F;
	staticKeyTable_guess['G'] = KEY_G;
	staticKeyTable_guess['H'] = KEY_H;
	staticKeyTable_guess['I'] = KEY_I;
	staticKeyTable_guess['J'] = KEY_J;
	staticKeyTable_guess['K'] = KEY_K;
	staticKeyTable_guess['L'] = KEY_L;
	staticKeyTable_guess['M'] = KEY_M;
	staticKeyTable_guess['N'] = KEY_N;
	staticKeyTable_guess['O'] = KEY_O;
	staticKeyTable_guess['P'] = KEY_P;
	staticKeyTable_guess['Q'] = KEY_Q;
	staticKeyTable_guess['R'] = KEY_R;
	staticKeyTable_guess['S'] = KEY_S;
	staticKeyTable_guess['T'] = KEY_T;
	staticKeyTable_guess['U'] = KEY_U;
	staticKeyTable_guess['V'] = KEY_V;
	staticKeyTable_guess['W'] = KEY_W;
	staticKeyTable_guess['X'] = KEY_X;
	staticKeyTable_guess['Y'] = KEY_Y;
	staticKeyTable_guess['Z'] = KEY_Z;
	staticKeyTable_guess[VK_NUMPAD0] = KEY_PAD_0;
	staticKeyTable_guess[VK_NUMPAD1] = KEY_PAD_1;
	staticKeyTable_guess[VK_NUMPAD2] = KEY_PAD_2;
	staticKeyTable_guess[VK_NUMPAD3] = KEY_PAD_3;
	staticKeyTable_guess[VK_NUMPAD4] = KEY_PAD_4;
	staticKeyTable_guess[VK_NUMPAD5] = KEY_PAD_5;
	staticKeyTable_guess[VK_NUMPAD6] = KEY_PAD_6;
	staticKeyTable_guess[VK_NUMPAD7] = KEY_PAD_7;
	staticKeyTable_guess[VK_NUMPAD8] = KEY_PAD_8;
	staticKeyTable_guess[VK_NUMPAD9] = KEY_PAD_9;
	staticKeyTable_guess[VK_DIVIDE] = KEY_PAD_DIVIDE;
	staticKeyTable_guess[VK_MULTIPLY] = KEY_PAD_MULTIPLY;
	staticKeyTable_guess[VK_SUBTRACT] = KEY_PAD_MINUS;
	staticKeyTable_guess[VK_ADD] = KEY_PAD_PLUS;
	staticKeyTable_guess[VK_RETURN] = KEY_PAD_ENTER;
	staticKeyTable_guess[VK_DECIMAL] = KEY_PAD_DECIMAL;
	staticKeyTable_guess[0xdb] = KEY_LBRACKET;
	staticKeyTable_guess[0xdd] = KEY_RBRACKET;
	staticKeyTable_guess[0xba] = KEY_SEMICOLON;
	staticKeyTable_guess[0xde] = KEY_APOSTROPHE;
	staticKeyTable_guess[0xc0] = KEY_BACKQUOTE;
	staticKeyTable_guess[0xbc] = KEY_COMMA;
	staticKeyTable_guess[0xbe] = KEY_PERIOD;
	staticKeyTable_guess[0xbf] = KEY_SLASH;
	staticKeyTable_guess[0xdc] = KEY_BACKSLASH;
	staticKeyTable_guess[0xbd] = KEY_MINUS;
	staticKeyTable_guess[0xbb] = KEY_EQUAL;
	staticKeyTable_guess[VK_RETURN] = KEY_ENTER;
	staticKeyTable_guess[VK_SPACE] = KEY_SPACE;
	staticKeyTable_guess[VK_BACK] = KEY_BACKSPACE;
	staticKeyTable_guess[VK_TAB] = KEY_TAB;
	staticKeyTable_guess[VK_CAPITAL] = KEY_CAPSLOCK;
	staticKeyTable_guess[VK_NUMLOCK] = KEY_NUMLOCK;
	staticKeyTable_guess[VK_ESCAPE] = KEY_ESCAPE;
	staticKeyTable_guess[VK_SCROLL] = KEY_SCROLLLOCK;
	staticKeyTable_guess[VK_INSERT] = KEY_INSERT;
	staticKeyTable_guess[VK_DELETE] = KEY_DELETE;
	staticKeyTable_guess[VK_HOME] = KEY_HOME;
	staticKeyTable_guess[VK_END] = KEY_END;
	staticKeyTable_guess[VK_PRIOR] = KEY_PAGEUP;
	staticKeyTable_guess[VK_NEXT] = KEY_PAGEDOWN;
	staticKeyTable_guess[VK_PAUSE] = KEY_BREAK;
	staticKeyTable_guess[VK_SHIFT] = KEY_LSHIFT;
	staticKeyTable_guess[VK_SHIFT] = KEY_RSHIFT;
	staticKeyTable_guess[VK_MENU] = KEY_LALT;
	staticKeyTable_guess[VK_MENU] = KEY_RALT;
	staticKeyTable_guess[VK_CONTROL] = KEY_LCONTROL;
	staticKeyTable_guess[VK_CONTROL] = KEY_RCONTROL;
	staticKeyTable_guess[VK_LWIN] = KEY_LWIN;
	staticKeyTable_guess[VK_RWIN] = KEY_RWIN;
	staticKeyTable_guess[VK_APPS] = KEY_APP;
	staticKeyTable_guess[VK_UP] = KEY_UP;
	staticKeyTable_guess[VK_LEFT] = KEY_LEFT;
	staticKeyTable_guess[VK_DOWN] = KEY_DOWN;
	staticKeyTable_guess[VK_RIGHT] = KEY_RIGHT;
	staticKeyTable_guess[VK_F1] = KEY_F1;
	staticKeyTable_guess[VK_F2] = KEY_F2;
	staticKeyTable_guess[VK_F3] = KEY_F3;
	staticKeyTable_guess[VK_F4] = KEY_F4;
	staticKeyTable_guess[VK_F5] = KEY_F5;
	staticKeyTable_guess[VK_F6] = KEY_F6;
	staticKeyTable_guess[VK_F7] = KEY_F7;
	staticKeyTable_guess[VK_F8] = KEY_F8;
	staticKeyTable_guess[VK_F9] = KEY_F9;
	staticKeyTable_guess[VK_F10] = KEY_F10;
	staticKeyTable_guess[VK_F11] = KEY_F11;
	staticKeyTable_guess[VK_F12] = KEY_F12;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
EngineSurfaceWrap::~EngineSurfaceWrap()
{
	delete _emulatedMouseImage;
}

//-----------------------------------------------------------------------------
// Purpose: passes a window message on to the VGUI1 app
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::AppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam )
{
	SurfaceBase *surface = this;
	App *app = App::getInstance();
	if ( !surface || !app )
		return;

	switch ( msg )
	{
	case WM_SETFOCUS:
		surface->getPanel()->requestFocus();
		break;
	case WM_MOUSEMOVE:
		app->internalCursorMoved( (short)LOWORD( lparam ), (short)HIWORD( lparam ), surface );
		break;
	case WM_LBUTTONDOWN:
		app->internalMousePressed( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONDOWN:
		app->internalMousePressed( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONDOWN:
		app->internalMousePressed( MOUSE_MIDDLE, surface );
		break;
	case WM_LBUTTONUP:
		app->internalMouseReleased( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONUP:
		app->internalMouseReleased( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONUP:
		app->internalMouseReleased( MOUSE_MIDDLE, surface );
		break;
	case WM_LBUTTONDBLCLK:
		app->internalMouseDoublePressed( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONDBLCLK:
		app->internalMouseDoublePressed( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONDBLCLK:
		app->internalMouseDoublePressed( MOUSE_MIDDLE, surface );
		break;
	case WM_MOUSEWHEEL:
		app->internalMouseWheeled( ( (short)HIWORD( wparam ) ) / WHEEL_DELTA, surface );
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			int code_guess = wparam;

			// bit 30 is set for an auto repeat
			if ( !( lparam & ( 1 << 30 ) ) )
			{
				app->internalKeyPressed( staticKeyTable_guess[code_guess], surface );
				app->internalKeyTyped( staticKeyTable_guess[code_guess], surface );
			}
			else
			{
				app->internalKeyTyped( staticKeyTable_guess[code_guess], surface );
			}
			break;
		}
	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			int code_guess = wparam;
			app->internalKeyReleased( staticKeyTable_guess[code_guess], surface );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::setCursor( Cursor *cursor )
{
	// the VGUI2 surface owns the cursor
	if ( !mousecontrol->VGUI2MouseControl() )
	{
		_currentCursor = cursor;

		if ( !cursor )
		{
			staticCurrentCursor = NULL;
			_emulatedCursor->setImage( NULL );
		}
		else
		{
			_emulatedCursor->setImage( _emulatedMouseImage );

			switch ( cursor->getDefaultCursor() )
			{
			case Cursor::dc_none:
				_emulatedCursor->setImage( NULL );
			case Cursor::dc_arrow:
			case Cursor::dc_ibeam:
			case Cursor::dc_hourglass:
			case Cursor::dc_crosshair:
			case Cursor::dc_up:
			case Cursor::dc_sizenwse:
			case Cursor::dc_sizenesw:
			case Cursor::dc_sizewe:
			case Cursor::dc_sizens:
			case Cursor::dc_sizeall:
			case Cursor::dc_no:
			case Cursor::dc_hand:
				staticCurrentCursor = staticDefaultCursor[cursor->getDefaultCursor()];
				break;
			}
		}

		if ( !_cursorLocked )
		{
			if ( vgui_emulatemouse.value == 0 )
			{
				SetCursor( staticCurrentCursor );
			}
			else
			{
				SetCursor( NULL );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::invalidate( Panel *panel )
{
}

//-----------------------------------------------------------------------------
// Purpose: renders the 256 characters of a font into texture pages
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::drawSetTextFont( Font *font )
{
	staticFont = font;

	if ( !staticFont )
		return;

	bool buildFont = false;
	staticFontInfoVGUI = NULL;

	for ( int i = 0; i < staticFontInfoVGUIDar.getCount(); i++ )
	{
		if ( staticFontInfoVGUIDar[i]->id == staticFont->getId() )
		{
			staticFontInfoVGUI = staticFontInfoVGUIDar[i];
			if ( staticFontInfoVGUI->contextCount != staticContextCount )
			{
				buildFont = true;
				break;
			}
		}
	}

	if ( !staticFontInfoVGUI )
	{
		staticFontInfoVGUI = new FontInfoVGUI();
		staticFontInfoVGUI->id = staticFont->getId();
		staticFontInfoVGUIDar.addElement( staticFontInfoVGUI );
		buildFont = true;
	}

	if ( buildFont )
	{
		staticFontInfoVGUI->contextCount = staticContextCount;

		int currentPage = 0;
		memset( staticRGBA, 0, sizeof( staticRGBA ) );

		int x = 0;
		int y = 0;

		for ( int i = 0; i < 256; i++ )
		{
			int abcA, abcB, abcC;
			staticFont->getCharABCwide( i, abcA, abcB, abcC );

			int wide = abcB;
			int tall = staticFont->getTall();

			// next line
			if ( x + wide + 1 > 256 )
			{
				x = 0;
				y = y + tall + 1;
			}

			// next page
			if ( y + tall + 1 > 256 )
			{
				if ( !staticFontInfoVGUI->bindIndex[currentPage] )
				{
					int bindIndex = createNewTextureID();
					staticFontInfoVGUI->bindIndex[currentPage] = bindIndex;
				}
				drawSetTextureRGBA( staticFontInfoVGUI->bindIndex[currentPage], (const char *)staticRGBA, 256, 256 );

				if ( currentPage >= 3 )
					break;

				currentPage++;
				memset( staticRGBA, 0, sizeof( staticRGBA ) );
				x = 0;
				y = 0;
			}

			staticFont->getCharRGBA( i, x, y, 256, 256, staticRGBA );
			staticFontInfoVGUI->pageForChar[i] = currentPage;
			staticFontInfoVGUI->texCoord[i][0] = x / 256.0f;
			staticFontInfoVGUI->texCoord[i][1] = y / 256.0f;
			staticFontInfoVGUI->texCoord[i][2] = ( x + wide ) / 256.0f;
			staticFontInfoVGUI->texCoord[i][3] = ( y + tall ) / 256.0f;
			x = x + wide + 1;
		}

		// upload the last page
		if ( !staticFontInfoVGUI->bindIndex[currentPage] )
		{
			int bindIndex = createNewTextureID();
			staticFontInfoVGUI->bindIndex[currentPage] = bindIndex;
		}
		drawSetTextureRGBA( staticFontInfoVGUI->bindIndex[currentPage], (const char *)staticRGBA, 256, 256 );
		staticFontInfoVGUI->pageCount = currentPage + 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose: draws a string with the font pages of drawSetTextFont
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::drawPrintText( const char *text, int textLen )
{
	if ( !text )
		return;

	if ( !staticFont )
		return;

	if ( !staticFontInfoVGUI )
		return;

	int x, y;
	drawGetTextPos( x, y );

	int tall = staticFont->getTall();

	for ( int i = 0; i < textLen; i++ )
	{
		unsigned char ch_guess = text[i];

		int abcA, abcB, abcC;
		staticFont->getCharABCwide( ch_guess, abcA, abcB, abcC );
		x += abcA;

		int wide = abcB;
		drawSetTexture( staticFontInfoVGUI->bindIndex[staticFontInfoVGUI->pageForChar[ch_guess]] );
		drawPrintChar( x, y, wide, tall, staticFontInfoVGUI->texCoord[ch_guess][0], staticFontInfoVGUI->texCoord[ch_guess][1], staticFontInfoVGUI->texCoord[ch_guess][2], staticFontInfoVGUI->texCoord[ch_guess][3] );
		x += wide;
		x += abcC;
	}

	drawSetTextPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::createPopup( Panel *embeddedPanel )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::lockCursor()
{
	_cursorLocked = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::unlockCursor()
{
	_cursorLocked = false;
	setCursor( _currentCursor );
}

void EngineSurfaceWrap::setTitle( const char *title )
{
}

bool EngineSurfaceWrap::setFullscreenMode( int wide, int tall, int bpp )
{
	return false;
}

void EngineSurfaceWrap::setWindowedMode()
{
}

void EngineSurfaceWrap::setAsTopMost( bool state )
{
}

bool EngineSurfaceWrap::hasFocus()
{
	return true;
}

bool EngineSurfaceWrap::isWithin( int x, int y )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: makes the panel the current draw target
//-----------------------------------------------------------------------------
void EngineSurfaceWrap::pushMakeCurrent( Panel *panel, bool useInsets )
{
	int insets[4] = { 0, 0, 0, 0 };
	int absExtents[4];
	int clipRect[4];

	if ( useInsets )
	{
		panel->getInset( insets[0], insets[1], insets[2], insets[3] );
	}

	panel->getAbsExtents( absExtents[0], absExtents[1], absExtents[2], absExtents[3] );
	panel->getClipRect( clipRect[0], clipRect[1], clipRect[2], clipRect[3] );

	_engineSurface->pushMakeCurrent( insets, absExtents, clipRect, true );
}

void EngineSurfaceWrap::popMakeCurrent( Panel *panel )
{
	_engineSurface->popMakeCurrent();
}

void EngineSurfaceWrap::enableMouseCapture( bool state )
{
}

void EngineSurfaceWrap::swapBuffers()
{
}

void EngineSurfaceWrap::applyChanges()
{
}

void EngineSurfaceWrap::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawFilledRect( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawOutlinedRect( x0, y0, x1, y1 );
}

void EngineSurfaceWrap::drawPolyLine( int *px, int *py, int n )
{
	_engineSurface->drawPolyLine( px, py, n );
}

void EngineSurfaceWrap::drawTexturedPolygon( vgui2::VGuiVertex *pVertices, int n )
{
	_engineSurface->drawTexturedPolygon( pVertices, n );
}

void EngineSurfaceWrap::drawLine( int x1, int y1, int x2, int y2 )
{
	_engineSurface->drawLine( x1, y1, x2, y2 );
}

void EngineSurfaceWrap::drawSetTextureRGBA( int id, const char *rgba, int wide, int tall )
{
	_engineSurface->drawSetTextureRGBA( id, rgba, wide, tall, 0, 0 );
}

void EngineSurfaceWrap::drawSetTexture( int id )
{
	_engineSurface->drawSetTexture( id );
}

void EngineSurfaceWrap::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_engineSurface->drawTexturedRect( x0, y0, x1, y1 );
}

int EngineSurfaceWrap::createNewTextureID()
{
	return _engineSurface->createNewTextureID();
}

void EngineSurfaceWrap::drawSetColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetColor( r, g, b, a );
}

void EngineSurfaceWrap::drawSetTextColor( int r, int g, int b, int a )
{
	_engineSurface->drawSetTextColor( r, g, b, a );
}

void EngineSurfaceWrap::drawSetTextPos( int x, int y )
{
	_engineSurface->drawSetTextPos( x, y );
}

void EngineSurfaceWrap::drawGetTextPos( int &x, int &y )
{
	_engineSurface->drawGetTextPos( x, y );
}

void EngineSurfaceWrap::drawPrintChar( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintChar( x, y, wide, tall, s0, t0, s1, t1 );
}

void EngineSurfaceWrap::drawPrintCharAdd( int x, int y, int wide, int tall, float s0, float t0, float s1, float t1 )
{
	_engineSurface->drawPrintCharAdd( x, y, wide, tall, s0, t0, s1, t1 );
}
