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
settings_page botmatch_menu;

#define MENU_ALIAS(func,command,leavem) static void func(void) { Cbuf_AddText(command "\n"); if (leavem) M_LeaveMenus(); }

void MIng_MainMenu(void)		{ M_Menu_Main_f(); }
void MIng_Back(void)			{ M_LeaveMenus(); }

MENU_ALIAS(MIng_ServerBrowser,"menu_slist",false);
MENU_ALIAS(MIng_Options,"menu_options",false);
MENU_ALIAS(MIng_Join,"join",true);
MENU_ALIAS(MIng_Observe,"observe",true);
MENU_ALIAS(MDemoCtrl_DemoBrowser,"menu_demos",false);
MENU_ALIAS(MDemoCtrl_SkipMinute,"demo_jump +1:00",false);
MENU_ALIAS(MIng_Disconnect,"disconnect",true);
//MENU_ALIAS(MIng_Quit,"quit",false);
MENU_ALIAS(MIng_Ready, "ready",true);
MENU_ALIAS(MIng_Break, "break",true);
MENU_ALIAS(MIng_SkillUp, "skillup",false);
MENU_ALIAS(MIng_SkillDown, "skilldown",false);
MENU_ALIAS(MIng_AddBot, "addbot",false);
MENU_ALIAS(MIng_RemoveBot, "removebot",false);
MENU_ALIAS(MIng_TeamBlue, "team blue;color 13",true);
MENU_ALIAS(MIng_TeamRed, "team red;color 4",true);
MENU_ALIAS(MDemoCtrl_Skip10Sec, "demo_jump +0:10",false);
MENU_ALIAS(MDemoCtrl_Back1Min, "demo_jump -1:00",false);
MENU_ALIAS(MDemoCtrl_Back10Sec, "demo_jump -0:10", false);

setting ingame_menu_entries[] = {
	ADDSET_SEPARATOR("In-game Menu"),
	ADDSET_ACTION("Ready", MIng_Ready, ""),
	ADDSET_ACTION("Break", MIng_Break, ""),
	ADDSET_ACTION("Join", MIng_Join, ""),
	ADDSET_ACTION("Observe", MIng_Observe, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_ACTION("Server Browser", MIng_ServerBrowser, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
};

setting democtrl_menu_entries[] = {
	ADDSET_SEPARATOR("Demo Control Menu"),
	ADDSET_ACTION("Rewind 1 Min", MDemoCtrl_Back1Min, ""),
	ADDSET_ACTION("Rewind 10 Sec", MDemoCtrl_Back10Sec, ""),
	ADDSET_ACTION("Skip 1 Min", MDemoCtrl_SkipMinute, ""),
	ADDSET_ACTION("Skip 10 Sec", MDemoCtrl_Skip10Sec, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_ACTION("Demo Browser", MDemoCtrl_DemoBrowser, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Demo", MIng_Back, ""),
};

setting botmatch_menu_entries[] = {
	ADDSET_SEPARATOR("Botmatch Menu"),
	ADDSET_ACTION("Ready", MIng_Ready, ""),
	ADDSET_ACTION("Break", MIng_Break, ""),
	ADDSET_ACTION("Team Blue", MIng_TeamBlue, ""),
	ADDSET_ACTION("Team Red", MIng_TeamRed, ""),
	ADDSET_ACTION("Add Bot", MIng_AddBot, ""),
	ADDSET_ACTION("Remove Bot", MIng_RemoveBot, ""),
	ADDSET_ACTION("Increase Bot Skill", MIng_SkillUp, ""),
	ADDSET_ACTION("Decrease Bot Skill", MIng_SkillDown, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
};

#define DEMOPLAYBACK() (cls.demoplayback || cls.mvdplayback)
#define BOTMATCH() (!strcmp(cls.gamedirfile, "fbca"))

static settings_page *M_Ingame_Current(void) {
	if (DEMOPLAYBACK()) {
		return &democtrl_menu;
	}
	else if (BOTMATCH()) {
		return &botmatch_menu;
	}
	else {
		return &ingame_menu;
	}
}

void M_Ingame_Draw(void) {
	M_Unscale_Menu();
	Settings_Draw(0, TOPMARGIN, vid.width, vid.height - TOPMARGIN, M_Ingame_Current());
}

void M_Ingame_Key(int key) {
	if (Settings_Key(M_Ingame_Current(), key, 0)) return;

	switch (key) {
	case K_MOUSE2:
	case K_ESCAPE: M_LeaveMenus(); break;
	}
}

qbool Menu_Ingame_Mouse_Event(const mouse_state_t *ms) {
	mouse_state_t m = *ms;
	m.y -= TOPMARGIN;
	return Settings_Mouse_Event(M_Ingame_Current(), &m);
}

void Menu_Ingame_Init(void)
{
	Settings_Page_Init(ingame_menu, ingame_menu_entries);
	Settings_Page_SetMinit(ingame_menu);
	Settings_Page_Init(democtrl_menu, democtrl_menu_entries);
	Settings_Page_SetMinit(democtrl_menu);
	Settings_Page_Init(botmatch_menu, botmatch_menu_entries);
	Settings_Page_SetMinit(botmatch_menu);
}
