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
/*
	Demo Menu module
	$Id: menu_demo.h,v 1.8 2007-03-19 13:23:20 johnnycz Exp $
*/

#include "keys.h"

// <interface for menu.c>
// initializes the "Demo" menu on client startup
void Menu_Demo_Init(void);

// process key press that belongs to demo menu
void Menu_Demo_Key(int key, wchar unichar);

// process request to draw the demo menu
void Menu_Demo_Draw (void);

// process mouse move event
qbool Menu_Demo_Mouse_Event(const mouse_state_t *);

// sets new starting dir
void Menu_Demo_NewHome(const char *);
// </interface>


// <interface for cl_demo.c>
void CL_Demo_Playlist_f(void);
void CL_Demo_NextInPlaylist(void);
void CL_Demo_Disconnected(void);
// </interface>
