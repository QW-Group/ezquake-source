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
#include "r_vao.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_buffers.h"

// GLC uses vertex array, all client state
typedef struct {
	qbool enabled;

	int size;
	unsigned int type;
	int stride;
	void* pointer_or_offset;
} glc_va_element;

typedef struct glc_vao_s {
	qbool initialised;
	buffer_ref vertex_buffer;
	buffer_ref element_index_buffer;

	glc_va_element vertex_array;
	glc_va_element color_array;
	glc_va_element texture_array[MAX_GLC_TEXTURE_UNIT_STATES];
} glc_vao_t;

static glc_vao_t vaos[vao_count];

qbool GLC_InitialiseVAOHandling(void)
{
	memset(&vaos, 0, sizeof(vaos));
	return true;
}

void GLC_BindVertexArray(r_vao_id vao)
{
	glc_va_element* vertexes = &vaos[vao].vertex_array;
	glc_va_element* colors = &vaos[vao].color_array;
	buffer_ref buf = vaos[vao].vertex_buffer;
	int i;

	R_GLC_VertexPointer(buf, vertexes->enabled, vertexes->size, vertexes->type, vertexes->stride, vertexes->pointer_or_offset);
	R_GLC_ColorPointer(buf, colors->enabled, colors->size, colors->type, colors->stride, colors->pointer_or_offset);
	for (i = 0; i < sizeof(vaos[vao].texture_array) / sizeof(vaos[vao].texture_array[0]); ++i) {
		glc_va_element* textures = &vaos[vao].texture_array[i];

		R_GLC_TexturePointer(buf, i, textures->enabled, textures->size, textures->type, textures->stride, textures->pointer_or_offset);
	}

	if (GL_BufferReferenceIsValid(vaos[vao].element_index_buffer)) {
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
	// nothing to do here
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

void GLC_VAOSetVertexBuffer(r_vao_id vao, buffer_ref ref)
{
	vaos[vao].vertex_buffer = ref;
}

void GLC_VAOSetIndexBuffer(r_vao_id vao, buffer_ref ref)
{
	vaos[vao].element_index_buffer = ref;
}

#ifdef WITH_OPENGL_TRACE
void GLC_PrintVAOState(FILE* output, int indent, r_vao_id vao)
{
	int i;

	indent += 2;
	if (vaos[vao].vertex_array.enabled) {
		fprintf(output, "%.*s   vertex-array: enabled\n", indent, "                                                          ");
	}
	if (vaos[vao].color_array.enabled) {
		fprintf(output, "%.*s   color_array: enabled\n", indent, "                                                          ");
	}
	for (i = 0; i < sizeof(vaos[vao].texture_array) / sizeof(vaos[vao].texture_array[0]); ++i) {
		if (vaos[vao].texture_array[i].enabled) {
			fprintf(output, "%.*s   texture-array[%d]: enabled\n", indent, "                                                          ", i);
		}
	}
}
#endif
