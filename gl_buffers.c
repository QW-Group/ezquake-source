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
#include "glm_vao.h"
#include "r_trace.h"
#include "r_buffers.h"

static void GL_BufferStartFrame(void);
static void GL_BufferEndFrame(void);
static qbool GL_BuffersReady(void);

static void GL_InitialiseBufferState(void);

static buffer_ref GL_CreateFixedBuffer(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage);
static buffer_ref GL_GenFixedBuffer(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage);
static uintptr_t GL_BufferOffset(buffer_ref ref);

static size_t GL_BufferSize(buffer_ref vbo);

static void GL_BindBuffer(buffer_ref ref);
static void GL_BindBufferBase(buffer_ref ref, unsigned int index);
static void GL_BindBufferRange(buffer_ref ref, unsigned int index, ptrdiff_t offset, int size);
static void GL_UnBindBuffer(buffertype_t type);
static void GL_UpdateBuffer(buffer_ref vbo, int size, void* data);
static void GL_UpdateBufferSection(buffer_ref vbo, ptrdiff_t offset, int size, const void* data);
static buffer_ref GL_ResizeBuffer(buffer_ref vbo, int size, void* data);
static void GL_EnsureBufferSize(buffer_ref ref, int size);

static qbool GL_BindBufferImpl(GLenum target, GLuint buffer);

typedef struct buffer_data_s {
	GLuint glref;
	char name[64];

	// These set at creation
	buffertype_t type;
	GLenum target;
	GLsizei size;
	GLuint usage;                // flags for BufferData()
	GLuint storageFlags;         // flags for BufferStorage()

	void* persistent_mapped_ptr;

	struct buffer_data_s* next_free;
} buffer_data_t;

static buffer_data_t buffer_data[256];
static int buffer_count;
static buffer_data_t* next_free_buffer = NULL;
static qbool tripleBuffer_supported = false;
static GLsync tripleBufferSyncObjects[3];

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

	assert(false);
	return 0;
}

static GLenum GL_BufferUsageToGLUsage(bufferusage_t usage)
{
	switch (usage) {
		case bufferusage_unknown:
			return 0;
		case bufferusage_once_per_frame:
			return GL_STREAM_DRAW;
		case bufferusage_reuse_per_frame:
			return GL_STREAM_DRAW;
		case bufferusage_reuse_many_frames:
			return GL_STATIC_DRAW;
		case bufferusage_constant_data:
			return GL_STATIC_COPY;
	}

	assert(false);
	return 0;
}

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

static buffer_data_t* GL_BufferAllocateSlot(buffertype_t type, GLenum target, const char* name, GLsizei size, GLenum usage)
{
	buffer_data_t* buffer = NULL;
	int i;

	if (name && name[0]) {
		for (i = 0; i < buffer_count; ++i) {
			if (!strcmp(name, buffer_data[i].name)) {
				buffer = &buffer_data[i];
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
		else if (buffer_count < sizeof(buffer_data) / sizeof(buffer_data[0])) {
			buffer = &buffer_data[buffer_count++];
		}
		else {
			Sys_Error("Too many graphics buffers allocated");
		}
		memset(buffer, 0, sizeof(buffer_data[0]));
		if (name) {
			strlcpy(buffer->name, name, sizeof(buffer->name));
		}
	}

	buffer->target = target;
	buffer->size = size;
	buffer->usage = usage;
	buffer->type = type;
	qglGenBuffers(1, &buffer->glref);
	return buffer;
}

static buffer_ref GL_GenFixedBuffer(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	GLenum target = GL_BufferTypeToTarget(type);
	GLenum glUsage = GL_BufferUsageToGLUsage(usage);
	buffer_data_t* buffer = GL_BufferAllocateSlot(type, target, name, size, glUsage);
	buffer_ref result;

	buffer->persistent_mapped_ptr = NULL;
	GL_BindBufferImpl(target, buffer->glref);
	GL_ObjectLabel(GL_BUFFER, buffer->glref, -1, name);
	qglBufferData(target, size, data, glUsage);
	result.index = buffer - buffer_data;
	if (target == GL_ELEMENT_ARRAY_BUFFER) {
		R_BindVertexArrayElementBuffer(result);
	}
	return result;
}

static buffer_ref GL_CreateFixedBuffer(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	GLenum target = GL_BufferTypeToTarget(type);
	buffer_data_t* buffer = GL_BufferAllocateSlot(type, target, name, size, usage);
	buffer_ref ref;

	qbool tripleBuffer = false;
	qbool useStorage = false;
	GLbitfield storageFlags = 0;
	GLsizei alignment = 1;

	if (usage == bufferusage_once_per_frame) {
		useStorage = tripleBuffer = tripleBuffer_supported;
		storageFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	}
	else if (usage == bufferusage_reuse_per_frame) {
		useStorage = (qglBufferStorage != NULL);
		storageFlags = GL_DYNAMIC_STORAGE_BIT;
		tripleBuffer = false;
	}
	else if (usage == bufferusage_reuse_many_frames) {
		storageFlags = GL_MAP_WRITE_BIT;
		useStorage = (qglBufferStorage != NULL);
		if (!data) {
			Sys_Error("Buffer %s flagged as constant but no initial data", name);
			return null_buffer_reference;
		}
	}
	else if (usage == bufferusage_constant_data) {
		storageFlags = (data ? 0 : GL_DYNAMIC_STORAGE_BIT);
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
		return GL_GenFixedBuffer(type, name, size, data, usage);
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
			return GL_GenFixedBuffer(type, name, size, data, usage);
		}
	}
	else {
		qglBufferStorage(target, size, data, storageFlags);
	}

	buffer->storageFlags = storageFlags;
	ref.index = buffer - buffer_data;
	return ref;
}

static void GL_UpdateBuffer(buffer_ref vbo, int size, void* data)
{
	assert(vbo.index);
	assert(buffer_data[vbo.index].glref);
	assert(data);
	assert(size <= buffer_data[vbo.index].size);

	if (buffer_data[vbo.index].persistent_mapped_ptr) {
		void* start = (void*)((uintptr_t)buffer_data[vbo.index].persistent_mapped_ptr + GL_BufferOffset(vbo));

		memcpy(start, data, size);

		GL_LogAPICall("GL_UpdateBuffer[memcpy](%s)", buffer_data[vbo.index].name);
	}
	else {
		if (qglNamedBufferSubData) {
			qglNamedBufferSubData(buffer_data[vbo.index].glref, 0, (GLsizei)size, data);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferSubData(buffer_data[vbo.index].target, 0, (GLsizeiptr)size, data);
		}

		GL_LogAPICall("GL_UpdateBuffer(%s)", buffer_data[vbo.index].name);
	}
}

static size_t GL_BufferSize(buffer_ref vbo)
{
	if (!vbo.index) {
		return 0;
	}

	return buffer_data[vbo.index].size;
}

static buffer_ref GL_ResizeBuffer(buffer_ref vbo, int size, void* data)
{
	assert(vbo.index);
	assert(buffer_data[vbo.index].glref);

	if (buffer_data[vbo.index].persistent_mapped_ptr) {
		if (qglUnmapNamedBuffer) {
			qglUnmapNamedBuffer(buffer_data[vbo.index].glref);
		}
		else {
			GL_BindBuffer(vbo);
			qglUnmapBuffer(buffer_data[vbo.index].target);
		}
		qglDeleteBuffers(1, &buffer_data[vbo.index].glref);

		buffer_data[vbo.index].next_free = next_free_buffer;

		return GL_CreateFixedBuffer(buffer_data[vbo.index].type, buffer_data[vbo.index].name, size, data, bufferusage_once_per_frame);
	}
	else {
		if (qglNamedBufferData) {
			qglNamedBufferData(buffer_data[vbo.index].glref, size, data, buffer_data[vbo.index].usage);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferData(buffer_data[vbo.index].target, size, data, buffer_data[vbo.index].usage);
		}

		buffer_data[vbo.index].size = size;
		GL_LogAPICall("GL_ResizeBuffer(%s)", buffer_data[vbo.index].name);
		return vbo;
	}
}

static void GL_UpdateBufferSection(buffer_ref vbo, ptrdiff_t offset, int size, const void* data)
{
	assert(vbo.index);
	assert(buffer_data[vbo.index].glref);
	assert(data);
	assert(offset >= 0);
	assert(offset < buffer_data[vbo.index].size);
	assert(offset + size <= buffer_data[vbo.index].size);

	if (buffer_data[vbo.index].persistent_mapped_ptr) {
		void* base = (void*)((uintptr_t)buffer_data[vbo.index].persistent_mapped_ptr + GL_BufferOffset(vbo) + offset);

		memcpy(base, data, size);
	}
	else {
		if (qglNamedBufferSubData) {
			qglNamedBufferSubData(buffer_data[vbo.index].glref, offset, size, data);
		}
		else {
			GL_BindBuffer(vbo);
			qglBufferSubData(buffer_data[vbo.index].target, offset, size, data);
		}
	}
	GL_LogAPICall("GL_UpdateBufferSection(%s)", buffer_data[vbo.index].name);
}

static void GL_BufferShutdown(void)
{
	int i;

	for (i = 0; i < buffer_count; ++i) {
		if (buffer_data[i].glref) {
			if (qglDeleteBuffers) {
				qglDeleteBuffers(1, &buffer_data[i].glref);
			}
		}
	}
	memset(buffer_data, 0, sizeof(buffer_data));
	next_free_buffer = NULL;

	for (i = 0; i < 3; ++i) {
		if (tripleBufferSyncObjects[i]) {
			qglDeleteSync(tripleBufferSyncObjects[i]);
		}
	}
	memset(tripleBufferSyncObjects, 0, sizeof(tripleBufferSyncObjects));
}

static void GL_BindBufferBase(buffer_ref ref, unsigned int index)
{
	assert(ref.index);
	assert(buffer_data[ref.index].glref);

	qglBindBufferBase(buffer_data[ref.index].target, index, buffer_data[ref.index].glref);
}

static void GL_BindBufferRange(buffer_ref ref, unsigned int index, ptrdiff_t offset, int size)
{
	if (size == 0) {
		return;
	}

	assert(ref.index);
	assert(buffer_data[ref.index].glref);
	assert(size >= 0);
	assert(offset >= 0);
	assert(offset + size <= buffer_data[ref.index].size * (buffer_data[ref.index].persistent_mapped_ptr ? 3 : 1));

	qglBindBufferRange(buffer_data[ref.index].target, index, buffer_data[ref.index].glref, offset, size);
}

static qbool GL_BindBufferImpl(GLenum target, GLuint buffer)
{
	if (target == GL_ARRAY_BUFFER) {
		if (buffer == currentArrayBuffer) {
			return false;
		}
		currentArrayBuffer = buffer;
	}
	else if (target == GL_UNIFORM_BUFFER) {
		if (buffer == currentUniformBuffer) {
			return false;
		}
		currentUniformBuffer = buffer;
	}
	else if (target == GL_DRAW_INDIRECT_BUFFER) {
		if (buffer == currentDrawIndirectBuffer) {
			return false;
		}

		currentDrawIndirectBuffer = buffer;
	}
	else if (target == GL_ELEMENT_ARRAY_BUFFER) {
		if (buffer == currentElementArrayBuffer) {
			return false;
		}

		currentElementArrayBuffer = buffer;
	}

	qglBindBuffer(target, buffer);
	return true;
}

static void GL_BindBuffer(buffer_ref ref)
{
	qbool switched;

	if (!(GL_BufferReferenceIsValid(ref) && buffer_data[ref.index].glref)) {
		GL_LogAPICall("GL_BindBuffer(<invalid-reference:%s>)", buffer_data[ref.index].name);
		return;
	}

	switched = GL_BindBufferImpl(buffer_data[ref.index].target, buffer_data[ref.index].glref);
	if (buffer_data[ref.index].target == GL_ELEMENT_ARRAY_BUFFER) {
		R_BindVertexArrayElementBuffer(ref);
	}

	if (switched) {
		GL_LogAPICall("GL_BindBuffer(%s)", buffer_data[ref.index].name);
	}
}

static void GL_InitialiseBufferState(void)
{
	currentDrawIndirectBuffer = currentArrayBuffer = currentUniformBuffer = 0;
	R_BindVertexArrayElementBuffer(null_buffer_reference);
	R_InitialiseVAOState();
}

static void GL_UnBindBuffer(buffertype_t type)
{
	GLenum target = GL_BufferTypeToTarget(type);
	GL_BindBufferImpl(target, 0);
	GL_LogAPICall("GL_UnBindBuffer(%s)", target == GL_ARRAY_BUFFER ? "array-buffer" : (target == GL_ELEMENT_ARRAY_BUFFER ? "element-array" : "?"));
}

static qbool GL_BufferValid(buffer_ref buffer)
{
	return buffer.index && buffer.index < sizeof(buffer_data) / sizeof(buffer_data[0]) && buffer_data[buffer.index].glref != 0;
}

// Called when the VAO is bound
static void GL_SetElementArrayBuffer(buffer_ref buffer)
{
	if (GL_BufferValid(buffer)) {
		currentElementArrayBuffer = buffer_data[buffer.index].glref;
	}
	else {
		currentElementArrayBuffer = 0;
	}
}

static void GL_EnsureBufferSize(buffer_ref ref, int size)
{
	assert(ref.index);
	assert(buffer_data[ref.index].glref);

	if (buffer_data[ref.index].size < size) {
		GL_ResizeBuffer(ref, size, NULL);
	}
}

static void GL_BufferStartFrame(void)
{
	if (tripleBuffer_supported && tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
		GLenum waitRet = qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], 0, 0);
		while (waitRet == GL_TIMEOUT_EXPIRED) {
			// Flush commands and wait for longer
			waitRet = qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
		}
	}
}

static void GL_BufferEndFrame(void)
{
	if (tripleBuffer_supported) {
		if (tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
			qglDeleteSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex]);
		}
		tripleBufferSyncObjects[glConfig.tripleBufferIndex] = qglFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glConfig.tripleBufferIndex = (glConfig.tripleBufferIndex + 1) % 3;
	}
}

static uintptr_t GL_BufferOffset(buffer_ref ref)
{
	return buffer_data[ref.index].persistent_mapped_ptr ? buffer_data[ref.index].size * glConfig.tripleBufferIndex : 0;
}

static qbool GL_BuffersReady(void)
{
	if (tripleBuffer_supported && tripleBufferSyncObjects[glConfig.tripleBufferIndex]) {
		if (qglClientWaitSync(tripleBufferSyncObjects[glConfig.tripleBufferIndex], 0, 0) == GL_TIMEOUT_EXPIRED) {
			return false;
		}
	}
	return true;
}

#ifdef WITH_OPENGL_TRACE
static void GL_PrintBufferState(FILE* debug_frame_out, int debug_frame_depth)
{
	char label[64];

	if (currentArrayBuffer) {
		GL_GetObjectLabel(GL_BUFFER, currentArrayBuffer, sizeof(label), NULL, label);
		fprintf(debug_frame_out, "%.*s   ArrayBuffer: %u (%s)\n", debug_frame_depth, "                                                          ", currentArrayBuffer, label);
	}
	if (currentElementArrayBuffer) {
		GL_GetObjectLabel(GL_BUFFER, currentElementArrayBuffer, sizeof(label), NULL, label);
		fprintf(debug_frame_out, "%.*s   ElementArray: %u (%s)\n", debug_frame_depth, "                                                          ", currentElementArrayBuffer, label);
	}
}
#endif

void GL_InitialiseBufferHandling(api_buffers_t* api)
{
	qbool buffers_supported = R_InitialiseVAOHandling();

	memset(api, 0, sizeof(*api));

	GL_LoadMandatoryFunctionExtension(glBindBuffer, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glBufferData, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glBufferSubData, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glGenBuffers, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glDeleteBuffers, buffers_supported);
	GL_LoadMandatoryFunctionExtension(glUnmapBuffer, buffers_supported);

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

	api->supported = buffers_supported;
	if (!api->supported) {
		Con_Printf("Using GL buffers: disabled\n");
	}
	else {
		api->Bind = GL_BindBuffer;
		api->BindBase = GL_BindBufferBase;
		api->BindRange = GL_BindBufferRange;
		api->BufferOffset = GL_BufferOffset;
		api->Create = GL_CreateFixedBuffer;

		api->StartFrame = GL_BufferStartFrame;
		api->EndFrame = GL_BufferEndFrame;
		api->FrameReady = GL_BuffersReady;

		api->EnsureSize = GL_EnsureBufferSize;
		api->Resize = GL_ResizeBuffer;

		api->InitialiseState = GL_InitialiseBufferState;
		api->Size = GL_BufferSize;
		api->UnBind = GL_UnBindBuffer;
		api->Update = GL_UpdateBuffer;
		api->UpdateSection = GL_UpdateBufferSection;

		api->Shutdown = GL_BufferShutdown;
		api->SetElementArray = GL_SetElementArrayBuffer;
		api->IsValid = GL_BufferValid;
#ifdef WITH_OPENGL_TRACE
		api->PrintState = GL_PrintBufferState;
#endif
		api->supported = true;

		Con_Printf("Triple-buffering of GL buffers: %s\n", tripleBuffer_supported ? "enabled" : "disabled");
	}
}
