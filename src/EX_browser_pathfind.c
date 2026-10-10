/*
Copyright (C) 2011 johnnycz

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
	\file

	\brief
	Module for finding best connection to a server, ping-wise.

	\author johnnycz
**/

#include "quakedef.h"
#include <limits.h>
#ifndef _WIN32
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
#endif
#include "EX_browser.h"

// declared in EX_browser.c
extern cvar_t sb_proxinfopersec;
extern cvar_t sb_proxretries;
extern cvar_t sb_proxtimeout;
extern cvar_t sb_listcache;

// non-leaf = proxy (or users computer) = has more than 1 neighbour
// at the time of writing this code there were 10 active proxies around the world
#define MAX_NONLEAVES 40

#define PROXY_PINGLIST_QUERY "\xff\xff\xff\xffpingstatus"
#define PROXY_PINGLIST_QUERY_LEN (sizeof(PROXY_PINGLIST_QUERY)-1)
#define PROXY_REPLY_ENTRY_LEN 8
#define PROXY_REPLY_BUFFER_SIZE (PROXY_REPLY_ENTRY_LEN*MAX_SERVERS)

// qwfwd mesh-capable reply: OOB + 'Q''M' + type(1) + nonce(4), followed
// by ip(4), port(2), average ping(2), jitter(2), loss percent(2).
#define MESH_REPLY_HEADER_LEN 11
#define MESH_REPLY_ENTRY_LEN 12
#define MESH_MSG_PINGSTATUS_REPLY 1

// current amount of qw servers ~ 300... but neighbour count means MAX_SERVERS*MAX_NONLEAVES
#define INVALID_NODE (-1)
typedef int nodeid_t;

// only pings 0-999 are in our interest; and -1 for invalid ping
#define DIST_INFINITY SHRT_MAX
typedef short dist_t;

// trick around language limits - allows to copy 4 bytes with "="
typedef struct ipaddr_t {
	byte data[4];
} ipaddr_t;

typedef struct ping_node_t {
	ipaddr_t ipaddr;
	nodeid_t prev;           // previous node on the shortest path
	nodeid_t nlist_start;    // index of the first neighbour
	nodeid_t nlist_end;      // index of the last neighbour + 1
	dist_t dist;              // distance (= ping)
	qbool visited;
	unsigned short proxport; // if there's a proxy on this address,
	                         // this is the port it's running on
	                         // and it is already in the network format
} ping_node_t;

typedef struct ping_neighbour_t {
	nodeid_t id;
	dist_t dist;
} ping_neighbour_t;

typedef struct proxy_query_request_t {
	socket_t sock;
	nodeid_t nodeid;
	qbool done;
	qbool ping_received;
	unsigned int mesh_nonce;
} proxy_query_request_t;

typedef struct proxy_request_queue_t {
	proxy_query_request_t *data;
	size_t items;
	qbool sending_done; // sending thread ended
	qbool allrecved;	// all proxies have been successfully scanned
} proxy_request_queue;

static ping_node_t ping_nodes[MAX_SERVERS];
static nodeid_t ping_nodes_count = 0;
#define MAX_SERVERS_BLOCKSIZE (MAX_SERVERS*MAX_NONLEAVES)
static ping_neighbour_t* ping_neighbours;
static unsigned long ping_neighbours_max;
static nodeid_t ping_neighbours_count = 0;

static nodeid_t startnode_id = 0;

static qbool building_pingtree = false; // when true, the pingtree build thread is still working
static qbool pingtree_built = false;
static double pingtree_built_at;

static sem_t phase2thread_lock;

static void SB_PingTree_Assertions(void)
{
	if (ping_nodes_count >= MAX_SERVERS) {
		Sys_Error("EX_Browser_pathfind: max nodes count reached");
	}

	if (ping_neighbours_count >= ping_neighbours_max) {
		while (ping_neighbours_count >= ping_neighbours_max)
			ping_neighbours_max += MAX_SERVERS_BLOCKSIZE;
		ping_neighbours = Q_realloc(ping_neighbours, ping_neighbours_max * sizeof(ping_neighbour_t));
		if (!ping_neighbours) {
			Sys_Error("EX_Browser_pathfind: max neighbours count reached");
		}
	}

	if (startnode_id != 0) {
		// a bit paranoid check, startnode is always the first node
		Sys_Error("EX_browser_pathfind: startnode_id != 0");
	}
}

static int SB_PingTree_FindIp(ipaddr_t ipaddr)
{
	int i;

	// xxx make the lookup faster than linear
	for (i = 0; i < ping_nodes_count; i++) {
		if (memcmp(&ping_nodes[i].ipaddr, &ipaddr, sizeof(ipaddr_t)) == 0) {
			return i;
		}
	}

	return INVALID_NODE;
}

static int SB_PingTree_AddNode(ipaddr_t ipaddr, unsigned short proxport)
{
	int id = SB_PingTree_FindIp(ipaddr);

	if (id != INVALID_NODE) {
		if (proxport && !ping_nodes[id].proxport) {
			ping_nodes[id].proxport = proxport;
		}
		return id;
	}

	id = ping_nodes_count++;

	SB_PingTree_Assertions();
	ping_nodes[id].ipaddr = ipaddr;
	ping_nodes[id].prev = INVALID_NODE;
	ping_nodes[id].nlist_start = INVALID_NODE;
	ping_nodes[id].nlist_end = INVALID_NODE;
	ping_nodes[id].dist = DIST_INFINITY;
	ping_nodes[id].proxport = proxport;
	ping_nodes[id].visited = false;
	SB_PingTree_Assertions();

	return id;
}

static int SB_PingTree_AddNeighbour(nodeid_t neighbour_id, dist_t dist)
{
	int id = ping_neighbours_count++;

	SB_PingTree_Assertions();
	ping_neighbours[id].id = neighbour_id;
	ping_neighbours[id].dist = dist;
	SB_PingTree_Assertions();

	return id;
}

static ipaddr_t SB_DummyIpAddr(void)
{
	ipaddr_t dummy = {{0, 0, 0, 0}};
	return dummy;
}

static void SB_PingTree_AddSelf(void)
{
	startnode_id = SB_PingTree_AddNode(SB_DummyIpAddr(), 0);
}

static void SB_PingTree_Clear(void)
{
	ping_nodes_count = 0;
	ping_neighbours_count = 0;
	SB_PingTree_AddSelf();
}

static ipaddr_t SB_Netaddr2Ipaddr(const netadr_t *netadr)
{
	ipaddr_t retval;
	memcpy(retval.data, &netadr->ip, 4);
	return retval;
}

static qbool SB_PingTree_IsServerDead(const server_data *data)
{
	return data->ping < 0;
}

static qbool SB_PingTree_IsProxyFiltered(const server_data *data)
{
	if (!data->qwfwd) {
		return false;
	}
	else if (sb_ignore_proxy.string[0] == '\0') {
		return false;
	}
	else {
		char ip_str[32];
		const byte *ip = data->address.ip;
		int port = (int) ntohs(data->address.port);
		
		snprintf(&ip_str[0], sizeof(ip_str), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);

		return strstr(sb_ignore_proxy.string, ip_str) != NULL;
	}
}

static nodeid_t SB_PingTree_AddServer(const server_data *data)
{
	nodeid_t node_id = INVALID_NODE;

	if (!SB_PingTree_IsServerDead(data) && !SB_PingTree_IsProxyFiltered(data)) {
			node_id = SB_PingTree_AddNode(SB_Netaddr2Ipaddr(&data->address),
				data->qwfwd ? data->address.port : 0);

		SB_PingTree_AddNeighbour(node_id, data->ping);
	}
	
	return node_id;
}

static void SB_PingTree_AddProxyPing(netadr_t adr, dist_t dist)
{
	nodeid_t id_neighbour;
	ipaddr_t ip = SB_Netaddr2Ipaddr(&adr);

	id_neighbour = SB_PingTree_FindIp(ip); // most of the servers should be found
	if (id_neighbour == INVALID_NODE) {
		// strange - there is no direct route to this server, but a proxy can reach it (!)
		id_neighbour = SB_PingTree_AddNode(ip, 0);
	}
	
	SB_PingTree_AddNeighbour(id_neighbour, dist);
}

static void SB_Proxy_ParseReply(const byte *buf, size_t buflen, proxy_ping_report_callback callback)
{
	size_t entries = buflen / PROXY_REPLY_ENTRY_LEN;
	int i;

	Com_DPrintf("Reading %d entries from a proxy reply\n", entries);

	for (i = 0; i < entries; i++) {
		netadr_t adr;
		dist_t dist = 0;

		adr.type = NA_IP;
		memcpy(adr.ip, buf, 4);
		buf += 4;
		
		adr.port = 0;
		adr.port |= 0x00FF & *buf++;
		adr.port |= 0xFF00 & (*buf++ << 8);

		dist |= 0x00FF & *buf++;
		dist |= 0xFF00 & (*buf++ << 8);

		if (dist >= 0) {
			// "server not reachable" is reported as 65536, in our case -1
			callback(adr, dist);
		}
	}
}

static qbool SB_Mesh_ParseReply(const byte *buf, size_t buflen,
	unsigned int expected_nonce, proxy_ping_report_callback callback)
{
	size_t offset;
	unsigned int nonce;

	if (buflen < MESH_REPLY_HEADER_LEN ||
	    buf[0] != 0xff || buf[1] != 0xff || buf[2] != 0xff || buf[3] != 0xff ||
	    buf[4] != 'Q' || buf[5] != 'M' ||
	    buf[6] != MESH_MSG_PINGSTATUS_REPLY ||
	    (buflen - MESH_REPLY_HEADER_LEN) % MESH_REPLY_ENTRY_LEN != 0)
		return false;

	nonce = (unsigned int)buf[7] |
	        ((unsigned int)buf[8] << 8) |
	        ((unsigned int)buf[9] << 16) |
	        ((unsigned int)buf[10] << 24);
	if (!nonce || nonce != expected_nonce)
		return false;

	for (offset = MESH_REPLY_HEADER_LEN;
	     offset + MESH_REPLY_ENTRY_LEN <= buflen;
	     offset += MESH_REPLY_ENTRY_LEN) {
		netadr_t adr;
		int ping, jitter, loss, quality_cost;

		memset(&adr, 0, sizeof(adr));
		adr.type = NA_IP;
		memcpy(adr.ip, buf + offset, 4);
		adr.port = htons((unsigned short)(buf[offset + 4] |
		                              (buf[offset + 5] << 8)));
		ping = (short)(buf[offset + 6] | (buf[offset + 7] << 8));
		jitter = (short)(buf[offset + 8] | (buf[offset + 9] << 8));
		loss = (short)(buf[offset + 10] | (buf[offset + 11] << 8));
		if (ping < 0 || jitter < 0 || loss < 0 || loss > 100)
			continue;

		// Preserve the existing dist_t graph while making route selection
		// quality-aware.  A small ping advantage no longer beats severe
		// jitter/loss; UI bestping remains an estimated route cost.
		quality_cost = ping + jitter / 2 + loss * 2;
		if (quality_cost > SHRT_MAX)
			quality_cost = SHRT_MAX;
		callback(adr, (dist_t)quality_cost);
	}

	return true;
}

void SB_Proxy_QueryForPingList(const netadr_t *address, proxy_ping_report_callback callback)
{
	byte buf[PROXY_REPLY_BUFFER_SIZE];
	char packet[] = PROXY_PINGLIST_QUERY;
	char adrstr[32];
	struct sockaddr_in addr_to, addr_from;
	struct timeval timeout;
	fd_set fd;
	socket_t sock;
	int i, ret;
	socklen_t inaddrlen;

	snprintf(&adrstr[0], sizeof(adrstr), "%d.%d.%d.%d", (int) address->ip[0], (int) address->ip[1], (int) address->ip[2], (int) address->ip[3]);

	addr_to.sin_addr.s_addr = inet_addr((const char *)adrstr);
	if (addr_to.sin_addr.s_addr == INADDR_NONE) {
		return;
	}
	addr_to.sin_family = AF_INET;
	addr_to.sin_port = address->port;

	sock = UDP_OpenSocket(PORT_ANY);
	for (i = 0; i < sb_proxretries.integer; i++) {
		ret = sendto(sock, packet, strlen(packet), 0, (struct sockaddr *)&addr_to, sizeof(struct sockaddr));
		if (ret == -1) // failure, try again
			continue;

_select:
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeout.tv_sec = 0;
		timeout.tv_usec = sb_proxtimeout.integer * 1000;
		ret = select(sock+1, &fd, NULL, NULL, &timeout);
		if (ret <= 0) { // timed out or error
			Com_DPrintf("select() gave errno = %d : %s\n", errno, strerror(errno));
			continue;
		}

		inaddrlen = sizeof(struct sockaddr_in);
		ret = recvfrom(sock, (char *) buf, sizeof(buf), 0, (struct sockaddr *)&addr_from, &inaddrlen);

		if (ret == -1) // failure, try again
			continue;
		if (addr_from.sin_addr.s_addr != addr_to.sin_addr.s_addr) // martian, discard and see if a valid response came in after it
			goto _select;
		if (strncmp("\xff\xff\xff\xffn", (char *) buf, 5) == 0)
			SB_Proxy_ParseReply(buf+5, ret-5, callback);

		break;
	}
	closesocket(sock);
}

static void SB_PingTree_AddNodes(void)
{
	int i;
	
	// add our neighbours - servers we directly ping
	SB_ServerList_Lock();
	ping_nodes[startnode_id].nlist_start = ping_neighbours_count;
	for (i = 0; i < serversn; i++) {
		SB_PingTree_AddServer(servers[i]);
	}
	ping_nodes[startnode_id].nlist_end = ping_neighbours_count;
	SB_ServerList_Unlock();
}

static netadr_t SB_NodeNetadr_Get(nodeid_t id)
{
	netadr_t ret;
	ret.type = NA_IP;
	ret.port = ping_nodes[id].proxport;
	memcpy(&ret.ip, ping_nodes[id].ipaddr.data, 4);
	return ret;
}

int SB_PingTree_SendQueryThread(void *thread_arg)
{
	proxy_request_queue *queue = (proxy_request_queue *) thread_arg;
	int i, ret;
	double interval_ms;
#ifdef _WIN32
	timerresolution_session_t timersession = {0, 0};
#endif

	// Null check for queue
	if (!queue) {
		Com_DPrintf("SB_PingTree_SendQueryThread: queue is NULL\n");
		return -1;
	}

	// Null check for queue data
	if (!queue->data) {
		Com_DPrintf("SB_PingTree_SendQueryThread: queue->data is NULL\n");
		return -1;
	}

	interval_ms = (1.0 / sb_proxinfopersec.value) * 1000.0;

	Sys_TimerResolution_InitSession(&timersession);
	Sys_TimerResolution_RequestMinimum(&timersession);

	for (i = 0; i < queue->items; i++) {
		if (!queue->data[i].done) {
			struct sockaddr_storage addr_to;
			netadr_t netadr;
			char mesh_query[64];
			int mesh_query_len;

			// Validate nodeid
			if (queue->data[i].nodeid < 0 || queue->data[i].nodeid >= ping_nodes_count) {
				Com_DPrintf("SB_PingTree_SendQueryThread: invalid nodeid %d\n", queue->data[i].nodeid);
				continue;
			}

			netadr = SB_NodeNetadr_Get(queue->data[i].nodeid);

			NetadrToSockadr(&netadr, &addr_to);
			// Always ask for the rich mesh snapshot first.  Updated qwfwd
			// instances answer with avg/jitter/loss; legacy instances ignore it.
			mesh_query_len = snprintf(mesh_query, sizeof(mesh_query),
				"\xff\xff\xff\xffmeshprobe %u", queue->data[i].mesh_nonce);
			ret = sendto(queue->data[i].sock, mesh_query, mesh_query_len, 0,
				(struct sockaddr *) &addr_to, sizeof (struct sockaddr));
			if (ret < 0)
				Com_DPrintf("SB_PingTree_SendQueryThread mesh sendto returned %d\n", ret);

			// Keep byte-for-byte legacy compatibility and provide an immediate
			// fallback when the remote proxy has not been upgraded.
			if (!queue->data[i].ping_received) {
				ret = sendto(queue->data[i].sock,
					PROXY_PINGLIST_QUERY, PROXY_PINGLIST_QUERY_LEN, 0,
					(struct sockaddr *) &addr_to, sizeof (struct sockaddr));
				if (ret < 0)
					Com_DPrintf("SB_PingTree_SendQueryThread ping sendto returned %d\n", ret);
			}
			Sys_MSleep(interval_ms);
		}

		if (queue->allrecved) break;
	}

	Sys_TimerResolution_Clear(&timersession);

	queue->sending_done = true;

	return 0;
}

#define PROXY_SERIALIZE_FILE_VERSION 1
void SB_Proxylist_Serialize_Start(FILE *f)
{
	int version = PROXY_SERIALIZE_FILE_VERSION;

	// header
	// - version
	fwrite(&version, sizeof(int), 1, f);
}

void SB_Proxylist_Serialize_Reply(FILE *f, netadr_t proxy, void *buf, size_t buflen)
{
	fwrite(&proxy, sizeof(netadr_t), 1, f);
	fwrite(&buflen, sizeof(size_t), 1, f);
	fwrite(buf, buflen, 1, f);
}

void SB_Proxylist_Serialize_End(FILE *f)
{
	netadr_t invalid;

	invalid.type = NA_INVALID;
	fwrite(&invalid, sizeof(netadr_t), 1, f);
}

static qbool SB_PingTree_RecvQuery(proxy_request_queue *queue, FILE *f)
{
	qbool last_cycle = false;
	fd_set recvset;
	int maxsock = 0;
	int i, ret;
	struct timeval timeout;
	qbool allrecved = false;

	timeout.tv_sec = 0;
	timeout.tv_usec = sb_proxtimeout.integer * 1000;

	for (;;) {
		if (queue->sending_done) {
			last_cycle = true;
		}
		
		allrecved = true;
		FD_ZERO(&recvset);
		for (i = 0; i < queue->items; i++) {
			if (!queue->data[i].done) {
				socket_t sock = queue->data[i].sock;
				FD_SET(sock, &recvset);
				if ((int) sock > maxsock) {
					maxsock = (int) sock;
				}
				allrecved = false;
			}
		}
		if (allrecved) {
			queue->allrecved = true;
			break;
		}

		ret = select((maxsock + 1), &recvset, NULL, NULL, &timeout);

		if (ret == 0 && last_cycle == true) {
			break; // ret = 0 means we got timeout
		}

		if (ret == 0) {
			continue; // not all proxies were queried yet
		}

		if (ret < 0) {
			Com_DPrintf("select returned %d\n", ret);
			break;
		}

		for (i = 0; i < queue->items; i++) {
			if (!queue->data[i].done && FD_ISSET(queue->data[i].sock, &recvset)) {
				byte buf[PROXY_REPLY_BUFFER_SIZE];
				struct sockaddr_storage addr_from;
				netadr_t from, expected;
				socklen_t addr_from_len = sizeof(struct sockaddr_in);

				ret = recvfrom(queue->data[i].sock, (char *) buf, PROXY_REPLY_BUFFER_SIZE, 0, (struct sockaddr *) &addr_from, &addr_from_len);
				if (ret == -1) {
					Com_DPrintf("SB_PingTree_RecvQuery recvfrom failed\n");
					continue;
				}

				SockadrToNetadr(&addr_from, &from);
				expected = SB_NodeNetadr_Get(queue->data[i].nodeid);
				if (!NET_CompareAdr(from, expected)) {
					Com_DPrintf("SB_PingTree_RecvQuery ignored unexpected source\n");
					continue;
				}

				if (ret >= 5 && !queue->data[i].ping_received &&
				    memcmp("\xff\xff\xff\xffn", buf, 5) == 0) {
					nodeid_t id = queue->data[i].nodeid;
					queue->data[i].ping_received = true;
					ping_nodes[id].nlist_start = ping_neighbours_count;
					if (f && ret > 5)
							SB_Proxylist_Serialize_Reply(f, SB_NodeNetadr_Get(id), buf+5, ret-5);
					SB_Proxy_ParseReply(buf+5, ret-5, SB_PingTree_AddProxyPing);
					ping_nodes[id].nlist_end = ping_neighbours_count;
				}
				else if (ret >= MESH_REPLY_HEADER_LEN) {
					nodeid_t id = queue->data[i].nodeid;
					nodeid_t old_start = ping_nodes[id].nlist_start;
					nodeid_t old_end = ping_nodes[id].nlist_end;
					ping_nodes[id].nlist_start = ping_neighbours_count;
					if (SB_Mesh_ParseReply(buf, (size_t)ret,
					        queue->data[i].mesh_nonce, SB_PingTree_AddProxyPing)) {
						ping_nodes[id].nlist_end = ping_neighbours_count;
						queue->data[i].done = true;
					}
					else {
						ping_nodes[id].nlist_start = old_start;
						ping_nodes[id].nlist_end = old_end;
					}
				}
				else {
					Com_DPrintf("Invalid reply received\n");
				}
			}
		}
	}

	return allrecved;
}

static void SB_PingTree_ScanProxies(void)
{
	int i;
	proxy_request_queue *queue = NULL;
	size_t request = 0;
	FILE *f = NULL;

	// Allocate queue on heap
	queue = (proxy_request_queue *) Q_malloc(sizeof(proxy_request_queue));
	if (!queue) {
		Com_Printf("Failed to allocate memory for proxy request queue\n");
		return;
	}

	// Initialize queue fields
	queue->data = NULL;
	queue->items = 0;
	queue->sending_done = false;
	queue->allrecved = false;

	for (i = 0; i < ping_nodes_count; i++) {
		if (ping_nodes[i].proxport) {
			queue->items++;
		}
	}

	if (!queue->items) {
		Q_free(queue);
		return;
	}

	queue->data = (proxy_query_request_t *) Q_malloc(sizeof(proxy_query_request_t) * queue->items);
	if (!queue->data) {
		Com_Printf("Failed to allocate memory for proxy request data\n");
		Q_free(queue);
		return;
	}

	for (i = 0; i < ping_nodes_count; i++) {
		if (ping_nodes[i].proxport) {
			queue->data[request].done = false;
			queue->data[request].ping_received = false;
			queue->data[request].nodeid = i;
			queue->data[request].mesh_nonce =
				((unsigned int)rand() << 16) ^ (unsigned int)rand() ^ (unsigned int)(request + 1);
			if (!queue->data[request].mesh_nonce)
				queue->data[request].mesh_nonce = (unsigned int)(request + 1);
			queue->data[request].sock = UDP_OpenSocket(PORT_ANY);
			request++;
		}
	}

	if (sb_listcache.value) {
		char prx_data_path[MAX_OSPATH] = {0};

		snprintf(&prx_data_path[0], sizeof(prx_data_path), "%s/%s", com_homedir, "proxies_data");
		f = fopen(prx_data_path, "wb");
		if (f)
			SB_Proxylist_Serialize_Start(f);
	}

	for (i = 0; i < sb_proxretries.integer; i++) {
		queue->sending_done = false;
		if (Sys_CreateDetachedThread(SB_PingTree_SendQueryThread, (void *) queue) < 0) {
			Com_Printf("Failed to create SB_PingTree_SendQueryThread thread\n");
			// If thread creation fails, don't continue with this retry
			continue;
		}
		SB_PingTree_RecvQuery(queue, f);
		
		// Wait for the sending thread to complete before next retry
		while (!queue->sending_done) {
			Sys_MSleep(10);
		}
		
		if (queue->allrecved) {
			break;
		}
	}

	if (f) {
		SB_Proxylist_Serialize_End(f);
		fclose(f);
	}

	for (i = 0; i < queue->items; i++) {
		closesocket(queue->data[i].sock);
	}

	Q_free(queue->data);
	Q_free(queue);
}

static nodeid_t SB_PingTree_NearestNodeGet(void)
{
	// XXX: implement using binary/fibonacci heap...
	int i;
	nodeid_t ret = INVALID_NODE;
	dist_t minimum = DIST_INFINITY;

	for (i = 0; i < ping_nodes_count; i++) {
		if (!ping_nodes[i].visited && ping_nodes[i].dist < minimum) {
			ret = i;
			minimum = ping_nodes[i].dist;
		}
	}

	return ret;
}

static void SB_PingTree_Dijkstra(void)
{
	int i;

	ping_nodes[startnode_id].dist = 0;

	for (;;) {
		nodeid_t cur = SB_PingTree_NearestNodeGet();
		if (cur == INVALID_NODE) break;

		ping_nodes[cur].visited = true;
		for (i = ping_nodes[cur].nlist_start; i < ping_nodes[cur].nlist_end; i++) {
			dist_t altdist = ping_nodes[cur].dist + ping_neighbours[i].dist;
			if (altdist < ping_nodes[ping_neighbours[i].id].dist) {
				// so-called Relax()
				ping_nodes[ping_neighbours[i].id].dist = altdist;
				ping_nodes[ping_neighbours[i].id].prev = cur;
			}
		}
	}
}

static void SB_PingTree_Phase1(void)
{
	SB_PingTree_Clear();
	SB_PingTree_AddNodes();
}

static void SB_PingTree_UpdateServerList(void)
{
	int i;

	SB_ServerList_Lock();

	for (i = 0; i < serversn; i++) {
		nodeid_t id = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(&servers[i]->address));
		if (id == INVALID_NODE || ping_nodes[id].prev == INVALID_NODE || ping_nodes[id].prev == startnode_id) continue;

		SB_Server_SetBestPing(servers[i], ping_nodes[id].dist);
	}

	SB_ServerList_Unlock();
}

int SB_PingTree_Phase2(void *ignored_arg)
{
	SB_PingTree_ScanProxies();
	SB_PingTree_Dijkstra();
	SB_PingTree_UpdateServerList();

	sb_queuedtriggers |= SB_TRIGGER_NOTIFY_PINGTREE;
	Sys_SemPost(&phase2thread_lock);
	building_pingtree = false;
	pingtree_built = true;
	pingtree_built_at = Sys_DoubleTime();
	return 0;
}

/// Has the Ping Tree been already built?
qbool SB_PingTree_Built(void)
{
	return pingtree_built;
}

double SB_PingTree_Age(void)
{
	return pingtree_built_at > 0 ? Sys_DoubleTime() - pingtree_built_at : DBL_MAX;
}

/// Creates whole graph structure for looking up shortest paths to servers (ping-wise).
///
/// Grabs data from the server browser and then from the proxies.
void SB_PingTree_Build(void)
{
	if (building_pingtree) {
		Com_Printf("Ping Tree is still being built...\n");
		return;
	}
	// no race condition here, as this must always get executed by the main thread
	building_pingtree = true;
	pingtree_built = false;
	pingtree_built_at = 0;
	Com_Printf("Building the Ping Tree...\n");

	// first quick phase is initialization + quick read of data from the server browser
	SB_PingTree_Phase1();
	// second longer phase is querying the proxies for their ping data + dijkstra algo
	Sys_SemWait(&phase2thread_lock);
	if (Sys_CreateDetachedThread(SB_PingTree_Phase2, NULL) < 0) {
		Com_Printf("Failed to create SB_PingTree_Phase2 thread\n");
		building_pingtree = false;
		pingtree_built = false;
		Sys_SemPost(&phase2thread_lock);
	}
}

/// Prints the shortest path to given IP address
void SB_PingTree_DumpPath(const netadr_t *addr)
{
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	if (target == INVALID_NODE) {
		Com_Printf("No route found to given host\n");
	}
	else {
		nodeid_t current = target;

		Com_Printf("Shortest path length: %d ms\nRoute: \n", ping_nodes[current].dist);
		while (current != startnode_id && current != INVALID_NODE) {
			byte *ip = ping_nodes[current].ipaddr.data;
			Com_Printf("%4d ms  %d.%d.%d.%d:%d\n", ping_nodes[current].dist, 
				ip[0], ip[1], ip[2], ip[3],	ntohs(ping_nodes[current].proxport));
			current = ping_nodes[current].prev;
		}
		Com_Printf("%4d ms  localhost (your machine)\n", 0);
	}
}

int SB_PingTree_GetPathLen(const netadr_t *addr)
{
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	if (target == INVALID_NODE || ping_nodes[target].prev == INVALID_NODE) {
		return -1;
	}
	else if (ping_nodes[target].prev == startnode_id) {
		return 0;
	}
	else {
		nodeid_t current = ping_nodes[target].prev;
		int proxies = 0;

		while (current != startnode_id && current != INVALID_NODE) {
			proxies++;
			current = ping_nodes[current].prev;
		}

		return proxies;
	}
}

/// Returns the proxy chain string for the best path to addr.
/// Does NOT modify cl_proxyaddr, does NOT issue any connect command.
///
/// out receives the proxylist string, e.g. "1.2.3.4:30000" or
/// "1.2.3.4:30000@5.6.7.8:30000" for multi-hop routes.
/// out_total_ping_ms receives the Dijkstra total ping estimate.
///
/// Returns true  if a proxy route was found and written to out.
/// Returns false if direct connection is best, or no route exists.
qbool SB_PingTree_GetProxyString(const netadr_t *addr, char *out, size_t outsz,
                                  int *out_total_ping_ms)
{
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	out[0] = '\0';
	*out_total_ping_ms = 0;

	if (target == INVALID_NODE || ping_nodes[target].prev == INVALID_NODE) {
		return false;
	}

	*out_total_ping_ms = (int)ping_nodes[target].dist;

	if (ping_nodes[target].prev == startnode_id) {
		return false;
	}

	{
		char proxylist_buf[32 * MAX_NONLEAVES];
		nodeid_t current = ping_nodes[target].prev;

		proxylist_buf[0] = '\0';

		while (current != startnode_id && current != INVALID_NODE) {
			byte *ip = ping_nodes[current].ipaddr.data;
			char newval[2048];

			snprintf(&newval[0], sizeof(newval), "%d.%d.%d.%d:%d%s%s",
			         (int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3],
			         (int)ntohs(ping_nodes[current].proxport),
			         *proxylist_buf ? "@" : "",
			         proxylist_buf);
			strlcpy(proxylist_buf, newval, sizeof(proxylist_buf));

			current = ping_nodes[current].prev;
		}

		strlcpy(out, proxylist_buf, outsz);
	}

	return (out[0] != '\0');
}

/*
 * Return the best route for every viable first hop.  This keeps route
 * discovery in sb_findroutes: connectbr is only a user-facing selector and
 * connectnext advances through this cached ranking without probing again.
 *
 * One reverse Dijkstra from the destination gives the cheapest continuation
 * from every proxy.  Combining that with each measured local->proxy edge
 * produces independent alternatives without maintaining a second graph.
 */
int SB_PingTree_GetRoutes(const netadr_t *addr, sb_route_t *routes, int max_routes)
{
	nodeid_t target;
	int *heads, *rev_next, *rev_source, *dist, *next_hop, *route_hops;
	qbool *visited;
	int reverse_count = 0;
	int i, source, count = 0;

	if (!routes || max_routes <= 0 || !SB_PingTree_Built() || building_pingtree)
		return 0;
	if (max_routes > SB_ROUTE_MAX_ALTERNATIVES)
		max_routes = SB_ROUTE_MAX_ALTERNATIVES;

	target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));
	if (target == INVALID_NODE)
		return 0;

	heads = (int *)Q_malloc(sizeof(int) * ping_nodes_count);
	rev_next = (int *)Q_malloc(sizeof(int) * ping_neighbours_count);
	rev_source = (int *)Q_malloc(sizeof(int) * ping_neighbours_count);
	dist = (int *)Q_malloc(sizeof(int) * ping_nodes_count);
	next_hop = (int *)Q_malloc(sizeof(int) * ping_nodes_count);
	route_hops = (int *)Q_malloc(sizeof(int) * ping_nodes_count);
	visited = (qbool *)Q_malloc(sizeof(qbool) * ping_nodes_count);
	if (!heads || !rev_next || !rev_source || !dist || !next_hop || !route_hops || !visited)
		goto cleanup;

	for (i = 0; i < ping_nodes_count; i++) {
		heads[i] = -1;
		dist[i] = INT_MAX;
		next_hop[i] = INVALID_NODE;
		route_hops[i] = INT_MAX;
		visited[i] = false;
	}

	for (source = 0; source < ping_nodes_count; source++) {
		if (ping_nodes[source].nlist_start == INVALID_NODE)
			continue;
		for (i = ping_nodes[source].nlist_start; i < ping_nodes[source].nlist_end; i++) {
			nodeid_t destination = ping_neighbours[i].id;
			rev_source[reverse_count] = source;
			rev_next[reverse_count] = heads[destination];
			heads[destination] = reverse_count++;
		}
	}

	dist[target] = 0;
	route_hops[target] = 0;
	for (;;) {
		nodeid_t current = INVALID_NODE;
		int best = INT_MAX;
		for (i = 0; i < ping_nodes_count; i++) {
			if (!visited[i] && dist[i] < best) {
				best = dist[i];
				current = i;
			}
		}
		if (current == INVALID_NODE)
			break;
		visited[current] = true;
		for (i = heads[current]; i >= 0; i = rev_next[i]) {
			nodeid_t predecessor = rev_source[i];
			int edge_index;
			if (predecessor == startnode_id || route_hops[current] >= MAX_NONLEAVES)
				continue;
			for (edge_index = ping_nodes[predecessor].nlist_start;
			     edge_index < ping_nodes[predecessor].nlist_end; edge_index++) {
				if (ping_neighbours[edge_index].id == current) {
					int alternative;
					if (ping_neighbours[edge_index].dist < 0 ||
					    dist[current] > INT_MAX - ping_neighbours[edge_index].dist)
						break;
					alternative = dist[current] + ping_neighbours[edge_index].dist;
					if (alternative < dist[predecessor]) {
						dist[predecessor] = alternative;
						next_hop[predecessor] = current;
						route_hops[predecessor] = route_hops[current] + 1;
					}
					break;
				}
			}
		}
	}

	if (ping_nodes[startnode_id].nlist_start != INVALID_NODE) {
		for (i = ping_nodes[startnode_id].nlist_start;
		     i < ping_nodes[startnode_id].nlist_end; i++) {
			nodeid_t first = ping_neighbours[i].id;
			sb_route_t candidate;
			nodeid_t current;
			int duplicate = false;
			int safety = 0;

			memset(&candidate, 0, sizeof(candidate));
			if (first == target) {
				candidate.total_cost_ms = ping_neighbours[i].dist;
			}
			else {
				if (!ping_nodes[first].proxport || dist[first] == INT_MAX)
					continue;
				if (ping_neighbours[i].dist < 0 ||
				    dist[first] > INT_MAX - ping_neighbours[i].dist)
					continue;
				candidate.total_cost_ms = ping_neighbours[i].dist + dist[first];
				current = first;
				while (current != target && current != INVALID_NODE && safety++ < MAX_NONLEAVES) {
					byte *ip = ping_nodes[current].ipaddr.data;
					char proxy[48];
					if (!ping_nodes[current].proxport) {
						candidate.proxylist[0] = '\0';
						break;
					}
					snprintf(proxy, sizeof(proxy), "%d.%d.%d.%d:%d",
					         ip[0], ip[1], ip[2], ip[3], ntohs(ping_nodes[current].proxport));
					if (strlen(candidate.proxylist) + (candidate.proxylist[0] ? 1 : 0) +
					    strlen(proxy) + 1 > sizeof(candidate.proxylist)) {
						candidate.proxylist[0] = '\0';
						break;
					}
					if (candidate.proxylist[0])
						strlcat(candidate.proxylist, "@", sizeof(candidate.proxylist));
					strlcat(candidate.proxylist, proxy, sizeof(candidate.proxylist));
					candidate.hops++;
					current = next_hop[current];
				}
				if (current != target || !candidate.proxylist[0])
					continue;
			}

			for (source = 0; source < count; source++) {
				if (!strcmp(routes[source].proxylist, candidate.proxylist)) {
					duplicate = true;
					break;
				}
			}
			if (!duplicate) {
				if (count < max_routes) {
					routes[count++] = candidate;
				}
				else {
					int worst = 0;
					for (source = 1; source < count; source++)
						if (routes[source].total_cost_ms > routes[worst].total_cost_ms)
							worst = source;
					if (candidate.total_cost_ms < routes[worst].total_cost_ms)
						routes[worst] = candidate;
				}
			}
		}
	}

	/* The candidate count is small; insertion sort keeps this C89-friendly. */
	for (i = 1; i < count; i++) {
		sb_route_t value = routes[i];
		int pos = i;
		while (pos > 0 && routes[pos - 1].total_cost_ms > value.total_cost_ms) {
			routes[pos] = routes[pos - 1];
			pos--;
		}
		routes[pos] = value;
	}

cleanup:
	Q_free(heads);
	Q_free(rev_next);
	Q_free(rev_source);
	Q_free(dist);
	Q_free(next_hop);
	Q_free(route_hops);
	Q_free(visited);
	return count;
}

/// Connects to given QW server using the best available route
void SB_PingTree_ConnectBestPath(const netadr_t *addr)
{
	extern cvar_t cl_proxyaddr;
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	if (target == INVALID_NODE || ping_nodes[target].prev == INVALID_NODE) {
		Com_Printf("No route found, trying to connect directly...\n");
		Cvar_Set(&cl_proxyaddr, "");
	}
	else if (ping_nodes[target].prev == startnode_id) {
		Com_Printf("Direct route is the best route, connecting directly...\n");
		Cvar_Set(&cl_proxyaddr, "");
	}
	else {
		char proxylist_buf[32*MAX_NONLEAVES] = "";
		nodeid_t current = ping_nodes[target].prev;
		int proxies = 0;

		while (current != startnode_id && current != INVALID_NODE) {
			byte *ip = ping_nodes[current].ipaddr.data;
			char newval[2048]; /* va() used 2048b buffer..*/

			snprintf(&newval[0], sizeof(newval), "%d.%d.%d.%d:%d%s%s", (int) ip[0], (int) ip[1], (int) ip[2],
				(int) ip[3], (int) ntohs(ping_nodes[current].proxport), *proxylist_buf ? "@" : "", proxylist_buf);
			strlcpy(proxylist_buf, newval, 32*MAX_NONLEAVES);

			proxies++;
			current = ping_nodes[current].prev;
		}
		
		Com_Printf("Connecting using %d %s with best ping %d ms\n",
			proxies, ((proxies == 1) ? "proxy" : "proxies"), ping_nodes[target].dist);
		Cvar_Set(&cl_proxyaddr, proxylist_buf);
	}

	/* FIXME: Create a Cbuf_AddTextFmt? */
	Cbuf_AddText("connect ");
	Cbuf_AddText(NET_AdrToString(*addr));
	Cbuf_AddText("\n");
}

int SB_Proxylist_Unserialize(FILE *f)
{
	int version, count = 0;

	if (fread(&version, sizeof(int), 1, f) != 1)
		return -1;
	if (version != PROXY_SERIALIZE_FILE_VERSION)
		return -1;

	while (!ferror(f) && !feof(f)) {
		netadr_t proxy;
		size_t buflen;
		byte buf[PROXY_REPLY_BUFFER_SIZE];
		nodeid_t id;

		if (fread(&proxy, sizeof(netadr_t), 1, f) != 1)
			return -3;

		if (proxy.type == NA_INVALID)
			break;

		if (fread(&buflen, sizeof(size_t), 1, f) != 1)
			return -3;
		if (buflen > PROXY_REPLY_BUFFER_SIZE)
			return -3;
		if (fread(buf, buflen, 1, f) != 1)
			return -3;

		id = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(&proxy));
		if (id == INVALID_NODE)
			return -3;

		ping_nodes[id].nlist_start = ping_neighbours_count;
		SB_Proxy_ParseReply(buf, buflen, SB_PingTree_AddProxyPing);
		ping_nodes[id].nlist_end = ping_neighbours_count;

		count++;
	}

	return count;
}

void SB_Proxylist_Unserialize_f(void)
{
	char filename[MAX_OSPATH] = {0};
	FILE *f;
	int err;
	
	snprintf(&filename[0], sizeof(filename), "%s/%s", com_homedir, "proxies_data");

	if (!(f	= fopen(filename, "rb"))) {
		Com_Printf("Couldn't read %s.\n", filename);
		return;
	}

	building_pingtree = true;
	SB_PingTree_Phase1();
	err = SB_Proxylist_Unserialize(f);
	if (err > 0) {
		Com_Printf("Successfully read %d proxies\n", err);
		SB_PingTree_Dijkstra();
		SB_PingTree_UpdateServerList();
		pingtree_built = true;
		pingtree_built_at = Sys_DoubleTime();
	}
	else if (err == -1) {
		Com_Printf("Format didn't match\n");
	}
	else if (err == -3) {
		Com_Printf("Corrupted data\n");
	}
	else { // err == 0
		Com_Printf("No proxies read\n");
	}
	building_pingtree = false;

	fclose(f);
}

qbool SB_PingTree_IsBuilding(void)
{
	return building_pingtree;
}

void SB_PingTree_Init(void)
{
	Sys_SemInit(&phase2thread_lock, 1, 1);
}

void SB_PingTree_Shutdown(void)
{
	Sys_SemWait(&phase2thread_lock);
	Sys_SemDestroy(&phase2thread_lock);
}
