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

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glm_vao.h"
#include "r_buffers.h"

typedef struct r_vao_s {
	GLuint vao;

	buffer_ref element_array_buffer;
} glm_vao_t;

static glm_vao_t vaos[vao_count];

// VAOs
typedef void (APIENTRY *glGenVertexArrays_t)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY *glBindVertexArray_t)(GLuint arrayNum);
typedef void (APIENTRY *glDeleteVertexArrays_t)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY *glEnableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY *glVertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribIPointer_t)(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribDivisor_t)(GLuint index, GLuint divisor);

// VAO functions
static glGenVertexArrays_t         qglGenVertexArrays = NULL;
static glBindVertexArray_t         qglBindVertexArray = NULL;
static glEnableVertexAttribArray_t qglEnableVertexAttribArray = NULL;
static glDeleteVertexArrays_t      qglDeleteVertexArrays = NULL;
static glVertexAttribPointer_t     qglVertexAttribPointer = NULL;
static glVertexAttribIPointer_t    qglVertexAttribIPointer = NULL;
static glVertexAttribDivisor_t     qglVertexAttribDivisor = NULL;

qbool GLM_InitialiseVAOHandling(void)
{
	qbool vaos_supported = true;

	// VAOs
	GL_LoadMandatoryFunctionExtension(glGenVertexArrays, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glBindVertexArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteVertexArrays, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glEnableVertexAttribArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribPointer, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribIPointer, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribDivisor, vaos_supported);

	return vaos_supported;
}

void GLM_BindVertexArray(r_vao_id vao)
{
	qglBindVertexArray(vaos[vao].vao);
	if (vao) {
		buffers.SetElementArray(vaos[vao].element_array_buffer);
	}
}

void GLM_ConfigureVertexAttribPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(vbo)) {
		buffers.Bind(vbo);
	}
	else {
		buffers.UnBind(GL_ARRAY_BUFFER);
	}

	qglEnableVertexAttribArray(index);
	qglVertexAttribPointer(index, size, type, normalized, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
}

void GLM_ConfigureVertexAttribIPointer(r_vao_id vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(vbo)) {
		buffers.Bind(vbo);
	}
	else {
		buffers.UnBind(GL_ARRAY_BUFFER);
	}

	qglEnableVertexAttribArray(index);
	qglVertexAttribIPointer(index, size, type, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
}

void GLM_SetVertexArrayElementBuffer(r_vao_id vao, buffer_ref ibo)
{
	assert(vao);
	assert(vaos[vao].vao);

	R_BindVertexArray(vao);
	if (R_BufferReferenceIsValid(ibo)) {
		buffers.Bind(ibo);
	}
	else {
		buffers.UnBind(GL_ELEMENT_ARRAY_BUFFER);
	}
}

void GLM_GenVertexArray(r_vao_id vao, const char* name)
{
	if (vaos[vao].vao) {
		qglDeleteVertexArrays(1, &vaos[vao].vao);
	}
	qglGenVertexArrays(1, &vaos[vao].vao);
	R_BindVertexArray(vao);
	R_TraceObjectLabelSet(GL_VERTEX_ARRAY, vaos[vao].vao, -1, name);
	buffers.SetElementArray(null_buffer_reference);
}

void GLM_DeleteVAOs(void)
{
	r_vao_id id;

	if (qglBindVertexArray) {
		qglBindVertexArray(0);
	}

	for (id = 0; id < vao_count; ++id) {
		if (vaos[id].vao) {
			if (qglDeleteVertexArrays) {
				qglDeleteVertexArrays(1, &vaos[id].vao);
			}
			vaos[id].vao = 0;
		}
	}
}

qbool GLM_VertexArrayCreated(r_vao_id vao)
{
	return vaos[vao].vao != 0;
}

void GLM_BindVertexArrayElementBuffer(r_vao_id vao, buffer_ref ref)
{
	if (vaos[vao].vao) {
		vaos[vao].element_array_buffer = ref;
	}
}
