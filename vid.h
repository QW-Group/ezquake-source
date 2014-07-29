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

	$Id: vid.h,v 1.7 2007-01-07 21:48:47 disconn3ct Exp $
*/
// vid.h -- video driver defs
#include <SDL.h>

#define VID_CBITS 6
#define VID_GRADES (1 << VID_CBITS)

// a pixel can be one, two, or four bytes
typedef byte pixel_t;

typedef struct vrect_s {
	int x,y,width,height;
	struct vrect_s *pnext;
} vrect_t;

typedef struct {
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	unsigned		rowbytes;		// may be > width if displayed in a window
	unsigned		width, actualwidth;		
	unsigned		height, actualheight;
	float			aspect;			// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	unsigned		conwidth;
	unsigned		conheight;
	pixel_t			*direct;		// direct drawing to framebuffer, if not NULL
	void 			(*blitter)(int x, int y, int *width, int *height);	// blitter function
} viddef_t;

extern viddef_t vid; // global video state
extern unsigned short d_8to16table[256];
extern unsigned	d_8to24table[256];
extern qbool vid_windowedmouse;
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);

void VID_SetPalette (unsigned char *palette);
// called at startup and after any gamma correction

void VID_ShiftPalette (unsigned char *palette);
// called for bonus and pain flashes, and for underwater color changes

void VID_Init (unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void VID_Shutdown (void);
// Called at shutdown

void VID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

// int VID_SetMode (int modenum, unsigned char *palette);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

void VID_LockBuffer (void);
void VID_UnlockBuffer (void);

void VID_NotifyActivity(void);

void VID_SetCaption (char *text);

void VID_SetDeviceGammaRamp (unsigned short *ramps);
extern qbool vid_hwgamma_enabled;

// these macros map "window" coordinates into "console"
#define VID_ConsoleX(x) ((x) * vid.conwidth / glwidth)
#define VID_ConsoleY(y) ((y) * vid.conheight / glheight)

void VID_Minimize();
void VID_Restore();

qbool VID_VSyncIsOn();
qbool VID_VSyncLagFix();
void VID_CvarInit();
void VID_GetModeList(SDL_DisplayMode **modelist_out, int *count_out);
int VID_GetCurrentModeIndex();
