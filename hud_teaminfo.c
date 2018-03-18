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

// hud_teaminfo.c

#include "quakedef.h"
#include "hud.h"
#include "teamplay.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "utils.h"
// Get rid of these once we remove matrix scaling from cl_screen.c version...
#include "gl_model.h"
#include "gl_local.h"

static void Update_TeamInfo(void);

cvar_t  scr_teaminfo_order           = { "scr_teaminfo_order", "%p%n $x10%l$x11 %a/%H %w", CVAR_NONE };
cvar_t	scr_teaminfo_align_right     = { "scr_teaminfo_align_right", "1" };
cvar_t	scr_teaminfo_frame_color     = { "scr_teaminfo_frame_color", "10 0 0 120", CVAR_COLOR };
cvar_t	scr_teaminfo_scale           = { "scr_teaminfo_scale",       "1" };
cvar_t	scr_teaminfo_y               = { "scr_teaminfo_y",           "0" };
cvar_t  scr_teaminfo_x               = { "scr_teaminfo_x",           "0" };
cvar_t  scr_teaminfo_loc_width       = { "scr_teaminfo_loc_width",   "5" };
cvar_t  scr_teaminfo_name_width      = { "scr_teaminfo_name_width",  "6" };
cvar_t	scr_teaminfo_low_health      = { "scr_teaminfo_low_health",  "25" };
cvar_t	scr_teaminfo_armor_style     = { "scr_teaminfo_armor_style", "3" };
cvar_t	scr_teaminfo_powerup_style   = { "scr_teaminfo_powerup_style", "1" };
cvar_t	scr_teaminfo_weapon_style    = { "scr_teaminfo_weapon_style","1" };
cvar_t  scr_teaminfo_show_enemies    = { "scr_teaminfo_show_enemies","0" };
cvar_t  scr_teaminfo_show_self       = { "scr_teaminfo_show_self",   "2" };
cvar_t  scr_teaminfo                 = { "scr_teaminfo",             "1" };

static int SCR_HudDrawTeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, float scale, const char* layout, int weapon_style, int armor_style, int powerup_style, int low_health);

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
		*hud_teaminfo_scale,
		*hud_teaminfo_armor_style,
		*hud_teaminfo_powerup_style,
		*hud_teaminfo_low_health,
		*hud_teaminfo_layout;

	if (hud_teaminfo_weapon_style == NULL) {
		// first time
		hud_teaminfo_weapon_style = HUD_FindVar(hud, "weapon_style");
		hud_teaminfo_align_right = HUD_FindVar(hud, "align_right");
		hud_teaminfo_loc_width = HUD_FindVar(hud, "loc_width");
		hud_teaminfo_name_width = HUD_FindVar(hud, "name_width");
		hud_teaminfo_show_enemies = HUD_FindVar(hud, "show_enemies");
		hud_teaminfo_show_self = HUD_FindVar(hud, "show_self");
		hud_teaminfo_scale = HUD_FindVar(hud, "scale");
		hud_teaminfo_armor_style = HUD_FindVar(hud, "armor_style");
		hud_teaminfo_powerup_style = HUD_FindVar(hud, "powerup_style");
		hud_teaminfo_low_health = HUD_FindVar(hud, "low_health");
		hud_teaminfo_layout = HUD_FindVar(hud, "layout");
	}

	// Don't update hud item unless first view is beeing displayed
	if (CL_MultiviewCurrentView() != 1 && CL_MultiviewCurrentView() != 0)
		return;

	if (cls.mvdplayback) {
		Update_TeamInfo();
	}

	// fill data we require to draw teaminfo
	for (maxloc = maxname = slots_num = i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator
			|| !ti_clients[i].time || ti_clients[i].time + 5 < r_refdef2.time
			)
			continue;

		// do not show enemy players unless it's MVD and user wishes to show them
		if (VX_TrackerIsEnemy(i) && (!cls.mvdplayback || !hud_teaminfo_show_enemies->integer))
			continue;

		// do not show tracked player to spectator
		if ((cl.spectator && Cam_TrackNum() == i) && hud_teaminfo_show_self->integer == 0)
			continue;

		// dynamically guess max length of name/location
		nick = (ti_clients[i].nick[0] ? ti_clients[i].nick : cl.players[i].name); // we use nick or name
		maxname = max(maxname, strlen(TP_ParseFunChars(nick, false)));

		strlcpy(tmp, TP_LocationName(ti_clients[i].org), sizeof(tmp));
		maxloc = max(maxloc, strlen(TP_ParseFunChars(tmp, false)));

		slots[slots_num++] = i;
	}

	// well, better use fixed loc length
	maxloc = bound(0, hud_teaminfo_loc_width->integer, 100);
	// limit name length
	maxname = bound(0, maxname, hud_teaminfo_name_width->integer);

	// this does't draw anything, just calculate width
	width = FONTWIDTH * hud_teaminfo_scale->value * SCR_HudDrawTeamInfoPlayer(&ti_clients[0], 0, 0, maxname, maxloc, true, hud_teaminfo_scale->value, hud_teaminfo_layout->string, hud_teaminfo_weapon_style->integer, hud_teaminfo_armor_style->integer, hud_teaminfo_powerup_style->integer, hud_teaminfo_low_health->integer);
	height = FONTWIDTH * hud_teaminfo_scale->value * (hud_teaminfo_show_enemies->integer ? slots_num + n_teams : slots_num);

	if (hud_editor) {
		HUD_PrepareDraw(hud, width, FONTWIDTH, &x, &y);
	}

	if (!slots_num) {
		return;
	}

	if (!cl.teamplay) {
		// non teamplay mode
		return;
	}

	if (!HUD_PrepareDraw(hud, width, height, &x, &y))
		return;

	_y = y;
	x = (hud_teaminfo_align_right->value ? x - (width * (FONTWIDTH * hud_teaminfo_scale->value)) : x);

	// If multiple teams are displayed then sort the display and print team header on overlay
	k = 0;
	if (hud_teaminfo_show_enemies->integer) {
		while (sorted_teams[k].name) {
			Draw_SString(x, _y, sorted_teams[k].name, hud_teaminfo_scale->value);
			sprintf(tmp, "%s %i", TP_ParseFunChars("$.", false), sorted_teams[k].frags);
			Draw_SString(x + (strlen(sorted_teams[k].name) + 1)*FONTWIDTH, _y, tmp, hud_teaminfo_scale->value);
			_y += FONTWIDTH * hud_teaminfo_scale->value;
			for (j = 0; j < slots_num; j++) {
				i = slots[j];
				if (!strcmp(cl.players[i].team, sorted_teams[k].name)) {
					SCR_HudDrawTeamInfoPlayer(&ti_clients[i], x, _y, maxname, maxloc, false, hud_teaminfo_scale->value, hud_teaminfo_layout->string, hud_teaminfo_weapon_style->integer, hud_teaminfo_armor_style->integer, hud_teaminfo_powerup_style->integer, hud_teaminfo_low_health->integer);
					_y += FONTWIDTH * hud_teaminfo_scale->value;
				}
			}
			k++;
		}
	}
	else {
		for (j = 0; j < slots_num; j++) {
			i = slots[j];
			SCR_HudDrawTeamInfoPlayer(&ti_clients[i], x, _y, maxname, maxloc, false, hud_teaminfo_scale->value, hud_teaminfo_layout->string, hud_teaminfo_weapon_style->integer, hud_teaminfo_armor_style->integer, hud_teaminfo_powerup_style->integer, hud_teaminfo_low_health->integer);
			_y += FONTWIDTH * hud_teaminfo_scale->value;
		}
	}
}

qbool Has_Both_RL_and_LG(int flags)
{
	return (flags & IT_ROCKET_LAUNCHER) && (flags & IT_LIGHTNING);
}

static int SCR_HudDrawTeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, float scale, const char* layout, int weapon_style, int armor_style, int powerup_style, int low_health)
{
	extern mpic_t * SCR_GetWeaponIconByFlag(int flag);
	extern cvar_t tp_name_rlg;

	char *s, *loc, tmp[1024], tmp2[MAX_MACRO_STRING], *aclr;
	int x_in = x; // save x
	int i;
	mpic_t *pic;

	extern mpic_t  *sb_face_invis, *sb_face_quad, *sb_face_invuln;
	extern mpic_t  *sb_armor[3];
	extern mpic_t *sb_items[32];

	if (!ti_cl)
		return 0;

	i = ti_cl->client;

	if (i < 0 || i >= MAX_CLIENTS) {
		Com_DPrintf("SCR_Draw_TeamInfoPlayer: wrong client %d\n", i);
		return 0;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, layout, sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));
	s = tmp2;

	//
	// parse/draw string like this "%n %h:%a %l %p %w"
	//

	for (; *s; s++) {
		switch ((int)s[0]) {
			case '%':

				s++; // advance

				switch ((int)s[0]) {
					case 'n': // draw name

						if (!width_only) {
							char *nick = TP_ParseFunChars(ti_cl->nick[0] ? ti_cl->nick : cl.players[i].name, false);
							str_align_right(tmp, sizeof(tmp), nick, maxname);
							Draw_SString(x, y, tmp, scale);
						}
						x += maxname * FONTWIDTH * scale;

						break;
					case 'w': // draw "best" weapon icon/name
						switch (weapon_style) {
							case 1:
								if (!width_only) {
									if (Has_Both_RL_and_LG(ti_cl->items)) {
										char *weap_str = tp_name_rlg.string;
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_SColoredStringBasic(x, y, weap_white_stripped, false, scale);
									}
									else {
										char *weap_str = TP_ItemName(BestWeaponFromStatItems(ti_cl->items));
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_SColoredStringBasic(x, y, weap_white_stripped, false, scale);
									}
								}
								x += 3 * FONTWIDTH * scale;

								break;
							default: // draw image by default
								if (!width_only)
									if ((pic = SCR_GetWeaponIconByFlag(BestWeaponFromStatItems(ti_cl->items))))
										Draw_SPic(x, y, pic, 0.5 * scale);
								x += 2 * FONTWIDTH * scale;

								break;
						}

						break;
					case 'h': // draw health, padding with space on left side
					case 'H': // draw health, padding with space on right side

						if (!width_only) {
							snprintf(tmp, sizeof(tmp), (s[0] == 'h' ? "%s%3d" : "%s%-3d"), (ti_cl->health < low_health ? "&cf00" : ""), ti_cl->health);
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
						switch (armor_style) {
							case 1: // image prefixed armor value
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_SPic(x, y, sb_armor[2], 1.0 / 3 * scale);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_SPic(x, y, sb_armor[1], 1.0 / 3 * scale);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_SPic(x, y, sb_armor[0], 1.0 / 3 * scale);
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
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										aclr = "&cf00";
									else if (ti_cl->items & IT_ARMOR2)
										aclr = "&cff0";
									else if (ti_cl->items & IT_ARMOR1)
										aclr = "&c0f0";
								}

								break;
							case 4: // armor value prefixed with letter
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_SString(x, y, "r", scale);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_SString(x, y, "y", scale);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_SString(x, y, "g", scale);
								}
								x += FONTWIDTH * scale;

								break;
						}

						if (!width_only) { // value drawed no matter which style
							snprintf(tmp, sizeof(tmp), (s[0] == 'a' ? "%s%3d" : "%s%-3d"), aclr, ti_cl->armor);
							Draw_SString(x, y, tmp, scale);
						}
						x += 3 * FONTWIDTH * scale;

						break;
					case 'l': // draw location

						if (!width_only) {
							loc = TP_LocationName(ti_cl->org);
							if (!loc[0])
								loc = "unknown";

							str_align_right(tmp, sizeof(tmp), TP_ParseFunChars(loc, false), maxloc);
							Draw_SString(x, y, tmp, scale);
						}
						x += maxloc * FONTWIDTH * scale;

						break;
					case 'p': // draw powerups
						switch (powerup_style) {
							case 1: // quad/pent/ring image
								if (!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_SPic(x, y, sb_items[5], 1.0 / 2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_SPic(x, y, sb_items[3], 1.0 / 2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_SPic(x, y, sb_items[2], 1.0 / 2);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;

							case 2: // player powerup face
								if (!width_only) {
									if (sb_face_quad && (ti_cl->items & IT_QUAD))
										Draw_SPic(x, y, sb_face_quad, 1.0 / 3);
									x += FONTWIDTH;
									if (sb_face_invuln && (ti_cl->items & IT_INVULNERABILITY))
										Draw_SPic(x, y, sb_face_invuln, 1.0 / 3);
									x += FONTWIDTH;
									if (sb_face_invis && (ti_cl->items & IT_INVISIBILITY))
										Draw_SPic(x, y, sb_face_invis, 1.0 / 3);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;

							case 3: // colored font (QPR)
								if (!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_ColoredString(x, y, "&c03fQ", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_ColoredString(x, y, "&cf00P", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_ColoredString(x, y, "&cff0R", false);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;
						}
						break;

					case 't':
						if (!width_only) {
							sprintf(tmp, "%i", Player_GetTrackId(cl.players[ti_cl->client].userid));
							Draw_SString(x, y, tmp, scale);
						}
						x += FONTWIDTH * scale; // will break if tracknumber is double digits
						break;

					case '%': // wow, %% result in one %, how smart

						if (!width_only)
							Draw_SString(x, y, "%", scale);
						x += FONTWIDTH * scale;

						break;

					default: // print %x - that mean sequence unknown

						if (!width_only) {
							snprintf(tmp, sizeof(tmp), "%%%c", s[0]);
							Draw_SString(x, y, tmp, scale);
						}
						x += (s[0] ? 2 : 1) * FONTWIDTH * scale;

						break;
				}

				break;

			default: // print x
				if (!width_only) {
					snprintf(tmp, sizeof(tmp), "%c", s[0]);
					if (s[0] != ' ') // inhuman smart optimization, do not print space!
						Draw_SString(x, y, tmp, scale);
				}
				x += FONTWIDTH * scale;

				break;
		}
	}

	return (x - x_in) / (FONTWIDTH * scale); // return width
}


// ORIGINAL teaminfo (cl_screen.c)

// scr_teaminfo 
// Variable ti_clients and related functions also used by hud_teaminfo in hud_common.c
ti_player_t ti_clients[MAX_CLIENTS];

mpic_t * SCR_GetWeaponIconByFlag(int flag);

void SCR_ClearTeamInfo(void)
{
	memset(ti_clients, 0, sizeof(ti_clients));
}

int SCR_Draw_TeamInfoPlayer(ti_player_t *ti_cl, int x, int y, int maxname, int maxloc, qbool width_only, const char* order_string)
{
	char *s, *loc, tmp[1024], tmp2[MAX_MACRO_STRING], *aclr;
	int x_in = x; // save x
	int i;
	mpic_t *pic;

	extern mpic_t  *sb_face_invis, *sb_face_quad, *sb_face_invuln;
	extern mpic_t  *sb_armor[3];
	extern mpic_t  *sb_items[32];
	extern cvar_t tp_name_rlg;

	if (!ti_cl)
		return 0;

	i = ti_cl->client;

	if (i < 0 || i >= MAX_CLIENTS) {
		Com_DPrintf("SCR_Draw_TeamInfoPlayer: wrong client %d\n", i);
		return 0;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, order_string, sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));
	s = tmp2;

	//
	// parse/draw string like this "%n %h:%a %l %p %w"
	//

	for (; *s; s++) {
		switch ((int)s[0]) {
			case '%':

				s++; // advance

				switch ((int)s[0]) {
					case 'n': // draw name
						if (!width_only) {
							char *nick = TP_ParseFunChars(ti_cl->nick[0] ? ti_cl->nick : cl.players[i].name, false);
							str_align_right(tmp, sizeof(tmp), nick, maxname);
							Draw_ColoredString(x, y, tmp, false);
						}
						x += maxname * FONTWIDTH;
						break;

					case 'w': // draw "best" weapon icon/name
						switch (scr_teaminfo_weapon_style.integer) {
							case 1:
								if (!width_only) {
									if (Has_Both_RL_and_LG(ti_cl->items)) {
										char *weap_str = tp_name_rlg.string;
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_ColoredString(x, y, weap_white_stripped, false);
									}
									else {
										char *weap_str = TP_ItemName(BestWeaponFromStatItems(ti_cl->items));
										char weap_white_stripped[32];
										Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
										Draw_ColoredString(x, y, weap_white_stripped, false);
									}
								}
								x += 3 * FONTWIDTH;
								break;
							default: // draw image by default
								if (!width_only)
									if ((pic = SCR_GetWeaponIconByFlag(BestWeaponFromStatItems(ti_cl->items))))
										Draw_SPic(x, y, pic, 0.5);
								x += 2 * FONTWIDTH;

								break;
						}
						break;

					case 'h': // draw health, padding with space on left side
					case 'H': // draw health, padding with space on right side

						if (!width_only) {
							snprintf(tmp, sizeof(tmp), (s[0] == 'h' ? "%s%3d" : "%s%-3d"), (ti_cl->health < scr_teaminfo_low_health.integer ? "&cf00" : ""), ti_cl->health);
							Draw_ColoredString(x, y, tmp, false);
						}
						x += 3 * FONTWIDTH;

						break;
					case 'a': // draw armor, padded with space on left side
					case 'A': // draw armor, padded with space on right side

						aclr = "";

						//
						// different styles of armor
						//
						switch (scr_teaminfo_armor_style.integer) {
							case 1: // image prefixed armor value
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_SPic(x, y, sb_armor[2], 1.0 / 3);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_SPic(x, y, sb_armor[1], 1.0 / 3);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_SPic(x, y, sb_armor[0], 1.0 / 3);
								}
								x += FONTWIDTH;

								break;
							case 2: // colored background of armor value
								if (!width_only) {
									byte col[4] = { 255, 255, 255, 0 };

									if (ti_cl->items & IT_ARMOR3) {
										col[0] = 255; col[1] = 0; col[2] = 0; col[3] = 255;
									}
									else if (ti_cl->items & IT_ARMOR2) {
										col[0] = 255; col[1] = 255; col[2] = 0; col[3] = 255;
									}
									else if (ti_cl->items & IT_ARMOR1) {
										col[0] = 0; col[1] = 255; col[2] = 0; col[3] = 255;
									}

									Draw_AlphaRectangleRGB(x, y, 3 * FONTWIDTH, FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));
								}

								break;
							case 3: // colored armor value
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										aclr = "&cf00";
									else if (ti_cl->items & IT_ARMOR2)
										aclr = "&cff0";
									else if (ti_cl->items & IT_ARMOR1)
										aclr = "&c0f0";
								}

								break;
							case 4: // armor value prefixed with letter
								if (!width_only) {
									if (ti_cl->items & IT_ARMOR3)
										Draw_ColoredString(x, y, "r", false);
									else if (ti_cl->items & IT_ARMOR2)
										Draw_ColoredString(x, y, "y", false);
									else if (ti_cl->items & IT_ARMOR1)
										Draw_ColoredString(x, y, "g", false);
								}
								x += FONTWIDTH;

								break;
						}

						if (!width_only) { // value drawn no matter which style
							snprintf(tmp, sizeof(tmp), (s[0] == 'a' ? "%s%3d" : "%s%-3d"), aclr, ti_cl->armor);
							Draw_ColoredString(x, y, tmp, false);
						}
						x += 3 * FONTWIDTH;

						break;
					case 'l': // draw location

						if (!width_only) {
							loc = TP_LocationName(ti_cl->org);
							if (!loc[0])
								loc = "unknown";

							str_align_right(tmp, sizeof(tmp), TP_ParseFunChars(loc, false), maxloc);
							Draw_ColoredString(x, y, tmp, false);
						}
						x += maxloc * FONTWIDTH;

						break;


					case 'p': // draw powerups	
						switch (scr_teaminfo_powerup_style.integer) {
							case 1: // quad/pent/ring image
								if (!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_SPic(x, y, sb_items[5], 1.0 / 2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_SPic(x, y, sb_items[3], 1.0 / 2);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_SPic(x, y, sb_items[2], 1.0 / 2);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;

							case 2: // player powerup face
								if (!width_only) {
									if (sb_face_quad && (ti_cl->items & IT_QUAD))
										Draw_SPic(x, y, sb_face_quad, 1.0 / 3);
									x += FONTWIDTH;
									if (sb_face_invuln && (ti_cl->items & IT_INVULNERABILITY))
										Draw_SPic(x, y, sb_face_invuln, 1.0 / 3);
									x += FONTWIDTH;
									if (sb_face_invis && (ti_cl->items & IT_INVISIBILITY))
										Draw_SPic(x, y, sb_face_invis, 1.0 / 3);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;

							case 3: // colored font (QPR)
								if (!width_only) {
									if (ti_cl->items & IT_QUAD)
										Draw_ColoredString(x, y, "&c03fQ", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVULNERABILITY)
										Draw_ColoredString(x, y, "&cf00P", false);
									x += FONTWIDTH;
									if (ti_cl->items & IT_INVISIBILITY)
										Draw_ColoredString(x, y, "&cff0R", false);
									x += FONTWIDTH;
								}
								else { x += 3 * FONTWIDTH; }
								break;
						}
						break;

					case '%': // wow, %% result in one %, how smart

						if (!width_only)
							Draw_ColoredString(x, y, "%", false);
						x += FONTWIDTH;

						break;

					default: // print %x - that mean sequence unknown

						if (!width_only) {
							snprintf(tmp, sizeof(tmp), "%%%c", s[0]);
							Draw_ColoredString(x, y, tmp, false);
						}
						x += (s[0] ? 2 : 1) * FONTWIDTH;

						break;
				}

				break;

			default: // print x
				if (!width_only) {
					snprintf(tmp, sizeof(tmp), "%c", s[0]);
					if (s[0] != ' ') // inhuman smart optimization, do not print space!
						Draw_ColoredString(x, y, tmp, false);
				}
				x += FONTWIDTH;

				break;
		}
	}

	return (x - x_in) / FONTWIDTH; // return width
}

void SCR_Draw_TeamInfo(void)
{
	int x, y, w, h;
	int i, j, slots[MAX_CLIENTS], slots_num, maxname, maxloc;
	char tmp[1024], *nick;
	float oldMatrix[16];

	float	scale = bound(0.1, scr_teaminfo_scale.value, 10);

	if (!cl.teamplay || !scr_teaminfo.integer) {
		// non teamplay mode
		return;
	}

	if (cls.mvdplayback)
		Update_TeamInfo();

	// fill data we require to draw teaminfo
	for (maxloc = maxname = slots_num = i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0] || cl.players[i].spectator
			|| !ti_clients[i].time || ti_clients[i].time + TI_TIMEOUT < r_refdef2.time
			)
			continue;

		// do not show enemy players unless it's MVD and user wishes to show them
		if (VX_TrackerIsEnemy(i) && (!cls.mvdplayback || !scr_teaminfo_show_enemies.integer))
			continue;

		// do not show tracked player to spectator
		if ((cl.spectator && Cam_TrackNum() == i) && !TEAMINFO_SHOWSELF())
			continue;

		// dynamically guess max length of name/location
		nick = (ti_clients[i].nick[0] ? ti_clients[i].nick : cl.players[i].name); // we use nick or name
		maxname = max(maxname, strlen(TP_ParseFunChars(nick, false)));

		strlcpy(tmp, TP_LocationName(ti_clients[i].org), sizeof(tmp));
		maxloc = max(maxloc, strlen(TP_ParseFunChars(tmp, false)));

		slots[slots_num++] = i;
	}

	// well, better use fixed loc length
	maxloc = bound(0, scr_teaminfo_loc_width.integer, 100);
	// limit name length
	maxname = bound(0, maxname, scr_teaminfo_name_width.integer);

	if (!slots_num) {
		return;
	}

	if (scale != 1) {
		GL_PushMatrix(GL_PROJECTION, oldMatrix);
		GL_Scale(GL_PROJECTION, scale, scale, 1);
	}

	y = vid.height*0.6 / scale + scr_teaminfo_y.value;

	// this does't draw anything, just calculate width
	w = SCR_Draw_TeamInfoPlayer(&ti_clients[0], 0, 0, maxname, maxloc, true, scr_teaminfo_order.string);
	h = slots_num;

	for (j = 0; j < slots_num; j++) {
		i = slots[j];

		x = (scr_teaminfo_align_right.value ? (vid.width / scale - w * FONTWIDTH) - FONTWIDTH : FONTWIDTH);
		x += scr_teaminfo_x.value;

		if (!j) { // draw frame
			byte	*col = scr_teaminfo_frame_color.color;

			Draw_AlphaRectangleRGB(x, y, w * FONTWIDTH, h * FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));
		}

		SCR_Draw_TeamInfoPlayer(&ti_clients[i], x, y, maxname, maxloc, false, scr_teaminfo_order.string);

		y += FONTWIDTH;
	}
}

void Parse_TeamInfo(char *s)
{
	int		client;

	Cmd_TokenizeString(s);

	client = atoi(Cmd_Argv(1));

	if (client < 0 || client >= MAX_CLIENTS) {
		Com_DPrintf("Parse_TeamInfo: wrong client %d\n", client);
		return;
	}

	ti_clients[client].client = client; // no, its not stupid

	ti_clients[client].time = r_refdef2.time;

	ti_clients[client].org[0] = atoi(Cmd_Argv(2));
	ti_clients[client].org[1] = atoi(Cmd_Argv(3));
	ti_clients[client].org[2] = atoi(Cmd_Argv(4));
	ti_clients[client].health = atoi(Cmd_Argv(5));
	ti_clients[client].armor = atoi(Cmd_Argv(6));
	ti_clients[client].items = atoi(Cmd_Argv(7));
	strlcpy(ti_clients[client].nick, Cmd_Argv(8), TEAMINFO_NICKLEN); // nick is optional
}

static void Update_TeamInfo(void)
{
	int		i;
	int		*st;

	static double lastupdate = 0;

	if (!cls.mvdplayback)
		return;

	// don't update each frame - it's less disturbing
	if (cls.realtime - lastupdate < 1)
		return;

	lastupdate = cls.realtime;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].spectator || !cl.players[i].name[0])
			continue;

		st = cl.players[i].stats;

		ti_clients[i].client = i; // no, its not stupid

		ti_clients[i].time = r_refdef2.time;

		VectorCopy(cl.frames[cl.parsecount & UPDATE_MASK].playerstate[i].origin, ti_clients[i].org);

		ti_clients[i].health = bound(0, st[STAT_HEALTH], 999);
		ti_clients[i].armor = bound(0, st[STAT_ARMOR], 999);
		ti_clients[i].items = st[STAT_ITEMS];
		ti_clients[i].nick[0] = 0; // sad, we don't have nick, will use name
	}
}

void TeamInfo_HudInit(void)
{
	Cvar_Register(&scr_teaminfo_order);
	Cvar_Register(&scr_teaminfo_align_right);
	Cvar_Register(&scr_teaminfo_frame_color);
	Cvar_Register(&scr_teaminfo_scale);
	Cvar_Register(&scr_teaminfo_y);
	Cvar_Register(&scr_teaminfo_x);
	Cvar_Register(&scr_teaminfo_loc_width);
	Cvar_Register(&scr_teaminfo_name_width);
	Cvar_Register(&scr_teaminfo_low_health);
	Cvar_Register(&scr_teaminfo_armor_style);
	Cvar_Register(&scr_teaminfo_powerup_style);
	Cvar_Register(&scr_teaminfo_weapon_style);
	Cvar_Register(&scr_teaminfo_show_enemies);
	Cvar_Register(&scr_teaminfo_show_self);
	Cvar_Register(&scr_teaminfo);

	HUD_Register(
		"teaminfo", NULL, "Show information about your team in short form.",
		0, ca_active, 0, SCR_HUD_DrawTeamInfo,
		"0", "", "right", "center", "0", "0", "0.2", "20 20 20", NULL,
		"layout", "%p%n $x10%l$x11 %a/%H %w",
		"align_right", "0",
		"loc_width", "5",
		"name_width", "6",
		"low_health", "25",
		"armor_style", "3",
		"weapon_style", "0",
		"show_enemies", "0",
		"show_self", "1",
		"scale", "1",
		"powerup_style", "1",
		NULL
	);
}
