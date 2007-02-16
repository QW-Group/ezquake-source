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

	$Id: menu.h,v 1.14 2007-02-16 20:10:36 johnnycz Exp $

*/

//
// menus
//
void M_Init (void);
void M_Keydown (int key, int unichar);
void M_Draw (void);
void M_ToggleMenu_f (void);
void M_LeaveMenus (void);
void M_Menu_Main_f (void);
void M_EnterProxyMenu (void);
void M_DrawTextBox (int x, int y, int width, int lines);
void M_Menu_Quit_f (void);
void M_Demos_Playlist_stop_f (void);
void M_DrawTransPic (int x, int y, mpic_t *pic);
void M_DrawPic (int x, int y, mpic_t *pic);
void M_PrintWhite (int cx, int cy, char *str);
void M_Print (int cx, int cy, char *str);
void M_DrawCharacter (int cx, int line, int num);
void M_DrawCheckbox (int x, int y, int on);
void M_DrawSlider (int x, int y, float range);
void M_DrawCheckbox (int x, int y, int on);
void M_FindKeysForCommand (const char *command, int *twokeys);
void M_BuildTranslationTable(int top, int bottom);

extern int m_yofs;

#define FLASHINGARROW() (12+((int)(curtime*4)&1))
#define FLASHINGCURSOR() (10+((int)(curtime*4)&1))

enum {
    m_none, m_main, m_proxy, m_singleplayer, m_load, m_save,
	m_multiplayer, m_gameoptions, m_slist, m_demos,
    m_options,
	m_help,
	m_quit,
#if defined(_WIN32) || ((defined(__linux__) || defined(__FreeBSD__)) && defined(WITH_XMMS))
    m_mp3_control, m_mp3_playlist
#endif
} m_state;
