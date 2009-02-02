/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: help.h,v 1.6 2007-09-30 22:59:23 disconn3ct Exp $
*/

#ifndef __HELP_H__
#define __HELP_H__

// variable description
void Help_VarDescription (const char *varname, char* buf, size_t bufsize);

// initialize help system
void Help_Init (void);

// help menu
void Menu_Help_Init (void);
void Menu_Help_Draw (int x, int y, int w, int h);
void Menu_Help_Key (int key, wchar unichar);
qbool Menu_Help_Mouse_Event (const mouse_state_t *ms);

#endif // __HELP_H__
