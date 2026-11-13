//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#undef PostMessage
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OptionsSubMultiplayer.h"
#include "MultiplayerAdvancedDialog.h"
#include "BitmapImagePanel.h"
#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "CvarSlider.h"
#include "LabeledCommandComboBox.h"
#include "EngineInterface.h"
#include "ModInfo.h"
#include "UtlBuffer.h"
#include "../game_shared/shared_util.h"

#include <FileSystem.h>
#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>

using namespace vgui;

#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))

#define DIB_HEADER_MARKER	((WORD) ('M' << 8) | 'B')

// LogoFile.cpp
extern bool UpdateLogoWAD( void *phdib, int r, int g, int b );
extern bool UpdateLogoWADFromTGA( const char *tga );

struct ColorItem_t
{
	char	*name;
	int		r, g, b;
};

static ColorItem_t itemlist[] =
{
	{ "#Valve_Orange", 255, 120, 24 },
	{ "#Valve_Yellow", 225, 180, 24 },
	{ "#Valve_Blue", 0, 60, 255 },
	{ "#Valve_Ltblue", 0, 167, 255 },
	{ "#Valve_Green", 0, 167, 0 },
	{ "#Valve_Red", 255, 43, 0 },
	{ "#Valve_Brown", 123, 73, 0 },
	{ "#Valve_Ltgray", 100, 100, 100 },
	{ "#Valve_Dkgray", 36, 36, 36 },
};

static ColorItem_t s_crosshairColors_guess[] =
{
	{ "#Valve_Green", 50, 250, 50 },
	{ "#Valve_Red", 250, 50, 50 },
	{ "#Valve_Blue", 50, 50, 250 },
	{ "#Valve_Yellow", 250, 250, 50 },
	{ "#Valve_Ltblue", 50, 250, 250 },
};

//-----------------------------------------------------------------------------
// Purpose: draws the crosshair preview
//-----------------------------------------------------------------------------
class CrosshairImagePanel : public ImagePanel
{
	typedef ImagePanel BaseClass;

public:
	CrosshairImagePanel( Panel *parent, const char *name, CCvarToggleCheckButton *pAdditive );
	virtual void Paint();
	void UpdateCrosshair( int r, int g, int b, int size );

protected:
	int m_R, m_G, m_B;
	int m_barSize;
	int m_barGap;
	CCvarToggleCheckButton *m_pAdditive;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CrosshairImagePanel::CrosshairImagePanel( Panel *parent, const char *name, CCvarToggleCheckButton *pAdditive ) : ImagePanel( parent, name )
{
	m_pAdditive = pAdditive;
	UpdateCrosshair( 50, 250, 50, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: sets the color and the size of the crosshair
//-----------------------------------------------------------------------------
void CrosshairImagePanel::UpdateCrosshair( int r, int g, int b, int size )
{
	m_R = r;
	m_G = g;
	m_B = b;

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	if ( size == 0 )
	{
		if ( screenWide <= 640 )
		{
			// if the screen is small, assume the crosshair is small
			size = 1;
		}
		else if ( screenWide < 1024 )
		{
			size = 2;
		}
		else
		{
			size = 3;
		}
	}

	int scaleBase = 1024;
	switch ( size )
	{
	case 3:
		scaleBase = 640;
		break;
	case 2:
		scaleBase = 800;
		break;
	default:
		scaleBase = 1024;
		break;
	}

	m_barSize = (int)9 * screenWide / scaleBase;
	m_barGap = (int)5 * screenWide / scaleBase;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CrosshairImagePanel::Paint()
{
	BaseClass::Paint();

	if ( !m_pAdditive )
		return;

	int wide, tall;
	GetSize( wide, tall );

	int a = 255;

	bool additive = m_pAdditive->IsSelected();

	if ( additive )
	{
		engine->pfnFillRGBA( ( wide / 2 ) - m_barGap - m_barSize + 1, tall / 2, m_barSize, 1, m_R, m_G, m_B, a );
		engine->pfnFillRGBA( ( wide / 2 ) + m_barGap, tall / 2, m_barSize, 1, m_R, m_G, m_B, a );
		engine->pfnFillRGBA( wide / 2, ( tall / 2 ) - m_barGap - m_barSize, 1, m_barSize, m_R, m_G, m_B, a );
		engine->pfnFillRGBA( wide / 2, ( tall / 2 ) + m_barGap, 1, m_barSize, m_R, m_G, m_B, a );
	}
	else
	{
		engine->pfnFillRGBABlend( ( wide / 2 ) - m_barGap - m_barSize + 1, tall / 2, m_barSize, 1, m_R, m_G, m_B, a );
		engine->pfnFillRGBABlend( ( wide / 2 ) + m_barGap, tall / 2, m_barSize, 1, m_R, m_G, m_B, a );
		engine->pfnFillRGBABlend( wide / 2, ( tall / 2 ) - m_barGap - m_barSize, 1, m_barSize, m_R, m_G, m_B, a );
		engine->pfnFillRGBABlend( wide / 2, ( tall / 2 ) + m_barGap, 1, m_barSize, m_R, m_G, m_B, a );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::COptionsSubMultiplayer( vgui::Panel *parent ) : vgui::PropertyPage( parent, "OptionsSubMultiplayer" )
{
	Button *cancel = new Button( this, "Cancel", "#GameUI_Cancel" );
	cancel->SetCommand( "Close" );

	Button *ok = new Button( this, "OK", "#GameUI_OK" );
	ok->SetCommand( "Ok" );

	Button *apply = new Button( this, "Apply", "#GameUI_Apply" );
	apply->SetCommand( "Apply" );

	Button *advanced = new Button( this, "Advanced", "#GameUI_AdvancedEllipsis" );
	advanced->SetCommand( "Advanced" );

	// player name
	new Label( this, "NameLabel", "#GameUI_PlayerName" );
	m_pNameTextEntry = new CCvarTextEntry( this, "NameEntry", "name" );

	m_pPrimaryColorSlider = new CCvarSlider( this, "Primary Color Slider", "#GameUI_PrimaryColor", 0.0f, 255.0f, "topcolor" );
	m_pSecondaryColorSlider = new CCvarSlider( this, "Secondary Color Slider", "#GameUI_SecondaryColor", 0.0f, 255.0f, "bottomcolor" );
	m_pHighQualityModelCheckBox = new CCvarToggleCheckButton( this, "High Quality Models", "#GameUI_HighModels", "cl_himodels" );

	m_pModelList = new CLabeledCommandComboBox( this, "Player model" );
	InitModelList( m_pModelList );

	m_pLogoList = new CLabeledCommandComboBox( this, "SpraypaintList" );
	m_LogoName[ 0 ] = 0;
	InitLogoList( m_pLogoList );

	m_pColorList = new CLabeledCommandComboBox( this, "SpraypaintColor" );

	const char *currentcolor = engine->pfnGetCvarString( "cl_logocolor" );
	int count = sizeof( itemlist ) / sizeof( itemlist[ 0 ] );
	int selected = 0;
	for ( int i = 0; i < count; i++ )
	{
		if ( currentcolor && !stricmp( currentcolor, itemlist[ i ].name ) )
		{
			selected = i;
		}

		char command[ 256 ];
		sprintf( command, "cl_logocolor %s\n", itemlist[ i ].name );
		m_pColorList->AddItem( itemlist[ i ].name, command );
	}

	m_pColorList->SetInitialItem( selected );
	m_pColorList->AddActionSignalTarget( this );

	m_pModelImage = new CBitmapImagePanel( this, "ModelImage", NULL );
	m_pModelImage->AddActionSignalTarget( this );

	m_pLogoImage = new CBitmapImagePanel( this, "LogoImage", NULL );
	m_pLogoImage->AddActionSignalTarget( this );

	m_nTopColor = 30;
	m_nBottomColor = 6;

	m_pCrosshairImage = NULL;
	m_pCrosshairColorComboBox = NULL;
	m_pCrosshairSize = NULL;
	m_pCrosshairTranslucencyCheckbox = NULL;
	m_displayedCrosshairColorRed = -1;
	m_displayedCrosshairColorGreen = -1;
	m_displayedCrosshairColorBlue = -1;

	// the crosshair settings are only in the Counter-Strike games
	if ( !stricmp( gEngfuncs.pfnGetGameDirectory(), "czero" ) ||
		 !stricmp( gEngfuncs.pfnGetGameDirectory(), "terror" ) ||
		 !stricmp( gEngfuncs.pfnGetGameDirectory(), "cstrike" ) ||
		 !stricmp( gEngfuncs.pfnGetGameDirectory(), "cstrike_beta" ) )
	{
		m_displayedCrosshairColorRed = 50;
		m_displayedCrosshairColorGreen = 250;
		m_displayedCrosshairColorBlue = 50;

		m_pCrosshairColorComboBox = new ComboBox( this, "CrosshairColorComboBox", 6, false );
		InitCrosshairColorEntries();

		m_pCrosshairSize = new CLabeledCommandComboBox( this, "CrosshairSizeComboBox" );
		m_pCrosshairTranslucencyCheckbox = new CCvarToggleCheckButton( this, "CrosshairTranslucencyCheckbox", "#GameUI_Translucent", "cl_crosshair_translucent" );
		InitCrosshairSizeList( m_pCrosshairSize );

		m_pCrosshairImage = new CrosshairImagePanel( this, "CrosshairImage", m_pCrosshairTranslucencyCheckbox );
		RedrawCrosshairImage();
	}

	m_nLogoR = 255;
	m_nLogoG = 255;
	m_nLogoB = 255;

	LoadControlSettings( "Resource/OptionsSubMultiplayer.res" );

	// turn off model selection stuff if the mod specifies "nomodels" in the liblist.gam file
	if ( ModInfo().NoModels() )
	{
		Panel *pTempPanel = NULL;

		if ( m_pModelImage )
		{
			m_pModelImage->SetVisible( false );
		}

		if ( m_pModelList )
		{
			m_pModelList->SetVisible( false );
		}

		if ( m_pPrimaryColorSlider )
		{
			m_pPrimaryColorSlider->SetVisible( false );
		}

		if ( m_pSecondaryColorSlider )
		{
			m_pSecondaryColorSlider->SetVisible( false );
		}

		// get rid of the label for the player model
		pTempPanel = FindChildByName( "Label1" );
		if ( pTempPanel )
		{
			pTempPanel->SetVisible( false );
		}

		pTempPanel = FindChildByName( "Colors" );
		if ( pTempPanel )
		{
			pTempPanel->SetVisible( false );
		}
	}

	// turn off the himodel stuff if the mod specifies "nohimodel" in the liblist.gam file
	if ( ModInfo().NoHiModel() )
	{
		if ( m_pHighQualityModelCheckBox )
		{
			m_pHighQualityModelCheckBox->SetVisible( false );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::~COptionsSubMultiplayer()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnCommand( const char *command )
{
	if ( !stricmp( command, "Advanced" ) )
	{
		if ( !m_hMultiplayerAdvancedDialog.Get() )
		{
			m_hMultiplayerAdvancedDialog = new CMultiplayerAdvancedDialog( this );
		}

		m_hMultiplayerAdvancedDialog->Activate();
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Builds the list of logos
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitLogoList( CLabeledCommandComboBox *cb )
{
	// Find out images
	FileFindHandle_t fh;
	char directory[ 512 ];

	filesystem()->RemoveFile( "logos/remapped.bmp", NULL );

	const char *logofile = engine->pfnGetCvarString( "cl_logofile" );
	int i = 0;
	int iItem = 0;

	sprintf( directory, "logos/*.bmp" );
	const char *fn = filesystem()->FindFirst( directory, &fh, NULL );
	while ( fn )
	{
		if ( stricmp( fn, "remapped.bmp" ) )
		{
			if ( fn[ 0 ] && fn[ 0 ] != '.' )
			{
				char filename[ 512 ];
				strcpy( filename, fn );
				if ( strlen( filename ) >= 4 )
				{
					filename[ strlen( filename ) - 4 ] = 0;
				}

				if ( !stricmp( filename, logofile ) )
				{
					iItem = i;
				}

				cb->AddItem( filename, "" );
			}

			i++;
		}

		fn = filesystem()->FindNext( fh );
	}

	filesystem()->FindClose( fh );

	cb->SetInitialItem( iItem );
}

//-----------------------------------------------------------------------------
// Purpose: Builds model list
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitModelList( CLabeledCommandComboBox *cb )
{
	// Find out images
	char cmdstring[ 256 ];
	char currentmodel[ 64 ];

	filesystem()->RemoveFile( "models/player/remapped.bmp", NULL );

	int c = 0;
	int selected = 0;

	char *p = engine->pfnGetCvarString( "model" );
	currentmodel[ 0 ] = 0;
	if ( p )
	{
		strcpy( currentmodel, p );
	}

	FileFindHandle_t findHandle;
	const char *filename;
	for ( filename = filesystem()->FindFirst( "models/player/*.*", &findHandle, NULL ); filename; filename = filesystem()->FindNext( findHandle ) )
	{
		if ( filesystem()->FindIsDirectory( findHandle ) && filename[ 0 ] != '.' )
		{
			_snprintf( cmdstring, sizeof( cmdstring ), "model %s\n", filename );

			if ( !stricmp( currentmodel, filename ) )
			{
				selected = c;
			}

			cb->AddItem( filename, cmdstring );
			c++;
		}
	}

	filesystem()->FindClose( findHandle );

	cb->SetInitialItem( selected );
}

//-----------------------------------------------------------------------------
// Purpose: fills the crosshair color combo box
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitCrosshairColorEntries()
{
	// parse out the string for the crosshair color
	int r = 50;
	int g = 250;
	int b = 50;

	const char *val = engine->pfnGetCvarString( "cl_crosshair_color" );
	if ( val )
	{
		val = SharedParse( val );
		const char *token = SharedGetToken();
		if ( token )
		{
			r = atoi( token );
			val = SharedParse( val );
			token = SharedGetToken();
		}

		if ( token )
		{
			g = atoi( token );
			val = SharedParse( val );
			token = SharedGetToken();
		}

		if ( token )
		{
			b = atoi( token );
		}
	}

	if ( m_pCrosshairColorComboBox )
	{
		int selection = 0;
		KeyValues *data = new KeyValues( "data" );
		data->Clear();

		int count = sizeof( s_crosshairColors_guess ) / sizeof( s_crosshairColors_guess[ 0 ] );
		int selected = 0;
		for ( int i = 0; i < count; i++ )
		{
			data->SetInt( "red", s_crosshairColors_guess[ i ].r );
			data->SetInt( "green", s_crosshairColors_guess[ i ].g );
			data->SetInt( "blue", s_crosshairColors_guess[ i ].b );

			m_pCrosshairColorComboBox->AddItem( s_crosshairColors_guess[ i ].name, data );

			if ( r == s_crosshairColors_guess[ i ].r && g == s_crosshairColors_guess[ i ].g && b == s_crosshairColors_guess[ i ].b )
			{
				selection = i;
			}
		}

		m_pCrosshairColorComboBox->ActivateItemByRow( selection );
		data->deleteThis();
	}

	RedrawCrosshairImage();
}

//-----------------------------------------------------------------------------
// Purpose: updates the crosshair preview from the color and size controls
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RedrawCrosshairImage()
{
	if ( m_pCrosshairColorComboBox == NULL )
		return;

	bool enableApplyButton = false;

	KeyValues *data = m_pCrosshairColorComboBox->GetActiveItemUserData();
	int r = data->GetInt( "red", 0 );
	int g = data->GetInt( "green", 0 );
	int b = data->GetInt( "blue", 0 );

	if ( r != m_displayedCrosshairColorRed || g != m_displayedCrosshairColorGreen || b != m_displayedCrosshairColorBlue )
	{
		enableApplyButton = true;
	}

	m_displayedCrosshairColorRed = r;
	m_displayedCrosshairColorGreen = g;
	m_displayedCrosshairColorBlue = b;

	if ( enableApplyButton )
	{
		OnApplyButtonEnable();
	}

	if ( m_pCrosshairImage && m_pCrosshairSize )
	{
		int size = m_pCrosshairSize->GetActiveItem();
		m_pCrosshairImage->UpdateCrosshair( m_displayedCrosshairColorRed, m_displayedCrosshairColorGreen, m_displayedCrosshairColorBlue, size );
	}
}

//-----------------------------------------------------------------------------
// Purpose: fills the crosshair size combo box
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::InitCrosshairSizeList( CLabeledCommandComboBox *cb )
{
	if ( cb == NULL )
		return;

	cb->Reset();

	cb->AddItem( "#GameUI_Auto", "cl_crosshair_size auto" );
	cb->AddItem( "#GameUI_Small", "cl_crosshair_size small" );
	cb->AddItem( "#GameUI_Medium", "cl_crosshair_size medium" );
	cb->AddItem( "#GameUI_Large", "cl_crosshair_size large" );

	char *val = engine->pfnGetCvarString( "cl_crosshair_size" );
	if ( val == NULL || !stricmp( "auto", val ) )
	{
		cb->SetInitialItem( 0 );
	}
	else if ( !stricmp( "small", val ) )
	{
		cb->SetInitialItem( 1 );
	}
	else if ( !stricmp( "medium", val ) )
	{
		cb->SetInitialItem( 2 );
	}
	else if ( !stricmp( "large", val ) )
	{
		cb->SetInitialItem( 3 );
	}
	else
	{
		cb->SetInitialItem( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: shows the selected logo in its color
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapLogo()
{
	char logoname[ 256 ];

	m_pLogoList->GetText( logoname, sizeof( logoname ) );
	if ( !logoname[ 0 ] )
		return;

	char texture[ 256 ];
	sprintf( texture, "logos/%s.tga", logoname );

	if ( filesystem()->FileExists( texture ) )
	{
		sprintf( texture, "logos/%s", logoname );

		m_pColorList->SetEnabled( false );
	}
	else
	{
		sprintf( texture, "logos/remapped" );

		const char *colorname = m_pColorList->GetActiveItemCommand();
		if ( !colorname || !colorname[ 0 ] )
			return;

		colorname += strlen( "cl_logocolor " );

		int r, g, b;
		ColorForName( colorname, r, g, b );

		RemapLogoPalette( logoname, r, g, b );

		m_pColorList->SetEnabled( true );
	}

	m_pLogoImage->setTexture( texture );
}

//-----------------------------------------------------------------------------
// Purpose: shows the selected model in its colors
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapModel()
{
	char modelname[ 256 ];

	m_pModelList->GetText( modelname, sizeof( modelname ) );
	if ( !modelname[ 0 ] )
		return;

	char texture[ 256 ];
	sprintf( texture, "models/player/remapped" );

	RemapPalette( modelname, m_nTopColor, m_nBottomColor );

	m_pModelImage->setTexture( texture );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnTextChanged( vgui::Panel *panel )
{
	if ( panel == m_pNameTextEntry )
		return;

	RemapModel();
	RemapLogo();
	RedrawCrosshairImage();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnSliderMoved( KeyValues *data )
{
	m_nTopColor = (int)m_pPrimaryColorSlider->GetSliderValue();
	m_nBottomColor = (int)m_pSecondaryColorSlider->GetSliderValue();

	RemapModel();
	RedrawCrosshairImage();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnApplyButtonEnable()
{
	PostMessage( GetParent(), new KeyValues( "ApplyButtonEnable" ) );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: replaces the hue of a range of palette entries
//-----------------------------------------------------------------------------
static void PaletteHueReplace( RGBQUAD *palSrc, int newHue, int Start, int end )
{
	int i;
	float r, b, g;
	float maxcol, mincol;
	float hue, val, sat;

	hue = (float)( newHue * ( 360.0 / 255 ) );

	for ( i = Start; i <= end; i++ )
	{
		b = palSrc[ i ].rgbBlue;
		g = palSrc[ i ].rgbGreen;
		r = palSrc[ i ].rgbRed;

		maxcol = max( max( r, g ), b ) / 255.0f;
		mincol = min( min( r, g ), b ) / 255.0f;

		val = maxcol;
		sat = ( maxcol - mincol ) / maxcol;

		mincol = val * ( 1.0f - sat );

		if ( hue <= 120 )
		{
			b = mincol;
			if ( hue < 60 )
			{
				r = val;
				g = mincol + hue * ( val - mincol ) / ( 120 - hue );
			}
			else
			{
				g = val;
				r = mincol + ( 120 - hue ) * ( val - mincol ) / hue;
			}
		}
		else if ( hue <= 240 )
		{
			r = mincol;
			if ( hue < 180 )
			{
				g = val;
				b = mincol + ( hue - 120 ) * ( val - mincol ) / ( 240 - hue );
			}
			else
			{
				b = val;
				g = mincol + ( 240 - hue ) * ( val - mincol ) / ( hue - 120 );
			}
		}
		else
		{
			g = mincol;
			if ( hue < 300 )
			{
				b = val;
				r = mincol + ( hue - 240 ) * ( val - mincol ) / ( 360 - hue );
			}
			else
			{
				r = val;
				b = mincol + ( 360 - hue ) * ( val - mincol ) / ( hue - 240 );
			}
		}

		palSrc[ i ].rgbBlue = (unsigned char)( b * 255 );
		palSrc[ i ].rgbGreen = (unsigned char)( g * 255 );
		palSrc[ i ].rgbRed = (unsigned char)( r * 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes models/player/remapped.bmp with the player colors
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapPalette( char *filename, int topcolor, int bottomcolor )
{
	char infile[ 256 ];
	char outfile[ 256 ];

	FileHandle_t file;
	CUtlBuffer outbuffer( 20480, 20480, false );

	sprintf( infile, "models/player/%s/%s.bmp", filename, filename );
	sprintf( outfile, "models/player/remapped.bmp" );

	file = filesystem()->Open( infile, "rb" );
	if ( !file )
		return;

	// Parse bitmap
	BITMAPFILEHEADER bmfHeader;
	DWORD dwBitsSize, dwFileSize;
	LPBITMAPINFO lpbmi;

	dwFileSize = filesystem()->Size( file );

	filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );

	outbuffer.Put( &bmfHeader, sizeof( bmfHeader ) );

	if ( bmfHeader.bfType == DIB_HEADER_MARKER )
	{
		dwBitsSize = dwFileSize - sizeof( bmfHeader );

		HGLOBAL hDIB = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize );
		char *pDIB = (LPSTR)GlobalLock( (HGLOBAL)hDIB );
		{
			filesystem()->Read( pDIB, dwBitsSize, file );

			lpbmi = (LPBITMAPINFO)pDIB;

			// Remap palette
			PaletteHueReplace( lpbmi->bmiColors, topcolor, 0xC0, 0xDF );
			PaletteHueReplace( lpbmi->bmiColors, bottomcolor, 0xA0, 0xBF );

			outbuffer.Put( pDIB, dwBitsSize );
		}

		GlobalUnlock( hDIB );
		GlobalFree( (HGLOBAL)hDIB );
	}

	filesystem()->Close( file );

	filesystem()->RemoveFile( outfile, NULL );
	filesystem()->CreateDirHierarchy( "models/player", NULL );

	file = filesystem()->Open( outfile, "wb" );
	if ( file )
	{
		filesystem()->Write( outbuffer.Base(), outbuffer.TellPut(), file );
		filesystem()->Close( file );
	}
}

//-----------------------------------------------------------------------------
// Purpose: color of a logo color name
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::ColorForName( char const *pszColorName, int &r, int &g, int &b )
{
	r = g = b = 0;

	int count = sizeof( itemlist ) / sizeof( itemlist[ 0 ] );

	for ( int i = 0; i < count; i++ )
	{
		if ( !_strnicmp( pszColorName, itemlist[ i ].name, strlen( itemlist[ i ].name ) ) )
		{
			r = itemlist[ i ].r;
			g = itemlist[ i ].g;
			b = itemlist[ i ].b;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes logos/remapped.bmp in the logo color
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapLogoPalette( char *filename, int r, int g, int b )
{
	char infile[ 256 ];

	FileHandle_t file;
	CUtlBuffer outbuffer( 20480, 20480, false );

	sprintf( infile, "logos/%s.bmp", filename );

	file = filesystem()->Open( infile, "rb" );
	if ( !file )
		return;

	// Parse bitmap
	BITMAPFILEHEADER bmfHeader;
	LPBITMAPCOREINFO lpCoreInfo_guess;
	DWORD dwBitsSize, dwFileSize;
	LPBITMAPINFO lpbmi;

	dwFileSize = filesystem()->Size( file );

	filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );

	outbuffer.Put( &bmfHeader, sizeof( bmfHeader ) );

	if ( bmfHeader.bfType == DIB_HEADER_MARKER )
	{
		dwBitsSize = dwFileSize - sizeof( bmfHeader );

		HGLOBAL hDIB = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize );
		char *pDIB = (LPSTR)GlobalLock( (HGLOBAL)hDIB );
		{
			filesystem()->Read( pDIB, dwBitsSize, file );

			lpbmi = (LPBITMAPINFO)pDIB;
			lpCoreInfo_guess = (LPBITMAPCOREINFO)pDIB;

			bool bWinStyle_guess = true;

			// Remap palette
			float f = 0.0f;
			for ( int i = 0; i < 256; i++ )
			{
				float t = f / 256.0f;
				if ( bWinStyle_guess )
				{
					lpbmi->bmiColors[ i ].rgbRed = (unsigned char)( r * t );
					lpbmi->bmiColors[ i ].rgbGreen = (unsigned char)( g * t );
					lpbmi->bmiColors[ i ].rgbBlue = (unsigned char)( b * t );
				}
				else
				{
					lpCoreInfo_guess->bmciColors[ i ].rgbtRed = (unsigned char)( r * t );
					lpCoreInfo_guess->bmciColors[ i ].rgbtGreen = (unsigned char)( g * t );
					lpCoreInfo_guess->bmciColors[ i ].rgbtBlue = (unsigned char)( b * t );
				}

				f += 1.0f;
			}

			outbuffer.Put( pDIB, dwBitsSize );
		}

		GlobalUnlock( hDIB );
		GlobalFree( (HGLOBAL)hDIB );
	}

	filesystem()->Close( file );

	char outfile[ 256 ];
	sprintf( outfile, "logos/remapped.bmp" );

	filesystem()->RemoveFile( outfile, NULL );
	filesystem()->CreateDirHierarchy( "logos", NULL );

	file = filesystem()->Open( outfile, "wb" );
	if ( file )
	{
		filesystem()->Write( outbuffer.Base(), outbuffer.TellPut(), file );
		filesystem()->Close( file );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnResetData()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnApplyChanges()
{
	m_pPrimaryColorSlider->ApplyChanges();
	m_pSecondaryColorSlider->ApplyChanges();

	m_pModelList->ApplyChanges();
	m_pLogoList->ApplyChanges();
	m_pLogoList->GetText( m_LogoName, sizeof( m_LogoName ) );
	m_pColorList->ApplyChanges();
	m_pHighQualityModelCheckBox->ApplyChanges();
	m_pNameTextEntry->ApplyChanges();

	if ( m_pCrosshairSize != NULL )
	{
		m_pCrosshairSize->ApplyChanges();
	}

	if ( m_pCrosshairTranslucencyCheckbox != NULL )
	{
		m_pCrosshairTranslucencyCheckbox->ApplyChanges();
	}

	ApplyCrosshairColorChanges();

	// save the logo name
	const char *colorname = m_pColorList->GetActiveItemCommand();
	if ( !colorname || !colorname[ 0 ] )
		return;

	colorname += strlen( "cl_logocolor " );

	char cmd[ 512 ];
	_snprintf( cmd, sizeof( cmd ) - 1, "cl_logofile %s\n", m_LogoName );
	engine->pfnClientCmd( cmd );

	// get the path to the logo
	char infile[ 256 ];
	sprintf( infile, "logos/%s.tga", m_LogoName );

	if ( filesystem()->FileExists( infile ) )
	{
		UpdateLogoWADFromTGA( infile );
	}
	else
	{
		int r, g, b;
		ColorForName( colorname, r, g, b );

		sprintf( infile, "logos/remapped.bmp" );

		FileHandle_t file = filesystem()->Open( infile, "rb" );
		if ( !file )
			return;

		// Parse bitmap
		BITMAPFILEHEADER bmfHeader;
		DWORD dwBitsSize, dwFileSize;

		dwFileSize = filesystem()->Size( file );
		filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );

		if ( bmfHeader.bfType == DIB_HEADER_MARKER )
		{
			dwBitsSize = dwFileSize - sizeof( bmfHeader );

			HGLOBAL hDIB = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, dwBitsSize );
			char *pDIB = (LPSTR)GlobalLock( (HGLOBAL)hDIB );

			filesystem()->Read( pDIB, dwBitsSize, file );

			GlobalUnlock( hDIB );

			UpdateLogoWAD( (void *)hDIB, r, g, b );

			GlobalFree( (HGLOBAL)hDIB );
		}

		filesystem()->Close( file );
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the displayed crosshair color into the cvar
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::ApplyCrosshairColorChanges()
{
	char cmd[ 256 ];
	cmd[ 0 ] = 0;

	if ( m_displayedCrosshairColorRed == -1 || m_displayedCrosshairColorGreen == -1 || m_displayedCrosshairColorBlue == -1 )
		return;

	sprintf( cmd, "%d %d %d", m_displayedCrosshairColorRed, m_displayedCrosshairColorGreen, m_displayedCrosshairColorBlue );
	engine->Cvar_Set( "cl_crosshair_color", cmd );
}
