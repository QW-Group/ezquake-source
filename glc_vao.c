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

// GLC uses vertex array, all client state
typedef struct {
	qbool enabled;

	int size;
	unsigned int type;
	int stride;
	void* pointer_or_offset;
} glc_va_element;

typedef struct glc_vao_s {
	buffer_ref vertex_buffer;
	buffer_ref element_index_buffer;

	glc_va_element  vertex_array;
	glc_va_element  color_array;
	glc_va_element  texture_array[MAX_GLC_TEXTURE_UNIT_STATES];
} glc_vao_t;

static glc_vao_t vaos[vao_count];

qbool GLC_InitialiseVAOHandling(void)
{
	return true;
}

void GLC_BindVertexArray(r_vao_id vao)
{
	glc_va_element* element;
	int i;

	if ((element = &vaos[vao].vertex_array)->enabled) {
		glVertexPointer(element->size, element->type, element->stride, element->pointer_or_offset);
		glEnableClientState(GL_VERTEX_ARRAY);
	}
	else {
		glDisableClientState(GL_VERTEX_ARRAY);
	}

	if ((element = &vaos[vao].color_array)->enabled) {
		glColorPointer(element->size, element->type, element->stride, element->pointer_or_offset);
		glEnableClientState(GL_COLOR_ARRAY);
		R_GLC_InvalidateColor();
	}
	else {
		glDisableClientState(GL_COLOR_ARRAY);
		R_GLC_InvalidateColor();
	}

	for (i = 0; i < sizeof(vaos[vao].texture_array) / sizeof(vaos[vao].texture_array[0]); ++i) {
		GLC_ClientActiveTexture(GL_TEXTURE0 + i);
		if ((element = &vaos[vao].vertex_array)->enabled) {
			glTexCoordPointer(element->size, element->type, element->stride, element->pointer_or_offset);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		else {
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}
}

qbool GLC_VertexArrayCreated(r_vao_id vao)
{
	// There isn't really an object to be created, so this is non-issue
	return true;
}

void GLC_GenVertexArray(r_vao_id vao, const char* name)
{
	memset(&vaos[vao], 0, sizeof(vaos[vao]));
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
	vaos[vao].vertex_array.enabled = false;
}

void GLC_VAOEnableColorPointer(r_vao_id vao, int size, GLenum type, GLsizei stride, GLvoid* pointer)
{
	GLC_VAOEnableComponent(&vaos[vao].color_array, size, type, stride, pointer);
}

void GLC_VAODisableColorPointer(r_vao_id vao)
{
	vaos[vao].color_array.enabled = false;
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
