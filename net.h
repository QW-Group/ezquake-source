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

    $Id: net.h,v 1.15 2007-05-03 12:03:54 johnnycz Exp $
*/
// net.h -- quake's interface to the networking layer

#ifndef __NET_H__
#define __NET_H__

#ifdef _WIN32
#include <winsock2.h>
typedef int socklen_t;
typedef SOCKET socket_t;

#define EWOULDBLOCK	WSAEWOULDBLOCK
#define EMSGSIZE	WSAEMSGSIZE
#define ECONNRESET	WSAECONNRESET
#define ECONNABORTED	WSAECONNABORTED
#define ECONNREFUSED	WSAECONNREFUSED
#define EADDRNOTAVAIL	WSAEADDRNOTAVAIL
#define EAFNOSUPPORT	WSAEAFNOSUPPORT
#define qerrno WSAGetLastError()
#else //_WIN32
#define qerrno errno

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>

#define closesocket close
#define ioctlsocket ioctl
typedef int socket_t;
#endif //_WIN32

#include <errno.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define PORT_ANY -1

typedef enum {NA_INVALID, NA_LOOPBACK, NA_IP} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct {
	netadrtype_t type;

	byte ip[4];

	unsigned short port;
} netadr_t;

extern	netadr_t	net_local_sv_ipadr;
extern	netadr_t	net_local_sv_tcpipadr;
extern	netadr_t	net_local_cl_ipadr;

extern	netadr_t	net_from; // address of who sent the packet
extern	sizebuf_t	net_message;

#define MAX_UDP_PACKET (MAX_MSGLEN*2) // one more than msg + header
extern	byte		net_message_buffer[MSG_BUF_SIZE];

extern	cvar_t	hostname;

qbool TCP_Set_KEEPALIVE(int sock);

int TCP_OpenStream (netadr_t remoteaddr); //makes things easier

void	NET_Init (void);
void	NET_InitClient (void);
void	NET_InitServer (void);
void	NET_CloseServer (void);
void	NET_Shutdown (void);
qbool	NET_GetPacket (netsrc_t sock);
void	NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to);

void	NET_ClearLoopback (void);
qbool	NET_Sleep (int msec);

qbool	NET_CompareAdr (netadr_t a, netadr_t b);
qbool	NET_CompareBaseAdr (netadr_t a, netadr_t b);
char	*NET_AdrToString (netadr_t a);
char	*NET_BaseAdrToString (netadr_t a);
qbool	NET_StringToAdr (char *s, netadr_t *a);

int		NET_UDPSVPort (void);

//============================================================================

#define OLD_AVG 0.99 // total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define MAX_LATENT 32

typedef struct {
	qbool		fatal_error;

	netsrc_t	sock;

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
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);

qbool Netchan_CanPacket (netchan_t *chan);
qbool Netchan_CanReliable (netchan_t *chan);

int  UDP_OpenSocket (int port);
void NetadrToSockadr (netadr_t *a, struct sockaddr_storage *s);
void SockadrToNetadr (struct sockaddr_storage *s, netadr_t *a);

#endif /* __NET_H__ */
