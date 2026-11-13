//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "CvarNegateCheckButton.h"
#include "EngineInterface.h"

#include <vgui/KeyValues.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCvarNegateCheckButton::CCvarNegateCheckButton( Panel *parent, const char *panelName, const char *text, char const *cvarname )
 : CheckButton( parent, panelName, text )
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;
	Reset();
	AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCvarNegateCheckButton::~CCvarNegateCheckButton()
{
	free( m_pszCvarName );
}

//-----------------------------------------------------------------------------
// Purpose: picks up a cvar changed from outside the button
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::Paint()
{
	if ( !m_pszCvarName )
	{
		BaseClass::Paint();
		return;
	}

	// Look up current value
	float value = engine->pfnGetCvarFloat( m_pszCvarName );

	if ( value < 0 )
	{
		if ( !m_bStartState )
		{
			SetSelected( true );
			m_bStartState = true;
		}
	}
	else
	{
		if ( m_bStartState )
		{
			SetSelected( false );
			m_bStartState = false;
		}
	}
	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: reads the cvar into the button
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::Reset()
{
	// Set device according to current state
	float value = engine->pfnGetCvarFloat( m_pszCvarName );
	if ( value < 0.0f )
	{
		m_bStartState = true;
	}
	else
	{
		m_bStartState = false;
	}
	SetSelected( m_bStartState );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCvarNegateCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartState;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );
}

//-----------------------------------------------------------------------------
// Purpose: writes the button state into the sign of the cvar
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::ApplyChanges()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] )
		return;

	float value = engine->pfnGetCvarFloat( m_pszCvarName );
	value = (float)fabs( value );
	if ( value < 0.00001 )
	{
		// correct the value if it's not set
		value = 0.022f;
	}

	m_bStartState = IsSelected();
	value = -value;

	engine->Cvar_SetValue( m_pszCvarName, m_bStartState ? value : -value );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarNegateCheckButton::OnButtonChecked()
{
	if ( HasBeenModified() )
	{
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}
}
