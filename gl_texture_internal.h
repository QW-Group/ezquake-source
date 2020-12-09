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

#ifndef GL_TEXTURE_INTERNAL_HEADER
#define GL_TEXTURE_INTERNAL_HEADER

#include "r_texture.h"
#include "gl_model.h"
#include "gl_local.h"

// --------------
// Texture functions
// --------------

// Replaces top-level of a texture - if dimensions don't match then texture is reloaded
void GL_TextureReplace2D(
	GLenum textureUnit, GLenum target, texture_ref* ref, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid *pixels
);
const char* GL_TextureIdentifierByGLReference(GLuint texnum);
void GL_SelectTexture(GLenum target);

void GL_CreateTexturesWithIdentifier(r_texture_type_id type, int n, texture_ref* references, const char* identifier);
void GL_TexStorage2D(texture_ref reference, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, qbool is_lightmap);
GLenum GL_TexStorage3D(GLenum textureUnit, texture_ref reference, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, qbool is_lightmap);
void GL_TextureMipmapGenerate(texture_ref reference);
void GL_TextureMipmapGenerateWithData(GLenum textureUnit, texture_ref texture, byte* newdata, int width, int height, GLint internal_format);

void GL_TexParameterf(GLenum textureUnit, texture_ref reference, GLenum pname, GLfloat param);
void GL_TexParameterfv(GLenum textureUnit, texture_ref reference, GLenum pname, const GLfloat *params);
void GL_TexParameteri(GLenum textureUnit, texture_ref reference, GLenum pname, GLint param);
void GL_TexParameteriv(GLenum textureUnit, texture_ref reference, GLenum pname, const GLint *params);
void GL_GetTexLevelParameteriv(GLenum textureUnit, texture_ref reference, GLint level, GLenum pname, GLint* params);

void GL_TexSubImage2D(GLenum textureUnit, texture_ref reference, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void GL_TexSubImage3D(int textureUnit, texture_ref reference, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
void GL_GetTexImage(GLenum textureUnit, texture_ref reference, GLint level, GLenum format, GLenum type, GLsizei bufSize, void* buffer);

void GL_BindTextureUnit(GLuint unit, texture_ref reference);
qbool GL_EnsureTextureUnitBound(int unit, texture_ref reference);

void GL_CreateTextureNames(GLenum textureUnit, GLenum target, GLsizei n, GLuint* textures);
void GL_BindTextureToTarget(GLenum textureUnit, GLenum targetType, GLuint name);

#endif // GL_TEXTURE_INTERNAL_HEADER
