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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "r_framestats.h"
#include "r_texture.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_brushmodel.h"
#include "glc_local.h"
#include "tr_types.h"

extern buffer_ref brushModel_vbo;

extern glpoly_t *fullbright_polys[MAX_GLTEXTURES];
extern glpoly_t *luma_polys[MAX_GLTEXTURES];

glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

enum {
	fb_lumas,
	fb_only,
	fb_none,

	fb_options
};

enum {
	tex_lightmap_and_luma,
	tex_no_lightmap_or_luma,
	tex_no_lightmap,
	tex_no_luma,

	tex_options
};

enum {
	multitexture_none,
	multitexture_2units,
	multitexture_3units,

	multitexture_options
};

//static rendering_state_t brushModelStates[multitexture_options][fb_options][tex_options];

void R_InitialiseBrushModelStates(void)
{
	rendering_state_t* current;

	current = R_InitRenderingState(r_state_drawflat_without_lightmaps_glc, true, "drawFlatNoLightmapState", vao_brushmodel);
	current->fog.enabled = true;

	current = R_InitRenderingState(r_state_drawflat_with_lightmaps_glc, true, "drawFlatLightmapState", vao_brushmodel_lm_unit1);
	current->fog.enabled = true;
	current->textureUnits[0].enabled = true;
	current->textureUnits[0].mode = r_texunit_mode_blend;

	// Single-texture: all of these are the same so we don't need to bother about others
	current = R_InitRenderingState(r_state_world_singletexture_glc, true, "world:singletex", vao_brushmodel);
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);

	// material * lightmap + luma
	current = R_InitRenderingState(r_state_world_material_lightmap_luma, true, "r_state_world_material_lightmap_luma", vao_brushmodel_lm_unit1);
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_blend);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_add);

	current = R_InitRenderingState(r_state_world_material_lightmap_fb, true, "r_state_world_material_lightmap_fb", vao_brushmodel_lm_unit1);
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_blend);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_decal);

	// no fullbrights, 3 units: blend(material + luma, lightmap) 
	current = R_InitRenderingState(r_state_world_material_fb_lightmap, true, "r_state_world_material_fb_lightmap", vao_brushmodel);
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_add);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_blend);

	// lumas enabled, 3 units
	current = R_InitRenderingState(r_state_world_material_luma_lightmap, true, "r_state_world_material_luma_lightmap", vao_brushmodel);
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, glConfig.texture_units >= 2, r_texunit_mode_add);
	R_GLC_TextureUnitSet(current, 2, glConfig.texture_units >= 3, r_texunit_mode_blend);
}

void GLC_EnsureVAOCreated(r_vao_id vao)
{
	if (R_VertexArrayCreated(vao)) {
		return;
	}

	if (!GL_BufferReferenceIsValid(brushModel_vbo)) {
		// TODO: vbo data in client memory
		return;
	}

	R_GenVertexArray(vao);
	GLC_VAOSetVertexBuffer(vao, brushModel_vbo);
	// TODO: index data _not_ in client memory

	switch (vao) {
		case vao_brushmodel:
		{
			// tmus: [material, material2, lightmap]
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 1, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 2, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			break;
		}
		case vao_brushmodel_lm_unit1:
		{
			// tmus: [material, lightmap, material2]
			GLC_VAOSetVertexBuffer(vao, brushModel_vbo);
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 1, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			GLC_VAOEnableTextureCoordPointer(vao, 2, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, material_coords));
			break;
		}
		case vao_brushmodel_drawflat:
		{
			// tmus: [lightmap]
			GLC_VAOSetVertexBuffer(vao, brushModel_vbo);
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
			break;
		}
		case vao_brushmodel_details:
		{
			// tmus: [details]
			GLC_VAOSetVertexBuffer(vao, brushModel_vbo);
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, detail_coords));
			break;
		}
		case vao_brushmodel_lightmap_pass:
		{
			// tmus: [lightmap]
			GLC_VAOSetVertexBuffer(vao, brushModel_vbo);
			GLC_VAOEnableVertexPointer(vao, 3, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, position));
			GLC_VAOEnableTextureCoordPointer(vao, 0, 2, GL_FLOAT, sizeof(glc_vbo_world_vert_t), VBO_FIELDOFFSET(glc_vbo_world_vert_t, lightmap_coords));
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
void GLC_RenderFullbrights(void);
void GLC_RenderLumas(void);
GLuint GLC_DrawIndexedPoly(glpoly_t* p, GLuint* modelIndexes, GLuint modelIndexMaximum, GLuint index_count);

static void GLC_DrawFlat(model_t *model)
{
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	int index_count = 0;

	msurface_t *s;
	int waterline, k;
	float *v;
	byte w[3], f[3], sky[3], current[3], desired[3];
	qbool draw_caustics = GL_TextureReferenceIsValid(underwatertexture) && gl_caustics.value;
	qbool first_surf = true;
	qbool use_vbo = buffers.supported && modelIndexes;
	int last_lightmap = -2;

	GLC_EnsureVAOCreated(vao_brushmodel_drawflat);

	if (!model->drawflat_chain[0] && !model->drawflat_chain[1]) {
		return;
	}

	memcpy(w, r_wallcolor.color, 3);
	memcpy(f, r_floorcolor.color, 3);
	memcpy(sky, r_skycolor.color, 3);
	memcpy(current, color_white, 3);
	memcpy(desired, current, 3);

	for (waterline = 0; waterline < 2; waterline++) {
		for (s = model->drawflat_chain[waterline]; s; s = s->drawflatchain) {
			int new_lightmap = s->lightmaptexturenum;

			if (s->flags & SURF_DRAWSKY) {
				memcpy(desired, sky, 3);
				new_lightmap = -1;
			}
			else if (s->flags & SURF_DRAWTURB) {
				memcpy(desired, SurfaceFlatTurbColor(s->texinfo->texture), 3);
				new_lightmap = -1;
			}
			else if (s->flags & SURF_DRAWFLAT_FLOOR) {
				memcpy(desired, f, 3);
			}
			else {
				memcpy(desired, w, 3);
			}

			if (index_count && last_lightmap != new_lightmap) {
				GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				index_count = 0;
			}

			if (first_surf || desired[0] != current[0] || desired[1] != current[1] || desired[2] != current[2]) {
				if (index_count) {
					GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
				}
				index_count = 0;
				R_CustomColor4ubv(desired);
			}

			if (last_lightmap != new_lightmap) {
				if (new_lightmap >= 0) {
					R_ApplyRenderingState(r_state_drawflat_with_lightmaps_glc);
					R_CustomColor4ubv(desired);
					GLC_SetTextureLightmap(0, new_lightmap);
				}
				else {
					R_ApplyRenderingState(r_state_drawflat_without_lightmaps_glc);
					R_CustomColor4ubv(desired);
				}
			}
			last_lightmap = new_lightmap;

			current[0] = desired[0];
			current[1] = desired[1];
			current[2] = desired[2];
			first_surf = false;

			{
				glpoly_t *p;
				for (p = s->polys; p; p = p->next) {
					v = p->verts[0];

					if (use_vbo) {
						index_count = GLC_DrawIndexedPoly(p, modelIndexes, modelIndexMaximum, index_count);
					}
					else {
						GLC_Begin(GL_POLYGON);
						for (k = 0; k < p->numverts; k++, v += VERTEXSIZE) {
							if (new_lightmap >= 0) {
								glTexCoord2f(v[5], v[6]);
							}
							GLC_Vertex3fv(v);
						}
						GLC_End();
					}
				}
			}

			// START shaman FIX /r_drawflat + /gl_caustics {
			if (waterline && draw_caustics) {
				s->polys->caustics_chain = caustics_polys;
				caustics_polys = s->polys;
			}
			// } END shaman FIX /r_drawflat + /gl_caustics
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	// START shaman FIX /r_drawflat + /gl_caustics {
	GLC_EmitCausticsPolys(use_vbo);
	// } END shaman FIX /r_drawflat + /gl_caustics
}

static void GLC_DrawTextureChains(entity_t* ent, model_t *model, qbool caustics)
{
	extern cvar_t gl_lumaTextures;
	extern cvar_t gl_textureless;
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	int index_count = 0;
	r_state_id state = r_state_world_singletexture_glc;
	texture_ref fb_texturenum = null_texture_reference;
	int waterline, i, k;
	msurface_t *s;
	float *v;

	qbool draw_caustics = GL_TextureReferenceIsValid(underwatertexture) && gl_caustics.value;
	qbool draw_details = GL_TextureReferenceIsValid(detailtexture) && gl_detail.value;
	qbool isLumaTexture;
	qbool use_vbo = buffers.supported && modelIndexes;

	qbool drawfullbrights = false;
	qbool drawlumas = false;
	qbool useLumaTextures = gl_lumaTextures.integer && r_refdef2.allow_lumas;

	qbool texture_change;
	texture_ref current_material = null_texture_reference;
	texture_ref current_material_fb = null_texture_reference;
	texture_ref null_fb_texture = null_texture_reference;
	int current_lightmap = -1;
	int fbTextureUnit = -1;
	int lmTextureUnit = -1;

	texture_ref desired_textures[4];
	int texture_unit_count = 1;

	// if (!gl_fb_bmodels)
	//   (material + fullbright) * lightmap
	// else
	//   material * lightmap + fullbright

	// For each texture we have to draw, it may or may not have a luma, so the old code changed
	//   up the bindings/allocations with each texture (binding lightmap to each unit with luma textures on/off)
	// Instead we keep 0 for material, 1 for (fullbright/lightmap), 2 for (fullbright/lightmap)
	//   and if fullbright/luma texture not available, we enable/disable texture unit 1 or 2
	// Default: no multi-texturing, each pass done at the bottom
	if (gl_mtexable) {
		texture_unit_count = gl_textureunits >= 3 ? 3 : 2;

		if (useLumaTextures && !gl_fb_bmodels.integer) {
			// blend(material + fb) * lightmap
			state = r_state_world_material_fb_lightmap;
			null_fb_texture = solidblack_texture;
			fbTextureUnit = 1;
			lmTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
		}
		else if (useLumaTextures) {
			// blend(material, lightmap) + luma
			state = r_state_world_material_lightmap_luma;
			lmTextureUnit = 1;
			fbTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
			null_fb_texture = solidblack_texture; // GL_ADD adds colors, multiplies alphas
		}
		else {
			// blend(material + luma, lightmap) 
			state = r_state_world_material_lightmap_luma;
			lmTextureUnit = 1;
			fbTextureUnit = -1;
			null_fb_texture = solidblack_texture; // GL_ADD adds colors, multiplies alphas
			texture_unit_count = 2;
		}

		GLC_EnsureVAOCreated(vao_brushmodel);
		GLC_EnsureVAOCreated(vao_brushmodel_lm_unit1);
	}
	else {
		GLC_EnsureVAOCreated(vao_brushmodel);
		state = r_state_world_singletexture_glc;
	}

	R_ApplyRenderingState(state);

	for (i = 0; i < model->numtextures; i++) {
		texture_t* t;

		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		t = R_TextureAnimation(ent, model->textures[i]);
		if (t->isLumaTexture) {
			isLumaTexture = useLumaTextures;
			fb_texturenum = isLumaTexture ? t->fb_texturenum : null_fb_texture;
		}
		else {
			isLumaTexture = false;
			fb_texturenum = null_fb_texture;
		}

		//bind the world texture
		texture_change = !GL_TextureReferenceEqual(t->gl_texturenum, current_material);
		texture_change |= !GL_TextureReferenceEqual(fb_texturenum, current_material_fb);

		current_material = t->gl_texturenum;
		current_material_fb = fb_texturenum;

		desired_textures[0] = current_material;
		if (fbTextureUnit >= 0) {
			desired_textures[fbTextureUnit] = current_material_fb;
		}

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline])) {
				continue;
			}

			for (; s; s = s->texturechain) {
				if (!(s->texinfo->flags & TEX_SPECIAL)) {
					if (lmTextureUnit >= 0) {
						texture_change |= (s->lightmaptexturenum != current_lightmap);

						desired_textures[lmTextureUnit] = GLC_LightmapTexture(s->lightmaptexturenum);
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

						GL_BindTextures(0, texture_unit_count, desired_textures);

						texture_change = false;
					}

					if (use_vbo) {
						index_count = GLC_DrawIndexedPoly(s->polys, modelIndexes, modelIndexMaximum, index_count);
					}
					else {
						v = s->polys->verts[0];

						GLC_Begin(GL_POLYGON);
						for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
							//Tei: textureless for the world brush models (Qrack)
							float tex_s = gl_textureless.value && model->isworldmodel ? 0 : v[3];
							float tex_t = gl_textureless.value && model->isworldmodel ? 0 : v[4];

							if (lmTextureUnit >= 0 || fbTextureUnit >= 0) {
								qglMultiTexCoord2f(GL_TEXTURE0, tex_s, tex_t);

								if (lmTextureUnit >= 0) {
									qglMultiTexCoord2f(GL_TEXTURE0 + lmTextureUnit, v[5], v[6]);
								}

								if (fbTextureUnit >= 0) {
									qglMultiTexCoord2f(GL_TEXTURE0 + fbTextureUnit, tex_s, tex_t);
								}
							}
							else {
								glTexCoord2f(tex_s, tex_t);
							}
							GLC_Vertex3fv(v);
						}
						GLC_End();
					}
				}

				if (draw_caustics && (waterline || caustics)) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}

				if (!waterline && draw_details) {
					s->polys->detail_chain = detail_polys;
					detail_polys = s->polys;
				}

				if (GL_TextureReferenceIsValid(fb_texturenum) && gl_fb_bmodels.integer && fbTextureUnit < 0) {
					if (isLumaTexture) {
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
			}
		}
	}

	if (index_count) {
		GL_DrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
	}

	if (developer.integer) {
		if (gl_fb_bmodels.integer) {
			if (lmTextureUnit < 0) {
				GLC_BlendLightmaps();
			}
			if (drawfullbrights) {
				GLC_RenderFullbrights();
				drawfullbrights = false;
			}
			if (drawlumas) {
				GLC_RenderLumas();
				drawlumas = false;
			}
		}
		else {
			if (drawlumas) {
				GLC_RenderLumas();
				drawlumas = false;
			}
			if (lmTextureUnit < 0) {
				GLC_BlendLightmaps();
			}
			if (drawfullbrights) {
				GLC_RenderFullbrights();
				drawfullbrights = false;
			}
		}
	}

	GLC_EmitCausticsPolys(use_vbo);
	GLC_EmitDetailPolys(use_vbo);
}

void GLC_DrawWorld(void)
{
	extern msurface_t* alphachain;

	if (r_drawflat.integer != 1) {
		GLC_DrawTextureChains(NULL, cl.worldmodel, false);
	}
	if (cl.worldmodel->drawflat_chain[0] || cl.worldmodel->drawflat_chain[1]) {
		GLC_DrawFlat(cl.worldmodel);
	}

	if (R_DrawWorldOutlines()) {
		GLC_DrawMapOutline(cl.worldmodel);
	}

	//draw the world alpha textures
	GLC_DrawAlphaChain(alphachain, polyTypeWorldModel);
}

void GLC_DrawBrushModel(entity_t* e, model_t* clmodel, qbool caustics)
{
	if (r_drawflat.integer && clmodel->isworldmodel) {
		if (r_drawflat.integer == 1) {
			GLC_DrawFlat(clmodel);
		}
		else {
			GLC_DrawTextureChains(e, clmodel, caustics);
			GLC_DrawFlat(clmodel);
		}
	}
	else {
		GLC_DrawTextureChains(e, clmodel, caustics);
	}

	if (clmodel->isworldmodel && R_DrawWorldOutlines()) {
		GLC_DrawMapOutline(clmodel);
	}
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

static void GLC_BlendLightmaps(void)
{
	extern void R_UploadLightMap(GLenum textureUnit, int lightmapnum);

	int i, j;
	glpoly_t *p;
	float *v;
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	qbool use_vbo = buffers.supported && modelIndexes;

	GLC_StateBeginBlendLightmaps(use_vbo);
	
	for (i = 0; i < GLC_LightmapCount(); i++) {
		if (!(p = GLC_LightmapChain(i))) {
			continue;
		}

		GLC_LightmapUpdate(i);
		R_TextureUnitBind(0, GLC_LightmapTexture(i));
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
				GLC_Begin(GL_POLYGON);
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

	GLC_StateBeginAlphaChain();
	for (s = alphachain; s; s = s->texturechain) {
		++frameStats.classic.polycount[polyType];
		R_RenderDynamicLightmaps(s, false);

		GLC_StateBeginAlphaChainSurface(s);

		GLC_Begin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				qglMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);
				qglMultiTexCoord2f(GL_TEXTURE1, v[5], v[6]);
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
