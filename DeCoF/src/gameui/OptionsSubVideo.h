//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef OPTIONS_SUB_VIDEO_H
#define OPTIONS_SUB_VIDEO_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

namespace vgui = vgui2;

class CCvarSlider;

namespace vgui2
{
class ComboBox;
};

struct CVidSettings
{
	int			w, h;
	int			bpp;
	int			windowed;
	char		renderer[ 128 ];
};

//-----------------------------------------------------------------------------
// Purpose: Video Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVideo : public vgui::PropertyPage
{
	DECLARE_CLASS_SIMPLE( COptionsSubVideo, vgui::PropertyPage );

public:
	COptionsSubVideo( vgui::Panel *parent );
	~COptionsSubVideo();

	virtual void OnResetData();
	virtual void OnApplyChanges();

private:
	CVidSettings		m_OrigSettings;
	CVidSettings		m_CurrentSettings;

	void		GetVidSettings();
	void		RevertVidSettings();
	void		ApplyVidSettings( bool bForceRefresh );

	void		SetCurrentRendererComboItem();
	void		SetCurrentResolutionComboItem();

	MESSAGE_FUNC( OnDataChanged, "ControlModified" );
	MESSAGE_FUNC_PARAMS( OnButtonChecked, "CheckButtonChecked", data );
	MESSAGE_FUNC_PTR_CHARPTR( OnTextChanged, "TextChanged", panel, text );

	void		PrepareResolutionList();

	vgui::ComboBox *m_pMode;
	vgui::ComboBox *m_pRenderer;
	vgui::ComboBox *m_pColorDepth_guess;
	vgui::ComboBox *m_pWindowed;
	vgui::ComboBox *m_pAspectRatio;
	CCvarSlider		*m_pBrightnessSlider;
	CCvarSlider		*m_pGammaSlider;

	char		m_pszRenderNames[ 3 ][ 32 ];
	char		m_pszAspectName[ 2 ][ 32 ];

	int			m_iStartRenderer;
	int			m_iStartResolution;
	bool		m_bStartWidescreen;
};

#endif // OPTIONS_SUB_VIDEO_H
