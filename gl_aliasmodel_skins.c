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
#include "gl_local.h"
#include "rulesets.h"

static void Mod_FloodFillSkin(byte *skin, int skinwidth, int skinheight);

//byte player_8bit_texels[320 * 200];
byte player_8bit_texels[256*256]; // Workaround for new player model, isn't proper for "real" quake skins

static texture_ref Mod_LoadExternalSkin(model_t* loadmodel, char *identifier, texture_ref *fb_texnum)
{
	char loadpath[64] = {0};
	GLuint texmode = 0;
	texture_ref texnum;
	qbool luma_allowed = Ruleset_IsLumaAllowed(loadmodel);

	GL_TextureReferenceInvalidate(texnum);
	GL_TextureReferenceInvalidate(*fb_texnum);

	if (RuleSets_DisallowExternalTexture(loadmodel)) {
		return texnum;
	}

	if (!(loadmodel->modhint & MOD_VMODEL) || gl_mipmap_viewmodels.integer) {
		texmode |= TEX_MIPMAP;
	}
	if (!gl_scaleModelTextures.value) {
		texmode |= TEX_NOSCALE;
	}

	// try "textures/models/..." path
	snprintf (loadpath, sizeof(loadpath), "textures/models/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (GL_TextureReferenceIsValid(texnum)) {
		if (luma_allowed) {
			// not a luma actually, but which suffix use then? _fb or what?
			snprintf (loadpath, sizeof(loadpath), "textures/models/%s_luma", identifier);
			*fb_texnum = GL_LoadTextureImage(loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);
		}

		return texnum;
	}

	// try "textures/..." path
	snprintf (loadpath, sizeof(loadpath), "textures/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	if (GL_TextureReferenceIsValid(texnum)) {
		// not a luma actually, but which suffix use then? _fb or what?
		if (luma_allowed) {
			snprintf (loadpath, sizeof(loadpath), "textures/%s_luma", identifier);
			*fb_texnum = GL_LoadTextureImage(loadpath, va("@fb_%s", identifier), 0, 0, texmode | TEX_FULLBRIGHT | TEX_ALPHA | TEX_LUMA);
		}

		return texnum;
	}

	return texnum; // we failed miserable
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
		Host_Error("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);
	}

	s = pheader->skinwidth * pheader->skinheight;

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	if (!(loadmodel->modhint & MOD_VMODEL) || gl_mipmap_viewmodels.integer) {
		texmode = TEX_MIPMAP;
	}
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) {
		texmode |= TEX_NOSCALE;
	}

	for (i = 0; i < numskins; i++) {
		if (pskintype->type == ALIAS_SKIN_SINGLE) {
			Mod_FloodFillSkin(skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (loadmodel->modhint == MOD_PLAYER) {
				if (s > sizeof(player_8bit_texels)) {
					Host_Error("Mod_LoadAllSkins: Player skin too large");
				}
				memcpy(player_8bit_texels, (byte *)(pskintype + 1), s);
			}

			snprintf(identifier, sizeof(identifier), "%s_%i", basename, i);

			GL_TextureReferenceInvalidate(gl_texnum);
			GL_TextureReferenceInvalidate(fb_texnum);

			gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
			if (!GL_TextureReferenceIsValid(gl_texnum)) {
				gl_texnum = GL_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1), pheader->skinwidth * pheader->skinheight)) {
					fb_texnum = GL_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype + 1), texmode | TEX_FULLBRIGHT, 1);
				}
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;

			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;

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

				GL_TextureReferenceInvalidate(gl_texnum);
				GL_TextureReferenceInvalidate(fb_texnum);

				gl_texnum = Mod_LoadExternalSkin(loadmodel, identifier, &fb_texnum);
				if (!GL_TextureReferenceIsValid(gl_texnum)) {
					gl_texnum = GL_LoadTexture(identifier, pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode, 1);

					if (Img_HasFullbrights((byte *)(pskintype), pheader->skinwidth*pheader->skinheight)) {
						fb_texnum = GL_LoadTexture(va("@fb_%s", identifier), pheader->skinwidth, pheader->skinheight, (byte *)(pskintype), texmode | TEX_FULLBRIGHT, 1);
					}
				}

				pheader->gl_texturenum[i][j & 3] = gl_texnum;
				pheader->fb_texturenum[i][j & 3] = fb_texnum;

				pskintype = (daliasskintype_t *)((byte *)(pskintype)+s);
			}

			for (k = j; j < 4; j++) {
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
				pheader->fb_texturenum[i][j & 3] = pheader->fb_texturenum[i][j - k];
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
