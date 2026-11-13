//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdlib.h>
#include <string.h>

#include "CvarToggleCheckButton.h"
#include "EngineInterface.h"

#include <vgui/KeyValues.h>

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCvarToggleCheckButton::CCvarToggleCheckButton( Panel *parent, const char *panelName, const char *text, char const *cvarname )
 : CheckButton( parent, panelName, text )
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;

	if ( m_pszCvarName )
	{
		Reset();
	}

	AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCvarToggleCheckButton::~CCvarToggleCheckButton()
{
	free( m_pszCvarName );
}

//-----------------------------------------------------------------------------
// Purpose: picks up a cvar changed from outside the button
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::Paint()
{
	if ( !m_pszCvarName || !m_pszCvarName[ 0 ] )
	{
		BaseClass::Paint();
		return;
	}

	// Look up current value
	bool value = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f ? true : false;

	if ( value != m_bStartValue )
	{
		SetSelected( value );
		m_bStartValue = value;
	}

	BaseClass::Paint();
}

//-----------------------------------------------------------------------------
// Purpose: writes the button state into the cvar
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::ApplyChanges()
{
	m_bStartValue = IsSelected();
	engine->Cvar_SetValue( m_pszCvarName, m_bStartValue ? 1.0f : 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: reads the cvar into the button
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::Reset()
{
	m_bStartValue = engine->pfnGetCvarFloat( m_pszCvarName ) > 0.0f ? true : false;
	SetSelected( m_bStartValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCvarToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarToggleCheckButton::OnButtonChecked()
{
	if ( HasBeenModified() )
	{
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}
}
