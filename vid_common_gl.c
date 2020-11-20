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

byte color_white[4] = {255, 255, 255, 255};
byte color_black[4] = {0, 0, 0, 255};

void GL_CheckMultiTextureExtensions(void);
static void OnChange_gl_ext_texture_compression(cvar_t *, char *, qbool *);

static cvar_t gl_ext_texture_compression = {"gl_ext_texture_compression", "0", CVAR_SILENT, OnChange_gl_ext_texture_compression};

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

	shaders_supported = GL_Supported(R_SUPPORT_MODERN_OPENGL_REQUIREMENTS);
	if ((glConfig.preferred_format == 0 && GL_VersionAtLeast(1, 2)) || glConfig.preferred_format == GL_BGRA) {
		glConfig.supported_features |= R_SUPPORT_BGRA_LIGHTMAPS;
	}
	if ((glConfig.preferred_type == 0 && GL_VersionAtLeast(1, 2)) || glConfig.preferred_type == GL_UNSIGNED_INT_8_8_8_8_REV) {
		glConfig.supported_features |= R_SUPPORT_INT8888R_LIGHTMAPS;
	}

	R_TraceAPI("Supported features:");
	if (glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS) {
		R_TraceAPI("... rendering to framebuffers");
	}
	if (glConfig.supported_features & R_SUPPORT_RENDERING_SHADERS) {
		R_TraceAPI("... rendering using shaders");
	}
	if (glConfig.supported_features & R_SUPPORT_COMPUTE_SHADERS) {
		R_TraceAPI("... non-rendering shaders");
	}
	if (glConfig.supported_features & R_SUPPORT_PRIMITIVERESTART) {
		R_TraceAPI("... primitive restart indexes");
	}
	if (glConfig.supported_features & R_SUPPORT_MULTITEXTURING) {
		R_TraceAPI("... multi-texturing (some people still disable this...)");
	}
	if (glConfig.supported_features & R_SUPPORT_IMAGE_PROCESSING) {
		R_TraceAPI("... reading/writing to images");
	}
	if (glConfig.supported_features & R_SUPPORT_TEXTURE_SAMPLERS) {
		R_TraceAPI("... texture samplers");
	}
	if (glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS) {
		R_TraceAPI("... 3D images (texture arrays)");
	}
	if (glConfig.supported_features & R_SUPPORT_INDIRECT_RENDERING) {
		R_TraceAPI("... indirect rendering (api parameters stored in buffer)");
	}
	if (glConfig.supported_features & R_SUPPORT_INSTANCED_RENDERING) {
		R_TraceAPI("... instanced rendering");
	}
	if (glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS_BLIT) {
		R_TraceAPI("... blit from one framebuffer to another");
	}
	if (glConfig.supported_features & R_SUPPORT_BGRA_LIGHTMAPS) {
		R_TraceAPI("... BGRA lightmaps (if optimal format)");
	}
	if (glConfig.supported_features & R_SUPPORT_INT8888R_LIGHTMAPS) {
		R_TraceAPI("... Lightmaps uploaded as UINT8888R rather than UNSIGNED_BYTE");
	}
	if (glConfig.supported_features & R_SUPPORT_CUBE_MAPS) {
		R_TraceAPI("... cube maps");
	}
	if (glConfig.supported_features & R_SUPPORT_SEAMLESS_CUBEMAPS) {
		R_TraceAPI("... filtering works across faces of the cubemap");
	}
	if (glConfig.supported_features & R_SUPPORT_DEPTH32F) {
		R_TraceAPI("... floating point 32-bit depth buffers");
	}
	if (glConfig.supported_features & R_SUPPORT_FRAMEBUFFERS_SRGB) {
		R_TraceAPI("... framebuffers support sRGB");
	}
	if (glConfig.supported_features & R_SUPPORT_IMMEDIATEMODE) {
		R_TraceAPI("... immediate-mode rendering (doesn't require programs)");
	}
	if (glConfig.supported_features & R_SUPPORT_FOG) {
		R_TraceAPI("... fog");
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

	R_TraceAPI("---Config---");
	if (glConfig.renderer_string) {
		R_TraceAPI("Renderer: %s", glConfig.renderer_string);
	}
	if (glConfig.vendor_string) {
		R_TraceAPI("Vendor: %s", glConfig.vendor_string);
	}
	if (glConfig.version_string) {
		R_TraceAPI("Version: %s", glConfig.version_string);
	}
	if (glConfig.glsl_version) {
		R_TraceAPI("GLSL: %s", glConfig.glsl_version);
	}

	R_TraceAPI("OpenGL version: %d.%d", glConfig.majorVersion, glConfig.minorVersion);
	R_TraceAPI("Color/depth/stencil: %d/%d/%d", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
	R_TraceAPI("Hardware type: %d", glConfig.hardwareType);
	R_TraceAPI("Max sizes: %d %d %d", glConfig.gl_max_size_default, glConfig.max_3d_texture_size, glConfig.max_texture_depth);
	R_TraceAPI("Texture units: %d", glConfig.texture_units);
	R_TraceAPI("Alignments: ubo(%d) ssb(%d)", glConfig.uniformBufferOffsetAlignment, glConfig.shaderStorageBufferOffsetAlignment);
}

// meag: EXT => ARB didn't change value of constants, so still using _EXT versions
static void GL_CheckAnisotropyExtensions(void)
{
	anisotropy_ext = 0;
	R_TraceAPI("Checking for anisotropic filtering...");
	if (GL_VersionAtLeast(4, 6) || SDL_GL_ExtensionSupported("GL_ARB_texture_filter_anisotropic") || SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
		int gl_anisotropy_factor_max;

		anisotropy_ext = 1;

		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_anisotropy_factor_max);

		Com_Printf_State(PRINT_OK, "Anisotropic Filtering Extension Found (%d max)\n", gl_anisotropy_factor_max);
		R_TraceAPI("... filtering extension found (max level %d)", gl_anisotropy_factor_max);
	}
}

static void GL_CheckTextureCompressionExtensions(void)
{
	R_TraceAPI("Checking for texture compression...");
	if (GL_VersionAtLeast(1, 3) || SDL_GL_ExtensionSupported("GL_ARB_texture_compression")) {
		Com_Printf_State(PRINT_OK, "Texture compression extensions found\n");
		R_TraceAPI("... texture compression extensions found");
		Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
		Cvar_Register(&gl_ext_texture_compression);
		Cvar_ResetCurrentGroup();
	}
}

static void GL_CheckNPOTTextureExtensions(void)
{
	qbool supported = false;

	if (!COM_CheckParm(cmdline_param_client_no_npot_textures)) {
		supported = (GL_VersionAtLeast(2, 0) || SDL_GL_ExtensionSupported("GL_ARB_texture_non_power_of_two"));
	}

	R_SetNonPowerOfTwoSupport(supported);
	Com_Printf_State(PRINT_OK, "GL_ARB_texture_non_power_of_two extension %s\n", supported ? "found" : "not found");
	R_TraceAPI("Non-power-of-two textures: %s", supported ? "supported" : "not supported (!)");
}

static void GL_CheckExtensions(void)
{
	GL_CheckMultiTextureExtensions();
	GL_CheckAnisotropyExtensions();
	GL_CheckTextureCompressionExtensions();
	GL_CheckNPOTTextureExtensions();
}

void GL_Init(void)
{
	GL_PopulateConfig();

	if (!GL_InitialiseRenderer()) {
		Sys_Error("Failed to initialise graphics renderer");
	}

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
