//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $Header: $
// $NoKeywords: $
//
// Serialization buffer
//=============================================================================

#ifndef UTLBUFFER_H
#define UTLBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "UtlMemory.h"
#include <stdarg.h>

//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------
class CUtlBuffer
{
public:
	enum SeekType_t
	{
		SEEK_HEAD = 0,
		SEEK_CURRENT,
		SEEK_TAIL
	};

	CUtlBuffer( int growSize = 0, int initSize = 0, bool text = false );
	CUtlBuffer( void const* pBuffer, int size, bool text = false );

	// Makes sure we've got at least this much memory
	void			EnsureCapacity( int num );

	// Read stuff out.
	// Binary mode: it'll just read the bits directly in, and characters will be
	//		read for strings until a null character is reached.
	// Text mode: it'll parse the file, turning text #s into real numbers.
	//		GetString will read a string until a space is reached
	char			GetChar( );
	unsigned char	GetUnsignedChar( );
	short			GetShort( );
	unsigned short	GetUnsignedShort( );
	int				GetInt( );
	unsigned int	GetUnsignedInt( );
	float			GetFloat( );
	double			GetDouble( );
	void			GetString( char* pString );
	void			Get( void* pMem, int size );

	// Just like scanf, but doesn't work in binary mode
	int				Scanf( char const* pFmt, ... );
	int				VaScanf( char const* pFmt, va_list list );

	// Eats white space, advances Get index
	void			EatWhiteSpace();

	// Write stuff in
	// Binary mode: it'll just write the bits directly in, and strings will be
	//		written with a null terminating character
	// Text mode: it'll convert the numbers to text versions
	//		PutString will not write a terminating character
	void			PutChar( char c );
	void			PutUnsignedChar( unsigned char uc );
	void			PutShort( short s );
	void			PutUnsignedShort( unsigned short us );
	void			PutInt( int i );
	void			PutUnsignedInt( unsigned int u );
	void			PutFloat( float f );
	void			PutDouble( double d );
	void			PutString( char const* pString );
	void			Put( void const* pMem, int size );

	// Just like printf, writes a terminating zero in binary mode
	void			Printf( char const* pFmt, ... );
	void			VaPrintf( char const* pFmt, va_list list );

	// What am I writing (put)/reading (get)?
	void*			PeekPut( int offset = 0 );
	void const*		PeekGet( int offset = 0 ) const;

	// Where am I writing (put)/reading (get)?
	int				TellPut( ) const;
	int				TellGet( ) const;

	// Change where I'm writing (put)/reading (get)
	void			SeekPut( SeekType_t type, int offset );
	void			SeekGet( SeekType_t type, int offset );

	// Buffer base
	void const*		Base() const;
	void*			Base();

	// memory allocation size, does *not* reflect size written or read,
	//	use TellPut or TellGet for that
	int				Size() const;

	// Am I a text buffer?
	bool			IsText() const;

	// Am I valid? (overflow or underflow error), Once invalid it stays invalid
	bool			IsValid() const;

	// Clears out the buffer; frees memory
	void			Clear();

private:
	// error flags
	enum
	{
		PUT_OVERFLOW = 0x1,
		GET_OVERFLOW = 0x2,
	};

	// flags
	enum
	{
		TEXT_BUFFER = 0x1,
	};

	// Checks if a get/put is ok
	bool CheckPut( int size );
	bool CheckGet( int size );

	CUtlMemory<unsigned char> m_Memory;
	int m_Get;
	int m_Put;
	unsigned char m_Error;
	unsigned char m_Flags;
};

//-----------------------------------------------------------------------------
// Where am I reading?
//-----------------------------------------------------------------------------
inline void const* CUtlBuffer::PeekGet( int offset ) const
{
	return &m_Memory[m_Get + offset];
}

//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------
#define GET_TYPE( _type, _val, _fmt )	\
	if (!IsText())						\
	{									\
		if (CheckGet( sizeof(_type) ))	\
		{								\
			_val = *(_type *)PeekGet();	\
			m_Get += sizeof(_type);		\
		}								\
		else							\
		{								\
			_val = 0;					\
		}								\
	}									\
	else								\
	{									\
		Scanf( _fmt, &_val );			\
	}

inline int CUtlBuffer::GetInt( )
{
	int i;
	GET_TYPE( int, i, "%d" );
	return i;
}

//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline void* CUtlBuffer::PeekPut( int offset )
{
	return &m_Memory[m_Put + offset];
}

inline int CUtlBuffer::TellGet( ) const
{
	return m_Get;
}

//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline int CUtlBuffer::TellPut( ) const
{
	return m_Put;
}

//-----------------------------------------------------------------------------
// Buffer base and size
//-----------------------------------------------------------------------------
inline void* CUtlBuffer::Base()
{
	return m_Memory.Base();
}

//-----------------------------------------------------------------------------
// Am I a text buffer?
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsText() const
{
	return (m_Flags & TEXT_BUFFER) != 0;
}

//-----------------------------------------------------------------------------
// Am I valid? (overflow or underflow error), Once invalid it stays invalid
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsValid() const
{
	return m_Error == 0;
}

//-----------------------------------------------------------------------------
// memory allocation size, does *not* reflect size written or read
//-----------------------------------------------------------------------------
inline int CUtlBuffer::Size() const
{
	return m_Memory.NumAllocated();
}

//-----------------------------------------------------------------------------
// Writes a character
//-----------------------------------------------------------------------------
inline void CUtlBuffer::PutChar( char c )
{
	if ( !IsText() )
	{
		if ( CheckPut( sizeof(char) ) )
		{
			*(char*)PeekPut() = c;
			++m_Put;
		}
	}
	else
	{
		Printf( "%c", c );
	}
}

//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------
#define PUT_TYPE( _type, _val, _fmt )	\
	if (!IsText())						\
	{									\
		if (CheckPut( sizeof(_type) ))	\
		{								\
			*(_type *)PeekPut() = _val;	\
			m_Put += sizeof(_type);		\
		}								\
	}									\
	else								\
	{									\
		Printf( _fmt, _val );			\
	}

inline void CUtlBuffer::PutInt( int i )
{
	PUT_TYPE( int, i, "%d" );
}

#endif // UTLBUFFER_H
