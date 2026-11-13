// IGA_Metadata_guess.cpp -- the ad images: 8-bit and 24-bit BMP files read through a CUtlBuffer
// into CAdContent_guess, quantized to 256 colors, the image pack of the metadata and the
// resamplers that scale a palettized image. No string names the file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "FileSystem.h"
#include "UtlBuffer.h"
#include "UtlVector.h"
#include "UtlRBTree.h"

extern "C"
{
#include "quakedef.h"
#include "mathlib.h"
#include "filesystem_.h"
}

#include "IGA_Engine_guess.h"
#include "IGA_Quantizer_guess.h"

// header of the image pack loaded with the metadata
struct AdImagePackHeader_guess
{
	unsigned int	m_nVersion_guess;
	unsigned int	m_nImages_guess;
};

struct AdImagePackEntry_guess
{
	char			m_szName_guess[64];
	unsigned int	m_nOffset_guess;
	unsigned int	m_nSize_guess;
};

float	*ComputeResampleWeights_guess( int nDest, int nSrc, bool bScale );
void	ResampleDown_guess( unsigned char *pPalette, unsigned char *pImage, int nSrcWidth, int nSrcHeight, unsigned char *pDest, int nDestWidth, int nDestHeight );
void	ResampleUp_guess( unsigned char *pPalette, unsigned char *pImage, int nSrcWidth, int nSrcHeight, unsigned char *pDest, int nDestWidth, int nDestHeight );
void	ResampleImage_guess( unsigned char *pPalette, unsigned char *pSrc, unsigned char *pDest, int nSrcWidth, int nSrcHeight, int nDestWidth, int nDestHeight );

static bool s_bForceHUD_guess;

// The local names are guesses; where two of them hash to the same stack slot bucket
// they are declared in the order that gives the retail frame.

bool CInGameAdvertisingSystem::IsPalettedImage_guess( void *pData, int nSize )
{
	CUtlBuffer buf( 0, 0, false );
	buf.Put( pData, nSize );

	BITMAPFILEHEADER bmfh;
	buf.Get( &bmfh, sizeof( bmfh ) );
	BITMAPINFOHEADER bmih;
	buf.Get( &bmih, sizeof( bmih ) );

	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 )
		return false;
	if ( bmih.biCompression != BI_RGB )
		return false;
	if ( bmih.biBitCount == 8 )
		return true;
	return false;
}

bool CInGameAdvertisingSystem::LoadPalettedImage_guess( void *pData, int nSize, CAdContent_guess *pContent )
{
	int palsize_guess;

	CUtlBuffer buf( 0, 0, false );
	buf.Put( pData, nSize );

	BITMAPFILEHEADER fileheader;
	buf.Get( &fileheader, sizeof( fileheader ) );
	pContent->m_bHUD_guess = ( fileheader.bfReserved1 == 1 ) ? true : s_bForceHUD_guess;

	BITMAPINFOHEADER bmih;
	buf.Get( &bmih, sizeof( bmih ) );
	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 )
		return false;
	if ( bmih.biBitCount != 8 )
		return false;
	if ( bmih.biCompression != BI_RGB )
		return false;

	palsize_guess = 0;
	if ( !bmih.biClrUsed )
	{
		bmih.biClrUsed = 256;
		palsize_guess = ( 1 << bmih.biBitCount ) * sizeof( RGBQUAD );
	}
	else
	{
		palsize_guess = bmih.biClrUsed * sizeof( RGBQUAD );
	}

	RGBQUAD rgbQuads_guess[256];
	buf.Get( rgbQuads_guess, palsize_guess );

	pContent->m_nType_guess = 1;
	pContent->m_nWidth_guess = bmih.biWidth;
	pContent->m_nHeight_guess = bmih.biHeight;
	int nImage_guess = pContent->m_nWidth_guess * pContent->m_nHeight_guess;
	int nData_guess = nImage_guess;
	pContent->m_pData_guess = new unsigned char[nData_guess];
	pContent->m_nMagic_guess = 0xdeadbeef;
	pContent->m_nDataSize_guess = nData_guess;
	if ( !pContent->m_pData_guess )
		return false;

	int iy_guess;
	for ( iy_guess = 0; iy_guess < pContent->m_nHeight_guess; iy_guess++ )
		buf.Get( pContent->m_pData_guess + ( pContent->m_nHeight_guess - iy_guess - 1 ) * pContent->m_nWidth_guess, pContent->m_nWidth_guess );

	RGBQUAD *pPal_guess = rgbQuads_guess;
	int j;
	for ( j = 0; j < (int)bmih.biClrUsed; j++, pPal_guess++ )
	{
		pContent->m_Palette_guess[j * 3 + 0] = pPal_guess->rgbRed;
		pContent->m_Palette_guess[j * 3 + 1] = pPal_guess->rgbGreen;
		pContent->m_Palette_guess[j * 3 + 2] = pPal_guess->rgbBlue;
	}
	for ( ; j < 256; j++ )
	{
		pContent->m_Palette_guess[j * 3 + 0] = 0;
		pContent->m_Palette_guess[j * 3 + 1] = 0;
		pContent->m_Palette_guess[j * 3 + 2] = 0;
	}

	Q_memcpy( pContent->m_pPalette_guess, pContent->m_Palette_guess, sizeof( pContent->m_Palette_guess ) );
	return true;
}

bool CInGameAdvertisingSystem::LoadTrueColorImage_guess( void *pData, int nSize, CAdContent_guess *pContent, const Color *pBorderColor )
{
	CUtlBuffer buf( 0, 0, false );
	buf.Put( pData, nSize );

	BITMAPFILEHEADER bmfh;
	buf.Get( &bmfh, sizeof( bmfh ) );
	pContent->m_bHUD_guess = ( bmfh.bfReserved1 == 1 ) ? true : s_bForceHUD_guess;

	BITMAPINFOHEADER bmih;
	buf.Get( &bmih, sizeof( bmih ) );
	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 )
		return false;
	if ( bmih.biBitCount != 24 )
		return false;
	if ( bmih.biCompression != BI_RGB )
		return false;

	pContent->m_nType_guess = 1;
	pContent->m_nWidth_guess = bmih.biWidth;
	pContent->m_nHeight_guess = bmih.biHeight;

	int iRead_guess = bmfh.bfSize - buf.TellGet();
	int bytes_guess = iRead_guess;
	unsigned char *raw2_guess = new unsigned char[iRead_guess];
	if ( !raw2_guess )
		return false;

	buf.Get( raw2_guess, iRead_guess );
	unsigned char *base_guess = raw2_guess;

	pContent->m_pData_guess = new unsigned char[bytes_guess];
	pContent->m_nMagic_guess = 0xdeadbeef;
	pContent->m_nDataSize_guess = bytes_guess;

	int cbRgba_guess = pContent->m_nWidth_guess * pContent->m_nHeight_guess * 4;
	int nSpan_guess = ( bmih.biWidth + 3 ) & ~3;
	unsigned char *pTrue_guess = new unsigned char[cbRgba_guess];
	unsigned char *pDest_guess = pTrue_guess;

	int nX_guess, nPy_guess;
	for ( nPy_guess = 0; nPy_guess < pContent->m_nHeight_guess; nPy_guess++ )
	{
		int offset_guess = ( pContent->m_nHeight_guess - 1 - nPy_guess ) * 3 * nSpan_guess;
		unsigned char *p = base_guess + offset_guess;
		for ( nX_guess = 0; nX_guess < pContent->m_nWidth_guess; nX_guess++ )
		{
			pDest_guess[2] = *p++;
			pDest_guess[1] = *p++;
			pDest_guess[0] = *p++;
			pDest_guess[3] = 0xff;
			pDest_guess += 4;
		}
	}

	if ( pBorderColor )
	{
		QuantizeImage_guess( pTrue_guess, pContent->m_nWidth_guess, pContent->m_nHeight_guess, pContent->m_pData_guess, pContent->m_Palette_guess, 255 );
		pContent->m_Palette_guess[255 * 3 + 0] = pBorderColor->r();
		pContent->m_Palette_guess[255 * 3 + 1] = pBorderColor->g();
		pContent->m_Palette_guess[255 * 3 + 2] = pBorderColor->b();
	}
	else
	{
		QuantizeImage_guess( pTrue_guess, pContent->m_nWidth_guess, pContent->m_nHeight_guess, pContent->m_pData_guess, pContent->m_Palette_guess, 256 );
	}

	delete [] pTrue_guess;
	pContent->m_pPalette_guess = pContent->m_Palette_guess;
	delete [] base_guess;
	return true;
}

bool CInGameAdvertisingSystem::LoadHUDImage_guess( void *pData, int nSize, CAdContent_guess *pContent )
{
	CUtlBuffer buf( 0, 0, false );
	buf.Put( pData, nSize );

	BITMAPFILEHEADER bmfh;
	buf.Get( &bmfh, sizeof( bmfh ) );
	pContent->m_bHUD_guess = ( bmfh.bfReserved1 == 1 ) ? true : s_bForceHUD_guess;

	BITMAPINFOHEADER bmih;
	buf.Get( &bmih, sizeof( bmih ) );
	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 )
		return false;
	if ( bmih.biBitCount != 24 && bmih.biBitCount != 8 )
		return false;

	bool bPaletted_guess = ( bmih.biBitCount == 8 ) ? true : false;
	if ( bmih.biCompression != BI_RGB )
		return false;

	int iCbPal_guess = 0;
	RGBQUAD colormap_guess[256] = { 0 };
	if ( bPaletted_guess )
	{
		if ( !bmih.biClrUsed )
		{
			bmih.biClrUsed = 256;
			iCbPal_guess = ( 1 << bmih.biBitCount ) * sizeof( RGBQUAD );
		}
		else
		{
			iCbPal_guess = bmih.biClrUsed * sizeof( RGBQUAD );
		}
		buf.Get( colormap_guess, iCbPal_guess );
	}

	pContent->m_nType_guess = 2;
	pContent->m_nWidth_guess = bmih.biWidth;
	pContent->m_nHeight_guess = bmih.biHeight;

	int len2_guess = bmfh.bfSize - buf.TellGet();
	int nStride_guess = ( bmih.biWidth + 3 ) & ~3;
	unsigned char *pMem_guess = new unsigned char[len2_guess];
	if ( !pMem_guess )
		return false;

	unsigned char *lpSrc_guess = pMem_guess;
	buf.Get( pMem_guess, len2_guess );
	pContent->m_pData_guess = new unsigned char[pContent->m_nWidth_guess * pContent->m_nHeight_guess * 4];
	pContent->m_nMagic_guess = 0xdeadbeef;
	pContent->m_nDataSize_guess = pContent->m_nWidth_guess * pContent->m_nHeight_guess * 4;

	unsigned char *lpDest_guess = pContent->m_pData_guess;
	int nX_guess, iy_guess;
	if ( bPaletted_guess )
	{
		for ( iy_guess = 0; iy_guess < pContent->m_nHeight_guess; iy_guess++ )
		{
			int offset_guess = ( pContent->m_nHeight_guess - 1 - iy_guess ) * nStride_guess;
			unsigned char *p = lpSrc_guess + offset_guess;
			for ( nX_guess = 0; nX_guess < pContent->m_nWidth_guess; nX_guess++ )
			{
				RGBQUAD *pColor_guess = &colormap_guess[*p++];
				lpDest_guess[2] = pColor_guess->rgbBlue;
				lpDest_guess[1] = pColor_guess->rgbGreen;
				lpDest_guess[0] = pColor_guess->rgbRed;
				lpDest_guess[3] = 0xff;
				lpDest_guess += 4;
			}
		}
	}
	else
	{
		for ( iy_guess = 0; iy_guess < pContent->m_nHeight_guess; iy_guess++ )
		{
			int offset_guess = ( pContent->m_nHeight_guess - 1 - iy_guess ) * 3 * nStride_guess;
			unsigned char *p = lpSrc_guess + offset_guess;
			for ( nX_guess = 0; nX_guess < pContent->m_nWidth_guess; nX_guess++ )
			{
				lpDest_guess[2] = *p++;
				lpDest_guess[1] = *p++;
				lpDest_guess[0] = *p++;
				lpDest_guess[3] = 0xff;
				lpDest_guess += 4;
			}
		}
	}

	pContent->m_pPalette_guess = NULL;
	delete [] lpSrc_guess;
	return true;
}

bool CInGameAdvertisingSystem::LoadHUDImageFile_guess( const char *pszFile, int nFlags, CAdContent_guess *pContent )
{
	bool bOK_guess = false;
	unsigned char *pMem_guess = NULL;
	int size_guess = 0;

	FileHandle_t fp_guess = FS_Open( pszFile, "rb" );
	if ( fp_guess )
	{
		size_guess = FS_Size( fp_guess );
		pMem_guess = new unsigned char[size_guess];
		FS_Read( pMem_guess, size_guess, 1, fp_guess );
		FS_Close( fp_guess );
	}

	if ( !pMem_guess )
		return bOK_guess;

	if ( LoadHUDImage_guess( pMem_guess, size_guess, pContent ) )
	{
		pContent->m_bLoaded_guess = true;
		pContent->m_bTexture_guess = false;
		if ( m_pClientAds_guess )
			m_pClientAds_guess->ContentLoaded_guess( nFlags, pContent->m_nCellId_guess, pContent->m_ContentId_guess, pContent->m_nWidth_guess, pContent->m_nHeight_guess, pContent->m_pData_guess );
		bOK_guess = true;
	}

	delete [] pMem_guess;
	return bOK_guess;
}

void ResampleDown_guess( unsigned char *pPalette, unsigned char *pImage, int nSrcWidth, int nSrcHeight, unsigned char *pDest, int nDestWidth, int nDestHeight )
{
	float *wx_guess;
	float *c2_guess;
	float *row0_guess;
	bool nextCol_guess;
	int nRowSize_guess;
	int jj_guess;
	float *sums_guess;
	int nX_guess;
	float *yw_guess;
	float *curN_guess;
	bool span_guess;
	float *pNextRow_guess;
	unsigned char *pSrc_guess;
	float flW_guess;
	int i;
	int y_guess;
	unsigned char *dstRow_guess;
	float *hWeights_guess;
	unsigned char rgb_guess[3];
	unsigned char *srcPix_guess;
	float *pYWeights_guess;

	pSrc_guess = pImage;
	dstRow_guess = pDest;
	hWeights_guess = ComputeResampleWeights_guess( nSrcWidth, nDestWidth, true );
	pYWeights_guess = ComputeResampleWeights_guess( nSrcHeight, nDestHeight, true );
	yw_guess = pYWeights_guess;
	nRowSize_guess = nDestWidth * 3 * sizeof( float );
	sums_guess = new float[nDestWidth * 6];
	row0_guess = sums_guess;
	pNextRow_guess = sums_guess + nDestWidth * 3;
	Q_memset( sums_guess, 0, nRowSize_guess * 2 );

	y_guess = 0;
	while ( y_guess < nDestHeight )
	{
		srcPix_guess = pSrc_guess;
		pSrc_guess += nSrcWidth;
		c2_guess = row0_guess;
		curN_guess = pNextRow_guess;
		nX_guess = 0;
		wx_guess = hWeights_guess;
		span_guess = yw_guess[1] > 0.0001f;

		while ( nX_guess < nDestWidth )
		{
			for ( i = 0; i < 3; i++ )
				rgb_guess[i] = pPalette[*srcPix_guess * 3 + i];

			flW_guess = wx_guess[0] * yw_guess[0];
			for ( i = 0; i < 3; i++ )
				c2_guess[i] += rgb_guess[i] * flW_guess;

			nextCol_guess = wx_guess[1] > 0.0001f;
			if ( nextCol_guess )
			{
				flW_guess = wx_guess[1] * yw_guess[0];
				for ( i = 0, jj_guess = 3; i < 3; i++, jj_guess++ )
					c2_guess[jj_guess] += rgb_guess[i] * flW_guess;
			}

			if ( span_guess )
			{
				flW_guess = wx_guess[0] * yw_guess[1];
				for ( i = 0; i < 3; i++ )
					curN_guess[i] += rgb_guess[i] * flW_guess;

				if ( nextCol_guess )
				{
					flW_guess = wx_guess[1] * yw_guess[1];
					for ( i = 0, jj_guess = 3; i < 3; i++, jj_guess++ )
						curN_guess[jj_guess] += rgb_guess[i] * flW_guess;
				}
			}

			if ( fabs( wx_guess[1] ) > 0.0001f )
			{
				nX_guess++;
				c2_guess += 3;
				curN_guess += 3;
			}
			wx_guess += 2;
			srcPix_guess++;
		}

		if ( fabs( yw_guess[1] ) > 0.0001f )
		{
			c2_guess = row0_guess;
			srcPix_guess = dstRow_guess;
			for ( i = nDestWidth - 1; i >= 0; i--, srcPix_guess++ )
			{
				int rgbIdx_guess[3];
				for ( int k_guess = 0; k_guess < 3; k_guess++ )
					rgbIdx_guess[k_guess] = (unsigned char)c2_guess[k_guess];
				*srcPix_guess = FindNearestColor_guess( pPalette, 256, rgbIdx_guess[0], rgbIdx_guess[1], rgbIdx_guess[2] );
				c2_guess += 3;
			}

			c2_guess = pNextRow_guess;
			pNextRow_guess = row0_guess;
			row0_guess = c2_guess;
			Q_memset( pNextRow_guess, 0, nRowSize_guess );
			y_guess++;
			dstRow_guess += nDestWidth;
		}
		yw_guess += 2;
	}

	delete [] hWeights_guess;
	delete [] pYWeights_guess;
	delete [] sums_guess;
}

float *ComputeResampleWeights_guess( int nDest, int nSrc, bool bScale )
{
	int offset_guess = 0;
	float *pTable_guess = new float[nDest * 2];
	float *wp_guess = pTable_guess;
	float flFactor_guess = bScale ? (float)nSrc / nDest : 1.0f;
	int divisor_guess = bScale ? nDest : nSrc;
	int nNext_guess;

	Q_memset( pTable_guess, 0, nDest * 2 * sizeof( float ) );
	for ( int i = 0; i < nDest; i++, wp_guess += 2 )
	{
		nNext_guess = offset_guess + nSrc;
		if ( nNext_guess > nDest )
		{
			wp_guess[0] = (float)( nDest - offset_guess ) / divisor_guess;
			wp_guess[1] = (float)( nNext_guess - nDest ) / divisor_guess;
			nNext_guess -= nDest;
		}
		else
		{
			wp_guess[0] = flFactor_guess;
			if ( nNext_guess == nDest )
			{
				wp_guess[1] = -1.0f;
				nNext_guess = 0;
			}
		}
		offset_guess = nNext_guess;
	}
	return pTable_guess;
}

void ResampleUp_guess( unsigned char *pPalette, unsigned char *pImage, int nSrcWidth, int nSrcHeight, unsigned char *pDest, int nDestWidth, int nDestHeight )
{
	float *wx_guess;
	unsigned char *d_guess;
	float flW_guess;
	int span_guess;
	unsigned char *top_guess;
	int i;
	unsigned char *pix_guess;
	int xSrc_guess;
	float *wy_guess;
	int nBelow_guess;
	unsigned char *sx_guess;
	unsigned char *pOutRow_guess;
	int y_guess;
	float *hWeights_guess;
	int rgbIdx_guess[3];
	unsigned char *pNext_guess;
	unsigned char *sLow_guess;
	float rgb_guess[3];
	float *pYWeights_guess;

	top_guess = pImage;
	pNext_guess = top_guess;
	pOutRow_guess = pDest;
	hWeights_guess = ComputeResampleWeights_guess( nDestWidth, nSrcWidth, false );
	pYWeights_guess = ComputeResampleWeights_guess( nDestHeight, nSrcHeight, false );
	wy_guess = pYWeights_guess;

	y_guess = 0;
	while ( y_guess < nSrcHeight )
	{
		nBelow_guess = wy_guess[1] > 0.0001f;
		xSrc_guess = 0;
		wx_guess = hWeights_guess;
		d_guess = pOutRow_guess;
		pOutRow_guess += nDestWidth;
		sx_guess = top_guess;
		if ( fabs( wy_guess[1] ) > 0.0001f )
		{
			y_guess++;
			top_guess += nSrcWidth;
			pNext_guess = top_guess;
		}

		while ( xSrc_guess < nSrcWidth )
		{
			span_guess = wx_guess[1] > 0.0001f;
			pix_guess = sx_guess;
			sLow_guess = pNext_guess;
			if ( fabs( wx_guess[1] ) > 0.0001f )
			{
				xSrc_guess++;
				sx_guess++;
				pNext_guess++;
			}

			flW_guess = wx_guess[0] * wy_guess[0];
			for ( i = 0; i < 3; i++ )
				rgb_guess[i] = pPalette[*pix_guess * 3 + i] * flW_guess;

			if ( span_guess )
			{
				flW_guess = wx_guess[1] * wy_guess[0];
				for ( i = 0; i < 3; i++ )
					rgb_guess[i] += pPalette[*sx_guess * 3 + i] * flW_guess;
			}

			if ( nBelow_guess )
			{
				flW_guess = wx_guess[0] * wy_guess[1];
				for ( i = 0; i < 3; i++ )
					rgb_guess[i] += pPalette[*sLow_guess * 3 + i] * flW_guess;

				if ( span_guess )
				{
					flW_guess = wx_guess[1] * wy_guess[1];
					for ( i = 0; i < 3; i++ )
						rgb_guess[i] += pPalette[*pNext_guess * 3 + i] * flW_guess;
				}
			}

			for ( i = 0; i < 3; i++ )
				rgbIdx_guess[i] = (unsigned char)rgb_guess[i];
			*d_guess = FindNearestColor_guess( pPalette, 256, rgbIdx_guess[0], rgbIdx_guess[1], rgbIdx_guess[2] );
			d_guess++;
			wx_guess += 2;
		}
		wy_guess += 2;
	}

	delete [] hWeights_guess;
	delete [] pYWeights_guess;
}

void ResampleImage_guess( unsigned char *pPalette, unsigned char *pSrc, unsigned char *pDest, int nSrcWidth, int nSrcHeight, int nDestWidth, int nDestHeight )
{
	if ( nSrcWidth >= nDestWidth && nSrcHeight >= nDestHeight )
		ResampleDown_guess( pPalette, pSrc, nSrcWidth, nSrcHeight, pDest, nDestWidth, nDestHeight );
	else
		ResampleUp_guess( pPalette, pSrc, nSrcWidth, nSrcHeight, pDest, nDestWidth, nDestHeight );
}

bool CInGameAdvertisingSystem::AddImage_guess( CUtlVector< CAdImage_guess * > &images, const char *pszName, void *pData, int nSize )
{
	unsigned char *ptr_guess = (unsigned char *)pData;
	AdImagePackHeader_guess *hdrPtr_guess = (AdImagePackHeader_guess *)ptr_guess;
	if ( hdrPtr_guess->m_nVersion_guess != 0x10001 )
		return false;
	if ( hdrPtr_guess->m_nImages_guess > 256 )
		return false;

	ptr_guess += sizeof( AdImagePackHeader_guess );
	AdImagePackEntry_guess *table_guess = (AdImagePackEntry_guess *)ptr_guess;
	for ( unsigned int i = 0; i < hdrPtr_guess->m_nImages_guess; i++ )
	{
		AdImagePackEntry_guess *e_guess = &table_guess[i];
		CAdImage_guess *newImage_guess = new CAdImage_guess;
		Q_strncpy( newImage_guess->m_szName_guess, e_guess->m_szName_guess, sizeof( newImage_guess->m_szName_guess ) );
		newImage_guess->m_nSize_guess = e_guess->m_nSize_guess;
		newImage_guess->m_pData_guess = new unsigned char[e_guess->m_nSize_guess];
		Q_memcpy( newImage_guess->m_pData_guess, (unsigned char *)pData + e_guess->m_nOffset_guess, e_guess->m_nSize_guess );
		images.AddToTail( newImage_guess );
	}
	return true;
}
