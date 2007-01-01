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

	$Id: menu.c,v 1.53 2007-01-01 16:38:21 tonik Exp $

*/

#include "quakedef.h"
#include "winquake.h"
//#include "cl_slist.h"
#ifndef CLIENTONLY
#include "server.h"
#endif

#include "mp3_player.h"
#include "EX_browser.h"
#include "menu_demo.h"

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef GLQUAKE
enum {mode_fastest, mode_faithful, mode_eyecandy, mode_movies} fps_mode = mode_faithful;
#else
enum {mode_fastest, mode_default} fps_mode = mode_default;
#endif


qbool vid_windowedmouse = true;
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);
void CL_Disconnect_f(void);

extern void Browser_Init(void);
extern void Browser_Draw(int, int, int, int);
extern cvar_t con_shift;
extern cvar_t sb_maxwidth, sb_maxheight;
extern cvar_t scr_fov;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_ServerList_f (void);
			void M_Menu_SEdit_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Demos_f (void);
		void M_Menu_GameOptions_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Fps_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_MP3_Control_f (void);
	void M_Menu_Quit_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_ServerList_Draw (void);
			void M_SEdit_Draw (void);
		void M_Setup_Draw (void);
		void M_Demo_Draw (void);
		void M_GameOptions_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Fps_Draw (void);
		void M_Video_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_ServerList_Key (int key);
			void M_SEdit_Key (int key);
		void M_Setup_Key (int key, int unichar);
		void M_Demo_Key (int key);
		void M_GameOptions_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Fps_Key (int key);
		void M_Video_Key (int key);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);


int FindBestNick (char *s,int use);

qbool    m_entersound;          // play after drawing a frame, so caching
                                // won't disrupt the sound
qbool    m_recursiveDraw;
int            m_topmenu;       // set if a submenu was entered via a
                                // menu_* command


//=============================================================================
/* Support Routines */

#ifdef GLQUAKE
cvar_t     scr_scaleMenu = {"scr_scaleMenu","1"};
int        menuwidth = 320;
int        menuheight = 240;
#else
#define menuwidth vid.width
#define menuheight vid.height
#endif

cvar_t     scr_centerMenu = {"scr_centerMenu","1"};
int        m_yofs = 0;

void M_DrawCharacter (int cx, int line, int num) {
	Draw_Character (cx + ((menuwidth - 320)>>1), line + m_yofs, num);
}

void M_Print (int cx, int cy, char *str) {
	Draw_Alt_String (cx + ((menuwidth - 320)>>1), cy + m_yofs, str);
}

void M_PrintWhite (int cx, int cy, char *str) {
	Draw_String (cx + ((menuwidth - 320)>>1), cy + m_yofs, str);
}

void M_DrawTransPic (int x, int y, mpic_t *pic) {
	Draw_TransPic (x + ((menuwidth - 320)>>1), y + m_yofs, pic);
}

void M_DrawPic (int x, int y, mpic_t *pic) {
	Draw_Pic (x + ((menuwidth - 320)>>1), y + m_yofs, pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom) {
	int        j;
	byte    *dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)    // the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}


void M_DrawTransPicTranslate (int x, int y, mpic_t *pic) {
	Draw_TransPicTranslate (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines) {
	Draw_TextBox (x + ((menuwidth - 320) >> 1), y + m_yofs, width, lines);
}

//=============================================================================

void M_ToggleMenu_f (void) {
	m_entersound = true;

	if (key_dest == key_menu) {
		if (m_state != m_main) {
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
	} else {
		M_Menu_Main_f ();
	}
}

void M_EnterMenu (int state) {
	if (key_dest != key_menu) {
		m_topmenu = state;
		Con_ClearNotify ();
		// hide the console
		scr_conlines = 0;
		scr_con_current = 0;
	} else {
		m_topmenu = m_none;
	}

	key_dest = key_menu;
	m_state = state;
	m_entersound = true;
}

void M_LeaveMenu (int parent) {
	if (m_topmenu == m_state) {
		m_state = m_none;
		key_dest = key_game;
	} else {
		m_state = parent;
		m_entersound = true;
	}
}

// dunno how to call this function
// must leave all menus instead of calling few functions in row
void M_LeaveMenus (void) {
//	m_entersound = true; // fixme: which value we must set ???

	m_topmenu = m_none;
	m_state   = m_none;
	key_dest  = key_game;
}

//=============================================================================
/* MAIN MENU */

int    m_main_cursor;
#define    MAIN_ITEMS    5


void M_Menu_Main_f (void) {
	M_EnterMenu (m_main);
}

void M_Main_Draw (void) {
	int f;
	mpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mainmenu.lmp") );

	f = (int)(curtime * 10)%6;

	M_DrawTransPic (54, 32 + m_main_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}

void M_Main_Key (int key) {
	extern cvar_t cl_confirmquit;
	switch (key) {
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor) {
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_MultiPlayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

	#if defined(_WIN32) || defined(__XMMS__)
		case 3:
			M_Menu_MP3_Control_f ();
			break;
	#endif

		case 4:
			// START shaman [Quake 1.09 quit screen]
			if (cl_confirmquit.value) {
				M_Menu_Quit_f ();
			}
			else {
				Host_Quit ();
			}
			// END shaman [Quake 1.09 quit screen]
			break;
		}
	}
}


//=============================================================================
/* OPTIONS MENU */

#define    OPTIONS_ITEMS    18

#define    SLIDER_RANGE    10

int        options_cursor;


void M_Menu_Options_f (void) {
	M_EnterMenu (m_options);
}


void M_AdjustSliders (int dir) {
	float valuebuf;
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor) {
	case 3:    // screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue (&scr_viewsize, scr_viewsize.value);
		break;
	case 4:    // gamma
		v_gamma.value -= dir * 0.05;
		if (v_gamma.value < 0.5)
			v_gamma.value = 0.5;
		if (v_gamma.value > 1)
			v_gamma.value = 1;
		Cvar_SetValue (&v_gamma, v_gamma.value);
		break;
	case 5:    // contrast
		v_contrast.value += dir * 0.1;
		if (v_contrast.value < 1)
			v_contrast.value = 1;
		if (v_contrast.value > 2)
			v_contrast.value = 2;
		Cvar_SetValue (&v_contrast, v_contrast.value);
		break;
	case 6:    // mouse speed
		sensitivity.value += dir * 0.5;
		if (sensitivity.value < 3)
			sensitivity.value = 3;
		if (sensitivity.value > 15)
			sensitivity.value = 15;
		Cvar_SetValue (&sensitivity, sensitivity.value);
		break;
	case 7:    // music volume
		valuebuf = scr_fov.value + dir * 5;
		valuebuf = bound(40, valuebuf, 140);
		Cvar_SetValue (&scr_fov, valuebuf);
		break;
	case 8:    // sfx volume
		s_volume.value += dir * 0.1;
		if (s_volume.value < 0)
			s_volume.value = 0;
		if (s_volume.value > 1)
			s_volume.value = 1;
		Cvar_SetValue (&s_volume, s_volume.value);
		break;

	case 9:    // always run
		if (cl_forwardspeed.value > 200) {
			Cvar_SetValue (&cl_forwardspeed, 200);
			Cvar_SetValue (&cl_backspeed, 200);
			Cvar_SetValue (&cl_sidespeed, 200);
		} else {
			Cvar_SetValue (&cl_forwardspeed, 400);
			Cvar_SetValue (&cl_backspeed, 400);
			Cvar_SetValue (&cl_sidespeed, 400);
		}
		break;

	case 10:    // mouse look
		Cvar_SetValue (&freelook, !freelook.value);
		break;

	case 11:    // invert mouse
		Cvar_SetValue (&m_pitch, -m_pitch.value);
		break;

	case 12:    // autoswitch weapons
		if ((w_switch.value > 2) || (!w_switch.value) || (b_switch.value > 2) || (!b_switch.value))
		{
			Cvar_SetValue (&w_switch, 1);
			Cvar_SetValue (&b_switch, 1);
		} else {
			Cvar_SetValue (&w_switch, 8);
			Cvar_SetValue (&b_switch, 8);
		}
		break;

	case 13:
		Cvar_SetValue (&cl_sbar, !cl_sbar.value);
		break;

	case 14:
		Cvar_SetValue (&cl_hudswap, !cl_hudswap.value);
		break;

	case 17:    // _windowed_mouse
		Cvar_SetValue (&_windowed_mouse, !_windowed_mouse.value);
		break;
	}
}


void M_DrawSlider (int x, int y, float range) {
	int    i;

	range = bound(0, range, 1);
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on) {
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void) {
	float        r;
	char temp[32];
	mpic_t    *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_option.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (16, 32, "    Customize controls");
	M_Print (16, 40, "         Go to console");
	M_Print (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "                 Gamma");
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "              Contrast");
	r = v_contrast.value - 1.0;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "           Mouse speed");
	r = (sensitivity.value - 3)/(15 - 3);
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "         Field of view");
	r = (scr_fov.value - 40) / (140 - 40);
	M_DrawSlider (220, 88, r);

	M_Print (16, 96, "          Sound volume");
	r = s_volume.value;
	M_DrawSlider (220, 96, r);

	M_Print (16, 104,  "            Always run");
	M_DrawCheckbox (220, 104, cl_forwardspeed.value > 200);

	M_Print (16, 112, "            Mouse look");
	M_DrawCheckbox (220, 112, freelook.value);

	M_Print (16, 120, "          Invert mouse");
	M_DrawCheckbox (220, 120, m_pitch.value < 0);

	M_Print (16, 128, "    Autoswitch weapons");
	M_DrawCheckbox (220, 128, !w_switch.value || (w_switch.value > 2) || !b_switch.value || (b_switch.value > 2));

	M_Print (16, 136, "    Use old status bar");
	M_DrawCheckbox (220, 136, cl_sbar.value);

	M_Print (16, 144, "      HUD on left side");
	M_DrawCheckbox (220, 144, cl_hudswap.value);

	M_Print (16, 152, "     Graphics settings");
	switch (fps_mode) {
		case mode_fastest   : strcpy(temp, "fastest"); break;
#ifdef GLQUAKE
		case mode_faithful    : strcpy(temp, "faithful"); break;
		case mode_movies    : strcpy(temp, "movies"); break;
		default : strcpy(temp, "eye candy"); break;
#else
		default : strcpy(temp, "default"); break;
#endif
	}
	M_PrintWhite (220, 152, temp);

	if (vid_menudrawfn)
		M_Print (16, 160, "           Video modes");

#ifdef _WIN32
	if (modestate == MS_WINDOWED)
#else
	if (vid_windowedmouse)
#endif
	{
		M_Print (16, 168, "             Use mouse");
		M_DrawCheckbox (220, 168, _windowed_mouse.value);
	}

	// cursor
	M_DrawCharacter (200, 32 + options_cursor * 8, FLASHINGARROW());
}


void M_Options_Key (int k) {
	switch (k) {
	case K_BACKSPACE:
		m_topmenu = m_none;    // intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_ENTER:
		m_entersound = true;
		switch (options_cursor) {
			case 0:
				M_Menu_Keys_f ();
				break;
			case 1:
				m_state = m_none;
				key_dest = key_console;
				//            Con_ToggleConsole_f ();
				break;
			case 2:
				Cbuf_AddText ("exec default.cfg\n");
				break;
			case 15:
				//        M_Menu_Fps_f ();
				switch (fps_mode) {
#ifdef GLQUAKE
					case mode_fastest:
						fps_mode = mode_faithful;
						break;
					case mode_faithful:
						fps_mode = mode_eyecandy;
						break;
					case mode_movies:
						fps_mode = mode_fastest;
						break;
					default:
						fps_mode = mode_movies;
						break;
#else
					case mode_fastest:
						fps_mode = mode_default;
						break;
					default:
						fps_mode = mode_fastest;
						break;
#endif
				}

				switch (fps_mode) {
#ifdef GLQUAKE
					case mode_fastest:
						Cbuf_AddText ("exec cfg/gfx_gl_fast.cfg\n");
						if (gl_max_size.value > 64) {
							Cvar_SetValue (&gl_max_size, 1);
						}
						break;
					case mode_faithful:
						Cbuf_AddText ("exec cfg/gfx_gl_faithful.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
					case mode_movies:
						Cbuf_AddText ("exec cfg/gfx_gl_movies.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
					default:
						Cbuf_AddText ("exec cfg/gfx_gl_eyecandy.cfg\n");
						Cvar_SetValue (&gl_max_size, gl_max_size_default);
						break;
#else
					case mode_fastest:
						Cbuf_AddText ("exec cfg/gfx_sw_fast.cfg\n");
						break;
					case mode_default:
						Cbuf_AddText ("exec cfg/gfx_sw_default.cfg\n");
						break;
#endif
				}

				break;
			case 16:
				M_Menu_Video_f ();
				break;
			default:
				M_AdjustSliders (1);
				break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		options_cursor = OPTIONS_ITEMS - 1;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}

	if (k == K_UPARROW || k == K_END || k == K_PGDN) {
#ifdef _WIN32
		if (options_cursor == 17 && modestate != MS_WINDOWED)
#else
		if (options_cursor == 17 && !vid_windowedmouse)
#endif
				options_cursor = 16;

		if (options_cursor == 16 && vid_menudrawfn == NULL)
			options_cursor = 15;
	} else {
		if (options_cursor == 16 && vid_menudrawfn == NULL)
			options_cursor = 17;

#ifdef _WIN32
		if (options_cursor == 17 && modestate != MS_WINDOWED)
#else
		if (options_cursor == 17 && !vid_windowedmouse)
#endif
				options_cursor = 0;
	}
}


//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
	{"+attack",         "attack"},
	{"+use",            "use"},
	{"+jump",           "jump"},
	{"+forward",        "move forward"},
	{"+back",           "move back"},
	{"+moveleft",       "move left"},
	{"+moveright",      "move right"},
	{"impulse 12",      "previous weapon"},
	{"impulse 10",      "next weapon"},
	{"weapon 1",       "axe"},
	{"weapon 2",       "shotgun"},
	{"weapon 3",       "super shotgun"},
	{"weapon 4",       "nailgun"},
	{"weapon 5",       "super nailgun"},
	{"weapon 6",       "grenade launcher"},
	{"weapon 7",       "rocket launcher"},
	{"weapon 8",       "thunderbolt"},
};

#define    NUMCOMMANDS    (sizeof(bindnames)/sizeof(bindnames[0]))

int        keys_cursor;
int        bind_grab;

void M_Menu_Keys_f (void) {
	M_EnterMenu (m_keys);
}

qbool Key_IsLeftRightSameBind(int b);

void M_FindKeysForCommand (char *command, int *twokeys) {
	int count, j, l;
	char *b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j = 0 ; j < (sizeof(keybindings) / sizeof(*keybindings)); j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l)) {
			if (count) {
				if (j == twokeys[0] + 1 && (twokeys[0] == K_LCTRL || twokeys[0] == K_LSHIFT || twokeys[0] == K_LALT)) {

					twokeys[0]--;
					continue;
				}
			}
			twokeys[count] = j;
			count++;
			if (count == 2) {

				if (Key_IsLeftRightSameBind(twokeys[1]))
					twokeys[1]++;
				break;
			}
		}
	}
}

void M_UnbindCommand (char *command) {
	int j, l;
	char *b;

	l = strlen(command);

	for (j = 0; j < (sizeof(keybindings) / sizeof(*keybindings)); j++) {
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_Unbind (j);
	}
}


void M_Keys_Draw (void) {
	int x, y, i, l, keys[2];
	char *name;
	mpic_t *p;

	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, del to clear");

	// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++) {
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1) {
			M_Print (156, y, "???");
		} else {
#ifdef WITH_KEYMAP
			char    str[256];
			name = Key_KeynumToString (keys[0], str);
#else // WITH_KEYMAP
			name = Key_KeynumToString (keys[0]);
#endif // WITH_KEYMAP else
			M_Print (156, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1) {
				M_Print (156 + x + 8, y, "or");
#ifdef WITH_KEYMAP
				M_Print (156 + x + 32, y, Key_KeynumToString (keys[1], str));
#else // WITH_KEYMAP
				M_Print (156 + x + 32, y, Key_KeynumToString (keys[1]));
#endif // WITH_KEYMAP else
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (142, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (142, 48 + keys_cursor*8, FLASHINGARROW());
}


void M_Keys_Key (int k) {
	int keys[2];

	if (bind_grab) {
		// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
			bind_grab = false;
#ifdef WITH_KEYMAP
		/* Massa: all keys should be bindable, including the console switching */
		else
#else // WITH_KEYMAP
        else if (k != '`')
#endif // WITH_KEYMAP else
            Key_SetBinding (k, bindnames[keys_cursor][0]);

			bind_grab = false;
			return;
	}

	switch (k) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_options);
			break;

		case K_LEFTARROW:
		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor--;
			if (keys_cursor < 0)
				keys_cursor = NUMCOMMANDS-1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor++;
			if (keys_cursor >= NUMCOMMANDS)
				keys_cursor = 0;
			break;

		case K_HOME:
		case K_PGUP:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor = 0;
			break;

		case K_END:
		case K_PGDN:
			S_LocalSound ("misc/menu1.wav");
			keys_cursor = NUMCOMMANDS - 1;
			break;

		case K_ENTER:        // go into bind mode
			M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
			S_LocalSound ("misc/menu2.wav");
			if (keys[1] != -1)
				M_UnbindCommand (bindnames[keys_cursor][0]);
			bind_grab = true;
			break;

		case K_DEL:                // delete bindings
			S_LocalSound ("misc/menu2.wav");
			M_UnbindCommand (bindnames[keys_cursor][0]);
			break;
	}
}



//=============================================================================
/* FPS SETTINGS MENU */

#define    FPS_ITEMS    15

int        fps_cursor = 0;

extern cvar_t v_bonusflash;
extern cvar_t cl_rocket2grenade;
extern cvar_t v_damagecshift;
extern cvar_t r_fastsky;
extern cvar_t r_drawflame;
extern cvar_t gl_part_inferno;

void M_Menu_Fps_f (void) {
	M_EnterMenu (m_fps);
}

#define ALIGN_FPS_OPTIONS    208

void M_Fps_Draw (void) {
	mpic_t    *p;
	char temp[32];

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp"));
	p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	M_DrawPic ((320-p->width)/ 2, 4, p);

	M_Print (16, 32, "            Explosions");
	switch ((int) r_explosiontype.value) {
		case 0    : strcpy(temp, "fire + sparks"); break;
		case 1    : strcpy(temp, "fire only"); break;
		case 2    : strcpy(temp, "teleport"); break;
		case 3    : strcpy(temp, "blood"); break;
		case 4    : strcpy(temp, "big blood"); break;
		case 5    : strcpy(temp, "dbl gunshot"); break;
		case 6    : strcpy(temp, "blob effect"); break;
		case 7    : strcpy(temp, "big explosion"); break;
		default : strcpy(temp, "fire + sparks"); break;
	}
	M_Print (ALIGN_FPS_OPTIONS, 32, temp);

	M_Print (16, 40, "         Muzzleflashes");
	M_Print (ALIGN_FPS_OPTIONS, 40, cl_muzzleflash.value == 2 ? "own off" : cl_muzzleflash.value ? "on" : "off");

	M_Print (16, 48, "            Gib filter");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 48, cl_gibfilter.value);

	M_Print (16, 56, "      Dead body filter");
	M_Print (ALIGN_FPS_OPTIONS, 56, cl_deadbodyfilter.value == 2 ? "on (instant)" : cl_deadbodyfilter.value ? "on (normal)" : "off");

	M_Print (16, 64, "          Rocket model");
	M_Print (ALIGN_FPS_OPTIONS, 64, cl_rocket2grenade.value ? "grenade" : "normal");

	switch ((int) r_rockettrail.value) {
		case 0    : strcpy(temp, "off"); break;
		case 1    : strcpy(temp, "normal"); break;
		case 2    : strcpy(temp, "grenade"); break;
		case 3    : strcpy(temp, "alt normal"); break;
		case 4    : strcpy(temp, "slight blood"); break;
		case 5    : strcpy(temp, "big blood"); break;
		case 6    : strcpy(temp, "tracer 1"); break;
		case 7    : strcpy(temp, "tracer 2"); break;
		default : strcpy(temp, "normal"); break;
	}

	M_Print (16, 72, "          Rocket trail");
	M_Print (ALIGN_FPS_OPTIONS, 72, temp);

	M_Print (16, 80, "          Rocket light");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 80, r_rocketlight.value);

	M_Print (16, 88, "         Damage filter");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 88, v_damagecshift.value == 0);

	M_Print (16, 96, "        Pickup flashes");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 96, v_bonusflash.value);

	M_Print (16, 104, "         Powerup glow");
	M_Print (ALIGN_FPS_OPTIONS, 104, r_powerupglow.value==2 ? "own off" :
		r_powerupglow.value ? "on" : "off");

	M_Print (16, 112, "         Draw torches");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 112, r_drawflame.value);

	M_Print (16, 120, "             Fast sky");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 120, r_fastsky.value);

#ifdef GLQUAKE
	M_Print (16, 128, "          Fast lights");
	M_DrawCheckbox (ALIGN_FPS_OPTIONS, 128, gl_flashblend.value);
#endif

	M_PrintWhite (16, 136, "            Fast mode");

	M_PrintWhite (16, 144, "         High quality");

	// cursor
	M_DrawCharacter (196, 32 + fps_cursor * 8, FLASHINGARROW());
}

void M_Fps_Key (int k) {
	int i;

	switch (k) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_options);
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor--;
#ifndef GLQUAKE
			if (fps_cursor == 12)
				fps_cursor = 11;
#endif
			if (fps_cursor < 0)
				fps_cursor = FPS_ITEMS - 1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor++;
#ifndef GLQUAKE
			if (fps_cursor == 12)
				fps_cursor = 13;
#endif
			if (fps_cursor >= FPS_ITEMS)
				fps_cursor = 0;
			break;

		case K_HOME:
		case K_PGUP:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor = 0;
			break;

		case K_END:
		case K_PGDN:
			S_LocalSound ("misc/menu1.wav");
			fps_cursor = FPS_ITEMS - 1;
			break;

		case K_RIGHTARROW:
		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			switch (fps_cursor) {
				case 0:
					i = r_explosiontype.value + 1;
					if (i > 7 || i < 0)
						i = 0;
					Cvar_SetValue (&r_explosiontype, i);
					break;
				case 1:
					Cvar_SetValue (&cl_muzzleflash, cl_muzzleflash.value == 2 ? 1 : cl_muzzleflash.value ? 0 : 2);
					break;
				case 2:
					Cvar_SetValue (&cl_gibfilter, !cl_gibfilter.value);
					break;
				case 3:
					Cvar_SetValue (&cl_deadbodyfilter, cl_deadbodyfilter.value == 2 ? 1 : cl_deadbodyfilter.value ? 0 : 2);
					break;
				case 4:
					Cvar_SetValue (&cl_rocket2grenade, !cl_rocket2grenade.value);
					break;
				case 5:
					i = r_rockettrail.value + 1;
					if (i < 0 || i > 7)
						i = 0;
					Cvar_SetValue (&r_rockettrail, i);
					break;
				case 6:
					Cvar_SetValue (&r_rocketlight, !r_rocketlight.value);
					break;
				case 7:
					Cvar_SetValue (&v_damagecshift, !v_damagecshift.value);
					break;
				case 8:
					Cvar_SetValue (&v_bonusflash, !v_bonusflash.value);
					break;
				case 9:
					i = r_powerupglow.value + 1;
					if (i < 0 || i > 2)
						i = 0;
					Cvar_SetValue (&r_powerupglow, i);
					break;
				case 10:
					Cvar_SetValue (&r_drawflame, !r_drawflame.value);
					break;
				case 11:
					Cvar_SetValue (&r_fastsky, !r_fastsky.value);
					break;

#ifdef GLQUAKE
				case 12:
					Cvar_SetValue (&gl_flashblend, !gl_flashblend.value);
					Cvar_SetValue (&r_dynamic, gl_flashblend.value);
					break;
#endif

					// fast
				case 13:
					Cvar_SetValue (&r_explosiontype, 1);
					Cvar_SetValue (&r_explosionlight, 0);
					Cvar_SetValue (&cl_muzzleflash, 0);
					Cvar_SetValue (&cl_gibfilter, 1);
					Cvar_SetValue (&cl_deadbodyfilter, 1);
					Cvar_SetValue (&r_rocketlight, 0);
					Cvar_SetValue (&r_powerupglow, 0);
					Cvar_SetValue (&r_drawflame, 0);
					Cvar_SetValue (&r_fastsky, 1);
					Cvar_SetValue (&r_rockettrail, 1);
					Cvar_SetValue (&v_damagecshift, 0);
#ifdef GLQUAKE
					Cvar_SetValue (&gl_flashblend, 1);
					Cvar_SetValue (&r_dynamic, 0);
					Cvar_SetValue (&gl_part_explosions, 0);
					Cvar_SetValue (&gl_part_trails, 0);
					Cvar_SetValue (&gl_part_spikes, 0);
					Cvar_SetValue (&gl_part_gunshots, 0);
					Cvar_SetValue (&gl_part_blood, 0);
					Cvar_SetValue (&gl_part_telesplash, 0);
					Cvar_SetValue (&gl_part_blobs, 0);
					Cvar_SetValue (&gl_part_lavasplash, 0);
					Cvar_SetValue (&gl_part_inferno, 0);
#endif
					break;

					// high quality
				case 14:
					Cvar_SetValue (&r_explosiontype, 0);
					Cvar_SetValue (&r_explosionlight, 1);
					Cvar_SetValue (&cl_muzzleflash, 1);
					Cvar_SetValue (&cl_gibfilter, 0);
					Cvar_SetValue (&cl_deadbodyfilter, 0);
					Cvar_SetValue (&r_rocketlight, 1);
					Cvar_SetValue (&r_powerupglow, 2);
					Cvar_SetValue (&r_drawflame, 1);
					Cvar_SetValue (&r_fastsky, 0);
					Cvar_SetValue (&r_rockettrail, 1);
					Cvar_SetValue (&v_damagecshift, 1);
#ifdef GLQUAKE
					Cvar_SetValue (&gl_flashblend, 0);
					Cvar_SetValue (&r_dynamic, 1);
					Cvar_SetValue (&gl_part_explosions, 1);
					Cvar_SetValue (&gl_part_trails, 1);
					Cvar_SetValue (&gl_part_spikes, 1);
					Cvar_SetValue (&gl_part_gunshots, 1);
					Cvar_SetValue (&gl_part_blood, 1);
					Cvar_SetValue (&gl_part_telesplash, 1);
					Cvar_SetValue (&gl_part_blobs, 1);
					Cvar_SetValue (&gl_part_lavasplash, 1);
					Cvar_SetValue (&gl_part_inferno, 1);
#endif
			}
	}
}


//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void) {
	M_EnterMenu (m_video);
}

void M_Video_Draw (void) {
	(*vid_menudrawfn) ();
}

void M_Video_Key (int key) {
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* HELP MENU */

int        help_page;
#define    NUM_HELP_PAGES    6

void M_Menu_Help_f (void) {
	M_EnterMenu (m_help);
}

void M_Help_Draw (void) {
	//    M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
	extern void Help_Draw(int, int, int, int);

	int x, y, w, h;

#ifdef GLQUAKE
	// disconnect: unscale help menu
	if (scr_scaleMenu.value) {
		menuwidth = vid.width;
		menuheight = vid.height;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	}
#endif

	w = min(max(512, 320), vid.width) - 8;
	h = min(max(432, 200), vid.height) - 8;
	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;

	Help_Draw (x, y, w, h);
}

void M_Help_Key (int key) {
	extern void Help_Key(int key);
	Help_Key(key);
}


//=============================================================================
/* QUIT MENU */

int        msgNumber;
int        m_quit_prevstate;
qbool    wasInMenus;

void M_Menu_Quit_f (void) {
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	m_quit_prevstate = m_state;
	msgNumber = rand()&7;
	M_EnterMenu (m_quit);
}

void M_Quit_Key (int key) {
	switch (key) {
		case K_ESCAPE:
		case 'n':
		case 'N':
			if (wasInMenus) {
				m_state = m_quit_prevstate;
				m_entersound = true;
			} else {
				key_dest = key_game;
				m_state = m_none;
			}
			break;

		case K_ENTER:
		case 'Y':
		case 'y':
			key_dest = key_console;
			Host_Quit ();
			break;

		default:
			break;
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

#ifndef CLIENTONLY

#define    SINGLEPLAYER_ITEMS    3
int    m_singleplayer_cursor;
qbool m_singleplayer_confirm;
qbool m_singleplayer_notavail;

extern    cvar_t    maxclients;

void M_Menu_SinglePlayer_f (void) {
	M_EnterMenu (m_singleplayer);
	m_singleplayer_confirm = false;
	m_singleplayer_notavail = false;
}

void M_SinglePlayer_Draw (void) {
	int f;
	mpic_t *p;

	if (m_singleplayer_notavail) {
		p = Draw_CachePic ("gfx/ttl_sgl.lmp");
		M_DrawPic ( (320-p->width)/2, 4, p);
		M_DrawTextBox (60, 10*8, 24, 4);
		M_PrintWhite (80, 12*8, " Cannot start a game");
		M_PrintWhite (80, 13*8, "spprogs.dat not found");
		return;
	}

	if (m_singleplayer_confirm) {
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	f = (int)(curtime * 10)%6;
	M_DrawTransPic (54, 32 + m_singleplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
}

static void CheckSPGame (void) {
	FILE *f;

	FS_FOpenFile ("spprogs.dat", &f);
	if (f) {
		fclose (f);
		m_singleplayer_notavail = false;
	} else {
		m_singleplayer_notavail = true;
	}
}

static void StartNewGame (void) {
	key_dest = key_game;
	Cvar_Set (&maxclients, "1");
	Cvar_Set (&teamplay, "0");
	Cvar_Set (&deathmatch, "0");
	Cvar_Set (&coop, "0");

	if (com_serveractive)
		Cbuf_AddText ("disconnect\n");


	progs = (dprograms_t *) FS_LoadHunkFile ("spprogs.dat");
	if (progs && !file_from_gamedir)
		Cbuf_AddText ("gamedir qw\n");
	Cbuf_AddText ("map start\n");
}

void M_SinglePlayer_Key (int key) {
	if (m_singleplayer_notavail) {
		switch (key) {
			case K_BACKSPACE:
			case K_ESCAPE:
			case K_ENTER:
				m_singleplayer_notavail = false;
				break;
		}
		return;
	}

	if (m_singleplayer_confirm) {
		if (key == K_ESCAPE || key == 'n') {
			m_singleplayer_confirm = false;
			m_entersound = true;
		} else if (key == 'y' || key == K_ENTER) {
			StartNewGame ();
		}
		return;
	}

	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
				m_singleplayer_cursor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			if (--m_singleplayer_cursor < 0)
				m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
			break;

		case K_HOME:
		case K_PGUP:
			S_LocalSound ("misc/menu1.wav");
			m_singleplayer_cursor = 0;
			break;

		case K_END:
		case K_PGDN:
			S_LocalSound ("misc/menu1.wav");
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
			break;

		case K_ENTER:
			switch (m_singleplayer_cursor) {
				case 0:
					CheckSPGame ();
					if (m_singleplayer_notavail) {
						m_entersound = true;
						return;
					}
					if (com_serveractive) {
						// bring up confirmation dialog
						m_singleplayer_confirm = true;
						m_entersound = true;
					} else {
						StartNewGame ();
					}
					break;

				case 1:
					M_Menu_Load_f ();
					break;

				case 2:
					M_Menu_Save_f ();
					break;
			}
	}
}

#else    // !CLIENTONLY

void M_Menu_SinglePlayer_f (void) {
	M_EnterMenu (m_singleplayer);
}

void M_SinglePlayer_Draw (void) {
	mpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/ttl_sgl.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	//    M_DrawTransPic (72, 32, Draw_CachePic ("gfx/sp_menu.lmp") );

	M_DrawTextBox (60, 10*8, 23, 4);
	M_PrintWhite (88, 12*8, "This client is for");
	M_PrintWhite (88, 13*8, "Internet play only");
}

void M_SinglePlayer_Key (key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
		case K_ENTER:
			M_LeaveMenu (m_main);
			break;
	}
}
#endif    // CLIENTONLY


//=============================================================================
/* LOAD/SAVE MENU */

#ifndef CLIENTONLY

#define    MAX_SAVEGAMES        12

int        load_cursor;        // 0 < load_cursor < MAX_SAVEGAMES
char    m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
int        loadable[MAX_SAVEGAMES];

void M_ScanSaves (char *sp_gamedir) {
	int i, j, version;
	char name[MAX_OSPATH];
	FILE *f;

	for (i = 0; i < MAX_SAVEGAMES; i++) {
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		snprintf (name, sizeof(name), "%s/save/s%i.sav", sp_gamedir, i);
		if (!(f = fopen (name, "r")))
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		strlcpy (m_filenames[i], name, sizeof(m_filenames[i]));

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

void M_Menu_Load_f (void) {
	FILE *f;

	if (FS_FOpenFile ("spprogs.dat", &f) == -1)
		return;

	M_EnterMenu (m_load);
	M_ScanSaves (!file_from_gamedir ? "qw" : com_gamedir);
}

void M_Menu_Save_f (void) {
	if (sv.state != ss_active)
		return;
	if (cl.intermission)
		return;

	M_EnterMenu (m_save);
	M_ScanSaves (com_gamedir);
}

void M_Load_Draw (void) {
	int i;
	mpic_t *p;

	p = Draw_CachePic ("gfx/p_load.lmp");
	M_DrawPic ( (320 - p->width) >> 1, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, FLASHINGARROW());
}

void M_Save_Draw (void) {
	int i;
	mpic_t *p;

	p = Draw_CachePic ("gfx/p_save.lmp");
	M_DrawPic ( (320 - p->width) >> 1, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8 * i, m_filenames[i]);

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, FLASHINGARROW());
}

void M_Load_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_singleplayer);
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			if (!loadable[load_cursor])
				return;
			m_state = m_none;
			key_dest = key_game;

			// issue the load command
			if (FS_LoadHunkFile ("spprogs.dat") && !file_from_gamedir)
				Cbuf_AddText("disconnect; gamedir qw\n");
			Cbuf_AddText (va ("load s%i\n", load_cursor) );
			return;

		case K_UPARROW:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES - 1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			if (load_cursor >= MAX_SAVEGAMES)
				load_cursor = 0;
			break;
	}
}

void M_Save_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_singleplayer);
			break;

		case K_ENTER:
			m_state = m_none;
			key_dest = key_game;
			Cbuf_AddText (va("save s%i\n", load_cursor));
			return;

		case K_UPARROW:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES-1;
			break;

		case K_DOWNARROW:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			if (load_cursor >= MAX_SAVEGAMES)
				load_cursor = 0;
			break;
	}
}

#endif

//=============================================================================
/* MULTIPLAYER MENU */

int    m_multiplayer_cursor;
#ifdef CLIENTONLY
#define    MULTIPLAYER_ITEMS    3
#else
#define    MULTIPLAYER_ITEMS    4
#endif

void M_Menu_MultiPlayer_f (void) {
	M_EnterMenu (m_multiplayer);
}

void M_MultiPlayer_Draw (void) {
	mpic_t    *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_Print (80, 40, "Server Browser");
	M_Print (80, 48, "Player Setup");
	M_Print (80, 56, "Demos");
#ifndef CLIENTONLY
	M_Print (80, 64, "New Game");
#endif

	// cursor
	M_DrawCharacter (64, 40 + m_multiplayer_cursor * 8, FLASHINGARROW());
}

void M_MultiPlayer_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
				m_multiplayer_cursor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			if (--m_multiplayer_cursor < 0)
				m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
			break;

		case K_HOME:
		case K_PGUP:
			S_LocalSound ("misc/menu1.wav");
			m_multiplayer_cursor = 0;
			break;

		case K_END:
		case K_PGDN:
			S_LocalSound ("misc/menu1.wav");
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
			break;

		case K_ENTER:
			m_entersound = true;
			switch (m_multiplayer_cursor) {
				case 0:
					M_Menu_ServerList_f ();
					break;

				case 1:
					M_Menu_Setup_f ();
					break;

				case 2:
					M_Menu_Demos_f ();
					break;

#ifndef CLIENTONLY
				case 3:
					M_Menu_GameOptions_f ();
					break;
#endif
			}
	}
}


//=============================================================================
// MULTIPLAYER MENU
// interface for menu_demo module
//=============================================================================

// entering demos menu
void M_Menu_Demos_f (void) {
	M_EnterMenu(m_demos); // switches client state
}

// key press in demos menu
void M_Demo_Draw(void){
	Menu_Demo_Draw(); // menu_demo module
}

// demos menu draw request
void M_Demo_Key (int key) {
	Menu_Demo_Key(key); // menu_demo module
}


//=============================================================================
// MP3 PLAYER MENU
//=============================================================================

#if _WIN32 || defined(__XMMS__)

#define M_MP3_CONTROL_HEADINGROW    8
#define M_MP3_CONTROL_MENUROW        (M_MP3_CONTROL_HEADINGROW + 56)
#define M_MP3_CONTROL_COL            104
#define M_MP3_CONTROL_NUMENTRIES    12
#define M_MP3_CONTROL_BARHEIGHT        (200 - 24)

static int mp3_cursor = 0;
static int last_status;

void MP3_Menu_DrawInfo(void);
void M_Menu_MP3_Playlist_f(void);

void M_MP3_Control_Draw (void) {
	char songinfo_scroll[38 + 1], *s = NULL;
	int i, scroll_index, print_time;
	float frac, elapsed, realtime;

	static char lastsonginfo[128], last_title[128];
	static float initial_time;
	static int last_length, last_elapsed, last_total, last_shuffle, last_repeat;


	M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " CONTROL")) >> 1, M_MP3_CONTROL_HEADINGROW, MP3_PLAYERNAME_ALLCAPS " CONTROL");

	M_Print (8, M_MP3_CONTROL_HEADINGROW + 16, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");


	if (!MP3_IsActive()) {
		M_PrintWhite((320 - 24 * 8) >> 1, M_MP3_CONTROL_HEADINGROW + 40, "XMMS LIBRARIES NOT FOUND");
		return;
	}

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Stopped");
	else
		M_PrintWhite(312 - 11 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Not Running");

	if (last_status == MP3_NOTRUNNING) {
		M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " is not running")) >> 1, 40, MP3_PLAYERNAME_LEADINGCAP " is not running");
		M_PrintWhite (56, 72, "Press");
		M_Print (56 + 48, 72, "ENTER");
		M_PrintWhite (56 + 48 + 48, 72, "to start " MP3_PLAYERNAME_NOCAPS);
		M_PrintWhite (56, 84, "Press");
		M_Print (56 + 48, 84, "ESC");
		M_PrintWhite (56 + 48 + 32, 84, "to exit this menu");
		M_Print (16, 116, "The variable");
		M_PrintWhite (16 + 104, 116, mp3_dir.name);
		M_Print (16 + 104 + 8 * (strlen(mp3_dir.name) + 1), 116, "needs to");
		M_Print (20, 124, "be set to the path for " MP3_PLAYERNAME_NOCAPS " first");
		return;
	}

	s = MP3_Menu_SongtTitle();
	if (!strcmp(last_title, s = MP3_Menu_SongtTitle())) {
		elapsed = 3.5 * max(realtime - initial_time - 0.75, 0);
		scroll_index = (int) elapsed;
		frac = bound(0, elapsed - scroll_index, 1);
		scroll_index = scroll_index % last_length;
	} else {
		snprintf(lastsonginfo, sizeof(lastsonginfo), "%s  ***  ", s);
		strlcpy(last_title, s, sizeof(last_title));
		last_length = strlen(lastsonginfo);
		initial_time = realtime;
		frac = scroll_index = 0;
	}

	if ((!mp3_scrolltitle.value || last_length <= 38 + 7) && mp3_scrolltitle.value != 2) {
		char name[38 + 1];
		strlcpy(name, last_title, sizeof(name));
		M_PrintWhite(max(8, (320 - (last_length - 7) * 8) >> 1), M_MP3_CONTROL_HEADINGROW + 32, name);
		initial_time = realtime;
	} else {
		for (i = 0; i < sizeof(songinfo_scroll) - 1; i++)
			songinfo_scroll[i] = lastsonginfo[(scroll_index + i) % last_length];
		songinfo_scroll[sizeof(songinfo_scroll) - 1] = 0;
		M_PrintWhite(12 -  (int) (8 * frac), M_MP3_CONTROL_HEADINGROW + 32, songinfo_scroll);
	}

	if (mp3_showtime.value) {
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, M_MP3_CONTROL_HEADINGROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW, "Play");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 8, last_status == MP3_PAUSED ? "Unpause" : "Pause");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 16, "Stop");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 24, "Next Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 32, "Prev Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 40, "Fast Forwd");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 48, "Rewind");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 56, "Volume");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 64, "Shuffle");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 72, "Repeat");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 88, "View Playlist");

	M_DrawCharacter (M_MP3_CONTROL_COL - 8, M_MP3_CONTROL_MENUROW + mp3_cursor * 8, FLASHINGARROW());

	M_DrawSlider(M_MP3_CONTROL_COL + 96, M_MP3_CONTROL_MENUROW + 56, bound(0, Media_GetVolume(), 1));

	MP3_GetToggleState(&last_shuffle, &last_repeat);
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 64, last_shuffle ? "On" : "Off");
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 72, last_repeat ? "On" : "Off");

	M_DrawCharacter (16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter (24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter (320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter (17 + 286 * ((float) last_elapsed / last_total), M_MP3_CONTROL_BARHEIGHT, 131);
}

void M_Menu_MP3_Control_Key(int key) {
	float volume;

	if (!MP3_IsActive() || last_status == MP3_NOTRUNNING) {
		switch(key) {
			case K_BACKSPACE:
				m_topmenu = m_none;
			case K_ESCAPE:
				M_LeaveMenu (m_main);
				break;
			case K_ENTER:
				if (MP3_IsActive())
					MP3_Execute_f();
				break;
		}
		return;
	}

	con_suppress = true;
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;
		case K_HOME:
		case K_PGUP:
			mp3_cursor = 0;
			break;
		case K_END:
		case K_PGDN:
			mp3_cursor = M_MP3_CONTROL_NUMENTRIES - 1;
			break;
		case K_DOWNARROW:
			if (mp3_cursor < M_MP3_CONTROL_NUMENTRIES - 1)
				mp3_cursor++;
			if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
				mp3_cursor++;
			break;
		case K_UPARROW:
			if (mp3_cursor > 0)
				mp3_cursor--;
			if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
				mp3_cursor--;
			break;
		case K_ENTER:
			switch (mp3_cursor) {
				case 0:  MP3_Play_f(); break;
				case 1:  MP3_Pause_f(); break;
				case 2:  MP3_Stop_f(); break;
				case 3:  MP3_Next_f(); break;
				case 4:  MP3_Prev_f(); break;
				case 5:  MP3_FastForward_f(); break;
				case 6:  MP3_Rewind_f(); break;
				case 7:  break;
				case 8:  MP3_ToggleShuffle_f(); break;
				case 9:  MP3_ToggleRepeat_f(); break;
				case 10: break;
				case 11: M_Menu_MP3_Playlist_f(); break;
			}
			break;
		case K_RIGHTARROW:
			switch(mp3_cursor) {
				case 7:
					volume = bound(0, Media_GetVolume(), 1);
					Media_SetVolume(volume + 0.02);
					break;
				default:
					MP3_FastForward_f();
					break;
			}
			break;
		case K_LEFTARROW:
			switch(mp3_cursor) {
				case 7:
					volume = bound(0, Media_GetVolume(), 1);
					Media_SetVolume(volume - 0.02);
					break;
				default:
					MP3_Rewind_f();
					break;
			}
			break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f();     break;
		case 'V': MP3_FadeOut_f();    break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: MP3_Rewind_f(); break;
		case KP_PGUP: MP3_FastForward_f(); break;
	}
	con_suppress = false;
}

void M_Menu_MP3_Control_f (void){
	M_EnterMenu (m_mp3_control);
}

#define PLAYLIST_MAXENTRIES        2048
#define PLAYLIST_MAXLINES        17
#define PLAYLIST_HEADING_ROW    8

#define PLAYLIST_MAXTITLE    (32 + 1)

static int playlist_size = 0;
static int playlist_cursor = 0, playlist_base = 0;

static void Center_Playlist(void) {
	int current;

	MP3_GetPlaylistInfo(&current, NULL);
	if (current >= 0 && current < playlist_size) {
		if (playlist_size - current - 1 < (PLAYLIST_MAXLINES >> 1)) {
			playlist_base = max(0, playlist_size - PLAYLIST_MAXLINES);
			playlist_cursor = current - playlist_base;
		} else {
			playlist_base = max(0, current - (PLAYLIST_MAXLINES >> 1));
			playlist_cursor = current - playlist_base;
		}
	}
}

static char *playlist_entries[PLAYLIST_MAXENTRIES];

#ifdef _WIN32

void M_Menu_MP3_Playlist_Read(void) {
	int i, count = 0, skip = 0;
	long length;
	char *playlist_buf = NULL;

	for (i = 0; i < playlist_size; i++) {
		if (playlist_entries[i]) {
			Q_free(playlist_entries[i]);
			playlist_entries[i] = NULL;
		}
	}

	playlist_base = playlist_cursor = playlist_size = 0;

	if ((length = MP3_GetPlaylist(&playlist_buf)) == -1)
		return;

	playlist_size = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, playlist_entries, PLAYLIST_MAXENTRIES, PLAYLIST_MAXTITLE);
	free(playlist_buf);
}

#else

void M_Menu_MP3_Playlist_Read(void) {
	int i;
	char *title;

	for (i = 0; i < playlist_size; i++) {
		if (playlist_entries[i]) {
			Q_free(playlist_entries[i]);
			playlist_entries[i] = NULL;
		}
	}

	playlist_base = playlist_cursor = playlist_size = 0;

	if (MP3_GetStatus() == MP3_NOTRUNNING)
		return;

	playlist_size = qxmms_remote_get_playlist_length(XMMS_SESSION);

	for (i = 0; i < PLAYLIST_MAXENTRIES && i < playlist_size; i++) {
		title = qxmms_remote_get_playlist_title(XMMS_SESSION, i);
		if (strlen(title) > PLAYLIST_MAXTITLE)
			title[PLAYLIST_MAXTITLE] = 0;
		playlist_entries[i] = Q_strdup(title);
		g_free(title);
	}
}

#endif

void M_Menu_MP3_Playlist_Draw(void) {
	int    index, print_time, i;
	char name[PLAYLIST_MAXTITLE];
	float realtime;

	static int last_status,last_elapsed, last_total, last_current;

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_NOTRUNNING) {
		M_Menu_MP3_Control_f();
		return;
	}

	M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " PLAYLIST")) >> 1, PLAYLIST_HEADING_ROW, MP3_PLAYERNAME_ALLCAPS " PLAYLIST");
	M_Print (8, PLAYLIST_HEADING_ROW + 16, "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, PLAYLIST_HEADING_ROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Stopped");

	if (mp3_showtime.value) {
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, PLAYLIST_HEADING_ROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	if (!playlist_size) {
		M_Print (92, 32, "Playlist is empty");
		return;
	}

	MP3_GetPlaylistInfo(&last_current, NULL);

	for (index = playlist_base; index < playlist_size && index < playlist_base + PLAYLIST_MAXLINES; index++) {
		char *spaces;

		if (index + 1 < 10)
			spaces = "  ";
		else if (index + 1 < 100)
			spaces = " ";
		else
			spaces = "";
		strlcpy(name, playlist_entries[index], sizeof(name));
		if (last_current != index)
			M_Print (16, PLAYLIST_HEADING_ROW + 24 + (index - playlist_base) * 8, va("%s%d %s", spaces, index + 1, name));
		else
			M_PrintWhite (16, PLAYLIST_HEADING_ROW + 24 + (index - playlist_base) * 8, va("%s%d %s", spaces, index + 1, name));
	}
	M_DrawCharacter (8, PLAYLIST_HEADING_ROW + 24 + playlist_cursor * 8, FLASHINGARROW());

	M_DrawCharacter (16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter (24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter (320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter (17 + 286 * ((float) last_elapsed / last_total), M_MP3_CONTROL_BARHEIGHT, 131);
}

void M_Menu_MP3_Playlist_Key (int k) {
	con_suppress = true;
	switch (k) {
		case K_BACKSPACE:
			m_topmenu = m_none;
		case K_ESCAPE:
			M_LeaveMenu (m_mp3_control);
			break;

		case K_UPARROW:
			if (playlist_cursor > 0)
				playlist_cursor--;
			else if (playlist_base > 0)
				playlist_base--;
			break;

		case K_DOWNARROW:
			if (playlist_cursor + playlist_base < playlist_size - 1) {
				if (playlist_cursor < PLAYLIST_MAXLINES - 1)
					playlist_cursor++;
				else
					playlist_base++;
			}
			break;

		case K_HOME:
			playlist_cursor = 0;
			playlist_base = 0;
			break;

		case K_END:
			if (playlist_size > PLAYLIST_MAXLINES) {
				playlist_cursor = PLAYLIST_MAXLINES - 1;
				playlist_base = playlist_size - playlist_cursor - 1;
			} else {
				playlist_base = 0;
				playlist_cursor = playlist_size - 1;
			}
			break;

		case K_PGUP:
			playlist_cursor -= PLAYLIST_MAXLINES - 1;
			if (playlist_cursor < 0) {
				playlist_base += playlist_cursor;
				if (playlist_base < 0)
					playlist_base = 0;
				playlist_cursor = 0;
			}
			break;

		case K_PGDN:
			playlist_cursor += PLAYLIST_MAXLINES - 1;
			if (playlist_base + playlist_cursor >= playlist_size)
				playlist_cursor = playlist_size - playlist_base - 1;
			if (playlist_cursor >= PLAYLIST_MAXLINES) {
				playlist_base += playlist_cursor - (PLAYLIST_MAXLINES - 1);
				playlist_cursor = PLAYLIST_MAXLINES - 1;
				if (playlist_base + playlist_cursor >= playlist_size)
					playlist_base = playlist_size - playlist_cursor - 1;
			}
			break;

		case K_ENTER:
			if (!playlist_size)
				break;
			Cbuf_AddText(va("mp3_playtrack %d\n", playlist_cursor + playlist_base + 1));
			break;
		case K_SPACE: M_Menu_MP3_Playlist_Read(); Center_Playlist();break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f();     break;
		case 'V': MP3_FadeOut_f();    break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: case K_LEFTARROW:  MP3_Rewind_f(); break;
		case KP_PGUP: case K_RIGHTARROW: MP3_FastForward_f(); break;
	}
	con_suppress = false;
}

void M_Menu_MP3_Playlist_f (void){
	if (!MP3_IsActive()) {
		M_Menu_MP3_Control_f();
		return;
	}

	M_Menu_MP3_Playlist_Read();
	M_EnterMenu (m_mp3_playlist);
	Center_Playlist();
}

#endif



//=============================================================================
/* GAME OPTIONS MENU */

#ifndef CLIENTONLY

typedef struct {
	char    *name;
	char    *description;
} level_t;

level_t        levels[] = {
	{"start", "Entrance"},    // 0

	{"e1m1", "Slipgate Complex"},                // 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},                // 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},            // 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},                // 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},            // 31

	{"dm1", "Place of Two Deaths"},                // 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

typedef struct {
	char    *description;
	int        firstLevel;
	int        levels;
} episode_t;

episode_t    episodes[] = {
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

extern cvar_t maxclients, maxspectators;

int    startepisode;
int    startlevel;
int _maxclients, _maxspectators;
int _deathmatch, _teamplay, _skill, _coop;
int _fraglimit, _timelimit;

void M_Menu_GameOptions_f (void) {
	M_EnterMenu (m_gameoptions);

	// 16 and 8 are not really limits --- just sane values
	// for these variables...
	_maxclients = min(16, (int)maxclients.value);
	if (_maxclients < 2) _maxclients = 8;
	_maxspectators = max(0, min((int)maxspectators.value, 8));

	_deathmatch = max (0, min((int)deathmatch.value, 5));
	_teamplay = max (0, min((int)teamplay.value, 2));
	_skill = max (0, min((int)skill.value, 3));
	_fraglimit = max (0, min((int)fraglimit.value, 100));
	_timelimit = max (0, min((int)timelimit.value, 60));
}

int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 96, 104, 120, 128};
#define    NUM_GAMEOPTIONS    9
int        gameoptions_cursor;

void M_GameOptions_Draw (void) {
	mpic_t *p;
	char *msg;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "        game type");
	if (!_deathmatch)
		M_Print (160, 56, "cooperative");
	else
		M_Print (160, 56, va("deathmatch %i", _deathmatch));

	M_Print (0, 64, "         teamplay");

	switch(_teamplay) {
		default: msg = "Off"; break;
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
	}
	M_Print (160, 64, msg);

	if (_deathmatch == 0) {
		M_Print (0, 72, "            skill");
		switch (_skill) {
			case 0:  M_Print (160, 72, "Easy"); break;
			case 1:  M_Print (160, 72, "Normal"); break;
			case 2:  M_Print (160, 72, "Hard"); break;
			default: M_Print (160, 72, "Nightmare");
		}
	} else {
		M_Print (0, 72, "        fraglimit");
		if (_fraglimit == 0)
			M_Print (160, 72, "none");
		else
			M_Print (160, 72, va("%i frags", _fraglimit));

		M_Print (0, 80, "        timelimit");
		if (_timelimit == 0)
			M_Print (160, 80, "none");
		else
			M_Print (160, 80, va("%i minutes", _timelimit));
	}
	M_Print (0, 96, "       maxclients");
	M_Print (160, 96, va("%i", _maxclients) );

	M_Print (0, 104, "       maxspect.");
	M_Print (160, 104, va("%i", _maxspectators) );

	M_Print (0, 120, "         Episode");
	M_Print (160, 120, episodes[startepisode].description);

	M_Print (0, 128, "           Level");
	M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].description);
	M_Print (160, 136, levels[episodes[startepisode].firstLevel + startlevel].name);

	// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], FLASHINGARROW());
}

void M_NetStart_Change (int dir) {
	int count;

	switch (gameoptions_cursor) {
		case 1:
			_deathmatch += dir;
			if (_deathmatch < 0) _deathmatch = 5;
			else if (_deathmatch > 5) _deathmatch = 0;
			break;

		case 2:
			_teamplay += dir;
			if (_teamplay < 0) _teamplay = 2;
			else if (_teamplay > 2) _teamplay = 0;
			break;

		case 3:
			if (_deathmatch == 0) {
				_skill += dir;
				if (_skill < 0) _skill = 3;
				else if (_skill > 3) _skill = 0;
			} else {
				_fraglimit += dir * 10;
				if (_fraglimit < 0) _fraglimit = 100;
				else if (_fraglimit > 100) _fraglimit = 0;
			}
			break;

		case 4:
			_timelimit += dir*5;
			if (_timelimit < 0) _timelimit = 60;
			else if (_timelimit > 60) _timelimit = 0;
			break;

		case 5:
			_maxclients += dir;
			if (_maxclients > 16)
				_maxclients = 2;
			else if (_maxclients < 2)
				_maxclients = 16;
			break;

		case 6:
			_maxspectators += dir;
			if (_maxspectators > 8)
				_maxspectators = 0;
			else if (_maxspectators < 0)
				_maxspectators = 8;
			break;

		case 7:
			startepisode += dir;
			count = 7;

			if (startepisode < 0)
				startepisode = count - 1;

			if (startepisode >= count)
				startepisode = 0;

			startlevel = 0;
			break;

		case 8:
			startlevel += dir;
			count = episodes[startepisode].levels;

			if (startlevel < 0)
				startlevel = count - 1;

			if (startlevel >= count)
				startlevel = 0;
			break;
	}
}

void M_GameOptions_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu (m_multiplayer);
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor--;
			if (!_deathmatch && gameoptions_cursor == 4)
				gameoptions_cursor--;
			if (gameoptions_cursor < 0)
				gameoptions_cursor = NUM_GAMEOPTIONS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor++;
			if (!_deathmatch && gameoptions_cursor == 4)
				gameoptions_cursor++;
			if (gameoptions_cursor >= NUM_GAMEOPTIONS)
				gameoptions_cursor = 0;
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor = 0;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
			break;

		case K_LEFTARROW:
			if (gameoptions_cursor == 0)
				break;
			S_LocalSound ("misc/menu3.wav");
			M_NetStart_Change (-1);
			break;

		case K_RIGHTARROW:
			if (gameoptions_cursor == 0)
				break;
			S_LocalSound ("misc/menu3.wav");
			M_NetStart_Change (1);
			break;

		case K_ENTER:
			S_LocalSound ("misc/menu2.wav");
			//        if (gameoptions_cursor == 0)
			{
				key_dest = key_game;

				// Kill the server, unless we continue playing
				// deathmatch on another level
				if (!_deathmatch || !deathmatch.value)
					Cbuf_AddText ("disconnect\n");

				if (_deathmatch == 0) {
					_coop = 1;
					_timelimit = 0;
					_fraglimit = 0;
				} else {
					_coop = 0;
				}

				Cvar_Set (&deathmatch, va("%i", _deathmatch));
				Cvar_Set (&skill, va("%i", _skill));
				Cvar_Set (&coop, va("%i", _coop));
				Cvar_Set (&fraglimit, va("%i", _fraglimit));
				Cvar_Set (&timelimit, va("%i", _timelimit));
				Cvar_Set (&teamplay, va("%i", _teamplay));
				Cvar_Set (&maxclients, va("%i", _maxclients));
				Cvar_Set (&maxspectators, va("%i", _maxspectators));

				// Cbuf_AddText ("gamedir qw\n");
				Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );
				return;
			}

			//        M_NetStart_Change (1);
			break;
	}
}
#endif    // !CLIENTONLY

//=============================================================================
/* SETUP MENU */

int        setup_cursor = 0;
int        setup_cursor_table[] = {40, 56, 80, 104, 140};

char    setup_name[16];
char    setup_team[16];
int        setup_oldtop;
int        setup_oldbottom;
int        setup_top;
int        setup_bottom;

extern cvar_t    name, team;
extern cvar_t    topcolor, bottomcolor;

#define    NUM_SETUP_CMDS    5

void M_Menu_Setup_f (void) {
	M_EnterMenu (m_setup);
	strlcpy (setup_name, name.string, sizeof(setup_name));
	strlcpy (setup_team, team.string, sizeof(setup_team));
	setup_top = setup_oldtop = (int)topcolor.value;
	setup_bottom = setup_oldbottom = (int)bottomcolor.value;
}

void M_Setup_Draw (void) {
	mpic_t    *p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintWhite (168, 40, setup_name);

	M_Print (64, 56, "Your team");
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_team);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], FLASHINGARROW());

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_name), setup_cursor_table [setup_cursor], FLASHINGCURSOR());

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_team), setup_cursor_table [setup_cursor], FLASHINGCURSOR());
}

typedef unsigned int wchar;
char wc2char (wchar wc);

void M_Setup_Key (int k, int unichar) {
	int l;

	switch (k) {
		case K_ESCAPE:
			M_LeaveMenu (m_multiplayer);
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor--;
			if (setup_cursor < 0)
				setup_cursor = NUM_SETUP_CMDS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor++;
			if (setup_cursor >= NUM_SETUP_CMDS)
				setup_cursor = 0;
			break;

		case K_HOME:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor = 0;
			break;

		case K_END:
			S_LocalSound ("misc/menu1.wav");
			setup_cursor = NUM_SETUP_CMDS-1;
			break;

		case K_LEFTARROW:
			if (setup_cursor < 2)
				return;
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top - 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom - 1;
			break;
		case K_RIGHTARROW:
			if (setup_cursor < 2)
				return;
			//forward:
			S_LocalSound ("misc/menu3.wav");
			if (setup_cursor == 2)
				setup_top = setup_top + 1;
			if (setup_cursor == 3)
				setup_bottom = setup_bottom + 1;
			break;

		case K_ENTER:
			//        if (setup_cursor == 0 || setup_cursor == 1)
			//            return;

			//        if (setup_cursor == 2 || setup_cursor == 3)
			//            goto forward;

			// setup_cursor == 4 (OK)
			Cvar_Set (&name, setup_name);
			Cvar_Set (&team, setup_team);
			Cvar_Set (&topcolor, va("%i", setup_top));
			Cvar_Set (&bottomcolor, va("%i", setup_bottom));
			m_entersound = true;
			M_Menu_MultiPlayer_f ();
			break;

		case K_BACKSPACE:
			if (setup_cursor == 0) {
				if (strlen(setup_name))
					setup_name[strlen(setup_name)-1] = 0;
			} else if (setup_cursor == 1) {
				if (strlen(setup_team))
					setup_team[strlen(setup_team)-1] = 0;
			} else {
				m_topmenu = m_none;
				M_LeaveMenu (m_multiplayer);
			}
			break;

		default:
			if (!unichar)
				break;
			if (setup_cursor == 0) {
				l = strlen(setup_name);
				if (l < 15) {
					setup_name[l+1] = 0;
					setup_name[l] = wc2char(unichar);
				}
			}
			if (setup_cursor == 1) {
				l = strlen(setup_team);
				if (l < 15) {
					setup_team[l + 1] = 0;
					setup_team[l] = wc2char(unichar);
				}
			}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

// SLIST -->

#define MENU_X 50
#define TITLE_Y 4
#define MENU_Y 21
#define STAT_Y 166

//int m_multip_cursor = 0, m_multip_mins = 0, m_multip_maxs = 16, m_multip_state;

void M_Menu_ServerList_f (void) {
	M_EnterMenu (m_slist);
	//    m_multip_state = 0;
}

void M_ServerList_Draw (void) {
	int x, y, w, h;

#ifdef GLQUAKE
	// disconnect: unscale serverbrowser menu
	if (scr_scaleMenu.value) {
		menuwidth = vid.width;
		menuheight = vid.height;
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	}
#endif

	w = min(max(sb_maxwidth.value, 320), vid.width)   - 8;
	h = min(max(sb_maxheight.value, 200), vid.height) - 8;
	x = (vid.width - w) / 2;
	y = (vid.height - h) / 2;

	Browser_Draw (x, y + con_shift.value, w, h - con_shift.value);
}

void M_ServerList_Key (key) {
	extern void Browser_Key(int key);
	Browser_Key(key);
}

// <-- SLIST

// START shaman [Quake 1.09 quit screen]
void M_Quit_Draw (void) {
	M_DrawTextBox (0, 0, 38, 23);
	M_PrintWhite (16, 12,  "  Quake version 1.09 by id Software\n\n");
	M_PrintWhite (16, 28,  "Programming        Art \n");
	M_Print (16, 36,  " John Carmack       Adrian Carmack\n");
	M_Print (16, 44,  " Michael Abrash     Kevin Cloud\n");
	M_Print (16, 52,  " John Cash          Paul Steed\n");
	M_Print (16, 60,  " Dave 'Zoid' Kirsch\n");
	M_PrintWhite (16, 68,  "Design             Biz\n");
	M_Print (16, 76,  " John Romero        Jay Wilbur\n");
	M_Print (16, 84,  " Sandy Petersen     Mike Wilson\n");
	M_Print (16, 92,  " American McGee     Donna Jackson\n");
	M_Print (16, 100,  " Tim Willits        Todd Hollenshead\n");
	M_PrintWhite (16, 108, "Support            Projects\n");
	M_Print (16, 116, " Barrett Alexander  Shawn Green\n");
	M_PrintWhite (16, 124, "Sound Effects\n");
	M_Print (16, 132, " Trent Reznor and Nine Inch Nails\n\n");
	M_PrintWhite (16, 140, "Quake is a trademark of Id Software,\n");
	M_PrintWhite (16, 148, "inc., (c)1996 Id Software, inc. All\n");
	M_PrintWhite (16, 156, "rights reserved. NIN logo is a\n");
	M_PrintWhite (16, 164, "registered trademark licensed to\n");
	M_PrintWhite (16, 172, "Nothing Interactive, Inc. All rights\n");
	M_PrintWhite (16, 180, "reserved. Press y to exit\n");
}
// END shaman [Quake 1.09 quit screen]

//=============================================================================
/* Menu Subsystem */

void M_Init (void) {

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_centerMenu);
#ifdef GLQUAKE
	Cvar_Register (&scr_scaleMenu);
#endif

	Cvar_ResetCurrentGroup();
	Browser_Init();
	Help_Init();
	Menu_Demo_Init();	// menu_demo module


	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
#endif
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_slist", M_Menu_ServerList_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
#if defined(_WIN32) || defined(__XMMS__)
	Cmd_AddCommand ("menu_mp3_control", M_Menu_MP3_Control_f);
	Cmd_AddCommand ("menu_mp3_playlist", M_Menu_MP3_Playlist_f);
#endif
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_fps", M_Menu_Fps_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}

void M_Draw (void) {
	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw) {
		scr_copyeverything = 1;

		if (SCR_NEED_CONSOLE_BACKGROUND) {
			Draw_ConsoleBackground (scr_con_current);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		} else {
			Draw_FadeScreen ();
		}

		scr_fullupdate = 0;
	} else {
		m_recursiveDraw = false;
	}

#ifdef GLQUAKE
	if (scr_scaleMenu.value) {
		menuwidth = 320;
		menuheight = min (vid.height, 240);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, menuwidth, menuheight, 0, -99999, 99999);
	} else {
		menuwidth = vid.width;
		menuheight = vid.height;
	}
#endif

	if (scr_centerMenu.value)
		m_yofs = (menuheight - 200) / 2;
	else
		m_yofs = 0;

	switch (m_state) {
		case m_none:
			break;

		case m_main:
			M_Main_Draw ();
			break;

		case m_singleplayer:
			M_SinglePlayer_Draw ();
			break;

#ifndef CLIENTONLY
		case m_load:
			M_Load_Draw ();
			break;

		case m_save:
			M_Save_Draw ();
			break;
#endif

		case m_multiplayer:
			M_MultiPlayer_Draw ();
			break;

		case m_setup:
			M_Setup_Draw ();
			break;

		case m_options:
			M_Options_Draw ();
			break;

		case m_keys:
			M_Keys_Draw ();
			break;

		case m_fps:
			M_Fps_Draw ();
			break;

		case m_video:
			M_Video_Draw ();
			break;

		case m_help:
			M_Help_Draw ();
			break;

		case m_quit:
			M_Quit_Draw ();
			break;

#ifndef CLIENTONLY
		case m_gameoptions:
			M_GameOptions_Draw ();
			break;
#endif

		case m_slist:
			M_ServerList_Draw ();
			break;
			/*
			   case m_sedit:
			   M_SEdit_Draw ();
			   break;
			 */
		case m_demos:
			M_Demo_Draw ();
			break;

#if defined(_WIN32) || defined(__XMMS__)
		case m_mp3_control:
			M_MP3_Control_Draw ();
			break;

		case m_mp3_playlist:
			M_Menu_MP3_Playlist_Draw();
			break;
#endif
	}

#ifdef GLQUAKE
	if (scr_scaleMenu.value) {
		glMatrixMode (GL_PROJECTION);
		glLoadIdentity ();
		glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	}
#endif

	if (m_entersound) {
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

void M_Keydown (int key, int unichar) {
	switch (m_state) {
		case m_none:
			return;

		case m_main:
			M_Main_Key (key);
			return;

		case m_singleplayer:
			M_SinglePlayer_Key (key);
			return;

#ifndef CLIENTONLY
		case m_load:
			M_Load_Key (key);
			return;

		case m_save:
			M_Save_Key (key);
			return;
#endif

		case m_multiplayer:
			M_MultiPlayer_Key (key);
			return;

		case m_setup:
			M_Setup_Key (key, unichar);
			return;

		case m_options:
			M_Options_Key (key);
			return;

		case m_keys:
			M_Keys_Key (key);
			return;

		case m_fps:
			M_Fps_Key (key);
			return;

		case m_video:
			M_Video_Key (key);
			return;

		case m_help:
			M_Help_Key (key);
			return;

		case m_quit:
			M_Quit_Key (key);
			return;

#ifndef CLIENTONLY
		case m_gameoptions:
			M_GameOptions_Key (key);
			return;
#endif

		case m_slist:
			M_ServerList_Key (key);
			return;
			/*
			   case m_sedit:
			   M_SEdit_Key (key);
			   break;
			 */
		case m_demos:
			M_Demo_Key (key);
			break;

#if defined(_WIN32) || defined(__XMMS__)
		case m_mp3_control:
			M_Menu_MP3_Control_Key (key);
			break;

		case m_mp3_playlist:
			M_Menu_MP3_Playlist_Key (key);
			break;
#endif
	}
}
