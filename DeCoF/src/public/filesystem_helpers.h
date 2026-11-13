//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef FILESYSTEM_HELPERS_H
#define FILESYSTEM_HELPERS_H

#ifdef _WIN32
#pragma once
#endif

struct characterset_s;

// Don't call this directly. You should (as long as your destination is an array) be
// able to use ParseFile() below.
const char* ParseFile( const char* pFileBytes, char* pToken, bool* pWasQuoted, characterset_s *pCharSet = NULL );

char* ParseFile( char* pFileBytes, char* pToken, bool* pWasQuoted );

#endif // FILESYSTEM_HELPERS_H
