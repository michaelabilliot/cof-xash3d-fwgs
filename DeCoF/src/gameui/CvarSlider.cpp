//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <string.h>

#include "CvarSlider.h"
#include "EngineInterface.h"

#include <vgui/KeyValues.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCvarSlider::CCvarSlider( Panel *parent, const char *panelName, char const *caption, float minValue, float maxValue, char const *cvarname, bool bAllowOutOfRange ) : Slider( parent, panelName )
{
	AddActionSignalTarget( this );

	SetRange( (int)( 100.0f * minValue ), (int)( 100.0f * maxValue ) );

	char szMin[ 32 ];
	char szMax[ 32 ];

	sprintf( szMin, "%.2f", minValue );
	sprintf( szMax, "%.2f", maxValue );

	SetTickCaptions( szMin, szMax );

	strcpy( m_szCvarName, cvarname );

	m_bModifiedOnce = false;
	m_bAllowOutOfRange = bAllowOutOfRange;

	// Set slider to current value
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCvarSlider::~CCvarSlider()
{
}

//-----------------------------------------------------------------------------
// Purpose: picks up a cvar changed from outside the slider
//-----------------------------------------------------------------------------
void CCvarSlider::Paint()
{
	// Get engine's current value
	float curvalue = engine->pfnGetCvarFloat( m_szCvarName );

	// did it get changed from under us?
	if ( curvalue != m_fStartValue )
	{
		int val = (int)( 100.0f * curvalue );
		m_fStartValue = curvalue;
		m_fCurrentValue = curvalue;

		SetValue( val );
		m_iStartValue = GetValue();
	}

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: writes the slider value into the cvar
//-----------------------------------------------------------------------------
void CCvarSlider::ApplyChanges()
{
	if ( m_bModifiedOnce )
	{
		m_iStartValue = GetValue();
		if ( m_bAllowOutOfRange )
		{
			m_fStartValue = m_fCurrentValue;
		}
		else
		{
			m_fStartValue = (float)m_iStartValue / 100.0f;
		}

		engine->Cvar_SetValue( m_szCvarName, m_fStartValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CCvarSlider::GetSliderValue()
{
	if ( m_bAllowOutOfRange )
	{
		return m_fCurrentValue;
	}
	else
	{
		return ( (float)GetValue() ) / 100.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarSlider::SetSliderValue( float fValue )
{
	int nVal = (int)( 100.0f * fValue );
	SetValue( nVal, false );

	// remember this slider value
	m_iLastSliderValue = GetValue();

	if ( m_fCurrentValue != fValue )
	{
		m_fCurrentValue = fValue;
		m_bModifiedOnce = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: reads the cvar into the slider
//-----------------------------------------------------------------------------
void CCvarSlider::Reset()
{
	// Set slider to current value
	m_fStartValue = engine->pfnGetCvarFloat( m_szCvarName );
	m_fCurrentValue = m_fStartValue;

	int value = (int)( 100.0f * m_fStartValue );
	SetValue( value );

	m_iStartValue = GetValue();
	m_iLastSliderValue = m_iStartValue;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCvarSlider::HasBeenModified()
{
	if ( GetValue() != m_iStartValue )
	{
		m_bModifiedOnce = true;
	}

	return m_bModifiedOnce;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarSlider::OnSliderMoved()
{
	if ( HasBeenModified() )
	{
		if ( m_iLastSliderValue != GetValue() )
		{
			m_iLastSliderValue = GetValue();
			m_fCurrentValue = ( (float)m_iLastSliderValue ) / 100.0f;
		}

		// tell parent that we've been modified
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}
}
