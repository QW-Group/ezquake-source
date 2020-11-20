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

#ifdef X11_GAMMA_WORKAROUND
#include <SDL.h>
#include <SDL_syswm.h>
#include <X11/extensions/xf86vmode.h>
#endif
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

#define R_SUPPORT_FRAMEBUFFERS        (1 << 0)      // rendering to framebuffers
#define R_SUPPORT_RENDERING_SHADERS   (1 << 1)      // rendering using shaders
#define R_SUPPORT_COMPUTE_SHADERS     (1 << 2)      // non-rendering shaders
#define R_SUPPORT_PRIMITIVERESTART    (1 << 3)      // primitive restart indexes
#define R_SUPPORT_MULTITEXTURING      (1 << 4)      // multi-texturing (some people still disable this...)
#define R_SUPPORT_IMAGE_PROCESSING    (1 << 5)      // reading/writing to images
#define R_SUPPORT_TEXTURE_SAMPLERS    (1 << 6)      // samplers
#define R_SUPPORT_TEXTURE_ARRAYS      (1 << 7)      // 3D images (texture arrays)
#define R_SUPPORT_INDIRECT_RENDERING  (1 << 8)      // indirect rendering (api parameters stored in buffer)
#define R_SUPPORT_INSTANCED_RENDERING (1 << 9)      // instanced rendering
#define R_SUPPORT_FRAMEBUFFERS_BLIT   (1 << 10)     // blit from one framebuffer to another
#define R_SUPPORT_BGRA_LIGHTMAPS      (1 << 11)     // BGRA lightmaps (if optimal format)
#define R_SUPPORT_INT8888R_LIGHTMAPS  (1 << 12)     // Lightmaps uploaded as UINT8888R rather than UNSIGNED_BYTE
#define R_SUPPORT_SEAMLESS_CUBEMAPS   (1 << 13)     // filtering works across faces of the cubemap
#define R_SUPPORT_DEPTH32F            (1 << 14)     // floating point 32-bit depth buffers
#define R_SUPPORT_FRAMEBUFFERS_SRGB   (1 << 15)     // framebuffers support sRGB
#define R_SUPPORT_IMMEDIATEMODE       (1 << 16)     // immediate-mode rendering (doesn't require programs)
#define R_SUPPORT_FOG                 (1 << 17)     // fog (OpenGL 1.4+, not currently working)
#define R_SUPPORT_CUBE_MAPS           (1 << 18)     // cube maps (OpenGL 1.3+)

#define R_SUPPORT_FEATURE_HW_LIGHTING (R_SUPPORT_TEXTURE_ARRAYS | R_SUPPORT_COMPUTE_SHADERS | R_SUPPORT_IMAGE_PROCESSING)

#define R_SUPPORT_MODERN_OPENGL_REQUIREMENTS ( \
	R_SUPPORT_FRAMEBUFFERS | R_SUPPORT_RENDERING_SHADERS | R_SUPPORT_COMPUTE_SHADERS | R_SUPPORT_PRIMITIVERESTART | \
	R_SUPPORT_MULTITEXTURING | R_SUPPORT_IMAGE_PROCESSING | R_SUPPORT_TEXTURE_SAMPLERS | R_SUPPORT_TEXTURE_ARRAYS | \
	R_SUPPORT_INDIRECT_RENDERING | R_SUPPORT_INSTANCED_RENDERING \
)

typedef struct {
	const unsigned char                     *renderer_string;
	const unsigned char                     *vendor_string;
	const unsigned char                     *version_string;
	const unsigned char                     *glsl_version;

	int		colorBits, depthBits, stencilBits;
	int		vidWidth, vidHeight;
	int		displayFrequency;
	int     majorVersion;
	int     minorVersion;

	glHardwareType_t			hardwareType;

	qbool					initialized;
#ifdef X11_GAMMA_WORKAROUND
	struct {
		SDL_SysWMinfo info;
		Display *display;
		int screen;
		int size;
	} gammacrap;
#endif

	int gl_max_size_default;
	int max_3d_texture_size;
	int max_texture_depth;
	int texture_units;

	int tripleBufferIndex;
	int uniformBufferOffsetAlignment;
	int shaderStorageBufferOffsetAlignment;

	unsigned int supported_features;
	unsigned int preferred_format;
	unsigned int preferred_type;

	qbool reversed_depth;
} glconfig_t;

#define GL_Supported(x) ((glConfig.supported_features & (x)) == (x))

extern glconfig_t	glConfig;

//
// latched variables that can only change over a restart
//

extern cvar_t	r_colorbits;
extern cvar_t	r_24bit_depth;
extern cvar_t	r_fullscreen;
extern cvar_t	vid_width;
extern cvar_t	vid_height;
extern cvar_t	vid_win_width;
extern cvar_t	vid_win_height;
extern cvar_t	r_win_save_pos;
extern cvar_t	r_win_save_size;
extern cvar_t	r_displayRefresh;
extern cvar_t	vid_borderless;
extern cvar_t   vid_usedesktopres;

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
