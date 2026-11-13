//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <direct.h>

#include "GameUI_Interface.h"
#include "EngineInterface.h"
#include "IBaseUI.h"
#include "ienginevgui.h"
#include "FileSystem.h"
#include "BasePanel.h"
#include "Taskbar.h"
#include "LoadingDialog.h"
#include "ModInfo.h"
#include "VGuiSystemModuleLoader.h"
#include "CareerGame.h"
#include "Sys_Utils.h"
#include "Socket_guess.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/TestScriptController.h>

//-----------------------------------------------------------------------------
// Purpose: Cry of Fear debugging helper
//-----------------------------------------------------------------------------
void DebugMessage_guess( const char *msg )
{
	MessageBox( NULL, msg, "Lol", MB_OK );
}

// interfaces from the engine
IGameUIFuncs *gameuifuncs = NULL;
IEngineVGui *enginevguifuncs = NULL;
vgui2::ISurface *enginesurfacefuncs = NULL;
IBaseUI *baseuifuncs = NULL;
class IFriendsUser *g_pFriendsUser = NULL;

cl_enginefunc_t gEngfuncs;
cl_enginefunc_t *engine = NULL;

IBaseSystem *g_pSystemWrapper = NULL;

// the taskbar
CTaskbar *g_pTaskbar = NULL;

static CBasePanel *staticPanel = NULL;

vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;

CGameUI *g_pGameUI = NULL;

// the platform mutexes
static WHANDLE g_hMutex_guess = NULL;
static WHANDLE g_hWaitMutex_guess = NULL;

static CGameUI g_GameUI;

static IGameClientExports *g_pGameClientExports = NULL;
IEngineVGui *g_pEngineVGUI = NULL;

IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

CGameUI &GameUI()
{
	return g_GameUI;
}

IEngineVGui &EngineVGUI()
{
	return *g_pEngineVGUI;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUI, GAMEUI_INTERFACE_VERSION, g_GameUI);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadTracker = false;
	m_iGameIP = 0;
	m_iGamePort = 0;
	m_flProgressStartTime = 0;
	m_pszCurrentProgressType = "";
	m_bActivatedUI = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn *factories, int count )
{
	CreateInterfaceFn fileSystemFactory = factories[2];
	CreateInterfaceFn vguiFactory = factories[1];
	CreateInterfaceFn engineFactory = factories[0];
	CreateInterfaceFn clientFactory = factories[4];

	m_FactoryList[0] = Sys_GetFactoryThis();
	m_FactoryList[1] = engineFactory;
	m_FactoryList[2] = vguiFactory;
	m_FactoryList[3] = fileSystemFactory;
	m_FactoryList[4] = clientFactory;
	m_iNumFactories = count;

	// load and initialize vgui
	vgui2::VGuiControls_Init("GameUI", m_FactoryList, count);

	// load localization file
	vgui2::localize()->AddFile(vgui2::filesystem(), "resource/gameui_%language%.txt");

	// setup base panel
	staticPanel = new CBasePanel();
	staticPanel->SetBounds(0, 0, 400, 300);
	staticPanel->SetPaintBorderEnabled(false);
	staticPanel->SetPaintBackgroundEnabled(true);
	staticPanel->SetPaintEnabled(false);
	staticPanel->SetVisible(true);
	staticPanel->SetMouseInputEnabled(false);
	staticPanel->SetKeyBoardInputEnabled(false);

	g_pEngineVGUI = (IEngineVGui *)engineFactory(VENGINE_VGUI_VERSION, NULL);
	if (g_pEngineVGUI)
	{
		vgui2::VPANEL rootpanel = g_pEngineVGUI->GetPanel(PANEL_GAMEUIDLL);
		staticPanel->SetParent(rootpanel);
	}

	// engine interfaces
	gameuifuncs = (IGameUIFuncs *)engineFactory("VENGINE_GAMEUIFUNCS_VERSION001", NULL);
	enginesurfacefuncs = (vgui2::ISurface *)engineFactory("VGUI_Surface026", NULL);
	baseuifuncs = (IBaseUI *)engineFactory(BASEUI_INTERFACE_VERSION, NULL);
	if (clientFactory)
	{
		g_pGameClientExports = (IGameClientExports *)clientFactory("GameClientExports001", NULL);
	}

	m_flRetryPlatformReleaseMessageTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Callback function; sends platform Shutdown message to specified window
//-----------------------------------------------------------------------------
int __stdcall SendShutdownMsgFunc_guess(WHANDLE hwnd, int lparam)
{
	Sys_PostMessage(hwnd, Sys_RegisterWindowMessage("ShutdownValvePlatform"), 0, 1);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Console command to run a vgui test script
//-----------------------------------------------------------------------------
void RunVguiTestScript()
{
	if (gEngfuncs.Cmd_Argc() < 2)
		return;

	g_GameUI.RunVguiTestScript(gEngfuncs.Cmd_Argv(1));
}

//-----------------------------------------------------------------------------
// Purpose: runs a vgui test script
//-----------------------------------------------------------------------------
void CGameUI::RunVguiTestScript(const char *script)
{
	if (!m_hTestScriptController.Get())
	{
		m_hTestScriptController = new vgui2::TestScriptController();
	}

	m_hTestScriptController->ExecuteScript(script);
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start(struct cl_enginefuncs_s *engineFuncs, int interfaceVersion, IBaseSystem *system)
{
	m_pMaster = NULL;

	// copy the engine interface
	memcpy(&gEngfuncs, engineFuncs, sizeof(gEngfuncs));
	engine = &gEngfuncs;
	// set the system wrapper
	g_pSystemWrapper = system;

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	gEngfuncs.pfnAddCommand("vgui_runscript", ::RunVguiTestScript);

	// determine Tracker location
	if (FindPlatformDirectory(m_szPlatformDir, sizeof(m_szPlatformDir)))
	{
		// add the tracker directory to the search path
		// add localization for the platform
		char szConfigDir[512];
		strcpy(szConfigDir, m_szPlatformDir);
		strcat(szConfigDir, "config");

		vgui2::filesystem()->AddSearchPath(szConfigDir, "CONFIG");
		// make sure the config directory has been created
		_mkdir(szConfigDir);

		vgui2::ivgui()->DPrintf("Platform config directory: %s\n", szConfigDir);

		// user dialog configuration
		vgui2::system()->SetUserConfigFile("InGameDialogConfig.vdf", "CONFIG");

		vgui2::localize()->AddFile(vgui2::filesystem(), "resource/platform_%language%.txt");
		vgui2::localize()->AddFile(vgui2::filesystem(), "Resource/vgui_%language%.txt");
	}

	// setup taskbar
	g_pTaskbar = new CTaskbar(staticPanel, "TaskBar");
	g_pTaskbar->SetVisible(false);

	// Start loading tracker
	if (m_szPlatformDir[0] != 0)
	{
		vgui2::ivgui()->DPrintf2("Initializing platform...\n");

		// open a mutex
		Sys_SetLastError(SYS_NO_ERROR);

		// primary mutex is the platform.exe name
		char szExeName_guess[sizeof(m_szPlatformDir) + 32];
		sprintf(szExeName_guess, "%splatform.exe", m_szPlatformDir);

		// convert the backslashes in the path string to be forward slashes so it can be a mutex name
		for (char *psz_guess = szExeName_guess; *psz_guess != 0; psz_guess++)
		{
			*psz_guess = tolower(*psz_guess);
			if (*psz_guess == '\\')
			{
				*psz_guess = '/';
			}
		}

		g_hMutex_guess = Sys_CreateMutex("ValvePlatformUIMutex");
		g_hWaitMutex_guess = Sys_CreateMutex("ValvePlatformWaitMutex");
		if (g_hMutex_guess == NULL || g_hWaitMutex_guess == NULL || Sys_GetLastError() == SYS_ERROR_INVALID_HANDLE)
		{
			// error, can't get handle to mutex
			if (g_hMutex_guess)
			{
				Sys_ReleaseMutex(g_hMutex_guess);
			}
			if (g_hWaitMutex_guess)
			{
				Sys_ReleaseMutex(g_hWaitMutex_guess);
			}
			g_hMutex_guess = NULL;
			g_hWaitMutex_guess = NULL;
			vgui2::ivgui()->DPrintf2("Tracker Error: Could not access Tracker, bad mutex\n");
			return;
		}

		unsigned int waitResult_guess = Sys_WaitForSingleObject(g_hMutex_guess, 0);
		if (!(waitResult_guess == SYS_WAIT_OBJECT_0 || waitResult_guess == SYS_WAIT_ABANDONED))
		{
			// mutex locked, need to close other tracker

			// get the wait mutex, so that tracker.exe knows that we're trying to acquire ValveTrackerMutex
			waitResult_guess = Sys_WaitForSingleObject(g_hWaitMutex_guess, 0);
			if (waitResult_guess == SYS_WAIT_OBJECT_0 || waitResult_guess == SYS_WAIT_ABANDONED)
			{
				Sys_EnumWindows(SendShutdownMsgFunc_guess, 1);
			}

			// assume we can't get it, and try again in a little while
			m_flRetryPlatformReleaseMessageTime = vgui2::system()->GetCurrentTime() + 2.0;
		}

		m_bTryingToLoadTracker = true;
	}

	staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_DESKTOPIMAGE);

	// start the startup music
	engine->pfnClientCmd("mp3 loop media/gamestartup.mp3\n");
}

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	strncpy(platformDir, "platform", bufferSize);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey(bool force, bool inConnect)
{
}

//-----------------------------------------------------------------------------
// Purpose: Cry of Fear addition, opens one of the GameUI dialogs
//-----------------------------------------------------------------------------
void CGameUI::OpenDialog_guess(int dialog)
{
	switch (dialog)
	{
	case 0:
		g_VModuleLoader.ActivateModule("Servers");
		break;
	case 1:
		g_pTaskbar->OnOpenCreateMultiplayerGameDialog();
		break;
	case 2:
		g_pTaskbar->OnOpenOptionsDialog();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{
	// notify all the modules of Shutdown
	g_VModuleLoader.ShutdownPlatformModules();

	// unload the modules them from memory
	g_VModuleLoader.UnloadPlatformModules();

	ModInfo().FreeModInfo();

	// release platform mutex
	// close the mutex
	if (g_hMutex_guess)
	{
		Sys_ReleaseMutex(g_hMutex_guess);
	}
	if (g_hWaitMutex_guess)
	{
		Sys_ReleaseMutex(g_hWaitMutex_guess);
	}
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
bool CGameUI::IsGameUIActive()
{
	if (m_bActivatedUI)
		return staticPanel->IsVisible();
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
int CGameUI::ActivateGameUI()
{
	if (IsGameUIActive())
		return 1;

	m_bActivatedUI = true;

	// pause the server in case it is pausable
	staticPanel->SetVisible(true);
	g_pTaskbar->SetVisible(true);
	engine->pfnClientCmd("setpause");

	if (g_pTaskbar)
	{
		g_pTaskbar->OnGameUIActivated();
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Opens the demo player
//-----------------------------------------------------------------------------
int CGameUI::ActivateDemoUI()
{
	if (g_pTaskbar)
	{
		g_pTaskbar->OnOpenDemoDialog();
		return 1;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	const char *levelName = engine->pfnGetLevelName();
	if (levelName && levelName[0])
	{
		staticPanel->SetVisible(false);
		g_pTaskbar->SetVisible(false);

		// unpause the game when leaving the UI
		if (!TheCareerGame || !TheCareerGame->IsPlayingMatch() || TheCareerGame->CanUnpause())
		{
			engine->pfnClientCmd("unpause");
		}

		engine->pfnClientCmd("hideconsole");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns 1 if the game UI is currently visible
//-----------------------------------------------------------------------------
int CGameUI::HasExclusiveInput()
{
	return IsGameUIActive();
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{
	// resize the background panel to the screen size
	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	staticPanel->SetSize(wide, tall);

	// Run frames
	g_VModuleLoader.RunFrame();

	if (g_pTaskbar)
	{
		g_pTaskbar->RunFrame();
	}

	if (m_bTryingToLoadTracker && g_hMutex_guess && g_hWaitMutex_guess)
	{
		// see if we can get the mutex
		unsigned int waitResult_guess = Sys_WaitForSingleObject(g_hMutex_guess, 0);
		if (waitResult_guess == SYS_WAIT_OBJECT_0 || waitResult_guess == SYS_WAIT_ABANDONED)
		{
			// we got the mutex, so load the platform
			m_bTryingToLoadTracker = false;
			m_flRetryPlatformReleaseMessageTime = 0;

			g_VModuleLoader.LoadPlatformModules(m_FactoryList, m_iNumFactories, false);

			// get our own interfaces from the modules
			for (int i = 0; i < g_VModuleLoader.GetModuleCount(); i++)
			{
				if (!g_pFriendsUser && g_VModuleLoader.GetModuleFactory(i))
				{
					g_pFriendsUser = (IFriendsUser *)g_VModuleLoader.GetModuleFactory(i)("FriendsUser001", NULL);
				}
			}

			// release the wait mutex
			Sys_ReleaseMutex(g_hWaitMutex_guess);

			// notify the game of our game name
			KeyValues *pKV = new KeyValues("ActiveGameName");
			pKV->SetString("name", engine->pfnGetGameDirectory());
			pKV->SetInt("type", 1);
			pKV->SetInt("appid", engine->pfnGetAppID());
			pKV->SetString("game", ModInfo().GetGameDescription());
			g_VModuleLoader.PostMessageToAllModules(pKV);

			// notify the ui of a game connect if we're already in a game
			if (m_iGameIP)
			{
				KeyValues *pKV = new KeyValues("ConnectedToGame");
				pKV->SetInt("ip", m_iGameIP);
				pKV->SetInt("connectionport", m_iGamePort);
				pKV->SetInt("queryport", m_iGamePort);
				g_VModuleLoader.PostMessageToAllModules(pKV);
			}
		}
		else if (m_flRetryPlatformReleaseMessageTime < vgui2::system()->GetFrameTime())
		{
			// haven't got the mutex yet, keep sending the shutdown message to the old platform
			Sys_EnumWindows(SendShutdownMsgFunc_guess, 1);

			m_flRetryPlatformReleaseMessageTime = vgui2::system()->GetFrameTime() + 5.0;
		}
	}

	if (m_pMaster)
	{
		m_pMaster->Frame_guess();
	}

	// Hack: if a modal panel is up, paint the whole UI
	if (vgui2::surface()->GetModalPanel())
	{
		vgui2::surface()->PaintTraverse(staticPanel->GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the career game load has finished
//-----------------------------------------------------------------------------
void CGameUI::FinishCareerLoad()
{
	engine->pfnClientCmd("mp3 stop\n");
	engine->pfnClientCmd("_careeraudio\n");
	baseuifuncs->HideGameUI();
	staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_DESKTOPIMAGE);
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::ConnectToServer(const char *game, int IP, int port)
{
	if (TheCareerGame && TheCareerGame->IsPlayingMatch())
	{
		baseuifuncs->ActivateGameUI();
		StartProgressBar("CareerPause", 1);
		staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_CAREERLOAD);
		ContinueProgressBar(1, 100.0f);
		SetProgressBarStatusText("#Career_Start");
		g_hLoadingDialog->SwitchToPausedCareerDialog();
		TheCareerGame->FinishMatchStart();
	}
	else
	{
		// stop the music
		engine->pfnClientCmd("mp3 stop\n");
		baseuifuncs->HideGameUI();
	}

	m_iGameIP = IP;
	m_iGamePort = port;

	KeyValues *pKV = new KeyValues("ConnectedToGame");
	pKV->SetInt("ip", m_iGameIP);
	pKV->SetInt("connectionport", m_iGamePort);
	pKV->SetInt("queryport", m_iGamePort);
	g_VModuleLoader.PostMessageToAllModules(pKV);
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::DisconnectFromServer()
{
	ActivateGameUI();

	m_iGameIP = 0;
	m_iGamePort = 0;

	g_VModuleLoader.PostMessageToAllModules(new KeyValues("DisconnectedFromGame"));
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server, displays why
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer( int eSteamLoginFailure, const char *username )
{
	DisconnectFromServer();

	if ( eSteamLoginFailure == STEAMLOGINFAILURE_NOSTEAMLOGIN )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayNoSteamConnectionError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_VACBANNED )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayVACBannedError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE ||
			  eSteamLoginFailure == STEAMLOGINFAILURE_CONNECTIONLOST ||
			  eSteamLoginFailure == STEAMLOGINFAILURE_NOCONNECTION ||
			  eSteamLoginFailure == STEAMLOGINFAILURE_BADTICKET )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplaySteamConnectionError( username, (ESteamLoginFailure)eSteamLoginFailure );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::LoadingStarted(const char *resourceType, const char *resourceName)
{
	g_VModuleLoader.PostMessageToAllModules(new KeyValues("LoadingStarted", "type", resourceType, "name", resourceName));

	if (!stricmp(resourceType, "transition"))
	{
		staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_LOADINGTRANSITION);
	}
	else
	{
		staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_LOADING);
	}
}

//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::LoadingFinished(const char *resourceType, const char *resourceName)
{
	g_VModuleLoader.PostMessageToAllModules(new KeyValues("LoadingFinished", "type", resourceType, "name", resourceName));

	staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_DESKTOPIMAGE);
	baseuifuncs->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: Starts a progress bar
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar(const char *progressType, int progressSteps)
{
	if (!g_hLoadingDialog.Get())
	{
		g_hLoadingDialog = new CLoadingDialog(staticPanel);
	}

	staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_LOADING);

	m_pszCurrentProgressType = progressType;
	if (m_flProgressStartTime < 0.001f)
	{
		m_flProgressStartTime = vgui2::system()->GetCurrentTime();
	}

	// open a loading dialog
	g_hLoadingDialog->SetProgressRange(0, progressSteps);
	g_hLoadingDialog->SetProgressPoint(0);
	g_hLoadingDialog->DisplayProgressBar(progressType, "invalid");

	// make sure the dialog is the http download dialog if needed
	if (progressType)
	{
		if (!strnicmp(progressType, "http://", 7) || !strnicmp(progressType, "https://", 8))
		{
			g_hLoadingDialog->SwitchToHttpDownloadDialog(progressType);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
int CGameUI::ContinueProgressBar(int progressPoint, float progressFraction)
{
	if (!g_hLoadingDialog.Get())
		return 0;

	g_hLoadingDialog->SetProgressPoint(progressPoint);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
	if (!g_hLoadingDialog.Get() && bError)
	{
		g_hLoadingDialog = new CLoadingDialog(staticPanel);
	}

	if (!g_hLoadingDialog.Get())
		return;

	if (bError)
	{
		// turn the dialog to error display mode
		g_hLoadingDialog->DisplayGenericError(failureReason, extendedReason);
	}
	else
	{
		// close loading dialog
		g_hLoadingDialog->Close();
		g_hLoadingDialog = NULL;
	}

	// should update the background to be in a transition here
	staticPanel->SetBackgroundRenderState(CBasePanel::BACKGROUND_DESKTOPIMAGE);
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
int CGameUI::SetProgressBarStatusText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return 0;

	g_hLoadingDialog->SetStatusText(statusText);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : progress -
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
	if (!g_hLoadingDialog.Get())
		return;

	g_hLoadingDialog->SetSecondaryProgress(progress);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *statusText -
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return;

	g_hLoadingDialog->SetSecondaryProgressText(statusText);
}
