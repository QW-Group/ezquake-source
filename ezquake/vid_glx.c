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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vt.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include "quakedef.h"
#include "keys.h"
#include "input.h"
#include "gl_local.h"

#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef WITH_VMODE
#include <X11/extensions/xf86vmode.h>
#endif

#ifdef WITH_DGA
#include <X11/extensions/xf86dga.h>
#endif

static Display *dpy = NULL;
static Window win;
static GLXContext ctx = NULL;

static float old_windowed_mouse = 0, mouse_x, mouse_y, old_mouse_x, old_mouse_y;

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask)

unsigned short *currentgammaramp = NULL;
static unsigned short systemgammaramp[3][256];

qboolean vid_gammaworks = false;
qboolean vid_hwgamma_enabled = false;
qboolean customgamma = false;

static int scr_width, scr_height, scrnum;

#ifdef WITH_DGA
static int dgamouse, dgakeyb;
#endif

#ifdef WITH_VMODE
static qboolean vidmode_ext = false;
static XF86VidModeModeInfo **vidmodes;
static int num_vidmodes;
static qboolean vidmode_active = false;
#endif

cvar_t	vid_ref = {"vid_ref", "gl", CVAR_ROM};
cvar_t	vid_mode = {"vid_mode", "0"};
qboolean OnChange_windowed_mouse(cvar_t *, char *);
cvar_t	_windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE, OnChange_windowed_mouse};
cvar_t	m_filter = {"m_filter", "0"};
cvar_t	cl_keypad = {"cl_keypad", "1"};
cvar_t	vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height) {}

void D_EndDirectRect (int x, int y, int width, int height) {}

/************************************* COMPATABILITY *************************************/

void VID_UnlockBuffer() {}

void VID_LockBuffer() {}

void VID_SetCaption (char *text) {}

/************************************* KEY MAPPINGS *************************************/

int XLateKey(XKeyEvent *ev) {
	int key, kp;
	char buf[64];
	KeySym keysym;

	key = 0;
	kp = (int) cl_keypad.value;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

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

		case XK_BackSpace: key = K_BACKSPACE; break;

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
			key = *(unsigned char*)buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';
			break;
	}
	return key;
}

static void install_grabs(void) {
#ifdef WITH_DGA
	int DGAflags = 0;
#endif

	XGrabPointer(dpy, win,
		True,
		0,
		GrabModeAsync, GrabModeAsync,
		win,
		None,
		CurrentTime);

#ifdef WITH_DGA
	if (!COM_CheckParm("-nomdga"))
		DGAflags |= XF86DGADirectMouse;
	if (!COM_CheckParm("-nokdga"))
		DGAflags |= XF86DGADirectKeyb;

	if (!COM_CheckParm("-nodga") && DGAflags) {
		XF86DGADirectVideo(dpy, DefaultScreen(dpy), DGAflags);
		if (DGAflags & XF86DGADirectMouse)
			dgamouse = 1;
		if (DGAflags & XF86DGADirectKeyb)
			dgakeyb = 1;
	} 
	else
#endif
	{
		XWarpPointer(dpy, None, win,
			0, 0, 0, 0,
			vid.width / 2, vid.height / 2);
	}

	XGrabKeyboard(dpy, win,
		False,
		GrabModeAsync, GrabModeAsync,
		CurrentTime);
}

static void uninstall_grabs(void) {
#ifdef WITH_DGA
	if (dgamouse || dgakeyb) {
		XF86DGADirectVideo(dpy, DefaultScreen(dpy), 0);
		dgamouse = dgakeyb = 0;
	}
#endif

	XUngrabPointer(dpy, CurrentTime);
	XUngrabKeyboard(dpy, CurrentTime);
}

qboolean OnChange_windowed_mouse(cvar_t *var, char *value) {
	if (vidmode_active && !Q_atof(value)) {
		Com_Printf("Cannot turn %s off when using -fullscreen mode\n", var->name);
		return true;
	}
	return false;
}

static void GetEvent(void) {
	XEvent event;

	if (!dpy)
		return;

	XNextEvent(dpy, &event);

	switch (event.type) {
	case KeyPress:
	case KeyRelease:
		Key_Event(XLateKey(&event.xkey), event.type == KeyPress);
		break;

	case MotionNotify:
		if (_windowed_mouse.value) {
	#ifdef WITH_DGA
			if (dgamouse) {
				mouse_x += event.xmotion.x_root;
				mouse_y += event.xmotion.y_root;
			} else
	#endif
			{
				mouse_x = ((int) event.xmotion.x - (int) (vid.width / 2));
				mouse_y = ((int) event.xmotion.y - (int) (vid.height / 2));
				// move the mouse to the window center again
				XSelectInput(dpy, win, X_MASK & ~PointerMotionMask);
				XWarpPointer(dpy, None, win, 0, 0, 0, 0, (vid.width / 2), (vid.height / 2));
				XSelectInput(dpy, win, X_MASK);
			}
		}
		break;

	case ButtonPress:
	case ButtonRelease:
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
		}
		break;
	}

	if (old_windowed_mouse != _windowed_mouse.value) {
		old_windowed_mouse = _windowed_mouse.value;

		if (!_windowed_mouse.value)
			uninstall_grabs();
		else
			install_grabs();
	}
}

void signal_handler(int sig) {
	printf("Received signal %d, exiting...\n", sig);
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

/************************************* HW GAMMA *************************************/

void VID_ShiftPalette(unsigned char *p) {}

void InitHWGamma (void) {
	int xf86vm_gammaramp_size;

	if (COM_CheckParm("-nohwgamma"))
		return;

	XF86VidModeGetGammaRampSize(dpy, scrnum, &xf86vm_gammaramp_size);
	
	vid_gammaworks = (xf86vm_gammaramp_size == 256);

	if (vid_gammaworks){
		XF86VidModeGetGammaRamp(dpy,scrnum,xf86vm_gammaramp_size,
			systemgammaramp[0], systemgammaramp[1], systemgammaramp[2]);
	}
	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks; // && fullscreen?
}

void VID_SetDeviceGammaRamp (unsigned short *ramps) {
	if (vid_gammaworks) {
		currentgammaramp = ramps;
		if (vid_hwgamma_enabled) {
			XF86VidModeSetGammaRamp(dpy, scrnum, 256, ramps, ramps + 256, ramps + 512);
			customgamma = true;
		}
	}
}

void RestoreHWGamma (void) {
	if (vid_gammaworks && customgamma) {
		customgamma = false;
		XF86VidModeSetGammaRamp(dpy, scrnum, 256, systemgammaramp[0], systemgammaramp[1], systemgammaramp[2]);
	}
}

/************************************* GL *************************************/

void GL_BeginRendering (int *x, int *y, int *width, int *height) {
	*x = *y = 0;
	*width = scr_width;
	*height = scr_height;
}

void GL_EndRendering (void) {
	static qboolean old_hwgamma_enabled;

	vid_hwgamma_enabled = vid_hwgammacontrol.value && vid_gammaworks;
	if (vid_hwgamma_enabled != old_hwgamma_enabled) {
		old_hwgamma_enabled = vid_hwgamma_enabled;
		if (vid_hwgamma_enabled && currentgammaramp)
			VID_SetDeviceGammaRamp (currentgammaramp);
		else
			RestoreHWGamma ();
	}

	glFlush();
	glXSwapBuffers(dpy, win);
}

/************************************* VID SHUTDOWN *************************************/

void VID_Shutdown(void) {
	if (!ctx)
		return;

	uninstall_grabs();
	
	RestoreHWGamma();

#ifdef WITH_VMODE
	if (dpy) {
		glXDestroyContext(dpy, ctx);
		if (win)
			XDestroyWindow(dpy, win);
		if (vidmode_active)
			XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[0]);
		XCloseDisplay(dpy);
		vidmode_active = false;
	}
#else
	glXDestroyContext(dpy, ctx);
#endif
}

/************************************* VID INIT *************************************/

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
	char gldir[MAX_OSPATH];
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
#ifdef WITH_VMODE
	qboolean fullscreen = false;
	int MajorVersion, MinorVersion, actualWidth, actualHeight;
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register(&vid_ref);
	Cvar_Register(&vid_mode);
	Cvar_Register(&vid_hwgammacontrol);
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&_windowed_mouse);
	Cvar_Register(&m_filter);
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register(&cl_keypad);
	Cvar_ResetCurrentGroup();

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	if (!(dpy = XOpenDisplay(NULL)))
		Sys_Error("Error couldn't open the X display");

	if (!(visinfo = glXChooseVisual(dpy, scrnum, attrib)))
		Sys_Error("Error couldn't get an RGB, Double-buffered, Depth visual");

	scrnum = DefaultScreen(dpy);
	root = RootWindow(dpy, scrnum);

#ifdef WITH_VMODE
	if (COM_CheckParm("-fullscreen"))
		fullscreen = true;
#endif

	// set vid parameters
	if (COM_CheckParm("-current")) {
		width = DisplayWidth(dpy, scrnum);
		height = DisplayHeight(dpy, scrnum);
	} else {
		if ((i = COM_CheckParm("-width")) && i + 1 < com_argc)
			width = atoi(com_argv[i + 1]);

		if ((i = COM_CheckParm("-height")) && i + 1 < com_argc)
			height = atoi(com_argv[i + 1]);
	}

	if ((i = COM_CheckParm("-conwidth")) && i + 1 < com_argc)
		vid.conwidth = Q_atoi(com_argv[i + 1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm("-conheight")) && i + 1 < com_argc)
		vid.conheight = Q_atoi(com_argv[i + 1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

#ifdef WITH_VMODE
	MajorVersion = MinorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &MajorVersion, &MinorVersion)) {
		vidmode_ext = false;
	} else {
		Com_Printf("Using XF86-VidModeExtension Ver. %d.%d\n", MajorVersion, MinorVersion);
		vidmode_ext = true;
	}
#endif

#ifdef WITH_VMODE
	if (vidmode_ext) {
		int best_fit, best_dist, dist, x, y;

		XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);
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
				actualWidth = vidmodes[best_fit]->hdisplay;
				actualHeight = vidmodes[best_fit]->vdisplay;
				// change to the mode
				XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);
				vidmode_active = true;
				// Move the viewport to top left
				XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
			} else {
				fullscreen = 0;
			}
		}
	}
#endif
	// window attributes
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

#ifdef WITH_VMODE
	// fullscreen
	if (vidmode_active) {
		mask = CWBackPixel | CWColormap | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
		attr.override_redirect = True;
		attr.backing_store = NotUseful;
		attr.save_under = False;
	}
#endif

	win = XCreateWindow(dpy, root, 0, 0, width, height,0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

    XDefineCursor(dpy, win, CreateNullCursor(dpy, win));

	XMapWindow(dpy, win);

#ifdef WITH_VMODE
	if (vidmode_active) {
		XRaiseWindow(dpy, win);
		XWarpPointer(dpy, None, win, 0, 0, 0, 0, 0, 0);
		XFlush(dpy);
		// Move the viewport to top left
		XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
	}
#endif

	XFlush(dpy);

	ctx = glXCreateContext(dpy, visinfo, NULL, True);

	glXMakeCurrent(dpy, win, ctx);

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

	Check_Gamma(palette);

	VID_SetPalette(palette);

	InitHWGamma();

	Com_Printf ("Video mode %dx%d initialized.\n", width, height);

	if (fullscreen)
		vid_windowedmouse = false;

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void Sys_SendKeyEvents(void) {
	if (dpy) {
		while (XPending(dpy)) 
			GetEvent();
	}
}

/************************************* INPUT *************************************/

void Force_CenterView_f (void) {
	cl.viewangles[PITCH] = 0;
}

void IN_Init(void) {
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
}

void IN_Shutdown(void) {}

void IN_Commands (void) {}

void IN_MouseMove (usercmd_t *cmd) {
    float tx, ty, filterfrac, mousespeed;

    tx = mouse_x;
    ty = mouse_y;

	if (m_filter.value) {
        filterfrac = bound(0, m_filter.value, 1) / 2.0;
        mouse_x = (tx * (1 - filterfrac) + old_mouse_x * filterfrac);
        mouse_y = (ty * (1 - filterfrac) + old_mouse_y * filterfrac);
	}

    old_mouse_x = tx;
    old_mouse_y = ty;

	if (m_accel.value) {
		mousespeed = sqrt (tx * tx + ty * ty);
		mouse_x *= (mousespeed * m_accel.value + sensitivity.value);
		mouse_y *= (mousespeed * m_accel.value + sensitivity.value);
	} else {
		mouse_x *= sensitivity.value;
		mouse_y *= sensitivity.value;
	}

	// add mouse X/Y movement to cmd
	if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	
	if (mlook_active)
		V_StopPitchDrift ();
		
	if (mlook_active && !(in_strafe.state & 1))	{
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
	} else {
		cmd->forwardmove -= m_forward.value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

void IN_Move (usercmd_t *cmd) {
	IN_MouseMove(cmd);
}
