/*
	Demo Menu module
	$Id: menu_demo.h,v 1.5 2007-01-15 21:14:33 cokeman1982 Exp $
*/

// <interface for menu.c>
// initializes the "Demo" menu on client startup
void Menu_Demo_Init(void);

// process key press that belongs to demo menu
void Menu_Demo_Key(int key);

// process request to draw the demo menu
void Menu_Demo_Draw (void);

// sets new starting dir
void Menu_Demo_NewHome(const char *);
// </interface>


// <interface for cl_demo.c>
void CL_Demo_Playlist_f (void);
// </interface>
