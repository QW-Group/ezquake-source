/*
	Settings page module
	$Id: settings_page.h,v 1.6 2007-02-17 23:24:54 johnnycz Exp $
*/

/** Guide to create settings page:
	1) make new variables:
		a) 'settings_page' variable
		b) array of 'setting' elements, use macros from settings.h to do this
	2) in some Init function call Settings_Page_Init(a, b); - a and b are variables from steps a and b
	3) place calls to _Draw and _Kry functions to appropriate places
 */

#define Settings_Page_Init(set_page, settings) Settings_Init(&set_page, settings, sizeof(settings) / sizeof(setting))

// initializes the page structure
void Settings_Init(settings_page *page, setting *settings, size_t size); 

// initilalize settings pages structures
void Settings_MainInit(void);

// draw request handler
void Settings_Draw(int x, int y, int w, int h, settings_page* page);

// optonal call at the moment of first display
void Settings_OnShow(settings_page *tab);

// key press handler
qbool Settings_Key(settings_page* page, int key);
