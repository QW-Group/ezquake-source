/*
	Demo Menu module
	$Id: menu_demo.h,v 1.7 2007-03-17 00:32:52 johnnycz Exp $
*/

#include "keys.h"

// <interface for menu.c>
// initializes the "Demo" menu on client startup
void Menu_Demo_Init(void);

// process key press that belongs to demo menu
void Menu_Demo_Key(int key);

// process request to draw the demo menu
void Menu_Demo_Draw (void);

// process mouse move event
qbool Menu_Demo_Mouse_Move(const mouse_state_t *);

// sets new starting dir
void Menu_Demo_NewHome(const char *);
// </interface>


// <interface for cl_demo.c>
void CL_Demo_Playlist_f (void);
// </interface>
