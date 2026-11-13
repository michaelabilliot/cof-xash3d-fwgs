//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button with a texture inside its padding
//
// $NoKeywords: $
//=============================================================================

#include <string.h>

#include "ImageButton.h"

#include <vgui/ISurface.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/TextImage.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CImageButton::CImageButton( Panel *parent, const char *panelName, const char *text ) : BaseClass( parent, panelName, text )
{
	m_szTexture[0] = 0;
	m_bUploaded = false;
	m_nTextureId = -1;
	m_padFill = 1;
	m_padImage = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Fills the padding, then draws the texture and a shadowed text
//-----------------------------------------------------------------------------
void CImageButton::PaintBackground()
{
	int w, h;
	GetSize( w, h );

	Color col = GetBgColor();
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( m_padFill, m_padFill, w - m_padFill, h - m_padFill );

	if ( !m_szTexture[0] )
		return;

	if ( !m_bUploaded )
	{
		ForceUpload();
	}

	col = GetImageColor();
	surface()->DrawSetColor( col );
	surface()->DrawSetTexture( m_nTextureId );
	surface()->DrawTexturedRect( m_padImage, m_padImage, w - m_padImage, h - m_padImage );

	TextImage *textImage = GetTextImage();
	int x, y;
	textImage->GetPos( x, y );
	textImage->SetPos( x + 1, y + 1 );
	textImage->SetColor( Color( 0, 0, 0, 192 ) );
	textImage->Paint();
	textImage->SetPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the padding around the texture
//-----------------------------------------------------------------------------
void CImageButton::SetImagePadding( int pad )
{
	m_padImage = pad;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the padding around the filled background
//-----------------------------------------------------------------------------
void CImageButton::SetFillPadding( int pad )
{
	m_padFill = pad;
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: The color the texture is drawn with
//-----------------------------------------------------------------------------
Color CImageButton::GetImageColor()
{
	return GetBgColor();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the texture file, loading it now if the button has one already
//-----------------------------------------------------------------------------
void CImageButton::SetTexture( const char *filename )
{
	strcpy( m_szTexture, filename );

	if ( m_bUploaded )
	{
		ForceReload();
	}
	else
	{
		ForceUpload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates the texture
//-----------------------------------------------------------------------------
void CImageButton::ForceUpload()
{
	if ( !m_szTexture[0] )
		return;

	m_bUploaded = true;
	m_nTextureId = surface()->CreateNewTextureID( false );
	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true );
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the texture file
//-----------------------------------------------------------------------------
void CImageButton::ForceReload()
{
	if ( !m_bUploaded )
		return;

	surface()->DrawSetTextureFile( m_nTextureId, m_szTexture, false, true );
}
