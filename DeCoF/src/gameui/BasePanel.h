//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The panel that sits under all the other GameUI panels and draws the background
//
// $NoKeywords: $
//=============================================================================

#ifndef BASEPANEL_H
#define BASEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <UtlVector.h>

//-----------------------------------------------------------------------------
// Purpose: The panel at the top of the vgui heirarchy in GameUI
//			Handles drawing the background
//-----------------------------------------------------------------------------
class CBasePanel : public vgui2::Panel
{
public:
	CBasePanel();

	// notifications
	virtual void OnChildAdded(vgui2::VPANEL child);

	// background rendering
	virtual void PaintBackground();
	virtual vgui2::VPANEL IsWithinTraverse(int x, int y, bool traversePopups);
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	enum EBackgroundState
	{
		BACKGROUND_NONE,
		BACKGROUND_BLACK,
		BACKGROUND_DESKTOPIMAGE,
		BACKGROUND_LOADING,
		BACKGROUND_LOADINGTRANSITION,
		BACKGROUND_CAREERLOAD,
	};
	void SetBackgroundRenderState(EBackgroundState state);

private:
	void DrawBackgroundImage();

	EBackgroundState m_eBackgroundState;

	struct bimage_t
	{
		int imageID;
		int x, y;
		int width, height;
		bool scaled;
	};
	CUtlVector<bimage_t> m_ImageID;
	int m_iBaseResX;
	int m_iBaseResY;

	// Cry of Fear addition, never used by BasePanel.cpp (sizeof is 0xa8)
	int m_iUnknown90_guess[6];
};

#endif // BASEPANEL_H
