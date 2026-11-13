//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Bit and byte stream used by the HLTV proxy code
//
// $NoKeywords: $
//=============================================================================

#ifndef BITBUFFER_H
#define BITBUFFER_H

#ifdef _WIN32
#pragma once
#endif

class BitBuffer
{
public:
	BitBuffer();
	BitBuffer( unsigned int size );
	BitBuffer( void *newData, unsigned int size );
	virtual ~BitBuffer();

	unsigned int	PeekBits( int numbits );
	int				CurrentBit();
	int				CurrentSize();
	void			SetBuffer( void *buffer, int size );
	void			Free();
	void			Reset();
	void			Clear();
	void			FastClear();
	int				SpaceLeft();
	void			EndBitMode();
	void			StartBitMode();
	bool			Resize( unsigned int size );
	void			ConcatBuffer( BitBuffer *buffer );
	int				SkipString();
	void			SkipBits( int n );
	void			SkipBytes( int n );
	void			AlignByte();

	unsigned int	ReadBits( int n );
	int				ReadBit();
	int				ReadChar();
	int				ReadByte();
	int				ReadShort();
	int				ReadWord();
	unsigned int	ReadLong();
	float			ReadFloat();
	char			*ReadString();
	char			*ReadStringLine();
	char			*ReadBitString();
	int				ReadSBits( int numbits );
	float			ReadBitAngle( int numbits );
	int				ReadBitData( void *dest, int length );
	bool			ReadBuf( int iSize, void *pbuf );
	float			ReadAngle();
	float			ReadHiresAngle();
	float			ReadCoord();
	float			ReadBitCoord();
	void			ReadBitVec3Coord( float *fa );

	void			WriteBits( unsigned int bits, int n );
	void			WriteBit( int c );
	void			WriteChar( int c );
	void			WriteByte( int c );
	void			WriteShort( int c );
	void			WriteWord( int c );
	void			WriteLong( unsigned int c );
	void			WriteFloat( float f );
	void			WriteString( const char *s );
	void			WriteBitString( const char *s );
	void			WriteSBits( int bits, int numbits );
	void			WriteBitAngle( float fAngle, int numbits );
	void			WriteBitData( void *src, int length );
	void			WriteBuf( const void *buf, int iSize );
	void			WriteBuf( BitBuffer *buf, int length );
	void			WriteAngle( float f );
	void			WriteHiresAngle( float f );
	void			WriteCoord( float f );

	bool			sizeError;
	unsigned char	*data;
	unsigned char	*currentByte;
	int				currentBit;
	int				maxSize;
	bool			littleEndian;
	bool			ownData;
};

#endif // BITBUFFER_H
