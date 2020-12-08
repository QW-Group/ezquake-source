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

// face
void SCR_HUD_DrawFace(hud_t *hud)
{
	extern mpic_t  *sb_faces[5][2]; // 0 is dead, 1-4 are alive
									// 0 is static, 1 is temporary animation
	extern mpic_t  *sb_face_invis;
	extern mpic_t  *sb_face_quad;
	extern mpic_t  *sb_face_invuln;
	extern mpic_t  *sb_face_invis_invuln;

	int     f, anim;
	int     x, y;
	float   scale;

	static cvar_t *v_scale = NULL;
	if (v_scale == NULL)  // first time called
	{
		v_scale = HUD_FindVar(hud, "scale");
	}

	scale = max(v_scale->value, 0.01);

	if (cl.spectator != cl.autocam)
		return;
	
	if (!HUD_PrepareDraw(hud, 24 * scale, 24 * scale, &x, &y))
		return;

	if ((HUD_Stats(STAT_ITEMS) & (IT_INVISIBILITY | IT_INVULNERABILITY))
		== (IT_INVISIBILITY | IT_INVULNERABILITY)) {
		Draw_SPic(x, y, sb_face_invis_invuln, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_QUAD) {
		Draw_SPic(x, y, sb_face_quad, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_INVISIBILITY) {
		Draw_SPic(x, y, sb_face_invis, scale);
		return;
	}
	if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) {
		Draw_SPic(x, y, sb_face_invuln, scale);
		return;
	}

	if (HUD_Stats(STAT_HEALTH) >= 100)
		f = 4;
	else
		f = max(0, HUD_Stats(STAT_HEALTH)) / 20;

	if (cl.time <= cl.faceanimtime)
		anim = 1;
	else
		anim = 0;
	Draw_SPic(x, y, sb_faces[f][anim], scale);
}

void Face_HudInit(void)
{
	// player face (health indicator)
	HUD_Register(
		"face", NULL, "Your bloody face.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawFace,
		"1", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"scale", "1",
		NULL
	);
}
