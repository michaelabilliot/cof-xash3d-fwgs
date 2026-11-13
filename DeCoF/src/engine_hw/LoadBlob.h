// LoadBlob.h -- loads encrypted ("blob") modules, or plain DLLs, into the engine

#ifndef LOADBLOB_H
#define LOADBLOB_H
#ifdef _WIN32
#pragma once
#endif

typedef struct BlobFootprint_s
{
	int			m_hDll;
} BlobFootprint_t;

int		FIsBlob( const char *pstFileName );
void	*NLoadBlobFile( const char *pstFileName, BlobFootprint_t *pblobfootprint, void *pv, int fLoadAsDll );
void	*LoadBlobFile_guess( char *pBuffer, BlobFootprint_t *pblobfootprint, void *pv, int nSize );
void	FreeBlob( BlobFootprint_t *pblobfootprint );

#endif // LOADBLOB_H
