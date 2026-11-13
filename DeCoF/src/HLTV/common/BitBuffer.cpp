//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Bit and byte stream used by the HLTV proxy code
//
// $NoKeywords: $
//=============================================================================

#include <stdlib.h>
#include <string.h>

#include "Bitbuffer.h"
#include "byteorder.h"

void *Mem_ZeroMalloc( unsigned int size );

static const unsigned int BITTABLE[33] =
{
	0x00000001, 0x00000002, 0x00000004, 0x00000008,
	0x00000010, 0x00000020, 0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
	0x00000000,
};

static const unsigned int ROWBITTABLE[33] =
{
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F,
	0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
	0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF,
	0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF,
	0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
	0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF,
	0x0FFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

static const unsigned int INVBITTABLE[33] =
{
	0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFB, 0xFFFFFFF7,
	0xFFFFFFEF, 0xFFFFFFDF, 0xFFFFFFBF, 0xFFFFFF7F,
	0xFFFFFEFF, 0xFFFFFDFF, 0xFFFFFBFF, 0xFFFFF7FF,
	0xFFFFEFFF, 0xFFFFDFFF, 0xFFFFBFFF, 0xFFFF7FFF,
	0xFFFEFFFF, 0xFFFDFFFF, 0xFFFBFFFF, 0xFFF7FFFF,
	0xFFEFFFFF, 0xFFDFFFFF, 0xFFBFFFFF, 0xFF7FFFFF,
	0xFEFFFFFF, 0xFDFFFFFF, 0xFBFFFFFF, 0xF7FFFFFF,
	0xEFFFFFFF, 0xDFFFFFFF, 0xBFFFFFFF, 0x7FFFFFFF,
	0xFFFFFFFF,
};

BitBuffer::BitBuffer()
{
	memset( this, 0, sizeof( BitBuffer ) );
}

BitBuffer::BitBuffer( void *newData, unsigned int size )
{
	memset( this, 0, sizeof( BitBuffer ) );

	data = (unsigned char *)newData;
	maxSize = size;
	currentByte = data;
	littleEndian = true;
}

BitBuffer::~BitBuffer()
{
	Free();
}

BitBuffer::BitBuffer( unsigned int size )
{
	memset( this, 0, sizeof( BitBuffer ) );
	Resize( size );
}

bool BitBuffer::Resize( unsigned int size )
{
	Free();

	data = (unsigned char *)Mem_ZeroMalloc( size + 4 );

	if ( !data )
	{
		maxSize = 0;
		currentByte = NULL;
		currentBit = 0;
		sizeError = false;
		ownData = false;
		return false;
	}

	maxSize = size;
	currentByte = data;
	currentBit = 0;
	sizeError = false;
	ownData = true;
	littleEndian = true;
	return true;
}

void BitBuffer::Clear()
{
	memset( data, 0, maxSize );

	currentByte = data;
	currentBit = 0;
	sizeError = false;
	littleEndian = true;
}

int BitBuffer::CurrentBit()
{
	return currentBit + 8 * ( currentByte - data );
}

void BitBuffer::Reset()
{
	currentByte = data;
	currentBit = 0;
	sizeError = false;
	littleEndian = true;
}

void BitBuffer::Free()
{
	if ( data && ownData )
		free( data );

	maxSize = 0;
	data = NULL;
	currentByte = NULL;
	currentBit = 0;
	ownData = false;
	sizeError = false;
	littleEndian = true;
}

unsigned int BitBuffer::ReadBits( int n )
{
	unsigned int bits, post;

	if ( littleEndian )
	{
		if ( currentByte - data >= maxSize )
		{
			sizeError = true;
			return -1;
		}

		int d = n + currentBit;

		if ( d > 32 )
		{
			bits = *(unsigned int *)currentByte >> currentBit;
			currentByte += 4;
			post = *(unsigned int *)currentByte;
			d &= 7;
			bits |= ( post & ROWBITTABLE[d] ) << ( 32 - currentBit );
			currentBit = d;
		}
		else
		{
			bits = ( *(unsigned int *)currentByte >> currentBit ) & ROWBITTABLE[n];

			currentByte += n >> 3;
			currentBit += n & 7;

			if ( currentBit > 7 )
			{
				currentBit &= 7;
				currentByte++;
			}
		}
	}
	else
	{
		bits = 0;

		while ( n-- > 0 )
		{
			if ( ReadBit() )
				bits |= 1 << n;
		}
	}

	return bits;
}

int BitBuffer::ReadBit()
{
	if ( currentByte - data >= maxSize )
	{
		sizeError = true;
		return -1;
	}

	if ( littleEndian )
	{
		if ( currentBit == 7 )
		{
			currentBit = 0;
			return ( *currentByte++ >> 7 ) & 1;
		}
		else
		{
			return ( *currentByte >> currentBit++ ) & 1;
		}
	}
	else
	{
		if ( currentBit == 7 )
		{
			currentBit = 0;
			return *currentByte++ & 1;
		}
		else
		{
			return ( *currentByte >> ( 7 - currentBit++ ) ) & 1;
		}
	}
}

unsigned int BitBuffer::PeekBits( int numbits )
{
	int oldcurrentBit = currentBit;
	unsigned char *oldcurrentByte = currentByte;

	unsigned int bits = ReadBits( numbits );

	currentBit = oldcurrentBit;
	currentByte = oldcurrentByte;

	return bits;
}

int BitBuffer::ReadChar()
{
	return ReadBits( 8 );
}

int BitBuffer::ReadByte()
{
	return ReadBits( 8 );
}

int BitBuffer::ReadShort()
{
	return ReadBits( 16 );
}

int BitBuffer::ReadWord()
{
	return ReadBits( 16 );
}

unsigned int BitBuffer::ReadLong()
{
	return ReadBits( 32 );
}

float BitBuffer::ReadFloat()
{
	union
	{
		unsigned char	b[4];
		float			f;
		int				l;
	} dat;

	dat.l = ReadBits( 32 );
	dat.l = LittleLong( dat.l );

	return dat.f;
}

bool BitBuffer::ReadBuf( int iSize, void *pbuf )
{
	if ( currentByte - data + iSize > maxSize )
	{
		sizeError = true;
		return false;
	}

	if ( !currentBit )
	{
		memcpy( pbuf, currentByte, iSize );
		currentByte += iSize;
	}
	else
	{
		int i;
		unsigned int *p = (unsigned int *)pbuf;

		for ( i = 4; i < iSize; i += 4 )
		{
			*p = ReadBits( 32 );
			p++;
		}

		iSize = iSize + 4 - i;

		for ( i = 0; i < iSize; i++ )
		{
			*( (unsigned char *)p + i ) = ReadBits( 8 );
		}
	}

	return true;
}

void BitBuffer::WriteBuf( BitBuffer *buf, int length )
{
	WriteBuf( buf->currentByte, length );
	buf->SkipBytes( length );
}

char *BitBuffer::ReadString()
{
	static char string[8192];
	int l, c;

	l = 0;

	do
	{
		c = ReadBits( 8 );

		if ( c == -1 || c == 0 )
			break;

		string[l] = c;
		l++;
	} while ( l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

char *BitBuffer::ReadStringLine()
{
	static char string[2048];
	int l, c;

	l = 0;

	do
	{
		c = ReadBits( 8 );

		if ( c == -1 || c == 0 || c == '\n' )
			break;

		string[l] = c;
		l++;
	} while ( l < sizeof( string ) - 1 );

	string[l] = 0;

	return string;
}

float BitBuffer::ReadAngle()
{
	return (float)( ReadChar() * ( 360.0 / 256 ) );
}

float BitBuffer::ReadHiresAngle()
{
	return (float)( ReadShort() * ( 360.0 / 65536 ) );
}

void BitBuffer::WriteBit( int c )
{
	if ( currentByte - data >= maxSize )
	{
		sizeError = true;
		return;
	}

	if ( littleEndian )
	{
		if ( currentBit == 7 )
		{
			if ( c )
				currentByte[0] |= 0x80;
			else
				currentByte[0] &= 0x7F;

			currentByte++;
			currentBit = 0;
		}
		else
		{
			if ( c )
			{
				currentByte[0] |= BITTABLE[currentBit];
				currentBit++;
			}
			else
			{
				currentByte[0] &= INVBITTABLE[currentBit];
				currentBit++;
			}
		}
	}
	else
	{
		static unsigned char masks[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
		static unsigned char inv_masks[8] = { 0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE };

		if ( c )
		{
			currentByte[0] |= masks[currentBit];
			currentBit++;
		}
		else
		{
			currentByte[0] &= inv_masks[currentBit];
			currentBit++;
		}

		if ( currentBit == 8 )
		{
			currentBit = 0;
			currentByte++;
		}
	}
}

void BitBuffer::WriteBits( unsigned int bits, int n )
{
	if ( sizeError )
		return;

	if ( littleEndian )
	{
		if ( currentByte - data + ( n >> 8 ) > maxSize )
		{
			sizeError = true;
			return;
		}

		int d = n + currentBit;

		if ( d > 32 )
		{
			bits &= ROWBITTABLE[n];
			*(unsigned int *)currentByte |= bits << currentBit;
			bits = bits >> ( 32 - currentBit );
			currentBit = d & 7;
			currentByte += 4;
			*(unsigned int *)currentByte |= bits;
		}
		else
		{
			bits &= ROWBITTABLE[n];
			*(unsigned int *)currentByte |= bits << currentBit;
			currentByte += n >> 3;
			currentBit += n & 7;

			if ( currentBit > 7 )
			{
				currentBit &= 7;
				currentByte++;
			}
		}
	}
	else
	{
		if ( n < 32 && bits >= (unsigned int)( 1 << n ) && bits != -1 )
		{
			bits = ( 1 << n ) - 1;
		}

		while ( n-- > 0 )
		{
			int nBitValue = bits & ( 1 << n );
			WriteBit( nBitValue );
		}
	}
}

void BitBuffer::WriteSBits( int bits, int numbits )
{
	unsigned int uidata;

	if ( numbits < 32 )
	{
		int sigbits = numbits - 1;
		int maxnum = ( 1 << sigbits ) - 1;

		if ( bits > maxnum )
			bits = maxnum;
		else if ( bits < -maxnum )
			bits = -maxnum;
	}

	if ( bits < 0 )
		WriteBit( 1 );
	else
		WriteBit( 0 );

	uidata = abs( bits );
	WriteBits( uidata, numbits - 1 );
}

void BitBuffer::WriteChar( int c )
{
	WriteBits( c, 8 );
}

void BitBuffer::WriteByte( int c )
{
	WriteBits( c, 8 );
}

void BitBuffer::WriteShort( int c )
{
	WriteBits( c, 16 );
}

void BitBuffer::WriteWord( int c )
{
	WriteBits( c, 16 );
}

void BitBuffer::WriteLong( unsigned int c )
{
	WriteBits( c, 32 );
}

void BitBuffer::WriteFloat( float f )
{
	union
	{
		unsigned char	b[4];
		float			f;
		int				l;
	} dat;

	dat.f = f;
	dat.l = LittleLong( dat.l );

	WriteBits( dat.l, 32 );
}

void BitBuffer::WriteString( const char *s )
{
	if ( !s )
	{
		WriteChar( 0 );
	}
	else
	{
		WriteBuf( s, strlen( s ) + 1 );
	}
}

void BitBuffer::WriteBuf( const void *buf, int iSize )
{
	if ( !buf || sizeError || !iSize )
		return;

	if ( currentByte - data + iSize > maxSize )
	{
		sizeError = true;
		return;
	}

	if ( !currentBit )
	{
		memcpy( currentByte, buf, iSize );
		currentByte += iSize;
	}
	else
	{
		int i;
		unsigned int *p = (unsigned int *)buf;

		for ( i = 4; i < iSize; i += 4 )
		{
			WriteBits( *p, 32 );
			p++;
		}

		iSize = iSize + 4 - i;

		for ( i = 0; i < iSize; i++ )
		{
			WriteBits( *( (unsigned char *)p + i ), 8 );
		}
	}
}

void BitBuffer::WriteBitData( void *src, int length )
{
	int i;
	unsigned char *p = (unsigned char *)src;

	for ( i = 0; i < length; i++ )
	{
		WriteBits( *p, 8 );
		p++;
	}
}

void BitBuffer::WriteAngle( float f )
{
	WriteByte( (int)( f * 256 / 360 ) & 255 );
}

void BitBuffer::WriteHiresAngle( float f )
{
	WriteShort( (int)( f * 65536 / 360 ) & 65535 );
}

int BitBuffer::CurrentSize()
{
	return currentByte - data + ( currentBit != 0 );
}

int BitBuffer::SpaceLeft()
{
	return maxSize - ( currentByte - data );
}

void BitBuffer::AlignByte()
{
	if ( currentBit )
	{
		currentByte++;
		currentBit = 0;
	}
}

int BitBuffer::ReadSBits( int numbits )
{
	int r, nSignBit;

	nSignBit = ReadBit();
	r = ReadBits( numbits - 1 );

	if ( nSignBit )
		r = -r;

	return r;
}

float BitBuffer::ReadBitAngle( int numbits )
{
	float fReturn;
	unsigned int ui;
	float shift;

	shift = (float)( 1 << numbits );

	ui = ReadBits( numbits );

	fReturn = (float)ui * (float)( 360.0 / shift );

	return fReturn;
}

void BitBuffer::WriteBitAngle( float fAngle, int numbits )
{
	int d;
	unsigned int mask;
	unsigned int shift;

	if ( numbits < 32 )
	{
		shift = 1 << numbits;
		mask = shift - 1;
	}
	else
	{
		sizeError = true;
		return;
	}

	d = (int)( shift * fAngle ) / 360;
	d &= mask;

	WriteBits( d, numbits );
}

char *BitBuffer::ReadBitString()
{
	static char buf[8192];
	char *p;
	unsigned int c;

	buf[0] = 0;
	p = buf;

	do
	{
		c = ReadBits( 8 );
		*p = c;
		p++;
	} while ( c );

	return buf;
}

void BitBuffer::WriteBitString( const char *s )
{
	char *pch = (char *)s;

	while ( *pch )
	{
		WriteBits( *pch, 8 );
		pch++;
	}

	WriteBits( 0, 8 );
}

void BitBuffer::StartBitMode()
{
	if ( currentBit )
		sizeError = true;
}

void BitBuffer::EndBitMode()
{
	AlignByte();
}

int BitBuffer::ReadBitData( void *dest, int length )
{
	int i;
	int readlen = 0;
	unsigned char *p = (unsigned char *)dest;

	for ( i = 0; i < length; i++ )
	{
		*p = ReadBits( 8 );
		p++;
	}

	return length;
}

void BitBuffer::SetBuffer( void *buffer, int size )
{
	Free();

	data = (unsigned char *)buffer;
	maxSize = size;
	currentByte = data;
	currentBit = 0;
	sizeError = false;
	ownData = false;
	littleEndian = true;
}

void BitBuffer::ReadBitVec3Coord( float *fa )
{
	int xflag, yflag, zflag;

	xflag = ReadBit();
	yflag = ReadBit();
	zflag = ReadBit();

	if ( xflag )
		fa[0] = ReadBitCoord();
	if ( yflag )
		fa[1] = ReadBitCoord();
	if ( zflag )
		fa[2] = ReadBitCoord();
}

float BitBuffer::ReadBitCoord()
{
	int intval = 0, fractval = 0, signbit = 0;
	float value = 0.0;

	// Read the required integer and fraction flags
	intval = ReadBit();
	fractval = ReadBit();

	// If we got either parse them, otherwise it's a zero.
	if ( intval || fractval )
	{
		// Read the sign bit
		signbit = ReadBit();

		// If there's an integer, read it in
		if ( intval )
		{
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			intval = ReadBits( 12 );
		}

		// If there's a fraction, read it in
		if ( fractval )
		{
			fractval = ReadBits( 3 );
		}

		// Calculate the correct floating point value
		value = intval + ( (float)fractval * ( 1.0f / 8 ) );

		// Fixup the sign if negative.
		if ( signbit )
			value = -value;
	}

	return value;
}

float BitBuffer::ReadCoord()
{
	float f = ReadShort() / 8.0f;
	return f;
}

void BitBuffer::SkipBytes( int n )
{
	if ( currentByte - data + n > maxSize )
		sizeError = true;

	currentByte += n;
}

void BitBuffer::SkipBits( int n )
{
	if ( littleEndian )
	{
		if ( currentByte - data >= maxSize )
		{
			sizeError = true;
			return;
		}

		int d = n + currentBit;

		if ( d > 32 )
		{
			currentByte += 4;
			currentBit = d & 7;
		}
		else
		{
			currentByte += n >> 3;
			currentBit += n & 7;

			if ( currentBit > 7 )
			{
				currentBit &= 7;
				currentByte++;
			}
		}
	}
	else
	{
		while ( n-- > 0 )
		{
			if ( currentBit == 7 )
			{
				currentBit = 0;
				currentByte++;
			}
			else
			{
				currentBit++;
			}
		}
	}
}

int BitBuffer::SkipString()
{
	int c;
	int l = 1;

	do
	{
		c = ReadBits( 8 );

		if ( c == -1 || c == 0 )
			break;

		l++;
	} while ( l < 8192 );

	return l;
}

void BitBuffer::FastClear()
{
	memset( data, 0, ( CurrentSize() + 4 < maxSize ) ? CurrentSize() + 4 : maxSize );

	currentByte = data;
	currentBit = 0;
	sizeError = false;
	littleEndian = true;
}

void BitBuffer::ConcatBuffer( BitBuffer *buffer )
{
	WriteBuf( buffer->data, buffer->CurrentSize() );
}

void BitBuffer::WriteCoord( float f )
{
	WriteShort( (int)( f * 8.0f ) );
}
