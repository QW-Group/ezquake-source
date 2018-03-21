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
#include "tr_types.h"

static void GL_BindBufferImpl(GLenum target, GLuint buffer);
void GL_BindVertexArrayElementBuffer(buffer_ref ref);

typedef struct buffer_data_s {
	GLuint glref;
	char name[64];

	// These set at creation
	GLenum target;
	size_t size;
	GLuint usage;

	void* persistent_mapped_ptr;

	struct buffer_data_s* next_free;
} buffer_data_t;

static buffer_data_t buffers[256];
static int buffer_count;
static buffer_data_t* next_free_buffer = NULL;
static qbool buffers_supported = false;
static qbool tripleBuffer_supported = false;
static GLsync tripleBufferSyncObjects[3];

// Linked list of all vao buffers
static glm_vao_t* vao_list = NULL;
static glm_vao_t* currentVAO = NULL;

typedef void (APIENTRY *glBindBuffer_t)(GLenum target, GLuint buffer);
typedef void (APIENTRY *glBufferData_t)(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
typedef void (APIENTRY *glBufferSubData_t)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
typedef void (APIENTRY *glGenBuffers_t)(GLsizei n, GLuint* buffers);
typedef void (APIENTRY *glDeleteBuffers_t)(GLsizei n, const GLuint* buffers);
typedef void (APIENTRY *glBindBufferBase_t)(GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRY *glBindBufferRange_t)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void (APIENTRY *glNamedBufferSubData_t)(GLuint buffer, GLintptr offset, GLsizei size, const void* data);
typedef void (APIENTRY *glNamedBufferData_t)(GLuint buffer, GLsizei size, const void* data, GLenum usage);
typedef void (APIENTRY *glUnmapNamedBuffer_t)(GLuint buffer);
typedef void (APIENTRY *glUnmapBuffer_t)(GLenum target);

// AZDO-buffer-streaming (4.4)
typedef void* (APIENTRY *glMapBufferRange_t)(GLenum mtarget, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRY *glBufferStorage_t)(GLenum target, GLsizeiptr size, const GLvoid* data, GLbitfield flags);
typedef GLsync (APIENTRY *glFenceSync_t)(GLenum condition, GLbitfield flags);
//typedef void (APIENTRY *glWaitSync_t)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef GLenum (APIENTRY *glClientWaitSync_t)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRY *glDeleteSync_t)(GLsync sync);

// VAO functions
static glGenVertexArrays_t         glGenVertexArrays = NULL;
static glBindVertexArray_t         glBindVertexArray = NULL;
static glEnableVertexAttribArray_t glEnableVertexAttribArray = NULL;
static glDeleteVertexArrays_t      glDeleteVertexArrays = NULL;
static glVertexAttribPointer_t     glVertexAttribPointer = NULL;
static glVertexAttribIPointer_t    glVertexAttribIPointer = NULL;
static glVertexAttribDivisor_t     glVertexAttribDivisor = NULL;

// Buffer functions
static glBindBuffer_t     glBindBuffer = NULL;
static glBufferData_t     glBufferData = NULL;
static glBufferSubData_t  glBufferSubData = NULL;
static glGenBuffers_t     glGenBuffers = NULL;
static glDeleteBuffers_t  glDeleteBuffers = NULL;
static glBindBufferBase_t glBindBufferBase = NULL;
static glBindBufferRange_t glBindBufferRange = NULL;
static glUnmapBuffer_t glUnmapBuffer = NULL;

// DSA
static glNamedBufferSubData_t glNamedBufferSubData = NULL;
static glNamedBufferData_t    glNamedBufferData = NULL;
static glUnmapNamedBuffer_t   glUnmapNamedBuffer = NULL;

// Persistent mapped buffers
static glMapBufferRange_t glMapBufferRange = NULL;
static glBufferStorage_t  glBufferStorage = NULL;
static glFenceSync_t      glFenceSync = NULL;
static glClientWaitSync_t glClientWaitSync = NULL;
static glDeleteSync_t     glDeleteSync = NULL;

// Cache OpenGL state
static GLuint currentArrayBuffer;
static GLuint currentUniformBuffer;
static GLuint currentDrawIndirectBuffer;
static GLuint currentElementArrayBuffer;

static buffer_data_t* GL_BufferAllocateSlot(GLenum target, const char* name, GLsizei size, GLenum usage)
{
	buffer_data_t* buffer = NULL;
	int i;

	if (name && name[0]) {
		for (i = 0; i < buffer_count; ++i) {
			if (!strcmp(name, buffers[i].name)) {
				buffer = &buffers[i];
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
		memset(buffer, 0, sizeof(buffers[0]));
		if (name) {
			strlcpy(buffer->name, name, sizeof(buffer->name));
		}
	}

	buffer->target = target;
	buffer->size = size;
	buffer->usage = usage;
	glGenBuffers(1, &buffer->glref);
	return buffer;
}

buffer_ref GL_GenFixedBuffer(GLenum target, const char* name, GLsizei size, void* data, GLenum usage)
{
	buffer_data_t* buffer = GL_BufferAllocateSlot(target, name, size, usage);
	buffer_ref result;

	buffer->persistent_mapped_ptr = NULL;
	GL_BindBufferImpl(target, buffer->glref);
	if (glObjectLabel && name) {
		glObjectLabel(GL_BUFFER, buffer->glref, -1, name);
	}
	glBufferData(target, size, data, usage);
	result.index = buffer - buffers;
	if (target == GL_ELEMENT_ARRAY_BUFFER) {
		GL_BindVertexArrayElementBuffer(result);
	}
	return result;
}

buffer_ref GL_CreateFixedBuffer(GLenum target, const char* name, GLsizei size, void* data, buffertype_t usage)
{
	GLenum glUsage;
	qbool tripleBuffer = false;

	if (usage == write_once_use_once) {
		glUsage = GL_STREAM_DRAW;
		
		tripleBuffer = tripleBuffer_supported;
	}
	else if (usage == write_once_use_once_safe) {
		glUsage = GL_STREAM_DRAW;
	}
	else if (usage == write_once_use_many) {
		glUsage = GL_STATIC_DRAW;
	}
	else if (usage == write_once_read_many) {
		glUsage = GL_STATIC_COPY;
	}
	else {
		Sys_Error("Unknown usage flag passed to GL_CreateFixedBuffer");
		return null_buffer_reference;
	}

	if (!tripleBuffer) {
		return GL_GenFixedBuffer(target, name, size, data, glUsage);
	}
	else {
		GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		buffer_data_t * buffer = GL_BufferAllocateSlot(target, name, size, usage);
		buffer_ref ref;

		GL_BindBufferImpl(target, buffer->glref);
		if (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) {
			GLsizei alignment = (target == GL_UNIFORM_BUFFER ? glConfig.uniformBufferOffsetAlignment : glConfig.shaderStorageBufferOffsetAlignment);

			size = ((size + (alignment - 1)) / alignment) * alignment;

			buffer->size = size;
		}
		glBufferStorage(target, size * 3, NULL, flags);
		buffer->persistent_mapped_ptr = glMapBufferRange(target, 0, size * 3, flags);

		if (buffer->persistent_mapped_ptr) {
			if (data) {
				void* base = (void*)((uintptr_t)buffer->persistent_mapped_ptr + size * glConfig.tripleBufferIndex);

				memcpy(base, data, size);
			}
		}
		else {
			Con_Printf("\20opengl\21 triple-buffered allocation failed (%dKB)\n", (size * 3) / 1024);
			return GL_GenFixedBuffer(target, name, size, data, glUsage);
		}

		if (glObjectLabel && name) {
			glObjectLabel(GL_BUFFER, buffer->glref, -1, name);
		}
		ref.index = buffer - buffers;
		return ref;
	}
}

void GL_UpdateBuffer(buffer_ref vbo, size_t size, void* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);
	assert(data);
	assert(size <= buffers[vbo.index].size);

	if (buffers[vbo.index].persistent_mapped_ptr) {
		void* start = (void*)((uintptr_t)buffers[vbo.index].persistent_mapped_ptr + GL_BufferOffset(vbo));

		memcpy(start, data, size);

		GL_LogAPICall("GL_UpdateBuffer[memcpy](%s)", buffers[vbo.index].name);
	}
	else {
		if (glNamedBufferSubData) {
			glNamedBufferSubData(buffers[vbo.index].glref, 0, size, data);
		}
		else {
			GL_BindBuffer(vbo);
			glBufferSubData(buffers[vbo.index].target, 0, size, data);
		}

		GL_LogAPICall("GL_UpdateBuffer(%s)", buffers[vbo.index].name);
	}
}

void GL_BindAndUpdateBuffer(buffer_ref vbo, size_t size, void* data)
{
	GL_BindBuffer(vbo);
	GL_UpdateBuffer(vbo, size, data);
}

size_t GL_BufferSize(buffer_ref vbo)
{
	if (!vbo.index) {
		return 0;
	}

	return buffers[vbo.index].size;
}

buffer_ref GL_ResizeBuffer(buffer_ref vbo, size_t size, void* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);

	if (buffers[vbo.index].persistent_mapped_ptr) {
		if (glUnmapNamedBuffer) {
			glUnmapNamedBuffer(buffers[vbo.index].glref);
		}
		else {
			GL_BindBuffer(vbo);
			glUnmapBuffer(buffers[vbo.index].target);
		}
		glDeleteBuffers(1, &buffers[vbo.index].glref);

		buffers[vbo.index].next_free = next_free_buffer;

		return GL_CreateFixedBuffer(buffers[vbo.index].target, buffers[vbo.index].name, size, data, write_once_use_once);
	}
	else {
		if (glNamedBufferData) {
			glNamedBufferData(buffers[vbo.index].glref, size, data, buffers[vbo.index].usage);
		}
		else {
			GL_BindBuffer(vbo);
			glBufferData(buffers[vbo.index].target, size, data, buffers[vbo.index].usage);
		}

		buffers[vbo.index].size = size;
		GL_LogAPICall("GL_ResizeBuffer(%s)", buffers[vbo.index].name);
		return vbo;
	}
}

void GL_UpdateBufferSection(buffer_ref vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);
	assert(data);
	assert(offset >= 0);
	assert(offset < buffers[vbo.index].size);
	assert(offset + size <= buffers[vbo.index].size);

	if (buffers[vbo.index].persistent_mapped_ptr) {
		void* base = (void*)((uintptr_t)buffers[vbo.index].persistent_mapped_ptr + GL_BufferOffset(vbo) + offset);

		memcpy(base, data, size);
	}
	else {
		if (glNamedBufferSubData) {
			glNamedBufferSubData(buffers[vbo.index].glref, offset, size, data);
		}
		else {
			GL_BindBuffer(vbo);
			glBufferSubData(buffers[vbo.index].target, offset, size, data);
		}
	}
	GL_LogAPICall("GL_UpdateBufferSection(%s)", buffers[vbo.index].name);
}

void GL_BindAndUpdateBufferSection(buffer_ref vbo, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	GL_BindBuffer(vbo);
	GL_UpdateBufferSection(vbo, offset, size, data);
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

	for (i = 0; i < 3; ++i) {
		if (tripleBufferSyncObjects[i]) {
			glDeleteSync(tripleBufferSyncObjects[i]);
		}
	}
	memset(tripleBufferSyncObjects, 0, sizeof(tripleBufferSyncObjects));
}

void GL_BindBufferBase(buffer_ref ref, GLuint index)
{
	assert(ref.index);
	assert(buffers[ref.index].glref);

	glBindBufferBase(buffers[ref.index].target, index, buffers[ref.index].glref);
}

void GL_BindBufferRange(buffer_ref ref, GLuint index, GLintptr offset, GLsizeiptr size)
{
	if (size == 0) {
		return;
	}

	assert(ref.index);
	assert(buffers[ref.index].glref);
	assert(size >= 0);
	assert(offset >= 0);
	assert(offset + size <= buffers[ref.index].size * (buffers[ref.index].persistent_mapped_ptr ? 3 : 1));

	glBindBufferRange(buffers[ref.index].target, index, buffers[ref.index].glref, offset, size);
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
	else if (target == GL_DRAW_INDIRECT_BUFFER) {
		if (buffer == currentDrawIndirectBuffer) {
			return;
		}

		currentDrawIndirectBuffer = buffer;
	}
	else if (target == GL_ELEMENT_ARRAY_BUFFER) {
		if (buffer == currentElementArrayBuffer) {
			return;
		}

		currentElementArrayBuffer = buffer;
	}

	glBindBuffer(target, buffer);
}

void GL_BindBuffer(buffer_ref ref)
{
	if (!(GL_BufferReferenceIsValid(ref) && buffers[ref.index].glref)) {
		return;
	}

	GL_BindBufferImpl(buffers[ref.index].target, buffers[ref.index].glref);

	if (buffers[ref.index].target == GL_ELEMENT_ARRAY_BUFFER) {
		GL_BindVertexArrayElementBuffer(ref);
	}

	GL_LogAPICall("GL_BindBuffer(%s)", buffers[ref.index].name);
}

void GL_InitialiseBufferHandling(void)
{
	glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBuffer");
	glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferData");
	glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubData");
	glGenBuffers = (glGenBuffers_t)SDL_GL_GetProcAddress("glGenBuffers");
	glDeleteBuffers = (glDeleteBuffers_t)SDL_GL_GetProcAddress("glDeleteBuffers");
	glUnmapBuffer = (glUnmapBuffer_t)SDL_GL_GetProcAddress("glUnmapBuffer");

	// Keeping this because it was in earlier versions of ezquake, I think we're past the days of OpenGL 1.x support tho?
	if (!glBindBuffer && SDL_GL_ExtensionSupported("GL_ARB_vertex_buffer_object")) {
		glBindBuffer = (glBindBuffer_t)SDL_GL_GetProcAddress("glBindBufferARB");
		glBufferData = (glBufferData_t)SDL_GL_GetProcAddress("glBufferDataARB");
		glBufferSubData = (glBufferSubData_t)SDL_GL_GetProcAddress("glBufferSubDataARB");
		glGenBuffers = (glGenBuffers_t)SDL_GL_GetProcAddress("glGenBuffersARB");
		glDeleteBuffers = (glDeleteBuffers_t)SDL_GL_GetProcAddress("glDeleteBuffersARB");
		glUnmapBuffer = (glUnmapBuffer_t)SDL_GL_GetProcAddress("glUnmapBufferARB");
	}

	buffers_supported = (glBindBuffer && glBufferData && glBufferSubData && glGenBuffers && glDeleteBuffers && glUnmapBuffer);

	// OpenGL 3.0 onwards, for 4.3+ support only
	glBindBufferBase = (glBindBufferBase_t)SDL_GL_GetProcAddress("glBindBufferBase");
	glBindBufferRange = (glBindBufferRange_t)SDL_GL_GetProcAddress("glBindBufferRange");

	// OpenGL 4.4, persistent mapping of buffers
	glFenceSync = (glFenceSync_t)SDL_GL_GetProcAddress("glFenceSync");
	glClientWaitSync = (glClientWaitSync_t)SDL_GL_GetProcAddress("glClientWaitSync");
	glBufferStorage = (glBufferStorage_t)SDL_GL_GetProcAddress("glBufferStorage");
	glMapBufferRange = (glMapBufferRange_t)SDL_GL_GetProcAddress("glMapBufferRange");
	glDeleteSync = (glDeleteSync_t)SDL_GL_GetProcAddress("glDeleteSync");

	// OpenGL 4.5 onwards, update directly
	glNamedBufferSubData = (glNamedBufferSubData_t)SDL_GL_GetProcAddress("glNamedBufferSubData");
	glNamedBufferData = (glNamedBufferData_t)SDL_GL_GetProcAddress("glNamedBufferData");
	glUnmapNamedBuffer = (glUnmapNamedBuffer_t)SDL_GL_GetProcAddress("glUnmapNamedBuffer");

	// VAOs
	glGenVertexArrays = (glGenVertexArrays_t)SDL_GL_GetProcAddress("glGenVertexArrays");
	glBindVertexArray = (glBindVertexArray_t)SDL_GL_GetProcAddress("glBindVertexArray");
	glDeleteVertexArrays = (glDeleteVertexArrays_t)SDL_GL_GetProcAddress("glDeleteVertexArrays");
	glEnableVertexAttribArray = (glEnableVertexAttribArray_t)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
	glVertexAttribPointer = (glVertexAttribPointer_t)SDL_GL_GetProcAddress("glVertexAttribPointer");
	glVertexAttribIPointer = (glVertexAttribIPointer_t)SDL_GL_GetProcAddress("glVertexAttribIPointer");
	glVertexAttribDivisor = (glVertexAttribDivisor_t)SDL_GL_GetProcAddress("glVertexAttribDivisor");

	buffers_supported &= (glGenVertexArrays && glBindVertexArray && glDeleteVertexArrays && glEnableVertexAttribArray);
	buffers_supported &= (glVertexAttribPointer && glVertexAttribIPointer && glVertexAttribDivisor);

	tripleBuffer_supported = buffers_supported && glFenceSync && glClientWaitSync && glBufferStorage && glMapBufferRange && !COM_CheckParm("-no-triple-gl-buffer");
	Con_Printf("Triple-buffering of GL buffers: %s\n", tripleBuffer_supported ? "enabled" : "disabled");
}

void GL_InitialiseBufferState(void)
{
	currentDrawIndirectBuffer = currentArrayBuffer = currentUniformBuffer = 0;
	GL_BindVertexArrayElementBuffer(null_buffer_reference);
	currentVAO = NULL;
}

void GL_UnBindBuffer(GLenum target)
{
	GL_BindBufferImpl(target, 0);
	GL_LogAPICall("GL_UnBindBuffer(%s)", target == GL_ARRAY_BUFFER ? "array-buffer" : (target == GL_ELEMENT_ARRAY_BUFFER ? "element-array" : "?"));
}

qbool GL_BuffersSupported(void)
{
	return buffers_supported;
}

qbool GL_BufferValid(buffer_ref buffer)
{
	return buffer.index && buffer.index < sizeof(buffers) / sizeof(buffers[0]) && buffers[buffer.index].glref != 0;
}

// Called when the VAO is bound
void GL_SetElementArrayBuffer(buffer_ref buffer)
{
	if (GL_BufferValid(buffer)) {
		currentElementArrayBuffer = buffers[buffer.index].glref;
	}
	else {
		currentElementArrayBuffer = 0;
	}
}

void GL_BindVertexArray(glm_vao_t* vao)
{
	if (currentVAO != vao) {
		glBindVertexArray(vao ? vao->vao : 0);
		currentVAO = vao;

		if (vao) {
			GL_SetElementArrayBuffer(vao->element_array_buffer);
		}

		GL_LogAPICall("GL_BindVertexArray()");
	}
}

void GL_BindVertexArrayElementBuffer(buffer_ref ref)
{
	if (currentVAO && currentVAO->vao) {
		currentVAO->element_array_buffer = ref;
	}
}

void GL_ConfigureVertexAttribPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vao->vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(vbo)) {
		GL_BindBuffer(vbo);
	}
	else {
		GL_UnBindBuffer(GL_ARRAY_BUFFER);
	}

	glEnableVertexAttribArray(index);
	glVertexAttribPointer(index, size, type, normalized, stride, pointer);
	glVertexAttribDivisor(index, divisor);
}

void GL_ConfigureVertexAttribIPointer(glm_vao_t* vao, buffer_ref vbo, GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer, int divisor)
{
	assert(vao);
	assert(vao->vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(vbo)) {
		GL_BindBuffer(vbo);
	}
	else {
		GL_UnBindBuffer(GL_ARRAY_BUFFER);
	}

	glEnableVertexAttribArray(index);
	glVertexAttribIPointer(index, size, type, stride, pointer);
	glVertexAttribDivisor(index, divisor);
}

void GL_SetVertexArrayElementBuffer(glm_vao_t* vao, buffer_ref ibo)
{
	assert(vao);
	assert(vao->vao);

	GL_BindVertexArray(vao);
	if (GL_BufferReferenceIsValid(ibo)) {
		GL_BindBuffer(ibo);
	}
	else {
		GL_UnBindBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}

void GL_GenVertexArray(glm_vao_t* vao, const char* name)
{
	extern void GL_SetElementArrayBuffer(buffer_ref buffer);

	if (vao->vao) {
		glDeleteVertexArrays(1, &vao->vao);
	}
	else {
		vao->next = vao_list;
		vao_list = vao;
	}
	glGenVertexArrays(1, &vao->vao);
	GL_BindVertexArray(vao);
	if (glObjectLabel) {
		glObjectLabel(GL_VERTEX_ARRAY, vao->vao, -1, name);
	}
	GL_SetElementArrayBuffer(null_buffer_reference);
}

void GL_DeleteVAOs(void)
{
	glm_vao_t* vao = vao_list;
	if (glBindVertexArray) {
		glBindVertexArray(0);
	}
	while (vao) {
		glm_vao_t* prev = vao;

		if (vao->vao) {
			if (glDeleteVertexArrays) {
				glDeleteVertexArrays(1, &vao->vao);
			}
			vao->vao = 0;
		}

		vao = vao->next;
		prev->next = NULL;
	}
	vao_list = NULL;
}

void GL_EnsureBufferSize(buffer_ref ref, size_t size)
{
	assert(ref.index);
	assert(buffers[ref.index].glref);

	if (buffers[ref.index].size < size) {
		GL_ResizeBuffer(ref, size, NULL);
	}
}

void GL_BufferStartFrame(void)
{
	if (tripleBuffer_supported && tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
		GLenum waitRet = glClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], 0, 0);
		while (waitRet == GL_TIMEOUT_EXPIRED) {
			// Flush commands and wait for longer
			waitRet = glClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
		}
	}
}

void GL_BufferEndFrame(void)
{
	if (tripleBuffer_supported) {
		if (tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
			glDeleteSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex]);
		}
		tripleBufferSyncObjects[glConfig.tripleBufferIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glConfig.tripleBufferIndex = (glConfig.tripleBufferIndex + 1) % 3;
	}
}

uintptr_t GL_BufferOffset(buffer_ref ref)
{
	return buffers[ref.index].persistent_mapped_ptr ? buffers[ref.index].size * glConfig.tripleBufferIndex : 0;
}
