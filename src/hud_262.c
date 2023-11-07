/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

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

// Moved from cl_screen.c
/// declarations may be found in screen.h

#include "quakedef.h"
#include <time.h>
#include "keys.h"
#include "hud.h"
#include "hud_common.h"
#include "hud_editor.h"
#include "utils.h"
#include "gl_model.h"
#include "teamplay.h"
#include "input.h"
#include "utils.h"
#include "sbar.h"
#include "menu.h"
#include "image.h"

static char *hud262_load_buff = NULL;

extern cvar_t cl_hud;

/**************************************** 262 HUD *****************************/
// QW262 -->
hud_element_t *hud_list = NULL;
hud_element_t *prev;

char *Macro_BestAmmo(void); // Teamplay.c
void Draw_AlphaRectangle(int x, int y, int w, int h, int c, float thickness, qbool fill, float alpha); // gl_draw.c

void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);

void Hud_262LoadOnFirstStart(void)
{
	if (hud262_load_buff != NULL) {
		Cbuf_AddText(hud262_load_buff);
		Cbuf_Execute();
	}
	Q_free(hud262_load_buff);
}
// <-- QW262

hud_element_t *Hud_FindElement(const char *name)
{
	hud_element_t *elem;

	prev = NULL;
	for (elem = hud_list; elem; elem = elem->next) {
		if (!strcasecmp(name, elem->name))
			return elem;
		prev = elem;
	}

	return NULL;
}

qbool Hud_ElementExists(const char* name)
{
	return Hud_FindElement(name) != NULL;
}

static hud_element_t* Hud_NewElement(void)
{
	hud_element_t*	elem;
	elem = (hud_element_t *)Q_malloc(sizeof(hud_element_t));
	elem->next = hud_list;
	hud_list = elem;
	elem->name = Q_strdup(Cmd_Argv(1));
	return elem;
}

static void Hud_DeleteElement(hud_element_t *elem)
{
	if (elem->flags & (HUD_STRING))
		Q_free(elem->contents);
	if (elem->flags & (HUD_IMAGE))
		CachePic_RemoveByPic(elem->contents);
	if (elem->f_hover)
		Q_free(elem->f_hover);
	if (elem->f_button)
		Q_free(elem->f_button);
	Q_free(elem->name);
	Q_free(elem);
}

typedef void Hud_Elem_func(hud_element_t*);

void Hud_ReSearch_do(Hud_Elem_func f)
{
	hud_element_t *elem;

	for (elem = hud_list; elem; elem = elem->next) {
		if (ReSearchMatch(elem->name))
			f(elem);
	}
}

void Hud_Add_f(void)
{
	hud_element_t*	elem;
	struct hud_element_s*	next = NULL;	// Sergio
	qbool	hud_restore = false;	// Sergio
	cvar_t		*var;
	char		*a2, *a3;
	//Hud_Func	func;
	unsigned	old_coords = 0;
	unsigned	old_width = 0;
	float		old_alpha = 1;
	qbool	old_enabled = true;

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_add <name> <type> <param>\n");
	else {
		if ((elem = Hud_FindElement(Cmd_Argv(1)))) {
			if (elem) {
				old_coords = *((unsigned*)&(elem->coords));
				old_width = elem->width;
				old_alpha = elem->alpha;
				old_enabled = elem->flags & HUD_ENABLED;
				next = elem->next; // Sergio
				if (prev) {
					prev->next = elem->next;
					hud_restore = true; // Sergio
				}
				else
					hud_list = elem->next;
				Hud_DeleteElement(elem);
			}
		}

		a2 = Cmd_Argv(2);
		a3 = Cmd_Argv(3);

		if (!strcasecmp(a2, "cvar")) {
			if ((var = Cvar_Find(a3))) {
				elem = Hud_NewElement();
				elem->contents = var;
				elem->flags = HUD_CVAR | HUD_ENABLED;
			}
			else {
				Com_Printf("cvar \"%s\" not found\n", a3);
				return;
			}
		}
		else if (!strcasecmp(a2, "str")) {
			elem = Hud_NewElement();
			elem->contents = Q_strdup(a3);
			elem->flags = HUD_STRING | HUD_ENABLED;
		}
		else if (!strcasecmp(a2, "img")) {
			char* pic_path = Q_strdup(a3);
			mpic_t* tmp_pic;
			if (pic_path && strlen(pic_path) > 0) {
				// Try loading the pic.
				if (!(tmp_pic = Draw_CachePicSafe(pic_path, false, true))) {
					Com_Printf("Couldn't load picture %s for '%s'\n", pic_path, a2);
					return;
				}
			}
			elem = Hud_NewElement();
			elem->contents = tmp_pic;
			elem->flags = HUD_IMAGE | HUD_ENABLED;
		}
		else {
			Com_Printf("\"%s\" is not a valid hud type\n", a2);
			return;
		}

		*((unsigned*)&(elem->coords)) = old_coords;
		elem->width = old_width;
		elem->alpha = old_alpha;
		if (!old_enabled)
			elem->flags &= ~HUD_ENABLED;

		// Sergio -->
		// Restoring old hud place in hud_list
		if (hud_restore) {
			hud_list = elem->next;
			elem->next = next;
			prev->next = elem;
		}
		// <-- Sergio
	}
}

void Hud_Elem_Remove(hud_element_t *elem)
{
	if (prev)
		prev->next = elem->next;
	else
		hud_list = elem->next;
	Hud_DeleteElement(elem);
}

void Hud_Remove_f(void)
{
	hud_element_t	*elem, *next_elem;
	char			*name;
	int				i;

	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if (!ReSearchInit(name))
				return;
			prev = NULL;
			for (elem = hud_list; elem; ) {
				if (ReSearchMatch(elem->name)) {
					next_elem = elem->next;
					Hud_Elem_Remove(elem);
					elem = next_elem;
				}
				else {
					prev = elem;
					elem = elem->next;
				}
			}
			ReSearchDone();
		}
		else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Remove(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_Position_f(void)
{
	hud_element_t *elem;

	if (Cmd_Argc() != 5) {
		Com_Printf("Usage: hud_position <name> <pos_type> <x> <y>\n");
		return;
	}
	if (!(elem = Hud_FindElement(Cmd_Argv(1)))) {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
		return;
	}

	elem->coords[0] = atoi(Cmd_Argv(2));
	elem->coords[1] = atoi(Cmd_Argv(3));
	elem->coords[2] = atoi(Cmd_Argv(4));
}

void Hud_Elem_Bg(hud_element_t *elem)
{
	elem->coords[3] = atoi(Cmd_Argv(2));
}

void Hud_Bg_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
		Com_Printf("Usage: hud_bg <name> <bgcolor>\n");
	else if (IsRegexp(name)) {
		if (!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Bg);
		ReSearchDone();
	}
	else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Bg(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Move(hud_element_t *elem)
{
	elem->coords[1] += atoi(Cmd_Argv(2));
	elem->coords[2] += atoi(Cmd_Argv(3));
}

void Hud262_Move_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_move <name> <dx> <dy>\n");
	else if (IsRegexp(name)) {
		if (!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Move);
		ReSearchDone();
	}
	else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Move(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Width(hud_element_t *elem)
{
	if (elem->flags & HUD_IMAGE) {
		mpic_t *pic = elem->contents;
		int width = atoi(Cmd_Argv(2)) * 8;
		int height = width * pic->height / pic->width;
		pic->height = height;
		pic->width = width;
	}
	elem->width = max(min(atoi(Cmd_Argv(2)), 128), 0);
}

void Hud_Width_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3)
		Com_Printf("Usage: hud_width <name> <width>\n");
	else if (IsRegexp(name)) {
		if (!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Width);
		ReSearchDone();
	}
	else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Width(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Alpha(hud_element_t *elem)
{
	float alpha = atof(Cmd_Argv(2));
	elem->alpha = bound(0, alpha, 1);
}

void Hud_Alpha_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 3) {
		Com_Printf("hud_alpha <name> <value> : set HUD transparency (0..1)\n");
		return;
	}
	if (IsRegexp(name)) {
		if (!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Alpha);
		ReSearchDone();
	}
	else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Alpha(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Blink(hud_element_t *elem)
{
	double		blinktime;
	unsigned	mask;

	blinktime = atof(Cmd_Argv(2)) / 1000.0;
	mask = atoi(Cmd_Argv(3));

	if (mask > 3) return; // bad mask
	if (blinktime < 0.0 || blinktime > 5.0) return;

	elem->blink = blinktime;
	elem->flags = (elem->flags & (~(HUD_BLINK_F | HUD_BLINK_B))) | (mask << 3);
}

void Hud_Blink_f(void)
{
	hud_element_t *elem;
	char	*name = Cmd_Argv(1);

	if (Cmd_Argc() != 4)
		Com_Printf("Usage: hud_blink <name> <ms> <mask>\n");
	else if (IsRegexp(name)) {
		if (!ReSearchInit(name))
			return;
		Hud_ReSearch_do(Hud_Elem_Blink);
		ReSearchDone();
	}
	else {
		if ((elem = Hud_FindElement(name)))
			Hud_Elem_Blink(elem);
		else
			Com_Printf("HudElement \"%s\" not found\n", name);
	}
}

void Hud_Elem_Disable(hud_element_t *elem)
{
	elem->flags &= ~HUD_ENABLED;
}

void Hud_Disable_f(void)
{
	hud_element_t	*elem;
	char			*name;
	int				i;

	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if (!ReSearchInit(name))
				return;
			Hud_ReSearch_do(Hud_Elem_Disable);
			ReSearchDone();
		}
		else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Disable(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_Elem_Enable(hud_element_t *elem)
{
	elem->flags |= HUD_ENABLED;
}

void Hud_Enable_f(void)
{
	hud_element_t	*elem;
	char			*name;
	int				i;

	for (i = 1; i < Cmd_Argc(); i++) {
		name = Cmd_Argv(i);
		if (IsRegexp(name)) {
			if (!ReSearchInit(name))
				return;
			Hud_ReSearch_do(Hud_Elem_Enable);
			ReSearchDone();
		}
		else {
			if ((elem = Hud_FindElement(name)))
				Hud_Elem_Enable(elem);
			else
				Com_Printf("HudElement \"%s\" not found\n", name);
		}
	}
}

void Hud_List_f(void)
{
	hud_element_t	*elem;
	char			*type;
	char			*param;
	int				c, i, m;

	c = Cmd_Argc();
	if (c > 1)
		if (!ReSearchInit(Cmd_Argv(1)))
			return;

	Com_Printf("List of hud elements:\n");
	for (elem = hud_list, i = m = 0; elem; elem = elem->next, i++) {
		if (c == 1 || ReSearchMatch(elem->name)) {
			if (elem->flags & HUD_CVAR) {
				type = "cvar";
				param = ((cvar_t*)elem->contents)->name;
			}
			else if (elem->flags & HUD_STRING) {
				type = "str";
				param = elem->contents;
			}
			else if (elem->flags & HUD_FUNC) {
				type = "std";
				param = "***";
			}
			else if (elem->flags & HUD_IMAGE) {
				type = "img";
				param = "***";
			}
			else {
				type = "invalid type";
				param = "***";
			}
			m++;
			Com_Printf("%s : %s : %s\n", elem->name, type, param);
		}
	}

	Com_Printf("------------\n%i/%i hud elements\n", m, i);
	if (c > 1)
		ReSearchDone();
}

void Hud_BringToFront_f(void)
{
	hud_element_t *elem, *start, *end;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: hud_bringtofront <name>\n");
		return;
	}

	end = Hud_FindElement(Cmd_Argv(1));
	if (end) {
		if (end->next) {
			start = hud_list;
			hud_list = end->next;
			end->next = NULL;
			for (elem = hud_list; elem->next; elem = elem->next) {}
			elem->next = start;
		}
	}
	else {
		Com_Printf("HudElement \"%s\" not found\n", Cmd_Argv(1));
	}
}

static qbool Hud_TranslateCoords(hud_element_t *elem, int *x, int *y)
{
	int l;

	if (!elem->scr_width || !elem->scr_height)
		return false;

	l = elem->scr_width / 8;

	switch (elem->coords[0]) {
	case 1:	*x = elem->coords[1] * 8 + 1;					// top left
		*y = elem->coords[2] * 8;
		break;
	case 2:	*x = vid.conwidth - (elem->coords[1] + l) * 8 - 1;	// top right
		*y = elem->coords[2] * 8;
		break;
	case 3:	*x = vid.conwidth - (elem->coords[1] + l) * 8 - 1;	// bottom right
		*y = vid.conheight - sb_lines - (elem->coords[2] + 1) * 8;
		break;
	case 4:	*x = elem->coords[1] * 8 + 1;					// bottom left
		*y = vid.conheight - sb_lines - (elem->coords[2] + 1) * 8;
		break;
	case 5:	*x = vid.conwidth / 2 - l * 4 + elem->coords[1] * 8;// top center
		*y = elem->coords[2] * 8;
		break;
	case 6:	*x = vid.conwidth / 2 - l * 4 + elem->coords[1] * 8;// bottom center
		*y = vid.conheight - sb_lines - (elem->coords[2] + 1) * 8;
		break;
	default:
		return false;
	}
	return true;
}

void SCR_DrawHud(void)
{
	hud_element_t* elem;
	int x, y;
	unsigned int l;
	char buf[1024];
	char *st = NULL;
	Hud_Func func;
	double tblink = 0.0;
	mpic_t *img = NULL;

	if (hud_list && cl_hud.value && !((cls.demoplayback || cl.spectator) && cl_restrictions.value)) {
		for (elem = hud_list; elem; elem = elem->next) {
			if (!(elem->flags & HUD_ENABLED)) {
				continue; // do not draw disabled elements
			}

			elem->scr_height = 8;

			if (elem->flags & HUD_CVAR) {
				st = ((cvar_t*)elem->contents)->string;
				strlcpy(buf, st, sizeof(buf));
				st = buf;
				l = strlen_color(st);
			}
			else if (elem->flags & HUD_STRING) {
				Cmd_ExpandString(elem->contents, buf); //, sizeof(buf));
				st = TP_ParseMacroString(buf);
				st = TP_ParseFunChars(st, false);
				l = strlen_color(st);
			}
			else if (elem->flags & HUD_FUNC) {
				func = (Hud_Func)elem->contents;
				st = (*func)();
				l = strlen(st);
			} else if (elem->flags & HUD_IMAGE) {
				img = (mpic_t*)elem->contents;
				l = img->width/8;
				elem->scr_height = img->height;
			}
			else {
				continue;
			}

			if (elem->width && !(elem->flags & (HUD_FUNC | HUD_IMAGE))) {
				if (elem->width < l) {
					l = elem->width;
					st[l] = '\0';
				}
				else {
					while (elem->width > l) {
						st[l++] = ' ';
					}
					st[l] = '\0';
				}
			}
			elem->scr_width = l * 8;

			if (!Hud_TranslateCoords(elem, &x, &y)) {
				continue;
			}

			if (elem->flags & (HUD_BLINK_B | HUD_BLINK_F)) {
				tblink = fmod(cls.realtime, elem->blink) / elem->blink;
			}

			if (!(elem->flags & HUD_BLINK_B) || tblink < 0.5) {
				if (elem->coords[3]) {
					if (elem->alpha < 1) {
						Draw_AlphaFill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3], elem->alpha);
					}
					else {
						Draw_Fill(x, y, elem->scr_width, elem->scr_height, (unsigned char)elem->coords[3]);
					}
				}
			}
			if (!(elem->flags & HUD_BLINK_F) || tblink < 0.5) {
				if (!(elem->flags & HUD_IMAGE)) {
					/*
					int std_charset = char_textures[0];
					if (elem->charset) {
						char_textures[0] = elem->charset;
					}*/
					if (elem->alpha < 1) {
						Draw_AlphaString(x, y, st, elem->alpha);
					}
					else {
						Draw_String(x, y, st);
					}
					//char_textures[0] = std_charset;
				}
				else {
					if (elem->alpha < 1) {
						Draw_AlphaPic(x, y, img, elem->alpha);
					}
					else {
						Draw_Pic(x, y, img);
					}
				}
			}
		}
	}
}

// QW262 -->
void Hud_262Init(void)
{
	//
	// register hud commands
	//
	Cmd_AddCommand("hud262_add", Hud_Add_f);
	Cmd_AddCommand("hud262_remove", Hud_Remove_f);
	Cmd_AddCommand("hud262_position", Hud_Position_f);
	Cmd_AddCommand("hud262_bg", Hud_Bg_f);
	Cmd_AddCommand("hud262_move", Hud262_Move_f);
	Cmd_AddCommand("hud262_width", Hud_Width_f);
	Cmd_AddCommand("hud262_alpha", Hud_Alpha_f);
	Cmd_AddCommand("hud262_blink", Hud_Blink_f);
	Cmd_AddCommand("hud262_disable", Hud_Disable_f);
	Cmd_AddCommand("hud262_enable", Hud_Enable_f);
	Cmd_AddCommand("hud262_list", Hud_List_f);
	Cmd_AddCommand("hud262_bringtofront", Hud_BringToFront_f);
}

void Hud_262CatchStringsOnLoad(char *line)
{
	char *tmpbuff;

	if (Utils_RegExpMatch("^((\\s+)?(?i)hud262_(add|alpha|bg|blink|disable|enable|position|width))", line)) {
		if (hud262_load_buff == NULL) {
			hud262_load_buff = (char*)Q_malloc((strlen(line) + 2) * sizeof(char));
			snprintf(hud262_load_buff, strlen(line) + 2, "%s\n", line);
		}
		else {
			tmpbuff = (char *)Q_malloc(strlen(hud262_load_buff) + 1);
			strcpy(tmpbuff, hud262_load_buff);
			hud262_load_buff = (char *)Q_realloc(hud262_load_buff, (strlen(tmpbuff) + strlen(line) + 2) * sizeof(char));
			snprintf(hud262_load_buff, strlen(tmpbuff) + strlen(line) + 2, "%s%s\n", tmpbuff, line);
			Q_free(tmpbuff);
		}
	}
}
