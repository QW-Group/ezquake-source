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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_lightmaps_internal.h"
#include "r_texture.h"
#include "gl_texture_internal.h"
#include "r_renderer.h"
#include "tr_types.h"
#include "r_lightmaps.h"

// FIXME: Check required depth against GL_MAX_ARRAY_TEXTURE_LAYERS
static texture_ref lightmap_texture_array;
static unsigned int lightmap_depth = 0;

static void GLC_CreateLightmapTextureArray(void);
static void GLC_CreateLightmapTexturesIndividual(void);

void GLC_UploadLightmap(int textureUnit, int lightmapnum);

qbool GLC_SetTextureLightmap(int textureUnit, int lightmap_num)
{
	//update lightmap if it has been modified by dynamic lights
	R_UploadLightMap(textureUnit, lightmap_num);
	if (R_TextureReferenceIsValid(lightmap_texture_array)) {
		return renderer.TextureUnitBind(textureUnit, lightmap_texture_array);
	}
	else {
		return renderer.TextureUnitBind(textureUnit, lightmaps[lightmap_num].gl_texref);
	}
}

void GLC_LightmapArrayToggle(qbool use_array)
{
	qbool array_in_use = R_TextureReferenceIsValid(lightmap_texture_array);
	use_array &= (glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS) != 0;

	if (array_in_use == use_array) {
		return;
	}

	if (use_array && !array_in_use) {
		GLC_CreateLightmapTextureArray();
	}
	else if (array_in_use && !use_array) {
		GLC_CreateLightmapTexturesIndividual();
		R_DeleteTextureArray(&lightmap_texture_array);
		lightmap_depth = 0;
	}

	// Force all lightmaps to be updated
	R_ForceReloadLightMaps();
}

void GLC_InvalidateLightmapTextures(void)
{
	R_TextureReferenceInvalidate(lightmap_texture_array);
	lightmap_depth = 0;
}

void GLC_ClearLightmapPolys(void)
{
	int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		lightmaps[i].poly_chain = NULL;
	}
}

texture_ref GLC_LightmapTexture(int index)
{
	if (R_TextureReferenceIsValid(lightmap_texture_array)) {
		return lightmap_texture_array;
	}
	else {
		if (index < 0 || index >= lightmap_array_size) {
			return lightmaps[0].gl_texref;
		}
		return lightmaps[index].gl_texref;
	}
}

static void GLC_CreateLightmapTextureArray(void)
{
	if (R_TextureReferenceIsValid(lightmap_texture_array) && lightmap_depth >= lightmap_array_size) {
		return;
	}
	if (R_TextureReferenceIsValid(lightmap_texture_array)) {
		R_DeleteTextureArray(&lightmap_texture_array);
	}
	GL_CreateTexturesWithIdentifier(texture_type_2d_array, 1, &lightmap_texture_array, "lightmap_texture_array");
	GL_TexStorage3D(GL_TEXTURE0, lightmap_texture_array, 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size, true);
	R_SetTextureArraySize(lightmap_texture_array, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmap_array_size, 4);
#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nopengl-texture,alloc,%u,%d,%d,%d,%s\n", lightmap_texture_array.index, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, LIGHTMAP_WIDTH * LIGHTMAP_HEIGHT * lightmap_array_size * 4, "lightmap_texture_array");
#endif
	renderer.TextureSetFiltering(lightmap_texture_array, texture_minification_linear, texture_magnification_linear);
	renderer.TextureWrapModeClamp(lightmap_texture_array);
	lightmap_depth = lightmap_array_size;
}

static void GLC_CreateLightmapTexturesIndividual(void)
{
	int i;
	char name[64];

	for (i = 0; i < lightmap_array_size; ++i) {
		if (!R_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			snprintf(name, sizeof(name), "lightmap-%03d", i);

			renderer.TextureCreate2D(&lightmaps[i].gl_texref, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, name, true);
		}
	}
}

void GLC_CreateLightmapTextures(void)
{
	extern cvar_t gl_program_world;
	qbool supported = (glConfig.supported_features & R_SUPPORT_TEXTURE_ARRAYS);
	qbool requested = gl_program_world.integer;

	if (supported && requested) {
		GLC_CreateLightmapTextureArray();
	}
	else {
		GLC_CreateLightmapTexturesIndividual();
	}
}

void GLC_LightmapUpdate(int index)
{
	if (index >= 0 && index < lightmap_array_size) {
		if (lightmaps[index].modified) {
			GLC_UploadLightmap(0, index);
		}
	}
}

void GLC_AddToLightmapChain(msurface_t* s)
{
	s->polys->chain = lightmaps[s->lightmaptexturenum].poly_chain;
	lightmaps[s->lightmaptexturenum].poly_chain = s->polys;
}

glpoly_t* GLC_LightmapChain(int i)
{
	if (i < 0 || i >= lightmap_array_size) {
		return NULL;
	}
	return lightmaps[i].poly_chain;
}

int GLC_LightmapCount(void)
{
	return lightmap_array_size;
}

void GLC_BuildLightmap(int i)
{
	GLenum format = GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS) ? GL_BGRA : GL_RGBA;
	GLenum type = GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS) ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE;

	if (R_TextureReferenceIsValid(lightmap_texture_array)) {
		GL_TexSubImage3D(GL_TEXTURE0, lightmap_texture_array, 0, 0, 0, i, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, 1, format, type, lightmaps[i].rawdata);
	}
	else {
		GL_TexSubImage2D(
			GL_TEXTURE0, lightmaps[i].gl_texref, 0, 0, 0,
			LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, format, type,
			lightmaps[i].rawdata
		);
	}
}

void GLC_UploadLightmap(int textureUnit, int lightmapnum)
{
	const lightmap_data_t* lm = &lightmaps[lightmapnum];
	const void* data_source = lm->rawdata + (lm->change_area.t) * LIGHTMAP_WIDTH * 4;
	GLenum format = GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS) ? GL_BGRA : GL_RGBA;
	GLenum type = GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS) ? GL_UNSIGNED_INT_8_8_8_8_REV : GL_UNSIGNED_BYTE;

	if (R_TextureReferenceIsValid(lightmap_texture_array)) {
		GL_TexSubImage3D(GL_TEXTURE0 + textureUnit, lightmap_texture_array, 0, 0, lm->change_area.t, lightmapnum, LIGHTMAP_WIDTH, lm->change_area.h, 1, format, type, data_source);
	}
	else {
		GL_TexSubImage2D(GL_TEXTURE0 + textureUnit, lm->gl_texref, 0, 0, lm->change_area.t, LIGHTMAP_WIDTH, lm->change_area.h, format, type, data_source);
	}
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
