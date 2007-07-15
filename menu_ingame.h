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
