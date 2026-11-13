//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Career dialog boxes (queries, difficulty selection, new profile)
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERBOX_H
#define CAREERBOX_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/Dar.h>
#include <vgui_controls/Frame.h>

namespace vgui2
{
class Button;
class CBitmapImagePanel;
class Label;
};

class CCareerButton;
class CCvarToggleCheckButton;
class CCvarTextEntry;

//-----------------------------------------------------------------------------
// Purpose: Base class for the career dialog boxes
//-----------------------------------------------------------------------------
class CCareerBaseBox : public vgui2::Frame
{
public:
	CCareerBaseBox( vgui2::Panel *parent, const char *panelName, bool loadResources = true );

	virtual void ShowWindow();
	virtual void DoModal();

	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void PerformLayout();

	void SetLabelText( const wchar_t *text );
	void SetCancelButtonAsDefault();

protected:
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );
	virtual void OnCommand( const char *command );
	void AddButton( vgui2::Button *pButton );

private:
	typedef vgui2::Frame BaseClass;

	vgui2::Button *m_pOkButton;
	vgui2::Button *m_pCancelButton;
	vgui2::Dar< vgui2::Button * > m_buttons;

	vgui2::CBitmapImagePanel *m_pTopLeftPanel;
	vgui2::CBitmapImagePanel *m_pTopRightPanel;
	vgui2::CBitmapImagePanel *m_pBottomLeftPanel;
	vgui2::CBitmapImagePanel *m_pBottomRightPanel;

	Color m_bgColor;
	vgui2::Label *m_pTextLabel;
	bool m_cancelFocus;
};

//-----------------------------------------------------------------------------
// Purpose: A yes/no question box
//-----------------------------------------------------------------------------
class CCareerQueryBox : public CCareerBaseBox
{
public:
	CCareerQueryBox( const char *title, const char *labelText, const char *panelName, vgui2::Panel *parent = NULL );
	CCareerQueryBox( const wchar_t *title, const wchar_t *labelText, const char *panelName, vgui2::Panel *parent = NULL );
	virtual ~CCareerQueryBox();
};

//-----------------------------------------------------------------------------
// Purpose: Selects the difficulty of a new career
//-----------------------------------------------------------------------------
class CCareerDifficultyBox : public CCareerBaseBox
{
public:
	CCareerDifficultyBox( const char *panelName, vgui2::Panel *parent = NULL );
	virtual ~CCareerDifficultyBox();

	virtual void ShowWindow();
	virtual void OnCommand( const char *command );

private:
	typedef CCareerBaseBox BaseClass;

	CCareerButton *m_pFocusButton;
	CCvarToggleCheckButton *m_pEnableTutorCheck;
};

//-----------------------------------------------------------------------------
// Purpose: Asks for the name of a new profile
//-----------------------------------------------------------------------------
class CCareerNewProfileBox : public CCareerBaseBox
{
public:
	CCareerNewProfileBox( const char *panelName, vgui2::Panel *parent = NULL );

	virtual void ShowWindow();
	virtual ~CCareerNewProfileBox();

protected:
	virtual void OnCommand( const char *command );
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );

private:
	typedef CCareerBaseBox BaseClass;

	CCvarTextEntry *m_pEntry;
};

#endif // CAREERBOX_H
