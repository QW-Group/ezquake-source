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

	$Id: keymap.c,v 1.17 2007-10-11 06:46:11 borisu Exp $

*/
// keymap.c -- support for international keyboard layouts

#ifdef WITH_KEYMAP
#include	"keymap.h"

extern cvar_t cl_keypad;

// name of the keymapping:
cvar_t	keymap_name = {"keymap_name", "Default"};
cvar_t	in_builtinkeymap = {"in_builtinkeymap", "0"};

// flag, which shows if a keymapping is active or not:
static qbool keymap_active = false; 

// internal functions:
static void IN_Keycode_Print_f (int scancode, qbool ext, qbool down, int key);
static void IN_Keycode_Set_f (qbool showerr, char *filename, unsigned int linecount);
static void IN_Keymap_Print_f (int scancode, qbool ext);
static void IN_Keymap_WriteHeader_f (FILE *f);
static void IN_Keymap_WriteKeycode_f (FILE *f, int scancode);

// default builtin keymaps;
// 0-127 are normal scancodes; 128-255 are with extended flag set:
static byte keymaps_default[ 2 ][ 256 ] =
{
  { // [0]=normal:
//  0        1        2        3        4        5        6        7
//  8        9        A        B        C        D        E        F
    0,    K_ESCAPE,  '1',     '2',     '3',     '4',     '5',     '6',
   '7',     '8',     '9',     '0',     '-',     '=',   K_BACKSPACE,K_TAB,    // 0
   'q',     'w',     'e',     'r',     't',     'y',     'u',     'i',
   'o',     'p',     '[',     ']',   K_ENTER, K_LCTRL,   'a',     's',       // 1
   'd',     'f',     'g',     'h',     'j',     'k',     'l',     ';',
   '\'',    '`',   K_LSHIFT,  '\\',    'z',     'x',     'c',     'v',       // 2
   'b',     'n',     'm',     ',',     '.',     '/',   K_RSHIFT,KP_STAR,
 K_LALT,    ' ',   K_CAPSLOCK,K_F1,  K_F2,    K_F3,    K_F4,    K_F5,        // 3
 K_F6,    K_F7,    K_F8,    K_F9,    K_F10,   K_PAUSE, K_SCRLCK,KP_HOME,
 KP_UPARROW,KP_PGUP,KP_MINUS,KP_LEFTARROW,KP_5,KP_RIGHTARROW,KP_PLUS,KP_END, // 4
 KP_DOWNARROW,KP_PGDN,KP_INS,KP_DEL,    0,       0,       0,    K_F11,
 K_F12,      0,       0,       0,       0,       0,       0,       0,        // 5
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 6
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 7
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 0 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,    KP_ENTER,K_RCTRL,    0,       0,        // 1 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 2 ext
    0,       0,       0,       0,       0,    KP_SLASH,   0, PRINT_SCREEN,
 K_RALT,     0,       0,       0,       0,       0,       0,       0,        // 3 ext
    0,       0,       0,       0,       0,    KP_NUMLOCK, 0,    K_HOME,
 K_UPARROW,K_PGUP,    0,    K_LEFTARROW,0,    K_RIGHTARROW,0,   K_END,       // 4 ext
 K_DOWNARROW,K_PGDN,K_INS,  K_DEL,      0,       0,       0,       0,
    0,       0,    K_WIN,   K_LWIN,  K_RWIN,  K_MENU,     0,       0,        // 5 ext
    0,       0,       0,    WAKE_UP,    0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 6 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0         // 7 ext
  }, // END [0]=normal

  { //[1]=shifted
//  0        1        2        3        4        5        6        7
//  8        9        A        B        C        D        E        F
    0,    K_ESCAPE,  '!',     '@',     '#',     '$',     '%',     '^',
   '&',     '*',     '(',     ')',     '_',     '+',   K_BACKSPACE,K_TAB,    // 0
   'Q',     'W',     'E',     'R',     'T',     'Y',     'U',     'I',
   'O',     'P',     '{',     '}',   K_ENTER, K_LCTRL,   'A',     'S',       // 1
   'D',     'F',     'G',     'H',     'J',     'K',     'L',     ':',
   '"',     '~',   K_LSHIFT,  '|',     'Z',     'X',     'C',     'V',       // 2
   'B',     'N',     'M',     '<',     '>',     '?',   K_RSHIFT,KP_STAR,
 K_LALT,    ' ',   K_CAPSLOCK,K_F1,  K_F2,    K_F3,    K_F4,    K_F5,        // 3
 K_F6,    K_F7,    K_F8,    K_F9,    K_F10,   K_PAUSE, K_SCRLCK,KP_HOME,
 KP_UPARROW,KP_PGUP,KP_MINUS,KP_LEFTARROW,KP_5,KP_RIGHTARROW,KP_PLUS,KP_END, // 4
 KP_DOWNARROW,KP_PGDN,KP_INS,KP_DEL,    0,       0,       0,    K_F11,
 K_F12,      0,       0,       0,       0,       0,       0,       0,        // 5
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 6
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 7
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 0 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,    KP_ENTER,K_RCTRL,    0,       0,        // 1 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 2 ext
    0,       0,       0,       0,       0,    KP_SLASH,   0, PRINT_SCREEN,
 K_RALT,     0,       0,       0,       0,       0,       0,       0,        // 3 ext
    0,       0,       0,       0,       0,    KP_NUMLOCK, 0,    K_HOME,
 K_UPARROW,K_PGUP,    0,    K_LEFTARROW,0,    K_RIGHTARROW,0,   K_END,       // 4 ext
 K_DOWNARROW,K_PGDN,K_INS,  K_DEL,      0,       0,       0,       0,
    0,       0,    K_WIN,   K_LWIN,  K_RWIN,  K_MENU,     0,       0,        // 5 ext
    0,       0,       0,    WAKE_UP,    0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0,        // 6 ext
    0,       0,       0,       0,       0,       0,       0,       0,
    0,       0,       0,       0,       0,       0,       0,       0         // 7 ext
  } // END [1]=shifted
};

// user defined keymaps;
// 0-127 are normal scancodes; 128-255 are with extended flag set:
static byte keymaps[ 3 ][ 256 ]; // [0]= normal, [1]=shifted, [2]=altgr level


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
	Cmd_AddCommand ("keymap_init",  IN_Keymap_Init_f);

	// write a list of all bindings at console:
	Cmd_AddLegacyCommand("keymaplist", "keymap_list");
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
	Cvar_Register (&in_builtinkeymap);
} /* END_FUNC IN_StartupKeymap */


/*
=======
IN_TranslateKeyEvent

Map from windows to quake keynums and generate Key_Event
=======
*/
void IN_TranslateKeyEvent (int lParam, int wParam, qbool down) {
	int		extended;
	int		scancode;
	int		key      = 0;
	int		unichar  = 0;

	extended = (lParam >> 24) & 0x01;
	scancode = (lParam >> 16) & 0xFF;
	if (scancode <= 127) {
		if (keymap_active) {
			key     = keymaps[ 0 ][ scancode + (extended ? 128 : 0) ];
			unichar = key;
			if ( keydown[K_ALTGR] )
				unichar = keymaps[ 2 ][ scancode + (extended ? 128 : 0) ];
			else
			if ( keydown[K_SHIFT] )
				unichar = keymaps[ 1 ][ scancode + (extended ? 128 : 0) ];
			if (unichar < 32 || unichar > 127)
				unichar = 0;
		}
		else
		{
			// use the internal default mapping:
			key     = keymaps_default[0][scancode + (extended ? 128 : 0)];
			if (key == 0)
				key = UNKNOWN + scancode + (extended ? 128 : 0);
			unichar = key;
			if (keydown[K_SHIFT])
				unichar = keymaps_default[1][scancode + (extended ? 128 : 0)];
			if (unichar < 32 || unichar > 127)
				unichar = 0;

			if (cl_keypad.value <= 0) {
				// compatibility mode without knowledge about keypad-keys:
				switch (key)
				{
					case KP_NUMLOCK:     key = K_PAUSE;          break;
					case KP_SLASH:       key = '/';              break;
					case KP_STAR:        key = '*';              break;
					case KP_MINUS:       key = '-';              break;
					case KP_HOME:        key = K_HOME;           break;
					case KP_UPARROW:     key = K_UPARROW;        break;
					case KP_PGUP:        key = K_PGUP;           break;
					case KP_LEFTARROW:   key = K_LEFTARROW;      break;
					case KP_5:           key = '5';              break;
					case KP_RIGHTARROW:  key = K_RIGHTARROW;     break;
					case KP_PLUS:        key = '+';              break;
					case KP_END:         key = K_END;            break;
					case KP_DOWNARROW:   key = K_DOWNARROW;      break;
					case KP_PGDN:        key = K_PGDN;           break;
					case KP_INS:         key = K_INS;            break;
					case KP_DEL:         key = K_DEL;            break;
					case KP_ENTER:       key = K_ENTER;          break;
					default:                                     break;
				}
			}
#ifdef _WIN32 // XXX: oldman: I presume this in WIN32 code
			if (!in_builtinkeymap.value) {
				BYTE	state[256];
				WCHAR	uni;
				GetKeyboardState (state);
				if (state[0x11] > 127)  // is CTRL key pressed ?
					if (state[0x12] < 127) // is ALT *not* pressed ?
						state[0x11] = 1;  // skip check for CTRL key since it messes up ToUnicode
				ToUnicode (wParam, lParam >> 16, state, &uni, 1, 0);
				unichar = uni;
			}
#endif
		}
	} // END if (scancode <= 127)

	// if set, print the current Key information
	if (cl_showkeycodes.value > 0) {
		IN_Keycode_Print_f (scancode, (extended > 0) ? true : false, down, unichar ? unichar : key);
	}

	Key_EventEx (key, unichar, down);

	// FIXME
	// the following is a workaround for the situation where
	//   1.) a key will be pressed (e.g. x)
	//   2.) then a shift or altgr-key will be pressed additionally (e.g. SHIFT)
	//   3.) then the original key will be released
	// in that situation, the software is unable to identify if the originally
	//   key (x) shall be released or the key together with the modifier key
	//   (SHIFT+x=X).
	// So we also generate an additional release event for the unshifted key
	//   (if it's different from the basekey)
	// Not very smart, but it seems to work...

	// FIXED
	// pressed operations occur with basekey only
/*	if ( (down != true) && (basekey != key) ) {
#ifdef _DEBUG
		char		str1[256];
		char		str2[256];
    Com_Printf ("KeyFix: Down-Event for key:%u=0x%X=%s basekey=%u=0x%X=%s\n", key, key, Key_KeynumToString (key, str1), basekey, basekey, Key_KeynumToString(basekey, str2));
#endif
		Key_Event (basekey, down);
	}*/
	return;
} /* END_FUNC IN_TranslateKeyEvent */


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
	char            filename[MAX_QPATH];
	char           *data;
	int             i;
	unsigned int    linecount;
	char            name[ 256 ];

	if (Cmd_Argc() < 2) {
		Com_Printf ("%s [layoutname] <filename>: load keymappings from file\n", Cmd_Argv(0));
		if ( keymap_active != true )
			Com_Printf ("Currently no keymappings are active!\n");
		return;
	}

	i = 1;
	name[0] = (char)'\0';
	if (Cmd_Argc() > 2) {
		// layout-name is also given -> store it for later usage
		strlcpy (name, Cmd_Argv(i), sizeof(name));
		i++;
	}

	// test, if the given file could be found and open it:
	strlcpy (filename, Cmd_Argv(i), sizeof(filename) - 5);
	COM_DefaultExtension (filename, ".kmap");

	// first check in subdirectory "keymaps":
	data = FS_LoadTempFile (va("keymaps/%s",filename), NULL);

	// if not found check in main directory:
	if ( data == NULL )
		data = FS_LoadTempFile (filename, NULL);
	if (data != NULL) {
		// Initialize the arrays with the default values:
		IN_Keymap_Init_f();

		// read the file line by line:
		linecount = 0;
		data = strtok (data, "\r\n");
		do {
			if (strlen(data) >= 256) { // sanity check
				if (cl_warncmd.integer || developer.integer)
					Com_Printf ("WARNING: Line %u in file \"%s\" is too long (>256 chars)!\n", linecount, filename);
			}
			else {
				Cmd_TokenizeString (data);

				if (Cmd_Argc() > 0) {
					if (!strcasecmp(Cmd_Argv(0), "keymap_name")) {
						if (name[0] == (char)'\0') {
							if (Cmd_Argc() > 1)
								strlcpy (name, Cmd_Argv(1), sizeof(name));
						}
					}
					else {
						if (!strcasecmp(Cmd_Argv(0), "keycode")) {
							IN_Keycode_Set_f (true, filename, linecount);
						}
						else {
							// unknown command; put in "normal" command buffer
							Cbuf_AddText (data);
						}
					}
				} // END if (Cmd_Argc() > 0) 
			}

			// next line
			linecount++;
		} while ( (data = strtok (NULL, "\r\n")) != NULL );

		// make sure, the layoutname is correct...
		if (name[0] != (char)'\0')
			Cvar_Set( &keymap_name, name );

		if (cl_warncmd.integer || developer.integer)
			Com_Printf ("keymapping \"%s\" from file \"%s\" loaded\n",
			            keymap_name.string ? keymap_name.string : "<custom>",
			            filename);
	}
	else {
		if (cl_warncmd.integer || developer.integer)
			Com_Printf ("Couldn't open keymap file \"%s\"\n", filename);
	}
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
	char    filename[ MAX_QPATH ];
	FILE   *f = NULL;
	int     i;

	if ( Cmd_Argc() < 2) {
		Com_Printf ("Usage: %s [<layoutname>] <filename>: save active keymappings to file\n", Cmd_Argv(0));
		if ( keymap_active != true )
			Com_Printf ("No keymappings active!\n" );
		return;
	}

	if ( keymap_active != true ) {
		Com_Printf ("No keymappings active!\n" );
		return;
	}

	i = 1;
	if (Cmd_Argc() > 2) {
		// layoutname is given:
		Cvar_Set( &keymap_name, Cmd_Argv(1) );
		i++;
	}

	strlcpy (filename, Cmd_Argv(i), sizeof(filename) - 5);
	COM_DefaultExtension (filename, ".kmap");
	if ( strchr(filename, (int)'/') == NULL && strchr(filename, (int)'\\') == NULL ) {
		// no directory given, so add "qw" to the directory...
		char   dummy[ MAX_QPATH ];

		strlcpy (dummy, filename, sizeof(dummy) - 3);
		snprintf(filename, sizeof (filename), "qw/%s", dummy);
	}

	FS_CreatePath (filename);

	if ( (f = fopen( filename, "w" )) != NULL ) {
		// write the header information:
		IN_Keymap_WriteHeader_f (f);

		// now write the keycodes:
		for (i=0; i<128; i++) {
			// write the normal keycode:
			IN_Keymap_WriteKeycode_f( f, i );
			// write the same keycode with extended flag set:
			IN_Keymap_WriteKeycode_f( f, (i + 128) );
		}

		fprintf (f, "// END OF KEYMAP FILE\n");
		fclose (f);

		if (cl_warncmd.integer || developer.integer)
			Com_Printf ("keymappings \"%s\" saved to \"%s\"\n", keymap_name.string, filename);
	}
	else {
		// file could not be opened
		Com_Printf ("Couldn't open \"%s\" for writing.\n", filename);
		return;
	}
} // END_FUNC IN_Keymap_Save_f


/*
===========
IN_Keymap_WriteHeader_f

Part of command "keymap_save"

Write the Header information to the keymapfile;
identified by the given and already open filehandle
===========
*/
static void IN_Keymap_WriteHeader_f (FILE *f) {
	fprintf (f, "// ZQuake/FuhQuake keymap file\n\n");
	fprintf (f, "keymap_name \"%s\"\n\n", keymap_name.string ? keymap_name.string : "Custom");
	fprintf (f,
	        "//--------------------------------------------------------------------------\n"
	        "//Syntax:\n"
	        "//  keycode [ext] <scancode> <key> [<shiftkey>] [<altgrkey>]\n"
	        "//    ext       the extended flag is set for this (raw) scancode.\n"
	        "//    scancode  is a number between 1 and 127.\n"
	        "//    key       is either\n"
	        "//              a character, which will be used when pressed the key or\n"
	        "//              a special name (like ESCAPE, TAB, ...)\n"
	        "//              a number > 31 and < 128, prefixed with a #;\n"
	        "//                this represents the decimal value in the character table.\n"
	        "//    shiftkey  this is the key which will be used when pressed together\n"
	        "//                with one of the SHIFT-keys (SHIFT, LSHIFT, RSHIFT)\n"
	        "//    altgrkey  this is the key which will be displayed when pressed together\n"
	        "//                with the ALTGR-key\n"
	        "//Note: the characters a-z used as unshifted key will automatically be converted\n"
	        "//      to uppercase shifted keys, if no other mapping is set for that key.\n"
	        "//--------------------------------------------------------------------------\n");
	fprintf (f,
	        "//The following special keynames for are known (some only exists on international\n"
	        "//  102-105 keys keyboard):\n"
	        "// ALT           = Alt-Key (use this if no separate mapping for left and\n"
	        "//                 right key is needed).\n"
	        "// LALT          = left Alt-Key (use this if separate mapping for left and\n"
	        "//                 right key is needed).\n"
	        "// RALT          = right Alt-Key (use this if separate mapping for left and\n"
	        "//                 right key and NO third level of keymapping is needed).\n"
	        "// ALTGR         = right Alt-Key; this IS the switch-key for the third level\n"
	        "//                 of keymapping (if the third level mapping is needed, one\n"
	        "//                   scancode MUST be set to this key).\n"
	        "// ALTCHAR       = this is another name for ALTGR.\n"
	        "// BACKSPACE\n"
	        "// CAPSLOCK\n"
	        "// CTRL          = Control-Key (use this if no separate mapping for left and\n"
	        "//                   right key is needed).\n"
	        "// LCTRL         = Left Control-Key (use this if separate mapping for left and\n"
	        "//                   right key is needed).\n"
	        "// RCTRL         = Right Control-Key (use this, if separate mapping for left\n"
	        "//                   and right key is needed).\n" );
	fprintf (f,
	        "// DEL            = Delete.\n"
	        "// DOWNARROW     = Cursor down.\n"
	        "// END           = End.\n"
	        "// ENTER.\n"
	        "// ESCAPE.\n"
	        "// F1 ... F12    = Function keys F1 - F12.\n"
	        "// HOME          = Home.\n"
	        "// INS           = Insert.\n"
	        "// LEFTARROW     = Cursor left.\n"
	        "// KP_0          = '0' at keypad (same as KP_INS).\n"
	        "// KP_1          = '1' at keypad (same as KP_END).\n"
	        "// KP_2          = '2' at keypad (same as KP_DOWNARROW).\n"
	        "// KP_3          = '3' at keypad (same as KP_PGDN).\n"
	        "// KP_4          = '4' at keypad (same as KP_LEFTARROW).\n"
	        "// KP_5          = '5' at keypad.\n"
	        "// KP_6          = '6' at keypad (same as KP_RIGHTARROW).\n"
	        "// KP_7          = '7' at keypad (same as KP_HOME).\n"
	        "// KP_8          = '8' at keypad (same as KP_UPARROW).\n"
	        "// KP_9          = '9' at keypad (same as KP_PGUP).\n"
	        "// KP_DEL        = Del at keypad.\n");
	fprintf (f,
	        "// DEL            = Delete.\n"
	        "// KP_DIVIDE     = '/' at keypad (same as KP_SLASH).\n"
	        "// KP_DOWNARROW  = Cursor down at keypad (same as KP_2).\n"
	        "// KP_END        = End at keypad (same as KP_1).\n"
	        "// KP_ENTER      = Enter at keypad.\n"
	        "// KP_HOME       = Keypad-Home (same as KP_7).\n"
	        "// KP_INS        = Insert at keypad (same as KP_0).\n"
	        "// KP_LEFTARROW  = Cursor left at keypad (same as KP_4).\n"
	        "// KP_MINUS      = '-' at keypad.\n"
	        "// KP_MULTIPLY   = '*' at keypad (same as KP_STAR).\n"
	        "// KP_NUMLCK     = Keypad-Numlock (same as NUMLOCK).\n"
	        "// KP_NUMLOCK    = Keypad-Numlock (same as NUMLOCK).\n"
	        "// KP_PGDN       = Page down at keypad (same as KP_3).\n"
	        "// KP_PGUP       = Page-Up at keypad (same as KP_9).\n"
	        "// KP_PLUS       = '+' at keypad.\n"
	        "// KP_RIGHTARROW = Cursor right at keypad (same as KP_6).\n"
	        "// KP_SLASH      = '/' at keypad (same as KP_DIVIDE).\n"
	        "// KP_STAR       = '*' at keypad (same as KP_MULTIPLY).\n"
	        "// KP_UPARROW    = Cursor up at keypad (same as KP_8).\n"
	        "// MENU          = Windows-Menu key.\n");
	fprintf (f,
	        "// NUMLOCK.\n"
	        "// PAUSE.\n"
	        "// PGDN          = Page down.\n"
	        "// PGUP          = Page up.\n"
	        "// POPUPMENU     = this is another name for MENU.\n"
	        "// PRINTSCR.\n"
	        "// RIGHTARROW    = Cursor right.\n"
	        "// SCRLCK        = Scroll-Lock (same as SCROLLLOCK).\n"
	        "// SCROLLLOCK    = Scroll-Lock (same as SCRLCK).\n"
	        "// SHIFT         = Shift-Key (this is the switch-key for the second level of\n"
	        "//                   keymappings; use this if NO separate mapping for left\n"
	        "//                   and right key is needed).\n"
	        "// LSHIFT        = Left Shift-Key (use this if a separate mapping for left and\n"
	        "//                   right key is needed).\n"
	        "// RSHIFT        = Right Shift-Key (use this if a separate mapping for left and\n"
	        "//                   right key is needed).\n"
	        "// SPACE.\n"
	        "// TAB.\n"
	        "// UPARROW       = Cursor up.\n");
	fprintf (f,
	        "// WIN           = Windows-Key (use this if no separate mapping for left and\n"
	        "//                   right key is needed).\n"
	        "// WINKEY        = this is another name for WIN.\n"
	        "// LWIN          = Left Windows-Key (use this if a separate mapping for left\n"
	        "//                   and right key is needed).\n"
	        "// LWINKEY       = this is another name for LWIN.\n"
	        "// RWIN          = Right Windows-Key (use this if a separate mapping for left\n"
	        "//                   and right key is needed).\n"
	        "// RWINKEY       = this is another name for RWIN.\n"
	        "//--------------------------------------------------------------------------\n"
	        "//ATTENTION: to use the second level of mappings at least one scancode needs\n"
	        "//             to be set to one of the Shift-Keys (SHIFT, LSHIFT or RSHIFT).\n"
	        "//           to use the third level of mappings at least one scancode needs\n"
	        "//             to be set to the ALTGR-key (or ALTCHAR, which is an alias).\n"
	        "//--------------------------------------------------------------------------\n\n");
} // END_FUNC IN_Keymap_WriteHeader_f


/*
===========
IN_Keymap_WriteKeycode_f

Part of command "keymap_save"

Write the given keycode information to the keymapfile;
identified by the given and already open filehandle
===========
*/
static void IN_Keymap_WriteKeycode_f (FILE *f, int scancode) {
	char     str[ 256 ];

	Key_KeynumToString (keymaps[0][scancode], str);
	if (keymaps[0][scancode] > 0 && 
	    ( strlen(str) == 1 || str[0] != '<' )) { // may not be "<NO KEY>", ...
		fprintf (f, "keycode  %s %3.3u  %s",
		         scancode > 128 ? "ext" : "   ",
		         scancode > 128 ? (scancode - 128) : scancode,
		         str);

		// only write the second level if the second or third
		//   level key is different to the first level:
		Key_KeynumToString (keymaps[1][scancode], str);
		if (keymaps[1][scancode] > 0 &&
		    ( keymaps[1][scancode] != keymaps[0][scancode] ||
		      keymaps[2][scancode] != keymaps[1][scancode] ) &&
		    ( strlen(str) == 1 || str[0] != '<' )) { // may not be "<NO KEY>", ...
			fprintf (f, " %s", str);

			// only write the third level, if the key is
			// different to the second level:
			Key_KeynumToString (keymaps[2][scancode], str);
			if (keymaps[2][scancode] > 0 &&
			    ( keymaps[2][scancode] != keymaps[1][scancode] ) &&
			    ( strlen(str) == 1 || str[0] != '<' )) { // may not be "<NO KEY>", ...
				fprintf (f, " %s", str);
			} // third level key
		} // second level key
		fprintf (f, "\n");
	} // first level key
} // END_FUNC IN_Keymap_WriteKeycode_f


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
	IN_Keycode_Set_f (true, NULL, 0);
}


/*
===========
IN_Keycode_Set_f

Set a custom mapping for one key.
mapping, shiftmapping, altgrmapping can be
 a character            -> maps directly to that character
 a symbol, like ENTER   -> maps to the corresponding ASCII-character
 a number, like #34      -> maps directly to the corresponding ASCII-character
===========
*/
static void IN_Keycode_Set_f (qbool showerr, char *filename, unsigned int linecount) {
	int        error, argcount, j;
	qbool   ext      = false;
	int        scancode = (int)0;
	int        key[3];
	char       linetext[ 256 ];

	linetext[0] = (char)'\0';
	if (filename != NULL)
		snprintf (linetext, sizeof (linetext), "%s (line %u): ", filename, linecount);

	if (cl_warncmd.integer <= 0 && developer.integer <= 0)
		showerr = false;

	key[0] = key[1] = key[2] = 0;

	error = 0;
	if (Cmd_Argc() < 2)
		error = 1;

	argcount = 1;
	if ( error == 0 ) {
		if (!strcasecmp( Cmd_Argv(argcount), "ext" )) {
			ext = true;
			argcount++;
			// only "ext", without a scancode:
			if (Cmd_Argc() < 3) {
				if (showerr == true)
					Com_Printf ("%sSYNTAX error in command%s!\n", linetext);
				error = 1;
			}
		} // END if (!strcasecmp( Cmd_Argv(argcount), "ext" )) 
	} // END if ( error == 0 ) 

	if ( error == 0 ) {
		scancode = Q_atoi( Cmd_Argv(argcount) );
		argcount++;
		if ( scancode < 1 || scancode > 127 ) {
			if (showerr == true)
				Com_Printf ("%sSCANCODE must be between 1 and 127!\n", linetext);
			error = 1;
		}
	} // END if ( error == 0 ) 

	// print command syntax only if not loading from file:
	if (error == 1 && filename == NULL) {
		Com_Printf ("Usage: keycode [ext] <scancode> <map> [<shiftmap>] [<altgrmap>]:\n"
		            "  Sets the key ('scancode' + 'ext' flag)\n"
		            "  to the given mappings (normal, optional shifted\n"
		            "    and optional with ALTGR)\n"
		            "where\n"
		            "  ext is a flag for extended codes\n"
		            "  scancode is a number between 1 and 127\n"
		            "  map, shiftmap, altgrmap are...\n"
		            "  ...a single character or\n"
		            "  ...a number with prefixed #:\n"
		            "       dec. number in ASCII-table or\n"
		            "  ...a keyname like ENTER identifies\n"
		            "       special keys.\n"
		            "Examples:\n"
		            " keycode ext 29 RCTRL\n"
		            "Maps the extended scancode 29 to the\n"
		            "  to the right CTRL key.\n"
		            " keycode 13 ' #34\n"
		            "Maps the scancode 13 to the character '\n"
		            "  and together with SHIFT to \"\n");
		return;
	} // END if (error == 1 && filename == NULL) 

	if ( Cmd_Argc() <= argcount ) {
		// no more arguments --> print the current mappings of the given scancode
		IN_Keymap_Print_f (scancode, ext);
		return;
	}

	// check, if one of the hard-wired (unsetable) keys shall be set:
	if (( scancode ==   1 && ext == false ) ||  // ESCAPE
	    ( scancode ==  28 && ext == false ) ||  // ENTER
	    ( scancode ==  72 && ext == true  ) ||  // UPARROW
	    ( scancode ==  75 && ext == true  ) ||  // LEFTARROW
	    ( scancode ==  77 && ext == true  ) ||  // RIGHTARROW
	    ( scancode ==  80 && ext == true  )) {  // RIGHTARROW
		char    str[ 256 ];

		if (showerr == true && filename == NULL)
			Com_Printf ("ERROR: Keycode \"%s%u\" is hard-wired to %s and may not be set!\n",
			            ext == true ? (char *)"ext " : (char *)"\0",
			            (unsigned int)scancode,
			            Key_KeynumToString( keymaps[0][ext == true ? scancode + 128 : scancode], str ));
		return;
	}


	for (j=0; j<3; j++) {
		if ( Cmd_Argc() > argcount ) {
			key[j] = Key_StringToKeynum( Cmd_Argv(argcount) );
			if ( key[j] <= 0 ) {
				if (showerr == true)
					Com_Printf( "%s\"%s\" is not a valid key\n", linetext, Cmd_Argv(argcount) );
				return;
			}
			argcount++;
		}
	}

	if ( key[1] <= 0 ) {
		// no shifted key given -> use the normal key
		key[1] = key[0];
		if ( key[0] >= (int)'a' && key[0] <= (int)'z')
			key[1] = key[0] +(int)('A' - 'a');
	}

	if ( key[2] <= 0 ) {
		// no third level key given -> use shifted key
		key[2] = key[1];
	}

	// now set the mapping:
	if ( keymap_active != true ) {
		// we need to initialize the mappings-arrays:
		IN_Keymap_Init_f();
	}

	// set the given mapping:
	keymaps[0][ ext == true ? scancode + 128 : scancode ] = key[0];
	keymaps[1][ ext == true ? scancode + 128 : scancode ] = key[1];
	keymaps[2][ ext == true ? scancode + 128 : scancode ] = key[2];
} // END_FUNC IN_Keycode_Set_f


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
		if ( keymap_active != true )
			Com_Printf ("Currently no keymappings are active!\n");
		return;
	}

	keymap_active = false;

	// reset the layout name to the default:
	Cvar_ResetVar( &keymap_name );

	if (cl_warncmd.integer || developer.integer)
		Com_Printf ("Keymappings have been reset to internal defaults (US keyboard)!\n");
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
		if ( keymap_active != true )
			Com_Printf ("Currently no keymappings are active!\n");
		return;
	}

	// copy the internal default-maps to a new mapping:
	for ( i=0; i<256; i++ ) {
		keymaps[0][i] = keymaps_default[0][i];
		keymaps[1][i] = keymaps_default[1][i];
		if ( keymaps[0][i] >= (int)'a' && keymaps[0][i] <= (int)'z' )
			keymaps[1][i] = keymaps[0][i] + (int)('A' - 'a');
		keymaps[2][i] = keymaps[1][i];
	}

	// at last set some hard-wired codes:
	keymaps[0][1]         = keymaps[1][1]         = keymaps[2][1]         = K_ESCAPE;
	keymaps[0][28]        = keymaps[1][28]        = keymaps[2][28]        = K_ENTER;
	keymaps[0][72 + 128]  = keymaps[1][72 + 128]  = keymaps[2][72 + 128]  = K_UPARROW;
	keymaps[0][75 + 128]  = keymaps[1][75 + 128]  = keymaps[2][75 + 128]  = K_LEFTARROW;
	keymaps[0][77 + 128]  = keymaps[1][77 + 128]  = keymaps[2][77 + 128]  = K_RIGHTARROW;
	keymaps[0][80 + 128]  = keymaps[1][80 + 128]  = keymaps[2][80 + 128]  = K_DOWNARROW;

	// set the layoutname to "custom":
	Cvar_Set( &keymap_name, "Custom");

	keymap_active = true;
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
static void IN_Keycode_Print_f( int scancode, qbool ext, qbool down, int key ) {
	int    flag = (int)cl_showkeycodes.value;

	if (( (flag & 2) > 0 && down != true ) || 
	    ( (flag & 1) > 0 && down == true )) {
		if ( key != K_SHIFT && key != K_LSHIFT && key != K_RSHIFT &&
		     key != K_CTRL && key != K_LCTRL && key != K_RCTRL &&
		     key != K_ALT && key != K_LALT && key != K_RALT && key != K_ALTGR )
			Com_Printf ("keycode %s %3.3u %s: %s%s%s%s%s %d\n",
			            ext == true ? "ext" : "   ",
			            (unsigned int)scancode,
			            down == true ? (char *)" pressed" : (char *)"released",
			            keydown[K_SHIFT] ? (char *)"SHIFT+" : (char *)"\0",
			            keydown[K_CTRL] ? (char *)"CTRL+" : (char *)"\0",
			            keydown[K_ALT] ? (char *)"ALT+" : (char *)"\0",
			            keydown[K_ALTGR] ? (char *)"ALTGR+" : (char *)"\0",
			            Key_KeynumToString(key, NULL), key );
		else
			Com_Printf ("keycode %s %3.3u %s: %s %i\n",
			            ext == true ? "ext" : "   ",
			            (unsigned int)scancode,
			            down == true ? (char *)" pressed" : (char *)"released",
			            Key_KeynumToString(key, NULL), key );
	}
} // END_FUNC IN_Keycode_Print_f


/*
===========
IN_Keymap_Print_f

Writes the mapping of the given scancode+ext
(normal, shift, altgr)
to the quake console
===========
*/
static void IN_Keymap_Print_f( int scancode, qbool ext )
{
	int    keycode = scancode + (ext == true ? 128 : 0);

	if (keymap_active == true) {
		char   str[3][256];

		Key_KeynumToString (keymaps[ 0 ][ keycode ], str[0]);
		Key_KeynumToString (keymaps[ 1 ][ keycode ], str[1]);
		Key_KeynumToString (keymaps[ 2 ][ keycode ], str[2]);

		if (keymaps[2][keycode] == keymaps[1][keycode])
			str[2][0] = '\0';

		if (keymaps[1][keycode] == keymaps[0][keycode] && str[2][0] == '\0')
			str[1][0] = '\0';

		Com_Printf ("keycode %s %3.3u %s %s %s\n",
		            ext == true ? (char *)"ext" : (char *)"   ",
		            scancode,
		            str[0], str[1], str[2]);
	}
	else
		Com_Printf ("No keymappings active!\n");
} // END_FUNC IN_Keymap_Print_f


/*
===========
IN_Keymaplist_f

  command "keymap_list"
Syntax:
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
		if ( keymap_active != true )
			Com_Printf ("Currently no keymappings are active!\n");
		return;
	}

	if ( keymap_active != true ) {
		Com_Printf ("No keymappings active!\n");
		return;
	}

	Com_Printf ("keymap_name \"%s\"\n", keymap_name.string);

	for (i=0; i<128; i++) {
		if (keymaps[0][i] > 0)
			IN_Keymap_Print_f( i, false );
		if (keymaps[0][i+128] > 0)
			IN_Keymap_Print_f( i, true );
	}
} // END_FUNC IN_Keymaplist_f

#endif // WITH_KEYMAP

/* vi:set ts=2 sts=2 sw=2 noet ai: */
