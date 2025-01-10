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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_framebuffer.h"
#include "tr_types.h"
#include "glsl/constants.glsl"
#include "r_lighting.h"
#include "r_matrix.h"
#include "r_buffers.h"
#include "glm_local.h"
#include "r_renderer.h"
#include "gl_texture.h"
#include "r_brushmodel_sky.h"

static uniform_block_frame_constants_t frameConstants;
static qbool frameConstantsUploaded = false;

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

	GLM_StateBeginPolyBlend();
	Draw_AlphaRectangleRGB(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0.0f, true, v_blend_color);
}

void GLM_PreRenderView(void)
{
	extern cvar_t gl_alphafont, gl_max_size;
	extern cvar_t r_telecolor, r_lavacolor, r_slimecolor, r_watercolor, r_fastturb, r_skycolor;
	extern cvar_t gl_textureless, gl_hwblend;
	int i, active_lights = 0;
	float blend_alpha;

	// General constants
	frameConstants.time = cl.time;
	frameConstants.gamma = bound(0.3, v_gamma.value, 3);
	frameConstants.contrast = bound(1, v_contrast.value, 3);
	frameConstants.r_alphatestfont = gl_alphafont.integer ? 0 : 1;
	blend_alpha = (!vid_hwgamma_enabled || !gl_hwblend.value || cl.teamfortress) ? 0 : v_blend[3];
	frameConstants.v_blend[0] = v_blend[0] * blend_alpha;
	frameConstants.v_blend[1] = v_blend[1] * blend_alpha;
	frameConstants.v_blend[2] = v_blend[2] * blend_alpha;
	frameConstants.v_blend[3] = 1 - blend_alpha;

	// Lights
	for (i = 0; i < MAX_DLIGHTS; ++i) {
		if (cl_dlight_active[i / 32] & (1 << (i % 32))) {
			extern cvar_t gl_colorlights;

			dlight_t* light = &cl_dlights[i];
			float* lightColor = frameConstants.lightColor[active_lights];
			float* lightPosition = frameConstants.lightPosition[active_lights];
			
			VectorCopy(light->origin, lightPosition);
			lightPosition[3] = light->radius;
			if (gl_colorlights.integer) {
				if (light->type == lt_custom) {
					lightColor[0] = light->color[0] / 255.0;
					lightColor[1] = light->color[1] / 255.0;
					lightColor[2] = light->color[2] / 255.0;
				}
				else {
					lightColor[0] = dlightcolor[light->type][0] / 255.0;
					lightColor[1] = dlightcolor[light->type][1] / 255.0;
					lightColor[2] = dlightcolor[light->type][2] / 255.0;
				}
			}
			else {
				lightColor[0] = lightColor[1] = lightColor[2] = 0.5;
			}
			lightColor[3] = light->minlight;
			++active_lights;
		}
	}
	frameConstants.lightsActive = active_lights;
	frameConstants.lightScale = ((lightmode == 2 ? 1.5 : 2) * bound(0.5, gl_modulate.value, 3));

	// Draw-world constants
	frameConstants.r_textureless = gl_textureless.integer || gl_max_size.integer == 1;
	frameConstants.skySpeedscale = r_refdef2.time * 8;
	frameConstants.skySpeedscale -= (int)frameConstants.skySpeedscale & ~127;
	frameConstants.skySpeedscale /= 128.0f;
	frameConstants.skySpeedscale2 = r_refdef2.time * 16;
	frameConstants.skySpeedscale2 -= (int)frameConstants.skySpeedscale2 & ~127;
	frameConstants.skySpeedscale2 /= 128.0f;

	frameConstants.r_drawflat = r_drawflat.integer;
	PASS_COLOR_AS_4F(frameConstants.r_wallcolor, r_wallcolor);
	PASS_COLOR_AS_4F(frameConstants.r_floorcolor, r_floorcolor);

	frameConstants.r_fastturb = r_fastturb.integer;
	PASS_COLOR_AS_4F(frameConstants.r_telecolor, r_telecolor);
	PASS_COLOR_AS_4F(frameConstants.r_lavacolor, r_lavacolor);
	PASS_COLOR_AS_4F(frameConstants.r_slimecolor, r_slimecolor);
	PASS_COLOR_AS_4F(frameConstants.r_watercolor, r_watercolor);

	PASS_COLOR_AS_4F(frameConstants.r_skycolor, r_skycolor);

	// Alias models
	{
		extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
		extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;

		frameConstants.shellSize_unused = 0.5f;
		frameConstants.shell_base_level1 = gl_powerupshells_base1level.value;
		frameConstants.shell_base_level2 = gl_powerupshells_base2level.value;
		frameConstants.shell_effect_level1 = gl_powerupshells_effect1level.value;
		frameConstants.shell_effect_level2 = gl_powerupshells_effect2level.value;
		frameConstants.shell_alpha = bound(0, gl_powerupshells.value, 1);
	}

	// Window constants
	frameConstants.r_width = VID_ScaledWidth3D();
	frameConstants.r_height = VID_ScaledHeight3D();
	frameConstants.r_inv_width = 1.0f / (float) frameConstants.r_width;
	frameConstants.r_inv_height = 1.0f / (float) frameConstants.r_height;
	frameConstants.r_zFar = R_FarPlaneZ();
	frameConstants.r_zNear = R_NearPlaneZ();

	// fog colors
	VectorCopy(r_refdef2.fog_color, frameConstants.fogColor);
	frameConstants.fogMinZ = r_refdef2.fog_linear_start;
	frameConstants.fogMaxZ = r_refdef2.fog_linear_end;
	frameConstants.skyFogMix = r_refdef2.fog_sky;
	frameConstants.fogDensity = r_refdef2.fog_density; // (r_refdef2.fog_calculation == fogcalc_exp2 ? r_refdef2.fog_density * r_refdef2.fog_density : r_refdef2.fog_density);

	Skywind_GetDirectionAndPhase(frameConstants.windDir, &frameConstants.windPhase);

	frameConstantsUploaded = false;
}

void GLM_SetupGL(void)
{
	memcpy(frameConstants.modelViewMatrix, R_ModelviewMatrix(), sizeof(frameConstants.modelViewMatrix));
	memcpy(frameConstants.projectionMatrix, R_ProjectionMatrix(), sizeof(frameConstants.projectionMatrix));
	VectorCopy(r_refdef.vieworg, frameConstants.position);

	frameConstants.camangles[PITCH] = DEG2RAD(r_refdef.viewangles[PITCH]);
	frameConstants.camangles[YAW] = DEG2RAD(r_refdef.viewangles[YAW]);
	frameConstants.camangles[ROLL] = DEG2RAD(r_refdef.viewangles[ROLL]);

	frameConstantsUploaded = false;
}

void GLM_UploadFrameConstants(void)
{
	if (!frameConstantsUploaded) {
		if (!R_BufferReferenceIsValid(r_buffer_frame_constants)) {
			buffers.Create(r_buffer_frame_constants, buffertype_uniform, "frameConstants", sizeof(frameConstants), &frameConstants, bufferusage_once_per_frame);
		}

		buffers.BindRange(r_buffer_frame_constants, EZQ_GL_BINDINGPOINT_FRAMECONSTANTS, buffers.BufferOffset(r_buffer_frame_constants), sizeof(frameConstants));
		buffers.Update(r_buffer_frame_constants, sizeof(frameConstants), &frameConstants);
		frameConstantsUploaded = true;
	}
}

void GLM_ScreenDrawStart(void)
{
	GL_FramebufferScreenDrawStart();
}

void GLM_PostProcessScreen(void)
{
	GL_FramebufferPostProcessScreen();
}

void GLM_Shutdown(r_shutdown_mode_t mode)
{
	if (mode != r_shutdown_reload) {
		renderer.ProgramsShutdown(mode == r_shutdown_restart);
		GL_DeleteSamplers();
	}
	GL_FramebufferDeleteAll();
}

#endif
