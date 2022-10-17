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
#include "vx_stuff.h"

static int TP_IsHealthLow(void)
{
	extern cvar_t tp_need_health;

	return cl.stats[STAT_HEALTH] <= tp_need_health.value;
}

static qbool HUD_HealthLow(void)
{
	extern cvar_t hud_tp_need;

	if (hud_tp_need.value) {
		return TP_IsHealthLow();
	}
	else {
		return HUD_Stats(STAT_HEALTH) <= 25;
	}
}

static void SCR_HUD_DrawHealth(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional;
	static int value;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
	}

	value = HUD_Stats(STAT_HEALTH);
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, (value < 0 ? 0 : value), HUD_HealthLow(), scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}

static void SCR_HUD_DrawHealthDamage(hud_t *hud)
{
	Draw_AMFStatLoss(STAT_HEALTH, hud);

	if (HUD_Stats(STAT_HEALTH) <= 0) {
		Amf_Reset_DamageStats();
	}
}

static void SCR_HUD_DrawBarHealth(hud_t *hud)
{
	static	cvar_t *width = NULL, *height, *direction, *color_nohealth, *color_normal, *color_mega, *color_twomega, *color_unnatural;
	int		x, y;
	int		health = cl.stats[STAT_HEALTH];

	if (width == NULL) {
		// first time called
		width = HUD_FindVar(hud, "width");
		height = HUD_FindVar(hud, "height");
		direction = HUD_FindVar(hud, "direction");
		color_nohealth = HUD_FindVar(hud, "color_nohealth");
		color_normal = HUD_FindVar(hud, "color_normal");
		color_mega = HUD_FindVar(hud, "color_mega");
		color_twomega = HUD_FindVar(hud, "color_twomega");
		color_unnatural = HUD_FindVar(hud, "color_unnatural");
	}

	if (HUD_PrepareDraw(hud, width->integer, height->integer, &x, &y) && (cl.spectator == cl.autocam)) {
		if (!width->integer || !height->integer) {
			return;
		}

		if (health > 250) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_unnatural->color, x, y, width->integer, height->integer);
		}
		else if (health > 200) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_normal->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_mega->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health - 200, 100.0, color_twomega->color, x, y, width->integer, height->integer);
		}
		else if (health > 100) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_normal->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health - 100, 100.0, color_mega->color, x, y, width->integer, height->integer);
		}
		else if (health > 0) {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_nohealth->color, x, y, width->integer, height->integer);
			SCR_HUD_DrawBar(direction->integer, health, 100.0, color_normal->color, x, y, width->integer, height->integer);
		}
		else {
			SCR_HUD_DrawBar(direction->integer, 100, 100.0, color_nohealth->color, x, y, width->integer, height->integer);
		}
	}
}

void Health_HudInit(void)
{
	HUD_Register(
		"bar_health", NULL, "Health bar.",
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

	// health
	HUD_Register(
		"health", NULL, "Part of your status - health level.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealth,
		"1", "face", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

	// healthdamage
	HUD_Register(
		"healthdamage", NULL, "Shows amount of damage done to your health.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealthDamage,
		"0", "health", "left", "before", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"duration", "0.8",
		"proportional", "0",
		NULL
	);
}
