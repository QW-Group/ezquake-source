/*

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
// keymap_x11.c -- stubs to support the same commands/cvars as windows version (for international keyboard layouts)
//   (mostly only subs because unneeded under X11)

#ifdef WITH_KEYMAP
#include	"keymap.h"

#include  <X11/Xutil.h>


// name of the keymapping:
cvar_t	keymap_name = {"keymap_name", "Default"};

/*
=======
IN_StartupKeymap

register the commands and cvars for the keymapping feature:
=======
*/
void	IN_StartupKeymap (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);

	// command for setting a mapping for one scancode:
	Cmd_AddCommand ("keycode",       IN_Keycode_f);

	// switch off keymapping:
	Cmd_AddCommand ("keymap_reset",  IN_Keymap_Reset_f);

	// initialize a new keymapping with internal defaults:
	Cmd_AddCommand ("keymap_init",   IN_Keymap_Init_f);

	// write a list of all bindings at console:
	Cmd_AddCommand ("keymaplist",    IN_Keymaplist_f);
	Cmd_AddCommand ("keymap_list",   IN_Keymaplist_f);

	// Initialize keymappings and the load from file:
	Cmd_AddCommand ("keymap_load",   IN_Keymap_Load_f);

	// Save keymappings to a file:
	Cmd_AddCommand ("keymap_save",   IN_Keymap_Save_f);

	// Show keycodes when pressing or releasing a key:
	Cvar_Register (&cl_showkeycodes);

	// Reset the saving group, we don't want to have the keymap_name in the saved config files
	Cvar_ResetCurrentGroup();

	// name of the current keymapping (just for informational purposes):
	Cvar_Register (&keymap_name);
} /* END_FUNC IN_StartupKeymap */


/*
===========
IN_Keymap_Load_f

  command "keymap_load"
Syntax:
  keymap_load [layoutname] <filename>

Load custom keymappings from file
The file-format is like this:
  keymap_name "Default"
  keycode 27 ] }
  keycode 28 ENTER
  keycode ext 28 KP_ENTER
  keycode 40 ' #34
===========
*/
void IN_Keymap_Load_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf ("%s [layoutname] <filename>: load keymappings from file\n", Cmd_Argv(0));
	}

	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
} // END_FUNC IN_Keymap_Load_f


/*
===========
IN_Keymap_Save_f

  command "keymap_save"
Syntax:
  keymap_save [layoutname] <filename>

Save the current keymappings to the given file
if no directory is given, it will be written to
the "qw" subdirectory.
===========
*/
void IN_Keymap_Save_f (void)
{
	if ( Cmd_Argc() < 2) {
		Com_Printf ("Usage: %s [<layoutname>] <filename>: save active keymappings to file\n", Cmd_Argv(0));
	}

	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
} // END_FUNC IN_Keymap_Save_f


/*
===========
IN_Keycode_f

  command "keycode"
Syntax:
  keycode [ext] scancode mapping [shiftmapping] [altgrmapping]

Set a custom mapping for one key.
mapping, shiftmapping, altgrmapping can be
 a character            -> maps directly to that character
 a symbol, like ENTER   -> maps to the corresponding ASCII-character
 a number, like #34      -> maps directly to the corresponding ASCII-character
===========
*/
void IN_Keycode_f (void) {
	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
}


/*
===========
IN_Keymap_Reset_f

  command "keymap_reset"
Syntax:
  keymap_reset
Resets the current keymapping
===========
*/
void IN_Keymap_Reset_f (void) {
	if (!strcasecmp(Cmd_Argv(0), "keymap_reset") && Cmd_Argc() > 1) {
		Com_Printf ("Usage: %s\n"
		            "Resets the active keymappings and sets it to the internal defaults (US keyboard).\n",
		            Cmd_Argv(0) );
	}

	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
} // END_FUNC IN_Keymap_Reset_f


/*
===========
IN_Keymap_Init_f

  command "keymap_init"
Syntax:
  keymap_init
Initialize the keymap-arrays for a new keymapping,
based on the internal defaults (US keyboard)
===========
*/
void IN_Keymap_Init_f (void) {
	int  i;

	if (!strcasecmp(Cmd_Argv(0), "keymap_init") && Cmd_Argc() > 1) {
		Com_Printf ("Usage: %s\n"
		            "Initializes the active keymappings:\n"
		            "resets an active keymapping and uses the\n"
		            "internal defaults (US keyboard) as new keymappings.\n",
		            Cmd_Argv(0) );
		return;
	}

	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
} // END_FUNC IN_Keymap_Init_f


/*
===========
IN_Keycode_Print_f

Writes the given keycode information 
(scancode, extension, mapping, ...)
to the console
if cl_showkeycodes == 1 --> only print "key-presses"
If cl_showkeycodes == 2 --> only print "key-releases"
if cl_showkeycodes == 3 --> print "key-releases" and "key-presses"
===========
*/
void IN_Keycode_Print_f ( XKeyEvent *ev, qbool ext, qbool down, int key ) {
	int     flag = (int)cl_showkeycodes.value;
	char    buf[ 64 ] = "";
	KeySym  keysym;

	if (( (flag & 2) > 0 && down != true ) || 
	    ( (flag & 1) > 0 && down == true )) {
		XLookupString(ev, buf, sizeof buf, &keysym, 0);
		if ( key != K_SHIFT && key != K_LSHIFT && key != K_RSHIFT &&
				 key != K_CTRL && key != K_LCTRL && key != K_RCTRL &&
				 key != K_ALT && key != K_LALT && key != K_RALT && key != K_ALTGR )
			Com_Printf ("keycode %s %3.3u %s: %s%s%s%s%s\n",
			            ext == true ? "ext" : "   ",
			            (unsigned int)keysym,
			            down == true ? (char *)"pressed" : (char *)"released",
			            keydown[K_SHIFT] ? (char *)"SHIFT+" : (char *)"\0",
			            keydown[K_CTRL] ? (char *)"CTRL+" : (char *)"\0",
			            keydown[K_ALT] ? (char *)"ALT+" : (char *)"\0",
			            keydown[K_ALTGR] ? (char *)"ALTGR+" : (char *)"\0",
			            Key_KeynumToString(key, NULL) );
		else
			Com_Printf ("keycode %s %3.3u %s: %s\n",
			            ext == true ? "ext" : "   ",
			            (unsigned int)keysym,
			            down == true ? (char *)"pressed" : (char *)"released",
			            Key_KeynumToString(key, NULL) );
	}
} // END_FUNC IN_Keycode_Print_f


/*
===========
IN_Keymaplist_f

  command "keymaplist"
  command "keymap_list"
Syntax:
  keymaplist
  keymap_list

Prints a list of all active keycode-mappings
to the console
===========
*/
void IN_Keymaplist_f (void)
{
	int    i;

	if (Cmd_Argc() > 1) {
		Com_Printf ("Usage: %s\n"
		            "Prints a list of all keymappings to the console.\n",
		            Cmd_Argv(0) );
	}

	Com_Printf ("%s: not needed in X11!\n", Cmd_Argv(0));
} // END_FUNC IN_Keymaplist_f
#endif // WITH_KEYMAP
/* vi:set ts=2 sts=2 sw=2 noet ai: */
