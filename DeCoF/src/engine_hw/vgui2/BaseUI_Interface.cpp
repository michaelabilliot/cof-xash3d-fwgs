//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine side of the VGUI2 user interface: loads vgui2.dll and
//			GameUI.dll, builds the root panels and runs the GameUI frame
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#undef PostMessage	// vgui2::Panel::PostMessage
#include <string.h>
#include <stdio.h>

#include "BaseUI_Interface.h"
#include "BaseUISurface.h"
#include "BasePanel.h"
#include "IClientVGUI.h"
#include "IGameUI.h"
#include "IGameConsole.h"
#include "ICareerUI.h"
#include "ienginevgui.h"
#include "EngineSurface.h"
#include "FileSystem.h"
#include "UtlVector.h"

#include <vgui/IInputInternal.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Controls.h>

// the set of includes the cdll_int interface needs
#include "mathlib.h"
#include "wrect.h"
typedef int ( *pfnUserMsgHook )( const char *pszName, int iSize, void *pbuf );
#include "cdll_int.h"
#include "keydefs.h"

using namespace vgui2;

struct IBaseSystem;
class SystemWrapper;

typedef unsigned int SteamHandle_t;

extern "C"
{
extern cldll_func_t	cl_funcs;
extern int			g_bIsDedicatedServer;

void	ClientDLL_ActivateMouse( void );
void	ClientDLL_DeactivateMouse( void );
int		COM_ExpandFilename( char *filename );
void	COM_FixSlashes( char *pname );
void	FS_GetLocalCopy( char *pszFileName );
}

extern SystemWrapper gSystemWrapper;
extern BaseUISurface g_BaseUISurface;
extern IInputInternal *g_InputInternal;
extern CUtlVector<char *> g_fallbackLocalizationFiles;

namespace vgui2
{
ISurface *g_pSurface = NULL;
}

cl_enginefunc_t gEngfuncs;

static CStaticPanel *staticPanel = NULL;

CEnginePanel *staticClientDLLPanel = NULL;
CEnginePanel *staticGameUIPanel = NULL;

BaseUISurface *staticSurface = NULL;
IMouseControl *mousecontrol = NULL;

CBaseUI *g_pBaseUI = NULL;

static CBaseUI g_BaseUI;

IGameUI *staticGameUIFuncs = NULL;
IGameConsole *staticGameConsole = NULL;
ICareerUI *staticCareerUI = NULL;

IClientVGUI *staticClient = NULL;

bool m_bConsoleShowing;

//-----------------------------------------------------------------------------
// Purpose: the engine's single CBaseUI
//-----------------------------------------------------------------------------
CBaseUI &BaseUI()
{
	return g_BaseUI;
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBaseUI, IBaseUI, BASEUI_INTERFACE_VERSION, g_BaseUI );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseUI::CBaseUI()
{
	g_pBaseUI = this;
	m_hVGuiModule = NULL;
	m_hStaticGameUIModule = NULL;
	m_bConsoleShowing = false;
	m_bHidingGameUI = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBaseUI::~CBaseUI()
{
	g_pBaseUI = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: loads vgui2.dll and GameUI.dll and collects their factories
//-----------------------------------------------------------------------------
void CBaseUI::Initialize( CreateInterfaceFn *factories, int count )
{
	// already initialized
	if ( staticGameUIFuncs )
		return;

	CreateInterfaceFn engineFactory = factories[0];
	CreateInterfaceFn fileSystemFactory = factories[1];

	IFileSystem *fs = (IFileSystem *)fileSystemFactory( FILESYSTEM_INTERFACE_VERSION, NULL );
	fs->GetLocalCopy( "vgui2.dll" );

	m_hVGuiModule = Sys_LoadModule( "vgui2.dll" );
	CreateInterfaceFn vguiFactory = Sys_GetFactory( m_hVGuiModule );

	m_FactoryList[0] = engineFactory;
	m_FactoryList[1] = vguiFactory;
	m_FactoryList[2] = fileSystemFactory;
	m_iNumFactories = 3;

	char szDllName[512];
	_snprintf( szDllName, sizeof( szDllName ), "cl_dlls\\GameUI.dll" );
	COM_ExpandFilename( szDllName );
	COM_FixSlashes( szDllName );
	FS_GetLocalCopy( szDllName );

	m_hStaticGameUIModule = Sys_LoadModule( szDllName );
	CreateInterfaceFn gameUIFactory = Sys_GetFactory( m_hStaticGameUIModule );
	m_FactoryList[m_iNumFactories] = gameUIFactory;
	if ( !gameUIFactory )
	{
	}
	else
	{
		staticGameUIFuncs = (IGameUI *)gameUIFactory( GAMEUI_INTERFACE_VERSION, NULL );
		staticGameConsole = (IGameConsole *)gameUIFactory( GAMECONSOLE_INTERFACE_VERSION, NULL );
		staticCareerUI = (ICareerUI *)gameUIFactory( CAREERUI_INTERFACE_VERSION, NULL );
		m_iNumFactories++;
	}

	g_InputInternal = (IInputInternal *)vguiFactory( VGUI_INPUTINTERNAL_INTERFACE_VERSION, NULL );
	if ( !g_InputInternal )
	{
		__asm int 3
	}

	if ( cl_funcs.pClientFactory )
	{
		CreateInterfaceFn clientDLLFactory = (CreateInterfaceFn)cl_funcs.pClientFactory();
		if ( clientDLLFactory )
		{
			m_FactoryList[m_iNumFactories] = clientDLLFactory;
			m_iNumFactories++;
		}

		staticClient = (IClientVGUI *)clientDLLFactory( VCLIENTVGUI_INTERFACE_VERSION, NULL );
	}
	else
	{
		m_FactoryList[4] = NULL;
	}

	vgui2::VGuiControls_Init( "BaseUI", m_FactoryList, m_iNumFactories );
}

//-----------------------------------------------------------------------------
// Purpose: builds the root panels and starts the GameUI and the client UI
//-----------------------------------------------------------------------------
void CBaseUI::Start( struct cl_enginefuncs_s *engineFuncs, int interfaceVersion )
{
	memcpy( &gEngfuncs, engineFuncs, sizeof( gEngfuncs ) );

	staticPanel = new CStaticPanel( NULL, "StaticPanel" );
	staticPanel->SetBounds( 0, 0, 40, 30 );
	staticPanel->SetPaintBorderEnabled( false );
	staticPanel->SetPaintBackgroundEnabled( false );
	staticPanel->SetPaintEnabled( false );
	staticPanel->SetCursor( dc_none );
	staticPanel->SetVisible( true );
	staticPanel->SetZPos( 0 );

	staticSurface = (BaseUISurface *)m_FactoryList[0]( VGUI_SURFACE_INTERFACE_VERSION, NULL );
	vgui2::g_pSurface = staticSurface;
	staticSurface->Init( staticPanel->GetVPanel(), (IEngineSurface *)m_FactoryList[0]( ENGINESURFACE_INTERFACE_VERSION, NULL ) );
	mousecontrol = staticSurface;

	if ( !vgui2::scheme()->LoadSchemeFromFile( "resource/trackerscheme.res", "BaseUI" ) )
	{
		gEngfuncs.Con_Printf( "Error loading '%s'\n", "resource/trackerScheme.res" );
	}

	vgui2::localize()->AddFile( vgui2::filesystem(), "resource/valve_%language%.txt" );

	char szGameDir[32];
	strncpy( szGameDir, gEngfuncs.pfnGetGameDirectory(), sizeof( szGameDir ) - 1 );
	szGameDir[ sizeof( szGameDir ) - 1 ] = 0;

	if ( strcmp( szGameDir, "cryoffear" ) )
	{
		char szFileName[128];
		_snprintf( szFileName, sizeof( szFileName ) - 1, "resource/%s_%%language%%.txt", szGameDir );
		szFileName[ sizeof( szFileName ) - 1 ] = 0;
		vgui2::localize()->AddFile( vgui2::filesystem(), szFileName );
	}

	for ( int i = 0; i < g_fallbackLocalizationFiles.Count(); i++ )
	{
		char *str = g_fallbackLocalizationFiles[i];
		vgui2::localize()->AddFile( vgui2::filesystem(), str );
		delete [] str;
	}
	g_fallbackLocalizationFiles.RemoveAll();

	vgui2::ivgui()->Start();
	vgui2::ivgui()->SetSleep( false );

	staticClientDLLPanel = new CEnginePanel( staticPanel, "BaseClientPanel" );
	staticClientDLLPanel->SetPaintBorderEnabled( false );
	staticClientDLLPanel->SetPaintBackgroundEnabled( false );
	staticClientDLLPanel->SetPaintEnabled( false );
	staticClientDLLPanel->SetVisible( true );
	staticClientDLLPanel->SetCursor( dc_none );
	staticClientDLLPanel->SetZPos( 25 );

	staticGameUIPanel = new CEnginePanel( staticPanel, "BaseGameUIPanel" );
	staticGameUIPanel->SetPaintBorderEnabled( false );
	staticGameUIPanel->SetPaintBackgroundEnabled( false );
	staticGameUIPanel->SetPaintEnabled( false );
	staticGameUIPanel->SetVisible( true );
	staticGameUIPanel->SetCursor( dc_none );
	staticGameUIPanel->SetZPos( 50 );

	if ( staticGameUIFuncs )
	{
		staticGameUIFuncs->Initialize( m_FactoryList, m_iNumFactories );
	}

	if ( staticClient )
	{
		staticClient->Initialize( m_FactoryList, m_iNumFactories );
		staticSurface->SetVGUI2MouseControl( true );
	}

	if ( staticGameUIFuncs )
	{
		staticGameUIFuncs->Start( &gEngfuncs, CLDLL_INTERFACE_VERSION, (IBaseSystem *)&gSystemWrapper );
	}

	staticClientDLLPanel->SetScheme( "ClientScheme" );

	if ( staticClient )
	{
		staticClient->Start();
		staticClient->SetParent( staticClientDLLPanel->GetVPanel() );
	}

	int swide, stall;
	vgui2::surface()->GetScreenSize( swide, stall );
	staticPanel->SetBounds( 0, 0, swide, stall );
	staticGameUIPanel->SetBounds( 0, 0, swide, stall );
	staticClientDLLPanel->SetBounds( 0, 0, swide, stall );

	staticGameConsole->Initialize();
	if ( staticGameConsole )
	{
		staticGameConsole->SetParent( staticGameUIPanel->GetVPanel() );
	}

	staticGameUIFuncs->ActivateGameUI();

	char *pszCmdLine_guess = GetCommandLine();
	if ( strstr( pszCmdLine_guess, "-console" ) )
	{
		staticGameConsole->Activate();
		m_bConsoleShowing = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: shuts the GameUI down and unloads GameUI.dll and vgui2.dll
//-----------------------------------------------------------------------------
void CBaseUI::Shutdown()
{
	vgui2::ivgui()->RunFrame();

	staticGameUIFuncs->Shutdown();

	char szGameDir[32];
	strncpy( szGameDir, gEngfuncs.pfnGetGameDirectory(), sizeof( szGameDir ) - 1 );
	szGameDir[ sizeof( szGameDir ) - 1 ] = 0;

	if ( staticClient && ( !_stricmp( szGameDir, "cstrike" ) || !_stricmp( szGameDir, "czero" ) ) )
	{
		staticClient->Shutdown();
	}

	Sys_UnloadModule( m_hStaticGameUIModule );
	m_hStaticGameUIModule = NULL;

	staticGameUIFuncs = NULL;
	staticGameConsole = NULL;
	staticCareerUI = NULL;

	vgui2::system()->SaveUserConfigFile();

	Sys_UnloadModule( m_hVGuiModule );
	m_hVGuiModule = NULL;

	staticSurface->Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: passes a window message on to the surface
//-----------------------------------------------------------------------------
void CBaseUI::CallEngineSurfaceAppHandler( void *hwnd, unsigned int msg, unsigned int wparam, long lparam )
{
	if ( staticSurface )
	{
		staticSurface->AppHandler( (HWND)hwnd, msg, wparam, lparam );
	}
}

//-----------------------------------------------------------------------------
// Purpose: closes the menus and gives the screen back to the game
//-----------------------------------------------------------------------------
void CBaseUI::HideGameUI()
{
	staticGameUIFuncs->HideGameUI();
	staticGameConsole->Hide();

	const char *levelName = gEngfuncs.pfnGetLevelName();
	if ( levelName && levelName[0] )
	{
		staticGameUIPanel->SetVisible( false );
		staticGameUIPanel->SetPaintBackgroundEnabled( false );
		staticClientDLLPanel->SetVisible( true );
		staticClientDLLPanel->SetMouseInputEnabled( true );

		if ( staticClient )
		{
			staticClient->ActivateClientUI();
		}
	}

	if ( vgui2::input()->IsMouseDown( MOUSE_LEFT ) )
	{
		m_bHidingGameUI = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: opens the menus
//-----------------------------------------------------------------------------
void CBaseUI::ActivateGameUI()
{
	staticGameUIFuncs->ActivateGameUI();
	staticGameUIPanel->SetVisible( true );
	staticClientDLLPanel->SetVisible( false );
	staticClientDLLPanel->SetMouseInputEnabled( false );

	if ( staticClient )
	{
		staticClient->HideClientUI();
		staticClient->HideScoreBoard();
	}

	if ( m_bConsoleShowing )
	{
		staticGameConsole->Activate();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseUI::HideConsole()
{
	m_bConsoleShowing = false;
	staticGameConsole->Hide();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseUI::ShowConsole()
{
	m_bConsoleShowing = true;
	staticGameConsole->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: returns 1 when the user interface takes the key
//-----------------------------------------------------------------------------
int CBaseUI::Key_Event( int down, int keynum, const char *pszCurrentBinding )
{
	// the console key goes to the engine
	if ( keynum == '~' || keynum == '`' )
		return 0;

	if ( down && keynum == K_ESCAPE )
	{
		const char *levelName = gEngfuncs.pfnGetLevelName();
		if ( staticGameUIFuncs->IsGameUIActive() && levelName && levelName[0] )
		{
			HideGameUI();
		}
		else
		{
			ActivateGameUI();
		}
		return 1;
	}

	// eat the click that closed the menus
	if ( m_bHidingGameUI && keynum == K_MOUSE1 && down )
	{
		m_bHidingGameUI = false;
		return 1;
	}

	if ( vgui2::surface()->NeedKBInput() )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: runs a VGUI2 frame and paints the panel tree
//-----------------------------------------------------------------------------
void CBaseUI::Paint( int x, int y, int right, int bottom )
{
	if ( !staticSurface )
		return;

	VPANEL pVPanel = staticSurface->GetEmbeddedPanel();
	if ( !pVPanel )
		return;

	staticGameUIFuncs->RunFrame();
	vgui2::ivgui()->RunFrame();

	g_BaseUISurface.SetScreenBounds( x, y, right - x, bottom - y );

	staticPanel->SetBounds( 0, 0, right, bottom );
	staticGameUIPanel->SetBounds( 0, 0, right, bottom );
	staticClientDLLPanel->SetBounds( 0, 0, right, bottom );

	staticPanel->Repaint();

	vgui2::surface()->PaintTraverse( pVPanel );
}

//-----------------------------------------------------------------------------
// Purpose: the panels of the engine user interface, for the client and GameUI
//-----------------------------------------------------------------------------
class CEngineVGui : public IEngineVGui
{
public:
	CEngineVGui();

	virtual vgui2::VPANEL GetPanel( VGUIPANEL type );
	virtual bool SteamRefreshLogin( const char *password, bool isSecure );
	virtual bool SteamProcessCall( bool *finished, TSteamProgress *progress, TSteamError *steamError );

	void SetEngineVisible( bool state );

private:
	bool m_bVisible;
	SteamHandle_t m_hRefreshLoginHandle;
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEngineVGui::CEngineVGui()
{
	m_bVisible = true;
	m_hRefreshLoginHandle = 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEngineVGui::SteamRefreshLogin( const char *password, bool isSecure )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEngineVGui::SteamProcessCall( bool *finished, TSteamProgress *progress, TSteamError *steamError )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEngineVGui::SetEngineVisible( bool state )
{
	m_bVisible = state;

	if ( g_bIsDedicatedServer )
		return;

	if ( state )
	{
		ClientDLL_ActivateMouse();
	}
	else
	{
		ClientDLL_DeactivateMouse();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the root, client or GameUI panel
//-----------------------------------------------------------------------------
vgui2::VPANEL CEngineVGui::GetPanel( VGUIPANEL type )
{
	switch ( type )
	{
	default:
		return staticPanel->GetVPanel();
	case PANEL_CLIENTDLL:
		return staticClientDLLPanel->GetVPanel();
	case PANEL_GAMEUIDLL:
		return staticGameUIPanel->GetVPanel();
	}
}

EXPOSE_SINGLE_INTERFACE( CEngineVGui, IEngineVGui, VENGINE_VGUI_VERSION );
