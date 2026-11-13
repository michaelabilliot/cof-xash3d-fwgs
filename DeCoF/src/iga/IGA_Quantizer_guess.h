// IGA_Quantizer_guess.h -- octree color quantizer (Jeff Prosise, MSJ "Wicked Code" sample),
// used to turn the ad images into 8-bit palettized textures

#ifndef IGA_QUANTIZER_GUESS_H
#define IGA_QUANTIZER_GUESS_H

typedef struct _NODE
{
	bool			bIsLeaf;			// true if node has no children
	int				nPixelCount;		// number of pixels represented by this leaf
	int				nRedSum;			// sum of red components
	int				nGreenSum;			// sum of green components
	int				nBlueSum;			// sum of blue components
	struct _NODE	*pChild[8];			// pointers to child nodes
	struct _NODE	*pNext;				// pointer to next reducible node
} NODE;

class CQuantizer
{
protected:
	NODE	*m_pTree;
	int		m_nLeafCount;
	NODE	*m_pReducibleNodes[9];
	int		m_nMaxColors;
	int		m_nColorBits;

public:
	CQuantizer( int nMaxColors, int nColorBits );
	virtual ~CQuantizer( void );
	void ProcessImage( unsigned char *pImage, int nWidth, int nHeight );
	int GetColorCount( void );
	void GetColorTable( unsigned char *prgb );

protected:
	int GetLeftShiftCount( int dwVal );
	int GetRightShiftCount( int dwVal );
	void AddColor( NODE **ppNode, unsigned char r, unsigned char g, unsigned char b, int nColorBits, int nLevel, int *pLeafCount, NODE **pReducibleNodes );
	NODE *CreateNode( int nLevel, int nColorBits, int *pLeafCount, NODE **pReducibleNodes );
	void ReduceTree( int nColorBits, int *pLeafCount, NODE **pReducibleNodes );
	void DeleteTree( NODE **ppNode );
	void GetPaletteColors( NODE *pTree, unsigned char *prgb, int *pIndex );
};

unsigned char FindNearestColor_guess( unsigned char *pPalette, int nColors, int r, int g, int b );
void QuantizeImage_guess( unsigned char *pImage, int nWidth, int nHeight, unsigned char *pIndices, unsigned char *pPalette, int nColors );

#endif // IGA_QUANTIZER_GUESS_H
