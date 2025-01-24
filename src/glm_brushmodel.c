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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"
#include "glsl/constants.glsl"
#include "glm_brushmodel.h"
#include "tr_types.h"
#include <limits.h>
#include "glm_vao.h"
#include "r_buffers.h"
#include "r_brushmodel.h"

typedef struct vbo_world_surface_s {
	float normal[4];
	float lm_vecs0[4];
	float lm_vecs1[4];
} vbo_world_surface_t;

void GLM_CreateBrushModelVAO(void)
{
	int i;

	if (!R_BufferReferenceIsValid(r_buffer_brushmodel_vertex_data) || !R_BufferReferenceIsValid(r_buffer_brushmodel_index_data)) {
		return;
	}

	// Create vao
	R_GenVertexArray(vao_brushmodel);
	buffers.Bind(r_buffer_brushmodel_index_data);

	GLM_ConfigureVertexAttribPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 0, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, position), 0);
	GLM_ConfigureVertexAttribPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 1, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, material_coords), 0);
	GLM_ConfigureVertexAttribPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 2, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, lightmap_coords), 0);
	GLM_ConfigureVertexAttribPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 3, 2, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, detail_coords), 0);
	// 
	GLM_ConfigureVertexAttribIPointer(vao_brushmodel, r_buffer_instance_number, 4, 1, GL_UNSIGNED_INT, sizeof(GLuint), 0, 1);
	GLM_ConfigureVertexAttribIPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 5, 1, GL_UNSIGNED_INT, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, flags), 0);
	GLM_ConfigureVertexAttribPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 6, 3, GL_FLOAT, GL_FALSE, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, flatcolor), 0);
	GLM_ConfigureVertexAttribIPointer(vao_brushmodel, r_buffer_brushmodel_vertex_data, 7, 1, GL_UNSIGNED_INT, sizeof(vbo_world_vert_t), VBO_FIELDOFFSET(vbo_world_vert_t, surface_num), 0);

	R_BindVertexArray(vao_none);

	// Create surface information SSBO
	if (cl.worldmodel) {
		vbo_world_surface_t* surfaces = (vbo_world_surface_t*) Q_malloc(cl.worldmodel->numsurfaces * sizeof(vbo_world_surface_t));
		for (i = 0; i < cl.worldmodel->numsurfaces; ++i) {
			msurface_t* surf = cl.worldmodel->surfaces + i;

			VectorCopy(surf->plane->normal, surfaces[i].normal);
			surfaces[i].normal[3] = surf->plane->dist;
			memcpy(surfaces[i].lm_vecs0, surf->lmvecs[0], sizeof(surf->lmvecs[0]));
			memcpy(surfaces[i].lm_vecs1, surf->lmvecs[1], sizeof(surf->lmvecs[1]));
			surfaces[i].lm_vecs0[3] = surf->lmvlen[0];
			surfaces[i].lm_vecs1[3] = surf->lmvlen[1];
		}
		buffers.Create(r_buffer_brushmodel_surface_data, buffertype_storage, "brushmodel-surfs", cl.worldmodel->numsurfaces * sizeof(vbo_world_surface_t), surfaces, bufferusage_constant_data);
		Q_free(surfaces);
		buffers.BindBase(r_buffer_brushmodel_surface_data, EZQ_GL_BINDINGPOINT_WORLDMODEL_SURFACES);
	}
}

// 'source' is from GLC's float[VERTEXSIZE]
int GLM_BrushModelCopyVertToBuffer(model_t* mod, void* vbo_buffer_, int position, float* source, int lightmap, int material, float scaleS, float scaleT, msurface_t* surf, qbool has_fb_texture, qbool has_luma_texture)
{
	vbo_world_vert_t* target = (vbo_world_vert_t*)vbo_buffer_ + position;

	VectorCopy(source, target->position);
	target->material_coords[0] = source[3] * (scaleS ? scaleS : 1);
	target->material_coords[1] = source[4] * (scaleT ? scaleT : 1);
	target->material_coords[2] = material;
	target->lightmap_coords[0] = source[5];
	target->lightmap_coords[1] = source[6];
	target->lightmap_coords[2] = lightmap;
	target->detail_coords[0] = source[7];
	target->detail_coords[1] = source[8];

	if (surf->flags & SURF_DRAWSKY) {
		target->flags = TEXTURE_TURB_SKY;
	}
	else if (surf->flags & SURF_DRAWTURB) {
		target->flags = (surf->texinfo->texture->turbType & EZQ_SURFACE_TYPE);
		if (!target->flags) {
			target->flags = TEXTURE_TURB_OTHER;
		}
	}
	else if (mod->isworldmodel) {
		target->flags = EZQ_SURFACE_WORLD;
		target->flags += (surf->flags & SURF_DRAWFLAT_FLOOR ? EZQ_SURFACE_IS_FLOOR: 0);
		target->flags += (surf->flags & SURF_UNDERWATER ? EZQ_SURFACE_UNDERWATER : 0);
	}
	else {
		target->flags = 0;
	}
	target->flags += (surf->flags & SURF_DRAWALPHA ? EZQ_SURFACE_ALPHATEST : 0);

	{
		byte rgba[4];

		COLOR_TO_RGBA(surf->texinfo->texture->flatcolor3ub, rgba);

		VectorScale(rgba, 1 / 255.0f, target->flatcolor);
	}
	target->surface_num = mod->isworldmodel ? surf - mod->surfaces : 0;

	return position + 1;
}

void GLM_ChainBrushModelSurfaces(model_t* clmodel, entity_t* ent)
{
	int i;
	msurface_t* psurf;
	qbool drawFlatFloors = r_drawflat_mode.integer == 0 && (r_drawflat.integer == 2 || r_drawflat.integer == 1) && clmodel->isworldmodel;
	qbool drawFlatWalls = r_drawflat_mode.integer == 0 && (r_drawflat.integer == 3 || r_drawflat.integer == 1) && clmodel->isworldmodel;

	// GLSL mode - always render the whole model, the surfaces will be re-used if there is
	//   another entity with the same model later in the scene
	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];
	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		if (psurf->flags & SURF_DRAWSKY) {
			// FIXME: Find an example...
			CHAIN_SURF_B2F(psurf, clmodel->drawflat_chain);
			clmodel->drawflat_todo = true;

			clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
			clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
		}
		else if (psurf->flags & SURF_DRAWTURB) {
			extern cvar_t r_fastturb;
			// FIXME: Find an example...
			if (r_fastturb.integer) {
				CHAIN_SURF_B2F(psurf, clmodel->drawflat_chain);
				clmodel->drawflat_todo = true;
			}
			else {
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain);
			}

			clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
			clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
		}
		else {
			if (drawFlatFloors && (psurf->flags & SURF_DRAWFLAT_FLOOR)) {
				chain_surfaces_simple_drawflat(&clmodel->drawflat_chain, psurf);
				clmodel->drawflat_todo = true;
			}
			else if (drawFlatWalls && !(psurf->flags & SURF_DRAWFLAT_FLOOR)) {
				chain_surfaces_simple_drawflat(&clmodel->drawflat_chain, psurf);
				clmodel->drawflat_todo = true;
			}
			else {
				chain_surfaces_simple(&psurf->texinfo->texture->texturechain, psurf);

				clmodel->first_texture_chained = min(clmodel->first_texture_chained, psurf->texinfo->miptex);
				clmodel->last_texture_chained = max(clmodel->last_texture_chained, psurf->texinfo->miptex);
			}
		}
	}
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
