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
#include "glm_draw.h"
#include "glm_vao.h"
#include "r_state.h"

static glm_program_t line_program;
static buffer_ref line_vbo;
static GLint line_matrix;

glm_line_framedata_t lineData;

void GLM_PrepareLines(void)
{
	if (GL_UseGLSL()) {
		if (!GL_BufferReferenceIsValid(line_vbo)) {
			line_vbo = GL_CreateFixedBuffer(buffertype_vertex, "line", sizeof(lineData.line_points), lineData.line_points, bufferusage_once_per_frame);
		}
		else if (lineData.lineCount) {
			GL_UpdateBuffer(line_vbo, sizeof(lineData.line_points[0]) * lineData.lineCount * 2, lineData.line_points);
		}

		if (vao_hud_lines) {
			GL_GenVertexArray(vao_hud_lines, "line-vao");

			GL_ConfigureVertexAttribPointer(vao_hud_lines, line_vbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof(glm_line_point_t), VBO_FIELDOFFSET(glm_line_point_t, position), 0);
			GL_ConfigureVertexAttribPointer(vao_hud_lines, line_vbo, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glm_line_point_t), VBO_FIELDOFFSET(glm_line_point_t, color), 0);
		}

		if (GLM_ProgramRecompileNeeded(&line_program, 0)) {
			GL_VFDeclare(hud_draw_line);

			// Very simple line-drawing
			GLM_CreateVFProgram(
				"LineDrawing",
				GL_VFParams(hud_draw_line),
				&line_program
			);
		}

		if (line_program.program && !line_program.uniforms_found) {
			line_matrix = GL_UniformGetLocation(line_program.program, "matrix");
			line_program.uniforms_found = true;
		}
	}
}

void GLM_Draw_Line3D(byte* color, vec3_t start, vec3_t end)
{
	if (lineData.lineCount >= MAX_LINES_PER_FRAME) {
		return;
	}
}

void GLM_Draw_LineRGB(float thickness, byte* color, int x_start, int y_start, int x_end, int y_end)
{
	if (lineData.lineCount >= MAX_LINES_PER_FRAME) {
		return;
	}
	if (!GLM_LogCustomImageType(imagetype_line, lineData.lineCount)) {
		return;
	}

	VectorSet(lineData.line_points[lineData.lineCount * 2 + 0].position, x_start, y_start, 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 0].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	VectorSet(lineData.line_points[lineData.lineCount * 2 + 1].position, x_end, y_end, 0);
	memcpy(lineData.line_points[lineData.lineCount * 2 + 1].color, color, sizeof(lineData.line_points[lineData.lineCount * 2 + 0].color));
	lineData.line_thickness[lineData.lineCount] = thickness;
	++lineData.lineCount;
}

void GLM_DrawLines(int start, int end)
{
	if (line_program.program && GL_VertexArrayCreated(vao_hud_lines)) {
		float matrix[16];
		int i;
		uintptr_t offset = GL_BufferOffset(line_vbo) / sizeof(glm_line_point_t);

		GL_UseProgram(line_program.program);
		GLM_GetMatrix(GL_PROJECTION, matrix);
		GL_UniformMatrix4fv(line_matrix, 1, GL_FALSE, matrix);
		GL_BindVertexArray(vao_hud_lines);

		for (i = start; i <= end; ++i) {
			GL_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
			GL_DrawArrays(GL_LINES, offset + i * 2, 2);
			GL_StateEndAlphaLineRGB();
		}
	}
}

void GLC_DrawLines(int start, int end)
{
	int i;

	for (i = start; i <= end; ++i) {
		GL_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
		glBegin(GL_LINES);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 0].color);
		glVertex3fv(lineData.line_points[i * 2 + 0].position);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 1].color);
		glVertex3fv(lineData.line_points[i * 2 + 1].position);
		glEnd();
		GL_StateEndAlphaLineRGB();
	}
}
