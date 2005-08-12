/*
	$Id: EX_browser.c,v 1.7 2005-08-12 15:57:21 vvd0 Exp $
*/

#include "quakedef.h"

#ifdef _WIN32
#include "winquake.h"
#else
#include <netinet/in.h>
#endif

#include "EX_browser.h"
#include "Ctrl_EditBox.h"
#include "Ctrl.h"
#include "cvar.h"
#include "menu.h"
#include "EX_misc.h"
#include "EX_FunNames.h"
#include "keys.h"

int source_unique = 0;


extern cvar_t cl_useproxy;
extern int connected2proxy;
extern void M_Menu_ServerList_f (void);
extern void M_Menu_Main_f (void);
extern void M_ToggleMenu_f (void);
extern void M_Draw (void);

// searching
#define MAX_SEARCH 20
#define SEARCH_TIME 3
double searchtime = -10;
enum {search_none = 0, search_server, search_player} searchtype = search_none;
char searchstring[MAX_SEARCH+1];


// add source
CEditBox edit1, edit2;
int adding_source = 0;
int newsource_master;   // 0 = file
int newsource_pos;

// add server
int adding_server = 0;
int newserver_pos;


cvar_t  sb_status        =  {"sb_status", "1"};

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
cvar_t  sb_pingspersec   = {"sb_pingspersec",    "150"};
cvar_t  sb_pings         = {"sb_pings",            "3"};
cvar_t  sb_inforetries   = {"sb_inforetries",      "3"};
cvar_t  sb_infospersec   = {"sb_infospersec",    "100"};
cvar_t  sb_mastertimeout = {"sb_mastertimeout", "1000"};
cvar_t  sb_masterretries = {"sb_masterretries",    "3"};

cvar_t  sb_liveupdate    = {"sb_liveupdate",       "2"};

cvar_t  sb_sortservers   = {"sb_sortservers",     "32"};
cvar_t  sb_sortplayers   = {"sb_sortplayers",     "92"};
cvar_t  sb_sortsources   = {"sb_sortsources",      "3"};

cvar_t  sb_maxwidth      = {"sb_maxwidth",       "512"};
cvar_t  sb_maxheight     = {"sb_maxheight",      "480"};
cvar_t  sb_autohide      = {"sb_autohide",         "1"};

// filters
cvar_t  sb_hideempty     = {"sb_hideempty",        "1"};
cvar_t  sb_hidenotempty  = {"sb_hidenotempty",     "0"};
cvar_t  sb_hidefull      = {"sb_hidefull",         "0"};
cvar_t  sb_hidedead      = {"sb_hidedead",         "1"};

cvar_t  sb_sourcevalidity  = {"sb_sourcevalidity", "30"};
cvar_t  sb_showcounters    = {"sb_showcounters",    "1"};
cvar_t  sb_mastercache     = {"sb_mastercache",     "1"};
cvar_t  sb_starttab        = {"sb_starttab",     "2"};
cvar_t  sb_autoupdate      = {"sb_autoupdate",     "0"};

// servers table
server_data *servers[MAX_SERVERS];
int serversn;
int serversn_passed;

// all players table
player_host ** all_players = NULL;
int all_players_n = 0;

int resort_servers = 1;
int resort_sources = 1;
void Sort_Servers (void);

int testing_connection = 0;
int ping_phase = 0;
double ping_pos;
int abort_ping;

void cvar_toggle (cvar_t *var)
{
    if (var->value == 0)
        Cvar_SetValue(var, 1);
    else
        Cvar_SetValue(var, 0);
}

void    Cvar2_Set (cvar_t *var, float value)
{
    char buf[128];

    sprintf(buf, "%f", value);
    Cvar_Set(var, buf);
}

void Join_Server(server_data *s)
{
    Cbuf_AddText("join ");
    Cbuf_AddText(s->display.ip);
    Cbuf_AddText("\n");

    if (sb_autohide.value)
    {
		if (sb_autohide.value > 1  ||  strcmp(s->display.gamedir, "qizmo")) {
            key_dest = key_game;
			m_state = m_none;
			M_Draw();
		}
    }
}

void Observe_Server(server_data *s)
{
    Cbuf_AddText("observe ");
    Cbuf_AddText(s->display.ip);
    Cbuf_AddText("\n");

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
int strcmp2(const char * s1, const char * s2)
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

int isThisAltDown(void)
{
	return keydown[K_ALT] || keydown[K_RALT];
}

int isThisCtrlDown(void)
{
	return keydown[K_CTRL] || keydown[K_RCTRL];
}

void CopyServerToClipboard(server_data *s)
{
    char buf[2048];
    if (isThisCtrlDown()  ||  s->display.name[0] == 0)
        strcpy(buf, s->display.ip);
    else
        sprintf(buf, "%s (%s)",
            s->display.name,
            s->display.ip);
    CopyToClipboard(buf);
}

void PasteServerToConsole(server_data *s)
{
    char buf[2048];
    sprintf(buf, "%s (%s)",
            s->display.name,
            s->display.ip);
    if (isThisCtrlDown())
        Cbuf_AddText("say_team ");
    else
        Cbuf_AddText("say ");
    Cbuf_AddText(buf);
    Cbuf_AddText("\n");
}


//
// browser routines
//


server_data * Create_Server(char *ip)
{
    server_data *s;

    s = (server_data *) Q_malloc (sizeof(server_data));
    s->display.name[0] = 0;
    s->display.ping[0] = 0;
    s->ping = -1;
    s->display.map[0] = 0;
    s->display.gamedir[0] = 0;
    s->display.players[0] = 0;
    s->display.fraglimit[0] = 0;
    s->display.timelimit[0] = 0;
    if (!NET_StringToAdr (ip, &(s->address)))
    {
        free(s);
        return NULL;
    }
    if (!strchr(ip, ':'))
        s->address.port = htons(27500);
    sprintf(s->display.ip, "%d.%d.%d.%d:%d",
        s->address.ip[0], s->address.ip[1], s->address.ip[2], s->address.ip[3],
        ntohs(s->address.port));

    s->keysn = 0;
    s->playersn = 0;
    s->spectatorsn = 0;
    return s;
}

server_data * Create_Server2(netadr_t n)
{
    server_data *s;

    s = (server_data *) Q_malloc (sizeof(server_data));

    memcpy(&(s->address), &n, sizeof(n));
    strcpy(s->display.ip, NET_AdrToString(n));

    s->display.name[0] = 0;
    s->display.ping[0] = 0;
    s->ping = -1;
    s->display.map[0] = 0;
    s->display.gamedir[0] = 0;
    s->display.players[0] = 0;
    s->display.fraglimit[0] = 0;
    s->display.timelimit[0] = 0;
    s->keysn = 0;
    s->playersn = 0;
    s->spectatorsn = 0;
    return s;
}

void Reset_Server(server_data *s)
{
    int i;
    for (i=0; i < s->keysn; i++)
    {
        free(s->keys[i]);
        free(s->values[i]);
    }
    s->keysn = 0;
    for (i=0; i < s->playersn + s->spectatorsn; i++)
        free(s->players[i]);
    s->playersn = 0;
    s->spectatorsn = 0;

    rebuild_all_players = 1;    // rebuild all-players list
}

void Delete_Server(server_data *s)
{
    Reset_Server(s);
    free(s);
}


char confirm_text[64];
qboolean confirmation;
void (*confirm_func)(void);

void SB_Confirmation(const char *text, void (*func)(void))
{
    strcpy(confirm_text, text);
    confirm_func = func;
    confirmation = 1;
}

void SB_Confirmation_Draw()
{
    int x, y, w, h;
    w = 32 + 8*max(strlen(confirm_text), strlen("Are you sure? <y/n>"));
    h = 24;

    x = (vid.width - w)/2;
    y = (vid.height - h)/2;
    x = (x/8) * 8;
    y = (y/8) * 8;

    Draw_TextBox (x-16, y-16, w/8+1, h/8+2);

    UI_Print_Center(x, y, w, confirm_text, false);
    UI_Print_Center(x, y+16, w, "Are you sure? <y/n>", true);
}

void SB_Confirmation_Key(int key)
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

enum {pos_servers, pos_sources, pos_players, pos_options, pos_none} Browser_pos;
    int Servers_pos;
    int Sources_pos;
    int Players_pos;
    int Options_pos;

server_data * show_serverinfo;
int serverinfo_pos;

int Servers_disp;   // server# at the top of the list
int Sources_disp;   // source# at the top of the list
int Players_disp;   // player# at the top of the list

void Browser_Draw (int x, int y, int w, int h);
    void Servers_Draw (int x, int y, int w, int h);
    void Sources_Draw (int x, int y, int w, int h);
    void Players_Draw (int x, int y, int w, int h);
    void Options_Draw (int x, int y, int w, int h);
    void Serverinfo_Draw ();
        void Serverinfo_Players_Draw(int x, int y, int w, int h);
        void Serverinfo_Rules_Draw(int x, int y, int w, int h);
        void Serverinfo_Sources_Draw(int x, int y, int w, int h);

void Browser_Key(int key);
    void Servers_Key(int key);
    void Sources_Key(int key);
    void Players_Key(int key);
    void Options_Key(int key);
    void Serverinfo_Key (int key);
        void Serverinfo_Players_Key(int key);
        void Serverinfo_Rules_Key(int key);
        void Serverinfo_Sources_Key(int key);

//
// serverinfo
//

int sourcesn_updated = 0;

int Sources_Compare_Func(const void * p_s1, const void * p_s2)
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
    qsort(sources+1, sourcesn-1, sizeof(sources[0]), Sources_Compare_Func);
    sourcesn_updated = 1;
    for (i=1; i < sourcesn; i++)
        if (sources[i]->last_update.wYear)
            sourcesn_updated ++;
}


int serverinfo_players_pos;
int serverinfo_sources_pos;
int serverinfo_sources_disp;
void Serverinfo_Stop(void)
{
    extern int autoupdate_serverinfo;       // declared in EX_browser_net.c
    show_serverinfo = NULL;
    autoupdate_serverinfo = 0;
}

void Serverinfo_Start(server_data *s)
{
    extern int autoupdate_serverinfo;       // declared in EX_browser_net.c

    if (show_serverinfo)
        Serverinfo_Stop();

    serverinfo_players_pos = 0;
    serverinfo_sources_pos = 0;

    autoupdate_serverinfo = 1;
    show_serverinfo = s;

    // sort for eliminating ot-updated
    Sort_Sources();
    resort_sources = 1; // and mark for resort on next sources draw

    Start_Autoupdate(s);

    // testing connection
    if (testing_connection)
    {
        char buf[256];
        sprintf(buf, "%d.%d.%d.%d",
            show_serverinfo->address.ip[0],
            show_serverinfo->address.ip[1],
            show_serverinfo->address.ip[2],
            show_serverinfo->address.ip[3]);
        SB_Test_Init(buf);
        testing_connection = 1;
    }
}

void Serverinfo_Change(server_data *s)
{
    Alter_Autoupdate(s);
    show_serverinfo = s;

    // testing connection
    if (testing_connection)
    {
        char buf[256];
        sprintf(buf, "%d.%d.%d.%d",
            show_serverinfo->address.ip[0],
            show_serverinfo->address.ip[1],
            show_serverinfo->address.ip[2],
            show_serverinfo->address.ip[3]);
        SB_Test_Change(buf);
        testing_connection = 1;
    }
}

// --

qboolean AddUnboundServer(char *addr)
{
    int i;
    server_data *s;
    qboolean existed = false;

    if (sources[0]->serversn >= MAX_UNBOUND)
        return false;
    s = Create_Server(addr);
    if (s == NULL)
        return false;

    for (i=0; i < sources[0]->serversn; i++)
        if (!memcmp(&s->address, &sources[0]->servers[i]->address, 6))
        {
            free(s);
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
    m_state = m_slist;
    Mark_Source(sources[0]);
    Browser_pos = pos_servers;
    GetServerPing(s);
    GetServerInfo(s);
    Serverinfo_Start(s);
	M_Menu_ServerList_f();
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

//
// drawing routines
//

void Browser_Draw (int x, int y, int w, int h)
{
    char buf[500];

	if (Browser_pos == pos_none) switch((int) sb_starttab.value) {
		case 1: 
			Browser_pos = pos_servers; 
			if(sb_autoupdate.value) GetServerPingsAndInfos();
			break;
		case 2: Browser_pos = pos_sources; break;
		case 3: Browser_pos = pos_players; break;
		case 4: Browser_pos = pos_options; break;
		default: Browser_pos = pos_servers; break;
	}

    strcpy(buf, " servers sources players options ");
    if (Browser_pos == pos_servers)
        strncpy(buf, "\x10óåòöåòó\x11", 9);
    if (Browser_pos == pos_sources)
        strncpy(buf+8, "\x10óïõòãåó\x11", 9);
    if (Browser_pos == pos_players)
        strncpy(buf+16, "\x10ðìáùåòó\x11", 9);
    if (Browser_pos == pos_options)
        strncpy(buf+24, "\x10ïðôéïîó\x11", 9);

    UI_Print_Center(x, y, w, buf, false);

    memset(buf, '\x1E', w/8);
    buf[w/8] = 0;
    buf[w/8-1] = '\x1F';
    buf[0] = '\x1D';
    UI_Print(x, y+8, buf, false);

    switch (Browser_pos)
    {
    case pos_servers: Servers_Draw (x, y+16, w, h-16); break;
    case pos_sources: Sources_Draw (x, y+16, w, h-16); break;
    case pos_players: Players_Draw (x, y+16, w, h-16); break;
    case pos_options: Options_Draw (x, y+16, w, h-16); break;
    default:    ; // hmm
    }

    if (show_serverinfo)
        Serverinfo_Draw();

    if (ping_phase)
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

    if (updating_sources)
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

    if (confirmation)
        SB_Confirmation_Draw();
}


void Add_Column(char *line, int *pos, char *t, int w)
{
    if ((*pos) - w - 1  <=  1)
        return;
    (*pos) -= w;
    memcpy(line+(*pos), t, min(w, strlen(t)));
    (*pos)--;
    line[*pos] = ' ';
}

void Add_Column2(int x, int y, int *pos, char *t, int w, int red)
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
    char buf[1000], line[1000];

    memset(line, '\x1E', w/8);
    line[w/8] = 0;
    line[w/8-1] = '\x1F';
    line[0] = '\x1D';
    if (total > 0  &&  sb_showcounters.value)
    {
        sprintf(buf, "%d/%d", count+1, total);
        memset(line+w/8-3-strlen(buf), ' ', strlen(buf)+1);
    }
    UI_Print(x, y+h-24, line, false);
    if (total > 0  &&  sb_showcounters.value)
        UI_Print(x+w-8*(3+strlen(buf))+4, y+h-24, buf, true);

    // line 1
    strcpy(line, s->display.name[1] ? s->display.name : s->display.ip);
    line[w/8] = 0;
    UI_Print_Center(x, y+h-16, w, line, false);

    // line 2
    if (searchtype)
    {
        int i;
        sprintf(line, "search for: %-7s", searchstring);
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
            strcpy(buf, ValueForKey(s, "*gamedir"));
            buf[8] = 0;
            strcat(line, buf);
            strcat(line, "\xa0 ");
        }

        if (d_map)
        {
            memset(buf, 0, 10);
            strcpy(buf, ValueForKey(s, "map"));
            buf[8] = 0;
            strcat(line, buf);
            strcat(line, "\xa0 ");
        }

        //if (d_players)
        {
            char buf[10], *max;
            max =  ValueForKey(s, "maxclients");
            sprintf(buf, "%d/%s", s->playersn, max==NULL ? "??" : max);
            strcat(line, buf);
            max =  ValueForKey(s, "maxspectators");
            sprintf(buf, "-%d/%s", s->spectatorsn, max==NULL ? "??" : max);
            strcat(line, buf);
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
                sprintf(buf, "\xa0 dmm%s", dm);
                strcat(line, buf);
            }

            if (fl  &&  strlen(line) + 8 <= w/8)
            {
                sprintf(buf, "\xa0 fl:%s", fl);
                strcat(line, buf);
            }

            if (tl  &&  strlen(line) + 7 <= w/8)
            {
                sprintf(buf, "\xa0 tl:%s", tl);
                strcat(line, buf);
            }
        }
        else
        {
            char buf[200];
            sprintf(buf, "\xa0 %s", ValueForKey(s, "status"));
            strcat(line, buf);
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

void Servers_Draw (int x, int y, int w, int h)
{
    char line[1000];
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
        Servers_pos = max(Servers_pos, 0);
        Servers_pos = min(Servers_pos, serversn_passed-1);

        listsize = (int)(h/8) - (sb_status.value ? 3 : 0);
        pos = w/8;
        memset(line, ' ', pos);
        line[pos] = 0;

        listsize--;     // column titles

/*
        if (sb_showtimelimit.value)
            Add_Column(line, &pos, "tl", COL_TIMELIMIT);
        if (sb_showfraglimit.value)
            Add_Column(line, &pos, " fl", COL_FRAGLIMIT);
        if (sb_showplayers.value)
            Add_Column(line, &pos, "plyrs", COL_PLAYERS);
        if (sb_showmap.value)
            Add_Column(line, &pos, "map", COL_MAP);
        if (sb_showgamedir.value)
            Add_Column(line, &pos, "gamedir", COL_GAMEDIR);
        if (sb_showping.value)
            Add_Column(line, &pos, "png", COL_PING);
        if (sb_showaddress.value)
            Add_Column(line, &pos, "address", COL_IP);
*/
//void Add_Column2(int x, int y, int *pos, char *t, int w, int red)
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
//        memcpy(line, "name", min(pos, 4));
//        UI_Print_Center(x, y, w, line, true);

        if (Servers_pos > Servers_disp + listsize - 1)
            Servers_disp = Servers_pos - listsize + 1;
        if (Servers_disp > serversn_passed - listsize)
            Servers_disp = max(serversn_passed - listsize, 0);
        if (Servers_pos < Servers_disp)
            Servers_disp = Servers_pos;

        for (i = 0; i < listsize; i++)
        {
            char *name;
            int servnum = Servers_disp + i;
            if (servnum >= serversn_passed)
                break;

            // Display server
            pos = w/8;
            memset(line, ' ', 1000);
/*
            if (sb_showtimelimit.value)
                Add_Column(line, &pos, servers[servnum]->display.timelimit, COL_TIMELIMIT);
            if (sb_showfraglimit.value)
                Add_Column(line, &pos, servers[servnum]->display.fraglimit, COL_FRAGLIMIT);
            if (sb_showplayers.value)
                Add_Column(line, &pos, servers[servnum]->display.players, COL_PLAYERS);
            if (sb_showmap.value)
                Add_Column(line, &pos, servers[servnum]->display.map, COL_MAP);
            if (sb_showgamedir.value)
                Add_Column(line, &pos, servers[servnum]->display.gamedir, COL_GAMEDIR);
            if (sb_showping.value)
                Add_Column(line, &pos, servers[servnum]->display.ping, COL_PING);
            if (sb_showaddress.value)
                Add_Column(line, &pos, servers[servnum]->display.ip, COL_IP);
*/
            if (sb_showtimelimit.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.timelimit, COL_TIMELIMIT, servnum==Servers_pos);
            if (sb_showfraglimit.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.fraglimit, COL_FRAGLIMIT, servnum==Servers_pos);
            if (sb_showplayers.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.players, COL_PLAYERS, servnum==Servers_pos);
            if (sb_showmap.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.map, COL_MAP, servnum==Servers_pos);
            if (sb_showgamedir.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.gamedir, COL_GAMEDIR, servnum==Servers_pos);
            if (sb_showping.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.ping, COL_PING, servnum==Servers_pos);
            if (sb_showaddress.value)
                Add_Column2(x, y+8*(i+1), &pos, servers[servnum]->display.ip, COL_IP, servnum==Servers_pos);

            name = (servers[servnum]->display.name[0]) ?
                    servers[servnum]->display.name :
                    servers[servnum]->display.ip;
/*
            memcpy(line, name, min(pos, strlen(name)));

            if (servnum==Servers_pos)
                line[pos] = 141;

            line[w/8] = 0;
            UI_Print_Center(x, y+8*(i+1), w, line, servnum==Servers_pos);
*/
            if (servnum==Servers_pos)
                UI_Print(x + 8*pos, y+8*(i+1), "\x8d", false);
            strncpy(line, name, pos);
            line[pos] = 0;
            UI_Print(x, y+8*(i+1), line, servnum==Servers_pos);
        }

        //
        // status line
        //
        if (sb_status.value  &&  serversn_passed > 0)
            Draw_Server_Statusbar(x, y, w, h, servers[Servers_pos], Servers_pos, serversn_passed);
    }

    // adding server
    if (adding_server)
        Add_Server_Draw();
}

void Options_Draw(int x, int y, int w, int h)
{
    char buf[200];

    // filters
    UI_Print_Center(x+8, y +  4, w/2, "Filters:", true);

    UI_Print_Center(x+8, y + 16, w/2, "hide empty", (int)sb_hideempty.value);
    UI_Print_Center(x+8, y + 24, w/2, "hide not empty", (int)sb_hidenotempty.value);
    UI_Print_Center(x+8, y + 32, w/2, "hide full", (int)sb_hidefull.value);
    UI_Print_Center(x+8, y + 40, w/2, "hide dead", (int)sb_hidedead.value);

    if (Options_pos < 4)
        UI_DrawCharacter(x+8 + w/4 - (int)((8.5)*8), y + 16 + Options_pos*8, 141);

    // knyft
    UI_Print_Center(x+8, y+56, w/2, "show counters", (int)sb_showcounters.value);
    if (Options_pos == 4)
        UI_DrawCharacter(x+8 + w/4 - (int)((8.5)*8), y + 56, 141);


    // coluns
    UI_Print_Center(x+w/2, y +  4, w/2, "Columns:", true);

    UI_Print_Center(x+w/2, y + 16, w/2, "address", (int)sb_showaddress.value);
    UI_Print_Center(x+w/2, y + 24, w/2, "ping", (int)sb_showping.value);
    UI_Print_Center(x+w/2, y + 32, w/2, "gamedir", (int)sb_showgamedir.value);
    UI_Print_Center(x+w/2, y + 40, w/2, "map", (int)sb_showmap.value);
    UI_Print_Center(x+w/2, y + 48, w/2, "players", (int)sb_showplayers.value);
    UI_Print_Center(x+w/2, y + 56, w/2, "fraglimit", (int)sb_showfraglimit.value);
    UI_Print_Center(x+w/2, y + 64, w/2, "timelimit", (int)sb_showtimelimit.value);

    if (Options_pos >= 5  &&  Options_pos <= 11)
        UI_DrawCharacter(x + 3*w/4 - 6*8, y + 16 + (Options_pos-5)*8, 141);

    UI_Print_Center(x, y + 80, w, "Network Options:", true);

    UI_Print(x + w/2 - 100, y +  92, "ping timeout         ", false);
    UI_Print(x + w/2 - 100, y + 100, "number of pings      ", false);
    UI_Print(x + w/2 - 100, y + 108, "pings per sec        ", false);

    UI_Print(x + w/2 - 100, y + 120, "serverinfo timeout   ", false);
    UI_Print(x + w/2 - 100, y + 128, "serverinfo retries   ", false);
    UI_Print(x + w/2 - 100, y + 136, "serverinfos per sec  ", false);

    UI_Print(x + w/2 - 100, y + 148, "master server timeout", false);
    UI_Print(x + w/2 - 100, y + 156, "master server retries", false);

    UI_Print(x + w/2 - 100, y + 168, "use proxy server     ", false);

    sprintf(buf, "%d", (int)(sb_pingtimeout.value));
    UI_Print(x + w/2 - 100 + 23*8, y +  92,  buf, false);
    sprintf(buf, "%d", (int)(sb_pings.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 100,  buf, false);
    sprintf(buf, "%d", (int)(sb_pingspersec.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 108,  buf, false);

    if (Options_pos >= 12  &&  Options_pos <= 14)
        UI_DrawCharacter(x + w/2 - 104 + 22*8, y + 92 + (Options_pos - 12)*8, 141);

    sprintf(buf, "%d", (int)(sb_infotimeout.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 120,  buf, false);
    sprintf(buf, "%d", (int)(sb_inforetries.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 128,  buf, false);
    sprintf(buf, "%d", (int)(sb_infospersec.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 136,  buf, false);

    if (Options_pos >= 15  &&  Options_pos <= 17)
        UI_DrawCharacter(x + w/2 - 104 + 22*8, y + 120 + (Options_pos - 15)*8, 141);

    sprintf(buf, "%d", (int)(sb_mastertimeout.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 148,  buf, false);
    sprintf(buf, "%d", (int)(sb_masterretries.value));
    UI_Print(x + w/2 - 100 + 23*8, y + 156,  buf, false);

    if (Options_pos >= 18  &&  Options_pos <= 19)
        UI_DrawCharacter(x + w/2 - 104 + 22*8, y + 148 + (Options_pos - 18)*8, 141);

    UI_Print(x + w/2 - 100 + 23*8, y + 168, cl_useproxy.value ? "on" : "off", false);

    if (Options_pos == 20)
        UI_DrawCharacter(x + w/2 - 104 + 22*8, y + 168, 141);
}

void Serverinfo_Draw ()
{
    extern int server_during_update;
    char buf[500];

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

    strcpy(buf, show_serverinfo->display.name);
    buf[w/8] = 0;
    UI_Print_Center(x, y, w, buf, false);
    strcpy(buf, show_serverinfo->display.ip);
    buf[w/8] = 0;
    UI_Print_Center(x, y+10, w, buf, false);

    strcpy(buf, " players serverinfo sources ");
    if (serverinfo_pos == 0)
        strncpy(buf, "\x10ðìáùåòó\x11", 9);
    if (serverinfo_pos == 1)
        strncpy(buf+8, "\x10óåòöåòéîæï\x11", 12);
    if (serverinfo_pos == 2)
        strncpy(buf+19, "\x10óïõòãåó\x11", 9);

    UI_Print_Center(x, y+24, w, buf, false);

    memset(buf, '\x1E', w/8);
    buf[w/8] = 0;
    buf[w/8-1] = '\x1F';
    buf[0] = '\x1D';
    UI_Print(x, y+32, buf, false);

    if (testing_connection)
        h -= 5*8;

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

    if (testing_connection)
        SB_Test_Frame();
}

void Serverinfo_Players_Draw(int x, int y, int w, int h)
{
    int i;
    int listsize;
    int match = 0;
    int top1=0, bottom1=0, frags1=0;
    int top2=0, bottom2=0, frags2=0;
    int qizmo = 0;

    server_data *s = show_serverinfo;

    if (s == NULL  ||  s->playersn + s->spectatorsn <= 0)
        return;

    if (!strcmp(show_serverinfo->display.gamedir, "qizmo"))
        qizmo = 1;

    // check if this is a teamplay (2 teams) match
    if (!qizmo  &&
        show_serverinfo->playersn > 3 &&
        ValueForKey(show_serverinfo, "teamplay") != NULL  &&
        strcmp(ValueForKey(show_serverinfo, "teamplay"), "0"))
    {
        top1 = show_serverinfo->players[0]->top;
        bottom1 = show_serverinfo->players[0]->bottom;
        frags1 = show_serverinfo->players[0]->frags;
        top2 = -1;
        frags2 = 0;
        match = 1;
        for (i=1; i < show_serverinfo->playersn + show_serverinfo->spectatorsn; i++)
        {
            if (show_serverinfo->players[i]->top == top1  &&
              show_serverinfo->players[i]->bottom == bottom1)
                frags1 += show_serverinfo->players[i]->frags;
            else if ((show_serverinfo->players[i]->top == top2  &&
              show_serverinfo->players[i]->bottom == bottom2)  ||
              top2 == -1)
            {
                frags2 += show_serverinfo->players[i]->frags;
                top2 = show_serverinfo->players[i]->top;
                bottom2 = show_serverinfo->players[i]->bottom;
            }
            else
            {
                match = 0;
                break;
            }
        }

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

    UI_Print(x, y, "png tm frgs name", true);
    for (i=0; i < listsize; i++)
    {
        char buf[100];
        int top, bottom;

        if (serverinfo_players_pos + i >= s->playersn + s->spectatorsn)
            break;
        sprintf(buf, "%3d %2d %3d  %s",
            max(min(s->players[serverinfo_players_pos+i]->ping, 999), 0),
            max(min(s->players[serverinfo_players_pos+i]->time, 99), 0),
            max(min(s->players[serverinfo_players_pos+i]->frags, 999), -99),
            s->players[serverinfo_players_pos+i]->name);

        buf[w/8] = 0;

        top = s->players[serverinfo_players_pos+i]->top;
        bottom = s->players[serverinfo_players_pos+i]->bottom;
    
        Draw_Fill (x+7*8-2, y+i*8+8   +1, 34, 4, top);
        Draw_Fill (x+7*8-2, y+i*8+8+4 +1, 34, 3, bottom);

        UI_Print(x, y+i*8+8, buf, false);
    }

    if (match)
    {
        char buf[100];
        Draw_Fill (x+13*8, y+listsize*8+8   +1, 40, 4, top1);
        Draw_Fill (x+13*8, y+listsize*8+8+4 +1, 40, 4, bottom1);
        Draw_Fill (x+21*8, y+listsize*8+8   +1, 40, 4, top2);
        Draw_Fill (x+21*8, y+listsize*8+8+4 +1, 40, 4, bottom2);

        sprintf(buf, "      score:  %3d  -  %3d", frags1, frags2);
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
        char buf[100];

        if (serverinfo_rules_pos + i >= s->keysn)
            break;
        sprintf(buf, "%-13.13s %-*s",
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
        if (!memcmp(&source->servers[i]->address, &serv->address, 6))
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
        char buf[100], buf2[16];

        if (serverinfo_sources_disp + i >= sourcesn_updated)
            break;

        if (sources[serverinfo_sources_disp+i]->type == type_file)
            strcpy(buf2, " file ");
        else if (sources[serverinfo_sources_disp+i]->type == type_master)
            strcpy(buf2, "master");
        else if (sources[serverinfo_sources_disp+i]->type == type_dummy)
            strcpy(buf2, "dummy ");

        sprintf(buf, "%s   %s", buf2, sources[serverinfo_sources_disp+i]->name);
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


void Sources_Draw (int x, int y, int w, int h)
{
    int i, listsize;
    char line[1000];
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
            strcpy(type, "master");
        else if (s->type == type_file)
            strcpy(type, " file ");
        else if (s->type == type_dummy)
            strcpy(type, "dummy ");
        else
            strcpy(type, " ???? ");

        if (s->type == type_dummy)
            strcpy(time, " n/a ");
        else
            if (s->last_update.wYear)
            {
                if (s->last_update.wYear != curtime.wYear)
                    sprintf(time, "%4dy", s->last_update.wYear);
                else if (s->last_update.wMonth != curtime.wMonth ||
                         s->last_update.wDay != curtime.wDay)
                    sprintf(time, "%02d-%02d", s->last_update.wMonth, s->last_update.wDay);
                else
                    sprintf(time, "%2d:%02d",
                        s->last_update.wHour,
                        s->last_update.wMinute);
            }
            else
                strcpy(time, "never");

        sprintf(line, "%s %c%-17.17s %4d  %s ", type,
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

        sprintf(line, "%d sources selected (%d servers)", sel_sources, sel_servers);
        UI_Print_Center(x, y+h-16, w, line, false);

        sprintf(line, "of %d total (%d servers)", sourcesn, total_servers);
        UI_Print_Center(x, y+h-8, w, line, false);
    }

    // adding source
    if (adding_source)
        Add_Source_Draw();
}


void Players_Draw (int x, int y, int w, int h)
{
    int i, listsize;
    char line[2000];
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
    sprintf(line, "name            %-*s png", hw, "server");
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

        sprintf(line, "%-15.15s%c%-*.*s %3s",
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

void WriteSourcesConfiguration(FILE *f)
{
    int i;
    fprintf(f, "sb_sourceunmarkall\n");
    for (i=0; i < sourcesn; i++)
        if (sources[i]->checked)
            fprintf(f, "sb_sourcemark \"%s\"\n", sources[i]->name);
}


//
// init
//

void Browser_Init(void)
{
    int i;

    Browser_pos = pos_none;
    Servers_pos = 0;
    Sources_pos = 0;
    Servers_disp = 0;
    show_serverinfo = NULL;
    serverinfo_pos = 0;

    for (i=0; i < MAX_SERVERS; i++)
        servers[i] = NULL;

    serversn = serversn_passed = 0;
    sourcesn = 0;

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
    Cvar_Register(&sb_sortservers);
    Cvar_Register(&sb_sortplayers);
    Cvar_Register(&sb_sortsources);
    Cvar_Register(&sb_maxwidth);
    Cvar_Register(&sb_maxheight);
    Cvar_Register(&sb_autohide);
    Cvar_Register(&sb_hideempty);
    Cvar_Register(&sb_hidenotempty);
    Cvar_Register(&sb_hidefull);
    Cvar_Register(&sb_hidedead);
    Cvar_Register(&sb_sourcevalidity);
    Cvar_Register(&sb_showcounters);
    Cvar_Register(&sb_mastercache);
    Cvar_Register(&sb_starttab);
	Cvar_Register(&sb_autoupdate);
	Cvar_ResetCurrentGroup();

//    Cmd_AddCommand("menu_serverbrowser", M_ServerBrowser_f);
    Cmd_AddCommand("addserver", AddServer_f);

    // read sources from SOURCES_PATH
    Reload_Sources();

    Cmd_AddCommand("sb_sourceunmarkall", SB_SourceUnmarkAll);
    Cmd_AddCommand("sb_sourcemark", SB_SourceMark);
}

void Add_Source_Key(int key)
{
    switch (key)
    {
    case K_HOME:
        if (isThisCtrlDown())
            newsource_pos = 0; break;
    case K_END:
        if (isThisCtrlDown())
            newsource_pos = 4; break;
    case K_UPARROW:
        newsource_pos--; break;
    case K_DOWNARROW:
        newsource_pos++; break;
    case K_ENTER:
    case '+':
    case '=':
    case '-':
        switch (newsource_pos)
        {
        case 0:
            newsource_master = !newsource_master; break;
        case 3:
            if (key == K_ENTER)
            {
                FILE *f;
                source_data *s;
                char addr[512];

                if (strlen(edit1.text) <= 0  ||  strlen(edit2.text) <= 0)
                    break;

                // create new source
                s = Create_Source();
                s->type = newsource_master ? type_master : type_file;
                strcpy(s->name, edit1.text);
                strcpy(addr, edit2.text);

                if (s->type == type_file)
                    strcpy(s->address.filename, edit2.text);
                else
                {
                    if (!strchr(addr, ':'))
                        strcat(addr, ":27000");
                    if (!NET_StringToAdr(addr, &(s->address.address)))
                        break;
                }

                sources[sourcesn] = s;
                sourcesn++;

                Sources_pos = sourcesn-1;
                
                // and also add to file
                //f = fopen(SOURCES_PATH, "at");
                //if (f == NULL)
                    //break;
                if (!COM_FCreateFile("sb/sources.txt", &f, "ezquake", "at"))
                    break;

                fprintf(f, "%s \"%s\" %s\n",
                        newsource_master ? "master" : "file",
                        edit1.text,
                        addr);

                fclose(f);

                Mark_Source(sources[Sources_pos]);
                Update_Source(sources[Sources_pos]);
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

    if ((!isThisCtrlDown() || tolower(key)=='v') && !isThisAltDown())
    {
        if (newsource_pos == 1)
            CEditBox_Key(&edit1, key);
        if (newsource_pos == 2)
            CEditBox_Key(&edit2, key);
    }

    newsource_pos = max(newsource_pos, 0);
    newsource_pos = min(newsource_pos, 4);
}

void Add_Server_Key(int key)
{
    switch (key)
    {
    case K_HOME:
        if (isThisCtrlDown())
            newserver_pos = 0; break;
    case K_END:
        if (isThisCtrlDown())
            newserver_pos = 4; break;
    case K_UPARROW:
        newserver_pos--; break;
    case K_DOWNARROW:
        newserver_pos++; break;
    case K_ENTER:
    case '+':
    case '=':
    case '-':
        switch (newserver_pos)
        {
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

    if ((!isThisCtrlDown() || tolower(key)=='v') && !isThisAltDown())
    {
        if (newserver_pos == 0)
            CEditBox_Key(&edit1, key);
    }

    newserver_pos = max(newserver_pos, 0);
    newserver_pos = min(newserver_pos, 2);
}

void Browser_Key(int key)
{
    if (confirmation)
    {
        SB_Confirmation_Key(key);
        return;
    }

	if (key == '`' || key == '~') {
		Con_ToggleConsole_f ();
		return;
	}

    if (key == K_ESCAPE  &&
        show_serverinfo == NULL  &&
        !adding_source  &&
        !adding_server  &&
        ping_phase == 0  &&
        !updating_sources)  // exit from browser to main menu
    {
        M_Menu_Main_f();
    }

    if (show_serverinfo)
    {
        Serverinfo_Key(key);
        return;
    }

    if (adding_source)
    {
        Add_Source_Key(key);
        return;
    }

    if (adding_server)
    {
        Add_Server_Key(key);
        return;
    }

    if (ping_phase || updating_sources)  // no keys when pinging
    {
        if (!abort_ping  &&  (key == K_ESCAPE || key == K_BACKSPACE))
            abort_ping = 1;
        return;
    }

    if (key != K_RIGHTARROW && key != K_LEFTARROW)
        switch (Browser_pos)
        {
            case pos_servers:
                Servers_Key(key); break;
            case pos_sources:
                Sources_Key(key); break;
            case pos_players:
                Players_Key(key); break;
            case pos_options:
                Options_Key(key); break;
            default:
                break;
        }
    else
    {
        searchtype = search_none;
        switch (key)
        {
            case K_RIGHTARROW:
                Browser_pos++;  break;
            case K_LEFTARROW:
                Browser_pos--;  break;
            default: ;
        }
    }
    Browser_pos = (Browser_pos + 4)%4;
}

qboolean SearchNextServer(int pos)
{
    int i;
    char tmp[1024];
    for (i=pos; i < serversn_passed; i++)
    {
        strcpy(tmp, servers[i]->display.name);
        FunToSort(tmp);
        if (strstr(tmp, searchstring))
        {
            Servers_pos = i;
            return true;
        }
    }
    return false;
}

void Servers_Key(int key)
{
    if (serversn_passed <= 0  &&  (key != K_SPACE || isThisAltDown())
        && tolower(key) != 'n'  && key != K_INS)
        return;

    if ((isThisAltDown()  || searchtype == search_server) &&
        key >= ' '  &&  key <= '}')  // search
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
                    strcpy(searchstring, "îïô æïõîä");  // not found
        }
    }
    else
    {
        searchtype = search_none;
        switch (key)
        {
            case K_INS:
            case 'n':       // new server
                newserver_pos = 0;
                CEditBox_Init(&edit1, 14, 64);
                adding_server = 1;
                break;
            case K_TAB: // go to players
                Browser_pos = pos_players;
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
                Servers_pos--; break;
            case K_DOWNARROW:
                Servers_pos++; break;
            case K_HOME:
                Servers_pos = 0; break;
            case K_END:
                Servers_pos = serversn_passed-1; break;
            case K_PGUP:
                Servers_pos -= 10; break;
            case K_PGDN:
                Servers_pos += 10; break;
            case K_ENTER:
                Serverinfo_Start(servers[Servers_pos]); break;
            case K_SPACE:
                GetServerPingsAndInfos(); 
                break;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':   // sorting mode
                if (!isThisCtrlDown())
                {
                    char buf[30];
                    if ((sb_sortservers.string[0] == '-' && sb_sortservers.string[1] == key)
                        || sb_sortservers.string[0] == key)
                    {
                        if (sb_sortservers.string[0] == '-')
                            strncpy(buf, sb_sortservers.string+1, 20);
                        else
                        {
                            strncpy(buf+1, sb_sortservers.string, 20);
                            buf[0] = '-';
                        }
                    }
                    else
                    {
                        char buf[30];
                        strncpy(buf+1, sb_sortservers.string, 20);
                        buf[0] = key;
                    }
                    buf[20] = 0;
                    Cvar_Set(&sb_sortservers, buf);
                    resort_servers = 1;
                }
                else
                {
                    switch (key)
                    {
                    case '2': cvar_toggle(&sb_showaddress);    break;
                    case '3': cvar_toggle(&sb_showping);       break;
                    case '4': cvar_toggle(&sb_showgamedir);    break;
                    case '5': cvar_toggle(&sb_showmap);        break;
                    case '6': cvar_toggle(&sb_showplayers);    break;
                    case '7': cvar_toggle(&sb_showfraglimit);  break;
                    case '8': cvar_toggle(&sb_showtimelimit);  break;
                    }
                }
                break;
            case 'c':   // copy server to clipboard
                    CopyServerToClipboard(servers[Servers_pos]);
                    break;
            case 'v':   // past server into console
                    PasteServerToConsole(servers[Servers_pos]);
                    break;
            default:
                ;
        }
    }

    Servers_pos = max(Servers_pos, 0);
    Servers_pos = min(Servers_pos, serversn-1);
}

void Serverinfo_Key(int key)
{
    switch (key)
    {
        case K_ENTER:
            if (serverinfo_pos != 2)
            {
                if (isThisCtrlDown())
                    Observe_Server(show_serverinfo);
                else
                    Join_Server(show_serverinfo);
            }
            else
                Serverinfo_Sources_Key(key);
            break;
        case K_ESCAPE:
        case K_BACKSPACE:
            Serverinfo_Stop(); break;
        case K_PGUP:
            if (Browser_pos == pos_players)
            {
                if (isThisCtrlDown())
                    Players_pos = 0;
                else
                    Players_pos--;
                Players_pos = max(0, Players_pos);
                Serverinfo_Change(all_players[Players_pos]->serv);
            }
            else
            {
                if (isThisCtrlDown())
                    Servers_pos = 0;
                else
                    Servers_pos--;
                Servers_pos = max(0, Servers_pos);
                Serverinfo_Change(servers[Servers_pos]);
            }
            break;
        case K_PGDN:
            if (Browser_pos == pos_players)
            {
                if (isThisCtrlDown())
                    Players_pos = all_players_n - 1;
                else
                    Players_pos++;
                Players_pos = min(all_players_n-1, Players_pos);
                Serverinfo_Change(all_players[Players_pos]->serv);
            }
            else
            {
                if (isThisCtrlDown())
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
                sprintf(buf, "%d.%d.%d.%d",
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
            serverinfo_players_pos--; break;
        case K_DOWNARROW:
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
            serverinfo_rules_pos--; break;
        case K_DOWNARROW:
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
            serverinfo_sources_pos--; break;
        case K_DOWNARROW:
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


void RemoveSourceProc(void)
{
    source_data *s;
    FILE *f;
    int length;
    char *filebuf; // *p, *q;
    int removed = 0;
    s = sources[Sources_pos];
    if (s->type == type_dummy)
        return;

    // remove from SB
    if (Sources_pos < sourcesn - 1)
    {
        free(sources+Sources_pos);
        memmove(sources+Sources_pos,
                sources+Sources_pos + 1,
                (sourcesn-Sources_pos-1)*sizeof(source_data *));
    }
    sourcesn--;

    // and from file
    //length = COM_FileOpenRead (SOURCES_PATH, &f);
    length = FS_FOpenFile("sb/sources.txt", &f);

    if (length < 0)
    {
        //Com_Printf ("sources file not found: %s\n", SOURCES_PATH);
        return;
    }

    filebuf = (char *)Q_malloc(length + 512);
    filebuf[0] = 0;
    while (!feof(f))
    {
        char c = 'A';
        char line[2000];
        char *p, *q;

        if (fscanf(f, "%[ -~	]s", line) != 1)
	{
	    while (!feof(f)  &&  c != '\n')
		fscanf(f, "%c", &c);
            continue;
	}
        while (!feof(f)  &&  c != '\n')
        {
            int len;
            fscanf(f, "%c", &c);
            len = strlen(line);
            line[len] = c;
            line[len+1] = 0;
        }

        if (removed)
        {
            strcat(filebuf, line);
            continue;
        }
        p = next_nonspace(line);
        if (*p == '/')
        {
            strcat(filebuf, line);
            continue;   // comment
        }

        q = next_space(p);

        if (s->type == type_master && strncmp(p, "master", q-p))
        {
            strcat(filebuf, line);
            continue;
        }
        if (s->type == type_file && strncmp(p, "file", q-p))
        {
            strcat(filebuf, line);
            continue;
        }

        p = next_nonspace(q);
        q = (*p == '\"') ? next_quote(++p) : next_space(p);

        if (q-p <= 0)
        {
            strcat(filebuf, line);
            continue;
        }

        if (strlen(s->name) != q-p  ||  strncmp(s->name, p, q-p))
        {
            strcat(filebuf, line);
            continue;
        }

        removed = 1;
    }
    fclose(f);

    //f = fopen(SOURCES_PATH, "wb");
    //if (f != NULL)
    if (COM_FCreateFile("sb/sources.txt", &f, "ezquake", "wb"))
    {
        fwrite(filebuf, 1, strlen(filebuf), f);
        fclose(f);
    }
    free(filebuf);
}

void Sources_Key(int key)
{
    int i;

    if (sourcesn <= 0)
        return;
    
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
                SB_Confirmation(va("Remove %-.20s", sources[Sources_pos]->name), RemoveSourceProc);
            break;
        case 'u':
            Update_Source(sources[Sources_pos]); break;
        case K_UPARROW:
            Sources_pos--; break;
        case K_DOWNARROW:
            Sources_pos++; break;
        case K_HOME:
            Sources_pos = 0; break;
        case K_END:
            Sources_pos = sourcesn-1; break;
        case K_PGUP:
            Sources_pos -= 10; break;
        case K_PGDN:
            Sources_pos += 10; break;
        case K_ENTER:
            Toggle_Source(sources[Sources_pos++]); break;
        case ']':
            Toggle_Source(sources[Sources_pos]); break;
        case K_SPACE:
            source_full_update = (isThisCtrlDown());
            Update_Multiple_Sources(sources, sourcesn); 
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
                char buf[30];
                if (sb_sortsources.string[0] == '-')
                    strncpy(buf, sb_sortsources.string+1, 20);
                else
                {
                    strncpy(buf+1, sb_sortsources.string, 20);
                    buf[0] = '-';
                }
                buf[20] = 0;
                Cvar_Set(&sb_sortsources, buf);
            }
            else
            {
                char buf[30];
                strncpy(buf+1, sb_sortsources.string, 20);
                buf[0] = key;
                buf[20] = 0;
                Cvar_Set(&sb_sortsources, buf);
            }
            resort_sources = 1;
            break;
        default:
            ;
    }

    Sources_pos = max(Sources_pos, 0);
    Sources_pos = min(Sources_pos, sourcesn-1);
    return;
}

qboolean SearchNextPlayer(int pos)
{
    int i;
    char tmp[1024];
    for (i=pos; i < all_players_n; i++)
    {
        strcpy(tmp, all_players[i]->name);
        FunToSort(tmp);
        if (strstr(tmp, searchstring))
        {
            Players_pos = i;
            return true;
        }
    }
    return false;
}

void Players_Key(int key)
{
    int i;

    if (all_players_n <= 0  &&  key != K_SPACE)
        return;
    
    if ((isThisAltDown()  ||  searchtype == search_player)  &&
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
                    strcpy(searchstring, "not found");  // not found
        }
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
                GetServerPingsAndInfos();
                break;
            case K_TAB: // go to servers -- locate
                Servers_pos = 0;
                for (i=0; i < serversn_passed; i++)
                {
                    if (servers[i] == all_players[Players_pos]->serv)
                    {
                        Servers_pos = i;
                        break;
                    }
                }
                Browser_pos = pos_servers;
                break;
            case K_UPARROW:
                Players_pos--; break;
            case K_DOWNARROW:
                Players_pos++; break;
            case K_HOME:
                Players_pos = 0; break;
            case K_END:
                Players_pos = all_players_n-1; break;
            case K_PGUP:
                Players_pos -= 10; break;
            case K_PGDN:
                Players_pos += 10; break;
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
                    char buf[30];
                    if (sb_sortplayers.string[0] == '-')
                        strncpy(buf, sb_sortplayers.string+1, 20);
                    else
                    {
                        strncpy(buf+1, sb_sortplayers.string, 20);
                        buf[0] = '-';
                    }
                    buf[20] = 0;
                    Cvar_Set(&sb_sortplayers, buf);
                }
                else
                {
                    char buf[30];
                    strncpy(buf+1, sb_sortplayers.string, 20);
                    buf[0] = key;
                    buf[20] = 0;
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
            default:
                ;
        }
    }

    Players_pos = max(Players_pos, 0);
    Players_pos = min(Players_pos, all_players_n-1);
}

void Options_Key(int key)
{
    int d;
    
    switch (key)
    {
        case K_UPARROW:
            Options_pos--; break;
        case K_DOWNARROW:
            Options_pos++; break;
        case K_HOME:
            Options_pos = 0; break;
        case K_END:
            Options_pos = 20; break;
        case K_PGUP:
            if (Options_pos >= 12)
                Options_pos = 5;
            else if (Options_pos >= 5)
                Options_pos = 0;
            break;
        case K_PGDN:
            if (Options_pos < 5)
                Options_pos = 5;
            else if (Options_pos < 12)
                Options_pos = 12;
            break;
        case K_ENTER:
            switch (Options_pos)
            {
            case 0: // filter empty
                cvar_toggle(&sb_hideempty); break;
            case 1: // filter not empty
                cvar_toggle(&sb_hidenotempty); break;
            case 2: // filter full
                cvar_toggle(&sb_hidefull); break;
            case 3: // filter dead
                cvar_toggle(&sb_hidedead); break;
            case 4: // show counters
                cvar_toggle(&sb_showcounters); break;
            case 5: // show address
                cvar_toggle(&sb_showaddress); break;
            case 6: // show ping
                cvar_toggle(&sb_showping); break;
            case 7: // show gamedir
                cvar_toggle(&sb_showgamedir); break;
            case 8: // show map
                cvar_toggle(&sb_showmap); break;
            case 9: // show address
                cvar_toggle(&sb_showplayers); break;
            case 10: // show address
                cvar_toggle(&sb_showfraglimit); break;
            case 11: // show address
                cvar_toggle(&sb_showtimelimit); break;
            case 20: // use proxy
				if (cl_useproxy.value == 1) {
                    Cvar_SetValue(&cl_useproxy, 0); break;
				}
				else {
					Cvar_SetValue(&cl_useproxy, 1); break;
				}
            }
            if (Options_pos >= 0  &&  Options_pos <= 3)
                resort_servers = 1;
            break;
        case '-':
        case '+':
        case '=':
            d = (key == '-' ? -1 : 1);
            switch (Options_pos)
            {
            case 12: // ping timeout
                Cvar2_Set(&sb_pingtimeout, min(max((int)(sb_pingtimeout.value) +50*d, 50), 5000)); break;
            case 13: // number of pings
                Cvar2_Set(&sb_pings, min(max((int)(sb_pings.value) +1*d, 1), 20)); break;
            case 14: // pings per sec
                Cvar2_Set(&sb_pingspersec, min(max((int)(sb_pingspersec.value) +10*d, 10), 1000)); break;

            case 15: // info timeout
                Cvar2_Set(&sb_infotimeout, min(max((int)(sb_infotimeout.value) +50*d, 50), 5000)); break;
            case 16: // info retries
                Cvar2_Set(&sb_inforetries, min(max((int)(sb_inforetries.value) +1*d, 1), 20)); break;
            case 17: // infos per sec
                Cvar2_Set(&sb_infospersec, min(max((int)(sb_infospersec.value) +10*d, 10), 1000)); break;

            case 18: // master timeout
                Cvar2_Set(&sb_mastertimeout, min(max((int)(sb_mastertimeout.value) +50*d, 50), 5000)); break;
            case 19: // master retries
                Cvar2_Set(&sb_masterretries, min(max((int)(sb_masterretries.value) +1*d, 1), 20)); break;
            }
            break;
        default:
            ;
    }

    Options_pos = max(Options_pos, 0);
    Options_pos = min(Options_pos, 20);
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

        if (sb_hidedead.value  &&  s->ping < 0)
            continue;

        if (sb_hideempty.value  &&  s->playersn + s->spectatorsn <= 0)
            continue;

        if (sb_hidenotempty.value  &&  s->playersn + s->spectatorsn > 0)
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
    Filter_Servers();
    qsort(servers, serversn, sizeof(servers[0]), Servers_Compare_Func);
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
    int i, j;
    int players = 0;

    // clear
    if (all_players != NULL)
    {
        for (i=0; i < all_players_n; i++)
            free(all_players[i]);
        free(all_players);
    }

    // count players
    for (i=0; i < serversn; i++)
        players += servers[i]->playersn + servers[i]->spectatorsn;

    // alloc memory
    all_players = (player_host **) Q_malloc(players * sizeof(player_host *));

    // make players
    all_players_n = 0;
    for (i=0; i < serversn; i++)
        for (j=0; j < servers[i]->playersn + servers[i]->spectatorsn; j++)
        {
            all_players[all_players_n] = (player_host *) Q_malloc(sizeof(player_host));
            strncpy(all_players[all_players_n]->name,
                    servers[i]->players[j]->name, 20);
            servers[i]->players[j]->name[20] = 0;
            all_players[all_players_n]->serv = servers[i];
            all_players_n++;
        }
    resort_all_players = 1;
    rebuild_all_players = 0;
    //Players_pos = 0;
}

void Shutdown_SB(void)
{
    Serverinfo_Stop();
    Sys_MSleep(150);     // wait for thread to terminate
}
