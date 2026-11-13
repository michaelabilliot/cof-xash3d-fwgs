//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "steamloginfailure.h"

namespace vgui2
{
class Button;
class Label;
class ProgressBar;
};

class CareerRichText;
class CBitmapImagePanel;

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying level loading status
//-----------------------------------------------------------------------------
class CLoadingDialog : public vgui2::Frame
{
	typedef vgui2::Frame BaseClass;

public:
	CLoadingDialog( vgui2::Panel *parent );
	~CLoadingDialog();

	void DisplayProgressBar(const char *resourceType, const char *resourceName);
	void SetProgressPoint(int progressPoint);
	void SetProgressRange(int min, int max);
	void SetStatusText(const char *statusText);
	void SetSecondaryProgress(float progress);
	void SetSecondaryProgressText(const char *statusText);
	bool IsCareerLoadingDialog();
	void SwitchToPausedCareerDialog();
	void SwitchToHttpDownloadDialog(const char *url);

	void DisplayGenericError(const char *failureReason, const char *extendedReason = NULL);
	void DisplayVACBannedError();
	void DisplayNoSteamConnectionError();
	void DisplaySteamConnectionError(const char *username, ESteamLoginFailure steamLoginFailure);

protected:
	virtual void OnCommand(const char *command);
	virtual void PerformLayout();
	virtual void OnThink();
	virtual void OnClose();
	virtual void OnKeyCodePressed(vgui2::KeyCode code);
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);
	void SetupControlSettingsForErrorDisplay(const char *settingsFile);

private:
	void CompleteSwitchToPausedCareerDialog();

	class LoadingHTML;

	// Cry of Fear addition, not initialized by the constructor (sizeof is 0x1b0)
	int m_iUnknown10C_guess[6];

	vgui2::ProgressBar *m_pProgress;
	vgui2::ProgressBar *m_pProgress2;
	vgui2::Label *m_pInfoLabel;
	vgui2::Label *m_pTimeRemainingLabel;
	vgui2::Button *m_pCancelButton;
	CareerRichText *m_pCareerMapDescLabel;
	vgui2::Label *m_pCareerMapLabel;
	vgui2::Label *m_pCareerScenarioInfo;
	CBitmapImagePanel *m_pCareerMapImagePanel;
	float m_switchTime;
	LoadingHTML *m_pHTML_guess;	// Cry of Fear addition ("BannerAd"), created by the constructor

	int m_iRangeMin, m_iRangeMax;
	bool m_bShowingSecondaryProgress;
	float m_flProgress;
	float m_flSecondaryProgress;
	float m_flLastSecondaryProgressUpdateTime;
	float m_flSecondaryProgressStartTime;
	bool m_isCareerLoadingDialog;
	bool m_isHttpDownloadDialog;
	bool m_bShowingVACInfo;
	bool m_bShowingVACError;
	char m_szUserName[64];
};

// returns the loading dialog, if it is up
extern CLoadingDialog *LoadingDialog();

#endif // LOADINGDIALOG_H
