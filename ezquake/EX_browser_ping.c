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

#define ioctlsocket ioctl

#endif

#include "EX_browser.h"

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

//
// pings multiple hosts simulnateously
//

typedef struct pinghost_s
{
    u_long ip;
    short port;
    int recv, send;
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

int PingHosts(server_data *servs[], int servsn, int count, int time_out)
{
	int i, j, ret, arg; // k
	double interval;
	struct sockaddr_in to;

	hosts = (pinghost *)malloc(sizeof(pinghost) * servsn);
	if (hosts == NULL)
		return 0;

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
			char *packet = "\xff\xff\xff\xffk\n"; // , buf[16];

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
}

void SB_Test_GetPackets(void)
{
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
