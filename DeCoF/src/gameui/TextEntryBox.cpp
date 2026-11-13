//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "TextEntryBox.h"
#include "CvarTextEntry.h"

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

#define max(a,b)            (((a) > (b)) ? (a) : (b))

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTextEntryBox::CTextEntryBox( const char *title, const char *queryText, const char *entryText, bool isCvar, vgui2::Panel *parent ) : QueryBox( title, queryText, parent )
{
	if ( isCvar )
	{
		m_pEntry = m_pCvarEntry = new CCvarTextEntry( this, "TextEntry", entryText );
	}
	else
	{
		m_pEntry = new TextEntry( this, "TextEntry" );
		m_pCvarEntry = NULL;
	}

	m_pEntry->SetTabPosition( 3 );
	m_pEntry->RequestFocus();
	m_pEntry->GotoTextEnd();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CTextEntryBox::~CTextEntryBox()
{
	delete m_pEntry;
}

//-----------------------------------------------------------------------------
// Purpose: Shows the box and gives the text entry the focus
//-----------------------------------------------------------------------------
void CTextEntryBox::ShowWindow( Frame *pFrameOver )
{
	BaseClass::ShowWindow( pFrameOver );

	m_pEntry->RequestFocus();
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the label and the text entry side by side
//-----------------------------------------------------------------------------
void CTextEntryBox::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;

	const int borderW = 10;

	int labelW, labelH;
	int entryW, entryH;
	m_pMessageLabel->GetSize( labelW, labelH );

	entryW = max( 120, wide - borderW - borderW - borderW - labelW );
	entryH = max( 24, labelH );
	m_pEntry->SetSize( entryW, entryH );

	int boxWidth, boxTall;
	GetSize( boxWidth, boxTall );
	if ( boxWidth < labelW + entryW + borderW * 3 )
		SetSize( labelW + entryW + borderW * 3, boxTall );

	m_pMessageLabel->GetPos( x, y );
	m_pMessageLabel->SetPos( borderW, y - ( entryH - labelH ) / 2 );

	m_pEntry->SetPos( borderW + m_pMessageLabel->GetWide() + borderW, y - ( entryH - labelH ) );
}

//-----------------------------------------------------------------------------
// Purpose: Applies the cvar on OK
//-----------------------------------------------------------------------------
void CTextEntryBox::OnCommand( const char *command )
{
	if ( !stricmp( command, "Ok" ) && m_pCvarEntry )
	{
		m_pCvarEntry->ApplyChanges( true );
	}

	BaseClass::OnCommand( command );
}

//-----------------------------------------------------------------------------
// Purpose: Escape cancels, enter accepts
//-----------------------------------------------------------------------------
void CTextEntryBox::OnKeyCodeTyped( KeyCode code )
{
	if ( code == KEY_ESCAPE )
	{
		OnCommand( "Cancel" );
	}
	if ( code == KEY_ENTER )
	{
		OnCommand( "Ok" );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}
