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
#include "gl_aliasmodel.h"

extern texture_ref solidskytexture, alphaskytexture;

#define ENTER_STATE GL_EnterTracedRegion(__FUNCTION__, true)
#define MIDDLE_STATE GL_MarkEvent(__FUNCTION__)
#define LEAVE_STATE GL_LeaveRegion()

float GL_WaterAlpha(void)
{
	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void GL_StateDefault3D(void)
{
	GL_ResetRegion(false);

	ENTER_STATE;

	GL_PrintState();
	// set drawing parms
	GL_CullFace(GL_FRONT);
	if (gl_cull.value) {
		glEnable(GL_CULL_FACE);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	if (CL_MultiviewEnabled()) {
		glClear(GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		GL_DepthFunc(GL_LEQUAL);
	}

	GL_DepthRange(gldepthmin, gldepthmax);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_ONE, GL_ZERO);

	glEnable(GL_DEPTH_TEST);

	if (gl_gammacorrection.integer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
	else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}
	GL_PrintState();
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

void GL_StateBeginEntities(visentlist_t* vislist)
{
	ENTER_STATE;

	// draw sprites separately, because of alpha_test
	GL_AlphaBlendFlags(
		(vislist->alpha ? GL_ALPHATEST_ENABLED : GL_ALPHATEST_DISABLED) |
		(vislist->alphablend ? GL_BLEND_ENABLED : GL_BLEND_DISABLED)
	);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	LEAVE_STATE;
}

void GL_StateEndEntities(visentlist_t* vislist)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (!GL_ShadersSupported()) {
		if (gl_affinemodels.value) {
			GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		}
		if (gl_smoothmodels.value) {
			GL_ShadeModel(GL_FLAT);
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
	extern texture_ref lightmap_textures[MAX_LIGHTMAPS];

	ENTER_STATE;

	//bind the world texture
	if (gl_mtexable) {
		GLC_InitTextureUnits2(t->gl_texturenum, GL_REPLACE, lightmap_textures[s->lightmaptexturenum], GLC_LightmapTexEnv());
	}
	else {
		GLC_InitTextureUnits1(t->gl_texturenum, GL_REPLACE);
	}

	LEAVE_STATE;
}

void GLC_StateBeginAliasOutlineFrame(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_CullFace(GL_BACK);
	glPolygonMode(GL_FRONT, GL_LINE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_LINE_SMOOTH);
	GLC_DisableAllTexturing();
}

void GLC_StateEndAliasOutlineFrame(void)
{
	LEAVE_STATE;

	glColor4f(1, 1, 1, 1);
	glPolygonMode(GL_FRONT, GL_FILL);
	glDisable(GL_LINE_SMOOTH);
	GL_CullFace(GL_FRONT);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);

	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
}

void GLC_StateBeginAliasPowerupShell(void)
{
	if (gl_powerupshells_style.integer) {
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else {
		GL_BlendFunc(GL_ONE, GL_ONE);
	}
}

void GLC_StateEndAliasPowerupShell(void)
{
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
}

void GLC_StateEndUnderwaterCaustics(void)
{
	LEAVE_STATE;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	GL_TextureEnvModeForUnit(GL_TEXTURE1, GL_REPLACE);
	GLC_EnsureTMUDisabled(GL_TEXTURE1);

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
}

void GLC_StateBeginMD3Draw(float alpha)
{
	ENTER_STATE;

	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	}
	GL_EnableFog();
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
}

void GLC_StateEndMD3Draw(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	glColor4f(1, 1, 1, 1);
	GL_ShadeModel(GL_FLAT);
	GL_DisableFog();
}

void GLC_StateBeginFastSky(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}
	GLC_DisableAllTexturing();
	glColor3ubv(r_skycolor.color);
}

void GLC_StateEndFastSky(void)
{
	LEAVE_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Color4ubv(color_white);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}
}

void GLC_StateBeginSky(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	glDisable(GL_DEPTH_TEST);
}

void GLC_StateBeginSkyZBufferPass(void)
{
	ENTER_STATE;

	if (gl_fogenable.value && gl_fogsky.value) {
		glEnable(GL_DEPTH_TEST);
		GL_EnableFog();
		glColor4f(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
		GL_BlendFunc(GL_ONE, GL_ZERO);
	}
	else {
		glEnable(GL_DEPTH_TEST);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		GL_BlendFunc(GL_ZERO, GL_ONE);
	}
	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
}

void GLC_StateEndSkyZBufferPass(void)
{
	LEAVE_STATE;

	// FIXME: GL_ResetState()
	if (gl_fogenable.value && gl_fogsky.value) {
		GL_DisableFog();
	}
	else {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
}

void GLC_StateEndSkyNoZBufferPass(void)
{
	LEAVE_STATE;

	glEnable(GL_DEPTH_TEST);
}

void GLC_StateBeginSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnits1(solidskytexture, GL_REPLACE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
}

void GLC_StateBeginSkyDomeCloudPass(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, alphaskytexture);
}

void GLC_StateBeginMultiTextureSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnits2(solidskytexture, GL_REPLACE, alphaskytexture, GL_DECAL);

	LEAVE_STATE;
}

void GLC_StateEndMultiTextureSkyDome(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginMultiTextureSkyChain(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}

	GLC_InitTextureUnits2(solidskytexture, GL_MODULATE, alphaskytexture, GL_DECAL);
}

void GLC_StateEndMultiTextureSkyChain(void)
{
	LEAVE_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}
}

void GLC_StateBeginSingleTextureSkyPass(void)
{
	ENTER_STATE;

	if (gl_fogsky.value) {
		GL_EnableFog();
	}
	GLC_InitTextureUnits1(solidskytexture, GL_REPLACE);
}

void GLC_StateBeginSingleTextureCloudPass(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, alphaskytexture);
}

void GLC_StateEndSingleTextureSky(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	if (gl_fogsky.value) {
		GL_DisableFog();
	}
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

void GLM_StateBeginDrawBillboards(void)
{
	ENTER_STATE;

	GL_DisableFog();
	GL_DepthMask(GL_FALSE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED | GL_ALPHATEST_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
	GL_ShadeModel(GL_SMOOTH);
}

void GLM_StateEndDrawBillboards(void)
{
	ENTER_STATE;

	GL_DepthMask(GL_TRUE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_EnableFog();

	LEAVE_STATE;
}

void GLM_StateBeginDrawWorldOutlines(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_CullFace(GL_BACK);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	GL_Disable(GL_DEPTH_TEST);
	GL_Disable(GL_CULL_FACE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	LEAVE_STATE;
}

void GLM_StateEndDrawWorldOutlines(void)
{
	ENTER_STATE;

	GL_Enable(GL_DEPTH_TEST);
	GL_Enable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_CullFace(GL_FRONT);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);

	LEAVE_STATE;
}

void GLC_StateBeginDrawAliasFrame(GLenum textureEnvMode, texture_ref texture, texture_ref fb_texture, qbool mtex, float alpha, struct custom_model_color_s* custom_model, qbool shells_only)
{
	ENTER_STATE;

	if (shells_only || alpha < 1) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	}
	else {
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	}

	if (custom_model) {
		GLC_DisableAllTexturing();
		GL_Color4ub(custom_model->color_cvar.color[0], custom_model->color_cvar.color[1], custom_model->color_cvar.color[2], alpha * 255);
	}
	else {
		if (GL_TextureReferenceIsValid(texture) && GL_TextureReferenceIsValid(fb_texture) && mtex) {
			GLC_InitTextureUnits2(texture, textureEnvMode, fb_texture, GL_DECAL);
		}
		else if (GL_TextureReferenceIsValid(texture)) {
			GLC_InitTextureUnits1(texture, textureEnvMode);
		}
		else if (shells_only) {
			GLC_InitTextureUnits1(shelltexture, GL_MODULATE);
		}
		else {
			GLC_DisableAllTexturing();
		}
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawAliasFrame(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginAliasModelShadow(void)
{
	ENTER_STATE;

	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	glColor4f(0, 0, 0, 0.5);

	LEAVE_STATE;
}

void GLC_StateEndAliasModelShadow(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	LEAVE_STATE;
}

void GLC_StateBeginDrawFlatModel(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_BLEND);

	// START shaman BUG /fog not working with /r_drawflat {
	if (gl_fogenable.value) {
		glEnable(GL_FOG);
	}
	// } END shaman BUG /fog not working with /r_drawflat

	LEAVE_STATE;
}

void GLC_StateEndDrawFlatModel(void)
{
	ENTER_STATE;

	if (gl_fogenable.value) {
		glDisable(GL_FOG);
	}

	GL_Color4ubv(color_white);

	LEAVE_STATE;
}

void GLC_StateBeginDrawTextureChains(GLenum lightmapTextureUnit, GLenum fullbrightTextureUnit, GLenum fullbrightMode)
{
	ENTER_STATE;

	if (gl_fogenable.value) {
		glEnable(GL_FOG);
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

	if (gl_fogenable.value) {
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
}

void GLC_StateEndFastTurbPoly(void)
{
	LEAVE_STATE;

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_DepthMask(GL_TRUE);
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	GL_DisableFog();
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Color4ubv(color_white);
	// END shaman RFE 1022504
}

void GLC_StateBeginTurbPoly(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_Enable(GL_TEXTURE_2D);
	GL_EnableFog();
}

void GLC_StateEndTurbPoly(void)
{
	LEAVE_STATE;

	GL_DisableFog();
}

void GLC_StateBeginBlendLightmaps(void)
{
	extern qbool gl_invlightmaps;

	ENTER_STATE;

	GL_DepthMask(GL_FALSE);		// don't bother writing Z
	if (gl_invlightmaps) {
		GL_BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	}
	else {
		GL_BlendFunc(GL_ZERO, GL_SRC_COLOR);
	}

	if (!(r_lightmap.value && r_refdef2.allow_cheats)) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	}
}

void GLC_StateEndBlendLightmaps(void)
{
	LEAVE_STATE;

	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_DepthMask(GL_TRUE);		// back to normal Z buffering
}

void GLC_StateBeginCausticsPolys(void)
{
	ENTER_STATE;

	GLC_InitTextureUnitsNoBind1(GL_DECAL);
	GL_BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
}

void GLC_StateEndCausticsPolys(void)
{
	LEAVE_STATE;

	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
}

void GL_StateBeginDrawViewModel(float alpha)
{
	ENTER_STATE;

	// hack the depth range to prevent view model from poking into walls
	GL_DepthRange(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	if (gl_mtexable) {
		if (alpha < 1) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	}

	LEAVE_STATE;
}

void GL_StateEndDrawViewModel(void)
{
	ENTER_STATE;

	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DepthRange(gldepthmin, gldepthmax);

	LEAVE_STATE;
}

void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset)
{
	GL_EnterTracedRegion(va("GL_StateBeginDrawBrushModel(%s)", e->model->name), true);

	if (e->alpha) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_TextureEnvMode(GL_MODULATE);
		glColor4f (1, 1, 1, e->alpha);
	}
	else {
		glColor3f (1,1,1);
	}

	if (!GL_ShadersSupported() && polygonOffset) {
		GL_PolygonOffset(POLYGONOFFSET_STANDARD);
	}
}

void GL_StateEndDrawBrushModel(void)
{
	LEAVE_STATE;

	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
}

void GL_StateDefaultInit(void)
{
#ifndef __APPLE__
	glClearColor(1, 0, 0, 0);
#else
	glClearColor(0.2, 0.2, 0.2, 1.0);
#endif

	GL_CullFace(GL_FRONT);
	if (!GL_ShadersSupported()) {
		glEnable(GL_TEXTURE_2D);
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	GL_AlphaFunc(GL_GREATER, 0.666);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_ShadeModel(GL_FLAT);

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TextureEnvMode(GL_REPLACE);
}

void GLC_StateBeginDrawBillboards(void)
{
	ENTER_STATE;

	GL_DisableFog();
	GL_DepthMask(GL_FALSE);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED | GL_ALPHATEST_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_ShadeModel(GL_SMOOTH);
}

void GLC_StateEndDrawBillboards(void)
{
	LEAVE_STATE;

	GL_DepthMask(GL_TRUE);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TextureEnvMode(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_EnableFog();
}

void GL_StateBeginDrawAliasModel(void)
{
	ENTER_STATE;

	GL_EnableFog();
}

void GL_StateEndDrawAliasModel(void)
{
	LEAVE_STATE;

	glColor3ubv(color_white);
	GL_DisableFog();
}

void GLM_StateBeginDrawAliasOutlines(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_CullFace(GL_BACK);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));
}

void GLM_StateEndDrawAliasOutlines(void)
{
	LEAVE_STATE;

	GL_CullFace(GL_FRONT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
}

void GL_StateEndFrame(void)
{
	if (developer.integer) {
		Cvar_SetValue(&developer, 0);
	}
}

void GLC_StateEndRenderScene(void)
{
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);

	GL_ConfigureFog();
}
