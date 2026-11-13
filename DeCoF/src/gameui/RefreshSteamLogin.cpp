//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Asks for the Steam password again when the login has expired
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "RefreshSteamLogin.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "ienginevgui.h"
#include "SteamCommon.h"

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CRefreshSteamLogin::CRefreshSteamLogin( vgui2::Panel *parent, const char *username ) : CTaskFrame( parent, "RefreshSteamLogin" )
{
	m_bLoggingIn = false;

	SetSizeable( false );
	SetSize( 360, 224 );
	SetTitle( "#GameUI_RefreshLogin", true );

	m_pUserName = new TextEntry( this, "username" );
	m_pPassword = new TextEntry( this, "password" );
	m_pSavePassword = new CheckButton( this, "SavePasswordCheck", "" );
	m_pSavePassword->SetSelected( true );
	m_pLoginButton = new Button( this, "OKButton", "" );

	LoadControlSettings( "Resource/RefreshLogin.res" );

	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );

	m_pUserName->SetEnabled( false );
	m_pUserName->SetText( username );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CRefreshSteamLogin::~CRefreshSteamLogin()
{
}

//-----------------------------------------------------------------------------
// Purpose: Starts the login, or closes the dialog
//-----------------------------------------------------------------------------
void CRefreshSteamLogin::OnCommand( const char *command )
{
	if ( !stricmp( command, "Login" ) )
	{
		char password_guess[128];
		m_pPassword->GetText( password_guess, sizeof( password_guess ) );
		bool savePassword_guess = m_pSavePassword->IsSelected();

		if ( !EngineVGUI().SteamRefreshLogin( password_guess, savePassword_guess ) )
		{
			input()->ReleaseAppModalSurface();
			MessageBox *box = new MessageBox( "#GameUI_Login_ErrorTitle", "#GameUI_ErrorLoginFailed", this );
			box->SetCommand( "errorclose" );
			box->DoModal();
			m_RestrictedPaintPanel = box->GetVPanel();
			return;
		}

		m_bLoggingIn = true;
		m_pPassword->SetEnabled( false );
		m_pLoginButton->SetEnabled( false );
	}
	else if ( !stricmp( command, "Close" ) )
	{
		Close();
	}
	else if ( !stricmp( command, "errorclose" ) )
	{
		m_RestrictedPaintPanel = GetVPanel();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CRefreshSteamLogin::OnClose()
{
	input()->ReleaseAppModalSurface();
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: Keeps the dialog on top and polls the login call
//-----------------------------------------------------------------------------
void CRefreshSteamLogin::OnThink()
{
	surface()->RestrictPaintToSinglePanel( m_RestrictedPaintPanel );

	if ( !input()->GetFocus() || !ipanel()->HasParent( input()->GetFocus(), GetVPanel() ) )
	{
		Activate();
	}

	if ( m_bLoggingIn )
	{
		TSteamProgress progress;
		TSteamError steamError;
		bool finished;

		if ( !EngineVGUI().SteamProcessCall( &finished, &progress, &steamError ) )
		{
			m_pPassword->SetEnabled( true );
			m_pLoginButton->SetEnabled( true );
			m_bLoggingIn = false;

			input()->ReleaseAppModalSurface();
			MessageBox *box = new MessageBox( "#GameUI_Login_ErrorTitle", "#GameUI_ErrorLoginFailed", this );
			box->SetCommand( "errorclose" );
			box->DoModal();
			m_RestrictedPaintPanel = box->GetVPanel();
			return;
		}

		if ( finished )
		{
			if ( steamError.eSteamError == eSteamErrorNone )
			{
				engine->pfnClientCmd( "retry\n" );
				Close();
			}
			else
			{
				input()->ReleaseAppModalSurface();
				MessageBox *box = new MessageBox( "#GameUI_Login_ErrorTitle", "#GameUI_ErrorLoginFailed", this );
				box->SetCommand( "errorclose" );
				box->DoModal();
				m_RestrictedPaintPanel = box->GetVPanel();
			}

			m_bLoggingIn = false;
			m_pLoginButton->SetEnabled( true );
			m_pPassword->SetEnabled( true );
		}
	}

	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: Gives the password field the focus
//-----------------------------------------------------------------------------
void CRefreshSteamLogin::Activate()
{
	BaseClass::Activate();
	m_pPassword->RequestFocus();
	m_RestrictedPaintPanel = GetVPanel();
}
