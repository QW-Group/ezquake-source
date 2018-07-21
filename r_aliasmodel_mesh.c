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
// gl_mesh.c: triangle model functions

#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"

void GLM_CreateAliasModelVBO(buffer_ref vbo, buffer_ref instanceVBO, int required_vbo_length, void* aliasModelData);
void GLC_AllocateAliasPoseBuffer(void);

void GLM_PrepareAliasModel(model_t* m, aliashdr_t* hdr)
{
	// 
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
	vbo_model_vert_t* vbo_buffer;
	int pose, j, k, v;
	
	v = 0;
	hdr->poseverts = hdr->vertsPerPose = 3 * hdr->numtris;
	m->vertsInVBO = 3 * hdr->numtris * hdr->numposes;
	m->temp_vbo_buffer = vbo_buffer = Q_malloc_named(m->vertsInVBO * sizeof(vbo_model_vert_t), m->name);

	// 
	for (pose = 0; pose < hdr->numposes; ++pose) {
		v = pose * hdr->vertsPerPose;
		for (j = 0; j < hdr->numtris; ++j) {
			for (k = 0; k < 3; ++k, ++v) {
				trivertx_t* src;
				float x, y, z, s, t;
				int l;
				int vert = triangles[j].vertindex[k];

				src = &poseverts[pose][vert];

				l = src->lightnormalindex;
				x = src->v[0] * hdr->scale[0];
				y = src->v[1] * hdr->scale[1];
				z = src->v[2] * hdr->scale[2];
				s = stverts[vert].s;
				t = stverts[vert].t;

				if (!triangles[j].facesfront && stverts[vert].onseam) {
					s += pheader->skinwidth / 2;	// on back side
				}
				s = (s + 0.5) / pheader->skinwidth;
				t = (t + 0.5) / pheader->skinheight;

				VectorSet(vbo_buffer[v].position, x, y, z);
				vbo_buffer[v].texture_coords[0] = s;
				vbo_buffer[v].texture_coords[1] = t;
				VectorCopy(r_avertexnormals[l], vbo_buffer[v].normal);
				vbo_buffer[v].vert_index = v - pose * hdr->vertsPerPose;
			}
		}
	}
}

void R_AliasModelPopulateVBO(model_t* mod, aliashdr_t* hdr, vbo_model_vert_t* aliasModelBuffer, int position)
{
	if (mod->temp_vbo_buffer) {
		memcpy(aliasModelBuffer + position, mod->temp_vbo_buffer, mod->vertsInVBO * sizeof(vbo_model_vert_t));

		hdr->vertsOffset = mod->vbo_start = position;
		Q_free(mod->temp_vbo_buffer);
	}
}

void R_AliasModelMD3PopulateVBO(model_t* mod, vbo_model_vert_t* aliasModelData, int position)
{
	memcpy(aliasModelData + position, mod->temp_vbo_buffer, mod->vertsInVBO * sizeof(vbo_model_vert_t));

	mod->vbo_start = position;
	Q_free(mod->temp_vbo_buffer);
}

static void R_ImportSpriteCoordsToVBO(vbo_model_vert_t* verts, int* position)
{
	VectorSet(verts[0].position, 0, -1, -1);
	verts[0].texture_coords[0] = 1;
	verts[0].texture_coords[1] = 1;
	verts[0].vert_index = 0;

	VectorSet(verts[1].position, 0, -1, 1);
	verts[1].texture_coords[0] = 1;
	verts[1].texture_coords[1] = 0;
	verts[1].vert_index = 1;

	VectorSet(verts[2].position, 0, 1, 1);
	verts[2].texture_coords[0] = 0;
	verts[2].texture_coords[1] = 0;
	verts[2].vert_index = 2;

	VectorSet(verts[3].position, 0, 1, -1);
	verts[3].texture_coords[0] = 0;
	verts[3].texture_coords[1] = 1;
	verts[3].vert_index = 3;

	*position += sizeof(verts) / sizeof(verts[0]);
}

static void R_ImportModelToVBO(model_t* mod, vbo_model_vert_t* aliasmodel_data, int* new_vbo_position)
{
	if (mod->type == mod_alias) {
		aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);

		R_AliasModelPopulateVBO(mod, paliashdr, aliasmodel_data, *new_vbo_position);
		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_alias3) {
		R_AliasModelMD3PopulateVBO(mod, aliasmodel_data, *new_vbo_position);

		*new_vbo_position += mod->vertsInVBO;
	}
	else if (mod->type == mod_sprite) {
		mod->vbo_start = 0;
	}
	else if (mod->type == mod_brush) {
		mod->vbo_start = 0;
	}
}

void R_CreateAliasModelVBO(buffer_ref instanceVBO)
{
	vbo_model_vert_t* aliasModelData;
	int new_vbo_position = 0;
	int required_vbo_length = 4;
	buffer_ref vbo;
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// custom models are explicitly loaded by client, not notified by server
	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod && (mod->type == mod_alias || mod->type == mod_alias3)) {
			required_vbo_length += mod->vertsInVBO;
		}
	}

	// Go back through all models, importing textures into arrays and creating new VBO
	aliasModelData = Q_malloc(required_vbo_length * sizeof(vbo_model_vert_t));

	// VBO starts with simple-model/sprite vertices
	R_ImportSpriteCoordsToVBO(aliasModelData, &new_vbo_position);

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	for (i = 0; i < custom_model_count; ++i) {
		model_t* mod = Mod_CustomModel(i, false);

		if (mod) {
			R_ImportModelToVBO(mod, aliasModelData, &new_vbo_position);
		}
	}

	vbo = buffers.Create(buffertype_vertex, "aliasmodel-vertex-data", required_vbo_length * sizeof(vbo_model_vert_t), aliasModelData, bufferusage_constant_data);
#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_CreateAliasModelVBO(vbo, instanceVBO, required_vbo_length, aliasModelData);
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_AllocateAliasPoseBuffer();
	}
#endif
	Q_free(aliasModelData);
}
