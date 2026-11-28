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
#ifndef __SV_BROADCAST_H__
#define __SV_BROADCAST_H__

// The message prefix used by clients to trigger a broadcast.
#define BROADCAST_PREFIX ".qw "
#define BROADCAST_PREFIX_LEN (sizeof(BROADCAST_PREFIX) - 1)

// Prefix used in log messages related to broadcasts.
#define BROADCAST_LOG_PREFIX "BROADCAST:"

// When this comment was added, there were approximately 800 servers registered
// with the most popular master servers, so 4096 should be sufficient for the
// foreseeable future.
#define BROADCAST_MAX_SERVERS 4096

// Defines the number of milliseconds to wait before giving up on acquiring the
// server list lock.
#define BROADCAST_SERVER_LIST_LOCK_TIMEOUT 5000

// Wait up to 5 seconds before giving up on the queried master server.
#define BROADCAST_MASTER_TIMEOUT 5

// Updates the server list every 10 minutes.
#define BROADCAST_SERVER_LIST_UPDATE_INTERVAL 600.0

// Permits broadcasts every 30 seconds.
#define BROADCAST_MESSAGE_INTERVAL 30.0

// Maximum number of errors for a broadcast before giving up.
#define BROADCAST_MAX_ERRORS 5

// Limits the number of entries each IP address can occupy in the broadcast
// server list. This is a safeguard against master server poisoning. Currently,
// there is no server that uses more than 12 ports.
#define BROADCAST_SERVER_MAX_ENTRIES_PER_IP 25

// Maximum number of unique IP addresses to track for rate limiting. This is
// tied to the maximum number of broadcast servers supported.
#define BROADCAST_RATELIMIT_MAX_ENTRIES BROADCAST_MAX_SERVERS

// Time window in seconds for which rate limiting is enforced. Entries are
// limited to a maximum number of actions within this time frame.
#define BROADCAST_RATELIMIT_WINDOW_SECONDS 60

// Maximum number of allowed broadcast messages per IP address within
// the rate limiting window.
#define BROADCAST_RATELIMIT_LIMIT 10

// Maximum number of broadcast entries retained in the log history. When the
// limit is reached, the oldest entries are overwritten by new ones.
#define BROADCAST_LOG_MAX_ENTRIES 64

// Default number of broadcast log entries to display if no argument is
// provided.
#define BROADCAST_LOG_DEFAULT_DISPLAY_ENTRIES 10

// Maximum number of broadcast entries retained in the cache history. When this
// limit is reached, the oldest entries are overwritten by new ones. The cache
// messages are reset at the start of a match and printed when the match ends.
#define BROADCAST_CACHE_MAX_ENTRIES 16

// Default timeout (in milliseconds) for acquiring the servers update lock and
// the broadcast update lock.
#define BROADCAST_DEFAULT_LOCK_TIMEOUT 5000

qbool SV_Broadcast(char *message);
void SVC_Broadcast(void);
void SV_BroadcastEnabledOnChange(cvar_t *cvar, char *value, qbool *cancel);
void SV_BroadcastInit(void);
void SV_BroadcastUpdateServerList(qbool force_update);
void SV_BroadcastUpdateServerList_f(void);
void SV_BroadcastPrintLog_f(void);
void SV_BroadcastPrintCache(void);

#endif /* !__SV_BROADCAST_H__ */
