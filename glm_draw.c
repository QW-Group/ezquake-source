/*
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
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#include "tr_types.h"
#include "glm_draw.h"
#include "glm_vao.h"
#include "r_draw.h"
#include "r_matrix.h"
#include "glsl/constants.glsl"
#include "glm_local.h"
#include "r_program.h"

void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t);
static void GLM_CreateMultiImageProgram(void);

static buffer_ref polygonVBO;
static buffer_ref line_vbo;

static buffer_ref imageIndexBuffer;
static buffer_ref imageVBO;
extern float overall_alpha;
extern float cachedMatrix[16];

static buffer_ref circleVBO;

// Flags to detect when the user has changed settings
#define GLM_HUDIMAGES_SMOOTHTEXT        1
#define GLM_HUDIMAGES_SMOOTHCROSSHAIR   2
#define GLM_HUDIMAGES_SMOOTHIMAGES      4
#define GLM_HUDIMAGES_SMOOTHEVERYTHING  (GLM_HUDIMAGES_SMOOTHTEXT | GLM_HUDIMAGES_SMOOTHCROSSHAIR | GLM_HUDIMAGES_SMOOTHIMAGES)

static void GLM_SetCoordinates(glm_image_t* targ, float x1, float y1, float x2, float y2, float s, float s_width, float t, float t_height, int flags)
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

	targ[0].flags = targ[1].flags = targ[2].flags = targ[3].flags = flags;
}

void GLM_HudDrawComplete(void)
{
	if ((R_ProgramCustomOptions(r_program_hud_images) & GLM_HUDIMAGES_SMOOTHEVERYTHING) == 0) {
		GLM_SamplerClear(0);
	}
	else if ((R_ProgramCustomOptions(r_program_hud_images) & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
		GLM_SamplerClear(1);
	}
}

void GLM_HudDrawPolygons(texture_ref texture, int start, int end)
{
	int i;
	uintptr_t offset = buffers.BufferOffset(polygonVBO) / sizeof(polygonData.polygonVertices[0]);

	R_BindVertexArray(vao_hud_polygons);
	R_ProgramUse(r_program_hud_polygon);
	R_ProgramUniformMatrix4fv(r_program_uniform_hud_polygon_matrix, R_ProjectionMatrix());

	GLM_StateBeginPolygonDraw();

	for (i = start; i <= end; ++i) {
		//R_TransformMatrix(R_ProjectionMatrix(), polygonData.polygonX[i], polygonData.polygonY[i], 0);
		R_ProgramUniform4fv(r_program_uniform_hud_polygon_color, polygonData.polygonColor[i]);

		GL_DrawArrays(GL_TRIANGLE_STRIP, offset + i * MAX_POLYGON_POINTS, polygonData.polygonVerts[i]);
	}
}

void GLM_HudPreparePolygons(void)
{
	if (R_ProgramRecompileNeeded(r_program_hud_polygon, 0) && !R_ProgramCompile(r_program_hud_polygon)) {
		return;
	}

	if (!R_BufferReferenceIsValid(polygonVBO)) {
		polygonVBO = buffers.Create(buffertype_vertex, "polygon-vbo", sizeof(polygonData.polygonVertices), polygonData.polygonVertices, bufferusage_once_per_frame);
	}
	else if (polygonData.polygonVerts[0]) {
		buffers.Update(polygonVBO, polygonData.polygonCount * MAX_POLYGON_POINTS * sizeof(polygonData.polygonVertices[0]), polygonData.polygonVertices);
	}

	if (!R_VertexArrayCreated(vao_hud_polygons)) {
		R_GenVertexArray(vao_hud_polygons);
		GLM_ConfigureVertexAttribPointer(vao_hud_polygons, polygonVBO, 0, 3, GL_FLOAT, GL_FALSE, sizeof(polygonData.polygonVertices[0]), NULL, 0);
		R_BindVertexArray(vao_none);
	}
}

void GLM_HudPrepareLines(void)
{
	if (!R_BufferReferenceIsValid(line_vbo)) {
		line_vbo = buffers.Create(buffertype_vertex, "line", sizeof(lineData.line_points), lineData.line_points, bufferusage_once_per_frame);
	}
	else if (lineData.lineCount) {
		buffers.Update(line_vbo, sizeof(lineData.line_points[0]) * lineData.lineCount * 2, lineData.line_points);
	}

	if (!R_VertexArrayCreated(vao_hud_lines)) {
		R_GenVertexArray(vao_hud_lines);

		GLM_ConfigureVertexAttribPointer(vao_hud_lines, line_vbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof(glm_line_point_t), VBO_FIELDOFFSET(glm_line_point_t, position), 0);
		GLM_ConfigureVertexAttribPointer(vao_hud_lines, line_vbo, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(glm_line_point_t), VBO_FIELDOFFSET(glm_line_point_t, color), 0);

		R_BindVertexArray(vao_none);
	}

	if (R_ProgramRecompileNeeded(r_program_hud_line, 0)) {
		R_ProgramCompile(r_program_hud_line);
	}
}

void GLM_HudDrawLines(int start, int end)
{
	if (R_ProgramReady(r_program_hud_line) && R_VertexArrayCreated(vao_hud_lines)) {
		int i;
		uintptr_t offset = buffers.BufferOffset(line_vbo) / sizeof(glm_line_point_t);

		R_ProgramUse(r_program_hud_line);
		R_ProgramUniformMatrix4fv(r_program_uniform_hud_line_matrix, R_ProjectionMatrix());
		R_BindVertexArray(vao_hud_lines);

		for (i = start; i <= end; ++i) {
			R_StateBeginAlphaLineRGB(lineData.line_thickness[i]);
			GL_DrawArrays(GL_LINES, offset + i * 2, 2);
		}
	}
}

void GLM_DrawRectangle(float x, float y, float width, float height, byte* color)
{
	texture_ref solid_texture;
	float s, t, alpha;
	glm_image_t* img;

	if (imageData.imageCount >= MAX_MULTI_IMAGE_BATCH) {
		return;
	}

	Atlas_SolidTextureCoordinates(&solid_texture, &s, &t);
	if (!R_LogCustomImageTypeWithTexture(imagetype_image, imageData.imageCount, solid_texture)) {
		return;
	}

	img = &imageData.images[imageData.imageCount * 4];
	alpha = (color[3] * overall_alpha / 255.0f);

	img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
	img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
	img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
	img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

	GLM_SetCoordinates(img, x, y, x + width, y + height, s, 0, t, 0, IMAGEPROG_FLAGS_TEXTURE);

	++imageData.imageCount;
}

void GLM_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, int flags)
{
	float alpha = (color[3] * overall_alpha / 255.0f);
	glm_image_t* img = &imageData.images[imageData.imageCount * 4];

	img->colour[0] = (img + 1)->colour[0] = (img + 2)->colour[0] = (img + 3)->colour[0] = color[0] * alpha;
	img->colour[1] = (img + 1)->colour[1] = (img + 2)->colour[1] = (img + 3)->colour[1] = color[1] * alpha;
	img->colour[2] = (img + 1)->colour[2] = (img + 2)->colour[2] = (img + 3)->colour[2] = color[2] * alpha;
	img->colour[3] = (img + 1)->colour[3] = (img + 2)->colour[3] = (img + 3)->colour[3] = color[3] * overall_alpha;

	GLM_SetCoordinates(img, x, y, x + width, y + height, tex_s, tex_width, tex_t, tex_height, flags);

	++imageData.imageCount;
}

void GLM_AdjustImages(int first, int last, float x_offset)
{
	int i;

	for (i = first; i < last; ++i) {
		imageData.images[i * 4 + 0].pos[0] += x_offset;
		imageData.images[i * 4 + 1].pos[0] += x_offset;
		imageData.images[i * 4 + 2].pos[0] += x_offset;
		imageData.images[i * 4 + 3].pos[0] += x_offset;
	}
}

void GLM_HudPrepareImages(void)
{
	GLM_CreateMultiImageProgram();

	if (imageData.imageCount) {
		buffers.Update(imageVBO, sizeof(imageData.images[0]) * imageData.imageCount * 4, imageData.images);
		if ((R_ProgramCustomOptions(r_program_hud_images) & GLM_HUDIMAGES_SMOOTHEVERYTHING) == 0) {
			// Everything is GL_NEAREST, no smoothing
			GLM_SamplerSetNearest(0);
		}
		else if ((R_ProgramCustomOptions(r_program_hud_images) & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			// Mixed, second texture unit for GL_NEAREST
			GLM_SamplerSetNearest(1);
		}
	}
}

static void GLM_CreateMultiImageProgram(void)
{
	extern cvar_t r_smoothtext, r_smoothcrosshair, r_smoothimages;

	int program_flags =
		(r_smoothtext.integer ? GLM_HUDIMAGES_SMOOTHTEXT : 0) |
		(r_smoothcrosshair.integer ? GLM_HUDIMAGES_SMOOTHCROSSHAIR : 0) |
		(r_smoothimages.integer ? GLM_HUDIMAGES_SMOOTHIMAGES : 0);

	if (R_ProgramRecompileNeeded(r_program_hud_images, program_flags)) {
		char included_definitions[512];

		included_definitions[0] = '\0';

		if (program_flags != 0 && program_flags != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			// depends on flags on individual elements
			strlcpy(included_definitions, "#define MIXED_SAMPLING", sizeof(included_definitions));
		}

		// Initialise program for drawing image
		R_ProgramCompileWithInclude(r_program_hud_images, included_definitions);

		R_ProgramSetCustomOptions(r_program_hud_images, program_flags);
	}

	if (!R_BufferReferenceIsValid(imageVBO)) {
		imageVBO = buffers.Create(buffertype_vertex, "image-vbo", sizeof(imageData.images), imageData.images, bufferusage_once_per_frame);
	}

	if (!R_VertexArrayCreated(vao_hud_images)) {
		R_GenVertexArray(vao_hud_images);

		if (!R_BufferReferenceIsValid(imageIndexBuffer)) {
			GLuint i;
			int imageIndexLength = MAX_MULTI_IMAGE_BATCH * 5 * sizeof(GLuint);
			GLuint* imageIndexData = Q_malloc(imageIndexLength);

			for (i = 0; i < MAX_MULTI_IMAGE_BATCH; ++i) {
				imageIndexData[i * 5 + 0] = i * 4;
				imageIndexData[i * 5 + 1] = i * 4 + 1;
				imageIndexData[i * 5 + 2] = i * 4 + 2;
				imageIndexData[i * 5 + 3] = i * 4 + 3;
				imageIndexData[i * 5 + 4] = ~(GLuint)0;
			}

			imageIndexBuffer = buffers.Create(buffertype_index, "image-indexes", imageIndexLength, imageIndexData, bufferusage_constant_data);
			Q_free(imageIndexData);
		}

		buffers.Bind(imageIndexBuffer);
		GLM_ConfigureVertexAttribPointer(vao_hud_images, imageVBO, 0, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, pos), 0);
		GLM_ConfigureVertexAttribPointer(vao_hud_images, imageVBO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, tex), 0);
		GLM_ConfigureVertexAttribPointer(vao_hud_images, imageVBO, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, colour), 0);
		GLM_ConfigureVertexAttribIPointer(vao_hud_images, imageVBO, 3, 1, GL_INT, sizeof(imageData.images[0]), VBO_FIELDOFFSET(glm_image_t, flags), 0);
		R_BindVertexArray(vao_none);
	}
}

void GLM_HudDrawImages(texture_ref texture, int start, int end)
{
	uintptr_t index_offset = (start * 5 * sizeof(GLuint));
	uintptr_t buffer_offset = buffers.BufferOffset(imageVBO) / sizeof(imageData.images[0]);

	R_ProgramUse(r_program_hud_images);
	GLM_StateBeginImageDraw();
	if (R_TextureReferenceIsValid(texture)) {
		if ((R_ProgramCustomOptions(r_program_hud_images) & GLM_HUDIMAGES_SMOOTHEVERYTHING) != GLM_HUDIMAGES_SMOOTHEVERYTHING) {
			texture_ref textures[] = { texture, texture };

			R_BindTextures(0, 2, textures);
		}
		else {
			R_TextureUnitBind(0, texture);
		}
	}
	GL_DrawElementsBaseVertex(GL_TRIANGLE_STRIP, (end - start + 1) * 5, GL_UNSIGNED_INT, (GLvoid*)index_offset, buffer_offset);
}

void GLM_HudPrepareCircles(void)
{
	if (R_ProgramRecompileNeeded(r_program_hud_circles, 0)) {
		if (!R_ProgramCompile(r_program_hud_circles)) {
			return;
		}
	}

	// Build VBO
	if (!R_BufferReferenceIsValid(circleVBO)) {
		circleVBO = buffers.Create(buffertype_vertex, "circle-vbo", sizeof(circleData.drawCirclePointData), circleData.drawCirclePointData, bufferusage_once_per_frame);
	}
	else if (circleData.circleCount) {
		buffers.Update(circleVBO, circleData.circleCount * FLOATS_PER_CIRCLE * sizeof(circleData.drawCirclePointData[0]), circleData.drawCirclePointData);
	}

	// Build VAO
	if (!R_VertexArrayCreated(vao_hud_circles)) {
		R_GenVertexArray(vao_hud_circles);

		GLM_ConfigureVertexAttribPointer(vao_hud_circles, circleVBO, 0, 2, GL_FLOAT, GL_FALSE, 0, NULL, 0);
	}
}

void GLM_HudDrawCircles(texture_ref texture, int start, int end)
{
	// FIXME: Not very efficient (but rarely used either)
	int i;
	uintptr_t offset = buffers.BufferOffset(circleVBO) / (sizeof(float) * 2);

	start = max(0, start);
	end = min(end, circleData.circleCount - 1);

	R_ProgramUse(r_program_hud_circles);
	R_BindVertexArray(vao_hud_circles);
	R_ProgramUniformMatrix4fv(r_program_uniform_hud_circle_matrix, R_ProjectionMatrix());

	for (i = start; i <= end; ++i) {
		R_ProgramUniform4fv(r_program_uniform_hud_circle_color, circleData.drawCircleColors[i]);

		GL_DrawArrays(circleData.drawCircleFill[i] ? GL_TRIANGLE_STRIP : GL_LINE_LOOP, offset + i * FLOATS_PER_CIRCLE / 2, circleData.drawCirclePoints[i]);
	}
}
