//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdlib.h>
#include <string.h>

#include "CommandCheckButton.h"
#include "EngineInterface.h"

using namespace vgui2;

CCommandCheckButton::CCommandCheckButton( Panel *parent, const char *panelName, const char *text, const char *downcmd, const char *upcmd )
 : CheckButton( parent, panelName, text )
{
	m_pszDown = downcmd ? strdup( downcmd ) : NULL;
	m_pszUp = upcmd ? strdup( upcmd ) : NULL;
}

CCommandCheckButton::~CCommandCheckButton()
{
	free( m_pszDown );
	free( m_pszUp );
}

void CCommandCheckButton::SetSelected( bool state )
{
	BaseClass::SetSelected( state );

	if ( IsSelected() && m_pszDown )
	{
		engine->pfnClientCmd( m_pszDown );
		engine->pfnClientCmd( "\n" );
	}
	else if ( !IsSelected() && m_pszUp )
	{
		engine->pfnClientCmd( m_pszUp );
		engine->pfnClientCmd( "\n" );
	}
}
