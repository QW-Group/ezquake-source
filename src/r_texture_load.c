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

#include "quakedef.h"
#include "r_texture_internal.h"
#include "r_local.h"
#include "image.h"
#include "crc.h"
#include "gl_texture.h"
#include "r_trace.h"

static void R_LoadTextureData(gltexture_t* glt, int width, int height, byte *data, int mode, int bpp);

// 
#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

static const texture_ref invalid_texture_reference = { 0 };
static qbool r_texture_support_non_power_of_two;

#define Block24BitTextures (COM_CheckParm(cmdline_param_client_no24bittextures) || gl_no24bit.integer)
#define ForceTextureReload (COM_CheckParm(cmdline_param_client_forcetexturereload))

static qbool CheckTextureLoaded(gltexture_t* current_texture)
{
	if (!ForceTextureReload) {
		if (current_texture && current_texture->pathname && !strcmp(fs_netpath, current_texture->pathname)) {
			int textureWidth, textureHeight;

			R_TextureUtil_ImageDimensionsToTexture(current_texture->image_width, current_texture->image_height, &textureWidth, &textureHeight, current_texture->texmode);

			if (current_texture->texture_width == textureWidth && current_texture->texture_height == textureHeight) {
				return true;
			}
		}
	}
	return false;
}

texture_ref R_LoadTextureImage(const char *filename, const char *identifier, int matchwidth, int matchheight, int mode)
{
	texture_ref reference;
	byte *data;
	int image_width = -1, image_height = -1;
	gltexture_t *gltexture;

	R_TraceAPI("R_LoadTextureImage(filename=%s, identifier=%s, matchwidth=%d, matchheight=%d, mode=%d)", filename, identifier, matchwidth, matchheight, mode);
	if (Block24BitTextures) {
		return invalid_texture_reference;
	}

	if (!identifier) {
		identifier = filename;
	}

	gltexture = R_FindTexture(identifier);
	if (CheckTextureLoaded(gltexture)) {
		return gltexture->reference;
	}
	if (!(data = R_LoadImagePixels(filename, matchwidth, matchheight, mode, &image_width, &image_height))) {
		if (gltexture) {
			reference = gltexture->reference;
			R_DeleteTexture(&reference);
		}
		return invalid_texture_reference;
	}
	else {
		reference = R_LoadTexturePixels(data, identifier, image_width, image_height, mode);
		Q_free(data);	// Data was Q_malloc'ed by R_LoadImagePixels.
	}

	return reference;
}

void R_ImagePreMultiplyAlpha(byte* image, int width, int height, qbool zero)
{
	// Pre-multiply alpha component
	int pos;

	for (pos = 0; pos < width * height * 4; pos += 4) {
		float alpha = image[pos + 3] / 255.0f;

		image[pos] *= alpha;
		image[pos + 1] *= alpha;
		image[pos + 2] *= alpha;
		if (zero) {
			image[pos + 3] = 0;
		}
	}
}

mpic_t* R_LoadPicImage(const char *filename, char *id, int matchwidth, int matchheight, int mode)
{
	int width, height, i, real_width, real_height;
	char identifier[MAX_QPATH] = "pic:";
	byte *data, *src, *dest, *buf;
	static mpic_t pic;

	// this is 2D texture loading so it must not have MIP MAPS
	mode &= ~TEX_MIPMAP;

	R_TraceAPI("R_LoadPicImage(filename=%s, identifier=%s, matchwidth=%d, matchheight=%d, mode=%d)", filename, id, matchwidth, matchheight, mode);
	if (Block24BitTextures) {
		return NULL;
	}

	// Load the image data from file.
	if (!(data = R_LoadImagePixels(filename, matchwidth, matchheight, 0, &real_width, &real_height))) {
		return NULL;
	}

	pic.width = real_width;
	pic.height = real_height;

	// Check if there's any actual alpha transparency in the image.
	if (mode & TEX_ALPHA) {
		mode &= ~TEX_ALPHA;
		for (i = 0; i < real_width * real_height; i++) {
			if (((((unsigned *)data)[i] >> 24) & 0xFF) < 255) {
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if (mode & TEX_ALPHA) {
		R_ImagePreMultiplyAlpha(data, real_width, real_height, false);
	}

	R_TextureSizeRoundUp(pic.width, pic.height, &width, &height);

	strlcpy(identifier + 4, id ? id : filename, sizeof(identifier) - 4);

	// Upload the texture to OpenGL.
	if (width == pic.width && height == pic.height) {
		// Size of the image is scaled by the power of 2 so we can just
		// load the texture as it is.
		pic.texnum = R_LoadTexture(identifier, pic.width, pic.height, data, mode, 4);
		pic.sl = 0;
		pic.sh = 1;
		pic.tl = 0;
		pic.th = 1;
	}
	else {
		// The size of the image data is not a power of 2, so we
		// need to load it into a bigger texture and then set the
		// texture coordinates so that only the relevant part of it is used.

		buf = (byte *)Q_calloc(width * height, 4);

		src = data;
		dest = buf;

		for (i = 0; i < pic.height; i++) {
			memcpy(dest, src, pic.width * 4);
			src += pic.width * 4;	// Next line in the source.
			dest += width * 4;		// Next line in the target.
		}

		pic.texnum = R_LoadTexture(identifier, width, height, buf, mode, 4);
		pic.sl = 0;
		pic.sh = (float)pic.width / width;
		pic.tl = 0;
		pic.th = (float)pic.height / height;
		Q_free(buf);
	}

	Q_free(data);	// Data was Q_malloc'ed by R_LoadImagePixels
	return &pic;
}

qbool R_LoadCharsetImage(char *filename, char *identifier, int flags, charset_t* pic)
{
	int i, j, image_size, real_width, real_height;
	byte *data, *buf = NULL, *dest, *src;
	texture_ref tex;

	R_TraceEnterRegion(va("R_LoadCharsetImage(filename=%s, identifier=%s, flags=%d)", filename, identifier, flags), true);
	if (Block24BitTextures) {
		R_TraceAPI("24-bit textures blocked");
		R_TraceLeaveRegion(true);
		return false;
	}

	if (!(data = R_LoadImagePixels(filename, 0, 0, flags, &real_width, &real_height))) {
		R_TraceAPI("Failed to load image pixels");
		R_TraceLeaveRegion(true);
		return false;
	}

	R_ImagePreMultiplyAlpha(data, real_width, real_height, false);

	if (!identifier) {
		identifier = filename;
	}

	image_size = real_width * real_height;

	buf = dest = (byte *)Q_calloc(image_size * 4, 4);
	src = data;
	for (j = 0; j < real_height; ++j) {
		for (i = 0; i < real_width; ++i) {
			int x_offset = (i / (real_width >> 4)) * (real_width >> 4);
			int y_offset = (j / (real_height >> 4)) * (real_height >> 4);

			memcpy(&buf[(i + x_offset + 2 * real_width * (j + y_offset)) * 4], &src[(i + real_width * j) * 4], 4);
		}
	}

	tex = R_LoadTexture(identifier, real_width * 2, real_height * 2, buf, flags, 4);
	Q_free(buf);
	if (R_TextureReferenceIsValid(tex)) {
		memset(pic->glyphs, 0, sizeof(pic->glyphs));
		for (i = 0; i < 255; ++i) {
			float char_height = 1.0f / (2 * CHARSET_CHARS_PER_ROW);
			float char_width = 1.0f / (2 * CHARSET_CHARS_PER_ROW);
			float frow = (i >> 4) * char_height * 2;
			float fcol = (i & 0x0F) * char_width * 2;

			pic->glyphs[i].texnum = tex;
			pic->glyphs[i].sl = fcol;
			pic->glyphs[i].sh = fcol + char_width;
			pic->glyphs[i].tl = frow;
			pic->glyphs[i].th = frow + char_height;
			pic->glyphs[i].width = real_width >> 4;
			pic->glyphs[i].height = real_height >> 4;
		}

		pic->master = tex;
	}

	pic->custom_scale_x = 1;
	pic->custom_scale_y = 1;

	Q_free(data);	// data was Q_malloc'ed by R_LoadImagePixels
	R_TraceLeaveRegion(true);
	return R_TextureReferenceIsValid(tex);
}

typedef byte* (*ImageLoadFunction)(vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height);
typedef struct image_load_format_s {
	const char* extension;
	ImageLoadFunction function;
	int filter_mask;
} image_load_format_t;

byte* R_LoadImagePixels(const char *filename, int matchwidth, int matchheight, int mode, int *real_width, int *real_height)
{
	char basename[MAX_QPATH], name[MAX_QPATH];
	byte *c, *data = NULL;
	vfsfile_t *f;

	COM_StripExtension(filename, basename, sizeof(basename));
	for (c = (byte *)basename; *c; c++) {
		if (*c == '*') {
			*c = '#';
		}
	}

	snprintf(name, sizeof(name), "%s.link", basename);
	if ((f = FS_OpenVFS(name, "rb", FS_ANY))) {
		char link[128];
		int len;
		VFS_GETS(f, link, sizeof(link));

		len = strlen(link);

		// Strip endline.
		if (link[len - 1] == '\n') {
			link[len - 1] = '\0';
			--len;
		}

		if (link[len - 1] == '\r') {
			link[len - 1] = '\0';
			--len;
		}

		snprintf(name, sizeof(name), "textures/%s", link);
		if ((f = FS_OpenVFS(name, "rb", FS_ANY))) {
			if (!data && !strcasecmp(link + len - 3, "tga")) {
				data = Image_LoadTGA(f, name, matchwidth, matchheight, real_width, real_height);
			}

#ifdef WITH_PNG
			if (!data && !strcasecmp(link + len - 3, "png")) {
				data = Image_LoadPNG(f, name, matchwidth, matchheight, real_width, real_height);
			}
#endif // WITH_PNG

#ifdef WITH_JPEG
			if (!data && !strcasecmp(link + len - 3, "jpg")) {
				data = Image_LoadJPEG(f, name, matchwidth, matchheight, real_width, real_height);
			}
#endif // WITH_JPEG

			// TEX_NO_PCX - preventing loading skins here
			if (!(mode & TEX_NO_PCX) && !data && !strcasecmp(link + len - 3, "pcx")) {
				data = Image_LoadPCX_As32Bit(f, name, matchwidth, matchheight, real_width, real_height);
			}

			if (data)
				return data;
		}
	}

	image_load_format_t formats[] = {
		{ "tga", Image_LoadTGA, 0 },
#ifdef WITH_PNG
		{ "png", Image_LoadPNG, 0 },
#endif
#ifdef WITH_JPEG
		{ "jpg", Image_LoadJPEG, 0 },
#endif
		{ "pcx", Image_LoadPCX_As32Bit, TEX_NO_PCX }
	};
	int i = 0;

	image_load_format_t* best = NULL;
	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
		vfsfile_t *file = NULL;

		if (mode & formats[i].filter_mask) {
			continue;
		}

		snprintf(name, sizeof(name), "%s.%s", basename, formats[i].extension);
		if ((file = FS_OpenVFS(name, "rb", FS_ANY))) {
			if (f == NULL || (f->copyprotected && !file->copyprotected)) {
				if (f) {
					VFS_CLOSE(f);
				}
				f = file;
				best = &formats[i];
			}
			else {
				VFS_CLOSE(file);
			}
		}
	}

	if (best && f) {
		snprintf(name, sizeof(name), "%s.%s", basename, best->extension);
		if ((data = best->function(f, name, matchwidth, matchheight, real_width, real_height))) {
			return data;
		}
	}

	if (mode & TEX_COMPLAIN) {
		if (!Block24BitTextures) {
			Com_Printf_State(PRINT_FAIL, "Couldn't load %s image\n", COM_SkipPath(filename));
		}
	}

	return NULL;
}

texture_ref R_LoadTexturePixels(byte *data, const char *identifier, int width, int height, int mode)
{
	int i, j, image_size;
	qbool gamma;
	extern byte vid_gamma_table[256];
	extern float vid_gamma;

	image_size = width * height;
	gamma = (vid_gamma != 1);

	if (mode & TEX_LUMA) {
		gamma = false;
	}
	else if (mode & TEX_ALPHA) {
		mode &= ~TEX_ALPHA;

		for (j = 0; j < image_size; j++) {
			if (((((unsigned *)data)[j] >> 24) & 0xFF) < 255) {
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if ((mode & TEX_ALPHA) && (mode & TEX_PREMUL_ALPHA)) {
		R_ImagePreMultiplyAlpha(data, width, height, mode & TEX_ZERO_ALPHA);
	}

	if (R_OldGammaBehaviour() && gamma) {
		for (i = 0; i < image_size; i++) {
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	return R_LoadTexture(identifier, width, height, data, mode, 4);
}

texture_ref R_LoadPicTexture(const char *name, mpic_t *pic, byte *data)
{
	int glwidth, glheight, i;
	char fullname[MAX_QPATH] = "pic:";
	byte *src, *dest, *buf;

	R_TextureSizeRoundUp(pic->width, pic->height, &glwidth, &glheight);

	strlcpy(fullname + 4, name, sizeof(fullname) - 4);
	if (glwidth == pic->width && glheight == pic->height) {
		pic->texnum = R_LoadTexture(fullname, glwidth, glheight, data, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = 1;
		pic->tl = 0;
		pic->th = 1;
	}
	else {
		buf = Q_calloc(glwidth * glheight, 1);

		src = data;
		dest = buf;
		for (i = 0; i < pic->height; i++) {
			memcpy(dest, src, pic->width);
			src += pic->width;
			dest += glwidth;
		}
		pic->texnum = R_LoadTexture(fullname, glwidth, glheight, buf, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = (float)pic->width / glwidth;
		pic->tl = 0;
		pic->th = (float)pic->height / glheight;
		Q_free(buf);
	}

	return pic->texnum;
}

texture_ref R_LoadTexture(const char *identifier, int width, int height, byte *data, int mode, int bpp)
{
	unsigned short crc = identifier[0] && data ? CRC_Block(data, width * height * bpp) : 0;
	qbool new_texture = false;
	gltexture_t *glt = R_TextureAllocateSlot(texture_type_2d, identifier, width, height, 0, bpp, mode, crc, &new_texture);

	R_TraceEnterFunctionRegion;
	R_TraceAPI("R_LoadTexture(id=%s, width=%d, height=%d, mode=%d, bpp=%d)", identifier, width, height, mode, bpp);
	if (glt && !new_texture) {
		R_TraceLeaveFunctionRegion;
		return glt->reference;
	}
	else if (!glt) {
		R_TraceLeaveFunctionRegion;
		return null_texture_reference;
	}

	if (data) {
		R_LoadTextureData(glt, width, height, data, mode, bpp);
	}

	R_TraceLeaveFunctionRegion;
	return glt->reference;
}

//
// Uploads a 32-bit texture to OpenGL. Makes sure it's the correct size and creates mipmaps if requested.
//
static void R_Upload32(gltexture_t* glt, unsigned *data, int width, int height, int mode)
{
	// Tell OpenGL the texnum of the texture before uploading it.
	extern cvar_t gl_lerpimages, gl_wicked_luma_level;
	int	tempwidth, tempheight;
	byte *newdata;

	R_TextureSizeRoundUp(width, height, &tempwidth, &tempheight);

	newdata = Q_malloc(tempwidth * tempheight * 4);

	// Resample the image if it's not scaled to the power of 2,
	// we take care of this when drawing using the texture coordinates.
	if (width < tempwidth || height < tempheight) {
		Image_Resample(data, width, height, newdata, tempwidth, tempheight, 4, !!gl_lerpimages.value);
		width = tempwidth;
		height = tempheight;
	}
	else {
		// Scale is a power of 2, just copy the data.
		memcpy(newdata, data, width * height * 4);
	}

	if ((mode & TEX_FULLBRIGHT) && (mode & TEX_LUMA) && gl_wicked_luma_level.integer > 0) {
		int i, cnt = width * height * 4, level = gl_wicked_luma_level.integer;

		for (i = 0; i < cnt; i += 4) {
			if (newdata[i] < level && newdata[i + 1] < level && newdata[i + 2] < level) {
				// make black pixels transparent, well not always black, depends of level...
				newdata[i] = newdata[i + 1] = newdata[i + 2] = newdata[i + 3] = 0;
			}
		}
	}

	// Get the scaled dimension (scales according to gl_picmip and max allowed texture size).
	R_TextureUtil_ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	// If the image size is bigger than the max allowed size or 
	// set picmip value we calculate it's next closest mip map.
	while (width > tempwidth || height > tempheight) {
		Image_MipReduce(newdata, newdata, &width, &height, 4);
	}

	glt->texture_width = width;
	glt->texture_height = height;

	if (mode & TEX_MIPMAP) {
		int largest = max(width, height);
		while (largest) {
			largest /= 2;
		}
	}

	if (mode & TEX_BRIGHTEN) {
		R_TextureUtil_Brighten32(newdata, width * height * 4);
	}

	if (R_UseImmediateOpenGL() || R_UseModernOpenGL()) {
		GL_UploadTexture(glt->reference, glt->texmode, width, height, newdata);
	}
	else if (R_UseVulkan()) {
		//VK_UploadTexture(glt->reference, glt->texmode, width, height, newdata);
	}

	R_TextureUtil_SetFiltering(glt->reference);

	Q_free(newdata);
}

static void R_Upload8(gltexture_t* glt, byte *data, int width, int height, int mode)
{
	static unsigned* trans;
	static int trans_size;
	int	i, image_size, p;
	unsigned *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > trans_size) {
		unsigned* newmem = Q_realloc(trans, image_size * 4);
		if (newmem == NULL) {
			Sys_Error("GL_Upload8: image too big (%s: %dx%d)", glt->identifier[0] ? glt->identifier : "?unknown?", width, height);
		}
		trans = newmem;
		trans_size = image_size * 4;
	}

	if (mode & TEX_FULLBRIGHT) {
		qbool was_alpha = mode & TEX_ALPHA;

		// This is a fullbright mask, so make all non-fullbright colors transparent.
		mode |= TEX_ALPHA;

		for (i = 0; i < image_size; i++) {
			p = data[i];
			if (p < 224 || (p == 255 && was_alpha)) {
				trans[i] = 0; // Transparent.
			}
			else {
				trans[i] = (p == 255) ? LittleLong(0xff535b9f) : table[p]; // Fullbright. Disable transparancy on fullbright colors (255).
			}
		}
	}
	else if (mode & TEX_ALPHA) {
		// If there are no transparent pixels, make it a 3 component
		// texture even if it was specified as otherwise
		mode &= ~TEX_ALPHA;

		for (i = 0; i < image_size; i++) {
			if ((p = data[i]) == 255) {
				mode |= TEX_ALPHA;
			}
			trans[i] = table[p];
		}
	}
	else {
		if (image_size & 3) {
			Sys_Error("GL_Upload8: image_size & 3");
		}

		// Convert the 8-bit colors to 24-bit.
		for (i = 0; i < image_size; i += 4) {
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}

	R_Upload32(glt, trans, width, height, mode & ~TEX_BRIGHTEN);
}

static void R_LoadTextureData(gltexture_t* glt, int width, int height, byte *data, int mode, int bpp)
{
	// Upload the texture to OpenGL based on the bytes per pixel.
	switch (bpp) {
		case 1:
			R_Upload8(glt, data, width, height, mode);
			break;
		case 4:
			R_Upload32(glt, (void *)data, width, height, mode);
			break;
		default:
			Sys_Error("R_LoadTexture: unknown bpp\n");
			break;
	}
}

void R_TextureSizeRoundUp(int orig_width, int orig_height, int* width, int* height)
{
	if (r_texture_support_non_power_of_two) {
		*width = orig_width;
		*height = orig_height;
	}
	else {
		Q_ROUND_POWER2(orig_width, (*width));
		Q_ROUND_POWER2(orig_height, (*height));
	}
}

void R_SetNonPowerOfTwoSupport(qbool supported)
{
	r_texture_support_non_power_of_two = supported;
}
