//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_MOUSE_H
#define OPTIONS_SUB_MOUSE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

namespace vgui = vgui2;

class CCvarNegateCheckButton;
class CKeyToggleCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;

namespace vgui2
{
class TextEntry;
};

//-----------------------------------------------------------------------------
// Purpose: Mouse Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubMouse : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubMouse, vgui::PropertyPage );

public:
	COptionsSubMouse( vgui::Panel *parent );
	~COptionsSubMouse();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	MESSAGE_FUNC_PTR( OnControlModified, "ControlModified", panel );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	void UpdateSensitivityLabel();

	CCvarNegateCheckButton		*m_pReverseMouseCheckBox;
	CKeyToggleCheckButton		*m_pMouseLookCheckBox;
	CCvarToggleCheckButton		*m_pMouseFilterCheckBox;

	CCvarToggleCheckButton		*m_pJoystickCheckBox;
	CKeyToggleCheckButton		*m_pJoystickLookCheckBox;

	CCvarSlider					*m_pMouseSensitivitySlider;
	vgui::TextEntry				*m_pMouseSensitivityLabel;

	CCvarToggleCheckButton		*m_pAutoAimCheckBox;
};

#endif // OPTIONS_SUB_MOUSE_H
