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

#define CIRCLE_LINE_COUNT	40
extern float overall_alpha;

#define IMAGEPROG_FLAGS_TEXTURE     1
#define IMAGEPROG_FLAGS_ALPHATEST   2
#define IMAGEPROG_FLAGS_TEXT        4

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color);

static glm_vao_t* GL_CreateLineVAO(void)
{
	static glm_vao_t vao;
	static buffer_ref vbo;

	float points[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};

	if (!GL_BufferReferenceIsValid(vbo)) {
		vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "line", sizeof(points), points, GL_DYNAMIC_DRAW);
	}

	if (!vao.vao) {
		GL_GenVertexArray(&vao, "line-vao");

		GL_ConfigureVertexAttribPointer(&vao, vbo, 0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	return &vao;
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

void GLM_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	byte color[] = { 255, 255, 255, 255 };
	if (alpha < 1.0) {
		color[3] = alpha * 255;
	}

	GLM_DrawImage(x, y, scale_x * src_width, scale_y * src_height, newsl, newtl, newsh - newsl, newth - newtl, color, false, pic->texnum, false);
}

void GLM_Draw_LineRGB(byte* color, int x_start, int y_start, int x_end, int y_end)
{
	static glm_program_t line_program;
	static GLint line_matrix;
	static GLint line_color;

	if (GLM_ProgramRecompileNeeded(&line_program, 0)) {
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
		++frameStats.draw_calls;
	}
}

void GLM_Draw_FadeScreen(float alpha)
{
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
}

#define MAX_MULTI_IMAGE_BATCH 1024

static glm_program_t multiImageProgram;

typedef struct glm_image_s {
	float x1, y1;
	float x2, y2;
	float s1, t1;
	float s2, t2;
	unsigned char colour[4];
	int flags;
	texture_ref texNumber;
} glm_image_t;

static glm_image_t images[MAX_MULTI_IMAGE_BATCH];
static int imageCount = 0;
static glm_vao_t imageVAO;
static buffer_ref imageVBO;

static void GLM_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2)
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float matrix[16];
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };

	GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	GLM_MultiplyMatrix(projectionMatrix, modelViewMatrix, matrix);
	GLM_MultiplyVector(matrix, v1, v1);
	GLM_MultiplyVector(matrix, v2, v2);

	targ->x1 = v1[0];
	targ->y1 = v1[1];
	targ->x2 = v2[0];
	targ->y2 = v2[1];
}

void GLM_CreateMultiImageProgram(void)
{
	if (GLM_ProgramRecompileNeeded(&multiImageProgram, 0)) {
		GL_VGFDeclare(multi_image_draw);

		// Initialise program for drawing image
		GLM_CreateVGFProgram("Multi-image", GL_VGFParams(multi_image_draw), &multiImageProgram);
	}

	if (multiImageProgram.program && !multiImageProgram.uniforms_found) {
		multiImageProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(imageVBO)) {
		imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(images), images, GL_DYNAMIC_DRAW);
	}

	if (!imageVAO.vao) {
		GL_GenVertexArray(&imageVAO, "image-vao");

		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, x1));
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, x2));
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 2, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, s1));
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 3, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, s2));
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, colour));
		GL_ConfigureVertexAttribIPointer(&imageVAO, imageVBO, 5, 1, GL_INT, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, flags));
	}
}

static void GLM_FlushImageDraw(void)
{
	extern cvar_t gl_alphafont;

	if (imageCount && glConfig.initialized) {
		int start = 0;
		int i;
		texture_ref currentTexture = null_texture_reference;

		GLM_CreateMultiImageProgram();
		GL_UpdateBuffer(imageVBO, sizeof(images[0]) * imageCount, images);
		GL_UseProgram(multiImageProgram.program);
		GL_BindVertexArray(&imageVAO);

		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);

		for (i = 0; i < imageCount; ++i) {
			glm_image_t* img = &images[i];

			if (GL_TextureReferenceIsValid(currentTexture) && GL_TextureReferenceIsValid(img->texNumber) && !GL_TextureReferenceEqual(currentTexture, img->texNumber)) {
				GL_EnsureTextureUnitBound(GL_TEXTURE0, currentTexture);
				glDrawArrays(GL_POINTS, start, i - start);
				++frameStats.draw_calls;
				start = i;
			}

			if (GL_TextureReferenceIsValid(img->texNumber)) {
				currentTexture = img->texNumber;
			}
		}

		if (GL_TextureReferenceIsValid(currentTexture)) {
			GL_EnsureTextureUnitBound(GL_TEXTURE0, currentTexture);
		}
		glDrawArrays(GL_POINTS, start, imageCount - start);
		++frameStats.draw_calls;
	}

	imageCount = 0;
}

void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha, texture_ref texnum, qbool isText)
{
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		GL_FlushImageDraw(true);
	}

	memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
	GLM_SetCoordinates(&images[imageCount], x, y, x + width, y + height);
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
		GL_FlushImageDraw(true);
	}

	memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
	GLM_SetCoordinates(&images[imageCount], x, y, x + width, y + height);
	images[imageCount].flags = 0;
	images[imageCount].s1 = images[imageCount].s2 = images[imageCount].t1 = images[imageCount].t2 = 0;
	GL_TextureReferenceInvalidate(images[imageCount].texNumber);

	++imageCount;
}

static void GLC_FlushImageDraw(void)
{
	if (imageCount) {
		int i, j;
		extern cvar_t gl_alphafont, scr_coloredText;
		float modelviewMatrix[16];
		float projectionMatrix[16];
		byte current_color[4];

		GL_PushMatrix(GL_MODELVIEW, modelviewMatrix);
		GL_PushMatrix(GL_PROJECTION, projectionMatrix);

		GL_IdentityModelView();
		GL_IdentityProjectionView();

		GLC_StateBeginImageDraw();

		for (i = 0; i < imageCount; ++i) {
			qbool alpha_test = images[i].flags & IMAGEPROG_FLAGS_ALPHATEST;
			qbool texture = images[i].flags & IMAGEPROG_FLAGS_TEXTURE;
			qbool text = images[i].flags & IMAGEPROG_FLAGS_TEXT;

			if (texture) {
				GLC_EnsureTMUEnabled(GL_TEXTURE0);
				GL_EnsureTextureUnitBound(GL_TEXTURE0, images[i].texNumber);
			}
			else {
				GLC_EnsureTMUDisabled(GL_TEXTURE0);
			}

			if (text) {
				alpha_test = !gl_alphafont.integer;

				if (scr_coloredText.integer) {
					GL_TextureEnvMode(GL_MODULATE);
				}
				else {
					GL_TextureEnvMode(GL_REPLACE);
				}
			}
			else {
				GL_TextureEnvMode(GL_MODULATE);
			}

			GL_AlphaBlendFlags(alpha_test ? GL_ALPHATEST_ENABLED : GL_ALPHATEST_DISABLED);
			GL_Color4ubv(images[i].colour);
			memcpy(current_color, images[i].colour, sizeof(current_color));

			glBegin(GL_QUADS);
			for (j = i; j < imageCount; ++j) {
				glm_image_t* next = &images[j];
				qbool next_alpha_test = next->flags & IMAGEPROG_FLAGS_ALPHATEST;
				qbool next_texture = next->flags & IMAGEPROG_FLAGS_TEXTURE;
				qbool next_text = next->flags & IMAGEPROG_FLAGS_TEXT;

				if (next_text) {
					next_alpha_test = !gl_alphafont.integer;
					if (!text && !scr_coloredText.integer) {
						break; // will need to toggle TMU
					}
				}

				if (next_alpha_test != alpha_test) {
					GL_MarkEvent("(break for alpha-test)");
					break;
				}
				if (next_texture != texture) {
					GL_MarkEvent("(break for texturing-toggle)");
					break;
				}
				if (next_texture && !GL_TextureReferenceEqual(images[i].texNumber, next->texNumber)) {
					GL_MarkEvent("(break for texture)");
					break;
				}

				// Don't need to break for colour
				if (memcmp(next->colour, current_color, sizeof(current_color))) {
					memcpy(current_color, next->colour, sizeof(current_color));
					GL_Color4ubv(next->colour);
				}

				if (texture) {
					glTexCoord2f(next->s1, next->t2);
				}
				glVertex2f(next->x1, next->y2);
				if (texture) {
					glTexCoord2f(next->s1, next->t1);
				}
				glVertex2f(next->x1, next->y1);
				if (texture) {
					glTexCoord2f(next->s2, next->t1);
				}
				glVertex2f(next->x2, next->y1);
				if (texture) {
					glTexCoord2f(next->s2, next->t2);
				}
				glVertex2f(next->x2, next->y2);
			}
			glEnd();

			i = j - 1;
		}

		GL_PopMatrix(GL_PROJECTION, projectionMatrix);
		GL_PopMatrix(GL_MODELVIEW, modelviewMatrix);
	}
}

void GL_FlushImageDraw(qbool draw)
{
	if (!draw || !imageCount) {
		imageCount = 0;
		return;
	}

	if (GL_ShadersSupported()) {
		GLM_FlushImageDraw();
	}
	else {
		GLC_FlushImageDraw();
	}

	imageCount = 0;
}

static glm_program_t polygonProgram;
static glm_vao_t polygonVAO;
static buffer_ref polygonVBO;
static GLint polygonUniforms_matrix;
static GLint polygonUniforms_color;

void GLM_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color)
{
	GL_FlushImageDraw(true);

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
		polygonVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "polygon-vbo", sizeof(vertices[0]) * max(num_vertices, 32), vertices, GL_DYNAMIC_DRAW);
	}
	else if (num_vertices * sizeof(vertices[0]) > GL_VBOSize(polygonVBO)) {
		GL_ResizeBuffer(polygonVBO, num_vertices * sizeof(vertices[0]), vertices);
	}
	else {
		GL_UpdateBuffer(polygonVBO, num_vertices * sizeof(vertices[0]), vertices);
	}

	if (!polygonVAO.vao) {
		GL_GenVertexArray(&polygonVAO, "polygon-vao");
		GL_ConfigureVertexAttribPointer(&polygonVAO, polygonVBO, 0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	{
		float matrix[16];
		byte glColor[4];

		COLOR_TO_RGBA(color, glColor);

		GL_Disable(GL_DEPTH_TEST);
		GLM_GetMatrix(GL_PROJECTION, matrix);
		GLM_TransformMatrix(matrix, x, y, 0);

		GL_BindVertexArray(&polygonVAO);
		GL_UseProgram(polygonProgram.program);
		glUniformMatrix4fv(polygonUniforms_matrix, 1, GL_FALSE, matrix);
		glUniform4f(polygonUniforms_color, glColor[0] / 255.0f, glColor[1] / 255.0f, glColor[2] / 255.0f, glColor[3] / 255.0f);

		glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertices);
		++frameStats.draw_calls;
	}
}

static glm_program_t circleProgram;
static glm_vao_t circleVAO;
static buffer_ref circleVBO;
static GLint drawCircleUniforms_matrix;
static GLint drawCircleUniforms_color;

void GLM_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color)
{
	float projectionMatrix[16];
	float pointData[(3 + 2 * CIRCLE_LINE_COUNT) * 2];
	byte bytecolor[4];
	double angle;
	int i;
	int start;
	int end;
	int points;

	GL_FlushImageDraw(true);

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
		circleVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "circle-vbo", sizeof(pointData), NULL, GL_STREAM_DRAW);
	}

	// Build VAO
	if (!circleVAO.vao) {
		GL_GenVertexArray(&circleVAO, "circle-vao");

		GL_ConfigureVertexAttribPointer(&circleVAO, circleVBO, 0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	// Get the vertex index where to start and stop drawing.
	start = Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end = Q_rint((endangle * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if (end < start) {
		end = end + CIRCLE_LINE_COUNT;
	}

	// Create a vertex at the exact position specified by the start angle.
	points = 0;
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

	GL_UpdateBuffer(circleVBO, sizeof(pointData[0]) * points * 2, pointData);
	GL_UseProgram(circleProgram.program);
	GL_BindVertexArray(&circleVAO);
	GL_GetMatrix(GL_PROJECTION, projectionMatrix);
	COLOR_TO_RGBA(color, bytecolor);
	glUniform4f(drawCircleUniforms_color, bytecolor[0] / 255.0f, bytecolor[1] / 255.0f, bytecolor[2] / 255.0f, (bytecolor[3] / 255.0f) * overall_alpha);
	glUniformMatrix4fv(drawCircleUniforms_matrix, 1, GL_FALSE, projectionMatrix);
	glDrawArrays(fill ? GL_TRIANGLE_STRIP : GL_LINE_LOOP, 0, points);
	++frameStats.draw_calls;
}
