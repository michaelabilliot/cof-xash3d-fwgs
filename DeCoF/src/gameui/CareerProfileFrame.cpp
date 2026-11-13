//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: The career profile selection screen
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "CareerProfileFrame.h"
#include "CareerBox.h"
#include "CareerDefinitions.h"
#include "CareerGame.h"
#include "CareerProfile.h"
#include "CareerProfileButton.h"
#include "DottedLabel.h"
#include "EngineInterface.h"
#include "Taskbar.h"
#include "../game_shared/shared_util.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/URLLabel.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: The mission pack list, selects the mission pack it shows
//-----------------------------------------------------------------------------
class MissionPackComboBox : public ComboBox
{
public:
	MissionPackComboBox( CCareerProfileFrame *parent, const char *panelName, int numLines, bool allowEdit ) : ComboBox( parent, panelName, numLines, allowEdit )
	{
		m_pParent = parent;
	}

	virtual void OnMenuItemSelected();

private:
	CCareerProfileFrame *m_pParent;
};

//-----------------------------------------------------------------------------
// Purpose: Activates the selected mission pack
//-----------------------------------------------------------------------------
void MissionPackComboBox::OnMenuItemSelected()
{
	ComboBox::OnMenuItemSelected();

	KeyValues *userData = GetActiveItemUserData();
	const char *selectedCareerName = userData->GetString( "MissionPackName", "" );
	if ( !selectedCareerName || !TheCareerDefinitions->GetActiveCareerDefinition() || strcmp( selectedCareerName, TheCareerDefinitions->GetActiveCareerDefinition()->GetCareerName() ) )
	{
		m_pParent->SelectMissionPack( selectedCareerName );
	}

	m_pParent->QueueFocusUpdate( 5 );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerProfileFrame::CCareerProfileFrame( Panel *parent ) : CCareerBaseFrame( parent, "CareerProfileFrame", false )
{
	CCareerGame::Create();

	SetScheme( scheme()->GetScheme( "CareerScheme" ) );
	g_pTaskbar->AddTask( GetVPanel() );
	SetTitle( "#GameUI_NewGame", true );

	m_pProfileSelectionBackground = new CDottedBgLabel( this, "ProfileSelectionBackground", "", 0, 0, 0 );
	m_pProfileSelectionBackground->SetTextInset( 2, 2 );
	m_pProfileList = new CCareerPanelListPanel( this, "ProfileListPanel", true );

	new CBitmapImagePanel( this, "TitlePanel", "gfx/career/Logo_CSCZ_Big" );
	new CBitmapImagePanel( this, "SelectProfilePanel", "gfx/career/hed_selectprofile" );

	m_buttons.PutElement( new CCareerButton( this, "Cancel", "#Career_Cancel", "gfx/career/icon_cancel", false ) );
	m_buttons.PutElement( m_pNewButton = new CCareerButton( this, "NewProfile", "#Career_New", "", false ) );

	m_buttons.PutElement( m_pResumeButton = new CCareerButton( this, "ResumeProfile", "#Career_Resume", "", false ) );

	m_buttons.PutElement( m_pDeleteButton = new CCareerButton( this, "DeleteProfile", "#Career_Delete", "", false ) );

	m_pMissionPackBackground = new CDottedBgLabel( this, "MissionPackSelectBackground", "", 0, 0, 0 );
	m_pMissionPackBackground->SetTextInset( 2, 2 );
	m_pMissionPackAuthor = new Label( this, "LabelMissionPackAuthor", "" );
	m_pMissionPackDescription = new Label( this, "LabelMissionPackDescription", "" );
	m_pMissionPackURL = new URLLabel( this, "LabelMissionPackURL", "", "" );
	m_pMissionPackLogo = new CBitmapImagePanel( this, "ImagePanelMissionPackLogo", "" );
	m_pMissionPackComboBox = new MissionPackComboBox( this, "ComboBoxMissionPack", TheCareerDefinitions->GetNumCareers(), false );

	LoadControlSettings( "resource/CareerProfileFrame.res" );

	ShowPlayerName( false );
	ShowMissionPackName( false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerProfileFrame::~CCareerProfileFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the frame and refreshes the mission pack details
//-----------------------------------------------------------------------------
void CCareerProfileFrame::PerformLayout()
{
	BaseClass::PerformLayout();
	PopulateMissionPacks();
}

//-----------------------------------------------------------------------------
// Purpose: Fills the profile list, asks for a new profile when there is none
//-----------------------------------------------------------------------------
void CCareerProfileFrame::Activate()
{
	BaseClass::Activate();
	ShowPlayerName( false );
	ShowMissionPackName( false );

	const int BufLen = 64;
	char buf[BufLen];
	IImage *image[MAX_CAREER_DIFFICULTY][2];
	bool medals[MAX_CAREER_DIFFICULTY];
	int i;
	for ( i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		medals[i] = false;
		_snprintf( buf, BufLen, "gfx/career/medals/Medal.0%d_Inactive", i+1 );
		image[i][0] = scheme()->GetImage( buf, false );
		_snprintf( buf, BufLen, "gfx/career/medals/Medal.0%d_Active", i+1 );
		image[i][1] = scheme()->GetImage( buf, false );
	}

	PopulateMissionPacks();
	int numProfiles = PopulateProfiles();
	RestoreFocus();
	m_needFocus = 0;

	if ( !numProfiles )
	{
		OnCommand( "NewProfile" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Focuses the resume button, or the new button when there is no profile
//-----------------------------------------------------------------------------
void CCareerProfileFrame::RestoreFocus()
{
	if ( m_pResumeButton->IsEnabled() )
	{
		m_pResumeButton->RequestFocus();
	}
	else
	{
		m_pNewButton->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restores the focus a few frames after the mission pack changed
//-----------------------------------------------------------------------------
void CCareerProfileFrame::OnThink()
{
	BaseClass::OnThink();

	if ( m_needFocus )
	{
		--m_needFocus;
		if ( !m_needFocus )
		{
			RestoreFocus();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a button for each saved profile, returns the profile count
//-----------------------------------------------------------------------------
int CCareerProfileFrame::PopulateProfiles()
{
	m_pProfileList->DeleteAllItems();
	m_profiles.RemoveAll();
	m_name[0] = 0;

	const int BufLen = 62;
	char buf[BufLen];
	CProfileButton *pButton;
	ProfileVector profiles = TheCareerGame->BuildProfileList();
	long latestProfile = 0;
	for ( ProfileVector::iterator it = profiles.begin(); it != profiles.end(); ++it )
	{
		CCareerProfileData *pProfile = *it;

		wchar_t wbuf[BufLen];
		localize()->ConvertANSIToUnicode( pProfile->name.c_str(), wbuf, sizeof( wbuf ) );
		pButton = new CProfileButton( this, "ProfileButton", wbuf, pProfile->medals );
		pButton->SetProfileName( pProfile->name.c_str() );
		_snprintf( buf, BufLen, "SelectProfileAndFocus %s", pProfile->name.c_str() );
		pButton->SetCommand( buf );
		pButton->AddActionSignalTarget( this );
		m_pProfileList->AddItem( pButton );
		m_profiles.AddElement( pButton );

		if ( pProfile->filetime >= latestProfile )
		{
			latestProfile = pProfile->filetime;
			strncpy( m_name, pProfile->name.c_str(), 32 );
			m_name[31] = 0;
		}

		delete pProfile;
	}

	UpdateSelectedProfile( m_name );

	if ( profiles.empty() )
	{
		m_pResumeButton->SetEnabled( false );
		m_pDeleteButton->SetEnabled( false );
	}
	else
	{
		m_pResumeButton->SetEnabled( true );
		m_pDeleteButton->SetEnabled( true );
	}

	m_pProfileList->SetSliderYOffset( 2 );

	return profiles.size();
}

//-----------------------------------------------------------------------------
// Purpose: Background colors and the mission pack URL
//-----------------------------------------------------------------------------
void CCareerProfileFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pProfileSelectionBackground->SetBgColor( pScheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackBackground->SetBgColor( pScheme->GetColor( "ControlDarkBG", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackURL->SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pMissionPackURL->SetBorder( NULL );

	PopulateMissionPacks();

	m_pMissionPackURL->SetFgColor( pScheme->GetColor( "MissionPackURL", Color( 255, 255, 255, 255 ) ) );
	m_pMissionPackLogo->SetBorder( pScheme->GetBorder( "BrowserBorder" ) );

	SetBgColor( Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: Opens the difficulty selection box
//-----------------------------------------------------------------------------
void CCareerProfileFrame::ChooseDifficulty()
{
	CCareerDifficultyBox *box = new CCareerDifficultyBox( "CareerBoxDifficulty", this );
	box->AddActionSignalTarget( this );
	box->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Marks the button of the selected profile
//-----------------------------------------------------------------------------
void CCareerProfileFrame::UpdateSelectedProfile( const char *profile )
{
	strncpy( m_name, profile, 32 );
	m_name[31] = 0;

	for ( int i=0; i<m_profiles.GetCount(); ++i )
	{
		CProfileButton *pProfileButton = m_profiles[i];
		if ( !strcmp( m_name, pProfileButton->GetProfileName() ) )
		{
			pProfileButton->SetActive( true );
		}
		else
		{
			pProfileButton->SetActive( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads the profile at the given difficulty and opens its career
//-----------------------------------------------------------------------------
bool HandleDifficulty( CareerDifficultyType difficulty )
{
	CCareerGame::Create();
	TheCareerGame->Reset();
	if ( !TheCareerGame->LoadProfile( difficulty ) )
		return false;

	CareerDefinition *pActiveCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	g_pTaskbar->OnOpenResumeCareerGameDialog( difficulty );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Profile buttons, the profile boxes and the difficulty choice
//-----------------------------------------------------------------------------
void CCareerProfileFrame::OnCommand( const char *command )
{
	if ( !stricmp( command, "NewProfile" ) )
	{
		CCareerNewProfileBox *box = new CCareerNewProfileBox( "CareerBoxNewProfile", this );
		box->AddActionSignalTarget( this );
		box->DoModal();
	}
	else if ( !stricmp( command, "CreateProfile" ) )
	{
		bool profileExists = false;
		ProfileVector profiles = TheCareerGame->BuildProfileList();
		for ( ProfileVector::iterator it = profiles.begin(); it != profiles.end(); ++it )
		{
			CCareerProfileData *pProfile = *it;
			if ( !stricmp( pProfile->name.c_str(), engine->pfnGetCvarString( "name" ) ) )
			{
				profileExists = true;
			}
		}

		if ( profileExists )
		{
			CCareerQueryBox *box = new CCareerQueryBox( "#Career_OverwriteConfirmationTitle", "#Career_OverwriteConfirmationText", "CareerBoxOverwrite", this );
			box->SetCancelButtonAsDefault();
			box->AddActionSignalTarget( this );
			box->DoModal();
		}
		else
		{
			TheCareerGame->CreateBlankProfile();
			PopulateProfiles();
			UpdateSelectedProfile( engine->pfnGetCvarString( "name" ) );
			ChooseDifficulty();
		}
	}
	else if ( !stricmp( command, "ReallyCreateProfile" ) )
	{
		TheCareerGame->CreateBlankProfile();
		PopulateProfiles();
		UpdateSelectedProfile( engine->pfnGetCvarString( "name" ) );
		ChooseDifficulty();
	}
	else if ( !strncmp( command, "SelectProfile ", 14 ) && strlen( command ) > 14 )
	{
		const char *profile = command + 14;
		UpdateSelectedProfile( profile );
	}
	else if ( !strncmp( command, "SelectProfileAndFocus ", 22 ) && strlen( command ) > 22 )
	{
		const char *profile = command + 22;
		UpdateSelectedProfile( profile );
		RestoreFocus();
	}
	else if ( !strcmp( command, "ResumeProfile" ) && m_name[0] )
	{
		engine->Cvar_Set( "name", m_name );
		ChooseDifficulty();
	}
	else if ( !strcmp( command, "ResumeProfile" ) )
	{
		OnCommand( "NewProfile" );
	}
	else if ( !strcmp( command, "DeleteProfile" ) && m_name[0] )
	{
		const int BufLen = 256;
		wchar_t wbuf[BufLen];
		wchar_t nameBuf[BufLen];
		localize()->ConvertANSIToUnicode( m_name, nameBuf, sizeof( nameBuf ) );
		localize()->ConstructString( wbuf, sizeof( wbuf ), localize()->Find( "#Career_DeleteProfileText" ), 1, nameBuf );

		CCareerQueryBox *box = new CCareerQueryBox( localize()->Find( "#Career_DeleteProfileTitle" ), wbuf, "CareerBoxDelete", this );
		box->SetCancelButtonAsDefault();
		box->SetLabelText( wbuf );
		box->AddActionSignalTarget( this );
		box->DoModal();
	}
	else if ( !strcmp( command, "ReallyDeleteProfile" ) && m_name[0] )
	{
		TheCareerGame->DeleteProfile( m_name );
		m_name[0] = 0;
		PopulateProfiles();
	}
	else if ( !strcmp( command, "ChooseDifficulty0" ) && m_name[0] )
	{
		engine->Cvar_SetValue( "cl_career_difficulty", 0 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_EASY ) )
		{
			Close();
		}
	}
	else if ( !strcmp( command, "ChooseDifficulty1" ) && m_name[0] )
	{
		engine->Cvar_SetValue( "cl_career_difficulty", 1 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_NORMAL ) )
		{
			Close();
		}
	}
	else if ( !strcmp( command, "ChooseDifficulty2" ) && m_name[0] )
	{
		engine->Cvar_SetValue( "cl_career_difficulty", 2 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_HARD ) )
		{
			Close();
		}
	}
	else if ( !strcmp( command, "ChooseDifficulty3" ) && m_name[0] )
	{
		engine->Cvar_SetValue( "cl_career_difficulty", 3 );
		if ( HandleDifficulty( CAREER_DIFFICULTY_EXPERT ) )
		{
			Close();
		}
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Escape closes the frame, enter presses the focused button
//-----------------------------------------------------------------------------
void CCareerProfileFrame::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "close" );
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
// Purpose: Fills the mission pack list and shows the active mission pack
//-----------------------------------------------------------------------------
void CCareerProfileFrame::PopulateMissionPacks()
{
	CareerDefinition *pCareer;
	CareerDefinition *pActiveCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( !pActiveCareer )
		return;

	m_pMissionPackComboBox->DeleteAllItems();

	KeyValues *userData = new KeyValues( "UserData" );
	int activeRow = 0;
	for ( int i=0; i<TheCareerDefinitions->GetNumCareers(); ++i )
	{
		pCareer = TheCareerDefinitions->GetCareer( i );
		userData->SetString( "MissionPackName", pCareer->GetCareerName() );

		const int BufLen = 128;
		wchar_t wbuf[BufLen] = L"";
		wchar_t *tmp = wbuf;
		int len = BufLen;
		BufWPrintf( tmp, len, L"  %s", pCareer->GetTitle() );
		m_pMissionPackComboBox->AddItem( wbuf, userData );

		if ( pCareer == pActiveCareer )
		{
			activeRow = i;
		}
	}
	userData->deleteThis();
	m_pMissionPackComboBox->ActivateItemByRow( activeRow );

	const int BufLen = 256;
	wchar_t wbuf[BufLen] = L"";
	wchar_t *authorFormatString = localize()->Find( "#Career_MissionPackAuthor" );
	if ( !authorFormatString )
	{
		authorFormatString = L"%s1";
	}
	localize()->ConstructString( wbuf, sizeof( wbuf ), authorFormatString, 1, pActiveCareer->GetAuthor() );
	m_pMissionPackAuthor->SetText( wbuf );
	m_pMissionPackDescription->SetText( pActiveCareer->GetDescription() );
	m_pMissionPackURL->SetText( pActiveCareer->GetURL() );
	m_pMissionPackURL->SetURL( pActiveCareer->GetURL() );

	char textureName[MAX_PATH];
	_snprintf( textureName, sizeof( textureName ), "MissionPacks/%s/thumbnail", pActiveCareer->GetCareerName() );
	m_pMissionPackLogo->setTexture( textureName, true );

	m_bgFade.SetFade( pActiveCareer->GetBgStartColor(), pActiveCareer->GetBgEndColor(), 25.0f, ColorFade::down );
}

//-----------------------------------------------------------------------------
// Purpose: Activates a mission pack and lists its profiles
//-----------------------------------------------------------------------------
void CCareerProfileFrame::SelectMissionPack( const char *missionPackName )
{
	TheCareerDefinitions->SetActiveCareerDefinition( missionPackName );
	PopulateMissionPacks();
	PopulateProfiles();
}
