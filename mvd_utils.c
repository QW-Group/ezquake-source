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

$Id: mvd_utils.c,v 1.57 2007-10-11 17:56:47 johnnycz Exp $
*/

// core module of the group of MVD tools: mvd_utils, mvd_xmlstats, mvd_autotrack

#include "quakedef.h"
#include <math.h>
#include "parser.h"
#include "localtime.h"
#include "gl_model.h"
#include "teamplay.h"
#include "utils.h"
#include "mvd_utils_common.h"
#include "Ctrl.h"
#include "sbar.h"
#include "vx_tracker.h"

#define MENTION_PICKED_UP_ITEM  1
#define MENTION_PICKED_UP_PACK  2
#define MENTION_DIED_WITH_ITEM -1
#define MENTION_ITEM_RAN_OUT   -2
#define MENTION_DROPPED_ITEM   -3

#define MAX_ANNOUNCER_LINES 16
#define MAX_ANNOUNCER_LINE_TIME 10
#define MAX_ANNOUNCER_LINE_TIME_FADE 8
static char announcer_line_strings[MAX_ANNOUNCER_LINES][256];
static double announcer_line_times[MAX_ANNOUNCER_LINES];
static int announcer_lines;

static const char* MVD_AnnouncerTeamPlayerName(player_info_t* info);

// Can contain 'pack'
#define MVD_ANNOUNCER_ITEM_LENGTH 9

extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_separator, tp_name_backpack, tp_name_suit;
extern cvar_t tp_name_axe, tp_name_sg, tp_name_ssg, tp_name_ng, tp_name_sng, tp_name_gl, tp_name_rl, tp_name_lg, tp_name_ga, tp_name_ya, tp_name_ra, tp_name_mh;

mvd_gt_info_t mvd_gt_info[mvd_gt_types] = {
	{gt_1on1,"duel"},
	{gt_2on2,"2on2"},
	{gt_3on3,"3on3"},
	{gt_4on4,"4on4"},
	{gt_unknown,"unknown"},
};

mvd_cg_info_s mvd_cg_info;

mvd_wp_info_t mvd_wp_info[mvd_info_types] = {
	{AXE_INFO,  "axe",  IT_AXE,              "axe",         0,                  0, 0xDA, 0xDA, 0xDA, NULL,          "&cf0f", "axe",             MVD_ADDCLOCK_NEVER, false },
	{SG_INFO,   "sg",   IT_SHOTGUN,          "sg",          0,                  0, 0xDA, 0xDA, 0xDA, NULL,          "&cf0f", "sg",              MVD_ADDCLOCK_NEVER, false },
	{SSG_INFO,  "ssg",  IT_SUPER_SHOTGUN,    "&cf0fssg&r",  0,                  0, 0xDA, 0xDA, 0xDA, &tp_name_ssg,  "&cf0f", "&cf0fssg pack&r", MVD_ADDCLOCK_NEVER, false },
	{NG_INFO,   "ng",   IT_NAILGUN,          "&cf0fng&r",   0,                  0, 0xDA, 0xDA, 0xDA, &tp_name_ng,   "&cf0f", "&cf0fng pack&r",  MVD_ADDCLOCK_NEVER, false },
	{SNG_INFO,  "sng",  IT_SUPER_NAILGUN,    "&cf0fsng&r",  0,                  0, 0xDA, 0xDA, 0xDA, &tp_name_sng,  "&cf0f", "&cf0fsng pack&r", MVD_ADDCLOCK_NEVER, false },
	{GL_INFO,   "gl",   IT_GRENADE_LAUNCHER, "&cf0fgl&r",   0,                  0, 0xDA, 0xDA, 0xDA, &tp_name_gl,   "&cf0f", "&cf0fgl pack&r",  MVD_ADDCLOCK_NEVER, false },
	{RL_INFO,   "rl",   IT_ROCKET_LAUNCHER,  "&cf0frl&r",   MOD_ROCKETLAUNCHER, 0, 0xDA, 0xDA, 0xDA, &tp_name_rl,   "&cf0f", "&cf0frl pack&r",  MVD_ADDCLOCK_DMM1,  false },
	{LG_INFO,   "lg",   IT_LIGHTNING,        "&cf0flg&r",   MOD_LIGHTNINGGUN,   0, 0xDA, 0xDA, 0xDA, &tp_name_lg,   "&cf0f", "&cf0flg pack&r",  MVD_ADDCLOCK_DMM1,  false },
	{RING_INFO, "rg",   IT_INVISIBILITY,     "&cff0ring&r", MOD_RING,           0, 0xA6, 0xA6, 0x00, &tp_name_ring, "&cff0", "",                MVD_ADDCLOCK_ALWAYS,  true  },
	{QUAD_INFO, "qd",   IT_QUAD,             "&c00fquad&r", MOD_QUAD,           0, 0x4D, 0x45, 0xC9, &tp_name_quad, "&c00f", "",                MVD_ADDCLOCK_ALWAYS,  true  },
	{PENT_INFO, "pt",   IT_INVULNERABILITY,  "&cf00pent&r", MOD_PENT,           0, 0x91, 0x01, 0x01, &tp_name_pent, "&cf00", "",                MVD_ADDCLOCK_ALWAYS,  true  },
	{GA_INFO,   "ga",   IT_ARMOR1,           "&c0f0ga&r",   MOD_ARMOR,          0, 0x00, 0x72, 0x36, &tp_name_ga,   "&c0f0", "",                MVD_ADDCLOCK_ALWAYS,  false },
	{YA_INFO,   "ya",   IT_ARMOR2,           "&cff0ya&r",   MOD_ARMOR,          1, 0xA6, 0xA6, 0x00, &tp_name_ya,   "&cff0", "",                MVD_ADDCLOCK_ALWAYS,  false },
	{RA_INFO,   "ra",   IT_ARMOR3,           "&cf00ra&r",   MOD_ARMOR,          2, 0x91, 0x01, 0x01, &tp_name_ra,   "&cf00", "",                MVD_ADDCLOCK_ALWAYS,  false },
	{MH_INFO,   "mh",   IT_SUPERHEALTH,      "&c00fmh&r",   MOD_MEGAHEALTH,     0, 0xAD, 0x54, 0x2A, &tp_name_mh,   "&c00f", "",                MVD_ADDCLOCK_ALWAYS,  false },
};
static int item_counts[mvd_info_types];

#define MVDCLOCK_PERSISTENT        1
#define MVDCLOCK_BACKPACK          2
#define MVDCLOCK_BACKPACK_REMOVED  4
#define MVDCLOCK_NEVERSPAWNED      8    // we add baseline ents, so might include quad in duel... use this to hide
#define MVDCLOCK_HIDDEN           16    // don't show in itemslist

#define ITEMSCLOCK_TAKEN_PAUSE     4    // in seconds

typedef struct mvd_clock_t {
	int itemtype;                       // RA, Quad, RL, ...
	double clockval;                    // time when the clock expires
	char location[MAX_MACRO_STRING];    // Player location when the object was picked up
	int flags;                          // flags
	int entity;                         // entity number
	int last_taken_by;                  // player entity item was last taken by (0 for unknown)
	int dropped_by;                     // player entity who dropped it (0 for unknown)
	double last_taken;                  // time when item was last taken
	double old_clockval;                // used to briefly keep items in same order as they're taken
	float hold_clockval;                // time when the player's hold-time will expire
	int order;                          // for static ordering

	struct mvd_clock_t* next;           // next item in the linked list
	struct mvd_clock_t* prev;           // prev item in the linked list
} mvd_clock_t;

// points to the first (earliest) item of the clock list
static mvd_clock_t* mvd_clocklist = NULL;

typedef struct quad_cams_s {
	vec3_t	org;
	vec3_t	angles;
} quad_cams_t;

// Warning: this is a bitmask, pent & quad values are subtracted
typedef enum {
	powerup_cam_inactive = 0,
	powerup_cam_quad_active = 1,
	powerup_cam_pent_active = 2,
	powerup_cam_quadpent_active = 3
} powerup_cam_status_id;

typedef struct cam_id_s {
	quad_cams_t cam;
	char* tag;
	powerup_cam_status_id filter;
} cam_id_t;

quad_cams_t quad_cams[3];
quad_cams_t pent_cams[3];

cam_id_t cam_id[7];

// NEW VERSION

typedef struct runs_s {
	double starttime;
	double endtime;
} runs_t;

typedef struct kill_s {
	int		type;	//0 - kill, 1 - selfkill
	double	time;
	vec3_t	location;
	int		lwf;
} kill_t;

typedef struct death_s {
	double time;
	vec3_t location;
	int id;
} death_t;

typedef struct spawn_s {
	double time;
	vec3_t location;
} spawn_t;

typedef struct mvd_new_info_cg_s {
	double game_starttime;
} mvd_new_info_cg_t; // mvd_new_info_cg;

typedef struct mvd_player_s {
	player_state_t* p_state;
	player_info_t* p_info;
} mvd_player_t;

typedef struct mvd_gameinfo_s {
	double starttime;
	char mapname[1024];
	char team1[1024];
	char team2[1024];
	char hostname[1024];
	int gametype;
	int timelimit;
	int pcount;
	int deathmatch;
} mvd_gameinfo_t;

extern	centity_t		cl_entities[CL_MAX_EDICTS];
extern	entity_t		cl_static_entities[MAX_STATIC_ENTITIES];
double lasttime1, lasttime2;
double lasttime = 0;
double gamestart_time;

double quad_time = 0;
double pent_time = 0;
qbool quad_is_active = false;
qbool pent_is_active = false;
powerup_cam_status_id powerup_cam_status = powerup_cam_inactive;
qbool powerup_cam_active[4];

static qbool was_standby = true;
static int fixed_ordering = 0;


extern cvar_t tp_name_none, tp_weapon_order;
char mvd_info_best_weapon[20];

extern qbool TP_LoadLocFile(char* path, qbool quiet);
extern char* TP_LocationName(vec3_t location);

//matchinfo_t *MT_GetMatchInfo(void);

//char Mention_Win_Buf[80][1024];

mvd_new_info_t mvd_new_info[MAX_CLIENTS];

int mvd_demo_track_run = 0;

// mvd_info cvars
cvar_t			mvd_info = { "mvd_info", "0" };
cvar_t			mvd_info_show_header = { "mvd_info_show_header", "0" };
cvar_t			mvd_info_setup = { "mvd_info_setup", "%p%n \x10%l\x11 %h/%a %w" }; // FIXME: non-ascii chars
cvar_t			mvd_info_x = { "mvd_info_x", "0" };
cvar_t			mvd_info_y = { "mvd_info_y", "0" };

// mvd_stats cvars
cvar_t			mvd_status = { "mvd_status","0" };
cvar_t			mvd_status_x = { "mvd_status_x","0" };
cvar_t			mvd_status_y = { "mvd_status_y","0" };

// Powerup cams
static cvar_t mvd_powerup_cam = { "mvd_powerup_cam", "0" };

static cvar_t mvd_pc_quad_1 = { "mvd_pc_quad_1", "" };
static cvar_t mvd_pc_quad_2 = { "mvd_pc_quad_2", "" };
static cvar_t mvd_pc_quad_3 = { "mvd_pc_quad_3", "" };

static cvar_t mvd_pc_pent_1 = { "mvd_pc_pent_1", "" };
static cvar_t mvd_pc_pent_2 = { "mvd_pc_pent_2", "" };
static cvar_t mvd_pc_pent_3 = { "mvd_pc_pent_3", "" };

static cvar_t powerup_cam_cvars[4] = {
	{ "mvd_pc_view_1", "" }, 
	{ "mvd_pc_view_2", "" },
	{ "mvd_pc_view_3", "" },
	{ "mvd_pc_view_4", "" }
};

cvar_t mvd_moreinfo = { "mvd_moreinfo","0" };
cvar_t mvd_autoadd_items = { "mvd_autoadd_items", "0" };
cvar_t mvd_sortitems = { "mvd_sortitems", "1" };

typedef struct bp_var_s {
	int id;
	int val;
} bp_var_t;

bp_var_t bp_var[MAX_CLIENTS];

char* Make_Red(char* s, int i) {
	static char buf[1024];
	char* p, * ret;
	buf[0] = 0;
	ret = buf;
	for (p = s; *p; p++) {
		if (!strspn(p, "1234567890.") || !(i))
			*ret++ = *p | 128;
		else
			*ret++ = *p;
	}
	*ret = 0;
	return buf;
}

void MVD_Init_Info(int player_slot)
{
	int i;
	int z;

	for (z = 0, i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator == 1)
			continue;
		mvd_new_info[z].id = i;
		if (player_slot == i || player_slot == MAX_CLIENTS) {
			mvd_new_info[z].mvdinfo.initialized = false;
		}
		// memset(mvd_new_info[z].item_info, 0, sizeof(mvd_new_info[z].item_info));
		mvd_new_info[z++].p_info = &cl.players[i];
	}

	strlcpy(mvd_cg_info.mapname, TP_MapName(), sizeof(mvd_cg_info.mapname));
	mvd_cg_info.timelimit = cl.timelimit;

	strlcpy(mvd_cg_info.team1, (z ? mvd_new_info[0].p_info->team : ""), sizeof(mvd_cg_info.team1));
	for (i = 0; i < z; i++) {
		if (strcmp(mvd_new_info[i].p_info->team, mvd_cg_info.team1)) {
			strlcpy(mvd_cg_info.team2, mvd_new_info[i].p_info->team, sizeof(mvd_cg_info.team2));
			break;
		}
	}

	if (z == 2)
		mvd_cg_info.gametype = 0;
	else if (z == 4)
		mvd_cg_info.gametype = 1;
	else if (z == 6)
		mvd_cg_info.gametype = 2;
	else if (z == 8)
		mvd_cg_info.gametype = 3;
	else
		mvd_cg_info.gametype = 4;

	strlcpy(mvd_cg_info.hostname, Info_ValueForKey(cl.serverinfo, "hostname"), sizeof(mvd_cg_info.hostname));
	mvd_cg_info.deathmatch = cl.deathmatch;

	mvd_cg_info.pcount = z;

	for (i = 0; i < mvd_cg_info.pcount; i++) {
		mvd_new_info[i].p_state = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[mvd_new_info[i].id];
	}

	memset(item_counts, 0, sizeof(item_counts));
	for (i = 0; i < sizeof(item_counts) / sizeof(item_counts[0]); ++i) {
		int item_model_hint = mvd_wp_info[i].model_hint;
		int item_skin_number = mvd_wp_info[i].skin_number;

		if (item_model_hint) {
			int j;

			for (j = 0; j < CL_MAX_EDICTS; j++) {
				int modindex = cl_entities[j].baseline.modelindex;
				int skin = cl_entities[j].baseline.skinnum;

				if (modindex >= 0 && modindex < sizeof(cl.model_precache) / sizeof(cl.model_precache[0]) && cl.model_precache[modindex]) {
					if (cl.model_precache[modindex]->modhint == item_model_hint && skin == item_skin_number) {
						++item_counts[i];
					}
				}
			}
		}
	}

	if (player_slot == MAX_CLIENTS) {
		memset(announcer_line_strings, 0, sizeof(announcer_line_strings));
		memset(announcer_line_times, 0, sizeof(announcer_line_times));
		announcer_lines = 0;
	}
}

double MVD_RespawnTimeGet(int itemtype)
{
	switch (itemtype) {
	case RA_INFO:
	case YA_INFO:
	case GA_INFO:
	case MH_INFO:
		return 20.0;

	case SSG_INFO:
	case NG_INFO:
	case SNG_INFO:
	case GL_INFO:
	case RL_INFO:
	case LG_INFO:
		return 30.0;

	case QUAD_INFO:
		return 60.0;

	case RING_INFO:
	case PENT_INFO:
		return 300.0;

	default:
		Com_DPrintf("Warning in MVD_RespawnTimeGet(): unknown item type %d\n", itemtype);
		return 0.0;
	}
}

void MVD_ClockList_Insert(mvd_clock_t* newclock)
{
	Com_DPrintf("MVD_ClockList_Insert type=%d\n", newclock->itemtype);
	if (mvd_clocklist == NULL) {
		mvd_clocklist = newclock;
		newclock->next = NULL;
		newclock->prev = NULL;
	}
	else {
		mvd_clock_t* current = mvd_clocklist;
		mvd_clock_t* last = NULL;
		while (current && current->clockval < newclock->clockval) {
			last = current;
			current = current->next;
		}
		if (current) {
			newclock->next = current;
			newclock->prev = current->prev;
			if (current->prev) {
				current->prev->next = newclock;
			}
			current->prev = newclock;
			if (last == NULL) {
				mvd_clocklist = newclock;
			}
		}
		else {
			if (last) {
				last->next = newclock;
				newclock->prev = last;
				newclock->next = NULL;
			}
			else {
				newclock->prev = NULL;
				newclock->next = mvd_clocklist;
				mvd_clocklist = newclock;
			}
		}
	}
}

mvd_clock_t* MVD_ClockList_Remove(mvd_clock_t* item)
{
	mvd_clock_t* ret = NULL;

	if (item == mvd_clocklist) {
		mvd_clocklist = item->next;
		if (mvd_clocklist) {
			mvd_clocklist->prev = NULL;
		}
		Q_free(item);
		return mvd_clocklist;
	}

	ret = item->next;
	// item->prev is not null
	if (item->next) {
		item->next->prev = item->prev;
		item->prev->next = item->next;
	}
	else {
		item->prev->next = NULL;
	}
	Q_free(item);
	return ret;
}

static void MVD_ClockStart(int itemtype, vec3_t origin)
{
	mvd_clock_t* newclock = (mvd_clock_t*)Q_malloc(sizeof(mvd_clock_t));
	newclock->clockval = cls.demopackettime + MVD_RespawnTimeGet(itemtype);
	newclock->itemtype = itemtype;
	if (origin) {
		strlcpy(newclock->location, TP_LocationName(origin), sizeof(newclock->location));
	}
	newclock->order = 0;
	MVD_ClockList_Insert(newclock);
}

static mvd_clock_t* MVD_ClockStartEntity(int entity, int itemtype, int flags)
{
	mvd_clock_t* newclock = (mvd_clock_t*)Q_malloc(sizeof(mvd_clock_t));
	newclock->clockval = 0;
	newclock->itemtype = itemtype;
	strlcpy(newclock->location, TP_LocationName(cl_entities[entity].baseline.origin), sizeof(newclock->location));
	newclock->flags = flags;
	newclock->entity = entity;
	newclock->order = ++fixed_ordering;
	MVD_ClockList_Insert(newclock);
	return newclock;
}

static mvd_clock_t* MVD_ClockFindEntity(int entity)
{
	mvd_clock_t* current;

	for (current = mvd_clocklist; current; current = current->next) {
		if (current->entity == entity) {
			return current;
		}
	}

	return NULL;
}

void MVD_ClockList_RemoveExpired(void)
{
	mvd_clock_t* current;

	for (current = mvd_clocklist; current; ) {
		// We don't remove persistent counters
		if (!(current->flags & MVDCLOCK_PERSISTENT)) {
			// Expired
			if (current->clockval + 1 < cls.demopackettime) {
				current = MVD_ClockList_Remove(current);
				continue;
			}

			//
			if (current->entity && !(current->flags & MVDCLOCK_BACKPACK_REMOVED)) {
				int mod = cl_entities[current->entity].current.modelindex;
				if (mod <= 0 || mod >= sizeof(cl.model_precache) / sizeof(cl.model_precache[0])) {
					if (current->last_taken) {
						// Backpack has been picked up, disconnect from entity
						current->flags &= ~(MVDCLOCK_BACKPACK);
						current->flags |= MVDCLOCK_BACKPACK_REMOVED;
						current->entity = 0;
					}
					else {
						current = MVD_ClockList_Remove(current);
					}
					continue;
				}

				if (cl_entities[current->entity].sequence < cl.validsequence) {
					// Backpack has been picked up or disappeared, disconnect from entity and let expire
					current->flags &= ~(MVDCLOCK_BACKPACK);
					current->flags |= MVDCLOCK_BACKPACK_REMOVED;
					current->entity = 0;
				}
				else if (cl.model_precache[mod] == NULL || cl.model_precache[mod]->modhint != MOD_BACKPACK) {
					// No longer a backpack, remove
					current = MVD_ClockList_Remove(current);
					continue;
				}
			}
		}

		current = current->next;
	}
}

int MVD_ClockList_GetLongestName(void)
{
	int i, current, longest = 0;
	int items[] = {
		IT_AXE, IT_SHOTGUN, IT_SUPER_SHOTGUN, IT_NAILGUN, IT_SUPER_NAILGUN,
		IT_GRENADE_LAUNCHER, IT_ROCKET_LAUNCHER, IT_LIGHTNING, IT_INVISIBILITY,
		IT_QUAD, IT_INVULNERABILITY, IT_ARMOR1, IT_ARMOR2, IT_ARMOR3, IT_SUPERHEALTH
	};

	for (i = 0; i < (sizeof(items) / sizeof(*items)); i++) {
		current = strlen_color(TP_ItemName(items[i]));
		if (longest < current)
			longest = current;
	}
	return longest;
}

static double MVD_ClockList_SortTime(mvd_clock_t* c)
{
	double t = c->clockval;

	if (c->last_taken && cls.demopackettime - c->last_taken < ITEMSCLOCK_TAKEN_PAUSE) {
		// item has just been taken, keep to the start of the list
		t = -1 - c->old_clockval;
	}
	else if (c->last_taken && t == -1) {
		// megahealth still held by player, put to end of list
		t = cls.demopackettime + c->last_taken + 1000;
	}

	return t;
}

// Moves persistent
static int MVD_ClockList_Compare(mvd_clock_t* c, mvd_clock_t* n)
{
	double c_time = MVD_ClockList_SortTime(c);
	double n_time = MVD_ClockList_SortTime(n);
	qbool c_persistent = c->flags & MVDCLOCK_PERSISTENT;
	qbool n_persistent = n->flags & MVDCLOCK_PERSISTENT;

	if (c_persistent && !n_persistent) {
		return 1;
	}
	if (!c_persistent && n_persistent) {
		return -1;
	}

	if (mvd_sortitems.integer) {
		return c_time - n_time;
	}
	else {
		return c->order - n->order;
	}
}

static void MVD_ClockList_Sort(void)
{
	qbool any_change = true;

	while (any_change) {
		mvd_clock_t* c = mvd_clocklist;

		any_change = false;
		while (c) {
			mvd_clock_t* n = c->next;
			int comparison;

			if (!n) {
				break;
			}

			comparison = MVD_ClockList_Compare(c, n);
			if (comparison > 0) {
				c->next = n->next;
				n->prev = c->prev;
				c->prev = n;
				n->next = c;

				if (n->prev) {
					n->prev->next = n;
				}
				if (c->next) {
					c->next->prev = c;
				}

				if (c == mvd_clocklist) {
					mvd_clocklist = n;
				}

				any_change = true;
			}
			else {
				c = n;
			}
		}
	}
}

// MEAG: Deliberately not taking 'proportional' argument into account here, don't think we should measure by item type either...
void MVD_ClockList_TopItems_DimensionsGet(double time_limit, int style, int* width, int* height, float scale, qbool backpacks, qbool proportional)
{
	int lines = 0;
	mvd_clock_t* current = mvd_clocklist;
	int persistent = 0, temporary = 0;

	if (style == 5) {
		MVD_ClockList_Sort();

		current = mvd_clocklist;
	}

	while (current) {
		int time = (int)((current->clockval - cls.demopackettime) + 1);

		// Skip if it's a backpack and the config turns these off
		if (!backpacks && (current->flags & (MVDCLOCK_BACKPACK | MVDCLOCK_BACKPACK_REMOVED))) {
			current = current->next;
			continue;
		}

		if (current->flags & MVDCLOCK_PERSISTENT) {
			// Skip if player has manually removed
			if (current->flags & MVDCLOCK_HIDDEN) {
				current = current->next;
				continue;
			}

			if (current->flags & MVDCLOCK_NEVERSPAWNED) {
				// Skip auto-added items that haven't spawned and have never spawned
				if (cl_entities[current->entity].current.modelindex == 0 && time <= 0) {
					current = current->next;
					continue;
				}

				current->flags &= ~(MVDCLOCK_NEVERSPAWNED);
			}

		}

		if (current->entity || current->clockval - cls.demopackettime < time_limit) {
			int time = (int)((current->clockval - cls.demopackettime) + 1);

			if (current->flags & MVDCLOCK_PERSISTENT) {
				if (cl_entities[current->entity].current.modelindex != 0 || time >= 0) {
					++persistent;
				}
			}
			else {
				++temporary;
			}
			lines++;
		}
		current = current->next;
	}

	// the longest possible string
	if (style == 1) {
		*width = LETTERWIDTH * (MVD_ClockList_GetLongestName() + sizeof(" spawn") - 1) * scale;
	}
	else if (style == 3) {
		*width = LETTERWIDTH * (2 + sizeof(" spawn") - 1) * scale;
	}
	else if (style == 4) {
		*width = LETTERWIDTH * (2 + sizeof(" spawn") - 1) * scale;
	}
	else {
		*width = LETTERWIDTH * (sizeof("QUAD spawn") - 1) * scale;
	}

	if (persistent && temporary) {
		lines++;
	}

	*height = LETTERHEIGHT * lines * scale * (style == 3 ? 2 : 1);
}

static qbool MVD_ClockIsHeld(mvd_clock_t* current, qbool test_held, float* alpha)
{
	double time_since;

	if (alpha) {
		*alpha = 1.0f;
	}

	if (!current->entity || (test_held && current->hold_clockval <= cls.demopackettime)) {
		return false;
	}

	if (current->last_taken_by <= 0 || current->last_taken_by > MAX_CLIENTS || !cl.players[current->last_taken_by - 1].name[0]) {
		return false;
	}

	if (test_held && (cl.players[current->last_taken_by - 1].stats[STAT_ITEMS] & mvd_wp_info[current->itemtype].it)) {
		return true;
	}

	if (!current->last_taken || current->last_taken > cls.demopackettime) {
		return false;
	}

	time_since = (cls.demopackettime - current->last_taken);
	if (alpha) {
		if (time_since > ITEMSCLOCK_TAKEN_PAUSE - 1 && time_since < ITEMSCLOCK_TAKEN_PAUSE) {
			*alpha = (ITEMSCLOCK_TAKEN_PAUSE - time_since);
		}
		else if (time_since >= ITEMSCLOCK_TAKEN_PAUSE && time_since < ITEMSCLOCK_TAKEN_PAUSE + 1) {
			*alpha = (time_since - ITEMSCLOCK_TAKEN_PAUSE);
		}
	}
	return time_since < ITEMSCLOCK_TAKEN_PAUSE;
}

void MVD_ClockList_TopItems_Draw(double time_limit, int style, int x, int y, float scale, int filter, qbool backpacks, qbool proportional)
{
	mvd_clock_t* current = mvd_clocklist;
	char clockitem[128];
	char temp[128];
	int base_x = x;
	qbool was_persistent = true;
	int barWidth;

	while (current) {
		float alpha = 1.0f;
		x = base_x;

		// Skip backpacks
		if (!backpacks && (current->flags & (MVDCLOCK_BACKPACK | MVDCLOCK_BACKPACK_REMOVED))) {
			current = current->next;
			continue;
		}

		// Skip auto-added items that have never spawned or manually removed
		if (current->flags & (MVDCLOCK_NEVERSPAWNED | MVDCLOCK_HIDDEN)) {
			current = current->next;
			continue;
		}

		if (current->entity || current->clockval - cls.demopackettime < time_limit) {
			int time = (int)((current->clockval - cls.demopackettime) + 1);
			mpic_t* texture = Mod_SimpleTextureForHint(mvd_wp_info[current->itemtype].model_hint, mvd_wp_info[current->itemtype].skin_number);

			if (filter & mvd_wp_info[current->itemtype].it) {
				current = current->next;
				continue;
			}

			if ((current->flags & MVDCLOCK_PERSISTENT) && !was_persistent) {
				y += LETTERHEIGHT * scale;
			}
			was_persistent = (current->flags & MVDCLOCK_PERSISTENT);

			if (style == 1) {
				// tp_name_*
				strlcpy(clockitem, TP_ItemName(mvd_wp_info[current->itemtype].it), sizeof(clockitem));
			}
			else if (style == 2) {
				// brown + white
				strlcpy(clockitem, mvd_wp_info[current->itemtype].name, sizeof(clockitem));
				CharsToBrown(clockitem, clockitem + strlen(clockitem));
			}
			else if (style == 3 && texture && R_TextureReferenceIsValid(texture->texnum)) {
				// simpleitem
				Draw_FitPic(x, y, 2 * LETTERWIDTH * scale, 2 * LETTERHEIGHT * scale, texture);
				x += 2 * LETTERWIDTH * scale;
				clockitem[0] = '\0';
				y += LETTERHEIGHT * scale / 2;
			}
			else if (style == 4) {
				char item_name[MAX_MACRO_STRING];

				if (mvd_wp_info[current->itemtype].name_cvar && mvd_wp_info[current->itemtype].name_cvar->string[0]) {
					Util_SkipChars(mvd_wp_info[current->itemtype].name_cvar->string, "{}", item_name, sizeof(item_name));
				}
				else {
					strlcpy(item_name, mvd_wp_info[current->itemtype].colored_name, sizeof(item_name));
				}

				snprintf(clockitem, sizeof(clockitem), "%*s", MVD_ANNOUNCER_ITEM_LENGTH + ((int)strlen(item_name) - strlen_color(item_name)), item_name);

				strlcat(clockitem, " \034", sizeof(clockitem));
			}
			else if (style == 5) {
				char item_name[MAX_MACRO_STRING];

				if (current->flags & (MVDCLOCK_BACKPACK | MVDCLOCK_BACKPACK_REMOVED)) {
					const char* name = mvd_wp_info[current->itemtype].colored_packname;

					snprintf(item_name, sizeof(item_name) - 1, "%*s", MVD_ANNOUNCER_ITEM_LENGTH + ((int)strlen(name) - strlen_color(name)), name);
				}
				else {
					const char* color = mvd_wp_info[current->itemtype].color_string;

					snprintf(item_name, sizeof(item_name) - 1, "%s%*s&r", color ? color : "", MVD_ANNOUNCER_ITEM_LENGTH + ((int)strlen(current->location) - strlen_color(current->location)), current->location);
				}

				strlcpy(clockitem, item_name, sizeof(clockitem));
				strlcat(clockitem, " \034", sizeof(clockitem));
				Draw_SString(x, y, clockitem, scale, proportional);

				x += strlen_color(clockitem) * 8 * scale;
				clockitem[0] = '\0';
			}
			else if (style == 6) {
				// progress bar countdown
				strlcpy(clockitem, mvd_wp_info[current->itemtype].name, sizeof(clockitem));

				barWidth = (round(67 * scale) / time_limit) * (time_limit - time + 1);
				if (time == 0) {
					barWidth = (round(67 * scale) / time_limit) * time_limit;
				}

				Draw_AlphaFillRGB(x - 1, y, barWidth, 10 * scale,
					RGBA_TO_COLOR(mvd_wp_info[current->itemtype].Rcolor,
						mvd_wp_info[current->itemtype].Gcolor,
						mvd_wp_info[current->itemtype].Bcolor, 128));
			}
			else if (style == 7) {
				// progress bar countdown, but itemname not in bar
				strlcpy(clockitem, mvd_wp_info[current->itemtype].name, sizeof(clockitem));

				barWidth = (round(44 * scale) / time_limit) * (time_limit - time + 1);
				if (time == 0) {
					barWidth = (round(44 * scale) / time_limit) * time_limit;
				}

				Draw_AlphaFillRGB(x + (22 * scale), y, barWidth, 10 * scale,
					RGBA_TO_COLOR(mvd_wp_info[current->itemtype].Rcolor,
						mvd_wp_info[current->itemtype].Gcolor,
						mvd_wp_info[current->itemtype].Bcolor, 128));
			}
			else {
				// built-in color(GL) or simple white (software)
				if (mvd_wp_info[current->itemtype].name_cvar) {
					strlcpy(clockitem, mvd_wp_info[current->itemtype].colored_name, sizeof(clockitem));
				}
				else {
					strlcpy(clockitem, mvd_wp_info[current->itemtype].colored_name, sizeof(clockitem));
				}
			}

			if (current->flags & MVDCLOCK_BACKPACK_REMOVED) {
				if (current->location[0]) {
					strlcat(clockitem, " ", sizeof(clockitem));
					strlcat(clockitem, current->location, sizeof(clockitem));
				}

				if (current->dropped_by && current->last_taken_by) {
					strlcat(clockitem, " (", sizeof(clockitem));
					strlcat(clockitem, MVD_AnnouncerTeamPlayerName(&cl.players[current->dropped_by - 1]), sizeof(clockitem));
					strlcat(clockitem, " \015 ", sizeof(clockitem));
					strlcat(clockitem, MVD_AnnouncerTeamPlayerName(&cl.players[current->last_taken_by - 1]), sizeof(clockitem));
					strlcat(clockitem, ")", sizeof(clockitem));
				}
				else {
					current = MVD_ClockList_Remove(current);
					continue;
				}
			}
			else if (time > 0) {
				if (current->flags & MVDCLOCK_BACKPACK) {
					strlcpy(current->location, TP_LocationName(cl_entities[current->entity].current.origin), sizeof(current->location));

					snprintf(temp, sizeof(temp), " %s", current->location);

					if (current->dropped_by) {
						strlcat(temp, " (", sizeof(temp));
						strlcat(temp, MVD_AnnouncerTeamPlayerName(&cl.players[current->dropped_by - 1]), sizeof(temp));
						strlcat(temp, ")", sizeof(temp));
					}
				}
				else if (MVD_ClockIsHeld(current, mvd_wp_info[current->itemtype].show_held, &alpha)) {
					snprintf(temp, sizeof(temp), " %s", MVD_AnnouncerTeamPlayerName(&cl.players[current->last_taken_by - 1]));
				}
				else if (style == 5) {
					if (time >= 60) {
						snprintf(temp, sizeof(temp), " %d:%02d", time / 60, time % 60);
					}
					else {
						snprintf(temp, sizeof(temp), " %d", time);
					}
				}
				else {
					snprintf(temp, sizeof(temp), " %*d", time_limit >= 10 ? 2 : 1, time);
				}
				strlcat(clockitem, temp, sizeof(clockitem));
			}
			else if (current->flags & MVDCLOCK_PERSISTENT) {
				strlcpy(temp, " up", sizeof(temp));

				// If a player is holding the item, use their name instead
				if (MVD_ClockIsHeld(current, true, NULL)) {
					snprintf(temp, sizeof(temp), " %s", MVD_AnnouncerTeamPlayerName(&cl.players[current->last_taken_by - 1]));
				}

				strlcat(clockitem, temp, sizeof(clockitem));
			}
			else {
				strlcat(clockitem, " spawn", sizeof(clockitem));
			}

			if (style == 4 && item_counts[current->itemtype] != 1 && current->location[0]) {
				strlcat(clockitem, " \020", sizeof(clockitem));
				strlcat(clockitem, current->location, sizeof(clockitem));
				strlcat(clockitem, "\021", sizeof(clockitem));
			}

			Draw_SStringAlpha(x, y, clockitem, scale, alpha, proportional);

			y += LETTERHEIGHT * scale;
			if (style == 3) {
				y += LETTERHEIGHT * scale / 2;
			}
			else if ((style == 6) || (style == 7)) {
				y += round(4 * scale);
			}
		}
		current = current->next;
	}
}

static void MVD_Took(int player, int item, qbool addclock)
{
	if (mvd_new_info[player].mvdinfo.initialized && !cl.mvd_ktx_markers) {
		if (addclock) {
			MVD_ClockStart(item, mvd_new_info[player].p_state ? mvd_new_info[player].p_state->origin : NULL);
		}
		mvd_new_info[player].mvdinfo.itemstats[item].mention = addclock ? MENTION_PICKED_UP_ITEM : MENTION_PICKED_UP_PACK;
	}
}

// this steps in action if the user has created a demo playlist and has specified
// which player should be prefered in the demos (so that he doesn't have to switch
// to that player at the start of each demo manually)
void MVD_Demo_Track(void) {
	extern char track_name[16];
	extern cvar_t demo_playlist_track_name;
	char track_player[128] = { 0 };

#ifdef DEBUG
	printf("MVD_Demo_Track Started\n");
#endif


	if (strlen(track_name))
	{
		if (FindBestNick(track_name, FBN_IGNORE_SPECS | FBN_IGNORE_QTVSPECS, track_player, sizeof(track_player)))
			Cbuf_AddText(va("track \"%s\"\n", track_player));
	}
	else if (strlen(demo_playlist_track_name.string))
	{
		if (FindBestNick(demo_playlist_track_name.string, FBN_IGNORE_SPECS | FBN_IGNORE_QTVSPECS, track_player, sizeof(track_player)))
			Cbuf_AddText(va("track \"%s\"\n", track_player));
	}

	mvd_demo_track_run = 1;
#ifdef DEBUG
	printf("MVD_Demo_Track Stopped\n");
#endif
}


int MVD_BestWeapon(int i) {
	int x;
	char* t[] = { tp_weapon_order.string, "78654321", NULL }, ** s;
	for (s = t; *s; s++) {
		for (x = 0; x < strlen(*s); x++) {
			switch ((*s)[x]) {
			case '1': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_AXE) return IT_AXE; break;
			case '2': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SHOTGUN) return IT_SHOTGUN; break;
			case '3': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
			case '4': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_NAILGUN) return IT_NAILGUN; break;
			case '5': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
			case '6': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
			case '7': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
			case '8': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_LIGHTNING) return IT_LIGHTNING; break;
			}
		}
	}
	return 0;
}

char* MVD_BestWeapon_strings(int i) {
	return TP_ItemName(MVD_BestWeapon(i));
}

int MVD_Weapon_LWF(int i) {
	switch (i) {
	case IT_AXE: return 0;
	case IT_SHOTGUN: return 1;
	case IT_SUPER_SHOTGUN: return 2;
	case IT_NAILGUN: return 3;
	case IT_SUPER_NAILGUN: return 4;
	case IT_GRENADE_LAUNCHER: return 5;
	case IT_ROCKET_LAUNCHER: return 6;
	case IT_LIGHTNING: return 7;
	default: return 666;
	}
}

char* MVD_BestAmmo(int i) {

	switch (MVD_BestWeapon(i)) {
	case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
		return va("%i", mvd_new_info[i].p_info->stats[STAT_SHELLS]);

	case IT_NAILGUN: case IT_SUPER_NAILGUN:
		return va("%i", mvd_new_info[i].p_info->stats[STAT_NAILS]);

	case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
		return va("%i", mvd_new_info[i].p_info->stats[STAT_ROCKETS]);

	case IT_LIGHTNING:
		return va("%i", mvd_new_info[i].p_info->stats[STAT_CELLS]);

	default: return "0";
	}
}

void MVD_Info(void) {
	char str[1024];
	char mvd_info_final_string[1024], mvd_info_powerups[20], mvd_info_header_string[1024];
	int x, y, z, i;



#ifdef DEBUG
	printf("MVD_Info Started\n");
#endif

	z = 1;

	if (!mvd_info.value)
		return;

	if (!cls.mvdplayback)
		return;

	x = ELEMENT_X_COORD(mvd_info);
	y = ELEMENT_Y_COORD(mvd_info);

	if (mvd_info_show_header.value) {
		strlcpy(mvd_info_header_string, mvd_info_setup.string, sizeof(mvd_info_header_string));
		Replace_In_String(mvd_info_header_string, sizeof(mvd_info_header_string), '%', \
			10, \
			"a", "Armor", \
			"f", "Frags", \
			"h", "Health", \
			"l", "Location", \
			"n", "Nick", \
			"P", "Ping", \
			"p", "Powerup", \
			"v", "Value", \
			"w", "Cur.Weap.", \
			"W", "Best Weap.");
		strlcpy(mvd_info_header_string, Make_Red(mvd_info_header_string, 0), sizeof(mvd_info_header_string));
		Draw_String(x, y + ((z++) * 8), mvd_info_header_string);
	}

	for (i = 0; i < mvd_cg_info.pcount; i++) {

		mvd_info_powerups[0] = 0;
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_QUAD)
			//strlcpy(mvd_info_powerups, tp_name_quad.string, sizeof(mvd_info_powerups));

			if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
				//if (mvd_info_powerups[0])
				//	strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
				//strlcat(mvd_info_powerups, tp_name_pent.string, sizeof(mvd_info_powerups));
			}

		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
			//if (mvd_info_powerups[0])
			//	strlcat(mvd_info_powerups, tp_name_separator.string, sizeof(mvd_info_powerups));
			//strlcat(mvd_info_powerups, tp_name_ring.string, sizeof(mvd_info_powerups));
		}

		strlcpy(mvd_info_final_string, mvd_info_setup.string, sizeof(mvd_info_final_string));
		Replace_In_String(mvd_info_final_string, sizeof(mvd_info_final_string), '%', \
			10, \
			"w", va("%s:%i", TP_ItemName(mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON]), mvd_new_info[i].p_info->stats[STAT_AMMO]), \
			"W", va("%s:%s", MVD_BestWeapon_strings(i), MVD_BestAmmo(i)), \
			"a", va("%i", mvd_new_info[i].p_info->stats[STAT_ARMOR]), \
			"f", va("%i", mvd_new_info[i].p_info->frags), \
			"h", va("%i", mvd_new_info[i].p_info->stats[STAT_HEALTH]), \
			"l", TP_LocationName(mvd_new_info[i].p_state->origin), \
			"n", mvd_new_info[i].p_info->name, \
			"P", va("%i", mvd_new_info[i].p_info->ping), \
			"p", mvd_info_powerups, \
			"v", va("%f", mvd_new_info[i].value));
		strlcpy(str, mvd_info_final_string, sizeof(str));
		Draw_String(x, y + ((z++) * 8), str);

#ifdef DEBUG
		printf("MVD_Info Stopped\n");
#endif
	}
}

static void MVD_AddString(const char* line)
{
	VX_TrackerPickupText(line);

	if (announcer_lines == MAX_ANNOUNCER_LINES) {
		memmove(&announcer_line_strings[0], &announcer_line_strings[1], sizeof(announcer_line_strings[0]) * (MAX_ANNOUNCER_LINES - 1));
		memmove(&announcer_line_times[0], &announcer_line_times[1], sizeof(announcer_line_times[0]) * (MAX_ANNOUNCER_LINES - 1));
		strlcpy(announcer_line_strings[announcer_lines - 1], line, sizeof(announcer_line_strings[announcer_lines - 1]));
		announcer_line_times[announcer_lines - 1] = cl.time;
	}
	else {
		strlcpy(announcer_line_strings[announcer_lines], line, sizeof(announcer_line_strings[announcer_lines]));
		announcer_line_times[announcer_lines] = cl.time;
		++announcer_lines;
	}
}

const char* MVD_AnnouncerString(int line, int total, float* alpha)
{
	int index = announcer_lines - total + line;
	if (index >= 0 && index < announcer_lines && announcer_line_times[index] > cl.time - MAX_ANNOUNCER_LINE_TIME) {
		if (cl.time - announcer_line_times[index] < MAX_ANNOUNCER_LINE_TIME_FADE) {
			*alpha = 1;
		}
		else {
			*alpha = (1 - (cl.time - announcer_line_times[index] - MAX_ANNOUNCER_LINE_TIME_FADE) / (MAX_ANNOUNCER_LINE_TIME - MAX_ANNOUNCER_LINE_TIME_FADE));
		}
		return announcer_line_strings[index];
	}
	else {
		*alpha = 0;
		return "";
	}
}

static const char* MVD_AnnouncerTeamPlayerName(player_info_t* info)
{
	if (cl.teamplay) {
		int player_color = Sbar_BottomColor(info);

		byte red = host_basepal[player_color * 3];
		byte green = host_basepal[player_color * 3 + 1];
		byte blue = host_basepal[player_color * 3 + 2];
		char* buf;

		if (info->bottomcolor == 3) {
			red = 99;
			green = 255;
			blue = 120;
		}
		else if (info->bottomcolor == 12) {
			red = green = 255;
			blue = 0;
		}

		buf = va("&c%x%x%x%s&r", (unsigned int)(red / 16), (unsigned int)(green / 16), (unsigned int)(blue / 16), info->name);
		CharsToWhite(buf, buf + strlen(buf));
		return buf;
	}
	else {
		return info->name;
	}
}

static const char* MVD_AnnouncerPlayerName(int i)
{
	return MVD_AnnouncerTeamPlayerName(mvd_new_info[i].p_info);
}

void MVD_Status_Announcer(int i, int z) {
	//char *pn = mvd_new_info[i].p_info->name;
	vec3_t* pl = &mvd_new_info[i].p_state->origin;

	if (cl.mvd_ktx_markers) {
		return;
	}

	if (mvd_new_info[i].mvdinfo.itemstats[z].mention > 0) {
		int mention = mvd_new_info[i].mvdinfo.itemstats[z].mention;

		mvd_new_info[i].mvdinfo.itemstats[z].mention = 0;

		if (!mvd_moreinfo.integer) {
			return;
		}

		if (z >= 2 && z < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]) && z != GA_INFO && z != GL_INFO) {
			char location_name[MAX_MACRO_STRING];
			char item_name[MAX_MACRO_STRING];
			char simple_item_name[MAX_MACRO_STRING];

			if (mvd_wp_info[z].name_cvar && mvd_wp_info[z].name_cvar->string && mvd_wp_info[z].name_cvar->string[0]) {
				Util_SkipChars(mvd_wp_info[z].name_cvar->string, "{}", item_name, sizeof(item_name));
			}
			else {
				strlcpy(item_name, mvd_wp_info[z].colored_name, sizeof(item_name));
			}
			Util_SkipEZColors(simple_item_name, item_name, sizeof(simple_item_name));
			strlcpy(location_name, TP_ParseFunChars(TP_LocationName(*pl), false), sizeof(location_name));
			if (mention == MENTION_PICKED_UP_ITEM) {
				qbool names_match = !strcmp(location_name, simple_item_name);

				if (item_counts[z] == 1 || names_match) {
					MVD_AddString(va("%*s \015 %s\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, MVD_AnnouncerPlayerName(i)));
				}
				else {
					MVD_AddString(va("%*s \015 %s \020%s\021\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, MVD_AnnouncerPlayerName(i), location_name));
				}
			}
			else if (mention == MENTION_PICKED_UP_PACK) {
				if (tp_name_backpack.string[0]) {
					char pack_name[MAX_MACRO_STRING];

					Util_SkipChars(tp_name_backpack.string, "{}", pack_name, sizeof(pack_name));

					strlcat(item_name, " ", sizeof(item_name));
					strlcat(item_name, pack_name, sizeof(item_name));
				}
				else {
					strlcat(item_name, " pack", sizeof(item_name));
				}

				MVD_AddString(va("%*s \015 %s \020%s\021\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, MVD_AnnouncerPlayerName(i), location_name));
			}
		}
	}
	else if (mvd_new_info[i].mvdinfo.itemstats[z].mention < 0) {
		int mention = mvd_new_info[i].mvdinfo.itemstats[z].mention;

		mvd_new_info[i].mvdinfo.itemstats[z].mention = 0;

		if (!mvd_moreinfo.integer) {
			return;
		}

		if (z >= 5 && z < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0])) {
			char location_name[MAX_MACRO_STRING];
			char item_name[MAX_MACRO_STRING];
			const char* pn = MVD_AnnouncerPlayerName(i);

			if (mvd_wp_info[z].name_cvar && mvd_wp_info[z].name_cvar->string && mvd_wp_info[z].name_cvar->string[0]) {
				Util_SkipChars(mvd_wp_info[z].name_cvar->string, "{}", item_name, sizeof(item_name));
			}
			else {
				strlcpy(item_name, mvd_wp_info[z].colored_name, sizeof(item_name));
			}
			strlcpy(location_name, TP_ParseFunChars(TP_LocationName(*pl), false), sizeof(location_name));
			if (mention == MENTION_DIED_WITH_ITEM) {
				// Weapon tracking should probably be added back in
				if (cl.deathmatch == 1 && (z == RING_INFO || z == QUAD_INFO)) {
					strlcat(item_name, " &cf00dead&r", sizeof(item_name));

					MVD_AddString(va("%*s   (%s)\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, pn));
				}
			}
			else if (mention == MENTION_ITEM_RAN_OUT) {
				strlcat(item_name, " over", sizeof(item_name));

				MVD_AddString(va("%*s   (%s)\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, pn));
			}
			else if (mention == MENTION_DROPPED_ITEM) {
				if (cl.deathmatch == 1) {
					strlcat(item_name, " &cf00drop&r", sizeof(item_name));

					MVD_AddString(va("%*s \015 %s \020%s\021\n", MVD_ANNOUNCER_ITEM_LENGTH + (strlen(item_name) - strlen_color(item_name)), item_name, MVD_AnnouncerPlayerName(i), location_name));
				}
			}
		}
	}
}

void MVD_Status_WP(int i, int* taken) {
	int j, k;
	for (k = j = SSG_INFO; j <= LG_INFO; j++, k = k * 2) {
		if (!mvd_new_info[i].mvdinfo.itemstats[j].has && mvd_new_info[i].p_info->stats[STAT_ITEMS] & k) {
			if (j >= GL_INFO && cl.deathmatch == 1) {
				*taken |= (1 << j);
			}
			mvd_new_info[i].mvdinfo.itemstats[j].has = 1;
			mvd_new_info[i].mvdinfo.itemstats[j].count++;
		}
	}

}

void MVD_Stats_Cleanup(void)
{
	quad_is_active = false;
	pent_is_active = false;
	powerup_cam_status = powerup_cam_inactive;
	memset(powerup_cam_active, 0, sizeof(powerup_cam_active));
	pent_time = quad_time = 0;
	was_standby = true;
	while (mvd_clocklist) {
		MVD_ClockList_Remove(mvd_clocklist);
	}

	memset(mvd_new_info, 0, sizeof(mvd_new_info));
	memset(&mvd_cg_info, 0, sizeof(mvd_cg_info));
	fixed_ordering = 0;
}

void MVD_Set_Armor_Stats(int z, int i) {
	switch (z) {
	case GA_INFO:
		mvd_new_info[i].mvdinfo.itemstats[YA_INFO].has = 0;
		mvd_new_info[i].mvdinfo.itemstats[RA_INFO].has = 0;
		break;
	case YA_INFO:
		mvd_new_info[i].mvdinfo.itemstats[GA_INFO].has = 0;
		mvd_new_info[i].mvdinfo.itemstats[RA_INFO].has = 0;
		break;
	case RA_INFO:
		mvd_new_info[i].mvdinfo.itemstats[GA_INFO].has = 0;
		mvd_new_info[i].mvdinfo.itemstats[YA_INFO].has = 0;
		break;

	}
}

// calculates the average values of run statistics
void MVD_Stats_CalcAvgRuns(void)
{
	int i;
	static double lastupdate = 0;

	// no need to recalculate the values in every frame
	if (cls.demopackettime - lastupdate < 0.5) {
		return;
	}
	else {
		lastupdate = cls.demopackettime;
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		mvd_info_t* pi = &mvd_new_info[i].mvdinfo;
		//		int r = mvd_new_info[i].mvdinfo.run;
		int tf, ttf, tt;
		int j;
		tf = ttf = tt = 0;

		for (j = 0; j < pi->run; j++) {
			tf += pi->runs[j].frags;
			ttf += pi->runs[j].teamfrags;
			tt += pi->runs[j].time;
		}

		if (pi->run) {
			pi->run_stats.all.avg_frags = tf / (double)pi->run;
			pi->run_stats.all.avg_teamfrags = ttf / (double)pi->run;
			pi->run_stats.all.avg_time = tt / (double)pi->run;
		}
	}
}

static qbool MVD_Weapon_From_Backpack(int weapon, int taken, int* ammotaken)
{
	// Things that signalize backpack took:
	// a) some taken ammo, different than weapon pickup gives, or
	// b) more than one weapon was taken

	// From the info we have the process is undeterministic.
	// The current semantics is "paranoid":
	// When not sure, expect it was a backpack pickup.
	// Howver, it is still impossible to distinguish some backpack pickups.

	// Why?
	// For example when player has sg and 199 shells and his status
	// changes to ssg + 200 shells, it's impossible to tell whether
	// he picked up ssg from pack or regular ssg spawn.
	// Or when he has 0 rockets and then picks rl pack with 5 rockets,
	// it looks like he picked regular rl.
	// Also there's the thing which is dependent on deathmatch setting when you
	// get extra rockets that even weren't in the rl pack originally
	// but you get them anyway if you have no rox.

	// Also worth mentioning this does not reflect custom mods
	// like tf, ctf and others...

	// Possible approach would be to check player coordinates
	// and check whether on given bsp some of the spawnpoints
	// of given item is close enough to the player.

	// Another possible approach would be to extend the protocol with
	// proper pickup info delivery.

	int i;

	if (weapon == SSG_INFO) {
		if (ammotaken[0] != 5 || ammotaken[1] > 0 || ammotaken[2] > 0 || ammotaken[3] > 0) {
			return true;
		}
	}
	if (weapon == NG_INFO) {
		if (ammotaken[0] > 0 || ammotaken[1] != 30 || ammotaken[2] > 0 || ammotaken[3] > 0) {
			return true;
		}
	}
	if (weapon == SNG_INFO) {
		if (ammotaken[0] > 0 || ammotaken[1] != 30 || ammotaken[2] > 0 || ammotaken[3] > 0) {
			return true;
		}
	}
	if (weapon == GL_INFO) {
		if (ammotaken[0] > 0 || ammotaken[1] > 0 || ammotaken[2] != 5 || ammotaken[3] > 0) {
			return true;
		}
	}
	if (weapon == RL_INFO) {
		if (ammotaken[0] > 0 || ammotaken[1] > 0 || ammotaken[2] != 5 || ammotaken[3] > 0) {
			return true;
		}
	}
	if (weapon == LG_INFO) {
		if (ammotaken[0] > 0 || ammotaken[1] > 0 || ammotaken[2] > 0 || ammotaken[3] != 15) {
			return true;
		}
	}

	for (i = SSG_INFO; i <= LG_INFO; i++) {
		if ((taken & (1 << i)) && i != weapon) {
			return true;
		}
	}

	return false;
}

static void MVD_Stats_Gather_AlivePlayer(int player_index)
{
	int x; // item index
	int z; // item index
	int i = player_index;
	int killdiff;
	int taken = 0;
	int ammotaken[AMMO_TYPES] = { 0, 0, 0, 0 };
	qbool had_mega;
	qbool has_mega;

	for (x = GA_INFO; x <= RA_INFO && mvd_cg_info.deathmatch != 4; x++) {
		if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it) {

			if (!mvd_new_info[i].mvdinfo.itemstats[x].has) {
				taken |= (1 << x);
				MVD_Set_Armor_Stats(x, i);
				mvd_new_info[i].mvdinfo.itemstats[x].count++;
				mvd_new_info[i].mvdinfo.itemstats[x].lost = mvd_new_info[i].p_info->stats[STAT_ARMOR];
				mvd_new_info[i].mvdinfo.itemstats[x].has = 1;
				mvd_new_info[i].mvdinfo.itemstats[x].starttime = cls.demopackettime;
			}

			if (mvd_new_info[i].mvdinfo.itemstats[x].lost < mvd_new_info[i].p_info->stats[STAT_ARMOR]) {
				taken |= (1 << x);
				mvd_new_info[i].mvdinfo.itemstats[x].count++;
				mvd_new_info[i].mvdinfo.itemstats[x].starttime = cls.demopackettime;
			}
			mvd_new_info[i].mvdinfo.itemstats[x].lost = mvd_new_info[i].p_info->stats[STAT_ARMOR];
		}
	}

	for (x = RING_INFO; x <= PENT_INFO && mvd_cg_info.deathmatch != 4; x++) {
		mvd_pw_t* item = &mvd_new_info[i].mvdinfo.itemstats[x];

		if (!item->has && (mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it)) {
			taken |= (1 << x);
			item->has = 1;

			if (x == PENT_INFO && (powerup_cam_status == powerup_cam_quadpent_active || powerup_cam_status == powerup_cam_pent_active)) {
				pent_is_active = true;
				powerup_cam_status -= powerup_cam_pent_active;
				pent_time = cls.demopackettime;
			}
			if (x == QUAD_INFO && (powerup_cam_status == powerup_cam_quadpent_active || powerup_cam_status == powerup_cam_quad_active)) {
				quad_is_active = true;
				powerup_cam_status -= powerup_cam_quad_active;
				quad_time = cls.demopackettime;
			}
			mvd_new_info[i].mvdinfo.itemstats[x].starttime = cls.demopackettime;
			mvd_new_info[i].mvdinfo.itemstats[x].count++;
		}
		if (mvd_new_info[i].mvdinfo.itemstats[x].has && !(mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it)) {
			mvd_new_info[i].mvdinfo.itemstats[x].has = 0;
			if (x == QUAD_INFO && quad_is_active) {
				quad_is_active = false;
			}
			if (x == PENT_INFO && pent_is_active) {
				pent_is_active = false;
			}
			mvd_new_info[i].mvdinfo.itemstats[x].runs[mvd_new_info[i].mvdinfo.itemstats[x].run].starttime = mvd_new_info[i].mvdinfo.itemstats[x].starttime;
			mvd_new_info[i].mvdinfo.itemstats[x].runs[mvd_new_info[i].mvdinfo.itemstats[x].run].time = cls.demopackettime - mvd_new_info[i].mvdinfo.itemstats[x].starttime;
			mvd_new_info[i].mvdinfo.itemstats[x].run++;
		}
	}

	had_mega = mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has > 0;
	has_mega = mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPERHEALTH;
	if (!had_mega && has_mega) {
		// Picked up mega
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].mention = MENTION_PICKED_UP_ITEM;
		VectorCopy(mvd_new_info[i].p_state->origin, mvd_new_info[i].mega_locations[0]);
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has = 1;
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].count++;
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].starttime = cls.demopackettime;
		MVD_Status_Announcer(i, MH_INFO);
	}
	else if (has_mega && mvd_new_info[i].mvdinfo.itemstats[MH_INFO].lost < mvd_new_info[i].p_info->stats[STAT_HEALTH] && mvd_new_info[i].p_info->stats[STAT_HEALTH] > 100) {
		// They already had mega health but health increased - must have been another mega
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].mention = MENTION_PICKED_UP_ITEM;
		if (mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has < MAX_MEGAS_PER_PLAYER) {
			VectorCopy(mvd_new_info[i].p_state->origin, mvd_new_info[i].mega_locations[mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has]);
		}
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has++;
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].count++;
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].starttime = cls.demopackettime;
		MVD_Status_Announcer(i, MH_INFO);
	}
	mvd_new_info[i].mvdinfo.itemstats[MH_INFO].lost = mvd_new_info[i].p_info->stats[STAT_HEALTH];

	if (had_mega && !has_mega) {
		// Might have had two megas ticking down, create as many clocks as necessary
		int megas = mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has;
		while (megas > 0) {
			--megas;
			if (!cl.mvd_ktx_markers) {
				MVD_ClockStart(MH_INFO, megas < MAX_MEGAS_PER_PLAYER ? mvd_new_info[i].mega_locations[megas] : NULL);
			}
		}
		mvd_new_info[i].mvdinfo.itemstats[MH_INFO].has = 0;
	}

	for (z = RING_INFO; z <= PENT_INFO; z++) {
		if (mvd_new_info[i].mvdinfo.itemstats[z].has == 1) {
			mvd_new_info[i].mvdinfo.itemstats[z].runs[mvd_new_info[i].mvdinfo.itemstats[z].run].starttime = mvd_new_info[i].mvdinfo.itemstats[z].starttime;
			mvd_new_info[i].mvdinfo.itemstats[z].runs[mvd_new_info[i].mvdinfo.itemstats[z].run].time = cls.demopackettime - mvd_new_info[i].mvdinfo.itemstats[z].starttime;
		}
	}

	if (mvd_new_info[i].mvdinfo.lastfrags != mvd_new_info[i].p_info->frags) {
		if (mvd_new_info[i].mvdinfo.lastfrags < mvd_new_info[i].p_info->frags) {
			killdiff = mvd_new_info[i].p_info->frags - mvd_new_info[i].mvdinfo.lastfrags;
			for (z = 0; z < 8; z++) {
				if (z == MVD_Weapon_LWF(mvd_new_info[i].mvdinfo.lfw))
					mvd_new_info[i].mvdinfo.killstats.normal[z].kills += killdiff;
			}
			if (mvd_new_info[i].mvdinfo.lfw == -1)
				mvd_new_info[i].mvdinfo.spawntelefrags += killdiff;
			for (z = 8; z < 11; z++) {
				if (mvd_new_info[i].mvdinfo.itemstats[z].has)
					mvd_new_info[i].mvdinfo.itemstats[z].runs[mvd_new_info[i].mvdinfo.itemstats[z].run].frags += killdiff;
			}
			mvd_new_info[i].mvdinfo.runs[mvd_new_info[i].mvdinfo.run].frags++;
		}
		else if (mvd_new_info[i].mvdinfo.lastfrags > mvd_new_info[i].p_info->frags) {
			killdiff = mvd_new_info[i].mvdinfo.lastfrags - mvd_new_info[i].p_info->frags;
			for (z = AXE_INFO; z <= LG_INFO; z++) {
				if (z == MVD_Weapon_LWF(mvd_new_info[i].mvdinfo.lfw))
					mvd_new_info[i].mvdinfo.killstats.normal[z].teamkills += killdiff;
			}
			if (mvd_new_info[i].mvdinfo.lfw == -1) {
				mvd_new_info[i].mvdinfo.teamspawntelefrags += killdiff;

			}
			for (z = 8; z < 11; z++) {
				if (mvd_new_info[i].mvdinfo.itemstats[z].has)
					mvd_new_info[i].mvdinfo.itemstats[z].runs[mvd_new_info[i].mvdinfo.itemstats[z].run].teamfrags += killdiff;
			}
			mvd_new_info[i].mvdinfo.runs[mvd_new_info[i].mvdinfo.run].teamfrags++;
		}


		mvd_new_info[i].mvdinfo.lastfrags = mvd_new_info[i].p_info->frags;
	}

	mvd_new_info[i].mvdinfo.runs[mvd_new_info[i].mvdinfo.run].time = cls.demopackettime - mvd_new_info[i].mvdinfo.das.alivetimestart;

	if (mvd_new_info[i].mvdinfo.lfw == -1) {
		if (mvd_new_info[i].mvdinfo.lastfrags > mvd_new_info[i].p_info->frags) {
			mvd_new_info[i].mvdinfo.teamspawntelefrags += mvd_new_info[i].p_info->frags - mvd_new_info[i].mvdinfo.lastfrags;
		}
		else if (mvd_new_info[i].mvdinfo.lastfrags < mvd_new_info[i].p_info->frags) {
			mvd_new_info[i].mvdinfo.spawntelefrags += mvd_new_info[i].p_info->frags - mvd_new_info[i].mvdinfo.lastfrags;
		}
		mvd_new_info[i].mvdinfo.lastfrags = mvd_new_info[i].p_info->frags;
	}

	if (mvd_new_info[i].p_state->weaponframe > 0)
		mvd_new_info[i].mvdinfo.lfw = mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON];
	if (mvd_cg_info.deathmatch != 4) {
		MVD_Status_WP(i, &taken);
		for (z = SSG_INFO; z <= RA_INFO; z++) {
			MVD_Status_Announcer(i, z);
		}
	}

	for (x = 0; x < AMMO_TYPES; x++) {
		int ammo_new = mvd_new_info[i].p_info->stats[STAT_SHELLS + x];
		int ammo_old = mvd_new_info[i].mvdinfo.ammostats[x];
		if (mvd_new_info[i].mvdinfo.initialized && ammo_new > ammo_old) {
			int diff = ammo_new - ammo_old;
			ammotaken[x] = diff;
		}
		mvd_new_info[i].mvdinfo.ammostats[x] =
			mvd_new_info[i].p_info->stats[STAT_SHELLS + x];
	}

	for (x = 0; x < mvd_info_types; x++) {
		if (taken & (1 << x)) {
			qbool weapon_from_backpack =
				IS_WEAPON(x) && MVD_Weapon_From_Backpack(x, taken, ammotaken);
			// don't start clock if item was from backpack
			qbool add_clock = !weapon_from_backpack;
			Com_DPrintf("player %i took %i, weapon from backpack: %s\n",
				i, x, weapon_from_backpack ? "yes" : "no");

			MVD_Took(i, x, add_clock);
		}
	}
}

int MVD_Stats_Gather(void)
{
	int death_stats = 0;
	int x, i;

	if (cl.countdown || cl.standby) {
		return 0;
	}

	for (i = 0; i < mvd_cg_info.pcount; i++) {
		mvd_new_info[i].p_state = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[mvd_new_info[i].id];
	}

	for (i = 0; i < mvd_cg_info.pcount; i++) {
		if (pent_time == 0 && quad_time == 0 && !mvd_new_info[i].mvdinfo.firstrun) {
			powerup_cam_status = powerup_cam_quadpent_active;
			quad_time = pent_time = cls.demopackettime;
		}

		if (mvd_new_info[i].mvdinfo.firstrun == 0) {
			mvd_new_info[i].mvdinfo.das.alivetimestart = cls.demopackettime;
			gamestart_time = cls.demopackettime;
			mvd_new_info[i].mvdinfo.firstrun = 1;
			mvd_new_info[i].mvdinfo.lfw = -1;
		}

		// death alive stats
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH] > 0 && mvd_new_info[i].mvdinfo.das.isdead == 1) {
			mvd_new_info[i].mvdinfo.das.isdead = 0;
			mvd_new_info[i].mvdinfo.das.alivetimestart = cls.demopackettime;
			mvd_new_info[i].mvdinfo.lfw = -1;
		}

		mvd_new_info[i].mvdinfo.das.alivetime = cls.demopackettime - mvd_new_info[i].mvdinfo.das.alivetimestart;
		if (mvd_new_info[i].p_info->stats[STAT_HEALTH] <= 0 && mvd_new_info[i].mvdinfo.das.isdead != 1) {
			mvd_new_info[i].mvdinfo.das.isdead = 1;
			mvd_new_info[i].mvdinfo.das.deathcount++;
			death_stats = 1;
		}

		if (death_stats) {
			death_stats = 0;
			mvd_new_info[i].mvdinfo.run++;

			for (x = 0; x < mvd_info_types; x++) {
				if (mvd_new_info[i].p_info && mvd_new_info[i].p_info->stats[STAT_ACTIVEWEAPON] == mvd_wp_info[x].it) {
					if (mvd_wp_info[x].it == IT_ROCKET_LAUNCHER || mvd_wp_info[x].it == IT_LIGHTNING) {
						mvd_new_info[i].mvdinfo.itemstats[x].mention = MENTION_DROPPED_ITEM;
					}
					mvd_new_info[i].mvdinfo.itemstats[x].lost++;
				}
				else if (mvd_new_info[i].p_info && (mvd_new_info[i].p_info->stats[STAT_ITEMS] & mvd_wp_info[x].it)) {
					if (mvd_wp_info[x].it == IT_ROCKET_LAUNCHER || mvd_wp_info[x].it == IT_LIGHTNING || mvd_wp_info[x].it == IT_QUAD || mvd_wp_info[x].it == IT_INVISIBILITY) {
						mvd_new_info[i].mvdinfo.itemstats[x].mention = MENTION_DIED_WITH_ITEM;
					}
				}
				/*if (x == MVD_Weapon_LWF(mvd_new_info[i].mvdinfo.lfw)) {
					mvd_new_info[i].mvdinfo.itemstats[x].mention = MENTION_DIED_WITH_ITEM;
					mvd_new_info[i].mvdinfo.itemstats[x].lost++;
				}*/

				if (x == QUAD_INFO && mvd_new_info[i].mvdinfo.itemstats[QUAD_INFO].has) {
					if (mvd_new_info[i].mvdinfo.itemstats[x].starttime - cls.demopackettime < 30) {
						quad_is_active = false;
					}
					mvd_new_info[i].mvdinfo.itemstats[x].run++;
					mvd_new_info[i].mvdinfo.itemstats[x].lost++;
				}
				mvd_new_info[i].mvdinfo.itemstats[x].has = 0;
			}
			mvd_new_info[i].mvdinfo.lfw = -1;
		}

		if (!mvd_new_info[i].mvdinfo.das.isdead) {
			MVD_Stats_Gather_AlivePlayer(i);
		}
		else {
			// Report dropped items immediately
			int z;
			for (z = SSG_INFO; z <= RA_INFO; z++) {
				MVD_Status_Announcer(i, z);
			}
		}

		if ((((pent_time + 300) - cls.demopackettime) < 5) && !pent_is_active) {
			powerup_cam_status |= powerup_cam_pent_active;
		}
		if ((((quad_time + 60) - cls.demopackettime) < 5) && !quad_is_active) {
			powerup_cam_status |= powerup_cam_quad_active;
		}
		mvd_new_info[i].mvdinfo.initialized = true;
	}

	return 1;
}

void MVD_Status(void)
{
	int x, y, p;
	char str[1024];
	int i;
	int id = 0;
	int z = 0;
	double av_f = 0;
	double av_t = 0;
	double av_tk = 0;

	if (!mvd_status.value)
		return;

	if (!cls.mvdplayback)
		return;

	for (i = 0; i < mvd_cg_info.pcount; i++) {
		if (mvd_new_info[i].id == cl.spec_track) {
			id = i;
		}
	}

	x = ELEMENT_X_COORD(mvd_status);
	y = ELEMENT_Y_COORD(mvd_status);
	if (mvd_new_info[id].p_info) {
		strlcpy(str, mvd_new_info[id].p_info->name, sizeof(str));
	}
	else {
		str[0] = '\0';
	}
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);
	strlcpy(str, "&cf40Took", sizeof(str));

	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, va("RL: %i LG: %i GL: %i RA: %i YA: %i GA:%i", \
		mvd_new_info[id].mvdinfo.itemstats[RL_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[LG_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[GL_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[RA_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[YA_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[GA_INFO].count), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);
	strlcpy(str, va("Ring: %i Quad: %i Pent: %i MH: %i", \
		mvd_new_info[id].mvdinfo.itemstats[RING_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[PENT_INFO].count, \
		mvd_new_info[id].mvdinfo.itemstats[MH_INFO].count), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	//	Com_Printf("%f %f %f \n",lasttime,mvd_new_info[id].mvdinfo.das.alivetimestart, mvd_new_info[id].mvdinfo.das.alivetime);
	if (cls.demopackettime >= lasttime + .1) {
		lasttime = cls.demopackettime;
		lasttime1 = mvd_new_info[id].mvdinfo.das.alivetime;
	}

	strlcpy(str, va("Deaths: %i", mvd_new_info[id].mvdinfo.das.deathcount), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "Average Run:", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "Time      Frags TKS", sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	for (p = 0; p <= mvd_new_info[id].mvdinfo.run; p++) {
		av_t += mvd_new_info[id].mvdinfo.runs[p].time;
		av_f += mvd_new_info[id].mvdinfo.runs[p].frags;
		av_tk += mvd_new_info[id].mvdinfo.runs[p].teamfrags;
	}
	if (av_t > 0) {
		av_t = av_t / (mvd_new_info[id].mvdinfo.run + 1);
		av_f = av_f / (mvd_new_info[id].mvdinfo.run + 1);
		av_tk = av_tk / (mvd_new_info[id].mvdinfo.run + 1);
	}

	strlcpy(str, va("%9.3f %3.3f %3.3f", av_t, av_f, av_tk), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);


	strlcpy(str, "Last 3 Runs:", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "No. Time      Frags TKS", sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	p = mvd_new_info[id].mvdinfo.run - 3;
	if (p < 0)
		p = 0;
	//&& mvd_new_info[id].mvdinfo.runs[p].time
	for (; p <= mvd_new_info[id].mvdinfo.run; p++) {
		strlcpy(str, va("%3i %9.3f %5i %3i", p + 1, mvd_new_info[id].mvdinfo.runs[p].time, mvd_new_info[id].mvdinfo.runs[p].frags, mvd_new_info[id].mvdinfo.runs[p].teamfrags), sizeof(str));
		Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);
	}
	strlcpy(str, va("Last Fired Weapon: %s", TP_ItemName(mvd_new_info[id].mvdinfo.lfw)), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "&cf40Lost", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, va("RL: %i LG: %i GL: %i QUAD: %i", \
		mvd_new_info[id].mvdinfo.itemstats[RL_INFO].lost, \
		mvd_new_info[id].mvdinfo.itemstats[LG_INFO].lost, \
		mvd_new_info[id].mvdinfo.itemstats[GL_INFO].lost, \
		mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].lost), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "&cf40Kills", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i", \
		mvd_new_info[id].mvdinfo.killstats.normal[RL_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[LG_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[GL_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SNG_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[NG_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SSG_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SG_INFO].kills, \
		mvd_new_info[id].mvdinfo.killstats.normal[AXE_INFO].kills), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, va("SPAWN: %i", \
		mvd_new_info[id].mvdinfo.spawntelefrags), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "&cf40Teamkills", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, va("RL: %i LG: %i GL: %i SNG: %i NG: %i SSG: %i SG: %i AXE: %i", \
		mvd_new_info[id].mvdinfo.killstats.normal[RL_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[LG_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[GL_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SNG_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[NG_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SSG_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[SG_INFO].teamkills, \
		mvd_new_info[id].mvdinfo.killstats.normal[AXE_INFO].teamkills), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);
	strlcpy(str, va("SPAWN: %i", \
		mvd_new_info[id].mvdinfo.teamspawntelefrags), sizeof(str));
	strlcpy(str, Make_Red(str, 1), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "Last 3 Quad Runs:", sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	strlcpy(str, "No. Time      Frags TKS", sizeof(str));
	strlcpy(str, Make_Red(str, 0), sizeof(str));
	Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);

	p = mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].run - 3;
	if (p < 0)
		p = 0;
	for (; p <= mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].run && mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].runs[p].time; p++) {
		strlcpy(str, va("%3i %9.3f %5i %3i", p + 1, mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].runs[p].time, mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].runs[p].frags, mvd_new_info[id].mvdinfo.itemstats[QUAD_INFO].runs[p].teamfrags), sizeof(str));
		Draw_ColoredString(x, y + ((z++) * 8), str, 1, false);
	}
}

qbool MVD_MatchStarted(void) {
	if (was_standby && !cl.standby) {
		was_standby = false;
		return true;
	}
	was_standby = cl.standby;
	return false;
}

void MVD_Mainhook(void) {
	if (MVD_MatchStarted()) {
		MVD_Init_Info(MAX_CLIENTS);
	}

	MVD_Stats_Gather();
	MVD_Stats_CalcAvgRuns();
	MVD_AutoTrack();
	MVD_ClockList_RemoveExpired();
	if (cls.mvdplayback && mvd_demo_track_run == 0) {
		MVD_Demo_Track();
	}
}

static void MVD_PowerupCam_GetCoords(void)
{
	char val[1024];

	strlcpy(val, mvd_pc_quad_1.string, sizeof(val));
	cam_id[0].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[0].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[0].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[0].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[0].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[0].tag = "q1";
	cam_id[0].filter = powerup_cam_quad_active;

	strlcpy(val, mvd_pc_quad_2.string, sizeof(val));
	cam_id[1].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[1].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[1].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[1].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[1].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[1].tag = "q2";
	cam_id[1].filter = powerup_cam_quad_active;

	strlcpy(val, mvd_pc_quad_3.string, sizeof(val));
	cam_id[2].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[2].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[2].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[2].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[2].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[2].tag = "q3";
	cam_id[2].filter = powerup_cam_quad_active;

	strlcpy(val, mvd_pc_pent_1.string, sizeof(val));
	cam_id[3].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[3].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[3].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[3].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[3].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[3].tag = "p1";
	cam_id[3].filter = powerup_cam_pent_active;

	strlcpy(val, mvd_pc_pent_2.string, sizeof(val));
	cam_id[4].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[4].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[4].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[4].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[4].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[4].tag = "p2";
	cam_id[4].filter = powerup_cam_pent_active;

	strlcpy(val, mvd_pc_pent_3.string, sizeof(val));
	cam_id[5].cam.org[0] = (float)atof(strtok(val, " "));
	cam_id[5].cam.org[1] = (float)atof(strtok(NULL, " "));
	cam_id[5].cam.org[2] = (float)atof(strtok(NULL, " "));
	cam_id[5].cam.angles[0] = (float)atof(strtok(NULL, " "));
	cam_id[5].cam.angles[1] = (float)atof(strtok(NULL, " "));
	cam_id[5].tag = "p3";
	cam_id[5].filter = powerup_cam_pent_active;
}

static qbool MVD_PowerupCam_Process(cvar_t* cvar)
{
	qbool active = false;

	if (strlen(cvar->string)) {
		int i;
		qbool found = false;

		for (i = 0; i < 6; i++) {
			if (!strcmp(cvar->string, cam_id[i].tag)) {
				found = true;

				if (cam_id[i].filter & powerup_cam_status) {
					VectorCopy(cam_id[i].cam.angles, r_refdef.viewangles);
					VectorCopy(cam_id[i].cam.org, r_refdef.vieworg);
					active = true;
				}
				break;
			}
		}
		if (!found) {
			Cvar_Set(cvar, "");
			Com_Printf("wrong tag for %s\n", cvar->name);
		}
	}

	return active;
}

// If powerup cam configured but not currently enabled
qbool MVD_PowerupCam_Hidden(void)
{
	int view_number = CL_MultiviewCurrentView();

	if (view_number < 1 || view_number > sizeof(powerup_cam_active) / sizeof(powerup_cam_active[0])) {
		return false;
	}

	--view_number;
	return powerup_cam_cvars[view_number].string && powerup_cam_cvars[view_number].string[0] && !(powerup_cam_status && powerup_cam_active[view_number]);
}

qbool MVD_PowerupCam_Enabled(void)
{
	int view_number = CL_MultiviewCurrentView();

	if (view_number < 1 || view_number > sizeof(powerup_cam_active) / sizeof(powerup_cam_active[0])) {
		return false;
	}

	--view_number;
	return powerup_cam_cvars[view_number].string && powerup_cam_cvars[view_number].string[0] && powerup_cam_status && powerup_cam_active[view_number];
}

void MVD_PowerupCam_Frame(void)
{
	int view_number = CL_MultiviewCurrentView();

	if (view_number < 1 || view_number > sizeof(powerup_cam_active) / sizeof(powerup_cam_active[0])) {
		return;
	}

	--view_number;
	if (!mvd_powerup_cam.integer || powerup_cam_status == powerup_cam_inactive) {
		memset(powerup_cam_active, 0, sizeof(powerup_cam_active));
		return;
	}

	MVD_PowerupCam_GetCoords();
	powerup_cam_active[view_number] = MVD_PowerupCam_Process(&powerup_cam_cvars[view_number]);
}

static void MVDAnnouncer_HelpListItems(void)
{
	int i;

	Con_Printf("Valid types: ");
	for (i = 0; i < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++i) {
		if (i) {
			Con_Printf(",");
		}
		Con_Printf("%s", mvd_wp_info[i].name);
	}
	Con_Printf("\n");
}

static int MVDAnnouncer_FindEntity(const vec3_t pos, int type)
{
	int j;

	for (j = 0; j < CL_MAX_EDICTS; j++) {
		int modindex = cl_entities[j].baseline.modelindex;
		int skin = cl_entities[j].baseline.skinnum;

		if (modindex >= 0 && modindex < sizeof(cl.model_precache) / sizeof(cl.model_precache[0]) && cl.model_precache[modindex]) {
			if (cl.model_precache[modindex]->modhint == mvd_wp_info[type].model_hint && skin == mvd_wp_info[type].skin_number) {
				vec3_t distance;
				VectorSubtract(cl_entities[j].baseline.origin, pos, distance);
				if (VectorLength(distance) < 50) {
					return j;
				}
			}
		}
	}

	return 0;
}

static void MVDAnnouncer_RemoveItem(void)
{
	vec3_t pos;
	const char* type;
	int i;
	mvd_clock_t* clock_entry;

	if (Cmd_Argc() < 5) {
		Con_Printf("Removes a persistent item clock for item at given position\n");
		Con_Printf("Usage: %s <x> <y> <z> <type>\n", Cmd_Argv(0));
		MVDAnnouncer_HelpListItems();
		return;
	}

	pos[0] = atof(Cmd_Argv(1));
	pos[1] = atof(Cmd_Argv(2));
	pos[2] = atof(Cmd_Argv(3));

	type = Cmd_Argv(4);

	for (i = 0; i < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++i) {
		if (!strcmp(type, mvd_wp_info[i].name)) {
			int ent = MVDAnnouncer_FindEntity(pos, i);

			if (ent) {
				for (clock_entry = mvd_clocklist; clock_entry; clock_entry = clock_entry->next) {
					if ((clock_entry->flags & MVDCLOCK_PERSISTENT) && clock_entry->entity == ent) {
						clock_entry->flags |= MVDCLOCK_HIDDEN;
						return;
					}
				}

				// Create it and mark it as hidden to stop it re-appearing
				MVD_ClockStartEntity(ent, mvd_wp_info[i].id, MVDCLOCK_PERSISTENT | MVDCLOCK_HIDDEN | MVDCLOCK_NEVERSPAWNED);
				return;
			}

			Con_Printf("No item found at specified location\n");
			return;
		}
	}

	Con_Printf("Invalid type specified\n");
	MVDAnnouncer_HelpListItems();
}

static void MVDAnnouncer_ListItems(void)
{
	mvd_clock_t* clock_entry;

	for (clock_entry = mvd_clocklist; clock_entry; clock_entry = clock_entry->next) {
		if ((clock_entry->flags & MVDCLOCK_PERSISTENT) && clock_entry->entity) {
			Con_Printf("mvd_name_item %d %d %d %s \"%s\" // entity %d\n",
				(int)cl_entities[clock_entry->entity].baseline.origin[0],
				(int)cl_entities[clock_entry->entity].baseline.origin[1],
				(int)cl_entities[clock_entry->entity].baseline.origin[2],
				mvd_wp_info[clock_entry->itemtype].name,
				clock_entry->location,
				clock_entry->entity
			);
		}
	}
}

static void MVDAnnouncer_NameItem(void)
{
	vec3_t pos;
	const char* type;
	const char* label;
	int i;

	if (Cmd_Argc() < 6) {
		Con_Printf("Creates a persistent item clock for item at given position\n");
		Con_Printf("Usage: %s <x> <y> <z> <type> <label>\n", Cmd_Argv(0));
		MVDAnnouncer_HelpListItems();
		return;
	}

	pos[0] = atof(Cmd_Argv(1));
	pos[1] = atof(Cmd_Argv(2));
	pos[2] = atof(Cmd_Argv(3));

	type = Cmd_Argv(4);
	label = Cmd_Argv(5);

	for (i = 0; i < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++i) {
		if (!strcmp(type, mvd_wp_info[i].name)) {
			int ent = MVDAnnouncer_FindEntity(pos, i);
			if (ent) {
				mvd_clock_t* existing = MVD_ClockFindEntity(ent);
				if (existing) {
					strlcpy(existing->location, label, sizeof(existing->location));
					// We move to bottom of the list to allow the user to list every item and dictate order
					existing->order = ++fixed_ordering;
					existing->flags &= ~(MVDCLOCK_HIDDEN);
				}
				else {
					MVD_ClockStartEntity(ent, mvd_wp_info[i].id, MVDCLOCK_PERSISTENT | MVDCLOCK_NEVERSPAWNED);
				}
				return;
			}

			Con_Printf("No entity of type %s found near %d %d %d\n", type, (int)pos[0], (int)pos[1], (int)pos[2]);
			return;
		}
	}

	Con_Printf("Invalid type specified '%s'\n", type);
	MVDAnnouncer_HelpListItems();
}

void MVD_Utils_Init(void)
{
	int i;

	MVD_AutoTrack_Init();
	MVD_XMLStats_Init();

	Cvar_SetCurrentGroup(CVAR_GROUP_MVD);
	Cvar_Register(&mvd_info);
	Cvar_Register(&mvd_info_show_header);
	Cvar_Register(&mvd_info_setup);
	Cvar_Register(&mvd_info_x);
	Cvar_Register(&mvd_info_y);

	Cvar_Register(&mvd_status);
	Cvar_Register(&mvd_status_x);
	Cvar_Register(&mvd_status_y);

	Cvar_Register(&mvd_powerup_cam);

	Cvar_Register(&mvd_pc_quad_1);
	Cvar_Register(&mvd_pc_quad_2);
	Cvar_Register(&mvd_pc_quad_3);

	Cvar_Register(&mvd_pc_pent_1);
	Cvar_Register(&mvd_pc_pent_2);
	Cvar_Register(&mvd_pc_pent_3);

	for (i = 0; i < sizeof(powerup_cam_cvars) / sizeof(powerup_cam_cvars[0]); ++i) {
		Cvar_Register(&powerup_cam_cvars[i]);
	}

	Cvar_Register(&mvd_moreinfo);
	Cvar_Register(&mvd_autoadd_items);
	Cvar_Register(&mvd_sortitems);
	Cmd_AddCommand("mvd_name_item", MVDAnnouncer_NameItem);
	Cmd_AddCommand("mvd_remove_item", MVDAnnouncer_RemoveItem);
	Cmd_AddCommand("mvd_list_items", MVDAnnouncer_ListItems);

	Cvar_ResetCurrentGroup();
}

void MVD_Screen(void) {
	MVD_Info();
	MVD_Status();
}

void MVD_FlushUserCommands(void)
{
	int i;
	float targettime = cls.demopackettime + cl.mvd_time_offset;

	for (i = 1; i < sizeof(cl.mvd_user_cmd) / sizeof(cl.mvd_user_cmd[0]); ++i) {
		if (cl.mvd_user_cmd_time[i] && cl.mvd_user_cmd_time[i] <= targettime) {
			cl.mvd_user_cmd_time[0] = cl.mvd_user_cmd_time[i];
			cl.mvd_user_cmd[0] = cl.mvd_user_cmd[i];
			cl.mvd_user_cmd_time[i] = 0;
			cl.mvd_user_cmd[i] = 0;
		}
	}
}

void MVD_ParseUserCommand(const char* s)
{
	float time;
	int command;
	int i;
	int plr;

	if (Cam_TrackNum() == -1) {
		return;
	}

	Cmd_TokenizeString((char*)s);

	if (Cmd_Argc() < 2) {
		return;
	}

	time = atof(Cmd_Argv(0));
	command = atoi(Cmd_Argv(1));
	plr = Cmd_Argc() >= 3 ? atoi(Cmd_Argv(2)) : 0;

	if (plr != 0 && cl.spec_track != plr - 1) {
		return;
	}

	if (!cl.mvd_time_offset) {
		cl.mvd_time_offset = time - cls.demopackettime;
	}

	MVD_FlushUserCommands();
	for (i = 0; i < sizeof(cl.mvd_user_cmd) / sizeof(cl.mvd_user_cmd[0]); ++i) {
		if (cl.mvd_user_cmd_time[i] == 0) {
			cl.mvd_user_cmd_time[i] = time;
			cl.mvd_user_cmd[i] = command;
			break;
		}
	}
}

mvd_new_info_t* MVD_StatsForPlayer(player_info_t* info)
{
	int i;
	for (i = 0; i < MAX_CLIENTS; ++i) {
		if (mvd_new_info[i].p_info == info) {
			return &mvd_new_info[i];
		}
	}
	return NULL;
}

void MVD_Initialise(void)
{
	memset(mvd_new_info, 0, sizeof(mvd_new_info));
	memset(&mvd_cg_info, 0, sizeof(mvd_cg_info));
	cl.mvd_ktx_markers = false;
}

void MVD_GameStart(void)
{
	int i;

	if (!cl.mvd_ktx_markers) {
		MVD_Initialise();
	}

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator == 1) {
			continue;
		}

		MVD_Init_Info(i);
	}
}

int MVD_ItemTypeForEntity(int ent)
{
	int modindex = cl_entities[ent].baseline.modelindex;
	int skin = cl_entities[ent].baseline.skinnum;

	if (modindex >= 0 && modindex < sizeof(cl.model_precache) / sizeof(cl.model_precache[0]) && cl.model_precache[modindex]) {
		int j;

		for (j = 0; j < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++j) {
			if (mvd_wp_info[j].add_clock == MVD_ADDCLOCK_ALWAYS || (mvd_wp_info[j].add_clock == MVD_ADDCLOCK_DMM1 && cl.deathmatch == 1)) {
				if (cl.model_precache[modindex]->modhint == mvd_wp_info[j].model_hint && skin == mvd_wp_info[j].skin_number) {
					return mvd_wp_info[j].id;
				}
			}
		}
	}
	return 0;
}

// ktx matchstart (no args)
void MVDAnnouncer_MatchStart(void)
{
	int i;
	int j;

	MVD_Stats_Cleanup();
	MVD_Init_Info(MAX_CLIENTS);
	MVD_GameStart();
	cl.mvd_ktx_markers = true;

	// Clocklist should start as persistent timers from entity baselines
	if (mvd_autoadd_items.integer) {
		for (i = 0; i < CL_MAX_EDICTS; i++) {
			int modindex = cl_entities[i].baseline.modelindex;
			int skin = cl_entities[i].baseline.skinnum;

			if (modindex >= 0 && modindex < sizeof(cl.model_precache) / sizeof(cl.model_precache[0]) && cl.model_precache[modindex]) {
				for (j = 0; j < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++j) {
					if (mvd_wp_info[j].add_clock == MVD_ADDCLOCK_ALWAYS || (mvd_wp_info[j].add_clock == MVD_ADDCLOCK_DMM1 && cl.deathmatch == 1)) {
						if (cl.model_precache[modindex]->modhint == mvd_wp_info[j].model_hint && skin == mvd_wp_info[j].skin_number) {
							MVD_ClockStartEntity(i, mvd_wp_info[j].id, MVDCLOCK_PERSISTENT | MVDCLOCK_NEVERSPAWNED);
						}
					}
				}
			}
		}
	}
	TP_ExecTrigger("f_demomatchstart");

	was_standby = false;
}

// Signifies an item has been taken
// ktx took <entity-num> <respawn-period> <plr-entnum>
void MVDAnnouncer_ItemTaken(const char* s)
{
	int entity, respawn, player_ent;
	mvd_clock_t* clock_entry;

	Cmd_TokenizeString((char*)s);

	if (Cmd_Argc() < 5) {
		Com_DPrintf("//ktx took: expected 5 args, found %d\n", Cmd_Argc());
		return;
	}

	cl.mvd_ktx_markers = true;

	entity = atoi(Cmd_Argv(2));
	respawn = atoi(Cmd_Argv(3));
	player_ent = atoi(Cmd_Argv(4));

	clock_entry = MVD_ClockFindEntity(entity);
	if (clock_entry) {
		clock_entry->last_taken = cls.demopackettime;
		clock_entry->old_clockval = clock_entry->clockval;
		if (respawn > 0) {
			clock_entry->clockval = cls.demopackettime + respawn;
			if (clock_entry->itemtype == QUAD_INFO || clock_entry->itemtype == PENT_INFO || clock_entry->itemtype == RING_INFO) {
				clock_entry->hold_clockval = cls.demopackettime + 30;
			}
			else {
				clock_entry->hold_clockval = 0;
			}
		}
		else {
			// Hold indefinitely
			clock_entry->clockval = -1;
			clock_entry->hold_clockval = HUGE_VAL;
		}

		if (player_ent >= 1 && player_ent <= MAX_CLIENTS) {
			clock_entry->last_taken_by = player_ent;
		}
	}
	else if (respawn) {
		int item = MVD_ItemTypeForEntity(entity);

		if (item) {
			MVD_ClockStart(item, cl_entities[entity].baseline.origin);
			mvd_new_info[player_ent - 1].mvdinfo.itemstats[item].mention = MENTION_PICKED_UP_ITEM;
		}
	}
}

// Used to manually start a countdown if it was indefinitely held in the past (e.g. mega-health worn off)
// ktx timer <entity-num> <respawn-period>
void MVDAnnouncer_StartTimer(const char* s)
{
	int entity, respawn;
	mvd_clock_t* clock_entry;

	Cmd_TokenizeString((char*)s);

	if (Cmd_Argc() < 4) {
		Com_DPrintf("//ktx timer: expected 4 args, found %d\n", Cmd_Argc());
		return;
	}

	cl.mvd_ktx_markers = true;
	entity = atoi(Cmd_Argv(2));
	respawn = atoi(Cmd_Argv(3));

	clock_entry = MVD_ClockFindEntity(entity);
	if (clock_entry) {
		clock_entry->old_clockval = clock_entry->clockval;
		clock_entry->clockval = cls.demopackettime + respawn;
		clock_entry->last_taken_by = 0;
		clock_entry->last_taken = 0;
	}
	else {
		int item = MVD_ItemTypeForEntity(entity);

		if (item) {
			MVD_ClockStart(item, cl_entities[entity].baseline.origin);
		}
	}
}

// ktx drop <entity-num> <weapon-it> <plr-entnum>
void MVDAnnouncer_PackDropped(const char* s)
{
	int entity, weapon, player_ent, i;
	mvd_clock_t* clock_entry;

	Cmd_TokenizeString((char*)s);

	if (Cmd_Argc() < 5) {
		Com_DPrintf("//ktx drop: expected 5 args, found %d\n", Cmd_Argc());
		return;
	}

	cl.mvd_ktx_markers = true;
	entity = atoi(Cmd_Argv(2));
	weapon = atoi(Cmd_Argv(3));
	player_ent = atoi(Cmd_Argv(4));

	if (entity >= 0 && entity < sizeof(cl_entities) / sizeof(cl_entities[0])) {
		cl_entities[entity].contents = weapon;

		clock_entry = MVD_ClockFindEntity(entity);
		if (clock_entry) {
			MVD_ClockList_Remove(clock_entry);
		}

		for (i = 0; i < sizeof(mvd_wp_info) / sizeof(mvd_wp_info[0]); ++i) {
			if (mvd_wp_info[i].it == weapon) {
				clock_entry = MVD_ClockStartEntity(entity, i, MVDCLOCK_BACKPACK);
				clock_entry->clockval = cls.demopackettime + 120;
				clock_entry->dropped_by = player_ent;
			}
		}
	}
}

// ktx expire <entity-num>
void MVDAnnouncer_Expired(const char* s)
{
	int entity;
	mvd_clock_t* clock_entry;

	Cmd_TokenizeString((char*)s);

	if (Cmd_Argc() < 3) {
		Com_DPrintf("//ktx expire: expected 3 args, found %d\n", Cmd_Argc());
		return;
	}

	cl.mvd_ktx_markers = true;
	entity = atoi(Cmd_Argv(2));
	if (entity >= 0 && entity < sizeof(cl_entities) / sizeof(cl_entities[0])) {
		cl_entities[entity].contents = 0;

		clock_entry = MVD_ClockFindEntity(entity);
		if (clock_entry) {
			MVD_ClockList_Remove(clock_entry);
		}
	}
}

// ktx bp <entnum> <plr-entnum>
void MVDAnnouncer_BackpackPickup(const char* s)
{
	int entity, player_ent;
	mvd_clock_t* clock_entry;

	Cmd_TokenizeString((char*)s);
	if (Cmd_Argc() < 4) {
		Com_DPrintf("//ktx bp: expected 4 args, found %d\n", Cmd_Argc());
		return;
	}

	cl.mvd_ktx_markers = true;
	entity = atoi(Cmd_Argv(2));
	player_ent = atoi(Cmd_Argv(3));

	clock_entry = MVD_ClockFindEntity(entity);
	if (clock_entry) {
		clock_entry->last_taken_by = player_ent;
		clock_entry->clockval = cls.demopackettime + ITEMSCLOCK_TAKEN_PAUSE;
		clock_entry->last_taken = cls.demopackettime;
	}

	if (entity >= 0 && entity < sizeof(cl_entities) / sizeof(cl_entities[0])) {
		cl_entities[entity].contents = 0;
	}
}
