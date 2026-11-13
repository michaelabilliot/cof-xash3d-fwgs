// filesystem_.h -- the engine's wrappers around the filesystem interface (filesystem.cpp)

#ifndef FILESYSTEM__H
#define FILESYSTEM__H

#define FILESYSTEM_SEEK_HEAD	0
#define FILESYSTEM_SEEK_CURRENT	1
#define FILESYSTEM_SEEK_TAIL	2

FileHandle_t	FS_Open( const char *pFileName, const char *pOptions );
FileHandle_t	FS_OpenPathID( const char *pFileName, const char *pOptions, const char *pathID );
void			FS_Close( FileHandle_t file );
void			FS_Seek( FileHandle_t file, int pos, int seekType );
unsigned int	FS_Tell( FileHandle_t file );
unsigned int	FS_Size( FileHandle_t file );
int				FS_IsOk( FileHandle_t file );
int				FS_EndOfFile( FileHandle_t file );
int				FS_Read( void *pOutput, int size, int count, FileHandle_t file );
int				FS_Write( const void *pInput, int size, int count, FileHandle_t file );
int				FS_FPrintf( FileHandle_t file, char *pFormat, ... );
char			*FS_ReadLine( char *pOutput, int maxChars, FileHandle_t file );
int				FS_FileExists( const char *pFileName );
int				FS_FileSize( const char *pFileName );
void			FS_Unlink( const char *filename );
void			FS_RemoveFile( const char *pRelativePath, const char *pathID );
void			FS_CreateDirHierarchy( const char *path, const char *pathID );
void			FS_AddSearchPath( const char *pPath, const char *pathID );
int				FS_GetFileTime( const char *pFileName );
void			FS_Flush( FileHandle_t file );
char			*FS_GetLocalPath( const char *pFileName, char *pLocalPath, int localPathBufferSize );
void			*FS_LoadLibrary( const char *dllName );

typedef int FileFindHandle_t;

const char		*FS_FindFirst( const char *pWildCard, FileFindHandle_t *pHandle, const char *pathID );
const char		*FS_FindNext( FileFindHandle_t handle );
void			FS_FindClose( FileFindHandle_t handle );

int				FileSystem_SetGameDirectory( const char *pDefaultDir, const char *pGameDir );
int				FileSystem_AddFallbackGameDir( const char *pGameDir );

void			FS_Rename( const char *originalName, const char *newName );

void			FS_LogLevelLoadStarted( const char *name );
void			FS_LogLevelLoadFinished( const char *name );
int				FS_HintResourceNeed( const char *hintlist, int forgetEverything );
int				FS_SetVBuf( FileHandle_t stream, char *buffer, int mode, long size );
void			*FS_GetReadBuffer( FileHandle_t file, int *outBufSize );
void			FS_ReleaseReadBuffer( FileHandle_t file, void *buffer );

// wrappers around IFileSystem::WaitForResources, GetWaitForResourcesProgress
// and CancelWaitForResources (0x01D5D7FA, 0x01D5D817, 0x01D5D862; names unproven)
int				FS_WaitForResources_guess( const char *resourcelist );
qboolean		FS_GetWaitForResourcesProgress_guess( int handle, float *progress, qboolean *complete );
void			FS_CancelWaitForResources_guess( int handle );

#endif // FILESYSTEM__H
