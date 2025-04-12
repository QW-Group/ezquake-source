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
//
// common HUD elements
// like clock etc..
//

#include "quakedef.h"
#include "gl_model.h"
#include "hud.h"
#include "hud_common.h"
#include "fonts.h"

extern cvar_t hud_tp_need;
extern cvar_t tp_need_shells, tp_need_nails, tp_need_rockets, tp_need_cells;

static int TP_TeamFortressEngineerSpanner(void)
{
	if (cl.teamfortress) {
		// MEAG: Fixme!
		char *player_skin = Info_ValueForKey(cl.players[cl.playernum].userinfo, "skin");
		char *model_name = cl.model_precache[CL_WeaponModelForView()->current.modelindex]->name;
		if (player_skin && (strcasecmp(player_skin, "tf_eng") == 0) && model_name && (strcasecmp(model_name, "progs/v_span.mdl") == 0)) {
			return 1;
		}
	}
	return 0;
}

static int State_AmmoNumForWeapon(int weapon)
{	// returns ammo number (shells = 1, nails = 2, rox = 3, cells = 4) for given weapon
	switch (weapon) {
		case 2: case 3: return 1;
		case 4: case 5: return 2;
		case 6: case 7: return 3;
		case 8: return 4;
		default: return 0;
	}
}

static int State_AmmoForWeapon(int weapon)
{	// returns ammo amount for given weapon
	int ammon = State_AmmoNumForWeapon(weapon);

	if (ammon)
		return cl.stats[STAT_SHELLS + ammon - 1];
	else
		return 0;
}

static int TP_IsAmmoLow(int weapon)
{
	int ammo = State_AmmoForWeapon(weapon);
	switch (weapon) {
		case 2:
		case 3:  return ammo <= tp_need_shells.value;
		case 4:
		case 5:  return ammo <= tp_need_nails.value;
		case 6:
		case 7:  return ammo <= tp_need_rockets.value;
		case 8:  return ammo <= tp_need_cells.value;
		default: return 0;
	}
}

int HUD_AmmoLowByWeapon(int weapon)
{
	if (hud_tp_need.value) {
		return TP_IsAmmoLow(weapon);
	}
	else {
		int a;
		switch (weapon) {
			case 2:
			case 3:
				a = STAT_SHELLS; break;
			case 4:
			case 5:
				a = STAT_NAILS; break;
			case 6:
			case 7:
				a = STAT_ROCKETS; break;
			case 8:
				a = STAT_CELLS; break;
			default:
				return false;
		}
		return (HUD_Stats(a) <= 10);
	}
}

static void SCR_HUD_DrawAmmo(
	hud_t *hud, int num, float scale, int style, int digits, char *s_align, qbool proportional, qbool always,
	byte *text_color_low, byte *text_color_normal
)
{
	extern mpic_t sb_ib_ammo[4];
	int value, num_old;
	qbool low;

	num_old = num;
	if (num < 1 || num > 4) {	// draw 'current' ammo, which one is it?
		if (ShowPreselectedWeap()) {
			// using weapon pre-selection so show info for current best pre-selected weapon ammo
			num = State_AmmoNumForWeapon(IN_BestWeapon(true));
		}
		else {
			// not using weapon pre-selection or player is dead so show current selected ammo
			if (HUD_Stats(STAT_ITEMS) & IT_SHELLS) {
				num = 1;
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_NAILS) {
				num = 2;
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS) {
				num = 3;
			}
			else if (HUD_Stats(STAT_ITEMS) & IT_CELLS) {
				num = 4;
			}
			else if (TP_TeamFortressEngineerSpanner()) {
				num = 4;
			}
			else {
				num = 0;
			}
		}
	}

	// 'New HUD' used to just return - instead draw blank space so other objects can be placed
	// If user has specified 'show_always', carry on and show current value of STAT_AMMO
	if (!num && !always) {
		if (style < 2 || style == 3) {
			// use this to calculate sizes, but draw_content is false
			SCR_HUD_DrawNum2(hud, 0, false, scale, style, digits, s_align, proportional, false, text_color_low, text_color_normal);
		}
		else {
			int x_, y;

			// calculate sizes but draw nothing
			HUD_PrepareDraw(hud, 42 * scale, 11 * scale, &x_, &y);
		}
		return;
	}

	low = HUD_AmmoLowByWeapon(num * 2);
	if (num < 1 || num > 4 || (num_old == 0 && (!ShowPreselectedWeap() || cl.standby))) {
		// this check is here to display a feature from KTPRO/KTX where you can see received damage in prewar
		// also we make sure this applies only to 'ammo' element
		// weapon preselection must always use HUD_Stats()
		value = cl.stats[STAT_AMMO];
	}
	else {
		value = HUD_Stats(STAT_SHELLS + num - 1);
	}

	if (style < 2 || style == 3) {
		// simply draw number
		SCR_HUD_DrawNum2(hud, value, style == 3 ? false : low, scale, style, digits, s_align, proportional, true, text_color_low, text_color_normal);
	}
	else {
		// else - draw classic ammo-count box with background
		char buf[8];
		int  x_, y;
		float x;
		text_alignment_t align;

		align = text_align_right;
		switch (tolower(s_align[0])) {
			case 'l':   // 'l'eft
				align = text_align_left; break;
			case 'c':   // 'c'enter
				align = text_align_center; break;
			default:
			case 'r':   // 'r'ight
				align = text_align_right; break;
		}

		scale = max(scale, 0.01);
		if (!HUD_PrepareDraw(hud, 42 * scale, 11 * scale, &x_, &y)) {
			return;
		}

		x = x_;
		if (num >= 1 && num <= sizeof(sb_ib_ammo) / sizeof(sb_ib_ammo[0]) && R_TextureReferenceIsValid(sb_ib_ammo[num - 1].texnum)) {
			Draw_SPic(x, y, &sb_ib_ammo[num - 1], scale);
		}

		snprintf(buf, sizeof(buf), "%d", value);

		// convert to nicer numbers
		buf[0] = buf[0] >= '0' ? 18 + buf[0] - '0' : buf[0];
		buf[1] = buf[1] >= '0' ? 18 + buf[1] - '0' : buf[1];
		buf[2] = buf[2] >= '0' ? 18 + buf[2] - '0' : buf[2];

		Draw_SStringAligned(x, y, buf, scale, 1.0f, proportional, align, x + 30 * scale);
	}
}

void SCR_HUD_DrawAmmoCurrent(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional, *always, *text_color_low, *text_color_normal;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
		always = HUD_FindVar(hud, "show_always");
		text_color_low = HUD_FindInitTextColorVar(hud, "text_color_low");
		text_color_normal = HUD_FindInitTextColorVar(hud, "text_color_normal");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmo(hud, 0, scale->value, style->value, digits->value, align->string,
			proportional->integer, always->integer,
			strlen(text_color_low->string) == 0 ? NULL : text_color_low->color,
			strlen(text_color_normal->string) == 0 ? NULL : text_color_normal->color);
	}
}

void SCR_HUD_DrawAmmo1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional, *text_color_low, *text_color_normal;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
		text_color_low = HUD_FindInitTextColorVar(hud, "text_color_low");
		text_color_normal = HUD_FindInitTextColorVar(hud, "text_color_normal");
	}

	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmo(hud, 1, scale->value, style->value, digits->value, align->string,
			proportional->integer, true,
			strlen(text_color_low->string) == 0 ? NULL : text_color_low->color,
			strlen(text_color_normal->string) == 0 ? NULL : text_color_normal->color);
	}
}

void SCR_HUD_DrawAmmo2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional, *text_color_low, *text_color_normal;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
		text_color_low = HUD_FindInitTextColorVar(hud, "text_color_low");
		text_color_normal = HUD_FindInitTextColorVar(hud, "text_color_normal");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmo(hud, 2, scale->value, style->value, digits->value, align->string,
			proportional->integer, true,
			strlen(text_color_low->string) == 0 ? NULL : text_color_low->color,
			strlen(text_color_normal->string) == 0 ? NULL : text_color_normal->color);
	}
}

void SCR_HUD_DrawAmmo3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional, *text_color_low, *text_color_normal;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
		text_color_low = HUD_FindInitTextColorVar(hud, "text_color_low");
		text_color_normal = HUD_FindInitTextColorVar(hud, "text_color_normal");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmo(hud, 3, scale->value, style->value, digits->value, align->string,
			proportional->integer, true,
			strlen(text_color_low->string) == 0 ? NULL : text_color_low->color,
			strlen(text_color_normal->string) == 0 ? NULL : text_color_normal->color);
	}
}

void SCR_HUD_DrawAmmo4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style, *digits, *align, *proportional, *text_color_low, *text_color_normal;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
		digits = HUD_FindVar(hud, "digits");
		align = HUD_FindVar(hud, "align");
		proportional = HUD_FindVar(hud, "proportional");
		text_color_low = HUD_FindInitTextColorVar(hud, "text_color_low");
		text_color_normal = HUD_FindInitTextColorVar(hud, "text_color_normal");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmo(hud, 4, scale->value, style->value, digits->value, align->string,
			proportional->integer, true,
			strlen(text_color_low->string) == 0 ? NULL : text_color_low->color,
			strlen(text_color_normal->string) == 0 ? NULL : text_color_normal->color);
	}
}

// icons - active ammo, armor, face etc..
void SCR_HUD_DrawAmmoIcon(hud_t *hud, int num, float scale, int style)
{
	extern mpic_t *sb_ammo[4];
	int   x, y, width, height;

	scale = max(scale, 0.01);

	width = height = (style ? 8 : 24) * scale;

	if (cl.spectator == cl.autocam) {
		if (!HUD_PrepareDraw(hud, width, height, &x, &y) || num == 0)
			return;

		if (style) {
			switch (num) {
				case 1: Draw_SAlt_String(x, y, "s", scale, false); break;
				case 2: Draw_SAlt_String(x, y, "n", scale, false); break;
				case 3: Draw_SAlt_String(x, y, "r", scale, false); break;
				case 4: Draw_SAlt_String(x, y, "c", scale, false); break;
			}
		}
		else {
			Draw_SPic(x, y, sb_ammo[num - 1], scale);
		}
	}
}

void SCR_HUD_DrawAmmoIconCurrent(hud_t *hud)
{
	int num;
	static cvar_t *scale = NULL, *style;

	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}

	if (ShowPreselectedWeap()) {
		// using weapon pre-selection so show info for current best pre-selected weapon ammo
		num = State_AmmoNumForWeapon(IN_BestWeapon(true));
	}
	else {
		// not using weapon pre-selection or player is dead so show current selected ammo
		if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
			num = 1;
		else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
			num = 2;
		else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
			num = 3;
		else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
			num = 4;
		else if (TP_TeamFortressEngineerSpanner())
			num = 4;
		else
			num = 0;
	}

	SCR_HUD_DrawAmmoIcon(hud, num, scale->value, style->value);
}

void SCR_HUD_DrawAmmoIcon1(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmoIcon(hud, 1, scale->value, style->value);
	}
}

void SCR_HUD_DrawAmmoIcon2(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmoIcon(hud, 2, scale->value, style->value);
	}
}

void SCR_HUD_DrawAmmoIcon3(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmoIcon(hud, 3, scale->value, style->value);
	}
}

void SCR_HUD_DrawAmmoIcon4(hud_t *hud)
{
	static cvar_t *scale = NULL, *style;
	if (scale == NULL)  // first time called
	{
		scale = HUD_FindVar(hud, "scale");
		style = HUD_FindVar(hud, "style");
	}
	if (cl.spectator == cl.autocam) {
		SCR_HUD_DrawAmmoIcon(hud, 4, scale->value, style->value);
	}
}

void Ammo_HudInit(void)
{
	// ammo/s
	HUD_Register(
		"ammo", NULL, "Part of your inventory - ammo for active weapon.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoCurrent,
		"1", "health", "after", "center", "32", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"align", "right",
		"digits", "3",
		"proportional", "0",
		"show_always", "0",
		"text_color_low", "",
		"text_color_normal", "",
		 NULL
	);

	HUD_Register("ammo1", NULL, "Part of your inventory - ammo - shells.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo1,
				 "0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "0",
				 "scale", "1",
				 "align", "right",
				 "digits", "3",
				 "proportional", "0",
				 "text_color_low", "",
				 "text_color_normal", "",
				 NULL);
	HUD_Register("ammo2", NULL, "Part of your inventory - ammo - nails.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo2,
				 "0", "ammo1", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "0",
				 "scale", "1",
				 "align", "right",
				 "digits", "3",
				 "proportional", "0",
				 "text_color_low", "",
				 "text_color_normal", "",
				 NULL);
	HUD_Register("ammo3", NULL, "Part of your inventory - ammo - rockets.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo3,
				 "0", "ammo2", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "0",
				 "scale", "1",
				 "align", "right",
				 "digits", "3",
				 "proportional", "0",
				 "text_color_low", "",
				 "text_color_normal", "",
				 NULL);
	HUD_Register("ammo4", NULL, "Part of your inventory - ammo - cells.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo4,
				 "0", "ammo3", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "0",
				 "scale", "1",
				 "align", "right",
				 "digits", "3",
				 "proportional", "0",
				 "text_color_low", "",
				 "text_color_normal", "",
				 NULL);

	// ammo icon/s
	HUD_Register(
		"iammo", NULL, "Part of your inventory - ammo icon.",
		HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIconCurrent,
		"1", "ammo", "before", "center", "0", "0", "0", "0 0 0", NULL,
		"style", "0",
		"scale", "1",
		"show_always", "0",
		NULL
	);

	HUD_Register("iammo1", NULL, "Part of your inventory - ammo icon.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon1,
				 "0", "ibar", "left", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "2",
				 "scale", "1",
				 NULL);
	HUD_Register("iammo2", NULL, "Part of your inventory - ammo icon.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon2,
				 "0", "iammo1", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "2",
				 "scale", "1",
				 NULL);
	HUD_Register("iammo3", NULL, "Part of your inventory - ammo icon.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon3,
				 "0", "iammo2", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "2",
				 "scale", "1",
				 NULL);
	HUD_Register("iammo4", NULL, "Part of your inventory - ammo icon.",
				 HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon4,
				 "0", "iammo3", "after", "top", "0", "0", "0", "0 0 0", NULL,
				 "style", "2",
				 "scale", "1",
				 NULL);
}