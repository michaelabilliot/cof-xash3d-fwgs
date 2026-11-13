// quakedef.h -- primary header for the engine (hw.dll)
//
// Cry of Fear ships its own build of the GoldSrc engine (2013-04-21, Visual
// C++ 6.0). Valve never released this source; the declarations follow the
// Linux build's debug information (tools/dwarf_headers.py) and are checked
// against the Windows binary file by file.

#ifndef QUAKEDEF_H
#define QUAKEDEF_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char	byte;
typedef int				qboolean;
typedef int				int32;
typedef void *			FileHandle_t;

#include "common.h"
#include "filesystem_.h"
#include "console.h"
#include "sys.h"

#ifndef __cplusplus
#define false	0
#define true	1
#endif

#define MAX_EDICTS	2048	// max number of edicts in a level (COM_EntsForPlayerSlots)

extern	qboolean	host_initialized;	// true if into command execution

extern	qboolean	g_fUseDInput;		// keys come from DirectInput instead of window messages

#define ARRAYSIZE( p )	( sizeof( p ) / sizeof( p[0] ) )

// from common/const.h, which the engine does not include here (its qboolean
// is an enum and would clash with the int above)
typedef struct
{
	byte	r, g, b;
} color24;

#define	MAX_QPATH	64		// max length of a quake game pathname

typedef unsigned int	uint32;

// precache limits of this engine build (sv.model_precache, sv.sound_precache,
// sv.generic_precache are 768 entries in the Windows binary)
#define	MAX_MODELS			768
#define	MAX_SOUNDS			768
#define	MAX_SOUNDS_HASHLOOKUP_SIZE	( MAX_SOUNDS * 2 - 1 )
#define	MAX_GENERIC			768
#define	MAX_LIGHTSTYLES		64

// buildnum.c
int		build_number( void );

typedef unsigned __int64	uint64;

#endif // QUAKEDEF_H
