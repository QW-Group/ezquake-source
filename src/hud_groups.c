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
#include "rulesets.h"

// ============================================================================0
// Groups
// ============================================================================0
#define HUD_NUM_GROUPS 9
#define HUD_GROUP_SCALEMODE_TILE		1
#define HUD_GROUP_SCALEMODE_STRETCH		2
#define HUD_GROUP_SCALEMODE_GROW		3
#define HUD_GROUP_SCALEMODE_CENTER		4

#define HUD_GROUP_PIC_BASEPATH	"gfx/%s"

static mpic_t *hud_group_pics[HUD_NUM_GROUPS];
static qbool hud_group_pics_is_loaded[HUD_NUM_GROUPS];

/* Workaround to reload pics on vid_restart
* since cache (where they are stored) is flushed
*/
void HUD_Common_Reset_Group_Pics(void)
{
	memset(hud_group_pics, 0, sizeof(hud_group_pics));
	memset(hud_group_pics_is_loaded, 0, sizeof(hud_group_pics_is_loaded));
}

static void SCR_HUD_DrawGroup(hud_t *hud, int width, int height, mpic_t *pic, int pic_scalemode, float pic_alpha)
{
	int x, y;

	clamp(width, 1, 99999);
	clamp(height, 1, 99999);

	// Set it to this, because 1.0 will make the colors
	// completly saturated, and no semi-transparency will show.
	pic_alpha = (pic_alpha) >= 1.0 ? 1 : pic_alpha;

	// Grow the group if necessary.
	if (pic_scalemode == HUD_GROUP_SCALEMODE_GROW
		&& pic != NULL && pic->height > 0 && pic->width > 0) {
		width = max(pic->width, width);
		height = max(pic->height, height);
	}

	if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
		return;
	}

	// Draw the picture if it's set.
	if (pic != NULL && pic->height > 0) {
		int pw, ph;

		if (pic_scalemode == HUD_GROUP_SCALEMODE_TILE) {
			// Tile.
			int cx = 0, cy = 0;
			while (cy < height) {
				while (cx < width) {
					pw = min(pic->width, width - cx);
					ph = min(pic->height, height - cy);

					if (pw >= pic->width  &&  ph >= pic->height) {
						Draw_AlphaPic(x + cx, y + cy, pic, pic_alpha);
					}
					else {
						Draw_AlphaSubPic(x + cx, y + cy, pic, 0, 0, pw, ph, pic_alpha);
					}

					cx += pic->width;
				}

				cx = 0;
				cy += pic->height;
			}
		}
		else if (pic_scalemode == HUD_GROUP_SCALEMODE_STRETCH) {
			// Stretch or shrink the picture to fit.
			float scale_x = (float)width / pic->width;
			float scale_y = (float)height / pic->height;

			Draw_SAlphaSubPic2(x, y, pic, 0, 0, pic->width, pic->height, scale_x, scale_y, pic_alpha);
		}
		else if (pic_scalemode == HUD_GROUP_SCALEMODE_CENTER) {
			// Center the picture in the group.
			int pic_x = x + (width - pic->width) / 2;
			int pic_y = y + (height - pic->height) / 2;

			int src_x = 0;
			int src_y = 0;

			if (x > pic_x) {
				src_x = x - pic_x;
				pic_x = x;
			}

			if (y > pic_y) {
				src_y = y - pic_y;
				pic_y = y;
			}

			Draw_AlphaSubPic(pic_x, pic_y, pic, src_x, src_y, min(width, pic->width), min(height, pic->height), pic_alpha);
		}
		else {
			// Normal. Draw in the top left corner.
			Draw_AlphaSubPic(x, y, pic, 0, 0, min(width, pic->width), min(height, pic->height), pic_alpha);
		}
	}
}

static qbool SCR_HUD_LoadGroupPic(cvar_t *var, unsigned int grp_id, char *newpic)
{
	char pic_path[MAX_PATH];
	mpic_t *tmp_pic = NULL;

	if (newpic && strlen(newpic) > 0) {
		/* Create path for new pic */
		snprintf(pic_path, sizeof(pic_path), HUD_GROUP_PIC_BASEPATH, newpic);

		// Try loading the pic.
		if (!(tmp_pic = Draw_CachePicSafe(pic_path, false, true))) {
			Com_Printf("Couldn't load picture %s for '%s'\n", newpic, var->name);
			return false;
		}
	}

	hud_group_pics[grp_id] = tmp_pic;

	return true;
}

static void SCR_HUD_OnChangePic_GroupX(cvar_t *var, char *newpic, qbool *cancel)
{
	int idx;
	int res;
	res = sscanf(var->name, "hud_group%d_picture", &idx);
	if (res == 0 || res == EOF) {
		Com_Printf("Failed to parse group number (OnChange func)\n");
		*cancel = true;
	}
	else {
		idx--; /* group1 is on index 0 etc */
		*cancel = Ruleset_BlockHudPicChange() || !SCR_HUD_LoadGroupPic(var, idx, newpic);
	}
}

static void SCR_HUD_Groups_Draw(hud_t *hud)
{
	/* FIXME: Perhaps make some nice structs... */
	static cvar_t *width[HUD_NUM_GROUPS];
	static cvar_t *height[HUD_NUM_GROUPS];
	static cvar_t *picture[HUD_NUM_GROUPS];
	static cvar_t *pic_alpha[HUD_NUM_GROUPS];
	static cvar_t *pic_scalemode[HUD_NUM_GROUPS];
	int res;
	int idx;

	res = sscanf(hud->name, "group%d", &idx);
	if (res == 0 || res == EOF) {
		Com_Printf("Failed to parse group number from \"%s\"\n", hud->name);
		return;
	}
	idx--;

	/* First init */
	if (width[idx] == NULL) { // first time called
		width[idx] = HUD_FindVar(hud, "width");
		height[idx] = HUD_FindVar(hud, "height");
		picture[idx] = HUD_FindVar(hud, "picture");
		pic_alpha[idx] = HUD_FindVar(hud, "pic_alpha");
		pic_scalemode[idx] = HUD_FindVar(hud, "pic_scalemode");

		picture[idx]->OnChange = SCR_HUD_OnChangePic_GroupX;
	}

	if (!hud_group_pics_is_loaded[idx]) {
		SCR_HUD_LoadGroupPic(picture[idx], idx, picture[idx]->string);
		hud_group_pics_is_loaded[idx] = true;
	}

	SCR_HUD_DrawGroup(hud, width[idx]->value, height[idx]->value, hud_group_pics[idx], pic_scalemode[idx]->value, pic_alpha[idx]->value);
}

void Groups_HudInit(void)
{
	// groups
	HUD_Register(
		"group1", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "left", "top", "0", "0", ".5", "0 0 0", NULL,
		"name", "group1",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group2", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "center", "top", "0", "0", ".5", "0 0 0", NULL,
		"name", "group2",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group3", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "right", "top", "0", "0", ".5", "0 0 0", NULL,
		"name", "group3",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group4", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "left", "center", "0", "0", ".5", "0 0 0", NULL,
		"name", "group4",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group5", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "center", "center", "0", "0", ".5", "0 0 0", NULL,
		"name", "group5",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group6", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "right", "center", "0", "0", ".5", "0 0 0", NULL,
		"name", "group6",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group7", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "left", "bottom", "0", "0", ".5", "0 0 0", NULL,
		"name", "group7",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group8", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "center", "bottom", "0", "0", ".5", "0 0 0", NULL,
		"name", "group8",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
	HUD_Register(
		"group9", NULL, "Group element.",
		HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Groups_Draw,
		"0", "screen", "right", "bottom", "0", "0", ".5", "0 0 0", NULL,
		"name", "group9",
		"width", "64",
		"height", "64",
		"picture", "",
		"pic_alpha", "1.0",
		"pic_scalemode", "0",
		NULL
	);
}
