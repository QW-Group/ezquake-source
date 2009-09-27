/*
	$Id: EX_browser.c,v 1.57 2007-10-11 07:02:37 dkure Exp $
*/

#include "quakedef.h"
#include "fs.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#endif
#ifdef _WIN32
#include "winquake.h"
#else
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "keys.h"
#include "EX_browser.h"
#include "Ctrl_EditBox.h"
#include "settings.h"
#include "settings_page.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "menu.h"
#include "utils.h"
#include "menu_multiplayer.h"
#include "qsound.h"


int source_unique = 0;


// searching
#define MAX_SEARCH 20
#define SEARCH_TIME 3
double searchtime = -10;

enum {
	search_none = 0,
	search_server,
	search_player
} searchtype = search_none;

char searchstring[MAX_SEARCH+1];

// add source
CEditBox edit1, edit2;
int adding_source = 0;
int newsource_master; // 0 = file
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
cvar_t  sb_mastertimeout = {"sb_mastertimeout", "1000"};
cvar_t  sb_masterretries = {"sb_masterretries",    "3"};
cvar_t  sb_nosockraw     = {"sb_nosockraw",        "0"}; // when enabled, forces "new ping" (udp qw query packet, multithreaded) to be used

cvar_t  sb_liveupdate    = {"sb_liveupdate",       "2"}; // not in menu

cvar_t  sb_sortservers   = {"sb_sortservers",     "32"}; // not in new menu
cvar_t  sb_sortplayers   = {"sb_sortplayers",     "92"}; // not in new menu
cvar_t  sb_sortsources   = {"sb_sortsources",      "3"}; // not in new menu

cvar_t  sb_autohide      = {"sb_autohide",         "1"}; // not in menu


// filters
cvar_t  sb_hideempty     = {"sb_hideempty",        "1"};
cvar_t  sb_hidenotempty  = {"sb_hidenotempty",     "0"};
cvar_t  sb_hidefull      = {"sb_hidefull",         "0"};
cvar_t  sb_hidedead      = {"sb_hidedead",         "1"};
cvar_t  sb_hidehighping  = {"sb_hidehighping",     "0"};
cvar_t  sb_pinglimit     = {"sb_pinglimit",       "80"};
cvar_t  sb_showproxies   = {"sb_showproxies",      "0"};

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

extern cvar_t cl_proxyaddr;

sem_t serverlist_semaphore;
sem_t serverinfo_semaphore;

void Serverinfo_Stop(void);

static qbool SB_Is_Selected_Proxy(server_data *s)
{
	return (strcmp(s->display.ip, cl_proxyaddr.string) == 0);
}

static void SB_Select_QWfwd(server_data *s)
{
	if (SB_Is_Selected_Proxy(s)) {
		// unselect
		Cvar_Set(&cl_proxyaddr, "");
	}
	else {
		// select
		Cvar_Set(&cl_proxyaddr, show_serverinfo->display.ip);
	}
	S_LocalSound ("misc/menu2.wav");
	Serverinfo_Stop();
}

static void Join_Server (server_data *s)
{
	Cbuf_AddText ("join ");
	Cbuf_AddText (s->display.ip);
	Cbuf_AddText ("\n");

	if (sb_autohide.value)
	{
		if (sb_autohide.value > 1  ||  strcmp(s->display.gamedir, "qizmo")) {
			key_dest = key_game;
			m_state = m_none;
			M_Draw();
		}
	}
}

static void Observe_Server (server_data *s)
{
	Cbuf_AddText ("observe ");
	Cbuf_AddText (s->display.ip);
	Cbuf_AddText ("\n");

	if (sb_autohide.value)
	{
		if (sb_autohide.value > 1  ||  strcmp(s->display.gamedir, "qizmo")) {
			key_dest = key_game;
			m_state = m_none;
			M_Draw();
		}
	}
}

// case insensitive and red-insensitive compare
static int strcmp2 (const char * s1, const char * s2)
{
	if (s1 == NULL  &&  s2 == NULL)
		return 0;

	if (s1 == NULL)
		return -1;

	if (s2 == NULL)
		return 1;

	while (*s1  ||  *s2)
    {
		if (tolower(*s1 & 0x7f) != tolower(*s2 & 0x7f))
			return tolower(*s1 & 0x7f) - tolower(*s2 & 0x7f);
		s1++;
		s2++;
	}

	return 0;
}

static void CopyServerToClipboard (server_data *s)
{
	char buf[2048];

	if (isCtrlDown() || s->display.name[0] == 0)
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

	Cbuf_AddText (isCtrlDown() ? "say_team " :  "say ");
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

void Serverinfo_Draw ();
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
	UI_Print_Center(x + (*pos)*8, y, 8*(w+4), buf, false);
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

void SB_Servers_Draw (int x, int y, int w, int h)
{
	char line[1024];
	int i, pos, listsize;

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
		Sys_SemWait(&serverlist_semaphore);

        Servers_pos = max(Servers_pos, 0);
        Servers_pos = min(Servers_pos, serversn_passed-1);

        listsize = (int)(h/8) - (sb_status.value ? 3 : 0);
        pos = w/8;
        memset(line, ' ', pos);
        line[pos] = 0;

        listsize--;     // column titles

        if (sb_showtimelimit.value)
            Add_Column2(x, y, &pos, "tl", COL_TIMELIMIT, true);
        if (sb_showfraglimit.value)
            Add_Column2(x, y, &pos, "fl", COL_FRAGLIMIT, true);
        if (sb_showplayers.value)
            Add_Column2(x, y, &pos, "plyrs", COL_PLAYERS, true);
        if (sb_showmap.value)
            Add_Column2(x, y, &pos, "map", COL_MAP, true);
        if (sb_showgamedir.value)
            Add_Column2(x, y, &pos, "gamedir", COL_GAMEDIR, true);
        if (sb_showping.value)
            Add_Column2(x, y, &pos, "png", COL_PING, true);
        if (sb_showaddress.value)
            Add_Column2(x, y, &pos, "address", COL_IP, true);

        UI_Print(x, y, "name", true);

        if (Servers_pos > Servers_disp + listsize - 1)
            Servers_disp = Servers_pos - listsize + 1;
        if (Servers_disp > serversn_passed - listsize)
            Servers_disp = max(serversn_passed - listsize, 0);
        if (Servers_pos < Servers_disp)
            Servers_disp = Servers_pos;

		if (updating_sources) {
			Sys_SemPost(&serverlist_semaphore);
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
            if (sb_showping.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.ping, COL_PING, servnum==Servers_pos);
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
		Sys_SemPost(&serverlist_semaphore);
	} else if (!adding_server) {
		UI_Print_Center(x, y+8, w, "No servers filtered", false);
		UI_Print_Center(x, y+24, w, "Press [space] to refresh the list", true);
		UI_Print_Center(x, y+40, w, "Mark some sources on the next tab", false);
		UI_Print_Center(x, y+48, w, "or press [Insert] to add a server", false);
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

void Serverinfo_Draw ()
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
        memcpy (buf, "\x10ðìáùåòó\x11", 9); // FIXME: non-ascii chars
    if (serverinfo_pos == 1)
        memcpy (buf + 8, "\x10óåòöåòéîæï\x11", 12); // FIXME: non-ascii chars
    if (serverinfo_pos == 2)
        memcpy (buf + 19, "\x10óïõòãåó\x11", 9); // FIXME: non-ascii chars

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

        if (sources[serverinfo_sources_disp+i]->type == type_file)
            strlcpy (buf2, " file ", sizeof (buf2));
        else if (sources[serverinfo_sources_disp+i]->type == type_master)
            strlcpy(buf2, "master", sizeof (buf2));
        else if (sources[serverinfo_sources_disp+i]->type == type_dummy)
            strlcpy(buf2, "dummy ", sizeof (buf2));

        snprintf(buf, sizeof (buf), "%s   %s", buf2, sources[serverinfo_sources_disp+i]->name);
        buf[w/8] = 0;

        UI_Print(x, y+i*8+8, buf,
            IsInSource(sources[serverinfo_sources_disp+i], s));

        if (serverinfo_sources_pos == serverinfo_sources_disp+i)
            UI_DrawCharacter(x+8*7, y+i*8+8, '\x8D');
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
    UI_Print(x+54, y + 20, newsource_master ? "master" : "file", newsource_pos==0);
    if (newsource_pos == 0)
        Draw_Character (x+42, y+20, 13);

    UI_Print(x+4, y + 30, "name", newsource_pos==1);
    CEditBox_Draw(&edit1, x+54, y+30, newsource_pos==1);
    if (newsource_pos == 1)
        Draw_Character (x+42, y+30, 13);

    UI_Print(x+4, y + 40, newsource_master ? "addr" : "file", newsource_pos==2);
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

        if (s->type == type_master)
            strlcpy(type, "master", sizeof (type));
        else if (s->type == type_file)
            strlcpy(type, " file ", sizeof (type));
        else if (s->type == type_dummy)
            strlcpy(type, "dummy ", sizeof (type));
        else
			strlcpy(type, " ???? ", sizeof (type));

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

        snprintf(line, sizeof (line), "%-15.15s%c%-*.*s %3s",
                s->name, num==Players_pos ? 141 : ' ',
                hw, hw, s->serv->display.name, s->serv->display.ping);

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
	else {
		Com_Printf("Usage: %s <name> <address/filename> <master|file>\n", Cmd_Argv(0));
		Com_Printf("Last argument must be 'master' or 'file'\n");
		return;
	}

	SB_Source_Add(Cmd_Argv(1), Cmd_Argv(2), type);
}

void Add_Source_Key(int key, wchar unichar)
{
    switch (key)
    {
    case K_HOME:
        if (isCtrlDown())
            newsource_pos = 0; break;
    case K_END:
        if (isCtrlDown())
            newsource_pos = 4; break;
    case K_UPARROW:
	case K_MWHEELUP:
        newsource_pos--; break;
	case K_TAB:
		isShiftDown()?newsource_pos--:newsource_pos++; break;
    case K_DOWNARROW:
	case K_MWHEELDOWN:
        newsource_pos++; break;
    case K_ENTER:
    case '+':
    case '=':
    case '-':
        switch (newsource_pos)
        {
        case 0:
            newsource_master = !newsource_master; break;
		case 2:
        case 3:
            if (key == K_ENTER)
            {
				int newpos;
				
				newpos = SB_Source_Add(edit1.text, edit2.text, newsource_master ? type_master : type_file);
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

    if ((!isCtrlDown() || tolower(key)=='v') && !isAltDown())
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
        if (isCtrlDown())
            newserver_pos = 0; break;
    case K_END:
        if (isCtrlDown())
            newserver_pos = 4; break;
    case K_UPARROW:
	case K_MWHEELUP:
        newserver_pos--; break;
	case K_TAB:
		isShiftDown()?newserver_pos--:newserver_pos++; break;
    case K_DOWNARROW:
	case K_MWHEELDOWN:
        newserver_pos++; break;
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

    if ((!isCtrlDown() || tolower(key)=='v') && !isAltDown())
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

int SB_Servers_Key(int key)
{
    if (serversn_passed <= 0  &&  (key != K_SPACE || isAltDown())
        && tolower(key) != 'n'  && key != K_INS)
        return false;

	if (key == K_SPACE) {
		GetServerPingsAndInfos(isCtrlDown());
		return true;
	}

	if (!isCtrlDown() && !isAltDown() && key > ' ' && key <= '}')  // search
    {
        int len;
        char c = tolower(key);
        if (searchtype != search_server)
        {
            searchtype = search_server;
            searchstring[0] = 0;
        }
        searchtime = cls.realtime;

        len = strlen(searchstring);
        if (len < MAX_SEARCH)
        {
            searchstring[len] = c;
            searchstring[len+1] = 0;

            if (!SearchNextServer(Servers_pos))
                if (!SearchNextServer(0))
					// FIXME: non-ascii chars
					strlcpy (searchstring, "îïô æïõîä", sizeof (searchstring));  // not found
        }
		return true;
    }
    else
    {
        searchtype = search_none;
        switch (key)
        {
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
			case K_MWHEELUP:
                Servers_pos--; break;
            case K_DOWNARROW:
			case K_MWHEELDOWN:
                Servers_pos++; break;
            case K_HOME:
                Servers_pos = 0; break;
            case K_END:
                Servers_pos = serversn_passed-1; break;
            case K_PGUP:
                Servers_pos -= 10; break;
            case K_PGDN:
                Servers_pos += 10; break;
			case K_MOUSE1:
            case K_ENTER:
                Serverinfo_Start(servers[Servers_pos]); break;
            case K_SPACE:
                GetServerPingsAndInfos(isCtrlDown());
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':   // sorting mode
				if (isAltDown()) // fixme
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
							strlcpy (buf + 1, sb_sortservers.string, sizeof (buf));
						}
                    }
					else
					{
						buf[0] = key;
						strlcpy (buf + 1, sb_sortservers.string, sizeof (buf));
                    }

					Cvar_Set(&sb_sortservers, buf);
					resort_servers = 1;
				}
				else if (isCtrlDown())
				{
					switch (key)
					{
					case '2': Cvar_Toggle(&sb_showaddress);    break;
					case '3': Cvar_Toggle(&sb_showping);       break;
					case '4': Cvar_Toggle(&sb_showgamedir);    break;
					case '5': Cvar_Toggle(&sb_showmap);        break;
					case '6': Cvar_Toggle(&sb_showplayers);    break;
					case '7': Cvar_Toggle(&sb_showfraglimit);  break;
					case '8': Cvar_Toggle(&sb_showtimelimit);  break;
					}
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
        case K_ENTER:
			if (serverinfo_pos != 2)
            {
				if (show_serverinfo->qwfwd) {
					SB_Select_QWfwd(show_serverinfo);
				} else if (isCtrlDown()) {
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
            Serverinfo_Stop(); break;
        case K_PGUP:
			if (CTab_GetCurrentId(&sb_tab) == SBPG_PLAYERS)
            {
                if (isCtrlDown())
                    Players_pos = 0;
                else
                    Players_pos--;
                Players_pos = max(0, Players_pos);
                Serverinfo_Change(all_players[Players_pos]->serv);
            }
            else
            {
                if (isCtrlDown())
                    Servers_pos = 0;
                else
                    Servers_pos--;
                Servers_pos = max(0, Servers_pos);
                Serverinfo_Change(servers[Servers_pos]);
            }
            break;
        case K_PGDN:
            if (CTab_GetCurrentId(&sb_tab) == SBPG_PLAYERS)
            {
                if (isCtrlDown())
                    Players_pos = all_players_n - 1;
                else
                    Players_pos++;
                Players_pos = min(all_players_n-1, Players_pos);
                Serverinfo_Change(all_players[Players_pos]->serv);
            }
            else
            {
                if (isCtrlDown())
                    Servers_pos = serversn_passed-1;
                else
                    Servers_pos++;
                Servers_pos = min(serversn_passed-1, Servers_pos);
                Serverinfo_Change(servers[Servers_pos]);
            }
            break;
        case K_LEFTARROW:
            serverinfo_pos--; break;
        case K_RIGHTARROW:
            serverinfo_pos++; break;
        case 'j':
        case 'p':
            Join_Server(show_serverinfo);
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
             }
             else
                testing_connection = 0;
             break;
        }
        case 'c':   // copy server to clipboard
             CopyServerToClipboard(show_serverinfo);
             break;
        case 'v':   // past server into console
             PasteServerToConsole(show_serverinfo);
             break;
        default:
            switch (serverinfo_pos)
            {
                case 0: // players list
                    Serverinfo_Players_Key(key); break;
                case 1: // serverinfo / rules
                    Serverinfo_Rules_Key(key); break;
                case 2: // sources
                    Serverinfo_Sources_Key(key); break;
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
            serverinfo_players_pos--; break;
        case K_DOWNARROW:
		case K_MWHEELDOWN:
            serverinfo_players_pos++; break;
        case K_HOME:
            serverinfo_players_pos = 0; break;
        case K_END:
            serverinfo_players_pos = 999; break;
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
            serverinfo_rules_pos--; break;
        case K_DOWNARROW:
		case K_MWHEELDOWN:
            serverinfo_rules_pos++; break;
        case K_HOME:
            serverinfo_rules_pos = 0; break;
        case K_END:
            serverinfo_rules_pos = 999; break;
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
            serverinfo_sources_pos--; break;
        case K_DOWNARROW:
		case K_MWHEELDOWN:
            serverinfo_sources_pos++; break;
        case K_HOME:
            serverinfo_sources_pos = 0; break;
        case K_END:
            serverinfo_sources_pos = 999; break;
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
            newsource_master = 1;
            CEditBox_Init(&edit1, 16, 25);
            CEditBox_Init(&edit2, 16, 100);
            adding_source = 1;
            break;
        case K_DEL:
        case 'd':       // remove source
            if (sources[Sources_pos]->type != type_dummy)
                SB_Confirmation(va("Remove %-.20s", sources[Sources_pos]->name), SB_RemoveSourceProc);
            break;
        case 'u':
            Update_Source(sources[Sources_pos]); break;
        case K_UPARROW:
		case K_MWHEELUP:
            Sources_pos--; break;
        case K_DOWNARROW:
		case K_MWHEELDOWN:
            Sources_pos++; break;
        case K_HOME:
            Sources_pos = 0; break;
        case K_END:
            Sources_pos = sourcesn-1; break;
        case K_PGUP:
            Sources_pos -= 10; break;
        case K_PGDN:
            Sources_pos += 10; break;
		case K_MOUSE1:
        case K_ENTER:
            Toggle_Source(sources[Sources_pos++]); break;
        case ']':
            Toggle_Source(sources[Sources_pos]); break;
        case K_SPACE:
			SB_Sources_Update_Begin(isCtrlDown());
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
                    strlcpy(buf+1, sb_sortsources.string, sizeof (buf));
                }
                Cvar_Set(&sb_sortsources, buf);
            }
            else
            {
                char buf[32];
				buf[0] = key;
                strlcpy(buf+1, sb_sortsources.string, sizeof (buf));
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

    if ((isAltDown()  ||  searchtype == search_player)  &&
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
                GetServerPingsAndInfos(isCtrlDown());
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
                Players_pos--; break;
            case K_DOWNARROW:
			case K_MWHEELDOWN:
                Players_pos++; break;
            case K_HOME:
                Players_pos = 0; break;
            case K_END:
                Players_pos = all_players_n-1; break;
            case K_PGUP:
                Players_pos -= 10; break;
            case K_PGDN:
                Players_pos += 10; break;
			case K_MOUSE1:
            case K_ENTER:
                Serverinfo_Start(all_players[Players_pos]->serv); break;
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
                        strlcpy(buf+1, sb_sortplayers.string, sizeof (buf));
                    }
                    Cvar_Set(&sb_sortplayers, buf);
                }
                else
                {
                    char buf[32];
					buf[0] = key;
                    strlcpy(buf+1, sb_sortplayers.string, sizeof (buf));
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
		return true;
    }
    if (ms->button_up == 1)
    {
        SB_Servers_Key(K_MOUSE1);
        return true;
    }
	Servers_pos = Servers_disp + ms->y / 8 - 1;
    Servers_pos = bound(0, Servers_pos, serversn - 1);
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
// sorting routine
//

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
                d = funcmp(s1->display.name, s2->display.name); break;
            case '2':
                d = memcmp(&(s1->address.ip), &(s2->address.ip), 4);
                if (!d)
                    d = ntohs(s1->address.port) - ntohs(s2->address.port);
                break;
            case '3':
                d = s1->ping - s2->ping; break;
            case '4':
                d = strcmp2(s1->display.gamedir, s2->display.gamedir); break;
            case '5':
                d = strcmp2(s1->display.map, s2->display.map); break;
            case '6':
                d = s1->playersn - s2->playersn; break;
            case '7':
                d = strcmp2(s1->display.fraglimit, s2->display.fraglimit); break;
            case '8':
                d = strcmp2(s1->display.timelimit, s2->display.timelimit); break;
            default:
                d = s1 - s2;
        }

        if (d)
            return reverse ? -d : d;
    }
}

void Filter_Servers(void)
{
    int i;
    serversn_passed = 0;
    for (i=0; i < serversn; i++)
    {
        char *tmp;
        server_data *s = servers[i];
        s->passed_filters = 0;

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

		s->passed_filters = 1;  // passed
        serversn_passed++;
    }
    return;
}

void Sort_Servers (void)
{
	Sys_SemWait(&serverlist_semaphore);
    Filter_Servers();
    qsort(servers, serversn, sizeof(servers[0]), Servers_Compare_Func);
	Sys_SemPost(&serverlist_semaphore);
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
                d = funcmp(s1->display.name, s2->display.name); break;
            case '2':
                d = memcmp(&(s1->address.ip), &(s2->address.ip), 4);
                if (!d)
                    d = ntohs(s1->address.port) - ntohs(s2->address.port);
                break;
            case '3':
                d = s1->ping - s2->ping; break;
            case '4':
                d = strcmp2(s1->display.gamedir, s2->display.gamedir); break;
            case '5':
                d = strcmp2(s1->display.map, s2->display.map); break;
            case '6':
                d = s1->playersn - s2->playersn; break;
            case '7':
                d = strcmp2(s1->display.fraglimit, s2->display.fraglimit); break;
            case '8':
                d = strcmp2(s1->display.timelimit, s2->display.timelimit); break;
            case '9':
                d = funcmp(p1->name, p2->name); break;
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
#define SERIALIZE_FILE_VERSION 1002
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

	fread(&version, sizeof(int), 1, f);
	if (version != SERIALIZE_FILE_VERSION) {
		return -1;
	}

	fread(&server_data_size, sizeof(size_t), 1, f);
	if (server_data_size != sizeof(server_data)) {
		return -1;
	}

	fread(&serversn_buffer, sizeof(int), 1, f);
	if (serversn_buffer > MAX_SERVERS) {
		return -2;
	}

	fread(&serversn_passed_buffer, sizeof(int), 1, f);
	if (serversn_passed_buffer > MAX_SERVERS) {
		return -2;
	}

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
	char *filename = va("%s/%s", com_homedir, "servers_data");

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
	char *filename = va("%s/%s", com_homedir, "servers_data");
	int err;

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

void Shutdown_SB(void)
{
    Serverinfo_Stop();
    Sys_MSleep(150);     // wait for thread to terminate
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
	Cvar_ResetCurrentGroup();

    Cmd_AddCommand("addserver", AddServer_f);
	Cmd_AddCommand("sb_refresh", GetServerPingsAndInfos_f);
	Cmd_AddCommand("sb_pingsdump", SB_PingsDump_f);
	Cmd_AddCommand("sb_sourceadd", SB_Source_Add_f);
	Cmd_AddCommand("sb_sourcesupdate", SB_Sources_Update_f);

	if (sb_listcache.integer) {
		SB_Serverlist_Unserialize_f();
	}
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

	Sys_SemInit(&serverlist_semaphore, 1, 1);

    // read sources from SOURCES_PATH
	Reload_Sources();
	MarkDefaultSources();
}
