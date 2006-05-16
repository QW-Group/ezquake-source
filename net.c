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

    $Id: net.c,v 1.3 2006-05-16 12:59:20 disconn3ct Exp $
*/

#include "quakedef.h"
#ifdef _WIN32
#include "winquake.h"
#endif

netadr_t	net_local_adr;

netadr_t	net_from;
sizebuf_t	net_message;
byte		net_message_buffer[MSG_BUF_SIZE];

#define	MAX_LOOPBACK	4	// must be a power of two

typedef struct {
	byte	data[MAX_UDP_PACKET];
	int		datalen;
} loopmsg_t;

typedef struct {
	loopmsg_t	msgs[MAX_LOOPBACK];
	unsigned int	get, send;
} loopback_t;

#ifdef _WIN32
WSADATA		winsockdata;
#endif

loopback_t	loopbacks[2];
int			ip_sockets[2] = { -1, -1 };

//=============================================================================

void NetadrToSockadr (netadr_t *a, struct sockaddr_in *s) {
	memset (s, 0, sizeof(*s));
	s->sin_family = AF_INET;

	*(int *)&s->sin_addr = *(int *)&a->ip;
	s->sin_port = a->port;
}

void SockadrToNetadr (struct sockaddr_in *s, netadr_t *a) {
	a->type = NA_IP;
	*(int *)&a->ip = *(int *)&s->sin_addr;
	a->port = s->sin_port;
}

qbool NET_CompareBaseAdr (netadr_t a, netadr_t b) {
	if (a.type == NA_LOOPBACK && b.type == NA_LOOPBACK)
		return true;
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3])
		return true;
	return false;
}

qbool NET_CompareAdr (netadr_t a, netadr_t b) {
	if (a.type == NA_LOOPBACK && b.type == NA_LOOPBACK)
		return true;
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

qbool NET_IsLocalAddress (netadr_t a) {
	if ((*(unsigned *)a.ip == *(unsigned *)net_local_adr.ip || *(unsigned *)a.ip == htonl(INADDR_LOOPBACK)) )
		return true;
	
	return false;
}

char *NET_AdrToString (netadr_t a) {
	static char s[64];

	if (a.type == NA_LOOPBACK)
		return "loopback";

	sprintf (s, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], ntohs(a.port));
	return s;
}

char *NET_BaseAdrToString (netadr_t a) {
	static char s[64];
	
	sprintf (s, "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s;
}

/*
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
*/
qbool NET_StringToAdr (char *s, netadr_t *a) {
	struct hostent	*h;
	struct sockaddr_in sadr;
	char *colon, copy[128];

	if (!strcmp(s, "local")) {
		memset(a, 0, sizeof(*a));
		a->type = NA_LOOPBACK;
		return true;
	}

	memset (&sadr, 0, sizeof(sadr));
	sadr.sin_family = AF_INET;

	sadr.sin_port = 0;

	strcpy (copy, s);
	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++) {
		if (*colon == ':') {
			*colon = 0;
			sadr.sin_port = htons((short)atoi(colon+1));
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9') {
		*(int *)&sadr.sin_addr = inet_addr(copy);
	} else {
		if ((h = gethostbyname(copy)) == 0)
			return 0;
		*(int *)&sadr.sin_addr = *(int *)h->h_addr_list[0];
	}

	SockadrToNetadr (&sadr, a);

	return true;
}

/*
=============================================================================
LOOPBACK BUFFERS FOR LOCAL PLAYER
=============================================================================
*/

qbool NET_GetLoopPacket (netsrc_t sock) {
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if ((int)(loop->send - loop->get) <= 0)
		return false;

	i = loop->get & (MAX_LOOPBACK-1);
	loop->get++;

	memcpy (net_message.data, loop->msgs[i].data, loop->msgs[i].datalen);
	net_message.cursize = loop->msgs[i].datalen;
	memset (&net_from, 0, sizeof(net_from));
	net_from.type = NA_LOOPBACK;
	return true;
}

void NET_SendLoopPacket (netsrc_t sock, int length, void *data, netadr_t to) {
	int i;
	loopback_t *loop;

	loop = &loopbacks[sock ^ 1];

	i = loop->send & (MAX_LOOPBACK - 1);
	loop->send++;

	if (length > (int) sizeof(loop->msgs[i].data))
		Sys_Error ("NET_SendLoopPacket: length > MAX_UDP_PACKET");

	memcpy (loop->msgs[i].data, data, length);
	loop->msgs[i].datalen = length;
}

//=============================================================================

void NET_ClearLoopback (void) {
	loopbacks[0].send = loopbacks[0].get = 0;
	loopbacks[1].send = loopbacks[1].get = 0;
}

qbool NET_GetPacket (netsrc_t sock) {
	int ret, net_socket;
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);

	if (NET_GetLoopPacket (sock))
		return true;

	net_socket = ip_sockets[sock];
	if (net_socket == -1)
		return false;

	ret = recvfrom (net_socket, (char *)net_message_buffer, sizeof(net_message_buffer), 0, (struct sockaddr *)&from, &fromlen);
	if (ret == -1) {
		if (qerrno == EWOULDBLOCK)
			return false;

		if (qerrno == EMSGSIZE) {
			Com_Printf ("Warning:  Oversize packet from %s\n", NET_AdrToString (net_from));
			return false;
		}

		if (qerrno == 10054) {
			Com_DPrintf ("NET_GetPacket: Error 10054 from %s\n", NET_AdrToString (net_from));
			return false;
		}

		Sys_Error ("NET_GetPacket: recvfrom: (%i): %s\n", qerrno, strerror(qerrno));
	}

	net_message.cursize = ret;
	SockadrToNetadr (&from, &net_from);
	if (ret == sizeof(net_message_buffer)) {
		Com_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

	return ret;
}

//=============================================================================

void NET_SendPacket (netsrc_t sock, int length, void *data, netadr_t to) {
	int ret;
	struct sockaddr_in	addr;
	int net_socket;

	if (to.type == NA_LOOPBACK) {
		NET_SendLoopPacket (sock, length, data, to);
		return;
	}

	net_socket = ip_sockets[sock];
	if (net_socket == -1)
		return;

	NetadrToSockadr (&to, &addr);

	ret = sendto (net_socket, data, length, 0, (struct sockaddr *)&addr, sizeof(addr) );
	if (ret == -1) {
		if (qerrno == EWOULDBLOCK)
			return;
		if (qerrno == ECONNREFUSED)
			return;
#ifdef _WIN32
#ifndef SERVERONLY
		if (qerrno == WSAEADDRNOTAVAIL)
			return;
#endif
#endif
		Sys_Printf ("NET_SendPacket: sendto: (%i): %s\n", qerrno, strerror(qerrno));
	}
}

//=============================================================================

int UDP_OpenSocket (int port) {
	int newsocket;
	struct sockaddr_in address;
	unsigned long _true = true;
	int i;

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		Sys_Error ("UDP_OpenSocket: socket: (%i): %s\n", qerrno, strerror(qerrno));

	if (ioctlsocket (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: (%i): %s\n", qerrno, strerror(qerrno));

	address.sin_family = AF_INET;

	// check for interface binding option
	if ((i = COM_CheckParm("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr(com_argv[i+1]);
		Com_Printf ("Binding to IP Interface Address of %s\n", inet_ntoa(address.sin_addr));
	} else {
		address.sin_addr.s_addr = INADDR_ANY;
	}

	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons((short)port);

	if (bind (newsocket, (void *)&address, sizeof(address)) == -1)
		return -1;

	return newsocket;
}

void NET_ClientConfig (qbool enable) {
	int port;

	if (enable) {
		if (ip_sockets[NS_CLIENT] == -1) {
			if ((port = COM_CheckParm("-clientport")) && port + 1 < com_argc) {
				ip_sockets[NS_CLIENT] = UDP_OpenSocket (Q_atoi(com_argv[port + 1]));
			} else {
				ip_sockets[NS_CLIENT] = UDP_OpenSocket (PORT_CLIENT); // try the default port first
				if (ip_sockets[NS_CLIENT] == -1)
					ip_sockets[NS_CLIENT] = UDP_OpenSocket (PORT_ANY); // any dynamic port
			}
			if (ip_sockets[NS_CLIENT] == -1)
				Sys_Error ("Couldn't allocate client socket");
		}
	} else {
		if (ip_sockets[NS_CLIENT] != -1) {
			closesocket (ip_sockets[NS_CLIENT]);
			ip_sockets[NS_CLIENT] = -1;
		}
	}
}

void NET_ServerConfig (qbool enable) {
	int i, port;

	if (enable) {
		if (ip_sockets[NS_SERVER] != -1)
			return;

		port = 0;
		i = COM_CheckParm ("-port");
		if (i && i < com_argc)
			port = atoi(com_argv[i+1]);
		if (!port)
			port = PORT_SERVER;

		ip_sockets[NS_SERVER] = UDP_OpenSocket (port);
		if (ip_sockets[NS_SERVER] == -1) {
#ifdef SERVERONLY
			Sys_Error ("Couldn't allocate server socket");
#else
			if (dedicated)
				Sys_Error ("Couldn't allocate server socket");
			else
				Com_Printf ("WARNING: Couldn't allocate server socket.\n");
#endif
		}

#ifdef _WIN32
		if (dedicated)
			SetConsoleTitle (va("ezqds: %i", port));
#endif

	} else {
		if (ip_sockets[NS_SERVER] != -1) {
			closesocket (ip_sockets[NS_SERVER]);
			ip_sockets[NS_SERVER] = -1;
		}
	}
}

void NET_Init (void) {
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

#ifdef _WIN32
	Com_Printf ("Winsock initialized\n");
#endif
}

void NET_Shutdown (void) {
	NET_ClientConfig (false);
	NET_ServerConfig (false);
#ifdef _WIN32
	WSACleanup ();
#endif
}
