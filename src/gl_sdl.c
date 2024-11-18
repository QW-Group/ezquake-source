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

static void GL_SDL_SetupAttributes(const opengl_version_t* version)
{
	if (version->legacy) {
		// SDL defaults - take what we can get.  SDL rules are profile_mask == flags == 0 && majorversion < 3 => legacy profile
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	}
	else {
		int contextFlags = 0;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, version->majorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, version->minorVersion);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, version->core ? SDL_GL_CONTEXT_PROFILE_CORE : SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

#ifdef __APPLE__
		// https://www.khronos.org/opengl/wiki/OpenGL_Context
		// Recommendation: You should use the forward compatibility bit only if you need compatibility with MacOS.
		// That API requires the forward compatibility bit to create any core profile context.
		contextFlags |= version->core ? SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG : 0;
#else
		contextFlags |= version->core && COM_CheckParm(cmdline_param_client_forwardonlyprofile) ? SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG : 0;
#endif
		contextFlags |= R_DebugProfileContext() ? SDL_GL_CONTEXT_DEBUG_FLAG : 0;
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	}
}

SDL_GLContext GL_SDL_CreateBestContext(SDL_Window* window, const opengl_version_t* versions, int count)
{
	SDL_GLContext context;
	int attempt;
#ifdef _WIN32
	int accelerated = 0;

	SDL_GL_GetAttribute(SDL_GL_ACCELERATED_VISUAL, &accelerated);
#endif

	for (attempt = 0; attempt < count; ++attempt) {
		extern cvar_t vid_gl_core_profile;
		const opengl_version_t* version = &versions[attempt];

		if (vid_gl_core_profile.integer && !version->core && !version->legacy) {
			continue;
		}

		GL_SDL_SetupAttributes(version);

		context = SDL_GL_CreateContext(window);
		if (COM_CheckParm(cmdline_param_console_debug)) {
			Con_Printf("Asked for OpenGL %d.%d, got %s\n", version->majorVersion, version->minorVersion, context ? (const char*)glGetString(GL_VENDOR) : "failure");
		}

#ifdef _WIN32
		// SDL2 falls back to not even asking if it's accelerated, so we might get software :(
		if (context && (version->core || accelerated) && strstr((const char*)glGetString(GL_VENDOR), "Microsoft") != NULL) {
			if (COM_CheckParm(cmdline_param_console_debug)) {
				Con_Printf("... rejecting unaccelerated context\n");
			}
			SDL_GL_DeleteContext(context);
			context = NULL;
		}
#endif
		if (context) {
			if (glGetString(GL_VERSION) && atoi((const char*)glGetString(GL_VERSION)) >= version->majorVersion) {
				return context;
			}
			
			if (COM_CheckParm(cmdline_param_console_debug)) {
				Con_Printf("... rejecting lower OpenGL version (%d vs %d)\n", atoi((const char*)glGetString(GL_VERSION)), version->majorVersion);
			}
			SDL_GL_DeleteContext(context);
			context = NULL;
		}
	}

	return NULL;
}
