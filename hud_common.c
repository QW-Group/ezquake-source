/*
Copyright (C) 2011 azazello and ezQuake team

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
//
// common HUD elements
// like clock etc..
//

#include "quakedef.h"
#include "common_draw.h"
#ifdef WITH_PNG
#include <png.h>
#endif
#include "image.h"
#include "stats_grid.h"
#include "vx_stuff.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "rulesets.h"
#include "utils.h"
#include "sbar.h"
#include "hud.h"
#include "Ctrl.h"
#include "console.h"
#include "teamplay.h"
#include "mvd_utils.h"
#include "mvd_utils_common.h"

#ifndef STAT_MINUS
#define STAT_MINUS		10
#endif

#define MAX_FRAGS_NAME 32
#define FRAGS_HEALTH_SPACING 1

void SCR_HUD_WeaponStats(hud_t *hud);
void WeaponStats_HUDInit(void);

hud_t *hud_netgraph = NULL;

// Items to be filtered out
static int itemsclock_filter = 0;

// ----------------
// HUD planning
//

struct
{
	// this is temporary storage place for some of user's settings
	// hud_* values will be dumped into config file
	int old_multiview;
	int old_fov;
	int old_newhud;

	qbool active;
} autohud;

void OnAutoHudChange(cvar_t *var, char *value, qbool *cancel);
qbool autohud_loaded = false;
cvar_t hud_planmode = {"hud_planmode",   "0"};
cvar_t mvd_autohud = {"mvd_autohud", "0", 0, OnAutoHudChange};
cvar_t hud_digits_trim = {"hud_digits_trim", "1"};

// player sorting
// for frags and players
typedef struct sort_teams_info_s
{
	char *name;
	int  frags;
	int  min_ping;
	int  avg_ping;
	int  max_ping;
	int  nplayers;
	int  top, bottom;   // leader colours
	int  rlcount;       // Number of RLs present in the team. (Cokeman 2006-05-27)
	int  lgcount;       // number of LGs present in the team
	int  weapcount;     // number of players with weapons (RLs | LGs) in the team
	int  ra_taken;      // Total red armors taken
	int  ya_taken;      // Total yellow armors taken
	int  ga_taken;      // Total green armors taken
	int  mh_taken;      // Total megahealths taken
	int  quads_taken;   // Total quads taken
	int  pents_taken;   // Total pents taken
	int  rings_taken;   // Total rings taken
	int  stack;         // Total damage the team can take
	float ra_lasttime;  // last time ra taken
	float ya_lasttime;  // last time ya taken
	float ga_lasttime;  // last time ga taken
	float mh_lasttime;  // last time mh taken
	float q_lasttime;   // last time quad taken
	float p_lasttime;   // last time pent taken
	float r_lasttime;   // last time ring taken
}
sort_teams_info_t;

typedef struct sort_players_info_s
{
	int playernum;
	sort_teams_info_t *team;
}
sort_players_info_t;

static sort_players_info_t		sorted_players[MAX_CLIENTS];
static sort_teams_info_t		sorted_teams[MAX_CLIENTS];
static int       n_teams;
static int       n_players;
static int       n_spectators;
static cvar_t    hud_sortrules_teamsort = { "hud_sortrules_teamsort", "0" };
static cvar_t    hud_sortrules_playersort = { "hud_sortrules_playersort", "1" };
static cvar_t    hud_sortrules_includeself = { "hud_sortrules_includeself", "1" };
static int       active_player_position = -1;
static int       active_team_position = -1;

int hud_stats[MAX_CL_STATS];

extern cvar_t cl_weaponpreselect;
extern int IN_BestWeapon(void);
extern void DumpHUD(char *);
extern char *Macro_MatchType(void);

int HUD_Stats(int stat_num)
{
	if (hud_planmode.value)
		return hud_stats[stat_num];
	else
		return cl.stats[stat_num];
}

// ----------------
// HUD low levels
//

cvar_t hud_tp_need = {"hud_tp_need",   "0"};

/* tp need levels
   int TP_IsHealthLow(void);
   int TP_IsArmorLow(void);
   int TP_IsAmmoLow(int weapon); */
extern cvar_t tp_need_health, tp_need_ra, tp_need_ya, tp_need_ga,
       tp_weapon_order, tp_need_weapon, tp_need_shells,
       tp_need_nails, tp_need_rockets, tp_need_cells;

int State_AmmoNumForWeapon(int weapon)
{	// returns ammo number (shells = 1, nails = 2, rox = 3, cells = 4) for given weapon
	switch (weapon) {
		case 2: case 3: return 1;
		case 4: case 5: return 2;
		case 6: case 7: return 3;
		case 8: return 4;
		default: return 0;
	}
}

int State_AmmoForWeapon(int weapon)
{	// returns ammo amount for given weapon
	int ammon = State_AmmoNumForWeapon(weapon);

	if (ammon)
		return cl.stats[STAT_SHELLS + ammon - 1];
	else
		return 0;
}

int TP_IsHealthLow(void)
{
	return cl.stats[STAT_HEALTH] <= tp_need_health.value;
}

int TP_IsArmorLow(void)
{
	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR3))
		return cl.stats[STAT_ARMOR] <= tp_need_ra.value;
	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR2))
		return cl.stats[STAT_ARMOR] <= tp_need_ya.value;
	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR1))
		return cl.stats[STAT_ARMOR] <= tp_need_ga.value;
	return 1;
}

int TP_IsWeaponLow(void)
{
	char *s = tp_weapon_order.string;
	while (*s  &&  *s != tp_need_weapon.string[0])
	{
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << (*s-'0'-2)))
			return false;
		s++;
	}
	return true;
}

int TP_IsAmmoLow(int weapon)
{
	int ammo = State_AmmoForWeapon(weapon);
	switch (weapon)
	{
		case 2:
		case 3:  return ammo <= tp_need_shells.value;
		case 4:
		case 5:  return ammo <= tp_need_nails.value;
		case 6:
		case 7:  return ammo <= tp_need_rockets.value;
		case 8:  return ammo <= tp_need_cells.value;
		default: return 0;
	}
}

int TP_TeamFortressEngineerSpanner(void)
{
	char *player_skin=Info_ValueForKey(cl.players[cl.playernum].userinfo,"skin");
	char *model_name=cl.model_precache[CL_WeaponModelForView()->current.modelindex]->name;
	if (cl.teamfortress && player_skin
			&& (strcasecmp(player_skin, "tf_eng") == 0)
			&& model_name
			&& (strcasecmp(model_name, "progs/v_span.mdl") == 0))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

qbool HUD_HealthLow(void)
{
	if (hud_tp_need.value)
		return TP_IsHealthLow();
	else
		return HUD_Stats(STAT_HEALTH) <= 25;
}

qbool HUD_ArmorLow(void)
{
	if (hud_tp_need.value)
		return (TP_IsArmorLow());
	else
		return (HUD_Stats(STAT_ARMOR) <= 25);
}

qbool HUD_AmmoLow(void)
{
	if (hud_tp_need.value)
	{
		if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
			return TP_IsAmmoLow(2);
		else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
			return TP_IsAmmoLow(4);
		else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
			return TP_IsAmmoLow(6);
		else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
			return TP_IsAmmoLow(8);
		return false;
	}
	else
		return (HUD_Stats(STAT_AMMO) <= 10);
}

int HUD_AmmoLowByWeapon(int weapon)
{
	if (hud_tp_need.value)
		return TP_IsAmmoLow(weapon);
	else
	{
		int a;
		switch (weapon)
		{
			case 2:
			case 3:
				a = STAT_SHELLS; break;
			case 4:
			case 5:
				a = STAT_NAILS; break;
			case 6:
			case 7:
				a = STAT_ROCKETS; break;
			case 8:
				a = STAT_CELLS; break;
			default:
				return false;
		}
		return (HUD_Stats(a) <= 10);
	}
}

// ----------------
// DrawFPS
void SCR_HUD_DrawFPS(hud_t *hud)
{
	int x, y;
	char st[128];

	static cvar_t
		*hud_fps_show_min = NULL,
		*hud_fps_style,
		*hud_fps_title,
		*hud_fps_drop,
		*hud_fps_scale;

	if (hud_fps_show_min == NULL) {
		// first time called
		hud_fps_show_min = HUD_FindVar(hud, "show_min");
		hud_fps_style    = HUD_FindVar(hud, "style");
		hud_fps_title    = HUD_FindVar(hud, "title");
		hud_fps_drop     = HUD_FindVar(hud, "drop");
		hud_fps_scale    = HUD_FindVar(hud, "scale");
	}

	if (hud_fps_show_min->value) {
		snprintf(st, sizeof(st), "%3d\xf%3d", (int)(cls.min_fps + 0.25), (int)(cls.fps + 0.25));
	}
	else {
		snprintf(st, sizeof(st), "%3d", (int)(cls.fps + 0.25));
	}

	if (hud_fps_title->value) {
		strlcat(st, " fps", sizeof(st));
	}

	if (HUD_PrepareDraw(hud, strlen(st) * 8 * hud_fps_scale->value, 8 * hud_fps_scale->value, &x, &y)) {
		if ((hud_fps_style->value) == 1) {
			Draw_SAlt_String(x, y, st, hud_fps_scale->value);
		}
		else if ((hud_fps_style->value) == 2) {
			// if fps is less than a user-set value, then show it
			if ((hud_fps_drop->value) >= cls.fps) {
				Draw_SString(x, y, st, hud_fps_scale->value);
			}
		}
		else if ((hud_fps_style->value) == 3) {
			// if fps is less than a user-set value, then show it
			if ((hud_fps_drop->value) >= cls.fps) {
				Draw_SAlt_String(x, y, st, hud_fps_scale->value);
			}
		}
		else {
			// hud_fps_style is anything other than 1,2,3
			Draw_SString(x, y, st, hud_fps_scale->value);
		}
	}
}

void SCR_HUD_DrawVidLag(hud_t *hud)
{
	int x, y;
	char st[128];
	static cvar_t *hud_vidlag_style = NULL;
	static cvar_t *hud_vidlag_scale = NULL;

	extern qbool VID_VSyncIsOn(void);
	extern double vid_vsync_lag;
	static double old_lag;

	if (VID_VSyncIsOn() || glConfig.displayFrequency) {
		// take the average of last two values, otherwise it
		// changes very fast and is hard to read
		double current, avg;
		if (VID_VSyncIsOn()) {
			current = vid_vsync_lag;
		}
		else {
			current = min(cls.trueframetime, 1.0 / glConfig.displayFrequency) * 0.5;
		}
		avg = (current + old_lag) * 0.5;
		old_lag = current;
		snprintf (st, sizeof (st), "%2.1f", avg * 1000);
	}
	else {
		strcpy(st, "?");
	}

	if (hud_vidlag_style == NULL) {
		// first time called
		hud_vidlag_style = HUD_FindVar(hud, "style");
		hud_vidlag_scale = HUD_FindVar(hud, "scale");
	}

	strlcat (st, " ms", sizeof (st));

	if (HUD_PrepareDraw(hud, strlen(st) * 8 * hud_vidlag_scale->value, 8 * hud_vidlag_scale->value, &x, &y)) {
		if (hud_vidlag_style->value) {
			Draw_SAlt_String(x, y, st, hud_vidlag_scale->value);
		}
		else {
			Draw_SString(x, y, st, hud_vidlag_scale->value);
		}
	}
}

#define MAX_TRACKING_STRING		512

void SCR_HUD_DrawTracking(hud_t *hud)
{
	int x = 0, y = 0, width = 0, height = 0;
	char track_string[MAX_TRACKING_STRING];
	int player = spec_track;

	static cvar_t *hud_tracking_format = NULL,
		      *hud_tracking_scale;

	if (!hud_tracking_format) {
		hud_tracking_format = HUD_FindVar(hud, "format");
		hud_tracking_scale = HUD_FindVar(hud, "scale");
	}

	strlcpy(track_string, hud_tracking_format->string, sizeof(track_string));
	Replace_In_String(track_string, sizeof(track_string), '%', 2,
			"n", cl.players[player].name,						// Replace %n with player name.
			"t", cl.teamplay ? cl.players[player].team : "");	// Replace %t with player team if teamplay is on.
	height = 8 * hud_tracking_scale->value;
	width = 8 * strlen_color(track_string) * hud_tracking_scale->value;

	if (HUD_PrepareDraw (hud, width, height, &x, &y)) {
		if (cl.spectator && autocam == CAM_TRACK) {
			// Normal
			Draw_SString (x, y, track_string, hud_tracking_scale->value);
		}
	}
}

void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency,
		int lost, int minping, int avgping, int maxping, int devping,
		int posx, int posy, int width, int height, int revx, int revy);
// ----------------
// Netgraph
void SCR_HUD_Netgraph(hud_t *hud)
{
	static cvar_t
		*par_width = NULL, *par_height,
		*par_swap_x, *par_swap_y,
		*par_ploss;

	if (par_width == NULL)  // first time
	{
		par_width  = HUD_FindVar(hud, "width");
		par_height = HUD_FindVar(hud, "height");
		par_swap_x = HUD_FindVar(hud, "swap_x");
		par_swap_y = HUD_FindVar(hud, "swap_y");
		par_ploss  = HUD_FindVar(hud, "ploss");
	}

	R_MQW_NetGraph(cls.netchan.outgoing_sequence, cls.netchan.incoming_sequence,
			packet_latency, par_ploss->value ? CL_CalcNet() : -1, -1, -1, -1, -1, -1,
			-1, (int)par_width->value, (int)par_height->value,
			(int)par_swap_x->value, (int)par_swap_y->value);
}

//---------------------
//
// draw HUD ping
//
void SCR_HUD_DrawPing(hud_t *hud)
{
	double t;
	static double last_calculated;
	static int ping_avg, pl, ping_min, ping_max;
	static float ping_dev;

	int width, height;
	int x, y;
	char buf[512];

	static cvar_t
		*hud_ping_period = NULL,
		*hud_ping_show_pl,
		*hud_ping_show_dev,
		*hud_ping_show_min,
		*hud_ping_show_max,
		*hud_ping_style,
		*hud_ping_blink,
		*hud_ping_scale;

	if (hud_ping_period == NULL)    // first time
	{
		hud_ping_period   = HUD_FindVar(hud, "period");
		hud_ping_show_pl  = HUD_FindVar(hud, "show_pl");
		hud_ping_show_dev = HUD_FindVar(hud, "show_dev");
		hud_ping_show_min = HUD_FindVar(hud, "show_min");
		hud_ping_show_max = HUD_FindVar(hud, "show_max");
		hud_ping_style    = HUD_FindVar(hud, "style");
		hud_ping_blink    = HUD_FindVar(hud, "blink");
		hud_ping_scale    = HUD_FindVar(hud, "scale");
	}

	t = Sys_DoubleTime();
	if (t - last_calculated  >  hud_ping_period->value)
	{
		// recalculate

		net_stat_result_t result;
		float period;

		last_calculated = t;

		period = max(hud_ping_period->value, 0);

		CL_CalcNetStatistics(
				period,             // period of time
				network_stats,      // samples table
				NETWORK_STATS_SIZE, // number of samples in table
				&result);           // results

		if (result.samples == 0)
			return; // error calculating net

		ping_avg = (int)(result.ping_avg + 0.5);
		ping_min = (int)(result.ping_min + 0.5);
		ping_max = (int)(result.ping_max + 0.5);
		ping_dev = result.ping_dev;
		pl = result.lost_lost;

		clamp(ping_avg, 0, 999);
		clamp(ping_min, 0, 999);
		clamp(ping_max, 0, 999);
		clamp(ping_dev, 0, 99.9);
		clamp(pl, 0, 100);
	}

	buf[0] = 0;

	// blink
	if (hud_ping_blink->value)   // add dot
		strlcat (buf, (last_calculated + hud_ping_period->value/2 > cls.realtime) ? "\x8f" : " ", sizeof (buf));

	// min ping
	if (hud_ping_show_min->value)
		strlcat (buf, va("%d\xf", ping_min), sizeof (buf));

	// ping
	strlcat (buf, va("%d", ping_avg), sizeof (buf));

	// max ping
	if (hud_ping_show_max->value)
		strlcat (buf, va("\xf%d", ping_max), sizeof (buf));

	// unit
	strlcat (buf, " ms", sizeof (buf));

	// standard deviation
	if (hud_ping_show_dev->value)
		strlcat (buf, va(" (%.1f)", ping_dev), sizeof (buf));

	// pl
	if (hud_ping_show_pl->value)
		strlcat (buf, va(" \x8f %d%%", pl), sizeof (buf));

	// display that on screen
	width = strlen(buf) * 8 * hud_ping_scale->value;
	height = 8 * hud_ping_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (hud_ping_style->value) {
			Draw_SAlt_String(x, y, buf, hud_ping_scale->value);
		}
		else {
			Draw_SString(x, y, buf, hud_ping_scale->value);
		}
	}
}

static const char *SCR_HUD_ClockFormat(int format)
{
	switch (format) {
		case 1: return "%I:%M %p";
		case 2: return "%I:%M:%S %p";
		case 3: return "%H:%M";
		default: case 0: return "%H:%M:%S";
	}
}

//---------------------
//
// draw HUD clock
//
void SCR_HUD_DrawClock(hud_t *hud)
{
	int width, height;
	int x, y;
	const char *t;

	static cvar_t
		*hud_clock_big = NULL,
		*hud_clock_style,
		*hud_clock_blink,
		*hud_clock_scale,
		*hud_clock_format;

	if (hud_clock_big == NULL)    // first time
	{
		hud_clock_big   = HUD_FindVar(hud, "big");
		hud_clock_style = HUD_FindVar(hud, "style");
		hud_clock_blink = HUD_FindVar(hud, "blink");
		hud_clock_scale = HUD_FindVar(hud, "scale");
		hud_clock_format= HUD_FindVar(hud, "format");
	}

	t = SCR_GetTimeString(TIMETYPE_CLOCK, SCR_HUD_ClockFormat(hud_clock_format->integer));
	width = SCR_GetClockStringWidth(t, hud_clock_big->integer, hud_clock_scale->value);
	height = SCR_GetClockStringHeight(hud_clock_big->integer, hud_clock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		if (hud_clock_big->value)
			SCR_DrawBigClock(x, y, hud_clock_style->value, hud_clock_blink->value, hud_clock_scale->value, t);
		else
			SCR_DrawSmallClock(x, y, hud_clock_style->value, hud_clock_blink->value, hud_clock_scale->value, t);
	}
}

//---------------------
//
// draw HUD notify
//

void SCR_HUD_DrawNotify(hud_t* hud)
{
	static cvar_t* hud_notify_rows = NULL;
	static cvar_t* hud_notify_scale;
	static cvar_t* hud_notify_time;
	static cvar_t* hud_notify_cols;

	int x;
	int y;
	int width;
	int height;
	int chars_per_line;

	if (hud_notify_rows == NULL) // First time.
	{
		hud_notify_rows  = HUD_FindVar(hud, "rows");
		hud_notify_cols  = HUD_FindVar(hud, "cols");
		hud_notify_scale = HUD_FindVar(hud, "scale");
		hud_notify_time  = HUD_FindVar(hud, "time");
	}

	chars_per_line = (hud_notify_cols->integer > 0 ? hud_notify_cols->integer : con_linewidth);
	height = Q_rint ((con_linewidth / chars_per_line) * hud_notify_rows->integer * 8 * hud_notify_scale->value);
	width = 8 * chars_per_line * hud_notify_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		SCR_DrawNotify(x, y, hud_notify_scale->value, hud_notify_time->integer, hud_notify_rows->integer, chars_per_line);
	}
}

//---------------------
//
// draw HUD gameclock
//
void SCR_HUD_DrawGameClock(hud_t *hud)
{
	int width, height;
	int x, y;
	int timetype;
	const char *t;

	static cvar_t
		*hud_gameclock_big = NULL,
		*hud_gameclock_style,
		*hud_gameclock_blink,
		*hud_gameclock_countdown,
		*hud_gameclock_scale,
		*hud_gameclock_offset;

	if (hud_gameclock_big == NULL)    // first time
	{
		hud_gameclock_big   = HUD_FindVar(hud, "big");
		hud_gameclock_style = HUD_FindVar(hud, "style");
		hud_gameclock_blink = HUD_FindVar(hud, "blink");
		hud_gameclock_countdown = HUD_FindVar(hud, "countdown");
		hud_gameclock_scale = HUD_FindVar(hud, "scale");
		hud_gameclock_offset = HUD_FindVar(hud, "offset");
		gameclockoffset = &hud_gameclock_offset->integer;
	}

	timetype = (hud_gameclock_countdown->value) ? TIMETYPE_GAMECLOCKINV : TIMETYPE_GAMECLOCK;
	t = SCR_GetTimeString(timetype, NULL);
	width = SCR_GetClockStringWidth(t, hud_gameclock_big->integer, hud_gameclock_scale->value);
	height = SCR_GetClockStringHeight(hud_gameclock_big->integer, hud_gameclock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		if (hud_gameclock_big->value)
			SCR_DrawBigClock(x, y, hud_gameclock_style->value, hud_gameclock_blink->value, hud_gameclock_scale->value, t);
		else
			SCR_DrawSmallClock(x, y, hud_gameclock_style->value, hud_gameclock_blink->value, hud_gameclock_scale->value, t);
	}
}

//---------------------
//
// draw HUD democlock
//
void SCR_HUD_DrawDemoClock(hud_t *hud)
{
	int width = 0;
	int height = 0;
	int x = 0;
	int y = 0;
	const char *t;
	static cvar_t
		*hud_democlock_big = NULL,
		*hud_democlock_style,
		*hud_democlock_blink,
		*hud_democlock_scale;

	if (!cls.demoplayback || cls.mvdplayback == QTV_PLAYBACK)
	{
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	if (hud_democlock_big == NULL)    // first time
	{
		hud_democlock_big   = HUD_FindVar(hud, "big");
		hud_democlock_style = HUD_FindVar(hud, "style");
		hud_democlock_blink = HUD_FindVar(hud, "blink");
		hud_democlock_scale = HUD_FindVar(hud, "scale");
	}

	t = SCR_GetTimeString(TIMETYPE_DEMOCLOCK, NULL);
	width = SCR_GetClockStringWidth(t, hud_democlock_big->integer, hud_democlock_scale->value);
	height = SCR_GetClockStringHeight(hud_democlock_big->integer, hud_democlock_scale->value);

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		if (hud_democlock_big->value)
			SCR_DrawBigClock(x, y, hud_democlock_style->value, hud_democlock_blink->value, hud_democlock_scale->value, t);
		else
			SCR_DrawSmallClock(x, y, hud_democlock_style->value, hud_democlock_blink->value, hud_democlock_scale->value, t);
	}
}

//---------------------
//
// network statistics
//
void SCR_HUD_DrawNetStats(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_net_period = NULL;

	if (hud_net_period == NULL)    // first time
	{
		hud_net_period = HUD_FindVar(hud, "period");
	}

	width = 16*8 ;
	height = 12 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8;

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
		SCR_NetStats(x, y, hud_net_period->value);
}

#define SPEED_GREEN				"52"
#define SPEED_BROWN_RED			"100"
#define SPEED_DARK_RED			"72"
#define SPEED_BLUE				"216"
#define SPEED_RED				"229"

#define	SPEED_STOPPED			SPEED_GREEN
#define	SPEED_NORMAL			SPEED_BROWN_RED
#define	SPEED_FAST				SPEED_DARK_RED
#define	SPEED_FASTEST			SPEED_BLUE
#define	SPEED_INSANE			SPEED_RED

//---------------------
//
// speed-o-meter
//
void SCR_HUD_DrawSpeed(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_speed_xyz = NULL,
		*hud_speed_width,
		*hud_speed_height,
		*hud_speed_tick_spacing,
		*hud_speed_opacity,
		*hud_speed_color_stopped,
		*hud_speed_color_normal,
		*hud_speed_color_fast,
		*hud_speed_color_fastest,
		*hud_speed_color_insane,
		*hud_speed_vertical,
		*hud_speed_vertical_text,
		*hud_speed_text_align,
		*hud_speed_style,
		*hud_speed_scale;

	if (hud_speed_xyz == NULL)    // first time
	{
		hud_speed_xyz           = HUD_FindVar(hud, "xyz");
		hud_speed_width         = HUD_FindVar(hud, "width");
		hud_speed_height        = HUD_FindVar(hud, "height");
		hud_speed_tick_spacing  = HUD_FindVar(hud, "tick_spacing");
		hud_speed_opacity       = HUD_FindVar(hud, "opacity");
		hud_speed_color_stopped = HUD_FindVar(hud, "color_stopped");
		hud_speed_color_normal  = HUD_FindVar(hud, "color_normal");
		hud_speed_color_fast    = HUD_FindVar(hud, "color_fast");
		hud_speed_color_fastest = HUD_FindVar(hud, "color_fastest");
		hud_speed_color_insane  = HUD_FindVar(hud, "color_insane");
		hud_speed_vertical      = HUD_FindVar(hud, "vertical");
		hud_speed_vertical_text = HUD_FindVar(hud, "vertical_text");
		hud_speed_text_align    = HUD_FindVar(hud, "text_align");
		hud_speed_style         = HUD_FindVar(hud, "style");
		hud_speed_scale         = HUD_FindVar(hud, "scale");
	}

	width = max(0, hud_speed_width->value) * hud_speed_scale->value;
	height = max(0, hud_speed_height->value) * hud_speed_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		SCR_DrawHUDSpeed(x, y, width, height,
			hud_speed_xyz->value,
			hud_speed_tick_spacing->value,
			hud_speed_opacity->value,
			hud_speed_vertical->value,
			hud_speed_vertical_text->value,
			hud_speed_text_align->value,
			hud_speed_color_stopped->value,
			hud_speed_color_normal->value,
			hud_speed_color_fast->value,
			hud_speed_color_fastest->value,
			hud_speed_color_insane->value,
			hud_speed_style->integer,
			hud_speed_scale->value
		);
	}
}

#define	HUD_SPEED2_ORIENTATION_UP		0
#define	HUD_SPEED2_ORIENTATION_DOWN		1
#define	HUD_SPEED2_ORIENTATION_RIGHT	2
#define	HUD_SPEED2_ORIENTATION_LEFT		3

void SCR_HUD_DrawSpeed2(hud_t *hud)
{
	int width, height;
	int x, y;

	static cvar_t *hud_speed2_xyz = NULL,
		*hud_speed2_opacity,
		*hud_speed2_color_stopped,
		*hud_speed2_color_normal,
		*hud_speed2_color_fast,
		*hud_speed2_color_fastest,
		*hud_speed2_color_insane,
		*hud_speed2_radius,
		*hud_speed2_wrapspeed,
		*hud_speed2_orientation,
		*hud_speed2_scale;

	if (hud_speed2_xyz == NULL)    // first time
	{
		hud_speed2_xyz              = HUD_FindVar(hud, "xyz");
		hud_speed2_opacity          = HUD_FindVar(hud, "opacity");
		hud_speed2_color_stopped    = HUD_FindVar(hud, "color_stopped");
		hud_speed2_color_normal     = HUD_FindVar(hud, "color_normal");
		hud_speed2_color_fast       = HUD_FindVar(hud, "color_fast");
		hud_speed2_color_fastest    = HUD_FindVar(hud, "color_fastest");
		hud_speed2_color_insane     = HUD_FindVar(hud, "color_insane");
		hud_speed2_radius           = HUD_FindVar(hud, "radius");
		hud_speed2_wrapspeed        = HUD_FindVar(hud, "wrapspeed");
		hud_speed2_orientation      = HUD_FindVar(hud, "orientation");
		hud_speed2_scale            = HUD_FindVar(hud, "scale");
	}

	// Calculate the height and width based on the radius.
	switch((int)hud_speed2_orientation->value)
	{
		case HUD_SPEED2_ORIENTATION_LEFT :
		case HUD_SPEED2_ORIENTATION_RIGHT :
			height = max(0, 2*hud_speed2_radius->value);
			width = max(0, (hud_speed2_radius->value));
			break;
		case HUD_SPEED2_ORIENTATION_DOWN :
		case HUD_SPEED2_ORIENTATION_UP :
		default :
			// Include the height of the speed text in the height.
			height = max(0, (hud_speed2_radius->value));
			width = max(0, 2*hud_speed2_radius->value);
			break;
	}

	width *= hud_speed2_scale->value;
	height *= hud_speed2_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		float radius;
		int player_speed;
		int arc_length;
		int color1, color2;
		int text_x = x;
		int text_y = y;
		vec_t *velocity;

		// Start and end points for the needle
		int needle_start_x = 0;
		int needle_start_y = 0;
		int needle_end_x = 0;
		int needle_end_y = 0;

		// The length of the arc between the zero point
		// and where the needle is pointing at.
		int needle_offset = 0;

		// The angle between the zero point and the position
		// that the needle is drawn on.
		float needle_angle = 0.0;

		// The angle where to start drawing the half circle and where to end.
		// This depends on the orientation of the circle (left, right, up, down).
		float circle_startangle = 0.0;
		float circle_endangle = 0.0;

		// Avoid divison by zero.
		if(hud_speed2_radius->value <= 0)
		{
			return;
		}

		// Get the velocity.
		if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0)
		{
			velocity = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].velocity;
		}
		else
		{
			velocity = cl.simvel;
		}

		// Calculate the speed
		if (!hud_speed2_xyz->value)
		{
			// Based on XY.
			player_speed = sqrt(velocity[0]*velocity[0]
					+ velocity[1]*velocity[1]);
		}
		else
		{
			// Based on XYZ.
			player_speed = sqrt(velocity[0]*velocity[0]
					+ velocity[1]*velocity[1]
					+ velocity[2]*velocity[2]);
		}

		// Set the color based on the wrap speed.
		switch ((int)(player_speed / hud_speed2_wrapspeed->value))
		{
			case 0:
				color1 = hud_speed2_color_stopped->integer;
				color2 = hud_speed2_color_normal->integer;
				break;
			case 1:
				color1 = hud_speed2_color_normal->integer;
				color2 = hud_speed2_color_fast->integer;
				break;
			case 2:
				color1 = hud_speed2_color_fast->integer;
				color2 = hud_speed2_color_fastest->integer;
				break;
			default:
				color1 = hud_speed2_color_fastest->integer;
				color2 = hud_speed2_color_insane->integer;
				break;
		}

		// Set some properties how to draw the half circle, needle and text
		// based on the orientation of the hud item.
		switch((int)hud_speed2_orientation->value)
		{
			case HUD_SPEED2_ORIENTATION_LEFT :
				{
					x += width;
					y += height / 2;
					circle_startangle = M_PI / 2.0;
					circle_endangle	= (3*M_PI) / 2.0;

					text_x = x - 4 * 8 * hud_speed2_scale->value;
					text_y = y - 8 * hud_speed2_scale->value * 0.5;
					break;
				}
			case HUD_SPEED2_ORIENTATION_RIGHT :
				{
					y += height / 2;
					circle_startangle = (3*M_PI) / 2.0;
					circle_endangle = (5*M_PI) / 2.0;
					needle_end_y = y + hud_speed2_radius->value * sin (needle_angle);

					text_x = x;
					text_y = y - 8 * hud_speed2_scale->value * 0.5;
					break;
				}
			case HUD_SPEED2_ORIENTATION_DOWN :
				{
					x += width / 2;
					circle_startangle = M_PI;
					circle_endangle = 2*M_PI;
					needle_end_y = y + hud_speed2_radius->value * sin (needle_angle);

					text_x = x - 2 * 8 * hud_speed2_scale->value;
					text_y = y;
					break;
				}
			case HUD_SPEED2_ORIENTATION_UP :
			default :
				{
					x += width / 2;
					y += height;
					circle_startangle = 0;
					circle_endangle = M_PI;
					needle_end_y = y - hud_speed2_radius->value * sin (needle_angle);

					text_x = x - 8 * 2 * hud_speed2_scale->value;
					text_y = y - 8 * hud_speed2_scale->value;
					break;
				}
		}

		//
		// Calculate the offsets and angles.
		//
		{
			// Calculate the arc length of the half circle background.
			arc_length = fabs((circle_endangle - circle_startangle) * hud_speed2_radius->value);

			// Calculate the angle where the speed needle should point.
			needle_offset = arc_length * (player_speed % Q_rint(hud_speed2_wrapspeed->value)) / Q_rint(hud_speed2_wrapspeed->value);
			needle_angle = needle_offset / hud_speed2_radius->value;

			// Draw from the center of the half circle.
			needle_start_x = x;
			needle_start_y = y;
		}

		radius = hud_speed2_radius->value * hud_speed2_scale->value;

		// Set the needle end point depending on the orientation of the hud item.
		switch((int)hud_speed2_orientation->value)
		{
			case HUD_SPEED2_ORIENTATION_LEFT :
				{
					needle_end_x = x - radius * sin (needle_angle);
					needle_end_y = y + radius * cos (needle_angle);
					break;
				}
			case HUD_SPEED2_ORIENTATION_RIGHT :
				{
					needle_end_x = x + radius * sin (needle_angle);
					needle_end_y = y - radius * cos (needle_angle);
					break;
				}
			case HUD_SPEED2_ORIENTATION_DOWN :
				{
					needle_end_x = x + radius * cos (needle_angle);
					needle_end_y = y + radius * sin (needle_angle);
					break;
				}
			case HUD_SPEED2_ORIENTATION_UP :
			default :
				{
					needle_end_x = x - radius * cos (needle_angle);
					needle_end_y = y - radius * sin (needle_angle);
					break;
				}
		}

		// Draw the speed-o-meter background.
		Draw_AlphaPieSlice (
			x, y,                            // Position
			radius,                          // Radius
			circle_startangle,               // Start angle
			circle_endangle - needle_angle,  // End angle
			1,                               // Thickness
			true,                            // Fill
			color1,                          // Color
			hud_speed2_opacity->value        // Opacity
		);

		// Draw a pie slice that shows the "color" of the speed.
		Draw_AlphaPieSlice(
			x, y,                               // Position
			radius,                             // Radius
			circle_endangle - needle_angle,     // Start angle
			circle_endangle,                    // End angle
			1,                                  // Thickness
			true,                               // Fill
			color2,                             // Color
			hud_speed2_opacity->value           // Opacity
		);

		// Draw the "needle attachment" circle.
		Draw_AlphaCircle (x, y, 2.0, 1, true, 15, hud_speed2_opacity->value);

		// Draw the speed needle.
		Draw_AlphaLineRGB (needle_start_x, needle_start_y, needle_end_x, needle_end_y, 1, RGBA_TO_COLOR(250, 250, 250, 255 * hud_speed2_opacity->value));

		// Draw the speed.
		Draw_SString (text_x, text_y, va("%d", player_speed), hud_speed2_scale->value);
	}
}

// =======================================================
//
//  s t a t u s   b a r   e l e m e n t s
//
//


// -----------
// gunz
//
void SCR_HUD_DrawGunByNum (hud_t *hud, int num, float scale, int style, int wide)
{
	extern mpic_t *sb_weapons[7][8];  // sbar.c
	int i = num - 2;
	int width, height;
	int x, y;
	char *tmp;

	scale = max(scale, 0.01);

	switch (style)
	{
		case 3: // opposite colors of case 1
		case 1:     // text, gold inactive, white active
			width = 16 * scale;
			height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;
			if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
			{
				switch (num)
				{
					case 2: tmp = "sg"; break;
					case 3: tmp = "bs"; break;
					case 4: tmp = "ng"; break;
					case 5: tmp = "sn"; break;
					case 6: tmp = "gl"; break;
					case 7: tmp = "rl"; break;
					case 8: tmp = "lg"; break;
					default: tmp = "";
				}

				if ( ((HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i)) && (style==1)) ||
						((HUD_Stats(STAT_ACTIVEWEAPON) != (IT_SHOTGUN<<i)) && (style==3))
				   )
					Draw_SString(x, y, tmp, scale);
				else
					Draw_SAlt_String(x, y, tmp, scale);
			}
			break;
		case 4: // opposite colors of case 2
		case 2:     // numbers, gold inactive, white active
			width = 8 * scale;
			height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;
			if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
			{
				if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) )
					num += '0' + (style == 4 ? 128 : 0);
				else
					num += '0' + (style == 4 ? 0 : 128);
				Draw_SCharacter(x, y, num, scale);
			}
			break;
		case 5: // COLOR active, gold inactive
		case 7: // COLOR active, white inactive
		case 6: // white active, COLOR inactive
		case 8: // gold active, COLOR inactive
			width = 16 * scale;
			height = 8 * scale;

			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;

			if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) ) {
				if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) ) {
					if ((style==5) || (style==7)) { // strip {}
						char *weap_str = TP_ItemName((IT_SHOTGUN<<i));
						char weap_white_stripped[32];
						Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
						Draw_SString(x, y, weap_white_stripped, scale);
					}
					else { //Strip both &cRGB and {}
						char inactive_weapon_buf[16];
						char inactive_weapon_buf_nowhite[16];
						Util_SkipEZColors(inactive_weapon_buf, TP_ItemName(IT_SHOTGUN<<i), sizeof(inactive_weapon_buf));
						Util_SkipChars(inactive_weapon_buf, "{}", inactive_weapon_buf_nowhite, sizeof(inactive_weapon_buf_nowhite));

						if (style==8) // gold active
							Draw_SAlt_String(x, y, inactive_weapon_buf_nowhite, scale);
						else if (style==6) // white active
							Draw_SString(x, y, inactive_weapon_buf_nowhite, scale);
					}
				}
				else {
					if ((style==5) || (style==7)) { //Strip both &cRGB and {}
						char inactive_weapon_buf[16];
						char inactive_weapon_buf_nowhite[16];
						Util_SkipEZColors(inactive_weapon_buf, TP_ItemName(IT_SHOTGUN<<i), sizeof(inactive_weapon_buf));
						Util_SkipChars(inactive_weapon_buf, "{}", inactive_weapon_buf_nowhite, sizeof(inactive_weapon_buf_nowhite));

						if (style==5) // gold inactive
							Draw_SAlt_String(x, y, inactive_weapon_buf_nowhite, scale);
						else if (style==7) // white inactive
							Draw_SString(x, y, inactive_weapon_buf_nowhite, scale);
					}
					else if ((style==6) || (style==8)) { // strip only {}
						char *weap_str = TP_ItemName((IT_SHOTGUN<<i));
						char weap_white_stripped[32];
						Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
						Draw_SString(x, y, weap_white_stripped, scale);
					}

				}
			}
			break;
		default:    // classic - pictures
			width  = scale * (wide ? 48 : 24);
			height = scale * 16;

			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;

			if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
			{
				float   time;
				int     flashon;

				time = cl.item_gettime[i];
				flashon = (int)((cl.time - time)*10);
				if (flashon < 0)
					flashon = 0;
				if (flashon >= 10)
				{
					if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) )
						flashon = 1;
					else
						flashon = 0;
				}
				else
					flashon = (flashon%5) + 2;

				if (wide  ||  num != 8)
					Draw_SPic (x, y, sb_weapons[flashon][i], scale);
				else
					Draw_SSubPic (x, y, sb_weapons[flashon][i], 0, 0, 24, 16, scale);
			}
			break;
	}
}

void SCR_HUD_DrawGun2 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time callse
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 2, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun3 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 3, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun4 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 4, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun5 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 5, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun6 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 6, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun7 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawGunByNum (hud, 7, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun8 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *wide;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		wide  = HUD_FindVar(hud, "wide");
	}
	SCR_HUD_DrawGunByNum (hud, 8, scale->value, style->value, wide->value);
}
void SCR_HUD_DrawGunCurrent (hud_t *hud)
{
	int gun;
	static cvar_t *scale = NULL, *style, *wide;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		wide  = HUD_FindVar(hud, "wide");
	}

	if (ShowPreselectedWeap()) {
		// using weapon pre-selection so show info for current best pre-selected weapon
		gun = IN_BestWeapon();
		if (gun < 2) {
			return;
		}
	} else {
		// not using weapon pre-selection or player is dead so show current selected weapon
		switch (HUD_Stats(STAT_ACTIVEWEAPON))
		{
			case IT_SHOTGUN << 0:   gun = 2; break;
			case IT_SHOTGUN << 1:   gun = 3; break;
			case IT_SHOTGUN << 2:   gun = 4; break;
			case IT_SHOTGUN << 3:   gun = 5; break;
			case IT_SHOTGUN << 4:   gun = 6; break;
			case IT_SHOTGUN << 5:   gun = 7; break;
			case IT_SHOTGUN << 6:   gun = 8; break;
			default: return;
		}
	}

	SCR_HUD_DrawGunByNum (hud, gun, scale->value, style->value, wide->value);
}

// ----------------
// powerzz
//
void SCR_HUD_DrawPowerup(hud_t *hud, int num, float scale, int style)
{
	extern mpic_t *sb_items[32];
	int    x, y, width, height;
	int    c;

	scale = max(scale, 0.01);

	switch (style)
	{
		case 1:     // letter
			width = height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;
			if (HUD_Stats(STAT_ITEMS) & (1<<(17+num)))
			{
				switch (num)
				{
					case 0: c = '1'; break;
					case 1: c = '2'; break;
					case 2: c = 'r'; break;
					case 3: c = 'p'; break;
					case 4: c = 's'; break;
					case 5: c = 'q'; break;
					default: c = '?';
				}
				Draw_SCharacter(x, y, c, scale);
			}
			break;
		default:    // classic - pics
			width = height = scale * 16;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;
			if (HUD_Stats(STAT_ITEMS) & (1<<(17+num)))
				Draw_SPic (x, y, sb_items[num], scale);
			break;
	}
}

void SCR_HUD_DrawKey1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 0, scale->value, style->value);
}
void SCR_HUD_DrawKey2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawRing(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawPent(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 3, scale->value, style->value);
}
void SCR_HUD_DrawSuit(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 4, scale->value, style->value);
}
void SCR_HUD_DrawQuad(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawPowerup(hud, 5, scale->value, style->value);
}

// -----------
// sigils
//
void SCR_HUD_DrawSigil(hud_t *hud, int num, float scale, int style)
{
	extern mpic_t *sb_sigil[4];
	int     x, y;

	scale = max(scale, 0.01);

	switch (style)
	{
		case 1:     // sigil number
			if (!HUD_PrepareDraw(hud, 8*scale, 8*scale, &x, &y))
				return;
			if (HUD_Stats(STAT_ITEMS) & (1<<(28+num)))
				Draw_SCharacter(x, y, num + '0', scale);
			break;
		default:    // classic - picture
			if (!HUD_PrepareDraw(hud, 8*scale, 16*scale, &x, &y))
				return;
			if (HUD_Stats(STAT_ITEMS) & (1<<(28+num)))
				Draw_SPic(x, y, sb_sigil[num], scale);
			break;
	}
}

void SCR_HUD_DrawSigil1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawSigil(hud, 0, scale->value, style->value);
}
void SCR_HUD_DrawSigil2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawSigil(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawSigil3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawSigil(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawSigil4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawSigil(hud, 3, scale->value, style->value);
}

// icons - active ammo, armor, face etc..
void SCR_HUD_DrawAmmoIcon(hud_t *hud, int num, float scale, int style)
{
	extern mpic_t *sb_ammo[4];
	int   x, y, width, height;

	scale = max(scale, 0.01);

	width = height = (style ? 8 : 24) * scale;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	if (style)
	{
		switch (num)
		{
			case 1: Draw_SAlt_String(x, y, "s", scale); break;
			case 2: Draw_SAlt_String(x, y, "n", scale); break;
			case 3: Draw_SAlt_String(x, y, "r", scale); break;
			case 4: Draw_SAlt_String(x, y, "c", scale); break;
		}
	}
	else
	{
		Draw_SPic (x, y, sb_ammo[num-1], scale);
	}
}
void SCR_HUD_DrawAmmoIconCurrent (hud_t *hud)
{
	int num;
	static cvar_t *scale = NULL, *style;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}

	if (ShowPreselectedWeap()) {
		// using weapon pre-selection so show info for current best pre-selected weapon ammo
		if (!(num = State_AmmoNumForWeapon(IN_BestWeapon())))
			return;
	} else {
		// not using weapon pre-selection or player is dead so show current selected ammo
		if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
			num = 1;
		else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
			num = 2;
		else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
			num = 3;
		else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
			num = 4;
		else if (TP_TeamFortressEngineerSpanner())
			num = 4;
		else
			return;
	}

	SCR_HUD_DrawAmmoIcon(hud, num, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon1 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawAmmoIcon(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon2 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawAmmoIcon(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon3 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawAmmoIcon(hud, 3, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon4 (hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	SCR_HUD_DrawAmmoIcon(hud, 4, scale->value, style->value);
}

void SCR_HUD_DrawArmorIcon(hud_t *hud)
{
	extern mpic_t  *sb_armor[3];
	extern mpic_t  *draw_disc;
	int   x, y, width, height;

	int style;
	float scale;

	static cvar_t *v_scale = NULL, *v_style;
	if (v_scale == NULL)  // first time called
	{
		v_scale = HUD_FindVar(hud, "scale");
		v_style = HUD_FindVar(hud, "style");
	}

	scale = max(v_scale->value, 0.01);
	style = (int)(v_style->value);

	width = height = (style ? 8 : 24) * scale;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	if (style)
	{
		int c;

		if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
			c = '@';
		else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3)
			c = 'r';
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2)
			c = 'y';
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1)
			c = 'g';
		else return;

		c += 128;

		Draw_SCharacter(x, y, c, scale);
	}
	else
	{
		mpic_t  *pic;

		if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
			pic = draw_disc;
		else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3)
			pic = sb_armor[2];
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2)
			pic = sb_armor[1];
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1)
			pic = sb_armor[0];
		else return;

		Draw_SPic (x, y, pic, scale);
	}
}

// face
void SCR_HUD_DrawFace(hud_t *hud)
{
	extern mpic_t  *sb_faces[5][2]; // 0 is dead, 1-4 is alive
	// 0 is static, 1 is temporary animation
	extern mpic_t  *sb_face_invis;
	extern mpic_t  *sb_face_quad;
	extern mpic_t  *sb_face_invuln;
	extern mpic_t  *sb_face_invis_invuln;

	int     f, anim;
	int     x, y;
	float   scale;

	static cvar_t *v_scale = NULL;
	if (v_scale == NULL)  // first time called
	{
		v_scale = HUD_FindVar(hud, "scale");
	}

	scale = max(v_scale->value, 0.01);

	if (!HUD_PrepareDraw(hud, 24*scale, 24*scale, &x, &y))
		return;

	if ( (HUD_Stats(STAT_ITEMS) & (IT_INVISIBILITY | IT_INVULNERABILITY) )
			== (IT_INVISIBILITY | IT_INVULNERABILITY) )
	{
		Draw_SPic (x, y, sb_face_invis_invuln, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_QUAD)
	{
		Draw_SPic (x, y, sb_face_quad, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_INVISIBILITY)
	{
		Draw_SPic (x, y, sb_face_invis, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
	{
		Draw_SPic (x, y, sb_face_invuln, scale);
		return;
	}

	if (HUD_Stats(STAT_HEALTH) >= 100)
		f = 4;
	else
		f = max(0, HUD_Stats(STAT_HEALTH)) / 20;

	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;
	Draw_SPic (x, y, sb_faces[f][anim], scale);
}


// status numbers
void SCR_HUD_DrawNum(hud_t *hud, int num, qbool low,
		float scale, int style, int digits, char *s_align)
{
	extern mpic_t *sb_nums[2][11];

	int  i;
	char buf[sizeof(int) * 3]; // each byte need <= 3 chars
	int  len;

	int width, height, x, y;
	int size;
	int align;

	clamp(num, -99999, 999999);

	scale = max(scale, 0.01);

	if (digits > 0)
		clamp(digits, 1, 6);
	else
		digits = 0; // auto-resize

	align = 2;
	switch (tolower(s_align[0]))
	{
		default:
		case 'l':   // 'l'eft
			align = 0; break;
		case 'c':   // 'c'enter
			align = 1; break;
		case 'r':   // 'r'ight
			align = 2; break;
	}

	snprintf(buf, sizeof (buf), "%d", (style == 2 || style == 3) ? num : abs(num));

	if(digits)
	{
		switch (hud_digits_trim.integer)
		{
			case 0: // 10030 -> 999
				len = strlen(buf);
				if (len > digits)
				{
					char *p = buf;
					if(num < 0)
						*p++ = '-';
					for (i = (num < 0) ? 1 : 0 ; i < digits; i++)
						*p++ = '9';
					*p = 0;
					len = digits;
				}
				break;
			default:
			case 1: // 10030 -> 030
				len = strlen(buf);
				if(len > digits)
				{
					char *p = buf;
					memmove(p, p + (len - digits), digits);
					buf[digits] = '\0';
					len = strlen(buf);
				}
				break;
			case 2: // 10030 -> 100
				buf[digits] = '\0';
				len = strlen(buf);
				break;
		}
	}
	else
	{
		len = strlen(buf);
	}

	switch (style)
	{
		case 1:
		case 3:
			size = 8;
			break;
		case 0:
		case 2:
		default:
			size = 24;
			break;
	}

	if(digits)
		width = digits * size;
	else
		width = size * len;

	height = size;

	switch (style)
	{
		case 1:
		case 3:
			if (!HUD_PrepareDraw(hud, scale*width, scale*height, &x, &y))
				return;
			switch (align)
			{
				case 0: break;
				case 1: x += scale * (width - size * len) / 2; break;
				case 2: x += scale * (width - size * len); break;
			}
			if (low)
				Draw_SAlt_String(x, y, buf, scale);
			else
				Draw_SString(x, y, buf, scale);
			break;

		case 0:
		case 2:
		default:
			if (!HUD_PrepareDraw(hud, scale*width, scale*height, &x, &y))
				return;
			switch (align)
			{
				case 0: break;
				case 1: x += scale * (width - size * len) / 2; break;
				case 2: x += scale * (width - size * len); break;
			}
			for (i = 0; i < len; i++)
			{
				if(buf[i] == '-' && style == 2)
				{
					Draw_STransPic (x, y, sb_nums[low ? 1 : 0][STAT_MINUS], scale);
					x += 24 * scale;
				}
				else
				{
					Draw_STransPic (x, y, sb_nums[low ? 1 : 0][buf[i] - '0'], scale);
					x += 24 * scale;
				}
			}
			break;
	}
}

void SCR_HUD_DrawHealth(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	static int value;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	value = HUD_Stats(STAT_HEALTH);
	SCR_HUD_DrawNum(hud, (value < 0 ? 0 : value), HUD_HealthLow(),
			scale->value, style->value, digits->value, align->string);
}

void SCR_HUD_DrawArmor(hud_t *hud)
{
	int level;
	qbool low;
	static cvar_t *scale = NULL, *style, *digits, *align, *pent_666;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
		pent_666 = HUD_FindVar(hud, "pent_666"); // Show 666 or armor value when carrying pentagram
	}

	if (HUD_Stats(STAT_HEALTH) > 0)
	{
		if ((HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) && pent_666->integer)
		{
			level = 666;
			low = true;
		}
		else
		{
			level = HUD_Stats(STAT_ARMOR);
			low = HUD_ArmorLow();
		}
	}
	else
	{
		level = 0;
		low = true;
	}

	SCR_HUD_DrawNum(hud, level, low,
			scale->value, style->value, digits->value, align->string);
}

void Draw_AMFStatLoss (int stat, hud_t* hud);
void SCR_HUD_DrawHealthDamage(hud_t *hud)
{
	// TODO: This is very naughty, HUD_PrepareDraw(hud, width, height, &x, &y); MUST be called.
	Draw_AMFStatLoss (STAT_HEALTH, hud);
	if (HUD_Stats(STAT_HEALTH) <= 0)
	{
		Amf_Reset_DamageStats();
	}
}

void SCR_HUD_DrawArmorDamage(hud_t *hud)
{
	// TODO: NAUGHTY!! HUD_PrepareDraw(hud, width, height, &x, &y); plz
	Draw_AMFStatLoss (STAT_ARMOR, hud);
}

void SCR_HUD_DrawAmmo(hud_t *hud, int num,
		float scale, int style, int digits, char *s_align)
{
	extern mpic_t *sb_ibar;
	int value, num_old;
	qbool low;

	num_old = num;
	if (num < 1 || num > 4)
	{	// draw 'current' ammo, which one is it?

		if (ShowPreselectedWeap()) {
			// using weapon pre-selection so show info for current best pre-selected weapon ammo
			if (!(num = State_AmmoNumForWeapon(IN_BestWeapon())))
				return;
		} else {
			// not using weapon pre-selection or player is dead so show current selected ammo
			if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
				num = 1;
			else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
				num = 2;
			else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
				num = 3;
			else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
				num = 4;
			else if (TP_TeamFortressEngineerSpanner())
				num = 4;
			else
				return;
		}
	}

	low = HUD_AmmoLowByWeapon(num * 2);
	if (num_old == 0 && (!ShowPreselectedWeap() || cl.standby)) {
		// this check is here to display a feature from KTPRO/KTX where you can see received damage in prewar
		// also we make sure this applies only to 'ammo' element
		// weapon preselection must always use HUD_Stats()
		value = cl.stats[STAT_AMMO];
	} else {
		value = HUD_Stats(STAT_SHELLS + num - 1);
	}

	if (style < 2)
	{
		// simply draw number
		SCR_HUD_DrawNum(hud, value, low, scale, style, digits, s_align);
	}
	else
	{
		// else - draw classic ammo-count box with background
		char buf[8];
		int  x, y;

		scale = max(scale, 0.01);

		if (!HUD_PrepareDraw(hud, 42*scale, 11*scale, &x, &y))
			return;

		snprintf (buf, sizeof (buf), "%3i", value);
		Draw_SSubPic(x, y, sb_ibar, 3+((num-1)*48), 0, 42, 11, scale);
		if (buf[0] != ' ')  Draw_SCharacter (x +  7*scale, y, 18+buf[0]-'0', scale);
		if (buf[1] != ' ')  Draw_SCharacter (x + 15*scale, y, 18+buf[1]-'0', scale);
		if (buf[2] != ' ')  Draw_SCharacter (x + 23*scale, y, 18+buf[2]-'0', scale);
	}
}

void SCR_HUD_DrawAmmoCurrent(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	SCR_HUD_DrawAmmo(hud, 0, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	SCR_HUD_DrawAmmo(hud, 1, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	SCR_HUD_DrawAmmo(hud, 2, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	SCR_HUD_DrawAmmo(hud, 3, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align;
	if (scale == NULL)  // first time called
	{
		scale  = HUD_FindVar(hud, "scale");
		style  = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align  = HUD_FindVar(hud, "align");
	}
	SCR_HUD_DrawAmmo(hud, 4, scale->value, style->value, digits->value, align->string);
}

// Problem icon, Net

void SCR_HUD_NetProblem (hud_t *hud) {
	extern mpic_t *scr_net;
	static cvar_t *scale = NULL;
	int x, y;
	extern qbool hud_editor;

	if(scale == NULL)
		scale = HUD_FindVar(hud, "scale");

	if ((cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1) || cls.demoplayback)
	{
		if (hud_editor)
			HUD_PrepareDraw(hud, scr_net->width, scr_net->height, &x, &y);
		return;
	}

	if (!HUD_PrepareDraw(hud, scr_net->width, scr_net->height, &x, &y))
		return;

	Draw_SPic (x, y, scr_net, scale->value);
}

// ============================================================================0
// Groups
// ============================================================================0
#define HUD_NUM_GROUPS 9

mpic_t *hud_group_pics[HUD_NUM_GROUPS];
qbool hud_group_pics_is_loaded[HUD_NUM_GROUPS];

/* Workaround to reload pics on vid_restart
 * since cache (where they are stored) is flushed
 */
void HUD_Common_Reset_Group_Pics(void)
{
	memset(hud_group_pics, 0, sizeof(hud_group_pics));
	memset(hud_group_pics_is_loaded, 0, sizeof(hud_group_pics_is_loaded));
}

void SCR_HUD_DrawGroup(hud_t *hud, int width, int height, mpic_t *pic, int pic_scalemode, float pic_alpha)
{
#define HUD_GROUP_SCALEMODE_TILE		1
#define HUD_GROUP_SCALEMODE_STRETCH		2
#define HUD_GROUP_SCALEMODE_GROW		3
#define HUD_GROUP_SCALEMODE_CENTER		4

	int x, y;

	clamp(width, 1, 99999);
	clamp(height, 1, 99999);

	// Set it to this, because 1.0 will make the colors
	// completly saturated, and no semi-transparency will show.
	pic_alpha = (pic_alpha) >= 1.0 ? 0.99 : pic_alpha;

	// Grow the group if necessary.
	if (pic_scalemode == HUD_GROUP_SCALEMODE_GROW
			&& pic != NULL && pic->height > 0 && pic->width > 0)
	{
		width = max(pic->width, width);
		height = max(pic->height, height);
	}

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		return;
	}

	// Draw the picture if it's set.
	if (pic != NULL && pic->height > 0)
	{
		int pw, ph;

		if (pic_scalemode == HUD_GROUP_SCALEMODE_TILE)
		{
			// Tile.
			int cx = 0, cy = 0;
			while (cy < height)
			{
				while (cx < width)
				{
					pw = min(pic->width, width - cx);
					ph = min(pic->height, height - cy);

					if (pw >= pic->width  &&  ph >= pic->height)
					{
						Draw_AlphaPic (x + cx , y + cy, pic, pic_alpha);
					}
					else
					{
						Draw_AlphaSubPic (x + cx, y + cy, pic, 0, 0, pw, ph, pic_alpha);
					}

					cx += pic->width;
				}

				cx = 0;
				cy += pic->height;
			}
		}
		else if (pic_scalemode == HUD_GROUP_SCALEMODE_STRETCH)
		{
			// Stretch or shrink the picture to fit.
			float scale_x = (float)width / pic->width;
			float scale_y = (float)height / pic->height;

			Draw_SAlphaSubPic2 (x, y, pic, 0, 0, pic->width, pic->height, scale_x, scale_y, pic_alpha);
		}
		else if (pic_scalemode == HUD_GROUP_SCALEMODE_CENTER)
		{
			// Center the picture in the group.
			int pic_x = x + (width - pic->width) / 2;
			int pic_y = y + (height - pic->height) / 2;

			int src_x = 0;
			int src_y = 0;

			if(x > pic_x)
			{
				src_x = x - pic_x;
				pic_x = x;
			}

			if(y > pic_y)
			{
				src_y = y - pic_y;
				pic_y = y;
			}

			Draw_AlphaSubPic (pic_x, pic_y,	pic, src_x, src_y, min(width, pic->width), min(height, pic->height), pic_alpha);
		}
		else
		{
			// Normal. Draw in the top left corner.
			Draw_AlphaSubPic (x, y, pic, 0, 0, min(width, pic->width), min(height, pic->height), pic_alpha);
		}
	}
}

#define HUD_GROUP_PIC_BASEPATH	"gfx/%s"
static qbool SCR_HUD_LoadGroupPic(cvar_t *var, unsigned int grp_id, char *newpic)
{
	char pic_path[MAX_PATH];
	mpic_t *tmp_pic = NULL;

	if(newpic && strlen(newpic) > 0) {
		/* Create path for new pic */
		snprintf(pic_path, sizeof(pic_path), HUD_GROUP_PIC_BASEPATH, newpic);

		// Try loading the pic.
		if (!(tmp_pic = Draw_CachePicSafe(pic_path, false, true))) {
			Com_Printf("Couldn't load picture %s for '%s'\n", newpic, var->name);
			return false;
		}
	}

	hud_group_pics[grp_id] = tmp_pic;

	return true;
}

void SCR_HUD_OnChangePic_GroupX(cvar_t *var, char *newpic, qbool *cancel)
{
	int idx;
	int res;
	res = sscanf(var->name, "hud_group%d_picture", &idx);
	if (res == 0 || res == EOF) {
		Com_Printf("Failed to parse group number (OnChange func)\n");
		*cancel = true;
	} else {
		idx--; /* group1 is on index 0 etc */
		*cancel = Ruleset_BlockHudPicChange() || !SCR_HUD_LoadGroupPic(var, idx, newpic);
	}
}

static void SCR_HUD_Groups_Draw(hud_t *hud)
{
	/* FIXME: Perhaps make some nice structs... */
	static cvar_t *width[HUD_NUM_GROUPS];
	static cvar_t *height[HUD_NUM_GROUPS];
	static cvar_t *picture[HUD_NUM_GROUPS];
	static cvar_t *pic_alpha[HUD_NUM_GROUPS];
	static cvar_t *pic_scalemode[HUD_NUM_GROUPS];
	int res;
	int idx;

	res = sscanf(hud->name, "group%d", &idx);
	if (res == 0 || res == EOF) {
		Com_Printf("Failed to parse group number from \"%s\"\n", hud->name);
		return;
	}
	idx--;

	/* First init */
	if (width[idx] == NULL) { // first time called
		width[idx] = HUD_FindVar(hud, "width");
		height[idx] = HUD_FindVar(hud, "height");
		picture[idx] = HUD_FindVar(hud, "picture");
		pic_alpha[idx] = HUD_FindVar(hud, "pic_alpha");
		pic_scalemode[idx] = HUD_FindVar(hud, "pic_scalemode");

		picture[idx]->OnChange = SCR_HUD_OnChangePic_GroupX;
	}

	if (!hud_group_pics_is_loaded[idx]) {
		SCR_HUD_LoadGroupPic(picture[idx], idx, picture[idx]->string);
		hud_group_pics_is_loaded[idx] = true;
	}

	SCR_HUD_DrawGroup(hud, width[idx]->value, height[idx]->value, hud_group_pics[idx], pic_scalemode[idx]->value, pic_alpha[idx]->value);
}

static int HUD_ComparePlayers(const void *vp1, const void *vp2)
{
	const sort_players_info_t *p1 = vp1;
	const sort_players_info_t *p2 = vp2;

	int r = 0;
	player_info_t *i1 = &cl.players[p1->playernum];
	player_info_t *i2 = &cl.players[p2->playernum];

	if (i1->spectator && !i2->spectator) {
		r = -1;
	}
	else if (!i1->spectator && i2->spectator) {
		r = 1;
	}
	else if (i1->spectator && i2->spectator) {
		r = Q_strcmp2(i1->name, i2->name);
	}
	else {
		//
		// Both are players.
		//
		if ((hud_sortrules_playersort.integer & 2) && cl.teamplay && p1->team && p2->team) {
			// Leading team on top, sort players inside of the teams.

			// Teamsort 1, first sort on team frags.
			if (hud_sortrules_teamsort.integer == 1) {
				r = p1->team->frags - p2->team->frags;
			}

			// sort on team name only.
			r = (r == 0) ? -Q_strcmp2(p1->team->name, p2->team->name) : r;
		}

		if (hud_sortrules_playersort.integer & 1) {
			r = (r == 0) ? i1->frags - i2->frags : r;
		}
		r = (r == 0) ? -Q_strcmp2(i1->name, i2->name) : r;
	}

	r = (r == 0) ? (p1->playernum - p2->playernum) : r;

	// qsort() sorts ascending by default, we want descending.
	// So negate the result.
	return -r;
}

static int HUD_CompareTeams(const void *vt1, const void *vt2)
{
	int r = 0;
	const sort_teams_info_t *t1 = vt1;
	const sort_teams_info_t *t2 = vt2;

	if (hud_sortrules_teamsort.integer == 1) {
		r = (t1->frags - t2->frags);
	}
	r = !r ? -Q_strcmp2(t1->name, t2->name) : r;

	// qsort() sorts ascending by default, we want descending.
	// So negate the result.
	return -r;
}

#define HUD_SCOREBOARD_ALL			0xffffffff
#define HUD_SCOREBOARD_SORT_TEAMS	(1 << 0)
#define HUD_SCOREBOARD_SORT_PLAYERS	(1 << 1)
#define HUD_SCOREBOARD_UPDATE		(1 << 2)
#define HUD_SCOREBOARD_AVG_PING		(1 << 3)

static void HUD_Sort_Scoreboard(int flags)
{
	int i;
	int team;
	int active_player = -1;
	qbool common_fragcounts = cl.teamfortress && cl.teamplay && cl.scoring_system == SCORING_SYSTEM_DEFAULT;

	active_player_position = -1;
	active_team_position = -1;

	n_teams = 0;
	n_players = 0;
	n_spectators = 0;

	// This taken from score_bar logic
	if (cls.demoplayback && !cl.spectator && !cls.mvdplayback) {
		active_player = cl.playernum;
	}
	else if ((cls.demoplayback || cl.spectator) && Cam_TrackNum() >= 0) {
		active_player = spec_track;
	}
	else {
		active_player = cl.playernum;
	}

	// Set team properties.
	if (flags & HUD_SCOREBOARD_UPDATE) {
		memset(sorted_teams, 0, sizeof(sorted_teams));

		for (i=0; i < MAX_CLIENTS; i++) {
			if (cl.players[i].name[0] && !cl.players[i].spectator) {
				if (cl.players[i].frags == 0 && cl.players[i].team[0] == '\0' && !strcmp(cl.players[i].name, "[ServeMe]")) {
					continue;
				}

				// Find players team
				for (team = 0; team < n_teams; team++) {
					if (cl.teamplay && !strcmp(cl.players[i].team, sorted_teams[team].name) && sorted_teams[team].name[0]) {
						break;
					}
					if (!cl.teamplay && !strcmp(cl.players[i].name, sorted_teams[team].name) && sorted_teams[team].name[0]) {
						break;
					}
				}

				// The team wasn't found in the list of existing teams
				// so add a new team.
				if (team == n_teams) {
					team = n_teams++;
					sorted_teams[team].min_ping = 999;
					sorted_teams[team].top = Sbar_TopColor(&cl.players[i]);
					sorted_teams[team].bottom = Sbar_BottomColor(&cl.players[i]);
					if (cl.teamplay) {
						sorted_teams[team].name = cl.players[i].team;
					}
					else {
						sorted_teams[team].name = cl.players[i].name;
					}
				}

				if (sorted_teams[team].nplayers >= 1) {
					common_fragcounts &= (sorted_teams[team].frags / sorted_teams[team].nplayers == cl.players[i].frags);
				}

				sorted_teams[team].nplayers++;
				if (cl.scoring_system == SCORING_SYSTEM_TEAMFRAGS) {
					if (sorted_teams[team].nplayers == 1) {
						sorted_teams[team].frags = cl.players[i].frags;
					}
					else {
						sorted_teams[team].frags = max(cl.players[i].frags, sorted_teams[team].frags);
					}
				}
				else {
					sorted_teams[team].frags += cl.players[i].frags;
				}
				sorted_teams[team].avg_ping += cl.players[i].ping;
				sorted_teams[team].min_ping = min(sorted_teams[team].min_ping, cl.players[i].ping);
				sorted_teams[team].max_ping = max(sorted_teams[team].max_ping, cl.players[i].ping);
				sorted_teams[team].stack += SCR_HUD_TotalStrength(cl.players[i].stats[STAT_HEALTH], cl.players[i].stats[STAT_ARMOR], SCR_HUD_ArmorType(cl.players[i].stats[STAT_ITEMS]));

				// The total RL count for the players team.
				if (cl.players[i].stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) {
					sorted_teams[team].rlcount++;
				}
				if (cl.players[i].stats[STAT_ITEMS] & IT_LIGHTNING) {
					sorted_teams[team].lgcount++;
				}
				if (cl.players[i].stats[STAT_ITEMS] & (IT_ROCKET_LAUNCHER | IT_LIGHTNING)) {
					sorted_teams[team].weapcount++;
				}

				if (cls.mvdplayback) {
					mvd_new_info_t* stats = MVD_StatsForPlayer(&cl.players[i]);
					sort_teams_info_t* t = &sorted_teams[team];

					if (stats) {
						t->quads_taken += stats->mvdinfo.itemstats[QUAD_INFO].count;
						t->pents_taken += stats->mvdinfo.itemstats[PENT_INFO].count;
						t->rings_taken += stats->mvdinfo.itemstats[RING_INFO].count;
						t->ga_taken += stats->mvdinfo.itemstats[GA_INFO].count;
						t->ya_taken += stats->mvdinfo.itemstats[YA_INFO].count;
						t->ra_taken += stats->mvdinfo.itemstats[RA_INFO].count;
						t->mh_taken += stats->mvdinfo.itemstats[MH_INFO].count;
						t->q_lasttime = max(t->q_lasttime, stats->mvdinfo.itemstats[QUAD_INFO].starttime);
						t->p_lasttime = max(t->p_lasttime, stats->mvdinfo.itemstats[PENT_INFO].starttime);
						t->r_lasttime = max(t->r_lasttime, stats->mvdinfo.itemstats[RING_INFO].starttime);
						t->ga_lasttime = max(t->ga_lasttime, stats->mvdinfo.itemstats[GA_INFO].starttime);
						t->ya_lasttime = max(t->ya_lasttime, stats->mvdinfo.itemstats[YA_INFO].starttime);
						t->ra_lasttime = max(t->ra_lasttime, stats->mvdinfo.itemstats[RA_INFO].starttime);
						t->mh_lasttime = max(t->mh_lasttime, stats->mvdinfo.itemstats[MH_INFO].starttime);
					}
				}

				// Set player data.
				sorted_players[n_players + n_spectators].playernum = i;
				if (i == active_player && !cl.players[i].spectator) {
					active_player_position = n_players + n_spectators;
				}

				// Increase the count.
				if (cl.players[i].spectator) {
					n_spectators++;
				}
				else {
					n_players++;
				}
			}
		}
	}

	// Calc avg ping.
	if (flags & HUD_SCOREBOARD_AVG_PING) {
		for (team = 0; team < n_teams; team++) {
			sorted_teams[team].avg_ping /= sorted_teams[team].nplayers;
		}
	}

	// Adjust team scores for team fortress scoring (required so it matches +showteamscores)
	if (cl.teamfortress && common_fragcounts) {
		for (team = 0; team < n_teams; ++team) {
			if (sorted_teams[team].nplayers) {
				sorted_teams[team].frags /= sorted_teams[team].nplayers;
			}
		}
	}

	// Sort teams.
	if (flags & HUD_SCOREBOARD_SORT_TEAMS) {
		active_team_position = -1;

		qsort(sorted_teams, n_teams, sizeof(sort_teams_info_t), HUD_CompareTeams);

		// if set, need to make sure that the player's team is first or second position
		if (hud_sortrules_includeself.integer && active_player_position >= 0) {
			player_info_t *player = &cl.players[sorted_players[active_player_position].playernum];
			sorted_players[active_player_position].team = NULL;

			// Find players team.
			for (team = 0; team < n_teams; team++) {
				if (!strcmp(player->team, sorted_teams[team].name) && sorted_teams[team].name[0]) {
					if (hud_sortrules_includeself.integer == 1 && team > 0) {
						sort_teams_info_t temp = sorted_teams[0];
						sorted_teams[0] = sorted_teams[team];
						sorted_teams[team] = temp;
					}
					else if (hud_sortrules_includeself.integer == 2 && team > 1) {
						sort_teams_info_t temp = sorted_teams[1];
						sorted_teams[1] = sorted_teams[team];
						sorted_teams[team] = temp;
					}
					break;
				}
			}
		}

		// BUGFIX, this needs to happen AFTER the team array has been sorted, otherwise the
		// players might be pointing to the incorrect team adress.
		for (i = 0; i < MAX_CLIENTS; i++) {
			player_info_t *player = &cl.players[sorted_players[i].playernum];
			sorted_players[i].team = NULL;

			// Find players team.
			for (team = 0; team < n_teams; team++) {
				if (!strcmp(player->team, sorted_teams[team].name) && sorted_teams[team].name[0]) {
					sorted_players[i].team = &sorted_teams[team];
					if (sorted_players[i].playernum == active_player) {
						active_team_position = team;
					}
					break;
				}
			}
		}
	}

	// Sort players.
	if (flags & HUD_SCOREBOARD_SORT_PLAYERS) {
		qsort(sorted_players, n_players + n_spectators, sizeof(sort_players_info_t), HUD_ComparePlayers);

		if (!cl.teamplay) {
			// Re-find player
			active_player_position = -1;
			for (i = 0; i < n_players + n_spectators; ++i) {
				if (sorted_players[i].playernum == active_player) {
					active_player_position = i;
					if (hud_sortrules_includeself.integer == 1 && i > 0) {
						sort_players_info_t temp = sorted_players[0];
						sorted_players[0] = sorted_players[i];
						sorted_players[i] = temp;
						active_player_position = 0;
					}
					else if (hud_sortrules_includeself.integer == 2 && i > 1) {
						sort_players_info_t temp = sorted_players[1];
						sorted_players[1] = sorted_players[i];
						sorted_players[i] = temp;
						active_player_position = 1;
					}
				}
			}
		}
	}
}

void Frags_DrawColors(int x, int y, int width, int height,
		int top_color, int bottom_color, float color_alpha,
		int frags, int drawBrackets, int style,
		float bignum, float scale)
{
	char buf[32];
	int posy = 0;
	int char_size = (bignum > 0) ? Q_rint(24 * bignum) : 8 * scale;

	Draw_AlphaFill(x, y, width, height / 2, top_color, color_alpha);
	Draw_AlphaFill(x, y + height / 2, width, height - height / 2, bottom_color, color_alpha);

	posy = y + (height - char_size) / 2;

	if (bignum > 0) {
		//
		// Scaled big numbers for frags.
		//
		extern  mpic_t *sb_nums[2][11];
		char *t = buf;
		int char_x;
		int char_y;
		snprintf(buf, sizeof (buf), "%d", frags);

		char_x = max(x, x + (width  - (int)strlen(buf) * char_size) / 2);
		char_y = max(y, posy);

		while (*t)
		{
			if (*t >= '0' && *t <= '9') {
				Draw_STransPic(char_x, char_y, sb_nums[0][*t - '0'], bignum);
				char_x += char_size;
			}
			else if (*t == '-') {
				Draw_STransPic(char_x, char_y, sb_nums[0][STAT_MINUS], bignum);
				char_x += char_size;
			}

			t++;
		}
	}
	else {
		// Normal text size.
		snprintf(buf, sizeof (buf), "%3d", frags);
		Draw_SString(x - 2 + (width - char_size * strlen(buf) - 2) / 2, posy, buf, scale);
	}

	if (drawBrackets && width) {
		// Brackets [] are not available scaled, so use normal size even
		// if we're drawing big frag nums.
		int brack_posy = y + (height - 8 * scale) / 2;
		int d = (width >= 32) ? 0 : 1;

		switch(style)
		{
			case 1 :
				Draw_SCharacter(x - 8, posy, 13, scale);
				break;
			case 2 :
				// Red outline.
				Draw_Fill(x, y - 1, width, 1, 0x4f);
				Draw_Fill(x, y - 1, 1, height + 2, 0x4f);
				Draw_Fill(x + width - 1, y - 1, 1, height + 2, 0x4f);
				Draw_Fill(x, y + height, width, 1, 0x4f);
				break;
			case 0 :
			default :
				Draw_SCharacter(x - 2 - 2 * d, brack_posy, 16, scale); // [
				Draw_SCharacter(x + width - 8 + 1 + d, brack_posy, 17, scale); // ]
				break;
		}
	}
}

#define FRAGS_HEALTHBAR_NORMAL_COLOR	75
#define FRAGS_HEALTHBAR_MEGA_COLOR		251
#define	FRAGS_HEALTHBAR_TWO_MEGA_COLOR	238
#define	FRAGS_HEALTHBAR_UNNATURAL_COLOR	144

float SCR_HUD_TotalStrength (float health, float armorValue, float armorType)
{
	return max (0, min (
		health / (1 - armorType),
		health + armorValue
	) );
}

float SCR_HUD_ArmorType(int items)
{
	if (items & IT_INVULNERABILITY) {
		return 0.99f;
	}
	else if (items & IT_ARMOR3) {
		return 0.8f;
	}
	else if (items & IT_ARMOR2) {
		return 0.6f;
	}
	else if (items & IT_ARMOR1) {
		return 0.3f;
	}
	return 0.0f;
}

void Frags_DrawHorizontalHealthBar(player_info_t* info, int x, int y, int width, int height, qbool flip, qbool use_power)
{
	static cvar_t* health_color_nohealth = NULL;
	static cvar_t* health_color_normal = NULL;
	static cvar_t* health_color_mega = NULL;
	static cvar_t* health_color_twomega = NULL;
	static cvar_t* health_color_unnatural = NULL;
	static cvar_t* armor_color_noarmor = NULL;
	static cvar_t* armor_color_ga = NULL;
	static cvar_t* armor_color_ya = NULL;
	static cvar_t* armor_color_ra = NULL;
	static cvar_t* armor_color_unnatural = NULL;

	int armor = info->stats[STAT_ARMOR];
	int items = info->stats[STAT_ITEMS];
	int health;
	int true_health = info->stats[STAT_HEALTH];
	float max_health = (use_power ? 450 : 250);
	float max_armor = 200;
	float armorType;
	float health_width, armor_width;
	float max_health_width, max_health_value;
	int health_height = (height * 3) / 4;
	int armor_height = height - health_height;
	color_t armor_color = 0;
	color_t health_color = 0;
	double now = (cls.mvdplayback ? cls.demotime : cl.time);
	qbool prewar = cl.standby || cl.countdown;

	float drop_wait_time = 3.0f;
	float drop_speed = 300.0f;

	byte* bk;
	color_t border_color;

	// Use other hud elements to get range of colours
	if (health_color_nohealth == NULL) {
		health_color_nohealth = Cvar_Find("hud_bar_health_color_nohealth");
		health_color_normal = Cvar_Find("hud_bar_health_color_normal");
		health_color_mega = Cvar_Find("hud_bar_health_color_mega");
		health_color_twomega = Cvar_Find("hud_bar_health_color_twomega");
		health_color_unnatural = Cvar_Find("hud_bar_health_color_unnatural");

		armor_color_noarmor = Cvar_Find("hud_bar_armor_color_noarmor");
		armor_color_ga = Cvar_Find("hud_bar_armor_color_ga");
		armor_color_ya = Cvar_Find("hud_bar_armor_color_ya");
		armor_color_ra = Cvar_Find("hud_bar_armor_color_ra");
		armor_color_unnatural = Cvar_Find("hud_bar_armor_color_unnatural");
	}

	bk = (byte*) &health_color_nohealth->color;
	border_color = RGBA_TO_COLOR(
		max(bk[0], 24) - 24,
		max(bk[1], 24) - 24,
		max(bk[2], 24) - 24,
		min(bk[3], 128) + 127
	);

	if (health_color_nohealth == NULL || armor_color_noarmor == NULL) {
		return;
	}

	// work out colours
	armorType = true_health >= 1 ? SCR_HUD_ArmorType(items) : 0;
	if (items & IT_INVULNERABILITY && true_health >= 1) {
		armor_color = RGBAVECT_TO_COLOR(armor_color_unnatural->color);
	}
	else if (items & IT_ARMOR3 && true_health >= 1) {
		armor_color = RGBAVECT_TO_COLOR(armor_color_ra->color);
		max_armor = 200;
	}
	else if (items & IT_ARMOR2 && true_health >= 1) {
		armor_color = RGBAVECT_TO_COLOR(armor_color_ya->color);
		max_armor = 150;
	}
	else if (items & IT_ARMOR1 && true_health >= 1) {
		armor_color = RGBAVECT_TO_COLOR(armor_color_ga->color);
		max_armor = 100;
	}

	if (use_power) {
		health = prewar ? 0 : SCR_HUD_TotalStrength(true_health, armor, armorType);
		if (health <= 110) {
			health_color = RGBAVECT_TO_COLOR(health_color_normal->color);
		}
		else if (health <= 220) {
			health_color = RGBAVECT_TO_COLOR(health_color_mega->color);
		}
		else if (health <= 450) {
			health_color = RGBAVECT_TO_COLOR(health_color_twomega->color);
		}
		else {
			health_color = RGBAVECT_TO_COLOR(health_color_unnatural->color);
		}
	}
	else {
		health = prewar ? 0 : true_health;
		if (health <= 100) {
			health_color = RGBAVECT_TO_COLOR(health_color_normal->color);
		}
		else if (health <= 200) {
			health_color = RGBAVECT_TO_COLOR(health_color_mega->color);
		}
		else if (health <= 250) {
			health_color = RGBAVECT_TO_COLOR(health_color_twomega->color);
		}
		else {
			health_color = RGBAVECT_TO_COLOR(health_color_unnatural->color);
		}
	}

	// Background
	Draw_AlphaRectangleRGB(x, y, width, health_height, 1.0, false, border_color);
	Draw_AlphaFillRGB(x, y + health_height, width, armor_height, RGBAVECT_TO_COLOR(armor_color_noarmor->color));
	x += 1;
	width -= 2;
	y += 1;
	health_height -= 2;
	Draw_AlphaFillRGB(x, y, width, health_height, RGBAVECT_TO_COLOR(health_color_nohealth->color));

	// Calculate figures
	health = min(health, max_health);
	if (info->prev_health > health && info->prev_health > 0) {
		// Decrease health bar to current health
		double dropTime = now - info->prev_health_last_set;
		int new_health = health;

		health = max(health, info->prev_health - dropTime * drop_speed);
		if (health == new_health) {
			info->prev_health = health;
			info->prev_health_last_set = now;
		}
	}
	else if (info->prev_health < health && info->prev_health > 0) {
		// Increase health bar to current health
		double dropTime = now - info->prev_health_last_set;
		int new_health = health;

		health = min(health, info->prev_health + dropTime * drop_speed);
		if (health == new_health) {
			info->prev_health = health;
			info->prev_health_last_set = now;
		}
	}
	else {
		info->prev_health = (prewar && health == max_health ? 0 : health);
		info->prev_health_last_set = now;
	}

	// Decrease the maximum health
	max_health_value = min(info->max_health, max_health);
	if (info->max_health_last_set && info->max_health_last_set < now - drop_wait_time) {
		double dropTime = now - info->max_health_last_set - drop_wait_time;

		if (health < info->max_health && info->prev_health > 0) {
			max_health_value = max(health, info->max_health - dropTime * drop_speed);
			if (max_health_value == health) {
				info->max_health = max_health_value = health;
				info->max_health_last_set = 0;
			}
		}
		else {
			info->max_health = max_health_value = (prewar ? 0 : health);
			info->max_health_last_set = 0;
		}
	}
	else if (health >= info->max_health) {
		info->max_health = (prewar ? 0 : health);
		info->max_health_last_set = 0;
	}
	else if (health < info->prev_health && !cls.mvdplayback) {
		info->max_health_last_set = now;
	}
	else if (prewar) {
		info->max_health = max_health_value = 0;
	}

	// Draw a health bar.
	health_width = prewar ? width : Q_rint((width / max_health) * health);
	health_width = (health_width > 0.0 && health_width < 1.0) ? 1 : health_width;
	health_width = max(health_width, 0);

	armor = min(armor, max_armor);
	armor_width = Q_rint((width / max_armor) * armor);
	armor_width = (armor_width > 0.0 && armor_width < 1.0) ? 1 : armor_width;
	armor_width = max(armor_width, 0);

	max_health_width = 0;
	if (info->max_health_last_set && health < max_health_value) {
		max_health_width = Q_rint((width / max_health) * max_health_value);
		max_health_width -= health_width;
		max_health_width = (max_health_width > 0.0 && max_health_width < 1.0) ? 1 : max_health_width;
		max_health_width = max(max_health_width, 0);
	}

	if (flip) {
		Draw_AlphaFillRGB(x + width - (int)health_width, y, health_width, health_height, health_color);
		if (max_health_width) {
			Draw_AlphaFillRGB(x + width - (int)health_width - (int)max_health_width, y, max_health_width, health_height, RGBA_TO_COLOR(255, 0, 0, 128));
		}

		if (armor_width > 0 && health > 0) {
			Draw_AlphaFillRGB(x + width - (int)armor_width, y + health_height, armor_width, armor_height, armor_color);
		}
	}
	else {
		Draw_AlphaFillRGB(x, y, health_width, health_height, health_color);
		if (max_health_width) {
			Draw_AlphaFillRGB(x + health_width, y, max_health_width, health_height, RGBA_TO_COLOR(255, 0, 0, 128));
		}

		Draw_AlphaFillRGB(x, y + health_height, width, armor_height, RGBAVECT_TO_COLOR(armor_color_noarmor->color));
		if (armor_width > 0 && health > 0) {
			Draw_AlphaFillRGB(x, y + health_height, armor_width, armor_height, armor_color);
		}
	}
}

void Frags_DrawHealthBar(int original_health, int x, int y, int height, int width, qbool horizontal, qbool flip)
{
	int health;

	// Get the health.
	health = original_health;
	health = min(100, health);

	// Draw a health bar.
	if (horizontal) {
		float health_width = Q_rint((width / 100.0) * health);
		health_width = (health_width > 0.0 && health_width < 1.0) ? 1 : health_width;
		health_width = (health_width < 0.0) ? 0.0 : health_width;

		if (flip) {
			Draw_Fill(x + width - (int)health_width, y, health_width, (height * 3) / 4, FRAGS_HEALTHBAR_NORMAL_COLOR);
		}
		else {
			Draw_Fill(x, y, x + (int)health_width, (height * 3) / 4, FRAGS_HEALTHBAR_NORMAL_COLOR);
		}
	}
	else {
		float health_height = Q_rint((height / 100.0) * health);
		health_height = (health_height > 0.0 && health_height < 1.0) ? 1 : health_height;
		health_height = (health_height < 0.0) ? 0.0 : health_height;
		Draw_Fill(x, y + height - (int)health_height, 3, (int)health_height, FRAGS_HEALTHBAR_NORMAL_COLOR);

		// Get the health again to check if health is more than 100.
		health = original_health;
		if(health > 100 && health <= 200)
		{
			float health_height = (int)Q_rint((height / 100.0) * (health - 100));
			Draw_Fill(x, y + height - health_height, width, health_height, FRAGS_HEALTHBAR_MEGA_COLOR);
		}
		else if(health > 200 && health <= 250)
		{
			float health_height = (int)Q_rint((height / 100.0) * (health - 200));
			Draw_Fill(x, y, width, height, FRAGS_HEALTHBAR_MEGA_COLOR);
			Draw_Fill(x, y + height - health_height, width, health_height, FRAGS_HEALTHBAR_TWO_MEGA_COLOR);
		}
		else if(health > 250)
		{
			// This will never happen during a normal game.
			Draw_Fill(x, y, width, health_height, FRAGS_HEALTHBAR_UNNATURAL_COLOR);
		}
	}
}

#define	TEAMFRAGS_EXTRA_SPEC_NONE	0
#define TEAMFRAGS_EXTRA_SPEC_BEFORE	1
#define	TEAMFRAGS_EXTRA_SPEC_ONTOP	2
#define TEAMFRAGS_EXTRA_SPEC_NOICON 3
#define TEAMFRAGS_EXTRA_SPEC_RLTEXT 4

int TeamFrags_DrawExtraSpecInfo(int num, int px, int py, int width, int height, int style)
{
	extern mpic_t *sb_weapons[7][8]; // sbar.c
	mpic_t rl_picture = *sb_weapons[0][5];

	// Only allow this for spectators.
	if (!(cls.demoplayback || cl.spectator)
			|| style > TEAMFRAGS_EXTRA_SPEC_RLTEXT
			|| style <= TEAMFRAGS_EXTRA_SPEC_NONE
			|| !style)
	{
		return px;
	}

	// Check if the team has any RL's.
	if(sorted_teams[num].rlcount > 0)
	{
		int y_pos = py;

		//
		// Draw the RL + count depending on style.
		//

		if((style == TEAMFRAGS_EXTRA_SPEC_BEFORE || style == TEAMFRAGS_EXTRA_SPEC_NOICON)
				&& style != TEAMFRAGS_EXTRA_SPEC_RLTEXT)
		{
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px, y_pos, va("%d", sorted_teams[num].rlcount), 0);
			px += 8 + 1;
		}

		if(style != TEAMFRAGS_EXTRA_SPEC_NOICON && style != TEAMFRAGS_EXTRA_SPEC_RLTEXT)
		{
			y_pos = Q_rint(py + (height / 2.0) - (rl_picture.height / 2.0));
			Draw_SSubPic (px, y_pos, &rl_picture, 0, 0, rl_picture.width, rl_picture.height, 1);
			px += rl_picture.width + 1;
		}

		if(style == TEAMFRAGS_EXTRA_SPEC_ONTOP && style != TEAMFRAGS_EXTRA_SPEC_RLTEXT)
		{
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px - 14, y_pos, va("%d", sorted_teams[num].rlcount), 0);
		}

		if(style == TEAMFRAGS_EXTRA_SPEC_RLTEXT)
		{
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px, y_pos, va("&ce00RL&cfff%d", sorted_teams[num].rlcount), 0);
			px += 8*3 + 1;
		}
	}
	else
	{
		// If the team has no RL's just pad with nothing.
		if(style == TEAMFRAGS_EXTRA_SPEC_BEFORE)
		{
			// Draw the rl count before the rl icon.
			px += rl_picture.width + 8 + 1 + 1;
		}
		else if(style == TEAMFRAGS_EXTRA_SPEC_ONTOP)
		{
			// Draw the rl count on top of the RL instead of infront.
			px += rl_picture.width + 1;
		}
		else if(style == TEAMFRAGS_EXTRA_SPEC_NOICON)
		{
			// Only draw the rl count.
			px += 8 + 1;
		}
		else if(style == TEAMFRAGS_EXTRA_SPEC_RLTEXT)
		{
			px += 8*3 + 1;
		}
	}

	return px;
}

static qbool hud_frags_extra_spec_info	= true;
static qbool hud_frags_show_rl			= true;
static qbool hud_frags_show_armor		= true;
static qbool hud_frags_show_health		= true;
static qbool hud_frags_show_powerup		= true;
static qbool hud_frags_textonly			= false;
static qbool hud_frags_horiz_health     = false;
static qbool hud_frags_horiz_power      = false;

void Frags_OnChangeExtraSpecInfo(cvar_t *var, char *s, qbool *cancel)
{
	// Parse the extra spec info.
	hud_frags_show_rl		= Utils_RegExpMatch("RL|ALL",		s);
	hud_frags_show_armor	= Utils_RegExpMatch("ARMOR|ALL",	s);
	hud_frags_show_health	= Utils_RegExpMatch("HEALTH|ALL",	s);
	hud_frags_show_powerup	= Utils_RegExpMatch("POWERUP|ALL",	s);
	hud_frags_textonly		= Utils_RegExpMatch("TEXT",			s);
	hud_frags_horiz_health  = Utils_RegExpMatch("HMETER",       s);
	hud_frags_horiz_power   = Utils_RegExpMatch("PMETER",       s);

	hud_frags_extra_spec_info = (hud_frags_show_rl || hud_frags_show_armor || hud_frags_show_health || hud_frags_show_powerup);
}

int Frags_DrawExtraSpecInfo(player_info_t *info,
		int px, int py,
		int cell_width, int cell_height,
		int space_x, int space_y, int flip)
{
	extern mpic_t *sb_weapons[7][8];		// sbar.c ... Used for displaying the RL.
	mpic_t *rl_picture = sb_weapons[0][5];	// Picture of RL.

	float	armor_height = 0.0;
	int		armor = 0;
	int		armor_bg_color = 0;
	float	armor_bg_power = 0;
	int		weapon_width = 0;

	// Only allow this for spectators.
	if (!(cls.demoplayback || cl.spectator))
	{
		return px;
	}

	// Set width based on text or picture.
	if (hud_frags_show_armor || hud_frags_show_rl || hud_frags_show_powerup) {
		weapon_width = (!hud_frags_textonly ? rl_picture->width : 24);
	}

	// Draw health bar. (flipped)
	if(flip && hud_frags_show_health)
	{
		Frags_DrawHealthBar(info->stats[STAT_HEALTH], px, py, cell_height, 3, false, false);
		px += 3 + FRAGS_HEALTH_SPACING;
	}

	armor = info->stats[STAT_ARMOR];

	// If the player has any armor, draw it in the appropriate color.
	if(info->stats[STAT_ITEMS] & IT_ARMOR1)
	{
		armor_bg_power = 100;
		armor_bg_color = 178; // Green armor.
	}
	else if(info->stats[STAT_ITEMS] & IT_ARMOR2)
	{
		armor_bg_power = 150;
		armor_bg_color = 111; // Yellow armor.
	}
	else if(info->stats[STAT_ITEMS] & IT_ARMOR3)
	{
		armor_bg_power = 200;
		armor_bg_color = 79; // Red armor.
	}

	// Only draw the armor if the current player has one and if the style allows it.
	if(armor_bg_power && hud_frags_show_armor)
	{
		armor_height = Q_rint((cell_height / armor_bg_power) * armor);

		Draw_AlphaFill(px,												// x
				py + cell_height - (int)armor_height,			// y (draw from bottom up)
				weapon_width,									// width
				(int)armor_height,								// height
				armor_bg_color,									// color
				0.3);											// alpha
	}

	// Draw the rl if the current player has it and the style allows it.
	if(info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER && hud_frags_show_rl)
	{
		if(!hud_frags_textonly)
		{
			// Draw the rl-pic.
			Draw_SSubPic (px,
					py + Q_rint((cell_height/2.0)) - (rl_picture->height/2.0),
					rl_picture, 0, 0,
					rl_picture->width,
					rl_picture->height, 1);
		}
		else
		{
			// Just print "RL" instead.
			Draw_String(px + 12 - 8, py + Q_rint((cell_height/2.0)) - 4, "RL");
		}
	}

	// Only draw powerups is the current player has it and the style allows it.
	if(hud_frags_show_powerup)
	{
		//float powerups_x = px + (spec_extra_weapon_w / 2.0);
		float powerups_x = px + (weapon_width / 2.0);

		if(info->stats[STAT_ITEMS] & IT_INVULNERABILITY
				&& info->stats[STAT_ITEMS] & IT_INVISIBILITY
				&& info->stats[STAT_ITEMS] & IT_QUAD)
		{
			Draw_ColoredString(Q_rint(powerups_x - 10), py, "&c0ffQ&cf00P&cff0R", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_QUAD
				&& info->stats[STAT_ITEMS] & IT_INVULNERABILITY)
		{
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&c0ffQ&cf00P", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_QUAD
				&& info->stats[STAT_ITEMS] & IT_INVISIBILITY)
		{
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&c0ffQ&cff0R", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_INVULNERABILITY
				&& info->stats[STAT_ITEMS] & IT_INVISIBILITY)
		{
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&cf00P&cff0R", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_QUAD)
		{
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&c0ffQ", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_INVULNERABILITY)
		{
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&cf00P", 0);
		}
		else if(info->stats[STAT_ITEMS] & IT_INVISIBILITY)
		{
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&cff0R", 0);
		}
	}

	px += weapon_width + FRAGS_HEALTH_SPACING;

	// Draw health bar. (not flipped)
	if(!flip && hud_frags_show_health)
	{
		Frags_DrawHealthBar(info->stats[STAT_HEALTH], px, py, cell_height, 3, false, false);
		px += 3 + FRAGS_HEALTH_SPACING;
	}

	return px;
}

void Frags_DrawBackground(int px, int py, int cell_width, int cell_height,
		int space_x, int space_y, int max_name_length, int max_team_length,
		int bg_color, int shownames, int showteams, int drawBrackets, int style)
{
	int bg_width = cell_width + space_x;
	//int bg_color = Sbar_BottomColor(info);
	float bg_alpha = 0.3;

	if(style == 4
			|| style == 6
			|| style == 8)
		bg_alpha = 0;

	if(shownames)
		bg_width += max_name_length*8 + space_x;

	if(showteams)
		bg_width += max_team_length * 8 + space_x;

	if(drawBrackets)
		bg_alpha = 0.7;

	if(style == 7 || style == 8)
		bg_color = 0x4f;

	Draw_AlphaFill(px - 1, py - space_y / 2, bg_width, cell_height + space_y, bg_color, bg_alpha);

	if (drawBrackets && (style == 5 || style == 6))
	{
		Draw_Fill(px - 1, py - 1 - space_y / 2, bg_width, 1, 0x4f);

		Draw_Fill(px - 1, py - space_y / 2, 1, cell_height + space_y, 0x4f);
		Draw_Fill(px + bg_width - 1, py - 1 - space_y / 2, 1, cell_height + 1 + space_y, 0x4f);

		Draw_Fill(px - 1, py + cell_height + space_y / 2, bg_width + 1, 1, 0x4f);
	}
}

int Frags_DrawText(int px, int py,
		int cell_width, int cell_height,
		int space_x, int space_y,
		int max_name_length, int max_team_length,
		int flip, int pad,
		int shownames, int showteams,
		char* name, char* team, float scale)
{
	char _name[MAX_FRAGS_NAME + 1];
	char _team[MAX_FRAGS_NAME + 1];
	int team_length = 0;
	int name_length = 0;
	float char_size = 8 * scale;
	int y_pos;

	y_pos = Q_rint(py + (cell_height / 2.0) - 4 * scale);

	// Draw team
	if (showteams && cl.teamplay) {
		strlcpy(_team, team, clamp(max_team_length, 0, sizeof(_team)));
		team_length = strlen(_team);

		if (!flip) {
			px += space_x;
		}

		if (pad && flip) {
			px += (max_team_length - team_length) * char_size;
			Draw_SString(px, y_pos, _team, scale);
			px += team_length * char_size;
		}
		else if (pad) {
			Draw_SString(px, y_pos, _team, scale);
			px += max_team_length * char_size;
		}
		else {
			Draw_SString(px, y_pos, _team, scale);
			px += team_length * char_size;
		}

		if (flip) {
			px += space_x;
		}
	}

	if (shownames) {
		// Draw name
		strlcpy(_name, name, clamp(max_name_length, 0, sizeof(_name)));
		name_length = strlen(_name);

		if (flip && pad) {
			px += (max_name_length - name_length) * char_size;
			Draw_SString(px, y_pos, _name, scale);
			px += name_length * char_size;
		}
		else if (pad) {
			Draw_SString(px, y_pos, _name, scale);
			px += max_name_length * char_size;
		}
		else {
			Draw_SString(px, y_pos, _name, scale);
			px += name_length * char_size;
		}

		px += space_x;
	}

	return px;
}

void SCR_HUD_DrawFrags(hud_t *hud)
{
	int width = 0, height = 0;
	int x, y;
	int max_team_length = 0;
	int max_name_length = 0;

	int rows, cols, cell_width, cell_height, space_x, space_y;
	int a_rows, a_cols; // actual
	qbool any_labels_or_info;
	qbool two_lines;

	static cvar_t
		*hud_frags_cell_width = NULL,
		*hud_frags_cell_height,
		*hud_frags_rows,
		*hud_frags_cols,
		*hud_frags_space_x,
		*hud_frags_space_y,
		*hud_frags_vertical,
		*hud_frags_strip,
		*hud_frags_shownames,
		*hud_frags_teams,
		*hud_frags_padtext,
		*hud_frags_extra_spec,
		*hud_frags_fliptext,
		*hud_frags_style,
		*hud_frags_bignum,
		*hud_frags_colors_alpha,
		*hud_frags_maxname,
		*hud_frags_notintp,
		*hud_frags_fixedwidth,
		*hud_frags_scale;

	extern mpic_t *sb_weapons[7][8]; // sbar.c ... Used for displaying the RL.
	mpic_t *rl_picture;				 // Picture of RL.
	rl_picture = sb_weapons[0][5];

	if (hud_frags_cell_width == NULL) {   // first time
		char specval[256];

		hud_frags_cell_width    = HUD_FindVar(hud, "cell_width");
		hud_frags_cell_height   = HUD_FindVar(hud, "cell_height");
		hud_frags_rows          = HUD_FindVar(hud, "rows");
		hud_frags_cols          = HUD_FindVar(hud, "cols");
		hud_frags_space_x       = HUD_FindVar(hud, "space_x");
		hud_frags_space_y       = HUD_FindVar(hud, "space_y");
		hud_frags_strip         = HUD_FindVar(hud, "strip");
		hud_frags_vertical      = HUD_FindVar(hud, "vertical");
		hud_frags_shownames		= HUD_FindVar(hud, "shownames");
		hud_frags_teams			= HUD_FindVar(hud, "showteams");
		hud_frags_padtext		= HUD_FindVar(hud, "padtext");
		hud_frags_extra_spec	= HUD_FindVar(hud, "extra_spec_info");
		hud_frags_fliptext		= HUD_FindVar(hud, "fliptext");
		hud_frags_style			= HUD_FindVar(hud, "style");
		hud_frags_bignum		= HUD_FindVar(hud, "bignum");
		hud_frags_colors_alpha	= HUD_FindVar(hud, "colors_alpha");
		hud_frags_maxname		= HUD_FindVar(hud, "maxname");
		hud_frags_notintp		= HUD_FindVar(hud, "notintp");
		hud_frags_fixedwidth    = HUD_FindVar(hud, "fixedwidth");
		hud_frags_scale         = HUD_FindVar(hud, "scale");

		// Set the OnChange function for extra spec info.
		hud_frags_extra_spec->OnChange = Frags_OnChangeExtraSpecInfo;
		strlcpy(specval, hud_frags_extra_spec->string, sizeof(specval));
		Cvar_Set(hud_frags_extra_spec, specval);
	}

	// Don't draw the frags if we're in teamplay.
	if (hud_frags_notintp->value && cl.teamplay) {
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	//
	// Clamp values to be "sane".
	//
	{
		rows = hud_frags_rows->value;
		clamp(rows, 1, MAX_CLIENTS);

		cols = hud_frags_cols->value;
		clamp(cols, 1, MAX_CLIENTS);

		// Some users doesn't want to show the actual frags, just
		// extra_spec_info stuff + names.
		cell_width = hud_frags_cell_width->value * hud_frags_scale->value;
		clamp(cell_width, 0, 128);

		cell_height = hud_frags_cell_height->value * hud_frags_scale->value;
		clamp(cell_height, 7, 32);

		space_x = hud_frags_space_x->value * hud_frags_scale->value;
		clamp(space_x, 0, 128);

		space_y = hud_frags_space_y->value * hud_frags_scale->value;
		clamp(space_y, 0, 128);
	}

	if (hud_frags_strip->integer) {
		// Auto set the number of rows / cols based on the number of players.
		// (This is kinda fucked up, but I won't mess with it for the sake of backwards compability).
		if (hud_frags_vertical->value) {
			a_cols = min((n_players + rows - 1) / rows, cols);
			a_rows = min(rows, n_players);
		}
		else {
			a_rows = min((n_players + cols - 1) / cols, rows);
			a_cols = min(cols, n_players);
		}
	}
	else {
		a_rows = rows;
		a_cols = cols;
	}

	width = (a_cols * cell_width) + ((a_cols + 1) * space_x);
	height = (a_rows * cell_height) + ((a_rows + 1) * space_y);

	// Get the longest name/team name for padding.
	if (hud_frags_shownames->value || hud_frags_teams->value) {
		int cur_length = 0;
		int n;

		// If the user has set a limit on how many chars that
		// are allowed to be shown for a name/teamname.
		if (hud_frags_fixedwidth->value) {
			max_name_length = min(max(0, (int)hud_frags_maxname->value) + 1, MAX_FRAGS_NAME);
			max_team_length = min(max(0, (int)hud_frags_maxname->value) + 1, MAX_FRAGS_NAME);
		}
		else {
			for(n = 0; n < n_players; n++) {
				player_info_t *info = &cl.players[sorted_players[n].playernum];
				cur_length = strlen(info->name);

				// Name.
				if (cur_length >= max_name_length) {
					max_name_length = cur_length + 1;
				}

				cur_length = strlen(info->team);

				// Team name.
				if (cur_length >= max_team_length) {
					max_team_length = cur_length + 1;
				}
			}

			max_name_length = min(max(0, (int)hud_frags_maxname->value), max_name_length) + 1;
			max_team_length = min(max(0, (int)hud_frags_maxname->value), max_team_length) + 1;
		}

		// We need a wider box to draw in if we show the names.
		if (hud_frags_shownames->value) {
			width += (a_cols * max_name_length * 8 * hud_frags_scale->value) + ((a_cols + 1) * space_x);
		}

		if (cl.teamplay && hud_frags_teams->value) {
			width += (a_cols * max_team_length * 8 * hud_frags_scale->value) + ((a_cols + 1) * space_x);
		}
	}

	// Make room for the extra spectator stuff.
	if (hud_frags_extra_spec_info && (cls.demoplayback || cl.spectator)) {
		if (hud_frags_show_health) {
			width += a_cols * (3 + FRAGS_HEALTH_SPACING);
		}
		if (hud_frags_show_armor || hud_frags_show_powerup || hud_frags_show_rl) {
			width += a_cols * (!hud_frags_textonly ? rl_picture->width : 24 * hud_frags_scale->value);
		}
	}

	any_labels_or_info = hud_frags_shownames->value || hud_frags_teams->value || hud_frags_extra_spec_info;
	two_lines = (cls.demoplayback || cl.spectator) && any_labels_or_info && (hud_frags_horiz_health || hud_frags_horiz_power);

	if (two_lines) {
		height *= 2;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		int i = 0;
		int player_x = 0;
		int player_y = 0;
		int num = 0;
		int drawBrackets = 0;

		// The number of players that are to be visible.
		int limit = min(n_players, a_rows * a_cols);

		num = 0;
		num = (num <= limit || num >= n_players) ? 0 : num;

		//
		// Loop through all the positions that should be drawn (columns * rows or number of players).
		//
		// Start drawing player "num", usually the first player in the array, but if
		// showself_always is set this might be someone else (since we need to make sure the current
		// player is always shown).
		//
		for (i = 0; i < limit; i++)
		{
			player_info_t *info;

			// Always include the current player's position
			if (active_player_position >= 0 && i == limit - 1 && num < active_player_position) {
				num = active_player_position;
			}

			info = &cl.players[sorted_players[num].playernum]; // FIXME! johnnycz; causes crashed on some demos

			//
			// Set the coordinates where to draw the next element.
			//
			if (hud_frags_vertical->value) {
				if (i % a_rows == 0) {
					// We're drawing a new column.
					int element_width = cell_width + space_x;

					// Get the width of all the stuff that is shown, the name, frag cell and so on.
					if (hud_frags_shownames->value) {
						element_width += (max_name_length) * 8 * hud_frags_scale->value;
					}

					if (hud_frags_teams->value) {
						element_width += (max_team_length) * 8 * hud_frags_scale->value;
					}

					if (hud_frags_extra_spec_info && (cls.demoplayback || cl.spectator)) {
						element_width += (!hud_frags_textonly ? rl_picture->width : 24 * hud_frags_scale->value);
					}

					player_x = x + space_x + ((i / a_rows) * element_width);

					// New column.
					player_y = y + space_y;
				}
			}
			else if (i % a_cols == 0)
			{
				// Drawing new row.
				player_x = x + space_x;
				player_y = y + space_y + (i / a_cols) * (cell_height * (two_lines ? 2 : 1) + space_y);
			}

			drawBrackets = 0;

			// Bug fix. Before the wrong player would be higlighted
			// during qwd-playback, since you ARE the player that you're
			// being spectated (you're not a spectator).
			if(cls.demoplayback && !cl.spectator && !cls.mvdplayback) {
				drawBrackets = (sorted_players[num].playernum == cl.playernum);
			}
			else if (cls.demoplayback || cl.spectator) {
				drawBrackets = (spec_track == sorted_players[num].playernum && Cam_TrackNum() >= 0);
			}
			else {
				drawBrackets = (sorted_players[num].playernum == cl.playernum);
			}

			// Don't draw any brackets in multiview since we're tracking several players.
			if (CL_MultiviewEnabled() && !CL_MultiviewInsetEnabled()) {
				drawBrackets = 0;
			}

			if (any_labels_or_info)
			{
				// Relative x coordinate where we draw the subitems.
				int rel_player_x = player_x;
				qbool odd_column = (hud_frags_vertical->value ? (i / a_rows) : (i % a_cols));
				qbool fliptext = hud_frags_fliptext->integer == 1;

				fliptext |= hud_frags_fliptext->integer == 2 && !odd_column;
				fliptext |= hud_frags_fliptext->integer == 3 && odd_column;

				if(hud_frags_style->value >= 4 && hud_frags_style->value <= 8)
				{
					// Draw background based on the style.
					Frags_DrawBackground(player_x, player_y, cell_width, cell_height, space_x, space_y,
							max_name_length, max_team_length, Sbar_BottomColor(info),
							hud_frags_shownames->value, hud_frags_teams->value, drawBrackets,
							hud_frags_style->value);
				}

				if (fliptext)
				{
					//
					// Flip the text
					// NAME | TEAM | FRAGS | EXTRA_SPEC_INFO
					//
					if (two_lines) {
						Frags_DrawHorizontalHealthBar(info, rel_player_x, player_y, max_name_length * 8 * hud_frags_scale->value, cell_height, fliptext, hud_frags_horiz_power);
					}

					// Draw name.
					rel_player_x = Frags_DrawText(
						rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						hud_frags_shownames->value, 0,
						info->name, info->team, hud_frags_scale->value
					);

					// Draw team.
					rel_player_x = Frags_DrawText(
						rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						0, hud_frags_teams->value,
						info->name, info->team, hud_frags_scale->value
					);

					Frags_DrawColors(
						rel_player_x, player_y, cell_width, cell_height * (two_lines ? 2 : 1),
						Sbar_TopColor(info), Sbar_BottomColor(info), hud_frags_colors_alpha->value,
						info->frags,
						drawBrackets,
						hud_frags_style->value,
						hud_frags_bignum->value,
						hud_frags_scale->value
					);

					rel_player_x += cell_width + space_x;

					// Show extra information about all the players if spectating:
					// - What armor they have.
					// - How much health.
					// - If they have RL or not.
					rel_player_x = Frags_DrawExtraSpecInfo(info, rel_player_x, player_y, cell_width, cell_height,
						space_x, space_y,
						fliptext
					);
				}
				else
				{
					//
					// Don't flip the text
					// EXTRA_SPEC_INFO | FRAGS | TEAM | NAME
					//
					rel_player_x = Frags_DrawExtraSpecInfo(info, rel_player_x, player_y, cell_width, cell_height,
						space_x, space_y,
						fliptext
					);

					Frags_DrawColors(rel_player_x, player_y, cell_width, cell_height * (two_lines ? 2 : 1),
						Sbar_TopColor(info), Sbar_BottomColor(info), hud_frags_colors_alpha->value,
						info->frags,
						drawBrackets,
						hud_frags_style->value,
						hud_frags_bignum->value,
						hud_frags_scale->value
					);

					rel_player_x += cell_width + space_x;

					if (two_lines) {
						Frags_DrawHorizontalHealthBar(info, rel_player_x, player_y, max_name_length * 8 * hud_frags_scale->value, cell_height, fliptext, hud_frags_horiz_power);
					}

					// Draw team.
					rel_player_x = Frags_DrawText(rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						0, hud_frags_teams->value,
						info->name, info->team, hud_frags_scale->value
					);

					// Draw name.
					rel_player_x = Frags_DrawText(rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						hud_frags_shownames->value, 0,
						info->name, info->team, hud_frags_scale->value
					);
				}

				if(hud_frags_vertical->value)
				{
					// Next row.
					player_y += cell_height * (two_lines ? 2 : 1) + space_y;
				}
				else
				{
					// Next column.
					player_x = rel_player_x + space_x;
				}
			}
			else
			{
				// Only showing the frags, no names or extra spec info.

				Frags_DrawColors(
					player_x, player_y, cell_width, cell_height,
					Sbar_TopColor(info), Sbar_BottomColor(info), hud_frags_colors_alpha->value,
					info->frags,
					drawBrackets,
					hud_frags_style->value,
					hud_frags_bignum->value,
					hud_frags_scale->value
				);

				if (hud_frags_vertical->value)
				{
					// Next row.
					player_y += cell_height + space_y;
				}
				else
				{
					// Next column.
					player_x += cell_width + space_x;
				}
			}

			// Next player.
			num++;
		}
	}
}

void SCR_HUD_DrawTeamFrags(hud_t *hud)
{
	int width = 0, height = 0;
	int x, y;
	int max_team_length = 0, num = 0;
	int rows, cols, cell_width, cell_height, space_x, space_y;
	int a_rows, a_cols; // actual

	static cvar_t
		*hud_teamfrags_cell_width,
		*hud_teamfrags_cell_height,
		*hud_teamfrags_rows,
		*hud_teamfrags_cols,
		*hud_teamfrags_space_x,
		*hud_teamfrags_space_y,
		*hud_teamfrags_vertical,
		*hud_teamfrags_strip,
		*hud_teamfrags_shownames,
		*hud_teamfrags_fliptext,
		*hud_teamfrags_padtext,
		*hud_teamfrags_style,
		*hud_teamfrags_extra_spec,
		*hud_teamfrags_onlytp,
		*hud_teamfrags_bignum,
		*hud_teamfrags_colors_alpha,
		*hud_teamfrags_scale;

	extern mpic_t *sb_weapons[7][8]; // sbar.c
	mpic_t rl_picture = *sb_weapons[0][5];

	if (hud_teamfrags_cell_width == 0)    // first time
	{
		hud_teamfrags_cell_width    = HUD_FindVar(hud, "cell_width");
		hud_teamfrags_cell_height   = HUD_FindVar(hud, "cell_height");
		hud_teamfrags_rows          = HUD_FindVar(hud, "rows");
		hud_teamfrags_cols          = HUD_FindVar(hud, "cols");
		hud_teamfrags_space_x       = HUD_FindVar(hud, "space_x");
		hud_teamfrags_space_y       = HUD_FindVar(hud, "space_y");
		hud_teamfrags_strip         = HUD_FindVar(hud, "strip");
		hud_teamfrags_vertical      = HUD_FindVar(hud, "vertical");
		hud_teamfrags_shownames		= HUD_FindVar(hud, "shownames");
		hud_teamfrags_fliptext		= HUD_FindVar(hud, "fliptext");
		hud_teamfrags_padtext		= HUD_FindVar(hud, "padtext");
		hud_teamfrags_style			= HUD_FindVar(hud, "style");
		hud_teamfrags_extra_spec	= HUD_FindVar(hud, "extra_spec_info");
		hud_teamfrags_onlytp		= HUD_FindVar(hud, "onlytp");
		hud_teamfrags_bignum		= HUD_FindVar(hud, "bignum");
		hud_teamfrags_colors_alpha	= HUD_FindVar(hud, "colors_alpha");
		hud_teamfrags_scale         = HUD_FindVar(hud, "scale");
	}

	// Don't draw the frags if we're not in teamplay.
	if (hud_teamfrags_onlytp->value && !cl.teamplay)
	{
		HUD_PrepareDraw(hud, width, height, &x, &y);
		return;
	}

	rows = hud_teamfrags_rows->value;
	clamp(rows, 1, MAX_CLIENTS);
	cols = hud_teamfrags_cols->value;
	clamp(cols, 1, MAX_CLIENTS);
	cell_width = hud_teamfrags_cell_width->value;
	clamp(cell_width, 28, 128);
	cell_height = hud_teamfrags_cell_height->value;
	clamp(cell_height, 7, 32);
	space_x = hud_teamfrags_space_x->value;
	clamp(space_x, 0, 128);
	space_y = hud_teamfrags_space_y->value;
	clamp(space_y, 0, 128);

	if (hud_teamfrags_strip->value)
	{
		if (hud_teamfrags_vertical->value)
		{
			a_cols = min((n_teams+rows-1) / rows, cols);
			a_rows = min(rows, n_teams);
		}
		else
		{
			a_rows = min((n_teams+cols-1) / cols, rows);
			a_cols = min(cols, n_teams);
		}
	}
	else
	{
		a_rows = rows;
		a_cols = cols;
	}

	width  = a_cols * cell_width  + (a_cols+1)*space_x;
	height = a_rows * cell_height + (a_rows+1)*space_y;

	// Get the longest team name for padding.
	if(hud_teamfrags_shownames->value || hud_teamfrags_extra_spec->value)
	{
		int rlcount_width = 0;

		int cur_length = 0;
		int n;

		for(n=0; n < n_teams; n++)
		{
			if(hud_teamfrags_shownames->value)
			{
				cur_length = strlen(sorted_teams[n].name);

				// Team name
				if(cur_length >= max_team_length)
				{
					max_team_length = cur_length + 1;
				}
			}
		}

		// Calculate the length of the extra spec info.
		if(hud_teamfrags_extra_spec->value && (cls.demoplayback || cl.spectator))
		{
			if(hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_BEFORE)
			{
				// Draw the rl count before the rl icon.
				rlcount_width = rl_picture.width + 8 + 1 + 1;
			}
			else if(hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_ONTOP)
			{
				// Draw the rl count on top of the RL instead of infront.
				rlcount_width = rl_picture.width + 1;
			}
			else if(hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_NOICON)
			{
				// Only draw the rl count.
				rlcount_width = 8 + 1;
			}
			else if(hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_RLTEXT)
			{
				rlcount_width = 8*3 + 1;
			}
		}

		width += a_cols*max_team_length*8 + (a_cols+1)*space_x + a_cols*rlcount_width;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y))
	{
		int i;
		int px = 0;
		int py = 0;
		int drawBrackets;
		int limit = min(n_teams, a_rows*a_cols);

		for (i=0; i < limit; i++)
		{
			if (hud_teamfrags_vertical->value)
			{
				if (i % a_rows == 0)
				{
					px = x + space_x + (i/a_rows) * (cell_width+space_x);
					py = y + space_y;
				}
			}
			else
			{
				if (i % a_cols == 0)
				{
					px = x + space_x;
					py = y + space_y + (i/a_cols) * (cell_height+space_y);
				}
			}

			drawBrackets = 0;

			// Bug fix. Before the wrong player would be higlighted
			// during qwd-playback, since you ARE the player that you're
			// being spectated.
			if(cls.demoplayback && !cl.spectator && !cls.mvdplayback)
			{
				// QWD Playback.
				if (!strcmp(sorted_teams[num].name, cl.players[cl.playernum].team))
				{
					drawBrackets = 1;
				}
			}
			else if (cls.demoplayback || cl.spectator)
			{
				// MVD playback / spectating.
				if (!strcmp(cl.players[spec_track].team, sorted_teams[num].name) && Cam_TrackNum() >= 0)
				{
					drawBrackets = 1;
				}
			}
			else
			{
				// Normal player.
				if (!strcmp(sorted_teams[num].name, cl.players[cl.playernum].team))
				{
					drawBrackets = 1;
				}
			}

			if (cl_multiview.value && CL_MultiviewCurrentView() != 1 )  // Only draw bracket for first view, might make todo below unnecessary
			{
				// TODO: Check if "track team" is set, if it is then draw brackets around that team.
				//cl.players[nPlayernum]

				drawBrackets = 0;
			}

			if(hud_teamfrags_shownames->value || hud_teamfrags_extra_spec->value)
			{
				int _px = px;
				qbool text_lhs = hud_teamfrags_fliptext->integer == 1;
				text_lhs |= hud_teamfrags_fliptext->integer == 2 && ((i % a_cols) % 2 == 0);
				text_lhs |= hud_teamfrags_fliptext->integer == 3 && ((i % a_cols) % 2 == 1);

				// Draw a background if the style tells us to.
				if(hud_teamfrags_style->value >= 4 && hud_teamfrags_style->value <= 8)
				{
					Frags_DrawBackground(px, py, cell_width, cell_height, space_x, space_y,
							0, max_team_length, sorted_teams[num].bottom,
							0, hud_teamfrags_shownames->value, drawBrackets,
							hud_teamfrags_style->value);
				}

				// Draw the text on the left or right side of the score?
				if (text_lhs)
				{
					// Draw team.
					_px = Frags_DrawText(
						_px, py, cell_width, cell_height,
						space_x, space_y, 0, max_team_length,
						true, hud_teamfrags_padtext->value,
						0, hud_teamfrags_shownames->value,
						"", sorted_teams[num].name, hud_teamfrags_scale->value
					);

					Frags_DrawColors(
						_px, py, cell_width, cell_height,
						sorted_teams[num].top,
						sorted_teams[num].bottom,
						hud_teamfrags_colors_alpha->value,
						sorted_teams[num].frags,
						drawBrackets,
						hud_teamfrags_style->value,
						hud_teamfrags_bignum->value,
						hud_teamfrags_scale->value
					);

					_px += cell_width + space_x;

					// Draw the rl if the current player has it and the style allows it.
					_px = TeamFrags_DrawExtraSpecInfo(num, _px, py, cell_width, cell_height, hud_teamfrags_extra_spec->value);
				}
				else
				{
					// Draw the rl if the current player has it and the style allows it.
					_px = TeamFrags_DrawExtraSpecInfo(num, _px, py, cell_width, cell_height, hud_teamfrags_extra_spec->value);

					Frags_DrawColors(
						_px, py, cell_width, cell_height,
						sorted_teams[num].top,
						sorted_teams[num].bottom,
						hud_teamfrags_colors_alpha->value,
						sorted_teams[num].frags,
						drawBrackets,
						hud_teamfrags_style->value,
						hud_teamfrags_bignum->value,
						hud_teamfrags_scale->value
					);

					_px += cell_width + space_x;

					// Draw team.
					_px = Frags_DrawText(
						_px, py, cell_width, cell_height,
						space_x, space_y, 0, max_team_length,
						false, hud_teamfrags_padtext->value,
						0, hud_teamfrags_shownames->value,
						"", sorted_teams[num].name,
						hud_teamfrags_scale->value
					);
				}

				if (hud_teamfrags_vertical->value) {
					py += cell_height + space_y;
				}
				else {
					px = _px + space_x;
				}
			}
			else {
				Frags_DrawColors(px, py, cell_width, cell_height,
					sorted_teams[num].top,
					sorted_teams[num].bottom,
					hud_teamfrags_colors_alpha->value,
					sorted_teams[num].frags,
					drawBrackets,
					hud_teamfrags_style->value,
					hud_teamfrags_bignum->value,
					hud_teamfrags_scale->value
				);

				if (hud_teamfrags_vertical->value) {
					py += cell_height + space_y;
				}
				else {
					px += cell_width + space_x;
				}
			}
			num ++;
		}
	}
}

#define TEMPHUD_NAME "_temphud"
#define TEMPHUD_FULLPATH "configs/"TEMPHUD_NAME".cfg"

// will check if user wants to un/load external MVD HUD automatically
void HUD_AutoLoad_MVD(int autoload) {
	char *cfg_suffix = "custom";
	extern cvar_t scr_fov;
	extern cvar_t scr_newHud;
	extern void Cmd_Exec_f (void);
	extern void DumpConfig(char *name);

	if (autoload && cls.mvdplayback) {
		// Turn autohud ON here

		Com_DPrintf("Loading MVD Hud\n");
		// Store current settings.
		if (!autohud.active)
		{
			extern cvar_t cfg_save_cmdline, cfg_save_cvars, cfg_save_cmds, cfg_save_aliases, cfg_save_binds;

			// Save old cfg_save values so that we don't screw the users
			// settings when saving the temp config.
			int old_cmdline = cfg_save_cmdline.value;
			int old_cvars	= cfg_save_cvars.value;
			int old_cmds	= cfg_save_cmds.value;
			int old_aliases = cfg_save_aliases.value;
			int old_binds	= cfg_save_binds.value;

			autohud.old_fov = (int) scr_fov.value;
			autohud.old_multiview = (int) cl_multiview.value;
			autohud.old_newhud = (int) scr_newHud.value;

			// Make sure everything current settings are saved.
			Cvar_SetValue(&cfg_save_cmdline,	1);
			Cvar_SetValue(&cfg_save_cvars,		1);
			Cvar_SetValue(&cfg_save_cmds,		1);
			Cvar_SetValue(&cfg_save_aliases,	1);
			Cvar_SetValue(&cfg_save_binds,		1);

			// Save a temporary config.
			DumpConfig(TEMPHUD_NAME".cfg");

			Cvar_SetValue(&cfg_save_cmdline,	old_cmdline);
			Cvar_SetValue(&cfg_save_cvars,		old_cvars);
			Cvar_SetValue(&cfg_save_cmds,		old_cmds);
			Cvar_SetValue(&cfg_save_aliases,	old_aliases);
			Cvar_SetValue(&cfg_save_binds,		old_binds);
		}

		// load MVD HUD config
		switch ((int) autoload) {
			case 1: // load 1on1 or 4on4 or custom according to $matchtype
				if (!strncmp(Macro_MatchType(), "duel", 4)) {
					cfg_suffix = "1on1";
				} else if (!strncmp(Macro_MatchType(), "4on4", 4)) {
					cfg_suffix = "4on4";
				} else {
					cfg_suffix = "custom";
				}
				break;
			default:
			case 2:
				cfg_suffix = "custom";
				break;
		}

		Cbuf_AddText(va("exec cfg/mvdhud_%s.cfg\n", cfg_suffix));

		autohud.active = true;
		return;
	}

	if ((!cls.mvdplayback || !autoload) && autohud.active) {
		// either user decided to turn mvd autohud off or mvd playback is over
		// -> Turn autohud OFF here
		FILE *tempfile;
		char *fullname = va("%s/ezquake/"TEMPHUD_FULLPATH, com_basedir);

		Com_DPrintf("Unloading MVD Hud\n");
		// load stored settings
		Cvar_SetValue(&scr_fov, autohud.old_fov);
		Cvar_SetValue(&cl_multiview, autohud.old_multiview);
		Cvar_SetValue(&scr_newHud, autohud.old_newhud);
		//Cmd_TokenizeString("exec "TEMPHUD_FULLPATH);
		Cmd_TokenizeString("cfg_load "TEMPHUD_FULLPATH);
		Cmd_Exec_f();

		// delete temp config with hud_* settings
		if ((tempfile = fopen(fullname, "rb")) && (fclose(tempfile) != EOF))
			unlink(fullname);

		autohud.active = false;
		return;
	}
}

void OnAutoHudChange(cvar_t *var, char *value, qbool *cancel) {
	HUD_AutoLoad_MVD(Q_atoi(value));
}

// Is run when a new map is loaded.
void HUD_NewMap(void) {
#if defined(WITH_PNG)
	HUD_NewRadarMap();
#endif // WITH_PNG

	autohud_loaded = false;
}

qbool HUD_ShowInDemoplayback(int val)
{
	if(!cl.teamplay && val == HUD_SHOW_ONLY_IN_TEAMPLAY)
	{
		return false;
	}
	else if(!cls.demoplayback && val == HUD_SHOW_ONLY_IN_DEMOPLAYBACK)
	{
		return false;
	}
	else if(!cl.teamplay && !cls.demoplayback
			&& val == HUD_SHOW_ONLY_IN_TEAMPLAY + HUD_SHOW_ONLY_IN_DEMOPLAYBACK)
	{
		return false;
	}

	return true;
}

// Team hold filters.
static qbool teamhold_show_pent		= false;
static qbool teamhold_show_quad		= false;
static qbool teamhold_show_ring		= false;
static qbool teamhold_show_suit		= false;
static qbool teamhold_show_rl		= false;
static qbool teamhold_show_lg		= false;
static qbool teamhold_show_gl		= false;
static qbool teamhold_show_sng		= false;
static qbool teamhold_show_mh		= false;
static qbool teamhold_show_ra		= false;
static qbool teamhold_show_ya		= false;
static qbool teamhold_show_ga		= false;

void TeamHold_DrawBars(int x, int y, int width, int height,
		float team1_percent, float team2_percent,
		int team1_color, int team2_color,
		float opacity)
{
	int team1_width = 0;
	int team2_width = 0;
	int bar_height = 0;

	bar_height = Q_rint (height/2.0);
	team1_width = (int) (width * team1_percent);
	team2_width = (int) (width * team2_percent);

	clamp(team1_width, 0, width);
	clamp(team2_width, 0, width);

	Draw_AlphaFill(x, y, team1_width, bar_height, team1_color, opacity);

	y += bar_height;

	Draw_AlphaFill(x, y, team2_width, bar_height, team2_color, opacity);
}

static void TeamHold_DrawPercentageBar(
	int x, int y, int width, int height,
	float team1_percent, float team2_percent,
	int team1_color, int team2_color,
	int show_text, int vertical,
	int vertical_text, float opacity, float scale
)
{
	int _x, _y;
	int _width, _height;

	if(vertical)
	{
		//
		// Draw vertical.
		//

		// Team 1.
		_x = x;
		_y = y;
		_width = max(0, width);
		_height = Q_rint(height * team1_percent);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team1_color, opacity);

		// Team 2.
		_x = x;
		_y = Q_rint(y + (height * team1_percent));
		_width = max(0, width);
		_height = Q_rint(height * team2_percent);
		_height = max(0, _height);

		Draw_AlphaFill(_x, _y, _width, _height, team2_color, opacity);

		// Show the percentages in numbers also.
		if(show_text)
		{
			// TODO: Move this to a separate function (since it's prett much copy and paste for both teams).
			// Team 1.
			if(team1_percent > 0.05)
			{
				if(vertical_text)
				{
					int percent = 0;
					int percent10 = 0;
					int percent100 = 0;

					_x = x + (width / 2) - 4 * scale;
					_y = Q_rint(y + (height * team1_percent)/2 - 8 * 1.5 * scale);

					percent = Q_rint(100 * team1_percent);

					if ((percent100 = percent / 100))
					{
						Draw_SString(_x, _y, va("%d", percent100), scale);
						_y += 8 * scale;
					}

					if ((percent10 = percent / 10))
					{
						Draw_SString(_x, _y, va("%d", percent10), scale);
						_y += 8 * scale;
					}

					Draw_SString(_x, _y, va("%d", percent % 10), scale);
					_y += 8 * scale;

					Draw_SString(_x, _y, "%", scale);
				}
				else
				{
					_x = x + (width / 2) - 8 * 1.5 * scale;
					_y = Q_rint(y + (height * team1_percent)/2 - 8 * scale * 0.5);
					Draw_SString(_x, _y, va("%2.0f%%", 100 * team1_percent), scale);
				}
			}

			// Team 2.
			if(team2_percent > 0.05)
			{
				if(vertical_text)
				{
					int percent = 0;
					int percent10 = 0;
					int percent100 = 0;

					_x = x + (width / 2) - 4;
					_y = Q_rint(y + (height * team1_percent) + (height * team2_percent)/2 - 12);

					percent = Q_rint(100 * team2_percent);

					if((percent100 = percent / 100))
					{
						Draw_String(_x, _y, va("%d", percent100));
						_y += 8;
					}

					if((percent10 = percent / 10))
					{
						Draw_String(_x, _y, va("%d", percent10));
						_y += 8;
					}

					Draw_String(_x, _y, va("%d", percent % 10));
					_y += 8;

					Draw_String(_x, _y, "%");
				}
				else
				{
					_x = x + (width / 2) - 12;
					_y = Q_rint(y + (height * team1_percent) + (height * team2_percent)/2 - 4);
					Draw_String(_x, _y, va("%2.0f%%", 100 * team2_percent));
				}
			}
		}
	}
	else
	{
		//
		// Draw horizontal.
		//

		// Team 1.
		_x = x;
		_y = y;
		_width = Q_rint(width * team1_percent);
		_width = max(0, _width);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team1_color, opacity);

		// Team 2.
		_x = Q_rint(x + (width * team1_percent));
		_y = y;
		_width = Q_rint(width * team2_percent);
		_width = max(0, _width);
		_height = max(0, height);

		Draw_AlphaFill(_x, _y, _width, _height, team2_color, opacity);

		// Show the percentages in numbers also.
		if(show_text)
		{
			// Team 1.
			if(team1_percent > 0.05)
			{
				_x = Q_rint(x + (width * team1_percent)/2 - 8 * scale);
				_y = y + (height / 2) - 4 * scale;
				Draw_SString(_x, _y, va("%2.0f%%", 100 * team1_percent), scale);
			}

			// Team 2.
			if(team2_percent > 0.05)
			{
				_x = Q_rint(x + (width * team1_percent) + (width * team2_percent)/2 - 8 * scale);
				_y = y + (height / 2) - 4 * scale;
				Draw_SString(_x, _y, va("%2.0f%%", 100 * team2_percent), scale);
			}
		}
	}
}

void SCR_HUD_DrawTeamHoldBar(hud_t *hud)
{
	int x, y;
	int height = 8;
	int width = 0;
	float team1_percent = 0;
	float team2_percent = 0;

	static cvar_t
		*hud_teamholdbar_style = NULL,
		*hud_teamholdbar_opacity,
		*hud_teamholdbar_width,
		*hud_teamholdbar_height,
		*hud_teamholdbar_vertical,
		*hud_teamholdbar_show_text,
		*hud_teamholdbar_onlytp,
		*hud_teamholdbar_vertical_text,
		*hud_teamholdbar_scale;

	if (hud_teamholdbar_style == NULL)    // first time
	{
		hud_teamholdbar_style               = HUD_FindVar(hud, "style");
		hud_teamholdbar_opacity             = HUD_FindVar(hud, "opacity");
		hud_teamholdbar_width               = HUD_FindVar(hud, "width");
		hud_teamholdbar_height              = HUD_FindVar(hud, "height");
		hud_teamholdbar_vertical            = HUD_FindVar(hud, "vertical");
		hud_teamholdbar_show_text           = HUD_FindVar(hud, "show_text");
		hud_teamholdbar_onlytp              = HUD_FindVar(hud, "onlytp");
		hud_teamholdbar_vertical_text       = HUD_FindVar(hud, "vertical_text");
		hud_teamholdbar_scale               = HUD_FindVar(hud, "scale");
	}

	height = max(1, hud_teamholdbar_height->value);
	width = max(0, hud_teamholdbar_width->value);

	// Don't show when not in teamplay/demoplayback.
	if (!HUD_ShowInDemoplayback(hud_teamholdbar_onlytp->value)) {
		HUD_PrepareDraw(hud, width , height, &x, &y);
		return;
	}

	if (HUD_PrepareDraw(hud, width , height, &x, &y)) {
		// We need something to work with.
		if(stats_grid != NULL)
		{
			// Check if we have any hold values to calculate from.
			if(stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count > 0)
			{
				// Calculate the percentage for the two teams for the "team strength bar".
				team1_percent = ((float)stats_grid->teams[STATS_TEAM1].hold_count) / (stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count);
				team2_percent = ((float)stats_grid->teams[STATS_TEAM2].hold_count) / (stats_grid->teams[STATS_TEAM1].hold_count + stats_grid->teams[STATS_TEAM2].hold_count);

				team1_percent = fabs(max(0, team1_percent));
				team2_percent = fabs(max(0, team2_percent));
			}
			else
			{
				Draw_AlphaFill(x, y, hud_teamholdbar_width->value, height, 0, hud_teamholdbar_opacity->value*0.5);
				return;
			}

			// Draw the percentage bar.
			TeamHold_DrawPercentageBar(
				x, y, width, height,
				team1_percent, team2_percent,
				stats_grid->teams[STATS_TEAM1].color,
				stats_grid->teams[STATS_TEAM2].color,
				hud_teamholdbar_show_text->value,
				hud_teamholdbar_vertical->value,
				hud_teamholdbar_vertical_text->value,
				hud_teamholdbar_opacity->value,
				hud_teamholdbar_scale->value
			);
		}
		else
		{
			// If there's no stats grid available we don't know what to show, so just show a black frame.
			Draw_AlphaFill(x, y, hud_teamholdbar_width->value, height, 0, hud_teamholdbar_opacity->value * 0.5);
		}
	}
}

void SCR_Hud_StackBar(hud_t* hud)
{
	int x, y;
	int height = 8;
	int width = 0;

	static cvar_t
		*hud_stackbar_style = NULL,
		*hud_stackbar_opacity,
		*hud_stackbar_width,
		*hud_stackbar_height,
		*hud_stackbar_vertical,
		*hud_stackbar_show_text,
		*hud_stackbar_vertical_text,
		*hud_stackbar_onlytp,
		*hud_stackbar_scale;

	if (hud_stackbar_style == NULL)    // first time
	{
		hud_stackbar_style               = HUD_FindVar(hud, "style");
		hud_stackbar_opacity             = HUD_FindVar(hud, "opacity");
		hud_stackbar_width               = HUD_FindVar(hud, "width");
		hud_stackbar_height              = HUD_FindVar(hud, "height");
		hud_stackbar_vertical            = HUD_FindVar(hud, "vertical");
		hud_stackbar_show_text           = HUD_FindVar(hud, "show_text");
		hud_stackbar_vertical_text       = HUD_FindVar(hud, "vertical_text");
		hud_stackbar_onlytp              = HUD_FindVar(hud, "onlytp");
		hud_stackbar_scale               = HUD_FindVar(hud, "scale");
	}

	height = max(1, hud_stackbar_height->value);
	width = max(0, hud_stackbar_width->value);

	// Don't show when not in teamplay/demoplayback.
	if (!HUD_ShowInDemoplayback(hud_stackbar_onlytp->value)) {
		HUD_PrepareDraw(hud, width , height, &x, &y);
		return;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		// Get stack for each team, convert to %
		int stack1 = sorted_teams[0].stack;
		int stack2 = sorted_teams[1].stack;

		if (stack1 + stack2 > 0) {
			float percent1 = (stack1 * 1.0f) / (stack1 + stack2);

			TeamHold_DrawPercentageBar(
				x, y, width, height,
				percent1, 1.0f - percent1,
				sorted_teams[0].bottom,
				sorted_teams[1].bottom,
				hud_stackbar_show_text->value,
				hud_stackbar_vertical->value,
				hud_stackbar_vertical_text->value,
				hud_stackbar_opacity->value,
				hud_stackbar_scale->value
			);
		}
		else {
			Draw_AlphaFill(x, y, hud_stackbar_width->value, height, 0, hud_stackbar_opacity->value * 0.5);
		}
	}
}

void ItemsClock_OnChangeItemFilter(cvar_t* var, char *s, qbool *cancel)
{
	int new_filter = 0;

	new_filter |= Utils_RegExpMatch("RL", s) ? IT_ROCKET_LAUNCHER : 0;
	new_filter |= Utils_RegExpMatch("QUAD",	s) ? IT_QUAD : 0;
	new_filter |= Utils_RegExpMatch("RING",	s) ? IT_INVISIBILITY : 0;
	new_filter |= Utils_RegExpMatch("PENT",	s) ? IT_INVULNERABILITY : 0;
	new_filter |= Utils_RegExpMatch("SUIT", s) ? IT_SUIT : 0;
	new_filter |= Utils_RegExpMatch("LG", s) ? IT_LIGHTNING : 0;
	new_filter |= Utils_RegExpMatch("GL", s) ? IT_GRENADE_LAUNCHER : 0;
	new_filter |= Utils_RegExpMatch("SNG", s) ? IT_SUPER_NAILGUN : 0;
	new_filter |= Utils_RegExpMatch("MH", s) ? IT_SUPERHEALTH : 0;
	new_filter |= Utils_RegExpMatch("RA", s) ? IT_ARMOR3 : 0;
	new_filter |= Utils_RegExpMatch("YA", s) ? IT_ARMOR2 : 0;
	new_filter |= Utils_RegExpMatch("GA", s) ? IT_ARMOR1 : 0;

	itemsclock_filter = new_filter;
}

void TeamHold_OnChangeItemFilterInfo(cvar_t *var, char *s, qbool *cancel)
{
	char *start = s;
	char *end = start;
	int order = 0;

	// Parse the item filter.
	teamhold_show_rl		= Utils_RegExpMatch("RL",	s);
	teamhold_show_quad		= Utils_RegExpMatch("QUAD",	s);
	teamhold_show_ring		= Utils_RegExpMatch("RING",	s);
	teamhold_show_pent		= Utils_RegExpMatch("PENT",	s);
	teamhold_show_suit		= Utils_RegExpMatch("SUIT",	s);
	teamhold_show_lg		= Utils_RegExpMatch("LG",	s);
	teamhold_show_gl		= Utils_RegExpMatch("GL",	s);
	teamhold_show_sng		= Utils_RegExpMatch("SNG",	s);
	teamhold_show_mh		= Utils_RegExpMatch("MH",	s);
	teamhold_show_ra		= Utils_RegExpMatch("RA",	s);
	teamhold_show_ya		= Utils_RegExpMatch("YA",	s);
	teamhold_show_ga		= Utils_RegExpMatch("GA",	s);

	// Reset the ordering of the items.
	StatsGrid_ResetHoldItemsOrder();

	// Trim spaces from the start of the word.
	while (*start && *start == ' ')
	{
		start++;
	}

	end = start;

	// Go through the string word for word and set a
	// rising order for each hold item based on their
	// order in the string.
	while (*end)
	{
		if (*end != ' ')
		{
			// Not at the end of the word yet.
			end++;
			continue;
		}
		else
		{
			// We've found a word end.
			char temp[256];

			// Try matching the current word with a hold item
			// and set it's ordering according to it's placement
			// in the string.
			strlcpy (temp, start, min(end - start, sizeof(temp)));
			StatsGrid_SetHoldItemOrder(temp, order);
			order++;

			// Get rid of any additional spaces.
			while (*end && *end == ' ')
			{
				end++;
			}

			// Start trying to find a new word.
			start = end;
		}
	}

	// Order the hold items.
	StatsGrid_SortHoldItems();
}

#define HUD_TEAMHOLDINFO_STYLE_TEAM_NAMES		0
#define HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS		1
#define HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS2	2

void SCR_HUD_DrawTeamHoldInfo(hud_t *hud)
{
	int i;
	int x, y;
	int width, height;

	static cvar_t
		*hud_teamholdinfo_style = NULL,
		*hud_teamholdinfo_opacity,
		*hud_teamholdinfo_width,
		*hud_teamholdinfo_height,
		*hud_teamholdinfo_onlytp,
		*hud_teamholdinfo_itemfilter,
		*hud_teamholdinfo_scale;

	if (hud_teamholdinfo_style == NULL)    // first time
	{
		char val[256];

		hud_teamholdinfo_style      = HUD_FindVar(hud, "style");
		hud_teamholdinfo_opacity    = HUD_FindVar(hud, "opacity");
		hud_teamholdinfo_width      = HUD_FindVar(hud, "width");
		hud_teamholdinfo_height     = HUD_FindVar(hud, "height");
		hud_teamholdinfo_onlytp     = HUD_FindVar(hud, "onlytp");
		hud_teamholdinfo_itemfilter = HUD_FindVar(hud, "itemfilter");
		hud_teamholdinfo_scale      = HUD_FindVar(hud, "scale");

		// Unecessary to parse the item filter string on each frame.
		hud_teamholdinfo_itemfilter->OnChange = TeamHold_OnChangeItemFilterInfo;

		// Parse the item filter the first time (trigger the OnChange function above).
		strlcpy (val, hud_teamholdinfo_itemfilter->string, sizeof(val));
		Cvar_Set (hud_teamholdinfo_itemfilter, val);
	}

	// Get the height based on how many items we have, or what the user has set it to.
	height = max(0, hud_teamholdinfo_height->value);
	width = max(0, hud_teamholdinfo_width->value);

	// Don't show when not in teamplay/demoplayback.
	if(!HUD_ShowInDemoplayback(hud_teamholdinfo_onlytp->value))
	{
		HUD_PrepareDraw(hud, width , height, &x, &y);
		return;
	}

	// We don't have anything to show.
	if(stats_important_ents == NULL || stats_grid == NULL)
	{
		HUD_PrepareDraw(hud, width , height, &x, &y);
		return;
	}

	if (HUD_PrepareDraw(hud, width , height, &x, &y))
	{
		int _y = 0;

		_y = y;

		// Go through all the items and print the stats for them.
		for(i = 0; i < stats_important_ents->count; i++)
		{
			float team1_percent;
			float team2_percent;
			int team1_hold_count = 0;
			int team2_hold_count = 0;
			int names_width = 0;

			// Don't draw outside the specified height.
			if((_y - y) + 8 * hud_teamholdinfo_scale->value > height)
			{
				break;
			}

			// If the item isn't of the specified type, then skip it.
			if(!(	(teamhold_show_rl	&& !strncmp(stats_important_ents->list[i].name, "RL",	2))
						||	(teamhold_show_quad	&& !strncmp(stats_important_ents->list[i].name, "QUAD", 4))
						||	(teamhold_show_ring	&& !strncmp(stats_important_ents->list[i].name, "RING", 4))
						||	(teamhold_show_pent	&& !strncmp(stats_important_ents->list[i].name, "PENT", 4))
						||	(teamhold_show_suit	&& !strncmp(stats_important_ents->list[i].name, "SUIT", 4))
						||	(teamhold_show_lg	&& !strncmp(stats_important_ents->list[i].name, "LG",	2))
						||	(teamhold_show_gl	&& !strncmp(stats_important_ents->list[i].name, "GL",	2))
						||	(teamhold_show_sng	&& !strncmp(stats_important_ents->list[i].name, "SNG",	3))
						||	(teamhold_show_mh	&& !strncmp(stats_important_ents->list[i].name, "MH",	2))
						||	(teamhold_show_ra	&& !strncmp(stats_important_ents->list[i].name, "RA",	2))
						||	(teamhold_show_ya	&& !strncmp(stats_important_ents->list[i].name, "YA",	2))
						||	(teamhold_show_ga	&& !strncmp(stats_important_ents->list[i].name, "GA",	2))
			    ))
			{
				continue;
			}

			// Calculate the width of the longest item name so we can use it for padding.
			names_width = 8 * (stats_important_ents->longest_name + 1) * hud_teamholdinfo_scale->value;

			// Calculate the percentages of this item that the two teams holds.
			team1_hold_count = stats_important_ents->list[i].teams_hold_count[STATS_TEAM1];
			team2_hold_count = stats_important_ents->list[i].teams_hold_count[STATS_TEAM2];

			team1_percent = ((float)team1_hold_count) / (team1_hold_count + team2_hold_count);
			team2_percent = ((float)team2_hold_count) / (team1_hold_count + team2_hold_count);

			team1_percent = fabs(max(0, team1_percent));
			team2_percent = fabs(max(0, team2_percent));

			// Write the name of the item.
			Draw_SColoredStringBasic(x, _y, va("&cff0%s:", stats_important_ents->list[i].name), 0, hud_teamholdinfo_scale->value);

			if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_TEAM_NAMES)
			{
				//
				// Prints the team name that holds the item.
				//
				if(team1_percent > team2_percent)
				{
					Draw_SColoredStringBasic(x + names_width, _y, stats_important_ents->teams[STATS_TEAM1].name, 0, hud_teamholdinfo_scale->value);
				}
				else if(team1_percent < team2_percent)
				{
					Draw_SColoredStringBasic(x + names_width, _y, stats_important_ents->teams[STATS_TEAM2].name, 0, hud_teamholdinfo_scale->value);
				}
			}
			else if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS)
			{
				//
				// Show a percenteage bar for the item.
				//
				TeamHold_DrawPercentageBar(
					x + names_width, _y,
					Q_rint(hud_teamholdinfo_width->value - names_width), 8,
					team1_percent, team2_percent,
					stats_important_ents->teams[STATS_TEAM1].color,
					stats_important_ents->teams[STATS_TEAM2].color,
					0, // Don't show percentage values, get's too cluttered.
					false,
					false,
					hud_teamholdinfo_opacity->value,
					hud_teamholdinfo_scale->value
				);
			}
			else if (hud_teamholdinfo_style->value == HUD_TEAMHOLDINFO_STYLE_PERCENT_BARS2)
			{
				TeamHold_DrawBars(x + names_width, _y,
						Q_rint(hud_teamholdinfo_width->value - names_width), 8,
						team1_percent, team2_percent,
						stats_important_ents->teams[STATS_TEAM1].color,
						stats_important_ents->teams[STATS_TEAM2].color,
						hud_teamholdinfo_opacity->value);
			}

			// Next line.
			_y += 8 * hud_teamholdinfo_scale->value;
		}
	}
}

static int SCR_HudDrawTeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, hud_t *hud);

static int HUD_CompareTeamInfoSlots(const void* lhs_, const void* rhs_)
{
	int lhs = *(const int*)lhs_;
	int rhs = *(const int*)rhs_;
	int lhs_pos = -1;
	int rhs_pos = -1;
	int i;

	for (i = 0; i < n_players; ++i) {
		if (sorted_players[i].playernum == lhs) {
			lhs_pos = i;
		}
		if (sorted_players[i].playernum == rhs) {
			rhs_pos = i;
		}
	}

	return lhs_pos - rhs_pos;
}

void SCR_HUD_DrawTeamInfo(hud_t *hud)
{
	int x, y, _y, width, height;
	int i, j, k, slots[MAX_CLIENTS], slots_num, maxname, maxloc;
	char tmp[1024], *nick;

	// Used for hud_teaminfo, data is collected in screen.c / scr_teaminfo
	extern ti_player_t ti_clients[MAX_CLIENTS];

	extern qbool hud_editor;

	static cvar_t
		*hud_teaminfo_weapon_style = NULL,
		*hud_teaminfo_align_right,
		*hud_teaminfo_loc_width,
		*hud_teaminfo_name_width,
		*hud_teaminfo_show_enemies,
		*hud_teaminfo_show_self,
		*hud_teaminfo_scale;

	if (hud_teaminfo_weapon_style == NULL)    // first time
	{
		hud_teaminfo_weapon_style			= HUD_FindVar(hud, "weapon_style");
		hud_teaminfo_align_right			= HUD_FindVar(hud, "align_right");
		hud_teaminfo_loc_width				= HUD_FindVar(hud, "loc_width");
		hud_teaminfo_name_width				= HUD_FindVar(hud, "name_width");
		hud_teaminfo_show_enemies			= HUD_FindVar(hud, "show_enemies");
		hud_teaminfo_show_self				= HUD_FindVar(hud, "show_self");
		hud_teaminfo_scale					= HUD_FindVar(hud, "scale");
	}

	// Don't update hud item unless first view is beeing displayed
	if ( CL_MultiviewCurrentView() != 1 && CL_MultiviewCurrentView() != 0)
		return;

	if (cls.mvdplayback)
		Update_TeamInfo();

	// fill data we require to draw teaminfo
	for ( maxloc = maxname = slots_num = i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !cl.players[i].name[0] || cl.players[i].spectator
				|| !ti_clients[i].time || ti_clients[i].time + 5 < r_refdef2.time
		   )
			continue;

		// do not show enemy players unless it's MVD and user wishes to show them
		if (VX_TrackerIsEnemy( i ) && (!cls.mvdplayback || !hud_teaminfo_show_enemies->integer))
			continue;

		// do not show tracked player to spectator
		if ((cl.spectator && Cam_TrackNum() == i) && hud_teaminfo_show_self->integer == 0)
			continue;

		// dynamically guess max length of name/location
		nick = (ti_clients[i].nick[0] ? ti_clients[i].nick : cl.players[i].name); // we use nick or name
		maxname = max(maxname, strlen(TP_ParseFunChars(nick, false)));

		strlcpy(tmp, TP_LocationName(ti_clients[i].org), sizeof(tmp));
		maxloc  = max(maxloc,  strlen(TP_ParseFunChars(tmp,  false)));

		slots[slots_num++] = i;
	}

	qsort(slots, slots_num, sizeof(slots[0]), HUD_CompareTeamInfoSlots);

	// well, better use fixed loc length
	maxloc  = bound(0, hud_teaminfo_loc_width->integer, 100);
	// limit name length
	maxname = bound(0, maxname, hud_teaminfo_name_width->integer);

	// this does't draw anything, just calculate width
	width = FONTWIDTH * hud_teaminfo_scale->value * SCR_HudDrawTeamInfoPlayer(&ti_clients[0], 0, 0, maxname, maxloc, true, hud);
	height = FONTWIDTH * hud_teaminfo_scale->value * (hud_teaminfo_show_enemies->integer?slots_num+n_teams:slots_num);

	if (hud_editor)
		HUD_PrepareDraw(hud, width , FONTWIDTH, &x, &y);

	if ( !slots_num )
		return;

	if (!cl.teamplay)  // non teamplay mode
		return;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	_y = y ;
	x = (hud_teaminfo_align_right->value ? x - (width * (FONTWIDTH * hud_teaminfo_scale->value)) : x);

	// If multiple teams are displayed then sort the display and print team header on overlay
	k=0;
	if (hud_teaminfo_show_enemies->integer)
	{
		while (sorted_teams[k].name)
		{
			Draw_SString (x, _y, sorted_teams[k].name, hud_teaminfo_scale->value);
			sprintf(tmp,"%s %4i", TP_ParseFunChars("$.",false), sorted_teams[k].frags);
			Draw_SString(x + width - 6 * FONTWIDTH * hud_teaminfo_scale->value, _y, tmp, hud_teaminfo_scale->value);
			_y += FONTWIDTH * hud_teaminfo_scale->value;
			for ( j = 0; j < slots_num; j++ ) 
			{
				i = slots[j];
				if (!strcmp(cl.players[i].team,sorted_teams[k].name))
				{
					SCR_HudDrawTeamInfoPlayer(&ti_clients[i], x, _y, maxname, maxloc, false, hud);
					_y += FONTWIDTH * hud_teaminfo_scale->value;
				}
			}
			k++;
		}
	}
	else 
	{
		for ( j = 0; j < slots_num; j++ ) {
			i = slots[j];
			SCR_HudDrawTeamInfoPlayer(&ti_clients[i], x, _y, maxname, maxloc, false, hud);
			_y += FONTWIDTH * hud_teaminfo_scale->value;
		}
	}
}

qbool Has_Both_RL_and_LG (int flags) { return (flags & IT_ROCKET_LAUNCHER) && (flags & IT_LIGHTNING); }
static int SCR_HudDrawTeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, hud_t *hud)
{
	extern mpic_t * SCR_GetWeaponIconByFlag (int flag);
	extern cvar_t tp_name_rlg;

	char *s, *loc, tmp[1024], tmp2[MAX_MACRO_STRING], *aclr;
	int x_in = x; // save x
	int i;
	mpic_t *pic;
	float scale = HUD_FindVar(hud, "scale")->value;

	extern mpic_t  *sb_face_invis, *sb_face_quad, *sb_face_invuln;
	extern mpic_t  *sb_armor[3];
	extern mpic_t *sb_items[32];

	if (!ti_cl)
		return 0;

	i = ti_cl->client;

	if (i < 0 || i >= MAX_CLIENTS)
	{
		Com_DPrintf("SCR_Draw_TeamInfoPlayer: wrong client %d\n", i);
		return 0;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, HUD_FindVar(hud, "layout")->string , sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));
	s = tmp2;

	//
	// parse/draw string like this "%n %h:%a %l %p %w"
	//

	for ( ; *s; s++) {
		switch( (int) s[0] ) {
			case '%':

				s++; // advance

				switch( (int) s[0] ) {
					case 'n': // draw name

						if(!width_only) {
							char *nick = TP_ParseFunChars(ti_cl->nick[0] ? ti_cl->nick : cl.players[i].name, false);
							str_align_right(tmp, sizeof(tmp), nick, maxname);
							Draw_SString (x, y, tmp, scale);
						}
						x += maxname * FONTWIDTH * scale;

						break;
					case 'w': // draw "best" weapon icon/name

						switch (HUD_FindVar(hud, "weapon_style")->integer) {
							case 1:
								if(!width_only) {
									if (Has_Both_RL_and_LG(ti_cl->items)) {
										char *weap_str = tp_name_rlg.string;
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_SColoredStringBasic(x, y, weap_white_stripped, false, scale);
									}
									else {
										char *weap_str = TP_ItemName(BestWeaponFromStatItems( ti_cl->items ));
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_SColoredStringBasic(x, y, weap_white_stripped, false, scale);
									}
								}
								x += 3 * FONTWIDTH * scale;

								break;
							default: // draw image by default
								if(!width_only)
									if ( (pic = SCR_GetWeaponIconByFlag(BestWeaponFromStatItems( ti_cl->items ))) )
										Draw_SPic (x, y, pic, 0.5 * scale);
								x += 2 * FONTWIDTH * scale;

								break;
						}

						break;
					case 'h': // draw health, padding with space on left side
					case 'H': // draw health, padding with space on right side

						if(!width_only) {
							snprintf(tmp, sizeof(tmp), (s[0] == 'h' ? "%s%3d" : "%s%-3d"), (ti_cl->health < HUD_FindVar(hud, "low_health")->integer ? "&cf00" : ""), ti_cl->health);
							Draw_SString (x, y, tmp, scale);
						}
						x += 3 * FONTWIDTH * scale;

						break;
					case 'f': // draw frags, space on left side
					case 'F': // draw frags, space on right side
						if (!width_only) {
							snprintf(tmp, sizeof(tmp), (s[0] == 'f' ? "%3d" : "%-3d"), cl.players[i].frags);
							Draw_SString(x, y, tmp, scale);
						}
						x += 3 * FONTWIDTH * scale;
						break;

					case 'a': // draw armor, padded with space on left side
					case 'A': // draw armor, padded with space on right side

						aclr = "";

						//
						// different styles of armor
						//
						switch (HUD_FindVar(hud,"armor_style")->integer) {
							case 1: // image prefixed armor value
								if(!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_SPic (x, y, sb_armor[2], 1.0/3 * scale);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_SPic (x, y, sb_armor[1], 1.0/3 * scale);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_SPic (x, y, sb_armor[0], 1.0/3 * scale);
								}
								x += FONTWIDTH * scale;

								break;
							case 2: // colored background of armor value
								/*
								   if(!width_only) {
								   byte col[4] = {255, 255, 255, 0};

								   if (ti_cl->items & IT_ARMOR3) {
								   col[0] = 255; col[1] =   0; col[2] =   0; col[3] = 255;
								   }
								   else if (ti_cl->items & IT_ARMOR2) {
								   col[0] = 255; col[1] = 255; col[2] =   0; col[3] = 255;
								   }
								   else if (ti_cl->items & IT_ARMOR1) {
								   col[0] =   0; col[1] = 255; col[2] =   0; col[3] = 255;
								   }
								   }
								   */

								break;
							case 3: // colored armor value
								if(!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										aclr = "&cf00";
									else if (ti_cl->items & IT_ARMOR2)
										aclr = "&cff0";
									else if (ti_cl->items & IT_ARMOR1)
										aclr = "&c0f0";
								}

								break;
							case 4: // armor value prefixed with letter
								if(!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_SString (x, y, "r", scale);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_SString (x, y, "y", scale);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_SString (x, y, "g", scale);
								}
								x += FONTWIDTH * scale;

								break;
						}

						if(!width_only) { // value drawed no matter which style
							snprintf(tmp, sizeof(tmp), (s[0] == 'a' ? "%s%3d" : "%s%-3d"), aclr, ti_cl->armor);
							Draw_SString (x, y, tmp, scale);
						}
						x += 3 * FONTWIDTH * scale;

						break;
					case 'l': // draw location

						if(!width_only) {
							loc = TP_LocationName(ti_cl->org);
							if (!loc[0])
								loc = "unknown";

							str_align_right(tmp, sizeof(tmp), TP_ParseFunChars(loc, false), maxloc);
							Draw_SString (x, y, tmp, scale);
						}
						x += maxloc * FONTWIDTH * scale;

						break;
					case 'p': // draw powerups
						switch (HUD_FindVar(hud, "powerup_style")->integer) {
							case 1: // quad/pent/ring image
								if(!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_SPic (x, y, sb_items[5], 1.0/2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_SPic (x, y, sb_items[3], 1.0/2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_SPic (x, y, sb_items[2], 1.0/2);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;

							case 2: // player powerup face
								if(!width_only) {
									if ( sb_face_quad && (ti_cl->items & IT_QUAD))
										Draw_SPic (x, y, sb_face_quad, 1.0/3);
									x += FONTWIDTH;
									if ( sb_face_invuln && (ti_cl->items & IT_INVULNERABILITY))
										Draw_SPic (x, y, sb_face_invuln, 1.0/3);
									x += FONTWIDTH;
									if ( sb_face_invis && (ti_cl->items & IT_INVISIBILITY))
										Draw_SPic (x, y, sb_face_invis, 1.0/3);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;

							case 3: // colored font (QPR)
								if(!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_ColoredString (x, y, "&c03fQ", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_ColoredString (x, y, "&cf00P", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_ColoredString (x, y, "&cff0R", false);
									x += FONTWIDTH;
								}
								else { x += 3* FONTWIDTH; }
								break;
						}
						break;

					case 't':
						if(!width_only)
						{
							sprintf(tmp, "%i", Player_GetTrackId(cl.players[ti_cl->client].userid));
							Draw_SString (x, y, tmp, scale);
						}
						x += FONTWIDTH * scale; // will break if tracknumber is double digits
						break;

					case '%': // wow, %% result in one %, how smart

						if(!width_only)
							Draw_SString (x, y, "%", scale);
						x += FONTWIDTH * scale;

						break;

					default: // print %x - that mean sequence unknown

						if(!width_only) {
							snprintf(tmp, sizeof(tmp), "%%%c", s[0]);
							Draw_SString (x, y, tmp, scale);
						}
						x += (s[0] ? 2 : 1) * FONTWIDTH * scale;

						break;
				}

				break;

			default: // print x
				if(!width_only) {
					snprintf(tmp, sizeof(tmp), "%c", s[0]);
					if (s[0] != ' ') // inhuman smart optimization, do not print space!
						Draw_SString (x, y, tmp, scale);
				}
				x += FONTWIDTH * scale;

				break;
		}
	}

	return (x - x_in) / (FONTWIDTH * scale); // return width
}

void SCR_HUD_DrawItemsClock(hud_t *hud)
{
	extern qbool hud_editor;
	extern const char* MVD_AnnouncerString(int line, int total, float* alpha);
	int width, height;
	int x, y;

	static cvar_t
		*hud_itemsclock_timelimit = NULL,
		*hud_itemsclock_style = NULL,
		*hud_itemsclock_scale = NULL,
		*hud_itemsclock_filter = NULL,
		*hud_itemsclock_backpacks = NULL;

	if (hud_itemsclock_timelimit == NULL) {
		char val[256];

		hud_itemsclock_timelimit = HUD_FindVar(hud, "timelimit");
		hud_itemsclock_style = HUD_FindVar(hud, "style");
		hud_itemsclock_scale = HUD_FindVar(hud, "scale");
		hud_itemsclock_filter = HUD_FindVar(hud, "filter");
		hud_itemsclock_backpacks = HUD_FindVar(hud, "backpacks");

		// Unecessary to parse the item filter string on each frame.
		hud_itemsclock_filter->OnChange = ItemsClock_OnChangeItemFilter;

		// Parse the item filter the first time (trigger the OnChange function above).
		strlcpy(val, hud_itemsclock_filter->string, sizeof(val));
		Cvar_Set(hud_itemsclock_filter, val);
	}

	MVD_ClockList_TopItems_DimensionsGet(hud_itemsclock_timelimit->value, hud_itemsclock_style->integer, &width, &height, hud_itemsclock_scale->value, hud_itemsclock_backpacks->integer);

	if (hud_editor)
		HUD_PrepareDraw(hud, width, LETTERHEIGHT * hud_itemsclock_scale->value, &x, &y);

	if (!height)
		return;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	MVD_ClockList_TopItems_Draw(hud_itemsclock_timelimit->value, hud_itemsclock_style->integer, x, y, hud_itemsclock_scale->value, itemsclock_filter, hud_itemsclock_backpacks->integer);
}

static qbool SCR_Hud_GetScores (int* team, int* enemy, char** teamName, char** enemyName)
{
	qbool swap = false;

	*team = *enemy = 0;
	*teamName = *enemyName = NULL;

	if (cl.teamplay) {
		*team = sorted_teams[0].frags;
		*teamName = sorted_teams[0].name;

		if (n_teams > 1) {
			if (hud_sortrules_includeself.integer >= 1 && active_team_position > 1) {
				*enemy = sorted_teams[active_team_position].frags;
				*enemyName = sorted_teams[active_team_position].name;
			}
			else {
				*enemy = sorted_teams[1].frags;
				*enemyName = sorted_teams[1].name;
			}

			swap = (hud_sortrules_includeself.integer == 1 && active_team_position > 0);
		}
	}
	else if (cl.deathmatch) {
		*team = cl.players[sorted_players[0].playernum].frags;
		*teamName = cl.players[sorted_players[0].playernum].name;

		if (n_players > 1) {
			if (hud_sortrules_includeself.integer >= 1 && active_player_position > 1) {
				*enemy = cl.players[sorted_players[active_player_position].playernum].frags;
				*enemyName = cl.players[sorted_players[active_player_position].playernum].name;
			}
			else {
				*enemy = cl.players[sorted_players[1].playernum].frags;
				*enemyName = cl.players[sorted_players[1].playernum].name;
			}

			swap = (hud_sortrules_includeself.integer == 1 && active_player_position > 0);
		}
	}

	if (!*teamName) {
		*teamName = "T";
	}
	if (!*enemyName) {
		*enemyName = "E";
	}

	if (swap) {
		int temp_frags = *team;
		char* temp_name = *teamName;

		*team = *enemy;
		*teamName = *enemyName;
		*enemy = temp_frags;
		*enemyName = temp_name;
	}

	return swap;
}

//
// TODO: decide what to do in freefly mode (and how to catch it?!), now all score_* hud elements just draws "0"
//
void SCR_HUD_DrawScoresTeam(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL)  // first time called
	{
		scale		= HUD_FindVar(hud, "scale");
		style		= HUD_FindVar(hud, "style");
		digits		= HUD_FindVar(hud, "digits");
		align		= HUD_FindVar(hud, "align");
		colorize	= HUD_FindVar(hud, "colorize");
	}

	SCR_Hud_GetScores (&teamFrags, &enemyFrags, &teamName, &enemyName);

	SCR_HUD_DrawNum(hud, teamFrags, (colorize->integer) ? (teamFrags < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string);
}

void SCR_HUD_DrawScoresEnemy(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL)  // first time called
	{
		scale		= HUD_FindVar(hud, "scale");
		style		= HUD_FindVar(hud, "style");
		digits		= HUD_FindVar(hud, "digits");
		align		= HUD_FindVar(hud, "align");
		colorize	= HUD_FindVar(hud, "colorize");
	}

	SCR_Hud_GetScores (&teamFrags, &enemyFrags, &teamName, &enemyName);

	SCR_HUD_DrawNum(hud, enemyFrags, (colorize->integer) ? (enemyFrags < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string);
}

void SCR_HUD_DrawScoresDifference(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize;
	int teamFrags = 0, enemyFrags = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL)  // first time called
	{
		scale		= HUD_FindVar(hud, "scale");
		style		= HUD_FindVar(hud, "style");
		digits		= HUD_FindVar(hud, "digits");
		align		= HUD_FindVar(hud, "align");
		colorize	= HUD_FindVar(hud, "colorize");
	}

	SCR_Hud_GetScores (&teamFrags, &enemyFrags, &teamName, &enemyName);

	SCR_HUD_DrawNum(hud, teamFrags - enemyFrags, (colorize->integer) ? ((teamFrags - enemyFrags) < 0 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string);
}

void SCR_HUD_DrawScoresPosition(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *colorize;
	int teamFrags = 0, enemyFrags = 0, position = 0, i = 0;
	char* teamName = 0, *enemyName = 0;

	if (scale == NULL)  // first time called
	{
		scale		= HUD_FindVar(hud, "scale");
		style		= HUD_FindVar(hud, "style");
		digits		= HUD_FindVar(hud, "digits");
		align		= HUD_FindVar(hud, "align");
		colorize	= HUD_FindVar(hud, "colorize");
	}

	SCR_Hud_GetScores (&teamFrags, &enemyFrags, &teamName, &enemyName);

	position = 1;
	if (cl.teamplay) {
		for (i = 0; i < n_teams; ++i) {
			if (sorted_teams[i].frags > teamFrags) {
				++position;
			}
		}
	}
	else {
		for (i = 0; i < n_players + n_spectators; ++i) {
			if (cl.players[sorted_players[i].playernum].frags > teamFrags) {
				++position;
			}
		}
	}

	SCR_HUD_DrawNum(hud, position, (colorize->integer) ? (position != 1 || colorize->integer > 1) : false, scale->value, style->value, digits->value, align->string);
}

/*
   ezQuake's analogue of +scores of KTX
   ( t:x e:x [x] )
   */
void SCR_HUD_DrawScoresBar(hud_t *hud)
{
	static	cvar_t *scale = NULL, *style, *format_big, *format_small, *frag_length, *reversed_big, *reversed_small;
	int		width = 0, height = 0, x, y;
	int		i = 0;

	int		s_team = 0, s_enemy = 0, s_difference = 0;
	char	*n_team = "T", *n_enemy = "E";
	qbool   swappedOrder = false;

	char	buf[MAX_MACRO_STRING];
	char	c, *out, *temp,	*in;
	int     frag_digits = 1;

	if (scale == NULL)  // first time called
	{
		scale          = HUD_FindVar(hud, "scale");
		style          = HUD_FindVar(hud, "style");
		format_big     = HUD_FindVar(hud, "format_big");
		format_small   = HUD_FindVar(hud, "format_small");
		frag_length    = HUD_FindVar(hud, "frag_length");
		reversed_big   = HUD_FindVar(hud, "format_reversed_big");
		reversed_small = HUD_FindVar(hud, "format_reversed_small");
	}

	frag_digits = max(1, min(frag_length->value, 4));

	swappedOrder = SCR_Hud_GetScores(&s_team, &s_enemy, &n_team, &n_enemy);
	s_difference = s_team - s_enemy;

	// two pots of delicious customized copypasta from math_tools.c
	switch (style->integer)
	{
		// Big
		case 1:
			in = TP_ParseFunChars(swappedOrder && reversed_big->string[0] ? reversed_big->string : format_big->string, false);
			buf[0] = 0;
			out = buf;

			while((c = *in++) && (out - buf < MAX_MACRO_STRING - 1)) {
				if((c == '%') && *in) {
					switch((c = *in++))
					{
						// c = colorize, r = reset
						case 'd':
							temp = va("%d", s_difference);
							width += (s_difference >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'D':
							temp = va("c%dr", s_difference);
							width += (s_difference >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'e':
							temp = va("%d", s_enemy);
							width += (s_enemy >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'E':
							temp = va("c%dr", s_enemy);
							width += (s_enemy >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'p':
							temp = va("%d", i + 1);
							width += 24;
							break;
						case 't':
							temp = va("%d", s_team);
							width += (s_team >= 0) ? strlen(temp) * 24 : ((strlen(temp) - 1) * 24) + 16;
							break;
						case 'T':
							temp = va("c%dr", s_team);
							width += (s_team >= 0) ? (strlen(temp) - 2) * 24 : ((strlen(temp) - 3) * 24) + 16;
							break;
						case 'z':
							if(s_difference >= 0)
							{
								temp = va("%d", s_difference);
								width += strlen(temp) * 24;
							}
							else
							{
								temp = va("c%dr", -(s_difference));
								width += (strlen(temp) - 2) * 24;
							}
							break;
						case 'Z':
							if(s_difference >= 0)
							{
								temp = va("c%dr", s_difference);
								width += (strlen(temp) - 2) * 24;
							}
							else
							{
								temp = va("%d", -(s_difference));
								width += strlen(temp) * 24;
							}
							break;
						default:
							temp = NULL;
							break;
					}

					if (temp != NULL) {
						strlcpy(out, temp, sizeof(buf) - (out - buf));
						out += strlen(temp);
					}
				}
				else if (c == ':' || c == '/' || c == '-' || c == ' ')
				{
					width += 16;
					*out++ = c;
				}
			}
			*out = 0;
			break;

			// Small
		case 0:	
		default:
			in = TP_ParseFunChars(swappedOrder && reversed_small->string[0] ? reversed_small->string : format_small->string, false);
			buf[0] = 0;
			out = buf;

			while((c = *in++) && (out - buf < MAX_MACRO_STRING - 1)) {
				if ((c == '%') && *in) {
					switch((c = *in++))
					{
						case '%':
							temp = "%";
							break;
						case 't':
							temp = va("%*d", frag_digits, s_team);
							break;
						case 'e':
							temp = va("%*d", frag_digits, s_enemy);
							break;
						case 'd':
							temp = va("%d", s_difference);
							break;
						case 'p':
							temp = va("%d", i + 1);
							break;
						case 'T':
							temp = n_team;
							break;
						case 'E':
							temp = n_enemy;
							break;
						case 'D':
							temp = va("%+d", s_difference);
							break;
						default:
							temp = va("%%%c", c);
							break;
					}
					strlcpy(out, temp, sizeof(buf) - (out - buf));
					out += strlen(temp);
				}
				else {
					*out++ = c;
				}
			}
			*out = 0;
			break;
	}

	switch(style->integer)
	{
		// Big
		case 1:
			width *= scale->value;
			height = 24 * scale->value;

			if(HUD_PrepareDraw(hud, width, height, &x, &y)) {
				SCR_DrawWadString(x, y, scale->value, buf);
			}
			break;

			// Small
		case 0:
		default:
			width = 8 * strlen_color(buf) * scale->value;
			height = 8 * scale->value;

			if(HUD_PrepareDraw(hud, width, height, &x, &y)) {
				Draw_SString(x, y, buf, scale->value);
			}
			break;
	}
}

void SCR_HUD_DrawBarArmor(hud_t *hud)
{
	static	cvar_t *width = NULL, *height, *direction, *color_noarmor, *color_ga, *color_ya, *color_ra, *color_unnatural;
	int		x, y;
	int		armor = HUD_Stats(STAT_ARMOR);
	qbool	alive = cl.stats[STAT_HEALTH] > 0;

	if (width == NULL)  // first time called
	{
		width			= HUD_FindVar(hud, "width");
		height			= HUD_FindVar(hud, "height");
		direction		= HUD_FindVar(hud, "direction");
		color_noarmor	= HUD_FindVar(hud, "color_noarmor");
		color_ga		= HUD_FindVar(hud, "color_ga");
		color_ya		= HUD_FindVar(hud, "color_ya");
		color_ra		= HUD_FindVar(hud, "color_ra");
		color_unnatural	= HUD_FindVar(hud, "color_unnatural");
	}

	if(HUD_PrepareDraw(hud, width->integer, height->integer, &x, &y))
	{
		if(!width->integer || !height->integer)
			return;

		if(HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY && alive)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_unnatural->color, x, y, width->integer, height->integer);
		}
		else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3 && alive)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 200.0, color_ra->color, x, y, width->integer, height->integer);
		}
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2 && alive)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 150.0, color_ya->color, x, y, width->integer, height->integer);
		}
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1 && alive)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 100.0, color_ga->color, x, y, width->integer, height->integer);
		}
		else
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
		}
	}
}

void SCR_HUD_DrawBarHealth(hud_t *hud)
{
	static	cvar_t *width = NULL, *height, *direction, *color_nohealth, *color_normal, *color_mega, *color_twomega, *color_unnatural;
	int		x, y;
	int		health = cl.stats[STAT_HEALTH];

	if (width == NULL)  // first time called
	{
		width			= HUD_FindVar(hud, "width");
		height			= HUD_FindVar(hud, "height");
		direction		= HUD_FindVar(hud, "direction");
		color_nohealth	= HUD_FindVar(hud, "color_nohealth");
		color_normal	= HUD_FindVar(hud, "color_normal");
		color_mega		= HUD_FindVar(hud, "color_mega");
		color_twomega	= HUD_FindVar(hud, "color_twomega");
		color_unnatural	= HUD_FindVar(hud, "color_unnatural");
	}

	if(HUD_PrepareDraw(hud, width->integer, height->integer, &x, &y))
	{
		if(!width->integer || !height->integer)
			return;

		if(health > 250)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_unnatural->color, x, y, width->integer, height->integer);
		}
		else if(health > 200)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_normal->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_mega->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health - 200, 100.0, color_twomega->color, x, y, width->integer, height->integer);
		}
		else if(health > 100)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_normal->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health - 100, 100.0, color_mega->color, x, y, width->integer, height->integer);
		}
		else if(health > 0)
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_nohealth->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health, 100.0, color_normal->color, x, y, width->integer, height->integer);
		}
		else
		{
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_nohealth->color, x, y, width->integer, height->integer);
		}
	}
}

#ifdef WITH_PNG

void SCR_HUD_DrawOwnFrags(hud_t *hud)
{
	// not implemented yet: scale, color
	// fixme: add appropriate opengl functions that will add alpha, scale and color
	int width = VX_OwnFragTextLen() * LETTERWIDTH;
	int height = LETTERHEIGHT;
	int x, y;
	double alpha;
	static cvar_t
		*hud_ownfrags_timeout = NULL,
		*hud_ownfrags_scale = NULL;
	//        *hud_ownfrags_color = NULL;
	extern qbool hud_editor;

	if (hud_ownfrags_timeout == NULL)    // first time
	{
		hud_ownfrags_scale				= HUD_FindVar(hud, "scale");
		// hud_ownfrags_color               = HUD_FindVar(hud, "color");
		hud_ownfrags_timeout            = HUD_FindVar(hud, "timeout");
	}

	if (hud_editor)
		HUD_PrepareDraw(hud, 10 * LETTERWIDTH , height * hud_ownfrags_scale->value, &x, &y);

	if (!width)
		return;

	if (VX_OwnFragTime() > hud_ownfrags_timeout->value)
		return;

	width *= hud_ownfrags_scale->value;
	height *= hud_ownfrags_scale->value;

	alpha = 2 - hud_ownfrags_timeout->value / VX_OwnFragTime() * 2;
	alpha = bound(0, alpha, 1);

	if (!HUD_PrepareDraw(hud, width , height, &x, &y))
		return;

	if (VX_OwnFragTime() < hud_ownfrags_timeout->value)
		Draw_SString(x, y, VX_OwnFragText(), hud_ownfrags_scale->value);
}

void SCR_HUD_DrawKeys(hud_t *hud)
{
	char line1[4], line2[4];
	int width, height, x, y;
	usermainbuttons_t b;
	int i;
	static cvar_t* vscale = NULL;
	static cvar_t* player = NULL;
	float scale;
	int player_slot = -1;

	if (!vscale) {
		vscale = HUD_FindVar(hud, "scale");
		player = HUD_FindVar(hud, "player");
	}

	if (cls.demoplayback && !cls.nqdemoplayback && !cls.mvdplayback && player->string[0]) {
		player_slot = Player_GetSlot(player->string);
	}

	b = CL_GetLastCmd(player_slot);

	scale = vscale->value;
	scale = max(0, scale);

	i = 0;
	line1[i++] = b.attack  ? 'x' + 128 : 'x';
	line1[i++] = b.forward ? '^' + 128 : '^';
	line1[i++] = b.jump    ? 'J' + 128 : 'J';
	line1[i++] = '\0';
	i = 0;
	line2[i++] = b.left    ? '<' + 128 : '<';
	line2[i++] = b.back    ? '_' + 128 : '_';
	line2[i++] = b.right   ? '>' + 128 : '>';
	line2[i++] = '\0';

	width = LETTERWIDTH * strlen(line1) * scale;
	height = LETTERHEIGHT * 2 * scale;

	if (!HUD_PrepareDraw(hud, width ,height, &x, &y))
		return;

	Draw_SString(x, y, line1, scale);
	Draw_SString(x, y + LETTERHEIGHT*scale, line2, scale);
}

#endif // WITH_PNG

void SCR_HUD_MultiLineString(hud_t* hud, const char* in, qbool large_font, int alignment, float scale);

//---------------------
//
// draw HUD static text
//
void SCR_HUD_DrawStaticText(hud_t *hud)
{
	const char *in;
	int alignment = 0;

	static cvar_t
		*hud_statictext_big = NULL,
		*hud_statictext_scale,
		*hud_statictext_text,
		*hud_statictext_textalign;

	if (hud_statictext_big == NULL) {
		// first time
		hud_statictext_big = HUD_FindVar(hud, "big");
		hud_statictext_scale = HUD_FindVar(hud, "scale");
		hud_statictext_text = HUD_FindVar(hud, "text");
		hud_statictext_textalign = HUD_FindVar(hud, "textalign");
	}

	// Static text valid for demos/qtv only
	if (!cls.demoplayback) {
		int x, y;
		HUD_PrepareDraw(hud, 0, 0, &x, &y);
		return;
	}

	alignment = 0;
	if (!strcmp(hud_statictext_textalign->string, "right"))
		alignment = 1;
	else if (!strcmp(hud_statictext_textalign->string, "center"))
		alignment = 2;

	// convert special characters
	in = hud_statictext_text->string;
	if (strlen(in) >= MAX_MACRO_STRING) {
		in = "error: input string too long";
	}
	in = TP_ParseFunChars(in, false);

	SCR_HUD_MultiLineString(hud, in, hud_statictext_big->integer, alignment, hud_statictext_scale->value);
}

void SCR_HUD_MultiLineString(hud_t* hud, const char* in, qbool large_font, int alignment, float scale)
{
	// find carriage returns
	int x, y;
	const char *line_start = in;
	int max_length = strlen_color_by_terminator(line_start, '\r');
	char* line_end;
	int lines = 0;
	int character_width, character_height;

	while ((line_end = strchr(line_start, '\r'))) {
		line_start = line_end + 1;
		max_length = max(max_length, strlen_color_by_terminator(line_start, '\r'));

		++lines;
	}

	if (*line_start) {
		++lines;
	}

	// collapse to invisible if nothing to display
	if (max_length == 0) {
		lines = 0;
	}

	character_width = 8 * scale;
	character_height = 8 * scale;
	if (large_font) {
		character_width = 24 * scale;
		character_height = 24 * scale;
	}

	if (HUD_PrepareDraw(hud, max_length * character_width, lines * character_height, &x, &y)) {
		for (line_start = in; *line_start; line_start = line_end + 1) {
			line_end = strchr(line_start, '\r');
			int diff = max_length - strlen_color(line_start);
			int line_x = x;

			if (line_end) {
				*line_end = '\0';
			}

			// Left pad string depending on alignment
			if (alignment == 1) {
				line_x += (max_length - strlen_color(line_start)) * character_width;
			}
			else if (alignment == 2) {
				if (diff % 2 == 1) {
					line_x += 0.5 * character_width;
				}
				line_x += (max_length - strlen_color(line_start)) / 2 * character_width;
			}

			if (large_font) {
				SCR_DrawWadString(line_x, y, scale, line_start); 
			}
			else {
				Draw_SString(line_x, y, line_start, scale);
			}

			y += character_height;
			if (!line_end)
				break;
		}
	}
}

//
// Run before HUD elements are drawn.
// Place stuff that is common for HUD elements here.
//
void HUD_BeforeDraw(void)
{
	// Only sort once per draw.
	HUD_Sort_Scoreboard (HUD_SCOREBOARD_ALL);
}

//
// Run after HUD elements are drawn.
// Place stuff that is common for HUD elements here.
//
void HUD_AfterDraw(void)
{
}

// ----------------
// Init
// and add some common elements to hud (clock etc)
//

static void SCR_Hud_GameSummary(hud_t* hud);

void CommonDraw_Init(void)
{
	int i;

	// variables
	Cvar_SetCurrentGroup(CVAR_GROUP_HUD);
	Cvar_Register(&hud_planmode);
	Cvar_Register(&hud_tp_need);
	Cvar_Register(&hud_digits_trim);
	Cvar_Register(&hud_sortrules_playersort);
	Cvar_Register(&hud_sortrules_teamsort);
	Cvar_Register(&hud_sortrules_includeself);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_MVD);
	Cvar_Register (&mvd_autohud);
	Cvar_ResetCurrentGroup();

	// init HUD STAT table
	for (i=0; i < MAX_CL_STATS; i++)
		hud_stats[i] = 0;
	hud_stats[STAT_HEALTH]  = 200;
	hud_stats[STAT_AMMO]    = 100;
	hud_stats[STAT_ARMOR]   = 200;
	hud_stats[STAT_SHELLS]  = 200;
	hud_stats[STAT_NAILS]   = 200;
	hud_stats[STAT_ROCKETS] = 100;
	hud_stats[STAT_CELLS]   = 100;
	hud_stats[STAT_ACTIVEWEAPON] = 32;
	hud_stats[STAT_ITEMS] = 0xffffffff - IT_ARMOR2 - IT_ARMOR1;

	autohud.active = 0;

	// init gameclock
	HUD_Register(
		"gameclock", NULL, "Shows current game time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawGameClock,
		"1", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"big",      "1",
		"style",    "0",
		"scale",    "1",
		"blink",    "1",
		"countdown","0",
		"offset","0",
		NULL
	);

	HUD_Register(
		"notify", NULL, "Shows last console lines",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawNotify,
		"0", "top", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"rows", "4",
		"cols", "30",
		"scale", "1",
		"time", "4",
		NULL
	);

	// fps
	HUD_Register(
		"fps", NULL,
		"Shows your current framerate in frames per second (fps)."
		"This can also show the minimum framerate that occured in the last measured period.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawFPS,
		"1", "gameclock", "center", "after", "0", "0", "0", "0 0 0", NULL,
		"show_min", "0",
		"style",    "0",
		"title",    "1",
		"scale",    "1",
		"drop",     "70",
		NULL
	);

	HUD_Register(
		"vidlag", NULL,
		"Shows the delay between the time a frame is rendered and the time it's displayed.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawVidLag,
		"0", "top", "right", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		NULL
	);

	// init clock
	HUD_Register(
		"clock", NULL, "Shows current local time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawClock,
		"0", "top", "right", "console", "0", "0", "0", "0 0 0", NULL,
		"big",      "1",
		"style",    "0",
		"scale",    "1",
		"blink",    "1",
		"format",   "0",
		NULL
	);

	// init democlock
	HUD_Register(
		"democlock", NULL, "Shows current demo time (hh:mm:ss).",
		HUD_PLUSMINUS, ca_disconnected, 7, SCR_HUD_DrawDemoClock,
		"1", "top", "right", "console", "0", "8", "0", "0 0 0", NULL,
		"big",      "0",
		"style",    "0",
		"scale",    "1",
		"blink",    "0",
		NULL
	);

	// init ping
	HUD_Register(
		"ping", NULL, "Shows most important net conditions, like ping and pl. Shown only when you are connected to a server.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawPing,
		"0", "screen", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"period",   "1",
		"show_pl",  "1",
		"show_min", "0",
		"show_max", "0",
		"show_dev", "0",
		"style",    "0",
		"blink",    "1",
		"scale",    "1",
		NULL);

	// init net
	HUD_Register(
		"net", NULL, "Shows network statistics, like latency, packet loss, average packet sizes and bandwidth. Shown only when you are connected to a server.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawNetStats,
		"0", "top", "left", "center", "0", "0", "0.2", "0 0 0", NULL,
		"period", "1",
		"scale", "1",
		NULL
	);

	// init speed
	HUD_Register("speed", NULL, "Shows your current running speed. It is measured over XY or XYZ axis depending on \'xyz\' property.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawSpeed,
		"0", "top", "center", "bottom", "0", "-5", "0", "0 0 0", NULL,
		"xyz",  "0",
		"width", "160",
		"height", "15",
		"opacity", "1.0",
		"tick_spacing", "0.2",
		"color_stopped", SPEED_STOPPED,
		"color_normal", SPEED_NORMAL,
		"color_fast", SPEED_FAST,
		"color_fastest", SPEED_FASTEST,
		"color_insane", SPEED_INSANE,
		"vertical", "0",
		"vertical_text", "1",
		"text_align", "1",
		"style", "0",
		"scale", "1",
		NULL
	);

	// Init speed2 (half circle thingie).
	HUD_Register("speed2", NULL, "Shows your current running speed. It is measured over XY or XYZ axis depending on \'xyz\' property.",
		HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawSpeed2,
		"0", "top", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"xyz",  "0",
		"opacity", "1.0",
		"color_stopped", SPEED_STOPPED,
		"color_normal", SPEED_NORMAL,
		"color_fast", SPEED_FAST,
		"color_fastest", SPEED_FASTEST,
		"color_insane", SPEED_INSANE,
		"radius", "50.0",
		"wrapspeed", "500",
		"orientation", "0",
		"scale", "1",
		NULL
	);

	// init guns
	HUD_Register("gun", NULL, "Part of your inventory - current weapon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGunCurrent,
			"0", "ibar", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
			"wide",  "0",
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun2", NULL, "Part of your inventory - shotgun.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun2,
			"1", "ibar", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun3", NULL, "Part of your inventory - super shotgun.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun3,
			"1", "gun2", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun4", NULL, "Part of your inventory - nailgun.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun4,
			"1", "gun3", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun5", NULL, "Part of your inventory - super nailgun.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun5,
			"1", "gun4", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun6", NULL, "Part of your inventory - grenade launcher.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun6,
			"1", "gun5", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun7", NULL, "Part of your inventory - rocket launcher.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun7,
			"1", "gun6", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("gun8", NULL, "Part of your inventory - thunderbolt.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun8,
			"1", "gun7", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"wide",  "0",
			"style", "0",
			"scale", "1",
			NULL);

	// init powerzz
	HUD_Register("key1", NULL, "Part of your inventory - silver key.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey1,
			"1", "ibar", "top", "left", "0", "64", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("key2", NULL, "Part of your inventory - gold key.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey2,
			"1", "key1", "left", "after", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("ring", NULL, "Part of your inventory - invisibility.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawRing,
			"1", "key2", "left", "after", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("pent", NULL, "Part of your inventory - invulnerability.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawPent,
			"1", "ring", "left", "after", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("suit", NULL, "Part of your inventory - biosuit.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSuit,
			"1", "pent", "left", "after", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("quad", NULL, "Part of your inventory - quad damage.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawQuad,
			"1", "suit", "left", "after", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);

	// netproblem icon
	HUD_Register("netproblem", NULL, "Shows an icon if you are experiencing network problems",
			HUD_NO_FRAME, ca_active, 0, SCR_HUD_NetProblem,
			"1", "top", "left", "top", "0", "0", "0", "0 0 0", NULL,
			"scale", "1",
			NULL);

	// sigilzz
	HUD_Register("sigil1", NULL, "Part of your inventory - sigil 1.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil1,
			"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("sigil2", NULL, "Part of your inventory - sigil 2.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil2,
			"0", "sigil1", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("sigil3", NULL, "Part of your inventory - sigil 3.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil3,
			"0", "sigil2", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("sigil4", NULL, "Part of your inventory - sigil 4.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil4,
			"0", "sigil3", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);

	// player face (health indicator)
	HUD_Register("face", NULL, "Your bloody face.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawFace,
			"1", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
			"scale", "1",
			NULL);

	// health
	HUD_Register("health", NULL, "Part of your status - health level.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealth,
			"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
			"style",  "0",
			"scale",  "1",
			"align",  "right",
			"digits", "3",
			NULL);

	// ammo/s
	HUD_Register("ammo", NULL, "Part of your inventory - ammo for active weapon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoCurrent,
			"1", "health", "after", "center", "32", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "3",
			NULL);
	HUD_Register("ammo1", NULL, "Part of your inventory - ammo - shells.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo1,
			"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "3",
			NULL);
	HUD_Register("ammo2", NULL, "Part of your inventory - ammo - nails.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo2,
			"0", "ammo1", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "3",
			NULL);
	HUD_Register("ammo3", NULL, "Part of your inventory - ammo - rockets.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo3,
			"0", "ammo2", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "3",
			NULL);
	HUD_Register("ammo4", NULL, "Part of your inventory - ammo - cells.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo4,
			"0", "ammo3", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "3",
			NULL);

	// ammo icon/s
	HUD_Register("iammo", NULL, "Part of your inventory - ammo icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIconCurrent,
			"1", "ammo", "before", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);
	HUD_Register("iammo1", NULL, "Part of your inventory - ammo icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon1,
			"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "2",
			"scale", "1",
			NULL);
	HUD_Register("iammo2", NULL, "Part of your inventory - ammo icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon2,
			"0", "iammo1", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "2",
			"scale", "1",
			NULL);
	HUD_Register("iammo3", NULL, "Part of your inventory - ammo icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon3,
			"0", "iammo2", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "2",
			"scale", "1",
			NULL);
	HUD_Register("iammo4", NULL, "Part of your inventory - ammo icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon4,
			"0", "iammo3", "after", "top", "0", "0", "0", "0 0 0", NULL,
			"style", "2",
			"scale", "1",
			NULL);

	// armor count
	HUD_Register("armor", NULL, "Part of your inventory - armor level.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmor,
			"1", "face", "before", "center", "-32", "0", "0", "0 0 0", NULL,
			"style",  "0",
			"scale",  "1",
			"align",  "right",
			"digits", "3",
			"pent_666", "1",  // Show 666 instead of armor value
			NULL);

	// armor icon
	HUD_Register("iarmor", NULL, "Part of your inventory - armor icon.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorIcon,
			"1", "armor", "before", "center", "0", "0", "0", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			NULL);

	// Tracking JohnNy_cz (Contains name of the player who's player we're watching at the moment)
	HUD_Register("tracking", NULL, "Shows the name of tracked player.",
			HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawTracking,
			"1", "face", "center", "before", "0", "0", "0", "0 0 0", NULL,
			"format", "\xD4\xF2\xE1\xE3\xEB\xE9\xEE\xE7\xBA %t %n, \xCA\xD5\xCD\xD0 for next", //"Tracking: team name, JUMP for next", "Tracking:" and "JUMP" are brown. default: "Tracking %t %n, [JUMP] for next"
			"scale", "1",
			NULL);

	// groups
	HUD_Register("group1", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "left", "top", "0", "0", ".5", "0 0 0", NULL,
			"name", "group1",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group2", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "center", "top", "0", "0", ".5", "0 0 0", NULL,
			"name", "group2",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group3", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "right", "top", "0", "0", ".5", "0 0 0", NULL,
			"name", "group3",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group4", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "left", "center", "0", "0", ".5", "0 0 0", NULL,
			"name", "group4",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group5", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "center", "center", "0", "0", ".5", "0 0 0", NULL,
			"name", "group5",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group6", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "right", "center", "0", "0", ".5", "0 0 0", NULL,
			"name", "group6",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group7", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "left", "bottom", "0", "0", ".5", "0 0 0", NULL,
			"name", "group7",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group8", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "center", "bottom", "0", "0", ".5", "0 0 0", NULL,
			"name", "group8",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);
	HUD_Register("group9", NULL, "Group element.",
			HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
			"0", "screen", "right", "bottom", "0", "0", ".5", "0 0 0", NULL,
			"name", "group9",
			"width", "64",
			"height", "64",
			"picture", "",
			"pic_alpha", "1.0",
			"pic_scalemode", "0",
			NULL);

	// healthdamage
	HUD_Register("healthdamage", NULL, "Shows amount of damage done to your health.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealthDamage,
			"0", "health", "left", "before", "0", "0", "0", "0 0 0", NULL,
			"style",  "0",
			"scale",  "1",
			"align",  "right",
			"digits", "3",
			"duration", "0.8",
			NULL);

	// armordamage
	HUD_Register("armordamage", NULL, "Shows amount of damage done to your armour.",
			HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorDamage,
			"0", "armor", "left", "before", "0", "0", "0", "0 0 0", NULL,
			"style",  "0",
			"scale",  "1",
			"align",  "right",
			"digits", "3",
			"duration", "0.8",
			NULL);

	HUD_Register(
		"frags", NULL, "Show list of player frags in short form.",
		0, ca_active, 0, SCR_HUD_DrawFrags,
		"0", "top", "right", "bottom", "0", "0", "0", "0 0 0", NULL,
		"cell_width", "32",
		"cell_height", "8",
		"rows", "1",
		"cols", "4",
		"space_x", "1",
		"space_y", "1",
		"teamsort", "0",
		"strip", "1",
		"vertical", "0",
		"shownames", "0",
		"showteams", "0",
		"padtext", "1",
		"showself_always", "1",
		"extra_spec_info", "ALL",
		"fliptext", "0",
		"style", "0",
		"bignum", "0",
		"colors_alpha", "1.0",
		"maxname", "16",
		"notintp", "0",
		"fixedwidth", "0",
		"scale", "1",
		NULL
	);

	HUD_Register(
		"teamfrags", NULL, "Show list of team frags in short form.",
		0, ca_active, 0, SCR_HUD_DrawTeamFrags,
		"1", "ibar", "center", "before", "0", "0", "0", "0 0 0", NULL,
		"cell_width", "32",
		"cell_height", "8",
		"rows", "1",
		"cols", "2",
		"space_x", "1",
		"space_y", "1",
		"strip", "1",
		"vertical", "0",
		"shownames", "0",
		"padtext", "1",
		"fliptext", "1",
		"style", "0",
		"extra_spec_info", "1",
		"onlytp", "0",
		"bignum", "0",
		"colors_alpha", "1.0",
		"maxname", "16",
		"scale", "1",
		NULL
	);

	HUD_Register("teaminfo", NULL, "Show information about your team in short form.",
			0, ca_active, 0, SCR_HUD_DrawTeamInfo,
			"0", "", "right", "center", "0", "0", "0.2", "20 20 20", NULL,
			"layout", "%p%n $x10%l$x11 %a/%H %w",
			"align_right","0",
			"loc_width","5",
			"name_width","6",
			"low_health","25",
			"armor_style","3",
			"weapon_style","0",
			"show_enemies","0",
			"show_self","1",
			"scale","1",
			"powerup_style","1",
			NULL);

	HUD_Register(
		"teamholdbar", NULL, "Shows how much of the level (in percent) that is currently being held by either team.",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawTeamHoldBar,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.8",
		"width", "200",
		"height", "8",
		"vertical", "0",
		"vertical_text", "0",
		"show_text", "1",
		"onlytp", "0",
		"scale", "1",
		NULL
	);

	HUD_Register(
		"teamholdinfo", NULL, "Shows which important items in the level that are being held by the teams.",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawTeamHoldInfo,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.8",
		"width", "200",
		"height", "8",
		"onlytp", "0",
		"style", "1",
		"itemfilter", "quad ra ya ga mega pent rl quad",
		"scale", "1",
		NULL
	);

#ifdef WITH_PNG
	HUD_Register("ownfrags" /* jeez someone give me a better name please */, NULL, "Highlights your own frags",
			0, ca_active, 1, SCR_HUD_DrawOwnFrags,
			"1", "screen", "center", "top", "0", "50", "0.2", "0 0 100", NULL,
			/*
			   "color", "255 255 255",
			   */
			"timeout", "3",
			"scale", "1.5",
			NULL
		    );

	HUD_Register("keys", NULL, "Shows which keys user does press at the moment",
			0, ca_active, 1, SCR_HUD_DrawKeys,
			"0", "screen", "right", "center", "0", "0", "0.5", "20 20 20", NULL,
			"scale", "2", "player", "",
			NULL
		    );
#endif

	HUD_Register(
		"itemsclock", NULL, "Displays upcoming item respawns",
		0, ca_active, 1, SCR_HUD_DrawItemsClock,
		"0", "screen", "right", "center", "0", "0", "0", "0 0 0", NULL,
		"timelimit", "5",
		"style", "0",
		"scale", "1",
		"filter", "",
		"backpacks", "0",
		NULL
	);

	HUD_Register("score_team", NULL, "Own scores or team scores.",
			0, ca_active, 0, SCR_HUD_DrawScoresTeam,
			"0", "screen", "left", "bottom", "0", "0", "0.5", "4 8 32", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "0",
			"colorize", "0",
			NULL
		    );

	HUD_Register("score_enemy", NULL, "Scores of enemy or enemy team.",
			0, ca_active, 0, SCR_HUD_DrawScoresEnemy,
			"0", "score_team", "after", "bottom", "0", "0", "0.5", "32 4 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "0",
			"colorize", "0",
			NULL
		    );

	HUD_Register("score_difference", NULL, "Difference between teamscores and enemyscores.",
			0, ca_active, 0, SCR_HUD_DrawScoresDifference,
			"0", "score_enemy", "after", "bottom", "0", "0", "0.5", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "0",
			"colorize", "1",
			NULL
		    );

	HUD_Register("score_position", NULL, "Position on scoreboard.",
			0, ca_active, 0, SCR_HUD_DrawScoresPosition,
			"0", "score_difference", "after", "bottom", "0", "0", "0.5", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"align", "right",
			"digits", "0",
			"colorize", "1",
			NULL
		    );

	HUD_Register("score_bar", NULL, "Team, enemy, and difference scores together.",
			HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawScoresBar,
			"0", "screen", "center", "console", "0", "0", "0.5", "0 0 0", NULL,
			"style", "0",
			"scale", "1",
			"format_small", "&c69f%T&r:%t &cf10%E&r:%e $[%D$]",
			"format_big", "%t:%e:%Z",
			"format_reversed_big", "",
			"format_reversed_small", "",
			"frag_length", "0",
			NULL
		    );

	HUD_Register("bar_armor", NULL, "Armor bar.",
			HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawBarArmor,
			"0", "armor", "left", "center", "0", "0", "0", "0 0 0", NULL,
			"height", "16",
			"width", "64",
			"direction", "1",
			"color_noarmor", "128 128 128 64",
			"color_ga", "32 128 0 128",
			"color_ya", "192 128 0 128",
			"color_ra", "128 0 0 128",
			"color_unnatural", "255 255 255 128",
			NULL
		    );

	HUD_Register("bar_health", NULL, "Health bar.",
			HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawBarHealth,
			"0", "health", "right", "center", "0", "0", "0", "0 0 0", NULL,
			"height", "16",
			"width", "64",
			"direction", "0",
			"color_nohealth", "128 128 128 64",
			"color_normal", "32 64 128 128",
			"color_mega", "64 96 128 128",
			"color_twomega", "128 128 255 128",
			"color_unnatural", "255 255 255 128",
			NULL
		    );

	HUD_Register("static_text", NULL, "Static text (demos only).",
			0, ca_active, 0, SCR_HUD_DrawStaticText,
			"0", "screen", "left", "top", "0", "0", "0", "0 0 0", NULL,
			"big", "0",
			"style", "0",
			"scale", "1",
			"text", "",
			"textalign", "left",
			NULL
		    );

	HUD_Register(
		"teamstackbar", NULL, "Shows relative stacks of each team.",
		HUD_PLUSMINUS, ca_active, 0, SCR_Hud_StackBar,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.8",
		"width", "200",
		"height", "8",
		"vertical", "0",
		"vertical_text", "0",
		"show_text", "1",
		"onlytp", "0",
		"scale", "1",
		"simpleitems", "1",
		NULL
	);

	HUD_Register(
		"gamesummary", NULL, "Shows total pickups & current weapons.",
		HUD_PLUSMINUS, ca_active, 0, SCR_Hud_GameSummary,
		"0", "top", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"onlytp", "0",
		"scale", "1",
		"format", "QYyq",
		"circles", "0",
		"ratio", "4",
		"flash", "1",
		NULL
	);

	Radar_HudInit();
	WeaponStats_HUDInit();
	/* hexum -> FIXME? this is used only for debug purposes, I wont bother to port it (it shouldnt be too difficult if anyone cares)
#ifdef _DEBUG
HUD_Register("framegraph", NULL, "Shows different frame times for debug/profiling purposes.",
HUD_PLUSMINUS | HUD_ON_SCORES, ca_disconnected, 0, SCR_HUD_DrawFrameGraph,
"0", "top", "left", "bottom", "0", "0", "2",
"swap_x",       "0",
"swap_y",       "0",
"scale",        "14",
"width",        "256",
"height",       "64",
"alpha",        "1",
NULL);
#endif
*/
}
static void SCR_Hud_GameSummary(hud_t* hud)
{
	int x, y;
	int height = 8;
	int width = 0;
	char* format_string;
	int icon_size = 32;

	static cvar_t
		*hud_gamesummary_style = NULL,
		*hud_gamesummary_onlytp,
		*hud_gamesummary_scale,
		*hud_gamesummary_format,
		*hud_gamesummary_circles,
		*hud_gamesummary_ratio,
		*hud_gamesummary_flash;

	if (hud_gamesummary_style == NULL)    // first time
	{
		hud_gamesummary_style               = HUD_FindVar(hud, "style");
		hud_gamesummary_onlytp              = HUD_FindVar(hud, "onlytp");
		hud_gamesummary_scale               = HUD_FindVar(hud, "scale");
		hud_gamesummary_format              = HUD_FindVar(hud, "format");
		hud_gamesummary_circles             = HUD_FindVar(hud, "circles");
		hud_gamesummary_ratio               = HUD_FindVar(hud, "ratio");
		hud_gamesummary_flash               = HUD_FindVar(hud, "flash");
	}

	// Don't show when not in teamplay/demoplayback.
	if (!cls.mvdplayback || !HUD_ShowInDemoplayback(hud_gamesummary_onlytp->value)) {
		HUD_PrepareDraw(hud, 0, 0, &x, &y);
		return;
	}

	// Measure for width
	icon_size = 8 * bound(1, hud_gamesummary_ratio->value, 8);
	width = strlen(hud_gamesummary_format->string) * icon_size * hud_gamesummary_scale->value;
	height = (icon_size + (hud_gamesummary_style->integer ? 8 : 0)) * hud_gamesummary_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		int text_offset = hud_gamesummary_scale->value * (icon_size / 2 - 4);
		int icon_offset = 0;

		if (hud_gamesummary_style->integer == 1) {
			text_offset = icon_size * hud_gamesummary_scale->value;
		}
		else if (hud_gamesummary_style->integer == 2) {
			text_offset = 0;
			icon_offset = 8 * hud_gamesummary_scale->value;
		}

		for (format_string = hud_gamesummary_format->string; *format_string; ++format_string) {
			int team = isupper(format_string[0]) ? 0 : 1;
			int background_tex = 0;
			int value = 0;
			float alpha = 1.0f;
			float last_taken = 0.0f;
			byte* background_color = NULL;
			byte mega_color[] = { 0x00, 0xAF, 0x00 };
			byte ra_color[] = { 0xFF, 0x00, 0x00 };
			byte ya_color[] = { 0xFF, 0xFF, 0x00 };
			byte ga_color[] = { 0x00, 0xBF, 0x00 };
			byte rl_color[] = { 0xFF, 0x1F, 0x3F };
			byte lg_color[] = { 0x2F, 0xAA, 0xAA };
			byte quad_color[] = { 0x00, 0x5F, 0xFF };
			byte pent_color[] = { 0xFF, 0x00, 0x00 };
			byte ring_color[] = { 0xFF, 0xFF, 0x00 };

			switch (tolower(format_string[0])) {
			case 'm':
				background_tex = Mod_SimpleTextureForHint(MOD_MEGAHEALTH, 0);
				value = sorted_teams[team].mh_taken;
				background_color = mega_color;
				last_taken = sorted_teams[team].mh_lasttime;
				break;
			case 'r':
				background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 2);
				value = sorted_teams[team].ra_taken;
				background_color = ra_color;
				last_taken = sorted_teams[team].ra_lasttime;
				break;
			case 'y':
				background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 1);
				value = sorted_teams[team].ya_taken;
				background_color = ya_color;
				last_taken = sorted_teams[team].ya_lasttime;
				break;
			case 'g':
				background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 0);
				value = sorted_teams[team].ga_taken;
				background_color = ga_color;
				last_taken = sorted_teams[team].ga_lasttime;
				break;
			case 'o':
				background_tex = Mod_SimpleTextureForHint(MOD_ROCKETLAUNCHER, 0);
				value = sorted_teams[team].rlcount;
				background_color = rl_color;
				last_taken = value ? -1 : 0;
				break;
			case 'l':
				background_tex = Mod_SimpleTextureForHint(MOD_LIGHTNINGGUN, 0);
				value = sorted_teams[team].lgcount;
				background_color = lg_color;
				last_taken = value ? -1 : 0;
				break;
			case 'w':
				background_tex = Mod_SimpleTextureForHint(MOD_ROCKETLAUNCHER, 0);
				value = sorted_teams[team].weapcount;
				background_color = rl_color;
				last_taken = value ? -1 : 0;
				break;
			case 'q':
				background_tex = Mod_SimpleTextureForHint(MOD_QUAD, 0);
				value = sorted_teams[team].quads_taken;
				background_color = quad_color;
				last_taken = sorted_teams[team].q_lasttime;
				break;
			case 'p':
				background_tex = Mod_SimpleTextureForHint(MOD_PENT, 0);
				value = sorted_teams[team].pents_taken;
				background_color = pent_color;
				last_taken = sorted_teams[team].p_lasttime;
				break;
			case 'e':
				background_tex = Mod_SimpleTextureForHint(MOD_RING, 0);
				value = sorted_teams[team].rings_taken;
				background_color = ring_color;
				last_taken = sorted_teams[team].r_lasttime;
				break;
			}

			alpha = last_taken == -1 ? 1.0f : 0.3f;
			if (last_taken > 0 && cls.demotime >= last_taken) {
				if (hud_gamesummary_flash->integer && cls.demotime - last_taken <= 1.0f) {
					// Flash for first second
					alpha = (int)((cls.demotime - last_taken) * 10) % 4 >= 2 ? 0.5f : 1.0f;
				}
				else if (cls.demotime - last_taken <= 2.3f) {
					// Then solid colour
					alpha = 1.0f;
				}
				else if (cls.demotime - last_taken <= 3.0f) {
					// Then fade back to normal
					alpha = bound(0.3f, 1.0f - (cls.demotime - last_taken - 2.3f), 1.0f);
				}
			}

			{
				char buffer[10];
				int length;

				snprintf(buffer, sizeof(buffer), "%d", value);
				length = strlen(buffer);

				if (hud_gamesummary_circles->integer && background_color) {
					float half_width = icon_size * 0.5f * hud_gamesummary_scale->value;

					Draw_AlphaCircleFillRGB(x + half_width, y + icon_offset + half_width, half_width, RGBA_TO_COLOR(background_color[0], background_color[1], background_color[2], alpha / 2 * 255));
				}
				else if (background_tex) {
					Draw_2dAlphaTexture(x, y + icon_offset, icon_size * hud_gamesummary_scale->value, icon_size * hud_gamesummary_scale->value, background_tex, alpha);
				}

				if (value) {
					Draw_SString(x + hud_gamesummary_scale->value * (icon_size / 2 - length * 8 / 2), y + text_offset, buffer, hud_gamesummary_scale->value);
				}
			}

			x += icon_size * hud_gamesummary_scale->value;
		}
	}
}

const char* HUD_FirstTeam(void)
{
	if (n_teams) {
		return sorted_teams[0].name;
	}
	return "";
}
