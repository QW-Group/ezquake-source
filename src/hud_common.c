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
#include "tr_types.h"
#include "rulesets.h"
#include "utils.h"
#include "sbar.h"
#include "hud.h"
#include "hud_common.h"
#include "Ctrl.h"
#include "console.h"
#include "teamplay.h"
#include "mvd_utils.h"
#include "mvd_utils_common.h"
#include "fonts.h"

void TeamHold_DrawPercentageBar(
	int x, int y, int width, int height,
	float team1_percent, float team2_percent,
	int team1_color, int team2_color,
	int show_text, int vertical,
	int vertical_text, float opacity, float scale,
	qbool proportional
);

#ifndef STAT_MINUS
#define STAT_MINUS		10
#endif

void SCR_HUD_DrawTracker(hud_t* hud);
void SCR_HUD_WeaponStats(hud_t *hud);
void WeaponStats_HUDInit(void);
void TeamInfo_HudInit(void);
void Speed_HudInit(void);
void TeamHold_HudInit(void);
void Clock_HudInit(void);
void Ammo_HudInit(void);
void Items_HudInit(void);
void Net_HudInit(void);
void Guns_HudInit(void);
void Groups_HudInit(void);
void Armor_HudInit(void);
void Health_HudInit(void);
void GameSummary_HudInit(void);
void Performance_HudInit(void);
void Scores_HudInit(void);
void Face_HudInit(void);
void Frags_HudInit(void);
void Tracking_HudInit(void);
void CenterPrint_HudInit(void);
void Qtv_HudInit(void);

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
static void OnRemovePrefixesChange(cvar_t* var, char* value, qbool* cancel);
qbool autohud_loaded = false;
cvar_t hud_planmode = {"hud_planmode",   "0"};
cvar_t mvd_autohud = {"mvd_autohud", "0", 0, OnAutoHudChange};
cvar_t hud_digits_trim = {"hud_digits_trim", "1"};
static cvar_t hud_name_remove_prefixes = { "hud_name_remove_prefixes", "", 0, OnRemovePrefixesChange };

sort_players_info_t		sorted_players_by_frags[MAX_CLIENTS];
sort_players_info_t		sorted_players[MAX_CLIENTS];
sort_teams_info_t		sorted_teams[MAX_CLIENTS];
int       n_teams;
int       n_players;
int       n_spectators;
int       active_player_position = -1;
int       active_team_position = -1;

int hud_stats[MAX_CL_STATS];

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
// draw HUD notify
//

void SCR_HUD_DrawNotify(hud_t* hud)
{
	static cvar_t* hud_notify_rows = NULL;
	static cvar_t* hud_notify_scale;
	static cvar_t* hud_notify_time;
	static cvar_t* hud_notify_cols;
	static cvar_t* hud_notify_proportional;

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
		hud_notify_proportional = HUD_FindVar(hud, "proportional");
	}

	// Deliberately leaving this alone for the moment, not doing FontFixedWidth()...
	chars_per_line = (hud_notify_cols->integer > 0 ? hud_notify_cols->integer : con_linewidth);
	height = Q_rint ((con_linewidth / chars_per_line) * hud_notify_rows->integer * 8 * hud_notify_scale->value);
	width = 8 * chars_per_line * hud_notify_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		SCR_DrawNotify(x, y, hud_notify_scale->value, hud_notify_time->integer, hud_notify_rows->integer, chars_per_line, hud_notify_proportional->integer);
	}
}

// =======================================================
//
//  s t a t u s   b a r   e l e m e n t s
//
//

// status numbers
void SCR_HUD_DrawNum2(
	hud_t *hud, int num, qbool low,
	float scale, int style, int digits, char *s_align, qbool proportional, qbool draw_content,
	byte *text_color_low, byte *text_color_normal
)
{
	extern mpic_t *sb_nums[2][11];

	int  i;
	char buf[sizeof(int) * 3]; // each byte need <= 3 chars
	int  len;

	int width, height, x, y;
	int size;
	int align;

	clrinfo_t clr = { .i = 0 };

	clamp(num, -99999, 999999);
	scale = max(scale, 0.01);
	if (digits > 0) {
		clamp(digits, 1, 6);
	}
	else {
		digits = 0; // auto-resize
	}

	align = 2;
	switch (tolower(s_align[0])) {
		default:
		case 'l':   // 'l'eft
			align = 0;
			break;
		case 'c':   // 'c'enter
			align = 1;
			break;
		case 'r':   // 'r'ight
			align = 2;
			break;
	}

	snprintf(buf, sizeof (buf), "%d", (style == 2 || style == 3) ? num : abs(num));
	if (digits) {
		switch (hud_digits_trim.integer) {
			case 0: // 10030 -> 999
				len = strlen(buf);
				if (len > digits) {
					char *p = buf;
					if (num < 0) {
						*p++ = '-';
					}
					for (i = (num < 0) ? 1 : 0; i < digits; i++) {
						*p++ = '9';
					}
					*p = 0;
					len = digits;
				}
				break;
			default:
			case 1: // 10030 -> 030
				len = strlen(buf);
				if(len > digits) {
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
	else {
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

	if (digits) {
		if (size == 8) {
			width = FontFixedWidth(digits, scale, true, proportional);
		}
		else {
			width = digits * size * scale;
		}
	}
	else {
		if (size == 8) {
			width = FontFixedWidth(len, scale, true, proportional);
		}
		else {
			width = len * size * scale;
		}
	}

	height = size * scale;

	switch (style)
	{
		case 1:
		case 3:
			if (!HUD_PrepareDraw(hud, width, height, &x, &y) || !draw_content) {
				return;
			}
			switch (align)
			{
				case 0: break;
				case 1: x += width / 2 - Draw_StringLength(buf, -1, scale, proportional) / 2; break;
				case 2: x += width - Draw_StringLength(buf, -1, scale, proportional); break;
			}

			if (low) {
				if (text_color_low != NULL)
				{
					clr.c = RGBAVECT_TO_COLOR(text_color_low);
					Draw_SColoredAlphaString(x, y, buf, &clr, 1, 0, scale, 1.0, proportional);
				}
				else
				{
					Draw_SAlt_String(x, y, buf, scale, proportional);
				}
			}
			else {
				if(style == 3) {
					// golden numbers
					for(i = 0; i < len; i++) {
						if(isdigit(buf[i])) {
							buf[i] = 18 + buf[i] - '0';
						}
					}
				}

				if (text_color_normal != NULL)
				{
					clr.c = RGBAVECT_TO_COLOR(text_color_normal);
					Draw_SColoredAlphaString(x, y, buf, &clr, 1, 0, scale, 1.0, proportional);
				}
				else
				{
					Draw_SString(x, y, buf, scale, proportional);
				}
			}
			break;

		case 0:
		case 2:
		default:
			if (!HUD_PrepareDraw(hud, width, height, &x, &y) || !draw_content) {
				return;
			}

			switch (align) {
				case 0: break;
				case 1: x += (width - size * len * scale) / 2; break;
				case 2: x += (width - size * len * scale); break;
			}

			for (i = 0; i < len; i++) {
				if(buf[i] == '-' && style == 2) {
					Draw_STransPic (x, y, sb_nums[low ? 1 : 0][STAT_MINUS], scale);
					x += 24 * scale;
				}
				else {
					Draw_STransPic (x, y, sb_nums[low ? 1 : 0][buf[i] - '0'], scale);
					x += 24 * scale;
				}
			}
			break;
	}
}

void SCR_HUD_DrawNum(
	hud_t* hud, int num, qbool low,
	float scale, int style, int digits, char* s_align, qbool proportional
)
{
	SCR_HUD_DrawNum2(hud, num, low, scale, style, digits, s_align, proportional, true, NULL, NULL);
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
		*hud_stackbar_scale,
		*hud_stackbar_proportional;

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
		hud_stackbar_proportional        = HUD_FindVar(hud, "proportional");
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
				hud_stackbar_scale->value,
				hud_stackbar_proportional->integer
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

void SCR_HUD_DrawItemsClock(hud_t *hud)
{
	extern qbool hud_editor;
	int width, height;
	int x, y;
	static cvar_t
		*hud_itemsclock_timelimit = NULL,
		*hud_itemsclock_style = NULL,
		*hud_itemsclock_scale = NULL,
		*hud_itemsclock_filter = NULL,
		*hud_itemsclock_proportional = NULL,
		*hud_itemsclock_backpacks = NULL;

	if (hud_itemsclock_timelimit == NULL) {
		char val[256];

		hud_itemsclock_timelimit = HUD_FindVar(hud, "timelimit");
		hud_itemsclock_style = HUD_FindVar(hud, "style");
		hud_itemsclock_scale = HUD_FindVar(hud, "scale");
		hud_itemsclock_filter = HUD_FindVar(hud, "filter");
		hud_itemsclock_proportional = HUD_FindVar(hud, "proportional");
		hud_itemsclock_backpacks = HUD_FindVar(hud, "backpacks");

		// Unecessary to parse the item filter string on each frame.
		hud_itemsclock_filter->OnChange = ItemsClock_OnChangeItemFilter;

		// Parse the item filter the first time (trigger the OnChange function above).
		strlcpy(val, hud_itemsclock_filter->string, sizeof(val));
		Cvar_Set(hud_itemsclock_filter, val);
	}

	MVD_ClockList_TopItems_DimensionsGet(hud_itemsclock_timelimit->value, hud_itemsclock_style->integer, &width, &height, hud_itemsclock_scale->value, hud_itemsclock_backpacks->integer, hud_itemsclock_proportional->integer);

	if (hud_editor)
		HUD_PrepareDraw(hud, width, LETTERHEIGHT * hud_itemsclock_scale->value, &x, &y);

	if (!height)
		return;

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	MVD_ClockList_TopItems_Draw(hud_itemsclock_timelimit->value, hud_itemsclock_style->integer, x, y, hud_itemsclock_scale->value, itemsclock_filter, hud_itemsclock_backpacks->integer, hud_itemsclock_proportional->integer);
}

#ifdef WITH_PNG

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
		player_slot = Player_GetSlot(player->string, false);
	}
	else if (cls.mvdplayback) {
		if (player->string[0]) {
			player_slot = Player_GetSlot(player->string, false);
		}
		else {
			// returns -1 if not tracking
			player_slot = Cam_TrackNum();
		}
	}

	b = CL_GetLastCmd(player_slot);

	scale = vscale->value;
	scale = max(0, scale);

	i = 0;
	line1[i++] = b.attack  ? (char)('x' + 128) : 'x';
	line1[i++] = b.forward ? (char)('^' + 128) : '^';
	line1[i++] = b.jump    ? (char)('J' + 128) : 'J';
	line1[i++] = '\0';
	i = 0;
	line2[i++] = b.left    ? (char)('<' + 128) : '<';
	line2[i++] = b.back    ? (char)('_' + 128) : '_';
	line2[i++] = b.right   ? (char)('>' + 128) : '>';
	line2[i++] = '\0';

	width = LETTERWIDTH * strlen(line1) * scale;
	height = LETTERHEIGHT * 2 * scale;

	if (!HUD_PrepareDraw(hud, width ,height, &x, &y))
		return;

	Draw_SString(x, y, line1, scale, false);
	Draw_SString(x, y + LETTERHEIGHT*scale, line2, scale, false);
}

#endif // WITH_PNG

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
		*hud_statictext_textalign,
		*hud_statictext_proportional;

	if (hud_statictext_big == NULL) {
		// first time
		hud_statictext_big = HUD_FindVar(hud, "big");
		hud_statictext_scale = HUD_FindVar(hud, "scale");
		hud_statictext_text = HUD_FindVar(hud, "text");
		hud_statictext_textalign = HUD_FindVar(hud, "textalign");
		hud_statictext_proportional = HUD_FindVar(hud, "proportional");
	}

	// Static text valid for demos/qtv only
	if (!cls.demoplayback) {
		int x, y;
		HUD_PrepareDraw(hud, 0, 0, &x, &y);
		return;
	}

	alignment = 0;
	if (!strcmp(hud_statictext_textalign->string, "right")) {
		alignment = 1;
	}
	else if (!strcmp(hud_statictext_textalign->string, "center")) {
		alignment = 2;
	}

	// convert special characters
	in = hud_statictext_text->string;
	if (strlen(in) >= MAX_MACRO_STRING) {
		in = "error: input string too long";
	}
	in = TP_ParseFunChars(in, false);

	SCR_HUD_MultiLineString(hud, in, hud_statictext_big->integer, alignment, hud_statictext_scale->value, hud_statictext_proportional->integer);
}

void SCR_HUD_MultiLineString(hud_t* hud, const char* in, qbool large_font, int alignment, float scale, qbool proportional)
{
	// find carriage returns
	int x, y;
	const char *line_start = in;
	int max_length = Draw_StringLengthColorsByTerminator(line_start, -1, scale, proportional, '\r');
	char* line_end;
	int lines = 0;
	int character_height;

	while ((line_end = strchr(line_start, '\r'))) {
		int line_length;

		line_start = line_end + 1;
		line_length = Draw_StringLengthColorsByTerminator(line_start, -1, scale, proportional, '\r');
		max_length = max(max_length, line_length);

		++lines;
	}

	if (*line_start) {
		++lines;
	}

	// collapse to invisible if nothing to display
	if (max_length == 0) {
		lines = 0;
	}

	character_height = 8 * scale;
	if (large_font) {
		character_height = 24 * scale;
	}

	if (HUD_PrepareDraw(hud, max_length, lines * character_height, &x, &y)) {
		for (line_start = in; *line_start; line_start = line_end + 1) {
			int line_x = x;
			line_end = strchr(line_start, '\r');

			if (line_end) {
				*line_end = '\0';
			}

			if (alignment == 1) {
				// Right-justified
				line_x += (max_length - Draw_StringLengthColors(line_start, -1, scale, proportional));
			}
			else if (alignment == 2) {
				// Centered
				line_x += (max_length - Draw_StringLengthColors(line_start, -1, scale, proportional)) / 2;
			}

			if (large_font) {
				SCR_DrawWadString(line_x, y, scale, line_start); 
			}
			else {
				Draw_SString(line_x, y, line_start, scale, proportional);
			}

			y += character_height;
			if (!line_end)
				break;
		}
	}
}

float SCR_HUD_TotalStrength(float health, float armorValue, float armorType)
{
	return max(0, min(
		health / (1 - armorType),
		health + armorValue
	));
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

void CommonDraw_Init(void)
{
	int i;

	// variables
	Cvar_SetCurrentGroup(CVAR_GROUP_HUD);
	Cvar_Register(&hud_planmode);
	Cvar_Register(&hud_tp_need);
	Cvar_Register(&hud_digits_trim);
	Cvar_Register(&hud_name_remove_prefixes);
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

	HUD_Register(
		"tracker", NULL, "Shows deaths, frag streaks etc",
		0, ca_active, 0, SCR_HUD_DrawTracker,
		"0", "screen", "left", "top", "0", "48", "0", "0 0 0", NULL,
		"scale", "1",
		"proportional", "0",
		"name_width", "0",
		"align_right", "0",
		"image_scale", "1",
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
		"proportional", "0",
		NULL
	);

#ifdef WITH_PNG
	HUD_Register(
		"keys", NULL, "Shows which keys user does press at the moment",
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
		"proportional", "0",
		"backpacks", "0",
		NULL
	);

	HUD_Register(
		"static_text", NULL, "Static text (demos only).",
		0, ca_active, 0, SCR_HUD_DrawStaticText,
		"0", "screen", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"big", "0",
		"style", "0",
		"scale", "1",
		"text", "",
		"textalign", "left",
		"proportional", "0",
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
		"proportional", "0",
		NULL
	);

	Performance_HudInit();
	Guns_HudInit();
	Groups_HudInit();
	Items_HudInit();
	Ammo_HudInit();
	Armor_HudInit();
	Scores_HudInit();
	Face_HudInit();
	GameSummary_HudInit();
	Net_HudInit();
	Clock_HudInit();
	Speed_HudInit();
	Radar_HudInit();
	WeaponStats_HUDInit();
	TeamInfo_HudInit();
	TeamHold_HudInit();
	Health_HudInit();
	Frags_HudInit();
	Tracking_HudInit();
	CenterPrint_HudInit();
	Qtv_HudInit();
}

const char* HUD_FirstTeam(void)
{
	if (!cl.teamlock1_teamname[0] && n_teams) {
		// setting for the first time when we have teams
		strlcpy(cl.teamlock1_teamname, sorted_teams[0].name, sizeof(cl.teamlock1_teamname));
	}
	else if (n_teams) {
		// check that the team still exists
		int i;
		for (i = 0; i < n_teams; ++i) {
			if (!strcmp(sorted_teams[i].name, cl.teamlock1_teamname)) {
				return cl.teamlock1_teamname; // found
			}
		}
		// not found, reset to first time
		strlcpy(cl.teamlock1_teamname, sorted_teams[0].name, sizeof(cl.teamlock1_teamname));
	}
	else {
		// no teams, clear and set again in the future
		memset(cl.teamlock1_teamname, 0, sizeof(cl.teamlock1_teamname));
	}
	return cl.teamlock1_teamname;
}

void CL_RemovePrefixFromName(int player)
{
	size_t skip;
	char* prefixes, * prefix, * name;
	char normalized_list[256];
	char normalized_name[MAX_SCOREBOARDNAME];
	const char* prefixes_list = hud_name_remove_prefixes.string;

	strlcpy(cl.players[player].shortname, cl.players[player].name, sizeof(cl.players[player].shortname));

	if (!prefixes_list || !prefixes_list[0]) {
		return;
	}

	skip = 0;

	strlcpy(normalized_list, prefixes_list, sizeof(normalized_list));
	prefixes = Q_normalizetext(normalized_list);
	prefix = strtok(prefixes, " ");
	strlcpy(normalized_name, cl.players[player].name, sizeof(normalized_name));
	name = Q_normalizetext(normalized_name);

	while (prefix != NULL) {
		if (strlen(prefix) > skip && strlen(name) > strlen(prefix) && strncasecmp(prefix, name, strlen(prefix)) == 0) {
			skip = strlen(prefix);
			// remove spaces from the new start of the name
			while (name[skip] == ' ') {
				skip++;
			}
			// if it would skip the whole name, just use the whole name
			if (name[skip] == 0) {
				skip = 0;
				break;
			}
		}
		prefix = strtok(NULL, " ");
	}

	strlcpy(cl.players[player].shortname, cl.players[player].name + skip, sizeof(cl.players[player].shortname));
}

static void OnRemovePrefixesChange(cvar_t* var, char* value, qbool* cancel)
{
	int i;

	Cvar_SetIgnoreCallback(var, value);

	for (i = 0; i < sizeof(cl.players) / sizeof(cl.players[0]); ++i) {
		CL_RemovePrefixFromName(i);
	}
}
