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

$Id: gl_texture.c,v 1.44 2007-10-05 19:06:24 johnnycz Exp $
*/

#include "quakedef.h"
#include "crc.h"
#include "image.h"
#include "gl_model.h"
#include "gl_local.h"
#include "tr_types.h"

// #define USE_OPENGL_DIMENSIONS

const texture_ref null_texture_reference = { 0 };

// Private GL function to allocate names
void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures);

void OnChange_gl_max_size (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_texturemode (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_miptexLevel (cvar_t *var, char *string, qbool *cancel);
void OnChange_gl_anisotropy (cvar_t *var, char *string, qbool *cancel);

static const texture_ref invalid_texture_reference = { 0 };
static qbool no24bit, forceTextureReload;

extern unsigned d_8to24table2[256];
extern byte vid_gamma_table[256];
extern float vid_gamma;

extern int anisotropy_ext;
int	anisotropy_tap = 1; //  1 - is off

GLenum gl_lightmap_format = GL_RGB8, gl_solid_format = GL_RGB8, gl_alpha_format = GL_RGBA8;

cvar_t	gl_max_size			= {"gl_max_size", "2048", 0, OnChange_gl_max_size};
cvar_t	gl_picmip			= {"gl_picmip", "0"};
cvar_t	gl_miptexLevel		= {"gl_miptexLevel", "0", 0, OnChange_gl_miptexLevel};
cvar_t	gl_lerpimages		= {"gl_lerpimages", "1"};
cvar_t	gl_texturemode		= {"gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", 0, OnChange_gl_texturemode};
cvar_t	gl_texturemode2d	= {"gl_texturemode2d", "GL_LINEAR", 0, OnChange_gl_texturemode};
cvar_t	gl_anisotropy		= {"gl_anisotropy","1", 0, OnChange_gl_anisotropy};

cvar_t	gl_scaleModelTextures		= {"gl_scaleModelTextures", "0"};
cvar_t	gl_scaleTurbTextures		= {"gl_scaleTurbTextures", "1"};
cvar_t	gl_externalTextures_world	= {"gl_externalTextures_world", "1"};
cvar_t	gl_externalTextures_bmodels	= {"gl_externalTextures_bmodels", "1"};
cvar_t  gl_no24bit                  = {"gl_no24bit", "0", CVAR_LATCH};

cvar_t  gl_wicked_luma_level        = {"gl_luma_level", "1", CVAR_LATCH};

typedef struct {
	// OpenGL texture number (or array)
	GLuint		texnum;
	// OpenGL texture array index (modern only)
	GLuint      texindex;

	// 
	char		identifier[MAX_QPATH];
	char		*pathname;
	int			width, height;
	int         gl_width, gl_height;
	int			texmode;
	GLint       internal_format;
	unsigned	crc;
	int			bpp;

	// Arrays
	qbool       is_array;
	int         depth;

	GLenum      target;
	texture_ref reference;
	int         next_free;

	qbool       mipmaps_generated;
	qbool       storage_allocated;
} gltexture_t;

static gltexture_t	gltextures[MAX_GLTEXTURES];
static int			numgltextures = 1;
static int          next_free_texture = 0;
//	   int			texture_extension_number = 1; // non static, sad but used in gl_framebufer.c too

void OnChange_gl_max_size (cvar_t *var, char *string, qbool *cancel) 
{
	int i;
	float newvalue = Q_atof(string);

	if (newvalue > glConfig.gl_max_size_default) 
	{
		Com_Printf("Your hardware doesn't support texture sizes bigger than %dx%d\n", glConfig.gl_max_size_default, glConfig.gl_max_size_default);
		*cancel = true;
		return;
	}

	Q_ROUND_POWER2(newvalue, i);

	if (i != newvalue) 
	{
		Com_Printf("Valid values for %s are powers of 2 only\n", var->name);
		*cancel = true;
		return;
	}
}

typedef struct 
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define GLMODE_NUMODES	(sizeof(modes) / sizeof(glmode_t))

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;
static int gl_filter_max_2d = GL_LINEAR;

void OnChange_gl_texturemode (cvar_t *var, char *string, qbool *cancel) 
{
	int i, filter_min, filter_max;
	qbool mipmap;
	gltexture_t	*glt;

	for (i = 0; i < GLMODE_NUMODES; i++) 
	{
		if (!strcasecmp (modes[i].name, string))
			break;
	}

	if (i == GLMODE_NUMODES) 
	{
		Com_Printf ("bad filter name: %s\n", string);
		*cancel = true;
		return;
	}

	if (var == &gl_texturemode)
	{
		gl_filter_min = filter_min = modes[i].minimize;
		gl_filter_max = filter_max = modes[i].maximize;
		mipmap = true;
	}
	else if (var == &gl_texturemode2d)
	{
		gl_filter_max_2d = filter_min = filter_max = modes[i].maximize;
		mipmap = false;
	}
	else
	{
		Sys_Error("OnChange_gl_texturemode: unexpected cvar!");
		return;
	}

	// Make sure we set the proper texture filters for textures.
	for (i = 1, glt = gltextures + 1; i < numgltextures; i++, glt++)
	{
		if (!GL_TextureReferenceIsValid(glt->reference)) {
			continue;
		}

		if (glt->texmode & TEX_NO_TEXTUREMODE) {
			continue;	// This texture must NOT be affected by texture mode changes,
						// for example charset which rather controlled by gl_smoothfont.
		}

		// true == true or false == false
		if ( mipmap == !!(glt->texmode & TEX_MIPMAP) )
		{
			if (developer.integer > 100) {
				Com_DPrintf("texturemode: %s\n", glt->identifier);
			}

			GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MIN_FILTER, filter_min);
			GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MAG_FILTER, filter_max);
		}
	}
}

void OnChange_gl_anisotropy (cvar_t *var, char *string, qbool *cancel) 
{
	int i;
	gltexture_t *glt;
	int newvalue = Q_atoi(string);

	anisotropy_tap = max(1, newvalue); // 0 is bad, 1 is off, 2 and higher are valid modes

	if (!anisotropy_ext) {
		return; // we doesn't have such extension
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
		if (glt->texmode & TEX_NO_TEXTUREMODE) {
			// This texture must NOT be affected by texture mode changes,
			// for example charset which rather controlled by gl_smoothfont.
			continue;	
		}

		if (glt->texmode & TEX_MIPMAP) {
			GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
		}
	}
}

void OnChange_gl_miptexLevel (cvar_t *var, char *string, qbool *cancel)
{
	float newval = Q_atof(string);

	if (newval != 0 && newval != 1 && newval != 2 && newval != 3) {
		Com_Printf("Valid values for %s are 0,1,2,3 only\n", var->name);
		*cancel = true;
	}
}

static void ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode) 
{
	int maxsize, picmip;
	qbool scale = (mode & TEX_MIPMAP) && !(mode & TEX_NOSCALE);

	if (gl_support_arb_texture_non_power_of_two)
	{
		*scaled_width = width;
		*scaled_height = height;
	}
	else
	{
		Q_ROUND_POWER2(width, *scaled_width);
		Q_ROUND_POWER2(height, *scaled_height);
	}

	if (scale) 
	{
		picmip = (int) bound(0, gl_picmip.value, 16);
		*scaled_width >>= picmip;
		*scaled_height >>= picmip;
	}

	maxsize = scale ? gl_max_size.value : glConfig.gl_max_size_default;

	*scaled_width = bound(1, *scaled_width, maxsize);
	*scaled_height = bound(1, *scaled_height, maxsize);
}

static void brighten32 (byte *data, int size)
{
	byte *p;
	int i;

	p = data;
	for (i = 0; i < size/4; i++)
	{
		p[0] = min(p[0] * 2.0/1.5, 255);
		p[1] = min(p[1] * 2.0/1.5, 255);
		p[2] = min(p[2] * 2.0/1.5, 255);
		p += 4;
	}
}

static int GL_InternalFormat(int mode)
{
	if (gl_gammacorrection.integer) {
		return (mode & TEX_ALPHA) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
	}
	else if (mode & TEX_NOCOMPRESS) {
		return (mode & TEX_ALPHA) ? GL_RGBA8 : GL_RGB8;
	}
	else {
		return (mode & TEX_ALPHA) ? gl_alpha_format : gl_solid_format;
	}
}

//
// Uploads a 32-bit texture to OpenGL. Makes sure it's the correct size and creates mipmaps if requested.
//
static void GL_Upload32(gltexture_t* glt, unsigned *data, int width, int height, int mode)
{
	// Tell OpenGL the texnum of the texture before uploading it.
	int	tempwidth, tempheight, levels;
	unsigned int *newdata;

	if (gl_support_arb_texture_non_power_of_two)
	{
		tempwidth = width;
		tempheight = height;
	}
	else
	{
		Q_ROUND_POWER2(width, tempwidth);
		Q_ROUND_POWER2(height, tempheight);
	}

	newdata = (unsigned int *) Q_malloc(tempwidth * tempheight * 4);

	// Resample the image if it's not scaled to the power of 2,
	// we take care of this when drawing using the texture coordinates.
	if (width < tempwidth || height < tempheight) 
	{
		Image_Resample (data, width, height, newdata, tempwidth, tempheight, 4, !!gl_lerpimages.value);
		width = tempwidth;
		height = tempheight;
	} 
	else 
	{
		// Scale is a power of 2, just copy the data.
		memcpy (newdata, data, width * height * 4);
	}

	if ((mode & TEX_FULLBRIGHT) && (mode & TEX_LUMA) && gl_wicked_luma_level.integer > 0) {
		int i, cnt = width * height * 4, level = gl_wicked_luma_level.integer;
		byte *bdata = (byte*)newdata;

		for (i = 0; i < cnt; i += 4) {
			if (bdata[i] < level && bdata[i + 1] < level && bdata[i + 2] < level) {
				bdata[i + 3] = 0; // make black pixels transparent, well not always black, depends of level...
			}
		}
	}

	// Get the scaled dimension (scales according to gl_picmip and max allowed texture size).
	ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	// If the image size is bigger than the max allowed size or 
	// set picmip value we calculate it's next closest mip map.
	while (width > tempwidth || height > tempheight) {
		Image_MipReduce((byte *)newdata, (byte *)newdata, &width, &height, 4);
	}

	glt->gl_width = width;
	glt->gl_height = height;

	if (mode & TEX_MIPMAP) {
		int largest = max(width, height);
		levels = 0;
		while (largest) {
			++levels;
			largest /= 2;
		}
	}
	else {
		levels = 1;
	}

	if (mode & TEX_BRIGHTEN) {
		brighten32((byte *)newdata, width * height * 4);
	}

	// Upload the main texture to OpenGL.
	GL_TexSubImage2D(GL_TEXTURE0, glt->reference, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, newdata);

	if (mode & TEX_MIPMAP) {
		GL_GenerateMipmapWithData(GL_TEXTURE0, glt->reference, (byte*)newdata, width, height, glt->internal_format);

		GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		if (anisotropy_ext) {
			GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
		}
	} 
	else {
		GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		GL_TexParameterf(GL_TEXTURE0, glt->reference, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

	Q_free(newdata);
}

static void GL_Upload8(gltexture_t* glt, byte *data, int width, int height, int mode)
{
	static unsigned trans[640 * 480];
	int	i, image_size, p;
	unsigned *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > sizeof(trans))
		Sys_Error ("GL_Upload8: image too big");

	if (mode & TEX_FULLBRIGHT) 
	{
		qbool was_alpha = mode & TEX_ALPHA;

		// This is a fullbright mask, so make all non-fullbright colors transparent.
		mode |= TEX_ALPHA;
	
		for (i = 0; i < image_size; i++)
		{
			p = data[i];
			if (p < 224 || (was_alpha && p == 255)) {
				trans[i] = 0; // Transparent.
			}
			else {
				trans[i] = (p == 255) ? LittleLong(0xff535b9f) : table[p]; // Fullbright. Disable transparancy on fullbright colors (255).
			}
		}
	} 
	else if (mode & TEX_ALPHA) 
	{
		// If there are no transparent pixels, make it a 3 component
		// texture even if it was specified as otherwise
		mode &= ~TEX_ALPHA;
	
		for (i = 0; i < image_size; i++) 
		{
			if ((p = data[i]) == 255)
				mode |= TEX_ALPHA;
			trans[i] = table[p];
		}
	} 
	else
	{
		if (image_size & 3)
			Sys_Error ("GL_Upload8: image_size & 3");

		// Convert the 8-bit colors to 24-bit.
		for (i = 0; i < image_size; i += 4)
		{
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}

	GL_Upload32(glt, trans, width, height, mode & ~TEX_BRIGHTEN);
}

static gltexture_t* GL_NextTextureSlot(GLenum target)
{
	int slot;
	gltexture_t* glt;

	// Re-use a deleted slot if possible
	if (next_free_texture) {
		slot = next_free_texture;
		next_free_texture = gltextures[slot].next_free;
		gltextures[slot].next_free = 0;
	}
	else if (numgltextures >= MAX_GLTEXTURES) {
		Sys_Error("GL_LoadTexture: numgltextures == MAX_GLTEXTURES");
	}
	else {
		slot = numgltextures++;
	}

	glt = &gltextures[slot];
	glt->reference.index = slot;
	glt->target = target;
	return glt;
}

static gltexture_t* GL_AllocateTextureSlot(GLenum target, const char* identifier, int width, int height, int depth, int bpp, int mode, unsigned short crc, qbool* new_texture)
{
	int gl_width, gl_height;
	int scaled_width, scaled_height;
	gltexture_t* glt = NULL;
	qbool load_over_existing = false;
	int miplevels = 1;

	*new_texture = true;

	if (lightmode != 2) {
		mode &= ~TEX_BRIGHTEN;
	}

	ScaleDimensions(width, height, &scaled_width, &scaled_height, mode);

	if (developer.integer >= 3) {
		Com_DPrintf("Texture: %s %dx%d -> %dx%d %s\n",
			identifier, width, height, scaled_width, scaled_height,
			((scaled_width & (scaled_width - 1)) || (scaled_height & (scaled_height - 1))) ? "non power of two" : "");
	}

	gl_width = width;
	gl_height = height;
	while (gl_width > scaled_width || gl_height > scaled_height) {
		gl_width = max(1, gl_width / 2);
		gl_height = max(1, gl_height / 2);
	}

	{
		int temp_w = gl_width;
		int temp_h = gl_height;

		while (temp_w > 1 || temp_h > 1) {
			temp_w = max(1, temp_w / 2);
			temp_h = max(1, temp_h / 2);
			++miplevels;
		}
	}

	// If we were given an identifier for the texture, search through
	// the list of loaded texture and see if we find a match, if so
	// return the texnum for the already loaded texture.
	if (identifier[0]) {
		int i;
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++) {
			if (!strncmp(identifier, glt->identifier, sizeof(glt->identifier) - 1)) {
				qbool same_dimensions = (width == glt->width && height == glt->height && depth == glt->depth);
				qbool same_scaling = (gl_width == glt->gl_width && gl_height == glt->gl_height);
				qbool same_format = (glt->bpp == bpp && glt->target == target);
				qbool same_options = (mode & ~(TEX_COMPLAIN | TEX_NOSCALE)) == (glt->texmode & ~(TEX_COMPLAIN | TEX_NOSCALE));

				// Identifier matches, make sure everything else is the same
				// so that we can be really sure this is the correct texture.
				if (same_dimensions && same_scaling && same_format && same_options && crc == glt->crc) {
					GL_BindTextureUnit(GL_TEXTURE0, glt->reference);
					*new_texture = false;
					return glt;
				}
				else {
					// Same identifier but different texture, so overwrite
					// the already loaded texture.
					load_over_existing = true;
					break;
				}
			}
		}
	}

	// If the identifier was the same as another textures, we won't bother
	// with taking up a new texture slot, just load the new texture
	// over the old one.
	if (!load_over_existing) {
		glt = GL_NextTextureSlot(target);

		strlcpy(glt->identifier, identifier, sizeof(glt->identifier));
		GL_CreateTextureNames(GL_TEXTURE0, glt->target, 1, &glt->texnum);
	}
	else if (glt && !glt->texnum) {
		GL_CreateTextureNames(GL_TEXTURE0, glt->target, 1, &glt->texnum);
	}
	else if (glt && glt->storage_allocated) {
		if (gl_width != glt->gl_width || gl_height != glt->gl_height || glt->bpp != bpp) {
			texture_ref ref = glt->reference;

			GL_DeleteTexture(&ref);

			return GL_AllocateTextureSlot(target, identifier, width, height, 0, bpp, mode, crc, new_texture);
		}
	}

	if (!glt) {
		Sys_Error("GL_LoadTexture: glt not initialized\n");
	}

	// Allocate storage
	if (!glt->storage_allocated && (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP)) {
		glt->internal_format = GL_InternalFormat(mode);

		GL_TexStorage2D(GL_TEXTURE0, glt->reference, miplevels, glt->internal_format, gl_width, gl_height);
		glt->storage_allocated = true;
	}

	glt->width = width;
	glt->height = height;
	glt->depth = depth;
	glt->texmode = mode;
	glt->crc = crc;
	glt->bpp = bpp;
	glt->is_array = (depth > 0);
	glt->gl_width = gl_width;
	glt->gl_height = gl_height;

	Q_free(glt->pathname);
	if (bpp == 4 && fs_netpath[0]) {
		glt->pathname = Q_strdup(fs_netpath);
	}

	GL_BindTextureUnit(GL_TEXTURE0, glt->reference);

	return glt;
}

static void GL_LoadTextureData(gltexture_t* glt, int width, int height, byte *data, int mode, int bpp)
{
	// Upload the texture to OpenGL based on the bytes per pixel.
	switch (bpp)
	{
	case 1:
		GL_Upload8(glt, data, width, height, mode);
		break;
	case 4:
		GL_Upload32(glt, (void *) data, width, height, mode);
		break;
	default:
		Sys_Error("GL_LoadTexture: unknown bpp\n"); break;
	}
}

texture_ref GL_LoadTexture(const char *identifier, int width, int height, byte *data, int mode, int bpp)
{
	unsigned short crc = identifier[0] ? CRC_Block(data, width * height * bpp) : 0;
	qbool new_texture = false;
	gltexture_t *glt = GL_AllocateTextureSlot(GL_TEXTURE_2D, identifier, width, height, 0, bpp, mode, crc, &new_texture);
	texture_ref ref = { 0 };

	if (glt && !new_texture) {
		return glt->reference;
	}

	GL_LoadTextureData(glt, width, height, data, mode, bpp);

	return glt->reference;
}

texture_ref GL_LoadPicTexture(const char *name, mpic_t *pic, byte *data) 
{
	int glwidth, glheight, i;
	char fullname[MAX_QPATH] = "pic:";
	byte *src, *dest, *buf;

	if (gl_support_arb_texture_non_power_of_two)
	{
		glwidth = pic->width;
		glheight = pic->height;
	}
	else
	{
		Q_ROUND_POWER2(pic->width, glwidth);
		Q_ROUND_POWER2(pic->height, glheight);
	}

	strlcpy (fullname + 4, name, sizeof(fullname) - 4);
	if (glwidth == pic->width && glheight == pic->height) {
		pic->texnum = GL_LoadTexture(fullname, glwidth, glheight, data, TEX_ALPHA, 1);
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
		pic->texnum = GL_LoadTexture(fullname, glwidth, glheight, buf, TEX_ALPHA, 1);
		pic->sl = 0;
		pic->sh = (float) pic->width / glwidth;
		pic->tl = 0;
		pic->th = (float) pic->height / glheight;
		Q_free (buf);
	}

	return pic->texnum;
}

gltexture_t *GL_FindTexture (char *identifier) 
{
	int i;

	for (i = 0; i < numgltextures; i++) 
	{
		if (!strcmp (identifier, gltextures[i].identifier))
			return &gltextures[i];
	}

	return NULL;
}

static gltexture_t *current_texture = NULL;

#define CHECK_TEXTURE_ALREADY_LOADED	\
	if (CheckTextureLoaded(mode)) {		\
		current_texture = NULL;			\
		VFS_CLOSE(f);					\
		return NULL;					\
	}


static qbool CheckTextureLoaded(int mode)
{
	int scaled_width, scaled_height;

	if (!forceTextureReload) {
		if (current_texture && current_texture->pathname && !strcmp(fs_netpath, current_texture->pathname)) {
			int width = current_texture->width;
			int height = current_texture->height;

			ScaleDimensions(current_texture->width, current_texture->height, &scaled_width, &scaled_height, mode);
			while (width > scaled_width || height > scaled_height) {
				width = max(1, width / 2);
				height = max(1, height / 2);
			}

			if (current_texture->gl_width == width && current_texture->gl_height == height) {
				return true;
			}
		}
	}
	return false;
}

typedef byte* (*ImageLoadFunction)(vfsfile_t *fin, const char *filename, int matchwidth, int matchheight, int *real_width, int *real_height);
typedef struct image_load_format_s {
	const char* extension;
	ImageLoadFunction function;
	int filter_mask;
} image_load_format_t;

byte *GL_LoadImagePixels (const char *filename, int matchwidth, int matchheight, int mode, int *real_width, int *real_height) 
{
	char basename[MAX_QPATH], name[MAX_QPATH];
	byte *c, *data = NULL;
	vfsfile_t *f;

	COM_StripExtension(filename, basename, sizeof(basename));
	for (c = (byte *) basename; *c; c++)
	{
		if (*c == '*')
			*c = '#';
	}

	snprintf (name, sizeof(name), "%s.link", basename);
	if ((f = FS_OpenVFS(name, "rb", FS_ANY))) 
	{
		char link[128];
		int len;
		VFS_GETS(f, link, sizeof(link));

		len = strlen(link);

		// Strip endline.
		if (link[len-1] == '\n') 
		{
			link[len-1] = '\0';
			--len;
		}

		if (link[len-1] == '\r') 
		{
			link[len-1] = '\0';
			--len;
		}

		snprintf (name, sizeof(name), "textures/%s", link);
		if ((f = FS_OpenVFS(name, "rb", FS_ANY))) 
		{
       		CHECK_TEXTURE_ALREADY_LOADED;
       		if( !data && !strcasecmp(link + len - 3, "tga") )
			{
				data = Image_LoadTGA (f, name, matchwidth, matchheight, real_width, real_height);
			}

			#ifdef WITH_PNG
       		if( !data && !strcasecmp(link + len - 3, "png") )
			{
       			data = Image_LoadPNG (f, name, matchwidth, matchheight, real_width, real_height);
			}
			#endif // WITH_PNG
			
			#ifdef WITH_JPEG
       		if( !data && !strcasecmp(link + len - 3, "jpg") )
			{
				data = Image_LoadJPEG (f, name, matchwidth, matchheight, real_width, real_height);
			}
			#endif // WITH_JPEG

			// TEX_NO_PCX - preventing loading skins here
       		if( !(mode & TEX_NO_PCX) && !data && !strcasecmp(link + len - 3, "pcx") )
			{
				data = Image_LoadPCX_As32Bit (f, name, matchwidth, matchheight, real_width, real_height);
			}

       		if ( data )
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
	for (i = 0; i < sizeof (formats) / sizeof (formats[0]); ++i) {
		vfsfile_t *file = NULL;

		if (mode & formats[i].filter_mask)
			continue;

		snprintf (name, sizeof (name), "%s.%s", basename, formats[i].extension);
		if ((file = FS_OpenVFS (name, "rb", FS_ANY))) {
			if (f == NULL || (f->copyprotected && !file->copyprotected)) {
				if (f) {
					VFS_CLOSE (f);
				}
				f = file;
				best = &formats[i];
			}
			else {
				VFS_CLOSE (file);
			}
		}
	}

	if (best && f) {
		CHECK_TEXTURE_ALREADY_LOADED;
		snprintf (name, sizeof (name), "%s.%s", basename, best->extension);
		if ((data = best->function (f, name, matchwidth, matchheight, real_width, real_height))) {
			return data;
		}
	}

	if (mode & TEX_COMPLAIN) 
	{
		if (!no24bit)
			Com_Printf_State(PRINT_FAIL, "Couldn't load %s image\n", COM_SkipPath(filename));
	}

	return NULL;
}

texture_ref GL_LoadTexturePixels(byte *data, char *identifier, int width, int height, int mode)
{
	int i, j, image_size;
	qbool gamma;

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

	if (gamma) {
		for (i = 0; i < image_size; i++) {
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	return GL_LoadTexture(identifier, width, height, data, mode, 4);
}

texture_ref GL_LoadTextureImage(char *filename, char *identifier, int matchwidth, int matchheight, int mode)
{
	texture_ref reference;
	byte *data;
	int image_width = -1, image_height = -1;
	gltexture_t *gltexture;

	if (no24bit) {
		return invalid_texture_reference;
	}

	if (!identifier) {
		identifier = filename;
	}

	gltexture = current_texture = GL_FindTexture(identifier);

	if (!(data = GL_LoadImagePixels(filename, matchwidth, matchheight, mode, &image_width, &image_height))) {
		reference = (gltexture && !current_texture) ? gltexture->reference : invalid_texture_reference;
	}
	else {
		reference = GL_LoadTexturePixels(data, identifier, image_width, image_height, mode);
		Q_free(data);	// Data was Q_malloc'ed by GL_LoadImagePixels.
	}

	current_texture = NULL;
	return reference;
}

mpic_t* GL_LoadPicImage(const char *filename, char *id, int matchwidth, int matchheight, int mode)
{
	int width, height, i, real_width, real_height;
	char identifier[MAX_QPATH] = "pic:";
	byte *data, *src, *dest, *buf;
	static mpic_t pic;

	// this is 2D texture loading so it must not have MIP MAPS
	mode = mode & ~TEX_MIPMAP;

	if (no24bit) {
		return NULL;
	}

	// Load the image data from file.
	if (!(data = GL_LoadImagePixels(filename, matchwidth, matchheight, 0, &real_width, &real_height))) {
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

	if (gl_support_arb_texture_non_power_of_two) {
		width = pic.width;
		height = pic.height;
	}
	else {
		Q_ROUND_POWER2(pic.width, width);
		Q_ROUND_POWER2(pic.height, height);
	}

	strlcpy(identifier + 4, id ? id : filename, sizeof(identifier) - 4);

	// Upload the texture to OpenGL.
	if (width == pic.width && height == pic.height) {
		// Size of the image is scaled by the power of 2 so we can just
		// load the texture as it is.
		pic.texnum = GL_LoadTexture(identifier, pic.width, pic.height, data, mode, 4);
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

		pic.texnum = GL_LoadTexture(identifier, width, height, buf, mode, 4);
		pic.sl = 0;
		pic.sh = (float)pic.width / width;
		pic.tl = 0;
		pic.th = (float)pic.height / height;
		Q_free(buf);
	}

	Q_free(data);	// Data was Q_malloc'ed by GL_LoadImagePixels
	return &pic;
}

qbool GL_LoadCharsetImage(char *filename, char *identifier, int flags, mpic_t* pic)
{
	int i, image_size, real_width, real_height;
	byte *data, *buf, *dest, *src;

	if (no24bit) {
		return false;
	}

	if (!(data = GL_LoadImagePixels(filename, 0, 0, flags, &real_width, &real_height))) {
		return false;
	}

	if (!identifier) {
		identifier = filename;
	}

	image_size = real_width * real_height;

	buf = dest = (byte *)Q_calloc(image_size * 2, 4);
	src = data;
	for (i = 0; i < 16; i++) {
		memcpy(dest, src, image_size >> 2);
		src += image_size >> 2;
		dest += image_size >> 1;
	}

	pic->texnum = GL_LoadTexture(identifier, real_width, real_height * 2, buf, flags, 4);
	pic->sl = pic->tl = 0.0f;
	pic->sh = pic->th = 1.0f;

	Q_free(buf);
	Q_free(data);	// data was Q_malloc'ed by GL_LoadImagePixels
	return GL_TextureReferenceIsValid(pic->texnum);
}

void GL_Texture_Init(void) 
{
	cvar_t *cv;
	int i;
	extern texture_ref lightmap_texture_array;
	extern texture_ref lightmap_textures[MAX_LIGHTMAPS];
	extern texture_ref sceneblur_texture;

	// Reset some global vars, probably we need here even more...

	// Reset textures array and linked globals
	for (i = 0; i < numgltextures; i++) {
		if (!gltextures[i].texnum) {
			glDeleteTextures(1, &gltextures[i].texnum);
		}
		Q_free(gltextures[i].pathname);
	}

	memset(gltextures, 0, sizeof(gltextures));

	//texture_extension_number = 1;
	current_texture = NULL; // nice names
	numgltextures = 1;
	next_free_texture = 0;

	GL_InitTextureState();

	// Netgraph.
	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, 1, &netgraphtexture);

	// Player skins.
	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, MAX_CLIENTS, playernmtextures);
	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, MAX_CLIENTS, playerfbtextures);

	// Lightmap.
	if (GL_ShadersSupported()) {
		GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 1, &lightmap_texture_array);

		GL_TexStorage3D(GL_TEXTURE0, lightmap_texture_array, 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT, MAX_LIGHTMAPS);
		GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL_TexParameteri(GL_TEXTURE0, lightmap_texture_array, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		for (i = 0; i < MAX_LIGHTMAPS; ++i) {
			lightmap_textures[i] = lightmap_texture_array;
		}
	}
	else {
		GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, MAX_LIGHTMAPS, lightmap_textures);

		for (i = 0; i < MAX_LIGHTMAPS; ++i) {
			GL_TexStorage2D(GL_TEXTURE0, lightmap_textures[i], 1, GL_RGBA8, LIGHTMAP_WIDTH, LIGHTMAP_HEIGHT);
			GL_TexParameterf(GL_TEXTURE0, lightmap_textures[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			GL_TexParameterf(GL_TEXTURE0, lightmap_textures[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GL_TexParameteri(GL_TEXTURE0, lightmap_textures[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GL_TexParameteri(GL_TEXTURE0, lightmap_textures[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
	}

	// Motion blur.
	GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, 1, &sceneblur_texture);

	// Powerup shells.
	GL_TextureReferenceInvalidate(shelltexture); // Force reload.

	// Particles.
	GL_TextureReferenceInvalidate(particletexture); // Force reload.

	// Sky.
	memset(skyboxtextures, 0, sizeof(skyboxtextures)); // Force reload.

	i = (cv = Cvar_Find(gl_max_size.name)) ? cv->integer : 0;

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_max_size);
	Cvar_Register(&gl_picmip);
	Cvar_Register(&gl_lerpimages);
	Cvar_Register(&gl_texturemode);
	Cvar_Register(&gl_texturemode2d);
	Cvar_Register(&gl_anisotropy);
	Cvar_Register(&gl_scaleModelTextures);
	Cvar_Register(&gl_scaleTurbTextures);
	Cvar_Register(&gl_miptexLevel);
	Cvar_Register(&gl_externalTextures_world);
	Cvar_Register(&gl_externalTextures_bmodels);
    Cvar_Register(&gl_no24bit);
	Cvar_Register(&gl_wicked_luma_level);

	Cvar_SetDefault(&gl_max_size, glConfig.gl_max_size_default);

	// This way user can specifie gl_max_size in his cfg.
	if (i) {
		Cvar_SetValue(&gl_max_size, i);
	}

	no24bit = (COM_CheckParm("-no24bit") || gl_no24bit.integer) ? true : false;
	forceTextureReload = COM_CheckParm("-forceTextureReload") ? true : false;
}

// We could flag the textures as they're created and then move all 2d>3d to this module?
texture_ref GL_CreateTextureArray(const char* identifier, int width, int height, int* depth, int mode, int minimum_depth)
{
	unsigned short crc = 0;
	qbool new_texture = false;
	gltexture_t* slot = GL_AllocateTextureSlot(GL_TEXTURE_2D_ARRAY, identifier, width, height, *depth, 4, mode | TEX_NOSCALE, 0, &new_texture);
	texture_ref gl_texturenum;
	int max_miplevels = 0;
	int min_dimension = min(width, height);

	if (!slot) {
		return invalid_texture_reference;
	}

	if (slot && !new_texture) {
		return slot->reference;
	}

	gl_texturenum = slot->reference;

	// 
	while (min_dimension > 0) {
		max_miplevels++;
		min_dimension /= 2;
	}

	GL_BindTextureUnit(GL_TEXTURE0, gl_texturenum);
	GL_ProcessErrors("Prior-texture-array-creation");
	while (*depth >= minimum_depth) {
		GLenum error;
		int array_width, array_height, array_depth;

		GL_Paranoid_Printf("Allocating %d x %d x %d, %d miplevels\n", width, height, *depth, max_miplevels);
		GL_TexStorage3D(GL_TEXTURE0, slot->reference, max_miplevels, GL_RGBA8, width, height, *depth);

		error = glGetError();
		if (error == GL_OUT_OF_MEMORY && *depth > 2) {
			*depth /= 2;
			GL_Paranoid_Printf("Array allocation failed (memory), reducing size...\n");
			continue;
		}
		else if (error != GL_NO_ERROR) {
			array_width = GL_TextureWidth(slot->reference);
			array_height = GL_TextureHeight(slot->reference);
			array_depth = GL_TextureDepth(slot->reference);

			GL_Paranoid_Printf("Array allocation failed, error %X: [mip %d, %d x %d x %d]\n", error, max_miplevels, width, height, *depth);
			GL_Paranoid_Printf(" > Sizes reported: %d x %d x %d\n", array_width, array_height, array_depth);
			gl_texturenum = invalid_texture_reference;
		}
		else {
			GL_Paranoid_Printf(" > Sizes reported: %d x %d x %d\n", array_width, array_height, array_depth);
		}
		break;
	}

	if (mode & TEX_MIPMAP) {
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		GL_TexParameterf(GL_TEXTURE0, slot->reference,GL_TEXTURE_MAG_FILTER, gl_filter_max);

		if (anisotropy_ext) {
			GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
		}
	}
	else {
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

	return gl_texturenum;
}

void GL_CreateTextures(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* textures)
{
	GLsizei i;

	for (i = 0; i < n; ++i) {
		gltexture_t* glt = GL_NextTextureSlot(target);

		GL_CreateTextureNames(textureUnit, target, 1, &glt->texnum);
		glt->target = target;
		GL_BindTextureUnit(textureUnit, glt->reference);

		textures[i] = glt->reference;
	}
}

void GL_DeleteTexture(texture_ref* texture)
{
	if (!texture || !GL_TextureReferenceIsValid(*texture)) {
		return;
	}

	// Delete OpenGL
	glDeleteTextures(1, &gltextures[texture->index].texnum);

	// Might have been bound when deleted, update state
	GL_InvalidateTextureReferences(gltextures[texture->index].texnum);

	// Free structure, updated linked list so we can re-use this slot
	Q_free(gltextures[texture->index].pathname);
	memset(&gltextures[texture->index], 0, sizeof(gltextures[0]));
	gltextures[texture->index].next_free = next_free_texture;
	next_free_texture = texture->index;

	// Invalidate the reference
	texture->index = 0;
}

void GL_DeleteCubeMap(texture_ref* texture)
{
	GL_DeleteTexture(texture);
}

// Identical to GL_DeleteTexture but for type correctness...
void GL_DeleteTextureArray(texture_ref* texture)
{
	GL_DeleteTexture(texture);
}

texture_ref GL_CreateCubeMap(const char* identifier, int width, int height, int mode)
{
	qbool new_texture;
	gltexture_t* slot = GL_AllocateTextureSlot(GL_TEXTURE_CUBE_MAP, identifier, width, height, 0, 4, mode | TEX_NOSCALE, 0, &new_texture);
	int max_miplevels = 0;
	int max_dimension;

	if (!slot) {
		return invalid_texture_reference;
	}

	if (slot && !new_texture) {
		return slot->reference;
	}

	// 
	max_dimension = max(slot->gl_width, slot->gl_height);
	while (max_dimension > 0) {
		max_miplevels++;
		max_dimension /= 2;
	}

	if (mode & TEX_MIPMAP) {
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MAG_FILTER, gl_filter_max);

		if (anisotropy_ext) {
			GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_tap);
		}
	}
	else {
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MIN_FILTER, gl_filter_max_2d);
		GL_TexParameterf(GL_TEXTURE0, slot->reference, GL_TEXTURE_MAG_FILTER, gl_filter_max_2d);
	}

	return slot->reference;
}

// For OpenGL wrapper functions
GLuint GL_TextureNameFromReference(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return gltextures[ref.index].texnum;
}

// For OpenGL wrapper functions
GLenum GL_TextureTargetFromReference(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return gltextures[ref.index].target;
}

qbool GL_TexturesAreSameSize(texture_ref tex1, texture_ref tex2)
{
	assert(tex1.index < sizeof(gltextures) / sizeof(gltextures[0]));
	assert(tex2.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return (gltextures[tex1.index].gl_width == gltextures[tex2.index].gl_width) &&
	       (gltextures[tex1.index].gl_height == gltextures[tex2.index].gl_height);
}

GLint GL_TextureWidth(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (ref.index >= numgltextures) {
		return 0;
	}

#ifdef USE_OPENGL_DIMENSIONS
	{
		GLint result;

		GL_GetTexLevelParameteriv(GL_TEXTURE0, ref, 0, GL_TEXTURE_WIDTH, &result);

		return result;
	}
#else
	return gltextures[ref.index].gl_width;
#endif
}

GLint GL_TextureHeight(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (ref.index >= numgltextures) {
		return 0;
	}

#ifdef USE_OPENGL_DIMENSIONS
	{
		GLint result;

		GL_GetTexLevelParameteriv(GL_TEXTURE0, ref, 0, GL_TEXTURE_HEIGHT, &result);

		return result;
	}
#else
	return gltextures[ref.index].gl_height;
#endif
}

GLint GL_TextureDepth(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	assert(gltextures[ref.index].target == GL_TEXTURE_2D_ARRAY);
	if (ref.index >= numgltextures || gltextures[ref.index].target != GL_TEXTURE_2D_ARRAY) {
		return 0;
	}

#ifdef USE_OPENGL_DIMENSIONS
	{
		GLint result;

		GL_GetTexLevelParameteriv(GL_TEXTURE0, ref, 0, GL_TEXTURE_DEPTH, &result);

		return result;
	}
#else
	return gltextures[ref.index].depth;
#endif
}

void GL_GenerateMipmapsIfNeeded(texture_ref ref)
{
	assert(ref.index && ref.index < numgltextures);
	if (!ref.index || ref.index >= numgltextures) {
		return;
	}

	if (!gltextures[ref.index].mipmaps_generated) {
		GL_GenerateMipmap(GL_TEXTURE0, ref);
		gltextures[ref.index].mipmaps_generated = true;
	}
}

void GL_TextureReplace2D(
	GLenum textureUnit, GLenum target, texture_ref* ref, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid *pixels
)
{
	gltexture_t* tex;

	if (!GL_TextureReferenceIsValid(*ref)) {
		return;
	}

	tex = &gltextures[ref->index];
	if (tex->gl_width != width || tex->gl_height != height || tex->target != target || tex->internal_format != internalformat) {
		gltexture_t old = *tex;
		GL_DeleteTexture(ref);
		*ref = GL_LoadTexturePixels(pixels, old.identifier, width, height, old.texmode);
	}
	else {
		GL_TexSubImage2D(textureUnit, *ref, 0, 0, 0, width, height, format, type, pixels);
	}
}
