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
} opengl_version_t;

static opengl_version_t versions[] = {
	{ 4, 6, false },
	{ 4, 5, false },
	{ 4, 4, false },
	{ 4, 3, false },
	{ 4, 6, true },
	{ 4, 5, true },
	{ 4, 4, true },
	{ 4, 3, true },
};

qbool GLM_SDL_SetupAttributes(int attempt)
{
	int contextFlags = 0;
	extern cvar_t vid_gl_core_profile;

	// This will make
	if (vid_gl_core_profile.integer) {
		while (attempt < sizeof(versions) / sizeof(versions[0]) && !versions[attempt].core) {
			++attempt;
		}
	}

	if (attempt >= sizeof(versions) / sizeof(versions[0])) {
		return false;
	}

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
	if (R_DebugProfileContext()) {
		contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);

	return true;
}
