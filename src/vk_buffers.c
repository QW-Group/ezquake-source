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

// vk_buffers.c
// - Vulkan buffer handling

#ifdef RENDERER_OPTION_VULKAN

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include "vk_local.h"
#include "r_local.h"
#include "r_buffers.h"

typedef struct vk_buffer_s {
	VkBuffer handle;
	VkDeviceMemory memory;
	buffertype_t type;
	bufferusage_t usage;
	size_t size;
} vk_buffer_t;

static vk_buffer_t bufferData[r_buffer_count];

static void VK_BufferResize(r_buffer_id id, int size, void* data);
static void VK_BufferUpdateSection(r_buffer_id id, ptrdiff_t offset, int size, const void* data);

static VkMemoryPropertyFlags VK_BufferMemoryStyle(bufferusage_t usage)
{
	switch (usage) {
		case bufferusage_once_per_frame:
			// filled & used once per frame
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case bufferusage_reuse_per_frame:
			// filled & used many times per frame
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case bufferusage_reuse_many_frames:
			// filled once, expect to use many times over subsequent frames
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case bufferusage_constant_data:
			// filled once, never updated again
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		default:
			assert(false);
			return 0;
	}
}

static VkBufferUsageFlags VK_BufferUsageForType(buffertype_t type)
{
	switch (type) {
		case buffertype_vertex:
			return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		case buffertype_index:
			return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		case buffertype_indirect:
			return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		case buffertype_storage:
			return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		case buffertype_uniform:
			return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}

	assert(false);
	Sys_Error("Invalid buffertype passed to VK_BufferUsage(%d)", type);
	return 0;
}

static void VK_BufferStartFrame(void)
{
}

static void VK_BufferEndFrame(void)
{
}

static qbool VK_BufferReady(void)
{
	return true;
}

static qbool VK_BufferCreate(r_buffer_id id, buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	vk_buffer_t* slot;
	VkBufferUsageFlags bufferUsage;
	VkMemoryPropertyFlags memoryStyle;
	void* mapped;

	assert(id > r_buffer_none && id < r_buffer_count);
	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return false;
	}

	slot = &bufferData[id];
	if (slot->handle != VK_NULL_HANDLE) {
		vkDestroyBuffer(vk_options.logicalDevice, slot->handle, NULL);
	}
	if (slot->memory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_options.logicalDevice, slot->memory, NULL);
	}
	memset(slot, 0, sizeof(*slot));

	bufferUsage = VK_BufferUsageForType(type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	memoryStyle = VK_BufferMemoryStyle(usage);
	(void)name;
	if (!VK_CreateBufferResource(size, bufferUsage, memoryStyle, &slot->handle, &slot->memory)) {
		return false;
	}

	slot->type = type;
	slot->usage = usage;
	slot->size = size;

	if (data && vkMapMemory(vk_options.logicalDevice, slot->memory, 0, size, 0, &mapped) == VK_SUCCESS) {
		memcpy(mapped, data, size);
		vkUnmapMemory(vk_options.logicalDevice, slot->memory);
	}

	return true;
}

static void VK_BufferEnsureSize(r_buffer_id id, int size)
{
	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return;
	}
	if (bufferData[id].handle == VK_NULL_HANDLE || bufferData[id].size < (size_t)size) {
		VK_BufferResize(id, size, NULL);
	}
}

static void VK_BufferInitialiseState(void)
{
}

static size_t VK_BufferSize(r_buffer_id id)
{
	return (id > r_buffer_none && id < r_buffer_count) ? bufferData[id].size : 0;
}

static uintptr_t VK_BufferOffset(r_buffer_id id)
{
	return 0;
}

static void VK_BufferBind(r_buffer_id id)
{
}

static void VK_BufferBindBase(r_buffer_id id, unsigned int index)
{
}

static void VK_BufferBindRange(r_buffer_id id, unsigned int index, ptrdiff_t offset, int size)
{
}

static void VK_BufferUnBind(buffertype_t type)
{
}

static void VK_BufferUpdate(r_buffer_id id, int size, void* data)
{
	VK_BufferUpdateSection(id, 0, size, data);
}

static void VK_BufferUpdateSection(r_buffer_id id, ptrdiff_t offset, int size, const void* data)
{
	vk_buffer_t* slot;
	void* mapped;

	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0 || !data) {
		return;
	}

	slot = &bufferData[id];
	if (slot->handle == VK_NULL_HANDLE || offset < 0 || (size_t)(offset + size) > slot->size) {
		VK_BufferEnsureSize(id, offset + size);
		slot = &bufferData[id];
	}
	if (slot->handle == VK_NULL_HANDLE) {
		return;
	}

	if (vkMapMemory(vk_options.logicalDevice, slot->memory, offset, size, 0, &mapped) == VK_SUCCESS) {
		memcpy(mapped, data, size);
		vkUnmapMemory(vk_options.logicalDevice, slot->memory);
	}
}

static void VK_BufferResize(r_buffer_id id, int size, void* data)
{
	vk_buffer_t old;

	if (id <= r_buffer_none || id >= r_buffer_count || size <= 0) {
		return;
	}

	old = bufferData[id];
	memset(&bufferData[id], 0, sizeof(bufferData[id]));

	if (!VK_BufferCreate(id, old.type ? old.type : buffertype_vertex, NULL, size, data, old.usage ? old.usage : bufferusage_once_per_frame)) {
		bufferData[id] = old;
		return;
	}

	if (old.handle != VK_NULL_HANDLE) {
		vkDestroyBuffer(vk_options.logicalDevice, old.handle, NULL);
	}
	if (old.memory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_options.logicalDevice, old.memory, NULL);
	}
}

static qbool VK_BufferIsValid(r_buffer_id id)
{
	return (id > r_buffer_none && id < r_buffer_count && bufferData[id].handle != VK_NULL_HANDLE);
}

static void VK_BufferSetElementArray(r_buffer_id id)
{
	return;
}

static void VK_BufferShutdown(void)
{
	int i;

	for (i = 0; i < r_buffer_count; ++i) {
		if (bufferData[i].handle != VK_NULL_HANDLE) {
			vkDestroyBuffer(vk_options.logicalDevice, bufferData[i].handle, NULL);
		}
		if (bufferData[i].memory != VK_NULL_HANDLE) {
			vkFreeMemory(vk_options.logicalDevice, bufferData[i].memory, NULL);
		}
	}
	memset(bufferData, 0, sizeof(bufferData));
	return;
}

VkBuffer VK_BufferHandle(r_buffer_id id)
{
	if (id <= r_buffer_none || id >= r_buffer_count) {
		return VK_NULL_HANDLE;
	}
	return bufferData[id].handle;
}

VkDeviceSize VK_BufferDeviceOffset(r_buffer_id id)
{
	(void)id;
	return 0;
}

#ifdef WITH_RENDERING_TRACE
static void VK_PrintBufferState(FILE* output, int depth)
{
}
#endif

void VK_InitialiseBufferHandling(api_buffers_t* api)
{
	memset(api, 0, sizeof(*api));

	api->InitialiseState = VK_BufferInitialiseState;

	api->StartFrame = VK_BufferStartFrame;
	api->EndFrame = VK_BufferEndFrame;
	api->FrameReady = VK_BufferReady;

	api->Size = VK_BufferSize;
	api->Create = VK_BufferCreate;
	api->BufferOffset = VK_BufferOffset;

	api->Bind = VK_BufferBind;
	api->BindBase = VK_BufferBindBase;
	api->BindRange = VK_BufferBindRange;
	api->UnBind = VK_BufferUnBind;

	api->Update = VK_BufferUpdate;
	api->UpdateSection = VK_BufferUpdateSection;
	api->Resize = VK_BufferResize;
	api->EnsureSize = VK_BufferEnsureSize;

	api->IsValid = VK_BufferIsValid;
	api->SetElementArray = VK_BufferSetElementArray;
	api->Shutdown = VK_BufferShutdown;

#ifdef WITH_RENDERING_TRACE
	api->PrintState = VK_PrintBufferState;
#endif
}

#endif // #ifdef RENDERER_OPTION_VULKAN
