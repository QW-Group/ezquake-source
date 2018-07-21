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
#include "r_state.h"
#include "r_trace.h"
#include "r_matrix.h"
#include "glc_matrix.h"
#include "tr_types.h"

extern texture_ref solidskytexture, alphaskytexture;
static int currentViewportX, currentViewportY;
static int currentViewportWidth, currentViewportHeight;

void R_InitialiseWorldStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_world_texture_chain, true, "worldTextureChainState", vao_brushmodel);
	state->fog.enabled = true;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;
	state->textureUnits[1].enabled = true;
	state->textureUnits[1].mode = r_texunit_mode_blend;

	state = R_InitRenderingState(r_state_world_texture_chain_fullbright, true, "worldTextureChainFullbrightState", vao_brushmodel);
	state->fog.enabled = true;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_world_blend_lightmaps, true, "blendLightmapState", vao_brushmodel_lightmap_pass);
	state->depth.mask_enabled = false;
	state->depth.func = r_depthfunc_equal;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_zero_dest_one_minus_src_color;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_world_caustics, true, "causticsState", vao_brushmodel);
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_decal;
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	state->blendingEnabled = true;

	state = R_InitRenderingState(r_state_aliasmodel_caustics, true, "aliasModelCausticsState", vao_aliasmodel);
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	state->blendingEnabled = true;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_decal;

	state = R_InitRenderingState(r_state_world_fast_opaque_water, true, "fastWaterSurfacesState", vao_brushmodel);
	state->depth.test_enabled = true;
	state->fog.enabled = true;

	state = R_CopyRenderingState(r_state_world_opaque_water, r_state_world_fast_opaque_water, "waterSurfacesState");
	state->cullface.enabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_CopyRenderingState(r_state_world_translucent_water, r_state_world_fast_opaque_water, "translucentWaterSurfacesState");
	state->depth.mask_enabled = false; // FIXME: water-alpha < 0.9 only?
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_modulate;

	state = R_InitRenderingState(r_state_world_alpha_surfaces, true, "alphaChainState", vao_brushmodel);
	state->alphaTesting.enabled = true;
	state->alphaTesting.func = r_alphatest_func_greater;
	state->alphaTesting.value = 0.333f;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;
	if (glConfig.texture_units >= 2) {
		state->textureUnits[1].enabled = true;
		state->textureUnits[1].mode = r_texunit_mode_blend; // modulate if !inv_lmaps
	}

	state = R_InitRenderingState(r_state_world_fullbrights, true, "fullbrightsState", vao_brushmodel);
	state->depth.mask_enabled = false;
	state->alphaTesting.enabled = true;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_world_lumas, true, "lumasState", vao_brushmodel);
	state->depth.mask_enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_world_details, true, "detailPolyState", vao_brushmodel_details);
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_decal;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;

	state = R_InitRenderingState(r_state_world_outline, true, "mapOutlineState", vao_brushmodel);
	state->polygonOffset.option = r_polygonoffset_outlines;
	state->depth.mask_enabled = false;
	//mapOutlineState.depth.test_enabled = false;
	state->cullface.enabled = false;

	state = R_InitRenderingState(r_state_alpha_surfaces_offset_glm, true, "glmAlphaOffsetWorldState", vao_brushmodel);
	state->polygonOffset.option = r_polygonoffset_standard;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_InitRenderingState(r_state_opaque_surfaces_offset_glm, true, "glmOffsetWorldState", vao_brushmodel);
	state->polygonOffset.option = r_polygonoffset_standard;

	state = R_InitRenderingState(r_state_alpha_surfaces_glm, true, "glmAlphaWorldState", vao_brushmodel);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->polygonOffset.option = r_polygonoffset_standard;

	R_InitRenderingState(r_state_opaque_surfaces_glm, true, "glmWorldState", vao_brushmodel);
}

void GLM_StateBeginDrawWorldOutlines(void)
{
	extern cvar_t gl_outline_width;

	R_TraceEnterFunctionRegion;

	// FIXME: This was different for GLC & GLM, why?  // disable depth-test
	R_ApplyRenderingState(r_state_world_outline);
	// limit outline width, since even width == 3 can be considered as cheat.
	R_CustomLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	R_TraceLeaveFunctionRegion;
}

void GLM_BeginDrawWorld(qbool alpha_surfaces, qbool polygon_offset)
{
	if (alpha_surfaces && polygon_offset) {
		R_ApplyRenderingState(r_state_alpha_surfaces_offset_glm);
	}
	else if (alpha_surfaces) {
		R_ApplyRenderingState(r_state_alpha_surfaces_glm);
	}
	else if (polygon_offset) {
		R_ApplyRenderingState(r_state_opaque_surfaces_offset_glm);
	}
	else {
		R_ApplyRenderingState(r_state_opaque_surfaces_glm);
	}
}

void GLC_InitialiseSkyStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_sky_fast, true, "fastSkyState", vao_brushmodel);
	state->depth.test_enabled = false;

	state = R_InitRenderingState(r_state_sky_fast_fogged, true, "fastSkyStateFogged", vao_brushmodel);
	state->depth.test_enabled = false;
	state->fog.enabled = true;

	state = R_InitRenderingState(r_state_skydome_zbuffer_pass, true, "skyDomeZPassState", vao_none);
	state->depth.test_enabled = true;
	state->blendingEnabled = true;
	state->fog.enabled = false;
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = false;
	state->blendFunc = r_blendfunc_src_zero_dest_one;

	state = R_InitRenderingState(r_state_skydome_zbuffer_pass_fogged, true, "skyDomeZPassFoggedState", vao_none);
	state->depth.test_enabled = true;
	state->blendingEnabled = true;
	state->fog.enabled = true;
	state->blendFunc = r_blendfunc_src_one_dest_zero;

	state = R_InitRenderingState(r_state_skydome_background_pass, true, "skyDomeFirstPassState", vao_none);
	state->depth.test_enabled = false;
	state->blendingEnabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_skydome_cloud_pass, true, "skyDomeCloudPassState", vao_none);
	state->depth.test_enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_skydome_single_pass, true, "skyDomeSinglePassState", vao_none);
	state->depth.test_enabled = false;
	state->blendingEnabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;
	state->textureUnits[1].enabled = true;
	state->textureUnits[1].mode = r_texunit_mode_decal;
}

void GLC_StateBeginFastSky(void)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor;

	R_TraceEnterFunctionRegion;

	if (gl_fogsky.integer && gl_fogenable.integer) {
		R_ApplyRenderingState(r_state_sky_fast_fogged);
	}
	else {
		R_ApplyRenderingState(r_state_sky_fast);
	}
	R_CustomColor(r_skycolor.color[0] / 255.0f, r_skycolor.color[1] / 255.0f, r_skycolor.color[2] / 255.0f, 1.0f);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSkyZBufferPass(void)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor, gl_fogred, gl_foggreen, gl_fogblue;

	R_TraceEnterFunctionRegion;

	if (gl_fogenable.integer && gl_fogsky.integer) {
		R_ApplyRenderingState(r_state_skydome_zbuffer_pass_fogged);
		R_CustomColor(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
	}
	else {
		R_ApplyRenderingState(r_state_skydome_zbuffer_pass);
	}

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyDome(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_background_pass);
	R_TextureUnitBind(0, solidskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyDomeCloudPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_cloud_pass);
	R_TextureUnitBind(0, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginMultiTextureSkyDome(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_single_pass);
	R_TextureUnitBind(0, solidskytexture);
	R_TextureUnitBind(1, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginMultiTextureSkyChain(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_single_pass);
	R_TextureUnitBind(0, solidskytexture);
	R_TextureUnitBind(1, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_background_pass);
	R_TextureUnitBind(0, solidskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureCloudPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_cloud_pass);
	R_TextureUnitBind(0, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void R_InitialiseStates(void)
{
	R_InitRenderingState(r_state_default_3d, true, "default3DState", vao_none);
}

float R_WaterAlpha(void)
{
	extern cvar_t r_wateralpha;

	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void R_StateDefault3D(void)
{
	R_TraceResetRegion(false);

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_PauseMatrixUpdate();
	}
#endif
	R_IdentityModelView();
	R_RotateModelview(-90, 1, 0, 0);	    // put Z going up
	R_RotateModelview(90, 0, 0, 1);	    // put Z going up
	R_RotateModelview(-r_refdef.viewangles[2], 1, 0, 0);
	R_RotateModelview(-r_refdef.viewangles[0], 0, 1, 0);
	R_RotateModelview(-r_refdef.viewangles[1], 0, 0, 1);
	R_TranslateModelview(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_ResumeMatrixUpdate();
		GLC_LoadModelviewMatrix();
	}
#endif

	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_default_3d);

	R_TraceLeaveFunctionRegion;
}

void GLM_StateBeginDraw3DSprites(void)
{
}

void R_Initialise2DStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_default_2d, true, "default2DState", vao_none);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;

	state = R_InitRenderingState(r_state_brighten_screen, true, "brightenScreenState", vao_postprocess);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->alphaTesting.enabled = true; // really?
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_dst_color_dest_one;

	state = R_InitRenderingState(r_state_line, true, "lineState", vao_hud_lines);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->line.flexible_width = true;

	state = R_InitRenderingState(r_state_scene_blur, true, "sceneBlurState", vao_postprocess);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->alphaTesting.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_InitRenderingState(r_state_hud_images_glc, true, "glcImageDrawState", vao_hud_images);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_modulate;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_CopyRenderingState(r_state_hud_images_alphatested_glc, r_state_hud_images_glc, "glcAlphaTestedImageDrawState");
	state->alphaTesting.enabled = true;

#ifdef BLOOM_SUPPORTED
	R_InitRenderingState(&glcBloomState, true, "glcBloomState", vao_postprocess);
	glcBloomState.depth.test_enabled = false;
	glcBloomState.cullface.enabled = false;
	glcBloomState.alphaTesting.enabled = true;
	glcBloomState.blendingEnabled = true;
	glcBloomState.blendFunc = r_blendfunc_additive_blending;
	glcBloomState.color[0] = glcBloomState.color[1] = glcBloomState.color[2] = r_bloom_alpha.value;
	glcBloomState.color[3] = 1.0f;
	glcBloomState.textureUnits[0].enabled = true;
	glcBloomState.textureUnits[0].mode = r_texunit_mode_modulate;
#endif

	state = R_InitRenderingState(r_state_poly_blend, true, "polyBlendState", vao_postprocess);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->color[0] = v_blend[0] * v_blend[3];
	state->color[1] = v_blend[1] * v_blend[3];
	state->color[2] = v_blend[2] * v_blend[3];
	state->color[3] = v_blend[3];

	state = R_InitRenderingState(r_state_hud_images_glm, true, "glmImageDrawState", vao_hud_images);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->alphaTesting.enabled = false;
	state->blendingEnabled = r_blendfunc_premultiplied_alpha;
}

void GLC_StateBeginBrightenScreen(void)
{
	R_ApplyRenderingState(r_state_brighten_screen);
}

void R_StateBeginAlphaLineRGB(float thickness)
{
	R_ApplyRenderingState(r_state_line);
	if (thickness > 0.0) {
		R_CustomLineWidth(thickness);
	}
}

void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness)
{
	// Same as lineState
	R_ApplyRenderingState(r_state_line);
	if (thickness > 0.0) {
		R_CustomLineWidth(thickness);
	}
}

void GLC_StateBeginSceneBlur(void)
{
	R_ApplyRenderingState(r_state_scene_blur);

	R_IdentityModelView();
	R_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
}

void GLC_StateBeginDrawPolygon(void)
{
	R_ApplyRenderingState(r_state_line);
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	R_ApplyRenderingState(r_state_postprocess_bloom);
	R_TextureUnitBind(0, texture);
}

void GLC_StateBeginPolyBlend(float v_blend[4])
{
	R_ApplyRenderingState(r_state_poly_blend);
}

void GLC_StateBeginImageDraw(qbool is_text)
{
	extern cvar_t gl_alphafont;

	if (is_text && !gl_alphafont.integer) {
		R_ApplyRenderingState(r_state_hud_images_alphatested_glc);
	}
	else {
		R_ApplyRenderingState(r_state_hud_images_glc);
	}
}

void GLM_StateBeginPolyBlend(void)
{
	R_ApplyRenderingState(r_state_poly_blend);
}

void GLM_StateBeginImageDraw(void)
{
	R_ApplyRenderingState(r_state_hud_images_glm);
}

void GLM_StateBeginPolygonDraw(void)
{
	R_ApplyRenderingState(r_state_hud_images_glm);
}

void R_InitialiseEntityStates(void)
{
	extern cvar_t gl_outline_width;
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_aliasmodel_powerupshell, true, "powerupShellState", vao_none);
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	state->alphaTesting.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;

	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_modulate;

	state = R_InitRenderingState(r_state_aliasmodel_notexture_opaque, true, "opaqueAliasModelNoTexture", vao_aliasmodel);
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = state->alphaTesting.enabled = false;
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	state->polygonMode = r_polygonmode_fill;
	state->line.smooth = false;
	state->fog.enabled = true;

	state = R_CopyRenderingState(r_state_aliasmodel_singletexture_opaque, r_state_aliasmodel_notexture_opaque, "opaqueAliasModelSingleTex");
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_modulate;

	state = R_CopyRenderingState(r_state_aliasmodel_multitexture_opaque, r_state_aliasmodel_notexture_opaque, "opaqueAliasModelMultiTex");
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_modulate;
	state->textureUnits[1].enabled = true;
	state->textureUnits[1].mode = r_texunit_mode_decal;

	state = R_CopyRenderingState(r_state_aliasmodel_notexture_transparent, r_state_aliasmodel_notexture_opaque, "transparentAliasModelNoTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state = R_CopyRenderingState(r_state_aliasmodel_singletexture_transparent, r_state_aliasmodel_singletexture_opaque, "transparentAliasModelSingleTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state = R_CopyRenderingState(r_state_aliasmodel_multitexture_transparent, r_state_aliasmodel_multitexture_opaque, "transparentAliasModelMultiTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_CopyRenderingState(r_state_weaponmodel_singletexture_opaque, r_state_aliasmodel_singletexture_opaque, "weaponModelSingleOpaque");
	state->depth.farRange = 0.3f;
	state = R_CopyRenderingState(r_state_weaponmodel_singletexture_transparent, r_state_aliasmodel_singletexture_transparent, "weaponModelSingleTransparent");
	state->depth.farRange = 0.3f;
	state = R_CopyRenderingState(r_state_weaponmodel_multitexture_opaque, r_state_weaponmodel_singletexture_opaque, "weaponModelMultiOpaque");
	state->depth.farRange = 0.3f;
	state = R_CopyRenderingState(r_state_weaponmodel_multitexture_transparent, r_state_weaponmodel_singletexture_transparent, "weaponModelMultiOpaque");
	state->depth.farRange = 0.3f;

	state = R_InitRenderingState(r_state_aliasmodel_shadows, true, "aliasModelShadowState", vao_aliasmodel);
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	state->polygonMode = r_polygonmode_fill;
	state->line.smooth = false;
	state->fog.enabled = false;
	state->alphaTesting.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->color[0] = state->color[1] = state->color[2] = 0;
	state->color[3] = 0.5f;

	state = R_InitRenderingState(r_state_aliasmodel_outline, true, "aliasModelOutlineState", vao_aliasmodel);
	state->alphaTesting.enabled = false;
	state->blendingEnabled = false;
	state->fog.enabled = false;
	state->polygonOffset.option = r_polygonoffset_outlines;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_back;
	state->polygonMode = r_polygonmode_line;
	// limit outline width, since even width == 3 can be considered as cheat.
	state->line.width = bound(0.1, gl_outline_width.value, 3.0);
	state->line.flexible_width = true;
	state->line.smooth = true;
	state->color[0] = state->color[1] = state->color[2] = 0;

	state = R_InitRenderingState(r_state_brushmodel_opaque, true, "brushModelOpaqueState", vao_brushmodel);
	state->cullface.mode = r_cullface_front;
	state->cullface.enabled = true;
	state->polygonMode = r_polygonmode_fill;
	state->alphaTesting.enabled = false;
	state->blendingEnabled = false;
	state->line.smooth = false;
	state->fog.enabled = false;
	state->polygonOffset.option = r_polygonoffset_disabled;

	state = R_CopyRenderingState(r_state_brushmodel_opaque_offset, r_state_brushmodel_opaque, "brushModelOpaqueOffsetState");
	state->polygonOffset.option = r_polygonoffset_standard;

	state = R_CopyRenderingState(r_state_brushmodel_translucent, r_state_brushmodel_opaque, "brushModelTranslucentState");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_CopyRenderingState(r_state_brushmodel_translucent_offset, r_state_brushmodel_translucent, "brushModelTranslucentOffsetState");
	state->polygonOffset.option = r_polygonoffset_standard;

	state = R_InitRenderingState(r_state_aliasmodel_opaque_batch, true, "aliasModelBatchState", vao_aliasmodel);
	state->cullface.mode = r_cullface_front;
	state->cullface.enabled = true;
	state->depth.mask_enabled = true;
	state->depth.test_enabled = true;
	state->blendingEnabled = false;

	state = R_CopyRenderingState(r_state_aliasmodel_translucent_batch, r_state_aliasmodel_opaque_batch, "aliasModelTranslucentBatchState");
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = true;
}

void R_StateBrushModelBeginDraw(entity_t* e, qbool polygonOffset)
{
	static r_state_id brushModelStates[] = {
		r_state_brushmodel_opaque,
		r_state_brushmodel_opaque_offset,
		r_state_brushmodel_translucent,
		r_state_brushmodel_translucent_offset
	};

	R_TraceEnterRegion("R_StateBrushModelBeginDraw", true);
	R_RotateForEntity(e);
	R_ApplyRenderingState(brushModelStates[(e->alpha ? 2 : 0) + (polygonOffset ? 1 : 0)]);
	if (e->alpha) {
		R_CustomColor(e->alpha, e->alpha, e->alpha, e->alpha);
	}

	R_TraceLeaveFunctionRegion;
}

void R_StateBeginDrawAliasModel(entity_t* ent, aliashdr_t* paliashdr)
{
	extern cvar_t r_viewmodelsize;

	R_TraceEnterRegion(__FUNCTION__, true);

	R_RotateForEntity(ent);
	if (ent->model->modhint == MOD_EYES) {
		R_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		R_ScaleModelview(paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->renderfx & RF_WEAPONMODEL) {
		float scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;

		R_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		R_ScaleModelview(paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else {
		R_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		R_ScaleModelview(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginAliasOutlineFrame(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_outline);
}

void GLM_StateBeginAliasOutlineBatch(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_outline);
}

void GLM_StateBeginAliasModelBatch(qbool translucent)
{
	if (translucent) {
		R_ApplyRenderingState(r_state_aliasmodel_translucent_batch);
	}
	else {
		R_ApplyRenderingState(r_state_aliasmodel_opaque_batch);
	}
}
