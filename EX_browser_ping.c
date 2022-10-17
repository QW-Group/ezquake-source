/*
Copyright (C) 2007 ezQuake team

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

$Id: EX_browser_ping.c,v 1.40 2007-10-27 08:51:12 dkure Exp $
*/

#ifdef _WIN32

#include <winsock2.h>
#include "quakedef.h"

#else

#include "quakedef.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>

#define SO_DONTLINGER 0 	 
#define GetCurrentProcessId getpid

#define ioctlsocket ioctl

#endif // _WIN32

#include "hud.h"
#include "hud_common.h"
#include "EX_browser.h"

// =============================================================================
//  Constants
// =============================================================================

#define ICMP_ECHO       8
#define ICMP_ECHOREPLY  0 	 
  	 
#define ICMP_MIN 8 // minimum 8 byte icmp packet (just header) 	 

#define STATUS_FAILED    0xFFFF
#define DEF_PACKET_SIZE  0
#define MAX_PACKET       1024

#define PING_PACKET_DATA "\xff\xff\xff\xffk\n"

// =============================================================================
//  Local Types
// =============================================================================

/* The IP header */
typedef struct IP_header_s
{
    unsigned int   h_len:4;        // length of the header
    unsigned int   version:4;      // Version of IP
    unsigned char  tos;            // Type of service
    unsigned short total_len;      // total length of the packet
    unsigned short ident;          // unique identifier
    unsigned short frag_and_flags; // flags
    unsigned char  ttl; 
    unsigned char  proto;          // protocol (TCP, UDP etc)
    unsigned short checksum;       // IP checksum

    unsigned int sourceIP;
    unsigned int destIP;
} IP_header_t;
 
typedef struct ICMP_header_s
{
    byte i_type;
    byte i_code; /* type sub code */
    unsigned short i_cksum;
    unsigned short i_id;
    unsigned short i_seq;
    /* This is not the std header, but we reserve space for our needs */
    double timestamp;
    u_int id;   // ip
    int index;  // server num
    int phase;  // ping phase
    int randomizer;
} ICMP_header_t; 

/* Packets */
typedef union IP_Packet_u {
	IP_header_t hdr;
	byte data[MAX_PACKET];
} IP_packet_t;

typedef union ICMP_Packet_u {
	ICMP_header_t hdr;
	byte data[MAX_PACKET];
} ICMP_packet_t;

/* Struct for storing ping reponses */
typedef struct pinghost_s
{
    int ip;
    unsigned short port;
    int recv, send;
    int phase;
	double ping;
    double stime[6];
} pinghost_t;

typedef struct {
	pinghost_t *hosts;
	int nelms;
} pinghost_list_t;

// =============================================================================
//  Function Prototypes
// =============================================================================

// =============================================================================
//  Global variables
// =============================================================================
qbool useNewPing = false; // New Ping = UDP QW Packet multithreaded ping

socket_t sock;
socket_t ping_sock;

/* Used for thread syncronisation */
static qbool ping_finished = false;
static sem_t ping_semaphore;

// =============================================================================
//  Local Functions
// =============================================================================
/**
 * Given a string of the form "ip:port", gets the addr, port in integer format
 */
static int ParseServerIp(char *server_port, int *addr, unsigned short *port) 
{
	char server_ip[50];
	char *port_divide;

	strlcpy (server_ip, server_port, sizeof(server_ip));

	/* Break the ip at the port */
	if ((port_divide = strchr(server_ip, ':')))
		*port_divide = 0;

	if (addr) {
		*addr = inet_addr(server_ip);
		if (*addr == INADDR_NONE)
			return 1;
	}

	if (port) 
	{
		if (port_divide != NULL)
			*port = (unsigned short) Q_atoi(port_divide+1);
		else
			*port = 27500;
	}

	return 0;
}

/**
 * Given a servs list, parse and create a pinghost_t list from it
 */
static pinghost_t *ParseServerList(server_data *servs[], int servsn, int *host_nelms) 
{
	pinghost_t *phosts;
	int nelms;
	int i, j;

	phosts = (pinghost_t *)Q_malloc(sizeof(pinghost_t) * servsn);
	nelms = 0;
	for (i=0; i < servsn; i++) 
	{
		int addr;
		unsigned short port;

		if (ParseServerIp(servs[i]->display.ip, &addr, &port)) 
		{
			continue;
		}

		/* Make sure it is unique */
		for (j = 0; j < nelms; j++) 
		{
			if (phosts[j].ip == addr && phosts[j].port == port) 
			{
				break;
			}
		}

		if (j == nelms) 
		{
			phosts[nelms].recv = 0;
			phosts[nelms].send = 0;
			phosts[nelms].ping = 0;
			phosts[nelms].ip = addr;
			phosts[nelms].port = port;
			nelms++;
		}
	}

	*host_nelms = nelms;
	return phosts;
}

/**
 * Given a ping host array takes the average ping and fills the display for
 * the servs array
 */
static void FillServerListPings(server_data *servs[], int servsn, 
							pinghost_t *phosts, int host_nelms) {
	int i, j;

	for (i = 0; i < host_nelms; i++) {
		int ping;
		int addr;
		unsigned short port;

		/* Take the average of the recieved pings */
		if (phosts[i].recv > 0)
			ping = (int)((phosts[i].ping / phosts[i].recv) * 1000);
		else
			ping = -1;

		/* Find the server from the host */
		for (j = 0; j < servsn; j++) 
		{
			if (ParseServerIp(servs[j]->display.ip, &addr, &port)) 
			{
				continue;
			}

			if (addr == phosts[i].ip && port == phosts[i].port) 
			{
				SetPing(servs[j], ping); // 10
			}
		}
	}
}

/**
 * Decode an IP packet and return the underlying ICMP header
 */
static ICMP_header_t *IP_DecodePacket(IP_packet_t *ip_packet, int bytes,
								struct sockaddr_in *from, u_int ip)
{
    ICMP_packet_t *icmp_packet;
    unsigned short iphdrlen;

    iphdrlen = ip_packet->hdr.h_len * 4 ; // number of 32-bit words *4 = bytes

    if (bytes  < iphdrlen + ICMP_MIN)
    {
        return NULL;
    }

    icmp_packet = (ICMP_packet_t *)(ip_packet->data + iphdrlen);

    if (icmp_packet->hdr.i_type != ICMP_ECHOREPLY)
	{
        return NULL;
    }
    if (icmp_packet->hdr.i_id != (unsigned short)GetCurrentProcessId())
	{
        return NULL;
    }

    return &icmp_packet->hdr;
} 

/** 
 * Decode an ICMP packet and calculate the total return trip time (RTT) is ms
 */
static int ICMP_GetEchoResponseTime(ICMP_header_t *icmp_hdr)
{
    return 1 + (int)((Sys_DoubleTime() - icmp_hdr->timestamp)*1000);
}

/**
 * Calculate the ICMP checksum which includes the header and data
 */
static unsigned short ICMP_Checksum(ICMP_packet_t *packet, int size)
{
  unsigned int cksum=0;
  unsigned short *buffer = (unsigned short *)packet->data;

	while(size >1)
	{
    cksum+=*buffer++;
    size -=sizeof(unsigned short);
  }
  
	if (size)
	{
    cksum += *(byte *)buffer;
  }

  cksum = (cksum >> 16) + (cksum & 0xffff);
  cksum += (cksum >>16);
  return (unsigned short)(~cksum);
}


/**
 * Helper function to fill in various stuff in our ICMP request.
 */
static void ICMP_FillData(ICMP_packet_t *packet, int datasize)
{
  packet->hdr.i_type = ICMP_ECHO;
  packet->hdr.i_code = 0;
  packet->hdr.i_id = (unsigned short)GetCurrentProcessId();
  packet->hdr.i_cksum = 0;
  packet->hdr.i_seq = 0;
  
  //
  // Place some junk in the buffer.
  //
  memset(packet->data + sizeof(packet->hdr), 'E', datasize);
}

// =============================================================================
//  Global Functions
// =============================================================================
void SB_RootInit(void)
{
    useNewPing = true;
} 

/**
 * Pings a single host up to count attempts, each attemp we send the request
 * and wait for the reply. The total return trip time (RTT) in ms is returned.
 */
int oldPingHost(char *host_to_ping, int count)
{
    struct sockaddr_in dest, from;
    int fromlen = sizeof(from);
    int bread, datasize;
    ICMP_packet_t icmp_packet;

	IP_packet_t   ip_recv_packet;
	ICMP_header_t *icmp_recv_header;

    int attempts, rtt;
    struct timeval timeout;
    fd_set fd_set_struct;

    unsigned int addr=0;
    unsigned short seq_no = 0;

    if (sock < 0)
        return 0;

    addr = inet_addr(host_to_ping);
    if (addr == INADDR_NONE)
        return 0;

    memset(&dest, 0, sizeof(dest));
    dest.sin_addr.s_addr = addr;
    dest.sin_family = AF_INET;

    memset(icmp_packet.data, 0, sizeof(icmp_packet.data));
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(icmp_packet.hdr);  
    ICMP_FillData(&icmp_packet, datasize);

    attempts = 0;
	rtt = 0;
    while (attempts < count)
    {
        int bwrote;
		int ret;

        attempts++;
    
		/* Prepare the packet */
        icmp_packet.hdr.i_cksum = 0;
        icmp_packet.hdr.timestamp = Sys_DoubleTime();
        icmp_packet.hdr.id = addr;

        icmp_packet.hdr.i_seq = seq_no++;
        icmp_packet.hdr.i_cksum = ICMP_Checksum(&icmp_packet, datasize);

		/* Prepare for response */
        FD_ZERO(&fd_set_struct);
        FD_SET(sock, &fd_set_struct);
        timeout.tv_sec = 0;
        timeout.tv_usec = sb_pingtimeout.value * 1000;

        ret = select(sock+1, NULL, (fd_set *) &fd_set_struct, NULL, &timeout);
        if (ret <= 0)
            continue;

		/* Send ping request */
        bwrote = sendto(sock, (char *) icmp_packet.data,datasize,0,
				(struct sockaddr*)&dest, sizeof(dest));
        if (bwrote <= 0 || bwrote < datasize)
            continue;

		/* Wait for response */
        FD_ZERO(&fd_set_struct);
        FD_SET(sock, &fd_set_struct);

        ret = select(sock+1, (fd_set *) &fd_set_struct, NULL, NULL, &timeout);

        if (ret <= 0)
            continue;

        bread = recvfrom(sock, (char *) ip_recv_packet.data, MAX_PACKET, 0,
				(struct sockaddr*)&from, (socklen_t *)&fromlen);
        if (bread <= 0)
            continue;

		/* Look for the RTT */
		icmp_recv_header = IP_DecodePacket(&ip_recv_packet, bread, &from, addr);
		if (icmp_recv_header) {
			if (!(rtt = ICMP_GetEchoResponseTime(icmp_recv_header))) {
				continue;
			}
		}

        break;
    }

    return rtt;
} 

/**
 * Ping multiple hosts listed by servs, ping each host count times
 */
int oldPingHosts(server_data *servs[], int servsn, int count)
{
    double interval;
    double lastsenttime;
    int ping_number;
    int randomizer;

	pinghost_list_t host_list;
    struct sockaddr_in dest,from;
    int bread,datasize;
    int fromlen = sizeof(from);
    ICMP_packet_t icmp_packet;
	IP_packet_t   ip_packet;

    int success;
    struct timeval timeout;
    fd_set fd_set_struct;

    unsigned int addr=0;
    unsigned short seq_no = 0;

    if (sock < 0)
        return 0;
	
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(icmp_packet.hdr);

	// FIXME: The datasize only sizeof(IcmpHeader) here so fill_icmp_data won't set any of the data "to junk", it will all be 0.
    memset(icmp_packet.data, 0, MAX_PACKET);
    ICMP_FillData(&icmp_packet, datasize);

    success = 0;
	host_list.hosts = ParseServerList(servs, servsn, &host_list.nelms);

    interval = (1000.0 / sb_pingspersec.value) / 1000;
    lastsenttime = Sys_DoubleTime() - interval;
    timeout.tv_sec = 0;
    timeout.tv_usec = (long)(interval * 1000.0 * 1000.0);
    ping_number = 0;
    randomizer = (int)(Sys_DoubleTime() * 10);

	// Ping all addresses in the host list we just created.
    while (!abort_ping)
    {
        int bwrote;
        double time = Sys_DoubleTime();

        if (time > (lastsenttime + 1.2 * (sb_pingtimeout.value / 1000)))
		{
            // No answers, no pings - finish.
            break;
		}

		// Send a ping request to the current host.
        if ((time > (lastsenttime + interval)) 
				&& (ping_number < (host_list.nelms * sb_pings.value)))
        {
            // Send next ping.
            pinghost_t * host = &host_list.hosts[ping_number % host_list.nelms];

            if (host->ping >= 0)
            {
                icmp_packet.hdr.i_cksum = 0;
                icmp_packet.hdr.timestamp = time;

                icmp_packet.hdr.id = host->ip;
                icmp_packet.hdr.index = ping_number % host_list.nelms;
                icmp_packet.hdr.phase = ping_number;
                icmp_packet.hdr.randomizer = randomizer;

                icmp_packet.hdr.i_seq = seq_no++;
                icmp_packet.hdr.i_cksum = ICMP_Checksum(&icmp_packet, datasize);

                memset(&dest, 0, sizeof(dest));
                dest.sin_addr.s_addr = host->ip;
                dest.sin_family = AF_INET;

                bwrote = sendto(sock, (char *) icmp_packet.data, datasize, 0,
								(struct sockaddr*)&dest, sizeof(dest));

                if (bwrote <= 0 || bwrote < datasize)
                    host->ping = -1;

                lastsenttime = time;
            }

            ping_number++;
            ping_pos = min(1, ping_number 
					/ (double)(host_list.nelms * sb_pings.value));
        }

		// Wait for an answer.
        while (!abort_ping)
        {
			int myvar;
            ICMP_header_t *icmp_answer;

            FD_ZERO(&fd_set_struct);
            FD_SET(sock, &fd_set_struct);

			myvar = select(sock+1, (fd_set *) &fd_set_struct, NULL, 
							NULL, &timeout);

			if (myvar <= 0)
                break;

            // there's an answer - get it!
            bread = recvfrom(sock, (char *) ip_packet.data, sizeof(ip_packet.data), 0,
							(struct sockaddr *)&from, (socklen_t *)&fromlen);
           
			if (bread <= 0)
                continue;

            icmp_answer = IP_DecodePacket(&ip_packet, bread, &from, addr);

			// Make sure the reply is ok.
            if (icmp_answer && (randomizer == icmp_answer->randomizer))
            {
                int index;
                int fromhost;

                fromhost = icmp_answer->id;
                index    = icmp_answer->index;
                if ((host_list.hosts[index].ip == fromhost)
						&& (host_list.hosts[index].ping >= 0))
                {
                    host_list.hosts[index].ping 
								= (host_list.hosts[index].ping 
									* host_list.hosts[index].phase
                                    + (Sys_DoubleTime()-icmp_answer->timestamp))
                                    / (host_list.hosts[index].phase + 1);
                    host_list.hosts[index].phase++;

					// Remove averaging that would occur in FillServerListPings
					host_list.hosts[index].recv = 1;
                }
            }
        }
    }

    // update pings in our servz
	if (!abort_ping) {
		FillServerListPings(servs, servsn, host_list.hosts, host_list.nelms);
	}

    Q_free(host_list.hosts);

    return success;
}

/**
 * Pings multiple hosts in parrallel, each host is pingged count times
 */
void PingSendParrallelMultiHosts(pinghost_t *phosts, int nelms, int count) {
	struct sockaddr_in to;
	int interval; 
	int i, j;
	int ret;
	timerresolution_session_t timer_session;
	
	memset(&timer_session, 0, sizeof(timer_session));

	count = bound(1, count, 6);
	interval = 1000.0 / sb_pingspersec.integer;

	Sys_TimerResolution_InitSession(&timer_session);
	Sys_TimerResolution_RequestMinimum(&timer_session);

	for (i = 0; i < count && !abort_ping; i++) {
		for (j = 0; j < nelms && !abort_ping; j++) {
			char *packet = PING_PACKET_DATA;

			ping_pos = min(1, (j / (double)(nelms * count))) + (i / (double)count);

			to.sin_family = AF_INET;
			to.sin_port = htons(phosts[j].port);
			to.sin_addr.s_addr = phosts[j].ip;

			ret = sendto(ping_sock, packet, strlen(packet), 0, (struct sockaddr *)&to, sizeof(struct sockaddr));

			if (ret == -1) {
				continue;
			}

			phosts[j].stime[phosts[j].send] = Sys_DoubleTime();
			phosts[j].send++;

			Sys_MSleep(interval);

		}
	}

	Sys_TimerResolution_Clear(&timer_session);
}

/**
 * Thread entry point for parrallel recving of ping responses
 */
int PingRecvProc(void *lpParameter)
{
	socklen_t inaddrlen;
	struct sockaddr_in from;
	fd_set fd;
	int k, ret;
	struct timeval timeout;
	pinghost_list_t *host_list = (pinghost_list_t *)lpParameter;

	while (!ping_finished && !abort_ping) {
		FD_ZERO(&fd);
		FD_SET(ping_sock, &fd);
		timeout.tv_sec = 0;
		timeout.tv_usec = 100 * 1000;

		ret = select(ping_sock+1, &fd, NULL, NULL, &timeout);
		if (ret <= 0) // error or timeout
			continue;

		while (1) {
			char buf[16];

			inaddrlen = sizeof(struct sockaddr_in);
			ret = recvfrom(ping_sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &inaddrlen);
			if (ret <= 0) // socket is asynch, so most likely this means there is no data to read
				break;
			if (buf[0] != 'l') // not A2A_ACK
				continue;
	
			for (k = 0; k < host_list->nelms; k++) {
				if (host_list->hosts[k].ip == (int) from.sin_addr.s_addr &&
				    host_list->hosts[k].port == ntohs(from.sin_port)) {
					host_list->hosts[k].ping += Sys_DoubleTime() 
									- host_list->hosts[k]
											.stime[host_list->hosts[k].recv];
					host_list->hosts[k].recv++;
					break;
				}
			}
		}
	}

	Sys_SemPost(&ping_semaphore);

	return 0;
}

/**
 * Ping a single host count times, returns the average of the responses
 */
int PingHost(char *host_to_ping, unsigned short port, int count, int time_out)
{
	socket_t sock;
	int i, ret, pings;
	struct sockaddr_in addr_to, addr_from;
	struct timeval timeout;
	fd_set fd;
	char *packet = "\xff\xff\xff\xffk\n", buf[16]; // A2A_PING, should return A2A_ACK
	socklen_t inaddrlen;
	double ping, t1;

	addr_to.sin_addr.s_addr = inet_addr(host_to_ping);
	if (addr_to.sin_addr.s_addr == INADDR_NONE)
		return 0;
	addr_to.sin_family = AF_INET;
	addr_to.sin_port = htons(port);

	sock = UDP_OpenSocket(PORT_ANY);
	pings = ping = 0;
	for (i = 0; i < count; i++) {
		ret = sendto(sock, packet, strlen(packet), 0, (struct sockaddr *)&addr_to, sizeof(struct sockaddr));
		if (ret == -1) // failure, try again
			continue;
		t1 = Sys_DoubleTime();

_select:
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000 * time_out;
		ret = select(sock+1, &fd, NULL, NULL, &timeout);
		if (ret <= 0) // timed out or error
			continue;

		inaddrlen = sizeof(struct sockaddr_in);
		ret = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr_from, &inaddrlen);
		if (ret == -1) // failure, try again
			continue;
		if (addr_from.sin_addr.s_addr != addr_to.sin_addr.s_addr) // martian, discard and see if a valid response came in after it
			goto _select;
		if (buf[0] != 'l') // not A2A_ACK, discard and see if a valid response came in after it
			goto _select;

		ping += Sys_DoubleTime() - t1;
		pings++;
	}

	closesocket(sock);
	return pings ? (int)((ping * 1000) / pings) : 0;
}

/**
 * Send several ping requests at once, start another thread to recieve
 * the responses.
 * FIXME: error handling is in rly bad shape
 */
int PingHosts(server_data *servs[], int servsn, int count)
{
	u_int arg;
	pinghost_list_t host_list;

	host_list.hosts = ParseServerList(servs, servsn, &host_list.nelms);
	ping_finished = false;

	ping_sock = UDP_OpenSocket(PORT_ANY);

#ifndef _WIN32
	if ((fcntl (ping_sock, F_SETFL, O_NONBLOCK)) == -1) { // O'Rly?! @@@
		Com_Printf ("TCP_OpenStream: fcntl: (%i): %s\n", qerrno, strerror(qerrno));
		//closesocket(ping_sock);
	}
#endif

	arg = 1;
	if (ioctlsocket (ping_sock, FIONBIO, (u_long *)&arg) == -1) { // make asynchronous
		Com_Printf ("PingHosts: ioctl: (%i): %s\n", qerrno, strerror(qerrno));
		//closesocket(newsocket);
	}

	Sys_SemInit(&ping_semaphore, 0, 1);

	if (Sys_CreateDetachedThread(PingRecvProc, (void *)&host_list) < 0) {
		Com_Printf("Failed to create Ping receiver thread\n");
	}

	PingSendParrallelMultiHosts(host_list.hosts, host_list.nelms, count);

	Sys_MSleep(500); // Wait for slow pings

	ping_finished = true; // let thread know we are done
	Sys_SemWait(&ping_semaphore);
	Sys_SemDestroy(&ping_semaphore);
	closesocket(ping_sock);

	if (!abort_ping) {
		FillServerListPings(servs, servsn, host_list.hosts, host_list.nelms);
	}

	Q_free(host_list.hosts);

	return 1;
}

//
// ----------------------------------------------
//  connection test
//  netgraph + ping stats
//
//

int sb_test_starttime;
int sb_test_incoming_sequence, sb_test_outgoing_sequence;
int sb_test_packet_latency[NET_TIMINGS];
double sent_time[NET_TIMINGS];
double received_time[NET_TIMINGS];
int    sequence_latency[NET_TIMINGS];
unsigned int sb_test_addr;
int sb_test_pl;
int sb_test_min, sb_test_max, sb_test_avg, sb_test_dev;
double last_stats_time;

server_data *sb_test_server = NULL;

void SB_Test_GetPackets(void)
{
    struct sockaddr_in dest,from;
    int bread;
    int fromlen = sizeof(from);
    IP_packet_t ip_packet;
//    int bwrote;

//    u_long arg;
//    int arg2, success;
//    struct timeval timeout;
//    fd_set fd_set_struct;

    unsigned int addr=0;
//    unsigned short seq_no = 0;

    if (sock < 0)
        return;

    memset(&dest, 0, sizeof(dest));

    addr = sb_test_addr;

    if (addr == INADDR_NONE)
        return;

    dest.sin_addr.s_addr = addr;
    dest.sin_family = AF_INET;

    while (1)
    {
//        int success;
        ICMP_header_t *icmp_answer;
        int seq;

        bread = recvfrom(sock, (char *) ip_packet.data, sizeof(ip_packet.data), 0,
						(struct sockaddr*)&from, (socklen_t *) &fromlen);
        if (bread <= 0)
            break;

        icmp_answer = IP_DecodePacket(&ip_packet, bread, &from, addr);

        if (!icmp_answer || icmp_answer->phase != sb_test_starttime)
            continue;

        seq = icmp_answer->i_seq;
        if (seq > sb_test_incoming_sequence)
        {
            received_time[seq%NET_TIMINGS] = cls.realtime;
            sequence_latency[seq%NET_TIMINGS] = sb_test_outgoing_sequence - seq;
            sb_test_incoming_sequence = seq;
        }
    }

}

void SB_Test_SendPacket(void)
{
    struct sockaddr_in dest/*,from*/;
//    int bread,
    int datasize;
//   int fromlen = sizeof(from);
	ICMP_packet_t icmp_packet;
//    int ret;

    //u_long arg;
    //int arg2, success;
    //struct timeval timeout;
    //fd_set fd_set_struct;

    unsigned int addr=0;
//    unsigned short seq_no = 0;

    sent_time[sb_test_outgoing_sequence%NET_TIMINGS] = cls.realtime;
    received_time[sb_test_outgoing_sequence%NET_TIMINGS] = -1;  // no answer yet

    if (sock < 0)
        return ;

    memset(&dest, 0, sizeof(dest));

    addr = sb_test_addr;

    if (addr == INADDR_NONE)
        return;

    dest.sin_addr.s_addr = addr;
    dest.sin_family = AF_INET;
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(icmp_packet.hdr);  

    memset(icmp_packet.data, 0, MAX_PACKET);
    ICMP_FillData(&icmp_packet, datasize);

    icmp_packet.hdr.phase = sb_test_starttime;

    icmp_packet.hdr.i_cksum = 0;
    icmp_packet.hdr.timestamp = cls.realtime;
    icmp_packet.hdr.id = addr;

    icmp_packet.hdr.i_seq = sb_test_outgoing_sequence++;
    icmp_packet.hdr.i_cksum = ICMP_Checksum(&icmp_packet, datasize);

    sendto(sock, (char *) icmp_packet.data,datasize,0,(struct sockaddr*)&dest,
                    sizeof(dest));
}

void SB_Test_CalcNet(void)
{
    int i, a;

    extern hud_t * hud_netgraph;
    cvar_t *netgraph_inframes = HUD_FindVar(hud_netgraph, "inframes");

    for (i=0; i < NET_TIMINGS; i++)
    {
        if (received_time[i] == -1)
            sb_test_packet_latency[i] = 9999;   // dropped
        else
        {
            double l;
            if (netgraph_inframes->value)      // [frames]
                l = 2*(sequence_latency[i]);
            else                                // [miliseconds]
                l = min((received_time[i] - sent_time[i])*1000, 1000);

            sb_test_packet_latency[i] = (int)l;
        }
    }

    if (last_stats_time +1 < cls.realtime)
    {
        int count;
        double sum;
        double avg;
        int i;

        last_stats_time = cls.realtime;

        // recalculate statistics
        sb_test_min = 9999999;
        sb_test_max = 0;
        sum = 0;
        count = 0;

        for (i=sb_test_outgoing_sequence-NET_TIMINGS; i < sb_test_incoming_sequence; i++)
        {
            int a = i % NET_TIMINGS;
            double ping;

            if (i < 0)
                continue;

            if (received_time[a] == -1)
                continue;

            ping = (received_time[a] - sent_time[a]) * 1000;

            if (ping < sb_test_min)
                sb_test_min = ping;

            if (ping > sb_test_max)
                sb_test_max = ping;

            sum += ping;
            count++;
        }

        if (count > 0)
        {
            avg = (sum / count);
            sb_test_avg = (int)(avg + 0.5);

            // standard deviation
            sum = 0;
            for (i=sb_test_outgoing_sequence-NET_TIMINGS; i < sb_test_incoming_sequence; i++)
            {
                int a = i % NET_TIMINGS;
                double ping;

                if (i < 0)
                    continue;

                if (received_time[a] == -1)
                    continue;

                ping = (received_time[a] - sent_time[a]) * 1000;

                sum += (ping - avg) * (ping - avg);
            }
            sb_test_dev = (int)(sqrt(sum / count) + 0.5);
        }
        else
            sb_test_min = sb_test_max = sb_test_avg = sb_test_dev = 0;

        // pl
        sb_test_pl = 0;
        for (a=0 ; a<NET_TIMINGS ; a++)
        {
            i = (sb_test_outgoing_sequence-a) & NET_TIMINGSMASK;
            if (sb_test_packet_latency[i] == 9999)
                sb_test_pl++;
        }
        sb_test_pl =  sb_test_pl * 100 / NET_TIMINGS;
    }

    sb_test_min = min(sb_test_min, 999);
    sb_test_max = min(sb_test_max, 999);
    sb_test_avg = min(sb_test_avg, 999);
    sb_test_dev = min(sb_test_dev, 99);
}

void SB_Test_Draw(void)
{
    int x, y, w, h;

	extern void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency, int lost, int minping, int avgping, int maxping, int devping, int posx, int posy, int width, int height, int revx, int revy);

    w = 240;
    h = 112;

    x = (vid.width - w)/2;
    y = (vid.height - h)/2;
    x -= 8;
    y += 76;

    if (vid.height >= 240)
        y += 20;

    R_MQW_NetGraph(sb_test_outgoing_sequence, sb_test_incoming_sequence,
               sb_test_packet_latency, sb_test_pl,
               sb_test_min, sb_test_avg, sb_test_max, sb_test_dev,
               x, y, -1, -1, 0, 0);
}

void SB_Test_Init(char *address)
{
    int i;

    sb_test_incoming_sequence = 0;
    sb_test_outgoing_sequence = 0;

    sb_test_addr = inet_addr(address);

    for (i=0; i < NET_TIMINGS; i++)
    {
        sb_test_packet_latency[i] = 0;
        received_time[i] = 0;
        sent_time[i] = 0;
        sequence_latency[i] = 0;
    }

    last_stats_time = -999;
    sb_test_starttime = cls.realtime*1000;
}

void SB_Test_Change(char *address)
{
    unsigned int addr = inet_addr(address);

    if (addr != sb_test_addr)
    {
        sb_test_incoming_sequence = sb_test_outgoing_sequence;
        sb_test_addr = inet_addr(address);

        last_stats_time = -999;
        sb_test_starttime = cls.realtime*1000;
    }
}

void SB_Test_Frame(void)
{
    // receive answers
    SB_Test_GetPackets();

    // send command
    SB_Test_SendPacket();

    // update stats
    SB_Test_CalcNet();

    // draw statistics
    SB_Test_Draw();
}
