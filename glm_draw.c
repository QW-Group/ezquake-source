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

#define IMAGEPROG_FLAGS_TEXTURE     1
#define IMAGEPROG_FLAGS_ALPHATEST   2
#define IMAGEPROG_FLAGS_TEXT        4

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color);

static GLuint GL_CreateLineVAO(void)
{
	static glm_vao_t vao;
	static glm_vbo_t vbo;

	float points[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};

	if (!vbo.vbo) {
		GL_GenBuffer(&vbo, "line");
		glBindBufferExt(GL_ARRAY_BUFFER, vbo.vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
	}

	if (!vao.vao) {
		GL_GenVertexArray(&vao);
		GL_BindVertexArray(vao.vao);
		glEnableVertexAttribArray(0);
		glBindBufferExt(GL_ARRAY_BUFFER, vbo.vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	return vao.vao;
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

void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	// MEAG: TODO
}

void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	byte color[] = { 255, 255, 255, 255 };
	if (alpha < 1.0) {
		color[3] = alpha * 255;
	}

	GLM_DrawImage(x, y, scale_x * src_width, scale_y * src_height, 0, newsl, newtl, newsh - newsl, newth - newtl, color, alpha < 1.0, pic->texnum, false);
}

void GLM_Draw_LineRGB(byte* color, int x_start, int y_start, int x_end, int y_end)
{
	static glm_program_t line_program;
	static GLint line_matrix;
	static GLint line_color;

	if (!line_program.program) {
		GL_VFDeclare(line_draw);

		// Very simple line-drawing
		GLM_CreateVFProgram(
			"LineDrawing",
			GL_VFParams(line_draw),
			&line_program
		);
	}

	if (line_program.program && !line_program.uniforms_found) {
		line_matrix = glGetUniformLocation(line_program.program, "matrix");
		line_color = glGetUniformLocation(line_program.program, "inColor");
		line_program.uniforms_found = true;
	}

	if (line_program.program) {
		float matrix[16];

		glDisable(GL_DEPTH_TEST);
		GLM_GetMatrix(GL_PROJECTION, matrix);
		GLM_TransformMatrix(matrix, x_start, y_start, 0);
		GLM_ScaleMatrix(matrix, x_end - x_start, y_end - y_start, 1.0f);

		GL_UseProgram(line_program.program);
		glUniformMatrix4fv(line_matrix, 1, GL_FALSE, matrix);
		glUniform4f(line_color, color[0] * 1.0 / 255, color[1] * 1.0 / 255, color[2] * 1.0 / 255, 1.0f);

		GL_BindVertexArray(GL_CreateLineVAO());
		glDrawArrays(GL_LINES, 0, 2);
	}
}

void GLM_Draw_FadeScreen(float alpha)
{
	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	}
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
	if (alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	}
}

#define MAX_MULTI_IMAGE_BATCH 1024

static glm_program_t multiImageProgram;
static GLint multiImage_modelViewMatrix;
static GLint multiImage_projectionMatrix;
static GLint multiImage_tex;

typedef struct glm_image_s {
	float x1, y1;
	float x2, y2;
	float s1, t1;
	float s2, t2;
	unsigned char colour[4];
	int flags;
	int texNumber;
} glm_image_t;

static glm_image_t images[MAX_MULTI_IMAGE_BATCH];
static int imageCount = 0;
static glm_vao_t imageVAO;
static glm_vbo_t imageVBO;

void GLM_CreateMultiImageProgram(void)
{
	if (!multiImageProgram.program) {
		GL_VGFDeclare(multi_image_draw);

		// Initialise program for drawing image
		GLM_CreateVGFProgram("Multi-image", GL_VGFParams(multi_image_draw), &multiImageProgram);
	}

	if (multiImageProgram.program && !multiImageProgram.uniforms_found) {
		multiImage_modelViewMatrix = glGetUniformLocation(multiImageProgram.program, "modelViewMatrix");
		multiImage_projectionMatrix = glGetUniformLocation(multiImageProgram.program, "projectionMatrix");
		multiImage_tex = glGetUniformLocation(multiImageProgram.program, "tex");
		multiImageProgram.uniforms_found = true;

		glProgramUniform1i(multiImageProgram.program, multiImage_tex, 0);
	}

	if (!imageVBO.vbo) {
		GL_GenBuffer(&imageVBO, __FUNCTION__);
		glBindBufferExt(GL_ARRAY_BUFFER, imageVBO.vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(images), images, GL_STATIC_DRAW);
	}

	if (!imageVAO.vao) {
		GL_GenVertexArray(&imageVAO);
		GL_BindVertexArray(imageVAO.vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glEnableVertexAttribArray(4);
		glEnableVertexAttribArray(5);
		glBindBufferExt(GL_ARRAY_BUFFER, imageVBO.vbo);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), (GLvoid*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), (GLvoid*) 8);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), (GLvoid*) 16);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), (GLvoid*) 24);
		glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(images[0]), (GLvoid*) 32);
		glVertexAttribIPointer(5, 1, GL_INT, sizeof(images[0]), (GLvoid*)36);
	}
}

void GLM_FlushImageDraw(void)
{
	float modelViewMatrix[16];
	float projectionMatrix[16];

	if (imageCount) {
		int start = 0;
		int i;
		int currentTexture = 0;

		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_SelectTexture(GL_TEXTURE0);

		GLM_CreateMultiImageProgram();

		glBindBufferExt(GL_ARRAY_BUFFER, imageVBO.vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(images[0]) * imageCount, images, GL_STATIC_DRAW);

		glDisable(GL_DEPTH_TEST);
		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		GL_UseProgram(multiImageProgram.program);
		glUniformMatrix4fv(multiImage_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(multiImage_projectionMatrix, 1, GL_FALSE, projectionMatrix);

		GL_BindVertexArray(imageVAO.vao);

		for (i = 0; i < imageCount; ++i) {
			glm_image_t* img = &images[i];

			if (currentTexture > 0 && img->texNumber && currentTexture != img->texNumber) {
				GL_Bind(currentTexture);
				glDrawArrays(GL_POINTS, start, i - start);
				start = i;
			}

			if (img->texNumber) {
				currentTexture = img->texNumber;
			}
		}

		if (currentTexture) {
			GL_Bind(currentTexture);
		}
		glDrawArrays(GL_POINTS, start, imageCount - start);
		glEnable(GL_DEPTH_TEST);
	}

	imageCount = 0;
}

void GLM_DrawImage(float x, float y, float width, float height, int texture_unit, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha, int texnum, qbool isText)
{
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		GLM_FlushImageDraw();
	}

	memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
	images[imageCount].x1 = x;
	images[imageCount].y1 = y;
	images[imageCount].x2 = x + width;
	images[imageCount].y2 = y + height;
	images[imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;
	if (alpha) {
		images[imageCount].flags |= IMAGEPROG_FLAGS_ALPHATEST;
	}
	if (isText) {
		images[imageCount].flags |= IMAGEPROG_FLAGS_TEXT;
	}
	images[imageCount].s1 = tex_s;
	images[imageCount].s2 = tex_s + tex_width;
	images[imageCount].t1 = tex_t;
	images[imageCount].t2 = tex_t + tex_height;
	images[imageCount].texNumber = texnum;

	++imageCount;
}

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		GLM_FlushImageDraw();
	}

	memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
	images[imageCount].x1 = x;
	images[imageCount].y1 = y;
	images[imageCount].x2 = x + width;
	images[imageCount].y2 = y + height;
	images[imageCount].flags = 0;
	images[imageCount].s1 = images[imageCount].s2 = images[imageCount].t1 = images[imageCount].t2 = 0;
	images[imageCount].texNumber = 0;

	++imageCount;
}
