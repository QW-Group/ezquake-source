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
#include "gl_local.h"
#include "r_lightmaps_internal.h"
#include "r_texture.h"
#include "gl_texture_internal.h"
#include "r_renderer.h"

void GLC_UploadLightmap(int textureUnit, int lightmapnum);

void GLC_SetTextureLightmap(int textureUnit, int lightmap_num)
{
	//update lightmap if it has been modified by dynamic lights
	if (lightmaps[lightmap_num].modified) {
		GLC_UploadLightmap(textureUnit, lightmap_num);
	}
	else {
		renderer.TextureUnitBind(textureUnit, lightmaps[lightmap_num].gl_texref);
	}
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
	if (index < 0 || index >= lightmap_array_size) {
		return lightmaps[0].gl_texref;
	}
	return lightmaps[index].gl_texref;
}

void GLC_CreateLightmapTextures(void)
{
	int i;

	for (i = 0; i < lightmap_array_size; ++i) {
		if (!R_TextureReferenceIsValid(lightmaps[i].gl_texref)) {
			R_CreateTexture2D(&lightmaps[i].gl_texref, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, va("lightmap-%03d", i));
		}
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
	GL_TexSubImage2D(
		GL_TEXTURE0, lightmaps[i].gl_texref, 0, 0, 0,
		LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		lightmaps[i].rawdata
	);
}

void GLC_UploadLightmap(int textureUnit, int lightmapnum)
{
	const lightmap_data_t* lm = &lightmaps[lightmapnum];
	const void* data_source = lm->rawdata + (lm->change_area.t) * LIGHTMAP_WIDTH * 4;

	GL_TexSubImage2D(GL_TEXTURE0 + textureUnit, lm->gl_texref, 0, 0, lm->change_area.t, LIGHTMAP_WIDTH, lm->change_area.h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data_source);
}
