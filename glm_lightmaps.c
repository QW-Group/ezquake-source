/*
Copyright (C) 1996-1997 Id Software, Inc.

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
#include "r_local.h"
#include "gl_local.h"
#include "r_lightmaps.h"
#include "r_lighting.h"
#include "r_texture.h"
#include "glsl/constants.glsl"
#include "r_lightmaps_internal.h"
#include "glm_local.h"

// Hardware lighting: flag surfaces as we add them
static buffer_ref ssbo_surfacesTodo;
static texture_ref lightmap_texture_array;
static texture_ref lightmap_data_array;
static texture_ref lightmap_source_array;
static unsigned int lightmap_depth;
static unsigned int* surfaceTodoData;
static int surfaceTodoLength;
static int maximumSurfaceNumber;

texture_ref GLM_LightmapArray(void)
{
	return lightmap_texture_array;
}

void GLM_ComputeLightmaps(void)
{
	static glm_program_t lightmap_program;
	static buffer_ref ssbo_lightingData;

	if (GLM_ProgramRecompileNeeded(&lightmap_program, 0)) {
		extern unsigned char lighting_compute_glsl[];
		extern unsigned int lighting_compute_glsl_len;

		if (!GLM_CompileComputeShaderProgram(&lightmap_program, (const char*)lighting_compute_glsl, lighting_compute_glsl_len)) {
			return;
		}
	}

	if (!GL_BufferReferenceIsValid(ssbo_lightingData)) {
		ssbo_lightingData = buffers.Create(buffertype_storage, "lightstyles", sizeof(d_lightstylevalue), d_lightstylevalue, bufferusage_once_per_frame);
	}
	else {
		buffers.Update(ssbo_lightingData, sizeof(d_lightstylevalue), d_lightstylevalue);
	}
	buffers.BindRange(ssbo_lightingData, EZQ_GL_BINDINGPOINT_LIGHTSTYLES, buffers.BufferOffset(ssbo_lightingData), sizeof(d_lightstylevalue));

	buffers.Update(ssbo_surfacesTodo, surfaceTodoLength, surfaceTodoData);
	buffers.BindRange(ssbo_surfacesTodo, EZQ_GL_BINDINGPOINT_SURFACES_TO_LIGHT, buffers.BufferOffset(ssbo_surfacesTodo), surfaceTodoLength);

	GL_BindImageTexture(0, lightmap_source_array, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32UI);
	GL_BindImageTexture(1, lightmap_texture_array, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
	GL_BindImageTexture(2, lightmap_data_array, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32I);

	GLM_UseProgram(lightmap_program.program);
	GL_DispatchCompute(LIGHTMAP_WIDTH / HW_LIGHTING_BLOCK_SIZE, LIGHTMAP_HEIGHT / HW_LIGHTING_BLOCK_SIZE, lightmap_array_size);
	GL_MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void GLM_CreateLightmapTextures(void)
{
	int i, j;

	maximumSurfaceNumber = 0;
	for (j = 1; j < MAX_MODELS; j++) {
		model_t* m = cl.model_precache[j];
		if (!m) {
			break;
		}
		if (m->name[0] == '*') {
			continue;
		}
		if (m->isworldmodel) {
			maximumSurfaceNumber = max(maximumSurfaceNumber, m->numsurfaces);
		}
	}

	// Round up to nearest 32
	surfaceTodoLength = ((maximumSurfaceNumber + 31) / 32) * sizeof(unsigned int);
	Q_free(surfaceTodoData);
	surfaceTodoData = (unsigned int*)Q_malloc(surfaceTodoLength);
	if (!GL_BufferReferenceIsValid(ssbo_surfacesTodo)) {
		ssbo_surfacesTodo = buffers.Create(buffertype_storage, "surfaces-to-light", surfaceTodoLength, NULL, bufferusage_once_per_frame);
	}
	else {
		buffers.EnsureSize(ssbo_surfacesTodo, surfaceTodoLength);
	}

	if (GL_TextureReferenceIsValid(lightmap_texture_array) && lightmap_depth >= lightmap_array_size) {
		return;
	}

	if (GL_TextureReferenceIsValid(lightmap_texture_array)) {
		R_DeleteTextureArray(&lightmap_texture_array);
	}

	if (GL_TextureReferenceIsValid(lightmap_data_array)) {
		R_DeleteTextureArray(&lightmap_data_array);
	}

	if (GL_TextureReferenceIsValid(lightmap_source_array)) {
		R_DeleteTextureArray(&lightmap_source_array);
	}

	GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_texture_array, "lightmap_texture_array");
	GL_TexStorage3D(GL_TEXTURE0, lightmap_texture_array, 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);
	GL_SetTextureFiltering(lightmap_texture_array, texture_minification_linear, texture_magnification_linear);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	lightmap_depth = lightmap_array_size;
	for (i = 0; i < lightmap_array_size; ++i) {
		lightmaps[i].gl_texref = lightmap_texture_array;
	}

	GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_source_array, "lightmap_source_array");
	GL_TexStorage3D(GL_TEXTURE0, lightmap_source_array, 1, GL_RGBA32UI, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);

	GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_data_array, "lightmap_data_array");
	GL_TexStorage3D(GL_TEXTURE0, lightmap_data_array, 1, GL_RGBA32I, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size);
}

void GLM_InvalidateLightmapTextures(void)
{
	GL_TextureReferenceInvalidate(lightmap_texture_array);
	GL_TextureReferenceInvalidate(lightmap_data_array);
	GL_TextureReferenceInvalidate(lightmap_source_array);
	lightmap_depth = 0;
}

void GLM_LightmapFrameInit(void)
{
	memset(surfaceTodoData, 0, surfaceTodoLength);
}

void GLM_LightmapShutdown(void)
{
	Q_free(surfaceTodoData);
	surfaceTodoLength = 0;
}

void GLM_RenderDynamicLightmaps(msurface_t* s, qbool world)
{
	R_RenderDynamicLightmaps(s, world);

	if (world && surfaceTodoData && s->surfacenum < maximumSurfaceNumber) {
		surfaceTodoData[s->surfacenum / 32] |= (1 << (s->surfacenum % 32));
	}
}

void GLM_BuildLightmap(int i)
{
	GL_TexSubImage3D(
		GL_TEXTURE0, lightmap_texture_array, 0, 0, 0, i,
		LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		lightmaps[i].rawdata
	);

	GL_TexSubImage3D(
		GL_TEXTURE0, lightmap_source_array, 0, 0, 0, i,
		LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
		lightmaps[i].sourcedata
	);

	GL_TexSubImage3D(
		GL_TEXTURE0, lightmap_data_array, 0, 0, 0, i,
		LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, GL_RGBA_INTEGER, GL_INT,
		lightmaps[i].computeData
	);
}

void GLM_UploadLightmap(int textureUnit, int lightmapnum)
{
	const lightmap_data_t* lm = &lightmaps[lightmapnum];
	const void* data_source = lm->rawdata + (lm->change_area.t) * LIGHTMAP_WIDTH * 4;

	GL_TexSubImage3D(GL_TEXTURE0 + textureUnit, lightmap_texture_array, 0, 0, lm->change_area.t, lightmapnum, LIGHTMAP_WIDTH, lm->change_area.h, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data_source);
}
