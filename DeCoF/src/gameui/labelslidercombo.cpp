//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: A slider with a text entry that shows its value
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>

#include "labelslidercombo.h"

#include <vgui/KeyValues.h>

#include <vgui_controls/TextEntry.h>

using namespace vgui2;

LabelSliderCombo::LabelSliderCombo( Panel *parent, const char *sliderName, const char *textEntryName ) : Slider( parent, sliderName )
{
	m_pTextEntry = new TextEntry( parent, textEntryName );
	m_pTextEntry->SetEnabled( false );

	SetValue( 0, true );
}

LabelSliderCombo::~LabelSliderCombo()
{
}

void LabelSliderCombo::OnCursorMoved( int x, int y )
{
	BaseClass::OnCursorMoved( x, y );
	UpdateTextEntry( true );
}

void LabelSliderCombo::SetValue( int value, bool bTriggerChangeMessage )
{
	BaseClass::SetValue( value, bTriggerChangeMessage );
	UpdateTextEntry( bTriggerChangeMessage );
}

void LabelSliderCombo::SetVisible( bool state )
{
	BaseClass::SetVisible( state );

	if ( m_pTextEntry )
	{
		m_pTextEntry->SetVisible( state );
	}
}

void LabelSliderCombo::UpdateTextEntry( bool bTriggerChangeMessage )
{
	int value = GetValue();
	if ( value == m_currentValueShown )
		return;

	if ( bTriggerChangeMessage )
	{
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}

	wchar_t scratch[64];
	_snwprintf( scratch, 64, L"%d", value );

	if ( m_pTextEntry )
	{
		m_pTextEntry->SetText( scratch );
		m_currentValueShown = value;
	}
}
