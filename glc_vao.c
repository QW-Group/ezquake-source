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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "r_vao.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_buffers.h"
#include "r_program.h"

GL_StaticProcedureDeclaration(glVertexAttribPointer, "index=%u, size=%d, type=%u, normalized=%d, stride=%d, pointer=%p", GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer)
GL_StaticProcedureDeclaration(glDisableVertexAttribArray, "index=%u", GLuint index)
GL_StaticProcedureDeclaration(glEnableVertexAttribArray, "index=%u", GLuint index)

void R_GLC_TexturePointer(r_buffer_id buffer_id, int unit, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset);
void R_GLC_ColorPointer(r_buffer_id buffer_id, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset);
void R_GLC_VertexPointer(r_buffer_id buffer_id, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset);
void R_GLC_NormalPointer(r_buffer_id buffer_id, qbool enabled, int size, GLenum type, int stride, void* pointer_or_offset);

// GLC uses vertex array, all client state
typedef struct {
	qbool enabled;

	int size;
	unsigned int type;
	int stride;
	void* pointer_or_offset;
} glc_va_element;

typedef struct glc_va_attribute_s {
	qbool enabled;

	r_program_attribute_id attr_id;
	GLint size;
	GLenum type;
	GLboolean normalized;
	GLsizei stride;
	const GLvoid* pointer;
} glc_va_attribute_t;

typedef struct glc_vao_s {
	qbool initialised;
	r_buffer_id vertex_buffer;
	r_buffer_id element_index_buffer;

	glc_va_element vertex_array;
	glc_va_element normal_array;
	glc_va_element color_array;
	glc_va_element texture_array[MAX_GLC_TEXTURE_UNIT_STATES];
	glc_va_attribute_t attributes[MAX_GLC_ATTRIBUTES];
} glc_vao_t;

static glc_vao_t vaos[vao_count];

qbool GLC_InitialiseVAOHandling(void)
{
	qbool vaos_supported = true;

	memset(&vaos, 0, sizeof(vaos));

	if (COM_CheckParm(cmdline_param_client_novao)) {
		GL_InvalidateFunction(glEnableVertexAttribArray);
		GL_InvalidateFunction(glDisableVertexAttribArray);
		GL_InvalidateFunction(glVertexAttribPointer);
		return false;
	}

	// OpenGL 2.0
	GL_LoadMandatoryFunctionExtension(glEnableVertexAttribArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glDisableVertexAttribArray, vaos_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribPointer, vaos_supported);

	R_TraceAPI("VAOs supported: %s", vaos_supported ? "yes" : "no (!)");

	return vaos_supported;
}

static glc_attribute_t attributes[MAX_GLC_ATTRIBUTES];

void GLC_BindVertexArrayAttributes(r_vao_id vao)
{
	int i;
	for (i = 0; i < MAX_GLC_ATTRIBUTES; ++i) {
		glc_va_attribute_t* attr = &vaos[vao].attributes[i];

		if (attr->enabled && R_ProgramInUse() == R_ProgramForAttribute(attr->attr_id)) {
			GLint location = R_ProgramAttributeLocation(attr->attr_id);

			if (location >= 0) {
				GL_Procedure(glEnableVertexAttribArray, location);
				GL_Procedure(glVertexAttribPointer, location, attr->size, attr->type, attr->normalized ? GL_TRUE : GL_FALSE, attr->stride, attr->pointer);
				attributes[i].location = location;
				attributes[i].enabled = true;
			}
			else {
				GL_Procedure(glDisableVertexAttribArray, attributes[i].location);
				attributes[i].enabled = false;
			}
		}
		else {
			GL_Procedure(glDisableVertexAttribArray, attributes[i].location);
			attributes[i].enabled = false;
		}
	}
}

void GLC_BindVertexArray(r_vao_id vao)
{
	glc_va_element* vertexes = &vaos[vao].vertex_array;
	glc_va_element* colors = &vaos[vao].color_array;
	glc_va_element* normals = &vaos[vao].normal_array;
	r_buffer_id buf = vaos[vao].vertex_buffer;
	int i;

	// Unbind any active attributes
	for (i = 0; i < MAX_GLC_ATTRIBUTES; ++i) {
		if (attributes[i].enabled) {
			GL_Procedure(glDisableVertexAttribArray, attributes[i].location);
			attributes[i].enabled = false;
		}
	}

	R_GLC_VertexPointer(buf, vertexes->enabled, vertexes->size, vertexes->type, vertexes->stride, vertexes->pointer_or_offset);
	R_GLC_ColorPointer(buf, colors->enabled, colors->size, colors->type, colors->stride, colors->pointer_or_offset);
	R_GLC_NormalPointer(buf, normals->enabled, normals->size, normals->type, normals->stride, normals->pointer_or_offset);
	for (i = 0; i < sizeof(vaos[vao].texture_array) / sizeof(vaos[vao].texture_array[0]); ++i) {
		glc_va_element* textures = &vaos[vao].texture_array[i];

		if (i < gl_textureunits) {
			R_GLC_TexturePointer(buf, i, textures->enabled, textures->size, textures->type, textures->stride, textures->pointer_or_offset);
		}
	}

	for (i = 0; i < MAX_GLC_ATTRIBUTES; ++i) {
		glc_va_attribute_t* attr = &vaos[vao].attributes[i];

		if (attr->enabled && R_ProgramInUse() == R_ProgramForAttribute(attr->attr_id)) {
			GLint location = R_ProgramAttributeLocation(attr->attr_id);

			if (location >= 0) {
				GL_Procedure(glEnableVertexAttribArray, location);
				GL_Procedure(glVertexAttribPointer, location, attr->size, attr->type, attr->normalized ? GL_TRUE : GL_FALSE, attr->stride, attr->pointer);
				attributes[i].location = location;
				attributes[i].enabled = true;
			}
		}
	}

	if (R_BufferReferenceIsValid(vaos[vao].element_index_buffer)) {
		buffers.Bind(vaos[vao].element_index_buffer);
	}
	else {
		buffers.UnBind(buffertype_index);
	}
}

qbool GLC_VertexArrayCreated(r_vao_id vao)
{
	return vaos[vao].initialised;
}

void GLC_GenVertexArray(r_vao_id vao, const char* name)
{
	memset(&vaos[vao], 0, sizeof(vaos[vao]));
	vaos[vao].initialised = true;
}

void GLC_DeleteVAOs(void)
{
	memset(vaos, 0, sizeof(vaos));
}

static void GLC_VAOEnableComponent(glc_va_element* el, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	el->enabled = true;
	el->size = size;
	el->stride = stride;
	el->type = type;
	el->pointer_or_offset = pointer;
}

static void GLC_VAODisableComponent(glc_va_element* el)
{
	el->enabled = false;
}

void GLC_VAOEnableVertexPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	GLC_VAOEnableComponent(&vaos[vao].vertex_array, size, type, stride, pointer);
}

void GLC_VAODisableVertexPointer(r_vao_id vao)
{
	GLC_VAODisableComponent(&vaos[vao].vertex_array);
}

void GLC_VAOEnableNormalPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	GLC_VAOEnableComponent(&vaos[vao].normal_array, size, type, stride, pointer);
}

void GLC_VAODisableNormalPointer(r_vao_id vao)
{
	GLC_VAODisableComponent(&vaos[vao].normal_array);
}

void GLC_VAOEnableColorPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	GLC_VAOEnableComponent(&vaos[vao].color_array, size, type, stride, pointer);
}

void GLC_VAODisableColorPointer(r_vao_id vao)
{
	GLC_VAODisableComponent(&vaos[vao].color_array);
}

void GLC_VAOEnableTextureCoordPointer(r_vao_id vao, int index, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	GLC_VAOEnableComponent(&vaos[vao].texture_array[index], size, type, stride, pointer);
}

void GLC_VAODisableTextureCoordPointer(r_vao_id vao, int index)
{
	GLC_VAODisableComponent(&vaos[vao].texture_array[index]);
}

void GLC_VAOEnableCustomAttribute(r_vao_id vao, int index, r_program_attribute_id attr_id, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer)
{
	glc_va_attribute_t* attr = &vaos[vao].attributes[index];

	memset(attr, 0, sizeof(*attr));
	attr->enabled = true;
	attr->attr_id = attr_id;
	attr->size = size;
	attr->type = type;
	attr->normalized = normalized;
	attr->stride = stride;
	attr->pointer = pointer;
}

void GLC_VAODisableCustomAttribute(r_vao_id vao, int index)
{
	glc_va_attribute_t* attr = &vaos[vao].attributes[index];

	attr->enabled = false;
}

void GLC_VAOSetVertexBuffer(r_vao_id vao, r_buffer_id ref)
{
	vaos[vao].vertex_buffer = ref;
}

void GLC_VAOSetIndexBuffer(r_vao_id vao, r_buffer_id ref)
{
	vaos[vao].element_index_buffer = ref;
}

#ifdef WITH_RENDERING_TRACE
void GLC_PrintVAOState(FILE* output, int indent, r_vao_id vao)
{
	int i;

	indent += 2;
	if (vaos[vao].vertex_array.enabled) {
		glc_va_element* el = &vaos[vao].vertex_array;
		fprintf(output, "%.*s   vertex-array: enabled(type %u, size %d, stride %d, pointer %p)\n", indent, "                                                          ", el->type, el->size, el->stride, el->pointer_or_offset);
	}
	if (vaos[vao].color_array.enabled) {
		glc_va_element* el = &vaos[vao].color_array;
		fprintf(output, "%.*s   color_array: enabled(type %u, size %d, stride %d, pointer %p)\n", indent, "                                                          ", el->type, el->size, el->stride, el->pointer_or_offset);
	}
	if (vaos[vao].normal_array.enabled) {
		glc_va_element* el = &vaos[vao].normal_array;
		fprintf(output, "%.*s   normal_array: enabled(type %u, size %d, stride %d, pointer %p)\n", indent, "                                                          ", el->type, el->size, el->stride, el->pointer_or_offset);
	}
	for (i = 0; i < sizeof(vaos[vao].texture_array) / sizeof(vaos[vao].texture_array[0]); ++i) {
		glc_va_element* el = &vaos[vao].texture_array[i];
		if (el->enabled) {
			fprintf(output, "%.*s   texture-array[%d]: enabled(type %u, size %d, stride %d, pointer %p)\n", indent, "                                                          ", i, el->type, el->size, el->stride, el->pointer_or_offset);
		}
	}
}
#endif

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
