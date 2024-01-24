/*
Copyright (C) 2011-2015 azazello and ezQuake team

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

#include "quakedef.h"
#include "fs.h"
#include "gl_model.h"
#ifndef _WIN32
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "keys.h"
#include "EX_browser.h"
#include "EX_qtvlist.h"
#include "Ctrl_EditBox.h"
#include "settings.h"
#include "settings_page.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "menu.h"
#include "utils.h"
#include "menu_multiplayer.h"
#include "qsound.h"
#include "teamplay.h"

int source_unique = 0;

typedef struct info_filter_s {
	char name[MAX_INFO_KEY];
	pcre2_code* regex;
	qbool pass;
	qbool exec;
} info_filter_t;

static info_filter_t* SB_InfoFilter_Parse(int* count);
static qbool SB_InfoFilter_Exec(info_filter_t* info_filters, int info_filter_count, server_data* s);
static void SB_InfoFilter_Free(info_filter_t* info_filters, int info_filter_count);

// searching
#define MAX_SEARCH 20
#define SEARCH_TIME 3
double searchtime = -10;

#define MWHEEL_SCROLL_STEP 4

enum {
	search_none = 0,
	search_server,
	search_player
} searchtype = search_none;

char searchstring[MAX_SEARCH+1];

// add source
CEditBox edit1, edit2;
int adding_source = 0;
sb_source_type_t newsource_type; // 0 = file
int newsource_pos;

// add server
int adding_server = 0;
int newserver_pos;

serverbrowser_window_t Browser_window;

// must correspond to server_occupancy enum values
static const char* occupancy_colors[] = { "00f", "0f0", "f00" };

cvar_t  sb_status        =  {"sb_status", 			"1"}; // Shows Server status at the bottom

// columns
cvar_t  sb_showping      = {"sb_showping",         "1"};
cvar_t  sb_showaddress   = {"sb_showaddress",      "0"};
cvar_t  sb_showmap       = {"sb_showmap",          "1"};
cvar_t  sb_showgamedir   = {"sb_showgamedir",      "0"};
cvar_t  sb_showplayers   = {"sb_showplayers",      "1"};
cvar_t  sb_showfraglimit = {"sb_showfraglimit",    "0"};
cvar_t  sb_showtimelimit = {"sb_showtimelimit",    "0"};

cvar_t  sb_pingtimeout   = {"sb_pingtimeout",   "1000"};
cvar_t  sb_infotimeout   = {"sb_infotimeout",   "1000"};
cvar_t  sb_pingspersec   = {"sb_pingspersec",    "150"}; // Pings per second
cvar_t  sb_pings         = {"sb_pings",            "3"}; // Number of times to ping a server
cvar_t  sb_inforetries   = {"sb_inforetries",      "3"};
cvar_t  sb_infospersec   = {"sb_infospersec",    "100"};
cvar_t  sb_proxinfopersec= {"sb_proxinfopersec",  "10"};
cvar_t  sb_proxretries   = {"sb_proxretries",      "3"};
cvar_t  sb_proxtimeout   = {"sb_proxtimeout",   "1000"};
cvar_t  sb_mastertimeout = {"sb_mastertimeout", "1000"};
cvar_t  sb_masterretries = {"sb_masterretries",    "3"};
cvar_t  sb_nosockraw     = {"sb_nosockraw",        "0"}; // when enabled, forces "new ping" (udp qw query packet, multithreaded) to be used

cvar_t  sb_liveupdate    = {"sb_liveupdate",       "2"}; // not in menu

cvar_t  sb_sortservers   = {"sb_sortservers",     "32"}; // not in new menu
cvar_t  sb_sortplayers   = {"sb_sortplayers",     "92"}; // not in new menu
cvar_t  sb_sortsources   = {"sb_sortsources",      "3"}; // not in new menu

cvar_t  sb_autohide      = {"sb_autohide",         "1"}; // not in menu
cvar_t  sb_findroutes    = {"sb_findroutes",       "0"};
cvar_t  sb_ignore_proxy  = {"sb_ignore_proxy",     ""};

// filters
static void sb_trigger_resort(cvar_t* var, char* value, qbool* cancel)
{
	resort_servers = 1;
}

cvar_t  sb_hideempty     = {"sb_hideempty",        "1", 0, sb_trigger_resort };
cvar_t  sb_hidenotempty  = {"sb_hidenotempty",     "0", 0, sb_trigger_resort };
cvar_t  sb_hidefull      = {"sb_hidefull",         "0", 0, sb_trigger_resort };
cvar_t  sb_hidedead      = {"sb_hidedead",         "1", 0, sb_trigger_resort };
cvar_t  sb_hidehighping  = {"sb_hidehighping",     "0", 0, sb_trigger_resort };
cvar_t  sb_pinglimit     = {"sb_pinglimit",       "80", 0, sb_trigger_resort };
cvar_t  sb_showproxies   = {"sb_showproxies",      "0", 0, sb_trigger_resort };
cvar_t  sb_info_filter   = {"sb_info_filter",       "", 0, sb_trigger_resort };

cvar_t  sb_sourcevalidity  = {"sb_sourcevalidity", "30"}; // not in menu
cvar_t  sb_mastercache     = {"sb_mastercache",    "1"};  // not in menu
cvar_t  sb_autoupdate      = {"sb_autoupdate",     "1"};  // not in menu
cvar_t  sb_listcache       = {"sb_listcache",      "0"};

// servers table
server_data *servers[MAX_SERVERS];
int serversn;
int serversn_passed;

// all players table
player_host ** all_players = NULL;
int all_players_n = 0;

/// when 1, in the next frame in which the list is drawn also sorting will be done
int resort_servers = 1; 
int resort_sources = 1;
void Sort_Servers (void);

int testing_connection = 0;
int ping_phase = 0;
double ping_pos;
int abort_ping;

// allow background threads to fire triggers
int sb_queuedtriggers = 0;

// mouse and server list columns
static qbool mouse_in_header_row = false;
static int mouse_header_pos_y = 0;
static unsigned int mouse_hovered_column;

extern cvar_t cl_proxyaddr;

static SDL_mutex* serverlist_mutex = NULL;
sem_t serverinfo_semaphore;

typedef struct sb_column_t {
	const char *name;
	unsigned int width;
	cvar_t *showvar;
} sb_column_t;

sb_column_t sb_columns[] = {
	{ "name", COL_NAME, NULL },
	{ "address", COL_IP, &sb_showaddress },
	{ "png", COL_PING, &sb_showping },
	{ "gamedir", COL_GAMEDIR, &sb_showgamedir },
	{ "map", COL_MAP, &sb_showmap },
	{ "plyrs", COL_PLAYERS, &sb_showplayers },
	{ "fl", COL_FRAGLIMIT, &sb_showfraglimit },
	{ "tl", COL_TIMELIMIT, &sb_showtimelimit }
};

static const unsigned int sb_columns_size = sizeof(sb_columns) / sizeof(sb_column_t);

void Serverinfo_Stop(void);

void SB_ServerList_Lock(void)
{
	SDL_LockMutex(serverlist_mutex);
}

void SB_ServerList_Unlock(void)
{
	SDL_UnlockMutex(serverlist_mutex);
}

static qbool SB_Is_Selected_Proxy(const server_data *s)
{
	return (strstr(cl_proxyaddr.string, s->display.ip) != NULL);
}

// removes given proxy from the proxyaddr list
// if cl_proxyaddr "a:b:c" and s->ip = "b" then it changes cl_proxyaddr to "a:c"
static void SB_Proxy_Unselect(const server_data *s)
{
	const char *start = strstr(cl_proxyaddr.string, s->display.ip);
	const char *end;
	char *buf = (char *) Q_malloc(strlen(cl_proxyaddr.string) + 1);
	char *bufstart = buf;
	const char *cur;

	if (start == NULL) {
		Q_free(buf);
		return; // invalid call, proxy is not selected
	}

	for (cur = cl_proxyaddr.string; cur < start - 1;) {
		*buf++ = *cur++;
	}
	*buf = '\0';

	end = strchr(start, '@');
	if (end != NULL) {
		cur = end;
		if (*bufstart == '\0') cur++; // don't copy the '@' sign
		while (*cur) *buf++ = *cur++;
		*buf++ = '\0';
	}

	Cvar_Set(&cl_proxyaddr, bufstart);

	Q_free(bufstart);
}

// adds selected proxy to the end of the proxies list
static void SB_Proxy_Select(const server_data *s)
{
	size_t len = strlen(s->display.ip) + strlen(cl_proxyaddr.string) + 2;
	char *buf = (char *) Q_malloc(len);

	*buf = '\0';
	strlcat(buf, cl_proxyaddr.string, len);
	if (*buf) {
		strlcat(buf, "@", len);
	}
	strlcat(buf, s->display.ip, len);

	Cvar_Set(&cl_proxyaddr, buf);

	Q_free(buf);
}

static void SB_Select_QWfwd(server_data *s)
{
	if (SB_Is_Selected_Proxy(s)) {
		SB_Proxy_Unselect(s);
	}
	else {
		SB_Proxy_Select(s);
	}
	S_LocalSound ("misc/menu2.wav");
	Serverinfo_Stop();
}

static const char* SB_Source_Type_Name(sb_source_type_t type)
{
	switch (type) {
		case type_master: return "master";
		case type_file: return "file  ";
		case type_url: return "url   ";
		case type_dummy: return "dummy ";
		default:
				 Sys_Error("SB_Source_Type_Name(): Invalid sb_source_type_t type");
				 return "ERROR";
	}
}

static void SB_Browser_Hide(const server_data *s)
{
	if (sb_autohide.value)
	{
		if (sb_autohide.value > 1  ||  strcmp(s->display.gamedir, "qizmo")) {
			key_dest = key_game;
			m_state = m_none;
			M_Draw();
		}
	}
}

static void Join_Server (server_data *s)
{
	if (sb_findroutes.integer) {
		Cbuf_AddText("spectator 0\n");
		SB_PingTree_ConnectBestPath(&s->address);
	} else {
		Cbuf_AddText ("join ");
		Cbuf_AddText (s->display.ip);
		Cbuf_AddText ("\n");
	}

	SB_Browser_Hide(s);
}

static void Join_Server_Direct(server_data *s)
{
	Cvar_Set(&cl_proxyaddr, "");
	Cbuf_AddText ("join ");
	Cbuf_AddText (s->display.ip);
	Cbuf_AddText ("\n");
	SB_Browser_Hide(s);
}

static void Observe_Server (server_data *s)
{
	if (sb_findroutes.integer) {
		Cbuf_AddText("spectator 1\n");
		SB_PingTree_ConnectBestPath(&s->address);
	} else {
		Cbuf_AddText ("observe ");
		Cbuf_AddText (s->display.ip);
		Cbuf_AddText ("\n");
	}
	SB_Browser_Hide(s);
}

static void CopyServerToClipboard (server_data *s)
{
	char buf[2048];

	if (keydown[K_CTRL] || s->display.name[0] == 0)
		strlcpy (buf, s->display.ip, sizeof(buf));
	else
		snprintf (buf, sizeof (buf), "%s (%s)",
				s->display.name,
				s->display.ip);

	CopyToClipboard(buf);
}

static void PasteServerToConsole (server_data *s)
{
	char buf[2048];

	snprintf(buf, sizeof (buf), "%s (%s)",
			s->display.name,
			s->display.ip);

	Cbuf_AddText (keydown[K_CTRL] ? "say_team " :  "say ");
	Cbuf_AddText (buf);
	Cbuf_AddText ("\n");
}


//
// browser routines
//


server_data * Create_Server (char *ip)
{
	server_data *s;

	s = (server_data *) Q_malloc (sizeof(server_data));
	memset (s, 0, sizeof(server_data));

	s->ping = -1;

	if (!NET_StringToAdr (ip, &(s->address)))
	{
		Q_free(s);
		return NULL;
	}

	if (!strchr(ip, ':'))
		s->address.port = htons(27500);

	snprintf (s->display.ip, sizeof (s->display.ip), "%d.%d.%d.%d:%d",
			s->address.ip[0], s->address.ip[1], s->address.ip[2], s->address.ip[3],
			ntohs(s->address.port));

	return s;
}

server_data* Clone_Server(server_data* source)
{
	int i = 0;

	server_data* new_server = Create_Server2(source->address);

	new_server->bestping = source->bestping;
	memcpy(&new_server->display, &source->display, sizeof(source->display));
	new_server->keysn = source->keysn;
	for (i = 0; i < new_server->keysn; ++i) {
		new_server->keys[i] = Q_strdup(source->keys[i]);
		new_server->values[i] = Q_strdup(source->values[i]);
	}
	new_server->occupancy = source->occupancy;
	new_server->passed_filters = source->passed_filters;
	new_server->ping = source->ping;

	new_server->playersn = source->playersn;
	for (i = 0; i < sizeof(source->players) / sizeof(source->players[0]); ++i) {
		if (source->players[i]) {
			new_server->players[i] = Q_malloc(sizeof(playerinfo));

			memcpy(new_server->players[i], source->players[i], sizeof(playerinfo));
		}
	}

	new_server->qizmo = source->qizmo;
	new_server->qwfwd = source->qwfwd;
	new_server->spectatorsn = source->spectatorsn;
	new_server->support_teams = source->support_teams;

	return new_server;
}

server_data * Create_Server2 (netadr_t n)
{
	server_data *s;

	s = (server_data *) Q_malloc (sizeof (server_data));
	memset (s, 0, sizeof(server_data));

	memcpy (&(s->address), &n, sizeof (netadr_t));
	strlcpy (s->display.ip, NET_AdrToString(n), sizeof (s->display.ip));

	s->ping = -1;

	return s;
}

void Reset_Server (server_data *s)
{
	int i;

	for (i = 0; i < s->keysn; i++)
	{
		Q_free(s->keys[i]);
		if (s->values[i]) {
			// fixme: this was causing a crash so a check for not-null-ness was added
			Q_free(s->values[i]);
		}
	}

	s->keysn = 0;

	for (i = 0; i < s->playersn + s->spectatorsn; i++)
		Q_free(s->players[i]);

	s->playersn = 0;
	s->spectatorsn = 0;
	s->support_teams = false;

	rebuild_all_players = 1; // rebuild all-players list
}

void Delete_Server (server_data *s)
{
	Reset_Server(s);
	Q_free(s);
}


char confirm_text[64];
qbool confirmation;
void (*confirm_func)(void);

void SB_Confirmation (const char *text, void (*func)(void))
{
	strlcpy (confirm_text, text, sizeof (confirm_text));
	confirm_func = func;
	confirmation = 1;
}

void SB_Confirmation_Draw (void)
{
	int x, y, w, h;

#define CONFIRM_TEXT_DEFAULT "Are you sure? <y/n>"

	w = 32 + 8 * max (strlen (confirm_text), strlen (CONFIRM_TEXT_DEFAULT));
	h = 24;

	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;
	x = (x / 8) * 8;
	y = (y / 8) * 8;

	Draw_TextBox (x - 16, y - 16, w / 8 + 1, h / 8 + 2);

	UI_Print_Center (x, y, w, confirm_text, false);
	UI_Print_Center (x, y+16, w, CONFIRM_TEXT_DEFAULT, true);
}

void SB_Confirmation_Key (int key)
{
	switch (key)
	{
		case K_BACKSPACE:
		case K_ESCAPE:
		case 'n':
		case 'N':
			confirmation = 0;
			break;
		case 'y':
		case 'Y':
			confirmation = 0;
			confirm_func();
	}
}

/* Menu drawing */

int Servers_pos;
int Sources_pos;
int Players_pos;
int Options_pos;

server_data * show_serverinfo;
int serverinfo_pos;

int Servers_disp;   // server# at the top of the list
int Sources_disp;   // source# at the top of the list
int Players_disp;   // player# at the top of the list

void Serverinfo_Draw (void);
void Serverinfo_Players_Draw(int x, int y, int w, int h);
void Serverinfo_Rules_Draw(int x, int y, int w, int h);
void Serverinfo_Sources_Draw(int x, int y, int w, int h);
void Serverinfo_Key (int key);
void Serverinfo_Players_Key(int key);
void Serverinfo_Rules_Key(int key);
void Serverinfo_Sources_Key(int key);

//
// serverinfo
//

int sourcesn_updated = 0;

int Sources_Compare_Func (const void * p_s1, const void * p_s2)
{
	int reverse = 0;
	char *sort_string = sb_sortsources.string;
	const source_data *s1 = *((source_data **)p_s1);
	const source_data *s2 = *((source_data **)p_s2);

	if (show_serverinfo)
	{
		if (!(s1->last_update.wYear)  &&  s2->last_update.wYear)
			return 1;
		if (!s2->last_update.wYear  &&  s1->last_update.wYear)
			return -1;
		if (!s1->last_update.wYear  &&  !s2->last_update.wYear)
			return s1 - s2;
	}

	while (true)
	{
		int d;  // difference

		if (*sort_string == '-')
		{
			reverse = 1;
			sort_string++;
			continue;
		}

		switch (*sort_string++)
		{
			case '1':
				d = s1->type - s2->type; break;
			case '2':
				d = funcmp(s1->name, s2->name); break;
			case '3':
				d = s1->unique - s2->unique; break;
			default:
				d = s1 - s2;
		}

		if (d)
			return reverse ? -d : d;
	}
}
void Sort_Sources (void)
{
	int i;

	qsort (sources+1, sourcesn-1, sizeof(sources[0]), Sources_Compare_Func);

	sourcesn_updated = 1;

	for (i = 1; i < sourcesn; i++)
		if (sources[i]->last_update.wYear)
			sourcesn_updated ++;
}


int serverinfo_players_pos;
int serverinfo_sources_pos;
int serverinfo_sources_disp;
extern int autoupdate_serverinfo; // declared in EX_browser_net.c

void Serverinfo_Stop(void)
{
	show_serverinfo = NULL;
	autoupdate_serverinfo = 0;
	Sys_SemDestroy(&serverinfo_semaphore);
}

void Serverinfo_Start (server_data *s)
{
	if (show_serverinfo)
		Serverinfo_Stop();

	serverinfo_players_pos = 0;
	serverinfo_sources_pos = 0;

	autoupdate_serverinfo = 1;
	show_serverinfo = s;

	Sys_SemInit(&serverinfo_semaphore, 1, 1);

	// sort for eliminating ot-updated
	Sort_Sources();
	resort_sources = 1; // and mark for resort on next sources draw

	Start_Autoupdate(s);

	// testing connection
	if (testing_connection)
	{
		char buf[256];
		snprintf(buf, sizeof (buf), "%d.%d.%d.%d",
				show_serverinfo->address.ip[0],
				show_serverinfo->address.ip[1],
				show_serverinfo->address.ip[2],
				show_serverinfo->address.ip[3]);
		SB_Test_Init(buf);
		testing_connection = 1;
	}
}

void Serverinfo_Change (server_data *s)
{
	Alter_Autoupdate(s);
	show_serverinfo = s;

	// testing connection
	if (testing_connection)
	{
		char buf[256];
		snprintf (buf, sizeof (buf), "%d.%d.%d.%d",
				show_serverinfo->address.ip[0],
				show_serverinfo->address.ip[1],
				show_serverinfo->address.ip[2],
				show_serverinfo->address.ip[3]);
		SB_Test_Change(buf);
		testing_connection = 1;
	}
}

// --

qbool AddUnboundServer(char *addr)
{
	int i;
	server_data *s;
	qbool existed = false;

	if (sources[0]->serversn >= MAX_UNBOUND)
		return false;
	s = Create_Server(addr);
	if (s == NULL)
		return false;

	for (i=0; i < sources[0]->serversn; i++)
		if (!memcmp(&s->address, &sources[0]->servers[i]->address, sizeof(netadr_t)))
		{
			Q_free(s);
			s = sources[0]->servers[i];
			existed = true;
			break;
		}
	if (!existed)  // not found
	{
		sources[0]->servers[sources[0]->serversn] = s;
		(sources[0]->serversn) ++;
		rebuild_servers_list = true;
	}

	// start menu
	key_dest = key_menu;
	Mark_Source(sources[0]);
	Menu_MultiPlayer_SwitchToServersTab();
	GetServerPing(s);
	GetServerInfo(s);
	Serverinfo_Start(s);
	// M_Menu_ServerList_f();
	return true;
}

void AddServer_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: addserver <addr>\n");
		return;
	}

	if (!AddUnboundServer(Cmd_Argv(1)))
	{
		if (sources[0]->serversn >= MAX_UNBOUND)
			Com_Printf("Error: maximum unbound servers number reached\n");
		else
			Com_Printf("Error: couldn't resolve\n");
	}
}

void SB_PingsDump_f(void)
{
	extern qbool useNewPing;
	int i;

	Com_Printf("// server ping dump\n// format: ip ping\n// protocol: %s\n", useNewPing ? "UDP" : "ICMP");
	for (i = 0; i < serversn; i++) {
		int ping = servers[i]->ping;

		if (ping >= 0 && ping < 999) {
			Com_Printf("%s %d\n", NET_AdrToString(servers[i]->address), ping);
		}
	}
}

//
// drawing routines
//

void Add_ColumnColored(int x, int y, int *pos, const char *t, int w, const char* color)
{
	char buf[128];
	if ((*pos) - w - 1  <=  5)
		return;

	snprintf (buf, sizeof(buf), "&c%s%s", color, t);

	(*pos) -= w;
	UI_Print_Center(x + (*pos)*8, y, 8*(w+5), buf, false);
	(*pos)--;
}

void Add_Column2(int x, int y, int *pos, const char *t, int w, int red)
{
	if ((*pos) - w - 1  <=  5)
		return;

	(*pos) -= w;
	UI_Print_Center(x + (*pos)*8, y, 8*w, t, red);
	(*pos)--;
}

void Draw_Server_Statusbar(int x, int y, int w, int h, server_data *s, int count, int total)
{
	int i;
	int d_gamedir, d_map;
	char buf[1024], line[1024];

	memset(line, '\x1E', w/8);
	line[w/8] = 0;
	line[w/8-1] = '\x1F';
	line[0] = '\x1D';
	if (total > 0)
	{
		snprintf(buf, sizeof (buf), "%d/%d", count+1, total);
		memset(line+w/8-3-strlen(buf), ' ', strlen(buf)+1);
	}
	UI_Print(x, y+h-24, line, false);
	if (total > 0)
		UI_Print(x+w-8*(3+strlen(buf))+4, y+h-24, buf, true);

	// line 1
	strlcpy (line, s->display.name[1] ? s->display.name : s->display.ip, sizeof (line));
	line[w/8] = 0;
	UI_Print_Center(x, y+h-16, w, line, false);

	// line 2
	if (searchtype)
	{
		int i;
		snprintf(line, sizeof (line), "search for: %-7s", searchstring);
		line[w/8] = 0;
		for (i=0; i < strlen(line); i++)
			line[i] ^= 128;
		UI_Print_Center(x, y+h-8, w, line, false);
	}
	else
	{
		d_gamedir = d_map = 1;
		if (ValueForKey(s, "map") == NULL)
			d_map = 0;
		if (ValueForKey(s, "*gamedir") == NULL)
			d_gamedir = 0;

		line[0] = 0;

		if (d_gamedir)
		{
			memset(buf, 0, 10);
			strlcpy(buf, ValueForKey(s, "*gamedir"), sizeof(buf));
			buf[8] = 0;
			strlcat (line, buf, sizeof (line));
			strlcat (line, "\xa0 ", sizeof (line));
		}

		if (d_map)
		{
			memset(buf, 0, 10);
			strlcpy(buf, ValueForKey(s, "map"), sizeof(buf));
			buf[8] = 0;
			strlcat (line, buf, sizeof (line));
			strlcat (line, "\xa0 ", sizeof (line));
		}

		//if (d_players)
		{
			char buf[10], *max;
			max =  ValueForKey(s, "maxclients");
			snprintf(buf, sizeof(buf), "%d/%s", s->playersn, max==NULL ? "??" : max);
			strlcat (line, buf, sizeof (line));
			max =  ValueForKey(s, "maxspectators");
			snprintf(buf, sizeof(buf), "-%d/%s", s->spectatorsn, max==NULL ? "??" : max);
			strlcat (line, buf, sizeof(line));
		}

		if (ValueForKey(s, "status") == NULL)
		{
			char *dm, *fl, *tl;
			char buf[200];
			dm = ValueForKey(s, "deathmatch");
			fl = ValueForKey(s, "fraglimit");
			tl = ValueForKey(s, "timelimit");

			if (dm  &&  strlen(line) + 7 <= w/8)
			{
				snprintf(buf, sizeof(buf), "\xa0 dmm%s", dm);
				strlcat(line, buf, sizeof(line));
			}

			if (fl  &&  strlen(line) + 8 <= w/8)
			{
				snprintf(buf, sizeof(buf), "\xa0 fl:%s", fl);
				strlcat(line, buf, sizeof(line));
			}

			if (tl  &&  strlen(line) + 7 <= w/8)
			{
				snprintf(buf, sizeof(buf), "\xa0 tl:%s", tl);
				strlcat(line, buf, sizeof(line));
			}
		}
		else
		{
			char buf[200];
			snprintf(buf, sizeof(buf), "\xa0 %s", ValueForKey(s, "status"));
			strlcat(line, buf, sizeof(line));
		}

		// draw line
		line[w/8] = 0;
		UI_Print_Center(x, y+h-8, w, line, false);
		// and dots  --  shifted by 4 pixels
		for (i=0; i < strlen(line); i++)
			line[i] = (line[i]=='\xa0' ? '\x85' : ' ');
		UI_Print_Center(x+4, y+h-8, w, line, false);
	}
}

void Add_Server_Draw(void)
{
	int x, y, w, h;
	w = 176;
	h = 56;
	x = (vid.width - w)/2;
	y = (vid.height - h)/2;
	x = (x/8) * 8;
	y = (y/8) * 8;

	Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

	x-=4;
	y-=4;
	w+=8;
	h+=8;

	UI_Print_Center(x, y+4, w, "Create new server", true);

	UI_Print(x+4, y + 20, "Address:", newserver_pos==0);
	CEditBox_Draw(&edit1, x+70, y+20, newserver_pos==0);

	UI_Print_Center(x+4, y+40, w, "accept", newserver_pos==1);
	if (newserver_pos == 1)
		Draw_Character (x+59, y+40, 13);

	UI_Print_Center(x+4, y+50, w, "cancel", newserver_pos==2);
	if (newserver_pos == 2)
		Draw_Character (x+59, y+50, 13);
}

void SB_Servers_OnShow (void)
{
	static qbool updated = false;

	if (sb_autoupdate.value && !updated) {

		GetServerPingsAndInfos(true);
		updated = true;
	}

	resort_servers = 1;
}

static const char *SB_Ping_Color(int ping)
{
	double frame_duration = 1000.0/77.0;
	int frames = ping / frame_duration;

	switch (frames) {
		case 0: return "1f0"; // 13
		case 1: return "3d0"; // 26
		case 2: return "790"; // 39
		case 3: return "880"; // 51
		case 4: return "a60"; // 65
		case 5: 
		case 6: 
		case 7: 
		case 8:
		case 9: return "d30"; // 70-116
		default: return "f00"; // 116+
	}
}

static unsigned int SB_Servers_Hovered_Column(int w)
{
	int w_from = w, w_to = w;
	int col;

	for (col = sb_columns_size - 1; col > 0; col--) {
		if (sb_columns[col].showvar->integer) {
			w_to = w_from;
			w_from -= sb_columns[col].width * LETTERWIDTH + LETTERWIDTH;
			if (w_from <= mouse_header_pos_y && mouse_header_pos_y < w_to) {
				return col;
			}
		}
	}
	return 0;
}

// returns the horizontal offset of the second shown column in characters
int SB_Servers_Draw_ColumnHeaders(int x, int y, int w)
{
	int pos = w/8;
	char line[1024];
	unsigned int colidx;
	qbool hovered;

	mouse_hovered_column = SB_Servers_Hovered_Column(w);
	memset(line, ' ', pos);
	line[pos] = 0;

	UI_DrawColoredAlphaBox(x, y, w, 8, RGBA_TO_COLOR(10, 10, 10, 200));

	for (colidx = sb_columns_size - 1; colidx > 0; colidx--) {
		sb_column_t *col = &sb_columns[colidx];
		hovered = mouse_in_header_row && mouse_hovered_column == colidx;

		if (col->showvar->integer) {
			if (mouse_in_header_row && hovered) {
				UI_DrawColoredAlphaBox(x + pos * LETTERWIDTH - col->width * LETTERWIDTH - LETTERWIDTH,
						y, col->width * LETTERWIDTH + LETTERWIDTH, 8, RGBA_TO_COLOR(30, 30, 30, 200));
			}
			Add_Column2(x, y, &pos, col->name, col->width, !hovered);
		}
	}
	// name is always displayed
	hovered = mouse_in_header_row && mouse_hovered_column == colidx;
	if (hovered) {
		UI_DrawColoredAlphaBox(x, y, pos * LETTERWIDTH + LETTERWIDTH, 8, RGBA_TO_COLOR(30, 30, 30, 200));
	}
	UI_Print(x, y, "name", !(mouse_in_header_row && mouse_hovered_column == 0));

	return pos;
}

void SB_Servers_Draw (int x, int y, int w, int h)
{
	char line[1024];
	int i, pos, listsize;

	if (updating_sources) {
		UI_Print_Center(x, y + 8, w, "Updating, please wait", false);
		return;
	}

	if (rebuild_servers_list)
		Rebuild_Servers_List();

	if (searchtype != search_server  ||  searchtime + SEARCH_TIME < cls.realtime)
		searchtype = search_none;

	if (resort_servers)
	{
		Sort_Servers();
		resort_servers = 0;
	}

	if (serversn_passed > 0)
	{
		SB_ServerList_Lock();

		Servers_pos = max(Servers_pos, 0);
		Servers_pos = min(Servers_pos, serversn_passed-1);

		listsize = (int)(h/8) - (sb_status.value ? 3 : 0);

		listsize--;     // column titles

		pos = SB_Servers_Draw_ColumnHeaders(x, y, w);

		if (Servers_disp < 0)
			Servers_disp = 0;
		if (Servers_pos > Servers_disp + listsize - 1)
			Servers_disp = Servers_pos - listsize + 1;
		if (Servers_disp > serversn_passed - listsize)
			Servers_disp = max(serversn_passed - listsize, 0);
		if (Servers_pos < Servers_disp)
			Servers_disp = Servers_pos;

		if (updating_sources) {
			SB_ServerList_Unlock();
			return;
		}

		for (i = 0; i < listsize; i++)
		{
			int servnum = Servers_disp + i;
			if (servnum >= serversn_passed)
				break;

			if (servnum==Servers_pos) {
				UI_DrawGrayBox(x, y+8*(i+1), w, 8);
				UI_DrawCharacter(x + 8*pos, y+8*(i+1), FLASHINGARROW());
			} else if (servers[servnum]->qizmo) {
				UI_DrawColoredAlphaBox(x, y+8*(i+1), w, 8, RGBA_TO_COLOR(25, 25, 75, 255));
			} else if (servers[servnum]->qwfwd) {
				if (SB_Is_Selected_Proxy(servers[servnum])) {
					UI_DrawColoredAlphaBox(x, y+8*(i+1), w, 8, RGBA_TO_COLOR(25, 120, 40, 255));
				} else {
					UI_DrawColoredAlphaBox(x, y+8*(i+1), w, 8, RGBA_TO_COLOR(25, 50, 25, 255));
				}
			} else if (servnum % 2) {
				UI_DrawColoredAlphaBox(x, y+8*(i+1), w, 8, RGBA_TO_COLOR(25, 25, 25, 125));
			} else {
				UI_DrawColoredAlphaBox(x, y+8*(i+1), w, 8, RGBA_TO_COLOR(50, 50, 50, 125));
			}

			// Display server
			pos = w/8;
			memset(line, ' ', 1000);

			if (sb_showtimelimit.value)
				Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.timelimit, COL_TIMELIMIT, servnum==Servers_pos);
			if (sb_showfraglimit.value)
				Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.fraglimit, COL_FRAGLIMIT, servnum==Servers_pos);
			if (sb_showplayers.value)
				Add_ColumnColored(x, y+8*(i+1), &pos, servers[servnum]->display.players, COL_PLAYERS, occupancy_colors[servers[servnum]->occupancy]);

			if (sb_showmap.value)
				Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.map, COL_MAP, servnum==Servers_pos);
			if (sb_showgamedir.value)
				Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.gamedir, COL_GAMEDIR, servnum==Servers_pos);
			if (sb_showping.value) {
				const char *ping = (servers[servnum]->bestping >= 0) ? servers[servnum]->display.bestping : servers[servnum]->display.ping;
				const char *color = SB_Ping_Color((servers[servnum]->bestping >= 0) ? servers[servnum]->bestping : servers[servnum]->ping);
				Add_ColumnColored(x, y+8*(i+1), &pos, ping, COL_PING, color);
			}
			if (sb_showaddress.value)
				Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.ip, COL_IP, servnum==Servers_pos);

			// 'name' column
			if (servers[servnum]->qwfwd) {
				if (servers[servnum]->display.name[0]) {
					snprintf(line, sizeof(line), "proxy %s", servers[servnum]->display.name);
				}
				else {
					snprintf(line, sizeof(line), "proxy %s", servers[servnum]->display.ip);
				}
			}
			else if (servers[servnum]->display.name[0]) {
				snprintf(line, sizeof(line), "%s", servers[servnum]->display.name);
			}
			else {
				snprintf(line, sizeof(line), "%s", servers[servnum]->display.ip);
			}

			// display only as much as fits into the column
			line[min(pos, sizeof(line)-1)] = '\0';

			UI_Print(x, y+8*(i+1), line, servnum==Servers_pos);
		}


		//
		// status line
		//
		if (sb_status.value  &&  serversn_passed > 0)
		{
			// Semaphore lock
			if(show_serverinfo)
			{
				Sys_SemWait(&serverinfo_semaphore);
				Draw_Server_Statusbar(x, y, w, h, servers[Servers_pos], Servers_pos, serversn_passed);
				Sys_SemPost(&serverinfo_semaphore);
			}
			else
			{
				Draw_Server_Statusbar(x, y, w, h, servers[Servers_pos], Servers_pos, serversn_passed);
			}
		}
		SB_ServerList_Unlock();
	} else if (!adding_server) {
		UI_Print_Center(x, y+8, w, "No servers filtered", false);
		UI_Print_Center(x, y+24, w, "Press [space] to refresh the list", true);
		UI_Print_Center(x, y+40, w, "Mark some sources on the next tab", false);
		UI_Print_Center(x, y+48, w, "or press [Insert] to add a server", false);

		if (strlen(searchstring)) {
			char line[1024];
			snprintf(line, sizeof (line), "search for: %-7s", searchstring);
			UI_Print_Center(x, y+h-8, w, line, true);
		}
	}

	// adding server
	if (adding_server)
		Add_Server_Draw();
}

void PingPhase_Draw(void)
{
	int x, y, w, h;
	w = 144;
	h = 24;
	x = (vid.width - w)/2;
	y = (vid.height - h)/2;
	x = (x/8) * 8;
	y = (y/8) * 8;

	Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

	UI_Print_Center(x, y, w,
			ping_phase==1 ? "Pinging Servers" : "Getting Infos",
			false);
	if (abort_ping)
		UI_Print_Center(x, y+16, w, "cancelled", true);
	else
	{
		UI_Print(x, y+16,
				"\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82",
				false);
		UI_DrawCharacter(x+8+(int)(ping_pos*15*8), y+16, '\x83');
	}
}

void UpdatingSources_Draw(void)
{
	int x, y, w, h;
	w = 144;
	h = 24;
	x = (vid.width - w)/2;
	y = (vid.height - h)/2;
	x = (x/8) * 8;
	y = (y/8) * 8;

	Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

	UI_Print_Center(x, y, w, "Updating Sources", false);

	if (abort_ping)
		UI_Print_Center(x, y+16, w, "cancelled", true);
	else
	{
		UI_Print(x, y+16,
				"\x80\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x82",
				false);
		UI_DrawCharacter(x+8+(int)(ping_pos*15*8), y+16, '\x83');
	}
}

void Serverinfo_Help_Draw(int x, int y, int wPixels)
{
	int wLetters = wPixels / 8;

	if (vid.conheight < 216 || vid.conwidth < 384) {
		return;
	}

	// make it wider
	x -= LETTERWIDTH * 4;
	wLetters += 8;

	Draw_TextBox (x, y, wLetters, sb_findroutes.integer > 0 ? 4 : 3);
	x += LETTERWIDTH * 2;
	y += LETTERWIDTH;
	UI_Print(x, y,  "\xDBj\xDD join \xDBo\xDD observe \xDBq\xDD QuakeTV obs.", false);
	y += LETTERWIDTH;
	UI_Print(x, y, "\xDB" "c\xDD copy to clipboard \xDBv\xDD say in chat", false);
	y += LETTERWIDTH;
	UI_Print(x, y, "\xDB" "ctrl+c\xDD copy addr. \xDB" "ctrl+v\xDD say team", false);

	if (sb_findroutes.integer > 0) {
		int pathlen = SB_PingTree_GetPathLen(&show_serverinfo->address);
		y += LETTERWIDTH;
		if (pathlen < 0) {
			UI_Print(x, y, "Route: No route found", false);
		}
		else if (pathlen == 0) {
			UI_Print(x, y, "Route: Direct route is best", false);
		}
		else if (pathlen == 1) {
			UI_Print(x, y, "Route: 1 hop " "\xDB" "n\xDD direct connect", false);
		}
		else if (pathlen > 1) {
			char tmp[64];
			snprintf(&tmp[0], sizeof(tmp), "Route: %d hops, " "\xDB" "n\xDD direct connect", pathlen);
			UI_Print(x, y, tmp, false);
		}
	}
}

void Serverinfo_Draw (void)
{
	extern int server_during_update;
	char buf[512];

	int x, y, w, h;
	w = 200 + 40;
	h = 112;

	if (vid.height >= 200 + 40)
		h += 8*5;

	x = (vid.width - w)/2;
	y = (vid.height - h)/2;

	x = (x/8) * 8;
	y = (y/8) * 8;

	Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

	x-=4;
	y-=4;
	w+=8;
	h+=8;

	while (server_during_update)
		Sys_MSleep(5);

	strlcpy(buf, show_serverinfo->display.name, sizeof(buf));
	buf[w/8] = 0;
	UI_Print_Center(x, y, w, buf, false);
	strlcpy(buf, show_serverinfo->display.ip, sizeof(buf));
	buf[w/8] = 0;
	UI_Print_Center(x, y+10, w, buf, false);

	strlcpy(buf, " players serverinfo sources ", sizeof(buf));
	if (serverinfo_pos == 0)
		memcpy (buf, "\x10\xF0\xEC\xE1\xF9\xE5\xF2\xF3\x11", 9);
	if (serverinfo_pos == 1)
		memcpy (buf + 8, "\x10\xF3\xE5\xF2\xF6\xE5\xF2\xE9\xEE\xE6\xEF\x11", 12); // FIXME: non-ascii chars
	if (serverinfo_pos == 2)
		memcpy (buf + 19, "\x10\xF3\xEF\xF5\xF2\xE3\xE5\xF3\x11", 9); // FIXME: non-ascii chars

	UI_Print_Center(x, y+24, w, buf, false);

	memset(buf, '\x1E', w/8);
	buf[w/8] = 0;
	buf[w/8-1] = '\x1F';
	buf[0] = '\x1D';
	UI_Print(x, y+32, buf, false);

	if (testing_connection)
		h -= 5*8;

	Sys_SemWait(&serverinfo_semaphore);
	switch (serverinfo_pos)
	{
		case 0: // players
			Serverinfo_Players_Draw(x, y+40, w, h-40); break;
		case 1: // serverinfo / rules
			Serverinfo_Rules_Draw(x, y+40, w, h-40); break;
		case 2: // sources
			Serverinfo_Sources_Draw(x, y+40, w, h-40); break;
		default:
			;
	}
	Sys_SemPost(&serverinfo_semaphore);

	Serverinfo_Help_Draw(x - 16, y + h + LETTERWIDTH, w);

	if (testing_connection)
		SB_Test_Frame();
}

void Serverinfo_Players_Draw(int x, int y, int w, int h)
{
	server_data *s = show_serverinfo; // shortcut
	char *tp = (ValueForKey(s, "teamplay") ? ValueForKey(s, "teamplay") : ""); // tp at least "" not NULL
	qbool support_tp = s->support_teams && strcmp(tp, "0"); // is server support team info per player and teamplay != 0

	int i;
	int listsize;
	int match = 0;
	int top1=0, bottom1=0, frags1=0;
	int top2=0, bottom2=0, frags2=0;
	char *t1 = "", *t2 = "";
	int qizmo = 0;

	if (s == NULL  ||  s->playersn + s->spectatorsn <= 0)
		return;

	if (!strcmp(s->display.gamedir, "qizmo"))
		qizmo = 1;

	// check if this is a teamplay (2 teams) match
	if (!qizmo && s->playersn > 2 && strcmp(tp, "0")) // at least 3 players
	{
		top1 = bottom1 = top2 = bottom2 = -1;
		frags1 = frags2 = 0;
		match = 1;

		for (i = 0; i < s->playersn + s->spectatorsn; i++)
		{
			if (s->players[i]->spec)
				continue; // ignore spec

			if (top1 < 0) // ok, we found member from first team, save colors/team
			{
				top1	= s->players[i]->top;
				bottom1	= s->players[i]->bottom;
				t1		= s->players[i]->team;
			}

			// we now can sort teams by two way: colors, and actual teams

			if (    ( support_tp && !strncmp(t1, s->players[i]->team, sizeof(s->players[0]->team) - 1)) // teams matched
					|| (!support_tp && s->players[i]->top == top1 && s->players[i]->bottom == bottom1) // colors matched
			   )
			{
				frags1 += s->players[i]->frags;
				continue;
			}

			if (top2 < 0) // ok, we found member from second team, save colors/team
			{
				top2	= s->players[i]->top;
				bottom2	= s->players[i]->bottom;
				t2		= s->players[i]->team;
			}

			if (    ( support_tp && !strncmp(t2, s->players[i]->team, sizeof(s->players[0]->team) - 1)) // teams matched
					|| (!support_tp && s->players[i]->top == top2 && s->players[i]->bottom == bottom2) // colors matched
			   )
			{
				frags2 += s->players[i]->frags;
				continue;
			}

			// found member from third team, we does't support such case
			match = 0;
			break;
		}

		if (top1 < 0 || top2 < 0)
			match = 0; // we does't have two teams

		if (match  &&  frags2 > frags1)
		{
			int swap;
			swap = frags2;   frags2  = frags1;   frags1  = swap;
			swap = top2;     top2    = top1;     top1    = swap;
			swap = bottom2;  bottom2 = bottom1;  bottom1 = swap;
		}
	}

	listsize = h/8-1 - match;

	if (serverinfo_players_pos > s->playersn + s->spectatorsn - listsize)
		serverinfo_players_pos = s->playersn + s->spectatorsn - listsize;
	if (serverinfo_players_pos < 0)
		serverinfo_players_pos = 0;

	UI_Print(x, y, support_tp ? "png tm frgs team name" : "png tm frgs name", true);
	for (i=0; i < listsize; i++)
	{
		char buf[100], fragsbuf[100] = {0};
		int top, bottom;

		if (serverinfo_players_pos + i >= s->playersn + s->spectatorsn)
			break;

		if (!s->players[serverinfo_players_pos+i]->spec) {
			int frags_tmp = bound(-99, s->players[serverinfo_players_pos+i]->frags, 9999);
			snprintf(fragsbuf, sizeof(fragsbuf), "%3d%s", frags_tmp, frags_tmp < 1000 ? " " : ""); // "centering" frags as much as possible
		}

		if (support_tp)
			snprintf(buf, sizeof(buf), "%3d %2d %4.4s %4.4s %s", // frags column fixed to 4 symbols
					max(min(s->players[serverinfo_players_pos+i]->ping, 999), 0),
					max(min(s->players[serverinfo_players_pos+i]->time, 99), 0),
					s->players[serverinfo_players_pos+i]->spec ? "spec" : fragsbuf,
					s->players[serverinfo_players_pos+i]->team,
					s->players[serverinfo_players_pos+i]->name);
		else
			snprintf(buf, sizeof(buf), "%3d %2d %4.4s %s", // frags column fixed to 4 symbols
					max(min(s->players[serverinfo_players_pos+i]->ping, 999), 0),
					max(min(s->players[serverinfo_players_pos+i]->time, 99), 0),
					s->players[serverinfo_players_pos+i]->spec ? "spec" : fragsbuf,
					s->players[serverinfo_players_pos+i]->name);

		buf[w/8] = 0;

		if (!s->players[serverinfo_players_pos+i]->spec) {
			top		= s->players[serverinfo_players_pos+i]->top;
			bottom	= s->players[serverinfo_players_pos+i]->bottom;

			if (support_tp && match) // force players have same colors in same team, in such case
			{
				if (!strncmp(t1, s->players[serverinfo_players_pos+i]->team, sizeof(s->players[0]->team) - 1))
				{
					top		= top1;
					bottom	= bottom1;
				}
				else if (!strncmp(t2, s->players[serverinfo_players_pos+i]->team, sizeof(s->players[0]->team) - 1))
				{
					top		= top2;
					bottom	= bottom2;
				}
			}

			Draw_Fill (x+7*8-2, y+i*8+8   +1, 34, 4, top);
			Draw_Fill (x+7*8-2, y+i*8+8+4 +1, 34, 3, bottom);
		}

		UI_Print(x, y+i*8+8, buf, false);
	}

	if (match)
	{
		char buf[100];
		Draw_Fill (x+13*8, y+listsize*8+8   +1, 40, 4, top1);
		Draw_Fill (x+13*8, y+listsize*8+8+4 +1, 40, 4, bottom1);
		Draw_Fill (x+21*8, y+listsize*8+8   +1, 40, 4, top2);
		Draw_Fill (x+21*8, y+listsize*8+8+4 +1, 40, 4, bottom2);

		snprintf(buf, sizeof (buf), "      score:  %3d  -  %3d", frags1, frags2);
		UI_Print(x, y+listsize*8+8, buf, false);
	}
}

int serverinfo_rules_pos = 0;

void Serverinfo_Rules_Draw(int x, int y, int w, int h)
{
	int i;
	int listsize = h/8;
	server_data *s = show_serverinfo;

	if (serverinfo_rules_pos > s->keysn - listsize)
		serverinfo_rules_pos = s->keysn - listsize;
	if (serverinfo_rules_pos < 0)
		serverinfo_rules_pos = 0;

	for (i=0; i < listsize; i++)
	{
		char buf[128];

		if (serverinfo_rules_pos + i >= s->keysn)
			break;
		snprintf(buf, sizeof (buf), "%-13.13s %-*s",
				s->keys[serverinfo_rules_pos+i],
				w/8-1-13,
				s->values[serverinfo_rules_pos+i]);

		buf[w/8] = 0;

		UI_Print(x, y+i*8, buf, false);
	}
}

int IsInSource(source_data *source, server_data *serv)
{
	int i;
	for (i=0; i < source->serversn; i++)
		if (!memcmp(&source->servers[i]->address, &serv->address, sizeof(netadr_t)))
			return i+1;
	return false;
}

void Serverinfo_Sources_Draw(int x, int y, int w, int h)
{
	int i;
	int listsize;

	server_data *s = show_serverinfo;

	if (s == NULL  ||  sourcesn_updated <= 0)
		return;

	listsize = h/8-1;

	if (serverinfo_sources_pos > serverinfo_sources_disp + listsize - 1)
		serverinfo_sources_disp = serverinfo_sources_pos - listsize + 1;
	if (serverinfo_sources_disp > sourcesn_updated - listsize)
		serverinfo_sources_disp = max(sourcesn_updated - listsize, 0);
	if (serverinfo_sources_pos < serverinfo_sources_disp)
		serverinfo_sources_disp = serverinfo_sources_pos;

	UI_Print(x, y, " type    name", true);
	for (i=0; i < listsize; i++)
	{
		char buf[128], buf2[16];

		if (serverinfo_sources_disp + i >= sourcesn_updated)
			break;

		strlcpy(buf2, SB_Source_Type_Name(sources[serverinfo_sources_disp+i]->type), sizeof (buf2));

		snprintf(buf, sizeof (buf), "%s   %s", buf2, sources[serverinfo_sources_disp+i]->name);
		buf[w/8] = 0;

		UI_Print(x, y+i*8+8, buf,
				IsInSource(sources[serverinfo_sources_disp+i], s));

		if (serverinfo_sources_pos == serverinfo_sources_disp+i)
			UI_DrawCharacter(x+8*7, y+i*8+8, '\x8D');
	}
}

const char *SB_Source_Type_Location_Name(sb_source_type_t type)
{
	switch (type) {
		case type_master: return "addr";
		case type_file: return "file";
		case type_url: return "url";
		case type_dummy: return "dummy";
		default:
				 Sys_Error("SB_Source_Type_Location_Name(): Invalid sb_source_type_t type");
				 return "ERROR";
	}
}

void Add_Source_Draw(void)
{
	int x, y, w, h;
	w = 176;
	h = 72;
	x = (vid.width - w)/2;
	y = (vid.height - h)/2;
	x = (x/8) * 8;
	y = (y/8) * 8;

	Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

	x-=4;
	y-=4;
	w+=8;
	h+=8;

	UI_Print_Center(x, y+4, w, "Create new source", true);

	UI_Print(x+4, y + 20, "type", newsource_pos==0);
	UI_Print(x+54, y + 20, SB_Source_Type_Name(newsource_type), newsource_pos==0);
	if (newsource_pos == 0)
		Draw_Character (x+42, y+20, 13);

	UI_Print(x+4, y + 30, "name", newsource_pos==1);
	CEditBox_Draw(&edit1, x+54, y+30, newsource_pos==1);
	if (newsource_pos == 1)
		Draw_Character (x+42, y+30, 13);

	UI_Print(x+4, y + 40, SB_Source_Type_Location_Name(newsource_type), newsource_pos==2);
	CEditBox_Draw(&edit2, x+54, y+40, newsource_pos==2);
	if (newsource_pos == 2)
		Draw_Character (x+42, y+40, 13);

	UI_Print_Center(x+4, y+60, w, "accept", newsource_pos==3);
	if (newsource_pos == 3)
		Draw_Character (x+59, y+60, 13);

	UI_Print_Center(x+4, y+70, w, "cancel", newsource_pos==4);
	if (newsource_pos == 4)
		Draw_Character (x+59, y+70, 13);
}


void SB_Sources_Draw (int x, int y, int w, int h)
{
	int i, listsize;
	char line[1024];
	SYSTEMTIME curtime;

	if (sourcesn <= 0)
		return;

	GetLocalTime(&curtime);

	if (resort_sources)
	{
		Sort_Sources();
		resort_sources = 0;
	}

	listsize = (int)(h/8) - (sb_status.value ? 3 : 0);

	UI_Print_Center(x, y, w, " type   name             servs updated", true);

	listsize--;     // subtract one line (column titles)

	if (Sources_pos > Sources_disp + listsize - 1)
		Sources_disp = Sources_pos - listsize + 1;
	if (Sources_disp > sourcesn - listsize)
		Sources_disp = max(sourcesn - listsize, 0);
	if (Sources_pos < Sources_disp)
		Sources_disp = Sources_pos;

	for (i = 0; i < listsize; i++)
	{
		char type[10], time[10];
		source_data *s = sources[Sources_disp + i];

		int sourcenum = Sources_disp + i;
		if (sourcenum >= sourcesn)
			break;

		strlcpy(type, SB_Source_Type_Name(s->type), sizeof (type));

		if (s->type == type_dummy)
			strlcpy (time, " n/a ", sizeof (time));
		else
			if (s->last_update.wYear)
			{
				if (s->last_update.wYear != curtime.wYear)
					snprintf(time, sizeof (time), "%4dy", s->last_update.wYear);
				else if (s->last_update.wMonth != curtime.wMonth ||
						s->last_update.wDay != curtime.wDay)
					snprintf(time, sizeof (time),  "%02d-%02d", s->last_update.wMonth, s->last_update.wDay);
				else
					snprintf(time, sizeof (time),  "%2d:%02d",
							s->last_update.wHour,
							s->last_update.wMinute);
			}
			else
				strlcpy (time, "never", sizeof (time));

		snprintf(line, sizeof (line), "%s %c%-17.17s %4d  %s ", type,
				sourcenum==Sources_pos ? 141 : ' ',
				s->name, s->serversn, time);

		UI_Print_Center(x, y+8*(i+1), w, line, s->checked);
	}

	//
	// status line
	//
	if (sb_status.value)
	{
		int total_servers = 0;
		int sel_servers = 0, sel_sources = 0;

		memset(line, '\x1E', w/8);
		line[w/8] = 0;
		line[w/8-1] = '\x1F';
		line[0] = '\x1D';
		UI_Print(x, y+h-24, line, false);

		// get stats
		for (i=0; i < sourcesn; i++)
		{
			total_servers += sources[i]->serversn;
			if (sources[i]->checked)
			{
				sel_servers += sources[i]->serversn;
				sel_sources ++;
			}
		}

		snprintf(line, sizeof (line), "%d sources selected (%d servers)", sel_sources, sel_servers);
		UI_Print_Center(x, y+h-16, w, line, false);

		snprintf(line, sizeof (line), "of %d total (%d servers)", sourcesn, total_servers);
		UI_Print_Center(x, y+h-8, w, line, false);
	}

	// adding source
	if (adding_source)
		Add_Source_Draw();
}


void SB_Players_Draw (int x, int y, int w, int h)
{
	int i, listsize;
	char line[2048];
	int hw = min(w/8 - 16, 60) -4; // hostname width (in chars)

	if (ping_phase)
		return;

	if (searchtype != search_player  ||  searchtime + SEARCH_TIME < cls.realtime)
		searchtype = search_none;

	if (rebuild_servers_list)
		Rebuild_Servers_List();

	if (rebuild_all_players  &&  show_serverinfo == NULL)
		Rebuild_All_Players();

	if (resort_all_players)
	{
		Sort_All_Players();
		resort_all_players = 0;
	}

	if (all_players_n <= 0)
		return;

	Players_pos = max(Players_pos, 0);
	Players_pos = min(Players_pos, all_players_n-1);

	listsize = (int)(h/8) - (sb_status.value ? 3 : 0);

	//UI_Print_Center(x, y, w, "name            server              ", true);
	snprintf(line, sizeof (line), "name            %-*s png", hw, "server");
	UI_Print_Center(x, y, w, line, true);

	listsize--;     // subtract one line (column titles)

	if (Players_pos > Players_disp + listsize - 1)
		Players_disp = Players_pos - listsize + 1;
	if (Players_disp > all_players_n - listsize)
		Players_disp = max(all_players_n - listsize, 0);
	if (Players_pos < Players_disp)
		Players_disp = Players_pos;

	for (i = 0; i < listsize; i++)
	{
		player_host *s = all_players[Players_disp + i];

		int num = Players_disp + i;
		if (num >= all_players_n)
			break;

		snprintf(line, sizeof (line), "%-15s%c%-*.*s %3s",
				s->name, num==Players_pos ? 141 : ' ',
				hw, hw, strlen(s->serv->display.name) > 0 ? s->serv->display.name : s->serv->display.ip, s->serv->display.ping);

		UI_Print_Center(x, y+8*(i+1), w, line, num == Players_pos);
	}

	//
	// status line
	//
	if (sb_status.value  &&  all_players_n > 0)
		Draw_Server_Statusbar(x, y, w, h, all_players[Players_pos]->serv, Players_pos, all_players_n);
}

void SB_SourceUnmarkAll(void)
{
	int i;
	for (i=0; i < sourcesn; i++)
		Unmark_Source(sources[i]);
}

void SB_SourceMark(void)
{
	int i;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("usage:  sb_sourcemark <source-name>\n");
		return;
	}

	for (i=0; i < sourcesn; i++)
		if (!strcmp(sources[i]->name, Cmd_Argv(1)))
		{
			Mark_Source(sources[i]);
			break;
		}
}

void MarkDefaultSources(void) {
	int i;
	for (i=0; i < sourcesn; i++)
	{
		if (!strcmp(sources[i]->name, "id limbo") || 
				!strcmp(sources[i]->name, "Global") ||
				!strcmp(sources[i]->name, "QuakeServers.net") ||
				!strcmp(sources[i]->name, "Asgaard")
		   )
			sources[i]->checked = 1;
	}
}


void WriteSourcesConfiguration(FILE *f)
{
	int i;
	fprintf(f, "sb_sourceunmarkall\n");
	for (i=0; i < sourcesn; i++)
		if (sources[i]->checked)
			fprintf(f, "sb_sourcemark \"%s\"\n", sources[i]->name);
}

void SB_Source_Add_f(void)
{
	sb_source_type_t type = type_dummy;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <name> <address/filename> <master|file>\n", Cmd_Argv(0));
		if (Cmd_Argc() != 1) {
			Com_Printf("You supplied incorrect amount of arguments\n");
		}
		return;
	}

	if (strcmp(Cmd_Argv(3), "master") == 0) {
		type = type_master;
	}
	else if (strcmp(Cmd_Argv(3), "file") == 0) {
		type = type_file;
	}
	else if (strcmp(Cmd_Argv(3), "url") == 0) {
		type = type_url;
	}
	else {
		Com_Printf("Usage: %s <name> <address/filename> <master|file>\n", Cmd_Argv(0));
		Com_Printf("Last argument must be 'master' or 'file'\n");
		return;
	}

	SB_Source_Add(Cmd_Argv(1), Cmd_Argv(2), type);
}

static void SB_NewSource_Shift(void)
{
	// excluding dummy, presuming dummy is last
	newsource_type = (newsource_type + 1) % (type_dummy);
}

void Add_Source_Key(int key, wchar unichar)
{
	switch (key)
	{
		case K_HOME:
			if (keydown[K_CTRL])
				newsource_pos = 0;
			break;
		case K_END:
			if (keydown[K_CTRL])
				newsource_pos = 4;
			break;
		case K_UPARROW:
		case K_MWHEELUP:
			newsource_pos--;
			break;
		case K_TAB:
			keydown[K_SHIFT]?newsource_pos--:newsource_pos++;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			newsource_pos++;
			break;
		case K_ENTER:
		case '+':
		case '=':
		case '-':
			switch (newsource_pos)
			{
				case 0:
					SB_NewSource_Shift();
					break;
				case 2:
				case 3:
					if (key == K_ENTER)
					{
						int newpos;

						newpos = SB_Source_Add(edit1.text, edit2.text, newsource_type);
						if (newpos >= 0) {
							Sources_pos = newpos;
						}

						adding_source = 0;
					}
					break;
				case 4: // cancel
					if (key == K_ENTER)
						adding_source = 0;
					break;
			}
			break;
		case K_ESCAPE:
			adding_source = 0;
			break;
		case K_BACKSPACE:
			if (newsource_pos != 1  &&  newsource_pos != 2)
				adding_source = 0;
			break;
	}

	if ((!keydown[K_CTRL] || tolower(key)=='v') && !keydown[K_ALT])
	{
		if (newsource_pos == 1)
			CEditBox_Key(&edit1, key, unichar);
		if (newsource_pos == 2)
			CEditBox_Key(&edit2, key, unichar);
	}

	// Make sure value stays within limits and enable field wrapping 0..4->0
	newsource_pos = (newsource_pos + 5) % 5;
}

void Add_Server_Key(int key, wchar unichar)
{
	switch (key)
	{
		case K_HOME:
			if (keydown[K_CTRL])
				newserver_pos = 0;
			break;
		case K_END:
			if (keydown[K_CTRL])
				newserver_pos = 4;
			break;
		case K_UPARROW:
		case K_MWHEELUP:
			newserver_pos--;
			break;
		case K_TAB:
			keydown[K_SHIFT] ? newserver_pos-- : newserver_pos++;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			newserver_pos++;
			break;
		case K_ENTER:
		case '+':
		case '=':
		case '-':
			switch (newserver_pos)
			{
				case 0:
				case 1: // accept
					if (key == K_ENTER)
					{
						if (strlen(edit1.text) > 0)
						{
							AddUnboundServer(edit1.text);
						}
						adding_server = 0;
					}
					break;
				case 2: // cancel
					if (key == K_ENTER)
						adding_server = 0;
					break;
			}
			break;
		case K_ESCAPE:
			adding_server = 0;
			break;
		case K_BACKSPACE:
			if (newserver_pos != 0)
				adding_server = 0;
			break;
	}

	if ((!keydown[K_CTRL] || tolower(key)=='v') && !keydown[K_ALT])
	{
		if (newserver_pos == 0)
			CEditBox_Key(&edit1, key, unichar);
	}

	// Make sure value stays within limits and enable field wrapping 0..2->0
	newserver_pos = (newserver_pos + 3) % 3;
}

qbool SearchNextServer (int pos)
{
	int i;
	char tmp[1024];

	for (i = pos; i < serversn_passed; i++) {
		strlcpy (tmp, servers[i]->display.name, sizeof (tmp));
		FunToSort (tmp);

		if (strstr (tmp, searchstring)) {
			Servers_pos = i;
			return true;
		}
	}

	return false;
}

static void SB_Servers_Toggle_Column_Show(int colidx)
{
	if (colidx > 0 && colidx < sb_columns_size) {
		Cvar_Toggle(sb_columns[colidx].showvar);
	}
}

static void SB_Servers_Toggle_Column_Sort(char key)
{
	char buf[32];
	if ((sb_sortservers.string[0] == '-' && sb_sortservers.string[1] == key)
			|| sb_sortservers.string[0] == key)
	{
		if (sb_sortservers.string[0] == '-')
		{
			strlcpy (buf, sb_sortservers.string + 1, sizeof (buf));
		}
		else
		{
			buf[0] = '-';
			strlcpy (buf + 1, sb_sortservers.string, sizeof (buf) - 1);
		}
	} else {
		buf[0] = key;
		strlcpy (buf + 1, sb_sortservers.string, sizeof (buf) - 1);
	}

	Cvar_Set(&sb_sortservers, buf);
	resort_servers = 1;
}

int SB_Servers_Key(int key)
{
	if (serversn_passed <= 0 && key == K_BACKSPACE) {
		searchstring[0] = '\0';
		searchtype = search_none;
		resort_servers = 1;
	}

	if (serversn_passed <= 0  &&  (key != K_SPACE || keydown[K_ALT])
			&& tolower(key) != 'n'  && key != K_INS)
		return false;

	if (key == K_SPACE) {
		GetServerPingsAndInfos(keydown[K_CTRL]);
		return true;
	}

	if (!keydown[K_CTRL] && !keydown[K_ALT] && key > ' ' && key <= '}')  // search
	{
		int len;
		char c = tolower(key);
		if (searchtype != search_server) {
			searchtype = search_server;
			searchstring[0] = 0;
		}
		searchtime = cls.realtime;

		len = strlen(searchstring);
		if (len < MAX_SEARCH) {
			searchstring[len] = c;
			searchstring[len+1] = 0;
			resort_servers = 1;
		}

		return true;
	} else {
		searchtype = search_none;
		switch (key)
		{
			case K_BACKSPACE:
				searchstring[0] = '\0';
				resort_servers = 1;
				break;
			case K_INS:
			case 'n':	// new server
				newserver_pos = 0;
				CEditBox_Init(&edit1, 14, 64);
				adding_server = 1;
				break;
			case 'j':
			case 'p':
				Join_Server(servers[Servers_pos]);
				break;
			case 'o':
			case 's':
				Observe_Server(servers[Servers_pos]);
				break;
			case 'r':
				GetServerInfo(servers[Servers_pos]);
				break;
			case K_UPARROW:
				Servers_pos--;
				break;
			case K_MWHEELUP:
				Servers_disp -= MWHEEL_SCROLL_STEP;
				Servers_pos -= MWHEEL_SCROLL_STEP;
				break;

			case K_DOWNARROW:
				Servers_pos++;
				break;
			case K_MWHEELDOWN:
				Servers_disp += MWHEEL_SCROLL_STEP;
				Servers_pos += MWHEEL_SCROLL_STEP;
				break;

			case K_HOME:
				Servers_pos = 0;
				break;
			case K_END:
				Servers_pos = serversn_passed-1;
				break;
			case K_PGUP:
				Servers_pos -= 10;
				break;
			case K_PGDN:
				Servers_pos += 10;
				break;
			case K_MOUSE1:
			case K_ENTER:
				Serverinfo_Start(servers[Servers_pos]);
				break;
			case K_SPACE:
				GetServerPingsAndInfos(keydown[K_CTRL]);
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':   // sorting mode
				if (keydown[K_ALT]) // fixme
				{
					SB_Servers_Toggle_Column_Sort(key);
				} else if (keydown[K_CTRL]) {
					SB_Servers_Toggle_Column_Show(key - '1');
				}
				break;
			case 'c':	// copy server to clipboard
				CopyServerToClipboard(servers[Servers_pos]);
				break;
			case 'v':	// past server into console
				PasteServerToConsole(servers[Servers_pos]);
				break;
			default: return false;
		}
	}

	Servers_pos = max(Servers_pos, 0);
	Servers_pos = min(Servers_pos, serversn-1);
	return true;
}

void Serverinfo_Key(int key)
{
	switch (key)
	{
		case K_MOUSE1:
		case 'x': // x was once used for best route connection
		case K_ENTER:
			if (serverinfo_pos != 2)
			{
				if (show_serverinfo->qwfwd) {
					SB_Select_QWfwd(show_serverinfo);
				} else if (keydown[K_CTRL]) {
					Observe_Server(show_serverinfo);
				} else {
					Join_Server(show_serverinfo);
				}
			}
			else
				Serverinfo_Sources_Key(key);
			break;
		case K_MOUSE2:
		case K_ESCAPE:
		case K_BACKSPACE:
			Serverinfo_Stop();
			break;
		case K_TAB:
			if (keydown[K_SHIFT]) {
				serverinfo_pos--;
			}
			else {
				serverinfo_pos++;
			}
			break;
		case K_LEFTARROW:
			serverinfo_pos--;
			break;
		case K_RIGHTARROW:
			serverinfo_pos++;
			break;
		case 'q':
			Cbuf_AddText("observeqtv ");
			Cbuf_AddText(NET_AdrToString(show_serverinfo->address));
			Cbuf_AddText("\n");
			break;
		case 'j':
		case 'p':
			Join_Server(show_serverinfo);
			break;
		case 'n':
			Join_Server_Direct(show_serverinfo);
			break;
		case 'o':
		case 's':
			Observe_Server(show_serverinfo);
			break;
		case 't':
			{
				if (!testing_connection)
				{
					char buf[256];
					snprintf(buf, sizeof (buf), "%d.%d.%d.%d",
							show_serverinfo->address.ip[0],
							show_serverinfo->address.ip[1],
							show_serverinfo->address.ip[2],
							show_serverinfo->address.ip[3]);
					SB_Test_Init(buf);
					testing_connection = 1;
				} else {
					testing_connection = 0;
				}
				break;
			}
		case 'c':   // copy server to clipboard
			CopyServerToClipboard(show_serverinfo);
			break;
		case 'v':   // past server into console
			PasteServerToConsole(show_serverinfo);
			break;
		case 'i':
			SB_PingTree_DumpPath(&show_serverinfo->address);
			break;
		case K_MWHEELUP:
	        case K_UPARROW:
	        case K_PGUP:
	          if (!keydown[K_CTRL]) {
		    Servers_pos--;
		    Servers_pos = max(0, Servers_pos);
		    Serverinfo_Change(servers[Servers_pos]);
		    break;
	          }
	        case K_MWHEELDOWN:
	        case K_DOWNARROW:
	        case K_PGDN:
	          if (!keydown[K_CTRL]) {
		    Servers_pos++;
		    Servers_pos = min(serversn_passed - 1, Servers_pos);
		    Serverinfo_Change(servers[Servers_pos]);
	          }
		default:
			switch (serverinfo_pos)
			{
				case 0: // players list
					Serverinfo_Players_Key(key);
					break;
				case 1: // serverinfo / rules
					Serverinfo_Rules_Key(key);
					break;
				case 2: // sources
					Serverinfo_Sources_Key(key);
					break;
				default:
					;
			}
	}

	serverinfo_pos = (serverinfo_pos + 3) % 3;
}

void Serverinfo_Players_Key(int key)
{
	switch (key)
	{
		case K_UPARROW:
		case K_MWHEELUP:
			serverinfo_players_pos--;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			serverinfo_players_pos++;
			break;
		case K_HOME:
			serverinfo_players_pos = 0;
			break;
		case K_END:
			serverinfo_players_pos = 999;
			break;
		default:
			;
	}
}

void Serverinfo_Rules_Key(int key)
{
	switch (key)
	{
		case K_UPARROW:
		case K_MWHEELUP:
			serverinfo_rules_pos--;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			serverinfo_rules_pos++;
			break;
		case K_HOME:
			serverinfo_rules_pos = 0;
			break;
		case K_END:
			serverinfo_rules_pos = 999;
			break;
		default:
			;
	}
}

void Serverinfo_Sources_Key(int key)
{
	switch (key)
	{
		case K_UPARROW:
		case K_MWHEELUP:
			serverinfo_sources_pos--;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			serverinfo_sources_pos++;
			break;
		case K_HOME:
			serverinfo_sources_pos = 0;
			break;
		case K_END:
			serverinfo_sources_pos = 999;
			break;
		case K_INS:
			if (!IsInSource(sources[serverinfo_sources_pos], show_serverinfo))
				AddToFileSource(sources[serverinfo_sources_pos], show_serverinfo);
			break;
		case K_DEL:
			if (IsInSource(sources[serverinfo_sources_pos], show_serverinfo))
				RemoveFromFileSource(sources[serverinfo_sources_pos], show_serverinfo);
			break;
		case K_ENTER:
			if (IsInSource(sources[serverinfo_sources_pos], show_serverinfo))
				RemoveFromFileSource(sources[serverinfo_sources_pos], show_serverinfo);
			else
				AddToFileSource(sources[serverinfo_sources_pos], show_serverinfo);
		default:
			;
	}

	serverinfo_sources_pos = max(serverinfo_sources_pos, 0);
	serverinfo_sources_pos = min(serverinfo_sources_pos, sourcesn_updated-1);
}

void SB_RemoveSourceProc(void)
{
	SB_Source_Remove(Sources_pos);

	if (Sources_pos >= sourcesn) {
		Sources_pos = sourcesn - 1;
	}
}

int SB_Sources_Key(int key)
{
	int i;

	if (sourcesn <= 0)
		return false;

	switch (key)
	{
		case K_INS:
		case 'n':       // new source
			newsource_pos = 0;
			newsource_type = type_file;
			CEditBox_Init(&edit1, 16, 25);
			CEditBox_Init(&edit2, 16, 100);
			adding_source = 1;
			break;
		case K_DEL:
		case 'd':       // remove source
			if (sources[Sources_pos]->type != type_dummy) {
				char tmp[64];
				snprintf(&tmp[0], sizeof(tmp), "Remove %-.20s", sources[Sources_pos]->name);
				SB_Confirmation(tmp, SB_RemoveSourceProc);
			}
			break;
		case 'u':
			Update_Source(sources[Sources_pos]); 
			break;
		case K_UPARROW:
		case K_MWHEELUP:
			Sources_pos--;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			Sources_pos++;
			break;
		case K_HOME:
			Sources_pos = 0;
			break;
		case K_END:
			Sources_pos = sourcesn-1;
			break;
		case K_PGUP:
			Sources_pos -= 10;
			break;
		case K_PGDN:
			Sources_pos += 10;
			break;
		case K_MOUSE1:
		case K_ENTER:
			Toggle_Source(sources[Sources_pos++]);
			break;
		case ']':
			Toggle_Source(sources[Sources_pos]);
			break;
		case K_SPACE:
			SB_Sources_Update_Begin(keydown[K_CTRL]);
			break;
		case '=':
		case '+':   // select all sources
			for (i=0; i < sourcesn; i++)
				Mark_Source(sources[i]);
			break;
		case '-':   // select none
			for (i=0; i < sourcesn; i++)
				Unmark_Source(sources[i]);
			break;
		case '*':   // invert selection
			for (i=0; i < sourcesn; i++)
				Toggle_Source(sources[i]);
			break;
		case '1':
		case '2':
		case '3':   // sorting mode
			if ((sb_sortsources.string[0] == '-' && sb_sortsources.string[1] == key)
					|| sb_sortsources.string[0] == key)
			{
				char buf[32];
				if (sb_sortsources.string[0] == '-')
					strlcpy(buf, sb_sortsources.string+1, sizeof (buf));
				else
				{
					buf[0] = '-';
					strlcpy(buf+1, sb_sortsources.string, sizeof (buf)-1);
				}
				Cvar_Set(&sb_sortsources, buf);
			}
			else
			{
				char buf[32];
				buf[0] = key;
				strlcpy(buf+1, sb_sortsources.string, sizeof (buf)-1);
				Cvar_Set(&sb_sortsources, buf);
			}
			resort_sources = 1;
			break;
		default: return false;
	}

	Sources_pos = max(Sources_pos, 0);
	Sources_pos = min(Sources_pos, sourcesn-1);
	return true;
}

qbool SearchNextPlayer(int pos)
{
	int i;
	char tmp[1024];

	for (i = pos; i < all_players_n; i++) {
		strlcpy (tmp, all_players[i]->name, sizeof (tmp));
		FunToSort (tmp);

		if (strstr (tmp, searchstring)) {
			Players_pos = i;
			return true;
		}
	}

	return false;
}

int SB_Players_Key(int key)
{
	int i;

	if (all_players_n <= 0  &&  key != K_SPACE)
		return false;

	if ((keydown[K_ALT]  ||  searchtype == search_player)  &&
			key >= ' '  &&  key <= '}')  // search
	{
		int len;
		char c = tolower(key);
		if (searchtype != search_player)
		{
			searchtype = search_player;
			searchstring[0] = 0;
		}
		searchtime = cls.realtime;

		len = strlen(searchstring);
		if (len < MAX_SEARCH)
		{
			searchstring[len] = c;
			searchstring[len+1] = 0;

			if (!SearchNextPlayer(Players_pos))
				if (!SearchNextPlayer(0))
					strlcpy (searchstring, "not found", sizeof (searchstring));  // not found
		}
		return true;
	}
	else
	{
		searchtype = search_none;
		switch (key)
		{
			case 'j':
			case 'p':
				Join_Server(all_players[Players_pos]->serv);
				break;
			case 'o':
			case 's':
				Observe_Server(all_players[Players_pos]->serv);
				break;
			case K_SPACE:
				GetServerPingsAndInfos(keydown[K_CTRL]);
				break;

			case K_INS: // go to servers -- locate
				Servers_pos = 0;
				for (i=0; i < serversn_passed; i++)
				{
					if (servers[i] == all_players[Players_pos]->serv)
					{
						Servers_pos = i;
						break;
					}
				}
				CTab_SetCurrentId(&sb_tab, SBPG_SERVERS);
				break;

			case K_UPARROW:
			case K_MWHEELUP:
				Players_pos--;
				break;
			case K_DOWNARROW:
			case K_MWHEELDOWN:
				Players_pos++;
				break;
			case K_HOME:
				Players_pos = 0;
				break;
			case K_END:
				Players_pos = all_players_n-1;
				break;
			case K_PGUP:
				Players_pos -= 10;
				break;
			case K_PGDN:
				Players_pos += 10;
				break;
			case K_MOUSE1:
			case K_ENTER:
				Serverinfo_Start(all_players[Players_pos]->serv);
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':   // sorting mode
				if ((sb_sortplayers.string[0] == '-' && sb_sortplayers.string[1] == key)
						|| sb_sortplayers.string[0] == key)
				{
					char buf[32];
					if (sb_sortplayers.string[0] == '-')
						strlcpy(buf, sb_sortplayers.string+1, sizeof (buf));
					else
					{
						buf[0] = '-';
						strlcpy(buf+1, sb_sortplayers.string, sizeof (buf)-1);
					}
					Cvar_Set(&sb_sortplayers, buf);
				}
				else
				{
					char buf[32];
					buf[0] = key;
					strlcpy(buf+1, sb_sortplayers.string, sizeof (buf)-1);
					Cvar_Set(&sb_sortplayers, buf);
				}
				resort_all_players = 1;
				break;
			case 'c':
				CopyServerToClipboard(all_players[Players_pos]->serv);
				break;
			case 'v':   // past server into console
				PasteServerToConsole(all_players[Players_pos]->serv);
				break;
			default: return false;
		}
	}

	Players_pos = max(Players_pos, 0);
	Players_pos = min(Players_pos, all_players_n-1);
	return true;
}

qbool SB_Servers_Mouse_Event(const mouse_state_t *ms)
{
	if (show_serverinfo) {
	}
	else if (ms->button_up == 1) {
		if (mouse_in_header_row) {
			SB_Servers_Toggle_Column_Sort('0' + mouse_hovered_column + 1);
		}
		else {
			SB_Servers_Key(K_MOUSE1);
		}
	}
	else {
		double mouse_row = ms->y / 8.0 - 1.0;
		if (mouse_row < 0.0) {
			mouse_in_header_row = true;
			mouse_header_pos_y = ms->x;
		}
		else {
			mouse_in_header_row = false;
			Servers_pos = Servers_disp + (int) mouse_row;
			Servers_pos = bound(0, Servers_pos, serversn - 1);
		}
	}
	return true;
}

qbool SB_Sources_Mouse_Event(const mouse_state_t *ms)
{
	if (show_serverinfo) return false;

	if (ms->button_up == 1)
	{
		SB_Sources_Key(K_MOUSE1);
		return true;
	}
	Sources_pos = Sources_disp + ms->y / 8 - 1;
	Sources_pos = bound(0, Sources_pos, sourcesn - 1);
	return true;
}

qbool SB_Players_Mouse_Event(const mouse_state_t *ms)
{
	if (show_serverinfo) return false;

	Players_pos = Players_disp + ms->y / 8 - 1;
	Players_pos = bound(0, Players_pos, all_players_n - 1);
	return true;
}

void SB_Specials_Draw(void)
{
	if (show_serverinfo) Serverinfo_Draw();
	if (ping_phase) PingPhase_Draw();
	if (updating_sources) UpdatingSources_Draw();
	if (confirmation) SB_Confirmation_Draw();
}

qbool SB_Specials_Key(int key, wchar unichar)
{
	if (confirmation)
	{
		SB_Confirmation_Key(key);
		return true;
	}

	if ((ping_phase || updating_sources) && (key == '`' || key == '~' || key == 'm')) {
		M_LeaveMenus();
		// Con_ToggleConsole_f ();
		return true;
	}

	if ((key == K_ESCAPE || key == K_MOUSE2)  &&
			show_serverinfo == NULL  &&
			!adding_source  &&
			!adding_server  &&
			ping_phase == 0  &&
			!updating_sources)  // exit from browser to main menu
	{
		M_Menu_Main_f();
		return true;
	}

	if (show_serverinfo)
	{
		Serverinfo_Key(key);
		return true;
	}

	if (adding_source)
	{
		Add_Source_Key(key, unichar);
		return true;
	}

	if (adding_server)
	{
		Add_Server_Key(key, unichar);
		return true;
	}

	if (ping_phase || updating_sources)  // no keys when pinging
	{
		if (!abort_ping  &&  (key == K_ESCAPE || key == K_BACKSPACE))
			abort_ping = 1;
		return true;
	}

	return false;
}

//
// sorting routines
//

static int Servers_Compare_Ping_Func(const server_data *s1, const server_data *s2)
{
	int p1 = (sb_findroutes.integer && s1->bestping > 0) ? s1->bestping : s1->ping;
	int p2 = (sb_findroutes.integer && s2->bestping > 0) ? s2->bestping : s2->ping;
	int diff = p1 - p2;

	return diff;
}

static int Servers_Occupancy_Sort_Rating(server_occupancy oc) {
	if (oc == SERVER_EMPTY) {
		return 0;
	}
	else if (oc == SERVER_FULL) {
		return 1;
	}
	else if (oc == SERVER_NONEMPTY) {
		return 2;
	}
	else {
		Com_DPrintf("Illegal server occupancy %d\n", oc);
		return -1;
	}
}

static int Servers_Compare_Natural(const server_data *s1, const server_data *s2) {
	if (s1->occupancy != s2->occupancy) {
		int o1 = Servers_Occupancy_Sort_Rating(s1->occupancy);
		int o2 = Servers_Occupancy_Sort_Rating(s2->occupancy);
		return o2 - o1;
	}
	else {
		server_occupancy oc = s1->occupancy;
		switch (oc) {
			case SERVER_NONEMPTY:
				return Servers_Compare_Ping_Func(s1, s2);
			case SERVER_FULL:
				return s2->playersn - s1->playersn;
			case SERVER_EMPTY:
				return Servers_Compare_Ping_Func(s1, s2);
			default:
				return 0;
		}
	}
}

int Servers_Compare_Func(const void * p_s1, const void * p_s2)
{
	int reverse = 0;
	char *sort_string = sb_sortservers.string;
	const server_data *s1 = *((server_data **)p_s1);
	const server_data *s2 = *((server_data **)p_s2);

	if (s1->ping < 0  &&  s2->ping >= 0)
		return 1;
	if (s2->ping < 0  &&  s1->ping >= 0)
		return -1;

	if (!s1->passed_filters  &&  s2->passed_filters)
		return 1;
	if (!s2->passed_filters  &&  s1->passed_filters)
		return -1;
	if (!s1->passed_filters  &&  !s2->passed_filters)
		return s1 - s2;

	if (sort_string[0] == '\0') {
		return Servers_Compare_Natural(s1, s2);
	}

	while (true)
	{
		int d;  // difference

		if (*sort_string == '-')
		{
			reverse = 1;
			sort_string++;
			continue;
		}

		switch (*sort_string++)
		{
			case '1':
				d = funcmp(s1->display.name, s2->display.name);
				break;
			case '2':
				d = memcmp(&(s1->address.ip), &(s2->address.ip), 4);
				if (!d)
					d = ntohs(s1->address.port) - ntohs(s2->address.port);
				break;
			case '3':
				d = Servers_Compare_Ping_Func(s1, s2);
				break;
			case '4':
				d = Q_strcmp2(s1->display.gamedir, s2->display.gamedir);
				break;
			case '5':
				d = Q_strcmp2(s1->display.map, s2->display.map);
				break;
			case '6':
				d = s1->playersn - s2->playersn;
				break;
			case '7':
				d = Q_strcmp2(s1->display.fraglimit, s2->display.fraglimit);
				break;
			case '8':
				d = Q_strcmp2(s1->display.timelimit, s2->display.timelimit);
				 break;
			default:
				d = s1 - s2;
		}

		if (d)
			return reverse ? -d : d;
	}
}

void Filter_Servers(void)
{
	int i, info_filter_count;
	info_filter_t* info_filters = SB_InfoFilter_Parse(&info_filter_count);

	serversn_passed = 0;
	for (i=0; i < serversn; i++)
	{
		char *tmp;
		server_data *s = servers[i];
		s->passed_filters = 0;

		if (searchstring[0] && !strstri(s->display.name, searchstring)) {
			continue;
		}

		if (sb_showproxies.integer == 0 && (s->qwfwd || s->qizmo))
			continue; // hide

		if (sb_showproxies.integer == 2 && !(s->qwfwd || s->qizmo))
			continue; // exclusive

		if (sb_hidedead.value  &&  s->ping < 0)
			continue;

		if (!s->qizmo && !s->qwfwd) {
			if (sb_hideempty.value  &&  s->playersn + s->spectatorsn <= 0)
				continue;

			if (sb_hidenotempty.value  &&  s->playersn + s->spectatorsn > 0)
				continue;
		}

		if (sb_hidehighping.integer && s->ping > sb_pinglimit.integer)
			continue;

		tmp = ValueForKey(s, "maxclients");
		if (sb_hidefull.value  &&  s->playersn >= (tmp ? atoi(tmp) : 255))
			continue;

		if (!SB_InfoFilter_Exec(info_filters, info_filter_count, s)) {
			continue;
		}

		s->passed_filters = 1;  // passed
		serversn_passed++;
	}

	SB_InfoFilter_Free(info_filters, info_filter_count);

	return;
}

void Sort_Servers (void)
{
	SB_ServerList_Lock();
	Filter_Servers();
	qsort(servers, serversn, sizeof(servers[0]), Servers_Compare_Func);
	SB_ServerList_Unlock();
}


//
// build all players list
//

int rebuild_all_players = 0;
int resort_all_players = 0;

int All_Players_Compare_Func(const void * p_p1, const void * p_p2)
{
	int reverse = 0;
	char *sort_string = sb_sortplayers.string;
	const player_host *p1 = *((player_host **)p_p1);
	const player_host *p2 = *((player_host **)p_p2);
	server_data *s1 = p1->serv;
	server_data *s2 = p2->serv;

	if (s1->ping < 0  &&  s2->ping >= 0)
		return 1;
	if (s2->ping < 0  &&  s1->ping >= 0)
		return -1;

	while (true)
	{
		int d;  // difference

		if (*sort_string == '-')
		{
			reverse = 1;
			sort_string++;
			continue;
		}

		switch (*sort_string++)
		{
			case '1':
				d = funcmp(s1->display.name, s2->display.name);
				break;
			case '2':
				d = memcmp(&(s1->address.ip), &(s2->address.ip), 4);
				if (!d)
					d = ntohs(s1->address.port) - ntohs(s2->address.port);
				break;
			case '3':
				d = s1->ping - s2->ping;
				break;
			case '4':
				d = Q_strcmp2(s1->display.gamedir, s2->display.gamedir);
				break;
			case '5':
				d = Q_strcmp2(s1->display.map, s2->display.map);
				break;
			case '6':
				d = s1->playersn - s2->playersn;
				break;
			case '7':
				d = Q_strcmp2(s1->display.fraglimit, s2->display.fraglimit);
				break;
			case '8':
				d = Q_strcmp2(s1->display.timelimit, s2->display.timelimit);
				break;
			case '9':
				d = funcmp(p1->name, p2->name);
				break;
			default:
				d = p1 - p2;
		}

		reverse = 0;

		if (d)
			return reverse ? -d : d;
	}
}

void Sort_All_Players(void)
{
	qsort(all_players, all_players_n, sizeof(all_players[0]), All_Players_Compare_Func);
}

void Rebuild_All_Players(void)
{
	int i, j, players = 0;

	// clear
	if (all_players != NULL)
	{
		for (i = 0; i < all_players_n; i++)
			Q_free(all_players[i]);

		Q_free(all_players);
	}

	// count players
	for (i = 0; i < serversn; i++)
		players += servers[i]->playersn + servers[i]->spectatorsn;

	// alloc memory
	all_players = (player_host **) Q_malloc (players * sizeof (player_host *));

	// make players
	all_players_n = 0;
	for (i = 0; i < serversn; i++)
	{
		for (j = 0; j < servers[i]->playersn + servers[i]->spectatorsn; j++)
		{
			all_players[all_players_n] = (player_host *) Q_malloc (sizeof (player_host));
			strlcpy (all_players[all_players_n]->name,
					servers[i]->players[j]->name,
					sizeof (all_players[all_players_n]->name));
			all_players[all_players_n]->serv = servers[i];
			all_players_n++;
		}
	}

	resort_all_players = 1;
	rebuild_all_players = 0;
	//Players_pos = 0;
}

void SB_Sources_Update_f(void)
{
	SB_Sources_Update_Begin(true);
}

// Server list serialization.
// When user set sb_listcache 1, the server list will be saved to his home dir after each update
// so that next time the client is started the list is immediately full of servers
// and it's not necessary to do full refresh, just get infos from alive servers is enough
// (which is significantly faster than full refresh).
// Of course users should do full-update of their list after some time so that 
// new servers have a chance to appear.
#define SERIALIZE_FILE_VERSION 1003
void SB_Serverlist_Serialize(FILE *f)
{
	int version = SERIALIZE_FILE_VERSION;
	size_t server_data_size = sizeof(server_data);
	int i;

	// header
	// - version
	fwrite(&version, sizeof(int), 1, f);
	// - server_data struct size
	fwrite(&server_data_size, sizeof(size_t), 1, f);
	// - number of servers
	fwrite(&serversn, sizeof(int), 1, f);
	// - number of filtered servers
	fwrite(&serversn_passed, sizeof(int), 1, f);

	// body
	for (i = 0; i < serversn; i++) {
		server_data t = *servers[i];
		t.keysn = 0; // we don't store the keys
		fwrite(&t, sizeof(server_data), 1, f);
	}
}

int SB_Serverlist_Unserialize(FILE *f)
{
	size_t server_data_size;
	int i;
	int version;
	int serversn_buffer, serversn_passed_buffer;

	if (fread(&version, sizeof(int), 1, f) != 1)
		return -3;
	if (version != SERIALIZE_FILE_VERSION)
		return -1;

	if (fread(&server_data_size, sizeof(size_t), 1, f) != 1)
		return -3;
	if (server_data_size != sizeof(server_data))
		return -1;

	if (fread(&serversn_buffer, sizeof(int), 1, f) != 1)
		return -3;
	if (serversn_buffer > MAX_SERVERS)
		return -2;

	if (fread(&serversn_passed_buffer, sizeof(int), 1, f) != 1)
		return -3;
	if (serversn_passed_buffer > MAX_SERVERS)
		return -2;

	serversn = serversn_buffer;
	serversn_passed = serversn_passed_buffer;

	for (i = 0; i < serversn; i++) {
		server_data *s = (server_data *) Q_malloc(sizeof(server_data));
		int j;

		if (fread(s, sizeof(server_data), 1, f) != 1 ||
				s->playersn < 0 || s->playersn > MAX_CLIENTS || s->spectatorsn < 0 || s->spectatorsn > MAX_CLIENTS) {
			// naive check for corrupted data
			serversn = 0;
			serversn_passed = 0;
			Q_free(s);
			for (j = 0; j < i; j++) {
				Q_free(servers[j]);
			}
			return -3;
		}

		servers[i] = s;

		// Create empty entries because
		// serialized playersn and spectatorsn is not zero, that's needed
		// to not get empty list on "hide empty". Empty list leads to full refresh
		// which makes the un/serialization stuff useless.
		for (j = 0; j < s->playersn + s->spectatorsn; j++) {
			servers[i]->players[j] = Q_calloc(1, sizeof(playerinfo));
		}
	}

	rebuild_servers_list = 0;

	return serversn;
}

void SB_Serverlist_Serialize_f(void)
{
	FILE *f;
	char filename[MAX_OSPATH] = {0};

	snprintf(&filename[0], sizeof(filename), "%s/%s", com_homedir, "servers_data");

	if (!(f	= fopen	(filename, "wb"))) {
		FS_CreatePath(filename);
		if (!(f	= fopen	(filename, "wb"))) {
			Com_Printf ("Couldn't write	%s.\n",	filename);
			return;
		}
	}

	SB_Serverlist_Serialize(f);
	Com_Printf("Wrote server list contents to disk\n");
	fclose(f);
	filesystemchanged = true;
}

void SB_Serverlist_Unserialize_f(void)
{
	FILE *f;
	int err;
	char filename[MAX_OSPATH] = {0};

	snprintf(&filename[0], sizeof(filename), "%s/%s", com_homedir, "servers_data");

	if (!(f	= fopen	(filename, "rb"))) {
		Com_Printf ("Couldn't read %s.\n", filename);
		return;
	}

	err = SB_Serverlist_Unserialize(f);
	if (err > 0) {
		Com_Printf("Successfully read %d servers\n", err);
	}
	else if (err == -1) {
		Com_Printf("Format didn't match\n");
	}
	else if (err == -2) {
		Com_Printf("Format error (servers number too big)\n");
	}
	else if (err == -3) {
		Com_Printf("Corrupted data\n");
	}
	else { // err == 0
		Com_Printf("No servers read\n");
	}

	fclose(f);
}

void SB_ProxyDumpPing(netadr_t adr, short dist)
{
	Com_Printf("%3d.%3d.%3d.%3d:%5d   %d\n",
			adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3], (int) adr.port, dist);
}

void SB_ProxyGetPings_f(void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <ip>\n", Cmd_Argv(0));
		return;
	}
	else {
		netadr_t adr;

		if (!NET_StringToAdr(Cmd_Argv(1), &adr)) {
			Com_Printf("Invalid address\n");
			return;
		}
		Com_Printf("List:\n");
		SB_Proxy_QueryForPingList(&adr, SB_ProxyDumpPing);
		Com_Printf("End of list.\n");
	}
}

void SB_Shutdown(void)
{
	int i;

	SB_PingTree_Shutdown();

	// FIXME - this probably never worked
	// Serverinfo_Stop();
	// Sys_MSleep(150);     // wait for thread to terminate

	for (i = 0; i < sourcesn; ++i) {
		Reset_Source(sources[i]);
		Q_free(sources[i]);
	}

	// delete servers[0...serversn) ?
}

void Browser_Init (void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SERVER_BROWSER);
	Cvar_Register(&sb_status);
	Cvar_Register(&sb_showping);
	Cvar_Register(&sb_showaddress);
	Cvar_Register(&sb_showmap);
	Cvar_Register(&sb_showgamedir);
	Cvar_Register(&sb_showplayers);
	Cvar_Register(&sb_showfraglimit);
	Cvar_Register(&sb_showtimelimit);
	Cvar_Register(&sb_pingtimeout);
	Cvar_Register(&sb_infotimeout);
	Cvar_Register(&sb_pings);
	Cvar_Register(&sb_pingspersec);
	Cvar_Register(&sb_inforetries);
	Cvar_Register(&sb_infospersec);
	Cvar_Register(&sb_proxinfopersec);
	Cvar_Register(&sb_proxretries);
	Cvar_Register(&sb_proxtimeout);
	Cvar_Register(&sb_liveupdate);
	Cvar_Register(&sb_mastertimeout);
	Cvar_Register(&sb_masterretries);
	Cvar_Register(&sb_nosockraw);
	Cvar_Register(&sb_sortservers);
	Cvar_Register(&sb_sortplayers);
	Cvar_Register(&sb_sortsources);
	Cvar_Register(&sb_autohide);
	Cvar_Register(&sb_hideempty);
	Cvar_Register(&sb_hidenotempty);
	Cvar_Register(&sb_hidefull);
	Cvar_Register(&sb_hidedead);
	Cvar_Register(&sb_hidehighping);
	Cvar_Register(&sb_pinglimit);
	Cvar_Register(&sb_showproxies);
	Cvar_Register(&sb_sourcevalidity);
	Cvar_Register(&sb_mastercache);
	Cvar_Register(&sb_autoupdate);
	Cvar_Register(&sb_listcache);
	Cvar_Register(&sb_findroutes);
	Cvar_Register(&sb_ignore_proxy);
	Cvar_Register(&sb_info_filter);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("addserver", AddServer_f);
	Cmd_AddCommand("sb_refresh", GetServerPingsAndInfos_f);
	Cmd_AddCommand("sb_pingsdump", SB_PingsDump_f);
	Cmd_AddCommand("sb_sourceadd", SB_Source_Add_f);
	Cmd_AddCommand("sb_sourcesupdate", SB_Sources_Update_f);
	Cmd_AddCommand("sb_buildpingtree", SB_PingTree_Build);
	Cmd_AddCommand("sb_proxygetpings", SB_ProxyGetPings_f);

	if (sb_listcache.integer) {
		SB_Serverlist_Unserialize_f();
		SB_Proxylist_Unserialize_f();
	}

	qtvlist_init();
}

void Browser_Init2 (void)
{
	int i;

	Servers_pos = 0;
	Sources_pos = 0;
	Servers_disp = 0;
	show_serverinfo = NULL;
	serverinfo_pos = 0;

	for (i=0; i < MAX_SERVERS; i++)
		servers[i] = NULL;

	serversn = serversn_passed = 0;
	sourcesn = 0;

	serverlist_mutex = SDL_CreateMutex();
	SB_PingTree_Init();

	// read sources from SOURCES_PATH
	Reload_Sources();
	MarkDefaultSources();
}

void SB_ExecuteQueuedTriggers(void) {
	if (sb_queuedtriggers & SB_TRIGGER_REFRESHDONE) {
		TP_ExecTrigger("f_sbrefreshdone");
		sb_queuedtriggers &= ~SB_TRIGGER_REFRESHDONE;
	}

	if (sb_queuedtriggers & SB_TRIGGER_SOURCESUPDATED) {
		TP_ExecTrigger("f_sbupdatesourcesdone");
		sb_queuedtriggers &= ~SB_TRIGGER_SOURCESUPDATED;
	}

	if (sb_queuedtriggers & SB_TRIGGER_NOTIFY_PINGTREE) {
		Com_Printf("Ping tree has been created\n");
		sb_queuedtriggers &= ~SB_TRIGGER_NOTIFY_PINGTREE;
	}
}

static qbool SB_InfoFilter_Exec(info_filter_t* info_filters, int info_filter_count, server_data* s)
{
	int j;
	qbool default_pass = true;

	for (j = 0; j < info_filter_count; ++j) {
		info_filter_t* filter = &info_filters[j];
		const char* value;

		if (!filter->exec) {
			// Invalid definition
			continue;
		}

		// if any filters were valid then block the remaining
		default_pass = !filter->pass;

		// shorthand for "rest"
		if (filter->name[0] == '*' && filter->name[1] == '\0') {
			return filter->pass;
		}

		value = ValueForKey(s, filter->name);
		if (!value) {
			// if server doesn't specify key then we move on to next (hmm)
			continue;
		}

		if (!filter->regex) {
			// Any value will do
			if (value != NULL && value[0]) {
				return filter->pass;
			}
		}
		else {
			// Rule specified, check it matches the regex
			pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(filter->regex, NULL);
			if (pcre2_match(filter->regex, (PCRE2_SPTR)value, strlen(value), 0, 0, match_data, NULL) >= 0) {
				pcre2_match_data_free (match_data);
				return filter->pass;
			}
			pcre2_match_data_free (match_data);
		}
	}

	// run out of rules - default 
	return default_pass;
}

static info_filter_t* SB_InfoFilter_Parse(int* count)
{
	int i;
	tokenizecontext_t info_filter_ctx;
	int info_filter_count;
	info_filter_t* info_filters;
	char definition[MAX_MACRO_STRING];

	Cmd_TokenizeStringEx(&info_filter_ctx, sb_info_filter.string);
	*count = info_filter_count = Cmd_ArgcEx(&info_filter_ctx);

	info_filters = Q_malloc(info_filter_count * sizeof(info_filter_t));
	for (i = 0; i < info_filter_count; ++i) {
		char* split;
		int error;
		PCRE2_SIZE error_offset;
		info_filter_t* filter = &info_filters[i];

		// filters must be +x=y or -x=y
		strlcpy(definition, Cmd_ArgvEx(&info_filter_ctx, i), sizeof(definition));
		if (definition[0] != '-' && definition[0] != '+') {
			Con_Printf("Invalid info-filter: bad syntax (%s)\n", definition);
			continue;
		}
		filter->pass = (definition[0] == '+');

		// Copy the serverinfo key name
		split = strchr(definition, '=');
		if (split != NULL) {
			split[0] = '\0';
		}
		strlcpy(filter->name, definition + 1, sizeof(filter->name));
		if (!filter->name[0]) {
			// empty key name is invalid...
			Con_Printf("Invalid info-filter: bad syntax (%s)\n", definition);
			continue;
		}

		if (split != NULL && split[1] && split[1] != '*') {
			// no regex is fine
			filter->regex = pcre2_compile((PCRE2_SPTR)(split + 1), PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error, &error_offset, NULL);
			if (filter->regex == NULL) {
				PCRE2_UCHAR error_str[256];
				pcre2_get_error_message(error, error_str, sizeof(error_str));
				Con_Printf("Invalid rule definition: %s\n", error_str);
				continue;
			}
		}

		filter->exec = true;
	}

	return info_filters;
}

static void SB_InfoFilter_Free(info_filter_t* info_filters, int info_filter_count)
{
	int i;

	for (i = 0; i < info_filter_count; ++i) {
		if (info_filters[i].regex) {
			pcre2_code_free(info_filters[i].regex);
		}
	}

	Q_free(info_filters);
}
