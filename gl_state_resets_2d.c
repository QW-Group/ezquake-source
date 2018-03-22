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

void R_Initialise2DStates(void)
{
	R_InitRenderingState(&default2DState, true);
	default2DState.depth.test_enabled = false;
	default2DState.cullface.enabled = false;

	R_InitRenderingState(&brightenScreenState, true);
	brightenScreenState.depth.test_enabled = false;
	brightenScreenState.cullface.enabled = false;
	brightenScreenState.alphaTestingEnabled = true; // really?
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
	sceneBlurState.alphaTestingEnabled = false;
	sceneBlurState.blendingEnabled = true;
	sceneBlurState.blendFunc = r_blendfunc_premultiplied_alpha;
}

void GL_StateDefault2D(void)
{
	GL_ResetRegion(false);
}

void GLC_StateBeginBrightenScreen(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&brightenScreenState);
	glColor3ubv(color_white);
	GLC_DisableAllTexturing();

	LEAVE_STATE;
}

void GLC_StateEndBrightenScreen(void)
{
}

void GL_StateBeginAlphaLineRGB(float thickness)
{
	ENTER_STATE;

	if (GL_UseImmediateMode()) {
		glColor3ubv(color_white);
		GLC_DisableAllTexturing();
	}

	R_ApplyRenderState(lineState);
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
	if (GL_UseImmediateMode()) {
		glColor3ubv(color_white);
		GLC_DisableAllTexturing();
	}

	R_ApplyRenderState(lineState);
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

	// Same as lineState
	if (GL_UseImmediateMode()) {
		glColor3ubv(color_white);
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	}

	R_ApplyRenderState(&sceneBlurState);

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

	GLC_DisableAllTexturing();
	R_ApplyRenderState(lineState);

	LEAVE_STATE;
}

void GLC_StateEndDrawPolygon(int oldFlags)
{
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);
	glColor4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	GLC_InitTextureUnits1(texture, GL_MODULATE);

	LEAVE_STATE;
}

void GLC_StateEndBloomDraw(void)
{
}

void GLC_StateBeginPolyBlend(float v_blend[4])
{
	ENTER_STATE;

	GLC_DisableAllTexturing();
	glColor4f(
		v_blend[0] * v_blend[3],
		v_blend[1] * v_blend[3],
		v_blend[2] * v_blend[3],
		v_blend[3]
	);

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	LEAVE_STATE;
}

void GLC_StateEndPolyBlend(void)
{
}

void GLC_StateBeginImageDraw(void)
{
	glColor3ubv(color_white);
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED); // alphatest depends on image type
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void GLC_StateEndImageDraw(void)
{
}

void GL_StateBeginPolyBlend(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	LEAVE_STATE;
}

void GL_StateEndPolyBlend(void)
{
}
