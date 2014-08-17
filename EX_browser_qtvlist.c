/*
Copyright (C) 2010       ezQuake team

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

#include "quakedef.h"
#include "utils.h"
#include "expat.h"
#include <curl/curl.h>

#define QTVLIST_CACHE_FILE_DIR "ezquake/sb/cache"
#define QTVLIST_CACHE_FILE "qtvcache"
#define CHARACTERDATA_BUFFERSIZE 256
#define MAX_QTV_ENTRY_TEXTLEN CHARACTERDATA_BUFFERSIZE

cvar_t sb_qtvlist_url = { "sb_qtvlist_url", "http://qtv.quakeworld.nu/?rss" };

typedef struct sb_qtvplayer_s {
	struct sb_qtvplayer_s *next; // next item in the linked list of players

	char name[MAX_SCOREBOARDNAME];
	char team[MAX_SCOREBOARDNAME];
	int frags;
	int ping;
	int pl;
	int topcolor;
	int bottomcolor;
} sb_qtvplayer_t;

typedef struct sb_qtventry_s {
	struct sb_qtventry_s *next; // next item in the linked list of entries

	char title[MAX_QTV_ENTRY_TEXTLEN];
	char link[MAX_QTV_ENTRY_TEXTLEN];
	char hostname[MAX_QTV_ENTRY_TEXTLEN];
	int port;
	netadr_t addr;
	int observercount;
	sb_qtvplayer_t *players; // first item of a linked list
} sb_qtventry_t;

typedef enum {
	QTVLIST_INITIAL,
	QTVLIST_ITEM,
	QTVLIST_PLAYER,
} sb_qtvlist_parse_position_t;

typedef enum {
	QTVLIST_INIT,
	QTVLIST_PROCESSING,
	QTVLIST_DOWNLOADING,
	QTVLIST_PARSING,
	QTVLIST_RESOLVING,
	QTVLIST_PRINTING,
	QTVLIST_READY
} sb_qtvlist_status_t;

typedef struct sb_qtvlist_parse_state_s {
	char chardata_buffer[CHARACTERDATA_BUFFERSIZE];
	sb_qtventry_t *list;
	sb_qtvlist_parse_position_t position;
} sb_qtvlist_parse_state_t;

typedef struct sb_qtvlist_cache_s {
	sb_qtventry_t *sb_qtventries;
	sb_qtvlist_status_t status;
} sb_qtvlist_cache_t;

static sb_qtvlist_cache_t sb_qtvlist_cache = { NULL, QTVLIST_INIT };

static sb_qtventry_t *QTVList_New_Entry(void)
{
	sb_qtventry_t *ret;

	ret = Q_calloc(sizeof (sb_qtventry_t), 1);

	return ret;
}

static sb_qtvplayer_t *QTVList_New_Player(void)
{
	sb_qtvplayer_t *ret;

	ret = Q_calloc(sizeof (sb_qtvplayer_t), 1);

	return ret;
}

static void QTVList_Parse_StartElement(void *userData, const XML_Char *name,
	const XML_Char **atts)
{
	sb_qtvlist_parse_state_t *sb_qtvparse = (sb_qtvlist_parse_state_t *) userData;

	if (sb_qtvparse->position == QTVLIST_INITIAL) {
		if (strcmp(name, "item") == 0) {
			sb_qtventry_t *current = sb_qtvparse->list;
			sb_qtventry_t *newentry = QTVList_New_Entry();

			newentry->next = current;
			sb_qtvparse->list = newentry;
			sb_qtvparse->position = QTVLIST_ITEM;
		}
		else {
		}
	}
	else if (sb_qtvparse->position == QTVLIST_ITEM) {
		if (strcmp(name, "player") == 0) {
			sb_qtvplayer_t *current = sb_qtvparse->list->players;
			sb_qtvplayer_t *newplayer = QTVList_New_Player();

			newplayer->next = current;
			sb_qtvparse->list->players = newplayer;

			sb_qtvparse->position = QTVLIST_PLAYER;
		}
	}
	else if (sb_qtvparse->position == QTVLIST_PLAYER) {
	}
	else {
		Sys_Error("QTVList_Parse_StartElement(): invalid position");
	}
}

void QTVList_Parse_CharacterData(void *userData, const XML_Char *s, int len)
{
	sb_qtvlist_parse_state_t *sb_qtvparse = (sb_qtvlist_parse_state_t *) userData;

	size_t copied;
	size_t n;

	copied = strlen(sb_qtvparse->chardata_buffer);
	n = 0;

	while (copied + 1 < CHARACTERDATA_BUFFERSIZE && n < len) {
		sb_qtvparse->chardata_buffer[copied++] = s[n++];
	}

	sb_qtvparse->chardata_buffer[copied] = '\0';
}

void QTVList_Parse_EndElement(void *userData, const XML_Char *name)
{
	sb_qtvlist_parse_state_t *sb_qtvparse = (sb_qtvlist_parse_state_t *) userData;
	char *buf = str_trim(sb_qtvparse->chardata_buffer);

	if (sb_qtvparse->position == QTVLIST_INITIAL) {
	}
	else if (sb_qtvparse->position == QTVLIST_ITEM) {
		if (strcmp(name, "hostname") == 0) {
			strlcpy(sb_qtvparse->list->hostname, buf, MAX_QTV_ENTRY_TEXTLEN);
		}
		else if (strcmp(name, "port") == 0) {
			sb_qtvparse->list->port = Q_atoi(buf);
		}
		else if (strcmp(name, "observercount") == 0) {
			sb_qtvparse->list->observercount = Q_atoi(buf);
		}
		else if (strcmp(name, "title") == 0) {
			strlcpy(sb_qtvparse->list->title, buf, MAX_QTV_ENTRY_TEXTLEN);
		}
		else if (strcmp(name, "link") == 0) {
			strlcpy(sb_qtvparse->list->link, buf, MAX_QTV_ENTRY_TEXTLEN);
		}
		else if (strcmp(name, "item") == 0) {
			sb_qtvparse->position = QTVLIST_INITIAL; // go up
		}
		else if (strcmp(name, "player") == 0) {
			sb_qtvparse->position = QTVLIST_PLAYER; // go down
		}
	}
	else if (sb_qtvparse->position == QTVLIST_PLAYER) {
		if (strcmp(name, "name") == 0) {
			strlcpy(sb_qtvparse->list->players->name, buf, MAX_SCOREBOARDNAME);
		}
		else if (strcmp(name, "team") == 0) {
			strlcpy(sb_qtvparse->list->players->team, buf, MAX_SCOREBOARDNAME);
		}
		else if (strcmp(name, "frags") == 0) {
			sb_qtvparse->list->players->frags = Q_atoi(buf);
		}
		else if (strcmp(name, "ping") == 0) {
			sb_qtvparse->list->players->ping = Q_atoi(buf);
		}
		else if (strcmp(name, "pl") == 0) {
			sb_qtvparse->list->players->pl = Q_atoi(buf);
		}
		else if (strcmp(name, "topcolor") == 0) {
			sb_qtvparse->list->players->topcolor = Q_atoi(buf);
		}
		else if (strcmp(name, "bottomcolor") == 0) {
			sb_qtvparse->list->players->bottomcolor = Q_atoi(buf);
		}
		else if (strcmp(name, "player") == 0) {
			sb_qtvparse->position = QTVLIST_ITEM;
		}
	}

	buf[0] = '\0';
}

sb_qtvlist_parse_state_t *QTVList_Parse_Init(void)
{
	sb_qtvlist_parse_state_t *ret = Q_malloc(sizeof (sb_qtvlist_parse_state_t));
	ret->chardata_buffer[0] = '\0';
	ret->list = NULL;
	ret->position = QTVLIST_INITIAL;

	return ret;
}

void QTVList_Entry_Destroy(sb_qtventry_t *entry)
{
	while (entry->players) {
		sb_qtvplayer_t *next = entry->players->next;
		Q_free(entry->players);
		entry->players = next;
	}

	Q_free(entry);
}

void QTVList_Parse_Destroy(sb_qtvlist_parse_state_t *parse)
{
	while (parse->list) {
		sb_qtventry_t *next = parse->list->next;

		QTVList_Entry_Destroy(parse->list);
		parse->list = next;
	}
}

void QTVList_Process_Full_List(vfsfile_t *f, sb_qtvlist_parse_state_t *sb_qtvparse)
{
	XML_Parser parser = NULL;
	int len;
	enum XML_Status status;
	char buf[4096];
	vfserrno_t err;

    // initialize XML parser
    parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		Com_Printf("Couldn't initialize XML parser\n");
        return;
	}
    XML_SetStartElementHandler(parser, QTVList_Parse_StartElement);
	XML_SetCharacterDataHandler(parser, QTVList_Parse_CharacterData);
	XML_SetEndElementHandler(parser, QTVList_Parse_EndElement);
    XML_SetUserData(parser, (void *) sb_qtvparse);

    while ((len = VFS_READ(f, buf, 4096, &err)) > 0)
    {
		if ((status = XML_Parse(parser, buf, len, 0)) != XML_STATUS_OK) {
			enum XML_Error err = XML_GetErrorCode(parser);
			Com_Printf("XML parser error.\n%s\n", status, XML_ErrorString(err));
			break;
		}
    }

    XML_ParserFree(parser);
}

void QTVList_Cache_File_Download(void)
{
	CURL *curl;
	CURLcode res;
	FILE *f;

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, sb_qtvlist_url.string);
	}
	else {
		Com_Printf_State(PRINT_FAIL, "QTVList_List_f() Can't init cURL\n");
		return;
	}

	if (!FS_FCreateFile(QTVLIST_CACHE_FILE, &f, QTVLIST_CACHE_FILE_DIR, "wb+")) {
		Com_Printf_State(PRINT_FAIL, "Can't create QTVList cache file\n");
        return;
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

	res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
	fclose(f);

	if (res != CURLE_OK) {
		Com_Printf_State(PRINT_FAIL, "Couldn't download QTV list");
		return;
	}
}

static vfsfile_t *QTVList_Cache_File_Open(char *mode)
{
	return FS_OpenVFS(QTVLIST_CACHE_FILE_DIR "/" QTVLIST_CACHE_FILE, mode, FS_BASE);
}

static void QTVList_Resolve_Hostnames(void)
{
	sb_qtventry_t *cur = sb_qtvlist_cache.sb_qtventries;

	while (cur) {
		NET_StringToAdr(va("%s:%d", cur->hostname, cur->port), &cur->addr);
		cur->addr.type = NA_IP;
		cur = cur->next;
	}
}

static qbool QTVList_Cache_File_Is_Old(void)
{
	SYSTEMTIME cache_time;
	SYSTEMTIME min_time; // minimum required time
	int diff;

	GetFileLocalTime(va("%s/%s/%s", com_basedir,
		QTVLIST_CACHE_FILE_DIR, QTVLIST_CACHE_FILE), &cache_time);

	GetLocalTime(&min_time);

	// subtract 7 days
	if (min_time.wDay > 7) {
		min_time.wDay -= 7;
	}
	else {
		// approximately 7 days in this case
		// let's not bother if a month has 28, 29, 30 or 31 days
		if (min_time.wMonth > 1) {
			min_time.wMonth -= 1;
			min_time.wDay += 22;
		}
		else {
			min_time.wYear -= 1;
			min_time.wMonth = 12;
			min_time.wDay += 22;
		}
	}

	diff = SYSTEMTIMEcmp(&cache_time, &min_time);
	return diff < 0;
}

void QTVList_Refresh_Cache(qbool force_redownload)
{
	vfsfile_t *cache_file;
	sb_qtvlist_parse_state_t *sb_qtvparse;

	Sys_mkdir(va("%s/" QTVLIST_CACHE_FILE_DIR, com_basedir));

	if (force_redownload
		|| !(cache_file = QTVList_Cache_File_Open("rb"))
		|| QTVList_Cache_File_Is_Old())
	{
		sb_qtvlist_cache.status = QTVLIST_DOWNLOADING;
		QTVList_Cache_File_Download();

			cache_file = QTVList_Cache_File_Open("rb");
		if (!cache_file) {
			Com_Printf("Can't open QTV cache file\n");
			return;
		}
	}

	sb_qtvlist_cache.status = QTVLIST_PARSING;
	
	sb_qtvparse = QTVList_Parse_Init();
	QTVList_Process_Full_List(cache_file, sb_qtvparse);
	VFS_CLOSE(cache_file);
	sb_qtvlist_cache.sb_qtventries = sb_qtvparse->list;
	Q_free(sb_qtvparse);
	
	sb_qtvlist_cache.status = QTVLIST_RESOLVING;
	QTVList_Resolve_Hostnames();
}

static size_t QTVList_Length(const sb_qtventry_t *first)
{
	size_t ret = 0;
	const sb_qtventry_t *cur = first;

	while (cur) {
		ret++;
		cur = cur->next;
	}

	return ret;
}

static size_t QTVList_Player_Count(const sb_qtventry_t *entry)
{
	const sb_qtvplayer_t *cur = entry->players;
	size_t ret = 0;

	while (cur) {
		ret++;
		cur = cur->next;
	}

	return ret;
}

int QTVList_Entry_Cmp(const void *a_v, const void *b_v)
{
	sb_qtventry_t *entry_a = (sb_qtventry_t *) a_v;
	sb_qtventry_t *entry_b = (sb_qtventry_t *) b_v;

	if (entry_a->observercount > entry_b->observercount) {
		return -1;
	}
	else if (entry_a->observercount == entry_b->observercount) {
		return 0;
	}
	else {
		return 1;
	}
}

static void QTVList_Print(void)
{
	size_t len = QTVList_Length(sb_qtvlist_cache.sb_qtventries);
	sb_qtventry_t *entries_array;
	sb_qtventry_t *cur;
	int i;
	size_t players;
	sb_qtvplayer_t *player;

	entries_array = Q_malloc(len * sizeof (sb_qtventry_t));

	cur = sb_qtvlist_cache.sb_qtventries;

	i = 0;
	while (cur) {
		memcpy(&entries_array[i++], cur, sizeof (sb_qtventry_t));
		cur = cur->next;
	}

	qsort(entries_array, len, sizeof (sb_qtventry_t), QTVList_Entry_Cmp);

	for (i = 0; i < len; i++) {
		cur = &entries_array[i];
		players = QTVList_Player_Count(cur);

		if (players == 0) {
			continue;
		}

		Com_Printf("--- %3d %3u %30s ---\n",
			cur->observercount, players, cur->title);

		player = cur->players;
		while (player) {
			Com_Printf("      %4d [%4s] %s\n", player->frags, player->team, player->name);
			player = player->next;
		}
	}
}

DWORD WINAPI QTVList_Refresh_Cache_Thread(void *userData)
{
	QTVList_Refresh_Cache((qbool) userData);
	sb_qtvlist_cache.status = QTVLIST_READY;
	return 0;
}

DWORD WINAPI QTVList_Download_And_Print_Thread(void *userData)
{
	Com_Printf("QuakeTV list downloading...\n");
	QTVList_Refresh_Cache(true);
	sb_qtvlist_cache.status = QTVLIST_PRINTING;
	QTVList_Print();
	sb_qtvlist_cache.status = QTVLIST_READY;

	return 0;
}

void QTVList_Print_Global(void)
{
	if (sb_qtvlist_cache.status != QTVLIST_READY && sb_qtvlist_cache.status != QTVLIST_INIT) {
		Com_Printf("QTV cache is still being rebuilt\n");
		return;
	}

	sb_qtvlist_cache.status = QTVLIST_PROCESSING;
	Sys_CreateThread(QTVList_Download_And_Print_Thread, NULL);
}

void QTVList_Initialize_Streammap(void)
{
	if (sb_qtvlist_cache.status != QTVLIST_READY && sb_qtvlist_cache.status != QTVLIST_INIT) {
		Com_Printf("QTV cache is still being rebuilt\n");
		return;
	}

	sb_qtvlist_cache.status = QTVLIST_PROCESSING;
	Sys_CreateThread(QTVList_Refresh_Cache_Thread, (void *) false);
}

static netadr_t QTVList_Current_IP(void)
{
	netadr_t adr;
	char *prx = Info_ValueForKey(cls.userinfo, "prx");

	if (prx && *prx) {
		char *lastsep = strrchr(prx, '@');
		lastsep = lastsep ? lastsep + 1 : prx;
		
		NET_StringToAdr(lastsep, &adr);
		return adr;
	}
	else {
		return cls.server_adr;
	}
}

void QTVList_Observeqtv_f(void)
{
	netadr_t addr;
	sb_qtventry_t *cur;

	if (sb_qtvlist_cache.status != QTVLIST_READY) {
		Com_Printf("QTV cache is still being rebuilt\n");
		return;
	}
	
	if (Cmd_Argc() < 2) {
		addr = QTVList_Current_IP();
	}
	else {
		NET_StringToAdr(Cmd_Argv(1), &addr);
	}

	if (addr.type == NA_IP) {
		cur = sb_qtvlist_cache.sb_qtventries;

		while (cur) {
			if (cur->addr.port == addr.port && memcmp(cur->addr.ip, addr.ip, sizeof (addr.ip)) == 0) {
				Cbuf_AddText(va("qtvplay %s\n", cur->link));
				return;
			}

			cur = cur->next;
		}

		Com_Printf("Cannot find current server on any QTV\n");
	}
	else {
		Com_Printf("Can't observe current server via QTV\n");
	}
}

void QTVList_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SERVER_BROWSER);

	Cvar_Register(&sb_qtvlist_url);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("observeqtv", QTVList_Observeqtv_f);

	QTVList_Initialize_Streammap();
}
