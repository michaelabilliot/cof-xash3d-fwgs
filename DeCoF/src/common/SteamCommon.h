//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Steam types passed through IEngineVGui (only the parts GameUI uses;
//			sizes checked against the GameUI.dll stack frames: TSteamError 268
//			bytes, TSteamProgress 264 bytes)
//
// $NoKeywords: $
//=============================================================================

#ifndef STEAMCOMMON_H
#define STEAMCOMMON_H

#ifdef _WIN32
#pragma once
#endif

#define STEAM_MAX_PATH		(255)

typedef enum
{
	eSteamErrorNone = 0,
} ESteamError;

typedef enum
{
	eNoDetailedErrorAvailable,
	eStandardCerrno,
	eWin32LastError,
	eWinSockLastError,
	eDetailedPlatformErrorCount
} EDetailedPlatformErrorType;

typedef struct TSteamError
{
	ESteamError eSteamError;
	EDetailedPlatformErrorType eDetailedErrorType;
	int nDetailedErrorCode;
	char szDesc[STEAM_MAX_PATH];
} TSteamError;

typedef struct TSteamProgress
{
	int bValid;
	unsigned int uPercentDone;
	char szProgress[STEAM_MAX_PATH];
} TSteamProgress;

#endif // STEAMCOMMON_H
