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

extern glpoly_t *fullbright_polys[MAX_GLTEXTURES];
extern glpoly_t *luma_polys[MAX_GLTEXTURES];

glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

static void GLC_BlendLightmaps(void);
static void GLC_DrawTextureChains(model_t *model, qbool caustics);
void GLC_RenderFullbrights(void);
void GLC_RenderLumas(void);

static void GLC_DrawFlat(model_t *model)
{
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	int index_count = 0;

	msurface_t *s;
	int waterline, k;
	float *v;
	byte w[3], f[3], sky[3];
	int lastType = -1;
	qbool draw_caustics = GL_TextureReferenceIsValid(underwatertexture) && gl_caustics.value;
	int last_lightmap = -1;

	if (!model->drawflat_chain[0] && !model->drawflat_chain[1]) {
		return;
	}

	memcpy(w, r_wallcolor.color, 3);
	memcpy(f, r_floorcolor.color, 3);
	memcpy(sky, r_skycolor.color, 3);

	GLC_StateBeginDrawFlatModel();

	for (waterline = 0; waterline < 2; waterline++) {
		if (!(s = model->drawflat_chain[waterline])) {
			continue;
		}

		for (; s; s = s->drawflatchain) {
			if (s->flags & SURF_DRAWSKY) {
				if (lastType != 2) {
					if (index_count) {
						glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						GL_LogAPICall("glDrawElements(switch to sky surfaces)");
					}
					index_count = 0;
					glColor3ubv(sky);
					lastType = 2;
				}
				GLC_EnsureTMUDisabled(GL_TEXTURE0);
			}
			else if (s->flags & SURF_DRAWTURB) {
				if (index_count) {
					glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
					GL_LogAPICall("glDrawElements(turb surfaces)");
				}
				index_count = 0;
				GLC_EnsureTMUDisabled(GL_TEXTURE0);
				glColor3ubv(SurfaceFlatTurbColor(s->texinfo->texture));
				lastType = -1;
			}
			else {
				if (last_lightmap != s->lightmaptexturenum) {
					if (index_count) {
						glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						GL_LogAPICall("glDrawElements(lightmap switch)");
					}
					index_count = 0;
				}
				GLC_SetTextureLightmap(GL_TEXTURE0, last_lightmap = s->lightmaptexturenum);
				GLC_EnsureTMUEnabled(GL_TEXTURE0);
				if (s->flags & SURF_DRAWFLAT_FLOOR) {
					if (lastType != 0) {
						if (index_count) {
							glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
							GL_LogAPICall("glDrawElements(switch to floor)");
						}
						index_count = 0;
						glColor3ubv(f);
						lastType = 0;
					}
				}
				else if (lastType != 1) {
					if (index_count) {
						glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
					}
					index_count = 0;
					glColor3ubv(w);
					lastType = 1;
				}
			}

			{
				glpoly_t *p;
				for (p = s->polys; p; p = p->next) {
					v = p->verts[0];

					if (GL_BuffersSupported()) {
						if (index_count + 1 + p->numverts > modelIndexMaximum) {
							glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
							GL_LogAPICall("glDrawElements(index buffer full)");
							index_count = 0;
						}

						if (index_count) {
							modelIndexes[index_count++] = ~(GLuint)0;
						}

						for (k = 0; k < p->numverts; ++k) {
							modelIndexes[index_count++] = p->vbo_start + k;
						}
					}
					else {
						glBegin(GL_POLYGON);
						for (k = 0; k < p->numverts; k++, v += VERTEXSIZE) {
							glTexCoord2f(v[5], v[6]);
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
		glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
		GL_LogAPICall("glDrawElements(flush)");
	}

	GLC_StateEndDrawFlatModel();

	// START shaman FIX /r_drawflat + /gl_caustics {
	EmitCausticsPolys();
	// } END shaman FIX /r_drawflat + /gl_caustics
}

static void GLC_DrawTextureChains(model_t *model, qbool caustics)
{
	extern cvar_t gl_lumaTextures;
	extern cvar_t gl_textureless;
	extern buffer_ref brushModel_vbo;
	extern GLuint* modelIndexes;
	extern GLuint modelIndexMaximum;
	int index_count = 0;

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

	GLenum materialTextureUnit = GL_TEXTURE0;
	GLenum lightmapTextureUnit = 0;
	GLenum fullbrightTextureUnit = 0;
	GLenum fullbrightMode;

	qbool texture_change;
	texture_ref current_material = null_texture_reference;
	texture_ref current_material_fb = null_texture_reference;
	int current_lightmap = -1;

	// if (!gl_fb_bmodels)
	//   (material + fullbright) * lightmap
	// else
	//   material * lightmap + fullbright

	// For each texture we have to draw, it may or may not have a luma, so the old code changed
	//   up the bindings/allocations with each texture (binding lightmap to each unit with luma textures on/off)
	// Instead we keep 0 for material, 1 for (fullbright/lightmap), 2 for (fullbright/lightmap)
	//   and if fullbright/luma texture not available, we enable/disable texture unit 1 or 2
	// Default: no multi-texturing, each pass done at the bottom
	if (gl_mtexable && !gl_fb_bmodels.integer) {
		fullbrightTextureUnit = GL_TEXTURE1;
		fullbrightMode = GL_ADD;

		if (gl_textureunits >= 3) {
			lightmapTextureUnit = GL_TEXTURE2;
		}
	}
	else if (gl_mtexable && gl_fb_bmodels.integer) {
		lightmapTextureUnit = GL_TEXTURE1;

		if (gl_textureunits >= 3) {
			fullbrightTextureUnit = GL_TEXTURE2;
			fullbrightMode = useLumaTextures ? GL_ADD : GL_DECAL;
		}
	}

	GLC_StateBeginDrawTextureChains(model, lightmapTextureUnit, fullbrightTextureUnit, fullbrightMode);
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
		if (index_count && texture_change) {
			glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
			GL_LogAPICall("glDrawElements(lightmap switch)");
			index_count = 0;
		}

		GL_EnsureTextureUnitBound(materialTextureUnit, t->gl_texturenum);
		if (GL_TextureReferenceIsValid(fb_texturenum)) {
			GLC_EnsureTMUEnabled(fullbrightTextureUnit);
			GL_EnsureTextureUnitBound(fullbrightTextureUnit, fb_texturenum);
		}
		else {
			GLC_EnsureTMUDisabled(fullbrightTextureUnit);
		}

		current_material = t->gl_texturenum;
		current_material_fb = t->fb_texturenum;

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline])) {
				continue;
			}

			for (; s; s = s->texturechain) {
				if (lightmapTextureUnit) {
					if (index_count && s->lightmaptexturenum != current_lightmap) {
						glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						GL_LogAPICall("glDrawElements(lightmap switch)");
						index_count = 0;
					}
					GLC_SetTextureLightmap(lightmapTextureUnit, current_lightmap = s->lightmaptexturenum);
				}
				else {
					GLC_AddToLightmapChain(s);
				}

				if (use_vbo) {
					if (index_count + 1 + s->polys->numverts > modelIndexMaximum) {
						glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
						GL_LogAPICall("glDrawElements(index buffer full)");
						index_count = 0;
					}

					if (index_count) {
						modelIndexes[index_count++] = ~(GLuint)0;
					}

					for (k = 0; k < s->polys->numverts; ++k) {
						modelIndexes[index_count++] = s->polys->vbo_start + k;
					}
				}
				else {
					glBegin(GL_POLYGON);
					v = s->polys->verts[0];

					if (!s->texinfo->flags & TEX_SPECIAL) {
						for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
							//Tei: textureless for the world brush models (Qrack)
							float tex_s = gl_textureless.value && model->isworldmodel ? 0 : v[3];
							float tex_t = gl_textureless.value && model->isworldmodel ? 0 : v[4];

							if (lightmapTextureUnit || fullbrightTextureUnit) {
								qglMultiTexCoord2f(materialTextureUnit, tex_s, tex_t);

								if (lightmapTextureUnit) {
									qglMultiTexCoord2f(lightmapTextureUnit, v[5], v[6]);
								}

								if (fullbrightTextureUnit) {
									qglMultiTexCoord2f(fullbrightTextureUnit, tex_s, tex_t);
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

				if (GL_TextureReferenceIsValid(fb_texturenum) && gl_fb_bmodels.integer && !fullbrightTextureUnit) {
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
		glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, modelIndexes);
		GL_LogAPICall("glDrawElements(flush)");
	}

	if (gl_fb_bmodels.value) {
		if (!lightmapTextureUnit) {
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
		if (lightmapTextureUnit) {
			GLC_BlendLightmaps();
		}
		if (drawfullbrights) {
			GLC_RenderFullbrights();
			drawfullbrights = false;
		}
	}
	GLC_StateEndDrawTextureChains();

	EmitCausticsPolys();
	EmitDetailPolys();
}

void GLC_DrawWorld(void)
{
	extern msurface_t* alphachain;
	extern cvar_t gl_outline;

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
	R_DrawAlphaChain(alphachain);
}

void GLC_DrawBrushModel(entity_t* e, model_t* clmodel, qbool caustics)
{
	extern cvar_t gl_outline;

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

	GLC_StateBeginBlendLightmaps();

	for (i = 0; i < GLC_LightmapCount(); i++) {
		if (!(p = GLC_LightmapChain(i))) {
			continue;
		}
		GLC_LightmapUpdate(i);
		GL_EnsureTextureUnitBound(GL_TEXTURE0, GLC_LightmapTexture(i));
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
	GLC_ClearLightmapPolys();

	GLC_StateEndBlendLightmaps();
}
