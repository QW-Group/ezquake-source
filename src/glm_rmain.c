/*
Copyright (C) 2018 ezQuake team

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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glm_local.h"
#include "glm_brushmodel.h"
#include "gl_framebuffer.h"
#include "tr_types.h"
#include "glm_vao.h"
#include "r_buffers.h"
#include "glm_local.h"
#include "r_program.h"
#include "r_renderer.h"
#include "r_aliasmodel.h"
#include "r_renderer.h"
#include "r_sprite3d.h"
#include "r_state.h"
#include "r_matrix.h"

texture_ref GL_FramebufferTextureReference(framebuffer_id id, fbtex_id tex_id);
qbool GLM_CompilePostProcessVAO(void);

// If this returns false then the framebuffer will be blitted instead
qbool GLM_CompileWorldGeometryProgram(void)
{
	int post_process_flags = 0;

	if (R_ProgramRecompileNeeded(r_program_fx_world_geometry, post_process_flags)) {
		// Initialise program for drawing image
		R_ProgramCompile(r_program_fx_world_geometry);

		R_ProgramSetCustomOptions(r_program_fx_world_geometry, post_process_flags);
	}

	return R_ProgramReady(r_program_fx_world_geometry) && GLM_CompilePostProcessVAO();
}

static void GLM_DrawWorldOutlines(void)
{
	texture_ref normals = GL_FramebufferTextureReference(framebuffer_std, fbtex_worldnormals);

	if (R_TextureReferenceIsValid(normals) && GLM_CompileWorldGeometryProgram()) {
		int viewport[4];
		int fullscreen_viewport[4];
		extern cvar_t gl_outline_color_world, gl_outline_world_depth_threshold; //, gl_outline_scale_world;

		R_GetViewport(viewport);

		// If we are only rendering to a section of the screen then that is the only part of the texture that will be filled in
		if (CL_MultiviewEnabled()) {
			R_GetFullScreenViewport(fullscreen_viewport);
			R_Viewport(fullscreen_viewport[0], fullscreen_viewport[1], fullscreen_viewport[2], fullscreen_viewport[3]);
			R_EnableScissorTest(viewport[0], viewport[1], viewport[2], viewport[3]);
		} else {
			// ignore viewsize and allat crap and set the viewport size to the whole window.
			// previously the viewport was already resized, and then resized again later, making the outlines not align.
			R_Viewport(0, 0, VID_ScaledWidth3D(), VID_ScaledHeight3D());
		}

		renderer.TextureUnitBind(0, normals);

		R_ProgramUniform1f(r_program_uniform_outline_depth_threshold, bound(1, gl_outline_world_depth_threshold.value, 16));
		// R_ProgramUniform1f(r_program_uniform_outline_scale, gl_outline_scale_world.value);
		R_ProgramUniform3f(r_program_uniform_outline_color,
                           (float)gl_outline_color_world.color[0] / 255.0f,
                           (float)gl_outline_color_world.color[1] / 255.0f,
                           (float)gl_outline_color_world.color[2] / 255.0f);

		R_ProgramUse(r_program_fx_world_geometry);
		R_ApplyRenderingState(r_state_fx_world_geometry);

		GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		// Restore viewport
		R_Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		if (CL_MultiviewEnabled()) {
			R_DisableScissorTest();
		}
	}
}

void GLM_RenderView(void)
{
	GLM_UploadFrameConstants();
	R_UploadChangedLightmaps();
	GLM_PrepareWorldModelBatch();
	GLM_PrepareAliasModelBatches();
	renderer.Prepare3DSprites();

	if (GL_FramebufferStartWorldNormals(framebuffer_std)) {
		GLM_DrawWorldModelBatch(opaque_world);
		GL_FramebufferEndWorldNormals(framebuffer_std);
		GLM_DrawWorldOutlines();
	}
	else {
		GLM_DrawWorldModelBatch(opaque_world);
	}

	R_TraceEnterNamedRegion("GLM_DrawEntities");
	GLM_DrawAliasModelBatches();
	R_TraceLeaveNamedRegion();

	renderer.Draw3DSprites();

	GLM_DrawWorldModelBatch(alpha_surfaces);

	GLM_DrawAliasModelPostSceneBatches();
}

void GLM_PrepareModelRendering(qbool vid_restart)
{
	GLM_BuildCommonTextureArrays(vid_restart);

	if (cls.state != ca_disconnected) {
		R_CreateInstanceVBO();
		R_CreateAliasModelVBO();
		R_BrushModelCreateVBO();

		GLM_CreateBrushModelVAO();
	}
	renderer.ProgramsInitialise();
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL

