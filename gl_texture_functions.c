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

typedef void (APIENTRY *glGetInternalformativ_t)(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params);

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

static glGetInternalformativ_t qglGetInternalformativ;

void GL_LoadTextureManagementFunctions(void)
{
	glConfig.supported_features &= ~(R_SUPPORT_TEXTURE_ARRAYS | R_SUPPORT_TEXTURE_SAMPLERS);

	glConfig.preferred_format = glConfig.preferred_type = 0;
	if (GL_VersionAtLeast(4, 3) || SDL_GL_ExtensionSupported("GL_ARB_internalformat_query2")) {
		GL_LoadOptionalFunction(glGetInternalformativ);

		if (qglGetInternalformativ) {
			GLint tempFormat = 0, tempType = 0;

			qglGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_TEXTURE_IMAGE_FORMAT, 1, &tempFormat);
			qglGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_TEXTURE_IMAGE_TYPE, 1, &tempType);

			glConfig.preferred_format = tempFormat;
			glConfig.preferred_type = tempType;
		}
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_texture_storage")) {
		GL_LoadOptionalFunction(glTexStorage2D);
		GL_LoadOptionalFunction(glTexStorage3D);
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_sampler_objects")) {
		qbool all_available = true;

		// OpenGL 1.2
		GL_LoadMandatoryFunctionExtension(glTexSubImage3D, all_available);

		glConfig.supported_features |= (all_available ? R_SUPPORT_TEXTURE_ARRAYS : 0);

		// Texture samplers in 3.3
		all_available = true;
		GL_LoadMandatoryFunctionExtension(glGenSamplers, all_available);
		GL_LoadMandatoryFunctionExtension(glDeleteSamplers, all_available);
		GL_LoadMandatoryFunctionExtension(glSamplerParameterf, all_available);
		GL_LoadMandatoryFunctionExtension(glBindSampler, all_available);
		glConfig.supported_features |= (all_available ? R_SUPPORT_TEXTURE_SAMPLERS : 0);
	}

	if (GL_VersionAtLeast(3, 0)) {
		// Core in 3.0
		GL_LoadOptionalFunction(glGenerateMipmap);
	}
	else if (SDL_GL_ExtensionSupported("GL_EXT_framebuffer_object")) {
		qglGenerateMipmap = (glGenerateMipmap_t)SDL_GL_GetProcAddress("glGenerateMipmap");
		if (!qglGenerateMipmap) {
			qglGenerateMipmap = (glGenerateMipmap_t)SDL_GL_GetProcAddress("glGenerateMipmapEXT");
		}
	}

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

	if (GL_VersionAtLeast(3, 2) || SDL_GL_ExtensionSupported("GL_ARB_seamless_cube_map")) {
		glConfig.supported_features |= R_SUPPORT_SEAMLESS_CUBEMAPS;
	}
}

void GL_TexSubImage3D(
	int unit, texture_ref texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
	GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels
)
{
	GLenum target = GL_TextureTargetFromReference(texture);

	GL_ProcessErrors("pre-" __FUNCTION__);
	if (target != GL_TEXTURE_CUBE_MAP && qglTextureSubImage3D) {
		qglTextureSubImage3D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	else {
		GLenum textureUnit = GL_TEXTURE0 + unit;

		renderer.TextureUnitBind(textureUnit, texture);
		if (target == GL_TEXTURE_CUBE_MAP) {
			glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + zoffset, level, xoffset, yoffset, width, height, format, type, pixels);
		}
		else {
			qglTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
		}
	}
	R_TraceLogAPICall("GL_TexSubImage3D(unit=GL_TEXTURE%d, texture=%u)", unit, GL_TextureNameFromReference(texture));
	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexSubImage2D(
	GLenum textureUnit, texture_ref texture, GLint level,
	GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels
)
{
	GL_ProcessErrors("pre-" __FUNCTION__);
	if (qglTextureSubImage2D) {
		qglTextureSubImage2D(GL_TextureNameFromReference(texture), level, xoffset, yoffset, width, height, format, type, pixels);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
#ifdef GL_PARANOIA
		{
			int glWidth, glHeight;
			int fullWidth, fullHeight;

			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &fullWidth);
			glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &fullHeight);

			glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &glWidth);
			glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &glHeight);

			assert(glWidth >= width && glHeight >= height);
		}
#endif
		glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
	}
	R_TraceLogAPICall("GL_TexSubImage2D(unit=GL_TEXTURE%d, texture=%u)", textureUnit - GL_TEXTURE0, GL_TextureNameFromReference(texture));
	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexStorage2D(
	texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, qbool is_lightmap
)
{
	int tempWidth = width;
	int tempHeight = height;

	GL_ProcessErrors("pre-" __FUNCTION__);

	R_TextureSizeRoundUp(tempWidth, tempHeight, &width, &height);

	if (qglTextureStorage2D) {
		qglTextureStorage2D(GL_TextureNameFromReference(texture), levels, internalformat, width, height);
	}
	else {
		GLenum textureUnit = GL_TEXTURE0;
		GLenum target = GL_TextureTargetFromReference(texture);

		GL_BindTextureUnit(textureUnit, texture);
		if (qglTexStorage2D) {
			qglTexStorage2D(target, levels, internalformat, width, height);
			GL_ProcessErrors("post-glTexStorage2D");
		}
		else {
			int level;
			GLsizei level_width = width;
			GLsizei level_height = height;
			GLenum format = (internalformat == GL_RGBA8 || internalformat == GL_SRGB8_ALPHA8 || internalformat == GL_RGBA16F ? GL_RGBA : GL_RGB);
			GLenum type = (internalformat == GL_RGBA16F || internalformat == GL_RGB16F ? GL_FLOAT : GL_UNSIGNED_BYTE);

			// this might be completely useless (we don't upload data anyway) but just to keep all calls to the texture consistent
			format = (is_lightmap & GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS) ? GL_BGRA : format);
			type = (is_lightmap & GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS) ? GL_UNSIGNED_INT_8_8_8_8_REV : type);

			for (level = 0; level < levels; ++level) {
				glTexImage2D(target, level, internalformat, level_width, level_height, 0, format, type, NULL);
				GL_ProcessErrors("post-TexImage2D");

				level_width = max(1, level_width / 2);
				level_height = max(1, level_height / 2);
			}
		}
	}
	R_TextureSetDimensions(texture, width, height);

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexStorage3D(
	GLenum textureUnit, texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth
)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglTextureStorage3D) {
		qglTextureStorage3D(GL_TextureNameFromReference(texture), levels, internalformat, width, height, depth);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		qglTexStorage3D(target, levels, internalformat, width, height, depth);
	}

	GL_ProcessErrors("post-" __FUNCTION__);
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

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexParameterfv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLfloat *params)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglTextureParameterfv) {
		qglTextureParameterfv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameterfv(target, pname, params);
	}

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexParameteri(GLenum textureUnit, texture_ref texture, GLenum pname, GLint param)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglTextureParameteri) {
		qglTextureParameteri(GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteri(target, pname, param);
	}

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TexParameteriv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLint *params)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglTextureParameteriv) {
		qglTextureParameteriv(GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glTexParameteriv(target, pname, params);
	}

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

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

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref texture, GLint level, GLenum pname, GLint* params)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglGetTextureLevelParameteriv) {
		qglGetTextureLevelParameteriv(GL_TextureNameFromReference(texture), level, pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		glGetTexLevelParameteriv(target, level, pname, params);
	}

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_GetTexImage(GLenum textureUnit, texture_ref texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

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

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TextureMipmapGenerateWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

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

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_TextureMipmapGenerate(texture_ref texture)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	GL_TextureMipmapGenerateWithData(GL_TEXTURE0, texture, NULL, 0, 0, 0);

	GL_ProcessErrors("post-" __FUNCTION__);
}

// Samplers
static unsigned int nearest_sampler;
static unsigned int linear_sampler;

void GLM_SamplerSetNearest(unsigned int texture_unit_number)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (!nearest_sampler) {
		qglGenSamplers(1, &nearest_sampler);

		qglSamplerParameterf(nearest_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		qglSamplerParameterf(nearest_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	qglBindSampler(texture_unit_number, nearest_sampler);

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GLM_SamplerSetLinear(unsigned int texture_unit_number)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (!linear_sampler) {
		qglGenSamplers(1, &linear_sampler);

		qglSamplerParameterf(linear_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglSamplerParameterf(linear_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	qglBindSampler(texture_unit_number, linear_sampler);

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GLM_SamplerClear(unsigned int texture_unit_number)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	qglBindSampler(texture_unit_number, 0);

	GL_ProcessErrors("post-" __FUNCTION__);
}

void GL_DeleteSamplers(void)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	if (qglDeleteSamplers) {
		qglDeleteSamplers(1, &nearest_sampler);
	}
	nearest_sampler = 0;

	GL_ProcessErrors("post-" __FUNCTION__);
}

// meag: *RGBA is bit ugly, just trying to keep OpenGL enums out of the way for now
void GL_TextureReplaceSubImageRGBA(texture_ref ref, int offsetx, int offsety, int width, int height, byte* buffer)
{
	GL_ProcessErrors("pre-" __FUNCTION__);

	GL_TexSubImage2D(GL_TEXTURE0, ref, 0, offsetx, offsety, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

	GL_ProcessErrors("post-" __FUNCTION__);
}
