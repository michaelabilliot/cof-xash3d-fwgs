//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "LoadingDialog.h"
#include "GameUI_Interface.h"
#include "EngineInterface.h"
#include "igameuifuncs.h"
#include "ModInfo.h"
#include "BitmapImagePanel.h"
#include "CareerGame.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/RichText.h>

using namespace vgui2;

// CareerMapFrame.cpp
extern void ShowCareerMapDescription( RichText *pText, CCareerMap *pMap, Color normalColor, Color friendlyFireColor );

// GameUI_Interface.cpp
extern vgui2::DHANDLE< CLoadingDialog > g_hLoadingDialog;

//-----------------------------------------------------------------------------
// Purpose: description text of the career map, gets a scrollbar when it overflows
//-----------------------------------------------------------------------------
class CareerRichText : public RichText
{
public:
	CareerRichText( Panel *parent, const char *panelName ) : RichText( parent, panelName, true )
	{
		m_fontHeight = 0;
		m_addedScrollbar = false;
	}

	virtual ~CareerRichText()
	{
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		RichText::ApplySchemeSettings( pScheme );

		HFont font = pScheme->GetFont( "Default", IsProportional() );
		m_fontHeight = surface()->GetFontTall( font );
		m_addedScrollbar = false;
	}

	bool IsFullHeight()
	{
		int wide, tall;
		GetSize( wide, tall );

		int numLines = GetNumLines();
		int textHeight_guess = numLines * ( m_fontHeight + 1 ) + 3;
		return ( tall >= textHeight_guess );
	}

	virtual void PerformLayout()
	{
		RichText::PerformLayout();

		if ( !IsFullHeight() && !m_addedScrollbar )
		{
			SetVerticalScrollbar( true );
			m_addedScrollbar = true;
			InsertString( L"" );
		}
	}

private:
	int m_fontHeight;
	bool m_addedScrollbar;
};

//-----------------------------------------------------------------------------
// Purpose: the banner of the loading dialog, the first page opens in place
//-----------------------------------------------------------------------------
class CLoadingDialog::LoadingHTML : public HTML
{
public:
	LoadingHTML( Panel *parent, const char *name ) : HTML( parent, name )
	{
		m_bFirstURL_guess = true;
	}

	virtual ~LoadingHTML()
	{
	}

	virtual bool OnStartURL( const char *url, const char *target, bool first );

private:
	bool m_bFirstURL_guess;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLoadingDialog::CLoadingDialog( vgui2::Panel *parent ) : Frame( parent, "LoadingDialog" )
{
	SetSize( 416, 100 );
	SetTitle( "#GameUI_Loading", true );

	m_bShowingSecondaryProgress = false;
	m_flSecondaryProgress = 0.0f;
	m_flLastSecondaryProgressUpdateTime = 0.0f;
	m_flSecondaryProgressStartTime = 0.0f;
	m_isCareerLoadingDialog = false;
	m_switchTime = 0.0f;
	m_flProgress = 0.0f;
	m_isHttpDownloadDialog = false;
	m_bShowingVACInfo = false;
	m_bShowingVACError = false;
	m_szUserName[ 0 ] = 0;
	m_pCareerMapImagePanel = NULL;

	m_pProgress = new ProgressBar( this, "Progress" );
	m_pProgress2 = new ProgressBar( this, "Progress2" );
	m_pInfoLabel = new Label( this, "InfoLabel", "" );
	m_pCancelButton = new Button( this, "CancelButton", "#GameUI_Cancel" );
	m_pHTML_guess = new LoadingHTML( this, "BannerAd" );
	m_pTimeRemainingLabel = new Label( this, "TimeRemainingLabel", "" );
	m_pCancelButton->SetCommand( "Cancel" );

	m_pCareerMapDescLabel = NULL;

	m_pInfoLabel->SetBounds( 20, 32, 392, 24 );
	m_pProgress->SetBounds( 20, 64, 300, 24 );
	m_pCancelButton->SetBounds( 330, 64, 72, 24 );
	m_pHTML_guess->SetBounds( 24, 204, 340, 56 );
	m_pHTML_guess->SetScrollbarsEnabled( false );
	m_pInfoLabel->SetTextColorState( Label::CS_DULL );

	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetMoveable( false );

	m_pProgress2->SetVisible( false );

	if ( TheCareerGame && TheCareerGame->IsPlayingMatch() )
	{
		m_pCareerMapDescLabel = new CareerRichText( this, "CareerInfo" );
		m_isCareerLoadingDialog = true;

		const int BufLen = 128;
		char buf[ BufLen ];
		buf[ BufLen - 1 ] = 0;

		sprintf( buf, "gfx/thumbnails/maps/%s", TheCareerGame->GetLastMap() );
		m_pCareerMapImagePanel = new CBitmapImagePanel( this, "CareerMapImage", NULL );
		m_pCareerMapImagePanel->AddActionSignalTarget( this );
		m_pCareerMapImagePanel->setTexture( buf );

		m_pCareerMapLabel = new Label( this, "CareerMapTitle", "" );
		_snprintf( buf, BufLen, "#Career_Map_%s", TheCareerGame->GetLastMap() );
		buf[ BufLen - 1 ] = 0;
		if ( !localize()->Find( buf ) )
		{
			_snprintf( buf, BufLen, "%s", TheCareerGame->GetLastMap() );
		}
		m_pCareerMapLabel->SetText( buf );

		m_pCareerScenarioInfo = new Label( this, "CareerScenarioInfo", "" );
		if ( TheCareerGame->PlayAsCT() )
		{
			if ( !strncmp( "de_", TheCareerGame->GetLastMap(), 3 ) )
			{
				m_pCareerScenarioInfo->SetText( "#Career_BombDefusalCT" );
			}
			else
			{
				m_pCareerScenarioInfo->SetText( "#Career_HostageRescueCT" );
			}
		}
		else
		{
			if ( !strncmp( "de_", TheCareerGame->GetLastMap(), 3 ) )
			{
				m_pCareerScenarioInfo->SetText( "#Career_BombDefusalT" );
			}
			else
			{
				m_pCareerScenarioInfo->SetText( "#Career_HostageRescueT" );
			}
		}

		IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
		Color normalColor = pScheme->GetColor( "ControlText", Color( 255, 255, 255, 255 ) );
		Color friendlyFireColor = pScheme->GetColor( "BrightControlText", Color( 255, 242, 0, 255 ) );
		ShowCareerMapDescription( m_pCareerMapDescLabel, TheCareerGame->GetLastMapPtr(), normalColor, friendlyFireColor );

		LoadControlSettings( "Resource/CareerLoadingDialog.res" );

		m_pHTML_guess->SetVisible( false );
		m_pCareerMapDescLabel->SetVerticalScrollbar( false );
	}
	else if ( engine->CheckParm( "-steam", NULL ) || engine->CheckParm( "-showplatform", NULL ) )
	{
		char szURL_guess[ 1024 ];
		if ( system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\LastContentProviderURL", szURL_guess, sizeof( szURL_guess ) ) )
		{
			m_pHTML_guess->OpenURL( szURL_guess );
		}

		if ( gameuifuncs->IsConnectedToVACSecureServer() )
		{
			LoadControlSettings( "Resource/LoadingDialogVAC.res" );
			m_bShowingVACInfo = true;
		}
		else
		{
			LoadControlSettings( "Resource/LoadingDialog.res" );
		}
	}
	else if ( ModInfo().IsSinglePlayerOnly() )
	{
		LoadControlSettings( "Resource/LoadingDialogNoBannerSingle.res" );
	}
	else
	{
		LoadControlSettings( "Resource/LoadingDialogNoBanner.res" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLoadingDialog::~CLoadingDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayProgressBar( const char *resourceType, const char *resourceName )
{
	SetTitle( "#GameUI_Loading", true );

	// we need to update the progress bar on the screen
	surface()->RestrictPaintToSinglePanel( GetVPanel() );

	BaseClass::Activate();

	m_pProgress->SetVisible( true );
	if ( !ModInfo().IsSinglePlayerOnly() )
	{
		m_pInfoLabel->SetVisible( true );
	}
	m_pInfoLabel->SetText( "" );

	m_pCancelButton->SetText( "#GameUI_Cancel" );
	m_pCancelButton->SetCommand( "Cancel" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetupControlSettingsForErrorDisplay( const char *settingsFile )
{
	SetTitle( "#GameUI_Disconnected", true );
	LoadControlSettings( settingsFile );

	surface()->RestrictPaintToSinglePanel( GetVPanel() );

	BaseClass::Activate();

	m_pProgress->SetVisible( false );
	m_pInfoLabel->SetVisible( true );
	m_pCancelButton->SetText( "#GameUI_Close" );
	m_pCancelButton->SetCommand( "Close" );
	m_pInfoLabel->SetText( "" );

	if ( m_pCareerMapDescLabel )
	{
		m_pCareerMapDescLabel->SetVisible( false );
		m_pCareerMapDescLabel->SetVisible( false );
		m_pCareerMapLabel->SetVisible( false );
		m_pCareerScenarioInfo->SetVisible( false );
		m_pCareerMapImagePanel->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the disconnect reason
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayGenericError( const char *failureReason, const char *extendedReason )
{
	if ( m_bShowingVACError )
	{
		// don't let the error message override the VAC error
		m_bShowingVACError = false;
		return;
	}

	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogError.res" );

	if ( extendedReason && strlen( extendedReason ) > 0 )
	{
		wchar_t compositeReason[ 256 ], finalMsg[ 512 ], formatStr[ 256 ];

		if ( extendedReason[ 0 ] == '#' )
		{
			wchar_t *tmpStr = localize()->Find( extendedReason );
			if ( tmpStr )
			{
				wcsncpy( compositeReason, tmpStr, sizeof( compositeReason ) / sizeof( wchar_t ) );
			}
			else
			{
				localize()->ConvertANSIToUnicode( extendedReason, compositeReason, sizeof( compositeReason ) );
			}
		}
		else
		{
			localize()->ConvertANSIToUnicode( extendedReason, compositeReason, sizeof( compositeReason ) );
		}

		if ( failureReason[ 0 ] == '#' )
		{
			wchar_t *tmpStr = localize()->Find( failureReason );
			if ( tmpStr )
			{
				wcsncpy( formatStr, tmpStr, sizeof( formatStr ) / sizeof( wchar_t ) );
			}
			else
			{
				localize()->ConvertANSIToUnicode( failureReason, formatStr, sizeof( formatStr ) );
			}
		}
		else
		{
			localize()->ConvertANSIToUnicode( failureReason, formatStr, sizeof( formatStr ) );
		}

		localize()->ConstructString( finalMsg, sizeof( finalMsg ), formatStr, 1, compositeReason );
		m_pInfoLabel->SetText( finalMsg );
	}
	else
	{
		m_pInfoLabel->SetText( failureReason );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayVACBannedError()
{
	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorVACBanned.res" );
	SetTitle( "#VAC_ConnectionRefusedTitle", true );
	m_bShowingVACError = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplayNoSteamConnectionError()
{
	SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorNoSteamConnection.res" );
	m_bShowingVACError = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::DisplaySteamConnectionError( const char *username, ESteamLoginFailure steamLoginFailure )
{
	switch ( steamLoginFailure )
	{
	case STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorLoggedInElsewhere.res" );
		break;

	case STEAMLOGINFAILURE_CONNECTIONLOST:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogConnectionLost.res" );
		break;

	case STEAMLOGINFAILURE_NOCONNECTION:
		SetupControlSettingsForErrorDisplay( "Resource/LoadingDialogErrorNoSteamConnection.res" );
		break;
	}

	m_bShowingVACError = true;
	m_pCancelButton->SetText( "#GameUI_Close" );
	m_pCancelButton->SetCommand( "close" );

	strncpy( m_szUserName, username, sizeof( m_szUserName ) );
	m_szUserName[ sizeof( m_szUserName ) - 1 ] = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetStatusText( const char *statusText )
{
	m_pInfoLabel->SetText( statusText );
}

//-----------------------------------------------------------------------------
// Purpose: updates the time remaining and the switch to the career dialog
//-----------------------------------------------------------------------------
void CLoadingDialog::OnThink()
{
	BaseClass::OnThink();

	if ( m_bShowingSecondaryProgress )
	{
		// calculate the time remaining string
		wchar_t unicode[ 512 ];
		if ( m_flSecondaryProgress >= 1.0f )
		{
			m_pTimeRemainingLabel->SetText( "complete" );
		}
		else if ( ProgressBar::ConstructTimeRemainingString( unicode, sizeof( unicode ), m_flSecondaryProgressStartTime, (float)system()->GetFrameTime(), m_flSecondaryProgress, m_flLastSecondaryProgressUpdateTime, true ) )
		{
			m_pTimeRemainingLabel->SetText( unicode );
		}
		else
		{
			m_pTimeRemainingLabel->SetText( "" );
		}
	}

	if ( m_switchTime != 0.0f )
	{
		if ( m_switchTime < system()->GetFrameTime() )
		{
			CompleteSwitchToPausedCareerDialog();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::PerformLayout()
{
	MoveToCenterOfScreen();
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetProgressPoint( int progressPoint )
{
	if ( !m_bShowingVACInfo && gameuifuncs->IsConnectedToVACSecureServer() )
	{
		LoadControlSettings( "Resource/LoadingDialogVAC.res" );
		m_bShowingVACInfo = true;
	}

	m_flProgress = (float)progressPoint / ( m_iRangeMax - m_iRangeMin );
	m_pProgress->SetProgress( m_flProgress );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetProgressRange( int min, int max )
{
	m_iRangeMin = min;
	m_iRangeMax = max;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetSecondaryProgress( float progress )
{
	// don't show the progress if we've jumped right to completion
	if ( !m_bShowingSecondaryProgress && progress > 0.99f )
		return;

	// if we haven't yet shown secondary progress then reconfigure the dialog
	if ( !m_bShowingSecondaryProgress )
	{
		if ( m_bShowingVACInfo || gameuifuncs->IsConnectedToVACSecureServer() )
		{
			LoadControlSettings( "Resource/LoadingDialogDualProgressVAC.res" );
			m_bShowingVACInfo = true;
		}
		else
		{
			LoadControlSettings( "Resource/LoadingDialogDualProgress.res" );
		}

		m_bShowingSecondaryProgress = true;
		m_pProgress->SetProgress( m_flProgress );
		m_pProgress2->SetVisible( true );
		m_flSecondaryProgressStartTime = (float)system()->GetFrameTime();
	}

	// if progress has increased then update the progress counters
	if ( progress > m_flSecondaryProgress )
	{
		m_pProgress2->SetProgress( progress );
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)system()->GetFrameTime();
	}

	// if progress has decreased then reset progress counters
	if ( progress < m_flSecondaryProgress )
	{
		m_pProgress2->SetProgress( progress );
		m_flSecondaryProgress = progress;
		m_flLastSecondaryProgressUpdateTime = (float)system()->GetFrameTime();
		m_flSecondaryProgressStartTime = (float)system()->GetFrameTime();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SetSecondaryProgressText( const char *statusText )
{
	SetControlString( "SecondaryProgressLabel", statusText );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::OnClose()
{
	// remove any rendering restrictions
	surface()->RestrictPaintToSinglePanel( NULL );

	BaseClass::OnClose();
	MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: shows the banner while files download
//-----------------------------------------------------------------------------
void CLoadingDialog::SwitchToHttpDownloadDialog( const char *url )
{
	m_isHttpDownloadDialog = true;

	char szURL_guess[ 1024 ];
	if ( system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\LastContentProviderURL", szURL_guess, sizeof( szURL_guess ) ) )
	{
		m_pHTML_guess->OpenURL( szURL_guess );
	}

	LoadControlSettings( "Resource/LoadingDialog.res" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::SwitchToPausedCareerDialog()
{
	if ( m_isCareerLoadingDialog )
	{
		// Wait 1.5 seconds before switching over
		m_switchTime = system()->GetFrameTime() + 1.5;
	}
	else
	{
		GameUI().FinishCareerLoad();
		Close();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::CompleteSwitchToPausedCareerDialog()
{
	if ( !m_isCareerLoadingDialog )
		return;

	m_switchTime = 0.0f;

	m_pCancelButton->SetText( "#Career_Start" );
	m_pCancelButton->SetCommand( "StartCareer" );

	int pX, pY, pW, pH;
	int bX, bY, bW, bH;
	int iX, iY, iW, iH;

	m_pCancelButton->GetBounds( bX, bY, bW, bH );
	m_pProgress->GetBounds( pX, pY, pW, pH );
	m_pInfoLabel->GetBounds( iX, iY, iW, iH );

	bX = GetWide() / 2 - bW;
	bY = iY + ( pY - iY + pH ) / 2 - bH / 2;

	m_pProgress->SetVisible( false );
	m_pInfoLabel->SetVisible( false );

	m_pCancelButton->SetBounds( bX, bY, bW + bW, bH );
	m_pCancelButton->SetContentAlignment( Label::a_center );
	m_pCancelButton->RequestFocus();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::OnCommand( const char *command )
{
	if ( !stricmp( command, "Cancel" ) )
	{
		if ( m_isHttpDownloadDialog )
		{
			engine->pfnClientCmd( "httpstop\n" );
		}
		else
		{
			if ( TheCareerGame && TheCareerGame->IsPlayingMatch() && !m_pProgress->IsVisible() )
			{
				GameUI().FinishCareerLoad();
			}

			// disconnect from the server
			engine->pfnClientCmd( "disconnect\n" );
		}

		// close
		Close();
	}
	else if ( !stricmp( command, "StartCareer" ) )
	{
		GameUI().FinishCareerLoad();
		Close();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Maps ESC to cancel
//-----------------------------------------------------------------------------
void CLoadingDialog::OnKeyCodePressed( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
	}
	else
	{
		BaseClass::OnKeyCodePressed( code );
	}
}

//-----------------------------------------------------------------------------
// Purpose: the first page opens in place, later pages open in a new window
//-----------------------------------------------------------------------------
bool CLoadingDialog::LoadingHTML::OnStartURL( const char *url, const char *target, bool first )
{
	if ( m_bFirstURL_guess )
	{
		m_bFirstURL_guess = false;
		NewWindowsOnly( true );
		return true;
	}

	return HTML::OnStartURL( url, target, first );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CLoadingDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	if ( m_pCareerMapImagePanel )
	{
		m_pCareerMapImagePanel->SetBorder( pScheme->GetBorder( "InsetBorder" ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns current loading dialog if any
//-----------------------------------------------------------------------------
CLoadingDialog *LoadingDialog()
{
	return g_hLoadingDialog.Get();
}
