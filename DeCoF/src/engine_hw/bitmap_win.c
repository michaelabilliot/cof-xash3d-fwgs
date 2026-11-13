// bitmap_win.c -- loads uncompressed Windows bitmaps (skyboxes)

#include "quakedef.h"
#include "mem.h"
#include "zone.h"

/*
===============
LoadBMP8

8 bit bitmap with its palette, rows bottom-up as in the file
===============
*/
void LoadBMP8( FileHandle_t hFile, byte **pPalette, int *nPalette, byte **pImage, int *nWidth, int *nHeight )
{
	int i, rc = 0;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	ULONG cbBmpBits;
	BYTE *pbBmpBits;
	byte *pb, *pbPal = NULL;
	ULONG cbPalBytes;
	ULONG biTrueWidth;
	byte *pbOut_guess = NULL;

	*pImage = NULL;
	*pPalette = NULL;

	if ( nWidth )
		*nWidth = 0;
	if ( nHeight )
		*nHeight = 0;

	// Read file header
	if ( FS_Read( &bmfh, sizeof( bmfh ), 1, hFile ) != sizeof( bmfh ) )
		goto GetOut;

	// Bogus file header check
	if ( !( bmfh.bfReserved1 == 0 && bmfh.bfReserved2 == 0 ) )
		goto GetOut;

	// Read info header
	if ( FS_Read( &bmih, sizeof( bmih ), 1, hFile ) != sizeof( bmih ) )
		goto GetOut;

	// Bogus info header check
	if ( !( bmih.biSize == sizeof( bmih ) && bmih.biPlanes == 1 ) )
		goto GetOut;

	// Bogus bit depth?  Only 8-bit supported.
	if ( bmih.biBitCount != 8 )
		goto GetOut;

	// Bogus compression?  Only non-compressed supported.
	if ( bmih.biCompression != BI_RGB )
		goto GetOut;

	// Figure out how many entires are actually in the table
	if ( bmih.biClrUsed == 0 )
	{
		bmih.biClrUsed = 256;
		cbPalBytes = ( 1 << bmih.biBitCount ) * sizeof( RGBQUAD );
	}
	else
	{
		cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
	}

	// Read palette (bmih.biClrUsed entries)
	if ( FS_Read( rgrgbPalette, cbPalBytes, 1, hFile ) != (int)cbPalBytes )
		goto GetOut;

	// convert to a packed 768 byte palette
	*pPalette = malloc( 768 );
	if ( *pPalette == NULL )
		goto GetOut;

	pb = *pPalette;
	Q_memset( pb, 0, 768 );

	// Copy over used entries
	for ( i = 0; i < (int)bmih.biClrUsed; i++ )
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Read bitmap bits (remainder of file)
	cbBmpBits = bmfh.bfSize - FS_Tell( hFile );
	pb = Mem_Malloc( cbBmpBits );
	if ( FS_Read( pb, cbBmpBits, 1, hFile ) != (int)cbBmpBits )
		return;

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = ( bmih.biWidth + 3 ) & ~3;

	*pImage = Mem_Malloc( cbBmpBits );
	pbBmpBits = *pImage;

	// reverse the order of the data.
	pb += ( bmih.biHeight - 1 ) * biTrueWidth;
	for ( i = 0; i < bmih.biHeight; i++ )
	{
		memmove( &pbBmpBits[biTrueWidth * i], pb, biTrueWidth );
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	Mem_Free( pb );

	if ( nWidth )
		*nWidth = bmih.biWidth;
	if ( nHeight )
		*nHeight = bmih.biHeight;

GetOut:
	if ( hFile )
		FS_Close( hFile );
}

/*
===============
LoadBMP16

16 bit bitmap, or an 8 bit one converted to 15 or 16 bit pixels
===============
*/
byte *LoadBMP16( FileHandle_t fin, qboolean is15bit )
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	byte *pImage16 = NULL;

	if ( FS_Read( &bmfh, sizeof( bmfh ), 1, fin ) == 1 &&
		bmfh.bfType == 0x4D42 &&
		bmfh.bfReserved1 == 0 &&
		bmfh.bfReserved2 == 0 &&
		FS_Read( &bmih, sizeof( bmih ), 1, fin ) == 1 &&
		bmih.biSize >= sizeof( bmih ) &&
		bmih.biPlanes == 1 &&
		bmih.biCompression == BI_RGB )
	{
		if ( bmih.biBitCount == 16 )
		{
			// the offset goes in as the seek type (original bug)
			FS_Seek( fin, 0, bmfh.bfOffBits );
			pImage16 = Hunk_AllocName( bmih.biSizeImage, "SKYBOX" );
			if ( pImage16 && FS_Read( pImage16, bmih.biSizeImage, 1, fin ) != (int)bmih.biSizeImage )
				pImage16 = NULL;
		}
		else if ( bmih.biBitCount == 8 )
		{
			int nPalette;
			RGBQUAD *pPalette;

			if ( bmih.biClrUsed == 0 )
				nPalette = 1 << bmih.biBitCount;
			else
				nPalette = bmih.biClrUsed;

			pPalette = Mem_Malloc( nPalette * sizeof( RGBQUAD ) );
			if ( pPalette && FS_Read( pPalette, nPalette * sizeof( RGBQUAD ), 1, fin ) == (int)( nPalette * sizeof( RGBQUAD ) ) )
			{
				int nImage8;
				byte *pImage8;

				nImage8 = bmih.biSizeImage;
				pImage8 = Mem_Malloc( nImage8 );
				FS_Seek( fin, 0, bmfh.bfOffBits );

				if ( pImage8 )
				{
					if ( FS_Read( pImage8, nImage8, 1, fin ) == nImage8 )
					{
						int SizeOfRow8 = ( bmih.biWidth + 3 ) / 4 * 4;
						int SizeOfRow16 = ( bmih.biWidth * sizeof( unsigned short ) + 3 ) / 4 * 4;
						int TrueHeight;
						int nImage16;

						TrueHeight = bmih.biHeight;
						if ( TrueHeight < 0 )
							TrueHeight = -TrueHeight;

						nImage16 = TrueHeight * SizeOfRow16;
						pImage16 = Hunk_AllocName( nImage16, "SKYBOX" );
						if ( pImage16 )
						{
							if ( is15bit )
							{
								if ( bmih.biHeight > 0 )
								{
									byte *row16 = pImage16;
									byte *row8 = pImage8 + nImage8 - SizeOfRow8;
									int nRows = TrueHeight;

									while ( nRows-- )
									{
										unsigned short *p16 = (unsigned short *)row16;
										byte *p8 = row8;
										int nPixels = bmih.biWidth;

										while ( nPixels-- )
										{
											short r, g, b;

											pPalette[*p8].rgbRed |= ( pPalette[*p8].rgbRed & 0xC0 ) >> 6;
											r = pPalette[*p8].rgbRed;
											pPalette[*p8].rgbGreen |= ( pPalette[*p8].rgbGreen & 0xC0 ) >> 6;
											g = pPalette[*p8].rgbGreen;
											pPalette[*p8].rgbBlue |= ( pPalette[*p8].rgbBlue & 0xC0 ) >> 6;
											b = pPalette[*p8].rgbBlue;

											*p16 = ( ( r << 7 ) & 0x7C00 ) | ( ( g << 2 ) & 0x3E0 ) | ( b >> 3 );
											p16++;
											p8++;
										}

										row16 += SizeOfRow16;
										row8 -= SizeOfRow8;
									}
								}
								else
								{
									byte *row16 = pImage16;
									byte *row8 = pImage8;
									int nRows = TrueHeight;

									while ( nRows-- )
									{
										unsigned short *p16 = (unsigned short *)row16;
										byte *p8 = row8;
										int nPixels = bmih.biWidth;

										while ( nPixels-- )
										{
											short r, g, b;

											pPalette[*p8].rgbRed |= ( pPalette[*p8].rgbRed & 0xC0 ) >> 6;
											r = pPalette[*p8].rgbRed;
											pPalette[*p8].rgbGreen |= ( pPalette[*p8].rgbGreen & 0xC0 ) >> 6;
											g = pPalette[*p8].rgbGreen;
											pPalette[*p8].rgbBlue |= ( pPalette[*p8].rgbBlue & 0xC0 ) >> 6;
											b = pPalette[*p8].rgbBlue;

											*p16 = ( ( r << 7 ) & 0x7C00 ) | ( ( g << 2 ) & 0x3E0 ) | ( b >> 3 );
											p16++;
											p8++;
										}

										row16 += SizeOfRow16;
										row8 += SizeOfRow8;
									}
								}
							}
							else
							{
								if ( bmih.biHeight > 0 )
								{
									byte *row16 = pImage16;
									byte *row8 = pImage8 + nImage8 - SizeOfRow8;
									int nRows = TrueHeight;

									while ( nRows-- )
									{
										unsigned short *p16 = (unsigned short *)row16;
										byte *p8 = row8;
										int nPixels = bmih.biWidth;

										while ( nPixels-- )
										{
											short r, g, b;

											pPalette[*p8].rgbRed |= ( pPalette[*p8].rgbRed & 0xC0 ) >> 6;
											r = pPalette[*p8].rgbRed;
											pPalette[*p8].rgbGreen |= ( pPalette[*p8].rgbGreen & 0xC0 ) >> 6;
											g = pPalette[*p8].rgbGreen;
											pPalette[*p8].rgbBlue |= ( pPalette[*p8].rgbBlue & 0xC0 ) >> 6;
											b = pPalette[*p8].rgbBlue;

											*p16 = ( ( r << 8 ) & 0xF800 ) | ( ( g << 3 ) & 0x7E0 ) | ( b >> 3 );
											p16++;
											p8++;
										}

										row16 += SizeOfRow16;
										row8 -= SizeOfRow8;
									}
								}
								else
								{
									byte *row16 = pImage16;
									byte *row8 = pImage8;
									int nRows = TrueHeight;

									while ( nRows-- )
									{
										unsigned short *p16 = (unsigned short *)row16;
										byte *p8 = row8;
										int nPixels = bmih.biWidth;

										while ( nPixels-- )
										{
											short r, g, b;

											pPalette[*p8].rgbRed |= ( pPalette[*p8].rgbRed & 0xC0 ) >> 6;
											r = pPalette[*p8].rgbRed;
											pPalette[*p8].rgbGreen |= ( pPalette[*p8].rgbGreen & 0xC0 ) >> 6;
											g = pPalette[*p8].rgbGreen;
											pPalette[*p8].rgbBlue |= ( pPalette[*p8].rgbBlue & 0xC0 ) >> 6;
											b = pPalette[*p8].rgbBlue;

											*p16 = ( ( r << 8 ) & 0xF800 ) | ( ( g << 3 ) & 0x7E0 ) | ( b >> 3 );
											p16++;
											p8++;
										}

										row16 += SizeOfRow16;
										row8 += SizeOfRow8;
									}
								}
							}
						}
					}
					else
					{
						Mem_Free( pImage8 );
						Mem_Free( pPalette );
					}
				}
			}
		}
	}

	FS_Close( fin );
	return pImage16;
}
