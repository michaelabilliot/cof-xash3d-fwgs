//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef TASKBAR_H
#define TASKBAR_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/Frame.h>
#include <UtlVector.h>
#include "ICareerUI.h"

namespace vgui = vgui2;

class CTaskButton;
class CBackgroundMenuButton;
class CGameMenu;
class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: Win9x-style taskbar for GameUI
//-----------------------------------------------------------------------------
class CTaskbar : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTaskbar, vgui::EditablePanel );

public:
	CTaskbar(vgui2::Panel *parent, const char *panelName);
	~CTaskbar();

	// update the taskbar a frame
	void RunFrame();

	// sets the title of a task
	void SetTitle(vgui2::VPANEL panel, const wchar_t *title);

	// adds a new task to the list
	void AddTask(vgui2::VPANEL panel);

	// notification that the GameUI has been activated
	void OnGameUIActivated();

	// dialogs
	void OnOpenNewGameDialog();
	void OnOpenLoadGameDialog();
	void OnOpenSaveGameDialog();
	void OnOpenOptionsDialog();
	void OnOpenServerBrowser();
	void OnOpenDemoDialog();
	void OnOpenCreateMultiplayerGameDialog();
	void OnOpenQuitConfirmationDialog();
	void OnOpenChangeGameDialog();
	void OnOpenPlayerListDialog();
	void OnOpenNewCareerNameDialog();
	void OnOpenCareerMapDialog( bool promptOnOverwrite );
	void OnOpenCareerBotDialog();
	void OnOpenCreateCareerGameDialog();
	vgui2::Frame *OnOpenResumeCareerGameDialog( CareerDifficultyType difficulty );
	void PositionDialog(vgui2::PHandle dlg);
	void OnOpenCareerProfileFrame();

private:
	void CreatePlatformMenu();
	void CreateGameMenu();
	void UpdateGameMenus();
	void UpdateTaskButtons();
	CGameMenu *RecursiveLoadGameMenu(KeyValues *datafile);
	CTaskButton *FindTask(vgui2::VPANEL panel);

	virtual void OnCommand(const char *command);
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui2::IScheme *pScheme);

	MESSAGE_FUNC_INT( OnActivateModule, "ActivateModule", moduleIndex );
	MESSAGE_FUNC( ActivateGameUI, "ActivateGameUI" );

	CBackgroundMenuButton *m_pGameMenuButton;
	CBackgroundMenuButton *m_pPlatformMenuButton;
	CGameMenu *m_pPlatformMenu;
	CGameMenu *m_pGameMenu;
	bool m_bShowPlatformMenu;
	bool m_bPlatformMenuInitialized;
	int m_iGameMenuInset;

	vgui2::DHANDLE<vgui2::Frame> m_hNewGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hLoadGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hSaveGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hOptionsDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCreateMultiplayerGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hDemoPlayerDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hChangeGameDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hPlayerListDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerProfileDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerDifficultyDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerMapDialog;
	vgui2::DHANDLE<vgui2::Frame> m_hCareerBotDialog;

	CUtlVector<CTaskButton *> g_Tasks;
};

extern CTaskbar *g_pTaskbar;

#endif // TASKBAR_H
