/*
Copyright (C) 2017 ezQuake team

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
#ifndef __APPLE__
#include "tr_types.h"
#endif
#include "glm_draw.h"

static glm_program_t polygonProgram;
static glm_vao_t polygonVAO;
static buffer_ref polygonVBO;
static GLint polygonUniforms_matrix;
static GLint polygonUniforms_color;

glm_polygon_framedata_t polygonData;

void GLM_PreparePolygons(void)
{
	if (GL_ShadersSupported()) {
		if (GLM_ProgramRecompileNeeded(&polygonProgram, 0)) {
			GL_VFDeclare(hud_draw_polygon);

			if (!GLM_CreateVFProgram("polygon-draw", GL_VFParams(hud_draw_polygon), &polygonProgram)) {
				return;
			}
		}

		if (!polygonProgram.uniforms_found) {
			polygonUniforms_matrix = glGetUniformLocation(polygonProgram.program, "matrix");
			polygonUniforms_color = glGetUniformLocation(polygonProgram.program, "color");
			polygonProgram.uniforms_found = true;
		}

		if (!GL_BufferReferenceIsValid(polygonVBO)) {
			polygonVBO = GL_CreateFixedBuffer(GL_ARRAY_BUFFER, "polygon-vbo", sizeof(polygonData.polygonVertices), polygonData.polygonVertices, write_once_use_once);
		}
		else if (polygonData.polygonVerts[0]) {
			GL_UpdateBuffer(polygonVBO, polygonData.polygonCount * MAX_POLYGON_POINTS * sizeof(polygonData.polygonVertices[0]), polygonData.polygonVertices);
		}

		if (!polygonVAO.vao) {
			GL_GenVertexArray(&polygonVAO, "polygon-vao");
			GL_ConfigureVertexAttribPointer(&polygonVAO, polygonVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(polygonData.polygonVertices[0]), NULL, 0);
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
	float matrix[16];
	int i;
	uintptr_t offset = GL_BufferOffset(polygonVBO) / sizeof(polygonData.polygonVertices[0]);

	GL_BindVertexArray(&polygonVAO);
	GL_UseProgram(polygonProgram.program);

	GL_Disable(GL_DEPTH_TEST);
	GLM_GetMatrix(GL_PROJECTION, matrix);
	glUniformMatrix4fv(polygonUniforms_matrix, 1, GL_FALSE, matrix);

	for (i = start; i <= end; ++i) {
		GLM_TransformMatrix(matrix, polygonData.polygonX[i], polygonData.polygonY[i], 0);

		glUniform4fv(polygonUniforms_color, 1, polygonData.polygonColor[i]);

		GL_DrawArrays(GL_TRIANGLE_STRIP, i * MAX_POLYGON_POINTS, polygonData.polygonVerts[i]);
	}
}

void GLC_DrawPolygons(int start, int end)
{
	int i, j;
	int oldFlags = GL_AlphaBlendFlags(GL_ALPHATEST_NOCHANGE | GL_BLEND_NOCHANGE);

	GLC_StateBeginDrawPolygon();

	for (i = start; i <= end; ++i) {
		glColor4fv(polygonData.polygonColor[i]);
		glBegin(GL_TRIANGLE_STRIP);
		for (j = 0; j < polygonData.polygonVerts[i]; j++) {
			glVertex3fv(polygonData.polygonVertices[j + i * MAX_POLYGON_POINTS]);
		}
		glEnd();
	}

	GLC_StateEndDrawPolygon(oldFlags);
}
