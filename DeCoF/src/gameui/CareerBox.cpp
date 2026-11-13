//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Career dialog boxes (queries, difficulty selection, new profile)
//
// $NoKeywords: $
//=============================================================================

#pragma warning( disable : 4530 )	// GameUI is built without exception handling

#include <stdio.h>
#include <string.h>

#include "CareerBox.h"
#include "CareerBaseFrame.h"
#include "CareerGame.h"
#include "CareerProfile.h"
#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"
#include "../game_shared/shared_util.h"

#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerBaseBox::CCareerBaseBox( Panel *parent, const char *panelName, bool loadResources ) : Frame( parent, panelName, false )
{
	SetScheme( "CareerBoxScheme" );
	SetProportional( true );
	SetMoveable( false );
	SetSizeable( false );

	m_bgColor = Color( 0, 0, 0, 0 );

	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/career/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/career/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/career/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/career/round_corner_se" );

	m_pTextLabel = new Label( this, "TextLabel", "" );

	m_pOkButton = new CCareerButton( this, "OkButton", "", "", false );
	m_pCancelButton = new CCareerButton( this, "CancelButton", "", "", false );
	m_pOkButton->SetVisible( false );

	m_buttons.PutElement( m_pOkButton );
	m_buttons.PutElement( m_pCancelButton );

	m_cancelFocus = false;

	if ( loadResources )
	{
		char *buf = new char[ strlen( panelName ) + 14 ];
		strcpy( buf, "resource/" );
		strcat( buf, panelName );
		strcat( buf, ".res" );
		LoadControlSettings( buf );
		delete[] buf;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gives the cancel button the focus when the box is shown
//-----------------------------------------------------------------------------
void CCareerBaseBox::SetCancelButtonAsDefault()
{
	m_cancelFocus = true;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of the box
//-----------------------------------------------------------------------------
void CCareerBaseBox::SetLabelText( const wchar_t *text )
{
	m_pTextLabel->SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: Button sounds, background color and corner images
//-----------------------------------------------------------------------------
void CCareerBaseBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	for ( int i=0; i<m_buttons.GetCount(); ++i )
	{
		m_buttons[i]->SetArmedSound( "sound/UI/buttonrollover.wav" );
		m_buttons[i]->SetDepressedSound( "sound/UI/buttonclick.wav" );
		m_buttons[i]->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	}

	m_bgColor = GetSchemeColor( "CareerBoxBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );

	int r, g, b, a;
	m_bgColor.GetColor( r, g, b, a );
	Color imageColor = Color( 255, 255, 255, a );
	m_pTopLeftPanel->setImageColor( imageColor );
	m_pTopRightPanel->setImageColor( imageColor );
	m_pBottomLeftPanel->setImageColor( imageColor );
	m_pBottomRightPanel->setImageColor( imageColor );

	SetBorder( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Keeps the box centered and places the corner images
//-----------------------------------------------------------------------------
void CCareerBaseBox::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	if ( x + w/2 != screenWide/2 )
	{
		SetPos( screenWide/2 - w/2, screenTall/2 - h/2 );
		GetBounds( x, y, w, h );
	}

	const int cornerSize = 10;
	m_pTopLeftPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	m_pTopRightPanel->SetBounds( w - cornerSize, 0, cornerSize, cornerSize );
	m_pBottomLeftPanel->SetBounds( 0, h - cornerSize, cornerSize, cornerSize );
	m_pBottomRightPanel->SetBounds( w - cornerSize, h - cornerSize, cornerSize, cornerSize );
}

//-----------------------------------------------------------------------------
// Purpose: Fills the box between the rounded corners
//-----------------------------------------------------------------------------
void CCareerBaseBox::PaintBackground()
{
	surface()->DrawSetColor( m_bgColor );

	int x1, x2, y1, y2;

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomLeftPanel->GetPos( x2, y2 );
	y1 += m_pTopLeftPanel->GetTall();
	x2 += m_pBottomLeftPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopLeftPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	x1 += m_pTopLeftPanel->GetWide();
	y2 += m_pBottomRightPanel->GetTall();
	surface()->DrawFilledRect( x1, y1, x2, y2 );

	m_pTopRightPanel->GetPos( x1, y1 );
	m_pBottomRightPanel->GetPos( x2, y2 );
	y1 += m_pTopRightPanel->GetTall();
	x2 += m_pBottomRightPanel->GetWide();
	surface()->DrawFilledRect( x1, y1, x2, y2 );
}

//-----------------------------------------------------------------------------
// Purpose: Shows the box and focuses a button
//-----------------------------------------------------------------------------
void CCareerBaseBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();

	if ( m_pCancelButton->IsVisible() && m_cancelFocus )
	{
		m_pCancelButton->RequestFocus();
	}
	else if ( m_pOkButton->IsVisible() )
	{
		m_pOkButton->RequestFocus();
	}
	else
	{
		RequestFocus();
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Shows the box as a modal window
//-----------------------------------------------------------------------------
void CCareerBaseBox::DoModal()
{
	ShowWindow();
	input()->SetAppModalSurface( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: Escape cancels, enter presses the focused button
//-----------------------------------------------------------------------------
void CCareerBaseBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
	}
	else if ( code == KEY_ENTER )
	{
		BaseClass::OnKeyCodeTyped( KEY_SPACE );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Passes the command to the parent and closes the box
//-----------------------------------------------------------------------------
void CCareerBaseBox::OnCommand( const char *command )
{
	stricmp( command, "Ok" );
	if ( stricmp( command, "close" ) )
	{
		PostActionSignal( new KeyValues( "Command", "command", command ) );
	}

	BaseClass::OnCommand( command );
	Close();
}

//-----------------------------------------------------------------------------
// Purpose: Adds a button that gets the career button sounds
//-----------------------------------------------------------------------------
void CCareerBaseBox::AddButton( Button *pButton )
{
	m_buttons.PutElement( pButton );
}

//-----------------------------------------------------------------------------
// Purpose: Constructors and destructor
//-----------------------------------------------------------------------------
CCareerQueryBox::CCareerQueryBox( const char *title, const char *labelText, const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName )
{
}

CCareerQueryBox::CCareerQueryBox( const wchar_t *title, const wchar_t *labelText, const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName )
{
}

CCareerQueryBox::~CCareerQueryBox()
{
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerDifficultyBox::CCareerDifficultyBox( const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName, false )
{
	CCareerButton *pDifficultyButton[MAX_CAREER_DIFFICULTY];
	pDifficultyButton[CAREER_DIFFICULTY_EASY] = new CCareerButton( this, "Difficulty0Button", "", "", false );
	pDifficultyButton[CAREER_DIFFICULTY_NORMAL] = new CCareerButton( this, "Difficulty1Button", "", "", false );
	pDifficultyButton[CAREER_DIFFICULTY_HARD] = new CCareerButton( this, "Difficulty2Button", "", "", false );
	pDifficultyButton[CAREER_DIFFICULTY_EXPERT] = new CCareerButton( this, "Difficulty3Button", "", "", false );

	m_pEnableTutorCheck = new CCvarToggleCheckButton( this, "Tutor", "#CStrike_Tutor_Enabled", "tutor_enable" );

	AddButton( pDifficultyButton[CAREER_DIFFICULTY_EASY] );
	AddButton( pDifficultyButton[CAREER_DIFFICULTY_NORMAL] );
	AddButton( pDifficultyButton[CAREER_DIFFICULTY_HARD] );
	AddButton( pDifficultyButton[CAREER_DIFFICULTY_EXPERT] );

	EditablePanel::LoadControlSettings( "resource/CareerBoxDifficulty.res" );

	const char *profileName = engine->pfnGetCvarString( "name" );
	CCareerProfileData *pProfile = TheCareerGame->GetProfileForName( CCareerGame::GetSaveGamePlayerName( profileName ) );
	if ( profileName )
	{
		SetLabelText( SharedWVarArgs( L"%hs", profileName ) );
	}

	m_pFocusButton = pDifficultyButton[pProfile->lastDifficulty];

	const int BufLen = 64;
	char buf[BufLen];
	wchar_t constructedBuf[BufLen];
	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		_snprintf( buf, BufLen, "#Career_Difficulty%d", i );
		if ( pProfile && pProfile->difficulty[i].played )
		{
			wchar_t *continueStr = localize()->Find( "#Career_ContinueDifficulty" );
			wchar_t *diffStr = localize()->Find( buf );
			localize()->ConstructString( constructedBuf, sizeof(constructedBuf), L"%s1 (%s2)", 2, diffStr, continueStr );
			pDifficultyButton[i]->SetText( constructedBuf );
		}
		else
		{
			pDifficultyButton[i]->SetText( buf );
		}
	}

	delete pProfile;
}

//-----------------------------------------------------------------------------
// Purpose: Applies the tutor checkbox when a difficulty is chosen
//-----------------------------------------------------------------------------
void CCareerDifficultyBox::OnCommand( const char *command )
{
	if ( strstr( command, "ChooseDifficulty" ) )
	{
		m_pEnableTutorCheck->ApplyChanges();
	}

	BaseClass::OnCommand( command );
}

CCareerDifficultyBox::~CCareerDifficultyBox()
{
}

//-----------------------------------------------------------------------------
// Purpose: Shows the box with the last played difficulty focused
//-----------------------------------------------------------------------------
void CCareerDifficultyBox::ShowWindow()
{
	SetVisible( true );
	SetEnabled( true );
	MoveToFront();
	m_pFocusButton->RequestFocus();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerNewProfileBox::CCareerNewProfileBox( const char *panelName, Panel *parent ) : CCareerBaseBox( parent, panelName, false )
{
	m_pEntry = new CCvarTextEntry( this, "ProfileTextEntry", "name" );
	LoadControlSettings( "resource/CareerBoxNewProfile.res" );
	m_pEntry->SetText( engine->pfnGetCvarString( "name" ) );
}

CCareerNewProfileBox::~CCareerNewProfileBox()
{
}

//-----------------------------------------------------------------------------
// Purpose: Shows the box with the name entry focused
//-----------------------------------------------------------------------------
void CCareerNewProfileBox::ShowWindow()
{
	BaseClass::ShowWindow();
	m_pEntry->RequestFocus();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Ok stores the name and creates the profile
//-----------------------------------------------------------------------------
void CCareerNewProfileBox::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) )
	{
		m_pEntry->ApplyChanges( true );
		BaseClass::OnCommand( "CreateProfile" );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Escape cancels, enter accepts the name
//-----------------------------------------------------------------------------
void CCareerNewProfileBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
	}
	else if ( code == KEY_ENTER )
	{
		OnCommand( "Ok" );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}
