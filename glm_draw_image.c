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
#include "tr_types.h"
#include "glsl/constants.glsl"

#ifndef HUD_IMAGE_GEOMETRY_SHADER
static GLuint imageIndexData[MAX_MULTI_IMAGE_BATCH * 5];
static buffer_ref imageIndexBuffer;
#endif

static texture_ref glc_last_texture_used;

extern cvar_t r_smoothtext, r_smoothcrosshair, r_smoothimages;

// Flags to detect when the user has changed settings
#define GLM_HUDIMAGES_SMOOTHTEXT        1
#define GLM_HUDIMAGES_SMOOTHCROSSHAIR   2
#define GLM_HUDIMAGES_SMOOTHIMAGES      4
#define GLM_HUDIMAGES_SMOOTHEVERYTHING  (GLM_HUDIMAGES_SMOOTHTEXT | GLM_HUDIMAGES_SMOOTHCROSSHAIR | GLM_HUDIMAGES_SMOOTHIMAGES)

extern float overall_alpha;

void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);

static glm_program_t multiImageProgram;

static glm_vao_t imageVAO;
static buffer_ref imageVBO;
static float cachedMatrix[16];

glm_image_framedata_t imageData;

static void GLC_SetCoordinates(glc_image_t* targ, float x1, float y1, float x2, float y2, float s, float tex_width, float t, float tex_height)
{
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };
	byte color[4];

	GLM_MultiplyVector(cachedMatrix, v1, v1);
	GLM_MultiplyVector(cachedMatrix, v2, v2);

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

#ifdef HUD_IMAGE_GEOMETRY_SHADER
static void GLM_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2)
#else
static void GLM_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2, float s, float s_width, float t, float t_height, int flags)
#endif
{
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };

	GLM_MultiplyVector(cachedMatrix, v1, v1);
	GLM_MultiplyVector(cachedMatrix, v2, v2);

#ifdef HUD_IMAGE_GEOMETRY_SHADER
	targ->x1 = v1[0];
	targ->y1 = v1[1];
	targ->x2 = v2[0];
	targ->y2 = v2[1];
#else
	// TL TL BR BR
	targ[0].pos[0] = v1[0]; targ[0].tex[0] = s;
	targ[1].pos[0] = v1[0]; targ[1].tex[0] = s;
	targ[2].pos[0] = v2[0]; targ[2].tex[0] = s + s_width;
	targ[3].pos[0] = v2[0]; targ[3].tex[0] = s + s_width;
	// TL BR TL BR
	targ[0].pos[1] = v1[1]; targ[0].tex[1] = t;
	targ[1].pos[1] = v2[1]; targ[1].tex[1] = t + t_height;
	targ[2].pos[1] = v1[1]; targ[2].tex[1] = t;
	targ[3].pos[1] = v2[1]; targ[3].tex[1] = t + t_height;

	targ[0].flags = targ[1].flags = targ[2].flags = targ[3].flags = flags;
#endif
}

void GLM_CreateMultiImageProgram(void)
{
	int program_flags =
		(r_smoothtext.integer ? GLM_HUDIMAGES_SMOOTHTEXT : 0) |
		(r_smoothcrosshair.integer ? GLM_HUDIMAGES_SMOOTHCROSSHAIR : 0) |
		(r_smoothimages.integer ? GLM_HUDIMAGES_SMOOTHIMAGES : 0);

#ifdef HUD_IMAGE_GEOMETRY_SHADER
	if (GLM_ProgramRecompileNeeded(&multiImageProgram, program_flags)) {
		GL_VGFDeclare(hud_draw_image);

		// Initialise program for drawing image
		GLM_CreateVGFProgramWithInclude("Multi-image", GL_VGFParams(hud_draw_image), &multiImageProgram, "#define HUD_IMAGE_GEOMETRY_SHADER\n");
	}
#else
	if (GLM_ProgramRecompileNeeded(&multiImageProgram, program_flags)) {
		char included_definitions[512];

		GL_VFDeclare(hud_draw_image);
		included_definitions[0] = '\0';

		if (program_flags != 0 && program_flags != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			// depends on flags on individual elements
			strlcpy(included_definitions, "#define MIXED_SAMPLING", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		GLM_CreateVFProgramWithInclude("Multi-image", GL_VFParams(hud_draw_image), &multiImageProgram, included_definitions);

		multiImageProgram.custom_options = program_flags;
	}
#endif

	if (multiImageProgram.program && !multiImageProgram.uniforms_found) {
		multiImageProgram.uniforms_found = true;
	}

	if (!GL_BufferReferenceIsValid(imageVBO)) {
		imageVBO = GL_CreateFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(imageData.images), imageData.images, buffertype_use_once);
	}

	if (!imageVAO.vao) {
		GL_GenVertexArray(&imageVAO, "image-vao");

#ifdef HUD_IMAGE_GEOMETRY_SHADER
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, x1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, x2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 2, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, s1), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 3, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, s2), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, colour), 0);
		GL_ConfigureVertexAttribIPointer(&imageVAO, imageVBO, 5, 1, GL_INT, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, flags), 0);
#else
		if (!GL_BufferReferenceIsValid(imageIndexBuffer)) {
			GLuint i;

			for (i = 0; i < MAX_MULTI_IMAGE_BATCH; ++i) {
				imageIndexData[i * 5 + 0] = i * 4;
				imageIndexData[i * 5 + 1] = i * 4 + 1;
				imageIndexData[i * 5 + 2] = i * 4 + 2;
				imageIndexData[i * 5 + 3] = i * 4 + 3;
				imageIndexData[i * 5 + 4] = ~(GLuint)0;
			}

			imageIndexBuffer = GL_CreateFixedBuffer(GL_ELEMENT_ARRAY_BUFFER, "image-indexes", sizeof(imageIndexData), imageIndexData, buffertype_constant);
		}

		GL_BindBuffer(imageIndexBuffer);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, pos), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, tex), 0);
		GL_ConfigureVertexAttribPointer(&imageVAO, imageVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, colour), 0);
		GL_ConfigureVertexAttribIPointer(&imageVAO, imageVBO, 3, 1, GL_INT, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, flags), 0);
		GL_BindVertexArray(NULL);
#endif
	}
}

void GLM_DrawImageArraySequence(texture_ref texture, int start, int end)
{
	uintptr_t index_offset = (start * 5 * sizeof(GLuint));
	uintptr_t buffer_offset = GL_BufferOffset(imageVBO) / sizeof(imageData.images[0]);

	GL_UseProgram(multiImageProgram.program);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_Disable(GL_DEPTH_TEST);
	GL_BindVertexArray(&imageVAO);
	if (GL_TextureReferenceIsValid(texture)) {
		GL_EnsureTextureUnitBound(GL_TEXTURE0, texture);
		if ((multiImageProgram.custom_options & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			// Mixed, bind a second texture unit for GL_NEAREST
			GL_EnsureTextureUnitBound(GL_TEXTURE1, texture);
		}
	}
#ifdef HUD_IMAGE_GEOMETRY_SHADER
	GL_DrawArrays(GL_POINTS, offset + start, end - start + 1);
#else
	GL_DrawElementsBaseVertex(GL_TRIANGLE_STRIP, (end - start + 1) * 5, GL_UNSIGNED_INT, (GLvoid*)index_offset, buffer_offset);
#endif
}

void GLC_DrawImageArraySequence(texture_ref ref, int start, int end)
{
	float modelviewMatrix[16];
	float projectionMatrix[16];
	byte current_color[4];
	qbool alpha_test = false;
	qbool nearest = imageData.images[start].flags & IMAGEPROG_FLAGS_NEAREST;
	int i;

	GL_PushMatrix(GL_MODELVIEW, modelviewMatrix);
	GL_PushMatrix(GL_PROJECTION, projectionMatrix);

	GL_IdentityModelView();
	GL_IdentityProjectionView();

	if (GL_TextureReferenceIsValid(glc_last_texture_used) && !GL_TextureReferenceEqual(glc_last_texture_used, ref)) {
		GL_SetTextureFiltering(GL_TEXTURE0, glc_last_texture_used, GL_LINEAR, GL_LINEAR);
	}
	glc_last_texture_used = ref;

	GLC_StateBeginImageDraw();
	GLC_InitTextureUnits1(ref, GL_MODULATE);
	GL_SetTextureFiltering(GL_TEXTURE0, ref, nearest ? GL_NEAREST : GL_LINEAR, nearest ? GL_NEAREST : GL_LINEAR);

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
		extern cvar_t gl_vbo_clientmemory;

		GLC_ClientActiveTexture(GL_TEXTURE0);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		if (gl_vbo_clientmemory.integer) {
			GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
			GL_UnBindBuffer(GL_ARRAY_BUFFER);

			glVertexPointer(2, GL_FLOAT, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].pos);
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].colour);
			glTexCoordPointer(2, GL_FLOAT, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].tex);
		}
		else {
			GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
			GL_BindBuffer(imageVBO);

			glVertexPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, pos));
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, colour));
			glTexCoordPointer(2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, tex));
		}

		for (i = start; i < end; ++i) {
			if (GL_TextureReferenceIsValid(ref) && nearest != (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST)) {
				GL_DrawArrays(GL_QUADS, start * 4, (i - start + 1) * 4);

				nearest = (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST);
				start = i;

				GL_SetTextureFiltering(GL_TEXTURE0, ref, nearest ? GL_NEAREST : GL_LINEAR, nearest ? GL_NEAREST : GL_LINEAR);
			}
		}

		GL_DrawArrays(GL_QUADS, start * 4, (i - start + 1) * 4);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		if (!gl_vbo_clientmemory.integer) {
			GL_UnBindBuffer(GL_ARRAY_BUFFER);
		}
	}
	else {
		int i;

		glColor4ubv(imageData.glc_images[start * 4].colour);
		memcpy(current_color, imageData.glc_images[start * 4].colour, sizeof(current_color));
		glBegin(GL_QUADS);

		for (i = start * 4; i <= 4 * end + 3; ++i) {
			glc_image_t* next = &imageData.glc_images[i];

			if (memcmp(next->colour, current_color, sizeof(current_color))) {
				memcpy(current_color, next->colour, sizeof(current_color));
				glColor4ubv(next->colour);
			}

			if (GL_TextureReferenceIsValid(ref) && nearest != (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST)) {
				GL_End();

				nearest = (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST);
				start = i;

				GL_SetTextureFiltering(GL_TEXTURE0, ref, nearest ? GL_NEAREST : GL_LINEAR, nearest ? GL_NEAREST : GL_LINEAR);

				glBegin(GL_QUADS);
			}

			glTexCoord2f(next->tex[0], next->tex[1]);
			glVertex2f(next->pos[0], next->pos[1]);
		}

		glEnd();
	}

	GL_PopMatrix(GL_PROJECTION, projectionMatrix);
	GL_PopMatrix(GL_MODELVIEW, modelviewMatrix);
}

void GLM_PrepareImages(void)
{
	GL_TextureReferenceInvalidate(glc_last_texture_used);

	if (GL_UseGLSL()) {
		GLM_CreateMultiImageProgram();

		if (imageData.imageCount) {
#ifdef HUD_IMAGE_GEOMETRY_SHADER
			GL_UpdateBuffer(imageVBO, sizeof(imageData.images[0]) * imageData.imageCount, imageData.images);
#else
			GL_UpdateBuffer(imageVBO, sizeof(imageData.images[0]) * imageData.imageCount * 4, imageData.images);
#endif
			if ((multiImageProgram.custom_options & GLM_HUDIMAGES_SMOOTHEVERYTHING) == 0) {
				// Everything is GL_NEAREST
				GLM_SamplerSetNearest(0);
			}
			else if ((multiImageProgram.custom_options & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
				// Mixed, second texture unit for GL_NEAREST
				GLM_SamplerSetNearest(1);
			}
		}
	}
	else if (GL_BuffersSupported()) {
		if (!GL_BufferReferenceIsValid(imageVBO)) {
			imageVBO = GL_GenFixedBuffer(GL_ARRAY_BUFFER, "image-vbo", sizeof(imageData.glc_images), imageData.glc_images, GL_STREAM_DRAW);
		}
		else {
			GL_UpdateBuffer(imageVBO, sizeof(imageData.glc_images[0]) * imageData.imageCount * 4, imageData.glc_images);
		}
	}
	else {

	}
}

void GLM_ImageDrawComplete(void)
{
	if (GL_UseGLSL()) {
		if ((multiImageProgram.custom_options & GLM_HUDIMAGES_SMOOTHEVERYTHING) == 0) {
			GLM_SamplerClear(0);
		}
		else if ((multiImageProgram.custom_options & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			GLM_SamplerClear(1);
		}
	}
	else if (GL_UseImmediateMode()) {
		if (GL_TextureReferenceIsValid(glc_last_texture_used)) {
			GL_SetTextureFiltering(GL_TEXTURE0, glc_last_texture_used, GL_LINEAR, GL_LINEAR);
		}
	}
}

void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText, qbool isCrosshair)
{
	int flags = IMAGEPROG_FLAGS_TEXTURE;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}
	if (!GLM_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, texnum)) {
		return;
	}

	flags |= (alpha_test ? IMAGEPROG_FLAGS_ALPHATEST : 0);
	if (isCrosshair) {
		if (!r_smoothcrosshair.integer) {
			flags |= IMAGEPROG_FLAGS_NEAREST;
		}
	}
	else if (isText) {
		flags |= IMAGEPROG_FLAGS_TEXT;
		if (!r_smoothtext.integer) {
			flags |= IMAGEPROG_FLAGS_NEAREST;
		}
	}
	else {
		if (!r_smoothimages.integer) {
			flags |= IMAGEPROG_FLAGS_NEAREST;
		}
	}

	if (GL_UseGLSL()) {
		float alpha = (color[3] * overall_alpha / 255.0f);
#ifdef HUD_IMAGE_GEOMETRY_SHADER
		imageData.images[imageData.imageCount].colour[0] = color[0] * alpha;
		imageData.images[imageData.imageCount].colour[1] = color[1] * alpha;
		imageData.images[imageData.imageCount].colour[2] = color[2] * alpha;
		imageData.images[imageData.imageCount].colour[3] = color[3] * overall_alpha;
		GLM_SetCoordinates(&imageData.images[imageData.imageCount], x, y, x + width, y + height);

		imageData.images[imageData.imageCount].s1 = tex_s;
		imageData.images[imageData.imageCount].s2 = tex_s + tex_width;
		imageData.images[imageData.imageCount].t1 = tex_t;
		imageData.images[imageData.imageCount].t2 = tex_t + tex_height;

		imageData.images[imageData.imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;
		imageData.images[imageData.imageCount].flags |= (alpha_test ? IMAGEPROG_FLAGS_ALPHATEST : 0);
		imageData.images[imageData.imageCount].flags |= (isText ? IMAGEPROG_FLAGS_TEXT : 0);
#else
		glm_image_t* img = &imageData.images[imageData.imageCount * 4];

		img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
		img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
		img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
		img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

		GLM_SetCoordinates(img, x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height, flags);
#endif
	}
	else {
		int imageIndex = imageData.imageCount * 4;
		glc_image_t* img = &imageData.glc_images[imageData.imageCount * 4];
		float alpha = (color[3] * overall_alpha / 255.0f);

		img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
		img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
		img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
		img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

		GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height);

		imageData.images[imageData.imageCount].flags = flags;
	}

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

	if (GL_UseGLSL()) {
#ifdef HUD_IMAGE_GEOMETRY_SHADER
		float alpha = (color[3] * overall_alpha / 255.0f);

		imageData.images[imageData.imageCount].colour[0] = color[0] * alpha;
		imageData.images[imageData.imageCount].colour[1] = color[1] * alpha;
		imageData.images[imageData.imageCount].colour[2] = color[2] * alpha;
		imageData.images[imageData.imageCount].colour[3] = color[3] * overall_alpha;
		GLM_SetCoordinates(&imageData.images[imageData.imageCount], x, y, x + width, y + height);
		imageData.images[imageData.imageCount].s2 = imageData.images[imageData.imageCount].s1 = s;
		imageData.images[imageData.imageCount].t2 = imageData.images[imageData.imageCount].t1 = t;
		imageData.images[imageData.imageCount].flags = IMAGEPROG_FLAGS_TEXTURE;
#else 
		glm_image_t* img = &imageData.images[imageData.imageCount * 4];
		float alpha = (color[3] * overall_alpha / 255.0f);

		img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
		img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
		img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
		img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

		GLM_SetCoordinates(img, x, y, x + width, y + height, s, 0, t, 0, IMAGEPROG_FLAGS_TEXTURE);
#endif
	}
	else {
		int imageIndex = imageData.imageCount * 4;
		float alpha = (color[3] * overall_alpha / 255.0f);

		imageData.glc_images[imageIndex].colour[0] = color[0] * alpha;
		imageData.glc_images[imageIndex].colour[1] = color[1] * alpha;
		imageData.glc_images[imageIndex].colour[2] = color[2] * alpha;
		imageData.glc_images[imageIndex].colour[3] = color[3] * overall_alpha;
		GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, s, 0, t, 0);
	}

	++imageData.imageCount;
}

void GLM_Cache2DMatrix(void)
{
	float modelViewMatrix[16];
	float projectionMatrix[16];

	GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
	GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

	GLM_MultiplyMatrix(projectionMatrix, modelViewMatrix, cachedMatrix);
}
