//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button drawn with a texture per state, which can flash
//
// $NoKeywords: $
//=============================================================================

#include <stdlib.h>

#include "BitmapButton.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>

#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor, loads the textures of the four states
//-----------------------------------------------------------------------------
CBitmapButton::CBitmapButton( Panel *parent, const char *panelName, const char *text, const char *normalImage, const char *armedImage, const char *clickedImage, const char *disabledImage ) : Button( parent, panelName, text )
{
	for ( int i = 0; i < 4; i++ )
	{
		bimage_t &bimage = m_ImageID[i];
		bimage.imageID = surface()->CreateNewTextureID( false );

		const char *filename = "";
		switch ( i )
		{
		case 0:
			if ( normalImage )
				filename = normalImage;
			break;
		case 1:
			if ( armedImage )
				filename = armedImage;
			break;
		case 2:
			if ( clickedImage )
				filename = clickedImage;
			break;
		case 3:
			if ( disabledImage )
				filename = disabledImage;
			else if ( normalImage )
				filename = normalImage;
			break;
		}

		surface()->DrawSetTextureFile( bimage.imageID, filename, true, false );
		surface()->DrawGetTextureSize( bimage.imageID, bimage.width, bimage.height );
	}

	m_shouldFlash = false;
	m_isFlashColor = false;
	m_transitionsLeft = 0;
	m_totalTransitions = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the texture of the current state
//-----------------------------------------------------------------------------
void CBitmapButton::PaintBackground()
{
	int imageNum = 0;
	if ( !IsEnabled() )
	{
		imageNum = 3;
	}
	else if ( IsDepressed() )
	{
		imageNum = 2;
	}
	else if ( IsArmed() )
	{
		imageNum = 1;
	}

	if ( m_shouldFlash && m_isFlashColor && imageNum == 0 )
	{
		imageNum = 1;
	}

	int w, h;
	GetSize( w, h );

	surface()->DrawSetColor( 255, 255, 255, 255 );
	surface()->DrawSetTexture( m_ImageID[imageNum].imageID );
	surface()->DrawTexturedRect( 0, 0, w, h );
}

//-----------------------------------------------------------------------------
// Purpose: No borders; the flash timing comes from the scheme
//-----------------------------------------------------------------------------
void CBitmapButton::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetBorder( NULL );
	SetDefaultBorder( NULL );
	SetDepressedBorder( NULL );
	SetKeyFocusBorder( NULL );

	m_interval = atof( pScheme->GetResourceString( "JumpButtonFlashPeriod" ) );
	m_totalTransitions = atoi( pScheme->GetResourceString( "JumpButtonNumFlashes" ) );
}

//-----------------------------------------------------------------------------
// Purpose: Toggles the flash state
//-----------------------------------------------------------------------------
void CBitmapButton::OnThink()
{
	if ( m_shouldFlash )
	{
		if ( m_transitionsLeft < 0 && m_totalTransitions )
		{
			m_transitionsLeft = m_totalTransitions;
		}

		float now = system()->GetCurrentTime();
		if ( now >= m_lastTransition + m_interval && m_transitionsLeft > 0 )
		{
			m_lastTransition = now;
			m_isFlashColor = !m_isFlashColor;
			m_transitionsLeft--;
		}
		else if ( m_transitionsLeft <= 0 )
		{
			m_isFlashColor = false;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Starts flashing
//-----------------------------------------------------------------------------
void CBitmapButton::Flash()
{
	m_shouldFlash = true;
	m_transitionsLeft = -1;
}
