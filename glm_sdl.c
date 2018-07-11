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

void GLM_SDL_SetupAttributes(void)
{
	int contextFlags = 0;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	if (GL_CoreProfileContext()) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		if (GL_ForwardOnlyProfile()) {
			contextFlags |= SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
		}
	}
	else {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	}
	if (R_DebugProfileContext()) {
		contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
}
