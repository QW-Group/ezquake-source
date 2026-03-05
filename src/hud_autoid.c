/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: cl_screen.c,v 1.156 2007-10-29 00:56:47 qqshka Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "teamplay.h"
#include "fonts.h"
#include "r_matrix.h"
#include "r_local.h"

/*********************************** AUTOID ***********************************/

// Not static so it can be toggled in menu
cvar_t scr_autoid                                  = { "scr_autoid", "5" };
static cvar_t scr_autoid_weapons                   = { "scr_autoid_weapons", "2" };
static cvar_t scr_autoid_namelength                = { "scr_autoid_namelength", "0" };
static cvar_t scr_autoid_barlength                 = { "scr_autoid_barlength", "16" };
static cvar_t scr_autoid_weaponicon                = { "scr_autoid_weaponicon", "1" };
static cvar_t scr_autoid_scale                     = { "scr_autoid_scale", "1" };
static cvar_t scr_autoid_proportional              = { "scr_autoid_proportional", "0" };

// These aren't static as they're also used for multiview hud
cvar_t scr_autoid_healthbar_bg_color               = { "scr_autoid_healthbar_bg_color", "180 115 115 100", CVAR_COLOR };
cvar_t scr_autoid_healthbar_normal_color           = { "scr_autoid_healthbar_normal_color", "80 0 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_mega_color             = { "scr_autoid_healthbar_mega_color", "255 0 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_two_mega_color         = { "scr_autoid_healthbar_two_mega_color", "255 100 0 255", CVAR_COLOR };
cvar_t scr_autoid_healthbar_unnatural_color        = { "scr_autoid_healthbar_unnatural_color", "255 255 255 255", CVAR_COLOR };

static cvar_t scr_autoid_armorbar_green_armor      = { "scr_autoid_armorbar_green_armor", "25 170 0 255", CVAR_COLOR };
static cvar_t scr_autoid_armorbar_yellow_armor     = { "scr_autoid_armorbar_yellow_armor", "255 220 0 255", CVAR_COLOR };
static cvar_t scr_autoid_armorbar_red_armor        = { "scr_autoid_armorbar_red_armor", "255 0 0 255", CVAR_COLOR };

typedef struct player_autoid_s {
	float x, y;
	player_info_t* player;

	// antilag indicators (debugging antilag queries)
	float rewind_x1, rewind_y1; // as rewound by server
	float rewind_x2, rewind_y2; // current position, for rewound line
	float client_x1, client_y1; // as claimed by the client
	float client_x2, client_y2; // current position, for client line
	qbool rewind_valid, client_valid;
} autoid_player_t;

// For 2-pass multiview... [<view-num>][<other-player>]
static autoid_player_t saved_autoids[4][MAX_CLIENTS];
static int saved_autoid_count[4];

static autoid_player_t autoids[MAX_CLIENTS];
static int autoid_count;

#define AUTOID_HEALTHBAR_OFFSET_Y			16

#define AUTOID_ARMORBAR_OFFSET_Y			(AUTOID_HEALTHBAR_OFFSET_Y + 5)
#define AUTOID_ARMORNAME_OFFSET_Y			(AUTOID_ARMORBAR_OFFSET_Y + 8 + 2)
#define AUTOID_ARMORNAME_OFFSET_X			8

#define AUTOID_WEAPON_OFFSET_Y				AUTOID_HEALTHBAR_OFFSET_Y
#define AUTOID_WEAPON_OFFSET_X				2

void SCR_SaveAutoID(void)
{
	int view_num = CL_MultiviewCurrentView() - 1;

	if (view_num >= 0 && view_num < 4) {
		memcpy(saved_autoids[view_num], autoids, sizeof(saved_autoids[view_num]));
		saved_autoid_count[view_num] = autoid_count;
	}
}

void SCR_RestoreAutoID(void)
{
	int view_num = CL_MultiviewCurrentView() - 1;

	if (view_num >= 0 && view_num < 4) {
		memcpy(autoids, saved_autoids[view_num], sizeof(autoids));
		autoid_count = saved_autoid_count[view_num];
	}
}

void SCR_SetupAutoID(void)
{
	int j, tracknum = -1;
	float winz, *origin;
	player_state_t *state;
	player_info_t *info;
	centity_t *cent;
	item_vis_t visitem;
	autoid_player_t *id;

	autoid_count = 0;

	if (!scr_autoid.value) {
		return;
	}

	if (cls.state != ca_active || !cl.validsequence || cl.intermission) {
		return;
	}

	if (!cls.demoplayback && !cl.spectator) {
		return;
	}

	if (cl.spectator) {
		tracknum = Cam_TrackNum();
	}

	VectorCopy(vpn, visitem.forward);
	VectorCopy(vright, visitem.right);
	VectorCopy(vup, visitem.up);
	VectorCopy(r_origin, visitem.vieworg);

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	cent = &cl_entities[1];

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++, cent++) {
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator) {
			continue;
		}

		if ((state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) ||
			state->modelindex == cl_modelindices[mi_h_player]) {
			continue;
		}

		origin = cent->lerp_origin;

		// FIXME: In multiview, this will detect some players being outside of the view even though
		// he's visible on screen, this only happens in some cases.
		if (R_CullSphere(origin, 0)) {
			continue;
		}

		VectorCopy(origin, visitem.entorg);
		visitem.entorg[2] += 27;
		VectorSubtract(visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct(visitem.dir, visitem.forward);
		visitem.radius = 25;

		if (!TP_IsItemVisible(&visitem)) {
			continue;
		}

		id = &autoids[autoid_count];
		id->player = info;
		if (R_Project3DCoordinates(origin[0], origin[1], origin[2] + 28, &id->x, &id->y, &winz)) {
			autoid_count++;

			id->rewind_valid = id->client_valid = false;
			if (state->antilag_flags & dbg_antilag_rewind_present) {
				id->rewind_valid = R_Project3DCoordinates(state->rewind_origin[0], state->rewind_origin[1], state->rewind_origin[2] + 16, &id->rewind_x1, &id->rewind_y1, &winz);
				id->rewind_valid &= R_Project3DCoordinates(origin[0], origin[1], origin[2] + 16, &id->rewind_x2, &id->rewind_y2, &winz);
			}
			if (state->antilag_flags & dbg_antilag_client_present) {
				id->client_valid = R_Project3DCoordinates(state->client_origin[0], state->client_origin[1], state->client_origin[2] + 18, &id->client_x1, &id->client_y1, &winz);
				id->client_valid &= R_Project3DCoordinates(origin[0], origin[1], origin[2] + 18, &id->client_x2, &id->client_y2, &winz);
			}
		}
	}
}

static void SCR_DrawAutoIDStatus(autoid_player_t *autoid_p, int x, int y, float scale, qbool proportional)
{
	char armor_name[20];
	char weapon_name[20];
	int bar_length;

	if (scr_autoid_barlength.integer > 0) {
		bar_length = scr_autoid_barlength.integer;
	}
	else if (scr_autoid_namelength.integer >= 1) {
		float fixed = FontFixedWidth(scr_autoid_namelength.integer, false, scale, proportional);
		float particular = Draw_StringLength(autoid_p->player->name, -1, scale, proportional);

		bar_length = min(fixed, particular) * 0.5;
	}
	else {
		bar_length = Draw_StringLength(autoid_p->player->name, -1, scale, proportional) * 0.5;
	}

	// Draw health above the name.
	if (scr_autoid.integer >= 2) {
		int health;
		int health_length;

		health = autoid_p->player->stats[STAT_HEALTH];
		health = min(100, health);
		health_length = Q_rint((bar_length / 100.0) * health);

		// Normal health.
		Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_bg_color.color));
		Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_normal_color.color));

		health = autoid_p->player->stats[STAT_HEALTH];

		// Mega health
		if (health > 100 && health <= 200) {
			health_length = Q_rint((bar_length / 100.0) * (health - 100));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_mega_color.color));
		}
		else if (health > 200 && health <= 250) {
			// Super health.
			health_length = Q_rint((bar_length / 100.0) * (health - 200));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_mega_color.color));
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, health_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_two_mega_color.color));
		}
		else if (health > 250) {
			// Crazy health.
			// This will never happen during a normal game.
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_HEALTHBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, RGBAVECT_TO_COLOR(scr_autoid_healthbar_unnatural_color.color));
		}
	}

	// Draw armor.
	if (scr_autoid.integer >= 2) {
		int armor;
		int armor_length;
		cvar_t* armor_color = NULL;

		armor = autoid_p->player->stats[STAT_ARMOR];
		armor = min(100, armor);
		armor_length = Q_rint((bar_length / 100.0) * armor);

		if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR1) {
			armor_color = &scr_autoid_armorbar_green_armor;
		}
		else if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR2) {
			armor_color = &scr_autoid_armorbar_yellow_armor;
		}
		else if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR3) {
			armor_color = &scr_autoid_armorbar_red_armor;
		}

		if (armor_color != NULL) {
			color_t background = RGBA_TO_COLOR(armor_color->color[0], armor_color->color[1], armor_color->color[2], 50);
			color_t foreground = RGBAVECT_TO_COLOR(armor_color->color);

			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_ARMORBAR_OFFSET_Y * scale, bar_length * 2 * scale, 4 * scale, background);
			Draw_AlphaFillRGB(x - bar_length * scale, y - AUTOID_ARMORBAR_OFFSET_Y * scale, armor_length * 2 * scale, 4 * scale, foreground);
		}
	}

	// Draw the name of the armor type.
	if (scr_autoid.integer >= 3 && scr_autoid.integer < 6 && autoid_p->player->stats[STAT_ITEMS] & (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) {
		if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR1) {
			strlcpy(armor_name, "&c0f0GA", sizeof(armor_name));
		}
		else if (autoid_p->player->stats[STAT_ITEMS] & IT_ARMOR2) {
			strlcpy(armor_name, "&cff0YA", sizeof(armor_name));
		}
		else {
			strlcpy(armor_name, "&cf00RA", sizeof(armor_name));
		}

		Draw_SColoredStringBasic(
			x - AUTOID_ARMORNAME_OFFSET_X * scale,
			y - AUTOID_ARMORNAME_OFFSET_Y * scale,
			armor_name, 0, scale, proportional
		);
	}

	if (scr_autoid_weapons.integer > 0 && (scr_autoid.integer >= 4)) {
		// Draw the players weapon.
		int best_weapon = -1;
		extern mpic_t *sb_weapons[7][8];
		mpic_t *weapon_pic = NULL;

		best_weapon = BestWeaponFromStatItems(autoid_p->player->stats[STAT_ITEMS]);

		switch (best_weapon) {
			case IT_SHOTGUN:
				weapon_pic = sb_weapons[0][0];
				strlcpy(weapon_name, "SG", sizeof(weapon_name));
				break;
			case IT_SUPER_SHOTGUN:
				weapon_pic = sb_weapons[0][1];
				strlcpy(weapon_name, "BS", sizeof(weapon_name));
				break;
			case IT_NAILGUN:
				weapon_pic = sb_weapons[0][2];
				strlcpy(weapon_name, "NG", sizeof(weapon_name));
				break;
			case IT_SUPER_NAILGUN:
				weapon_pic = sb_weapons[0][3];
				strlcpy(weapon_name, "SN", sizeof(weapon_name));
				break;
			case IT_GRENADE_LAUNCHER:
				weapon_pic = sb_weapons[0][4];
				strlcpy(weapon_name, "GL", sizeof(weapon_name));
				break;
			case IT_ROCKET_LAUNCHER:
				weapon_pic = sb_weapons[0][5];
				strlcpy(weapon_name, "RL", sizeof(weapon_name));
				break;
			case IT_LIGHTNING:
				weapon_pic = sb_weapons[0][6];
				strlcpy(weapon_name, "LG", sizeof(weapon_name));
				break;
			default:
				// No weapon.
				break;
		}

		if (weapon_pic != NULL && best_weapon > 0 && ((scr_autoid.integer == 4 && best_weapon == 7) || (scr_autoid.integer > 4 && best_weapon >= scr_autoid_weapons.integer))) {
			if (scr_autoid_weaponicon.value) {
				Draw_SSubPic(
					x - (bar_length + weapon_pic->width + AUTOID_WEAPON_OFFSET_X) * scale,
					y - (AUTOID_HEALTHBAR_OFFSET_Y + Q_rint((weapon_pic->height / 2.0))) * scale,
					weapon_pic,
					0,
					0,
					weapon_pic->width,
					weapon_pic->height,
					scale
				);
			}
			else {
				Draw_SColoredStringBasic(
					x - (bar_length + 16 + AUTOID_WEAPON_OFFSET_X) * scale,
					y - (AUTOID_HEALTHBAR_OFFSET_Y + 4) * scale,
					weapon_name, 1, scale, proportional
				);
			}
		}
	}
}

void SCR_DrawAntilagIndicators(void)
{
	int i;
	extern cvar_t cl_debug_antilag_lines;
	extern cvar_t cl_debug_antilag_view;
	extern cvar_t cl_debug_antilag_hud;
	antilag_stats_t *stats;
	char reasons[256];
	char line[256];
	unsigned int known_flags;
	unsigned int unknown_flags;
	int x = 8;
	int y = 40;
	double rewind_ms;
	double command_delta_ms;
	double latency_delta_ms;
	double avg_rewind_delta;

	if (cl.intermission) {
		return;
	}

	if (cl_debug_antilag_lines.integer && (cls.demoplayback || cl.spectator)) {
		for (i = 0; i < autoid_count; ++i) {
			color_t r_color = RGBA_TO_COLOR(255, 0, 0, 255);
			color_t c_color = RGBA_TO_COLOR(0, 255, 0, 255);
			color_t rc_color = RGBA_TO_COLOR(0, 0, 255, 255);
			float r_x1 = autoids[i].rewind_x1 * vid.width / glwidth;
			float r_y1 = (glheight - autoids[i].rewind_y1) * vid.height / glheight;
			float r_x2 = autoids[i].rewind_x2 * vid.width / glwidth;
			float r_y2 = (glheight - autoids[i].rewind_y2) * vid.height / glheight;
			float c_x1 = autoids[i].client_x1 * vid.width / glwidth;
			float c_y1 = (glheight - autoids[i].client_y1) * vid.height / glheight;
			float c_x2 = autoids[i].client_x2 * vid.width / glwidth;
			float c_y2 = (glheight - autoids[i].client_y2) * vid.height / glheight;

			if (cl_debug_antilag_view.integer == 0) {
				// player shown in current server position, draw from rewound & client
				if (autoids[i].rewind_valid) {
					Draw_AlphaLineRGB(r_x1, r_y1, r_x2, r_y2, 2, r_color);
				}
				if (autoids[i].client_valid) {
					Draw_AlphaLineRGB(c_x1, c_y1, c_x2, c_y2, 2, c_color);
				}
			}
			else if (cl_debug_antilag_view.integer == 1) {
				// player shown in rewound position, draw from current & client
				if (autoids[i].rewind_valid) {
					Draw_AlphaLineRGB(r_x1, r_y1, r_x2, r_y2, 2, r_color);
				}
				if (autoids[i].rewind_valid && autoids[i].client_valid) {
					Draw_AlphaLineRGB(c_x1, c_y1, r_x1, r_y1, 2, rc_color);
				}
			}
			else if (cl_debug_antilag_view.integer == 2) {
				// player shown in client position, draw to current & rewound
				if (autoids[i].rewind_valid && autoids[i].client_valid) {
					Draw_AlphaLineRGB(r_x1, r_y1, c_x1, c_y1, 2, rc_color);
				}
				if (autoids[i].client_valid) {
					Draw_AlphaLineRGB(c_x1, c_y1, c_x2, c_y2, 2, c_color);
				}
			}
		}
	}

	if (!cl_debug_antilag_hud.integer || cl.viewplayernum < 0 || cl.viewplayernum >= MAX_CLIENTS) {
		return;
	}

	stats = &cl.antilag_stats[cl.viewplayernum];
	if (stats->server_time <= 0 && stats->target_time <= 0 && !stats->reason_flags && stats->client_rewind_samples <= 0) {
		return;
	}

	reasons[0] = '\0';
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_DISABLED) strlcat(reasons, "disabled|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_ENABLED) strlcat(reasons, "enabled|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_PING_REJECT) strlcat(reasons, "ping_reject|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_CMD_FALLBACK) strlcat(reasons, "cmd_fallback|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_CLAMP_MAXUNLAG) strlcat(reasons, "clamp_unlag|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_CLAMP_FUTURE) strlcat(reasons, "clamp_future|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_PREFILTER_PVS) strlcat(reasons, "prefilter_pvs|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_PREFILTER_TEAM) strlcat(reasons, "prefilter_team|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_HIST_OLDEST) strlcat(reasons, "oldest|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_HIST_NEWEST) strlcat(reasons, "newest|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_HIST_BAD_INTERVAL) strlcat(reasons, "bad_interval|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_HIST_MISS) strlcat(reasons, "history_miss|", sizeof(reasons));
	if (stats->reason_flags & MVD_PEXT1_ANTILAG_REASON_ROLLOUT_GATE) strlcat(reasons, "rollout_gate|", sizeof(reasons));
	known_flags =
		MVD_PEXT1_ANTILAG_REASON_DISABLED
		| MVD_PEXT1_ANTILAG_REASON_ENABLED
		| MVD_PEXT1_ANTILAG_REASON_PING_REJECT
		| MVD_PEXT1_ANTILAG_REASON_CMD_FALLBACK
		| MVD_PEXT1_ANTILAG_REASON_CLAMP_MAXUNLAG
		| MVD_PEXT1_ANTILAG_REASON_CLAMP_FUTURE
		| MVD_PEXT1_ANTILAG_REASON_PREFILTER_PVS
		| MVD_PEXT1_ANTILAG_REASON_PREFILTER_TEAM
		| MVD_PEXT1_ANTILAG_REASON_HIST_OLDEST
		| MVD_PEXT1_ANTILAG_REASON_HIST_NEWEST
		| MVD_PEXT1_ANTILAG_REASON_HIST_BAD_INTERVAL
		| MVD_PEXT1_ANTILAG_REASON_HIST_MISS
		| MVD_PEXT1_ANTILAG_REASON_ROLLOUT_GATE;
	unknown_flags = stats->reason_flags & ~known_flags;
	if (unknown_flags) {
		char token[32];

		snprintf(token, sizeof(token), "unknown:0x%08x|", unknown_flags);
		strlcat(reasons, token, sizeof(reasons));
	}
	if (!reasons[0]) {
		strlcpy(reasons, "none", sizeof(reasons));
	}
	else if (reasons[strlen(reasons) - 1] == '|') {
		reasons[strlen(reasons) - 1] = '\0';
	}

	rewind_ms = max(0.0, (stats->server_time - stats->target_time) * 1000.0);
	command_delta_ms = (stats->command_target_time - stats->target_time) * 1000.0;
	latency_delta_ms = (stats->latency_target_time - stats->target_time) * 1000.0;
	avg_rewind_delta = (stats->client_rewind_samples > 0 ? (stats->client_rewind_distance / stats->client_rewind_samples) : 0.0);
	snprintf(line, sizeof(line), "AL rewind=%0.1fms avg=%0.1fu ping=%ums drop=%u", rewind_ms, avg_rewind_delta, stats->ping_ms, stats->net_drop);
	Draw_String(x, y, line);
	y += 8;
	snprintf(line, sizeof(line), "AL cmd=%+0.1fms lat=%+0.1fms interp=%0.1fms one-way=%0.1fms win=%0.1fms",
		command_delta_ms, latency_delta_ms, stats->interp_delay * 1000.0, stats->one_way_latency * 1000.0, stats->command_window * 1000.0);
	Draw_String(x, y, line);
	y += 8;
	snprintf(line, sizeof(line), "AL reason=%s", reasons);
	Draw_String(x, y, line);
	y += 8;

	if (cl_debug_antilag_hud.integer >= 2) {
		snprintf(line, sizeof(line), "AL scanned=%u kept=%u pre=%u pvs=%u team=%u", stats->scanned, stats->kept, stats->prefiltered, stats->pvs_rejects, stats->team_rejects);
		Draw_String(x, y, line);
		y += 8;
		snprintf(line, sizeof(line), "AL fallback o/n/b=%u/%u/%u miss=%u build=%0.3fms", stats->oldest_fallbacks, stats->newest_fallbacks, stats->bad_interval_fallbacks, stats->history_misses, stats->build_seconds * 1000.0);
		Draw_String(x, y, line);
		y += 8;
		snprintf(line, sizeof(line), "AL server=%0.3f target=%0.3f", stats->server_time, stats->target_time);
		Draw_String(x, y, line);
	}
}

void SCR_DrawAutoID(void)
{
	int i, x, y;
	float scale;
	qbool proportional = scr_autoid_proportional.integer;

	if (!scr_autoid.value || (!cls.demoplayback && !cl.spectator) || cl.intermission) {
		return;
	}

	for (i = 0; i < autoid_count; i++) {
		x = autoids[i].x * vid.width / glwidth;
		y = (glheight - autoids[i].y) * vid.height / glheight;
		scale = (scr_autoid_scale.value > 0 ? scr_autoid_scale.value : 1.0);

		if (scr_autoid.integer) {
			if (scr_autoid_namelength.integer >= 1 && scr_autoid_namelength.integer < MAX_SCOREBOARDNAME) {
				char name[MAX_SCOREBOARDNAME];

				strlcpy(name, autoids[i].player->name, sizeof(name));
				name[scr_autoid_namelength.integer] = 0;
				Draw_SString(x - Draw_StringLength(name, -1, 0.5 * scale, proportional), y - 8 * scale, name, scale, proportional);
			}
			else {
				Draw_SString(x - Draw_StringLength(autoids[i].player->name, -1, 0.5 * scale, proportional), y - 8 * scale, autoids[i].player->name, scale, proportional);
			}
		}

		// We only have health/armor info for all players when in demo playback.
		if (cls.mvdplayback && scr_autoid.value >= 2) {
			SCR_DrawAutoIDStatus(&autoids[i], x, y, scale, proportional);
		}
	}
}

void SCR_RegisterAutoIDCvars(void)
{
	if (!host_initialized) {
		Cvar_Register(&scr_autoid);
		Cvar_Register(&scr_autoid_weapons);
		Cvar_Register(&scr_autoid_namelength);
		Cvar_Register(&scr_autoid_barlength);
		Cvar_Register(&scr_autoid_weaponicon);
		Cvar_Register(&scr_autoid_scale);
		Cvar_Register(&scr_autoid_healthbar_bg_color);
		Cvar_Register(&scr_autoid_healthbar_normal_color);
		Cvar_Register(&scr_autoid_healthbar_mega_color);
		Cvar_Register(&scr_autoid_healthbar_two_mega_color);
		Cvar_Register(&scr_autoid_healthbar_unnatural_color);
		Cvar_Register(&scr_autoid_proportional);

		Cvar_Register(&scr_autoid_armorbar_green_armor);
		Cvar_Register(&scr_autoid_armorbar_yellow_armor);
		Cvar_Register(&scr_autoid_armorbar_red_armor);
	}
}
