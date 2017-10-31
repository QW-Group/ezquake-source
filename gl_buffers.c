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

// OpenGL buffer handling

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

static void GL_BindBufferImpl(GLenum target, GLuint buffer);

typedef struct buffer_data_s {
	GLuint glref;
	char name[64];

	// These set at creation
	GLenum target;
	size_t size;
	GLuint usage;

	struct buffer_data_s* next_free;
} buffer_data_t;

static buffer_data_t buffers[256];
static int buffer_count;
static buffer_data_t* next_free_buffer = NULL;
static qbool buffers_supported = false;

// Buffer functions
static glBindBuffer_t     glBindBuffer = NULL;
static glBufferData_t     glBufferData = NULL;
static glBufferSubData_t  glBufferSubData = NULL;
static glGenBuffers_t     glGenBuffers = NULL;
static glDeleteBuffers_t  glDeleteBuffers = NULL;
static glBindBufferBase_t glBindBufferBase = NULL;

// Cache OpenGL state
static GLuint currentArrayBuffer;
static GLuint currentUniformBuffer;

buffer_ref GL_GenFixedBuffer(GLenum target, const char* name, GLsizei size, void* data, GLenum usage)
{
	buffer_data_t* buffer = NULL;
	buffer_ref result;
	int i;

	if (name && name[0]) {
		for (i = 0; i < buffer_count; ++i) {
			if (!strcmp(name, buffers[i].name)) {
				buffer = &buffers[i];
				result.index = i;
				if (buffer->glref) {
					glDeleteBuffers(1, &buffer->glref);
				}
				break;
			}
		}
	}

	if (buffer_count == 0) {
		buffer_count = 1;
	}

	if (!buffer) {
		if (next_free_buffer) {
			buffer = next_free_buffer;
			next_free_buffer = buffer->next_free;
		}
		else if (buffer_count < sizeof(buffers) / sizeof(buffers[0])) {
			buffer = &buffers[buffer_count++];
		}
		else {
			Sys_Error("Too many graphics buffers allocated");
		}
	}

	memset(buffer, 0, sizeof(buffers[0]));

	if (name) {
		strlcpy(buffer->name, name, sizeof(buffer->name));
	}
	buffer->target = target;
	buffer->size = size;
	buffer->usage = usage;
	glGenBuffers(1, &buffer->glref);

	GL_BindBufferImpl(target, buffer->glref);
	glBufferData(target, size, data, usage);
	result.index = buffer - buffers;
	return result;
}

void GL_UpdateVBO(buffer_ref vbo, size_t size, void* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);
	assert(data);
	assert(size <= buffers[vbo.index].size);

	GL_BindBuffer(vbo);
	glBufferSubData(buffers[vbo.index].target, 0, size, data);
}

size_t GL_VBOSize(buffer_ref vbo)
{
	if (!vbo.index) {
		return 0;
	}

	return buffers[vbo.index].size;
}

void GL_ResizeBuffer(buffer_ref vbo, size_t size, void* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);
	assert(data);

	GL_BindBuffer(vbo);
	glBufferData(buffers[vbo.index].target, size, data, buffers[vbo.index].usage);
}

void GL_UpdateVBOSection(buffer_ref vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);
	assert(data);
	assert(offset >= 0);
	assert(offset < buffers[vbo.index].size);
	assert(offset + size <= buffers[vbo.index].size);

	GL_BindBuffer(vbo);
	glBufferSubData(buffers[vbo.index].target, offset, size, data);
}

void GL_DeleteBuffers(void)
{
	int i;

	for (i = 0; i < buffer_count; ++i) {
		if (buffers[i].glref) {
			if (glDeleteBuffers) {
				glDeleteBuffers(1, &buffers[i].glref);
			}
		}
	}
	memset(buffers, 0, sizeof(buffers));
	next_free_buffer = NULL;
}

void GL_BindBufferBase(buffer_ref ref, GLuint index)
{
	assert(ref.index);
	assert(buffers[ref.index].glref);

	glBindBufferBase(buffers[ref.index].target, index, buffers[ref.index].glref);
}

static void GL_BindBufferImpl(GLenum target, GLuint buffer)
{
	if (target == GL_ARRAY_BUFFER) {
		if (buffer == currentArrayBuffer) {
			return;
		}
		currentArrayBuffer = buffer;
	}
	else if (target == GL_UNIFORM_BUFFER) {
		if (buffer == currentUniformBuffer) {
			return;
		}
		currentUniformBuffer = buffer;
	}

	glBindBuffer(target, buffer);
}

void GL_BindBuffer(buffer_ref ref)
{
	if (!(GL_BufferReferenceIsValid(ref) && buffers[ref.index].glref)) {
		return;
	}

	GL_BindBufferImpl(buffers[ref.index].target, buffers[ref.index].glref);
}

void GL_InitialiseBufferHandling(void)
{
	glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBuffer");
	glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferData");
	glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubData");
	glGenBuffers = (glGenBuffers_t)SDL_GL_GetProcAddress("glGenBuffers");
	glDeleteBuffers = (glDeleteBuffers_t)SDL_GL_GetProcAddress("glDeleteBuffers");

	// Keeping this because it was in earlier versions of ezquake, I think we're past the days of OpenGL 1.x support tho?
	if (!glBindBuffer && SDL_GL_ExtensionSupported("GL_ARB_vertex_buffer_object")) {
		glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBufferARB");
		glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferDataARB");
		glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubDataARB");
		glGenBuffers = (glGenBuffers_t)SDL_GL_GetProcAddress("glGenBuffersARB");
		glDeleteBuffers = (glDeleteBuffers_t)SDL_GL_GetProcAddress("glDeleteBuffersARB");
	}

	// OpenGL 3.0 onwards, more for shaders
	glBindBufferBase = (glBindBufferBase_t)SDL_GL_GetProcAddress("glBindBufferBase");

	buffers_supported = (glBindBuffer && glBufferData && glBufferSubData && glGenBuffers && glDeleteBuffers);
}

buffer_ref GL_GenUniformBuffer(const char* name, void* data, GLuint size)
{
	return GL_GenFixedBuffer(GL_UNIFORM_BUFFER, name, size, data, GL_STREAM_DRAW);
}

void GL_InitialiseBufferState(void)
{
	currentArrayBuffer = currentUniformBuffer = 0;
}

void GL_UnBindBuffer(GLenum target)
{
	GL_BindBufferImpl(target, 0);
}

qbool GL_VBOsSupported(void)
{
	return buffers_supported;
}
