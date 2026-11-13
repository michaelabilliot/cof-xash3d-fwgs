// hashpak.c -- the .hpk pack of custom resources (player decals), stored and
// looked up by the MD5 hash of their data

#include "quakedef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "crc.h"
#include "custom.h"
#include "hashpak.h"
#include "sv_log.h"

typedef struct hash_pack_entry_s
{
	resource_t	resource;
	int			nOffset;
	int			nFileLength;
} hash_pack_entry_t;

typedef struct hash_pack_directory_s
{
	int					nEntries;
	hash_pack_entry_t	*p_rgEntries;
} hash_pack_directory_t;

typedef struct hash_pack_header_s
{
	char		szFileStamp[4];
	int			version;
	int			nDirectoryOffset;
} hash_pack_header_t;

typedef struct hash_pack_queue_s
{
	char		*pakname;
	resource_t	resource;
	int			datasize;
	void		*data;
	struct hash_pack_queue_s	*next;
} hash_pack_queue_t;

hash_pack_queue_t		*gp_hpak_queue = NULL;
hash_pack_directory_t	hash_pack_dir = { 0, NULL };
hash_pack_header_t		hash_pack_header;

/*
==================
HPAK_GetDataPointer

Finds the data of a resource in the pack (or in the queue of lumps not
written yet). The caller frees *pbuffer.
==================
*/
qboolean HPAK_GetDataPointer( char *pakname, struct resource_s *pResource, unsigned char **pbuffer, int *bufsize )
{
	qboolean		retval = true;
	FileHandle_t	fp = NULL;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry = NULL;
	char			name[MAX_PATH];
	int				i;
	qboolean		bfound = false;

	retval = false;

	if ( pbuffer )
		*pbuffer = NULL;
	if ( bufsize )
		*bufsize = 0;

	if ( gp_hpak_queue )
	{
		hash_pack_queue_t	*p;

		p = gp_hpak_queue;
		while ( p )
		{
			if ( !Q_stricmp( p->pakname, pakname ) && !Q_memcmp( p->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16 ) )
			{
				if ( pbuffer )
				{
					unsigned char	*pbuf;

					pbuf = (unsigned char *)Mem_Malloc( p->datasize );
					if ( !pbuf )
						Sys_Error( "Error allocating %i bytes for hpak!", p->datasize );

					Q_memcpy( pbuf, p->data, p->datasize );
					*pbuffer = pbuf;
				}

				if ( bufsize )
					*bufsize = p->datasize;

				return true;
			}

			p = p->next;
		}
	}

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	fp = FS_Open( name, "rb" );
	if ( !fp )
		return false;

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return false;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "HPAK_List:  version mismatch\n" );
		FS_Close( fp );
		return false;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return false;
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_ZeroMalloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	bfound = false;
	for ( i = 0; i < directory.nEntries; i++ )
	{
		entry = &directory.p_rgEntries[i];
		if ( Q_memcmp( entry->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16 ) )
			continue;

		FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
		bfound = true;
		break;
	}

	if ( bfound )
	{
		retval = true;

		if ( pbuffer && entry && entry->nFileLength > 0 )
		{
			unsigned char	*p;

			if ( bufsize )
				*bufsize = entry->nFileLength;

			p = (unsigned char *)Mem_Malloc( entry->nFileLength );
			if ( !p )
			{
				Con_Printf( "Couln't allocate %i bytes for HPAK entry\n", entry->nFileLength );
				if ( bufsize )
					*bufsize = 0;
				retval = false;
			}
			else
			{
				FS_Read( p, entry->nFileLength, 1, fp );
				*pbuffer = p;
			}
		}
	}

	Mem_Free( directory.p_rgEntries );
	FS_Close( fp );

	return retval;
}

/*
==================
HPAK_FindResource

==================
*/
qboolean HPAK_FindResource( hash_pack_directory_t *pDir, unsigned char *hash, struct resource_s *pResourceEntry )
{
	int		i;

	for ( i = 0; i < pDir->nEntries; i++ )
	{
		if ( Q_memcmp( hash, pDir->p_rgEntries[i].resource.rgucMD5_hash, 16 ) )
			continue;

		if ( pResourceEntry )
			*pResourceEntry = pDir->p_rgEntries[i].resource;

		return true;
	}

	return false;
}

/*
==================
HPAK_AddToQueue

==================
*/
void HPAK_AddToQueue( char *pakname, struct resource_s *pResource, void *pData, FileHandle_t fpSource )
{
	hash_pack_queue_t	*n;

	n = (hash_pack_queue_t *)Mem_Malloc( sizeof( hash_pack_queue_t ) );
	if ( !n )
		Sys_Error( "Unable to allocate %i bytes for hpak queue!", sizeof( hash_pack_queue_t ) );

	Q_memset( n, 0, sizeof( hash_pack_queue_t ) );

	n->pakname = Mem_Strdup( pakname );
	n->resource = *pResource;
	n->datasize = pResource->nDownloadSize;
	n->data = Mem_Malloc( n->datasize );
	if ( !n->data )
		Sys_Error( "Unable to allocate %i bytes for hpak queue!", n->datasize );

	if ( pData )
		Q_memcpy( n->data, pData, n->datasize );
	else if ( fpSource )
		FS_Read( n->data, n->datasize, 1, fpSource );
	else
		Sys_Error( "Add to Queue called without data or file pointer!" );

	n->next = gp_hpak_queue;
	gp_hpak_queue = n;
}

/*
==================
HPAK_FlushHostQueue

==================
*/
void HPAK_FlushHostQueue( void )
{
	hash_pack_queue_t	*p;

	if ( !gp_hpak_queue )
		return;

	while ( 1 )
	{
		p = gp_hpak_queue;
		if ( !p )
			break;

		gp_hpak_queue = gp_hpak_queue->next;

		HPAK_AddLump( false, p->pakname, &p->resource, p->data, NULL );

		Mem_Free( p->pakname );
		Mem_Free( p->data );
		Mem_Free( p );
	}
}

/*
==================
HPAK_AddLump

==================
*/
void HPAK_AddLump( qboolean bUseQueue, char *pakname, struct resource_s *pResource, void *pData, FileHandle_t fpSource )
{
	int				i, j;
	FileHandle_t	iRead, iWrite;
	int				cmp;
	char			name[MAX_PATH];
	int				copysize;
	char			szTempName[MAX_PATH];
	char			szOriginalName[MAX_PATH];
	hash_pack_directory_t	olddirectory, newdirectory;
	qboolean		bExact;
	hash_pack_entry_t	*pCurrentEntry, *pNewEntry;

	if ( !pakname || !pResource || ( !pData && !fpSource ) )
	{
		if ( !pakname )
			Con_Printf( "HPAK_AddLump called with invalid arguments:  no .pak filename\n" );
		else if ( !pResource )
			Con_Printf( "HPAK_AddLump called with invalid arguments:  no lump to add\n" );
		else if ( !pData && !fpSource )
			Con_Printf( "HPAK_AddLump called with invalid arguments:  no file handle\n" );
		return;
	}

	if ( pResource->nDownloadSize < 1024 || pResource->nDownloadSize > MAX_FILE_SIZE )
	{
		Con_Printf( "HPAK_AddLump called with bogus lump, size:  %i\n", pResource->nDownloadSize );
		return;
	}

	{
		unsigned char	md5[16];
		MD5Context_t	ctx;

		Q_memset( &ctx, 0, sizeof( MD5Context_t ) );
		MD5Init( &ctx );

		if ( pData )
		{
			MD5Update( &ctx, (unsigned char *)pData, pResource->nDownloadSize );
		}
		else
		{
			int				nOrig;
			unsigned char	*pDiskData;

			nOrig = FS_Tell( fpSource );

			pDiskData = (unsigned char *)Mem_Malloc( pResource->nDownloadSize + 1 );
			Q_memset( pDiskData, 0, pResource->nDownloadSize );

			FS_Read( pDiskData, pResource->nDownloadSize, 1, fpSource );
			FS_Seek( fpSource, nOrig, FILESYSTEM_SEEK_HEAD );

			MD5Update( &ctx, pDiskData, pResource->nDownloadSize );
			Mem_Free( pDiskData );
		}

		MD5Final( md5, &ctx );

		if ( Q_memcmp( pResource->rgucMD5_hash, md5, sizeof( md5 ) ) )
		{
			Con_Printf( "HPAK_AddLump called with bogus lump, md5 mismatch\n" );
			Con_Printf( "Purported:  %s\n", MD5_Print( pResource->rgucMD5_hash ) );
			Con_Printf( "Actual   :  %s\n", MD5_Print( md5 ) );
			Con_Printf( "Ignoring lump addition\n" );
			return;
		}
	}

	if ( bUseQueue )
	{
		HPAK_AddToQueue( pakname, pResource, pData, fpSource );
		return;
	}

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );
	COM_FixSlashes( name );

	Q_strncpy( szOriginalName, name, 255 );
	szOriginalName[255] = 0;

	iRead = FS_Open( name, "rb" );
	if ( !iRead )
	{
		// no pack yet, so create one with this lump
		HPAK_CreatePak( pakname, pResource, pData, fpSource );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".hp2" );

	iWrite = FS_Open( szTempName, "w+b" );
	if ( !iWrite )
	{
		FS_Close( iRead );
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Read( &hash_pack_header, sizeof( hash_pack_header_t ), 1, iRead );
	if ( hash_pack_header.version != HASHPAK_VERSION )
	{
		FS_Close( iRead );
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		Con_Printf( "Invalid .hpk version in HPAK_AddLump\n" );
		return;
	}

	// copy the whole pack, the directory is written again at the end
	FS_Seek( iRead, 0, FILESYSTEM_SEEK_HEAD );
	copysize = FS_Size( iRead );
	COM_CopyFileChunk( iWrite, iRead, copysize );

	FS_Seek( iRead, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &olddirectory.nEntries, 4, 1, iRead );

	if ( olddirectory.nEntries < 0 || olddirectory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: .hpk had bogus # of directory entries:  %i\n", olddirectory.nEntries );
		FS_Close( iRead );
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		return;
	}

	olddirectory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * ( olddirectory.nEntries + 1 ) );
	FS_Read( olddirectory.p_rgEntries, sizeof( hash_pack_entry_t ) * olddirectory.nEntries, 1, iRead );
	FS_Close( iRead );

	bExact = HPAK_FindResource( &olddirectory, pResource->rgucMD5_hash, NULL );
	if ( bExact )
	{
		// already in the pack
		FS_Close( iWrite );
		FS_Unlink( szTempName );
		Mem_Free( olddirectory.p_rgEntries );
		return;
	}

	newdirectory.nEntries = olddirectory.nEntries + 1;
	newdirectory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * newdirectory.nEntries );
	Q_memset( newdirectory.p_rgEntries, 0, sizeof( hash_pack_entry_t ) * newdirectory.nEntries );
	Q_memcpy( newdirectory.p_rgEntries, olddirectory.p_rgEntries, sizeof( hash_pack_entry_t ) * olddirectory.nEntries );

	// keep the directory sorted by hash
	pNewEntry = NULL;
	for ( i = 0; i < olddirectory.nEntries; i++ )
	{
		pCurrentEntry = &olddirectory.p_rgEntries[i];

		cmp = Q_memcmp( pResource->rgucMD5_hash, pCurrentEntry->resource.rgucMD5_hash, 16 );
		if ( cmp < 0 )
		{
			pNewEntry = &newdirectory.p_rgEntries[i];

			for ( j = i; j < olddirectory.nEntries; j++ )
				newdirectory.p_rgEntries[j + 1] = olddirectory.p_rgEntries[j];
			break;
		}
	}

	if ( !pNewEntry )
		pNewEntry = &newdirectory.p_rgEntries[newdirectory.nEntries - 1];

	pCurrentEntry = pNewEntry;
	Q_memset( pCurrentEntry, 0, sizeof( hash_pack_entry_t ) );

	// the new lump goes where the old directory was
	FS_Seek( iWrite, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );

	pCurrentEntry->resource = *pResource;
	pCurrentEntry->nOffset = FS_Tell( iWrite );
	pCurrentEntry->nFileLength = pResource->nDownloadSize;

	if ( pData )
		FS_Write( pData, pCurrentEntry->nFileLength, 1, iWrite );
	else
		COM_CopyFileChunk( iWrite, fpSource, pCurrentEntry->nFileLength );

	hash_pack_header.nDirectoryOffset = FS_Tell( iWrite );

	FS_Write( &newdirectory.nEntries, 4, 1, iWrite );
	for ( i = 0; i < newdirectory.nEntries; i++ )
		FS_Write( &newdirectory.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, iWrite );

	if ( newdirectory.p_rgEntries )
		Mem_Free( newdirectory.p_rgEntries );
	if ( olddirectory.p_rgEntries )
		Mem_Free( olddirectory.p_rgEntries );

	FS_Seek( iWrite, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header_t ), 1, iWrite );
	FS_Close( iWrite );

	FS_Unlink( szOriginalName );
	FS_Rename( szTempName, szOriginalName );
}

/*
==================
HPAK_RemoveLump

==================
*/
void HPAK_RemoveLump( char *pakname, struct resource_s *pResource )
{
	char			name[MAX_PATH];
	FileHandle_t	fp, tmp;
	char			szTempName[MAX_PATH];
	char			szOriginalName[MAX_PATH];
	hash_pack_directory_t	hash_pack_dir;
	hash_pack_directory_t	newdir;
	hash_pack_entry_t	*oldentry, *newentry;
	int				n, i;
	qboolean		bFound;

	if ( !pakname || !*pakname || !pResource )
	{
		Con_Printf( "HPAK_RemoveLump:  Invalid arguments\n" );
		return;
	}

	HPAK_FlushHostQueue();

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	Q_strncpy( szOriginalName, name, 255 );
	szOriginalName[255] = 0;

	fp = FS_Open( szOriginalName, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open HPAK file %s for removal.\n", name );
		return;
	}

	COM_StripExtension( name, szTempName );
	COM_DefaultExtension( szTempName, ".hp2" );

	tmp = FS_Open( szTempName, "w+b" );
	if ( !tmp )
	{
		FS_Close( fp );
		Con_Printf( "ERROR: couldn't create %s.\n", szTempName );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( tmp, 0, FILESYSTEM_SEEK_HEAD );

	// Copy the header
	FS_Read( &hash_pack_header, sizeof( hash_pack_header_t ), 1, fp );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header_t ), 1, tmp );

	if ( Q_strncmp( hash_pack_header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	if ( hash_pack_header.version != HASHPAK_VERSION )
	{
		Con_Printf( "ERROR: HPAK version outdated\n" );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	FS_Seek( fp, hash_pack_header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &hash_pack_dir.nEntries, 4, 1, fp );

	if ( hash_pack_dir.nEntries < 1 || hash_pack_dir.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", hash_pack_dir.nEntries );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	if ( hash_pack_dir.nEntries == 1 )
	{
		Con_Printf( "Removing final lump from HPAK, deleting HPAK:\n  %s\n", szOriginalName );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		FS_Unlink( szOriginalName );
		return;
	}

	hash_pack_dir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * hash_pack_dir.nEntries );
	FS_Read( hash_pack_dir.p_rgEntries, sizeof( hash_pack_entry_t ) * hash_pack_dir.nEntries, 1, fp );

	newdir.nEntries = hash_pack_dir.nEntries - 1;
	newdir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * newdir.nEntries );

	bFound = HPAK_FindResource( &hash_pack_dir, pResource->rgucMD5_hash, NULL );
	if ( !bFound )
	{
		Con_Printf( "ERROR: HPAK doesn't contain specified lump:  %s\n", pResource->szFileName );
		FS_Close( tmp );
		FS_Close( fp );
		FS_Unlink( szTempName );
		return;
	}

	Con_Printf( "Removing %s from HPAK %s.\n", pResource->szFileName, name );

	// Copy the other lumps
	n = 0;
	for ( i = 0; i < hash_pack_dir.nEntries; i++ )
	{
		oldentry = &hash_pack_dir.p_rgEntries[i];
		if ( !Q_memcmp( oldentry->resource.rgucMD5_hash, pResource->rgucMD5_hash, 16 ) )
			continue;

		newentry = &newdir.p_rgEntries[n];
		n++;

		*newentry = *oldentry;
		newentry->nOffset = FS_Tell( tmp );

		FS_Seek( fp, oldentry->nOffset, FILESYSTEM_SEEK_HEAD );
		COM_CopyFileChunk( tmp, fp, newentry->nFileLength );
	}

	// Write the new directory
	hash_pack_header.nDirectoryOffset = FS_Tell( tmp );

	FS_Write( &newdir.nEntries, 4, 1, tmp );
	for ( i = 0; i < newdir.nEntries; i++ )
		FS_Write( &newdir.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, tmp );

	// Rewrite the header
	FS_Seek( tmp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header_t ), 1, tmp );

	FS_Close( tmp );
	FS_Close( fp );

	FS_Unlink( szOriginalName );
	FS_Rename( szTempName, szOriginalName );

	Mem_Free( newdir.p_rgEntries );
	Mem_Free( hash_pack_dir.p_rgEntries );
	hash_pack_dir.p_rgEntries = NULL;
	hash_pack_dir.nEntries = 0;
}

/*
==================
HPAK_ResourceForIndex

==================
*/
qboolean HPAK_ResourceForIndex( char *pakname, int nIndex, struct resource_s *pResource )
{
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	char			name[MAX_PATH];
	FileHandle_t	fp;

	if ( cmd_source != src_command )
		return false;

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return false;
	}

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return false;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "HPAK_List:  version mismatch\n" );
		FS_Close( fp );
		return false;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return false;
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );

	if ( nIndex < 1 || nIndex > directory.nEntries )
	{
		Con_Printf( "ERROR: HPAK bogus directory entry request:  %i\n", nIndex );
		FS_Close( fp );
		return false;
	}

	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	entry = &directory.p_rgEntries[nIndex - 1];
	*pResource = entry->resource;

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );

	return true;
}

/*
==================
HPAK_ResourceForHash

==================
*/
qboolean HPAK_ResourceForHash( char *pakname, unsigned char *hash, struct resource_s *pResourceEntry )
{
	qboolean		bFound;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	char			name[MAX_PATH];
	FileHandle_t	fp;

	bFound = false;

	if ( gp_hpak_queue )
	{
		hash_pack_queue_t	*p;

		p = gp_hpak_queue;
		while ( p )
		{
			if ( !Q_stricmp( p->pakname, pakname ) && !Q_memcmp( p->resource.rgucMD5_hash, hash, 16 ) )
			{
				if ( pResourceEntry )
					*pResourceEntry = p->resource;
				return true;
			}

			p = p->next;
		}
	}

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return false;
	}

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return false;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "HPAK_List:  version mismatch\n" );
		FS_Close( fp );
		return false;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return false;
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	bFound = HPAK_FindResource( &directory, hash, pResourceEntry );

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );

	return bFound;
}

/*
==================
HPAK_List_f

==================
*/
void HPAK_List_f( void )
{
	int				nCurrent;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	char			name[MAX_PATH];
	char			szFileName[MAX_PATH];
	char			type[32];
	FileHandle_t	fp;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	Con_Printf( "Contents for %s.\n", name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "HPAK_List:  version mismatch\n" );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		switch ( entry->resource.type )
		{
		case t_decal:
			sprintf( type, "decal" );
			break;
		case t_skin:
			sprintf( type, "skin" );
			break;
		case t_sound:
			sprintf( type, "sound" );
			break;
		case t_model:
			sprintf( type, "model" );
			break;
		case t_generic:
			sprintf( type, "generic" );
			break;
		case t_eventscript:
			sprintf( type, "event" );
			break;
		default:
			sprintf( type, "?" );
			break;
		}

		Con_Printf( "%i: %10s %.2fK %s\n  :  %s\n", nCurrent + 1, type, entry->resource.nDownloadSize / 1024.0f, szFileName, MD5_Print( entry->resource.rgucMD5_hash ) );
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

/*
==================
HPAK_CreatePak

==================
*/
void HPAK_CreatePak( char *pakname, struct resource_s *pResource, void *pData, FileHandle_t fpSource )
{
	int				i;
	char			name[MAX_PATH];
	int32			curpos;
	FileHandle_t	fp;
	hash_pack_entry_t	*pCurrentEntry;

	if ( ( pData && fpSource ) || ( !pData && !fpSource ) )
	{
		Con_Printf( "HPAK_CreatePak, must specify one of pData or fpSource\n" );
		return;
	}

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	Con_Printf( "Creating HPAK %s.\n", name );

	fp = FS_Open( name, "wb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open new .hpk, check access rights to %s.\n", name );
		return;
	}

	{
		unsigned char	md5[16];
		MD5Context_t	ctx;

		Q_memset( &ctx, 0, sizeof( MD5Context_t ) );
		MD5Init( &ctx );

		if ( pData )
		{
			MD5Update( &ctx, (unsigned char *)pData, pResource->nDownloadSize );
		}
		else
		{
			int				nOrig;
			unsigned char	*pDiskData;

			nOrig = FS_Tell( fpSource );

			pDiskData = (unsigned char *)Mem_Malloc( pResource->nDownloadSize + 1 );
			Q_memset( pDiskData, 0, pResource->nDownloadSize );

			FS_Read( pDiskData, pResource->nDownloadSize, 1, fpSource );
			FS_Seek( fpSource, nOrig, FILESYSTEM_SEEK_HEAD );

			MD5Update( &ctx, pDiskData, pResource->nDownloadSize );
			Mem_Free( pDiskData );
		}

		MD5Final( md5, &ctx );

		if ( Q_memcmp( pResource->rgucMD5_hash, md5, sizeof( md5 ) ) )
		{
			Con_Printf( "HPAK_CreatePak called with bogus lump, md5 mismatch\n" );
			Con_Printf( "Purported:  %s\n", MD5_Print( pResource->rgucMD5_hash ) );
			Con_Printf( "Actual   :  %s\n", MD5_Print( md5 ) );
			Con_Printf( "Ignoring lump addition\n" );
			return;
		}
	}

	Q_memset( &hash_pack_header, 0, sizeof( hash_pack_header_t ) );
	Q_strncpy( hash_pack_header.szFileStamp, "HPAK", 4 );
	hash_pack_header.version = HASHPAK_VERSION;
	hash_pack_header.nDirectoryOffset = 0;	// filled in below

	FS_Write( &hash_pack_header, sizeof( hash_pack_header_t ), 1, fp );

	Q_memset( &hash_pack_dir, 0, sizeof( hash_pack_directory_t ) );

	hash_pack_dir.nEntries = 1;
	hash_pack_dir.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * hash_pack_dir.nEntries );
	Q_memset( hash_pack_dir.p_rgEntries, 0, sizeof( hash_pack_entry_t ) * hash_pack_dir.nEntries );

	pCurrentEntry = &hash_pack_dir.p_rgEntries[0];
	pCurrentEntry->resource = *pResource;
	pCurrentEntry->nOffset = FS_Tell( fp );
	pCurrentEntry->nFileLength = pResource->nDownloadSize;

	if ( pData )
		FS_Write( pData, pCurrentEntry->nFileLength, 1, fp );
	else
		COM_CopyFileChunk( fp, fpSource, pCurrentEntry->nFileLength );

	curpos = FS_Tell( fp );

	FS_Write( &hash_pack_dir.nEntries, 4, 1, fp );
	for ( i = 0; i < hash_pack_dir.nEntries; i++ )
		FS_Write( &hash_pack_dir.p_rgEntries[i], sizeof( hash_pack_entry_t ), 1, fp );

	if ( hash_pack_dir.p_rgEntries )
		Mem_Free( hash_pack_dir.p_rgEntries );
	hash_pack_dir.p_rgEntries = NULL;
	hash_pack_dir.nEntries = 0;

	hash_pack_header.nDirectoryOffset = curpos;

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &hash_pack_header, sizeof( hash_pack_header_t ), 1, fp );

	FS_Close( fp );
}

/*
==================
HPAK_Remove_f

==================
*/
void HPAK_Remove_f( void )
{
	resource_t	resource;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage:  hpkremove <hpk> <index>\n" );
		return;
	}

	if ( !HPAK_ResourceForIndex( Cmd_Argv( 1 ), Q_atoi( Cmd_Argv( 2 ) ), &resource ) )
	{
		Con_Printf( "Could not locate resource %i in %s\n", Q_atoi( Cmd_Argv( 2 ) ), Cmd_Argv( 1 ) );
		return;
	}

	HPAK_RemoveLump( Cmd_Argv( 1 ), &resource );
}

/*
==================
HPAK_Validate_f

==================
*/
void HPAK_Validate_f( void )
{
	int				nCurrent;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	char			name[MAX_PATH];
	char			szFileName[MAX_PATH];
	char			type[32];
	FileHandle_t	fp;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "Usage:  hpkval hpkname\n" );
		return;
	}

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	Con_Printf( "Validating %s.\n", name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "hpkval:  version mismatch\n" );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		switch ( entry->resource.type )
		{
		case t_decal:
			sprintf( type, "decal" );
			break;
		case t_skin:
			sprintf( type, "skin" );
			break;
		case t_sound:
			sprintf( type, "sound" );
			break;
		case t_model:
			sprintf( type, "model" );
			break;
		case t_generic:
			sprintf( type, "generic" );
			break;
		case t_eventscript:
			sprintf( type, "event" );
			break;
		default:
			sprintf( type, "?" );
			break;
		}

		Con_Printf( "%i: %10s %.2fK %s:  ", nCurrent + 1, type, entry->resource.nDownloadSize / 1024.0f, szFileName );

		{
			unsigned char	*pData;
			int				nDataSize;
			unsigned char	md5[16];
			MD5Context_t	ctx;

			pData = NULL;
			nDataSize = entry->nFileLength;

			if ( nDataSize > 0 && nDataSize < MAX_FILE_SIZE )
			{
				pData = (unsigned char *)Mem_Malloc( nDataSize + 1 );
				Q_memset( pData, 0, nDataSize );

				FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
				FS_Read( pData, nDataSize, 1, fp );

				Q_memset( &ctx, 0, sizeof( MD5Context_t ) );
				MD5Init( &ctx );
				MD5Update( &ctx, pData, nDataSize );
				MD5Final( md5, &ctx );

				if ( Q_memcmp( entry->resource.rgucMD5_hash, md5, sizeof( md5 ) ) )
				{
					Con_Printf( " MISMATCHED\n" );
					Con_Printf( "--------------------\n" );
					Con_Printf( " File  :  %s\n", MD5_Print( entry->resource.rgucMD5_hash ) );
					Con_Printf( " Actual:  %s\n", MD5_Print( md5 ) );
					Con_Printf( "--------------------\n" );
				}
				else
				{
					Con_Printf( " OK\n" );
				}

				if ( pData )
					Mem_Free( pData );
			}
			else
			{
				Con_Printf( "Unable to MD5 hash data, size invalid:  %i\n", nDataSize );
			}
		}
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

/*
==================
HPAK_Extract_f

==================
*/
void HPAK_Extract_f( void )
{
	int				nCurrent;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	char			name[MAX_PATH];
	char			szFileName[MAX_PATH];
	char			type[32];
	FileHandle_t	fp;
	int				nIndex;

	nIndex = -1;

	if ( cmd_source != src_command )
		return;

	HPAK_FlushHostQueue();

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "Usage:  hpkextract hpkname [all | single index]\n" );
		return;
	}

	if ( Q_stricmp( Cmd_Argv( 2 ), "all" ) )
		nIndex = Q_atoi( Cmd_Argv( 2 ) );

	_snprintf( name, 256, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	if ( nIndex == -1 )
		Con_Printf( "Extracting all lumps from %s.\n", name );
	else
		Con_Printf( "Extracting lump %i from %s\n", nIndex, name );

	fp = FS_Open( name, "rb" );
	if ( !fp )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		return;
	}

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return;
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "hpkextract:  version mismatch\n" );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return;
	}

	Con_Printf( "# of Entries:  %i\n", directory.nEntries );
	Con_Printf( "# Type Size FileName : MD5 Hash\n" );

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];

		if ( nIndex != -1 && nIndex != nCurrent )
			continue;

		COM_FileBase( entry->resource.szFileName, szFileName );

		switch ( entry->resource.type )
		{
		case t_decal:
			sprintf( type, "decal" );
			break;
		case t_skin:
			sprintf( type, "skin" );
			break;
		case t_sound:
			sprintf( type, "sound" );
			break;
		case t_model:
			sprintf( type, "model" );
			break;
		case t_generic:
			sprintf( type, "generic" );
			break;
		case t_eventscript:
			sprintf( type, "event" );
			break;
		default:
			sprintf( type, "?" );
			break;
		}

		Con_Printf( "Extracting %i: %10s %.2fK %s\n", nCurrent + 1, type, entry->resource.nDownloadSize / 1024.0f, szFileName );

		{
			unsigned char	*pData;
			int				nDataSize;
			FileHandle_t	fpOutput;
			char			szFileOut[MAX_PATH];

			pData = NULL;
			nDataSize = entry->nFileLength;

			if ( nDataSize > 0 && nDataSize < MAX_FILE_SIZE )
			{
				pData = (unsigned char *)Mem_Malloc( nDataSize + 1 );
				Q_memset( pData, 0, nDataSize );

				FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
				FS_Read( pData, nDataSize, 1, fp );

				_snprintf( szFileOut, sizeof( szFileOut ), "hpklmps\\lmp%04i.wad", nCurrent );
				COM_FixSlashes( szFileOut );
				COM_CreatePath( szFileOut );

				fpOutput = FS_Open( szFileOut, "wb" );
				if ( fpOutput )
				{
					FS_Write( pData, nDataSize, 1, fpOutput );
					FS_Close( fpOutput );
				}
				else
				{
					Con_Printf( "Error creating lump file %s\n", szFileOut );
				}

				if ( pData )
					Mem_Free( pData );
			}
			else
			{
				Con_Printf( "Unable to extract data, size invalid:  %s\n", nDataSize );
			}
		}
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

/*
==================
HPAK_Init

==================
*/
void HPAK_Init( void )
{
	Cmd_AddCommand( "hpklist", HPAK_List_f );
	Cmd_AddCommand( "hpkremove", HPAK_Remove_f );
	Cmd_AddCommand( "hpkval", HPAK_Validate_f );
	Cmd_AddCommand( "hpkextract", HPAK_Extract_f );

	gp_hpak_queue = NULL;
}

/*
==================
HPAK_GetItem

Returns the !MD5 name of a lump of custom.hpk
==================
*/
char *HPAK_GetItem( int item )
{
	int				nCurrent;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	static char		name[MAX_PATH];
	char			szFileName[MAX_PATH];
	FileHandle_t	fp;

	HPAK_FlushHostQueue();

	_snprintf( name, 256, "%s", "custom" );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );

	fp = FS_Open( name, "rb" );
	if ( !fp )
		return "";

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );
	if ( Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not an HPAK file\n", name );
		FS_Close( fp );
		return "";
	}

	if ( header.version != HASHPAK_VERSION )
	{
		Con_Printf( "HPAK_List:  version mismatch\n" );
		FS_Close( fp );
		return "";
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK had bogus # of directory entries:  %i\n", directory.nEntries );
		FS_Close( fp );
		return "";
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	nCurrent = min( item, directory.nEntries - 1 );

	entry = &directory.p_rgEntries[nCurrent];
	COM_FileBase( entry->resource.szFileName, szFileName );

	_snprintf( name, sizeof( name ), "!MD5%s", MD5_Print( entry->resource.rgucMD5_hash ) );

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );

	return name;
}

/*
==================
HPAK_CheckSize

Deletes the pack when it grew past hpk_maxsize megabytes
==================
*/
void HPAK_CheckSize( char *pakname )
{
	char			fullname[MAX_PATH];
	float			maxSize;
	float			actualSize;
	FileHandle_t	hfile;

	if ( hpk_maxsize.value == 0 || !pakname )
		return;

	if ( hpk_maxsize.value < 0 )
	{
		Con_Printf( "hpk_maxsize < 0, setting to 0\n" );
		Cvar_DirectSet( &hpk_maxsize, "0" );
		return;
	}

	_snprintf( fullname, 256, "%s", pakname );
	COM_DefaultExtension( fullname, HASHPAK_EXTENSION );
	COM_FixSlashes( fullname );

	maxSize = hpk_maxsize.value * 1000000.0f;

	hfile = FS_Open( fullname, "rb" );
	actualSize = 0;
	if ( hfile )
	{
		actualSize = FS_Size( hfile );
		FS_Close( hfile );
	}

	if ( actualSize >= maxSize )
	{
		Con_Printf( "Server: Size of %s > %f MB, deleting.\n", fullname, hpk_maxsize.value );
		Log_Printf( "Server: Size of %s > %f MB, deleting.\n", fullname, hpk_maxsize.value );
		FS_RemoveFile( fullname, NULL );
	}
}

/*
==================
HPAK_ValidatePak

Deletes the pack when it is damaged
==================
*/
void HPAK_ValidatePak( char *fullpakname )
{
	int				nCurrent;
	hash_pack_header_t		header;
	hash_pack_directory_t	directory;
	hash_pack_entry_t		*entry;
	char			szFileName[MAX_PATH];
	FileHandle_t	fp;

	HPAK_FlushHostQueue();

	fp = FS_Open( fullpakname, "rb" );
	if ( !fp )
		return;

	FS_Read( &header, sizeof( hash_pack_header_t ), 1, fp );

	if ( header.version != HASHPAK_VERSION || Q_strncmp( header.szFileStamp, "HPAK", 4 ) )
	{
		Con_Printf( "%s is not a PAK file, deleting\n", fullpakname );
		FS_Close( fp );
		FS_RemoveFile( fullpakname, NULL );
		return;
	}

	FS_Seek( fp, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &directory.nEntries, 4, 1, fp );

	if ( directory.nEntries < 1 || directory.nEntries > MAX_FILE_ENTRIES )
	{
		Con_Printf( "ERROR: HPAK %s had bogus # of directory entries:  %i, deleting\n", fullpakname, directory.nEntries );
		FS_Close( fp );
		FS_RemoveFile( fullpakname, NULL );
		return;
	}

	directory.p_rgEntries = (hash_pack_entry_t *)Mem_Malloc( sizeof( hash_pack_entry_t ) * directory.nEntries );
	FS_Read( directory.p_rgEntries, sizeof( hash_pack_entry_t ) * directory.nEntries, 1, fp );

	for ( nCurrent = 0; nCurrent < directory.nEntries; nCurrent++ )
	{
		entry = &directory.p_rgEntries[nCurrent];
		COM_FileBase( entry->resource.szFileName, szFileName );

		{
			unsigned char	*pData;
			int				nDataSize;
			unsigned char	md5[16];
			MD5Context_t	ctx;
			int				haderrors;

			pData = NULL;
			haderrors = 0;
			nDataSize = entry->nFileLength;

			if ( nDataSize > 0 && nDataSize < MAX_FILE_SIZE )
			{
				pData = (unsigned char *)Mem_Malloc( nDataSize + 1 );
				Q_memset( pData, 0, nDataSize );

				FS_Seek( fp, entry->nOffset, FILESYSTEM_SEEK_HEAD );
				FS_Read( pData, nDataSize, 1, fp );

				Q_memset( &ctx, 0, sizeof( MD5Context_t ) );
				MD5Init( &ctx );
				MD5Update( &ctx, pData, nDataSize );
				MD5Final( md5, &ctx );

				if ( Q_memcmp( entry->resource.rgucMD5_hash, md5, sizeof( md5 ) ) )
				{
					Con_Printf( "Mismatched data in HPAK file %s, deleting\n", fullpakname );
					haderrors = 1;
				}

				if ( pData )
					Mem_Free( pData );
			}
			else
			{
				haderrors = 2;
			}

			if ( haderrors )
			{
				Mem_Free( directory.p_rgEntries );

				if ( haderrors == 2 )
				{
					Con_Printf( "Mismatched data in HPAK file %s, deleting\n", fullpakname );
					Con_Printf( "Unable to MD5 hash data lump %i, size invalid:  %i\n", nCurrent + 1, nDataSize );
				}

				FS_Close( fp );
				FS_RemoveFile( fullpakname, NULL );
				return;
			}
		}
	}

	FS_Close( fp );
	Mem_Free( directory.p_rgEntries );
}

/*
==================
HPAK_CheckIntegrity

==================
*/
void HPAK_CheckIntegrity( char *pakname )
{
	char	name[MAX_PATH];

	_snprintf( name, 256, "%s", pakname );
	COM_DefaultExtension( name, HASHPAK_EXTENSION );
	COM_FixSlashes( name );

	HPAK_ValidatePak( name );
}
