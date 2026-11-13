//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VGUISYSTEMMODULELOADER_H
#define VGUISYSTEMMODULELOADER_H

#ifdef _WIN32
#pragma once
#endif

#include "IVGuiModuleLoader.h"
#include <UtlVector.h>

class KeyValues;
class IVGuiModule;

//-----------------------------------------------------------------------------
// Purpose: Handles loading/unloading of different vgui modules into a shared context
//-----------------------------------------------------------------------------
class CVGuiSystemModuleLoader : public IVGuiModuleLoader
{
public:
	CVGuiSystemModuleLoader();
	~CVGuiSystemModuleLoader();

	// loads all the modules in the platform
	bool LoadPlatformModules(CreateInterfaceFn *factorylist, int factorycount, bool useSteamModules);

	// returns true if the module loader has loaded the modules
	bool IsPlatformReady();

	// needs to be called every frame - updates all the modules states
	void RunFrame();

	// returns number of modules loaded
	virtual int GetModuleCount();

	// returns the string menu name (unlocalized) of a module
	// moduleIndex is of the range [0, GetModuleCount())
	virtual const char *GetModuleLabel(int moduleIndex);

	// returns true if the module is hidden
	bool IsModuleHidden(int moduleIndex);

	// returns a modules interface factory
	virtual CreateInterfaceFn GetModuleFactory(int moduleIndex);

	// brings the specified module to the foreground
	virtual bool ActivateModule(int moduleIndex);
	virtual bool ActivateModule(const char *moduleName);

	// deactivates all the modules (puts them into in inactive but recoverable state)
	void DeactivatePlatformModules();

	// Reenables all the deactivated platform modules
	void ReactivatePlatformModules();

	// shuts down all the platform modules
	void ShutdownPlatformModules();

	// unloads all the modules
	void UnloadPlatformModules();

	// posts a message to all active modules
	void PostMessageToAllModules(KeyValues *message);

	// sets the the platform should update and restart when it quits
	virtual void SetPlatformToRestart();

	// returns true if the platform should restart after exit
	bool ShouldPlatformRestart();

private:
	// sets up all the modules for use
	bool InitializeAllModules(CreateInterfaceFn *factorylist, int factorycount);

	bool m_bModulesInitialized;
	bool m_bPlatformShouldRestartAfterExit;

	struct module_t
	{
		CSysModule *module;
		IVGuiModule *moduleInterface;
		KeyValues *data;
	};
	CUtlVector<module_t> m_Modules;
	KeyValues *m_pPlatformModuleData;
};

extern CVGuiSystemModuleLoader g_VModuleLoader;

#endif // VGUISYSTEMMODULELOADER_H
