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
#include "r_framestats.h"

r_frame_stats_t prevFrameStats;
r_frame_stats_t frameStats;

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
		switch (hud_fps_style->integer) {
			case 1:
				Draw_SAlt_String(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);;
				break;
			case 2:
				// if fps is less than a user-set value, then show it
				if ((hud_fps_drop->value) >= cls.fps) {
					Draw_SString(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
				}
				break;
			case 3:
				// if fps is less than a user-set value, then show it
				if ((hud_fps_drop->value) >= cls.fps) {
					Draw_SAlt_String(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
				}
				break;
			default:
				Draw_SString(x, y, st, hud_fps_scale->value, hud_fps_proportional->integer);
				break;
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

static void FrameStats_AddLine(char content[2][64], const char* name, int value)
{
	strlcpy(content[0], name, sizeof(content[0]));
	if (name[0]) {
		snprintf(content[1], sizeof(content[1]), "%4d", value);
	}
	else {
		content[1][0] = '\0';
	}
}

#define MAX_FRAMESTATS_LINES 32

static void FrameStats_DrawElement(hud_t *hud)
{
	static cvar_t
		*hud_frameStats_style = NULL,
		*hud_frameStats_scale,
		*hud_frameStats_proportional;
	static char content[MAX_FRAMESTATS_LINES][2][64];

	int height = 8;
	int width = 0;
	int x = 0;
	int y = 0;
	int lines = 0;
	int i;
	int max_field_length;
	int max_value_length;
	int value_length[MAX_FRAMESTATS_LINES];

	if (hud_frameStats_style == NULL) {
		// first time
		hud_frameStats_style = HUD_FindVar(hud, "style");
		hud_frameStats_scale = HUD_FindVar(hud, "scale");
		hud_frameStats_proportional = HUD_FindVar(hud, "proportional");
	}

	FrameStats_AddLine(content[lines++], "Draw calls:", prevFrameStats.draw_calls);
	FrameStats_AddLine(content[lines++], "Sub-draw calls:", prevFrameStats.subdraw_calls);
	FrameStats_AddLine(content[lines++], "Texture switches:", prevFrameStats.texture_binds);
	FrameStats_AddLine(content[lines++], "Lightmap uploads:", prevFrameStats.lightmap_updates);
	if (frameStats.classic.polycount[polyTypeWorldModel]) {
		FrameStats_AddLine(content[lines++], "", 0);
		FrameStats_AddLine(content[lines++], "World-model polys:", frameStats.classic.polycount[polyTypeWorldModel]);
		if (cl.standby || com_serveractive) {
			FrameStats_AddLine(content[lines++], "Alias-model polys:", frameStats.classic.polycount[polyTypeAliasModel]);
			FrameStats_AddLine(content[lines++], "Brush-model polys:", frameStats.classic.polycount[polyTypeBrushModel]);
		}
	}

	height = lines * 8 * hud_frameStats_scale->value;
	max_field_length = max_value_length = 0;
	for (i = 0; i < lines; ++i) {
		int name_length = Draw_StringLength(content[i][0], -1, hud_frameStats_scale->value, hud_frameStats_proportional->integer);
		value_length[i] = Draw_StringLength(content[i][1], -1, hud_frameStats_scale->value, hud_frameStats_proportional->integer);

		max_field_length = max(max_field_length, name_length);
		max_value_length = max(max_value_length, value_length[i]);
	}

	width = max_field_length + 8 * hud_frameStats_scale->value + max_value_length;
	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		for (i = 0; i < lines; ++i, y += 8 * hud_frameStats_scale->value) {
			if (content[i][0][0]) {
				Draw_SString(x, y, content[i][0], hud_frameStats_scale->value, hud_frameStats_proportional->integer);
				Draw_SString(x + width - value_length[i], y, content[i][1], hud_frameStats_scale->value, hud_frameStats_proportional->integer);
			}
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
		"1", "gameclock", "center", "after", "0", "0", "0", "0 0 0", NULL,
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

	HUD_Register(
		"framestats", NULL,
		"Shows information about the renderer's status & workload.",
		HUD_PLUSMINUS, ca_active, 0, FrameStats_DrawElement,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		"proportional", "0",
		NULL
	);
}
