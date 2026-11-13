// wad.c -- WAD3 file loading and lump lookup

#include "quakedef.h"
#include "wad.h"

#define	NUM_WADS	2

typedef struct
{
	qboolean	loaded;
	char		wadname[32];
	int			wad_numlumps;
	lumpinfo_t	*wad_lumps;
	byte		*wad_base;
} wadlist_t;

static wadlist_t	wads[NUM_WADS];

void	SwapPic( qpic_t *pic );

/*
==================
W_CleanupName

Lowercases name and pads with spaces and a terminating 0 to the length of
lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables.
Can safely be performed in place.
==================
*/
void W_CleanupName( char *in, char *out )
{
	int		i;
	int		c;

	for ( i = 0; i < 16; i++ )
	{
		c = in[i];
		if ( !c )
			break;

		if ( c >= 'A' && c <= 'Z' )
			c += ( 'a' - 'A' );
		out[i] = c;
	}

	for ( ; i < 16; i++ )
		out[i] = 0;
}

/*
====================
W_LoadWadFile
====================
*/
int W_LoadWadFile( char *filename )
{
	lumpinfo_t	*lump_p;
	wadinfo_t	*header;
	int			i;
	int			infotableofs;
	int			slot;
	wadlist_t	*wad;

	for ( slot = 0; slot < NUM_WADS; slot++ )
	{
		wad = &wads[slot];
		if ( !wad->loaded )
			break;
	}

	if ( slot >= NUM_WADS )
	{
		Con_Printf( "No room for wad %s\n", filename );
		return -1;
	}

	wad->wad_base = COM_LoadHunkFile( filename );
	if ( !wad->wad_base )
	{
		if ( slot != 0 )
		{
			Con_Printf( "WARNING:  W_LoadWadFile, couldn't load %s\n", filename );
			return -1;
		}

		Sys_Error( "W_LoadWadFile: couldn't load %s", filename );
	}

	Q_strncpy( wad->wadname, filename, sizeof( wad->wadname ) - 1 );
	wad->wadname[sizeof( wad->wadname ) - 1] = 0;
	wad->loaded = true;

	header = (wadinfo_t *)wad->wad_base;

	if ( header->identification[0] != 'W'
		|| header->identification[1] != 'A'
		|| header->identification[2] != 'D'
		|| header->identification[3] != '3' )
		Sys_Error( "Wad file %s doesn't have WAD3 id\n", filename );

	wad->wad_numlumps = LittleLong( header->numlumps );
	infotableofs = LittleLong( header->infotableofs );
	wad->wad_lumps = (lumpinfo_t *)( wad->wad_base + infotableofs );

	for ( i = 0, lump_p = wad->wad_lumps; i < wad->wad_numlumps; i++, lump_p++ )
	{
		lump_p->filepos = LittleLong( lump_p->filepos );
		lump_p->size = LittleLong( lump_p->size );
		W_CleanupName( lump_p->name, lump_p->name );
		if ( lump_p->type == TYP_QPIC )
			SwapPic( (qpic_t *)( wad->wad_base + lump_p->filepos ) );
	}

	return slot;
}

/*
=============
W_GetLumpinfo
=============
*/
lumpinfo_t *W_GetLumpinfo( int wad, char *name, qboolean doerror )
{
	int			i;
	lumpinfo_t	*lump_p;
	char		clean[16];

	W_CleanupName( name, clean );

	for ( lump_p = wads[wad].wad_lumps, i = 0; i < wads[wad].wad_numlumps; i++, lump_p++ )
	{
		if ( !Q_strcmp( clean, lump_p->name ) )
			return lump_p;
	}

	if ( doerror )
		Sys_Error( "W_GetLumpinfo: %s not found", name );

	return NULL;
}

/*
=============
W_GetLumpName
=============
*/
void *W_GetLumpName( int wad, char *name )
{
	lumpinfo_t	*lump;

	lump = W_GetLumpinfo( wad, name, true );

	return (void *)( wads[wad].wad_base + lump->filepos );
}

/*
=============
W_GetLumpNum
=============
*/
void *W_GetLumpNum( int wad, int num )
{
	lumpinfo_t	*lump;

	if ( num < 0 || num > wads[wad].wad_numlumps )
		Sys_Error( "W_GetLumpNum: bad number: %i", num );

	lump = wads[wad].wad_lumps + num;

	return (void *)( wads[wad].wad_base + lump->filepos );
}

/*
=============
W_Shutdown
=============
*/
void W_Shutdown( void )
{
	int			slot;
	wadlist_t	*wad;

	for ( slot = 0; slot < NUM_WADS; slot++ )
	{
		wad = &wads[slot];
		if ( !wad->loaded )
			break;

		memset( wad, 0, sizeof( *wad ) );
	}
}

/*
=============================================================================

automatic byte swapping

=============================================================================
*/

/*
=============
SwapPic
=============
*/
void SwapPic( qpic_t *pic )
{
	pic->width = LittleLong( pic->width );
	pic->height = LittleLong( pic->height );
}
