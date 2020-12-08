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
#include "common_draw.h"
#include "hud.h"
#include "hud_common.h"
#include "fonts.h"
#include "teamplay.h"
#include "vx_stuff.h"

static int TP_IsArmorLow(void)
{
	extern cvar_t tp_need_ra, tp_need_ya, tp_need_ga;

	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR3)) {
		return cl.stats[STAT_ARMOR] <= tp_need_ra.value;
	}
	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR2)) {
		return cl.stats[STAT_ARMOR] <= tp_need_ya.value;
	}
	if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR1)) {
		return cl.stats[STAT_ARMOR] <= tp_need_ga.value;
	}
	return 1;
}

static qbool HUD_ArmorLow(void)
{
	extern cvar_t hud_tp_need;

	if (hud_tp_need.value) {
		return (TP_IsArmorLow());
	}
	else {
		return (HUD_Stats(STAT_ARMOR) <= 25);
	}
}

static void SCR_HUD_DrawArmor(hud_t *hud)
{
	int level;
	qbool low;
	static cvar_t *scale = NULL, *style, *digits, *align, *pent_666, *proportional;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		pent_666 = HUD_FindVar(hud, "pent_666"); // Show 666 or armor value when carrying pentagram
		proportional = HUD_FindVar(hud, "proportional");
	}

	if (HUD_Stats(STAT_HEALTH) > 0) {
		if ((HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) && pent_666->integer) {
			level = 666;
			low = true;
		}
		else {
			level = HUD_Stats(STAT_ARMOR);
			low = HUD_ArmorLow();
		}
	}
	else {
		level = 0;
		low = true;
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, level, low, scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}

static void SCR_HUD_DrawArmorIcon(hud_t *hud)
{
	extern mpic_t  *sb_armor[3];
	extern mpic_t  *draw_disc;
	int   x, y, height;

	int style;
	float scale;

	static cvar_t *v_scale = NULL, *v_style, *v_proportional;
	if (v_scale == NULL) {
		// first time called
		v_scale = HUD_FindVar(hud, "scale");
		v_style = HUD_FindVar(hud, "style");
		v_proportional = HUD_FindVar(hud, "proportional");
	}

	scale = max(v_scale->value, 0.01);
	style = (int)(v_style->value);

	height = (style ? 8 : 24) * scale;

	if (cl.spectator == cl.autocam) {
		if (style) {
			int c;

			if (!HUD_PrepareDraw(hud, FontFixedWidth(1, scale, 0, v_proportional->integer), height, &x, &y)) {
				return;
			}

			if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) {
				c = '@';
			}
			else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3) {
				c = 'r';
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2) {
				c = 'y';
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1) {
				c = 'g';
			}
			else {
				return;
			}

			c += 128;

			Draw_SCharacterP(x, y, c, scale, v_proportional->integer);
		}
		else {
			mpic_t* pic;

			if (!HUD_PrepareDraw(hud, 24 * scale, height, &x, &y)) {
				return;
			}

			if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) {
				pic = draw_disc;
			}
			else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3) {
				pic = sb_armor[2];
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2) {
				pic = sb_armor[1];
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1) {
				pic = sb_armor[0];
			}
			else {
				return;
			}

			Draw_SPic(x, y, pic, scale);
		}
	}
}

static void SCR_HUD_DrawArmorDamage(hud_t *hud)
{
	Draw_AMFStatLoss(STAT_ARMOR, hud);
}

void SCR_HUD_DrawBarArmor(hud_t *hud)
{
	static	cvar_t *width = NULL, *height, *direction, *color_noarmor, *color_ga, *color_ya, *color_ra, *color_unnatural;
	int		x, y;
	int		armor = HUD_Stats(STAT_ARMOR);
	qbool	alive = cl.stats[STAT_HEALTH] > 0;

	if (width == NULL) {
		// first time called
		width = HUD_FindVar(hud, "width");
		height = HUD_FindVar(hud, "height");
		direction = HUD_FindVar(hud, "direction");
		color_noarmor = HUD_FindVar(hud, "color_noarmor");
		color_ga = HUD_FindVar(hud, "color_ga");
		color_ya = HUD_FindVar(hud, "color_ya");
		color_ra = HUD_FindVar(hud, "color_ra");
		color_unnatural = HUD_FindVar(hud, "color_unnatural");
	}

	if (HUD_PrepareDraw(hud, width->integer, height->integer, &x, &y) && (cl.spectator == cl.autocam)) {
		if (!width->integer || !height->integer) {
			return;
		}

		if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY && alive) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_unnatural->color, x, y, width->integer, height->integer);
		}
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3 && alive) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 200.0, color_ra->color, x, y, width->integer, height->integer);
		}
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2 && alive) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 150.0, color_ya->color, x, y, width->integer, height->integer);
		}
		else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1 && alive) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, armor, 100.0, color_ga->color, x, y, width->integer, height->integer);
		}
		else {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_noarmor->color, x, y, width->integer, height->integer);
		}
	}
}

void Armor_HudInit(void)
{
	// armor count
	HUD_Register(
		"armor", NULL, "Part of your inventory - armor level.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmor,
		"1", "face", "before", "center", "-32", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"pent_666", "1",  // Show 666 instead of armor value
		"proportional", "0",
		NULL
	);

	// armor icon
	HUD_Register(
		"iarmor", NULL, "Part of your inventory - armor icon.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorIcon,
		"1", "armor", "before", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);

	// armordamage
	HUD_Register(
		"armordamage", NULL, "Shows amount of damage done to your armour.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorDamage,
		"0", "armor", "left", "before", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"duration", "0.8",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"bar_armor", NULL, "Armor bar.",
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
}
