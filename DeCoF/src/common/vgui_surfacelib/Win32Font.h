//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef WIN32FONT_H
#define WIN32FONT_H

#ifdef _WIN32
#pragma once
#endif

#include "UtlRBTree.h"

struct HFONT__;
struct HDC__;
struct HBITMAP__;

//-----------------------------------------------------------------------------
// Purpose: encapsulates a windows font
//-----------------------------------------------------------------------------
class CWin32Font
{
public:
	CWin32Font();
	~CWin32Font();

	// creates the font from windows.  returns false if font does not exist in the OS.
	bool Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// writes the char into the specified 32bpp texture
	void GetCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba );

	// returns true if the font is equivalent to that specified
	bool IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// returns true only if this font is valid for use
	bool IsValid();

	// sets the font as the active one in the device context
	void SetAsActiveFont(HDC__ *hdc);

	// gets the abc widths for a character
	void GetCharABCWidths( int ch, int &a, int &b, int &c );

	// returns the height of the font, in pixels
	int GetHeight();

	// returns the ascent of the font, in pixels (ascent=units above the base line)
	int GetAscent();

	// returns the maximum width of a character, in pixels
	int GetMaxCharWidth();

	// returns the flags used to make this font
	int GetFlags();

	// returns true if this font is underlined
	bool GetUnderlined() { return m_bUnderlined; }

	// gets the name of this font
	const char *GetName() { return m_szName; }

private:
	// effects
	void ApplyRotaryEffectToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba);
	void ApplyScanlineEffectToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba);
	void ApplyDropShadowToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, int charWide, int charTall, unsigned char *rgba);
	void ApplyGaussianBlurToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char *rgba);

	char m_szName[32];
	int m_iTall;
	int m_iWeight;
	int m_iFlags;
	bool m_bAntiAliased;
	bool m_bRotary;
	int m_iDropShadowOffset;
	bool m_bUnderlined;
	int m_iHeight;
	int m_iMaxCharWidth;
	int m_iAscent;

	enum { ABCWIDTHS_CACHE_SIZE = 128 };
	struct abc_t
	{
		char a;
		char b;
		char c;
		char pad_guess;
	};
	// cache for storing asian abc widths (since it's too big too just store them all)
	abc_t m_ABCWidthsCache[ABCWIDTHS_CACHE_SIZE];

	struct abc_cache_t
	{
		wchar_t wch;
		abc_t abc;
	};
	CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;
	static bool ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs);

	HFONT__ *m_hFont;
	HDC__ *m_hDC;
	HBITMAP__ *m_hDIB;

	// pointer to buffer for use when generated bitmap versions of a texture
	int m_rgiBitmapSize[2];
	unsigned char *m_pBuf;

	int m_iScanLines;
	int m_iBlur;
	float *m_pGaussianDistribution;
};

#endif // WIN32FONT_H
