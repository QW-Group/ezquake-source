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
#include "teamplay.h"
#include "r_brushmodel_sky.h"
#include "r_texture.h"
#include "r_local.h"
#include "r_trace.h"
#include "r_renderer.h"

texture_ref solidskytexture, alphaskytexture;
texture_ref skyboxtextures[MAX_SKYBOXTEXTURES];

qbool r_skyboxloaded;

//A sky texture is 256 * 128, with the right side being a masked overlay
void R_InitSky (texture_t *mt) {
	int i, j, p, r, g, b;
	byte *src;
	unsigned trans[128 * 128], transpix, *rgba;
	int flags = TEX_MIPMAP | (gl_scaleskytextures.integer ? 0 : TEX_NOSCALE);

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}
	}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	solidskytexture = R_LoadTexture ("***solidskytexture***", 128, 128, (byte *)trans, flags, 4);

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			trans[(i * 128) + j] = p ? d_8to24table[p] : transpix;
		}
	}

	alphaskytexture = R_LoadTexture ("***alphaskytexture***", 128, 128, (byte *)trans, TEX_ALPHA | flags, 4);
}

int R_SetSky(char *skyname)
{
	char *groupname;

	r_skyboxloaded = false;

	// set skyname to groupname if any
	skyname	= (groupname = TP_GetSkyGroupName(TP_MapName(), NULL)) ? groupname : skyname;

	if (!skyname || !skyname[0] || strchr(skyname, '.'))
	{
		// Empty name or contain dot(dot causing troubles with skipping part of the name as file extenson),
		// so do nothing.
		return 1;
	}

	if (!renderer.LoadSkyboxTextures(skyname)) {
		return 1;
	}

	// everything was OK
	r_skyboxloaded = true;
	return 0;
}

void OnChange_r_skyname (cvar_t *v, char *skyname, qbool* cancel)
{
	if (!skyname[0]) {		
		r_skyboxloaded = false;
		return;
	}

	*cancel = R_SetSky(skyname);
}

void R_LoadSky_f(void)
{
	switch (Cmd_Argc()) {
	case 1:
		if (r_skyboxloaded) {
			Com_Printf("Current skybox is \"%s\"\n", r_skyname.string);
		}
		else {
			Com_Printf("No skybox has been set\n");
		}
		break;
	case 2:
		if (!strcasecmp(Cmd_Argv(1), "none")) {
			Cvar_Set(&r_skyname, "");
		}
		else {
			Cvar_Set(&r_skyname, Cmd_Argv(1));
		}
		break;
	default:
		Com_Printf("Usage: %s <skybox>\n", Cmd_Argv(0));
		break;
	}
}

/*
==============
R_DrawSky

Draw either the classic cloudy quake sky or a skybox
==============
*/
void R_DrawSky (void)
{
	if (!skychain) {
		return;
	}

	R_TraceEnterNamedRegion("R_DrawSky");
	renderer.DrawSky();
	R_TraceLeaveNamedRegion();

	skychain = NULL;
	skychain_tail = &skychain;
}

typedef enum {
	r_skybox_right,
	r_skybox_back,
	r_skybox_left,
	r_skybox_front,
	r_skybox_up,
	r_skybox_down,

	r_skybox_direction_count
} r_skybox_direction_id;

qbool R_LoadSkyTexturePixels(r_skybox_direction_id dir, const char* skyname, byte** data, int* width, int* height)
{
	static const char *skybox_ext[r_skybox_direction_count] = { "rt", "bk", "lf", "ft", "up", "dn" };
	static const char* search_paths[][2] = { { "env/", "" }, { "gfx/env/", "" }, { "env/", "_" }, { "gfx/env/", "_" } };
	int j, flags = TEX_NOCOMPRESS | TEX_MIPMAP | (gl_scaleskytextures.integer ? 0 : TEX_NOSCALE);

	if (dir < 0 || dir >= r_skybox_direction_count) {
		return false;
	}

	for (j = 0; j < sizeof(search_paths) / sizeof(search_paths[0]); ++j) {
		char path[MAX_PATH];

		strlcpy(path, search_paths[j][0], sizeof(path));
		strlcat(path, skyname, sizeof(path));
		strlcat(path, search_paths[j][1], sizeof(path));
		strlcat(path, skybox_ext[dir], sizeof(path));

		*data = R_LoadImagePixels(path, 0, 0, flags, width, height);
		if (*data) {
			return *data != NULL;
		}
	}

	return false;
}

qbool Sky_LoadSkyboxTextures(const char* skyname)
{
	r_skybox_direction_id i;
	byte* data;
	int fixed_size = 0;
	int width, height;

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++) {
		// FIXME: Delete old textures?
		R_TextureReferenceInvalidate(skyboxtextures[i]);

		if (R_LoadSkyTexturePixels(i, skyname, &data, &width, &height)) {
			char id[16];

			if (R_UseCubeMapForSkyBox()) {
				int size = (i != 0 ? fixed_size : min(width, height));

				R_TextureRescaleOverlay(&data, &width, &height, size, size);

				fixed_size = size;
			}

			snprintf(id, sizeof(id) - 1, "skybox[%d]", i);
			skyboxtextures[i] = R_LoadTexture(id, width, height, data, TEX_NOCOMPRESS | TEX_MIPMAP | TEX_NOSCALE, 4);

			// we should free data from R_LoadImagePixels()
			Q_free(data);
		}

		if (!R_TextureReferenceIsValid(skyboxtextures[i])) {
			Com_Printf("Couldn't load skybox \"%s\"\n", skyname);
			return false;
		}
	}

	return true;
}

void R_ClearSkyTextures(void)
{
	R_TextureReferenceInvalidate(solidskytexture);
	R_TextureReferenceInvalidate(alphaskytexture);
}
