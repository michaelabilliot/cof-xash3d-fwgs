//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include "BackgroundMenuButton.h"

#include <vgui/IScheme.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBackgroundMenuButton::CBackgroundMenuButton( vgui2::Panel *parent, const char *name ) : BaseClass( parent, name, "" )
{
	m_pImage = NULL;
	m_pMouseOverImage = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBackgroundMenuButton::~CBackgroundMenuButton()
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBackgroundMenuButton::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Makes the button transparent
//-----------------------------------------------------------------------------
void CBackgroundMenuButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetFgColor( Color( 255, 255, 255, 255 ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetDefaultColor( Color( 255, 255, 255, 255 ), Color( 0, 0, 0, 0 ) );
	SetArmedColor( Color( 255, 255, 0, 255 ), Color( 0, 0, 0, 0 ) );
	SetDepressedColor( Color( 255, 255, 0, 255 ), Color( 0, 0, 0, 0 ) );
	SetContentAlignment( Label::a_west );
	SetBorder( NULL );
	SetDefaultBorder( NULL );
	SetDepressedBorder( NULL );
	SetKeyFocusBorder( NULL );
	SetTextInset( 0, 0 );

	SetArmedSound( "sound/UI/buttonrollover.wav" );
	SetDepressedSound( "sound/UI/buttonclick.wav" );
	SetReleasedSound( "sound/UI/buttonclickrelease.wav" );
}
