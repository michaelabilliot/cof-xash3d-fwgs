//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CvarTextEntry.h"
#include "EngineInterface.h"

#include <vgui/IScheme.h>
#include <vgui/KeyValues.h>

using namespace vgui2;

#define MAX_CVAR_TEXT 64

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CCvarTextEntry::CCvarTextEntry( Panel *parent, const char *panelName, char const *cvarname )
 : TextEntry( parent, panelName )
{
	m_pszCvarName = cvarname ? strdup( cvarname ) : NULL;
	m_pszStartValue[0] = 0;

	if ( m_pszCvarName )
	{
		Reset();
	}

	AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CCvarTextEntry::~CCvarTextEntry()
{
	if ( m_pszCvarName )
	{
		free( m_pszCvarName );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarTextEntry::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	if ( GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_CVAR_TEXT )
	{
		SetMaximumCharCount( MAX_CVAR_TEXT - 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the text into the cvar
//-----------------------------------------------------------------------------
void CCvarTextEntry::ApplyChanges( bool immediate )
{
	if ( !m_pszCvarName )
		return;

	char szText[ MAX_CVAR_TEXT ];
	GetText( szText, MAX_CVAR_TEXT );

	if ( !szText[ 0 ] )
		return;

	if ( immediate )
	{
		// set immediately - don't wait for the next frame
		engine->Cvar_Set( m_pszCvarName, szText );
	}
	else
	{
		char szCommand[ 256 ];
		sprintf( szCommand, "%s \"%s\"\n", m_pszCvarName, szText );
		engine->pfnClientCmd( szCommand );
	}

	strncpy( m_pszStartValue, szText, sizeof( m_pszStartValue ) );
	m_pszStartValue[ sizeof( m_pszStartValue ) - 1 ] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: reads the cvar into the text
//-----------------------------------------------------------------------------
void CCvarTextEntry::Reset()
{
	char *value = engine->pfnGetCvarString( m_pszCvarName );
	if ( value && value[ 0 ] )
	{
		SetText( value );
		strncpy( m_pszStartValue, value, sizeof( m_pszStartValue ) );
		m_pszStartValue[ sizeof( m_pszStartValue ) - 1 ] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CCvarTextEntry::HasBeenModified()
{
	char szText[ MAX_CVAR_TEXT ];
	GetText( szText, MAX_CVAR_TEXT );

	return stricmp( szText, m_pszStartValue ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarTextEntry::OnTextChanged()
{
	if ( !m_pszCvarName )
		return;

	if ( HasBeenModified() )
	{
		PostActionSignal( new KeyValues( "ControlModified" ) );
	}
}
