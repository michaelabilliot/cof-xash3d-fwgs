//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Builds the player's spray logo (tempdecal.wad) from a bitmap or a
//			targa file
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "UtlBuffer.h"
#include <FileSystem.h>
#include <vgui_controls/Controls.h>

typedef unsigned char byte;

#define TYP_LUMPY		64				// 64 + grab command number

typedef struct
{
	char		identification[4];		// should be WAD2 or 2DAW
	int			numlumps;
	int			infotableofs;
} wadinfo_t;

typedef struct
{
	int			filepos;
	int			disksize;
	int			size;					// uncompressed
	char		type;
	char		compression;
	char		pad1, pad2;
	char		name[16];				// must be null terminated
} lumpinfo_t;

typedef struct
{
	char		name[16];
	unsigned	width, height;
	unsigned	offsets[4];				// four mip maps stored
} miptex_t;

unsigned char	pixdata[256];

float			linearpalette[256][3];
float			d_red, d_green, d_blue;
int				colors_used;
int				color_used[256];
float			maxdistortion;
unsigned char	palLogo[768];

typedef struct _TargaHeader
{
	unsigned char	id_length, colormap_type, image_type;
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

	return (short)( b1 + b2 * 256 );
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
AveragePixels
=============
*/
unsigned char AveragePixels( int count )
{
	return pixdata[0];
}

/*
==============
GrabMip

filename MIP x y width height
must be multiples of sixteen
==============
*/
int GrabMip( HANDLE hdib, unsigned char *lump_p, char *lumpname, COLORREF crf, int *width, int *height )
{
	int				x, y, xl, yl, xh, yh, w, h;
	unsigned char	*screen_p, *source;
	miptex_t		*qtex;
	int				miplevel, mipstep;
	int				xx, yy;
	int				count;
	int				byteimagewidth, byteimageheight;
	unsigned char	*byteimage;
	LPBITMAPINFO	lpbmi;		// pointer to BITMAPINFO structure (Win3.0)

	lpbmi = (LPBITMAPINFO)GlobalLock( hdib );
	unsigned char *lump_start = lump_p;

	xl = yl = 0;
	w = lpbmi->bmiHeader.biWidth;
	h = lpbmi->bmiHeader.biHeight;

	*width = w;
	*height = h;

	byteimage = (unsigned char *)lpbmi + sizeof( BITMAPINFOHEADER ) + 256 * sizeof( RGBQUAD );

	if ( ( w & 15 ) || ( h & 15 ) )
		return 0;

	xh = xl + w;
	yh = yl + h;

	qtex = (miptex_t *)lump_p;
	qtex->width = w;
	qtex->height = h;
	strcpy( qtex->name, lumpname );

	lump_p = (byte *)&qtex->offsets[4];

	byteimagewidth = w;
	byteimageheight = h;

	source = lump_p;
	qtex->offsets[0] = lump_p - (byte *)qtex;

	// We're reading from a dib, so go bottom up
	screen_p = byteimage + ( h - 1 ) * w;

	for ( y = yl; y < yh; y++ )
	{
		for ( x = xl; x < xh; x++ )
			*lump_p++ = *screen_p++;

		screen_p -= 2 * w;
	}

	// calculate gamma corrected linear palette
	for ( int i = 0; i < 256; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			float f = palLogo[i * 3 + j] / 255.0;
			linearpalette[i][j] = f;	//pow((pal[i*3+j]/256), 2.2 );
			if ( f )
				f = 1.0;
		}
	}

	maxdistortion = 0;
	colors_used = 256;
	for ( i = 0; i < 256; i++ )
		color_used[i] = 1;

	//
	// subsample for greater mip levels
	//
	for ( miplevel = 1; miplevel < 4; miplevel++ )
	{
		d_red = d_green = d_blue = 0;	// no distortion yet
		qtex->offsets[miplevel] = lump_p - (byte *)qtex;

		mipstep = 1 << miplevel;

		for ( y = 0; y < h; y += mipstep )
		{
			for ( x = 0; x < w; x += mipstep )
			{
				count = 0;
				for ( yy = 0; yy < mipstep; yy++ )
				{
					for ( xx = 0; xx < mipstep; xx++ )
					{
						pixdata[count] = source[( y + yy ) * w + x + xx];
						count++;
					}
				}
				*lump_p++ = AveragePixels( count );
			}
		}
	}

	GlobalUnlock( lpbmi );

	// Write out palette in 16bit mode
	*(unsigned short *)lump_p = 256;	// palette size
	lump_p += sizeof( short );

	memcpy( lump_p, &palLogo[0], 765 );
	lump_p += 765;

	*lump_p++ = (unsigned char)( crf & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 8 ) & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 16 ) & 0xFF );

	return lump_p - lump_start;
}

bool UpdateLogoWAD( void *phdib, int r, int g, int b )
{
	char logoname[32];
	char *pszName;
	sprintf( logoname, "LOGO" );
	pszName = &logoname[0];

	HANDLE hdib = (HANDLE)phdib;
	COLORREF crf = RGB( r, g, b );

	if ( ( !pszName ) || ( pszName[0] == 0 ) || ( hdib == NULL ) )
		return false;

	// Generate lump
	unsigned char *buf = (unsigned char *)malloc( 0x5000 );
	if ( !buf )
		return false;

	memset( buf, 0, 0x5000 );

	CUtlBuffer buffer( 0, 0x5000 );

	int width, height;

	int length = GrabMip( hdib, buf, pszName, crf, &width, &height );
	if ( length == 0 )
		return false;

	bool sizevalid = false;

	if ( width == height )
	{
		if ( width == 16 ||
			 width == 32 ||
			 width == 64 )
		{
			sizevalid = true;
		}
	}

	if ( !sizevalid )
	{
		free( buf );
		return false;
	}

	while ( length & 3 )
		length++;

	// Write Header
	wadinfo_t header;
	header.identification[0] = 'W';
	header.identification[1] = 'A';
	header.identification[2] = 'D';
	header.identification[3] = '3';
	header.numlumps = 1;
	header.infotableofs = 0;

	buffer.Put( &header, sizeof( wadinfo_t ) );

	// Fill Ino info
	lumpinfo_t info;
	memset( &info, 0, sizeof( info ) );
	strcpy( info.name, pszName );
	info.filepos = (int)sizeof( wadinfo_t );
	info.size = info.disksize = length;
	info.type = TYP_LUMPY;
	info.compression = 0;

	// Write Lump
	buffer.Put( buf, length );

	// Write info
	buffer.Put( &info, sizeof( info ) );

	int savepos = buffer.TellPut();

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );

	header.infotableofs = length + sizeof( wadinfo_t );

	buffer.Put( &header, sizeof( wadinfo_t ) );

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, savepos );

	// Output to file
	FileHandle_t file;
	file = vgui2::filesystem()->Open( "tempdecal.wad", "wb" );
	if ( file != NULL )
	{
		vgui2::filesystem()->Write( buffer.Base(), buffer.TellPut(), file );
		vgui2::filesystem()->Close( file );
	}

	free( buf );

	return true;
}

int GrabMipFromTGA( unsigned char *byteimage, unsigned char *lump_p, char *lumpname, COLORREF crf, int width, int height )
{
	int				x, y, xl, yl, xh, yh, w, h;
	unsigned char	*source;
	miptex_t		*qtex;
	int				miplevel, mipstep;
	int				xx, yy;
	int				count;
	int				byteimagewidth, byteimageheight;
	unsigned char	*lump_start = lump_p;

	xl = yl = 0;
	w = width;
	h = height;

	if ( ( w & 15 ) || ( h & 15 ) )
		return 0;

	xh = xl + w;
	yh = yl + h;

	qtex = (miptex_t *)lump_p;
	qtex->width = w;
	qtex->height = h;
	strcpy( qtex->name, lumpname );

	lump_p = (byte *)&qtex->offsets[4];

	byteimagewidth = w;
	byteimageheight = h;

	source = lump_p;
	qtex->offsets[0] = lump_p - (byte *)qtex;

	unsigned char *screen_p = byteimage;

	for ( y = yl; y < yh; y++ )
	{
		for ( x = xl; x < xh; x++ )
		{
			// keep one channel, scaled by the alpha
			if ( GetRValue( crf ) )
				*lump_p = (unsigned char)( screen_p[0] * ( 255.0f / screen_p[3] ) );
			else if ( GetGValue( crf ) )
				*lump_p = (unsigned char)( screen_p[1] * ( 255.0f / screen_p[3] ) );
			else if ( GetBValue( crf ) )
				*lump_p = (unsigned char)( screen_p[2] * ( 255.0f / screen_p[3] ) );

			lump_p++;
			screen_p += 4;
		}
	}

	// calculate gamma corrected linear palette
	for ( int i = 0; i < 256; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			float f = palLogo[i * 3 + j] / 255.0;
			linearpalette[i][j] = f;	//pow((pal[i*3+j]/256), 2.2 );
			if ( f )
				f = 1.0;
		}
	}

	maxdistortion = 0;
	colors_used = 256;
	for ( i = 0; i < 256; i++ )
		color_used[i] = 1;

	//
	// subsample for greater mip levels
	//
	for ( miplevel = 1; miplevel < 4; miplevel++ )
	{
		d_red = d_green = d_blue = 0;	// no distortion yet
		qtex->offsets[miplevel] = lump_p - (byte *)qtex;

		mipstep = 1 << miplevel;

		for ( y = 0; y < h; y += mipstep )
		{
			for ( x = 0; x < w; x += mipstep )
			{
				count = 0;
				for ( yy = 0; yy < mipstep; yy++ )
				{
					for ( xx = 0; xx < mipstep; xx++ )
					{
						pixdata[count] = source[( y + yy ) * w + x + xx];
						count++;
					}
				}
				*lump_p++ = AveragePixels( count );
			}
		}
	}

	// Write out palette in 16bit mode
	*(unsigned short *)lump_p = 256;	// palette size
	lump_p += sizeof( short );

	memcpy( lump_p, &palLogo[0], 765 );
	lump_p += 765;

	*lump_p++ = (unsigned char)( crf & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 8 ) & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 16 ) & 0xFF );

	return lump_p - lump_start;
}

static bool LoadTGA( const char *szFilename, byte *buffer, int bufferSize, int *width, int *height, int errFail );

bool UpdateLogoWADFromTGA( const char *tga )
{
	char logoname[32];
	strcpy( logoname, "LOGO" );

	COLORREF r = RGB( 255, 0, 0 );
	COLORREF g = RGB( 0, 255, 0 );
	COLORREF b = RGB( 0, 0, 255 );

	CUtlBuffer sourceBuffer( 0, 0x5000 );
	int width, height;
	if ( !LoadTGA( tga, (byte *)sourceBuffer.Base(), sourceBuffer.Size(), &width, &height, 0 ) )
		return false;

	CUtlBuffer rBuffer( 0, 0x5000 );
	CUtlBuffer gBuffer( 0, 0x5000 );
	CUtlBuffer bBuffer( 0, 0x5000 );
	memset( rBuffer.Base(), 0, rBuffer.Size() );
	memset( gBuffer.Base(), 0, gBuffer.Size() );
	memset( bBuffer.Base(), 0, bBuffer.Size() );

	int length_r = GrabMipFromTGA( (byte *)sourceBuffer.Base(), (byte *)rBuffer.Base(), logoname, r, width, height );
	while ( length_r & 3 )
		length_r++;

	int length_g = GrabMipFromTGA( (byte *)sourceBuffer.Base(), (byte *)gBuffer.Base(), logoname, g, width, height );
	while ( length_g & 3 )
		length_g++;

	int length_b = GrabMipFromTGA( (byte *)sourceBuffer.Base(), (byte *)bBuffer.Base(), logoname, b, width, height );
	while ( length_b & 3 )
		length_b++;

	if ( !length_r || !length_g || !length_b )
		return false;

	if ( width != height )
		return false;

	if ( width != 16 && width != 32 && width != 64 )
		return false;

	// Write Header
	wadinfo_t header;
	header.identification[0] = 'W';
	header.identification[1] = 'A';
	header.identification[2] = 'D';
	header.identification[3] = '3';
	header.numlumps = 1;
	header.infotableofs = length_r + length_g + length_b + sizeof( wadinfo_t );

	CUtlBuffer buffer( 0, 0x5000 );
	buffer.Put( &header, sizeof( wadinfo_t ) );

	// Write Lumps
	buffer.Put( rBuffer.Base(), length_r );
	buffer.Put( rBuffer.Base(), length_g );
	buffer.Put( rBuffer.Base(), length_b );

	// Fill Ino info
	lumpinfo_t info;
	memset( &info, 0, sizeof( info ) );
	strcpy( info.name, logoname );
	info.filepos = (int)sizeof( wadinfo_t );
	info.size = info.disksize = length_r;
	info.type = TYP_LUMPY;
	info.compression = 0;

	// Write info
	buffer.Put( &info, sizeof( info ) );

	info.size = info.disksize = length_g;
	info.filepos = length_r + sizeof( wadinfo_t );

	info.size = info.disksize = length_b;
	info.filepos = length_r + length_g + sizeof( wadinfo_t );

	// Output to file
	FileHandle_t file;
	file = vgui2::filesystem()->Open( "tempdecal.wad", "wb" );
	if ( file != NULL )
	{
		vgui2::filesystem()->Write( buffer.Base(), buffer.TellPut(), file );
		vgui2::filesystem()->Close( file );
	}

	return true;
}

/*
=============
LoadTGA
=============
*/
static bool LoadTGA( const char *szFilename, byte *buffer, int bufferSize, int *width, int *height, int errFail )
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	byte	*targa_rgba;
	int		row, column;
	unsigned char *tgafile_mem_buffer;
	unsigned char *p;

	FileHandle_t f = vgui2::filesystem()->Open( szFilename, "rb" );
	int fileLen = vgui2::filesystem()->Size( f );
	if ( fileLen < 100 && fileLen > 0x5000 )
		return false;

	tgafile_mem_buffer = (unsigned char *)_alloca( fileLen );
	if ( !tgafile_mem_buffer )
		return false;

	vgui2::filesystem()->Read( tgafile_mem_buffer, fileLen, f );
	vgui2::filesystem()->Close( f );

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
		return false;

	if ( targa_header.colormap_type != 0 || ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) )
		return false;

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	if ( ( numPixels * ( targa_header.pixel_size == 24 ) ? 3 : 4 ) > bufferSize )
		return false;

	targa_rgba = buffer;

	if ( targa_header.id_length != 0 )
		p += targa_header.id_length;	// skip TARGA image comment

	if ( targa_header.image_type == 2 )		// Uncompressed, RGB images
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

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	return true;
}
