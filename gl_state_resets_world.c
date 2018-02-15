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

// gl_state_resets_world.c
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLC_StateBeginDrawFlatModel(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_BLEND);

	// START shaman BUG /fog not working with /r_drawflat {
	if (gl_fogenable.integer) {
		glEnable(GL_FOG);
	}
	// } END shaman BUG /fog not working with /r_drawflat

	LEAVE_STATE;
}

void GLC_StateEndDrawFlatModel(void)
{
	ENTER_STATE;

	if (gl_fogenable.integer) {
		GL_Disable(GL_FOG);
	}

	GL_Color4ubv(color_white);

	LEAVE_STATE;
}

void GLC_StateBeginDrawTextureChains(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit, GLenum fullbrightMode)
{
	ENTER_STATE;

	if (gl_fogenable.integer) {
		GL_Enable(GL_FOG);
	}

	GL_TextureEnvModeForUnit(GL_TEXTURE0, GL_REPLACE);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);

	if (lightmapTextureUnit) {
		GLC_EnsureTMUEnabled(lightmapTextureUnit);
		GLC_SetLightmapTextureEnvironment(lightmapTextureUnit);
	}
	if (fullbrightTextureUnit) {
		GL_TextureEnvModeForUnit(fullbrightTextureUnit, fullbrightMode);
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawTextureChainsFirstPass(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit)
{
	MIDDLE_STATE;

	if (fullbrightTextureUnit) {
		GLC_EnsureTMUDisabled(fullbrightTextureUnit);
	}
	if (lightmapTextureUnit) {
		GLC_EnsureTMUDisabled(lightmapTextureUnit);
	}
}

void GLC_StateEndDrawTextureChains(void)
{
	ENTER_STATE;

	if (gl_fogenable.integer) {
		glDisable(GL_FOG);
	}

	LEAVE_STATE;
}

void GLC_StateBeginFastTurbPoly(byte color[4])
{
	float wateralpha = GL_WaterAlpha();

	ENTER_STATE;

	GL_EnableFog();
	GLC_DisableAllTexturing();

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	if (wateralpha < 1.0 && wateralpha >= 0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		color[3] = wateralpha * 255;
		GL_Color4ubv(color); // 1, 1, 1, wateralpha
		GLC_InitTextureUnitsNoBind1(GL_MODULATE);
		if (wateralpha < 0.9) {
			GL_DepthMask(GL_FALSE);
		}
	}
	else {
		GL_Color3ubv(color);
	}
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	LEAVE_STATE;
}

void GLC_StateEndFastTurbPoly(void)
{
	ENTER_STATE;

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_DepthMask(GL_TRUE);
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	GL_DisableFog();
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Color4ubv(color_white);
	// END shaman RFE 1022504

	LEAVE_STATE;
}

void GLC_StateBeginTurbPoly(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Enable(GL_TEXTURE_2D);
	GL_EnableFog();

	LEAVE_STATE;
}

void GLC_StateEndTurbPoly(void)
{
	ENTER_STATE;

	GL_DisableFog();

	LEAVE_STATE;
}

void GLC_StateBeginBlendLightmaps(void)
{
	ENTER_STATE;

	GL_DepthMask(GL_FALSE);		// don't bother writing Z
	GL_BlendFunc(GL_ZERO, GLC_LightmapDestBlendFactor());

	if (!R_DrawLightmaps()) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	}

	LEAVE_STATE;
}

void GLC_StateEndBlendLightmaps(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(GL_TRUE);		// back to normal Z buffering

	LEAVE_STATE;
}

void GLC_StateBeginCausticsPolys(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_DECAL);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	LEAVE_STATE;
}

void GLC_StateEndCausticsPolys(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	LEAVE_STATE;
}

void GLC_StateBeginUnderwaterCaustics(void)
{
	ENTER_STATE;

	GLC_EnsureTMUEnabled(GL_TEXTURE1);
	GL_EnsureTextureUnitBound(GL_TEXTURE1, underwatertexture);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(0.5, 0.5, 1);
	glRotatef(r_refdef2.time * 10, 1, 0, 0);
	glRotatef(r_refdef2.time * 10, 0, 1, 0);
	glMatrixMode(GL_MODELVIEW);

	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	LEAVE_STATE;
}

void GLC_StateEndUnderwaterCaustics(void)
{
	ENTER_STATE;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	GL_TextureEnvModeForUnit(GL_TEXTURE1, GL_REPLACE);
	GLC_EnsureTMUDisabled(GL_TEXTURE1);

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginWaterSurfaces(void)
{
	float wateralpha = GL_WaterAlpha();

	ENTER_STATE;

	if (wateralpha < 1.0) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_Color4f(1, 1, 1, wateralpha);
		GLC_InitTextureUnitsNoBind1(GL_MODULATE);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		if (wateralpha < 0.9) {
			GL_DepthMask(GL_FALSE);
		}
	}
	else {
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
		GL_Color3ubv(color_white);
	}

	LEAVE_STATE;
}

void GLC_StateEndWaterSurfaces(void)
{
	float wateralpha = GL_WaterAlpha();

	ENTER_STATE;

	if (wateralpha < 1.0) {
		GL_TextureEnvMode(GL_REPLACE);

		GL_Color3ubv(color_white);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		if (wateralpha < 0.9) {
			GL_DepthMask(GL_TRUE);
		}
	}

	LEAVE_STATE;
}

void GLC_StateBeginAlphaChain(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	LEAVE_STATE;
}

void GLC_StateEndAlphaChain(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginAlphaChainSurface(msurface_t* s)
{
	texture_t* t = s->texinfo->texture;

	ENTER_STATE;

	//bind the world texture
	if (gl_mtexable) {
		GLC_InitTextureUnits2(t->gl_texturenum, GL_REPLACE, GLC_LightmapTexture(s->lightmaptexturenum), GLC_LightmapTexEnv());
	}
	else {
		GLC_InitTextureUnits1(t->gl_texturenum, GL_REPLACE);
	}

	LEAVE_STATE;
}

void GLC_StateBeginRenderFullbrights(void)
{
	ENTER_STATE;

	// don't bother writing Z
	GL_DepthMask(GL_FALSE);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateEndRenderFullbrights(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_DepthMask(GL_TRUE);

	LEAVE_STATE;
}

void GLC_StateBeginRenderLumas(void)
{
	ENTER_STATE;

	GL_DepthMask(GL_FALSE);	// don't bother writing Z
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateEndRenderLumas(void)
{
	ENTER_STATE;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(GL_TRUE);

	LEAVE_STATE;
}

void GLC_StateBeginEmitDetailPolys(void)
{
	ENTER_STATE;

	GLC_InitTextureUnits1(detailtexture, GL_DECAL);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	LEAVE_STATE;
}

void GLC_StateEndEmitDetailPolys(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	LEAVE_STATE;
}

void GLC_StateBeginDrawMapOutline(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_Color4ubv(color_white);
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);
	GLC_DisableAllTexturing();

	LEAVE_STATE;
}

void GLC_StateBeginEndMapOutline(void)
{
	ENTER_STATE;

	GL_Color4ubv(color_white);
	GL_Enable(GL_DEPTH_TEST);
	if (gl_cull.integer) {
		GL_Enable(GL_CULL_FACE);
	}
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);

	LEAVE_STATE;
}

void GLM_StateBeginDrawWorldOutlines(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_CullFace(GL_BACK);
	GL_PolygonMode(GL_LINE);
	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);
	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	LEAVE_STATE;
}

void GLM_StateEndDrawWorldOutlines(void)
{
	ENTER_STATE;

	GL_Enable(GL_DEPTH_TEST);
	GL_Enable(GL_CULL_FACE);
	GL_PolygonMode(GL_FILL);
	GL_CullFace(GL_FRONT);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);

	LEAVE_STATE;
}
