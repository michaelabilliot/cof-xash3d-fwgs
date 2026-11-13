//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "ContentControlDialog.h"
#include "checksum_md5.h"
#include "EngineInterface.h"
#include "Taskbar.h"

#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CContentControlDialog::CContentControlDialog( vgui2::Panel *parent ) : CTaskFrame( parent, "ContentControlDialog" )
{
	SetBounds( 0, 0, 372, 160 );
	SetSizeable( false );

	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_ContentLock", true );

	m_pStatus = new Label( this, "ContentStatus", "" );

	m_pPasswordLabel = new Label( this, "PasswordPrompt", "#GameUI_PasswordPrompt" );
	m_pPassword2Label = new Label( this, "PasswordReentryPrompt", "#GameUI_PasswordReentryPrompt" );

	m_pExplain = new Label( this, "ContentControlExplain", "" );

	m_pPassword = new TextEntry( this, "Password" );
	m_pPassword2 = new TextEntry( this, "Password2" );

	m_pOK = new Button( this, "Ok", "#GameUI_OK" );
	m_pOK->SetCommand( "Ok" );

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Cancel" );

	m_szGorePW[ 0 ] = 0;
	ResetPassword();

	LoadControlSettings( "Resource\\ContentControlDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CContentControlDialog::~CContentControlDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::Activate()
{
	BaseClass::Activate();

	m_pPassword->SetText( "" );
	m_pPassword->RequestFocus();
	m_pPassword2->SetText( "" );
	Explain( "" );
	UpdateContentControlStatus();

	input()->SetAppModalSurface( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::ResetPassword()
{
	// Set initial value
	if ( !system()->GetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", m_szGorePW, sizeof( m_szGorePW ) ) )
	{
		m_szGorePW[ 0 ] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::ApplyPassword()
{
	WriteToken( m_szGorePW );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::Explain( char const *fmt, ... )
{
	if ( !m_pExplain )
		return;

	va_list argptr;
	char text[1024];

	va_start( argptr, fmt );
	_vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );

	m_pExplain->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *command -
//-----------------------------------------------------------------------------
void CContentControlDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		bool canclose = false;

		char pw1[ 256 ];
		char pw2[ 256 ];

		m_pPassword->GetText( pw1, 256 );
		m_pPassword2->GetText( pw2, 256 );

		// Get text and check
		if ( IsPasswordEnabledInDialog() )
		{
			canclose = DisablePassword( pw1 );
		}
		else if ( !strcmp( pw1, pw2 ) )
		{
			canclose = EnablePassword( pw1 );
		}
		else
		{
			Explain( "#GameUI_PasswordsDontMatch" );
		}

		if ( canclose )
		{
			OnClose();
		}
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		OnClose();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::OnClose()
{
	BaseClass::OnClose();
	PostActionSignal( new KeyValues( "ContentControlClose" ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
char *CContentControlDialog::BinPrintf( unsigned char *buf, int nLen )
{
	static char szReturn[1024];
	unsigned char c;
	char szChunk[10];
	int i;

	memset( szReturn, 0, sizeof( szReturn ) );

	for ( i = 0; i < nLen; i++ )
	{
		c = (unsigned char)buf[i];
		_snprintf( szChunk, sizeof( szChunk ), "%02x", c );
		strncat( szReturn, szChunk, sizeof( szReturn ) - strlen( szReturn ) - 1 );
	}
	return szReturn;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *str -
//-----------------------------------------------------------------------------
void CContentControlDialog::WriteToken( const char *str )
{
	// Set initial value
	system()->SetRegistryString( "Software\\Valve\\Half-Life\\Settings\\User Token 2", str );
	strcpy( m_szGorePW, str );

	UpdateContentControlStatus();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::HashPassword( const char *newPW, char *hashBuffer )
{
	// Compute the md5 hash and save it.
	unsigned char md5_hash[16];
	MD5Context_t ctx;

	MD5Init( &ctx );
	MD5Update( &ctx, (unsigned char const *)newPW, strlen( newPW ) );
	MD5Final( md5_hash, &ctx );

	strcpy( hashBuffer, BinPrintf( md5_hash, sizeof( md5_hash ) ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CContentControlDialog::EnablePassword( const char *newPW )
{
	if ( !newPW[ 0 ] )
	{
		Explain( "#GameUI_MustEnterPassword" );
		return false;
	}

	char digestedPW[ 128 ];
	HashPassword( newPW, digestedPW );

	// disable violence
	engine->Cvar_SetValue( "violence_hblood", 0.0 );
	engine->Cvar_SetValue( "violence_hgibs", 0.0 );
	engine->Cvar_SetValue( "violence_ablood", 0.0 );
	engine->Cvar_SetValue( "violence_agibs", 0.0 );

	// Store digest to registry
	strcpy( m_szGorePW, digestedPW );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CContentControlDialog::DisablePassword( const char *oldPW )
{
	if ( !oldPW[ 0 ] )
	{
		Explain( "#GameUI_MustEnterPassword" );
		return false;
	}

	// Compare the hash
	char digestedPW[ 128 ];
	HashPassword( oldPW, digestedPW );

	if ( stricmp( m_szGorePW, digestedPW ) )
	{
		Explain( "#GameUI_IncorrectPassword" );
		return false;
	}

	m_szGorePW[0] = 0;

	// set the violence cvars
	engine->Cvar_SetValue( "violence_hblood", 1.0 );
	engine->Cvar_SetValue( "violence_hgibs", 1.0 );
	engine->Cvar_SetValue( "violence_ablood", 1.0 );
	engine->Cvar_SetValue( "violence_agibs", 1.0 );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CContentControlDialog::IsPasswordEnabledInDialog()
{
	return m_szGorePW[0] != 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CContentControlDialog::UpdateContentControlStatus( void )
{
	bool enabled = IsPasswordEnabledInDialog();
	m_pStatus->SetText( enabled ? "#GameUI_ContentStatusEnabled" : "#GameUI_ContentStatusDisabled" );

	if ( enabled )
	{
		m_pPasswordLabel->SetText( "#GameUI_PasswordDisablePrompt" );
	}
	else
	{
		m_pPasswordLabel->SetText( "#GameUI_PasswordPrompt" );
	}

	// hide the re-entry
	m_pPassword2Label->SetVisible( !enabled );
	m_pPassword2->SetVisible( !enabled );
}
