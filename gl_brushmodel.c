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
#include "gl_local.h"
#include "r_vao.h"
#include "glm_brushmodel.h"
#include "r_renderer.h"

buffer_ref brushModel_vbo;
buffer_ref vbo_brushElements;
GLuint* modelIndexes;
GLuint modelIndexMaximum;

static int GL_MeasureIndexSizeForBrushModel(model_t* m)
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

void GL_CreateBrushModelVBO(buffer_ref instance_vbo)
{
	int i;
	int size = 0;
	int position = 0;
	int indexes = 0;
	int max_entity_indexes = 0;
	void* buffer = NULL;
	GLsizei buffer_size;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			int index_count = GL_MeasureIndexSizeForBrushModel(mod);

			size += GL_MeasureVBOSizeForBrushModel(mod);
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
			int index_count = GL_MeasureIndexSizeForBrushModel(mod);

			size += GL_MeasureVBOSizeForBrushModel(mod);
			if (mod->isworldmodel) {
				indexes += index_count;
			}
			else {
				max_entity_indexes = max(max_entity_indexes, index_count);
			}
		}
	}

	indexes += max_entity_indexes * MAX_STANDARD_ENTITIES;
	if (!GL_BufferReferenceIsValid(vbo_brushElements) || indexes > buffers.Size(vbo_brushElements) / sizeof(modelIndexes[0])) {
		Q_free(modelIndexes);
		modelIndexMaximum = indexes;
		modelIndexes = Q_malloc(sizeof(*modelIndexes) * modelIndexMaximum);

		R_BindVertexArray(vao_none);
		if (GL_BufferReferenceIsValid(vbo_brushElements)) {
			vbo_brushElements = buffers.Resize(vbo_brushElements, modelIndexMaximum * sizeof(modelIndexes[0]), NULL);
		}
		else {
			vbo_brushElements = buffers.Create(buffertype_index, "brushmodel-elements", modelIndexMaximum * sizeof(modelIndexes[0]), NULL, bufferusage_once_per_frame);
		}
	}

	// Create vbo buffer
	buffer_size = size * (R_UseModernOpenGL() ? sizeof(vbo_world_vert_t) : sizeof(glc_vbo_world_vert_t));
	buffer = Q_malloc(buffer_size);

	// Copy data into buffer
	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = GL_PopulateVBOForBrushModel(mod, buffer, position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];
		if (mod && mod->type == mod_brush) {
			position = GL_PopulateVBOForBrushModel(mod, buffer, position);
		}
	}

	// Copy VBO buffer across
	brushModel_vbo = buffers.Create(buffertype_vertex, "brushmodel-vbo", buffer_size, buffer, bufferusage_constant_data);

#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_CreateBrushModelVAO(brushModel_vbo, vbo_brushElements, instance_vbo);
	}
#endif

	Q_free(buffer);
}

void GL_DeleteBrushModelIndexBuffer(void)
{
	modelIndexMaximum = 0;
	Q_free(modelIndexes);
}

int GL_MeasureVBOSizeForBrushModel(model_t* m)
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
int GL_PopulateVBOForBrushModel(model_t* m, void* vbo_buffer, int vbo_pos)
{
	int i, j;

	// Order vertices in the VBO by texture & lightmap
	for (i = 0; i < m->numtextures; ++i) {
		int length = 0;
		int surface_count = 0;
		qbool has_luma = false;

		if (!m->textures[i]) {
			continue;
		}

		has_luma = R_TextureReferenceIsValid(m->textures[i]->fb_texturenum);
		for (j = 0; j < m->nummodelsurfaces; ++j) {
			msurface_t* surf = m->surfaces + m->firstmodelsurface + j;
			int lightmap = surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY) ? -1 : surf->lightmaptexturenum;
			glpoly_t* poly;

			if (surf->texinfo->miptex != i) {
				continue;
			}

			// copy verts into buffer (alternate to turn fan into triangle strip)
			for (poly = surf->polys; poly; poly = poly->next) {
				int end_vert = 0;
				int start_vert = 1;
				int output = 0;
				int material = i;
				float scaleS = m->textures[i]->gl_texture_scaleS;
				float scaleT = m->textures[i]->gl_texture_scaleT;

				if (!poly->numverts) {
					continue;
				}

				// Store position for drawing individual polys
				poly->vbo_start = vbo_pos;
				vbo_pos = renderer.BrushModelCopyVertToBuffer(m, vbo_buffer, vbo_pos, poly->verts[0], lightmap, material, scaleS, scaleT, surf, has_luma);
				++output;

				start_vert = 1;
				end_vert = poly->numverts - 1;

				while (start_vert <= end_vert) {
					vbo_pos = renderer.BrushModelCopyVertToBuffer(m, vbo_buffer, vbo_pos, poly->verts[start_vert], lightmap, material, scaleS, scaleT, surf, has_luma);
					++output;

					if (start_vert < end_vert) {
						vbo_pos = renderer.BrushModelCopyVertToBuffer(m, vbo_buffer, vbo_pos, poly->verts[end_vert], lightmap, material, scaleS, scaleT, surf, has_luma);
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

	return vbo_pos;
}
