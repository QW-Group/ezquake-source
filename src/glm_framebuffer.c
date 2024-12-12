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

// GLM_FrameBuffer.c
// Framebuffer support in OpenGL core

// For the moment, also contains general framebuffer code as immediate-mode isn't supported

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"
#include "glm_vao.h"
#include "r_buffers.h"
#include "glm_local.h"
#include "r_state.h"
#include "r_program.h"
#include "r_renderer.h"
#include "r_texture.h"

#define POST_PROCESS_PALETTE         1
#define POST_PROCESS_3DONLY          2
#define POST_PROCESS_TONEMAP         4
#define POST_PROCESS_FXAA            8

extern cvar_t vid_software_palette, vid_framebuffer, vid_framebuffer_hdr, vid_framebuffer_hdr_tonemap, vid_framebuffer_multisample;
static texture_ref non_framebuffer_screen_texture;

qbool GLM_CompilePostProcessVAO(void)
{
	if (!R_BufferReferenceIsValid(r_buffer_postprocess_vertex_data)) {
		float verts[4][5] = { { 0 } };

		VectorSet(verts[0], -1, -1, 0);
		verts[0][3] = 0;
		verts[0][4] = 0;

		VectorSet(verts[1], -1, 1, 0);
		verts[1][3] = 0;
		verts[1][4] = 1;

		VectorSet(verts[2], 1, -1, 0);
		verts[2][3] = 1;
		verts[2][4] = 0;

		VectorSet(verts[3], 1, 1, 0);
		verts[3][3] = 1;
		verts[3][4] = 1;

		buffers.Create(r_buffer_postprocess_vertex_data, buffertype_vertex, "post-process-screen", sizeof(verts), verts, bufferusage_reuse_many_frames);
	}

	if (R_BufferReferenceIsValid(r_buffer_postprocess_vertex_data) && !R_VertexArrayCreated(vao_postprocess)) {
		R_GenVertexArray(vao_postprocess);

		GLM_ConfigureVertexAttribPointer(vao_postprocess, r_buffer_postprocess_vertex_data, 0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0, 0);
		GLM_ConfigureVertexAttribPointer(vao_postprocess, r_buffer_postprocess_vertex_data, 1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3), 0);

		R_BindVertexArray(vao_none);
	}

	return R_VertexArrayCreated(vao_postprocess);
}

// If this returns false then the framebuffer will be blitted instead
qbool GLM_CompilePostProcessProgram(void)
{
	int fxaa_preset = GL_FramebufferFxaaPreset();
	int post_process_flags =
		(vid_software_palette.integer ? POST_PROCESS_PALETTE : 0) |
		(vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY ? POST_PROCESS_3DONLY : 0) |
		(vid_framebuffer_hdr.integer && vid_framebuffer_hdr_tonemap.integer ? POST_PROCESS_TONEMAP : 0) |
		(fxaa_preset > 0 ? POST_PROCESS_FXAA : 0) | (fxaa_preset << 4); // mix in preset to detect change

	if (R_ProgramRecompileNeeded(r_program_post_process, post_process_flags)) {
		static char included_definitions[131072];

		memset(included_definitions, 0, sizeof(included_definitions));
		if (post_process_flags & POST_PROCESS_PALETTE) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_PALETTE\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_3DONLY) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_OVERLAY\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_TONEMAP) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_TONEMAP\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_FXAA) {
			extern const unsigned char fxaa_h_glsl[];
			char buffer[33];
			const char *settings =
					"#define EZ_POSTPROCESS_FXAA\n" \
					"#define FXAA_PC 1\n" \
					"#define FXAA_GLSL_130 1\n" \
					"#define FXAA_GREEN_AS_LUMA 1\n";
			snprintf(buffer, sizeof(buffer), "#define FXAA_QUALITY__PRESET %d\n", fxaa_preset);
			strlcat(included_definitions, settings, sizeof(included_definitions));
			strlcat(included_definitions, buffer, sizeof(included_definitions));
			strlcat(included_definitions, (const char *)fxaa_h_glsl, sizeof(included_definitions));
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(r_program_post_process, included_definitions);

		R_ProgramSetCustomOptions(r_program_post_process, post_process_flags);
	}

	return R_ProgramReady(r_program_post_process) && GLM_CompilePostProcessVAO();
}

qbool GLM_CompileSimpleProgram(void)
{
	if (R_ProgramRecompileNeeded(r_program_simple, 0)) {
		R_ProgramCompile(r_program_simple);
	}

	return R_ProgramReady(r_program_simple) && GLM_CompilePostProcessVAO();
}

void GLM_RenderFramebuffers(void)
{
	qbool flip2d = GL_FramebufferEnabled2D();
	qbool flip3d = GL_FramebufferEnabled3D();

	if (GLM_CompilePostProcessProgram()) {
		R_ProgramUse(r_program_post_process);
		R_ApplyRenderingState(r_state_default_2d);

		if (flip2d && flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_std, fbtex_standard));
			renderer.TextureUnitBind(1, GL_FramebufferTextureReference(framebuffer_hud, fbtex_standard));
		}
		else if (flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_std, fbtex_standard));
		}
		else if (flip2d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_hud, fbtex_standard));
		}
		else {
			// Create screen texture if required
			if (!R_TextureReferenceIsValid(non_framebuffer_screen_texture) || R_TextureWidth(non_framebuffer_screen_texture) != glwidth || R_TextureHeight(non_framebuffer_screen_texture) != glheight) {
				if (R_TextureReferenceIsValid(non_framebuffer_screen_texture)) {
					renderer.TextureDelete(non_framebuffer_screen_texture);
				}
				non_framebuffer_screen_texture = R_LoadTexture("glm:postprocess", glwidth, glheight, NULL, TEX_NOCOMPRESS | TEX_NOSCALE | TEX_NO_TEXTUREMODE, 4);
				if (R_TextureReferenceIsValid(non_framebuffer_screen_texture))
					renderer.TextureSetFiltering(non_framebuffer_screen_texture, texture_minification_nearest, texture_magnification_nearest);
			}

			if (!R_TextureReferenceIsValid(non_framebuffer_screen_texture)) {
				return;
			}

			// Copy from screen to texture
			renderer.TextureUnitBind(0, non_framebuffer_screen_texture);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, glwidth, glheight);
		}
		GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}

void GLM_BrightenScreen(void)
{
	float f;

	if (vid_hwgamma_enabled) {
		return;
	}
	if (v_contrast.value <= 1.0) {
		return;
	}

	f = min(v_contrast.value, 3);
	if (R_OldGammaBehaviour()) {
		extern float vid_gamma;

		f = pow(f, vid_gamma);
	}

	if (GLM_CompileSimpleProgram()) {
		R_ProgramUse(r_program_simple);
		R_ApplyRenderingState(r_state_brighten_screen);

		while (f > 1) {
			if (f >= 2) {
				float white[4] = { 1, 1, 1, 1 };

				R_ProgramUniform4fv(r_program_uniform_simple_color, white);
			}
			else {
				float shade[4] = { f - 1, f - 1, f - 1, f - 1 };

				R_ProgramUniform4fv(r_program_uniform_simple_color, shade);
			}

			GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			f *= 0.5;
		}
	}
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
