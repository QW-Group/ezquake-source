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

#ifdef RENDERER_OPTION_MODERN_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glm_vao.h"
#include "r_buffers.h"

typedef struct r_vao_s {
	GLuint vao;

	r_buffer_id element_array_buffer;
} glm_vao_t;

static glm_vao_t vaos[vao_count];

// VAOs
GL_StaticProcedureDeclaration(glGenVertexArrays, "size=%d, arrays=%p", GLsizei n, GLuint* arrays)
GL_StaticProcedureDeclaration(glBindVertexArray, "arrayNum=%u", GLuint arrayNum)
GL_StaticProcedureDeclaration(glDeleteVertexArrays, "n=%d, arrays=%p", GLsizei n, const GLuint* arrays)
GL_StaticProcedureDeclaration(glEnableVertexAttribArray, "index=%u", GLuint index)
GL_StaticProcedureDeclaration(glVertexAttribPointer, "index=%u, size=%d, type=%u, normalized=%d, stride=%d, pointer=%p", GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer)
GL_StaticProcedureDeclaration(glVertexAttribIPointer, "index=%u, size=%d, type=%u, stride=%d, pointer=%p", GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
GL_StaticProcedureDeclaration(glVertexAttribDivisor, "index=%u, divisor=%u", GLuint index, GLuint divisor)

qbool GLM_InitialiseVAOHandling(void)
{
	qbool vaos_supported = true;

	// VAOs: OpenGL 3.0
	if (SDL_GL_ExtensionSupported("GL_ARB_vertex_array_object")) {
		GL_LoadMandatoryFunctionExtension(glGenVertexArrays, vaos_supported);
		GL_LoadMandatoryFunctionExtension(glBindVertexArray, vaos_supported);
		GL_LoadMandatoryFunctionExtension(glDeleteVertexArrays, vaos_supported);
	}
	else {
		vaos_supported = false;
	}

	// OpenGL 2.0
	GL_LoadMandatoryFunctionExtension(glEnableVertexAttribArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribPointer, vaos_supported);

	// OpengGL 3.0
	GL_LoadMandatoryFunctionExtension(glVertexAttribIPointer, vaos_supported);

	if (SDL_GL_ExtensionSupported("GL_ARB_instanced_arrays")) {
		GL_LoadMandatoryFunctionExtension(glVertexAttribDivisor, vaos_supported);
	}
	else {
		vaos_supported = false;
	}

	return vaos_supported;
}

void GLM_BindVertexArray(r_vao_id vao)
{
	GL_Procedure(glBindVertexArray, vaos[vao].vao);
	if (vao) {
		buffers.SetElementArray(vaos[vao].element_array_buffer);
	}
}

void GLM_ConfigureVertexAttribPointer(r_vao_id vao, r_buffer_id vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(vbo)) {
		buffers.Bind(vbo);
	}
	else {
		buffers.UnBind(buffertype_vertex);
	}

	GL_Procedure(glEnableVertexAttribArray, index);
	GL_Procedure(glVertexAttribPointer, index, size, type, normalized, stride, pointer);
	GL_Procedure(glVertexAttribDivisor, index, divisor);
}

void GLM_ConfigureVertexAttribIPointer(r_vao_id vao, r_buffer_id vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(vbo)) {
		buffers.Bind(vbo);
	}
	else {
		buffers.UnBind(buffertype_vertex);
	}

	GL_Procedure(glEnableVertexAttribArray, index);
	GL_Procedure(glVertexAttribIPointer, index, size, type, stride, pointer);
	GL_Procedure(glVertexAttribDivisor, index, divisor);
}

void GLM_SetVertexArrayElementBuffer(r_vao_id vao, r_buffer_id ibo)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(ibo)) {
		buffers.Bind(ibo);
	}
	else {
		buffers.UnBind(buffertype_index);
	}
}

void GLM_GenVertexArray(r_vao_id vao, const char* name)
{
	if (vaos[vao].vao) {
		GL_Procedure(glDeleteVertexArrays, 1, &vaos[vao].vao);
	}
	GL_Procedure(glGenVertexArrays, 1, &vaos[vao].vao);
	R_BindVertexArray(vao);
	GL_TraceObjectLabelSet(GL_VERTEX_ARRAY, vaos[vao].vao, -1, name);
	buffers.SetElementArray(r_buffer_none);
}

void GLM_DeleteVAOs(void)
{
	r_vao_id id;

	if (GL_Available(glBindVertexArray)) {
		GL_Procedure(glBindVertexArray, 0);
	}

	for (id = 0; id < vao_count; ++id) {
		if (vaos[id].vao) {
			if (GL_Available(glDeleteVertexArrays)) {
				GL_Procedure(glDeleteVertexArrays, 1, &vaos[id].vao);
			}
			vaos[id].vao = 0;
		}
	}
}

qbool GLM_VertexArrayCreated(r_vao_id vao)
{
	return vaos[vao].vao != 0;
}

void GLM_BindVertexArrayElementBuffer(r_vao_id vao, r_buffer_id ref)
{
	if (vaos[vao].vao) {
		vaos[vao].element_array_buffer = ref;
	}
}

#endif // #ifdef RENDERER_OPTION_MODERN_OPENGL
