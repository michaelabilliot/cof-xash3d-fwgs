//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/KeyValues.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/TestScriptController.h>

#include "FileSystem.h"
#include "characterset.h"
#include "filesystem_helpers.h"

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TestScriptController::TestScriptController() : BaseClass(NULL, "TestScriptController"), m_TargetSymbols(0, 256, true)
{
	SetVisible(false);
	ClearScriptCommandBuffer();
	ivgui()->AddTickSignal(GetVPanel(), 50);
	m_szUniqueTestString[0] = 0;
	m_bParsingErrorOccured = false;
	m_flTimeBetweenRunningEachScriptLine = 0.1f;
	m_ScriptFunctions.EnsureCapacity(256);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TestScriptController::~TestScriptController()
{
}

//-----------------------------------------------------------------------------
// Purpose: runs the next script command when it is time
//-----------------------------------------------------------------------------
void TestScriptController::OnTick()
{
	if (m_flNextScriptCommandExecTime != 0.0f && m_flNextScriptCommandExecTime <= system()->GetFrameTime())
	{
		AbortOnAnyErrorLogsPresent();
		ExecuteNextScriptCommand();
	}
}

//-----------------------------------------------------------------------------
// Purpose: executes the next command in the current function
//-----------------------------------------------------------------------------
void TestScriptController::ExecuteNextScriptCommand()
{
	if (!m_CurrentScriptCommandIndex.Count())
		return;

	ScriptProgramCounter_t &pc = m_CurrentScriptCommandIndex[m_CurrentScriptCommandIndex.Count() - 1];
	ScriptFunction_t &function = m_ScriptFunctions[pc.currentScriptFunctionIndex];

	pc.currentScriptCommandIndex++;
	if (!function.scriptCommands.IsValidIndex(pc.currentScriptCommandIndex))
	{
		if (pc.currentScriptFunctionIndex == 0)
		{
			// we've finished the main function, the script is done
			ivgui()->DPrintf("TestScriptController : script completed successfully\n");
			m_flNextScriptCommandExecTime = 0.0f;
			pc.currentScriptCommandIndex = 0;
		}
		else
		{
			// return from the function
			m_CurrentScriptCommandIndex.Remove(m_CurrentScriptCommandIndex.Count() - 1);
			m_flNextScriptCommandExecTime = system()->GetFrameTime();
		}
		return;
	}

	ScriptCommand_t &command = function.scriptCommands[pc.currentScriptCommandIndex];
	const char *targetName = m_TargetSymbols.String(command.target);
	VPANEL target = FindMessageTarget(targetName);
	bool bRunNextLineImmediately = false;
	bool bOutputTrace = true;

	if (command.cmd == SCRIPTCOMMAND_EXEC)
	{
		// find the function
		int functionIndex = -1;
		for (int i = 0; i < m_ScriptFunctions.Count(); i++)
		{
			if (m_ScriptFunctions[i].name == command.target)
			{
				functionIndex = i;
				break;
			}
		}

		if (!m_ScriptFunctions.IsValidIndex(functionIndex))
		{
			OnPanelNotFoundError(m_TargetSymbols.String(command.target));
		}

		// call into the function
		int pcindex = m_CurrentScriptCommandIndex.AddToTail();
		m_CurrentScriptCommandIndex[pcindex].currentScriptFunctionIndex = functionIndex;
		m_CurrentScriptCommandIndex[pcindex].currentScriptCommandIndex = -1;
		bRunNextLineImmediately = true;
	}
	else if (command.cmd == SCRIPTCOMMAND_MESSAGE)
	{
		if (target)
		{
			PostMessage(target, command.msg->MakeCopy());
		}
		else
		{
			OnPanelNotFoundError(m_TargetSymbols.String(command.target));
		}
	}
	else if (command.cmd == SCRIPTCOMMAND_WAITUNTILVISIBLE)
	{
		if (!target || !ipanel()->IsVisible(target))
		{
			// stay on this line
			pc.currentScriptCommandIndex--;
			bOutputTrace = false;
		}
	}
	else if (command.cmd == SCRIPTCOMMAND_WAITUNTILENABLED)
	{
		if (!target)
		{
			OnPanelNotFoundError(m_TargetSymbols.String(command.target));
		}
		else if (!ipanel()->IsEnabled(target))
		{
			// stay on this line
			pc.currentScriptCommandIndex--;
			bOutputTrace = false;
		}
	}
	else if (command.cmd == SCRIPTCOMMAND_REGISTRYSET)
	{
		if (!stricmp(command.msg->GetName(), "Set"))
		{
			system()->SetRegistryString(command.msg->GetString("key"), command.msg->GetString("value"));
		}
		else if (!stricmp(command.msg->GetName(), "Delete"))
		{
			system()->DeleteRegistryKey(command.msg->GetString("key"));
		}
		bRunNextLineImmediately = true;
	}

	if (bRunNextLineImmediately)
	{
		m_flNextScriptCommandExecTime = system()->GetFrameTime();
	}
	else
	{
		m_flNextScriptCommandExecTime = (float)system()->GetFrameTime() + m_flTimeBetweenRunningEachScriptLine;
	}
}

//-----------------------------------------------------------------------------
// Purpose: finds a panel by name, searching up from the focused panel
//-----------------------------------------------------------------------------
VPANEL TestScriptController::FindMessageTarget(const char *targetName)
{
	// start with the focused panel, and work our way up
	VPANEL searchStart = input()->GetFocus();
	if (!searchStart)
	{
		searchStart = surface()->GetEmbeddedPanel();
	}

	VPANEL target = NULL;
	while (searchStart)
	{
		target = FindMessageTargetInSubPanel(searchStart, targetName);
		if (target)
			break;

		searchStart = ipanel()->GetParent(searchStart);
	}

	if (!target && !stricmp("TestScriptController", targetName))
	{
		target = GetVPanel();
	}

	return target;
}

//-----------------------------------------------------------------------------
// Purpose: recursively searches a panel and its children for a target
//-----------------------------------------------------------------------------
VPANEL TestScriptController::FindMessageTargetInSubPanel(VPANEL parent, const char *targetName)
{
	if (!parent)
		return NULL;

	const char *subName = NULL;
	char searchName[256];
	strcpy(searchName, targetName);
	char *period = strchr(searchName, '.');
	if (period)
	{
		*period = 0;
		subName = strchr(targetName, '.') + 1;
	}

	const char *name = ipanel()->GetName(parent);
	if (!stricmp(name, searchName))
	{
		if (subName)
		{
			return FindMessageTargetInSubPanel(parent, subName);
		}
		return parent;
	}

	// check the children
	for (int i = 0; i < ipanel()->GetChildCount(parent); i++)
	{
		VPANEL child = ipanel()->GetChild(parent, i);
		const char *name = ipanel()->GetName(child);
		if (!stricmp(name, searchName))
		{
			if (subName)
			{
				return FindMessageTargetInSubPanel(child, subName);
			}
			return child;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: prints out debug info on a failed panel search
//-----------------------------------------------------------------------------
void TestScriptController::OnPanelNotFoundError(const char *targetName)
{
	char contextPanel[512];
	CalculateFullNameForPanel(input()->GetFocus(), contextPanel, sizeof(contextPanel));
	ivgui()->DPrintf("TestScriptController: couldn't find target '%s' in context '%s'\n", targetName, contextPanel);
	ivgui()->DPrintf("TestScriptController: couldn't find target '%s'\n", targetName);
	ivgui()->DPrintf("Panels with matching names: \n");
	if (PrintMatchingPanels(surface()->GetEmbeddedPanel(), targetName) < 1)
	{
		ivgui()->DPrintf("\t< no matching names >\n");
	}
}

//-----------------------------------------------------------------------------
// Purpose: prints the full names of all the panels matching the target name
//-----------------------------------------------------------------------------
int TestScriptController::PrintMatchingPanels(VPANEL startPanel, const char *targetName)
{
	int count = 0;
	for (int i = 0; i < ipanel()->GetChildCount(startPanel); i++)
	{
		VPANEL child = ipanel()->GetChild(startPanel, i);
		count += PrintMatchingPanels(child, targetName);
		VPANEL target = FindMessageTargetInSubPanel(child, targetName);
		if (target)
		{
			char name[256];
			CalculateFullNameForPanel(target, name, sizeof(name));
			ivgui()->DPrintf("\t%s\n", name);
			count++;
		}
	}
	return count;
}

//-----------------------------------------------------------------------------
// Purpose: builds the dotted name of a panel from the names of its parents
//-----------------------------------------------------------------------------
void TestScriptController::CalculateFullNameForPanel(VPANEL panel, char *name, int nameBufSize)
{
	if (!panel)
	{
		strcpy(name, "< NULL >");
	}
	else
	{
		name[0] = 0;

		// get the parent name
		VPANEL parent = ipanel()->GetParent(panel);
		if (parent)
		{
			char buf[256];
			CalculateFullNameForPanel(parent, buf, sizeof(buf));
			strcpy(name, buf);
			strcat(name, ".");
		}

		const char *pname = ipanel()->GetName(panel);
		if (!pname || !pname[0])
		{
			pname = "< unnamed >";
		}
		strcat(name, pname);
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads and parses a script file
//-----------------------------------------------------------------------------
bool TestScriptController::LoadScriptFile(const char *scriptFile)
{
	char path[512];
	_snprintf(path, sizeof(path), "testscripts/%s", scriptFile);
	FileHandle_t f = filesystem()->Open(path, "rt");
	if (!f)
	{
		f = filesystem()->Open(scriptFile, "rt");
	}
	if (!f)
	{
		ivgui()->DPrintf("Couldn't find script file %s\n", path);
		m_CurrentScriptCommandIndex.RemoveAll();
		m_flNextScriptCommandExecTime = (float)system()->GetFrameTime() + m_flTimeBetweenRunningEachScriptLine;
		return false;
	}

	int size = filesystem()->Size(f);
	char *pMem = (char *)malloc(size);
	int bytesRead = filesystem()->Read(pMem, size, f);
	pMem[bytesRead] = 0;
	filesystem()->Close(f);

	bool success = ParseScriptFile(pMem, bytesRead);
	free(pMem);
	return success;
}

//-----------------------------------------------------------------------------
// Purpose: runs a script file
//-----------------------------------------------------------------------------
bool TestScriptController::ExecuteScript(const char *scriptFile)
{
	CreateNewUniqueTestString();
	ClearScriptCommandBuffer();

	// add the main function
	m_ScriptFunctions.AddToTail();
	ScriptFunction_t &mainFunction = m_ScriptFunctions[0];
	mainFunction.name = m_TargetSymbols.AddString("Main");

	if (!LoadScriptFile(scriptFile))
	{
		if (mainFunction.scriptCommands.Count() > 1)
		{
			ScriptCommand_t &command = mainFunction.scriptCommands[mainFunction.scriptCommands.Count() - 2];
			ivgui()->DPrintf("Failure parsing script file %s after line:\n\t%s\n", scriptFile, command.originalLine);
		}
		else
		{
			ivgui()->DPrintf("Failure parsing script file %s on first line.", scriptFile);
		}
	}

	// start at the beginning of the main function
	m_CurrentScriptCommandIndex.AddToTail();
	m_CurrentScriptCommandIndex[0].currentScriptFunctionIndex = 0;
	m_CurrentScriptCommandIndex[0].currentScriptCommandIndex = -1;
	m_flNextScriptCommandExecTime = (float)system()->GetFrameTime() + m_flTimeBetweenRunningEachScriptLine;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: frees all the loaded script commands
//-----------------------------------------------------------------------------
void TestScriptController::ClearScriptCommandBuffer()
{
	m_flNextScriptCommandExecTime = 0;
	for (int i = 0; i < m_ScriptFunctions.Count(); i++)
	{
		ScriptFunction_t &function = m_ScriptFunctions[i];
		for (int j = 0; j < function.scriptCommands.Count(); j++)
		{
			if (function.scriptCommands[j].msg)
			{
				function.scriptCommands[j].msg->deleteThis();
			}
		}
		function.scriptCommands.RemoveAll();
	}
	m_ScriptFunctions.RemoveAll();
	m_CurrentScriptCommandIndex.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: replaces %unique% in a value with the unique test string
//-----------------------------------------------------------------------------
void TestScriptController::MakeValueString(const char *value, char *output, int outputBufferSize)
{
	const char *sz = value;
	while (*sz)
	{
		if (!strnicmp(sz, "%unique%", strlen("%unique%")))
		{
			strcpy(output, m_szUniqueTestString);
			output += strlen(m_szUniqueTestString);
			sz += strlen("%unique%");
			continue;
		}
		else
		{
			*output = *sz;
			output++;
			sz++;
		}
	}
	*output = 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the rest of the line holds no more tokens
//-----------------------------------------------------------------------------
bool IsNextTokenNewline(const char *pMem)
{
	while (*pMem)
	{
		if (*pMem == '\n')
			return true;

		if (*pMem == '/' && pMem[1] == '/')
			return true;

		if (isalnum(*pMem))
			return false;

		pMem++;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: parses a script file into the main function
//-----------------------------------------------------------------------------
bool TestScriptController::ParseScriptFile(const char *pMem, int length)
{
	ScriptFunction_t &mainFunction = m_ScriptFunctions[0];
	m_bParsingErrorOccured = false;
	ParseFunction(mainFunction, pMem);
	return !m_bParsingErrorOccured;
}

//-----------------------------------------------------------------------------
// Purpose: parses the commands of a function up to its closing bracket
//-----------------------------------------------------------------------------
const char *TestScriptController::ParseFunction(ScriptFunction_t &function, const char *pMem)
{
	characterset_t breakSet;
	CharacterSetBuild(&breakSet, "{}()'=");

	char token[512];
	pMem = ParseFile(pMem, token, NULL, &breakSet);
	while (token[0] && token[0] != '}')
	{
		if (!stricmp(token, "#include"))
		{
			// load the included file
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!LoadScriptFile(token))
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			continue;
		}

		if (!stricmp(token, "function"))
		{
			// parse the function name
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			int i = m_ScriptFunctions.AddToTail();
			ScriptFunction_t &newFunction = m_ScriptFunctions[i];
			newFunction.name = m_TargetSymbols.AddString(token);

			// skip the open bracket and parse the body
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			pMem = ParseFunction(newFunction, pMem);
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			continue;
		}

		// add a new command
		int i = function.scriptCommands.AddToTail();
		ScriptCommand_t &command = function.scriptCommands[i];
		const char *lineStart = pMem - strlen(token);

		if (!stricmp(token, "exec"))
		{
			command.cmd = SCRIPTCOMMAND_EXEC;
			command.msg = NULL;

			// get the function name
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem)
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			command.target = m_TargetSymbols.AddString(token);
		}
		else if (!stricmp(token, "wait"))
		{
			command.cmd = SCRIPTCOMMAND_WAITUNTILVISIBLE;
			command.msg = NULL;

			// wait until <panel> is <visible|enabled>
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem || stricmp(token, "until"))
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem)
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			command.target = m_TargetSymbols.AddString(token);
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem || stricmp(token, "is"))
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem)
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			if (!stricmp(token, "visible"))
			{
				command.cmd = SCRIPTCOMMAND_WAITUNTILVISIBLE;
			}
			else if (!stricmp(token, "enabled"))
			{
				command.cmd = SCRIPTCOMMAND_WAITUNTILENABLED;
			}
			else
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
		}
		else
		{
			if (!stricmp(token, "Registry"))
			{
				command.cmd = SCRIPTCOMMAND_REGISTRYSET;
			}
			else
			{
				command.cmd = SCRIPTCOMMAND_MESSAGE;
			}

			// the target name, then the message name
			command.target = m_TargetSymbols.AddString(token);
			pMem = ParseFile(pMem, token, NULL, &breakSet);
			if (!pMem)
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
			command.msg = new KeyValues(token);

			// parse the key/value pairs to the end of the line
			while (!IsNextTokenNewline(pMem))
			{
				pMem = ParseFile(pMem, token, NULL, &breakSet);
				if (!pMem)
				{
					m_bParsingErrorOccured = true;
					return NULL;
				}
				char key[512];
				strncpy(key, token, sizeof(key) - 1);
				key[sizeof(key) - 1] = 0;

				pMem = ParseFile(pMem, token, NULL, &breakSet);
				if (!pMem || token[0] != '=')
				{
					m_bParsingErrorOccured = true;
					return NULL;
				}

				pMem = ParseFile(pMem, token, NULL, &breakSet);
				if (!pMem)
				{
					m_bParsingErrorOccured = true;
					return NULL;
				}

				char value[512];
				MakeValueString(token, value, sizeof(value));
				command.msg->SetString(key, value);
			}
		}

		// keep the original text of the line for error messages
		const char *lineEnd = pMem;
		int lineLen = lineEnd - lineStart;
		command.originalLine = new char[lineLen + 1];
		strncpy(command.originalLine, lineStart, lineLen);
		command.originalLine[lineLen] = 0;

		pMem = ParseFile(pMem, token, NULL, &breakSet);
	}

	return pMem;
}

//-----------------------------------------------------------------------------
// Purpose: stops the script when an error log has been written
//-----------------------------------------------------------------------------
void TestScriptController::AbortOnAnyErrorLogsPresent()
{
	FileFindHandle_t pHandle;
	const char *file = filesystem()->FindFirst("errorlogs/*", &pHandle);
	if (file)
	{
		while (file && *file)
		{
			if (file[0] != '.')
			{
				// there is an error log, stop everything
				ClearScriptCommandBuffer();
				PostMessage(surface()->GetEmbeddedPanel(), new KeyValues("Quit"));
				system()->ShellExecute("open", "errorlogs\\errorlog00001.txt");
			}
			file = filesystem()->FindNext(pHandle);
		}
		filesystem()->FindClose(pHandle);
	}
}

//-----------------------------------------------------------------------------
// Purpose: builds the string %unique% is replaced with
//-----------------------------------------------------------------------------
void TestScriptController::CreateNewUniqueTestString()
{
	const char *hostname = getenv("COMPUTERNAME");
	int regcount = 0;
	system()->GetRegistryInteger("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\TestSuite\\TestRunId", regcount);
	_snprintf(m_szUniqueTestString, sizeof(m_szUniqueTestString), "%s%d", hostname, regcount);
	strlwr(m_szUniqueTestString);
	system()->SetRegistryInteger("HKEY_CURRENT_USER\\Software\\Valve\\Steam\\TestSuite\\TestRunId", ++regcount);
}

//-----------------------------------------------------------------------------
// Purpose: delays the next script command
//-----------------------------------------------------------------------------
void TestScriptController::OnDelay(float time)
{
	if (m_flNextScriptCommandExecTime != 0.0f)
	{
		m_flNextScriptCommandExecTime = (float)system()->GetFrameTime() + time;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets the time between running each script line
//-----------------------------------------------------------------------------
void TestScriptController::OnSetScriptRunRate(float time)
{
	m_flTimeBetweenRunningEachScriptLine = time;
	if (m_flTimeBetweenRunningEachScriptLine < 0.01f)
	{
		m_flTimeBetweenRunningEachScriptLine = 0.1f;
	}
}
