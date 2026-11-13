//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "KeyToggleCheckButton.h"
#include "EngineInterface.h"
#include "igameuifuncs.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CKeyToggleCheckButton::CKeyToggleCheckButton( Panel *parent, const char *panelName, const char *text, char const *keyname, char const *cmdname )
 : CheckButton( parent, panelName, text )
{
	m_pszKeyName = keyname ? strdup( keyname ) : NULL;
	m_pszCmdName = cmdname ? strdup( cmdname ) : NULL;

	if ( m_pszKeyName )
	{
		Reset();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CKeyToggleCheckButton::~CKeyToggleCheckButton()
{
	free( m_pszKeyName );
	free( m_pszCmdName );
}

//-----------------------------------------------------------------------------
// Purpose: picks up a key state changed from outside the button
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Paint()
{
	BaseClass::Paint();

	if ( !m_pszKeyName )
		return;

	// Fixme, look up key state
	bool isdown;
	if ( gameuifuncs->IsKeyDown( m_pszKeyName, isdown ) )
	{
		// if someone changed the value using the console
		if ( m_bStartValue != isdown )
		{
			SetSelected( isdown );
			m_bStartValue = isdown;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::Reset()
{
	gameuifuncs->IsKeyDown( m_pszKeyName, m_bStartValue );

	if ( IsSelected() != m_bStartValue )
	{
		SetSelected( m_bStartValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CKeyToggleCheckButton::ApplyChanges()
{
	if ( !m_pszCmdName || !m_pszCmdName[ 0 ] )
		return;

	char szCommand[ 256 ];

	sprintf( szCommand, "%c%s\n", IsSelected() ? '+' : '-', m_pszCmdName );

	engine->pfnClientCmd( szCommand );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CKeyToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}
