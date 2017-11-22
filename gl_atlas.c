/*
Module for creating 2d atlas for UI elements

Copyright (C) 2011 ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quakedef.h"
#include "common_draw.h"
#include "gl_model.h"
#include "gl_local.h"

#define ATLAS_COUNT     4
#define ATLAS_WIDTH  2048
#define ATLAS_HEIGHT 2048

static cachepic_node_t wadpics[WADPIC_PIC_COUNT];
static cachepic_node_t charsetpics[MAX_CHARSETS];
static cachepic_node_t crosshairpics[NUMCROSSHAIRS + 2];

static int  atlas_allocated[ATLAS_COUNT][ATLAS_WIDTH];
static byte atlas_texels[ATLAS_COUNT][ATLAS_WIDTH * ATLAS_HEIGHT * 4];
static byte atlas_dirty = 0;
static int  atlas_texnum[ATLAS_COUNT];
static qbool atlas_refresh = false;

cvar_t gfx_atlasautoupload = { "gfx_atlasautoupload", "1" };

// Returns false if allocation failed.
static qbool CachePics_AllocBlock(int atlas_num, int w, int h, int *x, int *y)
{
	int i, j, best, best2;

	best = ATLAS_HEIGHT;

	for (i = 0; i < ATLAS_WIDTH - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (atlas_allocated[atlas_num][i + j] >= best)
				break;
			if (atlas_allocated[atlas_num][i + j] > best2)
				best2 = atlas_allocated[atlas_num][i + j];
		}

		if (j == w) {
			// This is a valid spot.
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > ATLAS_HEIGHT) {
		return false;
	}

	for (i = 0; i < w; i++) {
		atlas_allocated[atlas_num][*x + i] = best + h;
	}

	atlas_dirty |= (1 << atlas_num);

	return true;
}

int CachePics_AddToAtlas(mpic_t* pic)
{
	static char buffer[ATLAS_WIDTH * ATLAS_HEIGHT * 4];
	int width = pic->width, height = pic->height;
	int texWidth = 0, texHeight = 0;
	int i;

	// Find size of the source
	GL_Bind(pic->texnum);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);

	width = (pic->sh - pic->sl) * texWidth;
	height = (pic->th - pic->tl) * texHeight;

	if (texWidth > ATLAS_WIDTH || texHeight > ATLAS_HEIGHT) {
		return -1;
	}
	Con_Printf(" > size = %d x %d = %d\n", width, height, width * height);

	// Allocate space in an atlas texture
	for (i = 0; i < ATLAS_COUNT; ++i) {
		int x_pos, y_pos;
		int padding = 1;

		if (CachePics_AllocBlock(i, width + (width == ATLAS_WIDTH ? 0 : padding), height + (height == ATLAS_HEIGHT ? 0 : padding), &x_pos, &y_pos)) {
			int xOffset, yOffset;

			// Copy texture image
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

			for (yOffset = 0; yOffset < height; ++yOffset) {
				for (xOffset = 0; xOffset < width; ++xOffset) {
					int y = y_pos + yOffset;
					int x = x_pos + xOffset;
					int base = (x + y * ATLAS_WIDTH) * 4;
					int srcBase = (xOffset + pic->sl * texWidth + (yOffset + pic->tl * texHeight) * texWidth) * 4;

					atlas_texels[i][base] = buffer[srcBase];
					atlas_texels[i][base + 1] = buffer[srcBase + 1];
					atlas_texels[i][base + 2] = buffer[srcBase + 2];
					atlas_texels[i][base + 3] = buffer[srcBase + 3];
				}
			}

			if (false) {
				pic->sl = (x_pos + 0.25) / (float)ATLAS_WIDTH;
				pic->sh = (x_pos + width - 0.25) / (float)ATLAS_WIDTH;
				pic->tl = (y_pos + 0.25) / (float)ATLAS_HEIGHT;
				pic->th = (y_pos + height - 0.25) / (float)ATLAS_HEIGHT;
				pic->texnum = atlas_texnum[i];
			}
			else if (true) {
				pic->sl = (x_pos) / (float)ATLAS_WIDTH;
				pic->sh = (x_pos + width) / (float)ATLAS_WIDTH;
				pic->tl = (y_pos) / (float)ATLAS_HEIGHT;
				pic->th = (y_pos + height) / (float)ATLAS_HEIGHT;
				pic->texnum = atlas_texnum[i];
			}

			return i;
		}
		else {
			Con_Printf("*** FAILED TO PLACE: %d (%d x %d) ***\n", pic->texnum, width, height);
		}
	}

	return -1;
}

void CachePics_AtlasFrame(void)
{
	if (atlas_refresh) {
		CachePics_CreateAtlas();
	}
}

void CachePics_AtlasUpload(void)
{
	int i;

	for (i = 0; i < ATLAS_COUNT; ++i) {
		if (atlas_dirty & (1 << i)) {
			char id[64];
			// generate id
			snprintf(id, sizeof(id), "cachepics:%d", i);
			// 
			atlas_texnum[i] = GL_LoadTexture(id, ATLAS_WIDTH, ATLAS_HEIGHT, atlas_texels[i], TEX_ALPHA | TEX_NOSCALE, 4);
		}
	}
	atlas_dirty = 0;
}

void CachePics_Init(void)
{
	atlas_dirty = ~0;

	CachePics_AtlasUpload();

	Cvar_Register(&gfx_atlasautoupload);
	Cmd_AddCommand("gfx_atlasupload", CachePics_CreateAtlas);
}

void CachePics_InsertBySize(cachepic_node_t** sized_list, cachepic_node_t* node)
{
	int size_node;
	cachepic_node_t* current = *sized_list;
	int size_this;

	GL_Bind(node->data.pic->texnum);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &node->width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &node->height);

	node->width *= (node->data.pic->sh - node->data.pic->sl);
	node->height *= (node->data.pic->th - node->data.pic->tl);

	if (node->width == 0) {
		node = node;
	}

	size_node = node->width * node->height;

	while (current) {
		size_this = current->width * current->height;
		if (size_this > size_node) {
			sized_list = &current->size_order;
			current = *sized_list;
			continue;
		}

		break;
	}

	node->size_order = current;
	*sized_list = node;
}

void CachePics_LoadAmmoPics(mpic_t* ibar)
{
	extern mpic_t sb_ib_ammo[4];
	mpic_t* targPic;
	int i;
	GLint texWidth, texHeight;
	float sRatio = (ibar->sh - ibar->sl) / (float)ibar->width;
	float tRatio = (ibar->th - ibar->tl) / (float)ibar->height;
	byte* source;
	byte* target;
	int realwidth, realheight;
	float newsh, newsl;
	float newth, newtl;

	// Find size of the source
	GL_Bind(ibar->texnum);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);

	source = Q_malloc(texWidth * texHeight * 4);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);

	for (i = WADPIC_SB_IBAR_AMMO1; i <= WADPIC_SB_IBAR_AMMO4; ++i) {
		int num = i - WADPIC_SB_IBAR_AMMO1;
		int x, y;
		int x_src, y_src;
		char name[16];
		int xcoord = (3 + num * 48);
		int xwidth = 42;
		int ycoord = 0;
		int yheight = 11;

		newsl = ibar->sl + xcoord * sRatio;
		newsh = newsl + xwidth * sRatio;
		newtl = ibar->tl + ycoord * tRatio;
		newth = newtl + yheight * tRatio;

		x_src = texWidth * newsl;
		y_src = texHeight * newtl;

		realwidth = (newsh - newsl) * texWidth;
		realheight = (newth - newtl) * texHeight;
		target = Q_malloc(realwidth * realheight * 4);

		snprintf(name, sizeof(name), "hud_ammo_%d", i - WADPIC_SB_IBAR_AMMO1);

		memset(target, 0, realwidth * realheight * 4);
		for (x = 0; x < realwidth; ++x) {
			for (y = 0; y < realheight; ++y) {
				target[(x + y * realwidth) * 4] = source[((x_src + x) + (y_src + y) * texWidth) * 4];
				target[(x + y * realwidth) * 4 + 1] = source[((x_src + x) + (y_src + y) * texWidth) * 4 + 1];
				target[(x + y * realwidth) * 4 + 2] = source[((x_src + x) + (y_src + y) * texWidth) * 4 + 2];
				target[(x + y * realwidth) * 4 + 3] = source[((x_src + x) + (y_src + y) * texWidth) * 4 + 3];
			}
		}

		targPic = wad_pictures[i].pic = &sb_ib_ammo[num];
		targPic->texnum = GL_LoadTexture(name, realwidth, realheight, target, TEX_NOCOMPRESS | TEX_ALPHA | TEX_NOSCALE | TEX_NO_TEXTUREMODE, 4);
		targPic->sl = 0;
		targPic->sh = 1;
		targPic->tl = 0;
		targPic->th = 1;
		targPic->width = 42;
		targPic->height = 11;

		Q_free(target);
	}

	Q_free(source);
}

void CachePics_CreateAtlas(void)
{
	cachepic_node_t* sized_list = NULL;
	cachepic_node_t* cur;
	int i;
	int expected = 0, found = 0;

	// Delete old atlas textures
	memset(atlas_texels, 0, sizeof(atlas_texels));
	memset(atlas_allocated, 0, sizeof(atlas_allocated));
	memset(wadpics, 0, sizeof(wadpics));

	// Copy wadpic data over
	for (i = 0; i < WADPIC_PIC_COUNT; ++i) {
		extern wadpic_t wad_pictures[WADPIC_PIC_COUNT];

		if (wad_pictures[i].pic) {
			wadpics[i].data.pic = wad_pictures[i].pic;

			if (i != WADPIC_SB_IBAR) {
				CachePics_InsertBySize(&sized_list, &wadpics[i]);
			}
		}
	}

	// Copy text images over
	for (i = 0; i < MAX_CHARSETS; ++i) {
		extern mpic_t char_textures[MAX_CHARSETS];

		if (char_textures[i].texnum) {
			charsetpics[i].data.pic = &char_textures[i];

			CachePics_InsertBySize(&sized_list, &charsetpics[i]);
		}
	}

	// Copy crosshairs
	{
		extern mpic_t crosshairtexture_txt;
		extern mpic_t crosshairpic;
		extern mpic_t crosshairs_builtin[NUMCROSSHAIRS];

		if (crosshairtexture_txt.texnum) {
			crosshairpics[0].data.pic = &crosshairtexture_txt;
			CachePics_InsertBySize(&sized_list, &crosshairpics[0]);
		}

		if (crosshairpic.texnum) {
			crosshairpics[1].data.pic = &crosshairpic;
			CachePics_InsertBySize(&sized_list, &crosshairpics[1]);
		}

		for (i = 0; i < NUMCROSSHAIRS; ++i) {
			if (crosshairs_builtin[i].texnum) {
				crosshairpics[i + 2].data.pic = &crosshairs_builtin[i];
				CachePics_InsertBySize(&sized_list, &crosshairpics[i + 2]);
			}
		}
	}

	// Create atlas textures
	for (i = 0; i < CACHED_PICS_HDSIZE; ++i) {
		extern cachepic_node_t *cachepics[CACHED_PICS_HDSIZE];
		cachepic_node_t *cur;

		for (cur = cachepics[i]; cur; cur = cur->next) {
			CachePics_InsertBySize(&sized_list, cur);
		}
	}

	for (cur = sized_list; cur; cur = cur->size_order) {
		int old_tex = cur->data.pic->texnum;

		CachePics_AddToAtlas(cur->data.pic);
	}

	// Upload atlas textures
	CachePics_AtlasUpload();

	atlas_refresh = false;
}

void CachePics_MarkAtlasDirty(void)
{
	Con_Printf("CachePics_MarkAtlasDirty\n");
	atlas_refresh = true;
}
