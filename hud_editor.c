/**
	
	HUD Editor module

	made by jogihoogi, Feb 2007
	last edit:
	$Id: hud_editor.c,v 1.11 2007-02-27 17:51:12 cokeman1982 Exp $

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

// Cursor location.
double			hud_mouse_x;				// The screen coordinates of the mouse cursor.
double			hud_mouse_y;

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
	hud_greppos_t			pos;
	struct hud_grephandle_s	*next;
	struct hud_grephandle_s	*previous;
} hud_grephandle_t;

hud_grephandle_t *hud_greps = NULL;	// The list of "grep handles" that are shown if a HUD element is moved offscreen.

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

void vectoangles(vec3_t vec, vec3_t ang); // cl_cam.c

//
// Get which part of the screen to align to based on where the mouse cursor is.
//
static int HUD_Editor_Get_Alignment(int x, int y, hud_t *hud_element) 
{
	vec3_t center, pointer, angles;

	// Find the center of the HUD element.
	center[0] = hud_element->lx + hud_element->lw / 2;
	center[1] = hud_element->ly + hud_element->lh / 2;

	// The position of the mouse cursor relative to the 
	// center of the HUD element.
	pointer[0] = x - center[0];
	pointer[1] = center[1] - y;

	// Get the angles between the two vectors.
	vectoangles(pointer, angles);

	if (angles[1] < 45 || angles[1] > 315)
	{
		return 1;
	}
	else if (angles[1] > 45 && angles[1] < 135)
	{
		return 2; 
	}
	else if (angles[1] > 135 && angles[1] < 225)
	{
		return 3;
	}
	else if (angles[1] > 225 && angles[1] < 315)
	{
		return 4;
	}

	return -1;
}

//
// Moves a HUD element.
//
static void HUD_Editor_Move(float x, float y, float dx, float dy, hud_t *hud_element) 
{
	Cvar_Set(hud_element->pos_x, va("%f", hud_element->pos_x->value + dx));
	Cvar_Set(hud_element->pos_y, va("%f", hud_element->pos_y->value + dy));
}

// =============================================================================
// HUD Editor Grep Handles
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
	clrinfo_t color;
	hud_grephandle_t *greps_it = NULL;
	greps_it = hud_greps;

	// Orange.
	color.c = RGBA_2_Int(255, 150, 0, 255);
	color.i = 0;

	while(greps_it)
	{
		Draw_ColoredString3(greps_it->x, greps_it->y, va("%s %s", HUD_Editor_GetGrepArrow(greps_it), greps_it->hud->name), &color, 1, 0);

		greps_it = greps_it->next;
	}
}

//
// Get's the position offscreen for a HUD element
// left/right/top/bottom or visible if it's not offscreen.
//
static hud_greppos_t HUD_Editor_GetHudGrepPosition(hud_t *h)
{
	if(h->lx + h->lw <= 0)
	{
		return pos_left;
	}
	else if(h->lx >= vid.width)
	{
		return pos_right;
	}
	else if(h->ly + h->lh <= 0)
	{
		return pos_top;
	}
	else if(h->ly >= vid.height)
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
			return greps_it->hud;
		}

		greps_it = greps_it->next;
	}

	return NULL;
}

//
// Main HUD Editor loop.
//
static void HUD_Editor(void)
{
	extern float mouse_x, mouse_y;
	int status;
	int bx, by, bw, bh;
	qbool found;
	char bname[128], bplace[32], balignx[32], baligny[32];
	hud_t *hud;
	hud_t *temp_hud;
	hud_t *parent;
	hud_grephandle_t *grep;
	hud_greppos_t pos;
	static cvar_t *scale;

	// Getting the huds linked list.
	temp_hud = hud_huds;

	// Updating cursor location.
	hud_mouse_x = cursor_x;
	hud_mouse_y = cursor_y;

	// Check if we have a hud under the cursor (if one isn't already selected).
	found = false;
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
			hud = temp_hud;
		}

		// Draw an outline for all hud elements (faint).
		Draw_AlphaRectangleRGB(temp_hud->lx, temp_hud->ly, temp_hud->lw, temp_hud->lh, 0, 1, 0, 1, false, 0.1);

		// Check if the hud element is offscreen and
		// if there's any grep handle for this hud element
		pos = HUD_Editor_GetHudGrepPosition(temp_hud);
		grep = HUD_Editor_FindGrep(temp_hud);

		if(pos != pos_visible)
		{
			// We didn't find any grep handle so create one.
			if(!grep)
			{
				grep = HUD_Editor_CreateGrep(temp_hud);
			}

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

		temp_hud = temp_hud->next;
	}
	
	temp_hud = NULL;

	if(!found)
	{
		temp_hud = HUD_Editor_FindHudByGrep();

		if(temp_hud)
		{
			found = true;
			hud = temp_hud;
		}
	}

	HUD_Editor_DrawGreps();
	
	// If not hud stays null.
	if (!found)
	{
		hud = NULL;
	}

	// Checks if we are moving an old hud.
	if (last_moved_hud && selected_hud == NULL && keydown[K_MOUSE1])
	{
		found = true;
		hud = last_moved_hud;
	}
	
	// Draw a rectangle around the currently active huds.
	if(found && hud)
	{
		clrinfo_t info;
		info.c = RGBA_2_Int(255, 255, 0, 255);
		info.i = 0;
		Draw_OutlineRGB(hud->lx, hud->ly, hud->lw, hud->lh, 0, 1, 0, 1);

		// Draw Z-order of the current item.
		//Draw_ColoredString3(hud->lx, hud->ly, va("%d", (int)hud->order->value), &info, 1, 0);
	}
	
	// If we are realigning draw a green outline for the selected hud element.
	if (selected_hud)
	{
		Draw_OutlineRGB(selected_hud->lx, selected_hud->ly, selected_hud->lw, selected_hud->lh, 0, 1, 0, 1);
	}
	
	// Check if we hit any static boxes like screen edges etc.
	#define HUD_ED_ALIGNBOX_W 20
	if (!found)
	{
		if (hud_mouse_x < 20 
			&& hud_mouse_y < 20)
		{
			strcpy(bname, "screen top left");
			strcpy(bplace, "screen");
			strcpy(balignx, "left");
			strcpy(baligny, "top");
			bx = 0;
			by = 0;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x < vid.width 
			&& hud_mouse_x > vid.width - 20 
			&& hud_mouse_y < 20)
		{
			strcpy(bname, "screen top right");
			strcpy(bplace, "screen");
			strcpy(balignx, "right");
			strcpy(baligny, "top");
			by = 0;
			bx = vid.width - 20;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x < 20 
			&& hud_mouse_y > vid.height - 20 
			&& hud_mouse_y < vid.height)
		{
			strcpy(bname, "screen bottom left");
			strcpy(bplace, "screen");
			strcpy(balignx, "left");
			strcpy(baligny, "bottom");
			bx = 0;
			by = vid.height - 20;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x < vid.width 
			&& hud_mouse_x > vid.width - 20 
			&& hud_mouse_y > vid.height - 20 
			&& hud_mouse_y < vid.height)
		{
			strcpy(bname, "screen bottom right");
			strcpy(bplace, "screen");
			strcpy(balignx, "right");
			strcpy(baligny, "bottom");
			bx = vid.width - 20;
			by = vid.height - 20;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x > vid.width - 20  
			&& hud_mouse_x < vid.width 
			&& hud_mouse_y < vid.height - 20 
			&& hud_mouse_y >20)
		{
			strcpy(bname, "screen center right");
			strcpy(bplace, "screen");
			strcpy(balignx, "right");
			strcpy(baligny, "center");
			bx = vid.width - 20;
			by = 20;
			bw = 20;
			bh = vid.height - 40;
			found = true;
		}
		else if (hud_mouse_x > 0 
			&& hud_mouse_x < 20 
			&& hud_mouse_y < vid.height - 20 
			&& hud_mouse_y > 20)
		{
			strcpy(bname, "screen center left");
			strcpy(bplace, "screen");
			strcpy(balignx, "left");
			strcpy(baligny, "center");
			bx = 0;
			by = 20;
			bw = 20;
			bh = vid.height - 40;
			found = true;
		}
		else if (hud_mouse_x > 20
			&& hud_mouse_x < vid.width - 20 
			&& hud_mouse_y < vid.height 
			&& hud_mouse_y > vid.height - 20)
		{
			strcpy(bname, "screen bottom center");
			strcpy(bplace, "screen");
			strcpy(balignx, "center");
			strcpy(baligny, "bottom");
			bx = 20;
			by = vid.height - 20;
			bw = vid.width - 40;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x > 20 
			&& hud_mouse_x < vid.width - 20
			&& hud_mouse_y < 20
			&& hud_mouse_y > 0)
		{
			strcpy(bname, "screen top center");
			strcpy(bplace, "screen");
			strcpy(balignx, "center");
			strcpy(baligny, "top");
			bx = 20;
			by = 0;
			bw = vid.width - 40;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x > 20 
			&& hud_mouse_x < 40 
			&& hud_mouse_y < 40 
			&& hud_mouse_y > 20)
		{
			strcpy(bname, "console left");
			strcpy(bplace, "top");
			strcpy(balignx, "left");
			strcpy(baligny, "console");
			bx = 20;
			by = 20;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x > vid.width - 40  
			&& hud_mouse_x < vid.width - 20 
			&& hud_mouse_y < 40 
			&& hud_mouse_y > 20)
		{
			strcpy(bname, "console right");
			strcpy(bplace, "top");
			strcpy(balignx, "right");
			strcpy(baligny, "console");
			bx = vid.width - 40;
			by = 20;
			bw = 20;
			bh = 20;
			found = true;
		}
		else if (hud_mouse_x > 80 
			&& hud_mouse_x < vid.width - 20 
			&& hud_mouse_y < 40 
			&& hud_mouse_y > 20)
		{
			strcpy(bname, "console center");
			strcpy(bplace, "top");
			strcpy(balignx, "center");
			strcpy(baligny, "console");
			bx = 40;
			by = 20;
			bw = vid.width - 80;
			bh = 20;
			found = true;
		}
	}

	// If the hud we are hovering above has a parent draw a line to it.
	if (hud)
	{
		if (hud->place->string && strlen(hud->place->string))
		{
			parent = HUD_Find(hud->place->string);
			if (parent && !selected_hud)
			{
				Draw_AlphaLineRGB((hud->lx + (hud->lw / 2)), hud->ly + hud->lh / 2, 
					(parent->lx + (parent->lw / 2)), parent->ly + parent->lh / 2, 
					1, 1, 0, 0, 1); // Red.
			}
		}
	}

	// Draw a red line from selected hud to cursor.
	if (selected_hud)
	{
		Draw_AlphaLineRGB(hud_mouse_x, hud_mouse_y, 
			(selected_hud->lx + (selected_hud->lw / 2)), selected_hud->ly + selected_hud->lh / 2, 
			1, 1, 0, 0, 1); // Red.
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

	// Draw the tooltips.
	if (MOUSEDOWN_1_ONLY && hud)
	{
		HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("(%d, %d) %s moving", (int)hud->pos_x->value, (int)hud->pos_y->value, hud->name), 1, 0, 0, 0.5);
	}
	else if ((MOUSEDOWN_1_2_ONLY || MOUSEDOWN_3_ONLY) && hud)
	{
		// Both button clicked, resizing.
		HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("%s resizing", hud->name), 0, 1, 0, 0.5);
	}
	else if (MOUSEDOWN_NONE && hud)
	{
		// No mouse button clicked, but we're over a HUD so draw it's name.
		hud_editor_mode = hud_editmode_normal;

		HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, hud->name, 0, 0, 1, 0.5);
	}
	else if(MOUSEDOWN_2_ONLY)
	{
		if (hud)
		{
			if (selected_hud)
			{
				// Remove the alignment if we're pressing the right mouse button and are 
				// holding the mouse over the selected hud element.
				if (!strcmp(hud->name, selected_hud->name))
				{
					HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("remove alignment", hud->name), 0, 1, 1, 0.5);
				}
				else
				{
					status = HUD_Editor_Get_Alignment(hud_mouse_x, hud_mouse_y, hud);

					// only 4 lockons atm left,right,top,bottom
					if (status == 1)
					{
						HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s's right", hud->name), 0, 1, 1, 0.5);
					}
					else if (status == 2)
					{
						HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s's top", hud->name), 0, 1, 1, 0.5);
					}
					else if (status == 3)
					{
						HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s's left", hud->name), 0, 1, 1, 0.5);
					}
					else if (status == 4)
					{
						HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s's bottom", hud->name), 0, 1, 1, 0.5);
					}
				}
			}
			else
			{
				HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s", hud->name), 0, 1, 1, 0.5);
			}
		}
		else
		{
			// Drawing fixed stuff.
			Draw_OutlineRGB(bx, by, bw, bh, 1, 1, 0, 1);
			HUD_Editor_DrawTooltip(hud_mouse_x, hud_mouse_y, va("align to %s", bname), 0, 1, 1, 0.5);
		}
	}

	// Left mousebutton down = lets move it !
	if (MOUSEDOWN_1_ONLY && hud)
	{
		HUD_Editor_Move(hud_mouse_x, hud_mouse_y, mouse_x, mouse_y, hud);
		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}

	// Both mousebuttons (or mouse3) are down let's scale it.
	if ((MOUSEDOWN_1_2_ONLY || MOUSEDOWN_3_ONLY) && !selected_hud && hud)
	{
		hud_mouse_x -= mouse_x;
		hud_mouse_y -= mouse_y;

		scale = HUD_FindVar(hud, "scale");
		
		if (!scale)
		{
			return;
		}

		Cvar_Set(scale, va("%f", scale->value + (mouse_x + mouse_y) / 200));
		
		if (scale->value < 0)
		{
			Cvar_Set(scale, "0");
		}

		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}

	// If mouse 2 is pressed select the current hud for realigning.
	if (MOUSEDOWN_2_ONLY)
	{
		//extern void HUD_SetHudEditorAlignMode(hud_t *selected_hud, qbool isAligning);
		last_moved_hud = NULL;

		if (!selected_hud)
		{
			// We didn't have anything selected, so do that first.
			selected_hud = hud;
			return;
		}
		else if (hud)
		{
			// Set HUD drawing to "align/place mode" (Only draw outlines).
			hud_editor_mode = hud_editmode_align;

			center[0] = hud->lx + hud->lw / 2;
			center[1] = hud->ly + hud->lh / 2;
			
			// Draw redline from center ouf selected to cursor.
			Draw_AlphaLineRGB(center[0], center[1], hud_mouse_x, hud_mouse_y, 1, 0, 0, 1, 1);
			last_moved_hud = NULL;
			return;
		}
		else
		{
			// We havent found a hud so return.
			return;
		}
	}

	// No mouse button pressed and a selected hud.
	if (MOUSEDOWN_NONE && selected_hud)
	{
		last_moved_hud = NULL;

		// Remove the current alignment if we are still hovering above the 
		// same hud and set the defaults for screen alignment.
		if (selected_hud == hud)
		{
			Cvar_Set(hud->place, "screen");
			Cvar_Set(hud->pos_x, va("%f", hud_mouse_x));
			Cvar_Set(hud->pos_y, va("%f", hud_mouse_y));
			Cvar_Set(hud->align_x, "left");
			Cvar_Set(hud->align_y, "top");
			selected_hud = NULL;
		}
		else if (hud || strlen(bname))
		{
			if (hud)
			{
				Cvar_Set(selected_hud->place, hud->name);
				status = HUD_Editor_Get_Alignment(hud_mouse_x, hud_mouse_y, hud);

				// Only 4 alignments atm left, right, top, bottom.
				if (status == 1)
				{
					Cvar_Set(selected_hud->align_x, "after");
					Cvar_Set(selected_hud->align_y, "center");
				}
				else if (status == 2)
				{
					Cvar_Set(selected_hud->align_x, "center");
					Cvar_Set(selected_hud->align_y, "before");
				}
				else if (status == 3)
				{
					Cvar_Set(selected_hud->align_x, "before");
					Cvar_Set(selected_hud->align_y, "center");
				}
				else if (status == 4)
				{
					Cvar_Set(selected_hud->align_x, "center");
					Cvar_Set(selected_hud->align_y, "after");
				}
			}
			else
			{
				// We lock it to the extra stuff.
				Cvar_Set(selected_hud->place, bplace);
				center[0] = bx + bw / 2;
				center[1] = by + bh / 2;
				Cvar_Set(selected_hud->place, bplace);
				Cvar_Set(selected_hud->align_x, balignx);
				Cvar_Set(selected_hud->align_y, baligny);
			}

			Cvar_Set(selected_hud->pos_x, "0");
			Cvar_Set(selected_hud->pos_y, "0");
		}

		selected_hud = NULL;
		HUD_Recalculate();
		return;
	}

	last_moved_hud = NULL;
	return;
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
		hud_editor_mode = hud_editmode_normal;
	}
	else
	{
		key_dest = key_game;
		key_dest_beforecon = key_game;
		hud_editor_mode = hud_editmode_off;
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

void HUD_Editor_Init(void) 
{
	#ifdef GLQUAKE
	Cmd_AddCommand("hud_editor", HUD_Editor_Toggle_f);
	hud_editor = false;
	#endif // GLQUAKE
}

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
// when using the HUD editor.  
//
qbool HUD_Editor_ConfirmDraw(hud_t *hud)
{
	#ifdef GLQUAKE
	if(hud_editor_mode == hud_editmode_align)
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
