/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef EZQUAKE_R_TEXTURE_H
#define EZQUAKE_R_TEXTURE_H

#define TEX_COMPLAIN        (1<<0)  // shout if texture missing while loading
#define TEX_MIPMAP          (1<<1)  // use mipmaps generation while loading texture
#define TEX_ALPHA           (1<<2)  // use transparency in texture
// Note: if (TEX_LUMA & TEX_FULLBRIGHT) specified, colours close to black (determined by gl_luma_level) will be turned transparent
#define TEX_LUMA            (1<<3)  // do not apply gamma adjustment to texture when loading
#define TEX_FULLBRIGHT      (1<<4)  // make all non-fullbright colours transparent (8-bit only).
#define TEX_NOSCALE         (1<<5)  // do no use gl_max_size or gl_picmap variables while loading texture
#define TEX_BRIGHTEN        (1<<6)  // use brighter 8bit => 24bit conversion during load
#define TEX_NOCOMPRESS      (1<<7)  // do not use texture compression extension
#define TEX_NO_PCX          (1<<8)  // do not load pcx images
#define TEX_NO_TEXTUREMODE  (1<<9)  // ignore gl_texturemode* changes for texture
#define TEX_PREMUL_ALPHA    (1<<10) // pre-multiply alpha
#define TEX_ZERO_ALPHA      (1<<11) // after pre-multiplying alpha, set alpha to 0 (additive blending)
#define TEX_MERGED_LUMA     (1<<12) // alpha channel signifies luma %
#define TEX_FLOAT           (1<<13) // floating point texture format
#define TEX_VIEWMODEL       (1<<14) // use viewmodel texture mode, not normal

#define MAX_GLTEXTURES 8192	//dimman: old value 1024 isn't enough when using high framecount sprites (according to Spike)
#define MAX_CHARSETS 256
#define MAX_USER_CHARSETS 0xE0

#define PRIVATE_USE_TRACKERIMAGES_CHARSET 0xE0
// reservations:
//   0xE000 - 0xE0FF = Tracker weapon images

typedef struct charset_s {
	mpic_t glyphs[256];
	texture_ref master;
	float custom_scale_x;
	float custom_scale_y;
} charset_t;

extern charset_t char_textures[MAX_CHARSETS];

/*
typedef struct texture_api_s {
	texture_ref (*Load)(const char *identifier, int width, int height, byte *data, int mode, int bpp);
	texture_ref (*LoadPic)(const char *name, mpic_t *pic, byte *data);
	texture_ref (*LoadFromPixels)(byte *data, const char *identifier, int width, int height, int mode);
	texture_ref (*LoadFromFile)(const char *filename, const char *identifier, int matchwidth, int matchheight, int mode);
	texture_ref (*CreateArray)(const char* identifier, int width, int height, int* depth, int mode, int minimum_depth);
	texture_ref (*CreateCubeMap)(const char* identifier, int width, int height, int mode);
	void (*DeleteArray)(texture_ref* texture);
	void (*DeleteCubeMap)(texture_ref* texture);
	void (*Delete)(texture_ref* texture);
	void (*DeleteAll)(void);

	qbool (*SameSize)(texture_ref tex1, texture_ref tex2);

	int (*Width)(texture_ref ref);
	int (*Height)(texture_ref ref);
	int (*Depth)(texture_ref ref);
	void (*GenerateMipmaps)(texture_ref ref);

	void (*InvalidateAll)(void);
	void (*CreateInternal)(texture_ref* reference, int width, int height, const char* name);
	void (*ReplaceSubImageRGBA)(texture_ref ref, int offsetx, int offsety, int width, int height, byte* buffer);

	void (*R_TextureWrapModeClamp)(texture_ref tex);

#ifdef WITH_RENDERING_TRACE
	const char* (*Identifier)(texture_ref ref);
#endif

} texture_api_t;

extern texture_api_t textures;
*/

void R_Texture_Init(void);

void R_TextureAnisotropyChanged(texture_ref tex, qbool mipmap, qbool viewmodel);

mpic_t* R_LoadPicImage(const char *filename, char *id, int matchwidth, int matchheight, int mode);
byte* R_LoadImagePixels(const char *filename, int matchwidth, int matchheight, int mode, int *real_width, int *real_height);
qbool R_LoadCharsetImage(char *filename, char *identifier, int flags, charset_t* pic);
void R_ImagePreMultiplyAlpha(byte* image, int width, int height, qbool zero);

texture_ref R_LoadTexture(const char *identifier, int width, int height, byte *data, int mode, int bpp);
texture_ref R_LoadPicTexture(const char *name, mpic_t *pic, byte *data);
texture_ref R_LoadTexturePixels(byte *data, const char *identifier, int width, int height, int mode);
texture_ref R_LoadTextureImage(const char *filename, const char *identifier, int matchwidth, int matchheight, int mode);
texture_ref R_CreateTextureArray(const char* identifier, int width, int height, int depth, int mode);
texture_ref R_CreateCubeMap(const char* identifier, int width, int height, int mode);
void R_DeleteTextureArray(texture_ref* texture);
void R_DeleteCubeMap(texture_ref* texture);
void R_DeleteTexture(texture_ref* texture);
void R_DeleteTextures(void);

qbool R_TexturesAreSameSize(texture_ref tex1, texture_ref tex2);
qbool R_TextureValid(texture_ref ref);
int R_TextureWidth(texture_ref ref);
int R_TextureHeight(texture_ref ref);
int R_TextureDepth(texture_ref ref);
r_texture_type_id R_TextureType(texture_ref ref);
void R_GenerateMipmapsIfNeeded(texture_ref ref);

void R_TexturesInvalidateAllReferences(void);

qbool R_ExternalTexturesEnabled(qbool worldmodel);

void R_SetNonPowerOfTwoSupport(qbool supported);
void R_TextureSizeRoundUp(int orig_width, int orig_height, int* width, int* height);

#ifdef WITH_RENDERING_TRACE
const char* R_TextureIdentifier(texture_ref ref);
#endif

extern cvar_t gl_max_size, gl_scaleModelTextures, gl_scaleTurbTextures, gl_scaleAlphaTextures, gl_scaleModelSimpleTextures, gl_miptexLevel, gl_scaleskytextures;
extern cvar_t gl_no24bit;
extern texture_ref underwatertexture, detailtexture, solidwhite_texture, solidblack_texture, transparent_texture;

void R_ClearModelTextureData(void);

void R_AllocateTextureReferences(r_texture_type_id type_id, int width, int height, int mode, int number, texture_ref* references);
void R_TextureRescaleOverlay(byte** overlay_pixels, int* overlay_width, int* overlay_height, int underlying_width, int underlying_height);
int R_TextureCount(void);
void R_TextureFindIdentifierByReference(unsigned int ref, char* label, int labelsize);
int R_TextureGetFlag(texture_ref ref);
void R_TextureSetFlag(texture_ref ref, int mode);

void R_SetTextureArraySize(texture_ref tex, int width, int height, int depth, int bpp);

#endif	// EZQUAKE_R_TEXTURE_H
