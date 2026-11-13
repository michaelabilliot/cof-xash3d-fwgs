//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $Header: $
// $NoKeywords: $
//
// Serialization buffer
//=============================================================================

#pragma warning (disable : 4514)

#include "UtlBuffer.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define Assert( _exp ) ((void)0)

//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
CUtlBuffer::CUtlBuffer( int growSize, int initSize, bool text ) :
	m_Memory( growSize, initSize ), m_Error(0)
{
	m_Get = 0;
	m_Put = 0;
	m_Flags = 0;
	if (text)
		m_Flags |= TEXT_BUFFER;
}

CUtlBuffer::CUtlBuffer( void const* pBuffer, int size, bool text ) :
	m_Memory( (unsigned char*)pBuffer, size ), m_Error(0)
{
	m_Get = 0;
	m_Put = 0;
	m_Flags = 0;
	if (text)
		m_Flags |= TEXT_BUFFER;
}

//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
void CUtlBuffer::EnsureCapacity( int num )
{
	m_Memory.EnsureCapacity( num );
}

//-----------------------------------------------------------------------------
// Base get method from which all others derive
//-----------------------------------------------------------------------------
void CUtlBuffer::Get( void* pMem, int size )
{
	Assert( m_Get + size <= m_Memory.NumAllocated() );
	memcpy( pMem, &m_Memory[m_Get], size );
	m_Get += size;
}

//-----------------------------------------------------------------------------
// Eats whitespace
//-----------------------------------------------------------------------------
void CUtlBuffer::EatWhiteSpace()
{
	if (IsText() && IsValid())
	{
		int lastpos = Size();
		while (m_Get < lastpos)
		{
			if (!isspace( *(char*)&m_Memory[m_Get] ))
				return;
			m_Get += sizeof(char);
		}
	}
}

//-----------------------------------------------------------------------------
// Reads a null-terminated string
//-----------------------------------------------------------------------------
void CUtlBuffer::GetString( char* pString )
{
	if (!IsValid())
	{
		*pString = 0;
		return;
	}

	if (!IsText())
	{
		int len = strlen( (char*)&m_Memory[m_Get] );
		Get( pString, len + 1 );
	}
	else
	{
		// eat all whitespace
		EatWhiteSpace();

		int lastpos = Size();
		while (m_Get < lastpos)
		{
			char c = *(char*)&m_Memory[m_Get];
			if (isspace(c) || (!c))
				break;
			*pString++ = c;
			++m_Get;
		}

		// terminate
		*pString = 0;
	}
}

//-----------------------------------------------------------------------------
// Checks if a get is ok
//-----------------------------------------------------------------------------
bool CUtlBuffer::CheckGet( int size )
{
	if (m_Error)
		return false;

	if (m_Memory.NumAllocated() >= m_Get + size)
		return true;

	m_Error |= GET_OVERFLOW;
	return false;
}

//-----------------------------------------------------------------------------
// Change where I'm reading
//-----------------------------------------------------------------------------
void CUtlBuffer::SeekGet( SeekType_t type, int offset )
{
	switch( type )
	{
	case SEEK_HEAD:
		m_Get = offset;
		break;

	case SEEK_CURRENT:
		m_Get += offset;
		break;

	case SEEK_TAIL:
		m_Get = m_Memory.NumAllocated() - offset;
		break;
	}
}

//-----------------------------------------------------------------------------
// Parse...
//-----------------------------------------------------------------------------

#pragma warning ( disable : 4706 )

int CUtlBuffer::VaScanf( char const* pFmt, va_list list )
{
	Assert( pFmt );
	if (m_Error || !IsText())
		return 0;

	int numScanned = 0;
	char c;
	char* pEnd;
	while ( c = *pFmt++ )
	{
		// Stop if we hit the end of the buffer
		if (m_Get >= Size())
		{
			m_Error |= GET_OVERFLOW;
			break;
		}

		switch (c)
		{
		case ' ':
			// eat all whitespace
			EatWhiteSpace();
			break;

		case '%':
			{
				// Conversion character... try to convert baby!
				char type = *pFmt++;
				if (type == 0)
					return numScanned;

				switch(type)
				{
				case 'c':
					{
						char* ch = va_arg( list, char * );
						*ch = (char)m_Memory[m_Get];
						++m_Get;
					}
				break;

				case 'i':
				case 'd':
					{
						int* i = va_arg( list, int * );
						*i = strtol( (char*)PeekGet(), &pEnd, 10 );
						if (pEnd == PeekGet())
							return numScanned;
						m_Get = (int)pEnd - (int)Base();
					}
					break;

				case 'u':
					{
						unsigned int* u = va_arg( list, unsigned int *);
						*u = strtoul( (char*)PeekGet(), &pEnd, 10 );
						if (pEnd == PeekGet())
							return numScanned;
						m_Get = (int)pEnd - (int)Base();
					}
					break;

				case 'f':
					{
						float* f = va_arg( list, float *);
						*f = (float)strtod( (char*)PeekGet(), &pEnd );
						if (pEnd == PeekGet())
							return numScanned;
						m_Get = (int)pEnd - (int)Base();
					}
					break;

				case 's':
					{
						char* s = va_arg( list, char * );
						GetString( s );
					}
					break;

				default:
					{
						// unimplemented scanf type
						Assert(0);
						return numScanned;
					}
					break;
				}

				++numScanned;
			}
			break;

		default:
			{
				// Here we have to match the format string character
				// against what's in the buffer or we're done.
				if (c != m_Memory[m_Get])
					return numScanned;
				++m_Get;
			}
		}
	}
	return numScanned;
}

#pragma warning ( default : 4706 )

int CUtlBuffer::Scanf( char const* pFmt, ... )
{
	va_list args;

	va_start( args, pFmt );
	int count = VaScanf( pFmt, args );
	va_end( args );

	return count;
}

//-----------------------------------------------------------------------------
// Serialization
//-----------------------------------------------------------------------------
void CUtlBuffer::Put( void const* pMem, int size )
{
	if (CheckPut( size ))
	{
		memcpy( &m_Memory[m_Put], pMem, size );
		m_Put += size;
	}
}

//-----------------------------------------------------------------------------
// Writes a null-terminated string
//-----------------------------------------------------------------------------
void CUtlBuffer::PutString( char const* pString )
{
	int len = strlen( pString );

	// Not text? append a null at the end.
	if (!IsText())
		++len;

	Put( pString, len );
}

void CUtlBuffer::VaPrintf( char const* pFmt, va_list list )
{
	char temp[2048];
	int len = vsprintf( temp, pFmt, list );
	Assert( len < 2048 );

	// Not text? append a null at the end.
	if (!IsText())
		++len;

	Put( temp, len );
}

void CUtlBuffer::Printf( char const* pFmt, ... )
{
	va_list args;

	va_start( args, pFmt );
	VaPrintf( pFmt, args );
	va_end( args );
}

//-----------------------------------------------------------------------------
// Checks if a put is ok
//-----------------------------------------------------------------------------
bool CUtlBuffer::CheckPut( int size )
{
	if (m_Error)
		return false;

	while( m_Memory.NumAllocated() < m_Put + size )
	{
		if ( m_Memory.IsExternallyAllocated() )
		{
			m_Error |= PUT_OVERFLOW;
			return false;
		}

		m_Memory.Grow();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Change where I'm writing
//-----------------------------------------------------------------------------
void CUtlBuffer::SeekPut( SeekType_t type, int offset )
{
	switch( type )
	{
	case SEEK_HEAD:
		m_Put = offset;
		break;

	case SEEK_CURRENT:
		m_Put += offset;
		break;

	case SEEK_TAIL:
		m_Put = m_Memory.NumAllocated() - offset;
		break;
	}
}
