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

#ifndef EZQUAKE_GL_TEXTURE_H
#define EZQUAKE_GL_TEXTURE_H

// Replaces top-level of a texture - if dimensions don't match then texture is reloaded
extern GLenum gl_lightmap_format, gl_solid_format, gl_alpha_format;

void GL_TextureReplace2D(
	GLenum textureUnit, GLenum target, texture_ref* ref, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, GLvoid *pixels
);
const char* GL_TextureIdentifierByGLReference(GLuint texnum);
void GL_AllocateTextureReferences(GLenum target, int width, int height, int mode, GLsizei number, texture_ref* references);
void GL_SelectTexture(GLenum target);
void GLC_EnableTMU(GLenum target);
void GLC_DisableTMU(GLenum target);
void GLC_EnsureTMUEnabled(GLenum target);
void GLC_EnsureTMUDisabled(GLenum target);
extern GLenum gl_lightmap_format, gl_solid_format, gl_alpha_format;

#endif // EZQUAKE_GL_TEXTURE_H