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
	\file

	\brief
	In-game menu

	\author
	johnnycz
**/

#include "quakedef.h"
#include "keys.h"
#include "menu.h"
#include "Ctrl.h"
#include "settings.h"
#include "settings_page.h"
#include "server.h"
#include "version.h"

#define TOPMARGIN (6*LETTERWIDTH)

settings_page single_menu;
settings_page ingame_menu;
settings_page democtrl_menu;
settings_page botmatch_menu;
settings_page qtv_menu;

#define MENU_ALIAS(func,command,leavem) static void func(void) { Cbuf_AddText(command "\n"); if (leavem) M_LeaveMenus(); }

void MIng_MainMenu(void)		{ M_Menu_Main_f(); }
void MIng_Back(void)			{ M_LeaveMenus(); }

MENU_ALIAS(MIng_ServerBrowser,"menu_slist",false);
MENU_ALIAS(MIng_Options,"menu_options",false);
MENU_ALIAS(MIng_Join,"join",true);
MENU_ALIAS(MIng_Observe,"observe",true);
MENU_ALIAS(MIng_Disconnect,"disconnect",true);
MENU_ALIAS(MIng_Ready, "ready",true);
MENU_ALIAS(MIng_Break, "break",true);
MENU_ALIAS(MIng_SkillUp, "skillup",false);
MENU_ALIAS(MIng_SkillDown, "skilldown",false);
MENU_ALIAS(MIng_AddBot, "addbot",false);
MENU_ALIAS(MIng_RemoveBot, "removebot",false);
MENU_ALIAS(MIng_TeamBlue, "team blue;color 13",true);
MENU_ALIAS(MIng_TeamRed, "team red;color 4",true);
MENU_ALIAS(MDemoCtrl_DemoBrowser,"menu_demos",false);
MENU_ALIAS(MDemoCtrl_DemoControls, "demo_controls",true);
MENU_ALIAS(MSP_Load, "menu_load", false);
MENU_ALIAS(MSP_Save, "menu_save", false);
MENU_ALIAS(MQTV_Autotrack, "autotrack", true);
MENU_ALIAS(MQTV_Lastscores, "lastscores", true);
MENU_ALIAS(MQTV_Observers, "qtvusers", true);
MENU_ALIAS(MQTV_Reconnect, "qtvreconnect", true);

setting single_menu_entries[] = {
	ADDSET_SEPARATOR("In-game Menu"),
	ADDSET_ACTION("Load Game", MSP_Load, ""),
	ADDSET_ACTION("Save Game", MSP_Save, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
};

setting ingame_menu_entries[] = {
	ADDSET_SEPARATOR("In-game Menu"),
	ADDSET_ACTION("Ready", MIng_Ready, ""),
	ADDSET_ACTION("Break", MIng_Break, ""),
	ADDSET_ACTION("Join", MIng_Join, ""),
	ADDSET_ACTION("Observe", MIng_Observe, ""),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Server Browser", MIng_ServerBrowser, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
};

setting democtrl_menu_entries[] = {
	ADDSET_SEPARATOR("Demo Control Menu"),
	ADDSET_ACTION("Toggle autotrack", MQTV_Autotrack, ""),
	ADDSET_ACTION("Demo controls", MDemoCtrl_DemoControls, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Demo Browser", MDemoCtrl_DemoBrowser, ""),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Demo", MIng_Back, ""),
};

setting qtv_menu_entries[] = {
	ADDSET_SEPARATOR("QuakeTV Menu"),
	ADDSET_ACTION("Toggle autotrack", MQTV_Autotrack, ""),
	ADDSET_ACTION("Last scores", MQTV_Lastscores, ""),
	ADDSET_ACTION("List observers", MQTV_Observers, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Reconnect", MQTV_Reconnect, ""),
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
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
	ADDSET_ACTION("Disconnect", MIng_Disconnect, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Options", MIng_Options, ""),
	ADDSET_ACTION("Main Menu", MIng_MainMenu, ""),
	ADDSET_BLANK(),
	ADDSET_ACTION("Return To Game", MIng_Back, ""),
};

#define DEMOPLAYBACK() (cls.demoplayback && cls.mvdplayback != QTV_PLAYBACK)
#define BOTMATCH() (!strcmp(cls.gamedirfile, "fbca"))
#ifndef CLIENTONLY
#define SINGLEPLAYER() (com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1)
#endif
#define QTVPLAYBACK() (cls.mvdplayback == QTV_PLAYBACK)

static settings_page *M_Ingame_Current(void) {
	if (DEMOPLAYBACK()) {
		return &democtrl_menu;
	}
	else if (BOTMATCH()) {
		return &botmatch_menu;
	}
#ifndef CLIENTONLY
	else if (SINGLEPLAYER()) {
		return &single_menu;
	}
#endif
	else if (QTVPLAYBACK()) {
		return &qtv_menu;
	}
	else {
		return &ingame_menu;
	}
}

void M_Ingame_Draw(void) {
	char version[VERSION_MAX_LEN] = { 0 };
	qbool outdated;

	M_Unscale_Menu();
	Settings_Draw(0, TOPMARGIN, vid.width, vid.height - TOPMARGIN, M_Ingame_Current());

	outdated = VersionCheck_GetLatest(version);
	if (outdated)
	{
		char message[4096] = { 0 };
		snprintf(message, sizeof(message), "Outdated client %s, latest version is %s", VERSION_NUMBER, version);
		UI_Print_Center(0, 32, vid.width, message, 0);
	}
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
	Settings_Page_Init(single_menu, single_menu_entries);
	Settings_Page_SetMinit(single_menu);
	Settings_Page_Init(ingame_menu, ingame_menu_entries);
	Settings_Page_SetMinit(ingame_menu);
	Settings_Page_Init(democtrl_menu, democtrl_menu_entries);
	Settings_Page_SetMinit(democtrl_menu);
	Settings_Page_Init(botmatch_menu, botmatch_menu_entries);
	Settings_Page_SetMinit(botmatch_menu);
	Settings_Page_Init(qtv_menu, qtv_menu_entries);
	Settings_Page_SetMinit(qtv_menu);
}

void Menu_Ingame_Shutdown(void)
{
	Settings_Shutdown(&single_menu);
	Settings_Shutdown(&ingame_menu);
	Settings_Shutdown(&democtrl_menu);
	Settings_Shutdown(&botmatch_menu);
	Settings_Shutdown(&qtv_menu);
}
