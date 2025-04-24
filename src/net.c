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
#include "utils.h"
#define MAX_STRINGS 16 // well, this used not only for va, anyway, static buffers is evil...
#endif

#ifdef _WIN32
WSADATA winsockdata;
#define EZ_TCP_WOULDBLOCK (WSAEWOULDBLOCK)
#else
#define EZ_TCP_WOULDBLOCK (EINPROGRESS)
#endif


#ifndef SERVERONLY
netadr_t	net_local_cl_ipadr;

void NET_CloseClient (void);

static void cl_net_clientport_changed(cvar_t* var, char* value, qbool* cancel);
static cvar_t cl_net_clientport = { "cl_net_clientport", "27001", CVAR_AUTO, cl_net_clientport_changed };  // Was PORT_CLIENT in protocol.h

typedef struct {
	int port;
	double rtt;
} portpingprobe_t;

typedef struct {
	int thread_id;
	int num_ports;
	struct sockaddr_in *addr;
	portpingprobe_t *results;
} portpingprobe_worker_args_t;

typedef struct {
	char *target_addr;
	char *original_addr;
} portpingprobe_orchestrator_args_t;

static SDL_atomic_t portpingprobe_status;
static SDL_atomic_t portpingprobe_progress;

static void cl_portpingprobe_probes_changed(cvar_t *var, char *val, qbool *cancel);
static void cl_portpingprobe_port_probes_changed(cvar_t *var, char *val, qbool *cancel);
static cvar_t cl_portpingprobe_probes      = {"cl_portpingprobe_probes", "500", 0, cl_portpingprobe_probes_changed};
static cvar_t cl_portpingprobe_port_probes = {"cl_portpingprobe_port_probes", "1", 0, cl_portpingprobe_port_probes_changed};
static cvar_t cl_portpingprobe_enable      = {"cl_portpingprobe_enable", "0"};
static cvar_t cl_portpingprobe_delay       = {"cl_portpingprobe_delay",  "0"};

static double NET_PortPing(const struct sockaddr_in *srv_adr, const int probe_port);
static int NET_PortPingProbeWorker(void *data);
static SDL_Thread **NET_PortPingProbeInitWorkers(struct sockaddr_in *addr, int num_threads, portpingprobe_t *results);
static int NET_PortPingProbeOrchestrator(void *data);

#define MIN_TCP_TIMEOUT  500
#define MAX_TCP_TIMEOUT 5000

static cvar_t net_tcp_timeout = { "net_tcp_timeout", "2000" };
#endif

#ifndef CLIENTONLY
netadr_t	net_local_sv_ipadr;
netadr_t	net_local_sv_tcpipadr;

cvar_t		sv_local_addr = {"sv_local_addr", "", CVAR_ROM};
#endif

netadr_t	net_from;
sizebuf_t	net_message;

static byte net_message_buffer[MSG_BUF_SIZE];

// forward definition.
qbool NET_GetPacketEx (netsrc_t netsrc, qbool delay);
void NET_SendPacketEx (netsrc_t netsrc, int length, void *data, netadr_t to, qbool delay);

#ifdef SERVERONLY
#define TCP_LISTEN_BACKLOG 2
#else
#define TCP_LISTEN_BACKLOG 1
#ifndef _WIN32
extern qbool stdin_ready;
extern int do_stdin;
#endif
#endif

//=============================================================================
//
// LOOPBACK defs.
//

#define MAX_LOOPBACK 4 // must be a power of two

typedef struct {
	byte	data[MAX_UDP_PACKET];
	int		datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	unsigned int	get, send;
} loopback_t;

loopback_t	loopbacks[2];

//=============================================================================
//
// Delayed packets, CLIENT ONLY.
//

#ifndef SERVERONLY

typedef struct packet_queue_s {
	cl_delayed_packet_t packets[CL_MAX_DELAYED_PACKETS];
	int head;
	int tail;
	qbool outgoing;
} packet_queue_t;

static packet_queue_t delay_queue_get;
static packet_queue_t delay_queue_send;

static inline void NET_PacketQueueSetNextIndex(int* index)
{
	*index = (*index + 1) % CL_MAX_DELAYED_PACKETS;
}

static qbool NET_PacketQueueRemove(packet_queue_t* queue, sizebuf_t* buffer, netadr_t* from_address)
{
	cl_delayed_packet_t* next = &queue->packets[queue->head];
	double time;

	// Empty queue
	if (!next->time) {
		return false;
	}

	// Not time yet
	time = Sys_DoubleTime();
	if (next->time > time) {
		return false;
	}

	SZ_Clear(buffer);
	SZ_Write(buffer, next->data, next->length);
	*from_address = next->addr;
	next->time = 0;

	NET_PacketQueueSetNextIndex(&queue->head);
	return true;
}

static qbool NET_PacketQueueAdd(packet_queue_t* queue, byte* data, int size, netadr_t addr)
{
	cl_delayed_packet_t* next = &queue->packets[queue->tail];
	float deviation = 0;
	float ms_delay;

	if (cl_delay_packet_dev.integer) {
		deviation = f_rnd(-bound(0, cl_delay_packet_dev.integer, CL_MAX_PACKET_DELAY_DEVIATION), bound(0, cl_delay_packet_dev.integer, CL_MAX_PACKET_DELAY_DEVIATION));
	}

	// If buffer is full, can't prevent packet loss - drop this packet
	if (next->time && queue->head == queue->tail) {
		return false;
	}

	// calculate delay based on settings
	if (cls.state != ca_active) {
		// not yet connected, go as fast as possible
		ms_delay = 0;
	}
	else if (cl_delay_packet_target.integer && *(int*)data != -1) {
		if (!queue->outgoing) {
			// dynamically change delay to target a particular latency
			int sequence_ack;
			int sequencemod;
			double expected_latency;

			MSG_BeginReading();
			MSG_ReadLong(); // sequence =
			sequence_ack = MSG_ReadLong();
			sequence_ack &= ~(1 << 31);

			sequencemod = sequence_ack & UPDATE_MASK;
			expected_latency = (cls.realtime - cl.frames[sequencemod].senttime) * 1000.0;

			ms_delay = max(0, cl_delay_packet_target.value - expected_latency + deviation);
		}
		else {
			// push some of the delay onto outgoing
			ms_delay = cls.latency / 2;
		}
	}
	else {
		// delay by constant amount
		ms_delay = bound(0, 0.5 * cl_delay_packet.integer + deviation, CL_MAX_PACKET_DELAY);
	}

	memmove(next->data, data, size);
	next->length = size;
	next->addr = addr;
	next->time = Sys_DoubleTime() + 0.001 * ms_delay;

	NET_PacketQueueSetNextIndex(&queue->tail);
	return true;
}

static cl_delayed_packet_t* NET_PacketQueuePeek(packet_queue_t* queue)
{
	cl_delayed_packet_t* next = &queue->packets[queue->head];

	// Empty queue
	if (!next->time)
		return NULL;

	return next;
}

static void NET_PacketQueueAdvance(packet_queue_t* queue)
{
	cl_delayed_packet_t* head = NET_PacketQueuePeek(queue);

	if (head != NULL)
	{
		head->time = 0;

		NET_PacketQueueSetNextIndex(&queue->head);
	}
}

#endif

//=============================================================================
//
// Geters.
//
#ifndef CLIENTONLY
int NET_UDPSVPort (void)
{
	return ntohs(net_local_sv_ipadr.port);
}
#endif

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
	if (a.type == NA_LOOPBACK) {
		return "loopback";
	}
#endif

	snprintf (s[idx], sizeof(s[0]), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	return s[idx++];
}

char *NET_BaseAdrToString (const netadr_t a)
{
	static char s[MAX_STRINGS][32]; // 22 should be OK too
	static int idx = 0;

	idx %= MAX_STRINGS;

#ifndef SERVERONLY
	if (a.type == NA_LOOPBACK) {
		return "loopback";
	}
#endif

	snprintf (s[idx], sizeof(s[0]), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s[idx++];
}

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
	}
	else {
		if (!(h = gethostbyname(copy))) {
			return false;
		}

		if (h->h_addrtype != AF_INET) {
			return false;
		}

		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

qbool NET_StringToAdr (const char *s, netadr_t *a)
{
	struct sockaddr_storage sadr;

#ifndef SERVERONLY
	if (!strcmp(s, "local")) {
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

#ifndef CLIENTONLY
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
#endif

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
		return NET_PacketQueueRemove(&delay_queue_get, &net_message, &net_from);
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
	qbool delay = (netsrc == NS_CLIENT && (cl_delay_packet.integer || cl_delay_packet_target.integer));
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
		NET_PacketQueueAdd (&delay_queue_send, data, length, to);
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

#ifndef SERVERONLY
qbool CL_UnqueOutputPacket(qbool sendall)
{
	double time = 0;
	cl_delayed_packet_t* packet = NULL;
	qbool released = false;

	while ((packet = NET_PacketQueuePeek(&delay_queue_send)))
	{
		if (!time) {
			time = Sys_DoubleTime();
		}

		// Not yet ready to send
		if (packet->time > time && !sendall)
			break;

		// ok, send it
		NET_SendPacketEx(NS_CLIENT, packet->length, packet->data, packet->addr, false);

		// mark as unused slot
		NET_PacketQueueAdvance(&delay_queue_send);

		released = true;
	}

	return released;
}
#endif

//=============================================================================

qbool TCP_Set_KEEPALIVE(int sock)
{
	int		iOptVal = 1;

	if (sock == INVALID_SOCKET) {
		Con_Printf("TCP_Set_KEEPALIVE: invalid socket\n");
		return false;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&iOptVal, sizeof(iOptVal)) == SOCKET_ERROR) {
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt: (%i): %s\n", qerrno, strerror (qerrno));
		return false;
	}

#if defined(__linux__)

//	The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes, 
//  if the socket option SO_KEEPALIVE has been set on this socket.

	iOptVal = 60;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (void*)&iOptVal, sizeof(iOptVal)) == -1) {
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPIDLE: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

//  The time (in seconds) between individual keepalive probes.
	iOptVal = 30;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (void*)&iOptVal, sizeof(iOptVal)) == -1) {
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPINTVL: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

//  The maximum number of keepalive probes TCP should send before dropping the connection. 
	iOptVal = 6;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPCNT, (void*)&iOptVal, sizeof(iOptVal)) == -1) {
		Con_Printf ("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPCNT: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}
#else
	// FIXME: windows, bsd etc...
#endif

	return true;
}

int TCP_OpenStream(netadr_t remoteaddr)
{
	unsigned long _true = true;
	int newsocket;
	int temp;
	struct sockaddr_storage qs;

	NetadrToSockadr(&remoteaddr, &qs);
	temp = sizeof(struct sockaddr_in);

	if ((newsocket = socket(((struct sockaddr_in*)&qs)->sin_family, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		Con_Printf("TCP_OpenStream: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

	// Set socket to non-blocking
#if !defined(_WIN32)
	if ((fcntl(newsocket, F_SETFL, O_NONBLOCK)) == -1) { // O'Rly?! @@@
		Con_Printf("TCP_OpenStream: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}
#endif

	if (ioctlsocket(newsocket, FIONBIO, &_true) == -1) { // make asynchronous
		Con_Printf("TCP_OpenStream: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (connect (newsocket, (struct sockaddr *)&qs, temp) == INVALID_SOCKET) {
		// Socket is non-blocking, so check if the error is just because it would block
		if (qerrno == EZ_TCP_WOULDBLOCK) {
			struct timeval t;
			fd_set socket_set;

			t.tv_sec = bound(MIN_TCP_TIMEOUT, net_tcp_timeout.integer, MAX_TCP_TIMEOUT) / 1000;
			t.tv_usec = (bound(MIN_TCP_TIMEOUT, net_tcp_timeout.integer, MAX_TCP_TIMEOUT) % 1000) * 1000;

			FD_ZERO(&socket_set);
			FD_SET(newsocket, &socket_set);

			if (select(newsocket + 1, NULL, &socket_set, NULL, &t) <= 0) {
				Con_Printf("TCP_OpenStream: connection timeout\n");
				closesocket(newsocket);
				return INVALID_SOCKET;
			}
			else {
				int error = SOCKET_ERROR;
#ifdef _WIN32
				int len = sizeof(error);
#else
				socklen_t len = sizeof(error);
#endif

				getsockopt(newsocket, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
				if (error != 0) {
					Con_Printf("TCP_OpenStream: connect: (%i): %s\n", error, strerror(error));
					closesocket(newsocket);
					return INVALID_SOCKET;
				}
			}
		}
		else {
			Con_Printf("TCP_OpenStream: connect: (%i): %s\n", qerrno, strerror(qerrno));
			closesocket(newsocket);
			return INVALID_SOCKET;
		}
	}

	return newsocket;
}

int TCP_OpenListenSocket (unsigned short int port)
{
	int newsocket;
	struct sockaddr_in address = { 0 };
	unsigned long nonblocking = true;
	int i;

	if ((newsocket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
		Con_Printf ("TCP_OpenListenSocket: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

#ifndef _WIN32
	if ((fcntl (newsocket, F_SETFL, O_NONBLOCK)) == -1) { // O'Rly?! @@@
		Con_Printf ("TCP_OpenListenSocket: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}
#endif

	if (ioctlsocket (newsocket, FIONBIO, &nonblocking) == -1) { // make asynchronous
		Con_Printf ("TCP_OpenListenSocket: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

#ifdef __APPLE__
	address.sin_len = sizeof(address); // apple are special...
#endif
	address.sin_family = AF_INET;

	// check for interface binding option
	if ((i = COM_CheckParm(cmdline_param_net_ipaddress)) != 0 && i < COM_Argc()) {
		address.sin_addr.s_addr = inet_addr(COM_Argv(i+1));
		Con_DPrintf ("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	}
	else {
		address.sin_addr.s_addr = INADDR_ANY;
	}
	
	if (port == PORT_ANY) {
		address.sin_port = 0;
	}
	else {
		address.sin_port = htons(port);
	}

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1) {
		Con_Printf ("TCP_OpenListenSocket: bind: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (listen (newsocket, TCP_LISTEN_BACKLOG) == INVALID_SOCKET) {
		Con_Printf ("TCP_OpenListenSocket: listen: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	if (!TCP_Set_KEEPALIVE(newsocket)) {
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

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
		Con_Printf ("UDP_OpenSocket: socket: (%i): %s\n", qerrno, strerror(qerrno));
		return INVALID_SOCKET;
	}

#ifndef _WIN32
	if ((fcntl (newsocket, F_SETFL, O_NONBLOCK)) == -1) { // O'Rly?! @@@
		Con_Printf ("UDP_OpenSocket: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}
#endif

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1) { // make asynchronous
		Con_Printf ("UDP_OpenSocket: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		closesocket(newsocket);
		return INVALID_SOCKET;
	}

	address.sin_family = AF_INET;

	// check for interface binding option
	if ((i = COM_CheckParm(cmdline_param_net_ipaddress)) != 0 && i < COM_Argc()) {
		address.sin_addr.s_addr = inet_addr(COM_Argv(i+1));
		Con_DPrintf ("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	}
	else {
		address.sin_addr.s_addr = INADDR_ANY;
	}

	if (port == PORT_ANY) {
		address.sin_port = 0;
	}
	else {
		address.sin_port = htons(port);
	}

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1) {
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

	if (stdinissocket) {
		FD_SET (0, &fdset); // stdin is processed too (tends to be socket 0)
		maxfd = max(0, maxfd);
	}

#ifndef CLIENTONLY
	if (svs.socketip != INVALID_SOCKET) {
		FD_SET(svs.socketip, &fdset); // network socket
		maxfd = max(svs.socketip, maxfd);
	}
#endif

	timeout.tv_sec = msec/1000;
	timeout.tv_usec = (msec%1000)*1000;
	switch (select(maxfd + 1, &fdset, NULL, NULL, &timeout))
	{
		case -1:
		case  0:
			break;
		default:
			if (stdinissocket) {
				stdin_ready = FD_ISSET(0, &fdset);
			}
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
	if (getsockname (socket, (struct sockaddr *)&address, (socklen_t *)&namelen) == -1) {
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

	Cvar_Register(&cl_net_clientport);
	Cvar_Register(&net_tcp_timeout);

	Cvar_Register(&cl_portpingprobe_delay);
	Cvar_Register(&cl_portpingprobe_enable);
	Cvar_Register(&cl_portpingprobe_probes);
	Cvar_Register(&cl_portpingprobe_port_probes);
	NET_SetPortPingProbeStatus(PORTPINGPROBE_READY);

	delay_queue_send.outgoing = true;
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
static void cl_net_clientport_changed(cvar_t* var, char* value, qbool* cancel)
{
	int new_socket = INVALID_SOCKET;
	int new_port = atoi(value);
	qbool set_auto = false;

	// No change
	if (new_port == var->integer) {
		*cancel = true;
		return;
	}

#ifndef CLIENTONLY
	// FIXME: Technically this could be changed on the command line or dynamically allocated
	//        no damage done if they do this when disconnected 
	if (new_port == PORT_SERVER) {
		*cancel = true;
		Con_Printf("Port %i is reserved for the internal server.\n", new_port);
		return;
	}
#endif

	// In theory you could be connected to mvd...
	if (cls.state != ca_disconnected) {
		Con_Printf("You must be disconnected to change %s.\n", var->name);
		*cancel = true;
		return;
	}

	if (new_port > 0) {
		new_socket = UDP_OpenSocket(new_port);

		if (new_socket == INVALID_SOCKET) {
			Con_Printf("Unable to open new socket on port %d\n", new_port);
			*cancel = true;
			return;
		}
	}
	if (new_socket == INVALID_SOCKET) {
		new_socket = UDP_OpenSocket(PORT_ANY);
		set_auto = true;
	}
	
	if (new_socket == INVALID_SOCKET) {
		Con_Printf("Unable to open new socket\n");
		*cancel = true;
		return;
	}

	if (cls.socketip != INVALID_SOCKET) {
		closesocket(cls.socketip);
	}

	cls.socketip = new_socket;
	NET_GetLocalAddress(cls.socketip, &net_local_cl_ipadr);
	if (set_auto) {
		Com_Printf("Client port allocated: %i\n", ntohs(net_local_cl_ipadr.port));
		Cvar_AutoSetInt(var, ntohs(net_local_cl_ipadr.port));
	}
	else {
		Cvar_AutoReset(var);
	}
}

// This is called after config loaded
void NET_InitClient(void)
{
	int port = cl_net_clientport.integer;
	qbool set_auto = false;
	qbool set = (cls.socketip == INVALID_SOCKET);
	int p;

	// Allow user to override the config file
	p = COM_CheckParm(cmdline_param_net_clientport);
	if (p && p < COM_Argc()) {
		port = atoi(COM_Argv(p + 1));
		set_auto = true;
	}

	if (cls.socketip == INVALID_SOCKET && port > 0)
		cls.socketip = UDP_OpenSocket(port);

	if (cls.socketip == INVALID_SOCKET) {
		cls.socketip = UDP_OpenSocket(PORT_ANY); // any dynamic port
		set_auto = true;
	}

	if (cls.socketip == INVALID_SOCKET) {
		Sys_Error("Couldn't allocate client socket");
		return;
	}

	// init the message buffer
	SZ_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));

	// determine my name & address
	NET_GetLocalAddress(cls.socketip, &net_local_cl_ipadr);
	if (set) {
		if (set_auto) {
			Cvar_AutoSetInt(&cl_net_clientport, ntohs(net_local_cl_ipadr.port));
		}
		else {
			Cvar_AutoReset(&cl_net_clientport);
		}
	}

	Com_Printf_State(PRINT_OK, "Client port initialized: %i\n", ntohs(net_local_cl_ipadr.port));
}

void NET_CloseClient (void)
{
	if (cls.socketip != INVALID_SOCKET) {
		closesocket(cls.socketip);
		cls.socketip = INVALID_SOCKET;
	}

// TCPCONNECT -->
	// FIXME: is it OK? Probably we should send disconnect?
	if (cls.sockettcp != INVALID_SOCKET) {
		closesocket(cls.sockettcp);
		cls.sockettcp = INVALID_SOCKET;
	}
// <--TCPCONNECT
}

qbool CL_QueInputPacket(void)
{
	if (!NET_GetPacketEx(NS_CLIENT, false))
		return false;

	return NET_PacketQueueAdd(&delay_queue_get, net_message.data, net_message.cursize, net_from);
}

void CL_ClearQueuedPackets(void)
{
	memset(&delay_queue_get, 0, sizeof(delay_queue_get));
	memset(&delay_queue_send, 0, sizeof(delay_queue_send));
	delay_queue_send.outgoing = true;
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

	p = COM_CheckParm (cmdline_param_net_serverport);
	if (p && p < COM_Argc()) {
		port = atoi(COM_Argv(p+1));
	}

	if (svs.socketip == INVALID_SOCKET) {
		svs.socketip = UDP_OpenSocket (port);
	}

	if (svs.socketip != INVALID_SOCKET) {
		NET_GetLocalAddress (svs.socketip, &net_local_sv_ipadr);
		Cvar_SetROM (&sv_local_addr, NET_AdrToString (net_local_sv_ipadr));
	}
	else {
		// FIXME: is it right???
		Cvar_SetROM (&sv_local_addr, "");
	}

	if (svs.socketip == INVALID_SOCKET) {
#ifdef SERVERONLY
		Sys_Error
#else
		Con_Printf
#endif
			("WARNING: Couldn't allocate server socket\n");
	}

#ifndef SERVERONLY
	// init the message buffer
	SZ_Init (&net_message, net_message_buffer, sizeof(net_message_buffer));
#endif
}

void NET_CloseServer (void)
{
	if (svs.socketip != INVALID_SOCKET) {
		closesocket(svs.socketip);
		svs.socketip = INVALID_SOCKET;
	}

	net_local_sv_ipadr.type = NA_LOOPBACK; // FIXME: why not NA_INVALID?

// TCPCONNECT -->
	NET_InitServer_TCP(0);
// <--TCPCONNECT
}
#endif

static void cl_portpingprobe_probes_changed(cvar_t *var, char *val, qbool *cancel)
{
	int probes = Q_atoi(val);

	if (probes < 1 || probes > 1000)
	{
		Com_Printf("The number of probes needs to be between 1 and 1000.\n");
		*cancel = true;
	}
}

static void cl_portpingprobe_port_probes_changed(cvar_t *var, char *val, qbool *cancel)
{
	int probes = Q_atoi(val);

	if (probes < 1 || probes > 5)
	{
		Com_Printf("The number of port probes needs to be between 1 and 5.\n");
		*cancel = true;
	}
}

static double NET_PortPing(const struct sockaddr_in *srv_adr, const int probe_port) {
	static char payload[] = {0xff, 0xff, 0xff, 0xff, 'p', 'i', 'n', 'g'};
	static struct timeval timeout = {1, 0};
	struct sockaddr_in cli_addr;
	double start;
	char buf;
	int sock, ret = -1;
#ifdef _WIN32
	static int timeout_ms = -1;

	if (timeout_ms == -1)
	{
		timeout_ms = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;
	}
#endif

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		Com_Printf("NET_PortPing: Unable to initialize socket\n");
		return -1;
	}

#ifdef _WIN32
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms)) < 0)
#else
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
#endif
	{
		Com_Printf("NET_PortPing: Unable to set timeout on socket\n");
		goto cleanup;
	}

	memset(&cli_addr, 0, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = INADDR_ANY;
	cli_addr.sin_port = htons(probe_port);

	if (bind(sock, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
	{
		Com_DPrintf("NET_PortPing: Port is already in use: %d\n", probe_port);
		goto cleanup;
	}

	start = Sys_DoubleTime();

	if ((ret = sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)srv_adr, sizeof(*srv_adr))) < 0)
	{
		Com_Printf("NET_PortPing: Unable to send data to the server from port %d\n", probe_port);
		goto cleanup;
	}

	// If this fails, it is an indication that we are being rate-limited by
	// a router along the way, so we'll pause for a while before proceeding.
	if ((ret = recvfrom(sock, &buf, sizeof(buf), 0, NULL, NULL)) <= 0)
	{
		Com_DPrintf("NET_PortPing: No data received from the server\n");
		Sys_MSleep(100);
		ret = -1;
	}

cleanup:
	closesocket(sock);

	return ret == -1 ? ret : Sys_DoubleTime() - start;
}

static int NET_PortPingProbeWorker(void *data)
{
	portpingprobe_worker_args_t *args = (portpingprobe_worker_args_t *)data;
	double rtt;
	int i, port;
	int *ports = Q_malloc(args->num_ports * sizeof(int));
	int num_probes = args->num_ports * cl_portpingprobe_port_probes.integer;

	Com_DPrintf("[%d]: Generating %d random ports\n", args->thread_id, args->num_ports);
	for (i = 0; i < args->num_ports; i++)
	{
		ports[i] = rand() % (65536 - 1024) + 1024;
	}

	Com_DPrintf("[%d]: Launching %d probes\n", args->thread_id, num_probes);
	for (i = 0; i < num_probes; i++)
	{
		port = ports[i % args->num_ports];
		rtt = NET_PortPing(args->addr, port);
		args->results[i].port = port;
		args->results[i].rtt = rtt;

		// If the client issues a new connection while a probe is in
		// progress or disconnects, we'll abort and exit gracefully.
		if (NET_GetPortPingProbeStatus() == PORTPINGPROBE_ABORT)
		{
			goto cleanup;
		}

		// If set, we'll sleep for delay number of ms before moving on
		// to the next iteration.
		if (cl_portpingprobe_delay.integer > 0)
		{
			Sys_MSleep(cl_portpingprobe_delay.integer);
		}

		Com_DPrintf("[%d]: Probing port %d: RTT = %f ms\n", args->thread_id, port, rtt*1000.0);

		// Increment the progress counter.
		SDL_AtomicIncRef(&portpingprobe_progress);
	}

cleanup:
	free(args);
	free(ports);

	return 0;
}

static SDL_Thread **NET_PortPingProbeInitWorkers(struct sockaddr_in *addr, int num_threads, portpingprobe_t *results)
{
	SDL_Thread **workers = Q_malloc(sizeof(SDL_Thread *) * num_threads);
	portpingprobe_worker_args_t *args;
	int i, j, num_probes, remaining_probes;

	// Calculate the number of probes each thread should perform.
	num_probes = num_threads == 1
		? cl_portpingprobe_probes.integer
		: cl_portpingprobe_probes.integer / num_threads;
	remaining_probes = num_threads == 1 ? 0 : cl_portpingprobe_probes.integer % num_threads;

	for (i = 0; i < num_threads; i++)
	{
		// The arguments we allocate here will be freed by the
		// NET_PortPingProbeWorker when it is finished. If we can't
		// create the thread, we'll free the current arguments here and
		// let the cleanup handle the previously created workers.
		args = Q_malloc(sizeof(portpingprobe_worker_args_t));
		args->addr = addr;
		args->thread_id = i;
		args->results = &results[i * num_probes * cl_portpingprobe_port_probes.integer];
		args->num_ports = num_probes + (i == num_threads - 1 ? remaining_probes : 0);

		workers[i] = Sys_CreateThread(NET_PortPingProbeWorker, args);
		if (!workers[i])
		{
			Com_Printf("NET_PortPingProbeInitWorkers: Failed to create worker thread %d\n", i);
			Q_free(args);
			goto cleanup;
		}
	}

	return workers;

cleanup:
	// If we've started some threads, we'll need to push the ABORT signal so
	// they are exited before we proceed with further cleanup.
	if (i > 0)
	{
		NET_SetPortPingProbeStatus(PORTPINGPROBE_ABORT);

		for (j = 0; j < i; j++)
		{
			SDL_WaitThread(workers[j], NULL);
		}
	}

	Q_free(workers);

	return NULL;
}

static int NET_PortPingProbeOrchestrator(void *data)
{
	portpingprobe_orchestrator_args_t *args = (portpingprobe_orchestrator_args_t *)data;
	portpingprobe_status_t status = PORTPINGPROBE_READY;
	portpingprobe_t *results = NULL;
	SDL_Thread **workers = NULL;
	struct sockaddr_in addr;
	netadr_t net_addr;
	double best_rtt = 100000;
	int best_port = cl_net_clientport.integer;
	int num_cores = SDL_GetCPUCount();
	int num_threads = num_cores - 4;
	int i;

	if (num_threads < 1)
	{
		num_threads = 1;
	}

	// Ensure that the target address is valid before we proceed.
	if (!NET_StringToAdr(args->target_addr, &net_addr))
	{
		Com_Printf("NET_PortPingProbeOrchestrator: Invalid address %s\n", args->target_addr);
		goto cleanup;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = net_addr.port == 0 ? htons(27500) : net_addr.port;
	memcpy(&addr.sin_addr.s_addr, net_addr.ip, 4);

	// Allocate memory for the probe results.
	results = Q_malloc(sizeof(portpingprobe_t) * cl_portpingprobe_probes.integer * cl_portpingprobe_port_probes.integer);

	// Initialize the worker threads.
	workers = NET_PortPingProbeInitWorkers(&addr, num_threads, results);
	if (workers == NULL)
	{
		goto cleanup;
	}

	// Reset the progress counter. The worker threads will increment the
	// counter during the probing, and the results will be rendered using
	// the progress bar from console.c
	SDL_AtomicSet(&portpingprobe_progress, 0);

	Com_Printf("Probing %s to find the best source port (%d probes, %d threads, %d %s per port)\n",
		args->target_addr, cl_portpingprobe_probes.integer, num_threads,
		cl_portpingprobe_port_probes.integer,
		cl_portpingprobe_port_probes.integer == 1 ? "probe" : "probes");

	// Wait for the worker threads to finish; they will either finish when
	// all probes have been completed or if they are aborted by the user.
	for (i = 0; i < num_threads; i++)
	{
		SDL_WaitThread(workers[i], NULL);
	}

	// The probe was aborted. Let's clean up immediately and end the
	// orchestrator.
	if (NET_GetPortPingProbeStatus() == PORTPINGPROBE_ABORT)
	{
		Com_Printf("Port ping probe aborted\n");
		goto cleanup;
	}

	// Iterate over the results and select the port with the lowest latency.
	for (i = 0; i < cl_portpingprobe_probes.integer * cl_portpingprobe_port_probes.integer; i++)
	{
		Com_DPrintf("Results: index: %d, port: %d RTT: %f\n",
			i, results[i].port, results[i].rtt);

		if (results[i].rtt != -1 && results[i].rtt < best_rtt)
		{
			best_port = results[i].port;
			best_rtt = results[i].rtt;
		}
	}

	Com_Printf("Connecting to %s using source port %d (%.2f ms)\n",
		args->target_addr, best_port, best_rtt*1000.0);
	Cvar_SetValue(&cl_net_clientport, best_port);
	Cbuf_AddText(va("connect %s\n", args->original_addr));

	status = PORTPINGPROBE_COMPLETED;

cleanup:
	Q_free(workers);
	Q_free(results);
	Q_free(args->target_addr);
	Q_free(args->original_addr);
	Q_free(args);

	NET_SetPortPingProbeStatus(status);

	return 0;
}

qbool IsPortPingProbeEnabled(void)
{
	return cl_portpingprobe_enable.integer;
}

void NET_PortPingProbe(const char *target_addr, const char *original_addr)
{
	portpingprobe_orchestrator_args_t *args;

	// Ensure we are ready to start a new probe before we proceed.
	if (NET_GetPortPingProbeStatus() != PORTPINGPROBE_READY)
	{
		return;
	}

	// Set status to PROBING to ensure we aren't launching any more probes
	// when we're already working on one.
	NET_SetPortPingProbeStatus(PORTPINGPROBE_PROBING);

	// We need to allocate and copy the target and original addresses since
	// they will go out of scope as soon as the connect function ends. We'll
	// free the allocations once the worker function completes.
	args = Q_malloc(sizeof(portpingprobe_orchestrator_args_t));
	args->target_addr = Q_strdup(target_addr);
	args->original_addr = Q_strdup(original_addr);

	if (Sys_CreateDetachedThread(NET_PortPingProbeOrchestrator, (void *)args) < 0)
	{
		Com_Printf("NET_PortPingProbe: Unable to launch orchestrator thread\n");
	}
}

void NET_SetPortPingProbeStatus(const portpingprobe_status_t status)
{
	SDL_AtomicSet(&portpingprobe_status, (int)status);
}

portpingprobe_status_t NET_GetPortPingProbeStatus(void)
{
	return (portpingprobe_status_t)SDL_AtomicGet(&portpingprobe_status);
}

int NET_GetPortPingProbeProgress(void)
{
	return ((float)SDL_AtomicGet(&portpingprobe_progress) / (cl_portpingprobe_probes.integer * cl_portpingprobe_port_probes.integer)) * 100;
}
