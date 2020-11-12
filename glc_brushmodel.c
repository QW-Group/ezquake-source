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

$Id: gl_model.c,v 1.41 2007-10-07 08:06:33 tonik Exp $
*/
// gl_model.c  -- model loading and caching

// models are the only shared resource between a client and server running on the same machine.
#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "r_framestats.h"
#include "r_texture.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_brushmodel.h"
#include "r_brushmodel_sky.h"
#include "glc_local.h"
#include "tr_types.h"
#include "r_renderer.h"
#include "glsl/constants.glsl"
#include "r_lightmaps.h"
#include "r_trace.h"

extern glpoly_t *fullbright_polys[MAX_GLTEXTURES];
extern glpoly_t *luma_polys[MAX_GLTEXTURES];

void GLC_LightmapArrayToggle(qbool use_array);

glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

void GLC_EnsureVAOCreated(r_vao_id vao)
{
	if (R_VertexArrayCreated(vao)) {
		return;
	}

	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_vertex_data)) {
		// TODO: vbo data in client memory
		return;
	}

	R_GenVertexArray(vao);
	GLC_VAOSetVertexBuffer(vao, r_buffer_brushmodel_vertex_data);
	// TODO: index data _not_ in client memory

	switch (vao) {
		case vao_brushmodel:
		{
			// tmus: [material, material2, lightmap]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 1, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 2, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			GLC_VAOEnableCustomAttribute(vao, 0, r_program_attribute_world_drawflat_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 1, r_program_attribute_world_textured_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 2, r_program_attribute_world_textured_detailCoord, 2, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		case vao_brushmodel_lm_unit1:
		{
			// tmus: [material, lightmap, material2]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 1, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 2, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableCustomAttribute(vao, 0, r_program_attribute_world_drawflat_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 1, r_program_attribute_world_textured_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 2, r_program_attribute_world_textured_detailCoord, 2, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		case vao_brushmodel_details:
		{
			// tmus: [details]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			GLC_VAOEnableCustomAttribute(vao, 0, r_program_attribute_world_drawflat_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 1, r_program_attribute_world_textured_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 2, r_program_attribute_world_textured_detailCoord, 2, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		case vao_brushmodel_lightmap_pass:
		{
			// tmus: [lightmap]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			GLC_VAOEnableCustomAttribute(vao, 0, r_program_attribute_world_drawflat_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 1, r_program_attribute_world_textured_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 2, r_program_attribute_world_textured_detailCoord, 2, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		case vao_brushmodel_simpletex:
		{
			// tmus: [material]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableCustomAttribute(vao, 0, r_program_attribute_world_drawflat_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 1, r_program_attribute_world_textured_style, 1, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, flatstyle));
			GLC_VAOEnableCustomAttribute(vao, 2, r_program_attribute_world_textured_detailCoord, 2, GL_FLOAT, GL_FALSE, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		default:
		{
			assert(false);
			break;
		}
	}
}

static void GLC_BlendLightmaps(void);
static void GLC_BlendLightmaps_GLSL(void);
void GLC_RenderFullbrights(void);
void GLC_RenderLumas(void);
void GLC_RenderLumas_GLSL(void);
void GLC_RenderFullbrights_GLSL(void);

qbool GLC_DrawflatProgramCompile(void)
{
	int flags = (glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS) ? 1 : 0;

	if (R_ProgramRecompileNeeded(r_program_world_drawflat_glc, flags)) {
		char included_definitions[512];

		included_definitions[0] = '\0';
		if (flags) {
			strlcpy(included_definitions, "#define EZ_USE_TEXTURE_ARRAYS\n", sizeof(included_definitions));
		}
		R_ProgramCompileWithInclude(r_program_world_drawflat_glc, included_definitions);

		R_ProgramSetCustomOptions(r_program_world_drawflat_glc, flags);
	}

	return R_ProgramReady(r_program_world_drawflat_glc);
}

static void GLC_SurfaceColor(const msurface_t* s, byte* desired)
{
	if (s->flags & SURF_DRAWSKY) {
		memcpy(desired, r_skycolor.color, 3);
	}
	else if (s->flags & SURF_DRAWTURB) {
		memcpy(desired, SurfaceFlatTurbColor(s->texinfo->texture), 3);
	}
	else if (s->flags & SURF_DRAWFLAT_FLOOR) {
		memcpy(desired, r_floorcolor.color, 3);
	}
	else {
		memcpy(desired, r_wallcolor.color, 3);
	}
}

static void GLC_DrawFlat_GLSL(model_t* model, qbool polygonOffset)
{
	extern cvar_t r_watercolor, r_slimecolor, r_lavacolor, r_telecolor;
	float color[4] = { 0, 0, 0, 1 };
	int index_count = 0;
	msurface_t* s;
	msurface_t* prev;
	int i;
	unsigned int lightmap_count = R_LightmapCount();
	qbool first_lightmap_surf = true;
	qbool draw_caustics = r_refdef2.drawCaustics;
	qbool clear_chains = !r_refdef2.drawWorldOutlines;

	GLC_LightmapArrayToggle(true);

	R_ProgramUse(r_program_world_drawflat_glc);
	VectorScale(r_wallcolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_wallcolor, color);
	VectorScale(r_floorcolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_floorcolor, color);
	VectorScale(r_skycolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_skycolor, color);
	VectorScale(r_watercolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_watercolor, color);
	VectorScale(r_slimecolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_slimecolor, color);
	VectorScale(r_lavacolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_lavacolor, color);
	VectorScale(r_telecolor.color, 1.0f / 255.0f, color);
	R_ProgramUniform4fv(r_program_uniform_world_drawflat_glc_telecolor, color);

	if (model->drawflat_chain) {
		R_ApplyRenderingState(r_state_drawflat_without_lightmaps_glc);
		R_CustomPolygonOffset(polygonOffset ? r_polygonoffset_standard : r_polygonoffset_disabled);

		// drawflat_chain has no lightmaps
		s = model->drawflat_chain;
		while (s) {
			glpoly_t *p;

			for (p = s->polys; p; p = p->next) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}

			// START shaman FIX /r_drawflat + /gl_caustics {
			if ((s->flags & SURF_UNDERWATER) && draw_caustics) {
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
			}
			// } END shaman FIX /r_drawflat + /gl_caustics

			if (clear_chains) {
				prev = s;
				s = s->drawflatchain;
				prev->drawflatchain = NULL;
			}
			else {
				s = s->drawflatchain;
			}
		}
		if (index_count) {
			GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			index_count = 0;
		}

		if (clear_chains) {
			model->drawflat_chain = NULL;
		}
	}

	// go through lightmap chains
	for (i = 0; i < lightmap_count; ++i) {
		msurface_t* surf = R_DrawflatLightmapChain(i);

		if (surf) {
			if (first_lightmap_surf) {
				R_ApplyRenderingState(r_state_drawflat_with_lightmaps_glc);
				R_CustomPolygonOffset(polygonOffset ? r_polygonoffset_standard : r_polygonoffset_disabled);
			}

			if (GLC_SetTextureLightmap(0, i) && index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}
			while (surf) {
				glpoly_t* p;

				for (p = surf->polys; p; p = p->next) {
					index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
				}

				if (clear_chains) {
					prev = surf;
					surf = surf->drawflatchain;
					prev->drawflatchain = NULL;
				}
				else {
					surf = surf->drawflatchain;
				}
			}
			if (clear_chains) {
				R_ClearDrawflatLightmapChain(i);
			}
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
		index_count = 0;
	}
	R_ProgramUse(r_program_none);
}

static void GLC_DrawFlat_Immediate(model_t* model, qbool polygonOffset)
{
	int i;
	qbool first_surf = true;
	qbool draw_caustics = r_refdef2.drawCaustics;
	qbool use_vbo = buffers.supported && modelIndexes;
	qbool clear_chains = !r_refdef2.drawWorldOutlines;
	byte current[3] = { 255, 255, 255 }, desired[4] = { 255, 255, 255, 255 };
	unsigned int lightmap_count = R_LightmapCount();
	int index_count = 0;
	msurface_t *s, *prev;
	int k;
	float *v;

	GLC_LightmapArrayToggle(false);

	s = model->drawflat_chain;
	R_ProgramUse(r_program_none);
	while (s) {
		GLC_SurfaceColor(s, desired);

		if (first_surf || (use_vbo && (desired[0] != current[0] || desired[1] != current[1] || desired[2] != current[2]))) {
			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}
			if (first_surf) {
				R_ApplyRenderingState(r_state_drawflat_without_lightmaps_glc);
				R_CustomPolygonOffset(polygonOffset ? r_polygonoffset_standard : r_polygonoffset_disabled);
			}
			R_CustomColor4ubv(desired);
		}

		VectorCopy(desired, current);
		first_surf = false;

		{
			glpoly_t *p;
			for (p = s->polys; p; p = p->next) {
				v = p->verts[0];

				if (use_vbo) {
					index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
				}
				else {
					R_CustomColor4ubv(desired);
					GLC_Begin(GL_TRIANGLE_STRIP);
					for (k = 0; k < p->numverts; k++, v += VERTEXSIZE) {
						GLC_Vertex3fv(v);
					}
					GLC_End();
				}
			}
		}

		// START shaman FIX /r_drawflat + /gl_caustics {
		if ((s->flags & SURF_UNDERWATER) && draw_caustics) {
			s->polys->caustics_chain = caustics_polys;
			caustics_polys = s->polys;
		}
		// } END shaman FIX /r_drawflat + /gl_caustics

		if (clear_chains) {
			prev = s;
			s = s->drawflatchain;
			prev->drawflatchain = NULL;
		}
		else {
			s = s->drawflatchain;
		}
	}
	if (clear_chains) {
		model->drawflat_chain = NULL;
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
		index_count = 0;
	}

	first_surf = true;
	for (i = 0; i < lightmap_count; ++i) {
		msurface_t* surf = R_DrawflatLightmapChain(i);

		if (surf) {
			if (GLC_SetTextureLightmap(0, i) && index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}
			while (surf) {
				glpoly_t* p;

				GLC_SurfaceColor(surf, desired);

				if (first_surf || (use_vbo && (desired[0] != current[0] || desired[1] != current[1] || desired[2] != current[2]))) {
					if (index_count) {
						GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						index_count = 0;
					}
					if (first_surf) {
						R_ApplyRenderingState(r_state_drawflat_with_lightmaps_glc);
						R_CustomPolygonOffset(polygonOffset ? r_polygonoffset_standard : r_polygonoffset_disabled);
					}
					R_CustomColor4ubv(desired);
				}
				VectorCopy(desired, current);
				first_surf = false;

				for (p = surf->polys; p; p = p->next) {
					v = p->verts[0];

					if (use_vbo) {
						index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
					}
					else {
						R_CustomColor4ubv(desired);
						GLC_Begin(GL_TRIANGLE_STRIP);
						for (k = 0; k < p->numverts; k++, v += VERTEXSIZE) {
							glTexCoord2f(v[5], v[6]);
							GLC_Vertex3fv(v);
						}
						GLC_End();
					}
				}

				if (clear_chains) {
					prev = surf;
					surf = surf->drawflatchain;
					prev->drawflatchain = NULL;
				}
				else {
					surf = surf->drawflatchain;
				}
			}
			if (clear_chains) {
				R_ClearDrawflatLightmapChain(i);
			}
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}
}

static void GLC_DrawFlat(model_t *model, qbool polygonOffset)
{
	extern cvar_t gl_program_world;

	if (!model->drawflat_todo) {
		return;
	}

	R_TraceEnterFunctionRegion;

	if (gl_program_world.integer && buffers.supported && modelIndexes && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_DrawflatProgramCompile()) {
		GLC_DrawFlat_GLSL(model, polygonOffset);
	}
	else {
		GLC_DrawFlat_Immediate(model, polygonOffset);
	}

	model->drawflat_chain = NULL;
	model->drawflat_todo = false;

	// START shaman FIX /r_drawflat + /gl_caustics {
	if (r_refdef2.drawCaustics && caustics_polys) {
		GLC_EmitCausticsPolys();
	}
	caustics_polys = NULL;
	// } END shaman FIX /r_drawflat + /gl_caustics

	R_TraceLeaveFunctionRegion;
}

typedef struct texture_unit_allocation_s {
	int matTextureUnit;
	int fbTextureUnit;
	int lmTextureUnit;
	int detailTextureUnit;
	int causticTextureUnit;
	texture_ref null_fb_texture;

	int texture_unit_count;
	qbool couldUseLumaTextures;         // true if user requested lumas and is allowed
	qbool useLumaTextures;              // true if lumas could be used, and model has them
	qbool second_pass_caustics;         // draw caustics in second pass
	qbool second_pass_detail;           // draw detail/noise texture in second pass
	qbool second_pass_luma;             // draw fullbright/luma textures in second pass

	r_state_id rendering_state;         // basic rendering state
} texture_unit_allocation_t;

static void GLC_WorldAllocateTextureUnits(texture_unit_allocation_t* allocations, model_t* model, qbool program_compilation, qbool allow_lumas)
{
	extern cvar_t gl_lumatextures;
	qbool lumas;

	allocations->texture_unit_count = 1;
	allocations->null_fb_texture = null_texture_reference;
	allocations->couldUseLumaTextures = allow_lumas && gl_lumatextures.integer && r_refdef2.allow_lumas;
	allocations->useLumaTextures = allocations->couldUseLumaTextures && model->texturechains_have_lumas;
	allocations->matTextureUnit = allocations->fbTextureUnit = allocations->lmTextureUnit = allocations->detailTextureUnit = allocations->causticTextureUnit = -1;

	lumas = program_compilation ? allocations->couldUseLumaTextures : allocations->useLumaTextures;

	allocations->matTextureUnit = 0;
	if (gl_mtexable) {
		allocations->texture_unit_count = gl_textureunits >= 3 ? 3 : 2;

		if (lumas && !gl_fb_bmodels.integer) {
			// blend(material + fb, lightmap)
			allocations->rendering_state = r_state_world_material_fb_lightmap;
			allocations->null_fb_texture = solidblack_texture;
			allocations->fbTextureUnit = 1;
			allocations->lmTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
		}
		else if (lumas) {
			// blend(material, lightmap) + luma
			allocations->rendering_state = r_state_world_material_lightmap_luma;
			allocations->lmTextureUnit = 1;
			allocations->fbTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
			allocations->null_fb_texture = solidblack_texture; // GL_ADD adds colors, multiplies alphas
		}
		else if (gl_fb_bmodels.integer) {
			// blend(blend(material, lightmap), fb)
			allocations->rendering_state = r_state_world_material_lightmap_fb;
			allocations->lmTextureUnit = 1;
			allocations->fbTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
			allocations->null_fb_texture = transparent_texture; // GL_DECAL mixes color by alpha
		}
		else {
			// blend(material, lightmap) 
			allocations->rendering_state = r_state_world_material_lightmap;
			allocations->lmTextureUnit = 1;
			allocations->fbTextureUnit = -1;
			allocations->null_fb_texture = solidblack_texture; // GL_ADD adds colors, multiplies alphas
			allocations->texture_unit_count = 2;
		}

		if (r_refdef2.drawCaustics && allocations->texture_unit_count < gl_textureunits) {
			allocations->causticTextureUnit = allocations->texture_unit_count++;
		}

		if (gl_detail.integer && R_TextureReferenceIsValid(detailtexture) && allocations->texture_unit_count < gl_textureunits) {
			allocations->detailTextureUnit = allocations->texture_unit_count++;
		}
	}
	else {
		allocations->rendering_state = r_state_world_singletexture_glc;
	}

	allocations->second_pass_caustics = r_refdef2.drawCaustics && allocations->causticTextureUnit < 0;
	allocations->second_pass_detail = R_TextureReferenceIsValid(detailtexture) && gl_detail.integer && allocations->detailTextureUnit < 0;
	allocations->second_pass_luma = lumas && allocations->fbTextureUnit < 0;
}

static void GLC_DrawTextureChains_Immediate(entity_t* ent, model_t *model, qbool caustics, qbool polygonOffset, qbool lumas_only)
{
	texture_unit_allocation_t allocations;
	extern cvar_t gl_lumatextures;
	extern cvar_t gl_textureless;
	int index_count = 0;
	texture_ref fb_texturenum = null_texture_reference;
	int i, k;
	msurface_t *s, *prev;
	float *v;

	qbool use_vbo = buffers.supported && modelIndexes;

	qbool drawfullbrights = false;
	qbool drawlumas = false;
	qbool clear_chains = !r_refdef2.drawWorldOutlines;

	qbool texture_change;
	texture_ref current_material = null_texture_reference;
	texture_ref current_material_fb = null_texture_reference;
	int current_lightmap = -1;

	texture_ref desired_textures[8] = { { 0 } };

	qbool draw_textureless = gl_textureless.integer && model->isworldmodel;

	GLC_WorldAllocateTextureUnits(&allocations, model, false, lumas_only);
	if (lumas_only && !allocations.useLumaTextures) {
		return;
	}

	R_TraceEnterFunctionRegion;

	GLC_LightmapArrayToggle(false);
	R_ApplyRenderingState(allocations.rendering_state);
	if (ent && ent->alpha) {
		R_CustomColor(ent->alpha, ent->alpha, ent->alpha, ent->alpha);
	}
	if (polygonOffset) {
		R_CustomPolygonOffset(r_polygonoffset_standard);
	}

	if (allocations.causticTextureUnit >= 0) {
		allocations.causticTextureUnit = -1;
		allocations.second_pass_caustics = true;
		--allocations.texture_unit_count;
	}
	if (allocations.detailTextureUnit >= 0) {
		allocations.detailTextureUnit = -1;
		allocations.second_pass_detail = true;
		--allocations.texture_unit_count;
	}

	//Tei: textureless for the world brush models (Qrack)
	if (draw_textureless) {
		if (use_vbo) {
			// meag: better to have different states for this
			R_GLC_DisableTexturePointer(0);
			if (allocations.fbTextureUnit >= 0) {
				R_GLC_DisableTexturePointer(allocations.fbTextureUnit);
			}
		}

		if (GL_Supported(R_SUPPORT_MULTITEXTURING)) {
			GLC_MultiTexCoord2f(GL_TEXTURE0, 0, 0);

			if (allocations.fbTextureUnit >= 0) {
				GLC_MultiTexCoord2f(GL_TEXTURE0 + allocations.fbTextureUnit, 0, 0);
			}
		}
		else {
			glTexCoord2f(0, 0);
		}
	}

	R_ProgramUse(r_program_none);
	for (i = 0; i < model->numtextures; i++) {
		texture_t* t;

		if (!model->textures[i] || !model->textures[i]->texturechain) {
			continue;
		}

		t = R_TextureAnimation(ent, model->textures[i]);
		if (lumas_only && !t->isLumaTexture)
			continue;

		if (allocations.useLumaTextures && t->isLumaTexture) {
			fb_texturenum = t->fb_texturenum;
		}
		else if (!t->isLumaTexture && R_TextureReferenceIsValid(t->fb_texturenum)) {
			fb_texturenum = t->fb_texturenum;
		}
		else {
			fb_texturenum = allocations.null_fb_texture;
		}

		//bind the world texture
		texture_change = !R_TextureReferenceEqual(t->gl_texturenum, current_material);
		texture_change |= !R_TextureReferenceEqual(fb_texturenum, current_material_fb);

		current_material = t->gl_texturenum;
		current_material_fb = fb_texturenum;

		desired_textures[allocations.matTextureUnit] = current_material;
		if (allocations.fbTextureUnit >= 0) {
			desired_textures[allocations.fbTextureUnit] = current_material_fb;
		}

		s = model->textures[i]->texturechain;
		while (s) {
			if (!(s->texinfo->flags & TEX_SPECIAL)) {
				if (allocations.lmTextureUnit >= 0) {
					desired_textures[allocations.lmTextureUnit] = GLC_LightmapTexture(s->lightmaptexturenum);
					texture_change |= !R_TextureReferenceEqual(desired_textures[allocations.lmTextureUnit], GLC_LightmapTexture(current_lightmap));
					current_lightmap = s->lightmaptexturenum;
				}
				else {
					GLC_AddToLightmapChain(s);
				}

				if (texture_change) {
					if (index_count) {
						GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						index_count = 0;
					}

					renderer.TextureUnitMultiBind(0, allocations.texture_unit_count, desired_textures);
					if (r_lightmap_lateupload.integer && allocations.lmTextureUnit >= 0) {
						R_UploadLightMap(allocations.lmTextureUnit, current_lightmap);
					}

					texture_change = false;
				}

				if (use_vbo) {
					index_count = GLC_DrawIndexedPoly(s->polys, modelIndexes, modelIndexMaximum, index_count);
				}
				else {
					v = s->polys->verts[0];

					GLC_Begin(GL_TRIANGLE_STRIP);
					for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
						if (allocations.lmTextureUnit >= 0) {
							GLC_MultiTexCoord2f(GL_TEXTURE0 + allocations.lmTextureUnit, v[5], v[6]);
						}

						if (!draw_textureless) {
							if (GL_Supported(R_SUPPORT_MULTITEXTURING)) {
								GLC_MultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);

								if (allocations.fbTextureUnit >= 0) {
									GLC_MultiTexCoord2f(GL_TEXTURE0 + allocations.fbTextureUnit, v[3], v[4]);
								}
							}
							else {
								glTexCoord2f(v[3], v[4]);
							}
						}
						GLC_Vertex3fv(v);
					}
					GLC_End();
				}
			}

			if (allocations.second_pass_caustics && ((s->flags & SURF_UNDERWATER) || caustics)) {
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
			}

			if (allocations.second_pass_detail && !(s->flags & SURF_UNDERWATER)) {
				s->polys->detail_chain = detail_polys;
				detail_polys = s->polys;
			}

			if (!R_TextureReferenceEqual(fb_texturenum, allocations.null_fb_texture) && gl_fb_bmodels.integer && allocations.fbTextureUnit < 0) {
				if (allocations.useLumaTextures) {
					s->polys->luma_chain = luma_polys[fb_texturenum.index];
					luma_polys[fb_texturenum.index] = s->polys;
					drawlumas = true;
				}
				else {
					s->polys->fb_chain = fullbright_polys[fb_texturenum.index];
					fullbright_polys[fb_texturenum.index] = s->polys;
					drawfullbrights = true;
				}
			}

			if (clear_chains) {
				prev = s;
				s = s->texturechain;
				prev->texturechain = NULL;
			}
			else {
				s = s->texturechain;
			}
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	if (gl_fb_bmodels.integer) {
		if (allocations.lmTextureUnit < 0) {
			GLC_BlendLightmaps();
		}
		if (drawfullbrights) {
			GLC_RenderFullbrights();
		}
		if (drawlumas) {
			GLC_RenderLumas();
		}
	}
	else {
		if (drawlumas) {
			GLC_RenderLumas();
		}
		if (allocations.lmTextureUnit < 0) {
			GLC_BlendLightmaps();
		}
		if (drawfullbrights) {
			GLC_RenderFullbrights();
		}
	}

	GLC_EmitCausticsPolys();
	GLC_EmitDetailPolys(use_vbo);

	R_TraceLeaveFunctionRegion;
}

//#define GLC_WORLD_TEXTURELESS    1
#define GLC_WORLD_FULLBRIGHTS    2
#define GLC_WORLD_LUMATEXTURES   4
#define GLC_WORLD_DETAIL         8
#define GLC_WORLD_CAUSTICS      16
#define GLC_WORLD_LIGHTMAPS     32
#define GLC_WORLD_DF_NORMAL     64
#define GLC_WORLD_DF_TINTED    128
#define GLC_WORLD_DF_BRIGHT    256
#define GLC_WORLD_DF_FLOORS    512
#define GLC_WORLD_DF_WALLS    1024

#define GLC_USE_FULLBRIGHT_TEX  (GLC_WORLD_LUMATEXTURES | GLC_WORLD_FULLBRIGHTS)
#define GLC_USE_DRAWFLAT        (GLC_WORLD_DF_NORMAL | GLC_WORLD_DF_TINTED | GLC_WORLD_DF_BRIGHT)

static qbool GLC_WorldTexturedProgramCompile(texture_unit_allocation_t* allocations, model_t* model)
{
	extern cvar_t gl_lumatextures, gl_textureless;
	int options;
	qbool drawLumas = false;
	
	memset(allocations, 0, sizeof(*allocations));
	GLC_WorldAllocateTextureUnits(allocations, model, true, true);
	allocations->rendering_state = r_state_world_material_lightmap_luma;

	drawLumas = allocations->couldUseLumaTextures;

	options =
		(allocations->lmTextureUnit >= 0 ? GLC_WORLD_LIGHTMAPS : 0) |
		(drawLumas ? GLC_WORLD_LUMATEXTURES : 0) |
		(allocations->fbTextureUnit >= 0 && gl_fb_bmodels.integer ? GLC_WORLD_FULLBRIGHTS : 0) |
		(allocations->detailTextureUnit >= 0 ? GLC_WORLD_DETAIL : 0) |
		(allocations->causticTextureUnit >= 0 ? GLC_WORLD_CAUSTICS : 0) |
		(r_drawflat.integer && r_drawflat_mode.integer == 0 ? GLC_WORLD_DF_NORMAL : 0) |
		(r_drawflat.integer && r_drawflat_mode.integer == 1 ? GLC_WORLD_DF_TINTED : 0) |
		(r_drawflat.integer && r_drawflat_mode.integer == 2 ? GLC_WORLD_DF_BRIGHT : 0) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 2 ? GLC_WORLD_DF_FLOORS : 0) |
		(r_drawflat.integer == 1 || r_drawflat.integer == 3 ? GLC_WORLD_DF_WALLS : 0); 

	if (R_ProgramRecompileNeeded(r_program_world_textured_glc, options)) {
		int sampler = 0;
		char definitions[2048] = { 0 };

		allocations->matTextureUnit = 0;

		if (options & GLC_WORLD_LUMATEXTURES) {
			strlcat(definitions, "#define DRAW_LUMA_TEXTURES\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_FULLBRIGHTS) {
			strlcat(definitions, "#define DRAW_FULLBRIGHT_TEXTURES\n", sizeof(definitions));
		}
		if (options & GLC_USE_FULLBRIGHT_TEX) {
			strlcat(definitions, "#define DRAW_EXTRA_TEXTURES\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_CAUSTICS) {
			strlcat(definitions, "#define DRAW_CAUSTICS\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_DETAIL) {
			strlcat(definitions, "#define DRAW_DETAIL\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_LIGHTMAPS) {
			strlcat(definitions, "#define DRAW_LIGHTMAPS\n", sizeof(definitions));
		}
		if (glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS) {
			strlcat(definitions, "#define EZ_USE_TEXTURE_ARRAYS\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_DF_NORMAL) {
			strlcat(definitions, "#define DRAW_DF_NORMAL\n", sizeof(definitions));
		}
		else if (options & GLC_WORLD_DF_TINTED) {
			strlcat(definitions, "#define DRAW_DRAWFLAT_TINTED\n", sizeof(definitions));
		}
		else if (options & GLC_WORLD_DF_BRIGHT) {
			strlcat(definitions, "#define DRAW_DRAWFLAT_BRIGHT\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_DF_FLOORS) {
			strlcat(definitions, "#define DRAW_FLATFLOORS\n", sizeof(definitions));
		}
		if (options & GLC_WORLD_DF_WALLS) {
			strlcat(definitions, "#define DRAW_FLATWALLS\n", sizeof(definitions));
		}

		R_ProgramCompileWithInclude(r_program_world_textured_glc, definitions);

		allocations->causticTextureUnit = (options & GLC_WORLD_CAUSTICS) && allocations->causticTextureUnit >= 0 ? sampler++ : -1;
		allocations->detailTextureUnit = (options & GLC_WORLD_DETAIL) && allocations->detailTextureUnit >= 0 ? sampler++ : -1;
		allocations->lmTextureUnit = (options & GLC_WORLD_LIGHTMAPS) && allocations->lmTextureUnit >= 0 ? sampler++ : -1;
		allocations->matTextureUnit = allocations->matTextureUnit >= 0 ? sampler++ : -1;
		allocations->fbTextureUnit = (options & GLC_USE_FULLBRIGHT_TEX) && allocations->fbTextureUnit >= 0 ? sampler++ : -1;

		R_ProgramUniform1i(r_program_uniform_world_textured_glc_causticSampler, allocations->causticTextureUnit);
		R_ProgramUniform1i(r_program_uniform_world_textured_glc_detailSampler, allocations->detailTextureUnit);
		R_ProgramUniform1i(r_program_uniform_world_textured_glc_lightmapSampler, allocations->lmTextureUnit);
		R_ProgramUniform1i(r_program_uniform_world_textured_glc_texSampler, allocations->matTextureUnit);
		R_ProgramUniform1i(r_program_uniform_world_textured_glc_lumaSampler, allocations->fbTextureUnit);

		R_ProgramSetCustomOptions(r_program_world_textured_glc, options);
	}
	else {
		allocations->causticTextureUnit = R_ProgramUniformGet1i(r_program_uniform_world_textured_glc_causticSampler, -1);
		allocations->detailTextureUnit = R_ProgramUniformGet1i(r_program_uniform_world_textured_glc_detailSampler, -1);
		allocations->lmTextureUnit = R_ProgramUniformGet1i(r_program_uniform_world_textured_glc_lightmapSampler, -1);
		allocations->matTextureUnit = R_ProgramUniformGet1i(r_program_uniform_world_textured_glc_texSampler, -1);
		allocations->fbTextureUnit = R_ProgramUniformGet1i(r_program_uniform_world_textured_glc_lumaSampler, -1);

		allocations->texture_unit_count =
			(allocations->causticTextureUnit >= 0 ? 1 : 0) +
			(allocations->detailTextureUnit >= 0 ? 1 : 0) +
			(allocations->lmTextureUnit >= 0 ? 1 : 0) +
			(allocations->matTextureUnit >= 0 ? 1 : 0) +
			(allocations->fbTextureUnit >= 0 ? 1 : 0);
	}
	R_TraceLogAPICall("--- units      = %d", allocations->texture_unit_count);
	R_TraceLogAPICall("... caustics   = %d", allocations->causticTextureUnit);
	R_TraceLogAPICall("... detail     = %d", allocations->detailTextureUnit);
	R_TraceLogAPICall("... lightmaps  = %d", allocations->lmTextureUnit);
	R_TraceLogAPICall("... materials  = %d", allocations->matTextureUnit);
	R_TraceLogAPICall("... fullbright = %d", allocations->fbTextureUnit);

	if (R_ProgramRecompileNeeded(r_program_world_secondpass_glc, 0)) {
		// we could make multiple copies of the standard textured program,
		//  so that multiple passes use multiple textures
		R_ProgramCompile(r_program_world_secondpass_glc);

		R_ProgramUniform1i(r_program_uniform_world_textured_glc_causticSampler, 0);
	}

	return R_ProgramReady(r_program_world_textured_glc) && R_ProgramReady(r_program_world_secondpass_glc);
}

static void GLC_DrawTextureChains_GLSL(entity_t* ent, model_t *model, qbool caustics, qbool polygonOffset, texture_unit_allocation_t* allocations)
{
	extern cvar_t gl_lumatextures;
	int index_count = 0;
	int i;
	msurface_t *s, *prev;

	qbool requires_fullbright_pass = false;
	qbool requires_luma_pass = false;
	qbool clear_chains = !r_refdef2.drawWorldOutlines;

	qbool texture_change, uniform_change;
	texture_ref current_material = null_texture_reference;
	texture_ref current_material_fb = null_texture_reference;
	texture_ref desired_textures[8] = { { 0 } };
	int current_lightmap = -1;
	float current_lumaScale = -1, current_fbScale = -1;

	R_TraceEnterFunctionRegion;

	GLC_LightmapArrayToggle(true);

	R_ApplyRenderingState(allocations->rendering_state);
	if (ent && ent->alpha) {
		R_CustomColor(ent->alpha, ent->alpha, ent->alpha, ent->alpha);
	}
	if (polygonOffset) {
		R_CustomPolygonOffset(r_polygonoffset_standard);
	}

	// Common textures
	if (allocations->causticTextureUnit >= 0) {
		desired_textures[allocations->causticTextureUnit] = underwatertexture;
	}
	if (allocations->detailTextureUnit >= 0) {
		desired_textures[allocations->detailTextureUnit] = detailtexture;
	}

	for (i = 0; i < model->numtextures; i++) {
		texture_t* t;
		texture_ref fb_texturenum = null_texture_reference;
		float lumaScale = 0.0f, fbScale = 0.0f;

		if (!model->textures[i] || !model->textures[i]->texturechain) {
			continue;
		}

		t = R_TextureAnimation(ent, model->textures[i]);
		if (allocations->useLumaTextures && t->isLumaTexture && gl_fb_bmodels.integer) {
			fb_texturenum = t->fb_texturenum;
			lumaScale = 1.0f;
			//fbScale = 1.0f;
		}
		else if (allocations->fbTextureUnit >= 0 && R_TextureReferenceIsValid(t->fb_texturenum)) {
			fb_texturenum = t->fb_texturenum;
			lumaScale = (allocations->useLumaTextures && t->isLumaTexture && !gl_fb_bmodels.integer ? 1.0f : 0);
			fbScale = (t->isLumaTexture ? 0.0f : 1.0f);
		}
		else {
			fb_texturenum = allocations->null_fb_texture;
		}

		//bind the world texture
		texture_change = !R_TextureReferenceEqual(t->gl_texturenum, current_material);
		texture_change |= !R_TextureReferenceEqual(fb_texturenum, current_material_fb);
		current_material = t->gl_texturenum;
		current_material_fb = fb_texturenum;

		uniform_change = (current_lumaScale != lumaScale);
		uniform_change |= (current_fbScale != fbScale);

		desired_textures[allocations->matTextureUnit] = current_material;
		if (allocations->fbTextureUnit >= 0) {
			desired_textures[allocations->fbTextureUnit] = current_material_fb;
		}

		s = model->textures[i]->texturechain;
		while (s) {
			if (!(s->texinfo->flags & TEX_SPECIAL)) {
				if (allocations->lmTextureUnit >= 0) {
					texture_change |= !R_TextureReferenceEqual(GLC_LightmapTexture(s->lightmaptexturenum), GLC_LightmapTexture(current_lightmap));

					desired_textures[allocations->lmTextureUnit] = GLC_LightmapTexture(s->lightmaptexturenum);
					current_lightmap = s->lightmaptexturenum;
				}
				else {
					GLC_AddToLightmapChain(s);
				}

				if (texture_change) {
					if (index_count) {
						GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						index_count = 0;
					}

					renderer.TextureUnitMultiBind(0, allocations->texture_unit_count, desired_textures);
					if (r_lightmap_lateupload.integer && allocations->lmTextureUnit >= 0) {
						R_UploadLightMap(allocations->lmTextureUnit, current_lightmap);
					}

					texture_change = false;
				}
				if (uniform_change) {
					if (index_count) {
						GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						index_count = 0;
					}
					if (current_lumaScale != lumaScale) {
						R_ProgramUniform1f(r_program_uniform_world_textured_glc_lumaScale, current_lumaScale = lumaScale);
					}
					if (current_fbScale != fbScale) {
						R_ProgramUniform1f(r_program_uniform_world_textured_glc_fbScale, current_fbScale = fbScale);
					}
					uniform_change = false;
				}

				index_count = GLC_DrawIndexedPoly(s->polys, modelIndexes, modelIndexMaximum, index_count);
			}

			if (allocations->second_pass_caustics && ((s->flags & SURF_UNDERWATER) || caustics)) {
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
			}

			if (allocations->second_pass_detail && !(s->flags & SURF_UNDERWATER)) {
				s->polys->detail_chain = detail_polys;
				detail_polys = s->polys;
			}

			if (allocations->second_pass_luma && !R_TextureReferenceEqual(fb_texturenum, allocations->null_fb_texture)) {
				if (allocations->useLumaTextures) {
					s->polys->luma_chain = luma_polys[fb_texturenum.index];
					luma_polys[fb_texturenum.index] = s->polys;
					requires_luma_pass = true;
				}
				else {
					s->polys->fb_chain = fullbright_polys[fb_texturenum.index];
					fullbright_polys[fb_texturenum.index] = s->polys;
					requires_fullbright_pass = true;
				}
			}

			if (clear_chains) {
				prev = s;
				s = s->texturechain;
				prev->texturechain = NULL;
			}
			else {
				s = s->texturechain;
			}
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	if (gl_fb_bmodels.integer) {
		if (allocations->lmTextureUnit < 0) {
			GLC_BlendLightmaps_GLSL();
		}
		if (requires_fullbright_pass) {
			GLC_RenderFullbrights_GLSL();
		}
		if (requires_luma_pass) {
			GLC_RenderLumas_GLSL();
		}
	}
	else {
		if (requires_luma_pass) {
			GLC_RenderLumas_GLSL();
		}
		if (allocations->lmTextureUnit < 0) {
			GLC_BlendLightmaps_GLSL();
		}
		if (requires_fullbright_pass) {
			GLC_RenderFullbrights_GLSL();
		}
	}

	if (allocations->second_pass_caustics) {
		GLC_EmitCausticsPolys();
	}
	if (allocations->second_pass_detail) {
		GLC_EmitDetailPolys_GLSL();
	}

	R_TraceLeaveFunctionRegion;
}

static void GLC_DrawTextureChains(entity_t* ent, model_t *model, qbool caustics, qbool polygonOffset)
{
	extern cvar_t gl_program_world, gl_textureless;
	texture_unit_allocation_t allocations;

	if (r_drawflat.integer == 1 && r_drawflat_mode.integer == 0) {
		// Guaranteed to be no texturing
		return;
	}

	if (gl_program_world.integer && buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_WorldTexturedProgramCompile(&allocations, model)) {
		R_ProgramUse(r_program_world_textured_glc);
		R_ProgramUniform1f(r_program_uniform_world_textured_glc_time, cl.time);
		R_ProgramUniform3f(r_program_uniform_world_textured_glc_r_floorcolor, r_floorcolor.color[0] / 255.0f, r_floorcolor.color[1] / 255.0f, r_floorcolor.color[2] / 255.0f);
		R_ProgramUniform3f(r_program_uniform_world_textured_glc_r_wallcolor, r_wallcolor.color[0] / 255.0f, r_wallcolor.color[1] / 255.0f, r_wallcolor.color[2] / 255.0f);
		R_ProgramUniform1f(r_program_uniform_world_textures_glc_texture_multiplier, model->isworldmodel && gl_textureless.integer ? 0.0f : 1.0f);

		GLC_DrawTextureChains_GLSL(ent, model, caustics, polygonOffset, &allocations);
		R_ProgramUse(r_program_none);
	}
	else {
		if (model->texturechains_have_lumas) {
			GLC_DrawTextureChains_Immediate(ent, model, caustics, polygonOffset, true);
		}
		GLC_DrawTextureChains_Immediate(ent, model, caustics, polygonOffset, false);
	}
}

qbool GLC_PreCompileWorldPrograms(void)
{
	extern cvar_t gl_program_world;
	texture_unit_allocation_t allocations;

	return (gl_program_world.integer && buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_WorldTexturedProgramCompile(&allocations, cl.worldmodel));
}

// This is now very similar to GLC_DrawBrushModel, but we don't draw sky surfaces at this stage
//  (they've already been drawn)
// Also water surfaces are drawn immediately after if opaque, but after entities if not (on sub/instanced models
//   water surfaces are always drawn during the texture chaining phase, which is something to fix)
void GLC_DrawWorld(void)
{
	extern msurface_t* alphachain;

	R_TraceEnterFunctionRegion;

	GLC_DrawTextureChains(NULL, cl.worldmodel, false, false);
	GLC_DrawFlat(cl.worldmodel, false);
	GLC_DrawMapOutline(cl.worldmodel);
	GLC_DrawAlphaChain(alphachain, polyTypeWorldModel);

	R_TraceLeaveFunctionRegion;
}

void GLC_DrawBrushModel(entity_t* e, qbool polygonOffset, qbool caustics)
{
	extern msurface_t* alphachain;

	GLC_DrawTextureChains(e, e->model, caustics, polygonOffset);
	GLC_DrawFlat(e->model, polygonOffset);
	GLC_DrawMapOutline(e->model);
	GLC_SkyDrawChainedSurfaces();
	GLC_DrawAlphaChain(alphachain, polyTypeBrushModel);
}

/*
// This populates VBO, splitting up by lightmap for efficient
//   rendering when not using texture arrays
int GLC_PopulateVBOForBrushModel(model_t* m, float* vbo_buffer, int vbo_pos)
{
	int i, j;
	int combinations = 0;
	int original_pos = vbo_pos;

	for (i = 0; i < m->numtextures; ++i) {
		if (m->textures[i]) {
			m->textures[i]->gl_first_lightmap = -1;
			for (j = 0; j < MAX_LIGHTMAPS; ++j) {
				m->textures[i]->gl_next_lightmap[j] = -1;
			}
		}
	}

	// Order vertices in the VBO by texture & lightmap
	for (i = 0; i < m->numtextures; ++i) {
		int lightmap = -1;
		int length = 0;
		int surface_count = 0;
		int tex_vbo_start = vbo_pos;

		if (!m->textures[i]) {
			continue;
		}

		// Find first lightmap for this texture
		for (j = 0; j < m->numsurfaces; ++j) {
			msurface_t* surf = m->surfaces + j;

			if (surf->texinfo->miptex != i) {
				continue;
			}

			if (!(surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
				if (surf->texinfo->flags & TEX_SPECIAL) {
					continue;
				}
			}

			if (surf->lightmaptexturenum >= 0 && (lightmap < 0 || surf->lightmaptexturenum < lightmap)) {
				lightmap = surf->lightmaptexturenum;
			}
		}

		m->textures[i]->gl_first_lightmap = lightmap;

		// Build the VBO in order of lightmaps...
		while (lightmap >= 0) {
			int next_lightmap = -1;

			length = 0;
			m->textures[i]->gl_vbo_start[lightmap] = vbo_pos / VERTEXSIZE;
			++combinations;

			for (j = 0; j < m->numsurfaces; ++j) {
				msurface_t* surf = m->surfaces + j;
				glpoly_t* poly;

				if (surf->texinfo->miptex != i) {
					continue;
				}
				if (surf->lightmaptexturenum > lightmap && (next_lightmap < 0 || surf->lightmaptexturenum < next_lightmap)) {
					next_lightmap = surf->lightmaptexturenum;
				}

				if (surf->lightmaptexturenum == lightmap) {
					// copy verts into buffer (alternate to turn fan into triangle strip)
					for (poly = surf->polys; poly; poly = poly->next) {
						int end_vert = 0;
						int start_vert = 1;
						int output = 0;
						int material = m->textures[i]->gl_texture_index;
						float scaleS = m->textures[i]->gl_texture_scaleS;
						float scaleT = m->textures[i]->gl_texture_scaleT;

						if (!poly->numverts) {
							continue;
						}

						// Store position for drawing individual polys
						poly->vbo_start = vbo_pos / VERTEXSIZE;
						vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[0], surf->lightmaptexturenum, material, scaleS, scaleT);
						++output;

						start_vert = 1;
						end_vert = poly->numverts - 1;

						while (start_vert <= end_vert) {
							vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[start_vert], surf->lightmaptexturenum, material, scaleS, scaleT);
							++output;

							if (start_vert < end_vert) {
								vbo_pos = CopyVertToBuffer(vbo_buffer, vbo_pos, poly->verts[end_vert], surf->lightmaptexturenum, material, scaleS, scaleT);
								++output;
							}

							++start_vert;
							--end_vert;
						}

						length += poly->numverts;
						++surface_count;
					}
				}
			}

			m->textures[i]->gl_vbo_length[lightmap] = length;
			m->textures[i]->gl_next_lightmap[lightmap] = next_lightmap;
			lightmap = next_lightmap;
		}
	}

	Con_Printf("%s = %d verts, reserved %d\n", m->name, (vbo_pos - original_pos) / VERTEXSIZE, GLM_MeasureVBOSizeForBrushModel(m));
	return vbo_pos;
}
*/

static void GLC_BlendLightmaps_GLSL(void)
{
	int i;

	GLC_StateBeginBlendLightmaps(true);
	R_ProgramUse(r_program_world_secondpass_glc);

	for (i = 0; i < GLC_LightmapCount(); i++) {
		GLuint index_count = 0;
		glpoly_t *p = GLC_LightmapChain(i);

		if (p) {
			GLC_LightmapUpdate(i);
			renderer.TextureUnitBind(0, GLC_LightmapTexture(i));

			for (; p; p = p->chain) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}

			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}
		}
	}

	GLC_ClearLightmapPolys();
}

static void GLC_BlendLightmaps(void)
{
	int i, j;
	glpoly_t *p;
	float *v;
	qbool use_vbo = buffers.supported && modelIndexes;

	R_ProgramUse(r_program_none);
	GLC_StateBeginBlendLightmaps(use_vbo);
	
	for (i = 0; i < GLC_LightmapCount(); i++) {
		if (!(p = GLC_LightmapChain(i))) {
			continue;
		}

		GLC_LightmapUpdate(i);
		renderer.TextureUnitBind(0, GLC_LightmapTexture(i));
		if (use_vbo) {
			GLuint index_count = 0;

			for (; p; p = p->chain) {
				index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
			}

			if (index_count) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			}
		}
		else {
			for (; p; p = p->chain) {
				GLC_Begin(GL_TRIANGLE_STRIP);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					glTexCoord2f(v[5], v[6]);
					GLC_Vertex3fv(v);
				}
				GLC_End();
			}
		}
	}
	GLC_ClearLightmapPolys();
}

//draws transparent textures for HL world and nonworld models
void GLC_DrawAlphaChain(msurface_t* alphachain, frameStatsPolyType polyType)
{
	int k;
	msurface_t *s;
	float *v;

	if (!alphachain) {
		return;
	}

	R_ProgramUse(r_program_none);
	GLC_StateBeginAlphaChain();

	for (s = alphachain; s; s = s->texturechain) {
		++frameStats.classic.polycount[polyType];
		R_RenderDynamicLightmaps(s, false);

		GLC_StateBeginAlphaChainSurface(s);

		GLC_Begin(GL_TRIANGLE_STRIP);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				GLC_MultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);
				GLC_MultiTexCoord2f(GL_TEXTURE1, v[5], v[6]);
			}
			else {
				glTexCoord2f(v[3], v[4]);
			}
			GLC_Vertex3fv(v);
		}
		GLC_End();
	}
	alphachain = NULL;
}

int GLC_BrushModelCopyVertToBuffer(model_t* mod, void* vbo_buffer_, int position, float* source, int lightmap, int material, float scaleS, float scaleT, msurface_t* surf, qbool has_fb_texture, qbool has_luma_texture)
{
	glc_vbo_world_vert_t* target = (glc_vbo_world_vert_t*)vbo_buffer_ + position;

	VectorCopy(source, target->position);
	target->material_coords[0] = source[3];
	target->material_coords[1] = source[4];
	target->lightmap_coords[0] = source[5];
	target->lightmap_coords[1] = source[6];
	target->lightmap_coords[2] = lightmap;
	target->detail_coords[0] = source[7];
	target->detail_coords[1] = source[8];
	if (scaleS) {
		target->material_coords[0] *= scaleS;
	}
	if (scaleT) {
		target->material_coords[1] *= scaleT;
	}

	target->flatstyle = 0;
	if (surf->flags & SURF_DRAWSKY) {
		target->flatstyle = 1;
	}
	else if (surf->flags & SURF_DRAWTURB) {
		if (surf->texinfo->texture->turbType == TEXTURE_TURB_WATER) {
			target->flatstyle = 2;
		}
		else if (surf->texinfo->texture->turbType == TEXTURE_TURB_SLIME) {
			target->flatstyle = 4;
		}
		else if (surf->texinfo->texture->turbType == TEXTURE_TURB_LAVA) {
			target->flatstyle = 8;
		}
		else if (surf->texinfo->texture->turbType == TEXTURE_TURB_TELE) {
			target->flatstyle = 16;
		}
		else if (surf->texinfo->texture->turbType == TEXTURE_TURB_SKY) {
			target->flatstyle = 32;
		}
	}
	else if (mod->isworldmodel) {
		target->flatstyle = (surf->flags & SURF_DRAWFLAT_FLOOR ? 64 : 128);
	}

	target->flatstyle += (has_luma_texture ? 256 : 0);
	target->flatstyle += (mod->isworldmodel && (surf->flags & SURF_UNDERWATER)) ? 512 : 0;
	target->flatstyle += (has_fb_texture ? 1024 : 0);

	return position + 1;
}

void GLC_ChainBrushModelSurfaces(model_t* clmodel, entity_t* ent)
{
	extern void GLC_EmitWaterPoly(msurface_t* fa);
	qbool glc_first_water_poly = true;
	msurface_t* psurf;
	int i;
	qbool drawFlatFloors = clmodel->isworldmodel && (r_drawflat.integer == 2 || r_drawflat.integer == 1) && r_drawflat_mode.integer == 0;
	qbool drawFlatWalls = clmodel->isworldmodel && (r_drawflat.integer == 3 || r_drawflat.integer == 1) && r_drawflat_mode.integer == 0;
	extern msurface_t* skychain;
	extern msurface_t* alphachain;

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		mplane_t* pplane = psurf->plane;
		float dot = PlaneDiff(modelorg, pplane);
		qbool isFloor = (psurf->flags & SURF_DRAWFLAT_FLOOR);

		//draw the water surfaces now, and setup sky/normal chains
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {
			if (psurf->flags & SURF_DRAWSKY) {
				if (r_fastsky.integer) {
					CHAIN_SURF_B2F(psurf, clmodel->drawflat_chain);
					clmodel->drawflat_todo = true;
				}
				else {
					CHAIN_SURF_B2F(psurf, skychain);
				}
			}
			else if (psurf->flags & SURF_DRAWTURB) {
				if (glc_first_water_poly) {
					GLC_StateBeginWaterSurfaces();
					glc_first_water_poly = false;
				}
				GLC_EmitWaterPoly(psurf);
			}
			else if (psurf->flags & SURF_DRAWALPHA) {
				CHAIN_SURF_B2F(psurf, alphachain);
			}
			else {
				if (drawFlatFloors && isFloor) {
					R_AddDrawflatChainSurface(psurf, false);
					clmodel->drawflat_todo = true;
				}
				else if (drawFlatWalls && !isFloor) {
					R_AddDrawflatChainSurface(psurf, true);
					clmodel->drawflat_todo = true;
				}
				else {
					chain_surfaces_by_lightmap(&psurf->texinfo->texture->texturechain, psurf);

					clmodel->texturechains_have_lumas |= R_TextureAnimation(ent, psurf->texinfo->texture)->isLumaTexture;
					clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
					clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
				}
			}
		}
	}
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
