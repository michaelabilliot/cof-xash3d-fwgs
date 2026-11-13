//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: C interface to the connectionless packet rate limit
//
// $NoKeywords: $
//=============================================================================

extern "C"
{
#include "quakedef.h"
#include "net.h"

int CheckIP( netadr_t adr );
}

#include "ipratelimit.h"

static CIPRateLimit rateChecker;

int CheckIP( netadr_t adr )
{
	return rateChecker.CheckIP( adr );
}
