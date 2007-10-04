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

	$Id: vid_svgalib.c,v 1.29 2007-10-04 13:48:09 dkure Exp $
*/
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#else
#include <sys/vt.h>
#include <sys/io.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include "vga.h"
#include "vgakeyboard.h"
#include "vgamouse.h"

#include "quakedef.h"
#include "keys.h"
#include "input.h"
#include "d_local.h"

#ifdef WITH_KEYMAP
#include "keymap.h"
#endif // WITH_KEYMAP

#define stringify(m) { #m, m }

extern int ctrlDown, shiftDown, altDown;

static byte *vid_surfcache;
static int VID_highhunkmark;

static int num_modes;
static vga_modeinfo *modes;
static int current_mode = 0;

struct {
	char *name;
	int num;
} mice[] = {
	stringify(MOUSE_MICROSOFT),
	stringify(MOUSE_MOUSESYSTEMS),
	stringify(MOUSE_MMSERIES),
	stringify(MOUSE_LOGITECH),
	stringify(MOUSE_BUSMOUSE),
	stringify(MOUSE_PS2),
};

static byte vid_current_palette[768];
static unsigned char *framebuffer_ptr;
static byte backingbuf[48*24];
static int svgalib_inited = 0;

int VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes, VGA_planar;
byte *VGA_pagebase;

cvar_t vid_ref = {"vid_ref", "soft", CVAR_ROM};
cvar_t vid_mode = {"vid_mode","5"};
cvar_t vid_redrawfull = {"vid_redrawfull","0"};
cvar_t vid_vsync = {"vid_vsync","0",CVAR_ARCHIVE};

int mx, my;
qbool mouseinitialized = false;

/************************************** COMPATABILITY *************************************/

void VID_UnlockBuffer() {}

void VID_LockBuffer() {}
/************************************** COMPATABILITY *************************************/

void VGA_UpdatePlanarScreen (void *srcbuffer);


void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	int i, j, k, plane, reps, repshift, offset, vidpage, off;


	if (!svgalib_inited || !vid.direct || !vga_oktowrite())
		return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	vidpage = 0;
	vga_setpage(0);

	if (VGA_planar) {
		for (plane=0 ; plane<4 ; plane++) {
		// select the correct plane for reading and writing
			outb(0x02, 0x3C4);
			outb(1 << plane, 0x3C5);
			outb(4, 0x3CE);
			outb(plane, 0x3CF);

		for (i = 0; i < (height << repshift); i += reps) {
			for (k = 0; k < reps; k++) {
					for (j = 0; j < (width >> 2); j++) {
						backingbuf[(i + k) * 24 + (j << 2) + plane] = vid.direct[(y + i + k) * VGA_rowbytes + (x >> 2) + j];
						vid.direct[(y + i + k) * VGA_rowbytes + (x>>2) + j] = pbitmap[(i >> repshift) * 24 + (j << 2) + plane];
					}
				}
			}
		}
	} else {
		for (i = 0; i < (height << repshift); i += reps) {
			for (j = 0; j < reps; j++) {
				offset = x + ((y << repshift) + i + j) * vid.rowbytes;
				off = offset % 0x10000;
				if ((offset / 0x10000) != vidpage) {
					vidpage=offset / 0x10000;
					vga_setpage(vidpage);
				}
				memcpy (&backingbuf[(i + j) * 24],vid.direct + off, width);
				memcpy (vid.direct + off,&pbitmap[(i >> repshift)*width], width);
			}
		}
	}
}

void D_EndDirectRect (int x, int y, int width, int height)
{
	int i, j, k, plane, reps, repshift, offset, vidpage, off;


	if (!svgalib_inited || !vid.direct || !vga_oktowrite()) return;

	if (vid.aspect > 1.5) {
		reps = 2;
		repshift = 1;
	} else {
		reps = 1;
		repshift = 0;
	}

	vidpage = 0;
	vga_setpage(0);

	if (VGA_planar) {
		for (plane = 0; plane < 4; plane++) {
			// select the correct plane for writing
			outb(2, 0x3C4);
			outb(1 << plane, 0x3C5);
			outb(4, 0x3CE);
			outb(plane, 0x3CF);

			for (i = 0; i < (height << repshift); i += reps)
				for (k = 0; k < reps; k++)
					for (j = 0; j < (width >> 2); j++)
						vid.direct[(y + i + k) * VGA_rowbytes + (x >> 2) + j] =backingbuf[(i + k) * 24 + (j << 2) + plane];
		}
	} else {
		for (i = 0; i < (height << repshift); i += reps)
			for (j = 0; j < reps; j++) {
				offset = x + ((y << repshift) + i + j) * vid.rowbytes;
				off = offset % 0x10000;
				if (offset / 0x10000 != vidpage) {
					vidpage = offset / 0x10000;
					vga_setpage(vidpage);
				}
				memcpy (vid.direct + off, &backingbuf[(i + j) * 24], width);
			}
	}
}

/*
static void VID_Gamma_f (void)
{
	int i;
	float gamma, f, inf;
	unsigned char palette[768];


	if (Cmd_Argc() == 2) {
		gamma = Q_atof (Cmd_Argv(1));

		for (i = 0; i < 768; i++) {
			f = pow ( (host_basepal[i] + 1) / 256.0 , gamma );
			inf = f * 255 + 0.5;
			palette[i] = bound(0, inf, 255);
		}

		VID_SetPalette (palette);
		vid.recalc_refdef = 1; // force a surface cache flush
	}
}
*/

static void VID_ModeList_f (void)
{
	int i;


	for (i = 0; i < num_modes; i++) {
		if (modes[i].width) {
			Com_Printf ("%d: %d x %d - ", i, modes[i].width,modes[i].height);

			if (modes[i].bytesperpixel == 0)
				Com_Printf ("ModeX\n");
			else
				Com_Printf ("%d bpp\n", modes[i].bytesperpixel<<3);
		}
	}
}

/*
static void VID_Debug_f (void)
{
	Com_Printf ("mode: %d\n",current_mode);
	Com_Printf ("height x width: %d x %d\n",vid.height,vid.width);
	Com_Printf ("bpp: %d\n",modes[current_mode].bytesperpixel*8);
	Com_Printf ("vid.aspect: %f\n",vid.aspect);
}
*/

static void VID_InitModes (void)
{
	int i;


	// get complete information on all modes
	num_modes = vga_lastmodenumber()+1;
	modes = (vga_modeinfo *) Hunk_Alloc(num_modes * sizeof(vga_modeinfo));
	for (i = 0 ; i < num_modes; i++) {
		if (vga_hasmode(i))
			memcpy(&modes[i], vga_getmodeinfo(i), sizeof (vga_modeinfo));
		else
			modes[i].width = 0; // means not available
	}

	// filter for modes i don't support
	for (i = 0; i < num_modes ; i++)
		if ((modes[i].bytesperpixel != 1 && modes[i].colors != 256) || modes[i].width > MAXWIDTH || modes[i].height > MAXHEIGHT)
			modes[i].width = 0;

	vid_windowedmouse = false;
}

static int get_mode (char *name, int width, int height, int depth)
{
	int i, ok, match;


	match = (!!width) + (!!height) * 2 + (!!depth) * 4;

	if (name) {
		i = vga_getmodenumber(name);

		if (!modes[i].width) {
			Sys_Printf("Mode [%s] not supported\n", name);
			i = G320x200x256;
		}
	} else {
		for (i = 0 ; i<num_modes ; i++)
			if (modes[i].width) {
				ok = (modes[i].width == width) + (modes[i].height == height) * 2 + (modes[i].bytesperpixel == depth / 8) * 4;
				if ((ok & match) == ok)
					break;
			}

		if (i == num_modes) {
			Sys_Printf("Mode %dx%d (%d bits) not supported\n", width, height, depth);
			i = G320x200x256;
		}
	}

	return i;

}

static byte scantokey_kp[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0  ,   K_ESCAPE,'1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9,					// 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    K_ENTER,K_LCTRL, 'a',    's',					// 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_LSHIFT,'\\',   'z',    'x',    'c',    'v',					// 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_RSHIFT,KP_STAR,
	K_LALT,  ' ',  K_CAPSLOCK,K_F1,  K_F2,   K_F3,   K_F4,   K_F5,					// 3
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  KP_NUMLOCK,K_SCRLCK,KP_HOME,
	KP_UPARROW,KP_PGUP,KP_MINUS,KP_LEFTARROW,KP_5,KP_RIGHTARROW,KP_PLUS,KP_END,		// 4
	KP_DOWNARROW,KP_PGDN,KP_INS,KP_DEL, 0,  0,      0,      K_F11,
	K_F12,  0,      0,      0,      0,      0,      0,      0,						// 5
	KP_ENTER,K_RCTRL,KP_SLASH,0, K_RALT,  K_PAUSE,K_HOME, K_UPARROW,
	K_PGUP, K_LEFTARROW,K_RIGHTARROW,K_END, K_DOWNARROW,K_PGDN,K_INS,K_DEL,        // 6
	0,      0,      0,      0,      0,      0,      0,      K_PAUSE,
	0,      0,      0,      0,      0,      K_LWIN, K_RWIN, K_MENU					// 7
};

static byte scantokey[128] = {
//  0       1        2       3       4       5       6       7
//  8       9        A       B       C       D       E       F
	0  ,   K_ESCAPE,'1',    '2',    '3',    '4',    '5',    '6',
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9,   // 0
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
	'o',    'p',    '[',    ']',    K_ENTER,K_LCTRL, 'a',    's',      // 1
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
	'\'',   '`',    K_LSHIFT,'\\',   'z',    'x',    'c',    'v',      // 2
	'b',    'n',    'm',    ',',    '.',    '/',    K_RSHIFT,KP_STAR,
	K_LALT,  ' ',  K_CAPSLOCK,K_F1,  K_F2,   K_F3,   K_F4,   K_F5,     // 3
	K_F6,   K_F7,   K_F8,   K_F9,   K_F10,  KP_NUMLOCK,K_SCRLCK,K_HOME,
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',    K_END, // 4
	K_DOWNARROW,K_PGDN,K_INS,K_DEL, 0,      0,      0,      K_F11,
	K_F12,  0,      0,      0,      0,      0,      0,      0,        // 5
	K_ENTER,K_RCTRL,'/',     0, K_RALT,  K_PAUSE,K_HOME, K_UPARROW,
	K_PGUP, K_LEFTARROW,K_RIGHTARROW,K_END, K_DOWNARROW,K_PGDN,K_INS,K_DEL,        // 6
	0,      0,      0,      0,      0,      0,      0,      K_PAUSE,
	0,      0,      0,      0,      0,      K_LWIN, K_RWIN, K_MENU			// 7
};

static void keyhandler (int scancode, int state)
{
	byte *scan = cl_keypad.value ? scantokey_kp : scantokey;
	int sc = scancode & 0x7f;

	Key_Event(scan[sc], state == KEY_EVENTPRESS);

	// kazik -->
	if (sc == SCANCODE_LEFTCONTROL || sc == SCANCODE_RIGHTCONTROL)
		ctrlDown = (state == KEY_EVENTPRESS);

	if (sc == SCANCODE_LEFTSHIFT || sc == SCANCODE_RIGHTSHIFT)
		shiftDown = (state == KEY_EVENTPRESS);

	if (sc == SCANCODE_LEFTALT || sc == SCANCODE_RIGHTALT)
		altDown = (state == KEY_EVENTPRESS);
	// kazik <--
}

void VID_Shutdown (void)
{
	if (!svgalib_inited)
		return;

	keyboard_close();
	vga_setmode(TEXT);

	svgalib_inited = 0;
}

void VID_SetPalette (byte *palette)
{
	static int tmppal[256 * 3];
	int i, *tp;


	if (!svgalib_inited)
		return;

	memcpy(vid_current_palette, palette, sizeof(vid_current_palette));

	if (vga_getcolors() == 256) {
		tp = tmppal;

		for (i=256*3 ; i ; i--)
			*(tp++) = *(palette++) >> 2;

		if (vga_oktowrite())
			vga_setpalvec(0, 256, tmppal);
    }
}

void VID_ShiftPalette (unsigned char *p)
{
	VID_SetPalette(p);
}

int VID_SetMode (int modenum, unsigned char *palette)
{
	int bsize, zsize, tsize;


	if ((modenum >= num_modes) || (modenum < 0) || !modes[modenum].width) {
		Cvar_SetValue (&vid_mode, (float)current_mode);
		Com_Printf ("No such video mode: %d\n",modenum);
		return 0;
	}

	Cvar_SetValue (&vid_mode, (float)modenum);

	current_mode = modenum;

	vid.width = modes[current_mode].width;
	vid.height = modes[current_mode].height;

	VGA_width = modes[current_mode].width;
	VGA_height = modes[current_mode].height;
	VGA_planar = modes[current_mode].bytesperpixel == 0;
	VGA_rowbytes = modes[current_mode].linewidth;
	vid.rowbytes = modes[current_mode].linewidth;

	if (VGA_planar) {
		VGA_bufferrowbytes = modes[current_mode].linewidth * 4;
		vid.rowbytes = modes[current_mode].linewidth*4;
	}

	vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
	vid.colormap = (pixel_t *) host_colormap;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	vid.numpages = 1;

	// alloc zbuffer and surface cache
	if (d_pzbuffer) {
		D_FlushCaches();
		Hunk_FreeToHighMark (VID_highhunkmark);
		d_pzbuffer = NULL;
		vid_surfcache = NULL;
	}

	bsize = vid.rowbytes * vid.height;
	tsize = D_SurfaceCacheForRes (vid.width, vid.height);
	zsize = vid.width * vid.height * sizeof(*d_pzbuffer);

	VID_highhunkmark = Hunk_HighMark ();

	d_pzbuffer = (short *) Hunk_HighAllocName (bsize+tsize+zsize, "video");

	vid_surfcache = ((byte *)d_pzbuffer) + zsize;

	vid.buffer = (pixel_t *)(((byte *)d_pzbuffer) + zsize + tsize);

	D_InitCaches (vid_surfcache, tsize);

	// get goin'
	vga_setmode(current_mode);
	VID_SetPalette(palette);

	VGA_pagebase = vid.direct = framebuffer_ptr = vga_getgraphmem();
//		if (vga_setlinearaddressing()>0)
//			framebuffer_ptr = (char *) vga_getgraphmem();
	if (!framebuffer_ptr)
		Sys_Error("This mode isn't hapnin'\n");

	vga_setpage(0);

	svgalib_inited=1;

	vid.recalc_refdef = 1; // force a surface cache flush

	return 0;
}

void VID_Init (unsigned char *palette)
{
	int w = 0, h = 0;


	if (svgalib_inited)
		return;

	vga_init();

	VID_InitModes();

	Cvar_SetCurrentGroup(CVAR_GROUP_VIDEO);
	Cvar_Register (&vid_ref);
	Cvar_Register (&vid_mode);
	Cvar_Register (&vid_redrawfull);
	Cvar_Register (&vid_vsync);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("vid_modelist", VID_ModeList_f);
//	Cmd_AddCommand("vid_debug", VID_Debug_f);
//	Cmd_AddCommand ("gamma", VID_Gamma_f);

	// interpret command-line params

	if (COM_CheckParm("-mode"))
		current_mode = get_mode(COM_Argv(COM_CheckParm("-mode") + 1), w, h, 8);

	if (COM_CheckParm("-width"))
		w = Q_atoi(COM_Argv(COM_CheckParm("-width") + 1));

	if (COM_CheckParm("-height"))
		h = Q_atoi(COM_Argv(COM_CheckParm("-height") + 1));

	if (w || h)
		current_mode = get_mode(0, w, h, 8);

	if (!current_mode)
		current_mode = G320x200x256;

	// set vid parameters
	VID_SetMode(current_mode, palette);

	VID_SetPalette(palette);

	// we do want to run in the background when switched away
	vga_runinbackground(1);

	if (keyboard_init())
		Sys_Error("keyboard_init() failed");

	keyboard_seteventhandler(keyhandler);
}

void VID_Update (vrect_t *rects)
{
	if (!svgalib_inited)
		return;

	if (!vga_oktowrite())
		return; // can't update screen if it's not active

	if ((int) vid_vsync.value)
		vga_waitretrace();

	if (VGA_planar) {
		VGA_UpdatePlanarScreen (vid.buffer);
	} else if (vid_redrawfull.value) {
		int total = vid.rowbytes * vid.height;
		int offset;

		for (offset=0;offset<total;offset+=0x10000) {
			vga_setpage(offset/0x10000);
			memcpy(framebuffer_ptr,
					vid.buffer + offset,
					((total-offset>0x10000)?0x10000:(total-offset)));
		}
	} else {
		int ycount, offset, vidpage = 0;

		vga_setpage(0);

		// oppymv 010904
		if (!cls.mvdplayback || !cl_multiview.value || (cl_multiview.value>0 && CURRVIEW == 1)) {
			while (rects) {
				ycount = rects->height;
				offset = rects->y * vid.rowbytes + rects->x;
				while (ycount--) {
					register int i = offset % 0x10000;

					if (offset / 0x10000 != vidpage) {
						vidpage=offset / 0x10000;
						vga_setpage(vidpage);
					}

					if (rects->width + i > 0x10000) {
						memcpy(framebuffer_ptr + i, vid.buffer + offset, 0x10000 - i);
						vga_setpage(++vidpage);
						memcpy(framebuffer_ptr,	vid.buffer + offset + 0x10000 - i, 	rects->width - 0x10000 + i);
					} else {
						memcpy(framebuffer_ptr + i, vid.buffer + offset, rects->width);
					}

					offset += vid.rowbytes;
				}

				rects = rects->pnext;
			}
		}
	}

	if (vid_mode.value != current_mode)
		VID_SetMode ((int)vid_mode.value, vid_current_palette);
}

void Sys_SendKeyEvents(void)
{
	if (!svgalib_inited)
		return;

	while (keyboard_update());
}

static void mousehandler (int buttonstate, int dx, int dy, int dz, int drx, int dry, int drz)
{
	int i;
	static int oldbuttonstate;
	static int svga_buttonflags[] = {MOUSE_LEFTBUTTON, MOUSE_RIGHTBUTTON, MOUSE_MIDDLEBUTTON};

	mx += dx;
	my += dy;

	for (i = 0; i < sizeof(svga_buttonflags) / sizeof(svga_buttonflags[0]); i++) {
		if ((buttonstate & svga_buttonflags[i]) && !(oldbuttonstate & svga_buttonflags[i]))
			Key_Event(K_MOUSE1 + i, true);
		else if (!(buttonstate & svga_buttonflags[i]) && (oldbuttonstate & svga_buttonflags[i]))
			Key_Event(K_MOUSE1 + i, false);
	}

	if (drx < 0) {
		Key_Event(K_MWHEELUP, true);
		Key_Event(K_MWHEELUP, false);
	} else if (drx > 0) {
		Key_Event(K_MWHEELDOWN, true);
		Key_Event(K_MWHEELDOWN, false);
	}

	oldbuttonstate = buttonstate;
}

void IN_StartupMouse (void)
{
	int mouserate = 1200;
	char *mousedev = "/dev/mouse";
	int mtype = vga_getmousetype();

	if (COM_CheckParm("-mdev"))
		mousedev = COM_Argv(COM_CheckParm("-mdev") + 1);

	if (COM_CheckParm("-mrate"))
		mouserate = atoi(COM_Argv(COM_CheckParm("-mrate") + 1));

	if (mouse_init (mousedev, mtype, mouserate)) {
		Com_Printf ("No mouse found\n");
	} else {
		mouseinitialized = true;
		mouse_seteventhandler ((void *) mousehandler);
	}
}

void IN_Commands (void) {}
void VID_SetCaption (char *text) {}

// QW262 -->
/*
===================
VID_Console functions map "window" coordinates into "console".
===================
*/
int VID_ConsoleX (int x)
{
	return (x * vid.conwidth / VGA_width);
}

int VID_ConsoleY (int y)
{
	return (y * vid.conheight / VGA_height);
}
// <-- QW262
