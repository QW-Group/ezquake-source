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

#include <vulkan/vulkan.h>
#include "quakedef.h"

#include "vk_local.h"
#include "r_local.h"
#include "r_buffers.h"

typedef struct vk_buffer_s {
	VkBuffer handle;
} vk_buffer_t;

static vk_buffer_t bufferData[64];

static VkBufferUsageFlags VK_BufferMemoryStyle(bufferusage_t usage)
{
	switch (usage) {
		case bufferusage_once_per_frame:
			// filled & used once per frame
			break;
		case bufferusage_reuse_per_frame:
			// filled & used many times per frame
			break;
		case bufferusage_reuse_many_frames:
			// filled once, expect to use many times over subsequent frames
			break;
		case bufferusage_constant_data:
			// filled once, never updated again
			break;
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

static buffer_ref VK_BufferCreate(buffertype_t type, const char* name, int size, void* data, bufferusage_t usage)
{
	VkBuffer buffer = { 0 };
	VkBufferCreateInfo bufferInfo = { 0 };
	VkMemoryRequirements memRequirements = { 0 };

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BufferUsageForType(type);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // FIXME: compute queue?
	if (vkCreateBuffer(vk_options.logicalDevice, &bufferInfo, NULL, &buffer) != VK_SUCCESS) {
		return null_buffer_reference;
	}

	vkGetBufferMemoryRequirements(vk_options.logicalDevice, buffer, &memRequirements);

	// TODO
	//memRequirements.
	return null_buffer_reference;
}

static void VK_BufferEnsureSize(buffer_ref buffer, int size)
{
}

static void VK_BufferInitialiseState(void)
{
}

static size_t VK_BufferSize(buffer_ref vbo)
{
	return 0;
}

static uintptr_t VK_BufferOffset(buffer_ref ref)
{
	return 0;
}

static void VK_BufferBind(buffer_ref ref)
{
}

static void VK_BufferBindBase(buffer_ref ref, unsigned int index)
{
}

static void VK_BufferBindRange(buffer_ref ref, unsigned int index, ptrdiff_t offset, int size)
{
}

static void VK_BufferUnBind(buffertype_t type)
{
}

static void VK_BufferUpdate(buffer_ref vbo, int size, void* data)
{
}

static void VK_BufferUpdateSection(buffer_ref vbo, ptrdiff_t offset, int size, const void* data)
{
}

static buffer_ref VK_BufferResize(buffer_ref vbo, int size, void* data)
{
	return null_buffer_reference;
}

static qbool VK_BufferIsValid(buffer_ref ref)
{
	return false;
}

static void VK_BufferSetElementArray(buffer_ref ref)
{
	return;
}

static void VK_BufferShutdown(void)
{
	return;
}

#ifdef WITH_OPENGL_TRACE
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

#ifdef WITH_OPENGL_TRACE
	api->PrintState = VK_PrintBufferState;
#endif
}
