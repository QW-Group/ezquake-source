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
#include "glc_state.h"
#include "tr_types.h"

static void R_InitialiseWorldStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_world_texture_chain, true, "worldTextureChainState", vao_brushmodel);
	state->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(state, 1, true, r_texunit_mode_blend);

	state = R_InitRenderingState(r_state_world_texture_chain_fullbright, true, "worldTextureChainFullbrightState", vao_brushmodel);
	state->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_world_blend_lightmaps, true, "blendLightmapState", vao_brushmodel_lightmap_pass);
	state->depth.mask_enabled = false;
	state->depth.func = r_depthfunc_equal;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_zero_dest_one_minus_src_color;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_world_caustics, true, "causticsState", vao_brushmodel_simpletex);
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_decal);
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	state->blendingEnabled = true;

	state = R_InitRenderingState(r_state_aliasmodel_caustics, true, "aliasModelCausticsState", vao_aliasmodel);
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;
	state->blendingEnabled = true;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(state, 1, true, r_texunit_mode_decal);

	state = R_InitRenderingState(r_state_world_fast_opaque_water, true, "fastWaterSurfacesState", vao_brushmodel_simpletex);
	state->depth.test_enabled = true;
	state->fog.mode = r_fogmode_enabled;

	state = R_CopyRenderingState(r_state_world_opaque_water, r_state_world_fast_opaque_water, "waterSurfacesState");
	state->cullface.enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_CopyRenderingState(r_state_world_translucent_water, r_state_world_fast_opaque_water, "translucentWaterSurfacesState");
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = true;
	state->depth.mask_enabled = false; // FIXME: water-alpha < 0.9 only?
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_CopyRenderingState(r_state_world_fast_translucent_water, r_state_world_translucent_water, "translucentFastWater");
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = true;
	R_GLC_TextureUnitSet(state, 0, false, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_world_alpha_surfaces, true, "alphaChainState", vao_brushmodel);
	R_GLC_ConfigureAlphaTesting(state, true, r_alphatest_func_greater, 0.333f);
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);
	if (glConfig.texture_units >= 2) {
		R_GLC_TextureUnitSet(state, 1, true, r_texunit_mode_blend); // modulate if !inv_lmaps
	}

	state = R_InitRenderingState(r_state_world_fullbrights, true, "fullbrightsState", vao_brushmodel);
	state->depth.mask_enabled = false;
	R_GLC_EnableAlphaTesting(state);
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_world_lumas, true, "lumasState", vao_brushmodel);
	state->depth.mask_enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_world_details, true, "detailPolyState", vao_brushmodel_details);
	state->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_decal);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_dst_color_dest_src_color;

	state = R_InitRenderingState(r_state_world_outline, true, "mapOutlineState", vao_brushmodel);
	state->polygonOffset.option = r_polygonoffset_outlines;
	state->polygonMode = r_polygonmode_line;
	state->depth.mask_enabled = false;
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

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

static void R_Initialise2DStates(void)
{
	rendering_state_t* state;
	r_vao_id postprocess_vao = R_UseImmediateOpenGL() ? vao_none : vao_postprocess;

	state = R_InitRenderingState(r_state_default_2d, true, "default2DState", postprocess_vao);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;

	state = R_InitRenderingState(r_state_brighten_screen, true, "brightenScreenState", postprocess_vao);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	R_GLC_EnableAlphaTesting(state); // really?
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_dst_color_dest_one;

	state = R_InitRenderingState(r_state_line, true, "lineState", vao_hud_lines);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->line.flexible_width = true;

	if (R_UseImmediateOpenGL()) {
		state->vao_id = vao_none;
	}

	state = R_InitRenderingState(r_state_hud_images_glc, true, "glcImageDrawState", vao_hud_images);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_CopyRenderingState(r_state_hud_images_alphatested_glc, r_state_hud_images_glc, "glcAlphaTestedImageDrawState");
	R_GLC_EnableAlphaTesting(state);

	state = R_CopyRenderingState(r_state_hud_images_glc_non_glsl, r_state_hud_images_glc, "glcImageDrawStateNonGLSL");
	state->vao_id = vao_hud_images_non_glsl;

	state = R_CopyRenderingState(r_state_hud_images_alphatested_glc_non_glsl, r_state_hud_images_alphatested_glc, "glcAlphaTestedImageDrawStateNonGLSL");
	state->vao_id = vao_hud_images_non_glsl;

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	state = R_InitRenderingState(r_state_postprocess_bloom1, true, "glcBloomState", postprocess_vao);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	R_GLC_EnableAlphaTesting(state);
	state->blendingEnabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_CopyRenderingState(r_state_postprocess_bloom_darkenpass, r_state_postprocess_bloom1, "glcBloomState-darken");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_dst_color_dest_zero;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_CopyRenderingState(r_state_postprocess_bloom_blurpass, r_state_postprocess_bloom1, "glcBloomState-blur");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_src_one_dest_one_minus_src_color;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_CopyRenderingState(r_state_postprocess_bloom_downsample, r_state_postprocess_bloom1, "glcBloomState-downsamp");

	state = R_CopyRenderingState(r_state_postprocess_bloom_downsample_blend, r_state_postprocess_bloom1, "glcBloomState-downsamp-blend");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;

	state = R_CopyRenderingState(r_state_postprocess_bloom_restore, r_state_postprocess_bloom1, "glcBloomState-restore");

	state = R_CopyRenderingState(r_state_postprocess_bloom_draweffect, r_state_postprocess_bloom1, "glcBloomState-restore");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);
#endif

	state = R_InitRenderingState(r_state_poly_blend, true, "polyBlendState", postprocess_vao);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_InitRenderingState(r_state_hud_images_glm, true, "glmImageDrawState", vao_hud_images);
	state->depth.test_enabled = false;
	state->cullface.enabled = false;
	R_GLC_DisableAlphaTesting(state);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	state = R_CopyRenderingState(r_state_hud_polygons_glm, r_state_hud_images_glm, "glmPolygonDrawState");
	state->vao_id = vao_hud_polygons;

	state = R_InitRenderingState(r_state_fx_world_geometry, true, "r_state_fx_world_geometry", postprocess_vao);
	state->depth.test_enabled = false;
	state->depth.mask_enabled = false;
	state->cullface.enabled = false;
	R_GLC_EnableAlphaTesting(state); // really?
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
}

static void R_InitialiseSpriteStates(void)
{
	rendering_state_t* state;

	// Simple items
	state = R_Init3DSpriteRenderingState(r_state_sprites_textured, "sprite_entity_state");
	R_GLC_ConfigureAlphaTesting(state, true, r_alphatest_func_greater, 0.333f);
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	// Standard particles
	state = R_Init3DSpriteRenderingState(r_state_particles_classic, "particle_state");
	state->depth.mask_enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	// QMB particles
	state = R_Init3DSpriteRenderingState(r_state_particles_qmb_textured_blood, "qmb-textured-blood");
	state->blendFunc = r_blendfunc_src_zero_dest_one_minus_src_color;
	state->depth.mask_enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_Init3DSpriteRenderingState(r_state_particles_qmb_textured, "qmb-textured");
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->depth.mask_enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_Init3DSpriteRenderingState(r_state_particles_qmb_untextured, "qmb-untextured");
	state->depth.mask_enabled = false;

	// Flashblend bubbles
	state = R_Init3DSpriteRenderingState(r_state_light_bubble, "bubble-state");
	state->depth.test_enabled = true;
	state->depth.mask_enabled = false;

	// Chaticons
	state = R_Init3DSpriteRenderingState(r_state_chaticon, "chaticon_state");
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	// Coronas
	state = R_Init3DSpriteRenderingState(r_state_coronas, "coronaState");
	state->blendFunc = r_blendfunc_additive_blending;
	state->depth.mask_enabled = false;
	state->depth.test_enabled = false;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);
}

static void R_InitialiseEntityStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_aliasmodel_powerupshell, true, "powerupShellState", vao_aliasmodel);
	state->fog.mode = r_fogmode_enabled;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	R_GLC_DisableAlphaTesting(state);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_CopyRenderingState(r_state_aliasmodel_outline, r_state_aliasmodel_powerupshell, "aliasmodel-outline");
	state->fog.mode = r_fogmode_enabled;
	state->blendingEnabled = false;
	state->cullface.mode = r_cullface_back;

	state = R_CopyRenderingState(r_state_aliasmodel_outline_spec, r_state_aliasmodel_outline, "aliasmodel-outline-spec");
	state->cullface.mode = r_cullface_front;
	state->depth.test_enabled = false;

	state = R_CopyRenderingState(r_state_weaponmodel_outline, r_state_aliasmodel_outline, "weaponmodel-outline");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;

	state = R_CopyRenderingState(r_state_weaponmodel_powerupshell, r_state_aliasmodel_powerupshell, "weaponmodel-shell");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;

	state = R_InitRenderingState(r_state_aliasmodel_notexture_opaque, true, "opaqueAliasModelNoTexture", vao_aliasmodel);
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = false;
	R_GLC_DisableAlphaTesting(state);
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	state->polygonMode = r_polygonmode_fill;
	state->line.smooth = false;
	state->fog.mode = r_fogmode_enabled;

	state = R_CopyRenderingState(r_state_aliasmodel_transparent_zpass, r_state_aliasmodel_notexture_opaque, "aliasModelZPass");
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = false;

	state = R_CopyRenderingState(r_state_aliasmodel_singletexture_opaque, r_state_aliasmodel_notexture_opaque, "opaqueAliasModelSingleTex");
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);

	state = R_CopyRenderingState(r_state_aliasmodel_multitexture_opaque, r_state_aliasmodel_notexture_opaque, "opaqueAliasModelMultiTex");
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_modulate);
	R_GLC_TextureUnitSet(state, 1, true, r_texunit_mode_decal);

	// transparent
	state = R_CopyRenderingState(r_state_aliasmodel_notexture_transparent, r_state_aliasmodel_notexture_opaque, "transparentAliasModelNoTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state = R_CopyRenderingState(r_state_aliasmodel_singletexture_transparent, r_state_aliasmodel_singletexture_opaque, "transparentAliasModelSingleTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state = R_CopyRenderingState(r_state_aliasmodel_multitexture_transparent, r_state_aliasmodel_multitexture_opaque, "transparentAliasModelMultiTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;

	// additive
	state = R_CopyRenderingState(r_state_aliasmodel_notexture_additive, r_state_aliasmodel_notexture_opaque, "additiveAliasModelNoTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;
	state = R_CopyRenderingState(r_state_aliasmodel_singletexture_additive, r_state_aliasmodel_singletexture_opaque, "additiveAliasModelSingleTex");
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_additive_blending;

	state = R_CopyRenderingState(r_state_weaponmodel_singletexture_opaque, r_state_aliasmodel_singletexture_opaque, "weaponModelSingleOpaque");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;
	state = R_CopyRenderingState(r_state_weaponmodel_multitexture_opaque, r_state_weaponmodel_singletexture_opaque, "weaponModelMultiOpaque");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;

	// transparent
	state = R_CopyRenderingState(r_state_weaponmodel_singletexture_transparent, r_state_aliasmodel_singletexture_transparent, "weaponModelSingleTransparent");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;
	state = R_CopyRenderingState(r_state_weaponmodel_multitexture_transparent, r_state_weaponmodel_singletexture_transparent, "weaponModelMultiTransparent");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;
	state = R_CopyRenderingState(r_state_weaponmodel_transparent_zpass, r_state_weaponmodel_singletexture_transparent, "weaponModelZPass");
	R_GLC_TextureUnitSet(state, 0, false, r_texunit_mode_replace);
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = false;

	// additive
	state = R_CopyRenderingState(r_state_weaponmodel_singletexture_additive, r_state_aliasmodel_singletexture_additive, "weaponModelSingleAdditive");
	state->depth.farRange = R_UseImmediateOpenGL() ? 0.3f : state->depth.farRange;

	state = R_InitRenderingState(r_state_aliasmodel_shadows, true, "aliasModelShadowState", vao_aliasmodel);
	state->polygonOffset.option = r_polygonoffset_disabled;
	state->cullface.enabled = true;
	state->cullface.mode = r_cullface_front;
	state->polygonMode = r_polygonmode_fill;
	state->line.smooth = false;
	state->fog.mode = r_fogmode_enabled;
	R_GLC_DisableAlphaTesting(state);
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->color[0] = state->color[1] = state->color[2] = 0;
	state->color[3] = 0.5f;

	state = R_InitRenderingState(r_state_aliasmodel_opaque_batch, true, "aliasModelBatchState", vao_aliasmodel);
	state->cullface.mode = r_cullface_front;
	state->cullface.enabled = true;
	state->depth.mask_enabled = true;
	state->depth.test_enabled = true;
	state->blendingEnabled = false;

	state = R_CopyRenderingState(r_state_aliasmodel_translucent_batch, r_state_aliasmodel_opaque_batch, "aliasModelTranslucentBatchState");
	state->depth.mask_enabled = false;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->blendingEnabled = true;

	state = R_CopyRenderingState(r_state_aliasmodel_translucent_batch_zpass, r_state_aliasmodel_opaque_batch, "aliasModelTranslucentBatchZPass");
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = false;

	state = R_CopyRenderingState(r_state_aliasmodel_additive_batch, r_state_aliasmodel_opaque_batch, "aliasModelTranslucentBatchState");
	state->blendFunc = r_blendfunc_additive_blending;
	state->blendingEnabled = true;
}

static void R_InitialiseBrushModelStates(void)
{
	rendering_state_t* current;

	current = R_InitRenderingState(r_state_drawflat_without_lightmaps_glc, true, "drawFlatNoLightmapState", vao_brushmodel);
	current->fog.mode = r_fogmode_enabled;

	current = R_InitRenderingState(r_state_drawflat_without_lightmaps_unfogged_glc, true, "drawFlatNoLightmapStateUnfogged", vao_brushmodel);
	current->fog.mode = r_fogmode_disabled;

	current = R_InitRenderingState(r_state_drawflat_with_lightmaps_glc, true, "drawFlatLightmapState", vao_brushmodel_lightmap_pass);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_blend);

	// Single-texture: all of these are the same so we don't need to bother about others
	current = R_InitRenderingState(r_state_world_singletexture_glc, true, "world:singletex", vao_brushmodel);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);

	// material * lightmap
	current = R_InitRenderingState(r_state_world_material_lightmap, true, "r_state_world_material_lightmap", vao_brushmodel_lm_unit1);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_blend);

	// material * lightmap + luma
	current = R_InitRenderingState(r_state_world_material_lightmap_luma, true, "r_state_world_material_lightmap_luma", vao_brushmodel_lm_unit1);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_blend);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_add);

	// r_state_world_material_lightmap_fb
	current = R_CopyRenderingState(r_state_world_material_lightmap_fb, r_state_world_material_lightmap_luma, "r_state_world_material_lightmap_fb");
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_decal);

	// no fullbrights, 3 units: blend(material + luma, lightmap) 
	current = R_InitRenderingState(r_state_world_material_fb_lightmap, true, "r_state_world_material_fb_lightmap", vao_brushmodel);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_add);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_blend);

	// lumas enabled, 3 units
	current = R_InitRenderingState(r_state_world_material_luma_lightmap, true, "r_state_world_material_luma_lightmap", vao_brushmodel);
	current->fog.mode = r_fogmode_enabled;
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_add);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_blend);
}

void R_InitialiseStates(void)
{
	R_InitRenderingState(r_state_default_3d, true, "default3DState", vao_none);

	R_InitialiseSpriteStates();
	R_InitialiseBrushModelStates();
	R_Initialise2DStates();
	R_InitialiseEntityStates();
	R_InitialiseWorldStates();

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	GLC_InitialiseSkyStates();
#endif
}

float R_WaterAlpha(void)
{
	extern cvar_t r_wateralpha;

	return bound((1 - r_refdef2.max_watervis), r_wateralpha.value, 1);
}

void R_StateDefault3D(void)
{
	R_TraceResetRegion(false);

	R_TraceEnterFunctionRegion;
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
	R_TraceLeaveFunctionRegion;
}

void R_StateBeginAlphaLineRGB(float thickness)
{
	if (thickness > 0.0) {
		R_CustomLineWidth(thickness);
	}
}

void R_StateBeginDrawAliasModel(const entity_t* ent, aliashdr_t* paliashdr)
{
	extern cvar_t r_viewmodelsize;

	R_TraceEnterRegion(__func__, true);

	R_RotateForEntity(ent);
	if (ent->renderfx & RF_WEAPONMODEL) {
		R_ScaleModelview(0.5 + bound(0, r_viewmodelsize.value, 1) / 2, 1, 1);
	}

	R_TraceLeaveFunctionRegion;
}
