//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the game menu bar along the bottom of the GameUI desktop
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Taskbar.h"
#include "BackgroundMenuButton.h"
#include "TaskButton.h"
#include "EngineInterface.h"
#include "IBaseUI.h"
#include "GameUI_Interface.h"
#include "LoadingDialog.h"
#include "ModInfo.h"
#include "MusicManager.h"
#include "CareerGame.h"
#include "VGuiSystemModuleLoader.h"

#include "NewGameDialog.h"
#include "LoadGameDialog.h"
#include "SaveGameDialog.h"
#include "OptionsDialog.h"
#include "DemoPlayerDialog.h"
#include "CreateMultiplayerGameDialog.h"
#include "ChangeGameDialog.h"
#include "PlayerListDialog.h"
#include "CareerProfileFrame.h"
#include "CareerMapFrame.h"
#include "CareerBotFrame.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Menu.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/QueryBox.h>

#undef PostMessage	// windows.h, from cdll_int.h

using namespace vgui;

extern IBaseUI *baseuifuncs;
extern vgui2::DHANDLE< CLoadingDialog > g_hLoadingDialog;

//-----------------------------------------------------------------------------
// Purpose: a query box with the menu button sounds
//-----------------------------------------------------------------------------
class CareerQueryBox : public vgui::QueryBox
{
public:
	CareerQueryBox( const char *title, const char *queryText, vgui::Panel *parent );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
};

//-----------------------------------------------------------------------------
// Purpose: a transparent menu item of the game menu
//-----------------------------------------------------------------------------
class CGameMenuItem : public vgui::MenuItem
{
	typedef vgui::MenuItem BaseClass;

public:
	CGameMenuItem( vgui::Menu *parent, const char *name ) : BaseClass( parent, name, "GameMenuItem" )
	{
		m_bRightAligned = false;
		SetPaintEnabled( false );
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		// make fully transparent
		SetFgColor( GetSchemeColor( "InGameDesktop/MenuColor", pScheme ) );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetDefaultColor( GetSchemeColor( "InGameDesktop/MenuColor", pScheme ), Color( 0, 0, 0, 0 ) );
		SetArmedColor( GetSchemeColor( "InGameDesktop/ArmedMenuColor", pScheme ), Color( 0, 0, 0, 0 ) );
		SetDepressedColor( GetSchemeColor( "InGameDesktop/DepressedMenuColor", pScheme ), Color( 0, 0, 0, 0 ) );
		SetContentAlignment( Label::a_west );
		SetBorder( NULL );
		SetDefaultBorder( NULL );
		SetDepressedBorder( NULL );
		SetKeyFocusBorder( NULL );
		SetFont( pScheme->GetFont( "MenuLarge", IsProportional() ) );
		SetTextInset( 0, 0 );
		SetArmedSound( "sound/UI/buttonrollover.wav" );
		SetDepressedSound( "sound/UI/buttonclick.wav" );
		SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
		SetButtonActivationType( Button::ACTIVATE_ONPRESSED );

		if ( m_bRightAligned )
		{
			SetContentAlignment( Label::a_east );
		}
	}

	void SetRightAlignedText_guess( bool state )
	{
		m_bRightAligned = state;
	}

private:
	bool m_bRightAligned;
};

//-----------------------------------------------------------------------------
// Purpose: the game menu, its items fade in one after the other when it opens
//-----------------------------------------------------------------------------
class CGameMenu : public vgui::Menu
{
	typedef vgui::Menu BaseClass;

public:
	CGameMenu( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
	{
		m_bRestartListUpdate_guess = false;
		m_bListUpdating_guess = false;
		m_flListUpdateStartTime_guess = 0;
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		// make fully transparent
		SetMenuItemHeight( atoi( pScheme->GetResourceString( "InGameDesktop/MenuItemHeight" ) ) );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetBorder( NULL );

		m_flMenuItemVisibilityRate = atof( pScheme->GetResourceString( "InGameDesktop/MenuItemVisibilityRate" ) );
		if ( m_flMenuItemVisibilityRate < 0.0000001 )
		{
			m_flMenuItemVisibilityRate = 0.0000001f;
		}
	}

	virtual void LayoutMenuBorder()
	{
	}

	virtual void SetVisible( bool state )
	{
		// force to be always visible
		BaseClass::SetVisible( true );

		// move us to the back instead of going invisible
		if ( !state )
		{
			ipanel()->MoveToBack( GetVPanel() );
		}
	}

	virtual int AddMenuItem( const char *itemName, const char *itemText, const char *command, vgui::Panel *target, KeyValues *userData = NULL )
	{
		MenuItem *item = new CGameMenuItem( this, itemName );
		item->AddActionSignalTarget( target );
		item->SetCommand( command );
		item->SetText( itemText );
		item->SetUserData( userData );
		return BaseClass::AddMenuItem( item );
	}

	virtual int AddMenuItem( const char *itemName, const char *itemText, KeyValues *command, vgui::Panel *target, KeyValues *userData = NULL )
	{
		CGameMenuItem *item = new CGameMenuItem( this, itemName );
		item->AddActionSignalTarget( target );
		item->SetCommand( command );
		item->SetText( itemText );
		item->SetRightAlignedText_guess( true );
		item->SetUserData( userData );
		return BaseClass::AddMenuItem( item );
	}

	virtual void OnThink()
	{
		BaseClass::OnThink();

		if ( TheCareerGame )
		{
			TheCareerGame->Update();
		}

		if ( m_bRestartListUpdate_guess )
		{
			m_bListUpdating_guess = true;
			m_flListUpdateStartTime_guess = system()->GetFrameTime();
			m_bRestartListUpdate_guess = false;
		}

		if ( !m_bListUpdating_guess )
			return;

		// show one more item every m_flMenuItemVisibilityRate seconds, from the bottom up
		int visibleCount = ( system()->GetFrameTime() - m_flListUpdateStartTime_guess ) / m_flMenuItemVisibilityRate;
		if ( visibleCount >= GetChildCount() )
		{
			visibleCount = GetChildCount();
			m_bListUpdating_guess = false;
		}

		for ( int i = 0; i < GetChildCount(); i++ )
		{
			vgui::Panel *child = GetChild( i );
			if ( child )
			{
				if ( i >= GetChildCount() - visibleCount )
				{
					child->SetPaintEnabled( true );
				}
				else
				{
					child->SetPaintEnabled( false );
				}
			}
		}

		InvalidateLayout();
		Repaint();
	}

	virtual void OnCommand( const char *command )
	{
		if ( !stricmp( command, "Open" ) )
		{
			MoveToFront();
			RequestFocus();
			RestartListUpdate_guess();
		}
		else
		{
			BaseClass::OnCommand( command );
		}
	}

	void RestartListUpdate_guess()
	{
		m_bRestartListUpdate_guess = true;
	}

	virtual void OnKillFocus()
	{
		BaseClass::OnKillFocus();

		// force us to the rear when we lose focus (so it looks like the menu is always on the background)
		surface()->MovePopupToBack( GetVPanel() );
	}

	void UpdateMenuItemState_guess( bool isInGame, bool isMulti, bool isCareer )
	{
		bool isSteam = engine->CheckParm( "-steam", NULL ) != 0;

		// disable menu items that are only valid in some game states
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			vgui::Panel *child = GetChild( i );
			MenuItem *menuItem = dynamic_cast< MenuItem * >( child );
			if ( menuItem )
			{
				bool shouldBeVisible = true;

				// filter the visibility
				KeyValues *kv = menuItem->GetUserData();
				if ( !kv )
					continue;

				if ( isInGame && !isCareer && kv->GetInt( "OnlyInCareerGame" ) )
				{
					shouldBeVisible = false;
				}

				if ( isInGame && isCareer && kv->GetInt( "NotInCareerGame" ) )
				{
					shouldBeVisible = false;
				}

				if ( !isInGame && kv->GetInt( "OnlyInGame" ) )
				{
					shouldBeVisible = false;
				}
				else if ( isMulti && kv->GetInt( "notmulti" ) )
				{
					shouldBeVisible = false;
				}
				else if ( isInGame && !isMulti && kv->GetInt( "notsingle" ) )
				{
					shouldBeVisible = false;
				}
				else if ( isSteam && kv->GetInt( "notsteam" ) )
				{
					shouldBeVisible = false;
				}

				menuItem->SetVisible( shouldBeVisible );
			}
		}

		InvalidateLayout();
	}

private:
	bool m_bRestartListUpdate_guess;
	bool m_bListUpdating_guess;
	float m_flListUpdateStartTime_guess;
	float m_flMenuItemVisibilityRate;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CareerQueryBox::CareerQueryBox( const char *title, const char *queryText, vgui::Panel *parent ) : QueryBox( title, queryText, parent )
{
}

//-----------------------------------------------------------------------------
// Purpose: gives the buttons their sounds
//-----------------------------------------------------------------------------
void CareerQueryBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pOkButton->SetArmedSound( "sound/UI/buttonrollover.wav" );
	m_pOkButton->SetDepressedSound( "sound/UI/buttonclick.wav" );
	m_pOkButton->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );

	m_pCancelButton->SetArmedSound( "sound/UI/buttonrollover.wav" );
	m_pCancelButton->SetDepressedSound( "sound/UI/buttonclick.wav" );
	m_pCancelButton->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTaskbar::CTaskbar( vgui::Panel *parent, const char *panelName ) : EditablePanel( parent, panelName )
{
	m_iGameMenuInset = 24;
	m_bPlatformMenuInitialized = false;
	m_bShowPlatformMenu = false;

	if ( engine->CheckParm( "-steam", NULL ) || engine->CheckParm( "-showplatform", NULL ) )
	{
		m_bShowPlatformMenu = true;
	}

	m_pGameMenuButton = new CBackgroundMenuButton( this, "GameMenuButton" );
	m_pGameMenuButton->SetCommand( "OpenGameMenu" );
	m_pPlatformMenuButton = new CBackgroundMenuButton( this, "PlatformMenuButton" );
	m_pPlatformMenuButton->SetCommand( "OpenPlatformMenu" );

	if ( !m_bShowPlatformMenu )
	{
		m_pPlatformMenuButton->SetVisible( false );
	}

	m_pPlatformMenu = NULL;
	m_pGameMenu = NULL;

	CreateMusicManager();
	CreateGameMenu();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTaskbar::~CTaskbar()
{
	ShutdownMusicManager();
}

//-----------------------------------------------------------------------------
// Purpose: builds the menu of the platform modules
//-----------------------------------------------------------------------------
void CTaskbar::CreatePlatformMenu()
{
	m_pPlatformMenu = new CGameMenu( m_pPlatformMenuButton, "PlatformMenu" );

	for ( int i = 0; i < g_VModuleLoader.GetModuleCount(); i++ )
	{
		m_pPlatformMenu->AddMenuItem( g_VModuleLoader.GetModuleLabel( i ), g_VModuleLoader.GetModuleLabel( i ), new KeyValues( "ActivateModule", "moduleIndex", i ), this );
	}

	m_pPlatformMenuButton->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: loads the game menu from Resource/GameMenu.res
//-----------------------------------------------------------------------------
void CTaskbar::CreateGameMenu()
{
	// load settings from config file
	KeyValues *datafile = new KeyValues( "GameMenu" );
	if ( datafile->LoadFromFile( filesystem(), "Resource/GameMenu.res" ) )
	{
		m_pGameMenu = RecursiveLoadGameMenu( datafile );
		datafile->deleteThis();
	}
}

//-----------------------------------------------------------------------------
// Purpose: positions the menus and updates the menu music and item states
//-----------------------------------------------------------------------------
void CTaskbar::UpdateGameMenus()
{
	// check our current state
	int wide, tall;
	surface()->GetScreenSize( wide, tall );

	if ( m_pPlatformMenu )
	{
		m_pPlatformMenu->SetPos( wide - m_iGameMenuInset - m_pPlatformMenu->GetWide(), tall - m_pPlatformMenu->GetTall() - 64 );
		m_pPlatformMenu->SetVisible( true );
	}

	static bool s_bIsCareerGame_guess = false;

	bool isInGame = false;
	bool isMulti = false;

	if ( s_bIsCareerGame_guess )
	{
		if ( !TheCareerGame->IsPlayingMatch() )
		{
			TheCareerGame->StopMatch( 0, 0 );
			s_bIsCareerGame_guess = false;
		}
	}

	const char *serverMap_guess = engine->pfnGetLevelName();
	if ( serverMap_guess && serverMap_guess[ 0 ] )
	{
		isInGame = true;

		if ( engine->GetMaxClients() > 1 )
		{
			isMulti = true;

			if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
			{
				s_bIsCareerGame_guess = true;
			}
		}
	}

	if ( TheMusicManager )
	{
		if ( !g_hLoadingDialog.Get() && !isInGame && !TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
		{
			TheMusicManager->StartTrack( TRACKTYPE_SPLASH, true );
		}
		else if ( g_hLoadingDialog.Get() && !isInGame && s_bIsCareerGame_guess && !TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
		{
			TheMusicManager->StartTrack( TRACKTYPE_SPLASH, true );
		}
		else if ( g_hLoadingDialog.Get() && !s_bIsCareerGame_guess && TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
		{
			TheMusicManager->FadeOutTrack();
		}
		else if ( isInGame && TheMusicManager->IsPlayingTrack( TRACKTYPE_SPLASH ) )
		{
			TheMusicManager->FadeOutTrack();
		}
	}

	m_pGameMenu->UpdateMenuItemState_guess( isInGame, isMulti, s_bIsCareerGame_guess );
	m_pGameMenu->SetPos( m_iGameMenuInset, tall - m_pGameMenu->GetTall() - 64 );
	m_pGameMenu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: removes the buttons of deleted tasks, sorts and lays out the rest
//-----------------------------------------------------------------------------
void CTaskbar::UpdateTaskButtons()
{
	// iterate through all the tasks, removing invalid ones and finding the active task
	VPANEL focus = input()->GetFocus();
	int validTasks = 0;
	int sortedCount = 0;

	for ( int i = 0; i < g_Tasks.Count(); i++ )
	{
		// check for the active window
		if ( focus && ipanel()->HasParent( focus, g_Tasks[ i ]->GetTaskPanel() ) )
		{
			g_Tasks[ i ]->SetSelected( true );
		}
		else
		{
			g_Tasks[ i ]->SetSelected( false );
		}

		if ( !g_Tasks[ i ]->GetTaskPanel() )
		{
			// the panel has been deleted
			g_Tasks[ i ]->SetVisible( false );
			g_Tasks[ i ]->MarkForDeletion();
			g_Tasks.Remove( i );

			// restart the search
			i = -1;
			validTasks = 0;
			continue;
		}

		if ( g_Tasks[ i ]->ShouldDisplay() )
		{
			validTasks++;

			// move hidden buttons to the end of the list
			if ( !g_Tasks[ i ]->IsVisible() && sortedCount < 100 && i != g_Tasks.Count() - 1 )
			{
				CTaskButton *temp = g_Tasks[ i ];
				g_Tasks[ i ] = g_Tasks[ i + 1 ];
				g_Tasks[ i + 1 ] = temp;

				// restart the search
				i = -1;
				validTasks = 0;
				sortedCount++;
				continue;
			}
		}
	}

	// calculate button width
	int buttonWide = ( GetWide() - 256 ) / ( validTasks > 0 ? validTasks : 1 );
	int buttonTall = 24;
	if ( buttonWide > 128 )
	{
		buttonWide = 128;
	}

	int x = 280, y = 20;
	for ( i = 0; i < g_Tasks.Count(); i++ )
	{
		if ( !g_Tasks[ i ]->ShouldDisplay() )
		{
			g_Tasks[ i ]->SetVisible( false );
			continue;
		}

		g_Tasks[ i ]->SetVisible( true );
		g_Tasks[ i ]->SetBounds( x, y, buttonWide - 8, buttonTall );
		x += buttonWide;
	}
}

//-----------------------------------------------------------------------------
// Purpose: builds a menu from a key values file
//-----------------------------------------------------------------------------
CGameMenu *CTaskbar::RecursiveLoadGameMenu( KeyValues *datafile )
{
	CGameMenu *menu = new CGameMenu( this, datafile->GetName() );

	// loop through all the data adding items to the menu
	for ( KeyValues *dat = datafile->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey() )
	{
		const char *label = dat->GetString( "label", "<unknown>" );
		const char *cmd = dat->GetString( "command", NULL );
		const char *name = dat->GetString( "name", label );

		menu->AddMenuItem( name, label, cmd, this, dat );
	}

	return menu;
}

//-----------------------------------------------------------------------------
// Purpose: update the taskbar a frame
//-----------------------------------------------------------------------------
void CTaskbar::RunFrame()
{
	InvalidateLayout();

	if ( !m_bPlatformMenuInitialized )
	{
		// check to see if the platform is ready to load yet
		if ( g_VModuleLoader.IsPlatformReady() )
		{
			if ( m_bShowPlatformMenu )
			{
				CreatePlatformMenu();
				PostMessage( m_pPlatformMenu, new KeyValues( "Command", "command", "Open" ) );
			}

			m_bPlatformMenuInitialized = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the title of a task
//-----------------------------------------------------------------------------
void CTaskbar::SetTitle( VPANEL panel, const wchar_t *title )
{
	CTaskButton *task = FindTask( panel );
	if ( task )
	{
		task->SetTitle( title );
	}
}

//-----------------------------------------------------------------------------
// Purpose: lays out the bar along the bottom of the screen
//-----------------------------------------------------------------------------
void CTaskbar::PerformLayout()
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );

	// check the CD key a few frames after startup
	static int s_iValidateCDKeyDelay_guess = 2;
	if ( s_iValidateCDKeyDelay_guess-- == 0 )
	{
		GameUI().ValidateCDKey( false, false );
	}

	SetPos( 0, tall - 64 );
	SetSize( wide, 64 );

	m_pGameMenuButton->SetPos( m_iGameMenuInset, ( 64 - m_iGameMenuInset ) / 2 );
	m_pGameMenuButton->SetSize( 240, m_iGameMenuInset + 4 );

	m_pPlatformMenuButton->SetPos( wide - ( m_pPlatformMenuButton->GetWide() + m_iGameMenuInset ), 6 );
	m_pPlatformMenuButton->SetSize( 240, m_iGameMenuInset + 4 );
	m_pPlatformMenuButton->SetContentAlignment( Label::a_east );

	UpdateTaskButtons();
	UpdateGameMenus();
}

//-----------------------------------------------------------------------------
// Purpose: loads the menu button images and colors
//-----------------------------------------------------------------------------
void CTaskbar::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder( NULL );

	m_iGameMenuInset = atoi( pScheme->GetResourceString( "InGameDesktop/GameMenuInset" ) );

	m_pGameMenuButton->SetImages( scheme()->GetImage( "resource/game_menu", false ), scheme()->GetImage( "resource/game_menu_mouseover", false ) );
	m_pPlatformMenuButton->SetImages( scheme()->GetImage( "resource/steam_menu", false ), scheme()->GetImage( "resource/steam_menu_mouseover", false ) );

	SetBgColor( GetSchemeColor( "InGameDesktop/WidescreenBarColor", pScheme ) );

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: adds a new task to the list (tasks are not shown in Cry of Fear)
//-----------------------------------------------------------------------------
void CTaskbar::AddTask( VPANEL panel )
{
	// Cry of Fear shows no task buttons. The frame keeps the slots of the
	// unreachable code below: two locals and one temporary.
	return;

	CTaskButton *task_guess = FindTask( panel );
	int count_guess = g_Tasks.Count();

	// ignore if not a popup
	if ( !ipanel()->IsPopup( panel ) )
		return;
}

//-----------------------------------------------------------------------------
// Purpose: finds the button of a task
//-----------------------------------------------------------------------------
CTaskButton *CTaskbar::FindTask( VPANEL panel )
{
	for ( int i = 0; i < g_Tasks.Count(); i++ )
	{
		if ( g_Tasks[ i ]->GetTaskPanel() == panel )
			return g_Tasks[ i ];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: activates a platform module
//-----------------------------------------------------------------------------
void CTaskbar::OnActivateModule( int moduleIndex )
{
	g_VModuleLoader.ActivateModule( moduleIndex );
}

//-----------------------------------------------------------------------------
// Purpose: notification that the GameUI has been activated
//-----------------------------------------------------------------------------
void CTaskbar::OnGameUIActivated()
{
	if ( m_pGameMenu )
	{
		m_pGameMenu->RestartListUpdate_guess();
	}

	if ( m_pPlatformMenu )
	{
		m_pPlatformMenu->RestartListUpdate_guess();
	}
}

//-----------------------------------------------------------------------------
// Purpose: handles the menu commands
//-----------------------------------------------------------------------------
void CTaskbar::OnCommand( const char *command )
{
	if ( !stricmp( command, "OpenGameMenu" ) )
	{
		if ( m_pGameMenu )
		{
			PostMessage( m_pGameMenu, new KeyValues( "Command", "command", "Open" ) );
		}
	}
	else if ( !stricmp( command, "OpenPlatformMenu" ) )
	{
		if ( m_pPlatformMenu && m_bShowPlatformMenu )
		{
			PostMessage( m_pPlatformMenu, new KeyValues( "Command", "command", "Open" ) );
		}
	}
	else if ( !stricmp( command, "OpenPlayerListDialog" ) )
	{
		OnOpenPlayerListDialog();
	}
	else if ( !stricmp( command, "OpenNewGameDialog" ) )
	{
		OnOpenNewGameDialog();
	}
	else if ( !stricmp( command, "OpenLoadGameDialog" ) )
	{
		OnOpenLoadGameDialog();
	}
	else if ( !stricmp( command, "OpenSaveGameDialog" ) )
	{
		OnOpenSaveGameDialog();
	}
	else if ( !stricmp( command, "OpenOptionsDialog" ) )
	{
		OnOpenOptionsDialog();
	}
	else if ( !stricmp( command, "OpenServerBrowser" ) )
	{
		OnOpenServerBrowser();
	}
	else if ( !stricmp( command, "OpenLoadDemoDialog" ) )
	{
		OnOpenDemoDialog();
	}
	else if ( !stricmp( command, "OpenCreateMultiplayerGameDialog" ) )
	{
		OnOpenCreateMultiplayerGameDialog();
	}
	else if ( !stricmp( command, "OpenNewCareerNameDialog" ) )
	{
		if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
		{
			OnOpenNewCareerNameDialog();
		}
	}
	else if ( !stricmp( command, "OpenPlayOnLineDialog" ) )
	{
		QueryBox *box = new QueryBox( "#CZero_PlayOnLine", "#CZero_PlayOnLineText" );
		box->AddActionSignalTarget( this );
		box->SetOKCommand( new KeyValues( "Command", "command", "QuitToPlayOnLine" ) );
		box->DoModal();
	}
	else if ( !strnicmp( command, "OpenCareerMapDialog", strlen( "OpenCareerMapDialog" ) ) )
	{
		if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
		{
			OnOpenCareerMapDialog( !stricmp( command, "OpenCareerMapDialog 1" ) );
		}
	}
	else if ( !stricmp( command, "OpenCareerBotDialog" ) )
	{
		if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
		{
			OnOpenCareerBotDialog();
		}
	}
	else if ( !stricmp( command, "OpenCreateCareerGameDialog" ) )
	{
		if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
		{
			OnOpenCreateCareerGameDialog();
		}
	}
	else if ( !stricmp( command, "OpenChangeGameDialog" ) )
	{
		OnOpenChangeGameDialog();
	}
	else if ( !stricmp( command, "Quit" ) )
	{
		OnOpenQuitConfirmationDialog();
	}
	else if ( !stricmp( command, "QuitNoConfirm" ) )
	{
		if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
		{
			TheCareerGame->StopMatch( 0, 0 );
		}

		// hide everything while we quit
		engine->pfnClientCmd( "quit\n" );
		SetVisible( false );
		surface()->RestrictPaintToSinglePanel( GetVPanel() );
	}
	else if ( !stricmp( command, "QuitToPlayOnLine" ) )
	{
		if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
		{
			TheCareerGame->StopMatch( 0, 0 );
		}

		// hide everything while we quit
		engine->pfnClientCmd( "quit\n" );
		SetVisible( false );
		surface()->RestrictPaintToSinglePanel( GetVPanel() );

		system()->ShellExecute( "open", "steaminstall.exe" );
	}
	else if ( !stricmp( command, "ResumeGame" ) )
	{
		if ( baseuifuncs )
		{
			baseuifuncs->HideGameUI();
		}
	}
	else if ( !stricmp( command, "Surrender" ) )
	{
		QueryBox *box = new QueryBox( "#Career_Surrender", "#GameUI_QuitConfirmationText", this );
		box->SetProportional( false );
		box->SetOKCommand( new KeyValues( "Command", "command", "ReallySurrender" ) );
		box->AddActionSignalTarget( this );
		box->SetScheme( "TrackerScheme" );
		box->DoModal();
	}
	else if ( !stricmp( command, "ReallySurrender" ) )
	{
		engine->pfnClientCmd( "disconnect\n" );
		TheCareerGame->OnRoundEndMenuClose( false );
	}
	else if ( !stricmp( command, "EndRound" ) )
	{
		QueryBox *box = new QueryBox( "#Career_EndRound", "#Career_EndRoundText", this );
		box->SetProportional( false );
		box->SetOKCommand( new KeyValues( "Command", "command", "engine career_endround\n" ) );
		box->AddActionSignalTarget( this );
		box->SetScheme( "TrackerScheme" );
		box->DoModal();
	}
	else if ( !stricmp( command, "Disconnect" ) )
	{
		engine->pfnClientCmd( "disconnect\n" );
	}
	else if ( !stricmp( command, "ReleaseModalWindow" ) )
	{
		surface()->RestrictPaintToSinglePanel( NULL );
	}
	else if ( !stricmp( command, "RestartCareer" ) )
	{
		TheCareerGame->StopMatch( 0, 0 );
		surface()->RestrictPaintToSinglePanel( NULL );
		engine->pfnClientCmd( "disconnect\n" );
		OnOpenNewCareerNameDialog();
	}
	else if ( strstr( command, "engine " ) )
	{
		const char *engineCMD = strstr( command, "engine " ) + strlen( "engine " );
		if ( strlen( engineCMD ) > 0 )
		{
			gEngfuncs.pfnClientCmd( (char *)engineCMD );
		}

		if ( baseuifuncs )
		{
			baseuifuncs->HideGameUI();
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: asks before quitting
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenQuitConfirmationDialog()
{
	QueryBox *box = new QueryBox( "#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText", this );
	box->SetOKButtonText( "#GameUI_Quit" );
	box->SetOKCommand( new KeyValues( "Command", "command", "QuitNoConfirm" ) );
	box->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
	box->AddActionSignalTarget( this );
	box->DoModal();

	// paint nothing but the query box
	surface()->RestrictPaintToSinglePanel( box->GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenNewGameDialog()
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog( this );
		PositionDialog( m_hNewGameDialog );
	}

	m_hNewGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenLoadGameDialog()
{
	if ( !m_hLoadGameDialog.Get() )
	{
		m_hLoadGameDialog = new CLoadGameDialog( this );
		PositionDialog( m_hLoadGameDialog );
	}

	m_hLoadGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenSaveGameDialog()
{
	if ( !m_hSaveGameDialog.Get() )
	{
		m_hSaveGameDialog = new CSaveGameDialog( this );
		PositionDialog( m_hSaveGameDialog );
	}

	m_hSaveGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenOptionsDialog()
{
	if ( !m_hOptionsDialog.Get() )
	{
		m_hOptionsDialog = new COptionsDialog( this );
		PositionDialog( m_hOptionsDialog );
	}

	m_hOptionsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenServerBrowser()
{
	g_VModuleLoader.ActivateModule( "Servers" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenDemoDialog()
{
	if ( !m_hDemoPlayerDialog.Get() )
	{
		m_hDemoPlayerDialog = new CDemoPlayerDialog( this );
		PositionDialog( m_hDemoPlayerDialog );
	}

	m_hDemoPlayerDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCreateMultiplayerGameDialog()
{
	if ( !m_hCreateMultiplayerGameDialog.Get() )
	{
		m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog( this );
		PositionDialog( m_hCreateMultiplayerGameDialog );
	}

	m_hCreateMultiplayerGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: loads the career profile of a difficulty and opens the map frame
//-----------------------------------------------------------------------------
vgui::Frame *CTaskbar::OnOpenResumeCareerGameDialog( CareerDifficultyType difficulty )
{
	CCareerGame::Create();
	TheCareerGame->Reset();

	if ( TheCareerGame->LoadProfile( difficulty ) )
	{
		if ( !m_hCareerMapDialog.Get() )
		{
			m_hCareerMapDialog = new CCareerMapFrame( this );
			PositionDialog( m_hCareerMapDialog );
		}

		m_hCareerMapDialog->Activate();
	}

	return m_hCareerMapDialog;
}

//-----------------------------------------------------------------------------
// Purpose: starts a new career, asks first when a career match is running
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenNewCareerNameDialog()
{
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		CareerQueryBox *box = new CareerQueryBox( "#Career_RestartConfirmationTitle", "#Career_RestartConfirmationText", this );
		box->SetOKButtonText( "#Career_Restart" );
		box->SetOKCommand( new KeyValues( "Command", "command", "RestartCareer" ) );
		box->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
		box->AddActionSignalTarget( this );
		box->DoModal();

		// paint nothing but the query box
		surface()->RestrictPaintToSinglePanel( box->GetVPanel() );
		return;
	}

	engine->pfnClientCmd( "disconnect\n" );
	OnOpenCareerProfileFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCareerProfileFrame()
{
	if ( m_hCareerDifficultyDialog )
	{
		m_hCareerDifficultyDialog->Close();
	}

	if ( !m_hCareerProfileDialog.Get() )
	{
		m_hCareerProfileDialog = new CCareerProfileFrame( this );
		PositionDialog( m_hCareerProfileDialog );
	}

	m_hCareerProfileDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCareerMapDialog( bool promptOnOverwrite )
{
	if ( !m_hCareerMapDialog.Get() )
	{
		m_hCareerMapDialog = new CCareerMapFrame( this );
		PositionDialog( m_hCareerMapDialog );
	}

	m_hCareerMapDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCareerBotDialog()
{
	if ( !m_hCareerBotDialog.Get() )
	{
		m_hCareerBotDialog = new CCareerBotFrame( this );
		PositionDialog( m_hCareerBotDialog );
	}

	m_hCareerBotDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: starts a new career game, asks first when a career match is running
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenCreateCareerGameDialog()
{
	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		CareerQueryBox *box = new CareerQueryBox( "#Career_RestartConfirmationTitle", "#Career_RestartConfirmationText", this );
		box->SetOKButtonText( "#Career_Restart" );
		box->SetOKCommand( new KeyValues( "Command", "command", "RestartCareer" ) );
		box->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
		box->AddActionSignalTarget( this );
		box->DoModal();

		// paint nothing but the query box
		surface()->RestrictPaintToSinglePanel( box->GetVPanel() );
		return;
	}

	CCareerGame::Create();
	TheCareerGame->Reset();
	OnOpenCareerProfileFrame();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenChangeGameDialog()
{
	if ( !m_hChangeGameDialog.Get() )
	{
		m_hChangeGameDialog = new CChangeGameDialog( this );
		PositionDialog( m_hChangeGameDialog );
	}

	m_hChangeGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::OnOpenPlayerListDialog()
{
	if ( !m_hPlayerListDialog.Get() )
	{
		m_hPlayerListDialog = new CPlayerListDialog( this );
		PositionDialog( m_hPlayerListDialog );
	}

	m_hPlayerListDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: centers a dialog in the workspace
//-----------------------------------------------------------------------------
void CTaskbar::PositionDialog( PHandle dlg )
{
	if ( !dlg.Get() )
		return;

	int x, y, ww, wt, wide, tall;
	surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize( wide, tall );

	// Center it, keeping requested size
	dlg->SetPos( x + ( ( ww - wide ) / 2 ), y + ( ( wt - tall ) / 2 ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskbar::ActivateGameUI()
{
	baseuifuncs->ActivateGameUI();
}
