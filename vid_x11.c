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
// vid_x11.c -- general x video driver

#define _BSD

typedef unsigned short	PIXEL16;
typedef unsigned int	PIXEL24;

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>

#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"

cvar_t		vid_ref = {"vid_ref", "soft", CVAR_ROM};
cvar_t		_windowed_mouse = {"_windowed_mouse", "1", CVAR_ARCHIVE};
cvar_t		m_filter = {"m_filter", "1", CVAR_ARCHIVE};
cvar_t		cl_keypad = {"cl_keypad", "1"};
float		old_windowed_mouse;

// not used
int			VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes, VGA_planar;
byte		*VGA_pagebase;

qboolean	mouse_avail;
int			mouse_buttons = 3;
int			mouse_oldbuttonstate;
int			mouse_buttonstate;
float		mouse_x, mouse_y;
float		old_mouse_x, old_mouse_y;
int			p_mouse_x;
int			p_mouse_y;
int			ignorenext;
int			bits_per_pixel;

typedef struct {
	int input;
	int output;
} keymap_t;

extern viddef_t vid; // global video state
unsigned short d_8to16table[256];

int		num_shades = 32;
int		vid_buffersize;

static qboolean		doShm;
static Display		*x_disp;
static Colormap		x_cmap;
static Window		x_win;
static GC			x_gc;
static Visual		*x_vis;
static XVisualInfo	*x_visinfo;
//static XImage			*x_image;

static int			x_shmeventtype;
//static XShmSegmentInfo	x_shminfo;

static qboolean		oktodraw = false;

int XShmQueryExtension(Display *);
int XShmGetEventBase(Display *);

int current_framebuffer;
static XImage			*x_framebuffer[2] = {0, 0};
static XShmSegmentInfo	x_shminfo[2];

static int verbose = 0;

static byte current_palette[768];

static long X11_highhunkmark;
static long X11_buffersize;

int vid_surfcachesize;
void *vid_surfcache;

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);
void VID_MenuKey (int key);

static PIXEL16 st2d_8to16table[256];
static PIXEL24 st2d_8to24table[256];
static int shiftmask_fl = 0;
static long r_shift,g_shift,b_shift;
static unsigned long r_mask,g_mask,b_mask;

void shiftmask_init(void) {
	unsigned int x;
	r_mask = x_vis->red_mask;
	g_mask = x_vis->green_mask;
	b_mask = x_vis->blue_mask;

	if (r_mask > (1 << 31) || g_mask > (1 << 31) || b_mask > (1 << 31))
		Sys_Error("XGetVisualInfo returned bogus rgb mask");

	for (r_shift = -8, x = 1; x < r_mask; x = x << 1)
		r_shift++;
	for (g_shift = -8, x = 1; x < g_mask; x = x << 1)
		g_shift++;
	for (b_shift = -8, x = 1; x < b_mask; x = x << 1)
		b_shift++;

	shiftmask_fl = 1;
}

PIXEL16 xlib_rgb16(int r,int g,int b) {
    PIXEL16 p;

    if (shiftmask_fl == 0) 
		shiftmask_init();
    p = 0;

    if (r_shift > 0) {
        p = (r << (r_shift)) &r_mask;
    } else if(r_shift<0) {
        p = (r >> (-r_shift)) &r_mask;
	} else {
		p |= (r & r_mask);
	}

    if (g_shift>0) {
        p |= (g << (g_shift)) &g_mask;
    } else if(g_shift<0) {
        p |= (g >> (-g_shift)) &g_mask;
	} else {
		p|=(g & g_mask);
	}

    if (b_shift > 0) {
        p |= (b << (b_shift)) &b_mask;
    } else if (b_shift < 0) {
        p |= (b >> (-b_shift)) &b_mask;
	} else {
		p|=(b & b_mask);
	}

    return p;
}

PIXEL24 xlib_rgb24(int r,int g,int b) {
    PIXEL24 p;

    if (shiftmask_fl == 0) 
		shiftmask_init();
    p = 0;

    if (r_shift > 0) {
        p = (r << (r_shift)) &r_mask;
    } else if(r_shift<0) {
        p = (r >> (-r_shift)) &r_mask;
	} else {
		p |= (r & r_mask);
	}

    if (g_shift>0) {
        p |= (g << (g_shift)) &g_mask;
    } else if(g_shift<0) {
        p |= (g >> (-g_shift)) &g_mask;
	} else {
		p|=(g & g_mask);
	}

    if (b_shift > 0) {
        p |= (b << (b_shift)) &b_mask;
    } else if (b_shift < 0) {
        p |= (b >> (-b_shift)) &b_mask;
	} else {
		p|=(b & b_mask);
	}

    return p;
}

void st2_fixup( XImage *framebuf, int x, int y, int width, int height) {
	int xi,yi;
	unsigned char *src;
	PIXEL16 *dest;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < y + height; yi++) {
		src = &framebuf->data [yi * framebuf->bytes_per_line];
		dest = (PIXEL16 *) src;
		for (xi = (x + width - 1); xi >= x; xi--) {
			dest[xi] = st2d_8to16table[src[xi]];
		}
	}
}

void st3_fixup( XImage *framebuf, int x, int y, int width, int height) {
	int xi,yi;
	unsigned char *src;
	PIXEL24 *dest;

	if (x < 0 || y < 0)
		return;

	for (yi = y; yi < y + height; yi++) {
		src = &framebuf->data [yi * framebuf->bytes_per_line];
		dest = (PIXEL24 *) src;
		for (xi = (x + width - 1); xi >= x; xi--) {
			dest[xi] = st2d_8to24table[src[xi]];
		}
	}
}

// ========================================================================
// Tragic death handler
// ========================================================================

void TragicDeath(int signal_num) {
	XAutoRepeatOn(x_disp);
	XCloseDisplay(x_disp);
	Sys_Error("This death brought to you by the number %d\n", signal_num);
}

// ========================================================================
// makes a null cursor
// ========================================================================

static Cursor CreateNullCursor(Display *display, Window root) {
    Pixmap cursormask; 
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
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

void ResetFrameBuffer(void)  {
	int mem, pwidth;

	if (x_framebuffer[0]) {
		free(x_framebuffer[0]->data);
		free(x_framebuffer[0]);
	}

	if (d_pzbuffer)	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (X11_highhunkmark);
		d_pzbuffer = NULL;
	}
	X11_highhunkmark = Hunk_HighMark ();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	X11_buffersize = vid.width * vid.height * sizeof (*d_pzbuffer);

	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height);

	X11_buffersize += vid_surfcachesize;

	d_pzbuffer = Hunk_HighAllocName (X11_buffersize, "video");
	if (d_pzbuffer == NULL)
		Sys_Error ("Not enough memory for video mode\n");

	vid_surfcache = (byte *) d_pzbuffer + vid.width * vid.height * sizeof (*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);

	pwidth = x_visinfo->depth / 8;
	if (pwidth == 3) pwidth = 4;
	mem = ((vid.width*pwidth+7)&~7) * vid.height;

	x_framebuffer[0] = XCreateImage(	x_disp,
		x_vis,
		x_visinfo->depth,
		ZPixmap,
		0,
		Q_Malloc (mem),
		vid.width, vid.height,
		32,
		0);

	if (!x_framebuffer[0])
		Sys_Error("VID: XCreateImage failed\n");

	vid.buffer = (byte*) (x_framebuffer[0]);
}

void ResetSharedFrameBuffers(void) {
	int size, key, minsize = getpagesize(), frm;

	if (d_pzbuffer)	{
		D_FlushCaches ();
		Hunk_FreeToHighMark (X11_highhunkmark);
		d_pzbuffer = NULL;
	}

	X11_highhunkmark = Hunk_HighMark ();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	X11_buffersize = vid.width * vid.height * sizeof (*d_pzbuffer);

	vid_surfcachesize = D_SurfaceCacheForRes (vid.width, vid.height);

	X11_buffersize += vid_surfcachesize;

	d_pzbuffer = Hunk_HighAllocName (X11_buffersize, "video");
	if (d_pzbuffer == NULL)
		Sys_Error ("Not enough memory for video mode\n");

	vid_surfcache = (byte *) d_pzbuffer + vid.width * vid.height * sizeof (*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);

	for (frm = 0; frm < 2; frm++) {
		// free up old frame buffer memory
		if (x_framebuffer[frm]) {
			XShmDetach(x_disp, &x_shminfo[frm]);
			free(x_framebuffer[frm]);
			shmdt(x_shminfo[frm].shmaddr);
		}

		// create the image
		x_framebuffer[frm] = XShmCreateImage(	x_disp,
						x_vis,
						x_visinfo->depth,
						ZPixmap,
						0,
						&x_shminfo[frm],
						vid.width,
						vid.height );

		// grab shared memory
		size = x_framebuffer[frm]->bytes_per_line * x_framebuffer[frm]->height;
		if (size < minsize)
			Sys_Error("VID: Window must use at least %d bytes\n", minsize);

		key = random();
		x_shminfo[frm].shmid = shmget((key_t)key, size, IPC_CREAT|0777);
		if (x_shminfo[frm].shmid == -1)
			Sys_Error("VID: Could not get any shared memory\n");

		// attach to the shared memory segment
		x_shminfo[frm].shmaddr = (void *) shmat(x_shminfo[frm].shmid, 0, 0);

		if (verbose)
			printf("VID: shared memory id=%d, addr=0x%lx\n", x_shminfo[frm].shmid, (long) x_shminfo[frm].shmaddr);

		x_framebuffer[frm]->data = x_shminfo[frm].shmaddr;

		// get the X server to attach to it

		if (!XShmAttach(x_disp, &x_shminfo[frm]))
			Sys_Error("VID: XShmAttach() failed\n");
		XSync(x_disp, 0);
		shmctl(x_shminfo[frm].shmid, IPC_RMID, 0);
	}

}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void VID_Init (unsigned char *palette) {
	int pnum, i, num_visuals, template_mask;
	XVisualInfo template;

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register (&vid_ref);
	Cvar_ResetCurrentGroup();

	ignorenext=0;
	vid.width = 320;
	vid.height = 200;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	//	vid.cbits = VID_CBITS;
	//	vid.grades = VID_GRADES;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	
	srandom(getpid());

	verbose = COM_CheckParm("-verbose");

	// open the display
	x_disp = XOpenDisplay(0);
	if (!x_disp) {
		if (getenv("DISPLAY"))
			Sys_Error("VID: Could not open display [%s]\n",
				getenv("DISPLAY"));
		else
			Sys_Error("VID: Could not open local display\n");
	}

	// catch signals so i can turn on auto-repeat
	{
		struct sigaction sa;
		sigaction(SIGINT, 0, &sa);
		sa.sa_handler = TragicDeath;
		sigaction(SIGINT, &sa, 0);
		sigaction(SIGTERM, &sa, 0);
	}

	XAutoRepeatOff(x_disp);

	// for debugging only
	XSynchronize(x_disp, True);

	// check for command-line window size
	if ((pnum = COM_CheckParm("-winsize"))) {
		if (pnum >= com_argc-2)
			Sys_Error("VID: -winsize <width> <height>\n");
		vid.width = Q_atoi(com_argv[pnum+1]);
		vid.height = Q_atoi(com_argv[pnum+2]);
		if (!vid.width || !vid.height)
			Sys_Error("VID: Bad window width/height\n");
	}
	if ((pnum = COM_CheckParm("-width"))) {
		if (pnum >= com_argc - 1)
			Sys_Error("VID: -width <width>\n");
		vid.width = Q_atoi(com_argv[pnum + 1]);
		if (!vid.width)
			Sys_Error("VID: Bad window width\n");
		if (vid.width > MAXWIDTH)
			Sys_Error("VID: Maximum supported width is %d", MAXWIDTH);

	}
	if ((pnum = COM_CheckParm("-height"))) {
		if (pnum >= com_argc - 1)
			Sys_Error("VID: -height <height>\n");
		vid.height = Q_atoi(com_argv[pnum + 1]);
		if (!vid.height)
			Sys_Error("VID: Bad window height\n");
		if (vid.height > MAXHEIGHT)
			Sys_Error("VID: Maximum supported height is %d", MAXHEIGHT);
	}

	template_mask = 0;

	// specify a visual id
	if ((pnum = COM_CheckParm("-visualid")))	{
		if (pnum >= com_argc - 1)
			Sys_Error("VID: -visualid <id#>\n");
		template.visualid = Q_atoi(com_argv[pnum + 1]);
		template_mask = VisualIDMask;
	} else {// If not specified, use default visual
		int screen;
		screen = XDefaultScreen(x_disp);
		template.visualid = XVisualIDFromVisual(XDefaultVisual(x_disp, screen));
		template_mask = VisualIDMask;
	}

	// pick a visual- warn if more than one was available
	x_visinfo = XGetVisualInfo(x_disp, template_mask, &template, &num_visuals);
	if (num_visuals > 1) {
		printf("Found more than one visual id at depth %d:\n", template.depth);
		for (i = 0; i < num_visuals; i++)
			printf("	-visualid %d\n", (int)(x_visinfo[i].visualid));
	} else if (num_visuals == 0) {
		if (template_mask == VisualIDMask)
			Sys_Error("VID: Bad visual id %d\n", template.visualid);
		else
			Sys_Error("VID: No visuals at depth %d\n", template.depth);
	}

	if (verbose) {
		printf("Using visualid %d:\n", (int)(x_visinfo->visualid));
		printf("	screen %d\n", x_visinfo->screen);
		printf("	red_mask 0x%x\n", (int)(x_visinfo->red_mask));
		printf("	green_mask 0x%x\n", (int)(x_visinfo->green_mask));
		printf("	blue_mask 0x%x\n", (int)(x_visinfo->blue_mask));
		printf("	colormap_size %d\n", x_visinfo->colormap_size);
		printf("	bits_per_rgb %d\n", x_visinfo->bits_per_rgb);
	}

	x_vis = x_visinfo->visual;

	// setup attributes for main window
	{
		int attribmask = CWEventMask  | CWColormap | CWBorderPixel;
		XSetWindowAttributes attribs;
		Colormap tmpcmap;
		
		tmpcmap = XCreateColormap(x_disp, XRootWindow(x_disp, x_visinfo->screen), x_vis, AllocNone);
		
		attribs.event_mask = StructureNotifyMask|KeyPressMask|KeyReleaseMask|ExposureMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask;
		attribs.border_pixel = 0;
		attribs.colormap = tmpcmap;

// create the main window
		x_win = XCreateWindow(	x_disp,
			XRootWindow(x_disp, x_visinfo->screen),
			0, 0,	// x, y
			vid.width, vid.height,
			0, // borderwidth
			x_visinfo->depth,
			InputOutput,
			x_vis,
			attribmask,
			&attribs );
		XStoreName( x_disp,x_win,"FuhQuake");

		if (x_visinfo->class != TrueColor)
			XFreeColormap(x_disp, tmpcmap);

	}

	if (x_visinfo->depth == 8) {
		// create and upload the palette
		if (x_visinfo->class == PseudoColor) {
			x_cmap = XCreateColormap(x_disp, x_win, x_vis, AllocAll);
			VID_SetPalette(palette);
			XSetWindowColormap(x_disp, x_win, x_cmap);
		}

	}

	// inviso cursor
	XDefineCursor(x_disp, x_win, CreateNullCursor(x_disp, x_win));

	// create the GC
	{
		XGCValues xgcvalues;
		int valuemask = GCGraphicsExposures;
		xgcvalues.graphics_exposures = False;
		x_gc = XCreateGC(x_disp, x_win, valuemask, &xgcvalues );
	}

	// map the window
	XMapWindow(x_disp, x_win);

	// wait for first exposure event
	{
		XEvent event;
		do {
			XNextEvent(x_disp, &event);
			if (event.type == Expose && !event.xexpose.count)
				oktodraw = true;
		} while (!oktodraw);
	}
	// now safe to draw

	// even if MITSHM is available, make sure it's a local connection
	if (XShmQueryExtension(x_disp))	{
		char displayname[MAX_OSPATH], *d;
		doShm = true;
		if ((d = (char *) getenv("DISPLAY"))) {
			Q_strncpyz(displayname, d, sizeof(displayname));
			for (d = displayname; *d && (*d != ':'); d++)
				;
			*d = 0;
			if (!(!Q_strcasecmp(displayname, "unix") || !*displayname))
				doShm = false;
		}
	}
	if (doShm){
		x_shmeventtype = XShmGetEventBase(x_disp) + ShmCompletion;
		ResetSharedFrameBuffers();
	} else {
		ResetFrameBuffer();
	}

	current_framebuffer = 0;
	vid.rowbytes = x_framebuffer[0]->bytes_per_line;
	vid.buffer = x_framebuffer[0]->data;
	vid.direct = 0;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);

	//XSynchronize(x_disp, False);
}

void VID_ShiftPalette(unsigned char *p) {
	VID_SetPalette(p);
}

void VID_SetPalette(unsigned char *palette) {
	int i;
	XColor colors[256];

	for (i = 0; i < 256; i++) {
		st2d_8to16table[i]= xlib_rgb16(palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
		st2d_8to24table[i]= xlib_rgb24(palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
	}

	if (x_visinfo->class == PseudoColor && x_visinfo->depth == 8) {
		if (palette != current_palette)
			memcpy(current_palette, palette, 768);
		for (i = 0; i < 256; i++)	{
			colors[i].pixel = i;
			colors[i].flags = DoRed|DoGreen|DoBlue;
			colors[i].red = palette[i * 3] * 257;
			colors[i].green = palette[i * 3 + 1] * 257;
			colors[i].blue = palette[i * 3 + 2] * 257;
		}
		XStoreColors(x_disp, x_cmap, colors, 256);
	}
}

// Called at shutdown
void VID_Shutdown (void) {
	Com_Printf ("VID_Shutdown\n");
	XAutoRepeatOn(x_disp);
	XCloseDisplay(x_disp);
}

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
//			fprintf(stdout, "case 0x0%x: key = ___;break;/* [%c] */\n", keysym);
			break;
	}
	return key;
}

struct {
	int key;
	int down;
} keyq[64];
int keyq_head = 0;
int keyq_tail = 0;

int config_notify = 0;
int config_notify_width;
int config_notify_height;

void GetEvent(void) {
	XEvent event;

	XNextEvent(x_disp, &event);
	switch(event.type) {
	case KeyPress:
		keyq[keyq_head].key = XLateKey(&event.xkey);
		keyq[keyq_head].down = true;
		keyq_head = (keyq_head + 1) & 63;
		break;
	case KeyRelease:
		keyq[keyq_head].key = XLateKey(&event.xkey);
		keyq[keyq_head].down = false;
		keyq_head = (keyq_head + 1) & 63;
		break;

	case MotionNotify:
		if (_windowed_mouse.value) {
			mouse_x = (float) ((int)event.xmotion.x - (int) (vid.width / 2));
			mouse_y = (float) ((int)event.xmotion.y - (int) (vid.height / 2));

			/* move the mouse to the window center again */
			XSelectInput(x_disp,x_win,StructureNotifyMask|KeyPressMask
				|KeyReleaseMask|ExposureMask
				|ButtonPressMask
				|ButtonReleaseMask);
			XWarpPointer(x_disp,None,x_win,0,0,0,0, 
				(vid.width/2),(vid.height/2));
			XSelectInput(x_disp,x_win,StructureNotifyMask|KeyPressMask
				|KeyReleaseMask|ExposureMask
				|PointerMotionMask|ButtonPressMask
				|ButtonReleaseMask);
		} else {
			mouse_x = (float) (event.xmotion.x-p_mouse_x);
			mouse_y = (float) (event.xmotion.y-p_mouse_y);
			p_mouse_x=event.xmotion.x;
			p_mouse_y=event.xmotion.y;
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
	
	case ConfigureNotify:
		config_notify_width = event.xconfigure.width;
		config_notify_height = event.xconfigure.height;
		config_notify = 1;
		break;

	default:
		if (doShm && event.type == x_shmeventtype)
			oktodraw = true;
	}
   
	if (old_windowed_mouse != _windowed_mouse.value) {
		old_windowed_mouse = _windowed_mouse.value;

		if (!_windowed_mouse.value) {
			/* ungrab the pointer */
			XUngrabPointer(x_disp,CurrentTime);
		} else {
			/* grab the pointer */
			XGrabPointer(x_disp,x_win,True,0,GrabModeAsync, GrabModeAsync, x_win, None, CurrentTime);
		}
	}
}

// flushes the given rectangles from the view buffer to the screen

void VID_Update (vrect_t *rects) {
	// if the window changes dimension, skip this frame
	if (config_notify) {
		fprintf(stderr, "config notify\n");
		config_notify = 0;
		vid.width = config_notify_width & ~7;
		vid.height = config_notify_height;
		if (doShm)
			ResetSharedFrameBuffers();
		else
			ResetFrameBuffer();
		vid.rowbytes = x_framebuffer[0]->bytes_per_line;
		vid.buffer = x_framebuffer[current_framebuffer]->data;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
		vid.recalc_refdef = 1;				// force a surface cache flush
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	if (doShm) {
		while (rects){
			if (x_visinfo->depth == 24)
				st3_fixup(x_framebuffer[current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			else if (x_visinfo->depth == 16)
				st2_fixup(x_framebuffer[current_framebuffer], rects->x, rects->y, rects->width, rects->height);
			if (!XShmPutImage(x_disp, x_win, x_gc,
				x_framebuffer[current_framebuffer], rects->x, rects->y, rects->x, rects->y, rects->width, rects->height, True))
					Sys_Error("VID_Update: XShmPutImage failed\n");
			oktodraw = false;
			while (!oktodraw) GetEvent();
			rects = rects->pnext;
		}
		current_framebuffer = !current_framebuffer;
		vid.buffer = x_framebuffer[current_framebuffer]->data;
		XSync(x_disp, False);
	} else {
		while (rects) {
			if (x_visinfo->depth != 8)
				st2_fixup( x_framebuffer[current_framebuffer], rects->x, rects->y, rects->width,rects->height);
			XPutImage(x_disp, x_win, x_gc, x_framebuffer[0], rects->x, rects->y, rects->x, rects->y, rects->width, rects->height);
			rects = rects->pnext;
		}
		XSync(x_disp, False);
	}

}

static int dither;

void VID_DitherOn(void) {
    if (dither == 0) {
		vid.recalc_refdef = 1;
        dither = 1;
    }
}

void VID_DitherOff(void) {
    if (dither) {
		vid.recalc_refdef = 1;
        dither = 0;
    }
}

int Sys_OpenWindow(void) {
	return 0;
}

void Sys_EraseWindow(int window) {}

void Sys_DrawCircle(int window, int x, int y, int r) {}

void Sys_DisplayWindow(int window) {}

void Sys_SendKeyEvents(void) {
	// get events from x server
	if (x_disp)	{
		while (XPending(x_disp)) GetEvent();
		while (keyq_head != keyq_tail) {
			Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
			keyq_tail = (keyq_tail + 1) & 63;
		}
	}
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height) {
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void D_EndDirectRect (int x, int y, int width, int height) {
// direct drawing of the "accessing disk" icon isn't supported under Linux
}

void Force_CenterView_f (void) {
	cl.viewangles[PITCH] = 0;
}

void IN_Init (void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register(&cl_keypad);
	Cvar_ResetCurrentGroup();
	Cmd_AddCommand ("force_centerview", Force_CenterView_f);
	if ( COM_CheckParm ("-nomouse") )
		return;
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register (&m_filter);
	Cvar_Register (&_windowed_mouse);
	Cvar_ResetCurrentGroup();
	mouse_x = mouse_y = 0.0;
	mouse_avail = 1;
}

void IN_Shutdown (void) {
	mouse_avail = 0;
}

void IN_Commands (void) {
	int i;
   
	if (!mouse_avail) return;
   
	for (i = 0; i < mouse_buttons; i++) {
		if ( (mouse_buttonstate & (1 << i)) && !(mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, true);

		if ( !(mouse_buttonstate & (1 << i)) && (mouse_oldbuttonstate & (1<<i)) )
			Key_Event (K_MOUSE1 + i, false);
	}
	mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move (usercmd_t *cmd) {
    float tx, ty, filterfrac, mousespeed;

	if (!mouse_avail)
		return;   

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

	if ((in_strafe.state & 1) || (lookstrafe.value && mlook_active))
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;

	if (mlook_active)
		V_StopPitchDrift ();

	if (mlook_active && !(in_strafe.state & 1)) {
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		cl.viewangles[PITCH] = bound(-70, cl.viewangles[PITCH], 80);
	} else {
		cmd->forwardmove -= m_forward.value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

void VID_LockBuffer (void) {}

void VID_UnlockBuffer (void) {}

void VID_SetCaption (char *text) {}
