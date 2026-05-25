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

static int TP_IsHealthLowForTotalStrength(void)
{
	extern cvar_t tp_need_health;

	return cl.stats[STAT_HEALTH] <= tp_need_health.value;
}

static qbool HUD_HealthLowForTotalStrength(void)
{
	extern cvar_t hud_tp_need;

	if (hud_tp_need.value) {
		return TP_IsHealthLowForTotalStrength();
	}
	else {
		return HUD_Stats(STAT_HEALTH) <= 25;
	}
}

static void SCR_HUD_DrawTotalStrength(hud_t *hud)
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

        value = SCR_HUD_TotalStrength(HUD_Stats(STAT_HEALTH),HUD_Stats(STAT_ARMOR), SCR_HUD_ArmorType(HUD_Stats(STAT_ITEMS)));
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawNum(hud, (value < 0 ? 0 : value), HUD_HealthLowForTotalStrength(), scale->value, style->value, digits->value, align->string, proportional->integer);
	}
}


void TotalStrength_HudInit(void)
{
	// Total strength
	HUD_Register(
		"totalstrength", NULL, "Part of your status - total strength level.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawTotalStrength,
		"0", "screen", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		NULL
	);

}
