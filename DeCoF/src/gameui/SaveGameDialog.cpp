//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "SaveGameDialog.h"
#include "EngineInterface.h"
#include "IBaseUI.h"
#include "Taskbar.h"
#include "FileSystem.h"

#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui2;

extern IBaseUI *baseuifuncs;

#define MAKEID(d,c,b,a)		( ((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)) )
#define SAVEGAME_VERSION	0x0071		// Version 0.71

int TimeStampSortFunc( ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2 );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSaveGameDialog::CSaveGameDialog( vgui2::Panel *parent ) : CTaskFrame( parent, "SaveGameDialog" )
{
	SetBounds( 0, 0, 512, 384 );
	SetSizeable( true );

	MakePopup();
	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_SaveGame", true );

	new Label( this, "HelpText", "#GameUI_SaveGameHelp" );

	Button *savegame = new Button( this, "Save", "#GameUI_Save" );
	savegame->SetCommand( "Save" );

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	CreateSavedGamesList();
	ScanSavedGames();

	LoadControlSettings( "Resource\\SaveGameDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSaveGameDialog::~CSaveGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Saves the selected game
//-----------------------------------------------------------------------------
void CSaveGameDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Save" ) )
	{
		KeyValues *kv = m_pGameList->GetItem( m_pGameList->GetSelectedItem( 0 ) );
		if ( kv )
		{
			const char *shortName = kv->GetString( "ShortName", "" );
			if ( shortName && shortName[ 0 ] )
			{
				char sz[ 256 ];
				sprintf( sz, "save %s\n", shortName );
				engine->pfnClientCmd( sz );

				if ( baseuifuncs )
				{
					baseuifuncs->HideGameUI();
				}

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
void CSaveGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Creates the list of saved games
//-----------------------------------------------------------------------------
void CSaveGameDialog::CreateSavedGamesList()
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
// Purpose: Reads the map name and the comment of a saved game
//-----------------------------------------------------------------------------
int SaveReadNameAndComment( FileHandle_t f, char *name, char *comment )
{
	int i, tag, size, tokenSize, tokenCount;
	char *pSaveData, *pFieldName, **pTokenList;

	filesystem()->Read( &tag, sizeof(int), f );
	if ( tag != MAKEID('J','S','A','V') )
	{
		return 0;
	}

	filesystem()->Read( &tag, sizeof(int), f );
	if ( tag != SAVEGAME_VERSION )				// Enforce version for now
	{
		return 0;
	}

	name[0] = '\0';
	comment[0] = '\0';
	filesystem()->Read( &size, sizeof(int), f );

	filesystem()->Read( &tokenCount, sizeof(int), f );	// These two ints are the token list
	filesystem()->Read( &tokenSize, sizeof(int), f );
	size += tokenSize;

	// Sanity Check.
	if ( tokenCount < 0 || tokenCount > 1024*1024*32 )
	{
		return 0;
	}

	if ( tokenSize < 0 || tokenSize > 1024*1024*32 )
	{
		return 0;
	}

	pSaveData = new char[size];
	filesystem()->Read( pSaveData, size, f );

	int nNumberOfFields;

	char *pData;
	int nFieldSize;

	pData = pSaveData;

	// Allocate a table for the strings, and parse the table
	if ( tokenSize > 0 )
	{
		pTokenList = new char *[tokenCount];

		// Make sure the token strings pointed to by the pToken hashtable.
		for ( i=0; i<tokenCount; i++ )
		{
			pTokenList[i] = *pData ? pData : NULL;	// Point to each string in the pToken table
			while ( *pData++ );						// Find next token (after next null)
		}
	}
	else
		pTokenList = NULL;

	// short, short (size, index of field name)
	nFieldSize = *(short *)pData;
	pData += sizeof(short);
	pFieldName = pTokenList[ *(short *)pData ];

	if ( stricmp( pFieldName, "GameHeader" ) )
	{
		delete[] pSaveData;
		return 0;
	};

	// int (fieldcount)
	pData += sizeof(short);
	nNumberOfFields = (int)*pData;
	pData += nFieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for ( i = 0; i < nNumberOfFields; i++ )
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		nFieldSize = *(short *)pData;
		pData += sizeof(short);

		pFieldName = pTokenList[ *(short *)pData ];
		pData += sizeof(short);

		if ( !stricmp( pFieldName, "comment" ) )
		{
			strncpy( comment, pData, nFieldSize );
		}
		else if ( !stricmp( pFieldName, "mapName" ) )
		{
			strncpy( name, pData, nFieldSize );
		};

		// Move to Start of next field.
		pData += nFieldSize;
	};

	// Delete the string table we allocated
	delete[] pTokenList;
	delete[] pSaveData;

	if ( strlen( name ) > 0 && strlen( comment ) > 0 )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Sorts the saved games by time, newest first
//-----------------------------------------------------------------------------
int TimeStampSortFunc( ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2 )
{
	int t1 = p1.kv->GetInt( "timestamp" );
	int t2 = p2.kv->GetInt( "timestamp" );

	return ( t1 < t2 ) ? 1 : -1;
}

//-----------------------------------------------------------------------------
// Purpose: Fills a list item with the data of a saved game
//-----------------------------------------------------------------------------
bool CSaveGameDialog::ParseSaveData( char const *pszFileName, char const *pszShortName, KeyValues *kv )
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
	kv->SetInt( "Quick", strstr( pszFileName, "quick" ) != NULL );
	kv->SetInt( "Autosave", strstr( pszFileName, "autosave" ) != NULL );

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
// Purpose: Builds the list of saved games, with an entry for a new one
//-----------------------------------------------------------------------------
void CSaveGameDialog::ScanSavedGames()
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

	// add a new saved game option
	KeyValues *dummy = new KeyValues( "SavedGame" );
	dummy->SetString( "Game", "#GameUI_SaveGame_NewSavedGame" );
	dummy->SetString( "Elapsed Time", "#GameUI_SaveGame_New" );
	dummy->SetString( "Time", "#GameUI_SaveGame_Current" );
	dummy->SetInt( "timestamp", 0x7FFFFFFF );
	dummy->SetString( "ShortName", FindSaveSlot() );
	m_pGameList->AddItem( dummy, 0, false, false );

	// sort the save list
	m_pGameList->SortList();

	// select the first item
	m_pGameList->SetSingleSelectedItem( m_pGameList->GetItemIDFromRow( 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the name of the first unused save slot
//-----------------------------------------------------------------------------
const char *CSaveGameDialog::FindSaveSlot()
{
	static char szFileName[512];
	for ( int i = 0; i < 1000; i++ )
	{
		sprintf( szFileName, "save/Half-Life-%03i.sav", i );

		FileHandle_t fp = filesystem()->Open( szFileName, "rb" );
		if ( !fp )
		{
			// clean up name
			return szFileName + 5;
		}
		filesystem()->Close( fp );
	}

	return "error.sav";
}
