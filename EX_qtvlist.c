/*
Copyright (C) 2015 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <curl/curl.h>
#include <jansson.h>
#include <SDL_thread.h>
#include "quakedef.h"
#include "EX_qtvlist.h"

cvar_t qtv_api_url = {"qtv_api_url", "http://qtvapi.quakeworld.nu/api/v1/servers"};

static json_t *root;
static SDL_mutex *qtvlist_mutex;

extern char *CL_QTV_GetCurrentStream(void);

static size_t qtvlist_curl_callback(char *content, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = nmemb * size;
	struct str_buf *buf = (struct str_buf *)userp;
	char *tmpstr = NULL;

	if (buf->len > (4*1024*1024)) { /* Some sort of sanity check */
		Com_Printf("error: file too big\n");
		return -1;
	}

	tmpstr = Q_realloc(buf->str, buf->len + realsize + 1);
	if (tmpstr == NULL) {
		Com_Printf("error: Out of memory (realloc returned NULL)\n");
		return 0;
	}
	else {
		buf->str = tmpstr;
	}

	memcpy(&(buf->str[buf->len]), content, realsize);
	buf->len += realsize;
	buf->str[buf->len] = 0;

	return realsize;
}

/* Caller must free */
static char* qtvlist_get_jsondata(void)
{
	CURL *handle;
	struct str_buf buf;
	int res = 0;

	memset(&buf, 0, sizeof(buf));

	if ((handle = curl_easy_init()) == NULL) {
		Com_Printf("error: failed to init curl\n");
		return NULL;
	}

	buf.str = Q_calloc(1, 128); /*  Initially set to 128 bytes, will grow if necessary */

	res += curl_easy_setopt(handle, CURLOPT_URL, qtv_api_url.string);
	res += curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void*)&buf);
	res += curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, qtvlist_curl_callback);

	res += curl_easy_perform(handle);

	if (res != CURLE_OK) {
		Com_Printf("error: Failed to fetch qtv list JSON data\n");
		Q_free(buf.str); /* Will set to NULL */
	}
	
	curl_easy_cleanup(handle);
	return buf.str;
}

static void qtvlist_json_load_and_verify_string(const char *input)
{
	json_t *server_array = NULL;
	json_t *server_count = NULL;
	json_t *player_count = NULL;
	json_t *observ_count = NULL;
	json_error_t error;

	if (root != NULL) {
		json_decref(root);
		root = NULL;
	}

	root = json_loads(input, 0, &error);
	if (root == NULL) {
		Com_Printf("error: JSON error on line %d: %s\n", error.line, error.text);
		goto err;
	}

	if (!json_is_object(root)) {
		Com_Printf("error: invalid JSON, root is not an object\n");
		goto err;
	}

	server_array = json_object_get(root, "Servers");
	server_count = json_object_get(root, "ServerCount");
	player_count = json_object_get(root, "PlayerCount");
	observ_count = json_object_get(root, "ObserverCount");

	if (!json_is_array(server_array)  || !json_is_number(server_count) ||
	    !json_is_number(player_count) || !json_is_number(observ_count)) {
		Com_Printf("error: invalid JSON, unsupported format\n");
		goto err;
	}

	return;
err:
	if (root != NULL) {
		json_decref(root);
		root = NULL;
	}
}

#if 0
/* FIXME Verify JSON entries before printing + mutex lock it */
static void qtvlist_print_server_and_qtvaddress_list(void)
{
	int i, j;
	json_t *server_array, *server_entry, *gs_array, *gs_entry;

	server_array = json_object_get(root, "Servers");
	
	for (i = 0; i < json_array_size(server_array); i++) {
		server_entry = json_array_get(server_array, i);
		gs_array = json_object_get(server_entry, "GameStates");

		for (j = 0; j < json_array_size(gs_array); j++) {
			gs_entry = json_array_get(gs_array, j);
			Com_Printf("%s:%" JSON_INTEGER_FORMAT " %s\n", json_string_value(json_object_get(gs_entry, "Hostname")),
						json_integer_value(json_object_get(gs_entry, "Port")),
						json_string_value(json_object_get(gs_entry, "Link")));
		}
	}
}
#endif

static const char *qtvlist_get_qtvaddress(const char *qwserver, short port)
{
	int i, j;
	json_t *server_array, *server_entry, *gs_array, *gs_entry;
	const char *hostname, *ipaddress;

	if (qwserver == NULL) {
		return NULL;
	}

	if (root == NULL) {
		Com_Printf("error: qtv list data not initialized\n");
		return NULL;
	}

	server_array = json_object_get(root, "Servers");
	
	for (i = 0; i < json_array_size(server_array); i++) {
		server_entry = json_array_get(server_array, i);
		gs_array = json_object_get(server_entry, "GameStates");

		for (j = 0; j < json_array_size(gs_array); j++) {
			gs_entry = json_array_get(gs_array, j);
			if (gs_entry == NULL) {
				continue;
			}

			hostname = json_string_value(json_object_get(gs_entry, "Hostname"));
			ipaddress = json_string_value(json_object_get(gs_entry, "IpAddress"));
			if (hostname == NULL || ipaddress == NULL) {
				continue;
			}

			if (strcmp(qwserver, hostname) == 0 || strcmp(qwserver, ipaddress) == 0) {
				if ((short)json_integer_value(json_object_get(gs_entry, "Port")) == port) {
					return json_string_value(json_object_get(gs_entry, "Link"));
				}
			}
		}
	}

	return NULL;
}

static void qtvlist_find_player(const char *name, qbool list_all)
{
	json_t *server_array, *server_entry, *gs_array, *gs_entry;
	json_t *players_array, *player_entry;
	const char *player_name;
	int i,j,k;
	unsigned short found = 0;

	
	if (name == NULL) {
		return;
	}

	if (root == NULL) {
		Com_Printf("error: qtvlist data not initialized\n");
		return;
	}

	server_array = json_object_get(root, "Servers");

	if (server_array == NULL) {
		Com_Printf("error: invalid qtvlist json data\n");
		return;
	}

	for (i = 0; i < json_array_size(server_array); i++) {
		server_entry = json_array_get(server_array, i);
		if (server_entry == NULL) {
			Com_Printf("error: invalid qtvlist json data\n");
			return;
		}

		gs_array = json_object_get(server_entry, "GameStates");
		if (gs_array == NULL || !json_is_array(gs_array)) {
			Com_Printf("error: malformed qtvlist json data\n"); /* FIXME: Make better error prints */
			return;
		}

		for (j = 0; j < json_array_size(gs_array); j++) {
			gs_entry = json_array_get(gs_array, j);
			if (gs_entry == NULL) {
				continue;
			}

			players_array = json_object_get(gs_entry, "Players");
			if (players_array == NULL || !json_is_array(players_array)) {
				continue;
				/* FIXME: Print some debug stuff atleast ?? */
			}

			for (k = 0; k < json_array_size(players_array); k++) {
				player_entry = json_array_get(players_array, k);
				if (player_entry == NULL) {
					continue;
				}

				player_name = json_string_value(json_object_get(player_entry, "Name"));
				if (player_name) {
					if (list_all || strstri(player_name, name) != NULL) {
						found++;
						Com_Printf("&cff4%15s&r - %s:%" JSON_INTEGER_FORMAT "\n", player_name, json_string_value(json_object_get(gs_entry, "Hostname")), json_integer_value(json_object_get(gs_entry, "Port")));
					}
				}
			}
		}
	}

	if (list_all) {
		Com_Printf("Listing all players\n");
	} else if (found == 0) {
		Com_Printf("Found no players matching: \"%s\"\n", name);
	}
}

static void qtvlist_get_gameaddress(const char *qtvaddress, char *out_addr, size_t out_addr_len)
{
	int i, j;
	json_t *server_array, *server_entry, *gs_array, *gs_entry;
	const char *ipaddress, *link;
	json_int_t port;

	if (qtvaddress == NULL) {
		goto err;
	}

	if (root == NULL) {
		Com_Printf("error: qtv list data not initialized\n");
		goto err;
	}

	server_array = json_object_get(root, "Servers");

	if (server_array == NULL) {
		Com_Printf("error: invalid qtvlist json data\n");
		goto err;
	}
	
	for (i = 0; i < json_array_size(server_array); i++) {
		server_entry = json_array_get(server_array, i);
		gs_array = json_object_get(server_entry, "GameStates");

		for (j = 0; j < json_array_size(gs_array); j++) {
			gs_entry = json_array_get(gs_array, j);
			if (gs_entry == NULL) {
				continue;
			}

			link = json_string_value(json_object_get(gs_entry, "Link"));
			if (link == NULL) {
				continue;
			}

			if (strcmp(link, qtvaddress) == 0) {
				ipaddress = json_string_value(json_object_get(gs_entry, "IpAddress"));
				if (ipaddress == NULL) {
					continue;
				}

				port = json_integer_value(json_object_get(gs_entry, "Port"));

				snprintf(out_addr, out_addr_len, "%s:%" JSON_INTEGER_FORMAT, ipaddress, port);
				return;
			}
		}
	}
err:
	*out_addr = 0;
}

static void qtvlist_qtv_cmd(void)
{
	char tmp[256] = {0};
	char *port;
	const char *qtvaddress;
	extern qbool connected_via_proxy;

	if (qtvlist_mutex == NULL) {
		Com_Printf("error: cannot read QTV list, mutex not initialized\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		/* No argument, use current connected server ip:port */
		if (cls.state < ca_connected) {
			Com_Printf("error: not connected to a server\n");
			return;
		}

		/* FIXME: It's pretty ugly, refactor all this so that we can for sure
		 * keep track of where we are connected, what kind of endpoint it is and
		 * also which server we asked the proxy to connect to...
		 */
		if (connected_via_proxy) {
			/* If connected through proxy, the target server is 
			 * hopefully still in userinfo/prx
			 */
			char *prx = Info_ValueForKey(cls.userinfo, "prx");
			char *srv = strrchr(prx, '@');
			if (srv) {
				srv++;
			} else {
				srv = prx;
			}

			if (!prx[0]) {
				Com_Printf("error: connected through proxy but missing 'prx' userinfo, can't find QTV stream\n");
				return;
			}
			strlcpy(&tmp[0], srv, sizeof(tmp));
		} else {
			strlcpy(&tmp[0], NET_AdrToString(cls.server_adr), sizeof(tmp));
		}
	} else if (Cmd_Argc() == 2) {
		/* User provided which qwserver to find a QTV stream address for */
		strlcpy(&tmp[0], Cmd_Argv(1), sizeof(tmp));
	} else {
		Com_Printf("usage: %s [qwserver:port]\n", Cmd_Argv(0));
		return;
	}

	port = strchr(&tmp[0], ':');
	if (port == NULL) {
		port = "27500";
	} else {
		*port = 0;
		port++;
	}

	if (SDL_TryLockMutex(qtvlist_mutex) != 0) {
		Com_Printf("QTV list is being updated, please try again soon\n");
		return;
	}

	qtvaddress = qtvlist_get_qtvaddress((const char*)&tmp[0], Q_atoi(port));

	if (qtvaddress != NULL) {
		Cbuf_AddText("qtvplay ");
		Cbuf_AddText(qtvaddress);
		Cbuf_AddText("\n");
	} else {
		Com_Printf("No QTV stream address found for '%s:%s'\n", &tmp[0], port);
	}

	SDL_UnlockMutex(qtvlist_mutex);
}

static int qtvlist_update(void *unused)
{
	char *jsondata = NULL;
	int ret = -1;
	int res;
	(void)unused;

	res = SDL_TryLockMutex(qtvlist_mutex);

	if (res == SDL_MUTEX_TIMEDOUT) {
		Com_Printf("The qtvlist is already in the process of being updated\n");
		goto out;
	} else if (res < 0) {
		Com_Printf("error: mutex lock failed (SDL2): %s\n", SDL_GetError());
		goto out;
	}

	jsondata = qtvlist_get_jsondata();
	if (jsondata == NULL) {
		goto out;
	}

	qtvlist_json_load_and_verify_string(jsondata);
	Q_free(jsondata);

	if (root == NULL) {
		goto out;
	}

	ret = 0;
out:
	if (res == 0) {
		SDL_UnlockMutex(qtvlist_mutex);
	}
	return ret;
}

static void qtvlist_find_player_cmd(void)
{
	qbool list_all = false;

	if (qtvlist_mutex == NULL) {
		Com_Printf("error: cannot read QTV list, mutex not initialized\n");
		return;
	}

	if (Cmd_Argc() == 1) {
		list_all = true;
	} else if (Cmd_Argc() > 2) {
		Com_Printf("usage: find [nickname] (empty arg lists all)\n");
		return;
	}

	if (SDL_TryLockMutex(qtvlist_mutex) != 0) {
		Com_Printf("Player list is being updated, please try again soon\n");
		return;
	}

	if (list_all) {
		qtvlist_find_player("", true);
	} else {
		qtvlist_find_player(Cmd_Argv(1), false);
	}

	SDL_UnlockMutex(qtvlist_mutex);
}

static void qtvlist_spawn_updater(void)
{
	SDL_Thread *qtvlist_thread;

	if (qtvlist_mutex == NULL) {
		Com_Printf("error: cannot update QTV list, mutex not initialized\n");
		return;
	}

	qtvlist_thread = SDL_CreateThread(qtvlist_update, "qtvupdater", (void*)NULL);
	if (qtvlist_thread == NULL) {
		Com_Printf("error: failed to initialize qtvlist thread\n");
		Com_Printf("error: qtv/observeqtv commands may not work...\n");
		return;
	}

	SDL_DetachThread(qtvlist_thread);
}

void qtvlist_joinfromqtv_cmd(void)
{
	/* FIXME: Make this prettier */
	char addr[512];
	char httpaddr[512];
	char gameaddress[512];
	char *currstream, *server;

	currstream = CL_QTV_GetCurrentStream();
	if (currstream == NULL) {
		Com_Printf("Not connected to a QTV, can't join\n");
		return;
	}

	strlcpy(&addr[0], currstream, sizeof(addr));
	/* Bleh, transformation of id@server:port to http://server:port/watch.qtv?sid=id */
	server = strchr(&addr[0], '@');
	if (server == NULL) {
		Com_Printf("error: wrong format on input\n");
		return;
	}
	*server++ = 0;
	
	snprintf(&httpaddr[0], sizeof(httpaddr), "http://%s/watch.qtv?sid=%s", server, &addr[0]);

	if (SDL_TryLockMutex(qtvlist_mutex) != 0) {
		Com_Printf("qtvlist is being updated, please try again soon\n");
		return;
	}
	qtvlist_get_gameaddress((const char*)&httpaddr[0], &gameaddress[0], sizeof(gameaddress));
	SDL_UnlockMutex(qtvlist_mutex);

	if (gameaddress[0] != 0) {
		Cbuf_AddText(va("connect %s\n", &gameaddress[0]));
	} else {
		Com_Printf("No game address found for this QTV stream\n");
	}
}

void qtvlist_init(void)
{
	Cmd_AddCommand("qtv", qtvlist_qtv_cmd);
	Cmd_AddCommand("find", qtvlist_find_player_cmd);
	Cmd_AddCommand("find_update", qtvlist_spawn_updater);
	Cmd_AddCommand("observeqtv", qtvlist_qtv_cmd); /* For backwards compat */
	Cmd_AddCommand("qtv_update", qtvlist_spawn_updater);

	Cvar_SetCurrentGroup(CVAR_GROUP_QTV);
	Cvar_Register(&qtv_api_url);
	Cvar_ResetCurrentGroup();

	qtvlist_mutex = SDL_CreateMutex();
	if (qtvlist_mutex == NULL) {
		Com_Printf("error: failed to initialize qtvlist mutex\n");
		Com_Printf("error: qtv/observeqtv commands won't work...\n");
		return;
	}

	/* Initialize by running the updater at startup */
	qtvlist_spawn_updater();
}

void qtvlist_deinit(void)
{
	if (root != NULL) {
		json_decref(root);
		root = NULL;
	}

	if (qtvlist_mutex != NULL) {
		SDL_UnlockMutex(qtvlist_mutex);
		SDL_DestroyMutex(qtvlist_mutex);
	}
}


