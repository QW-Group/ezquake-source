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

#define PROXY_PINGLIST_QUERY "\xff\xff\xff\xffpingstatus"

// current amount of qw servers ~ 300
#define INVALID_NODE (-1)
typedef short nodeid_t;

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
} ping_node_t;

typedef struct ping_neighbour_t {
	nodeid_t id;
	dist_t dist;
} ping_neighbour_t;

static ping_node_t ping_nodes[MAX_SERVERS];
static nodeid_t ping_nodes_count = 0;
static ping_neighbour_t ping_neighbours[MAX_SERVERS*MAX_NONLEAVES];
static nodeid_t ping_neighbours_count = 0;

static nodeid_t startnode_id = 0;

static qbool building_pingtree = false; // when true, the pingtree build thread is still working
static qbool pingtree_built = false;

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

	return INVALID_NODE;
}

static qbool SB_PingTree_HasIp(ipaddr_t ipaddr)
{
	return SB_PingTree_FindIp(ipaddr) >= 0;
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
	ipaddr_t dummy;
	dummy.data[0] = 0;
	return dummy;
}

static void SB_PingTree_AddSelf(void)
{
	startnode_id = SB_PingTree_AddNode(SB_DummyIpAddr(), 0);
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
	nodeid_t node_id = INVALID_NODE;

	if (data->ping >= 0) {
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

static void SB_Proxy_ParseReply(const byte *buf, int buflen, proxy_ping_report_callback callback)
{
	int entries = buflen / 8;
	int i;

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

		callback(adr, dist);
	}
}

void SB_Proxy_QueryForPingList(const netadr_t *address, proxy_ping_report_callback callback)
{
	socket_t sock;
	char packet[] = PROXY_PINGLIST_QUERY;
	byte buf[MAX_SERVERS*8];
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

static void SB_PingTree_ScanProxies(void)
{
	int i;

	// scan proxies
	for (i = 0; i < ping_nodes_count; i++) {
		if (ping_nodes[i].proxport) {
			netadr_t adr;
			adr.type = NA_IP;
			adr.port = ping_nodes[i].proxport;
			memcpy(adr.ip, ping_nodes[i].ipaddr.data, 4);

			ping_nodes[i].nlist_start = ping_neighbours_count;
			SB_Proxy_QueryForPingList(&adr, SB_PingTree_AddProxyPing);
			ping_nodes[i].nlist_end = ping_neighbours_count;
		}
	}
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
	SB_PingTree_Init();
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

DWORD WINAPI SB_PingTree_Phase2(void *ignored_arg)
{
	SB_PingTree_ScanProxies();
	SB_PingTree_Dijkstra();
	SB_PingTree_UpdateServerList();

	Com_Printf("Ping tree has been created\n");
	building_pingtree = false;
	pingtree_built = true;
	return 0;
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
	Com_Printf("Building the Ping Tree...\n");

	// first quick phase is initialization + quick read of data from the server browser
	SB_PingTree_Phase1();
	// second longer phase is querying the proxies for their ping data + dijkstra algo
	Sys_CreateThread(SB_PingTree_Phase2, NULL);
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

		Com_Printf("Shortest path length: %d\nRoute: \n", ping_nodes[current].dist);
		while (current != startnode_id && current != INVALID_NODE) {
			byte *ip = ping_nodes[current].ipaddr.data;
			Com_Printf("%d.%d.%d.%d:%d (%d ms) < ", ip[0], ip[1], ip[2], ip[3],
				ntohs(ping_nodes[current].proxport), ping_nodes[current].dist);
			current = ping_nodes[current].prev;
		}
		Com_Printf("you\n");
	}
}

/// Connects to given QW server using the best available route
void SB_PingTree_ConnectBestPath(const netadr_t *addr)
{
	extern cvar_t cl_proxyaddr;
	nodeid_t target = SB_PingTree_FindIp(SB_Netaddr2Ipaddr(addr));

	if (target == INVALID_NODE || ping_nodes[target].prev == INVALID_NODE) {
		Com_Printf("No route found, trying to connect directly...");
		Cvar_Set(&cl_proxyaddr, "");
	}
	else if (ping_nodes[target].prev == startnode_id) {
		Com_Printf("Direct route is the best route, connecting directly...");
		Cvar_Set(&cl_proxyaddr, "");
		Cbuf_AddText(va("connect %s\n", NET_AdrToString(*addr)));
	}
	else {
		char proxylist_buf[32*MAX_NONLEAVES] = "";
		nodeid_t current = ping_nodes[target].prev;
		int proxies = 0;

		while (current != startnode_id && current != INVALID_NODE) {
			byte *ip = ping_nodes[current].ipaddr.data;
			char *newval = va("%d.%d.%d.%d:%d%s%s", (int) ip[0], (int) ip[1], (int) ip[2],
				(int) ip[3], (int) ntohs(ping_nodes[current].proxport), *proxylist_buf ? "@" : "", proxylist_buf);
			strlcpy(proxylist_buf, newval, 32*MAX_NONLEAVES);

			proxies++;
			current = ping_nodes[current].prev;
		}
		
		Com_Printf("Connecting using %d %s, with best ping %d ms\n",
			proxies, ((proxies == 1) ? "proxy" : "proxies"), ping_nodes[target].dist);
		Cvar_Set(&cl_proxyaddr, proxylist_buf);
	}

	Cbuf_AddText(va("connect %s\n", NET_AdrToString(*addr)));
}
