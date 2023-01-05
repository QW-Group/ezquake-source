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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include <SDL.h>
#include "quakedef.h"
#include "gl_local.h"

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

SDL_GLContext GLM_SDL_CreateContext(SDL_Window* window)
{
	return GL_SDL_CreateBestContext(window, versions, sizeof(versions) / sizeof(versions[0]));
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
