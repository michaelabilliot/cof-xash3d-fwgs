//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef LOADBLOB_H
#define LOADBLOB_H
#ifdef _WIN32
#pragma once
#endif

typedef struct BlobFootprint_s
{
	int			m_hDll;
} BlobFootprint_t;

char FIsBlob( const char *pstFileName );
void *NLoadBlobFile( const char *pstFileName, BlobFootprint_t *pblobfootprint, void *pv, char fLoadAsDll );
void FreeBlob( BlobFootprint_t *pblobfootprint );

#endif // LOADBLOB_H
