//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career map selection screen
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERMAPFRAME_H
#define CAREERMAPFRAME_H

#ifdef _WIN32
#pragma once
#endif

#include "CareerBaseFrame.h"
#include "ICareerUI.h"

namespace vgui2
{
class Button;
class CBitmapImagePanel;
class Label;
class RichText;
class ScrollBar;
};

class CCareerMap;
class CMapButton;

//-----------------------------------------------------------------------------
// Purpose: Shows the map triplets of the career and the selected map
//-----------------------------------------------------------------------------
class CCareerMapFrame : public CCareerBaseFrame
{
public:
	CCareerMapFrame( vgui2::Panel *parent );
	virtual ~CCareerMapFrame();

	virtual void Activate();
	void ShowMapDescription( CCareerMap *pMap );

	virtual void OnCommand( const char *command );
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );

	vgui2::Button *GetNextButton();

protected:
	virtual void OnThink();
	virtual void OnSliderMoved( int position );

	DECLARE_PANELMAP();

private:
	typedef CCareerBaseFrame BaseClass;

	void ChangeActiveMap( int right, int down );

	CMapButton *m_pMapButton[3][3];
	vgui2::Button *m_pNextButton;
	vgui2::Button *m_pPlayButton;
	vgui2::Button *m_pCancelButton;
	vgui2::RichText *m_pMapDesc;
	vgui2::Label *m_pMapName;
	vgui2::Label *m_pTripletBorder[3];
	vgui2::Label *m_pTripletStatus[3];
	vgui2::ScrollBar *m_pMapScroll;
	vgui2::Label *m_pMapSelectionBackground;
	vgui2::Label *m_pMapDescriptionBackground;
	vgui2::CBitmapImagePanel *m_pMapImage;
	vgui2::CBitmapImagePanel *m_pMedalImage[MAX_CAREER_DIFFICULTY];
	int m_lastTripletActive;
	int m_numTriplets;
	int m_activeMapOffset;
	bool m_doPostActivate;
	Color m_mapDescriptionColor;
	Color m_friendlyFireColor;
};

#endif // CAREERMAPFRAME_H
