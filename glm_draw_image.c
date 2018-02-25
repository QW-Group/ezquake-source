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

void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);

#define IMAGEPROG_FLAGS_TEXTURE     1
#define IMAGEPROG_FLAGS_ALPHATEST   2
#define IMAGEPROG_FLAGS_TEXT        4

static glm_program_t multiImageProgram;

static glm_vao_t imageVAO;
static buffer_ref imageVBO;

glm_image_framedata_t imageData;

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
		imageVBO = GL_CreateFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(imageData.images), imageData.images, write_once_use_once);
	}

	if (!imageVAO.vao) {
		GL_GenVertexArray(&imageVAO, "image-vao");

		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, x1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, x2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 2, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, s1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 3, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, s2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, colour), 0);
		GL_ConfigureVertexAttribIPointer(&imageVAO, imageVBO, 5, 1, GL_INT, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, flags), 0);
	}
}

void GLM_DrawImageArraySequence(texture_ref texture, int start, int end)
{
	uintptr_t offset = GL_BufferOffset(imageVBO) / sizeof(imageData.images[0]);

	GL_UseProgram(multiImageProgram.program);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_Disable(GL_DEPTH_TEST);
	GL_BindVertexArray(&imageVAO);
	if (GL_TextureReferenceIsValid(texture)) {
		GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);
	}
	GL_DrawArrays(GL_POINTS, offset + start, end - start + 1);
}

void GLC_DrawImageArraySequence(texture_ref ref, int start, int end)
{
	float modelviewMatrix[16];
	float projectionMatrix[16];
	byte current_color[4];
	qbool alpha_test = false;

	GL_PushMatrix(GL_MODELVIEW, modelviewMatrix);
	GL_PushMatrix(GL_PROJECTION, projectionMatrix);

	GL_IdentityModelView();
	GL_IdentityProjectionView();

	GLC_StateBeginImageDraw();
	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_EnsureTextureUnitBound(GL_TEXTURE0, ref);

	if (imageData.images[start].flags & IMAGEPROG_FLAGS_TEXT) {
		extern cvar_t gl_alphafont, scr_coloredText;
		
		alpha_test = !gl_alphafont.integer;

		GL_TextureEnvMode(scr_coloredText.integer ? GL_MODULATE : GL_REPLACE);
	}
	else {
		GL_TextureEnvMode(GL_MODULATE);
	}

	GL_AlphaBlendFlags(alpha_test ? GL_ALPHATEST_ENABLED : GL_ALPHATEST_DISABLED);

	if (GL_BuffersSupported()) {
		GL_BindVertexArray(NULL);
		GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);

		glVertexPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, pos));
		glEnableClientState(GL_VERTEX_ARRAY);

		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, colour));
		glEnableClientState(GL_COLOR_ARRAY);

		qglClientActiveTexture(GL_TEXTURE0);
		glTexCoordPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, tex));
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		GL_DrawArrays(GL_QUADS, start * 4, (end - start + 1) * 4);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else {
		int i;

		GL_Color4ubv(imageData.images[start].colour);
		glBegin(GL_QUADS);

		for (i = start; i <= end; ++i) {
			glm_image_t* next = &imageData.images[i];

			// Don't need to break for colour
			if (memcmp(next->colour, current_color, sizeof(current_color))) {
				memcpy(current_color, next->colour, sizeof(current_color));
				GL_Color4ubv(next->colour);
			}

			glTexCoord2f(next->s1, next->t2);
			glVertex2f(next->x1, next->y2);
			glTexCoord2f(next->s1, next->t1);
			glVertex2f(next->x1, next->y1);
			glTexCoord2f(next->s2, next->t1);
			glVertex2f(next->x2, next->y1);
			glTexCoord2f(next->s2, next->t2);
			glVertex2f(next->x2, next->y2);
		}

		glEnd();
	}

	GL_PopMatrix(GL_PROJECTION, projectionMatrix);
	GL_PopMatrix(GL_MODELVIEW, modelviewMatrix);
}

void GLM_PrepareImages(void)
{
	if (GL_ShadersSupported()) {
		GLM_CreateMultiImageProgram();

		if (imageData.imageCount) {
			GL_UpdateBuffer(imageVBO, sizeof(imageData.images[0]) * imageData.imageCount, imageData.images);
		}
	}
	else if (GL_BuffersSupported()) {
		if (!GL_BufferReferenceIsValid(imageVBO)) {
			imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(imageData.glc_images), imageData.glc_images, GL_STREAM_DRAW);
			GL_BindBuffer(imageVBO);
		}
		else {
			GL_BindAndUpdateBuffer(imageVBO, sizeof(imageData.glc_images[0]) * imageData.imageCount * 4, imageData.glc_images);
		}
	}
	else {

	}
}

void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText)
{
	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}
	if (!GLM_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, texnum)) {
		return;
	}

	if (GL_ShadersSupported() || !GL_BuffersSupported()) {
		memcpy(&imageData.images[imageData.imageCount].colour, color, sizeof(byte) * 4);
		GLM_SetCoordinates(&imageData.images[imageData.imageCount], x, y, x + width, y + height);
		imageData.images[imageData.imageCount].s1 = tex_s;
		imageData.images[imageData.imageCount].s2 = tex_s + tex_width;
		imageData.images[imageData.imageCount].t1 = tex_t;
		imageData.images[imageData.imageCount].t2 = tex_t + tex_height;
	}
	else {
		int imageIndex = imageData.imageCount * 4;
		memcpy(&imageData.glc_images[imageIndex].colour, color, sizeof(byte) * 4);
		GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height);
	}

	imageData.images[imageData.imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;
	imageData.images[imageData.imageCount].flags |= (alpha_test ? IMAGEPROG_FLAGS_ALPHATEST : 0);
	imageData.images[imageData.imageCount].flags |= (isText ? IMAGEPROG_FLAGS_TEXT : 0);

	++imageData.imageCount;
}

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	texture_ref solid_texture;
	float s, t;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	Atlas_SolidTextureCoordinates(&solid_texture, &s, &t);
	if (!GLM_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, solid_texture)) {
		return;
	}

	if (GL_ShadersSupported() || !GL_BuffersSupported()) {
		memcpy(&imageData.images[imageData.imageCount].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			imageData.images[imageData.imageCount].colour[0] *= alpha;
			imageData.images[imageData.imageCount].colour[1] *= alpha;
			imageData.images[imageData.imageCount].colour[2] *= alpha;
		}
		GLM_SetCoordinates(&imageData.images[imageData.imageCount], x, y, x + width, y + height);
		imageData.images[imageData.imageCount].s2 = imageData.images[imageData.imageCount].s1 = s;
		imageData.images[imageData.imageCount].t2 = imageData.images[imageData.imageCount].t1 = t;
	}
	else {
		int imageIndex = imageData.imageCount * 4;

		memcpy(&imageData.glc_images[imageIndex].colour, color, sizeof(byte) * 4);
		if (color[3] != 255) {
			float alpha = color[3] / 255.0f;

			imageData.glc_images[imageIndex].colour[0] *= alpha;
			imageData.glc_images[imageIndex].colour[1] *= alpha;
			imageData.glc_images[imageIndex].colour[2] *= alpha;
		}
		GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, s, 0, t, 0);
	}

	imageData.images[imageData.imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;

	++imageData.imageCount;
}
