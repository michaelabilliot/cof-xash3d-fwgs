//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CONTROLS_H
#define CONTROLS_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include "interface.h"

class IFileSystem;
class IKeyValues;

// #include <IKeyValues.h>
IKeyValues *keyvalues();

namespace vgui2
{

// handles the initialization of the vgui interfaces
// interfaces (listed below) are first attempted to be loaded from primaryProvider, then secondaryProvider
// moduleName should be the name of the module that this instance of the vgui_controls has been compiled into
bool VGuiControls_Init( const char *moduleName, CreateInterfaceFn *factoryList, int numFactories );
void VGuiControls_Shutdown();

// returns the name of the module as specified above
const char *GetControlsModuleName();

// set of accessor functions to vgui interfaces
// the appropriate header file for each is listed above the item

// #include <vgui/IInput.h>
class IInput *input();

// #include <vgui/IScheme.h>
class ISchemeManager *scheme();

// #include <vgui/ISurface.h>
class ISurface *surface();

// #include <vgui/ISystem.h>
class ISystem *system();

// #include <vgui/IVGui.h>
class IVGui *ivgui();

// #include <vgui/IPanel.h>
class IPanel *ipanel();

// #include "FileSystem.h"
IFileSystem *filesystem();

// #include <vgui/ILocalize.h>
class ILocalize *localize();

} // namespace vgui2

#endif // CONTROLS_H
