//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONSSUBMULTIPLAYER_H
#define OPTIONSSUBMULTIPLAYER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PHandle.h>

namespace vgui = vgui2;

class CLabeledCommandComboBox;
class CBitmapImagePanel;
class CCvarToggleCheckButton;
class CCvarTextEntry;
class CCvarSlider;
class CrosshairImagePanel;
class CMultiplayerAdvancedDialog;

namespace vgui2
{
class ComboBox;
};

//-----------------------------------------------------------------------------
// Purpose: multiplayer options property page
//-----------------------------------------------------------------------------
class COptionsSubMultiplayer : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubMultiplayer, vgui::PropertyPage );

public:
	COptionsSubMultiplayer( vgui::Panel *parent );
	~COptionsSubMultiplayer();

	virtual void OnCommand( const char *command );

protected:
	// Called when page is loaded.  Data should be reloaded from document into controls.
	virtual void OnResetData();
	// Called when the OK / Apply button is pressed.  Changed data should be written into document.
	virtual void OnApplyChanges();

private:
	void RedrawCrosshairImage();
	void InitModelList( CLabeledCommandComboBox *cb );
	void InitLogoList( CLabeledCommandComboBox *cb );

	void RemapModel();
	void RemapLogo();

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PARAMS( OnSliderMoved, "SliderMoved", data );
	MESSAGE_FUNC( OnApplyButtonEnable, "ControlModified" );

	void RemapPalette( char *filename, int topcolor, int bottomcolor );
	void RemapLogoPalette( char *filename, int r, int g, int b );

	void ColorForName( char const *pszColorName, int &r, int &g, int &b );

	void InitCrosshairSizeList( CLabeledCommandComboBox *cb );
	void InitCrosshairColorEntries();
	void ApplyCrosshairColorChanges();

	CBitmapImagePanel *m_pModelImage;
	CLabeledCommandComboBox *m_pModelList;

	CBitmapImagePanel *m_pLogoImage;
	CLabeledCommandComboBox *m_pLogoList;
	char m_LogoName[ 32 ];

	CLabeledCommandComboBox *m_pCrosshairSize;
	CCvarToggleCheckButton *m_pCrosshairTranslucencyCheckbox;
	vgui::ComboBox *m_pCrosshairColorComboBox;
	int m_displayedCrosshairColorRed;
	int m_displayedCrosshairColorGreen;
	int m_displayedCrosshairColorBlue;

	CLabeledCommandComboBox *m_pColorList;
	CCvarTextEntry *m_pNameTextEntry;
	CCvarSlider *m_pPrimaryColorSlider;
	CCvarSlider *m_pSecondaryColorSlider;
	CCvarToggleCheckButton *m_pHighQualityModelCheckBox;

	CrosshairImagePanel *m_pCrosshairImage;

	int m_nTopColor;
	int m_nBottomColor;

	int m_nLogoR;
	int m_nLogoG;
	int m_nLogoB;

	vgui::DHANDLE< CMultiplayerAdvancedDialog > m_hMultiplayerAdvancedDialog;
};

#endif // OPTIONSSUBMULTIPLAYER_H
