//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CreateMultiplayerGameDialog.h"
#include "CreateMultiplayerGameServerPage.h"
#include "CreateMultiplayerGameGameplayPage.h"
#include "CreateMultiplayerGameBotPage.h"
#include "EngineInterface.h"
#include "ModInfo.h"
#include "Taskbar.h"
#include "FileSystem.h"

#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/RadioButton.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameDialog::CCreateMultiplayerGameDialog( vgui2::Panel *parent ) : PropertyDialog( parent, "CreateMultiplayerGameDialog" )
{
	SetSize( 348, 460 );

	m_bBotsEnabled = false;
	if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
	{
		m_bBotsEnabled = true;
	}

	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_CreateServer", true );
	SetOKButtonText( "#GameUI_Start" );

	m_pServerPage = new CCreateMultiplayerGameServerPage( this, "ServerPage" );
	m_pGameplayPage = new CCreateMultiplayerGameGameplayPage( this, "GameplayPage" );
	m_pBotPage = NULL;

	AddPage( m_pServerPage, "#GameUI_Server" );
	AddPage( m_pGameplayPage, "#GameUI_Game" );

	// create KeyValues object to load/save config options
	m_pBotSavedData = new KeyValues( "CSBotConfig" );

	// load the config data
	if ( !m_pBotSavedData->LoadFromFile( filesystem(), "CSBotConfig.vdf", "CONFIG" ) )
	{
		// file not found, set the defaults
		m_pBotSavedData->SetString( "bot_difficulty", "0" );
		m_pBotSavedData->SetString( "bot_join_after_player", "1" );
		m_pBotSavedData->SetString( "bot_allow_rogues", "1" );
		m_pBotSavedData->SetString( "bot_allow_pistols", "1" );
		m_pBotSavedData->SetString( "bot_allow_shotguns", "1" );
		m_pBotSavedData->SetString( "bot_allow_sub_machine_guns", "1" );
		m_pBotSavedData->SetString( "bot_allow_machine_guns", "1" );
		m_pBotSavedData->SetString( "bot_allow_rifles", "1" );
		m_pBotSavedData->SetString( "bot_allow_snipers", "1" );
		m_pBotSavedData->SetString( "bot_allow_grenades", "1" );
		m_pBotSavedData->SetString( "bot_allow_shield", "1" );
		m_pBotSavedData->SetString( "bot_join_team", "any" );
		m_pBotSavedData->SetString( "bot_quota", "9" );
		m_pBotSavedData->SetString( "bot_defer_to_human", "0" );
		m_pBotSavedData->SetString( "bot_chatter", "normal" );
	}

	if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
	{
		// add bot page
		m_pBotPage = new CCreateMultiplayerGameBotPage( this, "BotPage", m_pBotSavedData );
		AddPage( m_pBotPage, "#GameUI_CPUPlayerOptions" );
	}

	// set bot difficulty and quota from the cvars when the bots are loaded
	int botDiff = (int)engine->pfnGetCvarFloat( "bot_difficulty" );
	if ( engine->pfnGetCvarPointer( "bot_difficulty" ) )
	{
		botDiff = (int)engine->pfnGetCvarFloat( "bot_difficulty" );
	}
	else
	{
		botDiff = m_pBotSavedData->GetInt( "bot_difficulty" );
	}

	if ( engine->pfnGetCvarPointer( "bot_quota" ) )
	{
		m_pServerPage->SetBotQuota( (int)engine->pfnGetCvarFloat( "bot_quota" ) );
	}
	else
	{
		m_pServerPage->SetBotQuota( atoi( m_pBotSavedData->GetString( "bot_quota", "" ) ) );
	}

	if ( m_bBotsEnabled )
	{
		m_pServerPage->SetBotsEnabled( botDiff >= 0 );
	}
	else
	{
		botDiff = -1;
		m_pServerPage->SetBotsEnabled( false );
	}

	// set the map
	const char *startMap = m_pBotSavedData->GetString( "map", "" );
	if ( startMap[0] )
	{
		m_pServerPage->SetMap( startMap );
	}

	// set the difficulty radio button
	static const char *buttonName[] =
	{
		"SkillLevelEasy",
		"SkillLevelNormal",
		"SkillLevelHard",
		"SkillLevelExpert"
	};

	if ( botDiff < 0 )
	{
		botDiff = 0;
	}
	else if ( botDiff > 3 )
	{
		botDiff = 3;
	}

	RadioButton *button = (RadioButton *)m_pServerPage->FindChildByName( buttonName[ botDiff ] );
	if ( button )
	{
		button->SetSelected( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameDialog::~CCreateMultiplayerGameDialog()
{
	if ( m_pBotSavedData )
	{
		m_pBotSavedData->deleteThis();
		m_pBotSavedData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the title, also in the taskbar
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::SetTitle( const char *title, bool surfaceTitle )
{
	BaseClass::SetTitle( title, surfaceTitle );

	if ( g_pTaskbar )
	{
		wchar_t *wTitle;
		wchar_t w_szTitle[1024];

		wTitle = localize()->Find( title );
		if ( !wTitle )
		{
			localize()->ConvertANSIToUnicode( title, w_szTitle, sizeof( w_szTitle ) );
			wTitle = w_szTitle;
		}

		g_pTaskbar->SetTitle( GetVPanel(), wTitle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: runs the server when the OK button is pressed
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnOK()
{
	// apply values to the server
	BaseClass::OnOK();

	// get these values from m_pServerPage and store them temporarily
	char szMapName[64], szHostName[64], szPassword[64];
	strncpy( szMapName, m_pServerPage->GetMapName(), sizeof( szMapName ) );
	strncpy( szHostName, m_pGameplayPage->GetHostName(), sizeof( szHostName ) );
	strncpy( szPassword, m_pGameplayPage->GetPassword(), sizeof( szPassword ) );

	char szMapCommand[1024];

	if ( m_bBotsEnabled )
	{
		// get bot difficulty from the radio buttons
		float botSkill = -1.0f;
		if ( m_pServerPage->GetBotsEnabled() )
		{
			if ( m_pServerPage->GetControlInt( "SkillLevelEasy", 0 ) )
			{
				botSkill = 0.0f;
			}
			if ( m_pServerPage->GetControlInt( "SkillLevelNormal", 0 ) )
			{
				botSkill = 1.0f;
			}
			if ( m_pServerPage->GetControlInt( "SkillLevelHard", 0 ) )
			{
				botSkill = 2.0f;
			}
			if ( m_pServerPage->GetControlInt( "SkillLevelExpert", 0 ) )
			{
				botSkill = 3.0f;
			}
		}

		m_pBotSavedData->SetInt( "bot_difficulty", (int)botSkill );

		// set other values
		if ( m_pBotPage )
		{
			m_pBotPage->UpdateKeys( m_pBotSavedData );
		}

		// set bot quota
		m_pBotSavedData->SetInt( "bot_quota", m_pServerPage->GetBotQuota() );

		// no bots when the difficulty is negative
		if ( m_pBotSavedData->GetInt( "bot_difficulty" ) < 0 )
		{
			m_pBotSavedData->SetInt( "bot_quota", 0 );
		}

		// set the map
		if ( m_pServerPage->IsRandomMapSelected() )
		{
			// it's set to random map, just save an
			m_pBotSavedData->SetString( "map", "" );
		}
		else
		{
			m_pBotSavedData->SetString( "map", szMapName );
		}

		// save the config data
		m_pBotSavedData->SaveToFile( filesystem(), "CSBotConfig.vdf", "CONFIG" );

		// create command to load map, set password, etc
		sprintf( szMapCommand, "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\ncd fadeout\nmap %s\n",
			m_pGameplayPage->GetMaxPlayers(),
			szPassword,
			szHostName,
			szMapName
		);

		// exec
		engine->pfnClientCmd( szMapCommand );
		engine->pfnClientCmd( "wait\nwait\n" );

		// set the bot cvars
		for ( KeyValues *key = m_pBotSavedData->GetFirstSubKey(); key; key = key->GetNextKey() )
		{
			static char buffer[128];
			if ( !stricmp( key->GetName(), "map" ) )
				continue;

			if ( key->GetDataType() == KeyValues::TYPE_STRING )
			{
				sprintf( buffer, "%s \"%s\"\n", key->GetName(), key->GetString() );
			}
			else
			{
				sprintf( buffer, "%s %d\n", key->GetName(), key->GetInt() );
			}

			engine->pfnClientCmd( buffer );
		}

		// make sure bots are not team balanced
		engine->pfnClientCmd( "mp_autoteambalance 0\n" );
		engine->pfnClientCmd( "mp_limitteams 0\n" );
	}
	else
	{
		// create command to load map, set password, etc
		sprintf( szMapCommand, "disconnect\nsv_lan 1\nsetmaster enable\nmaxplayers %i\nsv_password \"%s\"\nhostname \"%s\"\ncd fadeout\nmap %s\n",
			m_pGameplayPage->GetMaxPlayers(),
			szPassword,
			szHostName,
			szMapName
		);

		// exec
		engine->pfnClientCmd( szMapCommand );
		engine->pfnClientCmd( "wait\nwait\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog when it is closed
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameDialog::OnClose()
{
	MarkForDeletion();
	BaseClass::OnClose();
}
