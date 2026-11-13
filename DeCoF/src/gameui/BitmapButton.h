//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button drawn with a texture per state, which can flash
//
// $NoKeywords: $
//=============================================================================

#ifndef BITMAPBUTTON_H
#define BITMAPBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Button.h>

//-----------------------------------------------------------------------------
// Purpose: A button drawn with a texture per state, which can flash
//-----------------------------------------------------------------------------
class CBitmapButton : public vgui2::Button
{
public:
	CBitmapButton( vgui2::Panel *parent, const char *panelName, const char *text, const char *normalImage, const char *armedImage, const char *clickedImage, const char *disabledImage );

	virtual void PaintBackground();
	virtual void DrawFocusBorder( int tx0, int ty0, int tx1, int ty1 ) {}
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void OnThink();

	void Flash();

private:
	typedef vgui2::Button BaseClass;

	struct bimage_t
	{
		int imageID;
		int width;
		int height;
	};

	bimage_t m_ImageID[4];
	float m_interval;
	float m_lastTransition;
	int m_transitionsLeft;
	bool m_shouldFlash;
	bool m_isFlashColor;
	int m_totalTransitions;
};

#endif // BITMAPBUTTON_H
