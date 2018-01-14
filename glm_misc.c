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

void GLM_BrightenScreen(void)
{
	// MEAG: TODO
}

void GLM_DrawVelocity3D(void)
{
	// MEAG: TODO
}

void GLM_RenderSceneBlurDo(float alpha)
{
	// MEAG: TODO
}

void GLM_EmitCausticsPolys(void)
{
	// MEAG: TODO
	// Is this ever going to be a thing?
}

// Reference cvars for 3D views...
typedef struct block_refdef_s {
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float time;
	float gamma3d;

	// if enabled, texture coordinates are always 0,0
	int r_textureless;

	int padding;
} block_refdef_t;

// Reference settings for 2D views...
typedef struct block_common2d_s {
	float gamma2d;

	int r_alphafont;

	int padding[2];
} block_common2d;

static glm_ubo_t ubo_refdef;
static glm_ubo_t ubo_common2d;
static block_refdef_t refdef;
static block_common2d common2d;

void GLM_PreRenderView(void)
{
	extern cvar_t gl_alphafont;

	common2d.gamma2d = v_gamma.value;
	common2d.r_alphafont = gl_alphafont.value;

	if (!ubo_common2d.ubo) {
		GL_GenUniformBuffer(&ubo_common2d, "common2d", &common2d, sizeof(common2d));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_COMMON2D_CVARS, ubo_common2d.ubo);
	}

	GL_UpdateUBO(&ubo_common2d, sizeof(common2d), &common2d);
}

void GLM_SetupGL(void)
{
	extern cvar_t gl_textureless;

	GLM_GetMatrix(GL_MODELVIEW, refdef.modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, refdef.projectionMatrix);
	refdef.time = cl.time;
	refdef.gamma3d = v_gamma.value;
	refdef.r_textureless = gl_textureless.integer || gl_max_size.integer == 1;

	if (!ubo_refdef.ubo) {
		GL_GenUniformBuffer(&ubo_refdef, "refdef", &refdef, sizeof(refdef));
		glBindBufferBase(GL_UNIFORM_BUFFER, GL_BINDINGPOINT_REFDEF_CVARS, ubo_refdef.ubo);
	}

	GL_UpdateUBO(&ubo_refdef, sizeof(refdef), &refdef);
}
