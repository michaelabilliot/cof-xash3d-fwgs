//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Defines the interface that the GameUI dll exports
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMEUI_INTERFACE_H
#define GAMEUI_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "IGameUI.h"
#include <vgui_controls/PHandle.h>

namespace vgui2
{
class Frame;
class TestScriptController;
};

struct CSocket;
class CCDKeyEntryDialog;

//-----------------------------------------------------------------------------
// Purpose: Implementation of GameUI's exposed interface
//-----------------------------------------------------------------------------
class CGameUI : public IGameUI
{
public:
	CGameUI();
	~CGameUI();

	virtual void Initialize( CreateInterfaceFn *factories, int count );
	virtual void Start(struct cl_enginefuncs_s *engineFuncs, int interfaceVersion, IBaseSystem *system);
	virtual void Shutdown();

	virtual int ActivateGameUI();	// activates the menus, returns 0 if it doesn't want to handle it
	virtual int ActivateDemoUI();	// activates the demo player, returns 0 if it doesn't want to handle it

	virtual int HasExclusiveInput();

	virtual void RunFrame();

	virtual void ConnectToServer(const char *game, int IP, int port);
	virtual void DisconnectFromServer();
	virtual void HideGameUI();

	virtual bool IsGameUIActive();

	virtual void LoadingStarted(const char *resourceType, const char *resourceName);
	virtual void LoadingFinished(const char *resourceType, const char *resourceName);

	virtual void StartProgressBar(const char *progressType, int progressSteps);
	virtual int ContinueProgressBar(int progressPoint, float progressFraction);
	virtual void StopProgressBar(bool bError, const char *failureReason, const char *extendedReason);
	virtual int SetProgressBarStatusText(const char *statusText);

	virtual void SetSecondaryProgressBar(float progress /* range [0..1] */);
	virtual void SetSecondaryProgressBarText(const char *statusText);

	void FinishCareerLoad();

	virtual void ValidateCDKey(bool force, bool inConnect);

	void RunVguiTestScript(const char *script);

	virtual void OnDisconnectFromServer( int eSteamLoginFailure, const char *username );

	virtual void OpenDialog_guess(int dialog);

private:
	// searches for and finds the path to the platform directory, if any
	bool FindPlatformDirectory(char *platformDir, int bufferSize);

	bool m_bTryingToLoadTracker;
	char m_szPlatformDir[260];

	// our copy of the system handles
	int m_iNumFactories;
	CreateInterfaceFn m_FactoryList[5];

	// information on currently connected game
	int m_iGameIP;
	int m_iGamePort;

	bool m_bActivatedUI;

	float m_flRetryPlatformReleaseMessageTime;

	const char *m_pszCurrentProgressType;
	float m_flProgressStartTime;

	CSocket *m_pMaster;

	// not in the Linux build, never read here: the handle of the CD key dialog
	// that ValidateCDKey opened before Cry of Fear emptied it (CCDKeyEntryDialog
	// is still linked but never constructed); its DHANDLE constructor is a
	// COMDAT of this object, so no earlier object made a handle of this type
	vgui2::DHANDLE<CCDKeyEntryDialog> m_hCDKeyEntryDialog_guess;
	vgui2::DHANDLE<vgui2::TestScriptController> m_hTestScriptController;
};

// accessor to interface
CGameUI &GameUI();
extern class IEngineVGui &EngineVGUI();
class IGameClientExports *GameClientExports();

#endif // GAMEUI_INTERFACE_H
