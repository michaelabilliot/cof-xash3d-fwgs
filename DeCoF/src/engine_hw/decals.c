// decals.c -- decal and custom (spray) wad caches

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "mem.h"
#include "zone.h"
#include "host.h"
#include "sys.h"
#include "console.h"
#include "custom.h"
#include "client.h"
#include "GL_MODEL.H"
#include "bspfile.h"
#include "wad.h"
#include "draw.h"
#include "decal.h"
#include "filesystem_.h"
#include "vgui_int.h"

typedef struct lumplist_s
{
	lumpinfo_t			*lump;
	qboolean			breplaced;
	struct lumplist_s	*next;
} lumplist_t;

qboolean	gfCustomBuild;
char		szCustName[10];
char		decal_names[MAX_DECALS][16];

static cachewad_t	*decal_wad;
cachewad_t			*menu_wad;

qboolean	m_bDrawInitialized;

void Draw_Shutdown( void )
{
	if ( !m_bDrawInitialized )
		return;

	m_bDrawInitialized = false;
	Draw_FreeWad( menu_wad );
	if ( menu_wad )
		Mem_Free( menu_wad );
	menu_wad = NULL;
}

void Draw_DecalShutdown( void )
{
	Draw_FreeWad( decal_wad );
	if ( decal_wad )
		Mem_Free( decal_wad );
	decal_wad = NULL;
}

void Draw_AllocateCacheSpace( cachewad_t *wad, int cacheMax, int tempWad )
{
	int		len;

	len = cacheMax * sizeof( cacheentry_t );
	wad->cache = Mem_Malloc( len );
	Q_memset( wad->cache, 0, len );
	wad->tempWad = tempWad;
}

void Draw_CacheWadInitFromFile( FileHandle_t hFile, int len, char *name, int cacheMax, cachewad_t *wad )
{
	lumpinfo_t	*lump_p;
	wadinfo_t	header;
	int			i;

	FS_Read( &header, sizeof( header ), 1, hFile );

	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D' || header.identification[3] != '3' )
		Sys_Error( "Wad file %s doesn't have WAD3 id\n", name );

	wad->lumps = Mem_Malloc( len - header.infotableofs );
	FS_Seek( hFile, header.infotableofs, FILESYSTEM_SEEK_HEAD );
	FS_Read( wad->lumps, len - header.infotableofs, 1, hFile );

	for ( i = 0, lump_p = wad->lumps; i < header.numlumps; i++, lump_p++ )
		W_CleanupName( lump_p->name, lump_p->name );

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( name );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;
}

void Draw_CacheWadInit( char *name, int cacheMax, cachewad_t *wad )
{
	int				len;
	FileHandle_t	hFile;

	hFile = FS_Open( name, "rb" );
	if ( !hFile )
		Sys_Error( "Draw_LoadWad: Couldn't open %s\n", name );

	len = FS_Size( hFile );
	Draw_CacheWadInitFromFile( hFile, len, name, cacheMax, wad );
	FS_Close( hFile );
}

qboolean Draw_CustomCacheWadInit( int cacheMax, cachewad_t *wad, void *raw, int nFileSize )
{
	lumpinfo_t		*lump_p;
	wadinfo_t		header;
	unsigned int	i;
	int				len;
	byte			*pStart;

	pStart = (byte *)raw;
	header = *(wadinfo_t *)raw;

	if ( header.identification[0] != 'W' || header.identification[1] != 'A' || header.identification[2] != 'D' || header.identification[3] != '3' )
	{
		Con_Printf( "Custom file doesn't have WAD3 id\n" );
		return false;
	}

	if ( header.numlumps != 1 )
	{
		Con_Printf( "Custom file has wrong number of lumps %i\n", header.numlumps );
		return false;
	}

	if ( header.infotableofs <= 0 )
	{
		Con_Printf( "Custom file has bogus infotableofs %i\n", header.infotableofs );
		return false;
	}

	if ( header.infotableofs + header.numlumps * sizeof( lumpinfo_t ) != nFileSize )
	{
		Con_Printf( "Custom file has bogus infotableofs ( %i > %i )\n", header.infotableofs + header.numlumps * sizeof( lumpinfo_t ), nFileSize );
		return false;
	}

	len = nFileSize;
	wad->lumps = Mem_Malloc( len - header.infotableofs );
	Q_memcpy( wad->lumps, pStart + header.infotableofs, len - header.infotableofs );

	for ( i = 0, lump_p = wad->lumps; i < header.numlumps; i++, lump_p++ )
	{
		W_CleanupName( lump_p->name, lump_p->name );

		if ( lump_p->size != lump_p->disksize )
		{
			Con_Printf( "Custom file has mismatched lump size ( %i vs. %i )\n", lump_p->size, lump_p->disksize );
			return false;
		}

		if ( lump_p->disksize <= 0 )
		{
			Con_Printf( "Custom file has bogus lump size %i\n", lump_p->disksize );
			return false;
		}

		if ( lump_p->filepos < sizeof( wadinfo_t ) )
		{
			Con_Printf( "Custom file has bogus lump offset %i\n", lump_p->filepos );
			return false;
		}

		if ( lump_p->filepos + lump_p->disksize > header.infotableofs )
		{
			Con_Printf( "Custom file has bogus lump %i\n", i );
			return false;
		}
	}

	wad->lumpCount = header.numlumps;
	wad->cacheCount = 0;
	wad->cacheMax = cacheMax;
	wad->name = Mem_Strdup( "tempdecal.wad" );
	Draw_AllocateCacheSpace( wad, cacheMax, 0 );
	wad->pfnCacheBuild = NULL;
	wad->cacheExtra = 0;

	return true;
}

void Draw_CacheWadHandler( cachewad_t *wad, PFNCACHE fn, int extraDataSize )
{
	wad->pfnCacheBuild = fn;
	wad->cacheExtra = extraDataSize;
}

void Draw_FreeWad( cachewad_t *pWad )
{
	cacheentry_t	*pic;
	int				i;

	if ( !pWad )
		return;

	if ( pWad->lumps )
		Mem_Free( pWad->lumps );
	pWad->lumps = NULL;

	Mem_Free( pWad->name );

	if ( pWad->numpaths )
	{
		for ( i = 0; i < pWad->numpaths; i++ )
		{
			Mem_Free( pWad->basedirs[i] );
			pWad->basedirs[i] = NULL;
		}
		Mem_Free( pWad->basedirs );
		pWad->basedirs = NULL;
	}

	if ( pWad->lumppathindices )
	{
		Mem_Free( pWad->lumppathindices );
		pWad->lumppathindices = NULL;
	}

	pic = pWad->cache;
	if ( pic )
	{
		if ( !pWad->tempWad )
		{
			int		i;

			for ( i = 0; i < pWad->cacheCount; i++, pic++ )
			{
				if ( Cache_Check( &pic->cache ) )
					Cache_Free( &pic->cache );
			}
		}
		Mem_Free( pWad->cache );
	}
}

void Draw_DecalSetName( int decal, char *name )
{
	if ( decal < MAX_DECALS )
	{
		Q_strncpy( decal_names[decal], name, sizeof( decal_names[0] ) - 1 );
		decal_names[decal][ sizeof( decal_names[0] ) - 1 ] = 0;
	}
}

int Draw_DecalIndex( int id )
{
	char	tmp[32];
	char	*pName;
	extern	cvar_t	violence_hblood;

	if ( !decal_names[id][0] )
		Sys_Error( "Used decal #%d without a name\n", id );

	pName = decal_names[id];
	if ( !Host_IsServerActive() && !violence_hblood.value )
	{
		if ( !Q_strncmp( pName, "{blood", 6 ) )
		{
			_snprintf( tmp, sizeof( tmp ), "{yblood%s", pName + 6 );
			pName = tmp;
		}
	}

	return Draw_CacheIndex( decal_wad, pName );
}

int Draw_DecalCount( void )
{
	if ( decal_wad )
		return decal_wad->lumpCount;
	return 0;
}

int Draw_DecalSize( int number )
{
	if ( !decal_wad )
		return 0;

	if ( number >= decal_wad->lumpCount )
		return 0;

	return decal_wad->lumps[number].size;
}

const char *Draw_DecalName( int number )
{
	if ( !decal_wad )
		return NULL;

	if ( number >= decal_wad->lumpCount )
		return NULL;

	return decal_wad->lumps[number].name;
}

texture_t *Draw_DecalTexture( int index )
{
	int				playernum;
	customization_t	*pCust = NULL;

	if ( index < 0 )
	{
		playernum = -index;
		playernum--;

		pCust = cl.players[playernum].customdata.pNext;
		if ( pCust && pCust->bInUse && pCust->pInfo && pCust->pBuffer )
		{
			texture_t	*retval;

			retval = Draw_CustomCacheGet( pCust->pInfo, pCust->pBuffer, pCust->resource.nDownloadSize, pCust->nUserData1 );
			if ( retval )
				return retval;

			return Draw_CacheGet( decal_wad, 0 );
		}

		Sys_Error( "Failed to load custom decal for player #%i:%s using default decal 0.\n", playernum, cl.players[playernum].name );
		return NULL;
	}

	if ( index & 0x4000 )
		return AdsDecalTexture_guess( index );

	return Draw_CacheGet( decal_wad, index );
}

int Draw_CacheByIndex( cachewad_t *wad, int nIndex, int playernum )
{
	cacheentry_t	*pic;
	int				i;
	char			szTestName[32];

	_snprintf( szTestName, sizeof( szTestName ), "%03i%02i", playernum, nIndex );

	for ( pic = wad->cache, i = 0; i < wad->cacheCount; pic++, i++ )
	{
		if ( !Q_strcmp( pic->name, szTestName ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );
		wad->cacheCount++;
		_snprintf( pic->name, sizeof( pic->name ), "%s", szTestName );
	}

	return i;
}

int Draw_DecalIndexFromName( char *name )
{
	char	tmpName[16];
	int		i;

	Q_strncpy( tmpName, name, sizeof( tmpName ) - 1 );
	tmpName[ sizeof( tmpName ) - 1 ] = 0;

	if ( tmpName[0] == '}' )
		tmpName[0] = '{';

	for ( i = 0; i < MAX_DECALS; i++ )
	{
		if ( decal_names[i][0] && !Q_strcmp( tmpName, decal_names[i] ) )
			return i;
	}

	return 0;
}

void Decal_ReplaceOrAppendLump( lumplist_t **ppList, lumpinfo_t *lump, qboolean bsecondlump )
{
	lumplist_t	*p;

	p = *ppList;
	while ( p )
	{
		if ( !Q_stricmp( lump->name, p->lump->name ) )
		{
			Mem_Free( p->lump );
			p->lump = Mem_Malloc( sizeof( lumpinfo_t ) );
			Q_memcpy( p->lump, lump, sizeof( lumpinfo_t ) );
			p->breplaced = bsecondlump;
			return;
		}
		p = p->next;
	}

	p = Mem_Malloc( sizeof( lumplist_t ) );
	Q_memset( p, 0, sizeof( lumplist_t ) );
	p->lump = Mem_Malloc( sizeof( lumpinfo_t ) );
	Q_memcpy( p->lump, lump, sizeof( lumpinfo_t ) );
	p->breplaced = bsecondlump;
	p->next = *ppList;
	*ppList = p;
}

int Decal_CountLumps( lumplist_t *plist )
{
	int			c;
	lumplist_t	*p;

	c = 0;
	p = plist;
	while ( p )
	{
		c++;
		p = p->next;
	}

	return c;
}

int Decal_SizeLumps( lumplist_t *plist )
{
	return Decal_CountLumps( plist ) * sizeof( lumpinfo_t );
}

void Decal_MergeInDecals( cachewad_t *pwad, const char *pathID )
{
	int			i;
	int			lumpcount;
	lumpinfo_t	*lump;
	lumplist_t	*lumplist, *plump, *pnext;
	cachewad_t	*final;

	lumplist = NULL;

	if ( !pwad )
	{
		Sys_Error( "Decal_MergeInDecals called with NULL wad\n" );
		return;
	}

	if ( !decal_wad )
	{
		decal_wad = pwad;
		decal_wad->numpaths = 1;
		decal_wad->basedirs = Mem_Malloc( decal_wad->numpaths * sizeof( char * ) );
		decal_wad->basedirs[0] = Mem_Strdup( pathID );
		decal_wad->lumppathindices = Mem_Malloc( decal_wad->cacheMax * sizeof( int ) );
		Q_memset( decal_wad->lumppathindices, 0, decal_wad->cacheMax * sizeof( int ) );
		return;
	}

	final = Mem_Malloc( sizeof( cachewad_t ) );
	Q_memset( final, 0, sizeof( cachewad_t ) );

	for ( i = 0; i < decal_wad->lumpCount; i++ )
	{
		lump = &decal_wad->lumps[i];
		Decal_ReplaceOrAppendLump( &lumplist, lump, false );
	}

	for ( i = 0; i < pwad->lumpCount; i++ )
	{
		lump = &pwad->lumps[i];
		Decal_ReplaceOrAppendLump( &lumplist, lump, true );
	}

	lumpcount = Decal_CountLumps( lumplist );
	final->lumpCount = lumpcount;
	final->cacheCount = 0;
	final->cacheMax = decal_wad->cacheMax;
	final->name = Mem_Strdup( decal_wad->name );
	Draw_AllocateCacheSpace( final, final->cacheMax, 0 );
	final->pfnCacheBuild = decal_wad->pfnCacheBuild;
	final->cacheExtra = decal_wad->cacheExtra;

	final->lumppathindices = Mem_Malloc( final->cacheMax * sizeof( int ) );
	Q_memset( final->lumppathindices, 0, final->cacheMax * sizeof( int ) );

	final->numpaths = 2;
	final->basedirs = Mem_Malloc( final->numpaths * sizeof( char * ) );
	final->basedirs[0] = Mem_Strdup( decal_wad->basedirs[0] );
	final->basedirs[1] = Mem_Strdup( pathID );

	final->lumps = Mem_Malloc( Decal_SizeLumps( lumplist ) );

	lump = final->lumps;
	plump = lumplist;
	i = 0;
	while ( plump )
	{
		pnext = plump->next;
		Q_memcpy( lump, plump->lump, sizeof( lumpinfo_t ) );
		Mem_Free( plump->lump );
		plump->lump = NULL;
		final->lumppathindices[i] = ( plump->breplaced != 0 );
		Mem_Free( plump );
		plump = pnext;
		lump++;
		i++;
	}
	lumplist = NULL;

	Draw_FreeWad( decal_wad );
	if ( decal_wad )
		Mem_Free( decal_wad );
	decal_wad = final;

	Draw_FreeWad( pwad );
	Mem_Free( pwad );
}

void Decal_Init( void )
{
	int				i;
	FileHandle_t	hfile;
	char			pszPathID[2][15] = { "DEFAULTGAME", "GAME" };

	Draw_DecalShutdown();

	for ( i = 0; i < 2; i++ )
	{
		hfile = FS_OpenPathID( "decals.wad", "rb", pszPathID[i] );
		if ( hfile )
		{
			int			filesize;
			cachewad_t	*decal_wad_temp;

			filesize = FS_Size( hfile );
			decal_wad_temp = Mem_Malloc( sizeof( cachewad_t ) );
			Q_memset( decal_wad_temp, 0, sizeof( cachewad_t ) );
			Draw_CacheWadInitFromFile( hfile, filesize, "decals.wad", MAX_DECALS, decal_wad_temp );
			Draw_CacheWadHandler( decal_wad_temp, Draw_MiptexTexture, 32 );
			Decal_MergeInDecals( decal_wad_temp, pszPathID[i] );
			FS_Close( hfile );
		}
		else if ( i == 0 )
		{
			Sys_Error( "Couldn't find '%s' in \"%s\" search path\n", "decals.wad", pszPathID[i] );
		}
	}

	sv_decalnamecount = Draw_DecalCount();
	if ( sv_decalnamecount > MAX_DECALS )
		Sys_Error( "Too many decals: %d / %d\n", sv_decalnamecount, MAX_DECALS );

	for ( i = 0; i < sv_decalnamecount; i++ )
	{
		Q_memset( &sv_decalnames[i], 0, sizeof( decalname_t ) );
		Q_strncpy( sv_decalnames[i].name, Draw_DecalName( i ), sizeof( sv_decalnames[i].name ) - 1 );
		sv_decalnames[i].name[ sizeof( sv_decalnames[i].name ) - 1 ] = 0;
	}
}

qboolean CustomDecal_Validate( void *raw, int nFileSize )
{
	qboolean	bret;
	cachewad_t	*fakewad;

	bret = false;

	fakewad = Mem_ZeroMalloc( sizeof( cachewad_t ) );
	if ( !fakewad )
		return bret;

	bret = CustomDecal_Init( fakewad, raw, nFileSize, -2 );
	if ( bret )
		bret = ( Draw_CustomCacheGet( fakewad, raw, nFileSize, 0 ) != NULL );

	Draw_FreeWad( fakewad );
	Mem_Free( fakewad );

	return bret;
}

qboolean CustomDecal_Init( struct cachewad_s *wad, void *raw, int nFileSize, int playernum )
{
	int			i;
	qboolean	bret = true;

	bret = Draw_CustomCacheWadInit( 16, wad, raw, nFileSize );
	if ( bret )
	{
		Draw_CacheWadHandler( wad, Draw_MiptexTexture, 32 );
		for ( i = 0; i < wad->lumpCount; i++ )
			Draw_CacheByIndex( wad, i, playernum );
	}

	return bret;
}

void *Draw_CacheGet( cachewad_t *wad, int index )
{
	cacheentry_t	*pic;
	int				i;
	void			*dat;
	char			*path;

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];

	if ( !wad->tempWad )
	{
		dat = Cache_Check( &pic->cache );
		if ( dat )
			return dat;
	}

	{
		char		name[16];
		char		clean[16];
		lumpinfo_t	*pLump;

		path = pic->name;
		COM_FileBase( path, name );
		W_CleanupName( name, clean );

		for ( pLump = wad->lumps, i = 0; i < wad->lumpCount; i++, pLump++ )
		{
			if ( !Q_strcmp( clean, pLump->name ) )
				break;
		}

		if ( i >= wad->lumpCount )
			return NULL;

		if ( !Draw_CacheReload( wad, i, pLump, pic, clean, path ) )
			return NULL;
	}

	dat = pic->cache.data;
	if ( !dat )
		Sys_Error( "Draw_CacheGet: failed to load %s", path );

	return dat;
}

void *Draw_CustomCacheGet( cachewad_t *wad, void *raw, int rawsize, int index )
{
	cacheentry_t	*pic;
	void			*dat;
	char			*path;

	if ( index >= wad->cacheCount )
		Sys_Error( "Cache wad indexed before load %s: %d", wad->name, index );

	pic = &wad->cache[index];

	dat = Cache_Check( &pic->cache );
	if ( dat )
		return dat;

	{
		char		name[16];
		char		clean[16];

		path = pic->name;
		COM_FileBase( path, name );
		W_CleanupName( name, clean );

		if ( !Draw_CacheLoadFromCustom( clean, wad, raw, rawsize, pic ) )
			return NULL;
	}

	dat = pic->cache.data;
	if ( !dat )
		Sys_Error( "Draw_CacheGet: failed to load %s", path );

	return dat;
}

qboolean Draw_CacheReload( cachewad_t *wad, int i, lumpinfo_t *pLump, cacheentry_t *pic, char *clean, char *path )
{
	int				len;
	byte			*buf;
	FileHandle_t	hFile;

	if ( wad->numpaths > 0 )
	{
		const char	*pathID;

		pathID = wad->basedirs[ wad->lumppathindices[i] ];
		hFile = FS_OpenPathID( wad->name, "rb", pathID );
	}
	else
	{
		hFile = FS_Open( wad->name, "rb" );
	}

	if ( !hFile )
		return false;

	len = FS_Size( hFile );

	if ( wad->tempWad )
	{
		buf = Hunk_TempAlloc( pLump->size + 1 + wad->cacheExtra );
		pic->cache.data = buf;
	}
	else
	{
		buf = Cache_Alloc( &pic->cache, pLump->size + 1 + wad->cacheExtra, clean );
	}

	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", path, wad->name );

	buf[ pLump->size + wad->cacheExtra ] = 0;

	FS_Seek( hFile, pLump->filepos, FILESYSTEM_SEEK_HEAD );
	FS_Read( buf + wad->cacheExtra, pLump->size, 1, hFile );
	FS_Close( hFile );

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	return true;
}

qboolean Draw_ValidateCustomLogo( cachewad_t *wad, unsigned char *data, lumpinfo_t *lump )
{
	texture_t	tex;
	miptex_t	*mip, tmp;
	int			i, pix, paloffset, palettesize;
	int			size;

	if ( wad->cacheExtra != 32 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return false;
	}

	mip = (miptex_t *)( data + wad->cacheExtra );
	tmp = *mip;
	tex = *(texture_t *)data;

	Q_memcpy( tex.name, tmp.name, sizeof( tex.name ) );
	tex.width = LittleLong( tmp.width );
	tex.height = LittleLong( tmp.height );
	tex.anim_total = tex.anim_min = tex.anim_max = 0;
	tex.anim_next = tex.alternate_anims = NULL;

	for ( i = 0; i < MIPLEVELS; i++ )
		tex.offsets[i] = LittleLong( tmp.offsets[i] ) + wad->cacheExtra;

	pix = tex.width * tex.height;
	pix += ( pix >> 2 ) + ( pix >> 4 ) + ( pix >> 6 );
	paloffset = tex.offsets[0] + pix + 2;
	palettesize = *(unsigned short *)( (byte *)mip + pix + sizeof( miptex_t ) );

	if ( tex.width < 1 || tex.height < 1 || tex.width > 256 || tex.height > 256 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return false;
	}

	size = tex.width * tex.height;
	for ( i = 0; i < 3; i++ )
	{
		if ( tmp.offsets[i] + size != tmp.offsets[i + 1] )
		{
			Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
			return false;
		}
		size >>= 2;
	}

	if ( palettesize > 256 || palettesize < 0 )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad palette size %i on %s\n", palettesize, wad->name );
		return false;
	}

	if ( LittleLong( tmp.offsets[0] ) + pix + palettesize * 3 + sizeof( short ) > lump->disksize )
	{
		Con_Printf( "Draw_ValidateCustomLogo: Bad cached wad %s\n", wad->name );
		return false;
	}

	return true;
}

qboolean Draw_CacheLoadFromCustom( char *clean, cachewad_t *wad, void *raw, int rawsize, cacheentry_t *pic )
{
	int			idx;
	byte		*buf;
	lumpinfo_t	*pLump;
	byte		*pStart;
	byte		*pDest;

	pStart = (byte *)raw;

	if ( Q_strlen( clean ) < 5 )
	{
		idx = 0;
	}
	else
	{
		idx = Q_atoi( clean + 3 );
		if ( idx < 0 || idx >= wad->lumpCount )
			return false;
	}

	pLump = &wad->lumps[idx];

	buf = Cache_Alloc( &pic->cache, pLump->size + 1 + wad->cacheExtra, clean );
	if ( !buf )
		Sys_Error( "Draw_CacheGet: not enough space for %s in %s", clean, wad->name );

	buf[ pLump->size + wad->cacheExtra ] = 0;

	pDest = buf + wad->cacheExtra;
	Q_memcpy( pDest, pStart + pLump->filepos, pLump->size );

	if ( !Draw_ValidateCustomLogo( wad, buf, pLump ) )
		return false;

	gfCustomBuild = true;
	Q_strcpy( szCustName, "T" );
	Q_memcpy( &szCustName[1], clean, 5 );
	szCustName[6] = 0;

	if ( wad->pfnCacheBuild )
		wad->pfnCacheBuild( wad, buf );

	gfCustomBuild = false;
	return true;
}

int Draw_CacheIndex( cachewad_t *wad, char *path )
{
	cacheentry_t	*pic;
	int				i;

	for ( pic = wad->cache, i = 0; i < wad->cacheCount; pic++, i++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			break;
	}

	if ( i == wad->cacheCount )
	{
		if ( wad->cacheCount == wad->cacheMax )
			Sys_Error( "Cache wad (%s) out of %d entries", wad->name, wad->cacheMax );
		wad->cacheCount++;
		Q_strncpy( pic->name, path, sizeof( pic->name ) - 1 );
		pic->name[ sizeof( pic->name ) - 1 ] = 0;
	}

	return i;
}

int Draw_CacheFindIndex( cachewad_t *wad, char *path )
{
	cacheentry_t	*pic;
	int				i;

	pic = wad->cache;
	for ( i = 0; i < wad->cacheCount; pic++, i++ )
	{
		if ( !Q_strcmp( path, pic->name ) )
			return i;
	}

	return -1;
}
