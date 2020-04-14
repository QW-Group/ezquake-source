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
//=============================================================================
// MP3 PLAYER MENU
//=============================================================================


void M_Menu_MP3_Playlist_f(void);

void M_Menu_MP3_Control_Draw (void);

void M_Menu_MP3_Playlist_Draw(void);

void M_Menu_MP3_Control_Key(int key);

void M_Menu_MP3_Playlist_Key (int k);

qbool M_Menu_MP3_Control_Mouse_Event(const mouse_state_t *ms);

qbool M_Menu_MP3_Playlist_Mouse_Event(const mouse_state_t *ms);
