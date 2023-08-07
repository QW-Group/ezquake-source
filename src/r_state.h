/*
Copyright (C) 2018 ezQuake team

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

#ifndef EZQUAKE_R_STATE_HEADER
#define EZQUAKE_R_STATE_HEADER

#include "r_local.h"
#include "r_vao.h"

#define MAX_GLC_TEXTURE_UNIT_STATES    4
#define MAX_GLC_ATTRIBUTES            16

// rendering state
typedef enum {
	r_depthfunc_less,
	r_depthfunc_equal,
	r_depthfunc_lessorequal,
	r_depthfunc_count
} r_depthfunc_t;

typedef enum {
	r_cullface_front,
	r_cullface_back,

	r_cullface_count
} r_cullface_t;

typedef enum {
	r_blendfunc_overwrite,
	r_blendfunc_additive_blending,
	r_blendfunc_premultiplied_alpha,
	r_blendfunc_src_dst_color_dest_zero,
	r_blendfunc_src_dst_color_dest_one,
	r_blendfunc_src_dst_color_dest_src_color,
	r_blendfunc_src_zero_dest_one_minus_src_color,
	r_blendfunc_src_zero_dest_src_color,
	r_blendfunc_src_one_dest_zero,
	r_blendfunc_src_zero_dest_one,
	r_blendfunc_src_one_dest_one_minus_src_color,

	r_blendfunc_count
} r_blendfunc_t;

typedef enum {
	r_polygonmode_fill,
	r_polygonmode_line,

	r_polygonmode_count
} r_polygonmode_t;

typedef enum {
	r_polygonoffset_disabled,
	r_polygonoffset_standard,
	r_polygonoffset_outlines,

	r_polygonoffset_count
} r_polygonoffset_t;

typedef enum {
	r_alphatest_func_always,
	r_alphatest_func_greater,

	r_alphatest_func_count
} r_alphatest_func_t;

typedef enum {
	r_texunit_mode_blend,
	r_texunit_mode_replace,
	r_texunit_mode_modulate,
	r_texunit_mode_decal,
	r_texunit_mode_add,

	r_texunit_mode_count
} r_texunit_mode_t;

typedef enum {
	r_fogmode_disabled,
	r_fogmode_enabled,

	r_fogmode_count
} r_fogmode_t;

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
typedef struct glc_vertex_array_element_s {
	qbool enabled;

	r_buffer_id buf;
	int size;
	unsigned int type;
	int stride;
	void* pointer_or_offset;
} glc_vertex_array_element_t;

typedef struct glc_attribute_s {
	qbool enabled;

	int location;
} glc_attribute_t;
#endif

typedef struct rendering_state_s {
	struct {
		r_depthfunc_t func;
		double nearRange;
		double farRange;
		qbool test_enabled;
		qbool mask_enabled;
	} depth;

	// FIXME: currentWidth & currentHeight should be initialised to dimensions of window
	int currentViewportX, currentViewportY;
	int currentViewportWidth, currentViewportHeight;

	int fullScreenViewportX, fullScreenViewportY;
	int fullScreenViewportWidth, fullScreenViewportHeight;

	qbool framebuffer_srgb;

	struct {
		qbool smooth;
		float width;
		qbool flexible_width;
	} line;

	struct {
		r_fogmode_t mode;
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
		float color[4];
		float density;
		fogcalc_t calculation;
		float start;
		float end;
#endif
	} fog;

	struct {
		r_cullface_t mode;
		qbool enabled;
	} cullface;

	r_blendfunc_t blendFunc;

	struct {
		r_polygonoffset_t option;
		float factor;
		float units;
		qbool fillEnabled;
		qbool lineEnabled;
	} polygonOffset;

	r_polygonmode_t polygonMode;
	float clearColor[4];
	float color[4];
	qbool blendingEnabled;
	qbool colorMask[4];

	r_vao_id vao_id;

	// GLC only...
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	struct {
		qbool enabled;
		r_alphatest_func_t func;
		float value;
	} alphaTesting;

	struct {
		qbool enabled;
		r_texunit_mode_t mode;

		glc_vertex_array_element_t va;
	} textureUnits[MAX_GLC_TEXTURE_UNIT_STATES];

	qbool colorValid;

	glc_vertex_array_element_t vertex_array;
	glc_vertex_array_element_t color_array;
	glc_vertex_array_element_t normal_array;
#endif

	// always false if not classic
	qbool glc_vao_force_rebind;

	// misc (texture downloads, screenshots & atlas building)
	int pack_alignment;

	// meta
	qbool initialized;
	char name[32];
} rendering_state_t;

typedef enum {
	r_state_null,

	r_state_default_opengl,

	r_state_default_3d,
	r_state_sprites_textured,

	r_state_default_2d,
	r_state_brighten_screen,
	r_state_line,
	r_state_hud_images_glc,
	r_state_hud_images_glc_non_glsl,
	r_state_hud_images_alphatested_glc,
	r_state_hud_images_alphatested_glc_non_glsl,
	r_state_poly_blend,
	r_state_hud_images_glm,
	r_state_hud_polygons_glm,

	r_state_sky_fast,
	r_state_skydome_background_pass,
	r_state_skydome_cloud_pass,
	r_state_skydome_single_pass,
	r_state_skydome_single_pass_program,
	r_state_skydome_zbuffer_pass,
	r_state_skydome_zbuffer_pass_fogged,
	r_state_skybox,

	r_state_sky_fast_bmodel,
	r_state_skydome_single_pass_bmodel,
	r_state_skydome_cloud_pass_bmodel,
	r_state_skydome_background_pass_bmodel,

	r_state_world_texture_chain,
	r_state_world_texture_chain_fullbright,
	r_state_world_blend_lightmaps,
	r_state_world_caustics,

	r_state_world_fast_opaque_water,
	r_state_world_fast_translucent_water,
	r_state_world_opaque_water,
	r_state_world_translucent_water,
	r_state_world_alpha_surfaces,
	r_state_world_fullbrights,
	r_state_world_lumas,
	r_state_world_details,
	r_state_world_outline,

	r_state_world_singletexture_glc,
	r_state_world_material_lightmap,
	r_state_world_material_lightmap_luma,
	r_state_world_material_lightmap_fb,
	r_state_world_material_fb_lightmap,
	r_state_world_material_luma_lightmap,

	r_state_alpha_surfaces_offset_glm,
	r_state_opaque_surfaces_offset_glm,
	r_state_alpha_surfaces_glm,
	r_state_opaque_surfaces_glm,

	r_state_aliasmodel_caustics,

	r_state_aliasmodel_powerupshell,
	r_state_weaponmodel_powerupshell,

	// meag: no multitexture_additive: multitex currently only for .mdl files, additive only for .md3
	r_state_aliasmodel_notexture_opaque,
	r_state_aliasmodel_notexture_transparent,
	r_state_aliasmodel_notexture_additive,
	r_state_aliasmodel_singletexture_opaque,
	r_state_aliasmodel_singletexture_transparent,
	r_state_aliasmodel_singletexture_additive,
	r_state_aliasmodel_multitexture_opaque,
	r_state_aliasmodel_multitexture_transparent,
	r_state_aliasmodel_transparent_zpass,
	r_state_weaponmodel_singletexture_opaque,
	r_state_weaponmodel_singletexture_transparent,
	r_state_weaponmodel_singletexture_additive,
	r_state_weaponmodel_multitexture_opaque,
	r_state_weaponmodel_multitexture_transparent,
	r_state_weaponmodel_transparent_zpass,

	r_state_aliasmodel_shadows,
	r_state_aliasmodel_outline,
	r_state_aliasmodel_outline_spec,
	r_state_weaponmodel_outline,

	r_state_aliasmodel_opaque_batch,
	r_state_aliasmodel_translucent_batch,
	r_state_aliasmodel_translucent_batch_zpass,

	r_state_aliasmodel_additive_batch,

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	r_state_postprocess_bloom1,
	r_state_postprocess_bloom2,
	r_state_postprocess_bloom_darkenpass,
	r_state_postprocess_bloom_blurpass,
	r_state_postprocess_bloom_downsample,
	r_state_postprocess_bloom_downsample_blend,
	r_state_postprocess_bloom_restore,
	r_state_postprocess_bloom_draweffect,
#endif

	r_state_light_bubble,
	r_state_chaticon,
	r_state_particles_classic,
	r_state_particles_qmb_textured_blood,
	r_state_particles_qmb_textured,
	r_state_particles_qmb_untextured,
	r_state_coronas,

	r_state_drawflat_with_lightmaps_glc,
	r_state_drawflat_without_lightmaps_glc,
	r_state_drawflat_without_lightmaps_unfogged_glc,

	r_state_fx_world_geometry,

	r_state_count
} r_state_id;

rendering_state_t* R_InitRenderingState(r_state_id id, qbool default_state, const char* name, r_vao_id vao);
rendering_state_t* R_CopyRenderingState(r_state_id id, r_state_id original_id, const char* name);
rendering_state_t* R_Init3DSpriteRenderingState(r_state_id id, const char* name);
void R_ApplyRenderingState(r_state_id id);
void R_BufferInvalidateBoundState(r_buffer_id ref);

void R_CustomColor(float r, float g, float b, float a);
void R_CustomColor4ubv(const byte* color);
void R_CustomLineWidth(float width);
void R_EnableScissorTest(int x, int y, int width, int height);
void R_DisableScissorTest(void);
void R_ClearColor(float r, float g, float b, float a);
void R_CustomPolygonOffset(r_polygonoffset_t mode);

void R_Hud_Initialise(void);

void R_Viewport(int x, int y, int width, int height);
void R_SetFullScreenViewport(int x, int y, int width, int height);
void R_GetFullScreenViewport(int* viewport);

#endif // EZQUAKE_R_STATE_HEADER
