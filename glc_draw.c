/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2017-2018 ezQuake team

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
#include "glm_draw.h"
#include "r_draw.h"
#include "r_state.h"
#include "r_matrix.h"
#include "r_texture.h"
#include "glc_vao.h"
#include "glsl/constants.glsl" // FIXME: remove
#include "glc_local.h"
#include "r_renderer.h"

static texture_ref glc_last_texture_used;
static buffer_ref imageVBO;
extern float overall_alpha;
extern float cachedMatrix[16];
void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);

void GLC_DrawDisc(void)
{
	glDrawBuffer(GL_FRONT);
	Draw_Pic(vid.width - 24, 0, draw_disc);
	R_EmptyImageQueue();
	glDrawBuffer(GL_BACK);
}

void GLC_HudDrawComplete(void)
{
	if (R_TextureReferenceIsValid(glc_last_texture_used)) {
		renderer.TextureSetFiltering(glc_last_texture_used, texture_minification_linear, texture_magnification_linear);
	}
}

void GLC_HudDrawCircles(texture_ref texture, int start, int end)
{
	// FIXME: Not very efficient (but rarely used either)
	int i, j;

	start = max(0, start);
	end = min(end, circleData.circleCount - 1);

	for (i = start; i <= end; ++i) {
		GLC_StateBeginDrawAlphaPieSliceRGB(circleData.drawCircleThickness[i]);
		R_CustomColor(
			circleData.drawCircleColors[i][0],
			circleData.drawCircleColors[i][1],
			circleData.drawCircleColors[i][2],
			circleData.drawCircleColors[i][3]
		);

		GLC_Begin(circleData.drawCircleFill[i] ? GL_TRIANGLE_STRIP : GL_LINE_LOOP);
		for (j = 0; j < circleData.drawCirclePoints[i]; ++j) {
			GLC_Vertex2fv(&circleData.drawCirclePointData[i * FLOATS_PER_CIRCLE + j * 2]);
		}
		GLC_End();
	}
}

void GLC_HudPrepareCircles(void)
{
}

void GLC_HudPrepareLines(void)
{
}

void GLC_HudPreparePolygons(void)
{
}

void GLC_HudDrawLines(texture_ref texture, int start, int end)
{
	int i;

	for (i = start; i <= end; ++i) {
		R_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
		GLC_Begin(GL_LINES);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 0].color);
		GLC_Vertex3fv(lineData.line_points[i * 2 + 0].position);
		R_CustomColor4ubv(lineData.line_points[i * 2 + 1].color);
		GLC_Vertex3fv(lineData.line_points[i * 2 + 1].position);
		GLC_End();
	}
}

void GLC_HudDrawPolygons(texture_ref texture, int start, int end)
{
	int i, j;

	GLC_StateBeginDrawPolygon();

	for (i = start; i <= end; ++i) {
		R_CustomColor(
			polygonData.polygonColor[i][0],
			polygonData.polygonColor[i][1],
			polygonData.polygonColor[i][2],
			polygonData.polygonColor[i][3]
		);
		GLC_Begin(GL_TRIANGLE_STRIP);
		for (j = 0; j < polygonData.polygonVerts[i]; j++) {
			GLC_Vertex3fv(polygonData.polygonVertices[j + i * MAX_POLYGON_POINTS]);
		}
		GLC_End();
	}
}

static void GLC_SetCoordinates(glc_image_t* targ, float x1, float y1, float x2, float y2, float s, float tex_width, float t, float tex_height)
{
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };
	byte color[4];

	R_MultiplyVector(cachedMatrix, v1, v1);
	R_MultiplyVector(cachedMatrix, v2, v2);

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

void GLC_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	texture_ref solid_texture;
	float s, t;
	float alpha;
	int imageIndex;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	Atlas_SolidTextureCoordinates(&solid_texture, &s, &t);
	if (!R_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, solid_texture)) {
		return;
	}

	imageIndex = imageData.imageCount * 4;
	alpha = (color[3] * overall_alpha / 255.0f);

	imageData.glc_images[imageIndex].colour[0] = color[0] * alpha;
	imageData.glc_images[imageIndex].colour[1] = color[1] * alpha;
	imageData.glc_images[imageIndex].colour[2] = color[2] * alpha;
	imageData.glc_images[imageIndex].colour[3] = color[3] * overall_alpha;
	GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, s, 0, t, 0);

	++imageData.imageCount;
}

void GLC_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags)
{
	int imageIndex = imageData.imageCount * 4;
	glc_image_t* img = &imageData.glc_images[imageData.imageCount * 4];
	float alpha = (color[3] * overall_alpha / 255.0f);

	img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
	img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
	img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
	img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

	GLC_SetCoordinates(&imageData.glc_images[imageIndex], x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height);

	imageData.images[imageData.imageCount].flags = flags;
	++imageData.imageCount;
}

void GLC_AdjustImages(int first, int last, float x_offset)
{
	int i;

	for (i = first; i < last; ++i) {
		imageData.glc_images[i * 4 + 0].pos[0] += x_offset;
		imageData.glc_images[i * 4 + 1].pos[0] += x_offset;
		imageData.glc_images[i * 4 + 2].pos[0] += x_offset;
		imageData.glc_images[i * 4 + 3].pos[0] += x_offset;
	}
}

static void GLC_CreateImageVAO(void)
{
	extern cvar_t gl_vbo_clientmemory;

	if (gl_vbo_clientmemory.integer) {
		R_GenVertexArray(vao_hud_images);
		GLC_VAOEnableVertexPointer(vao_hud_images, 2, GL_FLOAT, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].pos);
		GLC_VAOEnableTextureCoordPointer(vao_hud_images, 0, 2, GL_FLOAT, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].tex);
		GLC_VAOEnableColorPointer(vao_hud_images, 4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), (GLvoid*)&imageData.glc_images[0].colour);
		R_BindVertexArray(vao_none);
	}
	else {
		R_GenVertexArray(vao_hud_images);
		GLC_VAOSetVertexBuffer(vao_hud_images, imageVBO);
		GLC_VAOEnableVertexPointer(vao_hud_images, 2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, pos));
		GLC_VAOEnableTextureCoordPointer(vao_hud_images, 0, 2, GL_FLOAT, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, tex));
		GLC_VAOEnableColorPointer(vao_hud_images, 4, GL_UNSIGNED_BYTE, sizeof(glc_image_t), VBO_FIELDOFFSET(glc_image_t, colour));
		R_BindVertexArray(vao_none);
	}
}

void GLC_HudPrepareImages(void)
{
	R_TextureReferenceInvalidate(glc_last_texture_used);

	if (buffers.supported) {
		if (!R_BufferReferenceIsValid(imageVBO)) {
			imageVBO = buffers.Create(buffertype_vertex, "image-vbo", sizeof(imageData.glc_images), imageData.glc_images, bufferusage_once_per_frame);
		}
		else {
			buffers.Update(imageVBO, sizeof(imageData.glc_images[0]) * imageData.imageCount * 4, imageData.glc_images);
		}
	}
}

void GLC_HudDrawImages(texture_ref ref, int start, int end)
{
	float modelviewMatrix[16];
	float projectionMatrix[16];
	byte current_color[4];
	qbool nearest = imageData.images[start].flags & IMAGEPROG_FLAGS_NEAREST;
	int i;

	if (buffers.supported) {
		if (!R_VertexArrayCreated(vao_hud_images)) {
			GLC_CreateImageVAO();
		}
	}

	R_PushModelviewMatrix(modelviewMatrix);
	R_PushProjectionMatrix(projectionMatrix);

	R_IdentityModelView();
	R_IdentityProjectionView();

	if (R_TextureReferenceIsValid(glc_last_texture_used) && !R_TextureReferenceEqual(glc_last_texture_used, ref)) {
		renderer.TextureSetFiltering(glc_last_texture_used, texture_minification_linear, texture_magnification_linear);
	}
	glc_last_texture_used = ref;

	GLC_StateBeginImageDraw(imageData.images[start].flags & IMAGEPROG_FLAGS_TEXT);
	renderer.TextureUnitBind(0, ref);
	renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);

	if (R_VAOBound()) {
		extern cvar_t gl_vbo_clientmemory;
		uintptr_t offset = gl_vbo_clientmemory.integer ? 0 : buffers.BufferOffset(imageVBO) / sizeof(imageData.glc_images[0]);

		for (i = start; i < end; ++i) {
			if (R_TextureReferenceIsValid(ref) && nearest != (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST)) {
				GL_DrawArrays(GL_QUADS, offset + start * 4, (i - start + 1) * 4);

				nearest = (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST);
				start = i;

				renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
			}
		}

		GL_DrawArrays(GL_QUADS, offset + start * 4, (i - start + 1) * 4);
	}
	else {
		int i;

		R_CustomColor4ubv(imageData.glc_images[start * 4].colour);
		memcpy(current_color, imageData.glc_images[start * 4].colour, sizeof(current_color));
		GLC_Begin(GL_QUADS);

		for (i = start * 4; i <= 4 * end + 3; ++i) {
			glc_image_t* next = &imageData.glc_images[i];

			R_CustomColor4ubv(next->colour);

			if (R_TextureReferenceIsValid(ref) && nearest != (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST)) {
				GLC_End();

				nearest = (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST);
				start = i;

				renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);

				GLC_Begin(GL_QUADS);
			}

			glTexCoord2f(next->tex[0], next->tex[1]);
			GLC_Vertex2f(next->pos[0], next->pos[1]);
		}

		GLC_End();
	}

	R_PopProjectionMatrix(projectionMatrix);
	R_PopModelviewMatrix(modelviewMatrix);
}
