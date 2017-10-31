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

#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, false)
#define MIDDLE_STATE GL_MarkEvent(__FUNCTION__)
#define LEAVE_STATE GL_LeaveRegion()

void GL_StateDefault2D(void)
{
	GL_ResetRegion(false);

	ENTER_STATE;

	GL_PrintState();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (!GL_ShadersSupported()) {
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
		GL_Color3ubv(color_white);
	}
	GL_PrintState();
}

void GLC_StateBeginBrightenScreen(void)
{
	ENTER_STATE;

	glDisable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_BlendFunc(GL_DST_COLOR, GL_ONE);
}

void GLC_StateEndBrightenScreen(void)
{
	LEAVE_STATE;

	// FIXME: GL_ResetState()
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glColor3ubv(color_white);
}

void GL_StateBeginAlphaLineRGB(float thickness)
{
	ENTER_STATE;

	if (!GL_ShadersSupported()) {
		glDisable(GL_TEXTURE_2D);
	}
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	if (thickness > 0.0) {
		glLineWidth(thickness);
	}

	LEAVE_STATE;
}

void GL_StateEndAlphaLineRGB(void)
{
	ENTER_STATE;

	if (!GL_ShadersSupported()) {
		glEnable(GL_TEXTURE_2D);
	}
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);

	LEAVE_STATE;
}

void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness)
{
	ENTER_STATE;

	glDisable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	if (thickness > 0.0) {
		glLineWidth(thickness);
	}
}

void GLC_StateEndDrawAlphaPieSliceRGB(float thickness)
{
	LEAVE_STATE;

	// FIXME: thickness is not reset
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	glColor4ubv(color_white);
}

void GLC_StateBeginSceneBlur(void)
{
	ENTER_STATE;

	// Remember all attributes.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	GL_Viewport(0, 0, glwidth, glheight);
	GL_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
	GL_IdentityModelView();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
}

void GLC_StateEndSceneBlur(void)
{
	LEAVE_STATE;

	// FIXME: Yup, this is the same

	// Restore attributes.
	glPopAttrib();
}

void GLC_StateBeginDrawPolygon(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	glDisable(GL_TEXTURE_2D);
	// (color also set)
}

void GLC_StateEndDrawPolygon(int oldFlags)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(oldFlags);
	glEnable(GL_TEXTURE_2D);
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_Color4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	GLC_InitTextureUnits1(texture, GL_MODULATE);
}

void GLC_StateEndBloomDraw(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
}

void GLC_StateBeginPolyBlend(void)
{
	ENTER_STATE;

	glDisable(GL_TEXTURE_2D);
	glColor4fv(v_blend);

	LEAVE_STATE;
}

void GLC_StateEndPolyBlend(void)
{
	ENTER_STATE;

	glEnable(GL_TEXTURE_2D);
	glColor3ubv(color_white);

	LEAVE_STATE;
}

void GL_StateBeginNetGraph(qbool texture)
{
	if (!GL_ShadersSupported()) {
		if (!texture || !GL_TextureReferenceIsValid(netgraphtexture)) {
			GLC_InitTextureUnitsNoBind1(GL_MODULATE);
			GLC_EnsureTMUDisabled(GL_TEXTURE0);
		}
		else {
			GLC_InitTextureUnits1(netgraphtexture, GL_MODULATE);
			GLC_EnsureTMUEnabled(GL_TEXTURE0);
		}
	}
}

void GL_StateEndNetGraph(void)
{
	if (!GL_ShadersSupported()) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
		GL_TextureEnvMode(GL_REPLACE);
		glColor4ubv(color_white);
		glEnable(GL_TEXTURE_2D);
	}
}
