//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "PlayerListDialog.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "IGameClientExports.h"

#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPlayerListDialog::CPlayerListDialog( vgui::Panel *parent ) : BaseClass( parent, "PlayerListDialog" )
{
	SetSize( 320, 240 );

	if ( GameClientExports() )
	{
		wchar_t title[256];
		wchar_t hostname[128];
		localize()->ConvertANSIToUnicode( GameClientExports()->GetServerHostName(), hostname, sizeof( hostname ) );
		localize()->ConstructString( title, sizeof( title ), localize()->Find( "#GameUI_PlayerListDialogTitle" ), 1, hostname );
		SetTitle( title, true );
	}
	else
	{
		SetTitle( "#GameUI_CurrentPlayers", true );
	}

	m_pMuteButton = new Button( this, "MuteButton", "" );

	m_pPlayerList = new ListPanel( this, "PlayerList" );
	m_pPlayerList->AddColumnHeader( 0, "Name", "#GameUI_PlayerName", 128 );
	m_pPlayerList->AddColumnHeader( 1, "Properties", "#GameUI_Properties", 80 );

	m_pPlayerList->SetEmptyListText( "#GameUI_NoOtherPlayersInGame" );

	LoadControlSettings( "Resource/PlayerListDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPlayerListDialog::~CPlayerListDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills the list with the players in the game
//-----------------------------------------------------------------------------
void CPlayerListDialog::Activate()
{
	BaseClass::Activate();

	// refresh player list
	m_pPlayerList->DeleteAllItems();
	int maxClients = engine->GetMaxClients();
	for ( int i = 1; i <= maxClients; i++ )
	{
		// get the player info from the engine
		char szPlayerIndex[32];
		sprintf( szPlayerIndex, "%d", i );

		KeyValues *data = new KeyValues( szPlayerIndex );
		const char *playerName = engine->PlayerInfo_ValueForKey( i, "name" );
		if ( !playerName || !*playerName )
			continue;

		data->SetString( "Name", playerName );
		data->SetInt( "index", i );

		// add to the list
		m_pPlayerList->AddItem( data, 0, false, false );
	}

	// refresh player properties info
	RefreshPlayerProperties();

	// select the first item by default
	m_pPlayerList->SetSingleSelectedItem( m_pPlayerList->GetItemIDFromRow( 0 ) );

	// toggle button states
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: walks the players and sets their info display in the list
//-----------------------------------------------------------------------------
void CPlayerListDialog::RefreshPlayerProperties()
{
	for ( int i = 0; i <= m_pPlayerList->GetItemCount(); i++ )
	{
		KeyValues *data = m_pPlayerList->GetItem( i );
		if ( !data )
			continue;

		// assemble properties
		int playerIndex = data->GetInt( "index" );

		// make sure the player is still connected
		const char *playerName = engine->PlayerInfo_ValueForKey( playerIndex, "name" );
		if ( !playerName )
		{
			// disconnected
			data->SetString( "properties", "Disconnected" );
			continue;
		}

		data->SetString( "name", playerName );

		bool muted = false, friends = false, bot = false;
		if ( GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted( playerIndex ) )
		{
			muted = true;
		}

		const char *botValue = engine ? engine->PlayerInfo_ValueForKey( playerIndex, "*bot" ) : NULL;
		if ( botValue && !stricmp( "1", botValue ) )
		{
			bot = true;
		}

		if ( bot )
		{
			data->SetString( "properties", "CPU Player" );
		}
		else if ( muted && friends )
		{
			data->SetString( "properties", "Friend; Muted" );
		}
		else if ( muted )
		{
			data->SetString( "properties", "Muted" );
		}
		else if ( friends )
		{
			data->SetString( "properties", "Friend" );
		}
		else
		{
			data->SetString( "properties", "" );
		}
	}

	m_pPlayerList->RereadAllItems();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the mute button
//-----------------------------------------------------------------------------
void CPlayerListDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Mute" ) )
	{
		ToggleMuteStateOfSelectedUser();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: toggles whether a user is muted or not
//-----------------------------------------------------------------------------
void CPlayerListDialog::ToggleMuteStateOfSelectedUser()
{
	if ( !GameClientExports() )
		return;

	KeyValues *data = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );
	if ( !data )
		return;

	int playerIndex = data->GetInt( "index" );

	if ( GameClientExports()->IsPlayerGameVoiceMuted( playerIndex ) )
	{
		GameClientExports()->UnmutePlayerGameVoice( playerIndex );
	}
	else
	{
		GameClientExports()->MutePlayerGameVoice( playerIndex );
	}

	RefreshPlayerProperties();
	OnItemSelected();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the mute button state for the selected player
//-----------------------------------------------------------------------------
void CPlayerListDialog::OnItemSelected()
{
	// make sure the data is up-to-date
	RefreshPlayerProperties();

	// set the button state based on the selected item
	if ( m_pPlayerList->GetSelectedItemsCount() > 0 )
	{
		KeyValues *data = m_pPlayerList->GetItem( m_pPlayerList->GetSelectedItem( 0 ) );

		const char *botValue = data ? engine->PlayerInfo_ValueForKey( data->GetInt( "index" ), "*bot" ) : NULL;
		bool isValidPlayer = botValue ? !stricmp( "1", botValue ) : false;

		// make sure the player is still connected
		if ( !engine->PlayerInfo_ValueForKey( data->GetInt( "index" ), "name" ) )
		{
			isValidPlayer = true;
		}

		if ( data && !isValidPlayer && GameClientExports() && GameClientExports()->IsPlayerGameVoiceMuted( data->GetInt( "index" ) ) )
		{
			m_pMuteButton->SetText( "#GameUI_UnmuteIngameVoice" );
		}
		else
		{
			m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );
		}

		if ( GameClientExports() && !isValidPlayer )
		{
			m_pMuteButton->SetEnabled( true );
		}
		else
		{
			m_pMuteButton->SetEnabled( false );
		}
	}
	else
	{
		m_pMuteButton->SetEnabled( false );
		m_pMuteButton->SetText( "#GameUI_MuteIngameVoice" );
	}
}
