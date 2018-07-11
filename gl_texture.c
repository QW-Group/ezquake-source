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
#include "gl_texture_internal.h"

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
static GLenum glTextureTargetForType[texture_type_count] = {
	GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY
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

void GL_UploadTexture(texture_ref texture, int mode, int width, int height, byte* newdata)
{
	// Upload the main texture to OpenGL.
	GL_TexSubImage2D(GL_TEXTURE0, texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
	if (mode & TEX_MIPMAP) {
		GL_GenerateMipmapWithData(GL_TEXTURE0, texture, (byte*)newdata, width, height, GL_InternalFormat(mode));
	}
}

void GL_CreateTexturesWithIdentifier(r_texture_type_id type, int n, texture_ref* textures, const char* identifier)
{
	GLenum textureUnit = GL_TEXTURE0;
	GLenum target = glTextureTargetForType[type];
	GLsizei i;

	for (i = 0; i < n; ++i) {
		gltexture_t* glt = GL_NextTextureSlot(type);

		GL_CreateTextureNames(textureUnit, target, 1, &glt->texnum);
		glt->type = type;
		GL_BindTextureUnit(textureUnit, glt->reference);

		textures[i] = glt->reference;

		if (identifier) {
			strlcpy(glt->identifier, identifier, sizeof(glt->identifier));
			GL_ObjectLabel(GL_TEXTURE, glt->texnum, -1, identifier);
		}
	}
}

void GL_CreateTextures(r_texture_type_id type, int n, texture_ref* textures)
{
	GL_CreateTexturesWithIdentifier(type, n, textures, NULL);
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

	return gltextures[ref.index].texnum;
}

// For OpenGL wrapper functions
GLenum GL_TextureTargetFromReference(texture_ref ref)
{
	assert(ref.index < sizeof(gltextures) / sizeof(gltextures[0]));

	return glTextureTargetForType[gltextures[ref.index].type];
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

	if (!R_TextureReferenceIsValid(*ref)) {
		GL_CreateTextures(texture_type_2d, 1, ref);
	}

	tex = &gltextures[ref->index];
	if (tex->texture_width != width || tex->texture_height != height || glTextureTargetForType[tex->type] != target || GL_InternalFormat(tex->texmode) != internalformat) {
		gltexture_t old = *tex;
		R_DeleteTexture(ref);
		*ref = R_LoadTexturePixels(pixels, old.identifier, width, height, old.texmode);
	}
	else {
		GL_TexSubImage2D(textureUnit, *ref, 0, 0, 0, width, height, format, type, pixels);
	}
}

void GL_SetTextureFiltering(texture_ref texture, texture_minification_id minification_filter, texture_magnification_id magnification_filter)
{
	assert(minification_filter >= 0 && minification_filter < texture_minification_count);
	assert(magnification_filter >= 0 && magnification_filter < texture_magnification_count);

	if (!R_TextureReferenceIsValid(texture)) {
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
	glDeleteTextures(1, &gltextures[texture.index].texnum);

	// Might have been bound when deleted, update state
	GL_InvalidateTextureReferences(gltextures[texture.index].texnum);
}

void GL_AllocateStorage(gltexture_t* texture)
{
	GL_TexStorage2D(texture->reference, texture->miplevels, GL_StorageFormat(texture->texmode), texture->texture_width, texture->texture_height);
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

void GL_AllocateTextureNames(gltexture_t* glt)
{
	GL_CreateTextureNames(GL_TEXTURE0, glTextureTargetForType[glt->type], 1, &glt->texnum);
}

void GL_CreateTexture2D(texture_ref* texture, int width, int height, const char* name)
{
	GL_CreateTexturesWithIdentifier(texture_type_2d, 1, texture, name);
	GL_TexStorage2D(*texture, 1, GL_RGBA8, width, height);
	GL_SetTextureFiltering(*texture, texture_minification_linear, texture_magnification_linear);
	GL_TextureWrapModeClamp(*texture);
}
