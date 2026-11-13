//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A button in the taskbar for one open dialog
//
// $NoKeywords: $
//=============================================================================

#include "TaskButton.h"

#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Controls.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTaskButton::CTaskButton( Panel *parent, VPANEL task ) : Label( parent, "TaskButton", "<unknown>" )
{
	m_hTask = ivgui()->PanelToHandle( task );
	m_bHasTitle = false;

	SetTextImageIndex( 1 );
	SetImageAtIndex( 0, &m_WindowStateImage, 4 );
	SetImagePreOffset( 1, 4 );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTaskButton::~CTaskButton()
{
}

//-----------------------------------------------------------------------------
// Purpose: Sets the text of the button, a button with no title is not shown
//-----------------------------------------------------------------------------
void CTaskButton::SetTitle( const wchar_t *title )
{
	SetText( title );

	if ( title && title[0] )
	{
		m_bHasTitle = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Brings the window to the front when selected
//-----------------------------------------------------------------------------
void CTaskButton::SetSelected( bool state )
{
	if ( state == m_bSelected )
		return;

	m_bSelected = state;

	VPANEL task = ivgui()->HandleToPanel( m_hTask );
	if ( !task )
		return;

	if ( m_bSelected )
	{
		surface()->SetMinimized( task, false );
		ipanel()->SetVisible( task, true );

		if ( !ipanel()->HasParent( surface()->GetTopmostPopup(), task ) )
		{
			ipanel()->MoveToFront( task );
		}

		SetFgColor( m_SelectedColor );
		m_WindowStateImage.SetFocused();
	}
	else
	{
		SetFgColor( m_LabelColor );

		if ( surface()->IsMinimized( task ) )
		{
			m_WindowStateImage.SetMinimized();
		}
		else
		{
			m_WindowStateImage.SetOutOfFocus();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the button should be in the taskbar
//-----------------------------------------------------------------------------
bool CTaskButton::ShouldDisplay()
{
	if ( !m_bHasTitle )
		return false;

	VPANEL task = ivgui()->HandleToPanel( m_hTask );
	if ( !task )
		return false;

	if ( !ipanel()->IsVisible( task ) && !surface()->IsMinimized( task ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the window this button stands for
//-----------------------------------------------------------------------------
VPANEL CTaskButton::GetTaskPanel()
{
	return ivgui()->HandleToPanel( m_hTask );
}

//-----------------------------------------------------------------------------
// Purpose: Selects the window, or minimizes it when it is already selected
//-----------------------------------------------------------------------------
void CTaskButton::OnMousePressed( MouseCode mouseCode )
{
	if ( m_bSelected )
	{
		VPANEL task_guess = ivgui()->HandleToPanel( m_hTask );
		if ( task_guess )
		{
			surface()->SetMinimized( task_guess, true );
		}
	}
	else
	{
		SetSelected( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTaskButton::OnMouseDoublePressed( MouseCode mouseCode )
{
	OnMousePressed( mouseCode );
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the colors and the window state image
//-----------------------------------------------------------------------------
void CTaskButton::ApplySchemeSettings( IScheme *pScheme )
{
	Label::ApplySchemeSettings( pScheme );

	m_LabelColor = GetSchemeColor( "DisabledText1", pScheme );
	m_SelectedColor = GetSchemeColor( "BrightBaseText", pScheme );

	SetContentAlignment( Label::a_west );
	SetBgColor( Color( 0, 0, 0, 0 ) );

	SetTextImageIndex( 1 );
	SetImageAtIndex( 0, &m_WindowStateImage, 4 );
	SetImagePreOffset( 1, 4 );

	m_WindowStateImage.SetTall( surface()->GetFontTall( GetFont() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWindowStateImage::CWindowStateImage()
{
	SetSize( 12, 24 );
	m_iState = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Matches the image to the height of the text
//-----------------------------------------------------------------------------
void CWindowStateImage::SetTall( int tall )
{
	SetSize( 12, tall );
}

//-----------------------------------------------------------------------------
// Purpose: Draws a bar for a focused or out of focus window, a lower one for a minimized window
//-----------------------------------------------------------------------------
void CWindowStateImage::Paint()
{
	int wide, tall;
	GetSize( wide, tall );

	const int xsize = 8;
	int xpad = ( wide - xsize ) / 2;
	int ypad = tall - xsize;
	int yoffs = 3;

	if ( m_iState == 0 )
	{
		DrawSetColor( 255, 255, 255, 255 );
		DrawFilledRect( xpad, ypad - yoffs, wide - xpad, tall - yoffs );
	}
	else if ( m_iState == 1 )
	{
		DrawSetColor( 100, 120, 100, 255 );
		DrawFilledRect( xpad, ypad - yoffs, wide - xpad, tall - yoffs );
	}
	else if ( m_iState == 2 )
	{
		DrawSetColor( 100, 120, 100, 255 );
		DrawFilledRect( xpad, tall - 4 - yoffs, wide - xpad, tall - yoffs );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWindowStateImage::SetFocused()
{
	m_iState = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWindowStateImage::SetOutOfFocus()
{
	m_iState = 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWindowStateImage::SetMinimized()
{
	m_iState = 2;
}
