//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button for one career profile, with its medals
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "CareerProfile.h"
#include "CareerProfileButton.h"
#include "CareerProfileFrame.h"

#include <vgui/IBorder.h>
#include <vgui/IImage.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

char *UTIL_va( char *format, ... );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CProfileButton::CProfileButton( CCareerProfileFrame *parent, const char *name, const wchar_t *text, bool *medals ) : BaseClass( NULL, name, "" )
{
	SetSize( GetWide(), 30 );

	const int BufLen = 64;
	char buf[BufLen];

	m_textImage = new TextImage( text );

	for ( int i=0; i<MAX_CAREER_DIFFICULTY; ++i )
	{
		_snprintf( buf, BufLen, "gfx/career/medals/Medal.0%d_%s_small", i+1, ( medals[i] ) ? "Active" : "Inactive" );
		m_image[i] = scheme()->GetImage( buf, false );
	}

	m_pParent = parent;
	m_name[0] = 0;
	m_active = false;
}

//-----------------------------------------------------------------------------
// Purpose: Stores the name of the profile this button stands for
//-----------------------------------------------------------------------------
void CProfileButton::SetProfileName( const char *name )
{
	strncpy( m_name, name, sizeof( m_name ) );
	m_name[ sizeof( m_name ) - 1 ] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Double clicking a profile selects and resumes it
//-----------------------------------------------------------------------------
void CProfileButton::OnMouseDoublePressed( MouseCode mouseCode )
{
	if ( !IsEnabled() )
		return;

	if ( mouseCode == MOUSE_LEFT && m_pParent )
	{
		m_pParent->OnCommand( UTIL_va( "SelectProfile %s", m_name ) );
		m_pParent->OnCommand( "ResumeProfile" );
	}
	else
	{
		BaseClass::OnMouseDoublePressed( mouseCode );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the colors, fonts, sounds and borders
//-----------------------------------------------------------------------------
void CProfileButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_preTextPad = 10;
	m_postTextPad = 10;
	m_interImagePad = 2;
	m_postImagePad = 10;

	m_textImage->SetColor( pScheme->GetColor( "ProfileButtonFG", Color() ) );
	m_textImage->SetFont( pScheme->GetFont( "DefaultShadow", true ) );
	m_textImage->ResizeImageToContent();

	m_bgColor = pScheme->GetColor( "ProfileButtonBG", Color() );
	m_activeBgColor = pScheme->GetColor( "ProfileButtonActiveBG", Color() );

	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );

	m_depressedBorder = pScheme->GetBorder( "CareerButtonDepressedBorder" );
	m_armedBorder = pScheme->GetBorder( "CareerButtonArmedBorder" );
}

//-----------------------------------------------------------------------------
// Purpose: Armed and depressed borders only
//-----------------------------------------------------------------------------
IBorder *CProfileButton::GetBorder( bool depressed, bool armed, bool selected, bool keyfocus )
{
	if ( !IsEnabled() )
		return NULL;

	if ( armed && !depressed )
		return m_armedBorder;

	if ( depressed )
		return m_depressedBorder;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The active profile gets its own background
//-----------------------------------------------------------------------------
Color CProfileButton::GetBgColor()
{
	if ( m_active )
		return m_activeBgColor;

	return m_bgColor;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the medals right aligned, with the name to their left
//-----------------------------------------------------------------------------
void CProfileButton::Paint()
{
	int buttonWide, buttonTall;
	GetSize( buttonWide, buttonTall );

	int imageWide, imageTall;

	int x = buttonWide - m_postImagePad;

	int i;
	for ( i=MAX_CAREER_DIFFICULTY-1; i>=0; --i )
	{
		m_image[i]->GetSize( imageWide, imageTall );
		x -= imageWide;
		m_image[i]->SetPos( x, (buttonTall - imageTall)/2 );
		m_image[i]->Paint();

		if ( i )
			x -= m_interImagePad;
	}

	x -= m_postTextPad;

	int textTall, textWide;
	m_textImage->GetSize( textWide, textTall );

	int textSpace = x - m_preTextPad;
	if ( textWide > textSpace )
	{
		m_textImage->SetSize( textSpace, GetTall() );
	}

	m_textImage->SetPos( x - textWide, (buttonTall - textTall)/2 );
	m_textImage->Paint();
}
