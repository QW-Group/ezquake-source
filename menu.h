/*
Copyright (C) 1996-1997 Id Software, Inc.

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

	$Id: menu.h,v 1.22 2007-10-17 17:08:26 dkure Exp $

*/

#ifndef __MENU_H__
#define __MENU_H__

#include "keys.h"

#define OPTPADDING 4

//
// menus
//
void M_Init (void);
void M_Keydown (int key, wchar unichar);
void M_Draw (void);
void M_ToggleMenu_f (void);
void M_LeaveMenus (void);
void M_LeaveMenu (int parent);
void M_EnterMenu (int state);
void M_Menu_Main_f (void);
void M_EnterProxyMenu (void);
void M_DrawTextBox (int x, int y, int width, int lines);
void M_Menu_Quit_f (void);
void M_Demos_Playlist_stop_f (void);
void M_Menu_ServerList_f (void);
void M_DrawTransPic (int x, int y, mpic_t *pic);
void M_DrawPic (int x, int y, mpic_t *pic);
void M_PrintWhite (int cx, int cy, char *str);
void M_Print (int cx, int cy, char *str);
void M_DrawCharacter (int cx, int line, int num);
void M_DrawSlider (int x, int y, float range);
void M_FindKeysForCommand (const char *command, int *twokeys);
void M_BuildTranslationTable(int top, int bottom);
void M_Unscale_Menu(void);
qbool Menu_Mouse_Event(const mouse_state_t* ms);
qbool Menu_ExecuteKey(int key);

extern int m_yofs;

#define FLASHINGARROW() (12+((int)(curtime*4)&1))
#define FLASHINGCURSOR() (10+((int)(curtime*4)&1))

typedef enum {
    m_none, m_main, m_proxy, m_singleplayer, m_load, m_save,
	m_multiplayer, m_demos, m_multiplayer_submenu,
    m_options,
	m_help,
	m_quit, m_ingame, 
} m_state_t;

extern m_state_t m_state;

#endif // __MENU_H_
