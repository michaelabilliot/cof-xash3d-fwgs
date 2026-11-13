//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: caches the font glyphs in textures (FontTextureCache.cpp)
//
// $NoKeywords: $
//=============================================================================

#ifndef FONTTEXTURECACHE_H
#define FONTTEXTURECACHE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "UtlRBTree.h"
#include "UtlVector.h"

using vgui2::HFont;

//-----------------------------------------------------------------------------
// Purpose: manages texture memory for unicode fonts
//-----------------------------------------------------------------------------
class CFontTextureCache
{
public:
	CFontTextureCache();
	~CFontTextureCache();

	// returns a texture ID and a pointer to an array of 4 texture coords for the given character & font
	// uploads more texture if necessary
	bool GetTextureForChar(HFont font, wchar_t wch, int *textureID, float *texCoords);

private:
	typedef unsigned short HCacheEntry;

	// a single character in the cache
	struct cacheentry_t
	{
		HFont font;
		wchar_t wch;
		short page;
		float texCoords[4];

		HCacheEntry nextEntry;	// doubly-linked list for use in the LRU
		HCacheEntry prevEntry;
	};

	// a single texture page
	struct page_t
	{
		short textureID;
		short fontHeight;
		short wide, tall;	// total size of the page
		short nextX, nextY;	// position to draw any new character positions
	};

	// allocates a new page for a given character
	bool AllocatePageForChar(int charWide, int charTall, int &pageIndex, int &drawX, int &drawY, int &twide, int &ttall);

	// Creates font materials
	static bool CacheEntryLessFunc(const cacheentry_t &lhs, const cacheentry_t &rhs);

	CUtlRBTree<cacheentry_t, HCacheEntry> m_CharCache;
	CUtlVector<page_t> m_PageList;
	HCacheEntry m_LRUListHeadIndex;
};

#endif // FONTTEXTURECACHE_H
