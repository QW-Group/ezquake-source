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

#define POST_PROCESS_PALETTE    1
#define POST_PROCESS_3DONLY     2

static qbool GLC_CompilePostProcessProgram(void)
{
	extern cvar_t vid_framebuffer_palette, vid_framebuffer;
	int post_process_flags =
		(vid_framebuffer_palette.integer ? POST_PROCESS_PALETTE : 0) |
		(vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY ? POST_PROCESS_3DONLY : 0);

	if (R_ProgramRecompileNeeded(r_program_post_process_glc, post_process_flags)) {
		static char included_definitions[512];

		memset(included_definitions, 0, sizeof(included_definitions));
		if (post_process_flags & POST_PROCESS_PALETTE) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_PALETTE\n", sizeof(included_definitions));
		}
		if (post_process_flags & POST_PROCESS_3DONLY) {
			strlcat(included_definitions, "#define EZ_USE_OVERLAY\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(r_program_post_process_glc, included_definitions);

		R_ProgramSetCustomOptions(r_program_post_process_glc, post_process_flags);
	}

	return R_ProgramReady(r_program_post_process_glc);
}

void GLC_RenderFramebuffers(framebuffer_ref fb_3d, framebuffer_ref fb_2d)
{
	qbool flip2d = GL_FramebufferReferenceIsValid(fb_2d);
	qbool flip3d = GL_FramebufferReferenceIsValid(fb_3d);

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

		if (flip2d && flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(fb_3d, 0));
			renderer.TextureUnitBind(1, GL_FramebufferTextureReference(fb_2d, 0));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_base, 0);
			R_ProgramUniform1i(r_program_uniform_post_process_glc_overlay, 1);
		}
		else if (flip3d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(fb_3d, 0));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_base, 0);
		}
		else if (flip2d) {
			renderer.TextureUnitBind(0, GL_FramebufferTextureReference(fb_2d, 0));

			R_ProgramUniform1i(r_program_uniform_post_process_glc_overlay, 0);
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
