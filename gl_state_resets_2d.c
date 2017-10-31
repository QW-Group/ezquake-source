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

extern int debug_frame_depth;
#define ENTER_STATE \
	Com_DPrintf("%.*s %s\n", debug_frame_depth, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __FUNCTION__); \
	++debug_frame_depth;
#define MIDDLE_STATE \
	Com_DPrintf("%.*s %s\n", debug_frame_depth, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __FUNCTION__);
#define LEAVE_STATE \
	Com_DPrintf("%.*s %s\n", debug_frame_depth, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>", __FUNCTION__); \
	--debug_frame_depth;

void GL_StateDefault2D(void)
{
	debug_frame_depth = 0;

	ENTER_STATE;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	GL_Color3ubv(color_white);
}

void GLC_StateBeginDrawImage(qbool alpha, byte color[4])
{
	ENTER_STATE;

	GL_TextureEnvMode(GL_MODULATE);
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	GL_AlphaBlendFlags((alpha ? GL_ALPHATEST_ENABLED : GL_ALPHATEST_DISABLED));
	GL_Color4ubv(color);
}

void GLC_StateEndDrawImage(void)
{
	LEAVE_STATE;

	// FIXME: GL_ResetState
	glColor3ubv (color_white);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
}

void GLC_StateBeginAlphaPic(float alpha)
{
	ENTER_STATE;

	if (alpha < 1.0) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_TextureEnvMode(GL_MODULATE);
		GL_CullFace(GL_FRONT);
		glColor4f(1, 1, 1, alpha);
	}
}

void GLC_StateEndAlphaPic(float alpha)
{
	LEAVE_STATE;

	if (alpha < 1.0) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
		GL_TextureEnvMode(GL_REPLACE);
		glColor4f(1, 1, 1, 1);
	}
}

void GLC_StateBeginAlphaRectangle(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	glDisable (GL_TEXTURE_2D);
}

void GLC_StateEndAlphaRectangle(void)
{
	LEAVE_STATE;

	glColor4ubv (color_white);
	glEnable (GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
}

void GLC_StateBeginFadeScreen(float alpha)
{
	ENTER_STATE;

	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		glColor4f(0, 0, 0, alpha);
	}
	else {
		glColor3f(0, 0, 0);
	}
	glDisable(GL_TEXTURE_2D);
}

void GLC_StateEndFadeScreen(void)
{
	LEAVE_STATE;

	glColor3ubv(color_white);
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
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


void GL_BeginStateAlphaLineRGB(float thickness)
{
	glDisable (GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	if (thickness > 0.0) {
		glLineWidth(thickness);
	}
}

void GL_EndStateAlphaLineRGB(void)
{
	glEnable(GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
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

void GLC_StateBeginDrawCharacterBase(qbool apply_overall_alpha, byte color[4])
{
	extern cvar_t gl_alphafont;
	extern cvar_t scr_coloredText;
	extern float overall_alpha;

	MIDDLE_STATE;

	// Turn on alpha transparency.
	if ((gl_alphafont.value || apply_overall_alpha)) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	}
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	if (scr_coloredText.integer) {
		GL_TextureEnvMode(GL_MODULATE);
	}
	else {
		GL_TextureEnvMode(GL_REPLACE);
	}

	// Set the overall alpha.
	glColor4ub(color[0], color[1], color[2], color[3] * overall_alpha);
}

void GLC_StateResetCharGLState(void)
{
	LEAVE_STATE

		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	glColor4ubv(color_white);
}

void GLC_StateBeginStringDraw(void)
{
	extern cvar_t gl_alphafont;
	extern cvar_t scr_coloredText;
	extern float overall_alpha;

	ENTER_STATE;

	// Turn on alpha transparency.
	if (gl_alphafont.value || (overall_alpha < 1.0)) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	}
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	if (scr_coloredText.integer) {
		GL_TextureEnvMode(GL_MODULATE);
	}
	else {
		GL_TextureEnvMode(GL_REPLACE);
	}
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

void GL_StateBeginSCRTeamInfo(void)
{
	ENTER_STATE;

	GL_TextureEnvMode(GL_MODULATE);
	GL_Color4f(1, 1, 1, 1);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
}

void GL_StateEndSCRTeamInfo(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	GL_Color4f(1, 1, 1, 1);
}

void GL_StateBeginSCRShowNick(void)
{
	ENTER_STATE;

	GL_Color4f(1, 1, 1, 1);
	GL_TextureEnvMode(GL_MODULATE);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
}

void GL_StateEndSCRShowNick(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	GL_Color4f(1, 1, 1, 1);
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

void GLC_StateBeginBloomDraw(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GL_Color4f(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	GL_TextureEnvMode(GL_MODULATE);
}

void GLC_StateEndBloomDraw(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
}

