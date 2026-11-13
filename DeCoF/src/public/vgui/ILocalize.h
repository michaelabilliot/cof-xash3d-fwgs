//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ILOCALIZE_H
#define ILOCALIZE_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <vgui/VGUI.h>

class IFileSystem;
class KeyValues;

namespace vgui2
{

// direct references to localized strings
typedef unsigned long StringIndex_t;
const unsigned long INVALID_STRING_INDEX = (unsigned long) -1;

//-----------------------------------------------------------------------------
// Purpose: Handles localization of text
//			looks up string names and returns the localized unicode text
//-----------------------------------------------------------------------------
class ILocalize : public IBaseInterface
{
public:
	virtual bool AddFile(IFileSystem *fileSystem, const char *szFileName) = 0;
	virtual void RemoveAll() = 0;
	virtual wchar_t *Find(const char *pName) = 0;
	virtual int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes) = 0;
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize) = 0;
	virtual StringIndex_t FindIndex(const char *pName) = 0;
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, wchar_t *formatString, int numFormatParameters, ...) = 0;
	virtual const char *GetNameByIndex(StringIndex_t index) = 0;
	virtual wchar_t *GetValueByIndex(StringIndex_t index) = 0;
	virtual StringIndex_t GetFirstStringIndex() = 0;
	virtual StringIndex_t GetNextStringIndex(StringIndex_t index) = 0;
	virtual void AddString(const char *pString, wchar_t *pValue, const char *fileName) = 0;
	virtual void SetValueByIndex(StringIndex_t index, wchar_t *newValue) = 0;
	virtual bool SaveToFile(IFileSystem *fileSystem, const char *szFileName) = 0;
	virtual int GetLocalizationFileCount() = 0;
	virtual const char *GetLocalizationFileName(int index) = 0;
	virtual const char *GetFileNameByIndex(StringIndex_t index) = 0;
	virtual void ReloadLocalizationFiles(IFileSystem *filesystem) = 0;
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables) = 0;
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, StringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables) = 0;
};

} // namespace vgui2

#define VGUI_LOCALIZE_INTERFACE_VERSION "VGUI_Localize003"

#endif // ILOCALIZE_H
