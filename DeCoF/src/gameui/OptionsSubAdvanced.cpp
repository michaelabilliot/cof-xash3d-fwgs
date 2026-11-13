//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <string.h>

#include "OptionsSubAdvanced.h"
#include "ContentControlDialog.h"

#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Controls.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubAdvanced::COptionsSubAdvanced( vgui::Panel *parent ) : PropertyPage( parent, NULL )
{
	m_pContentCheckButton = new CheckButton( this, "ContentlockButton", "#GameUI_ContentLock" );
	m_pContentCheckButton->SetCommand( "ContentControl" );

	LoadControlSettings( "Resource\\OptionsSubAdvanced.res" );

	m_pContentControlDialog = new CContentControlDialog( this );
	m_pContentControlDialog->AddActionSignalTarget( this );

	int x, y, ww, wt, wide, tall;
	surface()->GetWorkspaceBounds( x, y, ww, wt );
	m_pContentControlDialog->GetSize( wide, tall );

	// Center it, keeping requested size
	m_pContentControlDialog->SetPos( x + ( ( ww - wide ) / 2 ), y + ( ( wt - tall ) / 2 ) );

	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnCommand( const char *command )
{
	if ( !stricmp( command, "ContentControl" ) )
	{
		if ( !m_pContentCheckButton->IsSelected() && !m_pContentControlDialog->IsPasswordEnabled() )
		{
			// Just disable the checkbox
			m_pContentControlDialog->ResetPassword();
			return;
		}

		OnOpenContentControlDialog();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubAdvanced::~COptionsSubAdvanced()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnResetData()
{
	m_pContentControlDialog->ResetPassword();
	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabled() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnApplyChanges()
{
	m_pContentControlDialog->ApplyPassword();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnContentControlClose()
{
	// Reset check based on new state
	m_pContentCheckButton->SetSelected( m_pContentControlDialog->IsPasswordEnabledInDialog() );

	if ( m_pContentControlDialog->IsPasswordEnabled() != m_pContentControlDialog->IsPasswordEnabledInDialog() )
	{
		PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAdvanced::OnOpenContentControlDialog()
{
	m_pContentControlDialog->Activate();
}
