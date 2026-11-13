//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef GUESS_STEAMAPPSTARTUP_H
#define GUESS_STEAMAPPSTARTUP_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Call this first thing at startup
//			Works out if the app is a steam app that is being ran outside of steam,
//			and if so, launches steam and tells it to run us as a steam app
//
//			if it returns true, then exit
//			if it returns false, then continue with normal startup
//-----------------------------------------------------------------------------
bool ShouldLaunchAppViaSteam_guess( const char *cmdLine, const char *steamFilesystemDllName, const char *stdioFilesystemDllName );

#endif // GUESS_STEAMAPPSTARTUP_H
