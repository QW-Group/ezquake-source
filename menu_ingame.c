/**

	In-game menu

	made by johnnycz, June 2007
	last edit:
	$Id: menu_ingame.c,v 1.4 2007-10-07 14:30:31 johnnycz Exp $

*/

#include "quakedef.h"
#include "keys.h"
#include "menu.h"
#include "Ctrl.h"
#include "settings.h"
#include "settings_page.h"

#define TOPMARGIN (6*LETTERWIDTH)

settings_page ingame_menu;
settings_page democtrl_menu;

void MIng_MainMenu(void)		{ M_Menu_Main_f(); }
void MIng_ServerBrowser(void)	{ Cbuf_AddText("menu_slist\n"); }
void MIng_Options(void)			{ Cbuf_AddText("menu_options\n"); }
void MIng_Join(void)			{ Cbuf_AddText("join\n"); }
void MIng_Observe(void)			{ Cbuf_AddText("observe\n"); }
void MDemoCtrl_DemoBrowser(void){ Cbuf_AddText("menu_demos\n"); }
void MIng_Back(void)			{ M_LeaveMenus(); }
void MDemoCtrl_SkipMinute(void)	{ Cbuf_AddText("demo_jump +1:00\n"); }
void MIng_Disconnect(void)		{ Cbuf_AddText("disconnect\n"); }
void MIng_Quit(void)			{ Cbuf_AddText("quit\n"); }

setting ingame_menu_entries[] = {
	ADDSET_SEPARATOR("In-game Menu"),
	ADDSET_ACTION("Join", MIng_Join, ""),
	ADDSET_ACTION("Observe", MIng_Observe, ""),
	ADDSET_ACTION("Server Browser", MIng_ServerBrowser, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_ACTION("Return to game", MIng_Back, ""),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_ACTION("Quit", MIng_Quit, "")
};

setting democtrl_menu_entries[] = {
	ADDSET_SEPARATOR("Demo Control Menu"),
	ADDSET_ACTION("Demo Browser", MDemoCtrl_DemoBrowser, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_ACTION("Skip 1 minute", MDemoCtrl_SkipMinute, ""),
	ADDSET_ACTION("Return to game", MIng_Back, ""),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_ACTION("Quit", MIng_Quit, "")
};

void M_Ingame_Draw(void) {
	M_Unscale_Menu();
	Settings_Draw(0, TOPMARGIN, vid.width, vid.height - TOPMARGIN, &ingame_menu);
}
void M_Democtrl_Draw(void) {
	M_Unscale_Menu();
	Settings_Draw(0, TOPMARGIN, vid.width, vid.height - TOPMARGIN, &democtrl_menu);
}

void M_Ingame_Key(int key) {
	if (Settings_Key(&ingame_menu, key)) return;

	switch (key) {
	case K_MOUSE2:
	case K_ESCAPE: M_LeaveMenus(); break;
	}
}

void M_Democtrl_Key(int key) {
	if (Settings_Key(&democtrl_menu, key)) return;

	switch (key) {
	case K_MOUSE2:
	case K_ESCAPE: M_LeaveMenus(); break;
	}
}

qbool Menu_Ingame_Mouse_Event(const mouse_state_t *ms) {
	mouse_state_t m = *ms;
	m.y -= TOPMARGIN;
	return Settings_Mouse_Event(&ingame_menu, &m);
}
qbool Menu_Democtrl_Mouse_Event(const mouse_state_t *ms) {
	mouse_state_t m = *ms;
	m.y -= TOPMARGIN;
	return Settings_Mouse_Event(&democtrl_menu, &m);
}

void Menu_Ingame_Init(void)
{
	Settings_Page_Init(ingame_menu, ingame_menu_entries);
	Settings_Page_Init(democtrl_menu, democtrl_menu_entries);
}
