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

// GLM_FrameBuffer.c
// Framebuffer support in OpenGL core

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

// For the moment, also contains general framebuffer code as immediate-mode isn't supported

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glc_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"
#include "r_buffers.h"
#include "r_state.h"
#include "r_program.h"
#include "r_renderer.h"
#include "r_matrix.h"
#include "r_texture.h"

#define POST_PROCESS_PALETTE    1
#define POST_PROCESS_3DONLY     2
#define POST_PROCESS_FXAA       4

static texture_ref non_framebuffer_screen_texture;

qbool GLC_CompilePostProcessProgram(void)
{
	extern cvar_t vid_software_palette, vid_framebuffer;
	int fxaa_preset = GL_FramebufferFxaaPreset();
	int post_process_flags =
		(vid_software_palette.integer ? POST_PROCESS_PALETTE : 0) |
		(vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY ? POST_PROCESS_3DONLY : 0) |
		(fxaa_preset > 0 ? POST_PROCESS_FXAA : 0) | (fxaa_preset << 4); // mix in preset to detect change

	if (R_ProgramRecompileNeeded(r_program_post_process_glc, post_process_flags)) {
		static char included_definitions[131072];

		memset(included_definitions, 0, sizeof(included_definitions));
		if (post_process_flags & POST_PROCESS_PALETTE) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_PALETTE\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_3DONLY) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_OVERLAY\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_FXAA) {
			qbool supported = true;
			if (!SDL_GL_ExtensionSupported("GL_EXT_gpu_shader4"))
			{
				Com_Printf("WARNING: Missing GL_EXT_gpu_shader4, FXAA not available.");
				supported = false;
			}
#ifndef __APPLE__
			if (!SDL_GL_ExtensionSupported("GL_ARB_gpu_shader5"))
			{
				Com_Printf("WARNING: Missing GL_ARB_gpu_shader5, FXAA not available.");
				supported = false;
			}
#endif
			if (supported)
			{
				extern const unsigned char fxaa_h_glsl[];
				char buffer[33];
				const char *settings =
						"#extension GL_EXT_gpu_shader4 : enable\n"
#ifndef __APPLE__
						"#extension GL_ARB_gpu_shader5 : enable\n"
#endif
						"#define EZ_POSTPROCESS_FXAA\n"
						"#define FXAA_PC 1\n"
						"#define FXAA_GLSL_120 1\n"
						"#define FXAA_GREEN_AS_LUMA 1\n";
				snprintf(buffer, sizeof(buffer), "#define FXAA_QUALITY__PRESET %d\n", fxaa_preset);
				strlcat(included_definitions, settings, sizeof(included_definitions));
				strlcat(included_definitions, buffer, sizeof(included_definitions));
				strlcat(included_definitions, (const char *)fxaa_h_glsl, sizeof(included_definitions));
			}
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(r_program_post_process_glc, included_definitions);

		R_ProgramSetCustomOptions(r_program_post_process_glc, post_process_flags);
	}

	return R_ProgramReady(r_program_post_process_glc);
}

void GLC_RenderFramebuffers(void)
{
	qbool flip2d = GL_FramebufferEnabled2D();
	qbool flip3d = GL_FramebufferEnabled3D();

	if (GLC_CompilePostProcessProgram()) {
		extern cvar_t gl_hwblend;

		float blend_alpha = (!vid_hwgamma_enabled || !gl_hwblend.value || cl.teamfortress) ? 0 : v_blend[3];
		float blend_values[4] = {
			v_blend[0] * blend_alpha,
			v_blend[1] * blend_alpha,
			v_blend[2] * blend_alpha,
			1 - blend_alpha
		};

		R_ProgramUse(r_program_post_process_glc);
		R_ProgramUniform1f(r_program_uniform_post_process_glc_gamma, v_gamma.value);
		R_ProgramUniform4fv(r_program_uniform_post_process_glc_v_blend, blend_values);
		R_ProgramUniform1f(r_program_uniform_post_process_glc_contrast, bound(1, v_contrast.value, 3));
		R_ProgramUniform1f(r_program_uniform_post_process_glc_r_inv_width, 1.0f / (float)VID_ScaledWidth3D());
		R_ProgramUniform1f(r_program_uniform_post_process_glc_r_inv_height, 1.0f / (float)VID_ScaledHeight3D());
		R_ApplyRenderingState(r_state_default_2d);

		if (flip2d && flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_std, fbtex_standard));
			renderer.TextureUnitBind(1, GL_FramebufferTextureReference(framebuffer_hud, fbtex_standard));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_base, 0);
			R_ProgramUniform1i(r_program_uniform_post_process_glc_overlay, 1);
		}
		else if (flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_std, fbtex_standard));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_base, 0);
		}
		else if (flip2d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(framebuffer_hud, fbtex_standard));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_overlay, 0);
		}
		else {
			// Create screen texture if required
			if (!R_TextureReferenceIsValid(non_framebuffer_screen_texture) || R_TextureWidth(non_framebuffer_screen_texture) != glwidth || R_TextureHeight(non_framebuffer_screen_texture) != glheight) {
				if (R_TextureReferenceIsValid(non_framebuffer_screen_texture)) {
					renderer.TextureDelete(non_framebuffer_screen_texture);
				}
				non_framebuffer_screen_texture = R_LoadTexture("glc:postprocess", glwidth, glheight, NULL, TEX_NOCOMPRESS | TEX_NOSCALE | TEX_NO_TEXTUREMODE, 4);
				if (R_TextureReferenceIsValid(non_framebuffer_screen_texture))
					renderer.TextureSetFiltering(non_framebuffer_screen_texture, texture_minification_nearest, texture_magnification_nearest);
			}

			if (!R_TextureReferenceIsValid(non_framebuffer_screen_texture)) {
				return;
			}

			// Copy from screen to texture
			renderer.TextureUnitBind(0, non_framebuffer_screen_texture);
			GL_BuiltinProcedure(glCopyTexSubImage2D, "mode=GL_TEXTURE_2D(%u), level=%d, xoffset=%d, yoffset=%d, x=%d, y=%d, width=%d, height=%d", GL_TEXTURE_2D, 0, 0, 0, 0, 0, glwidth, glheight);
		}

		R_IdentityModelView();
		R_IdentityProjectionView();

		GLC_Begin(GL_TRIANGLE_STRIP);
		// Top left corner.
		glTexCoord2f(0, 0);
		GLC_Vertex2f(-1, -1);

		// Upper right corner.
		glTexCoord2f(0, 1);
		GLC_Vertex2f(-1, 1);

		// Bottom right corner.
		glTexCoord2f(1, 0);
		GLC_Vertex2f(1, -1);

		// Bottom left corner.
		glTexCoord2f(1, 1);
		GLC_Vertex2f(1, 1);
		GLC_End();

		R_ProgramUse(r_program_none);
	}
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
