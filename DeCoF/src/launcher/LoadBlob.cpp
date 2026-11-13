//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Loads encrypted ("blob") modules into the address space reserved
//			for them in the launcher image.
//
// $NoKeywords: $
//=============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "LoadBlob.h"
#include "FileSystem.h"

extern IFileSystem *g_pFileSystem;
extern char com_gamedir[];

// Plain text header at the start of a blob file
typedef struct BlobInfo_s_guess
{
	char		m_szPath[ 10 ];
	char		m_szDescribe[ 32 ];
	char		m_szCompany[ 22 ];
	DWORD		m_dwAlgorithm;
} BlobInfo_t_guess;

// Encrypted header that follows BlobInfo_t_guess
typedef struct BlobHeader_s_guess
{
	DWORD		m_dwCheckSum;
	int			m_nSectionCount;
	DWORD		m_dwExportPoint;
	DWORD		m_dwImageBase;
	DWORD		m_dwEntryPoint;
	DWORD		m_dwImportTable;
} BlobHeader_t_guess;

// Section table that follows BlobHeader_t_guess
typedef struct BlobSection_s_guess
{
	DWORD		m_dwVirtualAddress;
	int			m_nVirtualSize;
	int			m_nDataSize;
	DWORD		m_dwDataAddress;
	BOOL		m_bIsSpecial;
} BlobSection_t_guess;

#define BLOB_ALGORITHM			0x12345678
#define BLOB_XOR_EXPORTPOINT	0x7A32BC85
#define BLOB_XOR_IMAGEBASE		0x49C042D1
#define BLOB_XOR_IMPORTTABLE	0x872C3D47

typedef BOOL ( WINAPI *pfnDllMain_t_guess )( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved );
typedef void ( *pfnExportPoint_t_guess )( void *pv );

void *LoadBlobFile_guess( BYTE *pBuffer, BlobFootprint_t *pblobfootprint, void *pv, int nSize );
void ProcessBlobImports_guess( BlobHeader_t_guess *pHeader );

//-----------------------------------------------------------------------------
// Purpose: Is this file a blob?
// Input  : *pstFileName -
// Output : char
//-----------------------------------------------------------------------------
char FIsBlob( const char *pstFileName )
{
	BlobInfo_t_guess info;
	FileHandle_t hFile;

	hFile = g_pFileSystem->Open( pstFileName, "rb" );
	g_pFileSystem->Read( &info, sizeof( info ), hFile );
	g_pFileSystem->Close( hFile );

	return info.m_dwAlgorithm == BLOB_ALGORITHM;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a blob file and loads it
// Input  : *pstFileName -
//			*pblobfootprint -
//			*pv - passed to the export point of the blob
//			fLoadAsDll -
// Output : void
//-----------------------------------------------------------------------------
void *NLoadBlobFile( const char *pstFileName, BlobFootprint_t *pblobfootprint, void *pv, char fLoadAsDll )
{
	FileHandle_t hFile;
	int nSize;
	BYTE *pBuffer;
	void *nRet;

	hFile = g_pFileSystem->OpenFromCacheForRead( pstFileName, "rb" );
	if ( !hFile && strcmp( com_gamedir, "cstrike" ) )
		hFile = g_pFileSystem->Open( pstFileName, "rb" );

	g_pFileSystem->Seek( hFile, 0, FILESYSTEM_SEEK_TAIL );
	nSize = g_pFileSystem->Tell( hFile );
	g_pFileSystem->Seek( hFile, 0, FILESYSTEM_SEEK_HEAD );

	pBuffer = (BYTE *)malloc( nSize );
	g_pFileSystem->Read( pBuffer, nSize, hFile );

	nRet = LoadBlobFile_guess( pBuffer, pblobfootprint, pv, nSize );

	free( pBuffer );
	g_pFileSystem->Close( hFile );

	return nRet;
}

//-----------------------------------------------------------------------------
// Purpose: Decrypts a blob in memory, copies its sections into place and
//			starts it
// Input  : *pBuffer - the whole file
//			*pblobfootprint -
//			*pv - passed to the export point of the blob
//			nSize - size of the file
// Output : void * - address of the special section
//-----------------------------------------------------------------------------
void *LoadBlobFile_guess( BYTE *pBuffer, BlobFootprint_t *pblobfootprint, void *pv, int nSize )
{
	void *pSpecial = NULL;
	BYTE bXor = 'W';
	int i;
	BlobHeader_t_guess *pHeader = (BlobHeader_t_guess *)( pBuffer + sizeof( BlobInfo_t_guess ) );
	BlobSection_t_guess *pSection = (BlobSection_t_guess *)( pBuffer + sizeof( BlobInfo_t_guess ) + sizeof( BlobHeader_t_guess ) );

	for ( i = sizeof( BlobInfo_t_guess ); i < nSize; i++ )
	{
		pBuffer[ i ] ^= bXor;
		bXor += pBuffer[ i ] + 'W';
	}

	for ( i = 0; i <= pHeader->m_nSectionCount; i++ )
	{
		if ( pSection[ i ].m_bIsSpecial )
			pSpecial = (void *)pSection[ i ].m_dwVirtualAddress;

		if ( pSection[ i ].m_nVirtualSize > pSection[ i ].m_nDataSize )
			memset( (BYTE *)pSection[ i ].m_dwVirtualAddress + pSection[ i ].m_nDataSize, 0, pSection[ i ].m_nVirtualSize - pSection[ i ].m_nDataSize );

		memcpy( (void *)pSection[ i ].m_dwVirtualAddress, pBuffer + pSection[ i ].m_dwDataAddress, pSection[ i ].m_nDataSize );
	}

	ProcessBlobImports_guess( pHeader );

	( (pfnDllMain_t_guess)( pHeader->m_dwEntryPoint - 12 ) )( 0, DLL_PROCESS_ATTACH, 0 );
	( (pfnExportPoint_t_guess)( pHeader->m_dwExportPoint ^ BLOB_XOR_EXPORTPOINT ) )( pv );

	return pSpecial;
}

//-----------------------------------------------------------------------------
// Purpose: Resolves the import table of a loaded blob
// Input  : *pHeader -
//-----------------------------------------------------------------------------
void ProcessBlobImports_guess( BlobHeader_t_guess *pHeader )
{
	IMAGE_IMPORT_DESCRIPTOR *pImport;
	IMAGE_THUNK_DATA *pThunk;
	HMODULE hModule;
	DWORD dwName;

	pImport = (IMAGE_IMPORT_DESCRIPTOR *)( pHeader->m_dwImportTable ^ BLOB_XOR_IMPORTTABLE );

	while ( pImport->Name )
	{
		hModule = LoadLibrary( (char *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + pImport->Name ) );
		pThunk = (IMAGE_THUNK_DATA *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + pImport->FirstThunk );

		while ( pThunk->u1.Function )
		{
			dwName = pThunk->u1.Ordinal & 0x7FFFFFFF;

			if ( pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG )
				pThunk->u1.Function = (PDWORD)GetProcAddress( hModule, (char *)dwName );
			else
				pThunk->u1.Function = (PDWORD)GetProcAddress( hModule, (char *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + dwName + 2 ) );

			pThunk++;
		}

		pImport++;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pblobfootprint -
//-----------------------------------------------------------------------------
void FreeBlob( BlobFootprint_t *pblobfootprint )
{
	FreeLibrary( (HMODULE)pblobfootprint->m_hDll );
}
