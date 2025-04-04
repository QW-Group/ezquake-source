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

// net.h -- quake's interface to the networking layer

#ifndef __NET_H__
#define __NET_H__

#include <errno.h>

#define MAX_DUPLICATE_PACKETS (3)

#ifdef _WIN32

//
// OS specific includes.
//

#include <winsock2.h>
#include <ws2tcpip.h>

//
// OS specific types definition.
//

typedef int socklen_t;
typedef SOCKET socket_t;

//
// OS specific definitions.
//

#ifdef EWOULDBLOCK
#undef EWOULDBLOCK
#endif
#ifdef EMSGSIZE
#undef EMSGSIZE
#endif
#ifdef ECONNRESET
#undef ECONNRESET
#endif
#ifdef ECONNABORTED
#undef ECONNABORTED
#endif
#ifdef ECONNREFUSED
#undef ECONNREFUSED
#endif
#ifdef EADDRNOTAVAIL
#undef EADDRNOTAVAIL
#endif
#ifdef EAFNOSUPPORT
#undef EAFNOSUPPORT
#endif

#define EWOULDBLOCK     WSAEWOULDBLOCK
#define EMSGSIZE        WSAEMSGSIZE
#define ECONNRESET      WSAECONNRESET
#define ECONNABORTED    WSAECONNABORTED
#define ECONNREFUSED    WSAECONNREFUSED
#define EADDRNOTAVAIL   WSAEADDRNOTAVAIL
#define EAFNOSUPPORT    WSAEAFNOSUPPORT
#define qerrno          WSAGetLastError()
#else //_WIN32

//
// OS specific includes.
//

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __sun__
#include <sys/filio.h>
#endif //__sun__

#ifdef NeXT
#include <libc.h>
#endif //NeXT

//
// OS specific types definition.
//

typedef int socket_t;

//
// OS specific definitions.
//

#define qerrno 			errno

#define closesocket close
#define ioctlsocket ioctl

#endif //_WIN32

//
// common definitions.
//

// winsock2.h defines INVALID_SOCKET as (SOCKET)(~0)
// SOCKET is 64 bits on x86_64 while int may still be 32 bit wide
#if defined _WIN32 && defined INVALID_SOCKET
#undef INVALID_SOCKET
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  -1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR    -1
#endif

#define PORT_ANY ((unsigned short int)0xFFFF)

typedef enum {NA_INVALID, NA_LOOPBACK, NA_IP} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct {
	netadrtype_t    type;

	byte            ip[4];

	unsigned short  port;
} netadr_t;

extern	netadr_t	net_local_sv_ipadr;
extern	netadr_t	net_local_sv_tcpipadr;
extern	netadr_t	net_local_cl_ipadr;

extern	netadr_t	net_from; // address of who sent the packet
extern	sizebuf_t	net_message;

#define MAX_UDP_PACKET (MAX_MSGLEN*2) // one more than msg + header

// convert netadrt_t to sockaddr_storage.
void	NetadrToSockadr (const netadr_t *a, struct sockaddr_storage *s);
// convert sockaddr_storage to netadrt_t.
void	SockadrToNetadr (const struct sockaddr_storage *s, netadr_t *a);

// compare netart_t.
qbool	NET_CompareAdr (const netadr_t a, const netadr_t b);
// compare netart_t, ignore port.
qbool	NET_CompareBaseAdr (const netadr_t a, const netadr_t b);
// print netadr_t as string, xxx.xxx.xxx.xxx:xxxxx notation.
char	*NET_AdrToString (const netadr_t a);
// print netadr_t as string, port skipped, xxx.xxx.xxx.xxx notation.
char	*NET_BaseAdrToString (const netadr_t a);
// convert/resolve IP/DNS to netadr_t.
qbool	NET_StringToAdr (const char *s, netadr_t *a);

void	NET_Init (void);
void	NET_Shutdown (void);
void	NET_InitClient (void);
void	NET_InitServer (void);
void	NET_CloseServer (void);
qbool	NET_GetPacket (netsrc_t sock);
void	NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to);

void	NET_GetLocalAddress (int socket, netadr_t *out);

void	NET_ClearLoopback (void);
qbool	NET_Sleep(int msec, qbool stdinissocket);

// GETER: return port of UDP server socket.
int		NET_UDPSVPort (void);

// GETER: return client/server UDP/TCP socket.
int		NET_GetSocket(netsrc_t netsrc, qbool tcp);

// open server TCP socket.
void	NET_InitServer_TCP(unsigned short int port);

// UTILITY: set KEEPALIVE option on TCP socket (useful for faster timeout detection).
qbool 	TCP_Set_KEEPALIVE(int sock);
// UTILITY: open TCP socket for remove address (useful for client connection).
int		TCP_OpenStream (netadr_t remoteaddr);
// UTILITY: open TCP listen socket (useful for server).
int		TCP_OpenListenSocket (unsigned short int port);
// UTILITY: open UDP listen socket (useful for server).
int		UDP_OpenSocket (unsigned short int port);
//============================================================================

//
// netchan related.
//

#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct {
	netsrc_t	sock;

	qbool		fatal_error;

	int			dropped;			// between last packet and previous

	float		last_received;		// for timeouts

	// the statistics are cleared at each client begin, because
	// the server connecting process gives a bogus picture of the data
	float		frame_latency;		// rolling average
	float		frame_rate;

	int			drop_count;			// dropped packets, cleared each level
	int			good_count;			// cleared each level

	netadr_t	remote_address;
	int			qport;

	// bandwidth estimator
	double		cleartime;			// if curtime > nc->cleartime, free to go
	double		rate;				// seconds / byte

	int         dupe;               // duplicate packets to send (0 = no duplicates, as normal)

	// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged; // single bit

	int			incoming_reliable_sequence; // single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;	// single bit
	int			last_reliable_sequence; // sequence number of last send

	// reliable staging and holding areas
	sizebuf_t	message;			// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN];

	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN]; // unacked reliable message

	// time and size data to calculate bandwidth
	int			outgoing_size[MAX_LATENT];
	double		outgoing_time[MAX_LATENT];
} netchan_t;

void Netchan_Init (void);
void Netchan_Transmit (netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand (netsrc_t sock, netadr_t adr, int length, byte *data);
void Netchan_OutOfBandPrint (netsrc_t sock, netadr_t adr, char *format, ...);
qbool Netchan_Process (netchan_t *chan);
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport, int mtu);

qbool Netchan_CanPacket (netchan_t *chan);
qbool Netchan_CanReliable (netchan_t *chan);

typedef enum {
	PORTPINGPROBE_READY,
	PORTPINGPROBE_PROBING,
	PORTPINGPROBE_COMPLETED,
	PORTPINGPROBE_ABORT
} portpingprobe_status_t;

qbool IsPortPingProbeEnabled(void);
void NET_PortPingProbe(const char *target_addr, const char *original_addr);
void NET_SetPortPingProbeStatus(const portpingprobe_status_t status);
portpingprobe_status_t NET_GetPortPingProbeStatus(void);
int NET_GetPortPingProbeProgress(void);

#endif /* !__NET_H__ */
