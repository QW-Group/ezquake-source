/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


*/

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include <time.h>
#include "quakedef.h"
#include "server.h"
#endif

#define	PACKET_HEADER 8

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16  qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher than the last reliable sequence, but without the correct even/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

If the sequence number is -1, the packet should be handled without a netcon.

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are always placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.

*/

cvar_t  showpackets    = {"showpackets", "0"};
cvar_t  showdrop       = {"showdrop", "0"};
#ifndef SERVERONLY
cvar_t  qport          = {"qport", "0"};
#ifndef CLIENTONLY
cvar_t  sv_showpackets = {"sv_showpackets", "0"};
cvar_t  sv_showdrop    = {"sv_showdrop", "0"};
#endif
#endif

#ifdef SERVERONLY
#define CHAN_IDENTIFIER(sockType) " [s]"
#define ShowPacket(src,packet_type) (showpackets.integer == 1 || showpackets.integer == packet_type)
#define ShowDrop(src) (showdrop.integer)
#elif CLIENTONLY
#define CHAN_IDENTIFIER(sockType) " [c]"
#define ShowPacket(src,packet_type) (showpackets.integer == 1 || showpackets.integer == packet_type)
#define ShowDrop(src) (showdrop.integer)
#else
#define CHAN_IDENTIFIER(sockType) (sockType == NS_SERVER ? " [s]" : " [c]")

// Use the sv_ options for internal server
static qbool ShowPacket(netsrc_t src, int packet_type)
{
	if (src == NS_SERVER) {
		return sv_showpackets.integer == 1 || sv_showpackets.integer == packet_type;
	}
	else {
		return showpackets.integer == 1 || showpackets.integer == packet_type;
	}
}

static qbool ShowDrop(netsrc_t src)
{
	if (src == NS_SERVER) {
		return sv_showdrop.integer;
	}
	else {
		return showdrop.integer;
	}
}
#endif

#define PACKET_SENDING 2
#define PACKET_RECEIVING 3

/*
===============
Netchan_Init

===============
*/
void Netchan_Init(void)
{
#ifndef SERVERONLY
	int		port = 0xffff;

	srand((unsigned)time(NULL));
	port &= rand();
#endif // SERVERONLY

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);

	Cvar_Register(&showpackets);
	Cvar_Register(&showdrop);

#ifndef SERVERONLY
	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register(&qport);
	Cvar_SetValue(&qport, port);

#ifndef CLIENTONLY
	Cvar_Register(&sv_showpackets);
	Cvar_Register(&sv_showdrop);
#endif
#endif

	Cvar_ResetCurrentGroup();
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (netsrc_t sock, netadr_t adr, int length, byte *data)
{
	sizebuf_t send;
	byte send_buf[MAX_MSGLEN + PACKET_HEADER];

	// write the packet header
	SZ_Init (&send, send_buf, sizeof(send_buf));

	MSG_WriteLong (&send, -1);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

	// send the datagram
#ifndef SERVERONLY
	//zoid, no input in demo playback mode
	if (!cls.demoplayback)
#endif
		NET_SendPacket (sock, send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (netsrc_t sock, netadr_t adr, char *format, ...)
{
	va_list argptr;
	char string[8192];

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	Netchan_OutOfBand (sock, adr, strlen(string), (byte *)string);
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport, int mtu)
{
	if (mtu < 1) {
		mtu = (int)sizeof(chan->message_buf); // OLD way, backward compatibility.
	}
	else {
		mtu -= PACKET_HEADER; // new way.
	}

	memset (chan, 0, sizeof (*chan));

	chan->sock = sock;
	chan->remote_address = adr;
	chan->qport = qport;

	chan->last_received = curtime;
	chan->rate = 1.0/2500;

	SZ_InitEx (&chan->message, chan->message_buf, bound(min(MIN_MTU, (int)sizeof(chan->message_buf)), mtu, (int)sizeof(chan->message_buf)), true);
}

/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
#ifndef SERVERONLY
#define MAX_BACKUP  200
#else
#define	MAX_BACKUP	400
#endif
qbool Netchan_CanPacket (netchan_t *chan)
{
#ifndef SERVERONLY
	if (chan->remote_address.type == NA_LOOPBACK)
		return true; // unlimited bandwidth for local client
#endif

	return (chan->cleartime < curtime + MAX_BACKUP * chan->rate) ? true : false;
}

/*
===============
Netchan_CanReliable

Returns true if the bandwidth choke isn't 
================
*/
qbool Netchan_CanReliable (netchan_t *chan)
{
	if (chan->reliable_length)
		return false; // waiting for ack
	return Netchan_CanPacket (chan);
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit (netchan_t *chan, int length, byte *data)
{
	sizebuf_t send;
	byte send_buf[MAX_MSGLEN + PACKET_HEADER];
	qbool send_reliable;
	unsigned w1, w2;
	int i;
	static double last_error_time = 0;

	// check for message overflow
	if (chan->message.overflowed) {
		chan->fatal_error = true; //FIXME: THIS DOES NOTHING
		if (last_error_time - curtime > 5 || developer.value) {
			Con_Printf ("%s:Outgoing message overflow\n", NET_AdrToString (chan->remote_address));
			last_error_time = curtime;
		}
		return;
	}

	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence && chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

	// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize) {
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}

	// write the packet header
	SZ_Init (&send, send_buf, min(chan->message.maxsize + PACKET_HEADER, (int)sizeof(send_buf)));

	w1 = chan->outgoing_sequence | (send_reliable<<31);
	w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

#ifndef SERVERONLY
	// send the qport if we are a client
	if (chan->sock == NS_CLIENT)
		MSG_WriteShort (&send, chan->qport);
#endif

	// copy the reliable message to the packet first
	if (send_reliable) {
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}

	// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length) {
		SZ_Write(&send, data, length);
	}

	// send the datagram
	i = chan->outgoing_sequence & (MAX_LATENT-1);
	chan->outgoing_size[i] = send.cursize;
	chan->outgoing_time[i] = curtime;

#ifndef SERVERONLY
	//zoid, no input in demo playback mode
	if (!cls.demoplayback)
#endif
	{
		for (i = 0; i <= chan->dupe; ++i) {
			NET_SendPacket(chan->sock, send.cursize, send.data, chan->remote_address);
		}
	}

	if (chan->cleartime < curtime) {
		chan->cleartime = curtime + send.cursize * i * chan->rate;
	}
	else {
		chan->cleartime += send.cursize * i * chan->rate;
	}

#ifndef CLIENTONLY
	if (chan->sock == NS_SERVER && sv.paused)
		chan->cleartime = curtime;
#endif

	if (ShowPacket(chan->sock, PACKET_SENDING)) {
#ifndef SERVERONLY
		Print_flags[Print_current] |= PR_TR_SKIP;
#endif
		Con_Printf ("%.1f --> s=%i(%i) a=%i(%i) %i%s\n"
		            , cls.demopackettime * 1000, chan->outgoing_sequence
		            , send_reliable
		            , chan->incoming_sequence
		            , chan->incoming_reliable_sequence
		            , send.cursize, CHAN_IDENTIFIER(chan->sock)
		);
	}

}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qbool Netchan_Process (netchan_t *chan)
{
	unsigned sequence, sequence_ack;
	unsigned reliable_ack, reliable_message;

#ifdef SERVERONLY
	if (!NET_CompareAdr (net_from, chan->remote_address))
		return false;
#endif

	// get sequence numbers
	MSG_BeginReading ();
	sequence = MSG_ReadLong ();
	sequence_ack = MSG_ReadLong ();

	// read the qport if we are a server
	if (chan->sock == NS_SERVER)
		MSG_ReadShort ();

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1 << 31);
	sequence_ack &= ~(1 << 31);

	if (ShowPacket(chan->sock, PACKET_RECEIVING)) {
#ifndef SERVERONLY
		Print_flags[Print_current] |= PR_TR_SKIP;
#endif
		Con_Printf ("%.1f <-- s=%i(%i) a=%i(%i) %i%s\n"
		            , cls.demopackettime * 1000, sequence
		            , reliable_message
		            , sequence_ack
		            , reliable_ack
		            , net_message.cursize, CHAN_IDENTIFIER(chan->sock)
		);
	}

	// discard stale or duplicated packets
	if (sequence <= (unsigned)chan->incoming_sequence) {
		if (ShowDrop(chan->sock)) {
#ifndef SERVERONLY
			Print_flags[Print_current] |= PR_TR_SKIP;
#endif
			Con_Printf ("%s:Out of order packet %i at %i\n"
			            , NET_AdrToString (chan->remote_address)
			            , sequence
			            , chan->incoming_sequence);
		}
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->dropped = sequence - (chan->incoming_sequence+1);
	if (chan->dropped > 0) {
		chan->drop_count += 1;

		if (ShowDrop(chan->sock)) {
#ifndef SERVERONLY
			Print_flags[Print_current] |= PR_TR_SKIP;
#endif
			Con_Printf ("%s:Dropped %i packets at %i\n"
			            , NET_AdrToString (chan->remote_address)
			            , chan->dropped
			            , sequence);
		}
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if (reliable_ack == (unsigned)chan->reliable_sequence) {
		chan->reliable_length = 0;	// it has been received
	}

	// if this message contains a reliable message, bump incoming_reliable_sequence
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message) {
		chan->incoming_reliable_sequence ^= 1;
	}

	// the message can now be read from the current message pointer
	// update statistics counters
	chan->frame_latency = chan->frame_latency * OLD_AVG +
		(chan->outgoing_sequence - sequence_ack) * (1.0 - OLD_AVG);
	chan->frame_rate = chan->frame_rate * OLD_AVG +
		(curtime - chan->last_received) * (1.0 - OLD_AVG);
	chan->good_count += 1;

	chan->last_received = curtime;

	return true;
}
