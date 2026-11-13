//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "vgui_surfacelib/FontAmalgam.h"
#include "UtlVector.h"

#ifdef CreateFont
#undef CreateFont
#endif

using vgui2::HFont;

class CWin32Font;

//-----------------------------------------------------------------------------
// Purpose: Creates and maintains a set of fonts
//-----------------------------------------------------------------------------
class CFontManager
{
public:
	CFontManager();
	~CFontManager();

	void SetLanguage(const char *language);

	// clears the current font list, frees any resources
	void ClearAllFonts();

	HFont CreateFont();
	bool AddGlyphSetToFont(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);
	HFont GetFontByName(const char *name);
	CWin32Font *GetFontForChar(HFont font, wchar_t wch);
	void GetCharABCwide(HFont font, int ch, int &a, int &b, int &c);
	int GetFontTall(HFont font);
	int GetFontAscent(HFont font, wchar_t wch);
	int GetCharacterWidth(HFont font, int ch);
	void GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall);

	bool IsFontForeignLanguageCapable(const char *windowsFontName);
	bool GetFontUnderlined(HFont font);

private:
	CWin32Font *CreateOrFindWin32Font(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);
	const char *GetFallbackFontName(const char *windowsFontName);
	const char *GetForeignFallbackFontName();

	CUtlVector<CFontAmalgam> m_FontAmalgams;
	CUtlVector<CWin32Font *> m_Win32Fonts;

	char m_szLanguage[64];
};

// singleton accessor
extern CFontManager &FontManager();

#endif // FONTMANAGER_H
