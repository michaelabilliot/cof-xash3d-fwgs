//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: reasons a Steam login can fail
//
// $NoKeywords: $
//=============================================================================

#ifndef STEAMLOGINFAILURE_H
#define STEAMLOGINFAILURE_H

#ifdef _WIN32
#pragma once
#endif

enum ESteamLoginFailure
{
	STEAMLOGINFAILURE_NONE,
	STEAMLOGINFAILURE_BADTICKET,
	STEAMLOGINFAILURE_NOSTEAMLOGIN,
	STEAMLOGINFAILURE_VACBANNED,
	STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE,
	STEAMLOGINFAILURE_CONNECTIONLOST,
	STEAMLOGINFAILURE_NOCONNECTION,
};

#endif // STEAMLOGINFAILURE_H
