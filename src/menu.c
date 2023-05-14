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
*/

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif
#include "quakedef.h"
#include "gl_model.h"
#ifndef CLIENTONLY
#include "server.h"
#endif
#include "menu.h"
#include "EX_browser.h"
#include "Ctrl_Tab.h"
#include "menu_demo.h"
#include "menu_proxy.h"
#include "menu_options.h"
#include "menu_ingame.h"
#include "menu_multiplayer.h"
#include "EX_FileList.h"
#include "help.h"
#include "utils.h"
#include "qsound.h"
#include "keys.h"
#include "common_draw.h"
#include "r_matrix.h"

qbool vid_windowedmouse = true;
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);
void CL_Disconnect_f(void);

extern cvar_t con_shift, scr_menualpha;

void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_Browser_f (void);
	void M_Menu_MultiPlayer_f (void);
			void M_Menu_SEdit_f (void);
		void M_Menu_Demos_f (void);
		void M_Menu_GameOptions_f (void);
	void M_Menu_Options_f (void);
	void M_Menu_Quit_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_ServerList_Draw (void);
			void M_SEdit_Draw (void);
		void M_Demo_Draw (void);
		void M_GameOptions_Draw (void);
		void M_Proxy_Draw (void);
	void M_Options_Draw (void);
	void M_Help_Draw (void);
	void M_Quit_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayerSub_Key (int key);
		void M_ServerList_Key (int key);
			void M_SEdit_Key (int key);
		void M_Demo_Key (int key);
		qbool M_GameOptions_Key (int key);
		void M_Proxy_Key (int key);
	void M_Options_Key (int key, wchar unichar);
	void M_Help_Key (int key);
	void M_Quit_Key (int key);

void M_Menu_Help_f (void);

m_state_t m_state;

qbool    m_entersound;          // play after drawing a frame, so caching
                                // won't disrupt the sound
int            m_topmenu;       // set if a submenu was entered via a
                                // menu_* command
#define    SLIDER_RANGE    10

typedef struct menu_window_s {
	int x;
	int y;
	int w;
	int h;
} menu_window_t;

//=============================================================================
/* Support Routines */

cvar_t     scr_scaleMenu = {"scr_scaleMenu","2"};
int        menuwidth = 320;
int        menuheight = 240;

cvar_t     scr_centerMenu = {"scr_centerMenu","1"};
cvar_t     menu_ingame = {"menu_ingame", "1"};
cvar_t     menu_botmatch_gamedir = { "menu_botmatch_gamedir", "fbca" };
cvar_t     menu_botmatch_mod_old = { "menu_botmatch_mod_old", "1" };
int        m_yofs = 0;

void M_DrawCharacter (int cx, int line, int num) {
	Draw_Character (cx + ((menuwidth - 320)>>1), line + m_yofs, num);
}

void M_Print_GetPoint(int cx, int cy, int *rx, int *ry, const char *str, qbool red) {
	cx += ((menuwidth - 320)>>1);
	cy += m_yofs;
	*rx = cx;
	*ry = cy;
	if (red) {
		Draw_Alt_String(cx, cy, str, 1, false);
	}
	else {
		Draw_String(cx, cy, str);
	}
}

void M_Print (int cx, int cy, char *str) {
	int rx, ry;
	M_Print_GetPoint(cx, cy, &rx, &ry, str, true);
}

void M_PrintWhite (int cx, int cy, char *str) {
	Draw_String (cx + ((menuwidth - 320)>>1), cy + m_yofs, str);
}

// replacement of M_DrawTransPic - sometimes we need the real coordinate of the pic
static void M_DrawTransPic_GetPoint (int x, int y, int *rx, int *ry, mpic_t *pic)
{
	*rx = x + ((menuwidth - 320)>>1);
	*ry = y + m_yofs;
	Draw_TransPic (x + ((menuwidth - 320)>>1), y + m_yofs, pic);
}

void M_DrawTransPic (int x, int y, mpic_t *pic) {
	int tx, ty;
	M_DrawTransPic_GetPoint(x, y, &tx, &ty, pic);
}

void M_DrawPic (int x, int y, mpic_t *pic) {
	Draw_Pic (x + ((menuwidth - 320)>>1), y + m_yofs, pic);
}

void M_DrawTextBox (int x, int y, int width, int lines) {
	Draw_TextBox (x + ((menuwidth - 320) >> 1), y + m_yofs, width, lines);
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

qbool Key_IsLeftRightSameBind(int b);

void M_FindKeysForCommand (const char *command, int *twokeys) {
	int count, j, l;
	char *b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command) + 1;
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

void M_Unscale_Menu(void)
{
	// do not scale this menu
	if (scr_scaleMenu.value) 
	{
		menuwidth = vid.width;
		menuheight = vid.height;
		R_OrthographicProjection(0, menuwidth, menuheight, 0, -99999, 99999);
	}
}

// will apply menu scaling effect for given window region
// scr_scaleMenu 1 uses glOrtho function and we use the same algorithm in here
static void M_Window_Adjust(const menu_window_t *original, menu_window_t *scaled)
{
	double sc_x, sc_y; // scale factors

	if (scr_scaleMenu.value)
	{
		sc_x = (double) vid.width / (double) menuwidth;
		sc_y = (double) vid.height / (double) menuheight;
		scaled->x = original->x * sc_x;
		scaled->y = original->y * sc_y;
		scaled->w = original->w * sc_x;
		scaled->h = original->h * sc_y;
	}
	else
	{
		memcpy(scaled, original, sizeof(menu_window_t));
	}
}

// this function will look at window borders and current mouse cursor position
// and will change which item in the window should be selected
// we presume that all entries have same height and occupy the whole window
// 1st par: input, window position & size
// 2nd par: input, mouse position
// 3rd par: input, how many entries does the window have
// 4th par: output, newly selected entry, first entry is 0, second 1, ...
// return value: does the cursor belong to this window? yes/no
static qbool M_Mouse_Select(const menu_window_t *uw, const mouse_state_t *m, int entries, int *newentry)
{
	double entryheight;
	double nentry;
	menu_window_t rw;
	menu_window_t *w = &rw; // just a language "shortcut"
	
	M_Window_Adjust(uw, w);

	// window is invisible
	if (!(w->h > 0) || !(w->w > 0)) return false;

	// check if the pointer is inside of the window
	if (m->x < w->x || m->y < w->y || m->x > w->x + w->w || m->y > w->y + w->h)
		return false; // no, it's not

	entryheight = w->h / entries;
	nentry = (int) (m->y - w->y) / (int) entryheight;
	
	*newentry = bound(0, nentry, entries-1);

	return true;
}

//=============================================================================

void M_EnterMenu (int state) {
	if (key_dest != key_menu) {
		m_topmenu = state;
		if (state != m_proxy) {
			Con_ClearNotify ();
			// hide the console
			scr_conlines = 0;
			scr_con_current = 0;
		}
	} else {
		m_topmenu = m_none;
	}

	key_dest = key_menu;
	m_state = state;
	m_entersound = true;
}

static void M_ToggleHeadMenus(int type)
{
	m_entersound = true;

	// do not ever use ingame menu, if user doesn't wish so
	if (!menu_ingame.integer) {
		type = m_main;
	}

	if (key_dest == key_menu) {
		if (m_state != type) {
			M_EnterMenu(type);
			return;
		}
		key_dest = key_game;
		m_state = m_none;
	} else {
		M_EnterMenu(type);
	}
}

void M_ToggleMenu_f (void) {
	if (cls.state == ca_active) {
		M_ToggleHeadMenus(m_ingame);
	}
	else M_ToggleHeadMenus(m_main);
}

void M_EnterProxyMenu (void) {
	M_EnterMenu(m_proxy);
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

void M_ToggleProxyMenu_f (void) {
	// this is what user has bound to a key - that means 
	// when in proxy menu -> turn it off; and vice versa
	m_entersound = true;

	if ((key_dest == key_menu) && (m_state == m_proxy)) {
		M_LeaveMenus();
		Menu_Proxy_Toggle();
	} else {
		M_EnterMenu (m_proxy);
		Menu_Proxy_Toggle();
	}
}

//=============================================================================
/* MAIN MENU */

int    m_main_cursor;
static qbool	newmainmenu = false;
menu_window_t m_main_window;

void M_Menu_Main_f (void) {
	M_EnterMenu (m_main);
}

#define BIGLETTER_WIDTH	64
#define BIGLETTER_HEIGHT	64
#define BIGMENU_LEFT				72
#define BIGMENU_TOP					32
#define BIGMENU_ITEMS_SCALE			0.3
#define	BIGMENU_LETTER_SPACING		-2
#define BIGMENU_VERTICAL_PADDING	2

typedef struct bigmenu_items_s {
	const char *label;
	void (* enter_handler) (void);
} bigmenu_items_t;

#define BIGMENU_ITEMS_COUNT(x) (sizeof(x) / sizeof(bigmenu_items_t))

bigmenu_items_t mainmenu_items[] = {
	{"Single Player", M_Menu_SinglePlayer_f},
	{"Multiplayer", M_Menu_MultiPlayer_f},
	{"Options", M_Menu_Options_f},
	{"Demos", M_Menu_Demos_f},
	{"Help", M_Menu_Help_f},
	{"Quit", M_Menu_Quit_f}
};

#define    MAIN_ITEMS    (newmainmenu ? BIGMENU_ITEMS_COUNT(mainmenu_items) : 5)

// mcharset must be supported in this point
static void M_BigMenu_DrawItems(bigmenu_items_t *menuitems, const unsigned int items, int left_corner, int top_corner, int *width, int *height)
{
	int i;
	int mheight = 0;
	int mwidth = 0;
	int x = left_corner;
	int y = top_corner;

	for (i = 0; i < items; i++) {
		int thiswidth = strlen(menuitems[i].label)*BIGMENU_ITEMS_SCALE*BIGLETTER_WIDTH;
		mheight += BIGMENU_ITEMS_SCALE*BIGLETTER_HEIGHT + BIGMENU_VERTICAL_PADDING;
		mwidth = max(mwidth, thiswidth);
		Draw_BigString(x, y, menuitems[i].label, NULL, 0, 
			BIGMENU_ITEMS_SCALE, 1, BIGMENU_LETTER_SPACING);
		y += BIGMENU_ITEMS_SCALE*BIGLETTER_HEIGHT + BIGMENU_VERTICAL_PADDING;
	}

	*width = mwidth;
	*height = mheight;
}

void M_Main_Draw (void) {
	int f = (int) (curtime * 10) % 6;
	mpic_t *p;
	int itemheight;

	M_DrawTransPic (16, BIGMENU_TOP, Draw_CachePic (CACHEPIC_QPLAQUE) );

	// the Main Manu heading
	p = Draw_CachePic (CACHEPIC_TTL_MAIN);
	M_DrawPic ( (320-p->width)/2, 4, p);

	// Main Menu items
	if (Draw_BigFontAvailable()) {
		newmainmenu = true;
		m_main_window.x = BIGMENU_LEFT + (menuwidth - 320)/2;
		m_main_window.y = BIGMENU_TOP + m_yofs;
		M_BigMenu_DrawItems(mainmenu_items, BIGMENU_ITEMS_COUNT(mainmenu_items), m_main_window.x, m_main_window.y,
						 &m_main_window.w, &m_main_window.h);
		itemheight = m_main_window.h / BIGMENU_ITEMS_COUNT(mainmenu_items);
	}
	else {
		newmainmenu = false;
		p = Draw_CachePic (CACHEPIC_MAINMENU);
		m_main_window.w = p->width;
		m_main_window.h = p->height;
		M_DrawTransPic_GetPoint (72, 32, &m_main_window.x, &m_main_window.y, p);
		
		// main menu specific correction, mainmenu.lmp|png have some useless extra space at the bottom
		// that makes the mouse pointer position calculation imperfect
		m_main_window.h *= 0.9;

		itemheight = 20;
	}	

	M_DrawTransPic (54, BIGMENU_TOP + m_main_cursor * itemheight,
		Draw_CachePic(CACHEPIC_MENUDOT1 + f)
	);
}

static void M_Main_Enter(const unsigned int entry)
{
	if (newmainmenu) {
		mainmenu_items[entry].enter_handler();
	}
	else {
		switch (entry) {
		case 0: M_Menu_SinglePlayer_f (); break;
		case 1:	M_Menu_MultiPlayer_f (); break;
		case 2: M_Menu_Options_f (); break;
		case 4: M_Menu_Quit_f (); break;
		}
	}
}

void M_Main_Key (int key) {
	switch (key) {
	case K_ESCAPE:
	case K_MOUSE2:
		if (cls.state < ca_active)
			key_dest = key_console;
		else 
			key_dest = key_game;
		m_state = m_none;
		break;

	case '`':
	case '~':
		key_dest = key_console;
		m_state = m_none;
		break;

	case K_UPARROW:
	case K_MWHEELUP:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_DOWNARROW:
	case K_MWHEELDOWN:
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
	case K_MOUSE1:
		m_entersound = true;
		M_Main_Enter((unsigned) m_main_cursor);
	}
}

static qbool M_Main_Mouse_Event(const mouse_state_t* ms)
{
	M_Mouse_Select(&m_main_window, ms, MAIN_ITEMS, &m_main_cursor);
    
    if (ms->button_up == 1) M_Main_Key(K_MOUSE1);
    if (ms->button_up == 2) M_Main_Key(K_MOUSE2);

    return true;
}

//=============================================================================
/* OPTIONS MENU */

void M_Menu_Options_f (void) {
	M_EnterMenu (m_options);
}

void M_Options_Key (int key, wchar unichar) {
	Menu_Options_Key(key, unichar); // menu_options module
}

void M_Options_Draw (void) {
	Menu_Options_Draw();	// menu_options module
}

//=============================================================================
/* PROXY MENU */

// key press in demos menu
void M_Proxy_Key (int key) {
	int togglekeys[2];

	// the menu_proxy module doesn't know which key user has bound to toggleproxymenu action
	M_FindKeysForCommand("toggleproxymenu", togglekeys);
	if ((key == togglekeys[0]) || (key == togglekeys[1])) {
		Menu_Proxy_Toggle(); 
		M_LeaveMenus();
		return;
	}

	// ppl are used to access console even when in qizmo menu
	M_FindKeysForCommand("toggleconsole", togglekeys);
	if ((key == togglekeys[0]) || (key == togglekeys[1])) {
		Con_ToggleConsole_f();
		return;
	}

	Menu_Proxy_Key(key); // menu_proxy module
}


//=============================================================================
/* HELP MENU */

int        help_page;
#define    NUM_HELP_PAGES    6

void M_Menu_Help_f (void) {
	M_EnterMenu (m_help);
}

void M_Help_Draw (void) {
	int x, y, w, h;

	M_Unscale_Menu();

    // this will add top, left and bottom padding
    // right padding is not added because it causes annoying scrollbar behaviour
    // when mouse gets off the scrollbar to the right side of it
	w = vid.width - OPTPADDING; // here used to be a limit to 512x... size
	h = vid.height - OPTPADDING*2;
	x = OPTPADDING;
	y = OPTPADDING;

	Menu_Help_Draw (x, y, w, h);
}

//=============================================================================
/* QUIT MENU */

int        msgNumber;
int        m_quit_prevstate;
qbool    wasInMenus;

void M_Menu_Quit_f (void) {
	extern cvar_t cl_confirmquit;

	if (!cl_confirmquit.integer) Host_Quit();

	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	m_quit_prevstate = m_state;
	msgNumber = rand()&7;
	M_EnterMenu (m_quit);
}

void M_Quit_Key (int key) {
	switch (key) {
		case K_MOUSE2:
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

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_MOUSE1:
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

void M_Quit_Draw (void) { // Quit screen text.
	M_DrawTextBox (0, 0, 38, 26);
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
	M_PrintWhite (16, 148, "Quake is a trademark of Id Software,\n");
	M_PrintWhite (16, 156, "inc., (c)1996 Id Software, inc. All\n");
	M_PrintWhite (16, 164, "rights reserved. NIN logo is a\n");
	M_PrintWhite (16, 172, "registered trademark licensed to\n");
	M_PrintWhite (16, 180, "Nothing Interactive, Inc. All rights\n");
	M_PrintWhite (16, 188, "reserved.\n\n");
	M_Print (16, 204, "          Press y to exit\n");
}

//=============================================================================
/* SINGLE PLAYER MENU */

#ifndef CLIENTONLY

#define    SINGLEPLAYER_ITEMS    3
int    m_singleplayer_cursor;
qbool m_singleplayer_confirm;
qbool m_singleplayer_notavail;
menu_window_t m_singleplayer_window;
static qbool m_singleplayer_big = false;
static bigmenu_items_t m_singleplayer_items[] = {
	{"New Game", NULL},
	{"Load", NULL}, 
	{"Save", NULL}
};

extern    cvar_t    maxclients;

void M_Menu_SinglePlayer_f (void) {
	M_EnterMenu (m_singleplayer);
	m_singleplayer_confirm = false;
	m_singleplayer_notavail = false;
}

void M_SinglePlayer_Draw (void) {
	int f = (int)(curtime * 10)%6;
	mpic_t *p;
	int itemheight;

#ifndef WITH_NQPROGS
	if (m_singleplayer_notavail) {
		p = Draw_CachePic ("gfx/ttl_sgl.lmp");
		M_DrawPic ( (320-p->width)/2, 4, p);
		M_DrawTextBox (60, 10*8, 24, 4);
		M_PrintWhite (80, 12*8, " Cannot start a game");
		M_PrintWhite (80, 13*8, "spprogs.dat not found");
		return;
	}
#endif

	if (m_singleplayer_confirm) {
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawTransPic (16, BIGMENU_TOP, Draw_CachePic (CACHEPIC_QPLAQUE) );
	p = Draw_CachePic (CACHEPIC_TTL_SGL);
	M_DrawPic ( (320-p->width)/2, 4, p);

	if (Draw_BigFontAvailable()) {
		m_singleplayer_big = true;
		m_singleplayer_window.x = BIGMENU_LEFT + ((menuwidth - 320)>>1);
		m_singleplayer_window.y = BIGMENU_TOP + m_yofs;
		M_BigMenu_DrawItems(m_singleplayer_items, BIGMENU_ITEMS_COUNT(m_singleplayer_items),
			m_singleplayer_window.x, m_singleplayer_window.y, &m_singleplayer_window.w,
			&m_singleplayer_window.h);
		itemheight = m_singleplayer_window.h / BIGMENU_ITEMS_COUNT(m_singleplayer_items);
	}
	else {
		m_singleplayer_big = false;
		p = Draw_CachePic (CACHEPIC_SP_MENU);
		m_singleplayer_window.w = p->width;
		m_singleplayer_window.h = p->height;
		M_DrawTransPic_GetPoint(72, 32, &m_singleplayer_window.x, &m_singleplayer_window.y, p);
		itemheight = 20;
	}

	M_DrawTransPic (54, BIGMENU_TOP + m_singleplayer_cursor * itemheight,
		Draw_CachePic(CACHEPIC_MENUDOT1 + f)
	);
}

#ifndef WITH_NQPROGS
static void CheckSPGame (void) {
	vfsfile_t *v;
	if ((v = FS_OpenVFS("spprogs.dat", "rb", FS_ANY))) {
		VFS_CLOSE(v);
		m_singleplayer_notavail = false;
	} else {
		m_singleplayer_notavail = true;
	}
}
#endif	// !WITH_NQPROGS

static void StartNewGame(void)
{
	extern cvar_t sv_progtype;

	key_dest = key_game;
	m_state = m_none;
	Cvar_Set(&maxclients, "1");
	Cvar_Set(&teamplay, "0");
	Cvar_Set(&deathmatch, "0");
	Cvar_Set(&coop, "0");

	Cvar_Set(&sv_progsname, "spprogs"); // force progsname
#ifdef USE_PR2
	Cvar_Set(&sv_progtype, "0"); // force .dat
#endif

	if (com_serveractive) {
		Cbuf_AddText("disconnect\n");
	}

	Cbuf_AddText("map start\n");
}

void M_SinglePlayer_Key (int key) {
#ifndef WITH_NQPROGS
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
#endif

	if (m_singleplayer_confirm) {
		if (key == K_ESCAPE || key == 'n' || key == K_MOUSE2) {
			m_singleplayer_confirm = false;
			m_entersound = true;
		} else if (key == 'y' || key == K_ENTER || key == K_MOUSE1) {
			StartNewGame ();
		}
		return;
	}

	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_MOUSE2:
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
			S_LocalSound ("misc/menu1.wav");
			if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
				m_singleplayer_cursor = 0;
			break;

		case K_UPARROW:
		case K_MWHEELUP:
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
		case K_MOUSE1:
			switch (m_singleplayer_cursor) {
				case 0:
#ifndef WITH_NQPROGS
					CheckSPGame ();
					if (m_singleplayer_notavail) {
						m_entersound = true;
						return;
					}
#endif
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

qbool M_SinglePlayer_Mouse_Event(const mouse_state_t* ms)
{
	M_Mouse_Select(&m_singleplayer_window, ms, SINGLEPLAYER_ITEMS, &m_singleplayer_cursor);

    if (ms->button_up == 1) M_SinglePlayer_Key(K_MOUSE1);
    if (ms->button_up == 2) M_SinglePlayer_Key(K_MOUSE2);

    return true;
}

#else    // !CLIENTONLY

void M_Menu_SinglePlayer_f (void) {
	M_EnterMenu (m_singleplayer);
}

void M_SinglePlayer_Draw (void) {
	mpic_t *p;

	M_DrawTransPic (16, 4, Draw_CachePic (CACHEPIC_QPLAQUE) );
	p = Draw_CachePic (CACHEPIC_TTL_SGL);
	M_DrawPic ( (320-p->width)/2, 4, p);
	//    M_DrawTransPic (72, 32, Draw_CachePic (CACHEPIC_SP_MENU) );

	M_DrawTextBox (60, 10*8, 23, 4);
	M_PrintWhite (88, 12*8, "This client is for");
	M_PrintWhite (88, 13*8, "Internet play only");
}

void M_SinglePlayer_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_ESCAPE:
		case K_ENTER:
			M_LeaveMenu (m_main);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;
	}
}
#endif    // CLIENTONLY


//=============================================================================
/* LOAD/SAVE MENU */

#ifndef CLIENTONLY

#define    MAX_SAVEGAMES        12

const char* save_filenames[] = {
	"s0.sav", "s1.sav", "s2.sav", "s3.sav", "s4.sav", "s5.sav", "s6.sav", "s7.sav", "s8.sav", "s9.sav", "s10.sav", "s11.sav"
};
#ifdef C_ASSERT
C_ASSERT(sizeof(save_filenames) / sizeof(save_filenames[0]) == MAX_SAVEGAMES);
#endif

int        load_cursor;        // 0 < load_cursor < MAX_SAVEGAMES
char    m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
int        loadable[MAX_SAVEGAMES];
menu_window_t load_window, save_window;

void M_ScanSaves(char* sp_gamedir)
{
	int i, j;
	char name[MAX_OSPATH];
	vfsfile_t* f;

	for (i = 0; i < MAX_SAVEGAMES; i++) {
		loadable[i] = false;
		strlcpy(m_filenames[i], "--- UNUSED SLOT ---", SAVEGAME_COMMENT_LENGTH + 1);

		FS_SaveGameDirectory(name, sizeof(name));
		strlcat(name, save_filenames[i], sizeof(name));
		if (!(f = FS_OpenVFS(name, "rb", FS_NONE_OS))) {
			continue;
		}
		VFS_GETS(f, name, sizeof(name));
		VFS_GETS(f, name, sizeof(name));
		strlcpy(m_filenames[i], name, sizeof(m_filenames[i]));

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++) {
			if (m_filenames[i][j] == '_') {
				m_filenames[i][j] = ' ';
			}
		}
		loadable[i] = true;
		VFS_CLOSE(f);
	}
}

void M_Menu_Load_f (void) {
	vfsfile_t *f;

	if (!(f = FS_OpenVFS("spprogs.dat", "rb", FS_ANY)))
		return;

	M_EnterMenu (m_load);
	// VFS-FIXME: file_from_gamedir is not set in FS_OpenVFS
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
	int lx = 0, ly = 0;	// lower bounds of the window

	p = Draw_CachePic (CACHEPIC_P_LOAD);
	M_DrawPic ( (320 - p->width) >> 1, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		if (i == 0)
			M_Print_GetPoint (16, 32 + 8*i, &load_window.x, &load_window.y, m_filenames[i], load_cursor == 0);
		else 
			M_Print_GetPoint (16, 32 + 8*i, &lx, &ly, m_filenames[i], load_cursor == i);
	}

	load_window.w = SAVEGAME_COMMENT_LENGTH*8; // presume 8 pixels for each letter
	load_window.h = ly - load_window.y + 8;

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, FLASHINGARROW());
}

void M_Save_Draw (void) {
	int i;
	mpic_t *p;
	int lx = 0, ly = 0;	// lower bounds of the window

	p = Draw_CachePic (CACHEPIC_P_SAVE);
	M_DrawPic ( (320 - p->width) >> 1, 4, p);

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		if (i == 0)
			M_Print_GetPoint (16, 32 + 8 * i, &save_window.x, &save_window.y, m_filenames[i], load_cursor == 0);
		else
			M_Print_GetPoint (16, 32 + 8 * i, &lx, &ly, m_filenames[i], load_cursor == i);
	}

	save_window.w = SAVEGAME_COMMENT_LENGTH*8; // presume 8 pixels for each letter
	save_window.h = ly - save_window.y + 8;

	// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, FLASHINGARROW());
}

void M_Load_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_MOUSE2:
		case K_ESCAPE:
			M_LeaveMenu (m_singleplayer);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_ENTER:
		case K_MOUSE1:
			S_LocalSound ("misc/menu2.wav");
			if (!loadable[load_cursor])
				return;
			m_state = m_none;
			key_dest = key_game;

			// issue the load command
			Cbuf_AddText(va("load s%i\n", load_cursor));
			return;

		case K_UPARROW:
		case K_MWHEELUP:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES - 1;
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
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
		case K_MOUSE2:
		case K_ESCAPE:
			M_LeaveMenu (m_singleplayer);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_ENTER:
		case K_MOUSE1:
			m_state = m_none;
			key_dest = key_game;
			Cbuf_AddText (va("save s%i\n", load_cursor));
			return;

		case K_UPARROW:
		case K_MWHEELUP:
		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor--;
			if (load_cursor < 0)
				load_cursor = MAX_SAVEGAMES-1;
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");
			load_cursor++;
			if (load_cursor >= MAX_SAVEGAMES)
				load_cursor = 0;
			break;
	}
}

qbool M_Save_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&save_window, ms, MAX_SAVEGAMES, &load_cursor);

    if (ms->button_up == 1) M_Save_Key(K_MOUSE1);
    if (ms->button_up == 2) M_Save_Key(K_MOUSE2);

	return true;
}

qbool M_Load_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&load_window, ms, MAX_SAVEGAMES, &load_cursor);

    if (ms->button_up == 1) M_Load_Key(K_MOUSE1);
    if (ms->button_up == 2) M_Load_Key(K_MOUSE2);

    return true;
}

#endif


// ================================
// Multiplayer submenu
// used only if mcharset is not available making demo player menu inaccesible from mainmenu

int    m_multiplayer_cursor;
#define    MULTIPLAYER_ITEMS    2
menu_window_t m_multiplayer_window;

void M_MultiPlayerSub_Draw (void) {
	mpic_t    *p;
	int lx, ly;

	M_DrawTransPic (16, 4, Draw_CachePic (CACHEPIC_QPLAQUE) );
	p = Draw_CachePic (CACHEPIC_P_MULTI);
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_Print_GetPoint (80, 40, &m_multiplayer_window.x, &m_multiplayer_window.y, "Join Game", m_multiplayer_cursor == 0);
	m_multiplayer_window.h = 8;
	M_Print_GetPoint (80, 48, &lx, &ly, "Demos", m_multiplayer_cursor == MULTIPLAYER_ITEMS - 1);
	m_multiplayer_window.h += 8;
	m_multiplayer_window.w = 20 * 8; // presume 20 letters long word and 8 pixels for a letter
	
	// cursor
	M_DrawCharacter (64, 40 + m_multiplayer_cursor * 8, FLASHINGARROW());
}

void M_MultiPlayerSub_Key (int key) {
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;    // intentional fallthrough
		case K_MOUSE2:
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
			S_LocalSound ("misc/menu1.wav");
			if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
				m_multiplayer_cursor = 0;
			break;

		case K_UPARROW:
		case K_MWHEELUP:
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
		case K_MOUSE1:
			m_entersound = true;
			switch (m_multiplayer_cursor) {
				case 0:
					M_EnterMenu(m_multiplayer);
					break;
				case 1:
					M_Menu_Demos_f ();
					break;
			}
	}
}

qbool M_MultiPlayerSub_Mouse_Event(const mouse_state_t *ms)
{
	M_Mouse_Select(&m_multiplayer_window, ms, MULTIPLAYER_ITEMS, &m_multiplayer_cursor);

    if (ms->button_up == 1) M_MultiPlayerSub_Key(K_MOUSE1);
    if (ms->button_up == 2) M_MultiPlayerSub_Key(K_MOUSE2);
    
    return true;
}

void M_Menu_Browser_f (void) {
	M_EnterMenu(m_multiplayer);
}

void M_Menu_MultiPlayer_f (void)
{
	if (Draw_BigFontAvailable()) {
		M_EnterMenu(m_multiplayer);
	}
	else {
		// demos entry is not accessible with old main menu
		// so we need to create a submenu in the multiplayer menu
		M_EnterMenu(m_multiplayer_submenu);
	}
}

void M_Menu_Demos_f (void)
{
	M_EnterMenu(m_demos); // switches client state
}

//=============================================================================
/* Menu Subsystem */

void M_Init (void) {
	extern cvar_t menu_marked_bgcolor;
	extern cvar_t menu_marked_fade;

	Cvar_SetCurrentGroup(CVAR_GROUP_MENU);
	Cvar_Register(&scr_centerMenu);
	Cvar_Register(&menu_ingame);
	Cvar_Register(&scr_scaleMenu);
	Cvar_Register(&menu_marked_fade);
	Cvar_Register(&menu_botmatch_gamedir);
	Cvar_Register(&menu_botmatch_mod_old);

	Cvar_Register(&menu_marked_bgcolor);
	Browser_Init();
	Cvar_ResetCurrentGroup();
	Menu_Help_Init();	// help_files module
	Menu_Demo_Init();	// menu_demo module
	Menu_Options_Init(); // menu_options module
	Menu_Ingame_Init();
	Menu_MultiPlayer_Init(); // menu_multiplayer.h

	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);
	Cmd_AddCommand ("toggleproxymenu", M_ToggleProxyMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
#endif
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_slist", M_Menu_Browser_f);
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}

void M_Shutdown(void)
{
	Menu_Help_Shutdown();
	Menu_Demo_Shutdown();
	Menu_Options_Shutdown();
	Menu_Ingame_Shutdown();
	Menu_MultiPlayer_Shutdown();
}

void M_Draw(void)
{
	if (m_state == m_none || key_dest != key_menu || m_state == m_proxy) {
		return;
	}

	scr_copyeverything = 1;

	if (SCR_NEED_CONSOLE_BACKGROUND) {
		Draw_ConsoleBackground(scr_con_current);
	}
	else {
		// if you don't like fade in ingame menu, uncomment this
		// if (m_state != m_ingame && m_state != m_democtrl)
		Draw_FadeScreen(scr_menualpha.value);
	}

	scr_fullupdate = 0;

	if (scr_scaleMenu.value) {
		if (vid.aspect > 1.0) {
			menuheight = bound(240, (int)((vid.height / scr_scaleMenu.value) + 0.5f), 960);
			menuwidth = (int)((menuheight * vid.aspect) + 0.5f);
		}
		else {
			menuwidth = bound(320, (int)((vid.width / scr_scaleMenu.value) + 0.5f), 960);
			menuheight = (int)((menuwidth / vid.aspect) + 0.5f);
		}

		R_OrthographicProjection(0, menuwidth, menuheight, 0, -99999, 99999);
	}
	else {
		menuwidth = vid.width;
		menuheight = vid.height;
	}

	if (scr_centerMenu.value) {
		m_yofs = (menuheight - 200) / 2;
	}
	else {
		m_yofs = 0;
	}

	switch (m_state) {
		case m_none: break;
		case m_main:			M_Main_Draw(); break;
		case m_singleplayer:	M_SinglePlayer_Draw(); break;
#ifndef CLIENTONLY
		case m_load:			M_Load_Draw(); break;
		case m_save:			M_Save_Draw(); break;
#else
			// keeps gcc happy
		case m_load:
		case m_save:
			break;
#endif
		case m_multiplayer:		Menu_MultiPlayer_Draw(); break;
		case m_multiplayer_submenu: M_MultiPlayerSub_Draw(); break;
		case m_options:			M_Options_Draw(); break;
		case m_proxy:			Menu_Proxy_Draw(); break;
		case m_ingame:			M_Ingame_Draw(); break;
		case m_help:			M_Help_Draw(); break;
		case m_quit:			M_Quit_Draw(); break;
		case m_demos:			Menu_Demo_Draw(); break;
	}

	if (scr_scaleMenu.value) {
		R_OrthographicProjection(0, vid.width, vid.height, 0, -99999, 99999);
	}

	if (m_entersound) {
		S_LocalSound("misc/menu2.wav");
		m_entersound = false;
	}
}

// Return true if the system should execute the key, false if we want it to pass to M_Keydown
qbool Menu_ExecuteKey (int key) {
	// This always turns into /togglemenu anyway
	if (key == K_ESCAPE) {
		return true;
	}

	// Capture all keypresses when binding
	if (Menu_Options_IsBindingKey ()) {
		return false;
	}

	// Capture F1 in menus that have help boxes
	if (key == K_F1 && m_state != m_ingame) {
		return false;
	}

	// Other function keys should execute
	if (key >= K_F1 && key <= K_F12)
		return true;

	// Capture everything else
	return false;
}

void M_Keydown (int key, wchar unichar) {
	switch (m_state) {
		case m_none: return;
		case m_main:			M_Main_Key(key); return;
		case m_singleplayer:	M_SinglePlayer_Key(key); return;
#ifndef CLIENTONLY
		case m_load:			M_Load_Key(key); return;
		case m_save:			M_Save_Key(key); return;
#else
		case m_load:
		case m_save:
			break;
#endif
		case m_multiplayer:		Menu_MultiPlayer_Key(key, unichar); return;
		case m_multiplayer_submenu: M_MultiPlayerSub_Key(key); return;
		case m_options: 		M_Options_Key(key, unichar); return;
		case m_proxy:			M_Proxy_Key(key); return;
		case m_ingame:			M_Ingame_Key(key); return;
		case m_help:			Menu_Help_Key(key, unichar); return;
		case m_quit:			M_Quit_Key(key); return;
		case m_demos:			Menu_Demo_Key(key, unichar); break;
	}
}

qbool Menu_Mouse_Event(const mouse_state_t* ms)
{
	if (ms->button_down == K_MWHEELDOWN || ms->button_up == K_MWHEELDOWN ||
		ms->button_down == K_MWHEELUP   || ms->button_up == K_MWHEELUP)
	{
		// menus do not handle this type of mouse wheel event, they accept it as a key event	
		return false;
	}

	// send the mouse state to appropriate modules here
    // functions should report if they handled the event or not
    switch (m_state) {
	case m_main:			return M_Main_Mouse_Event(ms);
#ifndef CLIENTONLY
	case m_singleplayer:	return M_SinglePlayer_Mouse_Event(ms);
#endif
	case m_multiplayer:		return Menu_MultiPlayer_Mouse_Event(ms);
	case m_multiplayer_submenu: return M_MultiPlayerSub_Mouse_Event(ms);
#ifndef CLIENTONLY
	case m_load:			return M_Load_Mouse_Event(ms);
	case m_save:			return M_Save_Mouse_Event(ms);
#endif
	case m_options:			return Menu_Options_Mouse_Event(ms);
	case m_demos:			return Menu_Demo_Mouse_Event(ms);
	case m_ingame:			return Menu_Ingame_Mouse_Event(ms);
	case m_help:			return Menu_Help_Mouse_Event(ms);
	case m_none: default:	return false;
	}
}
