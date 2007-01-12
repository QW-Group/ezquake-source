/**
	
	Settings page module

	made by johnnycz, Jan 2007
	last edit:
		$Id: settings_page.c,v 1.3 2007-01-12 16:26:06 johnnycz Exp $

*/

#include "quakedef.h"
#include "settings.h"

#define LETW 8
#define COL1WIDTH 16

static float SliderPos(float min, float max, float val) { return (val-min)/(max-min); }

static int Setting_PrintLabel(int x, int y, int w, const char *l, qbool active)
{
	UI_Print(x + (COL1WIDTH - min(strlen(l), COL1WIDTH))*LETW, y, l, (int) active);
	x += COL1WIDTH * LETW;
	if (active)
		UI_DrawCharacter(x, y, FLASHINGARROW());
	return x + LETW*2;
}

static void Setting_DrawNum(int x, int y, int w, setting* setting, qbool active)
{
	char buf[16];
	x = Setting_PrintLabel(x,y,w, setting->label, active);
	x = UI_DrawSlider (x, y, SliderPos(setting->min, setting->max, setting->cvar->value));
	snprintf(buf, sizeof(buf), "%1.1f", setting->cvar->value);
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
	UI_Print(x, y, buf, true);
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
			else 
				Cvar_SetValue(set->cvar, set->min);
			break;
		case stt_action: if (set->readfnc) set->actionfnc(); break;
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
			else
				Cvar_SetValue(set->cvar, set->max);
			break;
	}
}

static void CheckCursor(settings_tab *tab, qbool up)
{
	while (tab->set_marked < 0) tab->set_marked += tab->set_count;
	tab->set_marked = tab->set_marked % tab->set_count;

	if (tab->set_tab[tab->set_marked].type == stt_separator) {
		if (up) {
			if (tab->set_marked == 0)
				tab->set_marked = tab->set_count - 1;
			else
				tab->set_marked--;
		} else tab->set_marked++;
		CheckCursor(tab, up);
	}
}

qbool Settings_Key(settings_tab* tab, int key)
{
	qbool up = false;
	setting_type type;
	CheckCursor(tab, up);

	type = tab->set_tab[tab->set_marked].type;

	switch (key) { 
	case K_DOWNARROW: tab->set_marked++; break;
	case K_UPARROW: tab->set_marked--; up = true; break;
	case K_PGDN: tab->set_marked += 5; break;
	case K_PGUP: tab->set_marked -= 5; up = true; break;
	case K_END: tab->set_marked = tab->set_count - 1; up = true; break;
	case K_HOME: tab->set_marked = 0; break;
	case K_RIGHTARROW:
		if (type != stt_action) {
			Setting_Increase(tab->set_tab + tab->set_marked);
			return true;
		} else return false;

	case K_ENTER:
		Setting_Increase(tab->set_tab + tab->set_marked);
		return true;

	case K_LEFTARROW: 
		if (type != stt_action) {
			Setting_Decrease(tab->set_tab + tab->set_marked);
			return true;
		} else return false;
	default: return false; break;
	}

	CheckCursor(tab, up);
	return true;
}

void Settings_Draw(int x, int y, int w, int h, settings_tab* tab)
{
	int i;
	setting *set;
	qbool active;

	CheckCursor(tab, false);

	for (i = 0; i < tab->set_count; i++)
	{
		active = i == tab->set_marked;
		set = tab->set_tab + i;
		switch (set->type) {
			case stt_bool: Setting_DrawBool(x, y, w, set, active); break;
			case stt_custom: Setting_DrawBoolAdv(x, y, w, set, active); break;
			case stt_num: Setting_DrawNum(x, y, w, set, active); break;
			case stt_separator: Setting_DrawSeparator(x, y, w, set); break;
			case stt_action: Setting_DrawAction(x, y, w, set, active); break;
			case stt_named: Setting_DrawNamed(x, y, w, set, active); break;
		}
		y += LETW;
	}
}
