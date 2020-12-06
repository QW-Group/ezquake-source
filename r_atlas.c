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
#include "tr_types.h"
#include "r_texture.h"
#include "r_local.h"
#include "r_trace.h"
#include "r_renderer.h"

#define MAXIMUM_ATLAS_TEXTURE_WIDTH  4096
#define MAXIMUM_ATLAS_TEXTURE_HEIGHT 4096

typedef struct deleteable_texture_s {
	texture_ref original;
	qbool moved_to_atlas;
} deleteable_texture_t;

static byte* atlas_texels;
static byte* buffer;
static byte* prev_atlas_texels;
static deleteable_texture_t atlas_deletable_textures[WADPIC_PIC_COUNT + NUMCROSSHAIRS + 2 + MAX_CHARSETS];
static int atlas_allocated[MAXIMUM_ATLAS_TEXTURE_WIDTH];
static int atlas_delete_count = 0;
static int atlas_texture_width = 0;
static int atlas_texture_height = 0;
static int atlas_block_width = 0;
static int atlas_block_height = 0;
static int atlas_chunk_size = 64;
static qbool atlas_dirty;
#define ATLAS_SIZE_IN_BYTES (atlas_texture_width * atlas_texture_height * 4)

static cachepic_node_t wadpics[WADPIC_PIC_COUNT];
static cachepic_node_t charsetpics[MAX_CHARSETS * 256];
static cachepic_node_t crosshairpics[NUMCROSSHAIRS + 2];
#ifdef EZ_FREETYPE_SUPPORT
static cachepic_node_t fontpics[MAX_CHARSETS * 256];
#endif

static float solid_s;
static float solid_t;
static texture_ref atlas_texnum;
static texture_ref solid_texnum;
static qbool atlas_refresh = false;

static void AddTextureToDeleteList(texture_ref tex)
{
	if (R_TextureReferenceIsValid(tex) && atlas_delete_count < sizeof(atlas_deletable_textures) / sizeof(atlas_deletable_textures[0])) {
		atlas_deletable_textures[atlas_delete_count].original = tex;
		atlas_deletable_textures[atlas_delete_count].moved_to_atlas = false;
		R_TraceAPI("[atlas] adding texture to delete list: %d [%s], pos %d", tex.index, R_TextureIdentifier(tex), atlas_delete_count);
		atlas_delete_count++;
	}
	else if (R_TextureReferenceIsValid(tex)) {
		R_TraceAPI("[atlas] !! attempted to add invalid texture reference to delete list");
	}
	else {
		R_TraceAPI("[atlas] !! failed to add texture to delete list (overflow) %d [%s]", tex.index, R_TextureIdentifier(tex));
	}
}

static void AddToDeleteList(mpic_t* src)
{
	if (!R_TextureReferenceEqual(atlas_texnum, src->texnum) && src->sl == 0 && src->tl == 0 && src->th == 1 && src->sh == 1) {
		AddTextureToDeleteList(src->texnum);
	}
	else if (!R_TextureReferenceEqual(atlas_texnum, src->texnum)) {
		R_TraceAPI("[atlas] !! not adding %d [%s] to delete list (incomplete texture): %.2f %.2f %.2f %.2f", src->texnum.index, R_TextureIdentifier(src->texnum), src->sl, src->sh, src->tl, src->th);
	}
}

static void ConfirmDeleteTexture(texture_ref tex)
{
	int i;
	for (i = 0; i < atlas_delete_count; ++i) {
		if (R_TextureReferenceEqual(atlas_deletable_textures[i].original, tex)) {
			atlas_deletable_textures[i].moved_to_atlas = true;
			R_TraceAPI("[atlas] marking deletable %d (%d/%s) as moved to atlas", i, atlas_deletable_textures[i].original.index, R_TextureIdentifier(atlas_deletable_textures[i].original));
		}
	}
}

static void DeleteOldTextures(void)
{
	int i;

	R_TraceEnterFunctionRegion;
	for (i = 0; i < atlas_delete_count; ++i) {
		if (atlas_deletable_textures[i].moved_to_atlas) {
			R_TraceAPI("[atlas] deleting %d (%d/%s)", i, atlas_deletable_textures[i].original.index, R_TextureIdentifier(atlas_deletable_textures[i].original));
			R_DeleteTexture(&atlas_deletable_textures[i].original);
		}
	}
	R_TraceLeaveFunctionRegion;
}

void Atlas_SolidTextureCoordinates(texture_ref* ref, float* s, float* t)
{
	if (ref) {
		*ref = solid_texnum;
	}
	*s = solid_s;
	*t = solid_t;
}

// Returns false if allocation failed.
static qbool Atlas_AllocBlock(int w, int h, int *x, int *y)
{
	int i, j, best, best2;

	w = (w + (atlas_chunk_size - 1)) / atlas_chunk_size;
	h = (h + (atlas_chunk_size - 1)) / atlas_chunk_size;
	best = atlas_block_height;

	for (i = 0; i < atlas_block_width - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (atlas_allocated[i + j] >= best) {
				break;
			}
			if (atlas_allocated[i + j] > best2) {
				best2 = atlas_allocated[i + j];
			}
		}

		if (j == w) {
			// This is a valid spot.
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > atlas_block_height) {
		*x = *y = 0;
		return false;
	}

	for (i = 0; i < w; i++) {
		atlas_allocated[*x + i] = best + h;
	}

	atlas_dirty = true;
	*x *= atlas_chunk_size;
	*y *= atlas_chunk_size;

	return true;
}

static void CachePics_AllocateSolidTexture(void)
{
	int x_pos, y_pos, y;

	if (!Atlas_AllocBlock(atlas_chunk_size, atlas_chunk_size, &x_pos, &y_pos)) {
		solid_texnum = solidwhite_texture;
		solid_s = 0.5;
		solid_t = 0.5;
		return;
	}

	for (y = 0; y < atlas_chunk_size; ++y) {
		memset(atlas_texels + (x_pos + (y_pos + y) * atlas_texture_width) * 4, 0xFF, atlas_chunk_size * 4);
	}

	solid_s = (x_pos + atlas_chunk_size / 2) * 1.0f / atlas_texture_width;
	solid_t = (y_pos + atlas_chunk_size / 2) * 1.0f / atlas_texture_height;
	solid_texnum = atlas_texnum;
}

static void CachePics_CopyToBuffer(mpic_t* pic, int x_pos, int y_pos, int new_texture_width, const byte* input, byte* output)
{
	int texWidth = R_TextureWidth(pic->texnum);
	int texHeight = R_TextureHeight(pic->texnum);
	int height = (pic->th - pic->tl) * texHeight;
	int width = (pic->sh - pic->sl) * texWidth;
	int yOffset;

	for (yOffset = 0; yOffset < height; ++yOffset) {
		int y = y_pos + yOffset;
		int base = (x_pos + y * new_texture_width) * 4;
		int srcBase = (pic->sl * texWidth + (yOffset + pic->tl * texHeight) * texWidth) * 4;

		memcpy(output + base, input + srcBase, 4 * width);
	}
}

static int CachePics_AddToAtlas(mpic_t* pic)
{
	int width = pic->width, height = pic->height;
	int texWidth = 0, texHeight = 0;
	int x_pos, y_pos;
	int padding = 1;

	// Find size of the source
	texWidth = R_TextureWidth(pic->texnum);
	texHeight = R_TextureHeight(pic->texnum);

	width = (pic->sh - pic->sl) * texWidth;
	height = (pic->th - pic->tl) * texHeight;

	if (width > atlas_texture_width || height > atlas_texture_height) {
		return -1;
	}

	// Allocate space in an atlas texture
	if (Atlas_AllocBlock(width + (width == atlas_texture_width ? 0 : padding), height + (height == atlas_texture_height ? 0 : padding), &x_pos, &y_pos)) {
		byte* input_image = NULL;

		// Copy texture image
		if (R_TextureReferenceEqual(pic->texnum, atlas_texnum)) {
			input_image = prev_atlas_texels;
		}
		else {
			renderer.TextureGet(pic->texnum, ATLAS_SIZE_IN_BYTES, buffer, 4);

			input_image = buffer;
		}

		CachePics_CopyToBuffer(pic, x_pos, y_pos, atlas_texture_width, input_image, atlas_texels);

		R_TraceAPI("  moved to atlas: %d,%d => %d,%d", x_pos, y_pos, x_pos + width, y_pos + height);
		pic->sl = (x_pos) / (float)atlas_texture_width;
		pic->sh = (x_pos + width) / (float)atlas_texture_width;
		pic->tl = (y_pos) / (float)atlas_texture_height;
		pic->th = (y_pos + height) / (float)atlas_texture_height;
		pic->texnum = atlas_texnum;

		return 0;
	}
	else if (R_TextureReferenceEqual(atlas_texnum, pic->texnum)) {
		// Was on texture but no longer fits - need to create new texture
		CachePics_CopyToBuffer(pic, 0, 0, width, prev_atlas_texels, buffer);
		R_TraceAPI("  !moved from atlas: %d,%d => %d,%d", x_pos, y_pos, x_pos + width, y_pos + height);

		pic->tl = pic->sl = 0;
		pic->th = pic->sh = 1;
		pic->texnum = R_LoadTexturePixels(buffer, "", width, height, TEX_ALPHA);
	}
	else {
		R_TraceAPI("  !unable to move to atlas");
	}

	return -1;
}

void CachePics_AtlasFrame(void)
{
	// cls.state != active should be safe, vid_restart builds atlas directly via GFX_Init()
	if (atlas_refresh && cls.state != ca_active) {
		CachePics_CreateAtlas();
	}
}

void CachePics_AtlasUpload(void)
{
	if (atlas_dirty) {
		R_TraceEnterFunctionRegion;
		atlas_texnum = R_LoadTexture("cachepics:atlas", atlas_texture_width, atlas_texture_height, atlas_texels, TEX_ALPHA | TEX_NOSCALE, 4);
		renderer.TextureSetFiltering(atlas_texnum, texture_minification_linear, texture_magnification_linear);
		renderer.TextureWrapModeClamp(atlas_texnum);
		R_TraceLeaveFunctionRegion;
	}
	atlas_dirty = false;
}

void CachePics_Init(void)
{
	atlas_dirty = true;
	atlas_texture_width = atlas_texture_height = min(glConfig.gl_max_size_default, MAXIMUM_ATLAS_TEXTURE_WIDTH);

	CachePics_AtlasUpload();
}

static void CachePics_InsertBySize(cachepic_node_t** sized_list, cachepic_node_t* node)
{
	int size_node;
	cachepic_node_t* current = *sized_list;
	int size_this;

	node->width = R_TextureWidth(node->data.pic->texnum);
	node->height = R_TextureHeight(node->data.pic->texnum);

	node->width *= (node->data.pic->sh - node->data.pic->sl);
	node->height *= (node->data.pic->th - node->data.pic->tl);

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
	int texWidth, texHeight;
	float sRatio = (ibar->sh - ibar->sl) / (float)ibar->width;
	float tRatio = (ibar->th - ibar->tl) / (float)ibar->height;
	byte* source;
	byte* target;
	int realwidth, realheight;
	float newsh, newsl;
	float newth, newtl;

	// Find size of the source
	texWidth = R_TextureWidth(ibar->texnum);
	texHeight = R_TextureHeight(ibar->texnum);

	source = Q_malloc(texWidth * texHeight * 4);
	target = Q_malloc(texWidth * texHeight * 4);

	renderer.TextureGet(ibar->texnum, texWidth * texHeight * 4, source, 4);
	for (i = WADPIC_SB_IBAR_AMMO1; i <= WADPIC_SB_IBAR_AMMO4; ++i) {
		int num = i - WADPIC_SB_IBAR_AMMO1;
		int y;
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

		snprintf(name, sizeof(name), "hud_ammo_%d", i - WADPIC_SB_IBAR_AMMO1);

		for (y = 0; y < realheight; ++y) {
			memcpy(target + (y * realwidth) * 4, &source[(x_src + (y_src + y) * texWidth) * 4], realwidth * 4);
		}

		targPic = wad_pictures[i].pic = &sb_ib_ammo[num];
		targPic->texnum = R_LoadTexture(name, realwidth, realheight, target, TEX_NOCOMPRESS | TEX_ALPHA | TEX_NOSCALE | TEX_NO_TEXTUREMODE, 4);
		targPic->sl = 0;
		targPic->sh = 1;
		targPic->tl = 0;
		targPic->th = 1;
		targPic->width = 42;
		targPic->height = 11;
	}

	Q_free(target);
	Q_free(source);

	AddToDeleteList(ibar);
}

static void DeleteCharsetTextures(void)
{
	int i;

	R_TraceEnterFunctionRegion;
	for (i = 0; i < MAX_CHARSETS; ++i) {
		charset_t* charset = &char_textures[i];
		int j;
		qbool all_on_atlas = true;

		if (!R_TextureReferenceIsValid(charset->master)) {
			continue;
		}

		R_TraceAPI("Checking normal charset %03d is on atlas...", i);
		for (j = 0; j < 256 && all_on_atlas; ++j) {
			texture_ref glyph_tex = charset->glyphs[j].texnum;

			all_on_atlas &= (!R_TextureReferenceIsValid(glyph_tex) || R_TextureReferenceEqual(glyph_tex, atlas_texnum));
		}
		if (all_on_atlas && R_TextureReferenceIsValid(charset->master)) {
			R_TraceAPI("- all on atlas, deleting...", i);
			R_DeleteTexture(&charset->master);
			R_TextureReferenceInvalidate(charset->master);
		}
		else {
			R_TraceAPI("- some glyphs not on atlas", i);
		}
	}

#ifdef EZ_FREETYPE_SUPPORT
	for (i = 0; i < MAX_CHARSETS; ++i) {
		extern charset_t proportional_fonts[MAX_CHARSETS];
		charset_t* charset = &proportional_fonts[i];
		int j;
		qbool all_on_atlas = true;

		if (!R_TextureReferenceIsValid(charset->master)) {
			continue;
		}

		R_TraceAPI("Checking proportional charset %03d is on atlas...", i);
		for (j = 0; j < 256 && all_on_atlas; ++j) {
			all_on_atlas &= R_TextureReferenceEqual(charset->glyphs[j].texnum, atlas_texnum);
		}
		if (all_on_atlas && R_TextureReferenceIsValid(charset->master)) {
			R_TraceAPI("- all on atlas, deleting...", i);
			R_DeleteTexture(&charset->master);
		}
		else {
			R_TraceAPI("- some glyphs not on atlas", i);
		}
	}
#endif
	R_TraceLeaveFunctionRegion;
}

void CachePics_CreateAtlas(void)
{
	cachepic_node_t* sized_list = NULL;
	cachepic_node_t* cur;
	cachepic_node_t simple_items[MOD_NUMBER_HINTS * MAX_SIMPLE_TEXTURES];
	int i, j;
	double start_time = Sys_DoubleTime();

	if (COM_CheckParm(cmdline_param_client_noatlas)) {
		atlas_refresh = false;
		return;
	}

	R_TraceEnterFunctionRegion;

	// Delete old atlas textures
	atlas_texels = Q_malloc(ATLAS_SIZE_IN_BYTES);
	prev_atlas_texels = Q_malloc(ATLAS_SIZE_IN_BYTES);
	if (R_TextureReferenceIsValid(atlas_texnum)) {
		renderer.TextureGet(atlas_texnum, ATLAS_SIZE_IN_BYTES, prev_atlas_texels, 4);
	}
	memset(atlas_allocated, 0, sizeof(atlas_allocated));
	memset(wadpics, 0, sizeof(wadpics));
	atlas_delete_count = 0;

	atlas_chunk_size = atlas_texture_width >= 4096 ? 64 : atlas_texture_width >= 2048 ? 32 : 16;
	atlas_block_width = atlas_texture_width / atlas_chunk_size;
	atlas_block_height = atlas_texture_height / atlas_chunk_size;

	// Copy wadpic data over
	for (i = 0; i < WADPIC_PIC_COUNT; ++i) {
		extern wadpic_t wad_pictures[WADPIC_PIC_COUNT];
		mpic_t* src = wad_pictures[i].pic;

		// This tiles, so don't include in atlas
		if (i == WADPIC_BACKTILE) {
			continue;
		}

		if (src) {
			wadpics[i].data.pic = src;

			if (i != WADPIC_SB_IBAR) {
				CachePics_InsertBySize(&sized_list, &wadpics[i]);

				AddToDeleteList(src);
			}
		}
	}

	for (i = 0; i < MAX_CHARSETS; ++i) {
		charset_t* charset = &char_textures[i];

		for (j = 0; j < 256; ++j) {
			if (R_TextureReferenceIsValid(charset->glyphs[j].texnum)) {
				charsetpics[i * 256 + j].data.pic = &charset->glyphs[j];

				CachePics_InsertBySize(&sized_list, &charsetpics[i * 256 + j]);
			}
		}
	}

#ifdef EZ_FREETYPE_SUPPORT
	for (i = 0; i < MAX_CHARSETS; ++i) {
		extern charset_t proportional_fonts[MAX_CHARSETS];
		charset_t* charset = &proportional_fonts[i];

		for (j = 0; j < 256; ++j) {
			if (R_TextureReferenceIsValid(charset->glyphs[j].texnum)) {
				fontpics[i * 256 + j].data.pic = &charset->glyphs[j];

				CachePics_InsertBySize(&sized_list, &fontpics[i * 256 + j]);
			}
		}
	}
#endif

	// Copy crosshairs
	{
		extern mpic_t crosshairtexture_txt;
		extern mpic_t crosshairpic;
		extern mpic_t crosshairs_builtin[NUMCROSSHAIRS];

		if (R_TextureReferenceIsValid(crosshairtexture_txt.texnum)) {
			crosshairpics[0].data.pic = &crosshairtexture_txt;
			CachePics_InsertBySize(&sized_list, &crosshairpics[0]);

			AddToDeleteList(&crosshairtexture_txt);
		}

		if (R_TextureReferenceIsValid(crosshairpic.texnum)) {
			crosshairpics[1].data.pic = &crosshairpic;
			CachePics_InsertBySize(&sized_list, &crosshairpics[1]);

			AddToDeleteList(&crosshairpic);
		}

		for (i = 0; i < NUMCROSSHAIRS; ++i) {
			if (R_TextureReferenceIsValid(crosshairs_builtin[i].texnum)) {
				crosshairpics[i + 2].data.pic = &crosshairs_builtin[i];
				CachePics_InsertBySize(&sized_list, &crosshairpics[i + 2]);

				AddToDeleteList(&crosshairs_builtin[i]);
			}
		}
	}

	// Add simple item textures (meag: these should be very low priority)
	for (i = 0; i < MOD_NUMBER_HINTS; ++i) {
		for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
			mpic_t* pic = Mod_SimpleTextureForHint(i, j);
			cachepic_node_t* node = &simple_items[i * MAX_SIMPLE_TEXTURES + j];

			node->data.pic = pic;

			if (pic && R_TextureReferenceIsValid(pic->texnum)) {
				CachePics_InsertBySize(&sized_list, node);

				// Don't delete these, used for 3d sprites
			}
		}
	}

	// Add cached picture images
	for (i = 0; i < CACHED_PICS_HDSIZE; ++i) {
		extern cachepic_node_t *cachepics[CACHED_PICS_HDSIZE];
		cachepic_node_t *cur;

		for (cur = cachepics[i]; cur; cur = cur->next) {
			if (!Draw_IsConsoleBackground(cur->data.pic) && !Draw_KeepOffAtlas(cur->data.name)) {
				CachePics_InsertBySize(&sized_list, cur);

				AddToDeleteList(cur->data.pic);
			}
		}
	}

	// Actually copy to the atlas texture
	buffer = Q_malloc(ATLAS_SIZE_IN_BYTES);
	for (cur = sized_list; cur; cur = cur->size_order) {
		texture_ref original = cur->data.pic->texnum;
		R_TraceAPI("[atlas] attempting to add %d/%s to atlas", original.index, R_TextureIdentifier(original));
		if (CachePics_AddToAtlas(cur->data.pic) >= 0) {
			ConfirmDeleteTexture(original);
		}
	}
	CachePics_AllocateSolidTexture();

	// Upload atlas texture
	CachePics_AtlasUpload();

	DeleteOldTextures();
	DeleteCharsetTextures();

	Q_free(atlas_texels);
	Q_free(prev_atlas_texels);
	Q_free(buffer);
	Con_DPrintf("Atlas build time: %f\n", Sys_DoubleTime() - start_time);

	// Make sure we don't reference any old textures
	R_EmptyImageQueue();

	R_TraceLeaveFunctionRegion;

	atlas_refresh = false;
}

void CachePics_MarkAtlasDirty(void)
{
	atlas_refresh = true;
}
