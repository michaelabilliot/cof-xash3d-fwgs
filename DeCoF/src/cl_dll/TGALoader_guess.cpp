// Cry of Fear TGA loader (the loader of the NeHe lesson 33 tutorial by
// Evan Pipho, in a class, printing to the console). Nothing in the client
// calls it.

#include <windows.h>
#include <gl/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "TGALoader_guess.h"

bool CTGALoader_guess::LoadTGA_guess( Texture_guess *texture, char *filename )
{
	FILE *fTGA;
	fTGA = fopen( filename, "rb" );

	if ( fTGA == NULL )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not open texture file\n" );
		return false;
	}

	if ( fread( &tgaheader_guess, sizeof( TGAHeader_guess ), 1, fTGA ) == 0 )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not read file header\n" );
		if ( fTGA != NULL )
			fclose( fTGA );
		return false;
	}

	if ( memcmp( uTGAcompare_guess, &tgaheader_guess, sizeof( tgaheader_guess ) ) == 0 )
	{
		LoadUncompressedTGA_guess( texture, filename, fTGA );
	}
	else if ( memcmp( cTGAcompare_guess, &tgaheader_guess, sizeof( tgaheader_guess ) ) == 0 )
	{
		LoadCompressedTGA_guess( texture, filename, fTGA );
	}
	else
	{
		gEngfuncs.Con_Printf( "TGA LOAD: TGA file must be type 2 or type 10\n" );
		fclose( fTGA );
		return false;
	}

	return true;
}

bool CTGALoader_guess::LoadUncompressedTGA_guess( Texture_guess *texture, char *filename, FILE *fTGA )
{
	if ( fread( tga_guess.header_guess, sizeof( tga_guess.header_guess ), 1, fTGA ) == 0 )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not read info header\n" );
		if ( fTGA != NULL )
			fclose( fTGA );
		return false;
	}

	texture->width_guess	= tga_guess.header_guess[1] * 256 + tga_guess.header_guess[0];
	texture->height_guess	= tga_guess.header_guess[3] * 256 + tga_guess.header_guess[2];
	texture->bpp_guess		= tga_guess.header_guess[4];
	tga_guess.Width_guess	= texture->width_guess;
	tga_guess.Height_guess	= texture->height_guess;
	tga_guess.Bpp_guess		= texture->bpp_guess;

	if ( ( texture->width_guess <= 0 ) || ( texture->height_guess <= 0 ) || ( ( texture->bpp_guess != 24 ) && ( texture->bpp_guess != 32 ) ) )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Invalid texture information\n" );
		if ( fTGA != NULL )
			fclose( fTGA );
		return false;
	}

	if ( texture->bpp_guess == 24 )
		texture->type_guess = GL_RGB;
	else
		texture->type_guess = GL_RGBA;

	tga_guess.bytesPerPixel_guess	= ( tga_guess.Bpp_guess / 8 );
	tga_guess.imageSize_guess		= ( tga_guess.bytesPerPixel_guess * tga_guess.Width_guess * tga_guess.Height_guess );
	texture->imageData_guess		= (GLubyte *)malloc( tga_guess.imageSize_guess );

	if ( texture->imageData_guess == NULL )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not allocate memory for image\n" );
		fclose( fTGA );
		return false;
	}

	if ( fread( texture->imageData_guess, 1, tga_guess.imageSize_guess, fTGA ) != tga_guess.imageSize_guess )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not read image data\n" );
		if ( texture->imageData_guess != NULL )
			free( texture->imageData_guess );
		fclose( fTGA );
		return false;
	}

	for ( GLuint cswap = 0; cswap < (int)tga_guess.imageSize_guess; cswap += tga_guess.bytesPerPixel_guess )
	{
		texture->imageData_guess[cswap] ^= texture->imageData_guess[cswap + 2] ^=
		texture->imageData_guess[cswap] ^= texture->imageData_guess[cswap + 2];
	}

	fclose( fTGA );
	return true;
}

bool CTGALoader_guess::LoadCompressedTGA_guess( Texture_guess *texture, char *filename, FILE *fTGA )
{
	if ( fread( tga_guess.header_guess, sizeof( tga_guess.header_guess ), 1, fTGA ) == 0 )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not read info header\n" );
		if ( fTGA != NULL )
			fclose( fTGA );
		return false;
	}

	texture->width_guess	= tga_guess.header_guess[1] * 256 + tga_guess.header_guess[0];
	texture->height_guess	= tga_guess.header_guess[3] * 256 + tga_guess.header_guess[2];
	texture->bpp_guess		= tga_guess.header_guess[4];
	tga_guess.Width_guess	= texture->width_guess;
	tga_guess.Height_guess	= texture->height_guess;
	tga_guess.Bpp_guess		= texture->bpp_guess;

	if ( ( texture->width_guess <= 0 ) || ( texture->height_guess <= 0 ) || ( ( texture->bpp_guess != 24 ) && ( texture->bpp_guess != 32 ) ) )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Invalid texture information\n" );
		if ( fTGA != NULL )
			fclose( fTGA );
		return false;
	}

	if ( texture->bpp_guess == 24 )
		texture->type_guess = GL_RGB;
	else
		texture->type_guess = GL_RGBA;

	tga_guess.bytesPerPixel_guess	= ( tga_guess.Bpp_guess / 8 );
	tga_guess.imageSize_guess		= ( tga_guess.bytesPerPixel_guess * tga_guess.Width_guess * tga_guess.Height_guess );
	texture->imageData_guess		= (GLubyte *)malloc( tga_guess.imageSize_guess );

	if ( texture->imageData_guess == NULL )
	{
		gEngfuncs.Con_Printf( "TGA LOAD: Could not allocate memory for image\n" );
		fclose( fTGA );
		return false;
	}

	GLuint pixelcount		= tga_guess.Height_guess * tga_guess.Width_guess;
	GLuint currentpixel		= 0;
	GLuint currentbyte		= 0;
	GLubyte *colorbuffer	= (GLubyte *)malloc( tga_guess.bytesPerPixel_guess );

	do
	{
		GLubyte chunkheader = 0;

		if ( fread( &chunkheader, sizeof( GLubyte ), 1, fTGA ) == 0 )
		{
			gEngfuncs.Con_Printf( "TGA LOAD: Could not read RLE header\n" );
			if ( fTGA != NULL )
				fclose( fTGA );
			if ( texture->imageData_guess != NULL )
				free( texture->imageData_guess );
			return false;
		}

		if ( chunkheader < 128 )
		{
			short counter;
			chunkheader++;
			for ( counter = 0; counter < chunkheader; counter++ )
			{
				if ( fread( colorbuffer, 1, tga_guess.bytesPerPixel_guess, fTGA ) != tga_guess.bytesPerPixel_guess )
				{
					gEngfuncs.Con_Printf( "TGA LOAD: Could not read image data\n" );
					if ( fTGA != NULL )
						fclose( fTGA );
					if ( colorbuffer != NULL )
						free( colorbuffer );
					if ( texture->imageData_guess != NULL )
						free( texture->imageData_guess );
					return false;
				}

				texture->imageData_guess[currentbyte] = colorbuffer[2];
				texture->imageData_guess[currentbyte + 1] = colorbuffer[1];
				texture->imageData_guess[currentbyte + 2] = colorbuffer[0];

				if ( tga_guess.bytesPerPixel_guess == 4 )
					texture->imageData_guess[currentbyte + 3] = colorbuffer[3];

				currentbyte += tga_guess.bytesPerPixel_guess;
				currentpixel++;

				if ( currentpixel > pixelcount )
				{
					gEngfuncs.Con_Printf( "TGA LOAD: Too many pixels read\n" );
					if ( fTGA != NULL )
						fclose( fTGA );
					if ( colorbuffer != NULL )
						free( colorbuffer );
					if ( texture->imageData_guess != NULL )
						free( texture->imageData_guess );
					return false;
				}
			}
		}
		else
		{
			short counter;
			chunkheader -= 127;
			if ( fread( colorbuffer, 1, tga_guess.bytesPerPixel_guess, fTGA ) != tga_guess.bytesPerPixel_guess )
			{
				gEngfuncs.Con_Printf( "TGA LOAD: Could not read from file\n" );
				if ( fTGA != NULL )
					fclose( fTGA );
				if ( colorbuffer != NULL )
					free( colorbuffer );
				if ( texture->imageData_guess != NULL )
					free( texture->imageData_guess );
				return false;
			}

			for ( counter = 0; counter < chunkheader; counter++ )
			{
				texture->imageData_guess[currentbyte] = colorbuffer[2];
				texture->imageData_guess[currentbyte + 1] = colorbuffer[1];
				texture->imageData_guess[currentbyte + 2] = colorbuffer[0];

				if ( tga_guess.bytesPerPixel_guess == 4 )
					texture->imageData_guess[currentbyte + 3] = colorbuffer[3];

				currentbyte += tga_guess.bytesPerPixel_guess;
				currentpixel++;

				if ( currentpixel > pixelcount )
				{
					gEngfuncs.Con_Printf( "TGA LOAD: Too many pixels read\n" );
					if ( fTGA != NULL )
						fclose( fTGA );
					if ( colorbuffer != NULL )
						free( colorbuffer );
					if ( texture->imageData_guess != NULL )
						free( texture->imageData_guess );
					return false;
				}
			}
		}
	}
	while ( currentpixel < pixelcount );

	fclose( fTGA );
	return true;
}
