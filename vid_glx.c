/*
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

	$Id: vid_glx.c,v 1.42 2007-10-04 13:48:09 dkure Exp $
*/
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "quakedef.h"


#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef WITH_VMODE
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef WITH_DGA
#include <X11/extensions/xf86dga.h>
#endif

#ifdef WITH_KEYMAP
#include "keymap.h"
extern void IN_Keycode_Print_f( XKeyEvent *ev, qbool ext, qbool down, int key );
#endif // WITH_KEYMAP

#ifdef WITH_EVDEV
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#endif

extern int ctrlDown, shiftDown, altDown;

static Display *vid_dpy = NULL;
static Window vid_window;
static GLXContext ctx = NULL;

int mx, my;
static qbool input_grabbed = false;

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask)

unsigned short *currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qbool vid_gammaworks = false;
qbool vid_hwgamma_enabled = false;
qbool customgamma = false;

static int scr_width, scr_height, scrnum;

#ifdef WITH_EVDEV
static int evdev_mouse;
static char evdev_device[32];
static int evdev_fd;
static int evdev_mt;
static pthread_t evdev_thread;
void EvDev_UpdateMouse(void *v);
#endif

#ifdef WITH_DGA
static int dgamouse, dgakeyb;
#endif

#ifdef WITH_VMODE
static qbool vidmode_ext = false;
static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qbool vidmode_active = false;
static double X_vrefresh_rate = 0;
static int best_fit = 0;
static Window minimized_window;
XF86VidModeModeInfo newvmode;
static int new_vidmode = 0;
#endif

static int vid_minimized = 0;

cvar_t	vid_ref = {"vid_ref", "gl", CVAR_ROM};
cvar_t	vid_mode = {"vid_mode", "0"};

cvar_t  auto_grabmouse = {"auto_grabmouse",
#ifdef NDEBUG
	 "1"
#else
	"0"
#endif
};

cvar_t	vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};


const char *glx_extensions=NULL;

PFNGLXGETVIDEOSYNCSGIPROC _glXGetVideoSyncSGI;
PFNGLXWAITVIDEOSYNCSGIPROC _glXWaitVideoSyncSGI;

// extern int glXGetVideoSyncSGI (unsigned int *);
// extern int glXWaitVideoSyncSGI (int, int, unsigned int *);

cvar_t	vid_vsync = {"vid_vsync", "0"};

void GL_Init_GLX(void);
//void VID_Minimize_f(void);

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height) {}

void D_EndDirectRect (int x, int y, int width, int height) {}

/************************************* COMPATABILITY *************************************/

void VID_UnlockBuffer() {}

void VID_LockBuffer() {}

void VID_SetCaption (char *text)
{
	if (!vid_dpy)
		return;
	XStoreName (vid_dpy, vid_window, text);
}

/************************************* KEY MAPPINGS *************************************/

static int XLateKey(XKeyEvent *ev) {
	int key, kp;
	//char buf[64];
	KeySym keysym;

	key = 0;
	kp = (int) cl_keypad.value;

	keysym = XLookupKeysym (ev, 0);
	//XLookupString(ev, buf, sizeof (buf), &keysym, 0);

	switch(keysym) {
		case XK_Scroll_Lock:	key = K_SCRLCK; break;

		case XK_Caps_Lock:		key = K_CAPSLOCK; break;

		case XK_Num_Lock:		key = kp ? KP_NUMLOCK : K_PAUSE; break;

		case XK_KP_Page_Up:		key = kp ? KP_PGUP : K_PGUP; break;
		case XK_Page_Up:		key = K_PGUP; break;

		case XK_KP_Page_Down:	key = kp ? KP_PGDN : K_PGDN; break;
		case XK_Page_Down:		key = K_PGDN; break;

		case XK_KP_Home:		key = kp ? KP_HOME : K_HOME; break;
		case XK_Home:			key = K_HOME; break;

		case XK_KP_End:			key = kp ? KP_END : K_END; break;
		case XK_End:			key = K_END; break;

		case XK_KP_Left:		key = kp ? KP_LEFTARROW : K_LEFTARROW; break;
		case XK_Left:			key = K_LEFTARROW; break;

		case XK_KP_Right:		key = kp ? KP_RIGHTARROW : K_RIGHTARROW; break;
		case XK_Right:			key = K_RIGHTARROW; break;

		case XK_KP_Down:		key = kp ? KP_DOWNARROW : K_DOWNARROW; break;

		case XK_Down:			key = K_DOWNARROW; break;

		case XK_KP_Up:			key = kp ? KP_UPARROW : K_UPARROW; break;

		case XK_Up:				key = K_UPARROW; break;

		case XK_Escape:			key = K_ESCAPE; break;

		case XK_KP_Enter:		key = kp ? KP_ENTER : K_ENTER; break;

		case XK_Return:			key = K_ENTER; break;

		case XK_Tab:			key = K_TAB; break;

		case XK_F1:				key = K_F1; break;

		case XK_F2:				key = K_F2; break;

		case XK_F3:				key = K_F3; break;

		case XK_F4:				key = K_F4; break;

		case XK_F5:				key = K_F5; break;

		case XK_F6:				key = K_F6; break;

		case XK_F7:				key = K_F7; break;

		case XK_F8:				key = K_F8; break;

		case XK_F9:				key = K_F9; break;

		case XK_F10:			key = K_F10; break;

		case XK_F11:			key = K_F11; break;

		case XK_F12:			key = K_F12; break;

		case XK_BackSpace:		key = K_BACKSPACE; break;

		case XK_KP_Delete:		key = kp ? KP_DEL : K_DEL; break;
		case XK_Delete:			key = K_DEL; break;

		case XK_Pause:			key = K_PAUSE; break;

		case XK_Shift_L:		key = K_LSHIFT; break;
		case XK_Shift_R:		key = K_RSHIFT; break;

		case XK_Execute: 
		case XK_Control_L:		key = K_LCTRL; break;
		case XK_Control_R:		key = K_RCTRL; break;

		case XK_Alt_L:	
		case XK_Meta_L:			key = K_LALT; break;
		case XK_Alt_R:	
		case XK_Meta_R:			key = K_RALT; break;

		case XK_Super_L:		key = K_LWIN; break;
		case XK_Super_R:		key = K_RWIN; break;
		case XK_Menu:			key = K_MENU; break;

		case XK_KP_Begin:		key = kp ? KP_5 : '5'; break;

		case XK_KP_Insert:		key = kp ? KP_INS : K_INS; break;
		case XK_Insert:			key = K_INS; break;

		case XK_KP_Multiply:	key = kp ? KP_STAR : '*'; break;

		case XK_KP_Add:			key = kp ? KP_PLUS : '+'; break;

		case XK_KP_Subtract:	key = kp ? KP_MINUS : '-'; break;

		case XK_KP_Divide:		key = kp ? KP_SLASH : '/'; break;


		default:
			if (keysym >= 32 && keysym <= 126) {
				key = tolower(keysym);
			}
			break;
	}
	return key;
}

// hide the mouse cursor when in window
static Cursor CreateNullCursor(Display *display, Window root) {
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(display, root, 1, 1, 1);
	xgc.function = GXclear;
	gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask,
								 &dummycolour,&dummycolour, 0,0);
	XFreePixmap(display,cursormask);
	XFreeGC(display,gc);
	return cursor;
}

static void install_grabs(void) {
	int MajorVersion, MinorVersion;
#ifdef WITH_DGA
	int DGAflags = 0;
#endif

	input_grabbed = true;

    // don't show mouse cursor icon
    XDefineCursor(vid_dpy, vid_window, CreateNullCursor(vid_dpy, vid_window));

	XGrabPointer(vid_dpy, vid_window, True, 0, GrabModeAsync, GrabModeAsync, vid_window, None, CurrentTime);

#ifdef WITH_DGA
	if (!COM_CheckParm("-nomdga"))
		DGAflags |= XF86DGADirectMouse;
	if (!COM_CheckParm("-nokdga"))
		DGAflags |= XF86DGADirectKeyb;

	if (!COM_CheckParm("-nodga") && DGAflags && XF86DGAQueryVersion(vid_dpy, &MajorVersion, &MinorVersion)) {
		// let us hope XF86DGADirectMouse will work
		XF86DGADirectVideo(vid_dpy, DefaultScreen(vid_dpy), DGAflags);
		if (DGAflags & XF86DGADirectMouse)
			dgamouse = 1;
		if (DGAflags & XF86DGADirectKeyb)
			dgakeyb = 1;

		XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0, 0, 0); // oldman: this should be here really
	} 
	else
#endif
	{
		XWarpPointer(vid_dpy, None, vid_window,
			0, 0, 0, 0,
			vid.width / 2, vid.height / 2);
	}

	XGrabKeyboard(vid_dpy, vid_window,
		False,
		GrabModeAsync, GrabModeAsync,
		CurrentTime);
}

static void uninstall_grabs(void) {
	input_grabbed = false;

#ifdef WITH_DGA
	if (dgamouse || dgakeyb) {
		XF86DGADirectVideo(vid_dpy, DefaultScreen(vid_dpy), 0);
		dgamouse = dgakeyb = 0;
	}
#endif

	XUngrabPointer(vid_dpy, CurrentTime);
	XUngrabKeyboard(vid_dpy, CurrentTime);

	// show cursor again
	XUndefineCursor(vid_dpy, vid_window);
}

static void GetEvent(void) {
	int key;
	XEvent event;
	qbool grab_input;

	if (!vid_dpy)
		return;

	XNextEvent(vid_dpy, &event);

	switch (event.type) {
	case KeyPress:
	case KeyRelease:
		key = XLateKey(&event.xkey);
	  if (key == K_CTRL  || key == K_LCTRL  || key == K_RCTRL)
      ctrlDown  = event.type == KeyPress;
	  if (key == K_SHIFT || key == K_LSHIFT || key == K_RSHIFT)
	    shiftDown = event.type == KeyPress;
	  if (key == K_ALT   || key == K_LALT   || key == K_RALT)
		  altDown   = event.type == KeyPress;

#ifdef WITH_KEYMAP
		// if set, print the current Key information
		if (cl_showkeycodes.value > 0) {
			IN_Keycode_Print_f (&event.xkey, false, event.type == KeyPress, key);
		}
#endif // WITH_KEYMAP

		Key_Event(key, event.type == KeyPress);
		break;

	case MotionNotify:
		if (input_grabbed) {
	#ifdef WITH_EVDEV
			if (evdev_mouse) break;
	#endif
	#ifdef WITH_DGA
			if (dgamouse) {
				mx += event.xmotion.x_root;
				my += event.xmotion.y_root;
			} else
	#endif
			{
				mx = event.xmotion.x - (vid.width / 2);
				my = event.xmotion.y - (vid.height / 2);
				// move the mouse to the window center again
				XSelectInput(vid_dpy, vid_window, X_MASK & ~PointerMotionMask);
				XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0, (vid.width / 2), (vid.height / 2));
				XSelectInput(vid_dpy, vid_window, X_MASK);
			}
		}
		break;

	case ButtonPress:
	case ButtonRelease:
	#ifdef WITH_EVDEV
		if (evdev_mouse) break;
	#endif
		switch (event.xbutton.button) {
		case 1:
			Key_Event(K_MOUSE1, event.type == ButtonPress); break;
		case 2:
			Key_Event(K_MOUSE3, event.type == ButtonPress); break;
		case 3:
			Key_Event(K_MOUSE2, event.type == ButtonPress); break;
		case 4:
			Key_Event(K_MWHEELUP, event.type == ButtonPress); break;
		case 5:
			Key_Event(K_MWHEELDOWN, event.type == ButtonPress); break;			
		case 6:
			Key_Event(K_MOUSE4, event.type == ButtonPress); break;
		case 7:
			Key_Event(K_MOUSE5, event.type == ButtonPress); break;
		}
		break;
	}

#ifdef WITH_VMODE
    grab_input = _windowed_mouse.value != 0 || vidmode_active;
#else
    grab_input = _windowed_mouse.value != 0;
#endif

	if (grab_input && !input_grabbed) {
		/* grab the pointer */
		install_grabs();
	} else if (!grab_input && input_grabbed) {
		/* ungrab the pointer */
		uninstall_grabs();
	}
}

void signal_handler(int sig) {
	printf("Received signal %d, exiting...\n", sig);
	VID_Shutdown();
	Sys_Quit();
	exit(0);
}

void InitSig(void) {
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/******************************* GLX EXTENSIONS *******************************/

qbool CheckGLXExtension (const char *extension) {
	const char *start;
	char *where, *terminator;

	if (!glx_extensions && !(glx_extensions = glXQueryExtensionsString (vid_dpy, scrnum)))
		return false;
	
	if (!extension || *extension == 0 || strchr (extension, ' '))
		return false;

	for (start = glx_extensions; where = strstr(start, extension); start = terminator) {
		terminator = where + strlen (extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}
	return false;
}

void CheckVsyncControlExtensions(void) {
    if (!COM_CheckParm("-noswapctrl") && CheckGLXExtension("GLX_SGI_video_sync")) {
		Com_Printf("Vsync control extensions found\n");
		Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
		Cvar_Register (&vid_vsync);
		Cvar_ResetCurrentGroup();
    }
}

void GL_Init_GLX(void) {
	glx_extensions = glXQueryExtensionsString (vid_dpy, scrnum);
	if (COM_CheckParm("-gl_ext"))
		Com_Printf("GLX_EXTENSIONS: %s\n", glx_extensions);

	CheckVsyncControlExtensions();

	_glXGetVideoSyncSGI = (PFNGLXGETVIDEOSYNCSGIPROC) GL_GetProcAddress("glXGetVideoSyncSGI");
	_glXWaitVideoSyncSGI = (PFNGLXWAITVIDEOSYNCSGIPROC) GL_GetProcAddress("glXWaitVideoSyncSGI");
}

/************************************* HW GAMMA *************************************/

void VID_ShiftPalette(unsigned char *p) {}

void InitHWGamma (void)
{
	int xf86vm_gammaramp_size;

	if (COM_CheckParm("-nohwgamma") && (!strncasecmp(Rulesets_Ruleset(), "MTFL", 4))) // FIXME
		return;

	XF86VidModeGetGammaRampSize(vid_dpy, scrnum, &xf86vm_gammaramp_size);
	
	vid_gammaworks = (xf86vm_gammaramp_size == 256);

	if (vid_gammaworks){
		XF86VidModeGetGammaRamp(vid_dpy,scrnum,xf86vm_gammaramp_size,
			systemgammaramp[0], systemgammaramp[1], systemgammaramp[2]);
	}
	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks; // && fullscreen?
}

void VID_SetDeviceGammaRamp (unsigned short *ramps) {
	if (vid_gammaworks) {
		currentgammaramp = ramps;
		if (vid_hwgamma_enabled) {
			XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, ramps, ramps + 256, ramps + 512);
			customgamma = true;
		}
	}
}

void RestoreHWGamma (void) {
	if (vid_gammaworks && customgamma) {
		customgamma = false;
		XF86VidModeSetGammaRamp(vid_dpy, scrnum, 256, systemgammaramp[0], systemgammaramp[1], systemgammaramp[2]);
	}
}

/************************************* GL *************************************/

void WaitForVSync (void) { // thanks to str-
	if(vid_vsync.value) {
		double sanity_time = Sys_DoubleTime() + 0.05;
		unsigned int count, latest;

		_glXGetVideoSyncSGI(&count);

		while(Sys_DoubleTime() < sanity_time) {
			_glXGetVideoSyncSGI(&latest);

			if(latest != count) {
				break;
			}
		}
	}
}

void GL_BeginRendering (int *x, int *y, int *width, int *height) {
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

void GL_EndRendering (void) {
	static qbool old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks;
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	if (vid_minimized) {
		 usleep(10*1000);
		 return;
	}

	WaitForVSync();

	glFlush(); /* no need for this ??? glXSwapBuffers calls it as well */
	glXSwapBuffers(vid_dpy, vid_window);
}

/************************************* VID MINIMIZE *************************************/

void VID_Minimize_f(void) {
#ifdef WITH_VMODE
	if (vidmode_active) {
		XSetWindowAttributes attr;
		Window root = RootWindow(vid_dpy, scrnum);
		unsigned long mask;

		attr.background_pixel = 0;
		attr.border_pixel = 0;
		attr.event_mask = X_MASK;
		mask = CWBackPixel | CWBorderPixel | CWEventMask;
		
		minimized_window = XCreateWindow(vid_dpy, root, 0, 0, 1, 1,0, CopyFromParent, InputOutput, CopyFromParent, mask, &attr);
		XMapWindow(vid_dpy, minimized_window);
		XIconifyWindow(vid_dpy, minimized_window, scrnum);

		switch (cls.state) {
		case ca_disconnected:
			XStoreName(vid_dpy, minimized_window, "ezQuake");
			XSetIconName(vid_dpy, minimized_window, "ezQuake");
			break;

		case ca_demostart:
		case ca_connected:
		case ca_onserver:
		case ca_active:
			XStoreName(vid_dpy, minimized_window, va("ezQuake - %s", cls.servername));
			XSetIconName(vid_dpy, minimized_window, va("ezQuake - %s", cls.servername));
			break;
		}
		
		XUnmapWindow(vid_dpy, vid_window);
		XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
	} else
#endif
	XIconifyWindow(vid_dpy, vid_window, scrnum);
}


/************************************* VID SHUTDOWN *************************************/

void VID_Shutdown(void) {
#ifdef WITH_EVDEV
	if (evdev_mouse)
		close(evdev_fd);
#endif

	if (!ctx)
		return;

	uninstall_grabs();
	
	RestoreHWGamma();

#ifdef WITH_VMODE
	if (vid_dpy) {
		glXDestroyContext(vid_dpy, ctx);
		if (vid_window)
			XDestroyWindow(vid_dpy, vid_window);
		if (vidmode_active)
			XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[0]);
		if (new_vidmode && vidmode_active)
			XF86VidModeDeleteModeLine(vid_dpy, scrnum, &newvmode);
		XCloseDisplay(vid_dpy);
		vidmode_active = false;
	}
#else
	glXDestroyContext(vid_dpy, ctx);
#endif
}

/************************************* VID INIT *************************************/

void VID_Init(unsigned char *palette) {
	int attrib[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
	int i, width = 640, height = 480;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
#ifdef WITH_VMODE
	qbool fullscreen = false;
	int MajorVersion, MinorVersion, actualWidth, actualHeight;
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register(&vid_ref);
	Cvar_Register(&vid_mode);
	Cvar_Register(&vid_hwgammacontrol);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("vid_minimize", VID_Minimize_f);
	Cvar_Register(&auto_grabmouse);

	vid.colormap = host_colormap; // FIXME

	if (!(vid_dpy = XOpenDisplay(NULL)))
		Sys_Error("Error couldn't open the X display");

	scrnum = DefaultScreen(vid_dpy);

	if (!(visinfo = glXChooseVisual(vid_dpy, scrnum, attrib)))
		Sys_Error("Error couldn't get an RGB, Double-buffered, Depth visual");

	root = RootWindow(vid_dpy, scrnum);

    // fullscreen cmdline check
#ifdef WITH_VMODE
	if (!COM_CheckParm("-window") && !COM_CheckParm("-startwindowed"))
		fullscreen = true;
#endif

	// set vid parameters
	if (COM_CheckParm("-current")) {
		width = DisplayWidth(vid_dpy, scrnum);
		height = DisplayHeight(vid_dpy, scrnum);
	} else {
		if ((i = COM_CheckParm("-width")) && i + 1 < COM_Argc())
			width = atoi(COM_Argv(i + 1));

		if ((i = COM_CheckParm("-height")) && i + 1 < COM_Argc())
			height = atoi(COM_Argv(i + 1));
	}

	if ((i = COM_CheckParm("-conwidth")) && i + 1 < COM_Argc())
		vid.conwidth = Q_atoi(COM_Argv(i + 1));
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm("-conheight")) && i + 1 < COM_Argc())
		vid.conheight = Q_atoi(COM_Argv(i + 1));
	if (vid.conheight < 200)
		vid.conheight = 200;
	
#ifdef WITH_VMODE
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(vid_dpy, &MajorVersion, &MinorVersion)) {
		vidmode_ext = false;
	} else {
		Com_Printf("Using XF86-VidModeExtension Ver. %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}
#endif

	// setup fullscreen size to fit display
#ifdef WITH_VMODE
	if (vidmode_ext) {
		int best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(vid_dpy, scrnum, &num_vidmodes, &vidmodes);
		// Are we going fullscreen?  If so, let's change video mode
		if (fullscreen) {
			best_dist = 9999999;
			best_fit = -1;

			for (i = 0; i < num_vidmodes; i++) {
				if (width > vidmodes[i]->hdisplay || height > vidmodes[i]->vdisplay)
					continue;

				x = width - vidmodes[i]->hdisplay;
				y = height - vidmodes[i]->vdisplay;
				dist = x * x + y * y;
				if (dist < best_dist) {
					best_dist = dist;
					best_fit = i;
				}
			}

			if (best_fit != -1) {
				if (( i = COM_CheckParm("-freq")) && i + 1 < COM_Argc()) { // FIXME: wow... this is horrible
					newvmode.hdisplay = vidmodes[best_fit]->hdisplay;
					newvmode.hsyncstart = vidmodes[best_fit]->hsyncstart;
					newvmode.hsyncend = vidmodes[best_fit]->hsyncend;
					newvmode.htotal = vidmodes[best_fit]->htotal;
					newvmode.hskew = vidmodes[best_fit]->hskew;
					newvmode.vdisplay = vidmodes[best_fit]->vdisplay;
					newvmode.vsyncstart = vidmodes[best_fit]->vsyncstart;
					newvmode.vsyncend = vidmodes[best_fit]->vsyncend;
					newvmode.vtotal = vidmodes[best_fit]->vtotal;
					newvmode.flags = vidmodes[best_fit]->flags;
					newvmode.dotclock = (unsigned int)rint((double)(Q_atoi(COM_Argv(i + 1)) * newvmode.htotal * newvmode.vtotal) / 1000.0);
					newvmode.privsize = 0;
					newvmode.private = NULL;
					if (XF86VidModeValidateModeLine(vid_dpy, scrnum, &newvmode) != 1)
						Com_Printf("VID_Init: Refresh rate %d out of range\n", Q_atoi(COM_Argv(i + 1)));
					else {
						XF86VidModeAddModeLine(vid_dpy, scrnum, &newvmode, NULL);
						new_vidmode = 1;
						actualWidth = newvmode.hdisplay;
						actualHeight = newvmode.vdisplay;
						X_vrefresh_rate = rint(((double)(newvmode.dotclock * 1000.0) / (double)(newvmode.htotal * newvmode.vtotal)));
						Com_Printf("X_vrefresh_rate: %f\n", X_vrefresh_rate);
						// change to the mode
						XF86VidModeSwitchToMode(vid_dpy, scrnum, &newvmode);
						vidmode_active = true;
						// Move the viewport to top left
						XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
					}
				}
				if (!vidmode_active) {
					actualWidth = vidmodes[best_fit]->hdisplay;
					actualHeight = vidmodes[best_fit]->vdisplay;
					X_vrefresh_rate = rint(((double)(vidmodes[best_fit]->dotclock * 1000.0) / (double)(vidmodes[best_fit]->htotal * vidmodes[best_fit]->vtotal)));
					Com_Printf("X_vrefresh_rate: %f\n", X_vrefresh_rate);
					// change to the mode
					XF86VidModeSwitchToMode(vid_dpy, scrnum, vidmodes[best_fit]);
					vidmode_active = true;
					// Move the viewport to top left
					XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
				}
			} else {
				fullscreen = 0;
			}
		}
	}
#endif
	// window attributes
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(vid_dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	// if fullscreen disable window manager decoration
#ifdef WITH_VMODE
	// fullscreen
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	}
#endif

	vid_window = XCreateWindow(vid_dpy, root, 0, 0, width, height,0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

	XStoreName(vid_dpy, vid_window, "ezQuake 0.31");
	XSetIconName(vid_dpy, vid_window, "ezQuake 0.31");

	XMapWindow(vid_dpy, vid_window);

#ifdef WITH_VMODE
	if (vidmode_active) {
		XRaiseWindow(vid_dpy, vid_window);
		XWarpPointer(vid_dpy, None, vid_window, 0, 0, 0, 0, 0, 0);
		XFlush(vid_dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(vid_dpy, scrnum, 0, 0);
	}
#endif

	XFlush(vid_dpy);

	ctx = glXCreateContext(vid_dpy, visinfo, NULL, True);

	glXMakeCurrent(vid_dpy, vid_window, ctx);

	scr_width = width;
	scr_height = height;

	if (vid.conheight > height)
		vid.conheight = height;
	if (vid.conwidth > width)
		vid.conwidth = width;
	vid.width = vid.conwidth;
	vid.height = vid.conheight;

	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	InitSig(); // trap evil signals

	GL_Init();
	GL_Init_GLX();

	Check_Gamma(palette);

	VID_SetPalette(palette);

	InitHWGamma();

	Com_Printf ("Video mode %dx%d initialized.\n", width, height);

	if (fullscreen)
		vid_windowedmouse = false;

	vid.recalc_refdef = 1; // force a surface cache flush

#ifdef WITH_EVDEV
	if ((i = COM_CheckParm("-mevdev")) && i + 1 < COM_Argc())
		{
		strncpy(evdev_device, COM_Argv(i + 1), sizeof(evdev_device));

		if (COM_CheckParm("-mmt")) {
			evdev_fd = open(evdev_device, O_RDONLY);
			if (!pthread_create(&evdev_thread, NULL, (void *)EvDev_UpdateMouse, NULL)) {
				Com_Printf("Multithreaded mouse input enabled\n");
				evdev_mt = 1;
			}
			else {
				Com_Printf("Evdev error creating thread\n");
				close(evdev_fd);
				evdev_fd = open(evdev_device, O_RDONLY | O_NONBLOCK);
				evdev_mt = 0;
			}
		} else
			evdev_fd = open(evdev_device, O_RDONLY | O_NONBLOCK);

		if (evdev_fd == -1)
			{
			Com_Printf("Evdev error: open %s failed\n", evdev_device);
			evdev_mouse = 0;
			return;
			}

		Com_Printf("Evdev %s enabled\n", evdev_device);
		evdev_mouse = 1;
		}
#endif
}

void Sys_SendKeyEvents(void) {
	if (vid_dpy) {
	#ifdef WITH_EVDEV
		if (evdev_mouse && !evdev_mt) EvDev_UpdateMouse(NULL);
	#endif
		while (XPending(vid_dpy))
			GetEvent();
	}
}

/************************************* INPUT *************************************/

void IN_StartupMouse (void)
{
//	mouseinitialized = true;
}

void IN_Commands (void) {}


#ifdef WITH_EVDEV
/************************************* EVDEV *************************************/
#define BTN_LOGITECH8 0x117

void EvDev_UpdateMouse(void *v) {
	struct input_event event;
	int ret;

	while ((ret = read(evdev_fd, &event, sizeof(struct input_event))) > 0) {
		if (vid_minimized || !_windowed_mouse.value) {
			if (evdev_mt) { usleep(10*1000); continue; }
			else continue;
		}

		if (ret < sizeof(struct input_event)) {
			Com_Printf("Error reading from %s (1)\nReverting to standard mouse input\n", evdev_device);
			close(evdev_fd);
			evdev_mouse = 0;
			return;
			}

		if (event.type == EV_REL) {
			switch (event.code) {
				case REL_X:
					mx += (signed int) event.value;
					break;

				case REL_Y:
					my += (signed int) event.value;
					break;

				case REL_WHEEL:
					switch ((signed int)event.value) {
						case 1:
							Key_Event(K_MWHEELUP, true); Key_Event(K_MWHEELUP, false); break;
						case -1:
							Key_Event(K_MWHEELDOWN, true); Key_Event(K_MWHEELDOWN, false); break;
					}
					break;
			}
		}

		if (event.type == EV_KEY) {
			switch (event.code) {
				case BTN_LEFT:
					Key_Event(K_MOUSE1, event.value); break;
				case BTN_RIGHT:
					Key_Event(K_MOUSE2, event.value); break;
				case BTN_MIDDLE:
					Key_Event(K_MOUSE3, event.value); break;
				case BTN_SIDE:
					Key_Event(K_MOUSE4, event.value); break;
				case BTN_EXTRA:
					Key_Event(K_MOUSE5, event.value); break;
				case BTN_FORWARD:
					Key_Event(K_MOUSE6, event.value); break;
				case BTN_BACK:
					Key_Event(K_MOUSE7, event.value); break;
				case BTN_LOGITECH8:
					Key_Event(K_MOUSE8, event.value); break;
			}
		}
	}

	if (ret == -1 && errno != EAGAIN) {
		Com_Printf("Error reading from %s (2)\nReverting to standard mouse input\n", evdev_device);
		close(evdev_fd);
		evdev_mouse = 0;
		}
}
#endif
