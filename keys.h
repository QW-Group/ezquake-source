/*
Copyright (C) 1996-1997 Id Software, Inc.

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

	$Id: keys.h,v 1.16 2007-05-28 10:47:34 johnnycz Exp $

*/

// these are the key numbers that should be passed to Key_Event

#ifndef _KEYS_H_
#define _KEYS_H_
 
typedef enum {
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE	= 32,

// normal keys should be passed as lowercased ascii

	K_BACKSPACE = 127,

	K_CAPSLOCK,
	K_PRINTSCR,
	K_SCRLCK,		//130
	K_PAUSE,

	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	
	K_ALT,
	K_LALT,
	K_RALT,
#ifdef WITH_KEYMAP
	K_ALTGR,    // Right Alt-Key (=AltGr Key; this key must be used, if a third level of mappings is needed)
#endif // WITH_KEYMAP 
	K_CTRL,			//140
	K_LCTRL,
	K_RCTRL,
	K_SHIFT,
	K_LSHIFT,
	K_RSHIFT,
#ifdef __APPLE__
	K_CMD,
	K_PARA,
#endif
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,			//150
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
#ifdef __APPLE__
	K_F13,
	K_F14,
	K_F15,
#endif
	K_INS,
	K_DEL,
	K_PGDN,			//160
	K_PGUP,
	K_HOME,
	K_END,

	K_WIN,
	K_LWIN,
	K_RWIN,
	K_MENU,

//
// Keypad stuff..
//

	KP_NUMLOCK,
	KP_SLASH,
	KP_STAR,		//170

	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	KP_MINUS,

	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_PLUS,

	KP_END,
	KP_DOWNARROW,	//180
	KP_PGDN,

	KP_INS,
	KP_DEL,
	KP_ENTER,

#ifdef __APPLE__
	// macintosh keypad
	KP_EQUAL,

	KP_7=KP_HOME,
	KP_8=KP_UPARROW,
	KP_9=KP_PGUP,

	KP_4=KP_LEFTARROW,
	KP_6=KP_RIGHTARROW,

	KP_1=KP_END,
	KP_2=KP_DOWNARROW,
	KP_3=KP_PGDN,

	KP_0=KP_INS,
	KP_DOT=KP_DEL,
#endif

	PRINT_SCREEN,
	WAKE_UP,		//186

//
// mouse buttons generate virtual keys
//
	K_MOUSE1 = 200,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,

//
// joystick buttons
//
	K_JOY1,
	K_JOY2,
	K_JOY3,			//210
	K_JOY4,

//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,			//220
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,		//230
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28,
	K_AUX29,		//240
	K_AUX30,
	K_AUX31,
	K_AUX32,

// JACK: Intellimouse(c) Mouse Wheel Support

	K_MWHEELUP,
	K_MWHEELDOWN,	//245

	UNKNOWN = 256

} keynum_t;


typedef enum { key_game, key_console, key_message, key_menu, key_hudeditor, key_demo_controls } keydest_t;

extern keydest_t	key_dest, key_dest_beforemm, key_dest_beforecon;
extern char 	*keybindings[UNKNOWN + 256];
extern int		key_repeats[UNKNOWN + 256];
extern qbool	keydown[UNKNOWN + 256];
extern int		key_lastpress;

#ifdef WITH_IRC
	typedef enum { chat_mm1, chat_mm2, chat_irc, chat_qtvtogame }	chat_type;
#else
	typedef enum { chat_mm1, chat_mm2, chat_qtvtogame }	chat_type;
#endif

extern wchar 	chat_buffer[];
extern int 		chat_linepos;
extern chat_type chat_team;


// this is message type sent across windows that accept mouse pointer
typedef struct mouse_state_s {
	double x;           // current mouse pointer horisontal position
	double y;           // current mouse pointer vertical position
	double x_old;	    // previous mouse pointer positions
	double y_old;
    qbool buttons[9];   // button states .. omit button 0
    int button_down;    // number of the button that just has been pressed down
    int button_up;      // number of the button that just has been released
} mouse_state_t;

// exported only to be changed in the cl_screen.c module where mouse position gets updated
// do not access this variable anywhere else, prefer "window-messages" system
extern mouse_state_t scr_pointer_state;

// used in cl_screen module which is responsible for updating the mouse pointer position
void Mouse_MoveEvent(void);

void History_Init (void);
void History_Shutdown (void);

// this will clear any typping in console
void Key_ClearTyping (void);

void Key_Event (int key, qbool down);
void Key_EventEx (int key, wchar unichar, qbool down);
void Key_Init (void);
void Key_WriteBindings (FILE *f);
void Key_SetBinding (int keynum, const char *binding);
void Key_Unbind (int keynum);
void Key_ClearStates (void);
int	 Key_StringToKeynum (const char *str);
#ifdef WITH_KEYMAP
char	*Key_KeynumToString (int keynum, char *buffer);
int	IN_Key_Clean(int key);
#else // WITH_KEYMAP
char *Key_KeynumToString (int keynum);
#endif // WITH_KEYMAP 
void Key_Unbindall_f (void);

int isShiftDown(void);                                                                       
int isCtrlDown(void);                                                                        
int isAltDown(void);

// should not be public actually but...
// {
#define		CMDLINES	(1<<8)
#define		MAXCMDLINE	256

extern wchar	key_lines[CMDLINES][MAXCMDLINE];
extern int		key_linepos;
extern int		edit_line;
extern qbool	con_redchars;

// }

#endif // _KEYS_H_ 
