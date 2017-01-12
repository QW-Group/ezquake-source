/*
Copyright (C) 2011 johnnycz

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
/** Guide to create settings page:
	1) make new variables:
		a) 'settings_page' variable
		b) array of 'setting' elements, use macros from settings.h to do this
	2) in some Init function call Settings_Page_Init(a, b); - a and b are variables from steps a and b
	3) place calls to _Draw and _Key functions to appropriate places
 */

#ifndef __SETTINGS_PAGE_H__
#define __SETTINGS_PAGE_H__

#include "keys.h"

#define Settings_Page_Init(set_page, settings) Settings_Init(&set_page, settings, sizeof(settings) / sizeof(setting))
#define Settings_Page_SetMinit(set_page) { set_page.mini = true; }

// initializes the page structure
void Settings_Init(settings_page *page, setting *settings, size_t size); 

// initilalize settings pages structures
void Settings_MainInit(void);

// draw request handler
void Settings_Draw(int x, int y, int w, int h, settings_page* page);

// optonal call at the moment of first display
void Settings_OnShow(settings_page *tab);

// key press handler
qbool Settings_Key(settings_page* page, int key, wchar unichar);

// mouse move handler
qbool Settings_Mouse_Event(settings_page * page, const mouse_state_t *ms);

#endif // __SETTINGS_PAGE_H__
