/**
	
	HUD Editor module

	made by jogihoogi, Feb 2007
	last edit:
	$Id: hud_editor.c,v 1.12 2007-03-01 23:23:22 cokeman1982 Exp $

*/

#include "quakedef.h"
#include "hud.h"
#include "EX_misc.h"
#include "cl_screen.h"
#include "hud_editor.h"

#ifdef GLQUAKE
extern hud_t		*hud_huds;				// The list of HUDs.
hud_t				*last_moved_hud = NULL;	// The HUD that was being moved during the last call of HUD_Editor()
hud_t				*selected_hud = NULL;	// The currently selected HUD.
int					win_x;
int					win_y;
vec3_t				points[3];
vec3_t				corners[4];
vec3_t				center;
vec3_t				pointer;

qbool				hud_editor = false;		// If we're in HUD editor mode or not.
hud_editor_mode_t	hud_editor_mode = hud_editmode_off;
hud_editor_mode_t	hud_editor_prevmode =  hud_editmode_off;

// Cursor location.
double				hud_mouse_x;			// The screen coordinates of the mouse cursor.
double				hud_mouse_y;

// Macros for what mouse buttons are clicked.
#define MOUSEDOWN_1_2		( keydown[K_MOUSE1] &&  keydown[K_MOUSE2])
#define MOUSEDOWN_1_3		( keydown[K_MOUSE1] &&  keydown[K_MOUSE3])
#define MOUSEDOWN_1_2_3		( keydown[K_MOUSE1] &&  keydown[K_MOUSE2] &&  keydown[K_MOUSE3])
#define MOUSEDOWN_1_ONLY	( keydown[K_MOUSE1] && !keydown[K_MOUSE2] && !keydown[K_MOUSE3])
#define MOUSEDOWN_2_ONLY	(!keydown[K_MOUSE1] &&  keydown[K_MOUSE2] && !keydown[K_MOUSE3])
#define MOUSEDOWN_3_ONLY	(!keydown[K_MOUSE1] && !keydown[K_MOUSE2] &&  keydown[K_MOUSE3])
#define MOUSEDOWN_1_2_ONLY	( keydown[K_MOUSE1] &&  keydown[K_MOUSE2] && !keydown[K_MOUSE3])
#define MOUSEDOWN_1_3_ONLY	( keydown[K_MOUSE1] && !keydown[K_MOUSE2] &&  keydown[K_MOUSE3])
#define MOUSEDOWN_2_3_ONLY	(!keydown[K_MOUSE1] &&  keydown[K_MOUSE2] &&  keydown[K_MOUSE3])
#define MOUSEDOWN_NONE		(!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && !keydown[K_MOUSE3])

#define HUD_CENTER_X(h)		((h)->lx + (h)->lw / 2)
#define HUD_CENTER_Y(h)		((h)->ly + (h)->lh / 2)

#define HUD_ALIGN_POLYCOUNT_CORNER	5
#define HUD_ALIGN_POLYCOUNT_EDGE	4
#define HUD_ALIGN_POLYCOUNT_CENTER	8

vec3_t *hud_align_current_poly = NULL;	// The current alignment polygon when in alignment mode.
int hud_align_current_polycount = 0;	// Number of vertices in teh polygon.

vec3_t hud_align_topright_poly[HUD_ALIGN_POLYCOUNT_CORNER];
vec3_t hud_align_top_poly[HUD_ALIGN_POLYCOUNT_EDGE];
vec3_t hud_align_topleft_poly[HUD_ALIGN_POLYCOUNT_CORNER];
vec3_t hud_align_left_poly[HUD_ALIGN_POLYCOUNT_EDGE];
vec3_t hud_align_bottomleft_poly[HUD_ALIGN_POLYCOUNT_CORNER];
vec3_t hud_align_bottom_poly[HUD_ALIGN_POLYCOUNT_EDGE];
vec3_t hud_align_bottomright_poly[HUD_ALIGN_POLYCOUNT_CORNER];
vec3_t hud_align_right_poly[HUD_ALIGN_POLYCOUNT_EDGE];
vec3_t hud_align_center_poly[HUD_ALIGN_POLYCOUNT_CENTER];

typedef enum hud_alignmode_s
{
	hud_align_center,
	hud_align_top,
	hud_align_topleft,
	hud_align_left,	
	hud_align_bottomleft,
	hud_align_bottom,
	hud_align_bottomright,
	hud_align_right,
	hud_align_topright
	// Not including before/after for now.
} hud_alignmode_t;

hud_alignmode_t				hud_alignmode = align_center;
int							hud_placement = HUD_PLACE_SCREEN;

typedef enum hud_greppos_s
{
	pos_visible,
	pos_top,
	pos_left,
	pos_bottom,
	pos_right
} hud_greppos_t;

typedef struct hud_grephandle_s
{
	hud_t					*hud;
	int						x;
	int						y;
	int						width;
	int						height;
	qbool					highlighted;
	hud_greppos_t			pos;
	struct hud_grephandle_s	*next;
	struct hud_grephandle_s	*previous;
} hud_grephandle_t;

hud_grephandle_t *hud_greps = NULL;	// The list of "grep handles" that are shown if a HUD element is moved offscreen.

//
// Sets a new HUD Editor mode (and saves the previous one).
//
static void HUD_Editor_SetMode(hud_editor_mode_t newmode)
{
	hud_editor_prevmode = hud_editor_mode;
	hud_editor_mode = newmode;
}

//
// Draws a tool tip with a background next to the cursor.
//
static void HUD_Editor_DrawTooltip(int x, int y, char *string, float r, float g, float b, float a) 
{
	int len = strlen(string) * 8;

	// Make sure we're drawing within the screen.
	if (x + len > vid.width)
	{
		x -= len;
	}

	if (y + 9 > vid.height)
	{
		y -= 9;
	}

	Draw_AlphaFillRGB(x, y, len, 8, r, g, b, a);
	Draw_String(x, y, string);
}

//
// Gets an alignment string for a specified alignmode enum.
//
static char *HUD_Editor_GetAlignmentString(hud_alignmode_t align)
{
	switch(hud_alignmode)
	{
		case hud_align_center :		return "center center";
		case hud_align_top :		return "center top";
		case hud_align_topleft :	return "left top";
		case hud_align_left :		return "left center";
		case hud_align_bottomleft :	return "left bottom";
		case hud_align_bottom :		return "center bottom";
		case hud_align_bottomright :return "right bottom";
		case hud_align_right :		return "right center";
		case hud_align_topright :	return "right top";
		default :					return "";
	}
}

//
// Returns an alignment enum based on a specified alignment string.
//
static hud_alignmode_t HUD_Editor_GetAlignmentFromString(char *alignstr)
{
	if(!strcmp(alignstr, "center center"))
	{
		return hud_align_center;
	}
	else if(!strcmp(alignstr, "center top"))
	{
		return hud_align_top;
	}
	else if(!strcmp(alignstr, "left top"))
	{
		return hud_align_topleft;
	}
	else if(!strcmp(alignstr, "left center"))
	{
		return hud_align_left;
	}
	else if(!strcmp(alignstr, "left bottom"))
	{
		return hud_align_bottomleft;
	}
	else if(!strcmp(alignstr, "center bottom"))
	{
		return hud_align_bottom;
	}
	else if(!strcmp(alignstr, "right bottom"))
	{
		return hud_align_bottomright;
	}
	else if(!strcmp(alignstr, "right center"))
	{
		return hud_align_right;
	}
	else if(!strcmp(alignstr, "right top"))
	{
		return hud_align_topright;
	}
}

//
// Returns the alignment we are trying to align the selected HUD to at it's placement
// when in alignment mode. 
//
static hud_alignmode_t HUD_Editor_GetAlignment(int x, int y, hud_t *hud_element)
{
	extern qbool IsPointInPolygon(int npol, vec3_t *v, float x, float y);
	
	// For less clutter.
	float mid_x = hud_element->lw / 2.0;
	float mid_y = hud_element->lh / 2.0;

	// Top right.
	{
		hud_align_topright_poly[0][0] = mid_x + (mid_x / 2.0);
		hud_align_topright_poly[0][1] = 0;
		hud_align_topright_poly[0][2] = 0;

		hud_align_topright_poly[1][0] = mid_x + (mid_x / 4.0);
		hud_align_topright_poly[1][1] = mid_y - (mid_y / 2.0);
		hud_align_topright_poly[1][2] = 0;

		hud_align_topright_poly[2][0] = mid_x + (mid_x / 2.0);
		hud_align_topright_poly[2][1] = mid_y - (mid_y / 4.0);
		hud_align_topright_poly[2][2] = 0;

		hud_align_topright_poly[3][0] = 2 * mid_x;
		hud_align_topright_poly[3][1] = mid_y - (mid_y / 2.0);;
		hud_align_topright_poly[3][2] = 0;

		hud_align_topright_poly[4][0] = 2 * mid_x;
		hud_align_topright_poly[4][1] = 0;
		hud_align_topright_poly[4][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_CORNER, hud_align_topright_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_topright_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CORNER;
			return hud_align_topright;
		}
	}
	
	// Top.
	{
		hud_align_top_poly[0][0] = mid_x / 2.0;
		hud_align_top_poly[0][1] = 0;
		hud_align_top_poly[0][2] = 0;

		hud_align_top_poly[1][0] = mid_x - (mid_x / 4.0);
		hud_align_top_poly[1][1] = mid_y - (mid_y / 2.0);
		hud_align_top_poly[1][2] = 0;

		hud_align_top_poly[2][0] = mid_x + (mid_x / 4.0);
		hud_align_top_poly[2][1] = mid_y - (mid_y / 2.0);
		hud_align_top_poly[2][2] = 0;

		hud_align_top_poly[3][0] = mid_x + (mid_x / 2.0);
		hud_align_top_poly[3][1] = 0;
		hud_align_top_poly[3][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_EDGE, hud_align_top_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_top_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_EDGE;
			return hud_align_top;
		}
	}

	// Top Left.
	{
		hud_align_topleft_poly[0][0] = 0;
		hud_align_topleft_poly[0][1] = 0;
		hud_align_topleft_poly[0][2] = 0;

		hud_align_topleft_poly[1][0] = 0;
		hud_align_topleft_poly[1][1] = mid_y - (mid_y / 2.0);
		hud_align_topleft_poly[1][2] = 0;

		hud_align_topleft_poly[2][0] = mid_x - (mid_x / 2.0);
		hud_align_topleft_poly[2][1] = mid_y - (mid_y / 4.0);
		hud_align_topleft_poly[2][2] = 0;

		hud_align_topleft_poly[3][0] = mid_x - (mid_x / 4.0);
		hud_align_topleft_poly[3][1] = mid_y - (mid_y / 2.0);
		hud_align_topleft_poly[3][2] = 0;

		hud_align_topleft_poly[4][0] = mid_x - (mid_x / 2.0);
		hud_align_topleft_poly[4][1] = 0;
		hud_align_topleft_poly[4][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_CORNER, hud_align_topleft_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_topleft_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CORNER;
			return hud_align_topleft;
		}
	}

	// Left.
	{
		hud_align_left_poly[0][0] = 0;
		hud_align_left_poly[0][1] = mid_y / 2.0;
		hud_align_left_poly[0][2] = 0;

		hud_align_left_poly[1][0] = 0;
		hud_align_left_poly[1][1] = mid_y + (mid_y / 2.0);
		hud_align_left_poly[1][2] = 0;

		hud_align_left_poly[2][0] = mid_x / 2.0;
		hud_align_left_poly[2][1] = mid_y + (mid_y / 4.0);
		hud_align_left_poly[2][2] = 0;

		hud_align_left_poly[3][0] = mid_x / 2.0;
		hud_align_left_poly[3][1] = mid_y - (mid_y / 4.0);
		hud_align_left_poly[3][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_EDGE, hud_align_left_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_left_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_EDGE;
			return hud_align_left;
		}
	}

	// Bottom Left.
	{
		hud_align_bottomleft_poly[0][0] = 0;
		hud_align_bottomleft_poly[0][1] = mid_y + (mid_y / 2.0);
		hud_align_bottomleft_poly[0][2] = 0;

		hud_align_bottomleft_poly[1][0] = 0;
		hud_align_bottomleft_poly[1][1] = 2 * mid_y;
		hud_align_bottomleft_poly[1][2] = 0;

		hud_align_bottomleft_poly[2][0] = mid_x / 2.0;
		hud_align_bottomleft_poly[2][1] = 2 * mid_y;
		hud_align_bottomleft_poly[2][2] = 0;

		hud_align_bottomleft_poly[3][0] = mid_x - (mid_x / 4.0);
		hud_align_bottomleft_poly[3][1] = mid_y + (mid_y / 2.0);
		hud_align_bottomleft_poly[3][2] = 0;

		hud_align_bottomleft_poly[4][0] = mid_x / 2.0;
		hud_align_bottomleft_poly[4][1] = mid_y + (mid_y / 4.0);
		hud_align_bottomleft_poly[4][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_CORNER, hud_align_bottomleft_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_bottomleft_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CORNER;
			return hud_align_bottomleft;
		}
	}

	// Bottom.
	{
		hud_align_bottom_poly[0][0] = mid_x - (mid_x / 2.0);
		hud_align_bottom_poly[0][1] = 2 * mid_y;
		hud_align_bottom_poly[0][2] = 0;

		hud_align_bottom_poly[1][0] = mid_x + (mid_x / 2.0);
		hud_align_bottom_poly[1][1] = 2 * mid_y;
		hud_align_bottom_poly[1][2] = 0;

		hud_align_bottom_poly[2][0] = mid_x + (mid_x / 4.0);
		hud_align_bottom_poly[2][1] = mid_y + (mid_y / 2.0);
		hud_align_bottom_poly[2][2] = 0;

		hud_align_bottom_poly[3][0] = mid_x - (mid_x / 4.0);
		hud_align_bottom_poly[3][1] = mid_y + (mid_y / 2.0);
		hud_align_bottom_poly[3][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_EDGE, hud_align_bottom_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_bottom_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_EDGE;
			return hud_align_bottom;
		}
	}

	// Bottom Right.
	{
		hud_align_bottomright_poly[0][0] = mid_x + (mid_x / 2.0);
		hud_align_bottomright_poly[0][1] = 2 * mid_y;
		hud_align_bottomright_poly[0][2] = 0;

		hud_align_bottomright_poly[1][0] = 2 * mid_x;
		hud_align_bottomright_poly[1][1] = 2 * mid_y;
		hud_align_bottomright_poly[1][2] = 0;

		hud_align_bottomright_poly[2][0] = 2 * mid_x;
		hud_align_bottomright_poly[2][1] = mid_y + (mid_y / 2.0);
		hud_align_bottomright_poly[2][2] = 0;

		hud_align_bottomright_poly[3][0] = mid_x + (mid_x / 2.0);
		hud_align_bottomright_poly[3][1] = mid_y + (mid_y / 4.0);
		hud_align_bottomright_poly[3][2] = 0;

		hud_align_bottomright_poly[4][0] = mid_x + (mid_x / 4.0);
		hud_align_bottomright_poly[4][1] = mid_y + (mid_y / 2.0);
		hud_align_bottomright_poly[4][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_CORNER, hud_align_bottomright_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_bottomright_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CORNER;
			return hud_align_bottomright;
		}
	}

	// Right.
	{
		hud_align_right_poly[0][0] = 2 * mid_x;
		hud_align_right_poly[0][1] = mid_y + (mid_y / 2.0);
		hud_align_right_poly[0][2] = 0;

		hud_align_right_poly[1][0] = 2 * mid_x;
		hud_align_right_poly[1][1] = mid_y / 2.0;
		hud_align_right_poly[1][2] = 0;

		hud_align_right_poly[2][0] = mid_x + (mid_x / 2.0);
		hud_align_right_poly[2][1] = mid_y - (mid_y / 4.0);
		hud_align_right_poly[2][2] = 0;

		hud_align_right_poly[3][0] = mid_x + (mid_x / 2.0);
		hud_align_right_poly[3][1] = mid_y + (mid_y / 4.0);
		hud_align_right_poly[3][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_EDGE, hud_align_right_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_right_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_EDGE;
			return hud_align_right;
		}
	}

	// Center.
	{
		hud_align_center_poly[0][0] = mid_x - (mid_x / 4.0);
		hud_align_center_poly[0][1] = mid_y / 2.0;
		hud_align_center_poly[0][2] = 0;

		hud_align_center_poly[1][0] = mid_x / 2.0;
		hud_align_center_poly[1][1] = mid_y - (mid_y / 4.0);
		hud_align_center_poly[1][2] = 0;

		hud_align_center_poly[2][0] = mid_x / 2.0;
		hud_align_center_poly[2][1] = mid_y + (mid_y / 4.0);
		hud_align_center_poly[2][2] = 0;

		hud_align_center_poly[3][0] = mid_x - (mid_x / 4.0);
		hud_align_center_poly[3][1] = mid_y + (mid_y / 2.0);
		hud_align_center_poly[3][2] = 0;

		hud_align_center_poly[4][0] = mid_x + (mid_x / 4.0);
		hud_align_center_poly[4][1] = mid_y + (mid_y / 2.0);
		hud_align_center_poly[4][2] = 0;

		hud_align_center_poly[5][0] = mid_x + (mid_x / 2.0);
		hud_align_center_poly[5][1] = mid_y + (mid_y / 4.0);
		hud_align_center_poly[5][2] = 0;

		hud_align_center_poly[6][0] = mid_x + (mid_x / 2.0);
		hud_align_center_poly[6][1] = mid_y - (mid_y / 4.0);
		hud_align_center_poly[6][2] = 0;

		hud_align_center_poly[7][0] = mid_x + (mid_x / 4.0);
		hud_align_center_poly[7][1] = mid_y / 2.0;
		hud_align_center_poly[7][2] = 0;

		if (IsPointInPolygon(HUD_ALIGN_POLYCOUNT_CENTER, hud_align_center_poly, x - hud_element->lx, y - hud_element->ly))
		{
			hud_align_current_poly = hud_align_center_poly;
			hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CENTER;
			return hud_align_center;
		}
	}

	// Default to center.
	hud_align_current_poly = hud_align_center_poly;
	hud_align_current_polycount = HUD_ALIGN_POLYCOUNT_CENTER;
	return hud_align_center;
}

//
// Finds the next child of the specified HUD element.
//
static hud_t *HUD_Editor_FindNextChild(hud_t *hud_element)
{
	static hud_t	*hud_it = NULL;
	static hud_t	*parent = NULL;
	hud_t			*hud_result = NULL;

	// Reset everything if we're given a NULL argument.
	if(!hud_element)
	{
		hud_it = NULL;
		parent = NULL;
		return NULL;
	}

	// There's a new parent so start searching from the beginning.
	if(!parent || strcmp(parent->name, hud_element->name))
	{
		parent = hud_element;
		hud_it = hud_huds;
	}

	while(hud_it)
	{
		// Check if this HUD is placed at the parent, if so we've found a child.
		if(hud_it->place_hud && !strcmp(hud_it->place_hud->name, parent->name))
		{
			hud_result = hud_it;
			hud_it = hud_it->next;
			return hud_result;
		}

		hud_it = hud_it->next;
	}

	parent = NULL;

	// No children found.
	return NULL;
}

//
// Moves a HUD element.
//
static void HUD_Editor_Move(float dx, float dy, hud_t *hud_element) 
{
	Cvar_Set(hud_element->pos_x, va("%f", hud_element->pos_x->value + dx));
	Cvar_Set(hud_element->pos_y, va("%f", hud_element->pos_y->value + dy));
}

//
// Check if we're supposed to be moving anything.
//
static qbool HUD_Editor_Moving(hud_t *hud)
{
	// Mouse delta (in_win.c)
	extern float mouse_x, mouse_y;

	// Left mousebutton down = lets move it !
	if (hud_editor_mode == hud_editmode_move_resize) //MOUSEDOWN_1_ONLY && hud)
	{
		// Move using the mouse delta instead of the absolute
		// mouse cursor coordinates on the screen.
		HUD_Editor_Move(mouse_x, mouse_y, hud);
		last_moved_hud = hud;
		HUD_Recalculate();
		return true;
	}

	// Should we stop after this?
	return false;
}

//
// Draw the current alignment.
//
static void HUD_Editor_DrawAlignment(hud_t *hud)
{
	extern void Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, int color);

	if(hud)
	{
		Draw_Polygon(hud->lx, hud->ly, hud_align_current_poly, hud_align_current_polycount, true, RGBA_2_Int(255, 255, 0, 50));
	}
	else
	{
		// TODO: Show alignment for non-HUDs
	}
}

//
// Handles input from mouse if in alignment mode.
//
static qbool HUD_Editor_Aligning(hud_t *hud_hover)
{
	if(hud_editor_mode == hud_editmode_align)
	{
		// If we just entered alignment mode, nothing is selected
		// so select the hud we're hovering to start with.
		if(!selected_hud && hud_hover)
		{
			selected_hud = hud_hover;
			return true;
		}

		// We have something selected so show some visual
		// feedback for when aligning to the user.
		if(selected_hud)
		{
			if(selected_hud->place_hud)
			{
				hud_alignmode = HUD_Editor_GetAlignment(hud_mouse_x, hud_mouse_y, selected_hud->place_hud);
				HUD_Editor_DrawAlignment(selected_hud->place_hud);
			}
			else
			{
				// TODO : Align to screen/console
			}
		}
	}
	else if(hud_editor_prevmode == hud_editmode_align && isAltDown())
	{
		// The user just released the mousebutton but is still holding
		// down ALT. If the user releases ALT before the mouse button
		// the operation will be cancelled. So commit the users actions.

		// We must have something to align.
		if(selected_hud)
		{
			if(selected_hud->place_hud)
			{
				// Placed another HUD.

				// Reset position.
				Cvar_Set(selected_hud->pos_x, "0");
				Cvar_Set(selected_hud->pos_y, "0");

				// Align to the area the mouse is placed over (previously set in hud_alignmode).
				Cbuf_AddText(va("align %s %s", selected_hud->name, HUD_Editor_GetAlignmentString(hud_alignmode)));
				HUD_Recalculate();

				// Free selection.
				selected_hud = NULL;
				return true;
			}
			else
			{
				// Not placing at a HUD.
				selected_hud = NULL;
			}
		}
	}

	return false;
}

//
// Handles feedback/commiting of actions when in placement mode.
//
static qbool HUD_Editor_Placing(hud_t *hud_hover)
{
	if(hud_editor_mode == hud_editmode_place)
	{
		// If we just entered placement mode, nothing is selected
		// so select the hud we're hovering to start with.
		if(!selected_hud && hud_hover)
		{
			selected_hud = hud_hover;
			return true;
		}
		
		if(selected_hud)
		{
			// Find the center of the selected HUD so we know where
			// to draw the line from.

			if(hud_hover)
			{
				// We're trying to place the HUD on itself or on the HUD it's already placed at.
				if(hud_hover == selected_hud || (selected_hud->place_hud && selected_hud->place_hud == hud_hover))
				{
					// Red "not allowed".
					Draw_AlphaRectangleRGB(hud_hover->lx, hud_hover->ly, hud_hover->lw, hud_hover->lh, 1, 0, 0, 1, true, 0.1);
					Draw_AlphaRectangleRGB(hud_hover->lx, hud_hover->ly, hud_hover->lw, hud_hover->lh, 1, 0, 0, 1, false, 1);
				}
				else
				{
					// Green "allowed" placement.
					Draw_AlphaRectangleRGB(hud_hover->lx, hud_hover->ly, hud_hover->lw, hud_hover->lh, 0, 1, 0, 1, true, 0.1);
				}

				return true;
			}
		}
	}
	else if(hud_editor_prevmode == hud_editmode_place && isCtrlDown())
	{
		// We've just exited placement mode, but control is still pressed,
		// that means we should place the selected_hud.
		// (If you release ctrl before you release Mouse 1, you cancel the place operation).

		if(selected_hud)
		{
			// If we're hovering a HUD place it there.
			if(hud_hover)
			{
				// We're trying to place the HUD on itself or on the HUD it's already placed at so do nothing.
				if(hud_hover == selected_hud || (selected_hud->place_hud && selected_hud->place_hud == hud_hover))
				{
					return true;
				}

				// Place at other HUD.
				Cvar_Set(selected_hud->align_x, "center");
				Cvar_Set(selected_hud->align_y, "center");
				Cvar_Set(selected_hud->pos_x, "0");
				Cvar_Set(selected_hud->pos_y, "0");
				Cvar_Set(selected_hud->place, hud_hover->name);
				HUD_Recalculate();

				// Free selection.
				selected_hud = NULL;
				return true;
			}

			// Mouse button was released on "non-occupied space", 
			// either screen or console (Don't bother with all the obscure ones like IFREE and so on).
			// TODO: Place @Screen/console
		}
	}

	// We weren't placing something so check other states also.
	return false;
}

// =============================================================================
//							HUD Editor Grep Handles.
// =============================================================================
// These show up when a HUD element is moved completly offscreen (by accident
// most likely), so that the user can still grab it and move it back onto the
// screen again.
// =============================================================================

//
// Finds a HUD grephandle (if the hud element has gone offscreen).
//
static hud_grephandle_t *HUD_Editor_FindGrep(hud_t *hud_element)
{
	hud_grephandle_t *greps_it = NULL;
	greps_it = hud_greps;

	while(greps_it)
	{
		if(!strcmp(greps_it->hud->name, hud_element->name))
		{
			return greps_it;
		}

		greps_it = greps_it->next;
	}

	return NULL;
}

//
// Returns an "offscreen" arrow in the correct direction based on where
// offscreen the HUD element is located.
//
static char *HUD_Editor_GetGrepArrow(hud_grephandle_t *grep)
{
	switch(grep->pos)
	{
		case pos_top :		return "/\\";
		case pos_bottom :	return "\\/";
		case pos_left :		return "<<<";
		case pos_right :	return ">>>";
		default :			return "";
	}
}

//
// Draws the grephandles.
//
static void HUD_Editor_DrawGreps()
{
	clrinfo_t color, highlight;
	hud_grephandle_t *greps_it = NULL;
	greps_it = hud_greps;

	// Highlight color (yellow).
	highlight.c = RGBA_2_Int(0, 255, 0, 255);
	highlight.i = 0;

	// Orange.
	color.c = RGBA_2_Int(255, 150, 0, 255);
	color.i = 0;

	while(greps_it)
	{
		Draw_ColoredString3(greps_it->x, greps_it->y, 
			va("%s %s", HUD_Editor_GetGrepArrow(greps_it), greps_it->hud->name), 
			(greps_it->highlighted ? &highlight : &color), 1, 0);

		greps_it = greps_it->next;
	}
}

//
// Get's the position offscreen for a HUD element
// left/right/top/bottom or visible if it's not offscreen.
//
static hud_greppos_t HUD_Editor_GetHudGrepPosition(hud_t *hud)
{
	if(hud->lx + hud->lw <= 0)
	{
		return pos_left;
	}
	else if(hud->lx >= (signed)vid.width)
	{
		return pos_right;
	}
	else if(hud->ly + hud->lh <= 0)
	{
		return pos_top;
	}
	else if(hud->ly >= (signed)vid.height)
	{
		return pos_bottom;
	}
	
	return pos_visible;
}

//
// Positions a grephandle based on it's position and where the HUD element
// it's associated with is located.
//
static void HUD_Editor_PositionGrep(hud_t *hud_element, hud_grephandle_t *grep)
{
	// Get the position of the grephandle.
	grep->pos = HUD_Editor_GetHudGrepPosition(hud_element);

	// Position the grephandle on screen.
	switch(grep->pos)
	{
		case pos_top :
			grep->x	= hud_element->lx;
			grep->y	= 5;
			break;
		case pos_bottom :
			grep->x	= hud_element->lx;
			grep->y	= vid.height - grep->height - 5;
			break;
		case pos_left :
			grep->x = 5;
			grep->y = hud_element->ly;
			break;
		case pos_right :
			grep->x = vid.width - grep->width - 5;
			grep->y = hud_element->ly;
			break;
		default :
			grep->x = hud_element->lx;
			grep->y = hud_element->ly;
			break;
	}
}

//
// Creates a new grephandle and associates it with a HUD element´that is offscreen.
//
static hud_grephandle_t *HUD_Editor_CreateGrep(hud_t *hud_element)
{
	hud_grephandle_t *grep = NULL;

	grep			= Q_malloc(sizeof(hud_grephandle_t));
	memset(grep, 0, sizeof(grep));

	grep->width		= 8 * (4 + strlen(hud_element->name));
	grep->height	= 8;
	grep->hud		= hud_element;
	grep->previous	= NULL;
	grep->next		= hud_greps;

	hud_greps		= grep;

	HUD_Editor_PositionGrep(hud_element, grep);

	return grep;
}

//
// Destroys a grephandle (called if it's no longer offscreen).
//
static void HUD_Editor_DestroyGrep(hud_grephandle_t *grep)
{
	// Already destroyed.
	if(!grep)
	{
		return;
	}

	// Relink any neighbours in the list.
	if(grep->next && grep->previous)
	{
		grep->previous->next = grep->next;
		grep->next->previous = grep->previous;
	}
	else if(grep->next)
	{
		grep->next->previous = NULL;
		hud_greps = grep->next;
	}
	else if(grep->previous)
	{
		grep->previous->next = NULL;
	}
	else
	{
		hud_greps = NULL;
	}

	memset(grep, 0, sizeof(grep));

	Q_free(grep);
}

//
// Finds a HUD element associated with the grephandle under the mouse cursor.
//
static hud_t *HUD_Editor_FindHudByGrep()
{
	hud_grephandle_t *greps_it = NULL;
	greps_it = hud_greps;

	while(greps_it)
	{
		if (greps_it->hud 
			&& hud_mouse_x >= greps_it->x
			&& hud_mouse_x <= (greps_it->x + greps_it->width) 
			&& hud_mouse_y >= greps_it->y
			&& hud_mouse_y <= (greps_it->y + greps_it->height))
		{
			greps_it->highlighted = true;
			return greps_it->hud;
		}

		greps_it->highlighted = false;

		greps_it = greps_it->next;
	}

	return NULL;
}

//
// Finds if there's any HUD under the cursor and draws outlines for all HUD elements.
//
static qbool HUD_Editor_FindHudUnderCursor(hud_t **hud)
{
	hud_grephandle_t	*grep		= NULL;
	hud_greppos_t		pos			= pos_visible;
	hud_t				*temp_hud	= hud_huds;
	qbool				found		= false;

	if(!temp_hud)
	{
		return false;
	}

	while(temp_hud->next)
	{
		// Not visible.
		if (!temp_hud->show->value)
		{
			temp_hud = temp_hud->next;
			continue;
		}

		// We found one.
		if (hud_mouse_x >= temp_hud->lx 
			&& hud_mouse_x <= (temp_hud->lx + temp_hud->lw) 
			&& hud_mouse_y >= temp_hud->ly 
			&& hud_mouse_y <= (temp_hud->ly + temp_hud->lh))
		{
			found = true;
			(*hud) = temp_hud;
		}

		// Draw an outline for all hud elements (faint).
		Draw_AlphaRectangleRGB(temp_hud->lx, temp_hud->ly, temp_hud->lw, temp_hud->lh, 0, 1, 0, 1, false, 0.1);

		// Check if the hud element is offscreen and
		// if there's any grep handle for this hud element
		{
			pos = HUD_Editor_GetHudGrepPosition(temp_hud);
			grep = HUD_Editor_FindGrep(temp_hud);

			if(pos != pos_visible)
			{
				// We didn't find any grep handle so create one.
				if(!grep)
				{
					grep = HUD_Editor_CreateGrep(temp_hud);
				}

				// Position the grep if we got one.
				if(grep)
				{
					HUD_Editor_PositionGrep(temp_hud, grep);
				}
			}
			else
			{
				// The HUD element is visbile, so no need for a grep handle for it.
				HUD_Editor_DestroyGrep(grep);
			}
		}

		temp_hud = temp_hud->next;
	}

	// We didn't find any HUD's under the cursor, but
	// what about "grep handles" (for offscreen HUDs).
	if(!found)
	{
		temp_hud = HUD_Editor_FindHudByGrep();

		if(temp_hud)
		{
			found = true;
			(*hud) = temp_hud;
		}
	}

	// Draw the "grep handles" for offscreen HUDs.
	HUD_Editor_DrawGreps();
	
	if (!found)
	{
		(*hud) = NULL;
	}

	// Check if we are moving an old hud already, then we want that HUD.
	if (last_moved_hud && selected_hud == NULL && keydown[K_MOUSE1])
	{
		found = true;
		(*hud) = last_moved_hud;
	}

	// Draw a rectangle around the currently active HUD element.
	if(found && (*hud))
	{
		Draw_OutlineRGB((*hud)->lx, (*hud)->ly, (*hud)->lw, (*hud)->lh, 0, 1, 0, 1);
	}

	// If we are realigning draw a green outline for the selected hud element.
	if (selected_hud)
	{
		Draw_OutlineRGB(selected_hud->lx, selected_hud->ly, selected_hud->lw, selected_hud->lh, 0, 1, 0, 1);
	}

	return found;
}

//
// Returns the point a HUD is aligned to.
//
static void HUD_Editor_GetAlignmentPoint(hud_t *hud, int *x, int *y)
{
	hud_alignmode_t alignmode = HUD_Editor_GetAlignmentFromString(va("%s %s", hud->align_x->string, hud->align_y->string));

	(*x) = HUD_CENTER_X(hud);
	(*y) = HUD_CENTER_Y(hud);

	if(hud->place_hud)
	{
		int parent_x = hud->place_hud->lx;
		int parent_y = hud->place_hud->ly;
		int parent_w = hud->place_hud->lw;
		int parent_h = hud->place_hud->lh;

		switch(alignmode)
		{
			case hud_align_center:
				(*x) = HUD_CENTER_X(hud->place_hud);
				(*y) = HUD_CENTER_Y(hud->place_hud);
				break;
			case hud_align_right:
				(*x) = parent_x + parent_w;
				(*y) = parent_y + (parent_h / 2);
				break;
			case hud_align_topright:
				(*x) = parent_x + parent_w;
				(*y) = parent_y;
				break;
			case hud_align_top:
				(*x) = parent_x + (parent_w / 2);
				(*y) = parent_y;
				break;
			case hud_align_topleft:
				(*x) = parent_x;
				(*y) = parent_y;
				break;
			case hud_align_left:
				(*x) = parent_x;
				(*y) = parent_y + (parent_h / 2);
				break;
			case hud_align_bottomleft:
				(*x) = parent_x;
				(*y) = parent_y + parent_h;
				break;
			case hud_align_bottom:
				(*x) = parent_x + (parent_w / 2);
				(*y) = parent_y + parent_h;
				break;
			case hud_align_bottomright:
				(*x) = parent_x + parent_w;
				(*y) = parent_y + parent_h;
				break;
		}
	}
	else
	{
		// TODO: Not aligned to another HUD.
	}
}

//
// Draws connections to/from a HUD element.
//
static void HUD_Editor_DrawConnections(hud_t *hud)
{
	hud_t *child = NULL;
	hud_t *parent = NULL;
	int align_x = 0.0;
	int align_y = 0.0;

	if (!hud)
	{
		return;
	}

	// If the hud we are hovering above has a parent draw a line to it.
	if (hud->place && hud->place->string && strlen(hud->place->string))
	{
		parent = HUD_Find(hud->place->string);
		if (parent && !selected_hud)
		{
			// Draw a line from the alignment point on the parent to the HUD.
			HUD_Editor_GetAlignmentPoint(hud, &align_x, &align_y);
			Draw_AlphaLineRGB(HUD_CENTER_X(hud), HUD_CENTER_Y(hud), align_x, align_y, 1, 1, 0, 0, 0.5); // Red.
		}
	}

	// Don't show the connnections to the children of the HUD element we're already placed at.
	if(hud_editor_mode == hud_editmode_place && parent && !strcmp(hud->name, parent->name))
	{
		return;
	}

	// Draw a line to all children of the HUD we're hovering.
	while((child = HUD_Editor_FindNextChild(hud)))
	{
		// Don't bother with hidden children.
		if(!child->show->value)
		{
			continue;
		}

		// Draw a line from the alignment point on the parent to the child.
		HUD_Editor_GetAlignmentPoint(child, &align_x, &align_y);
		Draw_AlphaLineRGB(align_x, align_y, HUD_CENTER_X(child), HUD_CENTER_Y(child), 1, 1, 0, 0, 0.5); // Red.
	}
}

//
// Evaluates the current mouse/keyboard state and sets the appropriate mode.
//
static void HUD_Editor_EvaluateState(hud_t *hud_hover)
{
	// Mouse 1			= Move + Resize
	// Mouse 2			= Toggle menu
	// Ctrl  + Mouse 1	= Place
	// Alt	 + Mouse 1	= Align
	// Shift + Mouse 1	= Lock moving to one axis (If you start dragging along x-axis, it will stick to that)

	if (hud_hover && MOUSEDOWN_1_ONLY && isShiftDown())
	{
		// Move + resize (Locked to an axis).
		HUD_Editor_SetMode(hud_editmode_move_lockedaxis);
	}
	else if (hud_hover && MOUSEDOWN_1_ONLY && isCtrlDown())
	{
		// Place.
		HUD_Editor_SetMode(hud_editmode_place);
	}
	else if (hud_hover && MOUSEDOWN_1_ONLY && isAltDown())
	{
		// Align.
		HUD_Editor_SetMode(hud_editmode_align);
	}
	else if (hud_hover && MOUSEDOWN_1_ONLY)
	{
		// Move + resize.
		HUD_Editor_SetMode(hud_editmode_move_resize);
	}
	else if (hud_hover && MOUSEDOWN_2_ONLY)
	{
		// HUD element menu for the HUD element we have the mouse over.
		HUD_Editor_SetMode(hud_editmode_hudmenu);
	}
	else if (MOUSEDOWN_2_ONLY)
	{
		// Main menu for adding HUDs if we right click non-occupied space.
		HUD_Editor_SetMode(hud_editmode_menu);
	}
	else 
	{
		// Nothing special happening. 
		HUD_Editor_SetMode(hud_editmode_normal);
	}
}

//
// Draws the tooltips for a HUD element based on the state we're in.
//
static void HUD_Editor_DrawTooltips(hud_t *hud_hover)
{
	char *message = NULL;
	float color[4] = {0, 0, 0, 0};

	if(!hud_hover)
	{
		return;
	}

	if (selected_hud)
	{
		switch(hud_editor_mode)
		{
			case hud_editmode_move_lockedaxis :
			case hud_editmode_move_resize :
			{
				message = va("(%d, %d) moving %s", (int)selected_hud->pos_x->value, (int)selected_hud->pos_y->value, selected_hud->name);
				color[0] = 1;
				color[3] = 0.5;
				break;
			}
			case hud_editmode_align :
			{
				char *align = NULL;

				align = HUD_Editor_GetAlignmentString(hud_alignmode);

				message = va("align %s to %s", selected_hud->name, align);
				color[1] = 1;
				color[2] = 1;
				color[3] = 0.5;
				
				break;
			}
			case hud_editmode_place :
			{
				message = va("placing %s", selected_hud->name);
				color[0] = 1;
				color[3] = 0.5;
				break;
			}
			case hud_editmode_normal :
			{
				message = hud_hover->name;
				color[2] = 1;
				color[3] = 0.5;
				break;
			}
		}
	}

	if(!message)
	{
		message = hud_hover->name;
		color[2] = 1;
		color[3] = 0.5;		
	}

	HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, message, color[0], color[1], color[2], color[3]);
}

//
// Main HUD Editor function.
//
static void HUD_Editor(void)
{
	extern float mouse_x, mouse_y;
	int status;
	qbool found;
	hud_t				*hud_hover	= NULL;

	// Updating cursor location.
	hud_mouse_x = cursor_x;
	hud_mouse_y = cursor_y;

	// Find the HUD we're moving or have the cursor over.
	// Also draws highlights and such.
	found = HUD_Editor_FindHudUnderCursor(&hud_hover);

	// Check the mouse/keyboard states and if we're hovering above a hud or not.
	HUD_Editor_EvaluateState(hud_hover);
	
	// Draw the child/parent connections the hud we're hovering has.
	HUD_Editor_DrawConnections(hud_hover);

	// Draw a red line from selected hud to cursor.
	if (selected_hud)
	{
		Draw_AlphaLineRGB(hud_mouse_x, hud_mouse_y, HUD_CENTER_X(selected_hud), HUD_CENTER_Y(selected_hud), 1, 1, 0, 0, 1); // Red.
	}

	// If we still havent found anything we return !
	if (!found)
	{
		if (MOUSEDOWN_NONE) 
		{
			selected_hud = NULL;
			last_moved_hud = NULL;
		}

		return;
	}

	// Draw tooltips for the HUD.
	HUD_Editor_DrawTooltips(hud_hover);
	
	// Check if we should move.
	if(HUD_Editor_Moving(hud_hover))
	{
		return;
	}

	// Check if we're placing.
	if(HUD_Editor_Placing(hud_hover))
	{
		return;
	}

	// Check if we're aligning.
	if(HUD_Editor_Aligning(hud_hover))
	{
		return;
	}
}

//
// Toggles the HUD Editor on or off.
//
void HUD_Editor_Toggle_f(void)
{
	static keydest_t key_dest_prev = key_game;

	if (cls.state != ca_active) 
	{
		return;
	}

	hud_editor = !hud_editor;
	S_LocalSound("misc/basekey.wav");

	if (hud_editor)
	{
		key_dest = key_hudeditor;
		HUD_Editor_SetMode(hud_editmode_normal);
	}
	else
	{
		key_dest = key_game;
		key_dest_beforecon = key_game;
		HUD_Editor_SetMode(hud_editmode_off);
	}
}
#endif // GLQUAKE

//
// Handles key events sent to the HUD editor.
//
void HUD_Editor_Key(int key, int unichar) 
{
	#ifdef GLQUAKE
	int togglekeys[2];

	M_FindKeysForCommand("toggleconsole", togglekeys);
	if ((key == togglekeys[0]) || (key == togglekeys[1])) 
	{
		Con_ToggleConsole_f();
		return;
	}

	switch (key) 
	{
		case K_ESCAPE:
			HUD_Editor_Toggle_f();
			break;
	}
	#endif
}

//
// Inits HUD Editor.
//
void HUD_Editor_Init(void) 
{
	#ifdef GLQUAKE
	Cmd_AddCommand("hud_editor", HUD_Editor_Toggle_f);
	hud_editor = false;
	HUD_Editor_SetMode(hud_editmode_off);
	#endif // GLQUAKE
}

//
// Draws the HUD Editor if it's on.
//
void HUD_Editor_Draw(void) 
{
	#ifdef GLQUAKE
	if (!hud_editor)
		return;

	HUD_Editor();
	#endif GLQUAKE
}

//
// Should this HUD element be fully drawn or not when in align mode
// when using the HUD editor?
//
qbool HUD_Editor_ConfirmDraw(hud_t *hud)
{
	#ifdef GLQUAKE
	if(hud_editor_mode == hud_editmode_align || hud_editor_mode == hud_editmode_place)
	{
		// If this is the selected hud, or the parent of the selected hud then draw it.
		if((selected_hud && !strcmp(selected_hud->name, hud->name))
			|| (selected_hud && hud->place_hud && !strcmp(selected_hud->name, hud->place_hud->name)))
		{
			return true;
		}

		return false;
	}

	#endif // GLQUAKE

	return true;
}

