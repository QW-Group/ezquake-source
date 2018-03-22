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
#include "r_local.h"

static void GL_BindBufferImpl(GLenum target, GLuint buffer);
void GL_BindVertexArrayElementBuffer(buffer_ref ref);

typedef struct buffer_data_s {
	GLuint glref;
	char name[64];

	// These set at creation
	GLenum target;
	GLsizei size;
	GLuint usage;                // flags for BufferData()
	GLuint storageFlags;         // flags for BufferStorage()

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

// VAOs
typedef void (APIENTRY *glGenVertexArrays_t)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY *glBindVertexArray_t)(GLuint arrayNum);
typedef void (APIENTRY *glDeleteVertexArrays_t)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY *glEnableVertexAttribArray_t)(GLuint index);
typedef void (APIENTRY *glVertexAttribPointer_t)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribIPointer_t)(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
typedef void (APIENTRY *glVertexAttribDivisor_t)(GLuint index, GLuint divisor);

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
typedef GLenum (APIENTRY *glClientWaitSync_t)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRY *glDeleteSync_t)(GLsync sync);

// VAO functions
static glGenVertexArrays_t         qglGenVertexArrays = NULL;
static glBindVertexArray_t         qglBindVertexArray = NULL;
static glEnableVertexAttribArray_t qglEnableVertexAttribArray = NULL;
static glDeleteVertexArrays_t      qglDeleteVertexArrays = NULL;
static glVertexAttribPointer_t     qglVertexAttribPointer = NULL;
static glVertexAttribIPointer_t    qglVertexAttribIPointer = NULL;
static glVertexAttribDivisor_t     qglVertexAttribDivisor = NULL;

// Buffer functions
static glBindBuffer_t        qglBindBuffer = NULL;
static glBufferData_t        qglBufferData = NULL;
static glBufferSubData_t     qglBufferSubData = NULL;
static glGenBuffers_t        qglGenBuffers = NULL;
static glDeleteBuffers_t     qglDeleteBuffers = NULL;
static glBindBufferBase_t    qglBindBufferBase = NULL;
static glBindBufferRange_t   qglBindBufferRange = NULL;
static glUnmapBuffer_t       qglUnmapBuffer = NULL;

// DSA
static glNamedBufferSubData_t qglNamedBufferSubData = NULL;
static glNamedBufferData_t    qglNamedBufferData = NULL;
static glUnmapNamedBuffer_t   qglUnmapNamedBuffer = NULL;

// Persistent mapped buffers
static glMapBufferRange_t qglMapBufferRange = NULL;
static glBufferStorage_t  qglBufferStorage = NULL;
static glFenceSync_t      qglFenceSync = NULL;
static glClientWaitSync_t qglClientWaitSync = NULL;
static glDeleteSync_t     qglDeleteSync = NULL;

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
					qglDeleteBuffers(1, &buffer->glref);
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
	qglGenBuffers(1, &buffer->glref);
	return buffer;
}

buffer_ref GL_GenFixedBuffer(GLenum target, const char* name, int size, void* data, GLenum usage)
{
	buffer_data_t* buffer = GL_BufferAllocateSlot(target, name, size, usage);
	buffer_ref result;

	buffer->persistent_mapped_ptr = NULL;
	GL_BindBufferImpl(target, buffer->glref);
	GL_ObjectLabel(GL_BUFFER, buffer->glref, -1, name);
	qglBufferData(target, size, data, usage);
	result.index = buffer - buffers;
	if (target == GL_ELEMENT_ARRAY_BUFFER) {
		GL_BindVertexArrayElementBuffer(result);
	}
	return result;
}

static GLenum GL_BufferTypeToTarget(buffertype_t type)
{
	switch (type) {
		case buffertype_index:
			return GL_ELEMENT_ARRAY_BUFFER;
		case buffertype_indirect:
			return GL_DRAW_INDIRECT_BUFFER;
		case buffertype_storage:
			return GL_SHADER_STORAGE_BUFFER;
		case buffertype_vertex:
			return GL_ARRAY_BUFFER;
		case buffertype_uniform:
			return GL_UNIFORM_BUFFER;
	}

	return GL_ARRAY_BUFFER;
}

buffer_ref GL_CreateFixedBuffer(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	GLenum target = GL_BufferTypeToTarget(type);
	buffer_data_t * buffer = GL_BufferAllocateSlot(target, name, size, usage);
	buffer_ref ref;

	GLenum glUsage;
	qbool tripleBuffer = false;
	qbool useStorage = false;
	GLbitfield storageFlags = 0;
	GLsizei alignment = 1;

	if (usage == bufferusage_once_per_frame) {
		glUsage = GL_STREAM_DRAW;

		useStorage = tripleBuffer = tripleBuffer_supported;
		storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	}
	else if (usage == bufferusage_reuse_per_frame) {
		glUsage = GL_STREAM_DRAW;

		useStorage = (qglBufferStorage != NULL);
		tripleBuffer = false;
	}
	else if (usage == bufferusage_reuse_many_frames) {
		glUsage = GL_STATIC_DRAW;
		storageFlags = GL_MAP_WRITE_BIT;
		useStorage = (qglBufferStorage != NULL);
		if (!data) {
			Sys_Error("Buffer %s flagged as constant but no initial data", name);
			return null_buffer_reference;
		}
	}
	else if (usage == bufferusage_constant_data) {
		glUsage = GL_STATIC_COPY;
		storageFlags = 0;
		useStorage = (qglBufferStorage != NULL);
		if (!data) {
			Sys_Error("Buffer %s flagged as constant but no initial data", name);
			return null_buffer_reference;
		}
	}
	else {
		Sys_Error("Unknown usage flag passed to GL_CreateFixedBuffer");
		return null_buffer_reference;
	}

	if (!useStorage) {
		// Fall back to standard mutable buffer
		return GL_GenFixedBuffer(target, name, size, data, glUsage);
	}

	GL_BindBufferImpl(target, buffer->glref);
	if (target == GL_UNIFORM_BUFFER) {
		alignment = glConfig.uniformBufferOffsetAlignment;
	}
	else if (target == GL_SHADER_STORAGE_BUFFER) {
		alignment = glConfig.shaderStorageBufferOffsetAlignment;
	}

	if (alignment > 1) {
		buffer->size = size = ((size + (alignment - 1)) / alignment) * alignment;
	}

	GL_ObjectLabel(GL_BUFFER, buffer->glref, -1, name);

	if (tripleBuffer) {
		qglBufferStorage(target, size * 3, NULL, storageFlags);
		buffer->persistent_mapped_ptr = qglMapBufferRange(target, 0, size * 3, storageFlags);

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
	}
	else {
		qglBufferStorage(target, size, data, storageFlags);
	}

	buffer->storageFlags = storageFlags;
	ref.index = buffer - buffers;
	return ref;
}

void GL_UpdateBuffer(buffer_ref vbo, GLsizei size, void* data)
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
		if (qglNamedBufferSubData) {
			qglNamedBufferSubData(buffers[vbo.index].glref, 0, (GLsizei)size, data);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferSubData(buffers[vbo.index].target, 0, (GLsizeiptr)size, data);
		}

		GL_LogAPICall("GL_UpdateBuffer(%s)", buffers[vbo.index].name);
	}
}

void GL_BindAndUpdateBuffer(buffer_ref vbo, GLsizei size, void* data)
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

buffer_ref GL_ResizeBuffer(buffer_ref vbo, int size, void* data)
{
	assert(vbo.index);
	assert(buffers[vbo.index].glref);

	if (buffers[vbo.index].persistent_mapped_ptr) {
		if (qglUnmapNamedBuffer) {
			qglUnmapNamedBuffer(buffers[vbo.index].glref);
		}
		else {
			GL_BindBuffer(vbo);
			qglUnmapBuffer(buffers[vbo.index].target);
		}
		qglDeleteBuffers(1, &buffers[vbo.index].glref);

		buffers[vbo.index].next_free = next_free_buffer;

		return GL_CreateFixedBuffer(buffers[vbo.index].target, buffers[vbo.index].name, size, data, bufferusage_once_per_frame);
	}
	else {
		if (qglNamedBufferData) {
			qglNamedBufferData(buffers[vbo.index].glref, size, data, buffers[vbo.index].usage);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferData(buffers[vbo.index].target, size, data, buffers[vbo.index].usage);
		}

		buffers[vbo.index].size = size;
		GL_LogAPICall("GL_ResizeBuffer(%s)", buffers[vbo.index].name);
		return vbo;
	}
}

void GL_UpdateBufferSection(buffer_ref vbo, GLintptr offset, GLsizei size, const GLvoid* data)
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
		if (qglNamedBufferSubData) {
			qglNamedBufferSubData(buffers[vbo.index].glref, offset, size, data);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferSubData(buffers[vbo.index].target, offset, size, data);
		}
	}
	GL_LogAPICall("GL_UpdateBufferSection(%s)", buffers[vbo.index].name);
}

void GL_DeleteBuffers(void)
{
	int i;

	for (i = 0; i < buffer_count; ++i) {
		if (buffers[i].glref) {
			if (qglDeleteBuffers) {
				qglDeleteBuffers(1, &buffers[i].glref);
			}
		}
	}
	memset(buffers, 0, sizeof(buffers));
	next_free_buffer = NULL;

	for (i = 0; i < 3; ++i) {
		if (tripleBufferSyncObjects[i]) {
			qglDeleteSync(tripleBufferSyncObjects[i]);
		}
	}
	memset(tripleBufferSyncObjects, 0, sizeof(tripleBufferSyncObjects));
}

void GL_BindBufferBase(buffer_ref ref, GLuint index)
{
	assert(ref.index);
	assert(buffers[ref.index].glref);

	qglBindBufferBase(buffers[ref.index].target, index, buffers[ref.index].glref);
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

	qglBindBufferRange(buffers[ref.index].target, index, buffers[ref.index].glref, offset, size);
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

	qglBindBuffer(target, buffer);
}

void GL_BindBuffer(buffer_ref ref)
{
	if (!(GL_BufferReferenceIsValid(ref) && buffers[ref.index].glref)) {
		GL_LogAPICall("GL_BindBuffer(<invalid-reference:%s>)", buffers[ref.index].name);
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
	buffers_supported = true;

	GL_LoadMandatoryFunctionExtension(glBindBuffer, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glBufferData, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glBufferSubData, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glGenBuffers, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteBuffers, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glUnmapBuffer, buffers_supported);

	// VAOs
	GL_LoadMandatoryFunctionExtension(glGenVertexArrays, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glBindVertexArray, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteVertexArrays, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glEnableVertexAttribArray, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribPointer, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribIPointer, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glVertexAttribDivisor, buffers_supported);

	// OpenGL 3.0 onwards, for 4.3+ support only
	GL_LoadOptionalFunction(glBindBufferBase);
	GL_LoadOptionalFunction(glBindBufferRange);

	// OpenGL 4.4, persistent mapping of buffers
	tripleBuffer_supported = !COM_CheckParm(cmdline_param_client_notriplebuffering);
	GL_LoadMandatoryFunctionExtension(glFenceSync, tripleBuffer_supported);
	GL_LoadMandatoryFunctionExtension(glClientWaitSync, tripleBuffer_supported);
	GL_LoadMandatoryFunctionExtension(glBufferStorage, tripleBuffer_supported);
	GL_LoadMandatoryFunctionExtension(glMapBufferRange, tripleBuffer_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteSync, tripleBuffer_supported);

	// OpenGL 4.5 onwards, update directly
	if (GL_UseDirectStateAccess()) {
		GL_LoadOptionalFunction(glNamedBufferSubData);
		GL_LoadOptionalFunction(glNamedBufferData);
		GL_LoadOptionalFunction(glUnmapNamedBuffer);
	}

	tripleBuffer_supported &= buffers_supported;

	if (!buffers_supported) {
		Con_Printf("Using GL buffers: disabled\n");
	}
	else {
		Con_Printf("Triple-buffering of GL buffers: %s\n", tripleBuffer_supported ? "enabled" : "disabled");
	}
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
		qglBindVertexArray(vao ? vao->vao : 0);
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

	qglEnableVertexAttribArray(index);
	qglVertexAttribPointer(index, size, type, normalized, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
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

	qglEnableVertexAttribArray(index);
	qglVertexAttribIPointer(index, size, type, stride, pointer);
	qglVertexAttribDivisor(index, divisor);
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
		qglDeleteVertexArrays(1, &vao->vao);
	}
	else {
		vao->next = vao_list;
		vao_list = vao;
	}
	qglGenVertexArrays(1, &vao->vao);
	GL_BindVertexArray(vao);
	GL_ObjectLabel(GL_VERTEX_ARRAY, vao->vao, -1, name);
	strlcpy(vao->name, name, sizeof(vao->name));
	GL_SetElementArrayBuffer(null_buffer_reference);
}

void GL_DeleteVAOs(void)
{
	glm_vao_t* vao = vao_list;
	if (qglBindVertexArray) {
		qglBindVertexArray(0);
	}
	while (vao) {
		glm_vao_t* prev = vao;

		if (vao->vao) {
			if (qglDeleteVertexArrays) {
				qglDeleteVertexArrays(1, &vao->vao);
			}
			vao->vao = 0;
		}

		vao = vao->next;
		prev->next = NULL;
	}
	vao_list = NULL;
}

void GL_EnsureBufferSize(buffer_ref ref, GLsizei size)
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
		GLenum waitRet = qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], 0, 0);
		while (waitRet == GL_TIMEOUT_EXPIRED) {
			// Flush commands and wait for longer
			waitRet = qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
		}
	}
}

void GL_BufferEndFrame(void)
{
	if (tripleBuffer_supported) {
		if (tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
			qglDeleteSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex]);
		}
		tripleBufferSyncObjects[glConfig.tripleBufferIndex] = qglFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glConfig.tripleBufferIndex = (glConfig.tripleBufferIndex + 1) % 3;
	}
}

uintptr_t GL_BufferOffset(buffer_ref ref)
{
	return buffers[ref.index].persistent_mapped_ptr ? buffers[ref.index].size * glConfig.tripleBufferIndex : 0;
}

qbool GL_BuffersReady(void)
{
	if (tripleBuffer_supported && tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
		if (qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], 0, 0) == GL_TIMEOUT_EXPIRED) {
			return false;
		}
	}
	return true;
}
