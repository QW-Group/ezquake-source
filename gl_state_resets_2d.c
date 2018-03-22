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

// gl_state_resets.c
// moving state init/reset to here with intention of getting rid of all resets

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_matrix.h"
#include "r_state.h"

static rendering_state_t default2DState;
static rendering_state_t brightenScreenState;
static rendering_state_t lineState;
static rendering_state_t sceneBlurState;
static rendering_state_t glcImageDrawState;
static rendering_state_t glmImageDrawState;
static rendering_state_t glcBloomState;
static rendering_state_t polyBlendState;

void R_Initialise2DStates(void)
{
	R_InitRenderingState(&default2DState, true);
	default2DState.depth.test_enabled = false;
	default2DState.cullface.enabled = false;

	R_InitRenderingState(&brightenScreenState, true);
	brightenScreenState.depth.test_enabled = false;
	brightenScreenState.cullface.enabled = false;
	brightenScreenState.alphaTesting.enabled = true; // really?
	brightenScreenState.blendingEnabled = true;
	brightenScreenState.blendFunc = r_blendfunc_src_dst_color_dest_one;

	R_InitRenderingState(&lineState, true);
	lineState.depth.test_enabled = false;
	lineState.cullface.enabled = false;
	lineState.blendingEnabled = true;
	lineState.blendFunc = r_blendfunc_premultiplied_alpha;
	lineState.line.flexible_width = true;

	R_InitRenderingState(&sceneBlurState, true);
	sceneBlurState.depth.test_enabled = false;
	sceneBlurState.cullface.enabled = false;
	//GL_Viewport(0, 0, glwidth, glheight);
	sceneBlurState.alphaTesting.enabled = false;
	sceneBlurState.blendingEnabled = true;
	sceneBlurState.blendFunc = r_blendfunc_premultiplied_alpha;
	sceneBlurState.textureUnits[0].enabled = true;
	sceneBlurState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&glcImageDrawState, true);
	glcImageDrawState.depth.test_enabled = false;
	glcImageDrawState.cullface.enabled = false;
	glcImageDrawState.textureUnits[0].enabled = true;
	glcImageDrawState.textureUnits[0].mode = r_texunit_mode_modulate;
	glcImageDrawState.blendingEnabled = true;
	glcImageDrawState.blendFunc = r_blendfunc_premultiplied_alpha;

	R_InitRenderingState(&glcBloomState, true);
	glcBloomState.depth.test_enabled = false;
	glcBloomState.cullface.enabled = false;
	glcBloomState.alphaTesting.enabled = true;
	glcBloomState.blendingEnabled = true;
	glcBloomState.blendFunc = r_blendfunc_additive_blending;
	glcBloomState.color[0] = glcBloomState.color[1] = glcBloomState.color[2] = r_bloom_alpha.value;
	glcBloomState.color[3] = 1.0f;
	glcBloomState.textureUnits[0].enabled = true;
	glcBloomState.textureUnits[0].mode = r_texunit_mode_modulate;

	R_InitRenderingState(&polyBlendState, true);
	polyBlendState.depth.test_enabled = false;
	polyBlendState.cullface.enabled = false;
	polyBlendState.blendingEnabled = true;
	polyBlendState.blendFunc = r_blendfunc_premultiplied_alpha;
	polyBlendState.color[0] = v_blend[0] * v_blend[3];
	polyBlendState.color[1] = v_blend[1] * v_blend[3];
	polyBlendState.color[2] = v_blend[2] * v_blend[3];
	polyBlendState.color[3] = v_blend[3];

	R_InitRenderingState(&glmImageDrawState, true);
	glmImageDrawState.depth.test_enabled = false;
	glmImageDrawState.alphaTesting.enabled = false;
	glmImageDrawState.blendingEnabled = r_blendfunc_premultiplied_alpha;
}

void GL_StateDefault2D(void)
{
	GL_ResetRegion(false);
}

void GLC_StateBeginBrightenScreen(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&brightenScreenState);

	LEAVE_STATE;
}

void GLC_StateEndBrightenScreen(void)
{
}

void GL_StateBeginAlphaLineRGB(float thickness)
{
	ENTER_STATE;

	R_ApplyRenderingState(&lineState);
	if (thickness > 0.0) {
		glLineWidth(thickness);
	}

	LEAVE_STATE;
}

void GL_StateEndAlphaLineRGB(void)
{
}

void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness)
{
	ENTER_STATE;

	// Same as lineState
	R_ApplyRenderingState(&lineState);
	if (thickness > 0.0) {
		glLineWidth(thickness);
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawAlphaPieSliceRGB(float thickness)
{
}

void GLC_StateBeginSceneBlur(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&sceneBlurState);

	GL_IdentityModelView();
	GL_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);

	LEAVE_STATE;
}

void GLC_StateEndSceneBlur(void)
{
}

void GLC_StateBeginDrawPolygon(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&lineState);

	LEAVE_STATE;
}

void GLC_StateEndDrawPolygon(int oldFlags)
{
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	ENTER_STATE;

	R_ApplyRenderingState(&glcBloomState);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);

	LEAVE_STATE;
}

void GLC_StateEndBloomDraw(void)
{
}

void GLC_StateBeginPolyBlend(float v_blend[4])
{
	ENTER_STATE;

	R_ApplyRenderingState(&polyBlendState);

	LEAVE_STATE;
}

void GLC_StateEndPolyBlend(void)
{
}

void GLC_StateBeginImageDraw(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&glcImageDrawState);

	LEAVE_STATE;
}

void GLC_StateEndImageDraw(void)
{
}

void GL_StateBeginPolyBlend(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&polyBlendState);

	LEAVE_STATE;
}

void GL_StateEndPolyBlend(void)
{
}

void GLM_StateBeginImageDraw(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&glmImageDrawState);

	LEAVE_STATE;
}

void GLM_StateBeginPolygonDraw(void)
{
}
