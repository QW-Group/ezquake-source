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

void GL_StateDefault2D(void)
{
	GL_ResetRegion(false);

	ENTER_STATE;

	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);

	LEAVE_STATE;
}

void GLC_StateBeginBrightenScreen(void)
{
	ENTER_STATE;

	GL_Color3ubv(color_white);
	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_DST_COLOR, GL_ONE);

	LEAVE_STATE;
}

void GLC_StateEndBrightenScreen(void)
{
}

void GL_StateBeginAlphaLineRGB(float thickness)
{
	ENTER_STATE;

	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GLC_DisableAllTexturing();
	}
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

	GLC_DisableAllTexturing();
	GL_Color3ubv(color_white);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

	if (!GL_ShadersSupported()) {
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
		GL_Color3ubv(color_white);
	}
	// Remember all attributes.
	GL_Viewport(0, 0, glwidth, glheight);
	GL_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
	GL_IdentityModelView();
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	LEAVE_STATE;
}

void GLC_StateEndSceneBlur(void)
{
}

void GLC_StateBeginDrawPolygon(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLC_DisableAllTexturing();
	// (color set during call)

	LEAVE_STATE;
}

void GLC_StateEndDrawPolygon(int oldFlags)
{
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	ENTER_STATE;

	GL_Color3ubv(color_white);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_Color4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	GLC_InitTextureUnits1(texture, GL_MODULATE);

	LEAVE_STATE;
}

void GLC_StateEndBloomDraw(void)
{
}

void GLC_StateBeginPolyBlend(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLC_DisableAllTexturing();
	glColor4fv(v_blend);

	LEAVE_STATE;
}

void GLC_StateEndPolyBlend(void)
{
}

void GL_StateBeginNetGraph(void)
{
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_Color3ubv(color_white);
	if (!GL_ShadersSupported()) {
		GLC_InitTextureUnitsNoBind1(GL_MODULATE);
		GLC_EnsureTMUDisabled(GL_TEXTURE0);
	}
}

void GL_StateEndNetGraph(void)
{
}

void GLC_StateBeginImageDraw(void)
{
	GL_Color3ubv(color_white);
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED); // alphatest depends on image type
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
