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
#ifndef _NET_H_
#define _NET_H_

#ifndef _WIN32
#include <netinet/in.h>
#endif

#define PORT_ANY -1

typedef enum {NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX} netadrtype_t;

typedef enum {NS_CLIENT, NS_SERVER} netsrc_t;

typedef struct {
	netadrtype_t	type;

	byte	ip[4];
	byte	ipx[10];

	unsigned short	port;
} netadr_t;

extern	netadr_t	net_local_adr;
extern	netadr_t	net_from;		// address of who sent the packet
extern	sizebuf_t	net_message;

void	NET_Init (void);
void	NET_Shutdown (void);
void	NET_ClientConfig (qbool enable);	// open/close client socket
void	NET_ServerConfig (qbool enable);	// open/close server socket

qbool	NET_GetPacket (netsrc_t sock);
void	NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to);
void	NET_ClearLoopback (void);
void	NET_Sleep (int msec);

qbool	NET_CompareAdr (netadr_t a, netadr_t b);
qbool	NET_CompareBaseAdr (netadr_t a, netadr_t b);
qbool	NET_IsLocalAddress (netadr_t a);
char	*NET_AdrToString (netadr_t a);
char	*NET_BaseAdrToString (netadr_t a);
qbool	NET_StringToAdr (char *s, netadr_t *a);

//============================================================================

#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct {
	qbool	fatal_error;

	netsrc_t	sock;

	int		dropped;			// between last packet and previous

	float		last_received;			// for timeouts

	// the statistics are cleared at each client begin, because
	// the server connecting process gives a bogus picture of the data
	float		frame_latency;			// rolling average
	float		frame_rate;

	int		drop_count;			// dropped packets, cleared each level
	int		good_count;			// cleared each level

	netadr_t	remote_address;
	int		qport;

	// bandwidth estimator
	double		cleartime;			// if curtime > nc->cleartime, free to go
	double		rate;				// seconds / byte

	// sequencing variables
	int		incoming_sequence;
	int		incoming_acknowledged;
	int		incoming_reliable_acknowledged;	// single bit

	int		incoming_reliable_sequence;	// single bit, maintained local

	int		outgoing_sequence;
	int		reliable_sequence;		// single bit
	int		last_reliable_sequence;		// sequence number of last send

	// reliable staging and holding areas
	sizebuf_t	message;			// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN];

	int		reliable_length;
	byte		reliable_buf[MAX_MSGLEN];	// unacked reliable message

	// time and size data to calculate bandwidth
	int		outgoing_size[MAX_LATENT];
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
void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s);
void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a);

#ifndef _WIN32
#define closesocket(a) close(a)
#endif

#endif /* _NET_H_ */

