//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: UDP socket of the tracker master server code, read by a worker
//			thread
//
// $NoKeywords: $
//=============================================================================

#define FD_SETSIZE 1024

#include <windows.h>
#include <winsock.h>
#include <string.h>

#include "Socket_guess.h"
#include "NetAPI_guess.h"

#define max(a,b)            (((a) > (b)) ? (a) : (b))

//-----------------------------------------------------------------------------
// Purpose: Waits for data on the sockets and reads it into their buffers
//-----------------------------------------------------------------------------
class CSocketThread
{
public:
	typedef struct threadsocket_s
	{
		struct threadsocket_s *next;
		CSocket *socket;
	} threadsocket_t;

	CSocketThread( void );
	virtual ~CSocketThread( void );

	// Sockets add/remove themselves via their constructor
	virtual void AddSocketToThread_guess( CSocket *socket );
	virtual void RemoveSocketFromThread_guess( CSocket *socket );

	// Lock changes to socket list, etc.
	virtual void Lock_guess( void );
	// Unlock socket list, etc.
	virtual void Unlock_guess( void );

	// Retrieve handle to shutdown event
	virtual HANDLE GetShutdownHandle_guess( void );
	// Get head of socket list
	virtual threadsocket_t *GetSocketList_guess( void );

	// Sample clock for socket thread
	virtual float GetClock_guess( void );

private:
	// Initialize the clock
	void InitTimer_guess( void );

private:
	// Critical section used for synchronizing access to socket list
	CRITICAL_SECTION m_cs_guess;
	// List of sockets we are listening on
	threadsocket_t *m_pSocketList_guess;
	// Thread handle
	HANDLE m_hThread_guess;
	// Thread id
	DWORD m_nThreadId_guess;
	// Event to set when we want to tell the thread to shut itself down
	HANDLE m_hShutdown_guess;

	// High performance clock frequency
	double m_dClockFrequency_guess;
	// Current accumulated time
	double m_dCurrentTime_guess;
	// How many bits to shift raw 64 bit sample count by
	int m_nTimeSampleShift_guess;
	// Previous 32 bit sampled count
	unsigned int m_uiPreviousTime_guess;
};

#pragma pack( push, 1 )
typedef struct
{
	int netID;
	int sequenceNumber;
	char packetID;
} SPLITPACKET_guess;
#pragma pack( pop )

#define MAX_RETRIES_guess	2

//-----------------------------------------------------------------------------
// Purpose: Thread to service socket data
//-----------------------------------------------------------------------------
static DWORD WINAPI SocketThreadFunc_guess( LPVOID threadobject )
{
	// Get pointer to CSocketThread object
	CSocketThread *socketthread = (CSocketThread *)threadobject;
	if ( !socketthread )
	{
		return 0;
	}

	// Keep looking for data until shutdown event is triggered
	while ( 1 )
	{
		// List of sockets
		CSocketThread::threadsocket_t *sockets;
		// file descriptor set for sockets
		fd_set fdset;
		// number of sockets with messages ready
		int number;
		// number of sockets added to fd_set
		int count;

		// Check for shutdown event
		if ( WAIT_OBJECT_0 == WaitForSingleObject( socketthread->GetShutdownHandle_guess(), 0 ) )
		{
			break;
		}

		// Clear the set
		FD_ZERO( &fdset );

		// No changes to list right now
		socketthread->Lock_guess();

		// Add all active sockets to the fdset
		count = 0;
		for ( sockets = socketthread->GetSocketList_guess(); sockets; sockets = sockets->next )
		{
			FD_SET( sockets->socket->GetSocketNumber_guess(), &fdset );
			count = max( count, sockets->socket->GetSocketNumber_guess() );
		}

		// Done
		socketthread->Unlock_guess();

		if ( count )
		{
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100000; // 100 millisecond == 100000 usec

			// Block for 100000 usec, or until a message is in the queue
			number = select( count + 1, &fdset, NULL, NULL, &tv );
			if ( number > 0 )
			{
				socketthread->Lock_guess();

				// Receive messages
				for ( sockets = socketthread->GetSocketList_guess(); sockets; sockets = sockets->next )
				{
					// See if there is a message waiting
					if ( FD_ISSET( sockets->socket->GetSocketNumber_guess(), &fdset ) )
					{
						// keep reading as long as there is data on the socket
						while ( sockets->socket->ReceiveData_guess() )
						{
						}
					}
				}

				// Done
				socketthread->Unlock_guess();
			}
		}
	}

	// Thread is done
	ExitThread( 0 );
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: The socket thread singleton
//-----------------------------------------------------------------------------
static CSocketThread *GetSocketThread_guess()
{
	static CSocketThread g_SocketThread_guess;
	return &g_SocketThread_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSocketThread::CSocketThread( void )
{
	InitTimer_guess();

	m_pSocketList_guess = NULL;

	InitializeCriticalSection( &m_cs_guess );

	m_hShutdown_guess = CreateEvent( NULL, TRUE, FALSE, NULL );

	m_hThread_guess = 0;
	m_nThreadId_guess = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSocketThread::~CSocketThread( void )
{
	Lock_guess();
	if ( m_hThread_guess )
	{
		SetEvent( m_hShutdown_guess );
		Sleep( 2 );
		TerminateThread( m_hThread_guess, 0 );
	}
	Unlock_guess();

	if ( m_hThread_guess )
	{
		CloseHandle( m_hThread_guess );
	}

	CloseHandle( m_hShutdown_guess );

	DeleteCriticalSection( &m_cs_guess );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the high resolution clock
//-----------------------------------------------------------------------------
void CSocketThread::InitTimer_guess( void )
{
	BOOL success;
	LARGE_INTEGER PerformanceFreq;
	unsigned int lowpart, highpart;

	// Start clock at zero
	m_dCurrentTime_guess = 0.0;

	success = QueryPerformanceFrequency( &PerformanceFreq );

	// get 32 out of the 64 time bits such that we have around
	// 1 microsecond resolution
	lowpart = (unsigned int)PerformanceFreq.LowPart;
	highpart = (unsigned int)PerformanceFreq.HighPart;

	m_nTimeSampleShift_guess = 0;

	while ( highpart || ( lowpart > 2000000.0 ) )
	{
		m_nTimeSampleShift_guess++;
		lowpart >>= 1;
		lowpart |= ( highpart & 1 ) << 31;
		highpart >>= 1;
	}

	m_dClockFrequency_guess = 1.0 / (double)lowpart;

	// Get initial sample
	unsigned int temp;
	LARGE_INTEGER PerformanceCount;
	QueryPerformanceCounter( &PerformanceCount );
	if ( !m_nTimeSampleShift_guess )
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}
	else
	{
		// Rotate counter to right by m_nTimeSampleShift places
		temp = ( (unsigned int)PerformanceCount.LowPart >> m_nTimeSampleShift_guess ) |
			( (unsigned int)PerformanceCount.HighPart << ( 32 - m_nTimeSampleShift_guess ) );
	}

	// Set first time stamp
	m_uiPreviousTime_guess = temp;
}

//-----------------------------------------------------------------------------
// Purpose: Thread local clock
//-----------------------------------------------------------------------------
float CSocketThread::GetClock_guess( void )
{
	LARGE_INTEGER PerformanceCount;
	unsigned int temp, t2;
	double time;

	// Get sample counter
	QueryPerformanceCounter( &PerformanceCount );

	if ( !m_nTimeSampleShift_guess )
	{
		temp = (unsigned int)PerformanceCount.LowPart;
	}
	else
	{
		// Rotate counter to right by m_nTimeSampleShift places
		temp = ( (unsigned int)PerformanceCount.LowPart >> m_nTimeSampleShift_guess ) |
			( (unsigned int)PerformanceCount.HighPart << ( 32 - m_nTimeSampleShift_guess ) );
	}

	// check for turnover or backward time
	if ( ( temp <= m_uiPreviousTime_guess ) && ( ( m_uiPreviousTime_guess - temp ) < 0x10000000 ) )
	{
		m_uiPreviousTime_guess = temp;	// so we can't get stuck
	}
	else
	{
		// gap in performance clocks
		t2 = temp - m_uiPreviousTime_guess;

		// Convert to time using frequencey of clock
		time = (double)t2 * m_dClockFrequency_guess;

		// Remember old time
		m_uiPreviousTime_guess = temp;

		// Increment clock
		m_dCurrentTime_guess += time;
	}

	// Convert to float
	return (float)m_dCurrentTime_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Returns handle of shutdown event
//-----------------------------------------------------------------------------
HANDLE CSocketThread::GetShutdownHandle_guess( void )
{
	return m_hShutdown_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Returns head of socket list
//-----------------------------------------------------------------------------
CSocketThread::threadsocket_t *CSocketThread::GetSocketList_guess( void )
{
	return m_pSocketList_guess;
}

int socketCount_guess = 0;

//-----------------------------------------------------------------------------
// Purpose: Locks object and adds socket to thread
//-----------------------------------------------------------------------------
void CSocketThread::AddSocketToThread_guess( CSocket *socket )
{
	// create the thread if it isn't there
	if ( !m_hThread_guess )
	{
		m_hThread_guess = CreateThread( NULL, 0, SocketThreadFunc_guess, (void *)this, 0, &m_nThreadId_guess );
	}

	socketCount_guess++;

	threadsocket_t *p = new threadsocket_t;
	p->socket = socket;

	Lock_guess();
	p->next = m_pSocketList_guess;
	m_pSocketList_guess = p;
	Unlock_guess();
}

//-----------------------------------------------------------------------------
// Purpose: Locks list and removes specified socket from thread
//-----------------------------------------------------------------------------
void CSocketThread::RemoveSocketFromThread_guess( CSocket *socket )
{
	if ( !m_hThread_guess )
		return;

	socketCount_guess--;

	Lock_guess();
	if ( m_pSocketList_guess )
	{
		threadsocket_t *p, *n;
		p = m_pSocketList_guess;
		if ( p->socket == socket )
		{
			m_pSocketList_guess = m_pSocketList_guess->next;
			delete p;
		}
		else
		{
			while ( p->next )
			{
				n = p->next;
				if ( n->socket == socket )
				{
					p->next = n->next;
					delete n;
					break;
				}
				p = n;
			}
		}
	}
	Unlock_guess();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSocketThread::Lock_guess( void )
{
	EnterCriticalSection( &m_cs_guess );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSocketThread::Unlock_guess( void )
{
	LeaveCriticalSection( &m_cs_guess );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMsgHandler::CMsgHandler( HANDLERTYPE type, void *typeinfo )
{
	m_Type_guess = type;
	m_pNext_guess = NULL;

	// Not associated with a socket yet
	SetSocket_guess( NULL );

	m_ByteCode_guess = 0;
	m_szString_guess[0] = 0;

	switch ( m_Type_guess )
	{
	case MSGHANDLER_BYTECODE_guess:
		m_ByteCode_guess = *(unsigned char *)typeinfo;
		break;
	case MSGHANDLER_STRING_guess:
		strcpy( m_szString_guess, (char *)typeinfo );
		break;
	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMsgHandler::~CMsgHandler( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Default message handler
//-----------------------------------------------------------------------------
bool CMsgHandler::Process_guess( netadr_t *from, CMsgBuffer *msg )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check for special handling
//-----------------------------------------------------------------------------
bool CMsgHandler::ProcessMessage_guess( netadr_t *from, CMsgBuffer *msg )
{
	bool bret = false;
	unsigned char ch;
	const char *str;

	// Crack bytecode or string code
	switch ( m_Type_guess )
	{
	case MSGHANDLER_BYTECODE_guess:
		msg->Push_guess();
		ch = (unsigned char)msg->ReadByte_guess();
		msg->Pop_guess();
		if ( ch == m_ByteCode_guess )
		{
			bret = Process_guess( from, msg );
		}
		break;
	case MSGHANDLER_STRING_guess:
		msg->Push_guess();
		str = msg->ReadString_guess();
		msg->Pop_guess();
		if ( str && str[0] && !stricmp( m_szString_guess, str ) )
		{
			bret = Process_guess( from, msg );
		}
		break;
	default:
		bret = Process_guess( from, msg );
		break;
	}

	return bret;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CMsgHandler *CMsgHandler::GetNext_guess( void ) const
{
	return m_pNext_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgHandler::SetNext_guess( CMsgHandler *next )
{
	m_pNext_guess = next;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CSocket *CMsgHandler::GetSocket_guess( void ) const
{
	return m_pSocket_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMsgHandler::SetSocket_guess( CSocket *socket )
{
	m_pSocket_guess = socket;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a non-blocking, broadcast capable UDP socket
//-----------------------------------------------------------------------------
CSocket::CSocket( const char *socketname, int port ) : m_SendBuffer_guess( socketname )
{
	struct sockaddr_in address;
	unsigned long _true = 1;
	int i = 1;

	m_pszName_guess = socketname;

	m_bValid_guess = false;
	m_bResolved_guess = false;
	m_pMessageHandlers_guess = NULL;
	m_nUserData_guess = 0;
	m_bBroadcastSend_guess = false;
	m_iTotalPackets_guess = 0;
	m_iCurrentPackets_guess = 0;
	m_iRetries_guess = 0;
	m_nUnused4_guess = 0;
	m_nUnused3_guess = 0;
	m_nUnused1_guess = 0;
	m_nUnused2_guess = 0;

	m_pBufferCS_guess = new CRITICAL_SECTION;
	InitializeCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );

	// ensure the socketthread singleton has been created
	GetSocketThread_guess();

	// Set up the socket
	m_Socket_guess = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( m_Socket_guess == -1 )
	{
		return;
	}

	// Set it to non-blocking
	if ( ioctlsocket( m_Socket_guess, FIONBIO, &_true ) == -1 )
	{
		closesocket( m_Socket_guess );
		m_Socket_guess = 0;
		return;
	}

	// Allow broadcast packets
	if ( setsockopt( m_Socket_guess, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof( i ) ) == -1 )
	{
		closesocket( m_Socket_guess );
		m_Socket_guess = 0;
		return;
	}

	address.sin_addr.s_addr = INADDR_ANY;

	if ( port == -1 )
	{
		address.sin_port = 0;
	}
	else
	{
		address.sin_port = htons( (short)port );
	}

	address.sin_family = AF_INET;

	// only bind if we're required to be on a certain port
	if ( address.sin_port > 0 )
	{
		// Bind the socket to specified port
		if ( bind( m_Socket_guess, (struct sockaddr *)&address, sizeof( address ) ) == -1 )
		{
			closesocket( m_Socket_guess );
			m_Socket_guess = 0;
			return;
		}
	}

	// Mark as valid
	m_bValid_guess = true;

	// Only add valid sockets to thread
	GetSocketThread_guess()->AddSocketToThread_guess( this );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSocket::~CSocket( void )
{
	DeleteCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );
	delete (CRITICAL_SECTION *)m_pBufferCS_guess;

	// Try to remove socket from thread
	GetSocketThread_guess()->RemoveSocketFromThread_guess( this );

	if ( m_bValid_guess )
	{
		::shutdown( m_Socket_guess, 0x01 );
		::shutdown( m_Socket_guess, 0x02 );
		closesocket( m_Socket_guess );
		m_Socket_guess = 0;
	}

	// Remove handlers
	CMsgHandler *handler = m_pMessageHandlers_guess;
	while ( handler )
	{
		RemoveMessageHandler_guess( handler );
		delete handler;
		handler = m_pMessageHandlers_guess;
	}
	m_pMessageHandlers_guess = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Add hander to head of chain
//-----------------------------------------------------------------------------
void CSocket::AddMessageHandler_guess( CMsgHandler *handler )
{
	handler->SetNext_guess( m_pMessageHandlers_guess );
	m_pMessageHandlers_guess = handler;

	// Set the socket pointer
	handler->SetSocket_guess( this );
}

//-----------------------------------------------------------------------------
// Purpose: Removed indicated handler
//-----------------------------------------------------------------------------
void CSocket::RemoveMessageHandler_guess( CMsgHandler *handler )
{
	if ( !handler )
	{
		return;
	}

	CMsgHandler *list = m_pMessageHandlers_guess;
	if ( list == handler )
	{
		m_pMessageHandlers_guess = m_pMessageHandlers_guess->GetNext_guess();
		return;
	}

	while ( list )
	{
		if ( list->GetNext_guess() == handler )
		{
			list->SetNext_guess( handler->GetNext_guess() );
			handler->SetNext_guess( NULL );
			return;
		}
		list = list->GetNext_guess();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Send message to specified address
//-----------------------------------------------------------------------------
int CSocket::SendMessage_guess( netadr_t *to, CMsgBuffer *msg )
{
	m_bBroadcastSend_guess = false;
	m_ToAddress_guess = *to;

	if ( !m_bValid_guess )
	{
		return 0;
	}

	if ( !msg )
	{
		msg = GetSendBuffer_guess();
	}

	struct sockaddr addr;
	net_guess->NetAdrToSockAddr_guess( to, &addr );

	int bytessent = sendto( m_Socket_guess, (const char *)msg->GetData_guess(), msg->GetCurSize_guess(), 0, &addr, sizeof( addr ) );
	if ( bytessent == msg->GetCurSize_guess() )
	{
		return bytessent;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Send broadcast message on specified port
//-----------------------------------------------------------------------------
int CSocket::Broadcast_guess( int port, CMsgBuffer *msg )
{
	m_bBroadcastSend_guess = true;
	memset( &m_ToAddress_guess, 0, sizeof( m_ToAddress_guess ) );

	if ( !m_bValid_guess )
	{
		return 0;
	}

	if ( !msg )
	{
		msg = GetSendBuffer_guess();
	}

	struct sockaddr addr;
	netadr_t to;

	to.port = htons( (unsigned short)port );
	to.type = NA_BROADCAST;

	net_guess->NetAdrToSockAddr_guess( &to, &addr );

	int bytessent = sendto( m_Socket_guess, (const char *)msg->GetData_guess(), msg->GetCurSize_guess(), 0, &addr, sizeof( addr ) );
	if ( bytessent == msg->GetCurSize_guess() )
	{
		return bytessent;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Retrieve internal message buffer
//-----------------------------------------------------------------------------
CMsgBuffer *CSocket::GetSendBuffer_guess( void )
{
	return &m_SendBuffer_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame (outside of the socket thread) to route the
//			received messages to the handlers
//-----------------------------------------------------------------------------
void CSocket::Frame_guess( void )
{
	if ( !m_MsgBuffers_guess.Count() )
		return;

	EnterCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );

	for ( int i = 0; i < m_MsgBuffers_guess.Count(); i++ )
	{
		// Scan for handlers
		CMsgHandler *handler = m_pMessageHandlers_guess;
		netadr_t addr = m_MsgBuffers_guess[i].GetNetAddress_guess();
		while ( handler )
		{
			if ( handler->ProcessMessage_guess( &addr, &m_MsgBuffers_guess[i] ) )
			{
				break;
			}

			handler = handler->GetNext_guess();
		}
	}

	m_MsgBuffers_guess.RemoveAll();

	LeaveCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );
}

//-----------------------------------------------------------------------------
// Purpose: Is socket set up correctly
//-----------------------------------------------------------------------------
bool CSocket::IsValid_guess( void ) const
{
	return m_bValid_guess;
}

//-----------------------------------------------------------------------------
// Purpose: The socket thread's clock
//-----------------------------------------------------------------------------
float CSocket::GetClock_guess( void )
{
	return GetSocketThread_guess()->GetClock_guess();
}

//-----------------------------------------------------------------------------
// Purpose: Resolves the socket address
//-----------------------------------------------------------------------------
const netadr_t *CSocket::GetAddress_guess( void )
{
	if ( !m_bResolved_guess )
	{
		m_bResolved_guess = true;
		// Determine resolved socket address
		net_guess->GetSocketAddress_guess( m_Socket_guess, &m_Address_guess );
	}

	return &m_Address_guess;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSocket::SetUserData_guess( unsigned int userData )
{
	m_nUserData_guess = userData;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int CSocket::GetUserData_guess( void ) const
{
	return m_nUserData_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the underlying socket id number for setting up the fd_set
//-----------------------------------------------------------------------------
int CSocket::GetSocketNumber_guess( void ) const
{
	return m_Socket_guess;
}

//-----------------------------------------------------------------------------
// Purpose: Called once per frame (outside of the socket thread) to allow socket
//			to receive incoming messages
//-----------------------------------------------------------------------------
bool CSocket::ReceiveData_guess( void )
{
	// Check for data
	struct sockaddr from;
	int fromlen;
	int bytes;
	unsigned char buffer[ CMsgBuffer::NET_MAXMESSAGE_guess ];

	fromlen = sizeof( from );
	bytes = recvfrom( m_Socket_guess, (char *)buffer, CMsgBuffer::NET_MAXMESSAGE_guess, 0, (struct sockaddr *)&from, &fromlen );

	// Socket error
	if ( bytes == SOCKET_ERROR )
	{
		return false;
	}

	// Mark the time no matter what since FD_SET said there was data and we should have it now
	float recvTime = GetClock_guess();

	// Too much data, ignore it
	if ( bytes >= CMsgBuffer::NET_MAXMESSAGE_guess )
	{
		return false;
	}

	// Packets must have -1 tag
	if ( bytes < 4 )
	{
		return false;
	}

	if ( *(int *)&buffer[0] == -2 ) // its a split packet
	{
		int curPacket = 0, offset = 0;
		SPLITPACKET_guess *pak = (SPLITPACKET_guess *)&buffer[0];

		if ( m_iTotalPackets_guess == 0 ) // this is the first in the series
		{
			m_iTotalPackets_guess = ( pak->packetID & 0x0f );
			m_iSeqNo_guess = pak->sequenceNumber;
			m_iRetries_guess = 0;

			m_iCurrentPackets_guess = 1; // packet numbers start at zero, total is the total number (i.e =2 for packet 0,1)
			curPacket = ( pak->packetID & 0xf0 ) >> 4;
		}
		else if ( m_iSeqNo_guess == pak->sequenceNumber )
		{
			m_iCurrentPackets_guess++;
			curPacket = ( pak->packetID & 0xf0 ) >> 4;
		}
		else
		{
			m_iRetries_guess++;
			if ( m_iRetries_guess > MAX_RETRIES_guess ) // make sure we give up eventually on fragments
			{
				m_iTotalPackets_guess = 0;
			}
			return false;
		}

		if ( curPacket == 0 )
		{
			offset = 4; // strip the "-1" at the front of the first packet
		}

		if ( curPacket < MAX_SPLITPACKETS_guess )
		{
			m_CurPacket_guess[curPacket].Clear_guess(); // new packet, clear the buffer out
			m_CurPacket_guess[curPacket].WriteBuf_guess( bytes - offset - sizeof( SPLITPACKET_guess ), &buffer[offset + sizeof( SPLITPACKET_guess )] );
		}

		if ( m_iCurrentPackets_guess == m_iTotalPackets_guess )
		{
			EnterCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );

			// Get from address
			netadr_t addr;
			net_guess->SockAddrToNetAdr_guess( &from, &addr );

			// append to the receive buffer
			int idx = m_MsgBuffers_guess.AddToTail();
			CMsgBuffer &msgBuffer = m_MsgBuffers_guess[idx];

			msgBuffer.Clear_guess();

			// copy all our fragments together
			for ( int i = 0; i < m_iTotalPackets_guess; i++ )
			{
				m_CurPacket_guess[i].ReadBuf_guess( m_CurPacket_guess[i].GetCurSize_guess(), buffer );
				msgBuffer.WriteBuf_guess( m_CurPacket_guess[i].GetCurSize_guess(), buffer );
			}

			msgBuffer.SetTime_guess( recvTime );
			msgBuffer.SetNetAddress_guess( addr );

			LeaveCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );

			m_iTotalPackets_guess = 0; // we have collected all the fragments for this packet, we can start on a new one now
		}
	}
	else if ( *(int *)&buffer[0] == -1 ) // Must have 255,255,255,255 oob tag
	{
		EnterCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );

		// Get from address
		netadr_t addr;
		net_guess->SockAddrToNetAdr_guess( &from, &addr );

		// append to the receive buffer
		int idx = m_MsgBuffers_guess.AddToTail();
		CMsgBuffer &msgBuffer = m_MsgBuffers_guess[idx];

		// Copy data into receive buffer
		msgBuffer.Clear_guess();
		msgBuffer.WriteBuf_guess( bytes - 4, &buffer[4] );
		msgBuffer.SetTime_guess( recvTime );
		msgBuffer.SetNetAddress_guess( addr );

		LeaveCriticalSection( (CRITICAL_SECTION *)m_pBufferCS_guess );
	}

	return true;
}
