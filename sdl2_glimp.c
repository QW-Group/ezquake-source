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
/*
 * This file contains ALL Linux specific stuff having to do with the
 * OpenGL refresh.
 */

#include "quakedef.h"

#include <SDL.h>
#include <GL/gl.h>

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

typedef enum
{
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;


static void in_raw_callback(cvar_t *var, char *value, qbool *cancel);
static void in_grab_windowed_mouse_callback(cvar_t *var, char *value, qbool *cancel);

cvar_t in_raw                 = {"in_raw", "1", CVAR_ARCHIVE | CVAR_SILENT, in_raw_callback};
cvar_t in_grab_windowed_mouse = {"in_grab_windowed_mouse", "1", CVAR_ARCHIVE | CVAR_SILENT, in_grab_windowed_mouse_callback};

// TODO: implement (SDL_PauseAudio func)
cvar_t sys_inactivesound  = { "sys_inactivesound", "1", CVAR_ARCHIVE };

glwstate_t glw_state;

qbool mouseinitialized = false; // unfortunately non static, lame...
int mx, my;
static int old_x = 0, old_y = 0;

qbool ActiveApp = true;
qbool Minimized = false;

//
// function declaration
//

static void GrabMouse(qbool grab, qbool raw);

qbool QGL_Init(const char *dllname)
{
	qglActiveTextureARB       = 0;
	qglClientActiveTextureARB = 0;
	qglMultiTexCoord2fARB     = 0;

	return true;
}

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
	if (!r_fullscreen.integer && (key_dest != key_game || cls.state != ca_active))
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

	switch (event->event) {
		case SDL_WINDOWEVENT_MINIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
		case SDL_WINDOWEVENT_ENTER:
		case SDL_WINDOWEVENT_LEAVE:
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST:

			if (flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS)) {
				ActiveApp = true;
				Minimized = false;
			} else if (flags & SDL_WINDOW_MINIMIZED) {
				ActiveApp = false;
				Minimized = true;
			} else {
				ActiveApp = false;
				Minimized = false;
			}
			break;

		case SDL_WINDOWEVENT_MOVED:
			if (!(flags & SDL_WINDOW_FULLSCREEN)) {
				int w,h;
				SDL_GetWindowSize(sdl_window, &w, &h);
				Cvar_LatchedSetValue(&r_customwidth, w);
				Cvar_LatchedSetValue(&r_customheight, h);
				Cvar_SetValue(&vid_ypos, event->data2);
				Cvar_SetValue(&vid_xpos, event->data1);
			}
			break;

		case SDL_WINDOWEVENT_RESIZED:
			if (!(flags & SDL_WINDOW_FULLSCREEN)) {
				glConfig.vidWidth = event->data1;
				glConfig.vidHeight = event->data2;
				glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight; 
				Cvar_SetValue(&r_customwidth, event->data1);
				Cvar_SetValue(&r_customheight, event->data2);
			}
			break;
	}
}

#define K_PRINTSCREEN 0
#define K_SCROLLOCK 0
#define K_NUMLOCK 0
#define K_KP_ENTER 0
#define K_KP_END 0
#define K_KP_SLASH 0
#define K_KP_MULTIPLY 0
#define K_KP_DOWNARROW 0
#define K_KP_MINUS 0
#define K_KP_PLUS 0
#define K_KP_PGDN 0
#define K_KP_LEFTARROW 0
#define K_KP_RIGHTARROW 0
#define K_KP_HOME 0
#define K_KP_UPARROW 0
#define K_KP_5 0
#define K_KP_INS  0
#define K_KP_DEL 0
#define K_KP_PGUP 0
#define K_102ND 0
#define K_LWINKEY 0
#define K_RWINKEY 0

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
    K_F7,           K_F8,           K_F9,           K_F10,          K_F11,          K_F12,          K_PRINTSCREEN,      K_SCROLLOCK,    // 4
    K_PAUSE,        K_INS,          K_HOME,         K_PGUP,         K_DEL,          K_END,          K_PGDN,             K_RIGHTARROW,
    K_LEFTARROW,    K_DOWNARROW,    K_UPARROW,      K_NUMLOCK,      K_KP_SLASH,     K_KP_MULTIPLY,  K_KP_MINUS,         K_KP_PLUS,      // 5
    K_KP_ENTER,     K_KP_END,       K_KP_DOWNARROW, K_KP_PGDN,      K_KP_LEFTARROW, K_KP_5,         K_KP_RIGHTARROW,    K_KP_HOME,
    K_KP_UPARROW,   K_KP_PGUP,      K_KP_INS,       K_KP_DEL,       K_102ND,        0,              0,                  0,              // 6
    0,              0,              0,              0,              0,              0,              0,                  0,
    0,              0,              0,              0,              0,              0,              K_MENU,             0,              // 7
    K_LCTRL,        K_LSHIFT,       K_LALT,         K_LWINKEY,      K_RCTRL,        K_RSHIFT,       K_RALT,             K_RWINKEY,      // E
};

static void key_event(SDL_KeyboardEvent *event)
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
	case SDL_BUTTON_X1:
		key = K_MOUSE4;
		break;
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
			key_event(&event.key);
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

	/*if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO) {
		SDL_Quit();
	} else */{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	memset(&glConfig, 0, sizeof(glConfig));
}

/*
 ** GLW_StartDriverAndSetMode
 */

/*
 ** GLW_SetMode
 */
int GLW_SetMode(const char *drivername, int mode, qbool fullscreen)
{
	GrabMouse(true, in_raw.integer);

	return RSERR_OK;
}


static int VID_SDL_InitSubSystem(void)
{
	int ret;

	ret = SDL_WasInit(SDL_INIT_EVERYTHING);
	if (ret == 0)
		ret = SDL_Init(SDL_INIT_VIDEO);
	else if (!(ret & SDL_INIT_VIDEO))
		ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
	else
		ret = 0;

	if (ret == -1) 
		Sys_Error("Couldn't initialize SDL video: %s\n", SDL_GetError());

	return ret;
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

	int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_ResetCurrentGroup();

#if defined(__linux__)
	InitSig();
#endif

	VID_SDL_InitSubSystem();

	sdl_window = SDL_CreateWindow(WINDOW_CLASS_NAME, vid_xpos.integer, vid_ypos.integer, r_customwidth.integer, r_customheight.integer, flags);
        icon_surface = SDL_CreateRGBSurfaceFrom((void *)ezquake_icon.pixel_data, ezquake_icon.width, ezquake_icon.height, ezquake_icon.bytes_per_pixel * 8,
                ezquake_icon.width * ezquake_icon.bytes_per_pixel,
                0xFF000000,0x00FF0000,0x0000FF00,0x000000FF);

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

	glConfig.vidWidth = r_customwidth.integer;
	glConfig.vidHeight = r_customheight.integer;
	glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight; 

	glConfig.colorBits = 24;
	glConfig.depthBits = 8;
	glConfig.stencilBits = 8;

	glConfig.vendor_string         = glGetString(GL_VENDOR);
	glConfig.renderer_string       = glGetString(GL_RENDERER);
	glConfig.version_string        = glGetString(GL_VERSION);
	glConfig.extensions_string     = glGetString(GL_EXTENSIONS);
	
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
		if (r_swapInterval.integer > 0) {
			if (SDL_GL_SetSwapInterval(1)) {
				Con_Printf("vsync: Failed to enable vsync...\n");
			}
		}
		else if (r_swapInterval.integer <= 0) {
			if (SDL_GL_SetSwapInterval(0)) {
				Con_Printf("vsync: Failed to disable vsync...\n");
			}
		}
		r_swapInterval.modified = false;
        }

	if (!scr_skipupdate || block_drawing)
	{
		// Multiview - Only swap the back buffer to front when all views have been drawn in multiview.
		if (cl_multiview.value && cls.mvdplayback) 
		{
			if (CURRVIEW == 1)
			{
				GLimp_EndFrame();
			}
		}
		else 
		{
			// Normal, swap on each frame.
			GLimp_EndFrame(); 
		}
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
	SDL_GL_SwapWindow(sdl_window);
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
}

/********************************* CLIPBOARD *********************************/

wchar *Sys_GetClipboardTextW(void)
{
	if (SDL_HasClipboardText())
		return str2wcs(SDL_GetClipboardText());
	else
		return NULL;
}

void Sys_CopyToClipboard(char *text)
{
	SDL_SetClipboardText(text);
}


void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
    /* stub */
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
}
