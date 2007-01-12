/*
	Settings page module
	$Id: settings_page.h,v 1.1 2007-01-12 12:45:58 johnnycz Exp $
*/

// draw request handler
void Settings_Draw(int x, int y, int w, int h, settings_tab* tab);

// key press handler
qbool Settings_Key(settings_tab* tab, int key);
