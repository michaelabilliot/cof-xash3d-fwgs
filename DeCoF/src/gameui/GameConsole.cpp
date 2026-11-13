//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <stdarg.h>
#include <stdio.h>

#include "GameConsole.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include "EngineInterface.h"
#include "Taskbar.h"

#include <vgui/ISurface.h>
#include <vgui/KeyValues.h>

#include <vgui_controls/Controls.h>

#undef PostMessage	// windows.h, from cdll_int.h

#define min(a,b)            (((a) < (b)) ? (a) : (b))

CGameConsole g_GameConsole;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameConsole::CGameConsole()
{
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameConsole::~CGameConsole()
{
	m_bInitialized = false;
}

//-----------------------------------------------------------------------------
// Purpose: sets up the console for use
//-----------------------------------------------------------------------------
void CGameConsole::Initialize()
{
	m_pConsole = vgui2::SETUP_PANEL( new CGameConsoleDialog() ); // we add text before displaying this so set it up now!
	m_pConsole->SetParent( g_pTaskbar->GetVPanel() );

	// set the console to taking up most of the right-half of the screen
	int swide, stall;
	vgui2::surface()->GetScreenSize( swide, stall );
	int offset = 40;

	m_pConsole->SetBounds( offset, offset, min( swide - offset * 2, 560 ), min( stall - offset * 2, 400 ) );

	m_bInitialized = true;

	gEngfuncs.pfnAddCommand( "condump", OnCmdCondump );
}

//-----------------------------------------------------------------------------
// Purpose: activates the console, makes it visible and brings it to the foreground
//-----------------------------------------------------------------------------
void CGameConsole::Activate()
{
	if ( !m_bInitialized )
		return;

	if ( LoadingDialog() )
		return; // don't activate the console while loading

	vgui2::surface()->RestrictPaintToSinglePanel( NULL );
	m_pConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: hides the console
//-----------------------------------------------------------------------------
void CGameConsole::Hide()
{
	if ( !m_bInitialized )
		return;

	m_pConsole->Hide();
}

//-----------------------------------------------------------------------------
// Purpose: clears the console
//-----------------------------------------------------------------------------
void CGameConsole::Clear()
{
	if ( !m_bInitialized )
		return;

	m_pConsole->Clear();
}

//-----------------------------------------------------------------------------
// Purpose: prints a message to the console
//-----------------------------------------------------------------------------
void CGameConsole::Printf( const char *format, ... )
{
	if ( !m_bInitialized )
		return;

	va_list argptr;
	char msg[4096];

	va_start( argptr, format );
	_vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );
	msg[sizeof( msg ) - 1] = 0;

	m_pConsole->Print( msg );
}

//-----------------------------------------------------------------------------
// Purpose: prints a debug message to the console
//-----------------------------------------------------------------------------
void CGameConsole::DPrintf( const char *format, ... )
{
	if ( !m_bInitialized )
		return;

	va_list argptr;
	char msg[4096];

	va_start( argptr, format );
	_vsnprintf( msg, sizeof( msg ), format, argptr );
	va_end( argptr );
	msg[sizeof( msg ) - 1] = 0;

	m_pConsole->DPrint( msg );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the console is currently in focus
//-----------------------------------------------------------------------------
bool CGameConsole::IsConsoleVisible()
{
	if ( !m_bInitialized )
		return false;

	return m_pConsole->IsVisible();
}

//-----------------------------------------------------------------------------
// Purpose: activates the console after a delay
//-----------------------------------------------------------------------------
void CGameConsole::ActivateDelayed( float time )
{
	if ( !m_bInitialized )
		return;

	m_pConsole->PostMessage( m_pConsole, new KeyValues( "Activate" ), time );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGameConsole::SetParent( int parent )
{
	if ( !m_bInitialized )
		return;

	m_pConsole->SetParent( (vgui2::VPANEL)parent );
}

//-----------------------------------------------------------------------------
// Purpose: writes the console text to a file
//-----------------------------------------------------------------------------
void CGameConsole::OnCmdCondump()
{
	g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}

// expose the console as an interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameConsole, IGameConsole, GAMECONSOLE_INTERFACE_VERSION, g_GameConsole );
