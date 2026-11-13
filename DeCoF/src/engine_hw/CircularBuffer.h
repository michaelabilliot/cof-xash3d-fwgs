//========= Copyright (c) 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: a fixed size ring buffer of bytes (voice channels)
//
// $NoKeywords: $
//=============================================================================

#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H
#pragma once

class CCircularBuffer
{
public:
	CCircularBuffer( int size );
	void	SetSize( int nSize );

protected:
	void	AssertValid();

public:
	void	Flush();
	int		GetSize();
	int		GetWriteAvailable();
	int		GetReadAvailable();
	int		Peek( char *pchDest, int nCount );
	int		Advance( int nCount );
	int		Read( void *pchDest, int nCount );
	int		Write( void *pchData, int nCount );

protected:
	int		m_nCount;
	int		m_nRead;
	int		m_nWrite;
	int		m_nSize;
	char	m_chData[1];
};

// a circular buffer with its storage
template< int size >
class CSizedCircularBuffer : public CCircularBuffer
{
public:
	CSizedCircularBuffer() : CCircularBuffer( size )
	{
	}

private:
	char	myData[size - 1];
};

#endif // CIRCULARBUFFER_H
