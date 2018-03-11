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
#include "glsl/constants.glsl"

void GL_AliasModelSetVertexDirection(int num_triangles, vbo_model_vert_t* vbo_buffer, int v1, int v2, qbool limit_lerp, int key_pose);
void GL_AliasModelFixNormals(vbo_model_vert_t* vbo_buffer, int v, int vertsPerPose);

void GLM_MakeAlias3DisplayLists(model_t* model)
{
	md3Surface_t* surf;
	md3St_t* texCoords;
	ezMd3XyzNormal_t* vertices;
	int surfnum;
	int framenum;
	md3Triangle_t* triangles;
	int v;
	md3model_t* md3Model = (md3model_t *)Mod_Extradata(model);
	md3Header_t* pheader = MD3_HeaderForModel(md3Model);
	vbo_model_vert_t* vbo;

	// Work out how many verts we are going to need to store in VBO
	model->vertsInVBO = 0;
	MD3_ForEachSurface(pheader, surf, surfnum) {
		model->vertsInVBO += 3 * surf->numTriangles;
	}
	model->vertsInVBO *= pheader->numFrames;
	model->temp_vbo_buffer = vbo = Q_malloc(sizeof(vbo_model_vert_t) * model->vertsInVBO);

	// foreach frame
	for (framenum = 0, v = 0; framenum < pheader->numFrames; ++framenum) {
		int initial_v = v;

		// loop through the surfaces.
		MD3_ForEachSurface(pheader, surf, surfnum) {
			int i, triangle;

			texCoords = MD3_SurfaceTextureCoords(surf);
			vertices = MD3_SurfaceVertices(surf);
			triangles = MD3_SurfaceTriangles(surf);

			for (triangle = 0; triangle < surf->numTriangles; ++triangle) {
				for (i = 0; i < 3; ++i, ++v) {
					int vertexNumber = framenum * surf->numVerts + triangles[triangle].indexes[i];
					int nextFrame = Mod_ExpectedNextFrame(model, framenum, pheader->numFrames);
					int nextVertexNumber = nextFrame * surf->numVerts + triangles[triangle].indexes[i];
					ezMd3XyzNormal_t* vert = &vertices[vertexNumber];
					ezMd3XyzNormal_t* nextVert = &vertices[nextVertexNumber];
					float s, t;

					s = texCoords[triangles[triangle].indexes[i]].s;
					t = texCoords[triangles[triangle].indexes[i]].t;

					VectorCopy(vert->xyz, vbo[v].position);
					VectorCopy(vert->normal, vbo[v].normal);
					vbo[v].texture_coords[0] = s;
					vbo[v].texture_coords[1] = t;
					vbo[v].flags = 0;

					// Set direction
					VectorSubtract(nextVert->xyz, vert->xyz, vbo[v].direction);
					if (model->renderfx & RF_LIMITLERP) {
						if ((vert->xyz[0] > 0) != (nextVert->xyz[0] > 0)) {
							vbo[v].flags |= AM_VERTEX_NOLERP;
						}
					}
				}
			}
		}

		GL_AliasModelFixNormals(vbo, initial_v, v - initial_v);
	}
}
