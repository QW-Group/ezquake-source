#ifndef __EX_BROWSER__H__
#define __EX_BROWSER__H__

#include "localtime.h"
#include "keys.h"

#define MAX_UNBOUND 200

#define MAX_SOURCES 200
#define MAX_SERVERS 5000

#define MAX_KEYS    100
#define MAX_PLAYERS 128

// column width
#define COL_PING        3
#define COL_IP          21
#define COL_NAME        60  // max
#define COL_MAP         8
#define COL_GAMEDIR     8
#define COL_PLAYERS     5
#define COL_FRAGLIMIT   2
#define COL_TIMELIMIT   2


extern cvar_t  sb_status;           // show status-bar

extern cvar_t  sb_showping;         // show ping column
extern cvar_t  sb_showaddress;      // show address colums
extern cvar_t  sb_showmap;          // show map column
extern cvar_t  sb_showgamedir;      // show gamedir column
extern cvar_t  sb_showplayers;      // show players column
extern cvar_t  sb_showfraglimit;    // show fraglimit column
extern cvar_t  sb_showtimelimit;    // show timelimit column

extern cvar_t  sb_pingtimeout;      // ping server timeout
extern cvar_t  sb_pings;            // number of pings
extern cvar_t  sb_pingspersec;      // pings per second

extern cvar_t  sb_infotimeout;      // get serverinfo timeout
extern cvar_t  sb_inforetries;      // max serverinfo retries
extern cvar_t  sb_infospersec;      // serverinfos per second

extern cvar_t  sb_mastertimeout;    // master server server timeout
extern cvar_t  sb_masterretries;    // max master-server retries
extern cvar_t  sb_nosockraw;        // when enabled, forces "new ping" (udp qw query packet, multithreaded) to be used

extern cvar_t  sb_liveupdate;       // serveinfo window update interval (0 = off)

extern cvar_t  sb_sortplayers;      // sorting mode in players list
extern cvar_t  sb_sortservers;      // sorting mode in servers list

extern cvar_t  sb_autohide;         // hide browser after connect

extern cvar_t  sb_sourcevalidity;   // source validity in minutes
extern cvar_t  sb_mastercache;      // if cache master query results

extern cvar_t  sb_hideempty;
extern cvar_t  sb_hidenotempty;
extern cvar_t  sb_hidefull;
extern cvar_t  sb_hidedead;
extern cvar_t  sb_hidehighping;
extern cvar_t  sb_pinglimit;
extern cvar_t  sb_showproxies;

typedef struct column_s
{
    char ping [COL_PING + 1];
    char ip [COL_IP + 1];
    char name [COL_NAME + 1];
    char map [COL_MAP + 1];
    char gamedir [COL_GAMEDIR + 1];
    char players [COL_PLAYERS + 1];
    char fraglimit [COL_FRAGLIMIT + 1];
    char timelimit [COL_TIMELIMIT + 1];
} columns;


typedef struct playerinfo_s
{
    int id;
    int frags;
    int time;
    int ping;
    char name[21];
    char skin[21];
    char team[21];
    int top;
    int bottom;
	qbool spec; // flag: is spectator or player
} playerinfo;

typedef enum {
	SERVER_EMPTY = 0, SERVER_NONEMPTY = 1, SERVER_FULL = 2
} server_occupancy;

typedef struct server_data_s
{
    int passed_filters;
    int ping;
    netadr_t    address;
    columns     display;
    char *keys[MAX_KEYS], *values[MAX_KEYS];
    int keysn;

    playerinfo *players[MAX_PLAYERS];
    int playersn;
	server_occupancy	occupancy;
    int spectatorsn;
	qbool qizmo;
	qbool qwfwd;
	qbool support_teams; // is server support team per player
} server_data;


#define MAX_SOURCE_NAME 25

typedef enum sb_source_type_e {
	type_master,
	type_file,
	type_dummy
} sb_source_type_t;

typedef struct source_data_s
{
    sb_source_type_t type;           // source type
    union
    {
        netadr_t    address;            // IP for master type
        char        filename[200];      // filename for file type
    } address;

    char name[MAX_SOURCE_NAME+1];       // source name
    SYSTEMTIME last_update;             // last update time

    server_data **servers;              // servers list
    int serversn;                       // servers no

    int checked;                        // 1 if use this source
    int unique;                         // order in file (for sorting)
} source_data;

typedef struct player_host_s
{
    char name[21];
    server_data *serv;
} player_host;

typedef struct serverbrowser_window_s {
	int x, y, w, h;
} serverbrowser_window_t;
extern serverbrowser_window_t Browser_window;

extern server_data * show_serverinfo;

extern int resort_servers;
extern int rebuild_servers_list;
extern int resort_all_players;
extern int rebuild_all_players;
extern int ping_phase;
extern double ping_pos;
extern int updating_sources;
extern int abort_ping;
extern int source_full_update;

// sources table
extern source_data *sources[MAX_SOURCES];
extern int sourcesn;

// servers table
extern server_data *servers[MAX_SERVERS];
extern int serversn;
extern int serversn_passed;
extern int Servers_pos;
extern int resort_sources;
extern int source_unique;


// servers
server_data * Create_Server(char *ip);
server_data * Create_Server2(netadr_t n);
void Reset_Server(server_data *s);
void Delete_Server(server_data *s);
source_data * Create_Source(void);
void Reset_Source(source_data *s);
void Delete_Source(source_data *s);
void Update_Source(source_data *s);
void Reload_Sources(void);
void Rebuild_Servers_List(void);
void Toggle_Source(source_data *);
void Mark_Source(source_data *s);
void Unmark_Source(source_data *s);
void MarkDefaultSources(void);
void WriteSourcesConfiguration(FILE *f);
void Rebuild_All_Players(void);
void Sort_All_Players(void);
void DumpSource(source_data *s);
void RemoveFromFileSource(source_data *source, server_data *serv);
void AddToFileSource(source_data *source, server_data *serv);
int IsInSource(source_data *source, server_data *serv);
void Precache_Source(source_data *s);

char * next_space(char *s);
char * next_nonspace(char *s);
char * next_quote(char *s);


// sources
qbool SB_Sources_Dump(void);
int SB_Source_Add(const char* name, const char* address, sb_source_type_t type);
// asynchronous sources update (in new thread)
void SB_Sources_Update_Begin(qbool full);
// synchronous sources update
void SB_Sources_Update(qbool full);
void SB_Source_Remove(int i);


// net
void GetServerInfo(server_data *serv);
void GetServerPing(server_data *serv);
void GetServerPingsAndInfos(int full);
void GetServerPingsAndInfos_f(void);
void Start_Autoupdate(server_data *s);
void Alter_Autoupdate(server_data *s);

char *ValueForKey(server_data *s, char *k);

void SetPing999(server_data *s);
void SetPing(server_data *s, int ping);

void Shutdown_SB(void);
void SB_RootInit(void);    // must be called as root

// connection tester
void SB_Test_Init(char *address);
void SB_Test_Change(char *address);
void SB_Test_Frame(void);

void Browser_Init(void);
void Browser_Init2(void);

void SB_Servers_Draw (int x, int y, int w, int h);
int SB_Servers_Key(int key);
qbool SB_Servers_Mouse_Event(const mouse_state_t *ms);
void SB_Servers_OnShow (void);

void SB_Sources_Draw (int x, int y, int w, int h);
int SB_Sources_Key(int key);
qbool SB_Sources_Mouse_Event(const mouse_state_t *ms);

void SB_Players_Draw (int x, int y, int w, int h);
int SB_Players_Key(int key);
qbool SB_Players_Mouse_Event(const mouse_state_t *ms);

void SB_Specials_Draw(void);
qbool SB_Specials_Key(int key, wchar unichar);

#endif  // __EX_BROWSER__H__
