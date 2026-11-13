//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "CDKeyEntryDialog.h"
#include "EngineInterface.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Text entry that only takes upper case letters and numbers
//-----------------------------------------------------------------------------
class CUpperCaseTextEntry : public TextEntry
{
public:
	CUpperCaseTextEntry( Panel *parent, const char *panelName ) : TextEntry( parent, panelName ) {}

	virtual void OnKeyTyped( wchar_t unichar )
	{
		// only allow alpha-numeric characters
		if ( !iswalnum( unichar ) )
			return;

		// convert to upper case
		if ( unichar >= 'a' && unichar <= 'z' )
		{
			TextEntry::OnKeyTyped( unichar - ( 'a' - 'A' ) );
		}
		else
		{
			TextEntry::OnKeyTyped( unichar );
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: Message box that quits the game when it is closed
//-----------------------------------------------------------------------------
class CloseMessageBox : public MessageBox
{
public:
	CloseMessageBox( const char *title, const char *text, Panel *parent = NULL ) : MessageBox( title, text, parent ) {}

	virtual void OnClose()
	{
		engine->pfnClientCmd( "quit\n" );
	}
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCDKeyEntryDialog::CCDKeyEntryDialog( vgui2::Panel *parent, bool inConnect ) : BaseClass( parent, "CDKeyEntryDialog" )
{
	m_bIsValveCDKey = true;
	m_bInConnect = inConnect;
	m_iErrCount = 0;

	char cdKeyType[64];
	if ( engine->CheckParm( "-forcevalve", NULL ) || system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Half-Life\\Settings\\ValveKey", cdKeyType, sizeof( cdKeyType ) ) )
	{
		m_bIsValveCDKey = true;
	}

	m_pOK = new Button( this, "OKButton", "#GameUI_OK" );
	m_pQuitGame = new Button( this, "CancelButton", "#GameUI_Quit" );
	m_pEntry1 = new CUpperCaseTextEntry( this, "Entry1" );
	m_pEntry2 = new CUpperCaseTextEntry( this, "Entry2" );
	m_pEntry3 = new CUpperCaseTextEntry( this, "Entry3" );
	m_pEntry4 = new CUpperCaseTextEntry( this, "Entry4" );
	m_pEntry5 = new CUpperCaseTextEntry( this, "Entry5" );

	if ( m_bIsValveCDKey )
	{
		m_pEntry1->SetMaximumCharCount( 5 );
		m_pEntry2->SetMaximumCharCount( 5 );
		m_pEntry3->SetMaximumCharCount( 5 );
		m_pEntry4->SetMaximumCharCount( 5 );
		m_pEntry5->SetMaximumCharCount( 5 );
	}

	SetSizeable( false );
	SetSize( 360, 224 );
	SetTitle( "#GameUI_CDKey", true );

	if ( m_bIsValveCDKey )
	{
		LoadControlSettings( "Resource/ValveCDKeyEntryDialog.res" );
	}
	else
	{
		LoadControlSettings( "Resource/CDKeyEntryDialog.res" );
	}

	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );

	m_pOK->SetEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCDKeyEntryDialog::~CCDKeyEntryDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Stores the key or tells the user it is invalid
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnCommand( const char *command )
{
	bool valid = IsEnteredKeyValid();

	if ( !stricmp( command, "OK" ) )
	{
		if ( valid )
		{
			char cdkey[32];
			memset( cdkey, 0, sizeof( cdkey ) );

			if ( m_bIsValveCDKey )
			{
				m_pEntry1->GetText( cdkey, 6 );
				m_pEntry2->GetText( cdkey + 6, 6 );
				m_pEntry3->GetText( cdkey + 12, 6 );
				m_pEntry4->GetText( cdkey + 18, 6 );
				m_pEntry5->GetText( cdkey + 24, 6 );
				cdkey[5] = '-';
				cdkey[11] = '-';
				cdkey[17] = '-';
				cdkey[23] = '-';
				system()->SetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Half-Life\\Settings\\ValveKey", cdkey );
			}

			if ( m_bInConnect )
			{
				engine->pfnClientCmd( "retry\n" );
			}

			BaseClass::OnCommand( "Close" );
		}
		else
		{
			MessageBox *bx = new MessageBox( "#GameUI_CDKey_Invalid_Title", "#GameUI_CDKey_Invalid_Text", this );
			bx->ShowWindow( this );
		}
	}
	else if ( !stricmp( command, "Cancel" ) || !stricmp( command, "Close" ) )
	{
		BaseClass::OnCommand( "Close" );
	}
	else
	{
		BaseClass::OnCommand( command );
	}

	if ( !valid )
	{
		m_iErrCount++;
		if ( m_iErrCount >= 5 )
		{
			CloseMessageBox *bx = new CloseMessageBox( "#GameUI_CDKey_Invalid_Title", "#GameUI_CDKey_TooManyTries", this );
			bx->ShowWindow( this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Quits the game when no valid key was entered
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnClose()
{
	if ( !IsEnteredKeyValid() )
	{
		engine->pfnClientCmd( "quit\n" );
	}

	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Keeps the dialog on top and focused
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnThink()
{
	// force us to be the only thing to render
	surface()->RestrictPaintToSinglePanel( GetVPanel() );

	// make sure we're the focus
	if ( !input()->GetFocus() || !ipanel()->HasParent( input()->GetFocus(), GetVPanel() ) )
	{
		Activate();
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Validates the key
//-----------------------------------------------------------------------------
bool CCDKeyEntryDialog::IsEnteredKeyValid()
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the focus to the next entry once one is full
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::OnTextChanged( Panel *entry )
{
	char cdkey[32];

	if ( m_bIsValveCDKey )
	{
		m_pEntry1->GetText( cdkey, 6 );
		m_pEntry2->GetText( cdkey + 6, 6 );
		m_pEntry3->GetText( cdkey + 12, 6 );
		m_pEntry4->GetText( cdkey + 18, 6 );
		m_pEntry5->GetText( cdkey + 24, 6 );
		cdkey[5] = '-';
		cdkey[11] = '-';
		cdkey[17] = '-';
		cdkey[23] = '-';
	}

	if ( m_bIsValveCDKey && strlen( cdkey ) == 29 )
	{
		m_pOK->SetEnabled( true );
	}
	else
	{
		memset( cdkey, 0, sizeof( cdkey ) );

		if ( entry == m_pEntry1 )
		{
			m_pEntry1->GetText( cdkey, 6 );
			if ( (int)strlen( cdkey ) >= ( m_bIsValveCDKey ? 5 : 4 ) )
			{
				m_pEntry2->RequestFocus();
			}
		}
		else if ( entry == m_pEntry2 )
		{
			m_pEntry2->GetText( cdkey, 6 );
			if ( strlen( cdkey ) >= 5 )
			{
				m_pEntry3->RequestFocus();
			}
		}
		else if ( m_bIsValveCDKey && entry == m_pEntry3 )
		{
			m_pEntry3->GetText( cdkey, 6 );
			if ( strlen( cdkey ) >= 5 )
			{
				m_pEntry4->RequestFocus();
			}
		}
		else if ( m_bIsValveCDKey && entry == m_pEntry4 )
		{
			m_pEntry4->GetText( cdkey, 6 );
			if ( strlen( cdkey ) >= 5 )
			{
				m_pEntry5->RequestFocus();
			}
		}

		m_pOK->SetEnabled( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Focuses the first entry
//-----------------------------------------------------------------------------
void CCDKeyEntryDialog::Activate()
{
	BaseClass::Activate();
	m_pEntry1->RequestFocus();
}
