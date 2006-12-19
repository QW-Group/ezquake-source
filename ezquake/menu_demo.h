/*
	Demo Menu module
	$Id: menu_demo.h,v 1.2 2006-12-19 20:30:42 johnnycz Exp $
*/

// <interface for menu.c>
// initializes the "Demo" menu on client startup
void Menu_Demo_Init(void);

// process key press that belongs to demo menu
void Menu_Demo_Key(int key);

// process request to draw the demo menu
void Menu_Demo_Draw (void);
// </interface>


// <interface for cl_demo.c>
void CL_Demo_playlist_f (void);
// </interface>