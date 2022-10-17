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
#include "stats_grid.h"
#include "utils.h"
#include "hud.h"
#include "gl_model.h"
#include "image.h"
#if defined(WITH_PNG)
#include <png.h>
#endif
#include "sbar.h"
#include "r_texture.h"
#include "r_renderer.h"

// What stats to draw.
#define HUD_RADAR_STATS_NONE				0
#define HUD_RADAR_STATS_BOTH_TEAMS_HOLD		1
#define HUD_RADAR_STATS_TEAM1_HOLD			2
#define HUD_RADAR_STATS_TEAM2_HOLD			3
#define HUD_RADAR_STATS_BOTH_TEAMS_DEATHS	4
#define HUD_RADAR_STATS_TEAM1_DEATHS		5
#define HUD_RADAR_STATS_TEAM2_DEATHS		6

// The skinnum property in the entity_s structure is used
// for determening what type of armor to draw on the radar.
#define HUD_RADAR_GA					0
#define HUD_RADAR_YA					1
#define HUD_RADAR_RA					2

#define HUD_RADAR_HIGHLIGHT_NONE			0
#define HUD_RADAR_HIGHLIGHT_TEXT_ONLY		1
#define HUD_RADAR_HIGHLIGHT_OUTLINE			2
#define HUD_RADAR_HIGHLIGHT_FIXED_OUTLINE	3
#define HUD_RADAR_HIGHLIGHT_CIRCLE			4
#define HUD_RADAR_HIGHLIGHT_FIXED_CIRCLE	5
#define HUD_RADAR_HIGHLIGHT_ARROW_BOTTOM	6
#define HUD_RADAR_HIGHLIGHT_ARROW_CENTER	7
#define HUD_RADAR_HIGHLIGHT_ARROW_TOP		8
#define HUD_RADAR_HIGHLIGHT_CROSS_CORNERS	9
#define HUD_RADAR_HIGHLIGHT_BRIGHT_VIEW    10

// Radar filters.
#define RADAR_SHOW_WEAPONS (radar_show_ssg || radar_show_ng || radar_show_sng || radar_show_gl || radar_show_rl || radar_show_lg)
static qbool radar_show_ssg = false;
static qbool radar_show_ng = false;
static qbool radar_show_sng = false;
static qbool radar_show_gl = false;
static qbool radar_show_rl = false;
static qbool radar_show_lg = false;

#define RADAR_SHOW_ITEMS (radar_show_backpacks || radar_show_health || radar_show_ra || radar_show_ya || radar_show_ga || radar_show_rockets || radar_show_nails || radar_show_cells || radar_show_shells || radar_show_quad || radar_show_pent || radar_show_ring || radar_show_suit)
static qbool radar_show_backpacks = false;
static qbool radar_show_health = false;
static qbool radar_show_ra = false;
static qbool radar_show_ya = false;
static qbool radar_show_ga = false;
static qbool radar_show_rockets = false;
static qbool radar_show_nails = false;
static qbool radar_show_cells = false;
static qbool radar_show_shells = false;
static qbool radar_show_quad = false;
static qbool radar_show_pent = false;
static qbool radar_show_ring = false;
static qbool radar_show_suit = false;
static qbool radar_show_mega = false;

#define RADAR_SHOW_OTHER (radar_show_gibs || radar_show_explosions || radar_show_nails_p || radar_show_rockets_p || radar_show_shaft_p || radar_show_teleport || radar_show_shotgun)
static qbool radar_show_nails_p = false;
static qbool radar_show_rockets_p = false;
static qbool radar_show_shaft_p = false;
static qbool radar_show_gibs = false;
static qbool radar_show_explosions = false;
static qbool radar_show_teleport = false;
static qbool radar_show_shotgun = false;

#define HUD_COLOR_DEFAULT_TRANSPARENCY	75

byte hud_radar_highlight_color[4] = { 255, 255, 0, HUD_COLOR_DEFAULT_TRANSPARENCY };

#ifdef WITH_PNG

// Map picture to draw for the mapoverview hud control.
mpic_t radar_pic;
static qbool radar_pic_found = false;

// The conversion formula used for converting from quake coordinates to pixel coordinates
// when drawing on the map overview.
static float map_x_slope;
static float map_x_intercept;
static float map_y_slope;
static float map_y_intercept;
static qbool conversion_formula_found = false;

// Used for drawing the height of the player.
static float map_height_diff = 0.0;

#define RADAR_BASE_PATH_FORMAT	"radars/%s.png"

//
// Is run when a new map is loaded.
//
void HUD_NewRadarMap(void)
{
	int i = 0;
	int len = 0;
	int n_textcount = 0;
	mpic_t *radar_pic_p = NULL;
	png_textp txt = NULL;
	char *radar_filename = NULL;

	if (!cl.worldmodel)
		return; // seems we are not ready to do that

	// Reset the radar pic status.
	memset (&radar_pic, 0, sizeof(radar_pic));
	radar_pic_found = false;
	conversion_formula_found = false;

	// Allocate a string for the path to the radar image.
	len = strlen (RADAR_BASE_PATH_FORMAT) + strlen (host_mapname.string);
	radar_filename = Q_calloc (len, sizeof(char));
	snprintf (radar_filename, len, RADAR_BASE_PATH_FORMAT, host_mapname.string);

	// Load the map picture.
	if ((radar_pic_p = R_LoadPicImage (radar_filename, host_mapname.string, 0, 0, TEX_ALPHA)) != NULL) {
		radar_pic = *radar_pic_p;
		radar_pic_found = true;

		renderer.TextureWrapModeClamp(radar_pic.texnum);

		// Calculate the height of the map.
		map_height_diff = fabs((float)(cl.worldmodel->maxs[2] - cl.worldmodel->mins[2]));

		// Get the comments from the PNG.
		txt = Image_LoadPNG_Comments(radar_filename, &n_textcount);

		// Check if we found any comments.
		if(txt != NULL)
		{
			int found_count = 0;

			// Find the conversion formula in the comments found in the PNG.
			for(i = 0; i < n_textcount; i++)
			{
				if(!strcmp(txt[i].key, "QWLMConversionSlopeX"))
				{
					map_x_slope = atof(txt[i].text);
					found_count++;
				}
				else if(!strcmp(txt[i].key, "QWLMConversionInterceptX"))
				{
					map_x_intercept = atof(txt[i].text);
					found_count++;
				}
				else if(!strcmp(txt[i].key, "QWLMConversionSlopeY"))
				{
					map_y_slope = atof(txt[i].text);
					found_count++;
				}
				else if(!strcmp(txt[i].key, "QWLMConversionInterceptY"))
				{
					map_y_intercept = atof(txt[i].text);
					found_count++;
				}

				Q_free(txt[i].key);
				Q_free(txt[i].text);

				conversion_formula_found = (found_count == 4);
			}

			// Free the text chunks.
			Q_free(txt);
		}
		else
		{
			conversion_formula_found = false;
			Con_Printf("No conversion formula found\n");
		}
	}
	else
	{
		// No radar pic found.
		memset (&radar_pic, 0, sizeof(radar_pic));
		radar_pic_found = false;
		conversion_formula_found = false;
	}

	// Free the path string to the radar png.
	Q_free (radar_filename);
}

static void Radar_DrawGrid(stats_weight_grid_t *grid, int x, int y, float scale, int pic_width, int pic_height, int style)
{
	int row, col;

	// Don't try to draw anything if we got no data.
	if(grid == NULL || grid->cells == NULL || style == HUD_RADAR_STATS_NONE)
	{
		return;
	}

	// Go through all the cells and draw them based on their weight.
	for(row = 0; row < grid->row_count; row++)
	{
		// Just to be safe if something went wrong with the allocation.
		if(grid->cells[row] == NULL)
		{
			continue;
		}

		for(col = 0; col < grid->col_count; col++)
		{
			float weight = 0.0;
			int color = 0;

			float tl_x, tl_y;			// The pixel coordinate of the top left corner of a grid cell.
			float p_cell_length_x;		// The pixel length of a cell.
			float p_cell_length_y;		// The pixel "length" on the Y-axis. We calculate this
										// seperatly because we'll get errors when converting from
										// quake coordinates -> pixel coordinates.

			// Calculate the pixel coordinates of the top left corner of the current cell.
			// (This is times 8 because the conversion formula was calculated from a .loc-file)
			tl_x = (map_x_slope * (8.0 * grid->cells[row][col].tl_x) + map_x_intercept) * scale;
			tl_y = (map_y_slope * (8.0 * grid->cells[row][col].tl_y) + map_y_intercept) * scale;

			// Calculate the cell length in pixel length.
			p_cell_length_x = map_x_slope*(8.0 * grid->cell_length) * scale;
			p_cell_length_y = map_y_slope*(8.0 * grid->cell_length) * scale;

			// Add rounding errors (so that we don't get weird gaps in the grid).
			p_cell_length_x += tl_x - Q_rint(tl_x);
			p_cell_length_y += tl_y - Q_rint(tl_y);

			// Don't draw the stats stuff outside the picture.
			if(tl_x + p_cell_length_x > pic_width || tl_y + p_cell_length_y > pic_height || x + tl_x < x || y + tl_y < y)
			{
				continue;
			}

			//
			// Death stats.
			//
			if(grid->cells[row][col].teams[STATS_TEAM1].death_weight + grid->cells[row][col].teams[STATS_TEAM2].death_weight > 0)
			{
				weight = 0;

				if(style == HUD_RADAR_STATS_BOTH_TEAMS_DEATHS || style == HUD_RADAR_STATS_TEAM1_DEATHS)
				{
					weight = grid->cells[row][col].teams[STATS_TEAM1].death_weight;
				}

				if(style == HUD_RADAR_STATS_BOTH_TEAMS_DEATHS || style == HUD_RADAR_STATS_TEAM2_DEATHS)
				{
					weight += grid->cells[row][col].teams[STATS_TEAM2].death_weight;
				}

				color = 79;
			}

			//
			// Team stats.
			//
			{
				// No point in drawing if we have no weight.
				if(grid->cells[row][col].teams[STATS_TEAM1].weight + grid->cells[row][col].teams[STATS_TEAM2].weight <= 0
					&& (style == HUD_RADAR_STATS_BOTH_TEAMS_HOLD
						||	style == HUD_RADAR_STATS_TEAM1_HOLD
						||	style == HUD_RADAR_STATS_TEAM2_HOLD))
				{
					continue;
				}

				// Get the team with the highest weight for this cell.
				if(grid->cells[row][col].teams[STATS_TEAM1].weight > grid->cells[row][col].teams[STATS_TEAM2].weight
					&& (style == HUD_RADAR_STATS_BOTH_TEAMS_HOLD
						||	style == HUD_RADAR_STATS_TEAM1_HOLD))
				{
					weight = grid->cells[row][col].teams[STATS_TEAM1].weight;
					color = stats_grid->teams[STATS_TEAM1].color;
				}
				else if(style == HUD_RADAR_STATS_BOTH_TEAMS_HOLD ||	style == HUD_RADAR_STATS_TEAM2_HOLD)
				{
					weight = grid->cells[row][col].teams[STATS_TEAM2].weight;
					color = stats_grid->teams[STATS_TEAM2].color;
				}
			}

			// Draw the cell in the color of the team with the
			// biggest weight for this cell. Or draw deaths.
			Draw_AlphaFill(
				x + Q_rint(tl_x),			// X.
				y + Q_rint(tl_y),			// Y.
				Q_rint(p_cell_length_x),		// Width.
				Q_rint(p_cell_length_y),		// Height.
				color,						// Color.
				weight);					// Alpha.
		}
	}
}

static void Radar_OnChangeWeaponFilter(cvar_t *var, char *newval, qbool *cancel)
{
	// Parse the weapon filter.
	radar_show_ssg		= Utils_RegExpMatch("SSG|SUPERSHOTGUN|ALL",			newval);
	radar_show_ng		= Utils_RegExpMatch("([^S]|^)NG|NAILGUN|ALL",		newval); // Yes very ugly, but we don't want to match SNG.
	radar_show_sng		= Utils_RegExpMatch("SNG|SUPERNAILGUN|ALL",			newval);
	radar_show_rl		= Utils_RegExpMatch("RL|ROCKETLAUNCHER|ALL",		newval);
	radar_show_gl		= Utils_RegExpMatch("GL|GRENADELAUNCHER|ALL",		newval);
	radar_show_lg		= Utils_RegExpMatch("LG|SHAFT|LIGHTNING|ALL",		newval);
}

static void Radar_OnChangeItemFilter(cvar_t *var, char *newval, qbool *cancel)
{
	// Parse the item filter.
	radar_show_backpacks		= Utils_RegExpMatch("BP|BACKPACK|ALL",					newval);
	radar_show_health			= Utils_RegExpMatch("HP|HEALTH|ALL",					newval);
	radar_show_ra				= Utils_RegExpMatch("RA|REDARMOR|ARMOR|ALL",			newval);
	radar_show_ya				= Utils_RegExpMatch("YA|YELLOWARMOR|ARMOR|ALL",			newval);
	radar_show_ga				= Utils_RegExpMatch("GA|GREENARMOR|ARMOR|ALL",			newval);
	radar_show_rockets			= Utils_RegExpMatch("ROCKETS|ROCKS|AMMO|ALL",			newval);
	radar_show_nails			= Utils_RegExpMatch("NAILS|SPIKES|AMMO|ALL",			newval);
	radar_show_cells			= Utils_RegExpMatch("CELLS|BATTERY|AMMO|ALL",			newval);
	radar_show_shells			= Utils_RegExpMatch("SHELLS|AMMO|ALL",					newval);
	radar_show_quad				= Utils_RegExpMatch("QUAD|POWERUPS|ALL",				newval);
	radar_show_pent				= Utils_RegExpMatch("PENT|PENTAGRAM|666|POWERUPS|ALL",	newval);
	radar_show_ring				= Utils_RegExpMatch("RING|INVISIBLE|EYES|POWERUPS|ALL",	newval);
	radar_show_suit				= Utils_RegExpMatch("SUIT|POWERUPS|ALL",				newval);
	radar_show_mega				= Utils_RegExpMatch("MH|MEGA|MEGAHEALTH|100\\+|ALL",	newval);
}

static void Radar_OnChangeOtherFilter(cvar_t *var, char *newval, qbool *cancel)
{
	// Parse the "other" filter.
	radar_show_nails_p			= Utils_RegExpMatch("NAILS|PROJECTILES|ALL",	newval);
	radar_show_rockets_p		= Utils_RegExpMatch("ROCKETS|PROJECTILES|ALL",	newval);
	radar_show_shaft_p			= Utils_RegExpMatch("SHAFT|PROJECTILES|ALL",	newval);
	radar_show_gibs				= Utils_RegExpMatch("GIBS|ALL",					newval);
	radar_show_explosions		= Utils_RegExpMatch("EXPLOSIONS|ALL",			newval);
	radar_show_teleport			= Utils_RegExpMatch("TELE|ALL",					newval);
	radar_show_shotgun			= Utils_RegExpMatch("SHOTGUN|SG|BUCK|ALL",		newval);
}

static void Radar_OnChangeHighlightColor(cvar_t *var, char *newval, qbool *cancel)
{
	char *new_color;
	char buf[MAX_COM_TOKEN];

	// Translate a colors name to RGB values.
	new_color = ColorNameToRGBString(newval);

	// Parse the colors.
	//color = StringToRGB(new_color);
	strlcpy(buf,new_color,sizeof(buf));
	memcpy(hud_radar_highlight_color, StringToRGB(buf), sizeof(byte) * 4);

	// Set the cvar to contain the new color string
	// (if the user entered "red" it will be "255 0 0").
	Cvar_Set(var, new_color);
}

static void Radar_DrawEntities(int x, int y, float scale, float player_size, int show_hold_areas, float text_scale, qbool simple_items, qbool proportional)
{
	int i;

	// Entities (weapons and such). cl_main.c
	extern visentlist_t cl_visents;

	// Go through all the entities and draw the ones we're supposed to.
	for (i = 0; i < cl_visents.count; i++) {
		entity_t* currententity = &cl_visents.list[i].ent;
		texture_ref simpletexture;
		qbool simple_valid;

		int entity_q_x = 0;
		int entity_q_y = 0;
		int entity_p_x = 0;
		int entity_p_y = 0;

		// Find simple texture for entity
		if (currententity->skinnum < 0 || currententity->skinnum >= MAX_SIMPLE_TEXTURES) {
			simpletexture = currententity->model->simpletexture[0]; // ah...
		}
		else {
			simpletexture = currententity->model->simpletexture[currententity->skinnum];
		}
		simple_valid = R_TextureReferenceIsValid(simpletexture);

		// Get quake coordinates (times 8 to get them in the same format as .locs).
		entity_q_x = currententity->origin[0] * 8;
		entity_q_y = currententity->origin[1] * 8;

		// Convert from quake coordiantes -> pixel coordinates.
		entity_p_x = x + Q_rint((map_x_slope*entity_q_x + map_x_intercept) * scale);
		entity_p_y = y + Q_rint((map_y_slope*entity_q_y + map_y_intercept) * scale);

		// TODO: Replace all model name comparison below with MOD_HINT's instead for less comparisons (create new ones in Mod_LoadAliasModel() in r_model.c and gl_model.c/.h for the ones that don't have one already).

		//
		// Powerups.
		//

		if (radar_show_pent && currententity->model->modhint == MOD_PENT) {
			// Pentagram.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SColoredStringBasic(entity_p_x, entity_p_y, "&cf00P", 0, text_scale, proportional);
			}
		}
		else if (radar_show_quad && currententity->model->modhint == MOD_QUAD) {
			// Quad.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SColoredStringBasic(entity_p_x, entity_p_y, "&c0ffQ", 0, text_scale, proportional);
			}
		}
		else if (radar_show_ring && currententity->model->modhint == MOD_RING) {
			// Ring.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SColoredStringBasic(entity_p_x, entity_p_y, "&cff0R", 0, text_scale, proportional);
			}
		}
		else if (radar_show_suit && currententity->model->modhint == MOD_SUIT) {
			// Suit.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SColoredStringBasic(entity_p_x, entity_p_y, "&c0f0S", 0, text_scale, proportional);
			}
		}

		//
		// Show RL, LG and backpacks.
		//
		if (radar_show_rl && currententity->model->modhint == MOD_ROCKETLAUNCHER) {
			// RL.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (2 * 8) / 2, entity_p_y - 4, "RL", text_scale, proportional);
			}
		}
		else if (radar_show_lg && currententity->model->modhint == MOD_LIGHTNINGGUN) {
			// LG.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (2 * 8) / 2, entity_p_y - 4, "LG", text_scale, proportional);
			}
		}
		else if (radar_show_backpacks && currententity->model->modhint == MOD_BACKPACK) {
			// Back packs.
			float back_pack_size = 0;

			back_pack_size = max(player_size * 0.5, 0.05);

			Draw_AlphaCircleFill(entity_p_x, entity_p_y, back_pack_size, 114, 1);
			Draw_AlphaCircleOutline(entity_p_x, entity_p_y, back_pack_size, 1.0, 0, 1);
		}

		if (currententity->model->modhint == MOD_ARMOR) {
			//
			// Show armors.
			//
			if (radar_show_ga && currententity->skinnum == HUD_RADAR_GA) {
				// GA.
				if (simple_valid && simple_items) {
					Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
				}
				else {
					Draw_AlphaCircleFill(entity_p_x, entity_p_y, 3.0, 178, 1.0);
					Draw_AlphaCircleOutline(entity_p_x, entity_p_y, 3.0, 1.0, 0, 1.0);
				}
			}
			else if (radar_show_ya && currententity->skinnum == HUD_RADAR_YA) {
				// YA.
				if (simple_valid && simple_items) {
					Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
				}
				else {
					Draw_AlphaCircleFill(entity_p_x, entity_p_y, 3.0, 192, 1.0);
					Draw_AlphaCircleOutline(entity_p_x, entity_p_y, 3.0, 1.0, 0, 1.0);
				}
			}
			else if (radar_show_ra && currententity->skinnum == HUD_RADAR_RA) {
				// RA.
				if (simple_valid && simple_items) {
					Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
				}
				else {
					Draw_AlphaCircleFill(entity_p_x, entity_p_y, 3.0, 251, 1.0);
					Draw_AlphaCircleOutline(entity_p_x, entity_p_y, 3.0, 1.0, 0, 1.0);
				}
			}
		}

		if (radar_show_mega && currententity->model->modhint == MOD_MEGAHEALTH) {
			//
			// Show megahealth.
			//
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				// Draw a red border around the cross.
				Draw_AlphaRectangleRGB(entity_p_x - 3, entity_p_y - 3, 8, 8, 1, false, RGBA_TO_COLOR(200, 0, 0, 200));

				// Draw a black outline cross.
				Draw_AlphaFill(entity_p_x - 3, entity_p_y - 1, 8, 4, 0, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 3, 4, 8, 0, 1);

				// Draw a 2 pixel cross.
				Draw_AlphaFill(entity_p_x - 2, entity_p_y, 6, 2, 79, 1);
				Draw_AlphaFill(entity_p_x, entity_p_y - 2, 2, 6, 79, 1);
			}
		}

		if (radar_show_ssg && !strcmp(currententity->model->name, "progs/g_shot.mdl")) {
			// SSG.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (3 * 8) / 2, entity_p_y - 4, "SSG", text_scale, proportional);
			}
		}
		else if (radar_show_ng && !strcmp(currententity->model->name, "progs/g_nail.mdl")) {
			// NG.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (2 * 8) / 2, entity_p_y - 4, "NG", text_scale, proportional);
			}
		}
		else if (radar_show_sng && !strcmp(currententity->model->name, "progs/g_nail2.mdl")) {
			// SNG.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (3 * 8) / 2, entity_p_y - 4, "SNG", text_scale, proportional);
			}
		}
		else if (radar_show_gl && currententity->model->modhint == MOD_GRENADELAUNCHER) {
			// GL.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_SString(entity_p_x - (2 * 8) / 2, entity_p_y - 4, "GL", text_scale, proportional);
			}
		}

		if (radar_show_gibs && currententity->model->modhint == MOD_GIB) {
			//
			// Gibs.
			//

			Draw_AlphaCircleFill(entity_p_x, entity_p_y, 2.0, 251, 1);
		}

		if (radar_show_health
			&& (!strcmp(currententity->model->name, "maps/b_bh25.bsp")
			|| !strcmp(currententity->model->name, "maps/b_bh10.bsp"))) {
			//
			// Health.
			//

			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				// Draw a black outline cross.
				Draw_AlphaFill(entity_p_x - 3, entity_p_y - 1, 7, 3, 0, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 3, 3, 7, 0, 1);

				// Draw a cross.
				Draw_AlphaFill(entity_p_x - 2, entity_p_y, 5, 1, 79, 1);
				Draw_AlphaFill(entity_p_x, entity_p_y - 2, 1, 5, 79, 1);
			}
		}

		//
		// Ammo.
		//
		if (radar_show_rockets
			&& (!strcmp(currententity->model->name, "maps/b_rock0.bsp")
			|| !strcmp(currententity->model->name, "maps/b_rock1.bsp"))) {
			//
			// Rockets.
			//

			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				// Draw a black outline.
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 6, 3, 5, 0, 1);
				Draw_AlphaFill(entity_p_x - 2, entity_p_y - 1, 5, 5, 0, 1);

				// The brown rocket.
				Draw_AlphaFill(entity_p_x, entity_p_y - 5, 1, 5, 120, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y, 1, 3, 120, 1);
				Draw_AlphaFill(entity_p_x + 1, entity_p_y, 1, 3, 120, 1);
			}
		}

		if (radar_show_cells
			&& (!strcmp(currententity->model->name, "maps/b_batt0.bsp")
			|| !strcmp(currententity->model->name, "maps/b_batt1.bsp"))) {
			//
			// Cells.
			//

			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				// Draw a black outline.
				Draw_AlphaLine(entity_p_x - 3, entity_p_y, entity_p_x + 4, entity_p_y - 5, 3, 0, 1);
				Draw_AlphaLine(entity_p_x - 3, entity_p_y, entity_p_x + 3, entity_p_y, 3, 0, 1);
				Draw_AlphaLine(entity_p_x + 3, entity_p_y, entity_p_x - 3, entity_p_y + 4, 3, 0, 1);

				// Draw a yellow lightning!
				Draw_AlphaLine(entity_p_x - 2, entity_p_y, entity_p_x + 3, entity_p_y - 4, 1, 111, 1);
				Draw_AlphaLine(entity_p_x - 2, entity_p_y, entity_p_x + 2, entity_p_y, 1, 111, 1);
				Draw_AlphaLine(entity_p_x + 2, entity_p_y, entity_p_x - 2, entity_p_y + 3, 1, 111, 1);
			}
		}

		if (radar_show_nails
			&& (!strcmp(currententity->model->name, "maps/b_nail0.bsp")
			|| !strcmp(currententity->model->name, "maps/b_nail1.bsp"))) {
			//
			// Nails.
			//

			// Draw a black outline.
			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				Draw_AlphaFill(entity_p_x - 3, entity_p_y - 3, 7, 3, 0, 1);
				Draw_AlphaFill(entity_p_x - 2, entity_p_y - 2, 5, 3, 0, 0.5);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y, 3, 3, 0, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y + 3, 1, 1, 0, 0.5);
				Draw_AlphaFill(entity_p_x + 1, entity_p_y + 3, 1, 1, 0, 0.5);
				Draw_AlphaFill(entity_p_x, entity_p_y + 4, 1, 1, 0, 1);

				Draw_AlphaFill(entity_p_x - 2, entity_p_y - 2, 5, 1, 6, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 1, 3, 1, 6, 0.5);
				Draw_AlphaFill(entity_p_x, entity_p_y, 1, 4, 6, 1);
			}
		}

		if (radar_show_shells
			&& (!strcmp(currententity->model->name, "maps/b_shell0.bsp")
			|| !strcmp(currententity->model->name, "maps/b_shell1.bsp"))) {
			//
			// Shells.
			//

			if (simple_valid && simple_items) {
				Draw_2dAlphaTexture(entity_p_x - 3.0, entity_p_y - 3.0, 6.0, 6.0, simpletexture, 1.0f);
			}
			else {
				// Draw a black outline.
				Draw_AlphaFill(entity_p_x - 2, entity_p_y - 3, 5, 9, 0, 1);

				// Draw 2 shotgun shells.
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 2, 1, 4, 73, 1);
				Draw_AlphaFill(entity_p_x - 1, entity_p_y - 2 + 5, 1, 2, 104, 1);

				Draw_AlphaFill(entity_p_x + 1, entity_p_y - 2, 1, 4, 73, 1);
				Draw_AlphaFill(entity_p_x + 1, entity_p_y - 2 + 5, 1, 2, 104, 1);
			}
		}

		//
		// Show projectiles (rockets, grenades, nails, shaft).
		//

		if (radar_show_nails_p && currententity->model->modhint == MOD_SPIKE) {
			//
			// Spikes from SNG and NG.
			//

			Draw_AlphaFill(entity_p_x, entity_p_y, 1, 1, 254, 1);
		}
		else if (radar_show_rockets_p &&
			(currententity->model->modhint == MOD_ROCKET || currententity->model->modhint == MOD_GRENADE)
			) {
			//
			// Rockets and grenades.
			//

			float entity_angle = 0;
			int x_line_end = 0;
			int y_line_end = 0;

			// Get the entity angle in radians.
			entity_angle = DEG2RAD(currententity->angles[1]);

			x_line_end = entity_p_x + 5 * cos(entity_angle) * scale;
			y_line_end = entity_p_y - 5 * sin(entity_angle) * scale;

			// Draw the rocket/grenade showing it's angle also.
			Draw_AlphaLine(entity_p_x, entity_p_y, x_line_end, y_line_end, 1.0, 254, 1);
		}
		else if (radar_show_shaft_p && currententity->model->modhint == MOD_THUNDERBOLT) {
			//
			// Shaft beam.
			//

			float entity_angle = 0;
			float shaft_length = 0;
			float x_line_end = 0;
			float y_line_end = 0;

			// Get the length and angle of the shaft.
			shaft_length = currententity->model->maxs[1];
			entity_angle = (currententity->angles[1] * M_PI) / 180;

			// Calculate where the shaft beam's ending point.
			x_line_end = entity_p_x + shaft_length * cos(entity_angle);
			y_line_end = entity_p_y - shaft_length * sin(entity_angle);

			// Draw the shaft beam.
			Draw_AlphaLine(entity_p_x, entity_p_y, x_line_end, y_line_end, 1.0, 254, 1);
		}
	}

	// Show corpses (cl_deadbodyfilter might mean they aren't in visible entity list)
	{
		int entity_q_x = 0;
		int entity_q_y = 0;
		int entity_p_x = 0;
		int entity_p_y = 0;
		packet_entities_t *pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;
		char buffer[MAX_SCOREBOARDNAME + 8];

		for (i = 0; i < pack->num_entities; ++i) {
			entity_state_t *state = &pack->entities[i];

			// must be body or gibbed head
			if (state->modelindex != cl_modelindices[mi_player] && state->modelindex != cl_modelindices[mi_h_player]) {
				continue;
			}

			if (state->colormap <= 0 || state->colormap > MAX_CLIENTS) {
				continue;
			}

			// Get quake coordinates (times 8 to get them in the same format as .locs).
			entity_q_x = state->origin[0] * 8;
			entity_q_y = state->origin[1] * 8;

			// Convert from quake coordiantes -> pixel coordinates.
			entity_p_x = x + Q_rint((map_x_slope * entity_q_x + map_x_intercept) * scale);
			entity_p_y = y + Q_rint((map_y_slope * entity_q_y + map_y_intercept) * scale);

			{
				int player_color = Sbar_BottomColor(&cl.players[state->colormap - 1]);

				byte red = host_basepal[player_color * 3];
				byte green = host_basepal[player_color * 3 + 1];
				byte blue = host_basepal[player_color * 3 + 2];

				snprintf(buffer, sizeof(buffer), "&c%x%x%x%s",
					(unsigned int)(red / 16),
					(unsigned int)(green / 16),
					(unsigned int)(blue / 16),
					"X"
				);

				Draw_SColoredStringBasic(entity_p_x - 4, entity_p_y - 4, buffer, 0, 0.75f, proportional);
			}
		}
	}

	// Draw a circle around "hold areas", the grid cells within this circle
	// are the ones that are counted for that particular hold area. The team
	// that has the most percentage of these cells is considered to hold that area.
	if (show_hold_areas && stats_important_ents != NULL && stats_important_ents->list != NULL) {
		int entity_p_x = 0;
		int entity_p_y = 0;

		for (i = 0; i < stats_important_ents->count; i++) {
			entity_p_x = x + Q_rint((map_x_slope * 8 * stats_important_ents->list[i].origin[0] + map_x_intercept) * scale);
			entity_p_y = y + Q_rint((map_y_slope * 8 * stats_important_ents->list[i].origin[1] + map_y_intercept) * scale);

			Draw_SColoredStringBasic(entity_p_x - Draw_StringLength(stats_important_ents->list[i].name, -1, text_scale, proportional) / 2.0, entity_p_y - 4,
				va("&c55f%s", stats_important_ents->list[i].name), 0, text_scale, proportional);

			Draw_AlphaCircleOutline(entity_p_x, entity_p_y, map_x_slope * 8 * stats_important_ents->hold_radius * scale, 1.0, 15, 0.2);
		}
	}

	//
	// Draw temp entities (explosions, blood, teleport effects).
	//
	for (i = 0; i < MAX_TEMP_ENTITIES; i++) {
		float time_diff = 0.0;

		int entity_q_x = 0;
		int entity_q_y = 0;
		int entity_p_x = 0;
		int entity_p_y = 0;

		// Get the time since the entity spawned.
		if (cls.demoplayback) {
			time_diff = cls.demotime - temp_entities.list[i].time;
		}
		else {
			time_diff = cls.realtime - temp_entities.list[i].time;
		}

		// Don't show temp entities for long.
		if (time_diff < 0.25) {
			float radius = 0.0;
			radius = (time_diff < 0.125) ? (time_diff * 32.0) : (time_diff * 32.0) - time_diff;
			radius *= scale;
			radius = min(max(radius, 0), 200);

			// Get quake coordinates (times 8 to get them in the same format as .locs).
			entity_q_x = temp_entities.list[i].pos[0] * 8;
			entity_q_y = temp_entities.list[i].pos[1] * 8;

			entity_p_x = x + Q_rint((map_x_slope*entity_q_x + map_x_intercept) * scale);
			entity_p_y = y + Q_rint((map_y_slope*entity_q_y + map_y_intercept) * scale);

			if (radar_show_explosions
				&& (temp_entities.list[i].type == TE_EXPLOSION
				|| temp_entities.list[i].type == TE_TAREXPLOSION)) {
				//
				// Explosions.
				//

				Draw_AlphaCircleFill(entity_p_x, entity_p_y, radius, 235, 0.8);
			}
			else if (radar_show_teleport && temp_entities.list[i].type == TE_TELEPORT) {
				//
				// Teleport effect.
				//

				radius *= 1.5;
				Draw_AlphaCircleFill(entity_p_x, entity_p_y, radius, 244, 0.8);
			}
			else if (radar_show_shotgun && temp_entities.list[i].type == TE_GUNSHOT) {
				//
				// Shotgun fire.
				//

#define SHOTGUN_SPREAD 10
				int spread_x = 0;
				int spread_y = 0;
				int n = 0;

				for (n = 0; n < 10; n++) {
					spread_x = (int)(rand() / (((double)RAND_MAX + 1) / SHOTGUN_SPREAD));
					spread_y = (int)(rand() / (((double)RAND_MAX + 1) / SHOTGUN_SPREAD));

					Draw_AlphaFill(entity_p_x + spread_x - (SHOTGUN_SPREAD / 2), entity_p_y + spread_y - (SHOTGUN_SPREAD / 2), 1, 1, 8, 0.9);
				}
			}
		}
	}
}

void Radar_DrawPlayers(int x, int y, int width, int height, float scale,
	float show_height, float show_powerups,
	float player_size, float show_names,
	float fade_players, float highlight,
	char *highlight_color, float text_scale, qbool team_colours_for_name, qbool proportional)
{
	int i;
	player_state_t *state;
	player_info_t *info;

	// Get player state so we can know where he is (or on rare occassions, she).
	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;

	// Get the info for the player.
	info = cl.players;

	//
	// Draw the players.
	//
	for (i = 0; i < MAX_CLIENTS; i++, info++, state++)
	{
		// Players quake coordinates
		// (these are multiplied by 8, since the conversion formula was
		// calculated using the coordinates in a .loc-file, which are in
		// the format quake-coordainte*8).
		int player_q_x = 0;
		int player_q_y = 0;

		// The height of the player.
		float player_z = 1.0;
		float player_z_relative = 1.0;

		// Players pixel coordinates.
		int player_p_x = 0;
		int player_p_y = 0;

		// Used for drawing the the direction the
		// player is looking at.
		float player_angle = 0;
		int x_line_start = 0;
		int y_line_start = 0;
		int x_line_end = 0;
		int y_line_end = 0;

		// Color and opacity of the player.
		int player_color = 0;
		float player_alpha = 1.0;
		qbool player_cross = false;
		float player_outline = 1.0f;
		qbool has_weapon = false;

		// Make sure we're not drawing any ghosts.
		if(!info->name[0])
		{
			continue;
		}

		if (state->messagenum == cl.parsecount)
		{
			// TODO: Implement lerping to get smoother drawing.

			// Get the quake coordinates. Multiply by 8 since
			// the conversion formula has been calculated using
			// a .loc-file which is in that format.
			player_q_x = state->origin[0]*8;
			player_q_y = state->origin[1]*8;

			// Get the players view angle.
			player_angle = cls.demoplayback ? state->viewangles[1] : cl.simangles[1];

			// Convert from quake coordiantes -> pixel coordinates.
			player_p_x = Q_rint((map_x_slope*player_q_x + map_x_intercept) * scale);
			player_p_y = Q_rint((map_y_slope*player_q_y + map_y_intercept) * scale);

			player_color = Sbar_BottomColor(info);

			has_weapon = (info->stats[STAT_ITEMS] & (IT_ROCKET_LAUNCHER | IT_LIGHTNING));

			// Calculate the height of the player.
			if(show_height)
			{
				player_z = state->origin[2];
				player_z += (player_z >= 0) ? fabs(cl.worldmodel->mins[2]) : fabs(cl.worldmodel->maxs[2]);
				player_z_relative = min(fabs(player_z / map_height_diff), 1.0);
				player_z_relative = max(player_z_relative, 0.2);
			}

			// Make the players fade out as they get less armor/health.
			if (fade_players == 1 || fade_players == 3) {
				int stack = bound(0, SCR_HUD_TotalStrength(info->stats[STAT_HEALTH], info->stats[STAT_ARMOR], SCR_HUD_ArmorType(info->stats[STAT_ITEMS])), 220);

				// Don't let the players get completly transparent so add 0.2 to the final value.
				player_alpha = bound(0, 0.3f + (stack / 315.0f), 1.0f);
			}
			else if (fade_players == 2) {
				// Players are faded if they don't have weapon
				player_alpha = has_weapon ? 1.0f : 0.3f;
			}

			// Turn dead people into crosses.
			if (info->stats[STAT_HEALTH] <= 0) {
				player_alpha = 1.0;
				player_cross = true;
			}

			if (fade_players == 1 && has_weapon) {
				player_outline = 2.5f;
			}
			else if (fade_players == 2) {
				float armorType = SCR_HUD_ArmorType(info->stats[STAT_ITEMS]);
				int stack = bound(0, SCR_HUD_TotalStrength(info->stats[STAT_HEALTH], info->stats[STAT_ARMOR], armorType), 360);

				player_outline = stack < 80 ? 0.0f : stack < 110 ? 1.0f : stack < 200 ? 2.0f : stack < 300 ? 3.0f : 4.0f;
			}

			// Draw a ring around players with powerups if it's enabled.
			if (show_powerups) {
				if (info->stats[STAT_ITEMS] & IT_INVISIBILITY) {
					Draw_AlphaCircleFill (x + player_p_x, y + player_p_y, player_size*2*player_z_relative, 161, 0.2);
				}

				if (info->stats[STAT_ITEMS] & IT_INVULNERABILITY) {
					Draw_AlphaCircleFill (x + player_p_x, y + player_p_y, player_size*2*player_z_relative, 79, 0.5);
				}

				if (info->stats[STAT_ITEMS] & IT_QUAD) {
					Draw_AlphaCircleFill (x + player_p_x, y + player_p_y, player_size*2*player_z_relative, 244, 0.2);
				}
			}

			// Draw a circle around the tracked player.
			if (highlight != HUD_RADAR_HIGHLIGHT_NONE && Cam_TrackNum() >= 0 && info->userid == cl.players[Cam_TrackNum()].userid)
			{
				color_t higlight_color = RGBAVECT_TO_COLOR(hud_radar_highlight_color);

				// Draw the highlight.
				switch ((int)highlight)
				{
				case HUD_RADAR_HIGHLIGHT_CROSS_CORNERS :
				{
					// Top left
					Draw_AlphaLineRGB (x, y, x + player_p_x, y + player_p_y, 2, higlight_color);

					// Top right.
					Draw_AlphaLineRGB (x + width, y, x + player_p_x, y + player_p_y, 2, higlight_color);

					// Bottom left.
					Draw_AlphaLineRGB (x, y + height, x + player_p_x, y + player_p_y, 2, higlight_color);

					// Bottom right.
					Draw_AlphaLineRGB (x + width, y + height, x + player_p_x, y + player_p_y, 2, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_ARROW_TOP :
				{
					// Top center.
					Draw_AlphaLineRGB (x + width / 2, y, x + player_p_x, y + player_p_y, 2, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_ARROW_CENTER :
				{
					// Center.
					Draw_AlphaLineRGB (x + width / 2, y + height / 2, x + player_p_x, y + player_p_y, 2, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_ARROW_BOTTOM :
				{
					// Bottom center.
					Draw_AlphaLineRGB (x + width / 2, y + height, x + player_p_x, y + player_p_y, 2, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_FIXED_CIRCLE :
				{
					Draw_AlphaCircleRGB (x + player_p_x, y + player_p_y, player_size * 1.5, 1.0, true, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_CIRCLE :
				{
					Draw_AlphaCircleRGB (x + player_p_x, y + player_p_y, player_size * player_z_relative * 2.0, 1.0, true, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_FIXED_OUTLINE :
				{
					Draw_AlphaCircleOutlineRGB (x + player_p_x, y + player_p_y, player_size * 1.5, 1.0, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_OUTLINE :
				{
					Draw_AlphaCircleOutlineRGB (x + player_p_x, y + player_p_y, player_size * player_z_relative * 2.0, 1.0, higlight_color);
					break;
				}
				case HUD_RADAR_HIGHLIGHT_TEXT_ONLY :
				default :
				{
					break;
				}
				}
			}

			// Draw the actual player and a line showing what direction the player is looking in.
			if (!player_cross) {
				float lhs_angle = DEG2RAD(player_angle - 45);
				float rhs_angle = DEG2RAD(player_angle + 45);

				qbool highlight_player = highlight == HUD_RADAR_HIGHLIGHT_BRIGHT_VIEW && Cam_TrackNum() >= 0 && info->userid == cl.players[Cam_TrackNum()].userid;

				x_line_start = x + player_p_x;
				y_line_start = y + player_p_y;

				Draw_AlphaPieSlice(x_line_start, y_line_start, player_size * 2 * player_z_relative + 1, lhs_angle, rhs_angle, 1.0f, true, 15, highlight_player ? 0.4f : 0.1f);

				// Draw the player on the map.
				Draw_AlphaCircleFill(x + player_p_x, y + player_p_y, player_size * player_z_relative + player_outline, 0, 1.0f);
				Draw_AlphaCircleFill(x + player_p_x, y + player_p_y, player_size * player_z_relative, player_color, 1.0f);
				Draw_AlphaCircleFill(x + player_p_x, y + player_p_y, player_size * player_z_relative, 0, 1.0f - player_alpha);

				if (fade_players == 3 && has_weapon) {
					Draw_AlphaCircleFill(x + player_p_x, y + player_p_y, player_size * player_z_relative * 0.5f, 0, 1.0f);
				}
			}
			else {
				float size = (player_size * player_z_relative);

				x_line_start = x + player_p_x - size;
				y_line_start = y + player_p_y - size;

				x_line_end = x_line_start + 2 * size;
				y_line_end = y_line_start + 2 * size;

				Draw_AlphaLine(x_line_start, y_line_start, x_line_end, y_line_end, 3.0, player_color, player_alpha);
				Draw_AlphaLine(x_line_start, y_line_end, x_line_end, y_line_start, 3.0, player_color, player_alpha);
			}

			// Draw the players name.
			if (show_names)
			{
				int name_x = 0;
				int name_y = 0;
				int text_length = Draw_StringLength(info->name, -1, text_scale, proportional);

				name_x = x + player_p_x;
				name_y = y + player_p_y;

				// Make sure we're not too far right.
				if (name_x + text_length > x + width) {
					name_x -= (name_x + text_length - (x + width));
				}

				// Make sure we're not outside the radar to the left.
				name_x = max(name_x, x);

				// Draw the name.
				if (highlight >= HUD_RADAR_HIGHLIGHT_TEXT_ONLY && info->userid == cl.players[Cam_TrackNum()].userid) {
					char buffer[MAX_SCOREBOARDNAME + 8];

					snprintf(buffer, sizeof(buffer), "&c%x%x%x%s",
						(unsigned int)(hud_radar_highlight_color[0] / 16),
						(unsigned int)(hud_radar_highlight_color[1] / 16),
						(unsigned int)(hud_radar_highlight_color[2] / 16),
						info->name
					);

					// Draw the tracked players name in the user specified color.
					Draw_SColoredStringBasic(name_x, name_y, buffer, 0, text_scale, proportional);
				}
				else if (team_colours_for_name) {
					char buffer[MAX_SCOREBOARDNAME + 8];

					byte red = host_basepal[player_color * 3];
					byte green = host_basepal[player_color * 3 + 1];
					byte blue = host_basepal[player_color * 3 + 2];

					snprintf(buffer, sizeof(buffer), "&c%x%x%x%s",
						(unsigned int)(red / 16),
						(unsigned int)(green / 16),
						(unsigned int)(blue / 16),
						info->name
					);

					// Draw the tracked players name in the user specified color.
					Draw_SColoredStringBasic(name_x, name_y, buffer, 0, text_scale, proportional);
				}
				else {
					// Draw other players in normal character color.
					Draw_SString (name_x, name_y, info->name, text_scale, proportional);
				}
			}

			// Show if a person lost an RL-pack.
			if (info->stats[STAT_HEALTH] <= 0 && info->stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
			{
				Draw_AlphaCircleOutline (x + player_p_x, y + player_p_y, player_size*player_z_relative*2, 1.0, 254, player_alpha);
				Draw_SColoredStringBasic (x + player_p_x, y + player_p_y, va("&cf00PACK!"), 1, text_scale, proportional);
			}
		}
	}
}

//
// Draws a map of the current level and plots player movements on it.
//
void SCR_HUD_DrawRadar(hud_t *hud)
{
	int width, height, x, y;
	float width_limit, height_limit;
	float scale;
	float x_scale;
	float y_scale;

	static cvar_t
		*hud_radar_opacity = NULL,
		*hud_radar_width,
		*hud_radar_height,
		*hud_radar_autosize,
		*hud_radar_fade_players,
		*hud_radar_show_powerups,
		*hud_radar_show_names,
		*hud_radar_highlight,
		*hud_radar_highlight_color,
		*hud_radar_player_size,
		*hud_radar_show_height,
		*hud_radar_show_stats,
		*hud_radar_show_hold,
		*hud_radar_weaponfilter,
		*hud_radar_itemfilter,
		*hud_radar_onlytp,
		*hud_radar_otherfilter,
		*hud_radar_scale,
		*hud_radar_simple,
		*hud_radar_colour_names,
		*hud_radar_proportional;

	if (hud_radar_opacity == NULL)    // first time
	{
		char checkval[256];

		hud_radar_opacity			= HUD_FindVar(hud, "opacity");
		hud_radar_width				= HUD_FindVar(hud, "width");
		hud_radar_height			= HUD_FindVar(hud, "height");
		hud_radar_autosize			= HUD_FindVar(hud, "autosize");
		hud_radar_fade_players		= HUD_FindVar(hud, "fade_players");
		hud_radar_show_powerups		= HUD_FindVar(hud, "show_powerups");
		hud_radar_show_names		= HUD_FindVar(hud, "show_names");
		hud_radar_player_size		= HUD_FindVar(hud, "player_size");
		hud_radar_show_height		= HUD_FindVar(hud, "show_height");
		hud_radar_show_stats		= HUD_FindVar(hud, "show_stats");
		hud_radar_show_hold			= HUD_FindVar(hud, "show_hold");
		hud_radar_weaponfilter		= HUD_FindVar(hud, "weaponfilter");
		hud_radar_itemfilter		= HUD_FindVar(hud, "itemfilter");
		hud_radar_otherfilter		= HUD_FindVar(hud, "otherfilter");
		hud_radar_onlytp			= HUD_FindVar(hud, "onlytp");
		hud_radar_highlight			= HUD_FindVar(hud, "highlight");
		hud_radar_highlight_color	= HUD_FindVar(hud, "highlight_color");
		hud_radar_scale             = HUD_FindVar(hud, "scale");
		hud_radar_simple            = HUD_FindVar(hud, "simpleitems");
		hud_radar_colour_names      = HUD_FindVar(hud, "colornames");
		hud_radar_proportional      = HUD_FindVar(hud, "proportional");

		//
		// Only parse the the filters when they change, not on each frame.
		//

		// Weapon filter.
		hud_radar_weaponfilter->OnChange = Radar_OnChangeWeaponFilter;
		strlcpy(checkval, hud_radar_weaponfilter->string, sizeof(checkval));
		Cvar_Set(hud_radar_weaponfilter, checkval);

		// Item filter.
		hud_radar_itemfilter->OnChange = Radar_OnChangeItemFilter;
		strlcpy(checkval, hud_radar_itemfilter->string, sizeof(checkval));
		Cvar_Set(hud_radar_itemfilter, checkval);

		// Other filter.
		hud_radar_otherfilter->OnChange = Radar_OnChangeOtherFilter;
		strlcpy(checkval, hud_radar_otherfilter->string, sizeof(checkval));
		Cvar_Set(hud_radar_otherfilter, checkval);

		// Highlight color.
		hud_radar_highlight_color->OnChange = Radar_OnChangeHighlightColor;
		strlcpy(checkval, hud_radar_highlight_color->string, sizeof(checkval));
		Cvar_Set(hud_radar_highlight_color, checkval);
	}

	// Don't show anything if it's a normal player.
	if(!(cls.demoplayback || cl.spectator))
	{
		HUD_PrepareDraw(hud, hud_radar_width->value, hud_radar_height->value, &x, &y);
		return;
	}

	// Don't show when not in teamplay/demoplayback.
	if(!HUD_ShowInDemoplayback(hud_radar_onlytp->value))
	{
		HUD_PrepareDraw(hud, hud_radar_width->value, hud_radar_height->value, &x, &y);
		return;
	}

	// Save the width and height of the HUD. We're using these because
	// if autosize is on these will be altered and we don't want to change
	// the settings that the user set, if we try, and the user turns off
	// autosize again the size of the HUD will remain "autosized" until the user
	// resets it by hand again.
	width_limit = hud_radar_width->value;
	height_limit = hud_radar_height->value;

	// we support also sizes specified as a percentage of total screen width/height
	if (strchr(hud_radar_width->string, '%')) {
		width_limit = width_limit * vid.conwidth / 100.0;
	}
	if (strchr(hud_radar_height->string, '%')) {
		height_limit = hud_radar_height->value * vid.conheight / 100.0;
	}

	// This map doesn't have a map pic.
	if (!radar_pic_found) {
		if (HUD_PrepareDraw(hud, Q_rint(width_limit), Q_rint(height_limit), &x, &y)) {
			Draw_SString(x, y, "No radar picture found!", hud_radar_scale->value, hud_radar_proportional->integer);
		}
		return;
	}

	// Make sure we can translate the coordinates.
	if (!conversion_formula_found) {
		if (HUD_PrepareDraw(hud, Q_rint(width_limit), Q_rint(height_limit), &x, &y)) {
			Draw_SString(x, y, "No conversion formula found!", hud_radar_scale->value, hud_radar_proportional->integer);
		}
		return;
	}

	x = 0;
	y = 0;

	scale = 1;

	if (hud_radar_autosize->value) {
		//
		// Autosize the hud element based on the size of the radar picture.
		//

		width = width_limit = radar_pic.width;
		height = height_limit = radar_pic.height;
	}
	else {
		//
		// Size the picture so that it fits inside the hud element.
		//

		// Set the scaling based on the picture dimensions.
		x_scale = (width_limit / radar_pic.width);
		y_scale = (height_limit / radar_pic.height);

		scale = (x_scale < y_scale) ? x_scale : y_scale;

		width = radar_pic.width * scale;
		height = radar_pic.height * scale;
	}

	if (HUD_PrepareDraw(hud, Q_rint(width_limit), Q_rint(height_limit), &x, &y)) {
		float player_size = 1.0;
		static int lastframecount = -1;

		// Place the map picture in the center of the HUD element.
		x += Q_rint((width_limit / 2.0) - (width / 2.0));
		x = max(0, x);
		x = min(x + width, x);

		y += Q_rint((height_limit / 2.0) - (height / 2.0));
		y = max(0, y);
		y = min(y + height, y);

		// Draw the radar background.
		Draw_SAlphaPic(x, y, &radar_pic, hud_radar_opacity->value, scale);

		// Only draw once per frame.
		if (cls.framecount == lastframecount) {
			return;
		}
		lastframecount = cls.framecount;

		if (!cl.oldparsecount || !cl.parsecount || cls.state < ca_active) {
			return;
		}

		// Scale the player size after the size of the radar.
		player_size = hud_radar_player_size->value * scale;

		// Draw team stats.
		if (hud_radar_show_stats->value) {
			Radar_DrawGrid(stats_grid, x, y, scale, width, height, hud_radar_show_stats->value);
		}

		// Draw entities such as powerups, weapons and backpacks.
		if (RADAR_SHOW_WEAPONS || RADAR_SHOW_ITEMS || RADAR_SHOW_OTHER) {
			Radar_DrawEntities(
				x, y, scale, player_size,
				hud_radar_show_hold->value, hud_radar_scale->value, hud_radar_simple->value, hud_radar_proportional->integer
			);
		}

		// Draw the players.
		Radar_DrawPlayers(
			x, y, width, height, scale,
			hud_radar_show_height->value,
			hud_radar_show_powerups->value,
			player_size,
			hud_radar_show_names->value,
			hud_radar_fade_players->value,
			hud_radar_highlight->value,
			hud_radar_highlight_color->string,
			hud_radar_scale->value,
			hud_radar_colour_names->integer,
			hud_radar_proportional->integer
		);
	}
}

#endif // WITH_PNG

void Radar_HudInit(void)
{
#ifdef WITH_PNG
	HUD_Register("radar", NULL, "Plots the players on a picture of the map. (Only when watching MVD's or QTV).",
		HUD_PLUSMINUS, ca_active, 0, SCR_HUD_DrawRadar,
		"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
		"opacity", "0.5",
		"width", "30%",
		"height", "25%",
		"autosize", "0",
		"show_powerups", "1",
		"show_names", "0",
		"highlight_color", "yellow",
		"highlight", "0",
		"player_size", "10",
		"show_height", "1",
		"show_stats", "1",
		"fade_players", "1",
		"show_hold", "0",
		"weaponfilter", "gl rl lg",
		"itemfilter", "backpack quad pent armor mega",
		"otherfilter", "projectiles gibs explosions shotgun",
		"onlytp", "0",
		"scale", "1",
		"simpleitems", "1",
		"colornames", "0",
		"proportional", "0",
		NULL);
#endif
}

