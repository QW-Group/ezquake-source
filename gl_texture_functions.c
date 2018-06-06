/*
Copyright (C) 2018 ezQuake team

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

// Wrapper functions over newer OpenGL texture management functions

#include <SDL.h>

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "image.h"
#include "tr_types.h"
#include "r_texture_internal.h"
#include "gl_texture_internal.h"
#include "r_trace.h"
#include "r_renderer.h"

GLuint GL_TextureNameFromReference(texture_ref ref);
GLenum GL_TextureTargetFromReference(texture_ref ref);

// <DSA-functions (4.5)>
// These allow modification of textures without binding (-bind-to-edit)
typedef void (APIENTRY *glCreateTextures_t)(GLenum target, GLsizei n, GLuint* textures);
typedef void (APIENTRY *glGetnTexImage_t)(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels);
typedef void (APIENTRY *glGetTextureImage_t)(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
typedef void (APIENTRY *glGetTextureLevelParameterfv_t)(GLuint texture, GLint level, GLenum pname, GLfloat *params);
typedef void (APIENTRY *glGetTextureLevelParameteriv_t)(GLuint texture, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRY *glTextureParameteri_t)(GLuint texture, GLenum pname, GLint param);
typedef void (APIENTRY *glTextureParameterf_t)(GLuint texture, GLenum pname, GLfloat param);
typedef void (APIENTRY *glTextureParameterfv_t)(GLuint texture, GLenum pname, const GLfloat *params);
typedef void (APIENTRY *glTextureParameteriv_t)(GLuint texture, GLenum pname, const GLint *params);
typedef void (APIENTRY *glGenerateTextureMipmap_t)(GLuint texture);
typedef void (APIENTRY *glTextureStorage3D_t)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *glTextureStorage2D_t)(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glTextureSubImage2D_t)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *glTextureSubImage3D_t)(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
typedef void (APIENTRY *glTexSubImage3D_t)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
typedef void (APIENTRY *glTexStorage2D_t)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY *glTexStorage3D_t)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth);
typedef void (APIENTRY *glGenerateMipmap_t)(GLenum target);

typedef void (APIENTRY *glSamplerParameterf_t)(GLuint sampler, GLenum pname, GLfloat param);
typedef void (APIENTRY *glGenSamplers_t)(GLsizei n, GLuint* samplers);
typedef void (APIENTRY *glDeleteSamplers_t)(GLsizei n, const GLuint* samplers);
typedef void (APIENTRY *glBindSampler_t)(GLuint unit, GLuint sampler);

static glGetTextureLevelParameterfv_t qglGetTextureLevelParameterfv;
static glGetTextureLevelParameteriv_t qglGetTextureLevelParameteriv;
static glGenerateTextureMipmap_t qglGenerateTextureMipmap;
static glGetTextureImage_t qglGetTextureImage;
static glCreateTextures_t qglCreateTextures;
static glGetnTexImage_t qglGetnTexImage;
static glTextureParameteri_t qglTextureParameteri;
static glTextureParameterf_t qglTextureParameterf;
static glTextureParameterfv_t qglTextureParameterfv;
static glTextureParameteriv_t qglTextureParameteriv;
static glTextureStorage3D_t qglTextureStorage3D;
static glTextureStorage2D_t qglTextureStorage2D;
static glTextureSubImage2D_t qglTextureSubImage2D;
static glTextureSubImage3D_t qglTextureSubImage3D;

static glTexStorage2D_t qglTexStorage2D;
static glTexSubImage3D_t qglTexSubImage3D;
static glTexStorage3D_t qglTexStorage3D;
static glGenerateMipmap_t qglGenerateMipmap;

static glGenSamplers_t qglGenSamplers;
static glDeleteSamplers_t qglDeleteSamplers;
static glSamplerParameterf_t qglSamplerParameterf;
static glBindSampler_t qglBindSampler;

qbool GLM_LoadTextureManagementFunctions(void)
{
	qbool all_available = true;

	GL_LoadMandatoryFunctionExtension(glTexSubImage3D, all_available);
	GL_LoadMandatoryFunctionExtension(glTexStorage2D, all_available);
	GL_LoadMandatoryFunctionExtension(glTexStorage3D, all_available);
	GL_LoadMandatoryFunctionExtension(glGenerateMipmap, all_available);

	GL_LoadMandatoryFunctionExtension(glGenSamplers, all_available);
	GL_LoadMandatoryFunctionExtension(glDeleteSamplers, all_available);
	GL_LoadMandatoryFunctionExtension(glSamplerParameterf, all_available);
	GL_LoadMandatoryFunctionExtension(glBindSampler, all_available);

	return all_available;
}

void GL_LoadTextureManagementFunctions(void)
{
	if (GL_UseDirectStateAccess()) {
		GL_LoadOptionalFunction(glGenerateTextureMipmap);
		GL_LoadOptionalFunction(glGetTextureImage);
		GL_LoadOptionalFunction(glCreateTextures);
		GL_LoadOptionalFunction(glTextureParameteri);
		GL_LoadOptionalFunction(glTextureParameterf);
		GL_LoadOptionalFunction(glTextureParameterfv);
		GL_LoadOptionalFunction(glTextureParameteriv);
		GL_LoadOptionalFunction(glTextureStorage3D);
		GL_LoadOptionalFunction(glTextureStorage2D);
		GL_LoadOptionalFunction(glTextureSubImage2D);
		GL_LoadOptionalFunction(glTextureSubImage3D);
		GL_LoadOptionalFunction(glGetTextureLevelParameterfv);
		GL_LoadOptionalFunction(glGetTextureLevelParameterfv);
		GL_LoadOptionalFunction(glGetTextureLevelParameteriv);
	}

	if (GL_VersionAtLeast(4, 5)) {
		GL_LoadOptionalFunction(glGetnTexImage);
	}
}

void GL_TexSubImage3D(
	int unit, texture_ref texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
	GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels
)
{
	if (qglTextureSubImage3D) {
		qglTextureSubImage3D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	else {
		GLenum textureUnit = GL_TEXTURE0 + unit;
		GLenum target = GL_TextureTargetFromReference(texture);

		renderer.TextureUnitBind(textureUnit, texture);
		qglTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	R_TraceLogAPICall("GL_TexSubImage3D(unit=GL_TEXTURE%d, texture=%u)", unit, GL_TextureNameFromReference(texture));
}

void GL_TexSubImage2D(
	GLenum textureUnit, texture_ref texture, GLint level,
	GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels
)
{
	if (qglTextureSubImage2D) {
		qglTextureSubImage2D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, width, height, format, type, pixels);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	}
	R_TraceLogAPICall("GL_TexSubImage2D(unit=GL_TEXTURE%d, texture=%u)", textureUnit - GL_TEXTURE0, GL_TextureNameFromReference(texture));
}

void GL_TexStorage2DImpl(GLenum textureUnit, GLenum target, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	if (qglTextureStorage2D) {
		qglTextureStorage2D(texture, levels, internalformat, width, height);
	}
	else {
		glBindTexture(textureUnit, target);
		if (qglTexStorage2D) {
			qglTexStorage2D(target, levels, internalformat, width, height);
		}
		else {
			int level;
			for (level = 0; level < levels; ++level) {
				glTexImage2D(target, level, internalformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				width = max(1, width / 2);
				height = max(1, height / 2);
			}
		}
		GL_BindTextureUnit(textureUnit, null_texture_reference);
	}
}

void GL_TexStorage2D(
	texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height
)
{
	if (qglTextureStorage2D) {
		qglTextureStorage2D(GL_TextureNameFromReference(texture), levels, internalformat, width, height);
	}
	else {
		GLenum textureUnit = GL_TEXTURE0;
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (qglTexStorage2D) {
			qglTexStorage2D(target, levels, internalformat, width, height);
		}
		else {
			int level;
			GLsizei level_width = width;
			GLsizei level_height = height;

			for (level = 0; level < levels; ++level) {
				glTexImage2D(target, level, internalformat, level_width, level_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				level_width = max(1, level_width / 2);
				level_height = max(1, level_height / 2);
			}
		}
	}
	R_TextureSetDimensions(texture, width, height);
}

void GL_TexStorage3D(
	GLenum textureUnit, texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth
)
{
	if (qglTextureStorage3D) {
		qglTextureStorage3D(GL_TextureNameFromReference(texture), levels, internalformat, width, height, depth);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		qglTexStorage3D(target, levels, internalformat, width, height, depth);
	}
}

void GL_TexParameterf(GLenum textureUnit, texture_ref texture, GLenum pname, GLfloat param)
{
	if (qglTextureParameterf) {
		qglTextureParameterf(GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameterf(target, pname, param);
	}
}

void GL_TexParameterfv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLfloat *params)
{
	if (qglTextureParameterfv) {
		qglTextureParameterfv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameterfv(target, pname, params);
	}
}

void GL_TexParameteri(GLenum textureUnit, texture_ref texture, GLenum pname, GLint param)
{
	if (qglTextureParameteri) {
		qglTextureParameteri(GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteri(target, pname, param);
	}
}

void GL_TexParameteriv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLint *params)
{
	if (qglTextureParameteriv) {
		qglTextureParameteriv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteriv(target, pname, params);
	}
}

void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures)
{
	if (qglCreateTextures) {
		qglCreateTextures(target, n, textures);
	}
	else {
		int i;

		glGenTextures(n, textures);
		for (i = 0; i < n; ++i) {
			GL_SelectTexture(textureUnit);
			glBindTexture(target, textures[i]);
		}
	}
}

void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref texture, GLint level, GLenum pname, GLint* params)
{
	if (qglGetTextureLevelParameteriv) {
		qglGetTextureLevelParameteriv(GL_TextureNameFromReference(texture), level, pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glGetTexLevelParameteriv(target, level, pname, params);
	}
}

void GL_GetTexImage(GLenum textureUnit, texture_ref texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer)
{
	// TODO: Use glGetnTexImage() if available (4.5)
	if (qglGetTextureImage) {
		qglGetTextureImage(GL_TextureNameFromReference(texture), level, format, type, bufSize, buffer);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (qglGetnTexImage) {
			qglGetnTexImage(target, level, format, type, bufSize, buffer);
		}
		else {
			glGetTexImage(target, level, format, type, buffer);
		}
	}
}

void GL_TextureMipmapGenerateWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format)
{
	if (qglGenerateTextureMipmap) {
		qglGenerateTextureMipmap(GL_TextureNameFromReference(texture));
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (qglGenerateMipmap) {
			qglGenerateMipmap(target);
		}
		else if (newdata) {
			int miplevel = 0;

			// Calculate the mip maps for the images.
			while (width > 1 || height > 1) {
				Image_MipReduce((byte *)newdata, (byte *)newdata, &width, &height, 4);
				miplevel++;
				GL_TexSubImage2D(GL_TEXTURE0, texture, miplevel, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
			}
		}
	}
}

void GL_TextureMipmapGenerate(texture_ref texture)
{
	GL_TextureMipmapGenerateWithData(GL_TEXTURE0, texture, NULL, 0, 0, 0);
}

// Samplers
static unsigned int nearest_sampler;
static unsigned int linear_sampler;

void GLM_SamplerSetNearest(unsigned int texture_unit_number)
{
	if (!nearest_sampler) {
		qglGenSamplers(1, &nearest_sampler);

		qglSamplerParameterf(nearest_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglSamplerParameterf(nearest_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	qglBindSampler(texture_unit_number, nearest_sampler);
}

void GLM_SamplerSetLinear(unsigned int texture_unit_number)
{
	if (!linear_sampler) {
		qglGenSamplers(1, &linear_sampler);

		qglSamplerParameterf(linear_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglSamplerParameterf(linear_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	qglBindSampler(texture_unit_number, linear_sampler);
}

void GLM_SamplerClear(unsigned int texture_unit_number)
{
	qglBindSampler(texture_unit_number, 0);
}

void GL_DeleteSamplers(void)
{
	if (qglDeleteSamplers) {
		qglDeleteSamplers(1, &nearest_sampler);
	}
	nearest_sampler = 0;
}

// meag: *RGBA is bit ugly, just trying to keep OpenGL enums out of the way for now
void GL_TextureReplaceSubImageRGBA(texture_ref ref, int offsetx, int offsety, int width, int height, byte* buffer)
{
	GL_TexSubImage2D(GL_TEXTURE0, ref, 0, offsetx, offsety, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
}
