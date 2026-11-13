//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "NewGameDialog.h"
#include "EngineInterface.h"
#include "ModInfo.h"
#include "Taskbar.h"

#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RadioButton.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CNewGameDialog::CNewGameDialog( vgui2::Panel *parent ) : CTaskFrame( parent, "NewGameDialog" )
{
	SetBounds( 0, 0, 372, 160 );
	SetSizeable( false );

	MakePopup();
	g_pTaskbar->AddTask( GetVPanel() );

	SetTitle( "#GameUI_NewGame", true );

	new Label( this, "HelpText", "#GameUI_NewGameHelpText" );

	m_pTraining = new RadioButton( this, "Training", "#GameUI_TrainingRoom" );
	m_pTraining->SetSelected( true );
	m_nPlayMode = 0;

	m_pEasy = new RadioButton( this, "Easy", "#GameUI_Easy" );
	m_pMedium = new RadioButton( this, "Medium", "#GameUI_Medium" );
	m_pHard = new RadioButton( this, "Hard", "#GameUI_Hard" );

	Button *play = new Button( this, "Play", "#GameUI_Play" );
	play->SetCommand( "Play" );

	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	LoadControlSettings( "Resource\\NewGameDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CNewGameDialog::~CNewGameDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Starts the selected game
//-----------------------------------------------------------------------------
void CNewGameDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Play" ) )
	{
		m_nPlayMode = 0;

		if ( m_pEasy->IsSelected() )
		{
			m_nPlayMode = 1;
		}
		else if ( m_pMedium->IsSelected() )
		{
			m_nPlayMode = 2;
		}
		else if ( m_pHard->IsSelected() )
		{
			m_nPlayMode = 3;
		}

		char mapcommand[512];
		mapcommand[0] = 0;

		if ( m_nPlayMode == 0 )
		{
			sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nmap %s\n", ModInfo().GetTrainMap() );
		}
		else
		{
			sprintf( mapcommand, "disconnect\nmaxplayers 1\ndeathmatch 0\nskill %i\nmap %s\n", m_nPlayMode, ModInfo().GetStartMap() );
		}

		engine->pfnClientCmd( mapcommand );

		if ( !stricmp( ModInfo().GetGameDescription(), "Condition-Zero" ) )
		{
			engine->pfnClientCmd( "cl_newgame 1\n" );
		}

		OnClose();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the dialog on close
//-----------------------------------------------------------------------------
void CNewGameDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}
