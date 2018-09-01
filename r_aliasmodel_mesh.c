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
#include "r_aliasmodel.h"

void GLM_CreateAliasModelVBO(buffer_ref instanceVBO);
void GLC_AllocateAliasPoseBuffer(void);

static void GLM_AliasModelSetVertexDirection(aliashdr_t* hdr, vbo_model_vert_t* vbo_buffer, int pose1, int pose2)
{
	int v1 = pose1 * hdr->vertsPerPose;
	int v2 = pose2 * hdr->vertsPerPose;
	int j, k;

	for (j = 0; j < hdr->numtris; ++j) {
		for (k = 0; k < 3; ++k, ++v1, ++v2) {
			// FIXME: This is a much better place to process RF_LIMITLERP
			VectorSubtract(vbo_buffer[v2].position, vbo_buffer[v1].position, vbo_buffer[v1].direction);
		}
	}
}

void GLM_PrepareAliasModel(model_t* m, aliashdr_t* hdr)
{
	// 
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
	vbo_model_vert_t* vbo_buffer;
	int pose, j, k, v;
	int f;
	
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

				memset(&vbo_buffer[v], 0, sizeof(vbo_buffer[v]));
				VectorSet(vbo_buffer[v].position, x, y, z);
				vbo_buffer[v].texture_coords[0] = s;
				vbo_buffer[v].texture_coords[1] = t;
				VectorCopy(r_avertexnormals[l], vbo_buffer[v].normal);
				vbo_buffer[v].vert_index = v - pose * hdr->vertsPerPose;
			}
		}
	}

	// Go back through and set directions
	for (f = 0; f < hdr->numframes; ++f) {
		maliasframedesc_t* frame = &hdr->frames[f];

		if (frame->numposes > 1) {
			// This frame has animated poses, so link them all together
			for (pose = 0; pose < frame->numposes - 1; ++pose) {
				GLM_AliasModelSetVertexDirection(hdr, vbo_buffer, frame->firstpose + pose, frame->firstpose + pose + 1);
			}
		}
		else {
			// Find next frame's pose
			maliasframedesc_t* frame2 = R_AliasModelFindFrame(hdr, frame->groupname, frame->groupnumber + 1);
			if (!frame2) {
				frame2 = R_AliasModelFindFrame(hdr, frame->groupname, 1);
			}

			if (frame2) {
				GLM_AliasModelSetVertexDirection(hdr, vbo_buffer, frame->firstpose, frame2->firstpose);
				frame->nextpose = frame2->firstpose;
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

void R_CreateAliasModelVBO(buffer_ref instanceVBO)
{
#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		GLM_CreateAliasModelVBO(instanceVBO);
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_AllocateAliasPoseBuffer();
	}
#endif
}
