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

// initialize the multiplayer menu
void Menu_MultiPlayer_Init(void);

void Menu_MultiPlayer_Draw (void);

void Menu_MultiPlayer_Key(int key, wchar unichar);

qbool Menu_MultiPlayer_Mouse_Event(const mouse_state_t *ms);

void Menu_MultiPlayer_SwitchToServersTab(void);

typedef enum
{
	SBPG_SERVERS,	// Servers page
	SBPG_SOURCES,	// Sources page
	SBPG_PLAYERS,	// Players page
	SBPG_OPTIONS,	// Options page
	SBPG_CREATEGAME   // Host Game page
} sb_tab_t;
extern CTab_t sb_tab;

