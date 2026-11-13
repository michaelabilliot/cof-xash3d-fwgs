//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A label that draws a dotted border with rounded corners
//
// $NoKeywords: $
//=============================================================================

#include "DottedLabel.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>

#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CDottedBgLabel::CDottedBgLabel( Panel *parent, const char *name, const char *text, int dash, int gap, int corner ) : Label( parent, name, text )
{
	m_dash = dash;
	m_gap = gap;
	m_corner = corner;

	if ( !m_dash )
		m_dash = 2;
	if ( !m_gap )
		m_gap = 2;
	if ( !m_corner )
		m_corner = 6;

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	m_color = pScheme->GetColor( "BorderBright", m_color );
}

//-----------------------------------------------------------------------------
// Purpose: Draws the dotted border and its corners
//-----------------------------------------------------------------------------
void CDottedBgLabel::PaintBackground()
{
	Panel::PaintBackground();

	int x, y, w, h;
	GetBounds( x, y, w, h );

	surface()->DrawSetColor( m_color );

	DrawDashedLine( 0, 0, w, 1, m_dash, m_gap );
	DrawDashedLine( 0, 0, 1, h, m_dash, m_gap );
	DrawDashedLine( w - 1, 0, w, h, m_dash, m_gap );
	DrawDashedLine( 0, h - 1, w, h, m_dash, m_gap );

	surface()->DrawFilledRect( 0, 0, m_corner, 1 );
	surface()->DrawFilledRect( 0, 0, 1, m_corner );
	surface()->DrawFilledRect( w - m_corner, 0, w, 1 );
	surface()->DrawFilledRect( w - 1, 0, w, m_corner );
	surface()->DrawFilledRect( 0, h - 1, m_corner, h );
	surface()->DrawFilledRect( 0, h - m_corner, 1, h );
	surface()->DrawFilledRect( w - m_corner, h - 1, w, h );
	surface()->DrawFilledRect( w - 1, h - m_corner, w, h );
}
