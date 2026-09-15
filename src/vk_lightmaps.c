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

#ifdef RENDERER_OPTION_VULKAN

#include "quakedef.h"
#include "gl_model.h"
#include "r_lightmaps.h"
#include "r_lightmaps_internal.h"
#include "r_texture.h"
#include "tr_types.h"
#include "vk_local.h"

void VK_CreateLightmapTextures(void)
{
	unsigned int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		char name[64];

		if (R_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			continue;
		}

		snprintf(name, sizeof(name), "vk_lightmap_%u", i);
		VK_TextureCreate2D(&lightmaps[i].gl_texref, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, name, true);
		if (R_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			VK_TextureSetFiltering(lightmaps[i].gl_texref, texture_minification_linear, texture_magnification_linear);
			VK_TextureWrapModeClamp(lightmaps[i].gl_texref);
		}
	}
}

void VK_UploadLightmap(int textureUnit, int lightmapnum)
{
	lightmap_data_t* lm;
	byte* data_source;

	(void)textureUnit;

	if (lightmapnum < 0 || lightmapnum >= (int)lightmap_array_size) {
		return;
	}

	lm = &lightmaps[lightmapnum];
	if (!R_TextureReferenceIsValid(lm->gl_texref)) {
		VK_CreateLightmapTextures();
	}
	if (!R_TextureReferenceIsValid(lm->gl_texref) || lm->change_area.h == 0) {
		return;
	}

	data_source = lm->rawdata + lm->change_area.t * LIGHTMAP_WIDTH * 4;
	VK_TextureReplaceSubImageRGBA(lm->gl_texref, 0, lm->change_area.t, LIGHTMAP_WIDTH, lm->change_area.h, data_source);
}

void VK_BuildLightmap(int lightmapnum)
{
	if (lightmapnum < 0 || lightmapnum >= (int)lightmap_array_size) {
		return;
	}
	if (!R_TextureReferenceIsValid(lightmaps[lightmapnum].gl_texref)) {
		VK_CreateLightmapTextures();
	}
	if (!R_TextureReferenceIsValid(lightmaps[lightmapnum].gl_texref)) {
		return;
	}

	VK_TextureReplaceSubImageRGBA(lightmaps[lightmapnum].gl_texref, 0, 0, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, lightmaps[lightmapnum].rawdata);
}

void VK_InvalidateLightmapTextures(void)
{
	unsigned int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		if (R_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			R_DeleteTexture(&lightmaps[i].gl_texref);
		}
	}
}

void VK_LightmapFrameInit(void)
{
}

void VK_LightmapShutdown(void)
{
}

void VK_RenderDynamicLightmaps(msurface_t* surface, qbool world)
{
	R_RenderDynamicLightmaps(surface, world);
}

#endif // #ifdef RENDERER_OPTION_VULKAN
