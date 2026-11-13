//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "IGameUI.h"
#include "DemoEventsDialog.h"
#include "DemoEditDialog.h"
#include "IEngineWrapper.h"
#include "IObjectContainer.h"
#include "../HLTV/common/IDemoPlayer.h"
#include "../HLTV/common/DirectorCmd.h"
#include "../HLTV/common/hltv.h"

#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDemoEventsDialog::CDemoEventsDialog( vgui::Panel *parent, const char *name, IEngineWrapper *engine, IDemoPlayer *demoPlayer ) : Frame( parent, name )
{
	m_DemoPlayer = demoPlayer;
	m_Engine = engine;

	SetBounds( 0, 0, 464, 380 );
	SetSizeable( false );

	surface()->CreatePopup( GetVPanel(), false, true, false, true, true );

	SetTitle( "#GameUI_EventList", true );

	m_EventList = new ListPanel( this, "EventList" );

	Button *add = new Button( this, "AddButton", "#GameUI_Add" );
	Button *remove = new Button( this, "RemoveButton", "#GameUI_Remove" );
	Button *modify = new Button( this, "ModifyButton", "#GameUI_Modify" );
	Button *goto_ = new Button( this, "GotoButton", "#GameUI_Goto" );

	LoadControlSettings( "Resource/DemoEventsDialog.res" );

	m_EventList->AddColumnHeader( 0, "time", "#GameUI_Time", 32, 0 );
	m_EventList->AddColumnHeader( 1, "type", "#GameUI_Type", 128, 0 );

	m_CurrentCmd = NULL;

	m_EditDialog = new CDemoEditDialog( this, "DemoEditDialog", m_Engine, m_DemoPlayer );
	m_EditDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDemoEventsDialog::~CDemoEventsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: handles the buttons
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnCommand( const char *command )
{
	if ( !m_DemoPlayer || !m_Engine )
	{
		BaseClass::OnCommand( command );
		return;
	}

	GetCurrentCmd();

	if ( !strcmp( command, "goto" ) )
	{
		OnGoto();
	}
	else if ( !strcmp( command, "remove" ) )
	{
		OnRemove();
	}
	else if ( !strcmp( command, "modify" ) )
	{
		OnModify();
	}
	else if ( !strcmp( command, "add" ) )
	{
		OnAdd();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: finds the command of the selected list row
//-----------------------------------------------------------------------------
void CDemoEventsDialog::GetCurrentCmd()
{
	m_CurrentCmd = NULL;

	int itemID = m_EventList->GetSelectedItem( 0 );

	if ( itemID < 0 )
		return;

	int row = m_EventList->GetItemCurrentRow( itemID );
	int index = row + 1;

	IObjectContainer *cmdList = m_DemoPlayer->GetCommands();

	if ( !cmdList )
		return;

	DirectorCmd *cmd = (DirectorCmd *)cmdList->GetFirst();

	while ( cmd )
	{
		if ( cmd->m_Index == index )
		{
			m_CurrentCmd = cmd;
			return;
		}

		cmd = (DirectorCmd *)cmdList->GetNext();
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a camera command at the current view and time
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnAdd()
{
	DirectorCmd newcmd;
	vec3_t origin, angles;

	m_DemoPlayer->SetPaused( true );

	m_Engine->GetViewOrigin( origin );
	m_Engine->GetViewAngles( angles );

	newcmd.m_Type = DRC_CMD_CAMERA;
	newcmd.Clear();
	newcmd.m_Time = m_DemoPlayer->GetWorldTime();
	newcmd.SetCameraData( origin, angles, 90.0f, 0 );
	newcmd.m_Index = -1;

	m_EditDialog->SetCommand( &newcmd );
	m_EditDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: jumps to the time of the selected command
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnGoto()
{
	if ( !m_CurrentCmd )
		return;

	m_DemoPlayer->SetWorldTime( m_CurrentCmd->m_Time, false );
	m_DemoPlayer->ExecuteDirectorCmd( m_CurrentCmd );
	m_DemoPlayer->SetPaused( true );
}

//-----------------------------------------------------------------------------
// Purpose: edits the selected command
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnModify()
{
	if ( !m_CurrentCmd )
		return;

	m_EditDialog->SetCommand( m_CurrentCmd );
	m_EditDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: removes the selected command
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnRemove()
{
	if ( !m_CurrentCmd )
		return;

	m_DemoPlayer->RemoveCommand( m_CurrentCmd->m_Index );
}

//-----------------------------------------------------------------------------
// Purpose: fills the list with the director commands
//-----------------------------------------------------------------------------
void CDemoEventsDialog::OnUpdate()
{
	m_EventList->DeleteAllItems();

	IObjectContainer *cmdList = m_DemoPlayer->GetCommands();

	if ( !cmdList )
		return;

	DirectorCmd *cmd = (DirectorCmd *)cmdList->GetFirst();

	while ( cmd )
	{
		char time[ 32 ];
		char index[ 8 ];

		sprintf( time, "%03u:%02u:%02u", (int)cmd->m_Time / 60, (int)cmd->m_Time % 60, (int)( cmd->m_Time * 100 ) % 100 );
		sprintf( index, "%i", cmd->m_Index );

		KeyValues *entry = new KeyValues( index, "time", time, "type", cmd->ToString() );
		m_EventList->AddItem( entry, 0, false, false );

		cmd = (DirectorCmd *)cmdList->GetNext();
	}

	m_CurrentCmd = m_DemoPlayer->GetLastCommand();

	if ( m_CurrentCmd )
	{
		int itemID = m_EventList->GetItemIDFromRow( m_CurrentCmd->m_Index - 1 );
		m_EventList->SetSingleSelectedItem( itemID );
	}
	else
	{
		int itemID = m_EventList->GetItemIDFromRow( 0 );
		m_EventList->SetSingleSelectedItem( itemID );
	}
}
