//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: keeps the characters of the unicode fonts in texture pages
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#undef GetCharABCWidths	// CWin32Font::GetCharABCWidths
#include <malloc.h>
#include <string.h>

#include "FontTextureCache.h"
#include "BaseUISurface.h"
#include "vgui_surfacelib/FontManager.h"
#include "vgui_surfacelib/Win32Font.h"

extern BaseUISurface *staticSurface;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontTextureCache::CFontTextureCache() : m_CharCache( 0, 256, CacheEntryLessFunc )
{
	// set up the linked list of the LRU
	cacheentry_t listHead = { 0, 0 };
	m_LRUListHeadIndex = m_CharCache.Insert( listHead );
	m_CharCache[m_LRUListHeadIndex].nextEntry = m_LRUListHeadIndex;
	m_CharCache[m_LRUListHeadIndex].prevEntry = m_LRUListHeadIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontTextureCache::~CFontTextureCache()
{
}

//-----------------------------------------------------------------------------
// Purpose: orders the cache by font, then by character
//-----------------------------------------------------------------------------
bool CFontTextureCache::CacheEntryLessFunc( const cacheentry_t &lhs, const cacheentry_t &rhs )
{
	if ( lhs.font < rhs.font )
		return true;
	else if ( lhs.font > rhs.font )
		return false;

	return ( lhs.wch < rhs.wch );
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given char & font
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureForChar( HFont font, wchar_t wch, int *textureID, float *texCoords )
{
	// look for the character in the cache
	cacheentry_t cacheitem = { font, wch };

	HCacheEntry cacheHandle = m_CharCache.Find( cacheitem );
	if ( m_CharCache.IsValidIndex( cacheHandle ) )
	{
		// we have an entry already, return that
		int page = m_CharCache[cacheHandle].page;
		*textureID = m_PageList[page].textureID;
		memcpy( texCoords, m_CharCache[cacheHandle].texCoords, sizeof( float ) * 4 );
		return true;
	}

	// get the char details
	CWin32Font *winFont = FontManager().GetFontForChar( font, wch );
	if ( !winFont )
		return false;

	// figure out the size of the char
	int fontTall = winFont->GetHeight();
	int a, b, c;
	winFont->GetCharABCWidths( wch, a, b, c );
	int fontWide = b;
	if ( winFont->GetUnderlined() )
	{
		fontWide += ( a + c );
	}

	// get a texture to render into
	int page, drawX, drawY, twide, ttall;
	if ( !AllocatePageForChar( fontWide, fontTall, page, drawX, drawY, twide, ttall ) )
		return false;

	// create a buffer for the character
	unsigned char *rgba = (unsigned char *)_alloca( fontTall * 4 * fontWide );
	memset( rgba, 0, fontTall * 4 * fontWide );
	winFont->GetCharRGBA( wch, 0, 0, fontWide, fontTall, rgba );

	// upload the new sub texture
	staticSurface->DrawSetTexture( m_PageList[page].textureID );
	staticSurface->DrawSetSubTextureRGBA( m_PageList[page].textureID, drawX, drawY, rgba, fontWide, fontTall );

	// set the cache info
	cacheitem.page = page;
	cacheitem.texCoords[0] = (float)drawX / twide;
	cacheitem.texCoords[1] = (float)drawY / ttall;
	cacheitem.texCoords[2] = (float)( drawX + fontWide ) / twide;
	cacheitem.texCoords[3] = (float)( drawY + fontTall ) / ttall;
	m_CharCache.Insert( cacheitem );

	// return the data
	*textureID = m_PageList[page].textureID;
	memcpy( texCoords, cacheitem.texCoords, sizeof( float ) * 4 );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new page for a given character
//-----------------------------------------------------------------------------
bool CFontTextureCache::AllocatePageForChar( int charWide, int charTall, int &pageIndex, int &drawX, int &drawY, int &twide, int &ttall )
{
	// see if there is room in the last page for this character
	pageIndex = m_PageList.Count() - 1;

	// the pages hold fonts of one power of two height
	int powerOfTwoFontHeight = 16;
	int i = 4;
	while ( powerOfTwoFontHeight < charTall && i < 32 )
	{
		powerOfTwoFontHeight = 1 << i;
		i++;
	}

	// the character does not fit in a page
	if ( powerOfTwoFontHeight > 256 )
		return false;

	if ( pageIndex > -1 && m_PageList[pageIndex].fontHeight == powerOfTwoFontHeight
		&& ( ( m_PageList[pageIndex].nextY + m_PageList[pageIndex].fontHeight ) < m_PageList[pageIndex].tall
		|| ( m_PageList[pageIndex].nextX + charWide ) < m_PageList[pageIndex].wide ) )
	{
		// there is room left in the last page
	}
	else
	{
		// allocate a new page
		pageIndex = m_PageList.AddToTail();
		m_PageList[pageIndex].textureID = staticSurface->CreateNewTextureID( false );
		m_PageList[pageIndex].fontHeight = powerOfTwoFontHeight;
		m_PageList[pageIndex].wide = 256;
		m_PageList[pageIndex].tall = 256;
		m_PageList[pageIndex].nextX = 0;
		m_PageList[pageIndex].nextY = 0;

		// create empty texture
		unsigned char rgba[256 * 256 * 4];
		memset( rgba, 0, sizeof( rgba ) );
		staticSurface->DrawSetTextureRGBAWithAlphaChannel( m_PageList[pageIndex].textureID, rgba, m_PageList[pageIndex].wide, m_PageList[pageIndex].tall, 0, true );
	}

	// see if we need to go down to the next line
	if ( ( m_PageList[pageIndex].nextX + charWide ) >= m_PageList[pageIndex].tall )
	{
		m_PageList[pageIndex].nextX = 0;
		m_PageList[pageIndex].nextY += m_PageList[pageIndex].fontHeight;
	}

	// return the position to draw at
	drawX = m_PageList[pageIndex].nextX;
	drawY = m_PageList[pageIndex].nextY;
	twide = m_PageList[pageIndex].wide;
	ttall = m_PageList[pageIndex].tall;

	// update the next position to draw in
	m_PageList[pageIndex].nextX += charWide;

	return true;
}
