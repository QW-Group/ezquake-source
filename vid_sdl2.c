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
#else
#include <GL/gl.h>
#endif

#include "ezquake-icon.c"
#include "keys.h"
#include "tr_types.h"
#include "input.h"
#include "rulesets.h"
#include "utils.h"

#define	WINDOW_CLASS_NAME	"ezQuake"

static SDL_Window       *sdl_window;
static SDL_GLContext    *sdl_context;

//
// cvars
//

int opengl_initialized = 0;
static qbool mouse_active = false;
qbool vid_hwgamma_enabled = false;

static void in_raw_callback(cvar_t *var, char *value, qbool *cancel);
static void in_grab_windowed_mouse_callback(cvar_t *var, char *value, qbool *cancel);

cvar_t in_raw                 = {"in_raw", "1", CVAR_ARCHIVE | CVAR_SILENT, in_raw_callback};
cvar_t in_grab_windowed_mouse = {"in_grab_windowed_mouse", "1", CVAR_ARCHIVE | CVAR_SILENT, in_grab_windowed_mouse_callback};
cvar_t vid_vsync_lag_fix      = {"vid_vsync_lag_fix", "0"};
cvar_t vid_vsync_lag_tweak    = {"vid_vsync_lag_tweak", "1.0"};

extern cvar_t sys_inactivesleep;

qbool mouseinitialized = false; // unfortunately non static, lame...
int mx, my;
static int old_x = 0, old_y = 0;

qbool ActiveApp = true;
qbool Minimized = false;

double vid_vsync_lag;
double vid_last_swap_time;

//
// function declaration
//

static void GrabMouse(qbool grab, qbool raw);

static void in_raw_callback(cvar_t *var, char *value, qbool *cancel)
{
	GrabMouse(mouse_active, (atoi(value) > 0 ? true : false));
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
	SDL_ShowCursor(grab ? SDL_DISABLE : SDL_ENABLE);
	SDL_SetCursor(NULL); /* Force rewrite of it */

	mouse_active = grab;
}

void IN_Commands(void)
{
}

void IN_StartupMouse(void)
{
	Cvar_Register (&in_raw);
	Cvar_Register (&in_grab_windowed_mouse);
	mouseinitialized = true;
	Com_Printf("SDL mouse initialized\n");
}

void IN_ActivateMouse(void)
{
	GrabMouse(true, in_raw.integer);
}

void IN_DeactivateMouse(void)
{
	GrabMouse(false, in_raw.integer);
}

int IN_GetMouseRate(void)
{
    /* can we implement this with SDL? */
    return -1;
}

void IN_Frame(void)
{
	if (!ActiveApp || Minimized || (!r_fullscreen.integer && (key_dest != key_game || cls.state != ca_active)))
		IN_DeactivateMouse ();
	else
		IN_ActivateMouse();
}

void IN_Restart_f(void)
{
	qbool old_mouse_active = mouse_active;

	IN_Shutdown();
	IN_Init();

	// if mouse was active before restart, try to re-activate it
	if (old_mouse_active)
		IN_ActivateMouse();
}

static void window_event(SDL_WindowEvent *event)
{
	int flags = SDL_GetWindowFlags(sdl_window);
	extern qbool scr_skipupdate;

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
				Cvar_SetValue(&vid_ypos, event->data2);
				Cvar_SetValue(&vid_xpos, event->data1);
			}
			break;

		case SDL_WINDOWEVENT_RESIZED:
			if (!(flags & SDL_WINDOW_FULLSCREEN)) {
				glConfig.vidWidth = event->data1;
				glConfig.vidHeight = event->data2;
				glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight;
				if (r_win_save_size.integer) {
					Cvar_LatchedSetValue(&r_win_width, event->data1);
					Cvar_LatchedSetValue(&r_win_height, event->data2);
				}
			}
			break;
	}
}

// FIXME: APPLE K_CMD, K_F13-15 etc...

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
    KP_UPARROW,     KP_PGUP,        KP_INS,         KP_DEL,         0,              K_MENU,         0,                  0,              // 6
    0,              0,              0,              0,              0,              0,              0,                  0,
    0,              0,              0,              0,              0,              0,              K_MENU,             0,              // 7
    K_LCTRL,        K_LSHIFT,       K_LALT,         K_LWIN,         K_RCTRL,        K_RSHIFT,       K_RALT,             K_RWIN,         // E
};

static void keyb_event(SDL_KeyboardEvent *event)
{
	byte result;

	if (event->keysym.scancode < 120)
		result = scantokey[event->keysym.scancode];
	else if (event->keysym.scancode >= 224 && event->keysym.scancode < 224 + 8)
		result = scantokey[event->keysym.scancode - 104];
	else
		result = 0;

	if (result == 0) {
		Com_Printf("%s: unknown scancode %d\n", __func__, event->keysym.scancode);
		return;
	}

	if (result == K_LALT || result == K_RALT)
		Key_Event(K_ALT, event->state);
	else if (result == K_LCTRL || result == K_RCTRL)
		Key_Event(K_CTRL, event->state);
	else if (result == K_LSHIFT || result == K_RSHIFT)
		Key_Event(K_SHIFT, event->state);

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
	SDL_Event	event;

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
		case SDL_MOUSEMOTION:
	                if (mouse_active && !SDL_GetRelativeMouseMode()) {
                            mx = old_x - event.motion.x;
                            my = old_y - event.motion.y;
			    old_x = event.motion.x;
			    old_y = event.motion.y;
			    SDL_WarpMouseInWindow(sdl_window, glConfig.vidWidth / 2, glConfig.vidHeight / 2);
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

void Sys_SendKeyEvents (void)
{
	if (!sdl_window)
		return;

	IN_Frame();
	HandleEvents();

	if (sys_inactivesleep.integer > 0) 
	{
		// Yield the CPU a little
		if ((ISPAUSED && (!ActiveApp)) || Minimized || block_drawing)
		{
			SDL_Delay(50);
			scr_skipupdate = 1; // no point to draw anything
		} 
		else if (!ActiveApp) // Delay a bit less if just not active window
		{
			SDL_Delay(20);
		}
	}

	if (mouse_active && SDL_GetRelativeMouseMode()) {
		SDL_GetRelativeMouseState(&mx, &my);
	}
}


/*****************************************************************************/

/*
 ** GLimp_Shutdown
 **
 ** This routine does all OS specific shutdown procedures for the OpenGL
 ** subsystem.  Under OpenGL this means NULLing out the current DC and
 ** HGLRC, deleting the rendering context, and releasing the DC acquired
 ** for the window.  The state structure is also nulled out.
 **
 */
void GLimp_Shutdown(void)
{
	if (!sdl_context || !sdl_window)
		return;

	IN_DeactivateMouse();
	opengl_initialized = 0;

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
}

static int VID_SDL_InitSubSystem(void)
{
	int ret = 0;

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
		ret = SDL_InitSubSystem(SDL_INIT_VIDEO);

	if (ret == -1) 
		Sys_Error("Couldn't initialize SDL video: %s\n", SDL_GetError());

	return ret;
}

// FIXME This is a big mess design wise...
void VID_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register(&vid_vsync_lag_fix);
	Cvar_Register(&vid_vsync_lag_tweak);
	Cvar_ResetCurrentGroup();
}

/*
 ** GLimp_Init
 **
 ** This routine is responsible for initializing the OS specific portions
 ** of OpenGL.
 */
void GLimp_Init( void )
{
	SDL_Surface *icon_surface;
	extern void InitSig(void);

	SDL_DisplayMode display_mode;

	int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_SHOWN;
#ifdef SDL_WINDOW_ALLOW_HIGHDPI
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

	if (r_fullscreen.integer == 1)
		flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_FULLSCREEN;

#if defined(__linux__)
	InitSig();
#endif

	VID_SDL_InitSubSystem();

	sdl_window = SDL_CreateWindow(
			WINDOW_CLASS_NAME,
			vid_xpos.integer,
			vid_ypos.integer,
			r_fullscreen.integer ? r_width.integer : r_win_width.integer,
			r_fullscreen.integer ? r_height.integer : r_win_height.integer,
			flags);

        icon_surface = SDL_CreateRGBSurfaceFrom((void *)ezquake_icon.pixel_data, ezquake_icon.width, ezquake_icon.height, ezquake_icon.bytes_per_pixel * 8,
                ezquake_icon.width * ezquake_icon.bytes_per_pixel,
                0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);

        if (icon_surface) {
            SDL_SetWindowIcon(sdl_window, icon_surface);
            SDL_FreeSurface(icon_surface);
        }

	SDL_SetWindowMinimumSize(sdl_window, 320, 240);

	sdl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_context) {
		Com_Printf("Couldn't create OpenGL context: %s\n", SDL_GetError());
		return;
	}

	if (r_fullscreen.integer) {
		glConfig.vidWidth = r_width.integer;
		glConfig.vidHeight = r_height.integer;
	} else {
		glConfig.vidWidth = r_win_width.integer;
		glConfig.vidHeight = r_win_height.integer;
	}

	if (!SDL_GetWindowDisplayMode(sdl_window, &display_mode))
		glConfig.displayFrequency = display_mode.refresh_rate;
	else
		glConfig.displayFrequency = 0;

	glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight;
	glConfig.colorBits = 24;
	glConfig.depthBits = 8;
	glConfig.stencilBits = 8;

	glConfig.vendor_string         = glGetString(GL_VENDOR);
	glConfig.renderer_string       = glGetString(GL_RENDERER);
	glConfig.version_string        = glGetString(GL_VERSION);
	glConfig.extensions_string     = glGetString(GL_EXTENSIONS);

	v_gamma.modified = true;
	r_swapInterval.modified = true;
	
#if defined(__linux__)
	InitSig(); // not clear why this is at begin & end of function
#endif
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	*x = *y = 0;
	*width  = glConfig.vidWidth;
	*height = glConfig.vidHeight;

	if (cls.state != ca_active)
		glClear (GL_COLOR_BUFFER_BIT);
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

	if (!scr_skipupdate || block_drawing)
	{
		// Multiview - Only swap the back buffer to front when all views have been drawn in multiview.
		if (cl_multiview.value && cls.mvdplayback) 
			if (CURRVIEW != 1)
				return;

		// Normal, swap on each frame.
		GLimp_EndFrame(); 
	}
}


/*
 ** GLimp_EndFrame
 **
 ** Responsible for doing a swapbuffers and possibly for other stuff
 ** as yet to be determined.  Probably better not to make this a GLimp
 ** function and instead do a call to GLimp_SwapBuffers.
 */
void GLimp_EndFrame (void)
{
	double time_before_swap;
	time_before_swap = Sys_DoubleTime();
	SDL_GL_SwapWindow(sdl_window);
	vid_last_swap_time = Sys_DoubleTime();
	vid_vsync_lag = vid_last_swap_time - time_before_swap;
}

/************************************* Window related *******************************/

void VID_SetCaption (char *text)
{
	if (!sdl_window)
		return;

	SDL_SetWindowTitle(sdl_window, text);
}

void VID_NotifyActivity(void)
{
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (ActiveApp || !vid_flashonactivity.value)
		return;

	if (SDL_GetWindowWMInfo(sdl_window, &info) == SDL_TRUE)
	{
		if (info.subsystem == SDL_SYSWM_WINDOWS)
			FlashWindow(info.info.win.window, TRUE);
	}
	else
		Com_DPrintf("Sys_NotifyActivity: SDL_GetWindowWMInfo failed: %s\n", SDL_GetError());
#endif
}

/********************************* CLIPBOARD *********************************/

wchar *Sys_GetClipboardTextW(void)
{
	return SDL_HasClipboardText() ? str2wcs(SDL_GetClipboardText()) : NULL;
}

void Sys_CopyToClipboard(char *text)
{
	SDL_SetClipboardText(text);
}


void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
	SDL_SetWindowGammaRamp(sdl_window, ramps, ramps+256,ramps+512);
	vid_hwgamma_enabled = true;
}

void VID_Minimize (void) 
{
    if (!sdl_window)
        return;

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
	return SDL_GL_GetSwapInterval() == 1;
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

	if (!VID_VSyncIsOn() || !vid_vsync_lag_fix.integer)
		return false;

	if (!glConfig.displayFrequency) {
		Com_Printf("VID_VSyncLagFix: displayFrequency isn't set, can't enable vsync lag fix\n");
		return false;
	}

        // collect statistics so that
        timings[timings_idx] = render_frame_end - render_frame_start;
        timings_idx = (timings_idx + 1) % NUMTIMINGS;
        avg_rendertime = tmin = tmax = 0;
        for (i = 0; i < NUMTIMINGS; i++) {
                if (timings[i] == 0)
                        return false;   // not enough statistics yet
                avg_rendertime += timings[i];
                if (timings[i] < tmin || !tmin)
                        tmax = timings[i];
                if (timings[i] > tmax)
                        tmax = timings[i];
        }
        avg_rendertime /= NUMTIMINGS;
        // if (tmax and tmin differ too much) do_something(); ?
        avg_rendertime = tmax;  // better be on the safe side

	double time_left = vid_last_swap_time + 1.0/glConfig.displayFrequency - Sys_DoubleTime();
	time_left -= avg_rendertime;
	time_left -= vid_vsync_lag_tweak.value * 0.001;
	if (time_left > 0) {
		extern cvar_t sys_yieldcpu;
		if (time_left > 0.001 && sys_yieldcpu.integer)
			Sys_MSleep(min(time_left * 1000, 500));
		return true;    // don't run a frame yet
	}
        return false;
}

