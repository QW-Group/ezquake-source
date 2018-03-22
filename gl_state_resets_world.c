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
#include "r_local.h"
#include "r_state.h"
#include "glc_vao.h"

static rendering_state_t worldTextureChainState;
static rendering_state_t worldTextureChainFullbrightState;
static rendering_state_t blendLightmapState;
static rendering_state_t causticsState;
static rendering_state_t aliasModelCausticsState;
static rendering_state_t fastWaterSurfacesState;
static rendering_state_t fastTranslucentWaterSurfacesState;
static rendering_state_t waterSurfacesState;
static rendering_state_t translucentWaterSurfacesState;
static rendering_state_t alphaChainState;
static rendering_state_t fullbrightsState;
static rendering_state_t lumasState;
static rendering_state_t detailPolyState;
static rendering_state_t mapOutlineState;
static rendering_state_t glmAlphaOffsetWorldState;
static rendering_state_t glmOffsetWorldState;
static rendering_state_t glmAlphaWorldState;
static rendering_state_t glmWorldState;

void R_InitialiseWorldStates(void)
{
	R_InitRenderingState(&worldTextureChainState, true, "worldTextureChainState", vao_brushmodel);
	worldTextureChainState.fog.enabled = true;
	worldTextureChainState.textureUnits[0].enabled = true;
	worldTextureChainState.textureUnits[0].mode = r_texunit_mode_replace;
	worldTextureChainState.textureUnits[1].enabled = true;
	//worldTextureChainState.textureUnits[1].mode = (gl_invlightmaps ? r_texunit_mode_blend : r_texunit_mode_modulate);
	worldTextureChainState.textureUnits[1].mode = r_texunit_mode_blend;

	R_InitRenderingState(&worldTextureChainFullbrightState, true, "worldTextureChainFullbrightState", vao_brushmodel);
	worldTextureChainFullbrightState.fog.enabled = true;
	worldTextureChainFullbrightState.textureUnits[0].enabled = true;
	worldTextureChainFullbrightState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&blendLightmapState, true, "blendLightmapState", vao_brushmodel_lightmaps);
	blendLightmapState.depth.mask_enabled = false;
	blendLightmapState.depth.func = r_depthfunc_equal;
	blendLightmapState.blendingEnabled = true;
	blendLightmapState.blendFunc = r_blendfunc_src_zero_dest_one_minus_src_color;
	blendLightmapState.textureUnits[0].enabled = true;
	blendLightmapState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&causticsState, true, "causticsState", vao_brushmodel);
	causticsState.textureUnits[0].enabled = true;
	causticsState.textureUnits[0].mode = r_texunit_mode_decal;
	causticsState.blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	causticsState.blendingEnabled = true;

	R_InitRenderingState(&aliasModelCausticsState, true, "aliasModelCausticsState", vao_aliasmodel);
	aliasModelCausticsState.blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	aliasModelCausticsState.blendingEnabled = true;
	aliasModelCausticsState.textureUnits[0].enabled = true;
	aliasModelCausticsState.textureUnits[0].mode = r_texunit_mode_decal;

	R_InitRenderingState(&fastWaterSurfacesState, true, "fastWaterSurfacesState", vao_brushmodel);
	fastWaterSurfacesState.depth.test_enabled = true;
	fastWaterSurfacesState.fog.enabled = true;

	R_CopyRenderingState(&waterSurfacesState, &fastWaterSurfacesState, "waterSurfacesState");
	waterSurfacesState.cullface.enabled = false;
	waterSurfacesState.textureUnits[0].enabled = true;
	waterSurfacesState.textureUnits[0].mode = r_texunit_mode_replace;

	R_CopyRenderingState(&translucentWaterSurfacesState, &fastWaterSurfacesState, "translucentWaterSurfacesState");
	translucentWaterSurfacesState.depth.mask_enabled = false; // FIXME: water-alpha < 0.9 only?
	translucentWaterSurfacesState.textureUnits[0].enabled = true;
	translucentWaterSurfacesState.textureUnits[0].mode = r_texunit_mode_modulate;

	R_InitRenderingState(&alphaChainState, true, "alphaChainState", vao_brushmodel);
	alphaChainState.alphaTesting.enabled = true;
	alphaChainState.alphaTesting.func = r_alphatest_func_greater;
	alphaChainState.alphaTesting.value = 0.333f;
	alphaChainState.blendFunc = r_blendfunc_premultiplied_alpha;
	alphaChainState.textureUnits[0].enabled = true;
	alphaChainState.textureUnits[0].mode = r_texunit_mode_replace;
	if (gl_mtexable) {
		alphaChainState.textureUnits[1].enabled = true;
		alphaChainState.textureUnits[1].mode = r_texunit_mode_blend; // modulate if !inv_lmaps
	}

	R_InitRenderingState(&fullbrightsState, true, "fullbrightsState", vao_brushmodel);
	fullbrightsState.depth.mask_enabled = false;
	fullbrightsState.alphaTesting.enabled = true;
	fullbrightsState.textureUnits[0].enabled = true;
	fullbrightsState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&lumasState, true, "lumasState", vao_brushmodel);
	lumasState.depth.mask_enabled = false;
	lumasState.blendingEnabled = true;
	lumasState.blendFunc = r_blendfunc_additive_blending;
	lumasState.textureUnits[0].enabled = true;
	lumasState.textureUnits[0].mode = r_texunit_mode_replace;

	R_InitRenderingState(&detailPolyState, true, "detailPolyState", vao_brushmodel_details);
	detailPolyState.textureUnits[0].enabled = true;
	detailPolyState.textureUnits[0].mode = r_texunit_mode_decal;
	detailPolyState.blendingEnabled = true;
	detailPolyState.blendFunc = r_blendfunc_src_dst_color_dest_src_color;

	R_InitRenderingState(&mapOutlineState, true, "mapOutlineState", vao_brushmodel);
	mapOutlineState.polygonOffset.option = r_polygonoffset_outlines;
	mapOutlineState.depth.mask_enabled = false;
	//mapOutlineState.depth.test_enabled = false;
	mapOutlineState.cullface.enabled = false;

	R_InitRenderingState(&glmAlphaOffsetWorldState, true, "glmAlphaOffsetWorldState", vao_brushmodel);
	glmAlphaOffsetWorldState.blendingEnabled = true;
	glmAlphaOffsetWorldState.blendFunc = r_blendfunc_premultiplied_alpha;

	R_InitRenderingState(&glmOffsetWorldState, true, "glmOffsetWorldState", vao_brushmodel);
	glmOffsetWorldState.polygonOffset.option = r_polygonoffset_standard;

	R_InitRenderingState(&glmAlphaWorldState, true, "glmAlphaWorldState", vao_brushmodel);
	glmAlphaWorldState.blendingEnabled = true;
	glmAlphaWorldState.blendFunc = r_blendfunc_premultiplied_alpha;
	glmAlphaWorldState.polygonOffset.option = r_polygonoffset_standard;

	R_InitRenderingState(&glmWorldState, true, "glmWorldState", vao_brushmodel);

	//if (fullbrightTextureUnit) {
	//		GL_TextureEnvModeForUnit(fullbrightTextureUnit, fullbrightMode);
	//	}
}

void GLC_StateBeginDrawFlatModel(void)
{
}

void GLC_StateEndDrawFlatModel(void)
{
}

void GLC_StateBeginDrawTextureChains(void)
{
}

void GLC_StateEndWorldTextureChains(void)
{
}

void GLC_StateEndDrawTextureChains(void)
{
}

void GLC_StateBeginFastTurbPoly(byte color[4])
{
	float wateralpha = GL_WaterAlpha();
	wateralpha = bound(0, wateralpha, 1);

	ENTER_STATE;

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	R_CustomColor(color[0] * wateralpha / 255.0f, color[1] * wateralpha / 255.0f, color[2] * wateralpha / 255.0f, wateralpha);
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	LEAVE_STATE;
}

void GLC_StateEndFastTurbPoly(void)
{
}

void GLC_StateBeginBlendLightmaps(qbool use_buffers)
{
	ENTER_STATE;

	R_ApplyRenderingState(&blendLightmapState);

	LEAVE_STATE;
}

void GLC_StateEndBlendLightmaps(void)
{
}

void GLC_StateBeginCausticsPolys(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&causticsState);

	LEAVE_STATE;
}

void GLC_StateEndCausticsPolys(void)
{
}

// Alias models
void GLC_StateBeginUnderwaterCaustics(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&aliasModelCausticsState);
	GLC_BeginCausticsTextureMatrix();

	LEAVE_STATE;
}

void GLC_StateEndUnderwaterCaustics(void)
{
	ENTER_STATE;

	GLC_EndCausticsTextureMatrix();

	LEAVE_STATE;
}

void GLC_StateBeginWaterSurfaces(void)
{
	extern cvar_t r_fastturb;
	float wateralpha = GL_WaterAlpha();

	ENTER_STATE;

	if (r_fastturb.integer) {
		if (wateralpha < 1.0) {
			R_ApplyRenderingState(&fastTranslucentWaterSurfacesState);
			R_CustomColor(wateralpha, wateralpha, wateralpha, wateralpha);
		}
		else {
			R_ApplyRenderingState(&fastWaterSurfacesState);
		}
	}
	else {
		if (wateralpha < 1.0) {
			R_ApplyRenderingState(&translucentWaterSurfacesState);
			R_CustomColor(wateralpha, wateralpha, wateralpha, wateralpha);
		}
		else {
			R_ApplyRenderingState(&waterSurfacesState);
		}
	}

	LEAVE_STATE;
}

void GLC_StateEndWaterSurfaces(void)
{
}

void GLC_StateBeginAlphaChain(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&alphaChainState);

	LEAVE_STATE;
}

void GLC_StateEndAlphaChain(void)
{
}

void GLC_StateBeginAlphaChainSurface(msurface_t* s)
{
	texture_t* t = s->texinfo->texture;

	ENTER_STATE;

	//bind the world texture
	GL_EnsureTextureUnitBound(GL_TEXTURE0, t->gl_texturenum);
	if (gl_mtexable) {
		GL_EnsureTextureUnitBound(GL_TEXTURE1, GLC_LightmapTexture(s->lightmaptexturenum));
	}

	LEAVE_STATE;
}

void GLC_StateBeginRenderFullbrights(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&fullbrightsState);

	LEAVE_STATE;
}

void GLC_StateEndRenderFullbrights(void)
{
}

void GLC_StateBeginRenderLumas(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&lumasState);

	LEAVE_STATE;
}

void GLC_StateEndRenderLumas(void)
{
}

void GLC_StateBeginEmitDetailPolys(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&detailPolyState);

	LEAVE_STATE;
}

void GLC_StateEndEmitDetailPolys(void)
{
}

void GLC_StateBeginDrawMapOutline(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	R_ApplyRenderingState(&mapOutlineState);
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	LEAVE_STATE;
}

void GLC_StateBeginEndMapOutline(void)
{
}

void GLM_StateBeginDrawWorldOutlines(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	// FIXME: This was different for GLC & GLM, why?  // disable depth-test
	R_ApplyRenderingState(&mapOutlineState);
	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	LEAVE_STATE;
}

void GLM_StateEndDrawWorldOutlines(void)
{
}

void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset)
{
	if (alpha_surfaces && polygon_offset) {
		R_ApplyRenderingState(&glmAlphaOffsetWorldState);
	}
	else if (alpha_surfaces) {
		R_ApplyRenderingState(&glmAlphaWorldState);
	}
	else if (polygon_offset) {
		R_ApplyRenderingState(&glmOffsetWorldState);
	}
	else {
		R_ApplyRenderingState(&glmWorldState);
	}
}
