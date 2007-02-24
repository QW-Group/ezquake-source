/**
	
	HUD Editor module

	made by jogihoogi, Feb 2007
	last edit:
	$Id: hud_editor.c,v 1.8 2007-02-24 14:52:22 johnnycz Exp $

*/

#include "quakedef.h"
#include "hud.h"
#include "EX_misc.h"

#ifdef GLQUAKE
extern hud_t	*hud_huds;
hud_t			*last_moved_hud;
hud_t			*selected_hud;
int				win_x;
int				win_y;
vec3_t			points[3];
vec3_t			corners[4];
vec3_t			center;
vec3_t			pointer;

mpic_t			hud_cursor;

qbool hud_editor = false;
cvar_t			hud_cursor_scale = {"hud_cursor_scale", "0.1"};
cvar_t			hud_cursor_alpha = {"hud_cursor_alpha", "1"};

// Cursor location.
double			hud_mouse_x;
double			hud_mouse_y;
double			cursor_loc[2];
#define			HUD_MOUSE_X 0
#define			HUD_MOUSE_Y 1

static void HUD_Editor_Draw_Tooltip(int x, int y, char *string, float r, float g, float b) 
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

	Draw_FillRGB(x, y, len, 8, r, g, b);
	Draw_String(x, y, string);
}

void vectoangles(vec3_t vec, vec3_t ang); // cl_cam.c

static int HUD_Editor_Get_Alignment(int x, int y, hud_t *hud_element) 
{
	vec3_t center, pointer, angles;

	center[0] = hud_element->lx + hud_element->lw / 2;
	center[1] = hud_element->ly + hud_element->lh / 2;
	pointer[0] = x - center[0];
	pointer[1] = center[1] - y;

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

static void HUD_Editor_Move(float x, float y, float dx, float dy, hud_t *hud_element) 
{
	Cvar_Set(hud_element->pos_x, va("%f", hud_element->pos_x->value + dx));
	Cvar_Set(hud_element->pos_y, va("%f", hud_element->pos_y->value + dy));
}

void HUD_Recalculate(void);

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
	static cvar_t *scale;

	// Getting the huds linked list.
	temp_hud = hud_huds;

	// Updating cursor location.
	hud_mouse_x += mouse_x;
	hud_mouse_y += mouse_y;

	// Check if we're within bounds.
	clamp(hud_mouse_x, 0, vid.width);
	clamp(hud_mouse_y, 0, vid.height);

	// Always draw the cursor.
	if (hud_cursor.texnum)
	{
		Draw_SAlphaPic(hud_mouse_x, hud_mouse_y, &hud_cursor, hud_cursor_alpha.value, hud_cursor_scale.value);
	}
	else
	{
		Draw_AlphaLineRGB(hud_mouse_x, hud_mouse_y, hud_mouse_x + 2, hud_mouse_y + 2, 2, 0, 1, 0, 1);
	}

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

		temp_hud = temp_hud->next;
	}
	
	temp_hud = NULL;
	
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
			strcpy(bname,"screen top right");
			strcpy(bplace,"screen");
			strcpy(balignx,"right");
			strcpy(baligny,"top");
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
			strcpy(bname,"screen center right");
			strcpy(bplace,"screen");
			strcpy(balignx,"right");
			strcpy(baligny,"center");
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
			strcpy(bname,"screen center left");
			strcpy(bplace,"screen");
			strcpy(balignx,"left");
			strcpy(baligny,"center");
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
			strcpy(bname,"screen top center");
			strcpy(bplace,"screen");
			strcpy(balignx,"center");
			strcpy(baligny,"top");
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

	// If the hud we are hovering above has a parent draw a line !
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
		if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2]) 
		{
			selected_hud = NULL;
			last_moved_hud = NULL;
		}
		return;
	}
		
	// Draw the tooltips.
	if (keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud)
	{
		HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("(%d, %d) %s moving", (int)hud->pos_x->value, (int)hud->pos_y->value, hud->name), 1, 0, 0);
	}
	else if (keydown[K_MOUSE1] && keydown[K_MOUSE2] && hud)
	{
		HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("%s resizing", hud->name), 0, 1, 0);
	}
	else if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud)
	{
		HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, hud->name, 0, 0, 1);
	}
	else if(!keydown[K_MOUSE1] && keydown[K_MOUSE2])
	{
		if (hud)
		{
			if (selected_hud)
			{
				// Remove the alignment if we're pressing the right mouse button and are 
				// holding the mouse over the selected hud element.
				if (!strcmp(hud->name, selected_hud->name))
				{
					HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("remove alignment", hud->name), 0, 1, 1);
				}
				else
				{
					status = HUD_Editor_Get_Alignment(hud_mouse_x, hud_mouse_y, hud);

					// only 4 lockons atm left,right,top,bottom
					if (status == 1)
					{
						HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s's right", hud->name), 0, 1, 1);
					}
					else if (status == 2)
					{
						HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s's top", hud->name), 0, 1, 1);
					}
					else if (status == 3)
					{
						HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s's left", hud->name), 0, 1, 1);
					}
					else if (status == 4)
					{
						HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s's bottom", hud->name), 0, 1, 1);
					}
				}
			}
			else
			{
				HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s", hud->name), 0, 1, 1);
			}
		}
		else
		{
			// Drawing fixed stuff.
			Draw_OutlineRGB(bx, by, bw, bh, 0, 1, 0, 1);
			HUD_Editor_Draw_Tooltip(hud_mouse_x, hud_mouse_y, va("align to %s", bname), 0, 1, 1);
		}
	}

	// Left mousebutton down = lets move it !
	if (keydown[K_MOUSE1] && !keydown[K_MOUSE2] && hud)
	{
		HUD_Editor_Move(hud_mouse_x, hud_mouse_y, mouse_x, mouse_y, hud);
		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}

	// Both mousebuttons (or mouse3) are down let's scale it.
	if (((keydown[K_MOUSE1] && keydown[K_MOUSE2]) || (!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && keydown[K_MOUSE3])) && !selected_hud)
	{
		hud_mouse_x -= mouse_x;
		hud_mouse_y -= mouse_y;

		scale = HUD_FindVar(hud, "scale");
		
		if (!scale)
		{
			return;
		}

		Cvar_Set(scale,va("%f", scale->value + (mouse_x + mouse_y) / 200));
		
		if (scale->value < 0)
		{
			Cvar_Set(scale, "0");
		}

		last_moved_hud = hud;
		HUD_Recalculate();
		return;
	}

	// If mouse 2 is pressed select the current hud for realigning.
	if (!keydown[K_MOUSE1] && keydown[K_MOUSE2])
	{
		last_moved_hud = NULL;

		if (!selected_hud)
		{
			selected_hud = hud;
			return;
		}
		else if (hud)
		{
			center[0] = hud->lx + hud->lw / 2;
			center[1] = hud->ly + hud->lh / 2;
			// draw redline from center ouf selected to cursor
			Draw_AlphaLineRGB(center[0], center[1], hud_mouse_x, hud_mouse_y, 1, 0, 0, 1, 1);
			last_moved_hud = 0;
			return;
		}
		else
		{
			// we havent found a hud so return;
			return;
		}
	}

	// No mouse button pressed and a selected hud.
	if (!keydown[K_MOUSE1] && !keydown[K_MOUSE2] && selected_hud) 
	{
		last_moved_hud = NULL;

		// Remove the current alignment if we are still hovering above the same hud and set the defaults for screen alignment.
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

void HUD_Editor_Toggle_f(void)
{
	static keydest_t key_dest_prev = key_game;

	if (cls.state != ca_active) return;

	hud_editor = !hud_editor;
	S_LocalSound("misc/basekey.wav");

	if (hud_editor)
	{
		key_dest = key_hudeditor;
	}
	else
	{
		key_dest = key_game;
		key_dest_beforecon = key_game;
	}
}
#endif // GLQUAKE

void HUD_Editor_Key(int key, int unichar) {
#ifdef GLQUAKE
	int togglekeys[2];

	M_FindKeysForCommand("toggleconsole", togglekeys);
	if ((key == togglekeys[0]) || (key == togglekeys[1])) {
		Con_ToggleConsole_f();
		return;
	}

	switch (key) {
	case K_ESCAPE:
		HUD_Editor_Toggle_f();
		break;
	}
#endif
}

void HUD_Editor_Init(void) 
{
	#ifdef GLQUAKE
	mpic_t *picp;

	Cvar_Register(&hud_cursor_scale);
	Cvar_Register(&hud_cursor_alpha);
	Cmd_AddCommand("hud_editor", HUD_Editor_Toggle_f);
	hud_editor = false;
	if (picp = GL_LoadPicImage(va("gfx/%s", "cursor"), "cursor", 0, 0, TEX_ALPHA))
		hud_cursor = *picp;
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
