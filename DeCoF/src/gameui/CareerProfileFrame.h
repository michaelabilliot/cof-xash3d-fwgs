//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career profile selection screen
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERPROFILEFRAME_H
#define CAREERPROFILEFRAME_H

#ifdef _WIN32
#pragma once
#endif

#include "CareerBaseFrame.h"

namespace vgui2
{
class Button;
class CBitmapImagePanel;
class ComboBox;
class Label;
class URLLabel;
};

class CProfileButton;

//-----------------------------------------------------------------------------
// Purpose: Lists the career profiles and the mission packs
//-----------------------------------------------------------------------------
class CCareerProfileFrame : public CCareerBaseFrame
{
public:
	CCareerProfileFrame( vgui2::Panel *parent );
	virtual ~CCareerProfileFrame();

	virtual void Activate();
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );
	virtual void PerformLayout();

	void SelectMissionPack( const char *missionPackName );

	virtual void OnThink();
	void QueueFocusUpdate( int frames ) { m_needFocus = frames; }

private:
	typedef CCareerBaseFrame BaseClass;

	int PopulateProfiles();
	void ChooseDifficulty();
	void UpdateSelectedProfile( const char *profile );
	void RestoreFocus();
	void PopulateMissionPacks();

	vgui2::Dar< CProfileButton * > m_profiles;
	CCareerPanelListPanel *m_pProfileList;
	vgui2::Label *m_pProfileSelectionBackground;
	vgui2::Button *m_pResumeButton;
	vgui2::Button *m_pDeleteButton;
	vgui2::Button *m_pNewButton;
	char m_name[32];
	vgui2::Label *m_pMissionPackBackground;
	vgui2::Label *m_pMissionPackAuthor;
	vgui2::Label *m_pMissionPackDescription;
	vgui2::URLLabel *m_pMissionPackURL;
	vgui2::CBitmapImagePanel *m_pMissionPackLogo;
	vgui2::ComboBox *m_pMissionPackComboBox;
	int m_needFocus;
};

#endif // CAREERPROFILEFRAME_H
