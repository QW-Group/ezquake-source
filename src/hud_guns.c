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
#include "teamplay.h"
#include "utils.h"

// -----------
// gunz
//
static void SCR_HUD_DrawGunByNum(hud_t *hud, int num, float scale, int style, int wide, qbool proportional)
{
	extern mpic_t *sb_weapons[7][8];  // sbar.c
	int i = num - 2;
	int width, height;
	int x, y;
	char *tmp;

	scale = max(scale, 0.01);

	switch (style) {
		case 3: // opposite colors of case 1
		case 1:     // text, gold inactive, white active
			width = FontFixedWidth(2, scale, false, proportional);
			height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
				return;
			}
			if (HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN << i)) {
				qbool weapon_active = (HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN << i));

				switch (num) {
					case 2: tmp = "sg"; break;
					case 3: tmp = "bs"; break;
					case 4: tmp = "ng"; break;
					case 5: tmp = "sn"; break;
					case 6: tmp = "gl"; break;
					case 7: tmp = "rl"; break;
					case 8: tmp = "lg"; break;
					default: tmp = "";
				}

				if ((weapon_active && style == 1) || (!weapon_active && style == 3)) {
					Draw_SString(x, y, tmp, scale, proportional);
				}
				else {
					Draw_SAlt_String(x, y, tmp, scale, proportional);
				}
			}
			break;
		case 4: // opposite colors of case 2
		case 2: // numbers, gold inactive, white active
			width = FontFixedWidth(1, scale, true, proportional);;
			height = 8 * scale;
			if (!HUD_PrepareDraw(hud, width, height, &x, &y))
				return;
			if (HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN << i)) {
				if (HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN << i)) {
					num += '0' + (style == 4 ? 128 : 0);
				}
				else {
					num += '0' + (style == 4 ? 0 : 128);
				}
				Draw_SCharacterP(x, y, num, scale, proportional);
			}
			break;
		case 5: // COLOR active, gold inactive
		case 7: // COLOR active, white inactive
		case 6: // white active, COLOR inactive
		case 8: // gold active, COLOR inactive
			width = FontFixedWidth(2, scale, false, proportional);
			height = 8 * scale;

			if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
				return;
			}

			if (HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN << i)) {
				if (HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN << i)) {
					if ((style == 5) || (style == 7)) { // strip {}
						char *weap_str = TP_ItemName((IT_SHOTGUN << i));
						char weap_white_stripped[32];
						Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
						Draw_SString(x, y, weap_white_stripped, scale, false);
					}
					else {
						//Strip both &cRGB and {}
						char inactive_weapon_buf[16];
						char inactive_weapon_buf_nowhite[16];
						Util_SkipEZColors(inactive_weapon_buf, TP_ItemName(IT_SHOTGUN << i), sizeof(inactive_weapon_buf));
						Util_SkipChars(inactive_weapon_buf, "{}", inactive_weapon_buf_nowhite, sizeof(inactive_weapon_buf_nowhite));

						if (style == 8) {
							// gold active
							Draw_SAlt_String(x, y, inactive_weapon_buf_nowhite, scale, proportional);
						}
						else if (style == 6) {
							// white active
							Draw_SString(x, y, inactive_weapon_buf_nowhite, scale, proportional);
						}
					}
				}
				else {
					if ((style == 5) || (style == 7)) { //Strip both &cRGB and {}
						char inactive_weapon_buf[16];
						char inactive_weapon_buf_nowhite[16];
						Util_SkipEZColors(inactive_weapon_buf, TP_ItemName(IT_SHOTGUN << i), sizeof(inactive_weapon_buf));
						Util_SkipChars(inactive_weapon_buf, "{}", inactive_weapon_buf_nowhite, sizeof(inactive_weapon_buf_nowhite));

						if (style == 5) {
							// gold inactive
							Draw_SAlt_String(x, y, inactive_weapon_buf_nowhite, scale, proportional);
						}
						else if (style == 7) {
							// white inactive
							Draw_SString(x, y, inactive_weapon_buf_nowhite, scale, proportional);
						}
					}
					else if ((style == 6) || (style == 8)) { // strip only {}
						char *weap_str = TP_ItemName((IT_SHOTGUN << i));
						char weap_white_stripped[32];
						Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
						Draw_SString(x, y, weap_white_stripped, scale, proportional);
					}
				}
			}
			break;
		default:    // classic - pictures
			width = scale * (wide ? 48 : 24);
			height = scale * 16;

			if (!HUD_PrepareDraw(hud, width, height, &x, &y)) {
				return;
			}

			if (HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN << i)) {
				float   time;
				int     flashon;

				time = cl.item_gettime[i];
				flashon = (int)((cl.time - time) * 10);
				if (flashon < 0) {
					flashon = 0;
				}
				if (flashon >= 10) {
					flashon = (HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN << i)) ? 1 : 0;
				}
				else {
					flashon = (flashon % 5) + 2;
				}

				if (wide || num != 8) {
					Draw_SPic(x, y, sb_weapons[flashon][i], scale);
				}
				else {
					Draw_SSubPic(x, y, sb_weapons[flashon][i], 0, 0, 24, 16, scale);
				}
			}
			break;
	}
}

static void SCR_HUD_DrawGun2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 2, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_SHOTGUN)
		? true
		: false;
}

static void SCR_HUD_DrawGun3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 3, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_SUPER_SHOTGUN)
		? true
		: false;
}

static void SCR_HUD_DrawGun4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 4, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_NAILGUN)
		? true
		: false;
}

static void SCR_HUD_DrawGun5(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 5, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_SUPER_NAILGUN)
		? true
		: false;
}

static void SCR_HUD_DrawGun6(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 6, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_GRENADE_LAUNCHER)
		? true
		: false;
}

static void SCR_HUD_DrawGun7(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 7, scale->value, style->value, 0, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_ROCKET_LAUNCHER)
		? true
		: false;
}

static void SCR_HUD_DrawGun8(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *wide, *proportional, *frame_hide;
	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		wide = HUD_FindVar(hud, "wide");
		proportional = HUD_FindVar(hud, "proportional");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, 8, scale->value, style->value, wide->value, proportional->integer);
	}

	frame_hide = HUD_FindVar(hud, "frame_hide");
	hud->frame_hide = frame_hide->value && !(HUD_Stats(STAT_ITEMS) & IT_LIGHTNING)
		? true
		: false;
}

static void SCR_HUD_DrawGunCurrent(hud_t *hud)
{
	int gun;
	static cvar_t *scale = NULL, *style, *wide, *proportional;

	if (scale == NULL) {
		// first time called
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		wide = HUD_FindVar(hud, "wide");
		proportional = HUD_FindVar(hud, "proportional");
	}

	if (ShowPreselectedWeap()) {
		// using weapon pre-selection so show info for current best pre-selected weapon
		gun = IN_BestWeapon(true);
		if (gun < 2) {
			return;
		}
	}
	else {
		// not using weapon pre-selection or player is dead so show current selected weapon
		switch (HUD_Stats(STAT_ACTIVEWEAPON)) {
			case IT_SHOTGUN << 0:   gun = 2; break;
			case IT_SHOTGUN << 1:   gun = 3; break;
			case IT_SHOTGUN << 2:   gun = 4; break;
			case IT_SHOTGUN << 3:   gun = 5; break;
			case IT_SHOTGUN << 4:   gun = 6; break;
			case IT_SHOTGUN << 5:   gun = 7; break;
			case IT_SHOTGUN << 6:   gun = 8; break;
			default: return;
		}
	}

	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawGunByNum(hud, gun, scale->value, style->value, wide->value, proportional->integer);
	}
}

void Guns_HudInit(void)
{
	// init guns
	HUD_Register("gun", NULL, "Part of your inventory - current weapon.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGunCurrent,
		"0", "ibar", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"wide", "0",
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun2", NULL, "Part of your inventory - shotgun.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun2,
		"1", "ibar", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun3", NULL, "Part of your inventory - super shotgun.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun3,
		"1", "gun2", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun4", NULL, "Part of your inventory - nailgun.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun4,
		"1", "gun3", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun5", NULL, "Part of your inventory - super nailgun.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun5,
		"1", "gun4", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun6", NULL, "Part of your inventory - grenade launcher.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun6,
		"1", "gun5", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun7", NULL, "Part of your inventory - rocket launcher.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun7,
		"1", "gun6", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
	HUD_Register("gun8", NULL, "Part of your inventory - thunderbolt.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun8,
		"1", "gun7", "after", "center", "0", "0", "0", "0 0 0", NULL,
		"wide", "0",
		"style", "0",
		"scale", "1",
		"proportional", "0",
		"frame_hide", "0",
		NULL);
}
