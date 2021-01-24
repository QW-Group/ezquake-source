/*
Copyright (C) 2001-2002 jogihoogi

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

$Id: mvd_utils.h,v 1.5 2007-09-24 21:41:17 johnnycz Exp $
*/

#ifndef MVDUTILS_HEADER
#define MVDUTILS_HEADER

// main header for a group of MVD tools: mvd_utils, mvd_xmlstats, mvd_autotrack
void MVD_Screen (void);

// initialize the module, add variables and commands
void MVD_Utils_Init(void); 
void MVD_Mainhook(void);
void MVD_Stats_Cleanup(void);
void MVD_ClockList_TopItems_Draw(double time_limit, int style, int x, int y, float scale, int filter, qbool backpacks, qbool proportional);
void MVD_ClockList_TopItems_DimensionsGet(double time_limit, int style, int *width, int *height, float scale, qbool backpacks, qbool proportional);

// update match info structures
void MVD_Init_Info(int player_slot);
void MVD_GameStart(void);
void MVD_Initialise(void);

// //ktx event notifications
void MVDAnnouncer_MatchStart(void);
void MVDAnnouncer_ItemTaken(const char* s);
void MVDAnnouncer_StartTimer(const char* s);
void MVDAnnouncer_PackDropped(const char* s);
void MVDAnnouncer_Expired(const char* s);
void MVDAnnouncer_BackpackPickup(const char* s);
void CL_ReadKtxDamageIndicatorString(const char* s);

// Powerup cams
qbool MVD_PowerupCam_Enabled(void);
void MVD_PowerupCam_Frame(void);

// JSON stats embedded in mvd
typedef struct {
	int frags;
	int deaths;
	int teamkills;
	int spawnfrags;
	int kills;
	int suicides;
} mvd_player_basic_stats;

typedef struct {
	int taken;
	int given;
	int team;
	int self;
	int team_weapons;
	int enemy_weapons;
	int taken_to_die;
} mvd_player_dmg_stats;

typedef struct {
	int max;
	int quad;
} mvd_player_spree_stats;

typedef struct {
	qbool present;
	float max;
	float avg;
} mvd_player_speed_stats;

typedef struct {
	int attacks;
	int hits;
	int real;
	int virtual;
} mvd_player_weapon_accuracy_stats;

typedef struct {
	int total;
	int team;
	int enemy;
	int self;
} mvd_player_weapon_kill_stats;

typedef struct {
	int enemy;
	int team;
} mvd_player_weapon_dmg_stats;

typedef struct {
	char name[16];
	mvd_player_weapon_accuracy_stats accuracy;
	mvd_player_weapon_kill_stats kills;
	int deaths;
	int pickups;
	int dropped;
	int taken;
	int total_taken;
	int spawn_taken;
	int spawn_total_taken;
	mvd_player_weapon_dmg_stats damage;
} mvd_player_weapon_stats;

typedef struct {
	char name[64];
	int taken;
	int time;
} mvd_player_item_stats;

typedef struct {
	qbool present;
	int points;
	int flag_caps;
	int defends;
	int carrier_defends;
	int carrier_frags;
	int pickups;
	int returns;
	int runes[4];
} mvd_player_ctf_stats;

typedef struct {
	int coilgun;
	int axe;
	int stomp;
	int multi;
	int air;
	int best_multi;
} mvd_player_instagib_gib_stats;

typedef struct {
	qbool present;
	int avg_height;
	int max_height;
	int rings;
	mvd_player_instagib_gib_stats gibs;
} mvd_player_instagib_stats;

typedef struct {
	int bronze;
	int silver;
	int gold;
	int platinum;
} mvd_player_midair_type_stats;

typedef struct {
	float total;
	float max;
	float avg;
} mvd_player_midair_height_stats;

typedef struct {
	qbool present;
	int stomps;
	mvd_player_midair_type_stats midairs;
	int total;
	int bonus;
	mvd_player_midair_height_stats heights;
} mvd_player_midair_stats;

typedef struct {
	qbool present;
	int wins;
	int losses;
} mvd_player_rocket_arena_stats;

#define HOONYMODE_MAXROUNDS 64

typedef struct {
	qbool present;
	int classic_roundcount;
	char classic_rounds[HOONYMODE_MAXROUNDS];
	int blitz_roundcount;
	int blitz_roundfrags[HOONYMODE_MAXROUNDS];
} mvd_player_hoonymode_stats;

#define LGCMODE_DISTANCE_BUCKETS 32

typedef struct {
	qbool present;
	int under;
	int over;
	int hit_buckets;
	int hits[LGCMODE_DISTANCE_BUCKETS];
	int misses[LGCMODE_DISTANCE_BUCKETS];
} mvd_player_lgc_stats;

typedef struct {
	qbool present;
	int skill;
	qbool customised;
} mvd_player_bot_stats;

typedef struct {
	int topcolor;
	int bottomcolor;
	int ping;
	char login[64];
	char team[64];
	mvd_player_basic_stats stats;
	mvd_player_dmg_stats dmg;
	int transferred_packs;
	mvd_player_spree_stats spree;
	float control;
	mvd_player_speed_stats speed;
	int handicap;
	mvd_player_weapon_stats weapons[16];
	mvd_player_item_stats items[32];
	mvd_player_ctf_stats ctf;
	mvd_player_instagib_stats instagib;
	mvd_player_midair_stats midair;
	mvd_player_rocket_arena_stats rocket_arena;
	mvd_player_hoonymode_stats hoonymode;
	mvd_player_lgc_stats lgc;
	mvd_player_bot_stats bot;
} mvd_player_stats;

typedef struct {
	int version;
	time_t date;
	char map[32];
	char hostname[32];
	char ip[64];
	int port;
	char matchtag[64];
	char mode[16];
	int timelimit;
	int fraglimit;
	int dmm;
	int teamplay;
	int duration;
	char demoname[128];

	mvd_player_stats players;
} mvd_game_stats;

#endif // MVDUTILS_HEADER
