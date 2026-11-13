//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Loads the platform modules (steam/games/PlatformMenu.vdf) into the
//			GameUI vgui context
//
// $NoKeywords: $
//=============================================================================

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "VGuiSystemModuleLoader.h"
#include "EngineInterface.h"
#include "Taskbar.h"
#include "IVGuiModule.h"

#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/KeyValues.h>
#include <vgui_controls/Controls.h>
#include "FileSystem.h"

// instance of class
CVGuiSystemModuleLoader g_VModuleLoader;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CVGuiSystemModuleLoader, IVGuiModuleLoader, VGUIMODULELOADER_INTERFACE_VERSION, g_VModuleLoader);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::CVGuiSystemModuleLoader()
{
	m_bModulesInitialized = false;
	m_bPlatformShouldRestartAfterExit = false;
	m_pPlatformModuleData = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::~CVGuiSystemModuleLoader()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module loader has acquired the platform mutex and loaded the modules
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsPlatformReady()
{
	return m_bModulesInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: sets up all the modules for use
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::InitializeAllModules(CreateInterfaceFn *factorylist, int factorycount)
{
	bool bSuccess = true;

	// Init vgui in the modules
	int i;
	for (i = 0; i < m_Modules.Size(); i++)
	{
		if (!m_Modules[i].moduleInterface->Initialize(factorylist, factorycount))
		{
			bSuccess = false;
			vgui2::ivgui()->DPrintf2("Platform Error: module failed to initialize\n");
		}
	}

	// create a table of all the loaded modules
	CreateInterfaceFn *moduleFactories = (CreateInterfaceFn *)_alloca(sizeof(CreateInterfaceFn) * m_Modules.Size());
	for (i = 0; i < m_Modules.Size(); i++)
	{
		moduleFactories[i] = Sys_GetFactory(m_Modules[i].module);
	}

	// give the modules a chance to link themselves together
	for (i = 0; i < m_Modules.Size(); i++)
	{
		if (!m_Modules[i].moduleInterface->PostInitialize(moduleFactories, m_Modules.Size()))
		{
			bSuccess = false;
			vgui2::ivgui()->DPrintf2("Platform Error: module failed to initialize\n");
		}

		m_Modules[i].moduleInterface->SetParent(g_pTaskbar->GetVPanel());
		g_pTaskbar->AddTask(m_Modules[i].moduleInterface->GetPanel());
	}

	m_bModulesInitialized = true;
	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: Loads and initializes all the modules specified in the platform file
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::LoadPlatformModules(CreateInterfaceFn *factorylist, int factorycount, bool useSteamModules)
{
	bool bSuccess = true;

	// load platform menu
	KeyValues *kv = new KeyValues("Platform");
	if (!kv->LoadFromFile(vgui2::filesystem(), "steam/games/PlatformMenu.vdf", NULL))
	{
		kv->deleteThis();
		return false;
	}

	// walk the platform menu loading all the interfaces
	KeyValues *menuKeys = kv->FindKey("Menu", true);
	for (KeyValues *it = menuKeys->GetFirstSubKey(); it != NULL; it = it->GetNextKey())
	{
		// see if we should skip steam modules
		if (!useSteamModules && it->GetInt("SteamApp"))
			continue;

		const char *pchInterface = it->GetString("interface");

		// don't load the tracker module
		if (!stricmp(pchInterface, "VGuiModuleTracker001"))
			continue;

		// get copy out of steam cache
		const char *dllPath = it->GetString("dll");
		char szDir[512];
		if (!vgui2::filesystem()->GetLocalPath(dllPath, szDir, sizeof(szDir)))
		{
			engine->Con_Printf("DLL Error: couldn't find %s, not loading\n", dllPath);
			vgui2::ivgui()->DPrintf2("Platform Error: couldn't find %s, not loading\n", it->GetString("dll"));
			bSuccess = false;
			continue;
		}
		vgui2::filesystem()->GetLocalCopy(szDir);

		// load the module
		CSysModule *mod = Sys_LoadModule(szDir);
		if (!mod)
		{
			engine->Con_Printf("DLL 2 Platform Error: bad module '%s', not loading\n", it->GetString("dll"));
			vgui2::ivgui()->DPrintf2("Platform Error: bad module '%s', not loading\n", it->GetString("dll"));
			bSuccess = false;
			continue;
		}

		// make sure we get the right version
		IVGuiModule *moduleInterface = (IVGuiModule *)Sys_GetFactory(mod)(pchInterface, NULL);
		if (!moduleInterface)
		{
			engine->Con_Printf("DLL 3 Platform Error: module version ('%s, %s) invalid, not loading\n", it->GetString("dll"), it->GetString("interface"));
			vgui2::ivgui()->DPrintf2("Platform Error: module version ('%s, %s) invalid, not loading\n", it->GetString("dll"), it->GetString("interface"));
			bSuccess = false;
			continue;
		}

		// store off the module
		int newIndex = m_Modules.AddToTail();
		m_Modules[newIndex].module = mod;
		m_Modules[newIndex].moduleInterface = moduleInterface;
		m_Modules[newIndex].data = it;
	}

	m_pPlatformModuleData = kv;
	return InitializeAllModules(factorylist, factorycount) && bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: gives all platform modules a chance to Shutdown gracefully
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ShutdownPlatformModules()
{
	// static include guard to prevent recursive calls
	static bool runningFunction = false;
	if (runningFunction)
		return;

	runningFunction = true;

	// deactivate all the modules first
	DeactivatePlatformModules();

	// give all the modules notice of quit
	int i;
	for (i = 0; i < m_Modules.Size(); i++)
	{
		vgui2::ivgui()->PostMessage(m_Modules[i].moduleInterface->GetPanel(), new KeyValues("Command", "command", "Quit"), NULL);
	}

	for (i = 0; i < m_Modules.Size(); i++)
	{
		m_Modules[i].moduleInterface->Shutdown();
	}

	runningFunction = false;
}

//-----------------------------------------------------------------------------
// Purpose: Deactivates all the modules (puts them into in inactive but recoverable state)
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::DeactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Size(); i++)
	{
		m_Modules[i].moduleInterface->Deactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reenables all the deactivated platform modules
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ReactivatePlatformModules()
{
	for (int i = 0; i < m_Modules.Size(); i++)
	{
		m_Modules[i].moduleInterface->Reactivate();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Disables and unloads platform
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::UnloadPlatformModules()
{
	for (int i = 0; i < m_Modules.Count(); i++)
	{
		Sys_UnloadModule(m_Modules[i].module);
	}

	m_Modules.RemoveAll();

	if (m_pPlatformModuleData)
	{
		m_pPlatformModuleData->deleteThis();
		m_pPlatformModuleData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::RunFrame()
{
	return;

	for (int i = 0; i < m_Modules.Count(); i++)
	{
		m_Modules[i].moduleInterface->RunFrame_guess();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns number of modules loaded
//-----------------------------------------------------------------------------
int CVGuiSystemModuleLoader::GetModuleCount()
{
	return m_Modules.Size();
}

//-----------------------------------------------------------------------------
// Purpose: returns the string menu name (unlocalized) of a module
//			moduleIndex is of the range [0, GetModuleCount())
//-----------------------------------------------------------------------------
const char *CVGuiSystemModuleLoader::GetModuleLabel(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetString("MenuName", "< unknown >");
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module is hidden
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsModuleHidden(int moduleIndex)
{
	return m_Modules[moduleIndex].data->GetInt("Hidden", 0);
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule(int moduleIndex)
{
	if (!m_Modules.IsValidIndex(moduleIndex))
		return false;

	m_Modules[moduleIndex].moduleInterface->Activate();

	if (g_pTaskbar)
	{
		wchar_t *wTitle;
		wchar_t w_szTitle[1024];

		wTitle = vgui2::localize()->Find(m_Modules[moduleIndex].data->GetName());
		if (!wTitle)
		{
			vgui2::localize()->ConvertANSIToUnicode(m_Modules[moduleIndex].data->GetName(), w_szTitle, sizeof(w_szTitle));
			wTitle = w_szTitle;
		}

		g_pTaskbar->SetTitle(m_Modules[moduleIndex].moduleInterface->GetPanel(), wTitle);
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: activates a module by name
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule(const char *moduleName)
{
	for (int i = 0; i < GetModuleCount(); i++)
	{
		if (!stricmp(GetModuleLabel(i), moduleName) || !stricmp(m_Modules[i].data->GetName(), moduleName))
		{
			ActivateModule(i);
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns a modules interface factory
//-----------------------------------------------------------------------------
CreateInterfaceFn CVGuiSystemModuleLoader::GetModuleFactory(int moduleIndex)
{
	return Sys_GetFactory(m_Modules[moduleIndex].module);
}

//-----------------------------------------------------------------------------
// Purpose: posts a message to all active modules
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::PostMessageToAllModules(KeyValues *message)
{
	for (int i = 0; i < m_Modules.Size(); i++)
	{
		vgui2::ivgui()->PostMessage(m_Modules[i].moduleInterface->GetPanel(), message->MakeCopy(), NULL);
	}
	message->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: sets the the platform should update and restart when it quits
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::SetPlatformToRestart()
{
	m_bPlatformShouldRestartAfterExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the platform should restart after exit
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ShouldPlatformRestart()
{
	return m_bPlatformShouldRestartAfterExit;
}
