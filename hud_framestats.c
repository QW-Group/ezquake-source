/*
Copyright (C) 2018 ezQuake team

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

// 'framestats' hud element
// like r_speeds, but hopefully better and data appropriate to different renderers

#include "quakedef.h"
#include "common_draw.h"
#include "hud.h"
#include "gl_model.h"
#include "gl_local.h"

static void FrameStats_DrawElement(hud_t *hud);

// Registers the component with the hud framework
void FrameStats_HudInit(void)
{
	HUD_Register(
		"framestats", NULL,
		"Shows information about the renderer's status & workload.",
		HUD_PLUSMINUS, ca_active, 0, FrameStats_DrawElement,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		NULL
	);
}

static void FrameStats_DrawElement(hud_t *hud)
{
	static cvar_t
		*hud_frameStats_style = NULL,
		*hud_frameStats_scale;
	static char content[32][128];

	int height = 8;
	int width = 0;
	int x = 0;
	int y = 0;
	int lines = 0;
	int i;

	if (hud_frameStats_style == NULL) {
		// first time
		hud_frameStats_style = HUD_FindVar(hud, "style");
		hud_frameStats_scale = HUD_FindVar(hud, "scale");
	}

	snprintf(content[lines++], sizeof(content[0]), "Draw calls:          %4d", prevFrameStats.draw_calls);
	snprintf(content[lines++], sizeof(content[0]), "Sub-draw calls:      %4d", prevFrameStats.subdraw_calls);
	snprintf(content[lines++], sizeof(content[0]), "Texture switches:    %4d", prevFrameStats.texture_binds);
	snprintf(content[lines++], sizeof(content[0]), "Lightmap uploads:    %4d", prevFrameStats.lightmap_updates);
	if (GL_UseImmediateMode()) {
		content[lines++][0] = '\0';
		snprintf(content[lines++], sizeof(content[0]), "World-model polys:   %4d", frameStats.classic.polycount[polyTypeWorldModel]);
		if (cl.standby || com_serveractive) {
			snprintf(content[lines++], sizeof(content[0]), "Alias-model polys:   %4d", frameStats.classic.polycount[polyTypeAliasModel]);
			snprintf(content[lines++], sizeof(content[0]), "Brush-model polys:   %4d", frameStats.classic.polycount[polyTypeBrushModel]);
		}
	}

	height = lines * 8;
	for (i = 0; i < lines; ++i) {
		width = max(strlen(content[i]) * 8, width);
	}

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		for (i = 0; i < lines; ++i, y += 8) {
			if (content[i][0]) {
				Draw_SString(x, y, content[i], hud_frameStats_scale->value, false);
			}
		}
	}
}
