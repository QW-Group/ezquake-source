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
#include "r_state.h"

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

static rendering_state_t brushModelStates[multitexture_options][fb_options][tex_options];
static rendering_state_t drawFlatLightmapState;
static rendering_state_t drawFlatNoLightmapState;

void R_InitialiseBrushModelStates(void)
{
	rendering_state_t* current;
	rendering_state_t* base;

	R_InitRenderingState(current = &drawFlatNoLightmapState, true, "drawFlatNoLightmapState");
	current->fog.enabled = true;

	R_InitRenderingState(current = &drawFlatLightmapState, true, "drawFlatLightmapState");
	current->fog.enabled = true;
	current->textureUnits[0].enabled = true;
	current->textureUnits[0].mode = r_texunit_mode_blend;

	// Single-texture: all of these are the same so we don't need to bother about others
	R_InitRenderingState(current = &brushModelStates[multitexture_none][0][0], true, "world:singletex");
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);

	// no fullbrights, 2 units
	R_InitRenderingState(base = current = &brushModelStates[multitexture_2units][fb_none][tex_lightmap_and_luma], true, "world:multitex[2][lm+luma]");
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, true, r_texunit_mode_add);

	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_none][tex_no_lightmap], base, "world:multitex[2][luma]");
	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_none][tex_no_luma], base, "world:multitex[2][lm]");
	current->textureUnits[1].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_none][tex_no_lightmap_or_luma], base, "world:multitex[2][_]");
	current->textureUnits[1].enabled = false;

	// no fullbrights, 3 units
	R_InitRenderingState(base = current = &brushModelStates[multitexture_3units][fb_none][tex_lightmap_and_luma], true, "world:multitex[3][lm+luma]");
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, true, r_texunit_mode_add);
	R_GLC_TextureUnitSet(current, 2, true, r_texunit_mode_blend);

	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_none][tex_no_lightmap], base, "world:multitex[3][luma]");
	current->textureUnits[2].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_none][tex_no_luma], base, "world:multitex[3][lm]");
	current->textureUnits[1].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_none][tex_no_lightmap_or_luma], base, "world:multitex[3][_]");
	current->textureUnits[1].enabled = false;
	current->textureUnits[2].enabled = false;

	// lumas enabled, 2 units
	R_InitRenderingState(base = current = &brushModelStates[multitexture_2units][fb_lumas][tex_lightmap_and_luma], true, "world:multitex-luma[2][lm+luma]");
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, true, r_texunit_mode_blend);

	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_lumas][tex_no_lightmap], base, "world:multitex-luma[2][luma]");
	current->textureUnits[1].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_lumas][tex_no_luma], base, "world:multitex-luma[2][lm]");
	R_CopyRenderingState(current = &brushModelStates[multitexture_2units][fb_lumas][tex_no_lightmap_or_luma], base, "world:multitex-luma[2][_]");
	current->textureUnits[1].enabled = false;

	// lumas enabled, 3 units
	R_InitRenderingState(base = current = &brushModelStates[multitexture_3units][fb_lumas][tex_lightmap_and_luma], true, "world:multitex-luma[3][lm+luma]");
	R_GLC_TextureUnitSet(current, 0, true, r_texunit_mode_replace);
	R_GLC_TextureUnitSet(current, 1, true, r_texunit_mode_blend);
	R_GLC_TextureUnitSet(current, 2, true, r_texunit_mode_add);

	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_lumas][tex_no_lightmap], base, "world:multitex-luma[3][luma]");
	current->textureUnits[1].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_lumas][tex_no_luma], base, "world:multitex-luma[3][lm]");
	current->textureUnits[2].enabled = false;
	R_CopyRenderingState(current = &brushModelStates[multitexture_3units][fb_lumas][tex_no_lightmap_or_luma], base, "world:multitex-luma[3][_]");
	current->textureUnits[1].enabled = false;
	current->textureUnits[2].enabled = false;

	// in 2unit mode 2nd unit is lightmap: so no changes required
	R_CopyRenderingState(&brushModelStates[multitexture_2units][fb_only][tex_lightmap_and_luma], &brushModelStates[multitexture_2units][fb_lumas][tex_lightmap_and_luma], "world:multitex-fb[2][lm+luma]");
	R_CopyRenderingState(&brushModelStates[multitexture_2units][fb_only][tex_no_lightmap_or_luma], &brushModelStates[multitexture_2units][fb_lumas][tex_no_lightmap_or_luma], "world:multitex-fb[2][_]");
	R_CopyRenderingState(&brushModelStates[multitexture_2units][fb_only][tex_no_lightmap], &brushModelStates[multitexture_2units][fb_lumas][tex_no_lightmap], "world:multitex-fb[2][luma]");
	R_CopyRenderingState(&brushModelStates[multitexture_2units][fb_only][tex_no_luma], &brushModelStates[multitexture_2units][fb_lumas][tex_no_luma], "world:multitex-fb[2][lm+luma]");

	// lumas not enabled, fb only... copy from lumas but change texunit_mode_add => texunit_mode_decal
	R_CopyRenderingState(&brushModelStates[multitexture_3units][fb_only][tex_lightmap_and_luma], &brushModelStates[multitexture_3units][fb_lumas][tex_lightmap_and_luma], "world:multitex-fb[3][lm+luma]");
	R_CopyRenderingState(&brushModelStates[multitexture_3units][fb_only][tex_no_lightmap_or_luma], &brushModelStates[multitexture_3units][fb_lumas][tex_no_lightmap_or_luma], "world:multitex-fb[3][_]");
	R_CopyRenderingState(&brushModelStates[multitexture_3units][fb_only][tex_no_lightmap], &brushModelStates[multitexture_3units][fb_lumas][tex_no_lightmap], "world:multitex-fb[3][luma]");
	R_CopyRenderingState(&brushModelStates[multitexture_3units][fb_only][tex_no_luma], &brushModelStates[multitexture_3units][fb_lumas][tex_no_luma], "world:multitex-fb[3][lm+luma]");
	brushModelStates[multitexture_3units][fb_only][tex_lightmap_and_luma].textureUnits[2].mode = r_texunit_mode_decal;
	brushModelStates[multitexture_3units][fb_only][tex_no_lightmap_or_luma].textureUnits[2].mode = r_texunit_mode_decal;
	brushModelStates[multitexture_3units][fb_only][tex_no_lightmap].textureUnits[2].mode = r_texunit_mode_decal;
	brushModelStates[multitexture_3units][fb_only][tex_no_luma].textureUnits[2].mode = r_texunit_mode_decal;
}

static void GLC_BlendLightmaps(void);
static void GLC_DrawTextureChains(model_t *model, qbool caustics);
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
	qbool use_vbo = GL_BuffersSupported() && modelIndexes;
	int last_lightmap = -2;

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
				glColor3ubv(desired);
			}

			if (last_lightmap != new_lightmap) {
				if (new_lightmap >= 0) {
					R_ApplyRenderingState(&drawFlatLightmapState);
					GLC_SetTextureLightmap(GL_TEXTURE0, new_lightmap);
				}
				else {
					R_ApplyRenderingState(&drawFlatNoLightmapState);
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
						glBegin(GL_POLYGON);
						for (k = 0; k < p->numverts; k++, v += VERTEXSIZE) {
							if (new_lightmap >= 0) {
								glTexCoord2f(v[5], v[6]);
							}
							glVertex3fv(v);
						}
						glEnd();
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

	GLC_StateEndDrawFlatModel();

	// START shaman FIX /r_drawflat + /gl_caustics {
	GLC_EmitCausticsPolys(use_vbo);
	// } END shaman FIX /r_drawflat + /gl_caustics
}

static void GLC_DrawTextureChains(model_t *model, qbool caustics)
{
	extern cvar_t gl_lumaTextures;
	extern cvar_t gl_textureless;
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	int index_count = 0;
	rendering_state_t* lumaState = &brushModelStates[multitexture_none][0][0];
	rendering_state_t* noLumaState = &brushModelStates[multitexture_none][0][0];
	rendering_state_t* noLightmapState = &brushModelStates[multitexture_none][0][0];
	rendering_state_t* noLightmapOrLumaState = &brushModelStates[multitexture_none][0][0];

	texture_ref fb_texturenum = null_texture_reference;
	int waterline, i, k;
	msurface_t *s;
	float *v;

	qbool draw_caustics = GL_TextureReferenceIsValid(underwatertexture) && gl_caustics.value;
	qbool draw_details = GL_TextureReferenceIsValid(detailtexture) && gl_detail.value;
	qbool isLumaTexture;
	qbool use_vbo = GL_BuffersSupported() && modelIndexes;

	qbool drawfullbrights = false;
	qbool drawlumas = false;
	qbool useLumaTextures = gl_lumaTextures.integer && r_refdef2.allow_lumas;

	qbool texture_change;
	texture_ref current_material = null_texture_reference;
	texture_ref current_material_fb = null_texture_reference;
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
		int units = gl_textureunits >= 3 ? multitexture_3units : multitexture_2units;
		int fb = fb_none;
		texture_unit_count = gl_textureunits >= 3 ? 3 : 2;

		if (!gl_fb_bmodels.integer) {
			fbTextureUnit = 1;
			lmTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
		}
		else {
			fb = (useLumaTextures ? fb_lumas : fb_only);

			lmTextureUnit = 1;
			fbTextureUnit = (gl_textureunits >= 3 ? 2 : -1);
		}

		lumaState = &brushModelStates[units][fb][tex_lightmap_and_luma];
		noLumaState = &brushModelStates[units][fb][tex_no_luma];
		noLightmapState = &brushModelStates[units][fb][tex_no_lightmap];
		noLightmapOrLumaState = &brushModelStates[units][fb][tex_no_lightmap_or_luma];
	}

	GLC_StateBeginDrawTextureChains(lmTextureUnit, fbTextureUnit);
	for (i = 0; i < model->numtextures; i++) {
		texture_t* t;

		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		t = R_TextureAnimation(model->textures[i]);

		if (t->isLumaTexture) {
			isLumaTexture = useLumaTextures;
			if (isLumaTexture) {
				fb_texturenum = t->fb_texturenum;
			}
		}
		else {
			isLumaTexture = false;
			fb_texturenum = t->fb_texturenum;
		}

		//bind the world texture
		texture_change = !GL_TextureReferenceEqual(t->gl_texturenum, current_material);
		texture_change |= !GL_TextureReferenceEqual(t->fb_texturenum, current_material_fb);

		current_material = t->gl_texturenum;
		current_material_fb = t->fb_texturenum;

		desired_textures[0] = current_material;
		if (fbTextureUnit >= 0) {
			desired_textures[fbTextureUnit] = current_material_fb;
		}

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline])) {
				continue;
			}

			for (; s; s = s->texturechain) {
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

					if (fbTextureUnit >= 0 && GL_TextureReferenceIsValid(fb_texturenum)) {
						if (lmTextureUnit >= 0 && current_lightmap != -1) {
							R_ApplyRenderingState(lumaState);
						}
						else {
							R_ApplyRenderingState(noLightmapState);
						}
					}
					else {
						if (lmTextureUnit >= 0 && current_lightmap != -1) {
							R_ApplyRenderingState(noLumaState);
						}
						else {
							R_ApplyRenderingState(noLightmapOrLumaState);
						}
					}
					GL_BindTextures(0, texture_unit_count, desired_textures);

					texture_change = false;
				}

				if (use_vbo) {
					index_count = GLC_DrawIndexedPoly(s->polys, modelIndexes, modelIndexMaximum, index_count);
				}
				else {
					glBegin(GL_POLYGON);
					v = s->polys->verts[0];

					if (!(s->texinfo->flags & TEX_SPECIAL)) {
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
							glVertex3fv(v);
						}
					}
					glEnd();
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

	GLC_EmitCausticsPolys(use_vbo);
	GLC_EmitDetailPolys(use_vbo);

	GLC_StateEndWorldTextureChains(lmTextureUnit, fbTextureUnit);
	GLC_StateEndDrawTextureChains();
}

void GLC_DrawWorld(void)
{
	extern msurface_t* alphachain;

	if (r_drawflat.integer != 1) {
		GLC_DrawTextureChains(cl.worldmodel, false);
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
			GLC_DrawTextureChains(clmodel, caustics);
			GLC_DrawFlat(clmodel);
		}
	}
	else {
		GLC_DrawTextureChains(clmodel, caustics);
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
	qbool use_vbo = GL_BuffersSupported() && modelIndexes;

	GLC_StateBeginBlendLightmaps(use_vbo);

	for (i = 0; i < GLC_LightmapCount(); i++) {
		if (!(p = GLC_LightmapChain(i))) {
			continue;
		}

		GLC_LightmapUpdate(i);
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GLC_LightmapTexture(i));
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
				glBegin(GL_POLYGON);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v += VERTEXSIZE) {
					glTexCoord2f(v[5], v[6]);
					glVertex3fv(v);
				}
				glEnd();
			}
		}
	}
	GLC_ClearLightmapPolys();

	GLC_StateEndBlendLightmaps();
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
		R_RenderDynamicLightmaps(s);

		GLC_StateBeginAlphaChainSurface(s);

		glBegin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
			if (gl_mtexable) {
				qglMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);
				qglMultiTexCoord2f(GL_TEXTURE1, v[5], v[6]);
			}
			else {
				glTexCoord2f(v[3], v[4]);
			}
			glVertex3fv(v);
		}
		glEnd();
	}

	alphachain = NULL;

	GLC_StateEndAlphaChain();
}
