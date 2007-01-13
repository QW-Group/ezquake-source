/**
	
	Settings page module

	made by johnnycz, Jan 2007
	last edit:
		$Id: settings_page.c,v 1.6 2007-01-13 11:13:17 johnnycz Exp $

*/

#include "quakedef.h"
#include "settings.h"

#define LETW 8
#define LINEHEIGHT 8

static float SliderPos(float min, float max, float val) { return (val-min)/(max-min); }

static int STHeight(setting_type st) {
	switch (st) {
	case stt_separator: return LINEHEIGHT*2;
	default: return LINEHEIGHT;
	}
}

static int Setting_PrintLabel(int x, int y, int w, const char *l, qbool active)
{
	int startpos = x + w/2 - min(strlen(l), w/2)*LETW;
	UI_Print(startpos, y, l, (int) active);
	x = w/2 + x;
	if (active)
		UI_DrawCharacter(x, y, FLASHINGARROW());
	return x + LETW*2;
}

static void Setting_DrawNum(int x, int y, int w, setting* setting, qbool active)
{
	char buf[16];
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	x = UI_DrawSlider (x, y, SliderPos(setting->min, setting->max, setting->cvar->value));
	if (setting->step > 0.99)
		snprintf(buf, sizeof(buf), "%3d", (int) setting->cvar->value);
	else
		snprintf(buf, sizeof(buf), "%3.1f", setting->cvar->value);

	UI_Print(x + LETW, y, buf, active);
}

static void Setting_DrawBool(int x, int y, int w, setting* setting, qbool active)
{
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	UI_Print(x, y, setting->cvar->value ? "on" : "off", active);
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
	UI_Print_Center(x, y+LINEHEIGHT/2, w, buf, true);
}

static void Setting_DrawAction(int x, int y, int w, setting* set, qbool active)
{
	Setting_PrintLabel(x, y, w, set->label, active);
}

static void Setting_DrawNamed(int x, int y, int w, setting* set, qbool active)
{
	x = Setting_PrintLabel(x, y, w, set->label, active);
	UI_Print(x, y, set->named_ints[(int) bound(set->min, set->cvar->value, set->max)], active);
}

static void Setting_Increase(setting* set) {
	float newval;

	switch (set->type) {
		case stt_bool: Cvar_Set (set->cvar, set->cvar->value ? "0" : "1"); break;
		case stt_custom: if (set->togglefnc) set->togglefnc(false); break;
		case stt_num:
		case stt_named:
			newval = set->cvar->value + set->step;
			if (set->max >= newval)
				Cvar_SetValue(set->cvar, newval);
			else if (set->type == stt_named)
				Cvar_SetValue(set->cvar, set->min);
			break;
		case stt_action: if (set->actionfnc) set->actionfnc(); break;
	}
}

static void Setting_Decrease(setting* set) {
	float newval;

	switch (set->type) {
		case stt_bool: Cvar_Set (set->cvar, set->cvar->value ? "0" : "1"); break;
		case stt_custom: if (set->togglefnc) set->togglefnc(true); break;
		case stt_num:
		case stt_named:
			newval = set->cvar->value - set->step;
			if (set->min <= newval)
				Cvar_SetValue(set->cvar, newval);
			else if (set->type == stt_named)
				Cvar_SetValue(set->cvar, set->max);
			break;
	}
}

static void CheckViewpoint(settings_page *tab, int h)
{
	if (tab->viewpoint > tab->marked) { 
	// marked entry is above us
		tab->viewpoint = tab->marked;
	} else while(STHeight(tab->settings[tab->marked].type) + tab->settings[tab->marked].top > tab->settings[tab->viewpoint].top + h) {
		// marked entry is below
		tab->viewpoint++;
	}
}

static void CheckCursor(settings_page *tab, qbool up)
{
	while (tab->marked < 0) tab->marked += tab->count;
	tab->marked = tab->marked % tab->count;

	if (tab->settings[tab->marked].type == stt_separator) {
		if (up) {
			if (tab->marked == 0)
				tab->marked = tab->count - 1;
			else
				tab->marked--;
		} else tab->marked++;
		CheckCursor(tab, up);
	}
}

qbool Settings_Key(settings_page* tab, int key)
{
	qbool up = false;
	setting_type type;
	CheckCursor(tab, up);

	type = tab->settings[tab->marked].type;

	switch (key) { 
	case K_DOWNARROW: tab->marked++; break;
	case K_UPARROW: tab->marked--; up = true; break;
	case K_PGDN: tab->marked += 5; break;
	case K_PGUP: tab->marked -= 5; up = true; break;
	case K_END: tab->marked = tab->count - 1; up = true; break;
	case K_HOME: tab->marked = 0; break;
	case K_RIGHTARROW:
		if (type != stt_action) {
			Setting_Increase(tab->settings + tab->marked);
			return true;
		} else return false;

	case K_ENTER:
		Setting_Increase(tab->settings + tab->marked);
		return true;

	case K_LEFTARROW: 
		if (type != stt_action) {
			Setting_Decrease(tab->settings + tab->marked);
			return true;
		} else return false;
	default: return false; break;
	}

	CheckCursor(tab, up);
	return true;
}

void Settings_Draw(int x, int y, int w, int h, settings_page* tab)
{
	int i;
	int nexttop;
	setting *set;
	qbool active;

	if (!tab->count) return;

	nexttop = tab->settings[0].top;

	CheckCursor(tab, false);
	CheckViewpoint(tab, h);

	for (i = tab->viewpoint; i < tab->count && tab->settings[i].top + STHeight(tab->settings[i].type) <= h + tab->settings[tab->viewpoint].top; i++)
	{
		active = i == tab->marked;
		set = tab->settings + i;

		switch (set->type) {
			case stt_bool: if (set->cvar) Setting_DrawBool(x, y, w, set, active); break;
			case stt_custom: Setting_DrawBoolAdv(x, y, w, set, active); break;
			case stt_num: Setting_DrawNum(x, y, w, set, active); break;
			case stt_separator: Setting_DrawSeparator(x, y, w, set); break;
			case stt_action: Setting_DrawAction(x, y, w, set, active); break;
			case stt_named: Setting_DrawNamed(x, y, w, set, active); break;
		}
		y += STHeight(tab->settings[i].type);
		if (i < tab->count)
			nexttop = tab->settings[i+1].top;
	}
}

void Settings_Init(settings_page *page, setting *arr, size_t size)
{
	int i;
	int curtop = 0;

	page->count = size;
	page->marked = 0;
	page->settings = arr;
	page->viewpoint = 0;

	for (i = 0; i < size; i++) {
		arr[i].top = curtop;
		curtop += STHeight(arr[i].type);
		if (arr[i].varname && !arr[i].cvar && arr[i].type == stt_bool) {
			arr[i].cvar = Cvar_FindVar(arr[i].varname);
			arr[i].varname = NULL;
			if (!arr[i].cvar)
				Cbuf_AddText(va("Warning: variable %s not found\n", arr[i].varname));
		}
	}
}
