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

static glm_program_t polygonProgram;
static glm_vao_t polygonVAO;
static buffer_ref polygonVBO;
static GLint polygonUniforms_matrix;
static GLint polygonUniforms_color;

static vec3_t polygonVertices[64];
static int polygonVerts;
static color_t polygonColor;
static int polygonX;
static int polygonY;

void GLM_PreparePolygon(void)
{
	if (GLM_ProgramRecompileNeeded(&polygonProgram, 0)) {
		GL_VFDeclare(draw_polygon);

		if (!GLM_CreateVFProgram("polygon-draw", GL_VFParams(draw_polygon), &polygonProgram)) {
			return;
		}
	}

	if (!polygonProgram.uniforms_found) {
		polygonUniforms_matrix = glGetUniformLocation(polygonProgram.program, "matrix");
		polygonUniforms_color = glGetUniformLocation(polygonProgram.program, "color");
		polygonProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(polygonVBO)) {
		polygonVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "polygon-vbo", sizeof(polygonVertices), polygonVertices, GL_STREAM_DRAW);
	}
	else {
		GL_UpdateBuffer(polygonVBO, polygonVerts * sizeof(polygonVertices[0]), polygonVertices);
	}

	if (!polygonVAO.vao) {
		GL_GenVertexArray(&polygonVAO, "polygon-vao");
		GL_ConfigureVertexAttribPointer(&polygonVAO, polygonVBO, 0, 3, GL_FLOAT, GL_FALSE, 0, NULL, 0);
	}
}

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color)
{
	if (num_vertices > sizeof(polygonVertices) / sizeof(polygonVertices[0])) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_polygon, 0)) {
		return;
	}

	polygonVerts = num_vertices;
	polygonColor = color;
	polygonX = x;
	polygonY = y;
	memcpy(polygonVertices, vertices, sizeof(polygonVertices[0]) * num_vertices);
}

void GLM_DrawPolygonImpl(void)
{
	float matrix[16];
	float alpha;
	byte glColor[4];

	COLOR_TO_RGBA(polygonColor, glColor);

	alpha = glColor[3] / 255.0f;

	GL_Disable(GL_DEPTH_TEST);
	GLM_GetMatrix(GL_PROJECTION, matrix);
	GLM_TransformMatrix(matrix, polygonX, polygonY, 0);

	GL_BindVertexArray(&polygonVAO);
	GL_UseProgram(polygonProgram.program);
	glUniformMatrix4fv(polygonUniforms_matrix, 1, GL_FALSE, matrix);
	glUniform4f(polygonUniforms_color, glColor[0] * alpha / 255.0f, glColor[1] * alpha / 255.0f, glColor[2] * alpha / 255.0f, alpha);

	GL_DrawArrays(GL_TRIANGLE_STRIP, 0, polygonVerts);
}
