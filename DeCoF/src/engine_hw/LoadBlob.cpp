// LoadBlob.cpp -- loads encrypted ("blob") modules, or plain DLLs, into the engine

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "FileSystem.h"

extern "C"
{
#include "LoadBlob.h"

extern char com_gamedir[];
}

extern IFileSystem *g_pFileSystem;

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

// export "F" of a module loaded as a plain DLL
typedef void ( *FTYPE )( void *pv );

static void ProcessBlobImports_guess( BlobHeader_t_guess *pHeader );

//-----------------------------------------------------------------------------
// Purpose: Is this file a blob?
//-----------------------------------------------------------------------------
int FIsBlob( const char *pstFileName )
{
	BlobInfo_t_guess info_guess;
	FileHandle_t hBlobFile_guess;

	hBlobFile_guess = g_pFileSystem->Open( pstFileName, "rb" );
	g_pFileSystem->Read( &info_guess, sizeof( info_guess ), hBlobFile_guess );
	g_pFileSystem->Close( hBlobFile_guess );

	if ( info_guess.m_dwAlgorithm == BLOB_ALGORITHM )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Loads a module, either as a plain DLL or from a blob file
//-----------------------------------------------------------------------------
void *NLoadBlobFile( const char *pstFileName, BlobFootprint_t *pblobfootprint, void *pv, int fLoadAsDll )
{
	void *nRet = NULL;

	if ( fLoadAsDll )
	{
		char rgchNew[ 2048 ];
		strcpy( rgchNew, pstFileName );

		char rgchLocalPath[ 2048 ];
		g_pFileSystem->GetLocalPath( rgchNew, rgchLocalPath, sizeof( rgchLocalPath ) );

		HMODULE hmoduleT = LoadLibrary( rgchLocalPath );
		pblobfootprint->m_hDll = (int)hmoduleT;

		if ( hmoduleT )
		{
			FTYPE pfF = (FTYPE)GetProcAddress( hmoduleT, "F" );
			pfF( pv );
		}
	}
	else
	{
		FileHandle_t hBlobFile_guess = g_pFileSystem->OpenFromCacheForRead( pstFileName, "rb" );
		if ( !hBlobFile_guess && strcmp( com_gamedir, "cstrike" ) )
			hBlobFile_guess = g_pFileSystem->Open( pstFileName, "rb" );

		g_pFileSystem->Seek( hBlobFile_guess, 0, FILESYSTEM_SEEK_TAIL );
		int nLength_guess = g_pFileSystem->Tell( hBlobFile_guess );
		g_pFileSystem->Seek( hBlobFile_guess, 0, FILESYSTEM_SEEK_HEAD );

		char *pBuffer_guess = (char *)malloc( nLength_guess );
		g_pFileSystem->Read( pBuffer_guess, nLength_guess, hBlobFile_guess );

		nRet = LoadBlobFile_guess( pBuffer_guess, pblobfootprint, pv, nLength_guess );

		free( pBuffer_guess );
		g_pFileSystem->Close( hBlobFile_guess );
	}

	return nRet;
}

//-----------------------------------------------------------------------------
// Purpose: Decrypts a blob in memory, copies its sections into place and
//			starts it
// Output : void * - address of the special section
//-----------------------------------------------------------------------------
void *LoadBlobFile_guess( char *pBuffer, BlobFootprint_t *pblobfootprint, void *pv, int nSize )
{
	void *pRet_guess = NULL;
	BlobHeader_t_guess *pHead_guess = (BlobHeader_t_guess *)( pBuffer + sizeof( BlobInfo_t_guess ) );
	BlobSection_t_guess *pSection_guess = (BlobSection_t_guess *)( pBuffer + sizeof( BlobInfo_t_guess ) + sizeof( BlobHeader_t_guess ) );
	BYTE bXor_guess = 'W';

	for ( int nByte_guess = sizeof( BlobInfo_t_guess ); nByte_guess < nSize; nByte_guess++ )
	{
		pBuffer[ nByte_guess ] = pBuffer[ nByte_guess ] ^ bXor_guess;
		bXor_guess += pBuffer[ nByte_guess ] + 'W';
	}

	for ( int iSect_guess = 0; iSect_guess <= pHead_guess->m_nSectionCount; iSect_guess++ )
	{
		BlobSection_t_guess *pSec_guess = &pSection_guess[ iSect_guess ];

		if ( pSec_guess->m_bIsSpecial )
			pRet_guess = (void *)pSec_guess->m_dwVirtualAddress;

		if ( pSec_guess->m_nVirtualSize > pSec_guess->m_nDataSize )
			memset( (char *)pSec_guess->m_dwVirtualAddress + pSec_guess->m_nDataSize, 0, pSec_guess->m_nVirtualSize - pSec_guess->m_nDataSize );

		memcpy( (void *)pSec_guess->m_dwVirtualAddress, pBuffer + pSec_guess->m_dwDataAddress, pSec_guess->m_nDataSize );
	}

	ProcessBlobImports_guess( pHead_guess );

	( (pfnDllMain_t_guess)( pHead_guess->m_dwEntryPoint - 12 ) )( 0, DLL_PROCESS_ATTACH, 0 );
	( (pfnExportPoint_t_guess)( pHead_guess->m_dwExportPoint ^ BLOB_XOR_EXPORTPOINT ) )( pv );

	return pRet_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Resolves the import table of a loaded blob
//-----------------------------------------------------------------------------
static void ProcessBlobImports_guess( BlobHeader_t_guess *pHeader )
{
	IMAGE_IMPORT_DESCRIPTOR *pImport_guess;
	IMAGE_THUNK_DATA *pThunk_guess;
	HMODULE hInst_guess;
	DWORD dwOrdinal_guess;
	FARPROC pfnProc_guess;

	for ( pImport_guess = (IMAGE_IMPORT_DESCRIPTOR *)( pHeader->m_dwImportTable ^ BLOB_XOR_IMPORTTABLE ); pImport_guess->Name; pImport_guess++ )
	{
		hInst_guess = LoadLibrary( (char *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + pImport_guess->Name ) );

		for ( pThunk_guess = (IMAGE_THUNK_DATA *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + pImport_guess->FirstThunk ); pThunk_guess->u1.Function; pThunk_guess++ )
		{
			dwOrdinal_guess = pThunk_guess->u1.Ordinal & 0x7FFFFFFF;

			if ( pThunk_guess->u1.Ordinal & IMAGE_ORDINAL_FLAG )
			{
				pfnProc_guess = GetProcAddress( hInst_guess, (char *)dwOrdinal_guess );
			}
			else
			{
				IMAGE_IMPORT_BY_NAME *pImportByName_guess = (IMAGE_IMPORT_BY_NAME *)( ( pHeader->m_dwImageBase ^ BLOB_XOR_IMAGEBASE ) + dwOrdinal_guess );
				pfnProc_guess = GetProcAddress( hInst_guess, (char *)pImportByName_guess->Name );
			}

			pThunk_guess->u1.Function = (PDWORD)pfnProc_guess;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Unloads a module loaded as a plain DLL
//-----------------------------------------------------------------------------
void FreeBlob( BlobFootprint_t *pblobfootprint )
{
	FreeLibrary( (HMODULE)pblobfootprint->m_hDll );
}
