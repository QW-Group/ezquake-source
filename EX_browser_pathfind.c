/**
	\file

	\brief
	Module for finding best connection to a server, ping-wise.

	\author johnnycz
**/

#include "quakedef.h"
#include <limits.h>
#ifdef _WIN32
#include "winquake.h"
#else
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

// non-leaf = proxy (or users computer) = has more than 1 neighbour
// at the time of writing this code there were 10 active proxies around the world
#define MAX_NONLEAVES 40

#define DIST_INFINITY SHRT_MAX
#define PROXY_PINGLIST_QUERY "\xff\xff\xff\xffpingstatus"

// current amount of qw servers ~ 300
typedef short nodeid_t;

// trick around language limits - allows to copy 4 bytes with "="
typedef struct ipaddr_t {
	byte data[4];
} ipaddr_t;

typedef struct ping_node_t {
	ipaddr_t ipaddr;
	nodeid_t prev;           // previous node on the shortest path
	nodeid_t nlist_start;    // index of the first neighbour
	nodeid_t nlist_end;      // index of the last neighbour + 1
	short dist;              // distance (= ping)
	qbool visited;
	unsigned short proxport; // if there's a proxy on this address,
	                         // this is the port it's running on
} ping_node_t;

typedef struct ping_neighbour_t {
	nodeid_t id;
	short dist;
} ping_neighbour_t;

static ping_node_t ping_nodes[MAX_SERVERS];
static nodeid_t ping_nodes_count = 0;
static ping_neighbour_t ping_neighbours[MAX_SERVERS*MAX_NONLEAVES];
static nodeid_t ping_neighbours_count = 0;

static nodeid_t startnode_id = 0;

static void SB_PingTree_Assertions(void)
{
	if (ping_nodes_count >= MAX_SERVERS) {
		Sys_Error("EX_Browser_pathfind: max nodes count reached");
	}

	if (ping_neighbours_count >= MAX_SERVERS*MAX_NONLEAVES) {
		Sys_Error("EX_Browser_pathfind: max neighbours count reached");
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

	return -1;
}

static qbool SB_PingTree_HasIp(ipaddr_t ipaddr)
{
	return SB_PingTree_FindIp(ipaddr) >= 0;
}

static int SB_PingTree_AddNode(ipaddr_t ipaddr, nodeid_t prev, nodeid_t nlist_start,
						   nodeid_t nlist_end, short dist, unsigned short proxport)
{
	int id = SB_PingTree_FindIp(ipaddr);

	if (id != -1) {
		if (proxport && !ping_nodes[id].proxport) {
			ping_nodes[id].proxport = proxport;
		}
		return id;
	}

	id = ping_nodes_count++;

	SB_PingTree_Assertions();
	ping_nodes[id].ipaddr = ipaddr;
	ping_nodes[id].prev = prev;
	ping_nodes[id].nlist_start = nlist_start;
	ping_nodes[id].nlist_end = nlist_end;
	ping_nodes[id].dist = dist;
	ping_nodes[id].proxport = proxport;
	ping_nodes[id].visited = false;
	SB_PingTree_Assertions();

	return id;
}

static int SB_PingTree_AddNeighbour(nodeid_t neighbour_id, short dist)
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
	ipaddr_t dummy;
	dummy.data[0] = 0;
	return dummy;
}

static void SB_PingTree_AddSelf(void)
{
	startnode_id = SB_PingTree_AddNode(SB_DummyIpAddr(), -1, -1, -1, 0, 0);
}

static void SB_PingTree_Init(void)
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

static nodeid_t SB_PingTree_AddServer(const server_data *data)
{
	nodeid_t node_id = -1;

	if (data->ping >= 0) {
		node_id = SB_PingTree_AddNode(SB_Netaddr2Ipaddr(&data->address),
			-1, -1, -1, DIST_INFINITY, data->qwfwd ? data->address.port : 0);

		SB_PingTree_AddNeighbour(node_id, data->ping);
	}
	
	return node_id;
}

static void SB_Proxy_ParseReply(nodeid_t id, const char *buf, int buflen)
{
	int pos = 0;
	ipaddr_t ip;
	short dist;
	nodeid_t id_neighbour;

	ping_nodes[id].nlist_start = ping_neighbours_count;
	while (pos + 8 <= buflen) {
		ip.data[0] = *buf++;
		ip.data[1] = *buf++;
		ip.data[2] = *buf++;
		ip.data[3] = *buf++;
		
		// skip the port
		buf++;
		buf++;

		dist = 0;
		dist |= 0xF & *buf++;
		dist |= 0xF0 & *buf++;

		// debug code
		if (ping_nodes[id].ipaddr.data[0] == 83 && ip.data[0] == 82) {
			Com_DPrintf("proxy 83.*.*.* ping to 82.*.*.* is %d ms\n", dist);
		}

		pos += 8;
		
		id_neighbour = SB_PingTree_FindIp(ip); // most of the servers should be found
		if (id_neighbour == -1) {
			// strange - there is no direct route to this server, but a proxy can reach it (!)
			id_neighbour = SB_PingTree_AddNode(ip, -1, -1, -1, DIST_INFINITY, 0);
		}
		
		SB_PingTree_AddNeighbour(id_neighbour, dist);
	}
	ping_nodes[id].nlist_end = ping_neighbours_count;
}

static void SB_Proxy_QueryForPingList(nodeid_t id, const netadr_t *address)
{
	socket_t sock;
	char packet[] = PROXY_PINGLIST_QUERY;
	char buf[MAX_SERVERS*8];
	struct sockaddr_in addr_to, addr_from;
	struct timeval timeout;
	fd_set fd;
	int i, ret;
	socklen_t inaddrlen;
	const char *adrstr = va("%d.%d.%d.%d",
		(int) address->ip[0], (int) address->ip[1], (int) address->ip[2], (int) address->ip[3]);

	addr_to.sin_addr.s_addr = inet_addr(adrstr);
	if (addr_to.sin_addr.s_addr == INADDR_NONE) {
		return;
	}
	addr_to.sin_family = AF_INET;
	addr_to.sin_port = address->port; // xxx: i'm not sure why htons() is not supposed to be used here

	sock = UDP_OpenSocket(PORT_ANY);
	for (i = 0; i < sb_inforetries.integer; i++) {
		ret = sendto(sock, packet, strlen(packet), 0, (struct sockaddr *)&addr_to, sizeof(struct sockaddr));
		if (ret == -1) // failure, try again
			continue;

_select:
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		ret = select(sock+1, &fd, NULL, NULL, &timeout);
		if (ret <= 0) { // timed out or error
			Com_DPrintf("select() gave errno = %d : %s\n", errno, strerror(errno));
			continue;
		}

		inaddrlen = sizeof(struct sockaddr_in);
		ret = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&addr_from, &inaddrlen);

		if (ret == -1) // failure, try again
			continue;
		if (addr_from.sin_addr.s_addr != addr_to.sin_addr.s_addr) // martian, discard and see if a valid response came in after it
			goto _select;
		if (strncmp("\xff\xff\xff\xffn", buf, 5) == 0)
			SB_Proxy_ParseReply(id, buf+5, ret-5);

		break;
	}
	closesocket(sock);
}

static void SB_PingTree_AddProxy(const server_data *data)
{
	nodeid_t id;

	id = SB_PingTree_AddServer(data);

	ping_nodes[id].nlist_start = ping_neighbours_count;
	SB_Proxy_QueryForPingList(id, &data->address);
	ping_nodes[id].nlist_end = ping_neighbours_count;
}

static void SB_PingTree_AddNodes(void)
{
	int i;
	
	// add our neighbours - servers we directly ping, except proxies
	ping_nodes[startnode_id].nlist_start = ping_neighbours_count;
	for (i = 0; i < serversn; i++) {
		if (!servers[i]->qwfwd) {
			SB_PingTree_AddServer(servers[i]);
		}
	}
	ping_nodes[startnode_id].nlist_end = ping_neighbours_count;

	// scan proxies and add their neighbours
	for (i = 0; i < serversn; i++) {
		if (servers[i]->qwfwd) {
			SB_PingTree_AddProxy(servers[i]);
		}
	}
}

static nodeid_t SB_PingTree_NearestNodeGet(void)
{
	// XXX: implement using binary/fibonacci heap...
	int i;
	nodeid_t ret = -1;
	short minimum = DIST_INFINITY;

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
	// initialization is already done, starting node has distance 0, all other nodes infinity
	int i;

	for (;;) {
		nodeid_t cur = SB_PingTree_NearestNodeGet();
		if (cur == -1) break;

		ping_nodes[cur].visited = true;
		for (i = ping_nodes[cur].nlist_start; i < ping_nodes[cur].nlist_end; i++) {
			short altdist = ping_nodes[cur].dist + ping_neighbours[i].dist;
			if (altdist < ping_nodes[ping_neighbours[i].id].dist) {
				// so-called Relax()
				ping_nodes[ping_neighbours[i].id].dist = altdist;
				ping_nodes[ping_neighbours[i].id].prev = cur;
			}
		}
	}
}

/// Creates whole graph structure for looking up shortest paths to servers (ping-wise).
///
/// Grabs data from the server browser and then from the proxies.
void SB_PingTree_Build(void)
{
	extern sem_t serverlist_semaphore;

	SB_PingTree_Init();

	Sys_SemWait(&serverlist_semaphore);
	SB_PingTree_AddNodes();
	Sys_SemPost(&serverlist_semaphore);

	SB_PingTree_Dijkstra();
}

/// Prints the shortest path to given IP address
void SB_PingTree_DumpPath(const netadr_t *addr)
{
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	if (target == -1) {
		Com_Printf("No route found to given host\n");
	}
	else {
		nodeid_t current = target;

		Com_Printf("Shortest path length: %d\nRoute: \n", ping_nodes[current].dist);
		while (startnode_id != current) {
			byte *ip = ping_nodes[current].ipaddr.data;
			Com_Printf("%d.%d.%d.%d:%d (%d ms) < ", ip[0], ip[1], ip[2], ip[3],
				ntohs(ping_nodes[current].proxport), ping_nodes[current].dist);
			current = ping_nodes[current].prev;
		}
		Com_Printf("you\n");
	}
}
