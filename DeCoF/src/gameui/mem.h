//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: GameUI has no engine heap (engine mem.c): the code shared with the
//			engine (common/ObjectList.cpp) allocates with the HLTV common code's
//			Mem_ZeroMalloc (HLTV/common/common.cpp, C++ linkage) and frees with
//			the CRT.
//
// $NoKeywords: $
//=============================================================================

#ifndef MEM_H
#define MEM_H

#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h>

extern "C++" void *Mem_ZeroMalloc( unsigned int size );

#define Mem_Free free

#endif // MEM_H
