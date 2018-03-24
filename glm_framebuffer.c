
// GLM_FrameBuffer.c
// Framebuffer support in OpenGL core

// For the moment, also contains general framebuffer code as immediate-mode isn't supported

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"

static framebuffer_ref framebuffer;
static glm_program_t post_process_program;
static buffer_ref post_process_vbo;
static glm_vao_t post_process_vao;

static qbool GLM_CompilePostProcessProgram(void);

qbool GL_FramebufferEnabled(void)
{
	return GL_FramebufferReferenceIsValid(framebuffer);
}

void VID_FramebufferFlip(void)
{
	if (GL_FramebufferReferenceIsValid(framebuffer)) {
		// render to screen from now on
		GL_FramebufferStopUsing(framebuffer);

		if (GLM_CompilePostProcessProgram()) {
			GL_UseProgram(post_process_program.program);
			GL_BindVertexArray(&post_process_vao);

			GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_FramebufferTextureReference(framebuffer, 0));
			GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		//else {
			//GL_FrameBufferBlit(framebuffer, 0, 0, 0, GL_FramebufferWidth(framebuffer), GL_FramebufferHeight(framebuffer), )
		//}
	}
}

void GLM_FramebufferScreenDrawStart(void)
{
	extern cvar_t vid_framebuffer;

	int effective_width = vid_framebuffer.integer != 0 ? VID_ScaledWidth3D() : 0;
	int effective_height = vid_framebuffer.integer != 0 ? VID_ScaledHeight3D() : 0;

	if (effective_width && effective_height) {
		if (!GL_FramebufferReferenceIsValid(framebuffer)) {
			framebuffer = GL_FramebufferCreate(effective_width, effective_height, true);
		}
		else if (GL_FrameBufferWidth(framebuffer) != effective_width || GL_FrameBufferHeight(framebuffer) != effective_height) {
			GL_FramebufferDelete(&framebuffer);

			framebuffer = GL_FramebufferCreate(effective_width, effective_height, true);
		}

		if (GL_FramebufferReferenceIsValid(framebuffer)) {
			GL_Viewport(0, 0, effective_width, effective_height);
			GL_FramebufferStartUsing(framebuffer);
		}
	}
	else {
		if (GL_FramebufferReferenceIsValid(framebuffer)) {
			GL_FramebufferDelete(&framebuffer);
		}
	}
}

// If this returns false then the framebuffer will be blitted instead
static qbool GLM_CompilePostProcessProgram(void)
{
	extern cvar_t vid_framebuffer_palette;

	// This is all we have at the moment, can just blit if turned off
	//if (!vid_framebuffer_palette.integer) {
	//	return false;
	//}

	if (GLM_ProgramRecompileNeeded(&post_process_program, vid_framebuffer_palette.integer)) {
		static char included_definitions[512];
		GL_VFDeclare(post_process_screen);

		memset(included_definitions, 0, sizeof(included_definitions));
		if (vid_framebuffer_palette.integer) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_PALETTE\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("post-process-screen", GL_VFParams(post_process_screen), &post_process_program, included_definitions);

		post_process_program.custom_options = vid_framebuffer_palette.integer;
	}

	post_process_program.uniforms_found = true;

	if (!GL_BufferReferenceIsValid(post_process_vbo)) {
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

		post_process_vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "post-process-screen", sizeof(verts), verts, GL_STATIC_DRAW);
	}

	if (GL_BufferReferenceIsValid(post_process_vbo) && !post_process_vao.vao) {
		GL_GenVertexArray(&post_process_vao, "post-process-vao");

		GL_ConfigureVertexAttribPointer(&post_process_vao, post_process_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0, 0);
		GL_ConfigureVertexAttribPointer(&post_process_vao, post_process_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3), 0);
	}

	return post_process_program.program && post_process_vao.vao;
}

void GLM_FramebufferPostProcessScreen(void)
{
	extern cvar_t vid_framebuffer, vid_framebuffer_palette;

	if (vid_framebuffer.integer && GL_FramebufferReferenceIsValid(framebuffer)) {
		GL_Viewport(glx, gly, glConfig.vidWidth, glConfig.vidHeight);

		VID_FramebufferFlip();

		if (!vid_framebuffer_palette.integer) {
			// Hardware palette changes
			V_UpdatePalette();
		}
	}
	else {
		// Hardware palette changes
		V_UpdatePalette();
	}
}
