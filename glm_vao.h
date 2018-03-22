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

typedef struct glm_vao_s {
	unsigned int vao;
	char name[32];

	struct glm_vao_s* next;

	buffer_ref element_array_buffer;
} glm_vao_t;

void GL_BindVertexArray(glm_vao_t* vao);
void GL_GenVertexArray(glm_vao_t* vao, const char* name);
void GL_ConfigureVertexAttribPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor);
void GL_ConfigureVertexAttribIPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor);
void GL_SetVertexArrayElementBuffer(glm_vao_t* vao, buffer_ref ibo);

qbool GLM_InitialiseVAOHandling(void);
void GLM_InitialiseVAOState(void);

#endif // EZQUAKE_GLM_VAO_HEADER
