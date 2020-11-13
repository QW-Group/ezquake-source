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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

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
#include "tr_types.h"
#include "gl_texture.h"

static texture_ref glc_last_texture_used;
static int extraIndexesPerImage;
extern float overall_alpha;
extern float cachedMatrix[16];

qbool GLC_ProgramHudImagesCompile(void);

void GLC_DrawDisc(void)
{
	if (!GL_FramebufferEnabled2D()) {
		GL_BuiltinProcedure(glDrawBuffer, "mode=GL_FRONT", GL_FRONT);
		Draw_Pic(vid.width - 24, 0, draw_disc);
		R_EmptyImageQueue();
		GL_BuiltinProcedure(glDrawBuffer, "mode=GL_BACK", GL_BACK);
	}
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

	R_ProgramUse(r_program_none);
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

static void GLC_HudDrawLinesVertexArray(r_program_id program_id, texture_ref texture, int start, int end)
{
	extern cvar_t gl_vbo_clientmemory;
	uintptr_t offset = gl_vbo_clientmemory.integer ? 0 : buffers.BufferOffset(r_buffer_hud_image_vertex_data) / sizeof(imageData.images[0]);
	int i;

	R_ProgramUse(program_id);
	R_ApplyRenderingState(program_id == r_program_none ? r_state_hud_images_glc_non_glsl : r_state_hud_images_glc);
	renderer.TextureUnitBind(0, texture);
	for (i = start; i <= end; ++i) {
		R_StateBeginAlphaLineRGB(lineData.line_thickness[i]);

		GL_DrawArrays(GL_LINES, offset + lineData.imageIndex[i], 2);
	}
}

static void GLC_HudDrawLinesImmediate(texture_ref texture, int start, int end)
{
	int i;

	R_ProgramUse(r_program_none);
	R_ApplyRenderingState(r_state_hud_images_glc_non_glsl);
	renderer.TextureUnitBind(0, texture);

	for (i = start; i <= end; ++i) {
		glm_image_t* img = &imageData.images[lineData.imageIndex[i]];

		R_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
		glTexCoord2f(img[0].tex[0], img[0].tex[1]);

		GLC_Begin(GL_LINES);
		R_CustomColor4ubv(img[0].colour);
		GLC_Vertex2fv(img[0].pos);
		R_CustomColor4ubv(img[1].colour);
		GLC_Vertex2fv(img[1].pos);
		GLC_End();
	}
}

void GLC_HudDrawLines(texture_ref texture, int start, int end)
{
	extern cvar_t gl_program_hud;

	if (buffers.supported && gl_program_hud.integer && GLC_ProgramHudImagesCompile()) {
		GLC_HudDrawLinesVertexArray(r_program_hud_images_glc, texture, start, end);
	}
	else if (R_VAOBound()) {
		GLC_HudDrawLinesVertexArray(r_program_none, texture, start, end);
	}
	else {
		GLC_HudDrawLinesImmediate(texture, start, end);
	}
}

static void GLC_HudDrawPolygonsImmediate(texture_ref texture, int start, int end)
{
	int i, j;

	R_ProgramUse(r_program_none);
	R_ApplyRenderingState(r_state_hud_images_glc_non_glsl);
	renderer.TextureUnitBind(0, texture);

	for (i = start; i <= end; ++i) {
		glm_image_t* img = &imageData.images[polygonData.polygonImageIndexes[i]];

		R_CustomColor4ubv(img[0].colour);
		glTexCoord2f(img[0].tex[0], img[0].tex[1]);

		GLC_Begin(GL_TRIANGLE_STRIP);
		for (j = 0; j < polygonData.polygonVerts[i]; j++) {
			GLC_Vertex2fv(img[j].pos);
		}
		GLC_End();
	}
}

static void GLC_HudDrawPolygonsVertexArray(r_program_id program_id, texture_ref texture, int start, int end)
{
	extern cvar_t gl_vbo_clientmemory;
	uintptr_t offset = gl_vbo_clientmemory.integer ? 0 : buffers.BufferOffset(r_buffer_hud_image_vertex_data) / sizeof(imageData.images[0]);
	int i;

	R_ProgramUse(program_id);
	R_ApplyRenderingState(program_id == r_program_none ? r_state_hud_images_glc_non_glsl : r_state_hud_images_glc);
	renderer.TextureUnitBind(0, texture);

	for (i = start; i <= end; ++i) {
		GL_DrawArrays(GL_TRIANGLE_STRIP, offset + polygonData.polygonImageIndexes[i], polygonData.polygonVerts[i]);
	}
}

void GLC_HudDrawPolygons(texture_ref texture, int start, int end)
{
	extern cvar_t gl_program_hud;

	if (buffers.supported && gl_program_hud.integer && GLC_ProgramHudImagesCompile()) {
		GLC_HudDrawPolygonsVertexArray(r_program_hud_images_glc, texture, start, end);
	}
	else if (R_VAOBound()) {
		GLC_HudDrawPolygonsVertexArray(r_program_none, texture, start, end);
	}
	else {
		GLC_HudDrawPolygonsImmediate(texture, start, end);
	}
}

static void GLC_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2, float s, float s_width, float t, float t_height, qbool nearest)
{
	float v1[4] = { x1, y1, 0, 1 };
	float v2[4] = { x2, y2, 0, 1 };

	R_MultiplyVector(cachedMatrix, v1, v1);
	R_MultiplyVector(cachedMatrix, v2, v2);

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

	targ[0].tex[3] = targ[1].tex[3] = targ[2].tex[3] = targ[3].tex[3] = nearest ? 0.0f : 1.0f;
}

void GLC_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	texture_ref solid_texture;
	float s, t;
	float alpha;
	glm_image_t* img = &imageData.images[imageData.imageCount * 4];

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	Atlas_SolidTextureCoordinates(&solid_texture, &s, &t);
	if (!R_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, solid_texture)) {
		return;
	}

	alpha = (color[3] * overall_alpha / 255.0f);

	img[0].colour[0] = img[1].colour[0] = img[2].colour[0] = img[3].colour[0] = color[0] * alpha;
	img[0].colour[1] = img[1].colour[1] = img[2].colour[1] = img[3].colour[1] = color[1] * alpha;
	img[0].colour[2] = img[1].colour[2] = img[2].colour[2] = img[3].colour[2] = color[2] * alpha;
	img[0].colour[3] = img[1].colour[3] = img[2].colour[3] = img[3].colour[3] = color[3] * overall_alpha;
	GLC_SetCoordinates(img, x, y, x + width, y + height, s, 0, t, 0, false);

	++imageData.imageCount;
}

void GLC_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags)
{
	int imageIndex = imageData.imageCount * 4;
	glm_image_t* img = &imageData.images[imageData.imageCount * 4];
	float alpha = (color[3] * overall_alpha / 255.0f);

	img[0].colour[0] = img[1].colour[0] = img[2].colour[0] = img[3].colour[0] = color[0] * alpha;
	img[0].colour[1] = img[1].colour[1] = img[2].colour[1] = img[3].colour[1] = color[1] * alpha;
	img[0].colour[2] = img[1].colour[2] = img[2].colour[2] = img[3].colour[2] = color[2] * alpha;
	img[0].colour[3] = img[1].colour[3] = img[2].colour[3] = img[3].colour[3] = color[3] * overall_alpha;
	GLC_SetCoordinates(&imageData.images[imageIndex], x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height, flags & IMAGEPROG_FLAGS_NEAREST);

	imageData.images[imageData.imageCount].flags = flags;
	++imageData.imageCount;
}

void GLC_AdjustImages(int first, int last, float x_offset)
{
	int i;

	for (i = first; i < last; ++i) {
		imageData.images[i * 4 + 0].pos[0] += x_offset;
		imageData.images[i * 4 + 1].pos[0] += x_offset;
		imageData.images[i * 4 + 2].pos[0] += x_offset;
		imageData.images[i * 4 + 3].pos[0] += x_offset;
	}
}

static void GLC_CreateImageVAO(void)
{
	extern cvar_t gl_vbo_clientmemory;

	R_TraceEnterFunctionRegion;
	R_TraceAPI("clientmemory: %s", gl_vbo_clientmemory.integer ? "true" : "false");
	if (gl_vbo_clientmemory.integer) {
		R_GenVertexArray(vao_hud_images);
		GLC_VAOSetIndexBuffer(vao_hud_images, r_buffer_hud_image_index_data);
		GLC_VAOEnableVertexPointer(vao_hud_images, 2, GL_FLOAT, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].pos);
		GLC_VAOEnableTextureCoordPointer(vao_hud_images, 0, 4, GL_FLOAT, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].tex);
		GLC_VAOEnableColorPointer(vao_hud_images, 4, GL_UNSIGNED_BYTE, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].colour);
		R_BindVertexArray(vao_none);

		// A copy of the above, but only 2 texture coordinates sent
		R_GenVertexArray(vao_hud_images_non_glsl);
		GLC_VAOSetIndexBuffer(vao_hud_images_non_glsl, r_buffer_hud_image_index_data);
		GLC_VAOEnableVertexPointer(vao_hud_images_non_glsl, 2, GL_FLOAT, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].pos);
		GLC_VAOEnableTextureCoordPointer(vao_hud_images_non_glsl, 0, 2, GL_FLOAT, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].tex);
		GLC_VAOEnableColorPointer(vao_hud_images_non_glsl, 4, GL_UNSIGNED_BYTE, sizeof(glm_image_t), (GLvoid*)&imageData.images[0].colour);
		R_BindVertexArray(vao_none);
	}
	else {
		R_GenVertexArray(vao_hud_images);
		GLC_VAOSetIndexBuffer(vao_hud_images, r_buffer_hud_image_index_data);
		GLC_VAOSetVertexBuffer(vao_hud_images, r_buffer_hud_image_vertex_data);
		GLC_VAOEnableVertexPointer(vao_hud_images, 2, GL_FLOAT, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, pos));
		GLC_VAOEnableTextureCoordPointer(vao_hud_images, 0, 4, GL_FLOAT, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, tex));
		GLC_VAOEnableColorPointer(vao_hud_images, 4, GL_UNSIGNED_BYTE, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, colour));
		R_BindVertexArray(vao_none);

		// A copy of the above, but only 2 texture coordinates sent
		R_GenVertexArray(vao_hud_images_non_glsl);
		GLC_VAOSetIndexBuffer(vao_hud_images_non_glsl, r_buffer_hud_image_index_data);
		GLC_VAOSetVertexBuffer(vao_hud_images_non_glsl, r_buffer_hud_image_vertex_data);
		GLC_VAOEnableVertexPointer(vao_hud_images_non_glsl, 2, GL_FLOAT, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, pos));
		GLC_VAOEnableTextureCoordPointer(vao_hud_images_non_glsl, 0, 2, GL_FLOAT, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, tex));
		GLC_VAOEnableColorPointer(vao_hud_images_non_glsl, 4, GL_UNSIGNED_BYTE, sizeof(glm_image_t), VBO_FIELDOFFSET(glm_image_t, colour));
		R_BindVertexArray(vao_none);
	}
	R_TraceLeaveFunctionRegion;
}

void GLC_HudPrepareImages(void)
{
	extern cvar_t gl_vbo_clientmemory;

	R_TextureReferenceInvalidate(glc_last_texture_used);

	if (buffers.supported && !gl_vbo_clientmemory.integer) {
		if (!R_BufferReferenceIsValid(r_buffer_hud_image_vertex_data)) {
			buffers.Create(r_buffer_hud_image_vertex_data, buffertype_vertex, "image-vbo", sizeof(imageData.images), imageData.images, bufferusage_once_per_frame);
		}
		else {
			buffers.Update(r_buffer_hud_image_vertex_data, sizeof(imageData.images[0]) * imageData.imageCount * 4, imageData.images);
		}
	}

	if (buffers.supported && !R_BufferReferenceIsValid(r_buffer_hud_image_index_data)) {
		R_BindVertexArray(vao_none);
		if (!R_BufferReferenceIsValid(r_buffer_hud_image_index_data)) {
			unsigned int data[MAX_MULTI_IMAGE_BATCH * 6 * 3];
			int pos, i;

			for (i = 0, pos = 0; i < MAX_MULTI_IMAGE_BATCH * 3; ++i) {
				if (i && !GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
					data[pos++] = i * 4;
				}
				data[pos++] = i * 4;
				data[pos++] = i * 4 + 1;
				data[pos++] = i * 4 + 2;
				data[pos++] = i * 4 + 3;
				if (GL_Supported(R_SUPPORT_PRIMITIVERESTART)) {
					data[pos++] = ~(unsigned int)0;
				}
				else {
					data[pos++] = i * 4 + 3;
				}
			}
			buffers.Create(r_buffer_hud_image_index_data, buffertype_index, "hudimage-elements", pos * sizeof(unsigned int), data, bufferusage_reuse_many_frames);
			extraIndexesPerImage = GL_Supported(R_SUPPORT_PRIMITIVERESTART) ? 1 : 2;
		}
	}

	R_TraceAPI("%s pre-check: %s, %s", __FUNCTION__, (buffers.supported ? "buffers-supported" : "no-buffers"), R_VertexArrayCreated(vao_hud_images) ? "vao-created" : "vao-not-created");
	if (buffers.supported && !R_VertexArrayCreated(vao_hud_images)) {
		GLC_CreateImageVAO();
	}
}

static void GLC_HudDrawImagesVertexArray(texture_ref ref, int start, int end)
{
	extern cvar_t gl_vbo_clientmemory;
	uintptr_t offset = gl_vbo_clientmemory.integer ? 0 : buffers.BufferOffset(r_buffer_hud_image_vertex_data) / (sizeof(imageData.images[0]) * 4);
	qbool nearest = (imageData.images[start].flags & IMAGEPROG_FLAGS_NEAREST);
	int i;

	R_ProgramUse(r_program_none);
	renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
	renderer.TextureUnitBind(0, ref);
	for (i = start; i < end; ++i) {
		if (R_TextureReferenceIsValid(ref) && nearest != (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST)) {
			if (i > start) {
				GL_DrawElements(GL_TRIANGLE_STRIP, (i - start) * (4 + extraIndexesPerImage), GL_UNSIGNED_INT, (void*)((offset + start) * (4 + extraIndexesPerImage) * sizeof(GLuint)));
			}

			nearest = (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST);
			start = i;

			renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
		}
	}

	GL_DrawElements(GL_TRIANGLE_STRIP, (end - start + 1) * (4 + extraIndexesPerImage), GL_UNSIGNED_INT, (void*)((offset + start) * (4 + extraIndexesPerImage) * sizeof(GLuint)));
}

#define GLC_PROGRAMFLAGS_SMOOTHCROSSHAIR     1
#define GLC_PROGRAMFLAGS_SMOOTHIMAGES        2
#define GLC_PROGRAMFLAGS_SMOOTHTEXT          4
#define GLC_PROGRAMFLAGS_ALPHAHACK           8
#define GLC_PROGRAMFLAGS_SMOOTHEVERYTHING    (GLC_PROGRAMFLAGS_SMOOTHCROSSHAIR | GLC_PROGRAMFLAGS_SMOOTHIMAGES | GLC_PROGRAMFLAGS_SMOOTHTEXT)

qbool GLC_ProgramHudImagesCompile(void)
{
	extern cvar_t r_smoothtext, r_smoothcrosshair, r_smoothimages, r_smoothalphahack;
	int flags =
		(r_smoothtext.integer ? GLC_PROGRAMFLAGS_SMOOTHTEXT : 0) |
		(r_smoothcrosshair.integer ? GLC_PROGRAMFLAGS_SMOOTHCROSSHAIR : 0) |
		(r_smoothimages.integer ? GLC_PROGRAMFLAGS_SMOOTHIMAGES : 0) |
		(r_smoothalphahack.integer ? GLC_PROGRAMFLAGS_ALPHAHACK : 0);
	
	if (R_ProgramRecompileNeeded(r_program_hud_images_glc, flags)) {
		static char included_definitions[512];
		int smooth_flags = (flags & GLC_PROGRAMFLAGS_SMOOTHEVERYTHING);
		qbool mixed_sampling = GL_Supported(R_SUPPORT_TEXTURE_SAMPLERS) && smooth_flags != 0 && smooth_flags != GLC_PROGRAMFLAGS_SMOOTHEVERYTHING;

		memset(included_definitions, 0, sizeof(included_definitions));
		if (mixed_sampling) {
			strlcat(included_definitions, "#define MIXED_SAMPLING\n", sizeof(included_definitions));
		}
		if (flags & GLC_PROGRAMFLAGS_ALPHAHACK) {
			strlcat(included_definitions, "#define PREMULT_ALPHA_HACK\n", sizeof(included_definitions));
		}

		R_ProgramCompileWithInclude(r_program_hud_images_glc, included_definitions);
		R_ProgramUniform1i(r_program_uniform_hud_images_glc_primarySampler, 0);
		if (mixed_sampling) {
			R_ProgramUniform1i(r_program_uniform_hud_images_glc_secondarySampler, 1);
		}
		R_ProgramSetCustomOptions(r_program_hud_images_glc, flags);
	}

	return R_ProgramReady(r_program_hud_images_glc);
}

static void GLC_HudDrawImagesProgram(r_program_id program_id, texture_ref ref, int start, int end)
{
	extern cvar_t gl_vbo_clientmemory;
	uintptr_t offset = gl_vbo_clientmemory.integer ? 0 : buffers.BufferOffset(r_buffer_hud_image_vertex_data) / (sizeof(imageData.images[0]) * 4);
	int i;

	R_ProgramUse(program_id);
	if (program_id != r_program_none && GL_Supported(R_SUPPORT_TEXTURE_SAMPLERS)) {
		qbool everything_nearest = (R_ProgramCustomOptions(r_program_hud_images_glc) & GLC_PROGRAMFLAGS_SMOOTHEVERYTHING) == 0;
		qbool everything_linear = (R_ProgramCustomOptions(r_program_hud_images_glc) & GLC_PROGRAMFLAGS_SMOOTHEVERYTHING) == GLC_PROGRAMFLAGS_SMOOTHEVERYTHING;

		renderer.TextureUnitBind(0, ref);
		if (everything_nearest) {
			// Everything is GL_NEAREST, no smoothing
			GL_SamplerSetNearest(0);
		}
		else if (!everything_linear) {
			// Mixed, second texture unit for GL_NEAREST
			renderer.TextureUnitBind(1, ref);
			GL_SamplerSetNearest(1);
		}

		GL_DrawElements(GL_TRIANGLE_STRIP, (end - start + 1) * (4 + extraIndexesPerImage), GL_UNSIGNED_INT, (void*)((offset + start) * (4 + extraIndexesPerImage) * sizeof(GLuint)));

		if (everything_nearest) {
			GL_SamplerClear(0);
		}
		else if (!everything_linear) {
			GL_SamplerClear(1);
		}
	}
	else {
		qbool nearest = (imageData.images[start].flags & IMAGEPROG_FLAGS_NEAREST);
		renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
		renderer.TextureUnitBind(0, ref);
		for (i = start; i < end; ++i) {
			if (R_TextureReferenceIsValid(ref) && nearest != (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST)) {
				GL_DrawElements(GL_TRIANGLE_STRIP, (i - start) * (4 + extraIndexesPerImage), GL_UNSIGNED_INT, (void*)((offset + start) * (4 + extraIndexesPerImage) * sizeof(GLuint)));

				nearest = (imageData.images[i].flags & IMAGEPROG_FLAGS_NEAREST);
				start = i;

				renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
			}
		}

		GL_DrawElements(GL_TRIANGLE_STRIP, (end - start + 1) * (4 + extraIndexesPerImage), GL_UNSIGNED_INT, (void*)((offset + start) * (4 + extraIndexesPerImage) * sizeof(GLuint)));
	}
	R_ProgramUse(r_program_none);
}

static void GLC_HudDrawImages_Immediate(texture_ref ref, int start, int end)
{
	int i;
	byte current_color[4];
	qbool nearest = (imageData.images[start].flags & IMAGEPROG_FLAGS_NEAREST);

	renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);
	renderer.TextureUnitBind(0, ref);
	R_CustomColor4ubv(imageData.images[start * 4].colour);
	memcpy(current_color, imageData.images[start * 4].colour, sizeof(current_color));

	R_ProgramUse(r_program_none);
	GLC_Begin(GL_QUADS);
	for (i = start * 4; i <= 4 * end + 3; i += 4) {
		glm_image_t* next = &imageData.images[i];

		R_CustomColor4ubv(next->colour);

		if (R_TextureReferenceIsValid(ref) && nearest != (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST)) {
			GLC_End();

			nearest = (imageData.images[i / 4].flags & IMAGEPROG_FLAGS_NEAREST);
			renderer.TextureSetFiltering(ref, nearest ? texture_minification_nearest : texture_minification_linear, nearest ? texture_magnification_nearest : texture_magnification_linear);

			GLC_Begin(GL_QUADS);
		}

		glTexCoord2f(next[0].tex[0], next[0].tex[1]);
		GLC_Vertex2f(next[0].pos[0], next[0].pos[1]);
		glTexCoord2f(next[2].tex[0], next[2].tex[1]);
		GLC_Vertex2f(next[2].pos[0], next[2].pos[1]);
		glTexCoord2f(next[3].tex[0], next[3].tex[1]);
		GLC_Vertex2f(next[3].pos[0], next[3].pos[1]);
		glTexCoord2f(next[1].tex[0], next[1].tex[1]);
		GLC_Vertex2f(next[1].pos[0], next[1].pos[1]);
	}
	GLC_End();
}

void GLC_HudDrawImages(texture_ref ref, int start, int end)
{
	extern cvar_t gl_program_hud;

	R_TraceEnterFunctionRegion;
	if (R_TextureReferenceIsValid(glc_last_texture_used) && !R_TextureReferenceEqual(glc_last_texture_used, ref)) {
		renderer.TextureSetFiltering(glc_last_texture_used, texture_minification_linear, texture_magnification_linear);
	}
	glc_last_texture_used = ref;

	if (buffers.supported && gl_program_hud.integer && GLC_ProgramHudImagesCompile()) {
		GLC_StateBeginImageDraw(imageData.images[start].flags & IMAGEPROG_FLAGS_TEXT);
		GLC_HudDrawImagesProgram(r_program_hud_images_glc, ref, start, end);
	}
	else if (R_VAOBound()) {
		GLC_StateBeginImageDrawNonGLSL(imageData.images[start].flags & IMAGEPROG_FLAGS_TEXT);
		GLC_HudDrawImagesVertexArray(ref, start, end);
	}
	else {
		GLC_StateBeginImageDrawNonGLSL(imageData.images[start].flags & IMAGEPROG_FLAGS_TEXT);
		GLC_HudDrawImages_Immediate(ref, start, end);
	}
	R_TraceLeaveFunctionRegion;
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
