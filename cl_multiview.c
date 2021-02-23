/*
Copyright (C) 1996-1997 Id Software, Inc.

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
*/

// Multiview code
#include "quakedef.h"
#include "utils.h"          // Reg expressions
#include "draw.h"           // RGBA macros
#include "teamplay.h"       // FPD_NO_FORCE_COLOR

// meag: for switching back to 3d resolution to draw the multiview outlines
#include "r_matrix.h"
#include "mvd_utils.h"

extern int glx, gly, glwidth, glheight;

#define	MV_VIEW1 0
#define	MV_VIEW2 1
#define	MV_VIEW3 2
#define	MV_VIEW4 3

static int      CURRVIEW;					// The current view being drawn in multiview mode.
static qbool    bExitmultiview;				// Used when saving effect values on each frame.
static int      nNumViews;					// The number of views in multiview mode.
static int      nPlayernum;
static int      mv_trackslots[MV_VIEWS];    // The different track slots for each view.
static char     currteam[MAX_INFO_STRING];	// The name of the current team being tracked in multiview mode.
static int      nSwapPov;					// Change in POV positive for next, negative for previous.
static int      nTrack1duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the main view.
static int      nTrack2duel;				// When cl_multiview = 2 and mvinset is on this is the tracking slot for the mvinset view.

// temporary storage (essentially caching OpenGL viewport state, can get rid of this in 3.5)
static int inset_x;
static int inset_y;
static int inset_width;
static int inset_height;

// Temporary storage so we keep same settings during demo rewind
static int rewind_trackslots[4];
static int rewind_duel_track1 = 0;
static int rewind_duel_track2 = 0;

static int next_spec_track = -1;
extern cvar_t gl_polyblend;
extern cvar_t gl_clear;

extern cvar_t scr_autoid_healthbar_bg_color;
extern cvar_t scr_autoid_healthbar_normal_color;
extern cvar_t scr_autoid_healthbar_mega_color;
extern cvar_t scr_autoid_healthbar_two_mega_color;
extern cvar_t scr_autoid_healthbar_unnatural_color;

extern cvar_t scr_autoid_armorbar_green_armor;
extern cvar_t scr_autoid_armorbar_yellow_armor;
extern cvar_t scr_autoid_armorbar_red_armor;

extern cvar_t cl_multiview_inset_offset_x, cl_multiview_inset_offset_y;
extern cvar_t cl_multiview_inset_size_x, cl_multiview_inset_size_y;
extern cvar_t cl_multiview_inset_top, cl_multiview_inset_right;

#define ALPHA_COLOR(x, y) RGBA_TO_COLOR((x)[0],(x)[1],(x)[2],(y))

typedef struct mv_viewrect_s
{
	int x;
	int y;
	int width;
	int height;
} mv_viewrect_t;

#define MV_HUD_POS_BOTTOM_CENTER	1
#define MV_HUD_POS_BOTTOM_LEFT		2
#define MV_HUD_POS_BOTTOM_RIGHT		3
#define MV_HUD_POS_TOP_CENTER		4
#define MV_HUD_POS_TOP_LEFT			5
#define MV_HUD_POS_TOP_RIGHT		6
#define MV_HUD_POS_GATHERED_CENTER	7

#define MV_HUDPOS(regexp, input, position, cvar) if(Utils_RegExpMatch(regexp, input)) { mv_hudpos = position; found = true; }

// true if the player in given slot is an existing player
#define VALID_PLAYER(i) (cl.players[i].name[0] && !cl.players[i].spectator)

int mv_hudpos = MV_HUD_POS_BOTTOM_CENTER;

mpic_t* SCR_GetWeaponIconByFlag (int flag);
void R_TranslatePlayerSkin (int playernum);
void CL_Track (int trackview);
static void CL_MultiviewOverrideValues (void);

static int CL_IncrLoop(int cview, int max)
{
	return (cview >= max) ? 1 : ++cview;
}

int CL_NextPlayer(int plr)
{
	do
	{
		plr++;
	} while ((plr < MAX_CLIENTS) && (cl.players[plr].spectator || !cl.players[plr].name[0]));

	if (plr >= MAX_CLIENTS)
	{
		plr = -1;

		do
		{
			plr++;
		} while ((plr < MAX_CLIENTS) && (cl.players[plr].spectator || !cl.players[plr].name[0]));
	}

	if (plr >= MAX_CLIENTS)
	{
		plr = 0;
	}

	return plr;
}

int CL_PrevPlayer(int plr)
{
	do
	{
		plr--;
	} while ((plr >= 0) && (cl.players[plr].spectator || !cl.players[plr].name[0]));

	if (plr < 0)
	{
		plr = MAX_CLIENTS;

		do
		{
			plr--;
		} while ((plr >= 0) && (cl.players[plr].spectator || !cl.players[plr].name[0]));
	}

	if (plr < 0)
	{
		plr = 0;
	}

	return plr;
}

void SCR_OnChangeMVHudPos (cvar_t *var, char *newval, qbool *cancel)
{
	qbool found = false;

	MV_HUDPOS ("(top\\s*left|tl)", newval, MV_HUD_POS_TOP_LEFT, var);
	MV_HUDPOS ("(top\\s*right|tr)", newval, MV_HUD_POS_TOP_RIGHT, var);
	MV_HUDPOS ("(top\\s*center|tc)", newval, MV_HUD_POS_TOP_CENTER, var);
	MV_HUDPOS ("(bottom\\s*left|bl)", newval, MV_HUD_POS_BOTTOM_LEFT, var);
	MV_HUDPOS ("(bottom\\s*right|br)", newval, MV_HUD_POS_BOTTOM_RIGHT, var);
	MV_HUDPOS ("(bottom\\s*center|bc)", newval, MV_HUD_POS_BOTTOM_CENTER, var);
	MV_HUDPOS ("(gather|g)", newval, MV_HUD_POS_GATHERED_CENTER, var);

	if (found) {
		Cvar_Set (var, newval);
	}

	*cancel = found;
}

// SCR_SetMVStatusPosition calls SCR_SetMVStatusGatheredPosition and vice versa.
void SCR_SetMVStatusGatheredPosition (mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y);

void SCR_SetMVStatusPosition (int position, mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y)
{
	switch (position) {
	case MV_HUD_POS_BOTTOM_LEFT:
	{
		(*x) = 0;
		(*y) = max (0, (view->height - hud_height));
		break;
	}
	case MV_HUD_POS_BOTTOM_RIGHT:
	{
		(*x) = max (0, (view->width - hud_width));
		(*y) = max (0, (view->height - hud_height));
		break;
	}
	case MV_HUD_POS_TOP_CENTER:
	{
		(*x) = max (0, (view->width - hud_width) / 2);
		(*y) = 0;
		break;
	}
	case MV_HUD_POS_TOP_LEFT:
	{
		(*x) = 0;
		(*y) = 0;
		break;
	}
	case MV_HUD_POS_TOP_RIGHT:
	{
		(*x) = max (0, view->width - hud_width);
		(*y) = 0;
		break;
	}
	case MV_HUD_POS_GATHERED_CENTER:
	{
		SCR_SetMVStatusGatheredPosition (view, hud_width, hud_height, &(*x), &(*y));
		break;
	}
	case MV_HUD_POS_BOTTOM_CENTER:
	default:
	{
		(*x) = max (0, (view->width - hud_width) / 2);
		(*y) = max (0, (view->height - hud_height));
		break;
	}
	}
}

void SCR_SetMVStatusGatheredPosition (mv_viewrect_t *view, int hud_width, int hud_height, int *x, int *y)
{
	//
	// Position the huds towards the center of the screen for the different views
	// so that all the HUD's are close to each other on the screen.
	//

	if (cl_multiview.value == 2) {
		if (!cl_mvinset.value) {
			if (CURRVIEW == 2) {
				// Top view. (Put the hud at the bottom)
				SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_CENTER, view, hud_width, hud_height, x, y);
			}
			else if (CURRVIEW == 1) {
				// Bottom view. (Put the hud at the top)
				SCR_SetMVStatusPosition (MV_HUD_POS_TOP_CENTER, view, hud_width, hud_height, x, y);
			}
		}
	}
	else if (cl_multiview.value == 3) {
		if (CURRVIEW == 2) {
			// Top view. (Put the hud at the bottom)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_CENTER, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 3) {
			// Bottom left view. (Put the hud at the top right)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 1) {
			// Bottom right view. (Put the hud at the top left)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_LEFT, view, hud_width, hud_height, x, y);
		}
	}
	else if (cl_multiview.value == 4) {
		if (CURRVIEW == 2) {
			// Top left view. (Put the hud at the bottom right)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 3) {
			// Top right view. (Put the hud at the bottom left)
			SCR_SetMVStatusPosition (MV_HUD_POS_BOTTOM_LEFT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 4) {
			// Bottom left view. (Put the hud at the top right)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_RIGHT, view, hud_width, hud_height, x, y);
		}
		else if (CURRVIEW == 1) {
			// Bottom right view. (Put the hud at the top left)
			SCR_SetMVStatusPosition (MV_HUD_POS_TOP_LEFT, view, hud_width, hud_height, x, y);
		}
	}
}

#define MV_HUD_ARMOR_WIDTH		(5*8)
#define MV_HUD_HEALTH_WIDTH		(5*8)
#define MV_HUD_CURRAMMO_WIDTH	(5*8)
#define MV_HUD_CURRWEAP_WIDTH	(3*8)
#define MV_HUD_POWERUPS_WIDTH	(2*8)
#define MV_HUD_POWERUPS_HEIGHT	(2*8)

#define MV_HUD_STYLE_ONLY_NAME	2
#define MV_HUD_STYLE_ALL		3
#define MV_HUD_STYLE_ALL_TEXT	4
#define MV_HUD_STYLE_ALL_FILL	5

void SCR_MV_SetBoundValue (int *var, int val)
{
	if (var != NULL) {
		(*var) = val;
	}
}

void SCR_MV_DrawName (int x, int y, int *width, int *height)
{
	char *name = cl.players[nPlayernum].name;
	SCR_MV_SetBoundValue (width, 8 * (strlen (name) + 1));
	SCR_MV_SetBoundValue (height, 8);

	Draw_String (x, y, name);
}

void SCR_MV_DrawArmor (int x, int y, int *width, int *height, int style)
{
	char armor_color_code[6] = "";
	int armor_amount_width = 0;

	byte armor_color[4] = { 0, 0, 0, 75 };	// RGBA.

											//
											// Set the armor text color based on what armor the player has.
											//
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1) {
		// Green armor.
		strlcpy (armor_color_code, "&c0f0", sizeof (armor_color_code));
		armor_amount_width = Q_rint (MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 100.0);

		armor_color[0] = 80;
		armor_color[1] = 190;
		armor_color[2] = 80;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2) {
		// Yellow armor.
		strlcpy (armor_color_code, "&cff0", sizeof (armor_color_code));
		armor_amount_width = Q_rint (MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 150.0);

		armor_color[0] = 250;
		armor_color[1] = 230;
		armor_color[2] = 0;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3) {
		// Red armor.
		strlcpy (armor_color_code, "&cf00", sizeof (armor_color_code));
		armor_amount_width = Q_rint (MV_HUD_ARMOR_WIDTH * cl.stats[STAT_ARMOR] / 200.0);

		armor_color[0] = 190;
		armor_color[1] = 50;
		armor_color[2] = 0;
	}

	if (cl.stats[STAT_ARMOR] > 0) {
		//
		// Background fill for armor.
		//
		Draw_AlphaFillRGB (x, y, armor_amount_width, 8, RGBA_TO_COLOR (armor_color[0], armor_color[1], armor_color[2], armor_color[3]));

		// Armor value.
		if (style >= MV_HUD_STYLE_ALL_TEXT) {
			Draw_ColoredString (x, y, va ("%s%4d", armor_color_code, cl.stats[STAT_ARMOR]), 0, false);
		}
	}

	SCR_MV_SetBoundValue (width, MV_HUD_ARMOR_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawHealth (int x, int y, int *width, int *height, int style)
{
#define MV_HEALTH_OPACITY 75
	int health = cl.stats[STAT_HEALTH];

	int health_amount_width = 0;

	health = min (100, health);
	health_amount_width = Q_rint (fabs((MV_HUD_HEALTH_WIDTH * health) / 100.0f));

	if (health > 0) {
		Draw_AlphaFillRGB (x, y, health_amount_width, 8, ALPHA_COLOR (scr_autoid_healthbar_normal_color.color, 2 * MV_HEALTH_OPACITY));
	}

	health = cl.stats[STAT_HEALTH];

	if (health > 100 && health <= 200) {
		// Mega health.
		health_amount_width = Q_rint ((MV_HUD_HEALTH_WIDTH / 100.0f) * (health - 100));

		Draw_AlphaFillRGB (x, y, health_amount_width, 8, ALPHA_COLOR (scr_autoid_healthbar_mega_color.color, MV_HEALTH_OPACITY));
	}
	else if (health > 200 && health <= 250) {
		// Super health.
		health_amount_width = Q_rint ((MV_HUD_HEALTH_WIDTH / 100.0f) * (health - 200));

		Draw_AlphaFillRGB (x, y, MV_HUD_HEALTH_WIDTH, 8, ALPHA_COLOR (scr_autoid_healthbar_mega_color.color, MV_HEALTH_OPACITY));
		Draw_AlphaFillRGB (x, y, health_amount_width, 8, ALPHA_COLOR (scr_autoid_healthbar_two_mega_color.color, MV_HEALTH_OPACITY));
	}
	else if (health > 250) {
		// Crazy health.
		Draw_AlphaFillRGB (x, y, MV_HUD_HEALTH_WIDTH, 8, ALPHA_COLOR (scr_autoid_healthbar_unnatural_color.color, MV_HEALTH_OPACITY));

	}

	// No powerup.
	if (style >= MV_HUD_STYLE_ALL_TEXT && !(cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)) {
		Draw_String (x, y, va ("%4d", health));
	}

	SCR_MV_SetBoundValue (width, MV_HUD_HEALTH_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawPowerups (int x, int y)
{
	extern mpic_t  *sb_face_invis;
	extern mpic_t  *sb_face_quad;
	extern mpic_t  *sb_face_invuln;
	extern mpic_t  *sb_face_invis_invuln;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY
		&& cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		// Pentagram + Ring.
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invis_invuln->width) / 2,
			y - sb_face_invis_invuln->height / 2,
			sb_face_invis_invuln, 0.4);
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		// Pentagram.
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invuln->width) / 2,
			y - sb_face_invuln->height / 2,
			sb_face_invuln, 0.4);
	}
	else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		// Ring.
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_invis->width) / 2,
			y - sb_face_invis->height / 2,
			sb_face_invis, 0.4);
	}

	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		// Ring.
		Draw_AlphaPic (x + (MV_HUD_HEALTH_WIDTH - sb_face_quad->width) / 2,
			y - sb_face_quad->height / 2,
			sb_face_quad, 0.4);
	}
}

static mpic_t *SCR_GetActiveWeaponIcon (void)
{
	return SCR_GetWeaponIconByFlag (cl.stats[STAT_ACTIVEWEAPON]);
}

static void SCR_MV_DrawCurrentWeapon (int x, int y, int *width, int *height)
{
	mpic_t *current_weapon = NULL;
	current_weapon = SCR_GetActiveWeaponIcon ();

	if (current_weapon) {
		Draw_Pic (x,
			y - (current_weapon->height / 4),
			current_weapon);
	}

	SCR_MV_SetBoundValue (width, MV_HUD_CURRWEAP_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

void SCR_MV_DrawCurrentAmmo (int x, int y, int *width, int *height)
{
	// Draw the ammo count in blue/greyish color.
	Draw_ColoredString (x, y, va ("&c5CE%4d", cl.stats[STAT_AMMO]), 0, false);
	SCR_MV_SetBoundValue (width, MV_HUD_CURRAMMO_WIDTH);
	SCR_MV_SetBoundValue (height, 8);
}

mpic_t *SCR_GetWeaponIconByWeaponNumber (int num)
{
	extern mpic_t *sb_weapons[7][8];  // sbar.c Weapon pictures.

	num -= 2;

	if (num >= 0 && num < 8) {
		return sb_weapons[0][num];
	}

	return NULL;
}

void SCR_MV_DrawWeapons (int x, int y, int *width, int *height, int hud_width, int hud_height, qbool vertical)
{
#define WEAPON_COUNT 8
	mpic_t *weapon_pic = NULL;
	int weapon = 0;
	int weapon_flag = 0;
	int weapon_x = 0;
	int weapon_y = 0;

	// Draw the weapons the user has.
	for (weapon = 0; weapon < WEAPON_COUNT; weapon++) {
		weapon_flag = IT_SHOTGUN << weapon;

		if (cl.stats[STAT_ITEMS] & weapon_flag) {
			// Get the weapon picture and draw it.
			weapon_pic = SCR_GetWeaponIconByWeaponNumber (weapon + 2);
			Draw_Pic (x + weapon_x, y + weapon_y, weapon_pic);
		}

		// Evenly distribute the weapons.
		if (!vertical) {
			weapon_x += Q_rint ((float)hud_width / WEAPON_COUNT);
		}
		else {
			weapon_y += Q_rint ((float)hud_height / WEAPON_COUNT);
		}
	}
}

void SCR_DrawMVStatusView (mv_viewrect_t *view, int style, int position, qbool flip, qbool vertical)
{
	int hud_x = 0;
	int hud_y = 0;
	int hud_width = 0;
	int hud_height = 0;

	char *name = cl.players[nPlayernum].name;

	if (style == MV_HUD_STYLE_ONLY_NAME) {
		// Only draw the players name.

		hud_height = 2 * 8;
		hud_width = 8 * (strlen (name) + 1);

		//
		// Get the position we should draw the hud at.
		//
		SCR_SetMVStatusPosition (position, view, hud_width, hud_height, &hud_x, &hud_y);

		Draw_String (view->x + hud_x, view->y + hud_y, name);
	}
	else if (style >= MV_HUD_STYLE_ALL) {
#define MV_HUD_VERTICAL_GAP	4

		int name_width = 0;
		int name_height = 0;
		int armor_width = 0;
		int armor_height = 0;
		int health_width = 0;
		int health_height = 0;
		int currweap_width = 0;
		int currweap_height = 0;
		int currammo_width = 0;
		int currammo_height = 0;

		//
		// Calculate the total width and height of the hud.
		//
		if (!vertical) {
			hud_height = 3 * 8;
			hud_width =
				8 * (strlen (name)) +				// Name.
				MV_HUD_ARMOR_WIDTH + 				// Armor + space.
				MV_HUD_HEALTH_WIDTH + 				// Health.
				MV_HUD_CURRWEAP_WIDTH +				// Current weapon.
				MV_HUD_CURRAMMO_WIDTH;				// Current weapon ammo count.

		}
		else {
			// Vertical.
			hud_height = 5 * (8 + MV_HUD_VERTICAL_GAP);
			hud_width = max (8 * strlen (name), MV_HUD_ARMOR_WIDTH) + MV_HUD_ARMOR_WIDTH;
		}

		//
		// Get the position we should draw the hud at.
		//
		SCR_SetMVStatusPosition (position, view, hud_width, hud_height, &hud_x, &hud_y);

		//
		// Draw a fill background.
		//
		if (style >= MV_HUD_STYLE_ALL_FILL) {
			Draw_AlphaFill (view->x + hud_x, view->y + hud_y, hud_width, hud_height, 0, 0.5);
		}

		// Draw powerups in the middle background of the hud.
		SCR_MV_DrawPowerups (view->x + hud_x + (hud_width / 2), view->y + hud_y + (hud_height / 2));

		// Draw the elements vertically? (Add a small gap between the items when
		// drawing them vertically, otherwise they're too close together).
#define MV_FLIP(W,H) if(vertical) { hud_y += (H) + MV_HUD_VERTICAL_GAP; } else { hud_x += (W); }

		if (!flip) {
			// Name.
			SCR_MV_DrawName (view->x + hud_x, view->y + hud_y, &name_width, &name_height);
			MV_FLIP (name_width, name_height);

			// Armor.
			SCR_MV_DrawArmor (view->x + hud_x, view->y + hud_y, &armor_width, &armor_height, style);
			MV_FLIP (armor_width, armor_height);

			// Health.
			SCR_MV_DrawHealth (view->x + hud_x, view->y + hud_y, &health_width, &health_height, style);
			MV_FLIP (health_width, health_height);

			// Current weapon.
			SCR_MV_DrawCurrentWeapon (view->x + hud_x, view->y + hud_y, &currweap_width, &currweap_height);
			MV_FLIP (currweap_width, currweap_height);

			// Ammo for current weapon.
			SCR_MV_DrawCurrentAmmo (view->x + hud_x, view->y + hud_y, &currammo_width, &currammo_height);
			MV_FLIP (currammo_width, currammo_height);
		}
		else {
			//
			// Flipped horizontally.
			//

			// Ammo for current weapon.
			SCR_MV_DrawCurrentAmmo (view->x + hud_x, view->y + hud_y, &currammo_width, &currammo_height);
			MV_FLIP (currammo_width, currammo_height);

			// Current weapon.
			SCR_MV_DrawCurrentWeapon (view->x + hud_x, view->y + hud_y, &currweap_width, &currweap_height);
			MV_FLIP (currweap_width, currweap_height);

			// Health.
			SCR_MV_DrawHealth (view->x + hud_x, view->y + hud_y, &health_width, &health_height, style);
			MV_FLIP (health_width, health_height);

			// Armor.
			SCR_MV_DrawArmor (view->x + hud_x, view->y + hud_y, &armor_width, &armor_height, style);
			MV_FLIP (armor_width, armor_height);

			// Name.
			SCR_MV_DrawName (view->x + hud_x, view->y + hud_y, &name_width, &name_height);
			MV_FLIP (name_width, name_height);
		}

		if (vertical) {
			// Start in the next column.
			hud_x += max (8 * strlen (name), MV_HUD_ARMOR_WIDTH);
			hud_y -= hud_height;
		}
		else {
			// Start on the next row.
			hud_x -= hud_width;
			hud_y += hud_height / 2;
		}

		// Weapons.
		SCR_MV_DrawWeapons (view->x + hud_x, view->y + hud_y, NULL, NULL, hud_width, hud_height, vertical);
	}
}

void SCR_SetMVStatusTwoViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2) {
		// Top.
		view->x = 0;
		view->y = 0;
		view->width = vid.width;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 1) {
		// Bottom.
		view->x = 0;
		view->y = vid.height / 2;
		view->width = vid.width;
		view->height = vid.height / 2;
	}
}

void SCR_SetMVStatusTwoInsetViewRect(mv_viewrect_t *view)
{
	if (CURRVIEW == 2) {
		// Main.
		view->x = 0;
		view->y = 0;
		view->width = vid.width;
		view->height = vid.height;
	}
	else if (CURRVIEW == 1) {
		float ratio_x = (vid.width * 1.0f) / glwidth;
		float ratio_y = (vid.height * 1.0f) / glheight;

		// inset window
		view->x = inset_x * ratio_x;
		view->y = (glheight - (inset_y + inset_height)) * ratio_y; // reversed for 2d/3d rendering
		view->width = ceil(inset_width * ratio_x);
		view->height = ceil(inset_height * ratio_y);
	}
}

void SCR_SetMVStatusThreeViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2) {
		// Top.
		view->x = 0;
		view->y = 0;
		view->width = vid.width;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 3) {
		// Bottom left.
		view->x = 0;
		view->y = vid.height / 2;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 1) {
		// Bottom right.
		view->x = vid.width / 2;
		view->y = vid.height / 2;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
}

void SCR_SetMVStatusFourViewRect (mv_viewrect_t *view)
{
	if (CURRVIEW == 2) {
		// Top left.
		view->x = 0;
		view->y = 0;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 3) {
		// Top right.
		view->x = vid.width / 2;
		view->y = 0;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 4) {
		// Bottom left.
		view->x = 0;
		view->y = vid.height / 2;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
	else if (CURRVIEW == 1) {
		// Bottom right.
		view->x = vid.width / 2;
		view->y = vid.height / 2;
		view->width = vid.width / 2;
		view->height = vid.height / 2;
	}
}

void SCR_DrawMVStatus (void)
{
	mv_viewrect_t view;

	// Only draw mini hud when there are more than 1 views.
	if (cl_multiview.value <= 1 || !cls.mvdplayback) {
		return;
	}

	// Reset the view.
	memset (&view, -1, sizeof (view));

	//
	// Get the view rect to draw the hud within based on the current
	// multiview mode, and what view that is being drawn.
	//
	if (cl_multiview.value == 2) {
		if (cl_mvinset.value) {
			// Only draw the mini hud for the inset, 
			// since we probably want the full size hud 
			// for the main view.
			if (CURRVIEW == 2) {
				return;
			}

			SCR_SetMVStatusTwoInsetViewRect(&view);
		}
		else {
			SCR_SetMVStatusTwoViewRect (&view);
		}
	}
	else if (cl_multiview.value == 3) {
		SCR_SetMVStatusThreeViewRect (&view);
	}
	else if (cl_multiview.value == 4) {
		SCR_SetMVStatusFourViewRect (&view);
	}

	// Only draw if we the view rect was set properly.
	if (view.x != -1) {
		SCR_DrawMVStatusView (&view,
			cl_mvdisplayhud.integer,
			mv_hudpos,
			(qbool)cl_mvhudflip.value,
			(qbool)cl_mvhudvertical.value);
	}
}

void SCR_DrawMVStatusStrings (void)
{
	int xb = 0, yb = 0, xd = 0, yd = 0;
	char strng[80];
	char weapons[40];
	char weapon[3];
	char sAmmo[3];
	char pups[4];
	char armor;
	char name[16];

	int i;

	// Only in MVD.
	if (!cl_multiview.value || !cls.mvdplayback) {
		return;
	}

	//
	// Get the current weapon.
	//
	if (cl.stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING || cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_LIGHTNING) {
		strlcpy (weapon, "lg", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER) {
		strlcpy (weapon, "rl", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER) {
		strlcpy (weapon, "gl", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN) {
		strlcpy (weapon, "sn", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_NAILGUN) {
		strlcpy (weapon, "ng", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN) {
		strlcpy (weapon, "ss", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_SHOTGUN) {
		strlcpy (weapon, "sg", sizeof (weapon));
	}
	else if (cl.stats[STAT_ACTIVEWEAPON] & IT_AXE) {
		strlcpy (weapon, "ax", sizeof (weapon));
	}
	else {
		strlcpy (weapon, "??", sizeof (weapon));
	}
	weapon[0] |= 128;
	weapon[1] |= 128;

	//
	// Get current powerups.
	//
	pups[0] = pups[1] = pups[2] = ' ';
	pups[3] = '\0';

	if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		pups[0] = 'Q';
		pups[0] |= 128;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		pups[1] = 'R';
		pups[1] |= 128;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		pups[2] = 'P';
		pups[2] |= 128;
	}

	strng[0] = '\0';

	for (i = 0; i < 8; i++) {
		weapons[i] = ' ';
		weapons[8] = '\0';
	}

	if (cl.stats[STAT_ITEMS] & IT_AXE) {
		weapons[0] = '1' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN) {
		weapons[1] = '2' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) {
		weapons[2] = '3' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN) {
		weapons[3] = '4' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN) {
		weapons[4] = '5' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) {
		weapons[5] = '6' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) {
		weapons[6] = '7' - '0' + 0x12;
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_LIGHTNING || cl.stats[STAT_ITEMS] & IT_LIGHTNING) {
		weapons[7] = '8' - '0' + 0x12;
	}

	armor = ' ';
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1) {
		armor = 'g';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2) {
		armor = 'y';
		armor |= 128;
	}
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3) {
		armor = 'r';
		armor |= 128;
	}

	//
	// Get the player's name.
	//
	strlcpy (name, cl.players[nPlayernum].name, sizeof (name));

	if (strcmp (cl.players[nPlayernum].name, "") && !cl.players[nPlayernum].spectator) {
		if ((cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) && (cl_multiview.value == 2) && cl_mvinset.integer) {
			// mvinset and dead
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf (strng, sizeof (strng), "%.5s   %s %s:%-3s", name,
				"dead   ",
				weapon,
				sAmmo);
		}
		else if ((cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) && (vid.width <= 400)) {
			// Resolution width <= 400 and dead
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf (strng, sizeof (strng), "%.4s  %s %s:%-3s", name,
				"dead   ",
				weapon,
				sAmmo);
		}
		else if (cl.players[nPlayernum].stats[STAT_HEALTH] <= 0) {
			// > 512 and dead
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf (strng, sizeof (strng), "%s   %s %s:%-3s", name,
				"dead   ",
				weapon,
				sAmmo);
		}

		else if ((cl_multiview.integer == 2) && cl_mvinset.integer && (CURRVIEW == 1)) {
			// mvinset
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf (strng, sizeof (strng), "%s %.5s  %c%03d %03d %s:%-3s", pups,
				name,
				armor,
				cl.players[nPlayernum].stats[STAT_ARMOR],
				cl.players[nPlayernum].stats[STAT_HEALTH],
				weapon,
				sAmmo);
		}
		else if (cl_multiview.value && vid.width <= 400) {
			// <= 400 and alive
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]);
			snprintf (strng, sizeof (strng), "%s %.4s %c%03d %03d %s:%-3s", pups,
				name,
				armor,
				cl.players[nPlayernum].stats[STAT_ARMOR],
				cl.players[nPlayernum].stats[STAT_HEALTH],
				weapon,
				sAmmo);
		}
		else {
			snprintf (sAmmo, sizeof (sAmmo), "%02d", cl.players[nPlayernum].stats[STAT_AMMO]); // > 512 and alive
			snprintf (strng, sizeof (strng), "%s %s  %c%03d %03d %s:%-3s", pups,
				name,
				armor,
				cl.players[nPlayernum].stats[STAT_ARMOR],
				cl.players[nPlayernum].stats[STAT_HEALTH],
				weapon,
				sAmmo);
		}
	}

	//
	// Powerup cam stuff: (don't display links if we're using fixed camera position)
	//
	if (MVD_PowerupCam_Enabled()) {
		sAmmo[0] = strng[0] = weapons[0] = '\0';
	}

	//
	// Placement.
	//
	if (cl_multiview.value == 1) {
		xb = vid.width - strlen (strng) * 8 - 12;
		yb = vid.height - sb_lines - 16;
		xd = vid.width - strlen (weapons) * 8 - 84;
		yd = vid.height - sb_lines - 8;
	}
	else if (cl_multiview.value == 2) {
		if (!cl_mvinset.value) {
			if (CURRVIEW == 2) {
				// Top
				xb = vid.width - strlen (strng) * 8 - 12;
				yb = vid.height / 2 - sb_lines - 16;
				xd = vid.width - strlen (weapons) * 8 - 84;
				yd = vid.height / 2 - sb_lines - 8;
			}
			else if (CURRVIEW == 1) {
				// Bottom
				xb = vid.width - strlen (strng) * 8 - 12;
				yb = vid.height - sb_lines - 16;
				xd = vid.width - strlen (weapons) * 8 - 84;
				yd = vid.height - sb_lines - 8;
			}
		}
		else if (cl_mvinset.value) {
			if (CURRVIEW == 2) {
				// Main
				xb = vid.width - strlen (strng) * 8 - 12;
				yb = vid.height / 2 - sb_lines - 16;
				xd = vid.width - strlen (weapons) * 8 - 84;
				yd = vid.height / 2 - sb_lines - 8;
			}
			else if (CURRVIEW == 1) {
				mv_viewrect_t view;

				SCR_SetMVStatusTwoInsetViewRect(&view);

				xb = (view.x + view.width) - strlen (strng) * 8; // hud
				yb = (view.y + view.height) - 16;
				xd = (view.x + view.width) - strlen (weapons) * 8 - 70; // weapons
				yd = (view.y + view.height) - 8;
			}
		}
	}
	else if (cl_multiview.value == 3) {
		if (CURRVIEW == 2) {
			// top
			xb = vid.width - strlen (strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xd = vid.width - strlen (weapons) * 8 - 84;
			yd = vid.height / 2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) {
			// Bottom left
			xb = vid.width - (vid.width / 2) - strlen (strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xd = vid.width - (vid.width / 2) - strlen (weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else if (CURRVIEW == 1) {
			// Bottom right
			xb = vid.width - strlen (strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xd = vid.width - strlen (weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}
	else if (cl_multiview.value == 4) {
		if (CURRVIEW == 2) {
			// Top left
			xb = vid.width - (vid.width / 2) - strlen (strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xd = vid.width - (vid.width / 2) - strlen (weapons) * 8 - 84;
			yd = vid.height / 2 - sb_lines - 8;
		}
		else if (CURRVIEW == 3) {
			// Top right
			xb = vid.width - strlen (strng) * 8 - 12;
			yb = vid.height / 2 - sb_lines - 16;
			xd = vid.width - strlen (weapons) * 8 - 84;
			yd = vid.height / 2 - sb_lines - 8;
		}
		else if (CURRVIEW == 4) {
			// Bottom left
			xb = vid.width - (vid.width / 2) - strlen (strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xd = vid.width - (vid.width / 2) - strlen (weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
		else {
			// Bottom right
			xb = vid.width - strlen (strng) * 8 - 12;
			yb = vid.height - sb_lines - 16;
			xd = vid.width - strlen (weapons) * 8 - 84;
			yd = vid.height - sb_lines - 8;
		}
	}

	// Hud info
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2)) {
		Draw_String (xb, yb, strng);
	}
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && cl_mvinsethud.value) {
		if (vid.width > 512) {
			Draw_String (xb, yb, strng);
		}
		else {
			// <= 512 mvinset, just draw the name
			int var, limit;
			char namestr[16];
			mv_viewrect_t view;

			SCR_SetMVStatusTwoInsetViewRect(&view);

			var = ((view.x + view.width) - 320) * 0.05;
			var--;
			var |= (var >> 1);
			var |= (var >> 2);
			var |= (var >> 4);
			var |= (var >> 8);
			var |= (var >> 16);
			var++;

			limit = bound(0, view.width / 8.0f, sizeof(namestr));
			strlcpy(namestr, name, limit);

			Draw_String((view.x + view.width) - strlen(namestr) * 8 - var - 2, yb + (cl_sbar.integer ? 1 : 4), namestr);
		}
	}

	// Weapons
	if ((cl_mvdisplayhud.value && !cl_mvinset.value && cl_multiview.value == 2)
		|| (cl_mvdisplayhud.value && cl_multiview.value != 2)) {
		Draw_String (xd, yd, weapons);
	}
	else if (cl_multiview.value == 2 && cl_mvdisplayhud.value && CURRVIEW == 1 && vid.width > 512 && cl_mvinsethud.value) {
		Draw_String (xd, yd, weapons);
	}
}






// cl_main.c
static void CL_Multiview (void)
{
	static int playernum = 0;

	if (!cls.mvdplayback) {
		return;
	}

	nNumViews = cl_multiview.value;

	/*
	// Only refresh skins once (to start with), I think this is the best solution for multiview
	// eg when viewing 4 players in a 2v2.
	// Also refresh them when the player changes what team to track.
	if ((!CURRVIEW && cls.state >= ca_connected) || nSwapPov) {
		TP_RefreshSkins ();
	}*/

	CL_MultiviewOverrideValues ();

	nPlayernum = playernum;

	// Copy the stats for the player we're about to draw in the next
	// view to the client state, so that the correct stats are drawn
	// in the multiview mini-HUD.
	memcpy (cl.stats, cl.players[playernum].stats, sizeof (cl.stats));

	//
	// Increase the current view being rendered.
	//
	CURRVIEW = CL_IncrLoop (CURRVIEW, cl_multiview.integer);

	if (cl_mvinset.value && cl_multiview.value == 2) {
		//
		// Special case for mvinset and tracking 2 people
		// this is meant for spectating duels primarily.
		// Lets the user swap which player is shown in the
		// main view and the mvinset by pressing jump.
		//

		// If both the mvinset and main view is set to show
		// the same player, pick the first player for the main view
		// and the next after that for the mvinset.
		if (nTrack1duel == nTrack2duel) {
			nTrack1duel = CL_NextPlayer (-1);
			nTrack2duel = CL_NextPlayer (nTrack1duel);
		}

		// The user pressed jump so we need to swap the pov.
		if (nSwapPov) {
			if (nSwapPov > 0) {
				nTrack1duel = CL_NextPlayer (nTrack1duel);
				nTrack2duel = CL_NextPlayer (nTrack2duel);
			}
			else {
				nTrack1duel = CL_PrevPlayer (nTrack1duel);
				nTrack2duel = CL_PrevPlayer (nTrack2duel);
			}
			nSwapPov = false;
		}
		else {
			// Set the playernum based on if we're drawing the mvinset
			// or the main view
			// (nTrack1duel = main view)
			// (nTrack2duel = mvinset)
			playernum = (CURRVIEW == 1) ? nTrack1duel : nTrack2duel;
		}
	}
	else {
		//
		// Normal multiview.
		//

		// Start from the first player on each new frame.
		playernum = ((CURRVIEW == 1) ? 0 : playernum);

		//
		// The player pressed jump and wants to change what team is spectated.
		//
		if (nSwapPov && cl_multiview.value >= 2 && cl.teamplay) {
			int j;
			int team_slot_count = 0;
			int last_mv_trackslots[4];

			// Save the old track values and reset them.
			memcpy (last_mv_trackslots, mv_trackslots, sizeof (last_mv_trackslots));
			memset (mv_trackslots, -1, sizeof (mv_trackslots));

			// Find the new team.
			for (j = 0; j < MAX_CLIENTS; j++) {
				if (!cl.players[j].spectator && cl.players[j].name[0]) {
					// Find the opposite team from the one we are tracking now.
					if (!currteam[0] || strcmp (currteam, cl.players[j].team)) {
						strlcpy (currteam, cl.players[j].team, sizeof (currteam));
						break;
					}
				}
			}

			// Find the team members.
			for (j = 0; j < MAX_CLIENTS; j++) {
				if (!cl.players[j].spectator
					&& cl.players[j].name[0]
					&& !strcmp (currteam, cl.players[j].team)) {
					// Find the player slot to track.
					mv_trackslots[team_slot_count] = Player_StringtoSlot (cl.players[j].name, false, false);
					team_slot_count++;
				}

				Com_DPrintf ("New trackslots: %i %i %i %i\n", mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);

				// Don't go out of bounds in the mv_trackslots array.
				if (team_slot_count == 4) {
					break;
				}
			}

			if (cl_multiview.value == 2 && team_slot_count == 2) {
				// Switch between 2on2 teams.
			}
			else if (cl_multiview.value < team_slot_count || team_slot_count >= 3) {
				// We don't want to show all from one team and then one of the enemies...
				cl_multiview.value = team_slot_count;
			}
			else if (team_slot_count == 2) {
				// 2on2... one team on top, on on the bottom
				// Swap the teams between the top and bottom in a 4 view setup.
				cl_multiview.value = 4;
				mv_trackslots[MV_VIEW3] = last_mv_trackslots[MV_VIEW1];
				mv_trackslots[MV_VIEW4] = last_mv_trackslots[MV_VIEW2];

				Com_DPrintf ("Team on top/bottom trackslots: %i %i %i %i\n",
					mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);
			}
		}
		else {
			// Check if the track* values have been set by the user,
			// otherwise show the first 4 players.

			if (CURRVIEW >= 1 && CURRVIEW <= 4) {
				// If the value of mv_trackslots[i] is negative, it means that view
				// doesn't have any track value set so we need to find someone to track using CL_NextPlayer().
				playernum = ((mv_trackslots[CURRVIEW - 1] < 0) ? CL_NextPlayer (playernum) : mv_trackslots[CURRVIEW - 1]);
			}
		}
	}

	// BUGFIX - Make sure the player we're tracking is still left, might have disconnected since
	// we picked him for tracking (This would result in being thrown into freefly mode and not being able
	// to go back into tracking mode when a player disconnected).
	if (!nSwapPov && (cl.players[playernum].spectator || !cl.players[playernum].name[0])) {
		mv_trackslots[CURRVIEW - 1] = -1;
	}

	if (nSwapPov) {
		Com_DPrintf ("Final trackslots: %i %i %i %i\n", mv_trackslots[0], mv_trackslots[1], mv_trackslots[2], mv_trackslots[3]);
	}

	nSwapPov = false;

	// Set the current player we're tracking for the next view to be drawn.
	// BUGFIX: Only change the spec_track if the new track target is a player.
	if (!cl.players[playernum].spectator && cl.players[playernum].name[0]) {
		next_spec_track = playernum;
	}

	// Make sure we reset variables we suppressed during multiview drawing.
	bExitmultiview = true;
}

typedef struct mv_temp_cvar_s {
	cvar_t* cvar;
	float   value;
} mv_temp_cvar_t;

static mv_temp_cvar_t multiviewCvars[] = {
	{ &scr_viewsize,      100.0f },
	{ &cl_fakeshaft,        0.0f },
	{ &gl_polyblend,        1.0f },
	{ &gl_clear,            0.0f },
};
#define MV_CVAR_VIEWSIZE 0                 // we reference saved value when cl_mvinset specified

void CL_MultiviewSaveValues (void)
{
	int i = 0;

	for (i = 0; i < sizeof (multiviewCvars) / sizeof (multiviewCvars[0]); ++i) {
		multiviewCvars[i].value = multiviewCvars[i].cvar->value;
	}
}

void CL_MultiviewRestoreValues (void)
{
	int i = 0;

	for (i = 0; i < sizeof (multiviewCvars) / sizeof (multiviewCvars[0]); ++i) {
		Cvar_SetValue (multiviewCvars[i].cvar, multiviewCvars[i].value);
	}

	bExitmultiview = false;
}

static void CL_MultiviewOverrideValues (void)
{
	// contrast was disabled for OpenGL build with the note "blanks all but 1 view"
	// this was due to gl_ztrick in R_Clear(void) that would clear these. FIXED
	// v_contrast.value = 1;

	// stop fakeshaft as it lerps with the other views
	if (cl_fakeshaft.value < 1 && cl_fakeshaft.value > 0) {
		cl_fakeshaft.value = 0;
	}

	// allow mvinset 1 to use viewsize value
	scr_viewsize.value = multiviewCvars[MV_CVAR_VIEWSIZE].value;
	if ((!cl_mvinset.value && cl_multiview.value == 2) || cl_multiview.value != 2) {
		scr_viewsize.value = 120;
	}

	// stop small screens
	if (cl_mvinset.value && cl_multiview.value == 2 && scr_viewsize.value < 100) {
		scr_viewsize.value = 100;
	}

	gl_polyblend.value = 0;
	gl_clear.value = 0;
}

static qbool CL_MultiviewCvarResetRequired (void)
{
	return bExitmultiview;
}

// Can be used during rendering to find out what the next spec track will be
//   (especially useful when CURRVIEW == 1 after CL_Multiview() to report who tracked in main view)
int CL_MultiviewNextPlayer (void)
{
	if (next_spec_track >= 0) {
		return next_spec_track;
	}
	return cl.spec_track;
}

int CL_MultiviewCurrentView (void)
{
	return CURRVIEW;
}

int CL_MultiviewNumberViews (void)
{
	return nNumViews;
}

void CL_MultiviewResetCvars (void)
{
	if (CL_MultiviewCvarResetRequired ()) {
		CL_MultiviewRestoreValues ();
		nTrack1duel = nTrack2duel = 0;
		CURRVIEW = 0;
	}
}

qbool CL_MultiviewEnabled (void)
{
	return cl_multiview.value && cls.mvdplayback && !cl.intermission;
}

qbool CL_MultiviewInsetEnabled (void)
{
	return cl_multiview.value == 2 && cls.mvdplayback && cl_mvinset.value;
}

qbool CL_MultiviewInsetView (void)
{
	return CL_MultiviewInsetEnabled() && CURRVIEW == 1;
}

int CL_MultiviewActiveViews (void)
{
	if (cls.mvdplayback)
		return bound (1, cl_multiview.integer, 4);
	return 1;
}





// Events called during game loop

void CL_MultiviewFrameStart (void)
{
	next_spec_track = -1;
}

void CL_MultiviewFrameFinish (void)
{
	if (next_spec_track >= 0) {
		cl.spec_track = next_spec_track;
	}
}

void CL_MultiviewDemoStart (void)
{
	memset (mv_trackslots, -1, sizeof (mv_trackslots));
	nTrack1duel = 0;
	nTrack2duel = 0;
}

void CL_MultiviewDemoFinish (void)
{
}

void CL_MultiviewDemoStopRewind (void)
{
	memcpy (mv_trackslots, rewind_trackslots, sizeof (mv_trackslots));
	nTrack1duel = rewind_duel_track1;
	nTrack2duel = rewind_duel_track2;
}

void CL_MultiviewDemoStartRewind (void)
{
	memcpy (rewind_trackslots, mv_trackslots, sizeof (rewind_trackslots));
	rewind_duel_track1 = nTrack1duel;
	rewind_duel_track2 = nTrack2duel;
}

void CL_MultiviewTrackingAdjustment (int adjustment)
{
	nSwapPov = adjustment;
}

void CL_MultiviewInitialise (void)
{
	memset (mv_trackslots, -1, sizeof (mv_trackslots));
}

void CL_TrackMV1_f (void)
{
	CL_Track (MV_VIEW1);
}

void CL_TrackMV2_f (void)
{
	CL_Track (MV_VIEW2);
}
void CL_TrackMV3_f (void)
{
	CL_Track (MV_VIEW3);
}
void CL_TrackMV4_f (void)
{
	CL_Track (MV_VIEW4);
}

void CL_TrackTeam_f (void)
{
	int i, teamchoice, team_slot_count = 0;

	if (cls.state < ca_connected) {
		Com_Printf ("You must be connected to track\n", Cmd_Argv (0));
		return;
	}

	if (!cl.spectator) {
		Com_Printf ("You can only track in spectator mode\n", Cmd_Argv (0));
		return;
	}

	if (Cmd_Argc () != 2) {
		Com_Printf ("Usage: %s < 1 | 2 >\n", Cmd_Argv (0));
		return;
	}

	// Get the team.
	teamchoice = atoi (Cmd_Args ());

	if (!currteam[0]) {
		// Find the the first team.
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (VALID_PLAYER (i) && strcmp (currteam, cl.players[i].team) != 0) {
				strlcpy (currteam, cl.players[i].team, sizeof (currteam));
				break;
			}
		}
	}

	// Find the team members.
	for (i = 0; i < MAX_CLIENTS; i++) {
		// Find the player slot to track.
		if (!cl.players[i].spectator && strcmp (cl.players[i].name, "")
			&& teamchoice == 1 && !strcmp (currteam, cl.players[i].team)) {
			mv_trackslots[team_slot_count] = Player_StringtoSlot (cl.players[i].name, false, false);
			team_slot_count++;
		}
		else if (!cl.players[i].spectator && strcmp (cl.players[i].name, "")
			&& teamchoice == 2 && strcmp (currteam, cl.players[i].team)) {
			mv_trackslots[team_slot_count] = Player_StringtoSlot (cl.players[i].name, false, false);
			team_slot_count++;
		}

		// Don't go out of bounds in the mv_trackslots array.
		if (team_slot_count == 4) {
			break;
		}
	}

	cl_multiview.value = team_slot_count;
}

void CL_MultiviewSetTrackSlot (int trackSlot, int player)
{
	if (trackSlot >= 0 && trackSlot < sizeof (mv_trackslots) / sizeof (mv_trackslots[0])) {
		mv_trackslots[trackSlot] = player;
	}
	else {
		nTrack1duel = player;
		nTrack2duel = CL_NextPlayer (player);
	}
}

void CL_MultiviewPreUpdateScreen (void)
{
	if (!CL_MultiviewCvarResetRequired ()) {
		CL_MultiviewSaveValues ();
	}
	if (CL_MultiviewCvarResetRequired () && !cl_multiview.value) {
		CL_MultiviewRestoreValues ();
	}
}

qbool CL_MultiviewAdvanceView (void)
{
	CL_Multiview ();

	return CURRVIEW > 1;
}

int CL_MultiviewMainView (void)
{
	return nTrack1duel;
}

int CL_MultiviewAutotrackSlot (void)
{
	return CL_MultiviewInsetEnabled () ? nTrack1duel : cl.viewplayernum;
}

qbool CL_MultiviewGetCrosshairCoordinates(qbool use_screen_coords, float* cross_x, float* cross_y, qbool* half_size)
{
	extern vrect_t scr_vrect;

	float x, y;
	float min_x = scr_vrect.x;
	float width = scr_vrect.width;
	float min_y = scr_vrect.y;
	float height = scr_vrect.height;

	if (use_screen_coords) {
		min_x = min_y = 0;
		width = VID_RenderWidth2D();
		height = VID_RenderHeight2D();

		min_y += scr_vrect.y * ((float)height / vid.height);
		height -= (vid.height - scr_vrect.height) * ((float)height / vid.height);
	}

	x = min_x + 0.5 * width;
	y = min_y + 0.5 * height;
	*half_size = false;

	if (CL_MultiviewEnabled()) {
		int active_views = CL_MultiviewActiveViews();

		if (active_views == 2) {
			if (CL_MultiviewInsetEnabled()) {
				// inset
				if (CL_MultiviewInsetView()) {
					if (!cl_mvinsetcrosshair.value) {
						return false;
					}

					x = inset_x + inset_width / 2.0f;
					y = inset_y + inset_height / 2.0f;

					// y is flipped in 2d world...
					y = VID_RenderHeight2D() - y;

					*half_size = true;
				}
			}
			else {
				if (CL_MultiviewCurrentView() == 1)
				{
					x = min_x + width / 2;
					y = min_y + height * 3.0f / 4.0f;
				}
				else if (CL_MultiviewCurrentView() == 2)
				{
					// top cv2
					x = min_x + width / 2;
					y = min_y + height * 1.0f / 4.0f;
				}
				*half_size = true;
			}
		}
		else if (active_views == 3) {
			if (CL_MultiviewCurrentView() == 2) {
				// top
				x = min_x + width / 2;
				y = min_y + height / 4.0f;
			}
			else if (CL_MultiviewCurrentView() == 3) {
				// bl
				x = min_x + width / 4.0f;
				y = min_y + height * 3.0f / 4.0f;
			}
			else {
				// br
				x = min_x + width * 3.0f / 4.0f;
				y = min_y + height * 3.0f / 4.0f;
			}
			*half_size = true;
		}
		else if (active_views >= 4) {
			if (CL_MultiviewCurrentView() == 2) {
				// tl
				x = min_x + width / 4.0f;
				y = min_y + height / 4.0f;
			}
			else if (CL_MultiviewCurrentView() == 3) {
				// tr
				x = min_x + width * 3.0f / 4.0f;
				y = min_y + height / 4.0f;
			}
			else if (CL_MultiviewCurrentView() == 4) {
				// bl
				x = min_x + width / 4.0f;
				y = min_y + height * 3.0f / 4.0f;
			}
			else if (CL_MultiviewCurrentView() == 1) {
				// br
				x = min_x + width * 3.0f / 4.0f;
				y = min_y + height * 3.0f / 4.0f;
			}
			*half_size = true;
		}
	}

	*cross_x = x;
	*cross_y = y;
	return true;
}

void SCR_DrawMultiviewBorders(void)
{
	//
	// Draw black borders around the views.
	//
	if (cl_multiview.integer == 2 && !cl_mvinset.integer) {
		Draw_Fill(0, vid.height / 2, vid.width - 1, 1, 0);
	}
	else if (cl_multiview.integer == 2 && cl_mvinset.integer) {
		extern byte color_black[4];

		R_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
		Draw_AlphaRectangleRGB(inset_x, inset_y, inset_width, inset_height, 1.0f, false, RGBAVECT_TO_COLOR(color_black));
		R_OrthographicProjection(0, vid.width, vid.height, 0, -99999, 99999);
	}
	else if (cl_multiview.integer == 3) {
		Draw_Fill(vid.width / 2, vid.height / 2, 1, vid.height / 2, 0);
		Draw_Fill(0, vid.height / 2, vid.width, 1, 0);
	}
	else if (cl_multiview.integer == 4) {
		Draw_Fill(vid.width / 2, 0, 1, vid.height, 0);
		Draw_Fill(0, vid.height / 2, vid.width, 1, 0);
	}
}

void CL_MultiviewInsetSetScreenCoordinates(int x, int y, int width, int height)
{
	inset_x = x;
	inset_y = y;
	inset_width = width;
	inset_height = height;
}

centity_t* CL_WeaponModelForView(void)
{
	int view = bound(0, CURRVIEW - 1, MV_VIEWS - 1);

	return &cl.viewent[view];
}

void CL_MultiviewInsetRestoreStats(void)
{
	if (cl_multiview.value == 2 && cl_mvinset.integer) {
		memcpy(cl.stats, cl.players[nTrack1duel].stats, sizeof(cl.stats));
	}
}
