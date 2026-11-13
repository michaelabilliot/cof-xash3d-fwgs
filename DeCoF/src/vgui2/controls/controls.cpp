//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include <locale.h>
#include <string.h>

#include "interface.h"
#include "IKeyValues.h"
#include "FileSystem.h"
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>

IKeyValues *g_pKeyValuesInterface = NULL;
IKeyValues *keyvalues()
{
	return g_pKeyValuesInterface;
}

namespace vgui2
{

IInput *g_pInputInterface = NULL;
IInput *input()
{
	return g_pInputInterface;
}

ISchemeManager *g_pSchemeInterface = NULL;
ISchemeManager *scheme()
{
	return g_pSchemeInterface;
}

ISurface *g_pSurfaceInterface = NULL;
ISurface *surface()
{
	return g_pSurfaceInterface;
}

ISystem *g_pSystemInterface = NULL;
ISystem *system()
{
	return g_pSystemInterface;
}

IVGui *g_pVGuiInterface = NULL;
IVGui *ivgui()
{
	return g_pVGuiInterface;
}

IPanel *g_pPanelInterface = NULL;
IPanel *ipanel()
{
	return g_pPanelInterface;
}

IFileSystem *g_pFileSystemInterface = NULL;
IFileSystem *filesystem()
{
	return g_pFileSystemInterface;
}

ILocalize *g_pLocalizeInterface = NULL;
ILocalize *localize()
{
	return g_pLocalizeInterface;
}

//-----------------------------------------------------------------------------
// Purpose: finds a particular interface in the factory set
//-----------------------------------------------------------------------------
static void *InitializeInterface_guess( char const *interfaceName, CreateInterfaceFn *factoryList, int numFactories )
{
	void *retval;

	for ( int i = 0; i < numFactories; i++ )
	{
		CreateInterfaceFn factory = factoryList[ i ];
		if ( !factory )
			continue;

		retval = factory( interfaceName, NULL );
		if ( retval )
			return retval;
	}

	// No provider for requested interface!!!
	return NULL;
}

static char g_szControlsModuleName[256];

//-----------------------------------------------------------------------------
// Purpose: Initializes the controls
//-----------------------------------------------------------------------------
bool VGuiControls_Init( const char *moduleName, CreateInterfaceFn *factoryList, int numFactories )
{
	strncpy( g_szControlsModuleName, moduleName, sizeof( g_szControlsModuleName ) );
	g_szControlsModuleName[ sizeof( g_szControlsModuleName ) - 1 ] = 0;

	setlocale( LC_CTYPE, "" );
	setlocale( LC_TIME, "" );
	setlocale( LC_COLLATE, "" );
	setlocale( LC_MONETARY, "" );

	g_pVGuiInterface = (IVGui *)InitializeInterface_guess( VGUI_IVGUI_INTERFACE_VERSION, factoryList, numFactories );
	g_pPanelInterface = (IPanel *)InitializeInterface_guess( VGUI_PANEL_INTERFACE_VERSION, factoryList, numFactories );
	g_pSurfaceInterface = (ISurface *)InitializeInterface_guess( VGUI_SURFACE_INTERFACE_VERSION, factoryList, numFactories );
	g_pSchemeInterface = (ISchemeManager *)InitializeInterface_guess( VGUI_SCHEME_INTERFACE_VERSION, factoryList, numFactories );
	g_pSystemInterface = (ISystem *)InitializeInterface_guess( VGUI_SYSTEM_INTERFACE_VERSION, factoryList, numFactories );
	g_pInputInterface = (IInput *)InitializeInterface_guess( VGUI_INPUT_INTERFACE_VERSION, factoryList, numFactories );
	g_pLocalizeInterface = (ILocalize *)InitializeInterface_guess( VGUI_LOCALIZE_INTERFACE_VERSION, factoryList, numFactories );
	g_pFileSystemInterface = (IFileSystem *)InitializeInterface_guess( FILESYSTEM_INTERFACE_VERSION, factoryList, numFactories );
	g_pKeyValuesInterface = (IKeyValues *)InitializeInterface_guess( KEYVALUES_INTERFACE_VERSION, factoryList, numFactories );

	if ( !g_pVGuiInterface )
		return false;

	g_pVGuiInterface->Init( factoryList, numFactories );

	if ( g_pKeyValuesInterface )
	{
		g_pKeyValuesInterface->RegisterSizeofKeyValues( sizeof(KeyValues) );
	}

	if ( g_pSchemeInterface &&
		 g_pSurfaceInterface &&
		 g_pSystemInterface &&
		 g_pInputInterface &&
		 g_pVGuiInterface &&
		 g_pFileSystemInterface &&
		 g_pLocalizeInterface &&
		 g_pKeyValuesInterface &&
		 g_pPanelInterface )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VGuiControls_Shutdown()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the module this has been compiled into
//-----------------------------------------------------------------------------
const char *GetControlsModuleName()
{
	return g_szControlsModuleName;
}

} // namespace vgui2
