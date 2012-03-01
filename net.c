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
// net.c

#ifdef SERVERONLY
#include "qwsvdef.h"
#else
#include "quakedef.h"
#include "server.h"
#endif

#ifdef _WIN32
WSADATA		winsockdata;
#endif

#ifndef SERVERONLY
netadr_t	net_local_cl_ipadr;
#endif

#ifndef CLIENTONLY
netadr_t	net_local_sv_ipadr;
netadr_t	net_local_sv_tcpipadr;

cvar_t		sv_local_addr = {"sv_local_addr", "", CVAR_ROM};
#endif

netadr_t	net_from;
sizebuf_t	net_message;

static byte	net_message_buffer[MSG_BUF_SIZE];

// forward definition.
qbool NET_GetPacketEx (netsrc_t netsrc, qbool delay);
void NET_SendPacketEx (netsrc_t netsrc, int length, void *data, netadr_t to, qbool delay);

//=============================================================================
//
// LOOPBACK defs.
//

#define MAX_LOOPBACK 4 // must be a power of two

typedef struct
{
	byte	data[MSG_BUF_SIZE];
	int		datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t	msgs[MAX_LOOPBACK];
	unsigned int	get, send;
} loopback_t;

loopback_t	loopbacks[2];

//=============================================================================
//
// Delayed packets, CLIENT ONLY.
//

#ifndef SERVERONLY

static cl_delayed_packet_t cl_delayed_packets_input[CL_MAX_DELAYED_PACKETS]; // from server to our client.
static cl_delayed_packet_t cl_delayed_packets_output[CL_MAX_DELAYED_PACKETS]; // from our client to server.

//
// Attempt to read packet from network and if succeed we put that packet in array/queue (so they are delayed).
// Packet will be unqued/extracted and used/parsed later, that depends of cl_delay_packet variable.
//
void CL_QueInputPacket(void)
{
	int i;

	// read packets.
	while (NET_GetPacketEx(NS_CLIENT, false))
	{
		// queue one packet.
		for (i = 0; i < CL_MAX_DELAYED_PACKETS; i++)
		{
			if (cl_delayed_packets_input[i].time)
				continue; // busy slot

			// we found unused slot, copy packet, get it later, later depends of cl_delay_packet
			memmove(cl_delayed_packets_input[i].data, net_message.data, net_message.cursize);
			cl_delayed_packets_input[i].length = net_message.cursize;
			cl_delayed_packets_input[i].addr = net_from;
			cl_delayed_packets_input[i].time = Sys_DoubleTime() + 0.001 * bound(0, 0.5 * cl_delay_packet.value, CL_MAX_PACKET_DELAY);
			break; // queued OK.
		}

		if (i >= CL_MAX_DELAYED_PACKETS)
		{
			Con_DPrintf("CL_QueInputPacket: cl_delayed_packets_input overflowed\n");
			break;
		}
	}

	return;
}

//
// Check if delayed output packets must be sent right now.
//
void CL_UnqueOutputPacket(qbool sendall)
{
	int i;
	double time = Sys_DoubleTime();

	for (i = 0; i < CL_MAX_DELAYED_PACKETS; i++)
	{
		if (!cl_delayed_packets_output[i].time)
			continue; // unused slot
		if (cl_delayed_packets_output[i].time > time && !sendall)
			continue; // we are not yet ready for send

		// ok, send it
		NET_SendPacketEx(NS_CLIENT, cl_delayed_packets_output[i].length, 
			cl_delayed_packets_output[i].data, cl_delayed_packets_output[i].addr, false);

		cl_delayed_packets_output[i].time = 0; // mark as unused slot

// perhaps there other packets should be sent
//		return;
	}
}

//
// Get packet from input queue.
//
qbool NET_GetDelayedPacket (netsrc_t netsrc, netadr_t *from, sizebuf_t *message)
{
	int i;
	double time = Sys_DoubleTime();

	for (i = 0; i < CL_MAX_DELAYED_PACKETS; i++)
	{
		if (!cl_delayed_packets_input[i].time)
			continue; // unused slot
		if (cl_delayed_packets_input[i].time > time)
			continue; // we are not yet ready to get this

		// ok, we got something
		SZ_Clear(message);
		SZ_Write(message, cl_delayed_packets_input[i].data, cl_delayed_packets_input[i].length);
		*from = cl_delayed_packets_input[i].addr;

		cl_delayed_packets_input[i].time = 0; // mark as free slot

		return true;
	}

	return false;
}

//
// Put packet in output queue.
//
void NET_SendDelayedPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	int i;

	for (i = 0; i < CL_MAX_DELAYED_PACKETS; i++)
	{
		if (cl_delayed_packets_output[i].time)
			continue; // busy slot
		// we found unused slot, copy packet, send it later, later depends of cl_delay_packet
		memmove(cl_delayed_packets_output[i].data, data, length);
		cl_delayed_packets_output[i].length = length;
		cl_delayed_packets_output[i].addr = to;
		cl_delayed_packets_output[i].time = Sys_DoubleTime() + 0.001 * bound(0, 0.5 * cl_delay_packet.value, CL_MAX_PACKET_DELAY);

		return;
	}

	Con_DPrintf("NET_SendPacketEx: cl_delayed_packets_output overflowed\n");
	return;
}

#endif

//=============================================================================
//
// Geters.
//

int NET_UDPSVPort (void)
{
	return ntohs(net_local_sv_ipadr.port);
}

int NET_GetSocket(netsrc_t netsrc, qbool tcp)
{
	if (netsrc == NS_SERVER)
	{
#ifdef CLIENTONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return INVALID_SOCKET;
#else
		return tcp ? svs.sockettcp : svs.socketip;
#endif
	}
	else
	{
#ifdef SERVERONLY
		Sys_Error("NET_GetPacket: Bad netsrc");
		return INVALID_SOCKET;
#else
		return tcp ? cls.sockettcp : cls.socketip;
#endif
	}
}

//=============================================================================
//
// Converters.
//

void NetadrToSockadr (const netadr_t *a, struct sockaddr_storage *s)
{
	memset (s, 0, sizeof(struct sockaddr_in));
	((struct sockaddr_in*)s)->sin_family = AF_INET;

	((struct sockaddr_in*)s)->sin_addr.s_addr = *(int *)&a->ip;
	((struct sockaddr_in*)s)->sin_port = a->port;
}

void SockadrToNetadr (const struct sockaddr_storage *s, netadr_t *a)
{
	a->type = NA_IP;
	*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
	a->port = ((struct sockaddr_in *)s)->sin_port;
	return;
}

//=============================================================================
//
// Comparators.
//

qbool NET_CompareBaseAdr (const netadr_t a, const netadr_t b)
{
#ifndef SERVERONLY
	if (a.type == NA_LOOPBACK && b.type == NA_LOOPBACK)
		return true;
#endif

	// FIXME: Should we check a.type == b.type here ???

	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
		return true;
	return false;
}

qbool NET_CompareAdr (const netadr_t a, const netadr_t b)
{
#ifndef SERVERONLY
	if (a.type == NA_LOOPBACK && b.type == NA_LOOPBACK)
		return true;
#endif

	// FIXME: Should we check a.type == b.type here ???

	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

//=============================================================================
//
// Printors.
//

char *NET_AdrToString (const netadr_t a)
{
	static char s[MAX_STRINGS][32]; // 22 should be OK too
	static int idx = 0;

	idx %= MAX_STRINGS;

#ifndef SERVERONLY
	if (a.type == NA_LOOPBACK)
	{
		snprintf (s[idx], sizeof(s[0]), "loopback");
	}
	else
#endif
	{
		snprintf (s[idx], sizeof(s[0]), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs (a.port));
	}

	return s[idx++];
}

char *NET_BaseAdrToString (const netadr_t a)
{
	static char s[MAX_STRINGS][32]; // 22 should be OK too
	static int idx = 0;

	idx %= MAX_STRINGS;

#ifndef SERVERONLY
	if (a.type == NA_LOOPBACK)
	{
		snprintf (s[idx], sizeof(s[0]), "loopback");
	}
	else
#endif
	{
		snprintf (s[idx], sizeof(s[0]), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	}

	return s[idx++];
}

// !!! Not used outside of the net.c !!!
static qbool NET_StringToSockaddr (const char *s, struct sockaddr_storage *sadr)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	if (!(*s))
		return false;

	memset (sadr, 0, sizeof(*sadr));

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	// can't resolve IP by hostname if hostname was truncated
	if (strlcpy (copy, s, sizeof(copy)) >= sizeof(copy))
		return false;

	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':') {
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
		}
	}

	//this is the wrong way to test. a server name may start with a number.
	if (copy[0] >= '0' && copy[0] <= '9')
	{
		//this is the wrong way to test. a server name may start with a number.
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	} else
	{
		if (!(h = gethostbyname(copy)))
			return false;

		if (h->h_addrtype != AF_INET)
			return false;

		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

qbool NET_StringToAdr (const char *s, netadr_t *a)
{
	struct sockaddr_storage sadr;

#ifndef SERVERONLY
	if (!strcmp(s, "local"))
	{
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}
#endif

	if (!NET_StringToSockaddr (s, &sadr))
		return false;

	SockadrToNetadr (&sadr, a);

	return true;
}

/*
=============================================================================
LOOPBACK BUFFERS FOR LOCAL PLAYER
=============================================================================
*/

qbool NET_GetLoopPacket (netsrc_t netsrc, netadr_t *from, sizebuf_t *message)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[netsrc];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	if (message->maxsize < loop->msgs[i].datalen)
		Sys_Error("NET_SendLoopPacket: Loopback buffer was too big");

	memcpy (message->data, loop->msgs[i].data, loop->msgs[i].datalen);
	message->cursize = loop->msgs[i].datalen;
	memset (from, 0, sizeof(*from));
	from->type = NA_LOOPBACK;
	return true;
}

void NET_SendLoopPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	int i;
	loopback_t *loop;

	loop = &loopbacks[netsrc ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	if (length > (int) sizeof(loop->msgs[i].data))
		Sys_Error ("NET_SendLoopPacket: length > %d", (int) sizeof(loop->msgs[i].data));

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

void NET_ClearLoopback (void)
{
	loopbacks[0].send = loopbacks[0].get = 0;
	loopbacks[1].send = loopbacks[1].get = 0;
}

//=============================================================================
//
// SV TCP connection.
//

// allocate, may link it in, if requested
svtcpstream_t *sv_tcp_connection_new(int sock, netadr_t from, char *buf, int buf_len, qbool link)
{
	svtcpstream_t *st = NULL;

	st = Q_malloc(sizeof(svtcpstream_t));
	st->waitingforprotocolconfirmation = true;
	st->socketnum = sock;
	st->remoteaddr = from;
	if (buf_len > 0 && buf_len < sizeof(st->inbuffer))
	{
		memmove(st->inbuffer, buf, buf_len);
		st->inlen = buf_len;
	}
	else
		st->drop = true; // yeah, funny

	// link it in if requested
	if (link)
	{
		st->next = svs.tcpstreams;
		svs.tcpstreams = st;
	}

	return st;
}

// free data, may unlink it out if requested
static void sv_tcp_connection_free(svtcpstream_t *drop, qbool unlink)
{
	if (!drop)
		return; // someone kidding us

	// unlink if requested
	if (unlink)
	{
		if (svs.tcpstreams == drop)
		{
			svs.tcpstreams = svs.tcpstreams->next;
		}
		else
		{
			svtcpstream_t *st = NULL;

			for (st = svs.tcpstreams; st; st = st->next)
			{
				if (st->next == drop)
				{
					st->next = st->next->next;
					break;
				}
			}
		}
	}

	// well, think socket may be zero, but most of the time zero is stdin fd, so better not close it
	if (drop->socketnum && drop->socketnum != INVALID_SOCKET)
		closesocket(drop->socketnum);

	Q_free(drop);
}

int sv_tcp_connection_count(void)
{
	svtcpstream_t *st = NULL;
	int cnt = 0;

	for (st = svs.tcpstreams; st; st = st->next)
		cnt++;

	return cnt;
}

//=============================================================================

qbool NET_GetUDPPacket (netsrc_t netsrc, netadr_t *from_adr, sizebuf_t *message)
{
	int ret, err;
	struct sockaddr_storage from = {0};
	socklen_t fromlen;
	int socket = NET_GetSocket(netsrc, false);

	if (socket == INVALID_SOCKET)
		return false;

	fromlen = sizeof(from);
	ret = recvfrom (socket, (char *)message->data, message->maxsize, 0, (struct sockaddr *)&from, &fromlen);
	SockadrToNetadr (&from, from_adr);

	if (ret == -1)
	{
		err = qerrno;

		if (err == EWOULDBLOCK)
			return false; // common error, does not spam in logs.

		if (err == EMSGSIZE)
		{
			Con_DPrintf ("Warning: Oversize packet from %s\n", NET_AdrToString (*from_adr));
			return false;
		}

		if (err == ECONNABORTED || err == ECONNRESET)
		{
			Con_DPrintf ("Connection lost or aborted\n");
			return false;
		}

		Con_Printf ("NET_GetPacket: recvfrom: (%i): %s\n", err, strerror(err));
		return false;
	}

	if (ret >= message->maxsize)
	{
		Con_Printf ("Oversize packet from %s\n", NET_AdrToString (*from_adr));
		return false;
	}

	message->cursize = ret;

	return ret;
}

#ifndef SERVERONLY
qbool NET_GetTCPPacket_CL (netsrc_t netsrc, netadr_t *from, sizebuf_t *message)
{
	int ret, err;

	if (netsrc != NS_CLIENT || cls.sockettcp == INVALID_SOCKET)
		return false;

	ret = recv(cls.sockettcp, (char *) cls.tcpinbuffer + cls.tcpinlen, sizeof(cls.tcpinbuffer) - cls.tcpinlen, 0);

	// FIXME: should we check for ret == 0  for disconnect ???

	if (ret == -1)
	{
		err = qerrno;

		if (err == EWOULDBLOCK)
		{
			ret = 0; // hint for code below that it was not cricial error.
		}
		else if (err == ECONNABORTED || err == ECONNRESET)
		{
			Con_Printf ("Connection lost or aborted\n"); //server died/connection lost.
		}
		else
		{
			Con_Printf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
		}

		if (ret)
		{
			// error detected, close socket then.
			closesocket(cls.sockettcp);
			cls.sockettcp = INVALID_SOCKET;
			return false;
		}
	}

	cls.tcpinlen += ret;

	if (cls.tcpinlen < 2)
		return false;

	message->cursize = BigShort(*(short*)cls.tcpinbuffer);
	if (message->cursize >= message->maxsize)
	{
		closesocket(cls.sockettcp);
		cls.sockettcp = INVALID_SOCKET;
		Con_Printf ("Warning: Oversize packet from %s\n", NET_AdrToString (cls.sockettcpdest));
		return false;
	}

	if (message->cursize + 2 > cls.tcpinlen)
	{
		//not enough buffered to read a packet out of it.
		return false;
	}

	memcpy(message->data, cls.tcpinbuffer + 2, message->cursize);
	memmove(cls.tcpinbuffer, cls.tcpinbuffer + message->cursize + 2, cls.tcpinlen - (message->cursize + 2));
	cls.tcpinlen -= message->cursize + 2;

	*from = cls.sockettcpdest;

	return true;
}
#endif

#ifndef CLIENTONLY
qbool NET_GetTCPPacket_SV (netsrc_t netsrc, netadr_t *from, sizebuf_t *message)
{
	int ret;
	float timeval = Sys_DoubleTime();
	svtcpstream_t *st = NULL, *next = NULL;

	if (netsrc != NS_SERVER)
		return false;

	for (st = svs.tcpstreams; st; st = next)
	{
		next = st->next;

		*from = st->remoteaddr;

		if (st->socketnum == INVALID_SOCKET || st->drop)
		{
			sv_tcp_connection_free(st, true); // free and unlink
			continue;
		}

		//due to the above checks about invalid sockets, the socket is always open for st below.

		// check for client timeout
		if (st->timeouttime < timeval)
		{
			st->drop = true;
			continue;
		}

		ret = recv(st->socketnum, st->inbuffer+st->inlen, sizeof(st->inbuffer)-st->inlen, 0);
		if (ret == 0)
		{
			// connection closed
			st->drop = true;
			continue;
		}
		else if (ret == -1)
		{
			int err = qerrno;

			if (err == EWOULDBLOCK)
			{
				ret = 0; // it's OK
			}
			else
			{
				if (err == ECONNABORTED || err == ECONNRESET)
				{
					Con_DPrintf ("Connection lost or aborted\n"); //server died/connection lost.
				}
				else
				{
					Con_DPrintf ("NET_GetPacket: Error (%i): %s\n", err, strerror(err));
				}

				st->drop = true;
				continue;
			}
		}
		else
		{
			// update timeout
			st->timeouttime = Sys_DoubleTime() + 10;
		}

		st->inlen += ret;

		if (st->waitingforprotocolconfirmation)
		{
			// not enough data
			if (st->inlen < 6)
				continue;

			if (strncmp(st->inbuffer, "qizmo\n", 6))
			{
				Con_Printf ("Unknown TCP client\n");
				st->drop = true;
				continue;
			}

			// remove leading 6 bytes
			memmove(st->inbuffer, st->inbuffer+6, st->inlen - (6));
			st->inlen -= 6;
			// confirmed
			st->waitingforprotocolconfirmation = false;
		}

		// need two bytes for packet len
		if (st->inlen < 2)
			continue;

		message->cursize = BigShort(*(short*)st->inbuffer);
		if (message->cursize < 0)
		{
			message->cursize = 0;
			Con_Printf ("Warning: malformed message from %s\n", NET_AdrToString (*from));
			st->drop = true;
			continue;
		}

		if (message->cursize >= message->maxsize)
		{
			Con_Printf ("Warning: Oversize packet from %s\n", NET_AdrToString (*from));
			st->drop = true;
			continue;
		}

		if (message->cursize + 2 > st->inlen)
		{
			//not enough buffered to read a packet out of it.
			continue;
		}

		memcpy(message->data, st->inbuffer + 2, message->cursize);
		memmove(st->inbuffer, st->inbuffer + message->cursize + 2, st->inlen - (message->cursize + 2));
		st->inlen -= message->cursize + 2;

		return true; // we got packet!
	}

	return false; // no packet received.
}
#endif

qbool NET_GetPacketEx (netsrc_t netsrc, qbool delay)
{
#ifndef SERVERONLY
	if (delay)
		return NET_GetDelayedPacket(netsrc, &net_from, &net_message);
#endif

#ifndef SERVERONLY
	if (NET_GetLoopPacket(netsrc, &net_from, &net_message))
		return true;
#endif

	if (NET_GetUDPPacket(netsrc, &net_from, &net_message))
		return true;

// TCPCONNECT -->
#ifndef SERVERONLY
	if (netsrc == NS_CLIENT && cls.sockettcp != INVALID_SOCKET && NET_GetTCPPacket_CL(netsrc, &net_from, &net_message))
		return true;
#endif

#ifndef CLIENTONLY
	if (netsrc == NS_SERVER && svs.tcpstreams && NET_GetTCPPacket_SV(netsrc, &net_from, &net_message))
		return true;
#endif
// <--TCPCONNECT

	return false;
}

qbool NET_GetPacket (netsrc_t netsrc)
{
#ifdef SERVERONLY
	qbool delay = false;
#else
	qbool delay = (netsrc == NS_CLIENT && cl_delay_packet.integer);
#endif

	return NET_GetPacketEx (netsrc, delay);
}

//=============================================================================

#ifndef SERVERONLY
qbool NET_SendTCPPacket_CL (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	unsigned short slen;

	if (netsrc != NS_CLIENT || cls.sockettcp == INVALID_SOCKET)
		return false;

	if (!NET_CompareAdr(to, cls.sockettcpdest))
		return false;

	// this goes to the server so send it via TCP.
	slen = BigShort((unsigned short)length);
	// FIXME: CHECK send() result, we use NON BLOCKIN MODE, FFS!
	send(cls.sockettcp, (char*)&slen, sizeof(slen), 0);
	send(cls.sockettcp, data, length, 0);

	return true;
}
#endif

#ifndef CLIENTONLY
qbool NET_SendTCPPacket_SV (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	svtcpstream_t *st;

	if (netsrc != NS_SERVER)
		return false;

	for (st = svs.tcpstreams; st; st = st->next)
	{
		if (st->socketnum == INVALID_SOCKET)
			continue;

		if (NET_CompareAdr(to, st->remoteaddr))
		{
			int sent;
			unsigned short slen = BigShort((unsigned short)length);

			if (st->outlen + length + sizeof(slen) >= sizeof(st->outbuffer))
			{
				// not enough space, we overflowed
				break; // well, quake should resist to some packet lost.. so we just drop that packet.
			}

			// put data in buffer
			memmove(st->outbuffer + st->outlen, (char*)&slen, sizeof(slen));
			st->outlen += sizeof(slen);
			memmove(st->outbuffer + st->outlen, data, length);
			st->outlen += length;

			sent = send(st->socketnum, st->outbuffer, st->outlen, 0);

			if (sent == 0)
			{
				// think it's OK
			}
			else if (sent > 0) //we put some data through
			{ //move up the buffer
				st->outlen -= sent;
				memmove(st->outbuffer, st->outbuffer + sent, st->outlen);
			}
			else
			{ //error of some kind. would block or something
				if (qerrno != EWOULDBLOCK && qerrno != EAGAIN)
				{
					st->drop = true; // something cricial, drop than
				}
			}

			break;
		}
	}

	// 'st' will be not zero, if we found 'to' in 'svs.tcpstreams'.
	// That does not mean we actualy send packet, since there case of overflow, but who cares,
	// all is matter that we found such 'to' and tried to send packet.
	return !!st;
}
#endif

qbool NET_SendUDPPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
	struct sockaddr_storage addr;
	int ret;
	int socket = NET_GetSocket(netsrc, false);

	if (socket == INVALID_SOCKET)
		return false;

	NetadrToSockadr (&to, &addr);

	ret = sendto (socket, data, length, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (ret == -1)
	{
		int err = qerrno;

		if (err == EWOULDBLOCK || err == ECONNREFUSED || err == EADDRNOTAVAIL)
			; // nothing
		else
			Con_Printf ("NET_SendPacket: sendto: (%i): %s %i\n", err, strerror(err), socket);
	}

	return true;
}

void NET_SendPacketEx (netsrc_t netsrc, int length, void *data, netadr_t to, qbool delay)
{
#ifndef SERVERONLY
	if (delay)
	{
		NET_SendDelayedPacket (netsrc, length, data, to);
		return;
	}
#endif

#ifndef SERVERONLY
	if (to.type == NA_LOOPBACK)
	{
		NET_SendLoopPacket (netsrc, length, data, to);
		return;
	}
#endif

// TCPCONNECT -->
#ifndef SERVERONLY
	if (netsrc == NS_CLIENT && cls.sockettcp != INVALID_SOCKET && NET_SendTCPPacket_CL(netsrc, length, data, to))
		return;
#endif

#ifndef CLIENTONLY
	if (netsrc == NS_SERVER && svs.tcpstreams && NET_SendTCPPacket_SV(netsrc, length, data, to))
		return;
#endif
// <--TCPCONNECT

	NET_SendUDPPacket(netsrc, length, data, to);
}

void NET_SendPacket (netsrc_t netsrc, int length, void *data, netadr_t to)
{
#ifdef SERVERONLY
	qbool delay = false;
#else
	qbool delay = (netsrc == NS_CLIENT && cl_delay_packet.integer);
#endif

	NET_SendPacketEx (netsrc, length, data, to, delay);
}

//=============================================================================

qbool TCP_Set_KEEPALIVE(int sock)
{
	int		iOptVal = 1;

	if (sock == INVALID_SOCKET)
	{
		Con_Printf("TCP_Set_KEEPALIVE: invalid socket\n");
		return false;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&iOptVal, sizeof(iOptVal)) == SOCKET_ERROR)
	{
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt: (%i): %s\n", qerrno, strerror (qerrno));
		return false;
	}

#if defined(__linux__)

//	The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes, 
//  if the socket option SO_KEEPALIVE has been set on this socket.

	iOptVal = 60;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (void*)&iOptVal, sizeof(iOptVal)) == -1)
	{
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPIDLE: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

//  The time (in seconds) between individual keepalive probes.
	iOptVal = 30;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (void*)&iOptVal, sizeof(iOptVal)) == -1)
	{
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPINTVL: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

//  The maximum number of keepalive probes TCP should send before dropping the connection. 
	iOptVal = 6;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPCNT, (void*)&iOptVal, sizeof(iOptVal)) == -1)
	{
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPCNT: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}
#else
	// FIXME: windows, bsd etc...
#endif

	return true;
}

int TCP_OpenStream (netadr_t remoteaddr)
{
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_storage qs;

	NetadrToSockadr(&remoteaddr, &qs);
	temp = sizeof(struct sockaddr_in);

	if ((newsocket = socket (((struct sockaddr_in*)&qs)->sin_family, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		Con_Printf ("TCP_OpenStream: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

	if (connect (newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET)
	{
		Con_Printf ("TCP_OpenStream: connect: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

#ifndef _WIN32
	if ((fcntl (newsocket, F_SETFL, O_NONBLOCK)) == -1)
	{ // O'Rly?! @@@
		Con_Printf ("TCP_OpenStream: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}
#endif

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
	{ // make asynchronous
		Con_Printf ("TCP_OpenStream: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	return newsocket;
}

int TCP_OpenListenSocket (unsigned short int port)
{
	int newsocket;
	struct sockaddr_in address = {0};
	unsigned long nonblocking = true;
	int i;

	if ((newsocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		Con_Printf ("TCP_OpenListenSocket: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

	if (ioctlsocket (newsocket, FIONBIO, &nonblocking) == -1)
	{ // make asynchronous
		Con_Printf ("TCP_OpenListenSocket: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

#ifdef __APPLE__
	address.sin_len = sizeof(address); // apple are special...
#endif
	address.sin_family = AF_INET;

	// check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < COM_Argc())
	{
		address.sin_addr.s_addr = inet_addr(COM_Argv(i+1));
		Con_DPrintf ("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	}
	else
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}
	
	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons(port);

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Con_Printf ("TCP_OpenListenSocket: bind: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (listen (newsocket, 2) == INVALID_SOCKET)
	{
		Con_Printf ("TCP_OpenListenSocket: listen: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (!TCP_Set_KEEPALIVE(newsocket))
	{
		Con_Printf ("TCP_OpenListenSocket: TCP_Set_KEEPALIVE: failed\n");
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	return newsocket;
}

int UDP_OpenSocket (unsigned short int port)
{
	int newsocket;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		Con_Printf ("UDP_OpenSocket: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

#ifndef _WIN32
	if ((fcntl (newsocket, F_SETFL, O_NONBLOCK)) == -1)
	{ // O'Rly?! @@@
		Con_Printf ("UDP_OpenSocket: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}
#endif

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
	{ // make asynchronous
		Con_Printf ("UDP_OpenSocket: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	address.sin_family = AF_INET;

	// check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < COM_Argc())
	{
		address.sin_addr.s_addr = inet_addr(COM_Argv(i+1));
		Con_DPrintf ("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	}
	else
	{
		address.sin_addr.s_addr = INADDR_ANY;
	}

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons(port);

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1)
	{
		Con_Printf ("UDP_OpenSocket: bind: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	return newsocket;
}

qbool NET_Sleep(int msec, qbool stdinissocket)
{
	struct timeval	timeout;
	fd_set			fdset;
	qbool			stdin_ready = false;
	int				maxfd = 0;

	FD_ZERO (&fdset);

	if (stdinissocket)
	{
		FD_SET (0, &fdset); // stdin is processed too (tends to be socket 0)
		maxfd = max(0, maxfd);
	}

	if (svs.socketip != INVALID_SOCKET)
	{
		FD_SET(svs.socketip, &fdset); // network socket
		maxfd = max(svs.socketip, maxfd);
	}

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	switch (select(maxfd + 1, &fdset, NULL, NULL, &timeout))
	{
		case -1: break;
		case  0: break;
		default:

		if (stdinissocket)
			stdin_ready = FD_ISSET (0, &fdset);

		break;
	}

	return stdin_ready;
}

void NET_GetLocalAddress (int socket, netadr_t *out)
{
	char buff[512];
	struct sockaddr_storage address;
	size_t namelen;
	netadr_t adr = {0};
	qbool notvalid = false;

	strlcpy (buff, "localhost", sizeof (buff));
	gethostname (buff, sizeof (buff));
	buff[sizeof(buff) - 1] = 0;

	if (!NET_StringToAdr (buff, &adr))	//urm
		NET_StringToAdr ("127.0.0.1", &adr);

	namelen = sizeof(address);
	if (getsockname (socket, (struct sockaddr *)&address, (socklen_t *)&namelen) == -1)
	{
		notvalid = true;
		NET_StringToSockaddr("0.0.0.0", (struct sockaddr_storage *)&address);
//		Sys_Error ("NET_Init: getsockname:", strerror(qerrno));
	}

	SockadrToNetadr(&address, out);
	if (!*(int*)out->ip)	//socket was set to auto
		*(int *)out->ip = *(int *)adr.ip;	//change it to what the machine says it is, rather than the socket.

#ifndef SERVERONLY
	if (notvalid)
		Com_Printf_State (PRINT_FAIL, "Couldn't detect local ip\n");
	else
		Com_Printf_State (PRINT_OK, "IP address %s\n", NET_AdrToString (*out));
#endif
}

void NET_Init (void)
{
#ifdef _WIN32
	WORD wVersionRequested;
	int r;

	wVersionRequested = MAKEWORD(1, 1);
	r = WSAStartup (wVersionRequested, &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif

	// init the message buffer
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));

	Con_DPrintf("UDP Initialized\n");

#ifndef SERVERONLY
	cls.socketip = INVALID_SOCKET;
// TCPCONNECT -->
	cls.sockettcp = INVALID_SOCKET;
// <--TCPCONNECT
#endif

#ifndef CLIENTONLY

	Cvar_Register (&sv_local_addr);

	svs.socketip = INVALID_SOCKET;
// TCPCONNECT -->
	svs.sockettcp = INVALID_SOCKET;
// <--TCPCONNECT
#endif

#ifdef SERVERONLY
	// As client+server we init it in SV_SpawnServer().
	// As serveronly we do it here.
	NET_InitServer();
#endif
}

void NET_Shutdown (void)
{
#ifndef CLIENTONLY
	NET_CloseServer();
#endif

#ifndef SERVERONLY
	NET_CloseClient();
#endif

#ifdef _WIN32
	WSACleanup ();
#endif
}

#ifndef SERVERONLY
void NET_InitClient(void)
{
	int port = PORT_CLIENT;
	int p;

	p = COM_CheckParm ("-clientport");
	if (p && p < COM_Argc())
	{
		port = atoi(COM_Argv(p+1));
	}

	if (cls.socketip == INVALID_SOCKET)
		cls.socketip = UDP_OpenSocket (port);

	if (cls.socketip == INVALID_SOCKET)
		cls.socketip = UDP_OpenSocket (PORT_ANY); // any dynamic port

	if (cls.socketip == INVALID_SOCKET)
		Sys_Error ("Couldn't allocate client socket");

	// determine my name & address
	NET_GetLocalAddress (cls.socketip, &net_local_cl_ipadr);

	Com_Printf_State (PRINT_OK, "Client port Initialized\n");
}

void NET_CloseClient (void)
{
	if (cls.socketip != INVALID_SOCKET)
	{
		closesocket(cls.socketip);
		cls.socketip = INVALID_SOCKET;
	}

// TCPCONNECT -->
	// FIXME: is it OK? Probably we should send disconnect?
	if (cls.sockettcp != INVALID_SOCKET)
	{
		closesocket(cls.sockettcp);
		cls.sockettcp = INVALID_SOCKET;
	}
// <--TCPCONNECT
}

#endif

#ifndef CLIENTONLY

//
// Open server TCP port.
// NOTE: Zero port will actually close already opened port.
//
void NET_InitServer_TCP(unsigned short int port)
{
	// close socket first.
	if (svs.sockettcp != INVALID_SOCKET)
	{
		Con_Printf("Server TCP port closed\n");
		closesocket(svs.sockettcp);
		svs.sockettcp = INVALID_SOCKET;
		net_local_sv_tcpipadr.type = NA_INVALID;
	}

	if (port)
	{
		svs.sockettcp = TCP_OpenListenSocket (port);

		if (svs.sockettcp != INVALID_SOCKET)
		{
			// get local address.
			NET_GetLocalAddress (svs.sockettcp, &net_local_sv_tcpipadr);
			Con_Printf("Opening server TCP port %u\n", (unsigned int)port);
		}
		else
		{
			Con_Printf("Failed to open server TCP port %u\n", (unsigned int)port);
		}
	}
}

void NET_InitServer (void)
{
	int port = PORT_SERVER;
	int p;

	p = COM_CheckParm ("-port");
	if (p && p < COM_Argc())
	{
		port = atoi(COM_Argv(p+1));
	}

	if (svs.socketip == INVALID_SOCKET)
	{
		svs.socketip = UDP_OpenSocket (port);
	}

	if (svs.socketip != INVALID_SOCKET)
	{
		NET_GetLocalAddress (svs.socketip, &net_local_sv_ipadr);
		Cvar_SetROM (&sv_local_addr, NET_AdrToString (net_local_sv_ipadr));
	}
	else
	{
		// FIXME: is it right???
		Cvar_SetROM (&sv_local_addr, "");
	}

// TCPCONNECT -->
#if 0 // qqshka: TCP port shared with QTV now, so it opened outside here.
	p = COM_CheckParm ("-tcpport");
	if (p && p < COM_Argc())
	{
		NET_InitServer_TCP(atoi(COM_Argv(p+1)));
	}
#endif
// <-- TCPCONNECT

	if (svs.socketip == INVALID_SOCKET)
	{
#ifdef SERVERONLY
		Sys_Error
#else
		Con_Printf
#endif
 		("WARNING: Couldn't allocate server socket\n");
	}
}

void NET_CloseServer (void)
{
	if (svs.socketip != INVALID_SOCKET)
	{
		closesocket(svs.socketip);
		svs.socketip = INVALID_SOCKET;
	}

	net_local_sv_ipadr.type = NA_LOOPBACK; // FIXME: why not NA_INVALID?

// TCPCONNECT -->
	NET_InitServer_TCP(0);
// <--TCPCONNECT
}
#endif

