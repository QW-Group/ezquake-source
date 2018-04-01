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
#include "tr_types.h"

// ----------------
// DrawFPS
static void SCR_HUD_DrawFPS(hud_t *hud)
{
	int x, y;
	char st[128];

	static cvar_t
		*hud_fps_show_min = NULL,
		*hud_fps_style,
		*hud_fps_title,
		*hud_fps_drop,
		*hud_fps_scale,
		*hud_fps_proportional;

	if (hud_fps_show_min == NULL) {
		// first time called
		hud_fps_show_min = HUD_FindVar(hud, "show_min");
		hud_fps_style = HUD_FindVar(hud, "style");
		hud_fps_title = HUD_FindVar(hud, "title");
		hud_fps_drop = HUD_FindVar(hud, "drop");
		hud_fps_scale = HUD_FindVar(hud, "scale");
		hud_fps_proportional = HUD_FindVar(hud, "proportional");
	}

	if (hud_fps_show_min->value) {
		snprintf(st, sizeof(st), "%3d\xf%3d", (int)(cls.min_fps + 0.25), (int)(cls.fps + 0.25));
	}
	else {
		snprintf(st, sizeof(st), "%3d", (int)(cls.fps + 0.25));
	}

	if (hud_fps_title->value) {
		strlcat(st, " fps", sizeof(st));
	}

	if (HUD_PrepareDraw(hud, Draw_StringLength(st, -1, hud_fps_scale->value, hud_fps_proportional->integer), 8 * hud_fps_scale->value, &x, &y)) {
		if ((hud_fps_style->value) == 1) {
			Draw_SAlt_String(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
		}
		else if ((hud_fps_style->value) == 2) {
			// if fps is less than a user-set value, then show it
			if ((hud_fps_drop->value) >= cls.fps) {
				Draw_SString(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
			}
		}
		else if ((hud_fps_style->value) == 3) {
			// if fps is less than a user-set value, then show it
			if ((hud_fps_drop->value) >= cls.fps) {
				Draw_SAlt_String(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
			}
		}
		else {
			// hud_fps_style is anything other than 1,2,3
			Draw_SString(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
		}
	}
}

static void SCR_HUD_DrawVidLag(hud_t *hud)
{
	int x, y;
	char st[128];
	static cvar_t *hud_vidlag_style = NULL;
	static cvar_t *hud_vidlag_scale = NULL;
	static cvar_t *hud_vidlag_proportional = NULL;

	extern qbool VID_VSyncIsOn(void);
	extern double vid_vsync_lag;
	static double old_lag;

	if (VID_VSyncIsOn() || glConfig.displayFrequency) {
		// take the average of last two values, otherwise it
		// changes very fast and is hard to read
		double current, avg;
		if (VID_VSyncIsOn()) {
			current = vid_vsync_lag;
		}
		else {
			current = min(cls.trueframetime, 1.0 / glConfig.displayFrequency) * 0.5;
		}
		avg = (current + old_lag) * 0.5;
		old_lag = current;
		snprintf(st, sizeof(st), "%2.1f", avg * 1000);
	}
	else {
		strcpy(st, "?");
	}

	if (hud_vidlag_style == NULL) {
		// first time called
		hud_vidlag_style = HUD_FindVar(hud, "style");
		hud_vidlag_scale = HUD_FindVar(hud, "scale");
		hud_vidlag_proportional = HUD_FindVar(hud, "proportional");
	}

	strlcat(st, " ms", sizeof(st));

	if (HUD_PrepareDraw(hud, Draw_StringLength(st, -1, hud_vidlag_scale->value, hud_vidlag_proportional->integer), 8 * hud_vidlag_scale->value, &x, &y)) {
		if (hud_vidlag_style->value) {
			Draw_SAlt_String(x, y, st, hud_vidlag_scale->value, hud_vidlag_proportional->integer);
		}
		else {
			Draw_SString(x, y, st, hud_vidlag_scale->value, hud_vidlag_proportional->integer);
		}
	}
}

void Performance_HudInit(void)
{
	// fps
	HUD_Register(
		"fps", NULL,
		"Shows your current framerate in frames per second (fps)."
		"This can also show the minimum framerate that occured in the last measured period.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawFPS,
		"1", "top", "center", "after", "0", "0", "0", "0 0 0", NULL,
		"show_min", "0",
		"style", "0",
		"title", "1",
		"scale", "1",
		"drop", "70",
		"proportional", "0",
		NULL
	);

	HUD_Register(
		"vidlag", NULL,
		"Shows the delay between the time a frame is rendered and the time it's displayed.",
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawVidLag,
		"0", "top", "right", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
