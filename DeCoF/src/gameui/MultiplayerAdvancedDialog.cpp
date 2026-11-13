//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ScriptObject.h"
#include "MultiplayerAdvancedDialog.h"
#include "PanelListPanel.h"
#include "ModInfo.h"
#include "Taskbar.h"

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

#define OPTIONS_FILE "user.scr"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedPage::CMultiplayerAdvancedPage( vgui2::Panel *parent ) : PropertyPage( parent, "MultiplayerAdvancedDialog" )
{
	m_pListPanel = new CPanelListPanel( this, "PanelListPanel" );

	m_pList = NULL;

	m_pDescription = new CInfoDescription( m_pListPanel );
	m_pDescription->InitFromFile( OPTIONS_FILE );
	m_pDescription->TransferCurrentValues( NULL );

	CreateControls();

	LoadControlSettings( "Resource\\MultiplayerAdvancedPage.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedDialog::CMultiplayerAdvancedDialog( vgui2::Panel *parent ) : BaseClass( parent, "MultiplayerAdvancedDialog" )
{
	SetBounds( 0, 0, 372, 160 );
	SetSizeable( false );

	MakePopup();
	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_MultiplayerAdvanced", true );

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	Button *ok = new Button( this, "OK", "#GameUI_OK" );
	ok->SetCommand( "Ok" );

	m_pListPanel = new CPanelListPanel( this, "PanelListPanel" );

	m_pList = NULL;

	m_pDescription = new CInfoDescription( m_pListPanel );
	m_pDescription->InitFromFile( OPTIONS_FILE );
	m_pDescription->TransferCurrentValues( NULL );

	CreateControls();

	LoadControlSettings( "Resource\\MultiplayerAdvancedDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedDialog::~CMultiplayerAdvancedDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMultiplayerAdvancedPage::~CMultiplayerAdvancedPage()
{
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the dialog when it is closed
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Saves the options on OK
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		// OnApplyChanges();
		SaveValues();
		OnClose();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reads the values back from the controls
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedPage::OnResetData()
{
	GatherCurrentValues();
}

//-----------------------------------------------------------------------------
// Purpose: Saves the options
//-----------------------------------------------------------------------------
void CMultiplayerAdvancedPage::OnApplyChanges()
{
	SaveValues();
}

//-----------------------------------------------------------------------------
// Purpose: Copies the values of the controls into the script objects
//-----------------------------------------------------------------------------
void CMultiplayerAdvanced::GatherCurrentValues()
{
	if ( !m_pDescription )
		return;

	// OK
	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;

	mpcontrol_t *pList;

	CScriptObject *pObj;
	CScriptListItem *pItem;

	char szValue[256];
	char strValue[256];

	pList = m_pList;
	while ( pList )
	{
		pObj = pList->pScrObj;

		if ( !pList->pControl )
		{
			pObj->SetCurValue( pObj->defValue );
			pList = pList->next;
			continue;
		}

		switch ( pObj->type )
		{
		case O_BOOL:
			pBox = (CheckButton *)pList->pControl;
			sprintf( szValue, "%s", pBox->IsSelected() ? "1" : "0" );
			break;
		case O_NUMBER:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_STRING:
			pEdit = ( TextEntry * )pList->pControl;
			pEdit->GetText( strValue, sizeof( strValue ) );
			sprintf( szValue, "%s", strValue );
			break;
		case O_LIST:
			{
				pCombo = (ComboBox *)pList->pControl;
				int activeItem = pCombo->GetActiveItem();

				pItem = pObj->pListItems;
				int n = (int)pObj->fdefValue;

				while ( pItem )
				{
					if ( !activeItem-- )
						break;

					pItem = pItem->pNext;
				}

				if ( pItem )
				{
					sprintf( szValue, "%s", pItem->szValue );
				}
				else  // Couln't find index
				{
					sprintf( szValue, "%s", pObj->defValue );
				}
				break;
			}
		}

		// Remove double quotes and % characters
		UTIL_StripInvalidCharacters( szValue );

		strcpy( strValue, szValue );

		pObj->SetCurValue( strValue );

		pList = pList->next;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds a control for each script object
//-----------------------------------------------------------------------------
void CMultiplayerAdvanced::CreateControls()
{
	DestroyControls();

	// Go through desciption creating controls
	CScriptObject *pObj;

	pObj = m_pDescription->pObjList;

	mpcontrol_t *pCtrl;

	CheckButton *pBox;
	TextEntry *pEdit;
	ComboBox *pCombo;
	CScriptListItem *pListItem;

	Panel *objParent = m_pListPanel;

	while ( pObj )
	{
		pCtrl = new mpcontrol_t( objParent, "mpcontrol_t" );
		pCtrl->type = pObj->type;

		switch ( pCtrl->type )
		{
		case O_BOOL:
			pBox = new CheckButton( pCtrl, "DescCheckButton", pObj->prompt );
			pBox->SetSelected( pObj->fdefValue != 0.0f ? true : false );

			pCtrl->pControl = (Panel *)pBox;
			break;
		case O_STRING:
		case O_NUMBER:
			pEdit = new TextEntry( pCtrl, "DescTextEntry" );
			pEdit->InsertString( pObj->defValue );
			pCtrl->pControl = (Panel *)pEdit;
			break;
		case O_LIST:
			pCombo = new ComboBox( pCtrl, "DescComboBox", 5, false );

			pListItem = pObj->pListItems;
			while ( pListItem )
			{
				pCombo->AddItem( pListItem->szItemText, NULL );
				pListItem = pListItem->pNext;
			}

			pCombo->ActivateItemByRow( (int)pObj->fdefValue );

			pCtrl->pControl = (Panel *)pCombo;
			break;
		default:
			break;
		}

		if ( pCtrl->type != O_BOOL )
		{
			pCtrl->pPrompt = new Label( pCtrl, "DescLabel", "" );
			pCtrl->pPrompt->SetContentAlignment( Label::a_west );
			pCtrl->pPrompt->SetTextInset( 5, 0 );
			pCtrl->pPrompt->SetText( pObj->prompt );
		}

		pCtrl->pScrObj = pObj;
		pCtrl->SetSize( 100, 28 );
		m_pListPanel->AddItem( pCtrl );

		// Link it in
		if ( !m_pList )
		{
			m_pList = pCtrl;
			pCtrl->next = NULL;
		}
		else
		{
			mpcontrol_t *p;
			p = m_pList;
			while ( p )
			{
				if ( !p->next )
				{
					p->next = pCtrl;
					pCtrl->next = NULL;
					break;
				}
				p = p->next;
			}
		}

		pObj = pObj->pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Deletes the controls
//-----------------------------------------------------------------------------
void CMultiplayerAdvanced::DestroyControls()
{
	mpcontrol_t *p, *n;

	p = m_pList;
	while ( p )
	{
		n = p->next;
		//
		delete p->pControl;
		delete p->pPrompt;
		delete p;
		p = n;
	}

	m_pList = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Writes the options to the config and to user.scr
//-----------------------------------------------------------------------------
void CMultiplayerAdvanced::SaveValues()
{
	// Get the values from the controls:
	GatherCurrentValues();

	// Create the game.cfg file
	if ( m_pDescription )
	{
		FileHandle_t fp;

		// Add settings to config.cfg
		m_pDescription->WriteToConfig();

		fp = filesystem()->Open( OPTIONS_FILE, "wb" );
		if ( fp )
		{
			m_pDescription->WriteToScriptFile( fp );
			filesystem()->Close( fp );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Header of user.scr
//-----------------------------------------------------------------------------
void CInfoDescription::WriteScriptHeader( FileHandle_t fp )
{
	char am_pm[] = "AM";
	tm newtime;
	time_t ltime;

	time( &ltime );
	tm *pTime = localtime( &ltime );
	memcpy( &newtime, pTime, sizeof( tm ) );

	filesystem()->FPrintf( fp, (char *)getHint() );
	filesystem()->FPrintf( fp, "// Half-Life User Info Configuration Layout Script (stores last settings chosen, too)\r\n" );
	filesystem()->FPrintf( fp, "// File generated:  %.19s %s\r\n", asctime( &newtime ), am_pm );
	filesystem()->FPrintf( fp, "//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
	filesystem()->FPrintf( fp, "VERSION %.1f\r\n\r\n", 1.0 );
	filesystem()->FPrintf( fp, "DESCRIPTION INFO_OPTIONS\r\n{\r\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Header of the generated config
//-----------------------------------------------------------------------------
void CInfoDescription::WriteFileHeader( FileHandle_t fp )
{
	char am_pm[] = "AM";
	tm newtime;
	time_t ltime;

	time( &ltime );
	tm *pTime = localtime( &ltime );
	memcpy( &newtime, pTime, sizeof( tm ) );

	filesystem()->FPrintf( fp, "// Half-Life User Info Configuration Settings\r\n" );
	filesystem()->FPrintf( fp, "// DO NOT EDIT, GENERATED BY HALF-LIFE\r\n" );
	filesystem()->FPrintf( fp, "// File generated:  %.19s %s\r\n", asctime( &newtime ), am_pm );
	filesystem()->FPrintf( fp, "//\r\n//\r\n// Cvar\t-\tSetting\r\n\r\n" );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CInfoDescription::CInfoDescription( CPanelListPanel *panel ) : CDescription( panel )
{
	setHint( "// NOTE:  THIS FILE IS AUTOMATICALLY REGENERATED, \r\n"
"//DO NOT EDIT THIS HEADER, YOUR COMMENTS WILL BE LOST IF YOU DO\r\n"
"// User options script\r\n"
"//\r\n"
"// Format:\r\n"
"//  Version [float]\r\n"
"//  Options description followed by \r\n"
"//  Options defaults\r\n"
"//\r\n"
"// Option description syntax:\r\n"
"//\r\n"
"//  \"cvar\" { \"Prompt\" { type [ type info ] } { default } }\r\n"
"//\r\n"
"//  type = \r\n"
"//   BOOL   (a yes/no toggle)\r\n"
"//   STRING\r\n"
"//   NUMBER\r\n"
"//   LIST\r\n"
"//\r\n"
"// type info:\r\n"
"// BOOL                 no type info\r\n"
"// NUMBER       min max range, use -1 -1 for no limits\r\n"
"// STRING       no type info\r\n"
"// LIST          delimited list of options value pairs\r\n"
"//\r\n"
"//\r\n"
"// default depends on type\r\n"
"// BOOL is \"0\" or \"1\"\r\n"
"// NUMBER is \"value\"\r\n"
"// STRING is \"value\"\r\n"
"// LIST is \"index\", where index \"0\" is the first element of the list\r\n\r\n\r\n" );

	setDescription( "INFO_OPTIONS" );
}

//-----------------------------------------------------------------------------
// Purpose: Loads user.scr and adds the options of user_default.scr it lacks
//-----------------------------------------------------------------------------
bool CInfoDescription::InitFromFile( char *pszFileName )
{
	bool ret = CDescription::InitFromFile( pszFileName );

	if ( !stricmp( ModInfo().GetGameDescription(), "Condition Zero" ) )
	{
		// Condition Zero calls the tutor cvar tutor_enable
		CScriptObject *p = pObjList;
		while ( p )
		{
			if ( !stricmp( p->cvarname, "cl_tutor" ) )
			{
				strcpy( p->cvarname, "tutor_enable" );
			}
			p = p->pNext;
		}
	}

	// Load the defaults
	CDescription *tmp = new CInfoDescription( NULL );

	FileHandle_t file = filesystem()->Open( "user_default.scr", "rb" );
	if ( file )
	{
		// read the file
		int len = filesystem()->Size( file );
		unsigned char *buffer = new unsigned char[ len ];
		filesystem()->Read( buffer, len, file );
		filesystem()->Close( file );

		char *pBuffer = (char *)buffer;
		tmp->ReadFromBuffer( &pBuffer );
		delete[] buffer;
	}

	// Add any defaults the user file lacks
	CScriptObject *defaultItem = tmp->pObjList;
	CScriptObject *userItem;

	while ( defaultItem )
	{
		bool found = false;
		userItem = pObjList;
		while ( userItem )
		{
			if ( !stricmp( userItem->cvarname, defaultItem->cvarname ) )
			{
				found = true;
				break;
			}
			userItem = userItem->pNext;
		}

		if ( !found )
		{
			CScriptObject *pObj = new CScriptObject();
			*pObj = *defaultItem;
			pObj->pListItems = NULL;

			CScriptListItem *pDefaultItem = defaultItem->pListItems;
			while ( pDefaultItem )
			{
				CScriptListItem *pItem = new CScriptListItem();
				*pItem = *pDefaultItem;
				pObj->AddItem( pItem );
				pDefaultItem = pDefaultItem->pNext;
			}

			AddObject( pObj );
		}

		defaultItem = defaultItem->pNext;
	}

	delete tmp;
	return ret;
}
