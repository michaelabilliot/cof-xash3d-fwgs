// net.h -- quake's interface to the networking layer (net_ws.c, net_chan.c)

#ifndef NET_H
#define NET_H

#include "netadr.h"

#define PORT_ANY			-1

typedef enum
{
	NS_CLIENT,
	NS_SERVER,
	NS_MULTICAST	// xxxMO
} netsrc_t;

// Size of the send buffers of Netchan_Transmit and Netchan_OutOfBand
#define NET_MAX_PAYLOAD			4037

// Max length of unreliable message
#define MAX_DATAGRAM			4000

// Max length of a reliable message
#define MAX_MSGLEN				3990

// 0 == regular, 1 == file stream
#define MAX_STREAMS				2

// Flow control bytes per second limits
#define MAX_RATE				100000.0f
#define MIN_RATE				1000.0f

// Default data rate
#define DEFAULT_RATE			( 9999.0f )

// NETWORKING INFO

// Size of fragmentation buffer internal buffers
#define FRAGMENT_SIZE			1400

// Fragment sizes the server sends to a client (SV_GetFragmentSize)
#define FRAGMENT_S2C_MIN_SIZE	256
#define FRAGMENT_S2C_MAX_SIZE	1024

// MAX_CHALLENGES is made large to prevent a denial
//  of service attack that could cycle all of them
//  out before legitimate users connected
#define MAX_CHALLENGES			1024

// Max size of udp packet payload
#define MAX_UDP_PACKET			4010

// Used for the cleartime in Netchan_Transmit: 28 bytes of IP and UDP header
#define UDP_HEADER_SIZE			28

// largest reliable payload of one packet
#define MAX_RELIABLE_PAYLOAD	1200

// the largest part of a packet that is routed without fragmentation
#define MAX_ROUTEABLE_PACKET	1400

#define MAX_LATENT				32

#define FLOW_OUTGOING			0
#define FLOW_INCOMING			1
#define MAX_FLOWS				2

#define FRAG_NORMAL_STREAM		0
#define FRAG_FILE_STREAM		1

#define MAKE_FRAGID( id, count )	( ( ( id & 0xffff ) << 16 ) | ( count & 0xffff ) )
#define FRAG_GETID( fragid )		( ( fragid >> 16 ) & 0xffff )
#define FRAG_GETCOUNT( fragid )		( fragid & 0xffff )

extern	netadr_t	net_local_adr;
extern	netadr_t	net_local_ipx_adr;
extern	netadr_t	net_from;		// address of who sent the packet
extern	sizebuf_t	net_message;

void		NET_SendPacket( netsrc_t sock, int length, void *data, netadr_t to );
char		*NET_AdrToString( netadr_t a );
qboolean	NET_CompareAdr( netadr_t a, netadr_t b );
qboolean	NET_CompareClassBAdr( netadr_t a, netadr_t b );
qboolean	NET_IsReservedAdr( netadr_t a );
qboolean	NET_CompareBaseAdr( netadr_t a, netadr_t b );
qboolean	NET_IsLocalAddress( netadr_t adr );
char		*NET_BaseAdrToString( netadr_t a );

// Message data
typedef struct flowstats_s
{
	// Size of message sent/received
	int		size;
	// Time that message was sent/received
	double	time;
} flowstats_t;

typedef struct flow_s
{
	// Data for last MAX_LATENT messages
	flowstats_t	stats[MAX_LATENT];
	// Current message position
	int			current;
	// Time when we should recompute k/sec data
	double		nextcompute;
	// Average data
	float		kbytespersec;
	float		avgkbytespersec;
} flow_t;

// Generic fragment structure
typedef struct fragbuf_s
{
	// Next buffer in chain
	struct fragbuf_s	*next;
	// Id of this buffer
	int					bufferid;
	// Message buffer where raw data is stored
	sizebuf_t			frag_message;
	// The actual data sits here
	byte				frag_message_buf[FRAGMENT_SIZE];
	// Is this a file buffer?
	qboolean			isfile;
	// Is this file buffer from memory ( custom decal, etc. ).
	qboolean			isbuffer;
	qboolean			iscompressed;
	// Name of the file to save out on remote host
	char				filename[MAX_PATH];
	// Offset in file from which to read data
	int					foffset;
	// Size of data to read at that offset
	int					size;
} fragbuf_t;

// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	// Next chain in waiting list
	struct fragbufwaiting_s	*next;
	// Number of buffers in this chain
	int						fragbufcount;
	// The actual buffers
	fragbuf_t				*fragbufs;
} fragbufwaiting_t;

// Network Connection Channel
typedef struct netchan_s
{
	// NS_SERVER or NS_CLIENT, depending on channel.
	netsrc_t			sock;

	// Address this channel is talking to.
	netadr_t			remote_address;

	int					player_slot;
	// For timeouts.  Time last message was received.
	float				last_received;
	// Time when channel was connected.
	float				connect_time;

	// Bandwidth choke
	// Bytes per second
	double				rate;
	// If realtime > cleartime, free to send next packet
	double				cleartime;

	// Sequencing variables
	//
	// Increasing count of sequence numbers
	int					incoming_sequence;
	// # of last outgoing message that has been ack'd.
	int					incoming_acknowledged;
	// Toggles T/F as reliable messages are received.
	int					incoming_reliable_acknowledged;
	// single bit, maintained local
	int					incoming_reliable_sequence;
	// Message we are sending to remote
	int					outgoing_sequence;
	// Whether the message contains reliable payload, single bit
	int					reliable_sequence;
	// Outgoing sequence number of last send that had reliable data
	int					last_reliable_sequence;

	void				*connection_status;
	int					( *pfnNetchan_Blocksize )( void * );

	// Staging and holding areas
	sizebuf_t			message;
	byte				message_buf[MAX_MSGLEN];

	// Reliable message buffer.  We keep adding to it until reliable is acknowledged.  Then we clear it.
	int					reliable_length;
	byte				reliable_buf[MAX_MSGLEN];

	// Waiting list of buffered fragments to go onto queue.  Multiple outgoing buffers can be queued in succession.
	fragbufwaiting_t	*waitlist[MAX_STREAMS];

	// Is reliable waiting buf a fragment?
	int					reliable_fragment[MAX_STREAMS];
	// Buffer id for each waiting fragment
	unsigned int		reliable_fragid[MAX_STREAMS];

	// The current fragment being set
	fragbuf_t			*fragbufs[MAX_STREAMS];
	// The total number of fragments in this stream
	int					fragbufcount[MAX_STREAMS];

	// Position in outgoing buffer where frag data starts
	short				frag_startpos[MAX_STREAMS];
	// Length of frag data in the buffer
	short				frag_length[MAX_STREAMS];

	// Incoming fragments are stored here
	fragbuf_t			*incomingbufs[MAX_STREAMS];
	// Set to true when incoming data is ready
	qboolean			incomingready[MAX_STREAMS];

	// Only referenced by the FRAG_FILE_STREAM component
	// Name of file being downloaded
	char				incomingfilename[MAX_PATH];

	void				*tempbuffer;
	int					tempbuffersize;

	// Incoming and outgoing flow metrics
	flow_t				flow[MAX_FLOWS];
} netchan_t;

extern	struct cvar_s	net_showpackets;
extern	int		net_drop;			// packets dropped before this one

extern	char	gDownloadFile[256];

void		Netchan_UnlinkFragment( fragbuf_t *buf, fragbuf_t **list );
void		Netchan_OutOfBand( netsrc_t sock, netadr_t adr, int length, byte *data );
void		Netchan_OutOfBandPrint( netsrc_t sock, netadr_t adr, char *format, ... );
void		Netchan_ClearFragbufs( fragbuf_t **ppbuf );
void		Netchan_ClearFragments( netchan_t *chan );
void		Netchan_Clear( netchan_t *chan );
void		Netchan_Setup( netsrc_t socketnumber, netchan_t *chan, netadr_t adr, int player_slot, void *connection_status, int ( *pfnNetchan_Blocksize )( void * ) );
qboolean	Netchan_CanPacket( netchan_t *chan );
void		Netchan_UpdateFlow( netchan_t *chan );
void		Netchan_Transmit( netchan_t *chan, int length, byte *data );
fragbuf_t	*Netchan_FindBufferById( fragbuf_t **pplist, int id, qboolean allocate );
void		Netchan_CheckForCompletion( netchan_t *chan, int stream, int intotalbuffers );
qboolean	Netchan_Validate( netchan_t *chan, qboolean *frag_message, unsigned int *fragid, int *frag_offset, int *frag_length );
qboolean	Netchan_Process( netchan_t *chan );
void		Netchan_FragSend( netchan_t *chan );
void		Netchan_AddBufferToList( fragbuf_t **pplist, fragbuf_t *pbuf );
fragbuf_t	*Netchan_AllocFragbuf( void );
void		Netchan_AddFragbufToTail( fragbufwaiting_t *wait, fragbuf_t *buf );
void		Netchan_CreateFragments_( qboolean server, netchan_t *chan, sizebuf_t *msg );
void		Netchan_CreateFragments( qboolean server, netchan_t *chan, sizebuf_t *msg );
void		Netchan_CreateFileFragmentsFromBuffer( qboolean server, netchan_t *chan, char *filename, unsigned char *uncompressed_pbuf, int uncompressed_size );
int			Netchan_CreateFileFragments( qboolean server, netchan_t *chan, char *filename );
void		Netchan_FlushIncoming( netchan_t *chan, int stream );
qboolean	Netchan_CopyNormalFragments( netchan_t *chan );
qboolean	Netchan_CopyFileFragments( netchan_t *chan );
qboolean	Netchan_IsSending( netchan_t *chan );
qboolean	Netchan_IsReceiving( netchan_t *chan );
qboolean	Netchan_IncomingReady( netchan_t *chan );
void		Netchan_UpdateProgress( netchan_t *chan );
void		Netchan_Init( void );
qboolean	Netchan_CompressPacket( sizebuf_t *chan );
qboolean	Netchan_DecompressPacket( sizebuf_t *chan );

qboolean	NET_StringToAdr( char *s, netadr_t *a );

extern	qboolean	noip;
extern	qboolean	noipx;

unsigned short	Q_ntohs( unsigned short netshort );
void		NET_Config( qboolean multiplayer );
void		NET_ClearLagData( qboolean bClient, qboolean bServer );
void		NET_FlushSocket( netsrc_t sock );
qboolean	NET_GetPacket( netsrc_t sock );
qboolean	NET_JoinGroup( netsrc_t sock, netadr_t addr );
qboolean	NET_LeaveGroup( netsrc_t sock, netadr_t addr );

// net_api.c
int			Net_StringToAdr( char *s, netadr_t *a );
void		Net_APIInit( void );
void		Net_APIShutDown( void );

// Client connection is initiated by requesting a challenge value
//  the server sends this value back
#define	S2C_CHALLENGE	'A'		// + challenge value
// Send a userid, client remote address, is this server secure and engine build number
#define	S2C_CONNECTION	'B'
// Print to client console
#define	A2A_PRINT		'l'		// print a message on client

// Response details about server
#define	S2A_INFO		'C'		// + Address, hostname, map, gamedir, gamedescription, active players, maxplayers, protocol
#define	S2A_INFO_DETAILED	'm'	// New Query protocol, returns dedicated or not, + other performance info.

// Response to player info requests
#define	S2A_PLAYER		'D'		// + Playernum, name, frags, /*deaths*/, time on server

// Response to rule info requests
#define	S2A_RULES		'E'		// + number of rules + string key and string value pairs

// Generic Ping Request
#define	A2A_PING		'i'		// respond with an A2A_ACK

// Generic Ack
#define	A2A_ACK			'j'		// general acknowledgement without info

// Batch of server addresses from the master
#define	A2M_GET_SERVERS_BATCH	'e'	// + unique id of the batch to start from
#define	M2A_SERVER_BATCH	'f'		// + unique id + list of ip:port

// Challenge response from master
#define	M2A_CHALLENGE	's'		// + challenge value

// Server and master queries that the 2013 server ignores
#define	A2A_GETCHALLENGE	'W'	// Request challenge # from another machine
#define	A2S_INFO		'T'
#define	A2S_PLAYER		'U'
#define	A2S_RULES		'V'
#define	S2A_LOGSTRING	'R'		// send a log string
#define	M2S_REQUESTRESTART	'O'	// HLMaster rejected a server's connection because the server needs to be updated

// The connection was refused
#define	S2C_ERROR		'9'		// + error string
#define	S2C_BADPASSWORD	'8'		// special code: password required

//
// client to server
//
#define	clc_bad				0
#define	clc_nop				1
#define	clc_move			2		// [usercmd_t]
#define	clc_stringcmd		3		// [string] message
#define	clc_delta			4		// [byte] sequence number, requests delta compression of message
#define	clc_resourcelist	5
#define	clc_tmove			6
#define	clc_fileconsistency	7
#define	clc_voicedata		8
#define	clc_hltv			9
#define	clc_cvarvalue		10
#define	clc_cvarvalue2		11

// server to master/log listener: a log line with the secret key
#define	S2A_LOGKEY		'S'

#endif // NET_H
