#ifdef _WIN32

#include <winsock2.h>
#include "quakedef.h"
#include "winquake.h"

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
#define ioctlsocket ioctl
#define USHORT unsigned short
#define BYTE   byte
#define UCHAR  unsigned char
#define GetCurrentProcessId getpid

#endif

#include "EX_browser.h"

#define ICMP_ECHO       8
#define ICMP_ECHOREPLY  0

#define ICMP_MIN 8 // minimum 8 byte icmp packet (just header)

int sock;

// call as root
void SB_RootInit(void)
{
    int arg;

    if (COM_CheckParm("-nosockraw"))
    {
        sock = -1;
        return;
    }

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    
    if (sock < 0)
        Sys_Error("SB_RootInit: error");

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

//
// ICMP header
//

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


int PingHost(char *host_to_ping, int count)
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

    icmp_data = malloc(MAX_PACKET);
    if (icmp_data == NULL)
        Sys_Error("Insufficient memory!");
    recvbuf = malloc(MAX_PACKET);
    if (recvbuf == NULL)
        Sys_Error("Insufficient memory!");

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
    int phase;
    double ping;
} pinghost;

int PingHosts(server_data *servs[], int servsn, int count)
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

    icmp_data = malloc(MAX_PACKET);
    if (icmp_data == NULL)
        Sys_Error("Insufficient memory!");
    recvbuf = malloc(MAX_PACKET);
    if (recvbuf == NULL)
        Sys_Error("Insufficient memory!");

    if (!icmp_data)
    {
        return 0;
    }
  
    memset(icmp_data, 0, MAX_PACKET);
    fill_icmp_data(icmp_data, datasize);

    arg2 = success = 0;

    hosts = (pinghost *) malloc(servsn * sizeof(pinghost));
    if (hosts == NULL)
        Sys_Error("Insufficient memory!");
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

    icmp_data = malloc(MAX_PACKET);
    if (icmp_data == NULL)
        Sys_Error("Insufficient memory!");

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

    recvbuf = malloc(MAX_PACKET);
    if (recvbuf == NULL)
        Sys_Error("Insufficient memory!");


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

    //extern hud_t *
    extern cvar_t r_netgraph;
    cvar_t *netgraph_inframes = &r_netgraph;

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
