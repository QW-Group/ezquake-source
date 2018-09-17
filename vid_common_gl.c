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
#include "r_texture.h"
#include "r_renderer.h"

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

static cvar_t gl_ext_texture_compression = {"gl_ext_texture_compression", "0", CVAR_SILENT, OnChange_gl_ext_texture_compression};
static cvar_t gl_maxtmu2                 = {"gl_maxtmu2", "0", CVAR_LATCH};

// GL_ARB_texture_non_power_of_two
cvar_t gl_ext_arb_texture_non_power_of_two = {"gl_ext_arb_texture_non_power_of_two", "1", CVAR_LATCH};

/************************************* EXTENSIONS *************************************/

static qbool GL_InitialiseRenderer(void)
{
	qbool shaders_supported = false;

	glConfig.supported_features = 0;

	GL_InitialiseFramebufferHandling();
	GL_LoadProgramFunctions();
	GL_LoadStateFunctions();
	GL_LoadTextureManagementFunctions();
	GL_LoadDrawFunctions();
	GL_InitialiseDebugging();

	shaders_supported = (glConfig.supported_features & R_SUPPORT_MODERN_OPENGL_REQUIREMENTS);
	if ((glConfig.preferred_format == 0 && GL_VersionAtLeast(1, 2)) || glConfig.preferred_format == GL_BGRA) {
		glConfig.supported_features |= R_SUPPORT_BGRA_LIGHTMAPS;
	}
	if ((glConfig.preferred_type == 0 && GL_VersionAtLeast(1, 2)) || glConfig.preferred_type == GL_UNSIGNED_INT_8_8_8_8_REV) {
		glConfig.supported_features |= R_SUPPORT_INT8888R_LIGHTMAPS;
	}

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

static void OnChange_gl_ext_texture_compression(cvar_t *var, char *string, qbool *cancel)
{
	if (renderer.TextureCompressionSet) {
		renderer.TextureCompressionSet(Q_atof(string));
	}
}

/************************************** GL INIT **************************************/

static void GL_PopulateConfig(void)
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

	if (glConfig.version_string) {
		const char* dot = strchr((const char*)glConfig.version_string, '.');
		glConfig.majorVersion = atoi((const char*)glConfig.version_string);
		glConfig.minorVersion = (dot ? atoi(dot + 1) : 0);
	}
	else {
		glConfig.majorVersion = 2;
		glConfig.minorVersion = 1;
	}

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.gl_max_size_default);
	if (R_UseImmediateOpenGL()) {
		if (GL_VersionAtLeast(2, 1)) {
			glGetIntegerv(GL_MAX_TEXTURE_UNITS, &glConfig.texture_units);
		}
		else if (SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
			glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glConfig.texture_units);
		}
		else {
			glConfig.texture_units = 1;
		}
	}
	else {
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &glConfig.texture_units);
	}
	glConfig.texture_units = max(glConfig.texture_units, 1);

	glConfig.max_3d_texture_size = 0;
	glConfig.glsl_version = (unsigned char*)"0";
	glConfig.max_texture_depth = 0;
	glConfig.uniformBufferOffsetAlignment = 0;
	glConfig.shaderStorageBufferOffsetAlignment = 0;
	if (GL_VersionAtLeast(2, 0)) {
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &glConfig.max_3d_texture_size);
		glConfig.glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);

		if (GL_VersionAtLeast(3, 0)) {
			glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &glConfig.max_texture_depth);

			if (GL_VersionAtLeast(3, 1)) {
				glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &glConfig.uniformBufferOffsetAlignment);

				if (GL_VersionAtLeast(4, 3)) {
					glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &glConfig.shaderStorageBufferOffsetAlignment);
				}
			}
		}
	}
}

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
	{
		qbool supported = gl_ext_arb_texture_non_power_of_two.integer && SDL_GL_ExtensionSupported("GL_ARB_texture_non_power_of_two");

		R_SetNonPowerOfTwoSupport(supported);

		Com_Printf_State(PRINT_OK, "GL_ARB_texture_non_power_of_two extension %s\n", supported ? "found" : "not found");
	}
}

static void GL_CheckMultiTextureExtensions(void)
{
	if (!COM_CheckParm(cmdline_param_client_nomultitexturing) && SDL_GL_ExtensionSupported("GL_ARB_multitexture")) {
		qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2f");
		if (!qglMultiTexCoord2f) {
			qglMultiTexCoord2f = SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
		}
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
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

	if (COM_CheckParm(cmdline_param_client_maximum2textureunits) || gl_maxtmu2.value) {
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
	// NOTE: we always register cvar even if ext is not supported.
	// cvar added just to be able force OFF an extension.
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_ext_arb_texture_non_power_of_two);
	Cvar_Register(&gl_maxtmu2);
	Cvar_ResetCurrentGroup();

	if (!GL_InitialiseRenderer()) {
#if defined(EZ_MULTIPLE_RENDERERS) && defined(RENDERER_OPTION_CLASSIC_OPENGL)
		Con_Printf("Failed to initialised desired renderer, falling back to classic OpenGL\n");
		Cvar_LatchedSetValue(&vid_renderer, 0);

		if (!GL_InitialiseRenderer()) {
			Sys_Error("Failed to initialise graphics renderer");
		}
#else
		Sys_Error("Failed to initialise graphics renderer");
#endif
	}

	GL_PopulateConfig();

	GL_CheckExtensions();

#if !defined( _WIN32 ) && !defined( __linux__ ) /* we print this in different place on WIN and Linux */
	{
		/* FIXME/TODO: FreeBSD too? */
		Com_Printf_State(PRINT_INFO, "GL_VENDOR: %s\n", (const char*)glGetString(GL_VENDOR));
		Com_Printf_State(PRINT_INFO, "GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));
		Com_Printf_State(PRINT_INFO, "GL_VERSION: %s\n", (const char*)glGetString(GL_VERSION));
		if (COM_CheckParm(cmdline_param_client_printopenglextensions)) {
			const char* gl_extensions = "";
			if (R_UseModernOpenGL()) {
				gl_extensions = "(using modern OpenGL)\n";
			}
			else {
				gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
			}
			Com_Printf_State(PRINT_INFO, "GL_EXTENSIONS: %s\n", gl_extensions);
		}
	}
#endif
}
