//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: engine text drawing through the VGUI2 surface (console and
//			credits fonts)
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

#include "BaseUISurface.h"
#include "EngineSurface.h"
#include "Color.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

extern "C"
{
#include "vgui_int.h"

void			VGUI2_Draw_Init( void );
void			VGUI2_ResetCurrentTexture( void );
int				VGUI2_Draw_Character( int x, int y, int ch, unsigned int font );
int				VGUI2_Draw_CharacterAdd( int x, int y, int ch, int r, int g, int b, unsigned int font );
int				VGUI2_DrawString( int x, int y, const char *str, unsigned int font );
int				VGUI2_DrawStringClient( int x, int y, const char *str, int r, int g, int b );
int				VGUI2_DrawStringReverseClient( int x, int y, const char *str, int r, int g, int b );
void			VGUI2_Draw_SetTextColor( int r, int g, int b );
void			VGUI2_Draw_SetTextColorAlpha( int r, int g, int b, int a );
int				VGUI2_Draw_StringLen( const char *psz, unsigned int font );
int				VGUI2_Draw_StringLenW( const wchar_t *wsz, unsigned int font );
int				VGUI2_GetFontWide( wchar_t ch, unsigned int font );
const wchar_t	*VGUI2_Find_String( const char *str );
int				VGUI2_MessageFontInfo( short *pWidth, unsigned int font );
}

static HFont _consoleFont;
static HFont _creditsFont;
static Color _col = Color( 255, 255, 255, 255 );

inline void BaseUISurface::ResetViewPort()
{
	_engineSurface->resetViewPort();
}

void VGUI2_Draw_Init( void )
{
	IScheme *pScheme = scheme()->GetIScheme( scheme()->GetDefaultScheme() );

	if ( !_consoleFont )
	{
		_consoleFont = pScheme->GetFont( "EngineFont", true );
	}

	if ( !_creditsFont )
	{
		_creditsFont = pScheme->GetFont( "Legacy_CreditsFont", true );
	}
}

void VGUI2_ResetCurrentTexture( void )
{
	g_BaseUISurface.ResetCurrentTexture();
}

unsigned int VGUI2_GetConsoleFont( void )
{
	return _consoleFont;
}

unsigned int VGUI2_GetCreditsFont( void )
{
	return _creditsFont;
}

int VGUI2_Draw_Character( int x, int y, int ch, unsigned int font )
{
	g_BaseUISurface.ResetViewPort();

	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawSetTextColor( _col );

	if ( iswprint( (wchar_t)ch ) )
	{
		surface()->DrawUnicodeChar( (wchar_t)ch );
		surface()->DrawFlushText();
	}

	int a, b, c;
	surface()->GetCharABCwide( font, ch, a, b, c );
	return a + b + c;
}

int VGUI2_Draw_CharacterAdd( int x, int y, int ch, int r, int g, int b, unsigned int font )
{
	g_BaseUISurface.ResetViewPort();

	VGUI2_Draw_SetTextColor( r, g, b );
	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawSetTextColor( _col );

	if ( iswprint( (wchar_t)ch ) )
	{
		surface()->DrawUnicodeCharAdd( (wchar_t)ch );
	}

	int a, z, c;
	int tall = surface()->GetFontTall( font );
	surface()->GetCharABCwide( font, ch, a, z, c );
	return a + z + c;
}

int VGUI2_DrawString( int x, int y, const char *str, unsigned int font )
{
	g_BaseUISurface.ResetViewPort();

	surface()->DrawSetTextFont( font );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawSetTextColor( _col );

	const wchar_t *localizedString = VGUI2_Find_String( str );

	// replace the characters the font cannot print
	wchar_t *fixString = (wchar_t *)localizedString;
	for ( unsigned int i = 0; i < wcslen( localizedString ); i++ )
	{
		if ( !iswprint( localizedString[i] ) )
		{
			fixString[i] = L' ';
		}
	}

	surface()->DrawPrintText( localizedString, wcslen( localizedString ) );
	surface()->DrawFlushText();

	return VGUI2_Draw_StringLenW( localizedString, font );
}

int VGUI2_DrawStringClient( int x, int y, const char *str, int r, int g, int b )
{
	g_BaseUISurface.ResetViewPort();

	const wchar_t *localizedString = VGUI2_Find_String( str );
	int xpos = x;

	for ( unsigned int i = 0; i < wcslen( localizedString ); i++ )
	{
		int next = VGUI2_Draw_CharacterAdd( xpos, y, localizedString[i], r, g, b, VGUI2_GetCreditsFont() );
		xpos += next;
	}

	return xpos - x;
}

int VGUI2_DrawStringReverseClient( int x, int y, const char *str, int r, int g, int b )
{
	g_BaseUISurface.ResetViewPort();

	const wchar_t *localizedString = VGUI2_Find_String( str );
	const wchar_t *szIt = localizedString + wcslen( localizedString );

	int a, z, c;
	surface()->GetCharABCwide( VGUI2_GetCreditsFont(), szIt[-2], a, z, c );

	int xpos = x - ( a + z + c );

	for ( szIt--; szIt != localizedString - 1; szIt-- )
	{
		int next = VGUI2_Draw_CharacterAdd( xpos, y, *szIt, r, g, b, VGUI2_GetCreditsFont() );
		xpos -= next;
	}

	return x - xpos + ( a + z + c );
}

void VGUI2_Draw_SetTextColor( int r, int g, int b )
{
	_col = Color( r, g, b, 255 );
}

void VGUI2_Draw_SetTextColorAlpha( int r, int g, int b, int a )
{
	_col = Color( r, g, b, a );
}

int VGUI2_Draw_StringLen( const char *psz, unsigned int font )
{
	const wchar_t *localizedString = VGUI2_Find_String( psz );

	int totalWidth = 0;
	for ( unsigned int i = 0; i < wcslen( localizedString ); i++ )
	{
		int a, b, c;
		surface()->GetCharABCwide( font, localizedString[i], a, b, c );
		totalWidth += a + b + c;
	}

	return totalWidth;
}

int VGUI2_Draw_StringLenW( const wchar_t *wsz, unsigned int font )
{
	int totalWidth = 0;
	for ( unsigned int i = 0; i < wcslen( wsz ); i++ )
	{
		int a, b, c;
		surface()->GetCharABCwide( font, wsz[i], a, b, c );
		totalWidth += a + b + c;
	}

	return totalWidth;
}

int VGUI2_GetFontTall( unsigned int font )
{
	wchar_t ch = L' ';
	return surface()->GetFontTall( font );
}

int VGUI2_GetFontWide( wchar_t ch, unsigned int font )
{
	int a, b, c;
	surface()->GetCharABCwide( font, ch, a, b, c );
	return a + b + c;
}

const wchar_t *VGUI2_Find_String( const char *str )
{
	static wchar_t tmpString[1024];
	const wchar_t *localizedString = NULL;

	if ( str && str[0] == '#' )
	{
		localizedString = localize()->Find( str );
	}

	if ( !localizedString )
	{
		localize()->ConvertANSIToUnicode( str, tmpString, sizeof( tmpString ) );
		localizedString = tmpString;
	}

	return localizedString;
}

int VGUI2_MessageFontInfo( short *pWidth, unsigned int font )
{
	int i;

	surface()->DrawSetTextFont( font );

	if ( pWidth )
	{
		for ( i = 0; i < 256; i++ )
		{
			pWidth[i] = VGUI2_GetFontWide( i, font );
		}
	}

	return VGUI2_GetFontTall( font );
}
