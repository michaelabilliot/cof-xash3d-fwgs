//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Base classes for the career screens
//
// $NoKeywords: $
//=============================================================================

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CareerBaseFrame.h"
#include "CareerDefinitions.h"
#include "CareerGame.h"
#include "EngineInterface.h"
#include "Taskbar.h"
#include "../game_shared/shared_util.h"

#include <vgui/IBorder.h>
#include <vgui/IImage.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/BitmapImagePanel.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ScrollBar.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Gives the scroll bar the browser border
//-----------------------------------------------------------------------------
void CCareerPanelListPanel::ApplySchemeSettings( IScheme *pScheme )
{
	CPanelListPanel::ApplySchemeSettings( pScheme );
	GetScrollbar()->SetBorder( pScheme->GetBorder( "BrowserBorder" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
ColorFade::ColorFade()
{
	m_startColor = m_endColor = Color( 0, 0, 0, 0 );
	m_fadePercent = 0.0f;
	m_direction = down;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the colors of the fade
//-----------------------------------------------------------------------------
void ColorFade::SetFade( Color startColor, Color endColor, float fadePercent, FadeDir direction )
{
	m_startColor = startColor;
	m_endColor = endColor;
	m_fadePercent = fadePercent;
	m_direction = direction;
}

//-----------------------------------------------------------------------------
// Purpose: Fills the rectangle with the start color
//-----------------------------------------------------------------------------
void ColorFade::DrawFade( int x, int y, int w, int h )
{
	surface()->DrawSetColor( m_startColor );
	surface()->DrawFilledRect( x, y, x + w, y + h );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerButton::CCareerButton( Panel *parent, const char *buttonName, const char *buttonText, const char *image, bool textFirst ) : Button( parent, buttonName, "" )
{
	m_armedBorder = NULL;
	m_textImage = new TextImage( buttonText );
	m_image = scheme()->GetImage( image, true );
	m_textFirst = textFirst;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the image and the text
//-----------------------------------------------------------------------------
void CCareerButton::Paint()
{
	int buttonWide, buttonTall;
	GetSize( buttonWide, buttonTall );

	int imageWide, imageTall;
	m_image->GetSize( imageWide, imageTall );

	int textOffset = m_textPad;

	if ( m_textFirst )
	{
		m_image->SetPos( buttonWide - imageWide - m_imagePad, (buttonTall - imageTall)/2 );
		m_image->Paint();
	}
	else
	{
		m_image->SetPos( m_imagePad, (buttonTall - imageTall)/2 );
		m_image->Paint();
		textOffset += imageWide + m_imagePad;
	}

	int textTall, textWide;
	m_textImage->GetSize( textWide, textTall );

	int textSpace = buttonWide - imageWide - m_imagePad - 2*m_textPad;

	if ( IsEnabled() )
	{
		m_textImage->SetColor( m_textNormalColor );
	}
	else
	{
		m_textImage->SetColor( m_textDisabledColor );
	}
	m_textImage->SetPos( textOffset + (textSpace - textWide)/2, (buttonTall - textTall)/2 );
	m_textImage->Paint();

	if ( HasFocus() && IsEnabled() )
	{
		int x0, y0, x1, y1;
		x0 = 3, y0 = 3;
		x1 = buttonWide - 4;
		y1 = buttonTall - 2;
		DrawFocusBorder( x0, y0, x1, y1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the border used while the mouse is over the button
//-----------------------------------------------------------------------------
void CCareerButton::SetArmedBorder( IBorder *border )
{
	m_armedBorder = border;
	InvalidateLayout( false );
}

//-----------------------------------------------------------------------------
// Purpose: Uses the armed border while the mouse is over the button
//-----------------------------------------------------------------------------
IBorder *CCareerButton::GetBorder( bool depressed, bool armed, bool selected, bool keyfocus )
{
	if ( armed && !depressed && IsEnabled() )
	{
		return m_armedBorder;
	}

	return BaseClass::GetBorder( depressed, armed, selected, keyfocus );
}

//-----------------------------------------------------------------------------
// Purpose: Borders, colors, pads and the size of the text
//-----------------------------------------------------------------------------
void CCareerButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetDefaultBorder( pScheme->GetBorder( "CareerButtonBorder" ) );
	SetDepressedBorder( pScheme->GetBorder( "CareerButtonDepressedBorder" ) );
	m_armedBorder = pScheme->GetBorder( "CareerButtonArmedBorder" );

	m_textNormalColor = pScheme->GetColor( "BrightBaseText", Color( 0, 0, 0, 0 ) );
	m_textDisabledColor = pScheme->GetColor( "DisabledText1", Color( 0, 0, 0, 0 ) );
	m_textImage->SetColor( m_textNormalColor );
	m_image->SetColor( GetFgColor() );
	m_textImage->SetFont( pScheme->GetFont( "Default", IsProportional() ) );

	m_textPad = atoi( pScheme->GetResourceString( "CareerButtonTextPad" ) );
	m_imagePad = atoi( pScheme->GetResourceString( "CareerButtonImagePad" ) );
	if ( IsProportional() )
	{
		m_textPad = scheme()->GetProportionalScaledValue( m_textPad );
		m_imagePad = scheme()->GetProportionalScaledValue( m_imagePad );
	}

	const int BufLen = 128;
	char buf[BufLen];
	GetText( buf, BufLen );
	m_textImage->SetText( buf );
	m_textImage->ResizeImageToContent();

	int buttonWide, buttonTall;
	GetSize( buttonWide, buttonTall );

	int imageWide, imageTall;
	m_image->GetSize( imageWide, imageTall );

	int textSpace = buttonWide - imageWide - m_imagePad - 2*m_textPad;

	int textWide, textTall;
	m_textImage->GetContentSize( textWide, textTall );
	if ( textSpace < textWide )
	{
		m_textImage->SetSize( textSpace, textTall );
	}

	Color bgColor = pScheme->GetColor( "CareerButtonBG", Color( 0, 0, 0, 0 ) );
	SetDefaultColor( bgColor, bgColor );
	SetArmedColor( bgColor, bgColor );
	SetDepressedColor( bgColor, bgColor );
}

//-----------------------------------------------------------------------------
// Purpose: A label with rounded corners for the autosave notice
//-----------------------------------------------------------------------------
class CAutosaveLabel : public Label
{
public:
	CAutosaveLabel( Panel *parent, const char *panelName, const char *text );

	virtual Color GetBgColor();
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PaintBackground();
	virtual void PerformLayout();

protected:
	Color m_bgColor;
	CBitmapImagePanel *m_pTopLeftPanel;
	CBitmapImagePanel *m_pTopRightPanel;
	CBitmapImagePanel *m_pBottomLeftPanel;
	CBitmapImagePanel *m_pBottomRightPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CAutosaveLabel::CAutosaveLabel( Panel *parent, const char *panelName, const char *text ) : Label( parent, panelName, text )
{
	m_bgColor = Color( 0, 0, 0, 0 );

	m_pTopLeftPanel = new CBitmapImagePanel( this, "TopLeftPanel", "gfx/career/round_corner_nw" );
	m_pTopRightPanel = new CBitmapImagePanel( this, "TopRightPanel", "gfx/career/round_corner_ne" );
	m_pBottomLeftPanel = new CBitmapImagePanel( this, "BottomLeftPanel", "gfx/career/round_corner_sw" );
	m_pBottomRightPanel = new CBitmapImagePanel( this, "BottomRightPanel", "gfx/career/round_corner_se" );
}

//-----------------------------------------------------------------------------
// Purpose: Background color and corner images
//-----------------------------------------------------------------------------
void CAutosaveLabel::ApplySchemeSettings( IScheme *pScheme )
{
	Label::ApplySchemeSettings( pScheme );

	m_bgColor = GetSchemeColor( "AutosaveNoticeLabelBG", GetSchemeColor( "ControlBG", pScheme ), pScheme );

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
// Purpose: Places the corner images
//-----------------------------------------------------------------------------
void CAutosaveLabel::PerformLayout()
{
	int x, y, w, h;
	GetBounds( x, y, w, h );

	int textWide, textTall;
	GetContentSize( textWide, textTall );

	const int cornerSize = 20;
	m_pTopLeftPanel->SetBounds( 0, 0, cornerSize, cornerSize );
	m_pTopRightPanel->SetBounds( w - cornerSize, 0, cornerSize, cornerSize );
	m_pBottomLeftPanel->SetBounds( 0, h - cornerSize, cornerSize, cornerSize );
	m_pBottomRightPanel->SetBounds( w - cornerSize, h - cornerSize, cornerSize, cornerSize );
}

//-----------------------------------------------------------------------------
// Purpose: Fills the label between the rounded corners
//-----------------------------------------------------------------------------
void CAutosaveLabel::PaintBackground()
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
// Purpose: The label draws its own background
//-----------------------------------------------------------------------------
Color CAutosaveLabel::GetBgColor()
{
	return m_bgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCareerBaseFrame::CCareerBaseFrame( Panel *parent, const char *panelName, bool showTaskbarIcon ) : Frame( parent, panelName, showTaskbarIcon )
{
	CCareerGame::Create();

	SetScheme( "CareerScheme" );
	SetProportional( true );
	SetMoveable( false );
	SetSizeable( false );
	SetTitleBarVisible( false );

	g_pTaskbar->AddTask( GetVPanel() );

	m_pTitleBackground = new Label( this, "TitleBackground", "" );
	m_pPlayerName = new Label( this, "PlayerNameLabel", "" );
	m_pMissionPackName = new Label( this, "MissionPackNameLabel", "" );
	m_showImageBackground = false;

	m_pAutosaveNotice = new CAutosaveLabel( this, "AutosaveNoticeLabel", "" );
	m_pAutosaveNotice->MoveToFront();
	m_pAutosaveNotice->SetVisible( false );
	m_autosaveHideTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCareerBaseFrame::~CCareerBaseFrame()
{
}

//-----------------------------------------------------------------------------
// Purpose: Hides the autosave notice when its time is up
//-----------------------------------------------------------------------------
void CCareerBaseFrame::OnThink()
{
	float now = system()->GetFrameTime();
	if ( m_autosaveHideTime != 0.0f && m_autosaveHideTime < now )
	{
		if ( m_pAutosaveNotice )
		{
			m_pAutosaveNotice->SetVisible( false );
		}
		m_autosaveHideTime = 0;
	}
	else if ( m_autosaveHideTime != 0.0f && m_pAutosaveNotice )
	{
		m_pAutosaveNotice->MoveToFront();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shows the autosave notice for two seconds
//-----------------------------------------------------------------------------
void CCareerBaseFrame::ShowAutosaveNotice()
{
	if ( m_pAutosaveNotice )
	{
		m_autosaveHideTime = (float)system()->GetFrameTime() + 2.0f;
		m_pAutosaveNotice->SetVisible( true );
		m_pAutosaveNotice->MoveToFront();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Label colors, button sounds and the mission pack background
//-----------------------------------------------------------------------------
void CCareerBaseFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pPlayerName->SetFgColor( pScheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );
	m_pMissionPackName->SetFgColor( pScheme->GetColor( "PlayerName", Color( 0, 0, 0, 0 ) ) );

	for ( int i=0; i<m_buttons.GetCount(); ++i )
	{
		m_buttons[i]->SetArmedSound( "sound/UI/buttonrollover.wav" );
		m_buttons[i]->SetDepressedSound( "sound/UI/buttonclick.wav" );
		m_buttons[i]->SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
	}

	CareerDefinition *pActiveCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( pActiveCareer )
	{
		m_bgFade.SetFade( pActiveCareer->GetBgStartColor(), pActiveCareer->GetBgEndColor(), 25.0f, ColorFade::down );
		m_pPlayerName->SetFgColor( pActiveCareer->GetTextColor() );
		m_pMissionPackName->SetFgColor( pActiveCareer->GetTextColor() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads the splash screen tiles for the image background
//-----------------------------------------------------------------------------
void CCareerBaseFrame::SetImageBackground( bool show )
{
	m_showImageBackground = show;
	if ( !show )
		return;

	int wide, tall;
	surface()->GetScreenSize( wide, tall );

	bool hardwareFilter = false;
	for ( int y=0; y<3; ++y )
	{
		for ( int x=0; x<4; ++x )
		{
			bimage_t &bimage = m_ImageID[y][x];
			bimage.imageID = surface()->CreateNewTextureID( false );

			char filename[512];
			sprintf( filename, "resource/background/SplashGrey_%d_%d", x, y );
			surface()->DrawSetTextureFile( bimage.imageID, filename, hardwareFilter, false );
			surface()->DrawGetTextureSize( bimage.imageID, bimage.width, bimage.height );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the splash screen tiles scaled to the screen
//-----------------------------------------------------------------------------
void CCareerBaseFrame::DrawBackgroundImage()
{
	int xpos, ypos;
	int wide, tall;
	GetSize( wide, tall );

	int swide, stall;
	surface()->GetScreenSize( swide, stall );

	float xScale = swide / 1024.0f;
	float yScale = stall / 768.0f;

	ypos = 0;
	for ( int y=0; y<3; ++y )
	{
		xpos = 0;
		for ( int x=0; x<4; ++x )
		{
			const bimage_t &bimage = m_ImageID[y][x];

			int dx = xpos;
			int dy = ypos;
			int dw = xpos + (int)ceil( bimage.width * xScale );
			int dt = ypos + (int)ceil( bimage.height * yScale );

			if ( x == 0 )
			{
				dx = 0;
			}
			if ( y == 0 )
			{
				dy = 0;
			}

			surface()->DrawSetColor( 255, 255, 255, 255 );
			surface()->DrawSetTexture( bimage.imageID );
			surface()->DrawTexturedRect( dx, dy, dw, dt );

			xpos += (int)ceil( bimage.width * xScale );
		}

		ypos += (int)ceil( m_ImageID[y][0].height * yScale );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws the background fade and the title bar
//-----------------------------------------------------------------------------
void CCareerBaseFrame::PaintBackground()
{
	if ( m_showImageBackground )
	{
		DrawBackgroundImage();
	}
	else
	{
		BaseClass::PaintBackground();

		int x, y, w, h;
		GetBounds( x, y, w, h );
		m_bgFade.DrawFade( x, y, w, h );

		m_pTitleBackground->GetBounds( x, y, w, h );

		Color col;
		IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
		col = pScheme->GetColor( "WindowTitleBG", col );

		int screenW, screenH;
		surface()->GetScreenSize( screenW, screenH );
		surface()->DrawSetColor( col );
		surface()->DrawFilledRect( x, y, x + w, y + h );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Scales a 640x480 value to the screen, keeping the aspect ratio
//-----------------------------------------------------------------------------
int GetAlternateProportionalValueFromNormal( int normalizedValue )
{
	int wide, tall;
	surface()->GetScreenSize( wide, tall );

	int proH, proW;
	surface()->GetProportionalBase( proW, proH );

	double scaleH = (double)tall / (double)proH;
	double scaleW = (double)wide / (double)proW;
	double scale = ( scaleW < scaleH ) ? scaleW : scaleH;

	return (int)( normalizedValue * scale );
}

//-----------------------------------------------------------------------------
// Purpose: Rescales a proportional value, keeping the aspect ratio
//-----------------------------------------------------------------------------
int GetAlternateProportionalValueFromScaled( int scaledValue )
{
	return GetAlternateProportionalValueFromNormal( scheme()->GetProportionalNormalizedValue( scaledValue ) );
}

//-----------------------------------------------------------------------------
// Purpose: Rescales the bounds of a control, keeping the aspect ratio
//-----------------------------------------------------------------------------
void RepositionControl( Panel *pPanel )
{
	int x, y, w, h;
	pPanel->GetBounds( x, y, w, h );

	x = GetAlternateProportionalValueFromScaled( x );
	y = GetAlternateProportionalValueFromScaled( y );
	w = GetAlternateProportionalValueFromScaled( w );
	h = GetAlternateProportionalValueFromScaled( h );

	pPanel->SetBounds( x, y, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: Player name, difficulty and mission pack title
//-----------------------------------------------------------------------------
void CCareerBaseFrame::Activate()
{
	BaseClass::Activate();

	CareerDefinition *pActiveCareer = TheCareerDefinitions->GetActiveCareerDefinition();
	if ( pActiveCareer )
	{
		m_bgFade.SetFade( pActiveCareer->GetBgStartColor(), pActiveCareer->GetBgEndColor(), 25.0f, ColorFade::down );
		m_pPlayerName->SetFgColor( pActiveCareer->GetTextColor() );
		m_pMissionPackName->SetFgColor( pActiveCareer->GetTextColor() );
	}

	const int BufLen = 128;

	char name[32];
	strncpy( name, engine->pfnGetCvarString( "name" ), 32 );
	name[31] = 0;

	char asciiDifficulty[32];
	sprintf( asciiDifficulty, "#Cstrike_Bot_Difficulty%d", TheCareerGame->GetDifficulty() );

	wchar_t wbuf[BufLen] = L"";
	wchar_t *tmp = wbuf;
	int len = BufLen;
	BufWPrintf( tmp, len, L"%hs / %s", name, localize()->Find( asciiDifficulty ) );
	m_pPlayerName->SetText( wbuf );

	m_pMissionPackName->SetText( TheCareerDefinitions->GetActiveCareerDefinition()->GetTitle() );
}

//-----------------------------------------------------------------------------
// Purpose: Fills the screen and rescales the controls to its aspect ratio
//-----------------------------------------------------------------------------
void CCareerBaseFrame::PerformLayout()
{
	SetPos( 0, 0 );
	BaseClass::PerformLayout();

	if ( IsProportional() )
	{
		int screenW_guess, screenH_guess;
		surface()->GetScreenSize( screenW_guess, screenH_guess );

		int width_guess, height_guess;
		GetSize( width_guess, height_guess );
		if ( width_guess != screenW_guess || height_guess != screenH_guess )
		{
			SetSize( screenW_guess, screenH_guess );

			width_guess = GetAlternateProportionalValueFromScaled( width_guess );
			height_guess = GetAlternateProportionalValueFromScaled( height_guess );
			int xOffset_guess = ( screenW_guess - width_guess ) / 2;
			int yOffset_guess = ( screenH_guess - height_guess ) / 2;

			if ( !GetBuildGroup() || !GetBuildGroup()->GetPanelList() )
				return;

			CUtlVector< PHandle > *children_guess = GetBuildGroup()->GetPanelList();
			CUtlVector< Panel * > visited_guess;
			CUtlVector< Panel * > moved_guess;

			for ( int panelIndex_guess=0; panelIndex_guess<children_guess->Count(); ++panelIndex_guess )
			{
				PHandle handle_guess = (*children_guess)[panelIndex_guess];
				Panel *child_guess = handle_guess.Get();

				bool bDone_guess = false;
				for ( int j_guess=0; j_guess<visited_guess.Count(); ++j_guess )
				{
					if ( child_guess == visited_guess[j_guess] )
					{
						bDone_guess = true;
					}
				}

				if ( !child_guess || bDone_guess )
					continue;

				visited_guess.AddToTail( child_guess );
				if ( child_guess == m_pTitleBackground )
				{
					RepositionControl( child_guess );
				}
				else if ( child_guess != this )
				{
					RepositionControl( child_guess );
				}
			}

			for ( int index2_guess=0; index2_guess<children_guess->Count(); ++index2_guess )
			{
				PHandle handle_guess = (*children_guess)[index2_guess];
				Panel *child_guess = handle_guess.Get();

				bool bDone_guess = false;
				for ( int j_guess=0; j_guess<moved_guess.Count(); ++j_guess )
				{
					if ( child_guess == moved_guess[j_guess] )
					{
						bDone_guess = true;
					}
				}

				if ( !child_guess || bDone_guess )
					continue;

				moved_guess.AddToTail( child_guess );
				if ( child_guess == m_pTitleBackground )
				{
					child_guess->SetSize( screenW_guess, child_guess->GetTall() + yOffset_guess );
				}
				else if ( child_guess != this )
				{
					int x_guess, y_guess;
					child_guess->GetPos( x_guess, y_guess );
					child_guess->SetPos( x_guess + xOffset_guess, y_guess + yOffset_guess );
				}
			}
		}
	}
}
