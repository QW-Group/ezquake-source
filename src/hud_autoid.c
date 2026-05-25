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
#include "utils.h"
#include "screen.h"

/*********************************** AUTOID ***********************************/

// Not static so it can be toggled in menu
cvar_t scr_autoid                                  = { "scr_autoid", "5" };
static cvar_t scr_autoid_weapons                   = { "scr_autoid_weapons", "2" };
static cvar_t scr_autoid_namelength                = { "scr_autoid_namelength", "0" };
static cvar_t scr_autoid_barlength                 = { "scr_autoid_barlength", "16" };
static cvar_t scr_autoid_weaponicon                = { "scr_autoid_weaponicon", "1" };
static cvar_t scr_autoid_scale                     = { "scr_autoid_scale", "1" };
static cvar_t scr_autoid_ingame					   = { "scr_autoid_ingame", "0" };
static cvar_t scr_autoid_ingame_namelength         = { "scr_autoid_ingame_namelength", "6" };
static cvar_t scr_autoid_ingame_weapon         	   = { "scr_autoid_ingame_weapon", "1" };
static cvar_t scr_autoid_ingame_armor_health	   = { "scr_autoid_ingame_armor_health", "1"};
static cvar_t scr_autoid_yoffset		           = { "scr_autoid_yoffset", "0" };
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
	qbool autoid_ingame = scr_autoid_ingame.integer;
	qbool currentlyplaying = (!cls.demoplayback && !cl.spectator);

	autoid_count = 0;

	if (!scr_autoid.value) {
		return;
	}

	if (cls.state != ca_active || !cl.validsequence || cl.intermission) {
		return;
	}

	if (currentlyplaying && !autoid_ingame) {
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

		// Skip players with no model
		if (!state->modelindex || !cl.model_precache[state->modelindex]) {
			continue;
		}

		// Skip players if camera is too close (viewing through their POV)
		if (cls.demoplayback) {
			vec3_t cam_dist;
			float dist;
			
			VectorSubtract(state->origin, r_refdef.vieworg, cam_dist);
			dist = VectorLength(cam_dist);
			
			if (dist < 25.0f) {
				continue;
			}
		}

		if ((state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) ||
			state->modelindex == cl_modelindices[mi_h_player]) {
			continue;
		}

		// Don't show autoid for eyes while playing a live game
		if (currentlyplaying && (state->modelindex == cl_modelindices[mi_eyes])) {
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
	qbool autoid_ingame = scr_autoid_ingame.integer;
	qbool currentlyplaying = (!cls.demoplayback && !cl.spectator);
	extern cvar_t cl_debug_antilag_lines;
	extern cvar_t cl_debug_antilag_view;

	if (!cl_debug_antilag_lines.integer || (currentlyplaying && !autoid_ingame) || cl.intermission) {
		return;
	}

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

void SCR_DrawAutoID(void)
{
	int i, j, k, x, y, yoffset;
	float scale;
	char name[MAX_SCOREBOARDNAME], tmp[1024];
	qbool proportional = scr_autoid_proportional.integer;
	qbool autoid_ingame = scr_autoid_ingame.integer;
	qbool currentlyplaying = (!cls.demoplayback && !cl.spectator);
	qbool ismyteam;
	static qbool teaminfo_available = false;
	static double last_teaminfo_check = 0;
	ti_player_t *ti_cl;
	extern ti_player_t ti_clients[MAX_CLIENTS];
	extern cvar_t tp_name_rlg;
	extern cvar_t tp_name_rl;
	extern cvar_t tp_name_lg;

	int sorted_autoids[MAX_CLIENTS];

	if (!scr_autoid.value || (currentlyplaying && !autoid_ingame) || cl.intermission) {
		return;
	}

	// Check if teaminfo is available when playing (cached, check every second)
	if (currentlyplaying && autoid_ingame) {
		if (r_refdef2.time - last_teaminfo_check > 1.0) {
			teaminfo_available = false;
			for (i = 0; i < MAX_CLIENTS; i++) {
				if (!cl.players[i].name[0] || cl.players[i].spectator) {
					continue;
				}
				if (cl.teamplay && strcmp(cl.players[i].team, cl.players[cl.playernum].team)) {
					continue; // not on our team
				}
				if (ti_clients[i].time && (ti_clients[i].time + TI_TIMEOUT >= r_refdef2.time)) {
					teaminfo_available = true;
					break;
				}
			}
			last_teaminfo_check = r_refdef2.time;
		}
		
		// If no teaminfo is available, disable autoid_ingame
		if (!teaminfo_available) {
			return;
		}
	}

	// Sort autoids to match clients
	for (i = 0; i < autoid_count; i++) {
		for (j = 0; j < MAX_CLIENTS; j++) {
			if (cl.players[j].userid == autoids[i].player->userid)
				sorted_autoids[i] = j;
		}

		sorted_autoids[i] = bound(0, sorted_autoids[i], MAX_CLIENTS);
	}

	yoffset = 8 + scr_autoid_yoffset.integer;

	for (i = 0; i < autoid_count; i++) {
		x = autoids[i].x * vid.width / glwidth;
		y = (glheight - autoids[i].y) * vid.height / glheight;
		k = sorted_autoids[i];
		scale = (scr_autoid_scale.value > 0 ? scr_autoid_scale.value : 1.0);
		ismyteam = (cl.teamplay && !strcmp(autoids[i].player->team, cl.players[cl.playernum].team));
		ti_cl = &ti_clients[k];
		
		if (scr_autoid.integer) {
			if ((currentlyplaying && !ismyteam)) {
				continue;	// Don't show autoid if playing a live game and player isn't on your team
			}
			else if (currentlyplaying && ismyteam) {	// in-game stuff here
				char *weap_str = "";
				char weap_white_stripped[32];
				char armor_health_str[32];
                char *aclr = "";
				// Skip if not getting player updates, such as if teamoverlay is disabled
				if (ti_cl->time && (ti_cl->time + TI_TIMEOUT >= r_refdef2.time)) {
					if ((ti_cl->items & IT_ROCKET_LAUNCHER) && (ti_cl->items & IT_LIGHTNING))
						weap_str = tp_name_rlg.string;
					else if (ti_cl->items & IT_LIGHTNING)
						weap_str = tp_name_lg.string;
					else if (ti_cl->items & IT_ROCKET_LAUNCHER)
						weap_str = tp_name_rl.string;
                    
                    if (ti_cl->items & IT_ARMOR3) {
                        aclr = "&cf00";
                    }
                    else if (ti_cl->items & IT_ARMOR2) {
                        aclr = "&cff0";
                    }
                    else if (ti_cl->items & IT_ARMOR1) {
                        aclr = "&c0f0";
                    }

			        snprintf(armor_health_str, sizeof(armor_health_str), "%s%d&cfff/%s%s%d", aclr, ti_cl->armor, "&c", (ti_cl->health < 25 ? "f00" : (ti_cl->health > 100 ? "9cf" : "fff")), ti_cl->health);
					// only draw if overlay is enabled, causes corrupted text otherwise
					if (scr_autoid_ingame_armor_health.integer) {
						Draw_SString(x - Draw_StringLengthColors(armor_health_str, -1, 0.5 * scale, proportional), y - AUTOID_HEALTHBAR_OFFSET_Y * scale, armor_health_str, scale, proportional);
					}
                }

				Util_SkipChars(weap_str, "{}", weap_white_stripped, 32); // hide curly brackets

				snprintf(name, sizeof(name), "%s", TP_ParseFunChars(ti_cl->nick[0] ? ti_cl->nick : cl.players[k].shortname, false));
				if (scr_autoid_ingame_namelength.integer >= 1 && scr_autoid_ingame_namelength.integer < MAX_SCOREBOARDNAME) {
					name[scr_autoid_ingame_namelength.integer] = 0;
				}
				snprintf(tmp, sizeof(tmp), "%s %s", Player_StripNameColor(name), scr_autoid_ingame_weapon.integer ? weap_white_stripped : "");
                Draw_SString(x - Draw_StringLengthColors(tmp, -1, 0.5 * scale, proportional), y - yoffset * scale, tmp, scale, proportional);
			}
			else {	// demos, qtv, spectating
				if (scr_autoid_namelength.integer >= 1 && scr_autoid_namelength.integer < MAX_SCOREBOARDNAME) {
					strlcpy(name, autoids[i].player->name, sizeof(name));
					name[scr_autoid_namelength.integer] = 0;
					Draw_SString(x - Draw_StringLength(name, -1, 0.5 * scale, proportional), y - yoffset * scale, name, scale, proportional);
				}
				else {
					Draw_SString(x - Draw_StringLength(autoids[i].player->name, -1, 0.5 * scale, proportional), y - yoffset * scale, autoids[i].player->name, scale, proportional);
				}
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
		Cvar_Register(&scr_autoid_ingame);
		Cvar_Register(&scr_autoid_ingame_namelength);
		Cvar_Register(&scr_autoid_ingame_weapon);
		Cvar_Register(&scr_autoid_ingame_armor_health);
		Cvar_Register(&scr_autoid_yoffset);
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
