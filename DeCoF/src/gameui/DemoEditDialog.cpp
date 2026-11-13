//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "IGameUI.h"
#include "DemoEditDialog.h"
#include "LabeledCommandComboBox.h"
#include "IEngineWrapper.h"
#include "../HLTV/common/IDemoPlayer.h"
#include "../HLTV/common/hltv.h"

#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui;

// HLTV/common/common.cpp
extern char *COM_VarArgs( char *format, ... );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDemoEditDialog::CDemoEditDialog( vgui::Panel *parent, const char *name, IEngineWrapper *engine, IDemoPlayer *demoPlayer ) : Frame( parent, name )
{
	int i;
	char fieldName[ 16 ];

	m_DemoPlayer = demoPlayer;
	m_Engine = engine;
	m_ChangingData = false;

	SetBounds( 0, 0, 336, 380 );
	SetSizeable( false );

	surface()->CreatePopup( GetVPanel(), false, true, false, true, true );

	SetTitle( "#GameUI_EditDirectorCommand", true );

	m_TypeBox = new CLabeledCommandComboBox( this, "TypeBox" );

	for ( i = DRC_CMD_CAMERA; i < DRC_CMD_LAST; i++ )
	{
		m_TypeBox->AddItem( DRC_CMD_NAME[ i ], "" );
	}

	m_TypeBox->SetInitialItem( 0 );

	m_TimeStamp = new TextEntry( this, "TimeEntry" );

	Button *ok = new Button( this, "OkButton", "#GameUI_OK" );
	Button *cancel = new Button( this, "CancelButton", "#GameUI_Cancel" );
	Button *gettime = new Button( this, "GetTimeButton", "#GameUI_GetTime" );
	m_GetViewButton = new Button( this, "GetViewButton", "#GameUI_GetView" );

	int offset = 96;

	for ( i = 0; i < MAX_DATA_FIELDS; i++ )
	{
		sprintf( fieldName, "DataLabel%i", i );
		m_DataLabels[ i ] = new Label( this, fieldName, "" );
		m_DataLabels[ i ]->SetBounds( 10, offset, 96, 24 );

		sprintf( fieldName, "DataEntry%i", i );
		m_DataEntries[ i ] = new TextEntry( this, fieldName );
		m_DataEntries[ i ]->SetBounds( 104, offset, 224, 24 );

		offset += 30;
	}

	LoadControlSettings( "Resource/DemoEditDialog.res" );

	m_DrcCmd.Clear();

	OnUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CDemoEditDialog::~CDemoEditDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: shows an integer field
//-----------------------------------------------------------------------------
void CDemoEditDialog::SetDataFieldi( int index, char *label, int i )
{
	m_DataLabels[ index ]->SetText( label );
	m_DataLabels[ index ]->SetVisible( true );

	m_DataEntries[ index ]->SetText( COM_VarArgs( "%d", i ) );
	m_DataEntries[ index ]->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: shows a float field
//-----------------------------------------------------------------------------
void CDemoEditDialog::SetDataFieldf( int index, char *label, float f )
{
	m_DataLabels[ index ]->SetText( label );
	m_DataLabels[ index ]->SetVisible( true );

	m_DataEntries[ index ]->SetText( COM_VarArgs( "%.1f", f ) );
	m_DataEntries[ index ]->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: shows a text field
//-----------------------------------------------------------------------------
void CDemoEditDialog::SetDataField( int index, char *label, char *text )
{
	m_DataLabels[ index ]->SetText( label );
	m_DataLabels[ index ]->SetVisible( true );

	m_DataEntries[ index ]->SetText( text );
	m_DataEntries[ index ]->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CDemoEditDialog::GetDataFieldi( int index )
{
	int i;
	char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );
	sscanf( sz, "%d", &i );

	return i;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CDemoEditDialog::GetDataFieldf( int index )
{
	float f;
	char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );
	sscanf( sz, "%f", &f );

	return f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoEditDialog::GetDataField2f( int index, float *f1, float *f2 )
{
	char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );
	sscanf( sz, "%f %f", f1, f2 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CDemoEditDialog::GetDataField3f( int index, float *f1, float *f2, float *f3 )
{
	char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );
	sscanf( sz, "%f %f %f", f1, f2, f3 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CDemoEditDialog::GetDataFieldx( int index )
{
	int i;
	char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );
	sscanf( sz, "%x", &i );

	return i;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
char *CDemoEditDialog::GetDataFields( int index )
{
	static char sz[ 1024 ];

	m_DataEntries[ index ]->GetText( sz, sizeof( sz ) );

	return sz;
}

//-----------------------------------------------------------------------------
// Purpose: stores the command in the demo
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnOK()
{
	if ( !m_DemoPlayer )
	{
		BaseClass::OnCommand( "Close" );
		return;
	}

	UpdateDrcCmd();

	if ( m_DrcCmd.m_Index > 0 )
	{
		m_DemoPlayer->RemoveCommand( m_DrcCmd.m_Index );
	}

	m_DemoPlayer->AddCommand( &m_DrcCmd );

	BaseClass::OnCommand( "Close" );
}

//-----------------------------------------------------------------------------
// Purpose: takes the camera position from the current view
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnGetView()
{
	if ( !m_Engine )
		return;

	vec3_t origin, angles;
	float fov;
	int i;

	if ( m_DrcCmd.GetType() == DRC_CMD_CAMERA )
	{
		m_DrcCmd.GetCameraData( origin, angles, fov, i );
	}
	else if ( m_DrcCmd.GetType() == DRC_CMD_CAMPATH )
	{
		m_DrcCmd.GetCamPathData( origin, angles, fov, i );
	}
	else
	{
		return;
	}

	m_Engine->GetViewOrigin( origin );
	m_Engine->GetViewAngles( angles );

	if ( m_DrcCmd.GetType() == DRC_CMD_CAMERA )
	{
		m_DrcCmd.SetCameraData( origin, angles, fov, i );
	}
	else if ( m_DrcCmd.GetType() == DRC_CMD_CAMPATH )
	{
		m_DrcCmd.SetCamPathData( origin, angles, fov, i );
	}

	OnUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: takes the time of the demo
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnGetTime()
{
	if ( !m_DemoPlayer )
		return;

	m_DrcCmd.SetTime( m_DemoPlayer->GetWorldTime() );

	OnUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: reads the fields into the command
//-----------------------------------------------------------------------------
void CDemoEditDialog::UpdateDrcCmd()
{
	char *t;
	char sz[ 1204 ];
	int i1, i2, i3;
	float f1, f2, f3, f4;
	vec3_t v1, v2;

	m_TimeStamp->GetText( sz, sizeof( sz ) );

	t = sz;
	while ( *t )
	{
		if ( *t == ':' )
			*t = ' ';
		t++;
	}

	sscanf( sz, "%d %d %d", &i1, &i2, &i3 );
	m_DrcCmd.SetTime( i1 * 60 + i2 + i3 / 100.0f );

	switch ( m_DrcCmd.GetType() )
	{
	case DRC_CMD_EVENT:
		i1 = GetDataFieldi( 0 );
		i2 = GetDataFieldi( 1 );
		i3 = GetDataFieldi( 2 );
		m_DrcCmd.SetEventData( i1, i2, i3 );
		break;

	case DRC_CMD_MODE:
		i1 = GetDataFieldi( 0 );
		m_DrcCmd.SetModeData( i1 );
		break;

	case DRC_CMD_CHASE:
		i1 = GetDataFieldi( 0 );
		i2 = GetDataFieldi( 1 );
		f1 = GetDataFieldf( 2 );
		i3 = GetDataFieldi( 3 );
		m_DrcCmd.SetChaseData( i1, i2, f1, i3 );
		break;

	case DRC_CMD_INEYE:
		i1 = GetDataFieldi( 0 );
		m_DrcCmd.SetInEyeData( i1 );
		break;

	case DRC_CMD_MAP:
		i1 = GetDataFieldi( 0 );
		f1 = GetDataFieldf( 1 );
		f2 = GetDataFieldf( 2 );
		m_DrcCmd.SetMapData( i1, f1, f2 );
		break;

	case DRC_CMD_CAMERA:
		GetDataField3f( 0, &v1[0], &v1[1], &v1[2] );
		GetDataField3f( 1, &v2[0], &v2[1], &v2[2] );
		f1 = GetDataFieldf( 2 );
		i1 = GetDataFieldi( 3 );
		m_DrcCmd.SetCameraData( v1, v2, f1, i1 );
		break;

	case DRC_CMD_CAMPATH:
		GetDataField3f( 0, &v1[0], &v1[1], &v1[2] );
		GetDataField3f( 1, &v2[0], &v2[1], &v2[2] );
		f1 = GetDataFieldf( 2 );
		i1 = GetDataFieldi( 3 );
		m_DrcCmd.SetCamPathData( v1, v2, f1, i1 );
		break;

	case DRC_CMD_TIMESCALE:
		f1 = GetDataFieldf( 0 );
		m_DrcCmd.SetTimeScaleData( f1 );
		break;

	case DRC_CMD_MESSAGE:
		i1 = GetDataFieldi( 0 );
		i2 = GetDataFieldx( 1 );
		GetDataField2f( 2, &v1[0], &v1[1] );
		f1 = GetDataFieldf( 3 );
		f2 = GetDataFieldf( 4 );
		f3 = GetDataFieldf( 5 );
		f4 = GetDataFieldf( 6 );
		t = GetDataFields( 7 );
		m_DrcCmd.SetMessageData( i1, i2, v1, f1, f2, f3, f4, t );
		break;

	case DRC_CMD_SOUND:
		t = GetDataFields( 0 );
		f1 = GetDataFieldf( 1 );
		m_DrcCmd.SetSoundData( t, f1 );
		break;

	case DRC_CMD_STATUS:
		m_DrcCmd.SetStatusData( 0, 0, 0 );
		break;

	case DRC_CMD_BANNER:
		t = GetDataFields( 0 );
		m_DrcCmd.SetBannerData( t );
		break;

	case DRC_CMD_STUFFTEXT:
		t = GetDataFields( 0 );
		m_DrcCmd.SetStuffTextData( t );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the command in the fields
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnUpdate()
{
	char t[ 1024 ];
	int i1, i2, i3;
	float f1, f2, f3, f4;
	vec3_t v1, v2;

	m_ChangingData = true;

	m_GetViewButton->SetVisible( false );

	ClearDataFields();

	int type = m_DrcCmd.GetType();
	float time = m_DrcCmd.GetTime();
	int itime = (int)time;

	if ( type <= DRC_CMD_NONE || type > DRC_CMD_LAST )
	{
		m_ChangingData = false;
		return;
	}

	m_TypeBox->SetInitialItem( type - DRC_CMD_CAMERA );

	sprintf( t, "%03u:%02u:%02u", itime / 60, itime % 60, (int)( time * 100 ) % 100 );
	m_TimeStamp->SetText( t );

	switch ( type )
	{
	case DRC_CMD_EVENT:
		m_DrcCmd.GetEventData( i1, i2, i3 );
		SetDataFieldi( 0, "Target 1", i1 );
		SetDataFieldi( 1, "Target 2", i2 );
		SetDataFieldi( 2, "Flags", i3 );
		break;

	case DRC_CMD_MODE:
		m_DrcCmd.GetModeData( i1 );
		SetDataField( 0, "View Mode", COM_VarArgs( "%i", i1 ) );
		break;

	case DRC_CMD_CHASE:
		m_DrcCmd.GetChaseData( i1, i2, f1, i3 );
		SetDataFieldi( 0, "Target 1", i1 );
		SetDataFieldi( 1, "Target 2", i2 );
		SetDataFieldf( 2, "Distance", f1 );
		SetDataFieldi( 3, "Flags", i3 );
		break;

	case DRC_CMD_INEYE:
		m_DrcCmd.GetInEyeData( i1 );
		SetDataFieldi( 0, "Player", i1 );
		break;

	case DRC_CMD_MAP:
		m_DrcCmd.GetMapData( i1, f1, f2 );
		SetDataFieldi( 0, "Player", i1 );
		SetDataFieldf( 1, "Angle", f1 );
		SetDataFieldf( 2, "Distance", f2 );
		break;

	case DRC_CMD_CAMERA:
		m_DrcCmd.GetCameraData( v1, v2, f1, i1 );
		SetDataField( 0, "Position", COM_VarArgs( "%.1f %.1f %.1f", v1[0], v1[1], v1[2] ) );
		SetDataField( 1, "View Angle", COM_VarArgs( "%.1f %.1f %.1f", v2[0], v2[1], v2[2] ) );
		SetDataFieldf( 2, "FOV", f1 );
		SetDataFieldi( 3, "Target", i1 );
		m_GetViewButton->SetVisible( true );
		break;

	case DRC_CMD_CAMPATH:
		m_DrcCmd.GetCamPathData( v1, v2, f1, i1 );
		SetDataField( 0, "Position", COM_VarArgs( "%.1f %.1f %.1f", v1[0], v1[1], v1[2] ) );
		SetDataField( 1, "View Angle", COM_VarArgs( "%.1f %.1f %.1f", v2[0], v2[1], v2[2] ) );
		SetDataFieldf( 2, "FOV", f1 );
		SetDataFieldi( 3, "Flags", i1 );
		m_GetViewButton->SetVisible( true );
		break;

	case DRC_CMD_TIMESCALE:
		m_DrcCmd.GetTimeScaleData( f1 );
		SetDataField( 0, "Time Scale", COM_VarArgs( "%.2f", f1 ) );
		break;

	case DRC_CMD_MESSAGE:
		m_DrcCmd.GetMessageData( i1, i2, v1, f1, f2, f3, f4, t );
		SetDataFieldi( 0, "Effect", i1 );
		SetDataField( 1, "Color", COM_VarArgs( "%x", i2 ) );
		SetDataField( 2, "Position", COM_VarArgs( "%.2f %.2f", v1[0], v1[1] ) );
		SetDataFieldf( 3, "Fade In", f1 );
		SetDataFieldf( 4, "Fade Out", f2 );
		SetDataFieldf( 5, "Hold Time", f3 );
		SetDataFieldf( 6, "FX Time", f4 );
		SetDataField( 7, "Text", t );
		break;

	case DRC_CMD_SOUND:
		m_DrcCmd.GetSoundData( t, f1 );
		SetDataField( 0, "Name", t );
		SetDataField( 1, "Volume", COM_VarArgs( "%.2f", f1 ) );
		break;

	case DRC_CMD_STATUS:
		m_DrcCmd.GetStatusData( i1, i2, i3 );
		break;

	case DRC_CMD_BANNER:
		m_DrcCmd.GetBannerData( t );
		SetDataField( 0, "File", t );
		break;

	case DRC_CMD_STUFFTEXT:
		m_DrcCmd.GetStuffTextData( t );
		SetDataField( 0, "Command", t );
		break;
	}

	m_ChangingData = false;
}

//-----------------------------------------------------------------------------
// Purpose: hides all data fields
//-----------------------------------------------------------------------------
void CDemoEditDialog::ClearDataFields()
{
	for ( int i = 0; i < MAX_DATA_FIELDS; i++ )
	{
		m_DataLabels[ i ]->SetText( "" );
		m_DataLabels[ i ]->SetVisible( false );
		m_DataEntries[ i ]->SetText( "" );
		m_DataEntries[ i ]->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: edits a copy of the command
//-----------------------------------------------------------------------------
void CDemoEditDialog::SetCommand( DirectorCmd *cmd )
{
	m_DrcCmd.Copy( cmd );
	OnUpdate();
}

//-----------------------------------------------------------------------------
// Purpose: handles the buttons
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnCommand( const char *command )
{
	if ( !strcmp( command, "ok" ) )
	{
		OnOK();
	}
	else if ( !strcmp( command, "getview" ) )
	{
		OnGetView();
	}
	else if ( !strcmp( command, "gettime" ) )
	{
		OnGetTime();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: starts a new command of the selected type with default data
//-----------------------------------------------------------------------------
void CDemoEditDialog::OnTypeChanged()
{
	if ( m_ChangingData )
		return;

	float time = m_DrcCmd.m_Time;
	int index = m_DrcCmd.m_Index;

	m_DrcCmd.Clear();

	m_DrcCmd.m_Time = time;
	m_DrcCmd.m_Index = index;

	vec3_t v;
	char type[ 256 ];

	m_TypeBox->GetText( type, sizeof( type ) );

	for ( int i = 0; i < DRC_CMD_LAST; i++ )
	{
		if ( !strcmp( type, DRC_CMD_NAME[ i ] ) )
		{
			if ( m_DrcCmd.m_Type == i )
				return;

			m_DrcCmd.m_Type = i;
			break;
		}
	}

	switch ( m_DrcCmd.m_Type )
	{
	case DRC_CMD_EVENT:
		m_DrcCmd.SetEventData( 1, 0, 0 );
		break;

	case DRC_CMD_MODE:
		m_DrcCmd.SetModeData( 4 );
		break;

	case DRC_CMD_CHASE:
		m_DrcCmd.SetChaseData( 1, 0, 96.0f, 0 );
		break;

	case DRC_CMD_INEYE:
		m_DrcCmd.SetInEyeData( 1 );
		break;

	case DRC_CMD_MAP:
		m_DrcCmd.SetMapData( 1, 90.0f, 128.0f );
		break;

	case DRC_CMD_CAMERA:
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
		m_DrcCmd.SetCameraData( v, v, 90.0f, 0 );
		break;

	case DRC_CMD_CAMPATH:
		v[0] = 0;
		v[1] = 0;
		v[2] = 0;
		m_DrcCmd.SetCamPathData( v, v, 90.0f, 0 );
		break;

	case DRC_CMD_TIMESCALE:
		m_DrcCmd.SetTimeScaleData( 1.0f );
		break;

	case DRC_CMD_MESSAGE:
		v[0] = 0.5f;
		v[1] = 0.9f;
		v[2] = 0;
		m_DrcCmd.SetMessageData( 0, 255, v, 1.0f, 1.0f, 5.0f, 0.0f, "Add your message here" );
		break;

	case DRC_CMD_SOUND:
		m_DrcCmd.SetSoundData( "barney/heybuddy.wav", 1.0f );
		break;

	case DRC_CMD_STATUS:
		m_DrcCmd.SetStatusData( 0, 0, 0 );
		break;

	case DRC_CMD_BANNER:
		m_DrcCmd.SetBannerData( "gfx/temp/file.tga" );
		break;

	case DRC_CMD_STUFFTEXT:
		m_DrcCmd.SetStuffTextData( "echo Enter command here" );
		break;
	}

	OnUpdate();
}
