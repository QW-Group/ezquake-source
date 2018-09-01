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

static void GL_PrintInfoLine(const char* label, int labelsize, const char* fmt, ...)
{
	va_list argptr;
	char msg[128];

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf_State(PRINT_ALL, "  %-*s ", labelsize, label);
	con_margin = labelsize + 3;
	Com_Printf_State(PRINT_ALL, "%s", msg);
	con_margin = 0;
	Com_Printf_State(PRINT_ALL, "\n");
}

void GL_PrintGfxInfo(void)
{
	SDL_DisplayMode current;
	GLint num_extensions;
	int i;

	Com_Printf_State(PRINT_ALL, "\nOpenGL (%s)\n", R_UseImmediateOpenGL() ? "classic" : "glsl");
	GL_PrintInfoLine("Vendor:", 9, "%s", (const char*)glConfig.vendor_string);
	GL_PrintInfoLine("Renderer:", 9, "%s", (const char*)glConfig.renderer_string);
	GL_PrintInfoLine("Version:", 9, "%s", (const char*)glConfig.version_string);
	if (R_UseModernOpenGL()) {
		GL_PrintInfoLine("GLSL:", 9, "%s", (const char*)glConfig.version_string);
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

	Com_Printf_State(PRINT_ALL, "Limits:\n");
	GL_PrintInfoLine("Texture Units:", 14, "%d", glConfig.texture_units);
	GL_PrintInfoLine("Texture Size:", 14, "%d", glConfig.gl_max_size_default);
	if (R_UseModernOpenGL()) {
		GL_PrintInfoLine("3D Textures:", 14, "%dx%dx%d\n", glConfig.max_3d_texture_size, glConfig.max_3d_texture_size, glConfig.max_texture_depth);
	}

	Com_Printf_State(PRINT_ALL, "Supported features:\n");
	GL_PrintInfoLine("Shaders:", 15, "%s", glConfig.supported_features & R_SUPPORT_RENDERING_SHADERS ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Compute:", 15, "%s", glConfig.supported_features & R_SUPPORT_COMPUTE_SHADERS ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Framebuffers:", 15, "%s", glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("Texture arrays:", 15, "%s", glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS ? "&c0f0available&r" : "&cf00unsupported&r");
	GL_PrintInfoLine("HW lighting:", 15, "%s", glConfig.supported_features & R_SUPPORT_FEATURE_HW_LIGHTING ? "&c0f0available&r" : "&cf00unsupported&r");

	if (SDL_GetCurrentDisplayMode(VID_DisplayNumber(r_fullscreen.value), &current) != 0) {
		current.refresh_rate = 0; // print 0Hz if we run into problem fetching data
	}

	Com_Printf_State(PRINT_ALL, "Video\n");
	GL_PrintInfoLine("Resolution:", 12, "%dx%d@%dhz [%s]", current.w, current.h, current.refresh_rate, r_fullscreen.integer ? "fullscreen" : "windowed");
	GL_PrintInfoLine("Format:", 12, "%2d-bit color\n%2d-bit z-buffer\n%2d-bit stencil", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
	if (vid.aspect) {
		GL_PrintInfoLine("Aspect:", 12, "%f%%", vid.aspect);
	}
	if (r_conwidth.integer || r_conheight.integer) {
		GL_PrintInfoLine("Console Res:", 12,"%d x %d", r_conwidth.integer, r_conheight.integer);
	}
}

void GL_Viewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

const char* GL_DescriptiveString(void)
{
	return (const char*)glGetString(GL_RENDERER);
}
