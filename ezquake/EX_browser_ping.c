#ifdef _WIN32

#include <winsock2.h>
#include "quakedef.h"
#include "winquake.h"

typedef unsigned int socklen_t;

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
#include <sys/unistd.h>

#define SO_DONTLINGER 0 	 
#define USHORT unsigned short 	 
#define BYTE   byte 	 
#define UCHAR  unsigned char 	 
#define GetCurrentProcessId getpid

#define ioctlsocket ioctl

#endif

#include "EX_browser.h"

#define ICMP_ECHO       8
#define ICMP_ECHOREPLY  0 	 
  	 
#define ICMP_MIN 8 // minimum 8 byte icmp packet (just header) 	 

qboolean useNewPing = false;

int sock;

void SB_RootInit(void)
{
    int arg;
/*
    if (COM_CheckParm("-nosockraw"))
    {
        sock = -1;
        return;
    }
*/
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

	if (sock < 0 || COM_CheckParm("-nosockraw")) {
		useNewPing = true;
		return;
	}

    arg = 1;
    ioctlsocket(sock, FIONBIO, &arg);  // make asynchronous

    arg = 1;
    setsockopt(sock, SOL_SOCKET, SO_DONTLINGER, (char*)&arg, sizeof(int));

} 

/* The IP header */
typedef struct iphdr
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
} IpHeader;
 
typedef struct _ihdr
{
    BYTE i_type;
    BYTE i_code; /* type sub code */
    USHORT i_cksum;
    USHORT i_id;
    USHORT i_seq;
    /* This is not the std header, but we reserve space for our needs */
    double timestamp;
    u_long id;   // ip
    int index;  // server num
    int phase;  // ping phase
    int randomizer;
} IcmpHeader; 

#define STATUS_FAILED    0xFFFF
#define DEF_PACKET_SIZE  0
#define MAX_PACKET       1024

void    fill_icmp_data (char *, int);
USHORT  checksum (USHORT *, int);
int     decode_resp (char *, int, struct sockaddr_in *, u_long ip);
IcmpHeader * get_icmp (char *, int, struct sockaddr_in *, u_long ip); 

int PingHost(char *host_to_ping, short port, int count, int time_out)
{
	int sock, i, ret, pings;
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

int oldPingHost(char *host_to_ping, int count)
{
//    int sock;
    struct sockaddr_in dest,from;
    int bread,datasize;
    int fromlen = sizeof(from);
    char *dest_ip;
    char *icmp_data;
    char *recvbuf;

//    u_long arg;
    int arg2, success;
    struct timeval timeout;
    fd_set fd_set_struct;

    unsigned int addr=0;
    USHORT seq_no = 0;

    if (sock < 0)
        return 0;

    memset(&dest, 0, sizeof(dest));

    addr = inet_addr(host_to_ping);

    if (addr == INADDR_NONE)
        return 0;

    dest.sin_addr.s_addr = addr;
    dest.sin_family = AF_INET;
    dest_ip = inet_ntoa(dest.sin_addr);
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(IcmpHeader);  

    icmp_data = Q_malloc(MAX_PACKET);
    recvbuf = Q_malloc(MAX_PACKET);

    if (!icmp_data)
        return 0;
  
    memset(icmp_data, 0, MAX_PACKET);
    fill_icmp_data(icmp_data, datasize);

    arg2 = success = 0;
    while (arg2 < count)
    {
        int bwrote;
    int ret;

        arg2++;
    
        ((IcmpHeader*)icmp_data)->i_cksum = 0;
        ((IcmpHeader*)icmp_data)->timestamp = Sys_DoubleTime();
        ((IcmpHeader*)icmp_data)->id = addr;

        ((IcmpHeader*)icmp_data)->i_seq = seq_no++;
        ((IcmpHeader*)icmp_data)->i_cksum = checksum((USHORT*)icmp_data, 
                                                     datasize);

        FD_ZERO(&fd_set_struct);
        FD_SET(sock, &fd_set_struct);
        timeout.tv_sec = 0;
        timeout.tv_usec = sb_pingtimeout.value * 1000;

        ret = select(sock+1, NULL, (fd_set *) &fd_set_struct, NULL, &timeout);

        if (ret <= 0)
            continue;

        bwrote = sendto(sock,icmp_data,datasize,0,(struct sockaddr*)&dest,
                        sizeof(dest));

        if (bwrote <= 0)
            continue;

        if (bwrote < datasize )
            continue;
    
        FD_ZERO(&fd_set_struct);
        FD_SET(sock, &fd_set_struct);

        ret = select(sock+1, (fd_set *) &fd_set_struct, NULL, NULL, &timeout);

        if (ret <= 0)
            continue;

        bread = recvfrom(sock, recvbuf, MAX_PACKET, 0, (struct sockaddr*)&from,
                         &fromlen);
        if (bread <= 0)
            continue;

        success = decode_resp(recvbuf, bread, &from, addr);
        if (!success)
            continue;

        break;
    }

    free(icmp_data);
    free(recvbuf);

    return success;
} 

//
// pings multiple hosts simulnateously
//

typedef struct pinghost_s
{
    u_long ip;
    short port;
    int recv, send;
    int phase;
	double ping;
    double stime[6];
} pinghost;

static int ping_sock, hostsn;
static pinghost *hosts;

DWORD WINAPI PingRecvProc(void *lpParameter)
{
	socklen_t inaddrlen;
	struct sockaddr_in from;
	fd_set fd;
	int k, ret;
	struct timeval timeout;

	while (ping_sock != -1) {
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
	
			for (k = 0; k < hostsn; k++) {
				if (hosts[k].ip == from.sin_addr.s_addr &&
				    hosts[k].port == ntohs(from.sin_port)) {
					hosts[k].ping += Sys_DoubleTime() - hosts[k].stime[hosts[k].recv];
					hosts[k].recv++;
					break;
				}
			}
		}
	}

	return 0;
}

int oldPingHosts(server_data *servs[], int servsn, int count)
{
    double interval;
    double lastsenttime;
    int ping_number;
    int randomizer;

    pinghost *hosts;
    int i, hostsn;
    struct sockaddr_in dest,from;
    int bread,datasize;
    int fromlen = sizeof(from);
    char *dest_ip;
    char *icmp_data;
    char *recvbuf;

//    u_long arg;
    int arg2, success;
    struct timeval timeout;
    fd_set fd_set_struct;

    unsigned int addr=0;
    USHORT seq_no = 0;

    if (sock < 0)
        return 0;

    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(IcmpHeader);  

    icmp_data = Q_malloc(MAX_PACKET);
    recvbuf = Q_malloc(MAX_PACKET);

    if (!icmp_data)
    {
        return 0;
    }
  
    memset(icmp_data, 0, MAX_PACKET);
    fill_icmp_data(icmp_data, datasize);

    arg2 = success = 0;

    hosts = (pinghost *) Q_malloc(servsn * sizeof(pinghost));
    hostsn = 0;
    for (i=0; i < servsn; i++)
    {
        char buf[50], *tmp;
        int j;
        unsigned int addr;

        strcpy(buf, servs[i]->display.ip);
        tmp = strchr(buf, ':');
        if (tmp != NULL)
            *tmp = 0;
        addr = inet_addr(buf);
        if (addr == INADDR_NONE)
            continue;

        for (j=0; j < hostsn; j++)
            if (hosts[j].ip == addr)
                break;

        if (j == hostsn)
        {
            hosts[hostsn].phase = 0;
            hosts[hostsn].ping = 0;
            hosts[hostsn].ip = addr;
            hostsn++;
        }
    }

    interval = (1000.0 / sb_pingspersec.value) / 1000;
    lastsenttime = Sys_DoubleTime() - interval;
    timeout.tv_sec = 0;
    timeout.tv_usec = (long)(interval * 1000.0 * 1000.0);
    ping_number = 0;
    randomizer = (int)(Sys_DoubleTime() * 10);

    while (1  &&  !abort_ping)
    {
        int bwrote;

        double time = Sys_DoubleTime();

        if (time > lastsenttime + 1.2 * (sb_pingtimeout.value / 1000))
            // no answers, no pings - finish
            break;

        if (time > lastsenttime + interval  &&  ping_number < hostsn * sb_pings.value)
        {
            // send next ping
            pinghost * host = &hosts[ping_number % hostsn];

            if (host->ping >= 0)
            {
                ((IcmpHeader*)icmp_data)->i_cksum = 0;
                ((IcmpHeader*)icmp_data)->timestamp = time;

                ((IcmpHeader*)icmp_data)->id = host->ip;
                ((IcmpHeader*)icmp_data)->index = ping_number % hostsn;
                ((IcmpHeader*)icmp_data)->phase = ping_number;
                ((IcmpHeader*)icmp_data)->randomizer = randomizer;

                ((IcmpHeader*)icmp_data)->i_seq = seq_no++;
                ((IcmpHeader*)icmp_data)->i_cksum = checksum((USHORT*)icmp_data, 
                                                             datasize);

                memset(&dest, 0, sizeof(dest));
                dest.sin_addr.s_addr = host->ip;
                dest.sin_family = AF_INET;
                dest_ip = inet_ntoa(dest.sin_addr);

                bwrote = sendto(sock,icmp_data,datasize,0,(struct sockaddr*)&dest,
                                sizeof(dest));

                if (bwrote <= 0)
                    host->ping = -1;

                if (bwrote < datasize )
                    host->ping = -1;

                lastsenttime = time;
            }
            ping_number++;
            ping_pos = min(1, ping_number / (double)(hostsn * sb_pings.value));
        }

        while (1  &&  !abort_ping)
        {
			int myvar;
            IcmpHeader *icmpanswer;

            FD_ZERO(&fd_set_struct);
            FD_SET(sock, &fd_set_struct);

			myvar = select(sock+1, (fd_set *) &fd_set_struct, NULL, NULL, &timeout);

			if (myvar <= 0)
                break;

            // there's an answer - get it!
            bread = recvfrom(sock, recvbuf, MAX_PACKET, 0, (struct sockaddr*)&from,
                             &fromlen);
            if (bread <= 0)
                continue;

            icmpanswer = get_icmp(recvbuf, bread, &from, addr);
            if (icmpanswer  &&  randomizer == icmpanswer->randomizer)
            {
                int index, phase;
                unsigned int fromhost;
                fromhost = icmpanswer->id;
                index = icmpanswer->index;
                phase = icmpanswer->phase;
                if (hosts[index].ip == fromhost  &&  hosts[index].ping >= 0)
                {
                    hosts[index].ping = (hosts[index].ping * hosts[index].phase
                                      + (Sys_DoubleTime()-icmpanswer->timestamp))
                                      / (hosts[index].phase + 1);
                    hosts[index].phase ++;
                }
            }
        }
    }

    // update pings in our servz

    if (!abort_ping)
    {
        for (i=0; i < hostsn; i++)
        {
            char *ip;
            struct in_addr addr;
            int ping, j;

            addr.s_addr = hosts[i].ip;
            ip = inet_ntoa(addr);
        
            if (hosts[i].phase > 0)
                ping = (int)(hosts[i].ping * 1000);
            else
                ping = -1;

            for (j=0; j < servsn; j++)
				if (!strncmp(ip, servs[j]->display.ip, strlen(ip))) {
                        SetPing(servs[j], ping); // 10
				}
        }
    }

    free(icmp_data);
    free(recvbuf);
    free(hosts);

    return success;
    
}


// 
// The response is an IP packet. We must decode the IP header to locate 
// the ICMP data 
//

int decode_resp(char *buf, int bytes,struct sockaddr_in *from, u_long ip)
{
    IpHeader *iphdr;
    IcmpHeader *icmphdr;
    unsigned short iphdrlen;

    iphdr = (IpHeader *)buf;

    iphdrlen = iphdr->h_len * 4 ; // number of 32-bit words *4 = bytes

    if (bytes  < iphdrlen + ICMP_MIN)
    {
        return 0;
    }

    icmphdr = (IcmpHeader*)(buf + iphdrlen);

    if (icmphdr->i_type != ICMP_ECHOREPLY) {
        return 0;
    }
    if (icmphdr->i_id != (USHORT)GetCurrentProcessId()) {
        return 0;
    }

    if (icmphdr->id != ip)
        return 0;

    return 1 + (int)((Sys_DoubleTime()-icmphdr->timestamp)*1000);
}

IcmpHeader * get_icmp(char *buf, int bytes,struct sockaddr_in *from, u_long ip)
{
    IpHeader *iphdr;
    IcmpHeader *icmphdr;
    unsigned short iphdrlen;

    iphdr = (IpHeader *)buf;

    iphdrlen = iphdr->h_len * 4 ; // number of 32-bit words *4 = bytes

    if (bytes  < iphdrlen + ICMP_MIN)
    {
        return NULL;
    }

    icmphdr = (IcmpHeader*)(buf + iphdrlen);

    if (icmphdr->i_type != ICMP_ECHOREPLY)
	{
        return NULL;
    }
    if (icmphdr->i_id != (USHORT)GetCurrentProcessId())
	{
        return NULL;
    }

    return icmphdr;
} 

USHORT checksum(USHORT *buffer, int size)
{
  unsigned long cksum=0;

	while(size >1)
	{
    cksum+=*buffer++;
    size -=sizeof(USHORT);
  }
  
	if (size)
	{
    cksum += *(UCHAR*)buffer;
  }

  cksum = (cksum >> 16) + (cksum & 0xffff);
  cksum += (cksum >>16);
  return (USHORT)(~cksum);
}


// 
// Helper function to fill in various stuff in our ICMP request.
//

void fill_icmp_data(char * icmp_data, int datasize)
{

  IcmpHeader *icmp_hdr;
  char *datapart;

  icmp_hdr = (IcmpHeader*)icmp_data;

  icmp_hdr->i_type = ICMP_ECHO;
  icmp_hdr->i_code = 0;
  icmp_hdr->i_id = (USHORT)GetCurrentProcessId();
  icmp_hdr->i_cksum = 0;
  icmp_hdr->i_seq = 0;
  
  datapart = icmp_data + sizeof(IcmpHeader);
  //
  // Place some junk in the buffer.
  //
  memset(datapart,'E', datasize - sizeof(IcmpHeader));

}
 
int PingHosts(server_data *servs[], int servsn, int count, int time_out)
{
	int i, j, ret, arg;
	double interval;
	struct sockaddr_in to;

	hosts = (pinghost *)Q_malloc(sizeof(pinghost) * servsn);

	hostsn = 0;
	for (i=0; i < servsn; i++) {
	        char buf[50], *tmp;
	        unsigned int addr;
		short port;

	        strcpy(buf, servs[i]->display.ip);
	        tmp = strchr(buf, ':');
	        if (tmp != NULL)
	            *tmp = 0;
	        addr = inet_addr(buf);
	        if (addr == INADDR_NONE)
        	    continue;

		if (tmp != NULL) {
			port = Q_atoi(tmp+1);
			*tmp = ':';
		} else
			port = 27500;

	        for (j=0; j < hostsn; j++)
	            if (hosts[j].ip == addr && hosts[j].port == port)
			break;

		if (j == hostsn) {
			hosts[hostsn].recv = 0;
			hosts[hostsn].send = 0;
			hosts[hostsn].ping = 0;
			hosts[hostsn].ip = addr;
			hosts[hostsn].port = port;
			hostsn++;
		}
	}

	ping_sock = UDP_OpenSocket(PORT_ANY);
	arg = 1;
	ioctlsocket(ping_sock, FIONBIO, &arg); // make asynchronous
	Sys_CreateThread(PingRecvProc, NULL);

	interval = 1000.0 / sb_pingspersec.value;
	count = (int)bound(1, count, 6);

	for (i = 0; i < count && !abort_ping; i++) {
		for (j = 0; j < hostsn && !abort_ping; j++) {
			char *packet = "\xff\xff\xff\xffk\n";

			ping_pos = min(1, (j / (double)(hostsn * count))) + (i / (double)count);

			to.sin_family = AF_INET;
			to.sin_port = htons(hosts[j].port);
			to.sin_addr.s_addr = hosts[j].ip;

			ret = sendto(ping_sock, packet, strlen(packet), 0, (struct sockaddr *)&to, sizeof(struct sockaddr));
			Sys_MSleep(interval);
			if (ret == -1) // error
				continue;

			hosts[j].stime[hosts[j].send] = Sys_DoubleTime();
			hosts[j].send++;
		}
	}

	closesocket(ping_sock);
	Sys_MSleep(500); // catch slow packets
	ping_sock = -1; // let thread know we are done

	if (!abort_ping) {
		for (i=0; i < hostsn; i++) {
			char buf[50], *tmp;
			unsigned int ping, addr, port;

			if (hosts[i].recv > 0)
				ping = (int)((hosts[i].ping / hosts[i].recv) * 1000);
			else
				ping = -1;

			for (j=0; j < servsn; j++) {
				strcpy(buf, servs[j]->display.ip);
				tmp = strchr(buf, ':');
				if (tmp != NULL)
					*tmp = 0;
				addr = inet_addr(buf);
				if (addr == INADDR_NONE)
					continue;

				if (tmp != NULL) {
					port = Q_atoi(tmp+1);
					*tmp = ':';
				} else
					port = 27500;

				if (addr == hosts[i].ip && port == hosts[i].port)
					SetPing(servs[j], ping); // 10
			}
		}
	}

	free(hosts);

	return 1;
}

//
//
//
// ----------------------------------------------
//  connection test
//  netgraph + ping stats
//
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

void SB_Test_SendPacket(void)
{
    struct sockaddr_in dest,from;
//    int bread,
	int datasize;
    int fromlen = sizeof(from);
    char *dest_ip;
    char *icmp_data;
    int bwrote;
//    int ret;

    //u_long arg;
    //int arg2, success;
    //struct timeval timeout;
    //fd_set fd_set_struct;

    unsigned int addr=0;
    USHORT seq_no = 0;

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
    dest_ip = inet_ntoa(dest.sin_addr);
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(IcmpHeader);  

    icmp_data = Q_malloc(MAX_PACKET);

    if (!icmp_data)
        return ;
  
    memset(icmp_data, 0, MAX_PACKET);
    fill_icmp_data(icmp_data, datasize);

    ((IcmpHeader*)icmp_data)->phase = sb_test_starttime;

    ((IcmpHeader*)icmp_data)->i_cksum = 0;
    ((IcmpHeader*)icmp_data)->timestamp = cls.realtime;
    ((IcmpHeader*)icmp_data)->id = addr;

    ((IcmpHeader*)icmp_data)->i_seq = sb_test_outgoing_sequence++;
    ((IcmpHeader*)icmp_data)->i_cksum = checksum((USHORT*)icmp_data, 
                                                 datasize);

    bwrote = sendto(sock,icmp_data,datasize,0,(struct sockaddr*)&dest,
                    sizeof(dest));

    free(icmp_data); 
}

void SB_Test_GetPackets(void)
{
    struct sockaddr_in dest,from;
    int bread,datasize;
    int fromlen = sizeof(from);
    char *dest_ip;
    char *recvbuf;
//    int bwrote;

//    u_long arg;
//    int arg2, success;
//    struct timeval timeout;
//    fd_set fd_set_struct;

    unsigned int addr=0;
    USHORT seq_no = 0;

    if (sock < 0)
        return;

    memset(&dest, 0, sizeof(dest));

    addr = sb_test_addr;

    if (addr == INADDR_NONE)
        return;

    dest.sin_addr.s_addr = addr;
    dest.sin_family = AF_INET;
    dest_ip = inet_ntoa(dest.sin_addr);
    datasize = DEF_PACKET_SIZE;
    datasize += sizeof(IcmpHeader);  

    recvbuf = Q_malloc(MAX_PACKET);


    while (1)
    {
//        int success;
        IcmpHeader *icmpanswer;
        int seq;

        bread = recvfrom(sock, recvbuf, MAX_PACKET, 0, (struct sockaddr*)&from,
                         &fromlen);
        if (bread <= 0)
            break;

        icmpanswer = get_icmp(recvbuf, bread, &from, addr);

        if (!icmpanswer)
            continue;

        if (icmpanswer->phase != sb_test_starttime)
            continue;

        seq = icmpanswer->i_seq;
        if (seq > sb_test_incoming_sequence)
        {
            received_time[seq%NET_TIMINGS] = cls.realtime;
            sequence_latency[seq%NET_TIMINGS] = sb_test_outgoing_sequence - seq;
            sb_test_incoming_sequence = seq;
        }
    }

    free(recvbuf); 
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
