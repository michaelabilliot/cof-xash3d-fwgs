//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the C interface of the engine to the VGUI2 game UI (GameUI.dll,
//			the game console and the client's VGUI2 panels)
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <string.h>
#include <wchar.h>

#include "interface.h"
#include "IBaseUI.h"
#include "IGameUI.h"
#include "IGameConsole.h"
#include "ICareerUI.h"
#include "vgui2/IClientVGUI.h"
#include "vgui2/BaseUISurface.h"
#include "UtlVector.h"

#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "const.h"
#include "cvar.h"
#include "client.h"
#include "modinfo.h"
#include "screen.h"
#include "winquake.h"
#include "vgui_int.h"

extern	struct cl_enginefuncs_s		cl_enginefuncs;

void	AllowFog( int allowed );
void	ClearIOStates( void );
void	VGuiWrap_ReleaseMouse( void );
void	VGuiWrap_GetMouse( void );

void	ResetTutorMessageDecayData( void );
}

#define CLDLL_INTERFACE_VERSION		7

extern CreateInterfaceFn g_FileSystemFactory;
extern BaseUISurface g_BaseUISurface;

extern IGameUI *staticGameUIFuncs;
extern IGameConsole *staticGameConsole;
extern ICareerUI *staticCareerUI;
extern IClientVGUI *staticClient;

// console text printed before the game console exists
CUtlVector<char> g_TempConsoleBuffer( 0, 1024 );

static qboolean staticExclusiveInputShadow = false;

IBaseUI *staticUIFuncs = NULL;

extern "C"
{

//-----------------------------------------------------------------------------
// Purpose: loads the BaseUI interface and starts it
//-----------------------------------------------------------------------------
void VGuiWrap2_Startup( void )
{
	CreateInterfaceFn factories[2];

	if ( staticUIFuncs )
		return;

	factories[0] = Sys_GetFactoryThis();
	factories[1] = g_FileSystemFactory;

	staticUIFuncs = (IBaseUI *)Sys_GetFactoryThis()( BASEUI_INTERFACE_VERSION, NULL );
	staticUIFuncs->Initialize( factories, 2 );
	staticUIFuncs->Start( &cl_enginefuncs, CLDLL_INTERFACE_VERSION );

	// print the text that came before the console
	g_TempConsoleBuffer.AddToTail( '\0' );
	VGuiWrap2_ConPrintf( g_TempConsoleBuffer.Base() );
	g_TempConsoleBuffer.Purge();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_Shutdown( void )
{
	if ( !staticUIFuncs )
		return;

	staticUIFuncs->Shutdown();
	staticUIFuncs = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: passes a window message to the VGUI2 surface
//-----------------------------------------------------------------------------
int VGuiWrap2_CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam )
{
	if ( staticUIFuncs )
		staticUIFuncs->CallEngineSurfaceAppHandler( hwnd, msg, wparam, lparam );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int VGuiWrap2_IsGameUIVisible( void )
{
	if ( !staticGameUIFuncs )
		return 0;

	return staticGameUIFuncs->HasExclusiveInput();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int VGuiWrap2_UseVGUI1( void )
{
	if ( !staticClient )
		return 1;

	return staticClient->UseVGUI1();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void *VGuiWrap2_GetPanel( void )
{
	if ( !staticUIFuncs )
		return NULL;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ReleaseMouse( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_GetMouse( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_SetVisible( int state )
{
}

//-----------------------------------------------------------------------------
// Purpose: the escape key toggles the game UI
//-----------------------------------------------------------------------------
int VGuiWrap2_GameUIKeyPressed( void )
{
	if ( !staticGameUIFuncs )
		return 0;

	if ( staticGameUIFuncs->HasExclusiveInput() )
	{
		// only hide the game UI when a level is loaded
		if ( cl.levelname[0] )
		{
			staticUIFuncs->HideGameUI();
		}
		return 1;
	}

	staticUIFuncs->ActivateGameUI();
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns 1 if the engine should handle the key
//-----------------------------------------------------------------------------
int VGuiWrap2_Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	if ( !staticUIFuncs )
		return 1;

	if ( staticUIFuncs->Key_Event( down, keynum, pszCurrentBinding ) )
		return 0;

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_Paint( void )
{
	POINT pnt;
	RECT rect;

	if ( !staticGameUIFuncs )
		return;

	pnt.x = 0;
	pnt.y = 0;

	ClientToScreen( *pmainwindow, &pnt );
	GetClientRect( *pmainwindow, &rect );

	AllowFog( false );

	staticUIFuncs->Paint( pnt.x, pnt.y, rect.right, rect.bottom );

	if ( VGuiWrap2_UseVGUI1() )
	{
		qboolean excl = staticGameUIFuncs->HasExclusiveInput();

		if ( excl != staticExclusiveInputShadow )
		{
			if ( excl )
			{
				VGuiWrap_ReleaseMouse();
			}
			else
			{
				VGuiWrap_GetMouse();
				ClearIOStates();
			}
		}

		staticExclusiveInputShadow = excl;
	}

	AllowFog( true );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_NotifyOfServerConnect( const char *game, int IP, int port )
{
	if ( !staticGameUIFuncs )
		return;

	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;

	StopLoadingProgressBar();

	staticGameUIFuncs->ConnectToServer( game, IP, port );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_NotifyOfServerDisconnect( void )
{
	if ( !staticGameUIFuncs )
		return;

	staticGameUIFuncs->DisconnectFromServer();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_HideGameUI( void )
{
	if ( !staticUIFuncs )
		return;

	staticUIFuncs->HideGameUI();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int VGuiWrap2_IsConsoleVisible( void )
{
	if ( staticGameConsole )
		return staticGameConsole->IsConsoleVisible();

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ShowConsole( void )
{
	if ( staticUIFuncs )
		staticUIFuncs->ActivateGameUI();

	if ( staticUIFuncs )
		staticUIFuncs->ShowConsole();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ShowDemoPlayer( void )
{
	if ( staticUIFuncs )
		staticUIFuncs->ActivateGameUI();

	if ( staticGameUIFuncs )
		staticGameUIFuncs->ActivateDemoUI();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_HideConsole( void )
{
	if ( staticUIFuncs )
		staticUIFuncs->HideConsole();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ClearConsole( void )
{
	if ( staticGameConsole )
		staticGameConsole->Clear();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ConPrintf( const char *msg )
{
	if ( staticGameConsole )
	{
		staticGameConsole->Printf( "%s", msg );
	}
	else
	{
		// keep it until the console exists
		int len = strlen( msg );
		int pos = g_TempConsoleBuffer.AddMultipleToTail( len );
		memcpy( &g_TempConsoleBuffer[pos], msg, len );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_ConDPrintf( const char *msg )
{
	if ( staticGameConsole )
	{
		staticGameConsole->DPrintf( "%s", msg );
	}
	else
	{
		// keep it until the console exists
		int len = strlen( msg );
		int pos = g_TempConsoleBuffer.AddMultipleToTail( len );
		memcpy( &g_TempConsoleBuffer[pos], msg, len );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_LoadingStarted( const char *resourceType, const char *resourceName )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->LoadingStarted( resourceType, resourceName );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGuiWrap2_LoadingFinished( const char *resourceType, const char *resourceName )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->LoadingFinished( resourceType, resourceName );
}

//-----------------------------------------------------------------------------
// Purpose: shows the loading dialog (not for a single player game on the
//			local server)
//-----------------------------------------------------------------------------
void StartLoadingProgressBar( const char *loadingType, int numProgressPoints )
{
	if ( ( UserIsConnectedOnLoopback() && gmodinfo.type != SINGLEPLAYER_ONLY ) || !UserIsConnectedOnLoopback() )
	{
		if ( staticUIFuncs )
			staticUIFuncs->ActivateGameUI();

		if ( staticGameUIFuncs )
		{
			staticGameUIFuncs->StartProgressBar( loadingType, numProgressPoints );
			SCR_UpdateScreen();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ContinueLoadingProgressBar( const char *loadingType, int progressPoint, float progressFraction )
{
	if ( staticGameUIFuncs )
	{
		if ( staticGameUIFuncs->ContinueProgressBar( progressPoint, progressFraction ) )
		{
			SCR_UpdateScreen();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SetLoadingProgressBarStatusText( const char *statusText )
{
	if ( staticGameUIFuncs )
	{
		if ( staticGameUIFuncs->SetProgressBarStatusText( statusText ) )
		{
			SCR_UpdateScreen();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void StopLoadingProgressBar( void )
{
	if ( cls.state != ca_active && staticUIFuncs )
	{
		if ( staticClient )
			staticClient->HideAllVGUIMenu();

		staticUIFuncs->ActivateGameUI();
	}
	else
	{
		if ( staticUIFuncs )
			staticUIFuncs->HideGameUI();
	}

	if ( staticGameUIFuncs )
		staticGameUIFuncs->StopProgressBar( gfExtendedError != 0, gszDisconnectReason, gszExtendedDisconnectReason );

	gfExtendedError = false;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SetSecondaryProgressBar( float progress )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->SetSecondaryProgressBar( progress );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SetSecondaryProgressBarText( const char *statusText )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->SetSecondaryProgressBarText( statusText );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ValidateCDKey( int force, int inConnect )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->ValidateCDKey( force != 0, inConnect != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Cry of Fear addition, opens a GameUI dialog for the optionshack
//			server command
//-----------------------------------------------------------------------------
void VGuiWrap2_OpenDialog_guess( int dialog )
{
	if ( staticGameUIFuncs )
	{
		staticUIFuncs->ActivateGameUI();
		staticGameUIFuncs->OpenDialog_guess( dialog );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VGUI2_OnDisconnectFromServer( int eLoginFailure )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->OnDisconnectFromServer( eLoginFailure, "" );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int VGuiWrap2_IsInCareerMatch( void )
{
	if ( staticCareerUI )
		return g_careerState;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void *VguiWrap2_GetCareerUI( void )
{
	return staticCareerUI;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int VGuiWrap2_GetLocalizedStringLength( const char *label )
{
	if ( !label )
		return 0;

	wchar_t *str = vgui2::localize()->Find( label );
	if ( !str )
		return 0;

	return wcslen( str );
}

static int s_tutorMessageDecayData[256];

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void RegisterTutorMessageShown( int mid )
{
	if ( mid < 0 || mid >= 256 )
		return;

	s_tutorMessageDecayData[mid]++;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GetTimesTutorMessageShown( int mid )
{
	if ( mid < 0 || mid >= 256 )
		return -1;

	return s_tutorMessageDecayData[mid];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ProcessTutorMessageDecayBuffer( int *buffer, int bufferLength )
{
	ResetTutorMessageDecayData();

	int amountToCopy = min( bufferLength, 256 );
	if ( amountToCopy > 0 )
	{
		memcpy( s_tutorMessageDecayData, buffer, amountToCopy * sizeof( int ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ConstructTutorMessageDecayBuffer( int *buffer, int bufferLength )
{
	if ( !buffer )
		return;

	memset( buffer, 0, bufferLength * sizeof( int ) );

	int amountToCopy = min( 256, bufferLength );
	if ( amountToCopy > 0 )
	{
		memcpy( buffer, s_tutorMessageDecayData, amountToCopy * sizeof( int ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ResetTutorMessageDecayData( void )
{
	memset( s_tutorMessageDecayData, 0, sizeof( s_tutorMessageDecayData ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VguiWrap2_GetMouseDelta( int *x, int *y )
{
	g_BaseUISurface.GetMouseDelta( x, y );
}

}
