//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career screen where the player hires his team
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERBOTFRAME_H
#define CAREERBOTFRAME_H

#ifdef _WIN32
#pragma once
#endif

#include "CareerBaseFrame.h"
#include "UtlVector.h"

namespace vgui2
{
class Button;
class Label;
class PanelHorizListPanel;
};

class CBotButton;
class CBitmapButton;
class CCareerCharacter;

//-----------------------------------------------------------------------------
// Purpose: The career screen where the player hires his team
//-----------------------------------------------------------------------------
class CCareerBotFrame : public CCareerBaseFrame
{
public:
	CCareerBotFrame( vgui2::Panel *parent );
	virtual ~CCareerBotFrame();

	virtual void Activate();
	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	void UpdatePointsRemaining();
	virtual void OnKeyCodeTyped( vgui2::KeyCode code );

private:
	virtual void OnCommand( const char *command );
	int Hire( const char *name );
	void Fire( const char *name );
	void SortHires();
	void AutoRoster();
	CBotButton *GetHiredButton( const char *name );
	CBotButton *GetPoolButton( const char *name );
	virtual void OnThink();
	CUtlVector< const CCareerCharacter * > *GetPoolCharacters( int points );

	typedef CCareerBaseFrame BaseClass;

	enum { MAX_HIRED = 5 };

	CBotButton *m_pHiredButton[MAX_HIRED];
	vgui2::Label *m_pPointsLabel;
	vgui2::PanelHorizListPanel *m_pListPanel;
	vgui2::Label *m_pPoolBackground;
	vgui2::Label *m_pHiredBackground;
	vgui2::Button *m_pAutoRosterButton;
	CBitmapButton *m_pPoolPgUpButton;
	CBitmapButton *m_pPoolPgDownButton;
	vgui2::Button *m_pStartButton;
	vgui2::Button *m_pPrevButton;
	vgui2::Label *m_pPointsLeftLabel;
	CUtlVector< CBitmapButton * > m_pJumpButtons;
	bool m_initialLayoutDone;
};

#endif // CAREERBOTFRAME_H
