//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "OptionsSubVideo.h"
#include "CvarSlider.h"
#include "EngineInterface.h"
#include "igameuifuncs.h"
#include "modes.h"

#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>

using namespace vgui;

inline bool IsWideScreen( int width, int height )
{
	// 16:9 or 16:10 is widescreen :)
	if ( ( width * 9 ) == ( height * 16.0f ) || ( width * 5.0 ) == ( height * 8.0 ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo( vgui::Panel *parent ) : PropertyPage( parent, NULL )
{
	memset( &m_OrigSettings, 0, sizeof( m_OrigSettings ) );
	memset( &m_CurrentSettings, 0, sizeof( m_CurrentSettings ) );

	m_pBrightnessSlider = new CCvarSlider( this, "Brightness", "#GameUI_Brightness", 0.0f, 0.3f, "gl_brightness" );
	m_pGammaSlider = new CCvarSlider( this, "Gamma", "#GameUI_Gamma", 0.0f, 2.0f, "gl_gamma" );

	GetVidSettings();

	m_pMode = new ComboBox( this, "Resolution", 6, false );

	m_pAspectRatio = new ComboBox( this, "AspectRatio", 2, false );

	wchar_t *unicodeText = localize()->Find( "#GameUI_AspectNormal" );
	localize()->ConvertUnicodeToANSI( unicodeText, m_pszAspectName[0], 32 );
	unicodeText = localize()->Find( "#GameUI_AspectWide" );
	localize()->ConvertUnicodeToANSI( unicodeText, m_pszAspectName[1], 32 );

	int iNormalItemID = m_pAspectRatio->AddItem( m_pszAspectName[0], NULL );
	int iWideItemID = m_pAspectRatio->AddItem( m_pszAspectName[1], NULL );

	m_bStartWidescreen = IsWideScreen( m_CurrentSettings.w, m_CurrentSettings.h );
	if ( m_bStartWidescreen )
	{
		m_pAspectRatio->ActivateItem( iWideItemID );
	}
	else
	{
		m_pAspectRatio->ActivateItem( iNormalItemID );
	}

	m_pWindowed = new ComboBox( this, "Windowed", 2, false );
	m_pWindowed->AddItem( "Run in a window", NULL );
	m_pWindowed->AddItem( "Full screen", NULL );

	SetCurrentRendererComboItem();

	LoadControlSettings( "Resource\\OptionsSubVideo.res" );

	PrepareResolutionList();
}

//-----------------------------------------------------------------------------
// Purpose: Generates resolution list
//-----------------------------------------------------------------------------
void COptionsSubVideo::PrepareResolutionList()
{
	vmode_t *plist = NULL;
	int count = 0;
	bool bFoundWidescreen = false;

	gameuifuncs->GetVideoModes( &plist, &count );

	// Clean up before filling the info again.
	m_pMode->DeleteAllItems();

	int selectedItemID = -1;
	for ( int i = 0; i < count; i++, plist++ )
	{
		char sz[ 256 ];
		sprintf( sz, "%i x %i", plist->width, plist->height );

		int itemID = -1;
		if ( IsWideScreen( plist->width, plist->height ) )
		{
			if ( m_bStartWidescreen == true )
			{
				itemID = m_pMode->AddItem( sz, NULL );
			}

			bFoundWidescreen = true;
		}
		else
		{
			if ( !m_bStartWidescreen )
			{
				itemID = m_pMode->AddItem( sz, NULL );
			}
		}

		// try and find the best match for the resolution to be selected
		if ( plist->width == m_CurrentSettings.w && plist->height == m_CurrentSettings.h )
		{
			selectedItemID = itemID;
		}
	}

	// disable ratio selection if we can't display widescreen.
	m_pAspectRatio->SetEnabled( bFoundWidescreen );

	if ( selectedItemID != -1 )
	{
		m_pMode->ActivateItem( selectedItemID );
	}
	else
	{
		m_pMode->ActivateItem( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
	RevertVidSettings();

	m_pBrightnessSlider->Reset();
	m_pGammaSlider->Reset();

	SetCurrentRendererComboItem();
	SetCurrentResolutionComboItem();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentRendererComboItem()
{
	m_iStartRenderer = 1;

	if ( m_CurrentSettings.windowed )
	{
		m_pWindowed->ActivateItemByRow( 0 );
	}
	else
	{
		m_pWindowed->ActivateItemByRow( 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
	vmode_t *plist = NULL;
	int count = 0;
	gameuifuncs->GetVideoModes( &plist, &count );

	int resolution = -1;
	for ( int i = 0; i < count; i++, plist++ )
	{
		if ( plist->width == m_CurrentSettings.w &&
			 plist->height == m_CurrentSettings.h )
		{
			resolution = i;
			break;
		}
	}

	if ( resolution != -1 )
	{
		char sz[ 256 ];
		sprintf( sz, "%i x %i", plist->width, plist->height );
		m_pMode->SetText( sz );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{
	bool bChanged = m_pBrightnessSlider->HasBeenModified() || m_pGammaSlider->HasBeenModified();

	m_pBrightnessSlider->ApplyChanges();
	m_pGammaSlider->ApplyChanges();

	ApplyVidSettings( bChanged );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::GetVidSettings()
{
	// Get original settings
	CVidSettings *p = &m_OrigSettings;

	gameuifuncs->GetCurrentVideoMode( &p->w, &p->h, &p->bpp );
	gameuifuncs->GetCurrentRenderer( p->renderer, 128, &p->windowed );
	strlwr( p->renderer );

	m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::RevertVidSettings()
{
	m_CurrentSettings = m_OrigSettings;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::ApplyVidSettings( bool bForceRefresh )
{
	// Retrieve text from active controls and parse out strings
	if ( m_pMode )
	{
		char sz[ 256 ];
		m_pMode->GetText( sz, 256 );

		int w, h;
		sscanf( sz, "%i x %i", &w, &h );
		m_CurrentSettings.w = w;
		m_CurrentSettings.h = h;
		m_CurrentSettings.bpp = 32;
	}

	if ( m_pWindowed )
	{
		char sz[ 256 ];
		m_pWindowed->GetText( sz, 256 );

		if ( strstr( sz, "window" ) )
		{
			m_CurrentSettings.windowed = 1;
		}
		else
		{
			m_CurrentSettings.windowed = 0;
		}
	}

	if ( memcmp( &m_OrigSettings, &m_CurrentSettings, sizeof( CVidSettings ) ) == 0 && !bForceRefresh )
	{
		return;
	}

	CVidSettings *p = &m_CurrentSettings;

	char szCmd[ 256 ];
	// Set mode
	sprintf( szCmd, "_setvideomode %i %i %i\n", p->w, p->h, p->bpp );
	engine->pfnClientCmd( szCmd );

	// Set renderer
	sprintf( szCmd, "_setrenderer %s %s\n", p->renderer, p->windowed ? "windowed" : "fullscreen" );
	engine->pfnClientCmd( szCmd );

	// Force restart of entire engine
	engine->pfnFilteredClientCmd();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnButtonChecked( KeyValues *data )
{
	int state = data->GetInt( "state", 0 );
	Panel *pPanel = (Panel *)data->GetPtr( "panel", NULL );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged( Panel *pPanel, const char *pszText )
{
	if ( pPanel == m_pMode )
	{
		char sz[ 256 ];
		sprintf( sz, "%i x %i", m_CurrentSettings.w, m_CurrentSettings.h );

		if ( strcmp( pszText, sz ) )
		{
			OnDataChanged();
		}
	}
	else if ( pPanel == m_pAspectRatio )
	{
		if ( strcmp( pszText, m_pszAspectName[ m_bStartWidescreen ] ) )
		{
			m_bStartWidescreen = !m_bStartWidescreen;
			PrepareResolutionList();
		}
	}
	else if ( pPanel == m_pWindowed )
	{
		int windowed_guess;
		char sz[ 256 ];
		m_pWindowed->GetText( sz, 256 );

		if ( strstr( sz, "window" ) )
		{
			windowed_guess = 1;
		}
		else
		{
			windowed_guess = 0;
		}

		if ( windowed_guess != m_CurrentSettings.windowed )
		{
			OnDataChanged();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}
