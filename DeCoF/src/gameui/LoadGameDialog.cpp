//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "LoadGameDialog.h"
#include "EngineInterface.h"
#include "Taskbar.h"
#include "FileSystem.h"

#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui2;

const int SAVEGAME_MAPNAME_LEN = 32;
const int SAVEGAME_COMMENT_LEN = 80;
const int SAVEGAME_ELAPSED_LEN = 32;

int SaveReadNameAndComment( FileHandle_t f, char *name, char *comment );
int TimeStampSortFunc( ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLoadGameDialog::CLoadGameDialog( vgui2::Panel *parent ) : CTaskFrame( parent, "LoadGameDialog" )
{
	SetBounds( 0, 0, 512, 384 );
	SetMinimumSize( 256, 300 );
	SetSizeable( true );

	MakePopup();
	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_LoadGame", true );

	new Label( this, "HelpText", "#GameUI_LoadGameHelp" );

	Button *load = new Button( this, "Load", "#GameUI_Load" );
	load->SetCommand( "Load" );

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	CreateSavedGamesList();
	ScanSavedGames();

	LoadControlSettings( "Resource\\LoadGameDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadGameDialog::~CLoadGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Loads the selected game
//-----------------------------------------------------------------------------
void CLoadGameDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Load" ) )
	{
		KeyValues *kv = m_pGameList->GetItem( m_pGameList->GetSelectedItem( 0 ) );
		if ( kv )
		{
			const char *shortName = kv->GetString( "ShortName", "" );
			if ( shortName && shortName[ 0 ] )
			{
				char sz[ 256 ];
				sprintf( sz, "load %s\n", shortName );
				engine->pfnClientCmd( sz );

				OnClose();
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog when it is closed
//-----------------------------------------------------------------------------
void CLoadGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the list of saved games
//-----------------------------------------------------------------------------
void CLoadGameDialog::CreateSavedGamesList()
{
	m_pGameList = new ListPanel( this, "listpanel_loadgame" );
	m_pGameList->AddColumnHeader( 0, "Game", "#GameUI_SavedGame", 175 );
	m_pGameList->AddColumnHeader( 1, "Type", "#GameUI_Type", 100 );
	m_pGameList->AddColumnHeader( 2, "Elapsed Time", "#GameUI_ElapsedTime", 75 );
	m_pGameList->AddColumnHeader( 3, "Time", "#GameUI_TimeStamp", 175 );
	m_pGameList->SetSortFunc( 3, TimeStampSortFunc );
	m_pGameList->SetSortColumn( 3 );
}

//-----------------------------------------------------------------------------
// Purpose: Fills a list item with the data of a saved game
//-----------------------------------------------------------------------------
bool CLoadGameDialog::ParseSaveData( char const *pszFileName, char const *pszShortName, KeyValues *kv )
{
	char szMapName[SAVEGAME_MAPNAME_LEN];
	char szComment[SAVEGAME_COMMENT_LEN];
	char szElapsedTime[SAVEGAME_ELAPSED_LEN];

	if ( !pszFileName || !pszShortName || !kv )
		return false;

	kv->SetString( "ShortName", pszShortName );
	kv->SetString( "FileName", pszFileName );

	FileHandle_t fh = filesystem()->Open( pszFileName, "rb" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
		return false;

	int readok = SaveReadNameAndComment( fh, szMapName, szComment );
	filesystem()->Close( fh );

	if ( !readok )
	{
		return false;
	}

	kv->SetString( "Map", szMapName );

	// Elapsed time is the last 5 characters in comment. (mmm:ss)
	int i;
	i = strlen( szComment );
	sprintf( szElapsedTime, "??" );
	if ( i >= 6 )
	{
		strncpy( szElapsedTime, (char *)&szComment[i - 5], 5 );
		szElapsedTime[5] = '\0';

		// Chop elapsed out of comment.
		int n;

		n = i - 5;
		szComment[n] = '\0';

		n--;

		// Strip back the spaces at the end.
		while ( ( n >= 1 ) && szComment[n] && szComment[n] == ' ' )
		{
			szComment[n--] = '\0';
		}
	}

	// calculate the file name to print
	const char *pszType = NULL;
	if ( strstr( pszFileName, "quick" ) )
	{
		pszType = "#GameUI_QuickSave";
	}
	else if ( strstr( pszFileName, "autosave" ) )
	{
		pszType = "#GameUI_AutoSave";
	}

	kv->SetString( "Type", pszType );
	kv->SetString( "Game", szComment );
	kv->SetString( "Elapsed Time", szElapsedTime );

	// Now get the file time
	long fileTime = filesystem()->GetFileTime( pszFileName );
	char szFileTime[32];
	filesystem()->FileTimeToString( szFileTime, sizeof( szFileTime ), fileTime );
	char *newline = strstr( szFileTime, "\n" );
	if ( newline )
	{
		*newline = 0;
	}
	kv->SetString( "Time", szFileTime );
	kv->SetInt( "timestamp", fileTime );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Builds the list of saved games
//-----------------------------------------------------------------------------
void CLoadGameDialog::ScanSavedGames()
{
	// populate list box with all saved games on record:
	char szFileName[_MAX_PATH];
	char szDirectory[_MAX_PATH];
	sprintf( szDirectory, "save/*.sav" );

	// iterate the saved files
	FileFindHandle_t handle;
	const char *pFileName = filesystem()->FindFirst( szDirectory, &handle );
	while ( pFileName )
	{
		if ( !strnicmp( pFileName, "HLSave", strlen( "HLSave" ) ) )
		{
			pFileName = filesystem()->FindNext( handle );
			continue;
		}

		sprintf( szFileName, "save/%s", pFileName );

		KeyValues *item = new KeyValues( "SavedGame" );
		if ( ParseSaveData( szFileName, pFileName, item ) )
		{
			m_pGameList->AddItem( item, 0, false, false );
		}
		item->deleteThis();

		pFileName = filesystem()->FindNext( handle );
	}

	filesystem()->FindClose( handle );

	// sort the save list
	m_pGameList->SortList();

	// select the first item
	m_pGameList->SetSingleSelectedItem( m_pGameList->GetItemIDFromRow( 0 ) );
}
