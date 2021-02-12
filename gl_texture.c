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
#include "r_renderer.h"
#include "tr_types.h"
#include "r_texture.h"
#include "r_texture_internal.h"
#include "r_brushmodel_sky.h"
#include "r_local.h"
#include "r_aliasmodel.h" // for shelltexture only
#include "r_lighting.h"
#include "gl_texture_internal.h"

const texture_ref null_texture_reference = { 0 };
static GLenum gl_solid_format = GL_RGB8, gl_alpha_format = GL_RGBA8;

extern unsigned d_8to24table2[256];

extern int anisotropy_ext;

static GLint glTextureMinificationOptions[] = {
	GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR
};
static GLint glTextureMagnificationOptions[] = {
	GL_NEAREST, GL_LINEAR
};
static GLenum glTextureTargetForType[] = {
	GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_CUBE_MAP
};

#ifdef C_ASSERT
C_ASSERT(sizeof(glTextureMinificationOptions) / sizeof(glTextureMinificationOptions[0]) == texture_minification_count);
C_ASSERT(sizeof(glTextureMagnificationOptions) / sizeof(glTextureMagnificationOptions[0]) == texture_magnification_count);
C_ASSERT(sizeof(glTextureTargetForType) / sizeof(glTextureTargetForType[0]) == texture_type_count);
#endif

static int GL_InternalFormat(int mode)
{
	if (vid_gammacorrection.integer) {
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
	if (mode & TEX_FLOAT) {
		return (mode & TEX_ALPHA) ? GL_RGBA16F : GL_RGB16F;
	}
	else if (vid_gammacorrection.integer) {
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
		GL_TextureMipmapGenerateWithData(GL_TEXTURE0, texture, (byte*)newdata, width, height, GL_InternalFormat(mode));
	}
}

void GL_CreateTexturesWithIdentifier(r_texture_type_id type, int n, texture_ref* textures, const char* identifier)
{
	GLenum textureUnit = GL_TEXTURE0;
	GLenum target = glTextureTargetForType[type];
	GLsizei i;

	for (i = 0; i < n; ++i) {
		gltexture_t* glt = R_NextTextureSlot(type);

		GL_CreateTextureNames(textureUnit, target, 1, &glt->texnum);
		glt->type = type;
		GL_BindTextureUnit(textureUnit, glt->reference);

		textures[i] = glt->reference;

		if (identifier) {
			strlcpy(glt->identifier, identifier, sizeof(glt->identifier));
			GL_TraceObjectLabelSet(GL_TEXTURE, glt->texnum, -1, identifier);
		}
	}
}

void GL_TexturesCreate(r_texture_type_id type, int n, texture_ref* textures)
{
	GL_CreateTexturesWithIdentifier(type, n, textures, NULL);
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

	memset(name, 0, sizeof(name));
	GL_TraceObjectLabelGet(GL_TEXTURE, texnum, sizeof(name), NULL, name);
	if (!name[0]) {
		R_TextureFindIdentifierByReference(texnum, name, sizeof(name));
	}

	return name;
}

void GL_TextureReplace2D(
	GLenum textureUnit, GLenum target, texture_ref* ref, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid *pixels
)
{
	gltexture_t* tex;

	if (!R_TextureReferenceIsValid(*ref)) {
		renderer.TexturesCreate(texture_type_2d, 1, ref);
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

void GL_TextureSetFiltering(texture_ref texture, texture_minification_id minification_filter, texture_magnification_id magnification_filter)
{
	assert(minification_filter >= 0 && minification_filter < texture_minification_count);
	assert(magnification_filter >= 0 && magnification_filter < texture_magnification_count);

	if (!R_TextureReferenceIsValid(texture)) {
		return;
	}

	gltextures[texture.index].minification_filter = minification_filter;
	gltextures[texture.index].magnification_filter = magnification_filter;

	GL_TexParameteri(GL_TEXTURE0, texture, GL_TEXTURE_MIN_FILTER, glTextureMinificationOptions[minification_filter]);
	GL_TexParameteri(GL_TEXTURE0, texture, GL_TEXTURE_MAG_FILTER, glTextureMagnificationOptions[magnification_filter]);
}

void GL_TextureWrapModeClamp(texture_ref tex)
{
	if (GL_VersionAtLeast(1, 2)) {
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (R_TextureType(tex) == texture_type_cubemap) {
			GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		}
	}
	else {
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_S, GL_CLAMP);
		GL_TexParameteri(GL_TEXTURE0, tex, GL_TEXTURE_WRAP_T, GL_CLAMP);
	}
}

void GL_TextureSetAnisotropy(texture_ref texture, int anisotropy)
{
	if (anisotropy_ext) {
		GL_TexParameterf(GL_TEXTURE0, texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
	}
}

void GL_TextureDelete(texture_ref texture)
{
	gltexture_t* slot = &gltextures[texture.index];
	GLuint texture_num = slot->texnum;

#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nopengl-texture,free,%u,%d,%d,%d,%s\n", texture.index, slot->texture_width, slot->texture_height, slot->texture_width * slot->texture_height * max(slot->depth,1) * slot->bpp, slot->identifier);
#endif

	GL_BuiltinProcedure(glDeleteTextures, "n=%d, textures=%p", 1, &texture_num);

	// Might have been bound when deleted, update state
	GL_InvalidateTextureReferences(texture_num);
	slot->texnum = 0;
}

void GL_AllocateStorage(gltexture_t* texture)
{
	GL_TexStorage2D(texture->reference, texture->miplevels, GL_StorageFormat(texture->texmode), texture->texture_width, texture->texture_height, false);
#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nopengl-texture,alloc,%u,%d,%d,%d,%s\n", texture->reference.index, texture->texture_width, texture->texture_height, texture->texture_width * texture->texture_height * texture->bpp, texture->identifier);
#endif
}

#ifdef RENDERER_OPTION_MODERN_OPENGL
qbool GLM_TextureAllocateArrayStorage(gltexture_t* slot)
{
	GLenum error;

	R_TraceAPI("Allocating %d x %d x %d, %d miplevels\n", slot->texture_width, slot->texture_height, slot->depth, slot->miplevels);
	GL_ProcessErrors("GLM_TextureAllocateArrayStorage flush");
	error = GL_TexStorage3D(GL_TEXTURE0, slot->reference, slot->miplevels, GL_StorageFormat(TEX_ALPHA), slot->texture_width, slot->texture_height, slot->depth, false);

	if (error != GL_NO_ERROR) {
#ifdef WITH_RENDERING_TRACE
		int array_width, array_height, array_depth;

		array_width = R_TextureWidth(slot->reference);
		array_height = R_TextureHeight(slot->reference);
		array_depth = R_TextureDepth(slot->reference);

		R_TraceAPI("!!ERROR Array allocation failed, error %X: [mip %d, %d x %d x %d]\n", error, slot->miplevels, slot->texture_width, slot->texture_height, slot->depth);
		R_TraceAPI(" > Sizes reported: %d x %d x %d\n", array_width, array_height, array_depth);
#endif
		return false;
	}

#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nopengl-texture,alloc,%u,%d,%d,%d,%s\n", slot->reference.index, slot->texture_width, slot->texture_height, slot->texture_width * slot->texture_height * slot->depth * (slot->texmode & TEX_ALPHA ? 4 : 3), slot->identifier);
#endif

	return true;
}
#endif // RENDERER_OPTION_MODERN_OPENGL

void GL_AllocateTextureNames(gltexture_t* glt)
{
	assert(glt->type >= 0 && glt->type < sizeof(glTextureTargetForType) / sizeof(glTextureTargetForType[0]));

	GL_CreateTextureNames(GL_TEXTURE0, glTextureTargetForType[glt->type], 1, &glt->texnum);
}

void GL_TextureCreate2D(texture_ref* texture, int width, int height, const char* name, qbool is_lightmap)
{
	GL_CreateTexturesWithIdentifier(texture_type_2d, 1, texture, name);
	GL_TexStorage2D(*texture, 1, GL_RGBA8, width, height, is_lightmap);
	renderer.TextureSetFiltering(*texture, texture_minification_linear, texture_magnification_linear);
	GL_TextureWrapModeClamp(*texture);
#ifdef DEBUG_MEMORY_ALLOCATIONS
	Sys_Printf("\nopengl-texture,alloc,%u,%d,%d,%d,%s\n", texture->index, width, height, width * height * 4, name);
#endif
}

void GL_TextureCompressionSet(qbool enabled)
{
	gl_alpha_format = (enabled ? GL_COMPRESSED_RGBA_ARB : GL_RGBA8);
	gl_solid_format = (enabled ? GL_COMPRESSED_RGB_ARB : GL_RGB8);
}

void GL_TextureGet(texture_ref texture, int buffer_size, byte* buffer, int bpp)
{
	GL_GetTexImage(GL_TEXTURE0, texture, 0, bpp == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, buffer_size, buffer);
}

void GL_TextureLoadCubemapFace(texture_ref cubemap, r_cubemap_direction_id direction, const byte* data, int width, int height)
{
	GL_TexSubImage3D(0, cubemap, 0, 0, 0, direction, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
}
