/*
Copyright (C) 2011 ezQuake team

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
/**

	Settings page module

	made by johnnycz, Jan 2007
	last edit:
		$Id: settings_page.c,v 1.46 2007-09-30 22:59:23 disconn3ct Exp $

*/

#include "quakedef.h"
#include "settings.h"
#include "Ctrl.h"
#include "Ctrl_EditBox.h"
#include "EX_FileList.h"
#include "help.h"
#include "crc.h"
#include "sbar.h"
#include "menu.h"
#include "keys.h"


CEditBox editbox;

extern cvar_t menu_advanced;

filelist_t skins_filelist;

#define LETW 8
#define LINEHEIGHT 8
#define PADDING 1
#define EDITBOXWIDTH 16
#define EDITBOXMAXLENGTH 64
#define HELPLINES 5
#define SKINPREVIEWHEIGHT 120

#define ENUM_NAME(setting_pointer,position) (setting_pointer->named_ints[position*2])
#define ENUM_VALUE(setting_pointer,position) (setting_pointer->named_ints[position*2+1])
#define ENUM_ITEM_NOT_FOUND	-1

static float SliderPos(float min, float max, float val) { return (val-min)/(max-min); }

static const char* colors[14] = { "White", "Brown", "Lavender", "Khaki", "Red", "Lt Brown", "Peach", "Lt Peach", "Purple", "Dk Purple", "Tan", "Green", "Yellow", "Blue" };
#define COLORNAME(x) colors[bound(0, ((int) x), sizeof(colors) / sizeof(char*) - 1)]

char* SettingColorName(int color)
{
	static char tempBuffer[64];

	if (color < 0 || color > sizeof(colors) / sizeof(colors[0]))
		Host_Error("SettingColorName(color %d) out of range.\n", color);

	strlcpy(tempBuffer, colors[color], sizeof(tempBuffer));

	return tempBuffer;
}

float VARFVAL(const cvar_t *v)
{
	if ((v->flags & CVAR_LATCH) && v->latchedString)
		return atof(v->latchedString);
	else return v->value;
}

#define VARSVAL(x) (((x).flags & CVAR_LATCH) ? ((x).latchedString) : ((x).string))

static int STHeight(setting* s) {
	if (s->advanced && !menu_advanced.value)
	{
		return 0;
	}
	switch (s->type) {
		case stt_separator: return LINEHEIGHT*3;
		case stt_advmark: case stt_basemark: return 0;
		default: return LINEHEIGHT+PADDING;
	}
}

static int Setting_PrintLabel(int x, int y, int w, const char *l, qbool active)
{
	int startpos = x + w/2 - min(strlen(l), w/2)*LETW;
	UI_Print(startpos, y, l, (int) active);
	x = w/2 + x;
	// if (active) UI_DrawCharacter(x, y, FLASHINGARROW());
	return x + LETW*2;
}

// tells what is the horizontal position of the left corner of the slider
static int Slider_Startpos(int w)
{
	return w/2 + LETW*2;
}

static void Setting_DrawIntNum(int x, int y, int w, setting* setting, qbool active)
{
	char buf[16];
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	x = UI_DrawSlider (x, y, SliderPos(setting->min, setting->max, *((int *) setting->cvar)));
	snprintf(buf, sizeof(buf), "%3d", *((int *) setting->cvar));

	UI_Print(x + LETW, y, buf, active);
}

static void Setting_DrawNum(int x, int y, int w, setting* setting, qbool active)
{
	char buf[16];
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	x = UI_DrawSlider (x, y, SliderPos(setting->min, setting->max, VARFVAL(setting->cvar)));
	if (setting->step > 0.99)
		snprintf(buf, sizeof(buf), "%3d", (int) VARFVAL(setting->cvar));
	else
		snprintf(buf, sizeof(buf), "%3.1f", VARFVAL(setting->cvar));

	UI_Print(x + LETW, y, buf, active);
}

static void Setting_DrawBool(int x, int y, int w, setting* setting, qbool active)
{
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	UI_Print(x, y, VARFVAL(setting->cvar) ? "on" : "off", active);
}

static void Setting_DrawBoolAdv(int x, int y, int w, setting* setting, qbool active)
{
	const char* val;
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	val = setting->readfnc ? setting->readfnc() : "off";
	UI_Print(x, y, val, active);
}

static void Setting_DrawSeparator(int x, int y, int w, setting* set)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1d %s \x1f", set->label);
	UI_Print_Center(x, y+LINEHEIGHT+LINEHEIGHT/2, w, buf, true);
}

static void Setting_DrawAction(int x, int y, int w, setting* set, qbool active)
{
	// this will make it centered
	UI_Print_Center(x, y, w, set->label, active);

	// this will make it aligned to the left side from the center
	// Setting_PrintLabel(x, y, w, set->label, active);
}

static void Setting_DrawNamed(int x, int y, int w, setting* set, qbool active)
{
	x = Setting_PrintLabel(x, y, w, set->label, active);
	UI_Print(x, y, set->named_ints[(int) bound(set->min, VARFVAL(set->cvar), set->max)], active);
}

static int Enum_Find_ValueCode(setting* set)
{
	int i;
	for (i = 0; i <= set->max; i++)
	{
		if (!strcmp(set->cvar->string, ENUM_VALUE(set, i)))
			return i;
	}
	return ENUM_ITEM_NOT_FOUND;
}

static void Setting_DrawEnum(int x, int y, int w, setting* set, qbool active)
{
	int i;
	x = Setting_PrintLabel(x, y, w, set->label, active);
	i = Enum_Find_ValueCode(set);
	if (i == ENUM_ITEM_NOT_FOUND) {
		UI_Print(x, y, "custom", active);
	} else {
		UI_Print(x, y, ENUM_NAME(set, i), active);
	}
}

static void Setting_DrawString(int x, int y, int w, setting* setting, qbool active)
{
	int x0 = x;
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	if (active) {
		editbox.width = (w - x + x0) / 8;
		CEditBox_Draw(&editbox, x, y, true);
	} else {
		UI_Print(x, y, VARSVAL(*(setting->cvar)), false);
	}
}

static void Setting_DrawColor(int x, int y, int w, setting* set, qbool active)
{
	x = Setting_PrintLabel(x, y, w, set->label, active);
	if (VARFVAL(set->cvar) >= 0) {
		Draw_Fill(x, y, LETW*3, LINEHEIGHT, Sbar_ColorForMap(VARFVAL(set->cvar)));
		UI_Print(x + 4*LETW, y, va("%i (%s)", (int) VARFVAL(set->cvar), COLORNAME(VARFVAL(set->cvar))), active);
	} else
		UI_Print(x, y, "off", active);
}

static void Setting_DrawSkin(int x, int y, int w, setting* set, qbool active)
{
	x = Setting_PrintLabel(x, y, w, set->label, active);
	UI_Print(x, y, set->cvar->string, active);
}

static void Setting_DrawBind(int x, int y, int w, setting* set, qbool active, qbool bindmode)
{
	int keys[2];
	char *name;
	char c[2];

	c[0] = FLASHINGARROW();
	c[1] = 0;

	x = Setting_PrintLabel(x, y, w, set->label, active);

	if (bindmode && active) {
		UI_Print(x, y, c, active);
	}

	//x += LETW*2;

	M_FindKeysForCommand (set->varname, keys);

	if (keys[0] == -1) {
		UI_Print (x, y, "???", active);
	} else {
		name = Key_KeynumToString (keys[0]);
		UI_Print (x, y, name, active);
		x += strlen(name)*8;
		if (keys[1] != -1) {
			UI_Print (x + 8, y, "or", active);
			UI_Print (x + 4*8, y, Key_KeynumToString (keys[1]), active);
		}
	}

}

static void Setting_IncreaseEnum(setting* set, int step)
{
	int i;
	if (set->type != stt_enum) return;
	i = Enum_Find_ValueCode(set) + step;
	if (i > set->max) i = 0;
	if (i < 0) i = set->max;
	// Cvar_Set doesn't take const char*
	Cvar_Set(set->cvar, va("%s", ENUM_VALUE(set, i)));
}

static void Setting_Increase(setting* set) {
	float newval;

	switch (set->type) {
		case stt_bool: Cvar_Set (set->cvar, VARFVAL(set->cvar) ? "0" : "1"); break;
		case stt_custom: if (set->togglefnc) set->togglefnc(false); break;
		case stt_num:
		case stt_named:
		case stt_playercolor:
					 newval = VARFVAL(set->cvar) + set->step;
					 if (set->max >= newval)
						 Cvar_SetValue(set->cvar, newval);
					 else if (set->type == stt_named || set->type == stt_playercolor)
						 Cvar_SetValue(set->cvar, set->min);
					 break;
		case stt_action: if (set->actionfnc) set->actionfnc(); break;
		case stt_enum: Setting_IncreaseEnum(set, set->step); break;
		case stt_intnum:
			       *((int *)set->cvar) += set->step;
			       if (*((int *) set->cvar) > set->max) {
				       *((int *)set->cvar) = set->max;
			       }
			       break;

			       // unhandled
		case stt_separator:
		case stt_string:
		case stt_bind:
		case stt_skin:
		case stt_advmark:
		case stt_basemark:
		case stt_blank:
			       break;
	}
}

static void Setting_Decrease(setting* set) {
	float newval;

	switch (set->type) {
		case stt_bool: Cvar_Set (set->cvar, VARFVAL(set->cvar) ? "0" : "1"); break;
		case stt_custom: if (set->togglefnc) set->togglefnc(true); break;
		case stt_num:
		case stt_named:
		case stt_playercolor:
					 newval = VARFVAL(set->cvar) - set->step;
					 if (set->min <= newval)
						 Cvar_SetValue(set->cvar, newval);
					 else if (set->type == stt_named || set->type == stt_playercolor)
						 Cvar_SetValue(set->cvar, set->max);
					 break;

		case stt_enum: Setting_IncreaseEnum(set, -set->step); break;
		case stt_intnum:
			       *((int *)set->cvar) -= set->step;
			       if (*((int *) set->cvar) < set->min) {
				       *((int *)set->cvar) = set->min;
			       }
			       break;

			       //unhandled
		case stt_separator:
		case stt_action:
		case stt_string:
		case stt_bind:
		case stt_skin:
		case stt_advmark:
		case stt_basemark:
		case stt_blank:
			       break;
	}
}

static void Setting_Reset(setting* set)
{
	switch (set->type) {
		case stt_num:
		case stt_string:
		case stt_named:
		case stt_bool:
			Cvar_ResetVar(set->cvar);
			break;
			// unhandled
		case stt_bind:
		case stt_separator:
		case stt_custom:
		case stt_enum:
		case stt_action:
		case stt_playercolor:
		case stt_skin:
		case stt_advmark:
		case stt_basemark:
		case stt_intnum:
		case stt_blank:
			break;
	}
}

static void Setting_BindKey(setting* set, int key)
{
	Key_SetBinding(key, set->varname);
}

static void M_UnbindCommand (const char *command) {
	int j, l;
	char *b;

	l = strlen(command) + 1;

	for (j = 0; j < (sizeof(keybindings) / sizeof(*keybindings)); j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_Unbind (j);
	}
}

static void Setting_UnbindKey(setting* set)
{
	M_UnbindCommand(set->varname);
}

static int Settings_PageHeight(const settings_page *page)
{
	return page->settings[page->count - 1].top + STHeight(page->settings + page->count - 1);
}

// will find the lowest number of the setting on 'page' that can be used as the lowest viewpoint
// and will ensure that whole bottom of the page is still visible
static int Settings_LowestViewpoint(const settings_page *page)
{
	int bottom = Settings_PageHeight(page);

	// represents the 'top' number we are looking for
	int best_top = bottom - page->height;

	// presume the lowest viewpoint is the last entry
	int lwp = page->count - 1;

	// this is when the page fits whole on the screen
	if (bottom < page->height) return 0;

	if (!page->count) return 0;

	// it can only get better from now
	while (lwp && page->settings[lwp].top >= best_top) lwp--;

	return lwp + 1;
}

// adjusts current viewed area of the settings page
static void CheckViewpoint(settings_page *tab)
{
	int lwp = Settings_LowestViewpoint(tab);

	tab->viewpoint = bound(0, tab->viewpoint, lwp);

	if (tab->marked == 1 && tab->settings[0].type == stt_separator) tab->viewpoint = 0;

	if (tab->viewpoint > tab->marked) {
		// marked entry is above us
		tab->viewpoint = tab->marked;
	} else while(STHeight(tab->settings + tab->marked) + tab->settings[tab->marked].top > tab->settings[tab->viewpoint].top + tab->height) {
		// marked entry is below
		tab->viewpoint++;
	}
}

static void CheckCursor(settings_page *tab, qbool up)
{	// this makes sure that cursor doesn't point at some meta-entry, section heading or hidden setting
	setting *s;
	while (tab->marked < 0) {
		if (tab->mini) {
			tab->marked = tab->count - 1;
		}
		else {
			tab->marked = 0;
		}
		up = false;
	}
	if (tab->marked >= tab->count) {
		if (tab->mini) {
			tab->marked = 1;
		}
		else {
			tab->marked = tab->count - 1;
		}
		up = true;
	}
	s = tab->settings + tab->marked;
	if (s->type == stt_separator || (s->advanced && !menu_advanced.value) || s->type == stt_advmark || s->type == stt_basemark || s->type == stt_blank) {
		tab->marked += up ? -1 : +1;
		CheckCursor(tab, up);
	}
}

static void StringEntryLeave(setting* set) {
	Cvar_Set(set->cvar, editbox.text);
}

static void StringEntryEnter(setting* set) {
	CEditBox_Init(&editbox, EDITBOXWIDTH, EDITBOXMAXLENGTH);
	strlcpy(editbox.text, set->cvar->string, EDITBOXMAXLENGTH);
}

static void EditBoxCheck(settings_page* tab, int oldm, int newm)
{
	if (tab->settings[oldm].type == stt_string && oldm != newm)
		StringEntryLeave(tab->settings + oldm);
	if (tab->settings[newm].type == stt_string && oldm != newm)
		StringEntryEnter(tab->settings + newm);
}

static void RecalcPositions(settings_page* page)
{	// especially when "show advanced options" is being changed
	// we need to recalculate where each option is
	int i;
	int curtop = 0;
	setting *s;

	for (i = 0; i < page->count; i++)
	{
		s = page->settings + i;
		s->top = curtop;

		curtop += STHeight(s);
	}
}

static int Setting_DrawHelpBox(int x, int y, int w, int h, settings_page* page, qbool full)
{
	setting* s;
	char buf[2048];
	const char *helptext = "";
	int maxh;
	const char *cp;

	s = page->settings + page->marked;

	switch (s->type) {
		case stt_bool:
		case stt_named:
		case stt_num:
		case stt_string:
		case stt_playercolor:
		case stt_enum:
			buf[0] = 0;
			helptext = "Further info not available...";
			Help_VarDescription (s->cvar->name, buf, sizeof(buf));
			helptext = buf;
			break;

		case stt_bind:
			if (page->mode == SPM_BINDING)
				helptext = "Press the key you want to assiciate with given action";
			else
				helptext = "Press Enter to change the associated key; Press Del to remove the binding";
			break;

		case stt_skin:
			helptext = "Press [Enter] to choose a skin image for this type of player";
			break;

		default:
			if (s->description)
				helptext = s->description;
			break;
	}

	if (full) {
		// add some lines for wrapped words, just a rough approximation here
		maxh = (int) ((((double) strlen(helptext) / (w / LETW - 2)) + 2) * 1.33) * LETW;
		cp = helptext;
		while (*cp) { if (*cp++ == '\n') maxh += LETW; } // add new line for each newline
		maxh = max((HELPLINES + 2) * LETW, maxh);
		if (maxh < h) {
			y += h - maxh;
			h = maxh;
		}
	}

	UI_DrawBox(x, y, w, h);

	if (!UI_PrintTextBlock(x + LETW, y + LETW, w - LETW*2, h - LETW*2, helptext, false) && !full)
		UI_Print(x + LETW, y + h - LETW, "Press [F1] to read more...", true);

	return h;
}

// will draw the skin preview in given window
static void Setting_DrawSkinPreview(int x, int y, int w, int h, char *skinfile)
{
	static mpic_t *curpic = NULL;
	static char lastpicname[MAX_PATH] = "";

	if (!skinfile) return;

	// this means the length of "qw/"
#define QWDIRLEN 3

	UI_DrawBox(x, y, w, h);
	x += LETTERWIDTH;
	y += LETTERHEIGHT;
	w -= LETTERWIDTH;
	h -= LETTERHEIGHT;

	if (strcmp(lastpicname, skinfile))
	{
		char *c;
		char buf[MAX_PATH];

		// get the "qw/skins/freddy" part from the full path
		if (strlen(skinfile) <= strlen(com_basedir) + QWDIRLEN)
		{
			return;
		}
		c = skinfile + strlen(com_basedir) + QWDIRLEN + 1;
		COM_StripExtension(c, buf, sizeof(buf));

		curpic = Draw_CachePicSafe(buf, false, true);
		strlcpy(lastpicname, skinfile, sizeof(lastpicname));
	}

	if (curpic)
	{
		Draw_FitPic(x, y, w, h, curpic);
	}
#undef QWDIRLEN
}

static int FindSetting_AtPos(const settings_page *page, int top)
{
	int i, r = 0;
	for (i = 0; i < page->count; i++)
	{
		if (page->settings[i].top < top)
			r = i;
	}
	return r;
}

// will choose correct value for the selected setting according to
// where the user has clicked on the slidebar
static void Setting_Slider_Click(const settings_page *page, const mouse_state_t *ms)
{
	double p, vmin, vmax, vnew, vsteps;
	setting* s = page->settings + page->marked;

	p = (ms->x - Slider_Startpos(page->width)) / UI_SliderWidth();
	p = bound(0, p, 1);

	if (s->type != stt_num && s->type != stt_intnum) return;

	vmin = s->min;
	vmax = s->max;
	vnew = vmin + (vmax-vmin) * p;
	for (vsteps = vmin; vsteps < vmax && vsteps < vnew; vsteps += s->step) ; // empty body

	if (s->type == stt_intnum) {
		*((int *) s->cvar) = vsteps;
	}
	else {
		Cvar_SetValue(s->cvar, vsteps);
	}
}

qbool Settings_Key(settings_page* tab, int key, wchar unichar)
{
	qbool up = false;
	setting_type type;
	int oldm = tab->marked;
	char *skinpath;
	qbool skip_check_viewpoint = false;

	type = tab->settings[tab->marked].type;

	if (tab->mode == SPM_BINDING) {
		if (key != K_ESCAPE)
			Setting_BindKey(tab->settings + tab->marked, key);

		tab->mode = SPM_NORMAL;
		return true;
	}

	if (tab->mode == SPM_CHOOSESKIN) {
		if (key == K_ENTER || key == K_MOUSE1) {
			char buf[MAX_PATH];
			skinpath = FL_GetCurrentPath(&skins_filelist);
			if (skinpath) {
				COM_StripExtension(COM_SkipPath(skinpath), buf, sizeof(buf));
				if (strcmp(buf, "."))
					Cvar_Set(tab->settings[tab->marked].cvar, buf);
			}
			tab->mode = SPM_NORMAL;
			return true;
		}

		if (key == K_ESCAPE || key == K_MOUSE2) {
			tab->mode = SPM_NORMAL;
			return true;
		}

		return FL_Key(&skins_filelist, key);
	}

	if (tab->mode == SPM_VIEWHELP) {
		tab->mode = SPM_NORMAL;
		return true;
	}

	switch (key) {
		case K_DOWNARROW:   tab->marked++; break;
		case K_UPARROW:     tab->marked--; up = true; break;
		case K_MWHEELDOWN:
				    if (tab->viewpoint < Settings_LowestViewpoint(tab))
					    tab->viewpoint++;

				    skip_check_viewpoint = true;
				    break;

		case K_MWHEELUP:
				    if (tab->viewpoint > 0)
					    tab->viewpoint--;
				    skip_check_viewpoint = true;
				    break;

		case K_PGDN: tab->marked += 5; break;
		case K_PGUP: tab->marked -= 5; up = true; break;
		case K_END: tab->marked = tab->count - 1; up = true; break;
		case K_HOME: tab->marked = 0; break;

		case K_RIGHTARROW:
			switch (type) {
				case stt_string:
					CEditBox_Key(&editbox, key, unichar);
					break;
				default:
					Setting_Increase(tab->settings + tab->marked);
					break;
			}
			break;
		case K_LEFTARROW:
			switch (type) {
				case stt_string:
					CEditBox_Key(&editbox, key, unichar);
					break;
				default:
					Setting_Decrease(tab->settings + tab->marked);
					break;
			}
			break;
		case K_ENTER:
		case K_MOUSE1:
		case '=':
		case KP_PLUS:
			     switch (type) {
				     case stt_string: StringEntryLeave(tab->settings + tab->marked); break;
				     case stt_bind: tab->mode = SPM_BINDING; break;
				     case stt_skin: tab->mode = SPM_CHOOSESKIN; break;
				     default: Setting_Increase(tab->settings + tab->marked); break;
			     }
			     return true;

		case K_BACKSPACE: case '-': case KP_MINUS:
			     switch (type) {
				     case stt_action: return false;
				     case stt_string: CEditBox_Key(&editbox, key, unichar); return true;
				     default: Setting_Decrease(tab->settings + tab->marked);	return true;
			     }

		case K_DEL:
			     switch (type) {
				     case stt_string: CEditBox_Key(&editbox, key, unichar); return true;
				     case stt_bind: Setting_UnbindKey(tab->settings + tab->marked); return true;
				     default: Setting_Reset(tab->settings + tab->marked); return true;
			     }

		case K_F1:
		case K_INS:
			     switch (tab->mode) {
				     case SPM_NORMAL: tab->mode = SPM_VIEWHELP; return true;
				     case SPM_VIEWHELP: tab->mode = SPM_NORMAL; return true;
							// unhandled
				     case SPM_BINDING:
				     case SPM_CHOOSESKIN:
							break;
			     }
			     break;

		case K_MOUSE2:
			     if (tab->mode == SPM_VIEWHELP) {
				     tab->mode = SPM_NORMAL; return true;
			     } else return false;

		case K_ESCAPE:
			     if (tab->mode == SPM_VIEWHELP) {
				     tab->mode = SPM_NORMAL;
				     return true;
			     } else return false;

		default:
			     switch (type) {
				     case stt_string:
					     if (key != K_TAB && key != K_ESCAPE && key != K_LEFTARROW && key != K_RIGHTARROW) {
						     CEditBox_Key(&editbox, key, unichar);
						     return true;
					     }
					     return false;

				     default: return false;
			     }
	}

	CheckCursor(tab, up);

	if (!skip_check_viewpoint)
		CheckViewpoint(tab);

	EditBoxCheck(tab, oldm, tab->marked);
	return true;
}

static void Setting_Click(settings_page* page, const mouse_state_t *ms)
{
	// don't accept clicks on the label
	if (ms->x < page->width/2 && (page->settings + page->marked)->type != stt_action) return;

	if (page->settings[page->marked].type == stt_num) {
		Setting_Slider_Click(page, ms);
	} else Settings_Key(page, K_MOUSE1, 0);
}

static void Settings_AdjustScrollBar(settings_page *page)
{
	double lwp = Settings_LowestViewpoint(page);
	double percentage = lwp ? (double) page->viewpoint / lwp : 0;

	// do not adjust the scrollbar if we are scrolling, it would look weird
	if (page->scrollbar->mouselocked) return;

	page->scrollbar->curpos = bound(0, percentage, 1);
}

void Settings_Draw(int x, int y, int w, int h, settings_page* tab)
{
	int i;
	int ch;
	//int nexttop;
	int hbh = 0;	// help box height
	setting *set;
	qbool active;
	static qbool prev_adv_state = false;

	if (!tab->count) return;

	if (prev_adv_state != (qbool) menu_advanced.value) {
		// someone toggled menu_advanced setting right in the currently viewed menu!
		RecalcPositions(tab);
		prev_adv_state = (qbool) menu_advanced.value;
	}

	//nexttop = tab->settings[0].top;

	if (tab->mode == SPM_CHOOSESKIN)
	{
		FL_Draw(&skins_filelist, x, y, w, h - SKINPREVIEWHEIGHT);
		Setting_DrawSkinPreview(x, y + h - SKINPREVIEWHEIGHT, w, SKINPREVIEWHEIGHT, FL_GetCurrentPath(&skins_filelist));
		return;
	}

	w -= tab->scrollbar->width;

	if (!tab->mini) {
		if (tab->mode != SPM_VIEWHELP) {
			hbh = HELPLINES * LINEHEIGHT;
			h -= Setting_DrawHelpBox(x, y + h - hbh, w, hbh, tab, false);
		} else {
			hbh = h - LINEHEIGHT * 4;
			h -= Setting_DrawHelpBox(x, y + h - hbh, w, hbh, tab, true);
		}
	}

	tab->width = w; tab->height = h;

	Settings_AdjustScrollBar(tab);
	if (tab->height < Settings_PageHeight(tab))
		ScrollBar_Draw(tab->scrollbar, x + w, y, h);

	for (i = tab->viewpoint; i < tab->count && tab->settings[i].top + STHeight(tab->settings + i) <= h + tab->settings[tab->viewpoint].top; i++)
	{
		active = i == tab->marked;
		set = tab->settings + i;
		ch = STHeight(tab->settings + i);
		if ((set->advanced && !menu_advanced.value) || set->type == stt_advmark || set->type == stt_basemark) continue;

		if (active && set->type != stt_separator) {
			UI_DrawGrayBox(x, y, w, ch);
		}
		switch (set->type) {
			case stt_bool: if (set->cvar) Setting_DrawBool(x, y, w, set, active); break;
			case stt_custom: Setting_DrawBoolAdv(x, y, w, set, active); break;
			case stt_num: Setting_DrawNum(x, y, w, set, active); break;
			case stt_intnum: Setting_DrawIntNum(x, y, w, set, active); break;
			case stt_separator: Setting_DrawSeparator(x, y, w, set); break;
			case stt_action: Setting_DrawAction(x, y, w, set, active); break;
			case stt_named: Setting_DrawNamed(x, y, w, set, active); break;
			case stt_enum: Setting_DrawEnum(x, y, w, set, active); break;
			case stt_string: Setting_DrawString(x, y, w, set, active); break;
			case stt_playercolor: Setting_DrawColor(x, y, w, set, active); break;
			case stt_skin: Setting_DrawSkin(x, y, w, set, active); break;
			case stt_bind: Setting_DrawBind(x, y, w, set, active, tab->mode == SPM_BINDING); break;
				       // unhandled
			case stt_advmark:
			case stt_basemark:
			case stt_blank:
				       break;
		}
		y += ch;
		//if (i < tab->count)
		//	nexttop = tab->settings[i+1].top;
	}
}

void Settings_OnShow(settings_page *page)
{
	RecalcPositions(page);

	CheckCursor(page, false);

	if (page->settings[page->marked].type == stt_string)
		StringEntryEnter(page->settings + page->marked);
}

qbool Settings_Mouse_Event(settings_page *page, const mouse_state_t *ms)
{
	int nmark;
	int omark = page->marked;

	// scrollbar associated with this page handles the event
	// page has to be in normal mode and user is already scrolling or
	// just started scrolling
	if (page->mode == SPM_NORMAL && (page->scrollbar->mouselocked ||
				(ms->x > (page->width - page->scrollbar->width))))
	{
		if (ScrollBar_MouseEvent(page->scrollbar, ms))
		{
			page->viewpoint = Settings_LowestViewpoint(page) * page->scrollbar->curpos;
		}

		return true;
	}

	if (page->mode == SPM_NORMAL && ms->button_up == 1) {
		Setting_Click(page, ms);
		return true;
	}

	switch (page->mode) {
		case SPM_BINDING:
			if (ms->button_down) return true;
			if (ms->button_up) {
				Setting_BindKey(page->settings + page->marked, K_MOUSE1 + ms->button_up - 1);
				page->mode = SPM_NORMAL;
				return true;
			}
			break;

		case SPM_CHOOSESKIN:
			if (FL_Mouse_Event(&skins_filelist, ms)) return true;
			else if (ms->button_up == 1) Settings_Key(page, K_MOUSE1, 0);
			return true;
			break;

		case SPM_NORMAL:
			nmark = FindSetting_AtPos(page, page->settings[page->viewpoint].top + ms->y);
			nmark = bound(0, nmark, page->count - 1);
			if ((page->settings[nmark].type == stt_num || page->settings[nmark].type == stt_intnum)
					&& ms->buttons[1] == true)
			{
				Setting_Slider_Click(page, ms);
			}
			else if (page->settings[nmark].type != stt_separator)
			{
				page->marked = nmark;
				CheckCursor(page, ms->x < ms->x_old);
				EditBoxCheck(page, omark, nmark);
			}
			return true;

		case SPM_VIEWHELP:
			return false;
			break;
	}
	return false;
}

void Settings_Init(settings_page *page, setting *arr, size_t size)
{
	int i;
	qbool onlyseparators = true;
	qbool advancedmode = false;

	page->count = size;
	page->marked = 0;
	page->settings = arr;
	page->viewpoint = 0;
	page->mode = SPM_NORMAL;
	page->scrollbar = ScrollBar_Create(NULL);
	page->mini = false;

	for (i = 0; i < size; i++) {
		if (arr[i].type == stt_advmark) advancedmode = true;
		if (arr[i].type == stt_basemark) advancedmode = false;
		arr[i].advanced = advancedmode;

		if (onlyseparators && arr[i].type != stt_separator) {
			onlyseparators = false;
			page->marked = i;
		}
		if (arr[i].varname && !arr[i].cvar && arr[i].type == stt_bool) {
			arr[i].cvar = Cvar_Find(arr[i].varname);
			arr[i].varname = NULL;
			if (!arr[i].cvar)
				Cbuf_AddText(va("Warning: variable %s not found\n", arr[i].varname));
		}
	}

	RecalcPositions(page);

	if (onlyseparators) {
		Cbuf_AddText("Warning (Settings_Init): menu contained only separators\n");
		page->count = 0;
	}
}

void Settings_MainInit(void)
{
	FL_Init(&skins_filelist, "./qw/skins");

	FL_SetDirUpOption(&skins_filelist, false);
	FL_SetDirsOption(&skins_filelist, false);
	FL_AddFileType(&skins_filelist, 0, ".pcx");
	FL_AddFileType(&skins_filelist, 1, ".png");
	FL_AddFileType(&skins_filelist, 2, ".jpg");
	FL_AddFileType(&skins_filelist, 3, ".tga");
}
