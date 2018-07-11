/*
Copyright (C) 2018 ezQuake team.

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

#ifndef EZQUAKE_GLM_VAO_HEADER
#define EZQUAKE_GLM_VAO_HEADER

#include "gl_local.h"
#include "r_vao.h"

void GLM_BindVertexArray(r_vao_id vao);
void GLM_GenVertexArray(r_vao_id vao, const char* name);
void GLM_SetVertexArrayElementBuffer(r_vao_id vao, buffer_ref ibo);
void GLM_DeleteVAOs(void);

qbool GLM_InitialiseVAOHandling(void);
qbool GLM_VertexArrayCreated(r_vao_id vao);

void GLM_ConfigureVertexAttribPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor);
void GLM_ConfigureVertexAttribIPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor);

void GLM_BindVertexArrayElementBuffer(r_vao_id, buffer_ref ref);

#endif // EZQUAKE_GLM_VAO_HEADER
