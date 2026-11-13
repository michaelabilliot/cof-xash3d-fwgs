//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "vgui_surfacelib/FontManager.h"
#include "vgui_surfacelib/Win32Font.h"
#include <ctype.h>
#include <locale.h>
#include <string.h>

static CFontManager s_FontManager;

extern bool s_bSupportsUnicode;

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CFontManager &FontManager()
{
	return s_FontManager;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontManager::CFontManager()
{
	// add a single empty font, to act as an invalid font handle 0
	m_FontAmalgams.EnsureCapacity(100);
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity(100);

	// setup our text locale
	setlocale( LC_CTYPE, "" );
	setlocale( LC_TIME, "" );
	setlocale( LC_COLLATE, "" );
	setlocale( LC_MONETARY, "" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontManager::~CFontManager()
{
	ClearAllFonts();
}

//-----------------------------------------------------------------------------
// Purpose: sets the language the fonts are used for
//-----------------------------------------------------------------------------
void CFontManager::SetLanguage(const char *language)
{
	strncpy(m_szLanguage, language, sizeof(m_szLanguage));
}

//-----------------------------------------------------------------------------
// Purpose: frees the fonts
//-----------------------------------------------------------------------------
void CFontManager::ClearAllFonts()
{
	for (int i = 0; i < m_Win32Fonts.Count(); i++)
	{
		delete m_Win32Fonts[i];
	}
	m_Win32Fonts.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new font handle
//-----------------------------------------------------------------------------
HFont CFontManager::CreateFont()
{
	int i = m_FontAmalgams.AddToTail();
	return i;
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::AddGlyphSetToFont(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if (m_FontAmalgams[font].GetCount() > 0)
	{
		// clear out the existing font
		return false;
	}

	// see if we already have the win32 font
	CWin32Font *winFont = CreateOrFindWin32Font(windowsFontName, tall, weight, blur, scanlines, flags);

	// cycle until valid english/extended font support has been created
	do
	{
		// add to the amalgam
		if (IsFontForeignLanguageCapable(windowsFontName))
		{
			if (winFont)
			{
				// font supports the full range of characters
				m_FontAmalgams[font].AddFont(winFont, 0x0000, 0xFFFF);
				return true;
			}
		}
		else
		{
			// font cannot provide glyphs and just supports the normal range
			// redirect to a font that can supports all the glyphs
			const char *localizedFontName = GetForeignFallbackFontName();
			if (winFont && !_stricmp(localizedFontName, windowsFontName))
			{
				// it's the same font and can support the full range
				m_FontAmalgams[font].AddFont(winFont, 0x0000, 0xFFFF);
				return true;
			}

			// create the extended support font
			CWin32Font *asianFont = CreateOrFindWin32Font(localizedFontName, tall, weight, blur, scanlines, flags);
			if (winFont && asianFont)
			{
				// use the normal font for english characters, and the asian font for the rest
				m_FontAmalgams[font].AddFont(winFont, 0x0000, 0x00FF);
				m_FontAmalgams[font].AddFont(asianFont, 0x0100, 0xFFFF);
				return true;
			}
			else if (asianFont)
			{
				// the normal font failed to create
				// just use the asian font for the full range
				m_FontAmalgams[font].AddFont(asianFont, 0x0000, 0xFFFF);
				return true;
			}
		}
		// no valid font has been created, so fallback to a different font and try again
	}
	while (NULL != (windowsFontName = GetFallbackFontName(windowsFontName)));

	// nothing successfully created
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
CWin32Font *CFontManager::CreateOrFindWin32Font(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	CWin32Font *winFont = NULL;
	int i;
	for (i = 0; i < m_Win32Fonts.Count(); i++)
	{
		if (m_Win32Fonts[i]->IsEqualTo(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			winFont = m_Win32Fonts[i];
			break;
		}
	}

	// create the new win32font if we didn't find it
	if (!winFont)
	{
		i = m_Win32Fonts.AddToTail();
		m_Win32Fonts[i] = new CWin32Font();
		if (m_Win32Fonts[i]->Create(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			// add to the list
			winFont = m_Win32Fonts[i];
		}
		else
		{
			// failed to create, remove
			delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}
	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: finds a font handle given a name, returns 0 if not found
//-----------------------------------------------------------------------------
HFont CFontManager::GetFontByName(const char *name)
{
	for (int i = 1; i < m_FontAmalgams.Count(); i++)
	{
		if (!_stricmp(name, m_FontAmalgams[i].Name()))
		{
			return i;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: gets the windows font for the particular font in the amalgam
//-----------------------------------------------------------------------------
CWin32Font *CFontManager::GetFontForChar(HFont font, wchar_t wch)
{
	return m_FontAmalgams[font].GetFontForChar(wch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CFontManager::GetCharABCwide(HFont font, int ch, int &a, int &b, int &c)
{
	CWin32Font *winFont = m_FontAmalgams[font].GetFontForChar(ch);
	if (winFont)
	{
		winFont->GetCharABCWidths(ch, a, b, c);
	}
	else
	{
		// no font for this range, just use the default width
		a = c = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontTall(HFont font)
{
	return m_FontAmalgams[font].GetFontHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontAscent(HFont font, wchar_t wch)
{
	CWin32Font *winFont = m_FontAmalgams[font].GetFontForChar(wch);
	if (winFont)
	{
		return winFont->GetAscent();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CFontManager::GetCharacterWidth(HFont font, int ch)
{
	if (iswprint(ch))
	{
		int a, b, c;
		GetCharABCwide(font, ch, a, b, c);
		return (a + b + c);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CFontManager::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	wide = 0;
	tall = 0;

	if (!text)
		return;

	tall = GetFontTall(font);

	int xx = 0;
	for (int i = 0; ; i++)
	{
		wchar_t ch = text[i];
		if (ch == 0)
		{
			break;
		}
		else if (ch == '\n')
		{
			tall += GetFontTall(font);
			xx=0;
		}
		else if (ch == '&')
		{
			// underscore character, so skip
		}
		else
		{
			xx += GetCharacterWidth(font, ch);
			if (xx > wide)
			{
				wide = xx;
			}
		}
	}
}

// font validation functions
// list of fonts that will be considered for asian languages
static const char *g_szValidAsianFonts[] = { "Marlett", NULL };

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is in the list of OK asian fonts
//-----------------------------------------------------------------------------
bool CFontManager::IsFontForeignLanguageCapable(const char *windowsFontName)
{
	for (int i = 0; g_szValidAsianFonts[i] != NULL; i++)
	{
		if (!_stricmp(g_szValidAsianFonts[i], windowsFontName))
			return true;
	}

	// typeface isn't supported by asian languages
	return false;
}

// list of fonts that will be used if the specified font is not available
struct FallbackFont_t
{
	const char *font;
	const char *fallbackFont;
};

FallbackFont_t g_FallbackFonts[] =
{
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
	{ "Tahoma", NULL },
	{ NULL, "Tahoma" },		// every other font falls back to this
};

//-----------------------------------------------------------------------------
// Purpose: get the fallback font name for the specified font
//-----------------------------------------------------------------------------
const char *CFontManager::GetFallbackFontName(const char *windowsFontName)
{
	int i;
	for ( i = 0; g_FallbackFonts[i].font != NULL; i++ )
	{
		if (!_stricmp(g_FallbackFonts[i].font, windowsFontName))
			break;
	}

	// the ultimate fallback
	return g_FallbackFonts[i].fallbackFont;
}

struct Win98ForeignFallbackFont_t
{
	const char *language;
	const char *fallbackFont;
};

// list of fonts that will be used if the specified language font is not available
Win98ForeignFallbackFont_t g_Win98ForeignFallbackFonts[] =
{
	{ "russian", "system" },
	{ "japanese", "win98japanese" },
	{ "thai", "system" },
	{ NULL, "Tahoma" },		// mark end of list
};

//-----------------------------------------------------------------------------
// Purpose: specialized fonts
//-----------------------------------------------------------------------------
const char *CFontManager::GetForeignFallbackFontName()
{
	if (s_bSupportsUnicode)
	{
		// tahoma has all the necessary characters for asian/russian languages for winXP/2K+
		return "Tahoma";
	}

	int i;
	for (i = 0; g_Win98ForeignFallbackFonts[i].language != NULL; i++)
	{
		if (!_stricmp(g_Win98ForeignFallbackFonts[i].language, m_szLanguage))
			return g_Win98ForeignFallbackFonts[i].fallbackFont;
	}

	return g_Win98ForeignFallbackFonts[i].fallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the first font of the set is underlined
//-----------------------------------------------------------------------------
bool CFontManager::GetFontUnderlined(HFont font)
{
	return m_FontAmalgams[font].GetUnderlined();
}
