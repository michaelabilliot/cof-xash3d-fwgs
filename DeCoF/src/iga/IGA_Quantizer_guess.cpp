// IGA_Quantizer_guess.cpp -- octree color quantizer
// Based on CQuantizer by Jeff Prosise (Microsoft Systems Journal, "Wicked Code"),
// changed to read 32-bit RGBA pixels and to write a 256 entry RGB palette

extern "C"
{
#include "quakedef.h"
}

#include "IGA_Quantizer_guess.h"

CQuantizer::CQuantizer( int nMaxColors, int nColorBits )
{
	m_pTree = NULL;
	m_nLeafCount = 0;
	for ( int i = 0; i <= nColorBits; i++ )
		m_pReducibleNodes[i] = NULL;
	m_nMaxColors = nMaxColors;
	m_nColorBits = nColorBits;
}

CQuantizer::~CQuantizer( void )
{
	if ( m_pTree != NULL )
		DeleteTree( &m_pTree );
}

void CQuantizer::ProcessImage( unsigned char *pImage, int nWidth, int nHeight )
{
	unsigned char *pbBits;
	unsigned char r, g, b;
	int i, j;

	pbBits = pImage;

	for ( i = 0; i < nHeight; i++ )
	{
		for ( j = 0; j < nWidth; j++ )
		{
			r = *pbBits++;
			g = *pbBits++;
			b = *pbBits++;
			pbBits++;
			AddColor( &m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount, m_pReducibleNodes );
			while ( m_nLeafCount > m_nMaxColors )
				ReduceTree( m_nColorBits, &m_nLeafCount, m_pReducibleNodes );
		}
	}
}

int CQuantizer::GetLeftShiftCount( int dwVal )
{
	int nCount = 0;
	for ( int i = 0; i < sizeof( int ) * 8; i++ )
	{
		if ( dwVal & 1 )
			nCount++;
		dwVal >>= 1;
	}
	return ( 8 - nCount );
}

int CQuantizer::GetRightShiftCount( int dwVal )
{
	for ( int i = 0; i < sizeof( int ) * 8; i++ )
	{
		if ( dwVal & 1 )
			return i;
		dwVal >>= 1;
	}
	return -1;
}

void CQuantizer::AddColor( NODE **ppNode, unsigned char r, unsigned char g, unsigned char b, int nColorBits, int nLevel, int *pLeafCount, NODE **pReducibleNodes )
{
	static unsigned char mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	// If the node doesn't exist, create it.
	if ( *ppNode == NULL )
		*ppNode = CreateNode( nLevel, nColorBits, pLeafCount, pReducibleNodes );

	// Update color information if it's a leaf node.
	if ( ( *ppNode )->bIsLeaf )
	{
		( *ppNode )->nPixelCount++;
		( *ppNode )->nRedSum += r;
		( *ppNode )->nGreenSum += g;
		( *ppNode )->nBlueSum += b;
	}
	// Recurse a level deeper if the node is not a leaf.
	else
	{
		int shift = 7 - nLevel;
		int nIndex = ( ( ( r & mask[nLevel] ) >> shift ) << 2 ) |
			( ( ( g & mask[nLevel] ) >> shift ) << 1 ) |
			( ( b & mask[nLevel] ) >> shift );
		AddColor( &( ( *ppNode )->pChild[nIndex] ), r, g, b, nColorBits, nLevel + 1, pLeafCount, pReducibleNodes );
	}
}

NODE *CQuantizer::CreateNode( int nLevel, int nColorBits, int *pLeafCount, NODE **pReducibleNodes )
{
	NODE *pNode;

	pNode = new NODE;
	Q_memset( pNode, 0, sizeof( NODE ) );

	pNode->bIsLeaf = ( nLevel == nColorBits ) ? true : false;
	if ( pNode->bIsLeaf )
	{
		( *pLeafCount )++;
	}
	else
	{
		// Add the node to the reducible list for this level
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel] = pNode;
	}
	return pNode;
}

void CQuantizer::ReduceTree( int nColorBits, int *pLeafCount, NODE **pReducibleNodes )
{
	// Find the deepest level containing at least one reducible node.
	for ( int i = nColorBits - 1; ( i > 0 ) && ( pReducibleNodes[i] == NULL ); i-- );

	// Reduce the node most recently added to the list at level i.
	NODE *pNode = pReducibleNodes[i];
	pReducibleNodes[i] = pNode->pNext;

	int nRedSum = 0;
	int nGreenSum = 0;
	int nBlueSum = 0;
	int nChildren = 0;

	for ( i = 0; i < 8; i++ )
	{
		if ( pNode->pChild[i] != NULL )
		{
			nRedSum += pNode->pChild[i]->nRedSum;
			nGreenSum += pNode->pChild[i]->nGreenSum;
			nBlueSum += pNode->pChild[i]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[i]->nPixelCount;
			delete pNode->pChild[i];
			pNode->pChild[i] = NULL;
			nChildren++;
		}
	}

	pNode->bIsLeaf = true;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum = nBlueSum;
	*pLeafCount -= ( nChildren - 1 );
}

void CQuantizer::DeleteTree( NODE **ppNode )
{
	for ( int i = 0; i < 8; i++ )
	{
		if ( ( *ppNode )->pChild[i] != NULL )
			DeleteTree( &( ( *ppNode )->pChild[i] ) );
	}
	delete *ppNode;
	*ppNode = NULL;
}

void CQuantizer::GetPaletteColors( NODE *pTree, unsigned char *prgb, int *pIndex )
{
	if ( pTree->bIsLeaf )
	{
		int nOffset_guess = *pIndex * 3;
		prgb[nOffset_guess] = (unsigned char)( ( pTree->nRedSum ) / ( pTree->nPixelCount ) );
		prgb[nOffset_guess + 1] = (unsigned char)( ( pTree->nGreenSum ) / ( pTree->nPixelCount ) );
		prgb[nOffset_guess + 2] = (unsigned char)( ( pTree->nBlueSum ) / ( pTree->nPixelCount ) );
		( *pIndex )++;
	}
	else
	{
		for ( int i = 0; i < 8; i++ )
		{
			if ( pTree->pChild[i] != NULL )
				GetPaletteColors( pTree->pChild[i], prgb, pIndex );
		}
	}
}

int CQuantizer::GetColorCount( void )
{
	return m_nLeafCount;
}

void CQuantizer::GetColorTable( unsigned char *prgb )
{
	Q_memset( prgb, 0, 256 * 3 );
	int nIndex = 0;
	GetPaletteColors( m_pTree, prgb, &nIndex );
}

unsigned char FindNearestColor_guess( unsigned char *pPalette, int nColors, int r, int g, int b )
{
	int bestColor_guess;
	int bestDist_guess;
	int i;

	bestDist_guess = 3 * 256 * 256 + 1;
	bestColor_guess = -1;

	for ( i = 0; i < nColors; i++ )
	{
		int dr = r - pPalette[i * 3];
		int dg = g - pPalette[i * 3 + 1];
		int db = b - pPalette[i * 3 + 2];
		int dist = dr * dr + dg * dg + db * db;

		if ( dist < bestDist_guess )
		{
			if ( dist == 0 )
				return i;

			bestDist_guess = dist;
			bestColor_guess = i;
		}
	}

	return bestColor_guess;
}

void QuantizeImage_guess( unsigned char *pImage, int nWidth, int nHeight, unsigned char *pIndices, unsigned char *pPalette, int nColors )
{
	CQuantizer *pColorQuantizer_guess;
	unsigned char *pOutput_guess;
	unsigned char *pInput_guess;
	int r, g, b;
	int i, j;

	pColorQuantizer_guess = new CQuantizer( nColors, 8 );
	pColorQuantizer_guess->ProcessImage( pImage, nWidth, nHeight );
	pColorQuantizer_guess->GetColorTable( pPalette );

	pOutput_guess = pIndices;
	pInput_guess = pImage;

	for ( i = 0; i < nWidth; i++ )
	{
		for ( j = 0; j < nHeight; j++ )
		{
			r = *pInput_guess++;
			g = *pInput_guess++;
			b = *pInput_guess++;
			pInput_guess++;
			*pOutput_guess++ = FindNearestColor_guess( pPalette, nColors, r, g, b );
		}
	}

	delete pColorQuantizer_guess;
}
