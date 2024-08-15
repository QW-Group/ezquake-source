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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "r_renderer.h"
#include "r_buffers.h"
#include "glc_vao.h"
#include "r_brushmodel.h"
#include "r_aliasmodel.h"
#include "tr_types.h"

cvar_t gl_program_sky          = { "gl_program_sky",           "1" };
cvar_t gl_program_turbsurfaces = { "gl_program_turbsurfaces",  "1" };
cvar_t gl_program_aliasmodels  = { "gl_program_aliasmodels",   "1", CVAR_LATCH_GFX };
cvar_t gl_program_world        = { "gl_program_world",         "1" };
cvar_t gl_program_sprites      = { "gl_program_sprites",       "1" };
cvar_t gl_program_hud          = { "gl_program_hud",           "1" };

static qbool glc_program_cvars_initialized = false;
static cvar_t* gl_program_cvars[] = {
	&gl_program_sky,
	&gl_program_turbsurfaces,
	&gl_program_aliasmodels,
	&gl_program_world,
	&gl_program_sprites,
	&gl_program_hud,
};

void GL_Init(void);
qbool GLC_InitialiseVAOHandling(void);
void GL_InitialiseBufferHandling(api_buffers_t* buffers);
void GL_InitialiseState(void);

static void GLC_NoOperation(void)
{
}

static void GLC_BindVertexArrayElementBuffer(r_vao_id vao, r_buffer_id ref)
{
}

#ifndef WITH_RENDERING_TRACE
static void GLC_TextureLabelSetNull(texture_ref texture, const char* name)
{
}
#endif

qbool GLC_False(void)
{
	return false;
}

static void GLC_ScreenDrawStart(void)
{
	GL_FramebufferScreenDrawStart();
}

static void GLC_PostProcessScreen(void)
{
	GL_FramebufferPostProcessScreen();
}

static void GLC_Begin2DRendering(void)
{
	GL_Framebuffer2DSwitch();
}

#define GLC_PrintGfxInfo                   GL_PrintGfxInfo
#define GLC_Viewport                       GL_Viewport
#define GLC_InvalidateViewport             GL_InvalidateViewport
#define GLC_RenderDynamicLightmaps         R_RenderDynamicLightmaps
#define GLC_LightmapFrameInit              GLC_NoOperation
#define GLC_LightmapShutdown               GLC_NoOperation
#define GLC_ClearRenderingSurface          GL_Clear
#define GLC_EnsureFinished                 GL_EnsureFinished
#define GLC_RenderView                     GLC_NoOperation
#define GLC_Screenshot                     GL_Screenshot
#define GLC_ScreenshotWidth                GL_ScreenshotWidth
#define GLC_ScreenshotHeight               GL_ScreenshotHeight
#define GLC_InitialiseVAOState             GL_InitialiseVAOState
#define GLC_DescriptiveString              GL_DescriptiveString
#define GLC_Draw3DSprites                  GLC_NoOperation
#define GLC_Prepare3DSprites               GLC_NoOperation
#define GLC_IsFramebufferEnabled3D         GL_FramebufferEnabled3D
#define GLC_TextureDelete                  GL_TextureDelete
#define GLC_TextureMipmapGenerate          GL_TextureMipmapGenerate
#define GLC_TextureWrapModeClamp           GL_TextureWrapModeClamp
#ifdef WITH_RENDERING_TRACE
#define GLC_TextureLabelSet                GL_TextureLabelSet
#else
#define GLC_TextureLabelSet                GLC_TextureLabelSetNull
#endif
#define GLC_TextureIsUnitBound             GL_IsTextureBound
#define GLC_TextureUnitBind                GL_EnsureTextureUnitBoundAndSelect
#define GLC_TextureGet                     GL_TextureGet
#define GLC_TextureCompressionSet          GL_TextureCompressionSet
#define GLC_TextureCreate2D                GL_TextureCreate2D
#define GLC_TextureUnitMultiBind           GL_TextureUnitMultiBind
#define GLC_TexturesCreate                 GL_TexturesCreate
#define GLC_TextureReplaceSubImageRGBA     GL_TextureReplaceSubImageRGBA
#define GLC_TextureSetAnisotropy           GL_TextureSetAnisotropy
#define GLC_TextureSetFiltering            GL_TextureSetFiltering
#define GLC_TextureLoadCubemapFace         GL_TextureLoadCubemapFace
#define GLC_CvarForceRecompile             GL_CvarForceRecompile
#define GLC_ProgramsInitialise             GL_ProgramsInitialise
#define GLC_ProgramsShutdown               GL_ProgramsShutdown
#define GLC_FramebufferCreate              GL_FramebufferCreate
#define GLC_PrepareAliasModel              GL_PrepareAliasModel

#define RENDERER_METHOD(returntype, name, ...) \
{ \
	extern returntype GLC_ ## name(__VA_ARGS__); \
	renderer.name = GLC_ ## name; \
}

void GLC_Initialise(void)
{
	int i;
	extern cvar_t vid_gl_core_profile;
#include "r_renderer_structure.h"

	if (!glc_program_cvars_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
		for (i = 0; i < sizeof(gl_program_cvars) / sizeof(gl_program_cvars[0]); ++i) {
			if (!(gl_program_cvars[i]->flags & CVAR_LATCH)) {
				Cvar_Register(gl_program_cvars[i]);
			}
		}

		Cvar_ResetCurrentGroup();
		glc_program_cvars_initialized = true;
	}

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	for (i = 0; i < sizeof(gl_program_cvars) / sizeof(gl_program_cvars[0]); ++i) {
		if (gl_program_cvars[i]->flags & CVAR_LATCH) {
			Cvar_Register(gl_program_cvars[i]);
		}
	}
	Cvar_ResetCurrentGroup();

	GL_Init();
	renderer.vaos_supported = GLC_InitialiseVAOHandling();
	GL_ProcessErrors("post-GLC_InitialiseVAOHandling");
	GL_InitialiseBufferHandling(&buffers);
	GL_ProcessErrors("post-GL_InitialiseBufferHandling");
	GL_InitialiseState();
	GL_ProcessErrors("post-GL_InitialiseState");

	if (!GL_Supported(R_SUPPORT_RENDERING_SHADERS)) {
		// GLSL not supported for some reason
		for (i = 0; i < sizeof(gl_program_cvars) / sizeof(gl_program_cvars[0]); ++i) {
			int flags = Cvar_GetFlags(gl_program_cvars[i]);
			if (flags & CVAR_LATCH) {
				Cvar_LatchedSetValue(gl_program_cvars[i], 0);
			}
			else {
				Cvar_SetValue(gl_program_cvars[i], 0);
			}
			Cvar_SetFlags(gl_program_cvars[i], flags | CVAR_ROM);
		}
		Con_Printf("&cf00ERROR&r: GLSL programs not supported.\n");
		glConfig.supported_features |= R_SUPPORT_IMMEDIATEMODE;
	}
	else if (vid_gl_core_profile.integer && buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS)) {
		// Force GLSL
		for (i = 0; i < sizeof(gl_program_cvars) / sizeof(gl_program_cvars[0]); ++i) {
			int flags = Cvar_GetFlags(gl_program_cvars[i]);
			if (flags & CVAR_LATCH) {
				Cvar_LatchedSetValue(gl_program_cvars[i], 1);
			}
			else {
				Cvar_SetValue(gl_program_cvars[i], 1);
			}
			Cvar_SetFlags(gl_program_cvars[i], flags | CVAR_ROM);
		}
		Con_Printf("&c0f0INFO&r: immediate-mode rendering disabled.\n");
	}
	else {
		// make optional
		for (i = 0; i < sizeof(gl_program_cvars) / sizeof(gl_program_cvars[0]); ++i) {
			Cvar_SetFlags(gl_program_cvars[i], Cvar_GetFlags(gl_program_cvars[i]) & ~CVAR_ROM);
		}
		if (!renderer.vaos_supported) {
			// disable aliasmodel program rendering, it requires attributes
			Cvar_LatchedSetValue(&gl_program_aliasmodels, 0);
			Cvar_SetFlags(&gl_program_aliasmodels, Cvar_GetFlags(&gl_program_aliasmodels) | CVAR_ROM);
		}
		glConfig.supported_features |= R_SUPPORT_IMMEDIATEMODE;
	}
}

void GLC_PrepareModelRendering(qbool vid_restart)
{
	if (buffers.supported) {
		R_CreateInstanceVBO();

		R_CreateAliasModelVBO();
		R_BrushModelCreateVBO();
	}

	renderer.ProgramsInitialise();
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
