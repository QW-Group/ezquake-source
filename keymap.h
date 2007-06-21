/*
 vi:set ts=2 sts=2 sw=2 noet ai:

Copyright (C) 2002-2003    Matthias Mohr (aka Massa)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
// keymap.h -- definitions for the keymap features

#ifndef _KEYMAP_H_
#define _KEYMAP_H_

#include  "quakedef.h"
#include	"keys.h"

#ifdef WITH_KEYMAP
// var to enable/disable displaying of key informations (e.g. scancode) at the console:
extern cvar_t cl_showkeycodes;

void	IN_StartupKeymap (void);    // register the variables
void	IN_Keycode_f (void);        // command "keycode"
void	IN_Keymap_Reset_f (void);   // command "keymap_reset"
void	IN_Keymap_Init_f (void);    // command "keymap_init"
void	IN_Keymaplist_f (void);     // command "keymap_list"
void	IN_Keymap_Load_f (void);    // command "keymap_load"
void	IN_Keymap_Save_f (void);    // command "keymap_save"

void IN_TranslateKeyEvent (int lParam, int wParam, qbool down);

#endif // WITH_KEYMAP

#endif // _KEYMAP_H_
