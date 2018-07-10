/*
Copyright (C) 2017-2018 ezQuake team

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
#include "common_draw.h"
#include "glm_draw.h"
#include "glm_vao.h"
#include "r_state.h"
#include "r_matrix.h"

static glm_program_t polygonProgram;
static buffer_ref polygonVBO;
static GLint polygonUniforms_matrix;
static GLint polygonUniforms_color;

glm_polygon_framedata_t polygonData;

void GLM_PreparePolygons(void)
{
	if (GL_UseGLSL()) {
		if (GLM_ProgramRecompileNeeded(&polygonProgram, 0)) {
			GL_VFDeclare(hud_draw_polygon);

			if (!GLM_CreateVFProgram("polygon-draw", GL_VFParams(hud_draw_polygon), &polygonProgram)) {
				return;
			}
		}

		if (!polygonProgram.uniforms_found) {
			polygonUniforms_matrix = GL_UniformGetLocation(polygonProgram.program, "matrix");
			polygonUniforms_color = GL_UniformGetLocation(polygonProgram.program, "color");
			polygonProgram.uniforms_found = true;
		}

		if (!GL_BufferReferenceIsValid(polygonVBO)) {
			polygonVBO = GL_CreateFixedBuffer(buffertype_vertex, "polygon-vbo", sizeof(polygonData.polygonVertices), polygonData.polygonVertices, bufferusage_once_per_frame);
		}
		else if (polygonData.polygonVerts[0]) {
			GL_UpdateBuffer(polygonVBO, polygonData.polygonCount * MAX_POLYGON_POINTS * sizeof(polygonData.polygonVertices[0]), polygonData.polygonVertices);
		}

		if (!R_VertexArrayCreated(vao_hud_polygons)) {
			R_GenVertexArray(vao_hud_polygons);
			GLM_ConfigureVertexAttribPointer(vao_hud_polygons, polygonVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(polygonData.polygonVertices[0]), NULL, 0);
			R_BindVertexArray(vao_none);
		}
	}
}

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color)
{
	if (num_vertices < 0 || num_vertices > MAX_POLYGON_POINTS) {
		return;
	}
	if (polygonData.polygonCount >= MAX_POLYGONS_PER_FRAME) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_polygon, 0)) {
		return;
	}

	polygonData.polygonVerts[polygonData.polygonCount] = num_vertices;
	COLOR_TO_FLOATVEC_PREMULT(color, polygonData.polygonColor[polygonData.polygonCount]);
	polygonData.polygonX[polygonData.polygonCount] = x;
	polygonData.polygonY[polygonData.polygonCount] = y;
	memcpy(polygonData.polygonVertices, vertices, sizeof(polygonData.polygonVertices[0]) * num_vertices);
	++polygonData.polygonCount;
}

void GLM_DrawPolygons(int start, int end)
{
	int i;
	uintptr_t offset = GL_BufferOffset(polygonVBO) / sizeof(polygonData.polygonVertices[0]);

	R_BindVertexArray(vao_hud_polygons);
	GL_UseProgram(polygonProgram.program);
	GL_UniformMatrix4fv(polygonUniforms_matrix, 1, GL_FALSE, GLM_ProjectionMatrix());

	GLM_StateBeginPolygonDraw();

	for (i = start; i <= end; ++i) {
		//GLM_TransformMatrix(GLM_ProjectionMatrix(), polygonData.polygonX[i], polygonData.polygonY[i], 0);
		GL_Uniform4fv(polygonUniforms_color, 1, polygonData.polygonColor[i]);

		GL_DrawArrays(GL_TRIANGLE_STRIP, offset + i * MAX_POLYGON_POINTS, polygonData.polygonVerts[i]);
	}
}
