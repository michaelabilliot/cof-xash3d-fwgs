//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Byte order conversion (the Windows build is little endian)
//
// $NoKeywords: $
//=============================================================================

#include "byteorder.h"

typedef unsigned char byte;

short BigShort( short l )
{
	byte b1, b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short LittleShort( short l )
{
	return l;
}

int32 BigLong( int32 l )
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int32)b1 << 24 ) + ( (int32)b2 << 16 ) + ( (int32)b3 << 8 ) + b4;
}

int32 LittleLong( int32 l )
{
	return l;
}

float BigFloat( float l )
{
	union { byte b[4]; float f; } in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float LittleFloat( float l )
{
	return l;
}

short ShortSwap( short l )
{
	byte b1, b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short ShortNoSwap( short l )
{
	return l;
}

int32 LongSwap( int32 l )
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int32)b1 << 24 ) + ( (int32)b2 << 16 ) + ( (int32)b3 << 8 ) + b4;
}

int32 LongNoSwap( int32 l )
{
	return l;
}

float FloatSwap( float f )
{
	union
	{
		float f;
		byte b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

float FloatNoSwap( float f )
{
	return f;
}
