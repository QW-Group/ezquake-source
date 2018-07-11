/*
Copyright (C) 2002-2003 A Nourai

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

// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include <SDL.h>

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"
#include "image.h"

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

int anisotropy_ext = 0;

qbool gl_mtexable = false;
int gl_textureunits = 1;
glMultiTexCoord2f_t qglMultiTexCoord2f = NULL;
glActiveTexture_t qglActiveTexture = NULL;
glClientActiveTexture_t qglClientActiveTexture = NULL;

byte color_white[4] = {255, 255, 255, 255};
byte color_black[4] = {0, 0, 0, 255};

static void GL_CheckMultiTextureExtensions(void);
static void OnChange_gl_ext_texture_compression(cvar_t *, char *, qbool *);
extern cvar_t vid_renderer;

cvar_t gl_strings                 = {"gl_strings", "", CVAR_ROM | CVAR_SILENT};
cvar_t gl_ext_texture_compression = {"gl_ext_texture_compression", "0", CVAR_SILENT, OnChange_gl_ext_texture_compression};
cvar_t gl_maxtmu2                 = {"gl_maxtmu2", "0", CVAR_LATCH};

// GL_ARB_texture_non_power_of_two
cvar_t gl_ext_arb_texture_non_power_of_two = {"gl_ext_arb_texture_non_power_of_two", "1", CVAR_LATCH};

/************************************* EXTENSIONS *************************************/

static qbool R_InitialiseRenderer(void)
{
	qbool shaders_supported = false;

	R_InitialiseBufferHandling();
	GL_InitialiseFramebufferHandling();

	shaders_supported = GLM_LoadProgramFunctions();
	shaders_supported = GLM_LoadStateFunctions() && shaders_supported;
	shaders_supported = GLM_LoadTextureManagementFunctions() && shaders_supported;
	shaders_supported = GLM_LoadDrawFunctions() && shaders_supported;

	GL_LoadTextureManagementFunctions();
	GL_LoadDrawFunctions();
	GL_InitialiseDebugging();

	if (R_UseModernOpenGL() && shaders_supported) {
		Con_Printf("&c0f0Renderer&r: OpenGL (GLSL)\n");
		return true;
	}
	else if (R_UseImmediateOpenGL()) {
		Con_Printf("&c0f0Renderer&r: OpenGL (classic)\n");
		return true;
	}

	return false;
}

static void GL_CheckShaderExtensions(void)
{
	if (!R_InitialiseRenderer()) {
		Con_Printf("Failed to initialised desired renderer, falling back to classic OpenGL\n");
		Cvar_LatchedSetValue(&vid_renderer, 0);

		if (!R_InitialiseRenderer()) {
			Sys_Error("Failed to initialise graphics renderer");
		}
	}
}

static void OnChange_gl_ext_texture_compression(cvar_t *var, char *string, qbool *cancel) {
	float newval = Q_atof(string);

	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA8;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB8;
}

/************************************** GL INIT **************************************/

static void GL_CheckExtensions(void)
{
	GL_CheckMultiTextureExtensions();

	if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
		int gl_anisotropy_factor_max;

		anisotropy_ext = 1;

		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_anisotropy_factor_max);

		Com_Printf_State(PRINT_OK, "Anisotropic Filtering Extension Found (%d max)\n", gl_anisotropy_factor_max);
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_texture_compression")) {
		Com_Printf_State(PRINT_OK, "Texture compression extensions found\n");
		Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
		Cvar_Register(&gl_ext_texture_compression);
		Cvar_ResetCurrentGroup();
	}

	// GL_ARB_texture_non_power_of_two
	// NOTE: we always register cvar even if ext is not supported.
	// cvar added just to be able force OFF an extension.
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_ext_arb_texture_non_power_of_two);
	Cvar_ResetCurrentGroup();

	{
		qbool supported = gl_ext_arb_texture_non_power_of_two.integer && SDL_GL_ExtensionSupported("GL_ARB_texture_non_power_of_two");

		R_SetNonPowerOfTwoSupport(supported);

		Com_Printf_State(PRINT_OK, "GL_ARB_texture_non_power_of_two extension %s\n",
			supported ? "found" : "not found");
	}
}

static void GL_CheckMultiTextureExtensions(void)
{
	extern cvar_t gl_maxtmu2;

	if (!COM_CheckParm(cmdline_param_client_nomultitexturing) && SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		if (strstr(gl_renderer, "Savage")) {
			return;
		}
		qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		if (!qglActiveTexture) {
			qglActiveTexture = SDL_GL_GetProcAddress("glActiveTextureARB");
		}
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
		if (!qglMultiTexCoord2f || !qglActiveTexture || !qglClientActiveTexture) {
			return;
		}
		Com_Printf_State(PRINT_OK, "Multitexture extensions found\n");
		gl_mtexable = true;
	}

	gl_textureunits = min(glConfig.texture_units, 4);

	if (COM_CheckParm(cmdline_param_client_maximum2textureunits) /*|| !strcmp(gl_vendor, "ATI Technologies Inc.")*/ || gl_maxtmu2.value) {
		gl_textureunits = min(gl_textureunits, 2);
	}

	if (gl_textureunits < 2) {
		gl_mtexable = false;
	}

	if (!gl_mtexable) {
		gl_textureunits = 1;
	}
	else {
		Com_Printf_State(PRINT_OK, "Enabled %i texture units on hardware\n", gl_textureunits);
	}
}

void GL_Init(void)
{
	gl_vendor = (const char*)glGetString(GL_VENDOR);
	gl_renderer = (const char*)glGetString(GL_RENDERER);
	gl_version = (const char*)glGetString(GL_VERSION);
	if (GL_UseGLSL()) {
		gl_extensions = "(using modern OpenGL)\n";
	}
	else {
		gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
	}

#if !defined( _WIN32 ) && !defined( __linux__ ) /* we print this in different place on WIN and Linux */
	/* FIXME/TODO: FreeBSD too? */
	Com_Printf_State(PRINT_INFO, "GL_VENDOR: %s\n", gl_vendor);
	Com_Printf_State(PRINT_INFO, "GL_RENDERER: %s\n", gl_renderer);
	Com_Printf_State(PRINT_INFO, "GL_VERSION: %s\n", gl_version);
#endif

	if (COM_CheckParm(cmdline_param_client_printopenglextensions)) {
		Com_Printf_State(PRINT_INFO, "GL_EXTENSIONS: %s\n", gl_extensions);
	}

	Cvar_Register(&gl_strings);
	Cvar_ForceSet(&gl_strings,
		va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		   "GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));
	Cvar_Register(&gl_maxtmu2);

	GL_CheckShaderExtensions();
	GL_StateDefaultInit();
	GL_CheckExtensions();
}
