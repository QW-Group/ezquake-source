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

#ifndef EZQUAKE_VID_HEADER
#define EZQUAKE_VID_HEADER

#undef strlen
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
	pixel_t*        colormap;       // 256 * VID_GRADES size
	int             width;
	int             height;
	float           aspect;         // width / height -- < 0 is taller than wide
	int             numpages;
	int             recalc_refdef;  // if true, recalc vid-based stuff
	int             conwidth;
	int             conheight;
} viddef_t;

extern viddef_t vid; // global video state
extern unsigned short d_8to16table[256];
extern unsigned	d_8to24table[256];
extern unsigned d_8to24table2[256];
extern qbool vid_windowedmouse;
extern void (*vid_menudrawfn)(void);
extern void (*vid_menukeyfn)(int key);

void VID_SetPalette (unsigned char *palette);
// called at startup and after any gamma correction

void VID_Init (unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

int VID_ScaledWidth3D(void);
int VID_ScaledHeight3D(void);

int VID_RenderWidth2D(void);
int VID_RenderHeight2D(void);

void VID_Shutdown(qbool restart);
void VID_SoftRestart(void);
// Called at shutdown

// int VID_SetMode (int modenum, unsigned char *palette);
// sets the mode; only used by the Quake engine for resetting to mode 0 (the
// base mode) on memory allocation failures

qbool IN_QuakeMouseCursorRequired (void);
qbool IN_MouseTrackingRequired (void);

void VID_NotifyActivity(void);

void VID_SetCaption (char *text);

int VID_SetDeviceGammaRamp (unsigned short *ramps);
extern qbool vid_hwgamma_enabled;

extern int glx, gly, glwidth, glheight;

void VID_Minimize(void);
void VID_Restore(void);

qbool VID_VSyncIsOn(void);
qbool VID_VSyncLagFix(void);
qbool VID_VsyncLagEnabled(void);
void VID_RenderFrameEnd(void);

const SDL_DisplayMode *VID_GetDisplayMode(int index);
int VID_GetCurrentModeIndex(void);
int VID_GetModeIndexCount(void);

void VID_ReloadCheck(void);

#endif // EZQUAKE_VID_HEADER
