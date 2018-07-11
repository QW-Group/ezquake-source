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
// gl_sky.c -- sky polygons (was previously in gl_warp.c)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_brushmodel_sky.h"
#include "r_texture.h"

texture_ref skybox_cubeMap;

void GLM_DrawSky(void)
{
	return;
}

static void GLM_CopySkyboxTexturesToCubeMap(texture_ref cubemap, int width, int height)
{
	static int skytexorder[MAX_SKYBOXTEXTURES] = {0,2,1,3,4,5};
	int i;
	GLbyte* data;

	// Copy data from 2d images into cube-map
	data = Q_malloc(4 * width * height);
	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		GL_GetTexImage(GL_TEXTURE0, skyboxtextures[skytexorder[i]], 0, GL_RGBA, GL_UNSIGNED_BYTE, 4 * width * height, data);

		GL_TexSubImage3D(GL_TEXTURE0, cubemap, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}
	Q_free(data);

	GL_GenerateMipmap(GL_TEXTURE0, cubemap);
}

qbool GLM_LoadSkyboxTextures(const char* skyname)
{
	int widths[MAX_SKYBOXTEXTURES], heights[MAX_SKYBOXTEXTURES];
	qbool all_same = true;
	int i;

	// FIXME: Delete previous?
	GL_TextureReferenceInvalidate(skybox_cubeMap);

	if (!Sky_LoadSkyboxTextures(skyname)) {
		return false;
	}

	// Get the actual sizes (may have been rescaled)
	for (i = 0; i < MAX_SKYBOXTEXTURES; ++i) {
		widths[i] = GL_TextureWidth(skyboxtextures[i]);
		heights[i] = GL_TextureHeight(skyboxtextures[i]);
	}

	// Check if they're all the same
	for (i = 0; i < MAX_SKYBOXTEXTURES - 1 && all_same; ++i) {
		all_same &= widths[i] == widths[i + 1];
		all_same &= heights[i] == heights[i + 1];
	}

	if (!all_same) {
		Con_Printf("Skybox found, but textures differ in dimensions\n");
		return false;
	}

	skybox_cubeMap = GL_CreateCubeMap("skybox:cubemap", widths[0], heights[0], TEX_NOCOMPRESS | TEX_MIPMAP);

	GLM_CopySkyboxTexturesToCubeMap(skybox_cubeMap, widths[0], heights[0]);

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	return true;
}
