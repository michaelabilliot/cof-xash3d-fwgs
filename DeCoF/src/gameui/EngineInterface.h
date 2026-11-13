//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Includes all the headers/declarations necessary to access the
//			engine interface
//
// $NoKeywords: $
//=============================================================================

#ifndef ENGINEINTERFACE_H
#define ENGINEINTERFACE_H

#ifdef _WIN32
#pragma once
#endif

// these stupid set of includes are required to use the cdll_int interface
typedef unsigned char byte;
typedef int (*pfnUserMsgHook)(const char *pszName, int iSize, void *pbuf);
#include "../cl_dll/wrect.h"
#include "../cl_dll/util_vector.h"
#include "cdll_int.h"

// engine interface
extern cl_enginefunc_t gEngfuncs;
extern cl_enginefunc_t *engine;
extern class IGameUIFuncs *gameuifuncs;

#endif // ENGINEINTERFACE_H
