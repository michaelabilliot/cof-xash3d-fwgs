//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Message buffer of the tracker sockets. The class name comes from
//			its RTTI and its error strings; the names of its methods and
//			members are not recorded anywhere.
//
// $NoKeywords: $
//=============================================================================

#ifndef MSGBUFFER_GUESS_H
#define MSGBUFFER_GUESS_H

#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"

//-----------------------------------------------------------------------------
// Purpose: Generic byte buffer with read and write functions
//-----------------------------------------------------------------------------
class CMsgBuffer
{
public:
	enum
	{
		NET_MAXMESSAGE_guess = 8192
	};

	// Buffers must be named
	CMsgBuffer( const char *buffername = "unnamed", void (*ef)( const char *fmt, ... ) = 0 );
	virtual ~CMsgBuffer( void );

	// Push read count ( to peek at data )
	void Push_guess( void );
	void Pop_guess( void );

	// Set/unset the allow overflow flag
	void SetOverflow_guess( bool allowed );

	int GetMaxSize_guess( void );
	void *GetData_guess( void );
	int GetCurSize_guess( void );
	int GetReadCount_guess( void );

	// setting and storing time received
	void SetTime_guess( float time );
	float GetTime_guess( void );

	// net address received from
	void SetNetAddress_guess( netadr_t &adr );
	netadr_t &GetNetAddress_guess( void );

	// Writing functions
	void WriteByte_guess( int c );
	void WriteShort_guess( int c );
	void WriteLong_guess( int c );
	void WriteFloat_guess( float f );
	void WriteString_guess( const char *s );
	void WriteBuf_guess( int iSize, void *buf );

	// Reading functions
	void BeginReading_guess( void );
	int ReadByte_guess( void );
	int ReadShort_guess( void );
	int ReadLong_guess( void );
	float ReadFloat_guess( void );
	int ReadBuf_guess( int iSize, void *pbuf );
	char *ReadString_guess( void );

	// Reset the buffer for writing
	void Clear_guess( void );

private:
	// Ensures sufficient space to append an object of length
	void *GetSpace_guess( int length );
	// Copy buffer in at current writing point
	void Write_guess( const void *data, int length );

private:
	const char *m_pszBufferName_guess;
	void ( *m_pfnErrorFunc_guess )( const char *fmt, ... );

	int m_nReadCount_guess;
	int m_nPushedCount_guess;
	bool m_bPushed_guess;
	bool m_bBadRead_guess;
	int m_nMaxSize_guess;
	int m_nCurSize_guess;
	bool m_bAllowOverflow_guess;
	bool m_bOverFlowed_guess;
	unsigned char m_rgData_guess[ NET_MAXMESSAGE_guess ];
	float m_fRecvTime_guess;
	netadr_t m_NetAddr_guess;
};

#endif // MSGBUFFER_GUESS_H
