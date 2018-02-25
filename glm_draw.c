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

#define MAX_MULTI_IMAGE_BATCH 4096
#define CIRCLE_LINE_COUNT	40
extern float overall_alpha;

#define IMAGEPROG_FLAGS_TEXTURE     1
#define IMAGEPROG_FLAGS_ALPHATEST   2
#define IMAGEPROG_FLAGS_TEXT        4

static void GLM_PreparePolygon(void);
static void GLM_PrepareCircleDraw(void);
void GLM_DrawRectangle(float x, float y, float width, float height, byte* color);
void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);

static glm_program_t circleProgram;
static glm_vao_t circleVAO;
static buffer_ref circleVBO;
static GLint drawCircleUniforms_matrix;
static GLint drawCircleUniforms_color;

#define FLOATS_PER_CIRCLE ((3 + 2 * CIRCLE_LINE_COUNT) * 2)
#define CIRCLES_PER_FRAME 256

static float drawCirclePointData[FLOATS_PER_CIRCLE * CIRCLES_PER_FRAME];
static float drawCircleColors[CIRCLES_PER_FRAME][4];
static qbool drawCircleFill[CIRCLES_PER_FRAME];
static int drawCirclePoints[CIRCLES_PER_FRAME];
static int circleCount;
static void GLM_DrawCircles(int start, int end);

static void GLM_DrawPolygonImpl(void);

static glm_vao_t* GL_CreateLineVAO(void)
{
	static glm_vao_t vao;
	static buffer_ref vbo;

	float points[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
	};

	if (!GL_BufferReferenceIsValid(vbo)) {
		vbo = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "line", sizeof(points), points, GL_STATIC_DRAW);
	}

	if (!vao.vao) {
		GL_GenVertexArray(&vao, "line-vao");

		GL_ConfigureVertexAttribPointer(&vao, vbo, 0, 3, GL_FLOAT, GL_FALSE, 0, NULL, 0);
	}

	return &vao;
}

void GLM_DrawAlphaRectangeRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor)
{
	byte color[4] = { bytecolor[0], bytecolor[1], bytecolor[2], bytecolor[3] };

	if (color[3] != 255) {
		float alpha = color[3] / 255.0f;

		color[0] *= alpha;
		color[1] *= alpha;
		color[2] *= alpha;
	}

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
		GL_DrawArrays(GL_LINES, 0, 2);
	}
}

void GLM_Draw_FadeScreen(float alpha)
{
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
}

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

typedef struct glc_image_s {
	float pos[2];
	float tex[2];
	unsigned char colour[4];
} glc_image_t;

typedef enum {
	imagetype_image,
	imagetype_circle,
	imagetype_polygon
} glm_image_type_t;

static glm_image_type_t imageTypes[MAX_MULTI_IMAGE_BATCH];
static glm_image_t images[MAX_MULTI_IMAGE_BATCH];
static glc_image_t glc_images[MAX_MULTI_IMAGE_BATCH * 4];
static int imageCount = 0;
static glm_vao_t imageVAO;
static buffer_ref imageVBO;

static void GLC_SetCoordinates(glc_image_t* targ, float x1, float y1, float x2, float y2, float s, float tex_width, float t, float tex_height)
{
	float modelViewMatrix[16];
	float projectionMatrix[16];
	float matrix[16];
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };
	byte color[4];

	GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	GLM_MultiplyMatrix(projectionMatrix, modelViewMatrix, matrix);
	GLM_MultiplyVector(matrix, v1, v1);
	GLM_MultiplyVector(matrix, v2, v2);

	memcpy(color, targ->colour, 4);
	targ->pos[0] = v1[0];
	targ->pos[1] = v2[1];
	targ->tex[0] = s;
	targ->tex[1] = t + tex_height;

	++targ;
	memcpy(targ->colour, color, 4);
	targ->pos[0] = v1[0];
	targ->pos[1] = v1[1];
	targ->tex[0] = s;
	targ->tex[1] = t;

	++targ;
	memcpy(targ->colour, color, 4);
	targ->pos[0] = v2[0];
	targ->pos[1] = v1[1];
	targ->tex[0] = s + tex_width;
	targ->tex[1] = t;

	++targ;
	memcpy(targ->colour, color, 4);
	targ->pos[0] = v2[0];
	targ->pos[1] = v2[1];
	targ->tex[0] = s + tex_width;
	targ->tex[1] = t + tex_height;
}

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
		imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(images), images, GL_STREAM_DRAW);
	}

	if (!imageVAO.vao) {
		GL_GenVertexArray(&imageVAO, "image-vao");

		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, x1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, x2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 2, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, s1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 3, 2, GL_FLOAT, GL_FALSE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, s2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, colour), 0);
		GL_ConfigureVertexAttribIPointer(&imageVAO, imageVBO, 5, 1, GL_INT, sizeof(images[0]), VBO_FIELDOFFSET(glm_image_t, flags), 0);
	}
}

static void GL_DrawImageArraySequence(texture_ref texture, int start, int length)
{
	GL_UseProgram(multiImageProgram.program);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_Disable(GL_DEPTH_TEST);
	GL_BindVertexArray(&imageVAO);
	if (GL_TextureReferenceIsValid(texture)) {
		GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);
	}
	GL_DrawArrays(GL_POINTS, start, length);
}

static void GLM_PrepareImageDraw(void)
{
	GLM_CreateMultiImageProgram();
	GL_UpdateBuffer(imageVBO, sizeof(images[0]) * imageCount, images);

	GLM_PreparePolygon();
	GLM_PrepareCircleDraw();
}

static void GLM_FlushImageDraw(void)
{
	if (imageCount && glConfig.initialized) {
		int start = 0;
		int i;
		texture_ref currentTexture = null_texture_reference;

		for (i = 0; i < imageCount; ++i) {
			glm_image_t* img = &images[i];

			if (imageTypes[i] != imagetype_image || (GL_TextureReferenceIsValid(currentTexture) && GL_TextureReferenceIsValid(img->texNumber) && !GL_TextureReferenceEqual(currentTexture, img->texNumber))) {
				if (i - start) {
					GL_DrawImageArraySequence(currentTexture, start, i - start);
				}
				start = i;
			}

			if (imageTypes[i] != imagetype_image) {
				for (start = i; i < imageCount && imageTypes[i] != imagetype_image; ++i) {
					// Draw sub-batch
					if (imageTypes[i] == imagetype_circle) {
						GLM_DrawCircles(images[i].flags, images[i].flags);
					}
					else if (imageTypes[i] == imagetype_polygon) {
						GLM_DrawPolygonImpl();
					}
					++start;
				}
				--i;
			}
			else if (GL_TextureReferenceIsValid(img->texNumber)) {
				currentTexture = img->texNumber;
			}
		}

		if (imageCount - start) {
			GL_DrawImageArraySequence(currentTexture, start, imageCount - start);
		}
	}

	memset(imageTypes, 0, sizeof(imageTypes));
	imageCount = 0;
	circleCount = 0;
}

void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText)
{
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	imageTypes[imageCount] = imagetype_image;
	if (GL_ShadersSupported() || !GL_BuffersSupported()) {
		memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			images[imageCount].colour[0] *= alpha;
			images[imageCount].colour[1] *= alpha;
			images[imageCount].colour[2] *= alpha;
		}
		GLM_SetCoordinates(&images[imageCount], x, y, x + width, y + height);
		images[imageCount].s1 = tex_s;
		images[imageCount].s2 = tex_s + tex_width;
		images[imageCount].t1 = tex_t;
		images[imageCount].t2 = tex_t + tex_height;
	}
	else {
		int imageIndex = imageCount * 4;
		memcpy(&glc_images[imageIndex].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			glc_images[imageIndex].colour[0] *= alpha;
			glc_images[imageIndex].colour[1] *= alpha;
			glc_images[imageIndex].colour[2] *= alpha;
		}
		GLC_SetCoordinates(&glc_images[imageIndex], x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height);
	}

	images[imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;
	images[imageCount].flags |= (alpha_test ? IMAGEPROG_FLAGS_ALPHATEST : 0);
	images[imageCount].flags |= (isText ? IMAGEPROG_FLAGS_TEXT : 0);
	images[imageCount].texNumber = texnum;

	++imageCount;
}

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	imageTypes[imageCount] = imagetype_image;
	if (GL_ShadersSupported() || !GL_BuffersSupported()) {
		memcpy(&images[imageCount].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			images[imageCount].colour[0] *= alpha;
			images[imageCount].colour[1] *= alpha;
			images[imageCount].colour[2] *= alpha;
		}
		GLM_SetCoordinates(&images[imageCount], x, y, x + width, y + height);
		Atlas_SolidTextureCoordinates(&images[imageCount].texNumber, &images[imageCount].s1, &images[imageCount].t1);
		images[imageCount].s2 = images[imageCount].s1;
		images[imageCount].t2 = images[imageCount].t1;
	}
	else {
		int imageIndex = imageCount * 4;
		float s, t;

		memcpy(&glc_images[imageIndex].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			glc_images[imageIndex].colour[0] *= alpha;
			glc_images[imageIndex].colour[1] *= alpha;
			glc_images[imageIndex].colour[2] *= alpha;
		}
		Atlas_SolidTextureCoordinates(&images[imageCount].texNumber, &s, &t);
		GLC_SetCoordinates(&glc_images[imageIndex], x, y, x + width, y + height, s, 0, t, 0);
	}

	images[imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;

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

		if (GL_BuffersSupported()) {
			GL_BindVertexArray(NULL);
			GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);

			if (!GL_BufferReferenceIsValid(imageVBO)) {
				imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(glc_images), glc_images, GL_STREAM_DRAW);
				GL_BindBuffer(imageVBO);
			}
			else {
				GL_BindAndUpdateBuffer(imageVBO, imageCount * 4 * sizeof(glc_images[0]), glc_images);
			}

			glVertexPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, pos));
			glEnableClientState(GL_VERTEX_ARRAY);

			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, colour));
			glEnableClientState(GL_COLOR_ARRAY);

			qglClientActiveTexture(GL_TEXTURE0);
			glTexCoordPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, tex));
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		for (i = 0; i < imageCount; ++i) {
			qbool alpha_test = images[i].flags & IMAGEPROG_FLAGS_ALPHATEST;
			qbool texture = images[i].flags & IMAGEPROG_FLAGS_TEXTURE;
			qbool text = images[i].flags & IMAGEPROG_FLAGS_TEXT;

			GLC_EnsureTMUEnabled(GL_TEXTURE0);
			GL_EnsureTextureUnitBound(GL_TEXTURE0, images[i].texNumber);

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
			}

			if (GL_BuffersSupported()) {
				GL_DrawArrays(GL_QUADS, i * 4, (j - i) * 4);

				i = j - 1;
			}
			else {
				glBegin(GL_QUADS);

				while (i < j) {
					glm_image_t* next = &images[i];

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

				--i;
				glEnd();
			}
		}

		GL_PopMatrix(GL_PROJECTION, projectionMatrix);
		GL_PopMatrix(GL_MODELVIEW, modelviewMatrix);

		if (GL_BuffersSupported()) {
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
}

void GL_EmptyImageQueue(void)
{
	imageCount = 0;
}

void GL_FlushImageDraw(void)
{
	GLM_PrepareImageDraw();

	if (GL_ShadersSupported()) {
		GLM_FlushImageDraw();
	}
	else {
		GLC_FlushImageDraw();
	}

	imageCount = 0;
	circleCount = 0;
}

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

static void GLM_PreparePolygon(void)
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
	if (imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	imageTypes[imageCount] = imagetype_polygon;
	images[imageCount].flags = 0;
	imageCount++;

	polygonVerts = num_vertices;
	polygonColor = color;
	polygonX = x;
	polygonY = y;
	memcpy(polygonVertices, vertices, sizeof(polygonVertices[0]) * num_vertices);
}

static void GLM_DrawPolygonImpl(void)
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

static void GLM_DrawCircles(int start, int end)
{
	// FIXME: Not very efficient (but rarely used either)
	float projectionMatrix[16];
	int i;

	GL_GetMatrix(GL_PROJECTION, projectionMatrix);

	start = max(0, start);
	end = min(end, circleCount - 1);

	GL_UseProgram(circleProgram.program);
	GL_BindVertexArray(&circleVAO);

	glUniformMatrix4fv(drawCircleUniforms_matrix, 1, GL_FALSE, projectionMatrix);
	for (i = start; i <= end; ++i) {
		glUniform4fv(drawCircleUniforms_color, 1, drawCircleColors[i]);

		GL_DrawArrays(drawCircleFill[i] ? GL_TRIANGLE_STRIP : GL_LINE_LOOP, i * FLOATS_PER_CIRCLE / 2, drawCirclePoints[i]);
	}
}

static void GLM_PrepareCircleDraw(void)
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
		circleVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "circle-vbo", sizeof(drawCirclePointData), drawCirclePointData, GL_STREAM_DRAW);
	}
	else {
		GL_UpdateBuffer(circleVBO, sizeof(drawCirclePointData), drawCirclePointData);
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

	if (imageCount >= MAX_MULTI_IMAGE_BATCH || circleCount >= CIRCLES_PER_FRAME) {
		return;
	}

	imageTypes[imageCount] = imagetype_circle;
	images[imageCount].flags = circleCount;

	// Get the vertex index where to start and stop drawing.
	start = Q_rint((startangle * CIRCLE_LINE_COUNT) / (2 * M_PI));
	end = Q_rint((endangle * CIRCLE_LINE_COUNT) / (2 * M_PI));

	// If the end is less than the start, increase the index so that
	// we start on a "new" circle.
	if (end < start) {
		end = end + CIRCLE_LINE_COUNT;
	}

	points = 0;
	pointData = drawCirclePointData + (FLOATS_PER_CIRCLE * circleCount);
	COLOR_TO_RGBA(color, color_bytes);
	drawCircleColors[circleCount][0] = (color_bytes[0] / 255.0f) * overall_alpha;
	drawCircleColors[circleCount][1] = (color_bytes[1] / 255.0f) * overall_alpha;
	drawCircleColors[circleCount][2] = (color_bytes[2] / 255.0f) * overall_alpha;
	drawCircleColors[circleCount][3] = (color_bytes[3] / 255.0f) * overall_alpha;
	drawCircleFill[circleCount] = fill;
	++imageCount;
	++circleCount;

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

	drawCirclePoints[circleCount - 1] = points;
}
