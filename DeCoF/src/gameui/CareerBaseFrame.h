//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Base classes for the career screens
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERBASEFRAME_H
#define CAREERBASEFRAME_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/Dar.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>
#include "PanelListPanel.h"

namespace vgui2
{
class IBorder;
class IImage;
class TextImage;
};

//-----------------------------------------------------------------------------
// Purpose: A panel list with the career scrollbar border
//-----------------------------------------------------------------------------
class CCareerPanelListPanel : public CPanelListPanel
{
public:
	CCareerPanelListPanel( vgui2::Panel *parent, const char *panelName, bool inverseButtons = false ) : CPanelListPanel( parent, panelName, inverseButtons )
	{
	}

	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
};

//-----------------------------------------------------------------------------
// Purpose: Draws a gradient between two colors
//-----------------------------------------------------------------------------
class ColorFade
{
public:
	enum FadeDir
	{
		up,
		down,
		left,
		right
	};

	ColorFade();
	void SetFade( Color startColor, Color endColor, float fadePercent, FadeDir direction );
	void DrawFade( int x, int y, int w, int h );

private:
	Color m_startColor;
	Color m_endColor;
	float m_fadePercent;
	FadeDir m_direction;
};

//-----------------------------------------------------------------------------
// Purpose: A button with an image and text
//-----------------------------------------------------------------------------
class CCareerButton : public vgui2::Button
{
public:
	CCareerButton( vgui2::Panel *parent, const char *buttonName, const char *buttonText, const char *image, bool textFirst );

	virtual void SetArmedBorder( vgui2::IBorder *border );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void Paint();

protected:
	typedef vgui2::Button BaseClass;

	virtual vgui2::IBorder *GetBorder( bool depressed, bool armed, bool selected, bool keyfocus );

	vgui2::IBorder *m_armedBorder;
	vgui2::IImage *m_image;
	vgui2::TextImage *m_textImage;
	bool m_textFirst;
	int m_textPad;
	int m_imagePad;
	Color m_textNormalColor;
	Color m_textDisabledColor;
};

//-----------------------------------------------------------------------------
// Purpose: Base class for the career screens
//-----------------------------------------------------------------------------
class CCareerBaseFrame : public vgui2::Frame
{
public:
	CCareerBaseFrame( vgui2::Panel *parent, const char *panelName, bool showTaskbarIcon = true );
	virtual ~CCareerBaseFrame();

	virtual void Activate();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void PerformLayout();
	virtual void ShowAutosaveNotice();
	virtual void OnThink();

protected:
	void ShowPlayerName( bool show ) { m_pPlayerName->SetVisible( show ); }
	void ShowMissionPackName( bool show ) { m_pMissionPackName->SetVisible( show ); }
	void SetImageBackground( bool show );

	vgui2::Dar< vgui2::Button * > m_buttons;
	ColorFade m_bgFade;

private:
	typedef vgui2::Frame BaseClass;

	void DrawBackgroundImage();

	struct bimage_t
	{
		int imageID;
		int width;
		int height;
	};

	vgui2::Label *m_pTitleBackground;
	vgui2::Label *m_pPlayerName;
	vgui2::Label *m_pMissionPackName;
	vgui2::Label *m_pAutosaveNotice;
	float m_autosaveHideTime;
	bool m_showImageBackground;
	bimage_t m_ImageID[3][4];
};

#endif // CAREERBASEFRAME_H
