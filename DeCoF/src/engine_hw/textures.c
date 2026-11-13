// textures.c -- the textures of a map from its wad files

#include "quakedef.h"
#include "wad.h"
#include "mem.h"

#include <ctype.h>

#define MAX_MAP_TEXTURES	512

typedef struct
{
	lumpinfo_t	lump;
	int			iTexFile;	// index of the wad this texture is located in
} texlumpinfo_t;

int				nummiptex;
char			miptex[MAX_MAP_TEXTURES][64];

FileHandle_t	texfiles[128];
int				nTexFiles = 0;
texlumpinfo_t	*lumpinfo = NULL;
int				nTexLumps = 0;

void SafeRead( FileHandle_t f, void *buffer, int count )
{
	if ( FS_Read( buffer, count, 1, f ) != count )
		Sys_Error( "File read failure" );
}

void CleanupName( char *in, char *out )
{
	int		i;

	for ( i = 0; i < 16; i++ )
	{
		if ( !in[i] )
			break;

		out[i] = toupper( in[i] );
	}

	for ( ; i < 16; i++ )
		out[i] = 0;
}

/*
=================
lump_sorter
=================
*/
int lump_sorter( const void *lump1, const void *lump2 )
{
	texlumpinfo_t	*plump1 = (texlumpinfo_t *)lump1;
	texlumpinfo_t	*plump2 = (texlumpinfo_t *)lump2;
	return strcmp( plump1->lump.name, plump2->lump.name );
}

void ForwardSlashes( char *pname )
{
	while ( *pname )
	{
		if ( *pname == '\\' )
			*pname = '/';

		pname++;
	}
}

/*
=================
TEX_InitFromWad
=================
*/
qboolean TEX_InitFromWad( char *path )
{
	int			i;
	wadinfo_t	wadinfo;
	char		szTmpPath[1024];
	char		*pszWadFile;

	Q_strncpy( szTmpPath, path, sizeof( szTmpPath ) - 2 );
	szTmpPath[sizeof( szTmpPath ) - 2] = 0;

	// temporary kludge so we don't have to deal with no occurances of a semicolon
	//  in the path name ..
	if ( strchr( szTmpPath, ';' ) == NULL )
		Q_strcat( szTmpPath, ";" );

	pszWadFile = strtok( szTmpPath, ";" );

	while ( pszWadFile )
	{
		FileHandle_t	texfile;	// temporary used in this loop
		char			wadPath[MAX_PATH], wadName[MAX_PATH];

		ForwardSlashes( pszWadFile );
		COM_FileBase( pszWadFile, wadName );
		_snprintf( wadPath, 256, "%s", wadName );
		COM_DefaultExtension( wadPath, ".wad" );

		// the player decal wads are not texture wads
		if ( Q_strstr( wadName, "pldecal" ) || Q_strstr( wadName, "tempdecal" ) )
		{
			pszWadFile = strtok( NULL, ";" );
			continue;
		}

		texfiles[nTexFiles] = FS_Open( wadPath, "rb" );

		if ( !texfiles[nTexFiles] )
		{
			Sys_Error( "WARNING: couldn't open %s\n", wadPath );
			return false;
		}

		++nTexFiles;

		// temp assignment to make things cleaner:
		texfile = texfiles[nTexFiles - 1];

		Con_DPrintf( "Using WAD File: %s\n", wadPath );

		SafeRead( texfile, &wadinfo, sizeof( wadinfo ) );
		if ( Q_strncmp( wadinfo.identification, "WAD2", 4 ) &&
			Q_strncmp( wadinfo.identification, "WAD3", 4 ) )
			Sys_Error( "TEX_InitFromWad: %s isn't a wadfile", wadPath );
		wadinfo.numlumps = LittleLong( wadinfo.numlumps );
		wadinfo.infotableofs = LittleLong( wadinfo.infotableofs );
		FS_Seek( texfile, wadinfo.infotableofs, FILESYSTEM_SEEK_HEAD );
		lumpinfo = Mem_Realloc( lumpinfo, ( nTexLumps + wadinfo.numlumps ) * sizeof( texlumpinfo_t ) );

		for ( i = 0; i < wadinfo.numlumps; i++ )
		{
			SafeRead( texfile, &lumpinfo[nTexLumps], sizeof( texlumpinfo_t ) - sizeof( int ) );	// iTexFile is NOT read from file
			CleanupName( lumpinfo[nTexLumps].lump.name, lumpinfo[nTexLumps].lump.name );
			lumpinfo[nTexLumps].lump.filepos = LittleLong( lumpinfo[nTexLumps].lump.filepos );
			lumpinfo[nTexLumps].lump.disksize = LittleLong( lumpinfo[nTexLumps].lump.disksize );
			lumpinfo[nTexLumps].iTexFile = nTexFiles - 1;

			++nTexLumps;
		}

		// next wad file
		pszWadFile = strtok( NULL, ";" );
	}

	qsort( (void *)lumpinfo, (size_t)nTexLumps, sizeof( lumpinfo[0] ), lump_sorter );

	return true;
}

/*
=================
TEX_CleanupWadInfo
=================
*/
void TEX_CleanupWadInfo( void )
{
	int		i;

	if ( lumpinfo )
	{
		Mem_Free( lumpinfo );
		lumpinfo = NULL;
	}

	for ( i = 0; i < nTexFiles; i++ )
	{
		FS_Close( texfiles[i] );
		texfiles[i] = NULL;
	}

	nTexLumps = 0;
	nTexFiles = 0;
}

/*
=================
TEX_LoadLump
=================
*/
int TEX_LoadLump( char *name, unsigned char *dest )
{
	texlumpinfo_t	key, *found;

	CleanupName( name, key.lump.name );
	found = bsearch( &key, lumpinfo, nTexLumps, sizeof( texlumpinfo_t ), lump_sorter );
	if ( found )
	{
		FS_Seek( texfiles[found->iTexFile], found->lump.filepos, FILESYSTEM_SEEK_HEAD );
		SafeRead( texfiles[found->iTexFile], dest, found->lump.disksize );
		return found->lump.disksize;
	}

	Con_SafePrintf( "WARNING: texture lump \"%s\" not found\n", name );
	return 0;
}

/*
=================
FindMiptex
=================
*/
int FindMiptex( char *name )
{
	int		i;

	for ( i = 0; i < nummiptex; i++ )
	{
		if ( !Q_strcasecmp( name, miptex[i] ) )
			return i;
	}

	if ( nummiptex == MAX_MAP_TEXTURES )
		Sys_Error( "Exceeded MAX_MAP_TEXTURES" );
	Q_strncpy( miptex[i], name, sizeof( miptex[i] ) - 1 );
	miptex[i][sizeof( miptex[i] ) - 1] = 0;
	nummiptex++;
	return i;
}

/*
==================
TEX_AddAnimatingTextures
==================
*/
void TEX_AddAnimatingTextures( void )
{
	int		base;
	int		i, j, k;
	char	name[32];

	base = nummiptex;

	for ( i = 0; i < base; i++ )
	{
		if ( miptex[i][0] != '+' && miptex[i][0] != '-' )
			continue;
		Q_strncpy( name, miptex[i], sizeof( name ) - 1 );
		name[sizeof( name ) - 1] = 0;

		for ( j = 0; j < 20; j++ )
		{
			if ( j < 10 )
				name[1] = '0' + j;
			else
				name[1] = 'A' + j - 10;		// alternate animation

		// see if this name exists in the wadfile
			for ( k = 0; k < nTexLumps; k++ )
				if ( !Q_strcmp( name, lumpinfo[k].lump.name ) )
				{
					FindMiptex( name );	// add to the miptex list
					break;
				}
		}
	}

	if ( nummiptex - base )
		Con_DPrintf( "added %i texture frames\n", nummiptex - base );
}
