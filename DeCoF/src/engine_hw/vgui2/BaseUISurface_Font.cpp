//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: the engine's copy of the VGUI1 font: Windows GDI fonts and bitmap
//			fonts loaded from 32 bit RLE TGA files
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "BaseUISurface_Font.h"
#include <vgui/VGUI.h>
#include <vgui/Dar.h>
#include <vgui_controls/Controls.h>
#include "FileSystem.h"

#ifdef CreateFont
#undef CreateFont
#endif

using namespace vgui2;

// the font code allocates through the C heap
static inline void *operator new( size_t size )
{
	return malloc( size );
}

static inline void operator delete( void *pMem )
{
	free( pMem );
}

void EngineFont::GetTextSize( const char *text, int &wide, int &tall )
{
	wide = 0;
	tall = 0;

	if ( text == NULL )
		return;

	tall = GetTall();

	int xx = 0;
	for ( int i = 0; ; i++ )
	{
		char ch = text[i];
		if ( ch == 0 )
			break;

		if ( ch == '\n' )
		{
			tall += GetTall();
			xx = 0;
		}
		else if ( ch == '&' )
		{
			// underscore character, so skip
		}
		else
		{
			int a, b, c;
			GetCharABCwide( ch, a, b, c );
			xx += a + b + c;
			if ( xx > wide )
			{
				wide = xx;
			}
		}
	}
}

int EngineFont::getId()
{
	return _id;
}

BaseFontPlat *EngineFont::getPlat()
{
	return _plat;
}

static int staticFontId = 100;
static Dar<BaseFontPlat *> staticFontPlatDar;

// ------------------------------------------------------------------------------------ //
// Helper functions for loading bitmap fonts.
// ------------------------------------------------------------------------------------ //
class FileImageStream
{
public:
	virtual void Read( void *pOut, int len ) = 0;

	// Returns true if there were any Read errors.
	// Clears error status.
	virtual bool ErrorStatus() = 0;
};

// Use to read out of a memory buffer..
class FileImageStream_Memory : public FileImageStream
{
public:
	FileImageStream_Memory( void *pData, int dataLen );

	virtual void Read( void *pData, int len );
	virtual bool ErrorStatus();

private:
	unsigned char *m_pData;
	int m_DataLen;
	int m_CurPos;
	bool m_bError;
};

// Generic image representation..
class FileImage
{
public:
	FileImage()
	{
		Clear();
	}

	~FileImage()
	{
		Term();
	}

	void Term()
	{
		if ( m_pData )
			delete [] m_pData;

		Clear();
	}

	// Clear the structure without deallocating.
	void Clear()
	{
		m_Width = m_Height = 0;
		m_pData = NULL;
	}

	int m_Width, m_Height;
	unsigned char *m_pData;
};

// Load a TGA file into a FileImage.
bool Load32BitTGA( FileImageStream *fp, FileImage *pImage );

// Save a FileImage as a 32-bit TGA file.
void Save32BitTGA( FileHandle_t fp, FileImage *pImage );

class VFontData
{
public:
	VFontData();
	~VFontData();

	int GetLineWidth()
	{
		return m_BitmapCharWidth * 256;
	}

	int m_CharWidths[256];
	int m_BitmapCharWidth;
	int m_BitmapCharHeight;
	unsigned char *m_pBitmap;
};

bool LoadVFontDataFrom32BitTGA( FileImageStream *fp, VFontData *pData );

VFontData::VFontData()
{
	m_BitmapCharWidth = m_BitmapCharHeight = 0;
	m_pBitmap = NULL;
}

VFontData::~VFontData()
{
	if ( m_pBitmap )
		delete [] m_pBitmap;
}

bool LoadVFontDataFrom32BitTGA( FileImageStream *fp, VFontData *pData )
{
	FileImage fileImage;
	unsigned char *pInLine, *pOutLine;
	int i, x, y;
	int rightX;

	if ( !Load32BitTGA( fp, &fileImage ) )
		return false;

	pData->m_pBitmap = new unsigned char[fileImage.m_Width * fileImage.m_Height];
	if ( !pData->m_pBitmap )
		return false;

	pData->m_BitmapCharWidth = fileImage.m_Width / 256;
	pData->m_BitmapCharHeight = fileImage.m_Height;

	for ( i = 0; i < 256; i++ )
	{
		pInLine = &fileImage.m_pData[i * pData->m_BitmapCharWidth * 4];
		pOutLine = &pData->m_pBitmap[i * pData->m_BitmapCharWidth];

		rightX = 0;
		for ( y = 0; y < pData->m_BitmapCharHeight; y++ )
		{
			for ( x = 0; x < pData->m_BitmapCharWidth; x++ )
			{
				if ( *( (int *)&pInLine[x * 4] ) )
				{
					pOutLine[x] = 1;
					if ( x > rightX )
						rightX = x;
				}
				else
				{
					pOutLine[x] = 0;
				}
			}

			pInLine += 256 * pData->m_BitmapCharWidth * 4;
			pOutLine += 256 * pData->m_BitmapCharWidth;
		}

		// Ok, now we know the right-most pixel in the character.
		if ( i == ' ' )
			pData->m_CharWidths[i] = pData->m_BitmapCharWidth / 4;
		else
			pData->m_CharWidths[i] = rightX;
	}

	return true;
}

FileImageStream_Memory::FileImageStream_Memory( void *pData, int dataLen )
{
	m_pData = (unsigned char *)pData;
	m_DataLen = dataLen;
	m_CurPos = 0;
	m_bError = false;
}

void FileImageStream_Memory::Read( void *pData, int len )
{
	unsigned char *pOut;
	int i;

	pOut = (unsigned char *)pData;
	for ( i = 0; i < len; i++ )
	{
		if ( m_CurPos < m_DataLen )
		{
			pOut[i] = m_pData[m_CurPos];
			++m_CurPos;
		}
		else
		{
			pOut[i] = 0;
			m_bError = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	bool ret = m_bError;
	m_bError = false;
	return ret;
}

// TGA header.
#pragma pack( 1 )
class TGAFileHeader
{
public:
	unsigned char m_IDLength;
	unsigned char m_ColorMapType;
	unsigned char m_ImageType;
	unsigned short m_CMapStart;
	unsigned short m_CMapLength;
	unsigned char m_CMapDepth;
	unsigned short m_XOffset;
	unsigned short m_YOffset;
	unsigned short m_Width;
	unsigned short m_Height;
	unsigned char m_PixelDepth;
	unsigned char m_ImageDescriptor;
};
#pragma pack()

// Long story here..
static inline void WriteRun( unsigned char *pColor, FileHandle_t fp, int runLength )
{
	unsigned char runCount;

	runCount = runLength - 1;
	runCount |= ( 1 << 7 );
	filesystem()->Write( &runCount, 1, fp );
	filesystem()->Write( pColor, 4, fp );
}

// Load in a 32-bit TGA file.
bool Load32BitTGA( FileImageStream *fp, FileImage *pImage )
{
	TGAFileHeader hdr;
	char dummyChar;
	int i, x, y;
	long color;
	int runLength, curOut;
	unsigned char *pLine;
	unsigned char packetHeader;

	pImage->Term();

	// Read the header.
	fp->Read( &hdr, sizeof( hdr ) );

	if ( hdr.m_PixelDepth != 32 || hdr.m_ImageType != 10 )
		return false;

	// Skip the image ID.
	for ( i = 0; i < hdr.m_IDLength; i++ )
		fp->Read( &dummyChar, 1 );

	pImage->m_Width = hdr.m_Width;
	pImage->m_Height = hdr.m_Height;
	pImage->m_pData = new unsigned char[hdr.m_Width * hdr.m_Height * 4];
	if ( !pImage->m_pData )
		return false;

	// Read in the data..
	for ( y = pImage->m_Height - 1; y >= 0; y-- )
	{
		pLine = &pImage->m_pData[y * pImage->m_Width * 4];

		curOut = 0;
		while ( curOut < pImage->m_Width )
		{
			fp->Read( &packetHeader, 1 );

			runLength = (int)( packetHeader & ~( 1 << 7 ) ) + 1;
			if ( curOut + runLength > pImage->m_Width )
				return false;

			if ( packetHeader & ( 1 << 7 ) )
			{
				fp->Read( &color, 4 );
				for ( x = 0; x < runLength; x++ )
				{
					*( (long *)pLine ) = color;
					pLine += 4;
				}
			}
			else
			{
				for ( x = 0; x < runLength; x++ )
				{
					fp->Read( &color, 4 );
					*( (long *)pLine ) = color;
					pLine += 4;
				}
			}

			curOut += runLength;
		}
	}

	return true;
}

// Write out a 32-bit TGA file.
void Save32BitTGA( FileHandle_t fp, FileImage *pImage )
{
	TGAFileHeader hdr;
	int y, runStart, x;
	unsigned long *pLine;

	memset( &hdr, 0, sizeof( hdr ) );
	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;	// Run-length encoded RGB.
	hdr.m_Width = pImage->m_Width;
	hdr.m_Height = pImage->m_Height;

	filesystem()->Write( &hdr, sizeof( hdr ), fp );

	// Lines are written bottom-up.
	for ( y = pImage->m_Height - 1; y >= 0; y-- )
	{
		pLine = (unsigned long *)&pImage->m_pData[y * pImage->m_Width * 4];

		runStart = 0;
		for ( x = 0; x < pImage->m_Width; x++ )
		{
			if ( ( x - runStart ) >= 128 || pLine[runStart] != pLine[x] )
			{
				// Encode this Run.
				WriteRun( (unsigned char *)&pLine[runStart], fp, x - runStart );
				runStart = x;
			}
		}

		// Encode the last Run.
		if ( x - runStart > 0 )
		{
			WriteRun( (unsigned char *)&pLine[runStart], fp, x - runStart );
		}
	}
}

class FontPlat : public BaseFontPlat
{
public:
	FontPlat( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol, int id );
	~FontPlat();

	virtual bool equals( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol );
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba );
	virtual void GetCharABCwide( int ch, int &a, int &b, int &c );
	virtual int GetTall();
	virtual int getId();
	virtual void DrawSetTextFont( SurfacePlat *plat );

protected:
	HFONT m_hFont_guess;
	HDC m_hDC_guess;
	HBITMAP m_hDIB_guess;
	TEXTMETRIC tm_guess;
	ABC m_ABCWidths_guess[256];
	int m_rgiBitmapSize[2];
	unsigned char *m_pBuf;

	VFontData m_BitmapFont;
	bool m_bBitmapFont;

	char *_name;
	int _wide;
	int _tall;
	float _rotation;
	int _weight;
	bool _italic;
	bool _underline;
	bool _strikeout;
	bool _symbol;
	int _id;
};

FontPlat::FontPlat( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol, int id )
{
	_name = new char[strlen( name ) + 1];
	strcpy( _name, name );
	_tall = tall;
	_wide = wide;
	_rotation = rotation;
	_weight = weight;
	_italic = italic;
	_underline = underline;
	_strikeout = strikeout;
	_symbol = symbol;
	_id = id;

	int rot = (int)( rotation * 10 );
	int charset = ANSI_CHARSET;
	if ( symbol )
	{
		charset = SYMBOL_CHARSET;
	}

	m_hDC_guess = ::CreateCompatibleDC( NULL );
	m_hFont_guess = ::CreateFontA( tall, wide, rot, rot, weight, italic, underline, strikeout, charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, name );
	::SelectObject( m_hDC_guess, m_hFont_guess );
	::SetTextAlign( m_hDC_guess, TA_UPDATECP );
	::GetTextMetrics( m_hDC_guess, &tm_guess );

	m_rgiBitmapSize[0] = tm_guess.tmMaxCharWidth;
	m_rgiBitmapSize[1] = tm_guess.tmAscent + tm_guess.tmHeight + tm_guess.tmDescent;

	// create a 32bpp DIB
	BITMAPINFOHEADER header;
	memset( &header, 0, sizeof( header ) );
	header.biSize = sizeof( header );
	header.biWidth = m_rgiBitmapSize[0];
	header.biHeight = -m_rgiBitmapSize[1];
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;

	m_hDIB_guess = ::CreateDIBSection( m_hDC_guess, (BITMAPINFO *)&header, DIB_RGB_COLORS, (void **)&m_pBuf, NULL, 0 );
	::SelectObject( m_hDC_guess, m_hDIB_guess );

	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	for ( int i = 0; i < 256; i++ )
	{
		m_ABCWidths_guess[i].abcA = 0;
		m_ABCWidths_guess[i].abcB = 0;
		m_ABCWidths_guess[i].abcC = 0;
	}

	if ( !::GetCharABCWidths( m_hDC_guess, 0, 255, m_ABCWidths_guess ) )
	{
		// since that failed, it must be fixed width, zero everything so a and c will be zeros, then
		// fill b with the value from TEXTMETRIC
		for ( int j = 0; j < 256; j++ )
		{
			m_ABCWidths_guess[j].abcB = tm_guess.tmAveCharWidth;
		}
	}
}

FontPlat::~FontPlat()
{
	delete [] _name;
	::DeleteObject( m_hFont_guess );
	::DeleteDC( m_hDC_guess );
	::DeleteObject( m_hDIB_guess );
}

bool FontPlat::equals( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	if ( _stricmp( name, _name ) )
		return false;

	if ( tall != _tall )
		return false;

	if ( wide != _wide )
		return false;

	if ( rotation != _rotation )
		return false;

	if ( weight != _weight )
		return false;

	if ( italic != _italic )
		return false;

	if ( underline != _underline )
		return false;

	if ( strikeout != _strikeout )
		return false;

	if ( symbol != _symbol )
		return false;

	return true;
}

void FontPlat::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba )
{
	::SelectObject( m_hDC_guess, m_hFont_guess );
	::SetBkColor( m_hDC_guess, RGB( 0, 0, 0 ) );
	::SetTextColor( m_hDC_guess, RGB( 255, 255, 255 ) );
	::SetBkMode( m_hDC_guess, OPAQUE );
	::MoveToEx( m_hDC_guess, -m_ABCWidths_guess[ch].abcA, 0, NULL );

	// render the character
	char wch = (char)ch;
	::ExtTextOutA( m_hDC_guess, 0, 0, 0, NULL, &wch, 1, NULL );
	::SetBkMode( m_hDC_guess, TRANSPARENT );

	int wide = m_ABCWidths_guess[ch].abcB;
	int tall = tm_guess.tmHeight;

	if ( wide > m_rgiBitmapSize[0] )
	{
		wide = m_rgiBitmapSize[0];
	}

	if ( tall > m_rgiBitmapSize[1] )
	{
		tall = m_rgiBitmapSize[1];
	}

	// iterate through copying the generated dib into the texture
	for ( int j = 0; j < tall; j++ )
	{
		for ( int i = 0; i < wide; i++ )
		{
			int x = rgbaX + i;
			int y = rgbaY + j;
			if ( ( x < rgbaWide ) && ( y < rgbaTall ) )
			{
				float r = ( m_pBuf[( j * m_rgiBitmapSize[0] + i ) * 4 + 0] ) / 255.0f;
				float g = ( m_pBuf[( j * m_rgiBitmapSize[0] + i ) * 4 + 1] ) / 255.0f;
				float b = ( m_pBuf[( j * m_rgiBitmapSize[0] + i ) * 4 + 2] ) / 255.0f;

				// Don't want anything drawn for tab characters.
				if ( ch == '\t' )
				{
					r = g = b = 0;
				}

				rgba[( y * rgbaWide + x ) * 4 + 0] = (unsigned char)( r * 255.0f );
				rgba[( y * rgbaWide + x ) * 4 + 1] = (unsigned char)( g * 255.0f );
				rgba[( y * rgbaWide + x ) * 4 + 2] = (unsigned char)( b * 255.0f );
				rgba[( y * rgbaWide + x ) * 4 + 3] = (unsigned char)( ( r * 0.34f + g * 0.55f + b * 0.11f ) * 255.0f );
			}
		}
	}
}

void FontPlat::GetCharABCwide( int ch, int &a, int &b, int &c )
{
	a = m_ABCWidths_guess[ch].abcA;
	b = m_ABCWidths_guess[ch].abcB;
	c = m_ABCWidths_guess[ch].abcC;
}

int FontPlat::GetTall()
{
	return tm_guess.tmHeight;
}

int FontPlat::getId()
{
	return _id;
}

void FontPlat::DrawSetTextFont( SurfacePlat *plat )
{
}

class FontPlat_Bitmap : public BaseFontPlat
{
private:
	FontPlat_Bitmap();

public:
	virtual ~FontPlat_Bitmap();
	static FontPlat_Bitmap *Create( const char *name, FileImageStream *pStream, int id );

	virtual bool equals( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol );
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba );
	virtual void GetCharABCwide( int ch, int &a, int &b, int &c );
	virtual int GetTall();
	virtual int getId();
	virtual void DrawSetTextFont( SurfacePlat *plat );

private:
	VFontData m_FontData;
	char *m_pName;
	int _id;
};

FontPlat_Bitmap::FontPlat_Bitmap()
{
	m_pName = NULL;
}

FontPlat_Bitmap::~FontPlat_Bitmap()
{
	if ( m_pName )
		delete [] m_pName;
}

FontPlat_Bitmap *FontPlat_Bitmap::Create( const char *name, FileImageStream *pStream, int id )
{
	FontPlat_Bitmap *pBitmap = new FontPlat_Bitmap;
	if ( pBitmap )
	{
		if ( LoadVFontDataFrom32BitTGA( pStream, &pBitmap->m_FontData ) )
		{
			pBitmap->m_pName = new char[strlen( name ) + 1];
			if ( pBitmap->m_pName )
			{
				strcpy( pBitmap->m_pName, name );
			}
			else
			{
				delete pBitmap;
				return NULL;
			}

			pBitmap->_id = id;
			return pBitmap;
		}
		else
		{
			delete pBitmap;
		}
	}

	return NULL;
}

bool FontPlat_Bitmap::equals( const char *name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	return !_stricmp( name, m_pName );
}

void FontPlat_Bitmap::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba )
{
	unsigned char *pSrcPos;
	uchar *pOutPos;
	int x, y, outX, outY;

	if ( ch >= 256 )
		ch = 256;
	else if ( ch < 0 )
		ch = 0;

	for ( y = 0; y < m_FontData.m_BitmapCharHeight; y++ )
	{
		pSrcPos = &m_FontData.m_pBitmap[y * m_FontData.GetLineWidth() + ch * m_FontData.m_BitmapCharWidth];
		outY = rgbaY + y;

		for ( x = 0; x < m_FontData.m_BitmapCharWidth; x++ )
		{
			outX = rgbaX + x;
			if ( outX < rgbaWide && outY < rgbaTall )
			{
				pOutPos = &rgba[( outY * rgbaWide + outX ) * 4];
				if ( *pSrcPos )
				{
					pOutPos[0] = pOutPos[1] = pOutPos[2] = pOutPos[3] = 255;
				}
				else
				{
					pOutPos[0] = pOutPos[1] = pOutPos[2] = pOutPos[3] = 0;
				}
			}

			++pSrcPos;
		}
	}
}

void FontPlat_Bitmap::GetCharABCwide( int ch, int &a, int &b, int &c )
{
	if ( ch >= 256 )
		ch = 255;
	else if ( ch < 0 )
		ch = 0;

	a = c = 0;
	b = m_FontData.m_CharWidths[ch] + 1;
}

int FontPlat_Bitmap::GetTall()
{
	return m_FontData.m_BitmapCharHeight;
}

int FontPlat_Bitmap::getId()
{
	return _id;
}

void FontPlat_Bitmap::DrawSetTextFont( SurfacePlat *plat )
{
}

static int s_iFontCount = 0;

EngineFont::EngineFont( const char *name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	Init( name, pFileData, fileDataLen, tall, wide, rotation, weight, italic, underline, strikeout, symbol );
	s_iFontCount++;
}

EngineFont::~EngineFont()
{
	delete [] _name;

	s_iFontCount--;
	if ( s_iFontCount == 0 )
	{
		Font_Reset();
	}
}

void EngineFont::Init( const char *name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol )
{
	FontPlat_Bitmap *pBitmapFont;
	int i;

	_name = new char[strlen( name ) + 1];
	strcpy( _name, name );

	_id = -1;

	// If a bitmap font was passed in, load it.
	if ( pFileData )
	{
		FileImageStream_Memory memStream( pFileData, fileDataLen );

		_id = staticFontId;
		pBitmapFont = FontPlat_Bitmap::Create( name, &memStream, _id );
		if ( pBitmapFont )
		{
			_plat = pBitmapFont;
			staticFontPlatDar.AddElement( pBitmapFont );
			staticFontId++;
		}
	}
	else
	{
		_plat = NULL;
		for ( i = 0; i < staticFontPlatDar.GetCount(); i++ )
		{
			if ( staticFontPlatDar[i]->equals( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol ) )
			{
				_plat = staticFontPlatDar[i];
				_id = _plat->getId();
				break;
			}
		}

		if ( !_plat )
		{
			_id = staticFontId++;
			_plat = new FontPlat( name, tall, wide, rotation, weight, italic, underline, strikeout, symbol, _id );
			staticFontPlatDar.AddElement( _plat );
		}
	}
}

void EngineFont::getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar *rgba )
{
	_plat->getCharRGBA( ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba );
}

void EngineFont::GetCharABCwide( int ch, int &a, int &b, int &c )
{
	_plat->GetCharABCwide( ch, a, b, c );
}

int EngineFont::GetTall()
{
	return _plat->GetTall();
}

void Font_Reset()
{
	for ( int i = 0; i < staticFontPlatDar.GetCount(); i++ )
	{
		delete staticFontPlatDar[i];
	}

	staticFontPlatDar.SetCount( 0 );
}
