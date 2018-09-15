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
// gl_brushmodel_textures.c  -- external texture loading for brush models

#include "quakedef.h"
#include "gl_model.h"
#include "teamplay.h"
#include "r_texture.h"
#include "r_brushmodel.h"
#include "image.h"
#include "r_renderer.h"

/* Some id maps have textures with identical names but different looks.
We hardcode a list of names, checksums and alternative names to provide a way
for external texture packs to differentiate them. */
static struct {
	int md4;
	char *origname, *newname;
} translate_names[] = {
	//	{ 0x8a010dc0, "sky4", "sky4" },
	{ 0xde688b77, "sky4", "sky1" },
	{ 0x45d110ec, "metal5_2", "metal5_2_arc" },
	{ 0x0d275f87, "metal5_2", "metal5_2_x" },
	{ 0xf8e27da8, "metal5_4", "metal5_4_arc" },
	{ 0xa301c52e, "metal5_4", "metal5_4_double" },
	{ 0xfaa8bf77, "metal5_8", "metal5_8_back" },
	{ 0x88792923, "metal5_8", "metal5_8_rune" },
	{ 0xfe4f9f5a, "plat_top1", "plat_top1_bolt" },
	{ 0x9ac3fccf, "plat_top1", "plat_top1_cable" },
	{ 0, NULL, NULL },
};

char *TranslateTextureName(texture_t *tx)
{
	int i;
	int checksum = 0;
	qbool checksum_done = false;

	for (i = 0; translate_names[i].origname; i++) {
		if (strcmp(tx->name, translate_names[i].origname)) {
			continue;
		}
		if (!checksum_done) {
			checksum = Com_BlockChecksum(tx+1, tx->width*tx->height);
			checksum_done = true;
			//Com_DPrintf ("checksum(\"%s\") = 0x%x\n", tx->name, checksum);
		}
		if (translate_names[i].md4 == checksum) {
			//Com_DPrintf ("Translating %s --> %s\n", tx->name, translate_names[i].newname);
			assert (strlen(translate_names[i].newname) < sizeof(tx->name));
			return translate_names[i].newname;
		}
	}

	return NULL;
}

qbool Mod_LoadExternalTexture(model_t* loadmodel, texture_t *tx, int mode, int brighten_flag)
{
	char *name, *altname, *mapname, *groupname;
	int luma_mode = TEX_LUMA;
	int material_width = 0;
	int material_height = 0;
	int luma_width = 0;
	int luma_height = 0;
	byte* material_pixels = NULL;
	byte* luma_pixels = NULL;
	char texture_path[MAX_OSPATH];
	extern cvar_t gl_lumatextures;

	if (!R_ExternalTexturesEnabled(loadmodel->isworldmodel)) {
		return false;
	}

	name = tx->name;
	altname = TranslateTextureName(tx);
	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, NULL);
	texture_path[0] = '\0';

	if (loadmodel->isworldmodel) {
		strlcpy(texture_path, "textures/", sizeof(texture_path));
		strlcat(texture_path, mapname, sizeof(texture_path));
		strlcat(texture_path, "/", sizeof(texture_path));
		strlcat(texture_path, name, sizeof(texture_path));

		material_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | brighten_flag, &material_width, &material_height);
		if (!material_pixels && groupname) {
			strlcpy(texture_path, "textures/", sizeof(texture_path));
			strlcat(texture_path, groupname, sizeof(texture_path));
			strlcat(texture_path, "/", sizeof(texture_path));
			strlcat(texture_path, name, sizeof(texture_path));

			material_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | brighten_flag, &material_width, &material_height);
		}
	}
	else {
		strlcpy(texture_path, "textures/bmodels/", sizeof(texture_path));
		strlcat(texture_path, name, sizeof(texture_path));

		material_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | brighten_flag, &material_width, &material_height);
	}

	if (!material_pixels && altname) {
		strlcpy(texture_path, "textures/", sizeof(texture_path));
		strlcat(texture_path, altname, sizeof(texture_path));

		material_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | brighten_flag, &material_width, &material_height);
	}

	if (!material_pixels) {
		strlcpy(texture_path, "textures/", sizeof(texture_path));
		strlcat(texture_path, name, sizeof(texture_path));

		material_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | brighten_flag, &material_width, &material_height);
	}

	// Try and load the corresponding luma
	if (material_pixels && !Mod_IsTurbTextureName(loadmodel, name)) {
		strlcat(texture_path, "_luma", sizeof(texture_path));

		luma_pixels = R_LoadImagePixels(texture_path, 0, 0, mode | luma_mode, &luma_width, &luma_height);
	}

	// resize luma if dimensions don't match material
	if (R_LumaTexturesMustMatchDimensions() && material_pixels && luma_pixels) {
		// make sure sizes match (so they fit in array): the shader still has to read from both alpha-material & luma
		R_TextureRescaleOverlay(&luma_pixels, &luma_width, &luma_height, material_width, material_height);
	}

	// Load into renderer
	if (material_pixels) {
		tx->gl_texturenum = R_LoadTexturePixels(material_pixels, name, material_width, material_height, mode);
		if (luma_pixels) {
			tx->fb_texturenum = R_LoadTexturePixels(luma_pixels, va("@fb_%s", name), luma_width, luma_height, mode | luma_mode);
			Q_free(luma_pixels);
		}

		Q_free(material_pixels);
	}

	tx->isLumaTexture = R_TextureReferenceIsValid(tx->fb_texturenum);

	return R_TextureReferenceIsValid(tx->gl_texturenum);
}

qbool Mod_IsTurbTextureName(model_t* model, const char* name)
{
	if (model->bspversion != HL_BSPVERSION && name[0] == '*') {
		return true;
	}

	return (model->bspversion == HL_BSPVERSION && name[0] == '!');
}

qbool Mod_IsSkyTextureName(model_t* model, const char* name)
{
	return (name[0] == 's' && name[1] == 'k' && name[2] == 'y');
}

qbool Mod_IsAlphaTextureName(model_t* model, const char* name)
{
	return (name[0] == '{');
}
