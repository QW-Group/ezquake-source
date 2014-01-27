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
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H

/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/

// FIXME: Is this still relevant?
typedef enum {
	GLHW_GENERIC, // where everthing works the way it should
	GLHW_INTEL    // Causes flickering console if you write directly to the front 
	              // buffer and then flip the back buffer for instance when drawing
	              // the I/O icon or doing timerefresh.
	              // http://www.intel.com/cd/ids/developer/asmo-na/eng/168252.htm?page=7
} glHardwareType_t;

typedef struct {
	const unsigned char                     *renderer_string;
	const unsigned char                     *vendor_string;
	const unsigned char                     *version_string;
	const unsigned char                     *extensions_string;

	int					colorBits, depthBits, stencilBits;
	int					vidWidth, vidHeight;
	int					displayFrequency;

	glHardwareType_t			hardwareType;

	qbool					initialized;
} glconfig_t;

extern glconfig_t	glConfig;

//
// latched variables that can only change over a restart
//

extern cvar_t	r_allowExtensions;
extern cvar_t	r_colorbits;
extern cvar_t	r_stereo;
extern cvar_t	r_stencilbits;
extern cvar_t	r_depthbits;
extern cvar_t	r_fullscreen;
extern cvar_t	vid_width;
extern cvar_t	vid_height;
extern cvar_t	vid_win_width;
extern cvar_t	vid_win_height;
extern cvar_t	r_win_save_pos;
extern cvar_t	r_win_save_size;
extern cvar_t	r_displayRefresh;
extern cvar_t	vid_borderless;

//
// archived variables that can change at any time
//
extern cvar_t	r_swapInterval;
extern cvar_t	vid_xpos;
extern cvar_t	vid_ypos;
extern cvar_t	vid_minpos;
extern cvar_t	r_conwidth;
extern cvar_t	r_conheight;
extern cvar_t	vid_ref;
extern cvar_t	vid_flashonactivity;
extern cvar_t	r_verbose;
extern cvar_t r_showextensions;


#endif	// __TR_TYPES_H
