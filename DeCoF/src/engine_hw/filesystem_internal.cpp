// filesystem_internal.cpp -- the engine's C interface to the filesystem module (IFileSystem)

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>
#include <windows.h>

#include "interface.h"
#include "FileSystem.h"

IFileSystem *g_pFileSystem;

extern "C"
{

void FS_RemoveAllSearchPaths( void )
{
	g_pFileSystem->RemoveAllSearchPaths();
}

void FS_AddSearchPath( const char *pPath, const char *pathID )
{
	g_pFileSystem->AddSearchPath( pPath, pathID );
}

int FS_RemoveSearchPath( const char *pPath )
{
	return g_pFileSystem->RemoveSearchPath( pPath );
}

void FS_RemoveFile( const char *pRelativePath, const char *pathID )
{
	g_pFileSystem->RemoveFile( pRelativePath, pathID );
}

void FS_CreateDirHierarchy( const char *path, const char *pathID )
{
	g_pFileSystem->CreateDirHierarchy( path, pathID );
}

int FS_FileExists( const char *pFileName )
{
	return g_pFileSystem->FileExists( pFileName );
}

int FS_IsDirectory( const char *pFileName )
{
	return g_pFileSystem->IsDirectory( pFileName );
}

FileHandle_t FS_Open( const char *pFileName, const char *pOptions )
{
	return g_pFileSystem->Open( pFileName, pOptions );
}

FileHandle_t FS_OpenPathID( const char *pFileName, const char *pOptions, const char *pathID )
{
	return g_pFileSystem->Open( pFileName, pOptions, pathID );
}

void FS_Close( FileHandle_t file )
{
	g_pFileSystem->Close( file );
}

void FS_Seek( FileHandle_t file, int pos, FileSystemSeek_t seekType )
{
	g_pFileSystem->Seek( file, pos, seekType );
}

unsigned int FS_Tell( FileHandle_t file )
{
	return g_pFileSystem->Tell( file );
}

unsigned int FS_Size( FileHandle_t file )
{
	return g_pFileSystem->Size( file );
}

unsigned int FS_FileSize( const char *pFileName )
{
	return g_pFileSystem->Size( pFileName );
}

long FS_GetFileTime( const char *pFileName )
{
	return g_pFileSystem->GetFileTime( pFileName );
}

void FS_FileTimeToString( char *pStrip, int maxCharsIncludingTerminator, long fileTime )
{
	g_pFileSystem->FileTimeToString( pStrip, maxCharsIncludingTerminator, fileTime );
}

int FS_IsOk( FileHandle_t file )
{
	return g_pFileSystem->IsOk( file );
}

void FS_Flush( FileHandle_t file )
{
	g_pFileSystem->Flush( file );
}

int FS_EndOfFile( FileHandle_t file )
{
	return g_pFileSystem->EndOfFile( file );
}

int FS_Read( void *pOutput, int size, int count, FileHandle_t file )
{
	return g_pFileSystem->Read( pOutput, size, file );
}

int FS_Write( const void *pInput, int size, int count, FileHandle_t file )
{
	return g_pFileSystem->Write( pInput, size, file );
}

char *FS_ReadLine( char *pOutput, int maxChars, FileHandle_t file )
{
	return g_pFileSystem->ReadLine( pOutput, maxChars, file );
}

int FS_FPrintf( FileHandle_t file, char *pFormat, ... )
{
	char data[8192];
	va_list argptr;

	va_start( argptr, pFormat );
	vsprintf( data, pFormat, argptr );
	va_end( argptr );

	return g_pFileSystem->FPrintf( file, "%s", data );
}

const char *FS_FindFirst( const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID )
{
	return g_pFileSystem->FindFirst( pWildCard, pHandle, pathID );
}

const char *FS_FindNext( FileFindHandle_t handle )
{
	return g_pFileSystem->FindNext( handle );
}

int FS_FindIsDirectory( FileFindHandle_t handle )
{
	return g_pFileSystem->FindIsDirectory( handle );
}

void FS_FindClose( FileFindHandle_t handle )
{
	g_pFileSystem->FindClose( handle );
}

void FS_GetLocalCopy( const char *pFileName )
{
	g_pFileSystem->GetLocalCopy( pFileName );
}

const char *FS_GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize )
{
	return g_pFileSystem->GetLocalPath( pFileName, pLocalPath, localPathBufferSize );
}

char *FS_ParseFile( char *pFileBytes, char *pToken, int *pWasQuoted )
{
	bool wasquoted;
	char *retval;

	retval = g_pFileSystem->ParseFile( pFileBytes, pToken, &wasquoted );

	if ( pWasQuoted )
		*pWasQuoted = wasquoted ? 1 : 0;

	return retval;
}

int FS_FullPathToRelativePath( const char *pFullpath, char *pRelative )
{
	return g_pFileSystem->FullPathToRelativePath( pFullpath, pRelative );
}

int FS_GetCurrentDirectory( char *pDirectory, int maxlen )
{
	return g_pFileSystem->GetCurrentDirectory( pDirectory, maxlen );
}

void FS_PrintOpenedFiles( void )
{
	g_pFileSystem->PrintOpenedFiles();
}

void FS_SetWarningFunc( void (*pfnWarning)( const char *fmt, ... ) )
{
	g_pFileSystem->SetWarningFunc( pfnWarning );
}

void FS_SetWarningLevel( FileWarningLevel_t level )
{
	g_pFileSystem->SetWarningLevel( level );
}

unsigned char FS_GetCharacter( FileHandle_t f )
{
	unsigned char retval;

	FS_Read( &retval, 1, 1, f );

	return retval;
}

void FS_LogLevelLoadStarted( const char *name )
{
	g_pFileSystem->LogLevelLoadStarted( name );
}

void FS_LogLevelLoadFinished( const char *name )
{
	g_pFileSystem->LogLevelLoadFinished( name );
}

int FS_HintResourceNeed( const char *hintlist, int forgetEverything )
{
	return g_pFileSystem->HintResourceNeed( hintlist, forgetEverything );
}

// wrappers around ResumeResourcePreloading and PauseResourcePreloading (names unproven)
int FS_ResumeResourcePreloading_guess( void )
{
	return g_pFileSystem->ResumeResourcePreloading();
}

int FS_PauseResourcePreloading_guess( void )
{
	return g_pFileSystem->PauseResourcePreloading();
}

int FS_SetVBuf( FileHandle_t stream, char *buffer, int mode, long size )
{
	return g_pFileSystem->SetVBuf( stream, buffer, mode, size );
}

void FS_GetInterfaceVersion( char *p, int maxlen )
{
	g_pFileSystem->GetInterfaceVersion( p, maxlen );
}

void *FS_GetReadBuffer( FileHandle_t file, int *outBufSize )
{
	return g_pFileSystem->GetReadBuffer( file, outBufSize, true );
}

void FS_ReleaseReadBuffer( FileHandle_t file, void *buffer )
{
	g_pFileSystem->ReleaseReadBuffer( file, buffer );
}

void FS_Unlink( const char *filename )
{
	char localPath[512];
	int res;

	FS_GetLocalPath( filename, localPath, sizeof( localPath ) );
	res = _unlink( localPath );
}

void FS_Rename( const char *originalName, const char *newName )
{
	char localPath[512], newPath[512], *cut;
	int res;

	FS_GetLocalPath( originalName, localPath, sizeof( localPath ) );
	strcpy( newPath, localPath );

	cut = strstr( newPath, originalName );
	if ( !cut )
		return;

	*cut = 0;
	strcat( newPath, newName );

	res = rename( localPath, newPath );
}

// wrappers around WaitForResources, GetWaitForResourcesProgress and
// CancelWaitForResources (names unproven)
WaitForResourcesHandle_t FS_WaitForResources_guess( const char *resourcelist )
{
	return g_pFileSystem->WaitForResources( resourcelist );
}

int FS_GetWaitForResourcesProgress_guess( WaitForResourcesHandle_t handle, float *progress, int *complete )
{
	bool bComplete_guess = false;
	int retval_guess = g_pFileSystem->GetWaitForResourcesProgress( handle, progress, &bComplete_guess );

	if ( complete )
		*complete = bComplete_guess;

	return retval_guess;
}

void FS_CancelWaitForResources_guess( WaitForResourcesHandle_t handle )
{
	g_pFileSystem->CancelWaitForResources( handle );
}

void *FS_LoadLibrary( const char *dllName )
{
	if ( !dllName )
		return NULL;

	FS_GetLocalCopy( dllName );

	return LoadLibrary( dllName );
}

}
