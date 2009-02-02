/*
	Options Menu module
	$Id: menu_options.h,v 1.4 2007-03-19 13:23:20 johnnycz Exp $
*/

// <interface for menu.c>
// initializes the "Demo" menu on client startup
void Menu_Options_Init(void);

// process key press that belongs to demo menu
void Menu_Options_Key(int key, wchar unichar);

// process request to draw the demo menu
void Menu_Options_Draw (void);

// process mouse move
qbool Menu_Options_Mouse_Event(const mouse_state_t *);
// </interface>
