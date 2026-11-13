//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Message buffer of the tracker sockets
//
// $NoKeywords: $
//=============================================================================

#include <string.h>

#include "MsgBuffer_guess.h"

//-----------------------------------------------------------------------------
// Purpose: Allocate message buffer
//-----------------------------------------------------------------------------
CMsgBuffer::CMsgBuffer( const char *buffername, void (*ef)( const char *fmt, ... ) )
{
	m_pszBufferName_guess = buffername;
	m_pfnErrorFunc_guess = ef;
	m_bAllowOverflow_guess = false;	// if false, Error
	m_bOverFlowed_guess = false;	// set to true if the buffer size failed
	m_nMaxSize_guess = NET_MAXMESSAGE_guess;
	m_nPushedCount_guess = 0;
	m_bPushed_guess = false;
	m_nReadCount_guess = 0;
	m_bBadRead_guess = false;

	Clear_guess();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMsgBuffer::~CMsgBuffer( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Temporarily remembers the read position so we can reset it
//-----------------------------------------------------------------------------
void CMsgBuffer::Push_guess( void )
{
	m_nPushedCount_guess = m_nReadCount_guess;
	m_bPushed_guess = true;
}

//-----------------------------------------------------------------------------
// Purpose: Restores the read position
//-----------------------------------------------------------------------------
void CMsgBuffer::Pop_guess( void )
{
	m_nReadCount_guess = m_nPushedCount_guess;
	m_bPushed_guess = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::SetOverflow_guess( bool allowed )
{
	m_bAllowOverflow_guess = allowed;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::GetMaxSize_guess( void )
{
	return m_nMaxSize_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void *CMsgBuffer::GetData_guess( void )
{
	return m_rgData_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::GetCurSize_guess( void )
{
	return m_nCurSize_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::GetReadCount_guess( void )
{
	return m_nReadCount_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::SetTime_guess( float time )
{
	m_fRecvTime_guess = time;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CMsgBuffer::GetTime_guess( void )
{
	return m_fRecvTime_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::SetNetAddress_guess( netadr_t &adr )
{
	m_NetAddr_guess = adr;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
netadr_t &CMsgBuffer::GetNetAddress_guess( void )
{
	return m_NetAddr_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteByte_guess( int c )
{
	unsigned char *buf;
	buf = (unsigned char *)GetSpace_guess( 1 );
	buf[0] = c;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteShort_guess( int c )
{
	unsigned char *buf;
	buf = (unsigned char *)GetSpace_guess( 2 );
	buf[0] = c & 0xff;
	buf[1] = c >> 8;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteLong_guess( int c )
{
	unsigned char *buf;

	buf = (unsigned char *)GetSpace_guess( 4 );
	buf[0] = c & 0xff;
	buf[1] = ( c >> 8 ) & 0xff;
	buf[2] = ( c >> 16 ) & 0xff;
	buf[3] = c >> 24;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteFloat_guess( float f )
{
	union
	{
		float f;
		int l;
	} dat;

	dat.f = f;
	Write_guess( &dat.l, 4 );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteString_guess( const char *s )
{
	if ( !s )
	{
		Write_guess( "", 1 );
	}
	else
	{
		Write_guess( s, strlen( s ) + 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::WriteBuf_guess( int iSize, void *buf )
{
	if ( !buf )
	{
		return;
	}

	Write_guess( buf, iSize );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgBuffer::BeginReading_guess( void )
{
	m_nReadCount_guess = 0;
	m_bBadRead_guess = false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns -1 when the buffer is exhausted
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadByte_guess( void )
{
	int c;

	if ( m_nReadCount_guess + 1 > m_nCurSize_guess )
	{
		m_bBadRead_guess = true;
		return -1;
	}

	c = ( unsigned char )m_rgData_guess[ m_nReadCount_guess ];
	m_nReadCount_guess++;
	return c;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadShort_guess( void )
{
	int c;

	if ( m_nReadCount_guess + 2 > m_nCurSize_guess )
	{
		m_bBadRead_guess = true;
		return -1;
	}

	c = (short)( m_rgData_guess[ m_nReadCount_guess ] + ( m_rgData_guess[ m_nReadCount_guess + 1 ] << 8 ) );
	m_nReadCount_guess += 2;

	return c;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadLong_guess( void )
{
	int c;

	if ( m_nReadCount_guess + 4 > m_nCurSize_guess )
	{
		m_bBadRead_guess = true;
		return -1;
	}

	c = m_rgData_guess[ m_nReadCount_guess ]
		+ ( m_rgData_guess[ m_nReadCount_guess + 1 ] << 8 )
		+ ( m_rgData_guess[ m_nReadCount_guess + 2 ] << 16 )
		+ ( m_rgData_guess[ m_nReadCount_guess + 3 ] << 24 );

	m_nReadCount_guess += 4;

	return c;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CMsgBuffer::ReadFloat_guess( void )
{
	union
	{
		unsigned char b[4];
		float f;
	} dat;

	dat.b[0] = m_rgData_guess[ m_nReadCount_guess ];
	dat.b[1] = m_rgData_guess[ m_nReadCount_guess + 1 ];
	dat.b[2] = m_rgData_guess[ m_nReadCount_guess + 2 ];
	dat.b[3] = m_rgData_guess[ m_nReadCount_guess + 3 ];
	m_nReadCount_guess += 4;
	return dat.f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CMsgBuffer::ReadBuf_guess( int iSize, void *pbuf )
{
	if ( m_nReadCount_guess + iSize > m_nCurSize_guess )
	{
		m_bBadRead_guess = true;
		return -1;
	}

	memcpy( pbuf, &m_rgData_guess[ m_nReadCount_guess ], iSize );
	m_nReadCount_guess += iSize;
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Reads a null terminated string into a static buffer
//-----------------------------------------------------------------------------
char *CMsgBuffer::ReadString_guess( void )
{
	static char string_guess[ NET_MAXMESSAGE_guess ];
	int length_guess, c_guess;

	length_guess = 0;
	do
	{
		c_guess = (char)ReadByte_guess();
		if ( c_guess == -1 || c_guess == 0 )
			break;
		string_guess[ length_guess ] = c_guess;
		length_guess++;
	} while ( length_guess < sizeof( string_guess ) - 1 );

	string_guess[ length_guess ] = 0;

	return string_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Reset the buffer for writing
//-----------------------------------------------------------------------------
void CMsgBuffer::Clear_guess( void )
{
	m_nCurSize_guess = 0;
	m_bOverFlowed_guess = false;
	m_nReadCount_guess = 0;
	m_bBadRead_guess = false;
	memset( m_rgData_guess, 0, sizeof( m_rgData_guess ) );
}

//-----------------------------------------------------------------------------
// Purpose: Ensures sufficient space to append an object of length
//-----------------------------------------------------------------------------
void *CMsgBuffer::GetSpace_guess( int length )
{
	void *d;

	if ( m_nCurSize_guess + length > m_nMaxSize_guess )
	{
		if ( !m_bAllowOverflow_guess )
		{
			if ( m_pfnErrorFunc_guess )
			{
				( *m_pfnErrorFunc_guess )( "CMsgBuffer(%s), no room for %i bytes, %i / %i already in use\n",
					m_pszBufferName_guess, length, m_nCurSize_guess, m_nMaxSize_guess );
			}
			return NULL;
		}

		if ( length > m_nMaxSize_guess )
		{
			if ( m_pfnErrorFunc_guess )
			{
				( *m_pfnErrorFunc_guess )( "CMsgBuffer(%s), no room for %i bytes, %i is max\n",
					m_pszBufferName_guess, length, m_nMaxSize_guess );
			}
			return NULL;
		}

		m_bOverFlowed_guess = true;
		Clear_guess();
	}

	d = m_rgData_guess + m_nCurSize_guess;
	m_nCurSize_guess += length;
	return d;
}

//-----------------------------------------------------------------------------
// Purpose: Copy buffer in at current writing point
//-----------------------------------------------------------------------------
void CMsgBuffer::Write_guess( const void *data, int length )
{
	memcpy( GetSpace_guess( length ), data, length );
}
