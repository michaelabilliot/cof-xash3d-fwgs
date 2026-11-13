// cmodel.c -- visibility decompression and the potentially audible set

#include "quakedef.h"
#include "mathlib.h"
#include "mem.h"
#include "const.h"
#include "bspfile.h"
#include "GL_MODEL.H"
#include "progs.h"
#include "eiface.h"
#include "delta.h"
#include "server.h"

static	byte	*gPAS;
static	byte	*gPVS;
static	int		gPVSRowBytes;

static	byte	mod_novis[MAX_MAP_LEAFS / 8];

void	CM_DecompressPVS( byte *in, byte *decompressed, int byteCount );
byte	*CM_LeafPVS( int leafnum );

/*
===============
Mod_Init
===============
*/
void Mod_Init( void )
{
	Q_memset( mod_novis, 0xff, sizeof( mod_novis ) );
}

/*
===================
Mod_DecompressVis
===================
*/
byte *Mod_DecompressVis( byte *in, model_t *model )
{
	static byte	decompressed[MAX_MAP_LEAFS / 8];
	int			row;

	row = ( model->numleafs + 7 ) >> 3;

	if ( !in )
		return mod_novis;

	CM_DecompressPVS( in, decompressed, row );

	return decompressed;
}

/*
===============
Mod_LeafPVS
===============
*/
byte *Mod_LeafPVS( mleaf_t *leaf, model_t *model )
{
	if ( leaf == model->leafs )
		return mod_novis;

	if ( gPVS )
	{
		int		leafnum;

		leafnum = leaf - model->leafs;
		return CM_LeafPVS( leafnum );
	}

	return Mod_DecompressVis( leaf->compressed_vis, model );
}

/*
===================
CM_DecompressPVS
===================
*/
void CM_DecompressPVS( byte *in, byte *decompressed, int byteCount )
{
	int		c;
	byte	*out;

	if ( !in )
	{
		// no vis info, so make all visible
		Q_memcpy( decompressed, mod_novis, byteCount );
		return;
	}

	out = decompressed;
	decompressed += byteCount;

	do
	{
		if ( *in )
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while ( c )
		{
			*out++ = 0;
			c--;
		}
	} while ( out < decompressed );
}

/*
===================
CM_LeafPVS
===================
*/
byte *CM_LeafPVS( int leafnum )
{
	if ( !gPVS )
		return mod_novis;

	return gPVS + leafnum * gPVSRowBytes;
}

/*
===================
CM_LeafPAS
===================
*/
byte *CM_LeafPAS( int leafnum )
{
	if ( !gPAS )
		return mod_novis;

	return gPAS + leafnum * gPVSRowBytes;
}

/*
===================
CM_FreePAS
===================
*/
void CM_FreePAS( void )
{
	if ( gPAS )
		Mem_Free( gPAS );

	if ( gPVS )
		Mem_Free( gPVS );

	gPAS = NULL;
	gPVS = NULL;
}

/*
===================
CM_CalcPAS
===================
*/
void CM_CalcPAS( model_t *pModel )
{
	int				rowwords, actualRowBytes;
	int				i, j, k, l, index, num;
	int				bitbyte;
	unsigned int	*dest, *src;
	byte			*scan;
	int				count, vcount;

	Con_DPrintf( "Building PAS...\n" );
	CM_FreePAS();

	// Calculate memory: matrix of each to each leaf visibility
	num = pModel->numleafs + 1;
	actualRowBytes = ( pModel->numleafs + 7 ) >> 3;
	gPVSRowBytes = ( actualRowBytes + 3 ) & ~3;
	rowwords = gPVSRowBytes >> 2;

	gPVS = Mem_Calloc( gPVSRowBytes, num );

	// Decompress the visibility data
	scan = gPVS;
	vcount = 0;
	for ( i = 0; i < num; i++, scan += gPVSRowBytes )
	{
		CM_DecompressPVS( pModel->leafs[i].compressed_vis, scan, actualRowBytes );

		if ( i == 0 )
			continue;

		for ( j = 0; j < num; j++ )
		{
			if ( scan[j >> 3] & ( 1 << ( j & 7 ) ) )
				vcount++;
		}
	}

	gPAS = Mem_Calloc( gPVSRowBytes, num );

	// Build the PAS from the PVS
	count = 0;
	scan = gPVS;
	dest = (unsigned int *)gPAS;
	for ( i = 0; i < num; i++, dest += rowwords, scan += gPVSRowBytes )
	{
		Q_memcpy( dest, scan, gPVSRowBytes );

		for ( j = 0; j < gPVSRowBytes; j++ )
		{
			bitbyte = scan[j];
			if ( !bitbyte )
				continue;

			for ( k = 0; k < 8; k++ )
			{
				if ( !( bitbyte & ( 1 << k ) ) )
					continue;

				// OR this pvs row into the phs
				index = j * 8 + k + 1;
				if ( index >= num || index <= 0 )
					continue;

				src = (unsigned int *)( gPVS + index * gPVSRowBytes );
				for ( l = 0; l < rowwords; l++ )
					dest[l] |= src[l];
			}
		}

		if ( i == 0 )
			continue;

		for ( j = 0; j < num; j++ )
		{
			if ( ( (byte *)dest )[j >> 3] & ( 1 << ( j & 7 ) ) )
				count++;
		}
	}

	Con_DPrintf( "Average leaves visible / audible / total: %i / %i / %i\n", vcount / num, count / num, num );
}

/*
===================
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
===================
*/
qboolean CM_HeadnodeVisible( mnode_t *node, byte *visbits, int *first_visible_leafnum )
{
	int		leafnum;
	mleaf_t	*leaf;

	if ( !node )
		return false;

	if ( node->contents == CONTENTS_SOLID )
		return false;

	if ( node->contents < 0 )
	{
		leaf = (mleaf_t *)node;
		leafnum = ( leaf - sv.worldmodel->leafs ) - 1;

		if ( visbits[leafnum >> 3] & ( 1 << ( leafnum & 7 ) ) )
		{
			if ( first_visible_leafnum )
				*first_visible_leafnum = leafnum;
			return true;
		}

		return false;
	}

	if ( CM_HeadnodeVisible( node->children[0], visbits, first_visible_leafnum ) )
		return true;

	return CM_HeadnodeVisible( node->children[1], visbits, first_visible_leafnum );
}
