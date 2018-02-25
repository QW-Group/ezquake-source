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
void GLM_UploadFrameConstants(void);

#define PASS_COLOR_AS_4F(target, cvar) \
{ \
	target[0] = (cvar.color[0] * 1.0f / 255); \
	target[1] = (cvar.color[1] * 1.0f / 255); \
	target[2] = (cvar.color[2] * 1.0f / 255); \
	target[3] = 1.0f; \
}

// TODO: !?  Called as first step in 2D.  Include in frame-buffer at end of 3D scene rendering?
void GLM_PolyBlend(float v_blend[4])
{
	color_t v_blend_color = RGBA_TO_COLOR(
		bound(0, v_blend[0], 1) * 255,
		bound(0, v_blend[1], 1) * 255,
		bound(0, v_blend[2], 1) * 255,
		bound(0, v_blend[3], 1) * 255
	);

	Draw_AlphaRectangleRGB(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0.0f, true, v_blend_color);
}

void GLM_DrawVelocity3D(void)
{
	// MEAG: TODO
}

void GLM_RenderSceneBlurDo(float alpha)
{
	// MEAG: TODO
}

static buffer_ref ubo_frameConstants;
static uniform_block_frame_constants_t frameConstants;
static qbool frameConstantsUploaded = false;

void GLM_PreRenderView(void)
{
	extern cvar_t gl_alphafont;
	extern cvar_t r_telecolor, r_lavacolor, r_slimecolor, r_watercolor, r_fastturb, r_skycolor;
	extern cvar_t gl_textureless;
	int i, active_lights = 0;

	// General constants
	frameConstants.time = cl.time;
	frameConstants.gamma2d = v_gamma.value;
	frameConstants.gamma3d = v_gamma.value;
	frameConstants.r_alphafont = gl_alphafont.value;

	// Lights
	for (i = 0; i < MAX_DLIGHTS; ++i) {
		if (cl_dlight_active[i / 32] & (1 << (i % 32))) {
			dlight_t* light = &cl_dlights[i];
			extern cvar_t gl_colorlights;
			
			// TODO: if PlaneDist(forward, light) > radius, skip
			VectorCopy(light->origin, frameConstants.lightPosition[active_lights]);
			frameConstants.lightPosition[active_lights][3] = light->radius;
			if (gl_colorlights.value) {
				if (light->type == lt_custom) {
					frameConstants.lightColor[active_lights][0] = light->color[0] / 255.0;
					frameConstants.lightColor[active_lights][1] = light->color[1] / 255.0;
					frameConstants.lightColor[active_lights][2] = light->color[2] / 255.0;
				}
				else {
					extern float bubblecolor[NUM_DLIGHTTYPES][4];

					frameConstants.lightColor[active_lights][0] = bubblecolor[light->type][0];
					frameConstants.lightColor[active_lights][1] = bubblecolor[light->type][1];
					frameConstants.lightColor[active_lights][2] = bubblecolor[light->type][2];
				}
			}
			else {
				frameConstants.lightColor[active_lights][0] = 0.5;
				frameConstants.lightColor[active_lights][1] = 0.5;
				frameConstants.lightColor[active_lights][2] = 0.5;
			}
			frameConstants.lightColor[active_lights][3] = light->minlight;
			++active_lights;
		}
	}
	frameConstants.lightsActive = active_lights;
	frameConstants.lightScale = ((lightmode == 2 ? 384 : 512) * bound(0.5, gl_modulate.value, 3)) / (65536);

	// Draw-world constants
	frameConstants.r_textureless = gl_textureless.integer || gl_max_size.integer == 1;
	frameConstants.r_farclip = max(r_farclip.value, 4096) * 0.577;
	frameConstants.skySpeedscale = r_refdef2.time * 8;
	frameConstants.skySpeedscale -= (int)frameConstants.skySpeedscale & ~127;
	frameConstants.skySpeedscale2 = r_refdef2.time * 16;
	frameConstants.skySpeedscale2 -= (int)frameConstants.skySpeedscale2 & ~127;

	frameConstants.waterAlpha = GL_WaterAlpha();

	frameConstants.r_drawflat = r_drawflat.integer;
	PASS_COLOR_AS_4F(frameConstants.r_wallcolor, r_wallcolor);
	PASS_COLOR_AS_4F(frameConstants.r_floorcolor, r_floorcolor);

	frameConstants.r_fastturb = r_fastturb.integer;
	PASS_COLOR_AS_4F(frameConstants.r_telecolor, r_telecolor);
	PASS_COLOR_AS_4F(frameConstants.r_lavacolor, r_lavacolor);
	PASS_COLOR_AS_4F(frameConstants.r_slimecolor, r_slimecolor);
	PASS_COLOR_AS_4F(frameConstants.r_watercolor, r_watercolor);

	PASS_COLOR_AS_4F(frameConstants.r_skycolor, r_skycolor);

	frameConstants.r_texture_luma_fb = gl_fb_bmodels.integer ? 1 : 0;

	// Alias models
	{
		extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
		extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;

		frameConstants.shellSize = bound(0, gl_powerupshells_size.value, 20);
		frameConstants.shell_base_level1 = gl_powerupshells_base1level.value;
		frameConstants.shell_base_level2 = gl_powerupshells_base2level.value;
		frameConstants.shell_effect_level1 = gl_powerupshells_effect1level.value;
		frameConstants.shell_effect_level2 = gl_powerupshells_effect2level.value;
		frameConstants.shell_alpha = bound(0, gl_powerupshells.value, 1);
	}

	frameConstantsUploaded = false;
	GLM_UploadFrameConstants();
}

void GLM_SetupGL(void)
{
	extern cvar_t gl_textureless;

	GLM_GetMatrix(GL_MODELVIEW, frameConstants.modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, frameConstants.projectionMatrix);
	VectorCopy(r_refdef.vieworg, frameConstants.position);

	frameConstantsUploaded = false;
}

void GLM_UploadFrameConstants(void)
{
	if (!frameConstantsUploaded) {
		if (!GL_BufferReferenceIsValid(ubo_frameConstants)) {
			ubo_frameConstants = GL_GenUniformBuffer("frameConstants", &ubo_frameConstants, sizeof(frameConstants));
			GL_BindBufferBase(ubo_frameConstants, GL_BINDINGPOINT_FRAMECONSTANTS);
		}

		GL_UpdateBuffer(ubo_frameConstants, sizeof(frameConstants), &frameConstants);
		frameConstantsUploaded = true;
	}
}

void GLM_ScreenDrawStart(void)
{
	extern cvar_t gl_postprocess_gamma;

	if (gl_postprocess_gamma.integer) {
		if (!GL_FramebufferReferenceIsValid(framebuffer)) {
			framebuffer = GL_FramebufferCreate(glConfig.vidWidth, glConfig.vidHeight, true);
		}

		if (GL_FramebufferReferenceIsValid(framebuffer)) {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			GL_FramebufferStartUsing(framebuffer);
		}
	}
	else {
		if (GL_FramebufferReferenceIsValid(framebuffer)) {
			GL_FramebufferDelete(&framebuffer);
		}
	}
}

void GLM_PostProcessScene(void)
{
}

void GLM_PostProcessScreen(void)
{
	if (GL_FramebufferReferenceIsValid(framebuffer)) {
		GL_FramebufferStopUsing(framebuffer);

		if (GLM_CompilePostProcessProgram()) {
			GL_UseProgram(post_process_program.program);
			GL_BindVertexArray(&post_process_vao);

			GL_EnsureTextureUnitBound(GL_TEXTURE0, GL_FramebufferTextureReference(framebuffer, 0));
			GL_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}
}

#define POSTPROCESS_GAMMA 1

static qbool GLM_CompilePostProcessProgram(void)
{
	int options = (v_gamma.value != 1 ? POSTPROCESS_GAMMA : 0);

	if (GLM_ProgramRecompileNeeded(&post_process_program, options)) {
		static char included_definitions[512];
		GL_VFDeclare(post_process_screen);

		memset(included_definitions, 0, sizeof(included_definitions));
		if (options & POSTPROCESS_GAMMA) {
			strlcat(included_definitions, "#define POSTPROCESS_GAMMA\n", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("post-process-screen", GL_VFParams(post_process_screen), &post_process_program, included_definitions);

		post_process_program.custom_options = options;
	}

	if (post_process_program.program && !post_process_program.uniforms_found) {
		GLint common2d_block = glGetUniformBlockIndex(post_process_program.program, "Common2d");

		post_process_program.uniforms_found = true;
	}

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

		GL_ConfigureVertexAttribPointer(&post_process_vao, post_process_vbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*) 0, 0);
		GL_ConfigureVertexAttribPointer(&post_process_vao, post_process_vbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*) (sizeof(float) * 3), 0);
	}

	return post_process_program.program && post_process_vao.vao;
}
