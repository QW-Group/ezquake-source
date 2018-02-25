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
#include "utils.h"
#include "common_draw.h"
#ifndef __APPLE__
#include "tr_types.h"
#endif
#include "glm_draw.h"

static glm_program_t circleProgram;
static glm_vao_t circleVAO;
static buffer_ref circleVBO;
static GLint drawCircleUniforms_matrix;
static GLint drawCircleUniforms_color;

glm_circle_framedata_t circleData;

extern float overall_alpha;

void GLM_DrawCircles(int start, int end)
{
	// FIXME: Not very efficient (but rarely used either)
	float projectionMatrix[16];
	int i;
	uintptr_t offset = GL_BufferOffset(circleVBO) / (sizeof(float) * 2);

	start = max(0, start);
	end = min(end, circleData.circleCount - 1);

	GL_UseProgram(circleProgram.program);
	GL_BindVertexArray(&circleVAO);

	GL_GetMatrix(GL_PROJECTION, projectionMatrix);
	glUniformMatrix4fv(drawCircleUniforms_matrix, 1, GL_FALSE, projectionMatrix);

	for (i = start; i <= end; ++i) {
		glUniform4fv(drawCircleUniforms_color, 1, circleData.drawCircleColors[i]);

		GL_DrawArrays(circleData.drawCircleFill[i] ? GL_TRIANGLE_STRIP : GL_LINE_LOOP, offset + i * FLOATS_PER_CIRCLE / 2, circleData.drawCirclePoints[i]);
	}
}

void GLM_PrepareCircleDraw(void)
{
	if (GLM_ProgramRecompileNeeded(&circleProgram, 0)) {
		GL_VFDeclare(draw_circle);

		if (!GLM_CreateVFProgram("circle-draw", GL_VFParams(draw_circle), &circleProgram)) {
			return;
		}
	}

	if (!circleProgram.uniforms_found) {
		drawCircleUniforms_matrix = glGetUniformLocation(circleProgram.program, "matrix");
		drawCircleUniforms_color = glGetUniformLocation(circleProgram.program, "color");

		circleProgram.uniforms_found = false;
	}

	// Build VBO
	if (!GL_BufferReferenceIsValid(circleVBO)) {
		circleVBO = GL_CreateFixedBuffer(GL_ARRAY_BUFFER, "circle-vbo", sizeof(circleData.drawCirclePointData), circleData.drawCirclePointData, write_once_use_once);
	}
	else if (circleData.circleCount) {
		GL_UpdateBuffer(circleVBO, circleData.circleCount * FLOATS_PER_CIRCLE * sizeof(circleData.drawCirclePointData[0]), circleData.drawCirclePointData);
	}

	// Build VAO
	if (!circleVAO.vao) {
		GL_GenVertexArray(&circleVAO, "circle-vao");

		GL_ConfigureVertexAttribPointer(&circleVAO, circleVBO, 0, 2, GL_FLOAT, GL_FALSE, 0, NULL, 0);
	}
}

void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	float* pointData;
	double angle;
	byte color_bytes[4];
	int i;
	int start;
	int end;
	int points;

	if (circleData.circleCount >= CIRCLES_PER_FRAME) {
		return;
	}

	if (!GLM_LogCustomImageType(imagetype_circle, circleData.circleCount)) {
		return;
	}

	// Get the vertex index where to start and stop drawing.
	start = Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end = Q_rint((endangle * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if (end < start) {
		end = end + CIRCLE_LINE_COUNT;
	}

	points = 0;
	pointData = circleData.drawCirclePointData + (FLOATS_PER_CIRCLE * circleData.circleCount);
	COLOR_TO_RGBA(color, color_bytes);
	circleData.drawCircleColors[circleData.circleCount][0] = (color_bytes[0] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][1] = (color_bytes[1] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][2] = (color_bytes[2] / 255.0f) * overall_alpha;
	circleData.drawCircleColors[circleData.circleCount][3] = (color_bytes[3] / 255.0f) * overall_alpha;
	circleData.drawCircleFill[circleData.circleCount] = fill;
	++circleData.circleCount;

	// Create a vertex at the exact position specified by the start angle.
	pointData[points * 2 + 0] = x + radius * cos(startangle);
	pointData[points * 2 + 1] = y - radius * sin(startangle);
	++points;

	// TODO: Use lookup table for sin/cos?
	for (i = start; i < end; i++) {
		angle = (i * 2 * M_PI) / CIRCLE_LINE_COUNT;
		pointData[points * 2 + 0] = x + radius * cos(angle);
		pointData[points * 2 + 1] = y - radius * sin(angle);
		++points;

		// When filling we're drawing triangles so we need to
		// create a vertex in the middle of the vertex to fill
		// the entire pie slice/circle.
		if (fill) {
			pointData[points * 2 + 0] = x;
			pointData[points * 2 + 1] = y;
			++points;
		}
	}

	pointData[points * 2 + 0] = x + radius * cos(endangle);
	pointData[points * 2 + 1] = y - radius * sin(endangle);
	++points;

	// Create a vertex for the middle point if we're not drawing a complete circle.
	if (endangle - startangle < 2 * M_PI) {
		pointData[points * 2 + 0] = x;
		pointData[points * 2 + 1] = y;
		++points;
	}

	circleData.drawCirclePoints[circleData.circleCount - 1] = points;
}
