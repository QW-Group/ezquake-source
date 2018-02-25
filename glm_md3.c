/*
Copyright (C) 2011 fuh and ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Code to display .md3 files

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_md3.h"
#include "gl_aliasmodel.h"
#include "vx_vertexlights.h" 

void GLM_MakeAlias3DisplayLists(model_t* model)
{
	md3Surface_t* surf;
	md3St_t* texCoords;
	md3XyzNormal_t* vertices;
	int surfnum;
	int framenum;
	md3Triangle_t* triangles;
	int v;
	vbo_model_vert_t* vbo;
	extern float r_avertexnormals[NUMVERTEXNORMALS][3];
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(model);
	md3Header_t* pheader = MD3_HeaderForModel(md3Model);
	int vertsPerFrame = 0;

	// Work out how many verts we are going to need to store in VBO
	model->vertsInVBO = vertsPerFrame = 0;
	MD3_ForEachSurface(pheader, surf, surfnum) {
		vertsPerFrame += 3 * surf[surfnum].numTriangles;
	}
	model->vertsInVBO = vertsPerFrame * pheader->numFrames;
	model->temp_vbo_buffer = vbo = Q_malloc(sizeof(vbo_model_vert_t) * model->vertsInVBO);

	// foreach frame
	for (framenum = 0, v = 0; framenum < pheader->numFrames; ++framenum) {
		int base = v;

		// loop through the surfaces.
		MD3_ForEachSurface(pheader, surf, surfnum) {
			int i, triangle;

			texCoords = MD3_SurfaceTextureCoords(surf);
			vertices = MD3_SurfaceVertices(surf);

			triangles = MD3_SurfaceTriangles(surf);

			for (triangle = 0; triangle < surf->numTriangles; ++triangle) {
				for (i = 0; i < 3; ++i, ++v) {
					int vertexNumber = framenum * surf->numVerts + triangles[triangle].indexes[i];
					md3XyzNormal_t* vert = &vertices[vertexNumber];
					float s, t;

					s = texCoords[triangles[triangle].indexes[i]].s;
					t = texCoords[triangles[triangle].indexes[i]].t;

					VectorScale(vert->xyz, MD3_XYZ_SCALE, vbo[v].position);
					VectorCopy(r_avertexnormals[vert->normal >> 8], vbo[v].normal);
					vbo[v].texture_coords[0] = s;
					vbo[v].texture_coords[1] = t;
					vbo[v].vert_index = v - base;
				}
			}
		}
	}
}

void GLM_DrawAlias3Model(entity_t* ent)
{
	extern cvar_t cl_drawgun, r_viewmodelsize, r_lerpframes, gl_fb_models;
	extern byte	*shadedots;
	extern byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS];
	extern void R_AliasSetupLighting(entity_t* ent);

	float lerpfrac = 1;
	float oldMatrix[16];

	int frame1 = ent->oldframe, frame2 = ent->frame;
	model_t* mod = ent->model;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(mod);
	surfinf_t* surfaceInfo = MD3_ExtraSurfaceInfoForModel(md3Model);
	md3Header_t* pHeader = MD3_HeaderForModel(md3Model);
	int surfnum;
	md3Surface_t* surf;

	int vertsPerFrame = 0;
	MD3_ForEachSurface(pHeader, surf, surfnum) {
		vertsPerFrame += 3 * surf[surfnum].numTriangles;
	}

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	R_RotateForEntity(ent);

	// 
	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	if (ent->alpha) {
		r_modelalpha = ent->alpha;
	}

	if ((ent->renderfx & RF_WEAPONMODEL) && r_viewmodelsize.value < 1) {
		// perform scalling for r_viewmodelsize
		GL_Scale(GL_MODELVIEW, bound(0.5, r_viewmodelsize.value, 1), 1, 1);
	}

	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	if (gl_fb_models.integer) {
		ambientlight = 999999;
	}
	if (frame1 >= pHeader->numFrames) {
		frame1 = pHeader->numFrames - 1;
	}
	if (frame2 >= pHeader->numFrames) {
		frame2 = pHeader->numFrames - 1;
	}

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame) {
		lerpfrac = 1.0;
	}
	else {
		lerpfrac = min(ent->framelerp, 1);
	}

	if (lerpfrac == 1) {
		lerpfrac = 0;
		frame1 = frame2;
	}

	GLM_DrawAliasModelFrame(
		mod, mod->vbo_start + vertsPerFrame * frame1, mod->vbo_start + vertsPerFrame * frame2, vertsPerFrame,
		surfaceInfo[0].texnum, null_texture_reference, false, ent->effects, ent->renderfx
	);

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}

void GL_MD3ModelAddToVBO(model_t* mod, buffer_ref vbo, buffer_ref ssbo, int position)
{
	GL_UpdateBufferSection(vbo, position * sizeof(vbo_model_vert_t), mod->vertsInVBO * sizeof(vbo_model_vert_t), mod->temp_vbo_buffer);
	if (GL_BufferReferenceIsValid(ssbo)) {
		GL_UpdateBufferSection(ssbo, position * sizeof(vbo_model_vert_t), mod->vertsInVBO * sizeof(vbo_model_vert_t), mod->temp_vbo_buffer);
	}

	mod->vbo_start = position;
	Q_free(mod->temp_vbo_buffer);
}
