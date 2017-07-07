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

$Id: gl_draw.c,v 1.104 2007-10-18 05:28:23 dkure Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#ifndef __APPLE__
#include "tr_types.h"
#endif

// Temp: very simple program to draw single texture on-screen
// imageProgram.program()
static glm_program_t imageProgram;
static GLint imageProgram_matrix;
static GLint imageProgram_tex;
static GLint imageProgram_sbase;
static GLint imageProgram_swidth;
static GLint imageProgram_tbase;
static GLint imageProgram_twidth;
static GLint imageProgram_color;
static GLint imageProgram_alphatest;

static GLuint GL_CreateRectangleVAO(void);

void GLM_DrawImage(float x, float y, float width, float height, int texture_unit, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha)
{
	// Matrix is transform > (x, y), stretch > x + (scale_x * src_width), y + (scale_y * src_height)
	float matrix[16];
	float inColor[4] = {
		color[0] * 1.0f / 255,
		color[1] * 1.0f / 255,
		color[2] * 1.0f / 255,
		color[3] * 1.0f / 255
	};

	if (!imageProgram.program) {
		GL_VFDeclare(image_draw);

		// Initialise program for drawing image
		GLM_CreateVFProgram("Image test", GL_VFParams(image_draw), &imageProgram);
		imageProgram_matrix = glGetUniformLocation(imageProgram.program, "matrix");
		imageProgram_tex = glGetUniformLocation(imageProgram.program, "tex");
		imageProgram_sbase = glGetUniformLocation(imageProgram.program, "sbase");
		imageProgram_swidth = glGetUniformLocation(imageProgram.program, "swidth");
		imageProgram_tbase = glGetUniformLocation(imageProgram.program, "tbase");
		imageProgram_twidth = glGetUniformLocation(imageProgram.program, "twidth");
		imageProgram_color = glGetUniformLocation(imageProgram.program, "color");
		imageProgram_alphatest = glGetUniformLocation(imageProgram.program, "alphatest");
	}

	glDisable(GL_DEPTH_TEST);
	GLM_GetMatrix(GL_PROJECTION, matrix);
	GLM_TransformMatrix(matrix, x, y, 0);
	GLM_ScaleMatrix(matrix, width, height, 1.0f);

	GL_UseProgram(imageProgram.program);
	glUniformMatrix4fv(imageProgram_matrix, 1, GL_FALSE, matrix);
	glUniform4f(imageProgram_color, inColor[0], inColor[1], inColor[2], inColor[3]);
	glUniform1i(imageProgram_tex, texture_unit);
	glUniform1f(imageProgram_sbase, tex_s);
	glUniform1f(imageProgram_swidth, tex_width);
	glUniform1f(imageProgram_tbase, tex_t);
	glUniform1f(imageProgram_twidth, tex_height);
	glUniform1f(imageProgram_alphatest, alpha);

	glBindVertexArray(GL_CreateRectangleVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

//
static glm_program_t rectProgram;
static GLint rectProgram_matrix;
static GLint rectProgram_color;

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	// Matrix is transform > (x, y), stretch > x + (scale_x * src_width), y + (scale_y * src_height)
	float matrix[16];
	float inColor[4] = {
		color[0] * 1.0f / 255,
		color[1] * 1.0f / 255,
		color[2] * 1.0f / 255,
		color[3] * 1.0f / 255
	};

	if (!rectProgram.program) {
		GL_VFDeclare(rectangle_draw);

		// Initialise program for drawing image
		GLM_CreateVFProgram("DrawRectangle", GL_VFParams(rectangle_draw), &rectProgram);
		rectProgram_matrix = glGetUniformLocation(rectProgram.program, "matrix");
		rectProgram_color = glGetUniformLocation(rectProgram.program, "color");
	}

	glDisable(GL_DEPTH_TEST);
	GLM_GetMatrix(GL_PROJECTION, matrix);
	GLM_TransformMatrix(matrix, x, y, 0);
	GLM_ScaleMatrix(matrix, width, height, 1.0f);

	GL_UseProgram(rectProgram.program);
	glUniformMatrix4fv(rectProgram_matrix, 1, GL_FALSE, matrix);
	glUniform4f(rectProgram_color, inColor[0], inColor[1], inColor[2], inColor[3]);

	glBindVertexArray(GL_CreateRectangleVAO());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static GLuint GL_CreateRectangleVAO(void)
{
	static GLuint vao;
	static GLuint vbo;

	float points[] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f
	};

	if (!vbo) {
		glGenBuffers(1, &vbo);
		glBindBufferExt(GL_ARRAY_BUFFER, vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	}

	if (!vao) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glEnableVertexAttribArray(0);
		glBindBufferExt(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	return vao;
}

void GLM_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor)
{
	if (fill) {
		GLM_DrawRectangle(x, y, w, h, bytecolor);
	}
	else {
		GLM_DrawRectangle(x, y, w, thickness, bytecolor);
		GLM_DrawRectangle(x, y + thickness, thickness, h - thickness, bytecolor);
		GLM_DrawRectangle(x + w - thickness, y + thickness, thickness, h - thickness, bytecolor);
		GLM_DrawRectangle(x, y + h, w, thickness, bytecolor);
	}
}

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, qbool fill, color_t color)
{
	// MEAG: TODO
}
