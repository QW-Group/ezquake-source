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
#include "keys.h"
#include "menu.h"
#include "hud.h"

static cvar_t scr_centertime  = { "scr_centertime",  "2" };
static cvar_t scr_centershift = { "scr_centershift", "0" };
static cvar_t scr_centerspeed = { "scr_centerspeed", "8" };

/**************************** CENTER PRINTING ********************************/

static char	 scr_centerstring_lines[1024][41];

static float scr_centertime_start;   // for slow victory printing
static float scr_centertime_off;
static int   scr_center_lines;
static int   scr_erase_lines;
static int   scr_erase_center;

void SCR_CenterPrint_Clear(void)
{
	// Make sure no centerprint messages are left from previous level.
	scr_centertime_off = 0;
	memset(scr_centerstring_lines, 0, sizeof(scr_centerstring_lines));
}

void SCR_CenterPrint_Init(void)
{
	if (!host_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
		Cvar_Register(&scr_centertime);
		Cvar_Register(&scr_centershift);
		Cvar_Register(&scr_centerspeed);
		Cvar_ResetCurrentGroup();

		Cmd_AddLegacyCommand("scr_printspeed", "scr_centerspeed");
	}
}

// Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint(const char *str)
{
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;
	memset(scr_centerstring_lines, 0, sizeof(scr_centerstring_lines));

	// count the number of lines for centering
	scr_center_lines = 0;
	while (*str) {
		const char* endl = strchr(str, '\n');
		if (!endl) {
			strlcpy(scr_centerstring_lines[scr_center_lines], str, sizeof(scr_centerstring_lines[scr_center_lines]));
			++scr_center_lines;
			break;
		}
		else {
			int len = endl - str;
			len = min(len, sizeof(scr_centerstring_lines[scr_center_lines]) - 1);
			strlcpy(scr_centerstring_lines[scr_center_lines], str, sizeof(scr_centerstring_lines[scr_center_lines]));
			scr_centerstring_lines[scr_center_lines][len] = '\0';
			++scr_center_lines;

			str = endl + 1;
		}
	}
}

static void SCR_DrawCenterString(float x, float y, float scale, qbool proportional, float speed)
{
	// the finale prints the characters one at a time
	int remaining = cl.intermission ? speed * (cl.time - scr_centertime_start) : -1;
	int l;
	float max_width = (sizeof(scr_centerstring_lines[l]) - 1) * 8 * scale;

	scale = max(scale, 0.1);
	scr_erase_center = 0;
	if (remaining == 0) {
		return;
	}

	for (l = 0; l < scr_center_lines; ++l) {
		if (remaining >= 0 && remaining < strlen(scr_centerstring_lines[l])) {
			// Can't use standard centering here... we center to the full line, we might print less than that...
			char temp[1024];
			int len;

			strlcpy(temp, scr_centerstring_lines[l], sizeof(temp));
			temp[remaining] = '\0';
			len = Draw_StringLength(scr_centerstring_lines[l], -1, scale, proportional);

			Draw_SString(x + (max_width - len) / 2, y, temp, scale, proportional);
		}
		else {
			Draw_SStringAligned(x, y, scr_centerstring_lines[l], scale, 1.0f, proportional, text_align_center, x + max_width);
		}

		if (remaining >= 0) {
			remaining -= strlen(scr_centerstring_lines[l]);
			if (remaining <= 0) {
				break;
			}
		}
		y += 8 * scale;
	}
}

static qbool SCR_CheckDrawCenterString(void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines) {
		scr_erase_lines = scr_center_lines;
	}

	if (!scr_center_lines) {
		return false;
	}

	scr_centertime_off -= cls.frametime;
	if (scr_centertime_off <= 0 && !cl.intermission) {
		return false;
	}

	// condition says: "Draw center string only when in game or in proxy menu, otherwise leave."
	if (key_dest != key_game && ((key_dest != key_menu) || (m_state != m_proxy))) {
		return false;
	}

	return true;
}

void SCR_CenterString_Draw(void)
{
	float y;
	float max_width;
	float scale = 1.0f;
	qbool proportional = false;
	extern cvar_t scr_newHud;
	static cvar_t* hud_draw = NULL;

	if (!SCR_CheckDrawCenterString()) {
		return;
	}

	if (hud_draw == NULL) {
		hud_draw = Cvar_Find("hud_centerprint_show");
	}

	if (scr_newHud.integer > 0 && hud_draw && hud_draw->integer) {
		return;
	}

	// shift all centerprint but not proxy menu - more user-friendly way
	y = ((scr_center_lines <= 4) ? vid.height * 0.35 : 48);
	if (m_state != m_proxy) {
		y += scr_centershift.value * 8 * scale;
	}
	max_width = (sizeof(scr_centerstring_lines[0]) - 1) * 8 * scale;

	SCR_DrawCenterString((vid.width - max_width) / 2, y, scale, proportional, max(scr_centerspeed.value, 1));
}

void SCR_EraseCenterString(void)
{
	int y;

	if (scr_erase_center++ > vid.numpages) {
		scr_erase_lines = 0;
		return;
	}

	y = (scr_center_lines <= 4) ? vid.height * 0.35 : 48;

	scr_copytop = 1;
	Draw_TileClear(0, y, vid.width, min(8 * scr_erase_lines, vid.height - y - 1));
}

static void SCR_HUD_DrawCenterPrint(hud_t* hud)
{
	int x = 0, y = 0;
	float width = 0, height = 0;

	static cvar_t
		*hud_scale = NULL,
		*hud_proportional,
		*hud_speed;

	if (!SCR_CheckDrawCenterString()) {
		return;
	}

	if (!hud_scale) {
		hud_scale = HUD_FindVar(hud, "scale");
		hud_proportional = HUD_FindVar(hud, "proportional");
		hud_speed = HUD_FindVar(hud, "speed");
	}

	width = 40 * 8 * hud_scale->value;
	height = 12 * 8 * hud_scale->value;

	if (height > 0 && width > 0 && HUD_PrepareDraw(hud, ceil(width), ceil(height), &x, &y)) {
		SCR_DrawCenterString(x, y, hud_scale->value, hud_proportional->value, max(hud_speed->integer, 1));
	}
}

void CenterPrint_HudInit(void)
{
	HUD_Register(
		"centerprint", NULL, "Shows alerts from server, countdowns etc.",
		HUD_PLUSMINUS | HUD_ON_FINALE, ca_active, 0, SCR_HUD_DrawCenterPrint,
		"0", "screen", "center", "center", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"proportional", "0",
		"speed", "8",
		NULL
	);
}
