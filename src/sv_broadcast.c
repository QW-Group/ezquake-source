/*
 * Copyright (C) 2025 Oscar Linderholm <osm@recv.se>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "qwsvdef.h"
#include "sv_broadcast.h"

typedef struct {
	char packet[1024];
	int length;
} broadcast_packet_t;

typedef struct {
	unsigned int ip;
	unsigned int count;
	time_t window_start;
} ratelimit_t;

typedef struct {
	char message[1024];
	date_t timestamp;

} broadcast_log_t;

static int SV_BroadcastSend(void *data);
static int SV_BroadcastQueryMasters(void *data);
static void SV_BroadcastQueryMaster(socket_t sock, netadr_t *naddr, netadr_t *server, int *server_count);
static qbool SVC_BroadcastIsRateLimited(netadr_t *from);
static void SV_BroadcastAddLog(char *msg);
static void SV_BroadcastAddCache(char *msg);

static sys_mutex_t servers_update_lock;
static double last_servers_update;
static qbool update_in_progress;

static sys_mutex_t broadcast_lock;
static double last_broadcast;
static qbool broadcast_in_progress;

static sys_mutex_t server_list_lock;
static netadr_t server_list[BROADCAST_MAX_SERVERS];
static int server_list_count;

static ratelimit_t ratelimit[BROADCAST_RATELIMIT_MAX_ENTRIES];

static broadcast_log_t broadcast_log[BROADCAST_LOG_MAX_ENTRIES];
static int broadcast_log_head = 0;
static int broadcast_log_count = 0;

static broadcast_log_t broadcast_cache[BROADCAST_CACHE_MAX_ENTRIES];
static int broadcast_cache_head = 0;
static int broadcast_cache_count = 0;

static qbool Sys_MutexTryLockWithTimeout(sys_mutex_t *m, unsigned long timeout_ms)
{
	unsigned long elapsed = 0;
	unsigned long sleep_interval = 1;

	while (elapsed < timeout_ms)
	{
		if (Sys_MutexTryLock(m))
		{
			return true;
		}

		Sys_MSleep(sleep_interval);
		elapsed += sleep_interval;

		if (sleep_interval < 32)
		{
			sleep_interval *= 2;
		}
	}

	return false;
}

void SV_BroadcastInit(void)
{
	Cmd_AddCommand("updatebroadcasts", SV_BroadcastUpdateServerList_f);

	Sys_MutexInit(&broadcast_lock);
	last_broadcast = -99999;
	broadcast_in_progress = false;

	Sys_MutexInit(&servers_update_lock);
	last_servers_update = -99999;
	update_in_progress = false;

	Sys_MutexInit(&server_list_lock);
}

void SV_BroadcastUpdateServerList_f(void)
{
	SV_BroadcastUpdateServerList(true);
}

void SV_BroadcastEnabledOnChange(cvar_t *cvar, char *value, qbool *cancel)
{
	// We can't use the CVAR_SERVERINFO flag in this case because we want
	// the key to be named "broadcast" not "sv_broadcast_enabled".
	// Therefore, the cvar will not have this flag set, instead, we'll
	// trigger the serverinfo update manually.
	SV_ServerinfoChanged("broadcast", value);
}

void SV_BroadcastUpdateServerList(qbool force_update)
{
	extern netadr_t master_adr[MAX_MASTERS];
	extern cvar_t sv_broadcast_enabled;
	netadr_t *masters;

	if (!sv_broadcast_enabled.value)
	{
		if (force_update)
		{
			Con_Printf("SV_BroadcastUpdateServerList: "
				"Cannot update broadcast servers, sv_broadcast_enabled is off\n");
		}
		return;
	}

	if (GameStarted())
	{
		if (force_update)
		{
			Con_Printf("SV_BroadcastUpdateServerList: "
				"Cannot update broadcast servers, a game has already started\n");
		}
		return;
	}

	if (!Sys_MutexTryLockWithTimeout(&servers_update_lock, BROADCAST_DEFAULT_LOCK_TIMEOUT))
	{
		Con_Printf("SV_BroadcastUpdateServerList: Failed to acquire servers update lock\n");
		return;
	}

	if (!force_update && Sys_DoubleTime() - last_servers_update < BROADCAST_SERVER_LIST_UPDATE_INTERVAL)
	{
		Sys_MutexUnlock(&servers_update_lock);
		return;
	}

	if (!master_adr[0].port)
	{
		last_servers_update = Sys_DoubleTime();
		Sys_MutexUnlock(&servers_update_lock);
		Con_Printf("SV_BroadcastUpdateServerList: No master servers configured\n");
		return;
	}

	if (update_in_progress)
	{
		if (force_update)
		{
			Con_Printf("Broadcast update already in progress\n");
		}
		Sys_MutexUnlock(&servers_update_lock);
		return;
	}

	update_in_progress = true;
	Sys_MutexUnlock(&servers_update_lock);

	masters = Q_malloc(sizeof(master_adr));
	memcpy(masters, master_adr, sizeof(master_adr));
	Con_Printf("Broadcast server list sync started\n");

	if (Sys_CreateDetachedThread(SV_BroadcastQueryMasters, masters))
	{
		Con_Printf("SV_BroadcastUpdateServerList: Unable to start query masters thread\n");
		Q_free(masters);

		Sys_MutexLock(&servers_update_lock);
		update_in_progress = false;
		last_servers_update = Sys_DoubleTime();
		Sys_MutexUnlock(&servers_update_lock);
	}
}

static int SV_BroadcastQueryMasters(void *data)
{
	static netadr_t servers[BROADCAST_MAX_SERVERS];
	netadr_t *masters = data;
	int server_count = 0;
	socket_t sock;
	int i = 0;

#ifdef _WIN32
	const int timeout = BROADCAST_MASTER_TIMEOUT * 1000;
#else
	const struct timeval timeout = {BROADCAST_MASTER_TIMEOUT, 0};
#endif

	sock = Sys_OpenSocket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		Sys_Printf("SV_BroadcastQueryMasters: Unable to initialize socket\n");
		goto out;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) != 0)
	{
		Sys_Printf("SV_BroadcastQueryMasters: Unable to set timeout on socket\n");
		goto cleanup;
	}

	memset(servers, 0, sizeof(servers));

	for (i = 0; i < MAX_MASTERS; i++)
	{
		if (!masters[i].port)
		{
			break;
		}

		SV_BroadcastQueryMaster(sock, &masters[i], servers, &server_count);
	}

	if (!Sys_MutexTryLockWithTimeout(&server_list_lock, BROADCAST_SERVER_LIST_LOCK_TIMEOUT))
	{
		Sys_Printf("SV_BroadcastQueryMasters: Failed to acquire server_list_lock, aborting\n");
		goto cleanup;
	}
	server_list_count = server_count;
	memcpy(server_list, servers, sizeof(netadr_t) * server_list_count);
	Sys_MutexUnlock(&server_list_lock);
	Sys_Printf("Broadcast server list sync complete: %d server(s) available.\n", server_list_count);

cleanup:
	closesocket(sock);
out:
	Q_free(masters);
	Sys_MutexLock(&servers_update_lock);
	update_in_progress = false;
	last_servers_update = Sys_DoubleTime();
	Sys_MutexUnlock(&servers_update_lock);

	return 0;
}

static void SV_BroadcastQueryMaster(socket_t sock, netadr_t *naddr, netadr_t *servers, int *server_count)
{
	// Payload to send to the master server, which returns a list of all
	// registered servers.
	static const char payload[] = { 0x63 };

	// This is the header we expect to receive from the master server.
	static const char header[] = { 0xff, 0xff, 0xff, 0xff, 0x64, 0x0a };

	const size_t header_size = sizeof(header);
	const int addr_size = 6;
	struct sockaddr_storage addr;
	netadr_t new_addr;
	qbool exists = false;
	char buf[1024*64] = {0};
	char master[32];
	int ip_count = 0;
	int offset = 0;
	int ret = 0;
	int i = 0;

	NetadrToSockadr(naddr, &addr);
	snprintf(master, sizeof(master), "%u.%u.%u.%u:%u",
		naddr->ip[0], naddr->ip[1], naddr->ip[2], naddr->ip[3], ntohs(naddr->port));

	ret = sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)&addr,
		sizeof(struct sockaddr_in));
	if (ret < 0)
	{
		Sys_Printf("SV_BroadcastQueryMaster: Unable to query %s\n", master);
		return;
	}

	ret = recvfrom(sock, buf, sizeof(buf), 0, NULL, NULL);
	if (ret <= 0)
	{
		Sys_Printf("SV_BroadcastQueryMaster: No data received from %s\n", master);
		return;
	}

	if (ret < header_size)
	{
		Sys_Printf("SV_BroadcastQueryMaster: Unexpected data returned from %s\n", master);
		return;
	}

	if (memcmp(buf, header, header_size) != 0)
	{
		Sys_Printf("SV_BroadcastQueryMaster: Unexpected header returned from %s\n", master);
		return;
	}

	if ((ret - header_size) % addr_size != 0)
	{
		Sys_Printf("SV_BroadcastQueryMaster: Invalid data received from %s\n", master);
		return;
	}

	for (offset = header_size; offset < ret; offset += addr_size)
	{
		if (*server_count >= BROADCAST_MAX_SERVERS)
		{
			Sys_Printf("SV_BroadcastQueryMaster: Server list is full!\n");
			break;
		}

		memcpy(&new_addr.ip, &buf[offset], 4);
		memcpy(&new_addr.port, &buf[offset + 4], 2);
		exists = false;
		ip_count = 0;

		// There are so few servers, so we'll accept the cost of
		// iterating over all entries.
		for (i = 0; i < *server_count; i++)
		{
			if (NET_CompareAdr(new_addr, servers[i]))
			{
				exists = true;
				break;
			}

			if (NET_CompareBaseAdr(new_addr, servers[i]))
			{
				if (ip_count >= BROADCAST_SERVER_MAX_ENTRIES_PER_IP)
				{
					break;
				}

				ip_count++;
			}
		}

		if (exists)
		{
			continue;
		}

		if (ip_count >= BROADCAST_SERVER_MAX_ENTRIES_PER_IP)
		{
			Sys_Printf("SV_BroadcastQueryMaster: Ignoring %u.%u.%u.%u:%u (IP limit reached)\n",
				new_addr.ip[0], new_addr.ip[1], new_addr.ip[2], new_addr.ip[3], ntohs(new_addr.port));
			continue;
		}

		memcpy(servers[*server_count].ip, &new_addr.ip, 4);
		servers[*server_count].port = new_addr.port;
		(*server_count)++;
	}
}

qbool SV_Broadcast(char *message)
{
	extern cvar_t sv_broadcast_enabled;
	broadcast_packet_t *job;
	client_t *client;
	int i, written, players = 0;
	int count;
	double elapsed;

	if (!sv_broadcast_enabled.value)
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Broadcasting is not enabled on this server\n");
		return false;
	}

	if (GameStarted())
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Broadcasting is not available during games\n");
		return false;
	}

	if (!Sys_MutexTryLockWithTimeout(&server_list_lock, BROADCAST_SERVER_LIST_LOCK_TIMEOUT))
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Failed to acquire server list lock\n");
		return false;
	}
	count = server_list_count;
	Sys_MutexUnlock(&server_list_lock);
	if (!count)
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "No broadcast servers configured\n");
		return false;
	}

	if (!Sys_MutexTryLockWithTimeout(&broadcast_lock, BROADCAST_DEFAULT_LOCK_TIMEOUT))
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Failed to acquire broadcast lock\n");
		return false;
	}
	elapsed = Sys_DoubleTime() - last_broadcast;
	if (elapsed < BROADCAST_MESSAGE_INTERVAL - 1)
	{
		Sys_MutexUnlock(&broadcast_lock);
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Wait %d seconds before broadcasting again\n",
			(int)(BROADCAST_MESSAGE_INTERVAL - elapsed));
		return false;
	}

	if (broadcast_in_progress)
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "A broadcast is already in progress\n");
		Sys_MutexUnlock(&broadcast_lock);
		return false;
	}

	broadcast_in_progress = true;
	Sys_MutexUnlock(&broadcast_lock);

	// Build the payload on the server thread. The detached worker only sends it.
	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (client->state >= cs_connected && !client->spectator)
			players++;
	}

	job = Q_malloc(sizeof(*job));
	memset(job->packet, 0xff, 4);
	written = snprintf(job->packet + 4, sizeof(job->packet) - 4,
		"broadcast "
		"\\hostport\\%s"
		"\\port\\%d"
		"\\name\\%s"
		"\\players\\%d"
		"\\maxplayers\\%d"
		"\\message\\%s\n",
		Cvar_String("hostport"), NET_UDPSVPort(), sv_client->name,
		players, (int)maxclients.value, message);
	if (written < 0 || written >= sizeof(job->packet) - 4)
	{
		Q_free(job);
		Sys_MutexLock(&broadcast_lock);
		broadcast_in_progress = false;
		Sys_MutexUnlock(&broadcast_lock);
		return false;
	}
	job->length = written + 4;

	if (Sys_CreateDetachedThread(SV_BroadcastSend, job))
	{
		SV_ClientPrintf(sv_client, PRINT_HIGH, "Unable to start broadcast thread\n");
		Q_free(job);
		Sys_MutexLock(&broadcast_lock);
		broadcast_in_progress = false;
		last_broadcast = Sys_DoubleTime();
		Sys_MutexUnlock(&broadcast_lock);
	}

	return true;
}

static int SV_BroadcastSend(void *data)
{
	broadcast_packet_t *job = data;
	struct sockaddr_storage addr;
	int err_count = 0;
	socket_t sock;
	int ret = 0;
	int i = 0;

	sock = Sys_OpenSocket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		Sys_Printf("SV_BroadcastSend: Unable to initialize socket\n");
		goto out;
	}

	if (!Sys_MutexTryLockWithTimeout(&server_list_lock, BROADCAST_SERVER_LIST_LOCK_TIMEOUT))
	{
		Sys_Printf("SV_BroadcastSend: Failed to acquire server_list_lock, aborting\n");
		goto close;
	}

	for (i = 0; i < server_list_count; i++)
	{
		NetadrToSockadr(&server_list[i], &addr);

		ret = sendto(sock, job->packet, job->length, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
		if (ret < 0)
		{
			const netadr_t *server = &server_list[i];

			Sys_Printf("SV_BroadcastSend: Unable to send broadcast to %u.%u.%u.%u:%u\n",
				server->ip[0], server->ip[1], server->ip[2], server->ip[3], ntohs(server->port));

			if (err_count++ == BROADCAST_MAX_ERRORS)
			{
				goto cancel;
			}
		}
	}

cancel:
	Sys_MutexUnlock(&server_list_lock);
close:
	closesocket(sock);
out:
	Sys_MutexLock(&broadcast_lock);
	last_broadcast = Sys_DoubleTime();
	broadcast_in_progress = false;
	Sys_MutexUnlock(&broadcast_lock);

	Q_free(job);

	return 0;
}

void SVC_Broadcast(void)
{
	extern cvar_t sv_broadcast_sender_validation_enabled;
	extern cvar_t sv_broadcast_enabled;
	client_t *client = NULL;
	qbool spectalk = false;
	qbool started = false;
	qbool valid = false;
	char addrport[22] = {0};
	char log[1024] = {0};
	char out[1024] = {0};
	char *displayaddr = NULL;
	char maxplayers[512];
	char hostport[512];
	char message[512];
	char *payload = NULL;
	char players[512];
	char *addr = NULL;
	char name[512];
	char port[512];
	int i = 0;

	if (!sv_broadcast_enabled.value || Cmd_Argc() < 1)
	{
		return;
	}

	if (SVC_BroadcastIsRateLimited(&net_from))
	{
		return;
	}

	payload = Cmd_Args();
	addr = NET_AdrToString(net_from);

	// If enabled, ensure that the sender's address exists in the list of
	// addresses received from the master server.
	if (sv_broadcast_sender_validation_enabled.value)
	{
		valid = false;

		if (!Sys_MutexTryLockWithTimeout(&server_list_lock, BROADCAST_SERVER_LIST_LOCK_TIMEOUT))
		{
			Con_Printf("SVC_Broadcast: Failed to acquire server_list_lock, aborting\n");
			return;
		}

		for (i = 0; i < server_list_count; i++)
		{
			if (NET_CompareBaseAdr(net_from, server_list[i]))
			{
				valid = true;
				break;
			}
		}

		Sys_MutexUnlock(&server_list_lock);

		if (!valid)
		{
			Con_Printf("Rejected broadcast from address: %s (payload: %s)\n",
				addr , payload);
			return;
		}
	}

	strlcpy(hostport, Info_ValueForKey(payload, "hostport"), sizeof(hostport));
	strlcpy(name, Info_ValueForKey(payload, "name"), sizeof(name));
	strlcpy(message, Info_ValueForKey(payload, "message"), sizeof(message));
	strlcpy(port, Info_ValueForKey(payload, "port"), sizeof(port));
	strlcpy(players, Info_ValueForKey(payload, "players"), sizeof(players));
	strlcpy(maxplayers, Info_ValueForKey(payload, "maxplayers"), sizeof(maxplayers));

	if (strlen(name) == 0 || strlen(message) == 0)
	{
		Con_Printf("Rejected broadcast with payload: %s (%s)\n", payload, addr);
		return;
	}

	if (strlen(hostport) > 0)
	{
		displayaddr = hostport;
	}
	else if (strlen(port) > 0)
	{
		snprintf(addrport, sizeof(addrport), "%s:%s", NET_BaseAdrToString(net_from), port);
		displayaddr = addrport;
	}
	else
	{
		displayaddr = addr;
	}

	if (strlen(players) > 0 && strlen(maxplayers) > 0)
	{
		// Example:
		// > tot.qwsv.net:27500 [6/8] ToT_Oddjob: prac now
		snprintf(out, sizeof(out), "%c %s %c%s/%s%c %s: %s",
			0x8d, displayaddr, 0x10, players, maxplayers, 0x11, name, message);
	}
	else
	{
		// Example:
		// [tot.qwsv.net:27500] ToT_Oddjob: prac now
		snprintf(out, sizeof(out), "[%s] %s: %s", displayaddr, name, message);
	}
	snprintf(log, sizeof(log), "%s \\addr\\%s%s\n", BROADCAST_LOG_PREFIX, addr, payload);

	SV_BroadcastAddLog(out);

	started = GameStarted();
	if (started)
	{
		SV_BroadcastAddCache(out);
	}

	Con_Printf("%s\n", out);
	SV_Write_Log(CONSOLE_LOG, 0, log);


	// If the KTX spectalk feature is enabled, we'll allow the broadcast
	// message to be seen by players, even if a game has already started.
	spectalk = strstr(Cvar_String("qwm_name"), "KTX") && Cvar_Value("k_spectalk");

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++)
	{
		if (client->state != cs_spawned || (started && !client->spectator && !spectalk))
		{
			continue;
		}

		SV_ClientPrintf2(client, PRINT_CHAT, "%s\n", out);
	}
}

qbool SVC_BroadcastIsRateLimited(netadr_t *from)
{
	struct sockaddr_storage addr;
	struct sockaddr_in *addr_in;
	time_t now = time(NULL);
	char *ip_str = NULL;
	unsigned int ip = 0;
	int next = -1;
	int i = 0;

	NetadrToSockadr(from, &addr);
	addr_in = (struct sockaddr_in *)&addr;
	ip = ntohl(addr_in->sin_addr.s_addr);
	ip_str = NET_BaseAdrToString(*from);

	for (i = 0; i < BROADCAST_RATELIMIT_MAX_ENTRIES; i++)
	{
		if (ratelimit[i].ip == ip)
		{
			// Enough time has passed, let's reset the window and
			// counter for the given address.
			if (now - ratelimit[i].window_start >= BROADCAST_RATELIMIT_WINDOW_SECONDS)
			{
				Con_DPrintf("SVC_BroadcastIsRateLimited: Reset ratelimit for %s\n",
					ip_str);
				ratelimit[i].count = 1;
				ratelimit[i].window_start = now;
				return false;
			}

			// The rate limit has been reached for the given IP,
			// let's reject it.
			if (ratelimit[i].count >= BROADCAST_RATELIMIT_LIMIT)
			{
				Con_DPrintf("SVC_BroadcastIsRateLimited: %s has been ratelimited\n",
					ip_str);
				return true;
			}

			// Increment the counter and proceed.
			ratelimit[i].count++;
			return false;
		}

		// Check whether or not we can evict the entry for this IP.
		if (ratelimit[i].ip != 0 &&
			now - ratelimit[i].window_start >= BROADCAST_RATELIMIT_WINDOW_SECONDS)
		{
			ratelimit[i].ip = 0;
			ratelimit[i].count = 0;
			ratelimit[i].window_start = 0;
		}

		// If next isn't set and the spot is available, we'll use it for
		// the next ratelimit entry.
		if (next == -1 && ratelimit[i].ip == 0)
		{
			next = i;
		}
	}

	// No available slots exists in the ratelimit array, if that's the case,
	// we'll just have to say that the IP is going to be ratelimited.
	if (next == -1)
	{
		Con_DPrintf("SVC_BroadcastIsRateLimited: All slots are full, unable to process %s\n",
			ip_str);
		return true;
	}

	ratelimit[next].ip = ip;
	ratelimit[next].count = 1;
	ratelimit[next].window_start = now;
	return false;
}

static void SV_BroadcastAddLog(char *msg)
{
	snprintf(broadcast_log[broadcast_log_head].message,
		sizeof(broadcast_log[broadcast_log_head].message), "%s", msg);
	SV_TimeOfDay(&broadcast_log[broadcast_log_head].timestamp, "%Y-%m-%d %H:%M:%S");

	broadcast_log_head = (broadcast_log_head + 1) % BROADCAST_LOG_MAX_ENTRIES;
	if (broadcast_log_count < BROADCAST_LOG_MAX_ENTRIES)
	{
		broadcast_log_count++;
	}
}

void SV_BroadcastPrintLog_f(void)
{
	int i = 0;
	int index = 0;
	int limit = 0;
	int start = 0;

	if (broadcast_log_count == 0)
	{
		Con_Printf("Broadcast log is empty.\n");
		return;
	}

	limit = (Cmd_Argc() > 1) ? atoi(Cmd_Argv(1)) : BROADCAST_LOG_DEFAULT_DISPLAY_ENTRIES;
	limit = bound(1, limit, broadcast_log_count);
	Con_Printf("List of last %d broadcasts:\n", limit);

	start = (broadcast_log_head - limit + BROADCAST_LOG_MAX_ENTRIES) % BROADCAST_LOG_MAX_ENTRIES;

	for (i = 0; i < limit; i++)
	{
		index = (start + i) % BROADCAST_LOG_MAX_ENTRIES;

		if (broadcast_log[index].timestamp.str[0] && broadcast_log[index].message[0])
		{
			Con_Printf("%s: %s\n",
				broadcast_log[index].timestamp.str, broadcast_log[index].message);
		}
	}
}

static void SV_BroadcastAddCache(char *msg)
{
	snprintf(broadcast_cache[broadcast_cache_head].message,
		sizeof(broadcast_cache[broadcast_cache_head].message), "%s", msg);
	SV_TimeOfDay(&broadcast_cache[broadcast_cache_head].timestamp, "%Y-%m-%d %H:%M:%S");

	broadcast_cache_head = (broadcast_cache_head + 1) % BROADCAST_CACHE_MAX_ENTRIES;
	if (broadcast_cache_count < BROADCAST_CACHE_MAX_ENTRIES)
	{
		broadcast_cache_count++;
	}
}

void SV_BroadcastPrintCache(void)
{
	int i = 0;
	int index = 0;
	int start = 0;

	if (broadcast_cache_count == 0)
	{
		return;
	}

	SV_BroadcastPrintf(PRINT_CHAT, "List of cached broadcasts:\n");

	start = (broadcast_cache_head - broadcast_cache_count + BROADCAST_CACHE_MAX_ENTRIES) % BROADCAST_CACHE_MAX_ENTRIES;

	for (i = 0; i < broadcast_cache_count; i++)
	{
		index = (start + i) % BROADCAST_CACHE_MAX_ENTRIES;

		if (broadcast_log[index].timestamp.str[0] && broadcast_log[index].message[0])
		{
			SV_BroadcastPrintf(PRINT_CHAT, "%s: %s\n",
				broadcast_cache[index].timestamp.str,
				broadcast_cache[index].message);
		}
	}

	broadcast_cache_head = 0;
	broadcast_cache_count = 0;
	memset(broadcast_cache, 0, sizeof(broadcast_log_t) * BROADCAST_CACHE_MAX_ENTRIES);
}
