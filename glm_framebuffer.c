
// GLM_FrameBuffer.c
// Framebuffer support in OpenGL core

// For the moment, also contains general framebuffer code as immediate-mode isn't supported

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"

static qbool GLM_CompilePostProcessProgram(void);

static framebuffer_ref framebuffer3d;
static framebuffer_ref framebuffer2d;
static glm_program_t post_process_program;
static buffer_ref post_process_vbo;
static glm_vao_t post_process_vao;

extern cvar_t vid_framebuffer;
extern cvar_t vid_framebuffer_palette;

qbool GL_FramebufferEnabled3D(void)
{
	return vid_framebuffer.integer && GL_FramebufferReferenceIsValid(framebuffer3d);
}

qbool GL_FramebufferEnabled2D(void)
{
	return GL_FramebufferReferenceIsValid(framebuffer2d);
}

static void VID_FramebufferFlip(void)
{
	qbool flip3d = vid_framebuffer.integer && GL_FramebufferReferenceIsValid(framebuffer3d);
	qbool flip2d = vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY && GL_FramebufferReferenceIsValid(framebuffer2d);

	if (flip3d || flip2d) {
		// render to screen from now on
		GL_FramebufferStartUsingScreen();

		if (GLM_CompilePostProcessProgram()) {
			GL_UseProgram(post_process_program.program);
			GL_BindVertexArray(&post_process_vao);

			if (flip2d && flip3d) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_FramebufferTextureReference(framebuffer3d, 0));
				GL_EnsureTextureUnitBound(GL_TEXTURE1, GL_FramebufferTextureReference(framebuffer2d, 0));
			}
			else if (flip3d) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_FramebufferTextureReference(framebuffer3d, 0));
			}
			else if (flip2d) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_FramebufferTextureReference(framebuffer2d, 0));
			}
			GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
		else if (flip3d) {
			GL_FramebufferBlitSimple(framebuffer3d, null_framebuffer_ref);
		}
	}
}

static qbool VID_FramebufferInit(framebuffer_ref* framebuffer, int effective_width, int effective_height, qbool is3D)
{
	if (effective_width && effective_height) {
		if (!GL_FramebufferReferenceIsValid(*framebuffer)) {
			*framebuffer = GL_FramebufferCreate(effective_width, effective_height, is3D);
		}
		else if (GL_FrameBufferWidth(*framebuffer) != effective_width || GL_FrameBufferHeight(*framebuffer) != effective_height) {
			GL_FramebufferDelete(framebuffer);

			*framebuffer = GL_FramebufferCreate(effective_width, effective_height, is3D);
		}

		if (GL_FramebufferReferenceIsValid(*framebuffer)) {
			GL_FramebufferStartUsing(*framebuffer);
			return true;
		}
	}
	else {
		if (GL_FramebufferReferenceIsValid(*framebuffer)) {
			GL_FramebufferDelete(framebuffer);
		}
	}

	return false;
}

void GLM_FramebufferScreenDrawStart(void)
{
	if (vid_framebuffer.integer) {
		VID_FramebufferInit(&framebuffer3d, VID_ScaledWidth3D(), VID_ScaledHeight3D(), true);
	}
}

qbool GL_Framebuffer2DSwitch(void)
{
	if (vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY) {
		if (VID_FramebufferInit(&framebuffer2d, glConfig.vidWidth, glConfig.vidHeight, false)) {
			GL_ClearColor(0, 0, 0, 0);
			GL_Viewport(0, 0, glConfig.vidWidth, glConfig.vidHeight);
			glClear(GL_COLOR_BUFFER_BIT);
			return true;
		}
	}
	
	GL_Viewport(glx, gly, glwidth, glheight);
	return false;
}

// If this returns false then the framebuffer will be blitted instead
static qbool GLM_CompilePostProcessProgram(void)
{
	if (vid_framebuffer.integer == 1 && !vid_framebuffer_palette.integer) {
		// Screen-wide framebuffer, so we can just blit if turned off
		return false;
	}

	if (GLM_ProgramRecompileNeeded(&post_process_program, vid_framebuffer_palette.integer)) {
		static char included_definitions[512];
		GL_VFDeclare(post_process_screen);

		memset(included_definitions, 0, sizeof(included_definitions));
		if (vid_framebuffer_palette.integer) {
			strlcat(included_definitions, "#define EZ_POSTPROCESS_PALETTE\n", sizeof(included_definitions));
		}
		if (vid_framebuffer.integer == USE_FRAMEBUFFER_3DONLY) {
			strlcat(included_definitions, "#define EZ_USE_OVERLAY\n", sizeof(included_definitions));
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
	qbool framebuffer_active = (GL_FramebufferReferenceIsValid(framebuffer3d) || GL_FramebufferReferenceIsValid(framebuffer2d));

	if (framebuffer_active) {
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
