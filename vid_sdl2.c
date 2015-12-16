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

#ifdef _WIN32
#include <windows.h>
#include <SDL_syswm.h>
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

static void in_raw_callback(cvar_t *var, char *value, qbool *cancel);
static void in_grab_windowed_mouse_callback(cvar_t *var, char *value, qbool *cancel);
static void conres_changed_callback (cvar_t *var, char *string, qbool *cancel);
static void GrabMouse(qbool grab, qbool raw);
static void GfxInfo_f(void);
static void HandleEvents();
static void VID_UpdateConRes(void);
void IN_Restart_f(void);

static SDL_Window       *sdl_window;
static SDL_GLContext    *sdl_context;

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

//
// cvars
//

extern cvar_t sys_inactivesleep;

// latched variables that can only change over a restart
cvar_t r_colorbits            = {"vid_colorbits",         "0",   CVAR_LATCH };
cvar_t r_24bit_depth          = {"vid_24bit_depth",       "1",   CVAR_LATCH };
cvar_t r_stereo               = {"vid_stereo",            "0",   CVAR_LATCH };
cvar_t r_fullscreen           = {"vid_fullscreen",        "1",   CVAR_LATCH };
cvar_t r_displayRefresh       = {"vid_displayfrequency",  "0",   CVAR_LATCH };
cvar_t vid_displayNumber      = {"vid_displaynumber",     "0",   CVAR_LATCH };
cvar_t vid_usedesktopres      = {"vid_usedesktopres",     "1",   CVAR_LATCH };
cvar_t vid_win_borderless     = {"vid_win_borderless",    "0",   CVAR_LATCH };
cvar_t vid_width              = {"vid_width",             "0",   CVAR_LATCH };
cvar_t vid_height             = {"vid_height",            "0",   CVAR_LATCH };
cvar_t vid_win_width          = {"vid_win_width",         "640", CVAR_LATCH };
cvar_t vid_win_height         = {"vid_win_height",        "480", CVAR_LATCH };

// TODO: Move the in_* cvars
cvar_t in_raw                 = {"in_raw",                "1",   CVAR_ARCHIVE | CVAR_SILENT, in_raw_callback};
cvar_t in_grab_windowed_mouse = {"in_grab_windowed_mouse","1",   CVAR_ARCHIVE | CVAR_SILENT, in_grab_windowed_mouse_callback};
cvar_t vid_vsync_lag_fix      = {"vid_vsync_lag_fix",     "0"};
cvar_t vid_vsync_lag_tweak    = {"vid_vsync_lag_tweak",   "1.0"};
cvar_t r_swapInterval         = {"vid_vsync",             "0",   CVAR_SILENT };
cvar_t r_win_save_pos         = {"vid_win_save_pos",      "1",   CVAR_SILENT };
cvar_t r_win_save_size        = {"vid_win_save_size",     "1",   CVAR_SILENT };
cvar_t vid_xpos               = {"vid_xpos",              "3",   CVAR_SILENT };
cvar_t vid_ypos               = {"vid_ypos",              "39",  CVAR_SILENT };
cvar_t vid_win_displayNumber  = {"vid_win_displaynumber", "0",   CVAR_SILENT };
cvar_t r_conwidth             = {"vid_conwidth",          "0",   CVAR_NO_RESET | CVAR_SILENT, conres_changed_callback };
cvar_t r_conheight            = {"vid_conheight",         "0",   CVAR_NO_RESET | CVAR_SILENT, conres_changed_callback };
cvar_t r_conscale             = {"vid_conscale",          "2.0", CVAR_NO_RESET | CVAR_SILENT, conres_changed_callback };
cvar_t vid_flashonactivity    = {"vid_flashonactivity",   "1",   CVAR_SILENT };
cvar_t r_verbose              = {"vid_verbose",           "0",   CVAR_SILENT };
cvar_t r_showextensions       = {"vid_showextensions",    "0",   CVAR_SILENT };
cvar_t gl_multisamples        = {"gl_multisamples",       "0",   CVAR_LATCH }; // It's here because it needs to be registered before window creation

//
// function declaration
//

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

	if (!ActiveApp || Minimized || (key_dest != key_game || cls.state != ca_active)) {
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
                        SDL_Delay(50);
                        scr_skipupdate = 1; // no point to draw anything
                } else if (!ActiveApp) { // Delay a bit less if just not active window
                        SDL_Delay(20);
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

static void window_event(SDL_WindowEvent *event)
{
	extern qbool scr_skipupdate;
	int flags = SDL_GetWindowFlags(sdl_window);

	switch (event->event) {
		case SDL_WINDOWEVENT_MINIMIZED:
			Minimized = true;

		case SDL_WINDOWEVENT_FOCUS_LOST:
			ActiveApp = false;
			break;

		case SDL_WINDOWEVENT_RESTORED:
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			Minimized = false;
			ActiveApp = true;
			scr_skipupdate = 0;
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
	if (scancode < 120)
		return scantokey[scancode];
	if (scancode >= 224 && scancode < 224 + 8)
		return scantokey[scancode - 104];
	return 0;
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
		Com_Printf("%s: unknown scancode %d\n", __func__, event->keysym.scancode);
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

static void HandleEvents()
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
				SDL_WarpMouseInWindow(sdl_window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
			} else {
				cursor_x = event.motion.x * ((double)vid.width / glConfig.vidWidth);
				cursor_y = event.motion.y * ((double)vid.height / glConfig.vidHeight);
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			mouse_button_event(&event.button);
			break;
		case SDL_MOUSEWHEEL:
			mouse_wheel_event(&event.wheel);
			break;
		}   
	} 
}

/*****************************************************************************/

void VID_Shutdown(void)
{
	IN_DeactivateMouse();

	SDL_StopTextInput();

	if (sdl_context) {
		SDL_GL_DeleteContext(sdl_context);
		sdl_context = NULL;
	}

	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		sdl_window = NULL;
	}

	if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);

	memset(&glConfig, 0, sizeof(glConfig));

	Q_free(modelist);
	modelist_count = 0;
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
	Cvar_Register(&r_colorbits);
	Cvar_Register(&r_24bit_depth);
	Cvar_Register(&r_fullscreen);
	Cvar_Register(&r_displayRefresh);
	Cvar_Register(&vid_usedesktopres);
	Cvar_Register(&vid_win_borderless);
	Cvar_Register(&gl_multisamples);
	Cvar_Register(&vid_displayNumber);

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

	Q_free(modelist);

	modelist_count = SDL_GetNumDisplayModes(0);
	modelist = Q_calloc(modelist_count, sizeof(*modelist));

	for (i = 0; i < modelist_count; i++) {
		SDL_GetDisplayMode(0, i, &modelist[i]);
	}
}

static void VID_SetupResolution(void)
{
	SDL_DisplayMode display_mode;

	if (r_fullscreen.integer == 1) {
		if (vid_usedesktopres.integer == 1) {
			if (SDL_GetDesktopDisplayMode(VID_DisplayNumber(true), &display_mode) == 0) {
				glConfig.vidWidth = display_mode.w;
				glConfig.vidHeight = display_mode.h;
				glConfig.displayFrequency = display_mode.refresh_rate;
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

	for (i = 0; i < modelist_count; i++) {
		if (modelist[i].w == vid_width.integer &&
		    modelist[i].h == vid_height.integer &&
		    modelist[i].refresh_rate == r_displayRefresh.integer) {
			Com_DPrintf("MATCHED: %dx%d hz:%d\n", modelist[i].w, modelist[i].h, modelist[i].refresh_rate);
			return i;
		}
	}

	return -1;
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

#ifdef __APPLE__
	SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#endif

	VID_SDL_InitSubSystem();
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

	if (r_fullscreen.integer > 0 && vid_usedesktopres.integer != 1) {
		int index;

		index = VID_GetCurrentModeIndex();

		/* FIXME: Make a pre-check if the values render a valid video mode before attempting to switch (when vid_usedesktopres != 1) !! */
		if (index < 0) {
			Com_Printf("Couldn't find a matching video mode for the selected values, check video settings!\n");
		} else {
			if (SDL_SetWindowDisplayMode(sdl_window, &modelist[index]) != 0) {
				Com_Printf("sdl error: %s\n", SDL_GetError());
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

void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	SDL_SetWindowGammaRamp(sdl_window, ramps, ramps+256,ramps+512);
	vid_hwgamma_enabled = true;
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
		Cvar_SetValue(&r_conwidth, (float)Q_atoi(COM_Argv(i + 1)));
	}

	if ((i = COM_CheckParm("-conheight")) && i + 1 < COM_Argc()) {
		Cvar_SetValue(&r_conheight, (float)Q_atoi(COM_Argv(i + 1)));
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

void VID_RegisterCommands(void) 
{
	if (!host_initialized) {
		Cmd_AddCommand("vid_gfxinfo", GfxInfo_f);
		Cmd_AddCommand("vid_restart", VID_Restart_f);
		Cmd_AddCommand("vid_displaylist", VID_DisplayList_f);
	}
}

static void VID_UpdateConRes(void)
{
	// Default
	if (r_conwidth.integer == 0 || r_conheight.integer == 0) {
		vid.width = vid.conwidth = bound(320, (int)(glConfig.vidWidth/r_conscale.value), glConfig.vidWidth);
		vid.height = vid.conheight = bound(200, (int)(glConfig.vidHeight/r_conscale.value), glConfig.vidHeight);;
	} else {
		// User specified, use that but check boundaries
		vid.width  = vid.conwidth  = bound(320, r_conwidth.integer, glConfig.vidWidth);
		vid.height = vid.conheight = bound(200, r_conheight.integer, glConfig.vidHeight);
		Cvar_SetValue(&r_conwidth, vid.conwidth);
		Cvar_SetValue(&r_conheight, vid.conheight);
	}

	vid.numpages = 2; // ??
	Draw_AdjustConback();
	vid.recalc_refdef = 1;
}

static void conres_changed_callback (cvar_t *var, char *string, qbool *cancel)
{
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
}

