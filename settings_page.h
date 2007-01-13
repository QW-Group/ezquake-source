/*
	Settings page module
	$Id: settings_page.h,v 1.4 2007-01-13 03:16:22 johnnycz Exp $
*/

/** Guide to create settings page:
	1) make new variables:
		a) array of 'setting' elements, use macros from settings.h to do this
		b) 'settings_page' variable
	2) in some Init function call Settings_Page_Init(b, a); - a and b are variables from steps a and b
	3) place calls to _Draw and _Kry functions to appropriate places
 */

#define Settings_Page_Init(settings, set_page) Settings_Init(&set_page, settings, sizeof(settings) / sizeof(setting))

// initializes the main structure
void Settings_Init(settings_page *tab, setting *arr, size_t size); 

// draw request handler
void Settings_Draw(int x, int y, int w, int h, settings_page* page);

// key press handler
qbool Settings_Key(settings_page* page, int key);
