//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "IGameUI.h"
#include "DemoPlayerDialog.h"
#include "DemoPlayerFileDialog.h"
#include "DemoEventsDialog.h"
#include "IEngineWrapper.h"
#include "IBaseSystem.h"
#include "../HLTV/common/IDemoPlayer.h"
#include "../HLTV/common/IWorld.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/ToggleButton.h>

using namespace vgui2;

extern IBaseSystem *g_pSystemWrapper;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDemoPlayerDialog::CDemoPlayerDialog( vgui2::Panel *parent ) : Frame( parent, "DemoPlayerDialog" )
{
	Button *button;

	m_World = NULL;
	m_Engine = NULL;
	m_DemoPlayer = NULL;
	m_System = NULL;
	m_Serial = 4001;
	m_SystemTime = 0;

	if ( !LoadModules() )
	{
		SetVisible( false );
		return;
	}

	if ( m_DemoPlayer->IsEditMode() )
	{
		SetBounds( 0, 0, 464, 128 );
	}
	else
	{
		SetBounds( 0, 0, 464, 96 );
	}

	SetSizeable( false );
	SetMoveable( true );

	surface()->CreatePopup( GetVPanel(), false, true, false, true, true );

	SetVisible( true );
	SetTitle( "#GameUI_DemoPlayer", true );

	m_pLableTimeCode = new Label( this, "TimeLabel", "00:00:00" );

	m_pTimeSlider = new Slider( this, "TimeSlider" );
	m_pTimeSlider->SetRange( 0, 60 );
	m_pTimeSlider->AddActionSignalTarget( this );

	m_pButtonLoad = new Button( this, "LoadButton", "" );
	m_pButtonPlay = new Button( this, "PlayButton", "" );
	m_pButtonPause = new Button( this, "PauseButton", "" );
	m_pButtonStepF = new Button( this, "StepFButton", "" );
	m_pButtonStepB = new Button( this, "StepBButton", "" );
	m_pButtonStart = new Button( this, "StartButton", "" );
	m_pButtonEnd = new Button( this, "EndButton", "" );
	m_pButtonSlower = new Button( this, "SlowerButton", "" );
	m_pButtonFaster = new Button( this, "FasterButton", "" );
	m_pButtonStop = new Button( this, "StopButton", "" );

	m_MasterButton = new ToggleButton( this, "MasterButton", "Master" );
	m_MasterButton->AddActionSignalTarget( this );

	button = new Button( this, "EventsButton", "#GameUI_Events" );
	button = new Button( this, "SaveButton", "#GameUI_Save" );

	m_NextTimeScale = 2.0f;
	m_lastSliderTime = -1;

	LoadControlSettings( "Resource\\DemoPlayerDialog.res" );

	m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
	m_hDemoEventsDialog->AddActionSignalTarget( this );

	m_hDemoPlayerFileDialog = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDemoPlayerDialog::~CDemoPlayerDialog()
{
	if ( m_DemoPlayer )
	{
		m_DemoPlayer->RemoveListener( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads the button images
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pButtonLoad->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_load", false ), 0 );
	m_pButtonPlay->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_play", false ), 0 );
	m_pButtonPause->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_pause", false ), 0 );
	m_pButtonStepF->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_stepf", false ), 0 );
	m_pButtonStepB->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_stepb", false ), 0 );
	m_pButtonStart->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_start", false ), 0 );
	m_pButtonEnd->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_end", false ), 0 );
	m_pButtonStop->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_stop", false ), 0 );
	m_pButtonFaster->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_faster", false ), 0 );
	m_pButtonSlower->SetImageAtIndex( 0, scheme()->GetImage( "Resource\\icon_slower", false ), 0 );
}

//-----------------------------------------------------------------------------
// Purpose: updates the controls while a demo plays
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnThink()
{
	BaseClass::OnThink();

	if ( !m_DemoPlayer )
		return;

	if ( !m_DemoPlayer->IsActive() )
		return;

	Update();
}

//-----------------------------------------------------------------------------
// Purpose: updates the title, the time slider and the status line
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::Update()
{
	if ( !m_DemoPlayer )
		return;

	if ( !m_DemoPlayer->IsActive() )
	{
		SetTitle( "#GameUI_DemoPlayer", false );
	}
	else if ( m_DemoPlayer->IsLoading() )
	{
		wchar_t title[ 256 ];
		wchar_t filename[ 128 ];

		localize()->ConvertANSIToUnicode( m_DemoPlayer->GetFileName(), filename, sizeof( filename ) );
		localize()->ConstructString( title, sizeof( title ), localize()->Find( "#GameUI_LoadingFilename" ), 1, filename );
		SetTitle( title, false );
	}
	else
	{
		wchar_t title[ 128 ];

		localize()->ConvertANSIToUnicode( m_DemoPlayer->GetFileName(), title, sizeof( title ) );
		SetTitle( title, false );
	}

	frame_t *firstFrame = m_World->GetFirstFrame();
	frame_t *lastFrame = m_World->GetLastFrame();

	if ( firstFrame && lastFrame )
	{
		float range = lastFrame->time - firstFrame->time;
		m_pTimeSlider->SetRange( (int)firstFrame->time, (int)lastFrame->time );
		m_pTimeSlider->InvalidateLayout();
	}

	double worldTime = m_DemoPlayer->GetWorldTime();
	float timeScale = m_DemoPlayer->GetTimeScale();

	int sliderTime = m_pTimeSlider->GetValue();

	if ( sliderTime != m_lastSliderTime )
	{
		m_DemoPlayer->SetWorldTime( sliderTime, false );

		if ( m_lastSliderTime > 0 )
		{
			OnPause();
		}

		m_lastSliderTime = sliderTime;
	}
	else
	{
		m_lastSliderTime = (int)worldTime;
		m_pTimeSlider->SetValue( m_lastSliderTime );
	}

	char szTimeCode[ 32 ];
	wchar_t wTimeCode[ 32 ];
	wchar_t wTitle[ 128 ];

	wchar_t *wTemp = NULL;
	wchar_t *wTimeScaleString = NULL;
	wchar_t *wDescription = localize()->Find( "#Valve_Playing" );

	if ( !m_DemoPlayer->IsActive() )
	{
		wDescription = localize()->Find( "#Valve_Stopped" );
	}
	else if ( m_DemoPlayer->IsPaused() )
	{
		wDescription = localize()->Find( "#Valve_Paused" );
	}

	if ( timeScale == 0.25f )
	{
		wTimeScaleString = localize()->Find( "#Valve_x_1_4" );
	}
	else if ( timeScale == 0.5f )
	{
		wTimeScaleString = localize()->Find( "#Valve_x_1_2" );
	}
	else if ( timeScale == 1.0f )
	{
		wTimeScaleString = localize()->Find( "#Valve_x_1" );
	}
	else if ( timeScale == 2.0f )
	{
		wTimeScaleString = localize()->Find( "#Valve_x_2" );
	}
	else if ( timeScale == 4.0f )
	{
		wTimeScaleString = localize()->Find( "#Valve_x_4" );
	}

	_snprintf( szTimeCode, sizeof( szTimeCode ) - 1, "%02u:%02u:%02u", (int)worldTime / 60, (int)worldTime % 60, (int)( worldTime * 100 ) % 100 );
	szTimeCode[ sizeof( szTimeCode ) - 1 ] = 0;

	localize()->ConvertANSIToUnicode( szTimeCode, wTimeCode, sizeof( wTimeCode ) );

	wTemp = localize()->Find( "#Valve_ViewDemoStatus" );
	localize()->ConstructString( wTitle, sizeof( wTitle ), wTemp, 3, wTimeCode, wTimeScaleString, wDescription );

	m_pLableTimeCode->SetText( wTitle );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnPlay()
{
	m_DemoPlayer->SetPaused( false );
	m_Engine->SetCvar( "spec_autodirector", "1" );
}

//-----------------------------------------------------------------------------
// Purpose: gets the engine, demo player and world modules
//-----------------------------------------------------------------------------
bool CDemoPlayerDialog::LoadModules()
{
	m_System = g_pSystemWrapper;

	if ( !m_System )
		return false;

	m_Engine = dynamic_cast< IEngineWrapper * >( m_System->GetModule( ENGINEWRAPPER_INTERFACE_VERSION, "" ) );

	if ( !m_Engine )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't get engine interface.\n" );
		return false;
	}

	m_DemoPlayer = dynamic_cast< IDemoPlayer * >( m_System->GetModule( DEMOPLAYER_INTERFACE_VERSION, "" ) );

	if ( !m_DemoPlayer )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't load demo player module.\n" );
		return false;
	}

	m_DemoPlayer->RegisterListener( this );

	m_World = m_DemoPlayer->GetWorld();

	if ( !m_World )
	{
		m_System->Printf( "CDemoPlayerDialog::LoadModules: couldn't get world module.\n" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnPause()
{
	m_DemoPlayer->SetPaused( true );
	m_Engine->Cbuf_AddText( "stopsound\n" );
}

//-----------------------------------------------------------------------------
// Purpose: steps one frame forward or backward
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnNextFrame( int direction )
{
	float time = m_DemoPlayer->GetWorldTime();

	frame_t *frame = m_World->GetFrameByTime( time );

	if ( !frame )
		return;

	frame = m_World->GetFrameBySeqNr( frame->seqnr + direction );

	if ( !frame )
		return;

	m_DemoPlayer->SetWorldTime( frame->time, false );
	m_DemoPlayer->SetPaused( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnStart()
{
	frame_t *first = m_World->GetFirstFrame();

	if ( !first )
		return;

	m_DemoPlayer->SetWorldTime( first->time - 0.01f, false );
	m_DemoPlayer->SetTimeScale( 1.0f );
	m_DemoPlayer->SetPaused( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnSlower()
{
	float timeScale = m_DemoPlayer->GetTimeScale();

	if ( timeScale <= 0.25f )
		return;

	timeScale /= 2.0f;
	m_DemoPlayer->SetTimeScale( timeScale );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnSave()
{
	m_DemoPlayer->SaveGame( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: shows the director command list
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnEvents()
{
	if ( !m_hDemoEventsDialog.Get() )
	{
		m_hDemoEventsDialog = new CDemoEventsDialog( this, "DemoEventsDialog", m_Engine, m_DemoPlayer );
		m_hDemoEventsDialog->AddActionSignalTarget( this );
	}

	m_hDemoEventsDialog->Activate();

	PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateCmdList" ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnFaster()
{
	float timeScale = m_DemoPlayer->GetTimeScale();

	if ( timeScale >= 4.0 )
		return;

	timeScale *= 2.0f;
	m_DemoPlayer->SetTimeScale( timeScale );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnEnd()
{
	frame_t *last = m_World->GetLastFrame();

	if ( !last )
		return;

	m_DemoPlayer->SetWorldTime( last->time, false );
	OnPause();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnClose()
{
	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: handles the buttons
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnCommand( const char *command )
{
	if ( !m_DemoPlayer || !m_World || !m_Engine )
	{
		BaseClass::OnCommand( command );
		return;
	}

	if ( !strcmp( command, "pause" ) )
	{
		OnPause();
	}
	else if ( !strcmp( command, "load" ) )
	{
		OnLoad();
	}
	else if ( !strcmp( command, "play" ) )
	{
		OnPlay();
	}
	else if ( !strcmp( command, "stepf" ) )
	{
		OnNextFrame( 1 );
	}
	else if ( !strcmp( command, "stepb" ) )
	{
		OnNextFrame( -1 );
	}
	else if ( !strcmp( command, "start" ) )
	{
		OnStart();
	}
	else if ( !strcmp( command, "end" ) )
	{
		OnEnd();
	}
	else if ( !strcmp( command, "slower" ) )
	{
		OnSlower();
	}
	else if ( !strcmp( command, "faster" ) )
	{
		OnFaster();
	}
	else if ( !strcmp( command, "load" ) )
	{
		OnLoad();
	}
	else if ( !strcmp( command, "stop" ) )
	{
		OnStop();
	}
	else if ( !strcmp( command, "events" ) )
	{
		OnEvents();
	}
	else if ( !strcmp( command, "save" ) )
	{
		OnSave();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: signals of the demo player module
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::ReceiveSignal( ISystemModule *module, unsigned int signal, void *data )
{
	if ( !m_DemoPlayer )
		return;

	if ( module->GetSerial() == m_DemoPlayer->GetSerial() )
	{
		switch ( signal )
		{
		case 1:
			if ( m_DemoPlayer->IsEditMode() )
			{
				PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateCmdList" ) );
			}
			break;

		case 2:
			if ( m_DemoPlayer->IsEditMode() )
			{
				PostMessage( m_hDemoEventsDialog->GetVPanel(), new KeyValues( "UpdateLastCmd" ) );
			}
			break;

		default:
			m_System->Printf( "CDemoPlayerDialog::ReceiveSignal: unknown signal %i.\n", signal );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: messages from the file dialog and the master button
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnMessage( const KeyValues *params, VPANEL fromPanel )
{
	BaseClass::OnMessage( params, fromPanel );

	if ( !m_DemoPlayer )
		return;

	if ( !strcmp( params->GetName(), "DemoSelected" ) )
	{
		char cmd[ 270 ];
		sprintf( cmd, "viewdemo %s\n", ( (KeyValues *)params )->GetString( "demoname", "" ) );
		m_DemoPlayer->Stop();
		m_Engine->Cbuf_AddText( cmd );
	}

	if ( !strcmp( params->GetName(), "ButtonToggled" ) )
	{
		m_DemoPlayer->SetMasterMode( ( (KeyValues *)params )->GetInt( "state", 0 ) != 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnStop()
{
	m_DemoPlayer->Stop();
	m_Engine->Cbuf_AddText( "stopdemo\n" );
	Update();
}

//-----------------------------------------------------------------------------
// Purpose: opens the demo file dialog
//-----------------------------------------------------------------------------
void CDemoPlayerDialog::OnLoad()
{
	if ( !m_hDemoPlayerFileDialog.Get() )
	{
		m_hDemoPlayerFileDialog = new CDemoPlayerFileDialog( this, "DemoPlayerFileDialog" );
		m_hDemoPlayerFileDialog->AddActionSignalTarget( this );
	}

	m_hDemoPlayerFileDialog->Activate();
}
