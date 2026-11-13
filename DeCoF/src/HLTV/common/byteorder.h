//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Byte order conversion
//
// $NoKeywords: $
//=============================================================================

#ifndef BYTEORDER_H
#define BYTEORDER_H

#ifdef _WIN32
#pragma once
#endif

#include "steam/steamtypes.h"

short	BigShort( short l );
short	LittleShort( short l );
int32	BigLong( int32 l );
int32	LittleLong( int32 l );
float	BigFloat( float l );
float	LittleFloat( float l );

#endif // BYTEORDER_H
