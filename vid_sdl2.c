/*
   ===========================================================================
   Copyright (C) 1999-2005 Id Software, Inc.

   This file is part of Quake III Arena source code.

   Quake III Arena source code is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the License,
   or (at your option) any later version.

   Quake III Arena source code is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Foobar; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
   ===========================================================================

 */

#include "quakedef.h"

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef __linux__
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef _WIN32
#include <windows.h>

void Sys_ActiveAppChanged (void);
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include "in_osx.h"
#else
#include <GL/gl.h>
#endif

#include "ezquake-icon.c"
#include "keys.h"
#include "tr_types.h"
#include "input.h"
#include "rulesets.h"
#include "utils.h"
#include "gl_model.h"
#include "gl_local.h"
#include "textencoding.h"

#define	WINDOW_CLASS_NAME	"ezQuake"

/* FIXME: This should be in a header file and it probably shouldn't be called TP_
 *        since there are a lot of triggers that has nothing to do with teamplay.
 *        Should probably split them
 */
extern void TP_ExecTrigger(const char *);

static void in_raw_callback(cvar_t *var, char *value, qbool *cancel);
static void in_grab_windowed_mouse_callback(cvar_t *var, char *value, qbool *cancel);
static void conres_changed_callback (cvar_t *var, char *string, qbool *cancel);
static void GrabMouse(qbool grab, qbool raw);
static void GfxInfo_f(void);
static void HandleEvents(void);
static void VID_UpdateConRes(void);
void IN_Restart_f(void);

static SDL_Window       *sdl_window;
static SDL_GLContext    sdl_context;

glconfig_t glConfig;
qbool vid_hwgamma_enabled = false;
static qbool mouse_active = false;
qbool mouseinitialized = false; // unfortunately non static, lame...
int mx, my;
static int old_x = 0, old_y = 0;
extern double cursor_x, cursor_y;

qbool ActiveApp = true;
qbool Minimized = false;

double vid_vsync_lag;
double vid_last_swap_time;

static SDL_DisplayMode *modelist;
static int modelist_count;

#ifdef __linux__
static unsigned short sysramps[768];
static qbool use_linux_gamma_workaround;
#endif

qbool vid_initialized = false;

static int last_working_width;
static int last_working_height;
static int last_working_hz;
static int last_working_display;
static qbool last_working_values = false;

//
// OS dependent cvar defaults
//
#ifdef __APPLE__
	#define CVAR_DEF1 "0"
#else
	#define CVAR_DEF1 "1"
#endif

#if defined(__linux__) || defined(__FreeBSD__)
	#define CVAR_DEF2 "0"
#else
	#define CVAR_DEF2 "1"
#endif

//
// cvars
//
extern cvar_t sys_inactivesleep;

// latched variables that can only change over a restart
cvar_t r_colorbits                = {"vid_colorbits",              "0",       CVAR_LATCH };
cvar_t r_24bit_depth              = {"vid_24bit_depth",            "1",       CVAR_LATCH };
cvar_t r_stereo                   = {"vid_stereo",                 "0",       CVAR_LATCH };
cvar_t r_fullscreen               = {"vid_fullscreen",             "1",       CVAR_LATCH };
cvar_t r_displayRefresh           = {"vid_displayfrequency",       "0",       CVAR_LATCH | CVAR_AUTO };
cvar_t vid_displayNumber          = {"vid_displaynumber",          "0",       CVAR_LATCH | CVAR_AUTO };
cvar_t vid_usedesktopres          = {"vid_usedesktopres",          "1",       CVAR_LATCH | CVAR_AUTO };
cvar_t vid_win_borderless         = {"vid_win_borderless",         "0",       CVAR_LATCH };
cvar_t vid_width                  = {"vid_width",                  "0",       CVAR_LATCH | CVAR_AUTO };
cvar_t vid_height                 = {"vid_height",                 "0",       CVAR_LATCH | CVAR_AUTO };
cvar_t vid_win_width              = {"vid_win_width",              "640",     CVAR_LATCH };
cvar_t vid_win_height             = {"vid_win_height",             "480",     CVAR_LATCH };
cvar_t vid_hwgammacontrol         = {"vid_hwgammacontrol",         "2",       CVAR_LATCH };
cvar_t vid_minimize_on_focus_loss = {"vid_minimize_on_focus_loss", CVAR_DEF1, CVAR_LATCH };
// TODO: Move the in_* cvars
cvar_t in_raw                     = {"in_raw",                     "1",       CVAR_ARCHIVE | CVAR_SILENT, in_raw_callback};
cvar_t in_grab_windowed_mouse     = {"in_grab_windowed_mouse",     "1",       CVAR_ARCHIVE | CVAR_SILENT, in_grab_windowed_mouse_callback};
cvar_t vid_grab_keyboard          = {"vid_grab_keyboard",          CVAR_DEF2, CVAR_LATCH  }; /* Needs vid_restart thus vid_.... */

#ifdef __linux__
cvar_t vid_gamma_workaround       = {"vid_gamma_workaround",       "1",       CVAR_LATCH  };
#endif

cvar_t in_release_mouse_modes     = {"in_release_mouse_modes",     "2",       CVAR_SILENT };
cvar_t vid_vsync_lag_fix          = {"vid_vsync_lag_fix",          "0"                    };
cvar_t vid_vsync_lag_tweak        = {"vid_vsync_lag_tweak",        "1.0"                  };
cvar_t r_swapInterval             = {"vid_vsync",                  "0",       CVAR_SILENT };
cvar_t r_win_save_pos             = {"vid_win_save_pos",           "1",       CVAR_SILENT };
cvar_t r_win_save_size            = {"vid_win_save_size",          "1",       CVAR_SILENT };
cvar_t vid_xpos                   = {"vid_xpos",                   "3",       CVAR_SILENT };
cvar_t vid_ypos                   = {"vid_ypos",                   "39",      CVAR_SILENT };
cvar_t vid_win_displayNumber      = {"vid_win_displaynumber",      "0",       CVAR_SILENT };
cvar_t r_conwidth                 = {"vid_conwidth",               "0",       CVAR_NO_RESET | CVAR_SILENT | CVAR_AUTO, conres_changed_callback };
cvar_t r_conheight                = {"vid_conheight",              "0",       CVAR_NO_RESET | CVAR_SILENT | CVAR_AUTO, conres_changed_callback };
cvar_t r_conscale                 = {"vid_conscale",               "2.0",     CVAR_NO_RESET | CVAR_SILENT, conres_changed_callback };
cvar_t vid_flashonactivity        = {"vid_flashonactivity",        "1",       CVAR_SILENT };
cvar_t r_verbose                  = {"vid_verbose",                "0",       CVAR_SILENT };
cvar_t r_showextensions           = {"vid_showextensions",         "0",       CVAR_SILENT };
cvar_t gl_multisamples            = {"gl_multisamples",            "0",       CVAR_LATCH }; // It's here because it needs to be registered before window creation

//
// function declaration
//

// True if we need to release the mouse and let the OS show cursor again
static qbool IN_OSMouseCursorRequired(void)
{
	// Explicit check here for key_game... really setting all modes is equivalent to "in_grab_windowed_mouse 0"
	qbool in_os_cursor_mode = (key_dest != key_game || cls.demoplayback) && (in_release_mouse_modes.integer & (1 << key_dest));

	// Windowed & (not-grabbing mouse | in OS cursor mode)
	return (!r_fullscreen.value && (!in_grab_windowed_mouse.value || in_os_cursor_mode));
}

// True if we're in a mode where we need to keep track of mouse movement
qbool IN_MouseTrackingRequired(void)
{
	return (key_dest == key_menu || key_dest == key_hudeditor || key_dest == key_demo_controls);
}

// True if we need to display the internal Quake cursor to track the mouse
qbool IN_QuakeMouseCursorRequired(void)
{
	return mouse_active && IN_MouseTrackingRequired() && !IN_OSMouseCursorRequired();
}

static void in_raw_callback(cvar_t *var, char *value, qbool *cancel)
{
	if (var == &in_raw)
		Cvar_SetValue(&in_raw, atoi(value));

	IN_Restart_f();
}

static void in_grab_windowed_mouse_callback(cvar_t *val, char *value, qbool *cancel)
{
	GrabMouse((atoi(value) > 0 ? true : false), in_raw.integer);
}

static void GrabMouse(qbool grab, qbool raw)
{
	if ((grab && mouse_active && raw == in_raw.integer) || (!grab && !mouse_active) || !mouseinitialized || !sdl_window)
		return;

	if (!r_fullscreen.integer && in_grab_windowed_mouse.integer == 0)
	{
		if (!mouse_active)
			return;
		grab = 0;
	}
	// set initial position
	if (!raw && grab) {
		SDL_WarpMouseInWindow(sdl_window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
		old_x = glConfig.vidWidth / 2;
		old_y = glConfig.vidHeight / 2;
	}

	SDL_SetWindowGrab(sdl_window, grab ? SDL_TRUE : SDL_FALSE);
	SDL_SetRelativeMouseMode((raw && grab) ? SDL_TRUE : SDL_FALSE);
	SDL_GetRelativeMouseState(NULL, NULL);

	// never show real cursor in fullscreen
	if (r_fullscreen.integer) {
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_ShowCursor(grab ? SDL_DISABLE : SDL_ENABLE);
	}

	SDL_SetCursor(NULL); /* Force rewrite of it */

	mouse_active = grab;
}

void IN_Commands(void)
{
}

void IN_StartupMouse(void)
{
	Cvar_Register(&in_raw);
	Cvar_Register(&in_grab_windowed_mouse);
	Cvar_Register(&in_release_mouse_modes);

	mouseinitialized = true;

	Com_Printf("%s mouse input initialized\n", in_raw.integer > 0 ? "RAW" : "SDL");
}

void IN_ActivateMouse(void)
{
	GrabMouse(true, in_raw.integer);
}

void IN_DeactivateMouse(void)
{
	GrabMouse(false, in_raw.integer);
}

void IN_Frame(void)
{
	if (!sdl_window)
		return;

	HandleEvents();

	if (!ActiveApp || Minimized || IN_OSMouseCursorRequired()) {
		IN_DeactivateMouse();
		return;
	} else {
		IN_ActivateMouse();
	}

	if (mouse_active && SDL_GetRelativeMouseMode()) {
#ifdef __APPLE__
		OSX_Mouse_GetMouseMovement(&mx, &my);
#else
		SDL_GetRelativeMouseState(&mx, &my);
#endif
	}
	
}

void Sys_SendKeyEvents(void)
{
	IN_Frame();

	if (sys_inactivesleep.integer > 0) {
		// Yield the CPU a little
		if ((ISPAUSED && (!ActiveApp)) || Minimized || block_drawing) {
			if (!cls.download) {
				SDL_Delay(50);
			}
			scr_skipupdate = 1; // no point to draw anything
		} else if (!ActiveApp) { // Delay a bit less if just not active window
			if (!cls.download) {
				SDL_Delay(20);
			}
		}
	}
}

void IN_Restart_f(void)
{
	qbool old_mouse_active = mouse_active;

	IN_Shutdown();
	IN_Init();

	// if mouse was active before restart, try to re-activate it
	if (old_mouse_active) {
		IN_ActivateMouse();
	}
}


// Converts co-ordinates for the whole desktop to co-ordinates for a specific display
static void VID_RelativePositionFromAbsolute(int* x, int* y, int* display)
{
	int displays = SDL_GetNumVideoDisplays();
	int i = 0;

	for (i = 0; i < displays; ++i)
	{
		SDL_Rect bounds;

		if (SDL_GetDisplayBounds(i, &bounds) == 0)
		{
			if (*x >= bounds.x && *x < bounds.x + bounds.w && *y >= bounds.y && *y < bounds.y + bounds.h)
			{
				*x = *x - bounds.x;
				*y = *y - bounds.y;
				*display = i;
				return;
			}
		}
	}

	*display = 0;
}

// Converts co-ordinates for a specific display to those for whole desktop
static void VID_AbsolutePositionFromRelative(int* x, int* y, int* display)
{
	SDL_Rect bounds;
	
	// Try and get bounds for the specified display - default back to main display if there's an issue
	if (SDL_GetDisplayBounds(*display, &bounds))
	{
		*display = 0;
		if (SDL_GetDisplayBounds(*display, &bounds))
		{
			// Still an issue - reset back to top-left of screen
			Com_Printf("Error detecting resolution...\n");
			*x = *y = 0;
			return;
		}
	}

	// Adjust co-ordinates, making sure some of the window will always be visible
	*x = bounds.x + min(*x, bounds.w - 30);
	*y = bounds.y + min(*y, bounds.h - 30);
}

static void VID_SetDeviceGammaRampReal(unsigned short *ramps)
{
#ifdef __linux__
	SDL_SysWMinfo info;
	Display *display;
	int screen;
	static short once = 1;
	static short gamma_works = 0;

	if (!use_linux_gamma_workaround) {
		SDL_SetWindowGammaRamp(sdl_window, ramps, ramps+256,ramps+512);
		vid_hwgamma_enabled = true;
		return;
	}

	SDL_VERSION(&info.version);
	screen = SDL_GetWindowDisplayIndex(sdl_window);

	if (screen < 0) {
		Com_Printf("error: couldn't get screen number to set gamma\n");
		return;
	}

	if (SDL_GetWindowWMInfo(sdl_window, &info) != SDL_TRUE) {
		Com_Printf("error: can not get display pointer, gamma won't work: %s\n", SDL_GetError());
		return;
	}

	if (info.subsystem != SDL_SYSWM_X11) {
		Com_Printf("error: not x11, gamma won't work\n");
		return;
	}

	display = info.info.x11.display;

	if (once) {
		int size;
		XF86VidModeGetGammaRampSize(display, screen, &size);

		if (size != 256) {
			Com_Printf("error: gamma size (%d) not supported, gamma wont work!\n", size);
			once = 0;
			return;
		}

		if (!XF86VidModeGetGammaRamp(display, screen, 256, sysramps, sysramps+256, sysramps+512)) {
			Com_DPrintf("error: cannot get system gamma ramps, gamma won't work\n");
			once = 0;
			return;
		}
		once = 0;
		gamma_works = 1;
	}

	if (gamma_works) {
		/* It returns true unconditionally ... */
		XF86VidModeSetGammaRamp(display, screen, 256, ramps, ramps+256, ramps+512);
		vid_hwgamma_enabled = true;
	}
	return;
#else
	SDL_SetWindowGammaRamp(sdl_window, ramps, ramps+256,ramps+512);
#endif

	vid_hwgamma_enabled = true;
}

#ifdef __linux__
static void VID_RestoreSystemGamma(void)
{
	VID_SetDeviceGammaRampReal(sysramps);
}
#endif

static void window_event(SDL_WindowEvent *event)
{
	extern qbool scr_skipupdate;
	int flags = SDL_GetWindowFlags(sdl_window);

	switch (event->event) {
		case SDL_WINDOWEVENT_MINIMIZED:
			Minimized = true;

		case SDL_WINDOWEVENT_FOCUS_LOST:
			ActiveApp = false;
#ifdef __linux__
			if (use_linux_gamma_workaround) {
				if (Minimized || vid_hwgammacontrol.integer != 3) {
					VID_RestoreSystemGamma();
				}
			}
#endif
#ifdef _WIN32
			Sys_ActiveAppChanged ();
#endif
			break;

		case SDL_WINDOWEVENT_FOCUS_GAINED:
			TP_ExecTrigger("f_focusgained");
			/* Fall through */
		case SDL_WINDOWEVENT_RESTORED:
			Minimized = false;
			ActiveApp = true;
			scr_skipupdate = 0;
#ifdef __linux__
			if (use_linux_gamma_workaround) {
				v_gamma.modified = true;
			}
#endif
#ifdef _WIN32
			Sys_ActiveAppChanged ();
#endif
			break;

		case SDL_WINDOWEVENT_MOVED:
			if (!(flags & SDL_WINDOW_FULLSCREEN) && r_win_save_pos.integer) {
				int displayNumber = 0;
				int x = event->data1;
				int y = event->data2;

				VID_RelativePositionFromAbsolute(&x, &y, &displayNumber);

				Cvar_SetValue(&vid_win_displayNumber, displayNumber);
				Cvar_SetValue(&vid_xpos, x);
				Cvar_SetValue(&vid_ypos, y);
			}
			break;

		case SDL_WINDOWEVENT_RESIZED:
			if (!(flags & SDL_WINDOW_FULLSCREEN)) {
				glConfig.vidWidth = event->data1;
				glConfig.vidHeight = event->data2;
				if (r_win_save_size.integer) {
					Cvar_LatchedSetValue(&vid_win_width, event->data1);
					Cvar_LatchedSetValue(&vid_win_height, event->data2);
				}
				if (!r_conwidth.integer || !r_conheight.integer)
					VID_UpdateConRes();
			}
			break;
	}
}

// FIXME: APPLE K_F13-15 etc...

static const byte scantokey[128] = {
//  0               1               2               3               4               5               6                   7
//  8               9               A               B               C               D               E                   F
    0,              0,              0,              0,              'a',            'b',            'c',                'd',            // 0
    'e',            'f',            'g',            'h',            'i',            'j',            'k',                'l',
    'm',            'n',            'o',            'p',            'q',            'r',            's',                't',            // 1
    'u',            'v',            'w',            'x',            'y',            'z',            '1',                '2',
    '3',            '4',            '5',            '6',            '7',            '8',            '9',                '0',            // 2
    K_ENTER,        K_ESCAPE,       K_BACKSPACE,    K_TAB,          K_SPACE,        '-',            '=',                '[',
    ']',            '\\',           0,              ';',            '\'',           '`',            ',',                '.',            // 3
    '/' ,           K_CAPSLOCK,     K_F1,           K_F2,           K_F3,           K_F4,           K_F5,               K_F6,
    K_F7,           K_F8,           K_F9,           K_F10,          K_F11,          K_F12,          K_PRINTSCR,         K_SCRLCK,    // 4
    K_PAUSE,        K_INS,          K_HOME,         K_PGUP,         K_DEL,          K_END,          K_PGDN,             K_RIGHTARROW,
    K_LEFTARROW,    K_DOWNARROW,    K_UPARROW,      KP_NUMLOCK,     KP_SLASH,       KP_STAR,        KP_MINUS,           KP_PLUS,        // 5
    KP_ENTER,       KP_END,         KP_DOWNARROW,   KP_PGDN,        KP_LEFTARROW,   KP_5,           KP_RIGHTARROW,      KP_HOME,
    KP_UPARROW,     KP_PGUP,        KP_INS,         KP_DEL,         K_ISO,          K_MENU,         0,                  0,              // 6
    0,              0,              0,              0,              0,              0,              0,                  0,
    0,              0,              0,              0,              0,              0,              K_MENU,             0,              // 7
#ifdef __APPLE__
    K_LCTRL,        K_LSHIFT,       K_LALT,         K_CMD,          K_RCTRL,        K_RSHIFT,       K_RALT,             K_CMD,         // E
#else
    K_LCTRL,        K_LSHIFT,       K_LALT,         K_LWIN,         K_RCTRL,        K_RSHIFT,       K_RALT,             K_RWIN,         // E
#endif
};

byte Key_ScancodeToQuakeCode(int scancode)
{
	byte quakeCode = 0;
	if (scancode < 120)
		quakeCode = scantokey[scancode];
	else if (scancode >= 224 && scancode < 224 + 8)
		quakeCode = scantokey[scancode - 104];

	if (!cl_keypad.integer) {
		// compatibility mode without knowledge about keypad-keys:
		switch (quakeCode)
		{
		case KP_NUMLOCK:     quakeCode = K_PAUSE;          break;
		case KP_SLASH:       quakeCode = '/';              break;
		case KP_STAR:        quakeCode = '*';              break;
		case KP_MINUS:       quakeCode = '-';              break;
		case KP_HOME:        quakeCode = K_HOME;           break;
		case KP_UPARROW:     quakeCode = K_UPARROW;        break;
		case KP_PGUP:        quakeCode = K_PGUP;           break;
		case KP_LEFTARROW:   quakeCode = K_LEFTARROW;      break;
		case KP_5:           quakeCode = '5';              break;
		case KP_RIGHTARROW:  quakeCode = K_RIGHTARROW;     break;
		case KP_PLUS:        quakeCode = '+';              break;
		case KP_END:         quakeCode = K_END;            break;
		case KP_DOWNARROW:   quakeCode = K_DOWNARROW;      break;
		case KP_PGDN:        quakeCode = K_PGDN;           break;
		case KP_INS:         quakeCode = K_INS;            break;
		case KP_DEL:         quakeCode = K_DEL;            break;
		case KP_ENTER:       quakeCode = K_ENTER;          break;
		default:                                           break;
		}
	}

	return quakeCode;
}

byte Key_CharacterToQuakeCode(char ch)
{
	// Uses fact that SDLK_a == 'a'... is this okay?
	
	// Convert from key-code to scan-code to see what physical button they pressed
	int scancode = SDL_GetScancodeFromKey(ch);

	return Key_ScancodeToQuakeCode(scancode);
}

wchar Key_Event_TextInput(wchar unichar);

static void keyb_textinputevent(char* text)
{
	int i = 0;
	int len = 0;
	wchar unichar = 0;

	// Only process text input messages here
	if (key_dest != key_console && key_dest != key_message)
		return;

	if (!*text)
		return;

	len = strlen(text);
	for (i = 0; i < len; ++i)
	{
		unichar = TextEncodingDecodeUTF8(text, &i);

		if (unichar)
			Key_Event_TextInput(unichar);
	}
}

static void keyb_event(SDL_KeyboardEvent *event)
{
	byte result = Key_ScancodeToQuakeCode(event->keysym.scancode);
	
	if (result == 0) {
		Com_DPrintf("%s: unknown scancode %d\n", __func__, event->keysym.scancode);
		return;
	}

	Key_Event(result, event->state);
}

static void mouse_button_event(SDL_MouseButtonEvent *event)
{
	unsigned key;

	switch (event->button) {
	case SDL_BUTTON_LEFT:
		key = K_MOUSE1;
		break;
	case SDL_BUTTON_RIGHT:
		key = K_MOUSE2;
		break;
	case SDL_BUTTON_MIDDLE:
		key = K_MOUSE3;
		break;
	case 8:
	case SDL_BUTTON_X1:
		key = K_MOUSE4;
		break;
	case 9:
	case SDL_BUTTON_X2:
		key = K_MOUSE5;
		break;
	default:
		Com_DPrintf("%s: unknown button %d\n", __func__, event->button);
		return;
	}

	Key_Event(key, event->state);
}

static void mouse_wheel_event(SDL_MouseWheelEvent *event)
{
	if (event->y > 0) {
		Key_Event(K_MWHEELUP, true);
		Key_Event(K_MWHEELUP, false);
	} else if (event->y < 0) {
		Key_Event(K_MWHEELDOWN, true);
		Key_Event(K_MWHEELDOWN, false);
	}
}

static void HandleEvents(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			Sys_Quit();
			break;
		case SDL_WINDOWEVENT:
			window_event(&event.window);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			keyb_event(&event.key);
			break;
		case SDL_TEXTINPUT:
			keyb_textinputevent(event.text.text);
			break;
		case SDL_MOUSEMOTION:
			if (mouse_active && !SDL_GetRelativeMouseMode()) {
				mx = old_x - event.motion.x;
				my = old_y - event.motion.y;
				old_x = event.motion.x;
				old_y = event.motion.y;
				cursor_x = min(max(0, cursor_x + event.motion.x - glConfig.vidWidth / 2), glConfig.vidWidth);
				cursor_y = min(max(0, cursor_y + event.motion.y - glConfig.vidHeight / 2), glConfig.vidHeight);
				SDL_WarpMouseInWindow(sdl_window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
			}
			else {
				cursor_x = event.motion.x;
				cursor_y = event.motion.y;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouse_button_event(&event.button);
			break;
		case SDL_MOUSEWHEEL:
			mouse_wheel_event(&event.wheel);
			break;
		case SDL_DROPFILE:
			/* TODO: Add handling for different file types */
			Cbuf_AddText("playdemo ");
			Cbuf_AddText(event.drop.file);
			Cbuf_AddText("\n");
			SDL_free(event.drop.file);
			break;
		}   
	} 
}

/*****************************************************************************/

void VID_Shutdown(void)
{
	IN_DeactivateMouse();

	SDL_StopTextInput();

#ifdef __linux__
	if (use_linux_gamma_workaround) {
		VID_RestoreSystemGamma();
	}
#endif

	if (sdl_context) {
		SDL_GL_DeleteContext(sdl_context);
		sdl_context = NULL;
	}

	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		sdl_window = NULL;
	}

	SDL_GL_ResetAttributes();

	if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	memset(&glConfig, 0, sizeof(glConfig));

	Q_free(modelist);
	modelist_count = 0;
	vid_hwgamma_enabled = false;
	vid_initialized = false;
}

static int VID_SDL_InitSubSystem(void)
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Sys_Error("Couldn't initialize SDL video: %s\n", SDL_GetError());
			return -1;
		}
	}

	SDL_StartTextInput();

	return 0;
}

void VID_RegisterLatchCvars(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);

	Cvar_Register(&vid_width);
	Cvar_Register(&vid_height);
	Cvar_Register(&vid_win_width);
	Cvar_Register(&vid_win_height);
	Cvar_Register(&vid_hwgammacontrol);
	Cvar_Register(&r_colorbits);
	Cvar_Register(&r_24bit_depth);
	Cvar_Register(&r_fullscreen);
	Cvar_Register(&r_displayRefresh);
	Cvar_Register(&vid_usedesktopres);
	Cvar_Register(&vid_win_borderless);
	Cvar_Register(&gl_multisamples);
	Cvar_Register(&vid_displayNumber);
	Cvar_Register(&vid_minimize_on_focus_loss);
	Cvar_Register(&vid_grab_keyboard);

#ifdef __linux__
	Cvar_Register(&vid_gamma_workaround);
#endif

	Cvar_ResetCurrentGroup();
}

void VID_RegisterCvars(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);

	Cvar_Register(&vid_vsync_lag_fix);
	Cvar_Register(&vid_vsync_lag_tweak);
	Cvar_Register(&r_win_save_pos);
	Cvar_Register(&r_win_save_size);
	Cvar_Register(&r_swapInterval);
	Cvar_Register(&r_verbose);
	Cvar_Register(&vid_xpos);
	Cvar_Register(&vid_ypos);
	Cvar_Register(&r_conwidth);
	Cvar_Register(&r_conheight);
	Cvar_Register(&r_conscale);
	Cvar_Register(&vid_flashonactivity);
	Cvar_Register(&r_showextensions);
	Cvar_Register(&vid_win_displayNumber);

	Cvar_ResetCurrentGroup();
}

// Returns valid display number
static int VID_DisplayNumber(qbool fullscreen)
{
	int displayNumber = (fullscreen ? vid_displayNumber.value : vid_win_displayNumber.value);
	int displays = SDL_GetNumVideoDisplays();

	return max(0, min(displays - 1, displayNumber));
}

static void VID_SetupModeList(void)
{
	int i;

	modelist_count = SDL_GetNumDisplayModes(VID_DisplayNumber(r_fullscreen.integer == 1));

	if (modelist_count <= 0) {
		Com_Printf("error getting display modes: %s\n", SDL_GetError());
		modelist_count = 0;
	}

	modelist = Q_calloc(modelist_count, sizeof(*modelist));

	for (i = 0; i < modelist_count; i++) {
		SDL_GetDisplayMode(0, i, &modelist[i]);
	}
}

static void VID_SetupResolution(void)
{
	SDL_DisplayMode display_mode;
	int display_nbr;

	if (r_fullscreen.integer == 1) {
		display_nbr = VID_DisplayNumber(true);
		if (vid_usedesktopres.integer == 1) {
			if (SDL_GetDesktopDisplayMode(display_nbr, &display_mode) == 0) {
				glConfig.vidWidth = last_working_width = display_mode.w;
				glConfig.vidHeight = last_working_height = display_mode.h;
				glConfig.displayFrequency = last_working_hz = display_mode.refresh_rate;
				last_working_display = display_nbr;
				last_working_values = true;
				Cvar_AutoSetInt(&vid_width, display_mode.w);
				Cvar_AutoSetInt(&vid_height, display_mode.h);
				Cvar_AutoSetInt(&r_displayRefresh, display_mode.refresh_rate);
				return;
			} else {
				Com_Printf("warning: failed to get desktop resolution\n");
			}
		}
		/* Note the fall through in case the above fails */

		if (vid_width.integer == 0 || vid_height.integer == 0) {
			/* Try some default if nothing is set and we failed to get desktop res */
			glConfig.vidWidth = 1024;
			glConfig.vidHeight = 768;
			glConfig.displayFrequency = 0;

			Cvar_LatchedSetValue(&vid_width, 1024);
			Cvar_LatchedSetValue(&vid_height, 768);
			Cvar_LatchedSetValue(&r_displayRefresh, 0);
			return;
		}

		/* USER specified resolution */
		glConfig.vidWidth = bound(320, vid_width.integer, vid_width.integer);
		glConfig.vidHeight = bound(200, vid_height.integer, vid_height.integer);
		glConfig.displayFrequency = r_displayRefresh.integer;

	} else { /* Windowed mode */
		if (vid_win_width.integer == 0 || vid_win_height.integer == 0) {
			Cvar_LatchedSetValue(&vid_win_width, 640);
			Cvar_LatchedSetValue(&vid_win_height, 480);
			Cvar_LatchedSetValue(&r_displayRefresh, 0);
		}

		glConfig.vidWidth = bound(320, vid_win_width.integer, vid_win_width.integer);
		glConfig.vidHeight = bound(240, vid_win_height.integer, vid_win_height.integer);
		glConfig.displayFrequency = 0;
	}
}

int VID_GetCurrentModeIndex(void)
{
	int i;

	int best_freq = 0;
	int best_idx = -1;

	for (i = 0; i < modelist_count; i++) {
		if (modelist[i].w == vid_width.integer && modelist[i].h == vid_height.integer) {
			if (modelist[i].refresh_rate == r_displayRefresh.integer) {
				Com_DPrintf("MATCHED: %dx%d hz:%d\n", modelist[i].w, modelist[i].h, modelist[i].refresh_rate);
				return i;
			}

			if (modelist[i].refresh_rate > best_freq) {
				best_freq = modelist[i].refresh_rate;
				best_idx = i;
			}
		}
	}

	/* width/height matched but not hz, using the best available */
	if (best_idx >= 0) {
		Cvar_AutoSetInt(&r_displayRefresh, modelist[best_idx].refresh_rate);
	}

	return best_idx;
}

int VID_GetModeIndexCount(void) {
	return modelist_count;
}

const SDL_DisplayMode *VID_GetDisplayMode(int index)
{
	if (index < 0 || index >= modelist_count) {
		return NULL;
	}

	return &modelist[index];
}

static void VID_SDL_GL_SetupAttributes(void)
{
	if (gl_multisamples.integer > 0) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, bound(0, gl_multisamples.integer, 16));
	}

	if (r_24bit_depth.integer == 1) {
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	} /* else, SDL2 defaults to 16 */
}

static int VID_SetWindowIcon(SDL_Window *sdl_window)
{
#ifdef __APPLE__
	// on OS X the icon is handled by the app bundle
	// it actually is higher resolution than the one here.
	return 0;
#else
	SDL_Surface *icon_surface;
        icon_surface = SDL_CreateRGBSurfaceFrom((void *)ezquake_icon.pixel_data, ezquake_icon.width, ezquake_icon.height, ezquake_icon.bytes_per_pixel * 8,
                ezquake_icon.width * ezquake_icon.bytes_per_pixel,
                0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);

        if (icon_surface) {
            SDL_SetWindowIcon(sdl_window, icon_surface);
            SDL_FreeSurface(icon_surface);
	    return 0;
        }

	return -1;
#endif
}

static void VID_SDL_Init(void)
{
	int flags;
	int r, g, b, a;
	
	if (glConfig.initialized == true) {
		return;
	}

	VID_SDL_InitSubSystem();

	flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_SHOWN;

#ifdef SDL_WINDOW_ALLOW_HIGHDPI
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
	if (r_fullscreen.integer > 0) {
		if (vid_usedesktopres.integer == 1) {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
	} else {
		if (vid_win_borderless.integer > 0) {
			flags |= SDL_WINDOW_BORDERLESS;
		}
	}

	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, vid_minimize_on_focus_loss.integer == 0 ? "0" : "1");

#ifdef __APPLE__
	SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
#endif
	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, vid_grab_keyboard.integer == 0 ? "0" : "1");
	SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0", SDL_HINT_OVERRIDE);

	VID_SDL_GL_SetupAttributes();

	VID_SetupModeList();
	VID_SetupResolution();

	if (r_fullscreen.integer == 0) {
		int displayNumber = VID_DisplayNumber(false);
		int xpos = vid_xpos.integer;
		int ypos = vid_ypos.integer;

		VID_AbsolutePositionFromRelative(&xpos, &ypos, &displayNumber);

		sdl_window = SDL_CreateWindow(WINDOW_CLASS_NAME, xpos, ypos, glConfig.vidWidth, glConfig.vidHeight, flags);
	} else {
		int windowWidth = glConfig.vidWidth;
		int windowHeight = glConfig.vidHeight;
		int windowX = SDL_WINDOWPOS_CENTERED;
		int windowY = SDL_WINDOWPOS_CENTERED;
		int displayNumber = VID_DisplayNumber(true);
		SDL_Rect bounds;

		if (SDL_GetDisplayBounds(displayNumber, &bounds) == 0)
		{
			windowX = bounds.x;
			windowY = bounds.y;
			windowWidth = bounds.w;
			windowHeight = bounds.h;
		}
		else
		{
			Com_Printf("Couldn't determine bounds of display #%d, defaulting to main display\n", displayNumber);
		}

		sdl_window = SDL_CreateWindow(WINDOW_CLASS_NAME, windowX, windowY, windowWidth, windowHeight, flags);
	}

	if (!sdl_window) {
		Sys_Error("Failed to create SDL window: %s\n", SDL_GetError());
	}

	if (r_fullscreen.integer > 0 && vid_usedesktopres.integer != 1) {
		int index = VID_GetCurrentModeIndex();

		if (index < 0) {
			Com_Printf("Couldn't find a matching video mode for the selected values, check video settings!\n");
			if (last_working_values == true) {
				Com_Printf("Using last known working settings: %dx%d@%dHz\n", last_working_width, last_working_height, last_working_hz);
				Cvar_LatchedSetValue(&vid_width, (float)last_working_width);
				Cvar_LatchedSetValue(&vid_height, (float)last_working_height);
				Cvar_LatchedSetValue(&r_displayRefresh, (float)last_working_hz);
				Cvar_LatchedSetValue(&vid_displayNumber, (float)last_working_display);
				Cvar_AutoSetInt(&vid_width, last_working_width);
				Cvar_AutoSetInt(&vid_height, last_working_height);
				Cvar_AutoSetInt(&r_displayRefresh, last_working_hz);
				Cvar_AutoSetInt(&vid_displayNumber, last_working_display);
			} else {
				Com_Printf("Using desktop resolution as fallback\n");
				Cvar_LatchedSet(&vid_usedesktopres, "1");
				Cvar_AutoSet(&vid_usedesktopres, "1");
			}
			VID_SetupResolution();
		} else {
			if (SDL_SetWindowDisplayMode(sdl_window, &modelist[index]) != 0) {
				Com_Printf("sdl error: %s\n", SDL_GetError());
			} else {
				last_working_width = (&modelist[index])->w;
				last_working_height = (&modelist[index])->h;
				last_working_hz = (&modelist[index])->refresh_rate;
				last_working_display = vid_displayNumber.integer;
				last_working_values = true;
			}
		}

		if (SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN) < 0) {
			Com_Printf("Failed to change to fullscreen mode\n");
		}
	}
      

	if (VID_SetWindowIcon(sdl_window) < 0) {
		Com_Printf("Failed to set window icon");
	}

	SDL_SetWindowMinimumSize(sdl_window, 320, 240);

	sdl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_context) {
		Com_Printf("Couldn't create OpenGL context: %s\n", SDL_GetError());
		return;
	}

	v_gamma.modified = true;
	r_swapInterval.modified = true;
	
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);

	glConfig.colorBits = r+g+b+a;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &glConfig.depthBits);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &glConfig.stencilBits);

	glConfig.vendor_string         = glGetString(GL_VENDOR);
	glConfig.renderer_string       = glGetString(GL_RENDERER);
	glConfig.version_string        = glGetString(GL_VERSION);
	glConfig.extensions_string     = glGetString(GL_EXTENSIONS);

	glConfig.initialized = true;
}

static void GL_SwapBuffers (void)
{
	SDL_GL_SwapWindow(sdl_window);
}

static void GL_SwapBuffersWithVsyncFix(void)
{
	double time_before_swap;

	time_before_swap = Sys_DoubleTime();

	SDL_GL_SwapWindow(sdl_window);

	vid_last_swap_time = Sys_DoubleTime();
	vid_vsync_lag = vid_last_swap_time - time_before_swap;
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width  = glConfig.vidWidth;
	*height = glConfig.vidHeight;

	if (cls.state != ca_active) {
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void GL_EndRendering (void)
{
	if (r_swapInterval.modified) {
		if (r_swapInterval.integer == 0) {
			if (SDL_GL_SetSwapInterval(0)) {
				Con_Printf("vsync: Failed to disable vsync...\n");
			}
		} else if (r_swapInterval.integer == -1) {
			if (SDL_GL_SetSwapInterval(-1)) {
				Con_Printf("vsync: Failed to enable late swap tearing (vid_vsync -1), setting vid_vsync 1 instead...\n");
				Cvar_SetValueByName("vid_vsync", 1);
			}
		}

		if (r_swapInterval.integer == 1) {
			if (SDL_GL_SetSwapInterval(1)) {
				Con_Printf("vsync: Failed to enable vsync...\n");
			}
		}

		r_swapInterval.modified = false;
        }

	if (!scr_skipupdate || block_drawing) {
		// Multiview - Only swap the back buffer to front when all views have been drawn in multiview.
		if (cl_multiview.value && cls.mvdplayback) {
			if (CURRVIEW != 1) {
				return;
			}
		}

		if (vid_vsync_lag_fix.integer > 0) {
			GL_SwapBuffersWithVsyncFix();
		} else {
			GL_SwapBuffers(); 
		}
	}
}

void VID_SetCaption (char *text)
{
	if (!sdl_window) {
		return;
	}

	SDL_SetWindowTitle(sdl_window, text);
}

void VID_NotifyActivity(void)
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (ActiveApp || !vid_flashonactivity.value) {
		return;
	}

	if (SDL_GetWindowWMInfo(sdl_window, &info) == SDL_TRUE) {
		if (info.subsystem == SDL_SYSWM_WINDOWS) {
			FlashWindow(info.info.win.window, TRUE);
		}
	} else {
		Com_DPrintf("Sys_NotifyActivity: SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
	}
#endif
}

void VID_SetDeviceGammaRamp(unsigned short *ramps)
{
	if (!sdl_window || COM_CheckParm("-nohwgamma")) {
		return;
	}

	if (r_fullscreen.integer > 0) {
		if (vid_hwgammacontrol.integer > 0) {
			VID_SetDeviceGammaRampReal(ramps);
		}
	} else {
		if (vid_hwgammacontrol.integer >= 2) {
			VID_SetDeviceGammaRampReal(ramps);
		}
	}
}

void VID_Minimize (void) 
{
	if (!sdl_window) {
		return;
	}

	SDL_MinimizeWindow(sdl_window);
}

void VID_Restore (void)
{
	if (!sdl_window)
		return;

	SDL_RestoreWindow(sdl_window);
	SDL_RaiseWindow(sdl_window);
}

qbool VID_VSyncIsOn(void)
{
	return (r_swapInterval.integer != 0);
}

#define NUMTIMINGS 5
double timings[NUMTIMINGS];
double render_frame_start, render_frame_end;

// Returns true if it's not time yet to run a frame
qbool VID_VSyncLagFix(void)
{
        extern double vid_last_swap_time;
        double avg_rendertime, tmin, tmax;

	static int timings_idx;
        int i;

	if (!VID_VSyncIsOn() || !vid_vsync_lag_fix.integer) {
		return false;
	}

	if (!glConfig.displayFrequency) {
		Com_Printf("VID_VSyncLagFix: displayFrequency isn't set, can't enable vsync lag fix\n");
		return false;
	}

        // collect statistics so that
        timings[timings_idx] = render_frame_end - render_frame_start;
        timings_idx = (timings_idx + 1) % NUMTIMINGS;
        avg_rendertime = tmin = tmax = 0;
        for (i = 0; i < NUMTIMINGS; i++) {
                if (timings[i] == 0) {
                        return false;   // not enough statistics yet
		}

                avg_rendertime += timings[i];

                if (timings[i] < tmin || !tmin) {
                        tmax = timings[i];
		}

                if (timings[i] > tmax) {
                        tmax = timings[i];
		}
        }
        avg_rendertime /= NUMTIMINGS;
        // if (tmax and tmin differ too much) do_something(); ?
        avg_rendertime = tmax;  // better be on the safe side

	double time_left = vid_last_swap_time + 1.0/glConfig.displayFrequency - Sys_DoubleTime();
	time_left -= avg_rendertime;
	time_left -= vid_vsync_lag_tweak.value * 0.001;
	if (time_left > 0) {
		extern cvar_t sys_yieldcpu;

		if (time_left > 0.001 && sys_yieldcpu.integer) {
			Sys_MSleep(min(time_left * 1000, 500));
		}

		return true;    // don't run a frame yet
	}
        return false;
}

static void GfxInfo_f(void)
{
	SDL_DisplayMode current;

	Com_Printf_State(PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	Com_Printf_State(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string );
	Com_Printf_State(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string );

	if (r_showextensions.value) {
		Com_Printf_State(PRINT_ALL, "GL_EXTENSIONS: %s\n", glConfig.extensions_string);
	}

	Com_Printf_State(PRINT_ALL, "PIXELFORMAT: color(%d-bits) Z(%d-bit)\n             stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);

	if (SDL_GetCurrentDisplayMode(VID_DisplayNumber(r_fullscreen.value), &current) != 0) {
		current.refresh_rate = 0; // print 0Hz if we run into problem fetching data
	}

	Com_Printf_State(PRINT_ALL, "MODE: %d x %d @ %d Hz ", current.w, current.h, current.refresh_rate);
	
	if (r_fullscreen.integer) {
		Com_Printf_State(PRINT_ALL, "[fullscreen]\n");
	} else {
		Com_Printf_State(PRINT_ALL, "[windowed]\n");
	}

	Com_Printf_State(PRINT_ALL, "CONRES: %d x %d\n", r_conwidth.integer, r_conheight.integer );

}

static void VID_ParseCmdLine(void)
{
	int i, w = 0, h = 0, display = 0;

	if (COM_CheckParm("-window") || COM_CheckParm("-startwindowed")) {
		Cvar_LatchedSetValue(&r_fullscreen, 0);
	}

	if ((i = COM_CheckParm("-freq")) && i + 1 < COM_Argc()) {
		Cvar_LatchedSetValue(&r_displayRefresh, Q_atoi(COM_Argv(i + 1)));
	}

	if ((i = COM_CheckParm("-bpp")) && i + 1 < COM_Argc()) {
		Cvar_LatchedSetValue(&r_colorbits, Q_atoi(COM_Argv(i + 1)));
	}

	w = ((i = COM_CheckParm("-width"))  && i + 1 < COM_Argc()) ? Q_atoi(COM_Argv(i + 1)) : 0;
	h = ((i = COM_CheckParm("-height")) && i + 1 < COM_Argc()) ? Q_atoi(COM_Argv(i + 1)) : 0;

	display = ((i = COM_CheckParm("-display")) && i + 1 < COM_Argc()) ? Q_atoi(COM_Argv(i + 1)) : 0;
	if (i) {
		if (COM_CheckParm("-window")) {
			Cvar_LatchedSetValue(&vid_win_displayNumber, display);
		} else {
			Cvar_LatchedSetValue(&vid_displayNumber, display);
		}
	}

	if (w && h) {
		if (COM_CheckParm("-window")) {
			Cvar_LatchedSetValue(&vid_win_width,  w);
			Cvar_LatchedSetValue(&vid_win_height, h);
		} else {
			Cvar_LatchedSetValue(&vid_width, w);
			Cvar_LatchedSetValue(&vid_height, h);
		}
	} // else if (w || h) { Sys_Error("Must specify both -width and -height\n"); }

	if ((i = COM_CheckParm("-conwidth")) && i + 1 < COM_Argc()) {
		Cvar_SetIgnoreCallback(&r_conwidth, COM_Argv(i + 1));
	}

	if ((i = COM_CheckParm("-conheight")) && i + 1 < COM_Argc()) {
		Cvar_SetIgnoreCallback(&r_conheight, COM_Argv(i + 1));
	}
}

static void VID_Restart_f(void)
{
	extern void GFX_Init(void);
	extern void ReloadPaletteAndColormap(void);
	qbool old_con_suppress;

	if (!host_initialized) { // sanity
		Com_Printf("Can't do %s yet\n", Cmd_Argv(0));
		return;
	}

	VID_Shutdown();

	ReloadPaletteAndColormap();

	// keys can get stuck because SDL2 doesn't send keyup event when the video system is down
	Key_ClearStates();

	VID_Init(host_basepal);

	// force models to reload (just flush, no actual loading code here)
	Cache_Flush();

	// shut up warnings during GFX_Init();
	old_con_suppress = con_suppress;
	con_suppress = (developer.value ? false : true);
	// reload 2D textures, particles textures, some other textures and gfx.wad
	GFX_Init();

	// reload skins
	Skin_Skins_f();

	con_suppress = old_con_suppress;

	// we need done something like for map reloading, for example reload textures for brush models
	R_NewMap(true);

	// force all cached models to be loaded, so no short HDD lag then u walk over level and discover new model
	Mod_TouchModels();

	// window may be re-created, so caption need to be forced to update
	CL_UpdateCaption(true);
}

static void VID_DisplayList_f(void)
{
	int displays = SDL_GetNumVideoDisplays();
	int i;

	for (i = 0; i < displays; i++) {
		const char *displayname = SDL_GetDisplayName(i);
		if (displayname == NULL) {
			displayname = "Unknown";
		}
		Com_Printf("%d: %s\n", i, displayname);
	}
}

static void VID_ModeList_f(void)
{
	int i = 0;

	if (modelist == NULL || modelist_count <= 0) {
		Com_Printf("error: no modes available\n");
		return;
	}
	
	for (; i < modelist_count; i++) {
		Com_Printf("%dx%d@%dHz\n", (&modelist[i])->w, (&modelist[i])->h, (&modelist[i])->refresh_rate);
	}
}

void VID_RegisterCommands(void) 
{
	if (!host_initialized) {
		Cmd_AddCommand("vid_gfxinfo", GfxInfo_f);
		Cmd_AddCommand("vid_restart", VID_Restart_f);
		Cmd_AddCommand("vid_displaylist", VID_DisplayList_f);
		Cmd_AddCommand("vid_modelist", VID_ModeList_f);
	}
}

static void VID_UpdateConRes(void)
{
	// Default
	if (r_conwidth.integer == 0 && r_conheight.integer == 0) {
		vid.width   = vid.conwidth  = bound(320, (int)(glConfig.vidWidth  / r_conscale.value), glConfig.vidWidth);
		vid.height  = vid.conheight = bound(200, (int)(glConfig.vidHeight / r_conscale.value), glConfig.vidHeight);
		Cvar_AutoSetInt(&r_conwidth, vid.conwidth);
		Cvar_AutoSetInt(&r_conheight, vid.conheight);

	} else if (r_conwidth.integer == 0) {
		double ar_w = (double)glConfig.vidWidth/(double)glConfig.vidHeight;

		vid.height  = vid.conheight = bound(200, r_conheight.integer, glConfig.vidHeight);
		vid.width   = vid.conwidth  = bound(320, (int)(r_conheight.integer*ar_w + 0.5), glConfig.vidWidth);
		Cvar_AutoSetInt(&r_conwidth, vid.conwidth);

	} else if (r_conheight.integer == 0) {
		double ar_h = (double)glConfig.vidHeight/(double)glConfig.vidWidth;

		vid.height  = vid.conheight = bound(200, (int)(r_conwidth.integer*ar_h + 0.5), glConfig.vidHeight);
		vid.width   = vid.conwidth  = bound(320, r_conwidth.integer, glConfig.vidWidth);
		Cvar_AutoSetInt(&r_conheight, vid.conheight);

	} else {
		// User specified, use that but check boundaries
		vid.width   = vid.conwidth  = bound(320, r_conwidth.integer,  glConfig.vidWidth);
		vid.height  = vid.conheight = bound(200, r_conheight.integer, glConfig.vidHeight);
		Cvar_SetValue(&r_conwidth, vid.conwidth);
		Cvar_SetValue(&r_conheight, vid.conheight);
	}

	vid.numpages = 2; // ??
	Draw_AdjustConback();
	vid.recalc_refdef = 1;
}

static void conres_changed_callback (cvar_t *var, char *string, qbool *cancel)
{
	/* Cvar_SetValue won't trigger a recursive callback since we're in the callback,
	 * but it's required to set the values here first to make them apply, and then cancel
	 * set to true will force the caller to return immediatly when this callback returns...
	 */
	if (var == &r_conwidth) {
		Cvar_SetValue(&r_conwidth, Q_atoi(string));
	} else if (var == &r_conheight) {
		Cvar_SetValue(&r_conheight, Q_atoi(string));
	} else if (var == &r_conscale) {
		Cvar_SetValue(&r_conscale, Q_atof(string));
	} else {
		Com_Printf("Called with unknown variable: %s\n", var->name ? var->name : "unknown");
	}

	VID_UpdateConRes();
	*cancel = true;
}


void VID_Init(unsigned char *palette) {

	vid.colormap = host_colormap;

	Check_Gamma(palette);
	VID_SetPalette(palette);

	VID_RegisterLatchCvars();

#ifdef __linux__
	use_linux_gamma_workaround = (vid_gamma_workaround.integer != 0);
#endif

	if (!host_initialized) {
		VID_RegisterCvars();
		VID_RegisterCommands();
		VID_ParseCmdLine();
	}

	VID_SDL_Init();

	// print info
	if (!host_initialized || r_verbose.integer) {
		GfxInfo_f();
	}

	VID_UpdateConRes();

	GL_Init(); // Real OpenGL stuff, vid_common_gl.c

	vid_initialized = true;
}

