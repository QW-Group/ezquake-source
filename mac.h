/*
 *  Mac Quakeworld
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id: mac.h,v 1.4 2005-10-23 17:44:31 disconn3ct Exp $
 */

#ifndef mac_h
#define mac_h

// Video Prefs

#define MAX_VIDEODEVICES 8	// This is probably overkill (Most macs only have 3 or 4 slots)
#define MAX_DISPLAYMODES 72 // This is NOT overkill!! (GF3 on OS9 returns > 50)
#define USEDSPFADES 		// Comment this out for debugging

// OpenGL Vendor flags (gathered for each device at startup)
#define MAC_GL_GENERIC		0	// No special hacks required (NVidia)
#define MAC_GL_3DFX			1	// Device is a 3Dfx card
#define MAC_GL_V2_V3		2	// Device is either a Voodoo2 or Voodoo3 (16bit)
#define	MAC_GL_ATI			4	// Device is an ATI card
#define	MAC_GL_RADEON		8	// Device is a Radeon class ATI card
#define MAC_GL_FULLSCREEN	16	// Can use aglSetFullScreen

// Group all variables that make up the display in a single struct
// This info is saved in the mac prefs and restored on the next run...
typedef struct
{
	UInt16	screen;			// vid_devices[gl_vid_screen.value]
	UInt16	mode;			// vid_modes[gl_vid_screen.value][gl_vid_mode.value]
	UInt16	colorbits;		// gl_vid_colorbits.value (16|32)
	UInt16	texturebits;	// gl_texturebits.value (16|32)
	Boolean	window;			// gl_vid_windowed.value (0|1)
}
video_profile_t;

// Keep a list of devices found at startup for easy access
typedef struct
{
	DisplayIDType 	id;
	GDHandle		handle;
	UInt16			numModes;	// number of valid modes found for this devices
	UInt8			vendor;		// OpenGL Vendor flags			
}
video_device_t;

// Colour bits is kept seperate - all modes *must* support 16 and 32 bit to make it to this point.
typedef struct
{
	UInt16	width;
	UInt16	height;
	Fixed	refresh;
}
video_mode_t;

// A nice structured list of pointers to use as our global video settings
typedef struct
{
	video_mode_t	*mode;
	video_device_t	*device;
	video_profile_t *profile;
}
video_ptr_t;

extern video_ptr_t		gScreen;
extern video_profile_t 	vid_currentprofile; // This is our valid, active screen
extern video_profile_t 	vid_newprofile;		// vid_restart fills this to determine what's changed
extern int numDevices;
extern cvar_t 			vid_mode,
						gl_vid_screen,
						gl_vid_windowed,
						vid_vsync,
						gl_vid_colorbits,
						gl_texturebits;
						
extern video_mode_t 	vid_modes[MAX_VIDEODEVICES][MAX_DISPLAYMODES];
extern video_device_t 	vid_devices[MAX_VIDEODEVICES];
extern int 				numDevices;

extern Point glWindowPos;	// The last window position
extern qbool suspend_mouse, inwindow, background;

void VID_GetVideoModesForActiveDisplays (void); // We call this right away in sys_mac.c
void VID_FadeOut (void);
void VID_FadeIn (void);

// Sound rate menu
enum { kMenuSound11, kMenuSound22, kMenuSound44 };

// Bit depth / texture depth menus
enum { kMenuBpp16, kMenuBpp32 };

extern char *kAppName;
#define kCreatorType	'GLQW'
#define kPrefsName	"ezQuake.plist"
#endif
