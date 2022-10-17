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

// ----------------
// powerzz
//
static void SCR_HUD_DrawPowerup(hud_t *hud, int num, float scale, int style, qbool proportional)
{
	extern mpic_t *sb_items[32];
	int    x, y, width, height;
	int    c;

	scale = max(scale, 0.01);

	if (cl.spectator == cl.autocam) {
		switch (style) {
		case 1:     // letter
			width = FontFixedWidth(1, scale, false, proportional);
			height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
				return;
			}
			if (HUD_Stats(STAT_ITEMS) & (1 << (17 + num))) {
				switch (num) {
					case 0: c = '1'; break;
					case 1: c = '2'; break;
					case 2: c = 'r'; break;
					case 3: c = 'p'; break;
					case 4: c = 's'; break;
					case 5: c = 'q'; break;
					default: c = '?';
				}
				Draw_SCharacterP(x, y, c, scale, proportional);
			}
			break;
		default:    // classic - pics
			width = height = scale * 16;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
				return;
			}
			if (HUD_Stats(STAT_ITEMS) & (1 << (17 + num))) {
				Draw_SPic(x, y, sb_items[num], scale);
			}
			break;
		}
	}
}

static void SCR_HUD_DrawKey1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 0, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawKey2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 1, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawRing(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 2, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawPent(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 3, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawSuit(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 4, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawQuad(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawPowerup(hud, 5, scale->value, style->value, proportional->integer);
	}
}

// -----------
// sigils
//
static void SCR_HUD_DrawSigil(hud_t *hud, int num, float scale, int style, qbool proportional)
{
	extern mpic_t *sb_sigil[4];
	int     x, y;

	scale = max(scale, 0.01);
	if (cl.spectator == cl.autocam) {
		switch (style) {
		case 1:     // sigil number
			if (!HUD_PrepareDraw(hud, 8 * scale, 8 * scale, &x, &y)) {
				return;
			}
			if (HUD_Stats(STAT_ITEMS) & (1 << (28 + num))) {
				Draw_SCharacterP(x, y, num + '0' + 1, scale, proportional);
			}
			break;
		default:    // classic - picture
			if (!HUD_PrepareDraw(hud, 8 * scale, 16 * scale, &x, &y)) {
				return;
			}
			if (HUD_Stats(STAT_ITEMS) & (1 << (28 + num))) {
				Draw_SPic(x, y, sb_sigil[num], scale);
			}
			break;
		}
	}
}

static void SCR_HUD_DrawSigil1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawSigil(hud, 0, scale->value, style->value, proportional->integer);
	}
}

void SCR_HUD_DrawSigil2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawSigil(hud, 1, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawSigil3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawSigil(hud, 2, scale->value, style->value, proportional->integer);
	}
}

static void SCR_HUD_DrawSigil4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawSigil(hud, 3, scale->value, style->value, proportional->integer);
	}
}

void Items_HudInit(void)
{
	// init powerzz
	HUD_Register(
		"key1", NULL, "Part of your inventory - silver key.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey1,
		"1", "ibar", "top", "left", "0", "64", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"key2", NULL, "Part of your inventory - gold key.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey2,
		"1", "key1", "left", "after", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"ring", NULL, "Part of your inventory - invisibility.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawRing,
		"1", "key2", "left", "after", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"pent", NULL, "Part of your inventory - invulnerability.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawPent,
		"1", "ring", "left", "after", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"suit", NULL, "Part of your inventory - biosuit.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSuit,
		"1", "pent", "left", "after", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"quad", NULL, "Part of your inventory - quad damage.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawQuad,
		"1", "suit", "left", "after", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	// sigilzz
	HUD_Register(
		"sigil1", NULL, "Part of your inventory - sigil 1.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil1,
		"0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"sigil2", NULL, "Part of your inventory - sigil 2.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil2,
		"0", "sigil1", "after", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"sigil3", NULL, "Part of your inventory - sigil 3.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil3,
		"0", "sigil2", "after", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
	HUD_Register(
		"sigil4", NULL, "Part of your inventory - sigil 4.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil4,
		"0", "sigil3", "after", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
