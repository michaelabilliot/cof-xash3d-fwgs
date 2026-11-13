// gl_warp.c -- sky and water polygons

#include "quakedef.h"
#include "mathlib.h"
#include "cvar.h"
#include "GL_MODEL.H"
#include "client.h"
#include "vid.h"
#include "render.h"
#include "GLQUAKE.H"
#include "gl_cvars.h"
#include "gl_water.h"
#include "cshift.h"
#include "view.h"
#include "mem.h"
#include "../pm_shared/pm_movevars.h"

#include <math.h>

cshift_t	cshift_water;

extern	model_t		*loadmodel;
extern	movevars_t	movevars;
extern	cvar_t		gl_palette_tex;
extern	cvar_t		gl_dither;

void	GL_Bind( int texnum );
void	GL_PaletteClearSky( void );
short	GL_PaletteAdd( unsigned char *pPal, qboolean isSky );
void	LoadBMP8( FileHandle_t hFile, byte **pPalette, int *nPalette, byte **pImage, int *nWidth, int *nHeight );
void	VideoMode_GetCurrentVideoMode( int *wide, int *tall, int *bpp );
int		CL_IsDevOverviewMode( void );

extern	qboolean	g_bFogSkybox;

#define	ON_EPSILON	0.1		// point on plane side epsilon

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky
colorVec	gWaterColor;

msurface_t	*warpface;

#define	SUBDIVIDE_SIZE	64

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

void BoundPoly( int numverts, float *verts, vec3_t mins, vec3_t maxs )
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for ( i = 0; i < numverts; i++ )
		for ( j = 0; j < 3; j++, v++ )
		{
			if ( *v < mins[j] )
				mins[j] = *v;
			if ( *v > maxs[j] )
				maxs[j] = *v;
		}
}

void SubdividePolygon( int numverts, float *verts )
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if ( numverts > 60 )
		Sys_Error( "numverts = %i", numverts );

	BoundPoly( numverts, verts, mins, maxs );

	for ( i = 0; i < 3; i++ )
	{
		m = ( mins[i] + maxs[i] ) * 0.5;
		m = SUBDIVIDE_SIZE * floor( m / SUBDIVIDE_SIZE + 0.5 );
		if ( maxs[i] - m < 8 )
			continue;
		if ( m - mins[i] < 8 )
			continue;

		// cut it
		v = verts + i;
		for ( j = 0; j < numverts; j++, v += 3 )
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy( verts, v );

		f = b = 0;
		v = verts;
		for ( j = 0; j < numverts; j++, v += 3 )
		{
			if ( dist[j] >= 0 )
			{
				VectorCopy( v, front[f] );
				f++;
			}
			if ( dist[j] <= 0 )
			{
				VectorCopy( v, back[b] );
				b++;
			}
			if ( dist[j] == 0 || dist[j+1] == 0 )
				continue;
			if ( ( dist[j] > 0 ) != ( dist[j+1] > 0 ) )
			{
				// clip point
				frac = dist[j] / ( dist[j] - dist[j+1] );
				for ( k = 0; k < 3; k++ )
					front[f][k] = back[b][k] = v[k] + frac * ( v[3+k] - v[k] );
				f++;
				b++;
			}
		}

		SubdividePolygon( f, front[0] );
		SubdividePolygon( b, back[0] );
		return;
	}

	poly = Hunk_Alloc( sizeof( glpoly_t ) + ( numverts - 4 ) * VERTEXSIZE * sizeof( float ) );
	poly->next = warpface->polys;
	poly->flags = warpface->flags;
	warpface->polys = poly;
	poly->numverts = numverts;
	for ( i = 0; i < numverts; i++, verts += 3 )
	{
		VectorCopy( verts, poly->verts[i] );
		s = DotProduct( verts, warpface->texinfo->vecs[0] );
		t = DotProduct( verts, warpface->texinfo->vecs[1] );
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct( verts, warpface->texinfo->vecs[0] ) + warpface->texinfo->vecs[0][3];
		s -= warpface->texturemins[0];
		s += warpface->light_s * 128 * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16;

		t = DotProduct( verts, warpface->texinfo->vecs[1] ) + warpface->texinfo->vecs[1][3];
		t -= warpface->texturemins[1];
		t += warpface->light_t * 128 * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface( msurface_t *fa )
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for ( i = 0; i < fa->numedges; i++ )
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if ( lindex > 0 )
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy( vec, verts[numverts] );
		numverts++;
	}

	SubdividePolygon( numverts, verts[0] );
}

//=========================================================



// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE ( 256.0 / ( 2 * M_PI ) )

void D_SetFadeColor( int r, int g, int b, int fog )
{
	gWaterColor.r = r;
	gWaterColor.g = g;
	gWaterColor.b = b;
	cshift_water.destcolor[0] = r;
	cshift_water.destcolor[1] = g;
	cshift_water.destcolor[2] = b;
	cshift_water.percent = fog;
}

void EmitWireFrameWaterPolys( msurface_t *fa, int direction )
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, scale;
	float		tempVert[3];

	if ( gl_wireframe.value == 0 )
		return;

	if ( fa->polys->verts[0][2] < r_refdef.vieworg[2] )
		scale = currententity->curstate.scale;
	else
		scale = -currententity->curstate.scale;

	if ( gl_wireframe.value == 2 )
		qglDisable( GL_DEPTH_TEST );

	qglColor3f( 1, 1, 1 );

	for ( p = fa->polys; p; p = p->next )
	{
		qglBegin( GL_LINE_LOOP );

		if ( direction )
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			tempVert[0] = v[0];
			tempVert[1] = v[1];

			s = turbsin[(int)( cl.time * 160.0 + tempVert[0] + tempVert[1] ) & 255] + 8;
			s += ( turbsin[(int)( cl.time * 171.0 + tempVert[0] * 5 - tempVert[1] ) & 255] + 8 ) * 0.8;
			tempVert[2] = s * scale + v[2];

			qglVertex3fv( tempVert );

			if ( direction )
				v -= VERTEXSIZE * 2;
		}

		qglEnd();
	}

	if ( gl_wireframe.value == 2 )
		qglEnable( GL_DEPTH_TEST );
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys( msurface_t *fa, int direction )
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;
	float		scale;
	float		tempVert[3];
	unsigned char	*pSourcePalette;

	pSourcePalette = fa->texinfo->texture->pPal;
	D_SetFadeColor( pSourcePalette[9], pSourcePalette[10], pSourcePalette[11], pSourcePalette[12] );

	if ( fa->polys->verts[0][2] < r_refdef.vieworg[2] )
		scale = currententity->curstate.scale;
	else
		scale = -currententity->curstate.scale;

	for ( p = fa->polys; p; p = p->next )
	{
		qglBegin( GL_POLYGON );

		if ( direction )
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		for ( i = 0; i < p->numverts; i++, v += VERTEXSIZE )
		{
			os = v[3];
			ot = v[4];

			tempVert[0] = v[0];
			tempVert[1] = v[1];

			s = turbsin[(int)( cl.time * 160.0 + tempVert[0] + tempVert[1] ) & 255] + 8;
			s += ( turbsin[(int)( cl.time * 171.0 + tempVert[0] * 5 - tempVert[1] ) & 255] + 8 ) * 0.8;
			tempVert[2] = s * scale + v[2];

			s = os + turbsin[(int)( ( ot * 0.125 + cl.time ) * TURBSCALE ) & 255];
			s *= ( 1.0 / 64 );

			t = ot + turbsin[(int)( ( os * 0.125 + cl.time ) * TURBSCALE ) & 255];
			t *= ( 1.0 / 64 );

			qglTexCoord2f( s, t );
			qglVertex3fv( tempVert );

			if ( direction )
				v -= VERTEXSIZE * 2;
		}

		qglEnd();
	}

	EmitWireFrameWaterPolys( fa, direction );
}

/*
=============================================================

	TARGA LOADING

=============================================================
*/

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


TargaHeader		targa_header;

int fgetLittleShort( unsigned char **p )
{
	byte	b1, b2;

	b1 = *((*p)++);
	b2 = *((*p)++);

	return (short)( b1 + ( b2 << 8 ) );
}

int fgetLittleLong( unsigned char **p )
{
	byte	b1, b2, b3, b4;

	b1 = *((*p)++);
	b2 = *((*p)++);
	b3 = *((*p)++);
	b4 = *((*p)++);

	return b1 + ( b2 << 8 ) + ( b3 << 16 ) + ( b4 << 24 );
}


/*
=============
LoadTGA2
=============
*/
qboolean LoadTGA2( char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height, int errFail )
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	byte	*targa_rgba;
	int		row, column;
	byte	*tgafile_mem_buffer;
	byte	*p;
	int		fileLen;

	tgafile_mem_buffer = COM_LoadFile( szFilename, 5, &fileLen );
	if ( !tgafile_mem_buffer )
		return false;

	p = tgafile_mem_buffer;

	if ( width )
		*width = 0;
	if ( height )
		*height = 0;

	targa_header.id_length = *p++;
	targa_header.colormap_type = *p++;
	targa_header.image_type = *p++;

	targa_header.colormap_index = fgetLittleShort( &p );
	targa_header.colormap_length = fgetLittleShort( &p );
	targa_header.colormap_size = *p++;
	targa_header.x_origin = fgetLittleShort( &p );
	targa_header.y_origin = fgetLittleShort( &p );
	targa_header.width = fgetLittleShort( &p );
	targa_header.height = fgetLittleShort( &p );
	targa_header.pixel_size = *p++;
	targa_header.attributes = *p++;

	if ( targa_header.image_type != 2 && targa_header.image_type != 10 )
	{
		if ( errFail )
		{
			Sys_Error( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
		}
		else
		{
			COM_FreeFile( tgafile_mem_buffer );
			Con_Printf( "LoadTGA: Only type 2 and 10 targa RGB images supported\n" );
			return false;
		}
	}

	if ( targa_header.colormap_type != 0 || ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) )
	{
		if ( errFail )
		{
			Sys_Error( "Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n" );
		}
		else
		{
			COM_FreeFile( tgafile_mem_buffer );
			Con_Printf( "Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n" );
			return false;
		}
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	if ( ( numPixels * ( targa_header.pixel_size == 24 ) ? 3 : 4 ) > bufferSize )
	{
		if ( errFail )
		{
			Sys_Error( "TGA texture has the wrong size!\n" );
		}
		else
		{
			COM_FreeFile( tgafile_mem_buffer );
			Con_Printf( "TGA texture has the wrong size!\n" );
			return false;
		}
	}

	targa_rgba = buffer;

	if ( targa_header.id_length != 0 )
		p += targa_header.id_length;	// skip TARGA image comment

	if ( targa_header.image_type == 2 )	// Uncompressed, RGB images
	{
		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for ( column = 0; column < columns; column++ )
			{
				unsigned char red, green, blue, alphabyte;
				switch ( targa_header.pixel_size )
				{
				case 24:
					blue = *p++;
					green = *p++;
					red = *p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *p++;
					green = *p++;
					red = *p++;
					alphabyte = *p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if ( targa_header.image_type == 10 )	// Runlength encoded RGB images
	{
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;
		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for ( column = 0; column < columns; )
			{
				packetHeader = *p++;
				packetSize = 1 + ( packetHeader & 0x7f );
				if ( packetHeader & 0x80 )	// run-length packet
				{
					switch ( targa_header.pixel_size )
					{
					case 24:
						blue = *p++;
						green = *p++;
						red = *p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *p++;
						green = *p++;
						red = *p++;
						alphabyte = *p++;
						break;
					}

					for ( j = 0; j < packetSize; j++ )
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if ( column == columns )	// run spans across rows
						{
							column = 0;
							if ( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else	// non run-length packet
				{
					for ( j = 0; j < packetSize; j++ )
					{
						switch ( targa_header.pixel_size )
						{
						case 24:
							blue = *p++;
							green = *p++;
							red = *p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *p++;
							green = *p++;
							red = *p++;
							alphabyte = *p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}
						column++;
						if ( column == columns )	// pixel packet run spans across rows
						{
							column = 0;
							if ( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
breakOut:;
		}
	}

	COM_FreeFile( tgafile_mem_buffer );

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	return true;
}

qboolean LoadTGA( char *szFilename, unsigned char *buffer, int bufferSize, int *width, int *height )
{
	return LoadTGA2( szFilename, buffer, bufferSize, width, height, true );
}

char	*suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

int		gLoadSky, gSkyTexNumber[6];

void R_LoadSkys( void )
{
	int		i, j, skipBMP;
	char	name[64];
	byte	*buffer;
	qboolean	printed;

	printed = false;

	if ( !gLoadSky )
	{
		for ( i = 0; i < 6; i++ )
		{
			j = i + 5800;
			qglDeleteTextures( 1, &j );
		}
		return;
	}

	if ( qglColorTableEXT && gl_palette_tex.value )
	{
		skipBMP = 0;
		GL_PaletteClearSky();
	}
	else
	{
		skipBMP = 1;
	}

	buffer = Mem_Malloc( 256 * 256 * 4 );

restart:
	for ( i = 0; i < 6; i++ )
	{
		int		loadedBMP, palCount, bmpWidth, bmpHeight, paletteIndex;
		byte	*packPalette, *pImage;
		FileHandle_t	hFile;

		loadedBMP = 0;

		_snprintf( name, sizeof( name ), "gfx/env/%s%s.bmp", movevars.skyName, suf[i] );
		if ( !skipBMP && ( hFile = FS_Open( name, "rb" ) ) != NULL )
		{
			LoadBMP8( hFile, &packPalette, &palCount, &pImage, &bmpWidth, &bmpHeight );
			if ( bmpWidth * bmpHeight > 0 )
			{
				paletteIndex = GL_PaletteAdd( packPalette, true );
				if ( paletteIndex >= 0 )
				{
					GL_Bind( i + 5800 );
					qglTexImage2D( GL_TEXTURE_2D, 0, GL_COLOR_INDEX8_EXT, 256, 256, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, pImage );
					gSkyTexNumber[i] = ( ( paletteIndex + 1 ) << 16 ) + i + 5800;
					loadedBMP = 1;
				}
				Mem_Free( pImage );
				Mem_Free( packPalette );
			}
		}

		if ( !loadedBMP )
		{
			skipBMP = 1;

			_snprintf( name, sizeof( name ), "gfx/env/%s%s.tga", movevars.skyName, suf[i] );
			if ( !LoadTGA( name, buffer, 256 * 256 * 4, NULL, NULL ) )
			{
				Con_Printf( "R_LoadSkys: Couldn't load %s\n", name );
				if ( i == 0 && Q_strcmp( movevars.skyName, "black" ) )
				{
					Q_strcpy( movevars.skyName, "black" );
					goto restart;
				}
				continue;
			}

			if ( gl_dither.value )
			{
				for ( j = 0; j < 256 * 256 * 4; j += 4 )
				{
					if ( buffer[j] < 252 )
					{
						buffer[j] |= ( buffer[j] & 0xC0 ) >> 6;
						buffer[j] = texgammatable[buffer[j]];
					}
					if ( buffer[j+1] < 252 )
					{
						buffer[j+1] |= ( buffer[j+1] & 0xC0 ) >> 6;
						buffer[j+1] = texgammatable[buffer[j+1]];
					}
					if ( buffer[j+2] < 252 )
					{
						buffer[j+2] |= ( buffer[j+2] & 0xC0 ) >> 6;
						buffer[j+2] = texgammatable[buffer[j+2]];
					}
				}
			}

			GL_Bind( i + 5800 );
			{
				int		w, h, bpp;

				VideoMode_GetCurrentVideoMode( &w, &h, &bpp );
				if ( bpp == 32 )
					qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
				else
					qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
			}
			gSkyTexNumber[i] = i + 5800;
		}

		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		if ( !printed )
		{
			printed = true;
			Con_DPrintf( "SKY:  " );
		}

		if ( loadedBMP )
			Con_DPrintf( "%s%s - paletted, ", movevars.skyName, suf[i] );
		else
			Con_DPrintf( "%s%s, ", movevars.skyName, suf[i] );
	}

	if ( printed )
		Con_DPrintf( "done\n" );

	Mem_Free( buffer );
	gLoadSky = 0;
}

vec3_t	skyclip[6] = {
	{ 1, 1, 0 },
	{ 1, -1, 0 },
	{ 0, -1, 1 },
	{ 0, 1, 1 },
	{ 1, 0, 1 },
	{ -1, 0, 1 }
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
int	st_to_vec[6][3] =
{
	{ 3, -1, 2 },
	{ -3, 1, 2 },

	{ 1, 3, 2 },
	{ -1, -3, 2 },

	{ -2, -1, 3 },		// 0 degrees yaw, look straight up
	{ 2, -1, -3 }		// look straight down

//	{ -1, 2, 3 },
//	{ 1, 2, -3 }
};

// s = [0]/[2], t = [1]/[2]
int	vec_to_st[6][3] =
{
	{ -2, 3, 1 },
	{ 2, 3, -1 },

	{ 1, 3, 2 },
	{ -1, 3, -2 },

	{ -2, -1, 3 },
	{ -2, 1, -3 }

//	{ -1, 2, 3 },
//	{ 1, 2, -3 }
};

float	skymins[2][6], skymaxs[2][6];

void DrawSkyPolygon( int nump, vec3_t vecs )
{
	int		i, j;
	vec3_t	v, av;
	float	s, t, dv;
	int		axis;
	float	*vp;

	c_sky++;

	// decide which face it maps to
	VectorCopy( vec3_origin, v );
	for ( i = 0, vp = vecs; i < nump; i++, vp += 3 )
	{
		VectorAdd( v, vp, v );
	}
	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );
	if ( av[0] > av[1] && av[0] > av[2] )
	{
		if ( v[0] < 0 )
			axis = 1;
		else
			axis = 0;
	}
	else if ( av[1] > av[2] && av[1] > av[0] )
	{
		if ( v[1] < 0 )
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if ( v[2] < 0 )
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for ( i = 0; i < nump; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		if ( j > 0 )
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		j = vec_to_st[axis][0];
		if ( j < 0 )
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;
		j = vec_to_st[axis][1];
		if ( j < 0 )
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if ( s < skymins[0][axis] )
			skymins[0][axis] = s;
		if ( t < skymins[1][axis] )
			skymins[1][axis] = t;
		if ( s > skymaxs[0][axis] )
			skymaxs[0][axis] = s;
		if ( t > skymaxs[1][axis] )
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
void ClipSkyPolygon( int nump, vec3_t vecs, int stage )
{
	float		*norm;
	float		*v;
	qboolean	front, back;
	float		d, e;
	float		dists[MAX_CLIP_VERTS];
	int			sides[MAX_CLIP_VERTS];
	vec3_t		newv[2][MAX_CLIP_VERTS];
	int			newc[2];
	int			i, j;

	if ( nump > MAX_CLIP_VERTS - 2 )
		Sys_Error( "ClipSkyPolygon: MAX_CLIP_VERTS" );
	if ( stage == 6 )
	{	// fully clipped, so draw it
		DrawSkyPolygon( nump, vecs );
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if ( d > ON_EPSILON )
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if ( d < ON_EPSILON )
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if ( !front || !back )
	{	// not clipped
		ClipSkyPolygon( nump, vecs, stage + 1 );
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, ( vecs + ( i * 3 ) ) );
	newc[0] = newc[1] = 0;

	for ( i = 0, v = vecs; i < nump; i++, v += 3 )
	{
		switch ( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if ( sides[i] == SIDE_ON || sides[i+1] == SIDE_ON || sides[i+1] == sides[i] )
			continue;

		d = dists[i] / ( dists[i] - dists[i+1] );
		for ( j = 0; j < 3; j++ )
		{
			e = v[j] + d * ( v[j+3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain( msurface_t *s )
{
	msurface_t	*fa;

	int		i;
	vec3_t	verts[MAX_CLIP_VERTS];
	glpoly_t	*p;

	if ( CL_IsDevOverviewMode() )
		return;

	c_sky = 0;

	// calculate vertex values for sky box
	for ( fa = s; fa; fa = fa->texturechain )
	{
		for ( p = fa->polys; p; p = p->next )
		{
			for ( i = 0; i < p->numverts; i++ )
			{
				VectorSubtract( p->verts[i], r_origin, verts[i] );
			}
			ClipSkyPolygon( p->numverts, verts[0], 0 );
		}
	}

	R_DrawSkyBox();
}

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox( void )
{
	int		i;

	for ( i = 0; i < 6; i++ )
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

void MakeSkyVec( float s, float t, int axis )
{
	vec3_t		v, b;
	int			j, k;
	float		width;

	width = movevars.zmax * 0.57735f;

	if ( s < -1 )
		s = -1;
	else if ( s > 1 )
		s = 1;
	if ( t < -1 )
		t = -1;
	else if ( t > 1 )
		t = 1;

	b[0] = s * width;
	b[1] = t * width;
	b[2] = width;

	for ( j = 0; j < 3; j++ )
	{
		k = st_to_vec[axis][j];
		if ( k < 0 )
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = ( s + 1 ) * 0.5;
	t = ( t + 1 ) * 0.5;

	if ( s < 1.0 / 512 )
		s = 1.0 / 512;
	else if ( s > 511.0 / 512 )
		s = 511.0 / 512;
	if ( t < 1.0 / 512 )
		t = 1.0 / 512;
	else if ( t > 511.0 / 512 )
		t = 511.0 / 512;

	t = 1.0 - t;
	qglTexCoord2f( s, t );
	qglVertex3fv( v );
}

/*
==============
R_DrawSkyBox
==============
*/
int	skytexorder[6] = { 0, 2, 1, 3, 4, 5 };
int	skyfacenormal_guess[6] = { 1, -1, 2, -2, 3, -3 };

void R_DrawSkyBox( void )
{
	int		i, iFogTurnedOff;
	vec3_t	normal;
	float	r, g, b;

	iFogTurnedOff = 0;

	r = 1.0;
	g = 1.0;
	b = 1.0;

	if ( filterMode )
	{
		r = filterColorRed * filterBrightness;
		g = filterColorGreen * filterBrightness;
		b = filterColorBlue * filterBrightness;
	}

	GL_DisableMultitexture();

	if ( !g_bFogSkybox && qglIsEnabled( GL_FOG ) == GL_TRUE )
	{
		iFogTurnedOff = 1;
		qglDisable( GL_FOG );
	}

	for ( i = 0; i < 6; i++ )
	{
		if ( skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i] )
			continue;

		VectorClear( normal );
		switch ( skyfacenormal_guess[i] )
		{
		case 1:
			normal[0] = 1;
			break;
		case -1:
			normal[0] = -1;
			break;
		case 2:
			normal[1] = 1;
			break;
		case -2:
			normal[1] = -1;
			break;
		case 3:
			normal[2] = 1;
			break;
		case -3:
			normal[2] = -1;
			break;
		}

		// the face points away from the view
		if ( DotProduct( vpn, normal ) < -( 1 - 0.70710678 ) )
			continue;

		GL_Bind( gSkyTexNumber[skytexorder[i]] );
		qglColor3f( r, g, b );

		qglBegin( GL_QUADS );
		MakeSkyVec( skymins[0][i], skymins[1][i], i );
		MakeSkyVec( skymins[0][i], skymaxs[1][i], i );
		MakeSkyVec( skymaxs[0][i], skymaxs[1][i], i );
		MakeSkyVec( skymaxs[0][i], skymins[1][i], i );
		qglEnd();
	}

	if ( iFogTurnedOff )
		qglEnable( GL_FOG );
}

void R_InitSky( void )
{
	gLoadSky = true;
}
