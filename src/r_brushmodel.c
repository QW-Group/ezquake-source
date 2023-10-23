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

#include "quakedef.h"
#include "gl_model.h"
#include "r_vao.h"
#include "r_renderer.h"
#include "r_state.h"
#include "tr_types.h"
#include "rulesets.h"
#include "r_trace.h"
#include "r_matrix.h"
#include "r_brushmodel.h"
#include "r_lightmaps.h"
#include "r_lighting.h"
#include "glc_state.h"

unsigned int* modelIndexes;
unsigned int modelIndexMaximum;

static int R_BrushModelMeasureVBOSize(model_t* m);
static int R_BrushModelPopulateVBO(model_t* m, void* vbo_buffer, int vbo_pos);
void R_BrushModelClearTextureChains(model_t *clmodel);

static int R_BrushModelMeasureIndexSize(model_t* m)
{
	int j, total_surf_verts = 0, total_surfaces = 0;

	for (j = 0; j < m->nummodelsurfaces; ++j) {
		msurface_t* surf = m->surfaces + m->firstmodelsurface + j;
		glpoly_t* poly;

		for (poly = surf->polys; poly; poly = poly->next) {
			total_surf_verts += poly->numverts;
			++total_surfaces;
		}
	}

	if (total_surf_verts <= 0 || total_surfaces < 1) {
		return 0;
	}

	// Every vert in every surface, + surface terminator
	return (total_surf_verts)+(total_surfaces - 1);
}

void R_BrushModelCreateVBO(void)
{
	int i;
	int size = 0;
	int position = 0;
	int indexes = 0;
	int max_entity_indexes = 0;
	void* buffer = NULL;
	unsigned int buffer_size;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			int index_count = R_BrushModelMeasureIndexSize(mod);

			size += R_BrushModelMeasureVBOSize(mod);
			if (mod->isworldmodel) {
				indexes += index_count;
			}
			else {
				max_entity_indexes = max(max_entity_indexes, index_count);
			}
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			int index_count = R_BrushModelMeasureIndexSize(mod);

			size += R_BrushModelMeasureVBOSize(mod);
			if (mod->isworldmodel) {
				indexes += index_count;
			}
			else {
				max_entity_indexes = max(max_entity_indexes, index_count);
			}
		}
	}

	indexes += max_entity_indexes * MAX_STANDARD_ENTITIES;
	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_index_data) || indexes > buffers.Size(r_buffer_brushmodel_index_data) / sizeof(modelIndexes[0])) {
		Q_free(modelIndexes);
		modelIndexMaximum = indexes;
		modelIndexes = Q_malloc(sizeof(*modelIndexes) * modelIndexMaximum);

		R_BindVertexArray(vao_none);
		if (R_BufferReferenceIsValid(r_buffer_brushmodel_index_data)) {
			buffers.Resize(r_buffer_brushmodel_index_data, modelIndexMaximum * sizeof(modelIndexes[0]), NULL);
		}
		else {
			buffers.Create(r_buffer_brushmodel_index_data, buffertype_index, "brushmodel-elements", modelIndexMaximum * sizeof(modelIndexes[0]), NULL, bufferusage_once_per_frame);
		}
	}

	// Create vbo buffer
	buffer_size = size * (R_UseModernOpenGL() ? sizeof(vbo_world_vert_t) : sizeof(glc_vbo_world_vert_t));
	buffer = Q_malloc(buffer_size);

	// Copy data into buffer
	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = R_BrushModelPopulateVBO(mod, buffer, position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = R_BrushModelPopulateVBO(mod, buffer, position);
		}
	}

	// Copy VBO buffer across
	buffers.Create(r_buffer_brushmodel_vertex_data, buffertype_vertex, "brushmodel-vbo", buffer_size, buffer, bufferusage_reuse_many_frames);

	Q_free(buffer);
}

static int R_BrushModelMeasureVBOSize(model_t* m)
{
	int j, total_surf_verts = 0, total_surfaces = 0;

	for (j = 0; j < m->nummodelsurfaces; ++j) {
		msurface_t* surf = m->surfaces + m->firstmodelsurface + j;
		glpoly_t* poly;

		if (!(surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))) {
			if (surf->texinfo->flags & TEX_SPECIAL) {
				continue;
			}
		}
		if (!m->textures[surf->texinfo->miptex]) {
			continue;
		}

		for (poly = surf->polys; poly; poly = poly->next) {
			total_surf_verts += poly->numverts;
			++total_surfaces;
		}
	}

	if (total_surf_verts <= 0 || total_surfaces < 1) {
		return 0;
	}

	return (total_surf_verts);// +2 * (total_surfaces - 1));
}

// Create VBO, ordering by texture array
static int R_BrushModelPopulateVBO(model_t* m, void* vbo_buffer, int vbo_pos)
{
	int i, j;

	// Order vertices in the VBO by texture & lightmap
	for (i = 0; i < m->numtextures; ++i) {
		qbool has_fb = false, has_luma = false;

		if (!m->textures[i]) {
			continue;
		}

		has_fb = R_TextureReferenceIsValid(m->textures[i]->fb_texturenum);
		has_luma = has_fb & m->textures[i]->isLumaTexture;
		for (j = 0; j < m->nummodelsurfaces; ++j) {
			msurface_t* surf = m->surfaces + m->firstmodelsurface + j;
			qbool isTurbOrSky = surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY);
			qbool isLitTurb = surf->texinfo->texture->isLitTurb;
			int lightmap = isTurbOrSky && !isLitTurb ? -1 : surf->lightmaptexturenum;
			glpoly_t* poly;

			if (surf->texinfo->miptex != i) {
				continue;
			}

			// copy verts into buffer (alternate to turn fan into triangle strip)
			for (poly = surf->polys; poly; poly = poly->next) {
				int k = 0;
				int material = i;
				float scaleS = m->textures[i]->gl_texture_scaleS;
				float scaleT = m->textures[i]->gl_texture_scaleT;

				if (!poly->numverts) {
					continue;
				}

				// Store position for drawing individual polys
				poly->vbo_start = vbo_pos;
				for (k = 0; k < poly->numverts; ++k) {
					vbo_pos = renderer.BrushModelCopyVertToBuffer(m, vbo_buffer, vbo_pos, poly->verts[k], lightmap, material, scaleS, scaleT, surf, has_fb, has_luma);
				}

			}
		}
	}

	return vbo_pos;
}

void R_BrushModelFreeMemory(void)
{
	modelIndexMaximum = 0;
	Q_free(modelIndexes);
}

void R_BrushModelDrawEntity(entity_t *e)
{
	int k;
	unsigned int li;
	unsigned int lj;
	vec3_t mins, maxs;
	model_t *clmodel;
	qbool rotated;
	float oldMatrix[16];
	extern cvar_t gl_brush_polygonoffset;
	qbool caustics = false;
	extern cvar_t gl_flashblend;
	extern msurface_t* skychain;

	// Get rid of Z-fighting for textures by offsetting the
	// drawing of entity models compared to normal polygons.
	// dimman: disabled for qcon
	qbool polygonOffset = gl_brush_polygonoffset.value > 0 && Ruleset_AllowPolygonOffset(e);

	clmodel = e->model;
	if (!clmodel->nummodelsurfaces) {
		return;
	}

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		if (R_CullSphere(e->origin, clmodel->radius)) {
			return;
		}
	}
	else {
		rotated = false;
		VectorAdd(e->origin, clmodel->mins, mins);
		VectorAdd(e->origin, clmodel->maxs, maxs);

		if (R_CullBox(mins, maxs)) {
			return;
		}
	}

	R_TraceEnterRegion(va("%s(%s)", __func__, e->model->name), true);

	VectorSubtract(r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface) {
		for (li = 0; li < MAX_DLIGHTS / 32; li++) {
			if (cl_dlight_active[li]) {
				for (lj = 0; lj < 32; lj++) {
					if ((cl_dlight_active[li] & (1 << lj)) && li * 32 + lj < MAX_DLIGHTS) {
						k = li * 32 + lj;

						if (!gl_flashblend.integer || (cl_dlights[k].bubble && gl_flashblend.integer != 2)) {
							R_MarkLights(&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->firstnode);
						}
					}
				}
			}
		}
	}

	R_BrushModelClearTextureChains(clmodel);
	renderer.ChainBrushModelSurfaces(clmodel, e);

	if (clmodel->last_texture_chained >= 0 || clmodel->drawflat_todo || skychain) {
		R_PushModelviewMatrix(oldMatrix);
		R_RotateForEntity(e);

		// START shaman FIX for no simple textures on world brush models {
		//draw the textures chains for the model
		if (clmodel->last_texture_chained >= 0 || clmodel->drawflat_todo) {
			if (clmodel->firstmodelsurface) {
				R_RenderAllDynamicLightmaps(clmodel);
			}

			//R00k added contents point for underwater bmodels
			if (r_refdef2.drawCaustics) {
				if (clmodel->isworldmodel) {
					vec3_t midpoint;

					VectorAdd(clmodel->mins, clmodel->maxs, midpoint);
					VectorScale(midpoint, 0.5f, midpoint);
					VectorAdd(midpoint, e->origin, midpoint);

					caustics = R_PointIsUnderwater(midpoint);
				}
				else {
					caustics = R_PointIsUnderwater(e->origin);
				}
			}
		}

		renderer.DrawBrushModel(e, polygonOffset, caustics);
		// } END shaman FIX for no simple textures on world brush models

		R_PopModelviewMatrix(oldMatrix);
	}

	R_TraceLeaveRegion(true);
}

// Convert ordering of verts for convex polygons to triangle strip
void R_BrushModelPolygonToTriangleStrip(glpoly_t* poly)
{
	float tempVert[VERTEXSIZE];
	int i;

	// Apart from first vertex, replace odd verts with vert at end of list
	for (i = 2; i < poly->numverts - 1; i += 2) {
		memcpy(tempVert, poly->verts[poly->numverts - 1], sizeof(tempVert));
		memmove(poly->verts[i + 1], poly->verts[i], sizeof(tempVert) * (poly->numverts - i - 1));
		memcpy(poly->verts[i], tempVert, sizeof(tempVert));
	}
}
