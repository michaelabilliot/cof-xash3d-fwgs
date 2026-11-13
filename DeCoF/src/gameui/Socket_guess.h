//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: UDP socket of the tracker master server code, read by a worker
//			thread. The class names come from the RTTI; the names of the
//			methods and members are not recorded anywhere.
//
// $NoKeywords: $
//=============================================================================

#ifndef SOCKET_GUESS_H
#define SOCKET_GUESS_H

#ifdef _WIN32
#pragma once
#endif

#include "netadr.h"
#include "MsgBuffer_guess.h"
#include "UtlVector.h"

class CSocket;

//-----------------------------------------------------------------------------
// Purpose: Handles the messages a socket receives, chained per socket
//-----------------------------------------------------------------------------
class CMsgHandler
{
public:
	typedef enum
	{
		MSGHANDLER_ALL_guess = 0,
		MSGHANDLER_BYTECODE_guess,
		MSGHANDLER_STRING_guess
	} HANDLERTYPE;

	CMsgHandler( HANDLERTYPE type, void *typeinfo = NULL );
	virtual ~CMsgHandler( void );

	// Handle the message
	virtual bool Process_guess( netadr_t *from, CMsgBuffer *msg ) = 0;

	// Check for special handling
	bool ProcessMessage_guess( netadr_t *from, CMsgBuffer *msg );

	// Linked list of handlers
	virtual CMsgHandler *GetNext_guess( void ) const;
	virtual void SetNext_guess( CMsgHandler *next );

	// Associated socket
	virtual CSocket *GetSocket_guess( void ) const;
	virtual void SetSocket_guess( CSocket *socket );

private:
	enum
	{
		MAX_HANDLER_STRING_guess = 64
	};

	int m_nUnused_guess;
	HANDLERTYPE m_Type_guess;
	unsigned char m_ByteCode_guess;
	char m_szString_guess[ MAX_HANDLER_STRING_guess ];
	CMsgHandler *m_pNext_guess;
	CSocket *m_pSocket_guess;
};

//-----------------------------------------------------------------------------
// Purpose: UDP socket whose incoming data is read by the socket thread
//-----------------------------------------------------------------------------
class CSocket
{
public:
	CSocket( const char *socketname, int port = -1 );
	virtual ~CSocket( void );

	// Message handlers
	virtual void AddMessageHandler_guess( CMsgHandler *handler );
	virtual void RemoveMessageHandler_guess( CMsgHandler *handler );

	// Send a message
	virtual int SendMessage_guess( netadr_t *to, CMsgBuffer *msg = NULL );
	virtual int Broadcast_guess( int port, CMsgBuffer *msg = NULL );

	// Buffer to write outgoing messages to
	virtual CMsgBuffer *GetSendBuffer_guess( void );

	// Called once per frame (outside of the socket thread) to route the
	//  received messages to the handlers
	virtual void Frame_guess( void );

	// Is the socket ready to be used
	virtual bool IsValid_guess( void ) const;

	// Clock of the socket thread
	static float GetClock_guess( void );

	// The socket's local address
	virtual const netadr_t *GetAddress_guess( void );

	// User data
	virtual void SetUserData_guess( unsigned int userData );
	virtual unsigned int GetUserData_guess( void ) const;

	// Underlying winsock socket
	virtual int GetSocketNumber_guess( void ) const;

	// Called by the socket thread when data is waiting on the socket
	virtual bool ReceiveData_guess( void );

private:
	enum
	{
		MAX_SPLITPACKETS_guess = 16
	};

	const char *m_pszName_guess;
	bool m_bValid_guess;
	netadr_t m_Address_guess;
	bool m_bResolved_guess;
	CUtlVector< CMsgBuffer > m_MsgBuffers_guess;
	CMsgBuffer m_SendBuffer_guess;
	void *m_pBufferCS_guess;
	CMsgHandler *m_pMessageHandlers_guess;
	int m_Socket_guess;
	unsigned int m_nUserData_guess;
	netadr_t m_ToAddress_guess;
	bool m_bBroadcastSend_guess;
	int m_iTotalPackets_guess;
	int m_iCurrentPackets_guess;
	int m_iSeqNo_guess;
	int m_iRetries_guess;
	int m_nUnused1_guess;
	int m_nUnused2_guess;
	int m_nUnused3_guess;
	int m_nUnused4_guess;
	CMsgBuffer m_CurPacket_guess[ MAX_SPLITPACKETS_guess ];
};

#endif // SOCKET_GUESS_H
