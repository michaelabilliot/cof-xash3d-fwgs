//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button for one career profile, with its medals
//
// $NoKeywords: $
//=============================================================================

#ifndef CAREERPROFILEBUTTON_H
#define CAREERPROFILEBUTTON_H

#ifdef _WIN32
#pragma once
#endif

#include <Color.h>
#include <vgui_controls/Button.h>
#include "ICareerUI.h"

namespace vgui2
{
class IBorder;
class IImage;
class TextImage;
};

class CCareerProfileFrame;

//-----------------------------------------------------------------------------
// Purpose: A profile button in the profile list
//-----------------------------------------------------------------------------
class CProfileButton : public vgui2::Button
{
public:
	CProfileButton( CCareerProfileFrame *parent, const char *panelName, const wchar_t *text, bool *medals );

	virtual void ApplySchemeSettings( vgui2::IScheme *pScheme );
	virtual void Paint();
	virtual void OnMouseDoublePressed( vgui2::MouseCode code );

	void SetProfileName( const char *name );
	void SetCoop( bool coop ) { m_isCoop = coop; }
	bool IsCoop() { return m_isCoop; }

	virtual Color GetBgColor();

	void SetActive( bool active ) { m_active = active; }
	bool IsActive() { return m_active; }
	const char *GetProfileName() { return m_name; }

protected:
	virtual vgui2::IBorder *GetBorder( bool depressed, bool armed, bool selected, bool keyfocus );

private:
	typedef vgui2::Button BaseClass;

	bool m_medals[MAX_CAREER_DIFFICULTY];
	bool m_active;
	bool m_isCoop;
	vgui2::IImage *m_image[MAX_CAREER_DIFFICULTY];
	vgui2::TextImage *m_textImage;
	int m_preTextPad;
	int m_postTextPad;
	int m_interImagePad;
	int m_postImagePad;
	CCareerProfileFrame *m_pParent;
	char m_name[32];
	Color m_bgColor;
	Color m_activeBgColor;
	vgui2::IBorder *m_armedBorder;
	vgui2::IBorder *m_depressedBorder;
};

#endif // CAREERPROFILEBUTTON_H
