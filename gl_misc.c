/*
Copyright (C) 1996-1997 Id Software, Inc.

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
*/
// gl_misc.c

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"

void GL_Clear(qbool clear_color)
{
	glClear((clear_color ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);
}

void GL_EnsureFinished(void)
{
	glFinish();
}

void GL_Screenshot(byte* buffer, size_t size)
{
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);
}

void GL_PopulateConfig(void)
{
	int r, g, b, a;

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);

	glConfig.colorBits = r + g + b + a;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &glConfig.depthBits);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &glConfig.stencilBits);

	glConfig.vendor_string = glGetString(GL_VENDOR);
	glConfig.renderer_string = glGetString(GL_RENDERER);
	glConfig.version_string = glGetString(GL_VERSION);

	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &glConfig.majorVersion);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glConfig.minorVersion);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.gl_max_size_default);
	if (GL_VersionAtLeast(2, 0)) {
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &glConfig.texture_units);
	}
	else {
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glConfig.texture_units);
	}
	if (GL_VersionAtLeast(4, 3)) {
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size);
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &glConfig.max_texture_depth);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &glConfig.uniformBufferOffsetAlignment);
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &glConfig.shaderStorageBufferOffsetAlignment);
		glConfig.glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
	}
	else {
		glConfig.max_3d_texture_size = 0;
		glConfig.max_texture_depth = 0;
		glConfig.uniformBufferOffsetAlignment = 0;
		glConfig.shaderStorageBufferOffsetAlignment = 0;
		glConfig.glsl_version = (unsigned char*)"0";
	}
}
