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

	In-game menu

	made by johnnycz, June 2007
	last edit:
	$Id: menu_ingame.h,v 1.2 2007-07-15 09:50:44 disconn3ct Exp $

*/

#ifndef __MENU_INGAME_H__
#define __MENU_INGAME_H__

extern void M_Ingame_Draw(void);
extern void M_Democtrl_Draw(void);

extern void M_Ingame_Key(int);
extern void M_Democtrl_Key(int);

extern qbool Menu_Ingame_Mouse_Event(const mouse_state_t *ms);
extern qbool Menu_Democtrl_Mouse_Event(const mouse_state_t *ms);

extern void Menu_Ingame_Init(void);

#endif // __MENU_INGAME_H__
