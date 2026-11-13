//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "OptionsDialog.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "MultiplayerAdvancedDialog.h"
#include "Taskbar.h"

#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog( vgui::Panel *parent ) : PropertyDialog( parent, "OptionsDialog" )
{
	SetBounds( 0, 0, 512, 406 );
	SetSizeable( false );

	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_Options", true );

	AddPage( new COptionsSubKeyboard( this ), "#GameUI_Keyboard" );
	AddPage( new COptionsSubMouse( this ), "#GameUI_Mouse" );
	AddPage( new COptionsSubAudio( this ), "#GameUI_Audio" );
	AddPage( new COptionsSubVideo( this ), "#GameUI_Video" );
	AddPage( new COptionsSubVoice( this ), "#GameUI_Voice" );
	AddPage( new CMultiplayerAdvancedPage( this ), "#GameUI_AdvancedNoEllipsis" );

	SetApplyButtonVisible( true );
	GetPropertySheet()->SetTabWidth( 72 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: sets the title in the dialog and in the taskbar
//-----------------------------------------------------------------------------
void COptionsDialog::SetTitle( const char *title, bool surfaceTitle )
{
	BaseClass::SetTitle( title, surfaceTitle );

	if ( g_pTaskbar )
	{
		wchar_t *wTitle;
		wchar_t w_szTitle[ 1024 ];

		wTitle = localize()->Find( title );

		if ( !wTitle )
		{
			localize()->ConvertANSIToUnicode( title, w_szTitle, sizeof( w_szTitle ) );
			wTitle = w_szTitle;
		}

		g_pTaskbar->SetTitle( GetVPanel(), wTitle );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
	BaseClass::Activate();
	EnableApplyButton( false );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle( "#GameUI_Options", true );
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void COptionsDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
