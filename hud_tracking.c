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
#include "sbar.h"
#include "utils.h"

// Tracking text.
// "Tracking: [team] name, JUMP for next", "Tracking:" and "JUMP" are brown. default: "Tracking %t %n, [JUMP] for next"
static cvar_t scr_tracking         = { "scr_tracking", "\xD4\xF2\xE1\xE3\xEB\xE9\xEE\xE7\xBA %t %n, \xCA\xD5\xCD\xD0 for next" };
static cvar_t scr_spectatorMessage = { "scr_spectatorMessage", "1" };

#define MAX_TRACKING_STRING		512

static void SCR_HUD_DrawTracking(hud_t *hud)
{
	int x = 0, y = 0, width = 0, height = 0;
	char track_string[MAX_TRACKING_STRING];
	int player = cl.spec_track;

	static cvar_t
		*hud_tracking_format = NULL,
		*hud_tracking_scale,
		*hud_tracking_proportional;

	if (!hud_tracking_format) {
		hud_tracking_format = HUD_FindVar(hud, "format");
		hud_tracking_scale = HUD_FindVar(hud, "scale");
		hud_tracking_proportional = HUD_FindVar(hud, "proportional");
	}

	strlcpy(track_string, hud_tracking_format->string, sizeof(track_string));
	Replace_In_String(track_string, sizeof(track_string), '%', 2,
		"n", cl.players[player].name,						// Replace %n with player name.
		"t", cl.teamplay ? cl.players[player].team : "");	// Replace %t with player team if teamplay is on.
	height = 8 * hud_tracking_scale->value;
	width = Draw_StringLength(track_string, -1, hud_tracking_scale->value, hud_tracking_proportional->integer);

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		if (cl.spectator && cl.autocam == CAM_TRACK) {
			// Normal
			Draw_SString(x, y, track_string, hud_tracking_scale->value, hud_tracking_proportional->integer);
		}
	}
}

void Tracking_HudInit(void)
{
	// Tracking JohnNy_cz (Contains name of the player who's player we're watching at the moment)
	HUD_Register(
		"tracking", NULL, "Shows the name of tracked player.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawTracking,
		"1", "face", "center", "before", "0", "0", "0", "0 0 0", NULL,
		"format", "\xD4\xF2\xE1\xE3\xEB\xE9\xEE\xE7\xBA %t %n, \xCA\xD5\xCD\xD0 for next", //"Tracking: team name, JUMP for next", "Tracking:" and "JUMP" are brown. default: "Tracking %t %n, [JUMP] for next"
		"scale", "1", "proportional", "0",
		NULL
	);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_tracking);
	Cvar_Register(&scr_spectatorMessage);
	Cvar_ResetCurrentGroup();
}

void Sbar_DrawSpectatorMessage(void)
{
	extern mpic_t* sb_scorebar;

	if (scr_spectatorMessage.value != 0) {
		Sbar_DrawPic(0, 0, sb_scorebar);
		Sbar_DrawString(160 - 7 * 8, 4, "SPECTATOR MODE");
		Sbar_DrawString(160 - 14 * 8 + 4, 12, "Press [ATTACK] for AutoCamera");
	}
}

void Sbar_DrawTrackingString(void)
{
	char st[512];
	extern cvar_t scr_tracking, scr_newHud;

	// If showing ammo on top of status, display higher up
	int y_coordinate = (Sbar_IsStandardBar() ? SBAR_HEIGHT - sb_lines : 0) - 8;

	if (sb_lines > 0 && scr_newHud.value != 1 && cl.spectator && cl.autocam == CAM_TRACK) {
		strlcpy(st, scr_tracking.string, sizeof(st));

		Replace_In_String(st, sizeof(st), '%', 2, "n", cl.players[cl.spec_track].name, "t", cl.teamplay ? cl.players[cl.spec_track].team : "");

		// Multiview
		// Fix displaying "tracking .." for both players with inset on
		if (cl_multiview.value != 2 || !cls.mvdplayback) {
			Sbar_DrawString(0, y_coordinate, st);
		}
		else if (CL_MultiviewCurrentView() == 1 && cl_mvinset.value) {
			Sbar_DrawString(0, y_coordinate, st);
		}
	}
}

