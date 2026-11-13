//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CreateMultiplayerGameServerPage.h"
#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"
#include "FileSystem.h"
#include "ModInfo.h"

#include <vgui/KeyValues.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

#define RANDOM_MAP "< Random Map >"

// common/Random.cpp
extern int32 RandomLong( int32 lLow, int32 lHigh );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::CCreateMultiplayerGameServerPage( vgui2::Panel *parent, const char *name ) : PropertyPage( parent, name )
{
	// we can use this if we decide we want to put "listen server" at the end of the game name
	m_pMapList = new ComboBox( this, "MapList", 12, false );

	m_pBotQuotaCombo = new TextEntry( this, "BotQuotaCombo" );

	m_pEnableTutorCheck = new CCvarToggleCheckButton( this, "CheckButtonTutor", "#CStrike_Tutor_Enabled", "tutor_enable" );

	m_pEnableBotsCheck = new CheckButton( this, "EnableBotsCheck", "Enable bots" );

	LoadControlSettings( "Resource/CreateMultiplayerGameServerPage.res" );

	LoadMapList();

	// the tutor is only in Condition Zero
	if ( stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
	{
		m_pEnableTutorCheck->SetVisible( false );
	}

	m_szMapName[0] = 0;
	m_szHostName[0] = 0;
	m_szPassword[0] = 0;
	m_iMaxPlayers = engine->GetMaxClients();
	if ( m_iMaxPlayers <= 1 )
	{
		m_iMaxPlayers = 20;
	}

	SetControlString( "ServerNameEdit", ModInfo().GetGameDescription() );
	SetControlString( "PasswordEdit", engine->pfnGetCvarString( "sv_password" ) );

	char szBuffer[4];
	_snprintf( szBuffer, sizeof( szBuffer ) - 1, "%d", m_iMaxPlayers );
	szBuffer[ sizeof( szBuffer ) - 1 ] = 0;
	SetControlString( "MaxPlayersEdit", szBuffer );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCreateMultiplayerGameServerPage::~CCreateMultiplayerGameServerPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: called to get the info from the dialog
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnApplyChanges()
{
	strncpy( m_szHostName, GetControlString( "ServerNameEdit", "Cry of Fear" ), DATA_STR_LENGTH );
	strncpy( m_szPassword, GetControlString( "PasswordEdit", "" ), DATA_STR_LENGTH );
	m_iMaxPlayers = atoi( GetControlString( "MaxPlayersEdit", "8" ) );

	KeyValues *kv = m_pMapList->GetActiveItemUserData();
	strncpy( m_szMapName, kv->GetString( "mapname", "" ), DATA_STR_LENGTH );

	if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
	{
		m_pEnableTutorCheck->ApplyChanges();
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMaps( const char *pszPathID )
{
	FileFindHandle_t findHandle = NULL;

	const char *pszMPFilter = ModInfo().GetMPFilter();
	if ( !pszMPFilter || strlen( pszMPFilter ) < 1 )
	{
		pszMPFilter = NULL;
	}

	const char *pszFilename = filesystem()->FindFirst( "maps/*.bsp", &findHandle, pszPathID );
	while ( pszFilename )
	{
		// remove the text 'maps/' and '.bsp' from the file name to get the map name
		char mapname[256];
		char *str = strstr( pszFilename, "maps" );
		if ( str )
		{
			strncpy( mapname, str + 5, sizeof( mapname ) - 1 );	// maps + \\ = 5
		}
		else
		{
			strncpy( mapname, pszFilename, sizeof( mapname ) - 1 );
		}
		str = strstr( mapname, ".bsp" );
		if ( str )
		{
			*str = 0;
		}

		//!! hack: strip out single player HL maps
		// this needs to be specified in a seperate file
		if ( !stricmp( ModInfo().GetGameDescription(), "Half-Life" ) && ( mapname[0] == 'c' || mapname[0] == 't' ) && mapname[2] == 'a' && mapname[1] >= '0' && mapname[1] <= '5' )
		{
			goto nextFile;
		}

		// strip out Opposing Force single player maps
		if ( !stricmp( ModInfo().GetGameDescription(), "Opposing Force" ) && mapname[0] == 'o' && mapname[1] == 'f' )
		{
			goto nextFile;
		}

		// strip out maps that shouldn't be displayed
		if ( pszMPFilter && strstr( mapname, pszMPFilter ) )
		{
			goto nextFile;
		}

		// add to the map list
		m_pMapList->AddItem( mapname, new KeyValues( "data", "mapname", mapname ) );

		// get the next file
	nextFile:
		pszFilename = filesystem()->FindNext( findHandle );
	}
	filesystem()->FindClose( findHandle );
}

//-----------------------------------------------------------------------------
// Purpose: loads the list of available maps into the map list
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::LoadMapList()
{
	// clear the current list (if any)
	m_pMapList->DeleteAllItems();

	// add special "name" to represent loading a randomly selected map
	m_pMapList->AddItem( "#GameUI_RandomMap", new KeyValues( "data", "mapname", RANDOM_MAP ) );

	// iterate the filesystem getting the list of all the files
	const char *pathID = "GAME";
	if ( !stricmp( ModInfo().GetGameDescription(), "Cry of Fear" ) )
	{
		pathID = NULL; // hl is the base dir
	}

	// Load the GameDir maps
	LoadMaps( pathID );

	// If we're not the Valve directory and we're using a "fallback_dir" in gameinfo.txt then include those maps...
	// (pathID is NULL if we're "Half-Life")
	const char *pszFallback = ModInfo().GetFallbackDir();
	if ( pathID && pszFallback[0] && ModInfo().UseFallbackDirMaps() )
	{
		LoadMaps( "GAME_FALLBACK" );
	}

	// set the first item to be selected
	m_pMapList->ActivateItem( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: sets the number of bots
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SetBotQuota( int quota )
{
	char buf[32];
	sprintf( buf, "%d", quota );
	m_pBotQuotaCombo->SetText( buf );
}

//-----------------------------------------------------------------------------
// Purpose: turns the bots on or off
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SetBotsEnabled( bool enabled )
{
	m_pEnableBotsCheck->SetSelected( enabled );
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of bots
//-----------------------------------------------------------------------------
int CCreateMultiplayerGameServerPage::GetBotQuota( void )
{
	char buf[32];
	m_pBotQuotaCombo->GetText( buf, sizeof( buf ) );
	return atoi( buf );
}

//-----------------------------------------------------------------------------
// Purpose: returns whether the bots are on
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameServerPage::GetBotsEnabled( void )
{
	return m_pEnableBotsCheck->IsSelected();
}

//-----------------------------------------------------------------------------
// Purpose: returns whether the random map entry is selected
//-----------------------------------------------------------------------------
bool CCreateMultiplayerGameServerPage::IsRandomMapSelected()
{
	const char *mapname = m_pMapList->GetActiveItemUserData()->GetString( "mapname" );
	if ( !stricmp( mapname, RANDOM_MAP ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns the selected map, or a random one
//-----------------------------------------------------------------------------
const char *CCreateMultiplayerGameServerPage::GetMapName()
{
	int count = m_pMapList->GetItemCount();

	// if there is only one entry it's the special "select random map" entry
	if ( count <= 1 )
		return NULL;

	const char *mapname = m_pMapList->GetActiveItemUserData()->GetString( "mapname" );
	if ( !strcmp( mapname, RANDOM_MAP ) )
	{
		int which = RandomLong( 1, count - 1 );
		mapname = m_pMapList->GetItemUserData( which )->GetString( "mapname" );
	}

	return mapname;
}

//-----------------------------------------------------------------------------
// Purpose: Sets currently selected map in the map combobox
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::SetMap( const char *mapName )
{
	for ( int i = 0; i < m_pMapList->GetItemCount(); i++ )
	{
		if ( !m_pMapList->IsItemIDValid( i ) )
			continue;

		if ( !stricmp( m_pMapList->GetItemUserData( i )->GetString( "mapname" ), mapName ) )
		{
			m_pMapList->ActivateItem( i );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: enables the bot controls when the bots are turned on
//-----------------------------------------------------------------------------
void CCreateMultiplayerGameServerPage::OnCheckButtonChecked()
{
	SetControlEnabled( "SkillLevelExpert", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelHard", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelNormal", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "SkillLevelEasy", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotQuotaCombo", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotDifficultyLabel", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotLabel1", m_pEnableBotsCheck->IsSelected() );
	SetControlEnabled( "BotLabel2", m_pEnableBotsCheck->IsSelected() );
}
