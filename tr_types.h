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


#define PRINT_R_VERBOSE ( r_verbose.integer ? PRINT_ALL : PRINT_DBG )

/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/


#define	MAX_STRING_CHARS	1024
#define	BIG_INFO_STRING		8192

typedef enum {
	GLDRV_ICD,					// driver is integrated with window system
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO				// driver is a 3Dfx standalone driver
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			// where everthing works the way it should
	GLHW_3DFX_2D3D,			// Voodoo Banshee or Voodoo3, relevant since if this is
						// the hardware type then there can NOT exist a secondary
						// display adapter
	GLHW_RIVA128,			// where you can't interpolate alpha
	GLHW_RAGEPRO,			// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2,			// where you don't have src*dst
	GLHW_INTEL				// Causes flickering console if you write directly to the front 
						// buffer and then flip the back buffer for instance when drawing
						// the I/O icon or doing timerefresh.
						// http://www.intel.com/cd/ids/developer/asmo-na/eng/168252.htm?page=7
} glHardwareType_t;

typedef struct {
	const unsigned char                     *renderer_string;
	const unsigned char                     *vendor_string;
	const unsigned char                     *version_string;
	const unsigned char                     *extensions_string;

	int						maxTextureSize;			// queried from GL

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qbool				isFullscreen;
	qbool				stereoEnabled;
} glconfig_t;

extern glconfig_t	glConfig;

#if defined(Q3_VM) || defined(_WIN32)
// TO BE REMOVED
#define _3DFX_DRIVER_NAME	"3dfxvgl"
#define OPENGL_DRIVER_NAME	"opengl32"

#else

#define _3DFX_DRIVER_NAME	"libMesaVoodooGL.so"
#define OPENGL_DRIVER_NAME	"libGL.so.1"

#endif	// !defined _WIN32

//===================================================
//
// something what we need to declare from tr_init.c
//
//===================================================

//
// latched variables that can only change over a restart
//

extern cvar_t	r_glDriver;
extern cvar_t	r_allowExtensions;

//extern cvar_t r_texturebits;
extern cvar_t	r_colorbits;
extern cvar_t	r_stereo;
extern cvar_t	r_stencilbits;
extern cvar_t	r_depthbits;
//extern cvar_t	r_overBrightBits;
extern cvar_t	r_mode;
extern cvar_t	r_fullscreen;
extern cvar_t	r_width;
extern cvar_t	r_height;
extern cvar_t	r_win_width;
extern cvar_t	r_win_height;
extern cvar_t	r_win_save_pos;
extern cvar_t	r_win_save_size;
extern cvar_t	r_customaspect; // qqshka: unused even in q3, but I keep cvar, just do not register it
extern cvar_t	r_displayRefresh;
extern cvar_t	vid_borderless;
//extern cvar_t	r_intensity;

//
// archived variables that can change at any time
//
extern cvar_t	r_ignoreGLErrors;
//extern cvar_t	r_textureMode;
extern cvar_t	r_swapInterval;
//extern cvar_t	r_gamma;

extern cvar_t	vid_xpos;
extern cvar_t	vid_ypos;
extern cvar_t	vid_minpos;
extern cvar_t	r_conwidth;
extern cvar_t	r_conheight;
extern cvar_t	vid_ref;
extern cvar_t	vid_hwgammacontrol;
#ifdef _WIN32
extern cvar_t	vid_flashonactivity;
#endif

extern cvar_t	r_verbose;

extern cvar_t r_showextensions;

void GL_SetDefaultState( void );
qbool R_GetModeInfo( int *width, int *height, float *windowAspect, int mode );
void RE_Init( void );
void RE_Shutdown( qbool destroyWindow );

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_Init( void );
void		GLimp_Shutdown( void );
void		GLimp_EndFrame( void );
void		GLimp_LogComment( char *comment );


#endif	// __TR_TYPES_H
