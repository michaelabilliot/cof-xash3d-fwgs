//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: a fixed size ring buffer of bytes (voice channels)
//
// $NoKeywords: $
//=============================================================================

#include <windows.h>
#include <memory.h>
#include "CircularBuffer.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor.
// Input  : size - size of the buffer
//-----------------------------------------------------------------------------
CCircularBuffer::CCircularBuffer( int size )
{
	SetSize( size );
}

//-----------------------------------------------------------------------------
// Purpose: Makes sure the buffer is consistent (debug builds only)
//-----------------------------------------------------------------------------
void CCircularBuffer::AssertValid()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : size - 
//-----------------------------------------------------------------------------
void CCircularBuffer::SetSize( int size )
{
	m_nSize = size;
	m_nRead = 0;
	m_nWrite = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Empties the buffer
//-----------------------------------------------------------------------------
void CCircularBuffer::Flush()
{
	AssertValid();

	m_nRead = 0;
	m_nWrite = 0;
	m_nCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the available space in the buffer
//-----------------------------------------------------------------------------
int CCircularBuffer::GetWriteAvailable()
{
	AssertValid();

	return( m_nSize - m_nCount );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the size of the buffer
//-----------------------------------------------------------------------------
int CCircularBuffer::GetSize()
{
	AssertValid();

	return( m_nSize );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of bytes in the buffer
//-----------------------------------------------------------------------------
int CCircularBuffer::GetReadAvailable()
{
	AssertValid();

	return( m_nCount );
}

//-----------------------------------------------------------------------------
// Purpose: Reads a specified number of bytes from the buffer without
//			removing them
// Input  : *pchDest - destination buffer
//			nCount - number of bytes to read
// Output : int - number of bytes actually read
//-----------------------------------------------------------------------------
int CCircularBuffer::Peek( char *pchDest, int nCount )
{
	// If no data available, just return.
	if ( m_nCount == 0 )
	{
		return( 0 );
	}

	//
	// Requested amount should not exceed the available amount.
	//
	nCount = min( m_nCount, nCount );

	//
	// Copy as many of the requested bytes as possible.
	// If buffer wrap occurs split the data into two chunks.
	//
	if ( m_nRead + nCount > m_nSize )
	{
		int nCount1 = m_nSize - m_nRead;
		memcpy( pchDest, &m_chData[m_nRead], nCount1 );
		pchDest += nCount1;

		int nCount2 = nCount - nCount1;
		memcpy( pchDest, m_chData, nCount2 );
	}
	// Otherwise copy it in one go.
	else
	{
		memcpy( pchDest, &m_chData[m_nRead], nCount );
	}

	AssertValid();
	return nCount;
}

//-----------------------------------------------------------------------------
// Purpose: Advances the read index, consuming a specified number of bytes from
//			the buffer
// Input  : nCount - number of bytes to consume
// Output : int - number of bytes actually consumed
//-----------------------------------------------------------------------------
int CCircularBuffer::Advance( int nCount )
{
	// If no data available, just return.
	if ( m_nCount == 0 )
	{
		return( 0 );
	}

	//
	// Requested amount should not exceed the available amount.
	//
	nCount = min( m_nCount, nCount );

	//
	// Advance the read pointer, checking for buffer wrap.
	//
	m_nRead = ( m_nRead + nCount ) % m_nSize;
	m_nCount -= nCount;

	//
	// If we have emptied the buffer, reset the read and write indices
	// to minimize buffer wrap.
	//
	if ( m_nCount == 0 )
	{
		m_nRead = 0;
		m_nWrite = 0;
	}

	AssertValid();
	return nCount;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a specified number of bytes from the buffer
// Input  : *pchDest - destination buffer
//			m_nCount - number of bytes to read
// Output : int - number of bytes actually read
//-----------------------------------------------------------------------------
int CCircularBuffer::Read( void *pchDestIn, int m_nCount )
{
	int nPeeked;
	int m_nRead;

	char *pchDest = (char *)pchDestIn;

	nPeeked = Peek( pchDest, m_nCount );

	if ( nPeeked != 0 )
	{
		m_nRead = Advance( nPeeked );
	}
	else
	{
		m_nRead = 0;
	}

	AssertValid();
	return( m_nRead );
}

//-----------------------------------------------------------------------------
// Purpose: Write data to the buffer
// Input  : *pData - source buffer
//			nBytesRequested - number of bytes to write
// Output : int - number of bytes written (the old data is overwritten when
//			the buffer is full)
//-----------------------------------------------------------------------------
int CCircularBuffer::Write( void *pData, int nBytesRequested )
{
	// Write all the data.
	int nBytesToWrite = nBytesRequested;
	char *pDataToWrite = (char *)pData;

	while ( nBytesToWrite )
	{
		int from = m_nWrite;
		int to = m_nWrite + nBytesToWrite;

		if ( to >= m_nSize )
		{
			to = m_nSize;
		}

		memcpy( &m_chData[from], pDataToWrite, to - from );
		pDataToWrite += to - from;

		m_nWrite = to % m_nSize;
		nBytesToWrite -= to - from;
	}

	// Did it cross the read pointer? Then slide the read pointer up.
	// This way, we will discard the old data.
	if ( nBytesRequested > ( m_nSize - m_nCount ) )
	{
		m_nCount = m_nSize;
		m_nRead = m_nWrite;
	}
	else
	{
		m_nCount += nBytesRequested;
	}

	AssertValid();
	return nBytesRequested;
}
