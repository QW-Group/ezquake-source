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

#include <SDL.h>
#include "quakedef.h"
#include "gl_local.h"

typedef struct opengl_version_s {
	int majorVersion;
	int minorVersion;
	qbool core;
	qbool legacy;
} opengl_version_t;

static opengl_version_t versions[] = {
	{ 0, 0, false, true },
};

qbool GLC_SDL_SetupAttributes(int attempt)
{
	extern cvar_t vid_gl_core_profile;

	// This will make some modes be attempted multiple times (int* attempt?)
	if (vid_gl_core_profile.integer) {
		while (attempt < sizeof(versions) / sizeof(versions[0]) && !versions[attempt].core) {
			++attempt;
		}
	}

	if (attempt >= sizeof(versions) / sizeof(versions[0])) {
		return false;
	}

	if (versions[attempt].legacy) {
		// SDL defaults - take what we can get.  SDL rules are profile_mask == flags == 0 && majorversion < 3 => legacy profile
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	}
	else {
		int contextFlags = 0;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, versions[attempt].majorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, versions[attempt].minorVersion);
		if (versions[attempt].core) {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
			// https://www.khronos.org/opengl/wiki/OpenGL_Context
			// Recommendation: You should use the forward compatibility bit only if you need compatibility with MacOS.
			// That API requires the forward compatibility bit to create any core profile context.
			contextFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#else
			contextFlags |= COM_CheckParm(cmdline_param_client_forwardonlyprofile) ? SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG : 0;
#endif
		}
		else {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		}
		contextFlags |= R_DebugProfileContext() ? SDL_GL_CONTEXT_DEBUG_FLAG : 0;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	}

	return attempt == 0;
}
