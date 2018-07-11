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
#include "r_texture.h"
#include "r_texture_internal.h"
#include "r_brushmodel_sky.h"
#include "r_local.h"
#include "r_aliasmodel.h" // for shelltexture only
#include "r_lighting.h"

const texture_ref null_texture_reference = { 0 };
GLenum gl_solid_format = GL_RGB8, gl_alpha_format = GL_RGBA8;

static const texture_ref invalid_texture_reference = { 0 };

extern unsigned d_8to24table2[256];

extern int anisotropy_ext;

static GLint glTextureMinificationOptions[texture_minification_count] = {
	GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
};

static GLint glTextureMagnificationOptions[texture_magnification_count] = {
	GL_NEAREST, GL_LINEAR
};

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

static int GL_StorageFormat(int mode)
{
	if (gl_gammacorrection.integer) {
		return (mode & TEX_ALPHA) ? GL_SRGB8_ALPHA8 : GL_SRGB8;
	}
	else {
		return (mode & TEX_ALPHA) ? GL_RGBA8 : GL_RGB8;
	}
}

//
// Uploads a 32-bit texture to OpenGL. Makes sure it's the correct size and creates mipmaps if requested.
//
static void GL_Upload32(gltexture_t* glt, unsigned *data, int width, int height, int mode)
{
	// Tell OpenGL the texnum of the texture before uploading it.
	extern cvar_t gl_lerpimages, gl_wicked_luma_level;
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
	if (width < tempwidth || height < tempheight) {
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
				// make black pixels transparent, well not always black, depends of level...
				bdata[i] = bdata[i + 1] = bdata[i + 2] = bdata[i + 3] = 0;
			}
		}
	}

	// Get the scaled dimension (scales according to gl_picmip and max allowed texture size).
	R_TextureUtil_ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	// If the image size is bigger than the max allowed size or 
	// set picmip value we calculate it's next closest mip map.
	while (width > tempwidth || height > tempheight) {
		Image_MipReduce((byte *)newdata, (byte *)newdata, &width, &height, 4);
	}

	glt->texture_width = width;
	glt->texture_height = height;

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
		R_TextureUtil_Brighten32((byte *)newdata, width * height * 4);
	}

	// Upload the main texture to OpenGL.
	GL_TexSubImage2D(GL_TEXTURE0, glt->reference, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, newdata);

	if (mode & TEX_MIPMAP) {
		GL_GenerateMipmapWithData(GL_TEXTURE0, glt->reference, (byte*)newdata, width, height, GL_InternalFormat(glt->texmode));
	}

	R_TextureUtil_SetFiltering(glt->reference);

	Q_free(newdata);
}

static void GL_Upload8(gltexture_t* glt, byte *data, int width, int height, int mode)
{
	static unsigned trans[640 * 480];
	int	i, image_size, p;
	unsigned *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > sizeof(trans)) {
		Sys_Error("GL_Upload8: image too big");
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

	GL_Upload32(glt, trans, width, height, mode & ~TEX_BRIGHTEN);
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
	unsigned short crc = identifier[0] && data ? CRC_Block(data, width * height * bpp) : 0;
	qbool new_texture = false;
	gltexture_t *glt = GL_AllocateTextureSlot(GL_TEXTURE_2D, identifier, width, height, 0, bpp, mode, crc, &new_texture);

	if (glt && !new_texture) {
		return glt->reference;
	}

	GL_LoadTextureData(glt, width, height, data, mode, bpp);

	return glt->reference;
}

void GL_CreateTexturesWithIdentifier(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* textures, const char* identifier)
{
	GLsizei i;

	for (i = 0; i < n; ++i) {
		gltexture_t* glt = GL_NextTextureSlot(target);

		GL_CreateTextureNames(textureUnit, target, 1, &glt->gl.texnum);
		glt->gl.target = target;
		GL_BindTextureUnit(textureUnit, glt->reference);

		textures[i] = glt->reference;

		if (identifier) {
			strlcpy(glt->identifier, identifier, sizeof(glt->identifier));
			GL_ObjectLabel(GL_TEXTURE, glt->gl.texnum, -1, identifier);
		}
	}
}

void GL_CreateTextures(GLenum textureUnit, GLenum target, GLsizei n, texture_ref* textures)
{
	GL_CreateTexturesWithIdentifier(textureUnit, target, n, textures, NULL);
}

void GL_CreateTexture2D(texture_ref* reference, int width, int height, const char* name)
{
	GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D, 1, reference, name);
	GL_TexStorage2D(GL_TEXTURE0, *reference, 1, GL_RGBA8, width, height);
	GL_SetTextureFiltering(*reference, texture_minification_linear, texture_magnification_linear);
	GL_TexParameteri(GL_TEXTURE0, *reference, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	GL_TexParameteri(GL_TEXTURE0, *reference, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void GL_AllocateTextureReferences(GLenum target, int width, int height, int mode, GLsizei number, texture_ref* references)
{
	GLsizei i;

	for (i = 0; i < number; ++i) {
		qbool new_texture;
		gltexture_t* slot = GL_AllocateTextureSlot(target, "", width, height, 0, 4, mode | TEX_NOSCALE, 0, &new_texture);

		if (slot) {
			references[i] = slot->reference;
		}
		else {
			references[i] = null_texture_reference;
		}
	}
}

// For OpenGL wrapper functions
GLuint GL_TextureNameFromReference(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return gltextures[ref.index].gl.texnum;
}

// For OpenGL wrapper functions
GLenum GL_TextureTargetFromReference(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return gltextures[ref.index].gl.target;
}

const char* GL_TextureIdentifierByGLReference(GLuint texnum)
{
	static char name[256];

	GL_GetObjectLabel(GL_TEXTURE, texnum, sizeof(name), NULL, name);

	return name;
}

void GL_TextureReplace2D(
	GLenum textureUnit, GLenum target, texture_ref* ref, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid *pixels
)
{
	gltexture_t* tex;

	if (!GL_TextureReferenceIsValid(*ref)) {
		GL_CreateTextures(GL_TEXTURE0, GL_TEXTURE_2D, 1, ref);
	}

	tex = &gltextures[ref->index];
	if (tex->texture_width != width || tex->texture_height != height || tex->gl.target != target || GL_InternalFormat(tex->texmode) != internalformat) {
		gltexture_t old = *tex;
		R_DeleteTexture(ref);
		*ref = GL_LoadTexturePixels(pixels, old.identifier, width, height, old.texmode);
	}
	else {
		GL_TexSubImage2D(textureUnit, *ref, 0, 0, 0, width, height, format, type, pixels);
	}
}

void GL_SetTextureFiltering(texture_ref texture, texture_minification_id minification_filter, texture_magnification_id magnification_filter)
{
	assert(minification_filter >= 0 && minification_filter < texture_minification_count);
	assert(magnification_filter >= 0 && magnification_filter < texture_magnification_count);

	if (!GL_TextureReferenceIsValid(texture)) {
		return;
	}

	GL_TexParameteri(GL_TEXTURE0, texture, GL_TEXTURE_MIN_FILTER, glTextureMinificationOptions[minification_filter]);
	GL_TexParameteri(GL_TEXTURE0, texture, GL_TEXTURE_MAG_FILTER, glTextureMagnificationOptions[magnification_filter]);
}

void GL_TextureWrapModeClamp(texture_ref tex)
{
	if (GL_VersionAtLeast(1, 2)) {
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else {
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
}

void GL_SetTextureAnisotropy(texture_ref texture, int anisotropy)
{
	if (anisotropy_ext) {
		GL_TexParameterf(GL_TEXTURE0, texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	}
}

void GL_DeleteTexture(texture_ref texture)
{
	glDeleteTextures(1, &gltextures[texture.index].gl.texnum);

	// Might have been bound when deleted, update state
	GL_InvalidateTextureReferences(gltextures[texture.index].gl.texnum);
}

void GL_AllocateStorage(gltexture_t* texture)
{
	GL_TexStorage2D(GL_TEXTURE0, texture->reference, texture->miplevels, GL_StorageFormat(texture->texmode), texture->texture_width, texture->texture_height);
}

qbool GL_AllocateTextureArrayStorage(gltexture_t* slot, int minimum_depth, int* depth)
{
	GLenum error;

	while ((error = glGetError()) != GL_NO_ERROR) {
		Com_Printf("Prior-texture-array-creation: OpenGL error %u\n", error);
	}
	while (*depth >= minimum_depth) {
		GL_Paranoid_Printf("Allocating %d x %d x %d, %d miplevels\n", width, height, *depth, max_miplevels);
		GL_TexStorage3D(GL_TEXTURE0, slot->reference, slot->miplevels, GL_StorageFormat(TEX_ALPHA), slot->texture_width, slot->texture_height, *depth);

		error = glGetError();
		if (error == GL_OUT_OF_MEMORY && *depth > 2) {
			*depth /= 2;
			GL_Paranoid_Printf("Array allocation failed (memory), reducing size...\n");
			continue;
		}
		else if (error != GL_NO_ERROR) {
#ifdef GL_PARANOIA
			int array_width, array_height, array_depth;
			array_width = R_TextureWidth(slot->reference);
			array_height = R_TextureHeight(slot->reference);
			array_depth = R_TextureDepth(slot->reference);

			GL_Paranoid_Printf("Array allocation failed, error %X: [mip %d, %d x %d x %d]\n", error, max_miplevels, width, height, *depth);
			GL_Paranoid_Printf(" > Sizes reported: %d x %d x %d\n", array_width, array_height, array_depth);
#endif
			return false;
		}
		break;
	}
	return true;
}