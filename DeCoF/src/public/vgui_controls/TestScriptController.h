//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef TESTSCRIPTCONTROLLER_H
#define TESTSCRIPTCONTROLLER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <UtlVector.h>
#include <utlsymbol.h>

namespace vgui = vgui2;

namespace vgui2
{

//-----------------------------------------------------------------------------
// Purpose: Runs vgui test scripts
//-----------------------------------------------------------------------------
class TestScriptController : public Panel
{
	DECLARE_CLASS_SIMPLE( TestScriptController, vgui::Panel );

public:
	TestScriptController();
	~TestScriptController();

	// runs a script file
	bool ExecuteScript(const char *scriptFile);

protected:
	virtual void OnTick();

private:
	void ClearScriptCommandBuffer();
	void ExecuteNextScriptCommand();
	void AbortOnAnyErrorLogsPresent();
	VPANEL FindMessageTarget(const char *targetName);
	VPANEL FindMessageTargetInSubPanel(VPANEL parent, const char *targetName);
	void MakeValueString(const char *value, char *output, int outputBufferSize);
	void CreateNewUniqueTestString();
	void OnPanelNotFoundError(const char *targetName);
	int PrintMatchingPanels(VPANEL startPanel, const char *targetName);
	void CalculateFullNameForPanel(VPANEL panel, char *name, int nameBufSize);

	MESSAGE_FUNC_FLOAT( OnDelay, "Delay", time );
	MESSAGE_FUNC_FLOAT( OnSetScriptRunRate, "SetScriptRunRate", time );

	enum ScriptCommandType_e
	{
		SCRIPTCOMMAND_MESSAGE,
		SCRIPTCOMMAND_WAITUNTILVISIBLE,
		SCRIPTCOMMAND_WAITUNTILENABLED,
		SCRIPTCOMMAND_REGISTRYSET,
		SCRIPTCOMMAND_EXEC,
	};

	struct ScriptCommand_t
	{
		ScriptCommandType_e cmd;
		UtlSymId_t target;
		KeyValues *msg;
		char *originalLine;
	};

	struct ScriptFunction_t
	{
		UtlSymId_t name;
		CUtlVector<ScriptCommand_t> scriptCommands;
	};

	struct ScriptProgramCounter_t
	{
		int currentScriptFunctionIndex;
		int currentScriptCommandIndex;
	};

	bool LoadScriptFile(const char *scriptFile);
	bool ParseScriptFile(const char *pMem, int length);
	const char *ParseFunction(ScriptFunction_t &function, const char *pMem);

	CUtlSymbolTable m_TargetSymbols;
	CUtlVector<ScriptFunction_t> m_ScriptFunctions;
	CUtlVector<ScriptProgramCounter_t> m_CurrentScriptCommandIndex;
	float m_flNextScriptCommandExecTime;
	float m_flTimeBetweenRunningEachScriptLine;
	char m_szUniqueTestString[256];
	bool m_bParsingErrorOccured;
};

} // namespace vgui2

#endif // TESTSCRIPTCONTROLLER_H
