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
#include "gl_model.h"
#include "hud.h"
#include "hud_common.h"

static void SCR_Hud_GameSummary(hud_t* hud)
{
	int x, y;
	int height = 8;
	int width = 0;
	char* format_string;
	int icon_size = 32;

	static cvar_t
		*hud_gamesummary_style = NULL,
		*hud_gamesummary_onlytp,
		*hud_gamesummary_scale,
		*hud_gamesummary_format,
		*hud_gamesummary_circles,
		*hud_gamesummary_ratio,
		*hud_gamesummary_flash,
		*hud_gamesummary_proportional;

	if (hud_gamesummary_style == NULL) {
		// first time
		hud_gamesummary_style = HUD_FindVar(hud, "style");
		hud_gamesummary_onlytp = HUD_FindVar(hud, "onlytp");
		hud_gamesummary_scale = HUD_FindVar(hud, "scale");
		hud_gamesummary_format = HUD_FindVar(hud, "format");
		hud_gamesummary_circles = HUD_FindVar(hud, "circles");
		hud_gamesummary_ratio = HUD_FindVar(hud, "ratio");
		hud_gamesummary_flash = HUD_FindVar(hud, "flash");
		hud_gamesummary_proportional = HUD_FindVar(hud, "proportional");
	}

	// Don't show when not in teamplay/demoplayback.
	if (!cls.mvdplayback || !HUD_ShowInDemoplayback(hud_gamesummary_onlytp->value)) {
		HUD_PrepareDraw(hud, 0, 0, &x, &y);
		return;
	}

	// Measure for width
	icon_size = 8 * bound(1, hud_gamesummary_ratio->value, 8);
	width = strlen(hud_gamesummary_format->string) * icon_size * hud_gamesummary_scale->value;
	height = (icon_size + (hud_gamesummary_style->integer ? 8 : 0)) * hud_gamesummary_scale->value;

	if (HUD_PrepareDraw(hud, width, height, &x, &y)) {
		int text_offset = hud_gamesummary_scale->value * (icon_size / 2 - 4);
		int icon_offset = 0;

		if (hud_gamesummary_style->integer == 1) {
			text_offset = icon_size * hud_gamesummary_scale->value;
		}
		else if (hud_gamesummary_style->integer == 2) {
			text_offset = 0;
			icon_offset = 8 * hud_gamesummary_scale->value;
		}

		for (format_string = hud_gamesummary_format->string; *format_string; ++format_string) {
			int team = isupper(format_string[0]) ? 0 : 1;
			mpic_t* background_tex = NULL;
			int value = 0;
			float alpha = 1.0f;
			float last_taken = 0.0f;
			byte* background_color = NULL;
			byte mega_color[] = { 0xFF, 0x8C, 0x00 };
			byte ra_color[] = { 0xFF, 0x00, 0x00 };
			byte ya_color[] = { 0xFF, 0xFF, 0x00 };
			byte ga_color[] = { 0x00, 0xBF, 0x00 };
			byte rl_color[] = { 0xFF, 0x1F, 0x3F };
			byte lg_color[] = { 0x2F, 0xAA, 0xAA };
			byte quad_color[] = { 0x00, 0x5F, 0xFF };
			byte pent_color[] = { 0xFF, 0x00, 0x00 };
			byte ring_color[] = { 0xFF, 0xFF, 0x00 };

			switch (tolower(format_string[0])) {
				case 'm':
					background_tex = Mod_SimpleTextureForHint(MOD_MEGAHEALTH, 0);
					value = sorted_teams[team].mh_taken;
					background_color = mega_color;
					last_taken = sorted_teams[team].mh_lasttime;
					break;
				case 'r':
					background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 2);
					value = sorted_teams[team].ra_taken;
					background_color = ra_color;
					last_taken = sorted_teams[team].ra_lasttime;
					break;
				case 'y':
					background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 1);
					value = sorted_teams[team].ya_taken;
					background_color = ya_color;
					last_taken = sorted_teams[team].ya_lasttime;
					break;
				case 'g':
					background_tex = Mod_SimpleTextureForHint(MOD_ARMOR, 0);
					value = sorted_teams[team].ga_taken;
					background_color = ga_color;
					last_taken = sorted_teams[team].ga_lasttime;
					break;
				case 'o':
					background_tex = Mod_SimpleTextureForHint(MOD_ROCKETLAUNCHER, 0);
					value = sorted_teams[team].rlcount;
					background_color = rl_color;
					last_taken = value ? -1 : 0;
					break;
				case 'l':
					background_tex = Mod_SimpleTextureForHint(MOD_LIGHTNINGGUN, 0);
					value = sorted_teams[team].lgcount;
					background_color = lg_color;
					last_taken = value ? -1 : 0;
					break;
				case 'w':
					background_tex = Mod_SimpleTextureForHint(MOD_ROCKETLAUNCHER, 0);
					value = sorted_teams[team].weapcount;
					background_color = rl_color;
					last_taken = value ? -1 : 0;
					break;
				case 'q':
					background_tex = Mod_SimpleTextureForHint(MOD_QUAD, 0);
					value = sorted_teams[team].quads_taken;
					background_color = quad_color;
					last_taken = sorted_teams[team].q_lasttime;
					break;
				case 'p':
					background_tex = Mod_SimpleTextureForHint(MOD_PENT, 0);
					value = sorted_teams[team].pents_taken;
					background_color = pent_color;
					last_taken = sorted_teams[team].p_lasttime;
					break;
				case 'e':
					background_tex = Mod_SimpleTextureForHint(MOD_RING, 0);
					value = sorted_teams[team].rings_taken;
					background_color = ring_color;
					last_taken = sorted_teams[team].r_lasttime;
					break;
			}

			alpha = last_taken == -1 ? 1.0f : 0.3f;
			if (last_taken > 0 && cls.demotime >= last_taken) {
				if (hud_gamesummary_flash->integer && cls.demotime - last_taken <= 1.0f) {
					// Flash for first second
					alpha = (int)((cls.demotime - last_taken) * 10) % 4 >= 2 ? 0.5f : 1.0f;
				}
				else if (cls.demotime - last_taken <= 2.3f) {
					// Then solid colour
					alpha = 1.0f;
				}
				else if (cls.demotime - last_taken <= 3.0f) {
					// Then fade back to normal
					alpha = bound(0.3f, 1.0f - (cls.demotime - last_taken - 2.3f), 1.0f);
				}
			}

			{
				char buffer[10];

				snprintf(buffer, sizeof(buffer), "%d", value);

				if (hud_gamesummary_circles->integer && background_color) {
					float half_width = icon_size * 0.5f * hud_gamesummary_scale->value;

					alpha /= 2;

					Draw_AlphaCircleFillRGB(x + half_width, y + icon_offset + half_width, half_width, RGBA_TO_COLOR(background_color[0] * alpha, background_color[1] * alpha, background_color[2] * alpha, alpha * 255));
				}
				else if (background_tex && R_TextureReferenceIsValid(background_tex->texnum)) {
					Draw_FitPicAlpha(x, y + icon_offset, icon_size * hud_gamesummary_scale->value, icon_size * hud_gamesummary_scale->value, background_tex, alpha);
				}

				if (value) {
					Draw_SStringAligned(x, y + text_offset, buffer, hud_gamesummary_scale->value, 1.0f, hud_gamesummary_proportional->integer, text_align_center, x + icon_size * hud_gamesummary_scale->value);
				}
			}

			x += icon_size * hud_gamesummary_scale->value;
		}
	}
}

void GameSummary_HudInit(void)
{
	HUD_Register(
		"gamesummary", NULL, "Shows total pickups & current weapons.",
		HUD_PLUSMINUS, ca_active, 0, SCR_Hud_GameSummary,
		"0", "top", "left", "top", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"onlytp", "0",
		"scale", "1",
		"format", "RYM myr",
		"circles", "0",
		"ratio", "4",
		"flash", "1",
		"proportional", "0",
		NULL
	);
}
