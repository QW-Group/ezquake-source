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

// Samplers
static unsigned int nearest_sampler;
static unsigned int linear_sampler;

GLuint GL_TextureNameFromReference(texture_ref ref);
GLenum GL_TextureTargetFromReference(texture_ref ref);

// <DSA-functions (4.5)>
// These allow modification of textures without binding (-bind-to-edit)
GL_StaticProcedureDeclaration(glGetTextureLevelParameterfv, "texture=%u, level=%d, pname=%u, params=%p", GLuint texture, GLint level, GLenum pname, GLfloat* params)
GL_StaticProcedureDeclaration(glGetTextureLevelParameteriv, "texture=%u, level=%d, pname=%u, params=%p", GLuint texture, GLint level, GLenum pname, GLint* params)
GL_StaticProcedureDeclaration(glGenerateTextureMipmap, "texture=%u", GLuint texture)
GL_StaticProcedureDeclaration(glGetTextureImage, "texture=%u, level=%d, format=%u, type=%u, bufSize=%d, pixels=%p", GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels)
GL_StaticProcedureDeclaration(glCreateTextures, "target=%u, n=%d, textures=%p", GLenum target, GLsizei n, GLuint* textures)
GL_StaticProcedureDeclaration(glGetnTexImage, "target=%u, level=%d, format=%u, type=%u, bufSize=%d, pixels=%p", GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* pixels)
GL_StaticProcedureDeclaration(glTextureParameteri, "texture=%u, pname=%u, param=%d", GLuint texture, GLenum pname, GLint param)
GL_StaticProcedureDeclaration(glTextureParameterf, "texture=%u, pname=%u, param=%f", GLuint texture, GLenum pname, GLfloat param)
GL_StaticProcedureDeclaration(glTextureParameterfv, "texture=%u, pname=%u, params=%p", GLuint texture, GLenum pname, const GLfloat* params)
GL_StaticProcedureDeclaration(glTextureParameteriv, "texture=%u, pname=%u, params=%p", GLuint texture, GLenum pname, const GLint* params)
GL_StaticProcedureDeclaration(glTextureStorage3D, "texture=%u, levels=%d, internalformat=%u, width=%d, height=%d, depth=%d", GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
GL_StaticProcedureDeclaration(glTextureStorage2D, "texture=%u, levels=%d, internalformat=%u, width=%d, height=%d", GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
GL_StaticProcedureDeclaration(glTextureSubImage2D, "texture=%u, level=%d, xoffset=%d, yoffset=%d, width=%d, height=%d, format=%u, type=%u, pixels=%p", GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels)
GL_StaticProcedureDeclaration(glTextureSubImage3D, "texture=%u, level=%d, xoffset=%d, yoffset=%d, zoffset=%d, width=%d, height=%d, depth=%d, format=%u, type=%u, pixels=%p", GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels)

GL_StaticProcedureDeclaration(glTexStorage2D, "target=%u, levels=%d, internalformat=%u, width=%d, height=%d", GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
GL_StaticProcedureDeclaration(glTexSubImage3D, "target=%u, level=%d, xoffset=%d, yoffset=%d, zoffset=%d, width=%d, height=%d, depth=%d, format=%u, type=%u, pixels=%p", GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid* pixels)
GL_StaticProcedureDeclaration(glTexStorage3D, "target=%u, levels=%d, internalformat=%u, width=%d, height=%d, depth=%d", GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
GL_StaticProcedureDeclaration(glTexImage3D, "target=%u, level=%d, internalformat=%d, width=%d, height=%d, depth=%d, border=%d, format=%u, type=%u, data=%p", GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* data)
GL_StaticProcedureDeclaration(glGenerateMipmap, "target=%u", GLenum target)

GL_StaticProcedureDeclaration(glGenSamplers, "n=%d, samplers=%p", GLsizei n, GLuint* samplers)
GL_StaticProcedureDeclaration(glDeleteSamplers, "n=%d, samplers=%p", GLsizei n, const GLuint* samplers)
GL_StaticProcedureDeclaration(glSamplerParameterf, "sampler=%u, pname=%u, param=%f", GLuint sampler, GLenum pname, GLfloat param)
GL_StaticProcedureDeclaration(glBindSampler, "unit=%u, sampler=%u", GLuint unit, GLuint sampler)

GL_StaticProcedureDeclaration(glGetInternalformativ, "target=%u, internalformat=%u, pname=%u, bufSize=%d, params=%p", GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint* params)

void GL_LoadTextureManagementFunctions(void)
{
	glConfig.supported_features &= ~(R_SUPPORT_TEXTURE_ARRAYS | R_SUPPORT_TEXTURE_SAMPLERS);
	nearest_sampler = linear_sampler = 0;

	glConfig.preferred_format = glConfig.preferred_type = 0;
	if (GL_VersionAtLeast(4, 3) || SDL_GL_ExtensionSupported("GL_ARB_internalformat_query2")) {
		GL_LoadOptionalFunction(glGetInternalformativ);

		if (GL_Available(glGetInternalformativ)) {
			GLint tempFormat = 0, tempType = 0;

			GL_Procedure(glGetInternalformativ, GL_TEXTURE_2D, GL_RGBA8, GL_TEXTURE_IMAGE_FORMAT, 1, &tempFormat);
			GL_Procedure(glGetInternalformativ, GL_TEXTURE_2D, GL_RGBA8, GL_TEXTURE_IMAGE_TYPE, 1, &tempType);

			glConfig.preferred_format = tempFormat;
			glConfig.preferred_type = tempType;
		}
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_texture_storage")) {
		GL_LoadOptionalFunction(glTexStorage2D);
		GL_LoadOptionalFunction(glTexStorage3D);
	}

	if (GL_VersionAtLeast(3,0) || SDL_GL_ExtensionSupported("GL_EXT_texture_array")) {
		qbool all_available = true;

		// OpenGL 1.2
		GL_LoadMandatoryFunctionExtension(glTexSubImage3D, all_available);
		GL_LoadMandatoryFunctionExtension(glTexImage3D, all_available);

		glConfig.supported_features |= (all_available ? R_SUPPORT_TEXTURE_ARRAYS : 0);
	}

	if (SDL_GL_ExtensionSupported("GL_ARB_sampler_objects")) {
		qbool all_available = true;

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
		GL_LoadOptionalFunction(glGenerateMipmap);
		if (!GL_Available(glGenerateMipmap)) {
			GL_LoadOptionalFunctionEXT(glGenerateMipmap);
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

	if (GL_VersionAtLeast(3, 3)) {
		glConfig.supported_features |= R_SUPPORT_TEXTURE_ARRAYS;
	}

	if (GL_VersionAtLeast(4, 5)) {
		GL_LoadOptionalFunction(glGetnTexImage);
	}

	if (GL_VersionAtLeast(3, 2) || SDL_GL_ExtensionSupported("GL_ARB_seamless_cube_map")) {
		glConfig.supported_features |= R_SUPPORT_SEAMLESS_CUBEMAPS;
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	if (GL_VersionAtLeast(1, 3)) {
		glConfig.supported_features |= R_SUPPORT_CUBE_MAPS;
	}

	if (GL_VersionAtLeast(1, 4)) {
		glConfig.supported_features |= R_SUPPORT_FOG;
	}
}

void GL_TexSubImage3D(
	int unit, texture_ref texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
	GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels
)
{
	GLenum target = GL_TextureTargetFromReference(texture);

	R_TraceLogAPICall("GL_TexSubImage3D(unit=GL_TEXTURE%d, texture=%u)", unit, GL_TextureNameFromReference(texture));
	if (target != GL_TEXTURE_CUBE_MAP && GL_Available(glTextureSubImage3D)) {
		GL_Procedure(glTextureSubImage3D, GL_TextureNameFromReference(texture), level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
	}
	else {
		renderer.TextureUnitBind(unit, texture);
		if (target == GL_TEXTURE_CUBE_MAP) {
			GL_BuiltinProcedure(glTexSubImage2D, "target=%u, level=%d, xoffset=%d, yoffset=%d, width=%d, height=%d, format=%u, type=%u, pixels=%p", GL_TEXTURE_CUBE_MAP_POSITIVE_X + zoffset, level, xoffset, yoffset, width, height, format, type, pixels);
		}
		else {
			GL_Procedure(glTexSubImage3D, target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
		}
	}
}

void GL_TexSubImage2D(
	GLenum textureUnit, texture_ref texture, GLint level,
	GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels
)
{
	R_TraceLogAPICall("GL_TexSubImage2D(unit=GL_TEXTURE%d, texture=%u)", textureUnit - GL_TEXTURE0, GL_TextureNameFromReference(texture));
	if (GL_Available(glTextureSubImage2D)) {
		GL_Procedure(glTextureSubImage2D, GL_TextureNameFromReference(texture), level, xoffset, yoffset, width, height, format, type, pixels);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glTexSubImage2D, "target=%u, level=%d, xoffset=%d, yoffset=%d, width=%d, height=%d, format=%u, type=%u, pixels=%p", target, level, xoffset, yoffset, width, height, format, type, pixels);
	}
}

void GL_TexStorage2D(
	texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, qbool is_lightmap
)
{
	int tempWidth = width;
	int tempHeight = height;

	GL_ProcessErrors("pre-" __FUNCTION__);

	R_TextureSizeRoundUp(tempWidth, tempHeight, &width, &height);

	if (GL_Available(glTextureStorage2D)) {
		GL_Procedure(glTextureStorage2D, GL_TextureNameFromReference(texture), levels, internalformat, width, height);
	}
	else {
		GLenum textureUnit = GL_TEXTURE0;
		GLenum target = GL_TextureTargetFromReference(texture);

		GL_BindTextureUnit(textureUnit, texture);
		if (GL_Available(glTexStorage2D)) {
			GL_Procedure(glTexStorage2D, target, levels, internalformat, width, height);
		}
		else {
			int level;
			GLsizei level_width = width;
			GLsizei level_height = height;
			GLenum format = (internalformat == GL_RGBA8 || internalformat == GL_SRGB8_ALPHA8 || internalformat == GL_RGBA16F ? GL_RGBA : GL_RGB);
			GLenum type = (internalformat == GL_RGBA16F || internalformat == GL_RGB16F ? GL_FLOAT : GL_UNSIGNED_BYTE);

			// this might be completely useless (we don't upload data anyway) but just to keep all calls to the texture consistent
			format = ((is_lightmap && GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS)) ? GL_BGRA : format);
			type = ((is_lightmap && GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS)) ? GL_UNSIGNED_INT_8_8_8_8_REV : type);

			for (level = 0; level < levels; ++level) {
				GL_BuiltinProcedure(glTexImage2D, "target=%u, level=%d, internalformat=%d, width=%d, height=%d, border=%d, format=%u, type=%u, pixels=%p", target, level, internalformat, level_width, level_height, 0, format, type, NULL);

				level_width = max(1, level_width / 2);
				level_height = max(1, level_height / 2);
			}
		}
	}
	R_TextureSetDimensions(texture, width, height);
}

GLenum GL_TexStorage3D(GLenum textureUnit, texture_ref texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, qbool is_lightmap)
{
	if (GL_Available(glTextureStorage3D)) {
		GL_ProcedureReturnError(glTextureStorage3D, GL_TextureNameFromReference(texture), levels, internalformat, width, height, depth);
	}
	else if (GL_Available(glTexStorage3D)) {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_ProcedureReturnError(glTexStorage3D, target, levels, internalformat, width, height, depth);
	}
	else {
		int level, z;
		GLenum target = GL_TextureTargetFromReference(texture);
		GLenum format = (internalformat == GL_RGBA8 || internalformat == GL_SRGB8_ALPHA8 || internalformat == GL_RGBA16F ? GL_RGBA : GL_RGB);
		GLenum type = (internalformat == GL_RGBA16F || internalformat == GL_RGB16F ? GL_FLOAT : GL_UNSIGNED_BYTE);

		// this might be completely useless (we don't upload data anyway) but just to keep all calls to the texture consistent
		format = ((is_lightmap && GL_Supported(R_SUPPORT_BGRA_LIGHTMAPS)) ? GL_BGRA : format);
		type = ((is_lightmap && GL_Supported(R_SUPPORT_INT8888R_LIGHTMAPS)) ? GL_UNSIGNED_INT_8_8_8_8_REV : type);

		for (z = 0; z < depth; ++z) {
			GLsizei level_width = width;
			GLsizei level_height = height;

			for (level = 0; level < levels; ++level) {
				GL_ProcedureReturnIfError(glTexImage3D, target, level, internalformat, level_width, level_height, z, 0, format, type, NULL);

				level_width = max(1, level_width / 2);
				level_height = max(1, level_height / 2);
			}
		}
		return GL_NO_ERROR;
	}
}

void GL_TexParameterf(GLenum textureUnit, texture_ref texture, GLenum pname, GLfloat param)
{
	if (GL_Available(glTextureParameterf)) {
		GL_Procedure(glTextureParameterf, GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glTexParameterf, "target=%u, pname=%u, param=%f", target, pname, param);
	}
}

void GL_TexParameterfv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLfloat *params)
{
	if (GL_Available(glTextureParameterfv)) {
		GL_Procedure(glTextureParameterfv, GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glTexParameterfv, "target=%u, pname=%u, params=%p", target, pname, params);
	}
}

void GL_TexParameteri(GLenum textureUnit, texture_ref texture, GLenum pname, GLint param)
{
	if (GL_Available(glTextureParameteri)) {
		GL_Procedure(glTextureParameteri, GL_TextureNameFromReference(texture), pname, param);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glTexParameteri, "target=%u, pname=%u, param=%d", target, pname, param);
	}
}

void GL_TexParameteriv(GLenum textureUnit, texture_ref texture, GLenum pname, const GLint *params)
{
	if (GL_Available(glTextureParameteriv)) {
		GL_Procedure(glTextureParameteriv, GL_TextureNameFromReference(texture), pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glTexParameteriv, "target=%u, pname=%u, params=%p", target, pname, params);
	}
}

void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures)
{
	int i;

	if (GL_Available(glCreateTextures)) {
		GL_Procedure(glCreateTextures, target, n, textures);
	}
	else {
		GL_BuiltinProcedure(glGenTextures, "n=%d, textures=%p", n, textures);
	}

	// glCreateTextures() shouldn't require this, but textures can't be sampled from buggy AMD drivers (reporting x.y.13399) otherwise
	// see https://github.com/ezQuake/ezquake-source/issues/416
	for (i = 0; i < n; ++i) {
		GL_BindTextureToTarget(textureUnit, target, textures[i]);
	}
}

void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref texture, GLint level, GLenum pname, GLint* params)
{
	if (GL_Available(glGetTextureLevelParameteriv)) {
		GL_Procedure(glGetTextureLevelParameteriv, GL_TextureNameFromReference(texture), level, pname, params);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		GL_BuiltinProcedure(glGetTexLevelParameteriv, "target=%u, level=%d, pname=%u, params=%p", target, level, pname, params);
	}
}

void GL_GetTexImage(GLenum textureUnit, texture_ref texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer)
{
	GL_PackAlignment(1);
	if (GL_Available(glGetTextureImage)) {
		GL_Procedure(glGetTextureImage, GL_TextureNameFromReference(texture), level, format, type, bufSize, buffer);
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (GL_Available(glGetnTexImage)) {
			GL_Procedure(glGetnTexImage, target, level, format, type, bufSize, buffer);
		}
		else {
			GL_BuiltinProcedure(glGetTexImage, "target=%u, level=%d, format=%u, type=%u, buffer=%p", target, level, format, type, buffer);
		}
	}
}

void GL_TextureMipmapGenerateWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format)
{
	if (GL_Available(glGenerateTextureMipmap)) {
		GL_Procedure(glGenerateTextureMipmap, GL_TextureNameFromReference(texture));
	}
	else {
		GLenum target = GL_TextureTargetFromReference(texture);
		GL_BindTextureUnit(textureUnit, texture);
		if (GL_Available(glGenerateMipmap)) {
			GL_Procedure(glGenerateMipmap, target);
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

void GL_SamplerSetNearest(unsigned int texture_unit_number)
{
	if (!nearest_sampler) {
		GL_Procedure(glGenSamplers, 1, &nearest_sampler);

		GL_Procedure(glSamplerParameterf, nearest_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GL_Procedure(glSamplerParameterf, nearest_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	GL_Procedure(glBindSampler, texture_unit_number, nearest_sampler);
}

void GL_SamplerSetLinear(unsigned int texture_unit_number)
{
	if (!linear_sampler) {
		GL_Procedure(glGenSamplers, 1, &linear_sampler);

		GL_Procedure(glSamplerParameterf, linear_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GL_Procedure(glSamplerParameterf, linear_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	GL_Procedure(glBindSampler, texture_unit_number, linear_sampler);
}

void GL_SamplerClear(unsigned int texture_unit_number)
{
	GL_Procedure(glBindSampler, texture_unit_number, 0);
}

void GL_DeleteSamplers(void)
{
	if (GL_Available(glDeleteSamplers)) {
		GL_Procedure(glDeleteSamplers, 1, &nearest_sampler);
	}
	nearest_sampler = 0;
}

// meag: *RGBA is bit ugly, just trying to keep OpenGL enums out of the way for now
void GL_TextureReplaceSubImageRGBA(texture_ref ref, int offsetx, int offsety, int width, int height, byte* buffer)
{
	GL_TexSubImage2D(GL_TEXTURE0, ref, 0, offsetx, offsety, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
}
