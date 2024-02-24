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

#include "quakedef.h"
#include "hud.h"
#include "hud_common.h"
#include "utils.h"
#include "sbar.h"
#include "fonts.h"

#define MAX_FRAGS_NAME 32
#define FRAGS_HEALTH_SPACING 1

#define	TEAMFRAGS_EXTRA_SPEC_NONE	0
#define TEAMFRAGS_EXTRA_SPEC_BEFORE	1
#define	TEAMFRAGS_EXTRA_SPEC_ONTOP	2
#define TEAMFRAGS_EXTRA_SPEC_NOICON 3
#define TEAMFRAGS_EXTRA_SPEC_RLTEXT 4

#define FRAGS_HEALTHBAR_NORMAL_COLOR	75
#define FRAGS_HEALTHBAR_MEGA_COLOR		251
#define	FRAGS_HEALTHBAR_TWO_MEGA_COLOR	238
#define	FRAGS_HEALTHBAR_UNNATURAL_COLOR	144

static qbool hud_frags_extra_spec_info = true;
static qbool hud_frags_show_rl = true;
static qbool hud_frags_show_lg = true;
static qbool hud_frags_show_armor = true;
static qbool hud_frags_show_health = true;
static qbool hud_frags_show_powerup = true;
static qbool hud_frags_textonly = false;
static qbool hud_frags_horiz_health = false;
static qbool hud_frags_horiz_power = false;

static void Frags_DrawHorizontalHealthBar(player_info_t* info, int x, int y, int width, int height, qbool flip, qbool use_power)
{
	static cvar_t* health_color_nohealth = NULL;
	static cvar_t* health_color_normal = NULL;
	static cvar_t* health_color_mega = NULL;
	static cvar_t* health_color_twomega = NULL;
	static cvar_t* health_color_unnatural = NULL;
	static cvar_t* armor_color_noarmor = NULL;
	static cvar_t* armor_color_ga_over = NULL;
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
		armor_color_ga_over = Cvar_Find("hud_bar_armor_color_ga_over");
		armor_color_ga = Cvar_Find("hud_bar_armor_color_ga");
		armor_color_ya = Cvar_Find("hud_bar_armor_color_ya");
		armor_color_ra = Cvar_Find("hud_bar_armor_color_ra");
		armor_color_unnatural = Cvar_Find("hud_bar_armor_color_unnatural");
	}

	bk = (byte*)&health_color_nohealth->color;
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
		if (armor > 100) {
			armor_color = RGBAVECT_TO_COLOR(armor_color_ga_over->color);
		} else {
			armor_color = RGBAVECT_TO_COLOR(armor_color_ga->color);
		}
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

		Draw_AlphaFillRGB(x, y + health_height, width, armor_height, RGBAVECT_TO_COLOR(armor_color_noarmor->color));
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

static void Frags_DrawColors(
	int x, int y, int width, int height,
	int top_color, int bottom_color, float color_alpha,
	int frags, int drawBrackets, int style,
	float bignum, float scale, qbool proportional, 
	int wipeout, int hidefrags, int isdead, int timetospawn
)
{
	char buf[32];
	clrinfo_t color;
	int posy = 0;
	int char_size = (bignum > 0) ? Q_rint(24 * bignum) : 8 * scale;
	qbool use_wipeout = (check_ktx_ca_wo() && wipeout);
	qbool norespawn = (use_wipeout && (isdead == 2));
	float wo_alpha = (use_wipeout && (isdead == 1)) ? 0.5 : 1.0;
	float bignum_alpha = (use_wipeout && isdead) ? 0.25 : 1.0;

	Draw_AlphaFill(x, y, width, height / 2, norespawn ? 3 : top_color, color_alpha * wo_alpha);
	Draw_AlphaFill(x, y + height / 2, width, height - height / 2, norespawn ? 3 : bottom_color, color_alpha * wo_alpha);

	posy = y + (height - char_size) / 2;
	color.i = 0;

	if ((bignum > 0) && !hidefrags) {
		//
		// Scaled big numbers for frags.
		//
		extern  mpic_t *sb_nums[2][11];
		char *t = buf;
		int char_x;
		int char_y;
		snprintf(buf, sizeof(buf), "%d", frags);
		
		char_x = max(x, x + (width - (int)strlen(buf) * char_size) / 2);
		char_y = max(y, posy);

		while (*t) {
			if (*t >= '0' && *t <= '9') {
				Draw_SAlphaPic(char_x, char_y, sb_nums[0][*t - '0'], bignum_alpha, bignum);
				char_x += char_size;
			}
			else if (*t == '-') {
				Draw_SAlphaPic(char_x, char_y, sb_nums[0][STAT_MINUS], bignum_alpha, bignum);
				char_x += char_size;
			}

			t++;
		}
	}
	else {
		if (use_wipeout) {
			if (isdead == 1 && timetospawn > 0 && timetospawn < 999) {
				color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0x00, (byte)(1 * 255));
			} 
			else if (isdead == 2) {
				color.c = RGBA_TO_COLOR(0x33, 0x33, 0x33, (byte)(1 * 255));
			} 
			else {
				color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, (byte)(1 * 255));
			}
		}
		else {
			color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, (byte)(1 * 255));
		}

		// Determine snprintf content
		if (use_wipeout && isdead == 1 && timetospawn > 0 && timetospawn < 999) {
			snprintf(buf, sizeof(buf), "%d", timetospawn);
		} 
		else if (use_wipeout && hidefrags) {
			snprintf(buf, sizeof(buf), "%s", " ");
		} 
		else {
			snprintf(buf, sizeof(buf), "%d", frags);
		}

		// Normal text size. (meag: why -3?)
		Draw_SColoredAlphaString(x + (width - Draw_StringLength(buf, -1, scale, proportional)) / 2, posy, buf, &color, 1, 0, scale, 1, proportional);
	}

	if (drawBrackets && width) {
		// Brackets [] are not available scaled, so use normal size even
		// if we're drawing big frag nums.
		int brack_posy = y + (height - 8 * scale) / 2;
		int d = (width >= 32) ? 0 : 1;

		switch (style) {
			case 1:
				Draw_SCharacterP(x - FontCharacterWidth(13, scale, proportional) * 0.5f, posy, 13, scale, proportional);
				break;
			case 2:
				// Red outline.
				Draw_Fill(x, y - 1, width, 1, 0x4f);
				Draw_Fill(x, y - 1, 1, height + 2, 0x4f);
				Draw_Fill(x + width - 1, y - 1, 1, height + 2, 0x4f);
				Draw_Fill(x, y + height, width, 1, 0x4f);
				break;
			case 3:
				// Draw nothing
				break;
			case 0:
			default:
				Draw_SCharacterP(x - 2 - 2 * d, brack_posy, 16, scale, proportional); // [
				Draw_SCharacterP(x + width - 8 + 1 + d, brack_posy, 17, scale, proportional); // ]
				break;
		}
	}
}

static void Frags_DrawHealthBar(int original_health, int x, int y, int height, int width, qbool horizontal, qbool flip)
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
		if (health > 100 && health <= 200) {
			float health_height = (int)Q_rint((height / 100.0) * (health - 100));
			Draw_Fill(x, y + height - health_height, width, health_height, FRAGS_HEALTHBAR_MEGA_COLOR);
		}
		else if (health > 200 && health <= 250) {
			float health_height = (int)Q_rint((height / 100.0) * (health - 200));
			Draw_Fill(x, y, width, height, FRAGS_HEALTHBAR_MEGA_COLOR);
			Draw_Fill(x, y + height - health_height, width, health_height, FRAGS_HEALTHBAR_TWO_MEGA_COLOR);
		}
		else if (health > 250) {
			// This will never happen during a normal game.
			Draw_Fill(x, y, width, health_height, FRAGS_HEALTHBAR_UNNATURAL_COLOR);
		}
	}
}

static int TeamFrags_DrawExtraSpecInfo(int num, int px, int py, int width, int height, int style)
{
	extern mpic_t *sb_weapons[7][8]; // sbar.c
	mpic_t rl_picture = *sb_weapons[0][5];

	// Only allow this for spectators.
	if (!(cls.demoplayback || cl.spectator)
		|| style > TEAMFRAGS_EXTRA_SPEC_RLTEXT
		|| style <= TEAMFRAGS_EXTRA_SPEC_NONE
		|| !style) {
		return px;
	}

	// Check if the team has any RL's.
	if (sorted_teams[num].rlcount > 0) {
		int y_pos = py;

		//
		// Draw the RL + count depending on style.
		//

		if ((style == TEAMFRAGS_EXTRA_SPEC_BEFORE || style == TEAMFRAGS_EXTRA_SPEC_NOICON)
			&& style != TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px, y_pos, va("%d", sorted_teams[num].rlcount), 0, false);
			px += 8 + 1;
		}

		if (style != TEAMFRAGS_EXTRA_SPEC_NOICON && style != TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
			y_pos = Q_rint(py + (height / 2.0) - (rl_picture.height / 2.0));
			Draw_SSubPic(px, y_pos, &rl_picture, 0, 0, rl_picture.width, rl_picture.height, 1);
			px += rl_picture.width + 1;
		}

		if (style == TEAMFRAGS_EXTRA_SPEC_ONTOP && style != TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px - 14, y_pos, va("%d", sorted_teams[num].rlcount), 0, false);
		}

		if (style == TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
			y_pos = Q_rint(py + (height / 2.0) - 4);
			Draw_ColoredString(px, y_pos, va("&ce00RL&cfff%d", sorted_teams[num].rlcount), 0, false);
			px += 8 * 3 + 1;
		}
	}
	else {
		// If the team has no RL's just pad with nothing.
		if (style == TEAMFRAGS_EXTRA_SPEC_BEFORE) {
			// Draw the rl count before the rl icon.
			px += rl_picture.width + 8 + 1 + 1;
		}
		else if (style == TEAMFRAGS_EXTRA_SPEC_ONTOP) {
			// Draw the rl count on top of the RL instead of infront.
			px += rl_picture.width + 1;
		}
		else if (style == TEAMFRAGS_EXTRA_SPEC_NOICON) {
			// Only draw the rl count.
			px += 8 + 1;
		}
		else if (style == TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
			px += 8 * 3 + 1;
		}
	}

	return px;
}

static void Frags_OnChangeExtraSpecInfo(cvar_t *var, char *s, qbool *cancel)
{
	// Parse the extra spec info.
	hud_frags_show_rl = Utils_RegExpMatch("RL|ALL", s);
	hud_frags_show_lg = Utils_RegExpMatch("LG|ALL", s);
	hud_frags_show_armor = Utils_RegExpMatch("ARMOR|ALL", s);
	hud_frags_show_health = Utils_RegExpMatch("HEALTH|ALL", s);
	hud_frags_show_powerup = Utils_RegExpMatch("POWERUP|ALL", s);
	hud_frags_textonly = Utils_RegExpMatch("TEXT", s);
	hud_frags_horiz_health = Utils_RegExpMatch("HMETER", s);
	hud_frags_horiz_power = Utils_RegExpMatch("PMETER", s);

	hud_frags_extra_spec_info = (hud_frags_show_rl || hud_frags_show_lg || hud_frags_show_armor || hud_frags_show_health || hud_frags_show_powerup);
}

static int Frags_DrawExtraSpecInfo(player_info_t *info,
	int px, int py,
	int cell_width, int cell_height,
	int space_x, int space_y, int flip
)
{
	extern mpic_t *sb_weapons[7][8];		// sbar.c ... Used for displaying the RL.
	mpic_t *rl_picture = sb_weapons[0][5];	// Picture of RL.
	mpic_t *lg_picture = sb_weapons[0][6];	// Picture of LG.

	float	armor_height = 0.0;
	int		armor = 0;
	int		armor_bg_color = 0;
	float	armor_bg_power = 0;
	int		weapon_width = 0;

	// Only allow this for spectators.
	if (!(cls.demoplayback || cl.spectator)) {
		return px;
	}

	// Set width based on text or picture.
	if (hud_frags_show_armor || hud_frags_show_rl || hud_frags_show_lg || hud_frags_show_powerup) {
		if (hud_frags_show_rl && hud_frags_show_lg) {
			weapon_width = (!hud_frags_textonly ? rl_picture->width + lg_picture->width : 24 * 2);
		}
		else if (hud_frags_show_rl) {
			weapon_width = (!hud_frags_textonly ? rl_picture->width : 24);
		}
		else {
			weapon_width = (!hud_frags_textonly ? lg_picture->width : 24);
		}
	}

	// Draw health bar. (flipped)
	if (flip && hud_frags_show_health) {
		Frags_DrawHealthBar(info->stats[STAT_HEALTH], px, py, cell_height, 3, false, false);
		px += 3 + FRAGS_HEALTH_SPACING;
	}

	armor = info->stats[STAT_ARMOR];

	// If the player has any armor, draw it in the appropriate color.
	if (info->stats[STAT_ITEMS] & IT_ARMOR1) {
		armor_bg_power = 100;
		armor_bg_color = 178; // Green armor.
	}
	else if (info->stats[STAT_ITEMS] & IT_ARMOR2) {
		armor_bg_power = 150;
		armor_bg_color = 111; // Yellow armor.
	}
	else if (info->stats[STAT_ITEMS] & IT_ARMOR3) {
		armor_bg_power = 200;
		armor_bg_color = 79; // Red armor.
	}

	// Only draw the armor if the current player has one and if the style allows it.
	if (armor_bg_power && hud_frags_show_armor) {
		armor_height = Q_rint((cell_height / armor_bg_power) * armor);

		Draw_AlphaFill(px,												// x
			py + cell_height - (int)armor_height,			// y (draw from bottom up)
			weapon_width,									// width
			(int)armor_height,								// height
			armor_bg_color,									// color
			0.3);											// alpha
	}

	// Draw the rl if the current player has it and the style allows it.
	if (info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER && hud_frags_show_rl) {
		if (!hud_frags_textonly) {
			// Draw the rl-pic.
			Draw_SSubPic(
				px,
				py + Q_rint((cell_height / 2.0)) - (rl_picture->height / 2.0),
				rl_picture, 0, 0,
				rl_picture->width,
				rl_picture->height, 1
			);
		}
		else {
			// Just print "RL" instead.
			Draw_String(px + 12 - 8, py + Q_rint((cell_height / 2.0)) - 4, "RL");
		}
	}

	// Draw the lg if the current player has it and the style allows it.
	if ((info->stats[STAT_ITEMS] & IT_LIGHTNING) && hud_frags_show_lg) {
		if (!hud_frags_textonly) {
			// Draw the lg-pic.
			Draw_SSubPic(
				hud_frags_show_rl ? px + rl_picture->width + 24 : px,
				py + Q_rint((cell_height / 2.0)) - (lg_picture->height / 2.0),
				lg_picture, 0, 0,
				lg_picture->width,
				lg_picture->height, 1
			);
		}
		else {
			// Just print "LG" instead.
			Draw_String(hud_frags_show_rl ? px + 8 + 24 : px + 12 - 8, py + Q_rint((cell_height / 2.0)) - 4, "LG");
		}
	}

	// Only draw powerups if the current player has one and the style allows it.
	if (hud_frags_show_powerup) {
		//float powerups_x = px + (spec_extra_weapon_w / 2.0);
		float powerups_x = px + (weapon_width / 2.0);

		if (info->stats[STAT_ITEMS] & IT_INVULNERABILITY
			&& info->stats[STAT_ITEMS] & IT_INVISIBILITY
			&& info->stats[STAT_ITEMS] & IT_QUAD) {
			Draw_ColoredString(Q_rint(powerups_x - 10), py, "&c0ffQ&cf00P&cff0R", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_QUAD
			&& info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&c0ffQ&cf00P", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_QUAD
			&& info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&c0ffQ&cff0R", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_INVULNERABILITY
			&& info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
			Draw_ColoredString(Q_rint(powerups_x - 8), py, "&cf00P&cff0R", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_QUAD) {
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&c0ffQ", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&cf00P", 0, false);
		}
		else if (info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
			Draw_ColoredString(Q_rint(powerups_x - 4), py, "&cff0R", 0, false);
		}
	}

	px += weapon_width + FRAGS_HEALTH_SPACING;

	// Draw health bar. (not flipped)
	if (!flip && hud_frags_show_health) {
		Frags_DrawHealthBar(info->stats[STAT_HEALTH], px, py, cell_height, 3, false, false);
		px += 3 + FRAGS_HEALTH_SPACING;
	}

	return px;
}

static void Frags_DrawBackground(
	int px, int py, int cell_width, int cell_height,
	int space_x, int space_y, int max_name_length, int max_team_length,
	int bg_color, int shownames, int showteams, int drawBrackets, int style
)
{
	int bg_width = cell_width + space_x;
	//int bg_color = Sbar_BottomColor(info);
	float bg_alpha = 0.3;

	if (style == 4
		|| style == 6
		|| style == 8)
		bg_alpha = 0;

	if (shownames)
		bg_width += max_name_length * 8 + space_x;

	if (showteams)
		bg_width += max_team_length * 8 + space_x;

	if (drawBrackets)
		bg_alpha = 0.7;

	if (style == 7 || style == 8)
		bg_color = 0x4f;

	Draw_AlphaFill(px - 1, py - space_y / 2, bg_width, cell_height + space_y, bg_color, bg_alpha);

	if (drawBrackets && (style == 5 || style == 6)) {
		Draw_Fill(px - 1, py - 1 - space_y / 2, bg_width, 1, 0x4f);

		Draw_Fill(px - 1, py - space_y / 2, 1, cell_height + space_y, 0x4f);
		Draw_Fill(px + bg_width - 1, py - 1 - space_y / 2, 1, cell_height + 1 + space_y, 0x4f);

		Draw_Fill(px - 1, py + cell_height + space_y / 2, bg_width + 1, 1, 0x4f);
	}
}

static int Frags_DrawText(
	int px, int py,
	int cell_width, int cell_height,
	int space_x, int space_y,
	int max_name_length, int max_team_length,
	int flip, int pad,
	int shownames, int showteams,
	char* name, char* team, float scale, qbool proportional,
	int wipeout, int isdead, int timetospawn
)
{
	char _name[MAX_FRAGS_NAME + 1];
	char _team[MAX_FRAGS_NAME + 1];
	float team_length = 0;
	int name_length = 0;
	float char_size = 8 * scale;
	int y_pos;
	clrinfo_t color;
	qbool use_wipeout = (check_ktx_ca_wo() && wipeout);

	color.i = 0;
	y_pos = Q_rint(py + (cell_height / 2.0) - 4 * scale);

	if (use_wipeout && (isdead == 1) && (timetospawn > 0) && (timetospawn < 999)){
		color.c = RGBA_TO_COLOR(0x55, 0x55, 0x55, (byte)(1 * 255));
	}
	else if (use_wipeout && (isdead == 2)){
		color.c = RGBA_TO_COLOR(0x33, 0x33, 0x33, (byte)(1 * 255));
	}
	else {
		color.c = RGBA_TO_COLOR(0xFF, 0xFF, 0xFF, (byte)(1 * 255));
	}

	// Draw team
	if (showteams && cl.teamplay) {
		strlcpy(_team, team, clamp(max_team_length, 0, sizeof(_team)));
		team_length = Draw_StringLength(_team, -1, scale, proportional);

		if (!flip) {
			px += space_x;
		}

		if (pad && flip) {
			Draw_SColoredAlphaString(px + max_team_length * char_size - team_length, y_pos, _team, &color, 1, 0, scale, 1, proportional);
			px += max_team_length * char_size;
		}
		else if (pad) {
			Draw_SColoredAlphaString(px, y_pos, _team, &color, 1, 0, scale, 1, proportional);
			px += max_team_length * char_size;
		}
		else {
			px += Draw_SColoredAlphaString(px, y_pos, _team, &color, 1, 0, scale, 1, proportional);
		}

		if (flip) {
			px += space_x;
		}
	}

	if (shownames) {
		// Draw name
		strlcpy(_name, name, clamp(max_name_length, 0, sizeof(_name)));
		name_length = Draw_StringLength(_name, -1, scale, proportional);

		if (flip && pad) {
			Draw_SColoredAlphaString(px + max_name_length * char_size - name_length, y_pos, _name, &color, 1, 0, scale, 1, proportional);
			px += max_name_length * char_size;
		}
		else if (pad) {
			Draw_SColoredAlphaString(px, y_pos, _name, &color, 1, 0, scale, 1, proportional);
			px += max_name_length * char_size;
		}
		else {
			px += Draw_SColoredAlphaString(px, y_pos, _name, &color, 1, 0, scale, 1, proportional);
		}

		px += space_x;
	}

	return px;
}

static void SCR_HUD_DrawFrags(hud_t *hud)
{
	int width = 0, height = 0;
	int x, y;
	int max_team_length = 0;
	int max_name_length = 0;

	int rows, cols, cell_width, cell_height, space_x, space_y;
	int a_rows, a_cols; // actual
	qbool any_labels_or_info;
	qbool two_lines;

	extern ti_player_t ti_clients[MAX_CLIENTS];

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
		*hud_frags_hidefrags,
		*hud_frags_wipeout,
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
		*hud_frags_scale,
		*hud_frags_proportional;

	extern mpic_t *sb_weapons[7][8]; // sbar.c ... Used for displaying the RL.
	mpic_t *rl_picture;				 // Picture of RL.
	rl_picture = sb_weapons[0][5];

	if (hud_frags_cell_width == NULL) {   // first time
		char specval[256];

		hud_frags_cell_width = HUD_FindVar(hud, "cell_width");
		hud_frags_cell_height = HUD_FindVar(hud, "cell_height");
		hud_frags_rows = HUD_FindVar(hud, "rows");
		hud_frags_cols = HUD_FindVar(hud, "cols");
		hud_frags_space_x = HUD_FindVar(hud, "space_x");
		hud_frags_space_y = HUD_FindVar(hud, "space_y");
		hud_frags_strip = HUD_FindVar(hud, "strip");
		hud_frags_vertical = HUD_FindVar(hud, "vertical");
		hud_frags_shownames = HUD_FindVar(hud, "shownames");
		hud_frags_hidefrags = HUD_FindVar(hud, "hidefrags");
		hud_frags_wipeout = HUD_FindVar(hud, "wipeout");
		hud_frags_teams = HUD_FindVar(hud, "showteams");
		hud_frags_padtext = HUD_FindVar(hud, "padtext");
		hud_frags_extra_spec = HUD_FindVar(hud, "extra_spec_info");
		hud_frags_fliptext = HUD_FindVar(hud, "fliptext");
		hud_frags_style = HUD_FindVar(hud, "style");
		hud_frags_bignum = HUD_FindVar(hud, "bignum");
		hud_frags_colors_alpha = HUD_FindVar(hud, "colors_alpha");
		hud_frags_maxname = HUD_FindVar(hud, "maxname");
		hud_frags_notintp = HUD_FindVar(hud, "notintp");
		hud_frags_fixedwidth = HUD_FindVar(hud, "fixedwidth");
		hud_frags_scale = HUD_FindVar(hud, "scale");
		hud_frags_proportional = HUD_FindVar(hud, "proportional");

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
		if (hud_frags_vertical->value == 1) {
			a_cols = min((n_players + rows - 1) / rows, cols);
			a_rows = min(rows, n_players);
		}
		else if (hud_frags_vertical->value == 2) {
			a_cols = min(n_teams, cols);
			a_rows = min(rows, ((n_players % a_cols) ? n_players/a_cols + 1 : n_players/a_cols));
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
			for (n = 0; n < n_players; n++) {
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
			width += (a_cols * FontFixedWidth(max_name_length, hud_frags_scale->value, false, hud_frags_proportional->integer)) + ((a_cols + 1) * space_x);
		}

		if (cl.teamplay && hud_frags_teams->value) {
			width += (a_cols * FontFixedWidth(max_team_length, hud_frags_scale->value, false, hud_frags_proportional->integer)) + ((a_cols + 1) * space_x);
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

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
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
		for (i = 0; i < limit; i++) {
			player_info_t *info;
			ti_player_t *ti_cl;

			// Always include the current player's position
			if (active_player_position >= 0 && i == limit - 1 && num < active_player_position) {
				num = active_player_position;
			}

			info = &cl.players[sorted_players[num].playernum]; // FIXME! johnnycz; causes crashed on some demos
			ti_cl = &ti_clients[sorted_players[num].playernum];

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
			else if (i % a_cols == 0) {
				// Drawing new row.
				player_x = x + space_x;
				player_y = y + space_y + (i / a_cols) * (cell_height * (two_lines ? 2 : 1) + space_y);
			}

			drawBrackets = 0;

			// Bug fix. Before the wrong player would be higlighted
			// during qwd-playback, since you ARE the player that you're
			// being spectated (you're not a spectator).
			if (cls.demoplayback && !cl.spectator && !cls.mvdplayback) {
				drawBrackets = (sorted_players[num].playernum == cl.playernum);
			}
			else if (cls.demoplayback || cl.spectator) {
				drawBrackets = (cl.spec_track == sorted_players[num].playernum && Cam_TrackNum() >= 0);
			}
			else {
				drawBrackets = (sorted_players[num].playernum == cl.playernum);
			}

			// Don't draw any brackets in multiview since we're tracking several players.
			if (CL_MultiviewEnabled() && !CL_MultiviewInsetEnabled()) {
				drawBrackets = 0;
			}

			if (any_labels_or_info) {
				// Relative x coordinate where we draw the subitems.
				int rel_player_x = player_x;
				qbool odd_column = (hud_frags_vertical->value ? (i / a_rows) : (i % a_cols));
				qbool fliptext = hud_frags_fliptext->integer == 1;

				fliptext |= hud_frags_fliptext->integer == 2 && !odd_column;
				fliptext |= hud_frags_fliptext->integer == 3 && odd_column;

				if (hud_frags_style->value >= 4 && hud_frags_style->value <= 8) {
					// Draw background based on the style.
					Frags_DrawBackground(
						player_x, player_y, cell_width, cell_height, space_x, space_y,
						max_name_length, max_team_length, Sbar_BottomColor(info),
						hud_frags_shownames->value, hud_frags_teams->value, drawBrackets,
						hud_frags_style->value
					);
				}

				if (fliptext) {
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
						info->name, info->team, hud_frags_scale->value, hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						ti_cl->isdead,
						ti_cl->timetospawn
					);

					// Draw team.
					rel_player_x = Frags_DrawText(
						rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						0, hud_frags_teams->value,
						info->name, info->team, hud_frags_scale->value, hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						ti_cl->isdead,
						ti_cl->timetospawn
					);

					Frags_DrawColors(
						rel_player_x, player_y, cell_width, cell_height * (two_lines ? 2 : 1),
						Sbar_TopColor(info), Sbar_BottomColor(info), hud_frags_colors_alpha->value,
						info->frags,
						drawBrackets,
						hud_frags_style->value,
						hud_frags_bignum->value,
						hud_frags_scale->value,
						hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						hud_frags_hidefrags->value,
						ti_cl->isdead,
						ti_cl->timetospawn
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
				else {
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
						hud_frags_scale->value,
						hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						hud_frags_hidefrags->value,
						ti_cl->isdead,
						ti_cl->timetospawn
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
						info->name, info->team, hud_frags_scale->value, hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						ti_cl->isdead,
						ti_cl->timetospawn
					);

					// Draw name.
					rel_player_x = Frags_DrawText(
						rel_player_x, player_y + (two_lines ? cell_height : 0), cell_width, cell_height,
						space_x, space_y, max_name_length, max_team_length,
						fliptext, hud_frags_padtext->value,
						hud_frags_shownames->value, 0,
						info->name, info->team, hud_frags_scale->value, hud_frags_proportional->integer,
						hud_frags_wipeout->value,
						ti_cl->isdead,
						ti_cl->timetospawn
					);
				}

				if (hud_frags_vertical->value) {
					// Next row.
					player_y += cell_height * (two_lines ? 2 : 1) + space_y;
				}
				else {
					// Next column.
					player_x = rel_player_x + space_x;
				}
			}
			else {
				// Only showing the frags, no names or extra spec info.

				Frags_DrawColors(
					player_x, player_y, cell_width, cell_height,
					Sbar_TopColor(info), Sbar_BottomColor(info), hud_frags_colors_alpha->value,
					info->frags,
					drawBrackets,
					hud_frags_style->value,
					hud_frags_bignum->value,
					hud_frags_scale->value,
					hud_frags_proportional->integer,
					hud_frags_wipeout->value,
					hud_frags_hidefrags->value,
					ti_cl->isdead,
					ti_cl->timetospawn
				);

				if (hud_frags_vertical->value) {
					// Next row.
					player_y += cell_height + space_y;
				}
				else {
					// Next column.
					player_x += cell_width + space_x;
				}
			}

			// Next player.
			num++;
		}
	}
}

static void SCR_HUD_DrawTeamFrags(hud_t *hud)
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
		*hud_teamfrags_scale,
		*hud_teamfrags_proportional;

	extern mpic_t *sb_weapons[7][8]; // sbar.c
	mpic_t rl_picture = *sb_weapons[0][5];

	if (hud_teamfrags_cell_width == 0)    // first time
	{
		hud_teamfrags_cell_width = HUD_FindVar(hud, "cell_width");
		hud_teamfrags_cell_height = HUD_FindVar(hud, "cell_height");
		hud_teamfrags_rows = HUD_FindVar(hud, "rows");
		hud_teamfrags_cols = HUD_FindVar(hud, "cols");
		hud_teamfrags_space_x = HUD_FindVar(hud, "space_x");
		hud_teamfrags_space_y = HUD_FindVar(hud, "space_y");
		hud_teamfrags_strip = HUD_FindVar(hud, "strip");
		hud_teamfrags_vertical = HUD_FindVar(hud, "vertical");
		hud_teamfrags_shownames = HUD_FindVar(hud, "shownames");
		hud_teamfrags_fliptext = HUD_FindVar(hud, "fliptext");
		hud_teamfrags_padtext = HUD_FindVar(hud, "padtext");
		hud_teamfrags_style = HUD_FindVar(hud, "style");
		hud_teamfrags_extra_spec = HUD_FindVar(hud, "extra_spec_info");
		hud_teamfrags_onlytp = HUD_FindVar(hud, "onlytp");
		hud_teamfrags_bignum = HUD_FindVar(hud, "bignum");
		hud_teamfrags_colors_alpha = HUD_FindVar(hud, "colors_alpha");
		hud_teamfrags_scale = HUD_FindVar(hud, "scale");
		hud_teamfrags_proportional = HUD_FindVar(hud, "proportional");
	}

	// Don't draw the frags if we're not in teamplay.
	if (hud_teamfrags_onlytp->value && !cl.teamplay) {
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

	if (hud_teamfrags_strip->value) {
		if (hud_teamfrags_vertical->value) {
			a_cols = min((n_teams + rows - 1) / rows, cols);
			a_rows = min(rows, n_teams);
		}
		else {
			a_rows = min((n_teams + cols - 1) / cols, rows);
			a_cols = min(cols, n_teams);
		}
	}
	else {
		a_rows = rows;
		a_cols = cols;
	}

	width = a_cols * cell_width + (a_cols + 1)*space_x;
	height = a_rows * cell_height + (a_rows + 1)*space_y;

	// Get the longest team name for padding.
	if (hud_teamfrags_shownames->value || hud_teamfrags_extra_spec->value) {
		int rlcount_width = 0;

		int cur_length = 0;
		int n;

		for (n = 0; n < n_teams; n++) {
			if (hud_teamfrags_shownames->value) {
				cur_length = strlen(sorted_teams[n].name);

				// Team name
				if (cur_length >= max_team_length) {
					max_team_length = cur_length + 1;
				}
			}
		}

		// Calculate the length of the extra spec info.
		if (hud_teamfrags_extra_spec->value && (cls.demoplayback || cl.spectator)) {
			if (hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_BEFORE) {
				// Draw the rl count before the rl icon.
				rlcount_width = rl_picture.width + 8 + 1 + 1;
			}
			else if (hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_ONTOP) {
				// Draw the rl count on top of the RL instead of infront.
				rlcount_width = rl_picture.width + 1;
			}
			else if (hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_NOICON) {
				// Only draw the rl count.
				rlcount_width = 8 + 1;
			}
			else if (hud_teamfrags_extra_spec->value == TEAMFRAGS_EXTRA_SPEC_RLTEXT) {
				rlcount_width = 8 * 3 + 1;
			}
		}

		width += a_cols * max_team_length * 8 + (a_cols + 1) * space_x + a_cols * rlcount_width;
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		int i;
		int px = 0;
		int py = 0;
		int drawBrackets;
		int limit = min(n_teams, a_rows*a_cols);

		for (i = 0; i < limit; i++) {
			if (hud_teamfrags_vertical->value) {
				if (i % a_rows == 0) {
					px = x + space_x + (i / a_rows) * (cell_width + space_x);
					py = y + space_y;
				}
			}
			else {
				if (i % a_cols == 0) {
					px = x + space_x;
					py = y + space_y + (i / a_cols) * (cell_height + space_y);
				}
			}

			drawBrackets = 0;

			// Bug fix. Before the wrong player would be higlighted
			// during qwd-playback, since you ARE the player that you're
			// being spectated.
			if (cls.demoplayback && !cl.spectator && !cls.mvdplayback) {
				// QWD Playback.
				if (!strcmp(sorted_teams[num].name, cl.players[cl.playernum].team)) {
					drawBrackets = 1;
				}
			}
			else if (cls.demoplayback || cl.spectator) {
				// MVD playback / spectating.
				if (!strcmp(cl.players[cl.spec_track].team, sorted_teams[num].name) && Cam_TrackNum() >= 0) {
					drawBrackets = 1;
				}
			}
			else {
				// Normal player.
				if (!strcmp(sorted_teams[num].name, cl.players[cl.playernum].team)) {
					drawBrackets = 1;
				}
			}

			if (hud_teamfrags_shownames->value || hud_teamfrags_extra_spec->value) {
				int _px = px;
				qbool text_lhs = hud_teamfrags_fliptext->integer == 1;
				text_lhs |= hud_teamfrags_fliptext->integer == 2 && ((i % a_cols) % 2 == 0);
				text_lhs |= hud_teamfrags_fliptext->integer == 3 && ((i % a_cols) % 2 == 1);

				// Draw a background if the style tells us to.
				if (hud_teamfrags_style->value >= 4 && hud_teamfrags_style->value <= 8) {
					Frags_DrawBackground(
						px, py, cell_width, cell_height, space_x, space_y,
						0, max_team_length, sorted_teams[num].bottom,
						0, hud_teamfrags_shownames->value, drawBrackets,
						hud_teamfrags_style->value
					);
				}

				// Draw the text on the left or right side of the score?
				if (text_lhs) {
					// Draw team.
					_px = Frags_DrawText(
						_px, py, cell_width, cell_height,
						space_x, space_y, 0, max_team_length,
						true, hud_teamfrags_padtext->value,
						0, hud_teamfrags_shownames->value,
						"", sorted_teams[num].name, hud_teamfrags_scale->value, hud_teamfrags_proportional->integer,
						0,
						0,
						0
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
						hud_teamfrags_scale->value,
						hud_teamfrags_proportional->integer,
						0,
						0,
						0,
						0
					);

					_px += cell_width + space_x;

					// Draw the rl if the current player has it and the style allows it.
					_px = TeamFrags_DrawExtraSpecInfo(num, _px, py, cell_width, cell_height, hud_teamfrags_extra_spec->value);
				}
				else {
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
						hud_teamfrags_scale->value,
						hud_teamfrags_proportional->integer,
						0,
						0,
						0,
						0
					);

					_px += cell_width + space_x;

					// Draw team.
					_px = Frags_DrawText(
						_px, py, cell_width, cell_height,
						space_x, space_y, 0, max_team_length,
						false, hud_teamfrags_padtext->value,
						0, hud_teamfrags_shownames->value,
						"", sorted_teams[num].name,
						hud_teamfrags_scale->value,
						hud_teamfrags_proportional->integer,
						0,
						0,
						0
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
				Frags_DrawColors(
					px, py, cell_width, cell_height,
					sorted_teams[num].top,
					sorted_teams[num].bottom,
					hud_teamfrags_colors_alpha->value,
					sorted_teams[num].frags,
					drawBrackets,
					hud_teamfrags_style->value,
					hud_teamfrags_bignum->value,
					hud_teamfrags_scale->value,
					hud_teamfrags_proportional->integer,
					0,
					0,
					0,
					0
				);

				if (hud_teamfrags_vertical->value) {
					py += cell_height + space_y;
				}
				else {
					px += cell_width + space_x;
				}
			}
			num++;
		}
	}
}

void Frags_HudInit(void)
{
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
		"hidefrags", "0",
		"shownames", "0",
		"showteams", "0",
		"wipeout", "1",
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
		"proportional", "0",
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
		"proportional", "0",
		NULL
	);
}
