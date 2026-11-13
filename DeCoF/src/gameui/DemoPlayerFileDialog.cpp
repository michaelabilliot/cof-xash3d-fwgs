//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "DemoPlayerFileDialog.h"

#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <FileSystem.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDemoPlayerFileDialog::CDemoPlayerFileDialog( vgui2::Panel *parent, const char *name ) : Frame( parent, name )
{
	SetBounds( 0, 0, 278, 380 );
	SetSizeable( false );

	surface()->CreatePopup( GetVPanel(), false );

	SetTitle( "#GameUI_LoadDemo", true );

	m_pDemoList = new ListPanel( this, "DemoList" );

	Button *load = new Button( this, "LoadButton", "#GameUI_Load" );
	Button *cancel = new Button( this, "CancelButton", "#GameUI_Cancel" );

	LoadControlSettings( "Resource/DemoPlayerFileDialog.res" );

	m_pDemoList->AddColumnHeader( 0, "demoname", "#GameUI_DemoFile", m_pDemoList->GetWide(), 0 );

	LoadDemoList();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDemoPlayerFileDialog::~CDemoPlayerFileDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Fills the list with the demo files of the game directory
//-----------------------------------------------------------------------------
void CDemoPlayerFileDialog::LoadDemoList()
{
	m_pDemoList->DeleteAllItems();

	FileFindHandle_t findHandle = NULL;
	const char *filename = filesystem()->FindFirst( "*.dem", &findHandle );

	while ( filename )
	{
		m_pDemoList->AddItem( new KeyValues( "data", "demoname", filename ), 0, false, false );
		filename = filesystem()->FindNext( findHandle );
	}

	filesystem()->FindClose( findHandle );

	if ( m_pDemoList->GetItemCount() > 0 )
	{
		int itemID = m_pDemoList->GetItemIDFromRow( 0 );
		m_pDemoList->SetSingleSelectedItem( itemID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Posts the selected demo
//-----------------------------------------------------------------------------
void CDemoPlayerFileDialog::OnCommand( const char *command )
{
	if ( !strcmp( command, "load" ) )
	{
		int itemID = m_pDemoList->GetSelectedItem( 0 );

		if ( m_pDemoList->IsValidItemID( itemID ) )
		{
			KeyValues *kv = m_pDemoList->GetItem( itemID );
			PostActionSignal( new KeyValues( "DemoSelected", "demoname", kv->GetString( "demoname", "" ) ) );
		}

		OnClose();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CDemoPlayerFileDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
