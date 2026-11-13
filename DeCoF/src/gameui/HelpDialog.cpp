//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "HelpDialog.h"
#include "Taskbar.h"

#include <vgui/ISystem.h>
#include <vgui/KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHelpDialog::CHelpDialog() : CTaskFrame( NULL, "HelpDialog" )
{
	g_pTaskbar->AddTask( GetVPanel() );

	m_pInfoText = new Label( this, "InfoText", "#GameUI_InfoText" );
	m_pClose = new Button( this, "CloseButton", "#GameUI_Close" );
	m_pNeverShowAgain = new CheckButton( this, "NeverShowButton", "#GameUI_NeverShowButton" );

	SetTitle( "#GameUI_HalfLifeDesktopHelp", true );

	LoadControlSettings( "Resource/DialogHelpIngame.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHelpDialog::~CHelpDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Remembers the "never show again" choice and deletes the dialog
//-----------------------------------------------------------------------------
void CHelpDialog::OnClose()
{
	BaseClass::OnClose();

	if ( m_pNeverShowAgain->IsSelected() )
	{
		system()->SetRegistryInteger( "HKEY_CURRENT_USER\\Software\\Valve\\Tracker\\NeverShowHelp", 1 );
	}

	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Handles the close button
//-----------------------------------------------------------------------------
void CHelpDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Close" ) )
	{
		PostMessage( this, new KeyValues( "Close" ) );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}
