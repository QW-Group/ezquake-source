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

// Alias model (.mdl) skin processing
// Most code taken from gl_rmain.c

#include "quakedef.h"
#include "gl_model.h"
#include "rulesets.h"
#include "r_texture.h"
#include "r_local.h"
#include "image.h"

static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight);

//byte player_8bit_texels[320 * 200];
byte player_8bit_texels[256*256]; // Workaround for new player model, isn't proper for "real" quake skins

void R_CompressFullbrightTexture(byte* skin_pixels, int skin_width, int skin_height, byte* fb_pixels, int fb_width, int fb_height)
{
	int x, y;

	// adjust fb-texture to add alpha (this is from (TEX_FULLBRIGHT | TEX_LUMA) processing)
	for (x = 0; x < skin_width; ++x) {
		for (y = 0; y < skin_height; ++y) {
			int base = (x + y * skin_width) * 4;
			extern cvar_t gl_wicked_luma_level;

			// For luma textures that don't have alpha component (or are just incorrect)
			if (fb_pixels[base] < gl_wicked_luma_level.integer && fb_pixels[base + 1] < gl_wicked_luma_level.integer && fb_pixels[base + 2] < gl_wicked_luma_level.integer) {
				fb_pixels[base] = fb_pixels[base + 1] = fb_pixels[base + 2] = fb_pixels[base + 3] = 0;
			}
		}
	}

	// re-scale fb texture to match underlying skin
	R_TextureRescaleOverlay(&fb_pixels, &fb_width, &fb_height, skin_width, skin_height);

	// Merge the fb texture with the original
	for (x = 0; x < skin_width; ++x) {
		for (y = 0; y < skin_height; ++y) {
			int base = (x + y * skin_width) * 4;

			skin_pixels[base + 3] = 255;
			if (fb_pixels[base + 3]) {
				float a = fb_pixels[base + 3] / 255.0f;
				int orig = skin_pixels[base];

				skin_pixels[base] = (byte)bound(0, (int)orig * (1 - a) + (int)fb_pixels[base] * a, 255);
				skin_pixels[base + 1] = (byte)bound(0, (int)orig * (1 - a) + (int)fb_pixels[base + 1] * a, 255);
				skin_pixels[base + 2] = (byte)bound(0, (int)orig * (1 - a) + (int)fb_pixels[base + 2] * a, 255);
				skin_pixels[base + 3] = (1 - a) * 255;
			}
		}
	}

	Q_free(fb_pixels);
}

static texture_ref Mod_LoadExternalSkin(model_t* loadmodel, char *identifier, texture_ref *fb_texnum)
{
	char loadpath[MAX_OSPATH] = {0};
	int texmode = 0, luma_texmode;
	texture_ref texnum;
	qbool luma_allowed = Ruleset_IsLumaAllowed(loadmodel);
	int skin_width = 0, skin_height = 0;
	int luma_width = 0, luma_height = 0;
	byte* skin_pixels = NULL;
	byte* luma_pixels = NULL;

	R_TextureReferenceInvalidate(texnum);
	R_TextureReferenceInvalidate(*fb_texnum);

	if (RuleSets_DisallowExternalTexture(loadmodel)) {
		return texnum;
	}

	texmode |= TEX_MIPMAP;
	texmode |= (loadmodel->modhint == MOD_VMODEL ? TEX_VIEWMODEL : 0);
	texmode |= (!gl_scaleModelTextures.value ? TEX_NOSCALE : 0);
	luma_texmode = texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA;

	// try "textures/models/..." path
	strlcpy(loadpath, "textures/models/", sizeof(loadpath));
	strlcat(loadpath, identifier, sizeof(loadpath));
	skin_pixels = R_LoadImagePixels(loadpath, 0, 0, texmode, &skin_width, &skin_height);

	if (!skin_pixels) {
		// try "textures/..." path
		strlcpy(loadpath, "textures/", sizeof(loadpath));
		strlcat(loadpath, identifier, sizeof(loadpath));

		skin_pixels = R_LoadImagePixels(loadpath, 0, 0, texmode, &skin_width, &skin_height);
	}

	if (skin_pixels && luma_allowed) {
		// not a luma actually, but which suffix use then? _fb or what?
		strlcat(loadpath, "_luma", sizeof(loadpath));

		luma_pixels = R_LoadImagePixels(loadpath, 0, 0, luma_texmode, &luma_width, &luma_height);
	}

	if (R_CompressFullbrightTextures() && skin_pixels && luma_pixels) {
		R_CompressFullbrightTexture(skin_pixels, skin_width, skin_height, luma_pixels, luma_width, luma_height);

		texmode |= (TEX_ALPHA | TEX_MERGED_LUMA);
		luma_pixels = NULL;
	}

	if (skin_pixels) {
		texnum = R_LoadTexturePixels(skin_pixels, identifier, skin_width, skin_height, texmode);
		if (luma_pixels) {
			*fb_texnum = R_LoadTexturePixels(luma_pixels, va("@fb_%s", identifier), luma_width, luma_height, luma_texmode);
			Q_free(luma_pixels);
		}

		Q_free(skin_pixels);
	}

	return texnum;
}

void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t* pskintype)
{
	int i, j, k, s, groupskins, texmode = 0;
	texture_ref gl_texnum, fb_texnum;
	char basename[64], identifier[64];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	skin = (byte *)(pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS) {
		Host_Error("Mod_LoadAllSkins: Invalid # of skins: %d (model %s)\n", numskins, loadmodel->name);
	}

	s = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	texmode |= TEX_MIPMAP;
	texmode |= (loadmodel->modhint == MOD_VMODEL ? TEX_VIEWMODEL : 0);
	texmode |= (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) ? TEX_NOSCALE : 0;

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER) {
				if (s > sizeof(player_8bit_texels)) {
					Host_Error("Mod_LoadAllSkins: Player skin too large (model %s)", loadmodel->name);
					return NULL;
				}
				memcpy(player_8bit_texels, (byte *)(pskintype + 1), s);
			}

			snprintf(identifier, sizeof(identifier), "%s_%i", basename, i);

			R_TextureReferenceInvalidate(gl_texnum);
			R_TextureReferenceInvalidate(fb_texnum);

			gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
			if (!R_TextureReferenceIsValid(gl_texnum)) {
				gl_texnum = R_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1), pheader->skinwidth * pheader->skinheight)) {
					fb_texnum = R_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode | TEX_FULLBRIGHT, 1);
				}
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] = pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;
			pheader->glc_fb_texturenum[i][0] = pheader->glc_fb_texturenum[i][1] = pheader->glc_fb_texturenum[i][2] = pheader->glc_fb_texturenum[i][3] = fb_texnum;

			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + s);
		}
		else {
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong(pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++) {
				Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

				snprintf(identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				R_TextureReferenceInvalidate(gl_texnum);
				R_TextureReferenceInvalidate(fb_texnum);

				gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
				if (!R_TextureReferenceIsValid(gl_texnum)) {
					gl_texnum = R_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode, 1);

					if (Img_HasFullbrights((byte *)(pskintype), pheader->skinwidth*pheader->skinheight)) {
						fb_texnum = R_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode | TEX_FULLBRIGHT, 1);
					}
				}

				pheader->gl_texturenum[i][j & 3] = gl_texnum;
				pheader->glc_fb_texturenum[i][j & 3] = fb_texnum;

				pskintype = (daliasskintype_t *)((byte *)(pskintype)+s);
			}

			for (k = j; j < 4; j++) {
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
				pheader->glc_fb_texturenum[i][j & 3] = pheader->glc_fb_texturenum[i][j - k];
			}
		}
	}
	return pskintype;
}

typedef struct {
	short x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

//Fill background pixels so mipmapping doesn't have haloes - Ed
static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1) {
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i) {
			if (d_8to24table[i] == (255 << 0)) { // alpha 1.0
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255)) {
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt) {
		int x = fifo[outpt].x, y = fifo[outpt].y, fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP(-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP(1, 1, 0);
		if (y > 0)				FLOODFILL_STEP(-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP(skinwidth, 0, 1);
		skin[x + skinwidth * y] = fdc;
	}
}
