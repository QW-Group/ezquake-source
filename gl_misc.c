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

#ifdef GL_PARANOIA
void GL_ProcessErrors(const char* message)
{
	GLenum error = glGetError();
	while (error != GL_NO_ERROR) {
		Con_Printf("%s> = %X\n", message, error);
		error = glGetError();
	}
}
#endif

void GL_PrintGfxInfo(void)
{
	SDL_DisplayMode current;
	GLint num_extensions;
	int i;

	Com_Printf_State(PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string);
	Com_Printf_State(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	Com_Printf_State(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	if (R_UseModernOpenGL()) {
		Com_Printf_State(PRINT_ALL, "GLSL_VERSION: %s\n", glConfig.glsl_version);
		Com_Printf_State(PRINT_ALL, "MAX_3D_TEXTURE_SIZE: %d (depth %d)\n", glConfig.max_3d_texture_size, glConfig.max_texture_depth);
	}

	if (r_showextensions.integer) {
		Com_Printf_State(PRINT_ALL, "GL_EXTENSIONS: ");
		if (GL_VersionAtLeast(3, 0)) {
			typedef const GLubyte* (APIENTRY *glGetStringi_t)(GLenum name, GLuint index);
			glGetStringi_t glGetStringi = (glGetStringi_t)SDL_GL_GetProcAddress("glGetStringi");
			if (glGetStringi) {
				glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
				for (i = 0; i < num_extensions; ++i) {
					Com_Printf_State(PRINT_ALL, "%s%s", i ? " " : "", glGetStringi(GL_EXTENSIONS, i));
				}
				Com_Printf_State(PRINT_ALL, "\n");
			}
			else {
				Com_Printf_State(PRINT_ALL, "(list unavailable)\n");
			}
		}
		else {
			Com_Printf_State(PRINT_ALL, "%s\n", (const char*)glGetString(GL_EXTENSIONS));
		}
	}

	Com_Printf_State(PRINT_ALL, "PIXELFORMAT: color(%d-bits) Z(%d-bit)\n             stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);

	if (SDL_GetCurrentDisplayMode(VID_DisplayNumber(r_fullscreen.value), &current) != 0) {
		current.refresh_rate = 0; // print 0Hz if we run into problem fetching data
	}

	Com_Printf_State(PRINT_ALL, "MAX_TEXTURE_SIZE: %d\n", glConfig.gl_max_size_default);
	Com_Printf_State(PRINT_ALL, "MAX_TEXTURE_IMAGE_UNITS: %d\n", glConfig.texture_units);
	Com_Printf_State(PRINT_ALL, "MODE: %d x %d @ %d Hz [%s]\n", current.w, current.h, current.refresh_rate, r_fullscreen.integer ? "fullscreen" : "windowed");
	Com_Printf_State(PRINT_ALL, "RATIO: %f\n", vid.aspect);
	Com_Printf_State(PRINT_ALL, "CONRES: %d x %d\n", r_conwidth.integer, r_conheight.integer);
}

void GL_Viewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

const char* GL_DescriptiveString(void)
{
	return (const char*)glGetString(GL_RENDERER);
}
