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

	$Id: menu.h,v 1.10 2006-12-30 11:30:14 disconn3ct Exp $

*/

//
// menus
//
void M_Init (void);
void M_Keydown (int key);
void M_Draw (void);
void M_ToggleMenu_f (void);
void M_LeaveMenus (void);
void M_DrawTextBox (int x, int y, int width, int lines);
void M_Menu_Quit_f (void);
void M_Demos_Playlist_stop_f (void);
void M_PrintWhite (int cx, int cy, char *str);
void M_Print (int cx, int cy, char *str);
void M_DrawCharacter (int cx, int line, int num);
void M_DrawCheckbox (int x, int y, int on);

#define FLASHINGARROW() (12+((int)(curtime*4)&1))
#define FLASHINGCURSOR() (10+((int)(curtime*4)&1))

enum {
    m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer,
    m_setup, m_options, m_video, m_keys, m_help, m_quit,
    m_gameoptions, m_slist,/* m_sedit,*/ m_fps, m_demos/*, m_demos_del, m_demos2*/
#if defined(_WIN32) || ((defined(__linux__) || defined(__FreeBSD__)) && defined(WITH_XMMS))
    , m_mp3_control, m_mp3_playlist
#endif
} m_state;
