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
#include "r_aliasmodel_md3.h"
#include "r_aliasmodel.h"
#include "vx_vertexlights.h" 
#include "r_matrix.h"

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
	MD3_ForEachSurface(pheader, surf, surfnum)
	{
		vertsPerFrame += 3 * surf[surfnum].numTriangles;
	}
	model->vertsInVBO = vertsPerFrame * pheader->numFrames;
	model->temp_vbo_buffer = vbo = Q_malloc(sizeof(vbo_model_vert_t) * model->vertsInVBO);

	// foreach frame
	for (framenum = 0, v = 0; framenum < pheader->numFrames; ++framenum) {
		int base = v;

		// loop through the surfaces.
		MD3_ForEachSurface(pheader, surf, surfnum)
		{
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

void GL_MD3ModelAddToVBO(model_t* mod, vbo_model_vert_t* aliasModelData, int position)
{
	memcpy(aliasModelData + position, mod->temp_vbo_buffer, mod->vertsInVBO * sizeof(vbo_model_vert_t));

	mod->vbo_start = position;
	Q_free(mod->temp_vbo_buffer);
}
