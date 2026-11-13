// net_chan.c -- the reliable sequenced channel over UDP, with fragmented
// transfers of big messages and files

#include "quakedef.h"
#include "mathlib.h"
#include "usercmd.h"
#include "cvardef.h"
#include "cvar.h"
#include "cmd.h"
#include "mem.h"
#include "zone.h"
#include "APIProxy.h"
#include "cdll_int.h"
#include "host.h"
#include "net.h"
#include "delta.h"
#include "server.h"
#include "client.h"
#include "screen.h"
#include "bzlib.h"

#include <stdarg.h>

#define svc_nop		1

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are allways placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

*/

int			net_drop;

cvar_t		net_log = { "net_log", "0" };
cvar_t		net_showpackets = { "net_showpackets", "0" };
cvar_t		net_showdrop = { "net_showdrop", "0" };
cvar_t		net_drawslider = { "net_drawslider", "0" };
cvar_t		net_chokeloopback = { "net_chokeloop", "0" };
cvar_t		sv_filetransfercompression = { "sv_filetransfercompression", "1" };
cvar_t		sv_filetransfermaxsize = { "sv_filetransfermaxsize", "10485760" };

char		gDownloadFile[256];

/*
===============
Netchan_UnlinkFragment

===============
*/
void Netchan_UnlinkFragment( fragbuf_t *buf, fragbuf_t **list )
{
	fragbuf_t	*search;

	if ( !list )
	{
		Con_Printf( "Netchan_UnlinkFragment:  Asked to unlink fragment from empty list, ignored\n" );
		return;
	}

	// At head of list
	if ( buf == *list )
	{
		// Remove first element
		*list = buf->next;

		// Destroy remnant
		Mem_Free( buf );
		return;
	}

	search = *list;
	while ( search->next )
	{
		if ( search->next == buf )
		{
			search->next = buf->next;

			// Destroy remnant
			Mem_Free( buf );
			return;
		}

		search = search->next;
	}

	Con_Printf( "Netchan_UnlinkFragment:  Couldn't find fragment\n" );
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand( netsrc_t sock, netadr_t adr, int length, byte *data )
{
	sizebuf_t	send;
	byte		send_buf[NET_MAX_PAYLOAD];

	// write the packet header
	send.buffername = "Netchan_OutOfBand";
	send.data = send_buf;
	send.maxsize = sizeof( send_buf );
	send.cursize = 0;
	send.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteLong( &send, -1 );	// -1 sequence means out of band
	SZ_Write( &send, data, length );

	// send the datagram
	if ( !cls.demoplayback )
		NET_SendPacket( sock, send.cursize, send.data, adr );
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint( netsrc_t sock, netadr_t adr, char *format, ... )
{
	va_list		argptr;
	static char	string[8192];		// ??? why static?

	va_start( argptr, format );
	_vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Netchan_OutOfBand( sock, adr, Q_strlen( string ) + 1, (byte *)string );
}

/*
==============================
Netchan_ClearFragbufs

==============================
*/
void Netchan_ClearFragbufs( fragbuf_t **ppbuf )
{
	fragbuf_t	*buf, *n;

	if ( !ppbuf )
		return;

	// Throw away any that are sitting around
	buf = *ppbuf;
	while ( buf )
	{
		n = buf->next;
		Mem_Free( buf );
		buf = n;
	}
	*ppbuf = NULL;
}

/*
==============================
Netchan_ClearFragments

==============================
*/
void Netchan_ClearFragments( netchan_t *chan )
{
	int		i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		fragbufwaiting_t	*wait, *next;

		wait = chan->waitlist[i];
		while ( wait )
		{
			next = wait->next;
			Netchan_ClearFragbufs( &wait->fragbufs );
			Mem_Free( wait );
			wait = next;
		}
		chan->waitlist[i] = NULL;

		Netchan_ClearFragbufs( &chan->fragbufs[i] );
		Netchan_FlushIncoming( chan, i );
	}
}

/*
==============================
Netchan_Clear

==============================
*/
void Netchan_Clear( netchan_t *chan )
{
	int		i;

	Netchan_ClearFragments( chan );

	if ( chan->reliable_length )
	{
		// prints the reliable length where it says the reliable sequence
		Con_DPrintf( "Netchan_Clear() : reliable length not 0, reliable_sequence: %d, incoming_reliable_acknowledged: %d\n", chan->reliable_length, chan->incoming_reliable_acknowledged );
		chan->reliable_sequence ^= 1;
		chan->reliable_length = 0;
	}

	chan->cleartime = 0.0;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		chan->reliable_fragid[i] = 0;
		chan->reliable_fragment[i] = 0;
		chan->fragbufcount[i] = 0;
		chan->frag_startpos[i] = 0;
		chan->frag_length[i] = 0;
		chan->incomingready[i] = false;
	}

	if ( chan->tempbuffer )
	{
		Mem_Free( chan->tempbuffer );
		chan->tempbuffer = NULL;
	}
	chan->tempbuffersize = 0;
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup( netsrc_t socketnumber, netchan_t *chan, netadr_t adr, int player_slot, void *connection_status, int ( *pfnNetchan_Blocksize )( void * ) )
{
	Netchan_Clear( chan );

	Q_memset( chan, 0, sizeof( *chan ) );

	chan->player_slot = player_slot + 1;
	chan->sock = socketnumber;
	chan->remote_address = adr;
	chan->last_received = realtime;
	chan->connect_time = realtime;

	chan->message.data = chan->message_buf;
	chan->message.flags = FSB_ALLOWOVERFLOW;
	chan->message.maxsize = sizeof( chan->message_buf );
	chan->message.buffername = "netchan->message";

	chan->rate = DEFAULT_RATE;

	// Prevent the first message from getting dropped after connection is set up.
	chan->outgoing_sequence = 1;

	chan->connection_status = connection_status;
	chan->pfnNetchan_Blocksize = pfnNetchan_Blocksize;
}

/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
qboolean Netchan_CanPacket( netchan_t *chan )
{
	// Never choke loopback packets.
	if ( !net_chokeloopback.value && chan->remote_address.type == NA_LOOPBACK )
	{
		chan->cleartime = realtime;
		return true;
	}

	if ( chan->cleartime < realtime )
		return true;

	return false;
}

#define FLOW_INTERVAL	0.1		// Compute data rate every 0.1 seconds
#define FLOW_AVG		( 2.0 / 3.0 )	// cur * 1/3 + avg * 2/3

/*
==============================
Netchan_UpdateFlow

==============================
*/
void Netchan_UpdateFlow( netchan_t *chan )
{
	int				flow;
	int				start;
	int				i;
	int				bytes = 0;
	float			faccumulatedtime = 0.0;
	flow_t			*pflow;
	flowstats_t		*pstat, *pprev;

	if ( !chan )
		return;

	// the byte and time sums are not reset between the two flows
	for ( flow = 0; flow < MAX_FLOWS; flow++ )
	{
		pflow = &chan->flow[flow];

		if ( ( realtime - pflow->nextcompute ) < FLOW_INTERVAL )
			continue;

		pflow->nextcompute = realtime + FLOW_INTERVAL;

		start = pflow->current - 1;

		// Compute data rate
		pprev = &pflow->stats[start & ( MAX_LATENT - 1 )];

		for ( i = 1; i < MAX_LATENT / 2; i++ )
		{
			// Most recent message then backward from there
			pstat = &pflow->stats[( start - i ) & ( MAX_LATENT - 1 )];

			bytes += pstat->size;
			faccumulatedtime += pprev->time - pstat->time;

			pprev = pstat;
		}

		if ( !faccumulatedtime )
		{
			pflow->kbytespersec = 0.0;
		}
		else
		{
			pflow->kbytespersec = (float)bytes / faccumulatedtime;
			pflow->kbytespersec /= 1024.0;
		}

		pflow->avgkbytespersec = pflow->avgkbytespersec * FLOW_AVG + pflow->kbytespersec * ( 1.0 - FLOW_AVG );
	}
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit( netchan_t *chan, int length, byte *data )
{
	sizebuf_t		send;
	byte			send_buf[NET_MAX_PAYLOAD];
	qboolean		send_reliable;
	qboolean		send_reliable_fragment;
	qboolean		send_resending = false;
	int				max_send_size;
	unsigned int	w1, w2;
	int				i, j;
	float			fRate;

	// get size of message
	send.buffername = "Netchan_Transmit";
	send.data = send_buf;
	send.maxsize = sizeof( send_buf );
	send.cursize = 0;
	send.flags = 0;

	// check for message overflow
	if ( chan->message.flags & FSB_OVERFLOWED )
	{
		Con_Printf( "%s:Outgoing message overflow\n", NET_AdrToString( chan->remote_address ) );
		return;
	}

	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if ( chan->incoming_acknowledged > chan->last_reliable_sequence &&
		chan->incoming_reliable_acknowledged != chan->reliable_sequence )
	{
		send_reliable = true;
		send_resending = true;
	}

	// A packet can have "reliable payload + frag payload + unreliable payload
	// frag payload can be a file chunk, if so, it needs to be parsed on the receiving end and reliable payload + unreliable payload need
	// to be passed on to the message queue.  The processing routine needs to be able to handle the case where a message comes in and a file
	// transfer completes

	// if the reliable transmit buffer is empty, copy the current message out
	if ( !chan->reliable_length )
	{
		qboolean	send_frag = false;
		fragbuf_t	*pbuf;
		int			send_from_frag[MAX_STREAMS] = { 0, 0 };
		int			send_from_regular = 0;

		// will be true if we are active and should let chan->message get some bandwidth

		// If we have data in the waiting list(s) and we have cleared the current queue(s), then
		// push the waitlist(s) into the current queue(s)
		Netchan_FragSend( chan );

		// Sending regular payload
		send_from_regular = ( chan->message.cursize != 0 );

		// Check to see if we are sending a frag payload
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			if ( chan->fragbufs[i] )
			{
				send_from_frag[i] = 1;
			}
		}

		// Stall reliable payloads if sending from frag buffer
		if ( send_from_regular && send_from_frag[FRAG_NORMAL_STREAM] )
		{
			send_from_regular = false;

			// If the reliable buffer has gotten too big, queue it at the end of everything and clear out buffer
			if ( chan->message.cursize > MAX_RELIABLE_PAYLOAD )
			{
				Netchan_CreateFragments( chan == &cls.netchan ? false : true, chan, &chan->message );
				chan->message.cursize = 0;
			}
		}

		// Startpos will be zero if there is no regular payload
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			chan->frag_startpos[i] = 0;

			// Assume no fragment is being sent
			chan->reliable_fragment[i] = 0;
			chan->reliable_fragid[i] = 0;
			chan->frag_length[i] = 0;

			if ( send_from_frag[i] )
			{
				send_frag = true;
			}
		}

		if ( send_from_regular || send_frag )
		{
			chan->reliable_sequence ^= 1;
			send_reliable = true;
		}

		if ( send_from_regular )
		{
			Q_memcpy( chan->reliable_buf, chan->message_buf, chan->message.cursize );
			chan->reliable_length = chan->message.cursize;
			chan->message.cursize = 0;

			// If we send fragments, this is where they'll start
			for ( i = 0; i < MAX_STREAMS; i++ )
			{
				chan->frag_startpos[i] = chan->reliable_length;
			}
		}

		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			int				fragment_size;
			FileHandle_t	hfile;

			// Is there something in the fragbuf?
			pbuf = chan->fragbufs[i];

			if ( pbuf )
			{
				fragment_size = pbuf->frag_message.cursize;

				// Files set size a bit differently.
				if ( pbuf->isfile && !pbuf->isbuffer )
				{
					fragment_size = pbuf->size;
				}
			}

			// Make sure we have enought space left
			if ( send_from_frag[i] && pbuf && ( chan->reliable_length + fragment_size ) < MAX_RELIABLE_PAYLOAD )
			{
				chan->reliable_fragid[i] = MAKE_FRAGID( pbuf->bufferid, chan->fragbufcount[i] ); // Which buffer are we sending?

				// If it's not in-memory, then we'll need to copy it in frame the file handle.
				if ( pbuf->isfile && !pbuf->isbuffer )
				{
					char			compressedfilename[MAX_PATH];

					if ( pbuf->iscompressed )
					{
						_snprintf( compressedfilename, sizeof( compressedfilename ), "%s.ztmp", pbuf->filename );
						hfile = FS_Open( compressedfilename, "rb" );
					}
					else
					{
						hfile = FS_Open( pbuf->filename, "rb" );
					}

					FS_Seek( hfile, pbuf->foffset, FILESYSTEM_SEEK_HEAD );
					FS_Read( &pbuf->frag_message.data[pbuf->frag_message.cursize], pbuf->size, 1, hfile );
					pbuf->frag_message.cursize += pbuf->size;
					FS_Close( hfile );
				}

				// Copy frag stuff on top of current buffer
				Q_memcpy( chan->reliable_buf + chan->reliable_length, pbuf->frag_message.data, pbuf->frag_message.cursize );

				chan->reliable_length += pbuf->frag_message.cursize;
				chan->frag_length[i] = pbuf->frag_message.cursize;

				// Unlink pbuf
				Netchan_UnlinkFragment( pbuf, &chan->fragbufs[i] );

				chan->reliable_fragment[i] = 1;

				// Offset the rest of the starting positions
				for ( j = i + 1; j < MAX_STREAMS; j++ )
				{
					chan->frag_startpos[j] += chan->frag_length[i];
				}
			}
		}
	}

	// Prepare the packet header
	w1 = chan->outgoing_sequence | ( send_reliable << 31 );
	w2 = chan->incoming_sequence | ( chan->incoming_reliable_sequence << 31 );

	send_reliable_fragment = false;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->reliable_fragment[i] )
		{
			send_reliable_fragment = true;
			break;
		}
	}

	if ( send_reliable && send_reliable_fragment )
	{
		w1 |= ( 1 << 30 );
	}

	chan->outgoing_sequence++;

	MSG_WriteLong( &send, w1 );
	MSG_WriteLong( &send, w2 );

	if ( send_reliable && send_reliable_fragment )
	{
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			if ( chan->reliable_fragment[i] )
			{
				MSG_WriteByte( &send, 1 );
				MSG_WriteLong( &send, chan->reliable_fragid[i] );
				MSG_WriteShort( &send, chan->frag_startpos[i] );
				MSG_WriteShort( &send, chan->frag_length[i] );
			}
			else
			{
				MSG_WriteByte( &send, 0 );
			}
		}
	}

	// Copy the reliable message to the packet first
	if ( send_reliable )
	{
		SZ_Write( &send, chan->reliable_buf, chan->reliable_length );
		chan->last_reliable_sequence = chan->outgoing_sequence - 1;
	}

	// Is there room for the unreliable payload?
	max_send_size = send_resending ? MAX_ROUTEABLE_PACKET : send.maxsize;

	if ( max_send_size - send.cursize >= length )
		SZ_Write( &send, data, length );
	else
		Con_DPrintf( "Netchan_Transmit:  Unreliable would overfow, ignoring\n" );

	// Deal with packets that are too small for some networks
	if ( send.cursize < 16 )	// Packet too small for some networks
	{
		int		i;

		// Go ahead and pad a full 16 extra bytes -- this only happens during authentication / signon
		for ( i = send.cursize; i < 16; i++ )
		{
			// Note that the server can parse svc_nop, too.
			MSG_WriteByte( &send, svc_nop );
		}
	}

	chan->flow[FLOW_OUTGOING].stats[chan->flow[FLOW_OUTGOING].current & ( MAX_LATENT - 1 )].size = send.cursize + UDP_HEADER_SIZE;
	chan->flow[FLOW_OUTGOING].stats[chan->flow[FLOW_OUTGOING].current & ( MAX_LATENT - 1 )].time = realtime;
	chan->flow[FLOW_OUTGOING].current++;

	Netchan_UpdateFlow( chan );

	if ( !cls.demoplayback )
	{
		Netchan_CompressPacket( &send );

		COM_Munge2( send.data + 8, send.cursize - 8, ( chan->outgoing_sequence - 1 ) & 0xFF );

		if ( g_modfuncs.m_pfnProcessOutgoingNet )
			g_modfuncs.m_pfnProcessOutgoingNet( chan, &send );

		NET_SendPacket( chan->sock, send.cursize, send.data, chan->remote_address );
	}

	if ( sv.active && sv_lan.value && sv_lan_rate.value > MIN_RATE )
		fRate = 1.0f / sv_lan_rate.value;
	else
		fRate = 1.0 / chan->rate;

	if ( chan->cleartime < realtime )
	{
		chan->cleartime = realtime;
	}

	chan->cleartime += ( send.cursize + UDP_HEADER_SIZE ) * fRate;

	if ( net_showpackets.value && net_showpackets.value != 2.0 )
	{
		int		mask = 63;
		char	c;

		c = ( chan == &cls.netchan ) ? 'c' : 's';

		Con_Printf( " %c --> sz=%i seq=%i ack=%i rel=%i tm=%f\n"
			, c
			, send.cursize
			, chan->outgoing_sequence - 1
			, chan->incoming_sequence
			, send_reliable ? 1 : 0
			, (float)( chan == &cls.netchan ? cl.time : sv.time ) );
	}
}

/*
==============================
Netchan_FindBufferById

==============================
*/
fragbuf_t *Netchan_FindBufferById( fragbuf_t **pplist, int id, qboolean allocate )
{
	fragbuf_t	*list;
	fragbuf_t	*pnewbuf;

	list = *pplist;
	while ( list )
	{
		if ( list->bufferid == id )
			return list;

		list = list->next;
	}

	if ( !allocate )
		return NULL;

	// Create new entry
	pnewbuf = (fragbuf_t *)Mem_ZeroMalloc( sizeof( fragbuf_t ) );
	pnewbuf->bufferid = id;
	pnewbuf->frag_message.cursize = 0;
	pnewbuf->frag_message.data = pnewbuf->frag_message_buf;
	pnewbuf->frag_message.maxsize = sizeof( pnewbuf->frag_message_buf );
	pnewbuf->frag_message.buffername = "Frag Buffer";
	pnewbuf->frag_message.flags = FSB_ALLOWOVERFLOW;

	Netchan_AddBufferToList( pplist, pnewbuf );

	return pnewbuf;
}

/*
==============================
Netchan_CheckForCompletion

==============================
*/
void Netchan_CheckForCompletion( netchan_t *chan, int stream, int intotalbuffers )
{
	int			c;
	int			size;
	int			id;
	fragbuf_t	*p;
	char		szCommand[32];

	size = 0;
	c = 0;

	p = chan->incomingbufs[stream];
	if ( !p )
		return;

	while ( p )
	{
		size += p->frag_message.cursize;
		c++;

		id = FRAG_GETID( p->bufferid );
		if ( id != c )
		{
			if ( chan == &cls.netchan )
			{
				if ( chan->sock == NS_MULTICAST )
				{
					_snprintf( szCommand, sizeof( szCommand ), "listen %s\n", NET_AdrToString( chan->remote_address ) );
					Cbuf_AddText( szCommand );
					return;
				}

				Con_Printf( "Netchan_CheckForCompletion:  Lost/dropped fragment would cause stall, retrying connection\n" );
				Cbuf_AddText( "retry\n" );
			}
		}

		p = p->next;
	}

	// Received final message
	if ( c == intotalbuffers )
	{
		chan->incomingready[stream] = true;
	}
}

#define MAX_FRAGMENTS	25000

/*
==============================
Netchan_Validate

==============================
*/
qboolean Netchan_Validate( netchan_t *chan, qboolean *frag_message, unsigned int *fragid, int *frag_offset, int *frag_length )
{
	int		i;
	int		buffer;
	int		count;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( !frag_message[i] )
			continue;

		buffer = FRAG_GETID( fragid[i] );
		count = FRAG_GETCOUNT( fragid[i] );

		// total fragments should be <= MAX_FRAGMENTS and current fragment can't be > total fragments
		if ( buffer < 0 || buffer > MAX_FRAGMENTS )
			break;

		if ( count < 0 || count > MAX_FRAGMENTS )
			break;

		if ( frag_length[i] < 0 || frag_length[i] > 2048 )
			break;

		if ( frag_offset[i] < 0 || frag_offset[i] > 16384 )
			break;
	}

	if ( i < MAX_STREAMS )
		return false;

	return true;
}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process( netchan_t *chan )
{
	int				i;
	unsigned int	sequence, sequence_ack;
	unsigned int	reliable_ack, reliable_message;
	unsigned int	fragid[MAX_STREAMS] = { 0, 0 };
	qboolean		frag_message[MAX_STREAMS] = { false, false };
	int				frag_offset[MAX_STREAMS] = { 0, 0 };
	int				frag_length[MAX_STREAMS] = { 0, 0 };
	qboolean		message_contains_fragments;

	if ( !cls.demoplayback && !cls.passive )
	{
		if ( !NET_CompareAdr( net_from, chan->remote_address ) )
			return false;
	}

	chan->last_received = realtime;

	// get sequence numbers
	MSG_BeginReading();
	sequence = MSG_ReadLong();
	sequence_ack = MSG_ReadLong();

	if ( ( sequence_ack & 0x40000000 ) && !g_modfuncs.m_pfnProcessIncomingNet )
		return false;

	if ( g_modfuncs.m_pfnProcessIncomingNet )
	{
		if ( !g_modfuncs.m_pfnProcessIncomingNet( chan, &net_message ) )
			return false;
	}

	COM_UnMunge2( net_message.data + 8, net_message.cursize - 8, sequence & 0xFF );

	Netchan_DecompressPacket( &net_message );

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	message_contains_fragments = ( sequence & ( 1 << 30 ) ) ? true : false;

	if ( message_contains_fragments )
	{
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			if ( MSG_ReadByte() )
			{
				frag_message[i] = true;
				fragid[i] = MSG_ReadLong();
				frag_offset[i] = MSG_ReadShort();
				frag_length[i] = MSG_ReadShort();
			}
		}

		if ( !Netchan_Validate( chan, frag_message, fragid, frag_offset, frag_length ) )
			return false;
	}

	sequence &= ~( 1 << 31 );
	sequence &= ~( 1 << 30 );
	sequence_ack &= ~( 1 << 31 );
	sequence_ack &= ~( 1 << 30 );

	if ( net_showpackets.value && net_showpackets.value != 3.0 )
	{
		int		mask = 63;
		char	c;

		c = ( chan == &cls.netchan ) ? 'c' : 's';

		Con_Printf( " %c <-- sz=%i seq=%i ack=%i rel=%i tm=%f\n"
			, c
			, net_message.cursize
			, sequence
			, sequence_ack
			, reliable_message
			, (float)( chan == &cls.netchan ? cl.time : sv.time ) );
	}

	// discard stale or duplicated packets
	if ( sequence <= (unsigned)chan->incoming_sequence )
	{
		if ( net_showdrop.value )
		{
			if ( sequence == (unsigned)chan->incoming_sequence )
			{
				Con_Printf( "%s:duplicate packet %i at %i\n"
					, NET_AdrToString( chan->remote_address )
					, sequence
					, chan->incoming_sequence );
			}
			else
			{
				Con_Printf( "%s:out of order packet %i at %i\n"
					, NET_AdrToString( chan->remote_address )
					, sequence
					, chan->incoming_sequence );
			}
		}
		return false;
	}

	// dropped packets don't keep the message from being used
	net_drop = sequence - ( chan->incoming_sequence + 1 );
	if ( net_drop > 0 && net_showdrop.value )
	{
		Con_Printf( "%s:Dropped %i packets at %i\n"
			, NET_AdrToString( chan->remote_address )
			, sequence - ( chan->incoming_sequence + 1 )
			, sequence );
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if ( reliable_ack == (unsigned)chan->reliable_sequence )
	{
		// Make sure we actually could have ack'd this message
		if ( chan->incoming_acknowledged + 1 >= chan->last_reliable_sequence )
		{
			chan->reliable_length = 0;	// it has been received
		}
	}

	// if this message contains a reliable message, bump incoming_reliable_sequence
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if ( reliable_message )
	{
		chan->incoming_reliable_sequence ^= 1;
	}

	// Update data flow stats
	chan->flow[FLOW_INCOMING].stats[chan->flow[FLOW_INCOMING].current & ( MAX_LATENT - 1 )].size = net_message.cursize + UDP_HEADER_SIZE;
	chan->flow[FLOW_INCOMING].stats[chan->flow[FLOW_INCOMING].current & ( MAX_LATENT - 1 )].time = realtime;
	chan->flow[FLOW_INCOMING].current++;

	Netchan_UpdateFlow( chan );

	if ( message_contains_fragments )
	{
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			int				j;
			unsigned char	*src, *dst;
			int				len;
			fragbuf_t		*pbuf;
			int				inbufferid;
			int				intotalbuffers;

			if ( !frag_message[i] )
				continue;

			inbufferid = FRAG_GETID( fragid[i] );
			intotalbuffers = FRAG_GETCOUNT( fragid[i] );

			if ( fragid[i] != 0 )
			{
				pbuf = Netchan_FindBufferById( &chan->incomingbufs[i], fragid[i], true );
				if ( pbuf )
				{
					int		nbytes = frag_length[i];

					SZ_Clear( &pbuf->frag_message );
					SZ_Write( &pbuf->frag_message, &net_message.data[msg_readcount + frag_offset[i]], nbytes );

					if ( pbuf->frag_message.flags & FSB_OVERFLOWED )
					{
						if ( chan != &cls.netchan )
							chan->message.flags |= FSB_OVERFLOWED;
						return false;
					}
				}
				else
				{
					Con_Printf( "Netchan_Process:  Couldn't allocate or find buffer %i\n", inbufferid );
				}

				// Count # of incoming bufs we've queued? are we done?
				Netchan_CheckForCompletion( chan, i, intotalbuffers );
			}

			// Rearrange incoming data to not have the frag stuff in the middle of it
			src = &net_message.data[msg_readcount + frag_offset[i] + frag_length[i]];
			dst = &net_message.data[msg_readcount + frag_offset[i]];
			len = net_message.cursize - frag_length[i] - frag_offset[i] - msg_readcount;

			for ( j = 0; j < len; j++ )
			{
				*dst++ = *src++;
			}

			net_message.cursize -= frag_length[i];

			// fragments order already validated
			for ( j = i + 1; j < MAX_STREAMS; j++ )
			{
				frag_offset[j] -= frag_length[i];
			}
		}

		// Is there anything left to process?
		if ( net_message.cursize <= 16 )
			return false;
	}

	return true;
}

/*
==============================
Netchan_FragSend

Fragmentation buffer is full and user is prepared to send
==============================
*/
void Netchan_FragSend( netchan_t *chan )
{
	fragbufwaiting_t	*wait;
	int					i;

	if ( !chan )
		return;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		// Already something queued up, just leave in waitlist
		if ( chan->fragbufs[i] )
			continue;

		// Nothing to queue?
		if ( !chan->waitlist[i] )
			continue;

		wait = chan->waitlist[i];
		chan->waitlist[i] = chan->waitlist[i]->next;

		wait->next = NULL;

		// Copy in to fragbuf
		chan->fragbufs[i] = wait->fragbufs;
		chan->fragbufcount[i] = wait->fragbufcount;

		// Throw away wait list
		Mem_Free( wait );
	}
}

/*
==============================
Netchan_AddBufferToList

==============================
*/
void Netchan_AddBufferToList( fragbuf_t **pplist, fragbuf_t *pbuf )
{
	// Find best slot
	fragbuf_t	*pprev, *n;
	int			id1, id2;

	pbuf->next = NULL;

	if ( !pplist )
		return;

	if ( !*pplist )
	{
		pbuf->next = *pplist;
		*pplist = pbuf;
		return;
	}

	pprev = *pplist;
	while ( pprev->next )
	{
		n = pprev->next; // Next item in list
		id1 = FRAG_GETID( n->bufferid );
		id2 = FRAG_GETID( pbuf->bufferid );

		if ( id1 > id2 )
		{
			// Insert here; links pbuf past n, so n drops out of the list
			pbuf->next = n->next;
			pprev->next = pbuf;
			return;
		}

		pprev = pprev->next;
	}

	// Insert at end
	pprev->next = pbuf;
}

/*
==============================
Netchan_AllocFragbuf

==============================
*/
fragbuf_t *Netchan_AllocFragbuf( void )
{
	fragbuf_t	*buf;

	buf = (fragbuf_t *)Mem_ZeroMalloc( sizeof( fragbuf_t ) );
	buf->bufferid = 0;
	buf->frag_message.cursize = 0;
	buf->frag_message.data = buf->frag_message_buf;
	buf->frag_message.maxsize = sizeof( buf->frag_message_buf );
	buf->frag_message.buffername = "Frag Buffer Alloc'd";
	buf->next = NULL;

	return buf;
}

/*
==============================
Netchan_AddFragbufToTail

==============================
*/
void Netchan_AddFragbufToTail( fragbufwaiting_t *wait, fragbuf_t *buf )
{
	fragbuf_t	*p;

	buf->next = NULL;
	wait->fragbufcount++;

	if ( !wait->fragbufs )
	{
		wait->fragbufs = buf;
	}
	else
	{
		p = wait->fragbufs;
		while ( p->next )
		{
			p = p->next;
		}
		p->next = buf;
	}
}

/*
==============================
Netchan_CreateFragments_

==============================
*/
void Netchan_CreateFragments_( qboolean server, netchan_t *chan, sizebuf_t *msg )
{
	fragbuf_t			*buf;
	int					chunksize;
	int					send;
	int					remaining;
	int					pos;
	int					bufferid = 1;
	fragbufwaiting_t	*wait, *p;

	if ( msg->cursize == 0 )
		return;

	// Compress if not done already
	if ( msg->data[0] != 'B' || msg->data[1] != 'Z' || msg->data[2] != '2' || msg->data[3] != '\0' )
	{
		unsigned char	compressed[65536];
		char			hdr[4];
		unsigned int	compressedSize;

		hdr[0] = 'B';
		hdr[1] = 'Z';
		hdr[2] = '2';
		hdr[3] = '\0';

		compressedSize = msg->cursize - sizeof( hdr );	// we should fit in same data buffer minus 4 bytes for a header

		if ( !BZ2_bzBuffToBuffCompress( (char *)compressed, &compressedSize, (char *)msg->data, msg->cursize, 9, 0, 30 ) )
		{
			Con_DPrintf( "Compressing split packet (%d -> %d bytes)\n", msg->cursize, compressedSize );
			memcpy( msg->data, hdr, sizeof( hdr ) );
			memcpy( msg->data + sizeof( hdr ), compressed, compressedSize );
			msg->cursize = compressedSize + sizeof( hdr );
		}
	}

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	wait = (fragbufwaiting_t *)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	remaining = msg->cursize;
	pos = 0;
	while ( remaining > 0 )
	{
		send = min( remaining, chunksize );
		remaining -= send;

		buf = Netchan_AllocFragbuf();
		if ( !buf )
		{
			Con_Printf( "Couldn't allocate fragbuf_t\n" );
			Mem_Free( wait );

			if ( server )
				SV_DropClient( host_client, 0, "Malloc problem" );
			else
				CL_Disconnect_f();
			return;
		}

		buf->bufferid = bufferid++;

		// Copy in data
		SZ_Clear( &buf->frag_message );
		SZ_Write( &buf->frag_message, &msg->data[pos], send );

		pos += send;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to the end of buffer queue
	if ( !chan->waitlist[FRAG_NORMAL_STREAM] )
	{
		chan->waitlist[FRAG_NORMAL_STREAM] = wait;
	}
	else
	{
		p = chan->waitlist[FRAG_NORMAL_STREAM];
		while ( p->next )
		{
			p = p->next;
		}
		p->next = wait;
	}
}

/*
==============================
Netchan_CreateFragments

==============================
*/
void Netchan_CreateFragments( qboolean server, netchan_t *chan, sizebuf_t *msg )
{
	// Always queue any pending reliable data ahead of the fragmentation buffer
	if ( chan->message.cursize > 0 )
	{
		Netchan_CreateFragments_( server, chan, &chan->message );
		chan->message.cursize = 0;
	}

	Netchan_CreateFragments_( server, chan, msg );
}

/*
==============================
Netchan_CreateFileFragmentsFromBuffer

==============================
*/
void Netchan_CreateFileFragmentsFromBuffer( qboolean server, netchan_t *chan, char *filename, unsigned char *uncompressed_pbuf, int uncompressed_size )
{
	fragbuf_t			*buf;
	int					chunksize;
	int					send;
	int					remaining;
	int					pos;
	int					bufferid = 1;
	int					filesize = 0;
	qboolean			firstfragment = true;
	fragbufwaiting_t	*wait, *p;
	unsigned char		*pbuf;
	unsigned int		size = uncompressed_size;
	qboolean			bCompressed = false;

	if ( !size )
		return;

	pbuf = Mem_Malloc( uncompressed_size );
	if ( !BZ2_bzBuffToBuffCompress( (char *)pbuf, &size, (char *)uncompressed_pbuf, uncompressed_size, 9, 0, 30 ) )
	{
		bCompressed = true;
		Con_DPrintf( "Compressed %s for transmission (%d -> %d)\n", filename, uncompressed_size, size );
	}
	else
	{
		bCompressed = false;
		Mem_Free( pbuf );
		pbuf = uncompressed_pbuf;
		size = uncompressed_size;
	}

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	wait = (fragbufwaiting_t *)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	remaining = size;
	pos = 0;
	while ( remaining > 0 )
	{
		send = min( remaining, chunksize );

		buf = Netchan_AllocFragbuf();
		if ( !buf )
		{
			Con_Printf( "Couldn't allocate fragbuf_t\n" );
			Mem_Free( wait );

			if ( server )
				SV_DropClient( host_client, 0, "Malloc problem" );
			else
				CL_Disconnect_f();
			return;
		}

		buf->bufferid = bufferid++;

		// Copy in data
		SZ_Clear( &buf->frag_message );

		if ( firstfragment )
		{
			firstfragment = false;

			// Write filename
			MSG_WriteString( &buf->frag_message, filename );
			if ( bCompressed )
				MSG_WriteString( &buf->frag_message, "bz2" );
			else
				MSG_WriteString( &buf->frag_message, "uncompressed" );
			MSG_WriteLong( &buf->frag_message, uncompressed_size );

			// Send a bit less on first package
			send -= buf->frag_message.cursize;
		}

		buf->isbuffer = true;
		buf->isfile = true;
		buf->size = send;
		buf->foffset = pos;

		MSG_WriteBuf( &buf->frag_message, send, pbuf + pos );

		pos += send;
		remaining -= send;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to the end of buffer queue
	if ( !chan->waitlist[FRAG_FILE_STREAM] )
	{
		chan->waitlist[FRAG_FILE_STREAM] = wait;
	}
	else
	{
		p = chan->waitlist[FRAG_FILE_STREAM];
		while ( p->next )
		{
			p = p->next;
		}
		p->next = wait;
	}
}

/*
==============================
Netchan_CreateFileFragments

==============================
*/
int Netchan_CreateFileFragments( qboolean server, netchan_t *chan, char *filename )
{
	fragbuf_t			*buf;
	int					chunksize;
	int					send;
	int					remaining;
	int					pos;
	int					bufferid = 1;
	FileHandle_t		hfile = NULL;
	int					filesize = 0;
	int					uncompressed_size = 0;
	qboolean			bCompressed = false;
	qboolean			firstfragment = true;
	char				compressedfilename[MAX_PATH];
	int					compressedFileTime;
	int					fileTime;
	fragbufwaiting_t	*wait, *p;

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	_snprintf( compressedfilename, sizeof( compressedfilename ), "%s.ztmp", filename );
	compressedFileTime = FS_GetFileTime( compressedfilename );
	fileTime = FS_GetFileTime( filename );

	if ( compressedFileTime >= fileTime )
	{
		// if the compressed file is newer, use it
		hfile = FS_Open( compressedfilename, "rb" );
	}

	if ( hfile )
	{
		filesize = FS_Size( hfile );
		FS_Close( hfile );
		bCompressed = true;

		hfile = FS_Open( filename, "rb" );
		if ( !hfile )
		{
			Con_Printf( "Warning:  Unable to open %s for transfer\n", filename );
			return 0;
		}

		uncompressed_size = FS_Size( hfile );
		if ( uncompressed_size > sv_filetransfermaxsize.value )
		{
			FS_Close( hfile );
			Con_Printf( "Warning:  File %s is too big to transfer from host %s\n", filename, NET_AdrToString( chan->remote_address ) );
			return 0;
		}

		FS_Close( hfile );
	}
	else
	{
		hfile = FS_Open( filename, "rb" );
		if ( !hfile )
		{
			Con_Printf( "Warning:  Unable to open %s for transfer\n", filename );
			return 0;
		}

		filesize = FS_Size( hfile );
		if ( filesize > sv_filetransfermaxsize.value )
		{
			FS_Close( hfile );
			Con_Printf( "Warning:  File %s is too big to transfer from host %s\n", filename, NET_AdrToString( chan->remote_address ) );
			return 0;
		}

		uncompressed_size = filesize;

		if ( sv_filetransfercompression.value )
		{
			unsigned char	*uncompressed;
			unsigned char	*compressed;
			unsigned int	compressedSize;

			uncompressed = Mem_Malloc( filesize );
			compressed = Mem_Malloc( filesize );
			compressedSize = filesize;

			FS_Read( uncompressed, filesize, 1, hfile );

			if ( !BZ2_bzBuffToBuffCompress( (char *)compressed, &compressedSize, (char *)uncompressed, uncompressed_size, 9, 0, 30 ) )
			{
				FileHandle_t	destFile;

				destFile = FS_Open( compressedfilename, "wb" );
				if ( destFile )
				{
					Con_DPrintf( "Creating compressed version of file %s (%d -> %d)\n", filename, filesize, compressedSize );
					FS_Write( compressed, compressedSize, 1, destFile );
					FS_Close( destFile );

					bCompressed = true;
					filesize = compressedSize;
				}
			}

			Mem_Free( uncompressed );
			Mem_Free( compressed );
		}

		FS_Close( hfile );
	}

	wait = (fragbufwaiting_t *)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	remaining = filesize;
	pos = 0;
	while ( remaining > 0 )
	{
		send = min( remaining, chunksize );

		buf = Netchan_AllocFragbuf();
		if ( !buf )
		{
			Con_Printf( "Couldn't allocate fragbuf_t\n" );
			Mem_Free( wait );

			if ( server )
				SV_DropClient( host_client, 0, "Malloc problem" );
			else
				CL_Disconnect_f();
			return 0;
		}

		buf->bufferid = bufferid++;

		// Copy in data
		SZ_Clear( &buf->frag_message );

		if ( firstfragment )
		{
			firstfragment = false;

			// Write filename
			MSG_WriteString( &buf->frag_message, filename );
			if ( bCompressed )
				MSG_WriteString( &buf->frag_message, "bz2" );
			else
				MSG_WriteString( &buf->frag_message, "uncompressed" );
			MSG_WriteLong( &buf->frag_message, uncompressed_size );

			// Send a bit less on first package
			send -= buf->frag_message.cursize;
		}

		buf->isfile = true;
		buf->iscompressed = bCompressed;
		buf->size = send;
		buf->foffset = pos;

		Q_strncpy( buf->filename, filename, MAX_PATH - 1 );
		buf->filename[MAX_PATH - 1] = 0;

		pos += send;
		remaining -= send;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to the end of buffer queue
	if ( !chan->waitlist[FRAG_FILE_STREAM] )
	{
		chan->waitlist[FRAG_FILE_STREAM] = wait;
	}
	else
	{
		p = chan->waitlist[FRAG_FILE_STREAM];
		while ( p->next )
		{
			p = p->next;
		}
		p->next = wait;
	}

	return 1;
}

/*
==============================
Netchan_FlushIncoming

==============================
*/
void Netchan_FlushIncoming( netchan_t *chan, int stream )
{
	fragbuf_t	*p, *n;

	SZ_Clear( &net_message );
	msg_readcount = 0;

	p = chan->incomingbufs[stream];
	while ( p )
	{
		n = p->next;
		Mem_Free( p );
		p = n;
	}

	chan->incomingbufs[stream] = NULL;
	chan->incomingready[stream] = false;
}

/*
==============================
Netchan_CopyNormalFragments

==============================
*/
qboolean Netchan_CopyNormalFragments( netchan_t *chan )
{
	fragbuf_t	*p, *n;

	if ( !chan->incomingready[FRAG_NORMAL_STREAM] )
		return false;

	if ( !chan->incomingbufs[FRAG_NORMAL_STREAM] )
	{
		Con_Printf( "Netchan_CopyNormalFragments:  Called with no fragments readied\n" );
		chan->incomingready[FRAG_NORMAL_STREAM] = false;
		return false;
	}

	p = chan->incomingbufs[FRAG_NORMAL_STREAM];

	SZ_Clear( &net_message );
	MSG_BeginReading();

	while ( p )
	{
		n = p->next;

		SZ_Write( &net_message, p->frag_message.data, p->frag_message.cursize );

		Mem_Free( p );
		p = n;
	}

	if ( net_message.data[0] == 'B' && net_message.data[1] == 'Z' && net_message.data[2] == '2' && net_message.data[3] == '\0' )
	{
		char			uncompressed[65536];
		unsigned int	uncompressedSize = 65536;

		BZ2_bzBuffToBuffDecompress( uncompressed, &uncompressedSize, (char *)net_message.data + 4, net_message.cursize - 4, 1, 0 );
		memcpy( net_message.data, uncompressed, uncompressedSize );
		net_message.cursize = uncompressedSize;
	}

	chan->incomingbufs[FRAG_NORMAL_STREAM] = NULL;

	// Reset flag
	chan->incomingready[FRAG_NORMAL_STREAM] = false;

	return true;
}

/*
==============================
Netchan_CopyFileFragments

==============================
*/
qboolean Netchan_CopyFileFragments( netchan_t *chan )
{
	fragbuf_t		*p, *n;
	char			filename[MAX_PATH];
	char			compressor[32];
	qboolean		bCompressed = false;
	int				nsize;
	unsigned int	uncompressedSize;
	FileHandle_t	hfile;
	unsigned char	*buffer;
	int				pos;

	if ( !chan->incomingready[FRAG_FILE_STREAM] )
		return false;

	if ( !chan->incomingbufs[FRAG_FILE_STREAM] )
	{
		Con_Printf( "Netchan_CopyFileFragments:  Called with no fragments readied\n" );
		chan->incomingready[FRAG_FILE_STREAM] = false;
		return false;
	}

	p = chan->incomingbufs[FRAG_FILE_STREAM];

	SZ_Clear( &net_message );
	MSG_BeginReading();
	SZ_Write( &net_message, p->frag_message.data, p->frag_message.cursize );

	Q_strncpy( filename, MSG_ReadString(), sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;

	Q_strncpy( compressor, MSG_ReadString(), sizeof( compressor ) - 1 );
	compressor[sizeof( compressor ) - 1] = 0;
	if ( !_stricmp( compressor, "bz2" ) )
		bCompressed = true;

	uncompressedSize = MSG_ReadLong();

	if ( Q_strlen( filename ) <= 0 )
	{
		Con_Printf( "File fragment received with no filename\nFlushing input queue\n" );

		// Clear out bufs
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return false;
	}

	if ( Q_strstr( filename, ".." ) )
	{
		Con_Printf( "File fragment received with relative path, ignoring\n" );

		// Clear out bufs
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return false;
	}

	Q_strncpy( chan->incomingfilename, filename, MAX_PATH - 1 );
	chan->incomingfilename[MAX_PATH - 1] = 0;

	if ( filename[0] != '!' )
	{
		hfile = FS_Open( filename, "rb" );
		if ( hfile )
		{
			Con_Printf( "Can't download %s, already exists\n", filename );

			// Clear out bufs
			Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );

			FS_Close( hfile );
			return true;
		}

		// Create file from buffers
		COM_CreatePath( filename );
	}

	nsize = 0;
	while ( p )
	{
		nsize += p->frag_message.cursize;

		// Skip outgoing message buffer
		if ( p == chan->incomingbufs[FRAG_FILE_STREAM] )
			nsize -= msg_readcount;

		p = p->next;
	}

	buffer = Mem_ZeroMalloc( nsize + 1 );
	if ( !buffer )
	{
		Con_Printf( "Buffer allocation failed on %i bytes\n", nsize + 1 );

		// Clear out bufs
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return false;
	}

	p = chan->incomingbufs[FRAG_FILE_STREAM];
	pos = 0;
	while ( p )
	{
		n = p->next;

		if ( p == chan->incomingbufs[FRAG_FILE_STREAM] )
		{
			// Copy it in
			p->frag_message.cursize -= msg_readcount;
			Q_memcpy( &buffer[pos], &p->frag_message.data[msg_readcount], p->frag_message.cursize );
		}
		else
		{
			Q_memcpy( &buffer[pos], p->frag_message.data, p->frag_message.cursize );
		}

		pos += p->frag_message.cursize;

		Mem_Free( p );
		p = n;
	}

	if ( bCompressed )
	{
		unsigned char	*uncompressedBuffer;

		uncompressedBuffer = Mem_Malloc( uncompressedSize );

		Con_DPrintf( "Decompressing file %s (%d -> %d)\n", filename, nsize, uncompressedSize );
		BZ2_bzBuffToBuffDecompress( (char *)uncompressedBuffer, &uncompressedSize, (char *)buffer, nsize, 1, 0 );
		Mem_Free( buffer );

		buffer = uncompressedBuffer;
		nsize = uncompressedSize;
		pos = uncompressedSize;
	}

	if ( filename[0] != '!' )
	{
		COM_WriteFile( filename, buffer, pos );
		Mem_Free( buffer );
	}
	else
	{
		// Customization files are kept in memory for the hpak
		if ( chan->tempbuffer )
		{
			Con_DPrintf( "Netchan_CopyFragments:  Freeing holdover tempbuffer\n" );
			Mem_Free( chan->tempbuffer );
		}
		chan->tempbuffer = buffer;
		chan->tempbuffersize = pos;
	}

	// clear remnants
	SZ_Clear( &net_message );
	msg_readcount = 0;

	chan->incomingbufs[FRAG_FILE_STREAM] = NULL;

	// Reset flag
	chan->incomingready[FRAG_FILE_STREAM] = false;

	return true;
}

/*
==============================
Netchan_IsSending

==============================
*/
qboolean Netchan_IsSending( netchan_t *chan )
{
	int		i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->fragbufs[i] )
			return true;
	}

	return false;
}

/*
==============================
Netchan_IsReceiving

==============================
*/
qboolean Netchan_IsReceiving( netchan_t *chan )
{
	int		i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->incomingbufs[i] )
			return true;
	}

	return false;
}

/*
==============================
Netchan_IncomingReady

==============================
*/
qboolean Netchan_IncomingReady( netchan_t *chan )
{
	int		i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->incomingready[i] )
			return true;
	}

	return false;
}

/*
==============================
Netchan_UpdateProgress

==============================
*/
void Netchan_UpdateProgress( netchan_t *chan )
{
	fragbuf_t	*p;
	int			c = 0;
	int			total = 0;
	int			i;
	float		bestpercent = 0.0;

	scr_downloading.value = -1.0;
	gDownloadFile[0] = 0;

	if ( net_drawslider.value != 1.0 )
	{
		// Do show slider for file downloads.
		if ( !chan->incomingbufs[FRAG_FILE_STREAM] )
			return;
	}

	for ( i = MAX_STREAMS - 1; i >= 0; i-- )
	{
		// If receiving data stream
		if ( chan->incomingbufs[i] )
		{
			p = chan->incomingbufs[i];

			total = FRAG_GETCOUNT( p->bufferid );

			while ( p )
			{
				c++;
				p = p->next;
			}

			if ( total )
			{
				float	percent;

				percent = 100.0 * (float)c / (float)total;

				if ( percent > bestpercent )
				{
					bestpercent = percent;
				}
			}

			p = chan->incomingbufs[i];

			if ( i == FRAG_FILE_STREAM )
			{
				char	sz[1024];
				char	*in, *out;
				int		len = 0;

				in = (char *)p->frag_message.data;
				out = sz;

				while ( *in )
				{
					*out++ = *in++;
					len++;
					if ( len > 128 )
						break;
				}
				*out = '\0';

				if ( Q_strlen( sz ) > 0 && ( *sz != '!' || !Q_strncmp( sz, "!ModuleC.dll", strlen( "ModuleC.dll" ) ) ) )
				{
					Q_strncpy( gDownloadFile, sz, 255 );
					gDownloadFile[255] = 0;
				}
			}
		}
		else if ( chan->fragbufs[i] )	// Sending data
		{
			if ( chan->fragbufcount[i] )
			{
				float	percent;

				percent = (float)chan->fragbufs[i]->bufferid * 100.0 / (float)chan->fragbufcount[i];

				if ( percent > bestpercent )
				{
					bestpercent = percent;
				}
			}
		}
	}

	scr_downloading.value = bestpercent;
}

/*
==============================
Netchan_Init

==============================
*/
void Netchan_Init( void )
{
	Cvar_RegisterVariable( &net_log );
	Cvar_RegisterVariable( &net_showpackets );
	Cvar_RegisterVariable( &net_showdrop );
	Cvar_RegisterVariable( &net_chokeloopback );
	Cvar_RegisterVariable( &net_drawslider );
	Cvar_RegisterVariable( &sv_filetransfercompression );
	Cvar_RegisterVariable( &sv_filetransfermaxsize );
}

/*
==============================
Netchan_CompressPacket

Packet compression is not used in this build
==============================
*/
qboolean Netchan_CompressPacket( sizebuf_t *chan )
{
	return false;
}

/*
==============================
Netchan_DecompressPacket

==============================
*/
qboolean Netchan_DecompressPacket( sizebuf_t *chan )
{
	return false;
}
