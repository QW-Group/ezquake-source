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

#include <SDL.h>
#include <GL/gl.h>

//#include "ezquake.xpm"
#include "quakedef.h"
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
	 mt_none = 0,
	 mt_normal
} mousetype_t;

typedef enum
{
	RSERR_OK,
	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,
	RSERR_UNKNOWN
} rserr_t;

cvar_t in_mouse           = { "in_mouse",    "1", CVAR_ARCHIVE | CVAR_LATCH }; // NOTE: "1" is mt_normal
cvar_t in_nograb          = { "in_nograb",   "0", CVAR_LATCH }; // this is strictly for developers
cvar_t r_allowSoftwareGL  = { "vid_allowSoftwareGL", "0", CVAR_LATCH };   // don't abort out if the pixelformat claims software


glwstate_t glw_state;

qbool mouseinitialized = false; // unfortunately non static, lame...
int mx, my;

qbool ActiveApp = true;
qbool Minimized = false;

//
// function declaration
//

qbool QGL_Init(const char *dllname)
{
	qglActiveTextureARB       = 0;
	qglClientActiveTextureARB = 0;
	qglMultiTexCoord2fARB     = 0;

	return true;
}

void QGL_Shutdown(void)
{
}


static void GrabMouse(qbool grab)
{
	SDL_bool relative = grab;// && !(Key_GetDest() & KEY_MENU);
	int cursor = r_fullscreen.integer ? SDL_DISABLE : SDL_ENABLE;

	SDL_SetWindowGrab(sdl_window, (SDL_bool)grab);
	SDL_SetRelativeMouseMode(relative);
	SDL_GetRelativeMouseState(NULL, NULL);
	SDL_ShowCursor(cursor);
}


void IN_Commands(void)
{
}

void IN_StartupMouse(void)
{
	Com_Printf("SDL mouse initialized.\n");
	mouseinitialized = true;
	Cvar_Register (&in_nograb);
}

void IN_ActivateMouse(void)
{
	if (!mouseinitialized || !sdl_window || mouse_active)
		return;

	if (!in_nograb.value)
		GrabMouse(true);
	mouse_active = true;
}

void IN_DeactivateMouse(void)
{
	if (!mouseinitialized || !sdl_window || !mouse_active)
		return;

	if (!in_nograb.value)
		GrabMouse(false);
	mouse_active = false;
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
			// currently ignoring window position
			break;

		case SDL_WINDOWEVENT_RESIZED:
			if (!(flags & SDL_WINDOW_FULLSCREEN)) {
				glConfig.vidWidth = event->data1;
				glConfig.vidHeight = event->data2;
				glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight; 
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
			//UI_MouseEvent(event.motion.x, event.motion.y);
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

	if (SDL_GetRelativeMouseMode()) {
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

	QGL_Shutdown();
}

/*
 ** GLW_StartDriverAndSetMode
 */
#if 0
int GLW_SetMode(const char *drivername, int mode, qbool fullscreen);

static qbool GLW_StartDriverAndSetMode(const char *drivername, int mode, qbool fullscreen)
{
	rserr_t err;

	if (fullscreen && in_nograb.value)
	{
		ST_Printf(PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		Cvar_Set(&r_fullscreen, "0");
		r_fullscreen.modified = false;
		fullscreen = false;
	}

	err = GLW_SetMode(drivername, mode, fullscreen);

	switch (err)
	{
		case RSERR_INVALID_FULLSCREEN:
			ST_Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
			return false;
		case RSERR_INVALID_MODE:
			ST_Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
			return false;
		case RSERR_UNKNOWN:
			return false;
		default:
			break;
	}
	opengl_initialized = 1;
	return true;
}
#endif

/*
 ** GLW_SetMode
 */
int GLW_SetMode(const char *drivername, int mode, qbool fullscreen)
{
	GrabMouse(true);

	mouse_active = true; /* ?? */

	return RSERR_OK;
}

#if 0
/*
 ** GLW_LoadOpenGL
 **
 ** GLimp_win.c internal function that that attempts to load and use
 ** a specific OpenGL DLL.
 */
static qbool GLW_LoadOpenGL( const char *name )
{
	qbool fullscreen;

	if ( QGL_Init( name ) )
	{
		fullscreen = r_fullscreen.integer;

		// create the window and set up the context
		if ( !GLW_StartDriverAndSetMode( name, r_mode.integer, fullscreen ) )
		{
			if (r_mode.integer != 3)
			{
				if ( !GLW_StartDriverAndSetMode( name, 3, fullscreen ) )
				{
					goto fail;
				}
			}
			else
			{
				goto fail;
			}
		}

		return true;
	}
	else
	{
		ST_Printf( PRINT_ALL, "failed\n" );
	}
fail:

	QGL_Shutdown();

	return false;
}
#endif

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
	extern void InitSig(void);

	int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
#if 0
	qbool attemptedlibGL = false;
	qbool success = false;
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_ResetCurrentGroup();

	InitSig();

	VID_SDL_InitSubSystem();

	sdl_window = SDL_CreateWindow(WINDOW_CLASS_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);

	SDL_SetWindowMinimumSize(sdl_window, 320, 240);

	sdl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_context) {
		Com_Printf("Couldn't create OpenGL context: %s\n", SDL_GetError());
		return;
	}

	glConfig.vidWidth = 640;
	glConfig.vidHeight = 480;
	glConfig.windowAspect = (float)glConfig.vidWidth / glConfig.vidHeight; 

	glConfig.colorBits = 24;
	glConfig.depthBits = 8;
	glConfig.stencilBits = 8;

#if 0
	//
	// load and initialize the specific OpenGL driver
	//
	if (!GLW_LoadOpenGL(r_glDriver.string))
	{
		if (!strcasecmp( r_glDriver.string, OPENGL_DRIVER_NAME))
			attemptedlibGL = true;

		if (!attemptedlibGL && !success)
		{
			attemptedlibGL = true;
			if (GLW_LoadOpenGL( OPENGL_DRIVER_NAME))
			{
				Cvar_Set( &r_glDriver, OPENGL_DRIVER_NAME );
				r_glDriver.modified = false;
				success = true;
			}
		}

		if (!success)
			ST_Printf(PRINT_ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem\n");
	}
#endif

	glConfig.vendor_string         = glGetString(GL_VENDOR);
	glConfig.renderer_string       = glGetString(GL_RENDERER);
	glConfig.version_string        = glGetString(GL_VERSION);
	glConfig.extensions_string     = glGetString(GL_EXTENSIONS);
	
	InitSig(); // not clear why this is at begin & end of function
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
#if 0
	if (r_swapInterval.modified) {
                if (!swapInterval)
                        Con_Printf("Warning: No vsync handler found...\n");
                else
                {
                        if (r_swapInterval.integer > 0) {
                                if (swapInterval(1)) {
                                        Con_Printf("vsync: Failed to enable vsync...\n");
                                }
                        }
                        else if (r_swapInterval.integer <= 0) {
                                if (swapInterval(0)) {
                                        Con_Printf("vsync: Failed to disable vsync...\n");
                                }
                        }
                        r_swapInterval.modified = false;
                }
        }
#endif

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

#define SYS_CLIPBOARD_SIZE		256
//static wchar clipboard_buffer[SYS_CLIPBOARD_SIZE] = {0};

wchar *Sys_GetClipboardTextW(void)
{
	return NULL;
}

void Sys_CopyToClipboard(char *text)
{
}


void VID_SetDeviceGammaRamp (unsigned short *ramps)
{
    /* stub */
}
