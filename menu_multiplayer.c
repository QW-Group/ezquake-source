
#include "quakedef.h"
#include "keys.h"
#include "EX_browser.h"
#include "settings.h"
#include "settings_page.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "menu.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#endif
#include "menu_multiplayer.h"

#define BROWSERPADDING 4

CTab_t sb_tab;

extern cvar_t scr_scaleMenu;
#ifdef GLQUAKE
extern int menuwidth;
extern int menuheight;
#else
#define menuwidth vid.width
#define menuheight vid.height
#endif

static settings_page sbsettings;
static setting sbsettings_arr[] = {
	ADDSET_SEPARATOR("Server Filters"),
	ADDSET_BOOL		("Hide Empty", sb_hideempty),
	ADDSET_BOOL		("Hide Full", sb_hidefull),
	ADDSET_BOOL		("Hide Not Empty", sb_hidenotempty),
	ADDSET_BOOL		("Hide Dead", sb_hidedead),

	ADDSET_SEPARATOR("Display Columns"),
	ADDSET_BOOL		("Show Ping", sb_showping),
	ADDSET_BOOL		("Show Map", sb_showmap),
	ADDSET_BOOL		("Show Gamedir", sb_showgamedir),
	ADDSET_BOOL		("Show Players", sb_showplayers),
	ADDSET_BOOL		("Show Timelimit", sb_showtimelimit),
	ADDSET_BOOL		("Show Fraglimit", sb_showfraglimit),
	ADDSET_BOOL		("Show Server Address", sb_showaddress),

	ADDSET_SEPARATOR("Display"),
	ADDSET_BOOL		("Server Status", sb_status),

	ADDSET_SEPARATOR("Network Filters"),
	ADDSET_NUMBER	("Ping Timeout", sb_pingtimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Pings Per Server", sb_pings, 1, 5, 1),
	ADDSET_NUMBER	("Pings Per Second", sb_pingspersec, 10, 300, 10),
	ADDSET_NUMBER	("Master Timeout", sb_mastertimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Master Retries", sb_masterretries, 1, 5, 1),
	ADDSET_NUMBER	("Info Timeout", sb_infotimeout, 50, 1000, 50),
	ADDSET_NUMBER	("Info Retries", sb_inforetries, 0, 4, 1),
	ADDSET_NUMBER	("Infos Per Second", sb_infospersec, 10, 1000, 10)
};

typedef enum gametype_basic_e {
	GT_COOPERATIVE = 0,
	GT_DEATHMATCH,
	GT_BOTMATCH,
	GT_UBOUND
} gametype_basic_t;
static const char* gametype_basic_desc[GT_UBOUND] = {
	"Cooperative", "Deathmatch", "Bot Match"
};

typedef enum game_weaponsmode_e {
	GWM_WEAPONSRESPAWN = 0,
	GWM_WEAPONSSTAY,
	GWM_INSTANTWEAPONS,
	GWM_UBOUND
} game_weaponsmode_t;
static const char* game_weaponsmode_desc[GWM_UBOUND] = {
	"Weapons Respawn", "Weapons Stay", "Instant Weapons"
};

typedef enum game_skill_e {
	GSKILL_EASY = 0,
	GSKILL_NORMAL,
	GSKILL_HARD,
	GSKILL_NIGHTMARE,
	GSKILL_UBOUND
} game_skill_t;
static const char* game_skill_desc[GSKILL_UBOUND] = {
	"Easy", "Normal", "Hard", "Nightmare"
};

typedef enum game_map_group_e {
	GMG_EPISODE1,	
	GMG_EPISODE2,
	GMG_EPISODE3,
	GMG_EPISODE4,
	GMG_CUSTOM,
	GMG_UBOUND
} game_map_group_t;
static const char* game_map_group_desc[GMG_UBOUND] = {
	"Doomed Dimension", "Realm of Black Magic", "Netherworld", "The Elder World", "Custom"
};
typedef struct gamesettings_basic_s {
	gametype_basic_t	gametype;
	game_weaponsmode_t	weaponsmode;
	game_skill_t		skill;
	unsigned int		teamplay;
	unsigned int		maxclients;
	unsigned int		maxspectators;
	game_map_group_t	mapgroup;
	unsigned int		mapnum;
	unsigned int		timelimit;
	unsigned int		fraglimit;
} gamesettings_basic_t;

gamesettings_basic_t menu_creategame_settings;

#define GENERATE_ENUM_TOGGLE_PROC(basename,var,upperbound)	\
	static void Menu_CG_##basename##_t(qbool back) {		\
		(var) += back ? -1 : 1;			\
		if ((var) >= upperbound) {		\
			(var) = 0;					\
		} else if ((var) < 0) {			\
			(var) = upperbound - 1;		\
		}								\
	}

#define GENERATE_ENUM_READ_PROC(basename,var,ubound,descarray)	\
	static const char* Menu_CG_##basename##_r(void) {	\
		if (((var) > 0) && ((var) < ubound)) {			\
			return descarray[var];						\
		} else {										\
			return descarray[0];						\
		}												\
	}

#define GENERATE_ENUM_MENU_FUNC(type,ubound,desc) \
	GENERATE_ENUM_READ_PROC(type,menu_creategame_settings.type,ubound,desc) \
	GENERATE_ENUM_TOGGLE_PROC(type,menu_creategame_settings.type,ubound)

GENERATE_ENUM_MENU_FUNC(gametype,GT_UBOUND,gametype_basic_desc)
GENERATE_ENUM_MENU_FUNC(weaponsmode,GWM_UBOUND,game_weaponsmode_desc)
GENERATE_ENUM_MENU_FUNC(skill,GSKILL_UBOUND,game_skill_desc)
GENERATE_ENUM_MENU_FUNC(mapgroup,GMG_UBOUND,game_map_group_desc)

static const char *Menu_CG_teamplay_r(void) { return ""; }
static void Menu_CG_teamplay_t(qbool back) {}

static const char *Menu_CG_maxclients_r(void) { return ""; }
static void Menu_CG_maxclients_t(qbool back) {}

static const char *Menu_CG_maxspectators_r(void) { return ""; }
static void Menu_CG_maxspectators_t(qbool back) {}

static const char *Menu_CG_timelimit_r(void) { return ""; }
static void Menu_CG_timelimit_t(qbool back) {}

static const char *Menu_CG_fraglimit_r(void) { return ""; }
static void Menu_CG_fraglimit_t(qbool back) {}

static const char *Menu_CG_map_r(void) { return ""; }
static void Menu_CG_map_t(qbool back) {}

void Menu_CG_StartGame(void) {
	switch (menu_creategame_settings.gametype) {
		case GT_BOTMATCH:
			Cbuf_AddText("disconnect;gamedir fbca;exec configs/duel;");
			break;
		case GT_DEATHMATCH:
			Cbuf_AddText("disconnect;deathmatch 3;gamedir qw;map dm4");
			break;
		case GT_COOPERATIVE:
			Cbuf_AddText("disconnect;deathmatch 0;coop 1;map start");
			break;
	}
}

static settings_page create_game_options;
static setting create_game_options_arr[] = {
	ADDSET_SEPARATOR("Create game"),
	ADDSET_CUSTOM("Game type", Menu_CG_gametype_r, Menu_CG_gametype_t,
				  "Choose one of the three available game types"),
	ADDSET_CUSTOM("Weapons mode", Menu_CG_weaponsmode_r, Menu_CG_weaponsmode_t,
				  ""),
	ADDSET_CUSTOM("Teamplay", Menu_CG_teamplay_r, Menu_CG_teamplay_t, ""),
	ADDSET_CUSTOM("Skill", Menu_CG_skill_r, Menu_CG_skill_t, ""),
	ADDSET_CUSTOM("Player slots", Menu_CG_maxclients_r, Menu_CG_maxclients_t, ""),
	ADDSET_CUSTOM("Spectator slots", Menu_CG_maxspectators_r, Menu_CG_maxspectators_t,
				  ""),
	ADDSET_CUSTOM("Timelimit", Menu_CG_timelimit_r, Menu_CG_timelimit_t, ""),
	ADDSET_CUSTOM("Fraglimit", Menu_CG_fraglimit_r, Menu_CG_fraglimit_t, ""),
	ADDSET_CUSTOM("Map Group", Menu_CG_mapgroup_r, Menu_CG_mapgroup_t, ""),
	ADDSET_CUSTOM("Map", Menu_CG_map_r, Menu_CG_map_t, ""),
	ADDSET_ACTION("Start Game", Menu_CG_StartGame, "Start game with given parameters")
};

static void Servers_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Servers_Draw(x,y,w,h);
}

static int Servers_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Servers_Key(key);
}

static void Servers_OnShow(void)
{
	SB_Servers_OnShow();
}

static qbool Servers_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Servers_Mouse_Event(ms);
}

static void Sources_Draw (int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Sources_Draw(x,y,w,h);
}

static int Sources_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Sources_Key(key);
}

static qbool Sources_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Sources_Mouse_Event(ms);
}

static void Players_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	SB_Players_Draw(x, y, w, h);
}

static int Players_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return SB_Players_Key(key);
}
static qbool Players_Mouse_Event(const mouse_state_t *ms)
{
	return SB_Players_Mouse_Event(ms);
}

static qbool Options_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&sbsettings, ms);
}

static int Options_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return Settings_Key(&sbsettings, key);
}

static void Options_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x, y, w, h, &sbsettings);
}

void Menu_MultiPlayer_Draw (void)
{
	int x, y, w, h;

#ifdef GLQUAKE
	// do not scale this menu
	if (scr_scaleMenu.value)
	{
		menuwidth = vid.width;
		menuheight = vid.height;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	}
#endif

	w = min(640, vid.width) - BROWSERPADDING*2;
	h = min(480, vid.height) - BROWSERPADDING*2;
	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;

	Browser_window.x = x;
	Browser_window.y = y;
	Browser_window.w = w;
	Browser_window.h = h;

	CTab_Draw(&sb_tab, x, y, w, h);

	SB_Specials_Draw();
}

void Menu_MultiPlayer_Key(int key)
{
	if (SB_Specials_Key(key)) return;
	
	CTab_Key(&sb_tab, key);
}

qbool Menu_MultiPlayer_Mouse_Event(const mouse_state_t *ms)
{
	mouse_state_t nms = *ms;

    if (ms->button_up == 2) {
        Menu_MultiPlayer_Key(K_MOUSE2);
        return true;
    }

	nms.x -= Browser_window.x;
	nms.y -= Browser_window.y;
	nms.x_old -= Browser_window.x;
	nms.y_old -= Browser_window.y;

	return CTab_Mouse_Event(&sb_tab, &nms);
}

static qbool CreateGame_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&create_game_options, ms);
}

static int CreateGame_Key(int key, CTab_t *tab, CTabPage_t *page)
{
	return Settings_Key(&create_game_options, key);
}

static void CreateGame_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x,y,w,h,&create_game_options);
}

CTabPage_Handlers_t sb_servers_handlers = {
	Servers_Draw,
	Servers_Key,
	Servers_OnShow,
	Servers_Mouse_Event
};

CTabPage_Handlers_t sb_sources_handlers = {
	Sources_Draw,
	Sources_Key,
	NULL,
	Sources_Mouse_Event
};

CTabPage_Handlers_t sb_players_handlers = {
	Players_Draw,
	Players_Key,
	NULL,
	Players_Mouse_Event
};

CTabPage_Handlers_t sb_options_handlers = {
	Options_Draw,
	Options_Key,
	NULL,
	Options_Mouse_Event
};

CTabPage_Handlers_t sb_creategame_handlers = {
	CreateGame_Draw,
	CreateGame_Key,
	NULL,
	CreateGame_Mouse_Event
};

void Menu_MultiPlayer_Init()
{
	Settings_Page_Init(sbsettings, sbsettings_arr);
	Settings_Page_Init(create_game_options, create_game_options_arr);

	CTab_Init(&sb_tab);
	CTab_AddPage(&sb_tab, "servers", SBPG_SERVERS, &sb_servers_handlers);
	CTab_AddPage(&sb_tab, "sources", SBPG_SOURCES, &sb_sources_handlers);
	CTab_AddPage(&sb_tab, "players", SBPG_PLAYERS, &sb_players_handlers);
	CTab_AddPage(&sb_tab, "options", SBPG_OPTIONS, &sb_options_handlers);
	CTab_AddPage(&sb_tab, "create", SBPG_CREATEGAME, &sb_creategame_handlers);
	CTab_SetCurrentId(&sb_tab, SBPG_SERVERS);
}

void Menu_MultiPlayer_SwitchToServersTab(void)
{
	CTab_SetCurrentId(&sb_tab, SBPG_SERVERS);
}
