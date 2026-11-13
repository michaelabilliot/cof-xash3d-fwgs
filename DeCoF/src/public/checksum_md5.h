//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Generic MD5 hashing algo
//
// $NoKeywords: $
//=============================================================================

#ifndef CHECKSUM_MD5_H
#define CHECKSUM_MD5_H

#ifdef _WIN32
#pragma once
#endif

typedef struct
{
	unsigned int	buf[4];
	unsigned int	bits[2];
	unsigned char	in[64];
} MD5Context_t;

void MD5Init( MD5Context_t *ctx );
void MD5Update( MD5Context_t *ctx, const unsigned char *buf, unsigned int len );
void MD5Final( unsigned char digest[16], MD5Context_t *ctx );

char *MD5_Print( unsigned char hash[16] );

#endif // CHECKSUM_MD5_H
